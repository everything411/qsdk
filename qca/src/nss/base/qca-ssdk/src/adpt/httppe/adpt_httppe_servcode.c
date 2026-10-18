/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "adpt.h"
#include "hsl_htt_reg.h"

sw_error_t adpt_httppe_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index,
					   fal_servcode_config_t *entry)
{
	union in_l2_service_tbl_u in_l2_service_tbl = {0};
	union eg_service_tbl_u eg_service_tbl = {0};
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	/* in l2 service table configurations */
	in_l2_service_tbl.bf.dst_port_id_valid = entry->dest_port_valid;
	in_l2_service_tbl.bf.dst_port_id = entry->dest_port_id;
	in_l2_service_tbl.bf.direction = entry->direction;
	in_l2_service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[1];
	in_l2_service_tbl.bf.rx_cnt_en = (entry->bypass_bitmap[2] >> 1) & 0x1;
	in_l2_service_tbl.bf.tx_cnt_en = (entry->bypass_bitmap[2] >> 3) & 0x1;

	in_l2_service_tbl.bf.bypass_bitmap_ext = entry->bypass_bitmap[1] >> SMAC_MC_DROP_BYP;
	in_l2_service_tbl.bf.bypass_bitmap_new = entry->bypass_bitmap[1] >> DOT1P_EG_PORT_OVERRIDE;
	in_l2_service_tbl.bf.post_bypass_bitmap = entry->bypass_bitmap[1] >> QM_QID_MISMATCH_BYPASS;

	rv = httppe_in_l2_service_tbl_set(dev_id, servcode_index, &in_l2_service_tbl);
	SW_RTN_ON_ERROR(rv);

	/* eg service table configurations */
	rv = httppe_eg_service_tbl_get(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);
	/*do not touch the athtag configurations*/
	eg_service_tbl.bf.field_update_action &= ATHTAG_UPDATE;
	eg_service_tbl.bf.field_update_action |= entry->field_update_bitmap[0];

	/* extend field update action */
	eg_service_tbl.bf.field_update_action_ext_0 = entry->field_update_bitmap[0] >>
		FLD_UPDATE_VLAN_XLT_DROP_BYPASS;
	eg_service_tbl.bf.field_update_action_ext_1 = entry->field_update_bitmap[0] >>
		(FLD_UPDATE_VLAN_XLT_DROP_BYPASS +
		 SW_FIELD_OFFSET_IN_WORD(EG_SERVICE_TBL_FIELD_UPDATE_ACTION_EXT_OFFSET));
	eg_service_tbl.bf.field_update_action_ext1_0 = entry->field_update_bitmap[0] >>
		FLD_UP_EG_CNT_VSI_BYPASS | entry->field_update_bitmap[1] <<
		(BITS_PER_TYPE(u64) - FLD_UP_EG_CNT_VSI_BYPASS);
	eg_service_tbl.bf.field_update_action_ext1_1 = entry->field_update_bitmap[1] >>
		(SW_FIELD_OFFSET_IN_WORD(EG_SERVICE_TBL_FIELD_UPDATE_ACTION_EXT1_OFFSET) -
		 (BITS_PER_TYPE(u64) - FLD_UP_EG_CNT_VSI_BYPASS));

	eg_service_tbl.bf.next_service_code = entry->next_service_code;
	eg_service_tbl.bf.hw_services = entry->hw_services;
	eg_service_tbl.bf.offset_sel = entry->offset_sel;
	eg_service_tbl.bf.tx_counting_en = (entry->bypass_bitmap[2] >> 2) & 0x1;

	return httppe_eg_service_tbl_set(dev_id, servcode_index, &eg_service_tbl);
}

sw_error_t adpt_httppe_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index,
					   fal_servcode_config_t *entry)
{
	union in_l2_service_tbl_u in_l2_service_tbl;
	union eg_service_tbl_u eg_service_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	/* in l2 service table configurations */
	rv = httppe_in_l2_service_tbl_get(dev_id, servcode_index, &in_l2_service_tbl);
	SW_RTN_ON_ERROR(rv);
	entry->dest_port_valid = in_l2_service_tbl.bf.dst_port_id_valid;
	entry->dest_port_id = in_l2_service_tbl.bf.dst_port_id;
	entry->direction = in_l2_service_tbl.bf.direction;
	entry->bypass_bitmap[1] = in_l2_service_tbl.bf.bypass_bitmap;

	entry->bypass_bitmap[1] |= (a_uint64_t)(in_l2_service_tbl.bf.bypass_bitmap_ext) <<
						SMAC_MC_DROP_BYP;
	entry->bypass_bitmap[1] |= (a_uint64_t)(in_l2_service_tbl.bf.bypass_bitmap_new) <<
						DOT1P_EG_PORT_OVERRIDE;
	entry->bypass_bitmap[1] |= (a_uint64_t)(in_l2_service_tbl.bf.post_bypass_bitmap) <<
						QM_QID_MISMATCH_BYPASS;

	entry->bypass_bitmap[2] |= in_l2_service_tbl.bf.rx_cnt_en << 1;
	entry->bypass_bitmap[2] |= in_l2_service_tbl.bf.tx_cnt_en << 3;

	/* eg service table configurations */
	rv = httppe_eg_service_tbl_get(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);

	entry->field_update_bitmap[0] = eg_service_tbl.bf.field_update_action;
	entry->field_update_bitmap[0] |= (a_uint64_t)(eg_service_tbl.bf.field_update_action_ext_0 |
		eg_service_tbl.bf.field_update_action_ext_1 <<
		SW_FIELD_OFFSET_IN_WORD(EG_SERVICE_TBL_FIELD_UPDATE_ACTION_EXT_OFFSET)) <<
		FLD_UPDATE_VLAN_XLT_DROP_BYPASS;
	entry->field_update_bitmap[0] |= (a_uint64_t)(eg_service_tbl.bf.field_update_action_ext1_0)
		<< FLD_UP_EG_CNT_VSI_BYPASS;
	entry->field_update_bitmap[1] = (a_uint64_t)(eg_service_tbl.bf.field_update_action_ext1_0 |
		eg_service_tbl.bf.field_update_action_ext1_1 <<
		SW_FIELD_OFFSET_IN_WORD(EG_SERVICE_TBL_FIELD_UPDATE_ACTION_EXT1_OFFSET)) >>
		(BITS_PER_TYPE(u64) - FLD_UP_EG_CNT_VSI_BYPASS);
	entry->next_service_code = eg_service_tbl.bf.next_service_code;
	entry->hw_services = eg_service_tbl.bf.hw_services;
	entry->offset_sel = eg_service_tbl.bf.offset_sel;
	entry->bypass_bitmap[2] |= eg_service_tbl.bf.tx_counting_en << 2;

	return SW_OK;
}

/**
 * @}
 */
