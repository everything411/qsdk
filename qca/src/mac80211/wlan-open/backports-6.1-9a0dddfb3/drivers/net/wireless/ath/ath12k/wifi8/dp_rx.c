// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/ieee80211.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <crypto/hash.h>
#include "../core.h"
#include "hal.h"
#include "../debug.h"
#include "../peer.h"
#include "../hw.h"
#include "../dp_rx.h"
#include "../debugfs_htt_stats.h"
#include "../dp_tx.h"
#include "../dp_mon.h"
#include "hal_rx.h"
#include "dp_rx.h"
#include "dp.h"
#include "hal_qcn9625.h"
#include "../debugfs.h"

#define ATH12K_WIFI8_REO_CMD_HIGHPRI_START_NUM	(DP_REO_CMD_RING_SIZE + 1)
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
#include <ppe_vp_public.h>
#include <ppe_vp_tx.h>
#endif
#include "../fse.h"
#include "dp_ast.h"
#include "dp.h"
#include "mgmt_rx.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppeds.h"
#endif

extern bool ath12k_debug_critical;

static int ath12k_wifi8_peer_rx_tid_delete_handler(struct ath12k_base *ab,
						   struct ath12k_dp_rx_tid *rx_tid,
						   u8 tid);
void ath12k_wifi8_peer_rx_tid_qref_reset(struct ath12k_base *ab, u16 peer_id, u16 tid);
static void ath12k_wifi8_dp_rx_fse_cache_inval_full_work(struct work_struct *work);

static inline bool ath12k_wifi8_dp_reo_cmd_shutdown(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_base *central_ab = central_dp->ab;

	return (test_bit(ATH12K_FLAG_CRASH_FLUSH, &central_ab->dev_flags) &&
		!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &central_ab->dev_flags)) ||
		test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &central_ab->dev_flags);
}

static int ath12k_wifi8_dp_reo_cmd_prepare(struct ath12k_dp_rx_reo_cmd **dp_cmd,
					   void *data, size_t len,
					   void (*cb)(struct ath12k_dp *dp,
						      void *ctx,
						      struct hal_reo_status *status))
{
	if (!cb) {
		*dp_cmd = NULL;
		return 0;
	}

	*dp_cmd = kzalloc(sizeof(**dp_cmd), GFP_ATOMIC);
	if (!*dp_cmd)
		return -ENOMEM;

	if (WARN_ON(len > sizeof((*dp_cmd)->u))) {
		kfree(*dp_cmd);
		*dp_cmd = NULL;
		return -EINVAL;
	}

	memcpy(&(*dp_cmd)->u, data, len);
	(*dp_cmd)->handler = cb;

	return 0;
}

static void ath12k_wifi8_dp_reo_cmd_queue(struct ath12k_dp *dp,
					  struct ath12k_dp_rx_reo_cmd *dp_cmd,
					  int cmd_num)
{
	if (!dp_cmd)
		return;

	dp_cmd->cmd_num = cmd_num;
	list_add_tail(&dp_cmd->list, &dp->reo_cmd_list);
}

static int validate_reo_batch(struct ath12k_reo_cmd_entry *cmds, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (!(cmds[i].cmd.flag & HAL_REO_CMD_FLG_FLUSH_BLOCK_LATER))
			continue;

		return -EOPNOTSUPP;
	}

	return 0;
}

static int ath12k_wifi8_dp_reo_cmd_send_ring(struct ath12k_base *ab,
					     struct hal_srng *cmd_ring,
					     void *data, size_t len,
					     enum hal_reo_cmd_type type,
					     struct ath12k_hal_reo_cmd *cmd,
					     void (*cb)(struct ath12k_dp *dp, void *ctx,
							struct hal_reo_status *status))
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_reo_cmd *dp_cmd;
	int cmd_num;
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_base *central_ab = central_dp->ab;
	int ret;

	if (ath12k_wifi8_dp_reo_cmd_shutdown(ab))
		return -ESHUTDOWN;

	ret = ath12k_wifi8_dp_reo_cmd_prepare(&dp_cmd, data, len, cb);
	if (ret)
		return ret;

	cmd_num = ath12k_wifi8_hal_reo_cmd_send(central_ab, cmd_ring, type, cmd);

	if (cmd_num < 0) {
		kfree(dp_cmd);
		return cmd_num;
	}

	if (cmd_num == 0) {
		kfree(dp_cmd);
		return -EINVAL;
	}

	if (!dp_cmd)
		return 0;

	spin_lock_bh(&central_dp->reo_cmd_lock);
	ath12k_wifi8_dp_reo_cmd_queue(central_dp, dp_cmd, cmd_num);
	spin_unlock_bh(&central_dp->reo_cmd_lock);

	return 0;
}

static int ath12k_wifi8_dp_reo_cmd_send_ring_n(struct ath12k_base *ab,
					       struct hal_srng *cmd_ring,
					       struct ath12k_reo_cmd_entry *entries,
					       struct ath12k_reo_dp_cmd_desc *dp_descs,
						       int n)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_base *central_ab = central_dp->ab;
	struct ath12k_reo_cmd_entry *cmds = entries;
	struct ath12k_dp_rx_reo_cmd **dp_cmds;
	int i, ret;

	if (WARN_ON(n <= 0))
		return -EINVAL;

	if (ath12k_wifi8_dp_reo_cmd_shutdown(ab))
		return -ESHUTDOWN;

	ret = validate_reo_batch(cmds, n);
	if (ret)
		return ret;

	dp_cmds = kcalloc(n, sizeof(*dp_cmds), GFP_ATOMIC);
	if (!dp_cmds)
		return -ENOMEM;

	for (i = 0; i < n; i++) {
		ret = ath12k_wifi8_dp_reo_cmd_prepare(&dp_cmds[i],
						      dp_descs[i].data,
						      dp_descs[i].len,
						      dp_descs[i].cb);
		if (ret)
			goto err_free;
	}

	spin_lock_bh(&cmd_ring->lock);
	ath12k_hal_srng_access_begin(central_ab, cmd_ring);

	ret = ath12k_wifi8_hal_reo_cmd_send_n_locked(central_ab, cmd_ring, cmds, n);
	if (!ret) {
		spin_lock_bh(&central_dp->reo_cmd_lock);
		for (i = 0; i < n; i++)
			ath12k_wifi8_dp_reo_cmd_queue(central_dp, dp_cmds[i],
						      cmds[i].cmd_num);
		spin_unlock_bh(&central_dp->reo_cmd_lock);
	}

	ath12k_hal_srng_access_end(central_ab, cmd_ring);
	spin_unlock_bh(&cmd_ring->lock);

	if (ret)
		goto err_free;

	kfree(dp_cmds);
	return 0;

err_free:
	for (i = 0; i < n; i++)
		kfree(dp_cmds[i]);
	kfree(dp_cmds);

	return ret;
}

int ath12k_wifi8_dp_reo_cmd_send(struct ath12k_base *ab,
				 void *data, size_t len,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    struct hal_reo_status *status))
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct hal_srng *cmd_ring;
	u32 ring_id = central_dp->reo_cmd_ring.ring_id;

	cmd_ring = &central_dp->ab->hal.srng_list[ring_id];

	return ath12k_wifi8_dp_reo_cmd_send_ring(ab, cmd_ring, data, len,
						 type, cmd, cb);
}

int ath12k_wifi8_dp_reo_cmd_send_highprio(struct ath12k_base *ab,
					  void *data, size_t len,
					  enum hal_reo_cmd_type type,
					  struct ath12k_hal_reo_cmd *cmd,
					  void (*cb)(struct ath12k_dp *dp, void *ctx,
						     struct hal_reo_status *status))
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(central_dp);
	struct hal_srng *cmd_ring;
	u32 ring_id = dp_wifi8->reo_high_prio_cmd_ring.ring_id;

	cmd_ring = &central_dp->ab->hal.srng_list[ring_id];

	return ath12k_wifi8_dp_reo_cmd_send_ring(ab, cmd_ring, data, len,
						 type, cmd, cb);
}

int ath12k_wifi8_dp_reo_cmd_send_highprio_n(struct ath12k_base *ab,
					    struct ath12k_reo_cmd_entry *entries,
					    struct ath12k_reo_dp_cmd_desc *dp_descs,
					    int n)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(central_dp);
	struct hal_srng *cmd_ring;
	u32 ring_id = dp_wifi8->reo_high_prio_cmd_ring.ring_id;

	cmd_ring = &central_dp->ab->hal.srng_list[ring_id];

	return ath12k_wifi8_dp_reo_cmd_send_ring_n(ab, cmd_ring, entries,
						   dp_descs, n);
}

int ath12k_wifi8_dp_reo_cache_flush(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS |
		HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS;

	/* For all QoS TIDs (except NON_QOS and MGMT), the driver allocates
	 * a maximum window size of 1024. In such cases, the driver can issue
	 * a single 1KB descriptor flush command instead of sending multiple
	 * 128-byte flush commands for each QoS TID, improving efficiency.
	 */

	if (!ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(rx_tid->tid))
		cmd.flag |= HAL_REO_CMD_FLG_FLUSH_QUEUE_1K_DESC;

	ret = ath12k_wifi8_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_FLUSH_CACHE,
					   &cmd, ath12k_dp_reo_cmd_free);

	return ret;
}

int ath12k_wifi8_dp_fse_cmd_send(struct ath12k_base *ab,
				 struct hal_fse_cmd *fse_cmd)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_base *central_ab;
	struct ath12k_dp_wifi8 *dp_wifi8;
	struct hal_srng *cmd_ring;
	int ret;

	if (!central_dp) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Central DP is NULL");
		return 0;
	}

	central_ab = central_dp->ab;
	dp_wifi8 = ath12k_get_dp_wifi8(central_dp);

	if ((test_bit(ATH12K_FLAG_CRASH_FLUSH, &central_ab->dev_flags) &&
	     !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &central_ab->dev_flags)) ||
	    test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &central_ab->dev_flags))
		return -ESHUTDOWN;


	cmd_ring = &central_ab->hal.srng_list[dp_wifi8->fse_cmd_ring.ring_id];
	ret = ath12k_wifi8_hal_fse_cmd_send(central_ab, cmd_ring, fse_cmd);
	if (ret == -ENOBUFS) {
		struct dp_rx_fst *fst = central_dp->dp_hw_grp->fst;

		/* The FSE command ring is full. Rather than failing the caller,
		 * schedule a single deferred full cache invalidation and return 0
		 * to the caller.
		 */
		if (!delayed_work_pending(&fst->cache_inval_full_work))
			schedule_delayed_work(&fst->cache_inval_full_work,
					      msecs_to_jiffies(ATH12K_FSE_CACHE_INVAL_FULL_DELAY_MS));
		ret = 0;
	}

	return ret;
}

void ath12k_wifi8_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id, u16 tid,
					 dma_addr_t paddr)
{
	struct ath12k_reo_queue_ref *qref;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (!central_dp->reoq_lut.vaddr) {
		ath12k_warn(ab, "reo qref table is not setup\n");
		return;
	}

	if (peer_id > DP_MAX_PEER_ID) {
		ath12k_warn(ab, "peer id %d is more than Max peer id\n", peer_id);
		return;
	}

	qref = (struct ath12k_reo_queue_ref *)central_dp->reoq_lut.vaddr +
			(peer_id * ab->hal.hal_params->num_tids + tid);

	qref->info0 = u32_encode_bits(lower_32_bits(paddr),
				      BUFFER_ADDR_INFO0_ADDR);
	qref->info1 = u32_encode_bits(upper_32_bits(paddr),
				      BUFFER_ADDR_INFO1_ADDR) |
		      u32_encode_bits(tid, DP_REO_QREF_NUM);
	ath12k_wifi8_hal_reo_shared_qaddr_cache_clear(central_dp->ab);
}

void ath12k_wifi8_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
				     struct hal_reo_status *status)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_rx_tid *rx_tid = ctx, *update_rx_tid;
	struct ath12k_dp_rx_reo_cache_flush_elem *elem, *tmp;
	struct dp_reo_update_rx_queue_elem *qelem, *qtmp;

	if (!status || status->uniform_hdr.cmd_status == HAL_REO_CMD_DRAIN) {
		goto free_desc;
	} else if (status->uniform_hdr.cmd_status != HAL_REO_CMD_SUCCESS) {
		/* Shouldn't happen! Cleanup in case of other failure? */
		ath12k_warn(ab, "failed to delete rx tid %d hw descriptor %d\n",
			    rx_tid->tid, status->uniform_hdr.cmd_status);
		return;
	}

	/* Check if there is any pending rx_queue, if yes then update it */
	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_for_each_entry_safe(qelem, qtmp, &dp->reo_cmd_update_rx_queue_list,
				 list) {
		if (qelem->reo_cmd_update_rx_queue_resend_flag &&
		    qelem->data.active) {
			update_rx_tid = &qelem->data;

			if (ath12k_wifi8_peer_rx_tid_delete_handler(ab, update_rx_tid,
								    qelem->tid)) {
				update_rx_tid->active = true;
				break;
			}
			update_rx_tid->active = false;
			update_rx_tid->vaddr = NULL;
			update_rx_tid->paddr = 0;
			update_rx_tid->size = 0;
			update_rx_tid->pending_desc_size = 0;

			list_del(&qelem->list);
			kfree(qelem);
		}
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	elem = kzalloc(sizeof(*elem), GFP_ATOMIC);
	if (!elem)
		goto free_desc;

	elem->ts = jiffies;
	memcpy(&elem->data, rx_tid, sizeof(*rx_tid));

	spin_lock_bh(&dp->reo_cmd_lock);
	list_add_tail(&elem->list, &dp->reo_cmd_cache_flush_list);
	dp->reo_cmd_cache_flush_count++;

	/* Flush and invalidate aged REO desc from HW cache */
	list_for_each_entry_safe(elem, tmp, &dp->reo_cmd_cache_flush_list,
				 list) {
		if (dp->reo_cmd_cache_flush_count > ATH12K_DP_RX_REO_DESC_FREE_THRES ||
		    time_after(jiffies, elem->ts +
			       msecs_to_jiffies(ATH12K_DP_RX_REO_DESC_FREE_TIMEOUT_MS))) {
			/* Unlock the reo_cmd_lock before using ath12k_dp_reo_cmd_send()
			 * within ath12k_wifi8_dp_reo_cache_flush. The
			 * reo_cmd_cache_flush_list is used in only two contexts, one is
			 * in this function called from napi and the other in
			 * ath12k_dp_free during core destroy.
			 * Before dp_free, the irqs would be disabled and would wait to
			 * synchronize. Hence there wouldn’t be any race against add or
			 * delete to this list. Hence unlock-lock is safe here.
			 */
			spin_unlock_bh(&dp->reo_cmd_lock);
			if (ath12k_wifi8_dp_reo_cache_flush(dp->ab, &elem->data)) {
				/* In failure case, just update the timestamp
				 * for flush cache elem and continue
				 */
				spin_lock_bh(&dp->reo_cmd_lock);
				elem->ts = jiffies;
				break;
			}
			spin_lock_bh(&dp->reo_cmd_lock);
			list_del(&elem->list);
			dp->reo_cmd_cache_flush_count--;
			kfree(elem);
		}
	}
	spin_unlock_bh(&dp->reo_cmd_lock);

	return;
free_desc:
	rx_tid->active = false;
	ath12k_dp_rx_tid_free_desc(ab, rx_tid);
}

static int ath12k_wifi8_peer_rx_tid_delete_handler(struct ath12k_base *ab,
						   struct ath12k_dp_rx_tid *rx_tid,
						   u8 tid)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_held(&dp->reo_cmd_update_rx_queue_lock);

	rx_tid->active = false;
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.upd0 |= HAL_REO_CMD_UPD0_VLD;
	cmd.upd0 |= HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
	cmd.ba_window_size = ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid) ?
			      rx_tid->ba_win_sz : DP_BA_WIN_SZ_MAX;
	cmd.upd1 |= HAL_REO_CMD_UPD1_VLD;

	return ath12k_wifi8_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					    HAL_REO_CMD_UPDATE_RX_QUEUE, &cmd,
					    ath12k_wifi8_dp_rx_tid_del_func);
}

void ath12k_wifi8_peer_rx_tid_qref_reset(struct ath12k_base *ab, u16 peer_id, u16 tid)
{
	struct ath12k_reo_queue_ref *qref;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (!central_dp->reoq_lut.vaddr) {
		ath12k_warn(ab, "reo qref table is not setup\n");
		return;
	}

	if (peer_id > DP_MAX_PEER_ID) {
		ath12k_warn(ab, "peer id %d is more than Max peer id\n", peer_id);
		return;
	}

	qref = (struct ath12k_reo_queue_ref *)central_dp->reoq_lut.vaddr +
			(peer_id * ab->hal.hal_params->num_tids + tid);

	qref->info0 = u32_encode_bits(0, BUFFER_ADDR_INFO0_ADDR);
	qref->info1 = u32_encode_bits(0, BUFFER_ADDR_INFO1_ADDR);
}

void ath12k_wifi8_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid)
{
	struct ath12k_dp_rx_tid *rx_tid = &peer->dp_peer->rx_tid[tid];
	struct ath12k_dp_rx_tid *temp_rx_tid = NULL;
	struct dp_reo_update_rx_queue_elem *elem, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp   = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_base *central_ab = central_dp->ab;

	if (!rx_tid->active)
		return;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER | ATH12K_DBG_SMD, ATH12K_DBG_L2,
			 "dp_rx_peer_tid_delete: peer %pM peer_id=%u tid=%u\n",
			 peer->addr, peer->dp_peer->peer_id, tid);

	elem = kzalloc(sizeof(*elem), GFP_ATOMIC);
	if (!elem)
		return;

	elem->reo_cmd_update_rx_queue_resend_flag = false;
	elem->peer_id = peer->peer_id;
	elem->tid = tid;
	elem->is_ml_peer = peer->mlo ? true : false;
	elem->ml_peer_id = peer->ml_id;

	memcpy(&elem->data, rx_tid, sizeof(*rx_tid));

	spin_lock_bh(&central_dp->reo_cmd_update_rx_queue_lock);
	list_add_tail(&elem->list, &central_dp->reo_cmd_update_rx_queue_list);

	list_for_each_entry_safe(elem, tmp, &central_dp->reo_cmd_update_rx_queue_list,
				 list) {
		temp_rx_tid = &elem->data;

		if (ath12k_wifi8_peer_rx_tid_delete_handler(central_ab, temp_rx_tid,
							    elem->tid)) {
			temp_rx_tid->active = true;
			elem->reo_cmd_update_rx_queue_resend_flag = true;
			break;
		}
		temp_rx_tid->active = false;
		temp_rx_tid->vaddr = NULL;
		temp_rx_tid->paddr = 0;
		temp_rx_tid->size = 0;
		temp_rx_tid->pending_desc_size = 0;

		list_del(&elem->list);
		kfree(elem);
	}
	spin_unlock_bh(&central_dp->reo_cmd_update_rx_queue_lock);

	rx_tid->active = false;
	ath12k_wifi8_peer_rx_tid_qref_reset(central_ab, peer->dp_peer->peer_id, tid);
	ath12k_wifi8_hal_reo_shared_qaddr_cache_clear(central_ab);
	rx_tid->vaddr = NULL;
	rx_tid->paddr = 0;
	rx_tid->size = 0;
	rx_tid->pending_desc_size = 0;
}

bool ath12k_wifi8_dp_mlo_peer_tid_teardown_ready(struct ath12k_dp_peer *dp_peer,
						 struct ath12k_dp_link_peer *link_peer)
{
	/* AP-mode MLO peers: primary_link is stable for the peer lifetime,
	 * same as wifi7.  Only the link that set up rx_tid[] tears it down.
	 *
	 * TODO: revisit if AP-side link reconfiguration or future AP UMAC
	 * migration can also move primary_link at runtime; if so, drop the
	 * is_sta_bss_peer restriction and use the peer_links_map check for
	 * all wifi8 MLO peers.
	 */
	if (!dp_peer->is_sta_bss_peer)
		return link_peer->primary_link;

	/* STA-mode BSS peers: during SMD BSS Transition the STA must keep the
	 * Serving AP's rx_tid[] active until all its links are torn down, so
	 * that in-flight frames from the Serving AP continue to drain through
	 * the existing REO queues.  Once the last Serving AP link is removed,
	 * the Target AP MLD peer owns the rx_tid[] state, so this is the only
	 * safe point to tear it down.
	 *
	 * (peer_links_map & ~BIT(link_id)) == 0 evaluates to true when this is
	 * the only remaining link, regardless of whether the caller has already
	 * decremented peer_links_map (ath12k_mac_dp_peer_cleanup_cb) or not
	 * (ath12k_dp_peer_cleanup, which defers it to
	 * ath12k_dp_cp_link_peer_unassign).
	 */
	return (dp_peer->peer_links_map & ~BIT(link_peer->hw_link_id)) == 0;
}

void  ath12k_wifi8_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					     struct ath12k_dp_rx_tid *rx_tid,
					     u32 cipher, enum set_key_cmd key_cmd)
{
	cmd->flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd->upd0 = HAL_REO_CMD_UPD0_PN |
			HAL_REO_CMD_UPD0_PN_SIZE |
			HAL_REO_CMD_UPD0_PN_VALID |
			HAL_REO_CMD_UPD0_PN_CHECK |
			HAL_REO_CMD_UPD0_SVLD;

	switch (cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		if (key_cmd == SET_KEY) {
			cmd->upd1 |= HAL_REO_CMD_UPD1_PN_CHECK;
			cmd->pn_size = 48;
		}
		break;
	default:
		break;
	}

	cmd->addr_lo = lower_32_bits(rx_tid->paddr);
	cmd->addr_hi = upper_32_bits(rx_tid->paddr);
}

static int
ath12k_wifi8_hal_reo_qdesc_update_bitmaps_direct(struct ath12k_base *ab,
						 struct ath12k_dp_rx_tid *rx_tid,
						 struct ath12k_rx_smd_ctx_per_tid
						 *smd_ctx)
{
	struct hal_rx_reo_queue *qdesc;

	/* Get virtual address of queue descriptor */
	qdesc = (struct hal_rx_reo_queue *)rx_tid->vaddr;
	if (!qdesc) {
		ath12k_err(ab, "NULL queue descriptor vaddr for tid %d\n", rx_tid->tid);
		return -EINVAL;
	}

	/*
	 * Update bitmap fields (0-287)
	 * These are always present in the main descriptor
	 */
	qdesc->rx_bitmap_31_0 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_31_0);
	qdesc->rx_bitmap_63_32 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_63_32);
	qdesc->rx_bitmap_95_64 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_95_64);
	qdesc->rx_bitmap_127_96 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_127_96);
	qdesc->rx_bitmap_159_128 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_159_128);
	qdesc->rx_bitmap_191_160 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_191_160);
	qdesc->rx_bitmap_223_192 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_223_192);
	qdesc->rx_bitmap_255_224 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_255_224);
	qdesc->rx_bitmap_287_256 = cpu_to_le32(smd_ctx->bitmap_287_0.rx_bitmap_287_256);

	/*
	 * Always update extended bitmap fields (288-1023)
	 * Update all fields unconditionally for simplicity and consistency
	 */
	qdesc->rx_bitmap_319_288 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_319_288);
	qdesc->rx_bitmap_351_320 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_351_320);
	qdesc->rx_bitmap_383_352 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_383_352);
	qdesc->rx_bitmap_415_384 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_415_384);
	qdesc->rx_bitmap_447_416 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_447_416);
	qdesc->rx_bitmap_479_448 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_479_448);
	qdesc->rx_bitmap_511_480 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_511_480);
	qdesc->rx_bitmap_543_512 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_543_512);
	qdesc->rx_bitmap_575_544 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_575_544);
	qdesc->rx_bitmap_607_576 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_607_576);
	qdesc->rx_bitmap_639_608 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_639_608);
	qdesc->rx_bitmap_671_640 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_671_640);
	qdesc->rx_bitmap_703_672 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_703_672);
	qdesc->rx_bitmap_735_704 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_735_704);
	qdesc->rx_bitmap_767_736 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_767_736);
	qdesc->rx_bitmap_799_768 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_799_768);
	qdesc->rx_bitmap_831_800 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_831_800);
	qdesc->rx_bitmap_863_832 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_863_832);
	qdesc->rx_bitmap_895_864 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_895_864);
	qdesc->rx_bitmap_927_896 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_927_896);
	qdesc->rx_bitmap_959_928 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_959_928);
	qdesc->rx_bitmap_991_960 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_991_960);
	qdesc->rx_bitmap_1023_992 =
		cpu_to_le32(smd_ctx->bitmap_1023_288.rx_bitmap_1023_992);

	/*
	 * Flush CPU cache for coherency, then invalidate REO HW cache so
	 * stale entries can't be written back over the updated bitmap.
	 */
	ath12k_core_dma_sync_single_for_device(ab->dev, rx_tid->paddr,
					       rx_tid->size, DMA_TO_DEVICE);

	return 0;
}

int ath12k_wifi8_peer_rx_tid_reo_update_for_smd(struct ath12k_base *ab,
						struct ath12k_dp_hw *dp_hw,
						struct ieee80211_vif *vif,
						const u8 *peer_addr,
						struct ath12k_rx_smd_ctx_per_tid
						*rx_tid_ctx)
{
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_hal_reo_cmd cmd;
	struct ath12k_dp_peer *dp_peer;
	u32 ba_win_sz;
	u16 ssn;
	int ret;

	if (!dp_hw || !peer_addr || !rx_tid_ctx) {
		ath12k_err(ab, "invalid args for SMD REO update\n");
		return -EINVAL;
	}

	if (rx_tid_ctx->ssn > 0xFFF) {
		ath12k_warn(ab, "Invalid SSN 0x%x for tid %d\n",
			    rx_tid_ctx->ssn, rx_tid_ctx->tid);
		return -EINVAL;
	}

	if (rx_tid_ctx->tid >= ab->hal.hal_params->num_tids) {
		ath12k_warn(ab, "invalid tid %d for SMD REO update\n",
			    rx_tid_ctx->tid);
		return -EINVAL;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, (u8 *)peer_addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "failed to find peer %pM for SMD REO update\n",
			    peer_addr);
		return -ENOENT;
	}

	rx_tid = &dp_peer->rx_tid[rx_tid_ctx->tid];
	spin_lock_bh(&rx_tid->tid_lock);
	if (!rx_tid->active) {
		spin_unlock_bh(&rx_tid->tid_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "inactive rx tid %d for peer %pM\n",
			    rx_tid_ctx->tid, peer_addr);
		return -EINVAL;
	}

	ba_win_sz = rx_tid_ctx->ba_win_sz;
	if (!ba_win_sz)
		ba_win_sz = rx_tid->ba_win_sz;

	if (!ba_win_sz) {
		ath12k_dp_rx_peer_tid_ba_config(ab->dp, rx_tid_ctx->tid,
						&ba_win_sz, &ssn);
	}

	if (!rx_tid_ctx->ba_win_sz && ba_win_sz)
		ath12k_info(ab,
			    "SMD RX BA update fallback peer %pM tid %u ba_win_sz %u\n",
			    peer_addr, rx_tid_ctx->tid, ba_win_sz);

	if (!ath12k_wifi8_smd_skip_bitmap_update) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		ret = ath12k_wifi8_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
						   HAL_REO_CMD_FLUSH_CACHE,
						   &cmd, NULL);
		if (ret) {
			spin_unlock_bh(&rx_tid->tid_lock);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			ath12k_warn(ab, "Failed REO cache flush cmd for tid %d: %d\n",
				    rx_tid_ctx->tid, ret);
			return ret;
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.upd0 = HAL_REO_CMD_UPD0_SSN;
	cmd.upd2 = u32_encode_bits(rx_tid_ctx->ssn, HAL_REO_CMD_UPD2_SSN);

	if (ba_win_sz) {
		cmd.upd0 |= HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
		cmd.ba_window_size = min_t(u32, ba_win_sz,
					   DP_BA_WIN_SZ_MAX);
	}

	if (rx_tid_ctx->pn_len > 0) {
		if (rx_tid_ctx->pn_len > 6 &&
		    rx_tid_ctx->pn_127_48_info == 0) {
			/* Avoid pushing invalid 128-bit PN without MSBs. */
			goto send_cmd;
		}
		cmd.upd0 |= HAL_REO_CMD_UPD0_PN | HAL_REO_CMD_UPD0_PN_VALID |
			    HAL_REO_CMD_UPD0_PN_SIZE;
		cmd.pn[0] = rx_tid_ctx->pn_31_0;
		cmd.pn[1] = rx_tid_ctx->pn_47_32;
		cmd.pn_127_48_info = rx_tid_ctx->pn_127_48_info;
		cmd.pn_size = rx_tid_ctx->pn_len * 8;

		if (rx_tid_ctx->pn_len > 6) {
			cmd.pn[2] = 0;
			cmd.pn[3] = 0;
		}
	}

send_cmd:
	ret = ath12k_wifi8_dp_reo_cmd_send_highprio(ab, rx_tid,
						    sizeof(*rx_tid),
						    HAL_REO_CMD_UPDATE_RX_QUEUE,
						    &cmd, NULL);
	if (ret) {
		spin_unlock_bh(&rx_tid->tid_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "Failed REO update cmd for tid %d: %d\n",
			    rx_tid_ctx->tid, ret);
		return ret;
	}

	if (ba_win_sz)
		rx_tid->ba_win_sz = cmd.ba_window_size;

	if (ath12k_wifi8_smd_skip_bitmap_update)
		goto done;

	ret = ath12k_wifi8_hal_reo_qdesc_update_bitmaps_direct(ab, rx_tid,
							       rx_tid_ctx);
	if (ret) {
		spin_unlock_bh(&rx_tid->tid_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "Failed bitmap update for tid %d: %d\n",
			    rx_tid_ctx->tid, ret);
		return ret;
	}

done:
	spin_unlock_bh(&rx_tid->tid_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "SMD REO update done for peer %pM tid %d: SSN=0x%x\n",
			 peer_addr, rx_tid_ctx->tid, rx_tid_ctx->ssn);

	return 0;
}

/*
 * Clear the SVLD (start sequence valid) bit in the REO queue for all data
 * TIDs of a peer.  Only HAL_REO_CMD_UPD0_SVLD is set in upd0; upd2.SVLD is
 * left at 0 (from memset) so that hardware writes SVLD=0 to the queue,
 * signalling that no prior sequence window is valid.
 *
 * No SSN, BA-window, PN, or bitmap update is performed — this function is
 * the minimum required when request_dl_sn_not_transferred is set.
 */
int ath12k_wifi8_peer_rx_tid_svld_reset(struct ath12k_base *ab,
					struct ath12k_dp_hw *dp_hw,
					struct ieee80211_vif *vif,
					const u8 *peer_addr)
{
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_hal_reo_cmd cmd;
	struct ath12k_dp_peer *dp_peer;
	int ret = 0;
	u8 tid;

	if (!dp_hw || !peer_addr)
		return -EINVAL;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, (u8 *)peer_addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "SMD SVLD reset: peer %pM not found\n", peer_addr);
		return -ENOENT;
	}
	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		rx_tid = &dp_peer->rx_tid[tid];
		if (!rx_tid->active)
			continue;

		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "SMD SVLD reset: peer %pM tid %u REO SVLD -> 0\n",
				 peer_addr, tid);

		memset(&cmd, 0, sizeof(cmd));
		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		cmd.flag    = HAL_REO_CMD_FLG_NEED_STATUS;
		/* Update SVLD field only; upd2.SVLD = 0 clears it in the queue */
		cmd.upd0    = HAL_REO_CMD_UPD0_SVLD;

		ret = ath12k_wifi8_dp_reo_cmd_send_highprio(ab, rx_tid,
							    sizeof(*rx_tid),
							    HAL_REO_CMD_UPDATE_RX_QUEUE,
							    &cmd, NULL);
		if (ret)
			ath12k_warn(ab,
				    "SMD SVLD reset failed tid %d peer %pM: %d\n",
				    tid, peer_addr, ret);
	}
#define ATH12K_SMD_RX_MGMT_TID 16
	/* Management Rx TID (ATH12K_SMD_RX_MGMT_TID = 16) */
	rx_tid = &dp_peer->rx_tid[ATH12K_SMD_RX_MGMT_TID];
	if (rx_tid->active) {
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "SMD SVLD reset: peer %pM tid %u (mgmt) REO SVLD -> 0\n",
				 peer_addr, ATH12K_SMD_RX_MGMT_TID);

		memset(&cmd, 0, sizeof(cmd));
		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		cmd.flag    = HAL_REO_CMD_FLG_NEED_STATUS;
		cmd.upd0    = HAL_REO_CMD_UPD0_SVLD;

		ret = ath12k_wifi8_dp_reo_cmd_send_highprio(ab, rx_tid,
							    sizeof(*rx_tid),
							    HAL_REO_CMD_UPDATE_RX_QUEUE,
							    &cmd, NULL);
		if (ret)
			ath12k_warn(ab,
				    "SMD SVLD reset failed mgmt tid peer %pM: %d\n",
				    peer_addr, ret);
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "SMD SVLD reset done for peer %pM\n", peer_addr);

	return ret;
}

void ath12k_wifi8_peer_rx_tid_reo_clear_vld_cmd_init(struct ath12k_dp_rx_tid *rx_tid,
						     struct ath12k_hal_reo_cmd *cmd)
{
	memset(cmd, 0, sizeof(*cmd));
	cmd->addr_lo = lower_32_bits(rx_tid->paddr);
	cmd->addr_hi = upper_32_bits(rx_tid->paddr);

	cmd->flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd->upd0 = HAL_REO_CMD_UPD0_VLD;
	cmd->upd2 = HAL_REO_CMD_UPD2_FLUSH_FROM_CACHE;
}

/*
 * ath12k_wifi8_peer_rx_tid_reo_clear_vld - Clear the VLD bit in the REO queue
 * descriptor for a given peer TID via REO_UPDATE_RX_QUEUE command.
 *
 * Setting update_vld=1 and vld=0 causes REO to stop accepting frames into the
 * reorder queue and instead release them to the error path with error code
 * reo_queue_desc_not_valid (enum 1).  Per HW spec, flush_from_cache MUST also
 * be set whenever VLD is being cleared so that the stale descriptor is evicted
 * from the REO cache immediately.
 *
 * @ab:        ath12k_base
 * @dp_hw:     dp_hw context that owns the peer table
 * @peer_addr: MAC address of the peer
 * @tid:       TID whose REO queue VLD bit should be cleared
 *
 * Returns 0 on success, negative errno on failure.
 */
int ath12k_wifi8_peer_rx_tid_reo_clear_vld(struct ath12k_base *ab,
					   struct ath12k_dp_hw *dp_hw,
					   struct ieee80211_vif *vif,
					   const u8 *peer_addr,
					   u8 tid)
{
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_hal_reo_cmd cmd;
	struct ath12k_dp_peer *dp_peer;
	int ret;

	if (!dp_hw || !peer_addr) {
		ath12k_err(ab, "invalid args for REO VLD clear\n");
		return -EINVAL;
	}

	if (tid >= IEEE80211_NUM_TIDS) {
		ath12k_warn(ab, "invalid tid %d for REO VLD clear\n", tid);
		return -EINVAL;
	}

	lockdep_assert_held(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, (u8 *)peer_addr, vif);
	if (!dp_peer) {
		ath12k_warn(ab, "failed to find peer %pM for REO VLD clear\n",
			    peer_addr);
		return -ENOENT;
	}

	rx_tid = &dp_peer->rx_tid[tid];
	spin_lock_bh(&rx_tid->tid_lock);
	if (!rx_tid->active) {
		spin_unlock_bh(&rx_tid->tid_lock);
		ath12k_warn(ab, "inactive rx tid %d for peer %pM, skip VLD clear\n",
			    tid, peer_addr);
		return -EINVAL;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);

	/*
	 * HAL spec requires status so SW can confirm the command was executed.
	 * update_vld=1, vld=0: REO will reject incoming frames for this queue
	 * and forward them to the error path (reo_queue_desc_not_valid).
	 * flush_from_cache: mandatory when clearing VLD to evict the descriptor
	 * from REO cache so subsequent frames see VLD=0 immediately.
	 */
	ath12k_wifi8_peer_rx_tid_reo_clear_vld_cmd_init(rx_tid, &cmd);
	ret = ath12k_wifi8_dp_reo_cmd_send_highprio(ab, rx_tid,
						    sizeof(*rx_tid),
						    HAL_REO_CMD_UPDATE_RX_QUEUE,
						    &cmd, NULL);
	spin_unlock_bh(&rx_tid->tid_lock);

	if (ret) {
		ath12k_warn(ab, "failed REO VLD clear cmd for peer %pM tid %d: %d\n",
			    peer_addr, tid, ret);
		return ret;
	}

	ath12k_info(ab,
		    "REO VLD cleared for peer %pM tid %d, frames will be released to error path\n",
		    peer_addr, tid);

	return 0;
}

int ath12k_wifi8_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	int ret;

	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.upd0 = HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
	cmd.ba_window_size = ba_win_sz;

	if (update_ssn) {
		cmd.upd0 |= HAL_REO_CMD_UPD0_SSN;
		cmd.upd2 = u32_encode_bits(ssn, HAL_REO_CMD_UPD2_SSN);
	}

	ret = ath12k_wifi8_dp_reo_cmd_send(ar->ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_UPDATE_RX_QUEUE, &cmd,
					   NULL);
	if (ret) {
		ath12k_warn(ar->ab, "failed to update rx tid queue, tid %d (%d)\n",
			    rx_tid->tid, ret);
		return ret;
	}

	rx_tid->ba_win_sz = ba_win_sz;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "rx tid queue update: tid=%u peer_id=%d peer=%pM link=%u ba_win=%u\n",
			 rx_tid->tid, peer->peer_id, peer->addr,
			 peer->link_id, rx_tid->ba_win_sz);

	return 0;
}

static inline
void ath12k_wifi8_dp_rx_update_ppe_msdu_mark(struct ath12k_base *ab,
					     struct ath12k_dp_peer *peer,
					     struct sk_buff *msdu,
					     struct rx_mpdu_desc_info *rx_mpdu_info,
					     struct hal_rx_desc *rx_desc)
{
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
	if (peer->ppe_vp_num <= 0)
		return;

	ab->hal.hal_ops->rx_desc_get_fse_info(rx_desc, rx_mpdu_info);
	if (!rx_mpdu_info->flow_idx_timeout &&
	    !rx_mpdu_info->flow_idx_invalid &&
	    rx_mpdu_info->flow_info.flow_metadata &&
	    (rx_mpdu_info->flow_info.flow_metadata &
	    ATH12K_RX_FSE_FLOW_MATCH_USE_PPE))
		msdu->mark =
			u32_encode_bits(ATH12K_FSE_MAGIC_NUM,
					ATH12K_FSE_MAGIC_NUM_MASK) |
			u32_encode_bits(rx_mpdu_info->flow_info.flow_metadata,
					ATH12K_PPE_VP_NUM);
#endif
}

void ath12k_wifi8_dp_rx_h_undecap_nwifi(struct ath12k_pdev_dp *dp_pdev,
					struct sk_buff *msdu,
					enum hal_encrypt_type enctype,
					struct ieee80211_rx_status *status,
					struct hal_rx_desc *desc,
					bool mesh_ctrl_present, u16 qos_ctl)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u8 decap_hdr[DP_MAX_NWIFI_HDR_LEN];
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	u8 *crypto_hdr;
	int len;

	/* pull decapped header */
	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	skb_pull(msdu, hdr_len);

	/*  Rebuild qos header */
	hdr->frame_control |= __cpu_to_le16(IEEE80211_STYPE_QOS_DATA);

	/* Reset the order bit as the HT_Control header is stripped */
	hdr->frame_control &= ~(__cpu_to_le16(IEEE80211_FCTL_ORDER));

	if (mesh_ctrl_present)
		qos_ctl |= IEEE80211_QOS_CTL_MESH_CONTROL_PRESENT;

	/* TODO: Add other QoS ctl fields when required */

	/* copy decap header before overwriting for reuse below */
	memcpy(decap_hdr, hdr, hdr_len);

	/* Rebuild crypto header for mac80211 use */
	if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		len = ath12k_dp_rx_crypto_param_len(dp, enctype);
		crypto_hdr = skb_push(msdu, len);

		ath12k_wifi8_dp_rx_desc_get_crypto_header(ab, desc,
							  crypto_hdr, enctype);
	}

	memcpy(skb_push(msdu,
			IEEE80211_QOS_CTL_LEN), &qos_ctl,
			IEEE80211_QOS_CTL_LEN);
	memcpy(skb_push(msdu, hdr_len), decap_hdr, hdr_len);
}

static bool ath12k_wifi8_dp_rx_h_rate(struct ath12k_pdev_dp *dp_pdev,
				      struct ieee80211_rx_status *rx_status,
				      struct rx_tlv_info_1 *tlv_info)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ieee80211_supported_band *sband;
	enum rx_msdu_start_pkt_type pkt_type = tlv_info->pkt_type;
	u8 bw = tlv_info->bw, sgi = tlv_info->sgi;
	u8 rate_mcs = tlv_info->rate_mcs, nss = tlv_info->nss;
	bool is_cck;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		if (rx_status->band == NUM_NL80211_BANDS) {
			ath12k_warn(dp->ab, "Received with invalid band");
			return true;
		}
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		sband = &dp_pdev->ar->mac.sbands[rx_status->band];
		rx_status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
								is_cck);
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		rx_status->encoding = RX_ENC_HT;
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in HT mode %d\n",
				     rate_mcs);
			return true;
		}
		rx_status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		rx_status->encoding = RX_ENC_VHT;
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in VHT mode %d\n",
				     rate_mcs);
			return true;
		}
		rx_status->nss = nss;
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in HE mode %d\n",
				    rate_mcs);
			return true;
		}
		rx_status->encoding = RX_ENC_HE;
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		rx_status->rate_idx = rate_mcs;

		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in EHT mode %d\n",
				    rate_mcs);
			return true;
		}

		rx_status->encoding = RX_ENC_EHT;
		rx_status->nss = nss;
		rx_status->eht.gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11BN:
		rx_status->rate_idx = rate_mcs;

		if (rate_mcs > ATH12K_UHR_MCS_MAX) {
			ath12k_warn(dp->ab,
					"Received with invalid mcs in UHR mode %d\n",
					rate_mcs);
			return true;
		}

		rx_status->encoding = RX_ENC_UHR;
		rx_status->nss = nss;
		rx_status->eht.gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;

	default:
		break;
	}

	return false;
}

bool ath12k_wifi8_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src)
{
	u8 channel_num;
	u32 center_freq, meta_data;
	struct ieee80211_channel *channel;
	struct ath12k *ar = dp_pdev->ar;

	rx_status->freq = 0;
	rx_status->rate_idx = 0;
	rx_status->nss = 0;
	rx_status->encoding = RX_ENC_LEGACY;
	rx_status->bw = RATE_INFO_BW_20;
	rx_status->enc_flags = 0;

	rx_status->flag |= RX_FLAG_NO_SIGNAL_VAL;

	meta_data = tlv_info->freq;
	channel_num = meta_data;
	center_freq = meta_data >> 16;

	rx_status->band = NUM_NL80211_BANDS;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		rx_status->band = NL80211_BAND_6GHZ;
		rx_status->freq = center_freq;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		rx_status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		rx_status->band = NL80211_BAND_5GHZ;
	}

	if (unlikely(rx_status->band == NUM_NL80211_BANDS ||
		     !dp_pdev->hw->wiphy->bands[rx_status->band])) {
		if (err_rel_src == HAL_WBM_REL_SRC_MODULE_REO ||
		    err_rel_src == HAL_WBM_REL_SRC_MODULE_RXDMA) {
			if (ath12k_debug_critical)
				WARN_ON_ONCE(1);
			return true;
		} else {
			ath12k_err(ar->ab,
				   "sband is NULL for status band %d channel_num %d center_freq %d pdev_id %d\n",
				   rx_status->band, channel_num, center_freq,
				   ar->pdev_idx);
		}

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rx_status->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		} else {
			ath12k_err(ar->ab, "unable to determine channel, band for rx packet");
		}
		spin_unlock_bh(&ar->data_lock);

		rx_status->freq = ieee80211_channel_to_frequency(channel_num,
								 rx_status->band);
		goto h_rate;
	}

	if (rx_status->band != NL80211_BAND_6GHZ)
		rx_status->freq = ieee80211_channel_to_frequency(channel_num,
								 rx_status->band);

h_rate:
	return ath12k_wifi8_dp_rx_h_rate(dp_pdev, rx_status, tlv_info);
}

void
ath12k_wifi8_deliver_raw_frame(struct ath12k_pdev_dp *dp_pdev,
			       struct hal_rx_spd_data *rx_spd,
			       struct ath12k_dp_peer *peer,
			       struct ieee80211_rx_status *status,
			       struct napi_struct *napi,
			       struct link_peer_rx_tid_stats *stats,
			       struct rx_tlv_info_1 *prev_tlv_info)
{
	enum hal_encrypt_type enctype = HAL_ENCRYPT_TYPE_OPEN;
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_hal *hal;
	struct hal_rx_desc *rx_tlv_hdr;
	bool is_mcbc = false;
	bool ret, decrypted;
	bool ra_mcbc = rx_spd->rx_msdu_info.da_is_mcbc;
	u16 peer_id = ATH12K_PEER_ID_INVALID;

	if (peer)
		ra_mcbc = ra_mcbc && !peer->is_reset_mcbc;

	is_mcbc = is_ieee80211_frame_da_mcast(msdu);

	hal = dp_pdev->dp->hal;

	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;

	rx_tlv_hdr = (struct hal_rx_desc *)rx_spd->vaddr;

	status->flag &= ~(RX_FLAG_FAILED_FCS_CRC |
			RX_FLAG_MMIC_ERROR |
			RX_FLAG_DECRYPTED |
			RX_FLAG_IV_STRIPPED |
			RX_FLAG_MMIC_STRIPPED);

	if (!peer) {
		status->link_valid = 0;
	} else {
		pubsta = peer->sta;
		enctype = ra_mcbc ? peer->sec_type_grp : peer->sec_type;
		peer_id = peer->peer_id;
	}

	if (pubsta && pubsta->valid_links) {
		int link_id = rx_mpdu_info->src_link_id;

		status->link_valid = 1;
		status->link_id =
			ath12k_dp_peer_convert_hw_to_logical_link_id(peer, link_id);
	}

	msdu->priority = rx_mpdu_info->tid;

	ath12k_wifi8_dp_extract_rx_spd_data(hal, rx_spd, rx_tlv_hdr);

	decrypted = ath12k_hal_rx_h_is_decrypted(hal, rx_tlv_hdr);

	if (decrypted && enctype != HAL_ENCRYPT_TYPE_OPEN) {
		status->flag |= RX_FLAG_DECRYPTED | RX_FLAG_MMIC_STRIPPED;

		if (ra_mcbc)
			status->flag |= RX_FLAG_MIC_STRIPPED | RX_FLAG_ICV_STRIPPED;
		else
			status->flag |= RX_FLAG_IV_STRIPPED | RX_FLAG_PN_VALIDATED;
	} else {
		decrypted = false;
	}

	/* copy from scratch_pad to ieee80211_rx_status */
	tlv_info = &rx_spd->tlv_info;
	ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev, status, tlv_info,
					HAL_WBM_REL_SRC_MODULE_REO);
	if (ret) {
		dev_kfree_skb_any(msdu);
		return;
	}

	ath12k_dp_rx_h_undecap_raw(dp_pdev, msdu,
				   rx_tlv_hdr,
				   enctype,
				   status, decrypted, peer_id);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!is_mcbc) {
		stats->sent_to_stack_ucast++;
		stats->sent_to_stack_ucast_bytes += msdu->len;
	} else {
		stats->sent_to_stack_mcast++;
		stats->sent_to_stack_mcast_bytes += msdu->len;
	}

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
	return;
}

void
ath12k_wifi8_deliver_nwifi_frame(struct ath12k_pdev_dp *dp_pdev,
				 struct hal_rx_spd_data *rx_spd,
				 struct ath12k_dp_peer *peer,
				 struct ieee80211_rx_status *status,
				 struct napi_struct *napi,
				 struct link_peer_rx_tid_stats *stats,
				 struct rx_tlv_info_1 *prev_tlv_info)
{
	enum hal_encrypt_type enctype = HAL_ENCRYPT_TYPE_OPEN;
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_hal *hal;
	struct hal_rx_desc *rx_tlv_hdr;
	struct ieee80211_hdr *hdr;
	u32 hdr_len;
	bool ret;
	bool ra_mcbc = rx_spd->rx_msdu_info.da_is_mcbc;
	u16 peer_id = ATH12K_PEER_ID_INVALID;

	if (peer)
		ra_mcbc = ra_mcbc && !peer->is_reset_mcbc;

	/* ideally driver should not be doing this check.
	 * instead HW should flag this with error_code
	 * "rxdma_msdu_len_err" and release it via WBM
	 * release ring, based on this error code driver
	 * should drop the frame.
	 */
	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	if ((likely(hdr_len > DP_MAX_NWIFI_HDR_LEN)))
		WARN_ON(1);

	hal = dp_pdev->dp->hal;

	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;

	rx_tlv_hdr = (struct hal_rx_desc *)rx_spd->vaddr;

	status->flag |= RX_FLAG_DECRYPTED |
			RX_FLAG_MMIC_STRIPPED |
			RX_FLAG_IV_STRIPPED |
			RX_FLAG_PN_VALIDATED |
			RX_FLAG_SKIP_MONITOR |
			RX_FLAG_DUP_VALIDATED;

	if (!peer) {
		status->link_valid = 0;
	} else {
		pubsta = peer->sta;
		enctype = ra_mcbc ? peer->sec_type_grp : peer->sec_type;
		peer_id = peer->peer_id;
	}

	if (pubsta && pubsta->valid_links) {
		int link_id = rx_mpdu_info->src_link_id;

		status->link_valid = 1;
		status->link_id =
			ath12k_dp_peer_convert_hw_to_logical_link_id(peer, link_id);
	}

	msdu->priority = rx_mpdu_info->tid;

	ath12k_wifi8_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	ath12k_wifi8_dp_extract_rx_spd_data(hal, rx_spd,
					    rx_tlv_hdr);

	/* copy from scratch_pad to ieee80211_rx_status */
	tlv_info = &rx_spd->tlv_info;
	ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev, status, tlv_info,
					HAL_WBM_REL_SRC_MODULE_REO);
	if (ret) {
		dev_kfree_skb_any(msdu);
		return;
	}

	ath12k_wifi8_dp_rx_h_undecap_nwifi(dp_pdev, msdu, enctype,
					   status,
					   rx_tlv_hdr,
					   rx_spd->tlv_info.mesh_ctrl_present,
					   rx_mpdu_info->tid);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!rx_msdu_info->da_is_mcbc) {
		stats->sent_to_stack_ucast++;
		stats->sent_to_stack_ucast_bytes += msdu->len;
	} else {
		stats->sent_to_stack_mcast++;
		stats->sent_to_stack_mcast_bytes += msdu->len;
	}

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
}

#ifndef CPTCFG_QCN_EXTN
void
ath12k_wifi8_deliver_ethernet_frame(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_spd_data *rx_spd,
				    struct ath12k_dp_peer *peer,
				    struct ieee80211_rx_status *status,
				    struct napi_struct *napi,
				    struct link_peer_rx_tid_stats *stats,
				    struct rx_tlv_info_1 *prev_tlv_info)
{
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_rx_desc *rx_tlv_hdr;
	u8 tid;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_hal *hal = dp_pdev->dp->hal;
	bool ret;

	rx_tlv_hdr = (struct hal_rx_desc *)rx_spd->vaddr;
	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;
	tid = rx_mpdu_info->tid;

	/* set checksum pass or fail only in parent skb */
	ath12k_wifi8_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	msdu->dev = peer->dev;

	/* ieee80211_rx_status object is reset when ever there is a
	 * peer change or radio change, if so extract radio params from
	 * the current MSDUs TLV headers and set the status info
	 * accordingly.
	 * Note: Radio params (ie: band, nss, sgi etc...) remain same
	 */
	ath12k_wifi8_dp_extract_rx_spd_data(hal, rx_spd, rx_tlv_hdr);

	tlv_info = &rx_spd->tlv_info;

	if (unlikely(!ath12k_wifi8_compare_tlv_info(prev_tlv_info,
						    tlv_info))) {
		ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev,
						status,
						tlv_info,
						HAL_WBM_REL_SRC_MODULE_REO);

		if (ret) {
			dev_kfree_skb_any(msdu);
			return;
		}

		status->flag |= RX_FLAG_8023 |
				RX_FLAG_DECRYPTED |
				RX_FLAG_MMIC_STRIPPED |
				RX_FLAG_IV_STRIPPED |
				RX_FLAG_PN_VALIDATED |
				RX_FLAG_SKIP_MONITOR |
				RX_FLAG_DUP_VALIDATED;
		ath12k_wifi8_copy_tlv_info(prev_tlv_info, tlv_info);
	}

	pubsta = peer->sta;
	if (pubsta && pubsta->valid_links) {
		int link_id = rx_mpdu_info->src_link_id;

		status->link_valid = 1;
		status->link_id =
			ath12k_dp_peer_convert_hw_to_logical_link_id(peer, link_id);
	}

	msdu->priority = rx_mpdu_info->tid;

	/* Convert 802.3 frame to 802.11 so mac80211 can handle it correctly.
	 * For 4addr frames, this is needed to support APVLAN interface creation.
	 * For TKIP peers, fast-rx is not set in mac80211, so RX_FLAG_8023 must
	 * be cleared and the frame converted to route through normal RX handlers.
	 */
	if ((!peer->use_4addr &&
	     (rx_spd->rx_msdu_info.fr_ds && rx_spd->rx_msdu_info.to_ds)) ||
	      peer->sec_type == HAL_ENCRYPT_TYPE_TKIP_MIC) {
		ath12k_wifi8_convert_eth_2_80211_frame(dp, rx_spd);
		status->flag &= ~RX_FLAG_8023;
	}

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (rx_msdu_info->da_is_mcbc) {
		stats->sent_to_stack_mcast++;
		stats->sent_to_stack_mcast_bytes += msdu->len;
	} else {
		stats->sent_to_stack_ucast++;
		stats->sent_to_stack_ucast_bytes += msdu->len;
	}

	dp->device_stats.non_fast_unicast_rx[rx_spd->ring_id][dp->device_id]++;
	prefetch(skb_shinfo(msdu));

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);

	return;
}
#endif

static inline u8
ath12k_wifi8_rx_create_fraglist(struct hal_rx_spd_data *spd_desc,
				u32 rx_tlv_sz)
{
	struct hal_rx_spd_data *spd_desc_orig = spd_desc;
	struct sk_buff *parent = NULL;
	struct sk_buff *frag_list = NULL;
	struct sk_buff *tmp = NULL;
	struct rx_msdu_desc_info *rx_msdu_info;
	u16 msdu_len;
	u8 l3_pad_bytes;
	u8 idx = 0;
	u16 frag_list_len = 0;
	u16 buf_size = DP_RX_BUFFER_SIZE;

	rx_msdu_info = &spd_desc->rx_msdu_info;

	msdu_len = rx_msdu_info->msdu_length;
	l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;

	parent = spd_desc->msdu;

	skb_put(parent, buf_size);
	skb_pull(parent, rx_tlv_sz + l3_pad_bytes);

	msdu_len -= parent->len;

	/* set checksum pass or fail only in parent skb */
	ath12k_wifi8_dp_rx_h_csum_offload(parent, rx_msdu_info);

	spd_desc->first_sg_frame = 0;
	do {
		spd_desc++;

		if (!frag_list) {
			frag_list = spd_desc->msdu;
			tmp = frag_list;
		} else {
			tmp->next = spd_desc->msdu;
			tmp = tmp->next;
		}

		if (msdu_len + rx_tlv_sz > buf_size) {
			skb_put(tmp, buf_size);
			msdu_len -= (buf_size - rx_tlv_sz);
		} else {
			skb_put(tmp, msdu_len + rx_tlv_sz);
			msdu_len = 0;
		}
		skb_pull(tmp, rx_tlv_sz);
		frag_list_len += tmp->len;
		idx++;
	} while (!spd_desc->last_sg_frame);
	spd_desc->last_sg_frame = 0;

	skb_shinfo(parent)->frag_list = frag_list;
	parent->data_len = 0;
	parent->data_len += frag_list_len;
	parent->len += frag_list_len;

	/* save the vaddr in the first scratch_pad desc
	 * since the last spad->vaddr (TLV_HDR) of the SG frame
	 * holds proper radio params and these parameters
	 * are needed to fill ieee80211_rx_status based on
	 * DECAP type.
	 */
	spd_desc_orig->vaddr = spd_desc->vaddr;
	return idx;
}

void ath12k_wifi8_dp_adjust_skb(struct hal_rx_spd_data *spd_desc_l,
				struct link_peer_rx_tid_stats *stats,
				int *msdu_idx, u32 hal_rx_desc_sz)
{
	struct sk_buff *msdu = spd_desc_l->msdu;
	struct rx_msdu_desc_info *rx_msdu_info = &spd_desc_l->rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info = &spd_desc_l->rx_mpdu_info;
	int l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;
	int msdu_len = rx_msdu_info->msdu_length;
	int idx;

	if (unlikely(rx_mpdu_info->fragment_flag))
		/* an ieee80211 rx fragmented MPDU is handled in
		 * exception path, only TLV need to pulled from skb
		 */
		skb_pull(msdu, hal_rx_desc_sz);
	else if (unlikely(spd_desc_l->first_sg_frame)) {
		/* create a frag_list for MSDUs which are spread across
		 * multiple buffers/skbs. pulling of TLV header and
		 * setting of length is done in below API.
		 */
		idx = ath12k_wifi8_rx_create_fraglist(spd_desc_l,
						      hal_rx_desc_sz);
		*msdu_idx += idx;
		if (stats) {
			stats->sg_cnt++;
			stats->sg_bytes++;
		}
	} else {
		/* this is the most likely case, a regular MSDU */
		skb_put(msdu, hal_rx_desc_sz + l3_pad_bytes + msdu_len);
		skb_pull(msdu, hal_rx_desc_sz + l3_pad_bytes);
	}
}

static void
ath12k_wifi8_dp_process_reo_rx_packets(struct ath12k_dp *dp,
				       struct napi_struct *napi,
				       struct hal_rx_spd_data *rx_spd,
				       int ring_id, int num_msdus)
{
	struct ath12k_tid_rx_stats *tid_rx_stats_ring = NULL;
	struct ieee80211_rx_status rx_status = {0};
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 prev_tlv_info = {0};
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct hal_rx_desc *rx_tlv_hdr;
	struct sk_buff *msdu;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	struct ath12k_base *partner_ab;
	struct ath12k_dp *partner_dp;
	u8 hw_link_id, pdev_id;
	u8 prev_hw_link_id = 0xff;
	int msdu_idx = 0;
	struct ath12k_hal *hal = dp->hal;
	u32 hal_rx_desc_sz = hal->hal_desc_sz;
	u16 msdu_len;
	u8 l3_pad_bytes;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	u16 peer_id = 0;
	u16 old_peer_id = 0xffff;
	struct link_peer_rx_tid_stats tid_stats[8] = {0};
	struct link_peer_rx_tid_stats *stats;
	u8 tid = 0;
	u8 active_tid_mask = 0;
	bool is_delay_enabled = false;

	rcu_read_lock();

	for (msdu_idx = 0; msdu_idx < num_msdus; msdu_idx++) {
		u8 *vaddr;
		struct hal_rx_spd_data *spd_desc_l = &rx_spd[msdu_idx];

		rx_msdu_info = &spd_desc_l->rx_msdu_info;
		rx_mpdu_info = &spd_desc_l->rx_mpdu_info;
		msdu = spd_desc_l->msdu;
		vaddr = spd_desc_l->vaddr;

		prefetch(&vaddr[128]);

		if (likely(msdu_idx + 1 < num_msdus)) {
			struct hal_rx_spd_data *spd_desc_next = &rx_spd[msdu_idx + 1];
			struct sk_buff *next_msdu = spd_desc_next->msdu;

			prefetch(next_msdu);
			prefetchw(&next_msdu->len);
			prefetchw(&next_msdu->protocol);
			prefetchw(&next_msdu->data);
		}

		hw_link_id = ath12k_dp_validate_hw_link_id(rx_mpdu_info->src_link_id);
		tid = rx_mpdu_info->tid;

		rx_mpdu_info->flow_info.peer_id =
			ath12k_wifi8_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
						       rx_mpdu_info->peer_meta_data);

		peer_id = rx_mpdu_info->flow_info.peer_id;

		if (peer)
			prefetch(&peer->rx_decap_type);

		/*
		 * access dp_pdev object only if there is a miss-match
		 * between old hw_link_id and current hw_link_id
		 * this ensures minimum cache misses
		 */
		if (unlikely(prev_hw_link_id != hw_link_id)) {
			int device_id, pdev_idx;
			struct ath12k_pdev *pdev_active;

			if (msdu_idx != 0 && peer && dp_pdev)
				ath12k_dp_rx_update_stats(dp_pdev, peer,
							  tid_stats,
							  ring_id,
							  prev_hw_link_id,
							  active_tid_mask);
			prev_hw_link_id = hw_link_id;
			memset(tid_stats, 0, sizeof(tid_stats));
			active_tid_mask = 0;

			device_id = hw_links[hw_link_id].device_id;
			pdev_idx = hw_links[hw_link_id].pdev_idx;

			partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
			pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
							      pdev_idx);

			dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
			if (unlikely(!dp_pdev)) {
				ath12k_dp_rx_skb_free(msdu, dp, ring_id,
						      DP_RX_ERR_DROP_PDEV_NA,
						      NULL, tid);
				spd_desc_l->msdu = NULL;
				prev_hw_link_id = 0xff;
				continue;
			}

			is_delay_enabled = ath12k_dp_latency_stats_enabled(dp_pdev);
			tid_rx_stats_ring = &dp_pdev->tid_stats.tid_rx[ring_id][0];

			partner_ab = partner_dp->ab;
			pdev_active = partner_ab->pdevs_active[pdev_id];

			if (unlikely(!rcu_dereference(pdev_active))) {
				ath12k_dp_rx_skb_free(msdu, dp, ring_id,
						      DP_RX_ERR_DROP_PDEV_NA,
						      NULL, tid);
				spd_desc_l->msdu = NULL;
				prev_hw_link_id = 0xff;
				continue;
			}
		}

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev) &&
			     (ath12k_dp_vow_stats_enabled(dp_pdev) || is_delay_enabled)))
			__net_timestamp(msdu);

		prefetch(&partner_dp->hal);
		/*
		 * access dp_peer object only if there is a miss-match
		 * between old peer_id and current peer_id
		 * this ensures minimum cache misses
		 */
		if (unlikely(old_peer_id != peer_id)) {
			old_peer_id = peer_id;
			if (peer)
				ath12k_dp_rx_update_stats(dp_pdev, peer,
							  tid_stats,
							  ring_id, hw_link_id,
							  active_tid_mask);

			peer = ath12k_dp_peer_find_by_peerid_index(partner_dp,
								   dp_pdev,
								   peer_id);
			if (unlikely(!peer)) {
				ath12k_dp_rx_skb_free(msdu, dp, ring_id,
						      DP_RX_ERR_DROP_INV_PEER,
						      dp_pdev, tid);
				spd_desc_l->msdu = NULL;
				old_peer_id = 0xffff;
				continue;
			}

			memset(tid_stats, 0, sizeof(tid_stats));
			active_tid_mask = 0;
		}

		/* stats should be collected only after the below assignment */
		active_tid_mask |= 1 << tid;
		stats = &tid_stats[tid];

		stats->received_frm_reo_cnt++;
		stats->received_frm_reo_bytes += rx_msdu_info->msdu_length;
		/*
		 * pull the TLV header + padding bytes and set the length of
		 * the skb accordingly. this is needed irrespective of
		 * decap type.
		 */
		msdu_len = rx_msdu_info->msdu_length;
		l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;

		ath12k_wifi8_dp_adjust_skb(spd_desc_l, stats,
					   &msdu_idx, hal_rx_desc_sz);

		rx_tlv_hdr = (struct hal_rx_desc *)spd_desc_l->vaddr;

		/* beyond this point RX TLV info could be over-written by
		 * user-specific meta data, hence copy all the nessacary info
		 * ex: flow_valid bit, flow_meta_info, BW, NSS etc...
		 * to scratchpad descriptor.
		 */
		ath12k_wifi8_dp_extract_rx_spd_data(dp_pdev->dp->hal,
						    spd_desc_l,
						    rx_tlv_hdr);

		{
			struct dp_rx_tag_params tag = {
				.cce_match        = spd_desc_l->cce_match,
				.cce_metadata     = spd_desc_l->cce_metadata,
				.flow_idx_invalid = rx_mpdu_info->flow_idx_invalid,
				.flow_idx_timeout = rx_mpdu_info->flow_idx_timeout,
				.flow_metadata    = rx_mpdu_info->flow_info.flow_metadata,
			};
			if (tag.cce_match && dp_pdev->protocol_tag_active_count)
				dp_rx_update_protocol_tag(dp_pdev, msdu, &tag);
			dp_rx_update_flow_tag(dp_pdev, msdu, &tag);
		}

		if (likely(msdu_idx + 1 < num_msdus)) {
			struct hal_rx_spd_data *spd_desc_next = &rx_spd[msdu_idx + 1];

			vaddr = spd_desc_next->vaddr;
			prefetch(vaddr);
			prefetch(&vaddr[64]);
			prefetch(&vaddr[128]);
		}

		switch (peer->rx_decap_type) {
		case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
			if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
				if (ath12k_proto_stats_enabled(dp_pdev))
					ath12k_dp_rx_update_protocol_stats(peer,
									   hw_link_id, msdu,
									   RX_SENT_TO_STACK,
									   ring_id);

				if (ath12k_dp_vow_stats_enabled(dp_pdev)) {
					bool mcbc = rx_msdu_info->da_is_mcbc;

					ath12k_dp_rx_update_vow_delay_stats(dp_pdev, stats,
									    msdu, mcbc, tid,
									    tid_rx_stats_ring);
				}

				if (is_delay_enabled)
					ath12k_dp_rx_update_delay_stats(peer, msdu,
									tid, ring_id);
			}

			ath12k_wifi8_deliver_ethernet_frame(dp_pdev, spd_desc_l,
							    peer, &rx_status,
							    napi, stats,
							    &prev_tlv_info);
			break;
		case DP_RX_DECAP_TYPE_NATIVE_WIFI:
			ath12k_wifi8_deliver_nwifi_frame(dp_pdev, spd_desc_l,
							 peer, &rx_status,
							 napi, stats,
							 &prev_tlv_info);
			break;
		case DP_RX_DECAP_TYPE_RAW:
			ath12k_wifi8_deliver_raw_frame(dp_pdev, spd_desc_l,
						       peer, &rx_status,
						       napi, stats,
						       &prev_tlv_info);
			break;
		default:
			pr_err("Unexpected decap type %d", peer->rx_decap_type);
			WARN_ON(1);
		}

		if (rx_msdu_info->first_msdu & rx_msdu_info->last_msdu)
			stats->non_amsdu++;
		else
			stats->amsdu++;

		if (rx_msdu_info->last_msdu & rx_mpdu_info->mpdu_retry_bit)
			stats->mpdu_retry++;
	}

	if (peer && dp_pdev)
		ath12k_dp_rx_update_stats(dp_pdev, peer, tid_stats,
					  ring_id, hw_link_id,
					  active_tid_mask);

	rcu_read_unlock();
}

static
void ath12k_wifi8_rx_sw_desc_sanity_check(struct hal_reo_dest_ring *hw_rx_desc,
					  struct ath12k_rx_desc_info *sw_desc,
					  struct hal_rx_spd_data *rx_spd)
{
	if (unlikely(!sw_desc)) {
		struct rx_mpdu_desc_info *rx_mpdu_info = &rx_spd->rx_mpdu_info;

		if (rx_mpdu_info->reo_dest_buffer_type ==
				HAL_REO_DEST_RING_BUFFER_TYPE_LINK_DESC) {
			rx_spd->buf_addr = hw_rx_desc->buf_addr_info;
			return;
		}

		pr_err_ratelimited("SPT lookup failed for rx desc (CC globally disabled), dropping frame");
		return;
	}

	if (unlikely(sw_desc->magic != ATH12K_DP_RX_DESC_MAGIC)) {
		pr_err("Check HW CC implementation, vaddr = 0x%p, mgmt_pkt = %lu, bar_frame = %lu",
			(void *)sw_desc->vaddr,
			FIELD_GET(HAL_RX_MPDU_EXT_DESC_INFO_INFO0_MGMT_PKT,
				  hw_rx_desc->rx_mpdu_ext_info.info0),
			FIELD_GET(HAL_RX_MPDU_DESC_INFO_INFO0_BAR_FRAME_FLAG,
				  hw_rx_desc->rx_mpdu_info.info0));
		WARN_ON(1);
	}

	if (unlikely(!sw_desc->in_use)) {
		pr_err("The SW descriptor is in free pool (!in_use), yet HW released it to host, vaddr = 0x%p, mgmt_pkt = %lu, bar_frame = %lu",
			(void *)sw_desc->vaddr,
			FIELD_GET(HAL_RX_MPDU_EXT_DESC_INFO_INFO0_MGMT_PKT,
				  hw_rx_desc->rx_mpdu_ext_info.info0),
			FIELD_GET(HAL_RX_MPDU_DESC_INFO_INFO0_BAR_FRAME_FLAG,
				  hw_rx_desc->rx_mpdu_info.info0));
		WARN_ON(1);
	}
}

static bool check_sg_termination(struct hal_srng *srng,
				 int valid_entries)
{
	struct hal_reo_dest_ring *desc;
	struct hal_rx_msdu_desc *msdu_info;

	if (valid_entries >= 9)
		return true;

	if (!valid_entries)
		return false;

	desc = (struct hal_reo_dest_ring *)
		ath12k_hal_srng_fetch_entry(srng, valid_entries - 1);
	msdu_info = &desc->rx_msdu_info;
	return !(le32_to_cpu(msdu_info->info0) & HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_CONTINUATION);
}

int ath12k_wifi8_dp_rx_process_reo_rings(struct ath12k_dp *dp,
					 struct hal_srng *srng,
					 struct hal_rx_spd_data *rx_status_desc,
					 int ring_id, int budget, int cpu_id)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct hal_reo_dest_ring *hw_rx_desc = NULL;
	struct hal_rx_spd_data *tmp_spd = NULL;
	struct hal_reo_dest_ring *next_hw_rx_desc = NULL;
	struct hal_reo_dest_ring *pf_next_hw_rx_desc = NULL;
	int total_msdu_reaped = 0;
	struct hal_srng *refill_srng;
	int valid_entries;
	u32 curr_tp = 0;
	struct ath12k_rx_desc_info *sw_rx_desc = NULL;
	struct list_head rx_desc_used_list;
	struct list_head ppe2wbm_used_list;
	bool first_sg_frame = true;

	INIT_LIST_HEAD(&rx_desc_used_list);
	INIT_LIST_HEAD(&ppe2wbm_used_list);

	__ath12k_hal_srng_access_begin(srng);

	valid_entries = __ath12k_hal_srng_dst_num_available_to_reap(srng,
								    false);
	if (unlikely(!valid_entries)) {
		__ath12k_hal_srng_access_end(ab, srng);
		return 0;
	}

	if (valid_entries > budget)
		valid_entries = budget;

	ath12k_dp_srng_dst_invalidate_entries(dp, srng, valid_entries);

	/* This loop will reap the HW desc from the reo ring and copy
	 * the contents of the HW desc to scratch-pad (spad) desc.
	 * get the corresponding sw_desc desc and save the skb in
	 * spad desc.
	 */
	while (valid_entries) {
		struct hal_rx_spd_data *rx_spd = &rx_status_desc[total_msdu_reaped];

		/* reset all the flags and msdu pointer before using scratch_pad desc */
		rx_spd->flags = 0;
		rx_spd->msdu = NULL;
		rx_spd->vaddr = NULL;
		hw_rx_desc =
			__ath12k_hal_get_dst_srng_desc(srng, &curr_tp,
						       (void **)&next_hw_rx_desc);

		if (unlikely(!hw_rx_desc)) {
			pr_err("HW bug: NULL entry in ring, invalid entry");
			WARN_ON(1);
		}

		ath12k_wifi8_cpy_hw_rx_desc_to_spad_desc(hw_rx_desc, rx_spd);
		sw_rx_desc = ath12k_wifi8_get_sw_desc_from_hw_desc(dp, hw_rx_desc);
		ath12k_wifi8_rx_sw_desc_sanity_check(hw_rx_desc, sw_rx_desc,
						     rx_spd);

		if (unlikely(!sw_rx_desc)) {
			valid_entries--;
			total_msdu_reaped++;
			continue;
		}

		rx_spd->rx_mpdu_info.fragment_flag = sw_rx_desc->is_frag;
		sw_rx_desc->is_frag = 0;

		if (pf_next_hw_rx_desc)
			ath12k_wifi8_pretech_next_sw_desc(dp, pf_next_hw_rx_desc);

		pf_next_hw_rx_desc = next_hw_rx_desc;

		valid_entries--;

		/* Scatter-gather (SG) frame reap logic
		 * This is a case where an MSDU is spread across
		 * multiple buffer. The continuation bit in
		 * HW descriptor indicates the current MSDU
		 * is spread across multiple buffers
		 */
		if (unlikely(rx_spd->rx_msdu_info.msdu_continuation)) {
			/* if this is the first SG frame and if the
			 * number of valid entries remaining to be reaped
			 * is less than 8 in this NAPI context, update the
			 * curr_tp as ring's tp and break. this mpdu/msdu
			 * will be reaped  in next NAPI poll context.
			 * Note: reason for why at least 8 valid entries are
			 *       needed to reap a SG MPDU and MSDU.
			 *       MAX MPDU size = 11454 buffer size = 1536
			 *       hence an MPDU at best will need 8 buffers
			 *       MAX MSDU size = 2304, buffer size = 1536
			 *       hence an MSDU at best will need 2 buffers.
			 */
			if (first_sg_frame) {
				if (!check_sg_termination(srng, valid_entries)) {
					__ath12k_hal_srng_update_tp(srng,
								    curr_tp);
					break;
				}
				tmp_spd = rx_spd;
				first_sg_frame = false;
				rx_spd->first_sg_frame = true;
			}
		} else {
			/* NON SG frame handling :
			 * if the previous reaped hw desc has continuation bit
			 * set, then the current reaped HW desc is the last SG
			 * frame of the MSDU, set the state accordingly
			 */
			if (unlikely(!first_sg_frame)) {
				rx_spd->last_sg_frame = true;
				first_sg_frame = true;
				tmp_spd->rx_msdu_info.msdu_length =
					rx_spd->rx_msdu_info.msdu_length;
			}
		}

		if (sw_rx_desc->is_ppe_desc)
			IPA_SET_RX_BUF_SMMU_UNMAP(ab, sw_rx_desc->skb, false);

		ath12k_dp_rx_buffer_unmap(dp, sw_rx_desc);
		rx_spd->msdu = sw_rx_desc->skb;
		rx_spd->vaddr = sw_rx_desc->vaddr;
		rx_spd->ring_id = ring_id;
		sw_rx_desc->in_use = 0;

		total_msdu_reaped++;
		list_add_tail(&sw_rx_desc->list, sw_rx_desc->is_ppe_desc ?
			      &ppe2wbm_used_list : &rx_desc_used_list);
	}

	ath12k_dsb();

	__ath12k_hal_srng_access_end(ab, srng);

	if (!list_empty(&ppe2wbm_used_list)) {
		if (dp_wifi8->dp_ppe2wbm_use_dedicated_pool) {
#ifdef CPTCFG_EXT_IPA_OFFLOAD
			ring_id = dp_wifi8->extn.ipa->ipa2wbm_ring
				[ATH12K_DP_WIFI8_IPA2WBM_HOST].ring_id;
#else
			ring_id = dp_wifi8->ppe2wbm_refill_ring
				[PPE2WBM_SW_REFILL_RING].ring_id;
#endif
			refill_srng = &ab->hal.srng_list[ring_id];
			ath12k_dp_rx_bufs_replenish(dp, refill_srng,
						    &ppe2wbm_used_list, false);
		} else {
			list_splice(&ppe2wbm_used_list, &rx_desc_used_list);
		}
	}

	if (!list_empty(&rx_desc_used_list)) {
		refill_srng = &ab->hal.srng_list[dp_wifi8->wbm_refill_ring[cpu_id %
			DP_WBM_REFILL_RING_MAX].ring_id];
		ath12k_dp_rx_bufs_replenish(dp, refill_srng, &rx_desc_used_list, false);
	}

	return total_msdu_reaped;
}

int ath12k_wifi8_dp_rx_process(struct ath12k_dp *dp, int ring_id,
			       struct napi_struct *napi, int budget)
{
	int cpu_id = smp_processor_id();
	struct hal_srng *srng = &dp->hal->srng_list[dp->reo_dst_ring[ring_id].ring_id];
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct hal_rx_spd_data *rx_status_desc =
		(struct hal_rx_spd_data *)dp_hw_grp->rx_status_buf[cpu_id];
	int total_msdu_reaped =
		ath12k_wifi8_dp_rx_process_reo_rings(dp, srng, rx_status_desc,
						     ring_id, budget, cpu_id);

	dp->device_stats.reo_rx[ring_id][dp->device_id] += total_msdu_reaped;

	if (total_msdu_reaped)
		ath12k_wifi8_dp_process_reo_rx_packets(dp, napi, rx_status_desc,
						       ring_id, total_msdu_reaped);
	return total_msdu_reaped;
}

int ath12k_wifi8_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned,
				    u16 stats_id)
{
	u8 tid = rx_tid->tid;
	u32 ba_win_sz = rx_tid->ba_win_sz;
	void *vaddr;
	u32 hw_desc_sz;
	dma_addr_t paddr;
	int __maybe_unused ret;

	/* TODO: Optimize the memory allocation for qos tid based on
	 * the actual BA window size in REO tid update path.
	 */
	if (ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid))
		hw_desc_sz = ath12k_wifi8_hal_reo_qdesc_size(ba_win_sz, tid);
	else
		hw_desc_sz = ath12k_wifi8_hal_reo_qdesc_size(DP_BA_WIN_SZ_MAX, tid);

	if (!ab->hw_params->alloc_cacheable_memory) {
		vaddr = dma_alloc_coherent(ab->dev, hw_desc_sz, &paddr, GFP_ATOMIC);
		if (!vaddr)
			return -ENOMEM;

		*addr_aligned = vaddr;
		ath12k_wifi8_hal_reo_qdesc_setup(*addr_aligned, tid, ba_win_sz, ssn,
						 pn_type, stats_id);
		rx_tid->vaddr = vaddr;
		rx_tid->paddr = paddr;
		rx_tid->size  = hw_desc_sz;

		ath12k_dbg(ab, ATH12K_DBG_DP_RX,
			   "REO qdesc alloc (SDX coherent): vaddr=%p paddr=%pad size=%u tid=%u\n",
			   vaddr, &paddr, hw_desc_sz, tid);
		return 0;
	}

	vaddr = kzalloc(hw_desc_sz + HAL_LINK_DESC_ALIGN - 1, GFP_ATOMIC);
	if (!vaddr)
		return -ENOMEM;

	*addr_aligned = PTR_ALIGN(vaddr, HAL_LINK_DESC_ALIGN);
	ath12k_wifi8_hal_reo_qdesc_setup(*addr_aligned, tid, ba_win_sz, ssn,
					 pn_type, stats_id);
#ifndef CONFIG_IO_COHERENCY
	paddr = dma_map_single(ab->dev, *addr_aligned, hw_desc_sz,
			       DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(ab->dev, paddr);
	if (ret) {
		ath12k_warn(ab, "failed to DMA-map REO qdesc tid %u: %d\n",
			    tid, ret);
		kfree(vaddr);
		return ret;
	}
#else
	paddr = virt_to_phys(*addr_aligned);
	if (!paddr) {
		ath12k_warn(ab, "virt_to_phys failed for REO qdesc tid %u\n",
			    tid);
		kfree(vaddr);
		return -ENOMEM;
	}
#endif /* CONFIG_IO_COHERENCY */
	rx_tid->vaddr = vaddr;
	rx_tid->paddr = paddr;
	rx_tid->size  = hw_desc_sz;

	ath12k_dbg(ab, ATH12K_DBG_DP_RX,
		   "REO qdesc alloc: vaddr=%p paddr=%pad size=%u tid=%u\n",
		   vaddr, &paddr, hw_desc_sz, tid);
	return 0;
}

static int ath12k_wifi8_dp_rx_wbm_idle_buf_0_config_qcn9625(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
	struct ath12k_dp_wifi8 *central_dp_wifi8;
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	if (!central_dp) {
		ath12k_err(ab, "central dp is NULL, skip WBM idle buf ring sel config\n");
		return -EINVAL;
	}

	central_dp_wifi8 = ath12k_get_dp_wifi8(central_dp);
	if (!central_dp_wifi8) {
		ath12k_err(ab, "central dp wifi8 context is NULL, skip WBM idle buf ring sel config\n");
		return -EINVAL;
	}

	ring_id = central_dp_wifi8->wbm_idle_buf_ring.ring_id;
	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
				    FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_mpdu_start_offset = ab->hal.rx_mpdu_start_offset;
	tlv_filter.rx_msdu_end_offset = ab->hal.rx_msdu_end_offset;

	tlv_filter.rx_mpdu_start_wmask = ab->hal.rx_mpdu_start_wmask;
	tlv_filter.rx_msdu_end_wmask = ab->hal.rx_msdu_end_wmask;

	tlv_filter.rdi_based_source_cfg =
			ath12k_wifi8_hal_get_rdi_source_cfg(ab, SOURCE_RING_CTRL_SFE);

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "Configuring compact tlv masks rx_mpdu_start_wmask 0x%x rx_msdu_end_wmask 0x%x\n",
		   tlv_filter.rx_mpdu_start_wmask, tlv_filter.rx_msdu_end_wmask);

	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
					       HAL_WBM_IDLE_BUF,
					       DP_RX_BUFFER_SIZE,
					       ATH12K_PKTLOG_DISABLED,
					       &tlv_filter);

	return ret;
}

static int ath12k_wifi8_dp_rx_ppe2wbm_idle_buf_config_qcn9625(struct ath12k_base *ab)
{
	/*
	 * PPE idle buf 1 pool configuration
	 */
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	/*
	 * When no dedicated PPE2WBM pool is configured, source ring
	 * is selected from the SFE pool.
	 */
	if (!dp_wifi8->dp_ppe2wbm_use_dedicated_pool)
		return 0;

	ring_id = dp_wifi8->ppe2wbm_idle_buf_ring.ring_id;
	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
		FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_mpdu_start_offset = ab->hal.rx_mpdu_start_offset;
	tlv_filter.rx_msdu_end_offset = ab->hal.rx_msdu_end_offset;

	tlv_filter.rx_mpdu_start_wmask = ab->hal.rx_mpdu_start_wmask;
	tlv_filter.rx_msdu_end_wmask = ab->hal.rx_msdu_end_wmask;

	/* Source ring configuration for PPEDS */
	tlv_filter.rdi_based_source_cfg |=
		ath12k_wifi8_hal_get_rdi_source_cfg(ab, SOURCE_RING_CTRL_PPE);

	ath12k_dbg(ab, ATH12K_DBG_DATA,
			"rx_mpdu_start_wmask:0x%x rx_msdu_end_wmask:0x%x\n",
			tlv_filter.rx_mpdu_start_wmask,
			tlv_filter.rx_msdu_end_wmask);

	/* rx filter - PPEDS IDLE BUF ring */
	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
			HAL_PPE2WBM_IDLE_BUF,
			DP_RX_BUFFER_SIZE,
			ATH12K_PKTLOG_DISABLED,
			&tlv_filter);
	return ret;
}

int ath12k_wifi8_dp_rxdma_ring_sel_config_qcn9625(struct ath12k_base *ab)
{
	int ret;

	/*
	 * FTM does not complete DP MLO init on purpose to save memory.
	 * Skip RXDMA ring selection programming which depends on central dp.
	 */
	if (ath12k_ftm_mode)
		return 0;

	ret = ath12k_wifi8_dp_rx_wbm_idle_buf_0_config_qcn9625(ab);
	if (ret) {
		ath12k_err(ab, "Idle buf pool 0 config failed\n");
		return ret;
	}

	ret = ath12k_wifi8_dp_rx_ppe2wbm_idle_buf_config_qcn9625(ab);
	if (ret) {
		ath12k_err(ab, "PPE Idle buf pool 1 config failed\n");
		return ret;
	}

	return 0;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_wifi8_dp_ppe2wbm_srng_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	for (i = 0 ; i < dp_wifi8->num_ppe2wbm_refill_rings; i++)
		ath12k_dp_srng_cleanup(ab, &dp_wifi8->ppe2wbm_refill_ring[i]);

	if (dp_wifi8->dp_ppe2wbm_use_dedicated_pool)
		ath12k_dp_srng_cleanup(ab, &dp_wifi8->ppe2wbm_idle_buf_ring);
}

int ath12k_wifi8_dp_ppe2wbm_srng_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i, ret;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ath12k_ppeds_ppe2wbm_ring_size ||
		ath12k_ppeds_ppe2wbm_ring_size > DP_PPE2WBM_REFILL_RING_SIZE) {
		ath12k_warn(ab, "Invalid ppe2wbm refill ring size:%d\n",
				ath12k_ppeds_ppe2wbm_ring_size);
		return -EINVAL;
	}

	/*
	 * When no dedicated PPE2WBM pool is configured, SFE buffers are used.
	 * Hence, a separate PPE2WBM idle pool is not required.
	 * A dedicated SW2WBM refill ring is used for PPE refill.
	 */
	if (!dp_wifi8->dp_ppe2wbm_use_dedicated_pool) {
		ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->ppe2wbm_refill_ring[0],
					   HAL_WBM_BUF,
					   DP_PPE2WBM_SFE_POOL_REFILL_RING_NUM, 0,
					   ath12k_ppeds_ppe2wbm_ring_size);
		if (ret) {
			ath12k_warn(ab, "failed to alloc WBM refill ring\n");
			goto fail;
		}
	} else {
		for (i = 0; i < DP_PPE2WBM_REFILL_RING_MAX; i++) {
			ret = ath12k_dp_srng_alloc(ab,
						   &dp_wifi8->ppe2wbm_refill_ring[i],
						   HAL_PPE2WBM_BUF, i, 0,
						   ath12k_ppeds_ppe2wbm_ring_size);
			if (ret) {
				ath12k_warn(ab, "failed to alloc WBM refill ring\n");
				goto fail;
			}
		}

		ret = ath12k_dp_srng_alloc(ab,
					   &dp_wifi8->ppe2wbm_idle_buf_ring,
					   HAL_PPE2WBM_IDLE_BUF, 0, 0,
					   DP_PPE2WBM_IDLE_BUF_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to alloc wbm idle buf ring\n");
			goto fail;
		}
	}

	return 0;
fail:
	ath12k_wifi8_dp_ppe2wbm_srng_free(ab);
	return ret;
}

int ath12k_wifi8_dp_ppe2wbm_srng_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i, ret;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!dp_wifi8->dp_ppe2wbm_use_dedicated_pool) {
		ret = ath12k_dp_srng_init(ab,
					  &dp_wifi8->ppe2wbm_refill_ring[0],
					  HAL_WBM_BUF,
					  DP_PPE2WBM_SFE_POOL_REFILL_RING_NUM, 0);
		if (ret) {
			ath12k_warn(ab, "failed to init WBM refill ring\n");
			return ret;
		}
	} else {
		for (i = 0; i < DP_PPE2WBM_REFILL_RING_MAX; i++) {
			ret = ath12k_dp_srng_init(ab,
						  &dp_wifi8->ppe2wbm_refill_ring[i],
						  HAL_PPE2WBM_BUF, i, 0);
			if (ret) {
				ath12k_warn(ab, "failed to init WBM refill ring\n");
				return ret;
			}
		}

		ret = ath12k_dp_srng_init(ab,
					  &dp_wifi8->ppe2wbm_idle_buf_ring,
					  HAL_PPE2WBM_IDLE_BUF, 0, 0);
		if (ret) {
			ath12k_warn(ab, "failed to init wbm idle buf ring\n");
			return ret;
		}
	}

	return 0;
}

int ath12k_wifi8_dp_ppe2wbm_buf_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct hal_srng *srng = NULL;
	size_t req_entries;
	u32 ring_id;
	LIST_HEAD(used_list);

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	/*
	 * When no dedicated PPE2WBM pool is configured, SFE buffers are used.
	 * Buffer allocation is handled by standard SFE pool initialization.
	 * Skip initialization of buffers in the PPE2WBM buffer ring.
	 */
	if (!dp_wifi8->dp_ppe2wbm_use_dedicated_pool)
		return 0;

	ring_id = dp_wifi8->ppe2wbm_refill_ring[PPE2WBM_SW_REFILL_RING].ring_id;
	if (ring_id >= HAL_SRNG_RING_ID_MAX) {
		ath12k_err(ab, "Invalid PPE2WBM ring_id: %u\n", ring_id);
		return -EINVAL;
	}

	srng = &ab->hal.srng_list[ring_id];
	if (!srng->initialized) {
		ath12k_err(ab, "PPE2WBM ring not initialized\n");
		return -EINVAL;
	}
	req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, srng, &used_list,
			DP_RX_PPE_POOL);
	if (req_entries)
		ath12k_dp_rx_bufs_replenish(ab->dp, srng, &used_list, false);

	return 0;
}

void ath12k_wifi8_dp_rx_ppe2wbm_idle_buff_init(struct ath12k_base *ab)
{
	LIST_HEAD(list);
	size_t req_entries;
	struct hal_srng *idle_buf_srng;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	/*
	 * When no dedicated PPE2WBM pool is configured, SFE buffers are used.
	 * The SFE idle pool initialization is sufficient to handle buffer allocation.
	 * PPE2WBM idle pool initialization is not required.
	 */
	if (!dp_wifi8->dp_ppe2wbm_use_dedicated_pool)
		return;

	idle_buf_srng = &ab->hal.srng_list[dp_wifi8->ppe2wbm_idle_buf_ring.ring_id];
	req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, idle_buf_srng, &list,
						DP_RX_PPE_POOL);
	if (req_entries)
		ath12k_dp_rx_bufs_replenish(ab->dp, idle_buf_srng, &list, false);

}
#endif

int ath12k_wifi8_dp_rx_process_reo_status(struct ath12k_dp *dp, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	struct ath12k_dp_rx_reo_cmd *cmd, *tmp;
	bool found = false;
	int quota = budget;
	bool done = false;
	u16 tag;
	struct hal_reo_status reo_status;

	srng = &ab->hal.srng_list[dp->reo_status_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- && (hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		memset(&reo_status, 0, sizeof(reo_status));

		reo_status.tag = tag;
		switch (tag) {
		case HAL_REO_GET_QUEUE_STATS_STATUS:
			ath12k_wifi8_hal_reo_status_queue_stats(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_GET_QUEUE_1K_STATS_STATUS:
			ath12k_wifi8_hal_reo_status_queue_1k_stats(ab, hdr,
								   &reo_status);
			break;
		case HAL_REO_FLUSH_QUEUE_STATUS:
			ath12k_wifi8_hal_reo_flush_queue_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_CACHE_STATUS:
			ath12k_wifi8_hal_reo_flush_cache_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_UNBLOCK_CACHE_STATUS:
			ath12k_wifi8_hal_reo_unblk_cache_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_TIMEOUT_LIST_STATUS:
			ath12k_wifi8_hal_reo_flush_timeout_list_status(ab, hdr,
								       &reo_status);
			break;
		case HAL_REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS:
			ath12k_wifi8_hal_reo_desc_thresh_reached_status(ab, hdr,
									&reo_status);
			break;
		case HAL_REO_UPDATE_RX_REO_QUEUE_STATUS:
			ath12k_wifi8_hal_reo_update_rx_reo_queue_status(ab, hdr,
									&reo_status);
			break;
		default:
			ath12k_warn(ab, "Unknown reo status type %d\n", tag);
			continue;
		}

		found = false;
		done = true;

		spin_lock_bh(&dp->reo_cmd_lock);
		list_for_each_entry_safe(cmd, tmp, &dp->reo_cmd_list, list) {
			if (reo_status.uniform_hdr.cmd_num == cmd->cmd_num) {
				found = true;

				if (tag == HAL_REO_GET_QUEUE_STATS_STATUS &&
				    reo_status.u.queue_stats.to_follow_1k)
					done = false;

				if (done)
					list_del(&cmd->list);

				break;
			}
		}
		spin_unlock_bh(&dp->reo_cmd_lock);

		if (found) {
			cmd->handler(dp, (void *)&cmd->u.data, &reo_status);
			if (done)
				kfree(cmd);
		}
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

int ath12k_wifi8_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst)
{
	struct ath12k_base *ab = dp->ab;

	fst->num_entries = 0;

	fst->base = kcalloc(HAL_RX_FLOW_SEARCH_TABLE_SIZE,
			    sizeof(struct dp_rx_fse), GFP_KERNEL);
	if (!fst->base)
		return -ENOMEM;

	fst->hal_rx_fst = ath12k_wifi8_hal_rx_fst_attach(ab);
	if (!fst->hal_rx_fst) {
		ath12k_err(ab, "Rx Hal fst allocation failed\n");
		kfree(fst->base);
		return -ENOMEM;
	}

	/* Deferred full cache invalidation used to coalesce per-entry FSE
	 * commands when the command ring is full.
	 */
	fst->ab = ab;
	INIT_DELAYED_WORK(&fst->cache_inval_full_work,
			  ath12k_wifi8_dp_rx_fse_cache_inval_full_work);

	return 0;
}

void ath12k_wifi8_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst)
{
	struct ath12k_base *ab = dp->ab;

	cancel_delayed_work_sync(&fst->cache_inval_full_work);
	ath12k_wifi8_hal_rx_fst_detach(ab, fst->hal_rx_fst);
	kfree(fst->base);
}

void ath12k_wifi8_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info)
{
	struct hal_flow_tuple_info *tuple_info = &flow_info->flow_tuple_info;
	struct ath12k_base *ab = dp->ab;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Dest IP address %x:%x:%x:%x",
		   tuple_info->dest_ip_127_96,
		   tuple_info->dest_ip_95_64,
		   tuple_info->dest_ip_63_32,
		   tuple_info->dest_ip_31_0);
	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Source IP address %x:%x:%x:%x",
		   tuple_info->src_ip_127_96,
		   tuple_info->src_ip_95_64,
		   tuple_info->src_ip_63_32,
		   tuple_info->src_ip_31_0);
	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Dest port %u, Src Port %u, Protocol %u",
		   tuple_info->dest_port,
		   tuple_info->src_port,
		   tuple_info->l4_protocol);
}

static
u32 ath12k_dp_rx_flow_compute_flow_hash(struct ath12k_base *ab,
					struct dp_rx_fst *fst,
					struct rx_flow_info *rx_flow_info,
					struct hal_rx_flow *flow)
{
	memcpy(&flow->tuple_info, &rx_flow_info->flow_tuple_info,
	       sizeof(struct hal_flow_tuple_info));

	return ath12k_wifi8_hal_flow_toeplitz_hash(ab, fst->hal_rx_fst,
						   &flow->tuple_info);
}

static inline struct dp_rx_fse *
ath12k_dp_rx_flow_get_fse(struct dp_rx_fst *fst, u32 flow_hash)
{
	struct dp_rx_fse *fse;
	u32 idx = ath12k_wifi8_hal_rx_get_trunc_hash(fst->hal_rx_fst, flow_hash);

	fse = (struct dp_rx_fse *)fst->base;
	return &fse[idx];
}

struct dp_rx_fse *
ath12k_dp_rx_flow_find_entry_by_tuple(struct ath12k_base *ab,
				      struct dp_rx_fst *fst,
				      struct rx_flow_info *flow_info,
				      struct hal_rx_flow *flow)
{
	u32 flow_hash;
	u32 flow_idx;
	int status;

	flow_hash = ath12k_dp_rx_flow_compute_flow_hash(ab, fst, flow_info, flow);

	status = ath12k_wifi8_hal_rx_find_flow_from_tuple(ab, fst->hal_rx_fst,
							  flow_hash,
							  &flow_info->flow_tuple_info,
							  &flow_idx);
	if (status != 0) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Could not find tuple with hash %u",
			   flow_hash);
		ath12k_wifi8_dp_rx_flow_dump_entry(ab->dp, flow_info);
		return NULL;
	}

	return ath12k_dp_rx_flow_get_fse(fst, flow_idx);
}

ssize_t ath12k_wifi8_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size)
{
	struct ath12k_base *ab = dp->ab;
	struct dp_rx_fst *fst = ab->ag->dp_hw_grp->fst;
	int len = 0;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	len += scnprintf(buf + len, size - len,
			 "Number of entries in FST table: %d\n", fst->num_entries);
	len += ath12k_wifi8_hal_rx_dump_fst_table(ab, fst->hal_rx_fst,
						  buf + len, size - len);

	return len;
}

bool
ath12k_wifi8_dp_rx_check_if_flow_to_be_updated(struct dp_rx_fse *fse,
					       struct rx_flow_info *flow_info)
{
	struct hal_rx_fse *hal_fse = fse->hal_fse;
	u32 use_ppe;

	use_ppe = u32_get_bits(hal_fse->info2, HAL_RX_FSE_SERVICE_CODE);

	/* If everything matches, it is a duplicate flow request,
	 * no need to modify anything,
	 * else modify the flow entry.
	 */
	if (use_ppe == flow_info->use_ppe &&
	    (hal_fse->metadata & flow_info->fse_metadata))
		return false;

	return true;
}

struct dp_rx_fse *
ath12k_wifi8_dp_rx_flow_alloc_entry(struct ath12k_base *ab,
				    struct dp_rx_fst *fst,
				    struct rx_flow_info *flow_info,
				    struct hal_rx_flow *flow)
{
	struct dp_rx_fse *fse;
	u32 flow_hash;
	u32 flow_idx;
	int status;

	flow_hash = ath12k_dp_rx_flow_compute_flow_hash(ab, fst, flow_info, flow);

	status = ath12k_wifi8_hal_rx_flow_insert_entry(ab, fst->hal_rx_fst, flow_hash,
						       &flow_info->flow_tuple_info,
						       &flow_idx);
	if (status != 0) {
		if (status == -EEXIST) {
			/* Even if the flow tuple info exists, there is a possibility
			 * that the flow entry has to be updated - so check
			 * for rx_flow_info if it matches exactly with the programmed
			 * fse entry
			 */
			fse = ath12k_dp_rx_flow_get_fse(fst, flow_idx);

			if (ath12k_wifi8_dp_rx_check_if_flow_to_be_updated(fse,
									   flow_info)) {
				ath12k_dbg(ab, ATH12K_DBG_DP_FST,
					   "Flow entry to be updated - hash %u",
					   flow_hash);
				return fse;
			}
		}
		ath12k_dbg(ab, ATH12K_DBG_DP_FST,
			   "Add entry failed with status %d for tuple with hash %u",
			   status, flow_hash);
		return NULL;
	}

	fse = ath12k_dp_rx_flow_get_fse(fst, flow_idx);
	fse->flow_hash = flow_hash;
	fse->flow_id = flow_idx;
	fse->is_valid = true;

	return fse;
}

static int ath12k_dp_fst_get_reo_indication(struct ath12k_dp *dp)
{
	u8 reo_indication;

	reo_indication = dp->fst_config.fst_core_map[dp->fst_config.core_idx] + 1;
	dp->fst_config.core_idx = (dp->fst_config.core_idx + 1) %
					dp->fst_config.fst_num_cores;

	return reo_indication;
}

int ath12k_wifi8_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	struct dp_rx_fse *fse;

	/* Allocate entry in DP FST */
	fse = ath12k_wifi8_dp_rx_flow_alloc_entry(ab, fst, flow_info, &flow);
	if (!fse) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "RX FSE alloc failed");
		return -ENOMEM;
	}

	flow.drop = flow_info->drop;

	/* Reo indication is required only when drop bit is not set */
	if (!flow.drop) {
		if (flow_info->ring_id && flow_info->ring_id <= DP_REO_DST_RING_MAX)
			flow.reo_indication = flow_info->ring_id;
		else
			flow.reo_indication = ath12k_dp_fst_get_reo_indication(dp);
	}

	fse->reo_indication = flow.reo_indication;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;
	flow.fse_metadata |= flow_info->fse_metadata;
	if (flow_info->use_ppe) {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		flow.service_code = PPE_DRV_SC_SPF_BYPASS;
		if (ath12k_ppeds_pkt_pre_hdr_mode >= 0 &&
			ath12k_ppeds_pkt_pre_hdr_mode  < PPEDS_CLASSIFY_READ_FULL_PKT)
			flow.ppe_classify_read_hint = ath12k_ppeds_pkt_pre_hdr_mode;
		else
			flow.ppe_classify_read_hint = PPEDS_CLASSIFY_READ_FULL_PKT;
		flow.reo_indication = PPEDS_REO2PPE1_RDI;
		flow.dest_info = ((flow.fse_metadata &
				ATH12K_DP_RX_FSE_FL_EGRESS_MACID_MASK) >>
				ATH12K_DP_RX_FSE_FL_EGRESS_MACID_SHIFT);
		flow.dest_info_valid = 1;
		flow.int_priority = 0;
		flow.int_priority_valid = 1;
		ath12k_dbg(ab, ATH12K_DBG_DP_FST,
				"read_hint:%u rdi:%u dest_info:%u\n",
				flow.ppe_classify_read_hint,
				flow.reo_indication, flow.dest_info);
#endif
	}

	fse->hal_fse = ath12k_wifi8_hal_rx_flow_setup_fse(ab, fst->hal_rx_fst,
							  fse->flow_id, &flow);
	if (!fse->hal_fse) {
		ath12k_err(ab, "Unable to alloc FSE entry");
		fse->is_valid = false;
		return -EEXIST;
	}

	fst->num_entries++;
	if (fse->reo_indication)
		fst->flows_per_reo[fse->reo_indication - 1]++;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d, reo_dest_ind = %d, reo_dest_hand = %u",
		   fst->num_entries, flow.reo_indication,
		   flow.reo_destination_handler);

	return 0;
}

int ath12k_wifi8_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fse *fse;
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;

	fse = ath12k_dp_rx_flow_find_entry_by_tuple(ab, fst, flow_info, &flow);
	if (!fse || !fse->is_valid) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "RX flow delete entry failed");
		return -EINVAL;
	}

	/* Delete the FSE in HW FST */
	ath12k_wifi8_hal_rx_flow_delete_entry(ab, fse->hal_fse);

	/* mark the FSE entry as invalid */
	fse->is_valid = false;

	/* Decrement number of valid entries in table */
	fst->num_entries--;
	if (fse->reo_indication)
		fst->flows_per_reo[fse->reo_indication - 1]--;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);

	return 0;
}

int ath12k_wifi8_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	struct dp_rx_fse *fse;
	int i;

	fse = (struct dp_rx_fse *)fst->base;
	if (!fse)
		return -ENODEV;

	for (i = 0; i < fst->hal_rx_fst->max_entries; i++, fse++) {
		if (!fse->is_valid)
			continue;

		ath12k_wifi8_hal_rx_flow_delete_entry(ab, fse->hal_fse);

		fse->is_valid = false;

		fst->num_entries--;
		if (fse->reo_indication)
			fst->flows_per_reo[fse->reo_indication - 1]--;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);

	return 0;
}

static void ath12k_wifi8_dp_rx_fse_cache_inval_full_work(struct work_struct *work)
{
	struct dp_rx_fst *fst = container_of(to_delayed_work(work),
					     struct dp_rx_fst,
					     cache_inval_full_work);

	ath12k_info(fst->ab, "Delayed fse cache invalidation work");

	ath12k_wifi8_dp_rx_flow_fse_cache_operation(fst->ab, DP_FST_CACHE_INVALIDATE_FULL,
						    NULL);
}

int ath12k_wifi8_dp_rx_flow_fse_cache_operation(struct ath12k_base *ab,
						enum dp_flow_fst_operation op_code,
						struct hal_flow_tuple_info *tuple_info)
{
	struct hal_fse_cmd fse_cmd = { 0 };
	int ret, i;
	u32 chips = 0;
	u32 chip[] = HAL_FSE_CMD_HDR_INFO0_SEND_TO_CHIP_N;
	struct ath12k_base *partner_ab;

	if (op_code == DP_FST_CACHE_INVALIDATE_ENTRY) {

	/* Populate tuple fields for cache invalidation */
		fse_cmd.src_ip[0] = htonl(tuple_info->src_ip_127_96);
		fse_cmd.src_ip[1] = htonl(tuple_info->src_ip_95_64);
		fse_cmd.src_ip[2] = htonl(tuple_info->src_ip_63_32);
		fse_cmd.src_ip[3] = htonl(tuple_info->src_ip_31_0);
		fse_cmd.dest_ip[0] = htonl(tuple_info->dest_ip_127_96);
		fse_cmd.dest_ip[1] = htonl(tuple_info->dest_ip_95_64);
		fse_cmd.dest_ip[2] = htonl(tuple_info->dest_ip_63_32);
		fse_cmd.dest_ip[3] = htonl(tuple_info->dest_ip_31_0);
		fse_cmd.src_port = cpu_to_le16(tuple_info->src_port);
		fse_cmd.dest_port = cpu_to_le16(tuple_info->dest_port);
		fse_cmd.info0 =
			cpu_to_le32(FIELD_PREP(HAL_FSE_CMD_INFO0_L4_PROTOCOL,
					       tuple_info->l4_protocol) |
				    FIELD_PREP(HAL_FSE_CMD_INFO0_GSE_CTRL,
					       HAL_FSE_GSE_CTRL_INVAL_SINGLE));
	} else if (op_code == DP_FST_CACHE_INVALIDATE_FULL) {
		fse_cmd.info0 =
			cpu_to_le32(FIELD_PREP(HAL_FSE_CMD_INFO0_GSE_CTRL,
					       HAL_FSE_GSE_CTRL_INVAL_ALL));
	} else if (op_code == DP_FST_DISABLE) {
		fse_cmd.info0 =
			cpu_to_le32(FIELD_PREP(HAL_FSE_CMD_INFO0_GSE_CTRL,
					       HAL_FSE_GSE_CTRL_SRCH_DIS));
	} else if (op_code == DP_FST_ENABLE) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST,
			   "DP_FST_ENABLE op: no GSE command required\n");
		return 0;
	}

	for (i = 0; i < ab->ag->num_devices; i++) {
		partner_ab = ab->ag->ab[i];
		if (!partner_ab || partner_ab->is_bypassed)
			continue;
		chips |= chip[partner_ab->device_id];
	}

	fse_cmd.cmd.info0 = cpu_to_le16(chips);

	ret = ath12k_wifi8_dp_fse_cmd_send(ab, &fse_cmd);

	return ret;
}

int ath12k_wifi8_dp_rx_ase_htt_srng_setup(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	u32 ring_id;

	ring_id = dp_wifi8->rx_ase_status_ring.ring_id;
	return ath12k_dp_tx_htt_srng_setup(ab, ring_id, 0, HAL_ASE_STATUS_RING);
}

int ath12k_wifi8_dp_rx_htt_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	ret = ath12k_wifi8_dp_rx_ase_htt_srng_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to configure ASE status ring %d\n",
			    ret);
		return ret;
	}

	ret = ath12k_dp_mon_rx_htt_setup(dp);
	if (ret) {
		ath12k_warn(ab, "Failed to setup rxdma monitor rings\n");
		return ret;
	}

	return 0;
}

void ath12k_wifi8_dp_pdev_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k *ar;
	int i;

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], NULL);
	}
	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		ath12k_fw_stats_free(&ar->fw_stats);

		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_tx_pdev_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);

			ar->dp.dp_mon_pdev_configured = false;
		}
	}
}

int ath12k_wifi8_dp_pdev_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k *ar;
	int ret;
	int i, j;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_dp_ppe_rxole_rxdma_cfg(ab);
	if (ret) {
		ath12k_err(ab, "Failed to send htt RxOLE and RxDMA messages to target :%d\n",
			   ret);
		goto out;
	}
#endif

	ret = ath12k_wifi8_dp_rx_htt_setup(ab);
	if (ret)
		goto out;

	/* TODO: Per-pdev rx ring unlike tx ring which is mapped to different AC's */
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		memset(&ar->stats, 0, sizeof(struct ath12k_pdev_ctrl_path_stats));
		dp_pdev = &ar->dp;

		dp_pdev->hw = ar->ah->hw;
		dp_pdev->dp = dp;
		/* Below linking is a temporary linking to handle few cases like cac
		 * timeout, active pdev etc in dp rx. Some flags/fileds can be added
		 * in dp_pdev to remove ar dependencies in the performance critical
		 * path.
		 *
		 * TODO: remove this once those dependencies are resolved.
		 */
		dp_pdev->ar = ar;
		dp_pdev->dp_hw = &ar->ah->dp_hw;
		dp_pdev->hw_link_id = ar->hw_link_id;

		/* Restore dp_stats_mask from INI to persist value across reboots */
		ar->dp.dp_stats_mask = DP_STATS_MASK;

		if (!dp_pdev->dp_mon_pdev_configured) {
			ret = ath12k_dp_mon_pdev_init(dp_pdev);
			if (ret) {
				ath12k_warn(ab, "failed to initialize mon pdev %d\n", i);
				goto err_cleanup_pdevs;
			}

			ret = ath12k_dp_mon_pdev_rx_alloc(dp_pdev, i);
			if (ret) {
				ath12k_warn(ab,
					    "failed to alloc rx filter for pdev %d\n",
					    i);
				goto err_mon_pdev_deinit;
			}

			ret = ath12k_dp_mon_pdev_rx_htt_setup(dp_pdev, i);
			if (ret) {
				ath12k_warn(ab,
					    "failed to setup rx htt for pdev %d\n", i);
				goto err_mon_pdev_rx_free;
			}

			ret = ath12k_dp_mon_tx_pdev_alloc(dp_pdev, i);
			if (ret) {
				ath12k_err(ab, "TX Monitor: alloc fail pdev %d(%d)\n",
					   i, ret);
				goto err_mon_pdev_tx_free;
			}


			dp_pdev->dp_mon_pdev_configured = true;
		}
	}

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], &ar->dp);
	}
	spin_unlock_bh(&dp->dp_lock);

	dp->num_radios = ab->num_radios;

	return ret;

err_mon_pdev_tx_free:
	ath12k_dp_mon_tx_pdev_free(dp_pdev);
err_mon_pdev_rx_free:
	/* Clean up the current pdev's RX allocation */
	ath12k_dp_mon_pdev_rx_free(dp_pdev);
err_mon_pdev_deinit:
	/* Clean up the current pdev's monitor initialization */
	ath12k_dp_mon_pdev_deinit(dp_pdev);
err_cleanup_pdevs:
	/* Clean up all previously configured pdevs */
	for (j = 0; j < i; j++) {
		ar = ab->pdevs[j].ar;
		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_tx_pdev_free(&ar->dp);
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);
			ar->dp.dp_mon_pdev_configured = false;
		}
	}

out:
	return ret;
}

void ath12k_wifi8_dp_rx_wbm_srng_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i;

	for (i = 0 ; i < DP_WBM_REFILL_RING_MAX; i++)
		ath12k_dp_srng_cleanup(ab, &dp_wifi8->wbm_refill_ring[i]);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->wbm_idle_buf_ring);
}

int ath12k_wifi8_dp_rx_wbm_srng_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i, ret;

	for (i = 0; i < DP_WBM_REFILL_RING_MAX; i++) {
		ret = ath12k_dp_srng_alloc(ab,
					   &dp_wifi8->wbm_refill_ring[i],
					   HAL_WBM_BUF, i, 0,
					   DP_WBM_REFILL_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to alloc WBM refill ring\n");
			goto fail;
		}
	}

	ret = ath12k_dp_srng_alloc(ab,
				   &dp_wifi8->wbm_idle_buf_ring,
				   HAL_WBM_IDLE_BUF, 0, 0,
				   ab->ag->num_devices * DP_WBM_IDLE_BUF_PER_SOC);
	if (ret) {
		ath12k_warn(ab, "failed to alloc wbm idle buf ring\n");
		goto fail;
	}

	return 0;
fail:
	ath12k_wifi8_dp_rx_wbm_srng_free(ab);
	return ret;
}

int ath12k_wifi8_dp_rx_wbm_srng_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i, ret;

	for (i = 0; i < DP_WBM_REFILL_RING_MAX; i++) {
		ret = ath12k_dp_srng_init(ab,
					  &dp_wifi8->wbm_refill_ring[i],
					  HAL_WBM_BUF, i, 0);
		if (ret) {
			ath12k_warn(ab, "failed to init WBM refill ring\n");
			return ret;
		}
	}

	ret = ath12k_dp_srng_init(ab,
				  &dp_wifi8->wbm_idle_buf_ring,
				  HAL_WBM_IDLE_BUF, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init wbm idle buf ring\n");
		return ret;
	}

	return 0;
}

int ath12k_wifi8_dp_rx_wbm_buf_ring_init(struct ath12k_base *ab)
{
	LIST_HEAD(list);
	size_t req_entries;
	struct hal_srng *idle_buf_srng;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	idle_buf_srng = &ab->hal.srng_list[dp_wifi8->wbm_idle_buf_ring.ring_id];
	req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, idle_buf_srng, &list,
				DP_RX_DEFAULT_POOL);
	if (req_entries)
		ath12k_dp_rx_bufs_replenish(ab->dp, idle_buf_srng, &list, false);

	return 0;
}

int ath12k_wifi8_dp_rx_fse_cmd_srng_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	return ath12k_dp_srng_alloc(ab, &dp_wifi8->fse_cmd_ring,
				    HAL_RXOLE_FSE_CMD,
				    0, 0, DP_FSE_CMD_RING_SIZE);
}

int ath12k_wifi8_dp_rx_fse_cmd_srng_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	return ath12k_dp_srng_init(ab, &dp_wifi8->fse_cmd_ring,
				   HAL_RXOLE_FSE_CMD, 0, 0);
}

void ath12k_wifi8_dp_rx_fse_cmd_srng_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	ath12k_dp_srng_cleanup(ab, &dp_wifi8->fse_cmd_ring);
}

int ath12k_wifi8_dp_rx_reo_flush_srng_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	return ath12k_dp_srng_alloc(ab, &dp_wifi8->reo_flush_ring,
				    HAL_REO_FLUSH,
				    0, 0, DP_REO_FLUSH_RING_SIZE);
}

int ath12k_wifi8_dp_rx_reo_flush_srng_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	return ath12k_dp_srng_init(ab, &dp_wifi8->reo_flush_ring,
				   HAL_REO_FLUSH, 0, 0);
}

void ath12k_wifi8_dp_rx_reo_flush_srng_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	ath12k_dp_srng_cleanup(ab, &dp_wifi8->reo_flush_ring);
}

void ath12k_wifi8_dp_rx_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	ath12k_wifi8_dp_rx_reo_flush_srng_free(ab);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->reo_high_prio_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_dst_ring[ATH12K_DP_RX_ROAMING_RING1]);
	ath12k_wifi8_dp_rx_wbm_srng_free(ab);
	ath12k_dp_rx_reo_cleanup(ab);
	ath12k_wifi8_dp_rx_fse_cmd_srng_free(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_dp_ppe2wbm_srng_free(ab);
#endif
}

static void ath12k_configure_wbm_rxdma_watermark(struct ath12k_base *ab)
{
	int i;
	int val;
	int sfe_watermark[] = HAL_UMAC_WBM_BUFF_DESC_RING_CFG1;
	int mgmt_watermark[] = HAL_UMAC_WBM_BUFF_DESC_RING_CFG2;
	int ppe_watermark[] = HAL_UMAC_WBM_BUFF_DESC_RING_CFG3;
	int wbm_base = HAL_SEQ_WCSS_UMAC_WBM_REG;
	u32 sfe_val = ((DP_WBM_SFE_HIGH_WATERMARK - HAL_UMAC_WBM_LOW_WATERMARK_DIFF) <<
			HAL_UMAC_WBM_LOW_WATERMARK_SHIFT) | DP_WBM_SFE_HIGH_WATERMARK;
	u32 mgmt_val = ((DP_WBM_MGMT_HIGH_WATERMARK - HAL_UMAC_WBM_LOW_WATERMARK_DIFF) <<
			HAL_UMAC_WBM_LOW_WATERMARK_SHIFT) | DP_WBM_MGMT_HIGH_WATERMARK;
	u32 ppe_val = ((DP_WBM_PPE_HIGH_WATERMARK - HAL_UMAC_WBM_LOW_WATERMARK_DIFF) <<
			HAL_UMAC_WBM_LOW_WATERMARK_SHIFT) | DP_WBM_PPE_HIGH_WATERMARK;

	/* Enable watermark configuration by clearing the disable bit */
	val = ath12k_hif_read32(ab, wbm_base + HAL_UMAC_WBM_MISC_CTRL);
	val &= ~HAL_UMAC_WBM_WATERMARK_DISABLE;
	ath12k_hif_write32(ab, wbm_base + HAL_UMAC_WBM_MISC_CTRL, val);

	for (i = 0; i < HAL_UMAC_WBM_MAX_WATERMARK_CFG_REGS; i++) {
		ath12k_hif_write32(ab, wbm_base + sfe_watermark[i], sfe_val);
		ath12k_hif_write32(ab, wbm_base + mgmt_watermark[i], mgmt_val);
		ath12k_hif_write32(ab, wbm_base + ppe_watermark[i], ppe_val);
	}
}

int ath12k_wifi8_dp_rx_ring_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int ret;

	ret = ath12k_dp_rx_reo_alloc(ab);
	if (ret) {
		ath12k_err(ab, "failed to allocate reo destination rings: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_alloc(ab,
				   &dp->reo_dst_ring[ATH12K_DP_RX_ROAMING_RING1],
				   HAL_REO_DST_ROAMING,
				   ATH12K_DP_RX_ROAMING_RING1, 0,
				   DP_REO_ROAMING_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to alloc reo_dst_ring[%d] for roaming :%d\n",
			    ATH12K_DP_RX_ROAMING_RING1, ret);
		return ret;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_wifi8_dp_ppe2wbm_srng_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to alloc ppe2wbm refill and idle buf rings\n");
		return ret;
	}
#endif

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->fse_cmd_ring,
				   HAL_RXOLE_FSE_CMD, 0, 0, DP_FSE_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to alloc fse_cmd ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->reo_high_prio_cmd_ring,
				   HAL_REO_CMD, 1, 0, DP_REO_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to alloc reo_high_prio_cmd ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->reo_flush_ring,
				   HAL_REO_FLUSH, 0, 0, DP_REO_FLUSH_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to alloc reo_flush_ring: %d\n", ret);
		return ret;
	}

	ret = ath12k_wifi8_dp_rx_wbm_srng_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to alloc rx wbm refill and idle buf rings\n");
		return ret;
	}

	return 0;
}

int ath12k_wifi8_dp_rx_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct hal_srng *srng;
	int ret;

	ret = ath12k_dp_rx_reo_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize reo destination rings: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab,
				  &dp->reo_dst_ring[ATH12K_DP_RX_ROAMING_RING1],
				  HAL_REO_DST_ROAMING,
				  ATH12K_DP_RX_ROAMING_RING1, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init reo_dst_ring[%d] for roaming :%d\n",
			    ATH12K_DP_RX_ROAMING_RING1, ret);
		return ret;
	}

	ath12k_configure_wbm_rxdma_watermark(ab);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_wifi8_dp_ppe2wbm_srng_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to init ppe2wbm refill and idle buf rings\n");
		return ret;
	}
#endif

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->fse_cmd_ring,
				  HAL_RXOLE_FSE_CMD, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init fse_cmd ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->reo_high_prio_cmd_ring,
				  HAL_REO_CMD, 1, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init reo_high_prio_cmd ring :%d\n", ret);
		return ret;
	}

	srng = &ab->hal.srng_list[dp_wifi8->reo_high_prio_cmd_ring.ring_id];
	ath12k_wifi8_hal_reo_init_cmd_ring_offset(ab, srng,
						  ATH12K_WIFI8_REO_CMD_HIGHPRI_START_NUM);

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->reo_flush_ring,
				  HAL_REO_FLUSH, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init reo_flush_ring: %d\n", ret);
		return ret;
	}

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	ret = ath12k_wifi8_dp_rx_wbm_srng_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to init rx wbm refill and idle buf rings\n");
		return ret;
	}

	ret = ath12k_wifi8_dp_rx_wbm_buf_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to configure rx wbm idle buf ring\n");
		return ret;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_dp_rx_ppe2wbm_idle_buff_init(ab);

	ret = ath12k_wifi8_dp_ppe2wbm_buf_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to configure wbm idle buf ring\n");
		return ret;
	}
#endif

	return 0;
}

static void
ath12k_wifi8_flush_handle_null_queue(struct ath12k_dp *dp,
				     struct ath12k_rx_desc_info *desc_info,
				     struct hal_reo_dest_ring *rx_desc,
				     struct napi_struct *napi)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ieee80211_rx_status rx_status = {0};
	struct rx_tlv_info_1 prev_tlv = {0};
	struct hal_rx_spd_data rx_spd = {0};
	struct ath12k_dp_peer *peer;
	struct ath12k_pdev_dp *dp_pdev;
	struct hal_rx_desc *hal_rx_desc;
	u32 hal_rx_desc_sz;
	u32 peer_metadata;
	u16 peer_id;
	u8 hw_link_id;
	int msdu_idx = 0;
	bool drop;
	int len;

	if (!desc_info->skb)
		return;

	ath12k_core_dmac_inv_range(desc_info->vaddr,
				   desc_info->vaddr + DP_RX_BUFFER_SIZE);

	hal_rx_desc = (struct hal_rx_desc *)desc_info->vaddr;

	ath12k_wifi8_cpy_hw_rx_desc_to_spad_desc(rx_desc, &rx_spd);
	ath12k_wifi8_dp_extract_rx_spd_data(dp->hal, &rx_spd, hal_rx_desc);

	if (rx_spd.rx_mpdu_info.reo_dest_buffer_type ==
	    HAL_REO_DEST_RING_BUFFER_TYPE_LINK_DESC)
		return;

	rx_spd.msdu = desc_info->skb;
	rx_spd.vaddr = desc_info->vaddr;

	desc_info->skb = NULL;

	hw_link_id = ath12k_dp_validate_hw_link_id(rx_spd.rx_mpdu_info.src_link_id);
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, hw_link_id);
	if (unlikely(!dp_pdev))
		goto drop_packet;

	if (!rcu_dereference(dp_pdev->dp->ab->pdevs_active[dp_pdev->ar->pdev_idx]))
		goto drop_packet;

	peer_metadata = rx_spd.rx_mpdu_info.peer_meta_data;
	peer_id = ath12k_wifi8_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
						 peer_metadata);

	peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev, peer_id);

	hw_link_id = ath12k_dp_validate_hw_link_id(hw_link_id);
	rx_spd.rx_mpdu_info.src_link_id = hw_link_id;

	if (rx_spd.rx_msdu_info.msdu_continuation)
		goto drop_packet;

	hal_rx_desc_sz = dp->hal->hal_desc_sz;

	len = rx_spd.rx_msdu_info.msdu_length + hal_rx_desc_sz +
		(rx_spd.rx_msdu_info.l3_header_padding_msb ? 2 : 0);

	if (skb_tailroom(rx_spd.msdu) < len)
		goto drop_packet;

	ath12k_wifi8_dp_adjust_skb(&rx_spd, NULL, &msdu_idx, hal_rx_desc_sz);

	drop = ath12k_wifi8_handle_null_queue(dp_pdev, peer, &rx_status,
					      &rx_spd, napi, &prev_tlv);
	if (drop)
		goto drop_packet;

	return;

drop_packet:
	dev_kfree_skb_any(rx_spd.msdu);
	return;
}

int ath12k_wifi8_dp_rx_process_reo_flush_err(struct ath12k_dp *dp,
					     struct napi_struct *napi,
					     int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_wifi8_rx_stats *stats = &dp_wifi8->stats.rx_stats;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	enum hal_wifi8_rx_buf_return_buf_manager rbm;
	struct ath12k_rx_desc_info *desc_info;
	int cpu_id = smp_processor_id();
	struct hal_reo_dest_ring *rx_desc;
	struct hal_srng *srng, *refill_srng;
	struct list_head rx_desc_used_list;
	struct list_head rx_mgmt_desc_used_list;
	struct list_head ppe2wbm_used_list;
	int quota = budget;
	u32 cookie;
	u64 paddr;

	srng = &ab->hal.srng_list[dp_wifi8->reo_flush_ring.ring_id];

	INIT_LIST_HEAD(&rx_desc_used_list);
	INIT_LIST_HEAD(&rx_mgmt_desc_used_list);
	INIT_LIST_HEAD(&ppe2wbm_used_list);

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget) {
		rx_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng);
		if (!rx_desc)
			break;
		--budget;

		rbm = le32_get_bits(rx_desc->buf_addr_info.info1,
				    BUFFER_ADDR_INFO1_RET_BUF_MGR);
		cookie = le32_get_bits(rx_desc->buf_addr_info.info1,
				       BUFFER_ADDR_INFO1_SW_COOKIE);
		paddr = (((u64)le32_get_bits(rx_desc->buf_addr_info.info1,
					     BUFFER_ADDR_INFO1_ADDR)) << 32) |
				le32_get_bits(rx_desc->buf_addr_info.info0,
					      BUFFER_ADDR_INFO0_ADDR);

		/*
		 * NULL descriptors are possible here due to WAR for HW issue TRSLONE-1155
		 * This issue is specific to V1 HW
		 *
		 * Handle this case gracefuly.
		 */
		if (!paddr && !cookie) {
			stats->rx_flush_null_descs++;
			ath12k_warn(ab, "NULL descriptor released to host WAR kicked in for TRSLONE-1155\n");
			continue;
		}

		if (rbm == dp->hal->hal_params->rx_buf_rbm || rbm == 0) {
			desc_info = ath12k_dp_get_rx_desc(dp, cookie);
			if (!desc_info)
				continue;

			if (desc_info->is_ppe_desc == DP_RX_PPE_POOL) {
				desc_info->skb = NULL;
				desc_info->paddr = 0;
				list_add_tail(&desc_info->list, &ppe2wbm_used_list);
			} else {
				struct hal_rx_reo_dest_rel_info err_info = {0};
				int ret;

				ret = ath12k_wifi8_hal_reo_rel_parse_err(dp, rx_desc,
									 &err_info);
				if (!ret) {
					if (err_info.err_rel_src !=
					    HAL_REO_REL_SRC_MODULE_REO ||
					    err_info.push_reason !=
					    HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED ||
					    err_info.err_code !=
					    HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO) {
						ret = -EINVAL;
					}
				}

				if (!ret) {
					rcu_read_lock();

					ath12k_wifi8_flush_handle_null_queue(dp,
									     desc_info,
									     rx_desc,
									     napi);
					rcu_read_unlock();
				} else {
					stats->rx_flush_pkts++;
				}

				if (desc_info->skb)
					dev_kfree_skb_any(desc_info->skb);

				list_add_tail(&desc_info->list, &rx_desc_used_list);
			}
		} else if (rbm == dp->hal->hal_params->rx_mgmt_buf_rbm) {
			desc_info = ath12k_mgmt_get_rx_desc_from_cookie(mgmt, cookie);
			if (!desc_info)
				continue;

			if (desc_info->skb)
				dev_kfree_skb_any(desc_info->skb);

			list_add_tail(&desc_info->list, &rx_mgmt_desc_used_list);
			stats->rx_mgmt_flush_pkts++;
		} else {
			ath12k_warn(ab, "invalid rbm received: %d\n", rbm);
			WARN_ON_ONCE(1);
		}
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	if (!list_empty(&ppe2wbm_used_list)) {
		u32 ring_id = dp_wifi8->extn.ipa->ipa2wbm_ring[ATH12K_DP_WIFI8_IPA2WBM_HOST].ring_id;

		refill_srng = &ab->hal.srng_list[ring_id];
		ath12k_dp_rx_bufs_replenish(dp, refill_srng,
					    &ppe2wbm_used_list, false);
	}
#endif

	if (!list_empty(&rx_desc_used_list)) {
		refill_srng = &ab->hal.srng_list[dp_wifi8->wbm_refill_ring[cpu_id %
						DP_WBM_REFILL_RING_MAX].ring_id];
		ath12k_dp_rx_bufs_replenish(dp, refill_srng, &rx_desc_used_list, false);
	}

	if (!list_empty(&rx_mgmt_desc_used_list))
		ath12k_wifi8_mgmt_rx_replenish_buffs(mgmt, &mgmt_wifi8->wbm_refill_ring,
						     &rx_mgmt_desc_used_list, false);

	return quota - budget;
}

static void ath12k_wifi8_dp_smd_rx_flush_done(struct ath12k_dp *dp, void *ctx,
					      struct hal_reo_status *status)
{
	struct ath12k_dp_rx_tid *rx_tid = ctx;
	struct completion *done = rx_tid->smd_ctx;

	if (status && status->uniform_hdr.cmd_status != HAL_REO_CMD_SUCCESS)
		ath12k_warn(dp->ab,
			    "smd prep: REO FLUSH_CACHE failed status=%d\n",
			    status->uniform_hdr.cmd_status);

	if (done)
		complete(done);
}

int ath12k_wifi8_dp_smd_prep_rx_tid(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    struct ieee80211_vif *vif,
				    const u8 *addr)
{
	struct ath12k_dp_peer *current_dp_peer;
	struct ath12k_dp_hw_group *hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_smd_parked_rx_info *parked;
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_base *ab = dp->ab;
	int ret = 0, tid;
	long timeout;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	current_dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	if (!current_dp_peer) {
		ath12k_warn(ab, "smd prep_rx_tid: peer %pM not found\n", addr);
		return -ENOENT;
	}

	/* Allocate parked state container */
	parked = kzalloc(sizeof(*parked), GFP_KERNEL);
	if (!parked)
		return -ENOMEM;

	init_completion(&parked->flush_done);
	parked->num_tids = ab->hal.hal_params->num_tids;

	spin_lock_bh(&dp->dp_lock);

	for (tid = 0; tid < parked->num_tids; tid++) {
		rx_tid = &current_dp_peer->rx_tid[tid];

		if (!rx_tid->active)
			continue;

		/* Drop in-flight reassembly fragments (safe under dp_lock) */
		ath12k_dp_rx_frags_cleanup(rx_tid, true);

		/*
		 * Intentionally do NOT clear the REO LUT entry here.
		 * The Serving AP peer's LUT must remain valid through Phase B
		 * so that DL frames arriving on the primary link are still
		 * routed to the (now-parked) REO queue descriptor.
		 * The LUT will be cleared in Phase C via
		 * ath12k_wifi8_dp_smd_clear_old_peer_rx_lut().
		 */
		current_dp_peer->smd_lut_active_tids |= BIT(tid);
	}

	rx_tid = &current_dp_peer->rx_tid[0];
	rx_tid->smd_ctx = &parked->flush_done;

	memset(&cmd, 0, sizeof(cmd));
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS | HAL_REO_CMD_FLG_FLUSH_ALL |
		   HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS;

	ret = ath12k_wifi8_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_FLUSH_CACHE,
					   &cmd,
					   ath12k_wifi8_dp_smd_rx_flush_done);
	if (ret) {
		ath12k_warn(ab, "smd prep: FLUSH_CACHE send failed (%d)\n", ret);
		rx_tid->smd_ctx = NULL;
		spin_unlock_bh(&dp->dp_lock);
		goto err_free;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.flag = HAL_REO_CMD_FLG_UNBLK_CACHE;

	ret = ath12k_wifi8_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_UNBLOCK_CACHE,
					   &cmd, NULL);
	if (ret)
		ath12k_warn(ab, "smd prep: UNBLOCK_CACHE failed (%d)\n", ret);

	spin_unlock_bh(&dp->dp_lock);

	timeout = wait_for_completion_timeout(&parked->flush_done,
					      msecs_to_jiffies(500));
	if (!timeout) {
		ath12k_warn(ab, "smd prep: FLUSH_CACHE timed out\n");
		/* Prevent stale pointer access if callback fires late */
		spin_lock_bh(&dp->dp_lock);
		current_dp_peer->rx_tid[0].smd_ctx = NULL;
		spin_unlock_bh(&dp->dp_lock);
		ret = -ETIMEDOUT;
		goto err_free;
	}

	spin_lock_bh(&dp->dp_lock);

	for (tid = 0; tid < parked->num_tids; tid++) {
		rx_tid = &current_dp_peer->rx_tid[tid];

		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd prep: park tid=%u peer_id=%u active=%d paddr=%pad ba_win=%u cur_sn=%u\n",
			   tid, current_dp_peer->peer_id,
			   rx_tid->active, &rx_tid->paddr,
			   rx_tid->ba_win_sz, rx_tid->cur_sn);

		/* Snapshot the full rx_tid state (vaddr/paddr ownership xfer) */
		memcpy(&parked->rx_tid[tid], rx_tid, sizeof(*rx_tid));
		parked->rx_tid[tid].smd_ctx = NULL;

		/* Invalidate old peer's entry — DMA memory ownership transferred */
		rx_tid->active            = false;
		rx_tid->vaddr             = NULL;
		rx_tid->paddr             = 0;
		rx_tid->size              = 0;
		rx_tid->pending_desc_size = 0;
		rx_tid->smd_ctx           = NULL;
		rx_tid->desc		  = NULL;
	}

	/* Transfer MMIC crypto context ownership */
	parked->tfm_mmic = current_dp_peer->tfm_mmic;
	current_dp_peer->tfm_mmic = NULL;
	current_dp_peer->primary_link_frag_setup = false;

	parked->valid = true;

	spin_unlock_bh(&dp->dp_lock);

	for (tid = 0; tid < parked->num_tids; tid++) {
		if (current_dp_peer->smd_lut_active_tids & BIT(tid))
			del_timer_sync(&current_dp_peer->rx_tid[tid].frag_timer);
	}

	spin_lock_bh(&hw_grp->smd_transition_lock);

	if (hw_grp->smd_parked_rx_info) {
		ath12k_warn(ab,
			    "smd prep: overwriting stale smd_parked_rx_info!\n");
		kfree(hw_grp->smd_parked_rx_info);
	}
	hw_grp->smd_parked_rx_info = parked;

	spin_unlock_bh(&hw_grp->smd_transition_lock);

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "smd prep: parked rx_tid state peer_id=%u num_tids=%u\n",
		   current_dp_peer->peer_id, parked->num_tids);
	return 0;

err_free:
	kfree(parked);
	return ret;
}

int ath12k_wifi8_dp_smd_exec_rx_tid(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    struct ieee80211_vif *vif,
				    const u8 *addr)
{
	struct ath12k_dp_peer *target_dp_peer;
	struct ath12k_dp_hw_group *hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_smd_parked_rx_info *parked;
	struct ath12k_dp_rx_tid *src_tid, *dst_tid;
	struct ath12k_base *ab = dp->ab;
	int tid;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	target_dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	if (!target_dp_peer) {
		ath12k_warn(ab, "smd exec_rx_tid: peer %pM not found\n", addr);
		return -ENOENT;
	}

	/* Retrieve and consume the parked state */
	spin_lock_bh(&hw_grp->smd_transition_lock);
	parked = hw_grp->smd_parked_rx_info;
	hw_grp->smd_parked_rx_info = NULL;
	spin_unlock_bh(&hw_grp->smd_transition_lock);

	if (!parked || !parked->valid) {
		ath12k_warn(ab, "smd exec: no valid smd_parked_rx_info\n");
		kfree(parked);
		return -ENOENT;
	}

	spin_lock_bh(&dp->dp_lock);

	for (tid = 0; tid < parked->num_tids; tid++) {
		src_tid = &parked->rx_tid[tid];
		dst_tid = &target_dp_peer->rx_tid[tid];

		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd exec: tid=%u src: act=%d ba=%u sn=%u paddr=%pad | dst: act=%d paddr=%pad id=%u\n",
			   tid,
			   src_tid->active, src_tid->ba_win_sz, src_tid->cur_sn,
			   &src_tid->paddr,
			   dst_tid->active, &dst_tid->paddr,
			   target_dp_peer->peer_id);

		if (!src_tid->active)
			continue;

		WARN_ON(dst_tid->active && dst_tid->cur_sn != 0);
		if (dst_tid->vaddr) {
			ath12k_core_dma_unmap_single(ab->dev, dst_tid->paddr,
						     dst_tid->size,
						     DMA_BIDIRECTIONAL);
			kfree(dst_tid->vaddr);
			dst_tid->vaddr = NULL;
			dst_tid->paddr = 0;
		}

		dst_tid->tid               = src_tid->tid;
		dst_tid->vaddr             = src_tid->vaddr;
		dst_tid->paddr             = src_tid->paddr;
		dst_tid->size              = src_tid->size;
		dst_tid->pending_desc_size = src_tid->pending_desc_size;
		dst_tid->ba_win_sz         = src_tid->ba_win_sz;
		dst_tid->active            = true;
		dst_tid->cur_sn            = src_tid->cur_sn;
		dst_tid->last_frag_no      = src_tid->last_frag_no;
		dst_tid->rx_frag_bitmap    = src_tid->rx_frag_bitmap;
		dst_tid->desc		   = src_tid->desc;
		dst_tid->dp                = dp;
		dst_tid->smd_ctx           = NULL;

		timer_setup(&dst_tid->frag_timer, ath12k_dp_rx_frag_timer, 0);
		skb_queue_head_init(&dst_tid->rx_frags);

		if (ab->hw_params->reoq_lut_support) {
			ath12k_wifi8_peer_rx_tid_qref_setup(ab,
							    target_dp_peer->peer_id,
							    tid,
							    dst_tid->paddr);
		}

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "smd exec: restored rx_tid[%u] paddr=%pad ba_win=%u active=%d peer_id=%u\n",
			   tid, &dst_tid->paddr, dst_tid->ba_win_sz,
			   dst_tid->active, target_dp_peer->peer_id);
	}

	if (target_dp_peer->tfm_mmic) {
		ath12k_warn(ab,
			    "smd exec: target peer already has tfm_mmic — replacing\n");
		crypto_free_shash(target_dp_peer->tfm_mmic);
	}
	target_dp_peer->tfm_mmic = parked->tfm_mmic;
	parked->tfm_mmic = NULL;

	target_dp_peer->primary_link_frag_setup = true;

	spin_unlock_bh(&dp->dp_lock);

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "smd exec: rx_tid state restored to peer_id=%u for tid_num: %d\n",
		   target_dp_peer->peer_id,
		   parked->num_tids);

	kfree(parked);

	return 0;
}

void ath12k_wifi8_dp_smd_clear_old_peer_rx_lut(struct ath12k_dp *dp,
					       struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_base *ab = dp->ab;
	u32 tids = dp_peer->smd_lut_active_tids;
	int tid;

	lockdep_assert_held(&dp->dp_lock);

	if (!tids)
		return;

	if (!ab->hw_params->reoq_lut_support) {
		dp_peer->smd_lut_active_tids = 0;
		return;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "smd phase-c: clearing REO LUT for old peer_id=%u tids=0x%x\n",
			 dp_peer->peer_id, tids);

	for_each_set_bit(tid, (unsigned long *)&tids,
			 ab->hal.hal_params->num_tids) {
		ath12k_wifi8_peer_rx_tid_qref_reset(ab, dp_peer->peer_id, tid);
	}

	ath12k_wifi8_hal_reo_shared_qaddr_cache_clear(ab);
	dp_peer->smd_lut_active_tids = 0;
}
