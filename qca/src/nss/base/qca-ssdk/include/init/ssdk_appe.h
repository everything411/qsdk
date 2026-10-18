/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _SSDK_APPE_H_
#define _SSDK_APPE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "ssdk_init.h"

sw_error_t qca_appe_hw_init(a_uint32_t dev_id);
sw_error_t qca_appe_tdm_hw_init(a_uint32_t dev_id);
sw_error_t qca_appe_portctrl_hw_init(a_uint32_t dev_id);
sw_error_t qca_appe_shaper_hw_init(a_uint32_t dev_id);
sw_error_t qca_appe_policer_hw_init(a_uint32_t dev_id);
sw_error_t qca_appe_rss_hash_hw_init(a_uint32_t dev_id);
#if defined(IN_PON)
sw_error_t qca_hmsppe_omci_enq_vp_init(a_uint32_t dev_id, a_uint8_t enq_vp);
#endif

#define APPE_MAX_C_TOKEN_NUM                0x3fffffff
#define APPE_MAX_E_TOKEN_NUM                0x3fffffff
#define APPE_PORT_SHAPER_TIMESLOT_DFT       8
#define APPE_FLOW_SHAPER_TIMESLOT_DFT       128
#define APPE_QUEUE_SHAPER_TIMESLOT_DFT      353*2
#define APPE_SHAPER_IPG_PREAMBLE_LEN_DFT    20
#define APPE_QUEUE_SHAPER_HEAD              0
#define APPE_QUEUE_SHAPER_TAIL              299
#define APPE_FLOW_SHAPER_HEAD               0
#define APPE_FLOW_SHAPER_TAIL               63
#define HMSPPE_PORT_SHAPER_TIMESLOT_DFT     8
#define JHPPE_PORT_SHAPER_TIMESLOT_DFT      9
#define HTTPPE_PORT_SHAPER_TIMESLOT_DFT     6
#define HMSPPE_QUEUE_SHAPER_TIMESLOT_DFT    429*2
#define JHPPE_QUEUE_SHAPER_TIMESLOT_DFT     462*2
#define HTTPPE_QUEUE_SHAPER_TIMESLOT_DFT    300*2
#define APPE_POLICER_TIMESLOT_DFT           353*4
#define MPPE_POLICER_TIMESLOT_DFT           200*2
#define MRPPE_POLICER_TIMESLOT_DFT          375*4
#define HMSPPE_POLICER_TIMESLOT_DFT         429*2
#define JHPPE_POLICER_TIMESLOT_DFT          462*3
#define HTTPPE_POLICER_TIMESLOT_DFT         250*2
#define POLICER_CFG_MAX_512                 512
#define POLICER_CFG_MAX_128                 128
#define POLICER_HEAD                        0
#define POLICER_TAIL(cfg_max) ((cfg_max) - 1)

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SSDK_APPE_H */

