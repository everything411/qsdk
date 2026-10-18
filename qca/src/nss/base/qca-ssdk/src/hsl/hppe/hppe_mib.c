/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_mac_mib_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_mib_ctrl_u *value)
{
	if (index >= MAC_MIB_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_MIB_CTRL_ADDRESS + \
				index * MAC_MIB_CTRL_INC,
				&value->val);
}

sw_error_t
hppe_mac_mib_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_mib_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + MAC_MIB_CTRL_ADDRESS + \
				index * MAC_MIB_CTRL_INC,
				value->val);
}

sw_error_t
hppe_rxbroad_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxbroad_u *value)
{
	if (index >= RXBROAD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXBROAD_ADDRESS + \
				index * RXBROAD_INC,
				&value->val);
}

sw_error_t
hppe_rxpause_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpause_u *value)
{
	if (index >= RXPAUSE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPAUSE_ADDRESS + \
				index * RXPAUSE_INC,
				&value->val);
}

sw_error_t
hppe_rxmulti_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxmulti_u *value)
{
	if (index >= RXMULTI_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXMULTI_ADDRESS + \
				index * RXMULTI_INC,
				&value->val);
}

sw_error_t
hppe_rxfcserr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxfcserr_u *value)
{
	if (index >= RXFCSERR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXFCSERR_ADDRESS + \
				index * RXFCSERR_INC,
				&value->val);
}

sw_error_t
hppe_rxalignerr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxalignerr_u *value)
{
	if (index >= RXALIGNERR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXALIGNERR_ADDRESS + \
				index * RXALIGNERR_INC,
				&value->val);
}

sw_error_t
hppe_rxrunt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxrunt_u *value)
{
	if (index >= RXRUNT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXRUNT_ADDRESS + \
				index * RXRUNT_INC,
				&value->val);
}

sw_error_t
hppe_rxfrag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxfrag_u *value)
{
	if (index >= RXFRAG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXFRAG_ADDRESS + \
				index * RXFRAG_INC,
				&value->val);
}

sw_error_t
hppe_rxjumbofcserr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxjumbofcserr_u *value)
{
	if (index >= RXJUMBOFCSERR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXJUMBOFCSERR_ADDRESS + \
				index * RXJUMBOFCSERR_INC,
				&value->val);
}

sw_error_t
hppe_rxjumboalignerr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxjumboalignerr_u *value)
{
	if (index >= RXJUMBOALIGNERR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXJUMBOALIGNERR_ADDRESS + \
				index * RXJUMBOALIGNERR_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt64_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt64_u *value)
{
	if (index >= RXPKT64_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT64_ADDRESS + \
				index * RXPKT64_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt65to127_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt65to127_u *value)
{
	if (index >= RXPKT65TO127_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT65TO127_ADDRESS + \
				index * RXPKT65TO127_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt128to255_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt128to255_u *value)
{
	if (index >= RXPKT128TO255_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT128TO255_ADDRESS + \
				index * RXPKT128TO255_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt256to511_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt256to511_u *value)
{
	if (index >= RXPKT256TO511_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT256TO511_ADDRESS + \
				index * RXPKT256TO511_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt512to1023_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt512to1023_u *value)
{
	if (index >= RXPKT512TO1023_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT512TO1023_ADDRESS + \
				index * RXPKT512TO1023_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt1024to1518_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt1024to1518_u *value)
{
	if (index >= RXPKT1024TO1518_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT1024TO1518_ADDRESS + \
				index * RXPKT1024TO1518_INC,
				&value->val);
}

sw_error_t
hppe_rxpkt1519tox_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxpkt1519tox_u *value)
{
	if (index >= RXPKT1519TOX_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXPKT1519TOX_ADDRESS + \
				index * RXPKT1519TOX_INC,
				&value->val);
}

sw_error_t
hppe_rxtoolong_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxtoolong_u *value)
{
	if (index >= RXTOOLONG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXTOOLONG_ADDRESS + \
				index * RXTOOLONG_INC,
				&value->val);
}

sw_error_t
hppe_rxgoodbyte_l_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxgoodbyte_l_u *value)
{
	if (index >= RXGOODBYTE_L_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXGOODBYTE_L_ADDRESS + \
				index * RXGOODBYTE_L_INC,
				&value->val);
}

sw_error_t
hppe_rxgoodbyte_h_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxgoodbyte_h_u *value)
{
	if (index >= RXGOODBYTE_H_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXGOODBYTE_H_ADDRESS + \
				index * RXGOODBYTE_H_INC,
				&value->val);
}

sw_error_t
hppe_rxbadbyte_l_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxbadbyte_l_u *value)
{
	if (index >= RXBADBYTE_L_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXBADBYTE_L_ADDRESS + \
				index * RXBADBYTE_L_INC,
				&value->val);
}

sw_error_t
hppe_rxbadbyte_h_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxbadbyte_h_u *value)
{
	if (index >= RXBADBYTE_H_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXBADBYTE_H_ADDRESS + \
				index * RXBADBYTE_H_INC,
				&value->val);
}

sw_error_t
hppe_rxuni_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rxuni_u *value)
{
	if (index >= RXUNI_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + RXUNI_ADDRESS + \
				index * RXUNI_INC,
				&value->val);
}

sw_error_t
hppe_txbroad_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txbroad_u *value)
{
	if (index >= TXBROAD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXBROAD_ADDRESS + \
				index * TXBROAD_INC,
				&value->val);
}

sw_error_t
hppe_txpause_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpause_u *value)
{
	if (index >= TXPAUSE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPAUSE_ADDRESS + \
				index * TXPAUSE_INC,
				&value->val);
}

sw_error_t
hppe_txmulti_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txmulti_u *value)
{
	if (index >= TXMULTI_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXMULTI_ADDRESS + \
				index * TXMULTI_INC,
				&value->val);
}

sw_error_t
hppe_txunderrun_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txunderrun_u *value)
{
	if (index >= TXUNDERRUN_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXUNDERRUN_ADDRESS + \
				index * TXUNDERRUN_INC,
				&value->val);
}

sw_error_t
hppe_txpkt64_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt64_u *value)
{
	if (index >= TXPKT64_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT64_ADDRESS + \
				index * TXPKT64_INC,
				&value->val);
}

sw_error_t
hppe_txpkt65to127_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt65to127_u *value)
{
	if (index >= TXPKT65TO127_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT65TO127_ADDRESS + \
				index * TXPKT65TO127_INC,
				&value->val);
}

sw_error_t
hppe_txpkt128to255_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt128to255_u *value)
{
	if (index >= TXPKT128TO255_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT128TO255_ADDRESS + \
				index * TXPKT128TO255_INC,
				&value->val);
}

sw_error_t
hppe_txpkt256to511_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt256to511_u *value)
{
	if (index >= TXPKT256TO511_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT256TO511_ADDRESS + \
				index * TXPKT256TO511_INC,
				&value->val);
}

sw_error_t
hppe_txpkt512to1023_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt512to1023_u *value)
{
	if (index >= TXPKT512TO1023_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT512TO1023_ADDRESS + \
				index * TXPKT512TO1023_INC,
				&value->val);
}

sw_error_t
hppe_txpkt1024to1518_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt1024to1518_u *value)
{
	if (index >= TXPKT1024TO1518_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT1024TO1518_ADDRESS + \
				index * TXPKT1024TO1518_INC,
				&value->val);
}

sw_error_t
hppe_txpkt1519tox_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txpkt1519tox_u *value)
{
	if (index >= TXPKT1519TOX_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXPKT1519TOX_ADDRESS + \
				index * TXPKT1519TOX_INC,
				&value->val);
}

sw_error_t
hppe_txbyte_l_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txbyte_l_u *value)
{
	if (index >= TXBYTE_L_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXBYTE_L_ADDRESS + \
				index * TXBYTE_L_INC,
				&value->val);
}

sw_error_t
hppe_txbyte_h_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txbyte_h_u *value)
{
	if (index >= TXBYTE_H_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXBYTE_H_ADDRESS + \
				index * TXBYTE_H_INC,
				&value->val);
}

sw_error_t
hppe_txcollisions_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txcollisions_u *value)
{
	if (index >= TXCOLLISIONS_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXCOLLISIONS_ADDRESS + \
				index * TXCOLLISIONS_INC,
				&value->val);
}

sw_error_t
hppe_txabortcol_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txabortcol_u *value)
{
	if (index >= TXABORTCOL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXABORTCOL_ADDRESS + \
				index * TXABORTCOL_INC,
				&value->val);
}

sw_error_t
hppe_txmulticol_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txmulticol_u *value)
{
	if (index >= TXMULTICOL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXMULTICOL_ADDRESS + \
				index * TXMULTICOL_INC,
				&value->val);
}

sw_error_t
hppe_txsinglecol_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txsinglecol_u *value)
{
	if (index >= TXSINGLECOL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXSINGLECOL_ADDRESS + \
				index * TXSINGLECOL_INC,
				&value->val);
}

sw_error_t
hppe_txexcessivedefer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txexcessivedefer_u *value)
{
	if (index >= TXEXCESSIVEDEFER_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXEXCESSIVEDEFER_ADDRESS + \
				index * TXEXCESSIVEDEFER_INC,
				&value->val);
}

sw_error_t
hppe_txdefer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txdefer_u *value)
{
	if (index >= TXDEFER_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXDEFER_ADDRESS + \
				index * TXDEFER_INC,
				&value->val);
}

sw_error_t
hppe_txlatecol_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txlatecol_u *value)
{
	if (index >= TXLATECOL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXLATECOL_ADDRESS + \
				index * TXLATECOL_INC,
				&value->val);
}

sw_error_t
hppe_txuni_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union txuni_u *value)
{
	if (index >= TXUNI_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_MAC_CSR_BASE_ADDR + TXUNI_ADDRESS + \
				index * TXUNI_INC,
				&value->val);
}

sw_error_t
hppe_mac_mib_ctrl_mib_reset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_mib_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_mib_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mib_reset;
	return ret;
}

sw_error_t
hppe_mac_mib_ctrl_mib_reset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_mib_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_mib_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mib_reset = value;
	ret = hppe_mac_mib_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_mib_ctrl_mib_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_mib_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_mib_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mib_en;
	return ret;
}

sw_error_t
hppe_mac_mib_ctrl_mib_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_mib_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_mib_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mib_en = value;
	ret = hppe_mac_mib_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mac_mib_ctrl_mib_rd_clr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mac_mib_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_mib_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mib_rd_clr;
	return ret;
}

sw_error_t
hppe_mac_mib_ctrl_mib_rd_clr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mac_mib_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mac_mib_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mib_rd_clr = value;
	ret = hppe_mac_mib_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

