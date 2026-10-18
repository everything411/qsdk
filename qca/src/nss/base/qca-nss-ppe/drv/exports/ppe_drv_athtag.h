/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_ATHTAG_H_
#define _PPE_DRV_ATHTAG_H_

struct ppe_drv_iface;

/**
 * ppe_drv_athtag_del_vp_mapping
 *	Del ath mapping between switch port and PPE VP.
 *
 * @datatypes
 * ppe_drv_iface
 * unsigned int
 *
 * @param[in] iface PPE interface for DSA netdev.
 * @param[in] swpt_id switch port id.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_athtag_del_vp_mapping(struct ppe_drv_iface *iface, unsigned int swpt_id);

/**
 * ppe_drv_athtag_add_vp_mapping
 *	Add ath mapping between switch port and PPE VP.
 *
 * @datatypes
 * ppe_drv_iface
 * unsigned int
 *
 * @param[in] iface PPE interface for DSA netdev.
 * @param[in] swpt_id switch port id.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_athtag_add_vp_mapping(struct ppe_drv_iface *iface, unsigned int swpt_id);
#endif /* _PPE_DRV_ATHTAG_H_ */
