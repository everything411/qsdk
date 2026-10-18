/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include "../core.h"
#include "../mac.h"
#include "../cmn_defs.h"
#include "../accel_cfg.h"
#include "vendor.h"

void ath12k_sdwf_fill_msduq_event_data(struct ath12k_base *ab,
				       struct ath12k_dp_link_peer *link_peer,
				       u16 msduq, u16 qos_id,
				       struct ath12k_sdwf_msduq_evt_data *evt_dt)
{
	u8 link_id = ATH12k_MLO_LINK_ID_INVALID;
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_qos_params *params = NULL;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	u16 peer_id;

	if (!link_peer || !evt_dt || msduq == QOS_INVALID_MSDUQ || !ab->dp)
		return;

	evt_dt->hw_link_id = link_peer->hw_link_id;
	ether_addr_copy(evt_dt->link_mac, link_peer->addr);
	evt_dt->mlo = link_peer->mlo;
	if (link_peer->mlo)
		ether_addr_copy(evt_dt->mld_mac, link_peer->ml_addr);
	evt_dt->msduq_id = msduq - MSDUQ_MAX_DEF;

	qos_ctx = ath12k_get_qos(ab);
	if (qos_ctx && qos_id < QOS_PROFILES_MAX) {
		spin_lock_bh(&qos_ctx->profile_lock);
		params = &qos_ctx->profiles[qos_id].params;
		evt_dt->svc_id = qos_ctx->profiles[qos_id].svc_id;
		evt_dt->svc_type = (qos_id < QOS_UL_ID_MIN) ?
			QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_SVC_TYPE_DL :
			QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_SVC_TYPE_UL;
		evt_dt->priority = params->priority;
		evt_dt->service_interval = params->min_service_interval;
		evt_dt->burst_size = params->burst_size;
		evt_dt->delay_bound = params->delay_bound;
		evt_dt->min_throughput = params->min_data_rate;
		evt_dt->tid = params->tid;
		spin_unlock_bh(&qos_ctx->profile_lock);
	}

	evt_dt->ac = ath12k_tid_to_ac(evt_dt->tid);

	rcu_read_lock();
	sta = ath12k_dp_peer_get_sta(link_peer->dp_peer);
	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		link_id = (!sta->mlo) ? ahsta->deflink.link_id :
					ahsta->primary_link_id;
	}
	rcu_read_unlock();

	peer_id = ab->dp->global_peer_id_supported ?
			(u16)link_peer->dp_peer->peer_id :
			(u16)link_peer->peer_id;

	evt_dt->mark_metadata = ath_encode_metadata(link_id,
			FIELD_PREP(SDWF_PEER_ID, peer_id) |
			FIELD_PREP(SDWF_MSDUQ_ID, msduq));

	evt_dt->peer_id = link_peer->peer_id;
}
