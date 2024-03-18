// Microwave oven control - A simple C code based programm for controlling a microwave oven
//
// Copyright (C) 2023 Ulrich Scheipers <ulrich at scheipers dot org>
//
// Release 17.03.2023 - US
//
// Runs on ESP8266, Qifei TM1638 LED&KEY, YA-3Relay V3.11
//
// Uses a modified TM1638 library originally by Ricardo Batista
//
// This program is free software: you can redistribute it and/or modify it under the terms
// of the version 3 GNU General Public License as published by the Free Software Foundation.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <TM1638.h>
#include <TM16XXFontsUSMod.h> // Defines some new characters

#define SHOW_MINUTES true     // Show minutes and seconds on display vs. show only seconds

int LED = D4;  // internal LED, used for Heartbeat

TM1638 ledkey_module(D5, D6, D7, true, 7);  // Create ledkey_module(DIO, CLK, STB);

// Define states for statemachine
typedef enum { IDLE,
               HEATING,
               PAUSED } STATES;
STATES mw_mainState = IDLE;

// Define array for slow pulse width modulation for duration of one minute: PWM 100->8, 200->15, 400->30, 600->45, 800->60
boolean pwm_mat[5][60] = { { 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                           { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } };
int8_t pwm_i = 0; // Index for PWM, 0...60 seconds of a minute

// Define standard setup parameters
uint16_t mw_power[] = { 100, 200, 400, 600, 800 };  // Watts
int8_t mw_power_i = 4;                              // Power or Watts index 0...4 with full power being the default
int16_t mw_time = 0;                                // Timer in seconds with 60 seconds being the default
uint32_t mw_time_increment_ms = 0;                  // Internal counter in milliseconds
uint8_t mw_time_seconds = 0;              // Seconds for display
uint8_t mw_time_minutes = 0;              // Minutes for display
const uint16_t mw_time_max = 900;                   // Maximunm time, i.e. 900 seconds, 15 mimutes
bool mw_grill_on     = false;             // Grill on/off flag
bool mw_magnetron_on = false;             // Magentron on/off flag
bool mw_door_closed  = false;             // Door open/closed
char txt[9];                                        // Display string
uint8_t buttons = 0;                      // Button action
uint8_t buttonsLast = 0;                  // Last button for debouncing

void setup() {
  pinMode(LED, OUTPUT);  // Heatbeat LED

  pinMode(D5, OUTPUT);  // TM1638 DIO
  pinMode(D6, OUTPUT);  // TM1638 CLK
  pinMode(D7, OUTPUT);  // TM1638 STB

  pinMode(D0, OUTPUT);    // Relays 0 Power (i.e. light, etc.)
  pinMode(D1, OUTPUT);    // Relays 1 Magnetron
  pinMode(D2, OUTPUT);    // Relays 2 Grill
  digitalWrite(D0, LOW);  // Relays 0 Power (i.e. light, etc.)
  digitalWrite(D1, LOW);  // Relays 1 Magnetron
  digitalWrite(D2, LOW);  // Relays 2 Grill

  ledkey_module.clearDisplay();
  ledkey_module.setupDisplay(true, 3);

  sprintf(txt, "HALLO");
  ledkey_module.setDisplayToString(txt);
  delay(2000);
  sprintf(txt, "PUPSI");
  ledkey_module.setDisplayToString(txt);
  delay(500);
}

void update_display() {
  if (SHOW_MINUTES) {     // Display minutes and seconds
    mw_time_seconds = mw_time % 60;
    mw_time_minutes = mw_time / 60;
    sprintf(txt, "%3d%1c%2d%02d", mw_power[mw_power_i], mw_grill_on?char(137):char(32), mw_time_minutes, mw_time_seconds);  // Create text for 7-segment display TM1638
    ledkey_module.setDisplayToString(txt);  // Display created text/number output
    ledkey_module.setDisplayDigit(mw_time_minutes%10,5,true);
  } else {                // Diplay only seconds
    sprintf(txt, "%3d%1c%4d", mw_power[mw_power_i], mw_grill_on?char(137):char(32), mw_time);  // Create text for 7-segment display TM1638
    ledkey_module.setDisplayToString(txt);  // Display created text/number output
  }
  ledkey_module.setLEDs(buttons);         // Set LEDs
  if (false) {      // Just a display test to check if custom fonts are correctly included, needs to be false later on
    for (int i = 137; i<138; i++){        // 127 is custom grill character
      sprintf(txt, "%4d  %c", i, char(i)); 
      ledkey_module.setDisplayToString(txt);  // Display created text/number output
      delay(1000);
    }
  }
}

void loop() {
  // Heartbeat
  digitalWrite(LED, HIGH);
  delay(10);
  digitalWrite(LED, LOW);
  delay(50);

  // Get button input and visualize it
  buttons = ledkey_module.getButtons();
  if (buttons != buttonsLast) {
    buttonsLast = buttons;
    mw_door_closed = false;
    if((buttons & 0b00000001)>0) {  // Decrease power
      mw_power_i--;
      if (mw_power_i < 0) mw_power_i = 0;
    }
    if((buttons & 0b00000010)>0) {  // Increase power
      mw_power_i++;
      if (mw_power_i > 4) mw_power_i = 4;
    }
    if((buttons & 0b00000100)>0) {  // Decrease time
      if (mw_time < 150) mw_time -= 10;
      else if (mw_time >= 120 && mw_time <= 480) mw_time -= 30;
      else mw_time -= 60;
      if (mw_time < 0) mw_time = 0;
    }
    if((buttons & 0b00001000)>0) {  // Increase time
      if (mw_time < 120) mw_time += 10;
      else if (mw_time >= 120 && mw_time <= 480) mw_time += 30;
      else mw_time += 60;
      if (mw_time > mw_time_max) mw_time = mw_time_max;
    }
    if((buttons & 0b00010000)>0) {  // Switch grill
      mw_grill_on = !mw_grill_on;
    }
    if((buttons & 0b00100000)>0) {  // Switch magentron on/off
      mw_magnetron_on = !mw_magnetron_on;
    }
    if((buttons & 0b01000000)>0) {  // Door open/closed oder on/off
      mw_door_closed = true;
    }
    if((buttons & 0b10000000)>0) {  // Not used yet
    }
    //delay(10);
  }

  update_display();

  switch (mw_mainState) {

    // System is idling, i.e. time=0 AND magnetron off AND (door closed OR open) 
    case IDLE:
      {
        mw_magnetron_on = false;
        if (!mw_door_closed) {        // Door open
          digitalWrite(D0, true);     // Power on
        }
        else if (mw_door_closed) {    // Door closed
          digitalWrite(D0, false);    // Power off
        }
        if (mw_time > 0) {            // Time available again --> go pause
          mw_time_increment_ms = millis();
          mw_mainState = PAUSED;
        }
        break;
      }

    // System is heating, i.e. magnetron on AND door closed AND t>0
    case HEATING:
      {
        if (mw_time_increment_ms + 1000 <= millis()) {
          mw_time--;
          mw_time_increment_ms = millis();
          pwm_i++;
          if (pwm_i > 59) pwm_i = 0;
          digitalWrite(D0, true);                         // Power on
          digitalWrite(D1, pwm_mat[mw_power_i][pwm_i]);   // Magnetron PWM on
          digitalWrite(D2, mw_grill_on);                  // Grill on or off depending on flag
        }
        if (mw_time <= 0) {           // No time left --> go idle
          mw_mainState = IDLE;
          mw_time = 0;
          mw_magnetron_on = false;
          digitalWrite(D0, false);    // Power off
          digitalWrite(D1, false);    // Magnetron off
          digitalWrite(D2, false);    // Grill off
          break;
        }
        if (!mw_magnetron_on) {       // Magnetron off --> go pause
          mw_mainState = PAUSED;
          digitalWrite(D0, false);    // Power off
          digitalWrite(D1, false);    // Magnetron off
          digitalWrite(D2, false);    // Grill off
          break;
        }
        if (!mw_door_closed) {        // Door open --> go pause
          mw_mainState = PAUSED;
          digitalWrite(D0, true);     // Power on
          digitalWrite(D1, false);    // Magneton off
          digitalWrite(D2, false);    // Grill off
          break;
        }
        break;
      }

    // System paused, i.e. t>0 and (door open and magnetron on or door closed magnetron off)
    case PAUSED:
      {
        if (mw_time_increment_ms + 1000 <= millis()) {  // remember the time, i.e. do not decrease or set to zero
          mw_time_increment_ms = millis();
        }
        if (!mw_door_closed) {        // Door open
          digitalWrite(D0, true);     // Power on
        }
        else if (mw_door_closed) {    // Door closed
          digitalWrite(D0, false);    // Power off
        }
        if (mw_time <= 0 && mw_door_closed ) {                    // time <= 0 AND door closed --> go idle
          mw_mainState = IDLE;
          mw_magnetron_on = false;
          mw_time = 0;
          digitalWrite(D0, false);  // Power off
          digitalWrite(D1, false);  // Magneton off
          digitalWrite(D2, false);  // Grill off
        }
        if (mw_time <= 0 && !mw_door_closed ) {                   // time <= 0 AND door open --> go idle
          mw_mainState = IDLE;
          mw_magnetron_on = false;
          mw_time = 0;
          digitalWrite(D0, true);   // Power on
          digitalWrite(D1, false);  // Magneton off
          digitalWrite(D2, false);  // Grill off
        }
        if (mw_time > 0 && mw_magnetron_on && mw_door_closed) {   // Magnetron on and door closed --> go heating
          mw_mainState = HEATING;
        }
        if (mw_time > 0 && mw_magnetron_on && !mw_door_closed)   {   // time > 0 AND door open AND magnetron on --> go idle
          mw_mainState = IDLE;
          mw_magnetron_on = false;
          mw_time = 0;
          digitalWrite(D0, true);   // Power on
          digitalWrite(D1, false);  // Magneton off
          digitalWrite(D2, false);  // Grill off
        }
        break;
      }
  }
}
