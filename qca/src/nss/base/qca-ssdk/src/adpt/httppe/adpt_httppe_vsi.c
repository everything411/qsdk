/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "adpt.h"
#include "adpt_appe_vsi.h"
#include "hsl_htt_reg.h"

#define ADPT_VSI_MAX FAL_VSI_MAX
#define ADPT_VSI_STRIP_VLAN_TAG 2

enum{
	ADPT_VSI_ADD,
	ADPT_VSI_DEL
};

static a_bool_t _adpt_httppe_vsi_xlt_match(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t stag_vid, a_uint32_t ctag_vid, union xlt_rule_tbl_u *xlt_rule)
{
	struct xlt_rule_tbl_0 bf = xlt_rule->bf;

	if (bf.port_type != adpt_port_type_convert(A_TRUE, FAL_PORT_ID_TYPE(port_id))) {
		return A_FALSE;
	}
	switch (FAL_PORT_ID_TYPE(port_id)) {
		case FAL_PORT_TYPE_VPORT:
			if (bf.port_bitmap != FAL_PORT_ID_VALUE(port_id)) {
				return A_FALSE;
			}
			break;
		case FAL_PORT_TYPE_PPORT:
			if (!SW_IS_PBMP_MEMBER(bf.port_bitmap,
						FAL_PORT_ID_VALUE(port_id))) {
				return A_FALSE;
			}
			break;
		default:
			SSDK_ERROR("Unsupported port type");
			return A_FALSE;
	}

	if(stag_vid != FAL_VLAN_INVALID)
	{
		if((bf.skey_vid_incl) &&
			(bf.skey_vid == stag_vid))
		{
			if(ctag_vid != FAL_VLAN_INVALID)
			{
				if((bf.ckey_vid_incl) &&
					(bf.ckey_vid == ctag_vid))
				{
					return A_TRUE;
				}
			}
			else
			{
				if(!(bf.ckey_vid_incl))
				{
					return A_TRUE;
				}
			}
		}
	}
	else
	{
		if(!(bf.skey_vid_incl))
		{
			if(ctag_vid != FAL_VLAN_INVALID)
			{
				if((bf.ckey_vid_incl) &&
					(bf.ckey_vid == ctag_vid))
				{
					return A_TRUE;
				}
			}
			else
			{
				if(!(bf.ckey_vid_incl))
				{
					return A_TRUE;
				}
			}
		}
	}
	return A_FALSE;
}

static sw_error_t _adpt_httppe_vsi_xlt_update(a_uint32_t dev_id,
				a_uint32_t vsi_id, a_uint32_t port_id,
				a_uint32_t stag_vid, a_uint32_t ctag_vid,
				a_uint32_t op)
{
	a_int32_t index = 0;
	a_uint32_t new_entry = 0;
	sw_error_t rv;
	union xlt_rule_tbl_u xlt_rule;
	union xlt_action_tbl_u xlt_action;
	struct xlt_rule_tbl_0 bf;

	for(index = XLT_RULE_TBL_NUM - 1; index >= 0; index--)
	{
		rv = httppe_xlt_rule_tbl_get(dev_id, index, &xlt_rule);
		if( rv != SW_OK )
			return rv;
		rv = httppe_xlt_action_tbl_get(dev_id, index, &xlt_action);
		if( rv != SW_OK )
			return rv;
		bf = xlt_rule.bf;

		if(bf.valid == A_FALSE && index >= new_entry)
		{
			new_entry = index;
		}
		if(bf.valid == A_TRUE)
		{
			if(_adpt_httppe_vsi_xlt_match(dev_id, port_id,
				stag_vid, ctag_vid, &xlt_rule))
			{
				if((xlt_action.bf.vsi_cmd == A_TRUE) &&
					(xlt_action.bf.vsi == vsi_id))
					break;
			}
		}
	}

	if(index >= 0) /*found*/
	{
		if(op == ADPT_VSI_DEL)/*Delete*/
		{
			if (FAL_PORT_ID_TYPE(port_id) == FAL_PORT_TYPE_PPORT) {
				SW_PBMP_DEL_PORT(bf.port_bitmap, FAL_PORT_ID_VALUE(port_id));
			}
			if(bf.port_bitmap == 0 ||
					FAL_PORT_ID_TYPE(port_id) == FAL_PORT_TYPE_VPORT) {
				rv= httppe_xlt_rule_tbl_valid_set(dev_id, index, A_FALSE);
				return rv;
			}
		}
		else/*add*/
		{
			SW_PBMP_ADD_PORT(bf.port_bitmap, FAL_PORT_ID_VALUE(port_id));
		}
		rv = httppe_xlt_rule_tbl_port_bitmap_set(dev_id, index, bf.port_bitmap);
		return rv;
	}
	else/*not found*/
	{
		if(op == ADPT_VSI_DEL)
			return SW_OK;
		if(new_entry >= XLT_RULE_TBL_NUM)
		{
			SSDK_ERROR("%s,%d: port_id 0x%x svlan %d cvlan %d vsi %d table is full\n",
					__FUNCTION__, __LINE__,
					port_id, stag_vid, ctag_vid, vsi_id);
			return SW_NO_RESOURCE;
		}
		else/*new entry exist*/
		{
			aos_mem_zero(&xlt_rule, sizeof(union xlt_rule_tbl_u));

			xlt_rule.bf.port_type = adpt_port_type_convert(A_TRUE,
					FAL_PORT_ID_TYPE(port_id));
			if (FAL_PORT_ID_TYPE(port_id) == FAL_PORT_TYPE_VPORT) {
				xlt_rule.bf.port_bitmap = FAL_PORT_ID_VALUE(port_id);
			} else {
				xlt_rule.bf.port_bitmap = BIT(FAL_PORT_ID_VALUE(port_id));
			}
			xlt_rule.bf.valid = A_TRUE;

			if(ctag_vid != FAL_VLAN_INVALID)
			{
				xlt_rule.bf.ckey_vid_incl = A_TRUE;
				xlt_rule.bf.ckey_vid = ctag_vid;
			}

			/* match all formats, bit0-untagged, bit1-pri-tagged, bit2-tagged */
			xlt_rule.bf.ckey_fmt = 0x7;

			/* new added rule fields in JUHU/HMS/HTT, 0x7 to match all type */
			xlt_rule.bf.dhcp_type = 0x7;
			xlt_rule.bf.mc_type = 0x7;

			if(stag_vid != FAL_VLAN_INVALID)
			{
				xlt_rule.bf.skey_vid_incl = A_TRUE;
				xlt_rule.bf.skey_vid = stag_vid;
			}

			/* match all formats, bit0-untagged, bit1-pri-tagged, bit2-tagged */
			xlt_rule.bf.skey_fmt = 0x7;

			rv = httppe_xlt_rule_tbl_set(dev_id, new_entry, &xlt_rule);
			if( rv != SW_OK )
				return rv;
			aos_mem_zero(&xlt_action, sizeof(union xlt_action_tbl_u));
			xlt_action.bf.vsi_cmd = A_TRUE;
			xlt_action.bf.vsi = vsi_id;
			if(ctag_vid != FAL_VLAN_INVALID)
			{
				xlt_action.bf.xlt_cvid_cmd = ADPT_VSI_STRIP_VLAN_TAG;
			}
			if(stag_vid != FAL_VLAN_INVALID)
			{
				xlt_action.bf.xlt_svid_cmd = ADPT_VSI_STRIP_VLAN_TAG;
			}
			rv = httppe_xlt_action_tbl_set(dev_id, new_entry, &xlt_action);
			return rv;
		}
	}
}

sw_error_t
adpt_httppe_port_vlan_vsi_get(a_uint32_t dev_id, fal_port_t port_id,
				a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t *vsi_id)
{
	a_uint32_t index = 0;
	sw_error_t rv;
	union xlt_rule_tbl_u xlt_rule;
	union xlt_action_tbl_u xlt_action;

	if (FAL_PORT_ID_TYPE(port_id) != FAL_PORT_TYPE_VPORT &&
			FAL_PORT_ID_TYPE(port_id) != FAL_PORT_TYPE_PPORT) {
		SSDK_ERROR("Unsuppoted port type\n");
		return SW_NOT_SUPPORTED;
	}
	for(index = 0; index < XLT_RULE_TBL_NUM; index++)
	{
		rv = httppe_xlt_rule_tbl_get(dev_id, index, &xlt_rule);
		if( rv != SW_OK )
			return rv;
		rv = httppe_xlt_action_tbl_get(dev_id, index, &xlt_action);
		if( rv != SW_OK )
			return rv;

		if(xlt_rule.bf.valid == A_TRUE)
		{
			if(_adpt_httppe_vsi_xlt_match(dev_id, port_id,
					stag_vid, ctag_vid, &xlt_rule))
			{
				if (xlt_action.bf.vsi_cmd == A_TRUE) {
					*vsi_id = xlt_action.bf.vsi;
					return SW_OK;
				}
			}
		}
	}

	/*not found*/
	*vsi_id = FAL_VSI_INVALID;
	return SW_NOT_FOUND;
}

sw_error_t
adpt_httppe_port_vlan_vsi_set(a_uint32_t dev_id, fal_port_t port_id,
				a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t vsi_id)
{
	sw_error_t rv;
	a_uint32_t org_vsi;

	ADPT_DEV_ID_CHECK(dev_id);

	if((stag_vid != FAL_VLAN_INVALID && stag_vid > FAL_VLAN_MAX) ||
			(ctag_vid != FAL_VLAN_INVALID && ctag_vid > FAL_VLAN_MAX))
		return SW_OUT_OF_RANGE;

	if (FAL_PORT_ID_TYPE(port_id) != FAL_PORT_TYPE_VPORT &&
			FAL_PORT_ID_TYPE(port_id) != FAL_PORT_TYPE_PPORT) {
		SSDK_ERROR("Unsuppoted port type\n");
		return SW_NOT_SUPPORTED;
	}

	adpt_httppe_port_vlan_vsi_get(dev_id, port_id, stag_vid, ctag_vid, &org_vsi);

	if(org_vsi == vsi_id)
		return SW_OK;

	if(FAL_VSI_INVALID == vsi_id || org_vsi != FAL_VSI_INVALID)
	{
		rv = _adpt_httppe_vsi_xlt_update(dev_id, org_vsi, port_id,
				stag_vid, ctag_vid, ADPT_VSI_DEL);
		if(rv != SW_OK)
			return rv;
	}
	if(vsi_id != FAL_VSI_INVALID)
	{
		rv = _adpt_httppe_vsi_xlt_update(dev_id, vsi_id, port_id,
				stag_vid, ctag_vid, ADPT_VSI_ADD);
	}

	return rv;
}

sw_error_t
adpt_httppe_vsi_stamove_set(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_stamove_t *stamove)
{
	sw_error_t rv;
	union vsi_tbl_u vsi_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(stamove);

	rv = httppe_vsi_tbl_get( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	vsi_tbl.bf.station_move_lrn_en = stamove->stamove_en;
	vsi_tbl.bf.station_move_fwd_cmd = stamove->action;

	rv = httppe_vsi_tbl_set( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	return SW_OK;
}

sw_error_t
adpt_httppe_vsi_stamove_get(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_stamove_t *stamove)
{
	sw_error_t rv;
	union vsi_tbl_u vsi_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(stamove);

	rv = httppe_vsi_tbl_get( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	stamove->stamove_en = vsi_tbl.bf.station_move_lrn_en;
	stamove->action = vsi_tbl.bf.station_move_fwd_cmd;

	return SW_OK;
}

sw_error_t
adpt_httppe_vsi_newaddr_lrn_get(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_newaddr_lrn_t *newaddr_lrn)
{
	sw_error_t rv;
	union vsi_tbl_u vsi_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(newaddr_lrn);

	rv = httppe_vsi_tbl_get( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	newaddr_lrn->lrn_en = vsi_tbl.bf.new_addr_lrn_en;
	newaddr_lrn->action = vsi_tbl.bf.new_addr_fwd_cmd;

	return SW_OK;
}

sw_error_t
adpt_httppe_vsi_newaddr_lrn_set(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_newaddr_lrn_t *newaddr_lrn)
{
	sw_error_t rv;
	union vsi_tbl_u vsi_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(newaddr_lrn);

	rv = httppe_vsi_tbl_get( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	vsi_tbl.bf.new_addr_lrn_en = newaddr_lrn->lrn_en;
	vsi_tbl.bf.new_addr_fwd_cmd = newaddr_lrn->action;

	rv = httppe_vsi_tbl_set( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	return SW_OK;
}

sw_error_t
_adpt_httppe_vsi_vp_member_set(a_uint32_t dev_id, a_uint32_t vsi_id,
	fal_vsi_member_t *vsi_member)
{
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_2_set(dev_id, vsi_id,
		vsi_member->member_vports[0]));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_3_set(dev_id, vsi_id,
		vsi_member->member_vports[1]));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_4_set(dev_id, vsi_id,
		vsi_member->member_vports[2]));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_5_set(dev_id, vsi_id,
		vsi_member->member_vports[3]));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_6_set(dev_id, vsi_id,
		vsi_member->member_vports[4]));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_7_set(dev_id, vsi_id,
		vsi_member->member_vports[5]));

	return SW_OK;
}

sw_error_t
_adpt_httppe_vsi_vp_member_get(a_uint32_t dev_id, a_uint32_t vsi_id,
	fal_vsi_member_t *vsi_member)
{
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_2_get(dev_id, vsi_id,
		&(vsi_member->member_vports[0])));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_3_get(dev_id, vsi_id,
		&(vsi_member->member_vports[1])));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_4_get(dev_id, vsi_id,
		&(vsi_member->member_vports[2])));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_5_get(dev_id, vsi_id,
		&(vsi_member->member_vports[3])));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_6_get(dev_id, vsi_id,
		&(vsi_member->member_vports[4])));
	SW_RTN_ON_ERROR(httppe_vsi_remap_tbl_member_port_bitmap_7_get(dev_id, vsi_id,
		&(vsi_member->member_vports[5])));

	return SW_OK;
}

sw_error_t
adpt_httppe_vsi_member_set(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_member_t *vsi_member)
{
	sw_error_t rv;
	union vsi_tbl_u vsi_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vsi_member);

	rv = httppe_vsi_tbl_get( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	vsi_tbl.bf.bc_bitmap_0 = vsi_member->bc_ports;
	vsi_tbl.bf.bc_bitmap_1 = vsi_member->bc_ports >> 5;
	vsi_tbl.bf.member_port_bitmap = vsi_member->member_ports;
	vsi_tbl.bf.umc_bitmap = vsi_member->umc_ports;
	vsi_tbl.bf.uuc_bitmap = vsi_member->uuc_ports;

	rv = httppe_vsi_tbl_set(dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	rv = _adpt_httppe_vsi_vp_member_set(dev_id, vsi_id, vsi_member);
	SW_RTN_ON_ERROR(rv);

	return SW_OK;
}

sw_error_t
adpt_httppe_vsi_member_get(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_member_t *vsi_member)
{
	sw_error_t rv;
	union vsi_tbl_u vsi_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vsi_member);

	rv = httppe_vsi_tbl_get( dev_id, vsi_id, &vsi_tbl);
	SW_RTN_ON_ERROR(rv);

	vsi_member->bc_ports = vsi_tbl.bf.bc_bitmap_0 | ((vsi_tbl.bf.bc_bitmap_1 & 0xf) << 5);
	vsi_member->member_ports = vsi_tbl.bf.member_port_bitmap;
	vsi_member->umc_ports = vsi_tbl.bf.umc_bitmap;
	vsi_member->uuc_ports = vsi_tbl.bf.uuc_bitmap;

	rv = _adpt_httppe_vsi_vp_member_get(dev_id, vsi_id, vsi_member);
	SW_RTN_ON_ERROR(rv);

	return SW_OK;
}

sw_error_t
adpt_httppe_vsi_bridge_vsi_get(a_uint32_t dev_id, a_uint32_t vsi_id,
	fal_vsi_bridge_vsi_t *bridge_vsi)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(bridge_vsi);

	rv = httppe_vsi_remap_tbl_vsi_remap_en_get(dev_id, vsi_id,
		&(bridge_vsi->bridge_vsi_enable));
	SW_RTN_ON_ERROR(rv);

	rv = httppe_vsi_remap_tbl_br_vsi_get(dev_id, vsi_id,
		&(bridge_vsi->bridge_vsi_id));

	return rv;
}

sw_error_t
adpt_httppe_vsi_bridge_vsi_set(a_uint32_t dev_id, a_uint32_t vsi_id,
	fal_vsi_bridge_vsi_t *bridge_vsi)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	rv = httppe_vsi_remap_tbl_vsi_remap_en_set(dev_id, vsi_id,
		bridge_vsi->bridge_vsi_enable);
	SW_RTN_ON_ERROR(rv);

	rv = httppe_vsi_remap_tbl_br_vsi_set(dev_id, vsi_id,
		bridge_vsi->bridge_vsi_id);

	return rv;
}

sw_error_t
adpt_httppe_vsi_invalidvsi_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_vsi_invalidvsi_ctrl_t *invalidvsi_ctrl)
{
	sw_error_t rv = SW_OK;
	a_uint32_t dest_port = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(invalidvsi_ctrl);

	port_id = FAL_PORT_ID_VALUE(port_id);
	rv = httppe_l2_vp_port_tbl_invalid_vsi_forwarding_en_get(dev_id,
		port_id, &(invalidvsi_ctrl->dest_en));
	SW_RTN_ON_ERROR(rv);
	rv = httppe_l2_vp_port_tbl_dst_info_get(dev_id, port_id, &dest_port);
	SW_RTN_ON_ERROR(rv);
	invalidvsi_ctrl->dest_info.dest_info_type = FAL_DEST_INFO_PORT_ID;
	invalidvsi_ctrl->dest_info.dest_info_value = dest_port;

	return rv;
}

sw_error_t
adpt_httppe_vsi_invalidvsi_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_vsi_invalidvsi_ctrl_t *invalidvsi_ctrl)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(invalidvsi_ctrl);

	port_id = FAL_PORT_ID_VALUE(port_id);
	rv = httppe_l2_vp_port_tbl_invalid_vsi_forwarding_en_set(dev_id,
			port_id, invalidvsi_ctrl->dest_en);
	SW_RTN_ON_ERROR(rv);
	if(invalidvsi_ctrl->dest_info.dest_info_type != FAL_DEST_INFO_PORT_ID)
	{
		SSDK_ERROR ("dest_info_type:0x%x is not supported\n",
			invalidvsi_ctrl->dest_info.dest_info_type);
		return SW_NOT_SUPPORTED;
	}
	rv = httppe_l2_vp_port_tbl_dst_info_set(dev_id, port_id,
		invalidvsi_ctrl->dest_info.dest_info_value);

	return rv;
}

/**
 * @}
 */
