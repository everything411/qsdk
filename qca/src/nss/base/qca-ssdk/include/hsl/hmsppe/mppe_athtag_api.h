/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MPPE_ATHTAG_API_H_
#define _MPPE_ATHTAG_API_H_

#define EG_HDR_XMIT_PRI_MAPPING_MAX_ENTRY	EG_HDR_XMIT_PRI_MAPPING_NUM

sw_error_t
mppe_eg_hdr_xmit_pri_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_hdr_xmit_pri_mapping_u *value);

sw_error_t
mppe_eg_hdr_xmit_pri_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_hdr_xmit_pri_mapping_u *value);

sw_error_t
mppe_eg_hdr_xmit_pri_mapping_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_eg_hdr_xmit_pri_mapping_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
mppe_eg_gen_ctrl_get(
		a_uint32_t dev_id,
		union eg_gen_ctrl_u *value);

sw_error_t
mppe_eg_gen_ctrl_set(
		a_uint32_t dev_id,
		union eg_gen_ctrl_u *value);

sw_error_t
mppe_eg_gen_ctrl_ath_hdr_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_ath_hdr_type_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_flow_cookie_pri_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_flow_cookie_pri_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_snap_len_update_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_snap_len_update_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit2_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit2_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit3_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit3_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit4_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit4_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit5_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_spare_bit5_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_strip_inner_padding_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_strip_inner_padding_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_bridge_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_bridge_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_data_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_data_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_route_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_eg_gen_ctrl_strip_padding_route_en_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
mppe_edma_vp_remap_0_get(
		a_uint32_t dev_id,
		union edma_vp_remap_0_u *value);

sw_error_t
mppe_edma_vp_remap_0_set(
		a_uint32_t dev_id,
		union edma_vp_remap_0_u *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_2_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_2_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_3_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_3_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_4_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_4_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_5_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_5_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_6_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_6_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_7_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_0_remap_vp_7_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
mppe_edma_vp_remap_1_get(
		a_uint32_t dev_id,
		union edma_vp_remap_1_u *value);

sw_error_t
mppe_edma_vp_remap_1_set(
		a_uint32_t dev_id,
		union edma_vp_remap_1_u *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_10_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_10_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_11_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_11_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_12_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_12_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_13_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_13_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_14_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_14_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_15_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_15_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_8_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_8_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_9_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mppe_edma_vp_remap_1_remap_vp_9_set(
		a_uint32_t dev_id,
		unsigned int value);

#define PRX_PORT_TO_VP_MAPPING_MAX_ENTRY	PRX_PORT_TO_VP_MAPPING_NUM

sw_error_t
mppe_prx_port_to_vp_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union prx_port_to_vp_mapping_u *value);

sw_error_t
mppe_prx_port_to_vp_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union prx_port_to_vp_mapping_u *value);

sw_error_t
mppe_prx_port_to_vp_mapping_atheros_hdr_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_prx_port_to_vp_mapping_atheros_hdr_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
mppe_prx_port_to_vp_mapping_atheros_hdr_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_prx_port_to_vp_mapping_atheros_hdr_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
mppe_prx_port_to_vp_mapping_atheros_hdr_ver_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_prx_port_to_vp_mapping_atheros_hdr_ver_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
mppe_prx_port_to_vp_mapping_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_prx_port_to_vp_mapping_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PRX_HDR_RCV_PRI_MAPPING_MAX_ENTRY	PRX_HDR_RCV_PRI_MAPPING_NUM

sw_error_t
mppe_prx_hdr_rcv_pri_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union prx_hdr_rcv_pri_mapping_u *value);

sw_error_t
mppe_prx_hdr_rcv_pri_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union prx_hdr_rcv_pri_mapping_u *value);

sw_error_t
mppe_prx_hdr_rcv_pri_mapping_int_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_prx_hdr_rcv_pri_mapping_int_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
