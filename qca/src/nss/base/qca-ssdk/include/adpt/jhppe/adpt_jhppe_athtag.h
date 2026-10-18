/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_ATHTAG_H_
#define _ADPT_JHPPE_ATHTAG_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t  adpt_jhppe_v1_athtag_ingress_port_mapping_get(a_uint32_t dev_id,
		fal_athtag_port_mapping_t * port_mapping);

sw_error_t adpt_jhppe_v1_athtag_port_mapping_set (a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t * port_mapping);

sw_error_t adpt_jhppe_athtag_rx_src_port_mapping_set(a_uint32_t dev_id,
		fal_port_t ath_src_port, fal_dest_info_t *int_dest_info);

sw_error_t adpt_jhppe_athtag_rx_src_port_mapping_get(a_uint32_t dev_id,
		fal_port_t ath_src_port, fal_dest_info_t *int_dest_info);

sw_error_t adpt_jhppe_athtag_tx_src_port_mapping_set(a_uint32_t dev_id,
		fal_port_t int_src_port, fal_port_t ath_src_port);

sw_error_t adpt_jhppe_athtag_tx_src_port_mapping_get(a_uint32_t dev_id,
		fal_port_t int_src_port, fal_port_t *ath_src_port);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
