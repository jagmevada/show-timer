#include <SPI.h>
#include <Wire.h>
// The project fails to build without the above two includes.

#include <Arduino.h>
#include <EEPROM.h>

#include "counter.hpp"
#include "lcdgfx.h"
#include "lcdgfx_gui.h"
#include "wireless.hpp"

#define CHARGEPIN PIN_PA7        // 3
#define BATTERY_SENSPIN PIN_PA6  // 2
#define LEFT_BUTTON PIN_PA5      // 1  // S4
#define RIGHT_BUTTON PIN_PC3     // 13  // S5
#define UP_BUTTON PIN_PC0        // 10  // S1
#define OK_BUTTON PIN_PC1        // 11  // S2
#define DOWN_BUTTON PIN_PC2      // 12  // S3
#define BMS_BUTTON \
  PIN_PB5  // battery management system button, for battery only
#define EXTRA_PIN PIN_PB4

#define OLED_SW PIN_PB3

// Analog to digital converter time?
#define ADCTIME 20  // multiply by 100ms
// Sync time?
#define SYNC_THRESHOLD 10          // multiply by 100ms
#define BMS_TRIGGER_THRESHOLD 100  // multiply by 100ms

DisplaySSD1306_128x64_I2C display(-1);
// or (-1,{busId, addr, scl, sda, frequency}). This line is suitable for most
// platforms by default

unsigned char charging_on = 0;
uint8_t battery_level;
char cbat[15];

ConfigurableDownCounter counter;
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

volatile bool data_available_on_serial = false;

char rx_data;
char crx_data[4];

unsigned char paritycheck(unsigned char data) {
  data ^= data >> 4;
  data ^= data >> 2;
  data ^= data >> 1;
  return data & 1;
}

typedef struct {
  unsigned int parity : 1;   // 1-bit field for parity
  unsigned int address : 4;  // 4-bit field for address
  unsigned int command : 3;  // 3-bit field for command
} protocol_byte;
typedef union {
  unsigned char byte;
  protocol_byte fields;
} protocol_data;

#define CMD_RESTART 0
#define CMD_RESUME 1
#define CMD_HOLD 2
#define CMD_SET_RESUME 3  // Will send payload
#define CMD_STATUS 4
#define CMD_NEWDATA 5
#define CMD_EEPROM 6  //
#define CMD_NONE2 7   //
volatile bool need_to_send = false;
volatile uint8_t command_to_send = 6;

protocol_data sent_data;

volatile bool wait_for_acknowledgement = false;
protocol_data recieve_data;

#define EEPROMADDR 8
volatile bool update_eeprom_display = false;

void isr_charger() {
  update_charger = true;
  if (!digitalRead(CHARGEPIN)) {
    digitalWrite(OLED_SW, 1);
    display.end();
  }
}

void isr_up_button() {
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

void isr_down_button() {
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

void isr_ok_button() {
  need_to_send = true;

  sent_data.fields.address = MYADDRESS;
  sent_data.fields.parity = 0;

  // Reset
  if (digitalRead(RIGHT_BUTTON) == LOW && digitalRead(LEFT_BUTTON) == HIGH) {
    uint8_t mins = EEPROM.read(EEPROMADDR);
    delay(10);
    uint8_t seconds = EEPROM.read(EEPROMADDR + 1);
    delay(10);

    counter = ConfigurableDownCounter(mins, seconds);

    sent_data.fields.command = CMD_RESTART;
    // is_paused = true;
  } else if (digitalRead(RIGHT_BUTTON) == LOW &&
             digitalRead(LEFT_BUTTON) == LOW) {
    // Write to EEPROM and start from there.
    unsigned char mins = (unsigned char)counter.GetMinutes();
    unsigned char seconds = (unsigned char)counter.GetSeconds();

    EEPROM.write(EEPROMADDR, mins);
    delay(10);
    EEPROM.write(EEPROMADDR + 1, seconds);
    delay(10);
    update_eeprom_display = true;

    sent_data.fields.command = CMD_EEPROM;
  } else {
    if (is_paused) {
      // press should resume
      sent_data.fields.command = CMD_SET_RESUME;
    } else {
      // press should pause
      sent_data.fields.command = CMD_HOLD;
    }
  }

  sent_data.fields.parity = paritycheck(sent_data.byte);
};

void isr_right_button() {
  if (is_paused) {
    update_display = true;
    position_to_update = min(position_to_update + 1, 3);
  }
};

void isr_left_button() {
  if (is_paused) {
    update_display = true;
    position_to_update = max(position_to_update - 1, 0);
  }
};

void isr_bms_button() { BMS_time = 2; };

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

  pinMode(BMS_BUTTON, INPUT);
  pinMode(CHARGEPIN, INPUT);
  pinMode(UP_BUTTON, INPUT);
  pinMode(OK_BUTTON, INPUT);
  pinMode(DOWN_BUTTON, INPUT);
  pinMode(LEFT_BUTTON, INPUT);
  pinMode(RIGHT_BUTTON, INPUT);

  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_SET0, OUTPUT);
  digitalWrite(PIN_M0, 1);    // 1-OFF, 0-ON
  digitalWrite(PIN_SET0, 0);  // 1-NOMRAL MODE, 0-SETTING MODE

  digitalWrite(EXTRA_PIN, 1);
  digitalWrite(OLED_SW, 1);
  digitalWrite(HC_SW, 1);
  digitalWrite(HC_SET, 1);
  digitalWrite(HC_SW, 0);

  attachInterrupt(CHARGEPIN, isr_charger, CHANGE);

  // All buttons are pulled up by default.
  attachInterrupt(UP_BUTTON, isr_up_button, FALLING);
  attachInterrupt(OK_BUTTON, isr_ok_button, FALLING);
  attachInterrupt(DOWN_BUTTON, isr_down_button, FALLING);
  attachInterrupt(LEFT_BUTTON, isr_left_button, FALLING);
  attachInterrupt(RIGHT_BUTTON, isr_right_button, FALLING);
  attachInterrupt(BMS_BUTTON, isr_bms_button, FALLING);
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

void battery_sensing() {  // read Battery voltage
  battery_level = map(analogRead(BATTERY_SENSPIN), 2400, 3480, 0, 100);
  // map(analogRead(BATTERY_SENSPIN), 2457, 3440, 0, 100);
  if (battery_level > 100) {
    battery_level = 100;
  }
  if (!charger_state) {
    snprintf(cbat, 15, "battery: %d%", battery_level);
  } else {
    snprintf(cbat, 12, "charging...");
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

  if (is_paused) {
    char marker[6];
    switch (position_to_update) {
      case 0:
        snprintf(marker, 6, "^    ");
        break;
      case 1:
        snprintf(marker, 6, " ^   ");
        break;
      case 2:
        snprintf(marker, 6, "   ^ ");
        break;
      case 3:
        snprintf(marker, 6, "    ^");
        break;
      default:
        snprintf(marker, 6, "^^^^^");
        break;
    }
    display.printFixedN(0, 34, marker, STYLE_NORMAL, 2);
  } else {
    display.printFixedN(0, 34, "     ", STYLE_NORMAL, 2);
  }

  // battery display
  display.printFixedN(0, 51, cbat, STYLE_NORMAL, 0);

  // dispaly address
  {
    static char address_display[7];
    snprintf(address_display, 7, "add:%d", MYADDRESS);
    display.printFixedN(82, 51, address_display, STYLE_NORMAL, 0);
  }

  // EEPROM display
  {
    static uint8_t mins = 0;
    static uint8_t seconds = 0;
    if (update_eeprom_display) {
      mins = EEPROM.read(EEPROMADDR);
      delay(10);
      seconds = EEPROM.read(EEPROMADDR + 1);
      delay(10);
      update_eeprom_display = false;
    }
    static char display_eeprom[17];
    snprintf(display_eeprom, 17, "eeprom : %d%d:%d%d", mins / 10, mins % 10,
             seconds / 10, seconds % 10);
    display.printFixedN(0, 58, display_eeprom, STYLE_NORMAL, 0);
  }
}

void setup() {
#ifdef FIRSTTIME
  EEPROM.write(EEPROMADDR, 20);
  delay(10);
  EEPROM.write(EEPROMADDR + 1, 0);
  delay(10);
#endif

  uint8_t mins = EEPROM.read(EEPROMADDR);
  delay(10);
  uint8_t seconds = EEPROM.read(EEPROMADDR + 1);
  delay(10);
  counter = ConfigurableDownCounter(mins, seconds);

  setup_io();
  setup_adc();
  setup_timer_b0();
  hc12_power_on_setup();
  hc12_normal_mode();

  setup_display();

  Serial.begin(2400);
  update_eeprom_display = true;

  // Enable global interrupts
  sei();
  // set_sleep_mode(SLEEP_MODE_STANDBY);
}

void loop() {
  // Enter sleep mode at the start of every iteration.
  // sleep_mode();

  // The following code is executed everytime an interrupt occures,
  // which causes an exit from the sleep mode.
  unsigned char mins;
  unsigned char seconds;
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
  if (need_to_send) {
    switch (sent_data.fields.command) {
      // mon.print("command RESUME received\n");
      break;
      case CMD_RESUME:  // 1
        // mon.print("command RESUME received\n");
        break;
      case CMD_RESTART:  // 0
      case CMD_HOLD:     // 2
        // mon.print("command HOLD received\n");
        hc12.write(sent_data.byte);
        break;
      case CMD_SET_RESUME:  // 3
        // mon.print("command SET received\n");
        hc12.write(sent_data.byte);

        mins = (unsigned char)counter.GetMinutes();
        mins |= (!paritycheck(mins)) << 7;

        seconds = (unsigned char)counter.GetSeconds();
        seconds |= (!paritycheck(seconds)) << 7;

        hc12.write(mins);
        hc12.write(seconds);
        break;
      case CMD_STATUS:  // 4
        // mon.print("command STATUS received\n");
        break;
      case CMD_NEWDATA:  // 5
        // mon.print("command NEWDATA received\n");
        break;
      case CMD_EEPROM:  // 6
        hc12.write(sent_data.byte);

        mins = (unsigned char)counter.GetMinutes();
        mins |= (!paritycheck(mins)) << 7;

        seconds = (unsigned char)counter.GetSeconds();
        seconds |= (!paritycheck(seconds)) << 7;

        hc12.write(mins);
        hc12.write(seconds);
        break;
      default:
        // mon.print("command %d not implemented\n", data.fields.command);
        break;
    }
    need_to_send = false;
    wait_for_acknowledgement = true;
  }
}

// Interrupt service routine for timer zero interrupt.
// Current being invoked every 10ms.
ISR(TCB0_INT_vect) {
  static uint32_t timer_count = 0;
  static uint8_t bat_adc_time = ADCTIME;
  static uint8_t oled_synctime = SYNC_THRESHOLD;

  // TL;DR: each case is executed every 100ms
  //
  // After the first round of timer_count increments,
  // each case is executed every 100ms (The ISR is invoked
  // every 10ms, at which point ONLY ONE switch case will be
  // executed, and timer_count will be incremented, essentially
  // doing a round-robin among the cases every 10ms, and coming
  // back to the same case every 10ms * NUM_SWITCH_CASES(10 in this case) =
  // 100ms)
  switch (timer_count) {
    case 0: {
      break;
    }
    case 2: {
      // The BMS module being used is designed such that
      // if the current being drawn is less the 40mA (the remote's
      // power draw is ~15mA), the module will go into sleep mode after
      // 15 seconds, essentially powering-off the whole system after 15
      // seconds; not something we want :). Therefore, we work around
      // this by doing the software equivalent of pushing the button
      // every 10s (for a duration of 100ms):
      if (BMS_time >= BMS_TRIGGER_THRESHOLD) {
        // Detach the interrupt as we want to write to it via software
        detachInterrupt(BMS_BUTTON);

        pinMode(BMS_BUTTON, OUTPUT);
        // The buttons are HIGH (aka, pulled-up) by default, and pushing
        // them makes the pin go LOW. Hence doing the same here.
        digitalWrite(BMS_BUTTON, LOW);

        BMS_time = 0;
      } else if (BMS_time == 1) {
        // This conditional is executed 100ms after the if-statement
        // above.
        digitalWrite(BMS_BUTTON, HIGH);
        pinMode(BMS_BUTTON, INPUT);
        attachInterrupt(BMS_BUTTON, isr_bms_button, FALLING);
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
      if (wait_for_acknowledgement) {
        if (hc12.available()) {
          recieve_data.byte = hc12.read();
          if (recieve_data.byte == sent_data.byte) {
            switch (sent_data.fields.command) {
              case CMD_RESUME:  // 1
                // mon.print("command RESUME received\n");
                break;
              case CMD_HOLD:  // 2
                // mon.print("command HOLD received\n");
                is_paused = true;
                update_display = true;
                break;
              case CMD_RESTART:     // 0
              case CMD_SET_RESUME:  // 3
                // mon.print("command SET received\n");
                is_paused = false;
                break;
              case CMD_STATUS:  // 4
                // mon.print("command STATUS received\n");
                break;
              case CMD_NEWDATA:  // 5
                // mon.print("command NEWDATA received\n");
                break;
              case CMD_EEPROM:  // 6
                is_paused = false;
                break;
              default:
                // mon.print("command %d not implemented\n",
                // data.fields.command);
                break;
            }
          }
        }
      }
      break;
    }
    // 10*10 ms round up
    case 10: {
      timer_count = 0;
      ++BMS_time;

      if (not is_paused) {
        --oled_synctime;
        if (oled_synctime == 0) {
          update_display = true;
          counter.CountDownByOneSecond();
          oled_synctime = SYNC_THRESHOLD;
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  ++timer_count;

  // Clear the interrupt flag
  TCB0.INTFLAGS = TCB_CAPT_bm;
}
