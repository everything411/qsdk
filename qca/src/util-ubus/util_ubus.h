/******************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 ******************************************************************/

#ifndef __UTIL_UBUS_H__
#define __UTIL_UBUS_H__

#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include <libubus.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>

static inline char *
util_purename(char *path)
{
	char *delim = rindex(path, '/');
	return delim ? (delim + 1) : path;
}

#define dbprintf(fmt, args...)		do { fprintf(stderr,"%s:%d: %s(): " fmt, util_purename(__FILE__), __LINE__, __FUNCTION__, ##args); } while (0)

#define UTIL_UBUS_DEBUG_DEFAULT		0
#define UBUS_INVOKE_TIMEOUT_DEFAULT	30
#define HAS_MAIN			1
#define BUF_LINE_LEN			1024	//ex: wireless.@wifi-device   wireless.@wifi-iface    device
#define TOKEN_LEN			256	//ex: @wifi-device

#define UTIL_UBUS_OK_ENTRY_FOUND			1001

#define UTIL_UBUS_ERROR_NULL_POINTER			-1001
#define UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL		-1002
#define UTIL_UBUS_ERROR_ENTRY_NOT_FOUND			-1003
#define UTIL_UBUS_ERROR_BLOB_MSG_NULL			-1004
#define UTIL_UBUS_ERROR_BLOB_MSG_PARSE			-1005
#define UTIL_UBUS_ERROR_BLOB_ATTR_NOT_FOUND		-1006
#define UTIL_UBUS_ERROR_BLOB_ATTR_PARSE			-1007
#define UTIL_UBUS_ERROR_BLOB_MSG_TYPE_UNSUPPORTED	-1008

#define UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_NAME	-1101
#define UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_VALUE	-1102
#define UTIL_ERROR_BUFFER_FOR_RETURN_IS_TOO_SMALL	-1103

#define UTIL_SECTION_RELATION_CONFIGURE			"/etc/section_relation.cfg"

/* util_ubus.c */
int util_ubus_client_init(void);
void util_ubus_client_finish(void);
char *util_ubus_error_str(int error);
char *util_ubus_blogmsg_type_str(unsigned int type);
int util_ubus_session_login(char *username, char *password, unsigned int session_timeout);
int util_ubus_session_validate(unsigned int *session_expire);
int util_ubus_session_destroy(void);
int util_ubus_uci_get(char *config, char *section, char *option, char *value, unsigned int value_size);
int util_ubus_uci_set(char *config, char *section, char *option, char *value);
int util_ubus_uci_set_list(char *config, char *section, char *option, char *array[], int array_size);
int util_ubus_uci_add_section(char *config, char *type, char *name);
int util_ubus_uci_delete(char *config, char *type, char *section, char *option);
int util_ubus_uci_revert(char *config);
int util_ubus_uci_commit(char *config);
int util_ubus_uci_apply(int rollback_enable, unsigned int auto_rollback_timeout);
int util_ubus_uci_confirm(void);
int util_ubus_uci_rollback(void);
int util_ubus_uci_dump_changes(char *config);
int find_parent_by_section_type(char *section_relation_file, char *child_config, char *child_section_type,
				int child_section_index, char *parent_config, char *parent_section_type);
int find_child_by_section_type(char *section_relation_file, char *child_config, char *child_section_type,
			       int *found_child_section_index, int child_section_index_size, char *parent_config,
			       char *parent_section_type, int parent_section_index);

#endif
