#include <SPI.h>
#include <Wire.h>
// The project fails to build without the above two includes.

#include <Arduino.h>

#include "lcdgfx.h"
#include "lcdgfx_gui.h"
// #include "owl.h"

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

#define ADCTIME 20              // multiply by 100ms
#define SYNCTIME 10             // multiply by 100ms
#define BMS_TRIGGER_PERIOD 100  // multiply by 100ms

DisplaySSD1306_128x64_I2C display(
    -1);  // or (-1,{busId, addr, scl, sda, frequency}). This line is suitable for most platforms by default
unsigned char resolution = 0;

int mcount = 0;
int fcount = 0;
int total = 0;
unsigned char chargingon = 0;
unsigned int batterylevel;
char cm[6];
char cf[6];
char ct[6];
char cbat[5];

unsigned char chargerstate = 0;
unsigned char buttonstate = 0;
unsigned char buttonupdate = 0;
uint8_t countupdate = 0;
uint16_t adc_reading = 0;
unsigned long t_charge = 0;
unsigned char chargerupdate = 1;
unsigned char displayupdate = 0;
unsigned char batterysenseupdate = 0;
unsigned char BMS_time = 2;

void isr_charger() {
    chargerupdate = 1;
    if (!digitalRead(CHARGEPIN)) {
        display.clear();
        digitalWrite(OLED_SW, 1);
        display.end();
    }
}

void isr_btn_up() { digitalWrite(HC_SW, 1); };

void isr_btn_ok() {
    buttonupdate = 1;
    fcount = 0;
    mcount = 0;
    // display.clear();
};

void isr_btn_right() { isr_btn_ok(); };

void isr_btn_left(){

};

void isr_btn_down() {
    static char x = 0;
    digitalWrite(HC_SET, x++ & 1);
};

void isr_btn_k() {
    BMS_time = 2;
};

class Remote {
    Remote() = default;
    int a;
};

void setup_display() {
    display.clear();
    digitalWrite(OLED_SW, 0);
    display.setFixedFont(ssd1306xled_font6x8);
    lcd_delay(10);
    display.clear();
    lcd_delay(100);
    display.begin();
    lcd_delay(100);
    display.clear();
    lcd_delay(100);
    display.setFixedFont(ssd1306xled_font6x8);
    lcd_delay(100);
};

void setup_io() {
    pinMode(OLED_SW, OUTPUT);
    pinMode(EXTRA_PIN, OUTPUT);
    pinMode(BTN_K, INPUT);
    pinMode(CHARGEPIN, INPUT);
    pinMode(BTN_UP, INPUT);
    pinMode(BTN_OK, INPUT);
    pinMode(BTN_DOWN, INPUT);
    pinMode(BTN_LEFT, INPUT);
    pinMode(BTN_RIGHT, INPUT);
    pinMode(HC_SW, OUTPUT);
    pinMode(HC_SET, OUTPUT);

    digitalWrite(EXTRA_PIN, 1);
    digitalWrite(OLED_SW, 1);
    digitalWrite(HC_SW, 1);
    digitalWrite(HC_SET, 1);
    digitalWrite(HC_SW, 0);
    attachInterrupt(CHARGEPIN, isr_charger, CHANGE);
    attachInterrupt(BTN_UP, isr_btn_up, FALLING);
    attachInterrupt(BTN_OK, isr_btn_ok, FALLING);
    attachInterrupt(BTN_DOWN, isr_btn_down, FALLING);
    attachInterrupt(BTN_LEFT, isr_btn_left, FALLING);
    attachInterrupt(BTN_RIGHT, isr_btn_right, FALLING);
    attachInterrupt(BTN_K, isr_btn_k, FALLING);
};

void setup_adc() {
    pinMode(BATTERY_SENSPIN, INPUT);
    analogReadResolution(12);
    analogReference(0x6);  // VDD);//INTERNAL2V048);VDDINTERNAL2V048
};

void setup_timer_b0() {
    TCB0.CCMP = 40000;  // CCMP/4MHz.
    TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm | TCB_RUNSTDBY_bm;
    TCB0.INTFLAGS = TCB_CAPT_bm;  // Clear any pending interrupt flags
    TCB0.INTCTRL = TCB_CAPT_bm;   // Enable TCB0 timeout interrupt
};

void counting() {
    total = mcount + fcount;
    sprintf(cm, "%d", mcount);  // analogRead(BUTTON));
    sprintf(cf, "%d", fcount);  // fcount);
    sprintf(ct, "%d", total);
}

void battery_sensing() {  // read Battery voltage
    batterylevel = map(analogRead(BATTERY_SENSPIN), 2400, 3480, 0,
                       100);  // map(analogRead(BATTERY_SENSPIN), 2457, 3440, 0, 100);
    if (batterylevel > 100) {
        batterylevel = 100;
    }
    if (!chargerstate) {
        sprintf(cbat, "%d", batterylevel);
    }
}

void battery_charge_state() {
    if (digitalRead(CHARGEPIN))
        chargerstate = 1;
    else
        chargerstate = 0;
}

void displaystate() {
    // display.clear();
    display.printFixedN(0, 1, "J :", STYLE_NORMAL, 1);
    display.printFixedN(48, 1, "         ", STYLE_NORMAL, 1);
    display.printFixedN(48, 1, cm, STYLE_NORMAL, 1);
    display.printFixedN(0, 20, "F :", STYLE_NORMAL, 1);
    display.printFixedN(48, 20, "        ", STYLE_NORMAL, 1);
    display.printFixedN(48, 20, cf, STYLE_NORMAL, 1);
    display.printFixedN(0, 38, "T :", STYLE_NORMAL, 1);
    display.printFixedN(48, 38, "        ", STYLE_NORMAL, 1);
    display.printFixedN(48, 38, ct, STYLE_NORMAL, 1);
    if (!chargerstate) {
        display.printFixedN(0, 60, "Bat:", STYLE_NORMAL, 0);

        if (batterylevel < 10) {
            display.printFixedN(24, 60, cbat, STYLE_NORMAL, 0);
            display.printFixedN(30, 60, "%  ", STYLE_NORMAL, 0);
        } else if (batterylevel > 99) {
            display.printFixedN(24, 60, cbat, STYLE_NORMAL, 0);
            display.printFixedN(42, 60, "%  ", STYLE_NORMAL, 0);
        } else {
            display.printFixedN(24, 60, cbat, STYLE_NORMAL, 0);
            display.printFixedN(36, 60, "%  ", STYLE_NORMAL, 0);
        };

    } else {
        display.printFixedN(24, 60, "Charging", STYLE_NORMAL, 0);
    };
}

void setup() {
    setup_io();
    setup_adc();
    setup_display();
    setup_timer_b0();
    sei();  // Enable global interrupts
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_mode();
}

void loop() {
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_mode();
    // fcount++;
    if (countupdate) {
        counting();
        countupdate = 0;
    }
    if (batterysenseupdate) {
        battery_sensing();
        batterysenseupdate = 0;
    }
    if (chargerupdate) {
        battery_charge_state();
        chargerupdate = 0;
    }
    if (displayupdate) {
        displaystate();
        displayupdate = 0;
    }
}

ISR(TCB0_INT_vect) {  /// 10ms timer
    static unsigned int timercount = 0, xx = 0;

    switch (timercount) {
        static uint8_t bat_adctime = ADCTIME;
        static uint8_t oled_synctime = SYNCTIME;
        static uint8_t counttime = SYNCTIME;
        case 0:
            if (buttonupdate == 1) {
                fcount = 0;
                mcount = 0;
                buttonupdate = 0;
            }
            break;
        case 2:
            digitalWrite(EXTRA_PIN, (xx++) & 1);
            if (BMS_time >= BMS_TRIGGER_PERIOD) {
                detachInterrupt(BTN_K);
                pinMode(BTN_K, OUTPUT);
                digitalWrite(BTN_K, 0);
                BMS_time = 0;
            } else if (BMS_time == 1) {
                digitalWrite(BTN_K, 1);
                pinMode(BTN_K, INPUT);
                attachInterrupt(BTN_K, isr_btn_k, FALLING);
            } else
                ;
            break;
        case 4:
            bat_adctime--;
            if (bat_adctime == 0) {
                bat_adctime = ADCTIME;
                batterysenseupdate = 1;
            }
            chargerupdate = 1;  // fail safe but consume clocks
            break;
        case 6:
            counttime--;
            if (counttime == 0) {
                countupdate = 1;
                mcount++;
                fcount++;
                counttime = SYNCTIME;
            }
            break;
        case 8:
            break;
        case 10:  // 10*10 ms round up
            timercount = 0;
            BMS_time++;
            oled_synctime--;
            if (oled_synctime == 0) {
                displayupdate = 1;
                oled_synctime = SYNCTIME;
            }
            break;
        default:
            break;
    }
    timercount++;
    TCB0.INTFLAGS = TCB_CAPT_bm; /* Clear the interrupt flag */
}
