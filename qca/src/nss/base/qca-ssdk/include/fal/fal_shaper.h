/*
 * Copyright (c) 2016-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_shaper FAL_SHAPER
 * @{
 */
#ifndef _FAL_SHAPER_H_
#define _FAL_SHAPER_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

/* Shaper rate limits */
#define FAL_BYTE_SHAPER_MIN_RATE      64         /* min in Kbps */
#define FAL_BYTE_SHAPER_MAX_RATE      10000000   /* 10G in Kbps */
#define FAL_FRAME_SHAPER_MIN_RATE     6          /* min in fps  */
#define FAL_FRAME_SHAPER_MAX_RATE     14881000   /* 10G in fps  */
#define FAL_BYTE_SHAPER_MAX_RATE_25G  25000000   /* 25G in Kbps */
#define FAL_FRAME_SHAPER_MAX_RATE_25G 37202500   /* 25G in fps  */

typedef enum
{
	FAL_IPG_PREAMBLE_FRAME_CRC = 0,  /* IPG + Preamble + Frame + CRC */
	FAL_FRAME_CRC,  /* Frame + CRC */
	FAL_L3_EXCLUDE_CRC  /* after Ethernet type excude CRC*/
} fal_shaper_frame_mode_t;

typedef enum
{
	FAL_SHAPER_METER_RFC = 0, /* legacy feature, add it for ipq95xx */
	FAL_SHAPER_METER_MEF10_3 /* mef 10-3 feature, add it for ipq95xx */
} fal_shaper_meter_type_t;

typedef struct
{
	a_bool_t  couple_en; /* two buckets coupled enable or disable*/
	a_uint32_t meter_unit; /* 0 byte based, 1 frame based */
	a_bool_t	c_shaper_en; /* egress shaer C bucket enable or disable */
	a_uint32_t cbs; /* committed burst size */
	a_uint32_t cir; /* committed information rate */
	a_bool_t	e_shaper_en; /* egress shaper E bucket enable or disable */
	a_uint32_t ebs; /* excess burst size */
	a_uint32_t eir; /* excess information rate */
	fal_shaper_frame_mode_t shaper_frame_mode; /* shaper frame mode */
	a_uint32_t cir_max; /* max committed information rate, add it for ipq95xx */
	a_uint32_t eir_max; /* max excess information rate, add it for ipq95xx */
	a_uint32_t next_ptr; /* next entry, add it for ipq95xx */
	a_bool_t grp_end; /* last entry or not in current group, add it for ipq95xx */
	a_bool_t grp_couple_en; /* group coupled not, add it for ipq95xx */
	fal_shaper_meter_type_t meter_type; /*legacy or mef10-3,add it for ipq95xx */
} fal_shaper_config_t;

typedef struct
{
     a_bool_t      c_token_number_negative_en; /* C token is negative or not */
     a_uint32_t    c_token_number; /* C token value */
     a_bool_t      e_token_number_negative_en; /* E token is negative or not */
     a_uint32_t    e_token_number; /* E token value */
} fal_shaper_token_number_t;

typedef struct
{
	a_uint32_t head; /* linklist head, add it for ipq95xx */
	a_uint32_t tail; /* linklist tail, add it for ipq95xx */
} fal_shaper_ctrl_t;

sw_error_t
fal_queue_shaper_token_number_set(a_uint32_t dev_id,a_uint32_t queue_id,
		fal_shaper_token_number_t *token_number);

sw_error_t
fal_flow_shaper_token_number_set(a_uint32_t dev_id, a_uint32_t flow_id,
		fal_shaper_token_number_t *token_number);

sw_error_t
fal_port_shaper_token_number_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_shaper_token_number_t *token_number);

sw_error_t
fal_port_shaper_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot);

sw_error_t
fal_queue_shaper_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot);

sw_error_t
fal_flow_shaper_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot);

sw_error_t
fal_shaper_ipg_preamble_length_set(a_uint32_t dev_id, a_uint32_t ipg_pre_length);

sw_error_t
fal_port_shaper_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_shaper_config_t * shaper);

sw_error_t
fal_queue_shaper_set(a_uint32_t dev_id,a_uint32_t queue_id,
		fal_shaper_config_t * shaper);

sw_error_t
fal_queue_shaper_get(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_shaper_config_t * shaper);

sw_error_t
fal_flow_shaper_set(a_uint32_t dev_id, a_uint32_t flow_id,
		fal_shaper_config_t * shaper);

sw_error_t
fal_queue_shaper_ctrl_set(a_uint32_t dev_id,
		fal_shaper_ctrl_t *queue_shaper_ctrl);

sw_error_t
fal_flow_shaper_ctrl_set(a_uint32_t dev_id,
		fal_shaper_ctrl_t *flow_shaper_ctrl);

#ifndef IN_SHAPER_MINI
sw_error_t
fal_port_shaper_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_shaper_config_t * shaper);

sw_error_t
fal_flow_shaper_get(a_uint32_t dev_id, a_uint32_t flow_id,
		fal_shaper_config_t * shaper);

sw_error_t
fal_queue_shaper_token_number_get(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_shaper_token_number_t *token_number);

sw_error_t
fal_flow_shaper_token_number_get(a_uint32_t dev_id, a_uint32_t flow_id,
		fal_shaper_token_number_t *token_number);

sw_error_t
fal_port_shaper_token_number_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_shaper_token_number_t *token_number);

sw_error_t
fal_port_shaper_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot);

sw_error_t
fal_queue_shaper_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot);

sw_error_t
fal_flow_shaper_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot);

sw_error_t
fal_shaper_ipg_preamble_length_get(a_uint32_t dev_id, a_uint32_t *ipg_pre_length);

sw_error_t
fal_queue_shaper_ctrl_get(a_uint32_t dev_id, fal_shaper_ctrl_t *queue_shaper_ctrl);

sw_error_t
fal_flow_shaper_ctrl_get(a_uint32_t dev_id, fal_shaper_ctrl_t *flow_shaper_ctrl);
#endif


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_SHAPER_H_ */
/**
 * @}
 */
