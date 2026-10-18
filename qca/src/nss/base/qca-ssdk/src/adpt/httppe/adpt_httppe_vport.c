/*
* Copyright (c) 2021, The Linux Foundation. All rights reserved.
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "adpt.h"
#include "hsl_htt_reg.h"

sw_error_t
adpt_httppe_vport_physical_port_id_get(a_uint32_t dev_id,
		fal_port_t vport_id, fal_port_t *phyport_id)
{
	sw_error_t rv = SW_OK;
	a_uint32_t pport_id = 0;
	a_uint32_t vport_value = FAL_PORT_ID_VALUE(vport_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(phyport_id);

	rv = httppe_l2_vp_port_tbl_physical_port_get(dev_id, vport_value, &pport_id);
	SW_RTN_ON_ERROR(rv);

	*phyport_id = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pport_id);

	return rv;
}

sw_error_t
adpt_httppe_vport_physical_port_id_set(a_uint32_t dev_id,
		fal_port_t vport_id, fal_port_t phyport_id)
{
	sw_error_t rv = SW_OK;
	a_uint32_t pport_id = 0;
	a_uint32_t vport_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	vport_value = FAL_PORT_ID_VALUE(vport_id);
	pport_id = FAL_PORT_ID_VALUE(phyport_id);

	rv = httppe_l2_vp_port_tbl_physical_port_set(dev_id, vport_value, pport_id);
	return rv;
}

sw_error_t
adpt_httppe_vport_state_check_get(a_uint32_t dev_id, fal_port_t port_id, fal_vport_state_t *vp_state)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_post_tbl_u l2_vp_port_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vp_state);

	aos_mem_zero(&l2_vp_port_tbl, sizeof(l2_vp_port_tbl));

	rv = httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	vp_state->check_en = l2_vp_port_tbl.bf.vp_state_check_en;
	vp_state->vp_type = l2_vp_port_tbl.bf.vp_type;
	vp_state->vp_active = l2_vp_port_tbl.bf.vp_context_active;
	vp_state->eg_data_valid = l2_vp_port_tbl.bf.vp_eg_data_valid;

	return rv;
}

sw_error_t
adpt_httppe_vport_state_check_set(a_uint32_t dev_id, fal_port_t port_id, fal_vport_state_t *vp_state)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_post_tbl_u l2_vp_port_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vp_state);

	aos_mem_zero(&l2_vp_port_tbl, sizeof(l2_vp_port_tbl));

	rv = httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	l2_vp_port_tbl.bf.vp_state_check_en = vp_state->check_en;
	l2_vp_port_tbl.bf.vp_type = vp_state->vp_type;
	l2_vp_port_tbl.bf.vp_context_active = vp_state->vp_active;
	l2_vp_port_tbl.bf.vp_eg_data_valid = vp_state->eg_data_valid;

	return httppe_l2_vp_port_post_tbl_set(dev_id, port_value, &l2_vp_port_tbl);
}

/**
 * @}
 */
