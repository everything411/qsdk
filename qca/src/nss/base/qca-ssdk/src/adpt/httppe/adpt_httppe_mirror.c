/*
* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw.h"
#include "adpt.h"
#include "hsl_htt_reg.h"

sw_error_t
adpt_httppe_mirr_vp_in_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t *enable)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (!FAL_IS_VPORT(port_id)) {
		return SW_BAD_VALUE;
	}

	rv = httppe_l2_vp_port_tbl_mirror_en_get(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
	return rv;
}

sw_error_t
adpt_httppe_mirr_vp_eg_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t *enable)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (!FAL_IS_VPORT(port_id)) {
		return SW_BAD_VALUE;
	}

	rv = httppe_l2_vp_port_post_tbl_mirror_en_get(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
	return rv;
}

sw_error_t
adpt_httppe_mirr_vp_in_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t enable)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!FAL_IS_VPORT(port_id)) {
		return SW_BAD_VALUE;
	}

	rv = httppe_l2_vp_port_tbl_mirror_en_set(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
	return rv;
}

sw_error_t
adpt_httppe_mirr_vp_eg_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t enable)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!FAL_IS_VPORT(port_id)) {
		return SW_BAD_VALUE;
	}

	rv = httppe_l2_vp_port_post_tbl_mirror_en_set(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
	return rv;
}

