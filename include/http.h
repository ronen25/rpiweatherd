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


#ifndef RPIWD_HTTP_H
#define RPIWD_HTTP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <parson.h>

#include "util.h"
#include "rpiweatherd_config.h"

/* Macro to string helper macros */
#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

/* Various lengths */
#define HTTP_RESPONSE_TEMPLATE		"HTTP/1.1 %s\r\n" \
									"Date: %s\r\nServer: %s\r\n" \
									"Cache-control: no-store\r\n" \
									"Content-Type: text/html\r\n%s\r\n" \
									"%s"
#define HTTP_RESPONSE_HEADER_SIZE	512
#define HTTP_COMMAND_MAX_SIZE		1024
#define HTTP_PROTO_MAX_SIZE			16
#define HTTP_ARGS_MAX_SIZE			512
#define RESPONSE_BUFFER_SIZE		4096
#define HTTP_MAX_RESPONSE_SIZE		16386

/* HTTP codes */
#define HTTP_CODE_OK					200
#define HTTP_CODE_NO_CONTENT			204
#define HTTP_CODE_NOT_FOUND				404
#define HTTP_CODE_REQUEST_TIMEOUT 		408
#define HTTP_CODE_REQUEST_BAD_REQUEST	400
#define HTTP_CODE_FORBIDDEN				403
#define HTTP_CODE_TOO_MANY_REQUESTS		429
#define HTTP_CODE_PAYLOAD_TOO_LARGE		413
#define HTTP_CODE_URI_TOO_LONG			414
#define HTTP_CODE_INTERNAL_SERVER_ERROR	500
#define HTTP_CODE_VERSION_NOT_SUPPORTED	505
#define HTTP_CODE_INSUFFICIENT_STORAGE	507

/* HTTP parser return codes */
#define HTTP_PARSER_ERROR_SUCCESS				0
#define HTTP_PARSER_ERROR_UNKNOWN_PARAM			-1
#define HTTP_PARSER_ERROR_REQUEST_TOO_LONG 		-2
#define HTTP_PARSER_ERROR_PARAMS_REQUIRED		-3
#define HTTP_PARSER_ERROR_BAD_REQUEST_FORMAT 	-4
#define HTTP_PARSER_ERROR_WRONG_PROTOCOL 		-5
#define HTTP_PARSER_ERROR_NO_MEMORY				-6
#define HTTP_PARSER_ERROR_DUPLICATE_PARAMS		-7

typedef struct http_cmd_param_s {
	char *name, *value;
} http_cmd_param;

typedef struct http_cmd_s {
	char *cmdname;
	size_t length;
	http_cmd_param *params;
} http_cmd;

/* http_cmd_param methods */
void http_cmd_param_free(http_cmd_param *ptr);

/* http_cmd methods */
void http_cmd_free(http_cmd *ptr);
int http_cmd_param_count(http_cmd *cmd, const char *name);

/* HTTP Parsing */
http_cmd *parse_http_request(const char *request, int *response);
http_cmd_param parse_http_param(const char *paramstr);
int break_request(const char *url, char *http_proto, char *args, char *commandstr);

/* Sending/recieving */
char *make_response(int code, const char *data);
ssize_t send_response(int sockfd, int code, const char *data);
ssize_t send_http_error_response(int sockfd, int httpcode, int errcode, const char *err);
http_cmd *read_and_parse_response(int sockfd, int *response, int *is_eof);
void end_response(int sockfd, http_cmd *cmd);

/* Utility */
const char *http_code_str(int code);
const char *http_parser_strerror(int errcode);

#endif /* RPIWD_HTTP_H */
