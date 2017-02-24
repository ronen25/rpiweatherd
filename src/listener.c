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

#include "listener.h"

static pthread_t __listener_thread_id;
static int __num_workers;
static mqd_t __worker_mqueue;
static pthread_t *__workers;

/* Host table mutex */
static pthread_mutex_t host_table_mtx = PTHREAD_MUTEX_INITIALIZER;

/* =================================================================================== */

/* Command callback table */
static cmd_callback CMD_CALLBACK_TABLE[] = {
	{ "fetch", fetch_command_callback },
	{ "current", current_command_callback },
	{ "statistics", statistics_command_callback },
	{ "config", config_command_callback },
	{ NULL, NULL }
};

/* =================================================================================== */

/* Init/quit */
void init_listener_loop(int num_worker_threads, int comm_port) {
	/* Initialize main worker thread */
	int result = pthread_create(&__listener_thread_id, NULL, main_listener_loop,
			(void *)comm_port);

	if (result != 0) {
        rpiwd_log(LOG_ERR, "Unable to create main listener thread: %s.", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Set worker thread count globally */
	if (num_worker_threads > MAX_WORKER_THREADS) {
        rpiwd_log(LOG_WARNING, "num_worker_threads is bigger then %d; using max instead.",
				MAX_WORKER_THREADS);
		num_worker_threads = MAX_WORKER_THREADS;
	}

	__num_workers = num_worker_threads;
}

void quit_listener_loop(void) {
	int flag = 0;

	/* Cancel main listener thread.
	 * That thread's cleanup routine will make sure to cancel all worker threads. */
	flag = pthread_cancel(__listener_thread_id);
	pthread_join(__listener_thread_id, NULL);

	/* Check return flag */
	if (flag == -1)
        rpiwd_log(LOG_ERR, "listener thread terminated with an error.");

	/* Free descriptor array pointer */
	free(__workers);
}

/* Main listener loop callback passed to pthread_create() */
void *main_listener_loop(void *comm_port) {
	int oldstate;
	int i, flag;
	void *retval;
	int sockfd, clientsock;
	socklen_t addrlen;
	struct mq_attr attr;
	struct sockaddr_storage claddr;
	struct timeval tv = { .tv_sec = DEFAULT_SOCKET_TIMEOUT, .tv_usec = 0 };
	char host[RPIWD_MAXHOST];
	rpiwd_mqmsg msgbuff;

	/* Initialize worker thread array */
	__workers = malloc(sizeof(pthread_t) * __num_workers);
	if (!__workers) {
        rpiwd_log(LOG_ERR, "Unable to allocate worker thread descriptor array: %s",
				strerror(errno));
		return (void *) -1;
	}

	/* Initialize the message queue used by the workers */
	attr.mq_flags = attr.mq_curmsgs = 0;
	attr.mq_maxmsg = LISTENER_MQUEUE_MAX_MESSAGES;
	attr.mq_msgsize = LISTENER_MAX_MESSAGE_SIZE;
	__worker_mqueue = mq_open(RPIWD_WORKER_QUEUE_NAME, O_CREAT | O_RDWR, 0666, &attr);
	if (__worker_mqueue == (mqd_t) -1) {
        rpiwd_log(LOG_ERR, "error creating worker thread queue: %s", strerror(errno));
		return (void * )-1;
	}

	/* Initialize each one of the worker threads */
	for (i = 0; i < __num_workers; i++) {
		flag = pthread_create(&__workers[i], NULL, worker_listener_loop, NULL);
		if (flag != 0) {
            rpiwd_log(LOG_ERR, "error creating worker thread: %s", strerror(errno));
			return (void *) -2;
		}
	}

	/* Set thread cancelability */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldstate);

	/* Prepare main listener socket */
	sockfd = get_bound_socket((int)comm_port);
	if (sockfd == -1) {
        rpiwd_log(LOG_ERR, "Could not get bound socket: %s", strerror(errno));
		return (void *) -1;
	}

	/* Register cleanup handler */
	pthread_cleanup_push(listener_loop_cleanup_routine, (void *)sockfd);

	/* Listen */
	listen(sockfd, SOMAXCONN);

	/* Main listener loop */
	for (;;) {
		/* Accept connection and pass it immediately to a currently
		 * available worker thread. */
		addrlen = sizeof(struct sockaddr_storage);
		clientsock = accept(sockfd, (struct sockaddr *)&claddr, &addrlen);
		if (clientsock == -1) {
            rpiwd_log(LOG_ERR, "error accepting connection: %s", strerror(errno));
			continue;
		}

		/* Set socket timeout */
		setsockopt(clientsock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, 
				sizeof(struct timeval));

        /* Build message */
        rpiwd_mqmsg_init(&msgbuff);
        msgbuff.sockfd = clientsock;
        msgbuff.receiver_mq = __worker_mqueue;

		/* Send to workers queue */
		mq_send(__worker_mqueue, (const char *)&msgbuff, sizeof(rpiwd_mqmsg), 0);
	}

	/* Cleanup */
	pthread_cleanup_pop(0);
}

void listener_loop_cleanup_routine(void *arg) {
	int sockfd = (int)arg;
	void *retval;

	/* Cancel all worker threads */
	for (int i = 0; i < __num_workers; i++) {
		pthread_cancel(__workers[i]);
		pthread_join(__workers[i], &retval);
	}

	/* Close main listener socket */
	close(sockfd);

	/* Quit listener queue */
	quit_listener_mq();
}

void quit_listener_mq(void) {
	/* Cleanup */
	mq_close(__worker_mqueue);
	mq_unlink(RPIWD_WORKER_QUEUE_NAME);
}

void *worker_listener_loop(void *arg) {
	rpiwd_mqmsg msgbuff;
	http_cmd *cmd;
	ssize_t flag = 0;
	int cmd_status = 0, response = HTTP_PARSER_ERROR_SUCCESS, is_eof = 0;
	JSON_Value *jval = NULL;

	while ((flag = mq_receive(__worker_mqueue, (char *)&msgbuff, MQ_MAXMSGSIZE,
					NULL)) != -1) {
		/* Check if request is completed */
		if (!msgbuff.is_completed) {
			/* Read and parse HTTP request */
			cmd = read_and_parse_response(msgbuff.sockfd, &response, &is_eof);
			if (!cmd) {
				if (!is_eof) {
					/* Check response code */
					send_http_error_response(msgbuff.sockfd, 
							HTTP_CODE_REQUEST_BAD_REQUEST,
							HTTP_CODE_REQUEST_BAD_REQUEST,
							http_parser_strerror(response));
				}

				/* Finish response */
				end_response(msgbuff.sockfd, cmd);

				continue;
			}

			/* Check if the command is any "special" browser stuff.
			 * Might be a request for a favico.ico, text/html (Midori does this),
			 * etc. */
			if (strcmp(cmd->cmdname, "favicon.ico") == 0 || 
				strcmp(cmd->cmdname, "text-html") == 0) {
				/* Send 204 No Content */
				send_response(msgbuff.sockfd, HTTP_CODE_NO_CONTENT, NULL);

				/* Finish response */
				end_response(msgbuff.sockfd, cmd);

				continue;
            }

			/* Dispatch command callback */
			cmd_status = dispatch_command(cmd, &msgbuff);
			if (cmd_status != CALLBACK_RETCODE_SUCCESS) {
				flag = send_http_error_response(msgbuff.sockfd, 
						HTTP_CODE_REQUEST_BAD_REQUEST,
						cmd_status,
						command_callback_strerror(cmd_status));
				
				/* Free all */
				end_response(msgbuff.sockfd, cmd);

				continue;
			}
			
			/* Send to DB thread to finish processing (if needed) */
			if (msgbuff.mtype == DB_MSGTYPE_FETCH || msgbuff.mtype == DB_MSGTYPE_STATS)
				mq_send(__db_mqd, (const char *)&msgbuff, sizeof(rpiwd_mqmsg), 0);
			else
				mq_send(__worker_mqueue, (const char *)&msgbuff, sizeof(rpiwd_mqmsg), 0);

			if (cmd)
				http_cmd_free(cmd);
		}
		else {
			/* Check response type, and get value accordingly */
			switch (msgbuff.mtype) {
				case DB_MSGTYPE_FETCH:
                    jval = entrylist_to_json_value((entrylist **)&msgbuff.data,
                                                   msgbuff.unitstr);
					break;
				case DB_MSGTYPE_CURRENT:
                    jval = entry_to_json_value((entry *)msgbuff.data, msgbuff.unitstr);
					break;
				case DB_MSGTYPE_STATS:
				case DB_MSGTYPE_CONFIG:
					jval = key_value_list_to_json_value((key_value_list **)&msgbuff.data);
					break;
			}

			/* If something was received, generate appropriate 
			 * HTTP response and send to client */
            if (jval) {
				/* Serialize and send */
				char *serialized = json_serialize_to_string(jval);

				/* Send it */
				flag = send_response(msgbuff.sockfd, HTTP_CODE_OK, serialized);
				close(msgbuff.sockfd);
						
				/* Free buffers */
				if (msgbuff.fselectq)
					free(msgbuff.fselectq);
	
				if (msgbuff.fcountq)
					free(msgbuff.fcountq);
	
				/* Free JSON values/buffers */
				json_free_serialized_string(serialized);
				json_value_free(jval);
	
				/* Free data */
				switch (msgbuff.mtype) {
					case DB_MSGTYPE_FETCH:
						entrylist_free((entrylist *)msgbuff.data);
						break;
					case DB_MSGTYPE_CURRENT:
						entry_ptr_free((entry *)msgbuff.data);
						break;
					case DB_MSGTYPE_STATS:
					case DB_MSGTYPE_CONFIG:
						key_value_list_free((key_value_list *)msgbuff.data);
						break;
				}
			}
			else {
				/* Some error has occurred... */
				send_http_error_response(msgbuff.sockfd,
						HTTP_CODE_REQUEST_BAD_REQUEST,
						msgbuff.retcode,
						dbhandler_strerror(msgbuff.retcode));

				/* Finish response */
				close(msgbuff.sockfd);
						
                continue;
			}
		}
    }

	return (void *) 0;
}

int get_bound_socket(int port) {
	struct addrinfo hints = { 0 };
	struct addrinfo *result, *rp;
	int optval = 1, sockfd;
	char str_port[STR_PORT_BUFFER_SIZE];

	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

	/* Print to str_port */
	sprintf(str_port, "%d", port);

	if (getaddrinfo(NULL, str_port, &hints, &result) != 0) {
        rpiwd_log(LOG_ERR, "getaddrinfo error: %s", strerror(errno));
		return -1;
	}

	/* Walk through results and check all sockets */
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1)
			continue;

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
			return -1;

		if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
			return sockfd;
		else
			return -1;

		/* On failure close socket */
		close(sockfd);
	}

	freeaddrinfo(result);

	/* Loop should eventually return; if it doesn't, this is reached.
	 * Should return an error. */
	return -1;
}

/* Command callbacks */
const char *command_callback_strerror(int errcode) {
	switch (errcode) {
		case CALLBACK_RETCODE_SUCCESS:
			return "Success";
		case CALLBACK_RETCODE_UNKNOWN_PARAM:
			return "Query contains unknown parameters; please refer to documentation.";
		case CALLBACK_RETCODE_PARAM_ERROR:
			return "Query parameter has incorrect value.";
		case CALLBACK_RETCODE_UNKNOWN_COMMAND:
			return "Unrecognized command.";
		case CALLBACK_RETCODE_PARAMS_MISSING:
			return "Command requires parameters.";
		case CALLBACK_RETCODE_TOO_MANY_PARAMS:
			return "Too many parameters.";
		case CALLBACK_RETCODE_NO_PARAMS_NEEDED:
			return "Parameters provided to command but the command does "\
				   "not accept any arguments.";
	}

	return "Unknown command callback error.";
}

int dispatch_command(http_cmd *params, rpiwd_mqmsg *msgbuff) {
	cmd_callback *ptr = &CMD_CALLBACK_TABLE[0];

	while (ptr->cmd_name)
        if (strcmp(ptr->cmd_name, params->cmdname) != 0)
            ptr++;
        else break;

    if (ptr->cmd_name)
        return ptr->callback(params, msgbuff);
	else
		return CALLBACK_RETCODE_UNKNOWN_COMMAND;
}

int fetch_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff) {
	time_t from = 0, to = 0, on = 0, temp;
	http_cmd_param *ptr;
    char *retval;
    int retflag = 0, select = 0;
    bool rdtn_performed;

	/* Point at first argument, if any */
	if (params->params)
		ptr = &params->params[0];
	else
		return CALLBACK_RETCODE_PARAMS_MISSING;

	/* Check parameters */
    for (int i = 0; i < params->length; i++, ptr++) {
        rdtn_performed = true;

		/* Check if parameter has value */
		if (!ptr->value) {
			retflag = CALLBACK_RETCODE_PARAM_ERROR;
			break;
        }

        if (strcmp(ptr->name, "tempunit") == 0) {
            /* Should be one character */
            if (strlen(ptr->value) == 1)
                ptr->value[0] = tolower(ptr->value[0]);
            else {
                retflag = CALLBACK_RETCODE_PARAM_ERROR;
                break;
            }

            /* Check */
            if (ptr->value[0] == RPIWD_TEMPERATURE_CELSIUS)
                msgbuff->unitstr[RPIWD_MEASURE_TEMPERATURE] =
                        RPIWD_TEMPERATURE_CELSIUS;
            else if (ptr->value[0] != RPIWD_TEMPERATURE_FARENHEIT) {
                /* Unrecognized unit */
                retflag = CALLBACK_RETCODE_PARAM_ERROR;
                break;
            }
        }
        else if (strcmp(ptr->name, "from") == 0 || strcmp(ptr->name, "to") == 0 ||
                 strcmp(ptr->name, "on") == 0) {
            /* Get date input and normalize it */
            temp = normalize_date(ptr->value, &rdtn_performed);
            if (temp == 0 || temp == -1) {
                retflag = CALLBACK_RETCODE_PARAM_ERROR;
                break;
            }

            /* Check parameter name again */
            if (strcmp(ptr->name, "from") == 0)
                from = rdtn_performed ? DAY_START(temp) : temp;
            else if (strcmp(ptr->name, "to") == 0)
                to = rdtn_performed ? DAY_END(temp) : temp;
            else if (strcmp(ptr->name, "on") == 0)
                on = temp;
        }
        else if (strcmp(ptr->name, "select") == 0) {
            select = strtol(ptr->value, NULL, 10);
            if (errno == ERANGE || select < 0)
                return CALLBACK_RETCODE_PARAM_ERROR;
        }
        else {
            retflag = CALLBACK_RETCODE_UNKNOWN_PARAM;
            break;
        }
	}

	/* If an error, return that error */
	if (retflag > 0)
		return retflag;

	/* Build SQL queries */
	msgbuff->mtype = DB_MSGTYPE_FETCH;

    /* Check date range parameters
     * TODO: This is pretty ugly. Replace this in the future. */
    if (from && !to && !on && !select) {
		/* Use "BY_DATE" queries */
		msgbuff->fcountq = format_query(SQLCMD_COUNT_BY_DATE, '>', difftime(from, 0));
		msgbuff->fselectq = format_query(SQLCMD_READ_BY_DATE, '>', difftime(from, 0));
	}
    else if (from && to && !select && !on) {
		/* Use "BY_DATE_RANGE" queries */
		msgbuff->fcountq = format_query(SQLCMD_COUNT_BY_DATE_RANGE, difftime(from, 0),
			   	difftime(to, 0));
		msgbuff->fselectq = format_query(SQLCMD_READ_BY_DATE_RANGE, difftime(from, 0),
			   	difftime(to, 0));
	}
    else if (!from && to && !select && !on) {
		/* Use "BY_DATE" queries */
		msgbuff->fcountq = format_query(SQLCMD_COUNT_BY_DATE, '<', difftime(to, 0));
		msgbuff->fselectq = format_query(SQLCMD_READ_BY_DATE, '<', difftime(to, 0));
	}
    else if (!from && !to && !select && on) {
		/* Use "BY_DATE_RANGE" queries */
		msgbuff->fcountq = format_query(SQLCMD_COUNT_BY_DATE_RANGE, 
				difftime(DAY_START(on), 0),
			   	difftime(DAY_END(on), 0));
		msgbuff->fselectq = format_query(SQLCMD_READ_BY_DATE_RANGE, 
				difftime(DAY_START(on), 0),
			   	difftime(DAY_END(on), 0));
	}
    else if (!from && !to && !on && select > 0) {
        msgbuff->fcountq = format_query(SQLCMD_COUNT_SELECT_N, select);
        msgbuff->fselectq = format_query(SQLCMD_SELECT_N, select);
    }
	else 
        return CALLBACK_RETCODE_PARAM_ERROR;

	return retflag;
}

/* TODO: This duplicates pretty much all of the code in the query loop
 * that's in rpiweatherd.c.
 * For future versions, combine these two into a single function in device.c */
int current_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff) {
	float temp[2];
    int qattempts = 0, qflag;
    bool keep_native_unit;

    /* Command should have one/no parameters. */
    if (params->length == 1) {
        http_cmd_param *param = &params->params[0];

        /* Should be one character, so lower it if needed. */
        if (strlen(param->value) == 1)
            param->value[0] = tolower(param->value[0]);
        else
            return CALLBACK_RETCODE_PARAM_ERROR;

        /* Only valid parameter is 'tempunit' */
        if (strcmp(param->name, "tempunit") == 0) {
            /* Check length and lower */
            switch (param->value[0]) {
            case RPIWD_TEMPERATURE_FARENHEIT:
                msgbuff->unitstr[RPIWD_MEASURE_TEMPERATURE] = RPIWD_TEMPERATURE_FARENHEIT;
                break;
            case RPIWD_TEMPERATURE_CELSIUS:
                msgbuff->unitstr[RPIWD_MEASURE_TEMPERATURE] = RPIWD_TEMPERATURE_CELSIUS;
                break;
            default:
                return CALLBACK_RETCODE_PARAM_ERROR;
            }
        }
        else
            return CALLBACK_RETCODE_UNKNOWN_PARAM;
    }
    else if (params->length > 1)
        return CALLBACK_RETCODE_TOO_MANY_PARAMS;

	/* Query data */
	do {
        qflag = device_query_current(temp);
        qattempts++;

        /* Check if conversion is needed */
        keep_native_unit = msgbuff->unitstr[RPIWD_MEASURE_TEMPERATURE] ==
                           RPIWD_TEMPERATURE_CELSIUS;
        if (qflag == RPIWD_DEVRETCODE_SUCCESS && !keep_native_unit)
            RPIWD_CELSIUS_TO_FARENHEIT(temp[0]);
	} while (qflag != RPIWD_DEVRETCODE_SUCCESS && qattempts < CONFIG_MAX_QUERY_ATTEMPTS);

	/* Check whether query has been successful */
	if (qflag != RPIWD_DEVRETCODE_SUCCESS)
		return CALLBACK_RETCODE_DEVICE_ERROR;

	/* Allocate an entry */
	msgbuff->data = entry_alloc();
	if (!(entry *)msgbuff->data)
		return CALLBACK_RETCODE_DEVICE_ERROR;

	entry *ent_ptr = (entry *)msgbuff->data;

	/* Put message type */
	msgbuff->mtype = DB_MSGTYPE_CURRENT;
	msgbuff->is_completed = 1;

	/* Put entry details */
	ent_ptr->id = -1;
	ent_ptr->record_date = NULL;
	ent_ptr->temperature = temp[0];
	ent_ptr->humidity = temp[1];
	ent_ptr->location = strdup(get_current_config()->measure_location);
	ent_ptr->device_name = strdup(get_current_config()->device_name);

	return CALLBACK_RETCODE_SUCCESS;
}

int statistics_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff) {
	size_t stats_count = exec_formatted_count_query(SQLCMD_COUNT_INITIAL_STATS);
	char buffer[STATS_COMMAND_BUFFER_LENGTH];
	struct sysinfo sinfo;
	struct statvfs statbuff;
	int ret = 0;
	key_value_list *lptr;

	/* Check parameter length */
	if (params->length > 0)
		return CALLBACK_RETCODE_NO_PARAMS_NEEDED;

	/* Create the list */
	msgbuff->data = key_value_list_alloc(stats_count + STATS_EXTRA_STATS_COUNT);
	lptr = (key_value_list *)msgbuff->data;

	/* Populate required data structures */
	sysinfo(&sinfo);
	ret = statvfs(CONFIG_FILE_DEFAULT_FOLDER, &statbuff);

	/* Hostname */
	gethostname(buffer, HOST_NAME_MAX);
	key_value_list_emplace(lptr, "hostname", buffer);

	/* Server version */
    sprintf(buffer, "%s", RPIWEATHERD_VERSION);
	key_value_list_emplace(lptr, "version", buffer);

	/* System uptime */
	sprintf(buffer, "%ld", sinfo.uptime);
	key_value_list_emplace(lptr, "uptime", buffer);

	/* Available RAM */
	sprintf(buffer, "%lu", sinfo.freeram);
	key_value_list_emplace(lptr, "freeram", buffer);

	/* Available disk space */
	sprintf(buffer, "%lu", statbuff.f_bsize * statbuff.f_bfree);
	key_value_list_emplace(lptr, "freedisk", buffer);

	/* The rest will be populated in the database thread, so prepare the queries... */
	msgbuff->mtype = DB_MSGTYPE_STATS;
	msgbuff->fselectq = strdup(SQLCMD_SELECT_STATS);
	msgbuff->fcountq = strdup(SQLCMD_COUNT_INITIAL_STATS);

	return CALLBACK_RETCODE_SUCCESS;
}

int config_command_callback(http_cmd *params, rpiwd_mqmsg *msgbuff) {
	rpiwd_config *config_ptr = get_current_config(); /* Read only, so no need to lock */
	char temp_buffer[JSON_SERIALIZER_TEMP_ID_BUFFER_SIZE];
	
	/* Initialize list */
	msgbuff->data = key_value_list_alloc(config_ptr->config_count);
	key_value_list *kvlist = (key_value_list *)msgbuff->data;
	if (!kvlist)
		return CALLBACK_RETCODE_MEMORY_ERROR;

	/* Get every configuration value and put it in the list */
	key_value_list_emplace(kvlist, CONFIG_LOCATION, config_ptr->measure_location);
	key_value_list_emplace(kvlist, CONFIG_QUERY_INTERVAL, config_ptr->query_interval);
	key_value_list_emplace(kvlist, CONFIG_DEVICE_NAME, config_ptr->device_name);

	sprintf(temp_buffer, "%d", config_ptr->device_config);
	key_value_list_emplace(kvlist, CONFIG_DEVICE_CONFIG, temp_buffer);

	sprintf(temp_buffer, "%d", config_ptr->comm_port);
	key_value_list_emplace(kvlist, CONFIG_COMM_PORT, temp_buffer);

	sprintf(temp_buffer, "%d", config_ptr->num_worker_threads);
	key_value_list_emplace(kvlist, CONFIG_NUM_WORKER_THREADS, temp_buffer);

	/* Mark message type + completed */
	msgbuff->mtype = DB_MSGTYPE_CONFIG;
	msgbuff->is_completed = 1;

	/* Return */
	return CALLBACK_RETCODE_SUCCESS;
}
