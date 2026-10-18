/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#include "hsl_reg.h"
#include "fal_servcode.h"
#include "adpt.h"

sw_error_t adpt_jhppe_spec_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index,
	fal_servcode_config_t *entry)
{
	union tl_spec_service_tbl_u tl_service_tbl = {0};
	union in_spec_service_tbl_u in_service_tbl = {0};
	union l2_spec_service_tbl_u l2_service_tbl = {0};
	union eg_spec_service_tbl_u eg_service_tbl = {0};
	sw_error_t rv;

	/* eg spec service table max entry num is larger than others */
	if (servcode_index >= TL_SPEC_SERVICE_TBL_MAX_ENTRY)
		goto eg_servcode_config;

	/* tl spec service table config */
	rv = jhppe_tl_spec_service_tbl_get(dev_id, servcode_index, &tl_service_tbl);
	SW_RTN_ON_ERROR(rv);

	tl_service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[3];
	rv = jhppe_tl_spec_service_tbl_set(dev_id, servcode_index, &tl_service_tbl);
	SW_RTN_ON_ERROR(rv);

	/* in spec service table config */
	rv = jhppe_in_spec_service_tbl_get(dev_id, servcode_index, &in_service_tbl);
	SW_RTN_ON_ERROR(rv);

	in_service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[0];
	in_service_tbl.bf.rx_counting_en = entry->bypass_bitmap[2] & 0x1;
	rv = jhppe_in_spec_service_tbl_set(dev_id, servcode_index, &in_service_tbl);
	SW_RTN_ON_ERROR(rv);

	/* l2 spec service table config */
	rv = jhppe_l2_spec_service_tbl_get(dev_id, servcode_index, &l2_service_tbl);
	SW_RTN_ON_ERROR(rv);

	l2_service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[1];
	l2_service_tbl.bf.rx_cnt_en = (entry->bypass_bitmap[2] >> 1) & 0x1;
	l2_service_tbl.bf.tx_cnt_en = (entry->bypass_bitmap[2] >> 3) & 0x1;
	l2_service_tbl.bf.bypass_bitmap_ext_0 = (entry->bypass_bitmap[1] >> SMAC_MC_DROP_BYP);
	l2_service_tbl.bf.bypass_bitmap_ext_1 = (entry->bypass_bitmap[1] >> SMAC_MC_DROP_BYP) >>
		SW_FIELD_OFFSET_IN_WORD(L2_SPEC_SERVICE_TBL_BYPASS_BITMAP_EXT_OFFSET);

	l2_service_tbl.bf.bypass_bitmap_new = entry->bypass_bitmap[1] >> DOT1P_EG_PORT_OVERRIDE;
	l2_service_tbl.bf.post_bypass_bitmap = entry->bypass_bitmap[1] >> QM_QID_MISMATCH_BYPASS;
	rv = jhppe_l2_spec_service_tbl_set(dev_id, servcode_index, &l2_service_tbl);
	SW_RTN_ON_ERROR(rv);

eg_servcode_config:
	/* eg spec service table config */
	if (servcode_index >= EG_SPEC_SERVICE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	rv = jhppe_eg_spec_service_tbl_get(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);
	eg_service_tbl.bf.field_update_action = entry->field_update_bitmap[0];
	eg_service_tbl.bf.field_update_action_ext = entry->field_update_bitmap[0] >>
		FLD_UPDATE_VLAN_XLT_DROP_BYPASS;
	eg_service_tbl.bf.field_update_action_ext1 = entry->field_update_bitmap[0] >>
		FLD_UP_EG_CNT_VSI_BYPASS | entry->field_update_bitmap[1] <<
		(BITS_PER_TYPE(u64) - FLD_UP_EG_CNT_VSI_BYPASS);
	eg_service_tbl.bf.tx_counting_en = (entry->bypass_bitmap[2] >> 2) & 0x1;
	return jhppe_eg_spec_service_tbl_set(dev_id, servcode_index, &eg_service_tbl);
}

sw_error_t adpt_jhppe_spec_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index,
	fal_servcode_config_t *entry)
{
	union tl_spec_service_tbl_u tl_service_tbl = {0};
	union in_spec_service_tbl_u in_service_tbl = {0};
	union l2_spec_service_tbl_u l2_service_tbl = {0};
	union eg_spec_service_tbl_u eg_service_tbl = {0};
	sw_error_t rv;

	/* eg spec service table max entry num is larger than others */
	if (servcode_index >= TL_SPEC_SERVICE_TBL_MAX_ENTRY)
		goto eg_servcode_config;

	/* tl spec service table config */
	rv = jhppe_tl_spec_service_tbl_get(dev_id, servcode_index, &tl_service_tbl);
	SW_RTN_ON_ERROR(rv);

	entry->bypass_bitmap[3] = tl_service_tbl.bf.bypass_bitmap;

	/* in spec service table config */
	rv = jhppe_in_spec_service_tbl_get(dev_id, servcode_index, &in_service_tbl);
	SW_RTN_ON_ERROR(rv);

	entry->bypass_bitmap[0] = in_service_tbl.bf.bypass_bitmap;
	entry->bypass_bitmap[2] |= in_service_tbl.bf.rx_counting_en;

	/* l2 spec service table config */
	rv = jhppe_l2_spec_service_tbl_get(dev_id, servcode_index, &l2_service_tbl);
	SW_RTN_ON_ERROR(rv);

	entry->bypass_bitmap[2] |= l2_service_tbl.bf.rx_cnt_en << 1;
	entry->bypass_bitmap[2] |= l2_service_tbl.bf.tx_cnt_en << 3;
	entry->bypass_bitmap[1] = l2_service_tbl.bf.bypass_bitmap;
	entry->bypass_bitmap[1] |= (a_uint64_t)(l2_service_tbl.bf.bypass_bitmap_ext_1 <<
		SW_FIELD_OFFSET_IN_WORD(L2_SPEC_SERVICE_TBL_BYPASS_BITMAP_EXT_OFFSET) |
		l2_service_tbl.bf.bypass_bitmap_ext_0) << SMAC_MC_DROP_BYP;
	entry->bypass_bitmap[1] |= (a_uint64_t)(l2_service_tbl.bf.bypass_bitmap_new) <<
		DOT1P_EG_PORT_OVERRIDE;
	entry->bypass_bitmap[1] |= (a_uint64_t)(l2_service_tbl.bf.post_bypass_bitmap) <<
		QM_QID_MISMATCH_BYPASS;

eg_servcode_config:
	/* eg spec service table config */
	if (servcode_index >= EG_SPEC_SERVICE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	rv = jhppe_eg_spec_service_tbl_get(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);
	entry->bypass_bitmap[2] |= eg_service_tbl.bf.tx_counting_en << 2;
	entry->field_update_bitmap[0] = eg_service_tbl.bf.field_update_action;
	entry->field_update_bitmap[0] |= (a_uint64_t)(eg_service_tbl.bf.field_update_action_ext) <<
		FLD_UPDATE_VLAN_XLT_DROP_BYPASS;
	entry->field_update_bitmap[0] |= (a_uint64_t)(eg_service_tbl.bf.field_update_action_ext1) <<
		FLD_UP_EG_CNT_VSI_BYPASS;
	entry->field_update_bitmap[1] = (a_uint64_t)(eg_service_tbl.bf.field_update_action_ext1) >>
		(BITS_PER_TYPE(u64) - FLD_UP_EG_CNT_VSI_BYPASS);

	return SW_OK;
}
