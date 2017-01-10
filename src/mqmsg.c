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

#include "mqmsg.h"

rpiwd_mqmsg *rpiwd_mqmsg_alloc(void) {
	rpiwd_mqmsg *ret = malloc(sizeof(rpiwd_mqmsg));
	if (!ret)
		return NULL;

	ret->fcountq = ret->fselectq = NULL;

	return ret;
}

void rpiwd_mqmsg_free(rpiwd_mqmsg *ptr, int free_ptr) {
	/* Free both queries if needed */
	if (free_ptr)
		free(ptr);
}

