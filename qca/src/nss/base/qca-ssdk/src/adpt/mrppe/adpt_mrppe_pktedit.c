/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "adpt.h"
#if defined(JHPPE)
#include "adpt_jhppe_pktedit.h"
#endif

sw_error_t
adpt_mrppe_pktedit_padding_set(a_uint32_t dev_id, fal_pktedit_padding_t *padding)
{
	union ipr_spare_reg_u ipr_spare_reg = {0};
	union eg_gen_ctrl_u eg_gen_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	SW_RTN_ON_ERROR(mppe_eg_gen_ctrl_get(dev_id, &eg_gen_ctrl));
	SW_RTN_ON_ERROR(mrppe_ipr_spare_reg_get(dev_id, &ipr_spare_reg));

	/* strip padding */
	eg_gen_ctrl.bf.strip_padding_en = padding->strip_padding_en;
	eg_gen_ctrl.bf.strip_padding_data_en = padding->strip_padding_en;

	/* strip padding only for route */
	eg_gen_ctrl.bf.strip_padding_route_en = padding->strip_padding_route_en;

	/* strip padding only for bridge */
	eg_gen_ctrl.bf.strip_padding_bridge_en = padding->strip_padding_bridge_en;

	/* strip padding for tunnel decap innner */
	eg_gen_ctrl.bf.strip_inner_padding_en = padding->strip_tunnel_inner_padding_en;
	ipr_spare_reg.bf.inner_ip_len_update_en = padding->strip_tunnel_inner_padding_en;

	/* strip padding for snap */
	eg_gen_ctrl.bf.snap_len_update_en = padding->strip_padding_snap_en;

	/* payload checksum update by strip padding */
	ipr_spare_reg.bf.payload_checksum_strip_padding_en = padding->strip_padding_checksum_en;

	/* exception */
	ipr_spare_reg.bf.inner_ip_padding_exp_en = padding->tunnel_inner_padding_exp_en;
	ipr_spare_reg.bf.inner_ip_len_gap_exp_en = padding->tunnel_ip_len_gap_exp_en;

	SW_RTN_ON_ERROR(mppe_eg_gen_ctrl_set(dev_id, &eg_gen_ctrl));
	return mrppe_ipr_spare_reg_set(dev_id, &ipr_spare_reg);
}

sw_error_t
adpt_mrppe_pktedit_padding_get(a_uint32_t dev_id, fal_pktedit_padding_t *padding)
{
	union ipr_spare_reg_u ipr_spare_reg = {0};
	union eg_gen_ctrl_u eg_gen_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(padding);

	SW_RTN_ON_ERROR(mppe_eg_gen_ctrl_get(dev_id, &eg_gen_ctrl));
	SW_RTN_ON_ERROR(mrppe_ipr_spare_reg_get(dev_id, &ipr_spare_reg));

	/* strip padding */
	padding->strip_padding_en = eg_gen_ctrl.bf.strip_padding_en &&
					eg_gen_ctrl.bf.strip_padding_data_en;

	/* strip padding only for route */
	padding->strip_padding_route_en = eg_gen_ctrl.bf.strip_padding_route_en;

	/* strip padding only for bridge */
	padding->strip_padding_bridge_en = eg_gen_ctrl.bf.strip_padding_bridge_en;

	/* strip padding for tunnel decap innner */
	padding->strip_tunnel_inner_padding_en = eg_gen_ctrl.bf.strip_inner_padding_en &&
					ipr_spare_reg.bf.inner_ip_len_update_en;

	/* strip padding for snap */
	padding->strip_padding_snap_en = eg_gen_ctrl.bf.snap_len_update_en;

	/* payload checksum update by strip padding */
	padding->strip_padding_checksum_en = ipr_spare_reg.bf.payload_checksum_strip_padding_en;

	/* exception */
	padding->tunnel_inner_padding_exp_en = ipr_spare_reg.bf.inner_ip_padding_exp_en;
	padding->tunnel_ip_len_gap_exp_en = ipr_spare_reg.bf.inner_ip_len_gap_exp_en;

	return SW_OK;
}

sw_error_t adpt_mrppe_pktedit_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	ADPT_NULL_POINT_CHECK(p_adpt_api);

	p_adpt_api->adpt_pktedit_padding_set = adpt_mrppe_pktedit_padding_set;
	p_adpt_api->adpt_pktedit_padding_get = adpt_mrppe_pktedit_padding_get;
#if defined(JHPPE)
	p_adpt_api->adpt_pktedit_en_set = adpt_jhppe_pktedit_en_set;
	p_adpt_api->adpt_pktedit_en_get = adpt_jhppe_pktedit_en_get;
	p_adpt_api->adpt_rxdesc_cpucode_en_set = adpt_jhppe_rxdesc_cpucode_en_set;
	p_adpt_api->adpt_rxdesc_cpucode_en_get = adpt_jhppe_rxdesc_cpucode_en_get;
#endif
	return SW_OK;
}

/**
 * @}
 */
