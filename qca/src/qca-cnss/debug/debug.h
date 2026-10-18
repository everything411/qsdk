/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CNSS_DEBUG_H
#define _CNSS_DEBUG_H

#if IS_ENABLED(CONFIG_IPC_LOGGING)
#include <linux/ipc_logging.h>
#endif
#include <linux/printk.h>

#define CNSS_IPC_LOG_PAGES		32

#define PER_CE_REG_SIZE			0x2000

#define DEFAULT_CE_COUNT	12
#define QCN9224_CE_COUNT	16

#define QCA8074_CE_SRC_RING_REG_BASE		0xA00000
#define QCA8074_CE_DST_RING_REG_BASE		0xA01000
#define QCA8074_CE_COMMON_REG_BASE		0xA18000

#define QCA5018_CE_SRC_RING_REG_BASE		0x8400000
#define QCA5018_CE_DST_RING_REG_BASE		0x8401000
#define QCA5018_CE_COMMON_REG_BASE		0x8418000

#define QCN9000_CE_SRC_RING_REG_BASE		0x1B80000
#define QCN9000_CE_DST_RING_REG_BASE		0x1B81000
#define QCN9000_CE_COMMON_REG_BASE		0x1B98000

#define QCN6122_CE_SRC_RING_REG_BASE		0x3B80000
#define QCN6122_CE_DST_RING_REG_BASE		0x3B81000
#define QCN6122_CE_COMMON_REG_BASE		0x3B98000
#define QCN6122_PCI_MHIREGLEN_REG		0x3E0E100
#define QCN6122_PCI_MHI_REGION_END		0x3E0EFFC

#define QCA5332_CE_SRC_RING_REG_BASE            0x740000
#define QCA5332_CE_DST_RING_REG_BASE            0x741000
#define QCA5332_CE_COMMON_REG_BASE              0x758000

#define QCA5424_CE_SRC_RING_REG_BASE            0x200000
#define QCA5424_CE_DST_RING_REG_BASE            0x201000
#define QCA5424_CE_COMMON_REG_BASE              0x218000

#define QCN9160_CE_SRC_RING_REG_BASE            0x3B80000
#define QCN9160_CE_DST_RING_REG_BASE            0x3B81000
#define QCN9160_CE_COMMON_REG_BASE              0x3B98000
#define QCN9160_PCI_MHIREGLEN_REG		0x3E0E100
#define QCN9160_PCI_MHI_REGION_END		0x3E0EFFC

#define QCN6432_CE_SRC_RING_REG_BASE            0x3B80000
#define QCN6432_CE_DST_RING_REG_BASE            0x3B81000
#define QCN6432_CE_COMMON_REG_BASE              0x3B98000

#define CE_SRC_RING_BASE_LSB_OFFSET		0x0
#define CE_SRC_RING_BASE_MSB_OFFSET		0x4
#define CE_SRC_RING_ID_OFFSET			0x8
#define CE_SRC_RING_MISC_OFFSET			0x10
#define CE_SRC_RING_SETUP_IX0_OFFSET		0x30
#define CE_SRC_RING_SETUP_IX1_OFFSET		0x34
#define CE_SRC_RING_MSI1_BASE_LSB_OFFSET	0x48
#define CE_SRC_RING_MSI1_BASE_MSB_OFFSET	0x4C
#define CE_SRC_RING_MSI1_DATA_OFFSET		0x50
#define CE_SRC_CTRL_OFFSET			0x58
#define CE_SRC_R0_CE_CH_SRC_IS_OFFSET		0x5C
#define CE_SRC_RING_HP_OFFSET			0x400
#define CE_SRC_RING_TP_OFFSET			0x404

#define CE_DEST_RING_BASE_LSB_OFFSET		0x0
#define CE_DEST_RING_BASE_MSB_OFFSET		0x4
#define CE_DEST_RING_ID_OFFSET			0x8
#define CE_DEST_RING_MISC_OFFSET		0x10
#define CE_DEST_RING_SETUP_IX0_OFFSET		0x30
#define CE_DEST_RING_SETUP_IX1_OFFSET		0x34
#define CE_DEST_RING_MSI1_BASE_LSB_OFFSET	0x48
#define CE_DEST_RING_MSI1_BASE_MSB_OFFSET	0x4C
#define CE_DEST_RING_MSI1_DATA_OFFSET		0x50
#define CE_DEST_CTRL_OFFSET			0xB0
#define CE_CH_DST_IS_OFFSET			0xB4
#define CE_CH_DEST_CTRL2_OFFSET			0xB8
#define CE_DEST_RING_HP_OFFSET			0x400
#define CE_DEST_RING_TP_OFFSET			0x404

#define CE_STATUS_RING_BASE_LSB_OFFSET	0x58
#define CE_STATUS_RING_BASE_MSB_OFFSET	0x5C
#define CE_STATUS_RING_ID_OFFSET	0x60
#define CE_STATUS_RING_MISC_OFFSET	0x68
#define CE_STATUS_RING_HP_OFFSET	0x408
#define CE_STATUS_RING_TP_OFFSET	0x40C

#define CE_COMMON_GXI_ERR_INTS		0x14
#define CE_COMMON_GXI_ERR_STATS		0x18
#define CE_COMMON_GXI_WDOG_STATUS	0x2C
#define CE_COMMON_TARGET_IE_0		0x48
#define CE_COMMON_TARGET_IE_1		0x4C

#define QCN9224_PCI_MHIREGLEN_REG		0x1E0E100
#define QCN9224_PCI_MHI_REGION_END		0x1E0EFFC
#define QCN9224_CE_COMMON_REG_BASE		0x1BA0000
#define QCN9224_PCIE_PCIE_LOCAL_REG_REMAP_BAR_CTRL	0x310C
#define QCN9224_WLAON_SOC_RESET_CAUSE_SHADOW_REG	0x1F80718
#define QCN9224_PCIE_PCIE_PARF_LTSSM			0x1E081B0
#define QCN9224_GCC_RAMSS_CBCR				0x1E38200
#define PCIE_PCIE_PARF_PM_STTS				0x1E08024
#define PCIE_CFG_PCIE_STATUS			0x230

#define QCN9224_SNOC_ERL_ErrVld_Low		0x1E80010
#define QCN9224_SNOC_ERL_ErrLog0_Low		0x1E80020
#define QCN9224_SNOC_ERL_ErrLog0_High		0x1E80024
#define QCN9224_SNOC_ERL_ErrLog1_Low		0x1E80028
#define QCN9224_SNOC_ERL_ErrLog1_High		0x1E8002C
#define QCN9224_SNOC_ERL_ErrLog2_Low		0x1E80030
#define QCN9224_SNOC_ERL_ErrLog2_High		0x1E80034
#define QCN9224_SNOC_ERL_ErrLog3_Low		0x1E80038
#define QCN9224_SNOC_ERL_ErrLog3_High		0x1E8003C
#define QCN9224_PCNOC_ERL_ErrVld_Low		0x1F00010
#define QCN9224_PCNOC_ERL_ErrLog0_Low		0x1F00020
#define QCN9224_PCNOC_ERL_ErrLog0_High		0x1F00024
#define QCN9224_PCNOC_ERL_ErrLog1_Low		0x1F00028
#define QCN9224_PCNOC_ERL_ErrLog1_High		0x1F0002C
#define QCN9224_PCNOC_ERL_ErrLog2_Low		0x1F00030
#define QCN9224_PCNOC_ERL_ErrLog2_High		0x1F00034
#define QCN9224_PCNOC_ERL_ErrLog3_Low		0x1F00038
#define QCN9224_PCNOC_ERL_ErrLog3_High		0x1F0003C

#define SBL_LOG_SIZE_MASK			0xFFFF
#define QCN9000_SRAM_START			0x01400000
#define QCN9000_SRAM_SIZE			0x003A0000
#define QCN9000_SRAM_END \
			(QCN9000_SRAM_START + QCN9000_SRAM_SIZE - 1)
#define QCN9000_PCIE_BHI_ERRDBG2_REG		0x1E0B238
#define QCN9000_PCIE_BHI_ERRDBG3_REG		0x1E0B23C
#define QCN9000_PCI_MHIREGLEN_REG		0x1E0B100
#define QCN9000_PCI_MHI_REGION_END		0x1E0BFFC

#define QCN9000_PBL_LOG_SRAM_START		0x1403d90
#define QCN9000_PBL_LOG_SRAM_MAX_SIZE		40
#define QCN9000_PBL_LOG_SRAM_START_V1		0x14061b8
#define QCN9000_PBL_LOG_SRAM_MAX_SIZE_V1	60
#define QCN9000_TCSR_PBL_LOGGING_REG		0x01B000F8
#define QCN9000_PBL_WLAN_BOOT_CFG		0x1E22B34
#define QCN9000_PBL_BOOTSTRAP_STATUS		0x01910008

#define QCN9224_SRAM_START			0x01300000
#define QCN9224_SRAM_SIZE			0x005A8000
#define QCN9224_SRAM_END \
			(QCN9224_SRAM_START + QCN9224_SRAM_SIZE - 1)

#define QCN9224_PCIE_BHI_ERRDBG2_REG		0x1E0E238
#define QCN9224_PCIE_BHI_ERRDBG3_REG		0x1E0E23C
#define QCN9224_PBL_LOG_SRAM_START		0x01303da0
#define QCN9224_v2_PBL_LOG_SRAM_START		0x01303e98
#define QCN9224_PBL_LOG_SRAM_MAX_SIZE		40
#define QCN9224_TCSR_PBL_LOGGING_REG		0x1B00094
#define QCN9224_PBL_WLAN_BOOT_CFG		0x1E22B34
#define QCN9224_PBL_BOOTSTRAP_STATUS		0x1A006D4
#define MAX_PBL_DATA_SNAPSHOT			2

#define PCIE_PCI_MSI_CAP_ID_NEXT_CTRL_REG	0x50
#define PCIE_MSI_CAP_OFF_04H_REG		0x54
#define PCIE_MSI_CAP_OFF_08H_REG		0x58
#define PCIE_MSI_CAP_OFF_0CH_REG		0x5C

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
#define QMI_WLANFW_QDSS_STOP_ALL_TRACE_LI 0x3F
#define QMI_WLANFW_QDSS_STOP_ALL_TRACE_BE 0x01

enum cnss_qdss_ops {
	CNSS_QDSS_STOP,
	CNSS_QDSS_START,
	CNSS_INVALID_OP,
};
#endif

struct cnss_pci_data;

struct cnss_ce_base_addr {
	u32 src_base;
	u32 dst_base;
	u32 common_base;
	u32 max_ce_count;
};

struct pbl_reg_addr {
	u32 pbl_log_sram_start;
	u32 pbl_log_sram_max_size;
	u32 pbl_log_sram_start_v1;
	u32 pbl_log_sram_max_size_v1;
	u32 tcsr_pbl_logging_reg;
	u32 pbl_wlan_boot_cfg;
	u32 pbl_bootstrap_status;
};

struct sbl_reg_addr {
	u32 sbl_sram_start;
	u32 sbl_sram_end;
	u32 sbl_log_start_reg;
	u32 sbl_log_size_reg;
	u32 sbl_log_size_shift;
};

struct noc_err_table {
	char *reg_name;
	unsigned long reg;
	int (*reg_handler)(struct cnss_plat_data *plat_priv, u32 addr,
				     u32 *val);
};

struct pbl_err_data {
	u32 *pbl_vals;
	u32 *pbl_reg_tbl;
	u32 pbl_tbl_len;
};

struct dump_pbl_sbl_data {
	u32 pbl_stage;
	u32 sbl_log_start;
	u32 pbl_wlan_boot_cfg;
	u32 pbl_bootstrap_status;
	u32 remap_bar_ctrl;
	u32 soc_rc_shadow_reg;
	u32 parf_ltssm;
	u32 parf_pm_stts;
	u32 gcc_ramss_cbcr;
	u32 pcie_cfg_pcie_status;
	u32 *sbl_vals;
	u32 sbl_len;
	u32 *noc_vals;
	u16 type0_status_cmd_reg;
	u16 pci_msi_cap_id_next_ctrl_reg;
	u16 pci_msi_cap_off_04h_reg;
	u16 pci_msi_cap_off_08h_reg;
	u16 pci_msi_cap_off_0ch_reg;
	struct pbl_err_data pbl_data[MAX_PBL_DATA_SNAPSHOT];
};

extern struct dentry *cnss_root_dentry;
extern const struct file_operations cnss_mlo_config_ini_debug_fops;

struct cnss_ce_base_addr *register_ce_object(struct cnss_plat_data *plat_priv);
int cnss_get_mhi_region_len(struct cnss_plat_data *plat_priv,
				   u32 *reg_start, u32 *reg_end);
void cnss_pci_dump_bl_sram_mem(struct cnss_pci_data *pci_priv);
bool cnss_wait_for_rddm_complete(struct cnss_plat_data *plat_priv);
int cnss_debug_init(void);
void cnss_debug_deinit(void);
int cnss_create_debug_only_node(struct cnss_plat_data *plat_priv);

#endif /* _CNSS_DEBUG_H */
