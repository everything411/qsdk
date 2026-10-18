/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "ppe_drv.h"

/*
 * ppe_drv_eip_set_sc_queue_config
 *	Configure ucast queue for service code for PPE to EIP
 */
void ppe_drv_eip_sc_queue_config_set(void)
{
	int32_t queue_id = ppe_drv_port_ucast_queue_get_by_port(PPE_DRV_PORT_EIP197);

	if (queue_id < 0) {
		ppe_drv_warn("Invalid queue id for EIP port : %u\n", PPE_DRV_PORT_EIP197);
		return;
	}

	/*
	 * Set ucast queue for decap direction
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_IPSEC_PPE2EIP_DECAP, queue_id,
			PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);

	/*
	 * Set ucast for encap direction
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP, queue_id,
			PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);

	/*
	 * Set ucast for VP encap direction
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_IPSEC_EDMA_VP_ENCAP, queue_id,
			PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);

	/*
	 * set ucast queue for VP 3-tuple encap direction.
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_IPSEC_EDMA_VP_3_TUPLE_ENCAP, queue_id,
			PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);

	/*
	 * Set ucast for VP decap direction
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_IPSEC_EDMA_VP_DECAP, queue_id,
			PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
}
EXPORT_SYMBOL(ppe_drv_eip_sc_queue_config_set);

/*
 * ppe_drv_eip_set_lookup_mode()
 *	Set lookup mode to flow record lookup mode.
 */
ppe_drv_ret_t ppe_drv_eip_set_lookup_mode(ppe_drv_eip_flow_lookup_mode_t mode)
{
	fal_flow_eip_lookup_mode_t lookup_mode;
	uint32_t dev_id = PPE_DRV_SWITCH_ID;
	sw_error_t ret = SW_OK;

	if (mode == PPE_DRV_EIP_FLOW_LOOKUP_MODE_TR) {
		lookup_mode = FAL_FLOW_EIP_LOOKUP_MODE_TRANSFORM;
	} else {
		lookup_mode = FAL_FLOW_EIP_LOOKUP_MODE_FLOW;
	}

	/*
	 * Set lookup mode to flow record lookup
	 */
	ret = fal_flow_eip_lookup_mode_set(dev_id, lookup_mode);
	if (ret != SW_OK) {
		ppe_drv_warn("Failed to set lookup mode to flow record lookup\n");
		return PPE_DRV_RET_EIP_LOOKUP_MODE_SET_FAIL;
	}

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_eip_set_lookup_mode);

/*
 * ppe_drv_eip_deinit()
 *	De-initialize EIP port.
 */
ppe_drv_ret_t ppe_drv_eip_deinit(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_port *port;

	spin_lock_bh(&p->lock);
	l3_if = ppe_drv_iface_l3_if_get(iface);
	if (!l3_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get l3_if from iface\n", iface);
		return PPE_DRV_RET_L3_IF_NOT_FOUND;
	}

	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface\n", iface);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/*
	 * Detach l3_if from port
	 * Dereference l3_if
	 * Dereference port
	 */
	ppe_drv_port_l3_if_detach(port, l3_if);
	port->port_l3_if = NULL;
	ppe_drv_l3_if_deref(l3_if);
	ppe_drv_port_deref(port);
	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_eip_deinit);

/*
 * ppe_drv_eip_init()
 *	Initialize the EIP port.
 */
ppe_drv_ret_t ppe_drv_eip_init(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_port *port;

	spin_lock_bh(&p->lock);
	port = ppe_drv_port_alloc(PPE_DRV_PORT_EIP, iface->dev, false, false);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get a valid port of type(%d)\n", iface, PPE_DRV_PORT_EIP);
		return PPE_DRV_RET_PORT_ALLOC_FAIL;
	}

	l3_if = ppe_drv_l3_if_alloc(PPE_DRV_L3_IF_TYPE_PORT);
	if (!l3_if) {
		ppe_drv_port_deref(port);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get a valid l3_if of type(%d)\n", iface, PPE_DRV_L3_IF_TYPE_PORT);
		return PPE_DRV_RET_L3_IF_ALLOC_FAIL;
	}

	/*
	 * Attach l3_if to port
	 */
	if (!ppe_drv_port_l3_if_attach(port, l3_if)) {
		ppe_drv_port_deref(port);
		ppe_drv_l3_if_deref(l3_if);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to attach valid l3_if(%p) to port(%p)\n", iface, l3_if, port);
		return PPE_DRV_RET_L3_IF_PORT_ATTACH_FAIL;
	}

	port->port_l3_if = l3_if;

	ppe_drv_iface_port_set(iface, port);
	ppe_drv_iface_l3_if_set(iface, l3_if);

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_eip_init);
