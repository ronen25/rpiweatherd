'''
rpiweatherd-admin.py - An administration script for rpiweatherd.
Copyright (C) 2016 Ronen Lapushner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
'''

import sys
import os
import sqlite3
import subprocess
import configparser
import time
import argparse

from deps.CheckedInput import CheckedInput

__author__ = 'Ronen Lapushner'
__version__ = '1.0'

# Various constants
DB_FILE_PATH = '/etc/rpiweatherd/rpiwd_data.db'
CONFIG_FILE_PATH = '/etc/rpiweatherd/rpiweatherd.conf'

#-----------------------------------------------------------
def __is_daemon_running():
    # Execute pgrep
    process = subprocess.Popen(['pgrep', 'rpiweatherd'], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    output, err = process.communicate()
    
    return output
    
def version_info():
    '''
    Shows daemon version information.
    '''
    # Display daemon version
    os.system('rpiweatherd -v')

def db_stats():
    '''
    Show server and database statistics.
    '''
    SQL_QUERIES = {
        1: '''SELECT COUNT(*) FROM tblData;''',
        2: '''SELECT * FROM tblStats;'''
    }
    
    # Get operation
    ans = CheckedInput.CheckedInput.input_choice_numeric_list('Enter your choice:',
        [
            'Count all entries',        # 1
            'View server statistics'    # 2
        ], True)
    
    # Check if aborted
    if not ans or ans == 0:
        print('Aborted.\n')
        return

    # Open database file and drop selected tables
    conn = sqlite3.connect(DB_FILE_PATH)
    c = conn.cursor()
                
    # Get result
    c.execute(SQL_QUERIES[ans])

    # Print result(s)
    if ans == 1:
        print('Entry count: {}\n'.format(c.fetchone()[0]))
    elif ans == 2:
        results = c.fetchall()

        for name, displayname, value in results:
            print('{:<40}{:<20}'.format(displayname, value))

    conn.close()
    
    print('DONE\n')

def db_cleanup():
    # Table of SQL queries to perform when a proper numeric action is selected.
    SQL_QUERIES = {
        1: '''DROP TABLE IF EXISTS tblStats;''',
        2: '''DROP TABLE IF EXISTS tblData;''',
    }

    # Check if database file exists
    if os.path.isfile(DB_FILE_PATH):
        print('Database file found at {}.'.format(DB_FILE_PATH))
        
        # Check if the daemon is running.
        # Database operations can not be done while the daemon is running!
        if __is_daemon_running():
            print('Can\'t perform database operations while rpiweatherd is running.')
            return
        
        # Print clearing menu
        ans = CheckedInput.CheckedInput.input_choice_numeric_list('Enter your choice:',
            [
                'Clear statistics table',
                'Clear data table',
            ], True)

        # Check input
        if not ans or ans == 0:
            return
        else:
            # Ask for confirmation
            print('WARNING: The erased data will be unrecoverable.')
            confirm = CheckedInput.CheckedInput.input_yes_no('Confirm? ')
            if not confirm or confirm == 'n':
                return
                
            # Open database file and drop selected tables
            try:
                conn = sqlite3.connect(DB_FILE_PATH)
                c = conn.cursor()

                c.execute(SQL_QUERIES[ans])
                
                conn.commit()
                conn.close()
            except sqlite3.OperationalError as ex:
                print('Operational error: {}.'.format(ex))
                
            print('Done.')
                
    else:
        print('Database file could not be found.')

def init_argparser():
    argparser = argparse.ArgumentParser()

    # Everything is in one group
    group = argparser.add_argument_group('arguments')
    group.add_argument('-s', '--db-statistics', help='Show database statistics',
            action='store_true')
    group.add_argument('-d', '--daemon-version', help='View rpiweatherd\'s version ' \
            'information.', action='store_true')
    group.add_argument('-r', '--is-running', help='View whether rpiweatherd is running.',
            action='store_true')
    group.add_argument('-c', '--cleanup', help='Lanuch an interactive cleanup tool.',
            action='store_true')

    return argparser

def main():
    # Parse arguments
    args = init_argparser().parse_args()

    # Check what option was specified
    if args.db_statistics:
        db_stats()
    elif args.daemon_version:
        version_info()
    elif args.is_running:
        print('rpiweatherd is currently {}.'.format(
            'running' if __is_daemon_running() else 'stopped'))
    elif args.cleanup:
        db_cleanup()

if __name__ == '__main__':
    # Check for python version
    if sys.version_info.major >= 3 and sys.version_info.minor >= 2:
        main()
    else:
        print('rpiweatherd-admin.py requires Python 3.2 but {}.{} was used to run it.'
                .format(sys.version_info.major, sys.version_info.minor))
