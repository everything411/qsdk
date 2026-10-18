/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_ME_SNOOP_EXTN_H
#define __ATH12K_ME_SNOOP_EXTN_H

#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/netfilter_bridge.h>

#include <me.h>
#include <dp.h>
#include <core.h>

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)

#ifndef CPTCFG_QCN_EXTN

void ath12k_me_snoop_list_init_extn(struct ath12k_me_snoop_list *list)
{
}

void ath12k_me_snoop_list_flush_extn(struct ath12k_me_snoop_list *list)
{
}

struct ath12k_me_peer_grp *ath12k_me_snoop_grp_find(struct ath12k_dp_vif *dp_vif,
						    struct sk_buff *skb)
{
	return NULL;
}

bool ath12k_me_snoop_filter_src(struct ath12k_me_peer *peer, union nf_inet_addr *addr)
{
	return false;
}

int ath12k_dp_me_tx_ucast_grp_extn(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
				   struct ath12k_dp_link_vif *dp_link_vif,
				   struct ath12k_dp_peer *peer,
				   void *app_data,
				   struct ath12k_dp_tx_msdu_info *msdu_info)
{
	return 0;
}

int ath12k_me_snoop_event_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	return 0;
}

int ath12k_core_me_notifier_register_extn(struct ath12k_base *ab)
{
	return 0;
}

int ath12k_core_me_notifier_unregister_extn(struct ath12k_base *ab)
{
	return 0;
}

int ath12k_me_snoop_print_extn(struct ath12k_base *ab, struct ath12k_me_db *db)
{
	return -1;
}

#else
/*
 * Initialize / De-initialize the DB for the hash list in ath12k_vif
 *
 * Note: The num_entries indicates the hash slots in the DB
 */
void ath12k_me_snoop_list_init_extn(struct ath12k_me_snoop_list *list);
void ath12k_me_snoop_list_flush_extn(struct ath12k_me_snoop_list *list);

struct ath12k_me_peer_grp *ath12k_me_snoop_grp_find(struct ath12k_dp_vif *dp_vif,
						    struct sk_buff *skb);

bool ath12k_me_snoop_filter_src(struct ath12k_me_peer *peer, union nf_inet_addr *addr);

int ath12k_dp_me_tx_ucast_grp_extn(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
				   struct ath12k_dp_link_vif *dp_link_vif,
				   struct ath12k_dp_peer *peer,
				   void *app_data,
				   struct ath12k_dp_tx_msdu_info *msdu_info);

int ath12k_me_snoop_print_extn(struct ath12k_base *ab, struct ath12k_me_db *db);

int ath12k_me_snoop_event_notify(struct notifier_block *nb, unsigned long action, void *data);

int  ath12k_core_me_notifier_register_extn(struct ath12k_base *ab);
int  ath12k_core_me_notifier_unregister_extn(struct ath12k_base *ab);

#endif /* CPTCFG_QCN_EXTN */
#endif /* CONFIG_BRIDGE_MCAST_OFFLOAD */
#endif /* __ATH12K_ME_SNOOP_EXTN_H */
