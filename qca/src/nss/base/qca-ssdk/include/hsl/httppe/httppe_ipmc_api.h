/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_IPMC_API_H_
#define _HTTPPE_IPMC_API_H_


sw_error_t
httppe_ipmc_tbl_op_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_u *value);

sw_error_t
httppe_ipmc_tbl_op_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_u *value);

sw_error_t
httppe_ipmc_tbl_op_byp_rslt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_byp_rslt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_op_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_op_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_op_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_op_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_tbl_op_data0_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data0_u *value);

sw_error_t
httppe_ipmc_tbl_op_data0_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data0_u *value);

sw_error_t
httppe_ipmc_tbl_op_data0_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data0_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data1_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data1_u *value);

sw_error_t
httppe_ipmc_tbl_op_data1_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data1_u *value);

sw_error_t
httppe_ipmc_tbl_op_data1_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data1_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data2_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data2_u *value);

sw_error_t
httppe_ipmc_tbl_op_data2_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data2_u *value);

sw_error_t
httppe_ipmc_tbl_op_data2_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data2_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data3_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data3_u *value);

sw_error_t
httppe_ipmc_tbl_op_data3_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data3_u *value);

sw_error_t
httppe_ipmc_tbl_op_data3_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data3_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data4_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data4_u *value);

sw_error_t
httppe_ipmc_tbl_op_data4_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data4_u *value);

sw_error_t
httppe_ipmc_tbl_op_data4_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data4_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data5_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data5_u *value);

sw_error_t
httppe_ipmc_tbl_op_data5_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data5_u *value);

sw_error_t
httppe_ipmc_tbl_op_data5_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data5_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data6_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data6_u *value);

sw_error_t
httppe_ipmc_tbl_op_data6_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data6_u *value);

sw_error_t
httppe_ipmc_tbl_op_data6_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data6_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data7_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data7_u *value);

sw_error_t
httppe_ipmc_tbl_op_data7_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data7_u *value);

sw_error_t
httppe_ipmc_tbl_op_data7_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data7_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data8_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data8_u *value);

sw_error_t
httppe_ipmc_tbl_op_data8_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data8_u *value);

sw_error_t
httppe_ipmc_tbl_op_data8_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data8_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data9_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data9_u *value);

sw_error_t
httppe_ipmc_tbl_op_data9_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data9_u *value);

sw_error_t
httppe_ipmc_tbl_op_data9_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data9_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_data10_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data10_u *value);

sw_error_t
httppe_ipmc_tbl_op_data10_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_data10_u *value);

sw_error_t
httppe_ipmc_tbl_op_data10_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_data10_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union ipmc_tbl_op_rslt_u *value);

sw_error_t
httppe_ipmc_tbl_op_rslt_set(
		a_uint32_t dev_id,
		union ipmc_tbl_op_rslt_u *value);

sw_error_t
httppe_ipmc_tbl_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_rslt_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_rslt_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_rslt_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_rslt_op_rslt_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_op_rslt_valid_cnt_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_byp_rslt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_byp_rslt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_op_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_op_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_op_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_op_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_tbl_rd_op_data0_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data0_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data0_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data0_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data0_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data0_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data1_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data1_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data1_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data1_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data1_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data1_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data2_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data2_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data2_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data2_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data2_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data2_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data3_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data3_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data3_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data3_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data3_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data3_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data4_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data4_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data4_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data4_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data4_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data4_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data5_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data5_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data5_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data5_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data5_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data5_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data6_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data6_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data6_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data6_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data6_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data6_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data7_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data7_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data7_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data7_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data7_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data7_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data8_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data8_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data8_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data8_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data8_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data8_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data9_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data9_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data9_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data9_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data9_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data9_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_data10_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data10_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data10_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_data10_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data10_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_data10_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_rslt_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_op_rslt_u *value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_op_rslt_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_op_rslt_valid_cnt_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);
sw_error_t
httppe_ipmc_tbl_rd_rslt_data0_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data0_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data0_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data0_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data0_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data0_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data1_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data1_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data1_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data1_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data1_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data1_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data2_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data2_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data2_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data2_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data2_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data2_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data3_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data3_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data3_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data3_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data3_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data3_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data4_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data4_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data4_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data4_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data4_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data4_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data5_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data5_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data5_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data5_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data5_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data5_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data6_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data6_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data6_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data6_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data6_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data6_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data7_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data7_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data7_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data7_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data7_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data7_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data8_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data8_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data8_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data8_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data8_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data8_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data9_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data9_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data9_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data9_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data9_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data9_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_tbl_rd_rslt_data10_get(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data10_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data10_set(
		a_uint32_t dev_id,
		union ipmc_tbl_rd_rslt_data10_u *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data10_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_tbl_rd_rslt_data10_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_ipmc_convert_uc_ctrl_get(
		a_uint32_t dev_id,
		union ipmc_convert_uc_ctrl_u *value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_set(
		a_uint32_t dev_id,
		union ipmc_convert_uc_ctrl_u *value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_convert_port_vp_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_convert_port_vp_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_enable_port_num_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_enable_port_num_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_service_code_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_ipmc_convert_uc_ctrl_service_code_set(
		a_uint32_t dev_id,
		unsigned int value);

#define IPMC_GIPV4_TBL_MAX_ENTRY	IPMC_GIPV4_TBL_NUM

sw_error_t
httppe_ipmc_gipv4_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv4_tbl_u *value);

sw_error_t
httppe_ipmc_gipv4_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv4_tbl_u *value);

sw_error_t
httppe_ipmc_gipv4_tbl_check_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_check_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_check_src_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_check_src_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_check_src_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_check_src_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_convert_uc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_convert_uc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_ip_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_ip_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_key_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_key_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_syn_toggle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_syn_toggle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv4_tbl_vid_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv4_tbl_vid_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IPMC_GIPV6_TBL_MAX_ENTRY	IPMC_GIPV6_TBL_NUM

sw_error_t
httppe_ipmc_gipv6_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv6_tbl_u *value);

sw_error_t
httppe_ipmc_gipv6_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_gipv6_tbl_u *value);

sw_error_t
httppe_ipmc_gipv6_tbl_check_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_check_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_check_src_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_check_src_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_check_src_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_check_src_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_convert_uc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_convert_uc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_ipv6_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_ipv6_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_key_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_key_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_syn_toggle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_syn_toggle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_gipv6_tbl_vid_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_gipv6_tbl_vid_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IPMC_SIPV4_GIPV4_TBL_MAX_ENTRY	IPMC_SIPV4_GIPV4_TBL_NUM

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv4_gipv4_tbl_u *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv4_gipv4_tbl_u *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_check_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_check_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_check_src_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_check_src_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_check_src_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_check_src_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_convert_uc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_convert_uc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_gip_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_gip_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_key_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_key_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_sip_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_sip_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_syn_toggle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_syn_toggle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_vid_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv4_gipv4_tbl_vid_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IPMC_SIPV6_GIPV6_TBL_MAX_ENTRY	IPMC_SIPV6_GIPV6_TBL_NUM

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv6_gipv6_tbl_u *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipmc_sipv6_gipv6_tbl_u *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_check_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_check_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_check_src_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_check_src_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_check_src_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_check_src_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_convert_uc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_convert_uc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_gipv6_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_gipv6_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_key_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_key_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_sipv6_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_sipv6_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_syn_toggle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_syn_toggle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_vid_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ipmc_sipv6_gipv6_tbl_vid_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
