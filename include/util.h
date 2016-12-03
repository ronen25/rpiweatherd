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

#ifndef RPIWD_UTIL_H
#define RPIWD_UTIL_H

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define PID_FILE "/tmp/rpiweatherd.pid"

#define SQLITE_UNIT_SECONDS "seconds"
#define SQLITE_UNIT_MINUTES "minutes"
#define SQLITE_UNIT_HOURS   "hours"
#define SQLITE_UNIT_DAYS	"days"

#define DIRECTION_STRING_FROM "from"
#define DIRECTION_STRING_TO "to"

#define PID_NUMBER_BUFFER_LENGTH	16

/* ASCII Art defines */
#define ASCII_TITLE "            _                    _   _                  _\n" \
	" _ __ _ __ (_)_      _____  __ _| |_| |__   ___ _ __ __| |\n" \
	"| '__| '_ \\| \\ \\ /\\ / / _ \\/ _` | __| '_ \\ / _ \\ '__/ _` |\n" \
	"| |  | |_) | |\\ V  V /  __/ (_| | |_| | | |  __/ | | (_| |\n" \
	"|_|  | .__/|_| \\_/\\_/ \\___|\\__,_|\\__|_| |_|\\___|_|  \\__,_| " \
	" Version %d.%d\n" \
	"     |_|                                                  \n"

/* Conversion from rpiwd time units to seconds */
unsigned int rpiwd_units_to_milliseconds(const char *unitstr);
int rpiwd_units_to_sqlite(const char *unitstr, char *sqlite_unit, float *unit_count);
char rpiwd_direction_to_char(const char *direction);
time_t rpiwd_units_to_time_t(const char *unitstr, char operation);
int is_rpiwd_units(const char *str);

/* Date/time parsing */
time_t normalize_date(const char *value, bool *rdtn_performed);

/* Custom sleep function */
void rpiwd_sleep(unsigned int milliseconds);

/* String manipulation */
char *rpiwd_getline(const char *line, const char *newline);

/* Daemon PID File and checks */
int pid_file_exists(void);
int write_pid_file(void);

#endif /* RPIWD_UTIL_H */
