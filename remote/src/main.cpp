#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
// The project fails to build without the above two includes.


#include "functions.hpp"
#include "counter.hpp"
#include "lcdgfx.h"
#include "lcdgfx_gui.h"
// #include "owl.h"

// Analog to digital converter time?
#define ADCTIME 20  // multiply by 100ms
// Sync time?
#define SYNC_TIME 10            // multiply by 100ms
#define BMS_TRIGGER_PERIOD 100  // multiply by 100ms

DisplaySSD1306_128x64_I2C display(-1);
// or (-1,{busId, addr, scl, sda, frequency}). This line is suitable for most platforms by default

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
uch rxdata;
uch crxdata[4];

ConfigurableDownCounter counter(1, 03);
char down_counter_output[10];
volatile bool is_paused = false;
volatile uint8_t position_to_update = 0;

volatile bool charger_state = false;
volatile unsigned char button_state = 0;
volatile bool button_update = false;
volatile bool update_count = false;
volatile uint16_t adc_reading = 0;
volatile unsigned long t_charge = 0;
volatile bool update_charger = true;
volatile bool update_display = false;
volatile bool update_battery_sense = false;
volatile unsigned char BMS_time = 2;

void isr_charger() {
    update_charger = true;
    if (!digitalRead(CHARGEPIN)) {
        display.clear();
        digitalWrite(OLED_SW, 1);
        display.end();
    }
}

void isr_btn_up() {
    if (is_paused) {
        update_display = true;
        switch (position_to_update) {
            case 0: {
                counter.IncrementTensMinuteDigit();
                break;
            }
            case 1: {
                counter.IncrementOnesMinuteDigit();
                break;
            }
            case 2: {
                counter.IncrementTensSecondDigit();
                break;
            }
            case 3: {
                counter.IncrementOnesSecondDigit();
                break;
            }
            default: {
            }
        }
    }
};

void isr_btn_down() {
    if (is_paused) {
        update_display = true;
        switch (position_to_update) {
            case 0: {
                counter.DecrementTensMinuteDigit();
                break;
            }
            case 1: {
                counter.DecrementOnesMinuteDigit();
                break;
            }
            case 2: {
                counter.DecrementTensSecondDigit();
                break;
            }
            case 3: {
                counter.DecrementOnesSecondDigit();
                break;
            }
            default: {
            }
        }
    }
};

void isr_btn_ok() {
    is_paused = !is_paused;
    // oled_synctime = 0;
    // update_display = true;
};

void isr_btn_right() {
    if (is_paused) {
        position_to_update = min(position_to_update + 1, 3);
    }
};

void isr_btn_left() {
    if (is_paused) {
        position_to_update = max(position_to_update - 1, 0);
    }
};

void isr_btn_k() {
    BMS_time = 2;
};

void setup_display() {
    display.clear();
    // 0 == power on
    digitalWrite(OLED_SW, 0);

    display.setFixedFont(ssd1306xled_font6x8);
    lcd_delay(10);

    display.clear();
    lcd_delay(100);

    display.begin();
    lcd_delay(100);

    display.clear();
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

    pinMode(PIN_M0, OUTPUT);
    pinMode(PIN_SET0, OUTPUT);
    digitalWrite(PIN_M0,1);  //1-OFF, 0-ON
    digitalWrite(PIN_SET0,0); // 1-NOMRAL MODE, 0-SETTING MODE

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
    // Enable the counter peripheral
    TCB0.CTRLA = TCB_ENABLE_bm;
    // Use the peripheral clock's frequency / 2 (8MHz/2 = 4MHz)
    TCB0.CTRLA |= TCB_CLKSEL_CLKDIV2_gc;
    // Enable the peripheral to run in Standby sleep mode
    TCB0.CTRLA |= TCB_RUNSTDBY_bm;

    // CCMP/4MHz = 0.01 seconds = 10 milliseconds
    // That is, the ISR will be invoked every 10ms.
    TCB0.CCMP = 40000;

    // Clear any pending interrupt flags
    TCB0.INTFLAGS = TCB_CAPT_bm;
    // Enable TCB0 timeout interrupt
    TCB0.INTCTRL = TCB_CAPT_bm;
};

void counting() {
    total = mcount + fcount;
    sprintf(cm, "%d", mcount);  // analogRead(BUTTON));
    sprintf(cf, "%d", fcount);  // fcount);a
    sprintf(ct, "%d", total);
}

void battery_sensing() {  // read Battery voltage
    batterylevel = map(analogRead(BATTERY_SENSPIN), 2400, 3480, 0,
                       100);  // map(analogRead(BATTERY_SENSPIN), 2457, 3440, 0, 100);
    if (batterylevel > 100) {
        batterylevel = 100;
    }
    if (!charger_state) {
        sprintf(cbat, "%d", batterylevel);
    }
}

void battery_charge_state() {
    if (digitalRead(CHARGEPIN))
        charger_state = true;
    else
        charger_state = false;
}

void update_display_state() {
    counter.SetOutputString(down_counter_output);
    display.printFixedN(0, 2, down_counter_output, STYLE_NORMAL, 2);
    snprintf(crxdata, 4, "%d", rxdata);
    display.printFixedN(0,34, crxdata, STYLE_NORMAL, 1);
}

void setup() {
    setup_io();
    setup_adc();
    setup_display();
    setup_timer_b0();
    hc12_poweron_setup();
    hc12_normalmode();

    sei();  // Enable global interrupts
  //  set_sleep_mode(SLEEP_MODE_STANDBY);
}

void loop() {
    // Enter sleep mode at the start of every iteration.
   // sleep_mode();

    // The following code is executed everytime an interrupt occures,
    // which causes an exit from the sleep mode.
    if (update_count) {
        counting();
        update_count = false;
    }
    if (update_battery_sense) {
        battery_sensing();
        update_battery_sense = false;
    }
    if (update_charger) {
        battery_charge_state();
        update_charger = false;
    }
    if (update_display) {
        update_display_state();
        update_display = false;
    }
}

// Interrupt service routine for timer zero interrupt.
// Current being invoked every 10ms.
ISR(TCB0_INT_vect) {
    static unsigned int timer_count = 0;
    static uint8_t bat_adc_time = ADCTIME;
    static uint8_t oled_synctime = SYNC_TIME;
    static uint8_t count_time = SYNC_TIME;
    // static unsigned int xx = 0;

    // TL;DR: each case is executed every 100ms
    //
    // After the first round of timer_count increments,
    // each case is executed every 100ms (The ISR is invoked
    // every 10ms, at which point ONLY ONE switch case will be
    // executed, and timer_count will be incremented, essentially
    // doing a round-robin among the cases every 10ms, and coming
    // back to the same case every 10ms * NUM_SWITCH_CASES(10 in this case) = 100ms)
    switch (timer_count) {
        case 0: {
            if (button_update) {
                fcount = 0;
                mcount = 0;
                button_update = false;
            }
            break;
        }
        case 2: {
            // digitalWrite(EXTRA_PIN, (xx++) & 1);

            // The BMS module being used is designed such that
            // if the current being drawn is less the 40mA (the remote's
            // power draw is ~15mA), the module will go into sleep mode after
            // 15 seconds, essentially powering-off the whole system after 15
            // seconds; not something we want :). Therefore, we work around
            // this by doing the software equivalent of pushing the button
            // every 10s (for a duration of 100ms):
            if (BMS_time >= BMS_TRIGGER_PERIOD) {
                // Detach the interrupt as we want to write to it via software
                detachInterrupt(BTN_K);
                pinMode(BTN_K, OUTPUT);

                // The buttons are HIGH (aka, pulled-up) by default, and pushing
                // them makes the pin go LOW. Hence doing the same here.
                digitalWrite(BTN_K, LOW);
                BMS_time = 0;
            } else if (BMS_time == 1) {
                // This conditional is executed 100ms after the if statement
                // above.
                digitalWrite(BTN_K, 1);
                pinMode(BTN_K, INPUT);
                attachInterrupt(BTN_K, isr_btn_k, FALLING);
            } else {
            }
            break;
        }
        case 4: {
            bat_adc_time--;
            if (bat_adc_time == 0) {
                bat_adc_time = ADCTIME;
                update_battery_sense = true;
            }
            update_charger = true;  // fail safe but consume clocks
            break;
        }
        case 6: {
            count_time--;
            if (count_time == 0) {
                static uch xx=0;
                update_count = true;
                mcount++;
                fcount++;
                count_time = SYNC_TIME;
                
            }
            break;
        }

        // 10*10 ms round up
        case 10: {
            timer_count = 0;
            BMS_time++;

            if (not is_paused) {
                --oled_synctime;
                if (oled_synctime == 0) {
                    update_display = true;
                    counter.CountDownByOneSecond();
                    oled_synctime = SYNC_TIME;
                }
            }

            if(hc12.available()){
                rxdata=hc12.read();
                hc12.write(rxdata);
            }
            break;
        }
        default: {
            break;
        }
    }

    timer_count++;
    // Clear the interrupt flag
    TCB0.INTFLAGS = TCB_CAPT_bm;
}
