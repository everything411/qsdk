/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include <linux/if.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/version.h>
#include <net/genetlink.h>

#include "nss_ppenl.h"
#include "nss_ppenl_acl.h"
#include "nss_ppenl_cmn_if.h"
#include "nss_ppenl_acl_if.h"
#include "nss_ppenl_policer.h"
#include "nss_ppenl_policer_if.h"
#include "nss_ppenl_qos.h"
#include "nss_ppenl_qos_if.h"
#include "nss_ppenl_exception.h"
#include "nss_ppenl_exception_if.h"
#include "nss_ppenl_cos_map.h"
#include "nss_ppenl_cos_map_if.h"
#ifdef NSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
#include "nss_ppenl_dscp.h"
#include "nss_ppenl_dscp_if.h"
#endif
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
#include "nss_ppenl_pm.h"
#include "nss_ppenl_pm_if.h"
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
#include "nss_ppenl_vlan.h"
#include "nss_ppenl_vlan_if.h"
#endif
#ifdef NSS_PPE_PPENL_PORT_MGMT_SUPPORT
#include "nss_ppenl_port_mgmt.h"
#include "nss_ppenl_port_mgmt_if.h"
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
#include "nss_ppenl_dot1p.h"
#include "nss_ppenl_dot1p_if.h"
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
#include "nss_ppenl_gemport.h"
#include "nss_ppenl_gemport_if.h"
#endif
#ifdef NSS_PPE_EDMA_DDRQ_FEATURE_SUPPORT
#include "nss_ppenl_edma_ddrq.h"
#include "nss_ppenl_edma_ddrq_if.h"
#endif
#ifdef NSS_PPE_MCAST_FEATURE_SUPPORT
#include "nss_ppenl_mcast.h"
#include "nss_ppenl_mcast_if.h"
#endif
#if defined(CONFIG_NSS_PPENL_TUN_RPS)
#include "nss_ppenl_tun_rps.h"
#include "nss_ppenl_tun_rps_if.h"
#endif
#ifdef NSS_PPE_PORT_MIRROR_FEATURE_SUPPORT
#include "nss_ppenl_port_mirror.h"
#include <nss_ppenl_port_mirror_if.h>
#endif

/*
 * nss_ppenl.c
 *	NSS PPE Netlink manager
 */

/*
 * NSS PPE NL family definition
 */
struct nss_ppenl_family {
	uint8_t *name;		/* name of the family */
	nss_ppenl_fn_t entry;	/* entry function of the family */
	nss_ppenl_fn_t exit;	/* exit function of the family */
	bool valid;		/* valid or invalid */
};

/*
 * Family handler table
 */
static struct nss_ppenl_family family_handlers[] = {
	{
		/*
		 * NSS_PPENL_ACL
		 */
		.name = NSS_PPENL_ACL_FAMILY,		/* ACL FAMILY */
		.entry = NSS_PPENL_ACL_INIT,		/* Init */
		.exit = NSS_PPENL_ACL_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_ACL		/* 1 or 0 */
	},
	{
		/*
		 * NSS_PPENL_POLICER
		 */
		.name = NSS_PPENL_POLICER_FAMILY,	/* Policer Family */
		.entry = NSS_PPENL_POLICER_INIT,	/* Init */
		.exit = NSS_PPENL_POLICER_EXIT,	/* exit */
		.valid = CONFIG_NSS_PPENL_POLICER	/* 1 or 0 */
	},
        {
                /*
                 * NSS_PPENL_QOS
                 */
                .name = NSS_PPENL_QOS_FAMILY,       	/* Qos Family */
                .entry = NSS_PPENL_QOS_INIT,        	/* Init */
                .exit = NSS_PPENL_QOS_EXIT,		/* exit */
                .valid = CONFIG_NSS_PPENL_QOS       	/* 1 or 0 */
        },
	{
		/*
		 * NSS_PPENL_COS_MAP
		 */
		.name = NSS_PPENL_COS_MAP_FAMILY,       	/* CosMap Family */
		.entry = NSS_PPENL_COS_MAP_INIT,        	/* Init */
		.exit = NSS_PPENL_COS_MAP_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_COS_MAP       	/* 1 or 0 */
	},
#ifdef NSS_PPE_FEATURE_DOT1P
        {
                /*
                 * NSS_PPENL_DOT1P
                 */
                .name = NSS_PPENL_DOT1P_FAMILY,       	/* Dot1p Family */
                .entry = NSS_PPENL_DOT1P_INIT,        	/* Init */
                .exit = NSS_PPENL_DOT1P_EXIT,		/* exit */
                .valid = CONFIG_NSS_PPENL_DOT1P       	/* 1 or 0 */
        },
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
        {
                /*
                 * NSS_PPENL_GEM_PORT
                 */
                .name = NSS_PPENL_GEM_PORT_FAMILY,      /* Gemp port Family */
                .entry = NSS_PPENL_GEM_PORT_INIT,	/* Init */
                .exit = NSS_PPENL_GEM_PORT_EXIT,	/* exit */
                .valid = CONFIG_NSS_PPENL_GEM_PORT      /* 1 or 0 */
        },
#endif
	{
		/*
		 * NSS_PPENL_EXCEPTION
		 */
		.name = NSS_PPENL_EXCEPTION_FAMILY,		/* Exception Family */
		.entry = NSS_PPENL_EXCEPTION_INIT,		/* Init */
		.exit = NSS_PPENL_EXCEPTION_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_EXCEPTION		/* 1 or 0 */
	},
#ifdef NSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
	{
		/*
		 * NSS_PPENL_DSCP
		 */
		.name = NSS_PPENL_DSCP_FAMILY,		/* DSCP Family */
		.entry = NSS_PPENL_DSCP_INIT,		/* Init */
		.exit = NSS_PPENL_DSCP_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_DSCP		/* 1 or 0 */
	},
#endif
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	{
		/*
		 * NSS_PPENL_PM
		 */
		.name = NSS_PPENL_PM_FAMILY,		/* PM Family */
		.entry = NSS_PPENL_PM_INIT,		/* Init */
		.exit = NSS_PPENL_PM_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_PM		/* 1 or 0 */
	},
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	{
		/*
		 * NSS_PPENL_VLAN
		 */
		.name = NSS_PPENL_VLAN_FAMILY,           /* VLAN Family */
		.entry = NSS_PPENL_VLAN_INIT,            /* Init */
		.exit = NSS_PPENL_VLAN_EXIT,             /* exit */
		.valid = CONFIG_NSS_PPENL_VLAN           /* 1 or 0 */
	},
#endif
#ifdef NSS_PPE_PPENL_PORT_MGMT_SUPPORT
	{
		/*
		 * NSS_PPENL_PORT_MGMT
		 */
		.name = NSS_PPENL_PORT_MGMT_FAMILY,		/* PORT_MGMT Family */
		.entry = NSS_PPENL_PORT_MGMT_INIT,		/* Init */
		.exit = NSS_PPENL_PORT_MGMT_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_PORT_MGMT		/* 1 or 0 */
	},
#endif
#ifdef NSS_PPE_EDMA_DDRQ_FEATURE_SUPPORT
	{
		/*
		 * NSS_PPENL_EDMA_DDRQ
		 */
		.name = NSS_PPENL_EDMA_DDRQ_FAMILY,	/* EDMA DDRQ Family */
		.entry = NSS_PPENL_EDMA_DDRQ_INIT,	/* Init */
		.exit = NSS_PPENL_EDMA_DDRQ_EXIT,	/* exit */
		.valid = CONFIG_NSS_PPENL_EDMA_DDRQ	/* 1 or 0 */
	},
#endif
#if defined(CONFIG_NSS_PPENL_TUN_RPS)
	{
		/*
		 * NSS_PPENL_TUN_RPS
		 */
		.name = NSS_PPENL_TUN_RPS_FAMILY,		/* Tunnel RPS Family */
		.entry = NSS_PPENL_TUN_RPS_INIT,		/* Init */
		.exit = NSS_PPENL_TUN_RPS_EXIT,			/* exit */
		.valid = CONFIG_NSS_PPENL_TUN_RPS		/* 1 or 0 */
	},
#endif
#ifdef NSS_PPE_MCAST_FEATURE_SUPPORT
	{
		/*
		 * NSS_PPENL_MCAST
		 */
		.name = NSS_PPENL_MCAST_FAMILY,       	/* Multicast Family */
		.entry = NSS_PPENL_MCAST_INIT,        	/* Init */
		.exit = NSS_PPENL_MCAST_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_MCAST       	/* 1 or 0 */
	},
#endif
#ifdef NSS_PPE_PORT_MIRROR_FEATURE_SUPPORT
	{
		/*
		 * NSS_PPENL_PORT_MIRROR
		 */
		.name = NSS_PPENL_PORT_MIRROR_FAMILY,		/* Port-mirror Family */
		.entry = NSS_PPENL_PORT_MIRROR_INIT,		/* Init */
		.exit = NSS_PPENL_PORT_MIRROR_EXIT,		/* exit */
		.valid = CONFIG_NSS_PPENL_PORT_MIRROR		/* 1 or 0 */
	},
#endif
};

#define NSS_PPENL_FAMILY_HANDLER_SZ ARRAY_SIZE(family_handlers)

/*
 * nss_ppenl_copy_msg()
 *	copy a existing NETLINK message into a new one
 *
 * NOTE: this returns the new SKB/message
 * 	This will be used for response
 */
struct sk_buff *nss_ppenl_copy_msg(struct sk_buff *orig)
{
	struct sk_buff *copy;
	struct nss_ppenl_cmn *cm;

	cm = nss_ppenl_get_data(orig);

	copy = skb_copy(orig, GFP_KERNEL);
	if (!copy) {
		nss_ppenl_warn("%d:unable to copy incoming message of len(%d)\n", cm->pid, orig->len);
		return NULL;
	}

	return copy;
}

/*
 * nss_ppenl_get_data()
 *	Returns start of payload data
 */
void  *nss_ppenl_get_data(struct sk_buff *skb)
{
	return genlmsg_data(NLMSG_DATA(skb->data));
}

/*
 * nss_ppenl_ucast_resp()
 *	send the response to the user (PID)
 *
 * NOTE: this assumes the socket to be available for reception
 */
int nss_ppenl_ucast_resp(struct sk_buff *skb)
{
	struct nss_ppenl_cmn *cm;
	struct net *net;

	cm = genlmsg_data(NLMSG_DATA(skb->data));

	net = (struct net *)cm->sock_data;
	cm->sock_data = 0;

	/*
	 * End the message as no more updates are left to happen
	 * After this message is assumed to be read-only
	 */
	genlmsg_end(skb, cm);
	return genlmsg_unicast(net, skb, cm->pid);
}

/*
 * nss_ppenl_get_msg()
 *	verifies and returns the message pointer
 */
struct nss_ppenl_cmn *nss_ppenl_get_msg(struct genl_family *family, struct genl_info *info, uint16_t cmd)
{
	struct nss_ppenl_cmn *cm;
	uint32_t pid;

	pid =  info->snd_portid;

	/*
	 * validate the common message header version & magic
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	cm = info->userhdr;
#else
	cm = genl_info_userhdr(info);
#endif
	if (nss_ppenl_cmn_chk_ver(cm, family->version) == false) {
		nss_ppenl_warn("%d, %s: version mismatch (%d)\n", pid, family->name, cm->version);
		return NULL;
	}

	/*
	 * check if the message len arrived matches with expected len
	 */
	if (nss_ppenl_cmn_get_cmd_len(cm) != family->hdrsize) {
		nss_ppenl_warn("%d, %s: invalid command len (%d)\n", pid, family->name, nss_ppenl_cmn_get_cmd_len(cm));
		return NULL;
	}

	cm->pid = pid;
	cm->sock_data = (nss_ptr_t)genl_info_net(info);

	return cm;
}

/*
 * nss_ppenl_init()
 *	init module
 */
static int __init nss_ppenl_init(void)
{
	struct nss_ppenl_family *family = NULL;
	int i = 0;
	bool status;

	nss_ppenl_info("NSS Netlink manager loaded: %s\n", NSS_CLIENT_BUILD_ID);

	/*
	 * initialize the handler families, the intention to init the
	 * families that are marked active
	 */
	family = &family_handlers[0];

	for (i = 0; i < NSS_PPENL_FAMILY_HANDLER_SZ; i++, family++) {
		/*
		 * Check if the family exists
		 */
		if (family->valid) {
			BUG_ON(!family->entry);
			status = family->entry();
			if (status) {
				nss_ppenl_info("attaching family:%s\n", family->name);
			} else {
				return -1;
			}
		} else {
			nss_ppenl_info("skipping family:%s\n", family->name);
			nss_ppenl_info("valid = %d, entry = %d\n", family->valid, !!family->entry);
		}
	}

	return 0;
}

/*
 * nss_ppenl_exit()
 *	deinit module
 */
static void __exit nss_ppenl_exit(void)
{
	struct nss_ppenl_family *family = NULL;
	int i = 0;
	bool status;

	nss_ppenl_info("NSS Netlink manager unloaded\n");

	/*
	 * initialize the handler families
	 */
	family = &family_handlers[0];

	for (i = 0; i < NSS_PPENL_FAMILY_HANDLER_SZ; i++, family++) {
		if (family->valid) {
			BUG_ON(!family->exit);
			status = family->exit();
			if (status) {
				nss_ppenl_info("attaching family:%s\n", family->name);
			}
		} else {
			nss_ppenl_info("skipping family:%s\n", family->name);
			nss_ppenl_info("valid = %d, entry = %d\n", family->valid, !!family->entry);
		}
	}
}

module_init(nss_ppenl_init);
module_exit(nss_ppenl_exit);

MODULE_DESCRIPTION("NSS PPE Netlink");
MODULE_LICENSE("Dual BSD/GPL");
