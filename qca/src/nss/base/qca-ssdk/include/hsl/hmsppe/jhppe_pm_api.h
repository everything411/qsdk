/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_PM_API_H_
#define _JHPPE_PM_API_H_

#define PM_COUNTER_TBL_MAX_ENTRY	PM_COUNTER_TBL_NUM

sw_error_t
jhppe_pm_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pm_counter_tbl_u *value);

sw_error_t
jhppe_pm_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pm_counter_tbl_u *value);

sw_error_t
jhppe_pm_counter_tbl_bcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_bcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_crc_error_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_crc_error_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_frame_1024_1518_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_frame_1024_1518_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_frame_128_255_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_frame_128_255_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_frame_256_511_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_frame_256_511_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_frame_512_1023_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_frame_512_1023_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_frame_64_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_frame_64_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_frame_65_127_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_frame_65_127_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_mcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_mcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_octets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_pm_counter_tbl_octets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
jhppe_pm_counter_tbl_oversize_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_oversize_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pm_counter_tbl_ucast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pm_counter_tbl_ucast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PRE_IPO_PM_COUNTER_GEN_MAX_ENTRY	PRE_IPO_PM_COUNTER_GEN_NUM

sw_error_t
jhppe_pre_ipo_pm_counter_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_pm_counter_gen_u *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_pm_counter_gen_u *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_counter_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_counter_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_ipmc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_ipmc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_tag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_tag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_PM_COUNTER_GEN_MAX_ENTRY	EG_PM_COUNTER_GEN_NUM

sw_error_t
jhppe_eg_pm_counter_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_gen_u *value);

sw_error_t
jhppe_eg_pm_counter_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_gen_u *value);

sw_error_t
jhppe_eg_pm_counter_gen_ipmc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_ipmc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_tag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_tag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_gen_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_gen_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_PM_COUNTER_TBL_MAX_ENTRY	EG_PM_COUNTER_TBL_NUM

sw_error_t
jhppe_eg_pm_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_tbl_u *value);

sw_error_t
jhppe_eg_pm_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_tbl_u *value);

sw_error_t
jhppe_eg_pm_counter_tbl_bcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_bcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_1024_1518_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_1024_1518_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_128_255_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_128_255_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_256_511_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_256_511_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_512_1023_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_512_1023_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_64_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_64_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_65_127_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_frame_65_127_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_mcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_mcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_octets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_octets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_oversize_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_oversize_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_pm_counter_tbl_ucast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_pm_counter_tbl_ucast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
