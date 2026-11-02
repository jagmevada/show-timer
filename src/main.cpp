// remote_v2 firmware (new board, 2026-11). Pin map: FIRMWARE_NOTES.md §1. Panel protocol: protocol.h.
// UI copied from the old remote (jj-timer-remote / timer_remote_code):
//   OK = HOLD (running) / SET_RESUME with the shown time (paused)
//   OK + RIGHT held = RESTART from the saved time;  OK + RIGHT + LEFT held = save the shown time (EEPROM cmd)
//   paused: LEFT/RIGHT pick a digit, UP/DOWN change it
// The panel never reports its time, so the remote MIRRORS it: same countdown, HOLD, and "next show" at 00:00
// (the panel then waits for RESTART only). The mirror keeps running through sleep AND off:
//   PWR short or 30 s idle = sleep: screen off, MCU power-down, count runs on the RTC PIT, LED blinks every 1 s;
//     any button wakes and shows the panel's current time.
//   PWR hold 1 s = off (no blink, only a PWR hold wakes it); power on shows the panel's current time.
//   Each shows a 1 s "SLEEP" / "POWER OFF" notice first; asleep the LED blinks every 1 s, off it stays dark.
// LED: on for 1 s at power-on, otherwise off while the screen is on (saves power).
//   5 h asleep = off.
// Only a battery / J6 power loss loses the mirror. The panel can't report its time (it only echoes the command
// byte), so then: "NOT SYNCED" -> OK sends HOLD (panel freezes on its own time) -> copy that time into the
// remote -> OK sends SET_RESUME with it: the panel carries on from exactly where it was. Or OK + RIGHT = RESTART.
// HC-12 powered only while sending.
// Debug: J5 (T R G) 115200.
// #define FIRSTTIME  // uncomment for the FIRST flash after fitting a new HC-12: it sets the module to the panel's
//                    // channel / power / baud at power-up (needs the battery). Then comment it out and reflash,
//                    // like the old remote. Without it the module is assumed to be set up already.
#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <avr/sleep.h>
#include "bigdigits.h"
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
const uint8_t BEAT_TICKS = 4;                   // sleep heartbeat: 4 x 0.25 s PIT ticks = 20 ms LED blink every 1 s (idle wait: ~45 uA avg)
const float VBAT_K = 1.33f;                     // (330k+1M)/1M. ponytail: trim against a multimeter

DisplaySSD1306_128x64_I2C display(-1);
ConfigurableDownCounter counter;
bool paused, ended, notSynced, screenOn;
bool hcReady;  // HC-12 set to the panel's channel / power / baud (done at power-up with a battery, else on first send)
// ended = panel reached 00:00 and shows "next show" until RESTART
uint8_t cursor;
uint32_t tick, lastPress;
char msg[22];
uint32_t ledOn, oledUpAt;  // ledOn: millis() of the power-on LED pulse (0 = none); oledUpAt: OLED power-up  // millis() of the power-on LED pulse, 0 = none
volatile uint8_t pitTicks;
volatile bool btnWake;

const char *const CMD_NAME[] = {"RESTART", "RESUME", "HOLD", "SET", "STATUS", "NEWDATA", "SAVE", "-"};

uint8_t readButtons() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < 6; i++)
    if (!digitalRead(BTN[i])) m |= 1 << i;
  return m;
}

// Saved start time (OK + LEFT + RIGHT). Flashing erases EEPROM to 0xFF, so blank = 20:00, the panel's default too.
void savedTime(uint8_t &m, uint8_t &s) {
  m = EEPROM.read(EEPROMADDR), s = EEPROM.read(EEPROMADDR + 1);
  if (m > 99 || s > 59) m = 20, s = 0;
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
  uint8_t m = counter.GetMinutes(), sec = counter.GetSeconds(), dig[] = {m / 10, m % 10, sec / 10, sec % 10};
  static const uint8_t X[] = {3, 31, 69, 97};  // digit columns; the narrow colon sits at 59
  for (uint8_t i = 0; i < 4; i++) {            // rows 16-63: the blue part of the 2-colour OLED
    display.setBackground(paused && cursor == i ? 0xFF : 0);  // the digit being edited is drawn inverted
    display.drawBitmap1(X[i], 16, BIG_W, BIG_H, BIG_DIGITS[dig[i]]);
  }
  display.setBackground(0);
  display.drawBitmap1(59, 16, COLON_W, BIG_H, BIG_COLON);
  display.setFixedFont(ssd1306xled_font6x8);
  snprintf(s, sizeof s, "%-21s", msg);
  display.printFixed(0, 8, s, STYLE_NORMAL);  // rows 0-15 are yellow: the two status lines
  uint16_t mv = vbatMv();
  // 2xAAA alkaline is close to linear between 2.0 V (empty) and 3.0 V (full); fresh cells read up to 3.2 V.
  int pct = constrain(((int)mv - 2000) / 10, 0, 100);
  uint8_t em, es;
  savedTime(em, es);
  snprintf(s, sizeof s, "%u.%02uV %3d%% E%02u:%02u A%u ", mv / 1000, mv % 1000 / 10, pct, em, es, MYADDRESS);
  display.printFixed(0, 0, s, STYLE_NORMAL);
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
  oledUpAt = millis();
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
  while (screenOn && millis() - oledUpAt < 100) {}  // never stack the HC-12 inrush on the OLED's (boost mode on 2xAAA)
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

// One AT command; logs the reply on J5. Returns true if the reply contains "OK".
bool hcCmd(const char *cmd, uint16_t waitMs = 200) {
  while (HC.available()) HC.read();
  HC.print(cmd);
  char r[64];
  uint8_t n = 0;
  for (uint32_t t = millis(); millis() - t < waitMs;)
    if (HC.available() && n < sizeof r - 1) r[n++] = HC.read();
  r[n] = 0;
  DBG.print(cmd);
  DBG.print(F(" -> "));
  DBG.println(r);
  return strstr(r, "OK") != nullptr;
}

// Puts the HC-12 on the panel's settings. Same sequence as the old remote's FIRSTTIME build, which is what
// makes brand-new modules work:
//   cold power-up with SET low (AT mode at 9600 whatever is stored) -> AT, channel, power, baud, AT+RX (logged)
//   -> SET high WHILE POWERED for 600 ms: the module leaves AT mode and applies the new settings (cutting power
//      while still in AT mode left new modules on their factory 9600 / CH001)
//   -> SET low again = AT mode at the CURRENT baud: an "OK" at 2400 proves the baud really changed.
// Runs at every power-up with a battery (or on the first send).
bool hcConfig() {
  char ch[8];
  snprintf(ch, sizeof ch, "AT+C%03u", RF_CH);
  const char *const AT[] = {"AT", ch, "AT+P8", "AT+B2400"};  // baud last
  rfOff();
  delay(1000);  // cold start, like the old FIRSTTIME (it waited 2 s)
  rfOn(true);
  bool ok = true;
  for (const char *c : AT) ok &= hcCmd(c);
  hcCmd("AT+RX", 400);  // all parameters, for the J5 log
  hcCmd("AT+V", 300);   // firmware version, for the J5 log
  digitalWrite(HC_SET, HIGH);  // leave AT mode while powered: settings take effect
  delay(600);
  HC.end();
  HC.begin(2400);
  digitalWrite(HC_SET, LOW);  // AT mode again, now at the stored baud
  delay(100);
  bool at2400 = hcCmd("AT");
  DBG.println(at2400 ? F("HC-12 OK at 2400") : F("HC-12 NOT at 2400"));
  digitalWrite(HC_SET, HIGH);
  delay(200);
  rfOff();
  return ok && at2400;
}

bool send(uint8_t cmd) {
  if (!hcReady && !(hcReady = hcConfig())) {  // e.g. batteries added after a power-up on the programmer
    setMsg("HC-12 no reply");
    return false;
  }
  setMsg(CMD_NAME[cmd], " sending...");
  rfOn(false);
  bool ack = false;
  for (uint8_t i = 0; i < TRIES && !ack; i++) {
    uint8_t f[3], n = frame(f, cmd, counter.GetMinutes(), counter.GetSeconds());  // fresh time each try
    HC.write(f, n);                                                                // one burst = one RF packet
    for (uint32_t t = millis(); millis() - t < ACK_MS && !ack;)
      if (HC.available()) {
        uint8_t b = HC.read();
        DBG.print(F("rx 0x"));  // debug: anything heard during the ACK wait (expect the command byte back)
        DBG.println(b, HEX);
        if (b == f[0]) ack = true;
      }
  }
  rfOff();
  setMsg(CMD_NAME[cmd], ack ? ": OK" : ": NO ACK");
  return ack;
}

void loadEeprom() {
  uint8_t m, s;
  savedTime(m, s);
  counter = ConfigurableDownCounter(m, s);
}

bool running() { return !paused && !ended; }

void countDown() {  // one panel second; at 00:00 the panel sits on "next show" until RESTART
  counter.CountDownByOneSecond();
  if (counter.GetMinutes() == 0 && counter.GetSeconds() == 0 && !ended) {
    ended = true;
    snprintf(msg, sizeof msg, "NEXT SHOW: OK+RIGHT");
  }
}

void okPressed(uint8_t held) {
  uint8_t cmd;
  if ((held & bit(RIGHT)) && (held & bit(LEFT))) {
    EEPROM.update(EEPROMADDR, counter.GetMinutes());
    EEPROM.update(EEPROMADDR + 1, counter.GetSeconds());
    cmd = CMD_EEPROM;
  } else if (held & bit(RIGHT)) {
    cmd = CMD_RESTART;
  } else {
    cmd = paused && !notSynced ? CMD_SET_RESUME : CMD_HOLD;  // not synced: freeze the panel first
  }
  if (send(cmd)) {  // like the old remote: local state follows the panel's ACK
    if (cmd == CMD_RESTART) ended = false, loadEeprom();  // only RESTART takes the panel out of "next show"
    if (ended) snprintf(msg, sizeof msg, "NEXT SHOW: OK+RIGHT");  // panel ACKs but stays there
    if (notSynced && cmd == CMD_HOLD) snprintf(msg, sizeof msg, "COPY PANEL TIME + OK");
    notSynced = false;
    paused = cmd == CMD_HOLD;
    tick = millis() - SEC_MS;  // the panel ACKs at the end of its 1 s loop and counts down right after
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

void btnIsr() { btnWake = true; }

ISR(RTC_PIT_vect) {
  RTC.PITINTFLAGS = RTC_PI_bm;
  pitTicks++;
}

// The 32 kHz ULP oscillator keeps time in sleep but is only ~3 % accurate, so measure it against the
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

void banner(const char *title, const char *hint) {  // 1 s notice before the screen goes dark
  if (!screenOn) return;
  display.clear();
  display.setFixedFont(ssd1306xled_font8x16);
  display.printFixed((128 - 8 * strlen(title)) / 2, 0, title, STYLE_NORMAL);  // yellow band
  display.setFixedFont(ssd1306xled_font6x8);
  display.printFixed(7, 32, "timer keeps running", STYLE_NORMAL);
  display.printFixed((128 - 6 * strlen(hint)) / 2, 48, hint, STYLE_NORMAL);
  delay(1000);
}

void ledPulse() {  // power-on indicator: 1 s, then loop() turns it off
  digitalWrite(LED, HIGH);
  ledOn = millis() | 1;
}

void sleepCountdown(bool off) {
  banner(off ? "POWER OFF" : "SLEEP", off ? "hold PWR to turn on" : "any key wakes");
  oledOff();
  digitalWrite(LED, LOW);
  ledOn = 0;
  DBG.println(off ? F("off") : F("sleep"));
  DBG.flush();
  uint32_t periodUs = pitPeriodUs(), acc = (millis() - tick) * 1000UL, ticks = 0;
  for (uint8_t p : BTN) attachInterrupt(p, btnIsr, CHANGE);  // CHANGE: every pin (not only PA6/PC2) wakes from power-down
  pitTicks = 0;
  PORTA.PIN2CTRL = PORT_PULLUPEN_bm;  // DBG RX floats with J5 unplugged
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  while (RTC.PITSTATUS) {}
  RTC.PITINTCTRL = RTC_PI_bm;
  RTC.PITCTRLA = RTC_PERIOD_CYC8192_gc | RTC_PITEN_bm;  // wake every ~0.25 s
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  auto drain = [&] {  // count the PIT ticks that came in
    while (pitTicks) {
      cli();
      pitTicks--;
      sei();
      if (running()) {
        acc += periodUs;
        while (acc >= SEC_MS * 1000UL) {
          acc -= SEC_MS * 1000UL;
          countDown();
        }
      }
      if (!off && ++ticks % BEAT_TICKS == 0) {
        digitalWrite(LED, HIGH);
        set_sleep_mode(SLEEP_MODE_IDLE);  // CPU sleeps; the millis timer (TCB1) wakes it every 1 ms
        for (uint32_t t = millis(); millis() - t < 20;) sleep_mode();
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        digitalWrite(LED, LOW);
      }
      if (ticks >= AUTO_OFF_MS / 250) off = true;
    }
  };
  for (;;) {
    btnWake = false;
    sleep_mode();
    drain();
    if (!btnWake) continue;
    if (!off) break;  // asleep: any button wakes
    uint32_t t = millis();  // off: only a PWR hold turns it on (PIT ticks queue meanwhile)
    while (!digitalRead(BTN[PWR]) && millis() - t < HOLD_MS) {}
    if (millis() - t >= HOLD_MS) break;
  }
  drain();
  while (RTC.PITSTATUS) {}
  RTC.PITCTRLA = 0;
  for (uint8_t p : BTN) detachInterrupt(p);
  PORTA.PIN2CTRL = 0;
  ADC0.CTRLA |= ADC_ENABLE_bm;
  tick = millis() - acc / 1000;  // carry the part-second into the awake count
  if (off) ledPulse();  // power-on; a wake from sleep keeps the LED off
  DBG.print(F("wake, buttons 0x"));
  DBG.println(readButtons(), HEX);
  delay(100);  // MCU wakes first, OLED inrush 100 ms later: two smaller load steps (2xAAA = boost mode, slower)
  oledOn();  // shows the panel's current time
  DBG.println(F("oled on"));
  for (uint32_t t = millis(); readButtons() && millis() - t < 3000;) {}  // the wake press only wakes; a stuck pin can't hang it
  if (readButtons()) DBG.print(F("STUCK button 0x")), DBG.println(readButtons(), HEX);
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
  DBG.println(F("HELLO"));  // power-on marker on J5
  DBG.print(F("remote_v2 addr "));
  DBG.print(MYADDRESS);
  DBG.print(F(", reset flags 0x"));
  DBG.println(GPIOR0, HEX);  // megaTinyCore saves RSTFR here and clears it: 1 power-on, 2 brown-out, 0x10 software (a crash), 0x20 UPDI
#ifdef FIRSTTIME
  hcReady = hcConfig();  // works on the programmer's USB power too
#else
  hcReady = true;  // set up by a FIRSTTIME flash
#endif
  loadEeprom();  // J6 just closed: the panel's state is unknown
  paused = notSynced = true;
  tick = lastPress = millis();
  ledPulse();
  snprintf(msg, sizeof msg, !hcReady ? "HC-12 no reply" : "NOT SYNCED: press OK");
  oledOn();
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
    sleepCountdown(true);
    last = readButtons();
    return;
  }
  if ((up & bit(PWR)) && !pwrLong) {  // short PWR: sleep now
    sleepCountdown(false);
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

  if (!running()) tick = millis();
  while (millis() - tick >= SEC_MS) {  // while: catches up after a blocking send
    tick += SEC_MS;
    countDown();
    draw();
  }
  if (ledOn && millis() - ledOn >= 1000) digitalWrite(LED, LOW), ledOn = 0;
  static uint32_t alive;  // J5 heartbeat while awake: checks the debug link without a power cycle
  if (millis() - alive >= 1000) {
    alive = millis();
    char a[24];
    snprintf(a, sizeof a, "alive %02u:%02u %s", counter.GetMinutes(), counter.GetSeconds(),
             ended ? "end" : paused ? "hold" : "run");
    DBG.println(a);
  }
  if (millis() - lastPress > SCREEN_MS) sleepCountdown(false);

  set_sleep_mode(SLEEP_MODE_IDLE);  // the millis tick wakes it every ms
  sleep_mode();
}
