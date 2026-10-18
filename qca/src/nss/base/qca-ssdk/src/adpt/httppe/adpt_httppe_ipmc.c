/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_htt_reg.h"
#include "fal_ipmc.h"
#include "adpt.h"

#define ADPT_IPMC_RD_RSLT_DATA_NUM (sizeof(union ipmc_sipv6_gipv6_tbl_u)/sizeof(a_uint32_t) + 1)

#define IPMC_ENTRY_ID_CHECK(entry_index) \
do { \
    if (entry_index >= IPMC_GIPV4_TBL_NUM) \
        return SW_OUT_OF_RANGE; \
} while (0)

enum {
	HSL_VLAN_MATCH_VID = 0, /* match vid */
	HSL_VLAN_MATCH_VSI = 1, /* match vsi */
	HSL_VLAN_WITHOUT_MATCH = 2, /* not match */
};

enum {
	HSL_VLAN_FORMAT_NOT_CHECK = 0, /* not check */
	HSL_VLAN_FORMAT_UNTAG = 1, /* untagged */
	HSL_VLAN_FORMAT_TAG = 2, /* tagged and pri-tagged */
};

enum {
	HSL_SRC_NOT_CHECK = 0, /* not check */
	HSL_SRC_CHECK_PORT = 1, /* check src port */
	HSL_SRC_CHECK_GEM_PORT = 2, /* check src gem port */
};

static sw_error_t
adpt_httppe_ipmc_common_fields_convert(a_uint32_t dev_id, fal_ipmc_entry_t *entry,
		union ipmc_gipv4_tbl_u *tbl_entry, a_bool_t to_hsl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);
	ADPT_NULL_POINT_CHECK(tbl_entry);

	if (to_hsl) {
		tbl_entry->bf.valid = A_TRUE;
		tbl_entry->bf.key_type = entry->key_type;
		tbl_entry->bf.fwd_cmd = entry->fwd_cmd;
		tbl_entry->bf.syn_toggle = entry->syn_toggle;
		tbl_entry->bf.dst_info = ADPT_DEST_INFO(adpt_dest_type_convert(A_TRUE,
						entry->dst_info.dest_info_type),
							entry->dst_info.dest_info_value);
		if (entry->vlan_valid) {
			tbl_entry->bf.vid_fmt = entry->vlan_mode;
			tbl_entry->bf.vid = entry->vlan_id;
		} else {
			tbl_entry->bf.vid_fmt = HSL_VLAN_WITHOUT_MATCH;
		}

		if (entry->vlan_fmt_check_en)
			tbl_entry->bf.check_fmt = entry->vlan_fmt ?
				HSL_VLAN_FORMAT_TAG : HSL_VLAN_FORMAT_UNTAG;
		else
			tbl_entry->bf.check_fmt = HSL_VLAN_FORMAT_NOT_CHECK;

		if (entry->src_port_check_en) {
			tbl_entry->bf.check_src = (FAL_PORT_ID_TYPE(entry->src_port) ==
							FAL_PORT_TYPE_GEM_PORT) ?
							HSL_SRC_CHECK_GEM_PORT :
							HSL_SRC_CHECK_PORT;
			tbl_entry->bf.check_src_port = FAL_PORT_ID_VALUE(entry->src_port);
		} else {
			tbl_entry->bf.check_src = HSL_SRC_NOT_CHECK;
		}

		tbl_entry->bf.convert_uc = entry->ucast_fwd_en;
	}else {
		a_uint32_t port_type;

		entry->key_type = tbl_entry->bf.key_type;
		entry->fwd_cmd = tbl_entry->bf.fwd_cmd;
		entry->syn_toggle = tbl_entry->bf.syn_toggle;
		entry->dst_info.dest_info_type = adpt_dest_type_convert(A_FALSE,
							ADPT_DEST_TYPE(tbl_entry->bf.dst_info));
		entry->dst_info.dest_info_value = ADPT_DEST_VAL(tbl_entry->bf.dst_info);

		if (tbl_entry->bf.vid_fmt == HSL_VLAN_WITHOUT_MATCH) {
			entry->vlan_valid = A_FALSE;
		} else {
			entry->vlan_valid = A_TRUE;
			entry->vlan_mode = tbl_entry->bf.vid_fmt;
			entry->vlan_id = tbl_entry->bf.vid;
		}

		if (tbl_entry->bf.check_fmt == HSL_VLAN_FORMAT_NOT_CHECK) {
			entry->vlan_fmt_check_en = A_FALSE;
		} else {
			entry->vlan_fmt_check_en = A_TRUE;
			entry->vlan_fmt = (tbl_entry->bf.check_fmt == HSL_VLAN_FORMAT_TAG) ? 1 : 0;
		}

		if (tbl_entry->bf.check_src == HSL_SRC_NOT_CHECK) {
			entry->src_port_check_en = A_FALSE;
		} else {
			entry->src_port_check_en = A_TRUE;
			port_type = (tbl_entry->bf.check_src == HSL_SRC_CHECK_GEM_PORT) ?
				FAL_PORT_TYPE_GEM_PORT : FAL_PORT_TYPE_VPORT;
			entry->src_port = FAL_PORT_ID(port_type, tbl_entry->bf.check_src_port);
		}

		entry->ucast_fwd_en = tbl_entry->bf.convert_uc;
	}

	return SW_OK;
}

static sw_error_t
adpt_httppe_ipmc_gipv4_entry_convert(a_uint32_t dev_id, fal_ipmc_entry_t *entry,
		union ipmc_gipv4_tbl_u *tbl_entry, a_bool_t to_hsl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);
	ADPT_NULL_POINT_CHECK(tbl_entry);

	/* gipv4 address */
	if (to_hsl) {
		tbl_entry->bf.ip_addr_0 = entry->gip.ip4_addr;
		tbl_entry->bf.ip_addr_1 = (entry->gip.ip4_addr >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV4_TBL_IP_ADDR_OFFSET));
	} else {
		entry->gip.ip4_addr = (tbl_entry->bf.ip_addr_1 <<
				SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV4_TBL_IP_ADDR_OFFSET)) |
				tbl_entry->bf.ip_addr_0;
		entry->sip.ip4_addr = 0;
	}

	return SW_OK;
}

static sw_error_t
adpt_httppe_ipmc_sgipv4_entry_convert(a_uint32_t dev_id, fal_ipmc_entry_t *entry,
		union ipmc_sipv4_gipv4_tbl_u *tbl_entry, a_bool_t to_hsl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);
	ADPT_NULL_POINT_CHECK(tbl_entry);

	/* sipv4 and gipv4 address */
	if (to_hsl) {
		tbl_entry->bf.sip_addr_0 = entry->sip.ip4_addr;
		tbl_entry->bf.sip_addr_1 = (entry->sip.ip4_addr >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV4_GIPV4_TBL_SIP_ADDR_OFFSET));
		tbl_entry->bf.gip_addr_0 = entry->gip.ip4_addr;
		tbl_entry->bf.gip_addr_1 = (entry->gip.ip4_addr >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV4_GIPV4_TBL_GIP_ADDR_OFFSET));
	} else {
		entry->sip.ip4_addr = (tbl_entry->bf.sip_addr_1 <<
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV4_GIPV4_TBL_SIP_ADDR_OFFSET)) |
				tbl_entry->bf.sip_addr_0;
		entry->gip.ip4_addr = (tbl_entry->bf.gip_addr_1 <<
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV4_GIPV4_TBL_GIP_ADDR_OFFSET)) |
				tbl_entry->bf.gip_addr_0;
	}

	return SW_OK;
}

static sw_error_t
adpt_httppe_ipmc_gipv6_entry_convert(a_uint32_t dev_id, fal_ipmc_entry_t *entry,
		union ipmc_gipv6_tbl_u *tbl_entry, a_bool_t to_hsl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);
	ADPT_NULL_POINT_CHECK(tbl_entry);

	/* gipv6 address */
	if (to_hsl) {
		tbl_entry->bf.ipv6_addr_0 = entry->gip.ip6_addr.ul[3] &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET));

		tbl_entry->bf.ipv6_addr_1 = (entry->gip.ip6_addr.ul[3] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET)) |
			entry->gip.ip6_addr.ul[2] << IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.ipv6_addr_2 = (entry->gip.ip6_addr.ul[2] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET)) |
			entry->gip.ip6_addr.ul[1] << IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.ipv6_addr_3 = (entry->gip.ip6_addr.ul[1] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET)) |
			entry->gip.ip6_addr.ul[0] << IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.ipv6_addr_4 = entry->gip.ip6_addr.ul[0] >>
			SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET);
	}else {
		entry->gip.ip6_addr.ul[3] = (tbl_entry->bf.ipv6_addr_0 &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.ipv6_addr_1 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET));

		entry->gip.ip6_addr.ul[2] = ((tbl_entry->bf.ipv6_addr_1 >>
			IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.ipv6_addr_2 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET));

		entry->gip.ip6_addr.ul[1] = ((tbl_entry->bf.ipv6_addr_2 >>
			IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.ipv6_addr_3 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET));

		entry->gip.ip6_addr.ul[0] = ((tbl_entry->bf.ipv6_addr_3 >>
			IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.ipv6_addr_4 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET));

		aos_mem_zero(&entry->sip.ip6_addr, sizeof(entry->sip.ip6_addr));
	}

	return SW_OK;
}

static sw_error_t
adpt_httppe_ipmc_sgipv6_entry_convert(a_uint32_t dev_id, fal_ipmc_entry_t *entry,
		union ipmc_sipv6_gipv6_tbl_u *tbl_entry, a_bool_t to_hsl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);
	ADPT_NULL_POINT_CHECK(tbl_entry);

	/* gipv6 address */
	if (to_hsl) {
		tbl_entry->bf.gipv6_addr_0 = entry->gip.ip6_addr.ul[3] &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET));

		tbl_entry->bf.gipv6_addr_1 = (entry->gip.ip6_addr.ul[3] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET)) |
			entry->gip.ip6_addr.ul[2] << IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.gipv6_addr_2 = (entry->gip.ip6_addr.ul[2] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET)) |
			entry->gip.ip6_addr.ul[1] << IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.gipv6_addr_3 = (entry->gip.ip6_addr.ul[1] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET)) |
			entry->gip.ip6_addr.ul[0] << IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.gipv6_addr_4 = entry->gip.ip6_addr.ul[0] >>
			SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET);

		tbl_entry->bf.sipv6_addr_0 = entry->sip.ip6_addr.ul[3] &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET));

		tbl_entry->bf.sipv6_addr_1 = (entry->sip.ip6_addr.ul[3] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET)) |
			entry->sip.ip6_addr.ul[2] << IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.sipv6_addr_2 = (entry->sip.ip6_addr.ul[2] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET)) |
			entry->sip.ip6_addr.ul[1] << IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.sipv6_addr_3 = (entry->sip.ip6_addr.ul[1] >>
				SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET)) |
			entry->sip.ip6_addr.ul[0] << IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET % 32;

		tbl_entry->bf.sipv6_addr_4 = entry->sip.ip6_addr.ul[0] >>
			SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET);
	}else {
		entry->gip.ip6_addr.ul[3] = (tbl_entry->bf.gipv6_addr_0 &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.gipv6_addr_1 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET));

		entry->gip.ip6_addr.ul[2] = ((tbl_entry->bf.gipv6_addr_1 >>
			IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.gipv6_addr_2 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET));

		entry->gip.ip6_addr.ul[1] = ((tbl_entry->bf.gipv6_addr_2 >>
			IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.gipv6_addr_3 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET));

		entry->gip.ip6_addr.ul[0] = ((tbl_entry->bf.gipv6_addr_3 >>
			IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.gipv6_addr_4 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET));

		entry->sip.ip6_addr.ul[3] = (tbl_entry->bf.sipv6_addr_0 &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.sipv6_addr_1 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET));

		entry->sip.ip6_addr.ul[2] = ((tbl_entry->bf.sipv6_addr_1 >>
			IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.sipv6_addr_2 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET));

		entry->sip.ip6_addr.ul[1] = ((tbl_entry->bf.sipv6_addr_2 >>
			IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.sipv6_addr_3 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET));

		entry->sip.ip6_addr.ul[0] = ((tbl_entry->bf.sipv6_addr_3 >>
			IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET % 32) &
			BITS(0, SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET))) |
			(tbl_entry->bf.sipv6_addr_4 <<
			 SW_FIELD_OFFSET_IN_WORD(IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET));
	}

	return SW_OK;
}

sw_error_t
adpt_httppe_ipmc_entry_convert(a_uint32_t dev_id, fal_ipmc_entry_t *entry,
		a_uint32_t *data, a_uint32_t *data_size, a_bool_t to_hsl)
{
	sw_error_t rv;

	rv = adpt_httppe_ipmc_common_fields_convert(dev_id, entry,
			(union ipmc_gipv4_tbl_u *)data, to_hsl);
	SW_RTN_ON_ERROR(rv);

	switch(entry->key_type) {
	case FAL_IPMC_KEY_TYPE_GIP:
		rv = adpt_httppe_ipmc_gipv4_entry_convert(dev_id, entry,
				(union ipmc_gipv4_tbl_u *)data, to_hsl);
		*data_size = sizeof(union ipmc_gipv4_tbl_u)/sizeof(a_uint32_t);
		break;
	case FAL_IPMC_KEY_TYPE_SIP_GIP:
		rv = adpt_httppe_ipmc_sgipv4_entry_convert(dev_id, entry,
				(union ipmc_sipv4_gipv4_tbl_u *)data, to_hsl);
		*data_size = sizeof(union ipmc_sipv4_gipv4_tbl_u)/sizeof(a_uint32_t);
		break;
	case FAL_IPMC_KEY_TYPE_GIPV6:
		rv = adpt_httppe_ipmc_gipv6_entry_convert(dev_id, entry,
				(union ipmc_gipv6_tbl_u *)data, to_hsl);
		*data_size = sizeof(union ipmc_gipv6_tbl_u)/sizeof(a_uint32_t);
		break;
	case FAL_IPMC_KEY_TYPE_SIPV6_GIPV6:
		rv = adpt_httppe_ipmc_sgipv6_entry_convert(dev_id, entry,
				(union ipmc_sipv6_gipv6_tbl_u *)data, to_hsl);
		*data_size = sizeof(union ipmc_sipv6_gipv6_tbl_u)/sizeof(a_uint32_t);
		break;
	default:
		rv = SW_BAD_PARAM;
		break;
	}

	return rv;
}

static sw_error_t
httppe_ipmc_op_common(a_uint32_t dev_id,
		fal_ipmc_op_type_t op_type,
		fal_ipmc_op_mode_t op_mode,
		a_uint32_t *index)
{
	union ipmc_tbl_op_rslt_u op_rslt;
	union ipmc_tbl_op_u op;
	a_uint32_t loop = 0x100;
	sw_error_t rv = SW_OK;

	if (FAL_IPMC_OP_TYPE_GET == op_type) {
		SSDK_ERROR("%s bad op type: %d\n", __func__, op_type);
		return SW_BAD_PARAM;
	}

	op.bf.cmd_id = 0;
	op.bf.byp_rslt_en = 0;
	op.bf.op_type = op_type;
	op.bf.hash_block_bitmap = 0x3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;

	rv = httppe_ipmc_tbl_op_set(dev_id, &op);
	SW_RTN_ON_ERROR(rv);

	while (loop > 0) {
		rv = httppe_ipmc_tbl_op_rslt_get(dev_id, &op_rslt);
		SW_RTN_ON_ERROR(rv);

		if (op_rslt.bf.valid_cnt) {
			if (op_rslt.bf.op_rslt == FAL_IPMC_OP_RSLT_OK)
				*index = op_rslt.bf.entry_index;
			else
				rv = SW_FAIL;
			SSDK_DEBUG("%s rv: %d entry index: %d loop times: %d\n",
					__func__, rv, op_rslt.bf.entry_index, loop);
			break;
		}
		loop --;
	}

	if (loop == 0)
		rv = SW_BUSY;

	return rv;
}

static sw_error_t
httppe_ipmc_rd_op_common(
		a_uint32_t dev_id,
		fal_ipmc_op_type_t op_type,
		fal_ipmc_op_mode_t op_mode,
		a_uint32_t *index)
{
	union ipmc_tbl_rd_op_rslt_u rd_op_rslt;
	union ipmc_tbl_rd_op_u rd_op;
	a_uint32_t loop = 0x100;
	sw_error_t rv;

	if (FAL_IPMC_OP_TYPE_GET != op_type) {
		SSDK_ERROR("%s bad op type: %d\n", __func__, op_type);
		return SW_BAD_PARAM;
	}

	rd_op.bf.cmd_id = 0;
	rd_op.bf.byp_rslt_en = 0;
	rd_op.bf.op_type = op_type;
	rd_op.bf.hash_block_bitmap = 0x3;
	rd_op.bf.op_mode = op_mode;
	rd_op.bf.entry_index = *index;

	rv = httppe_ipmc_tbl_rd_op_set(dev_id, &rd_op);
	SW_RTN_ON_ERROR(rv);

	while (loop > 0) {
		rv = httppe_ipmc_tbl_rd_op_rslt_get(dev_id, &rd_op_rslt);
		SW_RTN_ON_ERROR(rv);

		if (rd_op_rslt.bf.valid_cnt) {
			if (rd_op_rslt.bf.op_rslt == FAL_IPMC_OP_RSLT_OK)
				*index = rd_op_rslt.bf.entry_index;
			else
				rv = SW_FAIL;
			SSDK_DEBUG("%s rv: %d entry index: %d loop times: %d\n",
					__func__, rv, rd_op_rslt.bf.entry_index, loop);
			break;
		}
		loop --;
	}

	if (loop == 0)
		rv = SW_BUSY;

	return rv;
}

static sw_error_t httppe_ipmc_entry_op(a_uint32_t dev_id,
	fal_ipmc_op_type_t op_type,
	fal_ipmc_op_mode_t op_mode,
	a_uint32_t *data,
	a_uint32_t data_size,
	a_uint32_t *index)
{
	sw_error_t ret, rv = SW_OK;
	a_uint32_t i, tmp_data;

	switch (op_type) {
	case FAL_IPMC_OP_TYPE_ADD:
	case FAL_IPMC_OP_TYPE_DEL:
		if (op_type == FAL_IPMC_OP_TYPE_ADD ||
			FAL_IPMC_OP_MODE_HASH == op_mode) {
			for (i = 0; i < data_size; i++) {
				rv = httppe_ipmc_tbl_op_data_set(dev_id, i, data[i]);
				SW_RTN_ON_ERROR(rv);
			}
		}

		rv = httppe_ipmc_op_common(dev_id, op_type, op_mode, index);
		SW_RTN_ON_ERROR(rv);
		break;
	case FAL_IPMC_OP_TYPE_FLUSH:
		rv = httppe_ipmc_op_common(dev_id, op_type, op_mode, index);
		SW_RTN_ON_ERROR(rv);
		break;
	case FAL_IPMC_OP_TYPE_GET:
		if (FAL_IPMC_OP_MODE_HASH == op_mode) {
			for (i = 0; i < data_size; i++) {
				rv = httppe_ipmc_tbl_rd_op_data_set(dev_id, i, data[i]);
				SW_RTN_ON_ERROR(rv);
			}
		}

		ret = httppe_ipmc_rd_op_common(dev_id, op_type, op_mode, index);

		for (i = 0; i < data_size; i++) {
			rv = httppe_ipmc_tbl_rd_rslt_data_get(dev_id, i, &data[i]);
			SW_RTN_ON_ERROR(rv);
		}

		/* For IPMC GET operation, need to read all the result data even
		 * the read operation is failed or valid data is no more than all
		 * the data number. OR next GET operation will be failed.
		 */
		for (i = data_size; i < ADPT_IPMC_RD_RSLT_DATA_NUM; i++) {
			rv = httppe_ipmc_tbl_rd_rslt_data_get(dev_id, i, &tmp_data);
			SW_RTN_ON_ERROR(rv);
		}
		SW_RTN_ON_ERROR(ret);
		break;
	default:
		SSDK_ERROR("%s bad op type: %d\n", __func__, op_type);
		return SW_BAD_PARAM;
	}

	return SW_OK;
}

sw_error_t adpt_httppe_ipmc_entry_get(a_uint32_t dev_id, fal_ipmc_op_mode_t get_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	union ipmc_sipv6_gipv6_tbl_u tbl_entry = {0};
	a_uint32_t data_size = ARRAY_SIZE(tbl_entry.val);
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ipmc_entry);

	if (get_mode == FAL_IPMC_OP_MODE_INDEX) {
		IPMC_ENTRY_ID_CHECK(ipmc_entry->entry_id);
	}

	if (get_mode == FAL_IPMC_OP_MODE_HASH) {
		rv = adpt_httppe_ipmc_entry_convert(dev_id, ipmc_entry,
				tbl_entry.val, &data_size, A_TRUE);
		SW_RTN_ON_ERROR(rv);
	}

	rv = httppe_ipmc_entry_op(dev_id, FAL_IPMC_OP_TYPE_GET,
				get_mode, tbl_entry.val, data_size, &ipmc_entry->entry_id);
	SW_RTN_ON_ERROR(rv);

	return adpt_httppe_ipmc_entry_convert(dev_id, ipmc_entry,
			tbl_entry.val, &data_size, A_FALSE);
}

sw_error_t adpt_httppe_ipmc_entry_add(a_uint32_t dev_id, fal_ipmc_op_mode_t add_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	union ipmc_sipv6_gipv6_tbl_u tbl_entry = {0};
	a_uint32_t data_size = 0;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ipmc_entry);

	if (add_mode == FAL_IPMC_OP_MODE_INDEX) {
		IPMC_ENTRY_ID_CHECK(ipmc_entry->entry_id);
	}

	rv = adpt_httppe_ipmc_entry_convert(dev_id, ipmc_entry, tbl_entry.val, &data_size, A_TRUE);
	SW_RTN_ON_ERROR(rv);

	rv = httppe_ipmc_entry_op(dev_id, FAL_IPMC_OP_TYPE_ADD,
			add_mode, tbl_entry.val, data_size, &ipmc_entry->entry_id);
	SW_RTN_ON_ERROR(rv);

	return adpt_httppe_ipmc_entry_get(dev_id, FAL_IPMC_OP_MODE_INDEX, ipmc_entry);
}

sw_error_t adpt_httppe_ipmc_entry_del(a_uint32_t dev_id, fal_ipmc_op_mode_t del_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	union ipmc_sipv6_gipv6_tbl_u tbl_entry = {0};
	a_uint32_t data_size = 0;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ipmc_entry);

	if (del_mode == FAL_IPMC_OP_MODE_INDEX) {
		IPMC_ENTRY_ID_CHECK(ipmc_entry->entry_id);
	}

	if (del_mode == FAL_IPMC_OP_MODE_HASH) {
		rv = adpt_httppe_ipmc_entry_convert(dev_id, ipmc_entry,
				tbl_entry.val, &data_size, A_TRUE);
		SW_RTN_ON_ERROR(rv);
	}

	return httppe_ipmc_entry_op(dev_id, FAL_IPMC_OP_TYPE_DEL,
			del_mode, tbl_entry.val, data_size, &ipmc_entry->entry_id);
}

sw_error_t adpt_httppe_ipmc_entry_flush(a_uint32_t dev_id)
{
	a_uint32_t index = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	return httppe_ipmc_entry_op(dev_id, FAL_IPMC_OP_TYPE_FLUSH,
			FAL_IPMC_OP_MODE_HASH, NULL, 0, &index);
}

sw_error_t adpt_httppe_ipmc_entry_getnext(a_uint32_t dev_id, fal_ipmc_next_mode_t next_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	a_uint32_t index = 0, step = 0;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ipmc_entry);

	switch (next_mode) {
	case FAL_IPMC_NEXT_MODE_GIP:
	case FAL_IPMC_NEXT_MODE_ALL:
		index = ipmc_entry->entry_id + 1;
		step = 1;
		break;
	case FAL_IPMC_NEXT_MODE_SIP_GIP:
	case FAL_IPMC_NEXT_MODE_GIPV6:
		index = (ipmc_entry->entry_id & ~1) + 2;
		step = 2;
		break;
	case FAL_IPMC_NEXT_MODE_SIPV6_GIPV6:
		index = (ipmc_entry->entry_id & ~3) + 4;
		step = 4;
		break;
	default:
		return SW_BAD_PARAM;
	}

	if (FAL_NEXT_ENTRY_FIRST_ID == ipmc_entry->entry_id)
		index = 0;

	while (index < IPMC_GIPV4_TBL_NUM) {
		ipmc_entry->entry_id = index;
		rv = adpt_httppe_ipmc_entry_get(dev_id, FAL_IPMC_OP_MODE_INDEX, ipmc_entry);
		if (rv == SW_OK) {
			if (next_mode != FAL_IPMC_NEXT_MODE_ALL) {
				fal_ipmc_key_type_t key_type = (fal_ipmc_key_type_t)next_mode;
				if (key_type == ipmc_entry->key_type)
					return SW_OK;
			} else {
				SSDK_DEBUG("ipmc output entry id %d, input entry id %d\n",
						ipmc_entry->entry_id, index);
				if (ipmc_entry->entry_id == index) {
					return SW_OK;
				} else if (ipmc_entry->entry_id < index) {
					if (ipmc_entry->key_type == FAL_IPMC_KEY_TYPE_SIPV6_GIPV6)
						index = (index & ~3) + 4;
					if (ipmc_entry->key_type == FAL_IPMC_KEY_TYPE_SIP_GIP ||
						ipmc_entry->key_type == FAL_IPMC_KEY_TYPE_GIPV6)
						index = (index & ~1) + 2;
					continue;
				}
			}
		}

		index += step;
	}

	return SW_FAIL;
}

sw_error_t adpt_httppe_ipmc_status_set(a_uint32_t dev_id, a_bool_t enable)
{
	union l2_global_conf_u reg_val;
	sw_error_t rv;

	rv = httppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	reg_val.bf.ipmc_en = enable;
	return httppe_l2_global_conf_set(dev_id, &reg_val);
}

sw_error_t adpt_httppe_ipmc_status_get(a_uint32_t dev_id, a_bool_t *enable)
{
	union l2_global_conf_u reg_val;
	sw_error_t rv;

	rv = httppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	*enable = reg_val.bf.ipmc_en;
	return SW_OK;
}

sw_error_t adpt_httppe_ipmc_global_cfg_set(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg)
{
	union l2_global_conf_u reg_val;
	sw_error_t rv;

	rv = httppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	reg_val.bf.mc_dmac_check_en = cfg->mc_dmac_check_en;
	reg_val.bf.mc_vlan_match_mode = cfg->vlan_mode;
	reg_val.bf.ipmc_mismatch_act = cfg->mismatch_action;
	reg_val.bf.ipmc_hash_mode_0 = cfg->hash_mode[0];
	reg_val.bf.ipmc_hash_mode_1 = cfg->hash_mode[1];

	return httppe_l2_global_conf_set(dev_id, &reg_val);
}

sw_error_t adpt_httppe_ipmc_global_cfg_get(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg)
{
	union l2_global_conf_u reg_val;
	sw_error_t rv;

	rv = httppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	cfg->mc_dmac_check_en = reg_val.bf.mc_dmac_check_en;
	cfg->vlan_mode = reg_val.bf.mc_vlan_match_mode;
	cfg->mismatch_action = reg_val.bf.ipmc_mismatch_act;
	cfg->hash_mode[0] = reg_val.bf.ipmc_hash_mode_0;
	cfg->hash_mode[1] = reg_val.bf.ipmc_hash_mode_1;

	return SW_OK;
}

sw_error_t adpt_httppe_ipmc_ucast_fwd_set(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd)
{
	union ipmc_convert_uc_ctrl_u reg_val;

	reg_val.bf.enable_port_num = ucast_fwd->ucast_fwd_en_port;
	reg_val.bf.convert_port_vp = FAL_PORT_ID_VALUE(ucast_fwd->dest_vp);
	reg_val.bf.service_code = ucast_fwd->service_code;
	reg_val.bf.post_bypass_bitmap = ucast_fwd->bypass_bitmap;

	return httppe_ipmc_convert_uc_ctrl_set(dev_id, &reg_val);
}

sw_error_t adpt_httppe_ipmc_ucast_fwd_get(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t rv;

	rv = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	ucast_fwd->ucast_fwd_en_port = reg_val.bf.enable_port_num;
	ucast_fwd->dest_vp = reg_val.bf.convert_port_vp;
	ucast_fwd->service_code = reg_val.bf.service_code;
	ucast_fwd->bypass_bitmap = reg_val.bf.post_bypass_bitmap;

	return SW_OK;
}

sw_error_t adpt_httppe_ipmc_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);
	ADPT_NULL_POINT_CHECK(p_adpt_api);

	p_adpt_api->adpt_ipmc_status_set = adpt_httppe_ipmc_status_set;
	p_adpt_api->adpt_ipmc_status_get = adpt_httppe_ipmc_status_get;
	p_adpt_api->adpt_ipmc_global_cfg_set = adpt_httppe_ipmc_global_cfg_set;
	p_adpt_api->adpt_ipmc_global_cfg_get = adpt_httppe_ipmc_global_cfg_get;
	p_adpt_api->adpt_ipmc_entry_add = adpt_httppe_ipmc_entry_add;
	p_adpt_api->adpt_ipmc_entry_del = adpt_httppe_ipmc_entry_del;
	p_adpt_api->adpt_ipmc_entry_get = adpt_httppe_ipmc_entry_get;
	p_adpt_api->adpt_ipmc_entry_getnext = adpt_httppe_ipmc_entry_getnext;
	p_adpt_api->adpt_ipmc_entry_flush = adpt_httppe_ipmc_entry_flush;
	p_adpt_api->adpt_ipmc_ucast_fwd_set = adpt_httppe_ipmc_ucast_fwd_set;
	p_adpt_api->adpt_ipmc_ucast_fwd_get = adpt_httppe_ipmc_ucast_fwd_get;
	return SW_OK;
}

/**
 * @}
 */
