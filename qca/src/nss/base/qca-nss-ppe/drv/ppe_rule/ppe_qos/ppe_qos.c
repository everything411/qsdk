/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_qos.h"

#define PPE_QOS_TCONT_INVALID 0xFF
#define PPE_QOS_TCONT_DEFAULT_RES 0

struct ppe_qos_base gbl_ppe_qos = {0};

/*
 * ppe_qos_get_interface_id()
 *	Get the interface ID
 * TODO: Add support for Virtual ports
 */
static int ppe_qos_get_interface_id(struct ppe_qos_interface *data)
{
	struct net_device *dev = NULL;
	int port_id;

#ifdef NSS_PPE_PON_SUPPORT
	if (data->type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		if (data->interface.tcont_id >= PPE_DRV_QOS_TCONT_MAX) {
			ppe_qos_warn("%p invalid Tcont ID", data);
			return -1;
		}

		ppe_qos_trace("%p inteface type:%d tcont_id:%d", data, data->type, data->interface.tcont_id);
		return data->interface.tcont_id;
	}
#endif

	ppe_qos_trace("%p inteface type:%d dev:%s", data, data->type, data->interface.dev);
	dev = dev_get_by_name(&init_net, data->interface.dev);
	if (!dev) {
		ppe_qos_warn("%p failed to find valid dev", data);
		return -1;
	}

	port_id = ppe_drv_port_num_from_dev(dev);
	if ((port_id < 0) || (port_id >= PPE_DRV_PHYSICAL_MAX)) {
		ppe_qos_warn("%p invalid port_num", data);
		dev_put(dev);
		return -1;
	}

	dev_put(dev);

	ppe_qos_trace("%p inteface type:%d dev:%s port_id:%d", data, data->type, data->interface.dev, port_id);
	return port_id;
}

/*
 * ppe_qos_res_alloc()
 *	Allocate the QoS resource.
 */
static struct ppe_qos_interface_res *ppe_qos_res_alloc(uint32_t id)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *res = NULL;
	int i;

	res = kzalloc(sizeof(struct ppe_qos_interface_res), GFP_ATOMIC);
	if (!res) {
		ppe_qos_warn("Fetching of port scheduler resource information failed for UNI:%u", id);
		return NULL;
	}

	ppe_qos_trace("Resource allocation for UNI %u", id);
	res->id = id;
	res->type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
	res->valid = false;
	INIT_LIST_HEAD(&res->q_list);
	INIT_LIST_HEAD(&res->mq_list);

	if (ppe_drv_qos_port_res_get(id, &res->port) != PPE_DRV_RET_SUCCESS) {
		kfree(res);
		ppe_qos_warn("Fetching of port scheduler resource information failed for UNI:%u", id);
		return NULL;
	}

	res->l0sp = res->port.base[PPE_DRV_QOS_RES_TYPE_L0_SP];

#ifdef NSS_PPE_PON_SUPPORT
	if (id != g_qos->pon_port) {
#endif
		for (i = 0; i < PPE_DRV_QOS_PRIORITY_MAX; i++) {
			res->l0drr[i].offset = i;
		}
#ifdef NSS_PPE_PON_SUPPORT
	}
#endif

	list_add(&res->list, &g_qos->port_list);

	ppe_qos_warn("%px resource allocated for port:%d", res, id);
	return res;
}

/*
 * ppe_qos_disable_all_queue()
 *	Disables all queues corresponding to a port.
 */
static void ppe_qos_disable_all_queue(struct ppe_qos_interface_res *tm_if)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	uint32_t qid, offset;
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_qos_interface_queue *queue = NULL;

	port = &tm_if->port;
	qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];

	/*
	 * Disable queue enqueue, dequeue and flush the queue.
	 * For UNI, disable all queues, for T-cont disable only assigned queues.
	 */
	if (tm_if->type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		for (offset = 0; offset < port->max[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE]; offset++) {
			ppe_drv_qos_queue_disable(tm_if->id, qid + offset);
		}

		qid = port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE];
		for (offset = 0; offset < port->max[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE]; offset++) {
			ppe_drv_qos_queue_disable(tm_if->id, qid + offset);
		}
	} else {
		if (!list_empty(&tm_if->q_list)) {
			list_for_each_entry(queue, &tm_if->q_list, list) {
				if (!queue->valid) {
					continue;
				}

				ppe_drv_qos_queue_disable(g_qos->pon_port, qid + queue->offset);
			}
		}
	}

	ppe_qos_info("Interface type:%d id:%d all queues disabled", tm_if->type, tm_if->id);
}

/*
 * ppe_qos_enable_all_queue()
 *	Enables all level L0 queues corresponding to a port.
 */
static void ppe_qos_enable_all_queue(struct ppe_qos_interface_res *tm_if)
{
	uint32_t qid, offset;
	struct ppe_drv_qos_port *port = NULL;

	port = &tm_if->port;
	qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];

	/*
	 * Enable queue enqueue and dequeue.
	 */
	for (offset = 0; offset < port->max[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE]; offset++) {
		ppe_drv_qos_queue_enable(qid + offset);
	}

	qid = port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE];
	for (offset = 0; offset < port->max[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE]; offset++) {
		ppe_drv_qos_queue_enable(qid + offset);
	}

	ppe_qos_info("Interface type:%d id:%d all queues enabled", tm_if->type, tm_if->id);
}

/*
 * ppe_qos_enable_assigned_queues()
 *	Enables all assigned level L0 queues corresponding to a port.
 */
static void ppe_qos_enable_assigned_queues(struct ppe_qos_interface_res *tm_if)
{
	uint32_t qid;
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_qos_interface_queue *queue = NULL;

	port = &tm_if->port;
	qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];

	/*
	 * Enable queue enqueue and dequeue.
	 */
	if (!list_empty(&tm_if->q_list)) {
		list_for_each_entry(queue, &tm_if->q_list, list) {
			if (!queue->valid) {
				continue;
			}

			ppe_drv_qos_queue_enable(qid + queue->offset);
		}
	}

	/*
	 * Enable queue enqueue and dequeue for multicast queues.
	 */
	if (tm_if->type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		if (!list_empty(&tm_if->mq_list)) {
			qid = port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE];
			list_for_each_entry(queue, &tm_if->mq_list, list) {
				if (!queue->valid) {
					continue;
				}

				ppe_drv_qos_queue_enable(qid + queue->offset);
			}
		}
	}

	ppe_qos_info("Interface type:%d id:%d assigned queues enabled", tm_if->type, tm_if->id);
}

/*
 * ppe_qos_get_shaper_profile()
 *	Get shaper profile.
 */
static struct ppe_qos_shaper_profile* ppe_qos_get_shaper_profile(char *name)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_shaper_profile *profile;

	if (!list_empty(&g_qos->shaper_list)) {
		list_for_each_entry(profile, &g_qos->shaper_list, list) {
			if (!strcmp(profile->name, name)) {
				return profile;
			}
		}
	}

	return NULL;
}

/*
 * ppe_qos_delete_L0_shaper()
 *	Removes shaper from L0 SP.
 */
static ppe_qos_ret_t ppe_qos_delete_L0_shaper(struct ppe_qos_interface_res *tm_if, char *name)
{
	struct ppe_drv_qos_res res = {0};

	res.l0spid = tm_if->l0sp;
	if (!strcmp(tm_if->shaper_name, name)) {
		if (ppe_drv_qos_flow_shaper_reset(&res) != PPE_DRV_RET_SUCCESS) {
			ppe_qos_warn("%px queue shaper reset failed for shaper name:%s", tm_if, name);
			return PPE_QOS_FAIL;
		}
		memset(tm_if->shaper_name, 0, PPE_QOS_MAX_NAME_LENGTH);
	}

	return PPE_QOS_SUCCESS;
}

/*
 * ppe_qos_delete_shaper_profile()
 *	Removes shaper from interface and deletes QoS shaper profile.
 */
static ppe_qos_ret_t ppe_qos_delete_shaper_profile(char *name)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_shaper_profile *profile = NULL;
	struct ppe_qos_interface_res *tm_if = NULL;
	uint32_t i;

	profile = ppe_qos_get_shaper_profile(name);
	if (!profile) {
		ppe_qos_warn("shaper profile with name:%s does not exist", name);
		return PPE_QOS_FAIL;
	}

	/*
	 * Traverse the UNI ports and reset the shaper if configured.
	 */
	if (!list_empty(&g_qos->port_list)) {
		list_for_each_entry(tm_if, &g_qos->port_list, list) {
			if (ppe_qos_delete_L0_shaper(tm_if, name)) {
				ppe_qos_warn("%px queue shaper reset failed for shaper name:%s", tm_if, name);
				return PPE_QOS_FAIL;
			}
		}
	}

	/*
	 * Traverse the T-conts and reset the shaper if configured.
	 */
	 for (i = 0; i < PPE_DRV_QOS_TCONT_MAX; i++) {
		tm_if = &g_qos->tcont[i];
		if (!tm_if->valid) {
			continue;
		}

		if (ppe_qos_delete_L0_shaper(tm_if, name)) {
			ppe_qos_warn("%px queue shaper reset failed for shaper name:%s", tm_if, name);
			return PPE_QOS_FAIL;
		}
	}

	list_del(&profile->list);
	kfree(profile);

	return PPE_QOS_SUCCESS;
}

/*
 * ppe_qos_reset_l0drr()
 *	Reset L0 DRR
 */
static void ppe_qos_reset_l0drr(struct ppe_qos_interface_res *tm_if)
{
#ifdef NSS_PPE_PON_SUPPORT
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
#endif
	struct ppe_drv_qos_port *port = NULL;
	uint32_t i;

	port = &tm_if->port;

	for (i = 0; i < PPE_DRV_QOS_PRIORITY_MAX; i++) {
		if (tm_if->l0drr[i].ref_cnt) {
			tm_if->l0drr[i].ref_cnt = 0;

#ifdef NSS_PPE_PON_SUPPORT
			if (tm_if->type == PPE_QOS_INTERFACE_TYPE_TCONT) {
				g_qos->l0drr_in_use[tm_if->l0drr[i].offset] = false;
				tm_if->l0drr[i].offset = 0;
			}
#endif
		}
	}
}

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_qos_res_tconts_default_set()
 *	Set default config for Tconts
 */
static void ppe_qos_res_tconts_default_set(void)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_drv_qos_res res = {0};
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_qos_interface_queue *queue = NULL;
	int i;

	tm_if = ppe_qos_res_get(g_qos->pon_port, PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP);
	ppe_qos_disable_all_queue(tm_if);

	for (i = 0; i < PPE_DRV_QOS_TCONT_MAX; i++) {
		ppe_qos_info("Resource allocation for Tcont %u", i);
		g_qos->tcont[i].id = i;
		g_qos->tcont[i].type = PPE_QOS_INTERFACE_TYPE_TCONT;
		g_qos->tcont[i].valid = false;
		INIT_LIST_HEAD(&g_qos->tcont[i].q_list);

		g_qos->tcont[i].port = tm_if->port;
		g_qos->tcont[i].l0sp = tm_if->port.base[PPE_DRV_QOS_RES_TYPE_L0_SP] + i;

		/*
		 * Set Level 1 configuration
		 */
		res.l0spid = g_qos->tcont[i].l0sp;
		res.l1spid = tm_if->port.base[PPE_DRV_QOS_RES_TYPE_L1_SP] + i;
		res.scheduler.l1c_drrid = tm_if->port.base[PPE_DRV_QOS_RES_TYPE_L1_CDRR] + i;
		res.scheduler.l1e_drrid = tm_if->port.base[PPE_DRV_QOS_RES_TYPE_L1_EDRR] + i;

		if (ppe_drv_qos_l1_scheduler_set(&res, g_qos->pon_port) != PPE_DRV_RET_SUCCESS) {
			ppe_qos_enable_all_queue(tm_if);
			printk("%px level1 queue scheduler configuration failed tcont:%d", &res, i);
			return;
		}
	}

	/*
	 * Set PQ to Tcont mapping
	 */
	for (i = 0; i < PPE_DRV_QOS_TCONT_L0_RES_MAX; i++) {
		g_qos->pq_to_tcont_map[i] = PPE_QOS_TCONT_INVALID;
	}

	/*
	 * Set default tcont (Tcont0)
	 * make a hierarchy pq0->l0cdrr0->L0sp0->L1sp0->tcont0
	 * pq_to_tcont_map[0] and l0drr_in_use[0] will always be in use.
	 * Set L0 and L1 scheduler for default Tcont
	 */
	port = &g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES].port;
	queue = kzalloc(sizeof(struct ppe_qos_interface_queue), GFP_ATOMIC);
	if (!queue) {
		ppe_qos_enable_all_queue(tm_if);
		ppe_qos_warn("Free queue list allocation failed for default tcont");
		return;
	}

	queue->offset = PPE_QOS_TCONT_DEFAULT_RES;
	queue->weight = 1;
	queue->priority = 0;
	queue->valid = true;
	list_add(&queue->list, &g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES].q_list);
	g_qos->pon_port_num_pq = 1;

	res.q.ucast_qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];
	res.scheduler.l0c_drrid = port->base[PPE_DRV_QOS_RES_TYPE_L0_CDRR];
	res.scheduler.l0e_drrid = port->base[PPE_DRV_QOS_RES_TYPE_L0_EDRR];
	res.scheduler.priority = 0;
	res.scheduler.drr_weight = 1;
	res.scheduler.drr_unit = 0; /* 0: byte based, 1: packet based */
	res.l0spid = port->base[PPE_DRV_QOS_RES_TYPE_L0_SP];

	if (ppe_drv_qos_l0_scheduler_set(&res, g_qos->pon_port) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_enable_all_queue(tm_if);
		kfree(queue);
		ppe_qos_warn("level0 queue scheduler configuration failed for default tcont");
		return;
	}

	if (ppe_drv_qos_tcont_set(&res, PPE_QOS_TCONT_DEFAULT_RES, true) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_enable_all_queue(tm_if);
		kfree(queue);
		ppe_qos_warn("tcont configuration failed for default tcont");
		return;
	}

	ppe_qos_enable_assigned_queues(&g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES]);

	g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES].num_queues = 1;
	g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES].l0drr[0].offset = 0;
	g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES].l0drr[0].ref_cnt = 1;
	g_qos->pq_to_tcont_map[PPE_QOS_TCONT_DEFAULT_RES] = PPE_QOS_TCONT_DEFAULT_RES;
	g_qos->l0drr_in_use[PPE_QOS_TCONT_DEFAULT_RES] = true;
	g_qos->tcont[PPE_QOS_TCONT_DEFAULT_RES].valid = true;
}
#endif

/*
 * ppe_qos_get_queue_list()
 *	Get appropriate queue list based on type
 */
static struct list_head *ppe_qos_get_queue_list(struct ppe_qos_interface_res *tm_if,
						 ppe_qos_queue_type_t queue_type)
{
	return (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) ? &tm_if->mq_list : &tm_if->q_list;
}

/*
 * ppe_qos_get_num_queues_ptr()
 *	Get pointer to queue count based on type
 */
static uint32_t *ppe_qos_get_num_queues_ptr(struct ppe_qos_interface_res *tm_if,
					     ppe_qos_queue_type_t queue_type)
{
	return (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) ? &tm_if->num_mcast_queues : &tm_if->num_queues;
}

/*
 * ppe_qos_get_queue_base()
 *	Get base queue ID based on type
 */
static uint32_t ppe_qos_get_queue_base(struct ppe_drv_qos_port *port,
					ppe_qos_queue_type_t queue_type)
{
	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		return port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE];
	}
	return port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];
}

/*
 * ppe_qos_get_queue_type_str()
 *	Get string representation of queue type
 */
static const char *ppe_qos_get_queue_type_str(ppe_qos_queue_type_t queue_type)
{
	return (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) ? "multicast" : "unicast";
}

/*
 * ppe_qos_delete_interface_queues()
 *	Deletes port queues and restores default port configuration
 */
static ppe_qos_ret_t ppe_qos_delete_interface_queues(struct ppe_qos_interface_res *tm_if)
{
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_qos_interface_queue *queue, *tmp;
	uint32_t qid;
#ifdef NSS_PPE_PON_SUPPORT
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_drv_qos_res res = {0};
	uint32_t i;
#endif

	port = &tm_if->port;
	qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];

	if ((!tm_if->valid) || ((tm_if->type == PPE_QOS_INTERFACE_TYPE_TCONT) && (list_empty(&tm_if->q_list)))
		|| ((tm_if->type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) && (((list_empty(&tm_if->q_list)) && (list_empty(&tm_if->mq_list)))))) {
		ppe_qos_warn("No queue assigned to the interface:%d", tm_if->id);
		return PPE_QOS_FAIL;
	}

	/*
	 * Remove shaper, Reset and remove the allocated queues
	 * Set port's default config
	 */
	ppe_qos_disable_all_queue(tm_if);
	list_for_each_entry_safe(queue, tmp, &tm_if->q_list, list) {
#ifdef NSS_PPE_PON_SUPPORT
		if (tm_if->type == PPE_QOS_INTERFACE_TYPE_TCONT) {

			/*
			 * Delete T-cont to queue mapping
			 */
			if (queue->valid) {
				res.q.ucast_qid = qid + queue->offset;
				if (ppe_drv_qos_tcont_set(&res, tm_if->id, false) != PPE_DRV_RET_SUCCESS) {
					ppe_qos_enable_assigned_queues(tm_if);
					ppe_qos_warn("queue unmapping failed for interface:%d", tm_if->id);
					return PPE_QOS_FAIL;
				}
				g_qos->pq_to_tcont_map[queue->offset] = PPE_QOS_TCONT_INVALID;
				g_qos->pon_port_num_pq--;
			}
		}
#endif

		queue->valid = false;
		list_del(&queue->list);
		tm_if->num_queues--;
		kfree(queue);
	}

	/*
	 * Delete mcast queues for UNI ports
	 */
	if (tm_if->type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		list_for_each_entry_safe(queue, tmp, &tm_if->mq_list, list) {
			queue->valid = false;
			list_del(&queue->list);
			tm_if->num_mcast_queues--;
			kfree(queue);
		}
	}

	/*
	 * Reset L0 DRRs used for this port/Tcont
	 */
	ppe_qos_reset_l0drr(tm_if);
	tm_if->valid = false;

	/*
	 * Set port default configuration for UNI ports
	 */
	if (tm_if->type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		if (ppe_drv_qos_default_conf_set(tm_if->id) != PPE_DRV_RET_SUCCESS) {
			ppe_qos_warn("port %u default config set failed", tm_if->id);
			ppe_qos_enable_assigned_queues(tm_if);
			return PPE_QOS_FAIL;
		}

		ppe_qos_enable_all_queue(tm_if);
		return PPE_QOS_SUCCESS;
	}

#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Set upstream port default configuration if all Tconts queues are deleted
	 */
	 for (i = 0; i < PPE_DRV_QOS_TCONT_MAX; i++) {
		if (g_qos->tcont[i].valid) {
			return PPE_QOS_SUCCESS;
		}
	}

	if (ppe_drv_qos_default_conf_set(g_qos->pon_port) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_warn("port %u default config set failed", tm_if->id);
		return PPE_QOS_FAIL;
	}

	ppe_qos_res_tconts_default_set();
#endif

	return PPE_QOS_SUCCESS;
}

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_qos_get_tcont_l0drr_offset()
 *	Get free L0 DRR for T-cont
 */
static uint32_t ppe_qos_get_tcont_l0drr_offset(void)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	uint32_t i;

	/*
	 * index 0 is reserved for Default T-cont
	 */
	for (i = 1; i < ppe_drv_pon_port_max_pq_get(); i++) {
		if (!g_qos->l0drr_in_use[i]) {
			return i;
		}
	}

	return 0;
}
#endif

/*
 * ppe_qos_delete_interface_shaper()
 *	Delete shaper for a T-cont/UNI.
 */
static ppe_qos_ret_t ppe_qos_delete_interface_shaper(struct ppe_qos_interface_res *tm_if)
{
	struct ppe_drv_qos_res res = {0};

	/*
	 * Check if shaper is configured, reset shaper
	 */
	res.l0spid = tm_if->l0sp;
	if (strlen(tm_if->shaper_name)) {
		if (ppe_drv_qos_flow_shaper_reset(&res) != PPE_DRV_RET_SUCCESS) {
			ppe_qos_warn("interface shaper reset failed, interface id %d", tm_if->id);
				return PPE_QOS_FAIL;
		}
		memset(tm_if->shaper_name, 0, PPE_QOS_MAX_NAME_LENGTH);
	}

	return PPE_QOS_SUCCESS;
}

/*
 * ppe_qos_res_deinit()
 *	Qos resource deinit
 */
static void ppe_qos_res_deinit(void)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_shaper_profile *profile = NULL;
	struct ppe_qos_interface_res *res, *tmp;
#ifdef NSS_PPE_PON_SUPPORT
	uint32_t i;
#endif

	/*
	 * Remove all shapers, free allocated queues memory,
	 * reset PQ to Tcont mapping and set Tcont 0 default config
	 */
	spin_lock_bh(&g_qos->lock);
	if (!list_empty(&g_qos->shaper_list)) {
		list_for_each_entry(profile, &g_qos->shaper_list, list) {
			ppe_qos_delete_shaper_profile(profile->name);
		}
	}

	if (!list_empty(&g_qos->port_list)) {
		list_for_each_entry_safe(res, tmp, &g_qos->port_list, list) {
			if (res->valid) {
				ppe_qos_delete_interface_queues(res);
			}

			list_del(&res->list);
			kfree(res);
		}
	}

#ifdef NSS_PPE_PON_SUPPORT
	for (i = 0; i < PPE_DRV_QOS_TCONT_MAX; i++) {
		if (g_qos->tcont[i].valid) {
			ppe_qos_delete_interface_queues(&g_qos->tcont[i]);
		}
	}

	for (i = 0; i < PPE_DRV_QOS_TCONT_L0_RES_MAX; i++) {
		g_qos->pq_to_tcont_map[i] = PPE_QOS_TCONT_INVALID;
	}
#endif

	spin_unlock_bh(&g_qos->lock);
	return;
}

/*
 * ppe_qos_pon_port_res_init()
 *	Deferred PON port QoS resource allocation.
 *	Called via callback from ppe_drv_dp_gem_enable() after the PON port
 *	is configured in HW. This ensures fal_port_scheduler_resource_get()
 *	returns valid tcont resources for the PON port.
 *
 *	Also called directly from ppe_qos_init() as a fallback in case
 *	gem_enable() was already invoked before this module loaded/reloaded.
 */
#ifdef NSS_PPE_PON_SUPPORT
static void ppe_qos_pon_port_res_init(uint32_t pon_port_id)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;

	spin_lock_bh(&g_qos->lock);

	if (g_qos->pon_port) {
		ppe_qos_warn("PON port QoS resources already initialized for port %u", g_qos->pon_port);
		spin_unlock_bh(&g_qos->lock);
		return;
	}

	g_qos->pon_port = pon_port_id;

	if (!ppe_qos_res_alloc(g_qos->pon_port)) {
		ppe_qos_warn("Resource allocation failed for PON port %u", pon_port_id);
		g_qos->pon_port = 0;
		spin_unlock_bh(&g_qos->lock);
		return;
	}

	/*
	 * Set default tcont resources for the PON port.
	 */
	ppe_qos_res_tconts_default_set();

	spin_unlock_bh(&g_qos->lock);
	ppe_qos_info("PON port %u QoS resources initialized via gem_enable callback", pon_port_id);
}
#endif

/*
 * ppe_qos_res_init()
 *	QoS resource init.
 *	Initializes non-PON port resources. If the PON port was already
 *	enabled via gem_enable() before this module loaded or reloaded,
 *	allocates PON port resources immediately as a fallback by setting
 *	g_qos->pon_port and calling the allocation helpers directly.
 *	This ensures the guard check in ppe_qos_pon_port_res_init() correctly
 *	detects double-initialization if the callback fires afterwards.
 *	Otherwise, PON port resource allocation is deferred to
 *	ppe_qos_pon_port_res_init(), invoked via callback from
 *	ppe_drv_dp_gem_enable() after gem port is enabled.
 */
static void ppe_qos_res_init(void)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
#ifdef NSS_PPE_PON_SUPPORT
	uint32_t pon_port;
#endif

	spin_lock_bh(&g_qos->lock);
	INIT_LIST_HEAD(&g_qos->port_list);
	spin_unlock_bh(&g_qos->lock);

#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Fallback: if gem_enable() was already called before this module
	 * loaded or reloaded, allocate PON port resources immediately.
	 * Set g_qos->pon_port here (under the lock) so that the callback
	 * guard in ppe_qos_pon_port_res_init() correctly detects
	 * double-initialization if the callback fires afterwards.
	 */
	pon_port = ppe_drv_qos_pon_port_get();
	if (pon_port) {
		ppe_qos_info("PON port %u already enabled, initializing QoS resources now", pon_port);
		spin_lock_bh(&g_qos->lock);
		g_qos->pon_port = pon_port;
		if (!ppe_qos_res_alloc(g_qos->pon_port)) {
			ppe_qos_warn("Resource allocation failed for PON port %u", pon_port);
			g_qos->pon_port = 0;
			spin_unlock_bh(&g_qos->lock);
		} else {
			ppe_qos_res_tconts_default_set();
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_info("PON port %u QoS resources initialized at module init", pon_port);
		}
	}
#endif
}

/*
 * ppe_qos_res_get()
 *	Get the QoS resource.
 */
struct ppe_qos_interface_res *ppe_qos_res_get(uint32_t id, ppe_qos_interface_type_t type)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *res = NULL;

#ifdef NSS_PPE_PON_SUPPORT
	if (type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		return &g_qos->tcont[id];
	}
#endif

	/*
	 * Get the physical or VP port resource
	 */
	if (type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		if (!list_empty(&g_qos->port_list)) {
			list_for_each_entry(res, &g_qos->port_list, list) {
				if (res->id == id) {
					ppe_qos_trace("resource found for id:%d, type:%d", id, type);
					return res;
				}
			}
		}
	}

	return NULL;
}

/*
 * ppe_qos_delete_shaper()
 *	Delete QoS shaper profile.
 */
ppe_qos_ret_t ppe_qos_delete_shaper(struct ppe_qos_shaper_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_delete_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid shaper info");
		return PPE_QOS_DELETE_SHAPER_FAIL;
	}

	if (!strlen(info->name)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_delete_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("shaper profile name is empty");
		return PPE_QOS_DELETE_SHAPER_FAIL;
	}

	 if (ppe_qos_delete_shaper_profile(info->name)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_delete_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("shaper reset failed name:%s failed", info->name);
		return PPE_QOS_DELETE_SHAPER_FAIL;
	}

	ppe_qos_stats_inc(&g_qos->stats.qos_delete_shaper_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("delete shaper success, shaper name:%s", info->name);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_delete_shaper);

/*
 * ppe_qos_create_shaper()
 *	Create QoS shaper profile.
 */
ppe_qos_ret_t ppe_qos_create_shaper(struct ppe_qos_shaper_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_shaper_profile *profile = NULL;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid shaper info");
		return PPE_QOS_CREATE_SHAPER_FAIL;
	}

	if (!strlen(info->name)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("shaper profile name is empty");
		return PPE_QOS_CREATE_SHAPER_FAIL;
	}

	profile = ppe_qos_get_shaper_profile(info->name);
	if (profile) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("shaper profile with name:%s already exists", info->name);
		return PPE_QOS_CREATE_SHAPER_FAIL;
	}

	profile = kzalloc(sizeof(struct ppe_qos_shaper_profile), GFP_ATOMIC);
	if (!profile) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("shaper allocation failed for name: %s", info->name);
		return PPE_QOS_CREATE_SHAPER_FAIL;
	}

	strlcpy(profile->name, info->name, PPE_QOS_MAX_NAME_LENGTH);
	profile->shaper.rate = info->cir;
	profile->shaper.crate = info->eir;
	profile->shaper.burst = info->cbs;
	profile->shaper.cburst = info->ebs;
	list_add(&profile->list, &g_qos->shaper_list);

	ppe_qos_stats_inc(&g_qos->stats.qos_create_shaper_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("create shaper success, shaper name:%s", info->name);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_create_shaper);

/*
 * ppe_qos_set_queue_limit()
 *	Set QoS queue limit and threshold information.
 */
ppe_qos_ret_t ppe_qos_set_queue_limit(struct ppe_qos_queue_limit_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_drv_qos_res res = {0};
	struct ppe_qos_interface_queue *queue = NULL;
	struct list_head *queue_list = NULL;
	ppe_qos_queue_type_t queue_type;
	uint32_t queue_base;
	bool found = false;
	atomic64_t *failed_stats;
	int id;

	if (!info) {
		ppe_qos_warn("invalid queue info");
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}

	spin_lock_bh(&g_qos->lock);
	queue_type = info->queue_type;
	failed_stats = queue_type == PPE_QOS_QUEUE_TYPE_MCAST ?
			&g_qos->stats.qos_mcast_queue_limit_set_fail : &g_qos->stats.qos_set_queue_limit_fail;

	id = ppe_qos_get_interface_id(&info->if_data);
	if (id < 0) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}

	tm_if = ppe_qos_res_get(id, info->if_data.type);
	if (!tm_if) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}

	port = &tm_if->port;
	if (!port) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}

#ifndef NSS_PPE_PON_SUPPORT
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px Tcont not supported", info);
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}
#endif

	/*
	 * Validate multicast constraints
	 */
	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		if (info->if_data.type != PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
			ppe_qos_stats_inc(failed_stats);
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_warn("%px multicast queues not supported on T-cont", info);
			return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
		}
	}

	/*
	 * Validate params for UNI
	 */
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		if (queue_type == PPE_QOS_QUEUE_TYPE_UCAST) {
			if (info->queue_id >= tm_if->num_queues) {
				ppe_qos_stats_inc(failed_stats);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("%px invalid queue ID, queue_id: %d", info, info->queue_id);
				return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
			}
		}

		if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
			if (info->queue_id >= tm_if->num_mcast_queues) {
				ppe_qos_stats_inc(failed_stats);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("%px invalid queue ID, queue_id: %d", info, info->queue_id);
				return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
			}
		}
	}

#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Validate params for TCONT
	 */
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		if (info->queue_id >= ppe_drv_pon_port_max_pq_get()) {
			ppe_qos_stats_inc(failed_stats);
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_warn("%px invalid queue ID,queue_id: %d", info, info->queue_id);
			return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
		}
	}
#endif

	/* Get appropriate list and counter */
	queue_list = ppe_qos_get_queue_list(tm_if, queue_type);
	queue_base = ppe_qos_get_queue_base(port, queue_type);

	/*
	 * Get queue with the given offset from DB
	 */
	if (!list_empty(queue_list)) {
		list_for_each_entry(queue, queue_list, list) {
			if (queue->offset == info->queue_id) {
				found = true;
				break;
			}
		}
	}

	if ((!found) || (!queue->valid)) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px %s queue %d not assigned", info, ppe_qos_get_queue_type_str(queue_type), info->queue_id);
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}

	/*
	 * Set queue limits and thresholds
	 */
	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		res.q.mcast_qid = queue_base + queue->offset;
		res.q.mcast_valid = true;
	} else {
		res.q.ucast_qid = queue_base + queue->offset;
	}

	res.q.qlimit = info->ceiling / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.color_en = info->color_en;
	res.q.red_en = info->wred_en;

	/*
	 * PPE operates in terms of memory blocks, and each block
	 * is PPE_DRV_QOS_MEM_BLOCK_SIZE bytes in size. Therefore we divide the
	 * input parameters which are in bytes by PPE_DRV_QOS_MEM_BLOCK_SIZE to get
	 * the number of memory blocks to assign.
	 *
	 * Convert offsets (in bytes) to memory blocks
	 */
	res.q.min_th[PPE_DRV_QOS_QUEUE_COLOR_GREEN] = info->green_min_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.min_th[PPE_DRV_QOS_QUEUE_COLOR_YELLOW] = info->yellow_min_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.min_th[PPE_DRV_QOS_QUEUE_COLOR_RED] = info->red_min_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;

	res.q.max_th[PPE_DRV_QOS_QUEUE_COLOR_GREEN] = (info->ceiling - info->green_min_off) / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.max_th[PPE_DRV_QOS_QUEUE_COLOR_YELLOW] = info->yellow_max_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.max_th[PPE_DRV_QOS_QUEUE_COLOR_RED] = info->red_max_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;

	/*
	 * Convert resume offsets from bytes to memory blocks and pass to hardware.
	 * Resume offsets control when the queue resumes accepting packets after dropping,
	 * which helps prevent buffer oscillation.
	 */
	res.q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_GREEN] = info->green_resume_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_YELLOW] = info->yellow_resume_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;
	res.q.resume_off[PPE_DRV_QOS_QUEUE_COLOR_RED] = info->red_resume_off / PPE_DRV_QOS_MEM_BLOCK_SIZE;

	if (ppe_drv_qos_queue_limit_set(&res) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px %s queue limit configuration failed", info, ppe_qos_get_queue_type_str(queue_type));
		return PPE_QOS_SET_QUEUE_LIMIT_FAIL;
	}

	queue->limit.ceiling = info->ceiling;
	queue->limit.color_en = info->color_en;
	queue->limit.wred_en = info->wred_en;
	queue->limit.green_min_off = info->green_min_off;
	queue->limit.yellow_max_off = info->yellow_max_off;
	queue->limit.yellow_min_off = info->yellow_min_off;
	queue->limit.red_max_off = info->red_max_off;
	queue->limit.red_min_off = info->red_min_off;
	queue->limit.green_resume_off = info->green_resume_off;
	queue->limit.yellow_resume_off = info->yellow_resume_off;
	queue->limit.red_resume_off = info->red_resume_off;
	queue->limit.is_configured = true;

	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_limit_set_success);
	} else {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_queue_limit_success);
	}
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("Set %s queue limit for interface type:%d id:%d queue:%d",
		     ppe_qos_get_queue_type_str(queue_type), info->if_data.type, id, info->queue_id);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_set_queue_limit);

/*
 * ppe_qos_set_interface_queue_ctrl()
 *	Set interface queue control (enqueue/dequeue enable/disable/drop).
 */
ppe_qos_ret_t ppe_qos_set_interface_queue_ctrl(struct ppe_qos_interface_queue_ctrl_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_qos_interface_queue *queue = NULL;
	uint32_t qid;
	int id;
	ppe_drv_ret_t (*ctrl_func)(uint32_t, bool);
	bool enable;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid queue info");
		return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
	}

	id = ppe_qos_get_interface_id(&info->if_data);
	if (id < 0) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
	}

	tm_if = ppe_qos_res_get(id, info->if_data.type);
	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
	}

	port = &tm_if->port;
	if (!port) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
	}

	if (!tm_if->valid) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
	}

	/* Validate drop state is only used with dequeue mode */
	if ((info->state == PPE_QOS_QUEUE_CTRL_STATE_DROP) &&
	    (info->mode != PPE_QOS_QUEUE_CTRL_MODE_DEQUEUE)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px drop state only valid with dequeue mode, mode: %d", info, info->mode);
		return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
	}

	/* Select the appropriate control function based on mode and state */
	if (info->state == PPE_QOS_QUEUE_CTRL_STATE_DROP) {
		/* Drop state - use dequeue drop control */
		ctrl_func = ppe_drv_qos_queue_dequeue_drop_ctrl;
		enable = true;  /* Drop is enabled */
	} else {
		/* Enable/Disable state - use enqueue or dequeue control */
		ctrl_func = (info->mode == PPE_QOS_QUEUE_CTRL_MODE_ENQUEUE) ?
			    ppe_drv_qos_queue_enqueue_ctrl : ppe_drv_qos_queue_dequeue_ctrl;
		enable = (info->state == PPE_QOS_QUEUE_CTRL_STATE_ENABLE);
	}

	qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];

	/*
	 * Control enqueue/dequeue/drop for all queues on the interface.
	 * For physical interfaces, control all queues.
	 * For TCONT interfaces, control only assigned queues.
	 */
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		uint32_t offset;
		for (offset = 0; offset < port->max[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE] - 1; offset++) {
			if (ctrl_func(qid + offset, enable) != PPE_DRV_RET_SUCCESS) {
				ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("queue control failed for interface type:%d id:%d queue:%d",
					     info->if_data.type, id, offset);
				return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
			}
		}
	} else {
		if (!list_empty(&tm_if->q_list)) {
			list_for_each_entry(queue, &tm_if->q_list, list) {
				if (!queue->valid) {
					continue;
				}

				if (ctrl_func(qid + queue->offset, enable) != PPE_DRV_RET_SUCCESS) {
					ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_fail);
					spin_unlock_bh(&g_qos->lock);
					ppe_qos_warn("queue control failed for interface type:%d id:%d queue:%d",
						     info->if_data.type, id, queue->offset);
					return PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL;
				}
			}
		}
	}

	ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_queue_ctrl_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("Interface type:%d id:%d queue control set successfully mode:%d state:%d",
		     info->if_data.type, id, info->mode, info->state);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_set_interface_queue_ctrl);

/*
 * ppe_qos_set_queue_tm()
 *	Set QoS traffic management for a given port and queue.
 */
ppe_qos_ret_t ppe_qos_set_queue_tm(struct ppe_qos_queue_tm_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_drv_qos_res res = {0};
	struct ppe_qos_interface_queue *queue = NULL;
#ifdef NSS_PPE_PON_SUPPORT
	uint32_t offset;
#endif
	struct list_head *queue_list = NULL;
	ppe_qos_queue_type_t queue_type;
	uint32_t queue_base;
	bool found = false;
	int id, port_id;
	atomic64_t *failed_stats;

	if (!info) {
		ppe_qos_warn("invalid queue info");
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	spin_lock_bh(&g_qos->lock);
	queue_type =  info->queue_type;
	failed_stats = queue_type == PPE_QOS_QUEUE_TYPE_MCAST ?
			&g_qos->stats.qos_mcast_queue_tm_set_fail : &g_qos->stats.qos_set_queue_tm_fail;

	id = ppe_qos_get_interface_id(&info->if_data);
	if (id < 0) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	tm_if = ppe_qos_res_get(id, info->if_data.type);
	if (!tm_if) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	port = &tm_if->port;
	if (!port) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

#ifndef NSS_PPE_PON_SUPPORT
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px Tcont not supported", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}
#endif

	/*
	 * Validate multicast constraints
	 */
	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		if (info->if_data.type != PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
			ppe_qos_stats_inc(failed_stats);
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_warn("%px multicast queues not supported on T-cont", info);
			return PPE_QOS_SET_QUEUE_TM_FAIL;
		}
	}

	/*
	 * Validate params for UNI
	 */
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		if (queue_type == PPE_QOS_QUEUE_TYPE_UCAST) {
			if (info->queue_id >= tm_if->num_queues) {
				ppe_qos_stats_inc(failed_stats);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("%px invalid queue ID, queue: %d", info, info->queue_id);
				return PPE_QOS_SET_QUEUE_TM_FAIL;
			}

			if (info->priority >= PPE_DRV_QOS_PRIORITY_MAX) {
				ppe_qos_stats_inc(failed_stats);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("%px invalid priority:%d", info, info->priority);
				return PPE_QOS_SET_QUEUE_TM_FAIL;
			}
		}

		if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
			if (info->queue_id >= tm_if->num_mcast_queues) {
				ppe_qos_stats_inc(failed_stats);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("%px invalid queue ID, queue: %d", info, info->queue_id);
				return PPE_QOS_SET_QUEUE_TM_FAIL;
			}

			if (info->priority >= PPE_QOS_MCAST_PRIORITY_MAX) {
				ppe_qos_stats_inc(failed_stats);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("%px invalid priority:%d", info, info->priority);
				return PPE_QOS_SET_QUEUE_TM_FAIL;
			}
		}
	}

#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Validate params for TCONT
	 */
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		if (info->queue_id >= ppe_drv_pon_port_max_pq_get()) {
			ppe_qos_stats_inc(failed_stats);
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_warn("%px invalid queue ID, queue: %d", info, info->queue_id);
			return PPE_QOS_SET_QUEUE_TM_FAIL;
		}

		if (info->priority >= PPE_DRV_QOS_PRIORITY_MAX) {
			ppe_qos_stats_inc(failed_stats);
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_warn("%px invalid priority:%d", info, info->priority);
			return PPE_QOS_SET_QUEUE_TM_FAIL;
		}
	}
#endif

	if (!tm_if->valid) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	/*
	 * Check if this queue is mapped to this Tcont
	 */
	if ((info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) && (g_qos->pq_to_tcont_map[info->queue_id] != id)) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px queue not mapped to this interface", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	/* Get appropriate list and counter */
	queue_list = ppe_qos_get_queue_list(tm_if, queue_type);
	queue_base = ppe_qos_get_queue_base(port, queue_type);

	/*
	 * Get queue with the given offset from DB
	 */
	if (!list_empty(queue_list)) {
		list_for_each_entry(queue, queue_list, list) {
			if (queue->offset == info->queue_id) {
				found = true;
				break;
			}
		}
	}

	if ((!found) || (!queue->valid)) {
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px queue %d not assigned", info, info->queue_id);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	/*
	 * Get L0 DRR based on priority
	 */
	port_id = id;
#ifdef NSS_PPE_PON_SUPPORT
	 if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		port_id = g_qos->pon_port;
		if (!(tm_if->l0drr[info->priority].offset)) {
			offset = ppe_qos_get_tcont_l0drr_offset();
			if (!offset) {
				ppe_qos_stats_inc(&g_qos->stats.qos_set_queue_tm_fail);
				spin_unlock_bh(&g_qos->lock);
				ppe_qos_warn("No free l0drr available for T-cont %u", id);
				return PPE_QOS_SET_QUEUE_TM_FAIL;
			}

			tm_if->l0drr[info->priority].offset = offset;
			g_qos->l0drr_in_use[offset] = true;
		}
	}
#endif

	queue->priority = info->priority;
	queue->weight = info->weight;
	tm_if->l0drr[info->priority].ref_cnt++;

	/*
	 * Set Level 0 PPE configuration for SP and WRR
	 * Disable and flush the queues before
	 * changing scheduler's sp_id/drr_id/priority.
	 * Assuming 8 L0 DRRS are connected each to a single priority line 0~7,
	 * 0 - lowest, 7 - highest.
	 */
	ppe_qos_disable_all_queue(tm_if);

	res.q.ucast_qid = queue_base + queue->offset;
	res.scheduler.l0c_drrid = port->base[PPE_DRV_QOS_RES_TYPE_L0_CDRR] + tm_if->l0drr[info->priority].offset;
	res.scheduler.l0e_drrid = port->base[PPE_DRV_QOS_RES_TYPE_L0_EDRR] + tm_if->l0drr[info->priority].offset;
	res.scheduler.priority = info->priority;
	res.scheduler.drr_weight = info->weight;
	res.scheduler.drr_unit = 0; /* 0: byte based, 1: packet based */
	res.l0spid = tm_if->l0sp;

	if (ppe_drv_qos_l0_scheduler_set(&res, port_id) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_enable_assigned_queues(tm_if);
		ppe_qos_stats_inc(failed_stats);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px level0 queue scheduler configuration failed", info);
		return PPE_QOS_SET_QUEUE_TM_FAIL;
	}

	ppe_qos_enable_assigned_queues(tm_if);

	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_tm_set_success);
	} else {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_queue_tm_success);
	}
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("Set %s queue TM for interface type:%d id:%d queue:%d",
		     ppe_qos_get_queue_type_str(queue_type), info->if_data.type, id, info->queue_id);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_set_queue_tm);

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_qos_get_tcont_stats()
 *	Get stats of a T-cont.
 */
ppe_qos_ret_t ppe_qos_get_tcont_stats(struct ppe_qos_tcont_stats_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_drv_qos_tcont_stat stats = {0};

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_tcont_stats_get_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid T-cont info");
		return PPE_QOS_TCONT_STATS_GET_FAIL;
	}

	tm_if = &g_qos->tcont[info->tcont_id];
	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_tcont_stats_get_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px tcont info not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_TCONT_STATS_GET_FAIL;
	}

	/*
	 * Check T-cont validity
	 */
	if (!tm_if->valid) {
		ppe_qos_stats_inc(&g_qos->stats.qos_tcont_stats_get_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_TCONT_STATS_GET_FAIL;
	}

	if (ppe_drv_qos_tcont_stats_get(info->tcont_id, &stats) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_stats_inc(&g_qos->stats.qos_tcont_stats_get_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px t-cont stats get failed for tcont: %d", info, info->tcont_id);
		return PPE_QOS_TCONT_STATS_GET_FAIL;
	}

	info->credit = stats.credit;
	info->pend_bytes = stats.pend_bytes;

	ppe_qos_stats_inc(&g_qos->stats.qos_tcont_stats_get_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("%px t-cont stats get success for tcont: %d", info, info->tcont_id);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_get_tcont_stats);

/*
 * ppe_qos_reset_tcont_credit()
 *	Reset the credit of a T-cont.
 */
ppe_qos_ret_t ppe_qos_reset_tcont_credit(struct ppe_qos_tcont_stats_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_reset_tcont_credit_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid T-cont info");
		return PPE_QOS_RESET_TCONT_CREDIT_FAIL;
	}

	tm_if = &g_qos->tcont[info->tcont_id];
	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_reset_tcont_credit_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px tcont info not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_RESET_TCONT_CREDIT_FAIL;
	}

	/*
	 * Check T-cont validity
	 */
	if (!tm_if->valid) {
		ppe_qos_stats_inc(&g_qos->stats.qos_reset_tcont_credit_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_RESET_TCONT_CREDIT_FAIL;
	}

	if (ppe_drv_qos_tcont_credit_reset(info->tcont_id) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_stats_inc(&g_qos->stats.qos_reset_tcont_credit_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px t-cont credit reset failed for tcont: %d", info, info->tcont_id);
		return PPE_QOS_RESET_TCONT_CREDIT_FAIL;
	}

	ppe_qos_stats_inc(&g_qos->stats.qos_reset_tcont_credit_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("%px t-cont reset credit success for tcont: %d", info, info->tcont_id);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_reset_tcont_credit);

/*
 * ppe_qos_map_pq_to_tcont()
 *	Map the priority queue to a T-cont.
 */
ppe_qos_ret_t ppe_qos_map_pq_to_tcont(struct ppe_qos_pq_to_tcont_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	uint32_t queue_id;
	uint32_t id = info->tcont_id;
	struct ppe_qos_interface_queue *queue = NULL;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_drv_qos_port *port = NULL;
	struct ppe_drv_qos_res res = {0};
	bool found = false;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid mapping info");
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	queue_id = info->queue_id;
	tm_if = &g_qos->tcont[id];
	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px tcont info is not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	port = &tm_if->port;
	if (!port) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px tcont info is not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	/*
	 * Check T-cont validity
	 */
	if (!tm_if->valid) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not valid for tcont: %d", info, info->tcont_id);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	/*
	 * Check PQ range and mapping
	 */
	if ((queue_id >= ppe_drv_pon_port_max_pq_get())
		|| (g_qos->pq_to_tcont_map[queue_id] != PPE_QOS_TCONT_INVALID)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px queue id: %d is not in valid range or already mapped to a Tcont", info, info->queue_id);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	/*
	 * Get the unmapped queue from assigned Tcont queues
	 */
	if (list_empty(&tm_if->q_list)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface is not assigned any queues", info);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	list_for_each_entry(queue, &tm_if->q_list, list) {
		if (!queue->valid) {
			found = true;
			break;
		}
	}

	if (!found) {
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px no unmapped queue available ", info);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	/*
	 * Map T-cont to the queue
	 */
	ppe_qos_disable_all_queue(tm_if);

	res.q.ucast_qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE] + queue_id;
	if (ppe_drv_qos_tcont_set(&res, id, true) != PPE_DRV_RET_SUCCESS) {
		ppe_qos_enable_assigned_queues(tm_if);
		ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_fail);
		spin_unlock_bh(&g_qos->lock);
		return PPE_QOS_MAP_PQ_TO_TCONT_FAIL;
	}

	g_qos->pq_to_tcont_map[queue_id] = id;
	queue->offset = queue_id;
	queue->valid = true;
	ppe_qos_enable_assigned_queues(tm_if);

	ppe_qos_stats_inc(&g_qos->stats.qos_pq_to_tcont_mapping_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("%px mapping pq: %d to tcont: %d success", info, info->queue_id, info->tcont_id);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_map_pq_to_tcont);
#endif

/*
 * ppe_qos_set_interface_shaper()
 *	Set shaper for a T-cont/UNI.
 */
ppe_qos_ret_t ppe_qos_set_interface_shaper(struct ppe_qos_interface_shaper_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_qos_shaper_profile *profile = NULL;
	struct ppe_drv_qos_res res = {0};
	int id;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid command info");
		return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
	}

	id = ppe_qos_get_interface_id(&info->if_data);
	if (id < 0) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
	}

	tm_if = ppe_qos_res_get(id, info->if_data.type);
	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface info is not valid", info);
		return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
	}

	if (!tm_if->valid) {
		ppe_qos_warn("%px interface is not valid", info);
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
	}

	/*
	 * Set L0 SP shaper if present
	 * Get Shaper profile from shaper name, set port bitmap in shaper profile
	 * Set shaper profile name in queue
	 */
	res.l0spid = tm_if->l0sp;
	if (strlen(info->shaper_name)) {
		profile = ppe_qos_get_shaper_profile(info->shaper_name);
		if (!profile) {
			ppe_qos_warn("%px shaper profile: %s does not exist", info, info->shaper_name);
			ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
			spin_unlock_bh(&g_qos->lock);
			return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
		}

		res.shaper.rate = profile->shaper.rate;
		res.shaper.crate = profile->shaper.crate;
		res.shaper.burst = profile->shaper.burst;
		res.shaper.cburst = profile->shaper.cburst;

		if (ppe_drv_qos_flow_shaper_set(&res) != PPE_DRV_RET_SUCCESS) {
			ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
			spin_unlock_bh(&g_qos->lock);
			ppe_qos_warn("%px setting shaper: %s configuration failed for interface", info, info->shaper_name);
			return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
		}

		strlcpy(tm_if->shaper_name, info->shaper_name, PPE_QOS_MAX_NAME_LENGTH);
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_success);
		spin_unlock_bh(&g_qos->lock);
		return PPE_QOS_SUCCESS;
	}

	/*
	 * Delete shaper from the interface if shaper name is empty
	 */
	if (ppe_qos_delete_interface_shaper(tm_if)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px setting shaper: %s configuration failed for interface", info, info->shaper_name);
		return PPE_QOS_SET_INTERFACE_SHAPER_FAIL;
	}

	ppe_qos_stats_inc(&g_qos->stats.qos_set_interface_shaper_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_warn("%px setting shaper: %s configuration success for interface", info, info->shaper_name);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_set_interface_shaper);

/*
 * ppe_qos_flush_interface_queues()
 *	Delete/flush priority queues for a T-cont/UNI.
 */
ppe_qos_ret_t ppe_qos_flush_interface_queues(struct ppe_qos_interface_queues_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	int id;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_flush_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid command info");
		return PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL;
	}

	id = ppe_qos_get_interface_id(&info->if_data);
	if (id < 0) {
		ppe_qos_stats_inc(&g_qos->stats.qos_flush_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL;
	}

	tm_if = ppe_qos_res_get(id, info->if_data.type);
	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_flush_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px interface info is not valid", info);
		return PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL;
	}

	if (ppe_qos_delete_interface_queues(tm_if)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_flush_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px port queues reset failed", info);
		return PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL;
	}

	if (ppe_qos_delete_interface_shaper(tm_if)) {
		ppe_qos_stats_inc(&g_qos->stats.qos_flush_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px delete interface shaper failed", info);
		return PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL;
	}

	if (tm_if->type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		list_del(&tm_if->list);
		kfree(tm_if);
	}

	ppe_qos_stats_inc(&g_qos->stats.qos_flush_interface_queues_success);
	spin_unlock_bh(&g_qos->lock);

	ppe_qos_warn("%px flush interface queues success", info);
	return PPE_QOS_SUCCESS;
}
EXPORT_SYMBOL(ppe_qos_flush_interface_queues);

/*
 * ppe_qos_create_interface_queues()
 *	Create priority queues for a T-cont/UNI.
 */
ppe_qos_ret_t ppe_qos_create_interface_queues(struct ppe_qos_interface_queues_info *info)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_qos_interface_queue *queue = NULL;
	struct ppe_drv_qos_port *port = NULL;
	struct list_head *queue_list = NULL;
	uint32_t *num_queues_ptr = NULL;
	ppe_qos_queue_type_t queue_type;
	uint32_t i;
	int id;

	spin_lock_bh(&g_qos->lock);
	if (!info) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid command info");
		return PPE_QOS_CREATE_INTERFACE_QUEUES_FAIL;
	}

	if (!info->num_queues) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("invalid interface queues");
		return PPE_QOS_CREATE_INTERFACE_QUEUES_FAIL;
	}

	/* Get queue type from info structure, default to unicast if not specified */
	queue_type = info->queue_type;

	id = ppe_qos_get_interface_id(&info->if_data);
	if (id < 0) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL;
	}

	tm_if = ppe_qos_res_get(id, info->if_data.type);
	if ((!tm_if) && (info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP)) {
		ppe_qos_trace("%px allocate resource for port:%d", info, id);
		tm_if = ppe_qos_res_alloc(id);
	}

	if (!tm_if) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%px invalid interface data", info);
		return PPE_QOS_CREATE_INTERFACE_QUEUES_FAIL;
	}

	port = &tm_if->port;
	if (!port) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		ppe_qos_warn("%px tcont info is not valid", info);
		goto res_free;
	}

#ifndef NSS_PPE_PON_SUPPORT
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		ppe_qos_warn("%px Tcont not supported", info);
		goto res_free;
	}
#else
	/*
	 * Check Tcont queue limit
	 */
	if ((info->if_data.type == PPE_QOS_INTERFACE_TYPE_TCONT) &&
		(info->num_queues + g_qos->pon_port_num_pq > ppe_drv_pon_port_max_pq_get())) {
			ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
			ppe_qos_warn("%px invalid number of queues: %d", info, info->num_queues);
			goto res_free;
	}
#endif

	/*
	 * Validate multicast constraints
	 */
	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		if (info->if_data.type != PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
			ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_create_fail);
			ppe_qos_warn("%px multicast queues not supported on T-cont", info);
			goto res_free;
		}

		if (info->num_queues > port->max[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE]) {
			ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_create_fail);
			ppe_qos_warn("Multicast queues %u exceed max queues %u\n",
				     info->num_queues, PPE_QOS_MAX_MCAST_QUEUES_PER_UNI);
			goto res_free;
		}
	}

	/*
	 * Validate unicast constraints for physical ports
	 */
	if ((info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) &&
		(info->num_queues > port->max[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE])) {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		ppe_qos_warn("%px invalid number of queues: %d", info, info->num_queues);
		goto res_free;
	}

	/* Get appropriate list and counter based on queue type */
	queue_list = ppe_qos_get_queue_list(tm_if, queue_type);
	num_queues_ptr = ppe_qos_get_num_queues_ptr(tm_if, queue_type);

	/* Check if queues of this type already exist */
	if (!list_empty(queue_list)) {
		if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
			ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_create_fail);
		} else {
			ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
		}
		ppe_qos_warn("Queues of type %s already assigned to interface %u",
			     ppe_qos_get_queue_type_str(queue_type), id);
		goto res_free;
	}

	/*
	 * Allocate queues in QoS DB
	 */
	for (i = 0; i < info->num_queues; i++) {
		queue = kzalloc(sizeof(struct ppe_qos_interface_queue), GFP_ATOMIC);
		if (!queue) {
			if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
				ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_create_fail);
			} else {
				ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_fail);
			}

			/* Clean up allocated queues */
			while (!list_empty(queue_list)) {
				queue = list_first_entry(queue_list, struct ppe_qos_interface_queue, list);
				list_del(&queue->list);
				kfree(queue);
				(*num_queues_ptr)--;
			}
			ppe_qos_warn("Queue allocation failed for %s queues on interface %u",
				     ppe_qos_get_queue_type_str(queue_type), id);
			goto res_free;
		}

		if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
			queue->offset = i;
			queue->valid = true;
		} else {
			g_qos->pon_port_num_pq++;
		}

		list_add(&queue->list, queue_list);
		(*num_queues_ptr)++;
	}

	/* Mark interface as valid if not already */
	if (!tm_if->valid) {
		tm_if->valid = true;
	}

	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		ppe_qos_stats_inc(&g_qos->stats.qos_mcast_queue_create_success);
	} else {
		ppe_qos_stats_inc(&g_qos->stats.qos_create_interface_queues_success);
	}

	spin_unlock_bh(&g_qos->lock);

	ppe_qos_info("Created %u %s queues for interface type:%d id:%d",
		     info->num_queues, ppe_qos_get_queue_type_str(queue_type),
		     info->if_data.type, id);

	return PPE_QOS_SUCCESS;

res_free:
	if (info->if_data.type == PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP) {
		if (!tm_if->valid) {
			list_del(&tm_if->list);
			kfree(tm_if);
		}
	}

	spin_unlock_bh(&g_qos->lock);
	return PPE_QOS_CREATE_INTERFACE_QUEUES_FAIL;
}
EXPORT_SYMBOL(ppe_qos_create_interface_queues);

/*
 * ppe_qos_get_int_pri_func()
 *	Get int_pri and other details of PPE queue corresponding to the handle_id/class_id of input dev.
 *
*/
ppe_qos_ret_t ppe_qos_get_int_pri_func(struct ppe_qos_req *req)
{
	char dev_name[IFNAMSIZ] = {0};
	struct ppe_drv_queue_info q_info = {0};
	struct net_device *dev;

	strlcpy(dev_name, req->dev, IFNAMSIZ);
	if (dev_name[strlen(dev_name) - 1] == '\n') {
		dev_name[strlen(dev_name) - 1] = '\0';
	}

	dev = dev_get_by_name(&init_net, dev_name);
	q_info.handle_id = req->handle_id;
	if(!dev){
		ppe_qos_warn("DEVICE NOT FOUND FOR dev= %s\n", dev_name);
		return PPE_QOS_INVALID_DEV;
	}

	if (ppe_drv_qos_queue_info_get(dev, q_info.handle_id, &q_info)) {
		if (!q_info.valid) {
			ppe_qos_warn("%px:PPE qdisc are attached to only leaf classes.\n"
					"Classid %d is not a leaf class on %s interface", dev, q_info.handle_id, dev_name);
			dev_put(dev);
			return PPE_QOS_CLASS_NON_LEAF;
		}

		req->int_pri = q_info.int_pri;
		req->port_id = q_info.port_id;
		req->ucast_qid = q_info.ucast_qid;
		ppe_qos_info("%px:PPE DRV API called for dev = %s and handle_id = %d and the returned values are:"
				"\n int_pri = %d \n ucast_qid = %d\n", dev, dev_name, req->handle_id, req->int_pri, req->ucast_qid);
		dev_put(dev);
		return PPE_QOS_SUCCESS;
	}

	dev_put(dev);
	return PPE_QOS_INVALID_HANDLE_ID;
}
EXPORT_SYMBOL(ppe_qos_get_int_pri_func);

/*
 * ppe_qos_deinit()
 *	Qos deinit API
 */
void ppe_qos_deinit(void)
{
#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Unregister the PON port QoS resource allocation callback before
	 * tearing down resources to prevent a race with gem_enable().
	 */
	ppe_drv_qos_pon_port_alloc_callback_unregister();
#endif
	ppe_qos_stats_debugfs_exit();
	ppe_qos_res_deinit();
	ppe_qos_dump_exit();
}

/*
 * ppe_qos_init()
 *	qos init API
 */
void ppe_qos_init(struct dentry *d_rule)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;

	spin_lock_init(&g_qos->lock);
	INIT_LIST_HEAD(&g_qos->shaper_list);
	ppe_qos_stats_debugfs_init(d_rule);
	ppe_qos_res_init();
	ppe_qos_dump_init(d_rule);

#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Register the callback so that ppe_drv_dp_gem_enable() can trigger
	 * PON port QoS resource allocation after the PON port is configured in HW.
	 * The fallback for already-enabled PON port is handled inside ppe_qos_res_init().
	 */
	ppe_drv_qos_pon_port_alloc_callback_register(ppe_qos_pon_port_res_init);
#endif
}
