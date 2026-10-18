/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
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
#ifdef NSS_PPE_TUN_RPS_FEATURE
#include "ppecfg_tun_rps_json_parser.h"
#endif
#include "ppecfg_json_parser.h"
#define RULES "rules"
#define PPECFG_JSON_PARSER_CONFIG_DEF_PATH_LENGTH 100

/*
 * Default path for json config file
 */
#define PPECFG_JSON_PARSER_PATH "/etc/ppecfg/ppecfg_acl_config.json"

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
		fclose(fp);
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

	/*
	 * We can free the raw JSON data now as the object tree is built
	 */
	free(json_data);

	if (!jobj) {
		ppecfg_log_error("Failed to parse JSON\n");
		return -EINVAL;
	}

	include_file_obj = ppecfg_get_json_object(jobj, "include");

	if (include_file_obj != NULL) {
		char *new_file_path = (char *)json_object_get_string(include_file_obj);
		error = ppecfg_json_parser((const char *)new_file_path);
		if (error) {
			ppecfg_log_error("Failed to get policer config\n");
			json_object_put(jobj);
			return error;
		}
	}

	rule_list = ppecfg_get_json_object(jobj, RULES);
	if (rule_list == NULL) {
		ppecfg_log_error("Error: %s info not present!\n", RULES);
		json_object_put(jobj);
		return -EINVAL;
	}

	int rule_count = json_object_array_length(rule_list);

	for (int i = 0; i < rule_count; i++) {
		current_rule = json_object_array_get_idx(rule_list, i);

#ifdef NSS_PPE_TUN_RPS_FEATURE
		/*
		 * Try Tunnel RPS rule
		 */
		rule_obj = ppecfg_get_json_object(current_rule, "tun_rps");
		if (rule_obj) {
			error = ppecfg_tun_rps_json_rule_handler(rule_obj);
			if (error) {
				ppecfg_log_error("Failed to process tunnel RPS rule\n");
				break;
			}
			continue;
		}
#endif

		/*
		 * Try Policer rule
		 */
		rule_obj = ppecfg_get_json_object(current_rule, "policer");
		if (rule_obj) {
			error = ppecfg_policer_json_rule_add(rule_obj);
			if (error) {
				ppecfg_log_error("Failed to get Policer rule\n");
				break;
			}
			continue;
		}

		/*
		 * Try ACL rule
		 */
		rule_obj = ppecfg_get_json_object(current_rule, "acl");
		if (rule_obj) {
			error = ppecfg_acl_json_rule_add(rule_obj);
			if (error) {
				ppecfg_log_error("Failed to get ACL rule\n");
				break;
			}
			continue;
		}

		/*
		 * No valid rule type found notify and exit
		 */
		ppecfg_log_warn("Enter a valid type of rule\n");
	}

	/*
	 * Free the JSON object tree
	 */
	json_object_put(jobj);

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

	ppecfg_log_info("ppecfg_json_parser_handler called\n");

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

	ppecfg_log_info("JSON file path: %s\n", path);
	error = ppecfg_json_parser((const char *)path);
	return error;
}
