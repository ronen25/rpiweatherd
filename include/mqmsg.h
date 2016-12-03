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

#ifndef RPIWD_MQMSG_H
#define RPIWD_MQMSG_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <mqueue.h>

/* Message types */
#define DB_MSGTYPE_WRITEENTRY   100
#define DB_MSGTYPE_FETCH		101
#define DB_MSGTYPE_CURRENT		102
#define DB_MSGTYPE_STATS		103
#define DB_MSGTYPE_CONFIG		104

#define MQ_MAXMESSAGES		20
#define MQ_MAXMSGSIZE		1024

#define DB_MSG_NO_SOCKFD		-100

/* Message return codes */
#define RPIWD_MQ_RETCODE_OK				0
#define RPIWD_MQ_RETCODE_SQL_ERR		-1

/* DB Thread message structure */
typedef struct rpiwd_mqmsg_s {
	int mtype;						/* Operation type */
	int is_completed;				/* Used by HTTP listener */
	int sockfd;						/* Client socket to respond to (from HTTP handler) */
	int retcode;					/* Operation return code (used for logging, etc.) */
	mqd_t receiver_mq;				/* Reciever queue id (for read requests) */
    char *fcountq, *fselectq; 		/* Formatted count and selection queries */
    bool is_conversion_needed;		/* Used for fetch queries to determine whether to
                                       convert the measurment units used. */
	void *data;
} rpiwd_mqmsg;

/* Allocating/freeing dbhandler message structures */
rpiwd_mqmsg *rpiwd_mqmsg_alloc(void);
void rpiwd_mqmsg_free(rpiwd_mqmsg *ptr, int free_ptr);

#endif /* RPIWD_MQMSG_H */
