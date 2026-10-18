/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/*qca808x_start*/
#ifndef _SCOMPHY_PORT_CTRL_H_
#define _SCOMPHY_PORT_CTRL_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#include "fal/fal_port_ctrl.h"

sw_error_t scomphy_port_ctrl_init (a_uint32_t dev_id);

/*qca808x_end*/
#ifdef IN_PORTCONTROL
/*qca808x_start*/
#define SCOMPHY_PORT_CTRL_INIT(rv, dev_id) \
    { \
        rv = scomphy_port_ctrl_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
/*qca808x_end*/
#else
#define SCOMPHY_PORT_CTRL_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
scomphy_port_duplex_set (a_uint32_t dev_id, fal_port_t port_id,
		fal_port_duplex_t duplex);


HSL_LOCAL sw_error_t
scomphy_port_duplex_get (a_uint32_t dev_id, fal_port_t port_id,
		fal_port_duplex_t * pduplex);


HSL_LOCAL sw_error_t
scomphy_port_speed_set (a_uint32_t dev_id, fal_port_t port_id,
		fal_port_speed_t speed);


HSL_LOCAL sw_error_t
scomphy_port_speed_get (a_uint32_t dev_id, fal_port_t port_id,
		fal_port_speed_t * pspeed);

HSL_LOCAL sw_error_t
scomphy_port_link_status_get (a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t * status);

HSL_LOCAL sw_error_t
scomphy_port_combo_prefer_medium_set (a_uint32_t dev_id, a_uint32_t phy_id,
		fal_port_medium_t phy_medium);

HSL_LOCAL sw_error_t
scomphy_port_combo_prefer_medium_get (a_uint32_t dev_id, a_uint32_t phy_id,
		fal_port_medium_t * phy_medium);

HSL_LOCAL sw_error_t
scomphy_port_interface_mode_set (a_uint32_t dev_id, fal_port_t port_id,
			fal_port_interface_mode_t mode);

HSL_LOCAL sw_error_t
scomphy_port_interface_mode_get (a_uint32_t dev_id, fal_port_t port_id,
		fal_port_interface_mode_t * mode);

HSL_LOCAL sw_error_t
scomphy_port_interface_mode_status_get (a_uint32_t dev_id, fal_port_t port_id,
		fal_port_interface_mode_t * mode);
#endif
/*qca808x_start*/
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _SCOMPHY_PORT_CTRL_H_ */
/*qca808x_end*/
