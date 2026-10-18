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

#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include "ppecfg_param.h"
#include "ppecfg_acl_json_parser.h"

/*
 * ppecfg_acl_json_get_smac_obj()
 * 	Get smac values from JSON object
 */
static bool ppecfg_acl_json_get_smac_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "smac");
	if (obj == NULL) {
		return true;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SMAC_VALID;

	char *smac_val = ppecfg_json_object_handler(obj, "smac_val");
	char *smac_nval = ppecfg_json_object_handler(obj, "smac_nval");
	char *smac_mask = ppecfg_json_object_handler(obj, "smac_mask");

	if ((smac_val && smac_nval) || (!smac_val && !smac_nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return false;
	}

	if (smac_val) {
		error = ppecfg_param_verify_mac(smac_val, nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac);
		if (!error) {
			ppecfg_log_error("smac_val, %s\n", smac_val);
			return error;
		}
		ppecfg_log_info("SMAC VAL : %s\n", smac_val);
	}

	if (smac_nval) {
		error = ppecfg_param_verify_mac(smac_nval, nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac);
		if (!error) {
			ppecfg_log_error("smac_nval, %s\n", smac_nval);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
		ppecfg_log_info("SMAC NVAL : %s\n", smac_nval);
	}

	if (smac_mask) {
		error = ppecfg_param_verify_mac(smac_mask, nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac_mask);
		if (!error) {
			ppecfg_log_error("smac_mask, %s\n", smac_mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule_flags |= PPE_ACL_RULE_FLAG_MAC_MASK;
		ppecfg_log_info("SMAC MASK : %s\n", smac_mask);
	}

	return true;
}

/*
 * ppecfg_acl_json_get_dmac_obj()
 * 	Get dmac values from JSON object
 */
static bool ppecfg_acl_json_get_dmac_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "dmac");
	if (obj == NULL) {
		return true;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DMAC_VALID;

	char *dmac_val = ppecfg_json_object_handler(obj, "dmac_val");
	char *dmac_nval = ppecfg_json_object_handler(obj, "dmac_nval");
	char *dmac_mask = ppecfg_json_object_handler(obj, "dmac_mask");

	if ((dmac_val && dmac_nval) || (!dmac_val && !dmac_nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return false;
	}

	if (dmac_val) {
		error = ppecfg_param_verify_mac(dmac_val, nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac);
		if (!error) {
			ppecfg_log_error("dmac_val, %s\n", dmac_val);
			return error;
		}
		ppecfg_log_info("DMAC VAL : %s\n", dmac_val);
	}

	if (dmac_nval) {
		error = ppecfg_param_verify_mac(dmac_nval, nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac);
		if (!error) {
			ppecfg_log_error("dmac_nval, %s\n", dmac_nval);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
		ppecfg_log_info("DMAC NVAL : %s\n", dmac_nval);
	}

	if (dmac_mask) {
		error = ppecfg_param_verify_mac(dmac_mask, nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac_mask);
		if (!error) {
			ppecfg_log_error("dmac_mask, %s\n", dmac_mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule_flags |= PPE_ACL_RULE_FLAG_MAC_MASK;
		ppecfg_log_info("DMAC MASK : %s\n", dmac_mask);
	}

	return true;
}

/*
 * ppecfg_acl_json_get_cvid_obj()
 * 	Get cvid values from JSON object
 */
static int ppecfg_acl_json_get_cvid_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	bool bool_val = false;

	obj = ppecfg_get_json_object(rule_obj, "cvid");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_CVID_VALID;

	char *ctag = ppecfg_json_object_handler(obj, "cvid_tagged");
	char *val = ppecfg_json_object_handler(obj, "cvid_val");
	char *mask = ppecfg_json_object_handler(obj, "cvid_mask");
	char *range = ppecfg_json_object_handler(obj, "cvid_range");

	error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.vid_min);
	if (error) {
		ppecfg_log_info("cvid_val, %s\n",val);
		return error;
	}

	ppecfg_log_info("CVID VAL : %s\n", val);

	if (ctag) {
		error = ppecfg_param_get_int(ctag, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.tag_fmt);
		if (error) {
			ppecfg_log_error("cvid_tagged, %s\n", ctag);
			return error;
		}
		ppecfg_log_info("CVID TAGGED : %s\n", ctag);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.vid_mask_max);
		if (error) {
			ppecfg_log_error("cvid_mask, %s\n", mask);
			return error;
		}
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_MASK;
		ppecfg_log_info("CVID MASK : %s\n", mask);
	}

	if (range) {
		error = ppecfg_param_get_bool(range, &bool_val);
		if (error) {
			ppecfg_log_error("cvid_range, %s\n", range);
			return error;
		}
		if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags & PPE_ACL_RULE_FLAG_VID_MASK)) {
			return error;
		}
		if (bool_val == true) {
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_RANGE;
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags &= ~PPE_ACL_RULE_FLAG_VID_MASK;
		}
		ppecfg_log_info("CVID RANGE : %s\n", range);
	}

	bool_val = false;
	return error;
}

/*
 * ppecfg_acl_json_get_svid_obj()
 * 	Get svid values from JSON object
 */
static int ppecfg_acl_json_get_svid_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	bool bool_val = false;

	obj = ppecfg_get_json_object(rule_obj, "svid");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SVID_VALID;

	char *stag = ppecfg_json_object_handler(obj, "svid_tagged");
	char *val = ppecfg_json_object_handler(obj, "svid_val");
	char *mask = ppecfg_json_object_handler(obj, "svid_mask");
	char *range = ppecfg_json_object_handler(obj, "svid_range");

	error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.vid_min);
	if (error) {
		ppecfg_log_info("svid_val, %s\n", val);
		return error;
	}
	ppecfg_log_info("SVID VAL : %s\n", val);

	if (stag) {
		error = ppecfg_param_get_int(stag, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.tag_fmt);
		if (error) {
			ppecfg_log_error("svid_tagged, %s\n", stag);
			return error;
		}
		ppecfg_log_info("SVID TAGGED : %s\n", stag);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.vid_mask_max);
		if (error) {
			ppecfg_log_error("svid_mask, %s\n", mask);
			return error;
		}
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_MASK;
		ppecfg_log_info("SVID MASK : %s\n", mask);
	}

	if (range) {
		error = ppecfg_param_get_bool(range, &bool_val);
		if (error) {
			ppecfg_log_error("svid_range, %s\n", range);
			return error;
		}
		if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags & PPE_ACL_RULE_FLAG_VID_MASK)) {
			return error;
		}
		if (bool_val == true) {
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_RANGE;
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags &= ~PPE_ACL_RULE_FLAG_VID_MASK;
		}
		ppecfg_log_info("SVID RANGE : %s\n", range);
	}

	bool_val = false;
	return error;
}

/*
 * ppecfg_acl_json_get_cpcp_obj()
 * 	Get cpcp values from JSON object
 */
static int ppecfg_acl_json_get_cpcp_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "cpcp");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_CPCP_VALID;

	char *min = ppecfg_json_object_handler(obj, "cpcp_min");
	char *mask = ppecfg_json_object_handler(obj, "cpcp_mask");

	error = ppecfg_param_get_int(min, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule.cpcp.pcp);
	if (error) {
		ppecfg_log_info("cpcp_min, %s\n", min);
		return error;
	}

	ppecfg_log_info("CPCP MIN : %s\n", min);

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule.cpcp.pcp_mask);
		if (error) {
			ppecfg_log_error("cpcp_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule_flags |= PPE_ACL_RULE_FLAG_PCP_MASK;
		ppecfg_log_info("CPCP MASK : %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_spcp_obj()
 * 	Get spcp values from JSON object
 */
static int ppecfg_acl_json_get_spcp_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "spcp");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SPCP_VALID;

	char *min = ppecfg_json_object_handler(obj, "spcp_min");
	char *mask = ppecfg_json_object_handler(obj, "spcp_mask");

	error = ppecfg_param_get_int(min, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule.spcp.pcp);
	if (error) {
		ppecfg_log_info("spcp_min, %s\n", min);
		return error;
	}

	ppecfg_log_info("SPCP MIN : %s\n", min);

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule.spcp.pcp_mask);
		if (error) {
			ppecfg_log_error("spcp_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule_flags |= PPE_ACL_RULE_FLAG_PCP_MASK;
		ppecfg_log_info("SPCP MASK : %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_pppoe_obj()
 * 	Get pppoe values from JSON object
 */
static int ppecfg_acl_json_get_pppoe_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "pppoe");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS_VALID;

	char *val = ppecfg_json_object_handler(obj, "pppoe_session");
	char *nval = ppecfg_json_object_handler(obj, "pppoe_nsession");
	char *mask = ppecfg_json_object_handler(obj, "pppoe_mask");

	if ((val && nval) || (!val && !nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return -EINVAL;
	}

	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule.pppoe_sess.pppoe_session_id);
		if (error) {
			ppecfg_log_error("pppoe_session, %s\n", val);
			return error;
		}
		ppecfg_log_info("PPPOE VAL: %s\n", val);
	}

	if (nval) {
		error = ppecfg_param_get_int(nval, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule.pppoe_sess.pppoe_session_id);
		if (error) {
			ppecfg_log_error("pppoe_nsession, %s\n", nval);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
		ppecfg_log_info("PPPOE NVAL: %s\n", nval);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule.pppoe_sess.pppoe_session_id_mask);
		if (error) {
			ppecfg_log_error("pppoe_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule_flags |= PPE_ACL_RULE_FLAG_PPPOE_MASK;
		ppecfg_log_info("PPPOE MASK: %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_ether_obj()
 * 	Get ether values from JSON object
 */
static int ppecfg_acl_json_get_ether_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "ether_type");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE_VALID;

	char *val = ppecfg_json_object_handler(obj, "l4_proto_val");
	char *nval = ppecfg_json_object_handler(obj, "l4_proto_nval");
	char *mask = ppecfg_json_object_handler(obj, "l4_proto_mask");

	if ((val && nval) || (!val && !nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return -EINVAL;
	}

	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto);
		if (error) {
			ppecfg_log_error("l4_proto_val, %s\n", val);
			return error;
		}
		ppecfg_log_info("l4 PROTO: %s\n", val);
	}

	if (nval) {
		error = ppecfg_param_get_int(nval, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto);
		if (error) {
			ppecfg_log_error("l4_proto_nval, %s\n", nval);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
		ppecfg_log_info("l4 PROTO NVAL: %s\n", nval);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto_mask);
		if (error) {
			ppecfg_log_error("l4_proto_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule_flags |= PPE_ACL_RULE_FLAG_ETHTYPE_MASK;
		ppecfg_log_info("l4 PROTO MASK: %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_tos_obj()
 * 	Get tos values from JSON object
 */
static int ppecfg_acl_json_get_tos_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "tos_tc");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_TOS_TC_VALID;

	char *min = ppecfg_json_object_handler(obj, "tos_min");
	char *mask = ppecfg_json_object_handler(obj, "tos_mask");

	error = ppecfg_param_get_int(min, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule.tos_tc.l3_tos_tc);
	if (error) {
		ppecfg_log_info("tos_min, %s\n",min);
		return error;
	}

	ppecfg_log_info("TOS MIN : %s\n", min);

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule.tos_tc.l3_tos_tc_mask);
		if (error) {
			ppecfg_log_error("tos_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule_flags |= PPE_ACL_RULE_FLAG_TOS_TC_MASK;
		ppecfg_log_info("TOS MASK : %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_sip_obj()
 * 	Get sip values from JSON object
 */
static int ppecfg_acl_json_get_sip_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	uint8_t is_v6;

	obj = ppecfg_get_json_object(rule_obj, "sip");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SIP_VALID;

	char *type = ppecfg_json_object_handler(obj, "sip_is_v6");
	char *val = ppecfg_json_object_handler(obj, "sip_val");
	char *nval = ppecfg_json_object_handler(obj, "sip_nval");
	char *mask = ppecfg_json_object_handler(obj, "sip_mask");

	if ((val && nval) || (!val && !nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return -EINVAL;
	}

	error = ppecfg_param_get_int(type, sizeof(uint8_t), &is_v6);
	if (error) {
		ppecfg_log_info("sip_is_v6, %s\n", type);
		return error;
	}
	ppecfg_log_info("SIP TYPE: %s\n", type);

	if (is_v6 == 1) {
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_type = PPE_ACL_IP_TYPE_V6;
		ppecfg_log_trace("Ipv6 address : %pI6h\n", &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
	} else if (is_v6 == 0) {
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_type = PPE_ACL_IP_TYPE_V4;
		ppecfg_log_trace("Ipv4 address :%pI4h\n", &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
	} else {
		ppecfg_log_error("sip_is_v6");
		return -EINVAL;
	}

	if (is_v6 == 1) {
		if (val) {
			error = ppecfg_param_get_ipaddr(val, sizeof(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
			if (error) {
				ppecfg_log_error("sip_val, %s\n", val);
				return error;
			}

			ppecfg_log_info("SIP VAL: %s\n", val);
		}

		if (nval) {
			error = ppecfg_param_get_ipaddr(nval, sizeof(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
			if (error) {
				ppecfg_log_error("sip_nval, %s\n", nval);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			ppecfg_log_info("SIP NVAL: %s\n", nval);
		}

		if (mask) {
			error = ppecfg_param_get_ipaddr(mask, sizeof(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_mask), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_mask);
			if (error) {
				ppecfg_log_error("sip_mask, %s\n", mask);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_FLAG_SIP_MASK;
			ppecfg_log_info("SIP MASK: %s\n", mask);
		}
	}

	if (is_v6 == 0) {
		if (val) {
			error = ppecfg_param_get_ipaddr(val, sizeof(uint32_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip[0]);
			if (error) {
				ppecfg_log_error("sip_val, %s\n", val);
				return error;
			}

			ppecfg_log_info("SIP VAL: %s\n", val);
		}

		if (nval) {
			error = ppecfg_param_get_ipaddr(nval, sizeof(uint32_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip[0]);
			if (error) {
				ppecfg_log_error("sip_nval, %s\n", nval);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			ppecfg_log_info("SIP NVAL: %s\n", nval);
		}

		if (mask) {
			error = ppecfg_param_get_ipaddr(mask, sizeof(uint32_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_mask[0]);
			if (error) {
				ppecfg_log_error("sip_mask, %s\n", mask);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_FLAG_SIP_MASK;
			ppecfg_log_info("SIP MASK: %s\n", mask);
		}
	}

	return error;
}

/*
 * ppecfg_acl_json_get_dip_obj()
 * 	Get dip values from JSON object
 */
static int ppecfg_acl_json_get_dip_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	uint8_t is_v6;

	obj = ppecfg_get_json_object(rule_obj, "dip");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DIP_VALID;

	char *type = ppecfg_json_object_handler(obj, "dip_is_v6");
	char *val = ppecfg_json_object_handler(obj, "dip_val");
	char *nval = ppecfg_json_object_handler(obj, "dip_nval");
	char *mask = ppecfg_json_object_handler(obj, "dip_mask");

	if ((val && nval) || (!val && !nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return -EINVAL;
	}

	error = ppecfg_param_get_int(type, sizeof(uint8_t), &is_v6);
	if (error) {
		ppecfg_log_info("dip_is_v6, %s\n", type);
		return error;
	}
	ppecfg_log_info("DIP TYPE: %s\n", type);

	if (is_v6 == 1) {
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_type = PPE_ACL_IP_TYPE_V6;
	} else if (is_v6 == 0) {
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_type = PPE_ACL_IP_TYPE_V4;
	} else {
		ppecfg_log_error("dip_is_v6");
		return -EINVAL;
	}

	if (is_v6 == 1) {
		if (val) {
			error = ppecfg_param_get_ipaddr(val, sizeof(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip);
			if (error) {
				ppecfg_log_error("dip_val, %s\n", val);
				return error;
			}

			ppecfg_log_info("DIP VAL: %s\n", val);
		}

		if (nval) {
			error = ppecfg_param_get_ipaddr(nval, sizeof(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip);
			if (error) {
				ppecfg_log_error("dip_nval, %s\n", nval);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			ppecfg_log_info("DIP NVAL: %s\n", nval);
		}

		if (mask) {
			error = ppecfg_param_get_ipaddr(mask, sizeof(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_mask), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_mask);
			if (error) {
				ppecfg_log_error("dip_mask, %s\n", mask);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_FLAG_DIP_MASK;
			ppecfg_log_info("DIP MASK: %s\n", mask);
		}
	}

	if (is_v6 == 0) {
		if (val) {
			error = ppecfg_param_get_ipaddr(val, sizeof(uint32_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip[0]);
			if (error) {
				ppecfg_log_error("dip_val, %s\n", val);
				return error;
			}

			ppecfg_log_info("DIP VAL: %s\n", val);
		}

		if (nval) {
			error = ppecfg_param_get_ipaddr(nval, sizeof(uint32_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip[0]);
			if (error) {
				ppecfg_log_error("dip_nval, %s\n", nval);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			ppecfg_log_info("DIP NVAL: %s\n", nval);
		}

		if (mask) {
			error = ppecfg_param_get_ipaddr(mask, sizeof(uint32_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_mask[0]);
			if (error) {
				ppecfg_log_error("dip_mask, %s\n", mask);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_FLAG_DIP_MASK;
			ppecfg_log_info("DIP MASK: %s\n", mask);
		}
	}

	return error;
}

/*
 * ppecfg_acl_json_get_sport_obj()
 * 	Get sport values from JSON object
 */
static int ppecfg_acl_json_get_sport_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	uint16_t sport_val;
	bool bool_val = false;

	obj = ppecfg_get_json_object(rule_obj, "sport");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SPORT_VALID;

	char *val = ppecfg_json_object_handler(obj, "sport_val");
	char *nval = ppecfg_json_object_handler(obj, "sport_nval");
	char *mask = ppecfg_json_object_handler(obj, "sport_mask");
	char *range = ppecfg_json_object_handler(obj, "sport_range");

	if ((val && nval) || (!val && !nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return -EINVAL;
	}

	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &sport_val);
		if (error) {
			ppecfg_log_error("sport_val, %s\n", val);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule.sport.l4_port_min = ntohs(sport_val);
		ppecfg_log_info("SPORT VAL: %s\n", val);
	}

	if (nval) {
		error = ppecfg_param_get_int(nval, sizeof(uint16_t), &sport_val);
		if (error) {
			ppecfg_log_error("sport_nval, %s\n", nval);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule.sport.l4_port_min = ntohs(sport_val);
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
		ppecfg_log_info("SPORT NVAL: %s\n", nval);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &sport_val);
		if (error) {
			ppecfg_log_error("sport_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule.sport.l4_port_max_mask = ntohs(sport_val);
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags |= PPE_ACL_RULE_FLAG_SPORT_MASK;
		ppecfg_log_info("SPORT MASK: %s\n", mask);
	}

	if (range) {
		error = ppecfg_param_get_bool(range, &bool_val);
		if (error) {
			ppecfg_log_error("sport_range, %s\n", range);
			return error;
		}

		if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags & PPE_ACL_RULE_FLAG_SPORT_MASK)) {
			ppecfg_log_error("sport_range, %s\n", range);
			return error;
		}

		if (bool_val == true) {
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags |= PPE_ACL_RULE_FLAG_SPORT_RANGE;
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags &= ~PPE_ACL_RULE_FLAG_SPORT_MASK;
		}

		bool_val = false;
		ppecfg_log_info("SPORT MASK: %s\n", range);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_dport_obj()
 * 	Get dport values from JSON object
 */
static int ppecfg_acl_json_get_dport_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	uint16_t dport_val;
	bool bool_val = false;

	obj = ppecfg_get_json_object(rule_obj, "dport");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DPORT_VALID;

	char *val = ppecfg_json_object_handler(obj, "dport_val");
	char *nval = ppecfg_json_object_handler(obj, "dport_nval");
	char *mask = ppecfg_json_object_handler(obj, "dport_mask");
	char *range = ppecfg_json_object_handler(obj, "dport_range");

	if ((val && nval) || (!val && !nval)) {
		ppecfg_log_info("Error: Either val or nval is required\n");
		return -EINVAL;
	}

	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &dport_val);
		if (error) {
			ppecfg_log_error("dport_val, %s\n", val);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule.dport.l4_port_min = ntohs(dport_val);
		ppecfg_log_info("DPORT VAL: %s\n", val);
	}

	if (nval) {
		error = ppecfg_param_get_int(nval, sizeof(uint16_t), &dport_val);
		if (error) {
			ppecfg_log_error("dport_nval, %s\n", nval);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule.dport.l4_port_min = ntohs(dport_val);
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
		ppecfg_log_info("DPORT NVAL: %s\n", nval);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &dport_val);
		if (error) {
			ppecfg_log_error("dport_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule.dport.l4_port_max_mask = ntohs(dport_val);
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags |= PPE_ACL_RULE_FLAG_DPORT_MASK;
		ppecfg_log_info("DPORT MASK: %s\n", mask);
	}

	if (range) {
		error = ppecfg_param_get_bool(range, &bool_val);
		if (error) {
			ppecfg_log_error("dport_range, %s\n", range);
			return error;
		}

		if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags & PPE_ACL_RULE_FLAG_DPORT_MASK)) {
			ppecfg_log_error("dport_range, %s\n", range);
			return -EINVAL;
		}

		if (bool_val == true) {
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags |= PPE_ACL_RULE_FLAG_DPORT_RANGE;
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags &= ~PPE_ACL_RULE_FLAG_DPORT_MASK;
		}

		bool_val = false;
		ppecfg_log_info("DPORT MASK: %s\n", range);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_ttl_obj()
 * 	Get ttl values from JSON object
 */
static int ppecfg_acl_json_get_ttl_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "ttl_hop");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT_VALID;

	char *limit = ppecfg_json_object_handler(obj, "ttl_limit");
	char *mask = ppecfg_json_object_handler(obj, "ttl_limit_mask");

	error = ppecfg_param_get_int(limit, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT].rule.ttl_hop.hop_limit);
	if (error) {
		ppecfg_log_info("ttl_limit, %s\n", limit);
		return error;
	}

	ppecfg_log_info("TTL LIMIT: %s\n", limit);

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT].rule.ttl_hop.hop_limit_mask);
		if (error) {
			ppecfg_log_error("ttl_limit_mask, %s\n", mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT].rule_flags |= PPE_ACL_RULE_FLAG_TTL_HOPLIMIT_MASK;
		ppecfg_log_info("TTL LIMIT MASK: %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_l3_len_obj()
 * 	Get l3 len values from JSON object
 */
static int ppecfg_acl_json_get_l3_len_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	bool bool_val = false;

	obj = ppecfg_get_json_object(rule_obj, "l3_len");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_IP_LEN_VALID;

	char *val = ppecfg_json_object_handler(obj, "l3_len_val");
	char *mask = ppecfg_json_object_handler(obj, "l3_len_mask");
	char *range = ppecfg_json_object_handler(obj, "l3_len_range");

	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule.l3_len.l3_length_min);
		if (error) {
			ppecfg_log_error("l3_len_val, %s\n", val);
			return error;
		}
		ppecfg_log_info("L3_LEN_VAL: %s\n", val);
	}

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule.l3_len.l3_length_mask_max);
		if (error) {
			ppecfg_log_error("l3_len_mask, %s\n", mask);
			return error;
		}
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags |= PPE_ACL_RULE_FLAG_IPLEN_MASK;
		ppecfg_log_info("L3_LEN_MASK: %s\n", mask);
	}

	if (range) {
		error = ppecfg_param_get_bool(range, &bool_val);
		if (error) {
			ppecfg_log_error("l3_len_range, %s\n", range);
			return error;
		}
		if (bool_val) {
			if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags & PPE_ACL_RULE_FLAG_IPLEN_MASK)) {
				ppecfg_log_error("l3_len_range, %s\n", range);
				return error;
			}
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags |= PPE_ACL_RULE_FLAG_IPLEN_RANGE;
			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags &= ~PPE_ACL_RULE_FLAG_IPLEN_MASK;
		}
		ppecfg_log_info("L3_LEN_MASK: %s\n", range);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_first_frag_obj()
 * 	Get first fragment values from JSON object
 */
static int ppecfg_acl_json_get_first_frag_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "first_frag");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG_VALID;

	char *reserved = ppecfg_json_object_handler(obj, "first_frag");

	error = ppecfg_param_get_int(reserved, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG].rule.first_frag.reserved);
	if (error) {
		ppecfg_log_error("first_frag, %s\n", reserved);
		return error;
	}

	nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG].rule_flags |= PPE_ACL_RULE_FLAG_L3_FRAG;
	ppecfg_log_info("FIRST FRAGMENT: %s\n", reserved);

	return error;
}

/*
 * ppecfg_acl_json_get_tcp_flag_obj()
 * 	Get tcp flag values from JSON object
 */
static int ppecfg_acl_json_get_tcp_flag_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "tcp_flag");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG_VALID;

	char *val = ppecfg_json_object_handler(obj, "tcp_flags_val");
	char *mask = ppecfg_json_object_handler(obj, "tcp_flags_mask");

	error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG].rule.tcp_flag.tcp_flags);
	if (error) {
		ppecfg_log_info("tcp_flags, %s\n", val);
		return error;
	}

	ppecfg_log_info("TCP FLAGS: %s\n", val);

	if (mask) {
		error = ppecfg_param_get_int(mask, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG].rule.tcp_flag.tcp_flags_mask);
		if (error) {
			ppecfg_log_error("tcp_flags_mask, %s\n", mask);
			return error;
		}
		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG].rule_flags |= PPE_ACL_RULE_FLAG_TCP_FLG_MASK;
		ppecfg_log_info("TCP FLAGS MASK: %s\n", mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_l4_proto_obj()
 * 	Get l4_proto values from JSON object
 */
static int ppecfg_acl_json_get_l4_proto_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "l4_proto");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR_VALID;

	char *l3_v4proto_v6nexthdr = ppecfg_json_object_handler(obj, "l3_v4proto_v6nexthdr");
	char *l3_v4proto_v6nexthdr_mask = ppecfg_json_object_handler(obj, "l3_v4proto_v6nexthdr_mask");

	error = ppecfg_param_get_int(l3_v4proto_v6nexthdr, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR].rule.proto_nexthdr.l3_v4proto_v6nexthdr);
	if (error) {
		ppecfg_log_info("tcp_flag, %s\n", l3_v4proto_v6nexthdr);
		return error;
	}

	ppecfg_log_info("L3 V4 PROTO V6 HEADER: %s\n", l3_v4proto_v6nexthdr);

	if (l3_v4proto_v6nexthdr_mask) {
		error = ppecfg_param_get_int(l3_v4proto_v6nexthdr_mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR].rule.proto_nexthdr.l3_v4proto_v6nexthdr_mask);
		if (error) {
			ppecfg_log_error("l3_v4proto_v6nexthdr_mask, %s\n", l3_v4proto_v6nexthdr_mask);
			return error;
		}

		nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR].rule_flags |= PPE_ACL_RULE_FLAG_PROTO_NEXTHDR_MASK;
		ppecfg_log_info("L3 V4 PROTO V6 HEADER MASK: %s\n", l3_v4proto_v6nexthdr_mask);
	}

	return error;
}

/*
 * ppecfg_acl_json_get_udf_obj()
 * 	Get udf values from JSON object
 */
static int ppecfg_acl_json_get_udf_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	bool bool_val = false;

	obj = ppecfg_get_json_object(rule_obj, "udf");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_UDF_VALID;

	char *udf_a_min = ppecfg_json_object_handler(obj, "udf_a_min");
	char *udf_b_min = ppecfg_json_object_handler(obj, "udf_b_min");
	char *udf_c = ppecfg_json_object_handler(obj, "udf_c");
	char *udf_d = ppecfg_json_object_handler(obj, "udf_d");
	char *udf_a_mask_max = ppecfg_json_object_handler(obj, "udf_a_max_mask");
	char *udf_b_mask_max = ppecfg_json_object_handler(obj, "udf_b_mask_max");
	char *udf_c_mask = ppecfg_json_object_handler(obj, "udf_c_mask");
	char *udf_d_mask = ppecfg_json_object_handler(obj, "udf_d_mask");
	char *udf_a_valid = ppecfg_json_object_handler(obj, "udf_a_valid");
	char *udf_b_valid = ppecfg_json_object_handler(obj, "udf_b_valid");
	char *udf_c_valid = ppecfg_json_object_handler(obj, "udf_c_valid");
	char *udf_d_valid = ppecfg_json_object_handler(obj, "udf_d_valid");
	char *udf_a_range = ppecfg_json_object_handler(obj, "udf_a_range");
	char *udf_b_range = ppecfg_json_object_handler(obj, "udf_b_range");

	if (udf_a_min) {
		error = ppecfg_param_get_int(udf_a_min, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_a_min);
		if (error) {
			ppecfg_log_error("udf_a_min, %s\n", udf_a_min);
			return error;
		}

		ppecfg_log_info("UDF A MIN: %s\n", udf_a_min);

		if (udf_a_mask_max) {
			error = ppecfg_param_get_int(udf_a_mask_max, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_a_mask_max);
			if (error) {
				ppecfg_log_error("udf_a_mask_max, %s\n", udf_a_mask_max);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFA_MASK;
			ppecfg_log_info("UDF A MAX MASK: %s\n", udf_a_mask_max);
		}

		if (udf_a_range) {
			error = ppecfg_param_get_bool(udf_a_range, &bool_val);
			if (error) {
				ppecfg_log_error("udf_a_range, %s\n", udf_a_range);
				return error;
			}

			if (bool_val) {
				if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags & PPE_ACL_RULE_FLAG_UDFA_MASK)) {
					ppecfg_log_error("udf_a_range, %s\n", udf_a_range);
					return error;
				}
				nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFA_RANGE;
				nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags &= ~PPE_ACL_RULE_FLAG_UDFA_MASK;
				ppecfg_log_info("UDF A RANGE: %s\n", udf_a_range);
			}
		}

		if (udf_a_valid) {
			error = ppecfg_param_get_int(udf_a_valid, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_a_valid);
			if (error) {
				ppecfg_log_error("udf_a_valid, %s\n", udf_a_valid);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFA_RANGE;
			ppecfg_log_info("UDF A VALID: %s\n", udf_a_valid);
		}
	}

	if (udf_b_min) {
		error = ppecfg_param_get_int(udf_b_min, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_b_min);
		if (error) {
			ppecfg_log_error("udf_b_min, %s\n", udf_b_min);
			return error;
		}

		ppecfg_log_info("UDF B MIN: %s\n", udf_b_min);

		if (udf_b_mask_max) {
			error = ppecfg_param_get_int(udf_b_mask_max, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_b_mask_max);
			if (error) {
				ppecfg_log_error("udf_b_mask_max, %s\n", udf_b_mask_max);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFB_MASK;
			ppecfg_log_info("UDF B MAX MASK: %s\n", udf_b_mask_max);
		}

		if (udf_b_range) {
			error = ppecfg_param_get_bool(udf_b_range, &bool_val);
			if (error) {
				ppecfg_log_error("udf_b_range, %s\n", udf_b_range);
				return error;
			}

			if (bool_val) {
				if (!(nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags & PPE_ACL_RULE_FLAG_UDFB_MASK)) {
					ppecfg_log_error("udf_b_range, %s\n", udf_b_range);
					return error;
				}
				nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFB_RANGE;
				nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags &= ~PPE_ACL_RULE_FLAG_UDFB_MASK;
				ppecfg_log_info("UDF B RANGE: %s\n", udf_b_range);
			}
		}

		if (udf_b_valid) {
			error = ppecfg_param_get_int(udf_b_valid, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_b_valid);
			if (error) {
				ppecfg_log_error("udf_b_valid, %s\n", udf_b_valid);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFB_RANGE;
			ppecfg_log_info("UDF B VALID: %s\n", udf_b_valid);
		}
	}

	if (udf_c) {
		error = ppecfg_param_get_int(udf_c, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_c);
		if (error) {
			ppecfg_log_error("udf_c, %s\n", udf_c);
			return error;
		}

		ppecfg_log_info("UDF C: %s\n", udf_c);

		if (udf_c_mask) {
			error = ppecfg_param_get_int(udf_c_mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_c_mask);
			if (error) {
				ppecfg_log_error("udf_c_mask, %s\n", udf_c_mask);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFC_MASK;
			ppecfg_log_info("UDF C MASK: %s\n", udf_c_mask);
		}

		if (udf_c_valid) {
			error = ppecfg_param_get_int(udf_c_valid, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_c_valid);
			if (error) {
				ppecfg_log_error("udf_c_valid %s\n", udf_c_valid);
				return error;
			}

			ppecfg_log_info("UDF C VALID: %s\n", udf_c_valid);
		}
	}

	if (udf_d) {
		error = ppecfg_param_get_int(udf_d, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_d);
		if (error) {
			ppecfg_log_error("udf_d, %s\n", udf_d);
			return error;
		}

		ppecfg_log_info("UDF D: %s\n", udf_d);

		if (udf_d_mask) {
			error = ppecfg_param_get_int(udf_d_mask, sizeof(uint16_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_d_mask);
			if (error) {
				ppecfg_log_error("udf_d_mask, %s\n", udf_d_mask);
				return error;
			}

			nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule_flags |= PPE_ACL_RULE_FLAG_UDFD_MASK;
			ppecfg_log_info("UDF D MASK: %s\n", udf_d_mask);
		}

		if (udf_d_valid) {
			error = ppecfg_param_get_int(udf_d_valid, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_UDF].rule.udf.udf_d_valid);
			if (error) {
				ppecfg_log_error("udf_d_valid%s\n", udf_d_valid);
				return error;
			}

			ppecfg_log_info("UDF D VALID: %s\n", udf_d_valid);
		}
	}

	return error;
}

/*
 * ppecfg_acl_json_get_default_obj()
 * 	Get default values from JSON object
 */
static int ppecfg_acl_json_get_default_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;

	obj = ppecfg_get_json_object(rule_obj, "default");
	if (obj == NULL) {
		return error;
	}

	nl_msg->rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DEFAULT_VALID;

	char *reserved = ppecfg_json_object_handler(obj, "default");

	error = ppecfg_param_get_int(reserved, sizeof(uint8_t), &nl_msg->rule.rules[PPE_ACL_RULE_MATCH_TYPE_DEFAULT].rule.def.reserved);
	if (error) {
		ppecfg_log_error("default, %s\n", reserved);
		return error;
	}

	ppecfg_log_info("DEFAULT: %s\n", reserved);

	return error;
}

/*
 * ppecfg_acl_json_get_actions_obj()
 * 	Get actions values from JSON object
 */
static int ppecfg_acl_json_get_actions_obj(struct json_object *rule_obj, struct nss_ppenl_acl_rule *nl_msg)
{
	struct json_object *obj;
	int error = 0;
	uint8_t mirror_val;

	obj = ppecfg_get_json_object(rule_obj, "action");
	if (obj == NULL) {
		return error;
	}

	char *fwd_cmd = ppecfg_json_object_handler(obj, "fwd_cmd");
	char *service_code = ppecfg_json_object_handler(obj, "service_code");
	char *enqueue_priority = ppecfg_json_object_handler(obj, "enqueue_priority");
	char *qid = ppecfg_json_object_handler(obj, "qid");
	char *c_pcp = ppecfg_json_object_handler(obj, "c_pcp");
	char *s_pcp = ppecfg_json_object_handler(obj, "s_pcp");
	char *tos = ppecfg_json_object_handler(obj, "tos-tc");
	char *cvid = ppecfg_json_object_handler(obj, "cvid");
	char *svid = ppecfg_json_object_handler(obj, "svid");
	char *dest_dev = ppecfg_json_object_handler(obj, "dest_dev");
	char *redir_core = ppecfg_json_object_handler(obj, "redir_core");
	char *policer_id = ppecfg_json_object_handler(obj, "policer_id");
	char *mirror_en = ppecfg_json_object_handler(obj, "mirror_en");

	ppecfg_log_info("\nAction:\n");

	if (fwd_cmd) {
		if (strcmp("FWD", fwd_cmd) == 0) {
			nl_msg->rule.action.fwd_cmd = PPE_ACL_FWD_CMD_FWD;
		} else if (strcmp("DROP", fwd_cmd) == 0) {
			nl_msg->rule.action.fwd_cmd = PPE_ACL_FWD_CMD_DROP;
		} else if (strcmp("COPY", fwd_cmd) == 0) {
			nl_msg->rule.action.fwd_cmd = PPE_ACL_FWD_CMD_COPY;
		} else if (strcmp("REDIR", fwd_cmd) == 0) {
			nl_msg->rule.action.fwd_cmd = PPE_ACL_FWD_CMD_REDIR;
		} else {
			ppecfg_log_error("fwd_cmd, %s\n", fwd_cmd);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_FW_CMD;
		ppecfg_log_info("FWD CMD : %s\n", fwd_cmd);
	}

	if (service_code) {
		error = ppecfg_param_get_int(service_code, sizeof(uint8_t), &nl_msg->rule.action.service_code);
		if (error) {
			ppecfg_log_error("service_code, %s\n", service_code);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_SERVICE_CODE_EN;
		ppecfg_log_info("service_code : %s\n", service_code);
	}

	if (enqueue_priority) {
		error = ppecfg_param_get_int(enqueue_priority, sizeof(uint8_t), &nl_msg->rule.action.enqueue_pri);
		if (error) {
			ppecfg_log_error("service_code, %s\n", enqueue_priority);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_ENQUEUE_PRI_CHANGE_EN;
		ppecfg_log_info("enqueue_priority : %s\n", enqueue_priority);
	}

	if (qid) {
		error = ppecfg_param_get_int(qid, sizeof(uint8_t), &nl_msg->rule.action.qid);
		if (error) {
			ppecfg_log_error("qid, %s\n", qid);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_QID_EN;
		ppecfg_log_info("qid : %s\n", qid);
	}

	if (c_pcp) {
		error = ppecfg_param_get_int(c_pcp, sizeof(uint8_t), &nl_msg->rule.action.ctag_pcp);
		if (c_pcp && error) {
			ppecfg_log_error("c_pcp, %s\n", c_pcp);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CHANGE_EN;
		ppecfg_log_info("c_pcp : %s\n", c_pcp);
	}

	if (s_pcp) {
		error = ppecfg_param_get_int(s_pcp, sizeof(uint8_t), &nl_msg->rule.action.stag_pcp);
		if (s_pcp && error) {
			ppecfg_log_error("s_pcp, %s\n", s_pcp);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CHANGE_EN;
		ppecfg_log_info("s_pcp : %s\n", s_pcp);
	}

	if (tos) {
		error = ppecfg_param_get_int(tos, sizeof(uint8_t), &nl_msg->rule.action.tos_tc);
		if (error) {
			ppecfg_log_error("tos, %s\n", tos);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_TOS_TC_CHANGE_EN;
		ppecfg_log_info("tos : %s\n", tos);
	}

	if (cvid) {
		error = ppecfg_param_get_int(cvid, sizeof(uint16_t), &nl_msg->rule.action.cvid);
		if (error) {
			ppecfg_log_error("cvid, %s\n", cvid);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_CVID_CHANGE_EN;
		ppecfg_log_info("cvid : %s\n", cvid);
	}

	if (svid) {
		error = ppecfg_param_get_int(svid, sizeof(uint16_t), &nl_msg->rule.action.svid);
		if (error) {
			ppecfg_log_error("svid, %s\n", svid);
			return error;
		}

		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_SVID_CHANGE_EN;
		ppecfg_log_info("svid : %s\n", svid);
	}

	if (dest_dev) {
		error = ppecfg_param_get_str(dest_dev, sizeof(nl_msg->rule.action.dst.dev_name), &nl_msg->rule.action.dst.dev_name);
		if (error) {
			ppecfg_log_error("dest_dev, %s\n", dest_dev);
			return error;
		}
		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DEST_INFO_CHANGE_EN;
		ppecfg_log_info("dest_dev : %s\n", dest_dev);
	}

	if (redir_core) {
		error = ppecfg_param_get_int(redir_core, sizeof(uint8_t), &nl_msg->rule.action.redir_core);
		if (error) {
			ppecfg_log_error("redir_core, %s\n", redir_core);
			return error;
		}
		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_REDIR_TO_CORE_EN;
		ppecfg_log_info("redir_core : %s\n", redir_core);
	}

	if (policer_id) {
		error = ppecfg_param_get_int(policer_id, sizeof(uint16_t), &nl_msg->rule.action.policer_id);
		if (error) {
			ppecfg_log_error("policer_id, %s\n", policer_id);
			return error;
		}
		nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_POLICER_EN;
		ppecfg_log_info("policer_id : %s\n", policer_id);
	}

	if (mirror_en) {
		error = ppecfg_param_get_int(mirror_en, sizeof(uint8_t), &mirror_val);
		if (error) {
			ppecfg_log_error("mirror_en, %s\n", mirror_en);
			return error;
		}
		if (mirror_val == 1) {
			nl_msg->rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_MIRROR_EN;
		}
		ppecfg_log_info("mirror_en : %s\n", mirror_en);
	}

	return error;
}

/*
 * ppecfg_acl_json_rule_add()
 * 	Add rule from json config file
 */
int ppecfg_acl_json_rule_add(struct json_object *rule_obj)
{
	struct nss_ppenl_acl_rule nl_msg = {{0}};
	int error = 0;
	const char *val;
	bool bool_val = false;

	nss_ppenl_acl_init_rule(&nl_msg, 0);
	ppecfg_log_info("ACL Rule\n");

	/*
	 * Extractting RULE ID - mandatory
	 */
	val = ppecfg_json_object_handler(rule_obj, "rule_id");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint32_t), &nl_msg.rule.rule_id);
		if (error) {
			ppecfg_log_info("rule_id, %s\n", val);
			goto done;
		}
		ppecfg_log_info("RULE ID : %s\n", val);
	} else {
		goto done;
	}

	/*
	 * Extracting SRC DEV - mandatory
	 */
	val = ppecfg_json_object_handler(rule_obj, "src_dev");
	if (val) {
		error = ppecfg_param_get_str(val, sizeof(nl_msg.rule.src.dev_name), &nl_msg.rule.src.dev_name);
		if (error) {
			ppecfg_log_info("src_dev, %s\n", val);
			goto done;
		}

		if (strcmp("flow", nl_msg.rule.src.dev_name) == 0) {
			nl_msg.rule.stype = PPE_ACL_RULE_SRC_TYPE_FLOW;
		} else if (strcmp("sc", nl_msg.rule.src.dev_name) == 0) {
			memset(nl_msg.rule.src.dev_name, 0, sizeof(nl_msg.rule.src.dev_name));
			nl_msg.rule.stype = PPE_ACL_RULE_SRC_TYPE_SC;
		} else {
			nl_msg.rule.stype = PPE_ACL_RULE_SRC_TYPE_DEV;
		}
		ppecfg_log_info("SRC DEV : %s\n", val);
	} else {
		goto done;
	}

	/*
	 * Extracting POST ROUTE EN
	 */
	val = ppecfg_json_object_handler(rule_obj, "post_route_en");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("post_route_en, %s\n", val);
			goto done;
		}

		if (bool_val == true) {
			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_POST_RT_EN;
		}

		bool_val = false;
		ppecfg_log_info("POST ROUTE : %s\n", val);
	}

	/*
	 * Extracting FLOW QOS OVERRIDE
	 */
	val = ppecfg_json_object_handler(rule_obj, "flow_qos_override");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("flow_qos_override, %s\n", val);
			goto done;
		}

		if (bool_val == true) {
			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_FLOW_QOS_OVERRIDE;
		}

		bool_val = false;
		ppecfg_log_info("FLOW QOS OVERRIDE : %s\n", val);
	}

	/*
	 * Extracting PRIORITY
	 */
	val = ppecfg_json_object_handler(rule_obj, "priority");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg.rule.cmn.pri);
		if (error) {
			ppecfg_log_error("priority, %s\n", val);
			goto done;
		}

		nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_PRI_EN;
		ppecfg_log_info("PRIORITY : %s\n", val);
	}

	/*
	 * Extracting SRC SC
	 */
	val = ppecfg_json_object_handler(rule_obj, "src_sc");
	if (val) {
		if (nl_msg.rule.stype != PPE_ACL_RULE_SRC_TYPE_SC) {
			ppecfg_log_error("src_sc, %s\n", val);
			goto done;
		}

		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.rule.src.sc);
		if (error) {
			ppecfg_log_error("src_sc, %s\n", val);
			goto done;
		}

		ppecfg_log_info("nl_msg.rule.src.sc: %d\n", nl_msg.rule.src.sc);
	}

	/*
	 * Extracting OUTER HEADER
	 */
	val = ppecfg_json_object_handler(rule_obj, "outer_header_en");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("outer_header_en, %s\n", val);
			goto done;
		}

		if (bool_val == true) {
			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_OUTER_HDR_MATCH;
		}

		bool_val = false;
		ppecfg_log_info("OUTER HEADER %s\n", val);
	}

	/*
	 * Extracting METADATA
	 */
	val = ppecfg_json_object_handler(rule_obj, "metadata_en");
	if (val) {
		error = ppecfg_param_get_bool(val, &bool_val);
		if (error) {
			ppecfg_log_error("metadata_en, %s\n", val);
			goto done;
		}

		if (bool_val == true) {
			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_METADATA_EN;
		}

		bool_val = false;
		ppecfg_log_info("METADATA %s\n", val);
	}

	/*
	 * Extracting GROUP
	 */
	val = ppecfg_json_object_handler(rule_obj, "group");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint16_t), &nl_msg.rule.cmn.group);
		if (error) {
			ppecfg_log_error("group, %s\n", val);
			goto done;
		}

		ppecfg_log_info("nl_msg.rule.cmn.group: %d\n", nl_msg.rule.cmn.group);
		nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_GROUP_EN;
		ppecfg_log_info("GROUP %s\n", val);
	}

	/*
	 * Extracting SMAC
	 */
	error = ppecfg_acl_json_get_smac_obj(rule_obj, &nl_msg);
	if (!error) {
		goto done;
	}

	/*
	 * Extracting DMAC
	 */
	error = ppecfg_acl_json_get_dmac_obj(rule_obj, &nl_msg);
	if (!error) {
		goto done;
	}

	/*
	 * Extracting CVID
	 */
	error = ppecfg_acl_json_get_cvid_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting SVID
	 */
	error = ppecfg_acl_json_get_svid_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting CPCP
	 */
	error = ppecfg_acl_json_get_cpcp_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting SPCP
	 */
	error = ppecfg_acl_json_get_spcp_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting PPPOE
	 */
	error = ppecfg_acl_json_get_pppoe_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting ETHER TYPE
	 */
	error = ppecfg_acl_json_get_ether_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting TOS
	 */
	error = ppecfg_acl_json_get_tos_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting SIP
	 */
	error = ppecfg_acl_json_get_sip_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting DIP
	 */
	error = ppecfg_acl_json_get_dip_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting SPORT
	 */
	error = ppecfg_acl_json_get_sport_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting DPORT
	 */
	error = ppecfg_acl_json_get_dport_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting TTL
	 */
	error = ppecfg_acl_json_get_ttl_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting l3_len
	 */
	error = ppecfg_acl_json_get_l3_len_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting first fragment
	 */
	error = ppecfg_acl_json_get_first_frag_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting tcp flag
	 */
	error = ppecfg_acl_json_get_tcp_flag_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting default
	 */
	error = ppecfg_acl_json_get_default_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting l4 proto
	 */
	error = ppecfg_acl_json_get_l4_proto_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting udf
	 */
	error = ppecfg_acl_json_get_udf_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * Extracting first fragment
	 */
	error = ppecfg_acl_json_get_actions_obj(rule_obj, &nl_msg);
	if (error) {
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_acl_rule_add(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

	return error;

done:
		ppecfg_log_info("\nError in extracting ACL params\n");
		return error;
}
