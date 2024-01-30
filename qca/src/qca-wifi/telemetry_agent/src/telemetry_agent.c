/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

struct telemetry_agent_object g_agent_obj; 
struct telemetry_buffer stats_buffer;
struct telemetry_emesh_buffer *emesh_stats_buffer;

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

	num_socs = g_agent_obj.agent_db.num_socs;
	emesh_stats_buffer->relayfs_stats.num_soc = num_socs;

	for (i = 0; i< num_socs; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		rfs_soc_stats = &emesh_stats_buffer->relayfs_stats.soc_stats[i];

		num_pdevs = psoc_db->num_pdevs;
		rfs_soc_stats->num_links = num_pdevs;
		for (j = 0; j < num_pdevs; j++) {
			pdev_db = &psoc_db->pdev_db[j];
			rfs_link_stats = &rfs_soc_stats->link_stats[j];
			wifi_driver_get_emesh_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);
			rfs_link_stats->link_idle_airtime = pdev_stats.link_idle_airtime;
			memcpy(&rfs_link_stats->link_mac[0],
			       pdev_stats.link_mac,6);
			num_peers = pdev_db->num_peers;
			num_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
				if ((peer_db) && !wifi_driver_get_emesh_peer_stats(peer_db->peer_obj_ptr, &peer_stats))
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
		} /* pdev */
	} /* soc */

	emesh_stats_buffer->end_magic_num = 0xEEEEDEAD;
	relay_write(g_agent_obj.rfs_emesh_channel, emesh_stats_buffer,
			sizeof(struct telemetry_emesh_buffer));
	relay_flush(g_agent_obj.rfs_emesh_channel);

	schedule_delayed_work(&g_agent_obj.emesh_stats_work_periodic, msecs_to_jiffies(STATS_FREQUECY));
}


void telemetry_agent_stats_work_periodic(struct work_struct *work) 
{
	int i, j, ac;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	int num_socs = 0;
	int num_pdevs = 0;
	int num_peers = 0, num_mlo_peers = 0;
	struct agent_link_iface_stats_obj pdev_stats = {0};
	struct agent_peer_iface_stats_obj peer_stats = {0};

	struct agent_soc_stats *rfs_soc_stats = NULL;
	struct agent_link_stats *rfs_link_stats = NULL;
	struct agent_peer_stats *rfs_peer_stats = NULL;
	struct list_head *node;

	memset(&stats_buffer, 0, sizeof(struct telemetry_buffer));
	stats_buffer.header.start_magic_num = 0xDEADBEAF;
	stats_buffer.header.stats_version = 1;
	stats_buffer.header.stats_type = RFS_STATS_DATA;
	stats_buffer.header.payload_len = sizeof(struct telemetry_buffer);

	ta_print_debug("Agent > %s len: %d\n", __func__,
			stats_buffer.header.payload_len);

	num_socs = g_agent_obj.agent_db.num_socs;
	stats_buffer.u.periodic_stats.num_soc = num_socs;

	for (i = 0; i< num_socs; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		rfs_soc_stats = &stats_buffer.u.periodic_stats.soc_stats[i];

		/* call wifi driver and prepare RelayFS Message */
		rfs_soc_stats->soc_id = psoc_db->soc_id;
		rfs_soc_stats->num_links = psoc_db->num_pdevs;

		num_pdevs = psoc_db->num_pdevs;

		for (j = 0; j < num_pdevs; j++) {
			pdev_db  = &psoc_db->pdev_db[j];
			rfs_link_stats = &rfs_soc_stats->link_stats[j];
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
			wifi_driver_get_pdev_stats(pdev_db->pdev_obj_ptr, &pdev_stats);
			rfs_link_stats->hw_link_id = pdev_stats.link_id;
			memcpy(rfs_link_stats->available_airtime,
			       pdev_stats.available_airtime,
			       sizeof(rfs_link_stats->available_airtime));
			memcpy(rfs_link_stats->link_airtime,
			       pdev_stats.link_airtime,
			       sizeof(rfs_link_stats->link_airtime));
			rfs_link_stats->freetime = pdev_stats.freetime;
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
			}
#endif
			num_peers = pdev_db->num_peers;
			num_mlo_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
				if ((peer_db) && !wifi_driver_get_peer_stats(peer_db->peer_obj_ptr, &peer_stats))
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
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
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
					rfs_peer_stats->rssi = peer_stats.rssi;
					rfs_peer_stats->sla_mask = peer_stats.sla_mask;
					num_mlo_peers++;
				}
			} /* peer */
			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_mlo_peers;
		} /* pdev */
	} /* soc */

	stats_buffer.end_magic_num = 0xBEAFDEAD;
	relay_write(g_agent_obj.rfs_channel, &stats_buffer,
			sizeof(struct telemetry_buffer));
	relay_flush(g_agent_obj.rfs_channel);

	schedule_delayed_work(&g_agent_obj.stats_work_periodic, msecs_to_jiffies(STATS_FREQUECY));
	return;
}

void telemetry_agent_stats_work_init(struct work_struct *work) 
{
	int i, j, idx, tid;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;
	int num_socs = 0;
	int num_pdevs = 0;
	int num_peers = 0, num_mlo_peers = 0;
	struct agent_psoc_iface_init_obj soc_stats = {0};
	struct agent_pdev_iface_init_obj pdev_stats = {0};
	struct agent_peer_iface_init_obj peer_stats = {0};

	struct agent_soc_init_stats *rfs_soc_stats = NULL;
	struct agent_link_init_stats *rfs_link_stats = NULL;
	struct agent_peer_init_stats *rfs_peer_stats = NULL;
	struct list_head *node;


#ifdef WLAN_TELEMETRY_STATS_SUPPORT
	wlan_cfg80211_t2lm_app_reply_init_response();
#endif

	stats_buffer.header.start_magic_num = 0xDEADBEAF;
	stats_buffer.header.stats_version = 1;
	stats_buffer.header.stats_type = RFS_INIT_DATA;
	stats_buffer.header.payload_len = sizeof(struct telemetry_buffer);

	num_socs = g_agent_obj.agent_db.num_socs;
	stats_buffer.u.init_stats.num_soc = num_socs;

	for (i = 0; i < num_socs; i++) {
		psoc_db = &g_agent_obj.agent_db.psoc_db[i];
		rfs_soc_stats = &stats_buffer.u.init_stats.soc_stats[i];

		/* call wifi driver and prepare RelayFS Message */
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
		wifi_driver_get_psoc_info(psoc_db->psoc_obj_ptr, &soc_stats);
#endif
		rfs_soc_stats->soc_id = soc_stats.soc_id;
		rfs_soc_stats->num_peers = soc_stats.num_peers;
		rfs_soc_stats->num_links = psoc_db->num_pdevs;

		num_pdevs = psoc_db->num_pdevs;

		for (j = 0; j< num_pdevs; j++) {
			pdev_db  = &psoc_db->pdev_db[j];
			rfs_link_stats = &rfs_soc_stats->link_stats[j];
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
			wifi_driver_get_pdev_info(pdev_db->pdev_obj_ptr, &pdev_stats);
#endif
			rfs_link_stats->hw_link_id = pdev_stats.link_id;
			num_peers = pdev_db->num_peers;
			num_mlo_peers = 0;

			spin_lock_bh(&pdev_db->peer_db_lock);
			list_for_each(node, &pdev_db->peer_db_list) {
				peer_db = list_entry(node, struct agent_peer_db, node);
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
				if ((peer_db) && !wifi_driver_get_peer_info(peer_db->peer_obj_ptr, &peer_stats))
#endif
                {
					rfs_peer_stats = &rfs_link_stats->peer_stats[num_mlo_peers];
					memcpy(&rfs_peer_stats->mld_mac_addr[0],
							peer_stats.peer_mld_mac, 6);
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

					rfs_peer_stats->chan_bw = peer_stats.bw;
					rfs_peer_stats->chan_freq = peer_stats.freq;

					memcpy(&rfs_peer_stats->tx_mcs_nss_map,
							&peer_stats.caps.tx_mcs_nss_map,
							WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX);

					memcpy(&rfs_peer_stats->rx_mcs_nss_map,
							&peer_stats.caps.rx_mcs_nss_map,
							WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX);
					rfs_peer_stats->ieee_link_id = peer_stats.ieee_link_id;

					num_mlo_peers++;
				}

			} /* peer */

			spin_unlock_bh(&pdev_db->peer_db_lock);
			rfs_link_stats->num_peers = num_mlo_peers;
		} /* pdev */
	} /* soc */

	stats_buffer.end_magic_num = 0xBEAFDEAD;
	relay_write(g_agent_obj.rfs_channel, &stats_buffer,
			sizeof(struct telemetry_buffer));
	relay_flush(g_agent_obj.rfs_channel);

	schedule_delayed_work(&g_agent_obj.stats_work_periodic, msecs_to_jiffies(STATS_FREQUECY));
	return;
}

void telemetry_agent_notify_app_init(enum agent_notification_event event)
{
	ta_print_debug("Agent> %s event: %d\n", __func__, event);
	switch(event) {
		case AGENT_NOTIFY_EVENT_INIT:
			schedule_delayed_work(&g_agent_obj.stats_work_init, msecs_to_jiffies(1000));
			break;
		case AGENT_NOTIFY_EVENT_DEINIT:
			relay_reset(g_agent_obj.rfs_channel);
			cancel_delayed_work_sync(&g_agent_obj.stats_work_init);
			cancel_delayed_work_sync(&g_agent_obj.stats_work_periodic);
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
                        schedule_delayed_work(&g_agent_obj.emesh_stats_work_periodic, msecs_to_jiffies(STATS_FREQUECY));
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


int telemetry_agent_deinit_relayfs(struct telemetry_agent_object *agent_obj) 
{
	if (g_agent_obj.rfs_channel) {
		relay_close(g_agent_obj.rfs_channel);
	}

	debugfs_remove_recursive(g_agent_obj.dir_ptr);

	if (g_agent_obj.rfs_emesh_channel) {
		relay_close(g_agent_obj.rfs_emesh_channel);
	}

	debugfs_remove_recursive(g_agent_obj.dir_emesh_ptr);
	return STATUS_SUCCESS;
}

int telemetry_agent_init_relayfs(struct telemetry_agent_object *agent_obj) 
{
	agent_obj->dir_ptr = debugfs_create_dir("qca_telemetry", NULL);
	if (agent_obj->dir_ptr == NULL) 
		return -EPERM;

	agent_obj->rfs_channel = relay_open("telemetry_agent",
			agent_obj->dir_ptr,
			(sizeof(struct telemetry_buffer) + 1000), MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);

	if (!agent_obj->rfs_channel) {
		debugfs_remove_recursive(agent_obj->dir_ptr);
		agent_obj->dir_ptr = NULL;
		return -EPERM;
	}

	agent_obj->dir_emesh_ptr = debugfs_create_dir("qca_emesh_stats", NULL);
	if (agent_obj->dir_emesh_ptr == NULL){
		relay_close(agent_obj->rfs_channel);
		debugfs_remove_recursive(agent_obj->dir_ptr);
		agent_obj->dir_ptr = NULL;
		return -EPERM;
	}

	agent_obj->rfs_emesh_channel = relay_open("telemetry_agent",
			agent_obj->dir_emesh_ptr,
			(sizeof(struct telemetry_emesh_buffer) + 1000), EMESH_MAX_SUB_BUFFERS,
			&rfs_telemetry_agent_cb, NULL);

	if (!agent_obj->rfs_emesh_channel) {
		debugfs_remove_recursive(agent_obj->dir_emesh_ptr);
		agent_obj->dir_emesh_ptr = NULL;
		relay_close(g_agent_obj.rfs_channel);
		debugfs_remove_recursive(agent_obj->dir_ptr);
		agent_obj->dir_ptr = NULL;
		return -EPERM;
	}

	return STATUS_SUCCESS;
}

static int telemetry_agent_psoc_create_handler(void *arg, struct agent_psoc_obj *psoc_obj)
{
	int status = STATUS_SUCCESS;
	int soc_idx = g_agent_obj.agent_db.num_socs;
	struct agent_soc_db *psoc_db = &g_agent_obj.agent_db.psoc_db[soc_idx];

	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d\n",__func__,
			psoc_obj->psoc_back_pointer, psoc_obj->psoc_id);

	psoc_db->psoc_obj_ptr = psoc_obj->psoc_back_pointer;
	psoc_db->soc_id = soc_idx;
	psoc_db->num_pdevs = 0;

	ta_print_debug("AgentDB> %s: psoc: %p psoc_id: %d\n",__func__,
			psoc_db->psoc_obj_ptr, psoc_db->soc_id);

	g_agent_obj.agent_db.num_socs++;

	return status;
}


static int telemetry_agent_pdev_create_handler(void *arg, struct agent_pdev_obj *pdev_obj)

{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	int pdev_idx; 

	psoc_db  = &g_agent_obj.agent_db.psoc_db[pdev_obj->psoc_id];
	if (!psoc_db->psoc_obj_ptr) {
		ta_print_error("%s: pdev DB create fail as psoc DB not created for soc_id %d\n",
				__func__, pdev_obj->psoc_id);
		return STATUS_FAIL;
	}

	pdev_idx = psoc_db->num_pdevs;
	pdev_db  = &psoc_db->pdev_db[pdev_idx];

	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d\n",__func__,
			pdev_obj->psoc_back_pointer, pdev_obj->psoc_id,
			pdev_obj->pdev_back_pointer, pdev_obj->pdev_id
		  );

	pdev_db->psoc_obj_ptr =  pdev_obj->psoc_back_pointer;
	pdev_db->pdev_obj_ptr = pdev_obj->pdev_back_pointer;
	pdev_db->pdev_id = pdev_idx;
	pdev_db->num_peers = 0;

	spin_lock_init(&pdev_db->peer_db_lock);
	INIT_LIST_HEAD(&(pdev_db->peer_db_list));

	ta_print_debug("AgentDB> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d\n",__func__,
			pdev_db->psoc_obj_ptr , pdev_obj->psoc_id,
			pdev_db->pdev_obj_ptr, pdev_db->pdev_id
		  );
	psoc_db->num_pdevs++;
#if 0
	char buffer[200] = {'\0', };
	snprintf(buffer, sizeof(buffer), "Agent> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d\n",__func__,
			pdev_obj->psoc_back_pointer, pdev_obj->psoc_id,
			pdev_obj->pdev_back_pointer, pdev_obj->pdev_id
			);

	relay_write(g_agent_obj.rfs_channel, &buffer[0], strlen(buffer));
#endif

	return status;
}

static int  telemetry_agent_peer_create_handler(void *arg, struct agent_peer_obj *peer_obj)

{
	int status = STATUS_SUCCESS;
	struct agent_soc_db *psoc_db = NULL;
	struct agent_pdev_db *pdev_db = NULL;
	struct agent_peer_db *peer_db = NULL;

	psoc_db = &g_agent_obj.agent_db.psoc_db[peer_obj->psoc_id];
	if (!psoc_db->psoc_obj_ptr) {
		ta_print_error("%s: peer DB create fail as psoc DB not created for soc_id %d\n",
				__func__, peer_obj->psoc_id);
		return STATUS_FAIL;
	}

	pdev_db = &psoc_db->pdev_db[peer_obj->pdev_id];
	if (!pdev_db->pdev_obj_ptr) {
		ta_print_error("%s: peer DB creation failed as pdev DB not creted for pdev_id: %d\n",
				__func__,peer_obj->pdev_id);
		return STATUS_FAIL;
	}


	peer_db = kzalloc(sizeof(struct agent_peer_db), GFP_ATOMIC);
	if (!peer_db) {
		ta_print_error("peer context allocation failed");
		return -1;
	}

	peer_db->psoc_obj_ptr = peer_obj->psoc_back_pointer;
	peer_db->pdev_obj_ptr = peer_obj->pdev_back_pointer;
	peer_db->peer_obj_ptr = peer_obj->peer_back_pointer;

	memcpy(&peer_db->peer_mac_addr[0], &peer_obj->peer_mac_addr[0], 6);
	spin_lock_bh(&pdev_db->peer_db_lock);
	list_add_tail(&peer_db->node, &pdev_db->peer_db_list);

	ta_print_debug("AgentDB> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d \n peer: %p peer_mac: %s\n",__func__,
			peer_db->psoc_obj_ptr, peer_obj->psoc_id,
			peer_db->pdev_obj_ptr, peer_obj->pdev_id,
			peer_db->peer_obj_ptr, print_mac_addr(&peer_db->peer_mac_addr[0])
		  );
	pdev_db->num_peers++;
	spin_unlock_bh(&pdev_db->peer_db_lock);
	return status;
}


static int telemetry_agent_psoc_destroy_handler(void *arg, struct agent_psoc_obj *psoc_obj)

{
	int status = STATUS_SUCCESS;
	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d\n",__func__,
			psoc_obj->psoc_back_pointer, psoc_obj->psoc_id);


	return status;
}

static int telemetry_agent_pdev_destroy_handler(void *arg, struct agent_pdev_obj *pdev_obj)

{
	int status = STATUS_SUCCESS;
	ta_print_debug("Agent> %s: psoc: %p psoc_id: %d pdev: %p pdev_id: %d \n",__func__,
			pdev_obj->psoc_back_pointer, pdev_obj->psoc_id,
			pdev_obj->pdev_back_pointer, pdev_obj->pdev_id
		  );

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

	psoc_db = &g_agent_obj.agent_db.psoc_db[peer_obj->psoc_id];
	if (!psoc_db->psoc_obj_ptr) {
		ta_print_error("%s: peer DB destroy failed as psoc DB not created for soc_id: %d\n",
				__func__, peer_obj->psoc_id);
		return STATUS_FAIL;
	}

	pdev_db = &psoc_db->pdev_db[peer_obj->pdev_id];
	if (!pdev_db->pdev_obj_ptr) {
		ta_print_error("%s: peer DB destroy failed as pdev DB not pdev_id: %d\n",
				__func__, peer_obj->pdev_id);
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
	return status;
}

static int telemetry_agent_set_param_handler(struct agent_config_params *params)
{
	int status = STATUS_SUCCESS;

	return status;
}

static int telemetry_agent_get_param_handler(struct agent_config_params *params)
{
	int status = STATUS_SUCCESS;

	return status;
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

	/* SAWF-ops */
	sawf_set_sla_dtct_cfg: telemetry_sawf_set_sla_detect_cfg,
	sawf_set_sla_cfg: telemetry_sawf_set_sla_cfg,
	sawf_set_svclass_cfg: telemetry_sawf_set_svclass_cfg,
	sawf_updt_delay_mvng: telemetry_sawf_set_mov_avg_params,
	sawf_updt_sla_params: telemetry_sawf_set_sla_params,
	sawf_alloc_peer: telemetry_sawf_alloc_peer,
	sawf_updt_queue_info: telemetry_sawf_update_queue_info,
	sawf_free_peer: telemetry_sawf_free_peer,
	sawf_push_delay: telemetry_sawf_update_peer_delay,
	sawf_push_delay_mvng: telemetry_sawf_update_peer_delay_mov_avg,
	sawf_push_msdu_drop: telemetry_sawf_update_msdu_drop,
	sawf_pull_rate: telemetry_sawf_get_rate,
	sawf_pull_mov_avg: telemetry_sawf_pull_mov_avg,
	sawf_reset_peer_stats: telemetry_sawf_reset_peer_stats,
};

static int telemetry_agent_init_module(void)
{
	int status = 0;

	emesh_stats_buffer = kzalloc(sizeof(struct telemetry_emesh_buffer),GFP_ATOMIC);
	if (!emesh_stats_buffer)
	{
		ta_print_error("emesh stats buffer alloc failed");
		return -EPERM;
	}
	register_telemetry_agent_ops(&agent_ops);
	if(telemetry_agent_init_relayfs(&g_agent_obj) != STATUS_SUCCESS) {
		return -EPERM;
	}
	g_agent_obj.debug_mask = TA_PRINT_ERROR;
	INIT_DELAYED_WORK(&g_agent_obj.stats_work_init, telemetry_agent_stats_work_init);
	INIT_DELAYED_WORK(&g_agent_obj.stats_work_periodic, telemetry_agent_stats_work_periodic);
	INIT_DELAYED_WORK(&g_agent_obj.emesh_stats_work_periodic, telemetry_emesh_stats_work_periodic);
	telemetry_sawf_init_ctx();
	return status;
}

static void telemetry_agent_exit_module(void)
{
	cancel_delayed_work_sync(&g_agent_obj.stats_work_init);
	cancel_delayed_work_sync(&g_agent_obj.stats_work_periodic);
	cancel_delayed_work_sync(&g_agent_obj.emesh_stats_work_periodic);
	telemetry_agent_deinit_relayfs(&g_agent_obj);
	unregister_telemetry_agent_ops(&agent_ops);
	memset(&g_agent_obj, 0, sizeof(struct telemetry_agent_object));
	kfree(emesh_stats_buffer);
	telemetry_sawf_free_ctx();
	return;
}

module_init(telemetry_agent_init_module);
module_exit(telemetry_agent_exit_module);
MODULE_LICENSE("Dual BSD/GPL");
