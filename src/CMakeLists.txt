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

# Compile-time definitions
add_definitions(-D_BSD_SOURCE -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE -D_XOPEN_SOURCE)

# Check for all device sources
file(GLOB SOURCES *.c)
file(GLOB DEVICES ${PROJECT_SOURCE_DIR}/devices/*.c)
file(GLOB DEPS ${PROJECT_SOURCE_DIR}/deps/*.c)

## Find include paths
find_library(WIRINGPI_LIBS NAMES wiringPi)
find_path(WIRINGPI_INCLUDES NAMES wiringPi.h)
find_library(SQLITE3_LIBS NAMES sqlite3)
find_path(SQLITE3_INCLUDES NAMES sqlite3.h)
find_package(Threads)

# Find include directories
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(wiringPi DEFAULT_MSG WIRINGPI_LIBS WIRINGPI_INCLUDES)
find_package_handle_standard_args(sqlite3 DEFAULT_MSG SQLITE3_LIBS SQLITE3_INCLUDES)

include_directories(${WIRINGPI_INCLUDES})
include_directories(${SQLITE3_INCLUDES})
include_directories(${PROJECT_SOURCE_DIR}/deps/)

## Additional compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")

# DEBUG flags
set(CMAKE_C_FLAGS_DEBUG "-DRPIWD_DEBUG -g")

# Add executable
add_executable(rpiweatherd ${SOURCES} ${DEVICES} ${DEPS})
target_link_libraries(rpiweatherd ${WIRINGPI_LIBS} ${SQLITE3_LIBS})
target_link_libraries(rpiweatherd ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(rpiweatherd rt)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

# Install target and run script to determine init system
install(TARGETS rpiweatherd DESTINATION bin)
install(SCRIPT "${CMAKE_SOURCE_DIR}/PostInstall.cmake")
