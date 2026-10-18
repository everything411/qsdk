/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG Tunnel RPS JSON parser
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include <json-c/json.h>
#include "ppecfg_param.h"
#include "ppecfg_tun_rps.h"
#include "ppecfg_tun_rps_json_parser.h"

/*
 * ppecfg_tun_rps_json_get_hdr_len_type()
 *	Get header length type from JSON object
 */
static int ppecfg_tun_rps_json_get_hdr_len_type(struct json_object *rule_obj)
{
	struct json_object *hdr_len_type_obj;
	const char *hdr_len_type_str;

	hdr_len_type_obj = ppecfg_get_json_object(rule_obj, "hdr_len_type");

	if (!hdr_len_type_obj) {
		ppecfg_log_warn("hdr_len_type field not found in JSON\n");
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID;
	}

	hdr_len_type_str = (char*)json_object_get_string(hdr_len_type_obj);
	if (!hdr_len_type_str) {
		ppecfg_log_warn("hdr_len_type value is NULL\n");
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID;
	}

	if (strcmp(hdr_len_type_str, "eth") == 0) {
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH;
	} else if (strcmp(hdr_len_type_str, "ip") == 0) {
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_IP;
	} else if (strcmp(hdr_len_type_str, "udp") == 0) {
		return PPECFG_TUN_RPS_HDR_LEN_TYPE_UDP;
	}

	ppecfg_log_warn("Invalid hdr_len_type value in JSON: %s (must be eth, ip, or udp)\n", hdr_len_type_str);
	return PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID;
}

/*
 * ppecfg_tun_rps_json_get_inner_pkt_type()
 *	Get inner packet type from JSON object
 */
static int ppecfg_tun_rps_json_get_inner_pkt_type(struct json_object *rule_obj)
{
	struct json_object *inner_pkt_type_obj;
	const char *inner_pkt_type_str;

	inner_pkt_type_obj = ppecfg_get_json_object(rule_obj, "inner_pkt_type");
	if (!inner_pkt_type_obj) {
		ppecfg_log_warn("inner_pkt_type field not found in JSON\n");
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID;
	}

	inner_pkt_type_str = (char*)json_object_get_string(inner_pkt_type_obj);
	if (!inner_pkt_type_str) {
		ppecfg_log_warn("inner_pkt_type value is NULL\n");
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID;
	}

	if (strcmp(inner_pkt_type_str, "eth") == 0) {
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_ETH;
	} else if (strcmp(inner_pkt_type_str, "ip") == 0) {
		return PPECFG_TUN_RPS_INNER_PKT_TYPE_IP;
	}

	ppecfg_log_warn("Invalid inner_pkt_type value in JSON: %s (must be eth or ip)\n", inner_pkt_type_str);
	return PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID;
}

/*
 * ppecfg_tun_rps_json_get_user_data()
 *	Get user data from JSON object
 */
static int ppecfg_tun_rps_json_get_user_data(struct json_object *rule_obj, struct nss_ppenl_tun_rps_usr_data *usr_data)
{
	struct json_object *usr_data_obj;
	bool usr_data_en;

	/*
	 * Initialize user data structure
	 */
	memset(usr_data, 0, sizeof(*usr_data));

	/*
	 * User data 1
	 */
	usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data1_en");
	if (usr_data_obj) {
		usr_data_en = json_object_get_boolean(usr_data_obj);
		usr_data->udf[0].data_en = usr_data_en;

		if (usr_data_en) {
			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data1_offset");
			if (usr_data_obj) {
				usr_data->udf[0].data_offset = json_object_get_int(usr_data_obj);
			}

			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data1_val");
			if (usr_data_obj) {
				usr_data->udf[0].data_val = json_object_get_int(usr_data_obj);
			}

			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data1_mask");
			if (usr_data_obj) {
				if (json_object_is_type(usr_data_obj, json_type_string)) {
					char *endptr = NULL;
					const char *mask_str = json_object_get_string(usr_data_obj);
					usr_data->udf[0].data_mask = (uint16_t)strtoul(mask_str, &endptr, 0);
					if ((mask_str == endptr) || (*endptr != '\0')) {
						ppecfg_log_warn("Invalid user data 1 mask: %s\n", mask_str);
						return -EINVAL;
					}
				} else {
					usr_data->udf[0].data_mask = json_object_get_int(usr_data_obj);
				}
			} else {
				/*
				 * Default mask
				 */
				usr_data->udf[0].data_mask = 0xFFFF;
			}
		}
	}

	/*
	 * User data 2
	 */
	usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data2_en");
	if (usr_data_obj) {
		usr_data_en = json_object_get_boolean(usr_data_obj);
		usr_data->udf[1].data_en = usr_data_en;

		if (usr_data_en) {
			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data2_offset");
			if (usr_data_obj) {
				usr_data->udf[1].data_offset = json_object_get_int(usr_data_obj);
			}

			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data2_val");
			if (usr_data_obj) {
				usr_data->udf[1].data_val = json_object_get_int(usr_data_obj);
			}

			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data2_mask");
			if (usr_data_obj) {
				if (json_object_is_type(usr_data_obj, json_type_string)) {
					char *endptr = NULL;
					const char *mask_str = json_object_get_string(usr_data_obj);
					usr_data->udf[1].data_mask = (uint16_t)strtoul(mask_str, &endptr, 0);
					if ((mask_str == endptr) || (*endptr != '\0')) {
						ppecfg_log_warn("Invalid user data 2 mask: %s\n", mask_str);
						return -EINVAL;
					}
				} else {
					usr_data->udf[1].data_mask = json_object_get_int(usr_data_obj);
				}
			} else {
				/*
				 * Default mask
				 */
				usr_data->udf[1].data_mask = 0xFFFF;
			}
		}
	}

	/*
	 * User data 3
	 */
	usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data3_en");
	if (usr_data_obj) {
		usr_data_en = json_object_get_boolean(usr_data_obj);
		usr_data->udf[2].data_en = usr_data_en;

		if (usr_data_en) {
			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data3_offset");
			if (usr_data_obj) {
				usr_data->udf[2].data_offset = json_object_get_int(usr_data_obj);
			}

			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data3_val");
			if (usr_data_obj) {
				usr_data->udf[2].data_val = json_object_get_int(usr_data_obj);
			}

			usr_data_obj = ppecfg_get_json_object(rule_obj, "usr_data3_mask");
			if (usr_data_obj) {
				if (json_object_is_type(usr_data_obj, json_type_string)) {
					char *endptr = NULL;
					const char *mask_str = json_object_get_string(usr_data_obj);
					usr_data->udf[2].data_mask = (uint16_t)strtoul(mask_str, &endptr, 0);
					if ((mask_str == endptr) || (*endptr != '\0')) {
						ppecfg_log_warn("Invalid user data 3 mask: %s\n", mask_str);
						return -EINVAL;
					}
				} else {
					usr_data->udf[2].data_mask = json_object_get_int(usr_data_obj);
				}
			} else {
				/*
				 * Default mask
				 */
				usr_data->udf[2].data_mask = 0xFFFF;
			}
		}
	}

	return 0;
}

/*
 * ppecfg_tun_rps_json_process_rule()
 *	Core processing function - handles both add and delete operations
 */
static int ppecfg_tun_rps_json_process_rule(struct json_object *rule_obj,
		enum nss_ppe_tun_rps_message_types msg_type)
{
	struct nss_ppenl_tun_rps_rule nl_msg = {{0}};
	struct json_object *obj;
	bool is_ipv6 = false;
	const char *ip_str;
	int error = 0;

	if (!rule_obj) {
		ppecfg_log_warn("Rule object is NULL\n");
		return -EINVAL;
	}

	nss_ppenl_tun_rps_init_rule(&nl_msg, msg_type);

	/*
	 * Parse common parameters (required for both add and delete)
	 */

	/*
	 * Check if IPv6
	 */
	obj = ppecfg_get_json_object(rule_obj, "is_ipv6");
	if (obj) {
		is_ipv6 = json_object_get_boolean(obj);
		nl_msg.rule.is_ipv6 = is_ipv6;
	}

	/*
	 * Source IP
	 */
	obj = ppecfg_get_json_object(rule_obj, "sip");
	if (obj) {
		ip_str = (char*)json_object_get_string(obj);
		if (ip_str) {
			if (is_ipv6) {
				error = ppecfg_param_get_ipaddr(ip_str, sizeof(nl_msg.rule.ipv6_sip), &nl_msg.rule.ipv6_sip);
			} else {
				error = ppecfg_param_get_ipaddr(ip_str, sizeof(uint32_t), &nl_msg.rule.ipv4_sip);
			}
			if (error < 0) {
				ppecfg_log_warn("Invalid source IP address: %s\n", ip_str);
				return error;
			}
		}
	}

	/*
	 * Destination IP
	 */
	obj = ppecfg_get_json_object(rule_obj, "dip");
	if (obj) {
		ip_str = (char*)json_object_get_string(obj);
		if (ip_str) {
			if (is_ipv6) {
				error = ppecfg_param_get_ipaddr(ip_str, sizeof(nl_msg.rule.ipv6_dip), &nl_msg.rule.ipv6_dip);
			} else {
				error = ppecfg_param_get_ipaddr(ip_str, sizeof(uint32_t), &nl_msg.rule.ipv4_dip);
			}
			if (error < 0) {
				ppecfg_log_warn("Invalid destination IP address: %s\n", ip_str);
				return error;
			}
		}
	}

	/*
	 * Protocol
	 */
	obj = ppecfg_get_json_object(rule_obj, "proto");
	if (obj) {
		nl_msg.rule.protocol = json_object_get_int(obj);
	}

	/*
	 * Source port
	 */
	obj = ppecfg_get_json_object(rule_obj, "sport");
	if (obj) {
		nl_msg.rule.sport = json_object_get_int(obj);
	}

	/*
	 * Destination port
	 */
	obj = ppecfg_get_json_object(rule_obj, "dport");
	if (obj) {
		nl_msg.rule.dport = json_object_get_int(obj);
	}

	/*
	 * User data (required for both add and delete for rule matching)
	 */
	error = ppecfg_tun_rps_json_get_user_data(rule_obj, &nl_msg.rule.usr_data);
	if (error < 0) {
		ppecfg_log_warn("Failed to parse user data\n");
		return error;
	}

	/*
	 * Parse additional parameters only for ADD operations
	 */
	if (msg_type == NSS_PPE_TUN_RPS_CREATE_RULE_MSG) {
		int hdr_len_type;
		int inner_pkt_type;

		/*
		 * Header length
		 */
		obj = ppecfg_get_json_object(rule_obj, "hdr_len");
		if (obj) {
			nl_msg.rule.hdr_len = json_object_get_int(obj);
		}

		/*
		 * Validate header length upper bound
		 */
		if (nl_msg.rule.hdr_len > PPECFG_TUN_RPS_TPR_MAX_OFFSET) {
			ppecfg_log_warn("hdr_len %d exceeds maximum allowed value of %d\n",
					nl_msg.rule.hdr_len, PPECFG_TUN_RPS_TPR_MAX_OFFSET);
			return -EINVAL;
		}

		/*
		 * Validate that header length is even (multiple of 2) as required by hardware/SSDK
		 */
		if (nl_msg.rule.hdr_len % 2 != 0) {
			ppecfg_log_warn("hdr_len %d must be an even number (multiple of 2)\n", nl_msg.rule.hdr_len);
			return -EINVAL;
		}

		/*
		 * Header length type — validate before assigning to struct field
		 */
		hdr_len_type = ppecfg_tun_rps_json_get_hdr_len_type(rule_obj);
		if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID) {
			ppecfg_log_warn("hdr_len_type is mandatory for add operation\n");
			return -EINVAL;
		}
		nl_msg.rule.hdr_len_type = hdr_len_type;

		/*
		 * Inner packet type — validate before assigning to struct field
		 */
		inner_pkt_type = ppecfg_tun_rps_json_get_inner_pkt_type(rule_obj);
		if (inner_pkt_type == PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID) {
			ppecfg_log_warn("inner_pkt_type is mandatory for add operation\n");
			return -EINVAL;
		}
		nl_msg.rule.inner_pkt_type = inner_pkt_type;

		/*
		 * Validate UDF offsets and masks for consistency with Netlink and Driver
		 */
		for (int i = 0; i < 3; i++) {
			if (nl_msg.rule.usr_data.udf[i].data_en) {
				if (nl_msg.rule.usr_data.udf[i].data_offset > PPECFG_TUN_RPS_TPR_MAX_OFFSET) {
					ppecfg_log_warn("usr_data%d_offset %d exceeds maximum of %d\n",
							i + 1, nl_msg.rule.usr_data.udf[i].data_offset, PPECFG_TUN_RPS_TPR_MAX_OFFSET);
					return -EINVAL;
				}
				if (nl_msg.rule.usr_data.udf[i].data_offset % 2 != 0) {
					ppecfg_log_warn("usr_data%d_offset %d must be an even number (multiple of 2)\n",
							i + 1, nl_msg.rule.usr_data.udf[i].data_offset);
					return -EINVAL;
				}
				if (nl_msg.rule.usr_data.udf[i].data_mask == 0) {
					ppecfg_log_warn("usr_data%d_mask cannot be zero when enabled\n", i + 1);
					return -EINVAL;
				}
				if (nl_msg.rule.usr_data.udf[i].data_offset + 2 > nl_msg.rule.hdr_len) {
					ppecfg_log_warn("usr_data%d_offset %d + 2 exceeds hdr_len %d\n",
							i + 1, nl_msg.rule.usr_data.udf[i].data_offset, nl_msg.rule.hdr_len);
					return -EINVAL;
				}
				if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH &&
				    nl_msg.rule.usr_data.udf[i].data_offset < 14) {
					ppecfg_log_warn("usr_data%d_offset %d is within Ethernet header (< 14)\n",
							i + 1, nl_msg.rule.usr_data.udf[i].data_offset);
					return -EINVAL;
				}
			}
		}

		/*
		 * WAN interface
		 */
		obj = ppecfg_get_json_object(rule_obj, "wan_if");
		if (obj) {
			const char *wan_if_str = json_object_get_string(obj);
			if (wan_if_str) {
				strlcpy(nl_msg.rule.wan_if, wan_if_str, sizeof(nl_msg.rule.wan_if));
			}
		}

		if (strlen(nl_msg.rule.wan_if) == 0) {
			ppecfg_log_warn("wan_if is mandatory for add operation\n");
			return -EINVAL;
		}

		/*
		 * Inner IP protocol fields
		 */
		obj = ppecfg_get_json_object(rule_obj, "inner_ip_proto_offset");
		if (obj) {
			nl_msg.rule.inner_ip_proto.ip_proto_offset = json_object_get_int(obj);
		}

		obj = ppecfg_get_json_object(rule_obj, "inner_ipv4_proto_val");
		if (obj) {
			nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value = json_object_get_int(obj);
		}

		obj = ppecfg_get_json_object(rule_obj, "inner_ipv4_proto_mask");
		if (obj) {
			nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask = json_object_get_int(obj);
		}

		obj = ppecfg_get_json_object(rule_obj, "inner_ipv6_proto_val");
		if (obj) {
			nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value = json_object_get_int(obj);
		}

		obj = ppecfg_get_json_object(rule_obj, "inner_ipv6_proto_mask");
		if (obj) {
			nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask = json_object_get_int(obj);
		}

		if (inner_pkt_type == PPECFG_TUN_RPS_INNER_PKT_TYPE_IP) {
			uint16_t default_mask = 0xFFFF;

			/*
			 * user_data3 cannot be enabled when inner_pkt_type=ip
			 */
			if (nl_msg.rule.usr_data.udf[2].data_en) {
				ppecfg_log_warn("user_data3 cannot be enabled when inner_pkt_type=ip\n");
				return -EINVAL;
			}

			/*
			 * inner_ip_proto_offset is mandatory
			 */
			if (!ppecfg_get_json_object(rule_obj, "inner_ip_proto_offset")) {
				ppecfg_log_warn("inner_ip_proto_offset is mandatory when inner_pkt_type=ip\n");
				return -EINVAL;
			}

			/*
			 * Either IPv4, IPv6 protocol or both needs to be matched
			 */
			if (nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value == 0 &&
			    nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value == 0) {
				ppecfg_log_warn("At least one inner IP protocol value must be matched when inner_pkt_type=ip\n");
				return -EINVAL;
			}

			/*
			 * Ensure both masks are non-zero as required by hardware.
			 * If one mask is provided and the other is not, propagate the configured side's mask.
			 * If neither mask is provided, use 0xFFFF.
			 */
			if (ppecfg_get_json_object(rule_obj, "inner_ipv4_proto_mask")) {
				default_mask = nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask;
			} else if (ppecfg_get_json_object(rule_obj, "inner_ipv6_proto_mask")) {
				default_mask = nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask;
			}

			if (!ppecfg_get_json_object(rule_obj, "inner_ipv4_proto_mask")) {
				nl_msg.rule.inner_ip_proto.ip_proto_ipv4_mask = default_mask;
				if (!ppecfg_get_json_object(rule_obj, "inner_ipv4_proto_val")) {
					nl_msg.rule.inner_ip_proto.ip_proto_ipv4_value = 0;
				}
			}

			if (!ppecfg_get_json_object(rule_obj, "inner_ipv6_proto_mask")) {
				nl_msg.rule.inner_ip_proto.ip_proto_ipv6_mask = default_mask;
				if (!ppecfg_get_json_object(rule_obj, "inner_ipv6_proto_val")) {
					nl_msg.rule.inner_ip_proto.ip_proto_ipv6_value = 0;
				}
			}

			/*
			 * Validate offset bounds
			 */
			if (nl_msg.rule.inner_ip_proto.ip_proto_offset > PPECFG_TUN_RPS_TPR_MAX_OFFSET) {
				ppecfg_log_warn("inner_ip_proto_offset %d exceeds maximum of %d\n",
						nl_msg.rule.inner_ip_proto.ip_proto_offset, PPECFG_TUN_RPS_TPR_MAX_OFFSET);
				return -EINVAL;
			}

			/*
			 * Validate that inner IP protocol offset is an even number (multiple of 2)
			 */
			if (nl_msg.rule.inner_ip_proto.ip_proto_offset % 2 != 0) {
				ppecfg_log_warn("inner_ip_proto_offset %d must be an even number (multiple of 2)\n",
						nl_msg.rule.inner_ip_proto.ip_proto_offset);
				return -EINVAL;
			}

			/*
			 * Validate that inner IP protocol offset does not exceed header length
			 */
			if (nl_msg.rule.inner_ip_proto.ip_proto_offset + 2 > nl_msg.rule.hdr_len) {
				ppecfg_log_warn("inner_ip_proto_offset %d + 2 exceeds hdr_len %d\n",
						nl_msg.rule.inner_ip_proto.ip_proto_offset, nl_msg.rule.hdr_len);
				return -EINVAL;
			}

			/*
			 * Validate that inner IP protocol offset is not within Ethernet header if type is eth
			 */
			if (hdr_len_type == PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH &&
			    nl_msg.rule.inner_ip_proto.ip_proto_offset < 14) {
				ppecfg_log_warn("inner_ip_proto_offset %d falls within Ethernet header (< 14)\n",
						nl_msg.rule.inner_ip_proto.ip_proto_offset);
				return -EINVAL;
			}
		}

		/*
		 * PPPoE fields
		 */
		obj = ppecfg_get_json_object(rule_obj, "pppoe_en");
		if (obj) {
			nl_msg.rule.pppoe_en = json_object_get_boolean(obj);
		}

		if (nl_msg.rule.pppoe_en) {
			obj = ppecfg_get_json_object(rule_obj, "pppoe_session_id");
			if (obj) {
				nl_msg.rule.pppoe_session_id = json_object_get_int(obj);
			} else {
				ppecfg_log_warn("pppoe_session_id is mandatory when pppoe_en is set\n");
				return -EINVAL;
			}

			obj = ppecfg_get_json_object(rule_obj, "pppoe_server_mac");
			if (obj) {
				const char *mac_str = json_object_get_string(obj);
				if (!mac_str || !ppecfg_param_verify_mac((char *)mac_str, nl_msg.rule.pppoe_server_mac)) {
					ppecfg_log_warn("Invalid pppoe_server_mac value\n");
					return -EINVAL;
				}
			} else {
				ppecfg_log_warn("pppoe_server_mac is mandatory when pppoe_en is set\n");
				return -EINVAL;
			}
		}
	}
	/*
	 * Note: For delete operations, hdr_len, hdr_len_type, inner_pkt_type,
	 * wan_if, inner IP proto fields, and PPPoE fields are not required.
	 */

	/*
	 * Send appropriate message
	 */
	if (msg_type == NSS_PPE_TUN_RPS_CREATE_RULE_MSG) {
		error = nss_ppenl_tun_rps_rule_add(&nl_msg);
		if (error < 0) {
			ppecfg_log_warn("Failed to create tunnel RPS rule from JSON, error: %d\n", error);
			return error;
		}
		ppecfg_log_info("Tunnel RPS rule added successfully from JSON\n");
	} else {
		error = nss_ppenl_tun_rps_rule_del(&nl_msg);
		if (error < 0) {
			ppecfg_log_warn("Failed to destroy tunnel RPS rule from JSON, error: %d\n", error);
			return error;
		}
		ppecfg_log_info("Tunnel RPS rule deleted successfully from JSON\n");
	}

	return 0;
}

/*
 * ppecfg_tun_rps_json_rule_handler()
 *	Unified JSON rule handler - supports both add and delete operations
 */
int ppecfg_tun_rps_json_rule_handler(struct json_object *rule_obj)
{
	struct json_object *cmd_obj;
	const char *cmd_str;
	enum nss_ppe_tun_rps_message_types msg_type;

	if (!rule_obj) {
		ppecfg_log_warn("Rule object is NULL\n");
		return -EINVAL;
	}

	/*
	 * Get command (default to add if not specified)
	 */
	cmd_obj = ppecfg_get_json_object(rule_obj, "cmd");
	if (cmd_obj) {
		cmd_str = (char*)json_object_get_string(cmd_obj);
		if (cmd_str && strcmp(cmd_str, "del_rule") == 0) {
			msg_type = NSS_PPE_TUN_RPS_DESTROY_RULE_MSG;
		} else if (cmd_str && strcmp(cmd_str, "add_rule") == 0) {
			msg_type = NSS_PPE_TUN_RPS_CREATE_RULE_MSG;
		} else {
			ppecfg_log_warn("Unknown command '%s'\n", cmd_str ? cmd_str : "(null)");
			return -EINVAL;
		}
	} else {
		/*
		 * Default to add if no command specified
		 */
		msg_type = NSS_PPE_TUN_RPS_CREATE_RULE_MSG;
	}

	/*
	 * Use shared processing function
	 */
	return ppecfg_tun_rps_json_process_rule(rule_obj, msg_type);
}
