#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <Arduino.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <megaTinyCore.h>
#include <stdio.h>

// #define FIRSTTIME

#define SEC 996
#define EEPROMADDR 8

#define PIN_B0 PIN_PB4 // A  //F
#define PIN_B1 PIN_PA5 // B
#define PIN_B2 PIN_PA6 // C
#define PIN_B3 PIN_PA7 // D B3B2B1B0 is BCD code for selected digit
#define PIN_D0 PIN_PB5 // E
#define PIN_D1 PIN_PA4 // F  A D1D0 Selects digit
#define PA PIN_B0
#define PB PIN_B1
#define PC PIN_B2
#define PD PIN_B3
#define PE PIN_D0
#define PF PIN_D1

#define PIN_M0 PIN_PA3 // HC-12 module0 supply ON/OFF 1-OFF, 0-ON
#define PIN_SET0 \
  PIN_PC3                         // HC-12 module0 setting mode, 1-Normal mode, 0-setting mode
#define PIN_DISPLAY_POWER PIN_PB1 // ON/OFF  OF MAIN DIPOWERSPLAY 1-OFF, 0-ON
// #HC-12 module 1 is redundant, not to be used for JJ116
#define PIN_M1 PIN_PC0 // HC-12 module1 supply ON/OFF 1-OFF, 0-ON
#define PIN_SET1 \
  PIN_PB0 // HC-12 module1 setting mode, 1-Normal mode, 0-setting mode

#include "lcdgfx.h"
#ifndef MYADDRESS  // per panel (1..15, shown as 00:NN at power-up): build with -DMYADDRESS=N
#define MYADDRESS 1
#endif
#define DEFAULTBAUD 9600
#define SLOWBAUD 2400
#define TIMERADDR 12 // Max 1-15
#define POWER 8
#define RF_CH (10 + MYADDRESS)

String channel_cmd = String("AT+C0") + String(RF_CH);
String power_cmd = String("AT+P") + String(POWER);
String baud_cmd = String("AT+B") + String(SLOWBAUD);
String check_cmd = String("AT");
String rx_cmd = String("AT+RX");
void eepromwrite();
void eepromread();
// #define PIN_TX1 PIN_PA2 // turn off hc12 completely
// #define PIN_RX1 PIN_PA1 // turn off hc12 completely

typedef unsigned char uch;
typedef unsigned int unt;
typedef unsigned long int ulnt;
typedef struct
{
  unsigned int s0, s1, m0, m1;
} timestruct;
timestruct tx, ts, ti;
volatile uch stateset = LOW;
volatile uch statestop = LOW;
ulnt timesince = 0;
int t0 = 1200;
int x = 1200;
unt eeprom_x = 1200;
ulnt rxwait;
uch processnewdata;
uch staterestart;
HardwareSerial &hc12 = Serial1;
HardwareSerial &mon = Serial;
unsigned char connected = 0;

#include <stdio.h>

#define CMD_RESTART 0
#define CMD_RESUME 1
#define CMD_HOLD 2
#define CMD_SET_RESUME 3
#define CMD_STATUS 4
#define CMD_NEWDATA 5
#define CMD_EEPROM 6  //
#define CMD_EEPROM1 7 //

// Define a structure to represent the bitfields
typedef struct
{
  unsigned int parity : 1;  // 1-bit field for parity
  unsigned int address : 4; // 4-bit field for address
  unsigned int command : 3; // 3-bit field for command
} rxbyte;
typedef union
{
  uch byte;
  rxbyte fields;
} rxdata;

typedef struct
{
  unsigned char msb;
  unsigned char lsb;
} bytepayload;
typedef union
{
  unsigned int data;
  bytepayload fields;
} payload;
payload rnewdata;
rxdata rdata;
uch paritycheck(uch data)
{
  data ^= data >> 4;
  data ^= data >> 2;
  data ^= data >> 1;
  return data & 1;
}

void recieveprocess(rxdata data)
{
  if (paritycheck(data.byte) == 0)
  { // even parity{
    if (data.fields.address == MYADDRESS)
    {
      switch (data.fields.command)
      {
      case CMD_RESUME: // 1
        mon.println("RESUMED 1");
        statestop = LOW;
        hc12.write(data.byte);
        break;
      case CMD_HOLD: // 2
        mon.println("HOLD 2");
        statestop = HIGH;
        hc12.write(data.byte);
        break;
      case CMD_SET_RESUME: // 3
        if (paritycheck(rnewdata.fields.msb) == 1 &&
            paritycheck(rnewdata.fields.lsb) == 1)
        {
          mon.println("SET 3");
          x = (rnewdata.fields.msb & 0x7f) * 60 +
              (rnewdata.fields.lsb & 0x7f);
          // eepromwrite();
          statestop = LOW;
          hc12.write(data.byte);
        }
        break;

      case CMD_STATUS: // 4
        mon.println("STATUS 4");
        break;
      case CMD_NEWDATA: // 5
        mon.println("NEWDATA 5");
        break;
      case CMD_EEPROM:
        if (paritycheck(rnewdata.fields.msb) == 1 &&
            paritycheck(rnewdata.fields.lsb) == 1)
        {
          mon.println("EEPROM 6");
          eeprom_x = (rnewdata.fields.msb & 0x7f) * 60 +
                     (rnewdata.fields.lsb & 0x7f);
          eepromwrite();
          x = eeprom_x;
          statestop = LOW;
          hc12.write(data.byte);
        }
        break;
      case CMD_EEPROM1:
        mon.println("EEPROM 7");
        break;
      case CMD_RESTART:
        mon.println("RESTART 0");
        staterestart = HIGH;
        eepromread();
        statestop = LOW;
        hc12.write(data.byte);
        break;
      default:
        mon.print("command not implemented");
        mon.println(data.fields.command);
        break;
      }
      connected = HIGH;
    }
    else
    {
      mon.println("Invalid Address");
      mon.print(data.fields.address);
      mon.print("-");
      mon.println(data.fields.command);
    }
  }
  else
  {
    mon.println("parity failed");
  }
}

// void main(){
//     rxdata txdata;
//     txdata.fields.address = MYADDRESS;
//     txdata.fields.command = CMD_NEWDATA;
//     txdata.fields.parity = paritycheck((uch) txdata.byte);

//   uch receivedbyte= 0x53;
//   rxdata data;
//   data.byte= txdata.byte;
//   recieveprocess(data);
// }

extern void printme();
extern void hc12_poweron_setup();
void update_displaydigit(uch A, uch B);
void showtime(timestruct t, int del);

void blinkx(int x, int timex, int n, int d);
void initme();

void hc12_turnon() { digitalWrite(PIN_M0, 0); };
void hc12_turnoff()
{
  digitalWrite(PIN_M0, 1);
  // txrx_off();
};
void hc12_setmode() { digitalWrite(PIN_SET0, 0); };
void hc12_normalmode()
{
  digitalWrite(PIN_SET0, 1);
  delay(100);
};

void hc12r_turnon() { digitalWrite(PIN_M1, 0); };
void hc12r_turnoff() { digitalWrite(PIN_M1, 1); };
void hc12r_setmode() { digitalWrite(PIN_SET1, 0); };
void hc12r_normalmode() { digitalWrite(PIN_SET1, 1); };

void printme()
{
  static uch c;
  if (hc12.available())
  {
    while (hc12.available())
    {
      mon.write(hc12.read());
    }
  }
}

void hc12_poweron_setup()
{
  hc12_normalmode();
  delay(100);
#ifdef FIRSTTIME
  hc12_turnoff();
  delay(100);
  hc12.begin(9600);
  hc12_setmode();
  hc12_turnon();
  delay(100);
  while (hc12.available())
    hc12.read();
  hc12.print(check_cmd);
  delay(100);
  printme();
  hc12.print(channel_cmd);
  delay(100);
  printme();
  hc12.print(power_cmd);
  delay(100);
  printme();
  hc12.print(baud_cmd);
  delay(100);
  printme();
  hc12.print(check_cmd);
  delay(100);
  printme();
  hc12.print(rx_cmd);
  delay(200);
  printme();
  hc12_normalmode();
  delay(400);
  hc12.end();
  delay(100);
  hc12_setmode();
#endif
  hc12.begin(2400);
  delay(100);
  while (hc12.available())
    hc12.read();
#ifdef FIRSTTIME
  hc12.print(check_cmd);
  delay(100);
  printme();
  hc12.print(rx_cmd);
  delay(100);
  printme();
#endif
};

void hc12_setup_hot()
{
  // hc12_turnoff();
  hc12_normalmode();
  hc12_turnon();
  delay(1000);
  hc12.begin(9600);
  hc12_setmode();
};

void setup_io()
{
  // HC-12 MODULE Main IOs
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_SET0, OUTPUT);
  digitalWrite(PIN_M0, 1);   // 1-OFF, 0-ON
  digitalWrite(PIN_SET0, 0); // 1-NOMRAL MODE, 0-SETTING MODE

  // HC-12 MODULE Redundant IOs
  pinMode(PIN_M1, OUTPUT);
  pinMode(PIN_SET1, OUTPUT);
  digitalWrite(PIN_M1, 1);   // 1-OFF, 0-ON
  digitalWrite(PIN_SET1, 0); // 1-NOMRAL MODE, 0-SETTING MODE
                             // DISPLAY IOs
  pinMode(PIN_B0, OUTPUT);
  pinMode(PIN_B1, OUTPUT);
  pinMode(PIN_B2, OUTPUT);
  pinMode(PIN_B3, OUTPUT);
  pinMode(PIN_D0, OUTPUT);
  pinMode(PIN_D1, OUTPUT);
  digitalWrite(PIN_B0, 0);
  digitalWrite(PIN_B1, 0);
  digitalWrite(PIN_B2, 0);
  digitalWrite(PIN_B3, 0);
  digitalWrite(PIN_D0, 0);
  digitalWrite(PIN_D1, 0);
};

void setup_timer_b0()
{
  TCB0.CCMP = 40000; // CCMP/4MHz.
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm | TCB_RUNSTDBY_bm;
  TCB0.INTFLAGS = TCB_CAPT_bm; // Clear any pending interrupt flags
  TCB0.INTCTRL = TCB_CAPT_bm;  // Enable TCB0 timeout interrupt
};

int settime(int x)
{
  // this function not required
}

timestruct gettime(
    int x)
{ // convert time from absolute second format to m1m0:s1s0 format
  int m = 0, s = 0;
  timestruct times;
  m = x / 60;
  s = x - m * 60;
  times.m1 = (m / 10);
  times.m0 = m - times.m1 * 10;
  times.s1 = (s / 10);
  times.s0 = s - times.s1 * 10;
  return times;
}
void update_displaydigit(
    unsigned char A,
    unsigned char B)
{                                        // show 1 digit (BCD) on display at location B
                                         // (00-LSB-1st digit, 11-4th digit)
  digitalWrite(PIN_D0, (B >> 0) & 0x01); // E
  digitalWrite(PIN_D1, (B >> 1) & 0x01); // F

  digitalWrite(PIN_B3, (A >> 3) & 0x01); // d
  digitalWrite(PIN_B0, (A >> 0) & 0x01); // a
  digitalWrite(PIN_B1, (A >> 1) & 0x01); // b
  digitalWrite(PIN_B2, (A >> 2) & 0x01); // c
}

void showtime(timestruct t,
              int del)
{ // display time t and latch it for del seconds
  delay(1);
  update_displaydigit(t.m1, 3);
  delay(1);
  update_displaydigit(t.m0, 2);
  delay(1);
  update_displaydigit(t.s0, 0);
  // mon.print(t.m1);
  // mon.print(t.m0);
  // mon.print(t.s1);
  //   mon.println(t.s0);
  delay(1);
  update_displaydigit(t.s1, 1);
  delay(del);
}

void blinkx(int x, int timex, int n,
            int d)
{ // show time for n/d of timex delay and black for
  // remaining time of timex delay
  timestruct t;
  t = gettime(x);
  showtime(t, n * timex / d);        // visible period n/d
  showtime(tx, (d - n) * timex / d); // blank period (d-n)/d
}
void eepromwrite()
{ // used by EEPROM (6) CMD to write new timer data into
  // eeprom
  EEPROM.write(EEPROMADDR, uch(eeprom_x & 0xff));
  delay(10);
  EEPROM.write(EEPROMADDR + 1, uch(eeprom_x >> 8));
  delay(10);
}

void eepromread()
{ // used by restart (0) CMD to start timer with eeprom value
  // initialize timer from EEPROM
  unsigned char t00, t01;
  t00 = EEPROM.read(EEPROMADDR);
  delay(10);
  t01 = EEPROM.read(EEPROMADDR + 1);
  delay(10);
  unsigned int x1 = (unsigned int)(t01 << 8);
  unsigned int x0 = (unsigned int)(t00);
  t0 = (int)(x0 + x1);
  if (t0 <= 2) // 1200 default if store value less than lower limit
    t0 = 1200;
  if (t0 >= 6000) // 1200 default if store value exceed than upper limit
    t0 = 1200;
  x = t0;
  delay(10);
}
void initme()
{
  eepromread();
  tx.s1 = 14; // display two dots ":"
  tx.s0 = 14;
  tx.m1 = 14;
  tx.m0 = 14;

  ts.s1 = 15; // dislay "next show"
  ts.s0 = 15;
  ts.m1 = 15;
  ts.m0 = 15;

  // ttest.s0=1;
  // ttest.s1=1;
  // ttest.m0=1;
  // ttest.m1=1;
}

void call_routine()
{
  static byte startstop;
  timestruct ti;
  initme();
  x = t0;
  while (1)
  {
    if (stateset ==
        HIGH)
    { // setting mode, restore from stored value from EEPROM
      stateset = LOW;
      initme();
      x = t0;
    }
    else
    { // routine work  & start stop
      while (statestop)
      { // if time is hold  1=hold, 0=resume
        // blinkx(x, 400);
        showtime(gettime(x), 10); // show x time for 10ms
        if ((millis() - timesince > 2000))
          blinkx(x, 90, 8, 9); // to indicate the display is in hold state

        x = settime(x);
        if (stateset == HIGH)
        {                 /// in hold state if set button is pressed
          stateset = LOW; // setting is stored, now resume statestop=0
          x = t0;
        }
      }
      startstop = HIGH;
      if (((x % 9 == 6) ||
           (x % 9 == 7))) /// for 2 seconds in the middle display "next show"
        showtime(ts, SEC);
      else if (x <= 50 &&
               x > 0)
      { // start blinking when time is less than 50seconds
        blinkx(x, 500, 5, 6);
        blinkx(x, 500, 5, 6);
      }
      else if (x == 0)
      { // bursting mode when time is reached 0
        for (int i = 0; i < 20; i++)
          blinkx(0, (1000 - i * 50) / 5 + 50, 1, 2);
        for (int i = 0; i < 10; i++)
          blinkx(0, (1000 - i * 50) / 30 + 40, 1, 2);
        blinkx(0, 200, 1, 10);
        blinkx(0, 500, 9, 10);

        // showtime(tx,(d-n)*timex/d);

        int restart = LOW;

        // initme();
        // x=t0;
        while (!restart)
        {
          delay(40);
          showtime(ts, 700); // display "next show" on panel
          // showtime(ttest,500);
          showtime(tx, 300); // display two dots ":" on panel
          // showtime(ttest,500);
          if (stateset == HIGH)
          { // restart the timer
            stateset = LOW;
            restart = HIGH;
            initme(); // get time x from memory and store in t0 structure
            x = t0;
          }
        }
      }
      else
      { // tick counting down to 0
        ti = gettime(x);
        showtime(ti, SEC);
      }
      x--;
    }
  } // end while
}

#endif