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

#include "dht11.h"

int dht11_query_callback(int data_pin, float *arr) {
	uint8_t last_state = HIGH, count = 0, j = 0, i;
	int data[5] = { 0, 0, 0, 0, 0 };
	char buffer[DHT11_MAX_RESULT_STRING_LEN] = { 0 };
	int counter = 0;

	/* Pull down for 18ms and then up for 40us */
	pinMode(data_pin, OUTPUT);
	digitalWrite(data_pin, LOW);
	delay(18);

	digitalWrite(data_pin, HIGH);
	delayMicroseconds(40);

	/* Prepare to read */
	pinMode(data_pin, INPUT);

	/* Detect changes and read data */
	for (i = 0; i < DHT11_MAX_READS; i++) {
		counter = 0;
		while (digitalRead(data_pin) == last_state) {
			counter++;
			delayMicroseconds(1);

			if (counter == 255) break;
		}
		last_state = digitalRead(data_pin);

		if (counter == 255) break;

		if ((i >= 4) && (i % 2 == 0)) {
			data[j / 8] <<= 1;
			if (counter > 16)
				data[j / 8] |= 1;

			j++;
		}
	}

	/* Check if we have 5 bytes and that the data is correct */
	if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
		/* Print to the buffer to later convert it to a float */
		sprintf(buffer, "%d.%d", data[2], data[3]);
        arr[RPIWD_MEASURE_TEMPERATURE] = atof(buffer);

		/* Do the same for the humidity data, right after cleaning the buffer */
		memset(buffer, 0, DHT11_MAX_RESULT_STRING_LEN);
		sprintf(buffer, "%d.%d", data[0], data[1]);
        arr[RPIWD_MEASURE_HUMIDITY] = atof(buffer);
	}
	else
		return RPIWD_DEVRETCODE_DATA_FAILURE;

	return RPIWD_DEVRETCODE_SUCCESS;
}

int dht11_test(int data_pin) {
	/* DHT11 always pulls the data pin high. */
	return digitalRead(data_pin) == HIGH;
}
