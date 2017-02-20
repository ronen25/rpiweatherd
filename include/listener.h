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

#ifndef RPIWD_LISTENER_H
#define RPIWD_LISTENER_H

#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <mqueue.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>

#include "mqmsg.h"
#include "dbhandler.h"
#include "http.h"
#include "device.h"
#include "confighandler.h"
#include "datastructures.h"
#include "measurevals.h"
#include "rpiweatherd_config.h"

#define MAX_WORKER_THREADS                       4
#define LISTENER_MQUEUE_MAX_MESSAGES             512
#define LISTENER_MAX_MESSAGE_SIZE                128
#define RPIWD_WORKER_QUEUE_NAME                  "/rpiwd_worker_mqueue"
#define RPIWD_MAXHOST                            128
#define STR_PORT_BUFFER_SIZE                     16
#define DEFAULT_SOCKET_TIMEOUT                   2
#define STATS_COMMAND_BUFFER_LENGTH              64
#define STATS_EXTRA_STATS_COUNT                  5

/* Callback return codes */
#define CALLBACK_RETCODE_SUCCESS                 0
#define CALLBACK_RETCODE_UNKNOWN_PARAM		-1001
#define CALLBACK_RETCODE_PARAM_ERROR		-1002
#define CALLBACK_RETCODE_UNKNOWN_COMMAND 	-1003
#define CALLBACK_RETCODE_PARAMS_MISSING		-1004
#define CALLBACK_RETCODE_TOO_MANY_PARAMS	-1005
#define CALLBACK_RETCODE_NO_PARAMS_NEEDED	-1006
#define CALLBACK_RETCODE_MEMORY_ERROR		-1007
#define CALLBACK_RETCODE_DEVICE_ERROR		-1008
#define CALLBACK_RETCODE_DUPLICATE_PARAMS       -1009

/* Command callback structure */
typedef struct cmd_callback_s {
	const char *cmd_name;
	int (*callback)(http_cmd *params, rpiwd_mqmsg *msgbuff);
} cmd_callback;

/* Init/quit */
void init_listener_loop(int num_worker_threads, int comm_port);
void listener_loop_cleanup_routine(void *);
void quit_listener_loop(void);
void quit_listener_mq(void);

/* Main listener loop callback passed to pthread_create() */
void *main_listener_loop(void *comm_port);
void *worker_listener_loop(void *arg);

/* Various utility methods */
int get_bound_socket(int port);

/* Command callbacks */
const char *command_callback_strerror(int errcode);
int dispatch_command(http_cmd *params, rpiwd_mqmsg *msgbuff);

int fetch_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff);
int current_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff);
int statistics_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff);
int config_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff);

#endif /* RPIWD_LISTENER_H */
