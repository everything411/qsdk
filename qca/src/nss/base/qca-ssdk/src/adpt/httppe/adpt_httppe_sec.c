/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "hsl_htt_reg.h"
#include "adpt.h"
#include "adpt_httppe_sec.h"

#ifndef IN_SEC_MINI
sw_error_t
adpt_httppe_sec_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
{
	union l3_exception_parsing_ctrl_reg_u l3_exception_parsing_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	memset(&l3_exception_parsing_ctrl, 0, sizeof(l3_exception_parsing_ctrl));

	l3_exception_parsing_ctrl.bf.small_ttl = ctrl->small_ip4ttl;
	l3_exception_parsing_ctrl.bf.small_hop_limit = ctrl->small_ip6hoplimit;

	return httppe_l3_exception_parsing_ctrl_reg_set(dev_id, &l3_exception_parsing_ctrl);
}

sw_error_t
adpt_httppe_sec_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
{
	sw_error_t rv = SW_OK;
	union l3_exception_parsing_ctrl_reg_u l3_exception_parsing_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	rv = httppe_l3_exception_parsing_ctrl_reg_get(dev_id, &l3_exception_parsing_ctrl);
	if( rv != SW_OK )
		return rv;

	ctrl->small_ip4ttl = l3_exception_parsing_ctrl.bf.small_ttl;
	ctrl->small_ip6hoplimit = l3_exception_parsing_ctrl.bf.small_hop_limit;

	return SW_OK;
}
#endif

sw_error_t
adpt_httppe_sec_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
{
	union l4_exception_parsing_ctrl_0_reg_u l4_exception_parsing_ctrl_0;
	union l4_exception_parsing_ctrl_1_reg_u l4_exception_parsing_ctrl_1;
	union l4_exception_parsing_ctrl_2_reg_u l4_exception_parsing_ctrl_2;
	union l4_exception_parsing_ctrl_3_reg_u l4_exception_parsing_ctrl_3;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	l4_exception_parsing_ctrl_0.bf.tcp_flags0 = ctrl->tcp_flags[0];
	l4_exception_parsing_ctrl_0.bf.tcp_flags0_mask = ctrl->tcp_flags_mask[0];
	l4_exception_parsing_ctrl_0.bf.tcp_flags1 = ctrl->tcp_flags[1];
	l4_exception_parsing_ctrl_0.bf.tcp_flags1_mask = ctrl->tcp_flags_mask[1];
	l4_exception_parsing_ctrl_1.bf.tcp_flags2 = ctrl->tcp_flags[2];
	l4_exception_parsing_ctrl_1.bf.tcp_flags2_mask = ctrl->tcp_flags_mask[2];
	l4_exception_parsing_ctrl_1.bf.tcp_flags3 = ctrl->tcp_flags[3];
	l4_exception_parsing_ctrl_1.bf.tcp_flags3_mask = ctrl->tcp_flags_mask[3];
	l4_exception_parsing_ctrl_2.bf.tcp_flags4 = ctrl->tcp_flags[4];
	l4_exception_parsing_ctrl_2.bf.tcp_flags4_mask = ctrl->tcp_flags_mask[4];
	l4_exception_parsing_ctrl_2.bf.tcp_flags5 = ctrl->tcp_flags[5];
	l4_exception_parsing_ctrl_2.bf.tcp_flags5_mask = ctrl->tcp_flags_mask[5];
	l4_exception_parsing_ctrl_3.bf.tcp_flags6 = ctrl->tcp_flags[6];
	l4_exception_parsing_ctrl_3.bf.tcp_flags6_mask = ctrl->tcp_flags_mask[6];
	l4_exception_parsing_ctrl_3.bf.tcp_flags7 = ctrl->tcp_flags[7];
	l4_exception_parsing_ctrl_3.bf.tcp_flags7_mask = ctrl->tcp_flags_mask[7];

	httppe_l4_exception_parsing_ctrl_0_reg_set(dev_id, &l4_exception_parsing_ctrl_0);
	httppe_l4_exception_parsing_ctrl_1_reg_set(dev_id, &l4_exception_parsing_ctrl_1);
	httppe_l4_exception_parsing_ctrl_2_reg_set(dev_id, &l4_exception_parsing_ctrl_2);
	httppe_l4_exception_parsing_ctrl_3_reg_set(dev_id, &l4_exception_parsing_ctrl_3);
	return SW_OK;
}

sw_error_t
adpt_httppe_sec_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
{
	union l4_exception_parsing_ctrl_0_reg_u l4_exception_parsing_ctrl_0;
	union l4_exception_parsing_ctrl_1_reg_u l4_exception_parsing_ctrl_1;
	union l4_exception_parsing_ctrl_2_reg_u l4_exception_parsing_ctrl_2;
	union l4_exception_parsing_ctrl_3_reg_u l4_exception_parsing_ctrl_3;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	httppe_l4_exception_parsing_ctrl_0_reg_get(dev_id, &l4_exception_parsing_ctrl_0);
	httppe_l4_exception_parsing_ctrl_1_reg_get(dev_id, &l4_exception_parsing_ctrl_1);
	httppe_l4_exception_parsing_ctrl_2_reg_get(dev_id, &l4_exception_parsing_ctrl_2);
	httppe_l4_exception_parsing_ctrl_3_reg_get(dev_id, &l4_exception_parsing_ctrl_3);

	ctrl->tcp_flags[0] = l4_exception_parsing_ctrl_0.bf.tcp_flags0;
	ctrl->tcp_flags_mask[0] = l4_exception_parsing_ctrl_0.bf.tcp_flags0_mask;
	ctrl->tcp_flags[1] = l4_exception_parsing_ctrl_0.bf.tcp_flags1;
	ctrl->tcp_flags_mask[1] = l4_exception_parsing_ctrl_0.bf.tcp_flags1_mask;
	ctrl->tcp_flags[2] = l4_exception_parsing_ctrl_1.bf.tcp_flags2;
	ctrl->tcp_flags_mask[2] = l4_exception_parsing_ctrl_1.bf.tcp_flags2_mask;
	ctrl->tcp_flags[3] = l4_exception_parsing_ctrl_1.bf.tcp_flags3;
	ctrl->tcp_flags_mask[3] = l4_exception_parsing_ctrl_1.bf.tcp_flags3_mask;
	ctrl->tcp_flags[4] = l4_exception_parsing_ctrl_2.bf.tcp_flags4;
	ctrl->tcp_flags_mask[4] = l4_exception_parsing_ctrl_2.bf.tcp_flags4_mask;
	ctrl->tcp_flags[5] = l4_exception_parsing_ctrl_2.bf.tcp_flags5;
	ctrl->tcp_flags_mask[5] = l4_exception_parsing_ctrl_2.bf.tcp_flags5_mask;
	ctrl->tcp_flags[6] = l4_exception_parsing_ctrl_3.bf.tcp_flags6;
	ctrl->tcp_flags_mask[6] = l4_exception_parsing_ctrl_3.bf.tcp_flags6_mask;
	ctrl->tcp_flags[7] = l4_exception_parsing_ctrl_3.bf.tcp_flags7;
	ctrl->tcp_flags_mask[7] = l4_exception_parsing_ctrl_3.bf.tcp_flags7_mask;

	return SW_OK;
}

sw_error_t
adpt_httppe_sec_l3_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl)
{
	union l3_exception_cmd_u l3_exception_cmd;
	union l3_exp_l2_only_ctrl_u l2_only_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);
	if (excep_type >= L3_EXCEPTION_CMD_MAX_ENTRY)
		return SW_BAD_VALUE;

	httppe_l3_exception_cmd_get(dev_id, excep_type, &l3_exception_cmd);
	httppe_l3_exp_l2_only_ctrl_get(dev_id, excep_type, &l2_only_ctrl);

	l3_exception_cmd.bf.l3_excep_cmd= ctrl->cmd;
	l2_only_ctrl.bf.excep_en = ctrl->l2fwd_only_en;

	httppe_l3_exception_cmd_set(dev_id, excep_type, &l3_exception_cmd);
	httppe_l3_exp_l2_only_ctrl_set(dev_id, excep_type, &l2_only_ctrl);

	return SW_OK;
}

sw_error_t
adpt_httppe_sec_l3_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl)
{
	union l3_exception_cmd_u l3_exception_cmd;
	union l3_exp_l2_only_ctrl_u l2_only_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);
	if (excep_type >= L3_EXCEPTION_CMD_MAX_ENTRY)
		return SW_BAD_VALUE;

	httppe_l3_exception_cmd_get(dev_id, excep_type, &l3_exception_cmd);
	httppe_l3_exp_l2_only_ctrl_get(dev_id, excep_type, &l2_only_ctrl);

	ctrl->cmd = l3_exception_cmd.bf.l3_excep_cmd;
	ctrl->l2fwd_only_en = l2_only_ctrl.bf.excep_en;

	return SW_OK;
}

/**
 * @}
 */
