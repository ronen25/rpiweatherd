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

#ifndef RPIWD_DEVRETCODES_H
#define RPIWD_DEVRETCODES_H

/* Constants */
#define RPIWD_MAX_MEASUREMENTS              8

/* Measurement Mapping */
#define RPIWD_MEASURE_TEMPERATURE           0
#define RPIWD_MEASURE_HUMIDITY              1

/* Device query return values */
#define RPIWD_DEVRETCODE_SUCCESS            0
#define RPIWD_DEVRETCODE_DEVICE_FAILURE     -1
#define RPIWD_DEVRETCODE_DATA_FAILURE	    -2
#define RPIWD_DEVRETCODE_GENERAL_FAILURE	-3
#define RPIWD_DEVRETCODE_MEMORY_ERROR	    -4
#define RPIWD_DEVRETCODE_UNIT_ERROR	        -5

#endif /* RPIWD_DEVRETCODES_H */
