// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
#include <linux/skbuff.h>
#include <net/genetlink.h>
#include "athdbg_netlink.h"
#include "athdbg_core.h"

extern struct ath_debug_base *athdbg_base;

/* ------------------------------------------------------------------ */
/* Generic Netlink family                                               */
/* ------------------------------------------------------------------ */

/* userspace -> kernel: save sender portid for later unicast */
static int fw_anomaly_genl_cmd_register(struct sk_buff *skb,
					struct genl_info *info)
{
	athdbg_base->nl_portid = info->snd_portid;
	pr_debug("athdbg: userspace registered portid=%u\n",
		athdbg_base->nl_portid);
	return 0;
}

static const struct nla_policy
fw_anomaly_genl_policy[__NL_FW_ANAMOLY_ATTR_MAX] = {
	[NL_FW_ANAMOLY_ATTR_EVENT_ID] = { .type = NLA_U32    },
	[NL_FW_ANAMOLY_ATTR_WMI_TLV]  = { .type = NLA_BINARY },
	[NL_FW_ANAMOLY_ATTR_HW_LINK_ID] = { .type = NLA_U32    },
	[NL_FW_ANAMOLY_ATTR_RADIO_IDX]   = { .type = NLA_U32    },
};

static const struct genl_ops fw_anomaly_genl_ops[] = {
	{
		.cmd  = NL_FW_ANAMOLY_CMD_REGISTER,
		.doit = fw_anomaly_genl_cmd_register,
	},
};

static struct genl_family fw_anomaly_genl_family = {
	.name    = WLAN_FW_SS_HANDLER_FAMILY_NAME,
	.version = WLAN_FW_SS_HANDLER_GENL_VERSION,
	.maxattr = __NL_FW_ANAMOLY_ATTR_MAX - 1,
	.policy  = fw_anomaly_genl_policy,
	.ops     = fw_anomaly_genl_ops,
	.n_ops   = ARRAY_SIZE(fw_anomaly_genl_ops),
	.module  = THIS_MODULE,
};

/* ------------------------------------------------------------------ */
/* Send path: called from ath12k.ko via athdbg_if.c                    */
/* ------------------------------------------------------------------ */
void athdbg_netlink_send(struct ath12k_base *ab, u32 event_id,
			 const struct athdbg_wmi_event_info *info)
{
	int rc;
	struct sk_buff *skb;
	void *hdr;
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	u32 i;

	if (!athdbg_base || !athdbg_base->nl_portid)
		return;

	skb = genlmsg_new(nla_total_size(sizeof(u32)) +
			  nla_total_size(sizeof(u32)) +
			  nla_total_size(info->tlv_len), GFP_ATOMIC);
	if (!skb)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev->ar) {
			ar = pdev->ar;
			break;
		}
	}
	if (!ar) {
		pr_warn("athdbg_netlink: no active radio\n");
		return;
	}

	pr_debug("athdbg_netlink: radio_idx=%u hw_link_id=%u\n",
		info->radio_idx, ar->hw_link_id);
	hdr = genlmsg_put(skb, 0, 0, &fw_anomaly_genl_family, 0,
			  NL_FW_ANAMOLY_ATTR_WMI_TLV);
	if (!hdr)
		goto err;

	if (nla_put_u32(skb, NL_FW_ANAMOLY_ATTR_EVENT_ID, event_id)) {
		pr_err("athdbg_netlink: nla_put failed for fw_anomaly event_id");
		goto err;
	}

	if (nla_put_u32(skb, NL_FW_ANAMOLY_ATTR_HW_LINK_ID, ar->hw_link_id)) {
		pr_err("athdbg_netlink: nla_put failed for fw_anomaly_hw_link_id");
		goto err;
	}

	if (nla_put_u32(skb, NL_FW_ANAMOLY_ATTR_RADIO_IDX, info->radio_idx)) {
		pr_err("athdbg_netlink: nla_put failed for fw_anomaly radio_id");
		goto err;
	}

	if (nla_put(skb, NL_FW_ANAMOLY_ATTR_WMI_TLV, info->tlv_len, info->tlv_data)) {
		pr_err("athdbg_netlink: nla_put failed for fw_anomaly_wmi_tlv");
		goto err;
	}

	genlmsg_end(skb, hdr);
	rc = genlmsg_unicast(&init_net, skb, athdbg_base->nl_portid);
	if (rc)
		pr_err("athdbg_netlink: unicast failed for FW_SS_HANDLER: %d\n", rc);
	return;
err:
	nlmsg_free(skb);
}
EXPORT_SYMBOL(athdbg_netlink_send);

/* ------------------------------------------------------------------ */
/* Init / exit - called from athdbg_core.c                             */
/* ------------------------------------------------------------------ */
int athdbg_netlink_init(void)
{
	return genl_register_family(&fw_anomaly_genl_family);
}

void athdbg_netlink_exit(void)
{
	genl_unregister_family(&fw_anomaly_genl_family);
}
