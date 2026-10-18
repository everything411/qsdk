/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "fal_athtag.h"
#include "adpt.h"
#include "ssdk_dts.h"

sw_error_t adpt_jhppe_v1_athtag_ingress_port_mapping_get(a_uint32_t dev_id,
		fal_athtag_port_mapping_t * port_mapping)
{
	union src_port_mapping_tbl_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(port_mapping);

	rv = jhppe_src_port_mapping_tbl_get(dev_id,
			FAL_PORT_ID_VALUE(port_mapping->ath_port), &reg);
	SW_RTN_ON_ERROR(rv);

	if (reg.bf.port_valid)
		port_mapping->int_port = reg.bf.port_vp;
	else
		port_mapping->int_port = FAL_PORT_ID(FAL_PORT_TYPE_INVALID, 0);

	return SW_OK;
}

sw_error_t adpt_jhppe_v1_athtag_port_mapping_set (a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t * port_mapping)
{
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(port_mapping);

	if (direction == FAL_DIR_INGRESS || direction == FAL_DIR_BOTH) {
		union src_port_mapping_tbl_u reg = {0};

		rv = jhppe_src_port_mapping_tbl_get(dev_id,
				FAL_PORT_ID_VALUE(port_mapping->ath_port), &reg);
		SW_RTN_ON_ERROR(rv);

		if (FAL_PORT_ID_TYPE(port_mapping->int_port) == FAL_PORT_TYPE_INVALID) {
			reg.bf.port_valid = A_FALSE;
			reg.bf.port_vp = 0;
		} else {
			reg.bf.port_valid = A_TRUE;
			reg.bf.port_vp = FAL_PORT_ID_VALUE(port_mapping->int_port);
		}
		rv = jhppe_src_port_mapping_tbl_set(dev_id,
				FAL_PORT_ID_VALUE(port_mapping->ath_port), &reg);
		SW_RTN_ON_ERROR(rv);
	}

	if (direction == FAL_DIR_EGRESS || direction == FAL_DIR_BOTH) {
		fal_athtag_tx_cfg_t tx_cfg = {0};
		union eg_vp_tbl_u eg_vp_tbl = {0};
		a_uint32_t ath_port;
		rv = appe_egress_vp_tbl_get(dev_id,
				FAL_PORT_ID_VALUE(port_mapping->int_port), &eg_vp_tbl);
		SW_RTN_ON_ERROR(rv);

		ath_port = FAL_PORT_ID_VALUE(port_mapping->ath_port);
		eg_vp_tbl.bf.ath_hdr_disable_bit = (ath_port >> 7) & 0x1;
		eg_vp_tbl.bf.ath_port_bitmap_1 = (ath_port >> 6) & 0x1;
		eg_vp_tbl.bf.ath_port_bitmap_0 = ath_port & 0x3f;
		rv = appe_egress_vp_tbl_set(dev_id,
				FAL_PORT_ID_VALUE(port_mapping->int_port), &eg_vp_tbl);
		SW_RTN_ON_ERROR(rv);

		tx_cfg.version = FAL_ATHTAG_VER1;
		tx_cfg.athtag_en = A_TRUE;
		tx_cfg.bypass_fwd_en = A_TRUE;
		rv = fal_port_athtag_tx_set(dev_id, port_mapping->int_port, &tx_cfg);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

sw_error_t adpt_jhppe_athtag_rx_src_port_mapping_set(a_uint32_t dev_id,
		fal_port_t ath_src_port, fal_dest_info_t *int_dest_info)
{
	union src_port_mapping_tbl_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(int_dest_info);

	rv = jhppe_src_port_mapping_tbl_get(dev_id, FAL_PORT_ID_VALUE(ath_src_port), &reg);
	SW_RTN_ON_ERROR(rv);

	if (int_dest_info->dest_info_type == FAL_DEST_INFO_INVALID) {
		reg.bf.dst_info_valid = A_FALSE;
		reg.bf.dst_info = 0;
	} else {
		reg.bf.dst_info_valid = A_TRUE;
		reg.bf.dst_info =
		ADPT_DEST_INFO(adpt_dest_type_convert(A_TRUE, int_dest_info->dest_info_type),
				       int_dest_info->dest_info_value);
	}

	return jhppe_src_port_mapping_tbl_set(dev_id, FAL_PORT_ID_VALUE(ath_src_port), &reg);
}

sw_error_t adpt_jhppe_athtag_rx_src_port_mapping_get(a_uint32_t dev_id,
		fal_port_t ath_src_port, fal_dest_info_t *int_dest_info)
{
	union src_port_mapping_tbl_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(int_dest_info);

	rv = jhppe_src_port_mapping_tbl_get(dev_id, FAL_PORT_ID_VALUE(ath_src_port), &reg);
	SW_RTN_ON_ERROR(rv);

	if (reg.bf.dst_info_valid == A_FALSE) {
		int_dest_info->dest_info_type = FAL_DEST_INFO_INVALID;
		int_dest_info->dest_info_value = 0;
	} else {
		int_dest_info->dest_info_type = adpt_dest_type_convert(A_FALSE,
							ADPT_DEST_TYPE(reg.bf.dst_info));
		int_dest_info->dest_info_value = ADPT_DEST_VAL(reg.bf.dst_info);
	}

	return SW_OK;
}

sw_error_t adpt_jhppe_athtag_tx_src_port_mapping_set(a_uint32_t dev_id,
		fal_port_t int_src_port, fal_port_t ath_src_port)
{
	union eg_src_port_remap_u reg = {0};
	sw_error_t rv = SW_OK;

	rv = jhppe_eg_src_port_remap_get(dev_id, FAL_PORT_ID_VALUE(int_src_port), &reg);
	SW_RTN_ON_ERROR(rv);

	reg.bf.src_port_num = FAL_PORT_ID_VALUE(ath_src_port);

	return jhppe_eg_src_port_remap_set(dev_id, FAL_PORT_ID_VALUE(int_src_port), &reg);
}

sw_error_t adpt_jhppe_athtag_tx_src_port_mapping_get(a_uint32_t dev_id,
		fal_port_t int_src_port, fal_port_t *ath_src_port)
{
	union eg_src_port_remap_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(ath_src_port);

	rv = jhppe_eg_src_port_remap_get(dev_id, FAL_PORT_ID_VALUE(int_src_port), &reg);
	SW_RTN_ON_ERROR(rv);

	*ath_src_port = reg.bf.src_port_num;

	return SW_OK;
}

/**
 * @}
 */

