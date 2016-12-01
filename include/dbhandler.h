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

#ifndef RPIWD_DBHANDLER_H
#define RPIWD_DBHANDLER_H

#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <parson.h>

#include "mqmsg.h"
#include "datastructures.h"
#include "confighandler.h"

/* General constants */
#define DB_DEFAULT_FILE_PATH                "/etc/rpiweatherd/rpiwd_data.db"
#define DATE_BUFFER_SIZE                    20
#define SQL_COMMAND_BUFFER_SIZE             512
#define RPIWD_DB_MQ_NAME                    "/rpiwd_db_mqueue"
#define DBHANDLER_MAX_FETCHED_ENTRIES       2048

/* time_t manipulation helpers */
#define DAY_START(t)                        ((t) - ((t) % 86400))
#define DAY_END(t)                          ((t) + 86399 - ((t) % 86400))

/* Error codes */
#define DBHANDLER_ERROR_SUCCESS 			 0
#define DBHANDLER_ERROR_TOO_MANY_ENTRIES	-1
#define DBHANDLER_ERROR_SQL_ERROR			-2
#define DBHANDLER_ERROR_NO_MEMORY			-3

/* Stat table names */
#define STAT_NAME_TOTAL_REQUESTS			"total_requests"
#define STAT_NAME_TOTAL_ENTRIES				"total_entries"

/* SQL table creation queries */
static const char *SQLCMD_TABLE_CREATION_QUERIES[] = {
        /* Data table */
        "CREATE TABLE IF NOT EXISTS tblData(" \
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, " \
        "RECORD_DATE TEXT NOT NULL, " \
        "TEMPERATURE FLOAT NOT NULL, " \
        "HUMIDITY FLOAT NOT NULL, " \
        "LOCATION TEXT NOT NULL, " \
        "DEVICE_NAME TEXT NOT NULL);",

        /* Statistics table */
        "CREATE TABLE IF NOT EXISTS tblStats(" \
        "KEY TEXT PRIMARY KEY NOT NULL, " \
        "DISPLAY_NAME TEXT NOT NULL," \
        "VALUE INTEGER NOT NULL);",

        NULL
};

/* Data entry write query */
static const char *SQLCMD_WRITE_ENTRY = "INSERT INTO tblData " \
                       "VALUES(null, datetime('now'), @temp, @humid, " \
                       "@location, @devicename);";

/* Data coun query */
static const char *SQLCMD_COUNT_ALL_ROWS = "SELECT COUNT(*) FROM tblData;";

/* Status table update queries */
static const char *SQLCMD_INCREASE_STAT =
        "UPDATE tblStats SET VALUE = VALUE + 1 WHERE KEY = '%s';";
static const char *SQLCMD_INSERT_INITIAL_STATS =
        "INSERT OR IGNORE INTO tblStats VALUES" \
        "('total_requests', 'Total count of requests', 0)," \
         "('total_entries', 'Total count of entries', 0);";
static const char *SQLCMD_SELECT_STATS =
        "SELECT DISPLAY_NAME, VALUE FROM tblStats";
static const char *SQLCMD_COUNT_INITIAL_STATS =
        "SELECT COUNT(*) FROM tblStats";

/* BY SPECIFC TIMEPOINT */
static const char *SQLCMD_COUNT_BY_DATE =
        "SELECT COUNT(*) FROM tblData WHERE datetime(RECORD_DATE)" \
        " %c datetime(%f, 'unixepoch', 'localtime');";
static const char *SQLCMD_READ_BY_DATE =
        "SELECT * FROM tblData WHERE datetime(RECORD_DATE)" \
        " %c datetime(%f, 'unixepoch', 'localtime');";

/* BY DATE RANGE */
static const char *SQLCMD_READ_BY_DATE_RANGE =
        "SELECT * FROM tblData WHERE datetime(RECORD_DATE) " \
        "BETWEEN datetime(%f, 'unixepoch', 'localtime') " \
        "AND datetime(%f, 'unixepoch', 'localtime');";
static const char *SQLCMD_COUNT_BY_DATE_RANGE =
        "SELECT COUNT(*) FROM tblData WHERE " \
        "datetime(RECORD_DATE) BETWEEN " \
        "datetime(%f, 'unixepoch', 'localtime') " \
        "AND datetime(%f, 'unixepoch', 'localtime');";

/* POSIX message queue ID for the DB thread */
mqd_t __db_mqd;

/* Init/quit functions */
int init_dbhandler(void);
void quit_dbhandler(void);
void quit_db_mq(void);

/* DB Thread event loop */
void *db_thread_event_loop(void *);
void db_thread_cleanup_routine(void *arg);

/* Request functions */
void request_write_entry(float temp, float humid, const char *location,
		const char *device);

/* Query preperation functions */
char *format_query(const char *format, ...);

/* General functions */
size_t exec_formatted_count_query(const char *count_query);
entrylist *exec_fetch_query(const char *fcountq, const char *fselectq, int *errcode);
key_value_list *exec_key_value_query(const char *fcountq, const char *fselectq,
		int *errcode);

/* Writing/reading functions */
static int write_raw_entry(float temp, float humid, const char *location, const char *device);
static void increase_stat(const char *stat_name);

/* Utility */
const char *dbhandler_strerror(int errcode);

#endif /* RPIWD_DBHANDLER_H */
