#ifndef FUNCTIONS_H
#define FUNCTIONS_H
 #define FIRSTTIME
#include <Arduino.h>
#include <HardwareSerial.h>
#include <megaTinyCore.h>

typedef unsigned char uch;
HardwareSerial &hc12 = Serial1;

#define RF_CH 30
#define DEFAULTBAUD 9600
#define SLOWBAUD 2400
#define TIMERADDR 12  // Max 1-15
#define POWER 8 
#define MYADDRESS 10


String channel_cmd = String("AT+C0")+String(RF_CH);
String power_cmd = String("AT+P")+String(POWER);
String baud_cmd = String("AT+B")+String(SLOWBAUD);
String check_cmd = String("AT");
String rx_cmd = String("AT+RX");

#define CHARGEPIN PIN_PA7        // 3
#define BATTERY_SENSPIN PIN_PA6  // 2
#define BTN_LEFT PIN_PA5         // 1  // S4
#define BTN_RIGHT PIN_PC3        // 13  // S5
#define BTN_UP PIN_PC0           // 10  // S1
#define BTN_OK PIN_PC1           // 11  // S2
#define BTN_DOWN PIN_PC2         // 12  // S3
#define BTN_K PIN_PB5            // for battery only
#define EXTRA_PIN PIN_PB4
#define HC_SW PIN_PA3
#define HC_SET PIN_PA4
#define OLED_SW PIN_PB3

#define PIN_SET0 HC_SET
#define PIN_M0 HC_SW

extern void printme();
extern void hc12_poweron_setup();



void hc12_turnon(){
  digitalWrite(PIN_M0,0);
};
void hc12_turnoff(){
  digitalWrite(PIN_M0,1);
};
void hc12_setmode(){
  digitalWrite(PIN_SET0,0);
};
void hc12_normalmode(){
  digitalWrite(PIN_SET0,1);
  delay(200);
};

void printme(){
        static uch c;
        if(hc12.available()){
         while(hc12.available()){
           c=hc12.read();}
        }
 }
void hc12_poweron_setup(){

  hc12_normalmode();
  delay(100);
  #ifdef FIRSTTIME
    hc12_turnoff();
    delay(100);
    hc12.begin(9600);
    hc12_setmode();
    hc12_turnon();
    delay(100);
    while(hc12.available())
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
    while(hc12.available())
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

void hc12_setup_hot(){
    //hc12_turnoff();
    hc12_normalmode();
    hc12_turnon();
    delay(1000);
    hc12.begin(2400);
    hc12_setmode();
};




#endif