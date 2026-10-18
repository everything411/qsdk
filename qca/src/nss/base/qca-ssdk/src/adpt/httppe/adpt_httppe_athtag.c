/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_htt_reg.h"
#include "adpt.h"
#include "adpt_httppe_athtag.h"

sw_error_t adpt_httppe_athtag_egress_pri_mapping_set(a_uint32_t dev_id,
		fal_athtag_pri_mapping_t * pri_mapping)
{
	union eg_hdr_xmit_pri_mapping_u ptx_pri_map = {0};
	sw_error_t rv;

	ADPT_NULL_POINT_CHECK(pri_mapping);

	rv = httppe_eg_hdr_xmit_pri_mapping_get(dev_id,
			pri_mapping->int_pri, &ptx_pri_map);
	SW_RTN_ON_ERROR(rv);

	ptx_pri_map.bf.pri = pri_mapping->ath_pri;

	return httppe_eg_hdr_xmit_pri_mapping_set(dev_id,
			pri_mapping->int_pri, &ptx_pri_map);
}

sw_error_t adpt_httppe_athtag_egress_pri_mapping_get(a_uint32_t dev_id,
		fal_athtag_pri_mapping_t * pri_mapping)
{
	union eg_hdr_xmit_pri_mapping_u ptx_pri_map = {0};
	sw_error_t rv;

	ADPT_NULL_POINT_CHECK(pri_mapping);

	rv = httppe_eg_hdr_xmit_pri_mapping_get(dev_id,
			pri_mapping->int_pri, &ptx_pri_map);
	SW_RTN_ON_ERROR(rv);

	pri_mapping->ath_pri = ptx_pri_map.bf.pri;

	return SW_OK;
}

sw_error_t adpt_httppe_athtag_tx_ath_hdr_type_set(a_uint32_t dev_id,
		fal_athtag_tx_cfg_t *cfg)
{
	union eg_gen_ctrl_u eg_gen_ctrl = {0};
	sw_error_t rv;

	ADPT_NULL_POINT_CHECK(cfg);

	rv = httppe_eg_gen_ctrl_get(dev_id, &eg_gen_ctrl);
	SW_RTN_ON_ERROR(rv);

	eg_gen_ctrl.bf.ath_hdr_type = cfg->athtag_type;

	return httppe_eg_gen_ctrl_set(dev_id, &eg_gen_ctrl);
}

sw_error_t adpt_httppe_athtag_tx_ath_hdr_type_get(a_uint32_t dev_id,
		fal_athtag_tx_cfg_t *cfg)
{
	union eg_gen_ctrl_u eg_gen_ctrl = {0};
	sw_error_t rv;

	ADPT_NULL_POINT_CHECK(cfg);

	rv = httppe_eg_gen_ctrl_get(dev_id, &eg_gen_ctrl);
	SW_RTN_ON_ERROR(rv);

	cfg->athtag_type = eg_gen_ctrl.bf.ath_hdr_type;

	return SW_OK;
}

sw_error_t adpt_httppe_athtag_rx_dest_port_mapping_set(a_uint32_t dev_id,
		fal_port_t ath_dest_port, fal_dest_info_t *dest_info)
{
	union dst_port_mapping_tbl_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(dest_info);

	rv = httppe_dst_port_mapping_tbl_get(dev_id, FAL_PORT_ID_VALUE(ath_dest_port), &reg);
	SW_RTN_ON_ERROR(rv);

	if (dest_info->dest_info_type == FAL_DEST_INFO_INVALID) {
		reg.bf.dst_info_valid = A_FALSE;
		reg.bf.dst_info = 0;
	} else {
		reg.bf.dst_info_valid = A_TRUE;
		reg.bf.dst_info =
			ADPT_DEST_INFO(adpt_dest_type_convert(A_TRUE, dest_info->dest_info_type),
				       dest_info->dest_info_value);
	}

	return httppe_dst_port_mapping_tbl_set(dev_id, FAL_PORT_ID_VALUE(ath_dest_port), &reg);
}

sw_error_t adpt_httppe_athtag_rx_dest_port_mapping_get(a_uint32_t dev_id,
		fal_port_t ath_dest_port, fal_dest_info_t *dest_info)
{
	union dst_port_mapping_tbl_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(dest_info);

	rv = httppe_dst_port_mapping_tbl_get(dev_id, FAL_PORT_ID_VALUE(ath_dest_port), &reg);
	SW_RTN_ON_ERROR(rv);

	if (reg.bf.dst_info_valid == A_FALSE) {
		dest_info->dest_info_type = FAL_DEST_INFO_INVALID;
		dest_info->dest_info_value = 0;
	} else {
		dest_info->dest_info_type = adpt_dest_type_convert(A_FALSE,
				ADPT_DEST_TYPE(reg.bf.dst_info));
		dest_info->dest_info_value = ADPT_DEST_VAL(reg.bf.dst_info);
	}

	return SW_OK;
}

sw_error_t adpt_httppe_athtag_rx_servcode_mapping_set(a_uint32_t dev_id,
		a_uint32_t ath_servcode, a_uint32_t int_servcode)
{
	union service_code_mapping_u reg = {0};
	sw_error_t rv = SW_OK;

	rv = httppe_service_code_mapping_get(dev_id, ath_servcode, &reg);
	SW_RTN_ON_ERROR(rv);

	if (int_servcode == FAL_SERVCODE_INVALID) {
		reg.bf.service_code_valid = A_FALSE;
		reg.bf.service_code = 0;
	} else {
		reg.bf.service_code_valid = A_TRUE;
		reg.bf.service_code = int_servcode;
	}

	return httppe_service_code_mapping_set(dev_id, ath_servcode, &reg);
}

sw_error_t adpt_httppe_athtag_rx_servcode_mapping_get(a_uint32_t dev_id,
		a_uint32_t ath_servcode, a_uint32_t *int_servcode)
{
	union service_code_mapping_u reg = {0};
	sw_error_t rv = SW_OK;

	ADPT_NULL_POINT_CHECK(int_servcode);

	rv = httppe_service_code_mapping_get(dev_id, ath_servcode, &reg);
	SW_RTN_ON_ERROR(rv);

	if (reg.bf.service_code_valid == A_FALSE)
		*int_servcode = FAL_SERVCODE_INVALID;
	else
		*int_servcode = reg.bf.service_code;

	return SW_OK;
}

/**
 * @}
 */

