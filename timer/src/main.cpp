#include <SPI.h>
#include <Wire.h>
// The project fails to build without the above two includes.

#include <Arduino.h>

#include "lcdgfx.h"
#define RF_CH 30
#define DEFAULTBAUD 9600
#define SLOWBAUD 2400
#define TIMERADDR 12  // Max 1-15
#define POWER 8 

String channel_cmd = String("AT+C0")+String(RF_CH);
String power_cmd = String("AT+P")+String(POWER);
String baud_cmd = String("AT+B")+String(SLOWBAUD);
String check_cmd = String("AT");
String rx_cmd = String("AT+RX");
#define PIN_B0 PIN_PA4  //A
#define PIN_B1 PIN_PA5  //B
#define PIN_B2 PIN_PA6  //C
#define PIN_B3 PIN_PA7  //D B3B2B1B0 is BCD code for selected digit
#define PIN_D0 PIN_PB5  //E
#define PIN_D1 PIN_PB4  //F  D1D0 Selects digit
#define PIN_M0 PIN_PA3  //HC-12 module0 supply ON/OFF 1-OFF, 0-ON
#define PIN_SET0  PIN_PC3 //HC-12 module0 setting mode, 1-Normal mode, 0-setting mode
#define PIN_DISPLAY_POWER PIN_PB1 //ON/OFF POWER OF MAIN DISPLAY 1-OFF, 0-ON
//#HC-12 module 1 is redundant, not to be used for JJ116
#define PIN_M1 PIN_PC0  //HC-12 module1 supply ON/OFF 1-OFF, 0-ON
#define PIN_SET1 PIN_PB0 //HC-12 module1 setting mode, 1-Normal mode, 0-setting mode

void setup_io(){
// HC-12 MODULE Main IOs
pinMode(PIN_M0, OUTPUT);
pinMode(PIN_SET0, OUTPUT);
digitalWrite(PIN_M0,1);  //1-OFF, 0-ON
digitalWrite(PIN_SET0,0); // 1-NOMRAL MODE, 0-SETTING MODE

// HC-12 MODULE Redundant IOs
pinMode(PIN_M1, OUTPUT);
pinMode(PIN_SET1, OUTPUT);
digitalWrite(PIN_M1,1);  //1-OFF, 0-ON
digitalWrite(PIN_SET1,0); // 1-NOMRAL MODE, 0-SETTING MODE
  //DISPLAY IOs
pinMode(PIN_B0,OUTPUT);
pinMode(PIN_B1,OUTPUT);
pinMode(PIN_B2,OUTPUT);
pinMode(PIN_B3,OUTPUT);
pinMode(PIN_D0,OUTPUT);
pinMode(PIN_D1,OUTPUT);
digitalWrite(PIN_B0,0);
digitalWrite(PIN_B1,0);
digitalWrite(PIN_B2,0);
digitalWrite(PIN_B3,0);
digitalWrite(PIN_D0,0);
digitalWrite(PIN_D1,0);


};

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
};


void hc12r_turnon(){
  digitalWrite(PIN_M1,0);
};
void hc12r_turnoff(){
  digitalWrite(PIN_M1,1);
};
void hc12r_setmode(){
  digitalWrite(PIN_SET1,0);
};
void hc12r_normalmode(){
  digitalWrite(PIN_SET1,1);
};

void setup_timer_b0(){
  TCB0.CCMP = 40000;  // CCMP/4MHz. 
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm | TCB_RUNSTDBY_bm;
  TCB0.INTFLAGS = TCB_CAPT_bm;    // Clear any pending interrupt flags
  TCB0.INTCTRL = TCB_CAPT_bm;    // Enable TCB0 timeout interrupt
};

void printme(){
        if(Serial1.available()){
        while(Serial1.available()){
          Serial.write(Serial1.read());}
        }
}
void hc12_poweron_setup(){
  hc12_normalmode();
  delay(300);

    hc12_turnoff();
    delay(100);
    Serial1.begin(9600);
    hc12_setmode();
    hc12_turnon();
    delay(100);
    while(Serial1.available())
    Serial1.read();
    Serial1.print(check_cmd);
    delay(100);
    printme();
    Serial1.print(channel_cmd);
    delay(100);
    printme();
    Serial1.print(power_cmd);
    delay(100);
    printme();
    Serial1.print(baud_cmd);
    delay(100);
    printme();    
    Serial1.print(check_cmd);
    delay(100);
    printme();
    Serial1.print(rx_cmd);
    delay(200);
    printme();
    hc12_normalmode();
    delay(400);
    Serial1.end();
    delay(100);
    hc12_setmode();
    Serial1.begin(2400);
    
    
    delay(100);
    while(Serial1.available())
    Serial1.read();
       Serial1.print(check_cmd);
    delay(100);
    printme();
       Serial1.print(rx_cmd);
    delay(100);
    printme();
    hc12_normalmode();
    };

void hc12_setup_hot(){
    //hc12_turnoff();
    hc12_normalmode();
    hc12_turnon();
    delay(1000);
    Serial1.begin(9600);
    hc12_setmode();
};


void setup() {
  setup_io();
  setup_timer_b0();
  hc12r_turnoff();
  //hc12_setup_cold();
  
  Serial.begin(9600);
  hc12_poweron_setup();
  //  sei(); // Enable global interrupts
  // set_sleep_mode(SLEEP_MODE_STANDBY );  
  // sleep_mode();

}


void loop() {
  // set_sleep_mode(SLEEP_MODE_STANDBY );  
  // sleep_mode();
  if(Serial1.available()){
    Serial.write(Serial1.read());
  }
  if(Serial.available()){
    Serial1.write(Serial.read());
  }
  delay(10);
//   hc12r_turnon();
//    hc12_turnon();
//   delay(1000);
//   hc12r_turnoff();
//  hc12_turnoff();
 
 
}


ISR(TCB0_INT_vect) {   /// 10ms timer
 static unsigned int timercount=0;
     
  switch(timercount)
  {
    case 1:
    break;
    case 2:
    break;
    case 3:
    break;
    case 4:
    break;
    case 5:
    break;
    case 6:
    break;
    case 7:
    break;
    case 8:
    break;
    case 9:
    break;
    case 10:  // 10*10 ms round up
      timercount=0;      
      break;
  default:
    break;
    }
  timercount++;
 TCB0.INTFLAGS = TCB_CAPT_bm; /* Clear the interrupt flag */
};
