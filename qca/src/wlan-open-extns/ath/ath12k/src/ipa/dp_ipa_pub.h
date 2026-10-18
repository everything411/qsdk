/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../core.h"
#include "../../dp_peer.h"

#ifdef CPTCFG_EXT_IPA_OFFLOAD
void ath12k_dp_ipa_peer_notify(struct ath12k *ar,
			       struct ath12k_dp_link_peer *peer,
			       struct ath12k_link_vif *arvif,
			       u32 vdev_id, bool connect);

void ath12k_dp_ipa_vif_notify(struct ath12k_link_vif *arvif,
			      bool restart, bool connect);

int ath12k_dp_ipa_hw_group_init(struct ath12k_base *ab,
				struct ath12k_dp_hw_group *dp_hw_grp);

void ath12k_dp_ipa_hw_group_deinit(struct ath12k_dp_hw_group *dp_hw_grp);


u32 ath12k_dp_ipa_arch_op_rx_default_routing(struct ath12k_base *ab,
					     u32 reo_dest);

#else
static inline void
ath12k_dp_ipa_peer_notify(struct ath12k *ar,
			  struct ath12k_dp_link_peer *peer,
			  struct ath12k_link_vif *arvif,
			  u32 vdev_id, bool connect)
{
}

static inline void
ath12k_dp_ipa_vif_notify(struct ath12k_link_vif *arvif,
			 bool restart, bool connect)
{
}

static inline int
ath12k_dp_ipa_hw_group_init(struct ath12k_base *ab,
			    struct ath12k_dp_hw_group *dp_hw_grp)
{
	return 0;
}

static inline void
ath12k_dp_ipa_hw_group_deinit(struct ath12k_dp_hw_group *dp_hw_grp)
{
}
static inline u32
ath12k_dp_ipa_arch_op_rx_default_routing(struct ath12k_base *ab, u32 reo_dest)
{
	return reo_dest;
}

static inline int
ath12k_wifi8_dp_ipa_init(struct ath12k_base *ab)
{
	return 0;
}

static inline int
ath12k_wifi8_dp_ipa_xmit(struct ath12k *ar, struct sk_buff *skb,
			  const struct ath12k_skb_cb *skb_cb,
			  struct ath12k_vif *ahvif,
			  struct ath12k_sta *ahsta,
			  struct ath12k_link_vif *arvif,
			  bool is_mcast)
{
	return -EOPNOTSUPP;
}

static inline int
ath12k_wifi7_dp_ipa_init(struct ath12k_base *ab)
{
	return 0;
}
#endif /* CPTCFG_EXT_IPA_OFFLOAD */
