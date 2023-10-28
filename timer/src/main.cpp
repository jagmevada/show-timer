#include <SPI.h>
#include <Wire.h>
// The project fails to build without the above two includes.

#include <Arduino.h>

#include <EEPROM.h>
#define P_SIZE 3
int SEC = 996;
int x = 1740;
int EEPROMADDR = 8;
int t0 = 1740;

typedef struct
{
  unsigned int s0, s1, m0, m1;
} timestruct;

timestruct tx, ts, ttest;

void setup()
{
  pinMode(13, OUTPUT);
  pinMode(10, OUTPUT); // e
  pinMode(9, OUTPUT);  // f
  pinMode(8, OUTPUT);  // d
  pinMode(7, OUTPUT);  // c
  pinMode(6, OUTPUT);  // b
  pinMode(5, OUTPUT);  // a
}

void loop()
{
  timestruct ti;
}