/******************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *
 ******************************************************************/

#ifndef __SWACL_IF_H__
#define __SWACL_IF_H__

#define SWACL_IF_NAME		        "swacl"
#define	SWACL_PROC_NAME_ROOT		"swacl"
#define	SWACL_BIND_IF_NUM_MAX		6

#define TX_TIMEOUT		        (6*HZ)
#define SWACL_IF_FILTER_BIT_LEN         32

#define SWACL_IF_NETDEV_TX_OK		0	// skb consumed by callee
#define SWACL_IF_NETDEV_TX_DROP		1	// skb consumed by callee, but dropped

#define OUI_LEN                         3
#define SWCAL_IF_PARSE_PREFIX           0xff
#define SWCAL_IF_PARSE_4PREFIX          0xffffffff

#define SWACL_IF_DEBUG_LVL_1            1
#define SWACL_IF_DEBUG_LVL_2            2

#define SWACL_IF_NAME_MAX_NUM           5

#define SWACL_IF_SUCCESS                         0
#define SWACL_IF_ERROR                          -1
#define SWACL_IF_ERROR_ACT                      -2
#define SWACL_IF_ERROR_ACT_CONFLICT             -3
#define SWACL_IF_ERROR_PERMIT_CONFLICT          -4
#define SWACL_IF_ERROR_NO_IFNAME                -5
#define SWACL_IF_ERROR_FILTER                   -6
#define SWACL_IF_ERROR_DELETE                   -7
#define SWACL_IF_ERROR_ADD                      -8

#define SWACL_IF_ERROR_MATCH_ACT                -9
#define SWACL_IF_ERROR_PERMIT_CONFLICT1         -10


#define SWACL_IF_MATCH_ACT_DROP                  0
#define SWACL_IF_MATCH_ACT_OK                    1
#define SWACL_IF_MATCH_ACT_REDIRECT              2


#define SWACL_IF_MATCH_NO_FILTER                 0
#define SWACL_IF_MATCH_FILTER_OK                 1

#define SWACL_STRING_BUFF_LEN                    1344
#define SWACL_STRING_PARAM_LEN			 256
#define SWACL_STRING_PARAM_ARRAY_LEN		 48
#define SWACL_STRING_DBG_PARAM_ARRAY_LEN	 12


enum swacl_filter_field_e {
	SWACL_IF_filter_NONE = 0,
	SWACL_IF_filter_OUI             = 0x1,
	SWACL_IF_filter_ETHERTYPE       = 0x2,
	SWACL_IF_filter_DMAC            = 0x4,
	SWACL_IF_filter_SMAC            = 0x8,
	SWACL_IF_filter_V4_PROTO        = 0x10,
	SWACL_IF_filter_V6_NXT_HEAD     = 0x20,
	SWACL_IF_filter_DIP             = 0x40,
	SWACL_IF_filter_SIP             = 0x80,
	SWACL_IF_filter_V6_DIP          = 0x100,
	SWACL_IF_filter_V6_SIP          = 0x200,
	SWACL_IF_filter_DPORT           = 0x400,
	SWACL_IF_filter_SPORT           = 0x800,
	SWACL_IF_filter_VLAN            = 0x1000,
	SWACL_IF_filter_VLAN_PRI        = 0x2000,
	SWACL_IF_filter_IFNAME          = 0x4000,
	SWACL_IF_filter_DIP_PRE         = 0x8000,
	SWACL_IF_filter_SIP_PRE         = 0x10000,
	SWACL_IF_filter_V6_DIP_PRE      = 0x20000,
	SWACL_IF_filter_V6_SIP_PRE      = 0x40000,
	SWACL_IF_filter_DPORT_END       = 0x80000,
	SWACL_IF_filter_SPORT_END       = 0x100000,
	SWACL_IF_filter_DMAC_PRE        = 0x200000,
	SWACL_IF_filter_SMAC_PRE        = 0x400000,
	SWACL_IF_filter_ALL_PACKET      = 0x800000,
};

enum swacl_action_e {
	SWACL_IF_ACTION_NONE            = 0,
	SWACL_IF_ACTION_PERMIN          = 0x1,
	SWACL_IF_ACTION_DROP		= 0x2,
	SWACL_IF_ACTION_REDIRECT	= 0x4,
	SWACL_IF_ACTION_VLANTAG_CHG     = 0x8,
	SWACL_IF_ACTION_PBIT_CHG        = 0x10,
	SWACL_IF_ACTION_RULE_PRIORITY   = 0x20,
	SWACL_IF_ACTION_RULE_DEL_ALL    = 0x40,
	SWACL_IF_ACTION_RULE_VLAN_REMOVE= 0x80,
	SWACL_IF_ACTION_RULE_CLEAR      = 0x10000000,
	SWACL_IF_ACTION_RULE_ADD        = 0x20000000,
	SWACL_IF_ACTION_RULE_DEL        = 0x40000000,
	SWACL_IF_ACTION_RULE_COMMIT     = 0x80000000,
};

enum swacl_if_rule_act_mapping_e {
	SWACL_IF_ACTION_RULE_ADD_IDX=0,
	SWACL_IF_ACTION_RULE_DEL_IDX,
	SWACL_IF_ACTION_RULE_CLEAR_IDX,
	SWACL_IF_ACTION_RULE_COMMIT_IDX,
	SWACL_IF_ACTION_RULE_PRIORITY_IDX,
	SWACL_IF_ACTION_PERMIN_IDX,
	SWACL_IF_ACTION_DROP_IDX,
	SWACL_IF_ACTION_REDIRECT_IDX,
	SWACL_IF_ACTION_VLANTAG_CHG_IDX,
	SWACL_IF_ACTION_PBIT_CHG_IDX,
	SWACL_IF_ACTION_RULE_DEL_ALL_IDX,
	SWACL_IF_ACTION_RULE_REMOV_TAG_IDX
};
enum swacl_if_rule_mapping_e {
	SWACL_IF_filter_OUI_IDX = 0,
	SWACL_IF_filter_ETHERTYPE_IDX,
	SWACL_IF_filter_DMAC_IDX,
	SWACL_IF_filter_DMAC_PRE_IDX,
	SWACL_IF_filter_SMAC_IDX,
	SWACL_IF_filter_SMAC_PRE_IDX,
	SWACL_IF_filter_IFNAME_IDX,
	SWACL_IF_filter_V4_PROTO_IDX,
	SWACL_IF_filter_V6_NXT_HEAD_IDX,
	SWACL_IF_filter_DIP_IDX,
	SWACL_IF_filter_DIP_PRE_IDX,
	SWACL_IF_filter_SIP_IDX,
	SWACL_IF_filter_SIP_PRE_IDX,
	SWACL_IF_filter_V6_DIP_IDX,
	SWACL_IF_filter_V6_DIP_PRE_IDX,
	SWACL_IF_filter_V6_SIP_IDX,
	SWACL_IF_filter_V6_SIP_PRE_IDX,
	SWACL_IF_filter_DPORT_IDX,
	SWACL_IF_filter_DPORT_END_IDX,
	SWACL_IF_filter_SPORT_IDX,
	SWACL_IF_filter_SPORT_END_IDX,
	SWACL_IF_filter_VLAN_IDX,
	SWACL_IF_filter_VLAN_PRI_IDX,
	SWACL_IF_filter_ALL_PACKET_IDX,
	SWACL_IF_filter_ALL_IDX
};

struct swacl_if_rule_entry {
	unsigned short     is_commit:1;
	unsigned short     priority;
	unsigned int       filter_flags;
	unsigned int       action_flags;
	unsigned char      oui[OUI_LEN];
	unsigned short     ethertype;
	unsigned char      dmac[ETH_ALEN];
	unsigned short     dmac_prefix;
	unsigned char      smac[ETH_ALEN];
	unsigned short     smac_prefix;
	char               bind_ifname[SWACL_IF_NAME_MAX_NUM][IFNAMSIZ];
	char               redirect_ifname[SWACL_IF_NAME_MAX_NUM][IFNAMSIZ];
	unsigned char      v4_protocol;
	unsigned char      v6_next_header;
	unsigned int       dip;
	unsigned short     dip_prefix;
	unsigned int       sip;
	unsigned short     sip_prefix;
	struct in6_addr    v6dip;
	unsigned short     v6dip_prefix;
	struct in6_addr    v6sip;
	unsigned short     v6sip_prefix;
	unsigned short     dport_start;
	unsigned short     dport_end;
	unsigned short     sport_start;
	unsigned short     sport_end;
	unsigned short     vlantag_id;
	short              vlan_tag_pbit;
	unsigned short     vlan_mark;
	short              pbit_mark;
	unsigned int       hit_counter;
	struct list_head   swacl_entry;

};

struct swacl_if_priv {
	struct miscdevice        swacl_dev;
	struct proc_dir_entry    *swacl_pde;
	unsigned int              debug_rx:2;
	unsigned int              debug_filter:2;
	unsigned int              debug_cmp:2;
	unsigned int              rx_packets;
	unsigned int              rx_dropped;
	unsigned int              rx_act;
	struct mutex		  swacl_mutex;
	struct list_head	  swacl_list;

};
static inline char *
purename(char *name)
{
	char *s;
	for (s = name + strlen(name) - 1; s >= name; s--)
		if (*s == '/')
			return s + 1;
	return name;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/net_namespace.h>
#define dev_get_byname(ifname)		dev_get_by_name(&init_net, ifname)
#define dev_get_byindex(ifindex)	dev_get_by_index(&init_net, ifindex)
#else
#define dev_get_byname(ifname)		dev_get_by_name(ifname)
#define dev_get_byindex(ifindex)	dev_get_by_index(ifindex)
#endif

#define dbprintk(fmt, args...)		printk(KERN_ERR "%s:%d:%s(): " fmt, purename(__FILE__), __LINE__, __FUNCTION__, ##args)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#endif
