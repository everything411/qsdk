/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>

#include <nss_ppenl_base.h>
#include "ppecfg_hlos.h"
#include "ppecfg_param.h"
#include "ppecfg_acl_json_parser.h"
#include "ppecfg_policer_json_parser.h"
#include "ppecfg_json_parser.h"

#define RULES "rules"
#define PPECFG_JSON_PARSER_CONFIG_DEF_PATH_LENGTH 100

/*
 * Default path for json config file
 */
#define PPECFG_JSON_PARSER_PATH "/etc/ppecfg/ppecfg_acl_config.json"

struct json_object *json_obj;
struct ppecfg_param ppecfg_json_parser_param[PPECFG_JSON_PARSER_PARAM_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_JSON_PARSER_CONFIG_PATH, "path="),
};

/*
 * ppecfg_json_parser()
 * 	Parses the contents of json object from the configuration file
 */
static int ppecfg_json_parser(const char *file_path)
{
	struct stat filestat;
	struct json_object *include_file_obj;
	struct json_object *jobj;
	struct json_object *rule_list;
	struct json_object *current_rule;
	struct json_object *rule_obj;
	FILE *fp;
	char *json_data = NULL;
	int error = 0;

	fp = fopen(file_path, "rb");
	if (!fp) {
		ppecfg_log_error("Failed to open file\n");
		return -EACCES;
	}

	if (stat(file_path, &filestat) != 0) {
		ppecfg_log_error("File not found\n");
		return -ENOENT;
	}

	int file_length = filestat.st_size;
	if (file_length) {
		json_data = malloc(file_length);
	}

	if (json_data == NULL) {
		ppecfg_log_error("Cannot allocate memory!!");
		fclose(fp);
		return -ENOMEM;
	}

	if (fread(json_data, 1, file_length, fp) != file_length) {
		ppecfg_log_error("Error reading File!!\n");
		free(json_data);
		fclose(fp);
		return -EINVAL;
	}

	fclose(fp);

	jobj = json_tokener_parse(json_data);
	include_file_obj = ppecfg_get_json_object(jobj, "include");

	if (include_file_obj != NULL) {
		char *new_file_path = (char *)json_object_get_string(include_file_obj);
		error = ppecfg_json_parser((const char *)new_file_path);
		if (error) {
			ppecfg_log_error("Failed to get policer config\n");
			return error;
		}
	}

	rule_list = ppecfg_get_json_object(jobj, RULES);
	if (rule_list == NULL) {
		ppecfg_log_error("Error: %s info not present!\n", RULES);
		free(json_data);
		json_object_put(jobj);
		return -EINVAL;
	}

	int rule_count = json_object_array_length(rule_list);

	for (int i = 0; i < rule_count; i++) {
		current_rule = json_object_array_get_idx(rule_list, i);
		rule_obj = ppecfg_get_json_object(current_rule, "policer");
		if (rule_obj != NULL) {
			error = ppecfg_policer_json_rule_add(rule_obj);
			if (error) {
				ppecfg_log_error("Failed to get Policer rule\n");
				break;
			}
		} else {
			rule_obj = ppecfg_get_json_object(current_rule, "acl");
			if (rule_obj != NULL) {
				error = ppecfg_acl_json_rule_add(rule_obj);
				if (error) {
					ppecfg_log_error("Failed to get ACL rule\n");
					break;
				}
			} else {
				ppecfg_log_warn("Enter a valid type of rule\n");
				continue;
			}
		}
	}

	return error;
}

/*
 * ppecfg_json_acl_rule_flush()
 * 	Function to flush all acl rules
 */
static int ppecfg_json_acl_rule_flush(void)
{
	struct nss_ppenl_acl_rule nl_acl_msg = {{0}};
	int error;

	nss_ppenl_acl_init_rule(&nl_acl_msg, NSS_PPE_ACL_FLUSH_RULE_MSG);
	error = nss_ppenl_acl_rule_flush(&nl_acl_msg);

	if (error) {
		ppecfg_log_warn("Flush ACL rule failed!\n");
	}

	return error;
}

/*
 * ppecfg_json_policer_rule_flush()
 * 	Function to flush all policer rules
 */
static int ppecfg_json_policer_rule_flush(void)
{
	struct nss_ppenl_policer_rule nl_policer_msg = {{0}};
	int error;

	nss_ppenl_policer_init_rule(&nl_policer_msg, NSS_PPE_POLICER_FLUSH_RULE_MSG);
	error = nss_ppenl_policer_rule_flush(&nl_policer_msg);

	if (error) {
		ppecfg_log_warn("Flush Policer rule failed!\n");
	}

	return error;
}

/*
 * ppecfg_json_parser_handler()
 * 	Handles JSON config provided by user
 */
int ppecfg_json_parser_handler(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	int error;
	char path[PPECFG_JSON_PARSER_CONFIG_DEF_PATH_LENGTH];
	struct ppecfg_param *sub_params;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	error = ppecfg_json_acl_rule_flush();
	if (error) {
		ppecfg_log_error("Flush for ACL failed");
	}

	error = ppecfg_json_policer_rule_flush();
	if (error) {
		ppecfg_log_error("Flush for Policer failed");
	}

	/*
	 * Iterate through the param table to identify the matched arguments and 
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_info("default configuration\n");
		memcpy(path, PPECFG_JSON_PARSER_PATH, strlen(PPECFG_JSON_PARSER_PATH));
	} else {
		sub_params = &param->sub_params[PPECFG_JSON_PARSER_CONFIG_PATH];
		memcpy(path, sub_params->data, strlen(sub_params->data));
		ppecfg_log_info("New path for configuration: %s\n", path);
	}

	error = ppecfg_json_parser((const char *)path);
	return error;
}
