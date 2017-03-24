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

cmake_minimum_required(VERSION 3.0)
project(rpiweatherd C)

# Find git information
# Get the current working branch
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set version string
if(NOT GIT_BRANCH MATCHES "master")
    # Set versions
    set(RPIWEATHERD_VERSION "\"${GIT_BRANCH}-${GIT_COMMIT_HASH}\"")
    set(RPIWEATHERD_VERSION_RAW "${GIT_BRANCH}-${GIT_COMMIT_HASH}")
else()
    # Read version information from file
    file(READ version RPIWEATHERD_VERSION)
    string(REGEX REPLACE "\n$" "" RPIWEATHERD_VERSION "${RPIWEATHERD_VERSION}")

    # Set versions
    set(RPIWEATHERD_VERSION_RAW ${RPIWEATHERD_VERSION})
    set(RPIWEATHERD_VERSION "\"${RPIWEATHERD_VERSION}\"")
endif()

# Set major, minor and patch versions.
# This'll be used for packaging.
string(REPLACE "." ";" VERSION_LIST ${RPIWEATHERD_VERSION_RAW})

# Get list length
list(LENGTH VERSION_LIST VERSION_LIST_LENGTH)

# Get major and minor versions
list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)

# If patch version exists, get it as well.
if (VERSION_LIST_LENGTH GREATER 2)
    string(SUBSTRING "${VERSION_LIST}" 4 1 VERSION_PATCH)
else()
    set(VERSION_PATCH 0)
endif()

# Set build date
string(TIMESTAMP RPIWEATHERD_BUILD_DATE)
if (RPIWEATHERD_BUILD_DATE EQUAL "")
    set(RPIWEATHERD_BUILD_DATE "")
endif()

# Set the server id by running 'lsb_release -is'
set(RPIWEATHERD_FULL_SERVER_ID "\"rpiweatherd/${RPIWEATHERD_VERSION_RAW}\"")

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
include_directories(deps/)
include_directories(devices)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(include)
add_subdirectory(devices)
add_subdirectory(deps)

# Find functions
include(CheckFunctionExists)
check_function_exists(daemon HAVE_DAEMON)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(getopt HAVE_GETOPT)
check_function_exists(flock  HAVE_FLOCK)
check_function_exists(nanosleep HAVE_NANOSLEEP)
check_function_exists(strptime HAVE_STRPTIME)
check_function_exists(getline HAVE_GETLINE)
check_function_exists(vsyslog HAVE_VSYSLOG)
check_function_exists(fork HAVE_FORK)
check_function_exists(execl HAVE_EXECL)

# CPack configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_DESCRIPTION "rpiweatherd - A small daemon that records weather information.")
set(CPACK_PACKAGE_VENDOR "Ronen Lapushner")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ronen Lapushner")
set(CPACK_PACKAGE_CONTACT "ronenk17@gmail.com")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "rpiweatherd-${RPIWEATHERD_VERSION_RAW}_${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "python3 (>= 3.2.0), git, libsqlite3-dev (>= 3.5), wiringpi (>= 1.1)")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
include(CPack)
