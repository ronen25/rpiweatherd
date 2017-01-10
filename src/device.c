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

#include "device.h"

static pthread_mutex_t __mtx_device_lock = PTHREAD_MUTEX_INITIALIZER;
static device *__selected_device;

/* Array of supported device structures */
device supported_devices[] = {
	{ "dht11", 0, dht11_query_callback, dht11_test },
	{ NULL, 0, NULL, NULL }
};

int device_init_by_name(const char *device_name, int data_pin) {
	device *ptr = supported_devices;

	while (ptr->device_name) {
		if (strcmp(ptr->device_name, device_name) == 0) {
			/* Set pinout */
			pinMode(data_pin, INPUT);
			ptr->pin_data = data_pin;

			/* Finish */
			break;
		}

		ptr++;
	}

	if (!ptr)
		return RETCODE_DEVICE_INIT_UNKNOWN;

	/* Set the device */
	__selected_device = ptr;

	/* Return the structure to be copied over to the application.
	 * If device wasn't found return NULL */

	return RETCODE_DEVICE_INIT_OK;
}

/* A function to test a device */
int device_test_current(void) {
	int retflag;

	pthread_mutex_lock(&__mtx_device_lock);
		retflag = __selected_device->test_function(__selected_device->pin_data);
	pthread_mutex_unlock(&__mtx_device_lock);

	return retflag;
}

int device_query_current(float *arr) {
	int retflag;

	pthread_mutex_lock(&__mtx_device_lock);
        retflag = __selected_device->query_function(__selected_device->pin_data,
				arr);
	pthread_mutex_unlock(&__mtx_device_lock);

	return retflag;
}

device *get_current_device(void) {
	device *ptr = NULL;

	pthread_mutex_lock(&__mtx_device_lock);
		ptr = __selected_device;
	pthread_mutex_unlock(&__mtx_device_lock);

	return ptr;
}

/* Supported device names */
void print_supported_device_names(void) {
	device *ptr = supported_devices;

	printf("Supported devices:\n");
	while (ptr->device_name) {
		printf("%s, ", ptr->device_name);

		ptr++;
	}

	printf("\n");
}
