/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_UNIPHY_H_
#define _JHPPE_UNIPHY_H_

/*
 * PCS_UNIPHY_OPTION_3 Register Access Functions (Address: 0x588)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union pcs_uniphy_option_3_u *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union pcs_uniphy_option_3_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_up_digphy_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_up_digphy_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_up_digphy_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_up_digphy_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_hw_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_hw_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_hw_rstn_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_hw_rstn_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_uniphy_start_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_uniphy_start_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_energy_detect_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_energy_detect_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_lpi_clk_gat_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xlgpcs_lpi_clk_gat_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_lpi_clk_gat_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_xpcs_lpi_clk_gat_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_rpcs_energy_det_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_pcs_uniphy_option_3_rpcs_energy_det_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * QSERDES_RX_EXT_RXEQ_CTRL0 Register Access Functions (Address: 0xcb20)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_rxeq_ctrl0_u *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_rxeq_ctrl0_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mask_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mask_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mux_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mux_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_gx_early_training_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_gx_early_training_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_use_rxeqinphase0123_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_use_rxeqinphase0123_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_phase3_start_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_phase3_start_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_retrain_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_dfe_reset_on_retrain_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_rxeqeval_mask_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_rxeqeval_mask_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * QSERDES_RX_EXT_RO_POWER_STATE Register Access Functions (Address: 0xcdf4)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_ro_power_state_u *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_ro_power_state_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_power_state_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_power_state_power_state_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS Register Access Functions (Address: 0xcdfc)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union qserdes_rx_ext_ro_pmad_rxeq_status_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_timer_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_ctl_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_vga_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_dfe_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_tracking_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_rxeq_engine_done_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_pmad_rxeq_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

/*
 * SR_PMA_CTRL2 Register Access Functions (Address: 0x10007)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_sr_pma_ctrl2_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_ctrl2_u *value);

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_ctrl2_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pma_type_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pma_type_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pease_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_pease_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_piase_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_ctrl2_piase_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * SR_PMA_KR_FEC_CTRL Register Access Functions (Address: 0x100ab)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_kr_fec_ctrl_u *value);

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_kr_fec_ctrl_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_fec_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_fec_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_en_err_ind_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_kr_fec_ctrl_en_err_ind_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * SR_PMA_RS_FEC_CTRL Register Access Functions (Address: 0x100c8)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_rs_fec_ctrl_u *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pma_rs_fec_ctrl_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_cre_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_cre_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_ine_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_byp_ine_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_rsfec_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_rsfec_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_flp_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_flp_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_dser_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_dser_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl161_100g_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl161_100g_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl91_100g_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pma_rs_fec_ctrl_cl91_100g_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PMA_CWM00 Register Access Functions (Address: 0x180a3)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm00_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm00_u *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm00_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm00_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm00_cwm15_0_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm00_cwm15_0_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PMA_CWM01 Register Access Functions (Address: 0x180a4)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm01_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm01_u *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm01_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm01_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm01_cwm31_16_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm01_cwm31_16_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PMA_CWM02 Register Access Functions (Address: 0x180a5)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm02_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm02_u *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm02_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm02_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm02_cwm47_32_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm02_cwm47_32_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PMA_CWM03 Register Access Functions (Address: 0x180a6)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm03_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm03_u *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm03_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pma_cwm03_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pma_cwm03_cwm63_48_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pma_cwm03_cwm63_48_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * SR_PCS_CTRL1 Register Access Functions (Address: 0x30000)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl1_u *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl1_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_rst_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_rst_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss_5_2_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss_5_2_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss6_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss6_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_cs_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_cs_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_lpm_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_lpm_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss13_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl1_ss13_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * SR_PCS_STS1 Register Access Functions (Address: 0x30001)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_sr_pcs_sts1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_sts1_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_sr_pcs_sts1_lpms_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_rlu_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_csc_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_flt_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_rxlpii_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_txlpii_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_rxlpir_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_sts1_txlpir_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

/*
 * SR_PCS_CTRL2 Register Access Functions (Address: 0x30007)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl2_u *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union sr_pcs_ctrl2_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_pcs_type_sel_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_sr_pcs_ctrl2_pcs_type_sel_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PCS_DIG_CTRL1 Register Access Functions (Address: 0x38000)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl1_u *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl1_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_vr_rst_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_vr_rst_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_r2tlbe_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_r2tlbe_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_en_vsmmd1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_en_vsmmd1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_rx_init_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_rx_init_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_tx_init_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_tx_init_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_3_1_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_3_1_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_0_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_dtxlaned_0_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_lane_ctrl_ext_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl1_lane_ctrl_ext_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PCS_DIG_CTRL3 Register Access Functions (Address: 0x38003)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl3_u *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_dig_ctrl3_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cns_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cns_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_en_50g_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_en_50g_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cl72_en_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_cl72_en_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_msk_phy_rst_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_msk_phy_rst_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_bmx_544_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_bmx_544_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_gaui_544_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_dig_ctrl3_gaui_544_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

/*
 * VR_PCS_AM_CNT Register Access Functions (Address: 0x38018)
 */

/* Register level access functions */
sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_am_cnt_u *value);

sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    union vr_pcs_am_cnt_u *value);

/* Bit field level access functions */
sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_pcs_am_cnt_get(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t *value);

sw_error_t
jhppe_uniphy_vr_pcs_am_cnt_pcs_am_cnt_set(
    a_uint32_t dev_id,
    a_uint32_t index,
    a_uint32_t value);

#endif /* _JHPPE_UNIPHY_H_ */
