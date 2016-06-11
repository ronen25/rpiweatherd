/*
 * rpiweatherd - A weather daemon for the Raspberry Pi that stores sensor data.
 * Copyright (C) 2016 Ronen Lapushner
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

#ifndef RPIWD_CONFIGHANDLER_H
#define RPIWD_CONFIGHANDLER_H

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "ini.h"
#include "device.h"

#define CONFIG_FILE_DEFAULT_LOCATION 		"/etc/rpiweatherd/rpiweatherd.conf"
#define CONFIG_FILE_DEFAULT_FOLDER			"/etc/rpiweatherd"
#define CONFIG_LOCATION				 		"measure_location"
#define CONFIG_QUERY_INTERVAL		 		"query_interval"
#define CONFIG_DEVICE_NAME			 		"device_name"
#define CONFIG_DEVICE_CONFIG		 		"device_config"
#define CONFIG_UNITS				 		"units"
#define CONFIG_COMM_PORT			 		"comm_port"
#define CONFIG_NUM_WORKER_THREADS			"num_worker_threads"

#define CONFIG_ERROR_COMM_PORT 		 		-1
#define CONFIG_ERROR_DEVICE_CONFIG	 		-2
#define CONFIG_ERROR_NUM_WTHREADS			-3

/* Possible configuration values */
#define CONFIG_UNITS_METRIC					"metric"
#define CONFIG_UNITS_IMPERIAL				"imperial"
#define CONFIG_UNITS_UNITCHAR_METRIC		'm'
#define CONFIG_UNITS_UNITCHAR_IMPERIAL		'i'

/* Default values */
#define CONFIG_QUERY_INTERVAL_DEFAULT		"1h"
#define CONFIG_COMM_PORT_DEFAULT			6005
#define CONFIG_UNITS_DEFAULT				"imperial"
#define CONFIG_NUM_WORKER_THREADS_DEFAULT	1
#define CONFIG_NUM_WORKER_THREADS_MAX		4
#define CONFIG_MAX_QUERY_ATTEMPTS			64

typedef struct rpiwd_config_s {
	size_t config_count;
	char *measure_location;
	char *units;
	char *device_name;
	int device_config;
	char *query_interval;
	int comm_port;
	int num_worker_threads;
} rpiwd_config;

/* Internal callback */
int inih_callback(void *info, const char *section, const char *name, const char *value);

/* Actual function that parsed the file into a configuration structure */
int write_config_file(const char *path, rpiwd_config *confstrct);
int parse_config_file(const char *path, rpiwd_config *confstrct);

/* Allocating/freeing/zeroing configuration structures */
void free_config(rpiwd_config *confstrct);

/* Generation of a "blank" configuration file */
int generate_blank_config(const char *path);

/* A configuration checker */
int config_has_errors(rpiwd_config *confstrct);

/* Getting the current configuration */
int init_current_config(const char *config_path);
rpiwd_config *get_current_config(void);
void free_current_config(void);

#endif /* RPIWD_CONFIGHANDLER_H */
