/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_TUN_PUBLIC_H_
#define _PPE_DRV_TUN_PUBLIC_H_

/**
 * ppe_drv_tun_add_ce_callback_t
 *	Register for tunnel outer rule entry addition callback.
 *
 * @param[in] vp_num Virtual port number
 * @param[in] vcreate_rule pointer to create_rule
 *
 * @return
 * Success or failure.
 */
typedef bool (*ppe_drv_tun_add_ce_callback_t)(uint8_t vp_num, void *vcreate_rule);

/**
 * ppe_drv_tun_del_ce_callback_t
 *	Register for tunnel outer rule entry deletion callback.
 *
 * @param[in] vp_num Virtual port number
 * @param[in] vdestroy_rule pointer to destroy_rule
 * @param[in] is_flush true if trigger is from exception/flush
 *
 * @return
 * Success or failure.
 */
typedef bool (*ppe_drv_tun_del_ce_callback_t)(uint8_t vp_num, void *vdestroy_rule, bool is_flush);

/**
 * ppe_drv_tun_configure
 *	Allocate a tunnel context in ppe
 *
 * @param[in] port_num VP port number
 * @param[in] add_cb registered callback for tunnel entry addition
 * @param[in] del_cb registered callback for tunnel entry deletion
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_configure(uint16_t port_num, struct ppe_drv_tun_cmn_ctx *pth, void *add_cb, void *del_cb);

/**
 * ppe_drv_tun_configure_vxlan_and_dport
 *	Configure VXLAN destination port.
 *
 * @param[in] dport VXLAN destination port number
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_configure_vxlan_and_dport(uint16_t dport);

/**
 * ppe_drv_tun_configure_vxlan_gpe_and_dport
 *	Configure VXLAN-GPE destination port.
 *
 * @param[in] dport VXLAN-GPE destination port number
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_configure_vxlan_gpe_and_dport(uint16_t dport);

/**
 * ppe_drv_tun_deactivate
 *	deactivate a tunnel context in ppe
 *
 * @param[in] port_num VP port number
 * @param[in] vdestroy_rule pointer to connection entry
 * @param[in] is_flush true if trigger is from exception/flush
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_deactivate(uint16_t port_num, void *vdestroy_rule, bool is_flush);

/**
 * ppe_drv_tun_activate
 *	Activate a tunnel context in ppe
 *
 * @param[in] port_num VP port number
 * @param[in] vcreate_rule pointer to connection entry
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_activate(uint16_t port_num, void *vcreate_rule);

/**
 * ppe_drv_tun_deconfigure
 *	deconfigure tunnel entry encapsulation/decapsulation in ppe
 *
 * @param[in] port_num VP port number
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_deconfigure(uint16_t port_num);

/**
 * ppe_drv_tun_decap_disable_by_port_num
 *	disable decapsulation in ppe
 *
 * @param[in] port_num VP port number
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_decap_disable_by_port_num(uint16_t port_num);

/**
 * ppe_drv_tun_decap_enable_by_port_num
 *	enable decapsulation in ppe
 *
 * @param[in] port_num VP port number
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_decap_enable_by_port_num(uint16_t port_num);

/*
 * ppe_drv_tun_l2tp_port_set
 *      Set L2TP source and destination port.
 *
 * @param[in] sport Source port value
 * @param[in] dport Destination port value
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_l2tp_port_set(uint16_t sport, uint16_t dport);

/*
 * ppe_drv_tun_l2tp_port_get
 *      Get l2tp source and destination port configured.
 *
 * @param[in] sport Pointer to store source port
 * @param[in] dport Pointer to store destination port
 *
 * @return
 * Success or failure.
 */
bool ppe_drv_tun_l2tp_port_get(uint16_t *sport, uint16_t *dport);

/*
 * ppe_drv_tun_header_length_get
 *      Get encap header length for a tunnel.
 *
 * @param[in] port_num tunnel VP port number
 * @param[out] tunnel header length
 *
 * @return
 * Success or failure.
 * Note: Tunnel needs to be in active state prior calling this function
 */
bool ppe_drv_tun_header_length_get(uint16_t port_num, uint8_t *hdr_len);

/*
 * ppe_drv_tun_xmit_port_mtu_get
 *      Get Tunnel xmit port mtu.
 *
 * @param[in] port_num tunnel VP port number
 * @param[out] mtu xmit port mtu value
 *
 * @return
 * Success or failure.
 * Note: Tunnel needs to be in active state prior calling this function
 */
bool ppe_drv_tun_xmit_port_mtu_get(uint16_t port_num, uint32_t *mtu);

/*
 * ppe_drv_tun_is_active
 *      Check if the hardware tunnel VP is still in the active state.
 *
 * @param[in] port_num tunnel VP port number
 *
 * @return
 * True or false.
 */
bool ppe_drv_tun_is_active(uint16_t port_num);
#endif /* _PPE_DRV_TUN_PUBLIC_H_ */
