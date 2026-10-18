/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
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
#include "adpt_hppe.h"
#include "adpt_appe_servcode.h"
#if defined(MPPE)
#include "adpt_mppe_servcode.h"
#endif
#if defined(JHPPE)
#include "adpt_jhppe_servcode.h"
#endif
#if defined(HTTPPE)
#include "adpt_httppe_servcode.h"
#endif

#define MAX_PHYSICAL_PORT 8

sw_error_t adpt_hppe_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index,
					fal_servcode_config_t *entry)
{
	union in_l2_service_tbl_u in_l2_service_tbl = {0};
	union service_tbl_u service_tbl = {0};
	union eg_service_tbl_u eg_service_tbl = {0};
	a_uint32_t servcode_type = FAL_SERVCODE_TYPE(servcode_index);
	a_uint32_t ppe_type = adpt_ppe_type_get(dev_id);
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	servcode_index = FAL_SERVCODE_VALUE(servcode_index);

#if defined(JHPPE)
	if (servcode_type == FAL_SERVCODE_TYPE_PASSTHROUGH)
		return adpt_jhppe_spec_servcode_config_set(dev_id, servcode_index, entry);
#endif
	if (servcode_type != FAL_SERVCODE_TYPE_NORMAL)
		return SW_BAD_PARAM;

	/* in service table configurations */
	rv = hppe_service_tbl_get(dev_id, servcode_index, &service_tbl);
	SW_RTN_ON_ERROR(rv);
	service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[0];
	service_tbl.bf.rx_counting_en = entry->bypass_bitmap[2] & 0x1;
	rv = hppe_service_tbl_set(dev_id, servcode_index, &service_tbl);
	SW_RTN_ON_ERROR(rv);

#if defined(HTTPPE)
	if (ppe_type == HTTPPE_TYPE)
		return adpt_httppe_servcode_config_set(dev_id, servcode_index, entry);
#endif

	/* in l2 service table configurations */
	in_l2_service_tbl.bf.dst_port_id_valid = entry->dest_port_valid;
	in_l2_service_tbl.bf.dst_port_id = entry->dest_port_id;
	in_l2_service_tbl.bf.direction = entry->direction;
	in_l2_service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[1];
	in_l2_service_tbl.bf.rx_cnt_en = (entry->bypass_bitmap[2] >> 1) & 0x1;
	in_l2_service_tbl.bf.tx_cnt_en = (entry->bypass_bitmap[2] >> 3) & 0x1;
#if defined(MRPPE)
	in_l2_service_tbl.bf.bypass_bitmap_ext = entry->bypass_bitmap[1] >> SMAC_MC_DROP_BYP;
#endif
#if defined(JHPPE)
	in_l2_service_tbl.bf.bypass_bitmap_new = entry->bypass_bitmap[1] >> DOT1P_EG_PORT_OVERRIDE;
	in_l2_service_tbl.bf.post_bypass_bitmap = entry->bypass_bitmap[1] >> QM_QID_MISMATCH_BYPASS;
#endif
	rv = hppe_in_l2_service_tbl_set(dev_id, servcode_index, &in_l2_service_tbl);
	SW_RTN_ON_ERROR(rv);

	/* eg service table configurations */
#if defined(MPPE)
	/*do not touch the athtag configurations*/
	rv = hppe_eg_service_tbl_get(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);
	eg_service_tbl.bf.field_update_action &= ATHTAG_UPDATE;
	eg_service_tbl.bf.field_update_action |= entry->field_update_bitmap[0];
#if defined(JHPPE)
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
#endif
#else
	eg_service_tbl.bf.field_update_action = entry->field_update_bitmap[0];
#endif
	eg_service_tbl.bf.next_service_code = entry->next_service_code;
	eg_service_tbl.bf.hw_services = entry->hw_services;
	eg_service_tbl.bf.offset_sel = entry->offset_sel;
	eg_service_tbl.bf.tx_counting_en = (entry->bypass_bitmap[2] >> 2) & 0x1;
	rv = hppe_eg_service_tbl_set(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);

	/* tl servcie table configurations */
	return adpt_appe_servcode_tl_config_set(dev_id, servcode_index, entry);
}

sw_error_t adpt_hppe_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index,
					fal_servcode_config_t *entry)
{
	union in_l2_service_tbl_u in_l2_service_tbl;
	union service_tbl_u service_tbl;
	union eg_service_tbl_u eg_service_tbl;
	a_uint32_t servcode_type = FAL_SERVCODE_TYPE(servcode_index);
	a_uint32_t ppe_type = adpt_ppe_type_get(dev_id);
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(entry);

	servcode_index = FAL_SERVCODE_VALUE(servcode_index);

#if defined(JHPPE)
	if (servcode_type == FAL_SERVCODE_TYPE_PASSTHROUGH)
		return adpt_jhppe_spec_servcode_config_get(dev_id, servcode_index, entry);
#endif

	if (servcode_type != FAL_SERVCODE_TYPE_NORMAL)
		return SW_BAD_PARAM;

	/* in service table configurations */
	rv = hppe_service_tbl_get(dev_id, servcode_index, &service_tbl);
	SW_RTN_ON_ERROR(rv);
	entry->bypass_bitmap[0] = service_tbl.bf.bypass_bitmap;
	entry->bypass_bitmap[2] |= service_tbl.bf.rx_counting_en;

#if defined(HTTPPE)
	if (ppe_type == HTTPPE_TYPE)
		return adpt_httppe_servcode_config_get(dev_id, servcode_index, entry);
#endif

	/* in l2 service table configurations */
	rv = hppe_in_l2_service_tbl_get(dev_id, servcode_index, &in_l2_service_tbl);
	SW_RTN_ON_ERROR(rv);
	entry->dest_port_valid = in_l2_service_tbl.bf.dst_port_id_valid;
	entry->dest_port_id = in_l2_service_tbl.bf.dst_port_id;
	entry->direction = in_l2_service_tbl.bf.direction;
	entry->bypass_bitmap[1] = in_l2_service_tbl.bf.bypass_bitmap;
#if defined(MRPPE)
	entry->bypass_bitmap[1] |= (a_uint64_t)(in_l2_service_tbl.bf.bypass_bitmap_ext) <<
						SMAC_MC_DROP_BYP;
#endif
#if defined(JHPPE)
	entry->bypass_bitmap[1] |= (a_uint64_t)(in_l2_service_tbl.bf.bypass_bitmap_new) <<
						DOT1P_EG_PORT_OVERRIDE;
	entry->bypass_bitmap[1] |= (a_uint64_t)(in_l2_service_tbl.bf.post_bypass_bitmap) <<
						QM_QID_MISMATCH_BYPASS;
#endif
	entry->bypass_bitmap[2] |= in_l2_service_tbl.bf.rx_cnt_en << 1;
	entry->bypass_bitmap[2] |= in_l2_service_tbl.bf.tx_cnt_en << 3;

	/* eg service table configurations */
	rv = hppe_eg_service_tbl_get(dev_id, servcode_index, &eg_service_tbl);
	SW_RTN_ON_ERROR(rv);
	entry->field_update_bitmap[0] = eg_service_tbl.bf.field_update_action;
#if defined(JHPPE)
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
#endif
	entry->next_service_code = eg_service_tbl.bf.next_service_code;
	entry->hw_services = eg_service_tbl.bf.hw_services;
	entry->offset_sel = eg_service_tbl.bf.offset_sel;
	entry->bypass_bitmap[2] |= eg_service_tbl.bf.tx_counting_en << 2;

	/* tl service table configurations */
	return adpt_appe_servcode_tl_config_get(dev_id, servcode_index, entry);
}

sw_error_t adpt_hppe_servcode_loopcheck_en(a_uint32_t dev_id, a_bool_t enable)
{
	ADPT_DEV_ID_CHECK(dev_id);
#ifndef IN_FDB_MINI
	SW_RTN_ON_ERROR(hppe_l2_global_conf_service_code_loop_set(dev_id, enable));
#endif
	return SW_OK;
}

sw_error_t adpt_hppe_servcode_loopcheck_status_get(a_uint32_t dev_id, a_bool_t *enable)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);
#ifndef IN_FDB_MINI
	SW_RTN_ON_ERROR(hppe_l2_global_conf_service_code_loop_get(dev_id, enable));
#endif
	return SW_OK;
}

sw_error_t adpt_hppe_servcode_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

	p_adpt_api->adpt_servcode_config_set = adpt_hppe_servcode_config_set;
	p_adpt_api->adpt_servcode_config_get = adpt_hppe_servcode_config_get;
	p_adpt_api->adpt_servcode_loopcheck_en = adpt_hppe_servcode_loopcheck_en;
	p_adpt_api->adpt_servcode_loopcheck_status_get = adpt_hppe_servcode_loopcheck_status_get;
#if defined(MPPE)
	p_adpt_api->adpt_port_servcode_set = adpt_mppe_port_servcode_set;
	p_adpt_api->adpt_port_servcode_get = adpt_mppe_port_servcode_get;
	p_adpt_api->adpt_servcode_athtag_set = adpt_mppe_servcode_athtag_set;
	p_adpt_api->adpt_servcode_athtag_get = adpt_mppe_servcode_athtag_get;
#endif

	return SW_OK;
}

/**
 * @}
 */


