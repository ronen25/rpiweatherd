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

#include "confighandler.h"

static rpiwd_config __current_configuration;
static pthread_mutex_t __mtx_config = PTHREAD_MUTEX_INITIALIZER;
static size_t temp_count;

/* Internal callback */
int inih_callback(void *info, const char *section, const char *name, const char *value) {
    rpiwd_config *confstrct = (rpiwd_config *)info;

    /* Check if value has any content.
     * All values in the configuration must have values. */
    if (!*value) {
        fprintf(stderr, "\nERROR: configuration key '%s' is missing a value.\n",
                name);
        return 0;
    }

	/* Get query interval */
	if (strcmp(name, CONFIG_LOCATION) == 0) /* Measurement location */
		confstrct->measure_location = strdup(value);
	else if (strcmp(name, CONFIG_QUERY_INTERVAL) == 0) { /* Query interval */
		confstrct->query_interval = strdup(value);
	}
	else if (strcmp(name, CONFIG_DEVICE_NAME) == 0) /* Device name */
		confstrct->device_name = strdup(value);
	else if (strcmp(name, CONFIG_DEVICE_CONFIG) == 0) { /* Device configuration */
		confstrct->device_config = strtol(value, NULL, 10);
		if (errno == ERANGE)
			return CONFIG_ERROR_DEVICE_CONFIG; /* Configuration error */
	}
    else if (strcmp(name, CONFIG_UNITS) == 0) {/* Default measurement units */
        fprintf(stderr, "\nWarning: \'units\' setting has been deprecated " \
                "in version 1.1, and will be ignored.");
    }
	else if (strcmp(name, CONFIG_COMM_PORT) == 0) /* Default communication port */ {
		confstrct->comm_port = strtol(value, NULL, 10);
		if (errno == ERANGE)
			return CONFIG_ERROR_COMM_PORT; /* Configuration error */
	}
	else if (strcmp(name, CONFIG_NUM_WORKER_THREADS) == 0) /* Number of worker t. */ {
		confstrct->num_worker_threads = strtol(value, NULL, 10);
		if (errno == ERANGE)
			return CONFIG_ERROR_NUM_WTHREADS; /* Configuration error */
	}
	else
		return 0; /* Unknown section, name or error */

	/* Increase configuration count */
	temp_count++;

	return 1;
}

int write_config_file(const char *path, rpiwd_config *confstrct) {
	FILE *f = NULL;

	/* Open the file */
	f = fopen(path, "wt");
	if (f == NULL) {
		fprintf(stderr, "\nerror: Failed to open file %s for writing.", path);
		return -1; /* Failure to open file */
	}

	/* Write general configuration */
	fprintf(f, "[General]\n");
	fprintf(f, "%s=%s\n", CONFIG_LOCATION, confstrct->measure_location);
	fprintf(f, "%s=%s\n", CONFIG_QUERY_INTERVAL, confstrct->query_interval);

	/* Write device configuration */
	fprintf(f, "\n[Device Configuration]\n");
	fprintf(f, "%s=%s\n", CONFIG_DEVICE_NAME, confstrct->device_name);
	fprintf(f, "%s=%d\n", CONFIG_DEVICE_CONFIG, confstrct->device_config);

	/* Write server configuration */
	fprintf(f, "\n[Server Configuration]\n");
	fprintf(f, "%s=%d\n", CONFIG_COMM_PORT, confstrct->comm_port);
	fprintf(f, "%s=%d\n", CONFIG_NUM_WORKER_THREADS, confstrct->num_worker_threads);

	/* Close file */
	fclose(f);

	return 0;
}

/* Actual function that parsed the file into a configuration structure */
int parse_config_file(const char *path, rpiwd_config *confstrct) {
	/* A static variable should be reset just in case */
	temp_count = 0;

	int parse_flag = ini_parse(path, inih_callback, confstrct);
	confstrct->config_count = temp_count;

	return parse_flag;
}

void free_config(rpiwd_config *confstrct) {
	if (confstrct->measure_location)
		free(confstrct->measure_location);

	if (confstrct->device_name)
		free(confstrct->device_name);

	if (confstrct->query_interval)
		free(confstrct->query_interval);

	confstrct->comm_port = confstrct->device_config = 0;
}

int generate_blank_config(const char *path) {
    /* Attempt to copy the blank configuration file in /etc/rpiweatherd/skel */
    int status = rpiwd_copyfile(CONFIG_BLANK_FILE_LOCATION, path);
    if (!status) {
        perror("rpiwd_copyfle");
        return -1;
    }

    return 1;
}

/* A configuration checker */
int config_has_errors(rpiwd_config *confstrct) {
	int flag = 0, nameflag = 0;
	unsigned int convert_test;

	/* Measure location should not be NULL */
	if (!confstrct->measure_location) {
		flag++;
		fprintf(stderr, "\nconfiguration error: Measurement location not set.");
	}

	/* Check if device name is supported */
	device *ptr = supported_devices;
	while (ptr->device_name && !nameflag) {
		if (strcmp(confstrct->device_name, ptr->device_name) == 0)
			nameflag = 1;

		ptr++;
	}

	if (!nameflag) {
		flag++;
		fprintf(stderr, "\nconfiguration error: Unsupported device \"%s\".", 
				confstrct->device_name);
	}

	/* No real way to check device configuration... */

	/* Query interval is checked upon conversion */
	if (confstrct->query_interval == 0) {
		flag++;
		fprintf(stderr, "\nconfiguration error: Unable to parse query interval.");
	}

	printf("\n");

	/* No real way to check communication port... */

	/* Check number of worker threads provided */
	if (confstrct->num_worker_threads == 0) {
		flag++;
		fprintf(stderr, "\nconfiguration error: num_worker_threads unconfigured.");
	}
	else if (confstrct->num_worker_threads > CONFIG_NUM_WORKER_THREADS_MAX ||
			confstrct->num_worker_threads < 0) {
		flag++;
		fprintf(stderr, "\nconfiguration error: num_worker_threads out of bounds.");
	}

	/* Return flag */
	return flag;
}

rpiwd_config *get_current_config(void) {
	rpiwd_config *ret = NULL;

	pthread_mutex_lock(&__mtx_config);
		ret = &__current_configuration;
	pthread_mutex_unlock(&__mtx_config);

	return ret;
}

int init_current_config(const char *config_path) {
    int flag;

	pthread_mutex_lock(&__mtx_config);
		flag = parse_config_file(config_path, &__current_configuration);
	pthread_mutex_unlock(&__mtx_config);

	return flag;
}

void free_current_config(void) {
	pthread_mutex_lock(&__mtx_config);
		free_config(&__current_configuration);
	pthread_mutex_unlock(&__mtx_config);
}
