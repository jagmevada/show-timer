// Panel simulator: a second remote_v2 board acting as the show-timer panel, to test the remote without it.
//   pio run -e panel_sim -t upload --upload-port COM3
// Receive path copied from the deployed panel (jj-timer-remote, branch timer-boilerplate: timer/src/main.cpp
// TCB0 ISR + loop, functions.hpp recieveprocess / eepromread), so the remote sees the same decode, the same ACK
// echo and the same 0-1 s ACK delay. Panel quirks kept on purpose:
//   - shows its address (00:01) until the first command; the first pass after that loads the EEPROM time
//   - at 00:00 it shows "next show" and only RESTART gets it out (it still ACKs everything)
// Debug log on J5 115200, one line per received frame.
#include <Arduino.h>
#include <EEPROM.h>
#include "lcdgfx.h"
#include "protocol.h"

#define LED     PIN_PA3
#define RF_EN   PIN_PB5  // LOW = HC-12 powered
#define OLED_EN PIN_PB4  // LOW = OLED powered
#define HC_SET  PIN_PC1  // LOW = AT mode
#define DBG     Serial1
#define HC      Serial

const uint8_t EEPROMADDR = 8;  // panel layout: total seconds, uint16 little-endian

typedef struct {  // the panel's own decoder (not protocol.h), so this is an independent check
  unsigned int parity : 1;
  unsigned int address : 4;
  unsigned int command : 3;
} rxbyte;
typedef union {
  uint8_t byte;
  rxbyte fields;
} rxdata;

DisplaySSD1306_128x64_I2C display(-1);
volatile uint8_t rcmd, rmsb, rlsb;
volatile bool processnewdata;
int x = MYADDRESS;
bool connected, statestop, staterestart = true, ended;
uint16_t frames;
uint8_t raw[3];
char line[22] = "waiting for remote";

const char *const NAME[] = {"RESTART", "RESUME", "HOLD", "SET", "STATUS", "NEWDATA", "EEPROM", "EEPROM1"};

int t0() {  // panel eepromread(): 1200 if out of range
  uint16_t t;
  EEPROM.get(EEPROMADDR, t);
  return (t <= 2 || t >= 6000) ? 1200 : t;
}

ISR(TCB0_INT_vect) {  // 10 ms, as the panel: the payload is read one tick after the command byte
  static bool newdata_available;
  if (newdata_available) {
    newdata_available = false;
    if (HC.available() > 1) {
      rmsb = HC.read();
      rlsb = HC.read();
    }
    processnewdata = true;
  }
  if (HC.available() > 0 && !newdata_available) {
    newdata_available = true;
    rcmd = HC.read();
  }
  TCB0.INTFLAGS = TCB_CAPT_bm;
}

void recieveprocess(uint8_t b, uint8_t msb, uint8_t lsb) {
  rxdata d;
  d.byte = b;
  bool payOk = parity8(msb) == 1 && parity8(lsb) == 1, ack = false;
  int t = (msb & 0x7f) * 60 + (lsb & 0x7f);
  frames++;
  if (parity8(b) != 0) {
    snprintf(line, sizeof line, "PARITY FAIL");
  } else if (d.fields.address != MYADDRESS) {
    snprintf(line, sizeof line, "WRONG ADDR %u", d.fields.address);
  } else {
    uint8_t c = d.fields.command;
    switch (c) {
      case CMD_RESUME: statestop = false, ack = true; break;
      case CMD_HOLD: statestop = true, ack = true; break;
      case CMD_SET_RESUME:
        if (payOk) x = t, statestop = false, ack = true;
        break;
      case CMD_EEPROM:
        if (payOk) EEPROM.put(EEPROMADDR, (uint16_t)t), x = t, statestop = false, ack = true;
        break;
      case CMD_RESTART: staterestart = true, x = t0(), statestop = false, ack = true; break;
    }
    if (ack) HC.write(b);  // the ACK is the command byte echoed
    if (c == CMD_SET_RESUME || c == CMD_EEPROM)
      snprintf(line, sizeof line, "%s %02d:%02d %s", NAME[c], t / 60, t % 60, ack ? "ACK" : "BAD PAY");
    else
      snprintf(line, sizeof line, "%s %s", NAME[c], ack ? "ACK" : "(no ack)");
    connected = true;
  }
  raw[0] = b, raw[1] = msb, raw[2] = lsb;
  DBG.print(F("rx "));
  for (uint8_t v : raw) DBG.print(v, HEX), DBG.print(' ');
  DBG.println(line);
}

void draw() {
  char s[24];
  snprintf(s, sizeof s, "%02d:%02d", x / 60, x % 60);
  display.setFixedFont(comic_sans_font24x32_123);
  display.printFixed(4, 0, s, STYLE_NORMAL);
  display.setFixedFont(ssd1306xled_font8x16);
  snprintf(s, sizeof s, "%-10s#%-5u", !connected ? "NO LINK" : ended ? "NEXT SHOW" : statestop ? "HOLD" : "RUN",
           frames);
  display.printFixed(0, 32, s, STYLE_NORMAL);
  display.setFixedFont(ssd1306xled_font6x8);
  snprintf(s, sizeof s, "%-21s", line);
  display.printFixed(0, 48, s, STYLE_NORMAL);
  int e = t0();
  snprintf(s, sizeof s, "rx %02X %02X %02X  E%02d:%02d", raw[0], raw[1], raw[2], e / 60, e % 60);
  display.printFixed(0, 56, s, STYLE_NORMAL);
}

void setup() {
  digitalWrite(RF_EN, HIGH);
  digitalWrite(OLED_EN, LOW);
  pinMode(RF_EN, OUTPUT);
  pinMode(OLED_EN, OUTPUT);
  pinMode(HC_SET, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  DBG.begin(115200);
  DBG.println(F("\r\npanel_sim addr 1"));

  // SET low at power-up = AT mode at 9600: put the module on the panel settings (CH011 / P8 / 2400)
  digitalWrite(HC_SET, LOW);
  delay(200);  // module fully off
  HC.begin(9600);
  digitalWrite(RF_EN, LOW);
  delay(300);
  char ch[8];
  snprintf(ch, sizeof ch, "AT+C%03u", RF_CH);
  const char *const AT[] = {ch, "AT+P8", "AT+B2400"};  // baud last
  bool ok = true;
  for (const char *c : AT) {
    HC.print(c);
    char r[24];
    uint8_t n = 0;
    for (uint32_t t = millis(); millis() - t < 200;)
      if (HC.available() && n < sizeof r - 1) r[n++] = HC.read();
    r[n] = 0;
    DBG.print(c), DBG.print(F(" -> ")), DBG.println(r);
    ok &= strstr(r, "OK") != nullptr;
  }
  digitalWrite(HC_SET, HIGH);  // leaving AT mode applies the new settings
  delay(100);
  HC.begin(2400);
  while (HC.available()) HC.read();
  if (!ok) snprintf(line, sizeof line, "HC-12 no reply");

  delay(100);  // OLED power-up
  display.begin();
  display.clear();
  draw();

  TCB0.CCMP = F_CPU / 2 / 100 - 1;  // 10 ms
  TCB0.CTRLA = TCB_CLKSEL_DIV2_gc | TCB_ENABLE_bm;
  TCB0.INTFLAGS = TCB_CAPT_bm;
  TCB0.INTCTRL = TCB_CAPT_bm;
}

void loop() {
  static uint32_t pass = millis();
  if (millis() - pass < 1000) return;  // one panel loop pass is ~1 s (showtime / blinkx block), so the ACK is late too
  pass += 1000;
  if (connected) {
    if (staterestart) staterestart = false, ended = false, x = t0();
    if (!statestop && !ended) {
      if (x > 0) x--;
      if (x == 0) ended = true;
    }
  }
  if (processnewdata) {
    cli();
    uint8_t b = rcmd, m = rmsb, l = rlsb;
    processnewdata = false;
    sei();
    recieveprocess(b, m, l);
  }
  digitalWrite(LED, !ended || pass / 1000 % 2);  // LED blinks at "next show"
  draw();
}
