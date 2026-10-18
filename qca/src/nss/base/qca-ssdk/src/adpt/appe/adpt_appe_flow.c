/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

sw_error_t
adpt_appe_flow_copy_escape_set(a_uint32_t dev_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union l2_global_conf_u l2_global_conf;
	union l3_route_ctrl_ext_u l3_route_ctrl_ext;

	ADPT_DEV_ID_CHECK(dev_id);

	memset(&l2_global_conf, 0, sizeof(l2_global_conf));
	rv = hppe_l2_global_conf_get(dev_id, &l2_global_conf);
	SW_RTN_ON_ERROR(rv);
	l2_global_conf.bf.flow_cpy_escape = enable;
	rv = hppe_l2_global_conf_set(dev_id, &l2_global_conf);
	SW_RTN_ON_ERROR(rv);

	memset(&l3_route_ctrl_ext, 0, sizeof(l3_route_ctrl_ext));
	rv = hppe_l3_route_ctrl_ext_get(dev_id, &l3_route_ctrl_ext);
	SW_RTN_ON_ERROR(rv);
	l3_route_ctrl_ext.bf.flow_cpy_escape = enable;
	rv = hppe_l3_route_ctrl_ext_set(dev_id, &l3_route_ctrl_ext);
	return rv;
}

sw_error_t
adpt_appe_flow_copy_escape_get(a_uint32_t dev_id, a_bool_t *enable)
{

	sw_error_t rv = SW_OK;
	union l2_global_conf_u l2_global_conf;
	union l3_route_ctrl_ext_u l3_route_ctrl_ext;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	memset(&l2_global_conf, 0, sizeof(l2_global_conf));
	rv = hppe_l2_global_conf_get(dev_id, &l2_global_conf);
	SW_RTN_ON_ERROR(rv);

	memset(&l3_route_ctrl_ext, 0, sizeof(l3_route_ctrl_ext));
	rv = hppe_l3_route_ctrl_ext_get(dev_id, &l3_route_ctrl_ext);
	SW_RTN_ON_ERROR(rv);

	*enable = l2_global_conf.bf.flow_cpy_escape & l3_route_ctrl_ext.bf.flow_cpy_escape;
	return rv;
}

/**
 * @}
 */
