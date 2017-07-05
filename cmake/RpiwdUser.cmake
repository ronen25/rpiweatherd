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

function(createRpiwdUser)
    # Execute the "useradd" process to add a new system user
    execute_process(
        COMMAND useradd -r -s /usr/bin/nologin rpiweatherd
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE RPIWD_USER_ADD_RESULT
    )

    # Check the result.
    # If the user has been added successfully (0), we're ok
    if(${RPIWD_USER_ADD_RESULT} EQUAL 0)
        return(1)
    elseif(${RPIWD_USER_ADD_RESULT} EQUAL 9)
        # If the user exists, then we must delete it, and then execute this function again.
        # We do this just to make sure that we have created the user on our own terms.
        execute_process(
            COMMAND userdel rpiweatherd
            RESULT_VARIABLE RPIWD_USER_DEL_RESULT
        )

        # Check return value.
        if (${RPIWD_USER_DEL_RESULT} EQUAL 0)
            createRpiwdUser()
        else()
            message(FATAL_ERROR "Error: could not remove existing 'rpiweatherd' user.
                                 Please remove it manually!")
        endif()
    else()
        message(FATAL_ERROR "Error: Could not create the 'rpiweatherd' user.
                             Maybe you lack permissions to do so?")
    endif()

    return(0)
endfunction()