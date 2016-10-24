# 
# rpiweatherd - A weather daemon for the Raspberry Pi that stores sensor data.
# Copyright (C) 2016 Ronen Lapushner
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

cmake_minimum_required(VERSION 2.6)
project(rpiweatherd C)

# Set version
set(RPIWEATHERD_VERSION_MAJOR 1)
set(RPIWEATHERD_VERSION_MINOR 0)

# Set the server id by running 'lsb_release -is'
execute_process(COMMAND "lsb_release" "-is" OUTPUT_VARIABLE DISTRO_NAME)
if (DISTRO_NAME STREQUAL "")
	message(WARNING "Could not execute 'lsb_release -is'; default server ID used.")
	set(RPIWEATHERD_FULL_SERVER_ID "\"rpiweatherd v.${RPIWEATHERD_VERSION_MAJOR}.${RPIWEATHERD_VERSION_MINOR}\"")
else()
	string(REPLACE "\n" "" DISTRO_NAME "${DISTRO_NAME}")
	set(RPIWEATHERD_FULL_SERVER_ID "\"rpiweatherd v.${RPIWEATHERD_VERSION_MAJOR}.${RPIWEATHERD_VERSION_MINOR} (${DISTRO_NAME})\"")
endif()

# Set policies
cmake_policy(SET CMP0014 OLD)

# Configuration file
configure_file (
	"${PROJECT_SOURCE_DIR}/rpiweatherd_config.h.in"
	"${PROJECT_BINARY_DIR}/rpiweatherd_config.h"
)

# Uninstall target
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Include directories
include_directories(include)
include_directories("${PROJECT_BINARY_DIR}")
include_directories(deps/inih)
include_directories(devices)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(devices)
add_subdirectory(deps/inih)

# Find functions
include(CheckFunctionExists)
check_function_exists(daemon HAVE_DAEMON)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(getopt HAVE_GETOPT)
check_function_exists(flock  HAVE_FLOCK)
check_function_exists(nanosleep HAVE_NANOSLEEP)
check_function_exists(strptime HAVE_STRPTIME)

