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

#ifndef RPIWD_MEASUREVALS_H
#define RPIWD_MEASUREVALS_H

/* Constants */
#define RPIWD_MAX_MEASUREMENTS              8

/* Measurement Mapping */
#define RPIWD_MEASURE_TEMPERATURE           0
#define RPIWD_MEASURE_HUMIDITY              1

/* Temperature constants */
#define RPIWD_TEMPERATURE_CELSIUS           'c'
#define RPIWD_TEMPERATURE_FARENHEIT         'f'

/* Defaults */
#define RPIWD_DEFAULT_TEMP_UNIT             RPIWD_TEMPERATURE_FARENHEIT
#define RPIWD_DEFAULT_HUMID_UNIT            '%'

#endif /* RPIWD_MEASUREVALS_H */
