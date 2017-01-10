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

#ifndef RPIWD_DATASTRUCTURES_H
#define RPIWD_DATASTRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <parson.h>

/* Constants */
#define JSON_SERIALIZER_TEMP_ID_BUFFER_SIZE	32

/* Entry structure */
typedef struct entry_s {
	int id;
	char *record_date;
	float temperature, humidity;
	char *location;
	char *device_name;
} entry;

/* Entry list structure */
typedef struct entrylist_s {
	size_t length;
	entry *entries;
} entrylist;

/* Generic key-value pair structure */
typedef struct key_value_pair_s {
	char *key, *value;
} key_value_pair;

/* Generic key-value list structure */
typedef struct key_value_list_s {
	size_t length, capacity;
	key_value_pair *pairs;
} key_value_list;

/* Allocating/freeing entry lists */
entry *entry_alloc(void);
void entry_free(entry *ent);
void entry_ptr_free(entry *ent);

entrylist *entrylist_alloc(size_t max_items);
void entrylist_free(entrylist *listptr);

/* Allocating/freeing generic key/value lists 
 * Note: key_value_list lacks removal methods because they are not really needed. */
key_value_pair key_value_pair_emplace(const char *key, const char *value);
void key_value_pair_free(key_value_pair *pair);

key_value_list *key_value_list_alloc(size_t size);
void key_value_list_free(key_value_list *list);

int key_value_list_emplace(key_value_list *list, const char *key, const char *value);

/* JSON */
JSON_Value *entry_to_json_value(entry *ent);
JSON_Value *entrylist_to_json_value(entrylist **list);
JSON_Value *key_value_list_to_json_value(key_value_list **list);

#endif /* RPIWD_DATASTRUCTURES_H */
