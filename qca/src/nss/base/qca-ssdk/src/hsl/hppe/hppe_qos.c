/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
*  Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_tdm_depth_cfg_get(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + TDM_DEPTH_CFG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_tdm_depth_cfg_set(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + TDM_DEPTH_CFG_ADDRESS,
				value->val);
}

#if defined(JHPPE)
sw_error_t
hppe_l0_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_MAP_TBL_ADDRESS + \
				index * L0_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l0_flow_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l0_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_MAP_TBL_ADDRESS + \
				index * L0_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l0_flow_map_tbl_u)/sizeof(a_uint32_t));
}

#else
sw_error_t
hppe_l0_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value)
{
	if (index >= L0_FLOW_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_MAP_TBL_ADDRESS + \
				index * L0_FLOW_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l0_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_MAP_TBL_ADDRESS + \
				index * L0_FLOW_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_l0_c_sp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_c_sp_cfg_tbl_u *value)
{
	if (index >= L0_C_SP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_C_SP_CFG_TBL_ADDRESS + \
				index * L0_C_SP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l0_c_sp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_c_sp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_C_SP_CFG_TBL_ADDRESS + \
				index * L0_C_SP_CFG_TBL_INC,
				value->val);
}

sw_error_t
hppe_l0_e_sp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_e_sp_cfg_tbl_u *value)
{
	if (index >= L0_E_SP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_E_SP_CFG_TBL_ADDRESS + \
				index * L0_E_SP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l0_e_sp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_e_sp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_E_SP_CFG_TBL_ADDRESS + \
				index * L0_E_SP_CFG_TBL_INC,
				value->val);
}

#endif
sw_error_t
hppe_l0_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value)
{
	if (index >= L0_FLOW_PORT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L0_FLOW_PORT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l0_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L0_FLOW_PORT_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_ring_q_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ring_q_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + RING_Q_MAP_TBL_ADDRESS + \
				index * RING_Q_MAP_TBL_INC,
				value->val,
				sizeof(union ring_q_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_ring_q_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ring_q_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + RING_Q_MAP_TBL_ADDRESS + \
				index * RING_Q_MAP_TBL_INC,
				value->val,
				sizeof(union ring_q_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_rfc_block_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfc_block_tbl_u *value)
{
	if (index >= RFC_BLOCK_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + RFC_BLOCK_TBL_ADDRESS + \
				index * RFC_BLOCK_TBL_INC,
				&value->val);
}

sw_error_t
hppe_rfc_status_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfc_status_tbl_u *value)
{
	if (index >= RFC_STATUS_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + RFC_STATUS_TBL_ADDRESS + \
				index * RFC_STATUS_TBL_INC,
				&value->val);
}

sw_error_t
hppe_deq_dis_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union deq_dis_tbl_u *value)
{
	if (index >= DEQ_DIS_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + DEQ_DIS_TBL_ADDRESS + \
				index * DEQ_DIS_TBL_INC,
				&value->val);
}

sw_error_t
hppe_deq_dis_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union deq_dis_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + DEQ_DIS_TBL_ADDRESS + \
				index * DEQ_DIS_TBL_INC,
				value->val);
}

#if defined(JHPPE)
sw_error_t
hppe_l1_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_MAP_TBL_ADDRESS + \
				index * L1_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l1_flow_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l1_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_MAP_TBL_ADDRESS + \
				index * L1_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l1_flow_map_tbl_u)/sizeof(a_uint32_t));
}

#else
sw_error_t
hppe_l1_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value)
{
	if (index >= L1_FLOW_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_MAP_TBL_ADDRESS + \
				index * L1_FLOW_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l1_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_MAP_TBL_ADDRESS + \
				index * L1_FLOW_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_l1_c_sp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_c_sp_cfg_tbl_u *value)
{
	if (index >= L1_C_SP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_C_SP_CFG_TBL_ADDRESS + \
				index * L1_C_SP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l1_c_sp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_c_sp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_C_SP_CFG_TBL_ADDRESS + \
				index * L1_C_SP_CFG_TBL_INC,
				value->val);
}

sw_error_t
hppe_l1_e_sp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_e_sp_cfg_tbl_u *value)
{
	if (index >= L1_E_SP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_E_SP_CFG_TBL_ADDRESS + \
				index * L1_E_SP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l1_e_sp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_e_sp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_E_SP_CFG_TBL_ADDRESS + \
				index * L1_E_SP_CFG_TBL_INC,
				value->val);
}

#endif
sw_error_t
hppe_l1_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value)
{
	if (index >= L1_FLOW_PORT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L1_FLOW_PORT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l1_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L1_FLOW_PORT_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_psch_tdm_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value)
{
	if (index >= PSCH_TDM_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_TDM_CFG_TBL_ADDRESS + \
				index * PSCH_TDM_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_psch_tdm_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_TDM_CFG_TBL_ADDRESS + \
				index * PSCH_TDM_CFG_TBL_INC,
				value->val);
}

