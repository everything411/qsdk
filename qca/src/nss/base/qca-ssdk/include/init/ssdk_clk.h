/*
 * Copyright (c) 2017, 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

 #ifndef _SSDK_CLK_H_
#define _SSDK_CLK_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#define PPE_RESET_ID	"ppe_rst"
#define UNIPHY0_SOFT_RESET_ID	"uniphy0_soft_rst"
#define UNIPHY0_XPCS_RESET_ID	"uniphy0_xpcs_rst"
#define UNIPHY1_SOFT_RESET_ID	"uniphy1_soft_rst"
#define UNIPHY1_XPCS_RESET_ID	"uniphy1_xpcs_rst"
#define UNIPHY2_SOFT_RESET_ID	"uniphy2_soft_rst"
#define UNIPHY2_XPCS_RESET_ID	"uniphy2_xpcs_rst"
#define UNIPHY0_PORT1_DISABLE_ID	"uniphy0_port1_dis"
#define UNIPHY0_PORT2_DISABLE_ID	"uniphy0_port2_dis"
#define UNIPHY0_PORT3_DISABLE_ID	"uniphy0_port3_dis"
#define UNIPHY0_PORT4_DISABLE_ID	"uniphy0_port4_dis"
#define UNIPHY0_PORT5_DISABLE_ID	"uniphy0_port5_dis"
#define UNIPHY0_PORT_4_5_RESET_ID	"uniphy0_port_4_5_rst"
#define UNIPHY0_PORT_4_RESET_ID	"uniphy0_port_4_rst"
#define UNIPHY2_PORT_6_DISABLE_ID	"uniphy2_port6_dis"
#define UNIPHY0_SYS_RESET_ID	"uniphy0_sys_rst"
#define UNIPHY1_SYS_RESET_ID	"uniphy1_sys_rst"
#define UNIPHY2_SYS_RESET_ID	"uniphy2_sys_rst"
#define UNIPHY0_AHB_RESET_ID	"uniphy0_ahb_rst"
#define UNIPHY1_AHB_RESET_ID	"uniphy1_ahb_rst"
#define UNIPHY0_PMA_RESET_ID	"uniphy0_pma_rst"
#define UNIPHY1_PMA_RESET_ID	"uniphy1_pma_rst"
#define UNIPHY2_PMA_RESET_ID	"uniphy2_pma_rst"
#define UNIPHY_PORT1_RX_RESET_ID	"uniphy_port1_rx_rst"
#define UNIPHY_PORT1_TX_RESET_ID	"uniphy_port1_tx_rst"
#define UNIPHY_PORT2_RX_RESET_ID	"uniphy_port2_rx_rst"
#define UNIPHY_PORT2_TX_RESET_ID	"uniphy_port2_tx_rst"
#define UNIPHY_PORT3_RX_RESET_ID	"uniphy_port3_rx_rst"
#define UNIPHY_PORT3_TX_RESET_ID	"uniphy_port3_tx_rst"
#define UNIPHY_PORT4_RX_RESET_ID	"uniphy_port4_rx_rst"
#define UNIPHY_PORT4_TX_RESET_ID	"uniphy_port4_tx_rst"
#define UNIPHY_PORT5_RX_RESET_ID	"uniphy_port5_rx_rst"
#define UNIPHY_PORT5_TX_RESET_ID	"uniphy_port5_tx_rst"
#define UNIPHY_PORT6_RX_RESET_ID	"uniphy_port6_rx_rst"
#define UNIPHY_PORT6_TX_RESET_ID	"uniphy_port6_tx_rst"
#define PON_RESET_ID			"pon_rst"
#define UNIPHY1_XLGPCS_RESET_ID		"uniphy1_xlgpcs_rst"
#define UNIPHY2_XLGPCS_RESET_ID		"uniphy2_xlgpcs_rst"

#define SSDK_PORT1_RESET_ID	"nss_port1_rst"
#define SSDK_PORT2_RESET_ID	"nss_port2_rst"
#define SSDK_PORT3_RESET_ID	"nss_port3_rst"
#define SSDK_PORT4_RESET_ID	"nss_port4_rst"
#define SSDK_PORT5_RESET_ID	"nss_port5_rst"
#define SSDK_PORT6_RESET_ID	"nss_port6_rst"
#define SSDK_PORT1_RX_RESET_ID	"nss_port1_rx_rst"
#define SSDK_PORT1_TX_RESET_ID	"nss_port1_tx_rst"
#define SSDK_PORT2_RX_RESET_ID	"nss_port2_rx_rst"
#define SSDK_PORT2_TX_RESET_ID	"nss_port2_tx_rst"
#define SSDK_PORT3_RX_RESET_ID	"nss_port3_rx_rst"
#define SSDK_PORT3_TX_RESET_ID	"nss_port3_tx_rst"
#define SSDK_PORT4_RX_RESET_ID	"nss_port4_rx_rst"
#define SSDK_PORT4_TX_RESET_ID	"nss_port4_tx_rst"
#define SSDK_PORT5_RX_RESET_ID	"nss_port5_rx_rst"
#define SSDK_PORT5_TX_RESET_ID	"nss_port5_tx_rst"
#define SSDK_PORT6_RX_RESET_ID	"nss_port6_rx_rst"
#define SSDK_PORT6_TX_RESET_ID	"nss_port6_tx_rst"
#define EPHY_RX_RESET_ID	"ephy_rx_rst"
#define EPHY_TX_RESET_ID	"ephy_tx_rst"

#define SSDK_PORT1_MAC_RESET_ID	"nss_port1_mac_rst"
#define SSDK_PORT2_MAC_RESET_ID	"nss_port2_mac_rst"
#define SSDK_PORT3_MAC_RESET_ID	"nss_port3_mac_rst"
#define SSDK_PORT4_MAC_RESET_ID	"nss_port4_mac_rst"
#define SSDK_PORT5_MAC_RESET_ID	"nss_port5_mac_rst"
#define SSDK_PORT6_MAC_RESET_ID	"nss_port6_mac_rst"

#if defined(SSDK_RAW_CLOCK)
extern void __iomem *nsscc_clk_base_g;
#endif

enum ssdk_rst_action {
        SSDK_RESET_DEASSERT      = 0,
        SSDK_RESET_ASSERT        = 1
};

enum uniphy_rst_type {
	UNIPHY0_SOFT_RESET_E = 0,
	UNIPHY0_XPCS_RESET_E,
	UNIPHY1_SOFT_RESET_E,
	UNIPHY1_XPCS_RESET_E,
	UNIPHY2_SOFT_RESET_E,
	UNIPHY2_XPCS_RESET_E,
	UNIPHY0_PORT1_DISABLE_E,
	UNIPHY0_PORT2_DISABLE_E,
	UNIPHY0_PORT3_DISABLE_E,
	UNIPHY0_PORT4_DISABLE_E,
	UNIPHY0_PORT5_DISABLE_E,
	UNIPHY0_PORT_4_5_RESET_E,
	UNIPHY0_PORT_4_RESET_E,
	UNIPHY2_PORT6_DISABLE_E,
	UNIPHY0_SYS_RESET_E,
	UNIPHY1_SYS_RESET_E,
	UNIPHY2_SYS_RESET_E,
	UNIPHY0_AHB_RESET_E,
	UNIPHY1_AHB_RESET_E,
	UNIPHY_PORT1_RX_RESET_E,
	UNIPHY_PORT1_TX_RESET_E,
	UNIPHY_PORT2_RX_RESET_E,
	UNIPHY_PORT2_TX_RESET_E,
	UNIPHY_PORT3_RX_RESET_E,
	UNIPHY_PORT3_TX_RESET_E,
	UNIPHY_PORT4_RX_RESET_E,
	UNIPHY_PORT4_TX_RESET_E,
	UNIPHY_PORT5_RX_RESET_E,
	UNIPHY_PORT5_TX_RESET_E,
	UNIPHY_PORT6_RX_RESET_E,
	UNIPHY_PORT6_TX_RESET_E,
	PON_RESET_E,
	UNIPHY1_XLGPCS_RESET_E,
	UNIPHY2_XLGPCS_RESET_E,
	UNIPHY0_PMA_RESET_E,
	UNIPHY1_PMA_RESET_E,
	UNIPHY2_PMA_RESET_E,
	UNIPHY_RST_MAX
};

#define IM_SLEEP_CLK	"gcc_im_sleep_clk"
#define CMN_AHB_CLK		"cmn_ahb_clk"
#define CMN_SYS_CLK		"cmn_sys_clk"
#define UNIPHY0_AHB_CLK	"uniphy0_ahb_clk"
#define UNIPHY0_SYS_CLK	"uniphy0_sys_clk"
#define UNIPHY1_AHB_CLK	"uniphy1_ahb_clk"
#define UNIPHY1_SYS_CLK	"uniphy1_sys_clk"
#define UNIPHY2_AHB_CLK	"uniphy2_ahb_clk"
#define UNIPHY2_SYS_CLK	"uniphy2_sys_clk"
#define PORT1_MAC_CLK		"port1_mac_clk"
#define PORT2_MAC_CLK		"port2_mac_clk"
#define PORT3_MAC_CLK		"port3_mac_clk"
#define PORT4_MAC_CLK		"port4_mac_clk"
#define PORT5_MAC_CLK		"port5_mac_clk"
#define PORT6_MAC_CLK		"port6_mac_clk"
#define NSS_PPE_CLK		"nss_ppe_clk"
#define NSS_PPE_CFG_CLK	"nss_ppe_cfg_clk"
#define NSSNOC_PPE_CLK		"nssnoc_ppe_clk"
#define NSSNOC_PPE_CFG_CLK	"nssnoc_ppe_cfg_clk"
#define NSS_EDMA_CLK		"nss_edma_clk"
#define NSS_EDMA_CFG_CLK	"nss_edma_cfg_clk"
#define NSS_PPE_IPE_CLK		"nss_ppe_ipe_clk"
#define NSS_PPE_BTQ_CLK	"nss_ppe_btq_clk"
#define MDIO_AHB_CLK		"gcc_mdio_ahb_clk"
#define NSSNOC_CLK		"gcc_nss_noc_clk"
#define NSSNOC_SNOC_CLK	"gcc_nssnoc_snoc_clk"
#define MEM_NOC_NSSAXI_CLK	"gcc_mem_noc_nss_axi_clk"
#define CRYPTO_PPE_CLK		"gcc_nss_crypto_clk"
#define NSS_IMEM_CLK		"gcc_nss_imem_clk"
#define NSS_PTP_REF_CLK	"gcc_nss_ptp_ref_clk"
#define SNOC_NSSNOC_CLK	"gcc_snoc_nssnoc_clk"
#define NSSCC_CLK	"gcc_nsscc_clk"
#define NSSNOC_NSSCC_CLK	"gcc_nssnoc_nsscc_clk"
#define NSSNOC_SNOC_1_CLK	"gcc_nssnoc_snoc_1_clk"
#define NSS_CSR			"nss_csr_clk"
#define NSSNOC_NSS_CSR		"nssnoc_nss_csr_clk"
#define NSSCC_CE_APB		"nsscc_ce_apb_clk"
#define NSSCC_CE_AXI		"nsscc_ce_axi_clk"
#define NSSCC_NSSNOC_CE_APB	"nsscc_nssnoc_ce_apb_clk"
#define NSSCC_NSSNOC_CE_AXI	"nsscc_nssnoc_ce_axi_clk"
#define NSSCC_PON		"nsscc_pon_clk"

#define UNIPHY_AHB_CLK	"uniphy_ahb_clk"
#define UNIPHY_SYS_CLK	"uniphy_sys_clk"
#define MP_UNIPHY_SYS_CLK_RATE	24000000
#define MDIO0_AHB_CLK		"gcc_mdio0_ahb_clk"
#define MDIO1_AHB_CLK		"gcc_mdio1_ahb_clk"
#define GMAC0_CFG_CLK		"gcc_gmac0_cfg_clk"
#define GMAC0_SYS_CLK		"gcc_gmac0_sys_clk"
#define GMAC1_CFG_CLK		"gcc_gmac1_cfg_clk"
#define GMAC1_SYS_CLK		"gcc_gmac1_sys_clk"
#define SNOC_GMAC0_AHB_CLK		"gcc_snoc_gmac0_ahb_clk"
#define SNOC_GMAC1_AHB_CLK		"gcc_snoc_gmac1_ahb_clk"
#define GMAC0_PTP_CLK		"gcc_gmac0_ptp_clk"
#define GMAC1_PTP_CLK		"gcc_gmac1_ptp_clk"
#define GMAC_CLK_RATE		240000000

#define NSS_PORT1_RX_CLK	"nss_port1_rx_clk"
#define NSS_PORT1_TX_CLK	"nss_port1_tx_clk"
#define NSS_PORT2_RX_CLK	"nss_port2_rx_clk"
#define NSS_PORT2_TX_CLK	"nss_port2_tx_clk"
#define NSS_PORT3_RX_CLK	"nss_port3_rx_clk"
#define NSS_PORT3_TX_CLK	"nss_port3_tx_clk"
#define NSS_PORT4_RX_CLK	"nss_port4_rx_clk"
#define NSS_PORT4_TX_CLK	"nss_port4_tx_clk"
#define NSS_PORT5_RX_CLK	"nss_port5_rx_clk"
#define NSS_PORT5_TX_CLK	"nss_port5_tx_clk"
#define NSS_PORT6_RX_CLK	"nss_port6_rx_clk"
#define NSS_PORT6_TX_CLK	"nss_port6_tx_clk"
#define UNIPHY0_PORT1_RX_CLK	"uniphy0_port1_rx_clk"
#define UNIPHY0_PORT1_TX_CLK	"uniphy0_port1_tx_clk"
#define UNIPHY0_PORT2_RX_CLK	"uniphy0_port2_rx_clk"
#define UNIPHY0_PORT2_TX_CLK	"uniphy0_port2_tx_clk"
#define UNIPHY0_PORT3_RX_CLK	"uniphy0_port3_rx_clk"
#define UNIPHY0_PORT3_TX_CLK	"uniphy0_port3_tx_clk"
#define UNIPHY0_PORT4_RX_CLK	"uniphy0_port4_rx_clk"
#define UNIPHY0_PORT4_TX_CLK	"uniphy0_port4_tx_clk"
#define UNIPHY0_PORT5_RX_CLK	"uniphy0_port5_rx_clk"
#define UNIPHY0_PORT5_TX_CLK	"uniphy0_port5_tx_clk"
#define UNIPHY1_PORT5_RX_CLK	"uniphy1_port5_rx_clk"
#define UNIPHY1_PORT5_TX_CLK	"uniphy1_port5_tx_clk"
#define UNIPHY2_PORT6_RX_CLK	"uniphy2_port6_rx_clk"
#define UNIPHY2_PORT6_TX_CLK	"uniphy2_port6_tx_clk"
#define PORT5_RX_SRC		"nss_port5_rx_clk_src"
#define PORT5_TX_SRC		"nss_port5_tx_clk_src"
#define PORT4_RX_SRC		"nss_port4_rx_clk_src"
#define PORT4_TX_SRC		"nss_port4_tx_clk_src"
#define EPHY_RX_SRC_PARENT	"ephy_rx_clk_src_parent"
#define EPHY_TX_SRC_PARENT	"ephy_tx_clk_src_parent"
#define UNIPHY_PORT1_RX_CLK	"uniphy_port1_rx_clk"
#define UNIPHY_PORT1_TX_CLK	"uniphy_port1_tx_clk"
#define UNIPHY_PORT2_RX_CLK	"uniphy_port2_rx_clk"
#define UNIPHY_PORT2_TX_CLK	"uniphy_port2_tx_clk"
#define UNIPHY_PORT1_RX_DIV4_CLK	"uniphy_port1_rx_div4_clk"
#define UNIPHY_PORT1_TX_DIV4_CLK	"uniphy_port1_tx_div4_clk"
#define UNIPHY_PORT2_RX_DIV4_CLK	"uniphy_port2_rx_div4_clk"
#define UNIPHY_PORT2_TX_DIV4_CLK	"uniphy_port2_tx_div4_clk"
#define UNIPHY_PORT3_RX_DIV4_CLK	"uniphy_port3_rx_div4_clk"
#define UNIPHY_PORT3_TX_DIV4_CLK	"uniphy_port3_tx_div4_clk"
#define UNIPHY_PORT4_RX_DIV4_CLK	"uniphy_port4_rx_div4_clk"
#define UNIPHY_PORT4_TX_DIV4_CLK	"uniphy_port4_tx_div4_clk"

enum unphy_clk_type {
	NSS_PORT1_RX_CLK_E = 0,
	NSS_PORT1_TX_CLK_E,
	NSS_PORT2_RX_CLK_E,
	NSS_PORT2_TX_CLK_E,
	NSS_PORT3_RX_CLK_E,
	NSS_PORT3_TX_CLK_E,
	NSS_PORT4_RX_CLK_E,
	NSS_PORT4_TX_CLK_E,
	NSS_PORT5_RX_CLK_E,
	NSS_PORT5_TX_CLK_E,
	NSS_PORT6_RX_CLK_E,
	NSS_PORT6_TX_CLK_E,
	UNIPHY0_PORT1_RX_CLK_E,
	UNIPHY0_PORT1_TX_CLK_E,
	UNIPHY0_PORT2_RX_CLK_E,
	UNIPHY0_PORT2_TX_CLK_E,
	UNIPHY0_PORT3_RX_CLK_E,
	UNIPHY0_PORT3_TX_CLK_E,
	UNIPHY0_PORT4_RX_CLK_E,
	UNIPHY0_PORT4_TX_CLK_E,
	UNIPHY0_PORT5_RX_CLK_E,
	UNIPHY0_PORT5_TX_CLK_E,
	UNIPHY1_PORT5_RX_CLK_E,
	UNIPHY1_PORT5_TX_CLK_E,
	UNIPHY2_PORT6_RX_CLK_E,
	UNIPHY2_PORT6_TX_CLK_E,
	PORT5_RX_SRC_E,
	PORT5_TX_SRC_E,
	PORT4_RX_SRC_E,
	PORT4_TX_SRC_E,
	EPHY_RX_SRC_PARENT_E,
	EPHY_TX_SRC_PARENT_E,
	UNIPHY_PORT1_RX_DIV4_CLK_E,
	UNIPHY_PORT1_TX_DIV4_CLK_E,
	UNIPHY_PORT2_RX_DIV4_CLK_E,
	UNIPHY_PORT2_TX_DIV4_CLK_E,
	UNIPHY_PORT3_RX_DIV4_CLK_E,
	UNIPHY_PORT3_TX_DIV4_CLK_E,
	UNIPHY_PORT4_RX_DIV4_CLK_E,
	UNIPHY_PORT4_TX_DIV4_CLK_E,
	UNIPHY_CLK_MAX
};

enum cmnblk_clk_type {
	INTERNAL_48MHZ = 0,
	EXTERNAL_25MHZ,
	EXTERNAL_31250KHZ,
	EXTERNAL_40MHZ,
	EXTERNAL_48MHZ,
	EXTERNAL_50MHZ,
	INTERNAL_96MHZ,
};

enum cmnblk_pll_src_type {
	CMN_BLK_PLL_SRC_SEL_FROM_REG = 0,
	CMN_BLK_PLL_SRC_SEL_FROM_LOGIC = 1,
	CMN_BLK_PLL_SRC_SEL_FROM_PCS = 2,
};

enum mp_bcr_rst_type {
	GEPHY_BCR_RESET_E = 0,
	UNIPHY_BCR_RESET_E,
	GMAC0_BCR_RESET_E,
	GMAC1_BCR_RESET_E,
	GEPHY_MISC_RESET_E,
	MP_BCR_RST_MAX
};

#define GEHPY_BCR_RESET_ID	"gephy_bcr_rst"
#define UNIPHY_BCR_RESET_ID	"uniphy_bcr_rst"
#define GMAC0_BCR_RESET_ID	"gmac0_bcr_rst"
#define GMAC1_BCR_RESET_ID	"gmac1_bcr_rst"
#define GEPHY_MISC_RESET_ID	"gephy_misc_rst"

#define CMN_BLK_ADDR                0x0009B780
#define CMN_BLK_PLL_SRC_ADDR        0x0009B028
#define CMN_BLK_SIZE                0x100
#define PLL_CTRL_SRC_MASK           0xfffffcff
#define PLL_REFCLK_DIV_MASK         0xfffffe0f
#define PLL_REFCLK_DIV_2            0x20
#define FREQUENCY_MASK              0xfffffdf0
#define INTERNAL_48MHZ_CLOCK        0x7
#define EXTERNAL_25MHZ_CLOCK        0x203
#define EXTERNAL_31250KHZ_CLOCK     0x204
#define EXTERNAL_40MHZ_CLOCK        0x206
#define EXTERNAL_48MHZ_CLOCK        0x207
#define EXTERNAL_50MHZ_CLOCK        0x208
#define UNIPHY_AHB_CLK_RATE         100000000
#define UNIPHY_SYS_CLK_RATE         19200000
#define CPPE_UNIPHY_SYS_CLK_RATE    24000000
#define PPE_CLK_RATE                300000000
#define MDIO_AHB_RATE               100000000
#define NSS_NOC_RATE                461500000
#define NSSNOC_SNOC_RATE            266670000
#define NSS_IMEM_RATE               400000000
#define PTP_REF_RARE                150000000
#define NSS_AXI_RATE                461500000
#define NSS_HPPE_PORT5_DFLT_RATE    19200000
#define NSS_CPPE_PORT5_DFLT_RATE    24000000
#define NSS_APPE_PORT5_DFLT_RATE    24000000
#define NSS_NSSCC_CLK_RATE          100000000
#define NSS_NSSNOC_NSSCC_CLK_RATE   100000000
#define NSS_NSSNOC_SNOC_1_CLK_RATE  342857143
#define NSS_NSSNOC_SNOC_CLK_RATE    342857143
#define MPPE_NSS_NSSNOC_SNOC_1_CLK_RATE  266660000
#define MPPE_NSS_NSSNOC_SNOC_CLK_RATE    266660000
#define JHPPE_NSS_NSSNOC_SNOC_CLK_RATE   266660000

#define UNIPHY_CLK_RATE_25M         25000000
#define UNIPHY_CLK_RATE_50M         50000000
#define UNIPHY_CLK_RATE_125M        125000000
#define UNIPHY_CLK_RATE_312M        312500000
#define UNIPHY_CLK_RATE_781P25M     781250000
#define UNIPHY_DEFAULT_RATE         UNIPHY_CLK_RATE_125M

#define PQSGMII_SPEED_10M_CLK       2500000
#define PQSGMII_SPEED_100M_CLK      25000000
#define PQSGMII_SPEED_1000M_CLK     125000000
#define USXGMII_SPEED_10M_CLK       1250000
#define USXGMII_SPEED_100M_CLK      12500000
#define USXGMII_SPEED_1000M_CLK     125000000
#define USXGMII_SPEED_2500M_CLK     78125000
#define USXGMII_SPEED_5000M_CLK     156250000
#define USXGMII_SPEED_10000M_CLK    312500000
#define SGMII_PLUS_SPEED_2500M_CLK  312500000
#define SGMII_SPEED_10M_CLK         2500000
#define SGMII_SPEED_100M_CLK        25000000
#define SGMII_SPEED_1000M_CLK       125000000

#define CPPE_XGMAC_CLK_REG          0x0194900c
#define CPPE_XGMAC_CLK_SIZE         0x10
#define CPPE_XGMAC_CLK_ENABLE       0x20

#define APPE_UNIPHY_SYS_CLK_RATE    24000000
#define APPE_CLK_RATE               353000000
#define MPPE_CLK_RATE               200000000
#define MRPPE_CLK_RATE              375000000
#define JHPPE_CLK_RATE              462000000
#define HMSPPE_CLK_RATE             429000000

enum {
	UNIPHY_RX = 0,
	UNIPHY_TX,
};

void ssdk_uniphy_reset(
	a_uint32_t dev_id,
	enum uniphy_rst_type rst_type,
	a_uint32_t action);

void ssdk_ppe_port_clock_assert(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	a_uint32_t action);

void ssdk_ppe_mac_clock_assert(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	a_uint32_t action);

void ssdk_port_clock_assert(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	a_uint32_t action);

void
qca_gcc_mac_port_clock_set(a_uint32_t dev_id, a_uint32_t port_id,
                                a_bool_t enable);

void
qca_gcc_uniphy_port_clock_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
                                a_uint32_t port_id, a_bool_t enable);

int ssdk_gcc_clock_init(a_uint32_t dev_id);
void ssdk_gcc_clock_exit(a_uint32_t dev_id);

#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI) || defined(SSDK_PCIE_BUS)
void ssdk_pci_ppe_clock_init(a_uint32_t dev_id);
#endif

void
ssdk_port_speed_clock_set(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	a_uint32_t rate);
void ssdk_port_clock_reset(
	a_uint32_t dev_id,
	a_uint32_t port_id);
void ssdk_uniphy_clock_rate_set(
	a_uint32_t dev_id,
	enum unphy_clk_type clock_type,
	a_uint32_t rate);

void ssdk_ppe_reset_init(a_uint32_t dev_id);
void ssdk_uniphy_raw_clock_reset(a_uint32_t dev_id, a_uint8_t uniphy_index);
void ssdk_uniphy_raw_clock_set(
		a_uint32_t dev_id,
		a_uint8_t uniphy_index,
		a_uint8_t direction,
		a_uint32_t clock);
void ssdk_gcc_uniphy_sys_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
	a_bool_t enable);
void ssdk_uniphy_port5_clock_source_set(a_uint32_t dev_id);

void ssdk_appe_port_speed_clock_set(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	a_uint32_t *clk);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SSDK_CLK_H */
