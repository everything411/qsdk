/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/


/**
 * @defgroup fal_sampling FAL_SAMPL
 * @{
 */
#ifndef _FAL_SAMPL_H_
#define _FAL_SAMPL_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"


/* Actived Pingpong buffer */
#define FAL_SAMPL_BUFF0	0x0
#define FAL_SAMPL_BUFF1	0x1


typedef enum {
	FAL_SAMPL_CNT_ETHERNET_PKT = 0, /* packet length with ethernet header */
	FAL_SAMPL_CNT_IP_PKT,	/* IP header onwards (without L2 header) */
	FAL_SAMPL_CNT_TUNNEL_IP_PKT, /* Tunnel IP packet (without L2 outer header) */
} fal_sampling_counter_mode_t;

typedef struct {
	fal_sampling_counter_mode_t	counter_mode;
	a_bool_t counter_overflow_flag; /* 1 = counter overflow; 0 = keep */
	a_bool_t counter_clear_mode; /*1 = Not read clear;0 = read clear*/
	a_uint32_t comp_len; /* Compensate length */
	a_uint32_t iat_shift_bit; /* Inter Arrival Time shift bits */
} fal_sampling_ctrl_t;

typedef struct {
	a_uint32_t burst_threshold; /* unit: bytes */
	a_uint32_t burst_short_intvl; /* unit: cycle number * 4 */
	a_uint32_t burst_long_intvl; /* unit: cycle number * 4 */
} fal_sampling_burst_cfg_t;

typedef struct {
	a_uint32_t matched_pkts; /* packet counter */
	a_uint64_t matched_bytes; /* bytes counter */
	a_uint16_t bytes_min; /* Minimum packet length */
	a_uint16_t bytes_max; /* maximum packet length */
	a_uint16_t packet_iat_min; /* minimum inter-packet arrival time  */
	a_uint16_t packet_iat_max; /* maximum inter-packet arrival time  */
	a_uint32_t burst_oc_bytes; /* bytes after 1st burst open or close */
	a_uint32_t burst_is_bytes; /* bytes that open the burst */
	a_uint32_t burst_bytes[10]; /* bytes received at each burst short
					interval for up to ten intervals*/
	a_uint32_t burst_timestamp; /* timestamp of burst open or close */
	a_uint32_t flow_timestamp; /* timestamp of last packet */
	a_uint16_t packet_iat_sum; /* sum of inter-packet arrival time */
	a_uint8_t  burst_flags; /* burst flags (bit0: burst open,
						bit1: burst close) */
} fal_sampling_counter_entry_t;

sw_error_t
fal_sampling_time_set(a_uint32_t dev_id, a_uint32_t buff_index,
			a_uint32_t time_value); /* unit: cycle number * 4 */

sw_error_t
fal_sampling_time_get(a_uint32_t dev_id, a_uint32_t buff_index,
			a_uint32_t *time_value); /* unit: cycle number * 4 */

sw_error_t
fal_sampling_buff_status_get(a_uint32_t dev_id, a_uint32_t buff_index,
			a_bool_t *done_status);

sw_error_t
fal_sampling_buff_status_clear(a_uint32_t dev_id, a_uint32_t buff_index);

sw_error_t
fal_sampling_done_miss_counter_get(a_uint32_t dev_id, a_uint32_t *done_miss);

sw_error_t
fal_sampling_done_miss_counter_clear(a_uint32_t dev_id);

sw_error_t
fal_sampling_burst_cfg_set(a_uint32_t dev_id,
				fal_sampling_burst_cfg_t *burst_cfg);

sw_error_t
fal_sampling_burst_cfg_get(a_uint32_t dev_id,
				fal_sampling_burst_cfg_t *burst_cfg);

sw_error_t
fal_sampling_ctrl_set(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg);

sw_error_t
fal_sampling_ctrl_get(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg);

sw_error_t
fal_sampling_window_en_set(a_uint32_t dev_id,
				a_uint32_t window_index, a_bool_t enable);

sw_error_t
fal_sampling_window_en_get(a_uint32_t dev_id,
				a_uint32_t window_index, a_bool_t *enable);

sw_error_t
fal_sampling_counter_get(a_uint32_t dev_id, a_uint32_t buff_index,
		a_uint32_t counter_id, fal_sampling_counter_entry_t *counter);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_SAMPL_H_ */

/**
 * @}
 */

