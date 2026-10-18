/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_HTTPPE_ATHTAG_H_
#define _ADPT_HTTPPE_ATHTAG_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t adpt_httppe_athtag_egress_pri_mapping_set(a_uint32_t dev_id,
		fal_athtag_pri_mapping_t * pri_mapping);

sw_error_t adpt_httppe_athtag_egress_pri_mapping_get(a_uint32_t dev_id,
		fal_athtag_pri_mapping_t * pri_mapping);

sw_error_t adpt_httppe_athtag_tx_ath_hdr_type_set(a_uint32_t dev_id,
		fal_athtag_tx_cfg_t *cfg);

sw_error_t adpt_httppe_athtag_tx_ath_hdr_type_get(a_uint32_t dev_id,
		fal_athtag_tx_cfg_t *cfg);

sw_error_t adpt_httppe_athtag_rx_dest_port_mapping_set(a_uint32_t dev_id,
		fal_port_t ath_dest_port, fal_dest_info_t *dest_info);

sw_error_t adpt_httppe_athtag_rx_dest_port_mapping_get(a_uint32_t dev_id,
		fal_port_t ath_dest_port, fal_dest_info_t *dest_info);

sw_error_t adpt_httppe_athtag_rx_servcode_mapping_set(a_uint32_t dev_id,
		a_uint32_t ath_servcode, a_uint32_t int_servcode);

sw_error_t adpt_httppe_athtag_rx_servcode_mapping_get(a_uint32_t dev_id,
		a_uint32_t ath_servcode, a_uint32_t *int_servcode);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif

