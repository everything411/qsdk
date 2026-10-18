/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv_dp.h
 *	NSS PPE DP definitions.
 */

#ifndef _PPE_DRV_DP_H_
#define _PPE_DRV_DP_H_

#include <linux/netdevice.h>
#include "ppe_drv_port.h"

/**
 * @addtogroup ppe_drv_dp_subsystem
 * @{
 */

/**
 * enum ppe_drv_dp_mirror_direction
 *	PPE mirror directions
 */
typedef enum ppe_drv_dp_mirror_direction {
	PPE_DRV_DP_MIRR_DI_IN = 0,	/**< PPE mirror direction ingress */
	PPE_DRV_DP_MIRR_DI_EG,		/**< PPE mirror direction egress */
} ppe_drv_dp_mirror_direction_t;

/*
 * Default mirror analysis port priority
 */
#define PPE_DRV_MIRR_ANALYSIS_PRI 0

/**
 * ppe_drv_dp_deinit
 *	Deinitialize DP.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface   Pointer to the PPE interface for bridge.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_deinit(struct ppe_drv_iface *iface);

/**
 * ppe_drv_dp_init
 *	Initialize DP.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface   Pointer to the PPE interface for bridge.
 * @param[in] macid   MAC ID of the port.
 * @param[in] mht_dev MHT SWT device ports mapping flag.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_init(struct ppe_drv_iface *iface, uint32_t macid,
				bool mht_dev);

/**
 * ppe_drv_dp_set_mirror_if
 *	Set enable/disable ingress/egress port mirroring
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_dp_mirror_direction_t mirror direction
 * bool
 *
 * @param[in] iface       Pointer to the PPE interface.
 * @param[in] direction   Mirror direction (ingress/egress).
 * @param[in] enable      Enable/Disable mirroring.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_set_mirror_if(struct ppe_drv_iface *iface,
		ppe_drv_dp_mirror_direction_t direction, bool enable);

/**
 * ppe_drv_dp_set_mirr_analysis_port
 *	Set mirror analysis port
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_dp_mirror_direction_t mirror direction
 * bool
 *
 * @param[in] iface         Pointer to the PPE interface.
 * @param[in] direction     Mirror analysis port direction
 * @param[in] enable        True for enable and false for disable analysis port
 * @param[in] priority      Analysis port priority
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_set_mirr_analysis_port(struct ppe_drv_iface *iface,
		ppe_drv_dp_mirror_direction_t direction, bool enable, uint8_t priority);

/**
 * ppe_drv_dp_get_mirr_analysis_port
 *	Get mirror analysis port number
 *
 * @datatypes
 * ppe_drv_dp_mirror_direction_t mirror direction
 *
 * @param[in] direction     Mirror analysis port direction
 * @param[out] port_num     Mirror analysis port number
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_get_mirr_analysis_port(
		ppe_drv_dp_mirror_direction_t direction, uint8_t *port_num);

/**
 * ppe_drv_dp_set_ppe_offload_enable_flag
 *	API to set PPE offload enable flag in PPE port
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface         Pointer to the PPE interface.
 * @param[in] disable       True if PPE offload is disbled, otherwise false
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_set_ppe_offload_enable_flag(struct ppe_drv_iface *iface,
				bool disable);
/**
 * ppe_drv_dp_gem_enable
 *      API to set GEM type enable flag in PPE port
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface	Pointer to the PPE interface.
 * @param[in] enable	True if GEM type is enabled, otherwise false
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_dp_gem_enable(struct ppe_drv_iface *iface,
                bool enable);

/**
 * ppe_drv_dp_get_pon_stats
 *	Fill stats with the 64-bit accumulated PON upstream port counters.
 *
 * WAR: Read PPE port statistics instead of MIB for PON port.
 * Note: This will cover only packet and byte counters.
 *
 * @param[out] stats   Pointer to ppe_drv_port_hw_stats to fill.
 *
 * @return
 * None.
 */
void ppe_drv_dp_get_pon_stats(struct ppe_drv_port_hw_stats *stats);

/**
 * ppe_drv_dp_htt_enable
 *	Configure the software status of the HTT (High Throughput Transport)
 *	device associated with the given interface.
 *
 * This API updates the internal driver state to indicate whether the
 * interface is connected to or operating as an HTT device.
 *
 * @param[in] iface    Pointer to the PPE driver interface.
 * @param[in] htt_dev  Boolean flag indicating HTT status:
 *                    - true  : Interface is an HTT device
 *                    - false : Interface is not an HTT device
 *
 * @return
 * PPE driver return status:
 *  - PPE_DRV_RET_SUCCESS on successful update
 *  - Error code otherwise
 */
ppe_drv_ret_t ppe_drv_dp_htt_enable(struct ppe_drv_iface *iface, bool htt_dev);
/** @} */ /* end_addtogroup ppe_drv_dp_subsystem */
#endif /* _PPE_DRV_DP_H_ */
