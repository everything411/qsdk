/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/if.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/netstandby.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/version.h>
#include <net/genetlink.h>

#include <netstandby_nl_if.h>
#include "netstandby_nl_cmn.h"
#include "netstandby_nl.h"
#include "netstandby_main.h"
#include "netstandby_nss.h"

#define MSG_ALLOC_SIZE 1500

void netstandby_trigger_work(struct work_struct *work)
{
	struct netstandby_gbl_ctx *gbl_ctx = container_of(to_delayed_work(work), struct netstandby_gbl_ctx, trigger_work);
	struct netstandby_system_info *info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_NSS];
	struct netstandby_trigger_info trigger_info = {0};

	trigger_info.system_type = NETSTANDBY_SUBSYSTEM_TYPE_NSS;
	trigger_info.event_type = NETSTANDBY_NOTIF_TRIGGER;

	info->init_info.trigger_cb(NULL, &trigger_info);
}

/*
 * netstandby_acl_process_buf()
 *	Process acl index notification from ppe driver
 */
bool netstandby_acl_process_buf(void *app_data, __attribute__((unused))void *skb)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;

	/*
	 * To avoid burst of netlink message in the NL socket,
	 * we are sending the trigger only once to daemon after enter is completed.
	 * If the NL messages are sent in a burst, the exit completion of BSP is lost in core kernel.
	 * Also, when standby EXIT is in process, we will not send the trigger.
	 */
	if ((gbl_ctx->netstandby_state == NETSTANDBY_SYSTEM_ENTER_COMPLETED) &&
	    (!gbl_ctx->netstandby_first_trigger)) {
		gbl_ctx->netstandby_first_trigger = true;
		schedule_delayed_work(&gbl_ctx->trigger_work, msecs_to_jiffies(1));
	} else if (gbl_ctx->netstandby_state == NETSTANDBY_SYSTEM_EXIT_IN_PROGRESS) {
		return false;
	}

	schedule_delayed_work(&gbl_ctx->trigger_work, msecs_to_jiffies(100));
	return false;
}

/*
 * netstandby_acl_unregister()
 *	Unregister ACL callback with PPE driver
 */
void netstandby_acl_unregister(ppe_acl_rule_id_t acl_id)
{
	ppe_acl_rule_callback_unregister(acl_id);
}

/*
 * netstandby_acl_register()
 *	register ACL callback with PPE driver
 */
enum netstandby_status netstandby_acl_register(ppe_acl_rule_id_t acl_id)
{
	if (!ppe_acl_rule_callback_register(acl_id, netstandby_acl_process_buf, NULL)) {
		netstandby_warn("Callback register with ACL failed for network standby for acl_id: %d\n", acl_id);
		return NETSTANDBY_ACL_REGISTER_FAIL;
	}

	return NETSTANDBY_SUCCESS;
}

/*
 * netstandby_acl_rule_destroy()
 *	Default ACL rule destroy
 */
enum netstandby_status netstandby_acl_rule_destroy(ppe_acl_rule_id_t id)
{
	int status;

	/*
	 * Unregister callback
	 */
	ppe_acl_rule_callback_unregister(id);

	/*
	 * Destroy rule
	 */
	status = ppe_acl_rule_destroy(id);
	if (status != PPE_ACL_RET_SUCCESS) {
		netstandby_warn("unable to create rule in ppe driver, error = %d\n", status);
	}

	return status;
}

/*
 * netstandby_acl_rule_create()
 *	Default ACL rule create
 */
enum netstandby_status netstandby_acl_rule_create(struct netstandby_trigger_rule *trigger_rule, struct net_device *dev, int rule_idx, bool is_default)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_NSS];
	struct ppe_acl_rule rule = {0};
	int status;

	if (is_default) {
		rule.stype = PPE_ACL_RULE_SRC_TYPE_DEV;
		memcpy((void *)rule.src.dev_name, (void *)dev->name, sizeof(dev->name));

		rule.action.fwd_cmd = PPE_ACL_FWD_CMD_REDIR;
		rule.valid_flags = (1 << PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE);
		rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_FW_CMD;
		rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_NO_RULEID | PPE_ACL_RULE_CMN_FLAG_METADATA_EN;

		if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_IPV6) == NETSTANDBY_EXIT_TRIGGER_RULE_IPV6)
			rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto = ETH_P_IPV6;
		else
			rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto = ETH_P_IP;
	} else {
		if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_PROTOCOL_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_PROTOCOL_VALID) {
			rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR_VALID;
			rule.rules[PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR].rule.proto_nexthdr.l3_v4proto_v6nexthdr = trigger_rule->protocol;
		}

		if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_SRC_MAC_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_SRC_MAC_VALID) {
			rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SMAC_VALID;
			memcpy(&rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac, trigger_rule->smac, sizeof(trigger_rule->smac));
		}

		if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_DEST_MAC_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_DEST_MAC_VALID) {
			rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DMAC_VALID;
			memcpy(&rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac, trigger_rule->dmac, sizeof(trigger_rule->dmac));
		}

		if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_IPV6) == NETSTANDBY_EXIT_TRIGGER_RULE_IPV6) {
			if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_SRC_IP_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_SRC_IP_VALID) {
				memcpy(&rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip, trigger_rule->src_ip, sizeof(trigger_rule->src_ip));
				rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_type = PPE_ACL_IP_TYPE_V6;
				rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SIP_VALID;
			}

			if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_DES_IP_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_DES_IP_VALID) {
				memcpy(&rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip, trigger_rule->dest_ip, sizeof(trigger_rule->dest_ip));
				rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_type = PPE_ACL_IP_TYPE_V6;
				rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DIP_VALID;
			}

		} else if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_IPV4) == NETSTANDBY_EXIT_TRIGGER_RULE_IPV4) {
			if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_SRC_IP_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_SRC_IP_VALID) {
				memcpy(&rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip[0], &trigger_rule->src_ip[0], sizeof(trigger_rule->src_ip[0]));
				rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_type = PPE_ACL_IP_TYPE_V4;
				rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SIP_VALID;
			}

			if ((trigger_rule->valid_flags & NETSTANDBY_EXIT_TRIGGER_RULE_DES_IP_VALID) == NETSTANDBY_EXIT_TRIGGER_RULE_DES_IP_VALID) {
				memcpy(&rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip[0], &trigger_rule->dest_ip[0], sizeof(trigger_rule->dest_ip[0]));
				rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_type = PPE_ACL_IP_TYPE_V4;
				rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DIP_VALID;
			}
		}

		rule.stype = PPE_ACL_RULE_SRC_TYPE_DEV;
		memcpy((void *)rule.src.dev_name, (void *)dev->name, sizeof(dev->name));
		rule.action.fwd_cmd = PPE_ACL_FWD_CMD_REDIR;
		rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_FW_CMD;
		rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_NO_RULEID | PPE_ACL_RULE_CMN_FLAG_METADATA_EN;
	}

	status = ppe_acl_rule_create(&rule);
	if (status == PPE_ACL_RET_SUCCESS) {
		netstandby_info("%s: PPE ACL create success\n", __func__);
	} else {
		netstandby_warn("create rule in ppe driver failed, error = %d\n", status);
	}

	if (!ppe_acl_rule_callback_register(rule.rule_id, netstandby_acl_process_buf, NULL)) {
		netstandby_acl_rule_destroy(rule.rule_id);
		netstandby_warn("Callback register with ACL failed for network standby\n");
		return NETSTANDBY_ACL_REGISTER_FAIL;
	}

	info->acl_id[rule_idx] = rule.rule_id;
	info->is_acl_valid = true;

	return NETSTANDBY_SUCCESS;
}
