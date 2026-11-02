// remote_v2 firmware (new board, 2026-11). Pin map: FIRMWARE_NOTES.md §1. Panel protocol: protocol.h.
// UI copied from the old remote (jj-timer-remote / timer_remote_code):
//   OK = HOLD (running) / SET_RESUME with the shown time (paused)
//   OK + RIGHT held = RESTART from the saved time;  OK + RIGHT + LEFT held = save the shown time (EEPROM cmd)
//   paused: LEFT/RIGHT pick a digit, UP/DOWN change it
// New-board power: PWR hold 1 s = on/off (next power-on starts from the saved time, 20:00 by default).
// PWR short or 30 s idle = sleep: screen + LED off, MCU power-down, countdown keeps running on the RTC PIT,
// LED blinks every 4 s; any button wakes and shows the running time. Auto-off after 5 h asleep.
// HC-12 powered only while sending.
// Debug: J5 (T R G) 115200.
#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include "counter.hpp"
#include "lcdgfx.h"
#include "protocol.h"

#define LED     PIN_PA3  // HIGH = on
#define RF_EN   PIN_PB5  // LOW = HC-12 powered
#define OLED_EN PIN_PB4  // LOW = OLED powered
#define HC_SET  PIN_PC1  // LOW = AT mode
#define HC_TX   PIN_PB2
#define VBAT    PIN_PA7  // AIN7, 330k/1M
#define DBG     Serial1  // PA1/PA2 -> J5
#define HC      Serial   // PB2/PB3 -> HC-12

enum { PWR, UP, DOWN, LEFT, RIGHT, OK };
const uint8_t BTN[] = {PIN_PA6, PIN_PA4, PIN_PA5, PIN_PC0, PIN_PC2, PIN_PC3};

const uint8_t EEPROMADDR = 8;                   // minutes, seconds: same place as the old remote
const uint16_t HC_BOOT_MS = 100;                // HC-12 power-up to ready. ponytail: raise if the first frame is lost
const uint16_t ACK_MS = 1500;                   // panel echoes from its display loop, up to ~1 s late
const uint8_t TRIES = 3;
const uint16_t HOLD_MS = 1000;                  // PWR hold for on/off
const uint32_t SCREEN_MS = 30000UL;             // sleep after 30 s without a button
const uint32_t AUTO_OFF_MS = 5UL * 3600 * 1000; // power off after 5 h asleep
const uint16_t SEC_MS = 1000;                   // one countdown second. ponytail: trim if it drifts vs a stopwatch
const uint8_t BEAT_TICKS = 16;                  // sleep heartbeat: 16 x 0.25 s PIT ticks = LED blink every 4 s
const float VBAT_K = 1.33f;                     // (330k+1M)/1M. ponytail: trim against a multimeter

DisplaySSD1306_128x64_I2C display(-1);
ConfigurableDownCounter counter;
bool paused, screenOn;
uint8_t cursor;
uint32_t tick, lastPress;
char msg[22];
volatile uint8_t pitTicks;
volatile bool btnWake;

const char *const CMD_NAME[] = {"RESTART", "RESUME", "HOLD", "SET", "STATUS", "NEWDATA", "SAVE", "-"};

uint8_t readButtons() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < 6; i++)
    if (!digitalRead(BTN[i])) m |= 1 << i;
  return m;
}

uint16_t vbatMv() {
  analogReference(INTERNAL2V5);
  analogReadResolution(12);
  analogRead(VBAT);  // discard first reading after a reference change
  uint32_t s = 0;
  for (uint8_t i = 0; i < 8; i++) s += analogRead(VBAT);
  return s / 8 * (2500.0f * VBAT_K / 4095);
}

void draw() {
  if (!screenOn) return;
  char s[24];
  snprintf(s, sizeof s, "%02u:%02u", counter.GetMinutes(), counter.GetSeconds());
  display.setFixedFont(comic_sans_font24x32_123);  // native 24x32 digits: smooth, not a scaled 6x8
  display.printFixed(4, 0, s, STYLE_NORMAL);       // 5 chars x 24 px
  display.setFixedFont(ssd1306xled_font8x16);
  display.printFixed(0, 32, "                ", STYLE_NORMAL);
  static const uint8_t X[] = {4, 28, 76, 100};  // digit columns of the time
  if (paused) display.printFixed(X[cursor] + 8, 32, "^", STYLE_NORMAL);
  display.setFixedFont(ssd1306xled_font6x8);
  snprintf(s, sizeof s, "%-21s", msg);
  display.printFixed(0, 48, s, STYLE_NORMAL);
  uint16_t mv = vbatMv();
  // 2xAAA alkaline is close to linear between 2.0 V (empty) and 3.0 V (full); fresh cells read up to 3.2 V.
  int pct = constrain(((int)mv - 2000) / 10, 0, 100);
  snprintf(s, sizeof s, "%u.%02uV %3d%% E%02u:%02u A%u ", mv / 1000, mv % 1000 / 10, pct, EEPROM.read(EEPROMADDR),
           EEPROM.read(EEPROMADDR + 1), MYADDRESS);
  display.printFixed(0, 56, s, STYLE_NORMAL);
}

void setMsg(const char *a, const char *b = "") {
  snprintf(msg, sizeof msg, "%s%s", a, b);
  DBG.println(msg);
  draw();
}

void oledOn() {
  digitalWrite(OLED_EN, LOW);
  delay(100);
  display.begin();
  display.setFixedFont(ssd1306xled_font6x8);
  display.clear();
  screenOn = true;
  draw();
}

void oledOff() {
  if (screenOn) display.end();
  screenOn = false;
  pinMode(PIN_PB1, OUTPUT);  // SDA/SCL low before cutting VOLED: no back-feed
  pinMode(PIN_PB0, OUTPUT);
  digitalWrite(PIN_PB1, LOW);
  digitalWrite(PIN_PB0, LOW);
  digitalWrite(OLED_EN, HIGH);
}

void rfOn(bool atMode) {
  PORTB.PIN3CTRL = 0;  // RX input buffer on
  digitalWrite(HC_SET, atMode ? LOW : HIGH);
  HC.begin(atMode ? 9600 : 2400);  // TX idles high before the module boots
  digitalWrite(RF_EN, LOW);
  delay(atMode ? 300 : HC_BOOT_MS);
  while (HC.available()) HC.read();
}

void rfOff() {
  HC.end();  // module inputs low before cutting its power: no back-feed
  pinMode(HC_TX, OUTPUT);
  digitalWrite(HC_TX, LOW);
  digitalWrite(HC_SET, LOW);
  digitalWrite(RF_EN, HIGH);
  PORTB.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;  // RX floats while the module is off
}

// SET held low at power-up = AT mode at 9600 whatever is stored. Puts the module on the panel's settings,
// so no FIRSTTIME build is needed. Runs once per J6 power-on.
bool hcConfig() {
  char ch[8];
  snprintf(ch, sizeof ch, "AT+C%03u", RF_CH);
  const char *const AT[] = {ch, "AT+P8", "AT+B2400"};  // baud last: it applies on exit
  bool ok = true;
  rfOn(true);
  for (const char *c : AT) {
    HC.print(c);
    char r[24];
    uint8_t n = 0;
    for (uint32_t t = millis(); millis() - t < 200;)
      if (HC.available() && n < sizeof r - 1) r[n++] = HC.read();
    r[n] = 0;
    DBG.print(c);
    DBG.print(F(" -> "));
    DBG.println(r);
    ok &= strstr(r, "OK") != nullptr;
  }
  rfOff();
  return ok;
}

bool send(uint8_t cmd) {
  setMsg(CMD_NAME[cmd], " sending...");
  rfOn(false);
  bool ack = false;
  for (uint8_t i = 0; i < TRIES && !ack; i++) {
    uint8_t f[3], n = frame(f, cmd, counter.GetMinutes(), counter.GetSeconds());  // fresh time each try
    HC.write(f, n);                                                                // one burst = one RF packet
    for (uint32_t t = millis(); millis() - t < ACK_MS && !ack;)
      if (HC.available() && HC.read() == f[0]) ack = true;
  }
  rfOff();
  setMsg(CMD_NAME[cmd], ack ? ": OK" : ": NO ACK");
  return ack;
}

void loadEeprom() {
  uint8_t m = EEPROM.read(EEPROMADDR), s = EEPROM.read(EEPROMADDR + 1);
  if (m > 99 || s > 59) m = 20, s = 0;  // blank EEPROM: 20:00, the panel's default too
  counter = ConfigurableDownCounter(m, s);
}

void okPressed(uint8_t held) {
  uint8_t cmd;
  if ((held & bit(RIGHT)) && (held & bit(LEFT))) {
    EEPROM.update(EEPROMADDR, counter.GetMinutes());
    EEPROM.update(EEPROMADDR + 1, counter.GetSeconds());
    cmd = CMD_EEPROM;
  } else if (held & bit(RIGHT)) {
    loadEeprom();
    cmd = CMD_RESTART;
  } else {
    cmd = paused ? CMD_SET_RESUME : CMD_HOLD;
  }
  if (send(cmd)) {  // like the old remote: local state follows the panel's ACK
    paused = cmd == CMD_HOLD;
    tick = millis();
    draw();
  }
}

void editDigit(bool up) {
  switch (cursor) {
    case 0: up ? counter.IncrementTensMinuteDigit() : counter.DecrementTensMinuteDigit(); break;
    case 1: up ? counter.IncrementOnesMinuteDigit() : counter.DecrementOnesMinuteDigit(); break;
    case 2: up ? counter.IncrementTensSecondDigit() : counter.DecrementTensSecondDigit(); break;
    case 3: up ? counter.IncrementOnesSecondDigit() : counter.DecrementOnesSecondDigit(); break;
  }
}

void powerOn() {
  loadEeprom();
  paused = false;
  cursor = 0;
  tick = lastPress = millis();
  digitalWrite(LED, HIGH);
  snprintf(msg, sizeof msg, "RUNNING");
  oledOn();
}

void wake() {}

void powerOff() {
  oledOff();
  rfOff();
  digitalWrite(LED, LOW);
  DBG.println(F("off"));
  DBG.flush();
  DBG.end();
  PORTA.PIN2CTRL = PORT_PULLUPEN_bm;  // DBG RX floats with J5 unplugged
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  for (;;) {  // power-down until PWR is held for HOLD_MS
    while (!digitalRead(BTN[PWR])) {}
    delay(30);
    attachInterrupt(BTN[PWR], wake, FALLING);  // PA6 is fully async: wakes from power-down
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
    detachInterrupt(BTN[PWR]);
    uint32_t t = millis();
    while (!digitalRead(BTN[PWR]) && millis() - t < HOLD_MS) {}
    if (millis() - t >= HOLD_MS) break;
  }
  PORTA.PIN2CTRL = 0;
  ADC0.CTRLA |= ADC_ENABLE_bm;
  DBG.begin(115200);
  powerOn();
}

void btnIsr() { btnWake = true; }

ISR(RTC_PIT_vect) {
  RTC.PITINTFLAGS = RTC_PI_bm;
  pitTicks++;
}

// The 32 kHz ULP oscillator keeps time in power-down but is only ~3 % accurate, so measure it against the
// main oscillator. Returns the length of one 8192-cycle PIT period in us (nominal 250000).
uint32_t pitPeriodUs() {
  RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;
  while (RTC.STATUS) {}
  RTC.CTRLA = RTC_PRESCALER_DIV1_gc | RTC_RTCEN_bm;
  while (RTC.STATUS) {}
  uint16_t c0 = RTC.CNT;
  for (uint32_t t = millis(); millis() - t < 250;) {}
  uint16_t c = RTC.CNT - c0;  // ULP cycles in 250 ms, ~8192
  return 2048000000UL / c;
}

void sleepCountdown() {
  oledOff();
  digitalWrite(LED, LOW);
  DBG.flush();
  uint32_t periodUs = pitPeriodUs(), acc = (millis() - tick) * 1000UL, ticks = 0;
  for (uint8_t p : BTN) attachInterrupt(p, btnIsr, CHANGE);  // CHANGE: every pin (not only PA6/PC2) wakes from power-down
  btnWake = false;
  pitTicks = 0;
  PORTA.PIN2CTRL = PORT_PULLUPEN_bm;  // DBG RX floats with J5 unplugged
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  while (RTC.PITSTATUS) {}
  RTC.PITINTCTRL = RTC_PI_bm;
  RTC.PITCTRLA = RTC_PERIOD_CYC8192_gc | RTC_PITEN_bm;  // wake every ~0.25 s
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  bool off = false;
  while (!btnWake && !off) {
    sleep_mode();
    while (pitTicks) {
      cli();
      pitTicks--;
      sei();
      if (!paused) {
        acc += periodUs;
        while (acc >= SEC_MS * 1000UL) {
          acc -= SEC_MS * 1000UL;
          counter.CountDownByOneSecond();
        }
      }
      if (++ticks % BEAT_TICKS == 0) {
        digitalWrite(LED, HIGH);
        _delay_ms(5);
        digitalWrite(LED, LOW);
      }
      if (ticks >= AUTO_OFF_MS / 250) off = true;
    }
  }
  while (RTC.PITSTATUS) {}
  RTC.PITCTRLA = 0;
  for (uint8_t p : BTN) detachInterrupt(p);
  PORTA.PIN2CTRL = 0;
  ADC0.CTRLA |= ADC_ENABLE_bm;
  if (off) {
    powerOff();
    return;
  }
  tick = millis() - acc / 1000;  // carry the part-second into the awake count
  digitalWrite(LED, HIGH);
  oledOn();  // shows the running time
  while (readButtons()) {}  // the wake press only wakes
  delay(30);
  lastPress = millis();
}

void setup() {
  digitalWrite(RF_EN, HIGH);
  digitalWrite(OLED_EN, HIGH);
  pinMode(RF_EN, OUTPUT);
  pinMode(OLED_EN, OUTPUT);
  pinMode(HC_SET, OUTPUT);
  pinMode(LED, OUTPUT);
  for (uint8_t p : BTN) pinMode(p, INPUT);     // 10k external pull-ups
  PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;  // analog only
  rfOff();
  oledOff();
  DBG.begin(115200);
  DBG.print(F("\r\nremote_v2 addr "));
  DBG.print(MYADDRESS);
  DBG.print(F(", reset flags 0x"));
  DBG.println(RSTCTRL.RSTFR, HEX);
  RSTCTRL.RSTFR = 0x3F;
  bool hcOk = hcConfig();
  powerOn();  // J6 just closed: start on
  if (!hcOk) setMsg("HC-12 no reply");
}

void loop() {
  static uint8_t last;
  static uint32_t pwrDown;
  static bool pwrLong;
  uint8_t now = readButtons(), down = now & ~last, up = last & ~now;
  if (now != last) {
    lastPress = millis();
    delay(25);  // debounce (hardware RC is ~1 ms)
  }
  last = now;

  if (down & bit(PWR)) pwrDown = millis(), pwrLong = false;
  if ((now & bit(PWR)) && !pwrLong && millis() - pwrDown >= HOLD_MS) {
    pwrLong = true;  // stays set until the next press, so this release is ignored
    powerOff();
    last = readButtons();
    return;
  }
  if ((up & bit(PWR)) && !pwrLong) {  // short PWR: sleep now
    sleepCountdown();
    return;
  }

  down &= ~bit(PWR);
  if (down & bit(OK)) okPressed(now);
  if (paused && down) {
    if ((down & bit(LEFT)) && cursor > 0) cursor--;
    if ((down & bit(RIGHT)) && cursor < 3) cursor++;
    if (down & bit(UP)) editDigit(true);
    if (down & bit(DOWN)) editDigit(false);
    draw();
  }

  if (paused) tick = millis();
  while (millis() - tick >= SEC_MS) {  // while: catches up after a blocking send
    tick += SEC_MS;
    counter.CountDownByOneSecond();
    draw();
  }
  if (millis() - lastPress > SCREEN_MS) sleepCountdown();

  set_sleep_mode(SLEEP_MODE_IDLE);  // the millis tick wakes it every ms
  sleep_mode();
}
