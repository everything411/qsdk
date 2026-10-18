/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG Tunnel RPS handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_tun_rps.h"

static int ppecfg_tun_rps_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_tun_rps_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * User data 1 parameters
 */
static struct ppecfg_param usr_data1_params[PPECFG_TUN_RPS_USR_DATA1_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA1_EN, "usr_data1_en="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA1_OFFSET, "usr_data1_offset="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA1_VAL, "usr_data1_val="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA1_MASK, "usr_data1_mask="),
};

/*
 * User data 2 parameters
 */
static struct ppecfg_param usr_data2_params[PPECFG_TUN_RPS_USR_DATA2_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA2_EN, "usr_data2_en="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA2_OFFSET, "usr_data2_offset="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA2_VAL, "usr_data2_val="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA2_MASK, "usr_data2_mask="),
};

/*
 * User data 3 parameters
 */
static struct ppecfg_param usr_data3_params[PPECFG_TUN_RPS_USR_DATA3_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA3_EN, "usr_data3_en="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA3_OFFSET, "usr_data3_offset="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA3_VAL, "usr_data3_val="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_USR_DATA3_MASK, "usr_data3_mask="),
};

/*
 * Rule add parameters
 */
static struct ppecfg_param rule_add_params[PPECFG_TUN_RPS_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_IS_IPV6, "is_ipv6="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_SIP, "sip="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_DIP, "dip="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_PROTO, "proto="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_SPORT, "sport="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_DPORT, "dport="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_HDR_LEN_TYPE, "hdr_len_type="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_HDR_LEN, "hdr_len="),
	PPECFG_PARAMARR_INIT(PPECFG_TUN_RPS_RULE_ADD_USR_DATA1, "usr_data1", usr_data1_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_TUN_RPS_RULE_ADD_USR_DATA2, "usr_data2", usr_data2_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_TUN_RPS_RULE_ADD_USR_DATA3, "usr_data3", usr_data3_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_INNER_PKT_TYPE, "inner_pkt_type="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL, "eth_protocol="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL_MASK, "eth_protocol_mask="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL, "ip_protocol="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL_MASK, "ip_protocol_mask="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_UDP_SPORT, "udp_sport="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_UDP_DPORT, "udp_dport="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_WAN_IF, "wan_if="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_INNER_IP_PROTO_OFFSET, "inner_ip_proto_offset="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_VAL, "inner_ipv4_proto_val="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_MASK, "inner_ipv4_proto_mask="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_VAL, "inner_ipv6_proto_val="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_MASK, "inner_ipv6_proto_mask="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_PPPOE_EN, "pppoe_en="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_PPPOE_SESSION_ID, "pppoe_session_id="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_ADD_PPPOE_SERVER_MAC, "pppoe_server_mac="),
};

/*
 * Rule delete parameters
 */
static struct ppecfg_param rule_del_params[PPECFG_TUN_RPS_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_DEL_IS_IPV6, "is_ipv6="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_DEL_SIP, "sip="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_DEL_DIP, "dip="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_DEL_PROTO, "proto="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_DEL_SPORT, "sport="),
	PPECFG_PARAM_INIT(PPECFG_TUN_RPS_RULE_DEL_DPORT, "dport="),
	PPECFG_PARAMARR_INIT(PPECFG_TUN_RPS_RULE_DEL_USR_DATA1, "usr_data1", usr_data1_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_TUN_RPS_RULE_DEL_USR_DATA2, "usr_data2", usr_data2_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_TUN_RPS_RULE_DEL_USR_DATA3, "usr_data3", usr_data3_params, ppecfg_param_iter_tbl),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_tun_rps_cmd' should also get updated
 */
struct ppecfg_param ppecfg_tun_rps_params[PPECFG_TUN_RPS_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=add_rps_rule", rule_add_params, ppecfg_tun_rps_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=del_rps_rule", rule_del_params, ppecfg_tun_rps_rule_del),
};

/*
 * ppecfg_tun_rps_parse_hdr_len_type()
 *	Parse header length type string to enum
 */
static int ppecfg_tun_rps_parse_hdr_len_type(const char *hdr_len_type_str)
{
	if (!hdr_len_type_str) {
		ppecfg_log_warn("hdr_len_type is mandatory\n");
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID;
	}

	if (strcmp(hdr_len_type_str, "eth") == 0) {
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH;
	} else if (strcmp(hdr_len_type_str, "ip") == 0) {
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_IP;
	} else if (strcmp(hdr_len_type_str, "udp") == 0) {
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_UDP;
	}

	ppecfg_log_warn("Invalid hdr_len_type value: %s (must be eth, ip, or udp)\n", hdr_len_type_str);
	return PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID;
}

/*
 * ppecfg_tun_rps_parse_inner_pkt_type()
 *	Parse inner packet type string to enum
 */
static int ppecfg_tun_rps_parse_inner_pkt_type(const char *inner_pkt_type_str)
{
	if (!inner_pkt_type_str) {
		ppecfg_log_warn("inner_pkt_type is mandatory\n");
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID;
	}

	if (strcmp(inner_pkt_type_str, "eth") == 0) {
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_ETH;
	} else if (strcmp(inner_pkt_type_str, "ip") == 0) {
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_IP;
	}

	ppecfg_log_warn("Invalid inner_pkt_type value: %s (must be eth or ip)\n", inner_pkt_type_str);
	return  PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID;
}

/*
 * ppecfg_tun_rps_validate_inner_ip()
 *	Validate inner IP protocol configuration when inner_pkt_type is IP.
 *	inner_ip_proto_offset is mandatory; at least one of ipv4_val or ipv6_val must be provided.
 */
static int ppecfg_tun_rps_validate_inner_ip(struct nss_ppe_nl_tun_rps *rule,
					      struct ppecfg_param *params)
{
	char *offset_data;
	char *ipv4_val_data;
	char *ipv6_val_data;

	if (!rule || !params) {
		ppecfg_log_warn("Invalid parameters: rule or params is NULL\n");
		return -EINVAL;
	}

	/*
	 * When inner_pkt_type is IP, user_data3 cannot be enabled
	 */
	if (rule->usr_data.udf[2].data_en) {
		ppecfg_log_warn("user_data3 cannot be enabled when inner_pkt_type=ip\n");
		return -EINVAL;
	}

	/*
	 * inner_ip_proto_offset is MANDATORY
	 */
	offset_data = params[PPECFG_TUN_RPS_RULE_ADD_INNER_IP_PROTO_OFFSET].data;
	if (!offset_data) {
		ppecfg_log_warn("inner_ip_proto_offset is mandatory when inner_pkt_type=ip\n");
		return -EINVAL;
	}

	/*
	 * At least one of ipv4_val or ipv6_val must be provided
	 */
	ipv4_val_data = params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_VAL].data;
	ipv6_val_data = params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_VAL].data;

	if (!ipv4_val_data && !ipv6_val_data) {
		ppecfg_log_warn("At least one of inner_ipv4_proto_val or inner_ipv6_proto_val must be provided when inner_pkt_type=ip\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * ppecfg_tun_rps_fill_header_match()
 *	Fill header matching structure from parsed parameters with strict validation.
 */
static int ppecfg_tun_rps_fill_header_match(struct ppecfg_param *header_params,
					     struct nss_ppe_nl_tun_rps *rule,
					     int header_type)
{
	char *data;
	int error;

	if (!header_params || !rule) {
		ppecfg_log_warn("Invalid parameters: header_params or rule is NULL\n");
		return -EINVAL;
	}

	/*
	 * Initialize header match union to zero
	 */
	memset(&rule->header_match, 0, sizeof(rule->header_match));

	switch (header_type) {
	case PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH:
		/*
		 * Validate that only ETH fields are provided
		 */
		if (header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL_MASK].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_SPORT].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_DPORT].data) {
			ppecfg_log_warn("Only eth_protocol fields allowed when hdr_len_type=eth\n");
			return -EINVAL;
		}

		/*
		 * Get eth_protocol - MANDATORY for ETH type
		 */
		data = header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL].data;
		if (!data) {
			ppecfg_log_warn("eth_protocol is mandatory when hdr_len_type=eth\n");
			return -EINVAL;
		}

		ppecfg_log_info("Parsing eth_protocol from string: '%s'\n", data);
		error = ppecfg_param_get_int(data, sizeof(uint16_t), &rule->header_match.l2_match.eth_protocol);
		if (error < 0) {
			ppecfg_log_warn("Failed to parse eth_protocol value '%s' (error: %d)\n", data, error);
			ppecfg_log_data_error(&header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL]);
			return error;
		}

		ppecfg_log_info("Successfully parsed eth_protocol: 0x%04x (%u)\n",
				rule->header_match.l2_match.eth_protocol,
				rule->header_match.l2_match.eth_protocol);

		/*
		 * Validate eth_protocol is non-zero
		 */
		if (rule->header_match.l2_match.eth_protocol == 0) {
			ppecfg_log_warn("eth_protocol cannot be zero\n");
			return -EINVAL;
		}

		/*
		 * Get eth_protocol_mask - optional, defaults to 0xFFFF
		 */
		data = header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL_MASK].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &rule->header_match.l2_match.eth_protocol_mask);
			if (error < 0) {
				ppecfg_log_data_error(&header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL_MASK]);
				return error;
			}
		} else {
			/*
			 * Default mask
			 */
			rule->header_match.l2_match.eth_protocol_mask = 0xFFFF;
		}
		break;

	case PPECFG_TUN_RPS_HDR_LEN_TYPE_IP:
		/*
		 * Validate that only IP fields are provided
		 */
		if (header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL_MASK].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_SPORT].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_DPORT].data) {
			ppecfg_log_warn("Only ip_protocol fields allowed when hdr_len_type=ip\n");
			return -EINVAL;
		}

		/*
		 * Get ip_protocol - MANDATORY for IP type
		 */
		data = header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL].data;
		if (!data) {
			ppecfg_log_warn("ip_protocol is mandatory when hdr_len_type=ip\n");
			return -EINVAL;
		}

		error = ppecfg_param_get_int(data, sizeof(uint8_t), &rule->header_match.l3_match.ip_protocol);
		if (error < 0) {
			ppecfg_log_data_error(&header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL]);
			return error;
		}

		/*
		 * Validate ip_protocol is non-zero
		 */
		if (rule->header_match.l3_match.ip_protocol == 0) {
			ppecfg_log_warn("ip_protocol cannot be zero\n");
			return -EINVAL;
		}

		/*
		 * Get ip_protocol_mask - optional, defaults to 0xFF
		 */
		data = header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL_MASK].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &rule->header_match.l3_match.ip_protocol_mask);
			if (error < 0) {
				ppecfg_log_data_error(&header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL_MASK]);
				return error;
			}
		} else {
			/*
			 * Default mask
			 */
			rule->header_match.l3_match.ip_protocol_mask = 0xFF;
		}
		break;

	case PPECFG_TUN_RPS_HDR_LEN_TYPE_UDP:
		/*
		 * Validate that only UDP fields are provided
		 */
		if (header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL_MASK].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL].data ||
		    header_params[PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL_MASK].data) {
			ppecfg_log_warn("Only udp_sport/udp_dport fields allowed when hdr_len_type=udp\n");
			return -EINVAL;
		}

		/*
		 * Get udp_sport - optional (can be zero for wildcard)
		 */
		data = header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_SPORT].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &rule->header_match.l4_match.udp_sport);
			if (error < 0) {
				ppecfg_log_data_error(&header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_SPORT]);
				return error;
			}
		}

		/*
		 * Get udp_dport - optional (can be zero for wildcard)
		 */
		data = header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_DPORT].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &rule->header_match.l4_match.udp_dport);
			if (error < 0) {
				ppecfg_log_data_error(&header_params[PPECFG_TUN_RPS_RULE_ADD_UDP_DPORT]);
				return error;
			}
		}
		break;

	default:
		ppecfg_log_warn("Invalid header type: %d\n", header_type);
		return -EINVAL;
	}

	return 0;
}

/*
 * ppecfg_tun_rps_fill_user_data()
 *	Fill user data structure from parsed parameters.
 */
static int ppecfg_tun_rps_fill_user_data(struct ppecfg_param *usr_data_params,
					  struct nss_ppe_nl_tun_rps *rule,
					  int usr_data_num)
{
	char *data;
	int error;
	bool usr_data_en = false;
	int udf_index;

	/*
	 * Check if user data is enabled.
	 * _EN field is always first.
	 */
	data = usr_data_params[0].data;
	if (data) {
		error = ppecfg_param_get_bool(data, &usr_data_en);
		if (error < 0) {
			ppecfg_log_data_error(&usr_data_params[0]);
			return error;
		}
	}

	/*
	 * Map user data structure using array-based indexing
	 * usr_data_num is 1-based, convert to 0-based array index
	 */
	udf_index = usr_data_num - 1;

	if (udf_index < 0 || udf_index > 2) {
		ppecfg_log_warn("Invalid user data number: %d\n", usr_data_num);
		return -EINVAL;
	}

	rule->usr_data.udf[udf_index].data_en = usr_data_en;
	if (usr_data_en) {
		/*
		 * Get offset
		 */
		data = usr_data_params[1].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &rule->usr_data.udf[udf_index].data_offset);
			if (error < 0) {
				ppecfg_log_data_error(&usr_data_params[1]);
				return error;
			}
		}

		/*
		 * Get value
		 */
		data = usr_data_params[2].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &rule->usr_data.udf[udf_index].data_val);
			if (error < 0) {
				ppecfg_log_data_error(&usr_data_params[2]);
				return error;
			}
		}

		/*
		 * Get mask
		 */
		data = usr_data_params[3].data;
		if (data) {
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &rule->usr_data.udf[udf_index].data_mask);
			if (error < 0) {
				ppecfg_log_data_error(&usr_data_params[3]);
				return error;
			}
		} else {
			/*
			 * Default mask
			 */
			rule->usr_data.udf[udf_index].data_mask = 0xFFFF;
		}
	}

	return 0;
}

/*
 * ppecfg_tun_rps_rule_add()
 *	Handle tunnel RPS rule add
 */
static int ppecfg_tun_rps_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_tun_rps_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	bool bool_val = false;
	int hdr_len_type = PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL\n");
		return -EINVAL;
	}

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_tun_rps_init_rule(&nl_msg, NSS_PPE_TUN_RPS_CREATE_RULE_MSG);

	/*
	 * Validate and parse is_ipv6 before the main loop
	 * This parameter affects how IP addresses are parsed in the loop
	 */
	if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_IS_IPV6].valid) {
		ppecfg_log_warn("is_ipv6 is mandatory\n");
		error = -EINVAL;
		goto done;
	}

	error = ppecfg_param_get_bool(param->sub_params[PPECFG_TUN_RPS_RULE_ADD_IS_IPV6].data, &bool_val);
	if (error < 0) {
		ppecfg_log_data_error(&param->sub_params[PPECFG_TUN_RPS_RULE_ADD_IS_IPV6]);
		goto done;
	}
	nl_msg.rule.is_ipv6 = bool_val;

	/*
	 * Parse remaining parameters
	 * Note: is_ipv6 already parsed above
	 */
	for (int index = PPECFG_TUN_RPS_RULE_ADD_SIP; index < PPECFG_TUN_RPS_RULE_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_TUN_RPS_RULE_ADD_SIP:
			if (nl_msg.rule.is_ipv6) {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(nl_msg.rule.ipv6_sip), &nl_msg.rule.ipv6_sip);
			} else {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(uint32_t), &nl_msg.rule.ipv4_sip);
			}
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_DIP:
			if (nl_msg.rule.is_ipv6) {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(nl_msg.rule.ipv6_dip), &nl_msg.rule.ipv6_dip);
			} else {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(uint32_t), &nl_msg.rule.ipv4_dip);
			}
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_PROTO:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.protocol);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_SPORT:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.sport);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_DPORT:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.dport);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_HDR_LEN_TYPE:
			hdr_len_type = ppecfg_tun_rps_parse_hdr_len_type(sub_params->data);
			if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID) {
				error = -EINVAL;
				goto done;
			}
			nl_msg.rule.hdr_len_type = hdr_len_type;
			break;

		case PPECFG_TUN_RPS_RULE_ADD_HDR_LEN:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.hdr_len);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			/*
			 * Validate header length upper bound
			 */
			if (nl_msg.rule.hdr_len > PPECFG_TUN_RPS_TPR_MAX_OFFSET) {
				ppecfg_log_warn("hdr_len %d exceeds maximum allowed value of %d\n",
						nl_msg.rule.hdr_len, PPECFG_TUN_RPS_TPR_MAX_OFFSET);
				error = -EINVAL;
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_USR_DATA1:
			sub_params = param->sub_params[PPECFG_TUN_RPS_RULE_ADD_USR_DATA1].sub_params;
			error = ppecfg_tun_rps_fill_user_data(sub_params, &nl_msg.rule, 1);
			if (error < 0) {
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_USR_DATA2:
			sub_params = param->sub_params[PPECFG_TUN_RPS_RULE_ADD_USR_DATA2].sub_params;
			error = ppecfg_tun_rps_fill_user_data(sub_params, &nl_msg.rule, 2);
			if (error < 0) {
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_USR_DATA3:
			sub_params = param->sub_params[PPECFG_TUN_RPS_RULE_ADD_USR_DATA3].sub_params;
			error = ppecfg_tun_rps_fill_user_data(sub_params, &nl_msg.rule, 3);
			if (error < 0) {
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_INNER_PKT_TYPE: {
			int inner_pkt_type = ppecfg_tun_rps_parse_inner_pkt_type(sub_params->data);
			if (inner_pkt_type == PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID) {
				error = -EINVAL;
				goto done;
			}
			nl_msg.rule.inner_pkt_type = inner_pkt_type;
			break;
		}

		case PPECFG_TUN_RPS_RULE_ADD_WAN_IF:
			strlcpy(nl_msg.rule.wan_if, sub_params->data, sizeof(nl_msg.rule.wan_if));
			break;

		case PPECFG_TUN_RPS_RULE_ADD_INNER_IP_PROTO_OFFSET:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.inner_ip_proto.ip_proto_offset);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_VAL:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_MASK:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_VAL:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_MASK:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_PPPOE_EN:
			error = ppecfg_param_get_bool(sub_params->data, &nl_msg.rule.pppoe_en);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_PPPOE_SESSION_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.pppoe_session_id);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_ADD_PPPOE_SERVER_MAC:
			if (!ppecfg_param_verify_mac(sub_params->data, nl_msg.rule.pppoe_server_mac)) {
				ppecfg_log_data_error(sub_params);
				error = -EINVAL;
				goto done;
			}
			break;
		}
	}

	/*
	 * Post-loop validations for mandatory parameters
	 */

	/*
	 * Validate hdr_len was provided (mandatory parameter)
	 */
	if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_HDR_LEN].valid) {
		ppecfg_log_warn("hdr_len is mandatory\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Validate that header length is even (multiple of 2) as required by hardware/SSDK
	 */
	if (nl_msg.rule.hdr_len % 2 != 0) {
		ppecfg_log_warn("hdr_len %d must be an even number (multiple of 2)\n", nl_msg.rule.hdr_len);
		error = -EINVAL;
		goto done;
	}

	/*
	 * Validate hdr_len_type was provided (mandatory parameter)
	 */
	if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID) {
		ppecfg_log_warn("hdr_len_type is mandatory but was not provided\n");
		error = -EINVAL;
		goto done;
	}

	if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_PKT_TYPE].valid) {
		ppecfg_log_warn("inner_pkt_type is mandatory\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Validate wan_if was provided (mandatory parameter)
	 */
	if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_WAN_IF].valid) {
		ppecfg_log_warn("wan_if is mandatory\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Validate wan_if is not empty
	 */
	if (strlen(nl_msg.rule.wan_if) == 0) {
		ppecfg_log_warn("wan_if cannot be empty\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Validate UDF offsets and masks for consistency with Netlink and Driver
	 */
	for (int i = 0; i < 3; i++) {
		if (nl_msg.rule.usr_data.udf[i].data_en) {
			if (nl_msg.rule.usr_data.udf[i].data_offset > PPECFG_TUN_RPS_TPR_MAX_OFFSET) {
				ppecfg_log_warn("usr_data%d_offset %d exceeds maximum of %d\n",
						i + 1, nl_msg.rule.usr_data.udf[i].data_offset, PPECFG_TUN_RPS_TPR_MAX_OFFSET);
				error = -EINVAL;
				goto done;
			}
			if (nl_msg.rule.usr_data.udf[i].data_offset % 2 != 0) {
				ppecfg_log_warn("usr_data%d_offset %d must be an even number (multiple of 2)\n",
						i + 1, nl_msg.rule.usr_data.udf[i].data_offset);
				error = -EINVAL;
				goto done;
			}
			if (nl_msg.rule.usr_data.udf[i].data_mask == 0) {
				ppecfg_log_warn("usr_data%d_mask cannot be zero when enabled\n", i + 1);
				error = -EINVAL;
				goto done;
			}
			if (nl_msg.rule.usr_data.udf[i].data_offset + 2 > nl_msg.rule.hdr_len) {
				ppecfg_log_warn("usr_data%d_offset %d + 2 exceeds hdr_len %d\n",
						i + 1, nl_msg.rule.usr_data.udf[i].data_offset, nl_msg.rule.hdr_len);
				error = -EINVAL;
				goto done;
			}
			if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH &&
			    nl_msg.rule.usr_data.udf[i].data_offset < 14) {
				ppecfg_log_warn("usr_data%d_offset %d is within Ethernet header (< 14)\n",
						i + 1, nl_msg.rule.usr_data.udf[i].data_offset);
				error = -EINVAL;
				goto done;
			}
		}
	}

	/*
	 * Validate header match fields based on hdr_len_type.
	 * ppecfg_tun_rps_fill_header_match() zeroes header_match and then
	 * re-parses the relevant fields with cross-field validation
	 */
	error = ppecfg_tun_rps_fill_header_match(param->sub_params, &nl_msg.rule, hdr_len_type);
	if (error < 0) {
		ppecfg_log_warn("Failed to fill header match fields\n");
		goto done;
	}

	/*
	 * Validate inner IP proto fields if inner_pkt_type is IP
	 */
	if (nl_msg.rule.inner_pkt_type == PPECFG_TUN_RPS_INNER_PKT_TYPE_IP) {
		uint16_t default_mask = 0xFFFF;

		error = ppecfg_tun_rps_validate_inner_ip(&nl_msg.rule, param->sub_params);
		if (error < 0) {
			goto done;
		}

		/*
		 * Either IPv4, IPv6 protocol or both needs to be matched
		 */
		if (nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value == 0 &&
		    nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value == 0) {
			ppecfg_log_warn("At least one inner IP protocol value must be matched when inner_pkt_type=ip\n");
			error = -EINVAL;
			goto done;
		}

		/*
		 * Ensure both masks are non-zero as required by hardware.
		 * If one mask is provided and the other is not, propagate the configured side's mask.
		 * If neither mask is provided, use 0xFFFF.
		 */
		if (param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_MASK].valid) {
			default_mask = nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask;
		} else if (param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_MASK].valid) {
			default_mask = nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask;
		}

		if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_MASK].valid) {
			nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask = default_mask;
			if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_VAL].valid) {
				nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value = 0;
			}
		}

		if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_MASK].valid) {
			nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask = default_mask;
			if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_VAL].valid) {
				nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value = 0;
			}
		}

		/*
		 * Validate offset bounds
		 */
		if (nl_msg.rule.inner_ip_proto.ip_proto_offset > PPECFG_TUN_RPS_TPR_MAX_OFFSET) {
			ppecfg_log_warn("inner_ip_proto_offset %d exceeds maximum of %d\n",
					nl_msg.rule.inner_ip_proto.ip_proto_offset, PPECFG_TUN_RPS_TPR_MAX_OFFSET);
			error = -EINVAL;
			goto done;
		}

		/*
		 * Validate that inner IP protocol offset is an even number (multiple of 2)
		 */
		if (nl_msg.rule.inner_ip_proto.ip_proto_offset % 2 != 0) {
			ppecfg_log_warn("inner_ip_proto_offset %d must be an even number (multiple of 2)\n",
					nl_msg.rule.inner_ip_proto.ip_proto_offset);
			error = -EINVAL;
			goto done;
		}

		/*
		 * Validate that inner IP protocol offset does not exceed header length
		 */
		if (nl_msg.rule.inner_ip_proto.ip_proto_offset + 2 > nl_msg.rule.hdr_len) {
			ppecfg_log_warn("inner_ip_proto_offset %d + 2 exceeds hdr_len %d\n",
					nl_msg.rule.inner_ip_proto.ip_proto_offset, nl_msg.rule.hdr_len);
			error = -EINVAL;
			goto done;
		}

		/*
		 * Validate that inner IP protocol offset is not within Ethernet header if type is eth
		 */
		if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH &&
		    nl_msg.rule.inner_ip_proto.ip_proto_offset < 14) {
			ppecfg_log_warn("inner_ip_proto_offset %d falls within Ethernet header (< 14)\n",
					nl_msg.rule.inner_ip_proto.ip_proto_offset);
			error = -EINVAL;
			goto done;
		}

		ppecfg_log_info("Inner IP Proto: offset=%u, ipv4_val=0x%x, ipv4_mask=0x%x, ipv6_val=0x%x, ipv6_mask=0x%x\n",
				nl_msg.rule.inner_ip_proto.ip_proto_offset,
				nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value,
				nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask,
				nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value,
				nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask);
	}

	/*
	 * Validate PPPoE configuration
	 */
	if (nl_msg.rule.pppoe_en) {
		if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_PPPOE_SESSION_ID].valid) {
			ppecfg_log_warn("pppoe_session_id is mandatory when pppoe_en is set\n");
			error = -EINVAL;
			goto done;
		}
		if (!param->sub_params[PPECFG_TUN_RPS_RULE_ADD_PPPOE_SERVER_MAC].valid) {
			ppecfg_log_warn("pppoe_server_mac is mandatory when pppoe_en is set\n");
			error = -EINVAL;
			goto done;
		}
		ppecfg_log_info("PPPoE: enabled, session_id=%u, server_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
				nl_msg.rule.pppoe_session_id,
				nl_msg.rule.pppoe_server_mac[0], nl_msg.rule.pppoe_server_mac[1],
				nl_msg.rule.pppoe_server_mac[2], nl_msg.rule.pppoe_server_mac[3],
				nl_msg.rule.pppoe_server_mac[4], nl_msg.rule.pppoe_server_mac[5]);
	} else {
		ppecfg_log_info("PPPoE: disabled\n");
	}

	/*
	 * Log WAN interface if provided
	 */
	if (param->sub_params[PPECFG_TUN_RPS_RULE_ADD_WAN_IF].valid) {
		ppecfg_log_info("WAN interface: %s\n", nl_msg.rule.wan_if);
	}

	/*
	 * send message
	 */
	error = nss_ppenl_tun_rps_rule_add(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send tunnel RPS create message\n");
		return error;
	}

	return error;

done:
	return error;
}

/*
 * ppecfg_tun_rps_rule_del()
 *     Handle tunnel RPS rule delete
 */
static int ppecfg_tun_rps_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_tun_rps_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	bool bool_val = false;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL\n");
		return -EINVAL;
	}

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_tun_rps_init_rule(&nl_msg, NSS_PPE_TUN_RPS_DESTROY_RULE_MSG);

	/*
	 * Validate and parse is_ipv6 before the main loop
	 * This parameter affects how IP addresses are parsed in the loop
	 */
	if (!param->sub_params[PPECFG_TUN_RPS_RULE_DEL_IS_IPV6].valid) {
		ppecfg_log_warn("is_ipv6 is mandatory\n");
		error = -EINVAL;
		goto done;
	}

	error = ppecfg_param_get_bool(param->sub_params[PPECFG_TUN_RPS_RULE_DEL_IS_IPV6].data, &bool_val);
	if (error < 0) {
		ppecfg_log_data_error(&param->sub_params[PPECFG_TUN_RPS_RULE_DEL_IS_IPV6]);
		goto done;
	}
	nl_msg.rule.is_ipv6 = bool_val;

	/*
	 * Parse remaining parameters
	 * Note: is_ipv6 already parsed above
	 */
	for (int index = PPECFG_TUN_RPS_RULE_DEL_SIP; index < PPECFG_TUN_RPS_RULE_DEL_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_TUN_RPS_RULE_DEL_SIP:
			if (nl_msg.rule.is_ipv6) {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(nl_msg.rule.ipv6_sip), &nl_msg.rule.ipv6_sip);
			} else {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(uint32_t), &nl_msg.rule.ipv4_sip);
			}
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_DIP:
			if (nl_msg.rule.is_ipv6) {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(nl_msg.rule.ipv6_dip), &nl_msg.rule.ipv6_dip);
			} else {
				error = ppecfg_param_get_ipaddr(sub_params->data, sizeof(uint32_t), &nl_msg.rule.ipv4_dip);
			}
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_PROTO:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.protocol);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_SPORT:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.sport);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_DPORT:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.dport);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_USR_DATA1:
			sub_params = param->sub_params[PPECFG_TUN_RPS_RULE_DEL_USR_DATA1].sub_params;
			error = ppecfg_tun_rps_fill_user_data(sub_params, &nl_msg.rule, 1);
			if (error < 0) {
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_USR_DATA2:
			sub_params = param->sub_params[PPECFG_TUN_RPS_RULE_DEL_USR_DATA2].sub_params;
			error = ppecfg_tun_rps_fill_user_data(sub_params, &nl_msg.rule, 2);
			if (error < 0) {
				goto done;
			}
			break;

		case PPECFG_TUN_RPS_RULE_DEL_USR_DATA3:
			sub_params = param->sub_params[PPECFG_TUN_RPS_RULE_DEL_USR_DATA3].sub_params;
			error = ppecfg_tun_rps_fill_user_data(sub_params, &nl_msg.rule, 3);
			if (error < 0) {
				goto done;
			}
			break;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_tun_rps_rule_del(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send tunnel RPS destroy message\n");
		return error;
	}

	return error;

done:
	return error;
}
