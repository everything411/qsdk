/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ISISC_PORT_CTRL_H_
#define _ISISC_PORT_CTRL_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_port_ctrl.h"

#if defined(MHT)
/* the wakeup timer unit is us */
#define PORT_LPI_WAKEUP_TIMER_2500M	0x58
#define PORT_LPI_WAKEUP_TIMER_1000M	0x24

sw_error_t mht_port_mac_eee_timer_adjust(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t speed, struct qca_phy_priv *priv);
#endif
    sw_error_t isisc_port_ctrl_init(a_uint32_t dev_id);

#ifdef IN_PORTCONTROL
#define ISISC_PORT_CTRL_INIT(rv, dev_id) \
    { \
        rv = isisc_port_ctrl_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
#define ISISC_PORT_CTRL_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


    HSL_LOCAL sw_error_t
    isisc_port_duplex_set(a_uint32_t dev_id, fal_port_t port_id,
                         fal_port_duplex_t duplex);


    HSL_LOCAL sw_error_t
    isisc_port_duplex_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_port_duplex_t * pduplex);


    HSL_LOCAL sw_error_t
    isisc_port_speed_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_port_speed_t speed);


    HSL_LOCAL sw_error_t
    isisc_port_speed_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_port_speed_t * pspeed);


    HSL_LOCAL sw_error_t
    isisc_port_flowctrl_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_flowctrl_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_flowctrl_forcemode_set(a_uint32_t dev_id,
                                     fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_flowctrl_forcemode_get(a_uint32_t dev_id,
                                     fal_port_t port_id,
                                     a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_rxhdr_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                             fal_port_header_mode_t mode);


    HSL_LOCAL sw_error_t
    isisc_port_rxhdr_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                             fal_port_header_mode_t * mode);


    HSL_LOCAL sw_error_t
    isisc_port_txhdr_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                             fal_port_header_mode_t mode);


    HSL_LOCAL sw_error_t
    isisc_port_txhdr_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                             fal_port_header_mode_t * mode);


    HSL_LOCAL sw_error_t
    isisc_header_type_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t type);


    HSL_LOCAL sw_error_t
    isisc_header_type_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * type);


    HSL_LOCAL sw_error_t
    isisc_port_txmac_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_txmac_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_rxmac_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_rxmac_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_bp_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_bp_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_link_forcemode_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_link_forcemode_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    HSL_LOCAL sw_error_t
    isisc_port_link_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * status);

    HSL_LOCAL sw_error_t
    isisc_ports_link_status_get(a_uint32_t dev_id, a_uint32_t * status);

    HSL_LOCAL sw_error_t
    isisc_port_mac_loopback_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    HSL_LOCAL sw_error_t
    isisc_port_mac_loopback_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);
#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISISC_PORT_CTRL_H_ */

