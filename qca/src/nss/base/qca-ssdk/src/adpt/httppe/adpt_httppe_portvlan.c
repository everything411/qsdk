/*
* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw.h"
#include "adpt.h"
#include "hsl_htt_reg.h"

sw_error_t
adpt_httppe_port_qinq_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mode);

	if (ADPT_IS_VPORT(port_id)) {
		if (FAL_FLG_TST(mode->mask, FAL_PORT_QINQ_ROLE_INGRESS_EN)) {
			union ipr_vp_parsing_u vp_parsing_reg;
			aos_mem_zero(&vp_parsing_reg, sizeof(vp_parsing_reg));

			rtn = httppe_ipr_vp_parsing_get(dev_id, (port_value - SSDK_MIN_VIRTUAL_PORT_ID), &vp_parsing_reg);
			SW_RTN_ON_ERROR(rtn);

			vp_parsing_reg.bf.port_role = mode->ingress_port_role;
			vp_parsing_reg.bf.src_port_sel =
					mode->ingress_port_sel == FAL_QINQ_SEL_TNL_DECAP_SRC_VP ? 0 : 1;
			vp_parsing_reg.bf.vlan_mode = mode->in_port_ponmode_en;

			rtn = httppe_ipr_vp_parsing_set(dev_id, (port_value - SSDK_MIN_VIRTUAL_PORT_ID), &vp_parsing_reg);
			SW_RTN_ON_ERROR(rtn);
		}

		if (FAL_FLG_TST(mode->mask, FAL_PORT_QINQ_ROLE_EGRESS_EN)) {
			rtn = httppe_eg_vp_tbl_port_vlan_type_set(dev_id, port_value,
					(a_uint32_t)mode->egress_port_role);
			SW_RTN_ON_ERROR(rtn);
		}
	} else
	if (ADPT_IS_PPORT(port_id)) {
		if (FAL_FLG_TST(mode->mask, FAL_PORT_QINQ_ROLE_INGRESS_EN)) {
			union port_parsing_reg_u port_parsing_reg;
			aos_mem_zero(&port_parsing_reg, sizeof(port_parsing_reg));

			rtn = httppe_port_parsing_reg_get(dev_id, port_value, &port_parsing_reg);
			SW_RTN_ON_ERROR(rtn);

			port_parsing_reg.bf.port_role = mode->ingress_port_role;
			port_parsing_reg.bf.src_port_sel =
					mode->ingress_port_sel == FAL_QINQ_SEL_TNL_DECAP_SRC_VP ? 0 : 1;
			port_parsing_reg.bf.vlan_mode = mode->in_port_ponmode_en;

			rtn = httppe_port_parsing_reg_set(dev_id, port_value, &port_parsing_reg);
			SW_RTN_ON_ERROR(rtn);
		}

		if (FAL_FLG_TST(mode->mask, FAL_PORT_QINQ_ROLE_EGRESS_EN)) {
			SW_RTN_ON_ERROR(httppe_port_eg_vlan_port_vlan_type_set(dev_id, port_value,
						(a_uint32_t)mode->egress_port_role));
		}
	}

	return rtn;
}

sw_error_t
adpt_httppe_port_qinq_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mode);

	if (ADPT_IS_VPORT(port_id)) {
		union ipr_vp_parsing_u vp_parsing_reg;
		aos_mem_zero(&vp_parsing_reg, sizeof(vp_parsing_reg));

		rtn = httppe_ipr_vp_parsing_get(dev_id, (port_value - SSDK_MIN_VIRTUAL_PORT_ID), &vp_parsing_reg);
		SW_RTN_ON_ERROR(rtn);

		mode->ingress_port_role = (fal_qinq_port_role_t)vp_parsing_reg.bf.port_role;
		mode->ingress_port_sel =
				vp_parsing_reg.bf.src_port_sel ? FAL_QINQ_SEL_ORG_SRC_PORT : FAL_QINQ_SEL_TNL_DECAP_SRC_VP;
		mode->in_port_ponmode_en = vp_parsing_reg.bf.vlan_mode;

		rtn = httppe_eg_vp_tbl_port_vlan_type_get(dev_id, port_value,
				(a_uint32_t *)&mode->egress_port_role);
		SW_RTN_ON_ERROR(rtn);
	} else if (ADPT_IS_PPORT(port_id)) {
		union port_parsing_reg_u port_parsing_reg;
		aos_mem_zero(&port_parsing_reg, sizeof(port_parsing_reg));

		rtn = httppe_port_parsing_reg_get(dev_id, port_value, &port_parsing_reg);
		SW_RTN_ON_ERROR(rtn);

		mode->ingress_port_role = (fal_qinq_port_role_t)port_parsing_reg.bf.port_role;
		mode->ingress_port_sel =
				port_parsing_reg.bf.src_port_sel ? FAL_QINQ_SEL_ORG_SRC_PORT : FAL_QINQ_SEL_TNL_DECAP_SRC_VP;
		mode->in_port_ponmode_en = port_parsing_reg.bf.vlan_mode;

		rtn = httppe_port_eg_vlan_port_vlan_type_get(dev_id, port_value,
					(a_uint32_t *)&mode->egress_port_role);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_httppe_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid)
{
	sw_error_t rtn = SW_OK;
	union vlan_tpid_reg_u ppe_tpid;
	union vlan_tpid_reg_ext0_u ppe_tpid0;
	union vlan_tpid_reg_ext1_u ppe_tpid1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(tpid);

	rtn = httppe_vlan_tpid_reg_get(dev_id, &ppe_tpid);
	SW_RTN_ON_ERROR(rtn);

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_CTAG_EN)) {
		ppe_tpid.bf.ctag_tpid = tpid->ctpid;
	}

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_STAG_EN)) {
		ppe_tpid.bf.stag_tpid = tpid->stpid;
	}

	rtn = httppe_vlan_tpid_reg_ext0_get(dev_id, &ppe_tpid0);
	SW_RTN_ON_ERROR(rtn);

	rtn = httppe_vlan_tpid_reg_ext1_get(dev_id, &ppe_tpid1);
	SW_RTN_ON_ERROR(rtn);

	if (FAL_FLG_TST(tpid->mask, FAL_EXT_TPID_CTAG_EN)) {
		ppe_tpid0.bf.ctag_tpid = tpid->ext_ctpid;
	}

	if (FAL_FLG_TST(tpid->mask, FAL_EXT_TPID_STAG_EN)) {
		ppe_tpid0.bf.stag_tpid = tpid->ext_stpid;
	}

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_CTAG_MAP_EN)) {
		ppe_tpid1.bf.ctag_tpid_map = tpid->ctpid_map;
	}

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_STAG_MAP_EN)) {
		ppe_tpid1.bf.stag_tpid_map = tpid->stpid_map;
	}

	rtn = httppe_vlan_tpid_reg_ext0_set(dev_id, &ppe_tpid0);
	SW_RTN_ON_ERROR(rtn);

	rtn = httppe_vlan_tpid_reg_ext1_set(dev_id, &ppe_tpid1);
	SW_RTN_ON_ERROR(rtn);

	rtn = httppe_vlan_tpid_reg_set(dev_id, &ppe_tpid);

	return rtn;
}

sw_error_t
adpt_httppe_tpid_get(a_uint32_t dev_id, fal_tpid_t *tpid)
{
	sw_error_t rtn = SW_OK;
	union vlan_tpid_reg_u ppe_tpid;
	union vlan_tpid_reg_ext0_u ppe_tpid0;
	union vlan_tpid_reg_ext1_u ppe_tpid1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(tpid);

	rtn = httppe_vlan_tpid_reg_get(dev_id, &ppe_tpid);
	SW_RTN_ON_ERROR(rtn);

	rtn = httppe_vlan_tpid_reg_ext0_get(dev_id, &ppe_tpid0);
	SW_RTN_ON_ERROR(rtn);

	rtn = httppe_vlan_tpid_reg_ext1_get(dev_id, &ppe_tpid1);
	SW_RTN_ON_ERROR(rtn);

	tpid->ext_ctpid = ppe_tpid0.bf.ctag_tpid;
	tpid->ext_stpid = ppe_tpid0.bf.stag_tpid;
	tpid->ctpid_map = ppe_tpid1.bf.ctag_tpid_map;
	tpid->stpid_map = ppe_tpid1.bf.stag_tpid_map;
	tpid->ctpid = ppe_tpid.bf.ctag_tpid;
	tpid->stpid = ppe_tpid.bf.stag_tpid;

	return rtn;
}

sw_error_t
adpt_httppe_egress_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid)
{
	sw_error_t rtn = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_CTAG_EN)) {
		rtn = httppe_eg_vlan_tpid_ctpid_set(dev_id,
				(a_uint32_t)tpid->ctpid);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_STAG_EN)) {
		rtn = httppe_eg_vlan_tpid_stpid_set(dev_id,
				(a_uint32_t)tpid->stpid);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(tpid->mask, FAL_EXT_TPID_CTAG_EN)) {
		rtn = httppe_eg_vlan_tpid_ext0_ctpid_set(dev_id,
				(a_uint32_t)tpid->ext_ctpid);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(tpid->mask, FAL_EXT_TPID_STAG_EN)) {
		rtn = httppe_eg_vlan_tpid_ext0_stpid_set(dev_id,
				(a_uint32_t)tpid->ext_stpid);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_CTAG_MAP_EN)) {
		rtn = httppe_eg_vlan_tpid_ext1_ctag_tpid_map_set(dev_id,
				(a_uint32_t)tpid->ctpid_map);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(tpid->mask, FAL_TPID_STAG_MAP_EN)) {
		rtn = httppe_eg_vlan_tpid_ext1_stag_tpid_map_set(dev_id,
				(a_uint32_t)tpid->stpid_map);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_httppe_egress_tpid_get(a_uint32_t dev_id, fal_tpid_t *tpid)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t tmp = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(tpid);

	rtn = httppe_eg_vlan_tpid_ctpid_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(rtn);
	tpid->ctpid = tmp;

	rtn = httppe_eg_vlan_tpid_stpid_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(rtn);
	tpid->stpid = tmp;

	rtn = httppe_eg_vlan_tpid_ext0_ctpid_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(rtn);
	tpid->ext_ctpid = tmp;

	rtn = httppe_eg_vlan_tpid_ext0_stpid_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(rtn);
	tpid->ext_stpid = tmp;

	rtn = httppe_eg_vlan_tpid_ext1_ctag_tpid_map_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(rtn);
	tpid->ctpid_map = tmp;

	rtn = httppe_eg_vlan_tpid_ext1_stag_tpid_map_get(dev_id, &tmp);
	SW_RTN_ON_ERROR(rtn);
	tpid->stpid_map = tmp;

	return rtn;
}

a_uint32_t
_httppe_get_port_vlan_trans_adv_rule_by_index(a_uint32_t dev_id,
		a_uint32_t index, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t *rule, fal_vlan_trans_adv_action_t * action)
{
	union xlt_rule_tbl_u in_vlan_xlt_rule;
	union xlt_action_tbl_u in_vlan_xlt_action;
	union eg_vlan_xlt_rule_u eg_vlan_xlt_rule;
	union eg_vlan_xlt_action_u eg_vlan_xlt_action;
	a_uint32_t rtn = SW_OK;

	if (direction == FAL_PORT_VLAN_INGRESS) {
		/*rule part*/
		rtn = httppe_xlt_rule_tbl_get(dev_id, index, &in_vlan_xlt_rule);
		SW_RTN_ON_ERROR(rtn);

		if (!in_vlan_xlt_rule.bf.valid) {
			memset(&in_vlan_xlt_rule, 0, sizeof(in_vlan_xlt_rule));
		}

		rule->stpid_idx_en = in_vlan_xlt_rule.bf.stpid_incl;
		rule->stpid_idx = in_vlan_xlt_rule.bf.stpid_index;
		rule->ctpid_idx_en = in_vlan_xlt_rule.bf.ctpid_incl;
		rule->ctpid_idx = in_vlan_xlt_rule.bf.ctpid_index;
		rule->dhcp_type = in_vlan_xlt_rule.bf.dhcp_type;
		rule->mc_type = in_vlan_xlt_rule.bf.mc_type;
		rule->vni_resv = (in_vlan_xlt_rule.bf.vni_resv_0 |
			in_vlan_xlt_rule.bf.vni_resv_1 << SW_FIELD_OFFSET_IN_WORD(XLT_RULE_TBL_VNI_RESV_OFFSET));
		rule->vni_resv_enable = in_vlan_xlt_rule.bf.vni_resv_incl;
		rule->vni_resv_type = in_vlan_xlt_rule.bf.vni_resv_type;
		rule->port_bitmap = FAL_PORT_ID(adpt_port_type_convert(A_FALSE,
					in_vlan_xlt_rule.bf.port_type),
				in_vlan_xlt_rule.bf.port_bitmap);
		rule->s_tagged = in_vlan_xlt_rule.bf.skey_fmt;
		rule->s_vid_enable = in_vlan_xlt_rule.bf.skey_vid_incl;
		rule->s_vid = in_vlan_xlt_rule.bf.skey_vid;
		rule->s_pcp_enable = in_vlan_xlt_rule.bf.skey_pcp_incl;
		rule->s_pcp = in_vlan_xlt_rule.bf.skey_pcp;
		rule->s_dei_enable = in_vlan_xlt_rule.bf.skey_dei_incl;
		rule->s_dei = in_vlan_xlt_rule.bf.skey_dei;
		rule->protocol = (in_vlan_xlt_rule.bf.prot_value_0 |
			in_vlan_xlt_rule.bf.prot_value_1 << SW_FIELD_OFFSET_IN_WORD(XLT_RULE_TBL_PROT_VALUE_OFFSET));
		rule->c_tagged = in_vlan_xlt_rule.bf.ckey_fmt;

		rule->c_vid_enable = in_vlan_xlt_rule.bf.ckey_vid_incl;
		rule->c_vid = in_vlan_xlt_rule.bf.ckey_vid;
		rule->c_pcp_enable = in_vlan_xlt_rule.bf.ckey_pcp_incl;
		rule->c_pcp = in_vlan_xlt_rule.bf.ckey_pcp;
		rule->c_dei_enable = in_vlan_xlt_rule.bf.ckey_dei_incl;
		rule->c_dei = in_vlan_xlt_rule.bf.ckey_dei;

		rule->frmtype_enable = in_vlan_xlt_rule.bf.frm_type_incl;
		rule->frmtype = in_vlan_xlt_rule.bf.frm_type;
		rule->protocol_enable = in_vlan_xlt_rule.bf.prot_incl;

		/*action part*/
		rtn = httppe_xlt_action_tbl_get(dev_id, index, &in_vlan_xlt_action);
		SW_RTN_ON_ERROR(rtn);

		if (!in_vlan_xlt_rule.bf.valid) {
			memset(&in_vlan_xlt_action, 0, sizeof(struct xlt_action_tbl));
		}

		action->swap_svid_cvid = in_vlan_xlt_action.bf.vid_swap_cmd;
		action->svid_xlt_cmd = in_vlan_xlt_action.bf.xlt_svid_cmd;
		action->svid_xlt = in_vlan_xlt_action.bf.xlt_svid;
		action->cvid_xlt_cmd = in_vlan_xlt_action.bf.xlt_cvid_cmd;
		action->cvid_xlt = in_vlan_xlt_action.bf.xlt_cvid;

		action->swap_spcp_cpcp = in_vlan_xlt_action.bf.pcp_swap_cmd;
		action->spcp_xlt_cmd = in_vlan_xlt_action.bf.xlt_spcp_cmd;
		action->spcp_xlt = in_vlan_xlt_action.bf.xlt_spcp;
		action->cpcp_xlt_cmd = in_vlan_xlt_action.bf.xlt_cpcp_cmd;
		action->cpcp_xlt = in_vlan_xlt_action.bf.xlt_cpcp;

		action->swap_sdei_cdei = in_vlan_xlt_action.bf.dei_swap_cmd;
		action->sdei_xlt_cmd = in_vlan_xlt_action.bf.xlt_sdei_cmd;
		action->sdei_xlt = in_vlan_xlt_action.bf.xlt_sdei;
		action->cdei_xlt_cmd = in_vlan_xlt_action.bf.xlt_cdei_cmd;
		action->cdei_xlt = in_vlan_xlt_action.bf.xlt_cdei;
		action->counter_enable = in_vlan_xlt_action.bf.counter_en;
		action->tags_to_rm = in_vlan_xlt_action.bf.tags_to_remove;
		action->stpid_idx_xlt_cmd = in_vlan_xlt_action.bf.stpid_cmd;
		action->stpid_idx_xlt = in_vlan_xlt_action.bf.stpid_index;
		action->ctpid_idx_xlt_cmd = in_vlan_xlt_action.bf.ctpid_cmd;
		action->ctpid_idx_xlt = in_vlan_xlt_action.bf.ctpid_index;
		action->dscp_map_idx = in_vlan_xlt_action.bf.dscp_pbit_mapping_index;
		action->counter_mode = in_vlan_xlt_action.bf.counter_mode;
		action->fwd_cmd = in_vlan_xlt_action.bf.fwd_cmd;
		action->dst_valid = in_vlan_xlt_action.bf.dest_info_valid;
		action->dst_port.dest_info_value = (in_vlan_xlt_action.bf.dest_info_0 |
			(in_vlan_xlt_action.bf.dest_info_1 << SW_FIELD_OFFSET_IN_WORD(XLT_ACTION_TBL_DEST_INFO_OFFSET)));
		action->dst_port.dest_info_type = (in_vlan_xlt_action.bf.dest_info_valid) ? FAL_DEST_INFO_PORT_ID : 0;
		action->svc_code_en = in_vlan_xlt_action.bf.service_code_en;
		action->svc_code = in_vlan_xlt_action.bf.service_code;
		action->counter_id = in_vlan_xlt_action.bf.counter_id;
		action->src_info_enable = in_vlan_xlt_action.bf.src_info_valid;
		action->src_info_type = in_vlan_xlt_action.bf.src_info_type;
		action->src_info = in_vlan_xlt_action.bf.src_info;
		action->vsi_xlt_enable = in_vlan_xlt_action.bf.vsi_cmd;
		action->vsi_xlt = in_vlan_xlt_action.bf.vsi;

		return in_vlan_xlt_rule.bf.valid;
	} else {
		/*rule part*/
		rtn = httppe_eg_vlan_xlt_rule_get(dev_id, index, &eg_vlan_xlt_rule);
		SW_RTN_ON_ERROR(rtn);

		if (!eg_vlan_xlt_rule.bf.valid) {
			memset(&eg_vlan_xlt_rule, 0, sizeof(struct eg_vlan_xlt_rule));
		}

		rule->s_tagged = eg_vlan_xlt_rule.bf.skey_fmt;
		rule->s_vid_enable = eg_vlan_xlt_rule.bf.skey_vid_incl;
		rule->stpid_idx_en = eg_vlan_xlt_rule.bf.stpid_incl;
		rule->stpid_idx = eg_vlan_xlt_rule.bf.stpid_index;
		rule->ctpid_idx_en = eg_vlan_xlt_rule.bf.ctpid_incl;
		rule->ctpid_idx = eg_vlan_xlt_rule.bf.ctpid_index;
		rule->protocol_enable = eg_vlan_xlt_rule.bf.prot_incl;
		rule->protocol = eg_vlan_xlt_rule.bf.prot_value;
		rule->dhcp_type = eg_vlan_xlt_rule.bf.dhcp_type;
		rule->mc_type = eg_vlan_xlt_rule.bf.mc_type;
		rule->s_vid = (eg_vlan_xlt_rule.bf.skey_vid_0 |
			(eg_vlan_xlt_rule.bf.skey_vid_1 << SW_FIELD_OFFSET_IN_WORD(EG_VLAN_XLT_RULE_SKEY_VID_OFFSET)));
		rule->port_bitmap = FAL_PORT_ID(adpt_port_type_convert(A_FALSE,
					eg_vlan_xlt_rule.bf.port_type),
				eg_vlan_xlt_rule.bf.port_bitmap);
		rule->s_pcp_enable = eg_vlan_xlt_rule.bf.skey_pcp_incl;
		rule->s_pcp = eg_vlan_xlt_rule.bf.skey_pcp;
		rule->s_dei_enable = eg_vlan_xlt_rule.bf.skey_dei_incl;
		rule->s_dei = eg_vlan_xlt_rule.bf.skey_dei;

		rule->c_tagged = eg_vlan_xlt_rule.bf.ckey_fmt;
		rule->c_vid_enable = eg_vlan_xlt_rule.bf.ckey_vid_incl;
		rule->c_vid = eg_vlan_xlt_rule.bf.ckey_vid;
		rule->c_pcp_enable = eg_vlan_xlt_rule.bf.ckey_pcp_incl;
		rule->c_pcp = eg_vlan_xlt_rule.bf.ckey_pcp;
		rule->c_dei_enable = eg_vlan_xlt_rule.bf.ckey_dei_incl;
		rule->c_dei = eg_vlan_xlt_rule.bf.ckey_dei;

		rule->vsi_valid = eg_vlan_xlt_rule.bf.vsi_valid;
		rule->vsi_enable = eg_vlan_xlt_rule.bf.vsi_incl;
		rule->vsi = eg_vlan_xlt_rule.bf.vsi;

		/*action part*/
		rtn = httppe_eg_vlan_xlt_action_get(dev_id, index, &eg_vlan_xlt_action);
		SW_RTN_ON_ERROR(rtn);

		if (!eg_vlan_xlt_rule.bf.valid) {
			memset(&eg_vlan_xlt_action, 0, sizeof(struct eg_vlan_xlt_action));
		}

		action->swap_svid_cvid = eg_vlan_xlt_action.bf.vid_swap_cmd;
		action->svid_xlt_cmd = eg_vlan_xlt_action.bf.xlt_svid_cmd;
		action->svid_xlt = eg_vlan_xlt_action.bf.xlt_svid;
		action->cvid_xlt_cmd = eg_vlan_xlt_action.bf.xlt_cvid_cmd;
		action->cvid_xlt = eg_vlan_xlt_action.bf.xlt_cvid;

		action->swap_spcp_cpcp = eg_vlan_xlt_action.bf.pcp_swap_cmd;
		action->spcp_xlt_cmd = eg_vlan_xlt_action.bf.xlt_spcp_cmd;
		action->spcp_xlt = eg_vlan_xlt_action.bf.xlt_spcp;
		action->cpcp_xlt_cmd = eg_vlan_xlt_action.bf.xlt_cpcp_cmd;
		action->cpcp_xlt = eg_vlan_xlt_action.bf.xlt_cpcp;

		action->swap_sdei_cdei = eg_vlan_xlt_action.bf.dei_swap_cmd;
		action->sdei_xlt_cmd = eg_vlan_xlt_action.bf.xlt_sdei_cmd;
		action->sdei_xlt = eg_vlan_xlt_action.bf.xlt_sdei;
		action->cdei_xlt_cmd = eg_vlan_xlt_action.bf.xlt_cdei_cmd;
		action->cdei_xlt = eg_vlan_xlt_action.bf.xlt_cdei;

		action->counter_enable = eg_vlan_xlt_action.bf.counter_en;
		action->counter_id = eg_vlan_xlt_action.bf.counter_id;
		action->tags_to_rm = eg_vlan_xlt_action.bf.tags_to_remove;
		action->stpid_idx_xlt_cmd = eg_vlan_xlt_action.bf.stpid_cmd;
		action->stpid_idx_xlt = eg_vlan_xlt_action.bf.stpid_index;
		action->ctpid_idx_xlt_cmd = eg_vlan_xlt_action.bf.ctpid_cmd;
		action->ctpid_idx_xlt = eg_vlan_xlt_action.bf.ctpid_index;
		action->dscp_map_idx = eg_vlan_xlt_action.bf.dscp_pbit_mapping_index;
		action->fwd_cmd = eg_vlan_xlt_action.bf.fwd_cmd;
		action->vni_resv_enable = eg_vlan_xlt_action.bf.vni_resv_en;
		action->vni_resv = (eg_vlan_xlt_action.bf.vni_resv_0 |
			(eg_vlan_xlt_action.bf.vni_resv_1 << SW_FIELD_OFFSET_IN_WORD(EG_VLAN_XLT_ACTION_VNI_RESV_OFFSET)));

		return eg_vlan_xlt_rule.bf.valid;
	}
}

a_uint32_t
_httppe_check_if_rule_equal(fal_port_vlan_direction_t direction, fal_vlan_trans_adv_rule_t * rule1,
		fal_vlan_trans_adv_rule_t * rule2)
{
	if (!(rule1->s_tagged == rule2->s_tagged &&
		rule1->s_vid_enable == rule2->s_vid_enable && rule1->s_vid == rule2->s_vid &&
		rule1->s_pcp_enable == rule2->s_pcp_enable && rule1->s_pcp == rule2->s_pcp &&
		rule1->s_dei_enable == rule2->s_dei_enable && rule1->s_dei == rule2->s_dei &&
		rule1->c_tagged == rule2->c_tagged &&
		rule1->stpid_idx_en == rule2->stpid_idx_en &&
		rule1->stpid_idx == rule2->stpid_idx &&
		rule1->ctpid_idx_en == rule2->ctpid_idx_en &&
		rule1->ctpid_idx == rule2->ctpid_idx &&
		rule1->dhcp_type == rule2->dhcp_type &&
		rule1->mc_type == rule2->mc_type &&
		FAL_PORT_ID_TYPE(rule1->port_bitmap) == FAL_PORT_ID_TYPE(rule2->port_bitmap) &&
		rule1->c_vid_enable == rule2->c_vid_enable && rule1->c_vid == rule2->c_vid &&
		rule1->c_pcp_enable == rule2->c_pcp_enable && rule1->c_pcp == rule2->c_pcp &&
		rule1->c_dei_enable == rule2->c_dei_enable && rule1->c_dei == rule2->c_dei))
		return 1;

	if (direction == FAL_PORT_VLAN_INGRESS)
	{
		if (!(rule1->frmtype_enable == rule2->frmtype_enable &&
					rule1->frmtype == rule2->frmtype &&
					rule1->vni_resv_enable == rule2->vni_resv_enable &&
					rule1->vni_resv_type == rule2->vni_resv_type &&
					rule1->vni_resv == rule2->vni_resv &&
					rule1->protocol_enable == rule2->protocol_enable &&
					rule1->protocol == rule2->protocol))
			return 1;
	} else {
		if (!(rule1->vsi_valid == rule2->vsi_valid &&
					rule1->vsi_enable == rule2->vsi_enable &&
					rule1->vsi == rule2->vsi &&
					rule1->protocol_enable == rule2->protocol_enable &&
					rule1->protocol == rule2->protocol
					))
			return 1;
	}

	return 0;
}

a_uint32_t
_httppe_check_if_action_equal(fal_port_vlan_direction_t direction, fal_vlan_trans_adv_action_t * action1,
		fal_vlan_trans_adv_action_t * action2)
{
	if (!(action1->swap_svid_cvid == action2->swap_svid_cvid &&
		action1->svid_xlt_cmd == action2->svid_xlt_cmd &&
		action1->svid_xlt == action2->svid_xlt &&
		action1->cvid_xlt_cmd == action2->cvid_xlt_cmd &&
		action1->cvid_xlt == action2->cvid_xlt &&
		action1->swap_sdei_cdei == action2->swap_sdei_cdei &&
		action1->sdei_xlt_cmd == action2->sdei_xlt_cmd &&
		action1->sdei_xlt == action2->sdei_xlt &&
		action1->cdei_xlt_cmd == action2->cdei_xlt_cmd &&
		action1->cdei_xlt == action2->cdei_xlt &&
		action1->swap_spcp_cpcp == action2->swap_spcp_cpcp &&
		action1->spcp_xlt_cmd == action2->spcp_xlt_cmd &&
		action1->spcp_xlt == action2->spcp_xlt &&
		action1->cpcp_xlt_cmd == action2->cpcp_xlt_cmd &&
		action1->cpcp_xlt == action2->cpcp_xlt &&
		action1->tags_to_rm == action2->tags_to_rm &&
		action1->stpid_idx_xlt_cmd == action2->stpid_idx_xlt_cmd &&
		action1->stpid_idx_xlt == action2->stpid_idx_xlt &&
		action1->ctpid_idx_xlt_cmd == action2->ctpid_idx_xlt_cmd &&
		action1->ctpid_idx_xlt == action2->ctpid_idx_xlt &&
		action1->dscp_map_idx == action2->dscp_map_idx &&
		action1->fwd_cmd == action2->fwd_cmd &&
		action1->counter_enable == action2->counter_enable &&
		action1->counter_id == action2->counter_id))
		return 1;

	if (direction == FAL_PORT_VLAN_INGRESS) {
		if (!(action1->vsi_xlt_enable == action2->vsi_xlt_enable &&
					action1->svc_code_en == action2->svc_code_en &&
					action1->svc_code == action2->svc_code &&
					action1->dst_valid == action2->dst_valid &&
					action1->dst_port.dest_info_type == action2->dst_port.dest_info_type &&
					action1->dst_port.dest_info_value == action2->dst_port.dest_info_value &&
					action1->counter_mode == action2->counter_mode &&
					action1->src_info_enable == action2->src_info_enable &&
					action1->src_info_type == action2->src_info_type &&
					action1->src_info == action2->src_info &&
					action1->vsi_xlt == action2->vsi_xlt))
			return 1;
	} else {
		if (!(action1->vni_resv_enable == action2->vni_resv_enable &&
					action1->vni_resv == action2->vni_resv))
			return 1;
	}

	return 0;
}

sw_error_t
_httppe_insert_vlan_trans_adv_rule_action(a_uint32_t dev_id, a_uint32_t index,
		fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
{
	sw_error_t rtn = SW_OK;
	union xlt_rule_tbl_u in_vlan_xlt_rule;
	union eg_vlan_xlt_rule_u eg_vlan_xlt_rule;
	union xlt_action_tbl_u in_vlan_xlt_action;
	union eg_vlan_xlt_action_u eg_vlan_xlt_action;

	if (direction == FAL_PORT_VLAN_INGRESS)
	{
		in_vlan_xlt_rule.bf.stpid_incl = rule->stpid_idx_en;
		in_vlan_xlt_rule.bf.stpid_index = rule->stpid_idx;
		in_vlan_xlt_rule.bf.ctpid_incl = rule->ctpid_idx_en;
		in_vlan_xlt_rule.bf.ctpid_index = rule->ctpid_idx;
		in_vlan_xlt_rule.bf.dhcp_type = rule->dhcp_type;
		in_vlan_xlt_rule.bf.mc_type = rule->mc_type;
		in_vlan_xlt_rule.bf.vni_resv_0 = rule->vni_resv;
		in_vlan_xlt_rule.bf.vni_resv_1 = (rule->vni_resv >>
			(SW_FIELD_OFFSET_IN_WORD(XLT_RULE_TBL_VNI_RESV_OFFSET)));

		in_vlan_xlt_rule.bf.port_bitmap = FAL_PORT_ID_VALUE(rule->port_bitmap);
		in_vlan_xlt_rule.bf.port_type = adpt_port_type_convert(A_TRUE,
				FAL_PORT_ID_TYPE(rule->port_bitmap));
		in_vlan_xlt_rule.bf.vni_resv_incl = rule->vni_resv_enable;
		in_vlan_xlt_rule.bf.vni_resv_type = rule->vni_resv_type;
		in_vlan_xlt_rule.bf.valid = A_TRUE;
		in_vlan_xlt_rule.bf.skey_fmt = rule->s_tagged;
		in_vlan_xlt_rule.bf.skey_vid_incl = rule->s_vid_enable;
		in_vlan_xlt_rule.bf.skey_vid = rule->s_vid;
		in_vlan_xlt_rule.bf.skey_pcp_incl = rule->s_pcp_enable;
		in_vlan_xlt_rule.bf.skey_pcp = rule->s_pcp;
		in_vlan_xlt_rule.bf.skey_dei_incl = rule->s_dei_enable;
		in_vlan_xlt_rule.bf.skey_dei = rule->s_dei;
		in_vlan_xlt_rule.bf.ckey_fmt = rule->c_tagged;
		in_vlan_xlt_rule.bf.prot_value_0 = rule->protocol;
		in_vlan_xlt_rule.bf.prot_value_1 = (rule->protocol >>
			(SW_FIELD_OFFSET_IN_WORD(XLT_RULE_TBL_PROT_VALUE_OFFSET)));
		in_vlan_xlt_rule.bf.ckey_vid_incl = rule->c_vid_enable;
		in_vlan_xlt_rule.bf.ckey_vid = rule->c_vid;
		in_vlan_xlt_rule.bf.ckey_pcp_incl = rule->c_pcp_enable;
		in_vlan_xlt_rule.bf.ckey_pcp = rule->c_pcp;
		in_vlan_xlt_rule.bf.ckey_dei_incl = rule->c_dei_enable;
		in_vlan_xlt_rule.bf.ckey_dei = rule->c_dei;

		in_vlan_xlt_rule.bf.prot_incl = rule->protocol_enable;
		in_vlan_xlt_rule.bf.frm_type_incl = rule->frmtype_enable;
		in_vlan_xlt_rule.bf.frm_type = rule->frmtype;

		rtn = httppe_xlt_rule_tbl_set(dev_id, index, &in_vlan_xlt_rule);
		SW_RTN_ON_ERROR(rtn);

		/*action part*/
		in_vlan_xlt_action.bf.vid_swap_cmd = action->swap_svid_cvid;
		in_vlan_xlt_action.bf.xlt_svid_cmd = action->svid_xlt_cmd;
		in_vlan_xlt_action.bf.xlt_svid = action->svid_xlt;
		in_vlan_xlt_action.bf.xlt_cvid_cmd = action->cvid_xlt_cmd;
		in_vlan_xlt_action.bf.xlt_cvid = action->cvid_xlt;

		in_vlan_xlt_action.bf.pcp_swap_cmd = action->swap_spcp_cpcp;
		in_vlan_xlt_action.bf.xlt_spcp_cmd = action->spcp_xlt_cmd;
		in_vlan_xlt_action.bf.xlt_spcp = action->spcp_xlt;
		in_vlan_xlt_action.bf.xlt_cpcp_cmd = action->cpcp_xlt_cmd;
		in_vlan_xlt_action.bf.xlt_cpcp = action->cpcp_xlt;

		in_vlan_xlt_action.bf.dei_swap_cmd = action->swap_sdei_cdei;
		in_vlan_xlt_action.bf.xlt_cdei_cmd = action->cdei_xlt_cmd;
		in_vlan_xlt_action.bf.xlt_cdei = action->cdei_xlt;
		in_vlan_xlt_action.bf.xlt_sdei_cmd = action->sdei_xlt_cmd;
		in_vlan_xlt_action.bf.xlt_sdei = action->sdei_xlt;

		in_vlan_xlt_action.bf.counter_en = action->counter_enable;
		in_vlan_xlt_action.bf.tags_to_remove = action->tags_to_rm;
		in_vlan_xlt_action.bf.stpid_cmd = action->stpid_idx_xlt_cmd;
		in_vlan_xlt_action.bf.stpid_index = action->stpid_idx_xlt;
		in_vlan_xlt_action.bf.ctpid_cmd = action->ctpid_idx_xlt_cmd;
		in_vlan_xlt_action.bf.ctpid_index = action->ctpid_idx_xlt;
		in_vlan_xlt_action.bf.dscp_pbit_mapping_index = action->dscp_map_idx;
		in_vlan_xlt_action.bf.counter_mode = action->counter_mode;
		in_vlan_xlt_action.bf.fwd_cmd = action->fwd_cmd;
		if (action->dst_valid && action->dst_port.dest_info_type != FAL_DEST_INFO_PORT_ID) {
			SSDK_ERROR("dest_info_type: %#x is not supported\n",
				action->dst_port.dest_info_type);
			return SW_NOT_SUPPORTED;
		}
		in_vlan_xlt_action.bf.dest_info_valid = action->dst_valid;
		in_vlan_xlt_action.bf.dest_info_0 = action->dst_port.dest_info_value;
		in_vlan_xlt_action.bf.dest_info_1 = (action->dst_port.dest_info_value >>
			(SW_FIELD_OFFSET_IN_WORD(XLT_ACTION_TBL_DEST_INFO_OFFSET)));
		in_vlan_xlt_action.bf.service_code_en = action->svc_code_en;
		in_vlan_xlt_action.bf.service_code = action->svc_code;
		in_vlan_xlt_action.bf.counter_id = action->counter_id;
		in_vlan_xlt_action.bf.src_info_valid = action->src_info_enable;
		in_vlan_xlt_action.bf.src_info_type = action->src_info_type;
		in_vlan_xlt_action.bf.src_info = action->src_info;
		in_vlan_xlt_action.bf.vsi_cmd = action->vsi_xlt_enable;
		in_vlan_xlt_action.bf.vsi = action->vsi_xlt;

		rtn = httppe_xlt_action_tbl_set(dev_id, index, &in_vlan_xlt_action);
		SW_RTN_ON_ERROR(rtn);
	}
	else
	{
		eg_vlan_xlt_rule.bf.valid = A_TRUE;

		eg_vlan_xlt_rule.bf.skey_fmt = rule->s_tagged;
		eg_vlan_xlt_rule.bf.skey_vid_incl = rule->s_vid_enable;
		eg_vlan_xlt_rule.bf.stpid_incl = rule->stpid_idx_en;
		eg_vlan_xlt_rule.bf.stpid_index = rule->stpid_idx;
		eg_vlan_xlt_rule.bf.ctpid_incl = rule->ctpid_idx_en;
		eg_vlan_xlt_rule.bf.ctpid_index = rule->ctpid_idx;
		eg_vlan_xlt_rule.bf.prot_incl = rule->protocol_enable;
		eg_vlan_xlt_rule.bf.prot_value = rule->protocol;
		eg_vlan_xlt_rule.bf.dhcp_type = rule->dhcp_type;
		eg_vlan_xlt_rule.bf.mc_type = rule->mc_type;
		eg_vlan_xlt_rule.bf.skey_vid_0 = rule->s_vid;
		eg_vlan_xlt_rule.bf.skey_vid_1 = (rule->s_vid >>
			(SW_FIELD_OFFSET_IN_WORD(EG_VLAN_XLT_RULE_SKEY_VID_OFFSET)));
		eg_vlan_xlt_rule.bf.port_bitmap = FAL_PORT_ID_VALUE(rule->port_bitmap);
		eg_vlan_xlt_rule.bf.port_type = adpt_port_type_convert(A_TRUE,
				FAL_PORT_ID_TYPE(rule->port_bitmap));
		eg_vlan_xlt_rule.bf.skey_pcp_incl = rule->s_pcp_enable;
		eg_vlan_xlt_rule.bf.skey_pcp = rule->s_pcp;
		eg_vlan_xlt_rule.bf.skey_dei_incl = rule->s_dei_enable;
		eg_vlan_xlt_rule.bf.skey_dei = rule->s_dei;

		eg_vlan_xlt_rule.bf.ckey_fmt = rule->c_tagged;
		eg_vlan_xlt_rule.bf.ckey_vid_incl = rule->c_vid_enable;
		eg_vlan_xlt_rule.bf.ckey_vid = rule->c_vid;
		eg_vlan_xlt_rule.bf.ckey_pcp_incl = rule->c_pcp_enable;
		eg_vlan_xlt_rule.bf.ckey_pcp = rule->c_pcp;
		eg_vlan_xlt_rule.bf.ckey_dei_incl = rule->c_dei_enable;
		eg_vlan_xlt_rule.bf.ckey_dei = rule->c_dei;

		eg_vlan_xlt_rule.bf.vsi_valid = rule->vsi_valid;
		eg_vlan_xlt_rule.bf.vsi_incl = rule->vsi_enable;
		eg_vlan_xlt_rule.bf.vsi = rule->vsi;

		rtn = httppe_eg_vlan_xlt_rule_set(dev_id, index, &eg_vlan_xlt_rule);
		SW_RTN_ON_ERROR(rtn);

		/*action part*/
		eg_vlan_xlt_action.bf.vid_swap_cmd = action->swap_svid_cvid;
		eg_vlan_xlt_action.bf.xlt_svid_cmd = action->svid_xlt_cmd;
		eg_vlan_xlt_action.bf.xlt_svid = action->svid_xlt;
		eg_vlan_xlt_action.bf.xlt_cvid_cmd = action->cvid_xlt_cmd;
		eg_vlan_xlt_action.bf.xlt_cvid = action->cvid_xlt;

		eg_vlan_xlt_action.bf.pcp_swap_cmd = action->swap_spcp_cpcp;
		eg_vlan_xlt_action.bf.xlt_spcp_cmd = action->spcp_xlt_cmd;
		eg_vlan_xlt_action.bf.xlt_spcp = action->spcp_xlt;
		eg_vlan_xlt_action.bf.xlt_cpcp_cmd = action->cpcp_xlt_cmd;
		eg_vlan_xlt_action.bf.xlt_cpcp = action->cpcp_xlt;

		eg_vlan_xlt_action.bf.dei_swap_cmd = action->swap_sdei_cdei;
		eg_vlan_xlt_action.bf.xlt_sdei_cmd = action->sdei_xlt_cmd;
		eg_vlan_xlt_action.bf.xlt_sdei = action->sdei_xlt;
		eg_vlan_xlt_action.bf.xlt_cdei_cmd = action->cdei_xlt_cmd;
		eg_vlan_xlt_action.bf.xlt_cdei = action->cdei_xlt;

		eg_vlan_xlt_action.bf.counter_en = action->counter_enable;
		eg_vlan_xlt_action.bf.counter_id = action->counter_id;

		eg_vlan_xlt_action.bf.tags_to_remove = action->tags_to_rm;
		eg_vlan_xlt_action.bf.stpid_cmd = action->stpid_idx_xlt_cmd;
		eg_vlan_xlt_action.bf.stpid_index = action->stpid_idx_xlt;
		eg_vlan_xlt_action.bf.ctpid_cmd = action->ctpid_idx_xlt_cmd;
		eg_vlan_xlt_action.bf.ctpid_index = action->ctpid_idx_xlt;
		eg_vlan_xlt_action.bf.dscp_pbit_mapping_index = action->dscp_map_idx;
		if (action->fwd_cmd != FAL_MAC_FRWRD && action->fwd_cmd != FAL_MAC_DROP) {
			SSDK_ERROR("egress xlt cmd: %#x is not supported\n", action->fwd_cmd);
			return SW_NOT_SUPPORTED;
		}
		eg_vlan_xlt_action.bf.fwd_cmd = action->fwd_cmd;

		eg_vlan_xlt_action.bf.vni_resv_0 = action->vni_resv;
		eg_vlan_xlt_action.bf.vni_resv_1 = (action->vni_resv >>
			(SW_FIELD_OFFSET_IN_WORD(EG_VLAN_XLT_ACTION_VNI_RESV_OFFSET)));
		eg_vlan_xlt_action.bf.vni_resv_en = action->vni_resv_enable;

		rtn = httppe_eg_vlan_xlt_action_set(dev_id, index, &eg_vlan_xlt_action);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
_httppe_delete_vlan_trans_adv_rule_action(a_uint32_t dev_id, a_uint32_t index,
		fal_port_vlan_direction_t direction)
{
	sw_error_t rtn = SW_OK;
	union xlt_rule_tbl_u in_vlan_xlt_rule;
	union eg_vlan_xlt_rule_u eg_vlan_xlt_rule;
	union xlt_action_tbl_u in_vlan_xlt_action;
	union eg_vlan_xlt_action_u eg_vlan_xlt_action;

	memset(&in_vlan_xlt_rule, 0, sizeof(in_vlan_xlt_rule));
	memset(&eg_vlan_xlt_rule, 0, sizeof(struct eg_vlan_xlt_rule));
	memset(&in_vlan_xlt_action, 0, sizeof(struct xlt_action_tbl));
	memset(&eg_vlan_xlt_action, 0, sizeof(struct eg_vlan_xlt_action));

	if (direction == FAL_PORT_VLAN_INGRESS) {
		rtn = httppe_xlt_rule_tbl_set(dev_id,
				index, &in_vlan_xlt_rule);
		SW_RTN_ON_ERROR(rtn);
		rtn = httppe_xlt_action_tbl_set(dev_id,
				index, &in_vlan_xlt_action);
		SW_RTN_ON_ERROR(rtn);
	} else {
		rtn = httppe_eg_vlan_xlt_rule_set(dev_id,
				index, &eg_vlan_xlt_rule);
		SW_RTN_ON_ERROR(rtn);
		rtn = httppe_eg_vlan_xlt_action_set(dev_id,
				index, &eg_vlan_xlt_action);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_httppe_port_vlan_trans_adv_add(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t entry_idx = FAL_PORTVLAN_TRANS_TBL_NUM(direction);
	a_int32_t idx = 0, rule_valid = 0, entry_found = 0;
	fal_vlan_trans_adv_rule_t temp_rule = {.port_bitmap = 0};
	fal_vlan_trans_adv_action_t temp_action = {.svid_xlt = 0};
	a_bool_t entry_sign = A_FALSE, insert_entry_ready = A_FALSE;

	ADPT_DEV_ID_CHECK(dev_id);

	if (direction == FAL_PORT_VLAN_ALL)
		return SW_FAIL;

	if (FAL_PORT_ID_TYPE(port_id) == FAL_PORT_TYPE_PPORT) {
		rule->port_bitmap = BIT(port_id);
	} else {
		rule->port_bitmap = port_id;
	}

	for (idx = FAL_PORTVLAN_TRANS_TBL_NUM(direction) - 1; idx >= 0; idx--) {
		aos_mem_zero(&temp_rule, sizeof (fal_vlan_trans_adv_rule_t));
		aos_mem_zero(&temp_action, sizeof (fal_vlan_trans_adv_action_t));
		rule_valid = _httppe_get_port_vlan_trans_adv_rule_by_index(dev_id,
				idx, direction, &temp_rule, &temp_action);

		if (!rule_valid) { /* nonexist rule */
			if (!entry_sign) {
				entry_idx = idx;
				entry_sign = A_TRUE;
			}
			continue;
		}
		/* existing rule */
		if (!_httppe_check_if_rule_equal(direction, &temp_rule, rule)) {
			/* rule equal */
			if (!_httppe_check_if_action_equal(direction, &temp_action, action)) {
				/* action equal */
				entry_found = adpt_port_id_compare(temp_rule.port_bitmap,
						port_id);
				if (entry_found == ADPT_PORT_ID_EQUAL ||
						entry_found == ADPT_PORT_ID_INCLD) {
					return SW_ALREADY_EXIST;
				} else if (entry_found == ADPT_PORT_ID_EXCLD) {
					SW_PBMP_ADD_PORT(temp_rule.port_bitmap, port_id);
					rtn = _httppe_insert_vlan_trans_adv_rule_action(
							dev_id,
							idx, direction,
							&temp_rule,
							&temp_action);
					SW_RTN_ON_ERROR(rtn);
					rule->index = idx;
					return SW_OK;
				} else {
					/* need to insert a new portvlan entry */
					insert_entry_ready = A_TRUE;
				}
			} else { /* action not equal */
				entry_found = adpt_port_id_compare(temp_rule.port_bitmap,
						port_id);
				if (entry_found == ADPT_PORT_ID_EQUAL) {
					/* port equal, need update action */
					rtn = _httppe_insert_vlan_trans_adv_rule_action(dev_id,
							idx, direction,
							&temp_rule,
							action);
					SW_RTN_ON_ERROR(rtn);
					rule->index = idx;
					return SW_OK;
				} else {
					if (entry_found == ADPT_PORT_ID_INCLD) {
						/* port included, need remove port from
						 * existing rule bitmap, insert new rule
						 * and action later */
						SW_PBMP_DEL_PORT(temp_rule.port_bitmap, port_id);
						_httppe_insert_vlan_trans_adv_rule_action(dev_id,
								idx, direction,
								&temp_rule,
								&temp_action);
					}

					/* need to insert a new portvlan entry */
					insert_entry_ready = A_TRUE;
				}
			}
		}

		if (insert_entry_ready == A_TRUE && entry_sign == A_TRUE) {
			break;
		}
	}

	if (entry_idx == FAL_PORTVLAN_TRANS_TBL_NUM(direction)) {
		return SW_NO_RESOURCE;
	} else {
		/* insert new rule and action */
		rtn = _httppe_insert_vlan_trans_adv_rule_action(dev_id, entry_idx,
				direction, rule, action);
		SW_RTN_ON_ERROR(rtn);
		rule->index = entry_idx;
	}

	return rtn;
}

sw_error_t
adpt_httppe_port_vlan_trans_adv_del(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t idx = 0, rule_valid = 0, entry_found = 0;
	fal_vlan_trans_adv_rule_t temp_rule = {.port_bitmap = 0};
	fal_vlan_trans_adv_action_t temp_action = {.svid_xlt = 0};

	ADPT_DEV_ID_CHECK(dev_id);

	if (direction == FAL_PORT_VLAN_ALL)
		return SW_FAIL;

	if (FAL_PORT_ID_TYPE(port_id) == FAL_PORT_TYPE_PPORT) {
		rule->port_bitmap = BIT(port_id);
	} else {
		rule->port_bitmap = port_id;
	}

	for (idx = 0; idx < FAL_PORTVLAN_TRANS_TBL_NUM(direction); idx++) {
		aos_mem_zero(&temp_rule, sizeof (fal_vlan_trans_adv_rule_t));
		aos_mem_zero(&temp_action, sizeof (fal_vlan_trans_adv_action_t));
		rule_valid = _httppe_get_port_vlan_trans_adv_rule_by_index(dev_id,
				idx, direction, &temp_rule, &temp_action);

		if (!rule_valid) {
			continue;
		}

		/* existing valid rule */
		if (!_httppe_check_if_rule_equal(direction, &temp_rule, rule))
		{ /* rule equal */
			if (!_httppe_check_if_action_equal(direction, &temp_action, action))
			{ /* action equal */
				entry_found = adpt_port_id_compare(temp_rule.port_bitmap,
						port_id);

				if (entry_found == ADPT_PORT_ID_EQUAL) {
					/* port equal, need delete existing rule and action */
					rtn = _httppe_delete_vlan_trans_adv_rule_action(dev_id, idx, direction);
					SW_RTN_ON_ERROR(rtn);
					rule->index = idx;
				} else if (entry_found == ADPT_PORT_ID_INCLD) {
					/* current port_bitmap includes this port_id,
					   remove port from port_bitmap and update rule
					   and action */
					SW_PBMP_DEL_PORT(temp_rule.port_bitmap, port_id);
					rtn = _httppe_insert_vlan_trans_adv_rule_action(dev_id,
							idx, direction, &temp_rule,
							&temp_action);
				}
				else {
					/* VP or VP group NOT EQUAL */
					continue;
				}

				break;
			}
		}
	}

	if (idx == FAL_PORTVLAN_TRANS_TBL_NUM(direction))
		return SW_NOT_FOUND;

	return rtn;
}

sw_error_t
adpt_httppe_port_vlan_trans_adv_get(a_uint32_t dev_id,
		fal_port_vlan_direction_t direction, a_uint32_t index,
		fal_vlan_trans_adv_rule_t *rule, fal_vlan_trans_adv_action_t *action)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (index >= FAL_PORTVLAN_TRANS_TBL_NUM(direction))
		return SW_BAD_VALUE;

	if (1 == _httppe_get_port_vlan_trans_adv_rule_by_index(dev_id,
		index, direction,rule, action)) {
		rule->index = index;
		return SW_OK;
	} else
		return SW_NOT_FOUND;
}

sw_error_t
adpt_httppe_port_vlan_trans_adv_set(a_uint32_t dev_id,
		fal_port_vlan_direction_t direction, a_uint32_t index,
		fal_vlan_trans_adv_rule_t *rule, fal_vlan_trans_adv_action_t *action)
{
	fal_vlan_trans_adv_rule_t tmp_rule = {.port_bitmap = 0};
	fal_vlan_trans_adv_action_t tmp_action = {.svid_xlt = 0};
	a_uint32_t idx = 0;
	a_uint32_t port_type = 0, temp_port_type = 0;
	a_uint32_t port_bitmap_value = 0, temp_port_bitmap_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	if (index >= FAL_PORTVLAN_TRANS_TBL_NUM(direction))
		return SW_BAD_VALUE;

	memset(&tmp_rule, 0, sizeof(tmp_rule));
	memset(&tmp_action, 0, sizeof(tmp_action));

	if (!memcmp(&tmp_rule, rule, sizeof(*rule)) &&
		!memcmp(&tmp_action, action, sizeof(*action)))
		return _httppe_delete_vlan_trans_adv_rule_action(dev_id, index, direction);

	for (idx = 0; idx < FAL_PORTVLAN_TRANS_TBL_NUM(direction); idx++) {
		/* Check if entry is valid */
		if (!_httppe_get_port_vlan_trans_adv_rule_by_index(dev_id, idx, direction, &tmp_rule, &tmp_action))
			continue;

		/* Check if rule and action are same */
		if (!_httppe_check_if_rule_equal(direction, &tmp_rule, rule) &&
			!_httppe_check_if_action_equal(direction, &tmp_action, action)) {
			port_type = FAL_PORT_ID_TYPE(rule->port_bitmap);
			temp_port_type = FAL_PORT_ID_TYPE(tmp_rule.port_bitmap);
			port_bitmap_value = FAL_PORT_ID_VALUE(rule->port_bitmap);
			temp_port_bitmap_value = FAL_PORT_ID_VALUE(tmp_rule.port_bitmap);

			/* In set API, caller give port_bitmap for physical port directly */
			if (port_type == FAL_PORT_TYPE_PPORT) {
				/* Type 0: check if new bitmap is contained in or equal to existing bitmap */
				if ((port_bitmap_value & temp_port_bitmap_value) == port_bitmap_value) {
					return SW_ALREADY_EXIST;
				}
			} else {
				/* Type non-0: check if values are equal */
				if (port_type == temp_port_type && port_bitmap_value == temp_port_bitmap_value) {
					return SW_ALREADY_EXIST;
				}
			}
		}
	}

	return _httppe_insert_vlan_trans_adv_rule_action(dev_id, index, direction, rule, action);
}

sw_error_t
adpt_httppe_port_vlan_trans_adv_getfirst(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t idx = 0, rule_valid = 0, entry_found = 0;
	fal_vlan_trans_adv_rule_t temp_rule = {.port_bitmap = 0};
	fal_vlan_trans_adv_action_t temp_action = {.svid_xlt = 0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(rule);
	ADPT_NULL_POINT_CHECK(action);

	if (direction == FAL_PORT_VLAN_ALL)
		return SW_FAIL;

	entry_found = A_FALSE;
	for (idx = 0; idx < FAL_PORTVLAN_TRANS_TBL_NUM(direction); idx++) {
		aos_mem_zero(&temp_rule, sizeof (fal_vlan_trans_adv_rule_t));
		aos_mem_zero(&temp_action, sizeof (fal_vlan_trans_adv_action_t));
		rule_valid = _httppe_get_port_vlan_trans_adv_rule_by_index(dev_id,
				idx, direction, &temp_rule, &temp_action);
		if (rule_valid == 1) {
			entry_found = adpt_port_id_compare(temp_rule.port_bitmap, port_id);

			if (entry_found == ADPT_PORT_ID_EQUAL ||
					entry_found == ADPT_PORT_ID_INCLD) {
				aos_mem_copy(rule, &temp_rule,
						sizeof(fal_vlan_trans_adv_rule_t));
				rule->index = idx;
				aos_mem_copy(action, &temp_action,
						sizeof(fal_vlan_trans_adv_action_t));
				break;
			}
		}
	}

	if (idx == FAL_PORTVLAN_TRANS_TBL_NUM(direction))
		return SW_NOT_FOUND;

	return rtn;
}

sw_error_t
adpt_httppe_port_vlan_trans_adv_getnext(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t idx = 0, rule_valid = 0, entry_found = 0;
	fal_vlan_trans_adv_rule_t temp_rule = {.port_bitmap = 0};
	fal_vlan_trans_adv_action_t temp_action = {.svid_xlt = 0};
	a_bool_t sign_tag = A_FALSE;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(rule);
	ADPT_NULL_POINT_CHECK(action);

	if (direction == FAL_PORT_VLAN_ALL)
		return SW_FAIL;

	/* all the entries have been checked */
	if (rule->index == (FAL_PORTVLAN_TRANS_TBL_NUM(direction) - 1))
		return SW_NOT_FOUND;

	if (FAL_PORT_ID_TYPE(port_id) == FAL_PORT_TYPE_PPORT) {
		rule->port_bitmap = BIT(port_id);
	} else {
		rule->port_bitmap = port_id;
	}

	for (idx = rule->index; idx < FAL_PORTVLAN_TRANS_TBL_NUM(direction); idx++) {
		aos_mem_zero(&temp_rule, sizeof (fal_vlan_trans_adv_rule_t));
		aos_mem_zero(&temp_action, sizeof (fal_vlan_trans_adv_action_t));
		rule_valid = _httppe_get_port_vlan_trans_adv_rule_by_index(dev_id, idx,
				direction, &temp_rule, &temp_action);
		if (rule_valid == 1)
		{ /* existing rule */
			entry_found = adpt_port_id_compare(temp_rule.port_bitmap,
					port_id);
			if (sign_tag == A_TRUE && (entry_found == ADPT_PORT_ID_EQUAL ||
						entry_found == ADPT_PORT_ID_INCLD))
			{
				aos_mem_copy(rule, &temp_rule,
						sizeof (fal_vlan_trans_adv_rule_t));
				rule->index = idx;
				aos_mem_copy(action, &temp_action,
						sizeof (fal_vlan_trans_adv_action_t));
				break;
			}
			if (!_httppe_check_if_rule_equal(direction, &temp_rule, rule))
			{ /* rule equal */
				if (!_httppe_check_if_action_equal(direction, &temp_action, action))
				{ /* action equal */
					if (entry_found == ADPT_PORT_ID_EQUAL ||
							entry_found == ADPT_PORT_ID_INCLD) {
						sign_tag = A_TRUE;
					}
				}
			}
		}
	}

	if (idx == FAL_PORTVLAN_TRANS_TBL_NUM(direction))
		return SW_NOT_FOUND;

	return rtn;
}

sw_error_t
adpt_httppe_port_vlantag_egmode_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t stag_mode, a_uint32_t ctag_mode, a_uint32_t mask)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	if (FAL_FLG_TST(mask, FAL_EGRESSMODE_CTAG_EN)) {
		rtn = httppe_port_eg_vlan_port_eg_vlan_ctag_mode_set(dev_id,
					port_value, ctag_mode);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(mask, FAL_EGRESSMODE_STAG_EN)) {
		rtn = httppe_port_eg_vlan_port_eg_vlan_stag_mode_set(dev_id,
				port_value, stag_mode);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_httppe_port_vlantag_egmode_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t *stag_mode, a_uint32_t *ctag_mode)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_eg_vlan_port_eg_vlan_ctag_mode_get(dev_id,
			port_value, ctag_mode);
	SW_RTN_ON_ERROR(rtn);

	rtn = httppe_port_eg_vlan_port_eg_vlan_stag_mode_get(dev_id,
			port_value, stag_mode);

	return rtn;
}

sw_error_t
adpt_httppe_port_vlantag_vsi_egmode_enable_set(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_eg_vlan_vsi_tag_mode_en_set(dev_id, port_value, enable);
	return rtn;
}

sw_error_t
adpt_httppe_port_vlantag_vsi_egmode_enable_get(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t *enable)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_eg_vlan_vsi_tag_mode_en_get(dev_id, port_value, enable);
	return rtn;
}

sw_error_t
adpt_httppe_port_eg_vlan_tx_counting_en_get(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t *value)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_eg_vlan_tx_counting_en_get(dev_id, port_value, value);
	return rtn;
}

sw_error_t
adpt_httppe_port_eg_vlan_tx_counting_en_set(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t value)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_eg_vlan_tx_counting_en_set(dev_id, port_value, value);
	return rtn;
}

sw_error_t
adpt_httppe_port_default_pcp_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_tag_t *default_tag)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	if (FAL_FLG_TST(default_tag->mask, FAL_PORT_VLAN_TAG_CPCP_EN)) {
		rv = httppe_port_def_pcp_port_def_cpcp_set(dev_id, port_value,
				(a_uint32_t)default_tag->cpri);
		SW_RTN_ON_ERROR(rv);
	}

	if (FAL_FLG_TST(default_tag->mask, FAL_PORT_VLAN_TAG_SPCP_EN)) {
		rv = httppe_port_def_pcp_port_def_spcp_set(dev_id, port_value,
				(a_uint32_t)default_tag->spri);
		SW_RTN_ON_ERROR(rv);
	}

	if (FAL_FLG_TST(default_tag->mask, FAL_PORT_VLAN_TAG_CDEI_EN)) {
		rv = httppe_port_def_pcp_port_def_cdei_set(dev_id, port_value,
				(a_uint32_t)default_tag->cdei);
		SW_RTN_ON_ERROR(rv);
	}

	if (FAL_FLG_TST(default_tag->mask, FAL_PORT_VLAN_TAG_SDEI_EN)) {
		rv = httppe_port_def_pcp_port_def_sdei_set(dev_id, port_value,
				(a_uint32_t)default_tag->sdei);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_httppe_port_default_pcp_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_tag_t *default_tag)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);
	a_uint32_t tmp = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rv = httppe_port_def_pcp_port_def_cpcp_get(dev_id, port_value, &tmp);
	SW_RTN_ON_ERROR(rv);
	default_tag->cpri = tmp;

	rv = httppe_port_def_pcp_port_def_spcp_get(dev_id, port_value, &tmp);
	SW_RTN_ON_ERROR(rv);
	default_tag->spri = tmp;

	rv = httppe_port_def_pcp_port_def_cdei_get(dev_id, port_value, &tmp);
	SW_RTN_ON_ERROR(rv);
	default_tag->cdei = tmp;

	rv = httppe_port_def_pcp_port_def_sdei_get(dev_id, port_value, &tmp);
	SW_RTN_ON_ERROR(rv);
	default_tag->sdei = tmp;

	return rv;
}

sw_error_t
adpt_httppe_port_vlan_xlt_miss_fwd_cmd_get(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t *value)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rv = httppe_port_vlan_config_port_vlan_xlt_miss_fwd_cmd_get(dev_id,
				FAL_PORT_ID_VALUE(port_id), value);

	return rv;
}

sw_error_t
adpt_httppe_port_vlan_xlt_miss_fwd_cmd_set(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t value)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rv = httppe_port_vlan_config_port_vlan_xlt_miss_fwd_cmd_set(dev_id,
				FAL_PORT_ID_VALUE(port_id), value);

	return rv;
}

sw_error_t
adpt_httppe_port_ingress_vlan_filter_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_ingress_vlan_filter_t *filter)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_vlan_config_port_in_vlan_fltr_cmd_get(dev_id,
			port_value, (a_uint32_t *)&filter->membership_filter);
	SW_RTN_ON_ERROR(rtn);
	rtn = httppe_port_vlan_config_port_untag_fltr_cmd_get(dev_id,
			port_value, (a_uint32_t *)&filter->untagged_filter);
	SW_RTN_ON_ERROR(rtn);
	rtn = httppe_port_vlan_config_port_tag_fltr_cmd_get(dev_id,
			port_value, (a_uint32_t *)&filter->tagged_filter);
	SW_RTN_ON_ERROR(rtn);
	rtn = httppe_port_vlan_config_port_pri_tag_fltr_cmd_get(dev_id,
			port_value, (a_uint32_t *)&filter->priority_filter);
	SW_RTN_ON_ERROR(rtn);


	return rtn;
}

sw_error_t
adpt_httppe_port_ingress_vlan_filter_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_ingress_vlan_filter_t *filter)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	rtn = httppe_port_vlan_config_port_in_vlan_fltr_cmd_set(dev_id,
			port_value, (a_uint32_t)filter->membership_filter);
	SW_RTN_ON_ERROR(rtn);
	rtn = httppe_port_vlan_config_port_untag_fltr_cmd_set(dev_id,
			port_value, (a_uint32_t)filter->untagged_filter);
	SW_RTN_ON_ERROR(rtn);
	rtn = httppe_port_vlan_config_port_tag_fltr_cmd_set(dev_id,
			port_value, (a_uint32_t)filter->tagged_filter);
	SW_RTN_ON_ERROR(rtn);
	rtn = httppe_port_vlan_config_port_pri_tag_fltr_cmd_set(dev_id,
			port_value, (a_uint32_t)filter->priority_filter);
	SW_RTN_ON_ERROR(rtn);

	return rtn;
}

sw_error_t
adpt_httppe_global_qinq_mode_set(a_uint32_t dev_id, fal_global_qinq_mode_t *mode)
{
	sw_error_t rtn = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (FAL_FLG_TST(mode->mask, FAL_GLOBAL_QINQ_MODE_EGRESS_EN)) {
		rtn = httppe_eg_bridge_config_bridge_type_set(dev_id,
				(a_uint32_t)mode->egress_mode);
		SW_RTN_ON_ERROR(rtn);
	}

	if (FAL_FLG_TST(mode->mask, FAL_GLOBAL_QINQ_MODE_EGRESS_UNTOUCHED_FOR_CPU_CODE)) {
		SW_RTN_ON_ERROR(httppe_eg_bridge_config_pkt_l2_edit_en_set(dev_id,
					(a_uint32_t)!mode->untouched_for_cpucode));
	}

	return rtn;
}

sw_error_t
adpt_httppe_global_qinq_mode_get(a_uint32_t dev_id, fal_global_qinq_mode_t *mode)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t l2_edit_en = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mode);

	rtn = httppe_eg_bridge_config_bridge_type_get(dev_id,
			(a_uint32_t *)&mode->egress_mode);

	SW_RTN_ON_ERROR(httppe_eg_bridge_config_pkt_l2_edit_en_get(dev_id, &l2_edit_en));

	mode->untouched_for_cpucode = !l2_edit_en;

	return rtn;
}

sw_error_t
adpt_httppe_portvlan_vpmember_get(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t * mem_port_map)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mem_port_map);
	port_id = FAL_PORT_ID_VALUE(port_id);

	return httppe_l2_vp_port_tbl_port_isolation_bitmap_get(dev_id, port_id, mem_port_map);
}

sw_error_t
adpt_httppe_portvlan_vpmember_update(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t mem_port_map)
{
	ADPT_DEV_ID_CHECK(dev_id);
	port_id = FAL_PORT_ID_VALUE(port_id);

	return httppe_l2_vp_port_tbl_port_isolation_bitmap_set(dev_id, port_id, (a_uint32_t)mem_port_map);
}

sw_error_t
adpt_httppe_portvlan_vpmember_add(a_uint32_t dev_id, fal_port_t port_id, fal_port_t mem_port_id)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_port_tbl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	port_id = FAL_PORT_ID_VALUE(port_id);
	mem_port_id = FAL_PORT_ID_VALUE(mem_port_id);

	rv = httppe_l2_vp_port_tbl_get(dev_id, port_id, &l2_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	l2_vp_port_tbl.bf.port_isolation_bitmap_0 |= (0x1 << mem_port_id);
	l2_vp_port_tbl.bf.port_isolation_bitmap_1 |= (0x1 << (mem_port_id - 1));

	rv = httppe_l2_vp_port_tbl_set(dev_id, port_id, &l2_vp_port_tbl);

	return rv;
}

sw_error_t
adpt_httppe_portvlan_vpmember_del(a_uint32_t dev_id, fal_port_t port_id, fal_port_t mem_port_id)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_port_tbl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	port_id = FAL_PORT_ID_VALUE(port_id);
	mem_port_id = FAL_PORT_ID_VALUE(mem_port_id);

	rv = httppe_l2_vp_port_tbl_get(dev_id, port_id, &l2_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	l2_vp_port_tbl.bf.port_isolation_bitmap_0 &= ~(0x1 << mem_port_id);
	l2_vp_port_tbl.bf.port_isolation_bitmap_1 &= ~(0x1 << (mem_port_id - 1));

	rv = httppe_l2_vp_port_tbl_set(dev_id, port_id, &l2_vp_port_tbl);

	return rv;
}

sw_error_t
adpt_httppe_vlan_trans_dscp_pcp_mapping_set(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp)
{
	sw_error_t rv = SW_OK;

	if (direction == FAL_PORT_VLAN_INGRESS || direction == FAL_PORT_VLAN_ALL) {
		switch(group_id) {
		case 0:
			rv = httppe_in_vlan_dscp_pbit_map_tbl_pcp_0_set(dev_id, dscp, pcp);
			break;
		case 1:
			rv = httppe_in_vlan_dscp_pbit_map_tbl_pcp_1_set(dev_id, dscp, pcp);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	}

	SW_RTN_ON_ERROR(rv);

	if (direction == FAL_PORT_VLAN_EGRESS || direction == FAL_PORT_VLAN_ALL) {
		switch(group_id) {
		case 0:
			rv = httppe_dscp_pbit_map_tbl_pcp_0_set(dev_id, dscp, pcp);
			break;
		case 1:
			rv = httppe_dscp_pbit_map_tbl_pcp_1_set(dev_id, dscp, pcp);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	}

	return rv;
}

sw_error_t
adpt_httppe_vlan_trans_dscp_pcp_mapping_get(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	if (direction == FAL_PORT_VLAN_INGRESS) {
		switch(group_id) {
		case 0:
			rv = httppe_in_vlan_dscp_pbit_map_tbl_pcp_0_get(dev_id, dscp, &val);
			break;
		case 1:
			rv = httppe_in_vlan_dscp_pbit_map_tbl_pcp_1_get(dev_id, dscp, &val);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	} else if (direction == FAL_PORT_VLAN_EGRESS) {
		switch (group_id) {
		case 0:
			rv = httppe_dscp_pbit_map_tbl_pcp_0_get(dev_id, dscp, &val);
			break;
		case 1:
			rv = httppe_dscp_pbit_map_tbl_pcp_1_get(dev_id, dscp, &val);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	} else {
		rv = SW_NOT_SUPPORTED;
	}

	*pcp = val;
	return rv;
}

sw_error_t
adpt_httppe_private_vlan_rx_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_ctrl_u rx_ctrl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);
	a_uint32_t index = port_value - 1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	if (port_value == SSDK_PHYSICAL_PORT0 || port_value > SSDK_MAX_PORT_NUM) {
		return SW_BAD_PARAM;
	}

	aos_mem_zero(&rx_ctrl, sizeof(union vlan_to_port_mapping_ctrl_u));

	rv = httppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &rx_ctrl);
	SW_RTN_ON_ERROR(rv);

	rx_ctrl.bf.private_tag_en = cfg->map_en;
	rx_ctrl.bf.tpid = cfg->tpid;

	rv = httppe_vlan_to_port_mapping_ctrl_set(dev_id, index, &rx_ctrl);

	return rv;
}

sw_error_t
adpt_httppe_private_vlan_rx_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_ctrl_u rx_ctrl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);
	a_uint32_t index = port_value - 1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	if (port_value == SSDK_PHYSICAL_PORT0 || port_value > SSDK_MAX_PORT_NUM) {
		return SW_BAD_PARAM;
	}

	aos_mem_zero(&rx_ctrl, sizeof(union vlan_to_port_mapping_ctrl_u));

	rv = httppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &rx_ctrl);
	SW_RTN_ON_ERROR(rv);

	cfg->map_en = rx_ctrl.bf.private_tag_en;
	cfg->tpid = rx_ctrl.bf.tpid;

	return SW_OK;
}

static sw_error_t adpt_httppe_vlan_2port_map_tbl_find(a_uint32_t dev_id,
		fal_pvtvlan_map_t *port_mapping, a_uint32_t *index)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_tbl_u vlan_2port;
	a_uint32_t i;

	for (i = 0; i < VLAN_TO_PORT_MAPPING_TBL_NUM; i++) {
		rv = httppe_vlan_to_port_mapping_tbl_get(dev_id, i, &vlan_2port);
		SW_RTN_ON_ERROR(rv);

		if (!vlan_2port.bf.valid)
			continue;

		if (port_mapping->vlan_tci == vlan_2port.bf.tci_value &&
			port_mapping->vlan_tci_mask == vlan_2port.bf.tci_mask) {
			*index = i;
			return SW_OK;
		}
	}

	return SW_NOT_FOUND;
}

static sw_error_t adpt_httppe_vlan_2port_map_tbl_insert(a_uint32_t dev_id,
		fal_pvtvlan_map_t *port_mapping, a_uint32_t *index)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_tbl_u vlan_2port;
	a_uint32_t i;

	for (i = 0; i < VLAN_TO_PORT_MAPPING_TBL_NUM; i++) {
		rv = httppe_vlan_to_port_mapping_tbl_get(dev_id, i, &vlan_2port);
		SW_RTN_ON_ERROR(rv);

		if (vlan_2port.bf.valid)
			continue;

		vlan_2port.bf.tci_value = port_mapping->vlan_tci;
		vlan_2port.bf.tci_mask = port_mapping->vlan_tci_mask;
		vlan_2port.bf.port_vp = port_mapping->int_port;
		vlan_2port.bf.valid = A_TRUE;
		*index = i;

		rv = httppe_vlan_to_port_mapping_tbl_set(dev_id, i, &vlan_2port);
		return rv;
	}

	return SW_NO_RESOURCE;
}

sw_error_t
adpt_httppe_private_vlan_mapping_set(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping)
{
	sw_error_t rv = SW_OK;
	a_bool_t need_rollback = A_FALSE;
	a_uint32_t ingress_index = 0;
	a_bool_t ingress_inserted = A_FALSE;
	union vlan_to_port_mapping_tbl_u ingress_backup;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_mapping);

	if (direction == FAL_DIR_INGRESS || direction == FAL_DIR_BOTH) {
		union vlan_to_port_mapping_tbl_u vlan_2port;
		a_uint32_t index;

		rv = adpt_httppe_vlan_2port_map_tbl_find(dev_id, port_mapping, &index);

		if (rv == SW_OK) {
			/* backup original ingress cfg/idx for fail rollback in in/eg cfg both configured case */
			if (direction == FAL_DIR_BOTH) {
				rv = httppe_vlan_to_port_mapping_tbl_get(dev_id, index, &ingress_backup);
				SW_RTN_ON_ERROR(rv);

				ingress_index = index;
				need_rollback = A_TRUE;
			}

			if (port_mapping->ptmap_en) {
				vlan_2port.bf.tci_value = port_mapping->vlan_tci;
				vlan_2port.bf.tci_mask = port_mapping->vlan_tci_mask;
				vlan_2port.bf.port_vp = port_mapping->int_port;
				vlan_2port.bf.valid = A_TRUE;
			} else {
				aos_mem_zero(&vlan_2port, sizeof(union vlan_to_port_mapping_tbl_u));
			}

			rv = httppe_vlan_to_port_mapping_tbl_set(dev_id, index, &vlan_2port);
			SW_RTN_ON_ERROR(rv);
		} else {
			if (port_mapping->ptmap_en) {
				rv = adpt_httppe_vlan_2port_map_tbl_insert(dev_id, port_mapping, &index);
				SW_RTN_ON_ERROR(rv);

				/* recored new entry indx for fail rollback in in/eg cfg both configured case */
				if (direction == FAL_DIR_BOTH) {
					ingress_inserted = A_TRUE;
					ingress_index = index;
					need_rollback = A_TRUE;
				}
			} else {
				return SW_NOT_FOUND;
			}
		}
	}

	if (direction == FAL_DIR_EGRESS || direction == FAL_DIR_BOTH) {
		union eg_vp_tbl_u port_2vlan;

		rv = httppe_egress_vp_tbl_get(dev_id, port_mapping->int_port, &port_2vlan);
		if (rv != SW_OK)
			goto rollback;

		if (port_mapping->ptmap_en) {
			port_2vlan.bf.private_tag_en = A_TRUE;
			port_2vlan.bf.private_tag_tci = port_mapping->vlan_tci;
		} else {
			port_2vlan.bf.private_tag_en = A_FALSE;
			port_2vlan.bf.private_tag_tci = 0;
		}

		rv = httppe_egress_vp_tbl_set(dev_id, port_mapping->int_port, &port_2vlan);
		if (rv != SW_OK)
			goto rollback;
	}

	return rv;

rollback:
	/* for in/eg cfg both configured case need to rollback ingress cfg when egress process failed */
	if (need_rollback) {
		sw_error_t rollback_rv = SW_OK;

		if (ingress_inserted) {
			union vlan_to_port_mapping_tbl_u vlan_2port_clear;
			aos_mem_zero(&vlan_2port_clear, sizeof(union vlan_to_port_mapping_tbl_u));

			rollback_rv = httppe_vlan_to_port_mapping_tbl_set(dev_id, ingress_index, &vlan_2port_clear);
		} else {
			rollback_rv = httppe_vlan_to_port_mapping_tbl_set(dev_id, ingress_index, &ingress_backup);
		}

		if (rollback_rv != SW_OK) {
			SSDK_ERROR("Rollback operation failed with error: %d, original error: %d\n", rollback_rv, rv);
		}
	}

	return rv;
}

sw_error_t
adpt_httppe_private_vlan_mapping_get(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping)
{
	sw_error_t rv = SW_OK;

	if (direction == FAL_DIR_INGRESS) {
		union vlan_to_port_mapping_tbl_u vlan_2port;
		a_uint32_t index;
		rv = adpt_httppe_vlan_2port_map_tbl_find(dev_id, port_mapping, &index);
		SW_RTN_ON_ERROR(rv);

		rv = httppe_vlan_to_port_mapping_tbl_get(dev_id, index, &vlan_2port);
		SW_RTN_ON_ERROR(rv);

		port_mapping->ptmap_en = vlan_2port.bf.valid;
		port_mapping->int_port = vlan_2port.bf.port_vp;
	} else if (direction == FAL_DIR_EGRESS) {
		union eg_vp_tbl_u port_2vlan;

		rv = httppe_egress_vp_tbl_get(dev_id, port_mapping->int_port, &port_2vlan);
		SW_RTN_ON_ERROR(rv);

		port_mapping->ptmap_en = port_2vlan.bf.private_tag_en;
		port_mapping->vlan_tci = port_2vlan.bf.private_tag_tci;
	} else
		return SW_NOT_SUPPORTED;

	return rv;
}

sw_error_t
adpt_httppe_private_vlan_tx_cfg_set(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union eg_global_ctrl_u tx_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	aos_mem_zero(&tx_ctrl, sizeof(union eg_global_ctrl_u));

	rv = httppe_eg_global_ctrl_get(dev_id, &tx_ctrl);
	SW_RTN_ON_ERROR(rv);

	tx_ctrl.bf.private_tag_tpid = cfg->tpid;

	rv = httppe_eg_global_ctrl_set(dev_id, &tx_ctrl);

	return rv;
}

sw_error_t
adpt_httppe_private_vlan_tx_cfg_get(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union eg_global_ctrl_u tx_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	aos_mem_zero(&tx_ctrl, sizeof(union eg_global_ctrl_u));

	rv = httppe_eg_global_ctrl_get(dev_id, &tx_ctrl);
	SW_RTN_ON_ERROR(rv);

	cfg->tpid = tx_ctrl.bf.private_tag_tpid;

	return SW_OK;
}

sw_error_t
adpt_httppe_port_isol_ctrl_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_pre_tbl;
	union l2_vp_port_post_tbl_u l2_vp_post_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(isol_ctrl);

	aos_mem_zero(&l2_vp_pre_tbl, sizeof(union l2_vp_port_tbl_u));
	aos_mem_zero(&l2_vp_post_tbl, sizeof(union l2_vp_port_post_tbl_u));

	if (isol_ctrl->dir == FAL_DIR_INGRESS || isol_ctrl->dir == FAL_DIR_BOTH) {
		rv = httppe_l2_vp_port_tbl_get(dev_id, port_value, &l2_vp_pre_tbl);
		SW_RTN_ON_ERROR(rv);

		l2_vp_pre_tbl.bf.isol_profile_en = isol_ctrl->enable;
		l2_vp_pre_tbl.bf.isol_profile = isol_ctrl->isol_group_id;

		rv = httppe_l2_vp_port_tbl_set(dev_id, port_value, &l2_vp_pre_tbl);
		SW_RTN_ON_ERROR(rv);
	}

	if (isol_ctrl->dir == FAL_DIR_EGRESS || isol_ctrl->dir == FAL_DIR_BOTH) {
		rv = httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_post_tbl);
		SW_RTN_ON_ERROR(rv);

		l2_vp_post_tbl.bf.isol_profile_en = isol_ctrl->enable;
		l2_vp_post_tbl.bf.isol_profile = isol_ctrl->isol_group_id;

		rv = httppe_l2_vp_port_post_tbl_set(dev_id, port_value, &l2_vp_post_tbl);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_httppe_port_isol_ctrl_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_pre_tbl;
	union l2_vp_port_post_tbl_u l2_vp_post_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(isol_ctrl);

	aos_mem_zero(&l2_vp_pre_tbl, sizeof(union l2_vp_port_tbl_u));
	aos_mem_zero(&l2_vp_post_tbl, sizeof(union l2_vp_port_post_tbl_u));

	rv = httppe_l2_vp_port_tbl_get(dev_id, port_value, &l2_vp_pre_tbl);
	SW_RTN_ON_ERROR(rv);
	rv = httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_post_tbl);
	SW_RTN_ON_ERROR(rv);

	if (isol_ctrl->dir == FAL_DIR_BOTH) {
		if (l2_vp_pre_tbl.bf.isol_profile_en != l2_vp_post_tbl.bf.isol_profile_en ||
			l2_vp_pre_tbl.bf.isol_profile != l2_vp_post_tbl.bf.isol_profile)
			return SW_NOT_SUPPORTED;

		isol_ctrl->enable = l2_vp_pre_tbl.bf.isol_profile_en;
		isol_ctrl->isol_group_id = l2_vp_pre_tbl.bf.isol_profile;
	}

	if (isol_ctrl->dir == FAL_DIR_INGRESS) {
		isol_ctrl->enable = l2_vp_pre_tbl.bf.isol_profile_en;
		isol_ctrl->isol_group_id = l2_vp_pre_tbl.bf.isol_profile;
	}

	if (isol_ctrl->dir == FAL_DIR_EGRESS) {
		isol_ctrl->enable = l2_vp_post_tbl.bf.isol_profile_en;
		isol_ctrl->isol_group_id = l2_vp_post_tbl.bf.isol_profile;
	}

	return rv;
}

sw_error_t
adpt_httppe_port_isol_group_set(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(isol_group_bmp);

	rv = httppe_vp_isol_tbl_vp_profile_map_set(dev_id,
			isol_group_id, *isol_group_bmp);

	return rv;
}

sw_error_t
adpt_httppe_port_isol_group_get(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(isol_group_bmp);

	rv = httppe_vp_isol_tbl_vp_profile_map_get(dev_id,
			isol_group_id, isol_group_bmp);

	return rv;
}

sw_error_t
adpt_httppe_port_isol_action_ctrl_set(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_global_conf_u reg_val;

	rv = httppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	reg_val.bf.bc_pvlan_isol_en = act_ctrl->bc_isol_en;
	reg_val.bf.mc_pvlan_isol_en = act_ctrl->mc_isol_en;

	rv = httppe_l2_global_conf_set(dev_id, &reg_val);

	return rv;
}

sw_error_t
adpt_httppe_port_isol_action_ctrl_get(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_global_conf_u reg_val;

	rv = httppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	act_ctrl->bc_isol_en = reg_val.bf.bc_pvlan_isol_en;
	act_ctrl->mc_isol_en = reg_val.bf.mc_pvlan_isol_en;

	return SW_OK;
}

sw_error_t
adpt_httppe_port_isol_action_set(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act)
{
	sw_error_t rv = SW_OK;
	union port_isol_action_u pp_isol_act;
	union vp_isol_action_tbl_u vp_isol_act;

	if (isol_id->isol_type == FAL_ISOL_ACT_PPORT) {
		rv = httppe_port_isol_action_get(dev_id, isol_id->pport_id, &pp_isol_act);
		SW_RTN_ON_ERROR(rv);

		pp_isol_act.bf.action = isol_act->act_bitmap[0] & ((1 << PORT_ISOL_ACTION_ACTION_LEN) - 1);

		rv = httppe_port_isol_action_set(dev_id, isol_id->pport_id, &pp_isol_act);
		SW_RTN_ON_ERROR(rv);
	} else {
		a_uint32_t i;

		rv = httppe_vp_isol_action_tbl_get(dev_id, isol_id->isol_group_id, &vp_isol_act);
		SW_RTN_ON_ERROR(rv);

		for (i = 0; i < FAL_ISOL_ACT_ARR_SIZE; i++) {
			vp_isol_act.val[i] = isol_act->act_bitmap[i];
		}

		rv = httppe_vp_isol_action_tbl_set(dev_id, isol_id->isol_group_id, &vp_isol_act);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_httppe_port_isol_action_get(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act)
{
	sw_error_t rv = SW_OK;
	union port_isol_action_u pp_isol_act;
	union vp_isol_action_tbl_u vp_isol_act;

	if (isol_id->isol_type == FAL_ISOL_ACT_PPORT) {
		rv = httppe_port_isol_action_get(dev_id, isol_id->pport_id, &pp_isol_act);
		SW_RTN_ON_ERROR(rv);

		isol_act->act_bitmap[0] = pp_isol_act.bf.action;
	} else {
		a_uint32_t i;

		rv = httppe_vp_isol_action_tbl_get(dev_id, isol_id->isol_group_id, &vp_isol_act);
		SW_RTN_ON_ERROR(rv);

		for (i = 0; i < FAL_ISOL_ACT_ARR_SIZE; i++) {
			isol_act->act_bitmap[i] = vp_isol_act.val[i];
		}
	}

	return rv;
}

#ifndef IN_PORTVLAN_MINI
sw_error_t
adpt_httppe_port_tag_propagation_set(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t mask, fal_port_vlan_direction_t direction,
		a_uint32_t value_pcp, a_uint32_t value_dei)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	if (direction == FAL_PORT_VLAN_INGRESS) {
		if (FAL_FLG_TST(mask, FAL_PORT_PROPAGATION_PCP_EN)) {
			rtn = httppe_port_vlan_config_port_in_pcp_prop_cmd_set(dev_id,
					port_value, value_pcp);
			SW_RTN_ON_ERROR(rtn);
		}
		if (FAL_FLG_TST(mask, FAL_PORT_PROPAGATION_DEI_EN)) {
			rtn = httppe_port_vlan_config_port_in_dei_prop_cmd_set(dev_id,
					port_value, value_dei);
			SW_RTN_ON_ERROR(rtn);
		}
	} else if (direction == FAL_PORT_VLAN_EGRESS) {
		if (FAL_FLG_TST(mask, FAL_PORT_PROPAGATION_PCP_EN)) {
			rtn = httppe_port_eg_vlan_port_eg_pcp_prop_cmd_set(dev_id,
					port_value, value_pcp);
			SW_RTN_ON_ERROR(rtn);
		}
		if (FAL_FLG_TST(mask, FAL_PORT_PROPAGATION_DEI_EN)) {
			rtn = httppe_port_eg_vlan_port_eg_dei_prop_cmd_set(dev_id,
					port_value, value_dei);
			SW_RTN_ON_ERROR(rtn);
		}
	} else
		return SW_NOT_SUPPORTED;

	return rtn;
}

sw_error_t
adpt_httppe_port_tag_propagation_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		a_uint32_t *value_pcp, a_uint32_t *value_dei)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_NOT_SUPPORTED;

	if (direction == FAL_PORT_VLAN_INGRESS) {
		rtn = httppe_port_vlan_config_port_in_pcp_prop_cmd_get(dev_id,
				port_value, value_pcp);
		SW_RTN_ON_ERROR(rtn);

		rtn = httppe_port_vlan_config_port_in_dei_prop_cmd_get(dev_id,
				port_value, value_dei);
		SW_RTN_ON_ERROR(rtn);
	} else if (direction == FAL_PORT_VLAN_EGRESS) {
		rtn = httppe_port_eg_vlan_port_eg_pcp_prop_cmd_get(dev_id,
				port_value, value_pcp);
		SW_RTN_ON_ERROR(rtn);

		rtn = httppe_port_eg_vlan_port_eg_dei_prop_cmd_get(dev_id,
				port_value, value_dei);
		SW_RTN_ON_ERROR(rtn);
	} else
		return SW_NOT_SUPPORTED;

	return rtn;
}

sw_error_t
adpt_httppe_port_vlan_counter_get(a_uint32_t dev_id,
		a_uint32_t cnt_index, fal_port_vlan_counter_t *counter)
{
	sw_error_t rtn = SW_OK;
	union vlan_dev_tx_counter_tbl_u vlan_dev_tx_counter_tbl;

	rtn = httppe_vlan_dev_tx_counter_tbl_get(dev_id, cnt_index, &vlan_dev_tx_counter_tbl);
	SW_RTN_ON_ERROR(rtn);

	counter->tx_packet_counter = vlan_dev_tx_counter_tbl.bf.tx_packets;
	counter->tx_byte_counter = ((a_uint64_t)vlan_dev_tx_counter_tbl.bf.tx_bytes_1 << 32) |
		vlan_dev_tx_counter_tbl.bf.tx_bytes_0;

	return rtn;
}

sw_error_t
adpt_httppe_port_vlan_counter_cleanup(a_uint32_t dev_id, a_uint32_t cnt_index)
{
	union vlan_dev_tx_counter_tbl_u vlan_dev_tx_counter_tbl;
	memset(&vlan_dev_tx_counter_tbl, 0, sizeof(union vlan_dev_tx_counter_tbl_u));

	return httppe_vlan_dev_tx_counter_tbl_set(dev_id, cnt_index, &vlan_dev_tx_counter_tbl);
}

sw_error_t
adpt_httppe_port_egress_vlan_filter_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_post_tbl_u l2_vp_port_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(filter);

	aos_mem_zero(&l2_vp_port_tbl, sizeof(l2_vp_port_tbl));

	rv = httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	l2_vp_port_tbl.bf.eg_vlan_fltr_cmd = filter->membership_filter;

	rv = httppe_l2_vp_port_post_tbl_set(dev_id, port_value, &l2_vp_port_tbl);
	return rv;
}

sw_error_t
adpt_httppe_port_egress_vlan_filter_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_post_tbl_u l2_vp_port_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(filter);

	aos_mem_zero(&l2_vp_port_tbl, sizeof(l2_vp_port_tbl));

	rv = httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	filter->membership_filter = l2_vp_port_tbl.bf.eg_vlan_fltr_cmd;

	return rv;
}
#endif
