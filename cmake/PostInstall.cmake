# 
# rpiweatherd - A weather daemon for the Raspberry Pi that stores sensor data.
# Copyright (C) 2016-2017 Ronen Lapushner
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

include(RpiwdUser.cmake)

# Install init script or system unit files - depending on the init system present
execute_process(COMMAND "python3" "${CMAKE_CURRENT_LIST_DIR}/deps/FindInitSystem.py" 
    OUTPUT_VARIABLE INIT_SYS_NAME)
string(REPLACE "\n" "" INIT_SYS_NAME "${INIT_SYS_NAME}")

if (INIT_SYS_NAME STREQUAL "sysv" OR INIT_SYS_NAME STREQUAL "upstart")
    file(INSTALL ${CMAKE_CURRENT_LIST_DIR}/skel/initscripts/rpiweatherd DESTINATION /etc/init.d/rpiweatherd
        FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ)
elseif (INIT_SYS_NAME STREQUAL "systemd")
    file(INSTALL ${CMAKE_CURRENT_LIST_DIR}/skel/initscripts/rpiweatherd.service 
        DESTINATION /etc/systemd/system/)
else()
    message("Unknown/unsupported init system: ${INIT_SYS_NAME}")
    message("No daemon automation file will be installed!")
endif ()

# Create the 'rpiweatherd' user
createRpiwdUser()

# Install a blank configuration file
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/skel/rpiweatherd.conf"
     DESTINATION "/etc/rpiweatherd/")

# Install the trigger file
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/skel/rpiwd_triggers.conf"
     DESTINATION "/etc/rpiweatherd")
