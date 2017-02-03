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

#include "datastructures.h"

/* Allocating/freeing entry lists */
entry *entry_alloc(void) {
	entry *ptr = malloc(sizeof(entry));
	if (!ptr)
		return NULL;

	ptr->record_date = ptr->location = ptr->device_name = NULL;

	return ptr;
}

void entry_free(entry *ent) {
	/* Free all strings */
	free(ent->record_date);
	free(ent->location);
	free(ent->device_name);
}

void entry_ptr_free(entry *ent) {
	entry_free(ent);
	free(ent);
}

entrylist *entrylist_alloc(size_t size) {
	entrylist *entp = NULL;

	/* Check size */
	if (size < 0)
		return NULL;

	/* Allocate list pointer */
	entp = malloc(sizeof(entrylist));
	if (!entp)
		return NULL;

	/* Allocate array */
	entp->entries = malloc(sizeof(entry) * size);
	if (!entp->entries) {
		free(entp);
		return NULL;
	}

	/* Put size */
	entp->length = size;

	return entp;
}

void entrylist_free(entrylist *listptr) {
	int i = 0;

	/* Free every entry's stuff */
	for (i = 0; i < listptr->length; i++)
		entry_free(&(listptr->entries[i]));

	/* Free entry array and entry list pointer */
	free(listptr->entries);
	free(listptr);
}

key_value_pair key_value_pair_emplace(const char *key, const char *value) {
	key_value_pair pair;

	pair.key = key ? strdup(key) : NULL;
	pair.value = value ? strdup(value) : NULL;

	return pair;
}

void key_value_pair_free(key_value_pair *pair) {
	if (pair->key)
		free(pair->key);

	if (pair->value)
		free(pair->value);
}

key_value_list *key_value_list_alloc(size_t max_items) {
	/* Allocate the list itself */
	key_value_list *list = malloc(sizeof(key_value_list));
	if (!list)
		return NULL;

	/* Allocate array */
	list->pairs = malloc(sizeof(key_value_pair) * max_items);
	if (!list->pairs) {
		free(list);
		return NULL;
	}

	/* Set size */
	list->length = 0;
	list->capacity = max_items;

	return list;
}

void key_value_list_free(key_value_list *list) {
	/* Free all arguments */
	for (int i = 0; i < list->length; i++)
		key_value_pair_free(&(list->pairs[i]));

	/* Free the list */
	free(list);
}

int key_value_list_emplace(key_value_list *list, const char *key, const char *value) {
	/* Check if there is room */
	if (list->length + 1 > list->capacity)
		return -1;

	/* Add it */
	list->pairs[list->length] = key_value_pair_emplace(key, value);
	list->length++;

	return 1;
}

/* JSON */
JSON_Value *entry_to_json_value(entry *ent, char *unitstr) {
	char refkey_str[JSON_SERIALIZER_TEMP_ID_BUFFER_SIZE] = { 0 };
	JSON_Value *rootval = json_value_init_object();
	JSON_Object *mainobject = json_value_get_object(rootval);

	/* Check argument */
	if (!ent) {
		json_value_free(rootval);
		return NULL;
	}

	/* First element, simply enough, is the amount of entries  */
    json_object_set_number(mainobject, "length", 1);

    /* Append units */
    append_units(mainobject, unitstr);

	/* Put error code and message */
	json_object_set_number(mainobject, "errcode", 0);
	json_object_set_string(mainobject, "errmsg", "");

	/* Set main ID */
	json_object_set_number(mainobject, "id", (double)ent->id);

	/* Print id */
	sprintf(refkey_str, "%d.%s", ent->id, "id");
	json_object_dotset_number(mainobject, refkey_str, ent->id);

	/* Print record date */
	sprintf(refkey_str, "%d.%s", ent->id, "record_date");
	json_object_dotset_string(mainobject, refkey_str, ent->record_date);

	/* Print temperature */
	sprintf(refkey_str, "%d.%s", ent->id, "temperature");
	json_object_dotset_number(mainobject, refkey_str, ent->temperature);

	/* Print humidity */
	sprintf(refkey_str, "%d.%s", ent->id, "humidity");
	json_object_dotset_number(mainobject, refkey_str, ent->humidity);

	/* Print location */
	sprintf(refkey_str, "%d.%s", ent->id, "location");
	json_object_dotset_string(mainobject, refkey_str, ent->location);

	/* Print device name */
	sprintf(refkey_str, "%d.%s", ent->id, "device_name");
	json_object_dotset_string(mainobject, refkey_str, ent->device_name);

	return rootval;
}

/* NOTE: This doesn't use entry_to_json_value because it is
 * slightly faster when doing it the bad old way... */
JSON_Value *entrylist_to_json_value(entrylist **list, char *unitstr) {
	int refkey;
	char refkey_str[JSON_SERIALIZER_TEMP_ID_BUFFER_SIZE] = { 0 };
	entrylist *listptr = *list;
	JSON_Value *rootval = json_value_init_object();
	JSON_Object *mainobject = json_value_get_object(rootval);

	/* First element, simply enough, is the amount of entries  */
    json_object_set_number(mainobject, "length", (double)listptr->length);

    /* Append units */
    append_units(mainobject, unitstr);

	/* Put error code and message */
	json_object_set_number(mainobject, "errcode", 0);
	json_object_set_string(mainobject, "errmsg", "");

	/* Iterate through all elements and JSON-ify them */
	for (int i = 0; i < listptr->length; i++) {
		refkey = listptr->entries[i].id;

		/* Print ID */
		sprintf(refkey_str, "results.%d.%s", refkey, "id");
		json_object_dotset_number(mainobject, refkey_str, (double)refkey);

		/* Print record date */
		sprintf(refkey_str, "results.%d.%s", refkey, "record_date");
		json_object_dotset_string(mainobject, refkey_str, 
				listptr->entries[i].record_date);

		/* Print temperature */
		sprintf(refkey_str, "results.%d.%s", refkey, "temperature");
		json_object_dotset_number(mainobject, refkey_str, 
				listptr->entries[i].temperature);

		/* Print humidity */
		sprintf(refkey_str, "results.%d.%s", refkey, "humidity");
		json_object_dotset_number(mainobject, refkey_str, 
				listptr->entries[i].humidity);

		/* Print location */
		sprintf(refkey_str, "results.%d.%s", refkey, "location");
		json_object_dotset_string(mainobject, refkey_str, 
				listptr->entries[i].location);

		/* Print device name */
		sprintf(refkey_str, "results.%d.%s", refkey, "device_name");
		json_object_dotset_string(mainobject, refkey_str, 
				listptr->entries[i].device_name);
	}

	return rootval;
}

JSON_Value *key_value_list_to_json_value(key_value_list **list) {
	JSON_Value *rootval = json_value_init_object();
	JSON_Object *mainobject = json_value_get_object(rootval);
	char refkey_str[JSON_SERIALIZER_TEMP_ID_BUFFER_SIZE] = { 0 };
	key_value_list *listptr = *list;

	/* Put list length, errcode and errmsg */
	json_object_set_number(mainobject, "length", (double)listptr->length);
	json_object_set_number(mainobject, "errcode", 0);
	json_object_set_string(mainobject, "errmsg", "");

	/* Put all values in */
	for (int i = 0; i < listptr->length; i++) {
		/* Print refrence key */
		sprintf(refkey_str, "results.%s", listptr->pairs[i].key);
		json_object_dotset_string(mainobject, refkey_str, listptr->pairs[i].value);
	}

	/* Return */
	return rootval;
}


void append_units(JSON_Object *jobj, char *unitstr) {
    /* Quickly convert the unit characters to a string */
    char unitbuffer[4];
    unitbuffer[0] = unitstr[RPIWD_MEASURE_TEMPERATURE];
    unitbuffer[1] = unitbuffer[3] = '\0';
    unitbuffer[2] = RPIWD_DEFAULT_HUMID_UNIT;

    /* Apply to JSON object */
    json_object_dotset_string(jobj, "units.tempunit", unitbuffer);
    json_object_dotset_string(jobj, "units.humidunit", unitbuffer + 2);
}
