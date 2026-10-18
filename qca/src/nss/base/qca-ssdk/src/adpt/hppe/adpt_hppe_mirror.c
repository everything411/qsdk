/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "adpt.h"
#if defined(HTTPPE)
#include "adpt_httppe_mirror.h"
#endif

sw_error_t
adpt_hppe_mirr_analysis_config_get(a_uint32_t dev_id, fal_mirr_direction_t direction, fal_mirr_analysis_config_t * config)
{
	union mirror_analyzer_u mirror_analyzer;
	union in_mirror_priority_ctrl_u in_mirror_priority_ctrl;
	union eg_mirror_priority_ctrl_u eg_mirror_priority_ctrl;

	memset(&mirror_analyzer, 0, sizeof(mirror_analyzer));
	memset(&in_mirror_priority_ctrl, 0, sizeof(in_mirror_priority_ctrl));
	memset(&eg_mirror_priority_ctrl, 0, sizeof(eg_mirror_priority_ctrl));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(config);

	hppe_mirror_analyzer_get(dev_id, &mirror_analyzer);
	hppe_in_mirror_priority_ctrl_get(dev_id, &in_mirror_priority_ctrl);
	hppe_eg_mirror_priority_ctrl_get(dev_id, &eg_mirror_priority_ctrl);

	if (direction == FAL_MIRR_BOTH)
	{
		if ((mirror_analyzer.bf.in_analyzer_port != mirror_analyzer.bf.eg_analyzer_port) ||
			(in_mirror_priority_ctrl.bf.priority != eg_mirror_priority_ctrl.bf.priority))
			return SW_FAIL;

		config->port_id = mirror_analyzer.bf.in_analyzer_port;
		config->priority = in_mirror_priority_ctrl.bf.priority;
	}
	else if (direction == FAL_MIRR_INGRESS)
	{
		config->port_id = mirror_analyzer.bf.in_analyzer_port;
		config->priority = in_mirror_priority_ctrl.bf.priority;
	}
	else if (direction == FAL_MIRR_EGRESS)
	{
		config->port_id = mirror_analyzer.bf.eg_analyzer_port;
		config->priority = eg_mirror_priority_ctrl.bf.priority;
	}
	else
		return SW_NOT_SUPPORTED;

	if (config->port_id == 32 || config->port_id == 33)
		config->port_id = FAL_PORT_ID(FAL_PORT_TYPE_TRUNK, config->port_id);

	return SW_OK;
}

sw_error_t
adpt_hppe_mirr_port_in_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	union port_mirror_u port_mirror;

	memset(&port_mirror, 0, sizeof(port_mirror));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (FAL_IS_PPORT(port_id)) {
		rv = hppe_port_mirror_get(dev_id, FAL_PORT_ID_VALUE(port_id), &port_mirror);
		SW_RTN_ON_ERROR(rv);

		*enable = port_mirror.bf.in_mirr_en;
	}

	if (FAL_IS_VPORT(port_id)) {
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_mirr_vp_in_get(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
#if defined(JHPPE)
			rv = appe_l2_vp_port_tbl_mirror_en_get(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
			SW_RTN_ON_ERROR(rv);
#else
			return SW_NOT_SUPPORTED;
#endif
		}
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_mirr_port_eg_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	union port_mirror_u port_mirror;

	memset(&port_mirror, 0, sizeof(port_mirror));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (FAL_IS_PPORT(port_id)) {
		rv = hppe_port_mirror_get(dev_id, FAL_PORT_ID_VALUE(port_id), &port_mirror);
		SW_RTN_ON_ERROR(rv);

		*enable = port_mirror.bf.eg_mirr_en;
	}

	if (FAL_IS_VPORT(port_id)) {
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_mirr_vp_eg_get(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
#if defined(JHPPE)
			rv = jhppe_l2_vp_port_post_tbl_mirror_en_get(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
			SW_RTN_ON_ERROR(rv);
#else
			return SW_NOT_SUPPORTED;
#endif
		}
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_mirr_analysis_port_get(a_uint32_t dev_id, fal_port_t * port_id)
{
	sw_error_t rv = SW_OK;
	union mirror_analyzer_u mirror_analyzer;

	memset(&mirror_analyzer, 0, sizeof(mirror_analyzer));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_id);

	rv = hppe_mirror_analyzer_get(dev_id, &mirror_analyzer);

	if( rv != SW_OK )
		return rv;

	if (mirror_analyzer.bf.in_analyzer_port != mirror_analyzer.bf.eg_analyzer_port)
		return SW_FAIL;

	*port_id = mirror_analyzer.bf.eg_analyzer_port;

	if (*port_id == 32 || *port_id == 33)
		*port_id = FAL_PORT_ID(FAL_PORT_TYPE_TRUNK, *port_id);

	return SW_OK;
}

sw_error_t
adpt_hppe_mirr_port_in_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union port_mirror_u port_mirror;
	fal_mirr_analysis_config_t analysis_cfg = {0};

	memset(&port_mirror, 0, sizeof(port_mirror));
	ADPT_DEV_ID_CHECK(dev_id);

	SW_RTN_ON_ERROR(adpt_hppe_mirr_analysis_config_get(dev_id, FAL_MIRR_INGRESS,
		&analysis_cfg));
	if(!(FAL_IS_PPORT(analysis_cfg.port_id) || FAL_IS_TRUNK(analysis_cfg.port_id)))
		return SW_BAD_VALUE;

	if (FAL_IS_PPORT(port_id)) {
		rv = hppe_port_mirror_get(dev_id, FAL_PORT_ID_VALUE(port_id), &port_mirror);
		SW_RTN_ON_ERROR(rv);
		port_mirror.bf.in_mirr_en = enable;

		rv = hppe_port_mirror_set(dev_id, FAL_PORT_ID_VALUE(port_id), &port_mirror);
		SW_RTN_ON_ERROR(rv);
	}

	if (FAL_IS_VPORT(port_id)) {
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_mirr_vp_in_set(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
#if defined(JHPPE)
			rv = appe_l2_vp_port_tbl_mirror_en_set(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
			SW_RTN_ON_ERROR(rv);
#else
			return SW_NOT_SUPPORTED;
#endif
		}
	}

	return SW_OK;
}
sw_error_t
adpt_hppe_mirr_port_eg_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union port_mirror_u port_mirror;
	fal_mirr_analysis_config_t analysis_cfg = {0};

	memset(&port_mirror, 0, sizeof(port_mirror));
	ADPT_DEV_ID_CHECK(dev_id);

	SW_RTN_ON_ERROR(adpt_hppe_mirr_analysis_config_get(dev_id, FAL_MIRR_EGRESS,
		&analysis_cfg));
	if(!(FAL_IS_PPORT(analysis_cfg.port_id) || FAL_IS_TRUNK(analysis_cfg.port_id)))
		return SW_BAD_VALUE;

	/* from Juhu Mirror is supported on VP */
	if (FAL_IS_PPORT(port_id)) {
		rv = hppe_port_mirror_get(dev_id, FAL_PORT_ID_VALUE(port_id), &port_mirror);
		SW_RTN_ON_ERROR(rv);
		port_mirror.bf.eg_mirr_en = enable;

		rv = hppe_port_mirror_set(dev_id, FAL_PORT_ID_VALUE(port_id), &port_mirror);
		SW_RTN_ON_ERROR(rv);
	}

	if (FAL_IS_VPORT(port_id)) {
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_mirr_vp_eg_set(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
#if defined(JHPPE)
			rv = jhppe_l2_vp_port_post_tbl_mirror_en_set(dev_id, FAL_PORT_ID_VALUE(port_id), enable);
			SW_RTN_ON_ERROR(rv);
#else
			return SW_NOT_SUPPORTED;
#endif
		}
	}

	return SW_OK;
}
sw_error_t
adpt_hppe_mirr_analysis_port_set(a_uint32_t dev_id, fal_port_t port_id)
{
	union mirror_analyzer_u mirror_analyzer;

	memset(&mirror_analyzer, 0, sizeof(mirror_analyzer));
	ADPT_DEV_ID_CHECK(dev_id);

	/* analysis port just support physical port and trunk port, not support port */
	if (FAL_PORT_ID_TYPE(port_id) != 0 && FAL_PORT_ID_TYPE(port_id) != 1)
		return SW_BAD_PARAM;

	port_id = FAL_PORT_ID_VALUE(port_id);

	hppe_mirror_analyzer_get(dev_id, &mirror_analyzer);

	mirror_analyzer.bf.in_analyzer_port = port_id;
	mirror_analyzer.bf.eg_analyzer_port = port_id;

	hppe_mirror_analyzer_set(dev_id, &mirror_analyzer);

	return SW_OK;
}

sw_error_t
adpt_hppe_mirr_analysis_config_set(a_uint32_t dev_id, fal_mirr_direction_t direction, fal_mirr_analysis_config_t * config)
{
	union mirror_analyzer_u mirror_analyzer;
	union in_mirror_priority_ctrl_u in_mirror_priority_ctrl;
	union eg_mirror_priority_ctrl_u eg_mirror_priority_ctrl;

	memset(&mirror_analyzer, 0, sizeof(mirror_analyzer));
	memset(&in_mirror_priority_ctrl, 0, sizeof(in_mirror_priority_ctrl));
	memset(&eg_mirror_priority_ctrl, 0, sizeof(eg_mirror_priority_ctrl));

	ADPT_DEV_ID_CHECK(dev_id);

	/* analysis port just support physical port and trunk port, not support port */
	if (FAL_PORT_ID_TYPE(config->port_id) != 0 && FAL_PORT_ID_TYPE(config->port_id) != 1)
		return SW_BAD_PARAM;

	config->port_id = FAL_PORT_ID_VALUE(config->port_id);

	hppe_mirror_analyzer_get(dev_id, &mirror_analyzer);

	if (direction == FAL_MIRR_BOTH)
	{
		mirror_analyzer.bf.in_analyzer_port = config->port_id;
		mirror_analyzer.bf.eg_analyzer_port = config->port_id;
		in_mirror_priority_ctrl.bf.priority = config->priority;
		eg_mirror_priority_ctrl.bf.priority = config->priority;
		hppe_mirror_analyzer_set(dev_id, &mirror_analyzer);
		hppe_in_mirror_priority_ctrl_set(dev_id, &in_mirror_priority_ctrl);
		hppe_eg_mirror_priority_ctrl_set(dev_id, &eg_mirror_priority_ctrl);
	}
	else if (direction == FAL_MIRR_INGRESS)
	{
		mirror_analyzer.bf.in_analyzer_port = config->port_id;
		in_mirror_priority_ctrl.bf.priority = config->priority;
		hppe_mirror_analyzer_set(dev_id, &mirror_analyzer);
		hppe_in_mirror_priority_ctrl_set(dev_id, &in_mirror_priority_ctrl);
	}
	else if (direction == FAL_MIRR_EGRESS)
	{
		mirror_analyzer.bf.eg_analyzer_port = config->port_id;
		eg_mirror_priority_ctrl.bf.priority = config->priority;
		hppe_mirror_analyzer_set(dev_id, &mirror_analyzer);
		hppe_eg_mirror_priority_ctrl_set(dev_id, &eg_mirror_priority_ctrl);
	}
	else
		return SW_NOT_SUPPORTED;

	return SW_OK;
}

sw_error_t adpt_hppe_mirror_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

	p_adpt_api->adpt_mirr_port_in_set = adpt_hppe_mirr_port_in_set;
	p_adpt_api->adpt_mirr_port_in_get = adpt_hppe_mirr_port_in_get;
	p_adpt_api->adpt_mirr_port_eg_set = adpt_hppe_mirr_port_eg_set;
	p_adpt_api->adpt_mirr_port_eg_get = adpt_hppe_mirr_port_eg_get;
	p_adpt_api->adpt_mirr_analysis_port_set = adpt_hppe_mirr_analysis_port_set;
	p_adpt_api->adpt_mirr_analysis_port_get = adpt_hppe_mirr_analysis_port_get;
	p_adpt_api->adpt_mirr_analysis_config_set = adpt_hppe_mirr_analysis_config_set;
	p_adpt_api->adpt_mirr_analysis_config_get = adpt_hppe_mirr_analysis_config_get;

	return SW_OK;
}
/**
 * @}
 */
