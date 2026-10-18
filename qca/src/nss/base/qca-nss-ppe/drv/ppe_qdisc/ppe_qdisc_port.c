/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_qdisc.h"

static struct ppe_qdisc_port ppe_qdisc_port[PPE_DRV_QOS_PORT_MAX];

/*
 * ppe_qdisc_port_res_entries_free()
 *	Free all the resource for a given port.
 */
static void ppe_qdisc_port_res_entries_free(uint32_t port, ppe_drv_qos_res_type_t type)
{
	struct ppe_qdisc_port_res *res;
	struct ppe_qdisc_port_res *head = NULL;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port];

	spin_lock_bh(&ppe_port->lock);
	head = ppe_port->res_free[type];
	while (head) {
		res = head;
		head = head->next;
		kfree(res);
	}

	ppe_port->res_free[type] = NULL;
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, type:%d", port, type);
}

/*
 * ppe_qdisc_port_res_entries_alloc()
 *	Allocates all the resources for a given port.
 */
static struct ppe_qdisc_port_res *ppe_qdisc_port_res_entries_alloc(uint32_t port, ppe_drv_qos_res_type_t type)
{
	struct ppe_qdisc_port_res *res = NULL;
	struct ppe_qdisc_port_res *next = NULL;
	uint32_t i;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port];
	uint32_t max = ppe_port->port.max[type];

	spin_lock_bh(&ppe_port->lock);
	for (i = max; i > 0; i--) {
		res = kzalloc(sizeof(struct ppe_qdisc_port_res), GFP_ATOMIC);
		if (!res) {
			ppe_qdisc_warning("Free queue list allocation failed for port %u", port);
			goto fail;
		}

		res->offset = i - 1;
		res->type = type;
		res->next = next;
		next = res;

	}
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_trace("port:%d, type:%d", port, type);
	return res;

fail:
	while (next) {
		res = next;
		next = next->next;
		kfree(res);
	}
	spin_unlock_bh(&ppe_port->lock);
	return NULL;
}

/*
 * ppe_qdisc_port_res_attach_free()
 *	Attaches a resource to free list.
 */
static void ppe_qdisc_port_res_attach_free(uint32_t port, struct ppe_qdisc_port_res *res)
{
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port];

	if (res->type >= PPE_DRV_QOS_RES_TYPE_MAX) {
		ppe_qdisc_trace("Invalid type for port:%d, res-:%px type:%d", port, res, res->type);
		return;
	}
	spin_lock_bh(&ppe_port->lock);
	res->next = ppe_port->res_free[res->type];
	ppe_port->res_free[res->type] = res;
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, type:%d, res:%px", port, res->type, res);
	return;
}

/*
 * ppe_qdisc_port_res_max_get()
 *	Returns maximum number of the particular resource for a given port.
 */
uint32_t ppe_qdisc_port_res_max_get(uint32_t port_id, ppe_drv_qos_res_type_t type)
{
	uint32_t max = 0;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	spin_lock_bh(&ppe_port->lock);
	max = ppe_port->port.max[type];
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, type:%d, max:%d", port_id, type, max);
	return max;
}

/*
 * ppe_qdisc_port_disable_all_queue()
 *	Disables all queues corresponding to a port.
 */
void ppe_qdisc_port_disable_all_queue(uint32_t port_id)
{
	uint32_t qid = ppe_qdisc_port_res_base_get(port_id, PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE);
	uint32_t mcast_qid = ppe_qdisc_port_res_base_get(port_id, PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE);
	uint32_t offset;

	/*
	 * Disable queue enqueue, dequeue and flush the queue.
	 */
	for (offset = 0; offset < ppe_qdisc_port_res_max_get(port_id, PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE); offset++) {
		ppe_drv_qos_queue_disable(port_id, qid + offset);
	}

	for (offset = 0; offset < ppe_qdisc_port_res_max_get(port_id, PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE); offset++) {
		ppe_drv_qos_queue_disable(port_id, mcast_qid + offset);
	}

	ppe_qdisc_info("Port:%d all queues disabled", port_id);
}

/*
 * ppe_qdisc_port_enable_all_queue()
 *	Enables all level L0 queues corresponding to a port.
 */
void ppe_qdisc_port_enable_all_queue(uint32_t port_id)
{
	uint32_t qid = ppe_qdisc_port_res_base_get(port_id, PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE);
	uint32_t mcast_qid = ppe_qdisc_port_res_base_get(port_id, PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE);
	uint32_t offset;

	/*
	 * Enable queue enqueue and dequeue.
	 */
	for (offset = 0; offset < ppe_qdisc_port_res_max_get(port_id, PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE); offset++) {
		ppe_drv_qos_queue_enable(qid + offset);
	}

	for (offset = 0; offset < ppe_qdisc_port_res_max_get(port_id, PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE); offset++) {
		ppe_drv_qos_queue_enable(mcast_qid + offset);
	}

	ppe_qdisc_info("Port:%d all queues enabled", port_id);
}

/*
 * ppe_qdisc_port_enable_assigned_queue()
 *	Enables assigned level L0 queues corresponding to a port.
 */
void ppe_qdisc_port_enable_assigned_queue(uint32_t port_id)
{
	uint32_t qid = ppe_qdisc_port_res_base_get(port_id, PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE);
	uint32_t mcast_qid = ppe_qdisc_port_res_base_get(port_id, PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE);
	struct ppe_qdisc_port_res *res = NULL;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	/*
	 * Enable assigned queues enqueue and dequeue.
	 */
	spin_lock_bh(&ppe_port->lock);
	res = ppe_port->res_used[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];
	while (res) {
		ppe_drv_qos_queue_enable(qid + res->offset);
		res = res->next;
	}

	res = ppe_port->res_used[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE];
	while (res) {
		ppe_drv_qos_queue_enable(mcast_qid + res->offset);
		res = res->next;
	}
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("Port:%d all assigned queues enabled", port_id);
}

/*
 * ppe_qdisc_port_res_base_get()
 *	Returns base of the particular resource for a given port.
 */
uint32_t ppe_qdisc_port_res_base_get(uint32_t port_id, ppe_drv_qos_res_type_t type)
{
	uint32_t base = 0;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	spin_lock_bh(&ppe_port->lock);
	base = ppe_port->port.base[type];
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, type:%d, base:%d", port_id, type, base);
	return base;
}

/*
 * ppe_qdisc_port_res_drr_deref()
 *	Get and deref DRR mapped to respective priority
 */
int ppe_qdisc_port_res_drr_deref(uint32_t port_id, struct ppe_drv_qos_res *res, ppe_drv_qos_res_type_t type)
{
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];
	struct ppe_drv_qos_port_drr_map *map = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? ppe_port->l1map : ppe_port->l0map;
	struct ppe_drv_qos_port_drr_map *prev = map;
	uint32_t spid = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? port_id : res->l0spid;
	uint32_t drr_id = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? res->scheduler.l1c_drrid : res->scheduler.l0c_drrid;

	/*
	 * Find the DRR ID mapped to SP and Priority
	 */
	spin_lock_bh(&ppe_port->lock);
	while (map) {
		if ((map->sp_id == spid) && (map->drr_id == drr_id) && (map->priority == res->scheduler.priority)) {
			map->ref_cnt--;
			goto free_map;
		}

		prev = map;
		map = map->next;
	}

	spin_unlock_bh(&ppe_port->lock);
	ppe_qdisc_warning("port:%d, type:%d failed to get DRR mapping ", port_id, type);
	return -EINVAL;

free_map:
	uint32_t res_type = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? PPE_DRV_QOS_RES_TYPE_L1_CDRR : PPE_DRV_QOS_RES_TYPE_L0_CDRR;
	/*
	 * Access port.base directly since ppe_port->lock is already held here.
	 * Calling ppe_qdisc_port_res_base_get() would cause a recursive lock acquisition.
	 */
	uint32_t offset = drr_id - ppe_port->port.base[res_type];

	if(map->ref_cnt) {
		spin_unlock_bh(&ppe_port->lock);
		ppe_qdisc_info("port:%d, type:%d id:%d drr_id:%d priority:%d ref:%d DRR unmapping successful", port_id, type, spid, drr_id, map->priority, map->ref_cnt);
		return 0;
	}

	if (prev == map) {
		if (type == PPE_DRV_QOS_RES_TYPE_L0_SP) {
			ppe_port->l1map = map->next;
		} else {
			ppe_port->l0map = map->next;
		}
	} else {
		prev->next = map->next;
	}

	kfree(map);
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_port_res_free(port_id, offset, res_type);

	ppe_qdisc_info("port:%d, type:%d id:%d drr_id:%d priority:%d ref:%d DRR unmapping successful", port_id, type, spid, drr_id, map->priority, map->ref_cnt);
	return 0;
}

/*
 * ppe_qdisc_port_res_drr_ref()
 *	Get, set and ref DRR mapped to respective priority
 */
int ppe_qdisc_port_res_drr_ref(uint32_t port_id, struct ppe_drv_qos_res *res, ppe_drv_qos_res_type_t type)
{
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];
	struct ppe_drv_qos_port_drr_map *map = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? ppe_port->l1map : ppe_port->l0map;
	struct ppe_drv_qos_port_drr_map *map_iter = map;
	uint32_t spid = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? port_id : res->l0spid;

	/*
	 * Find the DRR ID mapped to SP and Priority
	 */
	spin_lock_bh(&ppe_port->lock);
	while (map_iter) {
		if ((map_iter->sp_id == spid) && (map_iter->priority == res->scheduler.priority)) {
			goto drr_found;
		}

		map_iter = map_iter->next;
	}

	/*
	 * DRR-ID mapped to priority not found, create a new map if DRR resource is available.
	 * Release the lock before calling ppe_qdisc_port_res_alloc() and kzalloc() to avoid
	 * recursive lock acquisition (both functions acquire ppe_port->lock internally).
	 */
	spin_unlock_bh(&ppe_port->lock);

	uint32_t res_type = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? PPE_DRV_QOS_RES_TYPE_L1_CDRR : PPE_DRV_QOS_RES_TYPE_L0_CDRR;
	struct ppe_qdisc_port_res *drr = NULL;

	drr = ppe_qdisc_port_res_alloc(port_id, res_type);
	if (!drr) {
		ppe_qdisc_warning("port:%d, type:%d failed to get DRR", port_id, type);
		return -EINVAL;
	}

	map_iter = kzalloc(sizeof(struct ppe_drv_qos_port_drr_map), GFP_ATOMIC);
	if (!map_iter) {
		ppe_qdisc_port_res_free(port_id, drr->offset, res_type);
		ppe_qdisc_warning("Port:%d Map allocation failed\n", port_id);
		return -EINVAL;
	}

	map_iter->level = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? PPE_DRV_QOS_FLOW_LEVEL - 1 : PPE_DRV_QOS_QUEUE_LEVEL - 1;
	map_iter->sp_id = spid;
	map_iter->priority = res->scheduler.priority;
	map_iter->drr_id = ppe_qdisc_port_res_base_get(port_id, res_type) + drr->offset;

	/*
	 * Re-acquire the lock before inserting the new entry into the map list.
	 * Use the current list head (not the stale 'map' snapshot) in case it
	 * changed while the lock was released.
	 */
	spin_lock_bh(&ppe_port->lock);
	map_iter->next = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? ppe_port->l1map : ppe_port->l0map;

	if (type == PPE_DRV_QOS_RES_TYPE_L0_SP) {
		ppe_port->l1map = map_iter;
	} else {
		ppe_port->l0map = map_iter;
	}

drr_found:
	if (type == PPE_DRV_QOS_RES_TYPE_L0_SP) {
		res->scheduler.l1c_drrid = map_iter->drr_id;
		res->scheduler.l1e_drrid = map_iter->drr_id;
	} else {
		res->scheduler.l0c_drrid = map_iter->drr_id;
		res->scheduler.l0e_drrid = map_iter->drr_id;
	}

	map_iter->ref_cnt++;
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, type:%d id:%d drr_id:%d priority:%d ref:%d DRR mapping successful", port_id, type, spid, map_iter->drr_id, map_iter->priority, map_iter->ref_cnt);
	return 0;
}

/*
 * ppe_qdisc_port_res_drr_map_free()
 *	Free the DRR map for a given port.
 */
static void ppe_qdisc_port_res_drr_map_free(uint32_t port_id, ppe_drv_qos_res_type_t type)
{
	struct ppe_drv_qos_port_drr_map *map, *head = NULL;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	head = (type == PPE_DRV_QOS_RES_TYPE_L0_SP) ? ppe_port->l1map : ppe_port->l0map;

	while (head) {
		map = head;
		head = head->next;
		kfree(map);
	}

	ppe_qdisc_info("port:%d, type:%d, DRR MAP freed", port_id, type);
}

/*
 * ppe_qdisc_port_res_all_drr_map_free()
 *	Frees the allocated DRR maps.
 */
void ppe_qdisc_port_res_all_drr_map_free(uint32_t port_id)
{
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	/*
	 * Free drr map
	 */
	spin_lock_bh(&ppe_port->lock);
	if (ppe_port->l0map) {
		ppe_qdisc_port_res_drr_map_free(port_id, PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE);
		ppe_port->l0map = NULL;
	}

	if (ppe_port->l1map) {
		ppe_qdisc_port_res_drr_map_free(port_id, PPE_DRV_QOS_RES_TYPE_L0_SP);
		ppe_port->l1map = NULL;
	}
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, DRR MAP freed", port_id);
}

/*
 * ppe_qdisc_port_res_free()
 *	Frees the allocated resource and attach it to free list.
 */
int ppe_qdisc_port_res_free(uint32_t port_id, uint32_t offset, ppe_drv_qos_res_type_t type)
{
	struct ppe_qdisc_port_res *temp = NULL;
	struct ppe_qdisc_port_res *res = NULL;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	if (type >= PPE_DRV_QOS_RES_TYPE_MAX) {
		ppe_qdisc_assert(false, "Resource type:%d not valid for port:%d", type, port_id);
		return -1;
	}

	spin_lock_bh(&ppe_port->lock);
	res = ppe_port->res_used[type];
	if (res->offset == offset) {
		ppe_port->res_used[type] = res->next;
		res->next = NULL;
		spin_unlock_bh(&ppe_port->lock);
		goto success;
	}

	temp = res;
	res = res->next;

	while (res) {
		if (res->offset == offset) {
			temp->next = res->next;
			res->next = NULL;
			break;
		} else {
			temp = res;
			res = res->next;
		}
	}
	spin_unlock_bh(&ppe_port->lock);

	if (!res) {
		ppe_qdisc_assert(false, "Resource:%d type:%d not found for port:%d", offset, type, port_id);
		return -1;
	}

success:
	ppe_qdisc_port_res_attach_free(port_id, res);
	ppe_qdisc_info("port:%d, type:%d, res:%px", port_id, type, res);
	return 0;
}

/*
 * ppe_qdisc_port_res_alloc()
 *	Allocates free resource for a given port.
 */
struct ppe_qdisc_port_res *ppe_qdisc_port_res_alloc(uint32_t port_id, ppe_drv_qos_res_type_t type)
{
	struct ppe_qdisc_port_res *res = NULL;
	struct ppe_qdisc_port *ppe_port = &ppe_qdisc_port[port_id];

	if (type >= PPE_DRV_QOS_RES_TYPE_MAX) {
		ppe_qdisc_assert(false, "Resource type:%d not valid for port:%d", type, port_id);
		return NULL;
	}

	/*
	 * Detach the resource from free list
	 * and attach to used list.
	 */
	spin_lock_bh(&ppe_port->lock);
	res = ppe_port->res_free[type];
	if (res) {
		ppe_port->res_free[type] = res->next;
		res->next = ppe_port->res_used[type];
		ppe_port->res_used[type] = res;
	}
	spin_unlock_bh(&ppe_port->lock);

	ppe_qdisc_info("port:%d, type:%d, res:%px", port_id, type, res);
	return res;
}

/*
 * ppe_qdisc_port_id_get()
 *	Returns the port number associated with the net device.
 */
int32_t ppe_qdisc_port_id_get(struct net_device *dev)
{
	int32_t port_id;

	struct ppe_drv_iface *iface = ppe_drv_iface_get_by_dev(dev);
	if (!iface) {
		ppe_qdisc_warning("%px: %s: couldn't get PPE iface", dev, dev->name);
		return -1;
	}

	/*
	 * Support is not provided for CPU port (port 0) and EIP port (Port 7).
	 */
	port_id = ppe_drv_iface_port_idx_get(iface);
	if (port_id > 0 && port_id < (PPE_DRV_QOS_PORT_MAX - 1)) {
		ppe_qdisc_info("%px: %s:%d is valid port", dev, dev->name, port_id);
		return port_id;
	}

	ppe_qdisc_warning("%px: %s:%d is not valid PPE port", dev, dev->name, port_id);
	return -1;
}

/*
 * ppe_qdisc_port_default_conf_set()
 *	Sets default queue scheduler in PPE.
 */
int ppe_qdisc_port_default_conf_set(uint32_t port_id)
{
	/*
	 * Disable all queues and set PPE configuration
	 * We need to disable and flush the queues before
	 * reseting to the default configuration.
	 */
	ppe_qdisc_port_disable_all_queue(port_id);

	/*
	 * Invoke PPE API to reset the default configuration for a given port.
	 */
	if (ppe_drv_qos_default_conf_set(port_id) != 0) {
		ppe_qdisc_warning("Port:%d reset default queue configuration failed", port_id);
		ppe_qdisc_port_enable_assigned_queue(port_id);
		return -EINVAL;
	}

	ppe_qdisc_port_enable_all_queue(port_id);

	ppe_qdisc_info("Port:%d queue configuration successful", port_id);
	return 0;
}

/*
 * ppe_qdisc_port_free()
 *	Free PPE ports
 */
int ppe_qdisc_port_free(void)
{
	uint32_t i, j;

	for (i = 1; i < PPE_DRV_QOS_PORT_MAX - 1; i++) {
		for (j = 0; j <= PPE_DRV_QOS_RES_TYPE_L0_SP; j++) {
			ppe_qdisc_port_res_entries_free(i, j);
		}

		/*
		 * Free drr map
		 */
		ppe_qdisc_port_res_all_drr_map_free(i);
	}

	ppe_qdisc_info("PPE ports resources freed");
	return 0;
}

/*
 * ppe_qdisc_port_alloc()
 *	Initializes PPE ports
 */
int ppe_qdisc_port_alloc(void)
{
	int j, type;
	int i = 0;

	ppe_qdisc_info("Allocating PPE ports resources");
	memset(&ppe_qdisc_port, 0, sizeof(struct ppe_qdisc_port) * PPE_DRV_QOS_PORT_MAX);

	/*
	 * Allocate resources for ethernet ports.
	 */
	for (i = 1; i < PPE_DRV_QOS_PORT_MAX - 1; i++) {
		ppe_qdisc_info("Resource allocation for port %u", i);
		spin_lock_init(&ppe_qdisc_port[i].lock);
		if (ppe_drv_qos_port_res_get(i, &ppe_qdisc_port[i].port) != PPE_DRV_RET_SUCCESS) {
			ppe_qdisc_warning("Fetching of port scheduler resource information failed for port:%u", i);
			goto failure;
		}

		for (type = 0; type < PPE_DRV_QOS_RES_TYPE_L1_SP; type++) {
			ppe_qdisc_port[i].res_free[type] = ppe_qdisc_port_res_entries_alloc(i, type);
			if (!ppe_qdisc_port[i].res_free[type]) {
				ppe_qdisc_warning("Resource list allocation failed for port:%u type:%u", i, type);
				goto failure;
			}
		}
	}

	ppe_qdisc_info("PPE ports resource allocation completed");
	return 0;

failure:
	/*
	 * Free other port resources
	 */
	while (i > 0) {
		for (j = 0; j < PPE_DRV_QOS_RES_TYPE_L1_SP; j++) {
			ppe_qdisc_port_res_entries_free(i, j);
		}
		i--;
	}
	return -EINVAL;
}
