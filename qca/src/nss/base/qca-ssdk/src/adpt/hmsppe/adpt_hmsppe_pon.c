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
#include "adpt.h"

sw_error_t
adpt_hmsppe_pon_gemport_global_set(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg)
{
	sw_error_t ret = SW_OK;
	union l2_global_conf_u reg_val;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(ret);

	reg_val.bf.dot1p_mapper_vlan_mode = cfg->vlan_mode;
	reg_val.bf.dot1p_mapper_pcp_mode = cfg->pcp_mode;

	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	SW_RTN_ON_ERROR(ret);

	ret = hmsppe_dot1p_mapper_default_miss_action_set(dev_id, cfg->gen_miss_cmd);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_mc_enq_ctrl_dot1p_pon_vp_set(dev_id, cfg->gen_miss_pon_port);
	SW_RTN_ON_ERROR(ret);

	/* egress PON port configuration */
	ret = hppe_eg_global_ctrl_pm_port_bitmap_for_gem_set(dev_id, BIT(PON_PORT_ID));
	SW_RTN_ON_ERROR(ret);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_global_get(a_uint32_t dev_id, fal_gemport_global_cfg_t *cfg)
{
	sw_error_t ret = SW_OK;
	union l2_global_conf_u reg_val;
	a_uint32_t tmp = 0;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(ret);

	cfg->vlan_mode = reg_val.bf.dot1p_mapper_vlan_mode;
	cfg->pcp_mode = reg_val.bf.dot1p_mapper_pcp_mode;

	ret = hmsppe_dot1p_mapper_default_miss_action_get(dev_id, &cfg->gen_miss_cmd);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_mc_enq_ctrl_dot1p_pon_vp_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(ret);
	cfg->gen_miss_pon_port = tmp;

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_gen_default_set(a_uint32_t dev_id,
				fal_gemport_gen_default_t *def_property)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_default_u reg_val;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(ret);

	reg_val.bf.vid = def_property->vlan_id;
	reg_val.bf.pcp = def_property->pcp;
	reg_val.bf.dei = def_property->dei;
	reg_val.bf.dscp = def_property->dscp;
	reg_val.bf.dscp_mask = def_property->dscp_mask;

	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	SW_RTN_ON_ERROR(ret);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_gen_default_get(a_uint32_t dev_id,
				fal_gemport_gen_default_t *def_property)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_default_u reg_val;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(ret);

	def_property->vlan_id = reg_val.bf.vid;
	def_property->pcp = reg_val.bf.pcp;
	def_property->dei = reg_val.bf.dei;
	def_property->dscp = reg_val.bf.dscp;
	def_property->dscp_mask = reg_val.bf.dscp_mask;

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_gen_entry_set(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_rule_u reg_val;

	fal_gemport_gen_t tmp_rule;
	memset(&tmp_rule, 0, sizeof(tmp_rule));

	if (!memcmp(&tmp_rule, gen_entry, sizeof(*gen_entry))) {
		/* all zero parameter used to invalid the entry */
		memset(&reg_val, 0, sizeof(reg_val));

		return hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	}

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	SW_RTN_ON_ERROR(ret);

	reg_val.bf.valid = 1;
	reg_val.bf.src_incl = gen_entry->src_info_valid;
	reg_val.bf.port_type = adpt_port_type_convert(A_TRUE, FAL_PORT_ID_TYPE(gen_entry->src_info));
	reg_val.bf.src_info = FAL_PORT_ID_VALUE(gen_entry->src_info);
	reg_val.bf.dest_incl = gen_entry->dest_info_valid;
	if (gen_entry->dest_info_valid && gen_entry->dest_info.dest_info_type != FAL_DEST_INFO_PORT_ID) {
		SSDK_ERROR("dest_info_type: %#x is not supported\n", gen_entry->dest_info.dest_info_type);
		return SW_NOT_SUPPORTED;
	}
	reg_val.bf.dest_info = gen_entry->dest_info.dest_info_value;
	reg_val.bf.vid_incl = gen_entry->vlan_id_valid;
	reg_val.bf.vid_0 = gen_entry->vlan_id &
		(((a_uint32_t)1 << SW_FIELD_OFFSET_IN_WORD(DOT1P_MAPPER_RULE_VID_OFFSET)) - 1);
	reg_val.bf.vid_1 = gen_entry->vlan_id >> SW_FIELD_OFFSET_IN_WORD(DOT1P_MAPPER_RULE_VID_OFFSET);
	reg_val.bf.priority_type = gen_entry->pri_type;
	if (gen_entry->pri_type == 0) {
		reg_val.bf.pcp_incl = gen_entry->pcp_valid;
		reg_val.bf.pcp = gen_entry->pcp;
		reg_val.bf.dei_incl = gen_entry->dei_valid;
		reg_val.bf.dei = gen_entry->dei;
	} else {
		reg_val.bf1.dscp_incl = gen_entry->dscp_valid;
		reg_val.bf1.dscp = gen_entry->dscp;
	}
	reg_val.bf.gem_port = gen_entry->gemport;

	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	SW_RTN_ON_ERROR(ret);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_gen_entry_get(a_uint32_t dev_id, a_uint32_t index, fal_gemport_gen_t *gen_entry)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_rule_u reg_val;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	SW_RTN_ON_ERROR(ret);

	if (!reg_val.bf.valid)
		return SW_NOT_FOUND;

	gen_entry->src_info_valid = reg_val.bf.src_incl;
	gen_entry->src_info = FAL_PORT_ID(adpt_port_type_convert(A_FALSE, reg_val.bf.port_type),
		reg_val.bf.src_info);
	gen_entry->dest_info_valid = reg_val.bf.dest_incl;
	gen_entry->dest_info.dest_info_type = (reg_val.bf.dest_incl) ? FAL_DEST_INFO_PORT_ID : 0;
	gen_entry->dest_info.dest_info_value = reg_val.bf.dest_info;
	gen_entry->vlan_id_valid = reg_val.bf.vid_incl;
	gen_entry->vlan_id = (reg_val.bf.vid_1 << SW_FIELD_OFFSET_IN_WORD(DOT1P_MAPPER_RULE_VID_OFFSET)) | reg_val.bf.vid_0;
	gen_entry->pri_type = reg_val.bf.priority_type;
	if (reg_val.bf.priority_type == 0) {
		gen_entry->pcp_valid = reg_val.bf.pcp_incl;
		gen_entry->pcp = reg_val.bf.pcp;
		gen_entry->dei_valid = reg_val.bf.dei_incl;
		gen_entry->dei = reg_val.bf.dei;
	} else {
		gen_entry->dscp_valid = reg_val.bf1.dscp_incl;
		gen_entry->dscp = reg_val.bf1.dscp;
	}
	gen_entry->gemport = reg_val.bf.gem_port;

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_gen_en_set(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t enable)
{
	return hmsppe_dot1p_mapper_action_gem_port_en_set(dev_id, gemport, enable);
}

sw_error_t
adpt_hmsppe_pon_gemport_gen_en_get(a_uint32_t dev_id, a_uint32_t gemport, a_bool_t *enable)
{
	return hmsppe_dot1p_mapper_action_gem_port_en_get(dev_id, gemport, enable);
}

sw_error_t
adpt_hmsppe_pon_gemport_map_en_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t mapping_en)
{
	return hmsppe_gem_port_mapping_ctrl_mapping_en_set(dev_id, FAL_PORT_ID_VALUE(port_id), mapping_en);
}

sw_error_t
adpt_hmsppe_pon_gemport_map_en_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *mapping_en)
{
	return hmsppe_gem_port_mapping_ctrl_mapping_en_get(dev_id, FAL_PORT_ID_VALUE(port_id), mapping_en);
}

sw_error_t
adpt_hmsppe_pon_gemport_map_set(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_map_t *map_entry)
{
	sw_error_t ret = SW_OK;
	union gem_port_mapping_tbl_u reg_val;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	reg_val.bf.port_valid = map_entry->src_en;
	reg_val.bf.port_vp = map_entry->src_port;
	reg_val.bf.dst_info_valid = map_entry->dest_en;
	if (map_entry->dest_en &&
		(map_entry->dest_info.dest_info_type != FAL_DEST_INFO_PORT_ID &&
		map_entry->dest_info.dest_info_type != FAL_DEST_INFO_PORT_BMP)) {
		SSDK_ERROR("dest_info_type: %#x is not supported\n", map_entry->dest_info.dest_info_type);
		return SW_NOT_SUPPORTED;
	}
	reg_val.bf.dst_info = ADPT_DEST_INFO(adpt_dest_type_convert(A_TRUE, map_entry->dest_info.dest_info_type),
		map_entry->dest_info.dest_info_value);
	reg_val.bf.int_pri_dp_valid = map_entry->int_pri_dp_en;
	reg_val.bf.int_pri = map_entry->int_pri;
	reg_val.bf.int_dp = map_entry->int_dp;
	reg_val.bf.service_code_0 = map_entry->service_code &
		(((a_uint32_t)SW_FIELD_OFFSET_IN_WORD(GEM_PORT_MAPPING_TBL_SERVICE_CODE_OFFSET) << 1) - 1);
	reg_val.bf.service_code_1 = map_entry->service_code >>
		SW_FIELD_OFFSET_IN_WORD(GEM_PORT_MAPPING_TBL_SERVICE_CODE_OFFSET);

	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_map_get(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_map_t *map_entry)
{
	sw_error_t ret = SW_OK;
	union gem_port_mapping_tbl_u reg_val;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	map_entry->src_en = reg_val.bf.port_valid;
	map_entry->src_port = reg_val.bf.port_vp;
	map_entry->dest_en = reg_val.bf.dst_info_valid;
	map_entry->dest_info.dest_info_type = adpt_dest_type_convert(A_FALSE, ADPT_DEST_TYPE(reg_val.bf.dst_info));
	map_entry->dest_info.dest_info_value = ADPT_DEST_VAL(reg_val.bf.dst_info);
	map_entry->int_pri_dp_en = reg_val.bf.int_pri_dp_valid;
	map_entry->int_pri = reg_val.bf.int_pri;
	map_entry->int_dp = reg_val.bf.int_dp;
	map_entry->service_code = (reg_val.bf.service_code_1 <<
		SW_FIELD_OFFSET_IN_WORD(GEM_PORT_MAPPING_TBL_SERVICE_CODE_OFFSET)) | reg_val.bf.service_code_0;

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_cfg_set(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_cfg_t *cfg)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_action_u reg_val;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	reg_val.bf.service_code_en = cfg->service_code_en;
	reg_val.bf.service_code = cfg->service_code;
	reg_val.bf.int_pri_en = cfg->int_pri_en;
	reg_val.bf.int_pri = cfg->int_pri;
	reg_val.bf.int_dp_en = cfg->int_dp_en;
	reg_val.bf.int_dp = cfg->int_dp;
	reg_val.bf.enqueue_vp_valid = cfg->enq_vp_en;
	reg_val.bf.enqueue_vp_0 = cfg->enq_vp &
		(((a_uint32_t)1 << SW_FIELD_OFFSET_IN_WORD(DOT1P_MAPPER_ACTION_ENQUEUE_VP_OFFSET)) - 1);
	reg_val.bf.enqueue_vp_1 = cfg->enq_vp >> SW_FIELD_OFFSET_IN_WORD(DOT1P_MAPPER_ACTION_ENQUEUE_VP_OFFSET);
	reg_val.bf.eg_port_override = cfg->dest_en;
	reg_val.bf.eg_port_vp = cfg->dest_vp;
	reg_val.bf.eg_phy_port = cfg->dest_pp;
	reg_val.bf.action = cfg->fwd_cmd;
	reg_val.bf.post_bypass_bitmap = cfg->bypass_bitmap;

	ret = hmsppe_dot1p_mapper_action_set(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_cfg_get(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_cfg_t *cfg)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_action_u reg_val;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	cfg->service_code_en = reg_val.bf.service_code_en;
	cfg->service_code = reg_val.bf.service_code;
	cfg->int_pri_en = reg_val.bf.int_pri_en;
	cfg->int_pri = reg_val.bf.int_pri;
	cfg->int_dp_en = reg_val.bf.int_dp_en;
	cfg->int_dp = reg_val.bf.int_dp;
	cfg->enq_vp_en = reg_val.bf.enqueue_vp_valid;
	cfg->enq_vp = ((a_uint32_t)(reg_val.bf.enqueue_vp_1 <<
		SW_FIELD_OFFSET_IN_WORD(DOT1P_MAPPER_ACTION_ENQUEUE_VP_OFFSET))) |
		reg_val.bf.enqueue_vp_0;
	cfg->dest_en = reg_val.bf.eg_port_override;
	cfg->dest_vp = reg_val.bf.eg_port_vp;
	cfg->dest_pp = reg_val.bf.eg_phy_port;
	cfg->fwd_cmd = reg_val.bf.action;
	cfg->bypass_bitmap = reg_val.bf.post_bypass_bitmap;

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_policer_set(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_policer_t *cfg)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_action_u reg_val;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	reg_val.bf.policer_en = cfg->us_policer_en;
	reg_val.bf.policer_id = cfg->us_policer_idx;
	reg_val.bf.policer_ds_en = cfg->ds_policer_en;

	ret = hmsppe_dot1p_mapper_action_set(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_pon_gemport_policer_get(a_uint32_t dev_id, a_uint32_t gemport,
				fal_gemport_policer_t *cfg)
{
	sw_error_t ret = SW_OK;
	union dot1p_mapper_action_u reg_val;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, gemport, &reg_val);
	SW_RTN_ON_ERROR(ret);

	cfg->us_policer_en = reg_val.bf.policer_en;
	cfg->us_policer_idx = reg_val.bf.policer_id;
	cfg->ds_policer_en = reg_val.bf.policer_ds_en;

	return SW_OK;
}

sw_error_t adpt_hmsppe_pon_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	ADPT_NULL_POINT_CHECK(p_adpt_api);

	p_adpt_api->adpt_pon_gemport_global_set = adpt_hmsppe_pon_gemport_global_set;
	p_adpt_api->adpt_pon_gemport_global_get = adpt_hmsppe_pon_gemport_global_get;

	p_adpt_api->adpt_pon_gemport_gen_default_set = adpt_hmsppe_pon_gemport_gen_default_set;
	p_adpt_api->adpt_pon_gemport_gen_default_get = adpt_hmsppe_pon_gemport_gen_default_get;

	p_adpt_api->adpt_pon_gemport_gen_entry_set = adpt_hmsppe_pon_gemport_gen_entry_set;
	p_adpt_api->adpt_pon_gemport_gen_entry_get = adpt_hmsppe_pon_gemport_gen_entry_get;

	p_adpt_api->adpt_pon_gemport_gen_en_set = adpt_hmsppe_pon_gemport_gen_en_set;
	p_adpt_api->adpt_pon_gemport_gen_en_get = adpt_hmsppe_pon_gemport_gen_en_get;

	p_adpt_api->adpt_pon_gemport_map_en_set = adpt_hmsppe_pon_gemport_map_en_set;
	p_adpt_api->adpt_pon_gemport_map_en_get = adpt_hmsppe_pon_gemport_map_en_get;

	p_adpt_api->adpt_pon_gemport_map_set = adpt_hmsppe_pon_gemport_map_set;
	p_adpt_api->adpt_pon_gemport_map_get = adpt_hmsppe_pon_gemport_map_get;

	p_adpt_api->adpt_pon_gemport_cfg_set = adpt_hmsppe_pon_gemport_cfg_set;
	p_adpt_api->adpt_pon_gemport_cfg_get = adpt_hmsppe_pon_gemport_cfg_get;

	p_adpt_api->adpt_pon_gemport_policer_set = adpt_hmsppe_pon_gemport_policer_set;
	p_adpt_api->adpt_pon_gemport_policer_get = adpt_hmsppe_pon_gemport_policer_get;

	return SW_OK;
}

/**
 * @}
 */
