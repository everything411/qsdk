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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/netfilter.h>
#include <linux/unistd.h>

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
//#include <linux/netdevice.h>
//#include <linux/etherdevice.h>
#include <linux/list.h>

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/if_arp.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/mutex.h>
#include <linux/ip.h>
#include <linux/ipv6.h>


#include <net/switchdev.h>
#include <linux/miscdevice.h>
#include "swacl_if.h"
#include "swacl_util.h"


#define SWACL_IF_ACTION_ALL_FLAGE (SWACL_IF_ACTION_RULE_ADD|SWACL_IF_ACTION_RULE_DEL|SWACL_IF_ACTION_RULE_CLEAR)

#define SWACL_IF_ACTION_CONFLICT1_FLAGE (SWACL_IF_ACTION_RULE_ADD|SWACL_IF_ACTION_RULE_DEL)
#define SWACL_IF_ACTION_CONFLICT2_FLAGE (SWACL_IF_ACTION_RULE_ADD|SWACL_IF_ACTION_RULE_CLEAR)
#define SWACL_IF_ACTION_CONFLICT3_FLAGE (SWACL_IF_ACTION_RULE_ADD|SWACL_IF_ACTION_RULE_COMMIT)
#define SWACL_IF_ACTION_CONFLICT4_FLAGE (SWACL_IF_ACTION_RULE_ADD|SWACL_IF_ACTION_RULE_COMMIT)
#define SWACL_IF_ACTION_CONFLICT5_FLAGE (SWACL_IF_ACTION_RULE_CLEAR|SWACL_IF_ACTION_RULE_DEL)
#define SWACL_IF_ACTION_CONFLICT6_FLAGE (SWACL_IF_ACTION_RULE_DEL|SWACL_IF_ACTION_RULE_COMMIT)
#define SWACL_IF_ACTION_CONFLICT7_FLAGE (SWACL_IF_ACTION_RULE_CLEAR|SWACL_IF_ACTION_RULE_COMMIT)

#define SWACL_IF_ACTION_CONFLICT_PERMIT_FLAGE (SWACL_IF_ACTION_PERMIN|SWACL_IF_ACTION_DROP)
#define SWACL_IF_ACTION_CONFLICT1_PERMIT_FLAGE (SWACL_IF_ACTION_REDIRECT|SWACL_IF_ACTION_DROP)


#define SWACL_IF_FILTER_ALL_FLAGE	(SWACL_IF_filter_OUI|SWACL_IF_filter_ETHERTYPE|SWACL_IF_filter_DMAC|SWACL_IF_filter_SMAC \
	                                 |SWACL_IF_filter_V4_PROTO|SWACL_IF_filter_V6_NXT_HEAD|SWACL_IF_filter_SIP|SWACL_IF_filter_DIP \
	                                 |SWACL_IF_filter_V6_DIP|SWACL_IF_filter_V6_SIP|SWACL_IF_filter_DPORT|SWACL_IF_filter_SPORT \
	                                 |SWACL_IF_filter_VLAN|SWACL_IF_filter_VLAN_PRI|SWACL_IF_filter_IFNAME|SWACL_IF_filter_DIP_PRE \
	                                 |SWACL_IF_filter_SIP_PRE|SWACL_IF_filter_V6_DIP_PRE|SWACL_IF_filter_V6_SIP_PRE|SWACL_IF_filter_DPORT_END \
	                                 |SWACL_IF_filter_SPORT_END|SWACL_IF_filter_DMAC_PRE|SWACL_IF_filter_SMAC_PRE|SWACL_IF_filter_ALL_PACKET)
struct swacl_rule_name_map_id {
	unsigned int    idx;
	char            *swacl_field_name;
	unsigned int    field_flags;
	int             is_support;
	char            *help_str;
	int (*parse_filter_func)(char * param_str,struct swacl_if_rule_entry * rule_entry);
	int (*to_str_func)(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry);
};

struct swacl_rule_name_map_id g_swacl_rule_mapping[] = {
	{SWACL_IF_filter_OUI_IDX,"oui",SWACL_IF_filter_OUI,1,"filter OUI xx:xx:xx",swacl_if_parse_OUI,swacl_if_OUI_to_str},
	{SWACL_IF_filter_ETHERTYPE_IDX,"ethertype",SWACL_IF_filter_ETHERTYPE,1,"ethertype 0x0806",swacl_if_parse_ethertype,swacl_if_ethertype_to_str},
	{SWACL_IF_filter_DMAC_IDX,"dmac",SWACL_IF_filter_DMAC,1,"filter DMAC xx:xx:xx:xx:xx:xx",swacl_if_parse_dmac,swacl_if_dmac_to_str},
	{SWACL_IF_filter_DMAC_PRE_IDX,"dmac_prefix",SWACL_IF_filter_DMAC_PRE,1,"filter DMAC prefix 8,16,..,48",swacl_if_parse_dmac_prefix,swacl_if_dmac_prefix_to_str},
	{SWACL_IF_filter_SMAC_IDX,"smac",SWACL_IF_filter_SMAC,1,"filter SMAC xx:xx:xx:xx:xx:xx",swacl_if_parse_smac,swacl_if_smac_to_str},
	{SWACL_IF_filter_SMAC_PRE_IDX,"smac_prefix",SWACL_IF_filter_SMAC_PRE,1,"filter SMAC prefix 8,16,..,48",swacl_if_parse_smac_prefix,swacl_if_smac_prefix_to_str},
	{SWACL_IF_filter_IFNAME_IDX,"ifname",SWACL_IF_filter_IFNAME,1,"filter IFNAME (ifname ethx)",swacl_if_parse_ifname,swacl_if_ifname_to_str},
	{SWACL_IF_filter_V4_PROTO_IDX,"v4_proto",SWACL_IF_filter_V4_PROTO,1,"v4_proto 17(udp)",swacl_if_parse_v4proto,swacl_if_v4proto_to_str},
	{SWACL_IF_filter_V6_NXT_HEAD_IDX,"v6_next_header",SWACL_IF_filter_V6_NXT_HEAD,1,"v6_next_header 17(udp)",swacl_if_parse_v6_nxt_header,swacl_if_v6_nxt_header_to_str},
	{SWACL_IF_filter_DIP_IDX,"dip",SWACL_IF_filter_DIP,1,"dip x.x.x.x",swacl_if_parse_dip,swacl_if_dip_to_str},
	{SWACL_IF_filter_DIP_PRE_IDX,"dip_prefix",SWACL_IF_filter_DIP_PRE,1,"dip_prefix 8.step 8",swacl_if_parse_dip_prefix,swacl_if_dip_prefix_to_str},
	{SWACL_IF_filter_SIP_IDX,"sip",SWACL_IF_filter_SIP,1,"sip x.x.x.x",swacl_if_parse_sip,swacl_if_sip_to_str},
	{SWACL_IF_filter_SIP_PRE_IDX,"sip_prefix",SWACL_IF_filter_SIP_PRE,1,"sip_prefix 8.step 8",swacl_if_parse_sip_prefix,swacl_if_sip_prefix_to_str},
	{SWACL_IF_filter_V6_DIP_IDX,"v6dip",SWACL_IF_filter_V6_DIP,1,"v6dip xx:xx....",swacl_if_parse_v6_dip,swacl_if_v6_dip_to_str},
	{SWACL_IF_filter_V6_DIP_PRE_IDX,"v6dip_prefix",SWACL_IF_filter_V6_DIP_PRE,1,"v6dip_prefix 8,step 8",swacl_if_parse_v6_dip_prefix,swacl_if_v6_dip_prefix_to_str},
	{SWACL_IF_filter_V6_SIP_IDX,"v6sip",SWACL_IF_filter_V6_SIP,1,"v6sip xx:xx....",swacl_if_parse_v6_sip,swacl_if_v6_sip_to_str},
	{SWACL_IF_filter_V6_SIP_PRE_IDX,"v6sip_prefix",SWACL_IF_filter_V6_SIP_PRE,1,"v6sip_prefix 8,step 8",swacl_if_parse_v6_sip_prefix,swacl_if_v6_sip_prefix_to_str},
	{SWACL_IF_filter_DPORT_IDX,"dport_start",SWACL_IF_filter_DPORT,1,"dport_start 1024",swacl_if_parse_dport,swacl_if_dport_to_str},
	{SWACL_IF_filter_DPORT_END_IDX,"dport_end",SWACL_IF_filter_DPORT_END,1,"dport_end 1026",swacl_if_parse_dport_end,swacl_if_dport_end_to_str},
	{SWACL_IF_filter_SPORT_IDX,"sport_start",SWACL_IF_filter_SPORT,1,"sport_start 1024",swacl_if_parse_sport,swacl_if_sport_to_str},
	{SWACL_IF_filter_SPORT_END_IDX,"sport_end",SWACL_IF_filter_SPORT_END,1,"sport_end 1026",swacl_if_parse_sport_end,swacl_if_sport_end_to_str},
	{SWACL_IF_filter_VLAN_IDX,"vlanid",SWACL_IF_filter_VLAN,1,"vlanid 4091",swacl_if_parse_filter_vlan,swacl_if_vlan_to_str},
	{SWACL_IF_filter_VLAN_PRI_IDX,"pbit",SWACL_IF_filter_VLAN_PRI,1,"pbit 7",swacl_if_parse_filter_vlan_pbit,swacl_if_vlan_pbit_to_str},
	{SWACL_IF_filter_ALL_PACKET_IDX,"filter_all",SWACL_IF_filter_ALL_PACKET,1,"filter alll packet",NULL,NULL},

} ;

struct swacl_rule_name_map_id g_swacl_act_rule_mapping[] = {
	{SWACL_IF_ACTION_RULE_ADD_IDX,"rule_add",SWACL_IF_ACTION_RULE_ADD,1,"rule add cmd",NULL,NULL},
	{SWACL_IF_ACTION_RULE_DEL_IDX,"rule_del",SWACL_IF_ACTION_RULE_DEL,1,"rule delete cmd",NULL,NULL},
	{SWACL_IF_ACTION_RULE_CLEAR_IDX,"rule_clear",SWACL_IF_ACTION_RULE_CLEAR,0,"rule clear cmd",NULL,NULL},
	{SWACL_IF_ACTION_RULE_COMMIT_IDX,"rule_commit",SWACL_IF_ACTION_RULE_COMMIT,0,"rule commit cmd",NULL,NULL},
	{SWACL_IF_ACTION_RULE_PRIORITY_IDX,"rule_pri",SWACL_IF_ACTION_RULE_PRIORITY,1,"rule ptiority (0 is highest)",swacl_if_parse_rule_priority,swacl_if_rule_priority_to_str},
	{SWACL_IF_ACTION_PERMIN_IDX,"act_permit",SWACL_IF_ACTION_PERMIN,1,"permit rule action",NULL,NULL},
	{SWACL_IF_ACTION_DROP_IDX,"act_drop",SWACL_IF_ACTION_DROP,1,"drop rule action",NULL,NULL},
	{SWACL_IF_ACTION_REDIRECT_IDX,"act_redirect",SWACL_IF_ACTION_REDIRECT,1,"redirect action",swacl_if_parse_act_redirect,swacl_if_act_redirect_to_str},
	{SWACL_IF_ACTION_VLANTAG_CHG_IDX,"vlanid_mark",SWACL_IF_ACTION_VLANTAG_CHG,1,"change or vlanid",swacl_if_parse_act_vlan,swacl_if_act_vlan_to_str},
	{SWACL_IF_ACTION_PBIT_CHG_IDX,"pbit_mark",SWACL_IF_ACTION_PBIT_CHG,1,"chnage or add pbit",swacl_if_parse_act_pbit,swacl_if_act_pbit_to_str},
	{SWACL_IF_ACTION_RULE_DEL_ALL_IDX,"rule_del_all",SWACL_IF_ACTION_RULE_DEL_ALL,1,"rule delete all",NULL,NULL},
	{SWACL_IF_ACTION_RULE_REMOV_TAG_IDX,"act_vlan_remove",SWACL_IF_ACTION_RULE_VLAN_REMOVE,1,"vlan remove",NULL,NULL},

};

struct swacl_if_priv g_swacl_priv;
//struct swacl_if_rule_entry g_swacl_rule_entry;
int (*swacl_net_recv) (struct sk_buff ** pskb);

static int
swacl_if_get_filter_map_idx(unsigned int umap_idx)
{
	int i;

	for( i = 0; i < ARRAY_SIZE(g_swacl_rule_mapping); i++) {
		if(umap_idx == g_swacl_rule_mapping[i].field_flags)
			return g_swacl_rule_mapping[i].idx;

	}
	return -1;
}
static int
swacl_if_get_act_map_idx(unsigned int umap_idx)
{
	int i;

	for( i = 0; i < ARRAY_SIZE(g_swacl_act_rule_mapping); i++) {
		if(umap_idx == g_swacl_act_rule_mapping[i].field_flags)
			return g_swacl_act_rule_mapping[i].idx;

	}
	return -1;
}

static void
swacl_if_reset_global_rule_entry(struct swacl_if_rule_entry * rule_entry)
{
	memset(rule_entry,0x0,sizeof(struct swacl_if_rule_entry));
	INIT_LIST_HEAD(&rule_entry->swacl_entry);
}
static int
swacl_if_add_rule_entry(struct swacl_if_rule_entry * rule_entry)
{
	struct swacl_if_rule_entry * rule_entry_pos,* rule_entry_nxt;
	int i=0;
	rcu_read_lock();
	list_for_each_entry_rcu(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {


		if(swacl_if_filter_cmp(rule_entry,rule_entry_pos) == SWACL_IF_MATCH_FILTER_OK ) {
			dbprintk("match rule(%d) exit (0x%4.4x)\n", i,rule_entry->filter_flags);
			rcu_read_unlock();
			return -1;
		}
		i++;
	}

	rcu_read_unlock();

	mutex_lock(&g_swacl_priv.swacl_mutex);
	if(!(rule_entry->action_flags&SWACL_IF_ACTION_DROP))
		rule_entry->action_flags |= SWACL_IF_ACTION_PERMIN;


	//add by priority
	if(list_empty(&g_swacl_priv.swacl_list)) {
		list_add_rcu(&rule_entry->swacl_entry,&g_swacl_priv.swacl_list);

	} else {
		rule_entry_nxt =NULL;
		//found if same priority
		list_for_each_entry(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {

			if(rule_entry->priority == rule_entry_pos->priority) {
				rule_entry_nxt = rule_entry_pos;

			}

		}
		//found same priority
		if(rule_entry_nxt) {
			list_add_rcu(&rule_entry->swacl_entry,&rule_entry_nxt->swacl_entry);
			goto add_entry_done;
		}

		list_for_each_entry(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {

			if(rule_entry->priority < rule_entry_pos->priority) {
				///add pre
				__list_add_rcu(&rule_entry->swacl_entry,rule_entry_pos->swacl_entry.prev,&rule_entry_pos->swacl_entry);
				goto add_entry_done;

			}


		}
		list_for_each_entry_reverse(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {


			if(rule_entry->priority > rule_entry_pos->priority) {
				list_add_rcu(&rule_entry->swacl_entry,&rule_entry_pos->swacl_entry);
				goto add_entry_done;

			}


		}
	}
add_entry_done:
	mutex_unlock(&g_swacl_priv.swacl_mutex);
	return 0;


}
static int swacl_if_del_rule_entry_all(void)
{
	struct swacl_if_rule_entry *rule_entry_pos,*rule_entry_nxt;

	mutex_lock(&g_swacl_priv.swacl_mutex);
	list_for_each_entry_safe(rule_entry_pos,rule_entry_nxt,&g_swacl_priv.swacl_list,swacl_entry) {

		list_del(&rule_entry_pos->swacl_entry);
		synchronize_rcu();
		kfree(rule_entry_pos);
	}
	mutex_unlock(&g_swacl_priv.swacl_mutex);
	return 0;
}
static int
swacl_if_del_rule_entry(struct swacl_if_rule_entry * rule_entry)
{

	struct swacl_if_rule_entry * rule_entry_pos;
	int bfound=0;
	unsigned int match_filter=0;

	rcu_read_lock();
	list_for_each_entry_rcu(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {

		match_filter = 0;
		if((rule_entry_pos->filter_flags & rule_entry->filter_flags)==rule_entry->filter_flags) {
			if(swacl_if_filter_cmp(rule_entry,rule_entry_pos) == SWACL_IF_MATCH_FILTER_OK ) {
				bfound = 1;
				break;
			}

		}

	}
	rcu_read_unlock();

	if(bfound) {
		mutex_lock(&g_swacl_priv.swacl_mutex);
		list_del_rcu(&rule_entry_pos->swacl_entry);
		mutex_unlock(&g_swacl_priv.swacl_mutex);
		synchronize_rcu();
		kfree(rule_entry_pos);
		return 0;
	}
	return -1;
}
static int swacl_if_commit_global_rule_entry(struct swacl_if_rule_entry * rule_entry)
{
	if(!(rule_entry->action_flags & SWACL_IF_ACTION_ALL_FLAGE))
		return SWACL_IF_ERROR_ACT;

	if((rule_entry->action_flags & SWACL_IF_ACTION_ALL_FLAGE)==SWACL_IF_ACTION_ALL_FLAGE)
		return SWACL_IF_ERROR_ACT_CONFLICT;

	if((rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT1_FLAGE)==SWACL_IF_ACTION_CONFLICT1_FLAGE ||
	                (rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT2_FLAGE)==SWACL_IF_ACTION_CONFLICT2_FLAGE ||
	                (rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT3_FLAGE)==SWACL_IF_ACTION_CONFLICT3_FLAGE ||
	                (rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT4_FLAGE)==SWACL_IF_ACTION_CONFLICT4_FLAGE ||
	                (rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT5_FLAGE)==SWACL_IF_ACTION_CONFLICT5_FLAGE ||
	                (rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT6_FLAGE)==SWACL_IF_ACTION_CONFLICT6_FLAGE ||
	                (rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT7_FLAGE)==SWACL_IF_ACTION_CONFLICT7_FLAGE )
		return SWACL_IF_ERROR_ACT_CONFLICT;

	if((rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT_PERMIT_FLAGE) == SWACL_IF_ACTION_CONFLICT_PERMIT_FLAGE)
		return SWACL_IF_ERROR_PERMIT_CONFLICT;
	if((rule_entry->action_flags & SWACL_IF_ACTION_CONFLICT1_PERMIT_FLAGE) == SWACL_IF_ACTION_CONFLICT1_PERMIT_FLAGE)
		return SWACL_IF_ERROR_PERMIT_CONFLICT;
	
	if((rule_entry->action_flags & SWACL_IF_ACTION_PERMIN) &&  !(rule_entry->action_flags&SWACL_IF_ACTION_VLANTAG_CHG) &&  !(rule_entry->action_flags&SWACL_IF_ACTION_RULE_VLAN_REMOVE))
		return SWACL_IF_ERROR_PERMIT_CONFLICT1;

	if(!(rule_entry->filter_flags & SWACL_IF_filter_IFNAME))
		return SWACL_IF_ERROR_NO_IFNAME;

	if(!(rule_entry->filter_flags &SWACL_IF_FILTER_ALL_FLAGE))
		return SWACL_IF_ERROR_FILTER;


	if(rule_entry->action_flags & SWACL_IF_ACTION_RULE_DEL || rule_entry->action_flags & SWACL_IF_ACTION_RULE_CLEAR) {
		if(swacl_if_del_rule_entry(rule_entry) < 0)
			return SWACL_IF_ERROR_DELETE;
	}

	if(rule_entry->action_flags & SWACL_IF_ACTION_RULE_ADD) {
		if(swacl_if_add_rule_entry(rule_entry) < 0)
			return SWACL_IF_ERROR_ADD;
	}

	return 0;
}

static int
swacl_if_open(struct inode *inode, struct file *filp)
{


	return 0;
}

static int
swacl_if_close(struct inode *inode, struct file *filp)
{
	//netif_stop_queue(dev);

	module_put(THIS_MODULE);
	return 0;
}

#if 0
static int
swacl_if_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return -EOPNOTSUPP;
}
#endif


static const struct file_operations swacl_if_fops = {
	.owner		= THIS_MODULE,
	.open   	= swacl_if_open,
	.release	= swacl_if_close,
};


static int
swacl_if_match_action_rule(struct sk_buff **pskb,struct swacl_if_rule_entry * rule_entry)
{
	struct sk_buff *skb,*skb2;
	u16 vlan_tci = 0,vlan_pbit=0;
	struct net_device *lowdev;
	int i = 0;
	int ret = SWACL_IF_ERROR_MATCH_ACT;

	skb= *pskb;
	if(rule_entry == NULL ||skb == NULL) {
		return SWACL_IF_ERROR_MATCH_ACT;
	}

	if((rule_entry->action_flags&SWACL_IF_ACTION_DROP)) {
		return SWACL_IF_MATCH_ACT_DROP;
	}

	if(rule_entry->action_flags&SWACL_IF_ACTION_VLANTAG_CHG) {

		if(rule_entry->action_flags&SWACL_IF_ACTION_PBIT_CHG)
			vlan_pbit = rule_entry->pbit_mark;
		else
			vlan_pbit = 0;

		vlan_tci = (rule_entry->vlan_mark) | (vlan_pbit << VLAN_PRIO_SHIFT);

		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_tci);
		ret = SWACL_IF_MATCH_ACT_OK;

	}

	if(rule_entry->action_flags&SWACL_IF_ACTION_RULE_VLAN_REMOVE) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
		__vlan_hwaccel_clear_tag(skb);
#else
		skb->vlan_tci = 0;
#endif
		ret = SWACL_IF_MATCH_ACT_OK;

	}

	if(rule_entry->action_flags&SWACL_IF_ACTION_REDIRECT) {

		for( i = 0 ; i<SWACL_IF_NAME_MAX_NUM ; i++) {


			if(strlen(rule_entry->redirect_ifname[i]) !=0 ) {
				skb2 = skb_copy(skb, GFP_ATOMIC);
				if(skb2 != NULL) {
					skb_push(skb2, ETH_HLEN);
					if ((lowdev = dev_get_byname(rule_entry->redirect_ifname[i])) == NULL) {
						dbprintk("redirect xmit: no netdev exist(%s)\n",rule_entry->redirect_ifname[i]);
						return SWACL_IF_ERROR_MATCH_ACT;
					}

					if(rule_entry->action_flags&SWACL_IF_ACTION_VLANTAG_CHG) {

						if(rule_entry->action_flags&SWACL_IF_ACTION_PBIT_CHG)
							vlan_pbit = rule_entry->pbit_mark;
						else
							vlan_pbit = 0;

						vlan_tci = (rule_entry->vlan_mark) | (vlan_pbit << VLAN_PRIO_SHIFT);

						__vlan_hwaccel_put_tag(skb2, htons(ETH_P_8021Q), vlan_tci);
					}

					if(rule_entry->action_flags&SWACL_IF_ACTION_RULE_VLAN_REMOVE) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
						__vlan_hwaccel_clear_tag(skb2);
#else
						skb2->vlan_tci = 0;
#endif
					}
					skb2->dev = lowdev;
					dev_queue_xmit(skb2);
					dev_put(lowdev);
				}
			}

		}

		ret = SWACL_IF_MATCH_ACT_REDIRECT;

	}
	return ret;
}

int
swacl_if_handle_frame(struct sk_buff **pskb)
{
	struct swacl_if_rule_entry * rule_entry_pos;
	int i,j,cmp_b=0,match_act=0,no_match=0,idx=0,if_num=0,match_if;
	struct sk_buff *skb;
	unsigned int match_filter=0,cmp_prex=0;
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	unsigned short sport,dport;
	struct tcphdr *tcp_hdr;
	struct udphdr *udp_hdr;


	skb= *pskb;

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
		return RX_HANDLER_PASS;

	if(skb->dev ==NULL)
		return RX_HANDLER_PASS;

	if(strlen(skb->dev->name) ==0)
		return RX_HANDLER_PASS;

	list_for_each_entry_rcu(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {

		match_filter = 0;
		match_if = 0;

		for(if_num = 0 ; if_num < SWACL_IF_NAME_MAX_NUM ; if_num++) {
			if(strlen (rule_entry_pos->bind_ifname[if_num]) == 0)
				continue;
			if(strcmp(skb->dev->name,rule_entry_pos->bind_ifname[if_num]) == 0) {
				match_if = 1;
				break;
			}
		}

		if(match_if) {
			match_filter |= SWACL_IF_filter_IFNAME;

			for(i =0; i<SWACL_IF_FILTER_BIT_LEN; i++) {

				if((rule_entry_pos->filter_flags & (1<<i))) {
					switch((1<<i)) {
					case SWACL_IF_filter_ALL_PACKET:
						match_filter |= SWACL_IF_filter_ALL_PACKET;
						i = SWACL_IF_FILTER_BIT_LEN;
						continue;
					case SWACL_IF_filter_OUI:
						if(memcmp(rule_entry_pos->oui,eth_hdr(skb)->h_source,OUI_LEN)!=0) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						match_filter |= SWACL_IF_filter_OUI;
						break;
					case SWACL_IF_filter_ETHERTYPE:
						if(rule_entry_pos->ethertype != ntohs(skb->protocol)) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						match_filter |= SWACL_IF_filter_ETHERTYPE;
						break;
					case SWACL_IF_filter_DMAC:
						if(rule_entry_pos->filter_flags & SWACL_IF_filter_DMAC_PRE) {
							no_match = 0;
							cmp_b = (rule_entry_pos->dmac_prefix+7)/8;
							for(j=0; j<cmp_b; j++) {
								if(rule_entry_pos->dmac[j] != eth_hdr(skb)->h_dest[j]) {
									i = SWACL_IF_FILTER_BIT_LEN;
									no_match = 1;
									break;
								}
							}
							if(!no_match)
								match_filter |= SWACL_IF_filter_DMAC | SWACL_IF_filter_DMAC_PRE;

						} else {
							if(memcmp(rule_entry_pos->dmac,eth_hdr(skb)->h_dest,ETH_ALEN)!=0) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_DMAC;
						}
						break;
					case SWACL_IF_filter_DMAC_PRE:
						//do nothing
						break;
					case SWACL_IF_filter_SMAC:
						if(rule_entry_pos->filter_flags & SWACL_IF_filter_SMAC_PRE) {
							no_match = 0;
							cmp_b = (rule_entry_pos->smac_prefix+7)/8;
							for(j=0; j<cmp_b; j++) {
								if(rule_entry_pos->smac[j] != eth_hdr(skb)->h_source[j]) {
									i = SWACL_IF_FILTER_BIT_LEN;
									no_match = 1;
									break;
								}
							}
							if(!no_match)
								match_filter |= SWACL_IF_filter_SMAC | SWACL_IF_filter_SMAC_PRE;

						} else {
							if(memcmp(rule_entry_pos->smac,eth_hdr(skb)->h_source,ETH_ALEN)!=0) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_SMAC;
						}
						break;
					case SWACL_IF_filter_SMAC_PRE:
						//do nothing
						break;
					case SWACL_IF_filter_V4_PROTO:
						if (ntohs(skb->protocol)!=ETH_P_IP ) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						iph = (struct iphdr*)skb->data;
						//dbprintk("v4proto %d\n",iph->protocol);
						if (rule_entry_pos->v4_protocol != (u8)iph->protocol) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						match_filter |= SWACL_IF_filter_V4_PROTO;
						break;
					case SWACL_IF_filter_V6_NXT_HEAD:
						if (ntohs(skb->protocol)!=ETH_P_IPV6) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						ip6h = (struct ipv6hdr *)skb->data;
						if (rule_entry_pos->v6_next_header != ip6h->nexthdr) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						match_filter |= SWACL_IF_filter_V6_NXT_HEAD;
						break;
					case SWACL_IF_filter_DIP:
						if (ntohs(skb->protocol)!=ETH_P_IP) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						iph = (struct iphdr *)skb->data;
						if(rule_entry_pos->filter_flags & SWACL_IF_filter_DIP_PRE) {
							cmp_prex = SWCAL_IF_PARSE_4PREFIX << (32 -rule_entry_pos->dip_prefix);
							if ((rule_entry_pos->dip & cmp_prex) != (ntohl(iph->daddr)& cmp_prex)) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_DIP | SWACL_IF_filter_DIP_PRE;
						} else {
							if (rule_entry_pos->dip != ntohl(iph->daddr)) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_DIP;
						}

						break;
					case SWACL_IF_filter_DIP_PRE:
						//do nothing
						break;
					case SWACL_IF_filter_SIP:
						if (ntohs(skb->protocol)!=ETH_P_IP) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						iph = (struct iphdr *)skb->data;
						//dbprintk("filter sip,0x%x,0x%x,pre=%d\n",rule_entry_pos->sip,ntohl(iph->saddr)
						//		,rule_entry_pos->sip_prefix);
						if(rule_entry_pos->filter_flags & SWACL_IF_filter_SIP_PRE) {
							cmp_prex = SWCAL_IF_PARSE_4PREFIX << (32 -rule_entry_pos->sip_prefix);
							if ((rule_entry_pos->sip & cmp_prex) != (ntohl(iph->saddr)& cmp_prex)) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_SIP | SWACL_IF_filter_SIP_PRE;
						} else {
							if (rule_entry_pos->sip != ntohl(iph->saddr)) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_SIP;
						}
						break;
					case SWACL_IF_filter_SIP_PRE:
						//do nothing
						break;
					case SWACL_IF_filter_V6_DIP:
						if (ntohs(skb->protocol)!=ETH_P_IPV6 ) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}

						ip6h = (struct ipv6hdr*)skb->data;

						if(rule_entry_pos->filter_flags & SWACL_IF_filter_V6_DIP_PRE) {
							cmp_b = (rule_entry_pos->v6dip_prefix+7)/8;
							no_match = 0;
							for(j=0; j<cmp_b; j++) {
								if(rule_entry_pos->v6dip.s6_addr[j] != ip6h->daddr.s6_addr[j]) {
									i = SWACL_IF_FILTER_BIT_LEN;
									no_match = 1;
									break;
								}
							}

							if(!no_match)
								match_filter |= SWACL_IF_filter_V6_DIP | SWACL_IF_filter_V6_DIP_PRE;
						} else {
							if (memcmp(&rule_entry_pos->v6dip,&ip6h->daddr,sizeof(struct in6_addr))!=0 ) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_V6_DIP;
						}

						break;
					case SWACL_IF_filter_V6_DIP_PRE:
						//do nothing
						break;
					case SWACL_IF_filter_V6_SIP:
						if (ntohs(skb->protocol)!=ETH_P_IPV6 ) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}

						ip6h = (struct ipv6hdr *)skb->data;

						if(rule_entry_pos->filter_flags & SWACL_IF_filter_V6_SIP_PRE) {
							cmp_b = (rule_entry_pos->v6sip_prefix+7)/8;
							no_match = 0;
							for(j=0; j<cmp_b; j++) {
								if(rule_entry_pos->v6sip.s6_addr[j] != ip6h->saddr.s6_addr[j]) {
									i = SWACL_IF_FILTER_BIT_LEN;
									no_match = 1;
									break;
								}
							}
							if(!no_match)
								match_filter |= SWACL_IF_filter_V6_SIP | SWACL_IF_filter_V6_SIP_PRE;
						} else {
							if (memcmp(&rule_entry_pos->v6sip,&ip6h->saddr,sizeof(struct in6_addr))!=0 ) {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							match_filter |= SWACL_IF_filter_V6_SIP;
						}

						break;
					case SWACL_IF_filter_V6_SIP_PRE:
						//do nothing
						break;
					case SWACL_IF_filter_DPORT:
						//dbprintk("dport filter \n");
						if (ntohs(skb->protocol)!=ETH_P_IPV6 && ntohs(skb->protocol)!=ETH_P_IP) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}

						if(ntohs(skb->protocol)==ETH_P_IP) {
							iph = (struct iphdr *)skb->data;
							//dbprintk("dport filter ip protto(%d)\n",iph->protocol);
							if(iph->protocol == IPPROTO_TCP) {
								tcp_hdr=(struct tcphdr *)(skb->data+sizeof(struct iphdr));
								dport = ntohs(tcp_hdr->dest);
							} else if(iph->protocol == IPPROTO_UDP) {
								udp_hdr = (struct udphdr *)(skb->data+sizeof(struct iphdr));
								dport = ntohs(udp_hdr->dest);

							} else {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}
							//dbprintk("dport filter dport=%d \n",dport);
							if(rule_entry_pos->filter_flags & SWACL_IF_filter_DPORT_END) {
								if(rule_entry_pos->dport_start > dport || rule_entry_pos->dport_end < dport ) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_DPORT | SWACL_IF_filter_DPORT_END;
							} else {
								if(rule_entry_pos->dport_start != dport) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_DPORT ;
							}
							continue;

						} else if(ntohs(skb->protocol)==ETH_P_IPV6) {
							ip6h = (struct ipv6hdr *)skb->data;;

							if(ip6h->nexthdr == NEXTHDR_TCP) {
								tcp_hdr = (struct tcphdr *)(skb->data+sizeof(struct ipv6hdr));
								dport = ntohs(tcp_hdr->dest);
							} else if(ip6h->nexthdr ==  NEXTHDR_UDP) {
								udp_hdr = (struct udphdr *)(skb->data+sizeof(struct ipv6hdr));
								dport = ntohs(udp_hdr->dest);

							} else {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}

							if(rule_entry_pos->filter_flags & SWACL_IF_filter_DPORT_END) {
								if(rule_entry_pos->dport_start > dport || rule_entry_pos->dport_end < dport ) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_DPORT | SWACL_IF_filter_DPORT_END;
							} else {
								if(rule_entry_pos->dport_start != dport) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_DPORT ;
							}
							continue;

						} else {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}

						break;
					case SWACL_IF_filter_DPORT_END:
						//do nothing
						break;
					case SWACL_IF_filter_SPORT:
						//dbprintk("sport filter 11111 0x%x\n",ntohs(skb->protocol));
						if (ntohs(skb->protocol)!=ETH_P_IPV6 && ntohs(skb->protocol)!=ETH_P_IP) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}

						if(ntohs(skb->protocol)==ETH_P_IP) {
							iph = (struct iphdr *)skb->data;
							//dbprintk("sport filter ip protto(%d)\n",iph->protocol);
							if(iph->protocol == IPPROTO_TCP) {
								tcp_hdr=(struct tcphdr *)(skb->data+sizeof(struct iphdr));
								sport = ntohs(tcp_hdr->source);
								//dbprintk("tcp sport %d\n",sport);
							} else if(iph->protocol == IPPROTO_UDP) {
								udp_hdr = (struct udphdr *)(skb->data+sizeof(struct iphdr));
								sport = ntohs(udp_hdr->source);
								//dbprintk("udp sport %d\n",sport);

							} else {
								i = SWACL_IF_FILTER_BIT_LEN;
								dbprintk("no tcp udp 0x%x\n",iph->protocol);
								continue;
							}

							if(rule_entry_pos->filter_flags & SWACL_IF_filter_SPORT_END) {
								if(rule_entry_pos->sport_start > sport || rule_entry_pos->sport_end < sport ) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_SPORT | SWACL_IF_filter_SPORT_END;
							} else {
								if(rule_entry_pos->sport_start != sport) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_SPORT ;
							}

						} else if(ntohs(skb->protocol)==ETH_P_IPV6) {
							ip6h = (struct ipv6hdr *)skb->data;;

							if(ip6h->nexthdr == NEXTHDR_TCP) {
								tcp_hdr = (struct tcphdr *)(skb->data+sizeof(struct ipv6hdr));
								sport = ntohs(tcp_hdr->source);
								//dbprintk("tcp sport %d\n",sport);
							} else if(ip6h->nexthdr ==  NEXTHDR_UDP) {
								udp_hdr = (struct udphdr *)(skb->data+sizeof(struct ipv6hdr));
								sport = ntohs(udp_hdr->source);
								//dbprintk("udp sport %d\n",sport);

							} else {
								i = SWACL_IF_FILTER_BIT_LEN;
								continue;
							}

							if(rule_entry_pos->filter_flags & SWACL_IF_filter_SPORT_END) {
								if(rule_entry_pos->sport_start > sport || rule_entry_pos->sport_end < sport ) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_SPORT | SWACL_IF_filter_SPORT_END;
							} else {
								if(rule_entry_pos->sport_start != sport) {
									i = SWACL_IF_FILTER_BIT_LEN;
									continue;
								}
								match_filter |= SWACL_IF_filter_SPORT ;
							}

						} else {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						break;
					case SWACL_IF_filter_SPORT_END:
						//do nothing
						break;
					case SWACL_IF_filter_VLAN:
						if (!skb_vlan_tag_present(skb)) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						if(rule_entry_pos->vlantag_id != skb_vlan_tag_get_id(skb)) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						match_filter |= SWACL_IF_filter_VLAN;
						break;
					case SWACL_IF_filter_VLAN_PRI:
						if (!skb_vlan_tag_present(skb)) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						if(rule_entry_pos->vlan_tag_pbit != skb_vlan_tag_get_prio(skb)) {
							i = SWACL_IF_FILTER_BIT_LEN;
							continue;
						}
						match_filter |= SWACL_IF_filter_VLAN_PRI;
						break;
					default:
						break;

					}
				}
			}
		}
		if(g_swacl_priv.debug_rx >= SWACL_IF_DEBUG_LVL_2) {
			dbprintk("before idx[%d] match_filter 0x%x filter_flags 0x%x\n",idx,match_filter
			         ,rule_entry_pos->filter_flags);
		}
		if((match_filter & rule_entry_pos->filter_flags) == rule_entry_pos->filter_flags 
			|| (match_filter &SWACL_IF_filter_ALL_PACKET)==SWACL_IF_filter_ALL_PACKET ){
			//do match action
			match_act = swacl_if_match_action_rule(pskb,rule_entry_pos);

			if(g_swacl_priv.debug_rx >= SWACL_IF_DEBUG_LVL_1) {
				dbprintk("idx[%d]%s:match filter 0x%x-0x%x,act=%d\n",idx,skb->dev->name,match_filter
				         ,rule_entry_pos->filter_flags,match_act);
			}
			if(match_act == SWACL_IF_MATCH_ACT_DROP || match_act == SWACL_IF_MATCH_ACT_REDIRECT) {
				g_swacl_priv.rx_packets++;
				g_swacl_priv.rx_dropped++;
				rule_entry_pos->hit_counter++;
				kfree_skb(skb);
				return RX_HANDLER_CONSUMED;

			} else if(match_act == SWACL_IF_MATCH_ACT_OK) {

				g_swacl_priv.rx_packets++;
				g_swacl_priv.rx_act++;
				rule_entry_pos->hit_counter++;
				return RX_HANDLER_PASS;
			} else {
				if(g_swacl_priv.debug_rx >= SWACL_IF_DEBUG_LVL_2) {
					dbprintk("RX MATCH ACT UNKNOW %d\n",match_act);
				}
			}
		}
		idx++;
	}
	g_swacl_priv.rx_packets++;
	return RX_HANDLER_PASS;
}

static int
swacl_if_probe_one(void)
{
	int error;

	g_swacl_priv.swacl_dev.minor		= MISC_DYNAMIC_MINOR;
	g_swacl_priv.swacl_dev.name		= SWACL_IF_NAME;
	g_swacl_priv.swacl_dev.fops		= &swacl_if_fops;


	error = misc_register(&g_swacl_priv.swacl_dev);
	if (error) {
		pr_warn("Module registration error (name=%s, major=%d, minor=%d, err=%d)\n",
		        g_swacl_priv.swacl_dev.name,
		        MISC_MAJOR, g_swacl_priv.swacl_dev.minor,
		        error);
		return error;
	}

	pr_info("swacl device registered (name=%s, major=%d, minor=%d)\n",
	        g_swacl_priv.swacl_dev.name, MISC_MAJOR, g_swacl_priv.swacl_dev.minor);
	return 0;
}

static int
swacl_if_cmd_process(char *command)
{
	int argc,i,j,ret=0,param_len;
	struct swacl_if_rule_entry * swacl_rule_entry_p;
	char *argv[SWACL_STRING_PARAM_ARRAY_LEN];
	static char parse_param[SWACL_STRING_PARAM_LEN];

	argc = swacl_if_str2argv(command, argv, SWACL_STRING_PARAM_ARRAY_LEN);
	if (argc == 0)
		return 0;

	swacl_rule_entry_p = kmalloc(sizeof(struct swacl_if_rule_entry), GFP_KERNEL);
	if (!swacl_rule_entry_p) {
		dbprintk("no memory\n");
		return -ENOMEM;
	}
	memset(swacl_rule_entry_p,0x0,sizeof(struct swacl_if_rule_entry));

	for ( i = 0 ; i < argc ; i++) {
		if(i+1 >= argc)
			param_len = 0;
		else
			param_len = strlen(argv[i+1]);
		if(param_len > 0 && param_len <SWACL_STRING_PARAM_LEN)
			snprintf(parse_param,sizeof(parse_param),"%s",argv[i+1]);
		else
			parse_param[0]='\0';

		for( j = 0; j < ARRAY_SIZE(g_swacl_act_rule_mapping); j++) {
			if(strcmp(argv[i], g_swacl_act_rule_mapping[j].swacl_field_name) == 0) {

				switch (g_swacl_act_rule_mapping[j].field_flags) {

				case SWACL_IF_ACTION_RULE_ADD:
				case SWACL_IF_ACTION_RULE_DEL:
				case SWACL_IF_ACTION_RULE_CLEAR:
					swacl_if_reset_global_rule_entry(swacl_rule_entry_p);
					swacl_rule_entry_p->action_flags|=g_swacl_act_rule_mapping[j].field_flags;

					break;
				case SWACL_IF_ACTION_RULE_PRIORITY:
				case SWACL_IF_ACTION_VLANTAG_CHG:
				case SWACL_IF_ACTION_PBIT_CHG:
				case SWACL_IF_ACTION_REDIRECT:
					if(g_swacl_act_rule_mapping[j].parse_filter_func ==NULL)
						continue;
					if( i+1 >= argc ) {
						dbprintk("%s: error no parameter value \n", g_swacl_act_rule_mapping[j].swacl_field_name);
						goto err_return;
					}
					if ( (g_swacl_act_rule_mapping[j].parse_filter_func(argv[i+1],swacl_rule_entry_p) < 0)) {
						dbprintk("%s: parse err (%s)\n", g_swacl_act_rule_mapping[j].swacl_field_name
						         ,argv[i+1]);
						goto err_return;
					}
					swacl_rule_entry_p->action_flags|=g_swacl_act_rule_mapping[j].field_flags;
					break;
				case SWACL_IF_ACTION_RULE_DEL_ALL:
					swacl_if_del_rule_entry_all();
					kfree(swacl_rule_entry_p);
					return 0;
				case SWACL_IF_ACTION_DROP:
					swacl_rule_entry_p->action_flags|=g_swacl_act_rule_mapping[j].field_flags;
					break;
				case SWACL_IF_ACTION_RULE_VLAN_REMOVE:
					swacl_rule_entry_p->action_flags|=g_swacl_act_rule_mapping[j].field_flags;
					break;

#if 0
				case SWACL_IF_ACTION_RULE_COMMIT:
					if(argc != 1) {
						dbprintk("%s:conflict command\n", g_swacl_rule_mapping[j].swacl_field_name);
						goto err_return;
						return 0;
					} else { /*commit rule*/
						if(swacl_if_commit_global_rule_entry(swacl_rule_entry_p) < 0) {
							dbprintk("%s: error idx (%d) \n", g_swacl_rule_mapping[j].swacl_field_name);
							goto err_return;
						}
						printk("%s: success \n", g_swacl_rule_mapping[j].swacl_field_name);
						return 0;
					}
#endif
				}
			}
		}
	}
	if(g_swacl_priv.debug_filter >= SWACL_IF_DEBUG_LVL_2)
		dbprintk("cmd action_flags=0x%x\n",swacl_rule_entry_p->action_flags);

	if( !(swacl_rule_entry_p->action_flags & SWACL_IF_ACTION_ALL_FLAGE) ) {
		dbprintk("no set action rule cmd");
		goto err_return;
	}

	for ( i = 0 ; i < argc ; i++) {
		if(i+1 >= argc)
			param_len = 0;
		else
			param_len = strlen(argv[i+1]);

		if(param_len > 0 && param_len <SWACL_STRING_PARAM_LEN)
			snprintf(parse_param,sizeof(parse_param),"%s",argv[i+1]);
		else
			parse_param[0]='\0';

		if(g_swacl_priv.debug_filter >= SWACL_IF_DEBUG_LVL_2)
			dbprintk("cmd[%d]=%s,param=%s\n",i,argv[i],parse_param);

		for( j = 0; j < ARRAY_SIZE(g_swacl_rule_mapping); j++) {

			if(strcmp(argv[i], g_swacl_rule_mapping[j].swacl_field_name) == 0) {

				if((g_swacl_rule_mapping[j].field_flags&SWACL_IF_filter_ALL_PACKET)){
					swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_ALL_PACKET;
					break;
				}
				if(!(g_swacl_rule_mapping[j].field_flags & SWACL_IF_FILTER_ALL_FLAGE) ) {
					continue;
				}
				
				if(g_swacl_rule_mapping[j].parse_filter_func ==NULL)
					continue;

				if( i+1 >= argc ) {
					dbprintk("%s: error no parameter value \n", g_swacl_rule_mapping[j].swacl_field_name);
					goto err_return;
				}
				if ( (g_swacl_rule_mapping[j].parse_filter_func(argv[i+1],swacl_rule_entry_p) < 0)) {
					dbprintk("%s: parse err (%s)\n", g_swacl_rule_mapping[j].swacl_field_name
					         ,argv[i+1]);
					goto err_return;
				}



			}
		}
	}
	if(g_swacl_priv.debug_filter >= SWACL_IF_DEBUG_LVL_2)
		dbprintk("cmd filter_flags=0x%x\n",swacl_rule_entry_p->filter_flags);

	if((swacl_rule_entry_p->filter_flags&(SWACL_IF_filter_DMAC|SWACL_IF_filter_DMAC_PRE))==SWACL_IF_filter_DMAC) {
		swacl_rule_entry_p->dmac_prefix = 48;
		swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_DMAC_PRE;
	}

	if((swacl_rule_entry_p->filter_flags&(SWACL_IF_filter_SMAC|SWACL_IF_filter_SMAC_PRE))==SWACL_IF_filter_SMAC) {
		swacl_rule_entry_p->smac_prefix = 48;
		swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_SMAC_PRE;
	}

	if((swacl_rule_entry_p->filter_flags&(SWACL_IF_filter_DIP|SWACL_IF_filter_DIP_PRE))==SWACL_IF_filter_DIP) {
		swacl_rule_entry_p->dip_prefix = 32;
		swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_DIP_PRE;
	}

	if((swacl_rule_entry_p->filter_flags&(SWACL_IF_filter_SIP|SWACL_IF_filter_SIP_PRE))==SWACL_IF_filter_SIP) {
		swacl_rule_entry_p->sip_prefix = 32;
		swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_SIP_PRE;
	}

	if((swacl_rule_entry_p->filter_flags&(SWACL_IF_filter_V6_SIP|SWACL_IF_filter_V6_SIP_PRE))==SWACL_IF_filter_V6_SIP) {
		swacl_rule_entry_p->v6sip_prefix = 128;
		swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_V6_SIP_PRE;
	}

	if((swacl_rule_entry_p->filter_flags&(SWACL_IF_filter_V6_DIP|SWACL_IF_filter_V6_DIP_PRE))==SWACL_IF_filter_V6_DIP) {
		swacl_rule_entry_p->v6dip_prefix = 128;
		swacl_rule_entry_p->filter_flags |= SWACL_IF_filter_V6_DIP_PRE;
	}

	if((ret=swacl_if_commit_global_rule_entry(swacl_rule_entry_p)) == 0) {
		dbprintk("commit: success (%d) \n", ret);
		dbprintk("commit: success filter(%4x) \n", swacl_rule_entry_p->filter_flags);
		dbprintk("commit: success act(%4x) \n", swacl_rule_entry_p->action_flags);
		return 0;
	}
	dbprintk("commit: error (%d) \n", ret);
err_return:
	kfree(swacl_rule_entry_p);

	//printk("%s:cmd error\n", SWACL_PROC_NAME_ROOT );

	return 0;
}
static int
swacl_if_proc_entry_hit_clean(int clean_entry)
{
	int pos=0;
	struct swacl_if_rule_entry * rule_entry_pos;

	rcu_read_lock();
	list_for_each_entry_rcu(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {

		if(clean_entry == -1)
			rule_entry_pos->hit_counter = 0;
		else {
			if(clean_entry == pos)
				rule_entry_pos->hit_counter = 0;
		}
		pos++;
	}
	rcu_read_unlock();
	return 0;
}

static ssize_t
swacl_if_proc_debug_write(struct file *file, const char *buffer, size_t count, loff_t * data)
{
	static char debug_cmd[SWACL_STRING_PARAM_LEN];
	int argc = 0,i,value;
	char *argv[SWACL_STRING_DBG_PARAM_ARRAY_LEN];
	char *param_str;

	memset(debug_cmd,0x0,SWACL_STRING_PARAM_LEN);

	if (count > 0 && count < SWACL_STRING_PARAM_LEN) {
		if (copy_from_user(debug_cmd, buffer, count))
			return -EFAULT;
		debug_cmd[count - 1] = '\0';
		argc = swacl_if_str2argv(debug_cmd, argv, SWACL_STRING_DBG_PARAM_ARRAY_LEN);
	}

	for ( i = 0 ; i < argc ; i++) {
		if(strcmp(argv[i],"debug_rx") == 0) {
			if(i+1 >= argc) {
				dbprintk("debug_rx no param\n");
				goto debug_proc_done;
			}
			param_str = swacl_util_trim(argv[i+1]);
			if(!swacl_util_string_is_digit(param_str)) {
				dbprintk("debug_rx isn't digit\n");
				goto debug_proc_done;
			}
			value = simple_strtoul(param_str,NULL,10);
			g_swacl_priv.debug_rx = value;
		} else if(strcmp(argv[i],"debug_filter") == 0) {
			if(i+1 >= argc) {
				dbprintk("debug_filter no param\n");
				goto debug_proc_done;
			}
			param_str = swacl_util_trim(argv[i+1]);
			if(!swacl_util_string_is_digit(param_str)) {
				dbprintk("debug_filter isn't digit\n");
				goto debug_proc_done;
			}
			value = simple_strtoul(param_str,NULL,10);
			g_swacl_priv.debug_filter = value;
		} else if(strcmp(argv[i],"debug_cmp") == 0) {
			if(i+1 >= argc) {
				dbprintk("debug_cmp no param\n");
				goto debug_proc_done;
			}
			param_str = swacl_util_trim(argv[i+1]);
			if(!swacl_util_string_is_digit(param_str)) {
				dbprintk("debug_cmp isn't digit\n");
				goto debug_proc_done;
			}
			value = simple_strtoul(param_str,NULL,10);
			g_swacl_priv.debug_cmp = value;
		} else if(strcmp(argv[i],"clean_hit") == 0) {
			if(i+1 >= argc) {
				swacl_if_proc_entry_hit_clean(-1);
				goto debug_proc_done;
			}
			param_str = swacl_util_trim(argv[i+1]);
			if(!swacl_util_string_is_digit(param_str)) {
				dbprintk("debug_cmp isn't digit\n");
				goto debug_proc_done;
			}
			value = simple_strtoul(param_str,NULL,10);
			swacl_if_proc_entry_hit_clean(value);
		}

	}

debug_proc_done:
	return count;

}

static ssize_t
swacl_if_proc_entry_write(struct file *file, const char *buffer, size_t count, loff_t * data)
{
	return count;
}
static ssize_t
swacl_if_proc_cmd_write(struct file *file, const char *buffer, size_t count, loff_t * data)
{
	static char cmd[SWACL_STRING_BUFF_LEN];
	memset(cmd,0x0,SWACL_STRING_BUFF_LEN);
	if (count > 0 && count < SWACL_STRING_BUFF_LEN) {
		if (copy_from_user(cmd, buffer, count))
			return -EFAULT;
		cmd[count - 1] = '\0';
		swacl_if_cmd_process(cmd);
	}
	return count;
}

static int
swacl_if_proc_cmd_read(struct seq_file *seq, void *v)
{
	int i;

	// omci tx
	seq_printf(seq, "swacl cmd usage\n");

	for( i = 0; i< ARRAY_SIZE(g_swacl_act_rule_mapping); i++) {
		if(!g_swacl_act_rule_mapping[i].is_support)
			continue;

		seq_printf(seq, "action:%s --> (%s)\n",g_swacl_act_rule_mapping[i].swacl_field_name,g_swacl_act_rule_mapping[i].help_str);


	}
	for( i = 0; i< ARRAY_SIZE(g_swacl_rule_mapping); i++) {
		if(!g_swacl_rule_mapping[i].is_support)
			continue;

		if((g_swacl_rule_mapping[i].field_flags & SWACL_IF_FILTER_ALL_FLAGE)) {
			seq_printf(seq, "filter:%s --> (%s)\n",g_swacl_rule_mapping[i].swacl_field_name,g_swacl_rule_mapping[i].help_str);

		}

	}
	seq_printf(seq, "\n");

	return 0;

}
static int
swacl_if_proc_entry_read(struct seq_file *seq, void *v)
{
	int i,pos=0,idx;
	struct swacl_if_rule_entry * rule_entry_pos;
	char str_parse[128];

	rcu_read_lock();
	list_for_each_entry_rcu(rule_entry_pos,&g_swacl_priv.swacl_list,swacl_entry) {

		seq_printf(seq, "swacl rule entry (%d)\n",pos);
		seq_printf(seq, "hit: %u\n",rule_entry_pos->hit_counter);
		seq_printf(seq, "filter flags(%4x)\n",rule_entry_pos->filter_flags);
		seq_printf(seq, "action flags(%4x)\n",rule_entry_pos->action_flags);
		for(i =0; i<SWACL_IF_FILTER_BIT_LEN; i++) {
			if((rule_entry_pos->filter_flags & (1<<i))) {
				if((idx = swacl_if_get_filter_map_idx((1<<i)))>=0) {
					if(idx >= ARRAY_SIZE(g_swacl_rule_mapping)) {
						dbprintk("filter map idx  err (%d)\n",idx );
						continue;
					}
					memset(str_parse,0x0,128);
					if(g_swacl_rule_mapping[idx].to_str_func!=NULL
					                && g_swacl_rule_mapping[idx].to_str_func(str_parse,sizeof(str_parse),rule_entry_pos) == 0) {
						seq_printf(seq, "%s:%s\n",g_swacl_rule_mapping[idx].swacl_field_name,str_parse);
					}
				}


			}


		}

		for(i =0; i<SWACL_IF_FILTER_BIT_LEN; i++) {
			if((rule_entry_pos->action_flags & (1<<i))) {
				if((idx = swacl_if_get_act_map_idx((1<<i)))>=0) {
					if(idx >= ARRAY_SIZE(g_swacl_act_rule_mapping)) {
						dbprintk("act map idx  err (%d)\n",idx );
						continue;
					}
					memset(str_parse,0x0,128);
					if(g_swacl_act_rule_mapping[idx].to_str_func==NULL) {
						seq_printf(seq, "action:%s\n",g_swacl_act_rule_mapping[idx].swacl_field_name);

					} else {
						if(g_swacl_act_rule_mapping[idx].to_str_func(str_parse,sizeof(str_parse),rule_entry_pos) == 0) {
							seq_printf(seq, "%s:%s\n",g_swacl_act_rule_mapping[idx].swacl_field_name,str_parse);
						}
					}
				}


			}


		}
		pos++;
		seq_printf(seq, "\n");
	}
	rcu_read_unlock();
	return 0;

}
static int
swacl_if_proc_debug_read(struct seq_file *seq, void *v)
{
	seq_printf(seq, "debug_rx=(%d)\n",g_swacl_priv.debug_rx);
	seq_printf(seq, "debug_filter=(%d)\n",g_swacl_priv.debug_filter);
	seq_printf(seq, "debug_cmp=(%d)\n",g_swacl_priv.debug_cmp);
	seq_printf(seq, "rx_packets=(%u)\n",g_swacl_priv.rx_packets);
	seq_printf(seq, "rx_dropped=(%u)\n",g_swacl_priv.rx_dropped);
	seq_printf(seq, "rx_act=(%u)\n",g_swacl_priv.rx_act);
	seq_printf(seq, "clean_hit (entry)\n");
	return 0;
}

static int
swacl_if_proc_cmd_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
	return single_open(file, swacl_if_proc_cmd_read, PDE(inode)->data);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
	return single_open(file, swacl_if_proc_cmd_read, PDE_DATA(inode));
#else
	return single_open(file, swacl_if_proc_cmd_read, pde_data(inode));
#endif

}

static int
swacl_if_proc_entry_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
	return single_open(file, swacl_if_proc_entry_read, PDE(inode)->data);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
	return single_open(file, swacl_if_proc_entry_read, PDE_DATA(inode));
#else
	return single_open(file, swacl_if_proc_entry_read, pde_data(inode));
#endif

}
static int
swacl_if_proc_debug_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
	return single_open(file, swacl_if_proc_debug_read, PDE(inode)->data);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
	return single_open(file, swacl_if_proc_debug_read, PDE_DATA(inode));
#else
	return single_open(file, swacl_if_proc_debug_read, pde_data(inode));
#endif

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static struct proc_ops
	swacl_proc_cmd_fop = {
	.proc_open = swacl_if_proc_cmd_open,
	.proc_write = swacl_if_proc_cmd_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

#else

static struct file_operations
	swacl_proc_cmd_fop = {
	.open = swacl_if_proc_cmd_open,
	.write = swacl_if_proc_cmd_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static struct proc_ops
	swacl_proc_entry_fop = {
	.proc_open = swacl_if_proc_entry_open,
	.proc_write = swacl_if_proc_entry_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static struct file_operations
	swacl_proc_entry_fop = {
	.open = swacl_if_proc_entry_open,
	.write = swacl_if_proc_entry_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)

static struct proc_ops
	swacl_proc_debug_fop = {
	.proc_open = swacl_if_proc_debug_open,
	.proc_write = swacl_if_proc_debug_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static struct file_operations
	swacl_proc_debug_fop = {
	.open = swacl_if_proc_debug_open,
	.write = swacl_if_proc_debug_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif
// proc swacl init & cleanup //////////////////////////////////////////////////////////////////////////

struct proc_name_fop_t {
	char *name;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
	struct proc_ops *fop;
#else
	struct file_operations *fop;
#endif
	void *private_data;
};
static struct proc_name_fop_t proc_swacl_list[] = {
	{ "cmd", &swacl_proc_cmd_fop, NULL },
	{ "entry", &swacl_proc_entry_fop, NULL },
	{ "debug", &swacl_proc_debug_fop, NULL },
};

int
swacl_if_proc_init(void)
{
	int i,j;
	g_swacl_priv.swacl_pde = proc_mkdir(SWACL_PROC_NAME_ROOT, NULL);

	if (!g_swacl_priv.swacl_pde)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(proc_swacl_list); i++) {

		if(proc_create_data(proc_swacl_list[i].name, S_IWUSR | S_IRUGO,
		                    g_swacl_priv.swacl_pde,
		                    proc_swacl_list[i].fop,
		                    proc_swacl_list[i].private_data) == NULL) {

			for (j = i-1; j>=0; j--)
				remove_proc_entry(proc_swacl_list[j].name, g_swacl_priv.swacl_pde);
			remove_proc_entry(SWACL_PROC_NAME_ROOT, NULL);
			return -ENOMEM;

		}

	}

	return 0;
}

void
swacl_if_proc_cleanup(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(proc_swacl_list); i++) {

		remove_proc_entry(proc_swacl_list[i].name, g_swacl_priv.swacl_pde);

	}
	remove_proc_entry(SWACL_PROC_NAME_ROOT, NULL);
}

static void
swacl_if_cleanup(void)
{

	RCU_INIT_POINTER(swacl_net_recv, NULL);
	swacl_if_del_rule_entry_all();
	swacl_if_proc_cleanup();
	mutex_destroy(&g_swacl_priv.swacl_mutex);
	misc_deregister(&g_swacl_priv.swacl_dev);
	printk("unload %s module\n", SWACL_IF_NAME);
}

static int __init
swacl_if_init(void)
{

	memset(&g_swacl_priv,0x0,sizeof(struct swacl_if_priv));

	if(swacl_if_probe_one()!=0) {
		return -ENODEV;
	}
	INIT_LIST_HEAD(&g_swacl_priv.swacl_list);
	mutex_init(&g_swacl_priv.swacl_mutex);
	swacl_if_proc_init();

	BUG_ON(swacl_net_recv);
	RCU_INIT_POINTER(swacl_net_recv, swacl_if_handle_frame);

	printk("load %s module\n", SWACL_IF_NAME);
	return 0;

}

module_init(swacl_if_init);
module_exit(swacl_if_cleanup);

MODULE_AUTHOR("TREX Software");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("swacl ifnet driver");

