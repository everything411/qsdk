/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "adpt.h"
#include "hsl_htt_reg.h"

static fal_queue_bmp_t port_queue_map[6] = {0};

sw_error_t
adpt_httppe_l1_flow_map_set(a_uint32_t dev_id,
			    a_uint32_t node_id,
			    fal_port_t port_id,
			    fal_qos_scheduler_cfg_t *scheduler_cfg)
{
	union l1_flow_port_map_tbl_u l1_flow_port_map_tbl;
	union l1_flow_map_tbl_u l1_flow_map_tbl;
	union l1_comp_cfg_tbl_u l1_comp_cfg_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(scheduler_cfg);

	if (node_id >= L1_FLOW_MAP_TBL_MAX_ENTRY)
		return SW_BAD_PARAM;

	memset(&l1_flow_map_tbl, 0, sizeof(l1_flow_map_tbl));

	l1_flow_map_tbl.bf.e_drr_credit_unit = scheduler_cfg->e_drr_unit;
	l1_flow_map_tbl.bf.c_drr_credit_unit = scheduler_cfg->c_drr_unit;
	l1_flow_map_tbl.bf.e_drr_id = scheduler_cfg->e_drr_id;
	l1_flow_map_tbl.bf.c_drr_id_0 = scheduler_cfg->c_drr_id;
	l1_flow_map_tbl.bf.c_drr_id_1 = (scheduler_cfg->c_drr_id >>
				(SW_FIELD_OFFSET_IN_WORD(L1_FLOW_MAP_TBL_C_DRR_ID_OFFSET)));

	l1_flow_map_tbl.bf.e_drr_wt= scheduler_cfg->e_drr_wt;
	l1_flow_map_tbl.bf.c_drr_wt = scheduler_cfg->c_drr_wt;
	l1_flow_map_tbl.bf.e_pri = scheduler_cfg->e_pri;
	l1_flow_map_tbl.bf.c_pri = scheduler_cfg->c_pri;
	l1_flow_map_tbl.bf.sp_id = scheduler_cfg->sp_id;
	httppe_l1_flow_map_tbl_set(dev_id, node_id, &l1_flow_map_tbl);

	httppe_l1_flow_port_map_tbl_get(dev_id, node_id, &l1_flow_port_map_tbl);
	l1_flow_port_map_tbl.bf.port_num = port_id;
	httppe_l1_flow_port_map_tbl_set(dev_id, node_id, &l1_flow_port_map_tbl);

	httppe_l1_comp_cfg_tbl_get(dev_id, node_id, &l1_comp_cfg_tbl);
	l1_comp_cfg_tbl.bf.drr_meter_len = scheduler_cfg->drr_frame_mode;
	httppe_l1_comp_cfg_tbl_set(dev_id, node_id, &l1_comp_cfg_tbl);

	return SW_OK;
}

sw_error_t
adpt_httppe_l1_flow_map_get(a_uint32_t dev_id,
			    a_uint32_t node_id,
			    fal_port_t *port_id,
			    fal_qos_scheduler_cfg_t *scheduler_cfg)
{
	union l1_flow_map_tbl_u l1_flow_map_tbl;
	union l1_comp_cfg_tbl_u l1_comp_cfg_tbl;
	union l1_flow_port_map_tbl_u port_map;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(scheduler_cfg);

	if (node_id >= L1_FLOW_MAP_TBL_MAX_ENTRY)
		return SW_BAD_PARAM;

	memset(&l1_flow_map_tbl, 0, sizeof(l1_flow_map_tbl));

	httppe_l1_flow_map_tbl_get(dev_id, node_id, &l1_flow_map_tbl);
	scheduler_cfg->e_drr_wt = l1_flow_map_tbl.bf.e_drr_wt;
	scheduler_cfg->c_drr_wt = l1_flow_map_tbl.bf.c_drr_wt;
	scheduler_cfg->e_pri = l1_flow_map_tbl.bf.e_pri;
	scheduler_cfg->c_pri = l1_flow_map_tbl.bf.c_pri;
	scheduler_cfg->sp_id = l1_flow_map_tbl.bf.sp_id;
	scheduler_cfg->c_drr_id = (l1_flow_map_tbl.bf.c_drr_id_0 |
				l1_flow_map_tbl.bf.c_drr_id_1 << SW_FIELD_OFFSET_IN_WORD(L1_FLOW_MAP_TBL_C_DRR_ID_OFFSET));

	scheduler_cfg->e_drr_id = l1_flow_map_tbl.bf.e_drr_id;
	scheduler_cfg->c_drr_unit = l1_flow_map_tbl.bf.c_drr_credit_unit;
	scheduler_cfg->e_drr_unit = l1_flow_map_tbl.bf.e_drr_credit_unit;

	httppe_l1_flow_port_map_tbl_get(dev_id, node_id, &port_map);
	*port_id = port_map.bf.port_num;

	httppe_l1_comp_cfg_tbl_get(dev_id, node_id, &l1_comp_cfg_tbl);
	scheduler_cfg->drr_frame_mode = l1_comp_cfg_tbl.bf.drr_meter_len;

	return SW_OK;
}

sw_error_t
adpt_httppe_l0_queue_map_set(a_uint32_t dev_id,
			     a_uint32_t node_id,
			     fal_port_t port_id,
			     fal_qos_scheduler_cfg_t *scheduler_cfg)
{
	union l0_flow_port_map_tbl_u l0_flow_port_map_tbl;
	union l0_flow_map_tbl_u l0_flow_map_tbl;
	union l0_comp_cfg_tbl_u l0_comp_cfg_tbl;
	a_uint32_t i, j, k;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(scheduler_cfg);

	if (node_id >= L0_FLOW_MAP_TBL_MAX_ENTRY)
		return SW_BAD_PARAM;

	memset(&l0_flow_map_tbl, 0, sizeof(l0_flow_map_tbl));

	l0_flow_map_tbl.bf.e_drr_credit_unit = scheduler_cfg->e_drr_unit;
	l0_flow_map_tbl.bf.c_drr_credit_unit = scheduler_cfg->c_drr_unit;
	l0_flow_map_tbl.bf.e_drr_id = scheduler_cfg->e_drr_id;
	l0_flow_map_tbl.bf.c_drr_id = scheduler_cfg->c_drr_id;

	l0_flow_map_tbl.bf.e_drr_wt= scheduler_cfg->e_drr_wt;
	l0_flow_map_tbl.bf.c_drr_wt = scheduler_cfg->c_drr_wt;
	l0_flow_map_tbl.bf.e_pri = scheduler_cfg->e_pri;
	l0_flow_map_tbl.bf.c_pri = scheduler_cfg->c_pri;
	l0_flow_map_tbl.bf.sp_id = scheduler_cfg->sp_id;
	httppe_l0_flow_map_tbl_set(dev_id, node_id, &l0_flow_map_tbl);

	httppe_l0_flow_port_map_tbl_get(dev_id, node_id, &l0_flow_port_map_tbl);
	l0_flow_port_map_tbl.bf.port_num = port_id;
	httppe_l0_flow_port_map_tbl_set(dev_id, node_id, &l0_flow_port_map_tbl);

	httppe_l0_comp_cfg_tbl_get(dev_id, node_id, &l0_comp_cfg_tbl);
	l0_comp_cfg_tbl.bf.drr_meter_len = scheduler_cfg->drr_frame_mode;
	httppe_l0_comp_cfg_tbl_set(dev_id, node_id, &l0_comp_cfg_tbl);

	i = node_id / 32;
	j = node_id % 32;
	port_queue_map[port_id].bmp[i] |= 1 << j;

	for (k = 0; k < 6; k++) {
		if (k != port_id) {
			port_queue_map[k].bmp[i] &= ~(1 << j);
		}
	}

	return SW_OK;
}

sw_error_t
adpt_httppe_l0_queue_map_get(a_uint32_t dev_id,
			     a_uint32_t node_id,
			     fal_port_t *port_id,
			     fal_qos_scheduler_cfg_t *scheduler_cfg)
{
	union l0_flow_port_map_tbl_u l0_flow_port_map_tbl;
	union l0_flow_map_tbl_u l0_flow_map_tbl;
	union l0_comp_cfg_tbl_u l0_comp_cfg_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(scheduler_cfg);

	if (node_id >= L0_FLOW_MAP_TBL_MAX_ENTRY)
		return SW_BAD_PARAM;

	memset(&l0_flow_map_tbl, 0, sizeof(l0_flow_map_tbl));

	httppe_l0_flow_map_tbl_get(dev_id, node_id, &l0_flow_map_tbl);
	scheduler_cfg->e_drr_wt = l0_flow_map_tbl.bf.e_drr_wt;
	scheduler_cfg->c_drr_wt = l0_flow_map_tbl.bf.c_drr_wt;
	scheduler_cfg->e_pri = l0_flow_map_tbl.bf.e_pri;
	scheduler_cfg->c_pri = l0_flow_map_tbl.bf.c_pri;
	scheduler_cfg->sp_id = l0_flow_map_tbl.bf.sp_id;
	scheduler_cfg->c_drr_id = l0_flow_map_tbl.bf.c_drr_id;
	scheduler_cfg->e_drr_id = l0_flow_map_tbl.bf.e_drr_id;
	scheduler_cfg->c_drr_unit = l0_flow_map_tbl.bf.c_drr_credit_unit;
	scheduler_cfg->e_drr_unit = l0_flow_map_tbl.bf.e_drr_credit_unit;

	httppe_l0_flow_port_map_tbl_get(dev_id, node_id, &l0_flow_port_map_tbl);
	*port_id = l0_flow_port_map_tbl.bf.port_num;

	httppe_l0_comp_cfg_tbl_get(dev_id, node_id, &l0_comp_cfg_tbl);
	scheduler_cfg->drr_frame_mode = l0_comp_cfg_tbl.bf.drr_meter_len;

	return SW_OK;
}

sw_error_t
adpt_httppe_tdm_tick_num_set(a_uint32_t dev_id, a_uint32_t tick_num)
{
	union tdm_depth_cfg_u tdm_depth_cfg;

	ADPT_DEV_ID_CHECK(dev_id);

	tdm_depth_cfg.bf.tdm_depth = tick_num;
	return httppe_tdm_depth_cfg_set(dev_id, &tdm_depth_cfg);
}

sw_error_t
adpt_httppe_port_scheduler_cfg_set(a_uint32_t dev_id,
				a_uint32_t tick_index,
				fal_port_scheduler_cfg_t *cfg)
{
	union psch_tdm_cfg_tbl_u psch_tdm_cfg = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	psch_tdm_cfg.bf.ens_port_bitmap = cfg->en_scheduler_port_bmp;
	psch_tdm_cfg.bf.ens_port = cfg->en_scheduler_port;
	psch_tdm_cfg.bf.des_port = cfg->de_scheduler_port;
	psch_tdm_cfg.bf.des_sec_port_en = cfg->de_scheduler_2nd_port_en;
	psch_tdm_cfg.bf.des_sec_port = cfg->de_scheduler_2nd_port;

	return httppe_psch_tdm_cfg_tbl_set(dev_id, tick_index, &psch_tdm_cfg);
}

#ifndef IN_QOS_MINI
sw_error_t
adpt_httppe_port_queues_get(a_uint32_t dev_id,
			    fal_port_t port_id,
			    fal_queue_bmp_t *queue_bmp)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(queue_bmp);

	*queue_bmp = port_queue_map[port_id];

	return SW_OK;
}

sw_error_t
adpt_httppe_tdm_tick_num_get(a_uint32_t dev_id, a_uint32_t *tick_num)
{
	union tdm_depth_cfg_u tdm_depth_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(tick_num);

	httppe_tdm_depth_cfg_get(dev_id, &tdm_depth_cfg);
	*tick_num = tdm_depth_cfg.bf.tdm_depth;

	return SW_OK;
}

sw_error_t
adpt_httppe_port_scheduler_cfg_get(a_uint32_t dev_id,
				a_uint32_t tick_index,
				fal_port_scheduler_cfg_t *cfg)
{
	union psch_tdm_cfg_tbl_u psch_tdm_cfg;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	httppe_psch_tdm_cfg_tbl_get(dev_id, tick_index, &psch_tdm_cfg);
	cfg->en_scheduler_port_bmp = psch_tdm_cfg.bf.ens_port_bitmap;
	cfg->en_scheduler_port = psch_tdm_cfg.bf.ens_port;
	cfg->de_scheduler_port = psch_tdm_cfg.bf.des_port;
	cfg->de_scheduler_2nd_port_en = psch_tdm_cfg.bf.des_sec_port_en;
	cfg->de_scheduler_2nd_port = psch_tdm_cfg.bf.des_sec_port;

	return SW_OK;
}
#endif

#define HTTPPE_QOS_MAPPING_DSCP_TBL_MAX_ENTRY	256
#define HTTPPE_QOS_MAPPING_PCP_TBL_MAX_ENTRY	16
#define HTTPPE_QOS_MAPPING_TBL_MAX_GROUP	2

static sw_error_t
adpt_httppe_qos_mapping_get(a_uint32_t dev_id, a_uint32_t index,
			fal_qos_cosmap_t *cosmap)
{
	sw_error_t rv = SW_OK;
	union qos_mapping_tbl_u qos_mapping_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cosmap);

	rv = httppe_qos_mapping_tbl_get(dev_id, index, &qos_mapping_tbl);
	if (rv != SW_OK)
		return rv;

	cosmap->internal_pcp = qos_mapping_tbl.bf.int_pcp;
	cosmap->internal_dei = qos_mapping_tbl.bf.int_dei;
	cosmap->internal_pri = qos_mapping_tbl.bf.int_pri;
	cosmap->internal_dscp = qos_mapping_tbl.bf.int_dscp_tc;
	cosmap->internal_dp = qos_mapping_tbl.bf.int_dp;
	cosmap->dscp_mask = qos_mapping_tbl.bf.dscp_tc_mask;
	cosmap->dscp_en = qos_mapping_tbl.bf.int_dscp_en;
	cosmap->pcp_en = qos_mapping_tbl.bf.int_pcp_en;
	cosmap->dei_en = qos_mapping_tbl.bf.int_dei_en;
	cosmap->pri_en = qos_mapping_tbl.bf.int_pri_en;
	cosmap->dp_en = qos_mapping_tbl.bf.int_dp_en;
	cosmap->qos_prec = qos_mapping_tbl.bf.qos_res_prec_0 |
			     qos_mapping_tbl.bf.qos_res_prec_1 << 1;

	return SW_OK;
}

static sw_error_t
adpt_httppe_qos_mapping_set(a_uint32_t dev_id, a_uint32_t index,
			fal_qos_cosmap_t *cosmap)
{
	sw_error_t rv = SW_OK;
	union qos_mapping_tbl_u qos_mapping_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cosmap);

	memset(&qos_mapping_tbl, 0, sizeof(qos_mapping_tbl));
	rv = httppe_qos_mapping_tbl_get(dev_id, index, &qos_mapping_tbl);
	if (rv != SW_OK)
		return rv;

	qos_mapping_tbl.bf.int_pcp = cosmap->internal_pcp;
	qos_mapping_tbl.bf.int_dei = cosmap->internal_dei;
	qos_mapping_tbl.bf.int_pri = cosmap->internal_pri;
	qos_mapping_tbl.bf.int_dscp_tc = cosmap->internal_dscp;
	qos_mapping_tbl.bf.int_dp = cosmap->internal_dp;
	qos_mapping_tbl.bf.dscp_tc_mask = cosmap->dscp_mask;
	qos_mapping_tbl.bf.int_dscp_en = cosmap->dscp_en;
	qos_mapping_tbl.bf.int_pcp_en = cosmap->pcp_en;
	qos_mapping_tbl.bf.int_dei_en = cosmap->dei_en;
	qos_mapping_tbl.bf.int_pri_en = cosmap->pri_en;
	qos_mapping_tbl.bf.int_dp_en = cosmap->dp_en;
	qos_mapping_tbl.bf.qos_res_prec_0 = cosmap->qos_prec & 1;
	qos_mapping_tbl.bf.qos_res_prec_1 = (cosmap->qos_prec >> 1) & 3;

	return httppe_qos_mapping_tbl_set(dev_id, index, &qos_mapping_tbl);
}

sw_error_t
adpt_httppe_qos_cosmap_pcp_get(a_uint32_t dev_id, a_uint8_t group_id,
			a_uint8_t pcp,
			fal_qos_cosmap_t *cosmap)
{
	a_uint32_t index = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cosmap);

	if (group_id >= HTTPPE_QOS_MAPPING_TBL_MAX_GROUP)
		return SW_BAD_PARAM;

	index = HTTPPE_QOS_MAPPING_TBL_MAX_GROUP * HTTPPE_QOS_MAPPING_DSCP_TBL_MAX_ENTRY +
		group_id * HTTPPE_QOS_MAPPING_PCP_TBL_MAX_ENTRY + pcp;

	if (index >= QOS_MAPPING_TBL_NUM)
		return SW_OUT_OF_RANGE;

	return adpt_httppe_qos_mapping_get(dev_id, index, cosmap);
}

sw_error_t
adpt_httppe_qos_cosmap_pcp_set(a_uint32_t dev_id, a_uint8_t group_id,
			a_uint8_t pcp,
			fal_qos_cosmap_t *cosmap)
{
	a_uint32_t index = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cosmap);

	if (group_id >= HTTPPE_QOS_MAPPING_TBL_MAX_GROUP)
		return SW_BAD_PARAM;

	index = HTTPPE_QOS_MAPPING_TBL_MAX_GROUP * HTTPPE_QOS_MAPPING_DSCP_TBL_MAX_ENTRY +
		group_id * HTTPPE_QOS_MAPPING_PCP_TBL_MAX_ENTRY + pcp;

	if (index >= QOS_MAPPING_TBL_NUM)
		return SW_OUT_OF_RANGE;

	return adpt_httppe_qos_mapping_set(dev_id, index, cosmap);
}

sw_error_t
adpt_httppe_qos_cosmap_dscp_get(a_uint32_t dev_id, a_uint8_t group_id,
			a_uint8_t dscp,
			fal_qos_cosmap_t *cosmap)
{
	a_uint32_t index = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cosmap);

	if (group_id >= HTTPPE_QOS_MAPPING_TBL_MAX_GROUP)
		return SW_BAD_PARAM;

	index = group_id * HTTPPE_QOS_MAPPING_DSCP_TBL_MAX_ENTRY + dscp;

	return adpt_httppe_qos_mapping_get(dev_id, index, cosmap);
}

sw_error_t
adpt_httppe_qos_cosmap_dscp_set(a_uint32_t dev_id, a_uint8_t group_id,
			a_uint8_t dscp,
			fal_qos_cosmap_t *cosmap)
{
	a_uint32_t index = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cosmap);

	if (group_id >= HTTPPE_QOS_MAPPING_TBL_MAX_GROUP)
		return SW_BAD_PARAM;

	index = group_id * HTTPPE_QOS_MAPPING_DSCP_TBL_MAX_ENTRY + dscp;

	return adpt_httppe_qos_mapping_set(dev_id, index, cosmap);
}

sw_error_t
adpt_httppe_qos_port_pri_set(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_pri_precedence_t *pri)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pri);

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if (rv != SW_OK)
		return rv;

	mru_mtu_ctrl.bf.pcp_res_prec = pri->pcp_pri;
	mru_mtu_ctrl.bf.dscp_res_prec = pri->dscp_pri;
	mru_mtu_ctrl.bf.preheader_res_prec = pri->preheader_pri;
	mru_mtu_ctrl.bf.flow_res_prec = pri->flow_pri;
	mru_mtu_ctrl.bf.pre_acl_res_prec = pri->acl_pri;
	mru_mtu_ctrl.bf.post_acl_res_prec = pri->post_acl_pri;
	mru_mtu_ctrl.bf.pcp_res_prec_force = pri->pcp_pri_force;
	mru_mtu_ctrl.bf.dscp_res_prec_force = pri->dscp_pri_force;
	mru_mtu_ctrl.bf.pre_ipo_outer_res_prec = pri->pre_acl_outer_pri;
	mru_mtu_ctrl.bf.pre_ipo_inner_res_prec_1 = pri->pre_acl_inner_pri >>
		SW_FIELD_OFFSET_IN_WORD(MRU_MTU_CTRL_TBL_PRE_IPO_INNER_RES_PREC_OFFSET);
	mru_mtu_ctrl.bf.pre_ipo_inner_res_prec_0 = pri->pre_acl_inner_pri;

	return httppe_mru_mtu_ctrl_tbl_set(dev_id, port_id, &mru_mtu_ctrl);
}

sw_error_t
adpt_httppe_qos_port_pri_get(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_pri_precedence_t *pri)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pri);

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if( rv != SW_OK )
		return rv;

	pri->pcp_pri = mru_mtu_ctrl.bf.pcp_res_prec;
	pri->dscp_pri = mru_mtu_ctrl.bf.dscp_res_prec;
	pri->preheader_pri = mru_mtu_ctrl.bf.preheader_res_prec;
	pri->flow_pri = mru_mtu_ctrl.bf.flow_res_prec;
	pri->acl_pri = mru_mtu_ctrl.bf.pre_acl_res_prec;
	pri->post_acl_pri = mru_mtu_ctrl.bf.post_acl_res_prec;
	pri->pcp_pri_force = mru_mtu_ctrl.bf.pcp_res_prec_force;
	pri->dscp_pri_force = mru_mtu_ctrl.bf.dscp_res_prec_force;
	pri->pre_acl_outer_pri = mru_mtu_ctrl.bf.pre_ipo_outer_res_prec;
	pri->pre_acl_inner_pri = mru_mtu_ctrl.bf.pre_ipo_inner_res_prec_1 <<
		SW_FIELD_OFFSET_IN_WORD(MRU_MTU_CTRL_TBL_PRE_IPO_INNER_RES_PREC_OFFSET) |
		mru_mtu_ctrl.bf.pre_ipo_inner_res_prec_0;

	return SW_OK;
}

sw_error_t
adpt_httppe_qos_port_group_set(a_uint32_t dev_id, fal_port_t port_id,
			fal_qos_group_t *group)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(group);

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if( rv != SW_OK )
		return rv;

	mru_mtu_ctrl.bf.pcp_qos_group_id = group->pcp_group;
	mru_mtu_ctrl.bf.dscp_qos_group_id = group->dscp_group;

	return httppe_mru_mtu_ctrl_tbl_set(dev_id, port_id, &mru_mtu_ctrl);
}

sw_error_t
adpt_httppe_qos_port_group_get(a_uint32_t dev_id, fal_port_t port_id,
			fal_qos_group_t *group)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(group);

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if( rv != SW_OK )
		return rv;

	group->pcp_group = mru_mtu_ctrl.bf.pcp_qos_group_id;
	group->dscp_group = mru_mtu_ctrl.bf.dscp_qos_group_id;

	return SW_OK;
}

sw_error_t
adpt_httppe_qos_port_pcp_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
			fal_qos_pcp_cfg_t *pcp_cfg)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pcp_cfg);

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if( rv != SW_OK )
		return rv;

	mru_mtu_ctrl.bf.pcp_qos_mode = pcp_cfg->pcp_mode;
	mru_mtu_ctrl.bf.default_pcp_dei = pcp_cfg->default_pcp_dei;

	return httppe_mru_mtu_ctrl_tbl_set(dev_id, port_id, &mru_mtu_ctrl);
}

sw_error_t
adpt_httppe_qos_port_pcp_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
			fal_qos_pcp_cfg_t *pcp_cfg)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pcp_cfg);

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if( rv != SW_OK )
		return rv;

	pcp_cfg->pcp_mode = mru_mtu_ctrl.bf.pcp_qos_mode;
	pcp_cfg->default_pcp_dei = mru_mtu_ctrl.bf.default_pcp_dei;

	return SW_OK;
}


