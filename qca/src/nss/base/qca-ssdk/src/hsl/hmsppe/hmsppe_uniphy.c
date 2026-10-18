/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw.h"
#include "hsl.h"
#include "hppe_reg_access.h"
#include "hmsppe_uniphy_reg.h"
#include "hmsppe_uniphy.h"
#include "hsl_dev.h"

/*
 * ========================================
 * UNIPHY0 (QP_UNIPHY) Functions
 * ========================================
 */

sw_error_t
hmsppe_uniphy_qp_usxg_reset_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union qp_usxg_reset_u *value)
{
    if (index >= QP_USXG_RESET_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + QP_USXG_RESET_ADDRESS,
                index * QP_USXG_RESET_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union qp_usxg_reset_u *value)
{
    if (index >= QP_USXG_RESET_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + QP_USXG_RESET_ADDRESS,
                index * QP_USXG_RESET_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_xpcs_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_xpcs_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_xpcs_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_xpcs_enable = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_qsgmii_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_qsgmii_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_qsgmii_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_qsgmii_enable = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_pqsgmii_func_rst_n_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_pqsgmii_func_rst_n;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_pqsgmii_func_rst_n_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_pqsgmii_func_rst_n = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_p3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_usxg_func_rst_n_p3;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_p3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_usxg_func_rst_n_p3 = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_p2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_usxg_func_rst_n_p2;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_p2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_usxg_func_rst_n_p2 = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_p1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_usxg_func_rst_n_p1;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_p1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_usxg_func_rst_n_p1 = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_usxg_func_rst_n;
    return ret;
}

sw_error_t
hmsppe_uniphy_qp_usxg_reset_mmd1_reg_usxg_func_rst_n_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union qp_usxg_reset_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_qp_usxg_reset_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_usxg_func_rst_n = value;
    ret = hmsppe_uniphy_qp_usxg_reset_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * ========================================
 * UNIPHY1 Functions
 * ========================================
 */

sw_error_t
hmsppe_uniphy_clock_gate_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union clock_gate_enable_u *value)
{
    if (index >= CLOCK_GATE_ENABLE_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + CLOCK_GATE_ENABLE_ADDRESS,
                index * CLOCK_GATE_ENABLE_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_clock_gate_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union clock_gate_enable_u *value)
{
    if (index >= CLOCK_GATE_ENABLE_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + CLOCK_GATE_ENABLE_ADDRESS,
                index * CLOCK_GATE_ENABLE_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_clock_gate_enable_mmd1_reg_xpcs_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union clock_gate_enable_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_clock_gate_enable_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_xpcs_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_clock_gate_enable_mmd1_reg_xpcs_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union clock_gate_enable_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_clock_gate_enable_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_xpcs_enable = value;
    ret = hmsppe_uniphy_clock_gate_enable_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_clock_gate_enable_mmd1_reg_qsgmii_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union clock_gate_enable_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_clock_gate_enable_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_qsgmii_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_clock_gate_enable_mmd1_reg_qsgmii_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union clock_gate_enable_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_clock_gate_enable_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_qsgmii_enable = value;
    ret = hmsppe_uniphy_clock_gate_enable_set(dev_id, index, &reg_val);
    return ret;
}

/*
 * ========================================
 * UNIPHY2 (PON_UNIPHY) Functions
 * ========================================
 */

/* PON_SMCBC_CTRL - 0x59C */
sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_smcbc_ctrl_u *value)
{
    if (index >= PON_SMCBC_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_SMCBC_CTRL_ADDRESS,
                index * PON_SMCBC_CTRL_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_smcbc_ctrl_u *value)
{
    if (index >= PON_SMCBC_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_SMCBC_CTRL_ADDRESS,
                index * PON_SMCBC_CTRL_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_pon_sdrx_smcbc_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_async_sw_clk_en_pon_sdrx_smcbc;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_pon_sdrx_smcbc_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_async_sw_clk_en_pon_sdrx_smcbc = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_uphy_pon_rx_sw_rstn_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_uphy_pon_rx_sw_rstn;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_uphy_pon_rx_sw_rstn_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_uphy_pon_rx_sw_rstn = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_uphy_pon_tx_sw_rstn_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_uphy_pon_tx_sw_rstn;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_uphy_pon_tx_sw_rstn_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_uphy_pon_tx_sw_rstn = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_pon_sdtx_smcbc_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_async_sw_clk_en_pon_sdtx_smcbc;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_pon_sdtx_smcbc_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_async_sw_clk_en_pon_sdtx_smcbc = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_gpon_smcbc_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_async_sw_clk_en_gpon_smcbc;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_gpon_smcbc_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_async_sw_clk_en_gpon_smcbc = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_xgpon_smcbc_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_async_sw_clk_en_xgpon_smcbc;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_sw_clk_en_xgpon_smcbc_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_async_sw_clk_en_xgpon_smcbc = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_clk_dis_override_smcbc_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_async_clk_dis_override_smcbc;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_smcbc_ctrl_mmd1_reg_async_clk_dis_override_smcbc_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_smcbc_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_smcbc_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_async_clk_dis_override_smcbc = value;
    ret = hmsppe_uniphy_pon_smcbc_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

/* FUNC_RST_PON - 0x5B8 */
sw_error_t
hmsppe_uniphy_func_rst_pon_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union func_rst_pon_u *value)
{
    if (index >= FUNC_RST_PON_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + FUNC_RST_PON_ADDRESS,
                index * FUNC_RST_PON_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_func_rst_pon_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union func_rst_pon_u *value)
{
    if (index >= FUNC_RST_PON_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + FUNC_RST_PON_ADDRESS,
                index * FUNC_RST_PON_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_func_mbist_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_func_mbist_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_func_mbist_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_func_mbist_sel = value;
    ret = hmsppe_uniphy_func_rst_pon_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_xpcs_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_xpcs_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_xpcs_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_xpcs_enable = value;
    ret = hmsppe_uniphy_func_rst_pon_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_qsgmii_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_qsgmii_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_qsgmii_enable_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_qsgmii_enable = value;
    ret = hmsppe_uniphy_func_rst_pon_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_func_rst_pon_rx_n_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_func_rst_pon_rx_n;
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_func_rst_pon_rx_n_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_func_rst_pon_rx_n = value;
    ret = hmsppe_uniphy_func_rst_pon_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_func_rst_pon_tx_n_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_func_rst_pon_tx_n;
    return ret;
}

sw_error_t
hmsppe_uniphy_func_rst_pon_mmd1_reg_func_rst_pon_tx_n_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union func_rst_pon_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_func_rst_pon_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_func_rst_pon_tx_n = value;
    ret = hmsppe_uniphy_func_rst_pon_set(dev_id, index, &reg_val);
    return ret;
}

/* PON_PLL_BANDWIDTH_CTRL - 0x5F0 */
sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_pll_bandwidth_ctrl_u *value)
{
    if (index >= PON_PLL_BANDWIDTH_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_PLL_BANDWIDTH_CTRL_ADDRESS,
                index * PON_PLL_BANDWIDTH_CTRL_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_pll_bandwidth_ctrl_u *value)
{
    if (index >= PON_PLL_BANDWIDTH_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_PLL_BANDWIDTH_CTRL_ADDRESS,
                index * PON_PLL_BANDWIDTH_CTRL_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_mmd1_reg_reg_pon_pll_cp_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_bandwidth_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_cp_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_mmd1_reg_reg_pon_pll_cp_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_bandwidth_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_cp_sel = value;
    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_mmd1_reg_reg_pon_pll_lpf_c2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_bandwidth_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_lpf_c2;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_mmd1_reg_reg_pon_pll_lpf_c2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_bandwidth_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_lpf_c2 = value;
    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_mmd1_reg_reg_pon_pll_lpf_res_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_bandwidth_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_lpf_res;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_bandwidth_ctrl_mmd1_reg_reg_pon_pll_lpf_res_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_bandwidth_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_lpf_res = value;
    ret = hmsppe_uniphy_pon_pll_bandwidth_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

/* PON_PLL_VCO_CTRL - 0x5F4 */
sw_error_t
hmsppe_uniphy_pon_pll_vco_ctrl_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_pll_vco_ctrl_u *value)
{
    if (index >= PON_PLL_VCO_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_PLL_VCO_CTRL_ADDRESS,
                index * PON_PLL_VCO_CTRL_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_pll_vco_ctrl_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_pll_vco_ctrl_u *value)
{
    if (index >= PON_PLL_VCO_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_PLL_VCO_CTRL_ADDRESS,
                index * PON_PLL_VCO_CTRL_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_pll_vco_ctrl_mmd1_reg_reg_pon_pll_vco_amp_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_vco_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_vco_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_vco_amp;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_vco_ctrl_mmd1_reg_reg_pon_pll_vco_amp_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_vco_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_vco_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_vco_amp = value;
    ret = hmsppe_uniphy_pon_pll_vco_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_vco_ctrl_mmd1_reg_reg_pon_pll_vco_temp_cmp_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_vco_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_vco_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_vco_temp_cmp;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_vco_ctrl_mmd1_reg_reg_pon_pll_vco_temp_cmp_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_vco_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_vco_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_vco_temp_cmp = value;
    ret = hmsppe_uniphy_pon_pll_vco_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

/* PON_PLL_REFCLK_FBCLK - 0x5F8 */
sw_error_t
hmsppe_uniphy_pon_pll_refclk_fbclk_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_pll_refclk_fbclk_u *value)
{
    if (index >= PON_PLL_REFCLK_FBCLK_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_PLL_REFCLK_FBCLK_ADDRESS,
                index * PON_PLL_REFCLK_FBCLK_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_pll_refclk_fbclk_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_pll_refclk_fbclk_u *value)
{
    if (index >= PON_PLL_REFCLK_FBCLK_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_PLL_REFCLK_FBCLK_ADDRESS,
                index * PON_PLL_REFCLK_FBCLK_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_pll_refclk_fbclk_mmd1_reg_reg_pon_pll_refclk_div_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_refclk_fbclk_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_refclk_fbclk_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_refclk_div;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_refclk_fbclk_mmd1_reg_reg_pon_pll_refclk_div_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_refclk_fbclk_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_refclk_fbclk_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_refclk_div = value;
    ret = hmsppe_uniphy_pon_pll_refclk_fbclk_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_refclk_fbclk_mmd1_reg_reg_pon_pll_fbclk_div_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_pll_refclk_fbclk_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_refclk_fbclk_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_pll_fbclk_div;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_pll_refclk_fbclk_mmd1_reg_reg_pon_pll_fbclk_div_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_pll_refclk_fbclk_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_pll_refclk_fbclk_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_pll_fbclk_div = value;
    ret = hmsppe_uniphy_pon_pll_refclk_fbclk_set(dev_id, index, &reg_val);
    return ret;
}

/* PON_REG_SOURCE_SEL1 - 0x5FC */
sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_reg_source_sel1_u *value)
{
    if (index >= PON_REG_SOURCE_SEL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_REG_SOURCE_SEL1_ADDRESS,
                index * PON_REG_SOURCE_SEL1_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_reg_source_sel1_u *value)
{
    if (index >= PON_REG_SOURCE_SEL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_REG_SOURCE_SEL1_ADDRESS,
                index * PON_REG_SOURCE_SEL1_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_mode_ext_internal_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_src_pon_mode_ext_internal;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_mode_ext_internal_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_src_pon_mode_ext_internal = value;
    ret = hmsppe_uniphy_pon_reg_source_sel1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_cp_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_src_pon_pll_cp_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_cp_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_src_pon_pll_cp_sel = value;
    ret = hmsppe_uniphy_pon_reg_source_sel1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_lpf_c2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_src_pon_pll_lpf_c2;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_lpf_c2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_src_pon_pll_lpf_c2 = value;
    ret = hmsppe_uniphy_pon_reg_source_sel1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_lpf_res_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_src_pon_pll_lpf_res;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_lpf_res_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_src_pon_pll_lpf_res = value;
    ret = hmsppe_uniphy_pon_reg_source_sel1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_vco_amp_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_src_pon_pll_vco_amp;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_vco_amp_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_src_pon_pll_vco_amp = value;
    ret = hmsppe_uniphy_pon_reg_source_sel1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_vco_temp_cmp_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_src_pon_pll_vco_temp_cmp;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_reg_source_sel1_mmd1_reg_src_pon_pll_vco_temp_cmp_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_reg_source_sel1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_reg_source_sel1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_src_pon_pll_vco_temp_cmp = value;
    ret = hmsppe_uniphy_pon_reg_source_sel1_set(dev_id, index, &reg_val);
    return ret;
}

/* PON_CDR_LOCK_WAIT_CNT - 0x610 */
sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_cdr_lock_wait_cnt_u *value)
{
    if (index >= PON_CDR_LOCK_WAIT_CNT_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_CDR_LOCK_WAIT_CNT_ADDRESS,
                index * PON_CDR_LOCK_WAIT_CNT_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_cdr_lock_wait_cnt_u *value)
{
    if (index >= PON_CDR_LOCK_WAIT_CNT_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_CDR_LOCK_WAIT_CNT_ADDRESS,
                index * PON_CDR_LOCK_WAIT_CNT_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_mmd1_reg_reg_pon_cdr_lock_wait_cnt_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_cdr_lock_wait_cnt_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_cdr_lock_wait_cnt_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_cdr_lock_wait_cnt;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_mmd1_reg_reg_pon_cdr_lock_wait_cnt_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_cdr_lock_wait_cnt_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_cdr_lock_wait_cnt_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_cdr_lock_wait_cnt = value;
    ret = hmsppe_uniphy_pon_cdr_lock_wait_cnt_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_CDR_CTRL - 0x640 */
sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_cdr_ctrl_u *value)
{
    if (index >= JCCDR_CDR_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_CDR_CTRL_ADDRESS,
                index * JCCDR_CDR_CTRL_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_cdr_ctrl_u *value)
{
    if (index >= JCCDR_CDR_CTRL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_CDR_CTRL_ADDRESS,
                index * JCCDR_CDR_CTRL_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_floop_gain_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_floop_gain;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_floop_gain_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_floop_gain = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_mmd1_uphy_rx_floop_byps_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_mmd1_uphy_rx_floop_byps;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_mmd1_uphy_rx_floop_byps_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_mmd1_uphy_rx_floop_byps = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_jccdr_rx_floop_byps_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_rx_floop_byps;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_jccdr_rx_floop_byps_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_rx_floop_byps = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_jccdr_rx_ploop_en_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_rx_ploop_en;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_jccdr_rx_ploop_en_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_rx_ploop_en = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_speed_mode_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_rx_speed_mode;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_speed_mode_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_rx_speed_mode = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_ploop_gain_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_rx_ploop_gain;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_ploop_gain_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_rx_ploop_gain = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_dlf_rate_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_rx_dlf_rate;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_dlf_rate_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_rx_dlf_rate = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_cdr_en_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_rx_cdr_en;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_cdr_ctrl_mmd1_reg_reg_jccdr_rx_cdr_en_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_cdr_ctrl_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_cdr_ctrl_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_rx_cdr_en = value;
    ret = hmsppe_uniphy_jccdr_cdr_ctrl_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_DIG_CTRL1 - 0x644 */
sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_dig_ctrl1_u *value)
{
    if (index >= JCCDR_DIG_CTRL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_DIG_CTRL1_ADDRESS,
                index * JCCDR_DIG_CTRL1_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_dig_ctrl1_u *value)
{
    if (index >= JCCDR_DIG_CTRL1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_DIG_CTRL1_ADDRESS,
                index * JCCDR_DIG_CTRL1_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_vco_done_wait_smpl_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_pon_vco_done_wait_smpl;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_vco_done_wait_smpl_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_pon_vco_done_wait_smpl = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_regular_lock_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_regular_lock;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_regular_lock_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_regular_lock = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_cdr_1st_wait_pon_vco_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_cdr_1st_wait_pon_vco;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_cdr_1st_wait_pon_vco_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_cdr_1st_wait_pon_vco = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_bypass_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_bypass;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_bypass_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_bypass = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_pll_bypass_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_pon_pll_bypass;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_pll_bypass_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_pon_pll_bypass = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_rxclk_floop_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_rxclk_floop_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_rxclk_floop_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_rxclk_floop_sel = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_rxclk_floop_sw_rstn_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_rxclk_floop_sw_rstn;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_jccdr_rxclk_floop_sw_rstn_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_rxclk_floop_sw_rstn = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_sa_calib_byps_2nd_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_sa_calib_byps_2nd;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_sa_calib_byps_2nd_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_sa_calib_byps_2nd = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_calib_clk_div_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_pon_calib_clk_div;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_calib_clk_div_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_pon_calib_clk_div = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_vco_calib_clk_div_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_pon_vco_calib_clk_div;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_dig_ctrl1_mmd1_reg_pon_vco_calib_clk_div_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_dig_ctrl1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_dig_ctrl1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_pon_vco_calib_clk_div = value;
    ret = hmsppe_uniphy_jccdr_dig_ctrl1_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_CALIB_STS_DEBUG - 0x688 */
sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_calib_sts_debug_u *value)
{
    if (index >= JCCDR_CALIB_STS_DEBUG_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_CALIB_STS_DEBUG_ADDRESS,
                index * JCCDR_CALIB_STS_DEBUG_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_jccdr_stg_state_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_stg_state;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_sa_calib_done_all_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_sa_calib_done_all;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_sdrx_lock_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_sdrx_lock;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_cdr_1st_locked_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_cdr_1st_locked;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_cdr_1st_enable_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_cdr_1st_enable;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_smpl_cal_ready_2nd_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_smpl_cal_ready_2nd;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_jccdr_rx_cal_out_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_rx_cal_out;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_calibration_done_uphypll_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_calibration_done_uphypll;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_calibration_done_jcpll_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_calibration_done_jcpll;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_calib_sts_debug_mmd1_reg_jc_pll_locked_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_calib_sts_debug_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_calib_sts_debug_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jc_pll_locked;
    return ret;
}

/* PON_CDR_LOCK_WAIT_CNT_EXTEND - 0x6A8 */
sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_cdr_lock_wait_cnt_extend_u *value)
{
    if (index >= PON_CDR_LOCK_WAIT_CNT_EXTEND_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_CDR_LOCK_WAIT_CNT_EXTEND_ADDRESS,
                index * PON_CDR_LOCK_WAIT_CNT_EXTEND_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union pon_cdr_lock_wait_cnt_extend_u *value)
{
    if (index >= PON_CDR_LOCK_WAIT_CNT_EXTEND_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + PON_CDR_LOCK_WAIT_CNT_EXTEND_ADDRESS,
                index * PON_CDR_LOCK_WAIT_CNT_EXTEND_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_mmd1_reg_reg_pon_cdr_lock_wait_cnt_extend_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union pon_cdr_lock_wait_cnt_extend_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_pon_cdr_lock_wait_cnt_extend;
    return ret;
}

sw_error_t
hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_mmd1_reg_reg_pon_cdr_lock_wait_cnt_extend_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union pon_cdr_lock_wait_cnt_extend_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_pon_cdr_lock_wait_cnt_extend = value;
    ret = hmsppe_uniphy_pon_cdr_lock_wait_cnt_extend_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_STG1_LOW - 0x6B0 */
sw_error_t
hmsppe_uniphy_jccdr_stg1_low_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_stg1_low_u *value)
{
    if (index >= JCCDR_STG1_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_STG1_LOW_ADDRESS,
                index * JCCDR_STG1_LOW_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_stg1_low_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_stg1_low_u *value)
{
    if (index >= JCCDR_STG1_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_STG1_LOW_ADDRESS,
                index * JCCDR_STG1_LOW_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_stg1_low_mmd1_reg_reg_jccdr_stg1_15_0_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_stg1_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_stg1_low_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_stg1_15_0;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_stg1_low_mmd1_reg_reg_jccdr_stg1_15_0_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_stg1_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_stg1_low_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_stg1_15_0 = value;
    ret = hmsppe_uniphy_jccdr_stg1_low_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_STG2_LOW - 0x6B8 */
sw_error_t
hmsppe_uniphy_jccdr_stg2_low_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_stg2_low_u *value)
{
    if (index >= JCCDR_STG2_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_STG2_LOW_ADDRESS,
                index * JCCDR_STG2_LOW_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_stg2_low_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_stg2_low_u *value)
{
    if (index >= JCCDR_STG2_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_STG2_LOW_ADDRESS,
                index * JCCDR_STG2_LOW_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_stg2_low_mmd1_reg_reg_jccdr_stg2_15_0_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_stg2_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_stg2_low_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_stg2_15_0;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_stg2_low_mmd1_reg_reg_jccdr_stg2_15_0_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_stg2_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_stg2_low_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_stg2_15_0 = value;
    ret = hmsppe_uniphy_jccdr_stg2_low_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_STG3_LOW - 0x6C0 */
sw_error_t
hmsppe_uniphy_jccdr_stg3_low_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_stg3_low_u *value)
{
    if (index >= JCCDR_STG3_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_STG3_LOW_ADDRESS,
                index * JCCDR_STG3_LOW_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_stg3_low_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_stg3_low_u *value)
{
    if (index >= JCCDR_STG3_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_STG3_LOW_ADDRESS,
                index * JCCDR_STG3_LOW_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_stg3_low_mmd1_reg_reg_jccdr_stg3_15_0_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_stg3_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_stg3_low_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_stg3_15_0;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_stg3_low_mmd1_reg_reg_jccdr_stg3_15_0_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_stg3_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_stg3_low_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_stg3_15_0 = value;
    ret = hmsppe_uniphy_jccdr_stg3_low_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_TUNE_CNT1_LOW - 0x6DC */
sw_error_t
hmsppe_uniphy_jccdr_tune_cnt1_low_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_tune_cnt1_low_u *value)
{
    if (index >= JCCDR_TUNE_CNT1_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_TUNE_CNT1_LOW_ADDRESS,
                index * JCCDR_TUNE_CNT1_LOW_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_tune_cnt1_low_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_tune_cnt1_low_u *value)
{
    if (index >= JCCDR_TUNE_CNT1_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_TUNE_CNT1_LOW_ADDRESS,
                index * JCCDR_TUNE_CNT1_LOW_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_tune_cnt1_low_mmd1_reg_reg_jccdr_tune_cnt1_15_0_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_tune_cnt1_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_tune_cnt1_low_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_tune_cnt1_15_0;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_tune_cnt1_low_mmd1_reg_reg_jccdr_tune_cnt1_15_0_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_tune_cnt1_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_tune_cnt1_low_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_tune_cnt1_15_0 = value;
    ret = hmsppe_uniphy_jccdr_tune_cnt1_low_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_TUNE_CNT2_LOW - 0x6E4 */
sw_error_t
hmsppe_uniphy_jccdr_tune_cnt2_low_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_tune_cnt2_low_u *value)
{
    if (index >= JCCDR_TUNE_CNT2_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_TUNE_CNT2_LOW_ADDRESS,
                index * JCCDR_TUNE_CNT2_LOW_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_tune_cnt2_low_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_tune_cnt2_low_u *value)
{
    if (index >= JCCDR_TUNE_CNT2_LOW_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_TUNE_CNT2_LOW_ADDRESS,
                index * JCCDR_TUNE_CNT2_LOW_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_tune_cnt2_low_mmd1_reg_reg_jccdr_tune_cnt2_15_0_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_tune_cnt2_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_tune_cnt2_low_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_tune_cnt2_15_0;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_tune_cnt2_low_mmd1_reg_reg_jccdr_tune_cnt2_15_0_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_tune_cnt2_low_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_tune_cnt2_low_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_tune_cnt2_15_0 = value;
    ret = hmsppe_uniphy_jccdr_tune_cnt2_low_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_FLOOP_GAIN_TUNE_VAL - 0x6EC */
sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_floop_gain_tune_val_u *value)
{
    if (index >= JCCDR_FLOOP_GAIN_TUNE_VAL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FLOOP_GAIN_TUNE_VAL_ADDRESS,
                index * JCCDR_FLOOP_GAIN_TUNE_VAL_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_floop_gain_tune_val_u *value)
{
    if (index >= JCCDR_FLOOP_GAIN_TUNE_VAL_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FLOOP_GAIN_TUNE_VAL_ADDRESS,
                index * JCCDR_FLOOP_GAIN_TUNE_VAL_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_reg_jccdr_floop_gain_tune2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_floop_gain_tune2;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_reg_jccdr_floop_gain_tune2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_floop_gain_tune2 = value;
    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_reg_jccdr_floop_gain_tune1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_floop_gain_tune1;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_reg_jccdr_floop_gain_tune1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_floop_gain_tune1 = value;
    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_reg_jccdr_floop_gain_tune0_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_jccdr_floop_gain_tune0;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_reg_jccdr_floop_gain_tune0_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_jccdr_floop_gain_tune0 = value;
    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_jccdr_floop_gain_tune_en_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_jccdr_floop_gain_tune_en;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_floop_gain_tune_val_mmd1_reg_jccdr_floop_gain_tune_en_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_floop_gain_tune_val_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_jccdr_floop_gain_tune_en = value;
    ret = hmsppe_uniphy_jccdr_floop_gain_tune_val_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_RGL_DIV_STG1_2 - 0x6F0 */
sw_error_t
hmsppe_uniphy_jccdr_rgl_div_stg1_2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_rgl_div_stg1_2_u *value)
{
    if (index >= JCCDR_RGL_DIV_STG1_2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_RGL_DIV_STG1_2_ADDRESS,
                index * JCCDR_RGL_DIV_STG1_2_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_div_stg1_2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_rgl_div_stg1_2_u *value)
{
    if (index >= JCCDR_RGL_DIV_STG1_2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_RGL_DIV_STG1_2_ADDRESS,
                index * JCCDR_RGL_DIV_STG1_2_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_div_stg1_2_mmd1_reg_reg_rgl_cdr_div_stg2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_rgl_div_stg1_2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_div_stg1_2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_rgl_cdr_div_stg2;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_div_stg1_2_mmd1_reg_reg_rgl_cdr_div_stg2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_rgl_div_stg1_2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_div_stg1_2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_rgl_cdr_div_stg2 = value;
    ret = hmsppe_uniphy_jccdr_rgl_div_stg1_2_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_div_stg1_2_mmd1_reg_reg_rgl_cdr_div_stg1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_rgl_div_stg1_2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_div_stg1_2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_rgl_cdr_div_stg1;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_div_stg1_2_mmd1_reg_reg_rgl_cdr_div_stg1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_rgl_div_stg1_2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_div_stg1_2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_rgl_cdr_div_stg1 = value;
    ret = hmsppe_uniphy_jccdr_rgl_div_stg1_2_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_FST_DIV_STG1_2_STEP1 - 0x6F4 */
sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_fst_div_stg1_2_step1_u *value)
{
    if (index >= JCCDR_FST_DIV_STG1_2_STEP1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FST_DIV_STG1_2_STEP1_ADDRESS,
                index * JCCDR_FST_DIV_STG1_2_STEP1_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_fst_div_stg1_2_step1_u *value)
{
    if (index >= JCCDR_FST_DIV_STG1_2_STEP1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FST_DIV_STG1_2_STEP1_ADDRESS,
                index * JCCDR_FST_DIV_STG1_2_STEP1_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_mmd1_reg_reg_fst_cdr_div_stg2_step1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_fst_div_stg1_2_step1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg2_step1;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_mmd1_reg_reg_fst_cdr_div_stg2_step1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_fst_div_stg1_2_step1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg2_step1 = value;
    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_mmd1_reg_reg_fst_cdr_div_stg1_step1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_fst_div_stg1_2_step1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg1_step1;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_mmd1_reg_reg_fst_cdr_div_stg1_step1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_fst_div_stg1_2_step1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg1_step1 = value;
    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step1_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_FST_DIV_STG1_2_STEP2 - 0x6F8 */
sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_fst_div_stg1_2_step2_u *value)
{
    if (index >= JCCDR_FST_DIV_STG1_2_STEP2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FST_DIV_STG1_2_STEP2_ADDRESS,
                index * JCCDR_FST_DIV_STG1_2_STEP2_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_fst_div_stg1_2_step2_u *value)
{
    if (index >= JCCDR_FST_DIV_STG1_2_STEP2_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FST_DIV_STG1_2_STEP2_ADDRESS,
                index * JCCDR_FST_DIV_STG1_2_STEP2_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_mmd1_reg_reg_fst_cdr_div_stg2_step2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_fst_div_stg1_2_step2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg2_step2;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_mmd1_reg_reg_fst_cdr_div_stg2_step2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_fst_div_stg1_2_step2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg2_step2 = value;
    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_mmd1_reg_reg_fst_cdr_div_stg1_step2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_fst_div_stg1_2_step2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg1_step2;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_mmd1_reg_reg_fst_cdr_div_stg1_step2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_fst_div_stg1_2_step2_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg1_step2 = value;
    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step2_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_FST_DIV_STG1_2_STEP3 - 0x6FC */
sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_fst_div_stg1_2_step3_u *value)
{
    if (index >= JCCDR_FST_DIV_STG1_2_STEP3_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FST_DIV_STG1_2_STEP3_ADDRESS,
                index * JCCDR_FST_DIV_STG1_2_STEP3_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_fst_div_stg1_2_step3_u *value)
{
    if (index >= JCCDR_FST_DIV_STG1_2_STEP3_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_FST_DIV_STG1_2_STEP3_ADDRESS,
                index * JCCDR_FST_DIV_STG1_2_STEP3_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_mmd1_reg_reg_fst_cdr_div_stg2_step3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_fst_div_stg1_2_step3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg2_step3;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_mmd1_reg_reg_fst_cdr_div_stg2_step3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_fst_div_stg1_2_step3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg2_step3 = value;
    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_mmd1_reg_reg_fst_cdr_div_stg1_step3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_fst_div_stg1_2_step3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg1_step3;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_mmd1_reg_reg_fst_cdr_div_stg1_step3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_fst_div_stg1_2_step3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg1_step3 = value;
    ret = hmsppe_uniphy_jccdr_fst_div_stg1_2_step3_set(dev_id, index, &reg_val);
    return ret;
}

/* JCCDR_RGL_FST_DIV_STG3 - 0x700 */
sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_rgl_fst_div_stg3_u *value)
{
    if (index >= JCCDR_RGL_FST_DIV_STG3_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_RGL_FST_DIV_STG3_ADDRESS,
                index * JCCDR_RGL_FST_DIV_STG3_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union jccdr_rgl_fst_div_stg3_u *value)
{
    if (index >= JCCDR_RGL_FST_DIV_STG3_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + JCCDR_RGL_FST_DIV_STG3_ADDRESS,
                index * JCCDR_RGL_FST_DIV_STG3_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_fst_cdr_div_stg3_step3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg3_step3;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_fst_cdr_div_stg3_step3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg3_step3 = value;
    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_fst_cdr_div_stg3_step2_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg3_step2;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_fst_cdr_div_stg3_step2_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg3_step2 = value;
    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_fst_cdr_div_stg3_step1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg3_step1;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_fst_cdr_div_stg3_step1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_fst_cdr_div_stg3_step1 = value;
    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_rgl_cdr_div_stg3_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    *value = reg_val.bf.mmd1_reg_reg_rgl_cdr_div_stg3;
    return ret;
}

sw_error_t
hmsppe_uniphy_jccdr_rgl_fst_div_stg3_mmd1_reg_reg_rgl_cdr_div_stg3_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union jccdr_rgl_fst_div_stg3_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.mmd1_reg_reg_rgl_cdr_div_stg3 = value;
    ret = hmsppe_uniphy_jccdr_rgl_fst_div_stg3_set(dev_id, index, &reg_val);
    return ret;
}

/* TX_CLK_GEN_1 - 0x7A0 */
sw_error_t
hmsppe_uniphy_tx_clk_gen_1_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union tx_clk_gen_1_u *value)
{
    if (index >= TX_CLK_GEN_1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_get(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + TX_CLK_GEN_1_ADDRESS,
                index * TX_CLK_GEN_1_INC,
                &value->val);
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union tx_clk_gen_1_u *value)
{
    if (index >= TX_CLK_GEN_1_NUM)
        return SW_OUT_OF_RANGE;

    return hppe_uniphy_reg_set(
                dev_id,
                NSS_UNIPHY_BASE_ADDR + TX_CLK_GEN_1_ADDRESS,
                index * TX_CLK_GEN_1_INC,
                value->val);
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txclk_source_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.miireg_uphy_txclk_source_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txclk_source_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.miireg_uphy_txclk_source_sel = value;
    ret = hmsppe_uniphy_tx_clk_gen_1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txclkgen_en_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.miireg_uphy_txclkgen_en;
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txclkgen_en_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.miireg_uphy_txclkgen_en = value;
    ret = hmsppe_uniphy_tx_clk_gen_1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txpi_en_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.miireg_uphy_txpi_en;
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txpi_en_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.miireg_uphy_txpi_en = value;
    ret = hmsppe_uniphy_tx_clk_gen_1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txpi_cur_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.miireg_uphy_txpi_cur_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txpi_cur_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.miireg_uphy_txpi_cur_sel = value;
    ret = hmsppe_uniphy_tx_clk_gen_1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txpi_slew_cur_sel_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.miireg_uphy_txpi_slew_cur_sel;
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_uphy_txpi_slew_cur_sel_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.miireg_uphy_txpi_slew_cur_sel = value;
    ret = hmsppe_uniphy_tx_clk_gen_1_set(dev_id, index, &reg_val);
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_reg_uphy_tx_speed_mode_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t *value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    *value = reg_val.bf.miireg_reg_uphy_tx_speed_mode;
    return ret;
}

sw_error_t
hmsppe_uniphy_tx_clk_gen_1_miireg_reg_uphy_tx_speed_mode_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        a_uint32_t value)
{
    union tx_clk_gen_1_u reg_val;
    sw_error_t ret = SW_OK;

    ret = hmsppe_uniphy_tx_clk_gen_1_get(dev_id, index, &reg_val);
    if (SW_OK != ret)
        return ret;
    reg_val.bf.miireg_reg_uphy_tx_speed_mode = value;
    ret = hmsppe_uniphy_tx_clk_gen_1_set(dev_id, index, &reg_val);
    return ret;
}
