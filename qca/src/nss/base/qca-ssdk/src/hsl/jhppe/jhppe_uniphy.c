/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hppe_reg_access.h"
#include "jhppe_uniphy_reg.h"
#include "jhppe_uniphy.h"
#include "hppe_init.h"

/*
 * PCS_UNIPHY_OPTION_3 Register Access Functions Implementation (Address: 0x5AC/0x588)
 */

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union pcs_uniphy_option_3_u *value)
{
    a_uint32_t reg;

    if (index >= PCS_UNIPHY_OPTION_3_NUM)
        return SW_OUT_OF_RANGE;

    reg = (index == 0) ? PCS0_UNIPHY_OPTION_3_ADDRESS: PCS_UNIPHY_OPTION_3_ADDRESS;
    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + reg,
        index * PCS_UNIPHY_OPTION_3_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union pcs_uniphy_option_3_u *value)
{
    a_uint32_t reg;

    if (index >= PCS_UNIPHY_OPTION_3_NUM)
        return SW_OUT_OF_RANGE;

    reg = (index == 0) ? PCS0_UNIPHY_OPTION_3_ADDRESS: PCS_UNIPHY_OPTION_3_ADDRESS;
    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + reg,
        index * PCS_UNIPHY_OPTION_3_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_up_digphy_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xlgpcs_up_digphy_rstn_sel;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_up_digphy_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xlgpcs_up_digphy_rstn_sel = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_up_digphy_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xpcs_up_digphy_rstn_sel;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_up_digphy_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xpcs_up_digphy_rstn_sel = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_hw_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xlgpcs_hw_rstn_sel;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_hw_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xlgpcs_hw_rstn_sel = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_hw_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xpcs_hw_rstn_sel;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_hw_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xpcs_hw_rstn_sel = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_uniphy_start_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.uniphy_start;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_uniphy_start_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.uniphy_start = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_energy_detect_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xlgpcs_energy_detect;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_energy_detect_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xlgpcs_energy_detect = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_lpi_clk_gat_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xlgpcs_lpi_clk_gat_en;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_lpi_clk_gat_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xlgpcs_lpi_clk_gat_en = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_lpi_clk_gat_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xpcs_lpi_clk_gat_en;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_lpi_clk_gat_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xpcs_lpi_clk_gat_en = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_rpcs_energy_det_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rpcs_energy_det;
    return ret;
}

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_rpcs_energy_det_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union pcs_uniphy_option_3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_pcs_uniphy_option_3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.rpcs_energy_det = value;
    ret = jhppe_uniphy_pcs_uniphy_option_3_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * QSERDES_RX_EXT_RXEQ_CTRL0 Register Access Functions Implementation (Address: 0xcb20)
 */

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_rxeq_ctrl0_u *value)
{
    if (index >= QSERDES_RX_EXT_RXEQ_CTRL0_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + QSERDES_RX_EXT_RXEQ_CTRL0_ADDRESS,
        index * QSERDES_RX_EXT_RXEQ_CTRL0_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_rxeq_ctrl0_u *value)
{
    if (index >= QSERDES_RX_EXT_RXEQ_CTRL0_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + QSERDES_RX_EXT_RXEQ_CTRL0_ADDRESS,
        index * QSERDES_RX_EXT_RXEQ_CTRL0_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mask_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xpcs_rxeq_en_mask;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mask_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xpcs_rxeq_en_mask = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xpcs_rxeq_en;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xpcs_rxeq_en = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mux_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.xpcs_rxeq_en_mux;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mux_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.xpcs_rxeq_en_mux = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_gx_early_training_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.gx_early_training_en;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_gx_early_training_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.gx_early_training_en = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_use_rxeqinphase0123_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.use_rxeqinphase0123;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_use_rxeqinphase0123_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.use_rxeqinphase0123 = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_phase3_start_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.dfe_reset_on_phase3_start_en;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_phase3_start_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.dfe_reset_on_phase3_start_en = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_retrain_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.dfe_reset_on_retrain_en;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_retrain_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.dfe_reset_on_retrain_en = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_rxeqeval_mask_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rxeqeval_mask_en;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_rxeqeval_mask_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_rxeq_ctrl0_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.rxeqeval_mask_en = value;
    ret = jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * QSERDES_RX_EXT_RO_POWER_STATE Register Access Functions Implementation (Address: 0xcdf4)
 */

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_ro_power_state_u *value)
{
    if (index >= QSERDES_RX_EXT_RO_POWER_STATE_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + QSERDES_RX_EXT_RO_POWER_STATE_ADDRESS,
        index * QSERDES_RX_EXT_RO_POWER_STATE_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_ro_power_state_u *value)
{
    if (index >= QSERDES_RX_EXT_RO_POWER_STATE_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + QSERDES_RX_EXT_RO_POWER_STATE_ADDRESS,
        index * QSERDES_RX_EXT_RO_POWER_STATE_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_power_state_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_power_state_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_power_state_get(dev_id, index, &reg_val);
    *value = reg_val.bf.power_state;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_power_state_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union qserdes_rx_ext_ro_power_state_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_power_state_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.power_state = value;
    ret = jhppe_uniphy_qserdes_rx_ext_ro_power_state_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS Register Access Functions Implementation (Address: 0xcdfc)
 */

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_ro_pmad_rxeq_status_u *value)
{
    if (index >= QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS,
        index * QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_timer_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.timer_done;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_ctl_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.ctl_done;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_vga_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.vga_done;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_dfe_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.dfe_done;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_tracking_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.tracking;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_rxeq_engine_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rxeq_engine_done;
    return ret;
}

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_pmad_rxeq_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union qserdes_rx_ext_ro_pmad_rxeq_status_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(dev_id, index, &reg_val);
    *value = reg_val.bf.pmad_rxeq_en;
    return ret;
}

/*
 * SR_PMA_CTRL2 Register Access Functions Implementation (Address: 0x10007)
 */

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_ctrl2_u *value)
{
    if (index >= SR_PMA_CTRL2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PMA_CTRL2_ADDRESS),
        index * SR_PMA_CTRL2_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_ctrl2_u *value)
{
    if (index >= SR_PMA_CTRL2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PMA_CTRL2_ADDRESS),
        index * SR_PMA_CTRL2_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pma_type_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_ctrl2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.pma_type;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pma_type_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_ctrl2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.pma_type = value;
    ret = jhppe_uniphy_sr_pma_ctrl2_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pease_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_ctrl2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.pease;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pease_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_ctrl2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.pease = value;
    ret = jhppe_uniphy_sr_pma_ctrl2_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_piase_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_ctrl2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.piase;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_piase_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_ctrl2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.piase = value;
    ret = jhppe_uniphy_sr_pma_ctrl2_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * SR_PMA_KR_FEC_CTRL Register Access Functions Implementation (Address: 0x100ab)
 */

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_kr_fec_ctrl_u *value)
{
    if (index >= SR_PMA_KR_FEC_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PMA_KR_FEC_CTRL_ADDRESS),
        index * SR_PMA_KR_FEC_CTRL_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_kr_fec_ctrl_u *value)
{
    if (index >= SR_PMA_KR_FEC_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PMA_KR_FEC_CTRL_ADDRESS),
        index * SR_PMA_KR_FEC_CTRL_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_fec_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_kr_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.fec_en;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_fec_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_kr_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.fec_en = value;
    ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_en_err_ind_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_kr_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.en_err_ind;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_en_err_ind_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_kr_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.en_err_ind = value;
    ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * SR_PMA_RS_FEC_CTRL Register Access Functions Implementation (Address: 0x100c8)
 */

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_rs_fec_ctrl_u *value)
{
    if (index >= SR_PMA_RS_FEC_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PMA_RS_FEC_CTRL_ADDRESS),
        index * SR_PMA_RS_FEC_CTRL_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_rs_fec_ctrl_u *value)
{
    if (index >= SR_PMA_RS_FEC_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PMA_RS_FEC_CTRL_ADDRESS),
        index * SR_PMA_RS_FEC_CTRL_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_cre_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.byp_cre;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_cre_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.byp_cre = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_ine_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.byp_ine;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_ine_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.byp_ine = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_rsfec_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rsfec_en;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_rsfec_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.rsfec_en = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_flp_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.flp;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_flp_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.flp = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_dser_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.dser_en;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_dser_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.dser_en = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl161_100g_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cl161_100g_en;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl161_100g_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cl161_100g_en = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl91_100g_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cl91_100g_en;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl91_100g_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pma_rs_fec_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cl91_100g_en = value;
    ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PMA_CWM00 Register Access Functions Implementation (Address: 0x180a3)
 */

sw_error_t
jhppe_uniphy_vr_pma_cwm00_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm00_u *value)
{
    if (index >= VR_PMA_CWM00_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM00_ADDRESS),
        index * VR_PMA_CWM00_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm00_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm00_u *value)
{
    if (index >= VR_PMA_CWM00_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM00_ADDRESS),
        index * VR_PMA_CWM00_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm00_cwm15_0_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pma_cwm00_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm00_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cwm15_0;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pma_cwm00_cwm15_0_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pma_cwm00_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm00_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cwm15_0 = value;
    ret = jhppe_uniphy_vr_pma_cwm00_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PMA_CWM01 Register Access Functions Implementation (Address: 0x180a4)
 */

sw_error_t
jhppe_uniphy_vr_pma_cwm01_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm01_u *value)
{
    if (index >= VR_PMA_CWM01_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM01_ADDRESS),
        index * VR_PMA_CWM01_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm01_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm01_u *value)
{
    if (index >= VR_PMA_CWM01_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM01_ADDRESS),
        index * VR_PMA_CWM01_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm01_cwm31_16_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pma_cwm01_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm01_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cwm31_16;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pma_cwm01_cwm31_16_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pma_cwm01_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm01_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cwm31_16 = value;
    ret = jhppe_uniphy_vr_pma_cwm01_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PMA_CWM02 Register Access Functions Implementation (Address: 0x180a5)
 */

sw_error_t
jhppe_uniphy_vr_pma_cwm02_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm02_u *value)
{
    if (index >= VR_PMA_CWM02_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM02_ADDRESS),
        index * VR_PMA_CWM02_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm02_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm02_u *value)
{
    if (index >= VR_PMA_CWM02_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM02_ADDRESS),
        index * VR_PMA_CWM02_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm02_cwm47_32_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pma_cwm02_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm02_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cwm47_32;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pma_cwm02_cwm47_32_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pma_cwm02_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm02_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cwm47_32 = value;
    ret = jhppe_uniphy_vr_pma_cwm02_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PMA_CWM03 Register Access Functions Implementation (Address: 0x180a6)
 */

sw_error_t
jhppe_uniphy_vr_pma_cwm03_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm03_u *value)
{
    if (index >= VR_PMA_CWM03_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM03_ADDRESS),
        index * VR_PMA_CWM03_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm03_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm03_u *value)
{
    if (index >= VR_PMA_CWM03_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PMA_CWM03_ADDRESS),
        index * VR_PMA_CWM03_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pma_cwm03_cwm63_48_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pma_cwm03_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm03_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cwm63_48;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pma_cwm03_cwm63_48_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pma_cwm03_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pma_cwm03_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cwm63_48 = value;
    ret = jhppe_uniphy_vr_pma_cwm03_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * SR_PCS_CTRL1 Register Access Functions Implementation (Address: 0x30000)
 */

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl1_u *value)
{
    if (index >= SR_PCS_CTRL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PCS_CTRL1_ADDRESS),
        index * SR_PCS_CTRL1_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl1_u *value)
{
    if (index >= SR_PCS_CTRL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PCS_CTRL1_ADDRESS),
        index * SR_PCS_CTRL1_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_rst_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rst;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_rst_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.rst = value;
    ret = jhppe_uniphy_sr_pcs_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_lpm_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.lpm;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_lpm_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.lpm = value;
    ret = jhppe_uniphy_sr_pcs_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_cs_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cs_en;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_cs_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cs_en = value;
    ret = jhppe_uniphy_sr_pcs_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss_5_2_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.ss_5_2;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss_5_2_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.ss_5_2 = value;
    ret = jhppe_uniphy_sr_pcs_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss6_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.ss6;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss6_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.ss6 = value;
    ret = jhppe_uniphy_sr_pcs_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss13_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.ss13;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss13_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.ss13 = value;
    ret = jhppe_uniphy_sr_pcs_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * SR_PCS_STS1 Register Access Functions Implementation (Address: 0x30001)
 */

sw_error_t
jhppe_uniphy_sr_pcs_sts1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_sts1_u *value)
{
    if (index >= SR_PCS_STS1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PCS_STS1_ADDRESS),
        index * SR_PCS_STS1_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_lpms_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.lpms;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_rlu_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rlu;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_csc_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.csc;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_flt_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.flt;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_rxlpii_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rxlpii;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_txlpii_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.txlpii;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_rxlpir_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rxlpir;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_sts1_txlpir_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_sts1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_sts1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.txlpir;
    return ret;
}

/*
 * SR_PCS_CTRL2 Register Access Functions Implementation (Address: 0x30007)
 */

sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl2_u *value)
{
    if (index >= SR_PCS_CTRL2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PCS_CTRL2_ADDRESS),
        index * SR_PCS_CTRL2_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl2_u *value)
{
    if (index >= SR_PCS_CTRL2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(SR_PCS_CTRL2_ADDRESS),
        index * SR_PCS_CTRL2_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_pcs_type_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union sr_pcs_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.pcs_type_sel;
    return ret;
}

sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_pcs_type_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union sr_pcs_ctrl2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_sr_pcs_ctrl2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.pcs_type_sel = value;
    ret = jhppe_uniphy_sr_pcs_ctrl2_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PCS_DIG_CTRL1 Register Access Functions Implementation (Address: 0x38000)
 */

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl1_u *value)
{
    if (index >= VR_PCS_DIG_CTRL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PCS_DIG_CTRL1_ADDRESS),
        index * VR_PCS_DIG_CTRL1_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl1_u *value)
{
    if (index >= VR_PCS_DIG_CTRL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PCS_DIG_CTRL1_ADDRESS),
        index * VR_PCS_DIG_CTRL1_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_vr_rst_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.vr_rst;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_vr_rst_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.vr_rst = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_r2tlbe_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.r2tlbe;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_r2tlbe_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.r2tlbe = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_en_vsmmd1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.en_vsmmd1;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_en_vsmmd1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.en_vsmmd1 = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_rx_init_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.rx_init;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_rx_init_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.rx_init = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_tx_init_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.tx_init;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_tx_init_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.tx_init = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_3_1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.dtxlaned_3_1;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_3_1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.dtxlaned_3_1 = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_0_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.dtxlaned_0;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_0_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.dtxlaned_0 = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_lane_ctrl_ext_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.lane_ctrl_ext;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_lane_ctrl_ext_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.lane_ctrl_ext = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PCS_DIG_CTRL3 Register Access Functions Implementation (Address: 0x38003)
 */

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl3_u *value)
{
    if (index >= VR_PCS_DIG_CTRL3_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PCS_DIG_CTRL3_ADDRESS),
        index * VR_PCS_DIG_CTRL3_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl3_u *value)
{
    if (index >= VR_PCS_DIG_CTRL3_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PCS_DIG_CTRL3_ADDRESS),
        index * VR_PCS_DIG_CTRL3_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cns_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cns_en;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cns_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cns_en = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_en_50g_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.en_50g;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_en_50g_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.en_50g = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cl72_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.cl72_en;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cl72_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.cl72_en = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_msk_phy_rst_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.msk_phy_rst;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_msk_phy_rst_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.msk_phy_rst = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_bmx_544_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.bmx_544;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_bmx_544_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.bmx_544 = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_gaui_544_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.gaui_544;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_gaui_544_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_dig_ctrl3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.gaui_544 = value;
    ret = jhppe_uniphy_vr_pcs_dig_ctrl3_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * VR_PCS_AM_CNT Register Access Functions Implementation (Address: 0x38018)
 */

sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_am_cnt_u *value)
{
    if (index >= VR_PCS_AM_CNT_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PCS_AM_CNT_ADDRESS),
        index * VR_PCS_AM_CNT_INC,
        &value->val);
}

sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_am_cnt_u *value)
{
    if (index >= VR_PCS_AM_CNT_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
        dev_id,
        NSS_UNIPHY_BASE_ADDR + JHPPE_CSR2_ADDR(VR_PCS_AM_CNT_ADDRESS),
        index * VR_PCS_AM_CNT_INC,
        value->val);
}

sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_pcs_am_cnt_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value)
{
    union vr_pcs_am_cnt_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_am_cnt_get(dev_id, index, &reg_val);
    *value = reg_val.bf.pcs_am_cnt;
    return ret;
}

sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_pcs_am_cnt_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value)
{
    union vr_pcs_am_cnt_u reg_val;
    sw_error_t ret = SW_OK;

    ret = jhppe_uniphy_vr_pcs_am_cnt_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.pcs_am_cnt = value;
    ret = jhppe_uniphy_vr_pcs_am_cnt_set(dev_id, index, &reg_val);
    return ret;
}
