/*
 * Copyright (c) 2012, 2015-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*qca808x_start*/
/**
 * @defgroup fal_port_ctrl FAL_PORT_CONTROL
 * @{
 */
#include "sw.h"
#include "fal_port_ctrl.h"
#include "hsl_phy.h"
#include "hsl_api.h"
/*qca808x_end*/
#include "adpt.h"
#include "ssdk_dts.h"
#include <linux/kernel.h>
#include <linux/module.h>
/*qca808x_start*/
#include "hsl_phy.h"

sw_error_t fal_port_duplex_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_duplex_t duplex)
    DEFINE_FAL_PORT_ADPT_HSL_PHY_FUNC(duplex_set, dev_id, port_id, duplex)

sw_error_t fal_port_speed_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_speed_t speed)
    DEFINE_FAL_PORT_ADPT_HSL_PHY_FUNC(speed_set, dev_id, port_id, speed)

sw_error_t fal_port_duplex_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_duplex_t * pduplex)
    DEFINE_FAL_FUNC_EXPORT(port_duplex_get, dev_id, port_id, pduplex)

sw_error_t fal_port_speed_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_speed_t * pspeed)
    DEFINE_FAL_FUNC_EXPORT(port_speed_get, dev_id, port_id, pspeed)

sw_error_t fal_port_hdr_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_hdr_status_set, dev_id, port_id, enable)

sw_error_t fal_port_hdr_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_hdr_status_get, dev_id, port_id, enable)

sw_error_t fal_port_rxhdr_mode_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_header_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_rxhdr_mode_get, dev_id, port_id, mode)

sw_error_t fal_port_txhdr_mode_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_header_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_txhdr_mode_get, dev_id, port_id, mode)

sw_error_t fal_header_type_get (a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * type)
    DEFINE_FAL_FUNC_HSL_EXPORT(header_type_get, dev_id, enable, type)

sw_error_t fal_port_flowctrl_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_flowctrl_get, dev_id, port_id, enable)

sw_error_t fal_port_flowctrl_forcemode_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_flowctrl_forcemode_get, dev_id, port_id, enable)

sw_error_t fal_port_flowctrl_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_flowctrl_set, dev_id, port_id, enable)

sw_error_t fal_port_flowctrl_forcemode_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_flowctrl_forcemode_set, dev_id, port_id, enable)

sw_error_t fal_port_rxhdr_mode_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_header_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_rxhdr_mode_set, dev_id, port_id, mode)

sw_error_t fal_port_txhdr_mode_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_header_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_txhdr_mode_set, dev_id, port_id, mode)

sw_error_t fal_header_type_set (a_uint32_t dev_id, a_bool_t enable, a_uint32_t type)
    DEFINE_FAL_FUNC_HSL_EXPORT(header_type_set, dev_id, enable, type)

sw_error_t fal_port_txmac_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_txmac_status_set, dev_id, port_id, enable)

sw_error_t fal_port_rxmac_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_rxmac_status_set, dev_id, port_id, enable)

sw_error_t fal_port_txfc_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_txfc_status_set, dev_id, port_id, enable)

sw_error_t fal_port_rxfc_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_rxfc_status_set, dev_id, port_id, enable)

sw_error_t fal_port_rxfc_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_rxfc_status_get, dev_id, port_id, enable)

sw_error_t fal_port_txfc_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_txfc_status_get, dev_id, port_id, enable)

sw_error_t fal_port_link_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * status)
    DEFINE_FAL_FUNC_EXPORT(port_link_status_get, dev_id, port_id, status)

sw_error_t fal_port_link_forcemode_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_link_forcemode_set, dev_id, port_id, enable)

sw_error_t fal_port_link_forcemode_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_link_forcemode_get, dev_id, port_id, enable)

sw_error_t fal_port_combo_prefer_medium_set (a_uint32_t dev_id, a_uint32_t port_id, fal_port_medium_t medium)
    DEFINE_FAL_FUNC_EXPORT(port_combo_prefer_medium_set, dev_id, port_id, medium)

sw_error_t fal_port_combo_prefer_medium_get (a_uint32_t dev_id, a_uint32_t port_id, fal_port_medium_t * medium)
    DEFINE_FAL_FUNC_EXPORT(port_combo_prefer_medium_get, dev_id, port_id, medium)

sw_error_t fal_port_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t max_frame)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_max_frame_size_set, dev_id, port_id, max_frame)

sw_error_t fal_port_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *max_frame)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_max_frame_size_get, dev_id, port_id, max_frame)

sw_error_t fal_port_mru_set(a_uint32_t dev_id, fal_port_t port_id, fal_mru_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mru_set, dev_id, port_id, ctrl)

sw_error_t fal_port_mru_get(a_uint32_t dev_id, fal_port_t port_id, fal_mru_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mru_get, dev_id, port_id, ctrl)

sw_error_t fal_port_mtu_set(a_uint32_t dev_id, fal_port_t port_id, fal_mtu_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mtu_set, dev_id, port_id, ctrl)

sw_error_t fal_port_mtu_get(a_uint32_t dev_id, fal_port_t port_id, fal_mtu_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mtu_get, dev_id, port_id, ctrl)

sw_error_t fal_port_mtu_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_mtu_cfg_t *mtu_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mtu_cfg_set, dev_id, port_id, mtu_cfg)

sw_error_t fal_port_mtu_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_mtu_cfg_t *mtu_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mtu_cfg_get, dev_id, port_id, mtu_cfg)

sw_error_t fal_port_mru_mtu_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t mru_size, a_uint32_t mtu_size)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mru_mtu_set, dev_id, port_id, mru_size, mtu_size)

sw_error_t fal_port_mru_mtu_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *mru_size, a_uint32_t *mtu_size)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mru_mtu_get, dev_id, port_id, mru_size, mtu_size)

sw_error_t fal_port_source_filter_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_ADPT(port_source_filter_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_source_filter_status_get);

sw_error_t fal_port_source_filter_enable(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT(port_source_filter_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_source_filter_enable);

sw_error_t fal_port_source_filter_config_set(a_uint32_t dev_id, fal_port_t port_id, fal_src_filter_config_t *src_filter_config)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_source_filter_config_set, dev_id, port_id, src_filter_config)

sw_error_t fal_port_source_filter_config_get(a_uint32_t dev_id, fal_port_t port_id, fal_src_filter_config_t *src_filter_config)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_source_filter_config_get, dev_id, port_id, src_filter_config)

sw_error_t fal_port_promisc_mode_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_promisc_mode_get, dev_id, port_id, enable)

sw_error_t fal_port_promisc_mode_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_promisc_mode_set, dev_id, port_id, enable)

sw_error_t fal_port_interface_eee_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_eee_cfg_t *port_eee_cfg)
    DEFINE_FAL_FUNC_EXPORT(port_interface_eee_cfg_set, dev_id, port_id, port_eee_cfg)

sw_error_t fal_port_interface_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_eee_cfg_t *port_eee_cfg)
    DEFINE_FAL_FUNC_EXPORT(port_interface_eee_cfg_get, dev_id, port_id, port_eee_cfg)

sw_error_t fal_switch_port_loopback_set(a_uint32_t dev_id, fal_port_t port_id, fal_loopback_config_t *loopback_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(switch_port_loopback_set, dev_id, port_id, loopback_cfg)

sw_error_t fal_switch_port_loopback_get(a_uint32_t dev_id, fal_port_t port_id, fal_loopback_config_t *loopback_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(switch_port_loopback_get, dev_id, port_id, loopback_cfg)

sw_error_t fal_port_flow_ctrl_thres_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint16_t on_thres, a_uint16_t off_thres)
    DEFINE_FAL_FUNC_ADPT_HSL(port_tx_buff_thresh_set, port_flowctrl_thresh_set, dev_id, port_id, on_thres, off_thres)
    EXPORT_SYMBOL(fal_port_flow_ctrl_thres_set);

sw_error_t fal_port_flow_ctrl_thres_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint16_t *on_thres, a_uint16_t *off_thres)
    DEFINE_FAL_FUNC_ADPT_HSL(port_tx_buff_thresh_get, port_flowctrl_thresh_get, dev_id, port_id, on_thres, off_thres)
    EXPORT_SYMBOL(fal_port_flow_ctrl_thres_get);

sw_error_t fal_port_rx_fifo_thres_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint16_t thres)
    DEFINE_FAL_FUNC_ADPT(port_rx_buff_thresh_set, dev_id, port_id, thres)
    EXPORT_SYMBOL(fal_port_rx_fifo_thres_set);

sw_error_t fal_port_cnt_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_cnt_cfg_set, dev_id, port_id, cnt_cfg)

sw_error_t fal_port_cnt_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_cnt_cfg_get, dev_id, port_id, cnt_cfg)

sw_error_t fal_port_cnt_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_t *port_cnt)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_cnt_get, dev_id, port_id, port_cnt)

sw_error_t fal_port_cnt_flush(a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_cnt_flush, dev_id, port_id)

sw_error_t fal_port_erp_power_mode_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_erp_power_mode_t power_mode)
    DEFINE_FAL_FUNC_EXPORT(port_erp_power_mode_set, dev_id, port_id, power_mode)

sw_error_t fal_port_autoneg_enable (a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_PORT_PHY_FUNC(autoneg_enable, dev_id, port_id)
    EXPORT_SYMBOL(fal_port_autoneg_enable);

sw_error_t fal_port_autoneg_restart (a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_PORT_PHY_FUNC(autoneg_restart, dev_id, port_id)
    EXPORT_SYMBOL(fal_port_autoneg_restart);

sw_error_t fal_port_autoneg_adv_set (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t autoadv)
    DEFINE_FAL_PORT_PHY_FUNC(autoadv_set, dev_id, port_id, autoadv)
    EXPORT_SYMBOL(fal_port_autoneg_adv_set);

sw_error_t fal_port_autoneg_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * status)
    DEFINE_FAL_PORT_PHY_FUNC(autoneg_status_get, dev_id, port_id, status)
    EXPORT_SYMBOL(fal_port_autoneg_status_get);

sw_error_t fal_port_autoneg_adv_get (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * autoadv)
    DEFINE_FAL_PORT_PHY_FUNC(autoadv_get, dev_id, port_id, autoadv)
    EXPORT_SYMBOL(fal_port_autoneg_adv_get);

sw_error_t fal_port_cdt (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t mdi_pair, fal_cable_status_t * cable_status, a_uint32_t * cable_len)
    DEFINE_FAL_PORT_PHY_FUNC(cdt, dev_id, port_id, mdi_pair, (void*)cable_status, cable_len)
    EXPORT_SYMBOL(fal_port_cdt);

sw_error_t fal_port_power_off (a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_PORT_PHY_FUNC(power_off, dev_id, port_id)
    EXPORT_SYMBOL(fal_port_power_off);

sw_error_t fal_port_power_on (a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_PORT_PHY_FUNC(power_on, dev_id, port_id)
    EXPORT_SYMBOL(fal_port_power_on);

sw_error_t fal_port_combo_link_status_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_combo_link_status_t * status)
    DEFINE_FAL_FUNC_HSL_DIRECT(port_combo_phy_link_status_get, dev_id, port_id, status)
    EXPORT_SYMBOL(fal_port_combo_link_status_get);

#ifndef IN_PORTCONTROL_MINI
sw_error_t fal_port_txmac_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_txmac_status_get, dev_id, port_id, enable)

sw_error_t fal_port_rxmac_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_rxmac_status_get, dev_id, port_id, enable)

sw_error_t fal_port_congestion_drop_set (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t queue_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_congestion_drop_set, dev_id, port_id, queue_id, enable)

sw_error_t fal_ring_flow_ctrl_thres_set (a_uint32_t dev_id, a_uint32_t ring_id, a_uint16_t on_thres, a_uint16_t off_thres)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_flow_ctrl_thres_set, dev_id, ring_id, on_thres, off_thres)

sw_error_t fal_port_bp_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_bp_status_set, dev_id, port_id, enable)

sw_error_t fal_port_bp_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_bp_status_get, dev_id, port_id, enable)

sw_error_t fal_ports_link_status_get (a_uint32_t dev_id, a_uint32_t * status)
    DEFINE_FAL_FUNC_EXPORT(ports_link_status_get, dev_id, status)

sw_error_t fal_port_mac_loopback_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_EXPORT(port_mac_loopback_set, dev_id, port_id, enable)

sw_error_t fal_port_mac_loopback_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_EXPORT(port_mac_loopback_get, dev_id, port_id, enable)

sw_error_t fal_port_congestion_drop_get (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t queue_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_congestion_drop_get, dev_id, port_id, queue_id, enable)

sw_error_t fal_ring_flow_ctrl_thres_get (a_uint32_t dev_id, a_uint32_t ring_id, a_uint16_t * on_thres, a_uint16_t * off_thres)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_flow_ctrl_thres_get, dev_id, ring_id, on_thres, off_thres)

sw_error_t fal_port_interface_mode_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_interface_mode_t mode)
    DEFINE_FAL_FUNC_EXPORT(port_interface_mode_set, dev_id, port_id, mode)

sw_error_t fal_port_interface_mode_apply (a_uint32_t dev_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_interface_mode_apply, dev_id)

sw_error_t fal_port_interface_mode_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_interface_mode_t * mode)
    DEFINE_FAL_FUNC_EXPORT(port_interface_mode_get, dev_id, port_id, mode)

sw_error_t fal_port_interface_mode_status_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_interface_mode_t * mode)
    DEFINE_FAL_FUNC_EXPORT(port_interface_mode_status_get, dev_id, port_id, mode)

sw_error_t fal_port_8023ah_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_8023ah_ctrl_t *port_8023ah_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_8023ah_set, dev_id, port_id, port_8023ah_ctrl)

sw_error_t fal_port_8023ah_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_8023ah_ctrl_t *port_8023ah_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_8023ah_get, dev_id, port_id, port_8023ah_ctrl)

sw_error_t fal_ring_flow_ctrl_status_get(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t *status)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_flow_ctrl_status_get, dev_id, ring_id, status)

sw_error_t fal_ring_union_set(a_uint32_t dev_id, a_bool_t en)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_union_set, dev_id, en)

sw_error_t fal_ring_union_get(a_uint32_t dev_id, a_bool_t *en)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_union_get, dev_id, en)

sw_error_t fal_port_rx_fifo_thres_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint16_t *thres)
    DEFINE_FAL_FUNC_ADPT(port_rx_buff_thresh_get, dev_id, port_id, thres)
    EXPORT_SYMBOL(fal_port_rx_fifo_thres_get);

sw_error_t fal_ring_flow_ctrl_config_get(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t *status)
    DEFINE_FAL_FUNC_HSL(ring_flow_ctrl_get, dev_id, ring_id, status)
    EXPORT_SYMBOL(fal_ring_flow_ctrl_config_get);

sw_error_t fal_ring_flow_ctrl_config_set(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t status)
    DEFINE_FAL_FUNC_HSL(ring_flow_ctrl_set, dev_id, ring_id, status)
    EXPORT_SYMBOL(fal_ring_flow_ctrl_config_set);

sw_error_t fal_port_8023az_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(ieee_8023az_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_8023az_set);

sw_error_t fal_port_8023az_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_PORT_PHY_FUNC(ieee_8023az_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_8023az_get);

sw_error_t fal_port_mdix_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_mdix_mode_t mode)
    DEFINE_FAL_PORT_PHY_FUNC(mdix_set, dev_id, port_id, (a_uint32_t)mode)
    EXPORT_SYMBOL(fal_port_mdix_set);

sw_error_t fal_port_mdix_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_mdix_mode_t * mode)
    DEFINE_FAL_PORT_PHY_FUNC(mdix_get, dev_id, port_id, (void*)mode)
    EXPORT_SYMBOL(fal_port_mdix_get);

sw_error_t fal_port_mdix_status_get (a_uint32_t dev_id, fal_port_t port_id, fal_port_mdix_status_t * mode)
    DEFINE_FAL_PORT_PHY_FUNC(mdix_status_get, dev_id, port_id, (void*)mode)
    EXPORT_SYMBOL(fal_port_mdix_status_get);

sw_error_t fal_port_powersave_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(powersave_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_powersave_set);

sw_error_t fal_port_powersave_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_PORT_PHY_FUNC(powersave_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_powersave_get);

sw_error_t fal_port_hibernate_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(hibernation_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_hibernate_set);

sw_error_t fal_port_hibernate_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_PORT_PHY_FUNC(hibernation_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_hibernate_get);

sw_error_t fal_port_combo_medium_status_get (a_uint32_t dev_id, a_uint32_t port_id, fal_port_medium_t * medium)
    DEFINE_FAL_PORT_PHY_FUNC(combo_medium_status_get, dev_id, port_id, (void*)medium)
    EXPORT_SYMBOL(fal_port_combo_medium_status_get);

sw_error_t fal_port_combo_fiber_mode_set (a_uint32_t dev_id, a_uint32_t port_id, fal_port_fiber_mode_t mode)
    DEFINE_FAL_PORT_PHY_FUNC(combo_fiber_mode_set, dev_id, port_id, (a_uint32_t)mode)
    EXPORT_SYMBOL(fal_port_combo_fiber_mode_set);

sw_error_t fal_port_combo_fiber_mode_get (a_uint32_t dev_id, a_uint32_t port_id, fal_port_fiber_mode_t * mode)
    DEFINE_FAL_PORT_PHY_FUNC(combo_fiber_mode_get, dev_id, port_id, (void*)mode)
    EXPORT_SYMBOL(fal_port_combo_fiber_mode_get);

sw_error_t fal_port_local_loopback_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(local_loopback_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_local_loopback_set);

sw_error_t fal_port_local_loopback_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_PORT_PHY_FUNC(local_loopback_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_local_loopback_get);

sw_error_t fal_port_remote_loopback_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(remote_loopback_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_remote_loopback_set);

sw_error_t fal_port_remote_loopback_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_PORT_PHY_FUNC(remote_loopback_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_remote_loopback_get);

sw_error_t fal_port_reset (a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_PORT_PHY_FUNC(reset, dev_id, port_id)
    EXPORT_SYMBOL(fal_port_reset);

sw_error_t fal_port_phy_id_get (a_uint32_t dev_id, fal_port_t port_id, a_uint16_t * org_id, a_uint16_t * rev_id)
    DEFINE_FAL_PORT_PHY_FUNC(phyid_get, dev_id, port_id, org_id, rev_id)

    EXPORT_SYMBOL(fal_port_phy_id_get);

sw_error_t fal_port_wol_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(wol_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_wol_status_set);

sw_error_t fal_port_wol_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
    DEFINE_FAL_PORT_PHY_FUNC(wol_get , dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_wol_status_get);

sw_error_t fal_port_magic_frame_mac_set (a_uint32_t dev_id, fal_port_t port_id, fal_mac_addr_t * mac)
    DEFINE_FAL_PORT_PHY_FUNC(magic_frame_set, dev_id, port_id, (void*)mac)
    EXPORT_SYMBOL(fal_port_magic_frame_mac_set);

sw_error_t fal_port_magic_frame_mac_get (a_uint32_t dev_id, fal_port_t port_id, fal_mac_addr_t * mac)
    DEFINE_FAL_PORT_PHY_FUNC(magic_frame_get, dev_id, port_id, (void*)mac)
    EXPORT_SYMBOL(fal_port_magic_frame_mac_get);

sw_error_t fal_debug_phycounter_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_PORT_PHY_FUNC(stats_status_set , dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_debug_phycounter_set);

sw_error_t fal_debug_phycounter_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_PORT_PHY_FUNC(stats_status_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_debug_phycounter_get);

sw_error_t fal_debug_phycounter_show (a_uint32_t dev_id, fal_port_t port_id, fal_port_counter_info_t* port_counter_info)
    DEFINE_FAL_PORT_PHY_FUNC(stats_get, dev_id, port_id, (void*)port_counter_info)
    EXPORT_SYMBOL(fal_debug_phycounter_show);

static sw_error_t
_fal_port_interface_eee_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
    hsl_api_t *p_api = NULL;
    adpt_api_t *p_adpt_api = NULL;

    if((p_adpt_api = adpt_api_ptr_get(dev_id)) != NULL &&
        p_adpt_api->adpt_port_interface_eee_cfg_set != NULL) {
        return p_adpt_api->adpt_port_interface_eee_cfg_set(dev_id, port_id, port_eee_cfg);
    }

  SW_RTN_ON_NULL(p_api = hsl_api_ptr_get (dev_id));
  if (NULL == p_api->port_interface_eee_cfg_set)
    return SW_NOT_SUPPORTED;

  return p_api->port_interface_eee_cfg_set(dev_id, port_id, port_eee_cfg);
}

static sw_error_t
_fal_port_interface_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	hsl_api_t *p_api = NULL;
	adpt_api_t *p_adpt_api = NULL;

	if((p_adpt_api = adpt_api_ptr_get(dev_id)) != NULL &&
		p_adpt_api->adpt_port_interface_eee_cfg_get != NULL) {
			return p_adpt_api->adpt_port_interface_eee_cfg_get(dev_id, port_id,
				port_eee_cfg);
	}

	SW_RTN_ON_NULL(p_api = hsl_api_ptr_get (dev_id));
	if (NULL == p_api->port_interface_eee_cfg_get)
		return SW_NOT_SUPPORTED;

	return p_api->port_interface_eee_cfg_get(dev_id, port_id, port_eee_cfg);
}

static sw_error_t
_fal_port_interface_3az_status_set(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	fal_port_eee_cfg_t port_eee_cfg = {0};

	rv = _fal_port_interface_eee_cfg_get(dev_id, port_id, &port_eee_cfg);
	SW_RTN_ON_ERROR(rv);
	port_eee_cfg.enable = enable;
	if(enable)
		port_eee_cfg.advertisement = FAL_PHY_EEE_ALL_ADV;
	port_eee_cfg.lpi_tx_enable = enable;
	rv = _fal_port_interface_eee_cfg_set(dev_id, port_id, &port_eee_cfg);

	return rv;
}

static sw_error_t
_fal_port_interface_3az_status_get(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	fal_port_eee_cfg_t port_eee_cfg = {0};

	rv = _fal_port_interface_eee_cfg_get(dev_id, port_id, &port_eee_cfg);
	SW_RTN_ON_ERROR(rv);
	*enable = (port_eee_cfg.enable && port_eee_cfg.lpi_tx_enable);

	return SW_OK;
}

sw_error_t
fal_port_interface_3az_status_set(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_interface_3az_status_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}
sw_error_t
fal_port_interface_3az_status_get(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_interface_3az_status_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}
	
EXPORT_SYMBOL(fal_port_interface_3az_status_set);
EXPORT_SYMBOL(fal_port_interface_3az_status_get);
#endif

static sw_error_t
_fal_port_erp_power_mode_get (a_uint32_t dev_id, fal_port_t port_id,
				fal_port_erp_power_mode_t *power_mode)
{
	if (hsl_port_feature_get(dev_id, port_id, PHY_F_ERP_LOW_POWER))
		*power_mode = FAL_ERP_LOW_POWER;
	else
		*power_mode = FAL_ERP_ACTIVE;

	return SW_OK;
}

sw_error_t
fal_port_erp_power_mode_get (a_uint32_t dev_id, fal_port_t port_id,
				fal_port_erp_power_mode_t *power_mode)
{
	sw_error_t rv;

	FAL_API_LOCK;
	rv = _fal_port_erp_power_mode_get (dev_id, port_id, power_mode);
	FAL_API_UNLOCK;
	return rv;
}

sw_error_t
fal_erp_standby_enter (a_uint32_t dev_id, a_uint32_t active_pbmap)
{
	a_uint32_t port_id = 0, pbmp = 0;
	pbmp = ssdk_wan_bmp_get(dev_id) | ssdk_lan_bmp_get(dev_id);
	while (pbmp) {
		if (pbmp & 1) {
			if (!SW_IS_PBMP_MEMBER(active_pbmap, port_id)) {
				if (!hsl_port_feature_get(dev_id, port_id, PHY_F_FORCE)) {
					SW_RTN_ON_ERROR(fal_port_erp_power_mode_set(dev_id,
						port_id, FAL_ERP_LOW_POWER));
				}
			}
		}
		pbmp >>= 1;
		port_id ++;
	}
	return SW_OK;
}

sw_error_t
fal_erp_standby_exit (a_uint32_t dev_id)
{
	a_uint32_t port_id = 0, pbmp = 0;
	pbmp = ssdk_wan_bmp_get(dev_id) | ssdk_lan_bmp_get(dev_id);
	while (pbmp) {
		if (pbmp & 1) {
			if (hsl_port_feature_get(dev_id, port_id, PHY_F_ERP_LOW_POWER)) {
				SW_RTN_ON_ERROR(fal_port_erp_power_mode_set(dev_id,
					port_id, FAL_ERP_ACTIVE));
			}
		}
		pbmp >>= 1;
		port_id ++;
	}
	return SW_OK;
}

EXPORT_SYMBOL(fal_port_erp_power_mode_get);
EXPORT_SYMBOL(fal_erp_standby_enter);
EXPORT_SYMBOL(fal_erp_standby_exit);

