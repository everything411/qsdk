/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_athtag FAL_ATHTAG
 * @{
 */
#include "sw.h"
#include "fal_athtag.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_athtag_pri_mapping_set(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_pri_mapping_set, dev_id, direction, pri_mapping)

sw_error_t fal_athtag_pri_mapping_get(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_pri_mapping_get, dev_id, direction, pri_mapping)

sw_error_t fal_athtag_port_mapping_set(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_port_mapping_set, dev_id, direction, port_mapping)

sw_error_t fal_athtag_port_mapping_get(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_port_mapping_get, dev_id, direction, port_mapping)

sw_error_t fal_port_athtag_rx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_rx_set, dev_id, port_id, cfg)

sw_error_t fal_port_athtag_rx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_rx_get, dev_id, port_id, cfg)

sw_error_t fal_port_athtag_tx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_tx_set, dev_id, port_id, cfg)

sw_error_t fal_port_athtag_tx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_tx_get, dev_id, port_id, cfg)

sw_error_t fal_athtag_rx_src_port_mapping_set(a_uint32_t dev_id, fal_port_t ath_src_port, fal_dest_info_t *int_dest_info)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_rx_src_port_mapping_set, dev_id, ath_src_port, int_dest_info)

sw_error_t fal_athtag_rx_src_port_mapping_get(a_uint32_t dev_id, fal_port_t ath_src_port, fal_dest_info_t *int_dest_info)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_rx_src_port_mapping_get, dev_id, ath_src_port, int_dest_info)

sw_error_t fal_athtag_rx_dest_port_mapping_set(a_uint32_t dev_id, fal_port_t ath_dest_port, fal_dest_info_t *int_dest_info)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_rx_dest_port_mapping_set, dev_id, ath_dest_port, int_dest_info)

sw_error_t fal_athtag_rx_dest_port_mapping_get(a_uint32_t dev_id, fal_port_t ath_dest_port, fal_dest_info_t *int_dest_info)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_rx_dest_port_mapping_get, dev_id, ath_dest_port, int_dest_info)

sw_error_t fal_athtag_rx_servcode_mapping_set(a_uint32_t dev_id, a_uint32_t ath_servcode, a_uint32_t int_servcode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_rx_servcode_mapping_set, dev_id, ath_servcode, int_servcode)

sw_error_t fal_athtag_rx_servcode_mapping_get(a_uint32_t dev_id, a_uint32_t ath_servcode, a_uint32_t *int_servcode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_rx_servcode_mapping_get, dev_id, ath_servcode, int_servcode)

sw_error_t fal_athtag_tx_src_port_mapping_set(a_uint32_t dev_id, fal_port_t int_src_port, fal_port_t ath_src_port)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_tx_src_port_mapping_set, dev_id, int_src_port, ath_src_port)

sw_error_t fal_athtag_tx_src_port_mapping_get(a_uint32_t dev_id, fal_port_t int_src_port, fal_port_t *ath_src_port)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_tx_src_port_mapping_get, dev_id, int_src_port, ath_src_port)
