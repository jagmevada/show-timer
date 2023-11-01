#include <SPI.h>
#include <Wire.h>
// The project fails to build without the above two includes.
#include "functions.hpp"
// #define FIRSTTIM
void setup() {
  setup_io();
  setup_timer_b0();
  hc12r_turnoff();
  //hc12_setup_cold();
    mon.begin(9600);
  hc12_poweron_setup();
  hc12_turnon();
  sei(); // Enable global interrupts
  // set_sleep_mode(SLEEP_MODE_STANDBY );  
  // sleep_mode();
 mon.end();
 hc12_normalmode();
 delay(100);
 mon.begin(2400);
 stateset = HIGH;
 statestop = LOW;
}


void loop() {
static byte startstop;

if(stateset==HIGH){  // setting mode, restore from stored value from EEPROM
    stateset=LOW;
    initme();
    x=t0;
    x=1660;
  }

 //routine work  & start stop
if(statestop==HIGH){  // time hold (hc12 can resume it)
    while(statestop){  // if time is hold  1=hold, 0=resume
        //blinkx(x, 400);
        showtime(gettime(x),10);  // show x time for 10ms
        if ((millis()-timesince >2000))
        blinkx(x,90,8,9);  // to indicate the display is in hold state
        x=settime(x);//update x using hc12 data
        startstop=LOW; 
                if(stateset==HIGH){  /// in hold state if set button is pressed
                  stateset=LOW; // setting is stored, now resume statestop=0
                  x=t0;
                  }     
      }
}
else{  // time resumed

}


startstop=HIGH;  
    if(((x%9==6 ) || (x%9==7) )){  ///for 2 seconds in the middle display "next show"
        showtime(ts,SEC); 
    }
    else if(x<=50 && x>0){  // start blinking when time is less than 50seconds
        blinkx(x,500,5,6);  
        blinkx(x,500,5,6);         
    }
    else if(x==0){   // bursting mode when time is reached 0
        for(int i=0;i<20;i++)
        blinkx(0,(1000-i*50)/5+50,1,2);
        for(int i=0;i<10;i++)
        blinkx(0,(1000-i*50)/30+40,1,2);
        blinkx(0,200,1,10);
        blinkx(0,500,9,10);         
        
        //showtime(tx,(d-n)*timex/d);
        
        int restart=LOW;
        
        //initme();
        //x=t0;
        while(!restart){
        delay(40);        
        showtime(ts,700 );  //display "next show" on panel
        //showtime(ttest,500);
        showtime(tx,300);  // display two dots ":" on panel
        //showtime(ttest,500);   
        if(stateset==HIGH){   // restart the timer
          stateset=LOW;
          restart=HIGH;
          initme();   // get time x from memory and store in t0 structure
          x=t0;   
            }       
          }
        }
    else{   // tick counting down to 0
        ti=gettime(x);
        showtime(ti,SEC);
       }    
        //mon.println(x);
      x--;
// hc12 data decode method 20ms skip required to get 3 byte data 
    if(hc12.available()){
    rxdata data;
    data.byte = hc12.read();
    recieveprocess(data);
}


      
}


  // set_sleep_mode(SLEEP_MODE_STANDBY );  
  // sleep_mode();
//  if(Serial1.available()){
//    while(Serial1.available()>0){
//    mon.print(Serial1.read());}
//  }
//   delay(10);
//  if(mon.available()){
//    while(mon.available()>0){
//    Serial1.write(mon.read());
//    }
//    }
//   static uch xx=0;
//   if(Serial1.available()){
//     Serial1.write(Serial1.read());
//   }
//

 //hc12_turnoff();
//hc12_normalmode();
   //delay(5000);
//    hc12_setmode();
 //  hc12_turnon();
//  delay(2000);
  //mon.print("x");
//   hc12r_turnon();
//    hc12_turnon();
//   delay(1000);
//   hc12r_turnoff();
//  hc12_turnoff();
 
 
 


void debug_comlink(){
  static uch newdata;
delay(1000);
if(hc12.available()){
  while(hc12.available()>0){
  mon.print(hc12.read());
  }
}
// mon.println(x);
hc12.write(newdata++);
delay(100);
}





ISR(TCB0_INT_vect) {   /// 10ms timer
 static uch timercount=0;
     
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
