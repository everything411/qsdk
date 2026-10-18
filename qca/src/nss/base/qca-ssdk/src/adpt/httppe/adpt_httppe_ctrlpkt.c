/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw.h"
#include "adpt.h"
#include "hsl_htt_reg.h"

sw_error_t
adpt_httppe_mgmtctrl_vpgroup_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t vpgroup_id)
{
	sw_error_t rv = SW_OK;

	rv = httppe_l2_vp_port_tbl_app_ctrl_profile_set(dev_id,
		FAL_PORT_ID_VALUE(port_id), vpgroup_id);

	return rv;
}

sw_error_t
adpt_httppe_mgmtctrl_vpgroup_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t *vpgroup_id)
{
	sw_error_t rv = SW_OK;

	rv = httppe_l2_vp_port_tbl_app_ctrl_profile_get(dev_id,
		FAL_PORT_ID_VALUE(port_id), vpgroup_id);

	return rv;
}

static a_uint32_t
_adpt_httppe_ctrlpkt_profile_get_by_index(a_uint32_t dev_id, a_uint32_t index,
	fal_ctrlpkt_profile_t *ctrlpkt)
{
	union app_ctrl_u entry;

	SW_RTN_ON_ERROR(httppe_app_ctrl_get(dev_id, index, &entry));

	ctrlpkt->action.action = entry.bf.cmd;
	ctrlpkt->action.sg_bypass = entry.bf.sg_byp;
	ctrlpkt->action.l2_filter_bypass = entry.bf.l2_sec_byp;
	ctrlpkt->action.in_stp_bypass = entry.bf.in_stg_byp;
	ctrlpkt->action.in_vlan_fltr_bypass = entry.bf.in_vlan_fltr_byp;

	if (entry.bf.portbitmap_include) {
		ctrlpkt->port_map = FAL_PORT_ID(adpt_port_type_convert(A_FALSE,
			entry.bf.port_type), entry.bf.portbitmap);
	}
	if (entry.bf.ethertype_include)
		ctrlpkt->ethtype_profile_bitmap = entry.bf.ethertype_index_bitmap_0 | (entry.bf.ethertype_index_bitmap_1 << 2);

	if (entry.bf.rfdb_include)
		ctrlpkt->rfdb_profile_bitmap = entry.bf.rfdb_index_bitmap_0| (entry.bf.rfdb_index_bitmap_1 << 30);

	if (entry.bf.protocol_include) {
		ctrlpkt->protocol_types.mgt_eapol = (entry.bf.protocol_bitmap & (0x1 << 0))?1:0;
		ctrlpkt->protocol_types.mgt_pppoe = (entry.bf.protocol_bitmap & (0x1 << 1))?1:0;
		ctrlpkt->protocol_types.mgt_igmp = (entry.bf.protocol_bitmap & (0x1 << 2))?1:0;
		ctrlpkt->protocol_types.mgt_arp_req = (entry.bf.protocol_bitmap & (0x1 << 3))?1:0;
		ctrlpkt->protocol_types.mgt_arp_rep = (entry.bf.protocol_bitmap & (0x1 << 4))?1:0;
		ctrlpkt->protocol_types.mgt_dhcp4 = (entry.bf.protocol_bitmap & (0x1 << 5))?1:0;
		ctrlpkt->protocol_types.mgt_mld = (entry.bf.protocol_bitmap & (0x1 << 6))?1:0;
		ctrlpkt->protocol_types.mgt_ns = (entry.bf.protocol_bitmap & (0x1 << 7))?1:0;
		ctrlpkt->protocol_types.mgt_na = (entry.bf.protocol_bitmap & (0x1 << 8))?1:0;
		ctrlpkt->protocol_types.mgt_dhcp6 = (entry.bf.protocol_bitmap & (0x1 << 9))?1:0;
		ctrlpkt->protocol_types.mgt_8023ah_oam =
			(entry.bf.protocol_bitmap & (0x1 << 10))?1:0;
	}

	return entry.bf.valid;
}

static a_bool_t
_adpt_httppe_ctrlpkt_equal_check(fal_ctrlpkt_profile_t *ctrlpkt1, fal_ctrlpkt_profile_t *ctrlpkt2)
{
	if (ctrlpkt1->action.action == ctrlpkt2->action.action &&
		ctrlpkt1->action.sg_bypass == ctrlpkt2->action.sg_bypass &&
		ctrlpkt1->action.l2_filter_bypass == ctrlpkt2->action.l2_filter_bypass &&
		ctrlpkt1->action.in_stp_bypass == ctrlpkt2->action.in_stp_bypass &&
		ctrlpkt1->action.in_vlan_fltr_bypass == ctrlpkt2->action.in_vlan_fltr_bypass &&
		ctrlpkt1->port_map == ctrlpkt2->port_map &&
		ctrlpkt1->ethtype_profile_bitmap == ctrlpkt2->ethtype_profile_bitmap &&
		ctrlpkt1->rfdb_profile_bitmap == ctrlpkt2->rfdb_profile_bitmap &&
		ctrlpkt1->protocol_types.mgt_eapol == ctrlpkt2->protocol_types.mgt_eapol &&
		ctrlpkt1->protocol_types.mgt_pppoe == ctrlpkt2->protocol_types.mgt_pppoe &&
		ctrlpkt1->protocol_types.mgt_igmp == ctrlpkt2->protocol_types.mgt_igmp &&
		ctrlpkt1->protocol_types.mgt_arp_req == ctrlpkt2->protocol_types.mgt_arp_req &&
		ctrlpkt1->protocol_types.mgt_arp_rep == ctrlpkt2->protocol_types.mgt_arp_rep &&
		ctrlpkt1->protocol_types.mgt_dhcp4 == ctrlpkt2->protocol_types.mgt_dhcp4 &&
		ctrlpkt1->protocol_types.mgt_mld == ctrlpkt2->protocol_types.mgt_mld &&
		ctrlpkt1->protocol_types.mgt_ns == ctrlpkt2->protocol_types.mgt_ns &&
		ctrlpkt1->protocol_types.mgt_na == ctrlpkt2->protocol_types.mgt_na &&
		ctrlpkt1->protocol_types.mgt_dhcp6 == ctrlpkt2->protocol_types.mgt_dhcp6 &&
		ctrlpkt1->protocol_types.mgt_8023ah_oam == ctrlpkt2->protocol_types.mgt_8023ah_oam)
		return A_TRUE;

	return A_FALSE;
}

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_add(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
{
	union app_ctrl_u entry;
	a_uint32_t index, ctrlpkt_valid, entry_sign, entry_index;
	fal_ctrlpkt_profile_t ctrlpkt_temp;

	ADPT_DEV_ID_CHECK(dev_id);

	entry_index = 0;
	entry_sign = 0;
	for (index = 0; index < APP_CTRL_MAX_ENTRY; index++)
	{
		memset(&ctrlpkt_temp, 0, sizeof(fal_ctrlpkt_profile_t));
		ctrlpkt_valid = _adpt_httppe_ctrlpkt_profile_get_by_index(dev_id, index, &ctrlpkt_temp);
		if (ctrlpkt_valid == 1)
		{
			if (_adpt_httppe_ctrlpkt_equal_check(&ctrlpkt_temp, ctrlpkt))
				return SW_ALREADY_EXIST;
		}
		else
		{
			if (entry_sign == 0) {
				entry_index = index;
				entry_sign = 1;
			}
		}
	}

	if (entry_sign == 0)
		return SW_NO_RESOURCE;

	memset(&entry, 0, sizeof(union app_ctrl_u));

	entry.bf.valid = A_TRUE;
	entry.bf.rfdb_include = ctrlpkt->rfdb_profile_bitmap?1:0;
	entry.bf.rfdb_index_bitmap_0 = (ctrlpkt->rfdb_profile_bitmap & 0x3fffffff);
	entry.bf.rfdb_index_bitmap_1 = (ctrlpkt->rfdb_profile_bitmap >> 30);

	if (ctrlpkt->protocol_types.mgt_eapol)
		entry.bf.protocol_bitmap |= (0x1 << 0);
	if (ctrlpkt->protocol_types.mgt_pppoe)
		entry.bf.protocol_bitmap |= (0x1 << 1);
	if (ctrlpkt->protocol_types.mgt_igmp)
		entry.bf.protocol_bitmap |= (0x1 << 2);
	if (ctrlpkt->protocol_types.mgt_arp_req)
		entry.bf.protocol_bitmap |= (0x1 << 3);
	if (ctrlpkt->protocol_types.mgt_arp_rep)
		entry.bf.protocol_bitmap |= (0x1 << 4);
	if (ctrlpkt->protocol_types.mgt_dhcp4)
		entry.bf.protocol_bitmap |= (0x1 << 5);
	if (ctrlpkt->protocol_types.mgt_mld)
		entry.bf.protocol_bitmap |= (0x1 << 6);
	if (ctrlpkt->protocol_types.mgt_ns)
		entry.bf.protocol_bitmap |= (0x1 << 7);
	if (ctrlpkt->protocol_types.mgt_na)
		entry.bf.protocol_bitmap |= (0x1 << 8);
	if (ctrlpkt->protocol_types.mgt_dhcp6)
		entry.bf.protocol_bitmap |= (0x1 << 9);
	if (ctrlpkt->protocol_types.mgt_8023ah_oam)
		entry.bf.protocol_bitmap |= (0x1 << 10);
	entry.bf.protocol_include = entry.bf.protocol_bitmap?1:0;

	entry.bf.ethertype_include = ctrlpkt->ethtype_profile_bitmap?1:0;
	entry.bf.ethertype_index_bitmap_0 = (ctrlpkt->ethtype_profile_bitmap & 0x3);
	entry.bf.ethertype_index_bitmap_1 = (ctrlpkt->ethtype_profile_bitmap >> 2);

	entry.bf.portbitmap_include = ctrlpkt->port_map?1:0;
	entry.bf.port_type = adpt_port_type_convert(A_TRUE,
		FAL_PORT_ID_TYPE(ctrlpkt->port_map));
	entry.bf.portbitmap = FAL_PORT_ID_VALUE (ctrlpkt->port_map);
	entry.bf.in_vlan_fltr_byp = ctrlpkt->action.in_vlan_fltr_bypass?1:0;
	entry.bf.in_stg_byp = ctrlpkt->action.in_stp_bypass?1:0;
	entry.bf.l2_sec_byp = ctrlpkt->action.l2_filter_bypass?1:0;
	entry.bf.sg_byp = ctrlpkt->action.sg_bypass?1:0;
	entry.bf.cmd = (a_uint32_t)ctrlpkt->action.action;
	SW_RTN_ON_ERROR(httppe_app_ctrl_set(dev_id, entry_index, &entry));

	return SW_OK;
}

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_del(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
{
	a_uint32_t index, ctrlpkt_valid;
	union app_ctrl_u entry;
	fal_ctrlpkt_profile_t ctrlpkt_temp;

	memset(&entry, 0, sizeof(union app_ctrl_u));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrlpkt);

	for (index = 0; index < APP_CTRL_MAX_ENTRY; index++)
	{
		memset(&ctrlpkt_temp, 0, sizeof(fal_ctrlpkt_profile_t));
		ctrlpkt_valid = _adpt_httppe_ctrlpkt_profile_get_by_index(dev_id, index, &ctrlpkt_temp);
		if (ctrlpkt_valid == 1)
		{
			if (_adpt_httppe_ctrlpkt_equal_check(&ctrlpkt_temp, ctrlpkt))
			{
				SW_RTN_ON_ERROR(httppe_app_ctrl_set(dev_id, index, &entry));
				return SW_OK;
			}
		}
	}

	return SW_NOT_FOUND;
}

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_getfirst(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
{
	a_uint32_t index, ctrlpkt_valid;

	for (index = 0; index < APP_CTRL_MAX_ENTRY; index++)
	{
		ctrlpkt_valid = _adpt_httppe_ctrlpkt_profile_get_by_index(dev_id, index, ctrlpkt);
		if (ctrlpkt_valid == 1)
			return SW_OK;
	}

	return SW_NO_MORE;
}

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_getnext(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
{
	a_uint32_t index, ctrlpkt_valid, sign_tag;
	fal_ctrlpkt_profile_t ctrlpkt_temp;

	sign_tag = 0;

	for (index = 0; index < APP_CTRL_MAX_ENTRY; index++)
	{
		memset(&ctrlpkt_temp, 0, sizeof(fal_ctrlpkt_profile_t));
		ctrlpkt_valid = _adpt_httppe_ctrlpkt_profile_get_by_index(dev_id, index, &ctrlpkt_temp);
		if (ctrlpkt_valid == 1)
		{
			if (sign_tag == 1) {
				aos_mem_copy(ctrlpkt, &ctrlpkt_temp, sizeof(fal_ctrlpkt_profile_t));
				return SW_OK;
			}
			if (_adpt_httppe_ctrlpkt_equal_check(&ctrlpkt_temp, ctrlpkt))
				sign_tag = 1;
		}
	}

	return SW_NO_MORE;
}
/**
 * @}
 */
