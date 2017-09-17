 /*

Moteino Door Monitor

Copyright (C) 2016 by Xose PÃ©rez <xose dot perez at gmail dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "settings.h"
#include <RFM69Manager.h>
#include <LowPower.h>

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

RFM69Manager radio;
volatile boolean flag = false;
int status = HIGH;

// -----------------------------------------------------------------------------
// Hardware
// -----------------------------------------------------------------------------

void doorEvent() {
    flag = true;
}

void blink(byte times, byte mseconds) {
    pinMode(LED_PIN, OUTPUT);
    for (byte i=0; i<times; i++) {
        if (i>0) delay(mseconds);
        digitalWrite(LED_PIN, HIGH);
        delay(mseconds);
        digitalWrite(LED_PIN, LOW);
    }
}

void hardwareSetup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(LED_PIN, OUTPUT);
    pinMode(REED_PIN, INPUT);
    pinMode(BATTERY_PIN, INPUT);
    pinMode(BATTERY_ENABLE_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(REED_PIN), doorEvent, CHANGE);
    delay(1);

}

// -----------------------------------------------------------------------------
// RFM69
// -----------------------------------------------------------------------------

void radioSetup() {
    radio.initialize(FREQUENCY, NODEID, NETWORKID, ENCRYPTKEY, GATEWAYID, ATC_RSSI);
    radio.sleep();
}

// -----------------------------------------------------------------------------
// Messages
// -----------------------------------------------------------------------------

void sendStatus() {
    int newstatus = digitalRead(REED_PIN);
    if (radio.send((char *) "STA", (char *) ((newstatus == HIGH) ? "1" : "0"), (uint8_t) 2)) {

        // We only update the status variable if the message has been acknowledge.
        // This way if it fails, the status will be sent again after the next sleeping period of 4 seconds.
        status = newstatus;

    }
    radio.sleep();
}

void sendBattery() {

    unsigned int voltage;

    // John k2ox proposes this method to save power in a voltage
    // monitoring circuit based on a voltage divider. Check original post:
    // https://lowpowerlab.com/forum/moteino/battery-voltage-monitoring/
    pinMode(BATTERY_ENABLE_PIN, OUTPUT);
    digitalWrite(BATTERY_ENABLE_PIN, LOW);
    voltage = analogRead(BATTERY_PIN);
    pinMode(BATTERY_ENABLE_PIN, INPUT);

    // Map analog reading to VIN value
    voltage = BATTERY_RATIO * voltage;

    char buffer[6];
    sprintf(buffer, "%d", voltage);
    radio.send((char *) "BAT", buffer, (uint8_t) 2);
    radio.sleep();

}

void send()  {

    // Send current status
    sendStatus();

    // Send battery status once every 10 messages, starting with the first one
    static unsigned char batteryCountdown = 0;
    if (batteryCountdown == 0) sendBattery();
    batteryCountdown = (batteryCountdown + 1) % SEND_BATTERY_EVERY;

    // Show visual notification
    blink(1, NOTIFICATION_TIME);

}

// -----------------------------------------------------------------------------
// Common methods
// -----------------------------------------------------------------------------

void setup() {
    hardwareSetup();
    radioSetup();
}

void loop() {

    // We got here for three possible reasons:
    // - it's the first time (so we report status and battery)
    // - after 4*15 seconds (we report status and maybe battery)
    // - after an event (same)
    send();

    // Sleep loop
    // 15 times 4 seconds equals 1 minute,
    // but in real life messages are received every 77 seconds
    // with this set up, so I'm using 13 here instead...
    for (byte i = 0; i < SLEEP_CYCLE; i++) {

        // Sleep for 8 seconds (the maximum the WDT accepts)
        LowPower.powerDown(SLEEP_FOR, ADC_OFF, BOD_OFF);

        // At this point either 4 seconds have passed or
        // an interrupt has been triggered. If the later
        // delay execution for a few milliseconds to avoid
        // bouncing signals and break
        if (flag) {
            flag = false;
            delay(DEBOUNCE_INTERVAL);
            break;
        }

        // If the former, check status and quit if it has changed
        if (status != digitalRead(REED_PIN)) break;

    }

}
