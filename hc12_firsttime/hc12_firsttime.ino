// remote_v2: one-time HC-12 setup, the old remote's FIRSTTIME sequence (timer_remote_code wireless.hpp,
// hc12_power_on_setup) with the NEW board's pins. Flash once after fitting a new HC-12, then power-cycle and
// flash the normal remote firmware.
//
// Arduino IDE (megaTinyCore): Board "ATtiny3226/1626/826/426", Chip ATtiny3226,
//   Clock "10 MHz internal" (the remote firmware assumes the 20 MHz oscillator / 2; 16/8 MHz options change the
//   oscillator fuse and the remote's baud rates would be wrong), Programmer "SerialUPDI - 230400 baud",
//   Sketch > Upload Using Programmer. J4: U + G (+ V if no battery).
//
// Result on the LED (and full log on J5, 115200):
//   LED steady ON      = module answered at 2400 after setup: done
//   LED fast blinking  = setup failed (no reply / not at 2400)

#define MYADDRESS 2              // the panel's address (its power-up display 00:NN); channel = 10 + address
#define RF_CH (10 + MYADDRESS)

#define LED     PIN_PA3          // HIGH = on
#define RF_EN   PIN_PB5          // LOW = HC-12 powered
#define OLED_EN PIN_PB4          // HIGH = OLED off
#define HC_SET  PIN_PC1          // LOW = AT mode
#define hc12    Serial           // PB2 TX / PB3 RX -> HC-12
#define mon     Serial1          // PA1 TX / PA2 RX -> J5 debug

void hc12_turn_on() { digitalWrite(RF_EN, LOW); }
void hc12_turn_off() { digitalWrite(RF_EN, HIGH); }
void hc12_set_mode() { digitalWrite(HC_SET, LOW); delay(100); }
void hc12_normal_mode() { digitalWrite(HC_SET, HIGH); delay(200); }

String reply;
void print_me() {  // collect + log the module's reply
  reply = "";
  while (hc12.available()) reply += (char)hc12.read();
  mon.println(reply);
}
void cmd(const String &c, int wait) {
  mon.print(c);
  mon.print(" -> ");
  hc12.print(c);
  delay(wait);
  print_me();
}

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(OLED_EN, HIGH);
  pinMode(OLED_EN, OUTPUT);
  digitalWrite(RF_EN, HIGH);
  pinMode(RF_EN, OUTPUT);
  pinMode(HC_SET, OUTPUT);
  mon.begin(115200);
  mon.println("\r\nHC-12 FIRSTTIME setup, channel " + String(RF_CH));

  hc12_normal_mode();
  delay(100);
  hc12_turn_off();               // cold start
  delay(2000);
  hc12_set_mode();               // SET low at power-up = AT mode at 9600
  hc12_turn_on();
  hc12.begin(9600);
  delay(100);
  while (hc12.available()) hc12.read();
  cmd("AT", 100);
  cmd(String("AT+C0") + String(RF_CH), 100);
  cmd("AT+P8", 100);
  cmd("AT+B2400", 100);
  cmd("AT", 100);
  cmd("AT+RX", 200);
  hc12_normal_mode();            // leave AT mode while powered: settings take effect
  delay(400);
  hc12.end();
  delay(100);
  hc12_set_mode();               // AT mode again, now at the stored baud
  hc12.begin(2400);
  delay(100);
  while (hc12.available()) hc12.read();
  cmd("AT", 100);
  bool ok = reply.indexOf("OK") >= 0;
  cmd("AT+RX", 100);
  hc12_normal_mode();
  hc12.end();
  hc12_turn_off();

  mon.println(ok ? "DONE: HC-12 answers at 2400" : "FAILED: no answer at 2400");
  if (ok) digitalWrite(LED, HIGH);
  else for (;;) { digitalWrite(LED, !digitalRead(LED)); delay(100); }
}

void loop() {}
