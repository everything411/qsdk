/*
 * Copyright (c) 2012, 2015-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_port_ctrl_legacy FAL_PORT_CONTROL_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_port_ctrl_legacy.h"
#include "hsl_api.h"
#include "adpt.h"
#include <linux/kernel.h>
#include <linux/module.h>

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

sw_error_t fal_port_rxhdr_mode_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_header_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_rxhdr_mode_set, dev_id, port_id, mode)

sw_error_t fal_port_txhdr_mode_set (a_uint32_t dev_id, fal_port_t port_id, fal_port_header_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_txhdr_mode_set, dev_id, port_id, mode)

sw_error_t fal_header_type_set (a_uint32_t dev_id, a_bool_t enable, a_uint32_t type)
    DEFINE_FAL_FUNC_HSL_EXPORT(header_type_set, dev_id, enable, type)

sw_error_t fal_port_link_forcemode_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_link_forcemode_set, dev_id, port_id, enable)

sw_error_t fal_port_link_forcemode_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_link_forcemode_get, dev_id, port_id, enable)

#ifndef IN_PORTCONTROL_MINI
sw_error_t fal_port_congestion_drop_set (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t queue_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_congestion_drop_set, dev_id, port_id, queue_id, enable)

sw_error_t fal_port_bp_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_bp_status_set, dev_id, port_id, enable)

sw_error_t fal_port_bp_status_get (a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_bp_status_get, dev_id, port_id, enable)

sw_error_t fal_port_congestion_drop_get (a_uint32_t dev_id, fal_port_t port_id, a_uint32_t queue_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_congestion_drop_get, dev_id, port_id, queue_id, enable)

sw_error_t fal_ring_union_set(a_uint32_t dev_id, a_bool_t en)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_union_set, dev_id, en)

sw_error_t fal_ring_union_get(a_uint32_t dev_id, a_bool_t *en)
    DEFINE_FAL_FUNC_HSL_EXPORT(ring_union_get, dev_id, en)
sw_error_t fal_vch_bp_status_get(a_uint32_t dev_id, a_uint32_t vch_id, a_bool_t *status)
    DEFINE_FAL_FUNC_HSL(ring_flow_ctrl_status_get, dev_id, vch_id, status)
	EXPORT_SYMBOL(fal_vch_bp_status_get);

#endif
