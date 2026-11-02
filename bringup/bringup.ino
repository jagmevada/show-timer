// remote_v2 bring-up test (2026-10-31). Pin map: FIRMWARE_NOTES.md §1.
// megaTinyCore, ATtiny3226. Clock 10 MHz internal: VCC is 3.37 V, and 20 MHz needs 4.5 V.
// BOD disabled/disabled. Program over J4 (V G U). Debug on J5 (T R G), 115200 8N1, 3.3 V USB-UART.
// Keys on the debug terminal: v r o a p s h. Buttons print by themselves; the LED lights while one is held.
#include <Wire.h>
#include <avr/sleep.h>

#define LED     PIN_PA3   // HIGH = on
#define RF_EN   PIN_PB5   // LOW = HC-12 powered (Q2)
#define OLED_EN PIN_PB4   // LOW = OLED powered (Q3)
#define HC_SET  PIN_PC1   // LOW = AT mode
#define HC_TX   PIN_PB2
#define VBAT    PIN_PA7   // AIN7, 330k/1M divider
#define DBG     Serial1   // USART1 PA1/PA2 -> J5
#define HC      Serial    // USART0 PB2/PB3 -> HC-12
#define HC_BAUD 9600      // module's stored baud: the 'a' test prints it (old remote code set 2400)

const float VBAT_K = 1.33f;  // (330k+1M)/1M. ponytail: trim against a multimeter if the reading is off
const uint8_t BTN[] = {PIN_PA6, PIN_PA4, PIN_PA5, PIN_PC0, PIN_PC2, PIN_PC3};
const char *const BTN_NAME[] = {"PWR", "UP", "DOWN", "LEFT", "RIGHT", "OK"};
bool rfOn, oledOn;
uint8_t oledAddr;

void help() {
  DBG.println(F("v=VBAT  r=HC-12 power toggle  o=OLED toggle (scan + all pixels on)  a=HC-12 AT test  p=radio PING (after r; radio RX is printed)  s=sleep (PWR wakes)"));
}

void vbat() {
  ADC0.CTRLA |= ADC_ENABLE_bm;  // sleep() turns it off
  analogReference(INTERNAL2V5);
  analogReadResolution(12);
  analogRead(VBAT);  // throw away the first reading after a reference change
  uint32_t s = 0;
  for (uint8_t i = 0; i < 8; i++) s += analogRead(VBAT);
  DBG.print(F("VBAT raw "));
  DBG.print(s / 8);
  DBG.print(F(" = "));
  DBG.print(s / 8.0f * 2.5f / 4095 * VBAT_K, 3);
  DBG.println(F(" V"));
}

void rf(bool on) {
  if (on) {
    PORTB.PIN3CTRL = 0;  // RX input buffer back on
    pinMode(HC_SET, OUTPUT);
    digitalWrite(HC_SET, HIGH);
    HC.begin(HC_BAUD);  // TX idles high before the module boots
    digitalWrite(RF_EN, LOW);
    delay(100);
  } else {
    // Drive the module's inputs low before cutting its power, so it isn't back-fed through them.
    HC.end();
    pinMode(HC_TX, OUTPUT);
    digitalWrite(HC_TX, LOW);
    pinMode(HC_SET, OUTPUT);
    digitalWrite(HC_SET, LOW);
    digitalWrite(RF_EN, HIGH);
    PORTB.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;  // RX floats while the module is off
  }
  rfOn = on;
  DBG.println(on ? F("HC-12 ON: VRF should read 3.37 V") : F("HC-12 OFF: VRF should read 0 V"));
}

void hcSend(const char *s) {
  while (HC.available()) HC.read();
  HC.print(s);
  DBG.print(F("  > "));
  DBG.print(s);
  DBG.print(F("   < "));
  for (uint32_t t = millis(); millis() - t < 300;)
    if (HC.available()) DBG.write(HC.read());
  DBG.println();
}

void hcAT() {
  // HC-12: SET held LOW at power-up = AT mode at 9600 8N1, whatever baud is stored.
  if (rfOn) rf(false);
  delay(100);
  PORTB.PIN3CTRL = 0;
  HC.begin(9600);
  digitalWrite(RF_EN, LOW);
  delay(300);
  hcSend("AT");     // expect OK
  hcSend("AT+RX");  // expect baud, channel, FU mode, power
  rf(false);
}

void oled(bool on) {
  if (on) {
    digitalWrite(OLED_EN, LOW);
    delay(100);
    Wire.begin();
    oledAddr = 0;
    DBG.print(F("I2C scan:"));
    for (uint8_t a = 8; a < 0x78; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        DBG.print(F(" 0x"));
        DBG.print(a, HEX);
        if (a == 0x3C || a == 0x3D) oledAddr = a;
      }
    }
    DBG.println();
    if (oledAddr) {
      static const uint8_t init[] = {0x00, 0xAE, 0x8D, 0x14, 0xA5, 0xAF};  // charge pump on, all pixels on, display on
      Wire.beginTransmission(oledAddr);
      Wire.write(init, sizeof init);
      Wire.endTransmission();
      DBG.println(F("OLED found: the whole screen should be lit"));
    } else {
      DBG.println(F("OLED: no ACK at 0x3C/0x3D"));
    }
  } else if (oledOn) {
    if (oledAddr) {
      Wire.beginTransmission(oledAddr);
      Wire.write(0x00);
      Wire.write(0xAE);  // display off
      Wire.endTransmission();
    }
    Wire.end();
    pinMode(PIN_PB1, OUTPUT);  // SDA/SCL low before cutting VOLED
    pinMode(PIN_PB0, OUTPUT);
    digitalWrite(PIN_PB1, LOW);
    digitalWrite(PIN_PB0, LOW);
    digitalWrite(OLED_EN, HIGH);
    DBG.println(F("OLED OFF: VOLED should read 0 V"));
  }
  oledOn = on;
}

void wake() {}

void sleepNow() {
  oled(false);
  if (rfOn) rf(false);
  digitalWrite(LED, LOW);
  DBG.println(F("Sleeping. Unplug J5 and measure the battery current (expect ~15 uA). Press PWR to wake."));
  DBG.flush();
  DBG.end();
  PORTA.PIN2CTRL = PORT_PULLUPEN_bm;  // DBG RX floats once J5 is unplugged
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  attachInterrupt(PIN_PA6, wake, FALLING);  // PA6 is fully async: wakes from power-down
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
  detachInterrupt(PIN_PA6);
  PORTA.PIN2CTRL = 0;
  DBG.begin(115200);
  DBG.println(F("Woke on PWR"));
}

void setup() {
  digitalWrite(RF_EN, HIGH);  // both modules off; the 100k gate pull-ups already hold them off
  digitalWrite(OLED_EN, HIGH);
  pinMode(RF_EN, OUTPUT);
  pinMode(OLED_EN, OUTPUT);
  pinMode(LED, OUTPUT);
  for (uint8_t p : BTN) pinMode(p, INPUT);  // 10k external pull-ups
  PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;  // analog only
  pinMode(PIN_PB1, OUTPUT);  // I2C idle low while the OLED is off
  pinMode(PIN_PB0, OUTPUT);

  DBG.begin(115200);
  DBG.print(F("\r\nremote_v2 bring-up. Reset flags 0x"));
  DBG.print(RSTCTRL.RSTFR, HEX);  // 01 power-on, 02 brown-out, 04 ext, 08 WDT, 10 SW, 20 UPDI
  DBG.println(F(" (a new banner without a key press = unexpected reset)"));
  RSTCTRL.RSTFR = 0x3F;
  rf(false);
  for (uint8_t i = 0; i < 3; i++) {  // LED check
    digitalWrite(LED, HIGH);
    delay(150);
    digitalWrite(LED, LOW);
    delay(150);
  }
  vbat();
  help();
}

void loop() {
  static uint8_t last;
  uint8_t now = 0;
  for (uint8_t i = 0; i < 6; i++)
    if (!digitalRead(BTN[i])) now |= 1 << i;
  if (now != last) {
    for (uint8_t i = 0; i < 6; i++)
      if ((now ^ last) & (1 << i)) {
        DBG.print(BTN_NAME[i]);
        DBG.println(now & (1 << i) ? F(" down") : F(" up"));
      }
    digitalWrite(LED, now != 0);
    last = now;
    delay(25);  // debounce
  }
  if (rfOn)
    while (HC.available()) DBG.write(HC.read());  // radio RX -> terminal: run r on a second board to see the PINGs
  switch (DBG.read()) {
    case 'v': vbat(); break;
    case 'r': rf(!rfOn); break;
    case 'o': oled(!oledOn); break;
    case 'a': hcAT(); break;
    case 'p':
      if (!rfOn) rf(true);
      HC.println(F("PING from remote_v2"));
      DBG.println(F("PING sent"));
      break;
    case 's': sleepNow(); break;
    case 'h': help(); break;
  }
}
