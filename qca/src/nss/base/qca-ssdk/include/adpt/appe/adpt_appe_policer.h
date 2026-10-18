/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_APPE_POLICER_H_
#define _ADPT_APPE_POLICER_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#define APPE_POLICER_ID_MIN                  0
#define APPE_POLICER_ID_MAX                  511

/* chip-agnostic IR for ACL meter cfg */
typedef struct {
	/* flags */
	a_uint32_t meter_en;
	a_uint32_t color_mode;
	a_uint32_t couple_en;
	a_uint32_t meter_mode;
	a_uint32_t meter_unit;
	a_uint32_t token_unit;
	a_uint32_t length_mod;

	/* refresh/bucket (joined values) */
	a_uint32_t cir_refresh;
	a_uint32_t eir_refresh;
	a_uint32_t cbs_bucket;
	a_uint32_t ebs_bucket;

	/* MEF10.3 max refresh */
	a_uint32_t cir_max_refresh;
	a_uint32_t eir_max_refresh;

	/* group fields */
	a_uint32_t grp_end;
	a_uint32_t grp_cf;
	a_uint32_t nxt_ptr;

	/* exceed action */
	a_uint32_t exceed_chg_pri_cmd;
	a_uint32_t exceed_chg_dp_cmd;
	a_uint32_t exceed_chg_pcp_cmd;
	a_uint32_t exceed_chg_dei_cmd;
	a_uint32_t exceed_pri;
	a_uint32_t exceed_dp;
	a_uint32_t exceed_pcp;
	a_uint32_t exceed_dei;

	/* violate action */
	a_uint32_t violate_cmd; /* 0: drop, 1: forward */
	a_uint32_t violate_chg_pri_cmd;
	a_uint32_t violate_chg_dp_cmd;
	a_uint32_t violate_chg_pcp_cmd;
	a_uint32_t violate_chg_dei_cmd;
	a_uint32_t violate_pri;
	a_uint32_t violate_dp;
	a_uint32_t violate_pcp;
	a_uint32_t violate_dei;

	/* DSCP/remap */
	a_uint32_t exceed_chg_dscp_cmd;
	a_uint32_t exceed_dscp;
	a_uint32_t violate_chg_dscp_cmd;
	a_uint32_t violate_dscp;
	a_uint32_t exceed_remap_cmd;
	a_uint32_t violate_remap_cmd;
} __adpt_ppe_acl_meter_cfg_ir_t;

sw_error_t
adpt_appe_policer_ctrl_set(a_uint32_t dev_id, fal_policer_ctrl_t *ctrl);

sw_error_t
adpt_appe_policer_ctrl_get(a_uint32_t dev_id, fal_policer_ctrl_t *ctrl);

#ifndef IN_POLICER_MINI
sw_error_t
adpt_appe_policer_priority_remap_get(a_uint32_t dev_id,
	fal_policer_priority_t *priority, fal_policer_remap_t *remap);

sw_error_t
adpt_appe_policer_priority_remap_set(a_uint32_t dev_id,
	fal_policer_priority_t *priority, fal_policer_remap_t *remap);
#endif
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
