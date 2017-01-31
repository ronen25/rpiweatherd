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

#ifndef RPIWD_TRIGGER_H
#define RPIWD_TRIGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <float.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wiringPi.h>

#include "confighandler.h"
#include "devicevals.h"

/* Constants */
#define TRIGGER_FILE_PATH                   "/etc/rpiweatherd/rpiwd_triggers.conf"
#define TRIGGER_ARG_STRING_MAX_LENGTH       2049    /* 2048 + '\0' */

/* Error codes */
#define TRIGGER_PARSE_OK                    0
#define TRIGGER_PARSE_COMMENT_LINE          1
#define TRIGGER_ERROR_FILE_NOT_FOUND        2
#define TRIGGER_ERROR_FILE_ERROR            3
#define TRIGGER_ERROR_UP_TO_DATE            4
#define TRIGGER_ERROR_EOF                   5
#define TRIGGER_ERROR_VALUE_SYNTAX          6
#define TRIGGER_ERROR_VALUE_TYPE_ERROR      7
#define TRIGGER_ERROR_COND_OP_SYNTAX        8
#define TRIGGER_ERROR_COND_UNKNOWN_OP       9
#define TRIGGER_ERROR_MALFORMED_VALUE       10
#define TRIGGER_ERROR_MISSING_VALUE         11
#define TRIGGER_ERROR_UNKNOWN_ACTION        12
#define TRIGGER_ERROR_MISSING_ACTION        13
#define TRIGGER_ERROR_TARGET_MALFORMED      14
#define TRIGGER_ERROR_TARGET_NOT_NUMERIC    15
#define TRIGGER_ERROR_MISSING_ARGS          16
#define TRIGGER_ERROR_ARGUMENTS_NOT_NEEDED  17
#define TRIGGER_ERROR_MISSING_TARGET        18
#define TRIGGER_ERROR_OUT_OF_MEMORY         19

/* Trigger value type */
#define TRIGGER_TYPE_TEMP                   1
#define TRIGGER_TYPE_HUMID                  2

/* Trigger Condition Operation */
#define TRIGGER_COND_EQ                     1
#define TRIGGER_COND_LEQ                    2
#define TRIGGER_COND_BEQ                    3
#define TRIGGER_COND_LESS                   4
#define TRIGGER_COND_MORE                   5
#define TRIGGER_COND_NEQ                    6

/* Trigger Action Type */
#define TRIGGER_ACTION_EXEC                 100
#define TRIGGER_ACTION_PINUP                101
#define TRIGGER_ACTION_PINDOWN              102

/* Trigger Execution Error Codes */
#define TRIGGER_EXEC_OK                     1

#define TRY_PARSE(flag, func, part, value_type, value_ptr)  \
    do {                                                    \
    (flag) = func((part), ((value_type *)((value_ptr))));   \
    if ((flag) != TRIGGER_PARSE_OK) {                       \
        free((line));	                                    \
        return (flag);                                      \
    }                                                       \
    } while(0);

/* Trigger structure definition */
typedef struct rpiwd_trigger_s {
    int val_type, op, action;
    float cond_value;
    char *target, *args;
} rpiwd_trigger;

/* Structures for tables */
struct value_type_mapping_s {
    const char *part_str;
    int value;
};

struct cond_op_to_str_s {
    int cond_op;
    const char *str;
};

struct cond_type_to_str_s {
    int cond_type;
    const char *str;
};

struct cond_action_to_str_s {
    int cond_action;
    const char *str;
};

struct trigger_parse_error_str_s {
    int errcode;
    const char *msg;
};

/* Trigger parsing and loading */
int parse_cond_action(char *part, int *value);
int parse_cond_value(char *part, float *value);
int parse_cond_op(const char *part, int *value);
int parse_cond_type(const char *part, int *value);

int load_triggers(void);
void unload_triggers(void);
int parse_trigger(rpiwd_trigger *trig, FILE **f);
int post_process_args(const char *arg_string, char *buffer, float **measurements);

/* Trigger execution */
int trigger_exec_callback(float *measurements);
int trigger_should_execute(rpiwd_trigger *trig, float **measurements);

/* To friendly name */
const char *cond_op_to_str(int op);
const char *cond_action_to_str(int action);
const char *cond_type_to_str(int type);

/* Helpers */
void list_triggers(void);
const size_t max_allowed_argstring_length(void);

#endif /* RPIWD_TRIGGER_H */
