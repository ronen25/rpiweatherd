'''
rpiweatherd-cli.py - A command-line client for rpiweatherd written in Python 3.
Copyright (C) 2016-2017 Ronen Lapushner

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
import urllib.request
import urllib.parse
import json
import cmd
import re
import argparse

from deps import CheckedInput

__author__ = 'Ronen Lapushner'
__version__ = '1.0.1'

# Connection constants
SERVER_REQUEST_URL = 'http://{}:{}/{}'

# Regex compiled
ARG_VALIDATOR = re.compile('\w+=.+')
CONN_DETAILS = (None, None, None)
CONFIG = None

# Other constants
ENTRY_COMPACT_FORMAT = '{:<20}{:>20}{}{:>15}%'

class CmdShell(cmd.Cmd):
    prompt= '(rpiweatherd-cli) '
    
    # Utility Methods
    def __is_connected(self):
        return CONN_DETAILS[0] != None

    def __print_entry(self, data, tempunit, compact_print=False):
        if not compact_print:
            print('Temperature:\t\t{}{}\nHumdity:\t\t{}%\n\nMeasurement Details:\n' \
                    'Taken in \t\t{} (using {})\n'
                    .format(data['temperature'], 
			    tempunit,
                            data['humidity'], 
                            data['location'], data['device_name']))
        else:
            print(ENTRY_COMPACT_FORMAT.format(
                data['record_date'], 
                data['temperature'],
		tempunit,
                data['humidity']))

    def __print_entries_header(self):
        print(ENTRY_COMPACT_FORMAT.format('Measurement Date and Time', 
            'Temperature ({})'.format('C' if CONFIG['units'] == 'metric' else 'F'),
            '', 'Humidity '))
        
    def get_data(self, server, port, command):
        data = None
        
        # Connect
        url = SERVER_REQUEST_URL.format(server, port, command)
        
        # Send request
        try:
            with urllib.request.urlopen(url) as req:
                # Read buffer
                data = req.read()
         
            # Parse JSON to Python object
            return json.loads(data.decode('utf-8')) if data else None
        except urllib.error.URLError as err:
            print('Error: {}.'.format(err))

            return None
        
    def validate_argument_format(self, arg):
        return ARG_VALIDATOR.match(arg)

    def get_cmp_key(self, item):
        return item[-1]['record_date']
    
    def get_config(self):
        '''
        An internal method, used by the client on startup, to get
        configuration information from the server.
        '''
        global CONN_DETAILS
        global CONFIG

        # Do request
        data = self.get_data(CONN_DETAILS[0], CONN_DETAILS[1], 'config')
        
        # Print configuration
        if data:
            # Store configuration
            CONFIG = data['results']
            return True

        return False

    # -----------------------------------------------------------------------------------

    # Methods
    def do_fetch(self, args):
        '''
        Fetch data from, to or a range of dates.
        
        For fetching data between two dates, use:
        fetch from=... to=...
        
        For fetching data on a certain data, use:
        fetch on=...
        
        For fetching data from a certain date until the current date, use:
        fetch from=...
        
        For fetching data from the first entry until a certain date, use:
        fetch to=...
        
        Note that the dates must be either RDTN or ISO-8601.
        '''
        global CONN_DETAILS

        # Verify server connectivity
        if not self.__is_connected():
            print('Must be connected to a server.')
            return

        # Verify arguments and connection
        parsed_args = args.split()
        if len(parsed_args) == 0:
            print('Arguments required.')
            return

        # Verify argument format
        for i, arg in zip(range(1, len(parsed_args)), parsed_args):
            if not self.validate_argument_format(arg):
                print('Format error in argument no.{}'.format(i))
                
        # Build parameter query
        params = '&'.join(parsed_args)
        
        # Get data
        data = self.get_data(CONN_DETAILS[0], CONN_DETAILS[1], 
                'fetch?{}'.format(params))
        
        # Check if data has been returned
        if data:
            # Check error
            if data['errcode'] != 0:
                print('Error {}: {}'.format(data['errcode'], data['errmsg']))
            else:
                # Print entry count
                print('Total entries: {}\n'.format(data['length']))

                # Print header
                self.__print_entries_header()
                
                # Print entries
                for entry in sorted(data['results'].items(), key=self.get_cmp_key):
                    self.__print_entry(entry[-1], data['units']['tempunits'], True)
    
    def do_config(self, args):
        '''
        View server configuration
        '''
        global CONN_DETAILS

        # Shouldn't have any arguments
        if args and len(args) > 0:
            print('Command does not accept any parameters.')
            return

        # Verify connection
        if not self.__is_connected():
            print('Must be connected to a server.')
            return
        
        # Do request
        data = self.get_data(CONN_DETAILS[0], CONN_DETAILS[1], 'config')
        
        # Print configuration
        if data:
            print('Total configuration entries: {}\n'.format(data['length']))

            # Store configuration
            CONFIG = data['results']

            # Print configuration
            for key, value in data['results'].items():
                print('{:<20}{:>30}'.format(key, value))
    
    def do_current(self, args):
        '''
        View the current temperature and humidity
        '''
        global CONN_DETAILS

        # Shouldn't have any arguments
        if len(args) > 0:
            print('Command does not accept any parameters.')
            return
        
        # Verify connection
        if not self.__is_connected():
            print('Must be connected to a server.')
            return

        # Do request
        data = self.get_data(CONN_DETAILS[0], CONN_DETAILS[1], 'current')

        # Read entry, if any
        if data and data['length'] > 0:
            self.__print_entry(data['-1'], data['units']['tempunit'])
    
    def do_statistics(self, args):
        '''
        View server statistics
        '''
        global CONN_DETAILS

        # Shouldn't have any arguments
        if len(args) > 0:
            print('Command does not accept any parameters.')
            return

        # Verify connection
        if not self.__is_connected():
            print('Must be connected to a server.')
            return

        # Do request
        data = self.get_data(CONN_DETAILS[0], CONN_DETAILS[1], 'statistics')
        
        # Print configuration
        if data:
            print('Total statistics: {}\n'.format(data['length']))
            for key, value in sorted(data['results'].items()):
                print('{:<25}{:>30}'.format(key, value))
    
    def do_connect(self, args):
        '''
        Connect to a rpiweatherd server
        '''
        global CONN_DETAILS

        # Check for arguments
        if args:
            parsed_args = args.split()

            if len(parsed_args) < 2:
                print('Not enough arguments.')
            else:
                CONN_DETAILS = parsed_args[0], int(parsed_args[1])
        else:
            try:
                server_name = input('Enter server hostname or IP: ')
        
                # Port input loop
                while True:
                    try:
                        comm_port = int(input('Enter server port: '))
                        break
                    except:
                        print('Input error.')

                # Update details
                CONN_DETAILS = server_name, comm_port
            except KeyboardInterrupt:
                print('Aborted.')
            
        # Test connection by getting configuration
        if self.get_config():
            print('Now connected to {} on port {}.'.format(*CONN_DETAILS))
        else:
            print('Error connecting to server {} on port {}.'.format(*CONN_DETAILS))

    def do_connection(self, args):
        '''
        View connection details
        '''
        if not self.__is_connected():
            print('Not connected.')
        else:
            print('Server: {}\nPort: {}\n'.format(*CONN_DETAILS))

    def do_exit(self, args):
        '''
        Exit the program.
        '''
        print('Exiting...')
        sys.exit(0)

def init_argparser():
    argparser = argparse.ArgumentParser()
    argparser.add_argument('hostname', help='Hostname to connect to.', nargs='?')
    argparser.add_argument('port', help='Communication port.', nargs='?')
    argparser.add_argument('--version', '-v', action='store_true', 
            help='Show version and exit.')

    return argparser

if __name__ == '__main__':
    cmd_shell = CmdShell()

    # Check minimal Python version
    if sys.version_info.major <= 3 and sys.version_info.minor < 2:
        print('rpiweatherd-cli requires Python 3.2 or newer.')
        exit(-1)

    # Check command line arguments
    args = init_argparser().parse_args()

    # Check version command
    if args.version:
        print(__version__)
        exit(0)

    # Check if hostname and port were provided.
    # If so attempt to connect.
    if args.hostname and args.port:
        CONN_DETAILS = args.hostname, args.port
        cmd_shell.do_connect(' '.join(CONN_DETAILS))
    
    # Run command loop
    try:
        cmd_shell.cmdloop()
    except KeyboardInterrupt:
        print('Ctrl-C pressed. Exiting...')

