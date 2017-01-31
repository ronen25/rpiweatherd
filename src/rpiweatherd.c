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
#include "trigger.h"

#ifdef RPIWD_DEBUG
#include <mcheck.h>
#endif /* RPIWD_DEBUG */

/* Global variables */
static volatile sig_atomic_t __hupsignal = 0, __termsignal = 0;
static int pid_fd = -1;
static char *config_path = NULL;

/* ============================ Getopt long option table ============================= */
static struct option rpiwd_long_options[] = {
    { "config", required_argument, 0, 'c' },
    { "genconfig", no_argument, 0, 'g' },
    { "listdevices", no_argument, 0, 'l' },
    { "foreground", no_argument, 0, 'f'},
    { "listtriggers", no_argument, 0, 't' },
    { "version", no_argument, 0, 'v' },
    { "help", no_argument, 0, 'h'},
    { 0, 0, 0, 0 }
};


/* ============================ Signal Handlers ============================= */
static void handle_sighup(int sig) {
	__hupsignal = 1;
}

static void handle_sigterm_sigint(int sig) {
	__termsignal = 1;
}

void init_sighandling(void) {
	signal(SIGHUP, handle_sighup);
    signal(SIGTERM, handle_sigterm_sigint);
    signal(SIGINT, handle_sigterm_sigint);
}

void init_routine(void) { }

void quit_routine(void) {
	/* Quit various subsystems */
	quit_logging();
	quit_dbhandler();
	quit_listener_loop();
    unload_triggers();

	/* Free memory */
	free_current_config();
    free(config_path);

    /* Close pid file */
    if (pid_fd != -1) {
        close(pid_fd);
        remove(PID_FILE);
    }
}

void version(void) {
    printf(ASCII_TITLE, RPIWEATHERD_VERSION,
            RPIWEATHERD_BUILD_DATE ? RPIWEATHERD_BUILD_DATE : "unknown");
}

void help(void) {
	printf("\nUsage: rpiweatherd [ -v | -h | -l | -g | -c PATH]\n");

    printf("%-16s%-5s%-10s%-50s", "\n--config", "-c", "[path]",
           "Use a different configuration file");
    printf("%-16s%-15s%-50s", "\n--genconfig", "-g",
           "Generate a default configuration file");
    printf("%-16s%-15s%-50s", "\n--listdevices", "-l",
           "List all supported sensor devices");
    printf("%-16s%-15s%-50s", "\n--foreground", "-f",
           "Run in foreground (do not daemonize)");
    printf("%-16s%-15s%-50s", "\n--version", "-v",
           "Show version information and exit");
    printf("%-16s%-15s%-50s", "\n--help", "-h",
           "Show help string and exit\n");

    fputc('\n', stdout);
}

void query_loop(void) {
    int slept = 0, retflag, qattempts, ok_flag;
    float results[RPIWD_MAX_MEASUREMENTS];

	/* Query loop */
	while (1) {
		/* Reset flag */
		ok_flag = 0;
		qattempts = 0;

		/* Query data */
		while (!ok_flag && qattempts < CONFIG_MAX_QUERY_ATTEMPTS) {
            retflag = device_query_current(results);

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
                    if (qattempts % 5)
                        rpiwd_log(LOG_WARNING,
                                "device %s failed to query; %d/%d attempts so far.",
                                get_current_config()->device_name, ++qattempts,
                                CONFIG_MAX_QUERY_ATTEMPTS);

					break;
				case RPIWD_DEVRETCODE_MEMORY_ERROR:
					rpiwd_log(LOG_ERR, "out of memory to perform query.");
					ok_flag = 1;
					break;
				default:
					rpiwd_log(LOG_ERR, "error code %d encountered while querying.",
							retflag);
					break;
			}
		}

        /* Execute triggers */
        trigger_exec_callback(results);

		/* Sleep to wait till the next query time */
		rpiwd_sleep(rpiwd_units_to_milliseconds(get_current_config()->query_interval));

		/* Check if "woken up" */
		if (__hupsignal) { /* SIGHUP = Reload all configs, devices, etc. */
			__hupsignal = 0;

			quit_dbhandler();
            quit_listener_loop();
            unload_triggers();
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
    int opt, opt_index = 0;
    int slept = 0, retflag, qattempts, ok_flag, devinit_flag;
    float results[2];
    bool run_in_foreground = false;

#ifdef RPIWD_DEBUG
	mtrace();
#endif /* RPIWD_DEBUG */

	/* version() acts as title message */
	version();

	/* Parse arguments */
    while ((opt = getopt_long(argc, argv, "tglfhvc:i", rpiwd_long_options,
                              &opt_index)) != -1) {
		switch (opt) {
			case 'c': /* Use some custom configuration file */
				{
					if (optarg)
						config_path = strdup(optarg);
					else {
                        fprintf(stderr, "error: Missing path to configuration file.\n");
						return EXIT_FAILURE;
					}
				}
				break;
			case 'v': /* Show version information (already shown!) */
				return EXIT_SUCCESS;
			case 'g': /* Create a blank configuration file */
                {
                    /* Check if the file already exists */
                    if (rpiwd_file_exists(CONFIG_FILE_DEFAULT_LOCATION)) {
                        printf("\nA configuration file is already present at '%s'.\n" \
                               "Remove it and replace it with a blank file? (y/n) ",
                               CONFIG_FILE_DEFAULT_FOLDER);

                        /* Input answer */
                        scanf("%c", &opt);
                        if (opt == 'y' || opt == 'Y')
                            remove(CONFIG_FILE_DEFAULT_LOCATION);
                        else
                            return EXIT_SUCCESS;
                    }

                    /* Generate a new configuration filee */
                    int status = generate_blank_config(CONFIG_FILE_DEFAULT_LOCATION);
					if (status == -1)
						perror("\nconfiguration error");
					else {
                        printf("\nConfig file generated at %s.",
                               CONFIG_FILE_DEFAULT_LOCATION);
                        printf("\nNOTE: Please configure it before use!\n");
					}
				}
                return EXIT_SUCCESS;
            case 'f': /* Run in foreground */
                run_in_foreground = true;
                break;
            case 't': /* List triggers */
                list_triggers();
                return EXIT_SUCCESS;
			case 'l': /* List all supported devices */
				print_supported_device_names();
				return EXIT_SUCCESS;
			case 'h': /* Show help text */
				help();
				return EXIT_SUCCESS;
			default: /* Unknown option */
                fprintf(stderr, "Unknown option '%c'.\n", opt);
				return EXIT_FAILURE;
		}
    }

    /* Initialize logging */
    init_logging(run_in_foreground);

    /* Initialize wiringPi */
    wiringPiSetup();

    /* Parse configuration file */
    if (!config_path)
        config_path = strdup(CONFIG_FILE_DEFAULT_LOCATION);

    if (init_current_config(config_path) != 0) {
        fprintf(stderr, "%s: errors in configuration file %s.\n",
                argv[0], config_path);
        return EXIT_FAILURE;
    }

    /* Check configuraton */
    if (config_has_errors(get_current_config()) > 0) {
        fprintf(stderr, "%s: error: Configuration errors encountered.\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Load triggers from file */
    if (load_triggers() != TRIGGER_PARSE_OK) {
        fprintf(stderr, "%s: error: could not load trigger file.", argv[0]);
        return EXIT_FAILURE;
    }

    /* Check if PID file is there. */
    if (pid_file_exists()) {
        fprintf(stderr, "\nError: Pid file found at %s.\nEither a rpiweatherd instance " \
                "is running, or that file is empty.\n", PID_FILE);
        return EXIT_FAILURE;
    }

    /* Test and initialize measuring instrument */
    devinit_flag = device_init_by_name(get_current_config()->device_name,
            get_current_config()->device_config);
    if (devinit_flag != RETCODE_DEVICE_INIT_OK) {
        fprintf(stderr, "%s: error: Could not initialize device \"%s\". Please " \
                "check configuration file.\n", argv[0],
                get_current_config()->device_name);

        return EXIT_FAILURE;
    }

    if (device_test_current() != 1) {
        fprintf(stderr, "%s: error: Device \"%s\" at data pin %d failed self-testing.\n",
                argv[0], get_current_config()->device_name,
                get_current_device()->pin_data);

        return EXIT_FAILURE;
    }

   /* Make into daemon */
    if (!run_in_foreground) {
        if (daemon(0, 0) == -1) {
            fprintf(stderr, "%s: error: failed to make into daemon.", argv[0]);
            perror("\ndaemon:");

            return EXIT_FAILURE;
        }
    }

    /* Write the PID file */
    pid_fd = write_pid_file();

	/* Initialize signal handling */
	init_sighandling();

	/* Initialize database */
	if (init_dbhandler() == -1) {
		rpiwd_log(LOG_ERR, "%s: error: Error initializing SQLite3 database.\n", argv[0]);
		quit_dbhandler();

		return EXIT_FAILURE;
	}

	/* Finally - spin thread that listens for incoming GET requests */
	init_listener_loop(get_current_config()->num_worker_threads, 
					   get_current_config()->comm_port);

	/* Initiate query loop */
	query_loop();

#ifdef RPIWD_DEBUG
	muntrace();
#endif

	return EXIT_SUCCESS;
}
