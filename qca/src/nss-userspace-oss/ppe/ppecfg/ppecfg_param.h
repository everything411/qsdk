/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __PPECFG_PARAM_H
#define __PPECFG_PARAM_H

#define PPECFG_PARAM_PROTO_LEN 20
#define PPECFG_PARAM_ETH_ALEN 6

#define PPECFG_PARAM_NUM(param_array) (sizeof(param_array) / sizeof(param_array[0]))

#define PPECFG_PARAM_INIT(_idx, _param) 	\
[_idx] = {	\
	.name = (_param),	\
	.len = sizeof(_param) - 1,	\
	.num_params = 1,	\
	.valid = false,		\
}

#define PPECFG_PARAMFUNC_INIT(_param, _match_cb) {	\
	.name = (_param),	\
	.len = sizeof(_param) - 1,	\
	.num_params = 1,	\
	.match_cb = (_match_cb),	\
	.valid = false,		\
}

#define PPECFG_PARAMLIST_INIT(_param, _sub_param_tbl, _match_cb) {	\
	.name = (_param),	\
	.len = sizeof(_param) - 1,	\
	.num_params = PPECFG_PARAM_NUM(_sub_param_tbl),	\
	.sub_params = (_sub_param_tbl),	\
	.match_cb = (_match_cb),	\
	.valid = false,		\
}

#define PPECFG_PARAMARR_INIT(_idx, _param, _sub_param_tbl, _match_cb) 	\
[_idx] = {	\
	.name = (_param),	\
	.len = sizeof(_param) - 1,	\
	.num_params = PPECFG_PARAM_NUM(_sub_param_tbl),	\
	.sub_params = (_sub_param_tbl),	\
	.match_cb = (_match_cb),	\
	.id = _idx,		\
	.valid = false,		\
}

struct ppecfg_param;

/**
 * @brief match list provided matching the paramters
 */
struct ppecfg_param_in {
	uint32_t total;			/**< Total number of parameters */
	struct ppecfg_param *cur_param;	/**< Currently used parameter */
	char **args;			/**< List of arguments */
};

/**
 * @brief callback function when the match succeeds
 *
 * @param param[IN] parameter that matched
 * @param match[IN] match list used
 * @param data[OUT] store data extracted from match list; caller should provide valid memory
 *
 * @return 0 for success and -ve for failure
 */
typedef int ( *ppecfg_param_match_t)(struct ppecfg_param *param, struct ppecfg_param_in *match);

/**
 * @brief parameter definition
 */
struct ppecfg_param {
	char *name;				/**< name of the parameter */
	char *data;				/**< pointer to data portion */

	uint16_t len;				/**< string length of the parameter */
	uint16_t num_params;			/**< number of sub-parameters present */

	uint8_t id;
	bool valid;

	struct ppecfg_param *sub_params;		/**< sub-parameter list */
	ppecfg_param_match_t match_cb;		/**< match callback function upon match */
};

/**
 * @brief iterate through a table of sub parameters and match it with the corresponding of arguments
 *
 * @param param[IN] paramter
 * @param match[IN] argument list
 *
 * @return 0 on sucess and -ve on failure
 */
int ppecfg_param_iter_tbl(struct ppecfg_param *param, struct ppecfg_param_in *match);

/**
 * @brief dump help options for the corresponding param table
 *
 * @param param[IN] parent parameter
 *
 * @return 0
 */
int ppecfg_param_help(struct ppecfg_param *param);
/**
 * @brief extract the string from the argument
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size of the string
 * @param data[OUT] storage location where the string needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int ppecfg_param_get_str(const char *arg, uint16_t data_sz, void *data);

/**
 * @brief extract the integer from the argument string
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size of the integer {1, 2, 4, 8}
 * @param data[OUT] storage location where the integer needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int ppecfg_param_get_int(const char *arg, uint16_t data_sz, void *data);

/**
 * @brief extract the boolean from the argument string
 *
 * @param arg[IN] argument string
 * @param data[OUT] storage location where the boolean needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int ppecfg_param_get_bool(const char *arg, bool *data);

/**
 * @brief extract the hex number from the argument string
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size of the hex number
 * @param data[OUT] storage location where the hex number needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int ppecfg_param_get_hex(const char *arg, uint16_t data_sz, uint8_t *data);

/**
 * @brief extract the IPv4 or IPv6 address from the argument string
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size for IP address {IPv4 = 4 & IPv6 = 16}
 * @param data[OUT] storage location where the IP address needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int ppecfg_param_get_ipaddr(const char *arg, uint16_t data_sz, void *data);

/**
 * @brief extract the IPv4 or IPv6 address from the argument string and convert to host byte
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size for IP address {IPv4 = 4 & IPv6 = 16}
 * @param data[OUT] storage location where the IP address needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int ppecfg_param_get_ipaddr_ntoh(const char *arg, uint16_t data_sz, uint32_t *data);

/**
 * @brief extract the protocol number from the protocol string
 *
 * @param str protocol string
 *
 * @param protocol_num[OUT] protocol number for matched protocol name
 *
 * @return 0 on success or -ve on failure
 */
int ppecfg_param_get_protocol(char *str, uint8_t *protocol_num);

/**
 * @brief Parse and verify the mac address
 *
 * @param str_mac MAC string address
 * @param mac stores parsed mac address
 * @return true on successful parsing else false
 */
bool ppecfg_param_verify_mac(char *str_mac, uint8_t mac[]);

/**
 * @brief Extracts Qdisc handle ID/Class ID from user input.
 *
 * @param arg[IN]        Address of user input string.
 * @param handle_id[OUT] Pointer to the parsed Qdisc handle ID/Class Id from user input.
 * @return               True on successful parsing else False.
 */
bool ppecfg_param_get_handle(char *arg, uint32_t *handle_id);

/**
 * @brief Checks for the matching parameter and returns the value
 *
 * @param obj		Previous Json Object Response
 * @param param 	Matching parameter
 *
 * @return		Json object value for matching parameter
 */
struct json_object *ppecfg_get_json_object(struct json_object *obj, const char *param);

/**
 * @brief Extracts Json object and converts the value to string
 *
 * @param obj		Previous Json Object Response
 * @param param		Matching parameter
 *
 * @return		Json object value for matching parameter in string format
 */
char *ppecfg_json_object_handler(struct json_object *rule_obj, const char *param);

#endif /* __PPECFG_PARAM_H*/
