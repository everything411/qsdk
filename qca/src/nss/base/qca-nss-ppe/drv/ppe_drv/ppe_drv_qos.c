/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>

#include <fal/fal_pon.h>
#include <fal/fal_qm.h>
#include <fal/fal_qos.h>
#include <fal/fal_shaper.h>
#include <fal/fal_bm.h>

#include "ppe_drv.h"

/*
 * Token number is assigned the max value so as to
 * avoid the packet loss at the start of shaper process.
 */
#define PPE_DRV_QOS_TOKEN_MAX		0x3fffffff

/*
 * Frame mode.
 */
enum ppe_drv_qos_frame_mode {
	PPE_DRV_QOS_FRAME_MODE_IPG_PREAMBLE_FRAME_CRC,	/* IPG + Preamble + Frame + CRC */
	PPE_DRV_QOS_FRAME_MODE_FRAME_CRC,	/* Frame + CRC */
	PPE_DRV_QOS_FRAME_MODE_L3_EXCLUDE_CRC,	/* after Ethernet type exclude CRC*/
	PPE_DRV_QOS_FRAME_MODE_MAX		/* Maximum frame mode */
};
typedef enum ppe_drv_qos_frame_mode ppe_drv_qos_frame_mode_t;

/*
 * ppe_drv_qos_int_pri_callback_unregister()
 *	API to unregister INT-PRI fetch callback.
 */
void ppe_drv_qos_int_pri_callback_unregister()
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->int_pri_get_cb = NULL;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_qos_int_pri_callback_unregister);

/*
 * ppe_drv_qos_queue_info_callback_unregister()
 *	API to unregister PPE-QUEUE fetch callback.
 */
void ppe_drv_qos_queue_info_callback_unregister()
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->queue_info_get_cb = NULL;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_qos_queue_info_callback_unregister);

/*
 * ppe_drv_qos_queue_info_callback_register()
 *	API to register QUEUE INFO fetch callback.
 */
void ppe_drv_qos_queue_info_callback_register(ppe_drv_qos_queue_info_callback_t cb)
{
	struct ppe_drv *p = ppe_drv_gbl;

	/*
	 * TODO: Return error in case cb is NULL
	 */
	spin_lock_bh(&p->lock);
	p->queue_info_get_cb = cb;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_qos_queue_info_callback_register);


/*
 * ppe_drv_qos_int_pri_callback_register()
 *	API to register INT-PRI fetch callback.
 */
void ppe_drv_qos_int_pri_callback_register(ppe_drv_qos_int_pri_callback_t cb)
{
	struct ppe_drv *p = ppe_drv_gbl;

	/*
	 * TODO: Return error in case cb is NULL
	 */
	spin_lock_bh(&p->lock);
	p->int_pri_get_cb = cb;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_qos_int_pri_callback_register);


/*
 * ppe_drv_qos_queue_info_get()
 *	Updates the the INT-PRI and unicast PPE queue ID for the given qdisc handle ID or leaf class ID.
 */
bool ppe_drv_qos_queue_info_get(struct net_device *dev, uint32_t handle_id, struct ppe_drv_queue_info* pq_info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	ppe_drv_qos_queue_info_callback_t cb;
	int8_t ret = 0;

	/*
	 * Read the callback pointer under p->lock for visibility, then release
	 * before invoking. This avoids an ABBA deadlock with the stats workqueue
	 * path (ppe_qdisc_stats_get_node) which holds cursor->lock and then
	 * acquires p->lock inside ppe_drv_qos_queue_stats_get. If we held p->lock
	 * here while the callback acquires cursor->lock, the two lock orders
	 * would be inverted causing a deadlock.
	 */
	spin_lock_bh(&p->lock);
	cb = p->queue_info_get_cb;
	spin_unlock_bh(&p->lock);

	if (cb) {
		ret = cb(dev, handle_id, pq_info);
	}

	ppe_drv_info("%px:get int_pri:%d successful for dev:%px handle_id:%u", p, pq_info->int_pri, dev, handle_id);
	return ret;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_info_get);

/*
 * ppe_drv_qos_int_pri_get()
 *	Returns the INT-PRI value for a qdisc handle ID or leaf class ID.
  */
int ppe_drv_qos_int_pri_get(struct net_device *dev, uint32_t handle_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	ppe_drv_qos_int_pri_callback_t cb;
	uint8_t int_pri = 0;

	/*
	 * Same lock ordering fix as ppe_drv_qos_queue_info_get: read the
	 * callback pointer under p->lock, then release before calling to
	 * avoid ABBA deadlock with the stats workqueue path.
	 */
	spin_lock_bh(&p->lock);
	cb = p->int_pri_get_cb;
	spin_unlock_bh(&p->lock);

	if (cb) {
		int_pri = cb(dev, handle_id);
	}

	ppe_drv_info("%px:get int_pri:%d successful for dev:%px handle_id:%u", p, int_pri, dev, handle_id);
	return int_pri;
}
EXPORT_SYMBOL(ppe_drv_qos_int_pri_get);

/*
 * ppe_drv_qos_queue_stats_get()
 *	API to fetch queue statistics from PPE HW.
 */
void ppe_drv_qos_queue_stats_get(uint32_t qid, bool is_red, struct ppe_drv_qos_q_stat *stats)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_queue_stats_t info;

	spin_lock_bh(&p->lock);
	fal_queue_counter_get(PPE_DRV_SWITCH_ID, qid, &info);
	spin_unlock_bh(&p->lock);

	stats->tx_pkts = info.tx_packets;
	stats->tx_bytes = info.tx_bytes;

	stats->drop_pkts = info.drop_packets[PPE_DRV_QOS_QUEUE_COLOR_GREEN + 3];
	stats->drop_bytes = info.drop_bytes[PPE_DRV_QOS_QUEUE_COLOR_GREEN + 3];

	if (is_red) {
		stats->drop_pkts += info.drop_packets[PPE_DRV_QOS_QUEUE_COLOR_GREEN];
		stats->drop_bytes += info.drop_bytes[PPE_DRV_QOS_QUEUE_COLOR_GREEN];
	}

	ppe_drv_info("%px:get queue stats successful for qid:%u", p, qid);
}
EXPORT_SYMBOL(ppe_drv_qos_queue_stats_get);

/*
 * ppe_drv_qos_queue_stats_reset()
 *	API to reset queue statistics in PPE HW.
 */
void ppe_drv_qos_queue_stats_reset(uint32_t qid)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	fal_queue_counter_cleanup(PPE_DRV_SWITCH_ID, qid);
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:reset queue stats successful for qid:%u", p, qid);
}
EXPORT_SYMBOL(ppe_drv_qos_queue_stats_reset);

/*
 * ppe_drv_qos_queue_enqueue_ctrl()
 *	Controls enqueue operation for a queue in PPE HW.
 */
ppe_drv_ret_t ppe_drv_qos_queue_enqueue_ctrl(uint32_t qid, bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	if (fal_qm_enqueue_ctrl_set(PPE_DRV_SWITCH_ID, qid, enable ? A_TRUE : A_FALSE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue enqueue control failed for qid:%u enable:%d", p, qid, enable);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:queue enqueue control successful for qid:%u enable:%d", p, qid, enable);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_enqueue_ctrl);

/*
 * ppe_drv_qos_queue_dequeue_ctrl()
 *	Controls dequeue operation for a queue in PPE HW.
 */
ppe_drv_ret_t ppe_drv_qos_queue_dequeue_ctrl(uint32_t qid, bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	if (fal_scheduler_dequeue_ctrl_set(PPE_DRV_SWITCH_ID, qid, enable ? A_TRUE : A_FALSE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue dequeue control failed for qid:%u enable:%d", p, qid, enable);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:queue dequeue control successful for qid:%u enable:%d", p, qid, enable);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_dequeue_ctrl);

/*
 * ppe_drv_qos_queue_dequeue_drop_ctrl()
 *	Controls dequeue drop operation for a queue in PPE HW.
 */
ppe_drv_ret_t ppe_drv_qos_queue_dequeue_drop_ctrl(uint32_t qid, bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);

	if (fal_qm_dequeue_drop_set(PPE_DRV_SWITCH_ID, qid, enable ? A_TRUE : A_FALSE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue dequeue drop control failed for qid:%u enable:%d", p, qid, enable);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:queue dequeue drop control successful for qid:%u enable:%d", p, qid, enable);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_dequeue_drop_ctrl);

/*
 * ppe_drv_qos_res_queue_disable()
 *	Disables a queue in PPE HW.
 */
ppe_drv_ret_t ppe_drv_qos_queue_disable(uint32_t port_id, uint32_t qid)
{
	struct ppe_drv *p = ppe_drv_gbl;

	/*
	 * Disable queue enqueue, dequeue and flush the queue.
	 */
	spin_lock_bh(&p->lock);
	if (fal_qm_enqueue_ctrl_set(PPE_DRV_SWITCH_ID, qid, A_FALSE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue enqueue failed for port:%u qid:%u", p, port_id, qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	if (fal_scheduler_dequeue_ctrl_set(PPE_DRV_SWITCH_ID, qid, A_FALSE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue dequeue failed for port:%u qid:%u", p, port_id, qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	if (fal_queue_flush(PPE_DRV_SWITCH_ID, port_id, qid) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue flush failed for port:%u qid:%u", p, port_id, qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:disable level0 queue scheduler successful for port:%u qid:%u", p, port_id, qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_disable);

/*
 * ppe_drv_qos_queue_enable()
 *	Enables a queue in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_queue_enable(uint32_t qid)
{
	struct ppe_drv *p = ppe_drv_gbl;

	/*
	 * Enable queue enqueue and dequeue.
	 */
	spin_lock_bh(&p->lock);
	if (fal_qm_enqueue_ctrl_set(PPE_DRV_SWITCH_ID, qid, A_TRUE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue enqueue failed for qid:%u", p, qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	if (fal_scheduler_dequeue_ctrl_set(PPE_DRV_SWITCH_ID, qid, A_TRUE) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue dequeue failed for qid:%u", p, qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:enable SSDK level0 queue scheduler successful for qid:%u", p, qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_enable);

/*
 * ppe_drv_qos_tcont_stats_get()
 *	API to fetch tcont statistics from PPE HW.
 */
ppe_drv_ret_t ppe_drv_qos_tcont_stats_get(uint32_t tcont_id, struct ppe_drv_qos_tcont_stat *stats)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_queue_tcont_stat_t info;

	spin_lock_bh(&p->lock);
	if (fal_qm_tcont_stat_get(PPE_DRV_SWITCH_ID, tcont_id, &info) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:tcont stats get failed for tcont:%u", p, tcont_id);
		return PPE_DRV_RET_QOS_TCONT_STATS_FAIL;
	}

	spin_unlock_bh(&p->lock);
	stats->pend_bytes = info.bytes;
	stats->credit = info.credit;

	ppe_drv_info("%px:get tcont stats get successful for tcont_id:%u", p, tcont_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_tcont_stats_get);

/*
 * ppe_drv_qos_tcont_credit_reset()
 *	Resets T-cont credit to 0.
 */
ppe_drv_ret_t ppe_drv_qos_tcont_credit_reset(uint32_t tcont_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_queue_tcont_stat_t stats = {0};

	spin_lock_bh(&p->lock);
	if (fal_qm_tcont_stat_set(PPE_DRV_SWITCH_ID, tcont_id, &stats) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:tcont stats resetting failed for tcont:%u", p, tcont_id);
		return PPE_DRV_RET_QOS_TCONT_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:tcont stats resetting successful for tcont:%u", p, tcont_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_tcont_credit_reset);

/*
 * ppe_drv_qos_tcont_set()
 *	Sets T-cont configuration for a queue in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_tcont_set(struct ppe_drv_qos_res *res, uint32_t tcont_id, bool valid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_queue_tcont_cfg_t cfg = {0};

	spin_lock_bh(&p->lock);
	cfg.tcont_id = tcont_id;
	cfg.valid = valid;

	ppe_drv_trace("%px:tcont configuration: queue_id:%u, tcont_id:%u, tcont_valid=%u",
			p, res->q.ucast_qid, tcont_id, valid);
	if (fal_qm_tcont_set(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:tcont configuration failed for tcont:%u", p, tcont_id);
		return PPE_DRV_RET_QOS_TCONT_CFG_FAIL;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:tcont configuration successful for tcont:%u", p, tcont_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_tcont_set);

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_drv_qos_pon_port_alloc_callback_unregister()
 *	API to unregister PON port QoS resource allocation callback.
 */
void ppe_drv_qos_pon_port_alloc_callback_unregister(void)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->pon_port_alloc_cb = NULL;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_qos_pon_port_alloc_callback_unregister);

/*
 * ppe_drv_qos_pon_port_alloc_callback_register()
 *	API to register PON port QoS resource allocation callback.
 */
void ppe_drv_qos_pon_port_alloc_callback_register(ppe_drv_qos_pon_port_alloc_callback_t cb)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->pon_port_alloc_cb = cb;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_qos_pon_port_alloc_callback_register);

/*
 * ppe_drv_qos_pon_port_get()
 *	Gets the PON port ID.
 */
uint32_t ppe_drv_qos_pon_port_get(void)
{
	struct ppe_drv *p __maybe_unused = ppe_drv_gbl;
	struct ppe_drv_iface *pon_iface = NULL;
	struct ppe_drv_port *pon_port = NULL;

	pon_iface = ppe_drv_pon_iface_get();

	if (!pon_iface) {
		ppe_drv_trace("%px:PON interface not present", p);
		return 0;
	}

	pon_port = pon_iface->port;
	if (!pon_port) {
		ppe_drv_trace("%px:PON port not present", p);
		return 0;
	}

	return pon_port->port;
}
EXPORT_SYMBOL(ppe_drv_qos_pon_port_get);
#endif

/*
 * ppe_drv_qos_l1_scheduler_set()
 *	Configures Level 1 scheduler in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_l1_scheduler_set(struct ppe_drv_qos_res *res, uint32_t port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_qos_scheduler_cfg_t l1cfg = {0};

	if (res->scheduler.drr_weight >= PPE_DRV_QOS_DRR_WEIGHT_MAX) {
		ppe_drv_warn("%px:drr weight:%u should be less than 1024",
				p, res->scheduler.drr_weight);
		return PPE_DRV_RET_QOS_SCHEDULER_CFG_FAIL;
	}

	spin_lock_bh(&p->lock);
	l1cfg.sp_id = res->l1spid;

	l1cfg.c_drr_wt = res->scheduler.drr_weight ? res->scheduler.drr_weight : 1;
	l1cfg.c_drr_unit = res->scheduler.drr_unit;
	l1cfg.e_drr_wt = res->scheduler.drr_weight ? res->scheduler.drr_weight : 1;
	l1cfg.e_drr_unit = res->scheduler.drr_unit;
	l1cfg.c_pri = res->scheduler.priority;
	l1cfg.e_pri = res->scheduler.priority;
	l1cfg.c_drr_id = res->scheduler.l1c_drrid;
	l1cfg.e_drr_id = res->scheduler.l1e_drrid;
	l1cfg.drr_frame_mode = (fal_qos_drr_frame_mode_t)PPE_DRV_QOS_FRAME_MODE_FRAME_CRC;

	ppe_drv_trace("%px:level1 configuration: port:%u, l0spid:%u, l1spid:%u, c_drrid:%u, c_pri:%u, c_drr_wt:%u, e_drrid:%u, e_pri:%u, e_drr_wt:%u, l1spid:%u",
			p, port_id, res->l0spid, res->l1spid, l1cfg.c_drr_id, l1cfg.c_pri, l1cfg.c_drr_wt, l1cfg.e_drr_id, l1cfg.e_pri, l1cfg.e_drr_wt, l1cfg.sp_id);
	if (fal_queue_scheduler_set(PPE_DRV_SWITCH_ID, res->l0spid, PPE_DRV_QOS_FLOW_LEVEL - 1, port_id, &l1cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:level1 queue scheduler configuration failed for port:%u", p, port_id);
		return PPE_DRV_RET_QOS_SCHEDULER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:level1 queue scheduler configuration successful for port:%u", p, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_l1_scheduler_set);

/*
 * ppe_drv_qos_l0_scheduler_set()
 *	Configures a level 0 queue scheduler in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_l0_scheduler_set(struct ppe_drv_qos_res *res, uint32_t port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_qos_scheduler_cfg_t l0cfg = {0};

	if (res->scheduler.drr_weight >= PPE_DRV_QOS_DRR_WEIGHT_MAX) {
		ppe_drv_warn("%px:drr weight:%u should be less than 1024", p, res->scheduler.drr_weight);
		return PPE_DRV_RET_QOS_SCHEDULER_CFG_FAIL;
	}

	spin_lock_bh(&p->lock);
	l0cfg.sp_id = res->l0spid;
	l0cfg.c_drr_wt = res->scheduler.drr_weight ? res->scheduler.drr_weight : 1;
	l0cfg.c_drr_unit = res->scheduler.drr_unit;
	l0cfg.e_drr_wt = res->scheduler.drr_weight ? res->scheduler.drr_weight : 1;
	l0cfg.e_drr_unit = res->scheduler.drr_unit;
	l0cfg.c_pri = res->scheduler.priority;
	l0cfg.e_pri = res->scheduler.priority;
	l0cfg.c_drr_id = res->scheduler.l0c_drrid;
	l0cfg.e_drr_id = res->scheduler.l0e_drrid;
	l0cfg.drr_frame_mode = (fal_qos_drr_frame_mode_t)PPE_DRV_QOS_FRAME_MODE_FRAME_CRC;

	ppe_drv_trace("%px:level0 configuration: port:%u, ucast_qid:%u, c_drrid:%u, c_pri:%u, c_drr_wt:%u, e_drrid:%u, e_pri:%u, e_drr_wt:%u, l0spid:%u",
			p, port_id, res->q.ucast_qid, l0cfg.c_drr_id, l0cfg.c_pri, l0cfg.c_drr_wt, l0cfg.e_drr_id, l0cfg.e_pri, l0cfg.e_drr_wt, l0cfg.sp_id);
	if (fal_queue_scheduler_set(PPE_DRV_SWITCH_ID, res->q.ucast_qid, PPE_DRV_QOS_QUEUE_LEVEL - 1, port_id, &l0cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:level0 queue scheduler configuration failed for port:%u", p, port_id);
		return PPE_DRV_RET_QOS_SCHEDULER_CFG_FAIL;
	}

	/*
	 * This function is called at the time of queue qdisc allocation at all
	 * levels and class allocation at PPE_DRV_QOS_QUEUE_LEVEL.
	 * Also invoked at time of change class at PPE_DRV_QOS_QUEUE_LEVEL and
	 * PPE_DRV_QOS_SUB_QUEUE_LEVEL levels. But, mcast will be valid only in
	 * case queue is attached at PPE_DRV_QOS_SUB_QUEUE_LEVEL and change class
	 * is invoked at PPE_DRV_QOS_QUEUE_LEVEL level.
	 * Other scenarios catering to mcast queue are handled in separate mcast APIs.
	 */
	if (res->q.mcast_qid) {
		ppe_drv_trace("%px:level0 configuration: port:%u, mcast_qid:%u, c_drrid:%u, c_pri:%u, c_drr_wt:%u, e_drrid:%u, e_pri:%u, e_drr_wt:%u, l0spid:%u",
				p, port_id, res->q.mcast_qid, l0cfg.c_drr_id, l0cfg.c_pri, l0cfg.c_drr_wt, l0cfg.e_drr_id, l0cfg.e_pri, l0cfg.e_drr_wt, l0cfg.sp_id);
		if (fal_queue_scheduler_set(PPE_DRV_SWITCH_ID, res->q.mcast_qid, PPE_DRV_QOS_QUEUE_LEVEL - 1, port_id, &l0cfg) != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%px:level0 multicast queue scheduler configuration failed for port:%u", p, port_id);
			return PPE_DRV_RET_QOS_SCHEDULER_CFG_FAIL;
		}
	}

	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px:level0 queue scheduler configuration successful for port:%u", p, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_l0_scheduler_set);

/*
 * ppe_drv_qos_port_shaper_reset()
 *	Resets a port shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_port_shaper_reset(struct ppe_drv_qos_res *res, uint32_t port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_config_t cfg = {0};

	spin_lock_bh(&p->lock);
	ppe_drv_trace("%px:port shaper reset for port:%u", p, port_id);
	if (fal_port_shaper_set(PPE_DRV_SWITCH_ID, port_id, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:port shaper configuration failed for port:%u",
				p, port_id);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px: port shaper configuration successful for port:%u", p, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_port_shaper_reset);

/*
 * ppe_drv_qos_port_shaper_set()
 *	Configures a port shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_port_shaper_set(struct ppe_drv_qos_res *res, uint32_t port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_token_number_t token = {0};
	fal_shaper_config_t cfg = {0};

	/*
	 * Set port shaper token number
	 */
	spin_lock_bh(&p->lock);
	token.c_token_number = PPE_DRV_QOS_TOKEN_MAX;
	token.e_token_number = PPE_DRV_QOS_TOKEN_MAX;

	ppe_drv_trace("%px:port token set for port:%u, c_token_number:%x, e_token_number:%x",
		p, port_id, token.c_token_number, token.e_token_number);
	if (fal_port_shaper_token_number_set(PPE_DRV_SWITCH_ID, port_id, &token) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:port shaper token configuration failed for port:%u",
				p, port_id);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	/*
	 * Set port shaper configuration
	 * Note: SSDK API requires burst in bytes/sec
	 * while rate in kbits/sec.
	 */
	cfg.c_shaper_en = 1;
	cfg.cbs = res->shaper.cburst;
	cfg.cir = (res->shaper.crate / 1000) * 8;
	cfg.shaper_frame_mode = (fal_shaper_frame_mode_t)PPE_DRV_QOS_FRAME_MODE_FRAME_CRC;

	ppe_drv_trace("%px:port shaper configuration for port:%u, couple_en:%u, meter_unit:%u, c_shaper_en:%u, cbs:%u, cir:%u, ebs:%u, eir:%u, shaper_frame_mode:%u",
			p, port_id, cfg.couple_en, cfg.meter_unit, cfg.c_shaper_en, cfg.cbs, cfg.cir, cfg.ebs, cfg.eir, cfg.shaper_frame_mode);
	if (fal_port_shaper_set(PPE_DRV_SWITCH_ID, port_id, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:port shaper configuration failed for port:%u",
				p, port_id);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:port shaper configuration successful for port:%u", p, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_port_shaper_set);

/*
 * ppe_drv_qos_flow_shaper_reset()
 *	Resets a flow shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_flow_shaper_reset(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_config_t cfg = {0};

	/*
	 * Reset flow shaper configuration
	 */
	spin_lock_bh(&p->lock);
	ppe_drv_trace("%px:flow shaper reset for l0spid:%u", p, res->l0spid);
	if (fal_flow_shaper_set(PPE_DRV_SWITCH_ID, res->l0spid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:flow shaper configuration failed for l0spid:%u",
			p, res->l0spid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:flow shaper configuration successful for l0spid:%u", p, res->l0spid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_flow_shaper_reset);

/*
 * ppe_drv_qos_flow_shaper_set()
 *	Configures a flow shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_flow_shaper_set(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_token_number_t token = {0};
	fal_shaper_config_t cfg = {0};

	/*
	 * Set flow shaper token number
	 */
	spin_lock_bh(&p->lock);
	token.c_token_number = PPE_DRV_QOS_TOKEN_MAX;
	token.e_token_number = PPE_DRV_QOS_TOKEN_MAX;

	ppe_drv_trace("%px:flow token set for l0spid:%u, c_token_number:%x, e_token_number:%x",
		p, res->l0spid, token.c_token_number, token.e_token_number);
	if (fal_flow_shaper_token_number_set(PPE_DRV_SWITCH_ID, res->l0spid, &token) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:flow shaper token configuration failed for l0spid:%u",
			p, res->l0spid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	/*
	 * Set flow shaper token configuration
	 * Note: SSDK API requires burst in bytes/sec
	 * while rate in kbits/sec.
	 */
	cfg.c_shaper_en = 1;
	cfg.cbs = res->shaper.burst;
	cfg.cir = (res->shaper.rate / 1000) * 8;
	cfg.e_shaper_en = 1;
	cfg.ebs = res->shaper.cburst;
	cfg.eir = ((res->shaper.crate / 1000) * 8) - cfg.cir;
	cfg.shaper_frame_mode = (fal_shaper_frame_mode_t)PPE_DRV_QOS_FRAME_MODE_FRAME_CRC;

	ppe_drv_trace("%px:flow shaper configuration: l0spid:%u, couple_en:%u, meter_unit:%u, c_shaper_en:%u, cbs:%u, cir:%u, ebs:%u, eir:%u, shaper_frame_mode:%u",
		p, res->l0spid, cfg.couple_en, cfg.meter_unit, cfg.c_shaper_en, cfg.cbs, cfg.cir, cfg.ebs, cfg.eir, cfg.shaper_frame_mode);
	if (fal_flow_shaper_set(PPE_DRV_SWITCH_ID, res->l0spid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:flow shaper configuration failed for l0spid:%u",
			p, res->l0spid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:flow shaper configuration successful for l0spid:%u", p, res->l0spid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_flow_shaper_set);

/*
 * ppe_drv_qos_mcast_queue_shaper_reset()
 *	Resets a multicast queue shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_mcast_queue_shaper_reset(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_config_t cfg = {0};

	spin_lock_bh(&p->lock);
	ppe_drv_trace("%px:multicast queue shaper reset for mcast_qid:%u", p, res->q.mcast_qid);
	if (fal_queue_shaper_set(PPE_DRV_SWITCH_ID, res->q.mcast_qid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:multicast queue shaper configuration failed for mcast_qid:%u",
			p, res->q.mcast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:multicast queue reset successful for mcast_qid:%u", p, res->q.mcast_qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_mcast_queue_shaper_reset);

/*
 * ppe_drv_qos_mcast_queue_shaper_set()
 *	Configures a multicast queue shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_mcast_queue_shaper_set(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_token_number_t token = {0};
	fal_shaper_config_t cfg = {0};

	spin_lock_bh(&p->lock);
	token.c_token_number = PPE_DRV_QOS_TOKEN_MAX;
	token.e_token_number = PPE_DRV_QOS_TOKEN_MAX;

	ppe_drv_trace("%px:multicast queue token set for mcast_qid:%u, c_token_number:%x, e_token_number:%x",
		p, res->q.mcast_qid, token.c_token_number, token.e_token_number);
	if (fal_queue_shaper_token_number_set(PPE_DRV_SWITCH_ID, res->q.mcast_qid, &token) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:multicast queue shaper token configuration failed for mcast_qid:%u", p, res->q.mcast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	if (fal_queue_shaper_get(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue shaper configuration fetch failed for ucast_qid:%u", p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	ppe_drv_trace("%px:multicast queue shaper configuration for mcast_qid:%u, couple_en:%u, meter_unit:%u, c_shaper_en:%u, cbs:%u, cir:%u, ebs:%u, eir:%u, shaper_frame_mode:%u",
		p, res->q.mcast_qid, cfg.couple_en, cfg.meter_unit, cfg.c_shaper_en, cfg.cbs, cfg.cir, cfg.ebs, cfg.eir, cfg.shaper_frame_mode);
	if (fal_queue_shaper_set(PPE_DRV_SWITCH_ID, res->q.mcast_qid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:multicast queue shaper configuration failed for mcast_qid:%u", p, res->q.mcast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:queue shaper configuration successful for mcast_qid:%u", p, res->q.mcast_qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_mcast_queue_shaper_set);

/*
 * ppe_drv_qos_queue_shaper_reset()
 *	Resets a queue shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_queue_shaper_reset(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_config_t cfg = {0};

	/*
	 * Reset queue shaper configuration
	 */
	spin_lock_bh(&p->lock);
	ppe_drv_trace("%px:queue shaper reset for ucast_qid:%u",
			p, res->q.ucast_qid);
	if (fal_queue_shaper_set(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue shaper configuration failed for ucast_qid:%u",
			p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:queue shaper configuration successful for ucast_qid:%u", p, res->q.ucast_qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_shaper_reset);

/*
 * ppe_drv_qos_queue_shaper_set()
 *	Configures a queue shaper in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_queue_shaper_set(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_shaper_token_number_t token = {0};
	fal_shaper_config_t cfg = {0};

	/*
	 * Set queue shaper token number
	 */
	spin_lock_bh(&p->lock);
	token.c_token_number = PPE_DRV_QOS_TOKEN_MAX;
	token.e_token_number = PPE_DRV_QOS_TOKEN_MAX;

	ppe_drv_trace("%px:queue token set for ucast_qid:%u, c_token_number:%x, e_token_number:%x",
		p, res->q.ucast_qid, token.c_token_number, token.e_token_number);
	if (fal_queue_shaper_token_number_set(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &token) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue shaper token configuration failed for ucast_qid:%u", p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	/*
	 * Set queue shaper confguration
	 * Note: SSDK API requires burst in bytes/sec
	 * while rate in kbits/sec.
	 */
	cfg.c_shaper_en = 1;
	cfg.cbs = res->shaper.burst;
	cfg.cir = (res->shaper.rate / 1000) * 8;
	cfg.e_shaper_en = 1;
	cfg.ebs = res->shaper.cburst;
	cfg.eir = ((res->shaper.crate / 1000) * 8) - cfg.cir;
	cfg.shaper_frame_mode = (fal_shaper_frame_mode_t)PPE_DRV_QOS_FRAME_MODE_FRAME_CRC;

	ppe_drv_trace("%px:queue shaper configuration for ucast_qid:%u, couple_en:%u, meter_unit:%u, c_shaper_en:%u, cbs:%u, cir:%u, ebs:%u, eir:%u, shaper_frame_mode:%u",
		p, res->q.ucast_qid, cfg.couple_en, cfg.meter_unit, cfg.c_shaper_en, cfg.cbs, cfg.cir, cfg.ebs, cfg.eir, cfg.shaper_frame_mode);
	if (fal_queue_shaper_set(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue shaper configuration failed for ucast_qid:%u", p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
	}

	/*
	 * This function is called at the time of class allocation and change at
	 * PPE_DRV_QOS_QUEUE_LEVEL. But, mcast will be valid only in case queue
	 * is attached at PPE_DRV_QOS_SUB_QUEUE_LEVEL and change class is invoked
	 * at PPE_DRV_QOS_QUEUE_LEVEL level.
	 * Other scenarios catering to mcast queue are handled in separate mcast APIs.
	 */
	if (res->q.mcast_qid) {
		ppe_drv_trace("%px:multicast queue shaper configuration for mcast_qid:%u, couple_en:%u, meter_unit:%u, c_shaper_en:%u, cbs:%u, cir:%u, ebs:%u, eir:%u, shaper_frame_mode:%u",
			p, res->q.mcast_qid, cfg.couple_en, cfg.meter_unit, cfg.c_shaper_en, cfg.cbs, cfg.cir, cfg.ebs, cfg.eir, cfg.shaper_frame_mode);
		if (fal_queue_shaper_set(PPE_DRV_SWITCH_ID, res->q.mcast_qid, &cfg) != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%px:multicast queue shaper configuration failed for mcast_qid:%u", p, res->q.mcast_qid);
			return PPE_DRV_RET_QOS_SHAPER_CFG_FAIL;
		}
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:queue shaper configuration successful for mcast_qid:%u", p, res->q.mcast_qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_shaper_set);

/*
 * ppe_drv_qos_mcast_queue_set()
 *	Configures a multicast queue in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_mcast_queue_set(struct ppe_drv_qos_res *res, uint32_t port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_qos_scheduler_cfg_t l0cfg;

	spin_lock_bh(&p->lock);
	ppe_drv_trace("%px:level0 configuration get for port:%u, ucast_qid:%u", p, port_id, res->q.ucast_qid);
	if (fal_queue_scheduler_get(PPE_DRV_SWITCH_ID, res->q.ucast_qid, PPE_DRV_QOS_QUEUE_LEVEL - 1, &port_id, &l0cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:level0 unicast queue scheduler configuration fetch failed for port:%u, ucast_qid:%u", p, port_id, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	ppe_drv_trace("%px:level0 configuration: port:%u, mcast_qid:%u, c_drrid:%u, c_pri:%u, c_drr_wt:%u, e_drrid:%u, e_pri:%u, e_drr_wt:%u, l0spid:%u",
			p, port_id, res->q.mcast_qid, l0cfg.c_drr_id, l0cfg.c_pri, l0cfg.c_drr_wt, l0cfg.e_drr_id, l0cfg.e_pri, l0cfg.e_drr_wt, l0cfg.sp_id);
	if (fal_queue_scheduler_set(PPE_DRV_SWITCH_ID, res->q.mcast_qid, PPE_DRV_QOS_QUEUE_LEVEL - 1, port_id, &l0cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:level0 multicast queue scheduler configuration failed for port:%u, ucast_qid:%u", p, port_id, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:multicast queue configuration successful for port:%u, ucast_qid:%u", p, port_id, res->q.ucast_qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_mcast_queue_set);

/*
 * ppe_drv_qos_queue_limit_set()
 *	Sets queue size in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_queue_limit_set(struct ppe_drv_qos_res *res)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_ac_obj_t obj = {0};
	fal_ac_dynamic_threshold_t dynamic_cfg;
	fal_ac_static_threshold_t static_cfg;
	fal_ac_ctrl_t ctrl_cfg = {0};

	/*
	 * Unicast queue configuration.
	 */
	spin_lock_bh(&p->lock);
	obj.obj_id = res->q.ucast_qid;

	/*
	 * Enable force drop for PPE qdisc.
	 * When ac_en/ac_fc_en are set to 1, the flow control will be overriden
	 * for that queue and packets drop gets enabled.
	 */
	ctrl_cfg.ac_en = 1;
	ctrl_cfg.ac_fc_en = 1;

	if (res->q.mcast_valid) {
		goto config_mcast;
	}

	ppe_drv_trace("%px:queue flow control set for ucast_qid:%u, enable:%u", p, res->q.ucast_qid, ctrl_cfg.ac_fc_en);
	if (fal_ac_ctrl_set(PPE_DRV_SWITCH_ID, &obj, &ctrl_cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue flow control set failed for ucast_qid:%u", p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

	if (fal_ac_dynamic_threshold_get(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &dynamic_cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue dynamic threshold get failed for ucast_qid:%u", p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	} else {
		dynamic_cfg.color_enable = res->q.color_en;
		dynamic_cfg.wred_enable = res->q.red_en;
		dynamic_cfg.ceiling = res->q.qlimit;
		dynamic_cfg.green_min_off = res->q.min_th[PPE_DRV_QOS_QUEUE_COLOR_GREEN];
		dynamic_cfg.yel_min_off = res->q.min_th[PPE_DRV_QOS_QUEUE_COLOR_YELLOW];
		dynamic_cfg.yel_max_off = res->q.max_th[PPE_DRV_QOS_QUEUE_COLOR_YELLOW];
		dynamic_cfg.red_min_off = res->q.min_th[PPE_DRV_QOS_QUEUE_COLOR_RED];
		dynamic_cfg.red_max_off = res->q.max_th[PPE_DRV_QOS_QUEUE_COLOR_RED];

		/*
		 * Set resume offsets for all three colors.
		 * Resume offsets control when the queue resumes accepting packets after dropping,
		 * which helps prevent buffer oscillation.
		 */
		dynamic_cfg.green_resume_off = res->q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_GREEN];
		dynamic_cfg.yel_resume_off = res->q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_YELLOW];
		dynamic_cfg.red_resume_off = res->q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_RED];
	}

	ppe_drv_trace("%px:queue ac dynamic threshold set for ucast_qid:%u, color_en:%u, wred_enable:%u, ceiling:%u, green_min_off:%u, yel_min_off:%u, yel_max_off:%u, red_min_off:%u, red_max_off:%u, green_resume_off:%u, yel_resume_off:%u, red_resume_off:%u",
		p, res->q.ucast_qid, dynamic_cfg.color_enable, dynamic_cfg.wred_enable, dynamic_cfg.ceiling,
		dynamic_cfg.green_min_off, dynamic_cfg.yel_min_off, dynamic_cfg.yel_max_off,
		dynamic_cfg.red_min_off, dynamic_cfg.red_max_off,
		dynamic_cfg.green_resume_off, dynamic_cfg.yel_resume_off, dynamic_cfg.red_resume_off);
	if (fal_ac_dynamic_threshold_set(PPE_DRV_SWITCH_ID, res->q.ucast_qid, &dynamic_cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue dynamic threshold failed for ucast_qid:%u", p, res->q.ucast_qid);
		return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
	}

config_mcast:
	/*
	 * Multicast queue configuration.
	 */
	if (res->q.mcast_qid) {
		if (fal_ac_static_threshold_get(PPE_DRV_SWITCH_ID, &obj, &static_cfg) != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%px:queue static threshold get failed for mcast_qid:%u", p, res->q.mcast_qid);
			return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
		} else {
			obj.obj_id = res->q.mcast_qid;
			static_cfg.color_enable = res->q.color_en;
			static_cfg.wred_enable = res->q.red_en;
			static_cfg.green_min_off = res->q.min_th[PPE_DRV_QOS_QUEUE_COLOR_GREEN];
			static_cfg.yel_min_off = res->q.min_th[PPE_DRV_QOS_QUEUE_COLOR_YELLOW];
			static_cfg.yel_max_off = res->q.max_th[PPE_DRV_QOS_QUEUE_COLOR_YELLOW];
			static_cfg.red_min_off = res->q.min_th[PPE_DRV_QOS_QUEUE_COLOR_RED];
			static_cfg.red_max_off = res->q.max_th[PPE_DRV_QOS_QUEUE_COLOR_RED];

			/*
			 * Set resume offsets for all three colors.
			 * Resume offsets control when the queue resumes accepting packets after dropping,
			 * which helps prevent buffer oscillation.
			 */
			static_cfg.green_resume_off = res->q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_GREEN];
			static_cfg.yel_resume_off = res->q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_YELLOW];
			static_cfg.red_resume_off = res->q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_RED];
		}

		ppe_drv_trace("%px:multicast queue buffer set for mcast_qid:%u, qlimit:%u", p, res->q.mcast_qid, res->q.qlimit);
		if (fal_ac_prealloc_buffer_set(PPE_DRV_SWITCH_ID, &obj, res->q.qlimit) != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%px:multicast queue buffer allocation failed for mcast_qid:%u", p, res->q.mcast_qid);
			return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
		}

		ppe_drv_trace("%px:multicast queue ac threshold set for mcast_qid:%u, wred_enable:%u, green_max:%u, green_min_off:%u",
			p, res->q.mcast_qid, static_cfg.wred_enable, static_cfg.green_max, static_cfg.green_min_off);
		if (fal_ac_static_threshold_set(PPE_DRV_SWITCH_ID, &obj, &static_cfg) != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%px:multicast queue static threshold failed for mcast_qid:%u", p, res->q.mcast_qid);
			return PPE_DRV_RET_QOS_QUEUE_CFG_FAIL;
		}
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:queue configuration successful for ucast_qid:%u", p, res->q.ucast_qid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_queue_limit_set);

/*
 *ppe_drv_qos_port_bm_control_enable()
 *	Enable or disable port buffer management for flow control
 */
void ppe_drv_qos_port_bm_control_enable(uint32_t port_id, bool set)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	fal_port_bm_ctrl_set(PPE_DRV_SWITCH_ID, port_id, set);
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%px: port buffer flow control is successfully set to %d for port %u", p, set, port_id);

}
EXPORT_SYMBOL(ppe_drv_qos_port_bm_control_enable);

/*
 * ppe_drv_qos_default_conf_set()
 *	Sets default queue scheduler in PPE.
 */
ppe_drv_ret_t ppe_drv_qos_default_conf_set(uint32_t port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;

	/*
	 * Invoke SSDK API to reset the default configuration for a given port.
	 */
	spin_lock_bh(&p->lock);
	if (fal_port_scheduler_cfg_reset(PPE_DRV_SWITCH_ID, port_id) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:reset default queue configuration failed for port:%u", p, port_id);
		return PPE_DRV_RET_QOS_PORT_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_qos_port_bm_control_enable(port_id, true);
	ppe_drv_info("%px:queue configuration successful for port:%u", p, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_default_conf_set);

/*
 * ppe_drv_qos_port_res_get()
 *	Gets boot time QoS resource allocation information for a given port.
 */
ppe_drv_ret_t ppe_drv_qos_port_res_get(uint32_t port_id, struct ppe_drv_qos_port *port)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_portscheduler_resource_t cfg = {0};

	/*
	 * Get boot time QoS resource information for a given port
	 */
	spin_lock_bh(&p->lock);
	if (fal_port_scheduler_resource_get(PPE_DRV_SWITCH_ID, port_id, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:port QoS resources info failed for port:%u", p, port_id);
		return PPE_DRV_RET_FAILURE_NO_RESOURCE;
	}

	spin_unlock_bh(&p->lock);
	port->max[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE] = cfg.ucastq_num;
	port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE] = cfg.ucastq_start;

	port->max[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE] = cfg.mcastq_num;
	port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE] = cfg.mcastq_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L0_CDRR] = cfg.l0cdrr_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L0_CDRR] = cfg.l0cdrr_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L0_EDRR] = cfg.l0edrr_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L0_EDRR] = cfg.l0edrr_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L0_SP] = cfg.l0sp_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L0_SP] = cfg.l0sp_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L1_CDRR] = cfg.l1cdrr_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L1_CDRR] = cfg.l1cdrr_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L1_EDRR] = cfg.l1edrr_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L1_EDRR] = cfg.l1edrr_start;


	port->max[PPE_DRV_QOS_RES_TYPE_L1_SP] = cfg.l1sp_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L1_SP] = cfg.l1sp_start;

	ppe_drv_info("%px:port QoS resources infosuccessful for port:%u", p, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_port_res_get);

#ifdef NSS_PPE_DSA_QUEUE_SUPPORT
/*
 * ppe_drv_qos_port_htt_res_get()
 *	Gets boot-time QoS resource allocation information for a given switch slave port.
 */
ppe_drv_ret_t ppe_drv_qos_port_htt_res_get(uint32_t port_id, uint8_t index, struct ppe_drv_qos_port *port)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_portscheduler_resource_t cfg = {0};
	int num_groups;

	/*
	 * Get boot time QoS resource information for a given port.
	 */
	spin_lock_bh(&p->lock);
	if (fal_port_scheduler_resource_get(PPE_DRV_SWITCH_ID, port_id, &cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:port QoS resources info failed for port:%u", p, port_id);
		return PPE_DRV_RET_FAILURE_NO_RESOURCE;
	}

	spin_unlock_bh(&p->lock);

	num_groups = cfg.ucastq_num / cfg.max_pri;
	if (!num_groups) {
		ppe_drv_warn("%px:invalid HTT queue groups:0 for port:%u ucastq_num:%u max_pri:%u",
				p, port_id, cfg.ucastq_num, cfg.max_pri);
		return PPE_DRV_RET_FAILURE_NO_RESOURCE;
	}

	port->max[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE] = cfg.ucastq_num / num_groups;
	port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE] = cfg.ucastq_start + index * cfg.max_pri;

	port->max[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE] = cfg.mcastq_num / num_groups;
	port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE] = cfg.mcastq_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L0_CDRR] = cfg.l0cdrr_num / num_groups;
	port->base[PPE_DRV_QOS_RES_TYPE_L0_CDRR] = cfg.l0cdrr_start + index * (cfg.l0cdrr_num / num_groups);

	port->max[PPE_DRV_QOS_RES_TYPE_L0_EDRR] = cfg.l0edrr_num / num_groups;
	port->base[PPE_DRV_QOS_RES_TYPE_L0_EDRR] = cfg.l0edrr_start + index * (cfg.l0edrr_num / num_groups);

	port->max[PPE_DRV_QOS_RES_TYPE_L0_SP] = cfg.l0sp_num / num_groups;;
	port->base[PPE_DRV_QOS_RES_TYPE_L0_SP] = cfg.l0sp_start + index * (cfg.l0sp_num / num_groups);;

	port->max[PPE_DRV_QOS_RES_TYPE_L1_CDRR] = cfg.l1cdrr_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L1_CDRR] = cfg.l1cdrr_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L1_EDRR] = cfg.l1edrr_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L1_EDRR] = cfg.l1edrr_start;

	port->max[PPE_DRV_QOS_RES_TYPE_L1_SP] = cfg.l1sp_num;
	port->base[PPE_DRV_QOS_RES_TYPE_L1_SP] = cfg.l1sp_start;

	port->max_pri = cfg.max_pri;

	ppe_drv_info("%px:port QoS resources infosuccessful for port:%u index:%u", p, port_id, index);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_qos_port_htt_res_get);
#endif

/*
 * ppe_drv_qos_ac_ctrl_cfg()
 *	API to configure AC control configuration on the given queue
 */
bool ppe_drv_qos_ac_ctrl_cfg(uint32_t qid, bool ac_en, bool ac_fc_en)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_ac_obj_t obj = {0};
	fal_ac_ctrl_t ctrl_cfg = {0};

	obj.obj_id = qid;
	obj.type = FAL_AC_QUEUE;
	ctrl_cfg.ac_en = ac_en ? A_TRUE : A_FALSE;
	ctrl_cfg.ac_fc_en = ac_fc_en ? A_TRUE : A_FALSE;

	spin_lock_bh(&p->lock);
	ppe_drv_trace("%px:queue flow control set for ucast_qid:%u, enable:%u", p, qid, ctrl_cfg.ac_fc_en);
	if (fal_ac_ctrl_set(PPE_DRV_SWITCH_ID, &obj, &ctrl_cfg) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:queue flow control set failed for ucast_qid:%u", p, qid);
		return false;
	}
	spin_unlock_bh(&p->lock);

	return true;
}
EXPORT_SYMBOL(ppe_drv_qos_ac_ctrl_cfg);
