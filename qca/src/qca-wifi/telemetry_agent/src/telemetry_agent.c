/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "../inc/telemetry_agent.h"
#include "../inc/telemetry_agent_sawf.h"
#include "../inc/telemetry_agent_wifi_driver_if.h"

#define TELEMETRY_MAC_ADDR_SIZE 6

struct telemetry_agent_object g_agent_obj;
struct telemetry_rm_main_buffer *main_stats_buffer;
struct telemetry_pmlo_buffer *pmlo_stats_buffer;
struct telemetry_emesh_buffer *emesh_stats_buffer;
struct telemetry_deter_buffer *deter_stats_buffer;
struct telemetry_erp_buffer *erp_stats_buffer;
struct telemetry_admctrl_buffer *admctrl_stats_buffer;
extern struct telemetry_agent_ops *g_agent_ops;
struct telemetry_energysvc_buffer *energysvc_stats_buffer;

/**
 *   print_mac_addr: prints the mac address.
 *   @mac: pointer to the mac address
 *
 *   return pointer to string
 */
static char *print_mac_addr(const uint8_t *mac)
{
	static char buf[32] = {'\0', };
	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return buf;
}

void telemetry_print_mac_addr(const uint8_t *mac)
{
	ta_print_debug("Agent > %s pdev_mac: %02x:%02x:%02x:%02x:%02x:%02x",
			__func__, mac[0], mac[1], mac[2], mac[3], mac[4],
			mac[5]);
}

static void telemetry_agent_trigger_rm_init_stats(bool dynamic_init,
				struct rchan *rfs_chan) {
	int i, j, k, idx, tid;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	int num_socs = 0;
	int num_pdevs = 0;
	int num_peers = 0, num_mlo_peers = 0;
	struct agent_psoc_iface_init_obj soc_stats = {0};
	struct agent_pdev_iface_init_obj pdev_stats = {0};
	struct agent_peer_iface_init_obj peer_stats = {0};
	struct agent_msduq_info_iface_obj *msduq_info = NULL;

	struct agent_soc_init_stats *rfs_soc_stats = NULL;
	struct agent_link_init_stats *rfs_link_stats = NULL;
	struct agent_peer_init_stats *rfs_peer_stats = NULL;
	struct agent_msduq_info *rfs_msduq_info = NULL;
	struct list_head *node;

	main_stats_buffer = kzalloc(sizeof(struct telemetry_rm_main_buffer), GFP_ATOMIC);
	if (!main_stats_buffer){
		ta_print_error("Allocation for stats buffer failed");
		return;
	}

	ta_print_debug("%s: Sending init stats\n", __func__);
	memset(main_stats_buffer, 0, sizeof(struct telemetry_rm_main_buffer));
	main_stats_buffer->header.start_magic_num = 0xDEADBEAF;
	main_stats_buffer->header.stats_version = 1;
	main_stats_buffer->header.stats_type =
			(dynamic_init) ? RFS_DYNAMIC_INIT_DATA : RFS_INIT_DATA;
	main_stats_buffer->header.payload_len = sizeof(struct telemetry_rm_main_buffer);

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (i = 0; i < MAX_SOCS_DB; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		/* call wifi driver and prepare RelayFS Message */
		if (g_agent_ops->agent_get_psoc_info(psoc_db->psoc_obj_ptr, &soc_stats))
			continue;
		rfs_soc_stats = &main_stats_buffer->init_stats.soc_stats[num_socs];
		rfs_soc_stats->soc_id = soc_stats.soc_id;
		rfs_soc_stats->num_peers = soc_stats.num_peers;

		num_pdevs = 0;

		for (j = 0; j < MAX_PDEV_LINKS_DB; j++) {
			pdev_db  = &psoc_db->pdev_db[j];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
			if (g_agent_ops->agent_get_pdev_info(pdev_db->pdev_obj_ptr, &pdev_stats))
				continue;
			rfs_link_stats->hw_link_id = pdev_stats.link_id;
			num_peers = pdev_db->num_peers;
			num_mlo_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
				if ((peer_db) && (peer_db->peer_obj_ptr) &&
				    !(g_agent_ops->agent_get_peer_info(peer_db->peer_obj_ptr, &peer_stats))) {
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_mlo_peers];
					memcpy(&rfs_peer_stats->mld_mac_addr[0],
							peer_stats.peer_mld_mac, 6);
					rfs_peer_stats->ifindex = peer_stats.ifindex;
					memcpy(&rfs_peer_stats->link_mac_addr[0],
							peer_stats.peer_link_mac, 6);
                    /* T2LM Info */
					for(idx = 0; idx < MAX_T2LM_INFO; idx++) {
						rfs_peer_stats->t2lm_info[idx].direction =
							peer_stats.t2lm_info[idx].direction;
						rfs_peer_stats->t2lm_info[idx].default_link_mapping =
							peer_stats.t2lm_info[idx].default_link_mapping;

						for (tid = 0; tid < NUM_TIDS; tid++) {
							rfs_peer_stats->t2lm_info[idx].tid_present[tid] =
								peer_stats.t2lm_info[idx].t2lm_provisioned_links[tid];
						}
					}

					rfs_peer_stats->is_assoc_link = peer_stats.is_assoc_link;
					rfs_peer_stats->chan_bw = peer_stats.bw;
					rfs_peer_stats->chan_freq = peer_stats.freq;

					memcpy(&rfs_peer_stats->tx_mcs_nss_map,
					       &peer_stats.caps.tx_mcs_nss_map,
					       (sizeof(uint32_t) * WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX));

					memcpy(&rfs_peer_stats->rx_mcs_nss_map,
					       &peer_stats.caps.rx_mcs_nss_map,
					       (sizeof(uint32_t) * WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX));
					rfs_peer_stats->ieee_link_id = peer_stats.ieee_link_id;
					rfs_peer_stats->vdev_id = peer_stats.vdev_id;
					memcpy(&rfs_peer_stats->ap_mld_addr[0],
						peer_stats.ap_mld_addr, 6);
					rfs_peer_stats->disabled_link_bitmap = peer_stats.disabled_link_bitmap;
					rfs_peer_stats->peer_flags = peer_stats.peer_flags;
					rfs_peer_stats->phymode = peer_stats.phymode;

					for (k = 0; k < SAWF_MAX_QUEUES; k++) {
						rfs_msduq_info = &rfs_peer_stats->msduq[k];
						msduq_info = &peer_stats.msduq_info[k];

						rfs_msduq_info->is_used = msduq_info->is_used;
						rfs_msduq_info->svc_id = msduq_info->svc_id;
						rfs_msduq_info->svc_type = msduq_info->svc_type;
						rfs_msduq_info->svc_tid = msduq_info->svc_tid;
						rfs_msduq_info->svc_ac = msduq_info->svc_ac;
						rfs_msduq_info->priority = msduq_info->priority;
						rfs_msduq_info->service_interval = msduq_info->service_interval;
						rfs_msduq_info->burst_size = msduq_info->burst_size;
						rfs_msduq_info->min_throughput = msduq_info->min_throughput;
						rfs_msduq_info->delay_bound = msduq_info->delay_bound;
						rfs_msduq_info->mark_metadata = msduq_info->mark_metadata;
					}
					num_mlo_peers++;
				}

				if (num_mlo_peers >= MAX_PEERS)
					break;

			} /* peer */

			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_mlo_peers;
			num_pdevs++;
		} /* pdev */
		rfs_soc_stats->num_links = num_pdevs;
		num_socs++;
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

	main_stats_buffer->init_stats.num_soc = num_socs;
	main_stats_buffer->end_magic_num = 0xBEAFDEAD;
	if (rfs_chan != NULL) {
		ta_print_debug("%s: Writing init stats to RFS: %s\n", __func__, rfs_chan->base_filename);
		relay_write(rfs_chan, main_stats_buffer,
			sizeof(struct telemetry_rm_main_buffer));
		relay_flush(rfs_chan);
	} else {
		relay_write(g_agent_obj.rm_telemetry.rfs_channel_main, main_stats_buffer,
			sizeof(struct telemetry_rm_main_buffer));
		relay_flush(g_agent_obj.rm_telemetry.rfs_channel_main);
	}
	kfree(main_stats_buffer);
}

void telemetry_deter_stats_work_periodic(struct work_struct *work)
{
	int i = 0, j = 0;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	int num_pdevs = 0;
	struct deter_link_iface_stats_obj pdev_stats = {0};
	/* int num_peers = 0;
	struct agent_peer_db *peer_db = NULL;
	struct deter_peer_iface_stats_obj *peer_stats = NULL;
	struct deter_peer_stats *rfs_peer_stats = NULL; */

	struct deter_soc_stats *rfs_soc_stats = NULL;
	struct deter_link_stats *rfs_link_stats = NULL;
	// struct list_head *node;

	if (!g_agent_obj.rm_telemetry.init_stats_pending[RM_DETSCHED_SERVICE]) {
		telemetry_agent_trigger_rm_init_stats(false,
			g_agent_obj.rm_telemetry.rfs_channel_deter);
		g_agent_obj.rm_telemetry.init_stats_pending[RM_DETSCHED_SERVICE] = true;
		goto restart_wq;
	}

	spin_lock_bh(&g_agent_obj.agent_lock);
	ta_print_debug("Agent > %s Num Socs: %d",__func__,
			g_agent_obj.agent_db.num_socs);

	for (i = 0; i < MAX_SOCS_DB; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		memset(deter_stats_buffer, 0, sizeof(struct telemetry_deter_buffer));
		deter_stats_buffer->header.start_magic_num = 0xFEEDEEEE;
		deter_stats_buffer->header.stats_version = 1;
		deter_stats_buffer->header.stats_type = RFS_STATS_DATA;
		deter_stats_buffer->header.payload_len = sizeof(struct telemetry_deter_buffer);
		deter_stats_buffer->relayfs_stats.num_soc = 1;

		ta_print_debug("Agent > %s len: %d\n", __func__,
				deter_stats_buffer->header.payload_len);

		deter_stats_buffer->relayfs_stats.soc_stats.soc_id = psoc_db->soc_id;

		rfs_soc_stats = &deter_stats_buffer->relayfs_stats.soc_stats;

		num_pdevs = 0;

		for (j = 0; j < MAX_PDEV_LINKS_DB; j++) {
			pdev_db = &psoc_db->pdev_db[j];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
			g_agent_ops->agent_get_deter_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);

			memcpy(&rfs_link_stats->link_mac[0],
			       pdev_stats.link_mac,6);
			rfs_link_stats->hw_link_id = pdev_stats.hw_link_id;
			rfs_link_stats->num_peers = pdev_db->num_peers;
			ta_print_debug("Agent > %s: Hw Link ID: %d",__func__,pdev_stats.hw_link_id);
			ta_print_debug("Agent > pdev_mac: %s\n",print_mac_addr(&pdev_stats.link_mac[0]));
			memcpy(rfs_link_stats->dl_ofdma_usr,
				pdev_stats.dl_ofdma_usr,
				sizeof(rfs_link_stats->dl_ofdma_usr));
			memcpy(rfs_link_stats->ul_ofdma_usr,
				pdev_stats.ul_ofdma_usr,
				sizeof(rfs_link_stats->ul_ofdma_usr));
			memcpy(rfs_link_stats->dl_mimo_usr,
				pdev_stats.dl_mimo_usr,
				sizeof(rfs_link_stats->dl_mimo_usr));
			memcpy(rfs_link_stats->ul_mimo_usr,
				pdev_stats.ul_mimo_usr,
				sizeof(rfs_link_stats->ul_mimo_usr));

			memcpy(rfs_link_stats->dl_mode_cnt,
				pdev_stats.dl_mode_cnt,
				sizeof(rfs_link_stats->dl_mode_cnt));
			memcpy(rfs_link_stats->ul_mode_cnt,
				pdev_stats.ul_mode_cnt,
				sizeof(rfs_link_stats->ul_mode_cnt));

			memcpy(rfs_link_stats->ch_access_delay,
				pdev_stats.ch_access_delay,
				sizeof(rfs_link_stats->ch_access_delay));

			memcpy(rfs_link_stats->ts,
				pdev_stats.ts,
				sizeof(rfs_link_stats->ts));

			rfs_link_stats->ch_util.tx_util = pdev_stats.ch_util.tx_util;
			rfs_link_stats->ch_util.rx_util = pdev_stats.ch_util.rx_util;
			rfs_link_stats->ch_util.chan_util = pdev_stats.ch_util.chan_util;
			rfs_link_stats->rx_su_cnt = pdev_stats.rx_su_cnt;

			/*
			num_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
				peer_stats = &pdev_db->peer_stats;
				if (!peer_stats) {
					ta_print_error("Agent > %s\n: error allocating peer_stats",__func__);
				};
				memset(peer_stats, 0, sizeof(struct deter_peer_iface_stats_obj));
				if ((peer_db) && !g_agent_ops->agent_get_deter_peer_stats(peer_db->peer_obj_ptr, peer_stats))
				{
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_peers++];
					memcpy(&rfs_peer_stats->peer_link_mac[0],
					peer_stats->peer_link_mac, 6);
					memcpy(&rfs_peer_stats->deter,
					&peer_stats->deter,sizeof(rfs_peer_stats->deter));
					rfs_peer_stats->vdev_id = peer_stats->vdev_id;
				}
			}

			spin_unlock_bh(&pdev_db->peer_db_lock); */
			num_pdevs++;
		}/* pdev */
		rfs_soc_stats->num_links = num_pdevs;

		deter_stats_buffer->end_magic_num = 0xFEEDEEEE ^ i;
		relay_write(g_agent_obj.rm_telemetry.rfs_channel_deter, deter_stats_buffer,
					sizeof(struct telemetry_deter_buffer));
		relay_flush(g_agent_obj.rm_telemetry.rfs_channel_deter);
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

restart_wq:
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_deter, msecs_to_jiffies(STATS_FREQUENCY));
}

void telemetry_emesh_stats_work_periodic(struct work_struct *work)
{
	int i, j;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	int num_socs = 0;
	int num_pdevs = 0;
	int num_peers = 0;
	struct emesh_link_iface_stats_obj pdev_stats = {0};
	struct emesh_peer_iface_stats_obj peer_stats = {0};

	struct emesh_soc_stats *rfs_soc_stats = NULL;
	struct emesh_link_stats *rfs_link_stats = NULL;
	struct emesh_peer_stats *rfs_peer_stats = NULL;
	struct list_head *node;

	memset(emesh_stats_buffer, 0, sizeof(struct telemetry_emesh_buffer));
	emesh_stats_buffer->header.start_magic_num = 0xDEADEEEE;
	emesh_stats_buffer->header.stats_version = 1;
	emesh_stats_buffer->header.stats_type = RFS_STATS_DATA;
	emesh_stats_buffer->header.payload_len = sizeof(struct telemetry_emesh_buffer);

	ta_print_debug("Agent > %s len: %d\n", __func__,
			emesh_stats_buffer->header.payload_len);

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (i = 0; i < MAX_SOCS_DB; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		rfs_soc_stats = &emesh_stats_buffer->relayfs_stats.soc_stats[num_socs];
		num_pdevs = 0;

		for (j = 0; j < MAX_PDEV_LINKS_DB; j++) {
			pdev_db = &psoc_db->pdev_db[j];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
			g_agent_ops->agent_get_emesh_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);
			rfs_link_stats->link_idle_airtime = pdev_stats.link_idle_airtime;
			memcpy(&rfs_link_stats->link_mac[0],
			       pdev_stats.link_mac,6);
			num_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
				if ((peer_db) && (peer_db->peer_obj_ptr) &&
				    !(g_agent_ops->agent_get_emesh_peer_stats(peer_db->peer_obj_ptr, &peer_stats)))
				{
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_peers];
					memcpy(&rfs_peer_stats->peer_link_mac[0],
				    	peer_stats.peer_link_mac, 6);
					memcpy(rfs_peer_stats->tx_airtime_consumption,
					       peer_stats.tx_airtime_consumption,
					       sizeof(rfs_peer_stats->tx_airtime_consumption));
					num_peers++;
				}
			} /* peer */
			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_peers;
			num_pdevs++;
		} /* pdev */
		rfs_soc_stats->num_links = num_pdevs;
		num_socs++;
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

	emesh_stats_buffer->relayfs_stats.num_soc = num_socs;
	emesh_stats_buffer->end_magic_num = 0xEEEEDEAD;
	relay_write(g_agent_obj.rfs_emesh_channel, emesh_stats_buffer,
			sizeof(struct telemetry_emesh_buffer));
	relay_flush(g_agent_obj.rfs_emesh_channel);
	schedule_delayed_work(&g_agent_obj.emesh_stats_work_periodic, msecs_to_jiffies(STATS_FREQUENCY));
}

void telemetry_agent_stats_work_periodic_erp(struct work_struct *work)
{
	int i,j;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	int num_socs = 0;
	int num_pdevs = 0;
	struct erp_link_iface_stats_obj pdev_stats = {0};
	struct erp_soc_stats *rfs_soc_stats = NULL;
	struct erp_link_stats *rfs_link_stats = NULL;

	memset(erp_stats_buffer, 0, sizeof(struct telemetry_erp_buffer));
	erp_stats_buffer->header.start_magic_num = 0xDEADBEAF;
	erp_stats_buffer->header.stats_version = 1;
	erp_stats_buffer->header.stats_type = RFS_STATS_DATA;
	erp_stats_buffer->header.payload_len = sizeof(struct telemetry_erp_buffer);

	ta_print_debug("Agent > %s len: %d\n", __func__,
			erp_stats_buffer->header.payload_len);

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (i = 0; i < MAX_SOCS_DB; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		rfs_soc_stats = &erp_stats_buffer->relayfs_stats.soc_stats[num_socs];
		num_pdevs = 0;

		for (j = 0; j < MAX_PDEV_LINKS_DB; j++) {
			pdev_db = &psoc_db->pdev_db[j];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
			g_agent_ops->agent_get_erp_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);
			rfs_link_stats->tx_data_msdu_cnt = pdev_stats.tx_data_msdu_cnt;
			rfs_link_stats->rx_data_msdu_cnt = pdev_stats.rx_data_msdu_cnt;
			rfs_link_stats->total_tx_data_bytes = pdev_stats.total_tx_data_bytes;
			rfs_link_stats->total_rx_data_bytes = pdev_stats.total_rx_data_bytes;
			rfs_link_stats->sta_vap_exist = pdev_stats.sta_vap_exist;
			rfs_link_stats->time_since_last_assoc = pdev_stats.time_since_last_assoc;
#endif
			num_pdevs++;
		} /* pdev */
		rfs_soc_stats->num_links = num_pdevs;
		num_socs++;
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

	erp_stats_buffer->relayfs_stats.num_soc = num_socs;
	erp_stats_buffer->end_magic_num = 0xBEAFDEAD;
	relay_write(g_agent_obj.rm_telemetry.rfs_channel_erp, erp_stats_buffer,
			sizeof(struct telemetry_erp_buffer));
	relay_flush(g_agent_obj.rm_telemetry.rfs_channel_erp);

	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_erp,
				msecs_to_jiffies(g_agent_obj.rm_telemetry.erp_wifi_sample_timer * 1000));
	return;
}

void telemetry_agent_stats_work_periodic_pmlo(struct work_struct *work)
{
	int i, j, ac;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	int num_socs = 0;
	int num_pdevs = 0;
	int num_mlo_peers = 0;
	struct agent_link_iface_stats_obj pdev_stats = {0};
	struct agent_peer_iface_stats_obj peer_stats = {0};

	struct agent_soc_stats *rfs_soc_stats = NULL;
	struct agent_link_stats *rfs_link_stats = NULL;
	struct agent_peer_stats *rfs_peer_stats = NULL;
	struct list_head *node;

	if (!g_agent_obj.rm_telemetry.init_stats_pending[RM_PMLO_SERVICE]) {
		telemetry_agent_trigger_rm_init_stats(false,
			g_agent_obj.rm_telemetry.rfs_channel_pmlo);
		g_agent_obj.rm_telemetry.init_stats_pending[RM_PMLO_SERVICE] = true;
		goto restart_wq;
	}

	memset(pmlo_stats_buffer, 0, sizeof(struct telemetry_pmlo_buffer));
	pmlo_stats_buffer->header.start_magic_num = 0xDEADBEAF;
	pmlo_stats_buffer->header.stats_version = 1;
	pmlo_stats_buffer->header.stats_type = RFS_STATS_DATA;
	pmlo_stats_buffer->header.payload_len = sizeof(struct telemetry_pmlo_buffer);

	ta_print_debug("Agent > %s len: %d\n", __func__,
			pmlo_stats_buffer->header.payload_len);

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (i = 0; i < MAX_SOCS_DB; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		if (!psoc_db->psoc_obj_ptr)
			continue;

#ifdef WLAN_CONFIG_TELEMETRY_AGENT
		if (g_agent_ops->agent_get_psoc_stats(psoc_db->psoc_obj_ptr))
			continue;
#endif
		rfs_soc_stats = &pmlo_stats_buffer->periodic_stats.soc_stats[num_socs];

		/* call wifi driver and prepare RelayFS Message */
		rfs_soc_stats->soc_id = psoc_db->soc_id;
		num_pdevs = 0;

		for (j = 0; j < MAX_PDEV_LINKS_DB; j++) {
			pdev_db = &psoc_db->pdev_db[j];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
			g_agent_ops->agent_get_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);
			rfs_link_stats->hw_link_id = pdev_stats.link_id;
			memcpy(rfs_link_stats->available_airtime,
			       pdev_stats.available_airtime,
			       sizeof(rfs_link_stats->available_airtime));
			memcpy(rfs_link_stats->link_airtime,
			       pdev_stats.link_airtime,
			       sizeof(rfs_link_stats->link_airtime));
			rfs_link_stats->freetime = pdev_stats.freetime;
			rfs_link_stats->obss_airtime = pdev_stats.obss_airtime;
			memcpy(rfs_link_stats->m3_stats, pdev_stats.congestion,
			       sizeof(rfs_link_stats->m3_stats));
			for (ac = 0; ac < WLAN_AC_MAX; ac++) {
				if (!pdev_stats.tx_mpdu_total[ac])
					rfs_link_stats->m4_stats[ac] = 0;
				else
					rfs_link_stats->m4_stats[ac] = ((pdev_stats.tx_mpdu_failed[ac] - pdev_db->tx_mpdu_failed[ac]) * 100)/
									(pdev_stats.tx_mpdu_total[ac] - pdev_db->tx_mpdu_total[ac]);
				pdev_db->tx_mpdu_failed[ac] = pdev_stats.tx_mpdu_failed[ac];
				pdev_db->tx_mpdu_total[ac] = pdev_stats.tx_mpdu_total[ac];

				/* Populate AA estimation stats */
				rfs_link_stats->aa_est.traffic_condition[ac] =
					pdev_stats.traffic_condition[ac];
				rfs_link_stats->aa_est.error_margin[ac] =
					pdev_stats.error_margin[ac];
				rfs_link_stats->aa_est.num_dl_asymmetric_clients[ac] =
					pdev_stats.num_dl_asymmetric_clients[ac];
				rfs_link_stats->aa_est.num_ul_asymmetric_clients[ac] =
					pdev_stats.num_ul_asymmetric_clients[ac];
				rfs_link_stats->aa_est.dl_payload_ratio[ac] =
					pdev_stats.dl_payload_ratio[ac];
				rfs_link_stats->aa_est.ul_payload_ratio[ac] =
					pdev_stats.ul_payload_ratio[ac];
				rfs_link_stats->aa_est.avg_chan_latency[ac] =
					pdev_stats.avg_chan_latency[ac];
			}
#endif
			num_mlo_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
				if ((peer_db) && (peer_db->peer_obj_ptr) &&
				    !(g_agent_ops->agent_get_peer_stats(peer_db, &peer_stats)))
#endif
				{
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_mlo_peers];
					memcpy(&rfs_peer_stats->peer_mld_mac[0],
					       peer_stats.peer_mld_mac, 6);
					memcpy(&rfs_peer_stats->peer_link_mac[0],
				    	peer_stats.peer_link_mac, 6);
					memcpy(rfs_peer_stats->airtime_consumption,
					       peer_stats.airtime_consumption,
					       sizeof(rfs_peer_stats->airtime_consumption));
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
					if (!peer_stats.tx_mpdu_total)
						rfs_peer_stats->m1_stats = 0;
					else
						rfs_peer_stats->m1_stats = ((peer_stats.tx_mpdu_retried - peer_db->tx_mpdu_retried) * 100)/
									    (peer_stats.tx_mpdu_total - peer_db->tx_mpdu_total);
					if (!peer_stats.rx_mpdu_total)
						rfs_peer_stats->m2_stats = 0;
					else
						rfs_peer_stats->m2_stats = ((peer_stats.rx_mpdu_retried - peer_db->rx_mpdu_retried) * 100)/
									    (peer_stats.rx_mpdu_total - peer_db->rx_mpdu_total);
#endif
					peer_db->tx_mpdu_total = peer_stats.tx_mpdu_total;
					peer_db->tx_mpdu_retried = peer_stats.tx_mpdu_retried;
					peer_db->rx_mpdu_total = peer_stats.rx_mpdu_total;
					peer_db->rx_mpdu_retried = peer_stats.rx_mpdu_retried;
					rfs_peer_stats->snr = peer_stats.rssi;
					rfs_peer_stats->sla_mask = peer_stats.sla_mask;
					rfs_peer_stats->eff_chan_bw = peer_db->eff_chan_bw;
					num_mlo_peers++;
				}

				if (num_mlo_peers >= MAX_PEERS)
					break;

			} /* peer */
			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_mlo_peers;
			num_pdevs++;
		} /* pdev */
		rfs_soc_stats->num_links = num_pdevs;
		num_socs++;
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

	pmlo_stats_buffer->periodic_stats.num_soc = num_socs;
	pmlo_stats_buffer->end_magic_num = 0xBEAFDEAD;
	relay_write(g_agent_obj.rm_telemetry.rfs_channel_pmlo, pmlo_stats_buffer,
			sizeof(struct telemetry_pmlo_buffer));
	relay_flush(g_agent_obj.rm_telemetry.rfs_channel_pmlo);

restart_wq:
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_pmlo, msecs_to_jiffies(STATS_FREQUENCY));
	return;
}

#ifdef TELEMETRY_AGENT_256M
static inline bool telemetry_agent_send_init_stats(void)
{
	return false;
}
#else /* TELEMETRY_AGENT_256M */
static inline bool telemetry_agent_send_init_stats(void)
{
	return true;
}

#endif /* TELEMETRY_AGENT_256M */

static inline void telemetry_agent_notify_host_event(enum agent_notification_event event,
						     enum rm_services service_id,
						     uint8_t category)
{
	if (g_agent_ops->agent_notify_host_event)
		g_agent_ops->agent_notify_host_event(event, service_id, category);
}


void telemetry_agent_stats_work_init_main(struct work_struct *work) {
	/*  Schedule work for ERP periodic relayfs stats if service is enabled and sample timer is non-zero,
	 *  Sample timer will be zero in case ERP service is disabled and lib alone is enabled
	 */
	if (g_agent_obj.rm_telemetry.app_init[RM_ERP_SERVICE] &&
			g_agent_obj.rm_telemetry.erp_wifi_sample_timer)
		schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_erp,
				msecs_to_jiffies(g_agent_obj.rm_telemetry.erp_wifi_sample_timer * 1000));

#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	wlan_cfg80211_t2lm_app_reply_generic_response(NULL, 1, RM_MAIN_SERVICE);
#else
	if (g_agent_obj.rm_telemetry.app_init[RM_MAIN_SERVICE])
		telemetry_agent_notify_host_event(AGENT_NOTIFY_EVENT_INIT,
						  RM_MAIN_SERVICE, 1);
	else if (g_agent_obj.rm_telemetry.app_init[RM_ENERGY_SERVICE])
		telemetry_agent_notify_host_event(AGENT_NOTIFY_EVENT_INIT,
						  RM_ENERGY_SERVICE, 1);
	else {
		telemetry_agent_notify_host_event(AGENT_NOTIFY_EVENT_DEINIT,
						  RM_MAIN_SERVICE, 1);

		telemetry_agent_notify_host_event(AGENT_NOTIFY_EVENT_DEINIT,
						  RM_ENERGY_SERVICE, 1);
	}
#endif

	if (!telemetry_agent_send_init_stats()) {
		ta_print_debug("Agent > %s Init Stats Skipped - Minimal Mode\n",
			       __func__);
		return;
	}

	/* Send Relayfs init stats to RM */
	telemetry_agent_trigger_rm_init_stats(false, NULL);

	/*  Schedule work for PMLO periodic relayfs stats if service is enabled */
	if (g_agent_obj.rm_telemetry.app_init[RM_PMLO_SERVICE])
		schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_pmlo, msecs_to_jiffies(STATS_FREQUENCY));

	/*  Schedule work for Admission Control periodic relayfs stats if service is enabled */
	if (g_agent_obj.rm_telemetry.app_init[RM_ADMCTRL_SERVICE])
		schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_admctrl, msecs_to_jiffies(STATS_FREQUENCY));

	/*  Schedule work for Deterministic scheduler periodic relayfs stats if service is enabled */
	if (g_agent_obj.rm_telemetry.app_init[RM_DETSCHED_SERVICE])
		schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_deter, msecs_to_jiffies(STATS_FREQUENCY));

	if (g_agent_obj.rm_telemetry.app_init[RM_ENERGY_SERVICE])
		schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_energysvc, msecs_to_jiffies(STATS_FREQUENCY));
}

void telemetry_agent_stats_work_dynamic_init_main(struct work_struct *work) {

	if (!telemetry_agent_send_init_stats()) {
		ta_print_debug("Agent > %s Init Stats Skipped - Minimal Mode\n",
				__func__);
		return;
	}

	/* Send Relayfs init stats to RM */
	telemetry_agent_trigger_rm_init_stats(true, NULL);
}

void telemetry_agent_stats_work_init_pmlo(struct work_struct *work)
{
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	wlan_cfg80211_t2lm_app_reply_generic_response(NULL, 1, RM_PMLO_SERVICE);
	if (g_agent_obj.rm_telemetry.is_container[RM_PMLO_SERVICE]) {
		/* Since for container case there is no RM_MAIN_SERVICE init
		 * received in driver, init stats will not be triggered.
		 * Mark init stats to be sent for container before starting
		 * periodic stats update.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_PMLO_SERVICE] = false;
	} else {
		/* Init stats for non-container case are sent on RM_MAIN_SERVICE
		 * app init received.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_PMLO_SERVICE] = true;
	}
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_pmlo, msecs_to_jiffies(STATS_FREQUENCY));
#endif
	return;
}

void telemetry_agent_deter_stats_work_init(struct work_struct *work)
{
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	wlan_cfg80211_t2lm_app_reply_generic_response(NULL, 1, RM_DETSCHED_SERVICE);
	if (g_agent_obj.rm_telemetry.is_container[RM_DETSCHED_SERVICE]) {
		/* Since for container case there is no RM_MAIN_SERVICE init
		 * received in driver, init stats will not be triggered.
		 * Mark init stats to be sent for container before starting
		 * periodic stats update.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_DETSCHED_SERVICE] = false;
	} else {
		/* Init stats for non-container case are sent on RM_MAIN_SERVICE
		 * app init received.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_DETSCHED_SERVICE] = true;
	}
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_deter, msecs_to_jiffies(STATS_FREQUENCY));
#endif
	return;
}

void telemetry_agent_stats_work_init_erp(struct work_struct *work)
{
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	wlan_cfg80211_t2lm_app_reply_generic_response(NULL, 1, RM_ERP_SERVICE);
#endif
	return;
}

void telemetry_agent_stats_work_init_admctrl(struct work_struct *work)
{
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	wlan_cfg80211_t2lm_app_reply_generic_response(NULL, 1, RM_ADMCTRL_SERVICE);
	if (g_agent_obj.rm_telemetry.is_container[RM_ADMCTRL_SERVICE]) {
		/* Since for container case there is no RM_MAIN_SERVICE init
		 * received in driver, init stats will not be triggered.
		 * Mark init stats to be sent for container before starting
		 * periodic stats update.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_ADMCTRL_SERVICE] = false;
	} else {
		/* Init stats for non-container case are sent on RM_MAIN_SERVICE
		 * app init received.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_ADMCTRL_SERVICE] = true;
	}
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_admctrl, msecs_to_jiffies(STATS_FREQUENCY));
#endif
	return;
}

void telemetry_agent_stats_work_init_energysvc(struct work_struct *work)
{
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	wlan_cfg80211_t2lm_app_reply_generic_response(NULL, 1, RM_ENERGY_SERVICE);
	if (g_agent_obj.rm_telemetry.is_container[RM_ENERGY_SERVICE]) {
		/* Since for container case there is no RM_MAIN_SERVICE init
		 * received in driver, init stats will not be triggered.
		 * Mark init stats to be sent for container before starting
		 * periodic stats update.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_ENERGY_SERVICE] = false;
	} else {
		/* Init stats for non-container case are sent on RM_MAIN_SERVICE
		 * app init received.
		 */
		g_agent_obj.rm_telemetry.init_stats_pending[RM_ENERGY_SERVICE] = true;
	}
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_energysvc, msecs_to_jiffies(STATS_FREQUENCY));
#else
	if (g_agent_obj.rm_telemetry.app_init[RM_ENERGY_SERVICE])
		schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_energysvc, msecs_to_jiffies(STATS_FREQUENCY));
	telemetry_agent_notify_host_event(AGENT_NOTIFY_EVENT_INIT,
					  RM_ENERGY_SERVICE, 1);
#endif
	return;
}

void telemetry_agent_stats_work_periodic_admctrl(struct work_struct *work)
{
	uint8_t i, j, k;
	struct list_head *node;
        int num_socs = 0, num_pdevs = 0, num_peers = 0;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	struct admctrl_link_iface_stats_obj pdev_stats = {0};
	struct admctrl_peer_iface_stats_obj peer_stats = {0};
	struct admctrl_soc_stats *rfs_soc_stats = NULL;
	struct admctrl_link_stats *rfs_link_stats = NULL;
	struct admctrl_peer_stats *rfs_peer_stats = NULL;
	struct admctrl_msduq_stats *rfs_msduq_stats = NULL;

	if (!g_agent_obj.rm_telemetry.init_stats_pending[RM_ADMCTRL_SERVICE]) {
		telemetry_agent_trigger_rm_init_stats(false,
			g_agent_obj.rm_telemetry.rfs_channel_admctrl);
		g_agent_obj.rm_telemetry.init_stats_pending[RM_ADMCTRL_SERVICE] = true;
		goto restart_wq;
	}

	memset(admctrl_stats_buffer, 0, sizeof(struct telemetry_admctrl_buffer));
	admctrl_stats_buffer->header.start_magic_num = 0xDEADABCD;
	admctrl_stats_buffer->header.stats_version = 1;
	admctrl_stats_buffer->header.stats_type = RFS_STATS_DATA;
	admctrl_stats_buffer->header.payload_len = sizeof(struct telemetry_admctrl_buffer);

	ta_print_debug("Agent > %s len: %d\n", __func__,
			admctrl_stats_buffer->header.payload_len);

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (i = 0; i < MAX_SOCS_DB; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		rfs_soc_stats = &admctrl_stats_buffer->relayfs_stats.soc_stats[num_socs];
		rfs_soc_stats->soc_id = psoc_db->soc_id;

		num_pdevs = 0;
		for (j = 0; j < MAX_PDEV_LINKS_DB; j++) {
			pdev_db = &psoc_db->pdev_db[j];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
			memset(&pdev_stats, 0, sizeof(struct admctrl_link_iface_stats_obj));
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
			g_agent_ops->agent_get_admctrl_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);
			rfs_link_stats->hw_link_id = pdev_stats.link_id;
			rfs_link_stats->freetime = pdev_stats.freetime;
			memcpy(rfs_link_stats->tx_link_airtime,
			       pdev_stats.tx_link_airtime,
			       sizeof(rfs_link_stats->tx_link_airtime));
#endif
			num_peers = 0;
			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
				memset(&peer_stats, 0,
				       sizeof(struct admctrl_peer_iface_stats_obj));
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
				if ((peer_db) && (peer_db->peer_obj_ptr) &&
				    !(g_agent_ops->agent_get_admctrl_peer_stats(peer_db->peer_obj_ptr, &peer_stats)))
#endif
				{
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_peers];
					memcpy(&rfs_peer_stats->peer_mld_mac[0],
					       peer_stats.peer_mld_mac, 6);
					memcpy(&rfs_peer_stats->peer_link_mac[0],
					       peer_stats.peer_link_mac, 6);
					rfs_peer_stats->is_assoc_link = peer_stats.is_assoc_link;
					rfs_peer_stats->tx_success_num = peer_stats.tx_success_num;
					rfs_peer_stats->mld_tx_success_num = peer_stats.mld_tx_success_num;
					rfs_peer_stats->avg_tx_rate = peer_stats.avg_tx_rate;
					memcpy(rfs_peer_stats->tx_airtime_consumption,
					       peer_stats.tx_airtime_consumption,
					       sizeof(rfs_peer_stats->tx_airtime_consumption));
					for (k = 0; k < SAWF_MAX_QUEUES; k++) {
						rfs_msduq_stats = &rfs_peer_stats->msduq_stats[k];
						rfs_msduq_stats->tx_success_num = peer_stats.msduq_stats[k].tx_success_num;
					}
					num_peers++;
				}

				if (num_peers >= MAX_PEERS)
					break;
			} /* peer */
			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_peers;
			num_pdevs++;
		} /* pdev */
		rfs_soc_stats->num_links = num_pdevs;
		num_socs++;
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

	admctrl_stats_buffer->relayfs_stats.num_soc = num_socs;
	admctrl_stats_buffer->end_magic_num = 0xABCDDEAD;
	relay_write(g_agent_obj.rm_telemetry.rfs_channel_admctrl,
		    admctrl_stats_buffer,
		    sizeof(struct telemetry_admctrl_buffer));
	relay_flush(g_agent_obj.rm_telemetry.rfs_channel_admctrl);

restart_wq:
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_admctrl,
			      msecs_to_jiffies(STATS_FREQUENCY));
}

void telemetry_agent_stats_work_periodic_energysvc(struct work_struct *work)
{
	int num_socs = 0, num_pdevs = 0, num_peers = 0;

	int psoc_idx, pdev_idx;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	struct agent_link_iface_stats_obj pdev_stats = {0};
	struct agent_peer_iface_stats_obj peer_stats = {0};
	struct emesh_peer_iface_stats_obj peer_stats_tx = {0};

	struct energysvc_soc_stats *rfs_soc_stats = NULL;
	struct energysvc_link_stats *rfs_link_stats = NULL;
	struct energysvc_peer_stats *rfs_peer_stats = NULL;
	struct list_head *node;

	if (!g_agent_obj.rm_telemetry.init_stats_pending[RM_ENERGY_SERVICE]) {
		telemetry_agent_trigger_rm_init_stats(false,
			g_agent_obj.rm_telemetry.rfs_channel_energysvc);
		g_agent_obj.rm_telemetry.init_stats_pending[RM_ENERGY_SERVICE] = true;
		goto restart_wq;
	}

	memset(energysvc_stats_buffer, 0, sizeof(struct telemetry_energysvc_buffer));
	energysvc_stats_buffer->header.start_magic_num = 0xDEADBEAF;
	energysvc_stats_buffer->header.stats_version = 1;
	energysvc_stats_buffer->header.stats_type = RFS_STATS_DATA;
	energysvc_stats_buffer->header.payload_len = sizeof(struct telemetry_energysvc_buffer);

	ta_print_debug("Agent > %s len: %d\n", __func__,
			energysvc_stats_buffer->header.payload_len);

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (psoc_idx = 0; psoc_idx < MAX_SOCS_DB; psoc_idx++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[psoc_idx];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		rfs_soc_stats = &energysvc_stats_buffer->relayfs_stats.soc_stats[num_socs];
		rfs_soc_stats->soc_id = psoc_db->soc_id;

		num_pdevs = 0;
		for (pdev_idx = 0; pdev_idx < MAX_PDEV_LINKS_DB; pdev_idx++) {
			pdev_db = &psoc_db->pdev_db[pdev_idx];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			rfs_link_stats = &rfs_soc_stats->link_stats[num_pdevs];
			if (g_agent_ops->agent_get_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats))
				continue;
			rfs_link_stats->hw_link_id = pdev_stats.link_id;
			memcpy(rfs_link_stats->available_airtime,
				pdev_stats.available_airtime,
				sizeof(rfs_link_stats->available_airtime));
			memcpy(rfs_link_stats->link_airtime,
				pdev_stats.link_airtime,
				sizeof(rfs_link_stats->link_airtime));
			rfs_link_stats->freetime = pdev_stats.freetime;
			rfs_link_stats->freq = pdev_stats.freq;

			rfs_link_stats->is_mon_enabled = pdev_stats.is_mon_enabled;

			num_peers = 0;
			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
				memset(&peer_stats, 0, sizeof(struct agent_peer_iface_stats_obj));
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
				if ((peer_db) && (peer_db->peer_obj_ptr) &&
				    !(g_agent_ops->agent_get_peer_stats(peer_db, &peer_stats)) &&
				    !(g_agent_ops->agent_get_emesh_peer_stats(peer_db->peer_obj_ptr, &peer_stats_tx)))
#else
				if ((peer_db) && (peer_db->peer_obj_ptr) &&
				    !(g_agent_ops->agent_get_peer_stats(peer_db, &peer_stats)))
#endif
				{
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_peers];
					memcpy(&rfs_peer_stats->peer_mld_mac[0], peer_stats.peer_mld_mac, 6);
					memcpy(&rfs_peer_stats->peer_link_mac[0], peer_stats.peer_link_mac, 6);
					memcpy(rfs_peer_stats->airtime_consumption,
						peer_stats.airtime_consumption,
						sizeof(rfs_peer_stats->airtime_consumption));
					memcpy(rfs_peer_stats->tx_airtime_consumption,
						peer_stats.tx_airtime_consumption,
						sizeof(rfs_peer_stats->tx_airtime_consumption));
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
					memcpy(rfs_peer_stats->tx_airtime_consumption,
						peer_stats_tx.tx_airtime_consumption,
						sizeof(rfs_peer_stats->tx_airtime_consumption));
#endif
					rfs_peer_stats->sla_mask = peer_stats.sla_mask;
#ifndef WLAN_CONFIG_TELEMETRY_AGENT
					rfs_peer_stats->snr = peer_stats.rssi;
#endif
					num_peers++;
				}

				if (num_peers >= MAX_PEERS)
					break;
			} /* peer */
			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_peers;
			num_pdevs++;
		} /* pdev */
		rfs_soc_stats->num_links = num_pdevs;
		num_socs++;
	} /* soc */
	spin_unlock_bh(&g_agent_obj.agent_lock);

	energysvc_stats_buffer->relayfs_stats.num_soc = num_socs;
	energysvc_stats_buffer->end_magic_num = 0xBEAFDEAD;
	relay_write(g_agent_obj.rm_telemetry.rfs_channel_energysvc, energysvc_stats_buffer,
			sizeof(struct telemetry_energysvc_buffer));
	relay_flush(g_agent_obj.rm_telemetry.rfs_channel_energysvc);

restart_wq:
	schedule_delayed_work(&g_agent_obj.rm_telemetry.stats_work_periodic_energysvc, msecs_to_jiffies(STATS_FREQUENCY));
	return;
}


#ifdef WLAN_CONFIG_TELEMETRY_AGENT
static void telemetry_agent_rm_get_delayed_work_info(struct rchan **rfs_channel,
						struct delayed_work **work_init,
						struct delayed_work **work_periodic,
						struct delayed_work **work_dynamic_init,
						uint8_t service_id,
						bool is_container_app)
{
	*work_dynamic_init =
		&g_agent_obj.rm_telemetry.stats_work_dynamic_init_main;
	switch(service_id) {
		case RM_MAIN_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_main;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_main;
			*work_periodic = NULL;
			break;
		}
		case RM_PMLO_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_pmlo;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_pmlo;
			g_agent_obj.rm_telemetry.is_container[RM_PMLO_SERVICE] =
				is_container_app;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_pmlo;
			break;
		}
		case RM_DETSCHED_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_deter;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_deter;
			g_agent_obj.rm_telemetry.is_container[RM_DETSCHED_SERVICE] =
				is_container_app;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_deter;
			break;
		}
		case RM_ERP_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_erp;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_erp;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_erp;
			break;
		}
		case RM_ADMCTRL_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_admctrl;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_admctrl;
			g_agent_obj.rm_telemetry.is_container[RM_ADMCTRL_SERVICE] =
				is_container_app;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_admctrl;
			break;
		}
		case RM_ENERGY_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_energysvc;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_energysvc;
			g_agent_obj.rm_telemetry.is_container[RM_ENERGY_SERVICE] =
				is_container_app;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_energysvc;
			break;
		}
		default:
		{
			*rfs_channel = NULL;
			*work_init = NULL;
			*work_periodic = NULL;
		}
	}
	return;
}
#else
static void telemetry_agent_rm_get_delayed_work_info(struct rchan **rfs_channel,
						struct delayed_work **work_init,
						struct delayed_work **work_periodic,
						struct delayed_work **work_dynamic_init,
						uint8_t service_id)
{
	*work_dynamic_init =
		&g_agent_obj.rm_telemetry.stats_work_dynamic_init_main;
	switch(service_id) {
		case RM_MAIN_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_main;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_main;
			*work_periodic = NULL;
			break;
		}
		case RM_PMLO_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_pmlo;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_pmlo;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_pmlo;
			break;
		}
		case RM_DETSCHED_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_deter;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_deter;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_deter;
			break;
		}
		case RM_ERP_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_erp;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_erp;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_erp;
			break;
		}
		case RM_ADMCTRL_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_admctrl;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_admctrl;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_admctrl;
			break;
		}
		case RM_ENERGY_SERVICE:
		{
			*rfs_channel = g_agent_obj.rm_telemetry.rfs_channel_energysvc;
			*work_init = &g_agent_obj.rm_telemetry.stats_work_init_energysvc;
			*work_periodic = &g_agent_obj.rm_telemetry.stats_work_periodic_energysvc;
			break;
		}
		default:
		{
			*rfs_channel = NULL;
			*work_init = NULL;
			*work_periodic = NULL;
		}
	}
	return;
}
#endif /* WLAN_CONFIG_TELEMETRY_AGENT */

static void telemetry_agent_rm_get_service_data(enum rm_services service_id, uint64_t service_data)
{
	if (service_id == RM_ERP_SERVICE)
		g_agent_obj.rm_telemetry.erp_wifi_sample_timer = service_data;
}

void telemetry_agent_notify_app_init(enum agent_notification_event event, enum rm_services service_id,
				     uint64_t service_data)
{
	struct rchan *rfs_channel;
	struct delayed_work *work_init;
	struct delayed_work *work_periodic;
	struct delayed_work *work_dynamic_init;
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	uint8_t id;
#endif

	ta_print_debug("Agent> %s event: %d for service %d \n", __func__, event, service_id);
	switch(event) {
		case AGENT_NOTIFY_EVENT_INIT:
			telemetry_agent_rm_get_service_data(service_id, service_data);
			g_agent_obj.rm_telemetry.app_init[service_id] = 1;
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel, &work_init,
					&work_periodic, &work_dynamic_init, service_id, false);
#else
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel, &work_init,
					&work_periodic, &work_dynamic_init, service_id);
#endif /* WLAN_CONFIG_TELEMETRY_AGENT */
			if (work_init)
				schedule_delayed_work(work_init, msecs_to_jiffies(1000));
			break;
		case AGENT_NOTIFY_EVENT_DEINIT:
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
			/*Deinit is explicitly called for RM_MAIN_SERVICE.
			 *Deinit all the initialized services too*/
			for (id = 0; id < RM_MAX_SERVICE; id++) {
				telemetry_agent_rm_get_delayed_work_info(&rfs_channel,
									 &work_init,
									 &work_periodic,
									 &work_dynamic_init,
									 id, false);
				g_agent_obj.rm_telemetry.app_init[id] = 0;
				if (rfs_channel)
					relay_reset(rfs_channel);
				if (work_init)
					cancel_delayed_work_sync(work_init);
				if (work_dynamic_init)
					cancel_delayed_work_sync(work_dynamic_init);
				if (work_periodic)
					cancel_delayed_work_sync(work_periodic);
			}
#else
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel,
								 &work_init,
								 &work_periodic,
								 &work_dynamic_init,
								 service_id);
			g_agent_obj.rm_telemetry.app_init[service_id] = 0;
			if (rfs_channel)
				relay_reset(rfs_channel);
			if (work_init)
				cancel_delayed_work_sync(work_init);
			if (work_dynamic_init)
				cancel_delayed_work_sync(work_dynamic_init);
			if (work_periodic)
				cancel_delayed_work_sync(work_periodic);
#endif
			break;
		default:
			break;
	}
	return;
}

void telemetry_agent_notifiy_emesh_init_deinit(enum agent_notification_event event)
{
        ta_print_debug("Agent> %s event: %d\n", __func__, event);
        switch(event) {
                case AGENT_NOTIFY_EVENT_INIT:
                        schedule_delayed_work(&g_agent_obj.emesh_stats_work_periodic, msecs_to_jiffies(STATS_FREQUENCY));
                        break;
                case AGENT_NOTIFY_EVENT_DEINIT:
                        relay_reset(g_agent_obj.rfs_emesh_channel);
                        cancel_delayed_work_sync(&g_agent_obj.emesh_stats_work_periodic);
                        break;
                default:
                        break;
        }
        return;
}

#ifdef WLAN_CONFIG_TELEMETRY_AGENT
void telemetry_agent_dynamic_app_init_deinit_notify(
				     enum agent_notification_event event,
				     enum rm_services service_id,
				     uint64_t service_data,
				     bool is_container_app)
{
	struct rchan *rfs_channel;
	struct delayed_work *work_init;
	struct delayed_work *work_periodic;
	struct delayed_work *work_dynamic_init;

	ta_print_debug("Agent> %s event: %d for service %d \n", __func__, event, service_id);
	switch(event) {
		case AGENT_NOTIFY_EVENT_INIT:
			telemetry_agent_rm_get_service_data(service_id, service_data);
			g_agent_obj.rm_telemetry.app_init[service_id] = 1;
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel, &work_init,
					&work_periodic, &work_dynamic_init, service_id, is_container_app);

			if (rfs_channel)
				relay_reset(rfs_channel);

			if (work_periodic)
				cancel_delayed_work_sync(work_periodic);

			if (work_init)
				schedule_delayed_work(work_init,
						msecs_to_jiffies(STATS_FREQUENCY));

			if (work_dynamic_init && !is_container_app)
				schedule_delayed_work(work_dynamic_init,
						msecs_to_jiffies(STATS_FREQUENCY));
			break;
		case AGENT_NOTIFY_EVENT_DEINIT:
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel,
								 &work_init,
								 &work_periodic,
								 &work_dynamic_init,
								 service_id, is_container_app);
			g_agent_obj.rm_telemetry.app_init[service_id] = 0;
			if (rfs_channel)
				relay_reset(rfs_channel);
			if (work_init)
				cancel_delayed_work_sync(work_init);
			if (work_dynamic_init)
				cancel_delayed_work_sync(work_dynamic_init);
			if (work_periodic)
				cancel_delayed_work_sync(work_periodic);
			break;
		default:
			break;
	}
	return;
}
#else
void telemetry_agent_dynamic_app_init_deinit_notify(
				     enum agent_notification_event event,
				     enum rm_services service_id,
				     uint64_t service_data)
{
	struct rchan *rfs_channel;
	struct delayed_work *work_init;
	struct delayed_work *work_periodic;
	struct delayed_work *work_dynamic_init;

	ta_print_debug("Agent> %s event: %d for service %d \n", __func__, event, service_id);
	switch(event) {
		case AGENT_NOTIFY_EVENT_INIT:
			telemetry_agent_rm_get_service_data(service_id, service_data);
			g_agent_obj.rm_telemetry.app_init[service_id] = 1;
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel, &work_init,
					&work_periodic, &work_dynamic_init, service_id);

			if (work_init)
				schedule_delayed_work(work_init,
						msecs_to_jiffies(STATS_FREQUENCY));
			if (work_dynamic_init)
				schedule_delayed_work(work_dynamic_init,
						msecs_to_jiffies(STATS_FREQUENCY));
			if (work_periodic)
				schedule_delayed_work(work_periodic,
						msecs_to_jiffies(STATS_FREQUENCY));
			break;
		case AGENT_NOTIFY_EVENT_DEINIT:
			telemetry_agent_rm_get_delayed_work_info(&rfs_channel,
								 &work_init,
								 &work_periodic,
								 &work_dynamic_init,
								 service_id);
			g_agent_obj.rm_telemetry.app_init[service_id] = 0;
			if (rfs_channel)
				relay_reset(rfs_channel);
			if (work_init)
				cancel_delayed_work_sync(work_init);
			if (work_dynamic_init)
				cancel_delayed_work_sync(work_dynamic_init);
			if (work_periodic)
				cancel_delayed_work_sync(work_periodic);
			break;
		default:
			break;
	}
	return;
}
#endif /* WLAN_CONFIG_TELEMETRY_AGENT */


static int remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);
	return 0;
}

static struct dentry *create_buf_file_handler(const char *filename,
		struct dentry *parent,
		umode_t mode,
		struct rchan_buf *buf,
		int *is_global)
{
	struct dentry *buf_file;

	buf_file = debugfs_create_file(filename, mode, parent, buf,
			&relay_file_operations);
	if (IS_ERR(buf_file))
		return NULL;

	*is_global = 1;
	return buf_file;
}

static struct rchan_callbacks rfs_telemetry_agent_cb = {
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

static inline int telemetry_agent_init_relayfs_basic(struct telemetry_agent_object *agent_obj)
{
	agent_obj->dir_ptr = debugfs_create_dir("qca_telemetry", NULL);
	if (agent_obj->dir_ptr == NULL)
		return -EPERM;

	agent_obj->rm_telemetry.rfs_channel_erp = relay_open("erp_telemetry_agent",
		agent_obj->dir_ptr,
		(sizeof(struct telemetry_erp_buffer) + 1000), ERP_MAX_SUB_BUFFERS,
		&rfs_telemetry_agent_cb, NULL);
	if (!agent_obj->rm_telemetry.rfs_channel_erp) {
		debugfs_remove_recursive(agent_obj->dir_ptr);
		agent_obj->dir_ptr = NULL;
		return -EPERM;
	}

	return STATUS_SUCCESS;
}

#ifdef TELEMETRY_AGENT_256M
static inline int telemetry_agent_init_relayfs_enhanched(struct telemetry_agent_object *agent_obj)
{
	return STATUS_SUCCESS;
}
#else /* TELEMETRY_AGENT_256M */
static inline int telemetry_agent_init_relayfs_enhanched(struct telemetry_agent_object *agent_obj)
{

	size_t subbuf_size = sizeof(struct telemetry_rm_main_buffer) + 1000;

	agent_obj->rm_telemetry.rfs_channel_main = relay_open("main_telemetry_agent",
			agent_obj->dir_ptr,
			subbuf_size, RM_MAIN_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);
	if (!agent_obj->rm_telemetry.rfs_channel_main) {
		return -EPERM;
	}

	agent_obj->rm_telemetry.rfs_channel_pmlo = relay_open("pmlo_telemetry_agent",
			agent_obj->dir_ptr,
			subbuf_size, PMLO_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);
	if (!agent_obj->rm_telemetry.rfs_channel_pmlo) {
		return -EPERM;
	}

	agent_obj->rm_telemetry.rfs_channel_deter = relay_open("ds_telemetry_agent",
			agent_obj->dir_ptr,
			subbuf_size, DETER_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);
	if (!agent_obj->rm_telemetry.rfs_channel_deter) {
		return -EPERM;
	}

	agent_obj->rm_telemetry.rfs_channel_admctrl = relay_open("admctrl_telemetry_agent",
			agent_obj->dir_ptr,
			subbuf_size, ADMCTRL_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);
	if (!agent_obj->rm_telemetry.rfs_channel_admctrl) {
		return -EPERM;
	}

	agent_obj->rm_telemetry.rfs_channel_energysvc = relay_open("energysvc_telemetry_agent",
			agent_obj->dir_ptr,
			subbuf_size, ENERGY_SVC_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);

	if (!agent_obj->rm_telemetry.rfs_channel_energysvc) {
		return -EPERM;
	}

	agent_obj->dir_emesh_ptr = debugfs_create_dir("qca_emesh_stats", NULL);
	if (agent_obj->dir_emesh_ptr == NULL){
		return -EPERM;
	}

	agent_obj->rfs_emesh_channel = relay_open("telemetry_agent",
			agent_obj->dir_emesh_ptr,
			(sizeof(struct telemetry_emesh_buffer) + 1000), EMESH_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);
	if (!agent_obj->rfs_emesh_channel) {
		return -EPERM;
	}

	return STATUS_SUCCESS;
}
#endif /* TELEMETRY_AGENT_256M */

int telemetry_agent_deinit_relayfs(struct telemetry_agent_object *agent_obj)
{
	if (g_agent_obj.rm_telemetry.rfs_channel_main)
		relay_close(g_agent_obj.rm_telemetry.rfs_channel_main);

	if (g_agent_obj.rm_telemetry.rfs_channel_pmlo)
		relay_close(g_agent_obj.rm_telemetry.rfs_channel_pmlo);

	if (g_agent_obj.rm_telemetry.rfs_channel_deter) {
		relay_close(g_agent_obj.rm_telemetry.rfs_channel_deter);
	}
	if (g_agent_obj.rm_telemetry.rfs_channel_admctrl)
		relay_close(g_agent_obj.rm_telemetry.rfs_channel_admctrl);

	if (g_agent_obj.rm_telemetry.rfs_channel_erp)
		relay_close(g_agent_obj.rm_telemetry.rfs_channel_erp);

	if (g_agent_obj.rm_telemetry.rfs_channel_energysvc)
		relay_close(g_agent_obj.rm_telemetry.rfs_channel_energysvc);

	debugfs_remove_recursive(g_agent_obj.dir_ptr);
	g_agent_obj.dir_ptr = NULL;

	if (g_agent_obj.rfs_emesh_channel) {
		relay_close(g_agent_obj.rfs_emesh_channel);
	}

	debugfs_remove_recursive(g_agent_obj.dir_emesh_ptr);
	g_agent_obj.dir_emesh_ptr = NULL;

	return STATUS_SUCCESS;
}

int telemetry_agent_init_relayfs(struct telemetry_agent_object *agent_obj)
{
	if(telemetry_agent_init_relayfs_basic(agent_obj) != STATUS_SUCCESS) {
		return -EPERM;
	}

	if(telemetry_agent_init_relayfs_enhanched(agent_obj) != STATUS_SUCCESS) {
		telemetry_agent_deinit_relayfs(agent_obj);
		return -EPERM;
	}
	return STATUS_SUCCESS;
}

static int telemetry_agent_psoc_create_handler(void *arg, struct agent_psoc_obj *psoc_obj)
{
	int status = STATUS_SUCCESS;
	int soc_idx;
	struct agent_soc_db *psoc_db;

	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d\n",__func__,
			psoc_obj->psoc_back_pointer, psoc_obj->psoc_id);

	spin_lock_bh(&g_agent_obj.agent_lock);
	soc_idx = g_agent_obj.agent_db.num_socs;
	psoc_db = &g_agent_obj.agent_db.psoc_db[soc_idx];
	psoc_db->psoc_obj_ptr = psoc_obj->psoc_back_pointer;
	psoc_db->soc_id = soc_idx;
	psoc_db->num_pdevs = 0;

	ta_print_debug("AgentDB> %s: psoc: %p psoc_id: %d\n",__func__,
			psoc_db->psoc_obj_ptr, psoc_db->soc_id);
	g_agent_obj.agent_db.num_socs++;
	spin_unlock_bh(&g_agent_obj.agent_lock);

	return status;
}


static int telemetry_agent_pdev_create_handler(void *arg, struct agent_pdev_obj *pdev_obj)

{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	int pdev_idx;

	spin_lock_bh(&g_agent_obj.agent_lock);
	psoc_db = &g_agent_obj.agent_db.psoc_db[pdev_obj->psoc_id];
	if (!psoc_db->psoc_obj_ptr) {
		ta_print_error("%s: pdev DB create fail as psoc DB not created for soc_id %d\n",
				__func__, pdev_obj->psoc_id);
		spin_unlock_bh(&g_agent_obj.agent_lock);
		return STATUS_FAIL;
	}
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	pdev_idx = psoc_db->num_pdevs;
#else
	pdev_idx = pdev_obj->pdev_id;
#endif
	pdev_db  = &psoc_db->pdev_db[pdev_idx];

	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d\n",__func__,
			pdev_obj->psoc_back_pointer, pdev_obj->psoc_id,
			pdev_obj->pdev_back_pointer, pdev_obj->pdev_id);

	pdev_db->psoc_obj_ptr =  pdev_obj->psoc_back_pointer;
	pdev_db->pdev_obj_ptr = pdev_obj->pdev_back_pointer;
	pdev_db->pdev_id = pdev_idx;
	pdev_db->num_peers = 0;

	spin_lock_init(&pdev_db->peer_db_lock);
	INIT_LIST_HEAD(&(pdev_db->peer_db_list));

	ta_print_debug("AgentDB> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d\n",__func__,
			pdev_db->psoc_obj_ptr , pdev_obj->psoc_id,
			pdev_db->pdev_obj_ptr, pdev_db->pdev_id);
	psoc_db->num_pdevs++;
#if 0
	char buffer[200] = {'\0', };
	snprintf(buffer, sizeof(buffer), "Agent> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d\n",__func__,
			pdev_obj->psoc_back_pointer, pdev_obj->psoc_id,
			pdev_obj->pdev_back_pointer, pdev_obj->pdev_id
			);

	relay_write(g_agent_obj.rfs_channel, &buffer[0], strlen(buffer));
#endif

	spin_unlock_bh(&g_agent_obj.agent_lock);
	return status;
}

void telemtry_get_telemetry_tx_peer_stats(struct agent_peer_db *peer_db,
					  struct agent_peer_tx_ext_stats *raw_peer_stats)
{
	if (g_agent_ops && g_agent_ops->agent_get_peer_tx_stats)
		g_agent_ops->agent_get_peer_tx_stats(peer_db,
						     (void *)raw_peer_stats);
}

void telemetry_compute_peer_tx_stats(struct agent_peer_db *peer_db,
				     struct agent_peer_tx_ext_stats *raw_peer_stats)
{
	uint64_t last_tx_bytes, temp_avg_throughput_cal;
	uint64_t last_total_retry_count, last_retry_count, curr_tx_cnt;
	int pkt_type_index, mcs_index;

	last_total_retry_count = raw_peer_stats->total_retries -
			peer_db->peer_stats.raw_peer_stats.total_retries;
	last_retry_count = raw_peer_stats->retries -
			peer_db->peer_stats.raw_peer_stats.retries;
	curr_tx_cnt = raw_peer_stats->tx_cnt -
			peer_db->peer_stats.raw_peer_stats.tx_cnt;

	peer_db->peer_stats.derived_peer_stats.avg_counter++;

	last_tx_bytes = peer_db->peer_stats.raw_peer_stats.tx_bytes;
	peer_db->peer_stats.raw_peer_stats.tx_bytes = raw_peer_stats->tx_bytes;
	peer_db->peer_stats.derived_peer_stats.eg_rate = raw_peer_stats->tx_bytes - last_tx_bytes;

	peer_db->peer_stats.raw_peer_stats.tx_cnt = raw_peer_stats->tx_cnt;
	peer_db->peer_stats.raw_peer_stats.avg_ack_rssi = raw_peer_stats->avg_ack_rssi;
	peer_db->peer_stats.raw_peer_stats.tx_failed = raw_peer_stats->tx_failed;
	peer_db->peer_stats.raw_peer_stats.retries = raw_peer_stats->retries;
	peer_db->peer_stats.raw_peer_stats.total_retries = raw_peer_stats->total_retries;
	peer_db->peer_stats.derived_peer_stats.eg_rate = raw_peer_stats->tx_bytes - last_tx_bytes;
	peer_db->peer_stats.derived_peer_stats.avg_counter++;

	if (peer_db->peer_stats.derived_peer_stats.avg_counter == 1) {
		peer_db->peer_stats.derived_peer_stats.min_throughput =
		peer_db->peer_stats.derived_peer_stats.eg_rate;
		peer_db->peer_stats.derived_peer_stats.max_throughput =
		peer_db->peer_stats.derived_peer_stats.eg_rate;
	}
	if (peer_db->peer_stats.derived_peer_stats.eg_rate <
		peer_db->peer_stats.derived_peer_stats.min_throughput) {
		peer_db->peer_stats.derived_peer_stats.min_throughput =
		peer_db->peer_stats.derived_peer_stats.eg_rate;
	}
	if (peer_db->peer_stats.derived_peer_stats.eg_rate >
		peer_db->peer_stats.derived_peer_stats.max_throughput) {
		peer_db->peer_stats.derived_peer_stats.max_throughput =
		peer_db->peer_stats.derived_peer_stats.eg_rate;
	}

	temp_avg_throughput_cal = peer_db->peer_stats.derived_peer_stats.avg_throughput *
				  (peer_db->peer_stats.derived_peer_stats.avg_counter - 1);

	peer_db->peer_stats.derived_peer_stats.avg_throughput = div_u64((temp_avg_throughput_cal +
					 peer_db->peer_stats.derived_peer_stats.eg_rate),
					 peer_db->peer_stats.derived_peer_stats.avg_counter);

	for (pkt_type_index = 0; pkt_type_index < TELEMETRY_DOT11_MAX; pkt_type_index++) {
		for (mcs_index = 0; mcs_index < TELEMETRY_MAX_MCS; mcs_index++) {
			peer_db->peer_stats.raw_peer_stats.packet_type[pkt_type_index].mcs_count[mcs_index] =
			raw_peer_stats->packet_type[pkt_type_index].mcs_count[mcs_index];
		}
	}

	peer_db->peer_stats.peer_per_retries_stats.packet_error_rate =
		div_u64((last_total_retry_count * 100),
			(last_total_retry_count + curr_tx_cnt));
	peer_db->peer_stats.peer_per_retries_stats.retries_percentage =
		div_u64((last_retry_count * 100),
			 curr_tx_cnt);
}

void telemetry_extract_and_detect_peer_breach(struct agent_peer_db *peer_db)
{
	struct agent_peer_tx_ext_stats raw_peer_stats = {0};
	struct telemetry_pkt_type curr_mcs_packet_type[TELEMETRY_DOT11_MAX] = {0};
	uint8_t pkt_type_index, mcs_index;
	uint32_t threshold;
	bool mcs_breach = false;
	uint32_t mcs_min_threshold;
	uint32_t mcs_max_threshold;
	uint32_t min_thruput_rate;
	uint32_t max_thruput_rate;

	telemtry_get_telemetry_tx_peer_stats(peer_db, &raw_peer_stats);

	telemetry_compute_peer_tx_stats(peer_db, &raw_peer_stats);

	for (pkt_type_index = 0; pkt_type_index < TELEMETRY_DOT11_MAX; pkt_type_index++) {
		for (mcs_index = 0; mcs_index < TELEMETRY_MAX_MCS; mcs_index++) {
			curr_mcs_packet_type[pkt_type_index].mcs_count[mcs_index] =
			raw_peer_stats.packet_type[pkt_type_index].mcs_count[mcs_index] -
			peer_db->peer_stats.raw_peer_stats.packet_type[pkt_type_index].mcs_count[mcs_index];
		}
	}

	threshold = peer_db->peer_stats.peer_tx_stats_threshold.packet_error_rate;
	if (threshold) {
		if (peer_db->peer_stats.peer_per_retries_stats.packet_error_rate >
		    100 - threshold) {
			ta_print_error("packet error rate Breach Detected peer MAC %02x:%02x:%02x:%02x:%02x:%02x",
				       peer_db->peer_mac_addr[0],
				       peer_db->peer_mac_addr[1],
				       peer_db->peer_mac_addr[2],
				       peer_db->peer_mac_addr[3],
				       peer_db->peer_mac_addr[4],
				       peer_db->peer_mac_addr[5]);
		}
	}

	threshold = peer_db->peer_stats.peer_tx_stats_threshold.retries_threshold;
	if (threshold) {
		if (peer_db->peer_stats.peer_per_retries_stats.retries_percentage >
		    100 - threshold) {
			ta_print_error("retries percentage Breach Detected peer MAC %02x:%02x:%02x:%02x:%02x:%02x",
				       peer_db->peer_mac_addr[0],
				       peer_db->peer_mac_addr[1],
				       peer_db->peer_mac_addr[2],
				       peer_db->peer_mac_addr[3],
				       peer_db->peer_mac_addr[4],
				       peer_db->peer_mac_addr[5]);
		}
	}

	mcs_min_threshold = peer_db->peer_stats.peer_tx_stats_threshold.mcs_min_threshold;
	mcs_max_threshold = peer_db->peer_stats.peer_tx_stats_threshold.mcs_max_threshold;
	if (mcs_min_threshold && mcs_max_threshold) {
		for (pkt_type_index = 0; pkt_type_index < TELEMETRY_DOT11_MAX; pkt_type_index++) {
			for (mcs_index = 0; mcs_index < mcs_min_threshold; mcs_index++) {
				if (curr_mcs_packet_type[pkt_type_index].mcs_count[mcs_index] > 0) {
					mcs_breach = true;
				}
				if (mcs_breach)
					break;
			}
			for (mcs_index = mcs_max_threshold + 1; mcs_index < TELEMETRY_MAX_MCS && !mcs_breach; mcs_index++) {
				if (curr_mcs_packet_type[pkt_type_index].mcs_count[mcs_index] > 0) {
					mcs_breach = true;
				}
				if (mcs_breach)
					break;
			}
			if (mcs_breach)
				break;
		}
		if (mcs_breach) {
			ta_print_error("mcs bound Breach Detected peer MAC %02x:%02x:%02x:%02x:%02x:%02x",
				       peer_db->peer_mac_addr[0],
				       peer_db->peer_mac_addr[1],
				       peer_db->peer_mac_addr[2],
				       peer_db->peer_mac_addr[3],
				       peer_db->peer_mac_addr[4],
				       peer_db->peer_mac_addr[5]);
		}
	}

	min_thruput_rate = peer_db->peer_stats.peer_tx_stats_threshold.min_thruput_rate;
	if (min_thruput_rate) {
		if (min_thruput_rate > peer_db->peer_stats.derived_peer_stats.eg_rate) {
			ta_print_error("min throuput Breach Detected peer MAC %02x:%02x:%02x:%02x:%02x:%02x",
				       peer_db->peer_mac_addr[0],
				       peer_db->peer_mac_addr[1],
				       peer_db->peer_mac_addr[2],
				       peer_db->peer_mac_addr[3],
				       peer_db->peer_mac_addr[4],
				       peer_db->peer_mac_addr[5]);
		}
	}

	max_thruput_rate = peer_db->peer_stats.peer_tx_stats_threshold.max_thruput_rate;
	if (max_thruput_rate) {
		if (max_thruput_rate < peer_db->peer_stats.derived_peer_stats.eg_rate) {
			ta_print_error("max throuput Breach Detected peer MAC %02x:%02x:%02x:%02x:%02x:%02x",
				       peer_db->peer_mac_addr[0],
				       peer_db->peer_mac_addr[1],
				       peer_db->peer_mac_addr[2],
				       peer_db->peer_mac_addr[3],
				       peer_db->peer_mac_addr[4],
				       peer_db->peer_mac_addr[5]);
		}
	}

	return;
}

static int telemetry_detect_peer_breach(void)
{
	int num_socs_index, num_pdevs_index;

	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	struct list_head *node;

	spin_lock_bh(&g_agent_obj.agent_lock);

	for (num_socs_index = 0; num_socs_index < MAX_SOCS_DB; num_socs_index++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[num_socs_index];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		for (num_pdevs_index = 0; num_pdevs_index < MAX_PDEV_LINKS_DB; num_pdevs_index++) {
			pdev_db = &psoc_db->pdev_db[num_pdevs_index];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db,
						     node);
				if(peer_db)
					telemetry_extract_and_detect_peer_breach(peer_db);
			}
			spin_unlock_bh(&pdev_db->peer_db_lock);
		}
	}
	spin_unlock_bh(&g_agent_obj.agent_lock);

	return 0;
}

void telemetry_detect_peer_stats_per_sec(struct timer_list *timer)
{
	telemetry_detect_peer_breach();
	mod_timer(&g_agent_obj.peer_stats_timer,
		  jiffies + msecs_to_jiffies(STATS_FREQUENCY));
}

void telemetry_peer_stats_timer_init(void)
{
	timer_setup(&g_agent_obj.peer_stats_timer,
		    telemetry_detect_peer_stats_per_sec, 0);
	add_timer(&g_agent_obj.peer_stats_timer);
	mod_timer(&g_agent_obj.peer_stats_timer,
		  jiffies + msecs_to_jiffies(STATS_FREQUENCY));
}

bool telemetry_get_pext_stats_config(struct agent_soc_db *soc_db)
{
	bool pext_stats_enabled = 0;

	if (g_agent_ops && g_agent_ops->agent_get_pext_stats_enabled_flag)
		g_agent_ops->agent_get_pext_stats_enabled_flag(soc_db,
							       &pext_stats_enabled);

	return pext_stats_enabled;
}

void telemetry_peer_stats_timer_stop(void)
{
	if (g_agent_obj.peer_stats_timer_enabled) {
		del_timer_sync(&g_agent_obj.peer_stats_timer);
		g_agent_obj.peer_stats_timer_enabled = false;
	}
}

static int  telemetry_agent_peer_create_handler(void *arg, struct agent_peer_obj *peer_obj)

{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;

	spin_lock_bh(&g_agent_obj.agent_lock);
	psoc_db = &g_agent_obj.agent_db.psoc_db[peer_obj->psoc_id];
	if (!psoc_db->psoc_obj_ptr) {
		ta_print_error("%s: peer DB create fail as psoc DB not created for soc_id %d\n",
				__func__, peer_obj->psoc_id);
		spin_unlock_bh(&g_agent_obj.agent_lock);
		return STATUS_FAIL;
	}

	pdev_db = &psoc_db->pdev_db[peer_obj->pdev_id];
	if (!pdev_db->pdev_obj_ptr) {
		ta_print_error("%s: peer DB creation failed as pdev DB not creted for pdev_id: %d\n",
				__func__,peer_obj->pdev_id);
		spin_unlock_bh(&g_agent_obj.agent_lock);
		return STATUS_FAIL;
	}


	peer_db = kzalloc(sizeof(struct agent_peer_db), GFP_ATOMIC);
	if (!peer_db) {
		ta_print_error("peer context allocation failed");
		spin_unlock_bh(&g_agent_obj.agent_lock);
		return STATUS_FAIL;
	}

	peer_db->psoc_obj_ptr = peer_obj->psoc_back_pointer;
	peer_db->pdev_obj_ptr = peer_obj->pdev_back_pointer;
	peer_db->peer_obj_ptr = peer_obj->peer_back_pointer;
	peer_db->peer_id = peer_obj->peer_id;
	memcpy(&peer_db->peer_mac_addr[0], &peer_obj->peer_mac_addr[0], 6);
	spin_lock_bh(&pdev_db->peer_db_lock);
	list_add_tail(&peer_db->node, &pdev_db->peer_db_list);

	ta_print_debug("AgentDB> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d \n peer: %p peer_mac: %s\n",__func__,
			peer_db->psoc_obj_ptr, peer_obj->psoc_id,
			peer_db->pdev_obj_ptr, peer_obj->pdev_id,
			peer_db->peer_obj_ptr, print_mac_addr(&peer_db->peer_mac_addr[0]));

	if (telemetry_get_pext_stats_config(psoc_db)) {
		if (!g_agent_obj.peer_stats_timer_enabled) {
			telemetry_peer_stats_timer_init();
			g_agent_obj.peer_stats_timer_enabled = true;
		}
	}

	pdev_db->num_peers++;
	spin_unlock_bh(&pdev_db->peer_db_lock);
	spin_unlock_bh(&g_agent_obj.agent_lock);
	return status;
}


static int telemetry_agent_psoc_destroy_handler(void *arg, struct agent_psoc_obj *psoc_obj)
{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;

	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d\n",__func__,
			psoc_obj->psoc_back_pointer, psoc_obj->psoc_id);

	spin_lock_bh(&g_agent_obj.agent_lock);
	psoc_db = &g_agent_obj.agent_db.psoc_db[psoc_obj->psoc_id];
	psoc_db->psoc_obj_ptr = NULL;
	psoc_db->num_pdevs = 0;
	g_agent_obj.agent_db.num_socs--;
	spin_unlock_bh(&g_agent_obj.agent_lock);

	return status;
}

static int telemetry_agent_pdev_destroy_handler(void *arg, struct agent_pdev_obj *pdev_obj)

{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;

	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d \n",__func__,
			pdev_obj->psoc_back_pointer, pdev_obj->psoc_id,
			pdev_obj->pdev_back_pointer, pdev_obj->pdev_id);

	spin_lock_bh(&g_agent_obj.agent_lock);
	psoc_db = &g_agent_obj.agent_db.psoc_db[pdev_obj->psoc_id];
	pdev_db = &psoc_db->pdev_db[pdev_obj->pdev_id];
	pdev_db->psoc_obj_ptr = NULL;
	pdev_db->pdev_obj_ptr = NULL;
	psoc_db->num_pdevs--;
	spin_unlock_bh(&g_agent_obj.agent_lock);

	return status;
}

static int  telemetry_agent_peer_destroy_handler(void *arg, struct agent_peer_obj *peer_obj)
{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct list_head *node;
	struct list_head *node_remove = NULL;
	struct agent_peer_db *peer_db = NULL;
	struct agent_peer_db *peer_db_remove = NULL;

	spin_lock_bh(&g_agent_obj.agent_lock);
	psoc_db = &g_agent_obj.agent_db.psoc_db[peer_obj->psoc_id];
	if (!psoc_db->psoc_obj_ptr) {
		ta_print_error("%s: peer DB destroy failed as psoc DB not created for soc_id: %d\n",
				__func__, peer_obj->psoc_id);
		spin_unlock_bh(&g_agent_obj.agent_lock);
		return STATUS_FAIL;
	}

	pdev_db = &psoc_db->pdev_db[peer_obj->pdev_id];
	if (!pdev_db->pdev_obj_ptr) {
		ta_print_error("%s: peer DB destroy failed as pdev DB not pdev_id: %d\n",
				__func__, peer_obj->pdev_id);
		spin_unlock_bh(&g_agent_obj.agent_lock);
		return STATUS_FAIL;
	}

	spin_lock_bh(&pdev_db->peer_db_lock);
		list_for_each(node, &pdev_db->peer_db_list) {
			peer_db = list_entry(node, struct agent_peer_db, node);
			if (!peer_db) {
				ta_print_error("Peer ctx is null");
				continue;
			}

			if (peer_obj->peer_back_pointer ==
			    peer_db->peer_obj_ptr) {
				node_remove = node;
				peer_db_remove = peer_db;
				break;
			}
		}

	if (node_remove && peer_db_remove) {
		list_del(node);
		kfree(peer_db_remove);
	} else {
		ta_print_error("%s: Node not found\n", __func__);
	}
	pdev_db->num_peers--;

	spin_unlock_bh(&pdev_db->peer_db_lock);
	spin_unlock_bh(&g_agent_obj.agent_lock);
	return status;
}

static int telemetry_agent_set_param_handler(int command, int value)
{
	int status = STATUS_SUCCESS;

	switch (command)
	{
	case AGENT_SET_DEBUG_LEVEL:
		g_agent_obj.debug_mask = value;
		ta_print_debug("Agent > %s debug_mask=%d",
			       __func__, g_agent_obj.debug_mask);
		break;
	default:
		break;
	}
	return status;
}

static int telemetry_agent_get_param_handler(int command)
{
	int status = STATUS_SUCCESS;

	return status;
}


int telemetry_mac_addr_cmp(uint8_t *mac_a, uint8_t *mac_b)
{
	int i;

	for (i = 0; i < TELEMETRY_MAC_ADDR_SIZE; i++) {
		if (mac_a[i] != mac_b[i])
			return 1;
	}

	return 0;
}

static int telemetry_peer_sla_detect(uint8_t *peer_mac,
				     uint32_t packet_error_rate,
				     uint32_t retries_threshold,
				     uint32_t mcs_min_threshold,
				     uint32_t mcs_max_threshold,
				     uint32_t min_thruput_rate,
				     uint32_t max_thruput_rate)
{
	int num_socs_index, num_pdevs_index;

	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	struct list_head *node;

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (num_socs_index = 0; num_socs_index < MAX_SOCS_DB; num_socs_index++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[num_socs_index];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		for (num_pdevs_index = 0; num_pdevs_index < MAX_PDEV_LINKS_DB; num_pdevs_index++) {
			pdev_db = &psoc_db->pdev_db[num_pdevs_index];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db,
						     node);
				if (peer_db &&
				   (telemetry_mac_addr_cmp(peer_mac, peer_db->peer_mac_addr)) == 0) {
					peer_db->peer_stats.peer_tx_stats_threshold.packet_error_rate =
										 packet_error_rate;
					peer_db->peer_stats.peer_tx_stats_threshold.retries_threshold =
										 retries_threshold;
					peer_db->peer_stats.peer_tx_stats_threshold.mcs_min_threshold =
										 mcs_min_threshold;
					peer_db->peer_stats.peer_tx_stats_threshold.mcs_max_threshold =
										 mcs_max_threshold;
					peer_db->peer_stats.peer_tx_stats_threshold.min_thruput_rate =
										 min_thruput_rate;
					peer_db->peer_stats.peer_tx_stats_threshold.max_thruput_rate =
										 max_thruput_rate;
				}
			}
			spin_unlock_bh(&pdev_db->peer_db_lock);
		}
	}
	spin_unlock_bh(&g_agent_obj.agent_lock);

	return 0;

}

static int telemetry_agent_pull_tx_peer_stats(uint8_t *peer_mac,
					      uint32_t *min_tput,
					      uint32_t *max_tput,
					      uint32_t *avg_tput,
					      uint32_t *per,
					      uint32_t *retries_pct)
{
	int num_socs_index, num_pdevs_index;

	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	struct list_head *node;

	spin_lock_bh(&g_agent_obj.agent_lock);
	for (num_socs_index = 0; num_socs_index < MAX_SOCS_DB; num_socs_index++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[num_socs_index];
		if (!psoc_db->psoc_obj_ptr)
			continue;

		for (num_pdevs_index = 0; num_pdevs_index < MAX_PDEV_LINKS_DB; num_pdevs_index++) {
			pdev_db = &psoc_db->pdev_db[num_pdevs_index];
			if (!pdev_db->pdev_obj_ptr)
				continue;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db,
						     node);
				if (peer_db &&
				   (telemetry_mac_addr_cmp(peer_mac, peer_db->peer_mac_addr)) == 0) {
					*min_tput = peer_db->peer_stats.derived_peer_stats.min_throughput;
					*max_tput = peer_db->peer_stats.derived_peer_stats.max_throughput;
					*avg_tput = peer_db->peer_stats.derived_peer_stats.avg_throughput;
					*per = peer_db->peer_stats.peer_per_retries_stats.packet_error_rate;
					*retries_pct = peer_db->peer_stats.peer_per_retries_stats.retries_percentage;
					spin_unlock_bh(&pdev_db->peer_db_lock);
					spin_unlock_bh(&g_agent_obj.agent_lock);
					return 0;
				}
			}
			spin_unlock_bh(&pdev_db->peer_db_lock);
		}
	}
	spin_unlock_bh(&g_agent_obj.agent_lock);

	return 0;

}

struct telemetry_agent_ops agent_ops = {
	agent_psoc_create_handler:telemetry_agent_psoc_create_handler,
	agent_psoc_destroy_handler:telemetry_agent_psoc_destroy_handler,
	agent_pdev_create_handler:telemetry_agent_pdev_create_handler,
	agent_pdev_destroy_handler:telemetry_agent_pdev_destroy_handler,
	agent_peer_create_handler:telemetry_agent_peer_create_handler,
	agent_peer_destroy_handler:telemetry_agent_peer_destroy_handler,
	agent_set_param:telemetry_agent_set_param_handler,
	agent_get_param:telemetry_agent_get_param_handler,
	agent_notify_app_event:telemetry_agent_notify_app_init,
	agent_notify_emesh_event:telemetry_agent_notifiy_emesh_init_deinit,
	agent_dynamic_app_init_deinit_notify:telemetry_agent_dynamic_app_init_deinit_notify,
	agent_peer_sla_stats_threshold:telemetry_peer_sla_detect,
	agent_pull_tx_peer_stats: telemetry_agent_pull_tx_peer_stats,

	/* SAWF-ops */
	sawf_set_sla_dtct_cfg: telemetry_sawf_set_sla_detect_cfg,
	sawf_set_sla_cfg: telemetry_sawf_set_sla_cfg,
	sawf_set_svclass_cfg: telemetry_sawf_set_svclass_cfg,
	sawf_updt_delay_mvng: telemetry_sawf_set_mov_avg_params,
	sawf_updt_sla_params: telemetry_sawf_set_sla_params,
	sawf_alloc_peer: telemetry_sawf_alloc_peer,
	sawf_updt_queue_info: telemetry_sawf_update_queue_info,
	sawf_update_msduq_info: telemetry_sawf_update_msdu_queue_info,
	sawf_clear_msduq_info: telemetry_sawf_clear_msdu_queue_info,
	sawf_free_peer: telemetry_sawf_free_peer,
	sawf_peer_stats_reset: telemetry_sawf_peer_stats_reset,
	sawf_push_delay: telemetry_sawf_update_peer_delay,
	sawf_push_delay_mvng: telemetry_sawf_update_peer_delay_mov_avg,
	sawf_push_msdu_drop: telemetry_sawf_update_msdu_drop,
	sawf_pull_rate: telemetry_sawf_get_rate,
	sawf_pull_tx_rate: telemetry_sawf_get_tx_rate,
	sawf_pull_mov_avg: telemetry_sawf_pull_mov_avg,
	sawf_reset_peer_stats: telemetry_sawf_reset_peer_stats,
	sawf_set_sla_detect_config:telemetry_sawf_set_sla_detect_config,
	sawf_set_sla_config: telemetry_sawf_set_sla_config,
};

static int telemetry_agent_init_module_basic(void)
{
	int status = 0;

	erp_stats_buffer = kzalloc(sizeof(struct telemetry_erp_buffer),GFP_ATOMIC);
	if (!erp_stats_buffer) {
		ta_print_error("erp stats buffer alloc failed");
		return -EPERM;
	}

	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_init_main, telemetry_agent_stats_work_init_main);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_dynamic_init_main, telemetry_agent_stats_work_dynamic_init_main);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_init_erp, telemetry_agent_stats_work_init_erp);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_periodic_erp, telemetry_agent_stats_work_periodic_erp);

	return status;
}

static inline void telemetry_agent_free_basic_mem(void)
{
	kfree(erp_stats_buffer);
}

static inline void telemetry_agent_remove_workqueues_basic(void)
{
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_init_main);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_dynamic_init_main);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_init_erp);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_periodic_erp);
}

#ifdef TELEMETRY_AGENT_256M
static inline void telemetry_agent_free_enhanched_mem(void)
{
}

static inline int telemetry_agent_init_module_enhanched(void)
{
	return 0;
}

static inline void telemetry_agent_remove_workqueues_enhanched(void)
{
}

#else /* TELEMETRY_AGENT_256M */

static inline void telemetry_agent_remove_workqueues_enhanched(void)
{
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_init_pmlo);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_periodic_pmlo);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_init_deter);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_periodic_deter);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_init_admctrl);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_periodic_admctrl);
	cancel_delayed_work_sync(&g_agent_obj.emesh_stats_work_periodic);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_init_energysvc);
	cancel_delayed_work_sync(&g_agent_obj.rm_telemetry.stats_work_periodic_energysvc);
}

static inline int telemetry_agent_init_module_enhanched(void)
{
	int status = 0;

	pmlo_stats_buffer = kzalloc(sizeof(struct telemetry_pmlo_buffer),GFP_ATOMIC);
	if (!pmlo_stats_buffer)
	{
		ta_print_error("pmlo stats buffer alloc failed");
		return -EPERM;
	}

	emesh_stats_buffer = kzalloc(sizeof(struct telemetry_emesh_buffer),GFP_ATOMIC);
	if (!emesh_stats_buffer)
	{
		ta_print_error("emesh stats buffer alloc failed");
		return -EPERM;
	}

	deter_stats_buffer = kzalloc(sizeof(struct telemetry_deter_buffer),GFP_ATOMIC);
	if (!deter_stats_buffer)
	{
		ta_print_error("deter stats buffer alloc failed");
		return -EPERM;
	}

	admctrl_stats_buffer = kzalloc(sizeof(struct telemetry_admctrl_buffer), GFP_ATOMIC);
	if (!admctrl_stats_buffer)
	{
		ta_print_error("admctrl stats buffer alloc failed");
		return -EPERM;
	}

	energysvc_stats_buffer = kzalloc(sizeof(struct telemetry_energysvc_buffer),GFP_ATOMIC);
	if (!energysvc_stats_buffer)
	{
		ta_print_error("energy service stats buffer alloc failed");
		return -EPERM;
	}

	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_init_pmlo, telemetry_agent_stats_work_init_pmlo);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_periodic_pmlo, telemetry_agent_stats_work_periodic_pmlo);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_init_deter, telemetry_agent_deter_stats_work_init);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_periodic_deter, telemetry_deter_stats_work_periodic);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_init_admctrl, telemetry_agent_stats_work_init_admctrl);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_periodic_admctrl, telemetry_agent_stats_work_periodic_admctrl);
	INIT_DELAYED_WORK(&g_agent_obj.emesh_stats_work_periodic, telemetry_emesh_stats_work_periodic);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_init_energysvc, telemetry_agent_stats_work_init_energysvc);
	INIT_DELAYED_WORK(&g_agent_obj.rm_telemetry.stats_work_periodic_energysvc, telemetry_agent_stats_work_periodic_energysvc);

	return status;
}

static inline void telemetry_agent_free_enhanched_mem(void)
{
	kfree(pmlo_stats_buffer);
	kfree(emesh_stats_buffer);
	kfree(deter_stats_buffer);
	kfree(admctrl_stats_buffer);
	kfree(energysvc_stats_buffer);
	return;
}
#endif /* TELEMETRY_AGENT_256M*/

static int telemetry_agent_init_module(void)
{
	spin_lock_init(&g_agent_obj.agent_lock);

	if (telemetry_agent_init_module_basic())
		return -EPERM;

	register_telemetry_agent_ops(&agent_ops);

	if(telemetry_agent_init_relayfs(&g_agent_obj) != STATUS_SUCCESS) {
		return -EPERM;
	}

	g_agent_obj.debug_mask = TA_PRINT_ERROR;
	telemetry_sawf_init_ctx();

	return telemetry_agent_init_module_enhanched();
}

static void telemetry_agent_exit_module(void)
{
	telemetry_peer_stats_timer_stop();
	telemetry_agent_remove_workqueues_basic();
	telemetry_agent_remove_workqueues_enhanched();
	telemetry_agent_deinit_relayfs(&g_agent_obj);
	telemetry_agent_free_basic_mem();
	telemetry_agent_free_enhanched_mem();
	unregister_telemetry_agent_ops(&agent_ops);
	memset(&g_agent_obj, 0, sizeof(struct telemetry_agent_object));
	telemetry_sawf_free_ctx();
	return;
}

module_init(telemetry_agent_init_module);
module_exit(telemetry_agent_exit_module);
MODULE_LICENSE("Dual BSD/GPL");
