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

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <syslog.h>
#include <wiringPi.h>

#include "rpiweatherd_config.h"
#include "confighandler.h"
#include "util.h"
#include "device.h"
#include "dbhandler.h"
#include "listener.h"

#ifdef RPIWD_DEBUG
#include <mcheck.h>
#endif /* RPIWD_DEBUG */

/* Global variables */
static volatile sig_atomic_t __hupsignal = 0, __termsignal = 0;
static int pid_fd;
static char *config_path = NULL;

static void handle_sighup(int sig) {
	__hupsignal = 1;
}

static void handle_sigterm(int sig) {
	__termsignal = 1;
}

void init_sighandling(void) {
	signal(SIGHUP, handle_sighup);
	signal(SIGTERM, handle_sigterm);
}

void init_logging(void) {
	openlog("rpiweatherd", LOG_PID, LOG_USER);
}

void quit_logging(void) {
	closelog();
}

void init_routine(void) {
}

void quit_routine(void) {
	/* Quit various subsystems */
	quit_logging();
	quit_dbhandler();
	quit_listener_loop();

	/* Free memory */
	free_current_config();
	free(config_path);

	/* Close and remove PID file */
	close(pid_fd);
	unlink(PID_FILE);
}

void version(void) {
	printf(ASCII_TITLE, RPIWEATHERD_VERSION_MAJOR, RPIWEATHERD_VERSION_MINOR);
}

void help(void) {
	printf("\nUsage: rpiweatherd [ -v | -h | -l | -g | -c PATH]\n");

	printf("\n-c\tUse a different configuration file");
	printf("\n-g\tGenerate a default configuration file");
	printf("\n-l\tList all supported sensor devices");
	printf("\n-v\tShow version information and exit");
	printf("\n-h\tShow help string and exit\n");
}

void query_loop(void) {
	int slept = 0, retflag, qattempts, ok_flag;
	float results[2];

	/* Query loop */
	while (1) {
		/* Reset flag */
		ok_flag = 0;
		qattempts = 0;

		/* Query data */
		while (!ok_flag && qattempts < CONFIG_MAX_QUERY_ATTEMPTS) {
			retflag = device_query_current(get_current_config()->units[0], results);

			/* Check return value */
			switch (retflag) {
				case RPIWD_DEVRETCODE_SUCCESS:
					request_write_entry(results[0], results[1], 
							get_current_config()->measure_location,
							get_current_config()->device_name);
					ok_flag = 1;
					qattempts = 0;

					break;
				case RPIWD_DEVRETCODE_DEVICE_FAILURE:
				case RPIWD_DEVRETCODE_DATA_FAILURE:
				case RPIWD_DEVRETCODE_GENERAL_FAILURE:
					syslog(LOG_WARNING, 
							"device %s failed to query; %d/%d attempts have been made.",
							get_current_config()->device_name, ++qattempts, 
							CONFIG_MAX_QUERY_ATTEMPTS);

					break;
				case RPIWD_DEVRETCODE_MEMORY_ERROR:
					syslog(LOG_ERR, "out of memory to perform query.");
					ok_flag = 1;
					break;
				default:
					syslog(LOG_ERR, "error code %d encountered while querying.",
							retflag);
					break;
			}
		}

		/* Sleep to wait till the next query time */
		rpiwd_sleep(rpiwd_units_to_milliseconds(get_current_config()->query_interval));

		/* Check if "woken up" */
		if (__hupsignal) { /* SIGHUP = Reload all configs, devices, etc. */
			__hupsignal = 0;

			quit_logging();
			quit_dbhandler();
			quit_listener_loop();
			init_logging();
			init_dbhandler();
			free_current_config();
			init_current_config(config_path);
			init_listener_loop(get_current_config()->num_worker_threads, 
							   get_current_config()->comm_port);
		}
		else if (__termsignal) { /* SIGTERM = Terminate application (quickly) */
			quit_routine();
			break;
		}
	}
}

int main(int argc, char **argv) {
	int opt;
	int slept = 0, retflag, qattempts, ok_flag, devinit_flag;
	float results[2];

#ifdef RPIWD_DEBUG
	mtrace();
#endif /* RPIWD_DEBUG */

	/* version() acts as title message */
	version();

	/* Parse arguments */
	while ((opt = getopt(argc, argv, "glhvc:")) != -1) {
		switch (opt) {
			case 'c': /* Use some custom configuration file */
				{
					if (optarg)
						config_path = strdup(optarg);
					else {
						fprintf(stderr, "error: -c missing an argument\n");
						return EXIT_FAILURE;
					}
				}
				break;
			case 'v': /* Show version information (already shown!) */
				return EXIT_SUCCESS;
			case 'g': /* Create a blank configuration file */
				{
					int status = generate_blank_config(CONFIG_FILE_DEFAULT_LOCATION);
					if (status == -1)
						perror("\nconfiguration error");
					else {
						printf("\nConfig file generated at %s.", CONFIG_FILE_DEFAULT_LOCATION);
						printf("\nNote that you must still configure it before use!\n");
					}
				}
				return EXIT_SUCCESS;
			case 'l': /* List all supported devices */
				print_supported_device_names();
				return EXIT_SUCCESS;
			case 'h': /* Show help text */
				help();
				return EXIT_SUCCESS;
			default: /* Unknown option */
				fprintf(stderr, "Unknown option '%c'.\n", optopt);
				return EXIT_FAILURE;
		}
	}

	/* Make into daemon */
	if (daemon(0, 0) == -1) {
		fprintf(stderr, "%s: error: failed to make into daemon.", argv[0]);
		perror("\ndaemon:");
		goto close_pidfile;
		return EXIT_FAILURE;
	}
	
	/* Check if PID file is there. */
	if (pid_file_exists()) {
		fprintf(stderr, "\nError: Pid file found at %s.\nEither a rpiweatherd instance " \
				"is running, or that file is empty.", PID_FILE);
		return EXIT_FAILURE;
	}
	else {
		/* Write the PID file */
		write_pid_file();
	}

	/* Initialize logging */
	init_logging();

	/* Parse configuration file */
	if (!config_path)
		config_path = strdup(CONFIG_FILE_DEFAULT_LOCATION);
	
	if (init_current_config(config_path) < 0) {
		syslog(LOG_ERR, "%s: error: Could not read configuration file %s.\n", 
				argv[0], config_path);
		goto close_pidfile;
		return EXIT_FAILURE;
	}

	/* Initialize signal handling */
	init_sighandling();

	/* Check configuraton */
	if (config_has_errors(get_current_config()) > 0) {
		syslog(LOG_ERR, "%s: error: Configuration errors encountered.\n", argv[0]);
		goto close_pidfile;
		return EXIT_FAILURE;
	}

	/* Initialize wiringPi */
	wiringPiSetup();

	/* Test and initialize measuring instrument */
	devinit_flag = device_init_by_name(get_current_config()->device_name, 
			get_current_config()->device_config);
	if (devinit_flag != RETCODE_DEVICE_INIT_OK) {
		syslog(LOG_ERR, "%s: error: Could not initialize device \"%s\". Please " \
				"check configuration file.\n", argv[0], 
				get_current_config()->device_name);
		goto close_pidfile;
		return EXIT_FAILURE;
	}

	if (device_test_current() != 1) {
		syslog(LOG_ERR, "%s: error: Device \"%s\" at data pin %d failed self-testing.\n",
				argv[0], get_current_config()->device_name, 
				get_current_device()->pin_data);
		goto close_pidfile;
		return EXIT_FAILURE;
	}

	/* Initialize database */
	if (init_dbhandler() == -1) {
		syslog(LOG_ERR, "%s: error: Error initializing SQLite3 database.\n", argv[0]);
		quit_dbhandler();
		goto close_pidfile;
		return EXIT_FAILURE;
	}

	/* Finally - spin thread that listens for incoming GET requests */
	init_listener_loop(get_current_config()->num_worker_threads, 
					   get_current_config()->comm_port);

	/* Initiate query loop */
	query_loop();

close_pidfile:
	close(pid_fd);
	unlink(PID_FILE);

#ifdef RPIWD_DEBUG
	muntrace();
#endif

	return EXIT_SUCCESS;
}
