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

#ifndef RPIWD_LOGGING_H
#define RPIWD_LOGGING_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#include <syslog.h>

void init_logging(bool is_foreground);
void quit_logging(void);

/* Logging function */
void rpiwd_log(int priority, const char *msg, ...);

#endif /* RPIWD_LOGGING_H */
