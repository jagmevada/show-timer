// HC-12 bridge: the PC drives this board's HC-12 directly through J5 (115200), for testing module settings.
//   pio run -e bridge -t upload  (remote, PC on J5)   pio run -e bridge_panel -t upload  (panel, PC on mon)
// Every byte from J5 goes to the HC-12 and every HC-12 byte goes back, unchanged. The panel protocol never sends
// 0x00, so 0x00 + one character is a bridge command:
//   p / P  HC-12 power off / on       s / S  SET low (AT mode) / high
//   1..8   HC-12 UART baud 1200 2400 4800 9600 19200 38400 57600 115200     ?  reply BRIDGE (link check)
// tools/hc12_sweep.py uses it.
#include <Arduino.h>

#ifdef PANEL             // the show-timer panel (env bridge_panel, 8 MHz build): PC on its "mon" port
#define RF_EN   PIN_PA3  // M0: LOW = HC-12 powered
#define HC_SET  PIN_PC3  // SET0
#define PC      Serial
#define HC      Serial1
#define HC_TX   PIN_PA1
#else
#define LED     PIN_PA3
#define RF_EN   PIN_PB5  // LOW = HC-12 powered
#define OLED_EN PIN_PB4  // LOW = OLED powered
#define HC_SET  PIN_PC1  // LOW = AT mode
#define PC      Serial1  // J5
#define HC      Serial
#define HC_TX   PIN_PB2
#endif

const uint32_t BAUD[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
uint32_t baud = 9600;
bool on;

void power(bool p) {  // as main.cpp rfOn/rfOff: TX held low while off, else it back-feeds the module (no cold start)
  on = p;
  HC.end();
  if (p) HC.begin(baud);  // TX idles high before the module boots
  else pinMode(HC_TX, OUTPUT), digitalWrite(HC_TX, LOW);
  digitalWrite(RF_EN, !p);
}

void setup() {
  digitalWrite(RF_EN, HIGH);
  digitalWrite(HC_SET, HIGH);
  pinMode(RF_EN, OUTPUT);
  pinMode(HC_SET, OUTPUT);
#ifdef PANEL
  digitalWrite(PIN_PC0, HIGH);  // M1: the panel's second HC-12 slot off
  pinMode(PIN_PC0, OUTPUT);
#else
  digitalWrite(OLED_EN, HIGH);  // OLED off
  pinMode(OLED_EN, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
#endif
  PC.begin(115200);
  power(false);
  PC.print(F("\r\nBRIDGE\r\n"));
}

void loop() {
  static bool esc;
  while (PC.available()) {
    uint8_t b = PC.read();
    if (!esc && b == 0) {
      esc = true;
      continue;
    }
    if (!esc) {
      HC.write(b);
      continue;
    }
    esc = false;
    if (b == 'p' || b == 'P') power(b == 'P');
    if (b == 's' || b == 'S') digitalWrite(HC_SET, b == 'S');
    if (b >= '1' && b <= '8') baud = BAUD[b - '1'], power(on);
    if (b == '?') PC.println(F("BRIDGE"));  // link check
  }
  while (HC.available()) PC.write(HC.read());
}
