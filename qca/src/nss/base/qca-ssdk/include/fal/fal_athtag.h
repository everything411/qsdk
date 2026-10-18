/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_athtag FAL_ATHTAG
 * @{
 */
#ifndef _FAL_ATHTAG_H_
#define _FAL_ATHTAG_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"

#define MHT_ATHTAG_TYPE		0xaaaa
typedef struct {
	a_uint8_t ath_pri; /*athtag priority 0-7*/
	a_uint8_t int_pri; /*internal priority 0-15*/
} fal_athtag_pri_mapping_t;

typedef struct {
	fal_pbmp_t ath_port; /* athtag portmap.
			      * port type 0 phy port bitmap for v2/v3 atheros header.
			      * port type 2 vp port for v1 atheros header.
			      */
	fal_port_t int_port; /*internal port*/
} fal_athtag_port_mapping_t;

typedef enum {
	FAL_ATHTAG_VER0 = 0, /* ver0: Legacy version (v2 and v3) Atheros header, for RX*/
	FAL_ATHTAG_VER1 = 1, /* ver1: New version (v1) Atheros header for
			      * JHPPE/HMSPPE/HTTPPE, for both RX and TX
			      */
	FAL_ATHTAG_VER2 = 2, /* ver2: v2 Atheros header, For TX */
	FAL_ATHTAG_VER3 = 3, /* ver3: v3 Atheros header with vchannel ID, For TX */
} fal_athtag_version_t;

typedef struct {
	a_bool_t athtag_en; /*enable rx athtag or not*/
	a_uint16_t athtag_type; /*two bytes athtag type*/
	fal_athtag_version_t version; /* 0 for legacy version (v2 and v3) Atheros header,
				       * 1 for new version (v1) Atheros header
				       */
} fal_athtag_rx_cfg_t;

typedef enum
{
	FAL_ATHTAG_ACTION_NORMAL = 0,
	FAL_ATHTAG_ACTION_READ_WRITE_REG,
	FAL_ATHTAG_ACTION_DISABLE_LEARN,
	FAL_ATHTAG_ACTION_DISABLE_OFFLOAD,
	FAL_ATHTAG_ACTION_DISABLE_LEARN_OFFLOAD,
} fal_athtag_action_t;

typedef struct {
	a_bool_t athtag_en; /*enable insert athtag or not*/
	a_uint16_t athtag_type; /*two bytes athtag type*/
	fal_athtag_version_t version; /* version field
				       * 1 for v1 atheros header
				       * 2 for v2 atheros header
				       * 3 for v3 atheros header
				       */
	fal_athtag_action_t action; /*ation field*/
	a_bool_t bypass_fwd_en; /*bypass fwd engine field*/
	a_bool_t field_disable; /*ver3 fields disable, only vchannel id is valid*/
} fal_athtag_tx_cfg_t;

sw_error_t
fal_athtag_pri_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping);

sw_error_t
fal_athtag_pri_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping);

sw_error_t
fal_athtag_port_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping);

sw_error_t
fal_athtag_port_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping);

sw_error_t
fal_port_athtag_rx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg);

sw_error_t
fal_port_athtag_rx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg);

sw_error_t
fal_port_athtag_tx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg);

sw_error_t
fal_port_athtag_tx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg);

sw_error_t
fal_athtag_rx_src_port_mapping_set(a_uint32_t dev_id, fal_port_t ath_src_port,
		fal_dest_info_t *int_dest_info);

sw_error_t
fal_athtag_rx_src_port_mapping_get(a_uint32_t dev_id, fal_port_t ath_src_port,
		fal_dest_info_t *int_dest_info);

sw_error_t
fal_athtag_rx_dest_port_mapping_set(a_uint32_t dev_id, fal_port_t ath_dest_port,
		fal_dest_info_t *int_dest_info);

sw_error_t
fal_athtag_rx_dest_port_mapping_get(a_uint32_t dev_id, fal_port_t ath_dest_port,
		fal_dest_info_t *int_dest_info);

sw_error_t
fal_athtag_rx_servcode_mapping_set(a_uint32_t dev_id, a_uint32_t ath_servcode,
		a_uint32_t int_servcode);

sw_error_t
fal_athtag_rx_servcode_mapping_get(a_uint32_t dev_id, a_uint32_t ath_servcode,
		a_uint32_t *int_servcode);

sw_error_t
fal_athtag_tx_src_port_mapping_set(a_uint32_t dev_id, fal_port_t int_src_port,
		fal_port_t ath_src_port);

sw_error_t
fal_athtag_tx_src_port_mapping_get(a_uint32_t dev_id, fal_port_t int_src_port,
		fal_port_t *ath_src_port);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_ATHTAG_H_ */
/**
 * @}
 */

