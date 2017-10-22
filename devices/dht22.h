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

#ifndef RPIWD_DHT22_H
#define RPIWD_DHT22_H

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "devicevals.h"
#include "confighandler.h"

#define DHT22_MAX_READS                 128
#define DHT22_MAX_RESULT_STRING_LEN     8 /* xxx.xxx + '\0' ==> 8 characters */

/* Internal helper callback */
static uint8_t sizecvt(const int read);

int dht22_query_callback(int data_pin, float *arr);
int dht22_test(int data_pin);

#endif /* RPIWD_DHT22_H */
