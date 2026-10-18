/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw.h"
#include "hsl_htt_reg.h"
#include "fal_bm.h"
#include "adpt.h"

sw_error_t
adpt_httppe_port_bufgroup_map_get(a_uint32_t dev_id, fal_port_t port,
			a_uint8_t *group)
{
	sw_error_t rv = SW_OK;
	union port_group_id_u port_group_id;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(group);
	memset(&port_group_id, 0, sizeof(port_group_id));

	rv = httppe_port_group_id_get(dev_id, port, &port_group_id);
	if( rv != SW_OK )
		return rv;

	*group = port_group_id.bf.port_shared_group_id;

	return SW_OK;
}

sw_error_t
adpt_httppe_port_bufgroup_map_set(a_uint32_t dev_id, fal_port_t port,
			a_uint8_t group)
{
	sw_error_t rv = SW_OK;
	union port_group_id_u port_group_id;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&port_group_id, 0, sizeof(port_group_id));

	port_group_id.bf.port_shared_group_id = group;
	rv = httppe_port_group_id_set(dev_id, port, &port_group_id);
	if( rv != SW_OK )
		return rv;

	return SW_OK;
}

sw_error_t
adpt_httppe_bm_bufgroup_buffer_set(a_uint32_t dev_id, a_uint8_t group,
			a_uint16_t buff_num)
{
	sw_error_t rv = SW_OK;
	union shared_group_cfg_u shared_group_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&shared_group_cfg, 0, sizeof(shared_group_cfg));

	shared_group_cfg.bf.shared_group_limit = buff_num;
	rv = httppe_shared_group_cfg_set(dev_id, group, &shared_group_cfg);
	if( rv != SW_OK )
		return rv;

	return SW_OK;
}


sw_error_t
adpt_httppe_bm_port_reserved_buffer_get(a_uint32_t dev_id, fal_port_t port,
			a_uint16_t *prealloc_buff, a_uint16_t *react_buff)
{
	sw_error_t rv = SW_OK;
	union port_fc_cfg_u port_fc_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(prealloc_buff);
	ADPT_NULL_POINT_CHECK(react_buff);
	memset(&port_fc_cfg, 0, sizeof(port_fc_cfg));

	rv = httppe_port_fc_cfg_get(dev_id, port, &port_fc_cfg);
	if (rv)
		return rv;

	*prealloc_buff = port_fc_cfg.bf.port_pre_alloc;
	*react_buff = port_fc_cfg.bf.port_react_limit;

	return SW_OK;
}

sw_error_t
adpt_httppe_bm_bufgroup_buffer_get(a_uint32_t dev_id, a_uint8_t group,
			a_uint16_t *buff_num)
{
	sw_error_t rv = SW_OK;
	union shared_group_cfg_u shared_group_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(buff_num);
	memset(&shared_group_cfg, 0, sizeof(shared_group_cfg));

	rv = httppe_shared_group_cfg_get(dev_id, group, &shared_group_cfg);
	if( rv != SW_OK )
		return rv;

	*buff_num = shared_group_cfg.bf.shared_group_limit;

	return SW_OK;
}

sw_error_t
adpt_httppe_bm_port_dynamic_thresh_get(a_uint32_t dev_id, fal_port_t port,
			fal_bm_dynamic_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union port_fc_cfg_u port_fc_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);
	memset(&port_fc_cfg, 0, sizeof(port_fc_cfg));

	rv = httppe_port_fc_cfg_get(dev_id, port, &port_fc_cfg);
	if( rv != SW_OK )
		return rv;

	cfg->is_dynamic = port_fc_cfg.bf.port_shared_dynamic;
	cfg->weight = port_fc_cfg.bf.port_shared_weight;
	cfg->shared_ceiling = port_fc_cfg.bf.port_shared_ceiling_0;
	cfg->shared_ceiling |= port_fc_cfg.bf.port_shared_ceiling_1 <<
		SW_FIELD_OFFSET_IN_WORD(PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET);
	cfg->resume_off = port_fc_cfg.bf.port_resume_offset;
	cfg->resume_min_thresh = port_fc_cfg.bf.port_resume_floor_th;

	return SW_OK;
}

#ifndef IN_BM_MINI
sw_error_t
adpt_httppe_bm_port_static_thresh_get(a_uint32_t dev_id, fal_port_t port,
			fal_bm_static_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union port_fc_cfg_u port_fc_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);
	memset(&port_fc_cfg, 0, sizeof(port_fc_cfg));

	rv = httppe_port_fc_cfg_get(dev_id, port, &port_fc_cfg);
	if( rv != SW_OK )
		return rv;

	cfg->is_dynamic = port_fc_cfg.bf.port_shared_dynamic;

	cfg->resume_off = port_fc_cfg.bf.port_resume_offset;
	cfg->max_thresh = port_fc_cfg.bf.port_shared_ceiling_0;
	cfg->max_thresh |= port_fc_cfg.bf.port_shared_ceiling_1 <<
		SW_FIELD_OFFSET_IN_WORD(PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET);

	return SW_OK;
}

sw_error_t
adpt_httppe_bm_port_static_thresh_set(a_uint32_t dev_id, fal_port_t port,
			fal_bm_static_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union port_fc_cfg_u port_fc_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);
	memset(&port_fc_cfg, 0, sizeof(port_fc_cfg));

	rv = httppe_port_fc_cfg_get(dev_id, port, &port_fc_cfg);
	if( rv != SW_OK )
		return rv;

	port_fc_cfg.bf.port_resume_offset = cfg->resume_off;
	port_fc_cfg.bf.port_shared_ceiling_0 = cfg->max_thresh;
	port_fc_cfg.bf.port_shared_ceiling_1 = cfg->max_thresh >> SW_FIELD_OFFSET_IN_WORD(PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET);
	port_fc_cfg.bf.port_shared_dynamic = 0;

	return httppe_port_fc_cfg_set(dev_id, port, &port_fc_cfg);
}
#endif

sw_error_t
adpt_httppe_bm_port_reserved_buffer_set(a_uint32_t dev_id, fal_port_t port,
			a_uint16_t prealloc_buff, a_uint16_t react_buff)
{
	sw_error_t rv = SW_OK;
	union port_fc_cfg_u port_fc_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&port_fc_cfg, 0, sizeof(port_fc_cfg));

	rv = httppe_port_fc_cfg_get(dev_id, port, &port_fc_cfg);
	if (rv)
		return rv;

	port_fc_cfg.bf.port_pre_alloc = prealloc_buff;
	port_fc_cfg.bf.port_react_limit = react_buff;

	return httppe_port_fc_cfg_set(dev_id, port, &port_fc_cfg);
}

sw_error_t
adpt_httppe_bm_port_dynamic_thresh_set(a_uint32_t dev_id, fal_port_t port,
			fal_bm_dynamic_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union port_fc_cfg_u port_fc_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);
	memset(&port_fc_cfg, 0, sizeof(port_fc_cfg));

	rv = httppe_port_fc_cfg_get(dev_id, port, &port_fc_cfg);
	if( rv != SW_OK )
		return rv;

	port_fc_cfg.bf.port_shared_weight = cfg->weight;
	port_fc_cfg.bf.port_shared_ceiling_0 = cfg->shared_ceiling;
	port_fc_cfg.bf.port_shared_ceiling_1 = cfg->shared_ceiling >> SW_FIELD_OFFSET_IN_WORD(PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET);
	port_fc_cfg.bf.port_resume_offset = cfg->resume_off;
	port_fc_cfg.bf.port_resume_floor_th = cfg->resume_min_thresh;
	port_fc_cfg.bf.port_shared_dynamic = 1;

	return httppe_port_fc_cfg_set(dev_id, port, &port_fc_cfg);;
}

sw_error_t
adpt_httppe_bm_port_counter_get(a_uint32_t dev_id, fal_port_t port,
			fal_bm_port_counter_t *counter)
{
	sw_error_t rv = SW_OK;
	union port_cnt_u port_cnt;
	union port_reacted_cnt_u reacted_cnt;
	union drop_stat_u drop_stat;
	union port_fc_status_u port_fc_status;
	a_uint32_t index = FAL_PORT_ID_VALUE(port);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(counter);

	memset(&port_cnt, 0, sizeof(port_cnt));
	memset(&reacted_cnt, 0, sizeof(reacted_cnt));
	memset(&drop_stat, 0, sizeof(drop_stat));
	memset(&port_fc_status, 0, sizeof(port_fc_status));

	rv = httppe_port_cnt_get(dev_id, index, &port_cnt);
	SW_RTN_ON_ERROR(rv);

	counter->used_counter = port_cnt.bf.port_cnt;

	rv = httppe_port_reacted_cnt_get(dev_id, index, &reacted_cnt);
	SW_RTN_ON_ERROR(rv);

	counter->react_counter = reacted_cnt.bf.port_reacted_cnt;

	rv = httppe_drop_stat_get(dev_id, index, &drop_stat);
	SW_RTN_ON_ERROR(rv);

	counter->drop_byte_counter = drop_stat.bf.bytes_0 | ((a_uint64_t)drop_stat.bf.bytes_1 << 32);
	counter->drop_packet_counter = drop_stat.bf.pkts;

	rv = httppe_drop_stat_get(dev_id, index + 6, &drop_stat);
	SW_RTN_ON_ERROR(rv);

	counter->fc_drop_byte_counter = drop_stat.bf.bytes_0 | ((a_uint64_t)drop_stat.bf.bytes_1 << 32);
	counter->fc_drop_packet_counter = drop_stat.bf.pkts;

	rv = httppe_port_fc_status_get(dev_id, index, &port_fc_status);
	SW_RTN_ON_ERROR(rv);

	counter->on_thresh = port_fc_status.bf.port_xon_th;
	counter->fc_status = port_fc_status.bf.port_fc_status;

	return SW_OK;
}

/**
 * @}
 */
