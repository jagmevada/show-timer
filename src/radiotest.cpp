// Radio link test: flash the SAME code on two remote_v2 boards (pio run -e radiotest -t upload).
// Each board sends "T<id> <count>" every ~1 s and shows what it hears, so TX and RX of each board are seen
// separately, without the panel. id = chip serial number % 100.
//   OLED: own id + TX count (yellow), RX lines, last line heard, garbage bytes (blue).
//   J5 (115200): HC-12 setup replies, every byte received (hex), every line sent.
// Channel = the panel's (10 + address). Only 'T', digits, space and '\n' are sent: all fail the panel's parity or
// address check (address 2), so the panel ignores them.
#include <Arduino.h>
#include "lcdgfx.h"
#include "protocol.h"

#define LED     PIN_PA3
#define RF_EN   PIN_PB5  // LOW = HC-12 powered
#define OLED_EN PIN_PB4  // LOW = OLED powered
#define HC_SET  PIN_PC1  // LOW = AT mode
#define DBG     Serial1
#define HC      Serial

DisplaySSD1306_128x64_I2C display(-1);
uint8_t id, ln;
uint16_t txn, rxn, junk;
char line[17], last[17] = "-";

void at(const char *c, uint16_t waitMs = 200) {
  while (HC.available()) HC.read();
  HC.print(c);
  delay(waitMs);
  DBG.print(c);
  DBG.print(F(" -> "));
  while (HC.available()) DBG.write(HC.read());
  DBG.println();
}

void draw() {
  char s[22];
  display.setFixedFont(ssd1306xled_font6x8);
  snprintf(s, sizeof s, "ID %02u  TX %-6u", id, txn);
  display.printFixed(0, 0, s, STYLE_NORMAL);
  snprintf(s, sizeof s, "RX lines %-6u", rxn);
  display.printFixed(0, 8, s, STYLE_NORMAL);
  display.setFixedFont(ssd1306xled_font8x16);
  display.printFixed(0, 16, "heard:          ", STYLE_NORMAL);
  snprintf(s, sizeof s, "%-16s", last);
  display.printFixed(0, 32, s, STYLE_NORMAL);
  display.setFixedFont(ssd1306xled_font6x8);
  snprintf(s, sizeof s, "garbage bytes %-5u", junk);
  display.printFixed(0, 56, s, STYLE_NORMAL);
}

void setup() {
  digitalWrite(RF_EN, HIGH);
  digitalWrite(OLED_EN, LOW);
  pinMode(RF_EN, OUTPUT);
  pinMode(OLED_EN, OUTPUT);
  pinMode(HC_SET, OUTPUT);
  pinMode(LED, OUTPUT);
  DBG.begin(115200);
  id = SIGROW.SERNUM9 % 100;
  DBG.print(F("\r\nRADIOTEST id "));
  DBG.println(id);

  // HC-12 setup, old FIRSTTIME style: cold start in AT mode, settings, leave AT mode while powered
  digitalWrite(HC_SET, LOW);
  delay(1000);
  HC.begin(9600);
  digitalWrite(RF_EN, LOW);
  delay(300);
  char ch[8];
  snprintf(ch, sizeof ch, "AT+C%03u", RF_CH);
  at("AT");
  at(ch);
  at("AT+P8");
  at("AT+B2400");
  at("AT+RX", 400);
  digitalWrite(HC_SET, HIGH);
  delay(600);
  HC.end();
  HC.begin(2400);
  while (HC.available()) HC.read();

  delay(100);  // OLED power-up
  display.begin();
  display.clear();
  draw();
}

void loop() {
  while (HC.available()) {
    uint8_t b = HC.read();
    DBG.print(F("rx 0x"));
    DBG.println(b, HEX);
    if (b == '\n') {
      line[ln] = 0;
      memcpy(last, line, sizeof last);
      ln = 0;
      rxn++;
      digitalWrite(LED, HIGH);  // LED flash = a full line heard
      draw();
      digitalWrite(LED, LOW);
    } else if ((b == 'T' || b == ' ' || (b >= '0' && b <= '9')) && ln < sizeof line - 1) {
      line[ln++] = b;
    } else {
      junk++;
    }
  }
  static uint32_t t;
  if (millis() - t >= 1000u + id * 7u) {  // ids differ, so the two boards drift apart and rarely collide
    t = millis();
    char m[16];
    snprintf(m, sizeof m, "T%02u %u\n", id, ++txn);
    HC.print(m);
    DBG.print(F("tx "));
    DBG.print(m);
    draw();
  }
}
