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
#include "adpt_appe_policer.h"
#if defined(HTTPPE)
#include "adpt_httppe_policer.h"
#endif

#define NR_ADPT_HPPE_POLICER_METER_UNIT         2
#define NR_ADPT_HPPE_POLICER_METER_TOKEN_UNIT   8
#define ADPT_HPPE_POLICER_METER_UNIT_BYTE       0
#define ADPT_HPPE_POLICER_METER_UNIT_FRAME      1
#define ADPT_HPPE_POLICER_BURST_SIZE_UNIT      65536
#define ADPT_HPPE_POLICER_REFRESH_BITS      18
#define ADPT_HPPE_POLICER_BUCKET_SIZE_BITS  16
#define ADPT_HPPE_POLICER_REFRESH_MAX  ((1 << ADPT_HPPE_POLICER_REFRESH_BITS) - 1)
#define ADPT_HPPE_POLICER_BUCKET_SIZE_MAX  ((1 << ADPT_HPPE_POLICER_BUCKET_SIZE_BITS) - 1)
#define BYTE_POLICER_MAX_RATE 10000000
#define BYTE_POLICER_MIN_RATE 64
#define FRAME_POLICER_MAX_RATE 14881000
#define FRAME_POLICER_MIN_RATE 6
#define ADPT_HPPE_ACL_POLICER_MIN_ENTRY  0
#define ADPT_APPE_POLICER_MAX           0x3ffff
#define ADPT_POLICER_TIME_SLOT_MAX	4095
/* 25G max rates for JHPPE */
#define BYTE_POLICER_MAX_RATE_25G      25000000
#define FRAME_POLICER_MAX_RATE_25G     37202500

#define ADPT_1BIT_MAGNI_SCALE   1000    /*Improve accuracy rate*/

static a_uint32_t appe_policer_type[SW_MAX_NR_DEV][APPE_POLICER_ID_MAX + 1] = {0};

static a_uint32_t hppe_policer_token_unit[NR_ADPT_HPPE_POLICER_METER_UNIT]
	[NR_ADPT_HPPE_POLICER_METER_TOKEN_UNIT] = {{2048 * 8,
	512 * 8,128 * 8,32 * 8,8 * 8,2 * 8, 4, 1},
	{2097152,524288,131072,32768,8192,2048,512,128}};

typedef struct
{
    a_uint64_t rate_1bit;
    a_uint64_t rate_max;
} adpt_ppe_policer_rate_t;

typedef struct
{
    a_uint64_t burst_size_1bit;
    a_uint64_t burst_size_max;
} adpt_ppe_policer_burst_size_t;

typedef struct
{
    adpt_ppe_policer_rate_t ppe_policer_rate[NR_ADPT_HPPE_POLICER_METER_UNIT]
		[NR_ADPT_HPPE_POLICER_METER_TOKEN_UNIT];
    adpt_ppe_policer_burst_size_t ppe_policer_burst[NR_ADPT_HPPE_POLICER_METER_UNIT]
		[NR_ADPT_HPPE_POLICER_METER_TOKEN_UNIT];
} adpt_ppe_policer_tbl_t;

static adpt_ppe_policer_tbl_t *g_ppe_policer_tbl[SW_MAX_NR_DEV] = { NULL };

/* acl meter ops */
typedef sw_error_t (*adpt_ppe_acl_read_ir_fn)(a_uint32_t dev_id, a_uint32_t index,
				__adpt_ppe_acl_meter_cfg_ir_t *ir);
typedef sw_error_t (*adpt_ppe_acl_write_ir_fn)(a_uint32_t dev_id, a_uint32_t index,
				const __adpt_ppe_acl_meter_cfg_ir_t *ir);
struct adpt_ppe_acl_meter_ops {
	adpt_ppe_acl_read_ir_fn  read_ir;
	adpt_ppe_acl_write_ir_fn write_ir;
};
static const struct adpt_ppe_acl_meter_ops *g_ppe_acl_meter_ops[SW_MAX_NR_DEV] = { NULL };

static inline a_uint32_t
__adpt_ppe_policer_byte_max_rate_get(a_uint32_t dev_id)
{
	if (adpt_chip_type_get(dev_id) == CHIP_JHPPE)
		return BYTE_POLICER_MAX_RATE_25G;
	return BYTE_POLICER_MAX_RATE;
}

static inline a_uint32_t
__adpt_ppe_policer_frame_max_rate_get(a_uint32_t dev_id)
{
	if (adpt_chip_type_get(dev_id) == CHIP_JHPPE)
		return FRAME_POLICER_MAX_RATE_25G;
	return FRAME_POLICER_MAX_RATE;
}

static inline a_uint32_t __adpt_ppe_acl_policer_max_num_get(a_uint32_t dev_id)
{
	a_uint32_t num = 0;
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);

	switch (ppe_type) {
	case MPPE_TYPE:
	case HMSPPE_TYPE:
	case HTTPPE_TYPE:
		num = 127;
		break;
	case APPE_TYPE:
	case MRPPE_TYPE:
	case JHPPE_TYPE:
		num = 511;
		break;
	default:
		break;
	}

	return num;
}

static sw_error_t
__adpt_hppe_policer_max_burst_size(a_uint32_t dev_id)
{
	a_uint32_t i = 0, j = 0;
	a_uint64_t temp = 0, temp1 = 0;
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	for (j = 0; j < 8; j++) {
		/*max size unit is 1/1000 byte based*/
		temp = ((a_uint64_t)(ADPT_HPPE_POLICER_BURST_SIZE_UNIT)) *
			ADPT_HPPE_POLICER_BUCKET_SIZE_MAX;
		do_div(temp, hppe_policer_token_unit[i][j]);
		tbl->ppe_policer_burst[i][j].burst_size_max =
						(a_uint64_t)(temp * 1000);
		temp1 = tbl->ppe_policer_burst[i][j].burst_size_max;
		do_div(temp1, ADPT_HPPE_POLICER_BUCKET_SIZE_MAX);
		tbl->ppe_policer_burst[i][j].burst_size_1bit = temp1;
	}

	i = i + 1;
	for (j = 0; j < 8; j++) {
		/* max size unit is 1/1000 frame based */
		temp = ((a_uint64_t)(ADPT_HPPE_POLICER_BURST_SIZE_UNIT)) *
			ADPT_HPPE_POLICER_BUCKET_SIZE_MAX;
		do_div(temp, hppe_policer_token_unit[i][j]);
		tbl->ppe_policer_burst[i][j].burst_size_max =
						(a_uint64_t)(temp * 1000);

		temp1 = tbl->ppe_policer_burst[i][j].burst_size_max;
		do_div(temp1, ADPT_HPPE_POLICER_BUCKET_SIZE_MAX);
		tbl->ppe_policer_burst[i][j].burst_size_1bit = temp1;
	}

	return SW_OK;
}

/* hppe: read table to IR (no unit conversion) */
static sw_error_t
__adpt_hppe_acl_meter_cfg_read_ir(a_uint32_t dev_id, a_uint32_t index,
				  __adpt_ppe_acl_meter_cfg_ir_t *ir)
{
	union in_acl_meter_cfg_tbl_u reg;
	sw_error_t rv = SW_OK;

	memset(&reg, 0, sizeof(reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ir);

	if ((index < ADPT_HPPE_ACL_POLICER_MIN_ENTRY) ||
	    (index > __adpt_ppe_acl_policer_max_num_get(dev_id)))
		return SW_BAD_PARAM;

	rv = hppe_in_acl_meter_cfg_tbl_get(dev_id, index, &reg);
	SW_RTN_ON_ERROR(rv);

	ir->cir_refresh = ((a_uint32_t)reg.bf.cir_1 << \
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_OFFSET)) | \
			reg.bf.cir_0;
	ir->eir_refresh = ((a_uint32_t)reg.bf.eir_1 << \
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_EIR_OFFSET)) | \
			reg.bf.eir_0;
	ir->cbs_bucket  = reg.bf.cbs;
	ir->ebs_bucket  = reg.bf.ebs;

	ir->cir_max_refresh = ((a_uint32_t)reg.bf.cir_max_1 << \
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_MAX_OFFSET)) | \
			reg.bf.cir_max_0;
	ir->eir_max_refresh = reg.bf.eir_max;

	ir->meter_en   = reg.bf.meter_en;
	ir->color_mode = reg.bf.color_mode;
	ir->couple_en  = reg.bf.coupling_flag;
	ir->meter_mode = reg.bf.meter_mode;
	ir->meter_unit = reg.bf.meter_unit;
	ir->token_unit = reg.bf.token_unit;

	ir->grp_end = reg.bf.grp_end;
	ir->grp_cf  = reg.bf.grp_cf;
	ir->nxt_ptr = ((a_uint32_t)reg.bf.nxt_ptr_1 << \
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_NXT_PTR_OFFSET)) | \
			reg.bf.nxt_ptr_0;

	ir->exceed_chg_pri_cmd = reg.bf.exceed_chg_pri_cmd;
	ir->exceed_chg_dp_cmd  = reg.bf.exceed_chg_dp_cmd;
	ir->exceed_chg_pcp_cmd = reg.bf.exceed_chg_pcp_cmd;
	ir->exceed_chg_dei_cmd = reg.bf.exceed_chg_dei_cmd;
	ir->exceed_pri         = reg.bf.exceed_pri;
	ir->exceed_dp          = reg.bf.exceed_dp;
	ir->exceed_pcp         = reg.bf.exceed_pcp;
	ir->exceed_dei         = reg.bf.exceed_dei;

	ir->violate_cmd         = reg.bf.violate_cmd;
	ir->violate_chg_pri_cmd = reg.bf.violate_chg_pri_cmd;
	ir->violate_chg_dp_cmd  = reg.bf.violate_chg_dp_cmd;
	ir->violate_chg_pcp_cmd = reg.bf.violate_chg_pcp_cmd;
	ir->violate_chg_dei_cmd = reg.bf.violate_chg_dei_cmd;
	ir->violate_pri         = ((a_uint32_t)reg.bf.violate_pri_1 << \
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_VIOLATE_PRI_OFFSET)) | \
			reg.bf.violate_pri_0;
	ir->violate_dp          = reg.bf.violate_dp;
	ir->violate_pcp         = reg.bf.violate_pcp;
	ir->violate_dei         = reg.bf.violate_dei;

	ir->exceed_chg_dscp_cmd = reg.bf.exceed_chg_dscp_cmd;
	ir->exceed_dscp         = reg.bf.exceed_dscp;
	ir->violate_chg_dscp_cmd= reg.bf.violate_chg_dscp_cmd;
	ir->violate_dscp        = reg.bf.violate_dscp;
	ir->exceed_remap_cmd    = reg.bf.exceed_remap_cmd;
	ir->violate_remap_cmd   = reg.bf.violate_remap_cmd;

#if defined(JHPPE)
	ir->length_mod = reg.bf.length_mod;
#endif
	return SW_OK;
}

/* hppe: write from IR to table */
static sw_error_t
__adpt_hppe_acl_meter_cfg_write_ir(a_uint32_t dev_id, a_uint32_t index,
				   const __adpt_ppe_acl_meter_cfg_ir_t *ir)
{
	union in_acl_meter_cfg_tbl_u reg;
	sw_error_t rv = SW_OK;

	memset(&reg, 0, sizeof(reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ir);

	if ((index < ADPT_HPPE_ACL_POLICER_MIN_ENTRY) ||
	    (index > __adpt_ppe_acl_policer_max_num_get(dev_id)))
		return SW_BAD_PARAM;

	rv = hppe_in_acl_meter_cfg_tbl_get(dev_id, index, &reg);
	SW_RTN_ON_ERROR(rv);

	reg.bf.meter_en      = ir->meter_en;
	reg.bf.color_mode    = ir->color_mode;
	reg.bf.coupling_flag = ir->couple_en;
	reg.bf.meter_mode    = ir->meter_mode;
	reg.bf.token_unit    = ir->token_unit;
	reg.bf.meter_unit    = ir->meter_unit;

	reg.bf.cbs = ir->cbs_bucket;
	reg.bf.ebs = ir->ebs_bucket;

	reg.bf.cir_0 = ir->cir_refresh &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_OFFSET)) - 1U);
	reg.bf.cir_1 = ir->cir_refresh >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_OFFSET);

	reg.bf.eir_0 = ir->eir_refresh &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_EIR_OFFSET)) - 1U);
	reg.bf.eir_1 = ir->eir_refresh >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_EIR_OFFSET);

	reg.bf.exceed_chg_pri_cmd = ir->exceed_chg_pri_cmd;
	reg.bf.exceed_chg_dp_cmd  = ir->exceed_chg_dp_cmd;
	reg.bf.exceed_chg_pcp_cmd = ir->exceed_chg_pcp_cmd;
	reg.bf.exceed_chg_dei_cmd = ir->exceed_chg_dei_cmd;
	reg.bf.exceed_pri         = ir->exceed_pri;
	reg.bf.exceed_dp          = ir->exceed_dp;
	reg.bf.exceed_pcp         = ir->exceed_pcp;
	reg.bf.exceed_dei         = ir->exceed_dei;

	reg.bf.violate_cmd         = ir->violate_cmd;
	reg.bf.violate_chg_pri_cmd = ir->violate_chg_pri_cmd;
	reg.bf.violate_chg_dp_cmd  = ir->violate_chg_dp_cmd;
	reg.bf.violate_chg_pcp_cmd = ir->violate_chg_pcp_cmd;
	reg.bf.violate_chg_dei_cmd = ir->violate_chg_dei_cmd;
	reg.bf.violate_pri_0 = ir->violate_pri &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_VIOLATE_PRI_OFFSET)) - 1U);
	reg.bf.violate_pri_1 = ir->violate_pri >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_VIOLATE_PRI_OFFSET);

	reg.bf.violate_dp          = ir->violate_dp;
	reg.bf.violate_pcp         = ir->violate_pcp;
	reg.bf.violate_dei         = ir->violate_dei;

	reg.bf.exceed_chg_dscp_cmd = ir->exceed_chg_dscp_cmd;
	reg.bf.exceed_dscp         = ir->exceed_dscp;
	reg.bf.violate_chg_dscp_cmd= ir->violate_chg_dscp_cmd;
	reg.bf.violate_dscp        = ir->violate_dscp;
	reg.bf.exceed_remap_cmd    = ir->exceed_remap_cmd;
	reg.bf.violate_remap_cmd   = ir->violate_remap_cmd;

	reg.bf.cir_max_0 = ir->cir_max_refresh &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_MAX_OFFSET)) - 1U);
	reg.bf.cir_max_1 = ir->cir_max_refresh >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_MAX_OFFSET);

	reg.bf.eir_max   = ir->eir_max_refresh;

	reg.bf.grp_end   = ir->grp_end;
	reg.bf.grp_cf    = ir->grp_cf;
	reg.bf.nxt_ptr_0 = ir->nxt_ptr &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_NXT_PTR_OFFSET)) - 1U);
	reg.bf.nxt_ptr_1 = ir->nxt_ptr >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_NXT_PTR_OFFSET);

#if defined(JHPPE)
	reg.bf.length_mod = ir->length_mod;
#endif
	return hppe_in_acl_meter_cfg_tbl_set(dev_id, index, &reg);
}

static sw_error_t adpt_ppe_policer_table_init(a_uint32_t dev_id)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (g_ppe_policer_tbl[dev_id] != NULL)
		return SW_OK;

	g_ppe_policer_tbl[dev_id] =
		(adpt_ppe_policer_tbl_t *)aos_mem_alloc(
			sizeof(*g_ppe_policer_tbl[dev_id]));
	if (!g_ppe_policer_tbl[dev_id])
		return SW_NO_RESOURCE;

	memset(g_ppe_policer_tbl[dev_id], 0, sizeof(*g_ppe_policer_tbl[dev_id]));

	/* burst table init once per device */
	return __adpt_hppe_policer_max_burst_size(dev_id);
}

sw_error_t
adpt_hppe_acl_policer_counter_get(a_uint32_t dev_id, a_uint32_t index,
		fal_policer_counter_t *counter)
{
	union in_acl_meter_cnt_tbl_u in_acl_meter_cnt_tbl;

	memset(&in_acl_meter_cnt_tbl, 0, sizeof(in_acl_meter_cnt_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(counter);

	if ((index < ADPT_HPPE_ACL_POLICER_MIN_ENTRY) ||
		(index > __adpt_ppe_acl_policer_max_num_get(dev_id)))
		return SW_BAD_PARAM;

	hppe_in_acl_meter_cnt_tbl_get(dev_id, index * 3, &in_acl_meter_cnt_tbl);
	counter->green_packet_counter = in_acl_meter_cnt_tbl.bf.pkt_cnt;
	counter->green_byte_counter = in_acl_meter_cnt_tbl.bf.byte_cnt_1;
	counter->green_byte_counter = (counter->green_byte_counter << 32) | in_acl_meter_cnt_tbl.bf.byte_cnt_0;

	hppe_in_acl_meter_cnt_tbl_get(dev_id, index * 3 + 1, &in_acl_meter_cnt_tbl);
	counter->yellow_packet_counter = in_acl_meter_cnt_tbl.bf.pkt_cnt;
	counter->yellow_byte_counter = in_acl_meter_cnt_tbl.bf.byte_cnt_1;
	counter->yellow_byte_counter = (counter->yellow_byte_counter << 32) | in_acl_meter_cnt_tbl.bf.byte_cnt_0;

	hppe_in_acl_meter_cnt_tbl_get(dev_id, index * 3 + 2, &in_acl_meter_cnt_tbl);
	counter->red_packet_counter = in_acl_meter_cnt_tbl.bf.pkt_cnt;
	counter->red_byte_counter = in_acl_meter_cnt_tbl.bf.byte_cnt_1;
	counter->red_byte_counter = (counter->red_byte_counter << 32) | in_acl_meter_cnt_tbl.bf.byte_cnt_0;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_policer_counter_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_policer_counter_t *counter)
{
	union in_port_meter_cnt_tbl_u in_port_meter_cnt_tbl;

	memset(&in_port_meter_cnt_tbl, 0, sizeof(in_port_meter_cnt_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(counter);

	if (port_id > adpt_ppe_port_id_max_get(dev_id))
		return SW_BAD_PARAM;

	hppe_in_port_meter_cnt_tbl_get(dev_id, port_id * 3, &in_port_meter_cnt_tbl);
	counter->green_packet_counter = in_port_meter_cnt_tbl.bf.pkt_cnt;
	counter->green_byte_counter = in_port_meter_cnt_tbl.bf.byte_cnt_1;
	counter->green_byte_counter = (counter->green_byte_counter << 32) | in_port_meter_cnt_tbl.bf.byte_cnt_0;

	hppe_in_port_meter_cnt_tbl_get(dev_id, port_id * 3 + 1, &in_port_meter_cnt_tbl);
	counter->yellow_packet_counter = in_port_meter_cnt_tbl.bf.pkt_cnt;
	counter->yellow_byte_counter = in_port_meter_cnt_tbl.bf.byte_cnt_1;
	counter->yellow_byte_counter = (counter->yellow_byte_counter << 32) | in_port_meter_cnt_tbl.bf.byte_cnt_0;

	hppe_in_port_meter_cnt_tbl_get(dev_id, port_id * 3 + 2, &in_port_meter_cnt_tbl);
	counter->red_packet_counter = in_port_meter_cnt_tbl.bf.pkt_cnt;
	counter->red_byte_counter = in_port_meter_cnt_tbl.bf.byte_cnt_1;
	counter->red_byte_counter = (counter->red_byte_counter << 32) | in_port_meter_cnt_tbl.bf.byte_cnt_0;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_compensation_byte_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t  *length)
{
	sw_error_t rv = SW_OK;
	union meter_cmpst_length_reg_u meter_cmpst_length_reg;

	memset(&meter_cmpst_length_reg, 0, sizeof(meter_cmpst_length_reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(length);

	if (port_id > adpt_ppe_port_id_max_get(dev_id))
		return SW_BAD_PARAM;

	rv = hppe_meter_cmpst_length_reg_get(dev_id, port_id, &meter_cmpst_length_reg);

	if( rv != SW_OK )
		return rv;

	*length = meter_cmpst_length_reg.bf.cmpst_length;

	return SW_OK;
}

static sw_error_t
__adpt_hppe_policer_rate_to_refresh(a_uint32_t dev_id, a_uint32_t rate,
							a_uint32_t *refresh,
							a_bool_t meter_unit,
							a_uint32_t  token_unit)
{
	a_uint32_t temp_refresh;
	a_uint64_t temp_rate;
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	if (tbl->ppe_policer_rate[meter_unit][token_unit].rate_1bit > 0)
	{
		temp_rate = ((a_uint64_t)rate)*1000*ADPT_1BIT_MAGNI_SCALE;
		do_div(temp_rate,
			tbl->ppe_policer_rate[meter_unit][token_unit].rate_1bit);
		temp_refresh = (a_uint32_t)temp_rate;
	}
	else
	{
		return SW_BAD_PARAM;;
	}

	if (temp_refresh > ADPT_HPPE_POLICER_REFRESH_MAX)
	{
		temp_refresh = ADPT_HPPE_POLICER_REFRESH_MAX;
	}

	*refresh = temp_refresh;

	return SW_OK;
}

static sw_error_t
__adpt_hppe_policer_burst_size_to_bucket_size(a_uint32_t dev_id,
						a_uint32_t burst_size,
						a_uint32_t *bucket_size,
						a_bool_t meter_unit,
						a_uint32_t  token_unit)
{
	a_uint32_t temp_bucket_size;
	a_uint64_t temp_burst_size;
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	if (tbl->ppe_policer_burst[meter_unit][token_unit].burst_size_1bit > 0)
	{
		temp_burst_size = ((a_uint64_t)burst_size) * 1000;
		do_div(temp_burst_size,
			tbl->ppe_policer_burst[meter_unit][token_unit].burst_size_1bit);
		temp_bucket_size = (a_uint32_t)temp_burst_size;
	}
	else
	{
		return SW_BAD_PARAM;
	}

	if(temp_bucket_size > ADPT_HPPE_POLICER_BUCKET_SIZE_MAX)
	{
		temp_bucket_size = ADPT_HPPE_POLICER_BUCKET_SIZE_MAX;
	}
	if(temp_bucket_size == 0)
	{
		temp_bucket_size = 1;
	}

	*bucket_size = temp_bucket_size;

	return SW_OK;
}

static sw_error_t
__adpt_hppe_policer_refresh_to_rate(a_uint32_t dev_id, a_uint32_t refresh,
							a_uint32_t *rate,
							a_bool_t meter_unit,
							a_uint32_t  token_unit)
{
	a_uint32_t temp_rate;
	a_uint64_t temp_refresh;
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	if (tbl->ppe_policer_rate[meter_unit][token_unit].rate_1bit > 0)
	{
		temp_refresh = ((a_uint64_t)refresh) *
			tbl->ppe_policer_rate[meter_unit][token_unit].rate_1bit;
		do_div(temp_refresh, 1000*ADPT_1BIT_MAGNI_SCALE);
		temp_rate = (a_uint32_t)temp_refresh;
	}
	else
	{
		return SW_BAD_PARAM;;
	}

	*rate = temp_rate;

	return SW_OK;
}

static sw_error_t
__adpt_hppe_policer_bucket_size_to_burst_size(a_uint32_t dev_id,
						a_uint32_t bucket_size,
						a_uint32_t *burst_size,
						a_bool_t meter_unit,
						a_uint32_t  token_unit)
{
	a_uint32_t temp_burst_size;
	a_uint64_t temp_bucket_size;
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	if (tbl->ppe_policer_burst[meter_unit][token_unit].burst_size_1bit > 0)
	{
		temp_bucket_size = ((a_uint64_t)bucket_size) *
			tbl->ppe_policer_burst[meter_unit][token_unit].burst_size_1bit;
		do_div(temp_bucket_size, 1000);
		temp_burst_size = (a_uint32_t)temp_bucket_size;
	}
	else
	{
		return SW_BAD_PARAM;
	}

	*burst_size = temp_burst_size;

	return SW_OK;
}

static sw_error_t
__adpt_hppe_policer_max_rate(a_uint32_t dev_id, a_uint32_t time_slot)
{
	a_uint32_t i = 0, j = 0;
	a_uint32_t time_cycle = 0;
	a_uint64_t temp = 0, temp1 = 0, temp2 = 0;
	a_uint32_t ppe_freq = adpt_chip_freq_get(dev_id);
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	/* time_cycle is ns*/
	time_cycle = ( time_slot * 8)/ ppe_freq;

	for (j = 0; j < 8; j++)
	{
		/*max rate is bps*/
		temp1 = (a_uint64_t)(ADPT_HPPE_POLICER_REFRESH_MAX  * 1000 * 8) * 1000;
		temp2 =  (a_uint64_t)hppe_policer_token_unit[i][j] * time_cycle;

		do_div(temp1, temp2);
		tbl->ppe_policer_rate[i][j].rate_max = temp1;

		temp = temp1*ADPT_1BIT_MAGNI_SCALE;
		do_div(temp, ADPT_HPPE_POLICER_REFRESH_MAX);
		/*new temp is the original rate_1bit multiplied by a MAGNI_SCALE of 1000*/
		tbl->ppe_policer_rate[i][j].rate_1bit = temp;

	//	printk("hppe policer byte max_rate generating =%llu\n", hppe_policer_rate[i][j].rate_max);
	//	printk("hppe policer byte based step =%llu\n", hppe_policer_rate[i][j].rate_1bit);
	}

	i = i + 1;
	for (j = 0; j < 8; j++)
	{
		/* max rate unit  is 1/1000 pps*/
		temp1 = (a_uint64_t)(ADPT_HPPE_POLICER_REFRESH_MAX * 1000) * 1000 * 1000;
		temp2 = (a_uint64_t)hppe_policer_token_unit[i][j] * time_cycle;

		do_div(temp1, temp2);
		tbl->ppe_policer_rate[i][j].rate_max = temp1;

		temp = temp1*ADPT_1BIT_MAGNI_SCALE;
		do_div(temp, ADPT_HPPE_POLICER_REFRESH_MAX);
		/*new temp is the original rate_1bit multiplied by a MAGNI_SCALE of 1000*/
		tbl->ppe_policer_rate[i][j].rate_1bit = temp;

	//	printk("policer frame hppe_max_rate generating =%llu\n", hppe_policer_rate[i][j].rate_max);
	//	printk("policer frame step rate =%llu\n", hppe_policer_rate[i][j].rate_1bit);
	}

	return SW_OK;
}

static sw_error_t
__adpt_hppe_policer_two_bucket_parameter_select(a_uint32_t dev_id,
						a_uint64_t c_rate,
						a_uint64_t c_burst_size,
						a_uint64_t e_rate,
						a_uint64_t e_burst_size,
						a_uint32_t meter_unit,
						a_uint32_t *token_unit)
{
	a_uint32_t  temp_token_unit;
	a_uint32_t match = A_FALSE;
	a_uint64_t temp_rate = 0, temp_burst_size = 0;
	adpt_ppe_policer_tbl_t *tbl = g_ppe_policer_tbl[dev_id];

	if (!tbl)
		return SW_FAIL;

	if(c_rate > e_rate)
		temp_rate = c_rate;
	else
		temp_rate = e_rate;

	if(c_burst_size > e_burst_size)
		temp_burst_size = c_burst_size;
	else
		temp_burst_size = e_burst_size;

	for (temp_token_unit = 0; temp_token_unit < 8; temp_token_unit++)
	{
		 if (temp_rate > tbl->ppe_policer_rate[meter_unit][temp_token_unit].rate_max)
		 {
		 	continue;
		 }
		 else if(temp_burst_size <= tbl->ppe_policer_burst[meter_unit][temp_token_unit].burst_size_max)
		{
			*token_unit = temp_token_unit;
			match = A_TRUE;
			break;
		}
	}

	if (match == A_FALSE)
	{
		printk("Not match C and E token bucket parameter rate configuration \n");
		return SW_BAD_PARAM;
	}

	return SW_OK;
}

#ifndef IN_POLICER_MINI
#if 0
static sw_error_t
__adpt_hppe_policer_one_bucket_parameter_select(a_uint64_t c_rate,
						a_uint64_t c_burst_size,
						a_uint32_t meter_unit,
						a_uint32_t *token_unit)
{
	a_uint32_t  temp_token_unit;
	a_uint32_t match = A_FALSE;

	for (temp_token_unit = 0; temp_token_unit < 8; temp_token_unit++)
	{
		if (c_rate > g_ppe_policer_tbl[dev_id]->ppe_policer_rate[meter_unit][temp_token_unit].rate_max)
		{
			continue;
		}
		else if (c_burst_size <= g_ppe_policer_tbl[dev_id]->ppe_policer_burst[meter_unit][temp_token_unit].burst_size_max)
		{
			*token_unit = temp_token_unit;
			match = A_TRUE;
			break;
		}
	}

	if (match == A_FALSE)
	{
		printk("Not match policer C token bucket parameter rate configuration \n");
		return SW_BAD_PARAM;
	}

	return SW_OK;
}
#endif
#endif

sw_error_t
adpt_hppe_acl_policer_entry_get(a_uint32_t dev_id, a_uint32_t index,
		fal_policer_config_t *policer, fal_policer_action_t *action)
{
	__adpt_ppe_acl_meter_cfg_ir_t ir;
	sw_error_t rv = SW_OK;

	memset(&ir, 0, sizeof(ir));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(policer);
	ADPT_NULL_POINT_CHECK(action);

	if ((index < ADPT_HPPE_ACL_POLICER_MIN_ENTRY) ||
		(index > __adpt_ppe_acl_policer_max_num_get(dev_id)))
		return SW_BAD_PARAM;

	if (!g_ppe_acl_meter_ops[dev_id])
	    return SW_FAIL;
	if (!g_ppe_acl_meter_ops[dev_id]->read_ir)
	    return SW_FAIL;

	rv = g_ppe_acl_meter_ops[dev_id]->read_ir(dev_id, index, &ir);
	SW_RTN_ON_ERROR(rv);

	rv = __adpt_hppe_policer_refresh_to_rate(dev_id, ir.cir_refresh,
				&policer->cir, ir.meter_unit, ir.token_unit);
	SW_RTN_ON_ERROR(rv);
	rv = __adpt_hppe_policer_refresh_to_rate(dev_id, ir.eir_refresh,
				&policer->eir, ir.meter_unit, ir.token_unit);
	SW_RTN_ON_ERROR(rv);
	rv = __adpt_hppe_policer_refresh_to_rate(dev_id, ir.cir_max_refresh,
				&policer->cir_max, ir.meter_unit, ir.token_unit);
	SW_RTN_ON_ERROR(rv);
	rv = __adpt_hppe_policer_refresh_to_rate(dev_id, ir.eir_max_refresh,
				&policer->eir_max, ir.meter_unit, ir.token_unit);
	SW_RTN_ON_ERROR(rv);

	rv = __adpt_hppe_policer_bucket_size_to_burst_size(dev_id, ir.cbs_bucket,
				&policer->cbs, ir.meter_unit, ir.token_unit);
	SW_RTN_ON_ERROR(rv);
	rv = __adpt_hppe_policer_bucket_size_to_burst_size(dev_id, ir.ebs_bucket,
				&policer->ebs, ir.meter_unit, ir.token_unit);
	SW_RTN_ON_ERROR(rv);

	policer->meter_en = ir.meter_en;
	policer->color_mode = ir.color_mode;
	policer->couple_en= ir.couple_en;
	policer->meter_mode = ir.meter_mode;
	policer->meter_unit = ir.meter_unit;
	action->yellow_priority_en = ir.exceed_chg_pri_cmd;
	action->yellow_drop_priority_en = ir.exceed_chg_dp_cmd;
	action->yellow_pcp_en = ir.exceed_chg_pcp_cmd;
	action->yellow_dei_en = ir.exceed_chg_dei_cmd;
	action->yellow_priority = ir.exceed_pri;
	action->yellow_drop_priority = ir.exceed_dp;
	action->yellow_pcp = ir.exceed_pcp;
	action->yellow_dei = ir.exceed_dei;
	if (ir.violate_cmd == 0)
		action->red_action = FAL_MAC_DROP;
	else
		action->red_action = FAL_MAC_FRWRD;
	action->red_priority_en = ir.violate_chg_pri_cmd;
	action->red_drop_priority_en = ir.violate_chg_dp_cmd;
	action->red_pcp_en = ir.violate_chg_pcp_cmd;
	action->red_dei_en = ir.violate_chg_dei_cmd;
	action->red_priority = ir.violate_pri;
	action->red_drop_priority = ir.violate_dp;
	action->red_pcp = ir.violate_pcp;
	action->red_dei = ir.violate_dei;
	policer->meter_type = appe_policer_type[dev_id][index];
	policer->grp_end = ir.grp_end;
	policer->grp_couple_en = ir.grp_cf;
	policer->next_ptr = ir.nxt_ptr;
	policer->length_mode = ir.length_mod;
	action->yellow_dscp_en = ir.exceed_chg_dscp_cmd;
	action->yellow_dscp = ir.exceed_dscp;
	action->red_dscp_en = ir.violate_chg_dscp_cmd;
	action->red_dscp = ir.violate_dscp;
	action->yellow_remap_en = ir.exceed_remap_cmd;
	action->red_remap_en = ir.violate_remap_cmd;

	return SW_OK;
}

sw_error_t
adpt_hppe_acl_policer_entry_set(a_uint32_t dev_id, a_uint32_t index,
		fal_policer_config_t *policer, fal_policer_action_t *action)
{
	a_uint64_t temp_cir = 0, temp_eir =0, temp_cbs =0, temp_ebs = 0;
	a_uint32_t token_unit = 0;
	sw_error_t rv = SW_OK;
	a_uint64_t temp_cir_max = 0, temp_eir_max =0;
	a_uint32_t cir_ref = 0, eir_ref = 0, cbs_bkt = 0, ebs_bkt = 0;
	a_uint32_t appe_cir_max_ref = 0, appe_eir_max_ref= 0;
	__adpt_ppe_acl_meter_cfg_ir_t ir;
	a_uint32_t byte_max_rate  = 0;
	a_uint32_t frame_max_rate = 0;

	memset(&ir, 0, sizeof(ir));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(policer);
	ADPT_NULL_POINT_CHECK(action);

	if (index < (ADPT_HPPE_ACL_POLICER_MIN_ENTRY) ||
		(index > __adpt_ppe_acl_policer_max_num_get(dev_id)))
		return SW_BAD_PARAM;

	byte_max_rate  = __adpt_ppe_policer_byte_max_rate_get(dev_id);
	frame_max_rate = __adpt_ppe_policer_frame_max_rate_get(dev_id);

	if(ADPT_HPPE_POLICER_METER_UNIT_BYTE == policer->meter_unit) {
		if ((policer->cir > byte_max_rate) ||
			(policer->eir > byte_max_rate))
			return SW_BAD_PARAM;
		if ((policer->cir < BYTE_POLICER_MIN_RATE) &&
			(policer->cir != 0))
			return SW_BAD_PARAM;
		if ((policer->eir < BYTE_POLICER_MIN_RATE) &&
			(policer->eir != 0))
			return SW_BAD_PARAM;
		if ((policer->cir_max> byte_max_rate) ||
			(policer->eir_max > byte_max_rate))
			return SW_BAD_PARAM;
		if ((policer->cir_max < BYTE_POLICER_MIN_RATE) &&
			(policer->cir_max != 0))
			return SW_BAD_PARAM;
		if ((policer->eir_max < BYTE_POLICER_MIN_RATE) &&
			(policer->eir_max != 0))
			return SW_BAD_PARAM;
	}

	if(ADPT_HPPE_POLICER_METER_UNIT_FRAME == policer->meter_unit)
	{
		if ((policer->cir > frame_max_rate) ||
			(policer->eir > frame_max_rate))
			return SW_BAD_PARAM;
		if ((policer->cir < FRAME_POLICER_MIN_RATE) &&
			(policer->cir != 0))
			return SW_BAD_PARAM;
		if ((policer->eir < FRAME_POLICER_MIN_RATE) &&
			(policer->eir != 0))
			return SW_BAD_PARAM;
		if ((policer->cir_max > frame_max_rate) ||
			(policer->eir_max > frame_max_rate))
			return SW_BAD_PARAM;
		if ((policer->cir_max < FRAME_POLICER_MIN_RATE) &&
			(policer->cir_max != 0))
			return SW_BAD_PARAM;
		if ((policer->eir_max < FRAME_POLICER_MIN_RATE) &&
			(policer->eir_max != 0))
			return SW_BAD_PARAM;
	}

	if((0 == policer->meter_mode) && (policer->cir > policer->eir))
		return SW_BAD_PARAM;

	temp_cir = ((a_uint64_t)policer->cir) * 1000;
	temp_cbs = ((a_uint64_t)policer->cbs) * 1000;
	temp_eir = ((a_uint64_t)policer->eir) * 1000;
	temp_ebs = ((a_uint64_t)policer->ebs) * 1000;

	if (policer->meter_type == FAL_POLICER_METER_MEF10_3) {
		temp_cir_max = ((a_uint64_t)policer->cir_max) * 1000;
		temp_eir_max = ((a_uint64_t)policer->eir_max) * 1000;

		if (temp_cir_max >= temp_cir) {
			temp_cir = temp_cir_max;
		}
		if (temp_eir_max >= temp_eir) {
			temp_eir = temp_eir_max;
		}
	}

	rv = __adpt_hppe_policer_two_bucket_parameter_select(dev_id, temp_cir,
				temp_cbs,
				temp_eir,
				temp_ebs,
				policer->meter_unit,
				&token_unit);
	if( rv != SW_OK )
		return rv;

	SSDK_DEBUG("current meter unit is = %d\n", policer->meter_unit);
	SSDK_DEBUG("current token unit is = %d\n", token_unit);

	rv = __adpt_hppe_policer_rate_to_refresh(dev_id, policer->cir,
				&cir_ref,
				policer->meter_unit,
				token_unit);
	SW_RTN_ON_ERROR(rv);

	rv = __adpt_hppe_policer_rate_to_refresh(dev_id, policer->eir,
				&eir_ref,
				policer->meter_unit,
				token_unit);
	SW_RTN_ON_ERROR(rv);

	if (policer->meter_type == FAL_POLICER_METER_MEF10_3) {
		rv = __adpt_hppe_policer_rate_to_refresh(dev_id, policer->cir_max,
				&appe_cir_max_ref,
				policer->meter_unit,
				token_unit);
		SW_RTN_ON_ERROR(rv);

		rv = __adpt_hppe_policer_rate_to_refresh(dev_id, policer->eir_max,
				&appe_eir_max_ref,
				policer->meter_unit,
				token_unit);
		SW_RTN_ON_ERROR(rv);
	}
	if (policer->meter_type == FAL_POLICER_METER_RFC) {
		if (policer->couple_en == A_FALSE) {
			appe_cir_max_ref = cir_ref;
			appe_eir_max_ref = eir_ref;
		} else {
			appe_cir_max_ref = cir_ref;
			appe_eir_max_ref = ADPT_APPE_POLICER_MAX;
		}
	}

	rv = __adpt_hppe_policer_burst_size_to_bucket_size(dev_id, policer->cbs,
				&cbs_bkt,
				policer->meter_unit,
				token_unit);
	SW_RTN_ON_ERROR(rv);

	rv = __adpt_hppe_policer_burst_size_to_bucket_size(dev_id, policer->ebs,
				&ebs_bkt,
				policer->meter_unit,
				token_unit);
	SW_RTN_ON_ERROR(rv);

	if (!g_ppe_acl_meter_ops[dev_id])
		return SW_FAIL;
	if (!g_ppe_acl_meter_ops[dev_id]->read_ir)
		return SW_FAIL;
	if (!g_ppe_acl_meter_ops[dev_id]->write_ir)
		return SW_FAIL;

	rv = g_ppe_acl_meter_ops[dev_id]->read_ir(dev_id, index, &ir);
	SW_RTN_ON_ERROR(rv);

	ir.meter_en   = policer->meter_en;
	ir.color_mode = policer->color_mode;
	ir.couple_en  = policer->couple_en;
	ir.meter_mode = policer->meter_mode;
	ir.meter_unit = policer->meter_unit;
	ir.token_unit = token_unit;

	ir.cir_refresh = cir_ref;
	ir.eir_refresh = eir_ref;
	ir.cbs_bucket  = cbs_bkt;
	ir.ebs_bucket  = ebs_bkt;

	ir.cir_max_refresh = appe_cir_max_ref;
	ir.eir_max_refresh = appe_eir_max_ref;

	ir.exceed_chg_pri_cmd = action->yellow_priority_en;
	ir.exceed_chg_dp_cmd  = action->yellow_drop_priority_en;
	ir.exceed_chg_pcp_cmd = action->yellow_pcp_en;
	ir.exceed_chg_dei_cmd = action->yellow_dei_en;
	ir.exceed_pri         = action->yellow_priority;
	ir.exceed_dp          = action->yellow_drop_priority;
	ir.exceed_pcp         = action->yellow_pcp;
	ir.exceed_dei         = action->yellow_dei;

	ir.violate_cmd         = (action->red_action == FAL_MAC_DROP) ? 0 : 1;
	ir.violate_chg_pri_cmd = action->red_priority_en;
	ir.violate_chg_dp_cmd  = action->red_drop_priority_en;
	ir.violate_chg_pcp_cmd = action->red_pcp_en;
	ir.violate_chg_dei_cmd = action->red_dei_en;
	ir.violate_pri         = action->red_priority;
	ir.violate_dp          = action->red_drop_priority;
	ir.violate_pcp         = action->red_pcp;
	ir.violate_dei         = action->red_dei;

	ir.exceed_chg_dscp_cmd = action->yellow_dscp_en;
	ir.exceed_dscp         = action->yellow_dscp;
	ir.violate_chg_dscp_cmd= action->red_dscp_en;
	ir.violate_dscp        = action->red_dscp;
	ir.exceed_remap_cmd    = action->yellow_remap_en;
	ir.violate_remap_cmd   = action->red_remap_en;

	if (policer->meter_type == FAL_POLICER_METER_MEF10_3) {
		ir.grp_end  = policer->grp_end;
		ir.grp_cf   = policer->grp_couple_en;
		ir.nxt_ptr  = policer->next_ptr;
	} else {
		ir.grp_end = 0x1;
	}
	ir.length_mod = policer->length_mode;
	
	rv = g_ppe_acl_meter_ops[dev_id]->write_ir(dev_id, index, &ir);
	SW_RTN_ON_ERROR(rv);

	appe_policer_type[dev_id][index] = policer->meter_type;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_policer_entry_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_policer_config_t *policer, fal_policer_action_t *action)
{
	union in_port_meter_cfg_tbl_u in_port_meter_cfg_tbl;
	a_uint32_t hppe_cir =0, hppe_cbs = 0, hppe_eir = 0,hppe_ebs = 0;
	union l2_vp_port_tbl_u l2_vp_port_tbl;
	sw_error_t rv = SW_OK;

	memset(&in_port_meter_cfg_tbl, 0, sizeof(in_port_meter_cfg_tbl));
	memset(&l2_vp_port_tbl, 0, sizeof(l2_vp_port_tbl));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(policer);
	ADPT_NULL_POINT_CHECK(action);

	if (ADPT_IS_VPORT(port_id)) {
		port_id = FAL_PORT_ID_VALUE(port_id);
		rv = appe_l2_vp_port_tbl_get(dev_id, port_id, &l2_vp_port_tbl);
		SW_RTN_ON_ERROR(rv);
		policer->vp_meter_index = l2_vp_port_tbl.bf.policer_index;
		rv = adpt_hppe_acl_policer_entry_get(dev_id,
				policer->vp_meter_index, policer, action);
		SW_RTN_ON_ERROR(rv);
		policer->meter_en = (policer->meter_en & l2_vp_port_tbl.bf.policer_en);

		return rv;
	}
	if (port_id > adpt_ppe_port_id_max_get(dev_id))
		return SW_BAD_PARAM;

	hppe_in_port_meter_cfg_tbl_get(dev_id, port_id, &in_port_meter_cfg_tbl);

	hppe_cir = (in_port_meter_cfg_tbl.bf.cir_1 << 3) | in_port_meter_cfg_tbl.bf.cir_0;
	hppe_cbs = in_port_meter_cfg_tbl.bf.cbs;
	hppe_eir = (in_port_meter_cfg_tbl.bf.eir_1 << 1) | in_port_meter_cfg_tbl.bf.eir_0;
	hppe_ebs = in_port_meter_cfg_tbl.bf.ebs;


	__adpt_hppe_policer_refresh_to_rate(dev_id, hppe_cir,
				&policer->cir,
				in_port_meter_cfg_tbl.bf.meter_unit,
				in_port_meter_cfg_tbl.bf.token_unit);

	__adpt_hppe_policer_refresh_to_rate(dev_id, hppe_eir,
				&policer->eir,
				in_port_meter_cfg_tbl.bf.meter_unit,
				in_port_meter_cfg_tbl.bf.token_unit);

	__adpt_hppe_policer_bucket_size_to_burst_size(dev_id, hppe_cbs,
				&policer->cbs,
				in_port_meter_cfg_tbl.bf.meter_unit,
				in_port_meter_cfg_tbl.bf.token_unit);

	__adpt_hppe_policer_bucket_size_to_burst_size(dev_id, hppe_ebs,
				&policer->ebs,
				in_port_meter_cfg_tbl.bf.meter_unit,
				in_port_meter_cfg_tbl.bf.token_unit);

	policer->meter_en = in_port_meter_cfg_tbl.bf.meter_en;
	policer->color_mode = in_port_meter_cfg_tbl.bf.color_mode;
	policer->frame_type = in_port_meter_cfg_tbl.bf.meter_flag;
	policer->couple_en = in_port_meter_cfg_tbl.bf.coupling_flag;
	policer->meter_mode = in_port_meter_cfg_tbl.bf.meter_mode;
	policer->meter_unit = in_port_meter_cfg_tbl.bf.meter_unit;
	action->yellow_priority_en = in_port_meter_cfg_tbl.bf.exceed_chg_pri_cmd;
	action->yellow_drop_priority_en = in_port_meter_cfg_tbl.bf.exceed_chg_dp_cmd;
	action->yellow_pcp_en = in_port_meter_cfg_tbl.bf.exceed_chg_pcp_cmd;
	action->yellow_dei_en = in_port_meter_cfg_tbl.bf.exceed_chg_dei_cmd;
	action->yellow_priority = in_port_meter_cfg_tbl.bf.exceed_pri;
	action->yellow_drop_priority = in_port_meter_cfg_tbl.bf.exceed_dp;
	action->yellow_pcp = in_port_meter_cfg_tbl.bf.exceed_pcp;
	action->yellow_dei = in_port_meter_cfg_tbl.bf.exceed_dei;
	if (in_port_meter_cfg_tbl.bf.violate_cmd == 0)
		action->red_action = FAL_MAC_DROP;
	else
		action->red_action = FAL_MAC_FRWRD;
	action->red_priority_en = in_port_meter_cfg_tbl.bf.violate_chg_pri_cmd;
	action->red_drop_priority_en = in_port_meter_cfg_tbl.bf.violate_chg_dp_cmd;
	action->red_pcp_en = in_port_meter_cfg_tbl.bf.violate_chg_pcp_cmd;
	action->red_dei_en = in_port_meter_cfg_tbl.bf.violate_chg_dei_cmd;
	action->red_priority = in_port_meter_cfg_tbl.bf.violate_pri;
	action->red_drop_priority = in_port_meter_cfg_tbl.bf.violate_dp;
	action->red_pcp = in_port_meter_cfg_tbl.bf.violate_pcp;
	action->red_dei = in_port_meter_cfg_tbl.bf.violate_dei;

	return SW_OK;
}

sw_error_t
adpt_hppe_policer_time_slot_get(a_uint32_t dev_id, a_uint32_t *time_slot)
{
	sw_error_t rv = SW_OK;
	union time_slot_reg_u time_slot_reg;

	memset(&time_slot_reg, 0, sizeof(time_slot_reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(time_slot);


	rv = hppe_time_slot_reg_get(dev_id, &time_slot_reg);

	if( rv != SW_OK )
		return rv;

	*time_slot = time_slot_reg.bf.time_slot;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_policer_entry_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_policer_config_t *policer, fal_policer_action_t *action)
{
	union in_port_meter_cfg_tbl_u in_port_meter_cfg_tbl;
	a_uint32_t hppe_cir = 0, hppe_cbs = 0, hppe_eir = 0,hppe_ebs = 0;
	a_uint64_t temp_cir = 0, temp_eir =0, temp_cbs =0, temp_ebs = 0;
	a_uint32_t token_unit = 0;
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_port_tbl;
	a_uint32_t byte_max_rate  = 0;
	a_uint32_t frame_max_rate = 0;

	memset(&in_port_meter_cfg_tbl, 0, sizeof(in_port_meter_cfg_tbl));
	memset(&l2_vp_port_tbl, 0, sizeof(l2_vp_port_tbl));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(policer);
	ADPT_NULL_POINT_CHECK(action);

	if (ADPT_IS_VPORT(port_id)) {
		port_id = FAL_PORT_ID_VALUE(port_id);
		rv = appe_l2_vp_port_tbl_get(dev_id, port_id, &l2_vp_port_tbl);
		SW_RTN_ON_ERROR(rv);
		l2_vp_port_tbl.bf.policer_en = policer->meter_en;
		l2_vp_port_tbl.bf.policer_index = policer->vp_meter_index;
		rv = appe_l2_vp_port_tbl_set(dev_id, port_id, &l2_vp_port_tbl);
		SW_RTN_ON_ERROR(rv);
		rv = adpt_hppe_acl_policer_entry_set(dev_id,
				policer->vp_meter_index, policer, action);
		SW_RTN_ON_ERROR(rv);

		return rv;
	}
	if (port_id > adpt_ppe_port_id_max_get(dev_id))
		return SW_BAD_PARAM;

	byte_max_rate  = __adpt_ppe_policer_byte_max_rate_get(dev_id);
	frame_max_rate = __adpt_ppe_policer_frame_max_rate_get(dev_id);

	if(ADPT_HPPE_POLICER_METER_UNIT_BYTE == policer->meter_unit)
	{
		if ((policer->cir > byte_max_rate) || (policer->eir > byte_max_rate))
			return SW_BAD_PARAM;
		if ((policer->cir < BYTE_POLICER_MIN_RATE) && (policer->cir != 0))
			return SW_BAD_PARAM;
		if ((policer->eir < BYTE_POLICER_MIN_RATE) && (policer->eir != 0))
			return SW_BAD_PARAM;
	}

	if(ADPT_HPPE_POLICER_METER_UNIT_FRAME == policer->meter_unit)
	{
		if ((policer->cir > frame_max_rate) || (policer->eir > frame_max_rate))
			return SW_BAD_PARAM;
		if ((policer->cir < FRAME_POLICER_MIN_RATE) && (policer->cir != 0))
			return SW_BAD_PARAM;
		if ((policer->eir < FRAME_POLICER_MIN_RATE) && (policer->eir != 0))
			return SW_BAD_PARAM;
	}

	if((0 == policer->meter_mode) && (policer->cir > policer->eir))
		return SW_BAD_PARAM;

	temp_cir = ((a_uint64_t)policer->cir) * 1000;
	temp_cbs = ((a_uint64_t)policer->cbs) * 1000;
	temp_eir = ((a_uint64_t)policer->eir) * 1000;
	temp_ebs = ((a_uint64_t)policer->ebs) * 1000;

	rv = __adpt_hppe_policer_two_bucket_parameter_select(dev_id, temp_cir,
				temp_cbs,
				temp_eir,
				temp_ebs,
				policer->meter_unit,
				&token_unit);
	if( rv != SW_OK )
		return rv;

	SSDK_DEBUG("current meter unit is = %d\n", policer->meter_unit);
	SSDK_DEBUG("current token unit is = %d\n", token_unit);

	__adpt_hppe_policer_rate_to_refresh(dev_id, policer->cir,
				&hppe_cir,
				policer->meter_unit,
				token_unit);

	__adpt_hppe_policer_rate_to_refresh(dev_id, policer->eir,
				&hppe_eir,
				policer->meter_unit,
				token_unit);

	__adpt_hppe_policer_burst_size_to_bucket_size(dev_id, policer->cbs,
				&hppe_cbs,
				policer->meter_unit,
				token_unit);

	__adpt_hppe_policer_burst_size_to_bucket_size(dev_id, policer->ebs,
				&hppe_ebs,
				policer->meter_unit,
				token_unit);

	in_port_meter_cfg_tbl.bf.meter_en = policer->meter_en;
	in_port_meter_cfg_tbl.bf.color_mode = policer->color_mode;
	in_port_meter_cfg_tbl.bf.meter_flag = policer->frame_type;
	in_port_meter_cfg_tbl.bf.coupling_flag = policer->couple_en;
	in_port_meter_cfg_tbl.bf.meter_mode = policer->meter_mode;
	in_port_meter_cfg_tbl.bf.token_unit = token_unit;
	in_port_meter_cfg_tbl.bf.meter_unit = (a_uint32_t)policer->meter_unit;
	in_port_meter_cfg_tbl.bf.cbs = hppe_cbs;
	in_port_meter_cfg_tbl.bf.cir_0 = hppe_cir & 0x7;
	in_port_meter_cfg_tbl.bf.cir_1 = hppe_cir >> 3;
	in_port_meter_cfg_tbl.bf.ebs = hppe_ebs;
	in_port_meter_cfg_tbl.bf.eir_0 = hppe_eir & 0x1;
	in_port_meter_cfg_tbl.bf.eir_1 = hppe_eir >> 1;
	in_port_meter_cfg_tbl.bf.exceed_chg_pri_cmd = action->yellow_priority_en;
	in_port_meter_cfg_tbl.bf.exceed_chg_dp_cmd = action->yellow_drop_priority_en;
	in_port_meter_cfg_tbl.bf.exceed_chg_pcp_cmd = action->yellow_pcp_en;
	in_port_meter_cfg_tbl.bf.exceed_chg_dei_cmd = action->yellow_dei_en;
	in_port_meter_cfg_tbl.bf.exceed_pri = action->yellow_priority;
	in_port_meter_cfg_tbl.bf.exceed_dp = action->yellow_drop_priority;
	in_port_meter_cfg_tbl.bf.exceed_pcp = action->yellow_pcp;
	in_port_meter_cfg_tbl.bf.exceed_dei = action->yellow_dei;
	if (action->red_action == FAL_MAC_DROP)
		in_port_meter_cfg_tbl.bf.violate_cmd = 0;
	else
		in_port_meter_cfg_tbl.bf.violate_cmd = 1;
	in_port_meter_cfg_tbl.bf.violate_chg_pri_cmd = action->red_priority_en;
	in_port_meter_cfg_tbl.bf.violate_chg_dp_cmd = action->red_drop_priority_en;
	in_port_meter_cfg_tbl.bf.violate_chg_pcp_cmd = action->red_pcp_en;
	in_port_meter_cfg_tbl.bf.violate_chg_dei_cmd = action->red_dei_en;
	in_port_meter_cfg_tbl.bf.violate_pri = action->red_priority;
	in_port_meter_cfg_tbl.bf.violate_dp = action->red_drop_priority;
	in_port_meter_cfg_tbl.bf.violate_pcp = action->red_pcp;
	in_port_meter_cfg_tbl.bf.violate_dei = action->red_dei;

	hppe_in_port_meter_cfg_tbl_set(dev_id, port_id, &in_port_meter_cfg_tbl);

	return SW_OK;

}

sw_error_t
adpt_hppe_port_compensation_byte_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t  length)
{
	union meter_cmpst_length_reg_u meter_cmpst_length_reg;

	memset(&meter_cmpst_length_reg, 0, sizeof(meter_cmpst_length_reg));
	ADPT_DEV_ID_CHECK(dev_id);

	if (port_id > adpt_ppe_port_id_max_get(dev_id))
		return SW_BAD_PARAM;

	if (length > 0x1f)
		return SW_BAD_PARAM;

	hppe_meter_cmpst_length_reg_get(dev_id, port_id, &meter_cmpst_length_reg);
	meter_cmpst_length_reg.bf.cmpst_length = length;

	hppe_meter_cmpst_length_reg_set(dev_id, port_id, &meter_cmpst_length_reg);

	return SW_OK;
}

static inline a_uint32_t __adpt_ppe_policer_min_timeslot_get(a_uint32_t dev_id)
{
	a_uint32_t min = 0;
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);

	switch (ppe_type) {
	case MPPE_TYPE:
	case HMSPPE_TYPE:
	case HTTPPE_TYPE:
		min = 256;
		break;
	case APPE_TYPE:
	case MRPPE_TYPE:
	case JHPPE_TYPE:
		min = 1024;
		break;
	default:
		break;
	}

	return min;
}

sw_error_t
adpt_hppe_policer_time_slot_set(a_uint32_t dev_id, a_uint32_t time_slot)
{
	union time_slot_reg_u time_slot_reg;
	sw_error_t rv = SW_OK;

	memset(&time_slot_reg, 0, sizeof(time_slot_reg));
	ADPT_DEV_ID_CHECK(dev_id);

	if ((time_slot > ADPT_POLICER_TIME_SLOT_MAX) ||
		(time_slot < __adpt_ppe_policer_min_timeslot_get(dev_id)))
		return SW_BAD_PARAM;

#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE) {
		rv = adpt_httppe_policer_time_slot_set(dev_id, time_slot);
		SW_RTN_ON_ERROR(rv);
	} else
#endif
	{
		time_slot_reg.bf.time_slot = time_slot;
		hppe_time_slot_reg_set(dev_id, &time_slot_reg);
	}

	__adpt_hppe_policer_max_rate(dev_id, time_slot);

	return SW_OK;
}
#ifndef IN_POLICER_MINI
sw_error_t
adpt_hppe_policer_global_counter_get(a_uint32_t dev_id,
		fal_policer_global_counter_t *counter)
{
	union pc_global_cnt_tbl_u pc_global_cnt_tbl;
	a_uint32_t index = 0;

	memset(&pc_global_cnt_tbl, 0, sizeof(pc_global_cnt_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(counter);

	hppe_pc_global_cnt_tbl_get(dev_id, index, &pc_global_cnt_tbl);
	counter->policer_drop_packet_counter = pc_global_cnt_tbl.bf.pkt_cnt;
	counter->policer_drop_byte_counter = pc_global_cnt_tbl.bf.byte_cnt_1;
	counter->policer_drop_byte_counter = (counter->policer_drop_byte_counter << 32) |
		pc_global_cnt_tbl.bf.byte_cnt_0;

	hppe_pc_global_cnt_tbl_get(dev_id, index + 1, &pc_global_cnt_tbl);
	counter->policer_forward_packet_counter = pc_global_cnt_tbl.bf.pkt_cnt;
	counter->policer_forward_byte_counter = pc_global_cnt_tbl.bf.byte_cnt_1;
	counter->policer_forward_byte_counter = (counter->policer_forward_byte_counter << 32) |
		pc_global_cnt_tbl.bf.byte_cnt_0;

	hppe_pc_global_cnt_tbl_get(dev_id, index + 2, &pc_global_cnt_tbl);
	counter->policer_bypass_packet_counter = pc_global_cnt_tbl.bf.pkt_cnt;
	counter->policer_bypass_byte_counter = pc_global_cnt_tbl.bf.byte_cnt_1;
	counter->policer_bypass_byte_counter = (counter->policer_bypass_byte_counter << 32) |
		pc_global_cnt_tbl.bf.byte_cnt_0;

	return SW_OK;
}
#endif

sw_error_t
adpt_hppe_policer_bypass_en_get(a_uint32_t dev_id, fal_policer_frame_type_t frame_type,
	a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union pc_drop_bypass_reg_u drop_bypass_reg;

	memset(&drop_bypass_reg, 0, sizeof(drop_bypass_reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	rv = hppe_pc_drop_bypass_reg_get(dev_id, &drop_bypass_reg);
	SW_RTN_ON_ERROR (rv);

	if (frame_type == FAL_FRAME_DROPPED) {
		*enable = drop_bypass_reg.bf.drop_bypass_en;
	} else {
		return SW_BAD_PARAM;
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_policer_bypass_en_set(a_uint32_t dev_id, fal_policer_frame_type_t frame_type,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union pc_drop_bypass_reg_u drop_bypass_reg;

	memset(&drop_bypass_reg, 0, sizeof(drop_bypass_reg));
	ADPT_DEV_ID_CHECK(dev_id);

	if (frame_type == FAL_FRAME_DROPPED) {
		drop_bypass_reg.bf.drop_bypass_en = enable;
	} else {
		return SW_BAD_PARAM;
	}

	rv = hppe_pc_drop_bypass_reg_set(dev_id, &drop_bypass_reg);
	SW_RTN_ON_ERROR (rv);

	return SW_OK;
}

sw_error_t adpt_hppe_policer_deinit(a_uint32_t dev_id)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (g_ppe_policer_tbl[dev_id]) {
		aos_mem_free(g_ppe_policer_tbl[dev_id]);
		g_ppe_policer_tbl[dev_id] = NULL;
	}
	g_ppe_acl_meter_ops[dev_id] = NULL;

	return SW_OK;
}

/* ops instances */
static const struct adpt_ppe_acl_meter_ops hppe_acl_meter_ops = {
	.read_ir  = __adpt_hppe_acl_meter_cfg_read_ir,
	.write_ir = __adpt_hppe_acl_meter_cfg_write_ir,
};
#if defined(HTTPPE)
static const struct adpt_ppe_acl_meter_ops httppe_acl_meter_ops = {
	.read_ir  = adpt_httppe_acl_meter_cfg_read_ir,
	.write_ir = adpt_httppe_acl_meter_cfg_write_ir,
};
#endif

sw_error_t adpt_hppe_policer_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;
	sw_error_t rv = SW_OK;

	rv = adpt_ppe_policer_table_init(dev_id);
	if (rv != SW_OK)
		return rv;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE) {
		g_ppe_acl_meter_ops[dev_id] = &httppe_acl_meter_ops;
		p_adpt_api->adpt_policer_bypass_en_get = adpt_httppe_policer_bypass_en_get;
		p_adpt_api->adpt_policer_bypass_en_set = adpt_httppe_policer_bypass_en_set;
		p_adpt_api->adpt_policer_time_slot_get = adpt_httppe_policer_time_slot_get;
	} else
#endif
	{
		g_ppe_acl_meter_ops[dev_id] = &hppe_acl_meter_ops;
		p_adpt_api->adpt_policer_bypass_en_get = adpt_hppe_policer_bypass_en_get;
		p_adpt_api->adpt_policer_bypass_en_set = adpt_hppe_policer_bypass_en_set;
		p_adpt_api->adpt_policer_time_slot_get = adpt_hppe_policer_time_slot_get;
	}

#ifndef IN_POLICER_MINI
#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
		p_adpt_api->adpt_policer_global_counter_get = adpt_httppe_policer_global_counter_get;
	else
#endif
		p_adpt_api->adpt_policer_global_counter_get = adpt_hppe_policer_global_counter_get;
	p_adpt_api->adpt_policer_priority_remap_get = adpt_appe_policer_priority_remap_get;
	p_adpt_api->adpt_policer_priority_remap_set = adpt_appe_policer_priority_remap_set;
#endif
	p_adpt_api->adpt_acl_policer_counter_get = adpt_hppe_acl_policer_counter_get;
	p_adpt_api->adpt_port_policer_counter_get = adpt_hppe_port_policer_counter_get;
	p_adpt_api->adpt_port_compensation_byte_get = adpt_hppe_port_compensation_byte_get;
	p_adpt_api->adpt_port_policer_entry_get = adpt_hppe_port_policer_entry_get;
	p_adpt_api->adpt_acl_policer_entry_get = adpt_hppe_acl_policer_entry_get;
	p_adpt_api->adpt_port_policer_entry_set = adpt_hppe_port_policer_entry_set;
	p_adpt_api->adpt_acl_policer_entry_set = adpt_hppe_acl_policer_entry_set;
	p_adpt_api->adpt_port_compensation_byte_set = adpt_hppe_port_compensation_byte_set;
	p_adpt_api->adpt_policer_time_slot_set = adpt_hppe_policer_time_slot_set;
	p_adpt_api->adpt_policer_ctrl_get = adpt_appe_policer_ctrl_get;
	p_adpt_api->adpt_policer_ctrl_set = adpt_appe_policer_ctrl_set;

	return SW_OK;
}

/**
 * @}
 */

