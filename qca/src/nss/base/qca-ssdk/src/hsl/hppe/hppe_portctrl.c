/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
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
hppe_mac_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_enable_u *value)
{
	if (index >= MAC_ENABLE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_ENABLE_ADDRESS + \
				index * MAC_ENABLE_INC,
				&value->val);
}

sw_error_t
hppe_mac_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_enable_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_ENABLE_ADDRESS + \
				index * MAC_ENABLE_INC,
				value->val);
}

sw_error_t
hppe_mac_speed_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_speed_u *value)
{
	if (index >= MAC_SPEED_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_SPEED_ADDRESS + \
				index * MAC_SPEED_INC,
				&value->val);
}

sw_error_t
hppe_mac_speed_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_speed_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_SPEED_ADDRESS + \
				index * MAC_SPEED_INC,
				value->val);
}

sw_error_t
hppe_mac_ctrl2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_ctrl2_u *value)
{
	if (index >= MAC_CTRL2_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_CTRL2_ADDRESS + \
				index * MAC_CTRL2_INC,
				&value->val);
}

sw_error_t
hppe_mac_ctrl2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_ctrl2_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_CTRL2_ADDRESS + \
				index * MAC_CTRL2_INC,
				value->val);
}

sw_error_t
hppe_mac_dbg_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_ctrl_u *value)
{
	if (index >= MAC_DBG_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_DBG_CTRL_ADDRESS + \
				index * MAC_DBG_CTRL_INC,
				&value->val);
}

sw_error_t
hppe_mac_dbg_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_DBG_CTRL_ADDRESS + \
				index * MAC_DBG_CTRL_INC,
				value->val);
}

sw_error_t
hppe_mac_dbg_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_addr_u *value)
{
	if (index >= MAC_DBG_ADDR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_DBG_ADDR_ADDRESS + \
				index * MAC_DBG_ADDR_INC,
				&value->val);
}

sw_error_t
hppe_mac_dbg_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_addr_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_DBG_ADDR_ADDRESS + \
				index * MAC_DBG_ADDR_INC,
				value->val);
}

sw_error_t
hppe_mac_dbg_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_data_u *value)
{
	if (index >= MAC_DBG_DATA_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_DBG_DATA_ADDRESS + \
				index * MAC_DBG_DATA_INC,
				&value->val);
}

sw_error_t
hppe_mac_jumbo_size_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_jumbo_size_u *value)
{
	if (index >= MAC_JUMBO_SIZE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_JUMBO_SIZE_ADDRESS + \
				index * MAC_JUMBO_SIZE_INC,
				&value->val);
}

sw_error_t
hppe_mac_jumbo_size_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_jumbo_size_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_JUMBO_SIZE_ADDRESS + \
				index * MAC_JUMBO_SIZE_INC,
				value->val);
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + MRU_MTU_CTRL_TBL_ADDRESS + \
				index * MRU_MTU_CTRL_TBL_INC,
				value->val,
				sizeof(union mru_mtu_ctrl_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + MRU_MTU_CTRL_TBL_ADDRESS + \
				index * MRU_MTU_CTRL_TBL_INC,
				value->val,
				sizeof(union mru_mtu_ctrl_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mc_mtu_ctrl_tbl_u *value)
{
	if (index >= MC_MTU_CTRL_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + MC_MTU_CTRL_TBL_ADDRESS + \
				index * MC_MTU_CTRL_TBL_INC,
				&value->val);
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mc_mtu_ctrl_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + MC_MTU_CTRL_TBL_ADDRESS + \
				index * MC_MTU_CTRL_TBL_INC,
				value->val);
}

sw_error_t
hppe_tdm_ctrl_get(
		a_uint32_t dev_id,
		union tdm_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + TDM_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
hppe_tdm_ctrl_set(
		a_uint32_t dev_id,
		union tdm_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + TDM_CTRL_ADDRESS,
				value->val);
}

sw_error_t
hppe_rx_fifo_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifo_cfg_u *value)
{
	if (index >= RX_FIFO_CFG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + RX_FIFO_CFG_ADDRESS + \
				index * RX_FIFO_CFG_INC,
				&value->val);
}

sw_error_t
hppe_rx_fifo_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifo_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + RX_FIFO_CFG_ADDRESS + \
				index * RX_FIFO_CFG_INC,
				value->val);
}

sw_error_t
hppe_tdm_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tdm_cfg_u *value)
{
	if (index >= TDM_CFG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + TDM_CFG_ADDRESS + \
				index * TDM_CFG_INC,
				&value->val);
}

sw_error_t
hppe_tdm_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tdm_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + TDM_CFG_ADDRESS + \
				index * TDM_CFG_INC,
				value->val);
}

sw_error_t
hppe_drop_stat_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + DROP_STAT_ADDRESS + \
				index * DROP_STAT_INC,
				value->val,
				3);
}

sw_error_t
hppe_drop_stat_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + DROP_STAT_ADDRESS + \
				index * DROP_STAT_INC,
				value->val,
				3);
}

sw_error_t
hppe_mac_ctrl2_ipg_dec_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipg_dec_en;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_ipg_dec_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipg_dec_en = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_mac_tx_thd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	//reg_val.bf.mac_tx_thd = value;//TO BE FIXED
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_crc_rsv_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.crc_rsv_en;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_crc_rsv_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.crc_rsv_en = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_crs_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.crs_sel;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_crs_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.crs_sel = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_ipg_dec_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipg_dec_len;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_ipg_dec_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipg_dec_len = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_maxfr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.maxfr;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_maxfr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.maxfr = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_mac_lpi_tx_idle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_lpi_tx_idle;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_mac_lpi_tx_idle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_lpi_tx_idle = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_mac_loop_back_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_loop_back;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_mac_loop_back_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_loop_back = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_ctrl2_test_pause_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.test_pause;
	return ret;
}

sw_error_t
hppe_mac_ctrl2_test_pause_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_ctrl2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.test_pause = value;
	ret = hppe_mac_ctrl2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_edxsdfr_transmit_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.edxsdfr_transmit_en;
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_edxsdfr_transmit_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.edxsdfr_transmit_en = value;
	ret = hppe_mac_dbg_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_mac_ipg_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_ipg_ctrl;
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_mac_ipg_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_ipg_ctrl = value;
	ret = hppe_mac_dbg_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_mac_len_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_len_ctrl;
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_mac_len_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_len_ctrl = value;
	ret = hppe_mac_dbg_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_ipgr1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipgr1;
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_ipgr1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipgr1 = value;
	ret = hppe_mac_dbg_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_dbg_addr_mac_debug_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_addr_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_addr_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_debug_addr;
	return ret;
}

sw_error_t
hppe_mac_dbg_addr_mac_debug_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_dbg_addr_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_addr_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_debug_addr = value;
	ret = hppe_mac_dbg_addr_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_dbg_data_mac_debug_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_data_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_data_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_debug_data;
	return ret;
}

sw_error_t
hppe_mac_jumbo_size_mac_jumbo_size_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_jumbo_size_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_jumbo_size_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mac_jumbo_size;
	return ret;
}

sw_error_t
hppe_mac_jumbo_size_mac_jumbo_size_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_jumbo_size_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_jumbo_size_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_jumbo_size = value;
	ret = hppe_mac_jumbo_size_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mtu_cmd;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mtu_cmd = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mtu;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mtu = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_rx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.rx_cnt_en;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_rx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_cnt_en = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_cnt_en;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_cnt_en = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mru_cmd;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mru_cmd = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mru;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mru = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.source_filtering_mode;
	return ret;
}

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.source_filtering_mode = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.source_filtering_bypass;
	return ret;
}

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.source_filtering_bypass = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.src_profile;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.src_profile = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mc_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mc_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mtu_cmd;
	return ret;
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mc_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mc_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mtu_cmd = value;
	ret = hppe_mc_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mc_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mc_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_cnt_en;
	return ret;
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mc_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mc_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_cnt_en = value;
	ret = hppe_mc_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mc_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mc_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mtu;
	return ret;
}

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mc_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mc_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mtu = value;
	ret = hppe_mc_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_in_forward_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_in_forward_u *value)
{
	if (index >= PORT_IN_FORWARD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_IN_FORWARD_ADDRESS + \
				index * PORT_IN_FORWARD_INC,
				&value->val);
}

sw_error_t
hppe_port_in_forward_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_in_forward_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_IN_FORWARD_ADDRESS + \
				index * PORT_IN_FORWARD_INC,
				value->val);
}

sw_error_t
hppe_drop_stat_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union drop_stat_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_drop_stat_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.bytes_1 << 32 | \
		reg_val.bf.bytes_0;
	return ret;
}

sw_error_t
hppe_drop_stat_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union drop_stat_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_drop_stat_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bytes_1 = value >> 32;
	reg_val.bf.bytes_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_drop_stat_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_drop_stat_pkts_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union drop_stat_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_drop_stat_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pkts;
	return ret;
}

sw_error_t
hppe_drop_stat_pkts_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union drop_stat_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_drop_stat_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pkts = value;
	ret = hppe_drop_stat_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_TX_COUNTER_TBL_REG_ADDRESS + \
				index * PORT_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union port_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_port_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_TX_COUNTER_TBL_REG_ADDRESS + \
				index * PORT_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union port_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vp_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + VP_TX_COUNTER_TBL_REG_ADDRESS + \
				index * VP_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union vp_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vp_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + VP_TX_COUNTER_TBL_REG_ADDRESS + \
				index * VP_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union vp_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union port_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.tx_bytes_1 << 32 | \
		reg_val.bf.tx_bytes_0;
	return ret;
}

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union port_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_bytes_1 = value >> 32;
	reg_val.bf.tx_bytes_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_port_tx_counter_tbl_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_packets;
	return ret;
}

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_packets = value;
	ret = hppe_port_tx_counter_tbl_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union vp_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vp_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.tx_bytes_1 << 32 | \
		reg_val.bf.tx_bytes_0;
	return ret;
}

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union vp_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vp_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_bytes_1 = value >> 32;
	reg_val.bf.tx_bytes_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_vp_tx_counter_tbl_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vp_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vp_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_packets;
	return ret;
}

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vp_tx_counter_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vp_tx_counter_tbl_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_packets = value;
	ret = hppe_vp_tx_counter_tbl_reg_set(dev_id, index, &reg_val);
	return ret;
}

#ifdef MRPPE
sw_error_t
mrppe_lpi_port_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_enable_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_ENABLE_ADDRESS + \
				index * LPI_PORT_ENABLE_INC,
				&value->val);
}

sw_error_t
mrppe_lpi_port_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_enable_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_ENABLE_ADDRESS + \
				index * LPI_PORT_ENABLE_INC,
				value->val);
}

sw_error_t
mrppe_lpi_wakeup_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_wakeup_timer_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_WAKEUP_TIMER_ADDRESS + \
				index * LPI_PORT_WAKEUP_TIMER_INC,
				&value->val);
}

sw_error_t
mrppe_lpi_wakeup_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_wakeup_timer_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_WAKEUP_TIMER_ADDRESS + \
				index * LPI_PORT_WAKEUP_TIMER_INC,
				value->val);
}

sw_error_t
mrppe_lpi_sleep_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_sleep_timer_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_SLEEP_TIMER_ADDRESS + \
				index * LPI_PORT_SLEEP_TIMER_INC,
				&value->val);
}

sw_error_t
mrppe_lpi_sleep_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_sleep_timer_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_SLEEP_TIMER_ADDRESS + \
				index * LPI_PORT_SLEEP_TIMER_INC,
				value->val);
}

sw_error_t
mrppe_lpi_1us_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_1us_cnt_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_1US_CNT_ADDRESS + \
				index * LPI_1US_CNT_INC,
				&value->val);
}

sw_error_t
mrppe_lpi_1us_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_1us_cnt_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_1US_CNT_ADDRESS + \
				index * LPI_1US_CNT_INC,
				value->val);
}

#else
sw_error_t
hppe_lpi_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_enable_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_ENABLE_ADDRESS + \
				index * LPI_ENABLE_INC,
				&value->val);
}

sw_error_t
hppe_lpi_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_enable_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_ENABLE_ADDRESS + \
				index * LPI_ENABLE_INC,
				value->val);
}

sw_error_t
hppe_lpi_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_timer_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_TIMER_ADDRESS + \
				index * LPI_PORT_TIMER_INC,
				&value->val);
}

sw_error_t
hppe_lpi_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_timer_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_PORT_TIMER_ADDRESS + \
				index * LPI_PORT_TIMER_INC,
				value->val);
}

sw_error_t
hppe_lpi_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_cnt_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_CNT_ADDRESS + \
				index * LPI_CNT_INC,
				&value->val);
}

sw_error_t
hppe_lpi_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_cnt_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_LPI_BASE_ADDR + LPI_CNT_ADDRESS + \
				index * LPI_CNT_INC,
				value->val);
}

#endif
sw_error_t
hppe_drop_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cnt_u *value)
{
	if (index >= DROP_CNT_NUM)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + DROP_CNT_ADDRESS + \
				index * DROP_CNT_INC,
				&value->val);
}

sw_error_t
hppe_drop_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cnt_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + DROP_CNT_ADDRESS + \
				index * DROP_CNT_INC,
				value->val);
}

sw_error_t
hppe_drop_cnt_drop_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union drop_cnt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_drop_cnt_get(dev_id, index, &reg_val);
	*value = reg_val.bf.drop_cnt;
	return ret;
}

sw_error_t
hppe_drop_cnt_drop_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union drop_cnt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_drop_cnt_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.drop_cnt = value;
	ret = hppe_drop_cnt_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_ipr_pkt_num_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_pkt_num_tbl_reg_u *value)
{
	if (index >= IPR_PKT_NUM_TBL_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_PKT_NUM_TBL_REG_ADDRESS + \
				index * IPR_PKT_NUM_TBL_REG_INC,
				&value->val);
}

sw_error_t
hppe_ipr_pkt_num_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_pkt_num_tbl_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_PKT_NUM_TBL_REG_ADDRESS + \
				index * IPR_PKT_NUM_TBL_REG_INC,
				value->val);
}

sw_error_t
hppe_ipr_byte_low_reg_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_low_reg_reg_u *value)
{
	if (index >= IPR_BYTE_LOW_REG_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_BYTE_LOW_REG_REG_ADDRESS + \
				index * IPR_BYTE_LOW_REG_REG_INC,
				&value->val);
}

sw_error_t
hppe_ipr_byte_low_reg_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_low_reg_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_BYTE_LOW_REG_REG_ADDRESS + \
				index * IPR_BYTE_LOW_REG_REG_INC,
				value->val);
}

sw_error_t
hppe_ipr_byte_high_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_high_reg_u *value)
{
	if (index >= IPR_BYTE_HIGH_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_BYTE_HIGH_REG_ADDRESS + \
				index * IPR_BYTE_HIGH_REG_INC,
				&value->val);
}

sw_error_t
hppe_ipr_byte_high_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_high_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_BYTE_HIGH_REG_ADDRESS + \
				index * IPR_BYTE_HIGH_REG_INC,
				value->val);
}

#ifdef JHPPE
sw_error_t
hppe_ipr_pkt_num_tbl_reg_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ipr_pkt_num_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_pkt_num_tbl_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipr_pkt_num_tbl_reg;
	return ret;
}

sw_error_t
hppe_ipr_pkt_num_tbl_reg_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_pkt_num_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_pkt_num_tbl_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipr_pkt_num_tbl_reg = value;
	ret = hppe_ipr_pkt_num_tbl_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_ipr_byte_low_reg_reg_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ipr_byte_low_reg_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_low_reg_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipr_byte_low_reg_reg;
	return ret;
}

sw_error_t
hppe_ipr_byte_low_reg_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_byte_low_reg_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_low_reg_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipr_byte_low_reg_reg = value;
	ret = hppe_ipr_byte_low_reg_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_ipr_byte_high_reg_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ipr_byte_high_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_high_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipr_byte_high_reg;
	return ret;
}

sw_error_t
hppe_ipr_byte_high_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_byte_high_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_high_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipr_byte_high_reg = value;
	ret = hppe_ipr_byte_high_reg_set(dev_id, index, &reg_val);
	return ret;
}

#else
sw_error_t
hppe_ipr_pkt_num_tbl_reg_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_pkt_num_tbl_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_pkt_num_tbl_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.packets = value;
	ret = hppe_ipr_pkt_num_tbl_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_ipr_byte_low_reg_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_byte_low_reg_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_low_reg_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bytes = value;
	ret = hppe_ipr_byte_low_reg_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_ipr_byte_high_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_byte_high_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_high_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bytes = value;
	ret = hppe_ipr_byte_high_reg_set(dev_id, index, &reg_val);
	return ret;
}
#endif
sw_error_t
appe_link_oam_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value)
{
	if (index >= LINK_OAM_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + LINK_OAM_CTRL_ADDRESS + \
				index * LINK_OAM_CTRL_INC,
				&value->val);
}

sw_error_t
appe_link_oam_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + LINK_OAM_CTRL_ADDRESS + \
				index * LINK_OAM_CTRL_INC,
				value->val);
}

sw_error_t
appe_link_oam_ctrl_loopback_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union link_oam_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_link_oam_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.loopback_state;
	return ret;
}

sw_error_t
appe_link_oam_ctrl_loopback_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union link_oam_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_link_oam_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.loopback_state = value;
	ret = appe_link_oam_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

#ifdef JHPPE
sw_error_t
hppe_ipr_byte_high_reg_clear_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ipr_byte_high_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_high_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.clear;
	return ret;
}

sw_error_t
hppe_ipr_byte_high_reg_clear_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ipr_byte_high_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ipr_byte_high_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.clear = value;
	ret = hppe_ipr_byte_high_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_default_pcp_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.default_pcp_dei;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_default_pcp_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.default_pcp_dei = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_qos_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dscp_qos_group_id;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_qos_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dscp_qos_group_id = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dscp_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dscp_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_force_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dscp_res_prec_force;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_force_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dscp_res_prec_force = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_flow_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.flow_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_flow_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.flow_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_qos_group_id;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_qos_group_id = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_qos_mode;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_qos_mode = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_force_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_res_prec_force;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_force_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_res_prec_force = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_post_acl_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.post_acl_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_post_acl_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.post_acl_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_acl_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pre_acl_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_acl_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pre_acl_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_inner_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pre_ipo_inner_res_prec_1 << 1 | \
		reg_val.bf.pre_ipo_inner_res_prec_0;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_inner_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pre_ipo_inner_res_prec_1 = value >> 1;
	reg_val.bf.pre_ipo_inner_res_prec_0 = value & (((a_uint64_t)1<<1)-1);
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_outer_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pre_ipo_outer_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_outer_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pre_ipo_outer_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_preheader_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.preheader_res_prec;
	return ret;
}

sw_error_t
hppe_mru_mtu_ctrl_tbl_preheader_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mru_mtu_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mru_mtu_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.preheader_res_prec = value;
	ret = hppe_mru_mtu_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

#endif
sw_error_t
hppe_mac_dbg_ctrl_high_ipg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.high_ipg;
	return ret;
}

sw_error_t
hppe_mac_dbg_ctrl_high_ipg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_dbg_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_dbg_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.high_ipg = value;
	ret = hppe_mac_dbg_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

