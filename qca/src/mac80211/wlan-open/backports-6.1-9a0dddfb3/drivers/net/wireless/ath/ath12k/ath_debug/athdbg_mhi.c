// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include "athdbg_core.h"
#include <linux/mhi.h>
#include "athdbg_mhi.h"
#include "../athdbg_if.h"
#include "../mhi.h"
#include "../pci.h"
#include "athdbg_hw.h"

#ifndef CONFIG_UPSTREAM_BUILD

extern struct ath_debug_base *athdbg_base;

static const struct athdbg_mhi_q6_noc_err_reg qcn9274_noc_err_regs[] = {
	{"SNOC_ERL_ErrVld_Low", QCN9224_SNOC_ERL_ErrVld_Low},
	{"SNOC_ERL_ErrLog0_Low", QCN9224_SNOC_ERL_ErrLog0_Low},
	{"SNOC_ERL_ErrLog0_High", QCN9224_SNOC_ERL_ErrLog0_High},
	{"SNOC_ERL_ErrLog1_Low", QCN9224_SNOC_ERL_ErrLog1_Low},
	{"SNOC_ERL_ErrLog1_High", QCN9224_SNOC_ERL_ErrLog1_High},
	{"SNOC_ERL_ErrLog2_Low", QCN9224_SNOC_ERL_ErrLog2_Low},
	{"SNOC_ERL_ErrLog2_High", QCN9224_SNOC_ERL_ErrLog2_High},
	{"SNOC_ERL_ErrLog3_Low", QCN9224_SNOC_ERL_ErrLog3_Low},
	{"SNOC_ERL_ErrLog3_High", QCN9224_SNOC_ERL_ErrLog3_High},
	{"PCNOC_ERL_ErrVld_Low", QCN9224_PCNOC_ERL_ErrVld_Low},
	{"PCNOC_ERL_ErrLog0_Low", QCN9224_PCNOC_ERL_ErrLog0_Low},
	{"PCNOC_ERL_ErrLog0_High", QCN9224_PCNOC_ERL_ErrLog0_High},
	{"PCNOC_ERL_ErrLog1_Low", QCN9224_PCNOC_ERL_ErrLog1_Low},
	{"PCNOC_ERL_ErrLog1_High", QCN9224_PCNOC_ERL_ErrLog1_High},
	{"PCNOC_ERL_ErrLog2_Low", QCN9224_PCNOC_ERL_ErrLog2_Low},
	{"PCNOC_ERL_ErrLog2_High", QCN9224_PCNOC_ERL_ErrLog2_High},
	{"PCNOC_ERL_ErrLog3_Low", QCN9224_PCNOC_ERL_ErrLog3_Low},
	{"PCNOC_ERL_ErrLog3_High", QCN9224_PCNOC_ERL_ErrLog3_High},
};

static inline u32 athdbg_mhi_pci_read32(struct ath12k_base *ab, u32 offset)
{
	const struct athdbg_to_ath12k_ops *ops = athdbg_base->dbg_to_ath_ops;

	if (!ops || !ops->pci_read32)
		return 0;

	return ops->pci_read32(ab, offset);
}

/**
 * athdbg_mhi_read_pbl_ram_info - Read PBL RAM information from TCSR_DEBUG_REG0
 * @ab: structure of ath12k base
 * @pbl_ram_start: pointer to store PBL RAM start address
 * @pbl_ram_size: pointer to store PBL RAM size
 *
 * Read PBL RAM information from TCSR_DEBUG_REG0 register. This register points
 * to a structure that contains the magic cookie, error region size, and error
 * region base address. If the magic cookie matches, use the error region base
 * and size as the PBL RAM start address and size. Otherwise, use default values.
 *
 * Return: 0 on success, negative error code on failure
 */
static int athdbg_mhi_read_pbl_ram_info(struct ath12k_base *ab,
					u32 *pbl_ram_start, u32 *pbl_ram_size)
{
	u32 tcsr_debug_reg0_val = 0;
	struct pbl_err_to_host_type pbl_err_info = {0};

	if (!ab) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	/* Read the value from TCSR_DEBUG_REG0 */
	tcsr_debug_reg0_val = athdbg_mhi_pci_read32(ab, TCSR_DEBUG_REG0);
	if (!tcsr_debug_reg0_val) {
		pr_err("Failed to read TCSR_DEBUG_REG0\n");
		return tcsr_debug_reg0_val;
	}

	/* Read the pbl_err_to_host_type structure using sizeof(u32) for field offsets */
	pbl_err_info.magic_cookie = athdbg_mhi_pci_read32(ab, tcsr_debug_reg0_val);
	if (!pbl_err_info.magic_cookie) {
		pr_err("Failed to read magic_cookie\n");
		return -EINVAL;
	}

	/* Verify magic cookie */
	if (pbl_err_info.magic_cookie != PBL_ERR_MAGIC_COOKIE) {
		pr_err("Invalid magic cookie: 0x%x, expected: 0x%x\n",
		       pbl_err_info.magic_cookie, PBL_ERR_MAGIC_COOKIE);
		return -EINVAL;
	}

	*pbl_ram_size = athdbg_mhi_pci_read32(ab, tcsr_debug_reg0_val + sizeof(u32));
	if (*pbl_ram_size == 0) {
		pr_err("Failed to read err_region_size\n");
		return -EINVAL;
	}

	*pbl_ram_start = athdbg_mhi_pci_read32(ab,
					       tcsr_debug_reg0_val + (2 * sizeof(u32)));
	if (*pbl_ram_start == 0) {
		pr_err("Failed to read err_region_base\n");
		return -EINVAL;
	}

	pbl_err_info.err_region_size = *pbl_ram_size;
	pbl_err_info.err_region_base = *pbl_ram_start;

	pr_debug("Using PBL RAM info from TCSR_DEBUG_REG0: start=0x%x, size=%u\n",
		 *pbl_ram_start, *pbl_ram_size);

	return 0;
}

static inline struct ath12k_pci *athdbg_mhi_get_pci_priv(
						struct ath12k_base *ab)
{
	const struct athdbg_to_ath12k_ops *ops = athdbg_base->dbg_to_ath_ops;

	if (!ops || !ops->pci_get_priv)
		return NULL;

	return ops->pci_get_priv(ab);
}


static inline unsigned long *athdbg_mhi_get_mhi_state(
						struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci;

	ab_pci = athdbg_mhi_get_pci_priv(ab);
	if (!ab_pci)
		return NULL;

	return &ab_pci->mhi_state;
}

static inline void *athdbg_mhi_get_mhi_ctrl(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci;

	ab_pci = athdbg_mhi_get_pci_priv(ab);
	if (!ab_pci)
		return NULL;

	return ab_pci->mhi_ctrl;
}

static inline void *athdbg_mhi_get_pci_dev(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci;

	ab_pci = athdbg_mhi_get_pci_priv(ab);
	if (!ab_pci)
		return NULL;

	return ab_pci->pdev;
}

static bool athdbg_mhi_q6_scan_rddm_cookie(struct ath12k_base *ab,
					    u32 cookie)
{
	void *mhi_ctrl;

	mhi_ctrl = athdbg_mhi_get_mhi_ctrl(ab);
	if (!mhi_ctrl)
		return false;
#ifndef PLATFORM_SDX
	return mhi_scan_rddm_cookie(mhi_ctrl, cookie);
#endif
	return true;
}

static void athdbg_mhi_q6_debug_reg_dump(struct ath12k_base *ab)
{
	void *mhi_ctrl;

	mhi_ctrl = athdbg_mhi_get_mhi_ctrl(ab);
	if (!mhi_ctrl)
		return;

#ifndef PLATFORM_SDX
	mhi_debug_reg_dump(mhi_ctrl);
#endif
}

static int athdbg_mhi_q6_debug_read_pbl_data(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_reg_addr *pbl_data,
					struct athdbg_mhi_q6_pbl_err_data *pbl_err_data)
{
	unsigned long *mhi_state;
	u32 log_size_v1 = pbl_data->pbl_log_sram_max_size_v1;
	u32 log_size = pbl_data->pbl_log_sram_max_size;
	u32 total_size = log_size + log_size_v1;
	int gfp = GFP_KERNEL;
	u32 *mem_addr = NULL;
	u32 *buf = NULL;
	int i = 0;
	int j = 0;

	mhi_state = athdbg_mhi_get_mhi_state(ab);
	if (!mhi_state)
		return -EINVAL;

	if (test_bit(ATH12K_MHI_MISSION_MODE, mhi_state)) {
		pr_err("skip Q6 PBL/SBL logging as device is in MISSION mode\n");
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	buf = kcalloc(total_size, sizeof(u32), gfp);
	if (!buf)
		return -ENOMEM;

	mem_addr = kcalloc(total_size, sizeof(u32), gfp);
	if (!mem_addr) {
		kfree(buf);
		return -ENOMEM;
	}

	for (i = 0, j = 0; i < log_size; i += sizeof(u32), j++) {
		mem_addr[j] = pbl_data->pbl_log_sram_start + i;
		buf[j] = athdbg_mhi_pci_read32(ab, mem_addr[j]);
	}

	for (i = 0; i < log_size_v1; i += sizeof(u32), j++) {
		mem_addr[j] = pbl_data->pbl_log_sram_start_v1 + i;
		buf[j] = athdbg_mhi_pci_read32(ab, mem_addr[j]);
	}

	pbl_err_data->pbl_tbl_len = j;
	pbl_err_data->pbl_vals = buf;
	pbl_err_data->pbl_reg_tbl = mem_addr;

	return 0;
}

static int athdbg_mhi_q6_debug_read_sbl_data(struct ath12k_base *ab,
					u32 log_size, u32 sram_start_reg,
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	int i = 0;
	int j = 0;
	int gfp = GFP_KERNEL;
	u32 mem_addr = 0;
	u32 *buf = NULL;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	buf = kcalloc(log_size, sizeof(u32), gfp);
	if (!buf)
		return -ENOMEM;

	for (i = 0, j = 0; i < log_size; i += sizeof(u32), j++) {
		mem_addr = sram_start_reg + i;
		buf[j] = athdbg_mhi_pci_read32(ab, mem_addr);
		if (buf[j] == 0)
			break;
	}
	pbl_sbl_err->sbl_vals = buf;
	pbl_sbl_err->sbl_len = i;

	return 0;
}

static int athdbg_mhi_q6_debug_read_noc_errors(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_sbl_data *pbl_sbl_err)
{
	unsigned long *mhi_state;
	int i;
	u32 *buf = NULL;
	int gfp = GFP_KERNEL;
	size_t len = pbl_sbl_err->noc_len;

	mhi_state = athdbg_mhi_get_mhi_state(ab);
	if (!mhi_state)
		return -EINVAL;

	if (test_bit(ATH12K_MHI_MISSION_MODE, mhi_state)) {
		pr_err("skip Q6 PBL/SBL logging as device is in MISSION mode\n");
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	buf = kcalloc(len, sizeof(u32), gfp);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < len; i++)
		buf[i] = athdbg_mhi_pci_read32(ab, pbl_sbl_err->noc_tbl[i].reg_offset);

	pbl_sbl_err->noc_vals = buf;

	return 0;
}

static int athdbg_mhi_q6_debug_read_misc_data(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_reg_addr *pbl_data,
					struct athdbg_mhi_q6_sbl_reg_addr *sbl_data,
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	struct pci_dev *pci_dev;

	pci_dev = athdbg_mhi_get_pci_dev(ab);
	if (!pci_dev)
		return -EINVAL;

	pbl_sbl_err->parf_pm_stts = athdbg_mhi_pci_read32(ab, PCIE_PCIE_PARF_PM_STTS);
	pci_read_config_word(pci_dev, PCI_COMMAND,
			     &pbl_sbl_err->type0_status_cmd_reg);

	pci_read_config_word(pci_dev,
			     PCIE_PCI_MSI_CAP_ID_NEXT_CTRL_REG,
			     &pbl_sbl_err->pci_msi_cap_id_next_ctrl_reg);
	pci_read_config_word(pci_dev, PCIE_MSI_CAP_OFF_04H_REG,
			     &pbl_sbl_err->pci_msi_cap_off_04h_reg);
	pci_read_config_word(pci_dev, PCIE_MSI_CAP_OFF_08H_REG,
			     &pbl_sbl_err->pci_msi_cap_off_08h_reg);
	pci_read_config_word(pci_dev, PCIE_MSI_CAP_OFF_0CH_REG,
			     &pbl_sbl_err->pci_msi_cap_off_0ch_reg);

	switch (pci_dev->device) {
	case QCN9274_DEVICE_ID:
		pbl_sbl_err->remap_bar_ctrl = athdbg_mhi_pci_read32(ab,
				QCN9224_PCIE_PCIE_LOCAL_REG_REMAP_BAR_CTRL);
		pbl_sbl_err->soc_rc_shadow_reg = athdbg_mhi_pci_read32(ab,
				QCN9224_WLAON_SOC_RESET_CAUSE_SHADOW_REG);
		pbl_sbl_err->parf_ltssm = athdbg_mhi_pci_read32(ab,
				QCN9224_PCIE_PCIE_PARF_LTSSM);
		pbl_sbl_err->gcc_ramss_cbcr = athdbg_mhi_pci_read32(ab,
				QCN9224_GCC_RAMSS_CBCR);
		break;
	case QCN9625_DEVICE_ID:
	case QCN9589_DEVICE_ID:
		pbl_sbl_err->remap_bar_ctrl = athdbg_mhi_pci_read32(ab,
				QCN9625_PCIE_PCIE_LOCAL_REG_REMAP_BAR_CTRL);
		pbl_sbl_err->soc_rc_shadow_reg = athdbg_mhi_pci_read32(ab,
				QCN9224_WLAON_SOC_RESET_CAUSE_SHADOW_REG);
		pbl_sbl_err->parf_ltssm = athdbg_mhi_pci_read32(ab,
				QCN9224_PCIE_PCIE_PARF_LTSSM);
		pbl_sbl_err->gcc_ramss_cbcr = athdbg_mhi_pci_read32(ab,
				QCN9625_GCC_RAMSS_CBCR);
		break;
	default:
		break;
	}

	pbl_sbl_err->sbl_log_start = athdbg_mhi_pci_read32(ab,
							   sbl_data->sbl_log_start_reg);
	pbl_sbl_err->pbl_stage = athdbg_mhi_pci_read32(ab,
							  pbl_data->tcsr_pbl_logging_reg);
	pbl_sbl_err->pbl_wlan_boot_cfg = athdbg_mhi_pci_read32(ab,
							  pbl_data->pbl_wlan_boot_cfg);
	pbl_sbl_err->pbl_bootstrap_status = athdbg_mhi_pci_read32(ab,
							  pbl_data->pbl_bootstrap_status);

	return 0;
}

static void athdbg_mhi_q6_debug_collect_bl_data(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_reg_addr *pbl_data,
					struct athdbg_mhi_q6_sbl_reg_addr *sbl_data,
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	unsigned long *mhi_state;
	u32 sbl_log_size = 0;
	u32 sbl_log_start;

	mhi_state = athdbg_mhi_get_mhi_state(ab);
	if (!mhi_state)
		return;

	if (test_bit(ATH12K_MHI_MISSION_MODE, mhi_state)) {
		pr_err("skip Q6 PBL/SBL logging as device is in MISSION mode\n");
		return;
	}

	/* Dump SRAM content twice before and after the register dump as
	 * it is needed for Q6 debugging
	 */

	if (athdbg_mhi_q6_debug_read_pbl_data(ab, pbl_data,
					      &pbl_sbl_err->pbl_data[0])) {
		pr_err("Failed to read Q6 PBL log data\n");
		return;
	}

	if (athdbg_mhi_q6_debug_read_misc_data(ab, pbl_data, sbl_data,
					       pbl_sbl_err)) {
		pr_err("Failed to read Q6 Misc log data\n");
		return;
	}

	/* Read NOC errors */
	{
		struct pci_dev *pci_dev = athdbg_mhi_get_pci_dev(ab);

		if (pci_dev) {
			switch (pci_dev->device) {
			case QCN9274_DEVICE_ID:
			case QCN9625_DEVICE_ID:
			case QCN9589_DEVICE_ID:
				pbl_sbl_err->noc_tbl = qcn9274_noc_err_regs;
				pbl_sbl_err->noc_len = ARRAY_SIZE(qcn9274_noc_err_regs);

				if (athdbg_mhi_q6_debug_read_noc_errors(ab, pbl_sbl_err))
					pr_err("Failed to read Q6 NOC error data\n");
				break;
			default:
				break;
			}
		}
	}

	if (athdbg_mhi_q6_debug_read_pbl_data(ab, pbl_data,
					      &pbl_sbl_err->pbl_data[1])) {
		pr_err("Failed to read Q6 PBL log data\n");
		return;
	}

	sbl_log_size = athdbg_mhi_pci_read32(ab, sbl_data->sbl_log_size_reg);

	if (!sbl_log_size) {
		pr_err("Invalid Q6 SBL log size\n");
		return;
	}

	sbl_log_start = pbl_sbl_err->sbl_log_start;
	sbl_log_size = ((sbl_log_size >> sbl_data->sbl_log_size_shift) &
			0xFFFF);

	if (sbl_log_start < sbl_data->sbl_sram_start ||
	    sbl_log_start > sbl_data->sbl_sram_end ||
	    (sbl_log_start + sbl_log_size) > sbl_data->sbl_sram_end) {
		pr_err("Invalid Q6 SBL log data\n");
		return;
	}

	if (athdbg_mhi_q6_debug_read_sbl_data(ab, sbl_log_size, sbl_log_start,
					      pbl_sbl_err))
		pr_err("Failed to read Q6 SBL log data\n");
}

static void athdbg_mhi_q6_debug_print_pbl_data(struct ath12k_base *ab,
					    struct athdbg_mhi_q6_pbl_err_data *pbl_data)
{
	int i;

	pr_info("Dumping Q6 PBL log data\n");
	for (i = 0; i < pbl_data->pbl_tbl_len; i++)
		pr_info("PBL: SRAM[0x%x] = 0x%x\n",
			   pbl_data->pbl_reg_tbl[i],
			   pbl_data->pbl_vals[i]);
}

static void athdbg_mhi_q6_debug_print_sbl_data(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	pr_info("Dumping Q6 SBL log data\n");
	print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4,
		       pbl_sbl_err->sbl_vals, pbl_sbl_err->sbl_len, 1);
}

static void athdbg_mhi_q6_debug_print_noc_data(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	int i;

	if (!pbl_sbl_err->noc_vals || !pbl_sbl_err->noc_len)
		return;

	pr_info("Dumping Q6 NOC error log data\n");
	for (i = 0; i < pbl_sbl_err->noc_len; i++)
		pr_info("%s: 0x%08x\n", pbl_sbl_err->noc_tbl[i].reg_name,
			   pbl_sbl_err->noc_vals[i]);
}

static void athdbg_mhi_q6_debug_print_bl_data(struct ath12k_base *ab,
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	pr_info("PARF_PM_STTS: 0x%08x, PCIE_TYPE0_STATUS_COMMAND_REG: 0x%08x\n",
		   pbl_sbl_err->parf_pm_stts,
		   pbl_sbl_err->type0_status_cmd_reg);

	pr_info("PCI_MSI_CAP_ID_NEXT_CTRL_REG: 0x%08x, MSI_CAP_OFF_04H_REG: 0x%08x\n",
		   pbl_sbl_err->pci_msi_cap_id_next_ctrl_reg,
		   pbl_sbl_err->pci_msi_cap_off_04h_reg);
	pr_info("PCIE_MSI_CAP_OFF_08H_REG: 0x%08x, PCIE_MSI_CAP_OFF_0CH_REG: 0x%08x\n",
		   pbl_sbl_err->pci_msi_cap_off_08h_reg,
		   pbl_sbl_err->pci_msi_cap_off_0ch_reg);

	athdbg_mhi_q6_debug_print_pbl_data(ab, &pbl_sbl_err->pbl_data[0]);

	pr_info("LOCAL_REG_REMAP_BAR_CTRL: 0x%08x\n", pbl_sbl_err->remap_bar_ctrl);
	pr_info("WLAON_SOC_RESET_CAUSE_SHADOW_REG: 0x%08x, PARF_LTSSM: 0x%08x\n",
		   pbl_sbl_err->soc_rc_shadow_reg,
		   pbl_sbl_err->parf_ltssm);
	pr_info("GCC_RAMSS_CBCR: 0x%08x\n",
		   pbl_sbl_err->gcc_ramss_cbcr);

	pr_info("TCSR_PBL_LOGGING: 0x%08x PCIE_BHI_ERRDBG: Start: 0x%08x\n",
		   pbl_sbl_err->pbl_stage, pbl_sbl_err->sbl_log_start);
	pr_info("PBL_WLAN_BOOT_CONFIG: 0x%08x PBL_BOOTSTRAP_STATUS: 0x%08x\n",
		   pbl_sbl_err->pbl_wlan_boot_cfg,
		   pbl_sbl_err->pbl_bootstrap_status);

	athdbg_mhi_q6_debug_print_pbl_data(ab, &pbl_sbl_err->pbl_data[1]);

	pr_err("\n");
	athdbg_mhi_q6_debug_print_sbl_data(ab, pbl_sbl_err);

	/* Print NOC errors */
	athdbg_mhi_q6_debug_print_noc_data(ab, pbl_sbl_err);
}

static void athdbg_mhi_q6_debug_cleanup_bl_data(
					struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err)
{
	int i;

	for (i = 0; i < MHI_Q6_MAX_PBL_DATA_SNAPSHOT; i++) {
		kfree(pbl_sbl_err->pbl_data[i].pbl_vals);
		kfree(pbl_sbl_err->pbl_data[i].pbl_reg_tbl);
		pbl_sbl_err->pbl_data[i].pbl_vals = NULL;
		pbl_sbl_err->pbl_data[i].pbl_reg_tbl = NULL;
	}

	kfree(pbl_sbl_err->sbl_vals);
	pbl_sbl_err->sbl_vals = NULL;

	/* Cleanup NOC data */
	kfree(pbl_sbl_err->noc_vals);
	pbl_sbl_err->noc_vals = NULL;
}

/**
 * athdbg_mhi_q6_dump_bl_sram_mem - Dump WLAN FW bootloader debug log
 * @ab: ath12k base structure
 *
 * Return: None
 */
void athdbg_mhi_q6_dump_bl_sram_mem(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci;
	struct mhi_controller *mhi_ctrl;
	struct pci_dev *pci_dev;
	unsigned long *mhi_state;
	struct athdbg_mhi_q6_sbl_reg_addr sbl_data = {0};
	struct athdbg_mhi_q6_pbl_reg_addr pbl_data = {0};
	struct athdbg_mhi_q6_pbl_sbl_data  *pbl_sbl_err = NULL;
	int gfp = GFP_KERNEL;

	ab_pci = athdbg_mhi_get_pci_priv(ab);
	if (!ab_pci) {
		pr_err("Q6 PBL/SBL logging failed as PCI private data is not present\n");
		return;
	}

	mhi_ctrl = ab_pci->mhi_ctrl;
	pci_dev = ab_pci->pdev;
	if (!pci_dev) {
		pr_err("Q6 PBL/SBL logging failed as pci_dev is NULL\n");
		return;
	}

	mhi_state = athdbg_mhi_get_mhi_state(ab);
	if (!mhi_state)
		return;

	if (test_bit(ATH12K_MHI_MISSION_MODE, mhi_state)) {
		pr_err("skip Q6 PBL/SBL logging as device is in MISSION mode\n");
		return;
	}

	/* Set up register addresses based on device type */
	switch (pci_dev->device) {
	case QCN9274_DEVICE_ID:
		sbl_data.sbl_sram_start = QCN9224_SRAM_START;
		sbl_data.sbl_sram_end = QCN9224_SRAM_END;
		sbl_data.sbl_log_size_reg = QCN9224_PCIE_BHI_ERRDBG3_REG;
		sbl_data.sbl_log_start_reg = QCN9224_PCIE_BHI_ERRDBG2_REG;
		sbl_data.sbl_log_size_shift = 0;

#if LINUX_VERSION_IS_LESS(6,8,0)
		if (mhi_ctrl && mhi_ctrl->major_version == 2)
#else
		if (mhi_ctrl)
#endif
			pbl_data.pbl_log_sram_start = QCN9224_v2_PBL_LOG_SRAM_START;
		else
			pbl_data.pbl_log_sram_start = QCN9224_PBL_LOG_SRAM_START;

		pbl_data.pbl_log_sram_max_size = QCN9224_PBL_LOG_SRAM_MAX_SIZE;
		pbl_data.tcsr_pbl_logging_reg = QCN9224_TCSR_PBL_LOGGING_REG;
		pbl_data.pbl_wlan_boot_cfg = QCN9224_PBL_WLAN_BOOT_CONFIG;
		pbl_data.pbl_bootstrap_status = QCN9224_PBL_BOOTSTRAP_STATUS;
		break;
	case QCN9625_DEVICE_ID:
	case QCN9589_DEVICE_ID:
		sbl_data.sbl_sram_start = QCN9625_SRAM_START;
		if (pci_dev->device == QCN9589_DEVICE_ID)
			sbl_data.sbl_sram_end = QCN9589_SRAM_END;
		else
			sbl_data.sbl_sram_end = QCN9625_SRAM_END;

		sbl_data.sbl_log_start_reg = QCN9224_PCIE_BHI_ERRDBG2_REG;
		sbl_data.sbl_log_size_reg = QCN9224_PCIE_BHI_ERRDBG3_REG;
		sbl_data.sbl_log_size_shift = 0;

		if (athdbg_mhi_read_pbl_ram_info(ab,
						 &pbl_data.pbl_log_sram_start,
						 &pbl_data.pbl_log_sram_max_size) != 0) {
			pbl_data.pbl_log_sram_start = QCN9625_PBL_LOG_SRAM_START;
			pbl_data.pbl_log_sram_max_size = QCN9224_PBL_LOG_SRAM_MAX_SIZE;
			pr_info("Using default PBL RAM values: start=0x%x, size=%u\n",
				pbl_data.pbl_log_sram_start,
				pbl_data.pbl_log_sram_max_size);
		}
		pbl_data.tcsr_pbl_logging_reg = QCN9224_TCSR_PBL_LOGGING_REG;
		pbl_data.pbl_wlan_boot_cfg = QCN9625_PBL_WLAN_BOOT_CONFIG;
		pbl_data.pbl_bootstrap_status = QCN9224_PBL_BOOTSTRAP_STATUS;
		break;
	default:
		pr_warn("Unsupported device ID: 0x%x\n", pci_dev->device);
		return;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	pbl_sbl_err = kzalloc(sizeof(*pbl_sbl_err), gfp);
	if (!pbl_sbl_err)
		return;

	athdbg_mhi_q6_debug_collect_bl_data(ab, &pbl_data, &sbl_data,
					    pbl_sbl_err);
	athdbg_mhi_q6_debug_print_bl_data(ab, pbl_sbl_err);
	athdbg_mhi_q6_debug_cleanup_bl_data(pbl_sbl_err);
	kfree(pbl_sbl_err);
}
EXPORT_SYMBOL(athdbg_mhi_q6_dump_bl_sram_mem);

/* Internal handler called from service interface */
void athdbg_mhi_q6_boot_debug_timeout_hdlr_internal(struct ath12k_base *ab)
{
	unsigned long *mhi_state;

	mhi_state = athdbg_mhi_get_mhi_state(ab);
	if (!mhi_state)
		return;

	/* Stop if MHI is powered on or mission mode */
	if (test_bit(ATH12K_MHI_POWER_ON, mhi_state) ||
	    test_bit(ATH12K_MHI_MISSION_MODE, mhi_state)) {
		pr_debug("Boot debug timer stopped: MHI ready\n");
		return;
	}

	if (athdbg_mhi_q6_scan_rddm_cookie(ab, MHI_DEVICE_RDDM_COOKIE))
		return;

	athdbg_mhi_q6_debug_reg_dump(ab);
	athdbg_mhi_q6_dump_bl_sram_mem(ab);
}
EXPORT_SYMBOL(athdbg_mhi_q6_boot_debug_timeout_hdlr_internal);
#endif

