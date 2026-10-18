/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_htt_reg.h"

sw_error_t
httppe_ipmc_tbl_op_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	return hppe_reg_set(
			dev_id,
			IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA0_ADDRESS +
			IPMC_TBL_OP_DATA0_INC * index,
			value);
}

sw_error_t
httppe_ipmc_tbl_op_data0_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA0_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data0_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA0_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data1_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA1_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data1_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA1_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data2_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data2_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA2_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data2_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data2_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA2_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data3_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data3_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA3_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data3_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data3_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA3_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data4_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data4_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA4_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data4_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data4_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA4_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data5_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data5_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA5_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data5_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data5_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA5_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data6_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data6_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA6_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data6_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data6_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA6_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data7_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data7_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA7_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data7_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data7_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA7_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data8_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data8_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA8_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data8_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data8_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA8_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data9_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data9_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA9_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data9_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data9_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA9_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data10_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data10_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA10_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_op_data10_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data10_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_DATA10_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	return hppe_reg_set(
			dev_id,
			IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA0_ADDRESS +
			IPMC_TBL_RD_OP_DATA0_INC * index,
			value);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data0_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA0_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data0_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA0_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data1_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA1_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data1_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA1_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data2_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data2_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA2_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data2_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data2_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA2_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data3_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data3_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA3_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data3_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data3_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA3_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data4_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data4_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA4_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data4_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data4_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA4_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data5_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data5_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA5_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data5_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data5_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA5_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data6_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data6_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA6_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data6_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data6_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA6_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data7_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data7_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA7_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data7_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data7_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA7_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data8_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data8_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA8_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data8_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data8_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA8_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data9_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data9_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA9_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data9_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data9_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA9_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data10_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data10_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA10_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_data10_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data10_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_DATA10_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	return hppe_reg_get(
			dev_id,
			IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA0_ADDRESS +
			IPMC_TBL_RD_RSLT_DATA0_INC * index,
			value);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data0_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA0_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data1_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA1_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data2_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data2_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA2_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data3_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data3_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA3_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data4_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data4_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA4_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data5_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data5_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA5_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data6_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data6_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA6_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data7_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data7_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA7_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data8_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data8_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA8_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data9_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data9_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA9_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_tbl_rd_rslt_data10_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data10_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_TBL_RD_RSLT_DATA10_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_get(
		a_uint32_t dev_id,
		union ipmc_convert_uc_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_CONVERT_UC_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_set(
		a_uint32_t dev_id,
		union ipmc_convert_uc_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_CONVERT_UC_CTRL_ADDRESS,
				value->val);
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_convert_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.convert_port_vp;
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_convert_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.convert_port_vp = value;
	ret = httppe_ipmc_convert_uc_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_enable_port_num_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.enable_port_num;
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_enable_port_num_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enable_port_num = value;
	ret = httppe_ipmc_convert_uc_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.post_bypass_bitmap;
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.post_bypass_bitmap = value;
	ret = httppe_ipmc_convert_uc_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.service_code;
	return ret;
}

sw_error_t
httppe_ipmc_convert_uc_ctrl_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union ipmc_convert_uc_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_ipmc_convert_uc_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.service_code = value;
	ret = httppe_ipmc_convert_uc_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
httppe_ipmc_gipv4_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv4_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_GIPV4_TBL_ADDRESS + \
				index * IPMC_GIPV4_TBL_INC,
				value->val,
				sizeof(union ipmc_gipv4_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_gipv4_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv4_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_GIPV4_TBL_ADDRESS + \
				index * IPMC_GIPV4_TBL_INC,
				value->val,
				sizeof(union ipmc_gipv4_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_gipv6_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv6_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_GIPV6_TBL_ADDRESS + \
				index * IPMC_GIPV6_TBL_INC,
				value->val,
				sizeof(union ipmc_gipv6_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_gipv6_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv6_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_GIPV6_TBL_ADDRESS + \
				index * IPMC_GIPV6_TBL_INC,
				value->val,
				sizeof(union ipmc_gipv6_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv4_gipv4_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_SIPV4_GIPV4_TBL_ADDRESS + \
				index * IPMC_SIPV4_GIPV4_TBL_INC,
				value->val,
				sizeof(union ipmc_sipv4_gipv4_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv4_gipv4_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_SIPV4_GIPV4_TBL_ADDRESS + \
				index * IPMC_SIPV4_GIPV4_TBL_INC,
				value->val,
				sizeof(union ipmc_sipv4_gipv4_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv6_gipv6_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_SIPV6_GIPV6_TBL_ADDRESS + \
				index * IPMC_SIPV6_GIPV6_TBL_INC,
				value->val,
				sizeof(union ipmc_sipv6_gipv6_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv6_gipv6_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPMC_SIPV6_GIPV6_TBL_ADDRESS + \
				index * IPMC_SIPV6_GIPV6_TBL_INC,
				value->val,
				sizeof(union ipmc_sipv6_gipv6_tbl_u)/sizeof(a_uint32_t));
}

