/*
 * rpiweatherd - A weather daemon for the Raspberry Pi that stores sensor data.
 * Copyright (C) 2016-2017 Ronen Lapushner
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* DHT22 driver code adapted from https://raw.githubusercontent.com/technion/lol_dht22 */

#include "dht22.h"

static uint8_t sizecvt(const int read) {
    /* digitalRead() and friends from wiringpi are defined as returning a value
       < 256. However, they are returned as int() types. This is a safety function */

    if (read > 255 || read < 0)
    {
        printf("Invalid data from wiringPi library\n");
        exit(EXIT_FAILURE);
    }
    return (uint8_t)read;
}

int dht22_query_callback(int data_pin, float *arr) {
    uint8_t laststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;
    int dht22_dat[5] = { 0 };

    dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

    // pull pin down for 18 milliseconds
    pinMode(data_pin, OUTPUT);
    digitalWrite(data_pin, HIGH);
    delay(10);
    digitalWrite(data_pin, LOW);
    delay(18);
    // then pull it up for 40 microseconds
    digitalWrite(data_pin, HIGH);
    delayMicroseconds(40);
    // prepare to read the pin
    pinMode(data_pin, INPUT);

    // detect change and read data
    for (i = 0; i < DHT22_MAX_READS; i++) {
        counter = 0;
        while (sizecvt(digitalRead(data_pin)) == laststate) {
            counter++;
            delayMicroseconds(1);
            if (counter == 255) {
                break;
            }
        }
        laststate = sizecvt(digitalRead(data_pin));

        if (counter == 255) break;

        // ignore first 3 transitions
        if ((i >= 4) && (i%2 == 0)) {
            // shove each bit into the storage bytes
            dht22_dat[j/8] <<= 1;
            if (counter > 16)
                dht22_dat[j/8] |= 1;
            j++;
        }
    }

    // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
    // print it out if data is good
    if ((j >= 40) &&
            (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

        /* Put into the array */
        arr[RPIWD_MEASURE_HUMIDITY] = h;
        arr[RPIWD_MEASURE_TEMPERATURE] = t;
    }
    else
        return RPIWD_DEVRETCODE_DATA_FAILURE;

    return RPIWD_DEVRETCODE_SUCCESS;
}

int dht22_test(int data_pin) {
    /* DHT22 always pulls the data pin high. */
    return digitalRead(data_pin) == HIGH;
}
