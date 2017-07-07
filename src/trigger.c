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

#include "trigger.h"

/* Static global variables */
static rpiwd_trigger __rpiwd_trigger_table[CONFIG_MAX_TRIGGERS];
static size_t __rpiwd_trigger_count = 0;
static time_t __rpiwd_trigger_last_update;

static const struct value_type_mapping_s VALUE_TYPE_MAPPING[] = {
    { "%temp%",         RPIWD_MEASURE_TEMPERATURE },
    { "%humid%",        RPIWD_MEASURE_HUMIDITY },
    { NULL,             -1 }
};

static const struct cond_op_to_str_s COND_OP_TO_STR[] = {
    { TRIGGER_COND_EQ,      "==" },
    { TRIGGER_COND_LEQ,     "<=" },
    { TRIGGER_COND_BEQ,     ">=" },
    { TRIGGER_COND_LESS,    "<" },
    { TRIGGER_COND_MORE,    ">" },
    { TRIGGER_COND_NEQ,     "!=" },
    { 0, NULL }
};

static const struct cond_type_to_str_s COND_TYPE_TO_STR[] = {
    { TRIGGER_TYPE_HUMID,   "%humid%" },
    { TRIGGER_TYPE_TEMP,    "%temp%" },
    { 0, NULL }
};

static const struct cond_action_to_str_s COND_ACTION_TO_STR[] = {
    { TRIGGER_ACTION_EXEC,      "exec" },
    { TRIGGER_ACTION_PINDOWN,   "pindown" },
    { TRIGGER_ACTION_PINUP,     "pinup" },
    { 0, NULL }
};

static const struct trigger_parse_error_str_s TRIGGER_PARSE_ERROR_STR[] = {
{ TRIGGER_PARSE_OK,                     "Parsing is OK" },
{ TRIGGER_PARSE_COMMENT_LINE,           "Parser encountered a commented line." },
{ TRIGGER_ERROR_FILE_NOT_FOUND,         "Trigger file could not be found." },
{ TRIGGER_ERROR_FILE_ERROR,             "I/O error in trigger file." },
{ TRIGGER_ERROR_UP_TO_DATE,             "Trigger file is up to date, no need to "
                                        "reload it." },
{ TRIGGER_ERROR_EOF,                    "EOF" },
{ TRIGGER_ERROR_VALUE_SYNTAX,           "Syntax error in the value "
                                        "compared by the condition." },
{ TRIGGER_ERROR_VALUE_TYPE_ERROR,       "Condition value has an unrecognized type." },
{ TRIGGER_ERROR_COND_OP_SYNTAX,         "Syntax error in condition operation." },
{ TRIGGER_ERROR_COND_UNKNOWN_OP,        "Unrecognized condition operation type." },
{ TRIGGER_ERROR_MALFORMED_VALUE,        "Malformed value." },
{ TRIGGER_ERROR_MISSING_VALUE,          "Value is missing." },
{ TRIGGER_ERROR_UNKNOWN_ACTION,         "Unknown action." },
{ TRIGGER_ERROR_MISSING_ACTION,         "Missing action." },
{ TRIGGER_ERROR_TARGET_MALFORMED,       "Trigger target is malformed." },
{ TRIGGER_ERROR_TARGET_NOT_NUMERIC,     "Trigger target is not numeric, but action is "
                                        "not PINUP or PINDOWN." },
{ TRIGGER_ERROR_MISSING_ARGS,           "Arguments are required, but are missing." },
{ TRIGGER_ERROR_ARGUMENTS_NOT_NEEDED ,  "Arguments are not needed, but provided." },
{ TRIGGER_ERROR_MISSING_TARGET,         "Target is required, but is missing." },
{ TRIGGER_ERROR_OUT_OF_MEMORY,          "rpiweathed could not allocate memory!" },
{ 0, NULL }
};

static const char TRIGGER_VALUE_VALID_MEASUREMENTS[] = {
    RPIWD_TEMPERATURE_CELSIUS,
    RPIWD_TEMPERATURE_FARENHEIT,
    RPIWD_DEFAULT_HUMID_UNIT,
    0
};

static const char TRIGGER_VALUE_VALID_TEMPS[] = {
    RPIWD_TEMPERATURE_CELSIUS,
    RPIWD_TEMPERATURE_FARENHEIT,
    0
};

/* =================================================================================== */

int parse_cond_type(char **part, int *value) {
    const struct cond_type_to_str_s *ptr = COND_TYPE_TO_STR;
    *value = 0;

    if (*part && *part[0] == '%' && (*part)[strlen(*part) - 1] == '%') {
        while (ptr->str) {
            if (strcmp(ptr->str, *part) == 0) {
                *value = ptr->cond_type;
                break;
            }

            ptr++;
        }
    }

    return *value != -1 ? TRIGGER_PARSE_OK : TRIGGER_ERROR_VALUE_SYNTAX;
}

int parse_cond_op(char **part, int *value) {
    const struct cond_op_to_str_s *ptr = COND_OP_TO_STR;
    *value = 0;

    if (*part && strlen(*part) <= 2) {
        while (ptr->cond_op) {
            if (strcmp(*part, ptr->str) == 0) {
                *value = ptr->cond_op;
                break;
            }

            ptr++;
        }

        if (!(*value))
            return TRIGGER_ERROR_COND_UNKNOWN_OP;
    }

    return *value ? TRIGGER_PARSE_OK : TRIGGER_ERROR_COND_OP_SYNTAX;
}

int parse_cond_value(char **part, float *value) {
    char *endptr;

    if (*part) {
        if ((*value = strtod(*part, &endptr)) != 0 || errno != ERANGE) {
            /* Check if there is a suffix.
             * If there is, advance the part so that parse_cond_value_suffix could
             * parse that suffix.
             * If not, leave part as-is and return. */
            if (*endptr != '\0')
                *part = endptr;

            /* Return */
            return TRIGGER_PARSE_OK;
        }
        else
            return TRIGGER_ERROR_MALFORMED_VALUE;
    }
    else
        return TRIGGER_ERROR_MISSING_VALUE;
}

int parse_cond_value_suffix(char **part, char *value) {
    const char *tableptr = TRIGGER_VALUE_VALID_MEASUREMENTS;

    while (*tableptr) {
        if (*tableptr == *part[0]) {
            *value = *tableptr;
            return TRIGGER_PARSE_OK;
        }

        tableptr++;
    }

    return TRIGGER_PARSE_OK;
}

int parse_cond_action(char **part, int *value) {
    int i = 0;
    char *ptr = *part;
    const struct cond_action_to_str_s *action_ptr = COND_ACTION_TO_STR;

    *value = 0;

    if (*part) {
        /* Make everything lowercase */
        while (*ptr) {
            if (isupper(*ptr))
                *ptr = tolower(*ptr);

            ptr++;
        }

        /* Check action */
        while (action_ptr->cond_action) {
            if (strcmp(*part, action_ptr->str) == 0) {
                *value = action_ptr->cond_action;
                break;
            }

            action_ptr++;
        }

        if (!(*value))
            return TRIGGER_ERROR_UNKNOWN_ACTION;
    }

    return *value ? TRIGGER_PARSE_OK : TRIGGER_ERROR_MISSING_ACTION;
}

int parse_trigger(rpiwd_trigger *trig, FILE **f) {
    char *line = NULL, *part, *saveptr;
    size_t len = 0;
    ssize_t read_chars;
    int flag;

    read_chars = getline(&line, &len, *f);
    if (read_chars == -1) {
        free(line);
        return TRIGGER_ERROR_EOF;
    }

    /* Parse value type */
    part = strtok_r(line, " \t", &saveptr);

    /* First check if it's a comment line */
    if (part[0] == '#') {
        free(line);
        return TRIGGER_PARSE_COMMENT_LINE;
    }
    else
        TRY_PARSE(flag, parse_cond_type, &part, int, &trig->val_type);

    /* Op */
    part = strtok_r(NULL, " \t", &saveptr);
    TRY_PARSE(flag, parse_cond_op, &part, int, &trig->op);

    /* Actual value */
    part = strtok_r(NULL, " \t", &saveptr);
    TRY_PARSE(flag, parse_cond_value, &part, float, &trig->cond_value);

    /* Parse value suffix, if there is any */
    if (part != '\0')
        TRY_PARSE(flag, parse_cond_value_suffix, &part, char, &trig->value_unit);

    /* Action */
    part = strtok_r(NULL, " \t", &saveptr);
    TRY_PARSE(flag, parse_cond_action, &part, int, &trig->action);

    /* Path */
    part = strtok_r(NULL, " \t", &saveptr);
    if (!part || strlen(part) < 1)
        return TRIGGER_ERROR_TARGET_MALFORMED;
    if (!part && (trig->action == TRIGGER_ACTION_PINDOWN ||
                  trig->action == TRIGGER_ACTION_PINUP))
        return TRIGGER_ERROR_MISSING_TARGET;

    trig->target = strdup(part);
    if (!trig->target)
        return TRIGGER_ERROR_OUT_OF_MEMORY;

    if ((trig->action == TRIGGER_ACTION_PINDOWN || trig->action == TRIGGER_ACTION_PINUP)
        && !rpiwd_is_number(trig->target))
        return TRIGGER_ERROR_TARGET_NOT_NUMERIC;

    /* Args */
    part = strtok_r(NULL, " \t\n", &saveptr);
    if (part && (trig->action == TRIGGER_ACTION_PINDOWN ||
                 trig->action == TRIGGER_ACTION_PINUP))
        return TRIGGER_ERROR_ARGUMENTS_NOT_NEEDED;

    if (part) {
        trig->args = strdup(part);
        if (!trig->args)
            return TRIGGER_ERROR_OUT_OF_MEMORY;
    }

    /* Free and return */
    free(line);
    return TRIGGER_PARSE_OK;
}

int load_triggers(void) {
    FILE *trigfile = NULL;
    int flag = 1;

    /* Unload any existing triggers */
    if (__rpiwd_trigger_count > 0)
        unload_triggers();

    trigfile = fopen(TRIGGER_FILE_PATH, "rt");
    if (!trigfile)
        return TRIGGER_ERROR_FILE_NOT_FOUND;

    /* Parse all triggers from file */
    while ((flag = parse_trigger(&(__rpiwd_trigger_table[__rpiwd_trigger_count]),
                                 &trigfile)) != TRIGGER_ERROR_EOF) {
        if (flag == TRIGGER_PARSE_OK)
            __rpiwd_trigger_count++;
        else if (flag == TRIGGER_PARSE_COMMENT_LINE)
            continue;
        else {
            rpiwd_log(LOG_ERR, "Parsing error in trigger no.%d: %s\n",
                      (__rpiwd_trigger_count + 1), TRIGGER_PARSE_ERROR_STR[flag].msg);
            break;
        }
    }

    /* Close file */
    fclose(trigfile);

    if (flag == TRIGGER_ERROR_EOF)
        flag = TRIGGER_PARSE_OK;

    return flag;
}

void unload_triggers(void) {
    /* Free targets and paths */
    for (size_t i = 0; i < __rpiwd_trigger_count; i++) {
        rpiwd_trigger *trig = &__rpiwd_trigger_table[i];

        if (trig->args)
            free(trig->args);

        if (trig->target)
            free(trig->target);
    }

    /* Reset count */
    __rpiwd_trigger_count = 0;
}

void list_triggers(void) {
    /* Load trigger file */
    int load_flag = load_triggers();
    if (load_flag != TRIGGER_PARSE_OK) {
        fprintf(stderr, "Some triggers failed parsing!");
        return;
    }

    /* Print triggers */
    printf("%-20s%-10s%-30s%-20s\n", "CONDITION", "ACTION", "TARGET", "ARGS");
    for (size_t i = 0; i < __rpiwd_trigger_count; i++) {
        rpiwd_trigger *trig = &__rpiwd_trigger_table[i];

        printf("%s %s %-10.2f%-10s%-30s%-20s\n", cond_type_to_str(trig->val_type),
                                                 cond_op_to_str(trig->op),
                                                 trig->cond_value,
                                                 cond_action_to_str(trig->action),
                                                 trig->target,
                                                 trig->args ? trig->args : "");
    }

    putc('\n', stdout);
}

int post_process_args(const char *arg_string, char *buffer, float **measurements) {
    const size_t max_allowed_len = max_allowed_argstring_length();
    const struct value_type_mapping_s *mapping_ptr = VALUE_TYPE_MAPPING;
    const char *part_ptr, *begin = arg_string;
    size_t appended = 0, copy_len, part_len;
    float converted_value;

    /* If there are no percent signs, no point in post-processing */
    if (!strchr(arg_string, '%'))
        return 0;

    /* Check if argument string is bigger then TRIGGER_ARG_STRING_MAX_LENGTH.
     * If it is, do not do post-processing. */
    if (strlen(arg_string) >= max_allowed_len) {
        rpiwd_log(LOG_ERR, "Argument string is too big, maximum allowed is %ld. "
                           "Post-processing would not be done - argument string "
                           "passed as-is.", max_allowed_len);
        return 0;
    }

    /* For every placeholder that's in the string:
     * 1. Find it's location.
     * 2. Setup pointers.
     * 3. Copy everything before it.
     * 4. Copy measurement as string.
     * 5. Advance pointers.
     * 6. After loop is done, copy the rest, if there is anything.
     */
    while (mapping_ptr->part_str) {
        if ((part_ptr = strstr(arg_string, mapping_ptr->part_str))) {
            copy_len = (size_t)(begin - part_ptr);
            part_len = strlen(mapping_ptr->part_str);

            /* Copy everything before the placeholder */
            if (copy_len) {
                appended += copy_len;
                strncpy(buffer, begin, copy_len);
            }

            /* Copy measurement */
            appended += sprintf(buffer + appended, "%.4f",
                                (* measurements[mapping_ptr->value]));

            /* Prepare pointers for next iteration */
            begin += copy_len + part_len;
        }

        /* Next mapping */
        mapping_ptr++;
    }

    /* Finally, copy everything that is after the last begin pointer,
     * if that even exists. */
    if (strlen(buffer) + 1 != appended)
        strncpy(buffer + appended, begin, strlen(begin) + 1);

    if (strlen(buffer) < TRIGGER_ARG_STRING_MAX_LENGTH)
        buffer[strlen(buffer) + 1] = '\0';
    else
        buffer[strlen(buffer)] = '\0';

    return 1;
}

int trigger_should_execute(rpiwd_trigger *trig, float **measurements) {
    int cond_eval = 0;

    /* Convert measurements to properly evaluate the condition */
    convert_measurements_maybe(trig, measurements);

    /* Evaluate the condition */
    switch (trig->op) {
    case TRIGGER_COND_EQ:
        cond_eval = ((* measurements)[trig->val_type] == trig->cond_value);
        break;
    case TRIGGER_COND_BEQ:
        cond_eval = ((* measurements)[trig->val_type] >= trig->cond_value);
        break;
    case TRIGGER_COND_LEQ:
        cond_eval = ((* measurements)[trig->val_type] <= trig->cond_value);
        break;
    case TRIGGER_COND_LESS:
        cond_eval = ((* measurements)[trig->val_type] < trig->cond_value);
        break;
    case TRIGGER_COND_MORE:
        cond_eval = ((* measurements)[trig->val_type] > trig->cond_value);
        break;
    case TRIGGER_COND_NEQ:
        cond_eval = ((* measurements)[trig->val_type] != trig->cond_value);
        break;
    default: /* Should not be here */
        cond_eval = -1;
        break;
    }

    return cond_eval;
}

int trigger_exec_callback(float *measurements) {
    char arg_string[TRIGGER_ARG_STRING_MAX_LENGTH];
    int retval = 0, arg_retval, fork_flag, eval_flag;

    /* Check if an update is needed */
    struct stat stat_info;
    if (stat(TRIGGER_FILE_PATH, &stat_info) == 0) {
        if (stat_info.st_ctim.tv_sec > __rpiwd_trigger_last_update) {
            __rpiwd_trigger_last_update = stat_info.st_ctim.tv_sec;
            load_triggers();
        }
    }
    else {
        rpiwd_log(LOG_ERR, "Error: Error loading triggers; errno %d.", errno);
        return TRIGGER_ERROR_FILE_ERROR;
    }

    /* Execute all stored triggers */
    for (size_t i = 0; i < __rpiwd_trigger_count; i++) {
        rpiwd_trigger *trig = &__rpiwd_trigger_table[i];

        /* Check trigger condition is true, and therefore needs execution. */
        eval_flag = trigger_should_execute(trig, &measurements);
        if (eval_flag == -1)
            rpiwd_log(LOG_ERR, "Error evaluating trigger; possibly one or more errors.");
        else if (eval_flag == 0)
            continue;

        /* Check trigger action and execute accordingly */
        switch (trig->action) {
        case TRIGGER_ACTION_PINUP:
            pinMode(atoi(trig->target), OUTPUT);
            digitalWrite(atoi(trig->target), 1);

            retval = TRIGGER_EXEC_OK;
            break;
        case TRIGGER_ACTION_PINDOWN:
            pinMode(atoi(trig->target), OUTPUT);
            digitalWrite(atoi(trig->target), 0);

            retval = TRIGGER_EXEC_OK;
            break;
        case TRIGGER_ACTION_EXEC:
            /* Process argument string */
            arg_retval = post_process_args(trig->args, arg_string, &measurements);

            /* Fork a child that'll execute the command */
            fork_flag = fork();

            /* Execute trigger target and log result */
            if (fork_flag < 0)
                rpiwd_log(LOG_ERR, "Failed to execute fork(): %s", strerror(errno));
            else if (fork_flag == 0) {
                /* setuid() to the restricted "rpiweatherd" system user */
                if (!seteuid_rpiwd_user()) {
                    rpiwd_log(LOG_ERR, "Failed to execute trigger: failed to seteuid()"
                                       " to system user.");
                    _exit(EXIT_FAILURE);
                }

                /* Set soft and hard execution timeouts for this child worker */
                if (!trigger_child_set_timeouts()) {
                    rpiwd_log(LOG_ERR, "Error: Failed to set timeout for child worker; "
                            "errno %d", errno);
                    _exit(EXIT_FAILURE);
                }

                /* Execute process */
                retval = execl(trig->target,
                               trig->target,
                               arg_retval ? arg_string : trig->args,
                               (char *)NULL);


                /* Check if there was an error in execution */
                if (retval == -1)
                    rpiwd_log(LOG_ERR, "Failed to execute trigger: %s", strerror(errno));
                else
                    rpiwd_log(LOG_ERR, "execl returned %d.", retval);

                /* Exit child */
                _exit(EXIT_SUCCESS);
            }
            else {
                rpiwd_log(LOG_INFO, "Child %d now executing triggers.", (int)fork_flag);
                wait(NULL);
            }

            break;
        }

        /* Log execution */
        if (retval == -1)
            rpiwd_log(LOG_ERR, "Trigger execution failed: %s", strerror(errno));
    }
}

int seteuid_rpiwd_user(void) {
    uid_t userId = 0;
    int resFlag, retCode = 0;

    /* Get the UID for the "rpiweatherd" user */
    resFlag = username_to_uid("rpiweatherd", &userId);
    if (resFlag != 0) {
        switch (resFlag) {
            case ENOENT:
            case ESRCH:
            case EBADF:
            case EPERM:
                rpiwd_log(LOG_ERR, "Error: Could not find \'rpiweatherd\' system user!");
                break;
            case EINTR:
                rpiwd_log(LOG_ERR, "Error: Signal was caught while trying seteuid() "
                        "to 'rpiweatherd'!");
                break;
            case EIO:
                rpiwd_log(LOG_ERR, "Error: I/O error!");
                break;
            case EMFILE:
            case ENFILE:
            case ENOMEM:
            case ERANGE:
                rpiwd_log(LOG_ERR, "Error: System memory/file descriptor limit "
                                   "reached!");
                break;
            default:
                rpiwd_log(LOG_ERR, "Error: Unknown error while seteuid() on "
                                   "'rpiweatherd': errno %d.", errno);
                break;
        }
    }
    else {
        /* Use seteuid() to set the effective user ID for the process */
        resFlag = seteuid(userId);
        if (resFlag != 0) {
            switch (errno) {
                case EINVAL:
                    rpiwd_log(LOG_ERR, "Error: Could not seteuid() for 'rpiweatherd' "
                                       "user:\n it is invalid!");
                    break;
                case EPERM:
                    rpiwd_log(LOG_ERR, "Error: Process is unprivileged for seteuid()!");
                    break;
            }
        }

        /* Success! Whew! */
        retCode = 1;
    }

    return retCode;
}

int trigger_child_set_timeouts(void) {
    int result;
    const struct rlimit limits = {
            .rlim_cur = TRIGGER_CHILD_SOFT_TIMEOUT_SEC,
            .rlim_max = TRIGGER_CHILD_HARD_TIMEOUT_SEC
    };

    result = setrlimit(RLIMIT_CPU, &limits);
    return result;
}

void convert_measurements_maybe(const rpiwd_trigger *trig, float **measurements) {
    /* Check if we need to convert the temperature (i.e. if there is a temperature
     * suffix and it's not c or C. */
    if (trig->val_type == TRIGGER_TYPE_TEMP &&
        trig->value_unit != RPIWD_TEMPERATURE_CELSIUS && trig->value_unit != '\0') {
        (* measurements)[RPIWD_MEASURE_TEMPERATURE] =
            (* measurements)[RPIWD_MEASURE_TEMPERATURE] * 1.8f + 32;
    }
}

const size_t max_allowed_argstring_length(void) {
    size_t len = 0;
    const struct value_type_mapping_s *mapping_ptr = VALUE_TYPE_MAPPING;

    while (mapping_ptr->part_str) {
        len += strlen(mapping_ptr->part_str);
        mapping_ptr++;
    }

    return TRIGGER_ARG_STRING_MAX_LENGTH - len - 1; /* For '\0' */
}

const char *cond_op_to_str(int op) {
    const struct cond_op_to_str_s *ptr = COND_OP_TO_STR;
    while (ptr->cond_op) {
        if (ptr->cond_op == op)
            return ptr->str;

        ptr++;
    }

    return "UNKN";
}

const char *cond_action_to_str(int action) {
    const struct cond_action_to_str_s *ptr = COND_ACTION_TO_STR;
    while (ptr->cond_action) {
        if (ptr->cond_action == action)
            return ptr->str;

        ptr++;
    }

    return "UNKN";
}

const char *cond_type_to_str(int type) {
    const struct cond_type_to_str_s *ptr = COND_TYPE_TO_STR;
    while (ptr->cond_type) {
        if (ptr->cond_type == type)
            return ptr->str;

        ptr++;
    }

    return "UNKN";
}

