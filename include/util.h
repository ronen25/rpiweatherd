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
#include <stdarg.h>
#include <syslog.h>

#include "logging.h"

#define PID_FILE "/tmp/rpiweatherd.pid"

#define SQLITE_UNIT_SECONDS "seconds"
#define SQLITE_UNIT_MINUTES "minutes"
#define SQLITE_UNIT_HOURS   "hours"
#define SQLITE_UNIT_DAYS	"days"

#define DIRECTION_STRING_FROM "from"
#define DIRECTION_STRING_TO "to"

#define PID_NUMBER_BUFFER_LENGTH	16
#define RPIWD_COPYFILE_BUFFSIZE     2048
#define RPIWD_DOUBLE_BUFFER_LENGTH   24

/* ASCII Art defines */
#define ASCII_TITLE "rpiweatherd, version %s\n" \
                    "Copyright (C) 2016-2017 Ronen Lapushner.\n\n" \
                    "License GPLv3+: GNU GPL version 3 or later " \
                    "<http://gnu.org/licenses/gpl.html>\n" \

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

/* File utilities */
int rpiwd_copyfile(const char *src, const char *dest);
int rpiwd_file_exists(const char *path);

/* Conversion Helpers */
int rpiwd_is_number(const char *str);

#define RPIWD_CELSIUS_TO_FARENHEIT(var) \
    ((var) = ((var) * 9 / 5 + 32))

#endif /* RPIWD_UTIL_H */
