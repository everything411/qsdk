/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <net/genetlink.h>
#include <net/sock.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_port_mgmt_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_port_mgmt.h"

static int nss_ppenl_port_mgmt_ops_port_isol_set(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_act_ctrl_set(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_isol_def_set(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_mac_lrn_limit_set(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_mac_filter_set(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_mac_filter_clear(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_omci_port_add(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_omci_port_del(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_port_mgmt_ops_omci_port_flush(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_port_mgmt_ops[] = {
	{.cmd = NSS_PPE_PORT_MGMT_PORT_ISOL_SET_MSG, .doit = nss_ppenl_port_mgmt_ops_port_isol_set,},	/* PORT_MGMT port isolation set */
	{.cmd = NSS_PPE_PORT_MGMT_ACT_CTRL_SET_MSG, .doit = nss_ppenl_port_mgmt_ops_act_ctrl_set,},	/* PORT_MGMT action control set */
	{.cmd = NSS_PPE_PORT_MGMT_PORT_ISOL_DEF_MSG, .doit = nss_ppenl_port_mgmt_ops_isol_def_set,},	/* PORT_MGMT default isolation set */
	{.cmd = NSS_PPE_PORT_MGMT_MAC_LRN_LIMIT_SET_MSG, .doit = nss_ppenl_port_mgmt_ops_mac_lrn_limit_set,},	/* PORT_MGMT mac learn limit set */
	{.cmd = NSS_PPE_PORT_MGMT_MAC_FILTER_SET_MSG, .doit = nss_ppenl_port_mgmt_ops_mac_filter_set,},   /* PORT_MGMT mac filter set */
	{.cmd = NSS_PPE_PORT_MGMT_MAC_FILTER_CLR_MSG, .doit = nss_ppenl_port_mgmt_ops_mac_filter_clear,},   /* PORT_MGMT mac filter clear */
	{.cmd = NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG, .doit = nss_ppenl_port_mgmt_ops_omci_port_add,},	/* PORT_MGMT OMCI port add */
	{.cmd = NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG, .doit = nss_ppenl_port_mgmt_ops_omci_port_del,},	/* PORT_MGMT OMCI port del */
	{.cmd = NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG, .doit = nss_ppenl_port_mgmt_ops_omci_port_flush,},	/* PORT_MGMT OMCI port flush */
};

/*
 * PORT_MGMT family definition
 */
static struct genl_family nss_ppenl_port_mgmt_family = {
	.name = NSS_PPENL_PORT_MGMT_FAMILY,		     /* family name string */
	.hdrsize = sizeof(struct nss_ppenl_port_mgmt_info),  /* NSS NETLINK PORT_MGMT rule */
	.version = NSS_PPENL_VER,				/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_PORT_MGMT_MAX_MSG_TYPES,	     /* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_port_mgmt_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_port_mgmt_ops),
};

/*
 * nss_ppenl_port_mgmt_ops_port_isol_set()
 *	Callback for port isolation set command.
 */
static int nss_ppenl_port_mgmt_ops_port_isol_set(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info  *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_PORT_ISOL_SET_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT port isol set info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d, port_id:%d, omci_id:%d\n", __func__, pid, nl_port_mgmt_rule->isol.port_id, nl_port_mgmt_rule->isol.omci_id);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_isol_config(&nl_port_mgmt_rule->isol);
	if (ret) {
		nss_ppenl_info("PPE port isolation set failed = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->isol.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_port_mgmt_rule->isol.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_act_ctrl_set()
 *	Callback for action control set command.
 */
static int nss_ppenl_port_mgmt_ops_act_ctrl_set(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info  *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_ACT_CTRL_SET_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT action control set info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_act_ctrl_set(&nl_port_mgmt_rule->isol);
	if (ret) {
		nss_ppenl_info("PPE action control set failed = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->isol.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_port_mgmt_rule->isol.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_isol_def_set()
 *	Callback for isolation deafualt set command.
 */
static int nss_ppenl_port_mgmt_ops_isol_def_set(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info  *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_PORT_ISOL_DEF_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT port defaul isol set info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_default_isol_set();
	if (ret) {
		nss_ppenl_info("PPE port default isolation set failed = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->isol.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_port_mgmt_rule->isol.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_mac_lrn_limit_set()
 *	Handle Netlink request to set MAC learning limit.
 */
static int nss_ppenl_port_mgmt_ops_mac_lrn_limit_set(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_MAC_LRN_LIMIT_SET_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT mac learn limit set info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_mac_lrn_limit_set(&nl_port_mgmt_rule->mac_lrn_limit);
	if (ret) {
		nss_ppenl_info("PPE port mac learn limit set failed = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->mac_lrn_limit.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_port_mgmt_rule->mac_lrn_limit.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;

}

/*
 * nss_ppenl_port_mgmt_ops_mac_filter_set()
 *	Handle Netlink request to set MAC filter configuration.
 */
static int nss_ppenl_port_mgmt_ops_mac_filter_set(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_MAC_FILTER_SET_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT mac filter set info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_mac_filter_set(&nl_port_mgmt_rule->mac_filter);
	if (ret) {
		nss_ppenl_info("PPE port mac filter set failed = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->mac_filter.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_port_mgmt_rule->mac_filter.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_mac_filter_clear()
 *	Handle Netlink request to clear MAC filter configuration.
 */
static int nss_ppenl_port_mgmt_ops_mac_filter_clear(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_MAC_FILTER_CLR_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT mac filter clear info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_mac_filter_clear(&nl_port_mgmt_rule->mac_filter);
	if (ret) {
		nss_ppenl_info("PPE port mac filter clear failed = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->mac_filter.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_port_mgmt_rule->mac_filter.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_omci_port_add()
 *	Handle Netlink request to add an OMCI-managed port.
 */
static int nss_ppenl_port_mgmt_ops_omci_port_add(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT OMCI port add info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_omci_port_add(&nl_port_mgmt_rule->omci_port);
	if (ret) {
		nss_ppenl_info("PPE OMCI port add failed = %d\n", ret);
	}

	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->omci_port.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_port_mgmt_rule->omci_port.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_omci_port_del()
 *	Handle Netlink request to delete an OMCI-managed port.
 */
static int nss_ppenl_port_mgmt_ops_omci_port_del(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT OMCI port del info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_omci_port_del(&nl_port_mgmt_rule->omci_port);
	if (ret) {
		nss_ppenl_info("PPE OMCI port del failed = %d\n", ret);
	}

	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->omci_port.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_port_mgmt_rule->omci_port.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_ops_omci_port_flush()
 *	Handle Netlink request to flush all OMCI-managed ports.
 */
static int nss_ppenl_port_mgmt_ops_omci_port_flush(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_port_mgmt_info *nl_port_mgmt_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	int ret;

	nl_cm = nss_ppenl_get_msg(&nss_ppenl_port_mgmt_family, info, NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PORT_MGMT OMCI port flush info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	nl_port_mgmt_rule = container_of(nl_cm, struct nss_ppenl_port_mgmt_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	ret = ppe_port_mgmt_omci_port_flush();
	if (ret) {
		nss_ppenl_info("PPE OMCI port flush failed = %d\n", ret);
	}

	nl_port_mgmt_rule = nss_ppenl_get_data(resp);
	nl_port_mgmt_rule->omci_port.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_port_mgmt_rule->omci_port.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_port_mgmt_init()
 *	handler init
 */
bool nss_ppenl_port_mgmt_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE PORT_MGMT handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	nss_ppenl_info("name: %s, strlen:%lu\n", nss_ppenl_port_mgmt_family.name,
		       (unsigned long)strlen(nss_ppenl_port_mgmt_family.name));
	error = genl_register_family(&nss_ppenl_port_mgmt_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register PORT_MGMT family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_port_mgmt_info));

	return true;
}
EXPORT_SYMBOL(nss_ppenl_port_mgmt_init);

/*
 * nss_ppenl_port_mgmt_exit()
 *	handler exit
 */
bool nss_ppenl_port_mgmt_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink PORT_MGMT handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_port_mgmt_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister PORT_MGMT  NETLINK family\n");
		return false;
	}

	return true;
}
EXPORT_SYMBOL(nss_ppenl_port_mgmt_exit);
