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

#include "dbhandler.h"

static sqlite3 *db;
static pthread_t __db_thread_pid;

int init_dbhandler(void) {
	int result = 0;
    struct mq_attr attr;
    const char **sqlcmd = SQLCMD_TABLE_CREATION_QUERIES;

	/* Open the database, or create it. */
	result = sqlite3_open(DB_DEFAULT_FILE_PATH, &db);
	if (result != SQLITE_OK) {
		syslog(LOG_ERR, "error: Can't open SQLite database: %s",
				sqlite3_errmsg(db));
		return -1;
	}

    /* Create all data tables */
    while (*sqlcmd) {
        result += sqlite3_exec(db, *sqlcmd, NULL, NULL, NULL);
        sqlcmd++;
    }

	/* Write initial values to that table, only if it does not exist! */
	result += sqlite3_exec(db, SQLCMD_INSERT_INITIAL_STATS, NULL, NULL, NULL);

	/* Check if all operations are sucessful */
	if (result != SQLITE_OK) {
		syslog(LOG_ERR, "error when creating/opening database file: %s", 
				sqlite3_errmsg(db));

		sqlite3_close(db);
		return -1;
	}

	/* Initialize message queue */
	attr.mq_flags = attr.mq_curmsgs = 0;
	attr.mq_maxmsg = MQ_MAXMESSAGES;
	attr.mq_msgsize = MQ_MAXMSGSIZE;
	__db_mqd = mq_open(RPIWD_DB_MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
	if (__db_mqd == (mqd_t) -1) {
		syslog(LOG_ERR, "error: mq_open: %s", strerror(errno));
		return -1;
	}

	/* Initialize DB thread */
	result = pthread_create(&__db_thread_pid, NULL, db_thread_event_loop, NULL);
	if (result != 0)
        syslog(LOG_ERR, "error: pthread_create: %s", strerror(errno));

	/* DONE! */
	return 1;
}

void quit_dbhandler(void) {
	/* Signal thread to stop and wait for it to */
	pthread_cancel(__db_thread_pid);
	pthread_join(__db_thread_pid, NULL);
}

void quit_db_mq(void) {
	/* Close message queue */
	mq_close(__db_mqd);
	mq_unlink(RPIWD_DB_MQ_NAME);
}

/* DB Thread event loop */
void *db_thread_event_loop(void *unused) {
	int old;
	ssize_t res;
	rpiwd_mqmsg msg_buffer;

	/* Initialize thread cancellation and cleanup */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old);
	pthread_cleanup_push(db_thread_cleanup_routine, NULL);

	/* Recieve messages (NOTE: Cancellation point for thread here) */
	while ((res = mq_receive(__db_mqd, (char *)&msg_buffer, MQ_MAXMSGSIZE, NULL)) != -1) {
		/* Get message type. This is the requested command. */
		if (msg_buffer.mtype == DB_MSGTYPE_WRITEENTRY) {
			entry *ent = (entry *)msg_buffer.data;

			write_raw_entry(ent->temperature, 
					ent->humidity, 
					ent->location, 
					ent->device_name);

			entry_ptr_free(ent);

			/* Update statistics */
			increase_stat(STAT_NAME_TOTAL_ENTRIES);

			/* No need to send anything anywhere so continue here... */
			continue;
		}
		else if (msg_buffer.mtype == DB_MSGTYPE_FETCH) {
			/* Execute query */
			msg_buffer.data = exec_fetch_query(msg_buffer.fcountq, 
                    msg_buffer.fselectq, msg_buffer.is_conversion_needed,
                    &msg_buffer.retcode);

			/* Update statistics */
			increase_stat(STAT_NAME_TOTAL_REQUESTS);
		}
		else if (msg_buffer.mtype == DB_MSGTYPE_STATS) {
			/* Execute query */
			key_value_list *listptr = exec_key_value_query(msg_buffer.fcountq,
					msg_buffer.fselectq, &msg_buffer.retcode);

			/* Add items to existing list in msg_buffer.data and free this list */
			for (int i = 0; i < listptr->length; i++)
				key_value_list_emplace((key_value_list *)msg_buffer.data,
						listptr->pairs[i].key, listptr->pairs[i].value);

			key_value_list_free(listptr);

			/* Update statistics */
			increase_stat(STAT_NAME_TOTAL_REQUESTS);
		}

		/* Mark as complete and send back to reciever message queue */
		msg_buffer.is_completed = 1;
		mq_send(msg_buffer.receiver_mq, (const char *)&msg_buffer, 
				sizeof(rpiwd_mqmsg), 0);
	}

	/* Cleanup */
	pthread_cleanup_pop(0);
}

void db_thread_cleanup_routine(void *arg) {
	/* Close and unlink MQ */
	quit_db_mq();

	/* Close DB connection */
	sqlite3_close(db);
}

void request_write_entry(float temp, float humid, const char *location,
		const char *device) {
	/* Build message buffer */
	rpiwd_mqmsg msgbuff;

	msgbuff.mtype = DB_MSGTYPE_WRITEENTRY;
	msgbuff.sockfd = DB_MSG_NO_SOCKFD;
	msgbuff.data = entry_alloc();
	((entry *)msgbuff.data)->temperature = temp;
	((entry *)msgbuff.data)->humidity = humid;
	((entry *)msgbuff.data)->location = strdup(location);
	((entry *)msgbuff.data)->device_name = strdup(device);

	/* Send message */
	mq_send(__db_mqd, (const char *)&msgbuff, sizeof(rpiwd_mqmsg), 0);
}

static int write_raw_entry(float temp, float humid, const char *location, const char *device) {
	sqlite3_stmt *query;
	int rc;

	/* Prepare query */
    rc = sqlite3_prepare_v2(db, SQLCMD_WRITE_ENTRY, -1, &query, 0);
	if (rc == SQLITE_OK) {
		/* Bind parameters */
		sqlite3_bind_double(query, 1, temp);
		sqlite3_bind_double(query, 2, humid);
		sqlite3_bind_text(query, 3, location, -1, SQLITE_STATIC);
		sqlite3_bind_text(query, 4, device, -1, SQLITE_STATIC);
	}
	else {
		/* Log error */
		syslog(LOG_ERR, "Error with preperation of statement: %s", sqlite3_errmsg(db));
	}

	/* Get result */
	rc = sqlite3_step(query);
	if (rc != SQLITE_DONE) {
		syslog(LOG_ERR, "Error executing statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(query);
		return -1;
	}

	sqlite3_finalize(query);
	return 1;
}

static void increase_stat(const char *stat_name) {
	/* Get formatted query */
	char *fquery = format_query(SQLCMD_INCREASE_STAT, stat_name);

	/* Execute query */
	sqlite3_exec(db, fquery, NULL, NULL, NULL);

	/* Free query */
	free(fquery);
}

char *format_query(const char *format, ...) {
	va_list args;

	char *qString = calloc(SQL_COMMAND_BUFFER_SIZE, sizeof(char));
	if (qString) {
		va_start(args, format);
		vsnprintf(qString, SQL_COMMAND_BUFFER_SIZE * sizeof(char), format, args);
		va_end(args);

		return qString;
	}
	else {
		va_end(args);
		return NULL;
	}
}

size_t exec_formatted_count_query(const char *count_query) {
	sqlite3_stmt *query;
	size_t count = 0;
	int rc;

	/* Prepare count query */
	rc = sqlite3_prepare_v2(db, count_query, -1, &query, 0);
	if (rc == SQLITE_OK) {
		/* Step */
		rc = sqlite3_step(query);
		if (rc == SQLITE_ROW) {
			/* Get the result */
			count = (size_t)sqlite3_column_int(query, 0);
		}
		else {
			count = 0;
			syslog(LOG_ERR, "Error executing formatted count query: %s",
					sqlite3_errmsg(db));
		}
	}
	else {
		count = 0;
		syslog(LOG_ERR, "Error executing formatted count query: %s",
				sqlite3_errmsg(db));
	}

	sqlite3_finalize(query);

	return count;
}

entrylist *exec_fetch_query(const char *fcountq, const char *fselectq, bool convert,
                            int *errcode) {
	size_t count = exec_formatted_count_query(fcountq);
	entrylist *list = entrylist_alloc(count);
	sqlite3_stmt *query;
	int rc, i = 0;

	/* Check list allocation */
	if (!list)
		return NULL;

	/* Check list count */
	if (count > DBHANDLER_MAX_FETCHED_ENTRIES) {
		*errcode = DBHANDLER_ERROR_TOO_MANY_ENTRIES;
		return NULL;
	}

	/* Prepare query */
	rc = sqlite3_prepare_v2(db, fselectq, -1, &query, 0);
	if (rc == SQLITE_OK) {
		/* Step while there is anything */
		while ((rc = sqlite3_step(query)) == SQLITE_ROW) {
			/* Initialize entry */
			list->entries[i].id = sqlite3_column_int(query, 0);
			list->entries[i].record_date = strdup(sqlite3_column_text(query, 1));
			list->entries[i].humidity = sqlite3_column_double(query, 3);
			list->entries[i].location = strdup(sqlite3_column_text(query, 4));
			list->entries[i].device_name = strdup(sqlite3_column_text(query, 5));

            /* Fetch temperature and then check if a conversion is required. */
            list->entries[i].temperature = sqlite3_column_double(query, 2);
            if (convert)
                list->entries[i].temperature =
                        list->entries[i].temperature * 9 / 5 + 32;

			/* Increment i */
			i++;
		}
	}
	else {
		/* Log error */
		syslog(LOG_ERR, "Error retrieving entries by date: %s", sqlite3_errmsg(db));
		*errcode = DBHANDLER_ERROR_SQL_ERROR;
	}

	sqlite3_finalize(query);

	*errcode = DBHANDLER_ERROR_SUCCESS;
	return list;
}

key_value_list *exec_key_value_query(const char *fcountq, const char *fselectq, 
		int *errcode) {
	size_t count = exec_formatted_count_query(fcountq);
	key_value_list *kvlist = key_value_list_alloc(count);
	sqlite3_stmt *query;
	int rc, addflag = 0;

	/* Check list and list count */
	if (!kvlist)
		return NULL;

	if (count > DBHANDLER_MAX_FETCHED_ENTRIES) {
		*errcode = DBHANDLER_ERROR_TOO_MANY_ENTRIES;
		return NULL;
	}

	/* Prepare query */
	rc = sqlite3_prepare_v2(db, fselectq, -1, &query, 0);
	if (rc == SQLITE_OK) {
		/* Step while there is anything there */
		while ((rc = sqlite3_step(query)) == SQLITE_ROW) {
			addflag = key_value_list_emplace(kvlist,
					sqlite3_column_text(query, 0),
					sqlite3_column_text(query, 1));

			if (!addflag) {
				*errcode = DBHANDLER_ERROR_NO_MEMORY;
				break;
			}
		}
	}

	/* Finish */
	sqlite3_finalize(query);

	/* Check return value */
	if (errcode < 0) {
		key_value_list_free(kvlist);
		return NULL;
	}

	return kvlist;
}

const char *dbhandler_strerror(int errcode) {
	switch (errcode) {
		case DBHANDLER_ERROR_SUCCESS:
			return "Success";
		case DBHANDLER_ERROR_TOO_MANY_ENTRIES:
			return "Too many entires to fetch. See documentation for more information.";
		case DBHANDLER_ERROR_SQL_ERROR:
			return "Internal SQL error";
		case DBHANDLER_ERROR_NO_MEMORY:
			return "Out of memory.";
	}

	return "General DBHANDLER error.";
}

