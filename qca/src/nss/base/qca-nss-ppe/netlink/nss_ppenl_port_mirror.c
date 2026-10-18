/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_port_mirror.c
 *	NSS Netlink port-mirror handler.
 *
 * Exposes the "nss_ppenl_pmirr" Generic Netlink family so that user-space
 * tools (ppecfg) can enable or disable L2 VP mirroring on a named physical
 * or virtual port.
 *
 * For each port the handler:
 *   1. Resolves the port name to a PPE port number via
 *      ppe_drv_port_num_from_ifname().
 *   2. Obtains the opaque ppe_drv_port object via
 *      ppe_drv_port_from_port_num().
 *   3. Detects whether the port is a GEM port (ppe_drv_port_is_gem()) and
 *      selects the appropriate service code:
 *        - GEM port  → PPE_DRV_SC_DDRQ_PON_PT_MODE
 *        - ETH (phy) → PPE_DRV_SC_DDRQ_ETH_PT_MODE
 *   4. On enable  calls ppe_drv_port_l2_vp_mirror_sc_config(pp, sc, port_num).
 *      On disable calls ppe_drv_port_l2_vp_mirror_sc_reset(pp).
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>

#include <net/genetlink.h>
#include <net/sock.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_port_mirror_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_port_mirror.h"

#include <ppe_mirror.h>

/*
 * Forward declarations
 */
static int nss_ppenl_port_mirror_ops_cfg(struct sk_buff *skb, struct genl_info *info);

/*
 * Operation table – one entry per message type.
 */
static struct genl_ops nss_ppenl_port_mirror_ops[] = {
	{
		.cmd  = NSS_PPE_PORT_MIRROR_CFG_MSG,
		.doit = nss_ppenl_port_mirror_ops_cfg,
	},
};

/*
 * Generic Netlink family definition for port-mirror.
 */
static struct genl_family nss_ppenl_port_mirror_family = {
	.name    = NSS_PPENL_PORT_MIRROR_FAMILY,
	.hdrsize = sizeof(struct nss_ppenl_port_mirror_rule),
	.version = NSS_PPENL_VER,
	.maxattr = NSS_PPE_PORT_MIRROR_MAX_MSG_TYPES,
	.netnsok = true,
	.pre_doit  = NULL,
	.post_doit = NULL,
	.ops   = nss_ppenl_port_mirror_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_port_mirror_ops),
};

/*
 * nss_ppenl_port_mirror_ops_cfg()
 *	Handle NSS_PPE_PORT_MIRROR_CFG_MSG – enable or disable L2 VP mirroring.
 */
static int nss_ppenl_port_mirror_ops_cfg(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mirror_rule *nl_rule;
	struct nss_ppenl_port_mirror_cfg  *cfg;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	ppe_mirror_ret_t ret;
	uint32_t pid;
	int error;

	/*
	 * Extract and validate the common message header.
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mirror_family, info,
				  NSS_PPE_PORT_MIRROR_CFG_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract port-mirror cfg message\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	nl_rule = container_of(nl_cm, struct nss_ppenl_port_mirror_rule, cm);
	cfg     = &nl_rule->cfg;
	pid     = nl_cm->pid;

	/*
	 * Ensure the port name is null-terminated (user may not have done so).
	 */
	cfg->port_name[IFNAMSIZ - 1] = '\0';

	nss_ppenl_info("%s: pid %u port_name '%s' dir %u mirror_en %d\n",
		       __func__, pid, cfg->port_name, cfg->dir, cfg->mirror_en);

	/*
	 * Copy the incoming message so we can send a response.
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%u: unable to copy NL message for response\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_mirror_port_mirror_set(cfg->port_name,
					   (enum ppe_mirror_port_dir)cfg->dir,
					   cfg->mirror_en);
	if (ret != PPE_MIRROR_RET_SUCCESS) {
		nss_ppenl_warn("%u: ppe_mirror_port_mirror_set failed for port '%s' (ret %d)\n",
			       pid, cfg->port_name, ret);
		cfg->ret = -EIO;
		goto send_resp;
	}

	nss_ppenl_info("%u: mirror dir=%u mirror_en=%d on port '%s'\n",
		       pid, cfg->dir, cfg->mirror_en, cfg->port_name);
	cfg->ret = 0;

send_resp:
	/*
	 * Write the return status into the response copy and send it back.
	 */
	nl_rule = nss_ppenl_get_data(resp);
	nl_rule->cfg.ret = cfg->ret;

	nss_ppenl_trace("%u: sending response ret=%d for port '%s'\n",
			pid, nl_rule->cfg.ret, cfg->port_name);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mirror_init()
 *	Register the port-mirror Generic Netlink family.
 */
bool nss_ppenl_port_mirror_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE port-mirror handler\n");

	error = genl_register_family(&nss_ppenl_port_mirror_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register port-mirror family\n");
		return false;
	}

	nss_ppenl_info("port-mirror netlink msg size = %zu\n",
		       sizeof(struct nss_ppenl_port_mirror_rule));
	return true;
}
EXPORT_SYMBOL(nss_ppenl_port_mirror_init);

/*
 * nss_ppenl_port_mirror_exit()
 *	Unregister the port-mirror Generic Netlink family.
 */
bool nss_ppenl_port_mirror_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS PPE port-mirror handler\n");

	error = genl_unregister_family(&nss_ppenl_port_mirror_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister port-mirror NETLINK family\n");
		return false;
	}

	return true;
}
EXPORT_SYMBOL(nss_ppenl_port_mirror_exit);
