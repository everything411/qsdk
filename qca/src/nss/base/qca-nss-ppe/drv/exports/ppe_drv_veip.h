/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_VEIP_H_
#define _PPE_DRV_VEIP_H_

#include "ppe_drv_port.h"

struct ppe_drv_iface;
struct net_device;
enum ppe_drv_port_type;

/**
 * struct ppe_drv_veip_vp_info - VP information for VEIP
 */
struct ppe_drv_veip_vp_info {
	uint8_t gw_vp_num;		/**< GW VP port number. */
	uint8_t pon_vp_num;		/**< PON VP port number. */
};

/**
 * ppe_drv_veip_init
 *      Allocate and initialize VEIP interface with both VP ports.
 *
 * @datatypes
 * net_device
 * ppe_drv_veip_vp_info
 *
 * @param[in] dev       VEIP net device.
 * @param[in] base_dev  Base net device.
 * @param[out] vp_info  VP port information (GW and PON port numbers).
 *
 * @return
 *      Pointer to PPE interface on success, NULL on failure.
 */
ppe_drv_ret_t ppe_drv_veip_init(struct ppe_drv_iface *iface, struct net_device *base_dev,
				struct ppe_drv_veip_vp_info *vp_info);

/**
 * ppe_drv_veip_deinit
 *      Deinitialize and free the VEIP interface along with all resources
 *      allocated during initialization.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface
 *      Pointer to the VEIP PPE interface instance to be deinitialized.
 *      Must be a valid interface initialized via ppe_drv_veip_init().
 *
 * @return
 *      None.
 */
void ppe_drv_veip_deinit(struct ppe_drv_iface *iface);

/**
 * ppe_drv_veip_get_port
 * 	Get VEIP VP ports of a specific type (PON or GW) from the interface.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_port_type
 *
 * @param[in] iface VEIP interface.
 * @param[in] type  Port type (GW/PON).
 *
 * @return
 * 	Port number on success, or PPE_DRV_INVALID_PORT on failure.
 */
extern int32_t ppe_drv_veip_get_port(struct ppe_drv_iface *iface, enum ppe_drv_port_type type);

/**
 * ppe_drv_veip_is_hgu_rule_valid - Validate if HGU rules can be applied on VEIP.
 *
 * This API checks if the VEIP interface has the required configuration to support
 * HGU (Home Gateway Unit) mode rule creation. It validates interface mode, port
 * association, and any other prerequisites before HGU VLAN rules are installed
 * by callers.
 *
 * @datatypes
 *  ppe_drv_iface
 *
 * @param[in] iface   VEIP interface to be validated.
 *
 * @return
 *  bool              true  - HGU rule configuration is valid and allowed.
 *                    false - HGU rule configuration is not valid.
 */
bool ppe_drv_veip_is_hgu_rule_valid(struct ppe_drv_iface *iface);

/**
 * ppe_drv_veip_is_enabled - Check if VEIP interface is enabled.
 *
 * This API verifies whether the given VEIP network device is enabled
 * and operational. It checks the device state to determine if VEIP
 * functionality is active before proceeding with any VEIP-specific
 * configuration or rule installation.
 *
 * @datatypes
 *  net_device
 *
 * @param[in] dev     Network device corresponding to the VEIP interface.
 *
 * @return
 *  bool              true  - VEIP interface is enabled and active.
 *                    false - VEIP interface is disabled or not available.
 */
bool ppe_drv_veip_is_enabled(struct net_device *dev);
#endif /* _PPE_DRV_VEIP_H_ */
