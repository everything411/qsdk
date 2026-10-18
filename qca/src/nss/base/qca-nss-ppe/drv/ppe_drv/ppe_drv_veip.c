/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/etherdevice.h>
#include <fal/fal_ip.h>
#include <fal/fal_qm.h>
#include <fal/fal_api.h>
#include <fal/fal_vsi.h>
#include <fal/fal_portvlan.h>
#include <fal/fal_port_ctrl.h>
#include <fal/fal_vport.h>
#include <fal/fal_servcode.h>
#include "ppe_drv.h"

#define MAX_VEIP_PORTS 2

/*
 * ppe_drv_veip_alloc()
 *	Allocate memory for VEIP context structure.
 */
static struct ppe_drv_veip_ctx *ppe_drv_veip_alloc(void)
{
	return kzalloc(sizeof(struct ppe_drv_veip_ctx), GFP_ATOMIC);
}

/*
 * ppe_drv_veip_free()
 *	Free memory for VEIP context structure.
 */
static void ppe_drv_veip_free(struct kref *kref)
{
	struct ppe_drv_veip_ctx *veip= container_of(kref, struct ppe_drv_veip_ctx, ref);
	list_del(&veip->list);
	kfree(veip);
}

/*
 * ppe_drv_veip_deref()
 *	Dereference VEIP context and free if refcount reaches zero.
 */
static bool ppe_drv_veip_deref( struct ppe_drv_veip_ctx *veip)
{
	if (kref_put(&veip->ref, ppe_drv_veip_free)) {
		ppe_drv_trace("%p: reference goes down to 0 for port\n", veip);
		return true;
	}
	return false;
}

/*
 * ppe_drv_veip_list_add()
 *	Add a PPE port to the VEIP interface's port list.
 *	Note: Caller must hold p->lock
 */
ppe_drv_ret_t ppe_drv_veip_port_list_add(struct ppe_drv_iface *ppe_iface, struct ppe_drv_port *pp)
{
	struct ppe_drv_veip_ctx *veip_ctx;

	veip_ctx = ppe_drv_veip_alloc();
	if (!veip_ctx) {
		ppe_drv_warn("%p: Failed to allocate context for veip vp\n", ppe_iface);
		return PPE_DRV_RET_PORT_ALLOC_FAIL;
	}

	veip_ctx->port = pp;
	kref_init(&veip_ctx->ref);
	list_add(&veip_ctx->list, &ppe_iface->veip_port);

	ppe_drv_trace("Successfully added port %u to list\n", pp->port);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_port_list_del()
 *	Remove all ports from the VEIP interface's list.
 *	Note: Caller must hold p->lock
 */
ppe_drv_ret_t ppe_drv_veip_port_list_del(struct ppe_drv_iface *ppe_iface)
{
	struct ppe_drv_veip_ctx *veip_ctx, *tmp;

	list_for_each_entry_safe(veip_ctx, tmp, &ppe_iface->veip_port, list) {
		ppe_drv_trace("Deleting VP Port Num: %u (port: %p)", veip_ctx->port->port, veip_ctx->port);
		ppe_drv_veip_deref(veip_ctx);
	}

	ppe_drv_trace("Port list deletion complete\n");
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_port_list_get()
 *	Retrieve the list of ports associated with a VEIP interface.
 *	Returns port numbers in a fixed-size array provided by the caller.
 */
ppe_drv_ret_t ppe_drv_veip_port_list_get(struct ppe_drv_iface *ppe_iface, struct ppe_drv_port **veip_ports, uint8_t *num_ports)
{
	struct ppe_drv_veip_ctx *veip_ctx;
	uint8_t index = 0;

	/*
	 * Traverse VEIP list and collect up to two port pointers.
	 * Note: This API does not take references on the ports.
	 */
	list_for_each_entry(veip_ctx, &ppe_iface->veip_port, list) {
		if (index >= MAX_VEIP_PORTS) {
			break;
		}

		veip_ports[index] = veip_ctx->port;
		index++;
	}

	*num_ports = index;
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_port_set()
 *	Associate a virtual VEIP port with a physical loopback port.
 */
ppe_drv_ret_t ppe_drv_veip_port_set(uint32_t vp_num)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint16_t loopback_port = p->loopback_port_info.port_id;
	sw_error_t err = SW_OK;

	/*
	 * if xmit port is know during init time Associate VP with xmit port.
	 */
	if (PPE_DRV_PHY_PORT_CHK(loopback_port)) {
		err = fal_vport_physical_port_id_set(PPE_DRV_SWITCH_ID, vp_num, loopback_port);
		if (err != SW_OK) {
			ppe_drv_warn("%p: failed to set loopback port:%d for vp port:%d", p, loopback_port, vp_num);
			return PPE_DRV_RET_PORT_ALLOC_FAIL;
		}
		ppe_drv_trace("Successfully set loopback port for vp_num: %u", vp_num);
	}
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_eg_vpgroup_set()
 *	Configure the VP group ID for a port in the Egress VP Table.
 */
ppe_drv_ret_t ppe_drv_veip_eg_vpgroup_set(uint32_t vport_index, uint32_t vpgroup_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t err = SW_OK;

	err = fal_port_vlan_vpgroup_set(PPE_DRV_SWITCH_ID, vport_index, FAL_PORT_VLAN_EGRESS, vpgroup_id);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Failed to set VP group id in EG_VP_TBL for index : %d, error: %d\n",
				p, vport_index, err);
		return PPE_DRV_RET_VEIP_VPGROUP_SET_FAIL;
	}

	ppe_drv_trace("Successfully set VP group for vport_index: %u", vport_index);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_is_enabled()
 *	Check if VEIP is enabled for the given net_device.
 */
bool ppe_drv_veip_is_enabled(struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface = NULL;

	/*
	 * Get iface from dev name.
	 */
	iface = ppe_drv_iface_get_by_dev(dev);
	if (!iface) {
		ppe_drv_warn("%p: Failed to get iface for interface: %s\n", p, dev->name);
		return false;
	}

	if (iface->type != PPE_DRV_IFACE_TYPE_VEIP) {
		ppe_drv_warn("%p: Interface is not of type VEIP: %u", p, iface->type);
		return false;
	}

	ppe_drv_trace("%s: Interface is of type VEIP.", dev->name);
	return true;
}
EXPORT_SYMBOL(ppe_drv_veip_is_enabled);

/*
 * ppe_drv_veip_is_hgu_rule_valid()
 *	Check if HGU rule is valid for VEIP interface or not.
 */
bool ppe_drv_veip_is_hgu_rule_valid(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;

	if (!iface) {
		ppe_drv_warn("%p: Invalid iface provided\n", p);
		return false;
	}

	if ((iface->type == PPE_DRV_IFACE_TYPE_VEIP) &&
			!(iface->flags & PPE_DRV_IFACE_FLAG_HGU_RULE_VALID)) {
		ppe_drv_info("%p: VEIP interface with HGU rule not set. Type: %u, Flags: %x\n",
				p, iface->type, iface->flags);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(ppe_drv_veip_is_hgu_rule_valid);

/*
 * ppe_drv_veip_get_port_internal()
 *      Get VEIP VP ports of a specific type (PON or GW) from the interface.
 *      Caller must hold ppe_drv_gbl->lock.
 */
int32_t ppe_drv_veip_get_port_internal(struct ppe_drv_iface *iface, enum ppe_drv_port_type type)
{
	struct ppe_drv_veip_ctx *veip_ctx;
	int32_t port_num = PPE_DRV_INVALID_PORT;

	list_for_each_entry(veip_ctx, &iface->veip_port, list) {
		if ((veip_ctx->port) && (veip_ctx->port->type == type)) {
			port_num = veip_ctx->port->port;
			break;
		}
	}

	return port_num;
}

/*
 * ppe_drv_veip_get_port()
 *      Get VEIP VP ports of a specific type (PON or GW) from the interface.
 */
int32_t ppe_drv_veip_get_port(struct ppe_drv_iface *iface, enum ppe_drv_port_type type)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int32_t port_num;

	spin_lock_bh(&p->lock);
	port_num = ppe_drv_veip_get_port_internal(iface, type);
	spin_unlock_bh(&p->lock);

	return port_num;
}
EXPORT_SYMBOL(ppe_drv_veip_get_port);

/*
 * ppe_drv_veip_flag_set()
 *	Set the VEIP flag (HGU rule valid) in iface.
 */
void ppe_drv_veip_flag_set(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;

	if (!iface) {
		ppe_drv_warn("%p: Invalid iface provided\n", p);
		return;
	}

	iface->flags |= PPE_DRV_IFACE_FLAG_HGU_RULE_VALID;
	ppe_drv_trace("ppe_drv_veip_flag_set: iface_flag: %d", iface->flags);

	return;
}

/*
 * ppe_drv_veip_flag_clear()
 *      Clear the VEIP flag (HGU rule valid) in iface.
 */
void ppe_drv_veip_flag_clear(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;

	if (!iface) {
		ppe_drv_warn("%p: Invalid iface provided\n", p);
		return;
	}

	iface->flags &= ~PPE_DRV_IFACE_FLAG_HGU_RULE_VALID;
	ppe_drv_trace("ppe_drv_veip_flag_clear: iface_flag: %d", iface->flags);

	return;
}

/*
 * ppe_drv_veip_eg_vpgroup_clear()
 *	Clear VP group ID in EG_VP_TBL.
 */
ppe_drv_ret_t ppe_drv_veip_eg_vpgroup_clear(uint32_t vport_index)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint32_t vpgroup_id = 0;
	sw_error_t err = SW_OK;

	err = fal_port_vlan_vpgroup_set(PPE_DRV_SWITCH_ID, vport_index, FAL_PORT_VLAN_EGRESS, vpgroup_id);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Failed to clear VP group id in EG_VP_TBL for index : %d, error: %d\n",
				p, vport_index, err);
		return PPE_DRV_RET_VEIP_VPGROUP_SET_FAIL;
	}

	ppe_drv_trace("%s: Successfully cleared VP group for vport_index: %u", __func__, vport_index);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_l2_vp_sc_config()
 *	Configure the Service Code for an L2 VP port.
 */
bool ppe_drv_veip_l2_vp_sc_config(struct ppe_drv_port *pp, ppe_drv_sc_t sc, uint32_t phy_port)
{
	fal_enqueue_cfg_t enq_cfg = {0};
	sw_error_t err;

	enq_cfg.index_entry.enqueue_servcode.service_code = sc;
	enq_cfg.index_entry.enqueue_servcode.phy_port = phy_port;
	enq_cfg.index_entry.enqueue_en = A_TRUE;
	enq_cfg.rule_entry.enqueue_type = FAL_ENQUEUE_SERVCODE;
	enq_cfg.rule_entry.dst_port = pp->port;
	enq_cfg.index_entry.enqueue_servcode.queue_select_en = A_FALSE;

	err = fal_qm_enqueue_config_set(PPE_DRV_SWITCH_ID, &enq_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to set service code config for port: %d", pp->port);
		return false;
	}

	ppe_drv_trace("L2_VP_TBL configured for port: %d with SC: %d", pp->port, sc);
	return true;
}

/*
 * ppe_drv_veip_port_vsi_attach()
 *	Attach the VEIP port to a VSI without modifying VSI membership directly.
 */
static void  ppe_drv_veip_port_vsi_attach(struct ppe_drv_port *pp, struct ppe_drv_vsi *vsi)
{
	sw_error_t err;
	struct ppe_drv_vsi *active_vsi;
	fal_port_t fal_port;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(pp->port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, pp->port)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pp->port);

	ppe_drv_assert(kref_read(&pp->ref_cnt), "%p: attaching vsi to unused port: %u", pp, pp->port);

	pp->port_vsi = ppe_drv_vsi_ref(vsi);

	/*
	 * Attach the new VSI to port
	 */
	active_vsi = pp->port_vsi;
	if (!active_vsi) {
		ppe_drv_warn("%p No active VSI assigned to port: %u",
				pp, fal_port);
		goto fail;
	}

	err = fal_port_vsi_set(PPE_DRV_SWITCH_ID, fal_port, active_vsi->index);
	if (err != SW_OK) {
		ppe_drv_warn("%p port vsi configuration failed: %p port_num: 0x%x vsi_num: %u",
				pp, active_vsi, fal_port, active_vsi->index);
		goto fail;
	}

	ppe_drv_trace("%p: attaching vsi %u to port %u", pp, active_vsi->index, pp->port);
	return;

fail:
	if (vsi->type == PPE_DRV_VSI_TYPE_PORT) {
		if (pp->port_vsi) {
			ppe_drv_vsi_deref(pp->port_vsi);
		}
		pp->port_vsi = NULL;
	}
}

/*
 * ppe_drv_veip_port_vsi_detach()
 *	Detach port from VSI.
 */
static void ppe_drv_veip_port_vsi_detach(struct ppe_drv_port *pp, struct ppe_drv_vsi *vsi)
{
	fal_port_t fal_port;
	sw_error_t err;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(pp->port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, pp->port)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pp->port);

	err = fal_port_vsi_set(PPE_DRV_SWITCH_ID, fal_port, FAL_VSI_INVALID);
	if (err != SW_OK) {
		ppe_drv_warn("%p port vsi configuration failed: %p port_num: 0x%x vsi_num: %u",
				pp, vsi, fal_port, vsi->index);
	}

	ppe_drv_trace("%p: detached vsi %u from port %u", pp, vsi->index, pp->port);

	ppe_drv_vsi_deref(vsi);
	return;
}

/*
 * ppe_drv_veip_vsi_member_set()
 *	Setting VSI membership for given VSI index.
 */
static ppe_drv_ret_t ppe_drv_veip_vsi_member_set(uint32_t port_id, uint32_t vsi_idx)
{
	uint32_t port_value = 0, vport_value = 0;
	fal_port_t fal_port;
	fal_vsi_member_t vsi_member;
	sw_error_t ret;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_id)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_id);
	/*
	 * VSI member get
	 */
	port_value = FAL_PORT_ID_VALUE(fal_port);
	vport_value = port_value - SSDK_MIN_VIRTUAL_PORT_ID;
	ppe_drv_trace("port_id:0x%x, v_port_value:%d\n", fal_port, vport_value);

	ret = fal_vsi_member_get(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if (ret != SW_OK) {
		ppe_drv_warn("Invalid VSI member for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_FOUND;
	}

	if (FAL_IS_VPORT(fal_port)) {
		vsi_member.member_vports[vport_value / 32] |= (1 << (vport_value % 32));
		ppe_drv_trace("vsi_member.member_vports[%d]: 0x%x\n",
				vport_value / 32, vsi_member.member_vports[vport_value / 32]);
	} else {
		vsi_member.member_ports |= (1 << vport_value);
		ppe_drv_trace("vsi_member.member_ports: 0x%x\n", vsi_member.member_ports);
	}

	/*
	 * VSI member set
	 */
	ret = fal_vsi_member_set(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if (ret != SW_OK) {
		ppe_drv_warn("VSI member updated failed for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_SET;
	}

	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_vsi_member_clear()
 *	Clear the VSI membership for given VSI index.
 */
static ppe_drv_ret_t ppe_drv_veip_vsi_member_clear(uint32_t port_id, uint32_t vsi_idx)
{
	uint32_t port_value = 0, vport_value = 0;
	fal_port_t fal_port;
	fal_vsi_member_t vsi_member;
	sw_error_t ret;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_id)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_id);
	/*
	 * VSI member get
	 */
	port_value = FAL_PORT_ID_VALUE(fal_port);
	vport_value = port_value - SSDK_MIN_VIRTUAL_PORT_ID;
	ppe_drv_trace("port_id:0x%x, v_port_value:%d\n", fal_port, vport_value);

	ret = fal_vsi_member_get(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if (ret != SW_OK) {
		ppe_drv_warn("Invalid VSI member for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_FOUND;
	}

	if (FAL_IS_VPORT(fal_port)) {
		vsi_member.member_vports[vport_value / 32] &= ~(1 << (vport_value % 32));
		ppe_drv_trace("vsi_member.member_vports[%d]: 0x%x\n",
				vport_value / 32, vsi_member.member_vports[vport_value / 32]);
	} else {
		vsi_member.member_ports &= ~(1 << vport_value);
		ppe_drv_trace("vsi_member.member_ports: 0x%x\n", vsi_member.member_ports);
	}

	/*
	 * VSI member set
	 */
	ret = fal_vsi_member_set(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if (ret != SW_OK) {
		ppe_drv_warn("VSI member updated failed for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_SET;
	}

	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_vp_mtu_mru_set()
 *	Set MTU and MRU of given VP port in PPE.
 */
bool ppe_drv_veip_vp_mtu_mru_set(struct ppe_drv_iface *iface, uint16_t mtu, uint16_t mru)
{
	sw_error_t err;
	fal_mtu_ctrl_t mtu_ctrl = {0};
	fal_mtu_cfg_t mtu_cfg = {0};
	fal_mru_ctrl_t mru_ctrl = {0};
	uint8_t num_ports;
	struct ppe_drv_port *veip_ports[MAX_VEIP_PORTS];
	uint8_t i;

	/*
	 * Adjust MTU/MRU to accept MAC header addtional to mtu.
	 *
	 * Note: PPE ignore vlan headers at L2 layer and only need
	 * additional 14 byte for MAC header in PORT MTU/MRU
	 * while considering size of frames at L2
	 */
	uint16_t port_mtu = mtu + ETH_HLEN;
	uint16_t port_mru = mru + ETH_HLEN;

	if (ppe_drv_veip_port_list_get(iface, veip_ports, &num_ports) != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to get the list of VEIP ports\n", iface);
		return false;
	}

	if (mtu > PPE_DRV_PORT_JUMBO_MAX || mru > PPE_DRV_PORT_JUMBO_MAX) {
		ppe_drv_warn("set mtu/mru fail - %u/%u larger than max %u", mtu, mru, PPE_DRV_PORT_JUMBO_MAX);
		return false;
	}

	for (i = 0; i < num_ports; i++) {
		struct ppe_drv_port *pp = veip_ports[i];
		ppe_drv_assert(kref_read(&pp->ref_cnt), "%p: setting mtu/mru for unused port:%u", pp, pp->port);

		/*
		 * Configure MTU.
		 * TODO: update extra header length and mtu_type as required for tunnel VP.
		 */
		mtu_ctrl.mtu_size = port_mtu;
		mtu_ctrl.action = FAL_MAC_RDT_TO_CPU;
		err = fal_port_mtu_set(PPE_DRV_SWITCH_ID, pp->port, &mtu_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to configure port mtu: %u", pp, port_mtu);
			return false;
		}

		mtu_cfg.mtu_enable = A_TRUE;
		mtu_cfg.mtu_type = FAL_MTU_ETHERNET;

		err = fal_port_mtu_cfg_set(PPE_DRV_SWITCH_ID, pp->port, &mtu_cfg);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to configure port mtu config: %u", pp, port_mtu);
			return false;
		}

		mru_ctrl.mru_size = port_mru;
		mru_ctrl.action = FAL_MAC_RDT_TO_CPU;

		err = fal_port_mru_set(PPE_DRV_SWITCH_ID, pp->port, &mru_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to configure port mru: %u", pp, port_mru);
			return false;
		}

		/*
		 * Update shadow copy
		 */
		pp->mtu = port_mtu;
		pp->mru = port_mru;
	}

	/*
	 * Set MTU/MRU of associated L3_IF
	 */
	if (iface->l3) {
		ppe_drv_l3_if_mtu_mru_set(iface->l3, mtu, mru);
	}

	ppe_drv_info("mtu %u mru %u set for ports %u and %u\n", mtu, mru, veip_ports[0]->port, veip_ports[1]->port);
	return true;
}

/*
 * ppe_drv_veip_vp_mtu_mru_disable()
 *	Disable VEIP VP port MTU and MRU check in PPE.
 */
bool ppe_drv_veip_vp_mtu_mru_disable(struct ppe_drv_iface *iface)
{
	sw_error_t err;
	fal_mtu_ctrl_t mtu_ctrl = {0};
	fal_mru_ctrl_t mru_ctrl = {0};
	uint8_t num_ports;
	struct ppe_drv_port *veip_ports[MAX_VEIP_PORTS];
	uint8_t i;

	if (ppe_drv_veip_port_list_get(iface, veip_ports, &num_ports) != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to get the list of VEIP ports\n", iface);
		return false;
	}

	for (i = 0; i < num_ports; i++) {
		struct ppe_drv_port *pp = veip_ports[i];
		ppe_drv_assert(kref_read(&pp->ref_cnt), "%p: setting mtu/mru for unused port:%u", pp, pp->port);

		/*
		 * Disable exception for MTU and MRU check
		 */
		mtu_ctrl.mtu_size = PPE_DRV_PORT_JUMBO_MAX;
		mtu_ctrl.action = FAL_MAC_FRWRD;
		err = fal_port_mtu_set(PPE_DRV_SWITCH_ID, pp->port, &mtu_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to configure port mtu: %u", pp, PPE_DRV_PORT_JUMBO_MAX);
			return false;
		}

		mru_ctrl.mru_size = PPE_DRV_PORT_JUMBO_MAX;
		mru_ctrl.action = FAL_MAC_FRWRD;
		err = fal_port_mru_set(PPE_DRV_SWITCH_ID, pp->port, &mru_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to configure port mru: %u", pp, PPE_DRV_PORT_JUMBO_MAX);
			return false;
		}
	}

	/*
	 * Set MTU/MRU of associated L3_IF
	 */
	if (iface->l3) {
		ppe_drv_l3_if_mtu_mru_disable(iface->l3);
	}

	ppe_drv_info("mtu-mru disable for port %u and %u", veip_ports[0]->port, veip_ports[1]->port);
	return true;
}

/*
 * ppe_drv_veip_mac_addr_set()
 *	Set MAC address for VEIP VPs.
 */
void ppe_drv_veip_mac_addr_set(struct ppe_drv_iface *iface, const uint8_t *mac_addr)
{
	sw_error_t err;
	fal_macaddr_entry_t macaddr = {0};
	uint8_t num_ports;
	struct ppe_drv_port *veip_ports[MAX_VEIP_PORTS];
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_vsi *vsi;
	uint8_t i;

	if (ppe_drv_veip_port_list_get(iface, veip_ports, &num_ports) != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to get the list of VEIP ports\n", iface);
		return;
	}

	vsi = ppe_drv_iface_vsi_get(iface);
	if (!vsi) {
		ppe_drv_warn("%p: No VSI associated with iface\n", iface);
		return;
	}

	l3_if = vsi->l3_if;
	if (!l3_if) {
		ppe_drv_warn("%p: No L3_IF associated with vsi(%p)\n", iface, vsi);
		return;
	}

	macaddr.valid = A_TRUE;
	memcpy(macaddr.mac_addr.uc, mac_addr, ETH_ALEN);

	for (i = 0; i < num_ports; i++) {
		struct ppe_drv_port *pp = veip_ports[i];
		ppe_drv_assert(kref_read(&pp->ref_cnt), "%p: setting mac addr on an unused port:%u", pp, pp->port);

		err = fal_ip_port_macaddr_set(PPE_DRV_SWITCH_ID, pp->port, &macaddr);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to configure port mac address", pp);
			return;
		}

		/*
		 * Update shadow copy
		 */
		memcpy(pp->mac_addr, mac_addr, ETH_ALEN);
		pp->mac_valid = 1;
	}

	/*
	 * Re-configuring L3_MY_MAC table.
	 */
	uint8_t mac_addr_rw[ETH_ALEN];
	memcpy(mac_addr_rw, mac_addr, ETH_ALEN);
	if (!ppe_drv_l3_if_ig_vsi_mac_update(l3_if, mac_addr_rw, vsi)) {
		ppe_drv_warn("%p: Failed to set mac addr(%pM) to l3_if in L3_MY_MAC %u", l3_if,
				mac_addr, l3_if->l3_if_index);
		return;
	}

	ppe_drv_trace("mac_addr set for port %u and %u", veip_ports[0]->port, veip_ports[1]->port);
	return;
}

/*
 * ppe_drv_veip_mac_addr_clear()
 *	Clear MAC address for VEIP VPs.
 */
void ppe_drv_veip_mac_addr_clear(struct ppe_drv_iface *iface)
{
	sw_error_t err;
	fal_macaddr_entry_t macaddr = {0};
	uint8_t num_ports;
	struct ppe_drv_port *veip_ports[MAX_VEIP_PORTS];
	uint8_t i;

	if (ppe_drv_veip_port_list_get(iface, veip_ports, &num_ports) != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to get the list of VEIP ports\n", iface);
		return;
	}

	macaddr.valid = A_FALSE;

	for (i = 0; i < num_ports; i++) {
		struct ppe_drv_port *pp = veip_ports[i];
		ppe_drv_assert(kref_read(&pp->ref_cnt), "%p: operating on an unused port:%u", pp, pp->port);

		err = fal_ip_port_macaddr_set(PPE_DRV_SWITCH_ID, pp->port, &macaddr);
		if (err != SW_OK) {
			ppe_drv_warn("%p: unable to clear port mac address", pp);
			return;
		}

		memset(pp->mac_addr, 0, ETH_ALEN);
		pp->mac_valid = 0;
	}

	ppe_drv_trace("mac addr cleared for port %u and %u\n", veip_ports[0]->port, veip_ports[1]->port);
	return;
}

/*
 * ppe_drv_veip_sc_map()
 *	Configure Service Code mapping for VEIP ports (GW/PON) based on their type.
 */
static void ppe_drv_veip_sc_map(struct ppe_drv_port *pp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	enum ppe_drv_port_type pt_type = pp->type;


	switch (pt_type) {
		case PPE_DRV_PORT_VIRTUAL_GW:
			/*
			 * Configure lpbk_port.
			 */
			if (!ppe_drv_lpbk_port_info_ctx_fill(PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US)) {
				ppe_drv_warn("%p: Error in configuring loopback port info.\n", p);
				return;
			}

			/*
			 * PON port to loopback port queue mapping.
			 */
			if (!ppe_drv_port_ucast_queue_set(pp, 0, p->loopback_port_info.ucastq_start)) {
				ppe_drv_warn("%p: loopback queue mapping to GW port failed.\n", p);
			}
			break;

		case PPE_DRV_PORT_VIRTUAL_PON:
			/*
			 * Configure lpbk_port.
			 */
			if (!ppe_drv_lpbk_port_info_ctx_fill(PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS)) {
				ppe_drv_warn("%p: Error in configuring loopback port info.\n", p);
				return;
			}

			/*
			 * Configure L2_VP port table for UNI port with LOOPBACK service code
			 */
			if (!ppe_drv_veip_l2_vp_sc_config(pp, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC, p->loopback_port_info.port_id)) {
				ppe_drv_warn("%p: Error in configuring L2 VP table  %d\n", p, pp->port);
				return;
			}

			/*
			 * PON port to loopback port queue mapping.
			 */
			if (!ppe_drv_port_ucast_queue_set(pp, 0, p->loopback_port_info.ucastq_start)) {
				ppe_drv_warn("%p: loopback queue mapping to PON port failed.\n", p);
			}
			break;
		default:
			ppe_drv_warn("%p: Invalid PPE port type: %u\n", p, pp->port);
	}

	return;
}

/*
 * ppe_drv_veip_sc_unmap()
 *	Unmap Service Codes and reset configuration for VEIP ports.
 */
static void ppe_drv_veip_sc_unmap(struct ppe_drv_port *pp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	enum ppe_drv_port_type pt_type = pp->type;

	switch (pt_type) {
		case PPE_DRV_PORT_VIRTUAL_GW:
			break;

		case PPE_DRV_PORT_VIRTUAL_PON:
			/*
			 * Reset L2_VP port table for UNI port with LOOPBACK service code
			 */
			if (!ppe_drv_port_l2_vp_sc_reset(pp)) {
				ppe_drv_warn("%p: Error in reset L2 VP table  %d\n", p, pp->port);
				return;
			}
			break;

		default:
			ppe_drv_warn("%p: Invalid PPE port type: %u\n", p, pp->port);
	}
	return;
}

/*
 * ppe_drv_veip_gw_port_sc()
 *	Determine if a Service Code should be applied for traffic destined to a GW VP.
 */
ppe_drv_ret_t ppe_drv_veip_gw_port_sc(struct ppe_drv_port *tx_port, ppe_drv_sc_t *service_code)
{
	struct net_device *tx_dev;
	struct ppe_drv *p = ppe_drv_gbl;
	uint32_t serv_code;
	bool valid = false;

	if (tx_port == NULL) {
		return PPE_DRV_RET_SUCCESS;
	}

	tx_dev = tx_port->dev;

	if (tx_port->type == PPE_DRV_PORT_VIRTUAL_GW) {
		/*
		 * Packet destined to GW_VP.
		 */
		serv_code = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC;
		valid = true;
	}

	if (valid && !ppe_drv_loopback_port_ft_pon_hgu_us_enabled(p)) {
		ppe_drv_trace("HGU US PPE accel not supported for flow rule tx dev %s\n",
				tx_dev->name);
		return PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD_FAIL;
	}

	if (valid) {
		*service_code = serv_code;
		ppe_drv_trace("HGU US service code %u added to flow rule tx dev %s\n",
				serv_code, tx_dev->name);
		return PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD;
	}

	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_tl_tbl_set()
 *	Configure Service code in TL_SERVICE_TBL.
 */
static void ppe_drv_veip_tl_tbl_set(struct ppe_drv_port *port, ppe_drv_sc_t sc)
{
	fal_port_t fal_port;
	sw_error_t err;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port->port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port->port)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);

	/*
	 * Setting TL_SERVICE_TBL.
	 */
	err = fal_port_servcode_set(PPE_DRV_SWITCH_ID, fal_port, sc);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to configure TL_SERVICE_TBL for SC: %u\n", sc);
		return;
	}

	ppe_drv_trace("TL_SERVICE_TBL configured for SC: %u  and port: %u\n", sc, port->port);
	return;
}

/*
 * ppe_drv_veip_tl_tbl_clear()
 *	Clear Service code in TL_SERVICE_TBL.
 */
static void ppe_drv_veip_tl_tbl_clear(struct ppe_drv_port *port)
{
	fal_port_t fal_port;
	uint32_t sc_idx = FAL_SERVCODE_INVALID;
	sw_error_t err;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port->port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port->port)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);

	/*
	 * Setting TL_SERVICE_TBL.
	 */
	err = fal_port_servcode_set(PPE_DRV_SWITCH_ID, fal_port, sc_idx);
	if (err != SW_OK) {
		ppe_drv_warn("Failed to clear TL_SERVICE_TBL for port: %u\n", port->port);
		return;
	}

	ppe_drv_trace("TL_SERVICE_TBL cleared for port: %u\n", port->port);
	return;
}

/*
 * ppe_drv_veip_port_init()
 *	Helper function to initialize a single VEIP port
 */
static ppe_drv_ret_t ppe_drv_veip_port_init(struct ppe_drv_port **port, enum ppe_drv_port_type type,
		struct net_device *dev, uint8_t *port_num)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp;

	/*
	 * Allocate VP port
	 */
	pp = ppe_drv_port_alloc(type, dev, false, false);
	if (!pp) {
		ppe_drv_warn("%p: Failed to allocate port type %d for %s", p, type, dev->name);
		return PPE_DRV_RET_PORT_ALLOC_FAIL;
	}

	*port_num = ppe_drv_port_num_get(pp);

	/*
	 * Clear port hardware stats
	 */
	if (!ppe_drv_port_clear_hw_vp_stats(*port_num)) {
		ppe_drv_warn("%p: Failed to clear port stats for port %u", p, *port_num);
		ppe_drv_port_deref(pp);
		return PPE_DRV_RET_PORT_ALLOC_FAIL;
	}

	/*
	 * Set loopback port mapping
	 */
	if (ppe_drv_veip_port_set(*port_num) != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to set loopback port for port %u", p, *port_num);
		ppe_drv_port_deref(pp);
		return PPE_DRV_RET_PORT_ALLOC_FAIL;
	}

	/*
	 * Configure service code
	 */
	ppe_drv_veip_sc_map(pp);

	/*
	 * Assign to output parameter
	 */
	*port = pp;

	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_veip_init()
 *	Allocate and initialize VEIP interface with both VP ports
 */
ppe_drv_ret_t ppe_drv_veip_init(struct ppe_drv_iface *iface, struct net_device *base_dev,
		struct ppe_drv_veip_vp_info *vp_info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *base_if, *parent_if;
	struct ppe_drv_port *veip_ports[MAX_VEIP_PORTS] = {NULL, NULL};
	uint8_t port_nums[MAX_VEIP_PORTS];
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_vsi *vsi;
	ppe_drv_ret_t ret;
	uint8_t i;
	struct net_device *dev = iface->dev;

	spin_lock_bh(&p->lock);

	/*
	 * Initialize both GW and PON ports
	 */
	ret = ppe_drv_veip_port_init(&veip_ports[0], PPE_DRV_PORT_VIRTUAL_GW, dev, &port_nums[0]);
	if (ret != PPE_DRV_RET_SUCCESS) {
		goto free_iface;
	}

	ret = ppe_drv_veip_port_init(&veip_ports[1], PPE_DRV_PORT_VIRTUAL_PON, dev, &port_nums[1]);
	if (ret != PPE_DRV_RET_SUCCESS) {
		goto cleanup_gw_port;
	}

	/*
	 * Add both ports to interface list
	 */
	for (i = 0; i < MAX_VEIP_PORTS; i++) {
		ret = ppe_drv_veip_port_list_add(iface, veip_ports[i]);
		if (ret != PPE_DRV_RET_SUCCESS) {
			ppe_drv_warn("%p: Failed to add port %u to list", p, port_nums[i]);
			goto cleanup_port_list;
		}
	}

	/*
	 * Set GW port (first port) on interface
	 */
	if (!ppe_drv_iface_port_set(iface, veip_ports[0])) {
		ppe_drv_warn("%p: Failed to set GW port on interface", p);
		ret = PPE_DRV_RET_PORT_ALLOC_FAIL;
		goto cleanup_port_list;
	}

	/*
	 * Set up base interface
	 */
	base_if = ppe_drv_iface_get_by_dev_internal(base_dev);
	if (!base_if) {
		ppe_drv_warn("%p: Failed to get base interface", p);
		ret = PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
		goto cleanup_port_list;
	}
	ppe_drv_iface_base_set(iface, base_if);
	base_if->dev = base_dev;

	/*
	 * Allocate and configure VSI
	 */
	vsi = ppe_drv_vsi_alloc(PPE_DRV_VSI_TYPE_PORT);
	if (!vsi) {
		ppe_drv_warn("%p: Failed to allocate VSI", p);
		ret = PPE_DRV_RET_VSI_ALLOC_FAIL;
		goto cleanup_base_if;
	}
	ppe_drv_iface_vsi_set(iface, vsi);

	/*
	 * Get and configure L3_IF
	 */
	l3_if = ppe_drv_vsi_l3_if_get_and_ref(vsi);
	if (!l3_if) {
		ppe_drv_warn("%p: Failed to get L3_IF", p);
		ret = PPE_DRV_RET_L3_IF_ALLOC_FAIL;
		goto cleanup_vsi;
	}
	ppe_drv_iface_l3_if_set(iface, l3_if);

	/*
	 * Configure L3_MY_MAC table
	 */
	if (!ppe_drv_l3_if_ig_vsi_mac_set(l3_if, (uint8_t *)dev->dev_addr, vsi)) {
		ppe_drv_warn("%p: Failed to set MAC in L3_MY_MAC", p);
		ret = PPE_DRV_RET_MAC_ADDR_SET_CFG_FAIL;
		goto cleanup_l3_if;
	}

	/*
	 * Configure IN_L3_IF table with PON port
	 */
	if (!ppe_drv_l3_if_dest_info_set(l3_if, veip_ports[1])) {
		ppe_drv_warn("%p: Failed to set dest info in IN_L3_IF", p);
		ret = PPE_DRV_RET_L3_IF_ALLOC_FAIL;
		goto cleanup_l3_mac;
	}

	/*
	 * Configure VSI REMAP table if bridge exists
	 */
	parent_if = ppe_drv_iface_parent_get(base_if);
	if (parent_if && (parent_if->type == PPE_DRV_IFACE_TYPE_BRIDGE) && parent_if->vsi) {
		if (!ppe_drv_vsi_remap_entries(vsi, parent_if->vsi)) {
			ppe_drv_warn("%p: Failed to remap VSI entries", p);
			goto cleanup_in_l3_if;
		}
	}

	/*
	 * Set VP group IDs in EG_VP_TBL (cross-reference between ports)
	 */
	ret = ppe_drv_veip_eg_vpgroup_set(port_nums[0], port_nums[1]);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to set GW VP group", p);
		ret = PPE_DRV_RET_VEIP_VSI_REMAP_FAIL;
		goto cleanup_vsi_remap;
	}

	ret = ppe_drv_veip_eg_vpgroup_set(port_nums[1], port_nums[0]);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to set PON VP group", p);
		goto cleanup_vp_group_gw;
	}

	/*
	 * Attach L3_IF and VSI to both ports
	 */
	for (i = 0; i < MAX_VEIP_PORTS; i++) {
		if (!ppe_drv_port_l3_if_attach(veip_ports[i], l3_if)) {
			ppe_drv_warn("%p: Failed to attach L3_IF to port %u", p, port_nums[i]);
			ret = PPE_DRV_RET_L3_IF_PORT_ATTACH_FAIL;
			goto cleanup_l3_if_attach;
		}
		ppe_drv_veip_port_vsi_attach(veip_ports[i], vsi);
	}

	/*
	 * Update VSI membership for PON port
	 */
	ret = ppe_drv_veip_vsi_member_set(port_nums[1], vsi->index);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Failed to set VSI membership", p);
		ret = PPE_DRV_RET_VEIP_VSI_ATTACH_FAIL;
		goto cleanup_vsi_attach;
	}

	/*
	 * Configure TL_SERVICE table.
	 */
	if ((base_if->port) && (!base_if->veip_cnt++)) {
		ppe_drv_veip_tl_tbl_set(base_if->port, PPE_DRV_SC_FEATURE_PON_HGU_PPTP);
	}

	/*
	 * Return VP port numbers
	 */
	vp_info->gw_vp_num = port_nums[0];
	vp_info->pon_vp_num = port_nums[1];

#ifdef NSS_PPE_DRV_HW_GRO
	/*
	 * Enable HW GRO for both VEIP VP ports (GW and PON) so that PPE flow
	 * rule push sets gro_en in the hardware flow entry when the peer port
	 * is a netfn offload VP.  This mirrors what edma_dp_init() does for
	 * physical ports.  Both ports must be set because either can appear as
	 * rx_port in ppe_drv_hw_gro_is_enabled() depending on traffic direction.
	 * This is enabled by default and cannot be disabled via ethtool like other physical ports
	 */
	veip_ports[0]->hw_gro_en = true;
	veip_ports[1]->hw_gro_en = true;
	ppe_drv_info("%p: HW GRO enabled for VEIP ports GW:%u PON:%u",
			p, port_nums[0], port_nums[1]);
#endif

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%p: VEIP interface initialized - GW: %u, PON: %u", p, port_nums[0], port_nums[1]);
	return PPE_DRV_RET_SUCCESS;

	/*
	 * Error handling with proper rollback
	 */
cleanup_vsi_attach:
	for (i = 0; i < MAX_VEIP_PORTS; i++) {
		ppe_drv_veip_port_vsi_detach(veip_ports[i], vsi);
	}
cleanup_l3_if_attach:
	for (i = 0; i < MAX_VEIP_PORTS; i++) {
		if (veip_ports[i]->active_l3_if_attached) {
			ppe_drv_port_l3_if_detach(veip_ports[i], l3_if);
		}
	}
cleanup_vp_group_gw:
	ppe_drv_veip_eg_vpgroup_clear(port_nums[0]);
cleanup_vsi_remap:
	if (parent_if && (parent_if->type == PPE_DRV_IFACE_TYPE_BRIDGE) && parent_if->vsi) {
		ppe_drv_vsi_clear_remap_entries(vsi, parent_if->vsi);
	}
cleanup_in_l3_if:
	ppe_drv_l3_if_dest_info_reset(l3_if);
cleanup_l3_mac:
	ppe_drv_l3_if_ig_vsi_mac_clear(l3_if);
cleanup_l3_if:
	ppe_drv_vsi_l3_if_deref(vsi);
cleanup_vsi:
	ppe_drv_vsi_deref(vsi);
cleanup_base_if:
	ppe_drv_iface_base_clear(iface);
	ret = PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
cleanup_port_list:
	ppe_drv_veip_port_list_del(iface);
	ppe_drv_veip_sc_unmap(veip_ports[1]);
	ppe_drv_port_deref(veip_ports[1]);
	ret = PPE_DRV_RET_PORT_ALLOC_FAIL;
cleanup_gw_port:
	ppe_drv_veip_sc_unmap(veip_ports[0]);
	ppe_drv_port_deref(veip_ports[0]);
	ret = PPE_DRV_RET_PORT_ALLOC_FAIL;
free_iface:
	spin_unlock_bh(&p->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_drv_veip_init);

/*
 * ppe_drv_veip_deinit()
 *	Free VEIP interface and all resources
 */
void ppe_drv_veip_deinit(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vsi *vsi;
	struct ppe_drv_iface *base_if, *parent_if;
	struct ppe_drv_port *veip_ports[2];
	uint8_t port_nums[2];
	uint8_t num_ports = 0;

	spin_lock_bh(&p->lock);

	/*
	 * Get VEIP VP port list
	 */
	if (ppe_drv_veip_port_list_get(iface, veip_ports, &num_ports) != PPE_DRV_RET_SUCCESS) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Failed to get VEIP port list", p);
		return;
	}

	if (num_ports != 2) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid number of VEIP ports: %u", p, num_ports);
		return;
	}

	/*
	 * Get port numbers
	 */
	port_nums[0] = ppe_drv_port_num_get(veip_ports[0]);
	port_nums[1] = ppe_drv_port_num_get(veip_ports[1]);

	/*
	 * Get VSI
	 */
	vsi = ppe_drv_iface_vsi_get(iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for VEIP interface", p);
		return;
	}

	/*
	 * Clear L3_MY_MAC
	 */
	if (iface->l3) {
		ppe_drv_l3_if_ig_vsi_mac_clear(iface->l3);
	}

	/*
	 * Clear VSI membership
	 */
	ppe_drv_veip_vsi_member_clear(port_nums[1], vsi->index);

	/*
	 * Detach VSIs from ports
	 */
	ppe_drv_veip_port_vsi_detach(veip_ports[0], vsi);
	ppe_drv_veip_port_vsi_detach(veip_ports[1], vsi);

	/*
	 * Detach L3_IF from ports
	 */
	if (iface->l3) {
		ppe_drv_port_l3_if_detach(veip_ports[0], iface->l3);
		ppe_drv_port_l3_if_detach(veip_ports[1], iface->l3);
	}

	/*
	 * Clear VP group IDs
	 */
	ppe_drv_veip_eg_vpgroup_clear(port_nums[0]);
	ppe_drv_veip_eg_vpgroup_clear(port_nums[1]);

	/*
	 * Clear VSI REMAP
	 */
	base_if = ppe_drv_iface_base_get(iface);
	if (base_if) {
		parent_if = ppe_drv_iface_parent_get(base_if);
		if (parent_if && (parent_if->type == PPE_DRV_IFACE_TYPE_BRIDGE) && parent_if->vsi) {
			ppe_drv_vsi_clear_remap_entries(vsi, parent_if->vsi);
		}
	}

	/*
	 * Clear TL_SERVICE table.
	 */
	if ((base_if->port) && (!--base_if->veip_cnt)){
		ppe_drv_veip_tl_tbl_clear(base_if->port);
	}

	/*
	 * Reset IN_L3_IF
	 */
	if (iface->l3) {
		ppe_drv_l3_if_dest_info_reset(iface->l3);
	}

	/*
	 * Cleanup service codes
	 */
	ppe_drv_veip_sc_unmap(veip_ports[0]);
	ppe_drv_veip_sc_unmap(veip_ports[1]);

	/*
	 * Delete port list
	 */
	ppe_drv_veip_port_list_del(iface);

	/*
	 * Deref ports
	 */
	ppe_drv_port_deref(veip_ports[0]);
	ppe_drv_port_deref(veip_ports[1]);

	/*
	 * Cleanup VSI and L3_IF
	 */
	if (vsi) {
		if (vsi->l3_if) {
			ppe_drv_l3_if_deref(vsi->l3_if);
		}
		ppe_drv_vsi_deref(vsi);
	}

	/*
	 * Clear interface associations
	 */
	ppe_drv_iface_base_clear(iface);
	ppe_drv_iface_vsi_clear(iface);
	ppe_drv_iface_l3_if_clear(iface);

	spin_unlock_bh(&p->lock);

	ppe_drv_info("%p: VEIP interface freed", p);
}
EXPORT_SYMBOL(ppe_drv_veip_deinit);
