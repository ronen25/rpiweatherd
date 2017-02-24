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

#ifndef RPIWD_DEVICE_H
#define RPIWD_DEVICE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <wiringPi.h>

#include "devicevals.h"
#include "dht11.h"

/* Function return codes */
#define RETCODE_DEVICE_INIT_OK          0
#define RETCODE_DEVICE_INIT_UNKNOWN     -1

/* Constants */
#define ATTEMPT_LOG_THRESHOLD           5

/* Typedefs for basic callbacks */
typedef int (* device_callback)(int data_pin, float *arr);
typedef int (* device_test_callback)(int data_pin);

typedef struct device_s {
	const char *device_name;
	int pin_data;
	device_callback query_function;
	device_test_callback test_function;
} device;

extern device supported_devices[];

/* The function actually used by the program; determines what device to initialize
 * based on device name. */
int device_init_by_name(const char *device_name, int data_pin);

/* A function to test a device */
int device_test_current(void);

/* A function to query the current device */
int device_query_current(float *arr);

/* Get the selected device */
device *get_current_device(void);

/* Supported device names */
void print_supported_device_names(void);

#endif /* RPIWD_DEVICE_H */
