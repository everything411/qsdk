/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <string.h>
#include <ctype.h>
#include <json-c/json.h>
#include "ppecfg_hlos.h"
#include "ppecfg_param.h"

/*
 * Object for json configuration files
 */
struct json_object *json_obj;

/*
 * ******************
 * Internal Functions
 * ******************
 */

/*
 * ppecfg_param_char2num()
 * 	converts a character to a hexadecimal number
 */
static inline uint8_t ppecfg_param_char2hex(char c)
{
	switch(c) {
	case '0' ... '9': /* numbers */
		return c - '0';

	case 'a' ... 'f': /* hex */
		return c - 'a' + 10;

	case 'A' ... 'F': /* hex */
		return c - 'A' + 10;

	default:
		return 0;
	}
}

/*
 * ******************
 * Exported Functions
 * ******************
 */
/*
 * ppecfg_param_search()
 * 	returns a parameter from the table that matches with the name
 */
static struct ppecfg_param *ppecfg_param_search(char *name, struct ppecfg_param param_tbl[], uint32_t num_params)
{
	struct ppecfg_param *param;

	if (!name || !param_tbl) {
		return NULL;
	}

	for (param = &param_tbl[0]; num_params; num_params--, param++) {

		/*
		 * match the param with the input name
		 */
		if (strncmp(name, param->name, param->len) == 0) {
			return param;
		}
	}

	return NULL;
}

/*
 * ppecfg_param_help()
 * 	iterate through the table and dump help
 */
int ppecfg_param_help(struct ppecfg_param *param)
{
	struct ppecfg_param *sub_param = NULL;
	int i;

	if (!param) {
		return 0;
	}

	/*
	 * walk the parameter table to print the options
	 */
	for (i = 0, sub_param = param->sub_params; i < param->num_params; i++, sub_param++) {
		ppecfg_log_arg_options((i + 1), sub_param);
	}

	return 0;
}

/*
 * ppecfg_param_iter_tbl()
 * 	iterate through the table and call the match callback for thee matching entries
 */
int ppecfg_param_iter_tbl(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	int error = -EINVAL;
	int i;

	if (!param || !match) {
		return error;
	}

	/*
	 * TODO: increment total by the number of params already processed
	 */

	/*
	 * walk the parameter table  to find the matching command
	 */
	for (i = 0; i < match->total; i++) {
		char *name = match->args[i];

		match->cur_param = param;

		/*
		 * search for the parameter in its sub_param table
		 */
		struct ppecfg_param *sub_param = ppecfg_param_search(name, param->sub_params, param->num_params);
		if (!sub_param || sub_param->name == NULL) {
			continue;
		}

		error = 0;

		sub_param->valid = true;
		sub_param->data = name + sub_param->len;


		/*
		 * parameter found, call the found handler if present
		 */
		if (!sub_param->match_cb) {
			continue;
		}

		error = sub_param->match_cb(sub_param, match);
		if (error) {
			break;
		}
	}

	return error;
}

/*
 * ppecfg_param_get_str()
 * 	extract the string from the incoming data
 */
int ppecfg_param_get_str(const char *arg, uint16_t data_sz, void *data)
{
	if (!arg || !data) {
		return -EINVAL;
	}

	strlcpy(data, arg, data_sz);

	return 0;
}

/*
 * ppecfg_param_get_int()
 * 	extract the integer from the incoming data
 * 	Supports decimal, hex (0x prefix), and octal (0 prefix) formats
 */
int ppecfg_param_get_int(const char *arg, uint16_t data_sz, void *data)
{
	long int_val;
	char *end;
	const char *start = arg;

	if (!arg || !data) {
		return -EINVAL;
	}

	/*
	 * Skip leading whitespace
	 */
	while (*start && isspace(*start)) {
		start++;
	}

	/*
	 * Check if we have a valid string after trimming
	 */
	if (*start == '\0') {
		return -EINVAL;
	}

	/*
	 * Reset errno to test if there any errors in the conversion
	 * process. If there are errors during the conversion, an
	 * error will be returned without any value produced in output.
	 * Use base 0 to auto-detect: 0x for hex, 0 for octal, otherwise decimal
	 */
	errno = 0;
	int_val = strtol(start, &end, 0);

	/*
	 * Check for conversion errors
	 */
	if (errno) {
		if (errno == ERANGE) { /* Overflow or underflow */
			return -E2BIG;
		}
		return -EINVAL;
	}

	/*
	 * Check if any conversion happened
	 */
	if (end == start) {
		return -EINVAL;
	}

	/*
	 * Check for trailing non-whitespace characters
	 */
	while (*end && isspace(*end)) {
		end++;
	}
	if (*end != '\0') {
		return -EINVAL;
	}

	memcpy(data, &int_val, data_sz);

	return 0;
}

/*
 * ppecfg_param_get_bool()
 * 	extract the boolean from the incoming data
 */
int ppecfg_param_get_bool(const char *arg, bool *data)
{
	int len;
	if (!arg || !data) {
		return -EINVAL;
	}

	int index = 0;
	len = strlen(arg);
	char arg_arr[len];
	while (index < len) {
		arg_arr[index] = tolower(arg[index]);
		index++;
	}

	if (!strncmp(arg_arr, "true", strlen("true"))) {
		*data = true;
		return 0;
	}

	if (!strncmp(arg_arr, "false", strlen("false"))) {
		*data = false;
		return 0;
	}

	return -EINVAL;
}

/*
 * ppecfg_param_get_hex()
 * 	extract the hexadecimal from the incoming data
 */
int ppecfg_param_get_hex(const char *arg, uint16_t data_sz, uint8_t *data)
{
	if (!arg || !data) {
		return -EINVAL;
	}

	/*
	 * 2 bytes in the string represents 1 byte in the data;
	 * Also, we want to ensure not going beyond the internal
	 * storage size
	 */
	size_t arg_len = strnlen(arg, data_sz * 2);

	/*
	 * align the argument length to 2 bytes
	 */
	size_t data_len = (PPECFG_ALIGN(arg_len, 2) / 2);

	int i = arg_len - 1;
	int j = data_len - 1;

	/*
	 * Read input stream and extract 2 bytes and store them
	 * in 1 byte unsigned array
	 */
	for (; i >= 0; i--, j--) {
		data[j] = ppecfg_param_char2hex(arg[i]);

		if (--i < 0) {
			break;
		}

		data[j] |= (ppecfg_param_char2hex(arg[i]) << 4);

	}

	return 0;
}

/*
 * ppecfg_param_get_ipaddr()
 * 	extract the IPv4 or IPv6 address in network order from the incoming data
 */
int ppecfg_param_get_ipaddr(const char *arg, uint16_t data_sz, void *data)
{
	if (!arg || !data) {
		return -EINVAL;
	}

	switch(data_sz) {
	case sizeof(struct in_addr): /* IPv4 */
		if (inet_pton(AF_INET, arg, data) == 0) {
			return -EINVAL;
		}

		break;

	case sizeof(struct in6_addr): /* IPv6 */
		if (inet_pton(AF_INET6, arg, data) == 0) {
			return -EINVAL;
		}

		break;

	default:
		return -E2BIG;
	}

	return 0;
}

/*
 * ppecfg_param_get_ipaddr_ntoh()
 * 	extract the IPv4 or IPv6 address in host order from the incoming data
 */
int ppecfg_param_get_ipaddr_ntoh(const char *arg, uint16_t data_sz, uint32_t *data)
{
	if (!arg || !data) {
		return -EINVAL;
	}

	switch(data_sz) {
	case sizeof(struct in_addr): /* IPv4 */
		if (inet_pton(AF_INET, arg, data) == 0) {
			return -EINVAL;
		}

		data[0] = ntohl(data[0]);
		return 0;

	case sizeof(struct in6_addr): /* IPv6 */
		if (inet_pton(AF_INET6, arg, data) == 0) {
			return -EINVAL;
		}

		data[0] = ntohl(data[0]);
		data[1] = ntohl(data[1]);
		data[2] = ntohl(data[2]);
		data[3] = ntohl(data[3]);

		return 0;

	default:
		return -E2BIG;
	}
}

/*
 * ppecfg_param_verify_mac()
 *	Extracts and verify the mac address.
 */
bool ppecfg_param_verify_mac(char *str_mac, uint8_t mac[])
{
	int ret;

	if (!mac || !str_mac) {
		return false;
	}

	ret = sscanf(str_mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2],
			&mac[3], &mac[4], &mac[5]);
	if (ret != PPECFG_PARAM_ETH_ALEN) {
		return false;
	}

	return true;
}

/*
 * ppecfg_get_handle()
 *      Extracts Qdisc handle ID/Class ID from user input.
 */
bool ppecfg_param_get_handle(char *arg, uint32_t *handle_id)
{
	uint16_t maj, min;
	int ret;

	if (!arg) {
		return false;
	}

	ret = sscanf(arg, "%hx:%hx", &maj, &min);
	if (!ret) {
		return false;
	}

	/*
	 * For classful qdiscs input can be either handle_id or class_id
	 */
	*handle_id = ((maj << 16) | min);
	return true;

}

/*
 * ppecfg_get_json_object()
 * 	returns the json object for the matching parameter
 */
struct json_object* ppecfg_get_json_object(struct json_object *res, const char *param)
{
	struct json_object *val;
	if (!json_object_object_get_ex(res, param, &val)) {
		return NULL;
	}

	return val;
}

/*
 * ppecfg_json_object_handler()
 *	returns the string value of a json_object
 */
char *ppecfg_json_object_handler(struct json_object *rule_obj, const char *param)
{
	struct json_object *obj = ppecfg_get_json_object(rule_obj, param);
	const char *val = NULL;

	if (!obj) {
		return NULL;
	} else {
		val = json_object_get_string(obj);
	}

	return (char *)val;
}
