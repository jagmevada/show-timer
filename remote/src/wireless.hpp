#pragma once

// #define FIRSTTIME

#include <Arduino.h>
#include <HardwareSerial.h>

HardwareSerial &hc12 = Serial1;

#define MYADDRESS 1
#define RF_CH (10 + MYADDRESS)
#define DEFAULTBAUD 9600
#define SLOWBAUD 2400
#define TIMERADDR 12  // Max 1-15
#define POWER 8

const String channel_cmd = String("AT+C0") + String(RF_CH);
const String power_cmd = String("AT+P") + String(POWER);
const String baud_cmd = String("AT+B") + String(SLOWBAUD);
const String check_cmd = String("AT");
const String rx_cmd = String("AT+RX");

#define HC_SW PIN_PA3
#define HC_SET PIN_PA4

#define PIN_SET0 HC_SET
#define PIN_M0 HC_SW

void hc12_turn_on() { digitalWrite(PIN_M0, 0); };
void hc12_turn_off() { digitalWrite(PIN_M0, 1); };
void hc12_set_mode() { digitalWrite(PIN_SET0, 0); };
void hc12_normal_mode() {
  digitalWrite(PIN_SET0, 1);
  delay(200);
};

void print_me() {
  static char c;
  if (hc12.available()) {
    while (hc12.available()) {
      c = hc12.read();
    }
  }
}

void hc12_power_on_setup() {
  hc12_normal_mode();
  delay(100);

#ifdef FIRSTTIME
  hc12_turn_off();
  delay(100);
  hc12.begin(9600);
  hc12_set_mode();
  hc12_turn_on();
  delay(100);
  while (hc12.available()) hc12.read();
  hc12.print(check_cmd);
  delay(100);
  print_me();
  hc12.print(channel_cmd);
  delay(100);
  print_me();
  hc12.print(power_cmd);
  delay(100);
  print_me();
  hc12.print(baud_cmd);
  delay(100);
  print_me();
  hc12.print(check_cmd);
  delay(100);
  print_me();
  hc12.print(rx_cmd);
  delay(200);
  print_me();
  hc12_normal_mode();
  delay(400);
  hc12.end();
  delay(100);
  hc12_set_mode();
#endif

  hc12.begin(2400);
  delay(100);
  while (hc12.available()) hc12.read();

#ifdef FIRSTTIME
  hc12.print(check_cmd);
  delay(100);
  print_me();
  hc12.print(rx_cmd);
  delay(100);
  print_me();
#endif
};

void hc12_setup_hot() {
  // hc12_turn_off();
  hc12_normal_mode();
  hc12_turn_on();
  delay(1000);
  hc12.begin(2400);
  hc12_set_mode();
};