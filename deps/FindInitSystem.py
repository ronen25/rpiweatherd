'''
FindInitSystem.py - A small script for finding the currently installed init system.
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

import os
import subprocess

from collections import OrderedDict

def __is_systemd():
    # Check if systemctl command gives any output.
    # We don't care what output though, just that the command exists.
    try:
        subprocess.check_output(['systemctl'])
        return True
    except:
        return False

def __is_upstart():
    # Check the --version flag output from /sbin/init
    process = subprocess.Popen(['/sbin/init', '--version'], stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
    (output, err) = process.communicate()

    # Check if the word 'upstart' is in there
    if 'upstart' in output.decode('utf-8'):
        return True
    else:
        return False

def __is_sysv():
    return os.path.isfile('/etc/init.d/cron') and not os.path.islink('/etc/init.d/cron')

# NOTE: EXECUTION ORDER IS IMPORTANT!!!
KNOWN_INIT_SYSTEMS_CHECKS = OrderedDict()
KNOWN_INIT_SYSTEMS_CHECKS['upstart'] = __is_upstart
KNOWN_INIT_SYSTEMS_CHECKS['systemd'] = __is_systemd
KNOWN_INIT_SYSTEMS_CHECKS['sysv'] = __is_sysv

def find_init_system():
    # For each init system, execute checker function
    for init_sys_name, check_func in KNOWN_INIT_SYSTEMS_CHECKS.items():
        result = check_func()

        if result:
            return init_sys_name

    return 'unknown'
    
def is_supported_init_system(isys):
    return isys == 'systemd' or isys == 'sysv' or isys == 'upstart'

# A small test function
if __name__ == '__main__':
    print(find_init_system())
