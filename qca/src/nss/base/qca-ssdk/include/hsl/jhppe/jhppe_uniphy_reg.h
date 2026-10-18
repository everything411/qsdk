/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_UNIPHY_REG_H_
#define _JHPPE_UNIPHY_REG_H_

/*[register] PCS_UNIPHY_OPTION_3*/
#define PCS_UNIPHY_OPTION_3
#define PCS_UNIPHY_OPTION_3_ADDRESS 0x588
#define PCS0_UNIPHY_OPTION_3_ADDRESS 0x5AC
#define PCS_UNIPHY_OPTION_3_NUM     3
#define PCS_UNIPHY_OPTION_3_INC     0x1
#define PCS_UNIPHY_OPTION_3_TYPE    REG_TYPE_RW
#define PCS_UNIPHY_OPTION_3_DEFAULT 0x0
	/*[field] XLGPCS_UP_DIGPHY_RSTN_SEL*/
	#define PCS_UNIPHY_OPTION_3_XLGPCS_UP_DIGPHY_RSTN_SEL
	#define PCS_UNIPHY_OPTION_3_XLGPCS_UP_DIGPHY_RSTN_SEL_OFFSET  10
	#define PCS_UNIPHY_OPTION_3_XLGPCS_UP_DIGPHY_RSTN_SEL_LEN     1
	#define PCS_UNIPHY_OPTION_3_XLGPCS_UP_DIGPHY_RSTN_SEL_DEFAULT 0x0
	/*[field] XPCS_UP_DIGPHY_RSTN_SEL*/
	#define PCS_UNIPHY_OPTION_3_XPCS_UP_DIGPHY_RSTN_SEL
	#define PCS_UNIPHY_OPTION_3_XPCS_UP_DIGPHY_RSTN_SEL_OFFSET  9
	#define PCS_UNIPHY_OPTION_3_XPCS_UP_DIGPHY_RSTN_SEL_LEN     1
	#define PCS_UNIPHY_OPTION_3_XPCS_UP_DIGPHY_RSTN_SEL_DEFAULT 0x0
	/*[field] XLGPCS_HW_RSTN_SEL*/
	#define PCS_UNIPHY_OPTION_3_XLGPCS_HW_RSTN_SEL
	#define PCS_UNIPHY_OPTION_3_XLGPCS_HW_RSTN_SEL_OFFSET  8
	#define PCS_UNIPHY_OPTION_3_XLGPCS_HW_RSTN_SEL_LEN     1
	#define PCS_UNIPHY_OPTION_3_XLGPCS_HW_RSTN_SEL_DEFAULT 0x0
	/*[field] XPCS_HW_RSTN_SEL*/
	#define PCS_UNIPHY_OPTION_3_XPCS_HW_RSTN_SEL
	#define PCS_UNIPHY_OPTION_3_XPCS_HW_RSTN_SEL_OFFSET  7
	#define PCS_UNIPHY_OPTION_3_XPCS_HW_RSTN_SEL_LEN     1
	#define PCS_UNIPHY_OPTION_3_XPCS_HW_RSTN_SEL_DEFAULT 0x0
	/*[field] UNIPHY_START*/
	#define PCS_UNIPHY_OPTION_3_UNIPHY_START
	#define PCS_UNIPHY_OPTION_3_UNIPHY_START_OFFSET  4
	#define PCS_UNIPHY_OPTION_3_UNIPHY_START_LEN     1
	#define PCS_UNIPHY_OPTION_3_UNIPHY_START_DEFAULT 0x0
	/*[field] XLGPCS_ENERGY_DETECT*/
	#define PCS_UNIPHY_OPTION_3_XLGPCS_ENERGY_DETECT
	#define PCS_UNIPHY_OPTION_3_XLGPCS_ENERGY_DETECT_OFFSET  3
	#define PCS_UNIPHY_OPTION_3_XLGPCS_ENERGY_DETECT_LEN     1
	#define PCS_UNIPHY_OPTION_3_XLGPCS_ENERGY_DETECT_DEFAULT 0x0
	/*[field] XLGPCS_LPI_CLK_GAT_EN*/
	#define PCS_UNIPHY_OPTION_3_XLGPCS_LPI_CLK_GAT_EN
	#define PCS_UNIPHY_OPTION_3_XLGPCS_LPI_CLK_GAT_EN_OFFSET  2
	#define PCS_UNIPHY_OPTION_3_XLGPCS_LPI_CLK_GAT_EN_LEN     1
	#define PCS_UNIPHY_OPTION_3_XLGPCS_LPI_CLK_GAT_EN_DEFAULT 0x0
	/*[field] XPCS_LPI_CLK_GAT_EN*/
	#define PCS_UNIPHY_OPTION_3_XPCS_LPI_CLK_GAT_EN
	#define PCS_UNIPHY_OPTION_3_XPCS_LPI_CLK_GAT_EN_OFFSET  1
	#define PCS_UNIPHY_OPTION_3_XPCS_LPI_CLK_GAT_EN_LEN     1
	#define PCS_UNIPHY_OPTION_3_XPCS_LPI_CLK_GAT_EN_DEFAULT 0x0
	/*[field] RPCS_ENERGY_DET*/
	#define PCS_UNIPHY_OPTION_3_RPCS_ENERGY_DET
	#define PCS_UNIPHY_OPTION_3_RPCS_ENERGY_DET_OFFSET  0
	#define PCS_UNIPHY_OPTION_3_RPCS_ENERGY_DET_LEN     1
	#define PCS_UNIPHY_OPTION_3_RPCS_ENERGY_DET_DEFAULT 0x0

struct pcs_uniphy_option_3 {
	a_uint32_t  rpcs_energy_det:1;
	a_uint32_t  xpcs_lpi_clk_gat_en:1;
	a_uint32_t  xlgpcs_lpi_clk_gat_en:1;
	a_uint32_t  xlgpcs_energy_detect:1;
	a_uint32_t  uniphy_start:1;
	a_uint32_t  _reserved0:2;
	a_uint32_t  xpcs_hw_rstn_sel:1;
	a_uint32_t  xlgpcs_hw_rstn_sel:1;
	a_uint32_t  xpcs_up_digphy_rstn_sel:1;
	a_uint32_t  xlgpcs_up_digphy_rstn_sel:1;
	a_uint32_t  _reserved1:21;
};

union pcs_uniphy_option_3_u {
	a_uint32_t val;
	struct pcs_uniphy_option_3 bf;
};

/*[register] QSERDES_RX_EXT_RXEQ_CTRL0*/
#define QSERDES_RX_EXT_RXEQ_CTRL0
#define QSERDES_RX_EXT_RXEQ_CTRL0_ADDRESS 0xcb20
#define QSERDES_RX_EXT_RXEQ_CTRL0_NUM     3
#define QSERDES_RX_EXT_RXEQ_CTRL0_INC     0x1
#define QSERDES_RX_EXT_RXEQ_CTRL0_TYPE    REG_TYPE_RW
#define QSERDES_RX_EXT_RXEQ_CTRL0_DEFAULT 0x18
	/*[field] XPCS_RXEQ_EN_MASK*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MASK
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MASK_OFFSET  7
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MASK_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MASK_DEFAULT 0x0
	/*[field] XPCS_RXEQ_EN*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_OFFSET  6
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_DEFAULT 0x0
	/*[field] XPCS_RXEQ_EN_MUX*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MUX
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MUX_OFFSET  5
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MUX_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_XPCS_RXEQ_EN_MUX_DEFAULT 0x0
	/*[field] GX_EARLY_TRAINING_EN*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_GX_EARLY_TRAINING_EN
	#define QSERDES_RX_EXT_RXEQ_CTRL0_GX_EARLY_TRAINING_EN_OFFSET  4
	#define QSERDES_RX_EXT_RXEQ_CTRL0_GX_EARLY_TRAINING_EN_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_GX_EARLY_TRAINING_EN_DEFAULT 0x1
	/*[field] USE_RXEQINPHASE0123*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_USE_RXEQINPHASE0123
	#define QSERDES_RX_EXT_RXEQ_CTRL0_USE_RXEQINPHASE0123_OFFSET  3
	#define QSERDES_RX_EXT_RXEQ_CTRL0_USE_RXEQINPHASE0123_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_USE_RXEQINPHASE0123_DEFAULT 0x1
	/*[field] DFE_RESET_ON_PHASE3_START_EN*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_PHASE3_START_EN
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_PHASE3_START_EN_OFFSET  2
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_PHASE3_START_EN_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_PHASE3_START_EN_DEFAULT 0x0
	/*[field] DFE_RESET_ON_RETRAIN_EN*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_RETRAIN_EN
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_RETRAIN_EN_OFFSET  1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_RETRAIN_EN_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_DFE_RESET_ON_RETRAIN_EN_DEFAULT 0x1
	/*[field] RXEQEVAL_MASK_EN*/
	#define QSERDES_RX_EXT_RXEQ_CTRL0_RXEQEVAL_MASK_EN
	#define QSERDES_RX_EXT_RXEQ_CTRL0_RXEQEVAL_MASK_EN_OFFSET  0
	#define QSERDES_RX_EXT_RXEQ_CTRL0_RXEQEVAL_MASK_EN_LEN     1
	#define QSERDES_RX_EXT_RXEQ_CTRL0_RXEQEVAL_MASK_EN_DEFAULT 0x0

struct qserdes_rx_ext_rxeq_ctrl0 {
	a_uint32_t  rxeqeval_mask_en:1;            /* [0] RXEQ Evaluation Mask Enable */
	a_uint32_t  dfe_reset_on_retrain_en:1;     /* [1] DFE Reset on Retrain Enable */
	a_uint32_t  dfe_reset_on_phase3_start_en:1; /* [2] DFE Reset on Phase3 Start Enable */
	a_uint32_t  use_rxeqinphase0123:1;         /* [3] Use RXEQ in Phase 0123 */
	a_uint32_t  gx_early_training_en:1;        /* [4] GX Early Training Enable */
	a_uint32_t  xpcs_rxeq_en_mux:1;            /* [5] XPCS RXEQ Enable MUX */
	a_uint32_t  xpcs_rxeq_en:1;                /* [6] XPCS RXEQ Enable */
	a_uint32_t  xpcs_rxeq_en_mask:1;           /* [7] XPCS RXEQ Enable Mask */
	a_uint32_t  _reserved0:24;                 /* [31:8] Reserved */
};

union qserdes_rx_ext_rxeq_ctrl0_u {
	a_uint32_t val;
	struct qserdes_rx_ext_rxeq_ctrl0 bf;
};

/*[register] QSERDES_RX_EXT_RO_POWER_STATE*/
#define QSERDES_RX_EXT_RO_POWER_STATE
#define QSERDES_RX_EXT_RO_POWER_STATE_ADDRESS 0xcdf4
#define QSERDES_RX_EXT_RO_POWER_STATE_NUM     3
#define QSERDES_RX_EXT_RO_POWER_STATE_INC     0x1
#define QSERDES_RX_EXT_RO_POWER_STATE_TYPE    REG_TYPE_RW
#define QSERDES_RX_EXT_RO_POWER_STATE_DEFAULT 0x0
	/*[field] POWER_STATE*/
	#define QSERDES_RX_EXT_RO_POWER_STATE_POWER_STATE
	#define QSERDES_RX_EXT_RO_POWER_STATE_POWER_STATE_OFFSET  0
	#define QSERDES_RX_EXT_RO_POWER_STATE_POWER_STATE_LEN     4
	#define QSERDES_RX_EXT_RO_POWER_STATE_POWER_STATE_DEFAULT 0x0

struct qserdes_rx_ext_ro_power_state {
	a_uint32_t  power_state:4;
	a_uint32_t  _reserved0:28;
};

union qserdes_rx_ext_ro_power_state_u {
	a_uint32_t val;
	struct qserdes_rx_ext_ro_power_state bf;
};

/*[register] QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS*/
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS 0xcdfc
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_NUM     3
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_INC     0x1
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TYPE    REG_TYPE_R
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_DEFAULT 0x0
	/*[field] TIMER_DONE*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TIMER_DONE
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TIMER_DONE_OFFSET  2
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TIMER_DONE_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TIMER_DONE_DEFAULT 0x0
	/*[field] CTL_DONE*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_CTL_DONE
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_CTL_DONE_OFFSET  3
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_CTL_DONE_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_CTL_DONE_DEFAULT 0x0
	/*[field] VGA_DONE*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_VGA_DONE
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_VGA_DONE_OFFSET  4
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_VGA_DONE_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_VGA_DONE_DEFAULT 0x0
	/*[field] DFE_DONE*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_DFE_DONE
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_DFE_DONE_OFFSET  5
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_DFE_DONE_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_DFE_DONE_DEFAULT 0x0
	/*[field] TRACKING*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TRACKING
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TRACKING_OFFSET  6
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TRACKING_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_TRACKING_DEFAULT 0x0
	/*[field] RXEQ_ENGINE_DONE*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_RXEQ_ENGINE_DONE
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_RXEQ_ENGINE_DONE_OFFSET  1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_RXEQ_ENGINE_DONE_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_RXEQ_ENGINE_DONE_DEFAULT 0x0
	/*[field] PMAD_RXEQ_EN*/
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_PMAD_RXEQ_EN
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_PMAD_RXEQ_EN_OFFSET  0
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_PMAD_RXEQ_EN_LEN     1
	#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_PMAD_RXEQ_EN_DEFAULT 0x0

struct qserdes_rx_ext_ro_pmad_rxeq_status {
	a_uint32_t  pmad_rxeq_en:1;                /* [0] PMAD RXEQ Enable */
	a_uint32_t  rxeq_engine_done:1;            /* [1] RXEQ Engine Done */
	a_uint32_t  timer_done:1;                  /* [2] Timer Done */
	a_uint32_t  ctl_done:1;                    /* [3] CTL Done */
	a_uint32_t  vga_done:1;                    /* [4] VGA Done */
	a_uint32_t  dfe_done:1;                    /* [5] DFE Done */
	a_uint32_t  tracking:1;                    /* [6] Tracking */
	a_uint32_t  _reserved0:25;                 /* [31:7] Reserved */
};

union qserdes_rx_ext_ro_pmad_rxeq_status_u {
	a_uint32_t val;
	struct qserdes_rx_ext_ro_pmad_rxeq_status bf;
};

/*[register] SR_PMA_CTRL2*/
#define SR_PMA_CTRL2
#define SR_PMA_CTRL2_ADDRESS 0x10007
#define SR_PMA_CTRL2_NUM     3
#define SR_PMA_CTRL2_INC     0x1
#define SR_PMA_CTRL2_TYPE    REG_TYPE_RW
#define SR_PMA_CTRL2_DEFAULT 0x0
	/*[field] Reserved_15_10*/
	#define SR_PMA_CTRL2_RESERVED_15_10
	#define SR_PMA_CTRL2_RESERVED_15_10_OFFSET  10
	#define SR_PMA_CTRL2_RESERVED_15_10_LEN     6
	#define SR_PMA_CTRL2_RESERVED_15_10_DEFAULT 0x0
	/*[field] PIASE*/
	#define SR_PMA_CTRL2_PIASE
	#define SR_PMA_CTRL2_PIASE_OFFSET  9
	#define SR_PMA_CTRL2_PIASE_LEN     1
	#define SR_PMA_CTRL2_PIASE_DEFAULT 0x0
	/*[field] PEASE*/
	#define SR_PMA_CTRL2_PEASE
	#define SR_PMA_CTRL2_PEASE_OFFSET  8
	#define SR_PMA_CTRL2_PEASE_LEN     1
	#define SR_PMA_CTRL2_PEASE_DEFAULT 0x0
	/*[field] Reserved_7*/
	#define SR_PMA_CTRL2_RESERVED_7
	#define SR_PMA_CTRL2_RESERVED_7_OFFSET  7
	#define SR_PMA_CTRL2_RESERVED_7_LEN     1
	#define SR_PMA_CTRL2_RESERVED_7_DEFAULT 0x0
	/*[field] PMA_TYPE*/
	#define SR_PMA_CTRL2_PMA_TYPE
	#define SR_PMA_CTRL2_PMA_TYPE_OFFSET  0
	#define SR_PMA_CTRL2_PMA_TYPE_LEN     7
	#define SR_PMA_CTRL2_PMA_TYPE_DEFAULT 0x0

struct sr_pma_ctrl2 {
	a_uint32_t  pma_type:7;         /* [6:0] PMA Type Select - During initialization, the host must program an appropriate value depending upon the active PHY with which DWC_xsgpcs is interfacing */
	a_uint32_t  reserved_7:1;       /* [7] Reserved */
	a_uint32_t  pease:1;            /* [8] PMA Egress AUI Stop Enable - Allows transition to a low-power state in the Egress direction */
	a_uint32_t  piase:1;            /* [9] PMA Ingress AUI Stop Enable - Allows transition to a low-power state in the Ingress direction */
	a_uint32_t  reserved_15_10:6;   /* [15:10] Reserved */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union sr_pma_ctrl2_u {
	a_uint32_t val;
	struct sr_pma_ctrl2 bf;
};

/*[register] SR_PMA_KR_FEC_CTRL*/
#define SR_PMA_KR_FEC_CTRL
#define SR_PMA_KR_FEC_CTRL_ADDRESS 0x100ab
#define SR_PMA_KR_FEC_CTRL_NUM     3
#define SR_PMA_KR_FEC_CTRL_INC     0x1
#define SR_PMA_KR_FEC_CTRL_TYPE    REG_TYPE_RW
#define SR_PMA_KR_FEC_CTRL_DEFAULT 0x0
	/*[field] Reserved_15_2*/
	#define SR_PMA_KR_FEC_CTRL_RESERVED_15_2
	#define SR_PMA_KR_FEC_CTRL_RESERVED_15_2_OFFSET  2
	#define SR_PMA_KR_FEC_CTRL_RESERVED_15_2_LEN     14
	#define SR_PMA_KR_FEC_CTRL_RESERVED_15_2_DEFAULT 0x0
	/*[field] EN_ERR_IND*/
	#define SR_PMA_KR_FEC_CTRL_EN_ERR_IND
	#define SR_PMA_KR_FEC_CTRL_EN_ERR_IND_OFFSET  1
	#define SR_PMA_KR_FEC_CTRL_EN_ERR_IND_LEN     1
	#define SR_PMA_KR_FEC_CTRL_EN_ERR_IND_DEFAULT 0x0
	/*[field] FEC_EN*/
	#define SR_PMA_KR_FEC_CTRL_FEC_EN
	#define SR_PMA_KR_FEC_CTRL_FEC_EN_OFFSET  0
	#define SR_PMA_KR_FEC_CTRL_FEC_EN_LEN     1
	#define SR_PMA_KR_FEC_CTRL_FEC_EN_DEFAULT 0x0

struct sr_pma_kr_fec_ctrl {
	a_uint32_t  fec_en:1;           /* [0] FEC_EN - FEC Enable. When set, this bit enables BASE-KR FEC. When reset, this bit disables BASE-KR FEC */
	a_uint32_t  en_err_ind:1;       /* [1] EN_ERR_IND - FEC Enable Error Indication. When set, this bit configures the FEC decoder to indicate uncorrectable errors to the PCS layer */
	a_uint32_t  reserved_15_2:14;   /* [15:2] Reserved_15_2 - Reserved */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union sr_pma_kr_fec_ctrl_u {
	a_uint32_t val;
	struct sr_pma_kr_fec_ctrl bf;
};

/*[register] SR_PMA_RS_FEC_CTRL*/
#define SR_PMA_RS_FEC_CTRL
#define SR_PMA_RS_FEC_CTRL_ADDRESS 0x100c8
#define SR_PMA_RS_FEC_CTRL_NUM     3
#define SR_PMA_RS_FEC_CTRL_INC     0x1
#define SR_PMA_RS_FEC_CTRL_TYPE    REG_TYPE_RW
#define SR_PMA_RS_FEC_CTRL_DEFAULT 0x0
	/*[field] Reserved_15_7*/
	#define SR_PMA_RS_FEC_CTRL_RESERVED_15_7
	#define SR_PMA_RS_FEC_CTRL_RESERVED_15_7_OFFSET  7
	#define SR_PMA_RS_FEC_CTRL_RESERVED_15_7_LEN     9
	#define SR_PMA_RS_FEC_CTRL_RESERVED_15_7_DEFAULT 0x0
	/*[field] CL91_100G_EN*/
	#define SR_PMA_RS_FEC_CTRL_CL91_100G_EN
	#define SR_PMA_RS_FEC_CTRL_CL91_100G_EN_OFFSET  6
	#define SR_PMA_RS_FEC_CTRL_CL91_100G_EN_LEN     1
	#define SR_PMA_RS_FEC_CTRL_CL91_100G_EN_DEFAULT 0x0
	/*[field] CL161_100G_EN*/
	#define SR_PMA_RS_FEC_CTRL_CL161_100G_EN
	#define SR_PMA_RS_FEC_CTRL_CL161_100G_EN_OFFSET  5
	#define SR_PMA_RS_FEC_CTRL_CL161_100G_EN_LEN     1
	#define SR_PMA_RS_FEC_CTRL_CL161_100G_EN_DEFAULT 0x0
	/*[field] DSER_EN*/
	#define SR_PMA_RS_FEC_CTRL_DSER_EN
	#define SR_PMA_RS_FEC_CTRL_DSER_EN_OFFSET  4
	#define SR_PMA_RS_FEC_CTRL_DSER_EN_LEN     1
	#define SR_PMA_RS_FEC_CTRL_DSER_EN_DEFAULT 0x0
	/*[field] FLP*/
	#define SR_PMA_RS_FEC_CTRL_FLP
	#define SR_PMA_RS_FEC_CTRL_FLP_OFFSET  3
	#define SR_PMA_RS_FEC_CTRL_FLP_LEN     1
	#define SR_PMA_RS_FEC_CTRL_FLP_DEFAULT 0x0
	/*[field] RSFEC_EN*/
	#define SR_PMA_RS_FEC_CTRL_RSFEC_EN
	#define SR_PMA_RS_FEC_CTRL_RSFEC_EN_OFFSET  2
	#define SR_PMA_RS_FEC_CTRL_RSFEC_EN_LEN     1
	#define SR_PMA_RS_FEC_CTRL_RSFEC_EN_DEFAULT 0x0
	/*[field] BYP_INE*/
	#define SR_PMA_RS_FEC_CTRL_BYP_INE
	#define SR_PMA_RS_FEC_CTRL_BYP_INE_OFFSET  1
	#define SR_PMA_RS_FEC_CTRL_BYP_INE_LEN     1
	#define SR_PMA_RS_FEC_CTRL_BYP_INE_DEFAULT 0x0
	/*[field] BYP_CRE*/
	#define SR_PMA_RS_FEC_CTRL_BYP_CRE
	#define SR_PMA_RS_FEC_CTRL_BYP_CRE_OFFSET  0
	#define SR_PMA_RS_FEC_CTRL_BYP_CRE_LEN     1
	#define SR_PMA_RS_FEC_CTRL_BYP_CRE_DEFAULT 0x0

struct sr_pma_rs_fec_ctrl {
	a_uint32_t  byp_cre:1;          /* [0] BYP_CRE - RS-FEC bypass correction enable. This bit enables the RS-FEC decoder to bypass Error Correction */
	a_uint32_t  byp_ine:1;          /* [1] BYP_INE - RS-FEC bypass indication enable. This bit enables the RS-FEC decoder to bypass error indication to the upper layers (PCS) through the sync bits */
	a_uint32_t  rsfec_en:1;         /* [2] RSFEC_EN - 25G Base Reed-Solomon FEC Enable. This bit enables the 25GBase-R Reed Solomon Forward Error Correction */
	a_uint32_t  flp:1;              /* [3] FLP - Four Lane PMD Indication. Encoding of this field is as follows: 0: four-lane-PMD is used, 1: four-lane PMD is not used */
	a_uint32_t  dser_en:1;          /* [4] DSER_EN - FEC Degraded SER Enable. When operating in RS-FEC (544,514) mode, set this bit to enable the 'Degraded Symbol Error Rate' feature */
	a_uint32_t  cl161_100g_en:1;    /* [5] CL161_100G_EN - 100G Interleaved RS-FEC Mode Enable. When this bit is set, Clause 161 code-word interleaved RS-FEC mode is enabled */
	a_uint32_t  cl91_100g_en:1;     /* [6] CL91_100G_EN - 100G Non-interleaved RS-FEC Mode Enable. For configurations with both Clause 91 and Clause 161 RSFEC support, Whenever this bit is set, Clause 91 based RS-FEC is enabled in 100G Mode */
	a_uint32_t  reserved_15_7:9;    /* [15:7] Reserved_15_7 - Reserved */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union sr_pma_rs_fec_ctrl_u {
	a_uint32_t val;
	struct sr_pma_rs_fec_ctrl bf;
};

/*[register] VR_PMA_CWM00*/
#define VR_PMA_CWM00
#define VR_PMA_CWM00_ADDRESS 0x180a3
#define VR_PMA_CWM00_NUM     3
#define VR_PMA_CWM00_INC     0x1
#define VR_PMA_CWM00_TYPE    REG_TYPE_RW
#define VR_PMA_CWM00_DEFAULT 0x7690
	/*[field] CWM15_0*/
	#define VR_PMA_CWM00_CWM15_0
	#define VR_PMA_CWM00_CWM15_0_OFFSET  0
	#define VR_PMA_CWM00_CWM15_0_LEN     16
	#define VR_PMA_CWM00_CWM15_0_DEFAULT 0x7690

struct vr_pma_cwm00 {
	a_uint32_t  cwm15_0:16;         /* [15:0] CWM15_0 - This register provides the RSFEC Code Word Marker bits[15:0] to be in 25G mode of operation */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pma_cwm00_u {
	a_uint32_t val;
	struct vr_pma_cwm00 bf;
};

/*[register] VR_PMA_CWM01*/
#define VR_PMA_CWM01
#define VR_PMA_CWM01_ADDRESS 0x180a4
#define VR_PMA_CWM01_NUM     3
#define VR_PMA_CWM01_INC     0x1
#define VR_PMA_CWM01_TYPE    REG_TYPE_RW
#define VR_PMA_CWM01_DEFAULT 0x3347
	/*[field] CWM31_16*/
	#define VR_PMA_CWM01_CWM31_16
	#define VR_PMA_CWM01_CWM31_16_OFFSET  0
	#define VR_PMA_CWM01_CWM31_16_LEN     16
	#define VR_PMA_CWM01_CWM31_16_DEFAULT 0x3347

struct vr_pma_cwm01 {
	a_uint32_t  cwm31_16:16;        /* [15:0] CWM31_16 - This register provides the RSFEC Code Word Marker bits[31:16] to be in 25G mode of operation */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pma_cwm01_u {
	a_uint32_t val;
	struct vr_pma_cwm01 bf;
};

/*[register] VR_PMA_CWM02*/
#define VR_PMA_CWM02
#define VR_PMA_CWM02_ADDRESS 0x180a5
#define VR_PMA_CWM02_NUM     3
#define VR_PMA_CWM02_INC     0x1
#define VR_PMA_CWM02_TYPE    REG_TYPE_RW
#define VR_PMA_CWM02_DEFAULT 0x896f
	/*[field] CWM47_32*/
	#define VR_PMA_CWM02_CWM47_32
	#define VR_PMA_CWM02_CWM47_32_OFFSET  0
	#define VR_PMA_CWM02_CWM47_32_LEN     16
	#define VR_PMA_CWM02_CWM47_32_DEFAULT 0x896f

struct vr_pma_cwm02 {
	a_uint32_t  cwm47_32:16;        /* [15:0] CWM47_32 - This register provides the RSFEC Code Word Marker bits[47:32] to be in 25G mode of operation */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pma_cwm02_u {
	a_uint32_t val;
	struct vr_pma_cwm02 bf;
};

/*[register] VR_PMA_CWM03*/
#define VR_PMA_CWM03
#define VR_PMA_CWM03_ADDRESS 0x180a6
#define VR_PMA_CWM03_NUM     3
#define VR_PMA_CWM03_INC     0x1
#define VR_PMA_CWM03_TYPE    REG_TYPE_RW
#define VR_PMA_CWM03_DEFAULT 0xccb8
	/*[field] CWM63_48*/
	#define VR_PMA_CWM03_CWM63_48
	#define VR_PMA_CWM03_CWM63_48_OFFSET  0
	#define VR_PMA_CWM03_CWM63_48_LEN     16
	#define VR_PMA_CWM03_CWM63_48_DEFAULT 0xccb8

struct vr_pma_cwm03 {
	a_uint32_t  cwm63_48:16;        /* [15:0] CWM63_48 - This register provides the RSFEC Code Word Marker bits[63:48] to be in 25G mode of operation */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pma_cwm03_u {
	a_uint32_t val;
	struct vr_pma_cwm03 bf;
};

/*[register] SR_PCS_CTRL1*/
#define SR_PCS_CTRL1
#define SR_PCS_CTRL1_ADDRESS 0x30000
#define SR_PCS_CTRL1_NUM     3
#define SR_PCS_CTRL1_INC     0x1
#define SR_PCS_CTRL1_TYPE    REG_TYPE_RW
#define SR_PCS_CTRL1_DEFAULT 0x0
	/*[field] Reserved_1_0*/
	#define SR_PCS_CTRL1_RESERVED_1_0
	#define SR_PCS_CTRL1_RESERVED_1_0_OFFSET  0
	#define SR_PCS_CTRL1_RESERVED_1_0_LEN     2
	#define SR_PCS_CTRL1_RESERVED_1_0_DEFAULT 0x0
	/*[field] SS_5_2*/
	#define SR_PCS_CTRL1_SS_5_2
	#define SR_PCS_CTRL1_SS_5_2_OFFSET  2
	#define SR_PCS_CTRL1_SS_5_2_LEN     4
	#define SR_PCS_CTRL1_SS_5_2_DEFAULT 0x5
	/*[field] SS6*/
	#define SR_PCS_CTRL1_SS6
	#define SR_PCS_CTRL1_SS6_OFFSET  6
	#define SR_PCS_CTRL1_SS6_LEN     1
	#define SR_PCS_CTRL1_SS6_DEFAULT 0x1
	/*[field] Reserved_9_7*/
	#define SR_PCS_CTRL1_RESERVED_9_7
	#define SR_PCS_CTRL1_RESERVED_9_7_OFFSET  7
	#define SR_PCS_CTRL1_RESERVED_9_7_LEN     3
	#define SR_PCS_CTRL1_RESERVED_9_7_DEFAULT 0x0
	/*[field] CS_EN*/
	#define SR_PCS_CTRL1_CS_EN
	#define SR_PCS_CTRL1_CS_EN_OFFSET  10
	#define SR_PCS_CTRL1_CS_EN_LEN     1
	#define SR_PCS_CTRL1_CS_EN_DEFAULT 0x1
	/*[field] LPM*/
	#define SR_PCS_CTRL1_LPM
	#define SR_PCS_CTRL1_LPM_OFFSET  11
	#define SR_PCS_CTRL1_LPM_LEN     1
	#define SR_PCS_CTRL1_LPM_DEFAULT 0x0
	/*[field] Reserved_12*/
	#define SR_PCS_CTRL1_RESERVED_12
	#define SR_PCS_CTRL1_RESERVED_12_OFFSET  12
	#define SR_PCS_CTRL1_RESERVED_12_LEN     1
	#define SR_PCS_CTRL1_RESERVED_12_DEFAULT 0x0
	/*[field] SS13*/
	#define SR_PCS_CTRL1_SS13
	#define SR_PCS_CTRL1_SS13_OFFSET  13
	#define SR_PCS_CTRL1_SS13_LEN     1
	#define SR_PCS_CTRL1_SS13_DEFAULT 0x1
	/*[field] Reserved_14*/
	#define SR_PCS_CTRL1_RESERVED_14
	#define SR_PCS_CTRL1_RESERVED_14_OFFSET  14
	#define SR_PCS_CTRL1_RESERVED_14_LEN     1
	#define SR_PCS_CTRL1_RESERVED_14_DEFAULT 0x0
	/*[field] RST*/
	#define SR_PCS_CTRL1_RST
	#define SR_PCS_CTRL1_RST_OFFSET  15
	#define SR_PCS_CTRL1_RST_LEN     1
	#define SR_PCS_CTRL1_RST_DEFAULT 0x0

struct sr_pcs_ctrl1 {
	a_uint32_t  reserved_1_0:2;     /* [1:0] Reserved */
	a_uint32_t  ss_5_2:4;           /* [5:2] Speed Selection Bits [5:2] - Speed encoding: 1000=5Gb/s, 0110=50Gb/s, 0101=25Gb/s, 0100=100Gb/s, 0011=40Gb/s, 0000=10Gb/s */
	a_uint32_t  ss6:1;              /* [6] Speed Selection Bit 6 - This bit always returns 1 */
	a_uint32_t  reserved_9_7:3;     /* [9:7] Reserved */
	a_uint32_t  cs_en:1;            /* [10] Clock Stop Enable - Controls stopping the clock during LPI mode on the XGMII Rx interface */
	a_uint32_t  lpm:1;              /* [11] Low Power Mode - Controls the power-down mode of DWC_xsgpcs */
	a_uint32_t  reserved_12:1;      /* [12] Reserved */
	a_uint32_t  ss13:1;             /* [13] Speed Selection Bit 13 - This bit always returns 1 */
	a_uint32_t  reserved_14:1;      /* [14] Reserved */
	a_uint32_t  rst:1;              /* [15] Soft Reset - When the host sets this bit, the CSR block triggers the software reset process */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union sr_pcs_ctrl1_u {
	a_uint32_t val;
	struct sr_pcs_ctrl1 bf;
};

/*[register] SR_PCS_STS1*/
#define SR_PCS_STS1
#define SR_PCS_STS1_ADDRESS 0x30001
#define SR_PCS_STS1_NUM     3
#define SR_PCS_STS1_INC     0x1
#define SR_PCS_STS1_TYPE    REG_TYPE_RO
#define SR_PCS_STS1_DEFAULT 0x0
	/*[field] Reserved_0*/
	#define SR_PCS_STS1_RESERVED_0
	#define SR_PCS_STS1_RESERVED_0_OFFSET  0
	#define SR_PCS_STS1_RESERVED_0_LEN     1
	#define SR_PCS_STS1_RESERVED_0_DEFAULT 0x0
	/*[field] LPMS*/
	#define SR_PCS_STS1_LPMS
	#define SR_PCS_STS1_LPMS_OFFSET  1
	#define SR_PCS_STS1_LPMS_LEN     1
	#define SR_PCS_STS1_LPMS_DEFAULT 0x0
	/*[field] RLU*/
	#define SR_PCS_STS1_RLU
	#define SR_PCS_STS1_RLU_OFFSET  2
	#define SR_PCS_STS1_RLU_LEN     1
	#define SR_PCS_STS1_RLU_DEFAULT 0x0
	/*[field] Reserved_5_3*/
	#define SR_PCS_STS1_RESERVED_5_3
	#define SR_PCS_STS1_RESERVED_5_3_OFFSET  3
	#define SR_PCS_STS1_RESERVED_5_3_LEN     3
	#define SR_PCS_STS1_RESERVED_5_3_DEFAULT 0x0
	/*[field] CSC*/
	#define SR_PCS_STS1_CSC
	#define SR_PCS_STS1_CSC_OFFSET  6
	#define SR_PCS_STS1_CSC_LEN     1
	#define SR_PCS_STS1_CSC_DEFAULT 0x0
	/*[field] FLT*/
	#define SR_PCS_STS1_FLT
	#define SR_PCS_STS1_FLT_OFFSET  7
	#define SR_PCS_STS1_FLT_LEN     1
	#define SR_PCS_STS1_FLT_DEFAULT 0x0
	/*[field] RXLPII*/
	#define SR_PCS_STS1_RXLPII
	#define SR_PCS_STS1_RXLPII_OFFSET  8
	#define SR_PCS_STS1_RXLPII_LEN     1
	#define SR_PCS_STS1_RXLPII_DEFAULT 0x0
	/*[field] TXLPII*/
	#define SR_PCS_STS1_TXLPII
	#define SR_PCS_STS1_TXLPII_OFFSET  9
	#define SR_PCS_STS1_TXLPII_LEN     1
	#define SR_PCS_STS1_TXLPII_DEFAULT 0x0
	/*[field] RXLPIR*/
	#define SR_PCS_STS1_RXLPIR
	#define SR_PCS_STS1_RXLPIR_OFFSET  10
	#define SR_PCS_STS1_RXLPIR_LEN     1
	#define SR_PCS_STS1_RXLPIR_DEFAULT 0x0
	/*[field] TXLPIR*/
	#define SR_PCS_STS1_TXLPIR
	#define SR_PCS_STS1_TXLPIR_OFFSET  11
	#define SR_PCS_STS1_TXLPIR_LEN     1
	#define SR_PCS_STS1_TXLPIR_DEFAULT 0x0
	/*[field] Reserved_15_12*/
	#define SR_PCS_STS1_RESERVED_15_12
	#define SR_PCS_STS1_RESERVED_15_12_OFFSET  12
	#define SR_PCS_STS1_RESERVED_15_12_LEN     4
	#define SR_PCS_STS1_RESERVED_15_12_DEFAULT 0x0

struct sr_pcs_sts1 {
	a_uint32_t  reserved_0:1;
	a_uint32_t  lpms:1;
	a_uint32_t  rlu:1;
	a_uint32_t  reserved_5_3:3;
	a_uint32_t  csc:1;
	a_uint32_t  flt:1;
	a_uint32_t  rxlpii:1;
	a_uint32_t  txlpii:1;
	a_uint32_t  rxlpir:1;
	a_uint32_t  txlpir:1;
	a_uint32_t  reserved_15_12:4;
	a_uint32_t  _reserved0:16;
};

union sr_pcs_sts1_u {
	a_uint32_t val;
	struct sr_pcs_sts1 bf;
};

/*[register] SR_PCS_CTRL2*/
#define SR_PCS_CTRL2
#define SR_PCS_CTRL2_ADDRESS 0x30007
#define SR_PCS_CTRL2_NUM     3
#define SR_PCS_CTRL2_INC     0x1
#define SR_PCS_CTRL2_TYPE    REG_TYPE_RW
#define SR_PCS_CTRL2_DEFAULT 0x7
	/*[field] Reserved_15_4*/
	#define SR_PCS_CTRL2_RESERVED_15_4
	#define SR_PCS_CTRL2_RESERVED_15_4_OFFSET  4
	#define SR_PCS_CTRL2_RESERVED_15_4_LEN     12
	#define SR_PCS_CTRL2_RESERVED_15_4_DEFAULT 0x0
	/*[field] PCS_TYPE_SEL*/
	#define SR_PCS_CTRL2_PCS_TYPE_SEL
	#define SR_PCS_CTRL2_PCS_TYPE_SEL_OFFSET  0
	#define SR_PCS_CTRL2_PCS_TYPE_SEL_LEN     4
	#define SR_PCS_CTRL2_PCS_TYPE_SEL_DEFAULT 0x7

struct sr_pcs_ctrl2 {
	a_uint32_t  pcs_type_sel:4;     /* [3:0] PCS Type Select - This field is used to select the speed mode in DWC_xsgpcs when multiple speed modes are present */
	a_uint32_t  reserved_15_4:12;   /* [15:4] Reserved */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union sr_pcs_ctrl2_u {
	a_uint32_t val;
	struct sr_pcs_ctrl2 bf;
};

/*[register] VR_PCS_DIG_CTRL1*/
#define VR_PCS_DIG_CTRL1
#define VR_PCS_DIG_CTRL1_ADDRESS 0x38000
#define VR_PCS_DIG_CTRL1_NUM     3
#define VR_PCS_DIG_CTRL1_INC     0x1
#define VR_PCS_DIG_CTRL1_TYPE    REG_TYPE_RW
#define VR_PCS_DIG_CTRL1_DEFAULT 0x2000
	/*[field] VR_RST*/
	#define VR_PCS_DIG_CTRL1_VR_RST
	#define VR_PCS_DIG_CTRL1_VR_RST_OFFSET  15
	#define VR_PCS_DIG_CTRL1_VR_RST_LEN     1
	#define VR_PCS_DIG_CTRL1_VR_RST_DEFAULT 0x0
	/*[field] R2TLBE*/
	#define VR_PCS_DIG_CTRL1_R2TLBE
	#define VR_PCS_DIG_CTRL1_R2TLBE_OFFSET  14
	#define VR_PCS_DIG_CTRL1_R2TLBE_LEN     1
	#define VR_PCS_DIG_CTRL1_R2TLBE_DEFAULT 0x0
	/*[field] EN_VSMMD1*/
	#define VR_PCS_DIG_CTRL1_EN_VSMMD1
	#define VR_PCS_DIG_CTRL1_EN_VSMMD1_OFFSET  13
	#define VR_PCS_DIG_CTRL1_EN_VSMMD1_LEN     1
	#define VR_PCS_DIG_CTRL1_EN_VSMMD1_DEFAULT 0x1
	/*[field] CL37_BP*/
	#define VR_PCS_DIG_CTRL1_CL37_BP
	#define VR_PCS_DIG_CTRL1_CL37_BP_OFFSET  12
	#define VR_PCS_DIG_CTRL1_CL37_BP_LEN     1
	#define VR_PCS_DIG_CTRL1_CL37_BP_DEFAULT 0x0
	/*[field] PWRSV*/
	#define VR_PCS_DIG_CTRL1_PWRSV
	#define VR_PCS_DIG_CTRL1_PWRSV_OFFSET  11
	#define VR_PCS_DIG_CTRL1_PWRSV_LEN     1
	#define VR_PCS_DIG_CTRL1_PWRSV_DEFAULT 0x0
	/*[field] Reserved_10*/
	#define VR_PCS_DIG_CTRL1_RESERVED_10
	#define VR_PCS_DIG_CTRL1_RESERVED_10_OFFSET  10
	#define VR_PCS_DIG_CTRL1_RESERVED_10_LEN     1
	#define VR_PCS_DIG_CTRL1_RESERVED_10_DEFAULT 0x0
	/*[field] RX_INIT*/
	#define VR_PCS_DIG_CTRL1_RX_INIT
	#define VR_PCS_DIG_CTRL1_RX_INIT_OFFSET  9
	#define VR_PCS_DIG_CTRL1_RX_INIT_LEN     1
	#define VR_PCS_DIG_CTRL1_RX_INIT_DEFAULT 0x0
	/*[field] TX_INIT*/
	#define VR_PCS_DIG_CTRL1_TX_INIT
	#define VR_PCS_DIG_CTRL1_TX_INIT_OFFSET  8
	#define VR_PCS_DIG_CTRL1_TX_INIT_LEN     1
	#define VR_PCS_DIG_CTRL1_TX_INIT_DEFAULT 0x0
	/*[field] DTXLANED_3_1*/
	#define VR_PCS_DIG_CTRL1_DTXLANED_3_1
	#define VR_PCS_DIG_CTRL1_DTXLANED_3_1_OFFSET  5
	#define VR_PCS_DIG_CTRL1_DTXLANED_3_1_LEN     3
	#define VR_PCS_DIG_CTRL1_DTXLANED_3_1_DEFAULT 0x0
	/*[field] DTXLANED_0*/
	#define VR_PCS_DIG_CTRL1_DTXLANED_0
	#define VR_PCS_DIG_CTRL1_DTXLANED_0_OFFSET  4
	#define VR_PCS_DIG_CTRL1_DTXLANED_0_LEN     1
	#define VR_PCS_DIG_CTRL1_DTXLANED_0_DEFAULT 0x0
	/*[field] CR_CJN*/
	#define VR_PCS_DIG_CTRL1_CR_CJN
	#define VR_PCS_DIG_CTRL1_CR_CJN_OFFSET  3
	#define VR_PCS_DIG_CTRL1_CR_CJN_LEN     1
	#define VR_PCS_DIG_CTRL1_CR_CJN_DEFAULT 0x0
	/*[field] LANE_CTRL_EXT*/
	#define VR_PCS_DIG_CTRL1_LANE_CTRL_EXT
	#define VR_PCS_DIG_CTRL1_LANE_CTRL_EXT_OFFSET  2
	#define VR_PCS_DIG_CTRL1_LANE_CTRL_EXT_LEN     1
	#define VR_PCS_DIG_CTRL1_LANE_CTRL_EXT_DEFAULT 0x0
	/*[field] BYP_PWRUP*/
	#define VR_PCS_DIG_CTRL1_BYP_PWRUP
	#define VR_PCS_DIG_CTRL1_BYP_PWRUP_OFFSET  1
	#define VR_PCS_DIG_CTRL1_BYP_PWRUP_LEN     1
	#define VR_PCS_DIG_CTRL1_BYP_PWRUP_DEFAULT 0x0
	/*[field] DSKBYP*/
	#define VR_PCS_DIG_CTRL1_DSKBYP
	#define VR_PCS_DIG_CTRL1_DSKBYP_OFFSET  0
	#define VR_PCS_DIG_CTRL1_DSKBYP_LEN     1
	#define VR_PCS_DIG_CTRL1_DSKBYP_DEFAULT 0x0

struct vr_pcs_dig_ctrl1 {
	a_uint32_t  dskbyp:1;           /* [0] DSKBYP - Reserved */
	a_uint32_t  byp_pwrup:1;        /* [1] BYP_PWRUP - Reserved */
	a_uint32_t  lane_ctrl_ext:1;    /* [2] LANE_CTRL_EXT - Lane Control External. The transmit and receive lane disabling selection is based on the value programmed to this bit */
	a_uint32_t  cr_cjn:1;           /* [3] CR_CJN - Reserved */
	a_uint32_t  dtxlaned_0:1;       /* [4] DTXLANED_0 - Tx Disable on Lane 0. When this bit is set, the DWC_xlgpcs disables the Tx Lane 0 of the PHY */
	a_uint32_t  dtxlaned_3_1:3;     /* [7:5] DTXLANED_3_1 - Tx Disable on Lanes[3:1]. When any bits of this field are set, the DWC_xlgpcs disables the corresponding Tx lanes (3, 2, or 1) of the PHY */
	a_uint32_t  tx_init:1;          /* [8] TX_INIT - Tx Datapath Initialization Control. This bit can be set to flush/initialize the various FIFOs implemented in Transmit data-path of DWC_xlgpcs */
	a_uint32_t  rx_init:1;          /* [9] RX_INIT - Rx Datapath Initialization Control. This bit can be set to flush/initialize the various FIFOs implemented in Receive data-path of DWC_xlgpcs */
	a_uint32_t  reserved_10:1;      /* [10] Reserved_10 - Reserved */
	a_uint32_t  pwrsv:1;            /* [11] PWRSV - Reserved */
	a_uint32_t  cl37_bp:1;          /* [12] CL37_BP - Reserved */
	a_uint32_t  en_vsmmd1:1;        /* [13] EN_VSMMD1 - Enable Vendor-Specific MMD1. When this bit is set to 1, the Vendor-Specific MMD1 (VSMMD1) is enabled */
	a_uint32_t  r2tlbe:1;           /* [14] R2TLBE - Rx to Tx Loopback Enable. This bit controls the loopback path from the XGMII Rx to the XGMII Tx at the XGMII interface */
	a_uint32_t  vr_rst:1;           /* [15] VR_RST - Vendor-Specific Soft Reset (RW,SC Type). When the host sets this bit, the CSR block triggers the software reset process */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pcs_dig_ctrl1_u {
	a_uint32_t val;
	struct vr_pcs_dig_ctrl1 bf;
};

/*[register] VR_PCS_DIG_CTRL3*/
#define VR_PCS_DIG_CTRL3
#define VR_PCS_DIG_CTRL3_ADDRESS 0x38003
#define VR_PCS_DIG_CTRL3_NUM     3
#define VR_PCS_DIG_CTRL3_INC     0x1
#define VR_PCS_DIG_CTRL3_TYPE    REG_TYPE_RW
#define VR_PCS_DIG_CTRL3_DEFAULT 0x0
	/*[field] reserved_15_12*/
	#define VR_PCS_DIG_CTRL3_RESERVED_15_12
	#define VR_PCS_DIG_CTRL3_RESERVED_15_12_OFFSET  12
	#define VR_PCS_DIG_CTRL3_RESERVED_15_12_LEN     4
	#define VR_PCS_DIG_CTRL3_RESERVED_15_12_DEFAULT 0x0
	/*[field] Rsvd*/
	#define VR_PCS_DIG_CTRL3_RSVD
	#define VR_PCS_DIG_CTRL3_RSVD_OFFSET  11
	#define VR_PCS_DIG_CTRL3_RSVD_LEN     1
	#define VR_PCS_DIG_CTRL3_RSVD_DEFAULT 0x0
	/*[field] GAUI_544*/
	#define VR_PCS_DIG_CTRL3_GAUI_544
	#define VR_PCS_DIG_CTRL3_GAUI_544_OFFSET  10
	#define VR_PCS_DIG_CTRL3_GAUI_544_LEN     1
	#define VR_PCS_DIG_CTRL3_GAUI_544_DEFAULT 0x0
	/*[field] BMX_544*/
	#define VR_PCS_DIG_CTRL3_BMX_544
	#define VR_PCS_DIG_CTRL3_BMX_544_OFFSET  9
	#define VR_PCS_DIG_CTRL3_BMX_544_LEN     1
	#define VR_PCS_DIG_CTRL3_BMX_544_DEFAULT 0x0
	/*[field] MSK_PHY_RST*/
	#define VR_PCS_DIG_CTRL3_MSK_PHY_RST
	#define VR_PCS_DIG_CTRL3_MSK_PHY_RST_OFFSET  8
	#define VR_PCS_DIG_CTRL3_MSK_PHY_RST_LEN     1
	#define VR_PCS_DIG_CTRL3_MSK_PHY_RST_DEFAULT 0x0
	/*[field] FEC_PN_BYP_EN*/
	#define VR_PCS_DIG_CTRL3_FEC_PN_BYP_EN
	#define VR_PCS_DIG_CTRL3_FEC_PN_BYP_EN_OFFSET  7
	#define VR_PCS_DIG_CTRL3_FEC_PN_BYP_EN_LEN     1
	#define VR_PCS_DIG_CTRL3_FEC_PN_BYP_EN_DEFAULT 0x0
	/*[field] TRX_SCR_BYP_EN*/
	#define VR_PCS_DIG_CTRL3_TRX_SCR_BYP_EN
	#define VR_PCS_DIG_CTRL3_TRX_SCR_BYP_EN_OFFSET  6
	#define VR_PCS_DIG_CTRL3_TRX_SCR_BYP_EN_LEN     1
	#define VR_PCS_DIG_CTRL3_TRX_SCR_BYP_EN_DEFAULT 0x0
	/*[field] NKM_SEL*/
	#define VR_PCS_DIG_CTRL3_NKM_SEL
	#define VR_PCS_DIG_CTRL3_NKM_SEL_OFFSET  4
	#define VR_PCS_DIG_CTRL3_NKM_SEL_LEN     2
	#define VR_PCS_DIG_CTRL3_NKM_SEL_DEFAULT 0x0
	/*[field] NKM_EN*/
	#define VR_PCS_DIG_CTRL3_NKM_EN
	#define VR_PCS_DIG_CTRL3_NKM_EN_OFFSET  3
	#define VR_PCS_DIG_CTRL3_NKM_EN_LEN     1
	#define VR_PCS_DIG_CTRL3_NKM_EN_DEFAULT 0x0
	/*[field] CL72_EN*/
	#define VR_PCS_DIG_CTRL3_CL72_EN
	#define VR_PCS_DIG_CTRL3_CL72_EN_OFFSET  2
	#define VR_PCS_DIG_CTRL3_CL72_EN_LEN     1
	#define VR_PCS_DIG_CTRL3_CL72_EN_DEFAULT 0x0
	/*[field] EN_50G*/
	#define VR_PCS_DIG_CTRL3_EN_50G
	#define VR_PCS_DIG_CTRL3_EN_50G_OFFSET  1
	#define VR_PCS_DIG_CTRL3_EN_50G_LEN     1
	#define VR_PCS_DIG_CTRL3_EN_50G_DEFAULT 0x0
	/*[field] CNS_EN*/
	#define VR_PCS_DIG_CTRL3_CNS_EN
	#define VR_PCS_DIG_CTRL3_CNS_EN_OFFSET  0
	#define VR_PCS_DIG_CTRL3_CNS_EN_LEN     1
	#define VR_PCS_DIG_CTRL3_CNS_EN_DEFAULT 0x0

struct vr_pcs_dig_ctrl3 {
	a_uint32_t  cns_en:1;           /* [0] CNS Enable - This control bit provides enabling 25G or 50G mode of operation */
	a_uint32_t  en_50g:1;           /* [1] EN_50G - This control bit provides enabling 25G or 50G mode of operation In 50G enabled configuration */
	a_uint32_t  cl72_en:1;          /* [2] CL72_EN - This control bit provides enabling Clause 72 based training. In 25G/50G mode When training is enabled */
	a_uint32_t  nkm_en:1;           /* [3] NKM_EN - Reserved */
	a_uint32_t  nkm_sel:2;          /* [5:4] NKM_SEL - Reserved */
	a_uint32_t  trx_scr_byp_en:1;   /* [6] TRX_SCR_BYP_EN - Reserved */
	a_uint32_t  fec_pn_byp_en:1;    /* [7] FEC_PN_BYP_EN - Reserved */
	a_uint32_t  msk_phy_rst:1;      /* [8] MSK_PHY_RST - Mask PHY reset. Generally, when vendor-soft-reset (VR_RST) is programmed, DWC_xsgpcs asserts the xsgpcs_reset_n_o port */
	a_uint32_t  bmx_544:1;          /* [9] BMX_544 - Enable PMA Bit-Mux in RS(544,514) Mode. When DWC_xsgpcs is used to implement a 50GBASE-KR or 100GBASE-KR2 link */
	a_uint32_t  gaui_544:1;         /* [10] GAUI_544 - 100GAUI-4 or 50GAUI-2 Mode that uses RS544 FEC. This bit should be programmed to one, when desired operating mode is 100GAUI-4 or 50GAUI-2 */
	a_uint32_t  rsvd:1;             /* [11] Rsvd - Reserved Field: Yes */
	a_uint32_t  reserved_15_12:4;   /* [15:12] reserved_15_12 - Reserved */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pcs_dig_ctrl3_u {
	a_uint32_t val;
	struct vr_pcs_dig_ctrl3 bf;
};

/*[register] VR_PCS_AM_CNT*/
#define VR_PCS_AM_CNT
#define VR_PCS_AM_CNT_ADDRESS 0x38018
#define VR_PCS_AM_CNT_NUM     3
#define VR_PCS_AM_CNT_INC     0x1
#define VR_PCS_AM_CNT_TYPE    REG_TYPE_RW
#define VR_PCS_AM_CNT_DEFAULT 0x3fff
	/*[field] Reserved_15_14*/
	#define VR_PCS_AM_CNT_RESERVED_15_14
	#define VR_PCS_AM_CNT_RESERVED_15_14_OFFSET  14
	#define VR_PCS_AM_CNT_RESERVED_15_14_LEN     2
	#define VR_PCS_AM_CNT_RESERVED_15_14_DEFAULT 0x0
	/*[field] PCS_AM_CNT*/
	#define VR_PCS_AM_CNT_PCS_AM_CNT
	#define VR_PCS_AM_CNT_PCS_AM_CNT_OFFSET  0
	#define VR_PCS_AM_CNT_PCS_AM_CNT_LEN     14
	#define VR_PCS_AM_CNT_PCS_AM_CNT_DEFAULT 0x3fff

struct vr_pcs_am_cnt {
	a_uint32_t  pcs_am_cnt:14;      /* [13:0] PCS_AM_CNT - Alignment Marker Count Value. This field determines the interval at which Alignment Markers are inserted into the data-stream */
	a_uint32_t  reserved_15_14:2;   /* [15:14] Reserved_15_14 - Reserved */
	a_uint32_t  _reserved0:16;      /* [31:16] Reserved */
};

union vr_pcs_am_cnt_u {
	a_uint32_t val;
	struct vr_pcs_am_cnt bf;
};

#endif /* _JHPPE_UNIPHY_REG_H_ */
