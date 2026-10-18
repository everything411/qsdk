/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "ppe_drv.h"
#include <fal/fal_fdb.h>
#include <linux/seq_file.h>

/*
 * ppe_drv_port_mgmt_entries_init()
 * 	Initialize the values for port_isol tables.
 */
void ppe_drv_port_mgmt_entries_init(struct ppe_drv_port_mgmt *port_mgmt)
{
#ifdef NSS_PPE_DRV_PORT_ISOL_SUPPORT
	/*
	 * Initialize:
	 *
	 * OMCI index map: default all ports to invalid index
	 * Isolation bitmaps: disable subgroup flooding for all ports
	 * OMCI index info: mark all entries as invalid
	 * VP profile info: mark all entries as invalid
	 *
	 */
	for (size_t id = 0; id < PPE_DRV_PORTS_MAX; id++) {
		port_mgmt->port_omci_index_map[id] = (uint8_t)PPE_DRV_OMCI_INDEX_MAX;
	}

	for (size_t i = 0; i < PPE_DRV_OMCI_INDEX_MAX; i++) {
		port_mgmt->port_phy_isol_bitmap[i] = 0x0;
		port_mgmt->port_vp_isol_bitmap[i]  = 0x0;
	}

	for (size_t i = 0; i < PPE_DRV_OMCI_INDEX_MAX; i++) {
		port_mgmt->omci_index_info[i].omci_index = (uint8_t)PPE_DRV_OMCI_INDEX_MAX;
		port_mgmt->omci_index_info[i].in_use = false;
	}

	for (size_t i = 0; i < PPE_DRV_VP_PROFILE_ID_MAX; i++) {
		port_mgmt->vp_profile_id_info[i].profile_id = (uint8_t)PPE_DRV_VP_PROFILE_ID_MAX;
		port_mgmt->vp_profile_id_info[i].in_use = false;
	}

	/*
	 * Clear action tables: default set flooding
	 */
	memset(port_mgmt->port_phy_isol_action, 0xAA, sizeof(port_mgmt->port_phy_isol_action));
#endif
}

/*
 * ppe_drv_port_mgmt_entries_alloc()
 * 	Allocate the memory for port mgmt.
 */
struct ppe_drv_port_mgmt *ppe_drv_port_mgmt_entries_alloc(void)
{
	struct ppe_drv_port_mgmt *port_mgmt;

	port_mgmt = kzalloc(sizeof(*port_mgmt), GFP_KERNEL);
	if (!port_mgmt) {
		pr_warn("Port Mgmt: Failed to allocate table entries\n");
		return NULL;
	}

	spin_lock_init(&port_mgmt->lock);
	INIT_LIST_HEAD(&port_mgmt->active_rules);
	INIT_LIST_HEAD(&port_mgmt->omci_port_list);
	INIT_LIST_HEAD(&port_mgmt->mac_filter_cache);
	ppe_drv_port_mgmt_entries_init(port_mgmt);
	return port_mgmt;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_entries_alloc);

/*
 * ppe_drv_port_mgmt_entries_free()
 * 	Free the memory for Port Mgmt
 */
void ppe_drv_port_mgmt_entries_free(struct ppe_drv_port_mgmt *port_mgmt)
{
	if (!port_mgmt)
		return;

	ppe_drv_port_mgmt_omci_port_flush();

	/*
	 * Free all cached MAC filter nodes.
	 */
	struct ppe_drv_mac_filter_node *node, *tmp;
	list_for_each_entry_safe(node, tmp, &port_mgmt->mac_filter_cache, list) {
		list_del(&node->list);
		kfree(node);
	}
	kfree(port_mgmt);
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_entries_free);

/*
 * ppe_drv_port_mgmt_omci_port_add()
 *	Add a port to the OMCI-managed port list.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_omci_port_add(const char *dev_name)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
	struct ppe_drv_omci_port_node *node;
	struct net_device *dev;
	int32_t port_id;

	if (!dev_name) {
		ppe_drv_warn("Invalid argument: dev_name is NULL\n");
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	if (!port_mgmt) {
		ppe_drv_warn("port_mgmt context not initialized\n");
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	dev = dev_get_by_name(&init_net, dev_name);
	if (!dev) {
		ppe_drv_warn("Device '%s' not found\n", dev_name);
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	port_id = ppe_drv_port_num_from_dev(dev);
	dev_put(dev);
	if (port_id < 0) {
		ppe_drv_warn("Failed to get PPE port number for dev '%s'\n", dev_name);
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	/*
	 * Duplicate check
	 */
	spin_lock_bh(&port_mgmt->lock);
	list_for_each_entry(node, &port_mgmt->omci_port_list, list) {
		if (strncmp(node->dev_name, dev_name, IFNAMSIZ) == 0) {
			spin_unlock_bh(&port_mgmt->lock);
			ppe_drv_warn("Device '%s' already in OMCI port list\n", dev_name);
			return PPE_DRV_RET_FAILURE_INVALID_PARAM;
		}
	}
	spin_unlock_bh(&port_mgmt->lock);

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		ppe_drv_warn("Failed to allocate OMCI port node for '%s'\n", dev_name);
		return PPE_DRV_RET_FAILURE_NO_RESOURCE;
	}

	strlcpy(node->dev_name, dev_name, IFNAMSIZ);
	node->port_id = (uint32_t)port_id;

	spin_lock_bh(&port_mgmt->lock);
	list_add_tail(&node->list, &port_mgmt->omci_port_list);
	spin_unlock_bh(&port_mgmt->lock);

	ppe_drv_info("Added OMCI port: dev='%s' port_id=%u\n", node->dev_name, node->port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_omci_port_add);

/*
 * ppe_drv_port_mgmt_omci_port_del()
 *	Remove a port from the OMCI-managed port list.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_omci_port_del(const char *dev_name)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
	struct ppe_drv_omci_port_node *node, *tmp;
	bool found = false;

	if (!dev_name) {
		ppe_drv_warn("Invalid argument: dev_name is NULL\n");
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	if (!port_mgmt) {
		ppe_drv_warn("port_mgmt context not initialized\n");
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	spin_lock_bh(&port_mgmt->lock);
	list_for_each_entry_safe(node, tmp, &port_mgmt->omci_port_list, list) {
		if (strncmp(node->dev_name, dev_name, IFNAMSIZ) == 0) {
			found = true;
			if (atomic_read(&node->refcount) == 0) {
				ppe_drv_info("Removed OMCI port: dev='%s' port_id=%u\n",
				     node->dev_name, node->port_id);
				list_del(&node->list);
				kfree(node);
				break;
			} else {
				spin_unlock_bh(&port_mgmt->lock);
				ppe_drv_warn("Device '%s' is use\n", node->dev_name);
				return PPE_DRV_RET_PORT_MGMT_PORT_IN_USE;
			}
		}
	}
	spin_unlock_bh(&port_mgmt->lock);

	if (!found) {
		ppe_drv_warn("Device '%s' not found in OMCI port list\n", dev_name);
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_omci_port_del);

/*
 * ppe_drv_port_mgmt_omci_port_flush()
 *	Remove all ports from the OMCI-managed port list.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_omci_port_flush(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
	struct ppe_drv_omci_port_node *node, *tmp;
	bool in_use = false;

	if (!port_mgmt) {
		ppe_drv_warn("port_mgmt context not initialized\n");
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	spin_lock_bh(&port_mgmt->lock);
	list_for_each_entry_safe(node, tmp, &port_mgmt->omci_port_list, list) {
		if (atomic_read(&node->refcount) == 0) {
			list_del(&node->list);
			ppe_drv_info("Flushed OMCI port: dev='%s' port_id=%u\n",
			     node->dev_name, node->port_id);
			kfree(node);
		} else {
			ppe_drv_warn("Device '%s' in use\n", node->dev_name);
			in_use = true;
		}
	}

	spin_unlock_bh(&port_mgmt->lock);

	if (in_use) {
		return PPE_DRV_RET_PORT_MGMT_PORT_IN_USE;
	}

	ppe_drv_info("OMCI port list flushed\n");
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_omci_port_flush);

/*
 * ppe_drv_port_mgmt_isol_dump()
 *	Dump port isolation state grouped by OMCI ID.
 *	For each active OMCI index, prints the omci_id, port_cnt, and
 *	the dev_name + port_id of every port mapped to that OMCI index.
 */
void ppe_drv_port_mgmt_isol_dump(struct seq_file *m)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt;
	uint16_t omci_idx;
	uint32_t port_id;
	bool found_any = false;

	if (!p || !p->port_mgmt) {
		seq_puts(m, "PPE driver not initialized\n");
		return;
	}

	port_mgmt = p->port_mgmt;

	seq_puts(m, "  [Port Isolation State]\n");
	seq_puts(m, "  ----------------------------------------\n");

	spin_lock_bh(&port_mgmt->lock);

	for (omci_idx = 0; omci_idx < PPE_DRV_OMCI_INDEX_MAX; omci_idx++) {
		if (!port_mgmt->omci_index_info[omci_idx].in_use)
			continue;

		found_any = true;
		seq_printf(m, "\nomci_id: %u  omci_index: %u  port_cnt: %u\n",
			   port_mgmt->omci_index_info[omci_idx].omci_id,
			   omci_idx,
			   port_mgmt->omci_index_info[omci_idx].port_cnt);
		seq_printf(m, "  %-16s  %-10s  %-6s\n", "dev_name", "port_id", "type");
		seq_puts(m, "  ----------------------------------------\n");

		for (port_id = 0; port_id < PPE_DRV_PORTS_MAX; port_id++) {
			if (port_mgmt->port_omci_index_map[port_id] != omci_idx)
				continue;

			if (PPE_DRV_PHY_PORT_CHK(port_id)) {
				struct net_device *dev = ppe_drv_port_num_to_dev(port_id);
				seq_printf(m, "  %-16s  %-10u  %-6s\n",
					   dev ? dev->name : "<unknown>",
					   port_id, "PHY");
			} else if (PPE_DRV_VIRTUAL_PORT_CHK(port_id)) {
				struct net_device *dev = ppe_drv_port_num_to_dev(port_id);
				seq_printf(m, "  %-16s  %-10u  %-6s\n",
					   dev ? dev->name : "<unknown>",
					   port_id, "VP");
			}
		}
	}

	if (!found_any)
		seq_puts(m, "(no active isolation entries)\n");

	spin_unlock_bh(&port_mgmt->lock);
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_isol_dump);

/*
 * ppe_drv_port_mgmt_omci_port_list_show()
 *	Print the OMCI-managed port list to a seq_file (for debugfs).
 */
void ppe_drv_port_mgmt_omci_port_list_show(struct seq_file *m)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt;
	struct ppe_drv_omci_port_node *node;

	if (!p || !p->port_mgmt) {
		seq_puts(m, "PPE driver not initialized\n");
		return;
	}

	port_mgmt = p->port_mgmt;

	seq_puts(m, "OMCI-managed ports:\n");
	seq_puts(m, "-------------------\n");
	seq_printf(m, "%-16s  %-10s\n", "dev_name", "port_id");

	spin_lock_bh(&port_mgmt->lock);
	if (list_empty(&port_mgmt->omci_port_list)) {
		seq_puts(m, "(empty)\n");
	} else {
		list_for_each_entry(node, &port_mgmt->omci_port_list, list) {
			seq_printf(m, "%-16s  %-10u\n", node->dev_name, node->port_id);
		}
	}
	spin_unlock_bh(&port_mgmt->lock);
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_omci_port_list_show);

/*
 * ppe_drv_port_mgmt_omci_port_find_and_ref()
 *	Find a device in the OMCI-managed port list and increment its refcount.
 */
struct ppe_drv_omci_port_node *ppe_drv_port_mgmt_omci_port_find_and_ref(const char *dev_name)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
	struct ppe_drv_omci_port_node *node;

	if (!dev_name || !port_mgmt)
		return NULL;

	spin_lock_bh(&port_mgmt->lock);
	list_for_each_entry(node, &port_mgmt->omci_port_list, list) {
		if (strncmp(node->dev_name, dev_name, IFNAMSIZ) == 0) {
			atomic_inc(&node->refcount);
			spin_unlock_bh(&port_mgmt->lock);
			ppe_drv_info("OMCI port %s referenced\n", node->dev_name);
			return node;
		}
	}
	spin_unlock_bh(&port_mgmt->lock);

	return NULL;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_omci_port_find_and_ref);

/*
 * ppe_drv_port_mgmt_omci_port_deref()
 *	Decrement the refcount of an OMCI port node.
 *	The node is NOT freed here; memory is managed by del/flush.
 */
void ppe_drv_port_mgmt_omci_port_deref(struct ppe_drv_omci_port_node *node)
{
	if (!node)
		return;

	WARN_ON(atomic_read(&node->refcount) == 0);
	atomic_dec(&node->refcount);
	ppe_drv_info("OMCI port %s dereferenced\n", node->dev_name);
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_omci_port_deref);

#ifdef NSS_PPE_DRV_PORT_ISOL_SUPPORT
/*
 * ppe_drv_port_mgmt_set_omci_index()
 * 	Set the omci index in omci field.
 */
void ppe_drv_port_mgmt_set_omci_index(struct ppe_drv_port_mgmt *port_mgmt, uint8_t omci_index, uint8_t dev_id)
{
	port_mgmt->port_omci_index_map[dev_id] = omci_index;
}

/*
 * ppe_drv_port_mgmt_get_exist_vp_profile_id()
 * 	Get the existing VP profile
 */
uint8_t ppe_drv_port_mgmt_get_exist_vp_profile_id(struct ppe_drv_port_mgmt *port_mgmt, uint32_t port_id)
{
	uint8_t profile_id;
	/*
	 * The profile_id is reseved for phy_ports(0-8)
	 */
	if (PPE_DRV_PHY_PORT_CHK(port_id)) {
		port_mgmt->vp_profile_id_info[profile_id].in_use = true;
		port_mgmt->vp_profile_id_info[profile_id].port_id = port_id;
		return port_id;
	}

	/*
	 * Check if we have any existing profile_id for the port_id
	 */
	for (profile_id = PPE_DRV_PHYSICAL_MAX; profile_id < PPE_DRV_VP_PROFILE_ID_MAX; profile_id++) {
		if (port_mgmt->vp_profile_id_info[profile_id].in_use &&
				(port_mgmt->vp_profile_id_info[profile_id].port_id == port_id)) {
			return profile_id;
		}
	}
	return PPE_DRV_VP_PROFILE_ID_MAX;
}

/*
 * ppe_drv_port_mgmt_get_vp_profile_id()
 * 	Get the existing or new profile ID.
 */
uint8_t ppe_drv_port_mgmt_get_vp_profile_id(struct ppe_drv_port_mgmt *port_mgmt, uint32_t port_id)
{
	uint8_t profile_id;

	/*
	 * Check if we have any existing profile_id for the port_id
	 * Else, assign a new profile ID.
	 */
	profile_id = ppe_drv_port_mgmt_get_exist_vp_profile_id(port_mgmt, port_id);
	if (profile_id != PPE_DRV_VP_PROFILE_ID_MAX) {
		return profile_id;
	}

	for (profile_id = PPE_DRV_PHYSICAL_MAX; profile_id < PPE_DRV_VP_PROFILE_ID_MAX; profile_id++) {
		if (!port_mgmt->vp_profile_id_info[profile_id].in_use) {
			port_mgmt->vp_profile_id_info[profile_id].in_use = true;
			port_mgmt->vp_profile_id_info[profile_id].port_id = port_id;
			return profile_id;
		}
	}
	return PPE_DRV_VP_PROFILE_ID_MAX;
}

/*
 * ppe_drv_port_mgmt_clear_vp_profile_id()
 * 	Reset the vp profile if not in use.
 */
void ppe_drv_port_mgmt_clear_vp_profile_id(struct ppe_drv_port_mgmt *port_mgmt, uint32_t port_id)
{
	uint8_t profile_id;
	profile_id = ppe_drv_port_mgmt_get_exist_vp_profile_id(port_mgmt, port_id);
	if (profile_id != PPE_DRV_VP_PROFILE_ID_MAX) {
		port_mgmt->vp_profile_id_info[profile_id].in_use = false;
		port_mgmt->vp_profile_id_info[profile_id].port_id = PPE_DRV_PORTS_MAX;
		port_mgmt->vp_profile_id_info[profile_id].profile_id = PPE_DRV_VP_PROFILE_ID_MAX;
	}
}

/*
 * ppe_drv_get_omci_index()
 * 	Get the Omci index corresponding to OMCI ID.
 */
uint16_t ppe_drv_get_omci_index(struct ppe_drv_port_mgmt *port_mgmt, uint32_t omci_id)
{
	uint16_t omci_index;

	/*
	 * Check if we have any existing omci_index for the omci_id
	 * Else, Assign a new OMCI index.
	 */
	for (omci_index = 0; omci_index < PPE_DRV_OMCI_INDEX_MAX; omci_index++) {
		if (port_mgmt->omci_index_info[omci_index].in_use &&
				(port_mgmt->omci_index_info[omci_index].omci_id == omci_id)) {
			port_mgmt->omci_index_info[omci_index].port_cnt += 1;
			return omci_index;
		}
	}

	for (omci_index = 0; omci_index < PPE_DRV_OMCI_INDEX_MAX; omci_index++) {
		if (!port_mgmt->omci_index_info[omci_index].in_use) {
			port_mgmt->omci_index_info[omci_index].in_use = true;
			port_mgmt->omci_index_info[omci_index].omci_id = omci_id;
			port_mgmt->omci_index_info[omci_index].port_cnt = 1;
			return omci_index;
		}
	}
	return PPE_DRV_OMCI_INDEX_MAX;
}

/*
 * ppe_drv_clear_omci_index()
 * 	Reset the flag if OMCI index.
 */
void ppe_drv_clear_omci_index(struct ppe_drv_port_mgmt *port_mgmt, uint16_t omci_index)
{
	if (port_mgmt->omci_index_info[omci_index].in_use) {
		port_mgmt->omci_index_info[omci_index].in_use = false;
		port_mgmt->omci_index_info[omci_index].omci_id = PPE_DRV_OMCI_INDEX_MAX;
		port_mgmt->omci_index_info[omci_index].port_cnt = 0;
	}
}

/*
 * ppe_drv_deref_omci_index()
 * 	Deref the OMCI index if not in use.
 */
void ppe_drv_deref_omci_index(struct ppe_drv_port_mgmt *port_mgmt, uint16_t omci_index)
{
	if (omci_index != PPE_DRV_OMCI_INDEX_MAX) {
		port_mgmt->omci_index_info[omci_index].port_cnt--;
		if(!port_mgmt->omci_index_info[omci_index].port_cnt)
			ppe_drv_clear_omci_index(port_mgmt, omci_index);
	}
}

/*
 * ppe_drv_port_isol_ctrl_set()
 * 	configure control actions for the corresponding profile ID.
 */
int ppe_drv_port_isol_ctrl_set(struct ppe_drv_port_mgmt *port_mgmt,
		uint32_t src_port_id,
		uint32_t dst_port_id,
		bool enable)
{
	sw_error_t err;
	fal_port_isol_ctrl_t isol_ctrl = {0};

	uint8_t src_isol_type = PPE_DRV_PHY_PORT_CHK(src_port_id) ? PPE_DRV_PORT_PHYSICAL : PPE_DRV_PORT_VIRTUAL;
	uint8_t dst_isol_type = PPE_DRV_PHY_PORT_CHK(dst_port_id) ? PPE_DRV_PORT_PHYSICAL : PPE_DRV_PORT_VIRTUAL;
	uint8_t src_profile_id = ppe_drv_port_mgmt_get_vp_profile_id(port_mgmt, src_port_id);
	uint8_t dst_profile_id = ppe_drv_port_mgmt_get_vp_profile_id(port_mgmt, dst_port_id);

	if (src_profile_id == PPE_DRV_VP_PROFILE_ID_MAX ||
			dst_profile_id == PPE_DRV_VP_PROFILE_ID_MAX)
		return PPE_DRV_RET_FAILURE_INVALID_PARAM;

	/*
	 * Set isol control in PRE Routing table
	 */
	isol_ctrl.dir = FAL_DIR_INGRESS;
	isol_ctrl.enable = enable;
	isol_ctrl.isol_group_id = src_profile_id;
	err = fal_port_isol_ctrl_set(PPE_DRV_SWITCH_ID, src_isol_type, &isol_ctrl);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to set port isolation control, err %d\n", err);
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
	}

	/*
	 * Set isol control in Post Routing table
	 */
	isol_ctrl.dir = FAL_DIR_EGRESS;
	isol_ctrl.enable = enable;
	isol_ctrl.isol_group_id = dst_profile_id;
	err = fal_port_isol_ctrl_set(PPE_DRV_SWITCH_ID, dst_isol_type, &isol_ctrl);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to set port isolation control, err %d\n", err);
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
	}

	return err;
}

/*
 * ppe_drv_port_isol_action_set()
 *    Set port isolation action bitmap.
 */
ppe_drv_ret_t ppe_drv_port_isol_action_set(struct ppe_drv_port_mgmt *port_mgmt, int dev_id,
		uint32_t *action_tab,
		enum ppe_drv_port_type isol_type)
{
	sw_error_t err;
	fal_port_isol_act_idx_t isol_id = {0};
	fal_port_isol_act_t isol_act = {0};
	uint8_t vp_profile_id;

	switch (isol_type) {
		case PPE_DRV_PORT_VIRTUAL:
			vp_profile_id = ppe_drv_port_mgmt_get_vp_profile_id(port_mgmt, dev_id);
			if (vp_profile_id == PPE_DRV_VP_PROFILE_ID_MAX)
				return PPE_DRV_RET_FAILURE_INVALID_PARAM;

			isol_id.isol_group_id = vp_profile_id;
			isol_id.isol_type = FAL_ISOL_ACT_GROUP;
			memcpy(isol_act.act_bitmap, action_tab, sizeof(isol_act.act_bitmap));

			err = fal_port_isol_action_set(PPE_DRV_SWITCH_ID, &isol_id, &isol_act);
			if (err != SW_OK) {
				ppe_drv_warn("Failed to set port isolation action for port_id %d, err %d\n", dev_id, err);
				return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
			}
			break;

		case PPE_DRV_PORT_PHYSICAL:
			isol_id.isol_type = FAL_ISOL_ACT_PPORT;
			isol_id.pport_id = dev_id;
			memcpy(isol_act.act_bitmap, action_tab, sizeof(isol_act.act_bitmap));

			err = fal_port_isol_action_set(PPE_DRV_SWITCH_ID, &isol_id, &isol_act);
			if (err != SW_OK) {
				ppe_drv_warn("Failed to set port isolation action for port_id %d, err %d\n", dev_id, err);
				return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
			}
			break;

		default:
			ppe_drv_warn("%s: invalid isol_type %d\n", __func__, isol_type);
			return PPE_DRV_RET_FAILURE_INVALID_PARAM;
	}

	ppe_drv_info("Set port isolation: isol_act[3]:%d isol_act[2]:%d isol_act[1]:%d isol_act[0]:%d\n", isol_act.act_bitmap[3], isol_act.act_bitmap[2], isol_act.act_bitmap[1], isol_act.act_bitmap[0]);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_port_isol_bitmap_set()
 *    Set port isolation membership bitmap.
 */
ppe_drv_ret_t ppe_drv_port_isol_bitmap_set(int dev_id,
		uint64_t *bitmap_tab)
{
	sw_error_t err;
	err = fal_port_isol_member_set(PPE_DRV_SWITCH_ID, dev_id, *bitmap_tab);
	if (err != SW_OK) {
		ppe_drv_warn("Issue while pushing isolation bitmap\n");
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
	}

	ppe_drv_info("Bitmap set, dev_id: %d isol_bitmap: %lld\n", dev_id, *bitmap_tab);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_port_mgmt_isol_fal_set()
 * 	Apply isolation bitmap and action for port.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_isol_fal_set(struct ppe_drv_port_mgmt *port_mgmt, int dev_id,
		uint64_t *bitmap_tab,
		uint32_t *action_tab,
		enum ppe_drv_port_type isol_type)
{
	ppe_drv_ret_t ret;
	ret = ppe_drv_port_isol_bitmap_set(dev_id, bitmap_tab);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("Failed to set isolation bitmap for %s port %d, err: %d\n",
				(isol_type == PPE_DRV_PORT_VIRTUAL) ? "VP" : "PHY", dev_id, ret);
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
	}

	ret = ppe_drv_port_isol_action_set(port_mgmt, dev_id, action_tab, isol_type);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("Failed to set isolation action for %s port %d, err: %d\n",
				(isol_type == PPE_DRV_PORT_VIRTUAL) ? "VP" : "PHY", dev_id, ret);
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
	}

	return PPE_DRV_RET_SUCCESS;
}

/**
 * ppe_drv_port_mgmt_isol_bit_set()
 * 	Update isolation bitmap and action for a given OMCI ID.
 */
static inline void ppe_drv_port_mgmt_isol_bit_set(struct ppe_drv_port_mgmt *port_mgmt, bool is_phy,
		int omci_index, uint16_t idx, bool fwd)
{
	uint8_t bit_pos = idx;
	size_t byte_idx = bit_pos / 16;
	uint8_t shift_b  = (bit_pos % 16) * 2;

	if (byte_idx >= PPE_DRV_PORT_MGMT_ACT_ARR_SIZE)
		return;

	if (fwd) {
		if (is_phy) {
			/*
			 * Set membership within the OMCI group:
			 * Clear flood bit, and
			 * Clear 2-bit field to allow subgroup forwarding
			 */
			port_mgmt->port_phy_isol_bitmap[omci_index] |= (0x1ULL << bit_pos);
			port_mgmt->port_phy_isol_action[omci_index][byte_idx] &= ~(0x3ULL << shift_b);
		} else {
			/*
			 * Set membership within the OMCI group:
			 * Clear flood bit to restrict forwarding to isol
			 * Set the VP profile ID in isol_action.
			 */
			port_mgmt->port_vp_isol_bitmap[omci_index] |= port_mgmt->port_phy_isol_bitmap[omci_index];
			port_mgmt->port_vp_isol_action[omci_index][byte_idx] &= ~(0x3ULL << shift_b);
		}
	} else {
		if (is_phy) {
			/*
			 * Clear membership within the OMCI group:
			 * Set flood bit, and
			 * Set 2-bit field to restrict forwarding
			 */
			port_mgmt->port_phy_isol_bitmap[omci_index] &= ~(0x1ULL << bit_pos);
			port_mgmt->port_phy_isol_action[omci_index][byte_idx] &= ~(0x3ULL << shift_b);
			port_mgmt->port_phy_isol_action[omci_index][byte_idx] |=  (0x2ULL << shift_b);
		} else {
			/*
			 * Clear membership within the OMCI group:
			 * Set flood bit, and
			 * Set 2-bit field to restrict forwarding.
			 */
			port_mgmt->port_vp_isol_bitmap[omci_index] |= port_mgmt->port_phy_isol_bitmap[omci_index];
			port_mgmt->port_vp_isol_action[omci_index][byte_idx] &= ~(0x3ULL << shift_b);
			port_mgmt->port_vp_isol_action[omci_index][byte_idx] |=  (0x2ULL << shift_b);
		}
	}

	/*
	 * Always allow forwarding to CPU port 0
	 */
	port_mgmt->port_phy_isol_action[omci_index][0] &= ~(0x3ULL);
	port_mgmt->port_vp_isol_action[omci_index][0]  &= ~(0x3ULL);
	port_mgmt->port_phy_isol_bitmap[omci_index] |= 0x1ULL;
	port_mgmt->port_vp_isol_bitmap[omci_index]  |= 0x1ULL;

#ifdef NSS_PPE_PON_SUPPORT
	/*
	 * Always enable Forwarding to upstream port
	 */
	if (gem_port_bitmap && (port_mgmt->us_port_id < PPE_DRV_PHYSICAL_MAX)) {
		bit_pos = port_mgmt->us_port_id;
		byte_idx = bit_pos / 16;
		shift_b = (bit_pos % 16) * 2;
		port_mgmt->port_phy_isol_action[omci_index][byte_idx] &= ~(0x3ULL << shift_b);
		port_mgmt->port_vp_isol_action[omci_index][byte_idx] &= ~(0x3ULL << shift_b);
	}
#endif
}

/**
 * ppe_drv_port_mgmt_isol_apply_all()
 * 	Apply isolation bitmap and action for all ports with same omci index.
 */
static inline ppe_drv_ret_t ppe_drv_port_mgmt_isol_apply_all(struct ppe_drv_port_mgmt *port_mgmt,
		uint8_t omci_idx)
{
	ppe_drv_ret_t ret;
	int phy_port, vp_port;
#ifdef NSS_PPE_PON_SUPPORT
	uint64_t us_port_bitmap = (uint64_t)gem_port_bitmap;
#endif
	/*
	 * PHY table
	 *
	 * CASE 1: PHY<=>PHY mapping
	 * 	Flooding allowed to PHY ports and US ports.
	 */
	for (phy_port = PPE_DRV_PHYSICAL_START; phy_port < PPE_DRV_PHYSICAL_MAX; phy_port++) {
		/*
		 * Modify the bitmap and action fields for OMCI index.
		 */
		if (omci_idx == port_mgmt->port_omci_index_map[phy_port]) {
			ret = ppe_drv_port_mgmt_isol_fal_set(port_mgmt, phy_port,
					&port_mgmt->port_phy_isol_bitmap[omci_idx],
					port_mgmt->port_phy_isol_action[omci_idx],
					PPE_DRV_PORT_PHYSICAL);
			if (ret != PPE_DRV_RET_SUCCESS)
				return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
		}
	}

	/*
	 * VP table
	 *
	 * CASE 2: VP<=>US(PHY) mapping
	 * 	Flooding allowed to US port only.
	 *
	 * CASE 3: PHY<=>VP mapping
	 * 	Flooding allowed to phy ports and US port.
	 */
#ifdef NSS_PPE_PON_SUPPORT
	if (gem_port_bitmap) {
		for (vp_port = PPE_DRV_VIRTUAL_START; vp_port < PPE_DRV_PORTS_MAX; vp_port++) {
			if (omci_idx == port_mgmt->port_omci_index_map[vp_port]) {
				struct net_device *dev = ppe_drv_port_num_to_dev(vp_port);
				if (!dev) {
					port_mgmt->port_omci_index_map[vp_port] = PPE_DRV_OMCI_INDEX_MAX;
					continue;
				}

				ret = ppe_drv_port_mgmt_isol_fal_set(port_mgmt, vp_port,
						&us_port_bitmap, port_mgmt->port_vp_isol_action[omci_idx],
						PPE_DRV_PORT_VIRTUAL);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

				ret = ppe_drv_port_isol_ctrl_set(port_mgmt, vp_port,
						port_mgmt->us_port_id, A_TRUE);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

				ret = ppe_drv_port_isol_ctrl_set(port_mgmt, port_mgmt->us_port_id,
						vp_port, A_TRUE);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
			}
		}
	}
#endif
	for (vp_port = PPE_DRV_VIRTUAL_START; vp_port < PPE_DRV_PORTS_MAX; vp_port++) {
		for (phy_port = PPE_DRV_PHYSICAL_START; phy_port < PPE_DRV_PHYSICAL_MAX; phy_port++) {
			if (omci_idx == port_mgmt->port_omci_index_map[phy_port]) {
				struct net_device *dev = ppe_drv_port_num_to_dev(vp_port);
				if (!dev)
				{
					port_mgmt->port_omci_index_map[vp_port] = PPE_DRV_OMCI_INDEX_MAX;
					continue;
				}

				ret = ppe_drv_port_mgmt_isol_fal_set(port_mgmt, vp_port,
						&port_mgmt->port_vp_isol_bitmap[omci_idx],
						port_mgmt->port_vp_isol_action[omci_idx],
						PPE_DRV_PORT_VIRTUAL);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

				ret = ppe_drv_port_isol_ctrl_set(port_mgmt, phy_port,
						vp_port, A_TRUE);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

				ret = ppe_drv_port_isol_ctrl_set(port_mgmt, vp_port,
						phy_port, A_TRUE);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
			}
		}
	}
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_port_mgmt_isol_configure()
 * 	Configure isolation for a given port and update context.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_isol_configure(struct ppe_drv_port_mgmt_isol *isol_info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
	uint8_t cur_omci_idx;
	uint8_t new_omci_idx;
	uint8_t vp_profile_id;
	uint8_t idx;
	uint16_t dev_id;
	uint32_t omci_id;
	bool is_phy, is_vp;
	ppe_drv_ret_t ret;

	if (!isol_info)
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

	dev_id = isol_info->port_id;
	omci_id = isol_info->omci_id;
	is_phy = PPE_DRV_PHY_PORT_CHK(dev_id);
	is_vp  = PPE_DRV_VIRTUAL_PORT_CHK(dev_id);

	if (!is_phy && !is_vp)
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

	cur_omci_idx = port_mgmt->port_omci_index_map[dev_id];
	new_omci_idx = ppe_drv_get_omci_index(port_mgmt, omci_id);
	if (new_omci_idx == PPE_DRV_OMCI_INDEX_MAX)
		return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;

	ppe_drv_port_mgmt_set_omci_index(port_mgmt, new_omci_idx, dev_id);

	if (is_vp) {
		vp_profile_id = ppe_drv_port_mgmt_get_vp_profile_id(port_mgmt, dev_id);
		if (vp_profile_id == PPE_DRV_VP_PROFILE_ID_MAX)
			return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
	}

	idx = is_phy ? dev_id : vp_profile_id;

	/*
	 * Clear bit for old isol map
	 */
	if (cur_omci_idx != PPE_DRV_OMCI_INDEX_MAX)
		ppe_drv_port_mgmt_isol_bit_set(port_mgmt, is_phy, cur_omci_idx, idx, false);
	/*
	 * Set bit for flooding
	 */
	ppe_drv_port_mgmt_isol_bit_set(port_mgmt, is_phy, new_omci_idx, idx, true);

	/*
	 * Update OMCI Index map
	 */
	if (cur_omci_idx != PPE_DRV_OMCI_INDEX_MAX)
		ret = ppe_drv_port_mgmt_isol_apply_all(port_mgmt, cur_omci_idx);
	ret = ppe_drv_port_mgmt_isol_apply_all(port_mgmt, new_omci_idx);
	ppe_drv_deref_omci_index(port_mgmt, cur_omci_idx);

	return ret;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_isol_configure);

/*
 * ppe_drv_port_mgmt_act_ctrl_fill()
 *     Set multicast/broadcast isolation action control.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_act_ctrl_fill(struct ppe_drv_port_mgmt *port_mgmt)
{
	sw_error_t err;
	fal_port_isol_act_ctrl_t fal_act_ctrl = {0};

	fal_act_ctrl.bc_isol_en = port_mgmt->bc_isol_en;
	fal_act_ctrl.mc_isol_en = port_mgmt->mc_isol_en;

	err = fal_port_isol_action_ctrl_set(PPE_DRV_SWITCH_ID, &fal_act_ctrl);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to set port isolation action control, err %d\n", err);
		return PPE_DRV_RET_PORT_MGMT_ACT_CONFIG_FAIL;
	}

	ppe_drv_info("Set port isolation action control: mc_isol_en=%d, bc_isol_en=%d\n", fal_act_ctrl.mc_isol_en, fal_act_ctrl.bc_isol_en);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_port_mgmt_act_ctrl_set()
 * 	Sets the action control fields.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_act_ctrl_set(struct ppe_drv_port_mgmt_isol *isol_info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;

	port_mgmt->mc_isol_en = isol_info->mc_isol_en;
	port_mgmt->bc_isol_en = isol_info->bc_isol_en;
	ppe_drv_port_mgmt_act_ctrl_fill(port_mgmt);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_act_ctrl_set);

/*
 * ppe_drv_port_mgmt_default_isol_set()
 * 	Set the default isolation configuration.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_default_isol_set(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
	uint32_t action[PPE_DRV_PORT_MGMT_ACT_ARR_SIZE];
	uint64_t isol;
	int phy_port, vp_port;
	ppe_drv_ret_t ret;

	for (phy_port = PPE_DRV_PHYSICAL_START; phy_port < PPE_DRV_PHYSICAL_MAX; phy_port++) {
		if (port_mgmt->port_omci_index_map[phy_port] != PPE_DRV_OMCI_INDEX_MAX) {
			memset(action, 0x0, sizeof(action));
			memset(&isol, 0xFF, sizeof(isol));
			ret = ppe_drv_port_mgmt_isol_fal_set(port_mgmt, phy_port,
					&isol, action, PPE_DRV_PORT_PHYSICAL);
			if (ret != PPE_DRV_RET_SUCCESS)
				return PPE_DRV_RET_PORT_MGMT_ISOL_DEFAULT_FAIL;
		}
	}

	for (vp_port = PPE_DRV_VIRTUAL_START; vp_port < PPE_DRV_PORTS_MAX; vp_port++) {
		if (port_mgmt->port_omci_index_map[vp_port] != PPE_DRV_OMCI_INDEX_MAX) {
			struct net_device *dev = ppe_drv_port_num_to_dev(vp_port);
			if (!dev)
				continue;

			for (phy_port = PPE_DRV_PHYSICAL_START; phy_port < PPE_DRV_PHYSICAL_MAX; phy_port++) {
				ret = ppe_drv_port_isol_ctrl_set(port_mgmt, phy_port,
						vp_port, A_FALSE);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_DEFAULT_FAIL;

				ret = ppe_drv_port_isol_ctrl_set(port_mgmt, vp_port,
						phy_port, A_FALSE);
				if (ret != PPE_DRV_RET_SUCCESS)
					return PPE_DRV_RET_PORT_MGMT_ISOL_DEFAULT_FAIL;
			}
		}
	}

	ppe_drv_port_mgmt_entries_init(port_mgmt);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_default_isol_set);
#else
ppe_drv_ret_t ppe_drv_port_mgmt_isol_configure(struct ppe_drv_port_mgmt_isol *isol_info)
{
	return PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_isol_configure);

ppe_drv_ret_t ppe_drv_port_mgmt_act_ctrl_set(struct ppe_drv_port_mgmt_isol *isol_info)
{
	return PPE_DRV_RET_PORT_MGMT_ACT_CONFIG_FAIL;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_act_ctrl_set);

ppe_drv_ret_t ppe_drv_port_mgmt_default_isol_set(void)
{
	return PPE_DRV_RET_PORT_MGMT_ISOL_DEFAULT_FAIL;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_default_isol_set);
#endif

/*
 * ppe_drv_port_mgmt_mac_lrn_limit_set()
 *	Set the MAC learning limit configuration.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_mac_lrn_limit_set(struct ppe_drv_port_mac_lrn_limit *mac_lrn_limit)
{
	sw_error_t err;
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	err = fal_port_fdb_learn_limit_set(PPE_DRV_SWITCH_ID, mac_lrn_limit->port_id,
			mac_lrn_limit->port_learn_limit_en, mac_lrn_limit->port_learn_limit);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to set port mac leran limit, err %d\n", err);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_PORT_MGMT_MAC_LRN_LMT_CONFIG_FAIL;
	}

	if (mac_lrn_limit->lrn_exceed_action_en) {
		fal_port_fdb_learn_exceed_cmd_set(PPE_DRV_SWITCH_ID, mac_lrn_limit->port_id,
				(fal_fwd_cmd_t)mac_lrn_limit->lrn_exceed_action);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to set action command when mac learn limit exceeds, err %d\n", err);
			spin_unlock_bh(&p->lock);
			return PPE_DRV_RET_PORT_MGMT_MAC_LRN_LMT_CONFIG_FAIL;
		}
	}

	/*
	 * Cache the applied configuration so the debugfs dump can read it
	 * back without querying the hardware.
	 */
	if (mac_lrn_limit->port_id < PPE_DRV_PORTS_MAX) {
		struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
		if (port_mgmt) {
			port_mgmt->mac_lrn_limit_cache[mac_lrn_limit->port_id] = *mac_lrn_limit;
			port_mgmt->mac_lrn_limit_valid[mac_lrn_limit->port_id] = true;
		}
	}

	ppe_drv_info("port mac learn limit is enabled:%d for port_id: %d and limit: %d\n", mac_lrn_limit->port_learn_limit_en, mac_lrn_limit->port_id, mac_lrn_limit->port_learn_limit);
	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_mac_lrn_limit_set);


/*
 * ppe_drv_port_mgmt_fid_get()
 *	Gets the FID associated with the MAC filter.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_fid_get(struct net_device *dev, uint8_t* fid_index)
{
	struct ppe_drv_iface *iface;
	struct ppe_drv_vsi *vsi;

	/*
	 * Resolve iface by FID name when available, else via upstream GEM bitmap.
	 */
	if (dev) {
		iface = ppe_drv_iface_get_by_dev(dev);
		if (!iface) {
			ppe_drv_warn("Failed to resolve PPE iface for dev '%s'\n", dev->name);
			return PPE_DRV_RET_PORT_MGMT_FID_GET_FAIL;
		}

	} else {
#ifdef NSS_PPE_PON_SUPPORT
		/*
		 * No explicit device given; derive the iface from the upstream
		 * GEM port bitmap.
		 */
		if (!gem_port_bitmap) {
			ppe_drv_warn("No upstream port (gem_port_bitmap=0) and no FID name\n");
			return PPE_DRV_RET_PORT_MGMT_FID_GET_FAIL;
		}

		int port_idx = __builtin_ffs(gem_port_bitmap) - 1;
		iface = ppe_drv_iface_get_by_idx(port_idx);
		if (!iface) {
			ppe_drv_warn("Failed to resolve PPE iface for upstream port idx=%d\n", port_idx);
			return PPE_DRV_RET_PORT_MGMT_FID_GET_FAIL;
		}
#else
		ppe_drv_warn("No upstream port (gem_port_bitmap=0) and no FID name\n");
		return PPE_DRV_RET_PORT_MGMT_FID_GET_FAIL;
#endif
	}

	/*
	 * Retrieve the VSI associated with the resolved iface.
	 */
	vsi = ppe_drv_iface_vsi_get(iface);
	if (!vsi) {
		ppe_drv_warn("Invalid VSI for given iface\n");
		return PPE_DRV_RET_PORT_MGMT_FID_GET_FAIL;
	}

	*fid_index = vsi->index;
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_fid_get);

/*
 * ppe_drv_port_mgmt_mac_filter_clear()
 *	Clears the MAC filter configuration.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_mac_filter_clear(struct ppe_drv_port_mac_filter *mac_filter)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_fdb_entry_t entry;
	sw_error_t err;
	ppe_drv_ret_t ret = PPE_DRV_RET_SUCCESS;

	if (!mac_filter) {
		ppe_drv_warn("Invalid arguments: mac_filter=%p\n", mac_filter);
		return PPE_DRV_RET_PORT_MGMT_MAC_FILTER_CLR_FAIL;
	}

	memset(&entry, 0, sizeof(entry));
	memcpy(&entry.addr, mac_filter->mac, ETH_ALEN);
	entry.fid = mac_filter->fid_index;

	spin_lock_bh(&p->lock);

	/*
	 * Look up the existing FDB entry; only clear it if it is currently
	 * in the DROP state installed by mac_filter_set().
	 */
	err = fal_fdb_entry_search(PPE_DRV_SWITCH_ID, &entry);
	if (err != SW_OK) {
		ppe_drv_warn("FDB entry not found for MAC %pM (fid=%u), err=%d\n",
				mac_filter->mac, entry.fid, err);
		ret = PPE_DRV_RET_PORT_MGMT_MAC_FILTER_CLR_FAIL;
		goto out_unlock;
	}

	if (entry.dacmd == FAL_MAC_DROP && entry.sacmd == FAL_MAC_DROP) {
		entry.dacmd = FAL_MAC_RDT_TO_CPU;
		entry.sacmd = FAL_MAC_RDT_TO_CPU;

		err = fal_fdb_entry_add(PPE_DRV_SWITCH_ID, &entry);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to program RDT-to-CPU for MAC %pM (fid=%u), err=%d\n",
					mac_filter->mac, entry.fid, err);
			ret = PPE_DRV_RET_PORT_MGMT_MAC_FILTER_CLR_FAIL;
			goto out_unlock;
		}

		ppe_drv_info("RDT-to-CPU applied for MAC %pM (fid=%u)\n",
				mac_filter->mac, entry.fid);
	} else {
		/*
		 * Entry exists but is not in DROP state; treat as a no-op.
		 */
		ppe_drv_info("No-op: FDB for MAC %pM (fid=%u) not in DROP state (dacmd=%u sacmd=%u)\n",
				mac_filter->mac, entry.fid, entry.dacmd, entry.sacmd);
	}

out_unlock:
	spin_unlock_bh(&p->lock);

	if (ret == PPE_DRV_RET_SUCCESS) {
		/*
		 * Remove the corresponding node from the MAC filter cache.
		 */
		struct ppe_drv_port_mgmt *port_mgmt = p->port_mgmt;
		if (port_mgmt) {
			struct ppe_drv_mac_filter_node *node, *tmp;
			spin_lock_bh(&port_mgmt->lock);
			list_for_each_entry_safe(node, tmp, &port_mgmt->mac_filter_cache, list) {
				if (!memcmp(node->mac, mac_filter->mac, ETH_ALEN) &&
				    node->fid_index == mac_filter->fid_index) {
					list_del(&node->list);
					kfree(node);
					break;
				}
			}
			spin_unlock_bh(&port_mgmt->lock);
		}
	}

	return ret;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_mac_filter_clear);

/*
 * ppe_drv_port_mgmt_mac_lrn_limit_dump()
 *	Print the cached MAC learn-limit configuration for every port that has
 *	had ppe_drv_port_mgmt_mac_lrn_limit_set() called on it.  The cache is
 *	updated on every successful FAL call so it always reflects the last
 *	applied configuration.
 *
 *	Output columns:
 *	  port_id         - PPE port number
 *	  enabled         - whether the learn limit is active (yes/no)
 *	  learn_limit     - maximum number of MACs the port may learn
 *	  exceed_action_en- whether an exceed action is configured (yes/no)
 *	  exceed_action   - action taken when the limit is exceeded
 */
void ppe_drv_port_mgmt_mac_lrn_limit_dump(struct seq_file *m)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt;
	struct ppe_drv_port_mac_lrn_limit *cfg;
	uint32_t port_id;
	bool found_any = false;

	/*
	 * String table for the exceed-action enum; indexed by
	 * ppe_drv_port_mgmt_fwd_cmd_t.
	 */
	static const char * const action_str[] = {
		[PPE_DRV_PORT_MGMT_FWD_CMD_FWD]   = "forward",
		[PPE_DRV_PORT_MGMT_FWD_CMD_DROP]  = "drop",
		[PPE_DRV_PORT_MGMT_FWD_CMD_COPY]  = "copy-to-cpu",
		[PPE_DRV_PORT_MGMT_FWD_CMD_REDIR] = "redir-to-cpu",
	};

	if (!p || !p->port_mgmt) {
		seq_puts(m, "  PPE driver not initialized\n");
		return;
	}

	port_mgmt = p->port_mgmt;

	seq_printf(m, "  %-10s  %-8s  %-12s  %-18s  %-14s\n",
		   "port_id", "enabled", "learn_limit", "exceed_action_en", "exceed_action");
	seq_puts(m, "  -----------------------------------------------------------------------\n");

	spin_lock_bh(&port_mgmt->lock);
	for (port_id = 0; port_id < PPE_DRV_PORTS_MAX; port_id++) {
		if (!port_mgmt->mac_lrn_limit_valid[port_id])
			continue;

		found_any = true;
		cfg = &port_mgmt->mac_lrn_limit_cache[port_id];

		seq_printf(m, "  %-10u  %-8s  %-12u  %-18s  %-14s\n",
			   port_id,
			   cfg->port_learn_limit_en ? "yes" : "no",
			   cfg->port_learn_limit,
			   cfg->lrn_exceed_action_en ? "yes" : "no",
			   cfg->lrn_exceed_action_en ?
				action_str[cfg->lrn_exceed_action] : "n/a");
	}
	spin_unlock_bh(&port_mgmt->lock);

	if (!found_any)
		seq_puts(m, "  (no MAC learn-limit entries configured)\n");
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_mac_lrn_limit_dump);

/*
 * ppe_drv_port_mgmt_mac_filter_dump()
 *	Print all MAC filter entries from the cache maintained by
 *	ppe_drv_port_mgmt_mac_filter_set() / ppe_drv_port_mgmt_mac_filter_clear().
 *	No FAL or hardware query is performed.
 *
 *	Output columns:
 *	  mac_addr  - filtered MAC address
 *	  fid_valid - whether a FID is associated
 *	  fid       - forwarding/filter domain ID
 */
void ppe_drv_port_mgmt_mac_filter_dump(struct seq_file *m)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt;
	struct ppe_drv_mac_filter_node *node;
	bool found_any = false;

	if (!p || !p->port_mgmt) {
		seq_puts(m, "  PPE driver not initialized\n");
		return;
	}

	port_mgmt = p->port_mgmt;

	seq_printf(m, "  %-20s  %-6s\n", "mac_addr", "fid");
	seq_puts(m, "  ----------------------------\n");

	spin_lock_bh(&port_mgmt->lock);
	list_for_each_entry(node, &port_mgmt->mac_filter_cache, list) {
		found_any = true;
		seq_printf(m, "  %pM  %-6u\n",
			   node->mac,
			   node->fid_index);
	}
	spin_unlock_bh(&port_mgmt->lock);

	if (!found_any)
		seq_puts(m, "  (no MAC filter entries)\n");
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_mac_filter_dump);

/*
 * ppe_drv_port_mgmt_mac_filter_set()
 *	Sets the MAC filter configuration and caches the entry.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_mac_filter_set(struct ppe_drv_port_mac_filter *mac_filter)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port_mgmt *port_mgmt;
	struct ppe_drv_mac_filter_node *node;
	fal_fdb_entry_t entry;
	sw_error_t err;

	if (!mac_filter) {
		ppe_drv_warn("Invalid arguments: p=%p mac_filter=%p\n", p, mac_filter);
		return PPE_DRV_RET_PORT_MGMT_MAC_FILTER_SET_FAIL;
	}

	memset(&entry, 0, sizeof(entry));
	memcpy(&entry.addr, mac_filter->mac, ETH_ALEN);
	entry.fid = mac_filter->fid_index;

	spin_lock_bh(&p->lock);

	/*
	 * Search for an existing FDB entry so we can update it in-place
	 * rather than creating a duplicate.
	 */
	fal_fdb_entry_search(PPE_DRV_SWITCH_ID, &entry);

	/*
	 * Set both DA and SA commands to DROP and write the entry back.
	 */
	entry.dacmd = FAL_MAC_DROP;
	entry.sacmd = FAL_MAC_DROP;
	err = fal_fdb_entry_add(PPE_DRV_SWITCH_ID, &entry);

	spin_unlock_bh(&p->lock);

	if (err != SW_OK) {
		ppe_drv_warn("Failed to add/update drop FDB for MAC %pM (fid=%u), err=%d\n",
				mac_filter->mac, entry.fid, err);
		return PPE_DRV_RET_PORT_MGMT_MAC_FILTER_SET_FAIL;
	}

	/*
	 * Cache the entry so the debugfs dump can read it back without
	 * querying the FDB via FAL.  If an entry for the same MAC+fid
	 * already exists in the cache, reuse it; otherwise allocate a new node.
	 */
	port_mgmt = p->port_mgmt;
	if (port_mgmt) {
		spin_lock_bh(&port_mgmt->lock);
		list_for_each_entry(node, &port_mgmt->mac_filter_cache, list) {
			if (!memcmp(node->mac, mac_filter->mac, ETH_ALEN) &&
			    node->fid_index == mac_filter->fid_index) {
				spin_unlock_bh(&port_mgmt->lock);
				goto done;
			}
		}
		spin_unlock_bh(&port_mgmt->lock);

		node = kzalloc(sizeof(*node), GFP_ATOMIC);
		if (node) {
			memcpy(node->mac, mac_filter->mac, ETH_ALEN);
			node->fid_index = mac_filter->fid_index;
			spin_lock_bh(&port_mgmt->lock);
			list_add_tail(&node->list, &port_mgmt->mac_filter_cache);
			spin_unlock_bh(&port_mgmt->lock);
		} else {
			ppe_drv_warn("Failed to allocate MAC filter cache node for %pM\n",
				     mac_filter->mac);
		}
	}

done:
	ppe_drv_info("Drop FDB entry applied for MAC %pM (fid=%u)\n",
			mac_filter->mac, entry.fid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_port_mgmt_mac_filter_set);
