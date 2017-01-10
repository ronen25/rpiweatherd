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

#include "http.h"

void http_cmd_param_free(http_cmd_param *ptr) {
	free(ptr->name);
	free(ptr->value);
}

void http_cmd_free(http_cmd *ptr) {
	/* Free each parameter */
	for (int i = 0; i < ptr->length; i++)
		http_cmd_param_free(&ptr->params[i]);

	/* Free array and pointer */
	if (ptr->params)
		free(ptr->params);

	free(ptr);
}

int http_cmd_param_count(http_cmd *cmd, const char *name) {
	int count = 0;

	for (int i = 0; i < cmd->length; i++)
		if (strcmp(cmd->params[i].name, name) == 0)
			count++;

	return count;
}

int break_request(const char *url, char *http_proto, char *args, char *commandstr) {
	int scanflag;
	char all_line[HTTP_COMMAND_MAX_SIZE] = { 0 };
	char *saveptr, *ptr;

	/* Create local copy for strtok_r */
	strcpy(all_line, url);

	/* Break once - get request type */
	ptr = strtok_r(all_line, " ", &saveptr);
	if (!ptr)
		return HTTP_PARSER_ERROR_BAD_REQUEST_FORMAT;

	/* Break twice - get command */
	ptr = strtok_r(NULL, " /?", &saveptr);
	if (ptr)
		commandstr = strcpy(commandstr, ptr);
	else
		return HTTP_PARSER_ERROR_BAD_REQUEST_FORMAT;

	/* Check if the command has any arguments.
	 * We can determine this is the args or http_proto
	 * by comparing it to the server's supported protocols */
	ptr = strtok_r(NULL, " ", &saveptr);
	if (!ptr)
		return HTTP_PARSER_ERROR_BAD_REQUEST_FORMAT;
	else {
		if (strcmp(ptr, "HTTP/1.0") == 0 || strcmp(ptr, "HTTP/1.1") == 0) {
			http_proto = strcpy(http_proto, ptr);
			return HTTP_PARSER_ERROR_SUCCESS;
		}
		else
			args = strcpy(args, ptr);
	}

	/* Check protocol */
	ptr = strtok_r(NULL, " ", &saveptr);
	if (!ptr)
		return HTTP_PARSER_ERROR_BAD_REQUEST_FORMAT;
	else
		http_proto = strcpy(http_proto, ptr);

	return HTTP_PARSER_ERROR_SUCCESS;
}

http_cmd *parse_http_request(const char *request, int *response) {
	char *line, *part, *saveptr;
	char http_proto[HTTP_PROTO_MAX_SIZE] = { 0 }, 
		 args[HTTP_ARGS_MAX_SIZE] = { 0 },
		 commandstr[HTTP_COMMAND_MAX_SIZE] = { 0 };
	http_cmd *cmd = NULL;
	int actual_count = 0, bflag = 0, i;
	size_t cmd_iter = 0;

	/* Get first line, which is the request line itself. */
	line = rpiwd_getline(request, "\r\n");
	if (!line)
		return NULL; /* Bad request / bad request format*/

	/* Parse the entire line */
	bflag = break_request(line, http_proto, args, commandstr);
	if (bflag < 0) {
		*response = bflag;
		return NULL; /* Bad request */
	}

	/* Check protocol */
	if (strcmp(http_proto, "HTTP/1.1") != 0 && strcmp(http_proto, "HTTP/1.0") != 0) {
		*response = HTTP_PARSER_ERROR_WRONG_PROTOCOL;
		return NULL; /* Wrong protocol */
	}

	/* Allocate cmd */
	cmd = malloc(sizeof(http_cmd));
	if (!cmd) {
		*response = HTTP_PARSER_ERROR_NO_MEMORY;
		return NULL;
	}

	/* Copy command */
	cmd->cmdname = strdup(commandstr);
	if (!cmd->cmdname) {
		*response = HTTP_PARSER_ERROR_NO_MEMORY;
		return NULL;
	}

	/* Now parse all arguments. First determine count.
	 * Note that if there are more AND symbols then actual arguments,
	 * this will be corrected later using a realloc(). */
	for (i = 0; args[i]; args[i] == '&' ? actual_count++ : 0, i++);

	/* Allocate params array */
	cmd->params = malloc(sizeof(http_cmd_param) * (actual_count + 1));
	if (!cmd->params) {
		*response = HTTP_PARSER_ERROR_NO_MEMORY;
		free(cmd);
		return NULL;
	}

	/* If actual_count == 0, might be a single parameter, or no parameters at all. */
	if (actual_count == 0) {
		cmd->params[0] = parse_http_param(args);

		/* If name is something, then there is a single parameter */
		if (cmd->params[0].name)
			cmd->length = 1;
		else {
			/* No parameters for this command */
			cmd->length = 0;
		}
	}
	else {
		/* Now actually parse arguments */
		part = strtok_r(args, "&", &saveptr);
		do {
			cmd->params[cmd_iter] = parse_http_param(part);
			cmd_iter++;
		} while ((part = strtok_r(NULL, "&", &saveptr)) != NULL);

		/* Check if re-allocation is needed (e.g. More & than actual args).
		 * Also remember to reserve an extra entry for the NULL entry */
		if ((cmd_iter - 1) != actual_count) {
			cmd->length = cmd_iter - 1;
			cmd->params = realloc(cmd->params, sizeof(http_cmd_param) * cmd->length);
			if (!cmd->params) {
				*response = HTTP_PARSER_ERROR_NO_MEMORY;
				http_cmd_free(cmd);
				return NULL;
			}
		}
		else
			cmd->length = cmd_iter;
	}

	/* Check for duplicate arguments in parametrs */
	for (i = 0; i < cmd->length; i++) {
		if (http_cmd_param_count(cmd, cmd->params[i].name) > 1) {
			http_cmd_free(cmd);
			cmd = NULL;
			*response = HTTP_PARSER_ERROR_DUPLICATE_PARAMS;

			break;
		}
	}

	return cmd;
}

http_cmd_param parse_http_param(const char *paramstr) {
	http_cmd_param param;
	char *part, *saveptr;
	char local[strlen(paramstr) + 1];

	strcpy(local, paramstr);

	/* Split by the '=' sign, if any. */
	part = strtok_r(local, "=", &saveptr);
	if (part)
		param.name = strdup(part);
	else
		param.name = NULL;

	part = strtok_r(NULL, "=", &saveptr);
	if (part)
		param.value = strdup(part);
	else
		param.value = NULL;

	return param;
}

char *make_response(int code, const char *data) {
	char resp_string[HTTP_RESPONSE_HEADER_SIZE / 2], 
		 content_length[HTTP_RESPONSE_HEADER_SIZE / 2],
		 date_buffer[26]; /* See ctime(2) */
	size_t datalen = data ? strlen(data): 0;
	time_t current_time;
	char *response = calloc(sizeof(char), HTTP_RESPONSE_HEADER_SIZE + datalen);
	if (!response)
		return NULL;

	/* Check response code and build response accordingly */
	sprintf(resp_string, http_code_str(code));

	/* Print content length if needed */
	sprintf(content_length, datalen ? "Content-Length: %d\r\n" : "", 
			datalen ? datalen : 0);

	/* Generate date and properly concatenate the result string */
	current_time = time(NULL);
	ctime_r(&current_time, date_buffer);
	date_buffer[strlen(date_buffer) - 1] = '\0';

	/* Print the actual content of the response */
	sprintf(response, HTTP_RESPONSE_TEMPLATE,
			resp_string,										/* Response string */
			date_buffer,										/* Date */
			RPIWEATHERD_FULL_SERVER_ID,							/* Server ID */
			content_length ? content_length : "",				/* Content-Length */
			data ? data : ""									/* Data if needed */
		   );

	return response;
}

ssize_t send_response(int sockfd, int code, const char *data) {
	ssize_t flag;
	char *response = make_response(code, data);
	if (!response)
		return -1;

	/* Respond to client */
	flag = write(sockfd, response, strlen(response));

	/* Free response string */
	free(response);

	return flag;
}

ssize_t send_http_error_response(int sockfd, int httpcode, int errcode, const char *err) {
	ssize_t flag;
	char *serialized = NULL, *response = NULL;
	JSON_Value *rootval = json_value_init_object();
	JSON_Object *mainobject = json_value_get_object(rootval);

	/* Build JSON value */
	json_object_set_number(mainobject, "length", 0);
	json_object_set_number(mainobject, "errcode", errcode);
	json_object_set_string(mainobject, "errmsg", err);

	serialized = json_serialize_to_string(rootval);
	if (!serialized) {
		json_value_free(rootval);
		return -1;
	}

	/* Make HTTP response */
	response = make_response(httpcode, serialized);

	/* Respond to client */
	flag = write(sockfd, response, strlen(response));

	/* Free all buffers */
	json_free_serialized_string(serialized);
	json_value_free(rootval);
	free(response);

	return flag;
}

http_cmd *read_and_parse_response(int sockfd, int *response, int *is_eof) {
	ssize_t flag = 0;
	char buffer[RESPONSE_BUFFER_SIZE];

	/* Read from socket */
	flag = read(sockfd, buffer, RESPONSE_BUFFER_SIZE);
	if (flag == -1)
		return NULL;
	else if (flag == 0) { /* Socket EOF */
		*is_eof = 1;
		return NULL;
	}

	/* Parse */
	return parse_http_request(buffer, response);
}

void end_response(int sockfd, http_cmd *cmd) {
	/* Close socket */
	close(sockfd);

	/* Remove command */
	if (cmd)
		http_cmd_free(cmd);
}

const char *http_code_str(int code) {
	switch (code) {
		case HTTP_CODE_OK: /* 200 OK */
			return "200 OK";
		case HTTP_CODE_NO_CONTENT: /* 204 No Content */
			return "204 No Content";
		case HTTP_CODE_REQUEST_BAD_REQUEST: /* 400 Bad Request */
			return "400 Bad Request";
		case HTTP_CODE_FORBIDDEN: /* 403 Forbidden */
			return "403 Forbidden";
		case HTTP_CODE_NOT_FOUND: /* 404 Not Found */
			return "404 Not Found";
		case HTTP_CODE_TOO_MANY_REQUESTS: /* 429 Too Many Requests */
			return "429 Too Many Requests";
		case HTTP_CODE_REQUEST_TIMEOUT: /* 408 Request Timeout */
			return "408 Request Timeout";
		case HTTP_CODE_PAYLOAD_TOO_LARGE: /* 413 Payload Too Large */
			return "413 Payload Too Large";
		case HTTP_CODE_URI_TOO_LONG: /* 414 URI Too Long */
			return "414 URI Too Long";
		case HTTP_CODE_INTERNAL_SERVER_ERROR: /* 500 Internal Server Error */
			return "500 Internal Server Error";
		case HTTP_CODE_VERSION_NOT_SUPPORTED: /* 505 HTTP Version Not Supported */
			return "505 HTTP Version Not Supported";
		case HTTP_CODE_INSUFFICIENT_STORAGE: /* 507 Insufficient Storage */
			return "507 Insufficient Storage";
	}

	/* If control reaches here = Unknown code */
	return NULL;
}

const char *http_parser_strerror(int errcode) {
	switch (errcode) {
		case HTTP_PARSER_ERROR_SUCCESS:
			return "Success";
		case HTTP_PARSER_ERROR_UNKNOWN_PARAM:
			return "Unknown parameter";
		case HTTP_PARSER_ERROR_REQUEST_TOO_LONG:
			return "HTTP request line is too long";
		case HTTP_PARSER_ERROR_PARAMS_REQUIRED:
			return "HTTP command requires parameters";
		case HTTP_PARSER_ERROR_BAD_REQUEST_FORMAT:
			return "HTTP request is malformed.";
		case HTTP_PARSER_ERROR_WRONG_PROTOCOL:
			return "rpiweatherd only supports HTTP/1.0 or HTTP/1.1";
		case HTTP_PARSER_ERROR_DUPLICATE_PARAMS:
			return "Duplicate parameters in HTTP query string";
	}

	return "Unknown error";
}

