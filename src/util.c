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

#include "util.h"

static const char *SUPPORTED_DATETIME_FORMATS[] = {
    "%Y-%m-%d %H:%M",
    "%Y-%m-%d",
    NULL
};

int write_pid_file(void) {
	int fd = 0;
	char temp_buffer[PID_NUMBER_BUFFER_LENGTH];

    if ((fd = open(PID_FILE, O_RDWR | O_CREAT | O_EXCL, 0444)) == -1)
		return 0;
	else {
		/* Write PID */
		sprintf(temp_buffer, "%ld", (long)getpid());
		write(fd, temp_buffer, strlen(temp_buffer));
	}

	return fd;
}

int pid_file_exists(void) {
	struct stat st;
	int result = stat(PID_FILE, &st);

	return result == 0;
}

/* Conversion from rpiwd time units to seconds */
unsigned int rpiwd_units_to_milliseconds(const char *unitstr) {
	const char *ptr = unitstr;
	char unit = '\0';
	float count = -1, seconds = 1;

	/* Iterate until the unit is found, or end of string. */
	while (isdigit(*ptr) || *ptr == '.' && *ptr != '\0') { ptr++; }
	if (*ptr)
		unit = *ptr;

	/* After the unit there should be no more tokens remaining.
	 * If there are more characters than that, it is an error. */
	if (*(ptr + 1) != '\0')
		return 0;

	/* Check unit */
	switch (unit) {
		case 's': /* Seconds */
			break;
		case 'm': /* Minutes */
			seconds *= 60;
			break;
		case 'h': /* Hours */
			seconds *= 60 * 60;
			break;
		case 'd': /* Days */
			seconds *= 60 * 60 * 24;
			break;
		default: /* Unknown / missing */
			return 0;
	}

	/* Get count and check it. If it is correct, multiply it by value in seconds */
	count = strtof(unitstr, NULL);
	if (errno == ERANGE || count == 0)
		return 0;

	/* Multiply */
	seconds *= count * 1000;

	return (unsigned int)seconds;
}

char rpiwd_direction_to_char(const char *direction) {
	if (strcmp(direction, DIRECTION_STRING_FROM) == 0)
		return '>';
	else if (strcmp(direction, DIRECTION_STRING_TO) == 0)
		return '<';
	else
		return '?';
}

int is_rpiwd_units(const char *str) {
	float count = 0;

	/* Check for a number */
	count = strtof(str, NULL);
	if (count == 0 || errno == ERANGE)
		return 0;

	return 1;
}

void rpiwd_sleep(unsigned int milliseconds) {
	struct timespec tms;

	/* Convert milliseconds back to seconds.
	 * If anything remains, multiply and put in nanoseconds. */
	tms.tv_sec = milliseconds / 1000;
	tms.tv_nsec = milliseconds % 1000 * 1000000;

	nanosleep(&tms, NULL);
}

char *rpiwd_getline(const char *line, const char *newline) {
	char *nline_pos = NULL, *buffer;
	ptrdiff_t length;

	/* Find the newline */
	nline_pos = strstr(line, newline);
	if (!nline_pos)
		return NULL; /* No newline */

	/* Get the difference. This is the length. */
	length = nline_pos - line;
	if (length <= 0)
		return NULL; /* Empty string */

	/* Allocate and copy memory */
	buffer = malloc(sizeof(char) * length + 1);
	if (!buffer)
		return NULL;

	/* Copy and put newline */
	strncpy(buffer, line, length);
	buffer[length] = '\0';

	return buffer;
}

time_t rpiwd_units_to_time_t(const char *unitstr, char operation) {
	time_t now = time(NULL);

	/* Get milliseconds */
	unsigned int millis = rpiwd_units_to_milliseconds(unitstr);
	if (millis == 0)
		return 0;

	/* Add/subtract */
	switch (operation) {
		case '+':
			now += (time_t) millis / 1000;
			break;
		case '-':
			now -= (time_t) millis / 1000;
			break;
		default:
			now = 0;
			break;
	}

	return now;
}


time_t normalize_date(const char *value, bool *rdtn_performed) {
    time_t temp;
    struct tm temptm = { 0 };
    char *retval;

    /* Check if it is the "now" string */
    if (strcmp(value, "now") == 0)
        return time(NULL);

    for (const char **format_ptr = SUPPORTED_DATETIME_FORMATS;
         *format_ptr; format_ptr++) {
        if ((retval = strptime(value, *format_ptr, &temptm)) != NULL) {
            temp = mktime(&temptm);
            *rdtn_performed = false;

            break;
        }
    }

    /* Check value */
    if (*rdtn_performed) {
        /* Attempt to parse as rpiwd units */
        temp = rpiwd_units_to_time_t(value, '-');
    }

    /* We tried all we can */
    return temp;
}

int rpiwd_copyfile(const char *src, const char *dest) {
    FILE *fsrc, *fdst;
    char buffer[RPIWD_COPYFILE_BUFFSIZE];
    size_t read_bytes = 0, written_bytes = 0;

    /* Open source and dest file */
    fsrc = fopen(src, "rb");
    if (!fsrc)
        return 0;

    fdst = fopen(dest, "wb");
    if (!fdst)
        return 0;

    /* Read/Write loop */
    while ((read_bytes =
            fread(buffer, sizeof(char), RPIWD_COPYFILE_BUFFSIZE, fsrc)) != 0) {
        written_bytes = fwrite(buffer, sizeof(char), read_bytes, fdst);
        if (written_bytes != read_bytes) {
            fclose(fsrc);
            fclose(fdst);

            return -1;
        }
    }

    /* Done. */
    fclose(fsrc);

    fflush(fdst);
    fclose(fdst);

    return 1;
}

int rpiwd_file_exists(const char *path) {
    return access(path, F_OK) != -1;
}

int rpiwd_is_number(const char *str) {
    const char *ptr = str;
    while (*ptr) {
        if (!isdigit(*ptr))
            return 0;

        ptr++;
    }

    return 1;
}
