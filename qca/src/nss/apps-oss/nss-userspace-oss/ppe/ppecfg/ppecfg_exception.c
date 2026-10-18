/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "ppecfg_exception.h"

#define PPECFG_EXCEPTION_CONFIG_DEF_PATH_LENGTH 100

/*
 * Default path for exception config file
 */
#define PPECFG_EXCEPTION_PARSER_PATH "/etc/ppecfg/ppecfg_exception_config.json"

struct ppecfg_param ppecfg_exception_params[PPECFG_EXCEPTION_PARAM_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_EXCEPTION_CONFIG_PATH, "path="),
};

/*
 * ppecfg_exception_get_flow_type()
 *	Converts the flow type to a 16 bit mask value
 */
static uint16_t ppecfg_exception_get_flow_type(const char *flow_type)
{
	uint16_t mask = 0;
	const char *delimiters = "|";
	char *input = strdup(flow_type);
	char *saveptr;
	char *token = strtok_r(input, delimiters, &saveptr);

	while (token != NULL) {
		while (*token == ' ') {
			token++;
		}
		char *end = token + strlen(token) - 1;
		while (*end == ' ' && end > token) {
			*end-- = '\0';
		}

		if (strcmp("L2_ONLY", token) == 0) {
			mask |= (1 << 0);
		} else if (strcmp("L3_ONLY", token) == 0) {
			mask |= (1 << 1);
		} else if (strcmp("L2_FLOW", token) == 0) {
			mask |= (1 << 2);
		} else if (strcmp("L3_FLOW", token) == 0) {
			mask |= (1 << 3);
		} else if (strcmp("MULTICAST_FLOW", token) == 0) {
			mask |= (1 << 4);
		} else if (strcmp("L2_FLOW_HIT", token) == 0) {
			mask |= (1 << 5);
		} else if (strcmp("L3_FLOW_HIT", token) == 0) {
			mask |= (1 << 6);
		} else if (strcmp("L2_FLOW_MISS", token) == 0) {
			mask |= (1 << 7);
		} else if (strcmp("L3_FLOW_MISS", token) == 0) {
			mask |= (1 << 8);
		} else if (strcmp("TUNNEL_FLOW", token) == 0) {
			mask |= (1 << 9);
		}

		token = strtok_r(NULL, delimiters, &saveptr);
	}

	return mask;
}

/*
 * ppecfg_exception_get_profile_mask()
 *	Converts the tunnel profile to a 8 bit mask value
 */
static uint8_t ppecfg_exception_get_profile_mask(const char *profile)
{
	uint8_t mask = 0;
	const char *delimiters = "|";
	char *input = strdup(profile);
	char *saveptr;
	char *token = strtok_r(input, delimiters, &saveptr);

	while (token != NULL) {
		while (*token == ' ') token++;
		char *end = token + strlen(token) - 1;
		while (*end == ' ' && end > token) *end-- = '\0';

		if (strcmp("0", token) == 0) {
			mask |= (1 << 0);
		} else if (strcmp("1", token) == 0) {
			mask |= (1 << 1);
		} else if (strcmp("2", token) == 0) {
			mask |= (1 << 2);
		} else if (strcmp("3", token) == 0) {
			mask |= (1 << 3);
		}

		token = strtok_r(NULL, delimiters, &saveptr);
	}

	return mask;
}

/*
 * ppecfg_exception_configure_params()
 *	Parse exception params and store in netlink message
 */
static int ppecfg_exception_configure_params(struct json_object *exception)
{
	struct nss_ppenl_exception_rule nl_msg = {{0}};
	const char *val;
	bool bool_val;
	uint16_t mask;
	uint8_t profile_mask;
	int error = 0;

	nss_ppenl_exception_init_rule(&nl_msg, NSS_PPENL_EXCEPTION_CONFIG_EXCEPTION_MSG);

	/*
	 * Exception code
	 */
	val = ppecfg_json_object_handler(exception, "code");
	if (val) {
		char exception_code[50];
		error = ppecfg_param_get_str(val, sizeof(exception_code), &exception_code);
		if (error) {
			ppecfg_log_error("code, %s\n", val);
			return error;
		}

		if (strcmp("UNKNOWN_L2_PROT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UNKNOWN_L2_PROT;
		} else if (strcmp("PPPOE_WRONG_VER_TYPE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PPPOE_WRONG_VER_TYPE;
		} else if (strcmp("PPPOE_WRONG_CODE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PPPOE_WRONG_CODE;
		} else if (strcmp("PPPOE_UNSUPPORTED_PPP_PROT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PPPOE_UNSUPPORTED_PPP_PROT;
		} else if (strcmp("IPV4_WRONG_VER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_WRONG_VER;
		} else if (strcmp("IPV4_SMALL_IHL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_SMALL_IHL;
		} else if (strcmp("IPV4_WITH_OPTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_WITH_OPTION;
		} else if (strcmp("IPV4_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_HDR_INCOMPLETE;
		} else if (strcmp("IPV4_BAD_TOTAL_LEN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_BAD_TOTAL_LEN;
		} else if (strcmp("IPV4_DATA_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_DATA_INCOMPLETE;
		} else if (strcmp("IPV4_FRAG", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_FRAG;
		} else if (strcmp("IPV4_PING_OF_DEATH", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_PING_OF_DEATH;
		} else if (strcmp("IPV4_SMALL_TTL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_SMALL_TTL;
		} else if (strcmp("IPV4_UNK_IP_PROT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_UNK_IP_PROT;
		} else if (strcmp("IPV4_CHECKSUM_ERR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_CHECKSUM_ERR;
		} else if (strcmp("IPV4_INV_SIP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_INV_SIP;
		} else if (strcmp("IPV4_INV_DIP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_INV_DIP;
		} else if (strcmp("IPV4_LAND_ATTACK", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_LAND_ATTACK;
		} else if (strcmp("IPV4_AH_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_AH_HDR_INCOMPLETE;
		} else if (strcmp("IPV4_AH_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_AH_HDR_CROSS_BORDER;
		} else if (strcmp("IPV4_ESP_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_ESP_HDR_INCOMPLETE;
		} else if (strcmp("IPV6_WRONG_VER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_WRONG_VER;
		} else if (strcmp("IPV6_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_HDR_INCOMPLETE;
		} else if (strcmp("IPV6_BAD_PAYLOAD_LEN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_BAD_PAYLOAD_LEN;
		} else if (strcmp("IPV6_DATA_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_DATA_INCOMPLETE;
		} else if (strcmp("IPV6_WITH_EXT_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_WITH_EXT_HDR;
		} else if (strcmp("IPV6_SMALL_HOP_LIMIT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_SMALL_HOP_LIMIT;
		} else if (strcmp("IPV6_INV_SIP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_INV_SIP;
		} else if (strcmp("IPV6_INV_DIP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_INV_DIP;
		} else if (strcmp("IPV6_LAND_ATTACK", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_LAND_ATTACK;
		} else if (strcmp("IPV6_FRAG", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_FRAG;
		} else if (strcmp("IPV6_PING_OF_DEATH", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_PING_OF_DEATH;
		} else if (strcmp("IPV6_WITH_MORE_EXT_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_WITH_MORE_EXT_HDR;
		} else if (strcmp("IPV6_UNK_LAST_NEXT_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_UNK_LAST_NEXT_HDR;
		} else if (strcmp("IPV6_MOBILITY_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_MOBILITY_HDR_INCOMPLETE;
		} else if (strcmp("IPV6_MOBILITY_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_MOBILITY_HDR_CROSS_BORDER;
		} else if (strcmp("IPV6_AH_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_AH_HDR_INCOMPLETE;
		} else if (strcmp("IPV6_AH_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_AH_HDR_CROSS_BORDER;
		} else if (strcmp("IPV6_ESP_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_ESP_HDR_INCOMPLETE;
		} else if (strcmp("IPV6_ESP_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_ESP_HDR_CROSS_BORDER;
		} else if (strcmp("IPV6_OTHER_EXT_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_OTHER_EXT_HDR_INCOMPLETE;
		} else if (strcmp("IPV6_OTHER_EXT_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_OTHER_EXT_HDR_CROSS_BORDER;
		} else if (strcmp("TCP_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_HDR_INCOMPLETE;
		} else if (strcmp("TCP_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_HDR_CROSS_BORDER;
		} else if (strcmp("TCP_SAME_SP_DP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_SAME_SP_DP;
		} else if (strcmp("TCP_SMALL_DATA_OFFSET", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_SMALL_DATA_OFFSET;
		} else if (strcmp("TCP_FLAGS_0", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_0;
		} else if (strcmp("TCP_FLAGS_1", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_1;
		} else if (strcmp("TCP_FLAGS_2", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_2;
		} else if (strcmp("TCP_FLAGS_3", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_3;
		} else if (strcmp("TCP_FLAGS_4", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_4;
		} else if (strcmp("TCP_FLAGS_5", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_5;
		} else if (strcmp("TCP_FLAGS_6", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_6;
		} else if (strcmp("TCP_FLAGS_7", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_FLAGS_7;
		} else if (strcmp("TCP_CHECKSUM_ERR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TCP_CHECKSUM_ERR;
		} else if (strcmp("UDP_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_HDR_INCOMPLETE;
		} else if (strcmp("UDP_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_HDR_CROSS_BORDER;
		} else if (strcmp("UDP_SAME_SP_DP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_SAME_SP_DP;
		} else if (strcmp("UDP_BAD_LEN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_BAD_LEN;
		} else if (strcmp("UDP_DATA_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_DATA_INCOMPLETE;
		} else if (strcmp("UDP_CHECKSUM_ERR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_CHECKSUM_ERR;
		} else if (strcmp("UDP_LITE_HDR_INCOMPLETE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_HDR_INCOMPLETE;
		} else if (strcmp("UDP_LITE_HDR_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_HDR_CROSS_BORDER;
		} else if (strcmp("UDP_LITE_SAME_SP_DP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_SAME_SP_DP;
		} else if (strcmp("UDP_LITE_CSM_COV_1_TO_7", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_CSM_COV_1_TO_7;
		} else if (strcmp("UDP_LITE_CSM_COV_TOO_LONG", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_CSM_COV_TOO_LONG;
		} else if (strcmp("UDP_LITE_CSM_COV_CROSS_BORDER", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_CSM_COV_CROSS_BORDER;
		} else if (strcmp("UDP_LITE_CHECKSUM_ERR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UDP_LITE_CHECKSUM_ERR;
		} else if (strcmp("FAKE_L2_PROT_ERR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_FAKE_L2_PROT_ERR;
		} else if (strcmp("FAKE_MAC_HEADER_ERR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_FAKE_MAC_HEADER_ERR;
		} else if (strcmp("BITMAP_MAX", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_BITMAP_MAX;
		} else if (strcmp("L2_EXP_MRU_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_EXP_MRU_FAIL;
		} else if (strcmp("L2_EXP_MTU_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_EXP_MTU_FAIL;
		} else if (strcmp("L3_EXP_IP_PREFIX_BC", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_IP_PREFIX_BC;
		} else if (strcmp("L3_EXP_MTU_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_MTU_FAIL;
		} else if (strcmp("L3_EXP_MRU_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_MRU_FAIL;
		} else if (strcmp("L3_EXP_ICMP_RDT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_ICMP_RDT;
		} else if (strcmp("L3_EXP_IP_RT_TTL1_TO_ME", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_IP_RT_TTL1_TO_ME;
		} else if (strcmp("L3_EXP_IP_RT_TTL_ZERO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_IP_RT_TTL_ZERO;
		} else if (strcmp("L3_FLOW_SERVICE_CODE_LOOP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_SERVICE_CODE_LOOP;
		} else if (strcmp("L3_FLOW_DE_ACCELEARTE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_DE_ACCELEARTE;
		} else if (strcmp("L3_EXP_FLOW_SRC_IF_CHK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_FLOW_SRC_IF_CHK_FAIL;
		} else if (strcmp("L3_FLOW_SYNC_TOGGLE_MISMATCH", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_SYNC_TOGGLE_MISMATCH;
		} else if (strcmp("L3_EXP_MTU_DF_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_MTU_DF_FAIL;
		} else if (strcmp("L3_EXP_PPPOE_MULTICAST", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_PPPOE_MULTICAST;
		} else if (strcmp("L3_EXP_FLOW_MTU_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_FLOW_MTU_FAIL;
		} else if (strcmp("L3_EXP_FLOW_MTU_DF_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_EXP_FLOW_MTU_DF_FAIL;
		} else if (strcmp("L3_UDP_CHECKSUM_0_EXP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_UDP_CHECKSUM_0_EXP;
		} else if (strcmp("MGMT_OFFSET", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_OFFSET;
		} else if (strcmp("MGMT_EAPOL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_EAPOL;
		} else if (strcmp("MGMT_PPPOE_DIS", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_PPPOE_DIS;
		} else if (strcmp("MGMT_IGMP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_IGMP;
		} else if (strcmp("MGMT_ARP_REQ", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_ARP_REQ;
		} else if (strcmp("MGMT_ARP_REP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_ARP_REP;
		} else if (strcmp("MGMT_DHCPv4", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_DHCPv4;
		} else if (strcmp("MGMT_LINKOAM", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_LINKOAM;
		} else if (strcmp("MGMT_MLD", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_MLD;
		} else if (strcmp("MGMT_NS", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_NS;
		} else if (strcmp("MGMT_NA", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_NA;
		} else if (strcmp("MGMT_DHCPv6", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_MGMT_DHCPv6;
		} else if (strcmp("PTP_OFFSET", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_OFFSET;
		} else if (strcmp("PTP_FOLLOW_UP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_FOLLOW_UP;
		} else if (strcmp("PTP_DELAY_REQ", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_DELAY_REQ;
		} else if (strcmp("PTP_DELAY_RESP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_DELAY_RESP;
		} else if (strcmp("PTP_PDELAY_REQ", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_PDELAY_REQ;
		} else if (strcmp("PTP_PDELAY_RESP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_PDELAY_RESP;
		} else if (strcmp("PTP_PDELAY_RESP_FOLLOW_UP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_PDELAY_RESP_FOLLOW_UP;
		} else if (strcmp("PTP_ANNONCE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_ANNONCE;
		} else if (strcmp("PTP_MANAGEMENT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_MANAGEMENT;
		} else if (strcmp("PTP_SIGNALING", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_SIGNALING;
		} else if (strcmp("PTP_PKT_RSV_MSG", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PTP_PKT_RSV_MSG;
		} else if (strcmp("IPV4_SG_UNKNOWN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_SG_UNKNOWN;
		} else if (strcmp("IPV6_SG_UNKNOWN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_SG_UNKNOWN;
		} else if (strcmp("ARP_SG_UNKNOWN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_ARP_SG_UNKNOWN;
		} else if (strcmp("ND_SG_UNKNOWN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_ND_SG_UNKNOWN;
		} else if (strcmp("IPV4_SG_VIO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV4_SG_VIO;
		} else if (strcmp("IPV6_SG_VIO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IPV6_SG_VIO;
		} else if (strcmp("ARP_SG_VIO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_ARP_SG_VIO;
		} else if (strcmp("ND_SG_VIO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_ND_SG_VIO;
		} else if (strcmp("L3_ROUTING_HOST_MISMATCH", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTING_HOST_MISMATCH;
		} else if (strcmp("L3_FLOW_SNAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_SNAT_ACTION;
		} else if (strcmp("L3_FLOW_DNAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_DNAT_ACTION;
		} else if (strcmp("L3_FLOW_RT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_RT_ACTION;
		} else if (strcmp("L3_FLOW_BR_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_BR_ACTION;
		} else if (strcmp("L3_MC_BRIDGE_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_MC_BRIDGE_ACTION;
		} else if (strcmp("L3_ROUTE_PREHEAD_RT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PREHEAD_RT_ACTION;
		} else if (strcmp("L3_ROUTE_PREHEAD_SNAPT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PREHEAD_SNAPT_ACTION;
		} else if (strcmp("L3_ROUTE_PREHEAD_DNAPT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PREHEAD_DNAPT_ACTION;
		} else if (strcmp("L3_ROUTE_PREHEAD_SNAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PREHEAD_SNAT_ACTION;
		} else if (strcmp("L3_ROUTE_PREHEAD_DNAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PREHEAD_DNAT_ACTION;
		} else if (strcmp("L3_NO_ROUTE_PREHEAD_NAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_NO_ROUTE_PREHEAD_NAT_ACTION;
		} else if (strcmp("L3_NO_ROUTE_PREHEAD_NAT_ERROR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_NO_ROUTE_PREHEAD_NAT_ERROR;
		} else if (strcmp("L3_ROUTE_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_ACTION;
		} else if (strcmp("L3_NO_ROUTE_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_NO_ROUTE_ACTION;
		} else if (strcmp("L3_NO_ROUTE_NH_INVALID_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_NO_ROUTE_NH_INVALID_ACTION;
		} else if (strcmp("L3_NO_ROUTE_PREHEAD_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_NO_ROUTE_PREHEAD_ACTION;
		} else if (strcmp("L3_BRIDGE_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_BRIDGE_ACTION;
		} else if (strcmp("L3_FLOW_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_ACTION;
		} else if (strcmp("L3_FLOW_MISS_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_FLOW_MISS_ACTION;
		} else if (strcmp("L2_NEW_MAC_ADDRESS", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_NEW_MAC_ADDRESS;
		} else if (strcmp("L2_HASH_COLLOSION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_HASH_COLLOSION;
		} else if (strcmp("L2_STATION_MOVE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_STATION_MOVE;
		} else if (strcmp("L2_LEARN_LIMIT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_LEARN_LIMIT;
		} else if (strcmp("L2_SA_LOOKUP_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_SA_LOOKUP_ACTION;
		} else if (strcmp("L2_DA_LOOKUP_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_DA_LOOKUP_ACTION;
		} else if (strcmp("APP_CTRL_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_APP_CTRL_ACTION;
		} else if (strcmp("IN_VLAN_FILTER_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IN_VLAN_FILTER_ACTION;
		} else if (strcmp("IN_VLAN_XLT_MISS", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_IN_VLAN_XLT_MISS;
		} else if (strcmp("EG_VLAN_FILTER_DROP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_EG_VLAN_FILTER_DROP;
		} else if (strcmp("ACL_PRE_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_ACL_PRE_ACTION;
		} else if (strcmp("ACL_POST_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_ACL_POST_ACTION;
		} else if (strcmp("SERVICE_CODE_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_SERVICE_CODE_ACTION;
		} else if (strcmp("L3_ROUTE_PRE_IPO_RT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PRE_IPO_RT_ACTION;
		} else if (strcmp("L3_ROUTE_PRE_IPO_SNAPT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PRE_IPO_SNAPT_ACTION;
		} else if (strcmp("L3_ROUTE_PRE_IPO_DNAPT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PRE_IPO_DNAPT_ACTION;
		} else if (strcmp("L3_ROUTE_PRE_IPO_SNAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PRE_IPO_SNAT_ACTION;
		} else if (strcmp("L3_ROUTE_PRE_IPO_DNAT_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L3_ROUTE_PRE_IPO_DNAT_ACTION;
		} else if (strcmp("TL_EXP_IF_CHECK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_IF_CHECK_FAIL;
		} else if (strcmp("TL_EXP_VLAN_CHECK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_VLAN_CHECK_FAIL;
		} else if (strcmp("TL_EXP_PPPOE_MC_TERM", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_PPPOE_MC_TERM;
		} else if (strcmp("TL_EXP_DE_ACCE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_DE_ACCE;
		} else if (strcmp("TL_UDP_CSUM_ZERO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_UDP_CSUM_ZERO;
		} else if (strcmp("TL_TTL_EXCEED", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_TTL_EXCEED;
		} else if (strcmp("TL_EXP_LPM_IF_CHECK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_LPM_IF_CHECK_FAIL;
		} else if (strcmp("TL_EXP_LPM_VLAN_CHECK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_LPM_VLAN_CHECK_FAIL;
		} else if (strcmp("TL_EXP_MAP_SRC_CHECK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_MAP_SRC_CHECK_FAIL;
		} else if (strcmp("TL_EXP_MAP_DST_CHECK_FAIL", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_MAP_DST_CHECK_FAIL;
		} else if (strcmp("TL_EXP_MAP_UDP_CSUM_ZERO", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_MAP_UDP_CSUM_ZERO;
		} else if (strcmp("TL_EXP_MAP_NON_TCP_UDP", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_EXP_MAP_NON_TCP_UDP;
		} else if (strcmp("TL_FWD_CMD", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TL_FWD_CMD;
		} else if (strcmp("L2_PRE_IPO_ACTION", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_PRE_IPO_ACTION;
		} else if (strcmp("L2_TUNL_CONTEXT_INVALID", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_L2_TUNL_CONTEXT_INVALID;
		} else if (strcmp("RESERVE0", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_RESERVE0;
		} else if (strcmp("RESERVE1", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_RESERVE1;
		} else if (strcmp("TUNNEL_DECAP_ECN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_TUNNEL_DECAP_ECN;
		} else if (strcmp("INNER_PACKET_TOO_SHORT", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_INNER_PACKET_TOO_SHORT;
		} else if (strcmp("VXLAN_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_VXLAN_HDR;
		} else if (strcmp("VXLAN_GPE_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_VXLAN_GPE_HDR;
		} else if (strcmp("GENEVE_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_GENEVE_HDR;
		} else if (strcmp("GRE_HDR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_GRE_HDR;
		} else if (strcmp("GRE_CSUM", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_GRE_CSUM;
		} else if (strcmp("UNKNOWN_INNER_TYPE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_UNKNOWN_INNER_TYPE;
		} else if (strcmp("FLAG_VXLAN", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_FLAG_VXLAN;
		} else if (strcmp("FLAG_VXLAN_GPE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_FLAG_VXLAN_GPE;
		} else if (strcmp("FLAG_GRE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_FLAG_GRE;
		} else if (strcmp("FLAG_GENEVE", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_FLAG_GENEVE;
		} else if (strcmp("PROGRAM0", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PROGRAM0;
		} else if (strcmp("PROGRAM1", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PROGRAM1;
		} else if (strcmp("PROGRAM2", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PROGRAM2;
		} else if (strcmp("PROGRAM3", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PROGRAM3;
		} else if (strcmp("PROGRAM4", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PROGRAM4;
		} else if (strcmp("PROGRAM5", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_PROGRAM5;
		} else if (strcmp("CPU_CODE_EG_MIRROR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_CPU_CODE_EG_MIRROR;
		} else if (strcmp("CPU_CODE_IN_MIRROR", exception_code) == 0) {
			nl_msg.info.code = PPE_DRV_CC_CPU_CODE_IN_MIRROR;
		} else {
			ppecfg_log_error("Invalide exception code, %s\n", exception_code);
			return error;
		}

		ppecfg_log_info("EXCEPTION CODE: %s\n", val);
	} else {
		ppecfg_log_error("No exception code present!!\n");
		return error;
	}

	/*
	 * Flush is enabled
	 */
	val = ppecfg_json_object_handler(exception, "flush_en");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("flush_en, %s\n", val);
			return error;
		}

		if (bool_val == true) {
			nl_msg.info.flush_en = bool_val;
		}

		bool_val = false;
		ppecfg_log_info("FLUSH ENABLED: %s\n", val);
	} else {
		ppecfg_log_error("Please mention if flush is enabled or not, true/false!!\n");
		return error;
	}

	/*
	 * Deaccel is enabled
	 */
	val = ppecfg_json_object_handler(exception, "deaccel_en");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("deaccel_en, %s\n", val);
			return error;
		}

		if (bool_val == true) {
			nl_msg.info.deaccel_en = bool_val;
		}

		bool_val = false;
		ppecfg_log_info("DEACCEL ENABLED: %s\n", val);
	} else {
		ppecfg_log_error("Please mention if deacceleration is enabled or not, true/false!!\n");
		return error;
	}

	/*
	 * Exception packet edit enable/disable.
	 */
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	val = ppecfg_json_object_handler(exception, "exception_edit_en");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("exception_edit_en, %s\n", val);
			return error;
		}

		nl_msg.info.exception_edit_en = bool_val;
		ppecfg_log_info("EXCEPTION EDIT ENABLED: %s\n", val);
	} else {
		ppecfg_log_error("Please mention if packet exception edit is enabled or not, true/false!!\n");
		return error;
	}
#endif

	/*
	 * Flow type for the exception
	 */
	val = ppecfg_json_object_handler(exception, "flow_type");
	if (val) {
		mask = ppecfg_exception_get_flow_type(val);
		if (!mask) {
			ppecfg_log_error("flow_type, %s\n", val);
			return error;
		} else {
			nl_msg.info.flow_type = mask;
		}

		ppecfg_log_info("FLOW TYPE: %s\n", val);
	} else {
		ppecfg_log_error("No flow type provided, provide different types in this way - l2_only|l3_only|l2_flow|l3_flow|l2_flow_hit...!!\n");
		return error;
	}

	/*
	 * Action for the exception
	 */
	val = ppecfg_json_object_handler(exception, "action");
	if (val) {
		if (strcmp("FWD", val) == 0) {
			nl_msg.info.action = PPE_DRV_CC_USR_ACTION_FWD;
		} else if (strcmp("DROP", val) == 0) {
			nl_msg.info.action = PPE_DRV_CC_USR_ACTION_DROP;
		} else if (strcmp("COPY", val) == 0) {
			nl_msg.info.action = PPE_DRV_CC_USR_ACTION_COPY;
		} else if (strcmp("REDIR", val) == 0) {
			nl_msg.info.action = PPE_DRV_CC_USR_ACTION_REDIR;
		} else {
			ppecfg_log_error("action, %s\n", val);
			return error;
		}

		ppecfg_log_info("ACTION : %s\n", val);
	} else {
		ppecfg_log_error("No action provided, DROP/FWD/COPY/REDIR!!\n");
		return error;
	}

	/*
	 * Tunnel profile if flow type is tunnel flow
	 */
	if (mask & PPECFG_EXCEPTION_TUNNEL_FLOW_TYPE) {
		val = ppecfg_json_object_handler(exception, "tun_profile");
		if (val) {
			profile_mask = ppecfg_exception_get_profile_mask(val);
			if (!profile_mask) {
				ppecfg_log_error("tun_profile, %s\n", val);
				return error;
			} else {
				nl_msg.info.tun_profile = profile_mask;
			}

			ppecfg_log_info("TUN PROFILE: %s\n", val);
		}
	} else {
		val = ppecfg_json_object_handler(exception, "tun_profile");
		if (val) {
			ppecfg_log_error("The flow_type is not of Tunnel type, no profile needed!!\n");
			return error;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_exception_rule_config(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		return error;
	}

	return error;
}

/*
 * ppecfg_exception_parser()
 * 	Handles JSON config provided by user
 */
int ppecfg_exception_parser(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct ppecfg_param *sub_params;
	struct stat filestat;
	struct json_object *exception_list;
	struct json_object *jobj;
	struct json_object *exception;
	struct json_object *current_rule;
	char *json_data = NULL;
	char path[PPECFG_EXCEPTION_CONFIG_DEF_PATH_LENGTH];
	FILE *fp;
	int error = 0;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	/*
	 * Iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_info("default configuration\n");
		memcpy(path, PPECFG_EXCEPTION_PARSER_PATH, strlen(PPECFG_EXCEPTION_PARSER_PATH));
	} else {
		sub_params = &param->sub_params[PPECFG_EXCEPTION_CONFIG_PATH];
		memcpy(path, sub_params->data, strlen(sub_params->data));
		ppecfg_log_info("New path for configuration: %s\n", path);
	}

	fp = fopen(path, "rb");
	if (!fp) {
		ppecfg_log_error("Failed to open file\n");
		return -EACCES;
	}

	if (stat(path, &filestat) != 0) {
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
	exception_list = ppecfg_get_json_object(jobj, "exceptions");
	if (exception_list == NULL) {
		ppecfg_log_error("Error: No exception present!\n");
		free(json_data);
		json_object_put(jobj);
		return -EINVAL;
	}

	int exception_count = json_object_array_length(exception_list);

	for (int i = 0; i < exception_count; i++) {
		current_rule = json_object_array_get_idx(exception_list, i);
		exception = ppecfg_get_json_object(current_rule, "exception");
		if (exception == NULL) {
			ppecfg_log_warn("Enter a valid type of exception\n");
			continue;
		}

		error = ppecfg_exception_configure_params(exception);
		if (error) {
			return error;
		}
	}

	return error;
}
