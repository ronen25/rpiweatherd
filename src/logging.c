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

#include "logging.h"

static bool __rpiwd_log_is_foreground;

void init_logging(bool is_foreground) {
    /* Set foreground flag */
    __rpiwd_log_is_foreground = is_foreground;

    /* Determine whether to open a syslog or to write to stderr */
    if (!__rpiwd_log_is_foreground)
        openlog("rpiweatherd", LOG_PID, LOG_USER);
}

void quit_logging(void) {
    if (!__rpiwd_log_is_foreground)
        closelog();
}

void rpiwd_log(int priority, const char *msg, ...) {
    va_list arglist;
    FILE *log_stream;

    /* Get argument list */
    va_start(arglist, msg);

    /* Log */
    if (__rpiwd_log_is_foreground) {
        if (priority == LOG_ERR)
            log_stream = stderr;
        else
            log_stream = stdout;

        vfprintf(log_stream, msg, arglist);
        fputc('\n', log_stream);
    }
    else
        vsyslog(priority, msg, arglist);

    va_end(arglist);
}
