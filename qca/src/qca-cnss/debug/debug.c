/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/sizes.h>
#include "cnss_common/cnss_common.h"
#include "../main.h"
#include "debug/debug.h"
#include "pci/pci.h"

#define MAX_MLO_SOC_BITMASK	8

#if IS_ENABLED(CONFIG_IPC_LOGGING)
void *cnss_ipc_log_context;
void *cnss_ipc_log_long_context;
#endif
extern void cnss_dump_qmi_history(void);

static struct cnss_ce_base_addr ce_base_addr_qca8074 = {
	.src_base = QCA8074_CE_SRC_RING_REG_BASE,
	.dst_base = QCA8074_CE_DST_RING_REG_BASE,
	.common_base = QCA8074_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

/* Bar address is mapped to CE_BASE */
static struct cnss_ce_base_addr ce_base_addr_qca5018 = {
	.src_base = QCA5018_CE_SRC_RING_REG_BASE,
	.dst_base = QCA5018_CE_DST_RING_REG_BASE,
	.common_base = QCA5018_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_ce_base_addr ce_base_addr_qcn9000 = {
	.src_base = QCN9000_CE_SRC_RING_REG_BASE,
	.dst_base = QCN9000_CE_DST_RING_REG_BASE,
	.common_base = QCN9000_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_ce_base_addr ce_base_addr_qcn9224 = {
	.src_base = QCN9000_CE_SRC_RING_REG_BASE,
	.dst_base = QCN9000_CE_DST_RING_REG_BASE,
	.common_base = QCN9224_CE_COMMON_REG_BASE,
	.max_ce_count = QCN9224_CE_COUNT,
};

/* Bar address is mapped to CE_BASE */
static struct cnss_ce_base_addr ce_base_addr_qca5332 = {
	.src_base = QCA5332_CE_SRC_RING_REG_BASE,
	.dst_base = QCA5332_CE_DST_RING_REG_BASE,
	.common_base = QCA5332_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_ce_base_addr ce_base_addr_qca5424 = {
	.src_base = QCA5424_CE_SRC_RING_REG_BASE,
	.dst_base = QCA5424_CE_DST_RING_REG_BASE,
	.common_base = QCA5424_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_ce_base_addr ce_base_addr_qcn6122 = {
	.src_base = QCN6122_CE_SRC_RING_REG_BASE,
	.dst_base = QCN6122_CE_DST_RING_REG_BASE,
	.common_base = QCN6122_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_ce_base_addr ce_base_addr_qcn9160 = {
	.src_base = QCN9160_CE_SRC_RING_REG_BASE,
	.dst_base = QCN9160_CE_DST_RING_REG_BASE,
	.common_base = QCN9160_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_ce_base_addr ce_base_addr_qcn6432 = {
	.src_base = QCN6432_CE_SRC_RING_REG_BASE,
	.dst_base = QCN6432_CE_DST_RING_REG_BASE,
	.common_base = QCN6432_CE_COMMON_REG_BASE,
	.max_ce_count = DEFAULT_CE_COUNT,
};

static struct cnss_reg_offset ce_src[] = {
	{ "SRC_RING_BASE_LSB", CE_SRC_RING_BASE_LSB_OFFSET },
	{ "SRC_RING_BASE_MSB", CE_SRC_RING_BASE_MSB_OFFSET },
	{ "SRC_RING_ID", CE_SRC_RING_ID_OFFSET },
	{ "SRC_RING_MISC", CE_SRC_RING_MISC_OFFSET },
	{ "SRC_RING_SETUP_IX0", CE_SRC_RING_SETUP_IX0_OFFSET },
	{ "SRC_RING_SETUP_IX1", CE_SRC_RING_SETUP_IX1_OFFSET },
	{ "SRC_RING_MSI1_BASE_LSB", CE_SRC_RING_MSI1_BASE_LSB_OFFSET },
	{ "SRC_RING_MSI1_BASE_MSB", CE_SRC_RING_MSI1_BASE_MSB_OFFSET },
	{ "SRC_RING_MSI1_DATA", CE_SRC_RING_MSI1_DATA_OFFSET },
	{ "SRC_CTRL", CE_SRC_CTRL_OFFSET },
	{ "SRC_R0_CE_CH_SRC_IS", CE_SRC_R0_CE_CH_SRC_IS_OFFSET },
	{ "SRC_RING_HP", CE_SRC_RING_HP_OFFSET },
	{ "SRC_RING_TP", CE_SRC_RING_TP_OFFSET },
	{ NULL },
};

static struct cnss_reg_offset ce_dst[] = {
	{ "DEST_RING_BASE_LSB", CE_DEST_RING_BASE_LSB_OFFSET },
	{ "DEST_RING_BASE_MSB", CE_DEST_RING_BASE_MSB_OFFSET },
	{ "DEST_RING_ID", CE_DEST_RING_ID_OFFSET },
	{ "DEST_RING_MISC", CE_DEST_RING_MISC_OFFSET },
	{ "DEST_RING_SETUP_IX0", CE_DEST_RING_SETUP_IX0_OFFSET },
	{ "DEST_RING_SETUP_IX1", CE_DEST_RING_SETUP_IX1_OFFSET },
	{ "DEST_RING_MSI1_BASE_LSB", CE_DEST_RING_MSI1_BASE_LSB_OFFSET },
	{ "DEST_RING_MSI1_BASE_MSB", CE_DEST_RING_MSI1_BASE_MSB_OFFSET },
	{ "DEST_RING_MSI1_DATA", CE_DEST_RING_MSI1_DATA_OFFSET },
	{ "DEST_CTRL", CE_DEST_CTRL_OFFSET },
	{ "CE_CH_DST_IS", CE_CH_DST_IS_OFFSET },
	{ "CE_CH_DEST_CTRL2", CE_CH_DEST_CTRL2_OFFSET },
	{ "DEST_RING_HP", CE_DEST_RING_HP_OFFSET },
	{ "DEST_RING_TP", CE_DEST_RING_TP_OFFSET },
	{ "STATUS_RING_BASE_LSB", CE_STATUS_RING_BASE_LSB_OFFSET },
	{ "STATUS_RING_BASE_MSB", CE_STATUS_RING_BASE_MSB_OFFSET },
	{ "STATUS_RING_ID", CE_STATUS_RING_ID_OFFSET },
	{ "STATUS_RING_MISC", CE_STATUS_RING_MISC_OFFSET },
	{ "STATUS_RING_HP", CE_STATUS_RING_HP_OFFSET },
	{ "STATUS_RING_TP", CE_STATUS_RING_TP_OFFSET },
	{ NULL },
};

static struct cnss_reg_offset ce_cmn[] = {
	{ "GXI_ERR_INTS", CE_COMMON_GXI_ERR_INTS },
	{ "GXI_ERR_STATS", CE_COMMON_GXI_ERR_STATS },
	{ "GXI_WDOG_STATUS", CE_COMMON_GXI_WDOG_STATUS },
	{ "TARGET_IE_0", CE_COMMON_TARGET_IE_0 },
	{ "TARGET_IE_1", CE_COMMON_TARGET_IE_1 },
	{ NULL },
};

static struct noc_err_table noc_err_table_list[] = {
	{"SNOC_ERL_ErrVld_Low", QCN9224_SNOC_ERL_ErrVld_Low,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog0_Low", QCN9224_SNOC_ERL_ErrLog0_Low,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog0_High", QCN9224_SNOC_ERL_ErrLog0_High,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog1_Low", QCN9224_SNOC_ERL_ErrLog1_Low,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog1_High", QCN9224_SNOC_ERL_ErrLog1_High,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog2_Low", QCN9224_SNOC_ERL_ErrLog2_Low,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog2_High", QCN9224_SNOC_ERL_ErrLog2_High,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog3_Low", QCN9224_SNOC_ERL_ErrLog3_Low,
							&cnss_pci_reg_read},
	{"SNOC_ERL_ErrLog3_High", QCN9224_SNOC_ERL_ErrLog3_High,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrVld_Low", QCN9224_PCNOC_ERL_ErrVld_Low,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog0_Low", QCN9224_PCNOC_ERL_ErrLog0_Low,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog0_High", QCN9224_PCNOC_ERL_ErrLog0_High,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog1_Low", QCN9224_PCNOC_ERL_ErrLog1_Low,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog1_High", QCN9224_PCNOC_ERL_ErrLog1_High,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog2_Low", QCN9224_PCNOC_ERL_ErrLog2_Low,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog2_High", QCN9224_PCNOC_ERL_ErrLog2_High,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog3_Low", QCN9224_PCNOC_ERL_ErrLog3_Low,
							&cnss_pci_reg_read},
	{"PCNOC_ERL_ErrLog3_High", QCN9224_PCNOC_ERL_ErrLog3_High,
							&cnss_pci_reg_read},
	{ NULL },
};

struct cnss_ce_base_addr *register_ce_object(struct cnss_plat_data *plat_priv)
{
	struct cnss_ce_base_addr *ce_object = NULL;

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA9574_DEVICE_ID:
		ce_object = &ce_base_addr_qca8074;
		break;
	case QCA5018_DEVICE_ID:
		ce_object = &ce_base_addr_qca5018;
		break;
	case QCN9000_DEVICE_ID:
		ce_object = &ce_base_addr_qcn9000;
		break;
	case QCN9224_DEVICE_ID:
		ce_object = &ce_base_addr_qcn9224;
		break;
	case QCA5332_DEVICE_ID:
		ce_object = &ce_base_addr_qca5332;
		break;
	case QCN6122_DEVICE_ID:
		ce_object = &ce_base_addr_qcn6122;
		break;
	case QCN9160_DEVICE_ID:
		ce_object = &ce_base_addr_qcn9160;
		break;
	case QCN6432_DEVICE_ID:
		ce_object = &ce_base_addr_qcn6432;
		break;
	case QCA5424_DEVICE_ID:
		ce_object = &ce_base_addr_qca5424;
		break;
	default:
		cnss_pr_err("Unsupported device id 0x%lx\n",
			    plat_priv->device_id);
		return NULL;
	}
	return ce_object;
}

static void cnss_get_ce_base(struct cnss_plat_data *plat_priv,
			     struct cnss_ce_base_addr *ce_object,
			     u32 *src_base, u32 *dst_base, u32 *common_base)
{
	*src_base = ce_object->src_base;
	*dst_base = ce_object->dst_base;
	*common_base = ce_object->common_base;

	switch (plat_priv->device_id) {
	case QCA5018_DEVICE_ID:
		*src_base -= QCA5018_CE_SRC_RING_REG_BASE;
		*dst_base -= QCA5018_CE_SRC_RING_REG_BASE;
		*common_base -= QCA5018_CE_SRC_RING_REG_BASE;
		break;
	case QCA5332_DEVICE_ID:
		*src_base -= QCA5332_CE_SRC_RING_REG_BASE;
		*dst_base -= QCA5332_CE_SRC_RING_REG_BASE;
		*common_base -= QCA5332_CE_SRC_RING_REG_BASE;
		break;
	case QCA5424_DEVICE_ID:
		*src_base -= QCA5424_CE_SRC_RING_REG_BASE;
		*dst_base -= QCA5424_CE_SRC_RING_REG_BASE;
		*common_base -= QCA5424_CE_SRC_RING_REG_BASE;
		break;
	default:
		break;
	}
}

static void cnss_get_ce_bar_from_ce_base(struct cnss_plat_data *plat_priv,
					 u32 *reg_offset)
{
	switch (plat_priv->device_id) {
	case QCA5018_DEVICE_ID:
		*reg_offset += QCA5018_CE_SRC_RING_REG_BASE;
		break;
	case QCA5332_DEVICE_ID:
		*reg_offset += QCA5332_CE_SRC_RING_REG_BASE;
		break;
	case QCA5424_DEVICE_ID:
		*reg_offset += QCA5424_CE_SRC_RING_REG_BASE;
		break;
	default:
		break;
	}
}

static void cnss_dump_ce_reg(struct cnss_plat_data *plat_priv,
		      enum cnss_ce_index ce,
		      struct cnss_ce_base_addr *ce_object)
{
	int i;
	u32 ce_base = ce * PER_CE_REG_SIZE;
	u32 reg_offset, val;
	u32 src_base;
	u32 dst_base;
	u32 common_base;
	struct device *dev = &plat_priv->plat_dev->dev;

	cnss_get_ce_base(plat_priv, ce_object, &src_base,
			 &dst_base, &common_base);
	if (ce >= CNSS_CE_00 && ce < ce_object->max_ce_count) {
		for (i = 0; ce_src[i].name; i++) {
			reg_offset = src_base +
				ce_base + ce_src[i].offset;

			if (cnss_bus_reg_read(dev, reg_offset, &val, NULL))
				return;
			cnss_get_ce_bar_from_ce_base(plat_priv, &reg_offset);
			cnss_pr_info("CE_%02d_%s[0x%x] = 0x%x\n",
				     ce, ce_src[i].name, reg_offset, val);
		}

		for (i = 0; ce_dst[i].name; i++) {
			reg_offset = dst_base +
				ce_base + ce_dst[i].offset;

			if (cnss_bus_reg_read(dev, reg_offset, &val, NULL))
				return;
			cnss_get_ce_bar_from_ce_base(plat_priv, &reg_offset);
			cnss_pr_info("CE_%02d_%s[0x%x] = 0x%x\n",
				     ce, ce_dst[i].name, reg_offset, val);
		}
	} else if (ce == CNSS_CE_COMMON) {
		for (i = 0; ce_cmn[i].name; i++) {
			reg_offset = common_base +
				ce_base + ce_cmn[i].offset;

			if (cnss_bus_reg_read(dev, reg_offset, &val, NULL))
				return;
			cnss_get_ce_bar_from_ce_base(plat_priv, &reg_offset);
			cnss_pr_info("CE_COMMON_%s[0x%x] = 0x%x\n",
				     ce_cmn[i].name, reg_offset, val);
		}
	} else {
		cnss_pr_err("Unsupported CE[%d] registers dump\n", ce);
	}
}

int cnss_dump_all_ce_reg(struct cnss_plat_data *plat_priv)
{
	int ce = 0;
	struct cnss_ce_base_addr *ce_object;

	if (!plat_priv) {
		pr_err("Plat Priv is null\n");
		return -ENODEV;
	}

	ce_object = register_ce_object(plat_priv);
	if (!ce_object) {
		cnss_pr_err("CE object is null\n");
		return -1;
	}

	cnss_pr_dbg("Start to dump debug registers\n");
	for (ce = 0; ce < ce_object->max_ce_count; ce++)
		cnss_dump_ce_reg(plat_priv, ce, ce_object);
	return 0;
}
EXPORT_SYMBOL(cnss_dump_all_ce_reg);

int cnss_get_mhi_region_len(struct cnss_plat_data *plat_priv,
				   u32 *reg_start, u32 *reg_end)
{
	switch (plat_priv->device_id) {
	case QCN9000_DEVICE_ID:
		*reg_start = QCN9000_PCI_MHIREGLEN_REG;
		*reg_end = QCN9000_PCI_MHI_REGION_END;
		break;
	case QCN9224_DEVICE_ID:
		*reg_start = QCN9224_PCI_MHIREGLEN_REG;
		*reg_end = QCN9224_PCI_MHI_REGION_END;
		break;
	case QCN6122_DEVICE_ID:
		*reg_start = QCN6122_PCI_MHIREGLEN_REG;
		*reg_end = QCN6122_PCI_MHI_REGION_END;
		break;
	case QCN9160_DEVICE_ID:
		*reg_start = QCN9160_PCI_MHIREGLEN_REG;
		*reg_end = QCN9160_PCI_MHI_REGION_END;
		break;
	default:
		cnss_pr_err("Unknown device type 0x%lx\n",
			    plat_priv->device_id);
		return -ENODEV;
	}

	return 0;
}

static int cnss_debug_read_pbl_data(struct cnss_pci_data *pci_priv,
				    struct pbl_reg_addr *pbl_data,
				    struct pbl_err_data *pbl_err_data)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	u32 log_size_v1 = pbl_data->pbl_log_sram_max_size_v1;
	u32 log_size = pbl_data->pbl_log_sram_max_size;
	u32 total_size = log_size + log_size_v1;
	int gfp = GFP_KERNEL;
	u32 *mem_addr = NULL;
	u32 *buf = NULL;
	int i = 0;
	int j = 0;

	if (test_bit(CNSS_MHI_MISSION_MODE, &pci_priv->mhi_state)) {
		cnss_pr_err("%s: %s is already in Mission mode\n",
			    __func__, plat_priv->device_name);
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
		if (cnss_pci_reg_read(pci_priv->plat_priv,
					mem_addr[j], &buf[j]))
			break;
	}

	for (i = 0; i < log_size_v1; i += sizeof(u32), j++) {
		mem_addr[j] = pbl_data->pbl_log_sram_start_v1 + i;
		if (cnss_pci_reg_read(pci_priv->plat_priv,
					mem_addr[j], &buf[j]))
			break;
	}

	pbl_err_data->pbl_tbl_len = j;
	pbl_err_data->pbl_vals = buf;
	pbl_err_data->pbl_reg_tbl = mem_addr;

	return 0;
}

static int cnss_debug_read_sbl_data(struct cnss_pci_data *pci_priv,
				    u32 log_size,
				    u32 sram_start_reg,
				    struct dump_pbl_sbl_data *pbl_sbl_err)
{
	int i = 0;
	int j = 0;
	int gfp = GFP_KERNEL;
	u32 mem_addr = 0;
	u32 *buf = NULL;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (test_bit(CNSS_MHI_MISSION_MODE, &pci_priv->mhi_state)) {
		cnss_pr_err("%s: %s is already in Mission mode\n",
			    __func__, plat_priv->device_name);
		return -EINVAL;
	}
	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	buf = kcalloc(log_size, sizeof(u32), gfp);
	if (!buf)
		return -ENOMEM;

	for (i = 0, j = 0; i < log_size; i += sizeof(u32), j++) {
		mem_addr = sram_start_reg + i;
		cnss_pci_reg_read(pci_priv->plat_priv, mem_addr, &buf[j]);
		if (buf[j] == 0)
			break;
	}
	pbl_sbl_err->sbl_vals = buf;
	pbl_sbl_err->sbl_len = i;

	return 0;
}

static int cnss_debug_read_noc_errors(struct cnss_pci_data *pci_priv,
				      struct dump_pbl_sbl_data *pbl_sbl_err)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	int i = 0;
	u32 *buf = NULL;
	int gfp = GFP_KERNEL;
	size_t len = sizeof(noc_err_table_list) /
			sizeof(noc_err_table_list[0]);

	if (test_bit(CNSS_MHI_MISSION_MODE, &pci_priv->mhi_state)) {
		cnss_pr_err("%s: %s is already in Mission mode\n",
			    __func__, plat_priv->device_name);
		return -EINVAL;
	}
	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	buf = kcalloc(len, sizeof(u32), gfp);
	if (!buf)
		return -ENOMEM;

	switch (plat_priv->device_id) {
	case QCN9224_DEVICE_ID:
		for (i = 0; noc_err_table_list[i].reg_name; i++)
			noc_err_table_list[i].reg_handler(plat_priv,
				noc_err_table_list[i].reg, &buf[i]);
		pbl_sbl_err->noc_vals = buf;
		break;
	default:
		break;
	}

	return 0;
}

static int cnss_debug_read_misc_data(struct cnss_pci_data *pci_priv,
				     struct pbl_reg_addr *pbl_data,
				     struct sbl_reg_addr *sbl_data,
				     struct dump_pbl_sbl_data *pbl_sbl_err)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (test_bit(CNSS_MHI_MISSION_MODE, &pci_priv->mhi_state)) {
		cnss_pr_err("%s: %s is already in Mission mode\n",
			    __func__, plat_priv->device_name);
		return -EINVAL;
	}
#if defined(CONFIG_CNSS2_QCOM_KERNEL_DEPENDENCY) && IS_ENABLED(CONFIG_PCIE_QCOM)
	pcie_parf_read(pci_priv->pci_dev, PCIE_CFG_PCIE_STATUS,
		       &pbl_sbl_err->pcie_cfg_pcie_status);
#endif
	cnss_pci_reg_read(plat_priv,
			  PCIE_PCIE_PARF_PM_STTS,
			  &pbl_sbl_err->parf_pm_stts);
	pci_read_config_word(pci_priv->pci_dev, PCI_COMMAND,
			     &pbl_sbl_err->type0_status_cmd_reg);

	pci_read_config_word(pci_priv->pci_dev,
			     PCIE_PCI_MSI_CAP_ID_NEXT_CTRL_REG,
			     &pbl_sbl_err->pci_msi_cap_id_next_ctrl_reg);
	pci_read_config_word(pci_priv->pci_dev, PCIE_MSI_CAP_OFF_04H_REG,
			     &pbl_sbl_err->pci_msi_cap_off_04h_reg);
	pci_read_config_word(pci_priv->pci_dev, PCIE_MSI_CAP_OFF_08H_REG,
			     &pbl_sbl_err->pci_msi_cap_off_08h_reg);
	pci_read_config_word(pci_priv->pci_dev, PCIE_MSI_CAP_OFF_0CH_REG,
			     &pbl_sbl_err->pci_msi_cap_off_0ch_reg);

	if (plat_priv->device_id == QCN9224_DEVICE_ID) {
		cnss_pci_reg_read(plat_priv,
				  QCN9224_PCIE_PCIE_LOCAL_REG_REMAP_BAR_CTRL,
				  &pbl_sbl_err->remap_bar_ctrl);
		cnss_pci_reg_read(plat_priv,
				  QCN9224_WLAON_SOC_RESET_CAUSE_SHADOW_REG,
				  &pbl_sbl_err->soc_rc_shadow_reg);
		cnss_pci_reg_read(plat_priv,
				  QCN9224_PCIE_PCIE_PARF_LTSSM,
				  &pbl_sbl_err->parf_ltssm);
		cnss_pci_reg_read(plat_priv,
				  QCN9224_GCC_RAMSS_CBCR,
				  &pbl_sbl_err->gcc_ramss_cbcr);
	}

	if (cnss_pci_reg_read(plat_priv, sbl_data->sbl_log_start_reg,
			      &pbl_sbl_err->sbl_log_start)) {
		cnss_pr_err("Invalid SBL log data\n");
		return -EINVAL;
	}

	cnss_pci_reg_read(plat_priv, pbl_data->tcsr_pbl_logging_reg,
			  &pbl_sbl_err->pbl_stage);
	cnss_pci_reg_read(plat_priv, pbl_data->pbl_wlan_boot_cfg,
			  &pbl_sbl_err->pbl_wlan_boot_cfg);
	cnss_pci_reg_read(plat_priv, pbl_data->pbl_bootstrap_status,
			  &pbl_sbl_err->pbl_bootstrap_status);

	return 0;
}

static void cnss_debug_collect_bl_data(struct cnss_pci_data *pci_priv,
				       struct pbl_reg_addr *pbl_data,
				       struct sbl_reg_addr *sbl_data,
				       struct dump_pbl_sbl_data *pbl_sbl_err)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	u32 sbl_log_size;
	u32 sbl_log_start;

	if (test_bit(CNSS_MHI_MISSION_MODE, &pci_priv->mhi_state)) {
		cnss_pr_err("%s: %s is already in Mission mode\n",
			    __func__, plat_priv->device_name);
		return;
	}
	/* Dump SRAM content twice before and after the register dump as
	 * Q6 team requested. Please check with Q6 team before removing one
	 * of the SRAM dump.
	 */
	if (cnss_debug_read_pbl_data(pci_priv, pbl_data,
				     &pbl_sbl_err->pbl_data[0])) {
		cnss_pr_err("%s: Failed to read PBL log data\n",
			    __func__);
		return;
	}

	if (cnss_debug_read_misc_data(pci_priv, pbl_data, sbl_data,
				      pbl_sbl_err)) {
		cnss_pr_err("%s: Failed to read Misc log data\n",
			    __func__);
		return;
	}

	if (cnss_debug_read_noc_errors(pci_priv, pbl_sbl_err)) {
		cnss_pr_err("%s: Failed to read NOC data\n",
			    __func__);
		return;
	}

	if (cnss_debug_read_pbl_data(pci_priv, pbl_data,
				     &pbl_sbl_err->pbl_data[1])) {
		cnss_pr_err("%s: Failed to read PBL log data\n",
			    __func__);
		return;
	}

	if (cnss_pci_reg_read(plat_priv, sbl_data->sbl_log_size_reg,
			      &sbl_log_size)) {
		cnss_pr_err("Invalid SBL log data\n");
		return;
	}

	sbl_log_start = pbl_sbl_err->sbl_log_start;
	sbl_log_size = ((sbl_log_size >> sbl_data->sbl_log_size_shift) &
			SBL_LOG_SIZE_MASK);
	if (sbl_log_start < sbl_data->sbl_sram_start ||
	    sbl_log_start > sbl_data->sbl_sram_end ||
	    (sbl_log_start + sbl_log_size) > sbl_data->sbl_sram_end) {
		cnss_pr_err("Invalid SBL log data\n");
		return;
	}

	if (cnss_debug_read_sbl_data(pci_priv, sbl_log_size, sbl_log_start,
				     pbl_sbl_err))
		cnss_pr_err("%s: Failed to read SBL log data\n",
			    __func__);
}

static void cnss_debug_print_pbl_data(struct cnss_pci_data *pci_priv,
				      struct pbl_err_data *pbl_data)
{
	int i;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pr_err("Dumping PBL log data\n");
	for (i = 0; i < pbl_data->pbl_tbl_len; i++)
		cnss_pr_err("SRAM[0x%x] = 0x%x\n",
			    pbl_data->pbl_reg_tbl[i],
			    pbl_data->pbl_vals[i]);
}

static void cnss_debug_print_sbl_data(struct cnss_pci_data *pci_priv,
				      struct dump_pbl_sbl_data *pbl_sbl_err)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pr_err("Dumping SBL log data\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 32, 4,
		       pbl_sbl_err->sbl_vals, pbl_sbl_err->sbl_len, 1);
}

static void cnss_debug_print_noc_data(struct cnss_pci_data *pci_priv,
				      struct dump_pbl_sbl_data *pbl_sbl_err)
{
	int i;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pr_err("Dumping NOC log data\n");
	for (i = 0; noc_err_table_list[i].reg_name; i++)
		cnss_pr_err("%s: %s: 0x%08x\n", __func__,
			noc_err_table_list[i].reg_name,
			pbl_sbl_err->noc_vals[i]);
}

static void cnss_debug_print_bl_data(struct cnss_pci_data *pci_priv,
				     struct dump_pbl_sbl_data *pbl_sbl_err)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

#if defined(CONFIG_CNSS2_QCOM_KERNEL_DEPENDENCY) && IS_ENABLED(CONFIG_PCIE_QCOM)
	cnss_pr_err("%s: PCIE_CFG_PCIE_STATUS: 0x%08x\n",
		    __func__, pbl_sbl_err->pcie_cfg_pcie_status);
#endif
	cnss_pr_err("%s: PARF_PM_STTS: 0x%08x, PCIE_TYPE0_STATUS_COMMAND_REG: 0x%08x\n",
		    __func__, pbl_sbl_err->parf_pm_stts,
		    pbl_sbl_err->type0_status_cmd_reg);

	cnss_pr_err("%s: PCIE_PCI_MSI_CAP_ID_NEXT_CTRL_REG: 0x%08x, PCIE_MSI_CAP_OFF_04H_REG: 0x%08x\n",
		    __func__, pbl_sbl_err->pci_msi_cap_id_next_ctrl_reg,
		    pbl_sbl_err->pci_msi_cap_off_04h_reg);
	cnss_pr_err("%s: PCIE_MSI_CAP_OFF_08H_REG: 0x%08x, PCIE_MSI_CAP_OFF_0CH_REG: 0x%08x\n",
		    __func__, pbl_sbl_err->pci_msi_cap_off_08h_reg,
		    pbl_sbl_err->pci_msi_cap_off_0ch_reg);

	cnss_debug_print_pbl_data(pci_priv, &pbl_sbl_err->pbl_data[0]);

	if (plat_priv->device_id == QCN9224_DEVICE_ID) {
		cnss_pr_err("%s: LOCAL_REG_REMAP_BAR_CTRL: 0x%08x, WLAON_SOC_RESET_CAUSE_SHADOW_REG: 0x%08x, PARF_LTSSM: 0x%08x\n",
			    __func__, pbl_sbl_err->remap_bar_ctrl,
			    pbl_sbl_err->soc_rc_shadow_reg,
			    pbl_sbl_err->parf_ltssm);
		cnss_pr_err("%s: GCC_RAMSS_CBCR: 0x%08x\n",
			    __func__, pbl_sbl_err->gcc_ramss_cbcr);

		cnss_debug_print_noc_data(pci_priv, pbl_sbl_err);
	}

	cnss_pr_err("TCSR_PBL_LOGGING: 0x%08x PCIE_BHI_ERRDBG: Start: 0x%08x\n",
		    pbl_sbl_err->pbl_stage, pbl_sbl_err->sbl_log_start);
	cnss_pr_err("PBL_WLAN_BOOT_CFG: 0x%08x PBL_BOOTSTRAP_STATUS: 0x%08x\n",
		    pbl_sbl_err->pbl_wlan_boot_cfg,
		    pbl_sbl_err->pbl_bootstrap_status);

	cnss_debug_print_pbl_data(pci_priv, &pbl_sbl_err->pbl_data[1]);

	cnss_pr_err("\n");
	cnss_debug_print_sbl_data(pci_priv, pbl_sbl_err);
}

void cnss_debug_cleanup_bl_data(struct dump_pbl_sbl_data *pbl_sbl_err)
{
	int i;

	for (i = 0; i < MAX_PBL_DATA_SNAPSHOT; i++) {
		kfree(pbl_sbl_err->pbl_data[i].pbl_vals);
		kfree(pbl_sbl_err->pbl_data[i].pbl_reg_tbl);
		pbl_sbl_err->pbl_data[i].pbl_vals = NULL;
		pbl_sbl_err->pbl_data[i].pbl_reg_tbl = NULL;
	}

	kfree(pbl_sbl_err->sbl_vals);
	kfree(pbl_sbl_err->noc_vals);
	pbl_sbl_err->sbl_vals = NULL;
	pbl_sbl_err->noc_vals = NULL;

	kfree(pbl_sbl_err);
}

/**
 * cnss_debug_dump_bl_sram_mem - Dump WLAN FW bootloader debug log
 * @pci_priv: PCI device private data structure of cnss platform driver
 *
 * Dump Primary and secondary bootloader debug log data. For SBL check the
 * log struct address and size for validity.
 *
 * Supported only on QCN9000
 *
 * Return: None
 */
void cnss_pci_dump_bl_sram_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct sbl_reg_addr sbl_data = {0};
	struct pbl_reg_addr pbl_data = {0};
	struct dump_pbl_sbl_data *pbl_sbl_err = NULL;
	struct mhi_controller *mhi_ctrl = pci_priv->mhi_ctrl;
	int gfp = GFP_KERNEL;

	if (test_bit(CNSS_MHI_MISSION_MODE, &pci_priv->mhi_state)) {
		cnss_pr_err("%s: %s is already in Mission mode\n",
			    __func__, plat_priv->device_name);
		return;
	}
	switch (plat_priv->device_id) {
	case QCN9000_DEVICE_ID:
		sbl_data.sbl_sram_start = QCN9000_SRAM_START;
		sbl_data.sbl_sram_end = QCN9000_SRAM_END;
		sbl_data.sbl_log_size_reg = QCN9000_PCIE_BHI_ERRDBG2_REG;
		sbl_data.sbl_log_start_reg = QCN9000_PCIE_BHI_ERRDBG3_REG;
		sbl_data.sbl_log_size_shift = 16;
		pbl_data.pbl_log_sram_start = QCN9000_PBL_LOG_SRAM_START;
		pbl_data.pbl_log_sram_max_size = QCN9000_PBL_LOG_SRAM_MAX_SIZE;
		pbl_data.pbl_log_sram_start_v1 = QCN9000_PBL_LOG_SRAM_START_V1;
		pbl_data.pbl_log_sram_max_size_v1 =
					QCN9000_PBL_LOG_SRAM_MAX_SIZE_V1;
		pbl_data.tcsr_pbl_logging_reg = QCN9000_TCSR_PBL_LOGGING_REG;
		pbl_data.pbl_wlan_boot_cfg = QCN9000_PBL_WLAN_BOOT_CFG;
		pbl_data.pbl_bootstrap_status = QCN9000_PBL_BOOTSTRAP_STATUS;
		break;
	case QCN9224_DEVICE_ID:
		sbl_data.sbl_sram_start = QCN9224_SRAM_START;
		sbl_data.sbl_sram_end = QCN9224_SRAM_END;
		sbl_data.sbl_log_size_reg = QCN9224_PCIE_BHI_ERRDBG3_REG;
		sbl_data.sbl_log_start_reg = QCN9224_PCIE_BHI_ERRDBG2_REG;
		if (mhi_ctrl->major_version == 2)
			pbl_data.pbl_log_sram_start =
				QCN9224_v2_PBL_LOG_SRAM_START;
		else
			pbl_data.pbl_log_sram_start =
				QCN9224_PBL_LOG_SRAM_START;

		pbl_data.pbl_log_sram_max_size = QCN9224_PBL_LOG_SRAM_MAX_SIZE;
		pbl_data.tcsr_pbl_logging_reg = QCN9224_TCSR_PBL_LOGGING_REG;
		pbl_data.pbl_wlan_boot_cfg = QCN9224_PBL_WLAN_BOOT_CFG;
		pbl_data.pbl_bootstrap_status = QCN9224_PBL_BOOTSTRAP_STATUS;
		break;
	default:
		cnss_pr_err("Unknown device type 0x%lx\n",
			    plat_priv->device_id);
		return;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	pbl_sbl_err = kzalloc(sizeof(*pbl_sbl_err), gfp);
	if (!pbl_sbl_err)
		return;

	cnss_debug_collect_bl_data(pci_priv, &pbl_data, &sbl_data,
				   pbl_sbl_err);
	cnss_debug_print_bl_data(pci_priv, pbl_sbl_err);
	cnss_debug_cleanup_bl_data(pbl_sbl_err);
}

static int cnss_pin_connect_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *cnss_priv = s->private;

	seq_puts(s, "Pin connect results\n");
	seq_printf(s, "FW power pin result: %04x\n",
		   cnss_priv->pin_result.fw_pwr_pin_result);
	seq_printf(s, "FW PHY IO pin result: %04x\n",
		   cnss_priv->pin_result.fw_phy_io_pin_result);
	seq_printf(s, "FW RF pin result: %04x\n",
		   cnss_priv->pin_result.fw_rf_pin_result);
	seq_printf(s, "Host pin result: %04x\n",
		   cnss_priv->pin_result.host_pin_result);
	seq_puts(s, "\n");

	return 0;
}

static int cnss_pin_connect_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_pin_connect_show, inode->i_private);
}

static const struct file_operations cnss_pin_connect_fops = {
	.read		= seq_read,
	.release	= single_release,
	.open		= cnss_pin_connect_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_stats_show_state(struct seq_file *s,
				 struct cnss_plat_data *plat_priv)
{
	enum cnss_driver_state i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "\nState: 0x%lx(", plat_priv->driver_state);
	for (i = 0, state = plat_priv->driver_state; state != 0;
	     state >>= 1, i++) {
		if (!(state & 0x1))
			continue;

		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case CNSS_QMI_WLFW_CONNECTED:
			seq_puts(s, "QMI_WLFW_CONNECTED");
			continue;
		case CNSS_FW_MEM_READY:
			seq_puts(s, "FW_MEM_READY");
			continue;
		case CNSS_FW_READY:
			seq_puts(s, "FW_READY");
			continue;
		case CNSS_COLD_BOOT_CAL:
			seq_puts(s, "COLD_BOOT_CAL");
			continue;
		case CNSS_DRIVER_LOADING:
			seq_puts(s, "DRIVER_LOADING");
			continue;
		case CNSS_DRIVER_UNLOADING:
			seq_puts(s, "DRIVER_UNLOADING");
			continue;
		case CNSS_DRIVER_IDLE_RESTART:
			seq_puts(s, "IDLE_RESTART");
			continue;
		case CNSS_DRIVER_IDLE_SHUTDOWN:
			seq_puts(s, "IDLE_SHUTDOWN");
			continue;
		case CNSS_DRIVER_PROBED:
			seq_puts(s, "DRIVER_PROBED");
			continue;
		case CNSS_DRIVER_RECOVERY:
			seq_puts(s, "DRIVER_RECOVERY");
			continue;
		case CNSS_FW_BOOT_RECOVERY:
			seq_puts(s, "FW_BOOT_RECOVERY");
			continue;
		case CNSS_DEV_ERR_NOTIFY:
			seq_puts(s, "DEV_ERR");
			continue;
		case CNSS_DRIVER_DEBUG:
			seq_puts(s, "DRIVER_DEBUG");
			continue;
		case CNSS_COEX_CONNECTED:
			seq_puts(s, "COEX_CONNECTED");
			continue;
		case CNSS_IMS_CONNECTED:
			seq_puts(s, "IMS_CONNECTED");
			continue;
		case CNSS_IN_SUSPEND_RESUME:
			seq_puts(s, "IN_SUSPEND_RESUME");
			continue;
		case CNSS_DAEMON_CONNECTED:
			seq_puts(s, "DAEMON_CONNECTED");
			continue;
		case CNSS_QDSS_STARTED:
			seq_puts(s, "QDSS_STARTED");
			continue;
		case CNSS_RECOVERY_WAIT_FOR_DRIVER:
			seq_puts(s, "CNSS_RECOVERY_WAIT_FOR_DRIVER");
			continue;
		case CNSS_RDDM_DUMP_IN_PROGRESS:
			seq_puts(s, "RDDM_DUMP_IN_PROGRESS");
			continue;
		}

		seq_printf(s, "UNKNOWN-%d", i);
	}
	seq_puts(s, ")\n");

	return 0;
}

static int cnss_stats_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	cnss_stats_show_state(s, plat_priv);

	return 0;
}

static int cnss_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_stats_show, inode->i_private);
}

static const struct file_operations cnss_stats_fops = {
	.read		= seq_read,
	.release	= single_release,
	.open		= cnss_stats_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_debug_probe(struct pci_dev *pdev,
			     const struct pci_device_id *id)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(plat_dev);
	struct pci_dev *pci_dev;

	if (!plat_priv)
		return -ENODEV;

	pci_dev = plat_priv->pci_dev;

	if (!pci_dev)
		return -ENODEV;

	cnss_pr_err("%s: %d: plat_priv %pK device %pK\n",
		    __func__, __LINE__, plat_priv, pci_dev);

	cnss_wait_for_fw_ready(&pci_dev->dev);

	return 0;
}

static void cnss_debug_remove(struct pci_dev *pdev)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(plat_dev);

	cnss_pr_err("%s: %d: plat_priv %pK\n",
		    __func__, __LINE__, plat_priv);
}

static void cnss_debug_shutdown(struct pci_dev *pdev)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(plat_dev);

	cnss_pr_err("%s: %d: plat_priv %pK\n",
		    __func__, __LINE__, plat_priv);
}

static void cnss_debug_update_status(struct pci_dev *pdev,
				     const struct pci_device_id *id,
				     int status)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(plat_dev);

	cnss_pr_err("%s: %d: plat_priv %pK status %d\n",
		    __func__, __LINE__, plat_priv, status);
}

static int  cnss_debug_fatal(struct pci_dev *pdev,
			     const struct pci_device_id *id)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(plat_dev);

	cnss_pr_err("%s: %d: device %x\n",
		    __func__, __LINE__, id->device);

	return 0;
}

struct cnss_wlan_driver debug_driver_ops = {
	.name		= "pld_pcie",
	.probe		= cnss_debug_probe,
	.remove		= cnss_debug_remove,
	.update_status	= cnss_debug_update_status,
	.fatal		= cnss_debug_fatal,
	.shutdown	= cnss_debug_shutdown,
};

static ssize_t cnss_dev_boot_debug_write(struct file *fp,
					 const char __user *user_buf,
					 size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	struct cnss_pci_data *pci_priv;
	char buf[64];
	char *cmd;
	unsigned int len = 0;
	int ret = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	cmd = (char *)buf;

	if (sysfs_streq("test_driver_load", cmd)) {
		ret = cnss_wlan_register_driver_ops(&debug_driver_ops);
		if (ret) {
			cnss_pr_err("Fail to register driver ops\n");
			return ret;
		}
		ret = cnss_wlan_probe_driver();
		if (ret) {
			cnss_pr_err("Fail to register driver probe\n");
			return ret;
		}
		return count;
	}

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	if (sysfs_streq("on", cmd)) {
		ret = cnss_power_on_device(plat_priv, 0);
	} else if (sysfs_streq("off", cmd)) {
		cnss_power_off_device(plat_priv, 0);
	} else if (sysfs_streq("enumerate", cmd)) {
		ret = cnss_pci_init(plat_priv);
	} else if (sysfs_streq("download", cmd)) {
		set_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state);
		ret = cnss_pci_start_mhi(pci_priv);
	} else if (sysfs_streq("linkup", cmd)) {
		ret = cnss_resume_pci_link(pci_priv);
	} else if (sysfs_streq("linkdown", cmd)) {
		ret = cnss_suspend_pci_link(pci_priv);
	} else if (sysfs_streq("powerup", cmd)) {
		set_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state);
		ret = cnss_driver_event_post(plat_priv,
					     CNSS_DRIVER_EVENT_POWER_UP,
					     CNSS_EVENT_SYNC, NULL);
	} else if (sysfs_streq("shutdown", cmd)) {
		ret = cnss_driver_event_post(plat_priv,
					     CNSS_DRIVER_EVENT_POWER_DOWN,
					     0, NULL);
		clear_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state);
	} else if (sysfs_streq("assert", cmd)) {
		ret = cnss_force_fw_assert(&pci_priv->pci_dev->dev);
	} else {
		cnss_pr_err("Device boot debugfs command is invalid\n");
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static int cnss_dev_boot_debug_show(struct seq_file *s, void *data)
{
	seq_puts(s, "\nUsage: echo <action> > <debugfs_path>/cnss/dev_boot\n");
	seq_puts(s, "<action> can be one of below:\n");
	seq_puts(s, "on: turn on device power, assert WLAN_EN\n");
	seq_puts(s, "off: de-assert WLAN_EN, turn off device power\n");
	seq_puts(s, "enumerate: de-assert PERST, enumerate PCIe\n");
	seq_puts(s, "download: download FW and do QMI handshake with FW\n");
	seq_puts(s, "linkup: bring up PCIe link\n");
	seq_puts(s, "linkdown: bring down PCIe link\n");
	seq_puts(s, "powerup: full power on sequence to boot device, download FW and do QMI handshake with FW\n");
	seq_puts(s, "shutdown: full power off sequence to shutdown device\n");
	seq_puts(s, "assert: trigger firmware assert\n");

	return 0;
}

static int cnss_dev_boot_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_dev_boot_debug_show, inode->i_private);
}

static const struct file_operations cnss_dev_boot_debug_fops = {
	.read		= seq_read,
	.write		= cnss_dev_boot_debug_write,
	.release	= single_release,
	.open		= cnss_dev_boot_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_reg_read_debug_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	mutex_lock(&plat_priv->dev_lock);
	if (!plat_priv->diag_reg_read_buf) {
		seq_puts(s, "\nUsage: echo <mem_type> <offset> <data_len> > <debugfs_path>/cnss/reg_read\n");
		mutex_unlock(&plat_priv->dev_lock);
		return 0;
	}

	seq_printf(s, "\nRegister read, address: 0x%x memory type: 0x%x length: 0x%x\n\n",
		   plat_priv->diag_reg_read_addr,
		   plat_priv->diag_reg_read_mem_type,
		   plat_priv->diag_reg_read_len);

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 32, 4,
		     plat_priv->diag_reg_read_buf,
		     plat_priv->diag_reg_read_len, false);

	plat_priv->diag_reg_read_len = 0;
	kfree(plat_priv->diag_reg_read_buf);
	plat_priv->diag_reg_read_buf = NULL;
	mutex_unlock(&plat_priv->dev_lock);

	return 0;
}

static ssize_t cnss_reg_read_debug_write(struct file *fp,
					 const char __user *user_buf,
					 size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	u32 reg_offset, mem_type;
	u32 data_len = 0;
	u8 *reg_buf = NULL;
	const char *delim = " ";
	int ret = 0;

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Firmware is not ready yet\n");
		return -EINVAL;
	}

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &data_len))
		return -EINVAL;

	mutex_lock(&plat_priv->dev_lock);
	kfree(plat_priv->diag_reg_read_buf);
	plat_priv->diag_reg_read_buf = NULL;

	reg_buf = kzalloc(data_len, GFP_KERNEL);
	if (!reg_buf) {
		mutex_unlock(&plat_priv->dev_lock);
		return -ENOMEM;
	}

	ret = cnss_wlfw_athdiag_read_send_sync(plat_priv, reg_offset,
					       mem_type, data_len,
					       reg_buf);
	if (ret) {
		kfree(reg_buf);
		mutex_unlock(&plat_priv->dev_lock);
		return ret;
	}

	plat_priv->diag_reg_read_addr = reg_offset;
	plat_priv->diag_reg_read_mem_type = mem_type;
	plat_priv->diag_reg_read_len = data_len;
	plat_priv->diag_reg_read_buf = reg_buf;
	mutex_unlock(&plat_priv->dev_lock);

	return count;
}

static int cnss_reg_read_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_reg_read_debug_show, inode->i_private);
}

static const struct file_operations cnss_reg_read_debug_fops = {
	.read		= seq_read,
	.write		= cnss_reg_read_debug_write,
	.open		= cnss_reg_read_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_reg_write_debug_show(struct seq_file *s, void *data)
{
	seq_puts(s, "\nUsage: echo <mem_type> <offset> <reg_val> > <debugfs_path>/cnss/reg_write\n");

	return 0;
}

static ssize_t cnss_reg_write_debug_write(struct file *fp,
					  const char __user *user_buf,
					  size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	u32 reg_offset, mem_type, reg_val;
	const char *delim = " ";
	int ret = 0;

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Firmware is not ready yet\n");
		return -EINVAL;
	}

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_val))
		return -EINVAL;

	ret = cnss_wlfw_athdiag_write_send_sync(plat_priv, reg_offset, mem_type,
						sizeof(u32),
						(u8 *)&reg_val);
	if (ret)
		return ret;

	return count;
}

static int cnss_reg_write_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_reg_write_debug_show, inode->i_private);
}

static const struct file_operations cnss_reg_write_debug_fops = {
	.read		= seq_read,
	.write		= cnss_reg_write_debug_write,
	.open		= cnss_reg_write_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static ssize_t cnss_runtime_pm_debug_write(struct file *fp,
					   const char __user *user_buf,
					   size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	struct cnss_pci_data *pci_priv;
	char buf[64];
	char *cmd;
	unsigned int len = 0;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	cmd = buf;

	if (sysfs_streq("usage_count", cmd)) {
		cnss_pci_pm_runtime_show_usage_count(pci_priv);
	} else if (sysfs_streq("request_resume", cmd)) {
		ret = cnss_pci_pm_request_resume(pci_priv);
	} else if (sysfs_streq("resume", cmd)) {
		ret = cnss_pci_pm_runtime_resume(pci_priv);
	} else if (sysfs_streq("get", cmd)) {
		ret = cnss_pci_pm_runtime_get(pci_priv);
	} else if (sysfs_streq("get_noresume", cmd)) {
		cnss_pci_pm_runtime_get_noresume(pci_priv);
	} else if (sysfs_streq("put_autosuspend", cmd)) {
		ret = cnss_pci_pm_runtime_put_autosuspend(pci_priv);
	} else if (sysfs_streq("put_noidle", cmd)) {
		cnss_pci_pm_runtime_put_noidle(pci_priv);
	} else if (sysfs_streq("mark_last_busy", cmd)) {
		cnss_pci_pm_runtime_mark_last_busy(pci_priv);
	} else {
		cnss_pr_err("Runtime PM debugfs command is invalid\n");
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static int cnss_runtime_pm_debug_show(struct seq_file *s, void *data)
{
	seq_puts(s, "\nUsage: echo <action> > <debugfs_path>/cnss/runtime_pm\n");
	seq_puts(s, "<action> can be one of below:\n");
	seq_puts(s, "usage_count: get runtime PM usage count\n");
	seq_puts(s, "get: do runtime PM get\n");
	seq_puts(s, "get_noresume: do runtime PM get noresume\n");
	seq_puts(s, "put_noidle: do runtime PM put noidle\n");
	seq_puts(s, "put_autosuspend: do runtime PM put autosuspend\n");
	seq_puts(s, "mark_last_busy: do runtime PM mark last busy\n");

	return 0;
}

static int cnss_runtime_pm_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_runtime_pm_debug_show, inode->i_private);
}

static const struct file_operations cnss_runtime_pm_debug_fops = {
	.read		= seq_read,
	.write		= cnss_runtime_pm_debug_write,
	.open		= cnss_runtime_pm_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static ssize_t cnss_control_params_debug_write(struct file *fp,
					       const char __user *user_buf,
					       size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	unsigned int prev_board_id;
	char buf[64];
	char *sptr, *token;
	char *cmd;
	u32 val;
	unsigned int len = 0;
	const char *delim = " ";
	int ret;

	if (!plat_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	cmd = token;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "quirks") == 0)
		plat_priv->ctrl_params.quirks = val;
	else if (strcmp(cmd, "mhi_timeout") == 0)
		plat_priv->ctrl_params.mhi_timeout = val;
	else if (strcmp(cmd, "qmi_timeout") == 0)
		plat_priv->ctrl_params.qmi_timeout = val;
	else if (strcmp(cmd, "bdf_type") == 0)
		plat_priv->ctrl_params.bdf_type = val;
	else if (strcmp(cmd, "time_sync_period") == 0)
		plat_priv->ctrl_params.time_sync_period = val;
	else if (strcmp(cmd, "board_id") == 0 &&
			(plat_priv->bus_type == CNSS_BUS_PCI)) {
		prev_board_id = plat_priv->board_info.board_id_override;
		plat_priv->board_info.board_id_override = val;
		ret = cnss_set_fw_type_and_name(plat_priv);
		if (ret) {
			cnss_pr_err("%s: Failed to override firmware type for %s\n",
				    __func__, plat_priv->device_name);
			plat_priv->board_info.board_id_override = prev_board_id;
			cnss_set_fw_type_and_name(plat_priv);
			return ret;
		}
		cnss_pr_dbg("Updated firmware type %s for %s\n",
			    plat_priv->firmware_name, plat_priv->device_name);
	} else {
		return -EINVAL;
	}

	return count;
}

static int cnss_show_quirks_state(struct seq_file *s,
				  struct cnss_plat_data *plat_priv)
{
	enum cnss_debug_quirks i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "quirks: 0x%lx (", plat_priv->ctrl_params.quirks);
	for (i = 0, state = plat_priv->ctrl_params.quirks;
	     state != 0; state >>= 1, i++) {
		if (!(state & 0x1))
			continue;
		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case LINK_DOWN_SELF_RECOVERY:
			seq_puts(s, "LINK_DOWN_SELF_RECOVERY");
			continue;
		case SKIP_DEVICE_BOOT:
			seq_puts(s, "SKIP_DEVICE_BOOT");
			continue;
		case USE_CORE_ONLY_FW:
			seq_puts(s, "USE_CORE_ONLY_FW");
			continue;
		case SKIP_RECOVERY:
			seq_puts(s, "SKIP_RECOVERY");
			continue;
		case QMI_BYPASS:
			seq_puts(s, "QMI_BYPASS");
			continue;
		case ENABLE_WALTEST:
			seq_puts(s, "WALTEST");
			continue;
		case ENABLE_PCI_LINK_DOWN_PANIC:
			seq_puts(s, "PCI_LINK_DOWN_PANIC");
			continue;
		case FBC_BYPASS:
			seq_puts(s, "FBC_BYPASS");
			continue;
		case ENABLE_DAEMON_SUPPORT:
			seq_puts(s, "DAEMON_SUPPORT");
			continue;
		case DISABLE_DRV:
			seq_puts(s, "DISABLE_DRV");
			continue;
		}

		seq_printf(s, "UNKNOWN-%d", i);
	}
	seq_puts(s, ")\n");
	return 0;
}

static int cnss_control_params_debug_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	seq_puts(s, "\nUsage: echo <params_name> <value> > <debugfs_path>/cnss/control_params\n");
	seq_puts(s, "<params_name> can be one of below:\n");
	seq_puts(s, "quirks: Debug quirks for driver\n");
	seq_puts(s, "mhi_timeout: Timeout for MHI operation in milliseconds\n");
	seq_puts(s, "qmi_timeout: Timeout for QMI message in milliseconds\n");
	seq_puts(s, "bdf_type: Type of board data file to be downloaded\n");
	seq_puts(s, "time_sync_period: Time period to do time sync with device in milliseconds\n");

	seq_puts(s, "\nCurrent value:\n");
	cnss_show_quirks_state(s, plat_priv);
	seq_printf(s, "mhi_timeout: %u\n", plat_priv->ctrl_params.mhi_timeout);
	seq_printf(s, "qmi_timeout: %u\n", plat_priv->ctrl_params.qmi_timeout);
	seq_printf(s, "bdf_type: %u\n", plat_priv->ctrl_params.bdf_type);
	seq_printf(s, "time_sync_period: %u\n",
		   plat_priv->ctrl_params.time_sync_period);
	if (plat_priv->bus_type == CNSS_BUS_PCI)
		seq_printf(s, "board_id: 0x%x\n",
			   plat_priv->board_info.board_id_override);

	return 0;
}

static int cnss_control_params_debug_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, cnss_control_params_debug_show,
			   inode->i_private);
}

static const struct file_operations cnss_control_params_debug_fops = {
	.read = seq_read,
	.write = cnss_control_params_debug_write,
	.open = cnss_control_params_debug_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};

static ssize_t cnss_ce_reg_info_debug_write(struct file *fp,
					    const char __user *user_buf,
					    size_t count, loff_t *off)
{
	u64 ce_bitmask;
	int ret;
	int ce;
	struct cnss_ce_base_addr *ce_object;
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;

	ret = kstrtou64_from_user(user_buf, count, 16, &ce_bitmask);
	if (ret)
		return ret;

	ce_object = register_ce_object(plat_priv);
	if (!ce_object) {
		cnss_pr_err("CE object is null\n");
		return -1;
	}
	if (ce_bitmask >= (1 << ce_object->max_ce_count))
		return -EINVAL;
	for (ce = 0; ce < ce_object->max_ce_count; ce++) {
		/* Each bit represents CEx register. The user can also dump the
		 * specific CE register(s).
		 * e.g: echo 0x5 > ce_info will dump CE0 and CE2 registers.
		 */
		if (ce_bitmask & (1 << ce))
			cnss_dump_ce_reg(plat_priv, ce, ce_object);
	}
	return count;
}

static int cnss_ce_reg_info_debug_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	cnss_pr_info("To print specific CEs, echo bitmask > ce_info\n\n");
	cnss_dump_all_ce_reg(plat_priv);

	return 0;
}

static int cnss_ce_reg_info_debug_open(struct inode *inode,
				       struct file *file)
{
	return single_open(file, cnss_ce_reg_info_debug_show,
			   inode->i_private);
}

static const struct file_operations cnss_ce_reg_debug_fops = {
	.read = seq_read,
	.write = cnss_ce_reg_info_debug_write,
	.open = cnss_ce_reg_info_debug_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};

static ssize_t cnss_dynamic_feature_write(struct file *fp,
					  const char __user *user_buf,
					  size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	int ret = 0;
	u64 val;

	ret = kstrtou64_from_user(user_buf, count, 0, &val);
	if (ret)
		return ret;

	plat_priv->dynamic_feature = val;
	ret = cnss_wlfw_dynamic_feature_mask_send_sync(plat_priv);
	if (ret < 0)
		return ret;

	return count;
}

static int cnss_dynamic_feature_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *cnss_priv = s->private;

	seq_printf(s, "dynamic_feature: 0x%llx\n", cnss_priv->dynamic_feature);

	return 0;
}

static int cnss_dynamic_feature_open(struct inode *inode,
				     struct file *file)
{
	return single_open(file, cnss_dynamic_feature_show,
			   inode->i_private);
}

static const struct file_operations cnss_dynamic_feature_fops = {
	.read = seq_read,
	.write = cnss_dynamic_feature_write,
	.open = cnss_dynamic_feature_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static ssize_t cnss_platform_features_write(struct file *fp,
				      const char __user *user_buf,
				      size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	u32 val;
	char buf[64];
	char *sptr, *token;
	char *cmd;
	unsigned int len = 0;
	const char *delim = " ";

	if (!plat_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	cmd = token;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "enable_cold_boot_support") == 0) {
		plat_priv->cold_boot_support = val;
		cnss_pr_info("Setting cold_boot_support=%u for instance_id 0x%x\n",
			     val, plat_priv->wlfw_service_instance_id);
	} else if (strcmp(cmd, "enable_qdss_tracing") == 0) {
		plat_priv->qdss_support = val;
		cnss_pr_info("Setting qdss_support=%u for instance_id 0x%x\n",
			     val, plat_priv->wlfw_service_instance_id);
#ifndef CONFIG_TARGET_SDX_WKK
			/* For 11BE chipsets, QDSS Memory will be allocated via
			 * DMA alloc instead of dts and if the QDSS feature is
			 * disabled in the firmware ini file, stop QDSS if
			 * already started and clear the memory.
			 */
			if (!val) {
				cnss_free_qdss_mem(plat_priv);
			}
#endif
	} else if (strcmp(cmd, "enable_hds_support") == 0) {
		plat_priv->hds_support = val;
		cnss_pr_info("Setting hds_support=%u for instance_id 0x%x\n",
			     val, plat_priv->wlfw_service_instance_id);
	} else if (strcmp(cmd, "enable_regdb_support") == 0) {
		plat_priv->regdb_support = val;
		cnss_pr_info("Setting regdb_support=%u for instance_id 0x%x\n",
			     val, plat_priv->wlfw_service_instance_id);
	} else if (strcmp(cmd, "trace_qdss") == 0) {
		switch (val) {
		case CNSS_QDSS_STOP:
			if (!test_bit(CNSS_QDSS_STARTED,
						&plat_priv->driver_state)) {
				cnss_pr_info("QDSS not started, ignoring stop command. 0x%lx\n",
					     plat_priv->driver_state);
				return -EINVAL;
			}
			if (sptr) {
				token = strsep(&sptr, delim);
				if (!token)
					return -EINVAL;
				if (kstrtou32(token, 0, &val))
					return -EINVAL;
			} else {
				if (cnss_check_be_target(plat_priv))
					val = QMI_WLANFW_QDSS_STOP_ALL_TRACE_BE;
				else
					val = QMI_WLANFW_QDSS_STOP_ALL_TRACE_LI;
			}
			cnss_wlfw_send_qdss_trace_mode_req(plat_priv,
						QMI_WLFW_QDSS_TRACE_OFF_V01,
						val);
			break;
		case CNSS_QDSS_START:
			if (test_bit(CNSS_QDSS_STARTED,
						&plat_priv->driver_state)) {
				cnss_pr_info("QDSS is already started: 0x%lx\n",
					     plat_priv->driver_state);
				return -EINVAL;
			}

			plat_priv->qdss_etr_sg_mode = 0;
			cnss_wlfw_qdss_dnld_send_sync(plat_priv);
			break;
		default:
			cnss_pr_err("Invalid arg. for %s\n",
				    plat_priv->device_name);
			break;
		}
	} else
		return -EINVAL;

	return count;
}

static int cnss_platform_features_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	seq_puts(s, "\nCurrent value:\n");

	seq_printf(s, "coldboot_support: %s\n",
		   plat_priv->cold_boot_support ? "enabled" : "disabled");
	seq_printf(s, "qdss_tracing: %s\n",
		   plat_priv->qdss_support ? "enabled" : "disabled");
	seq_printf(s, "hds_support: %s\n",
		   plat_priv->hds_support ? "enabled" : "disabled");
	seq_printf(s, "regdb_support: %s\n",
		   plat_priv->regdb_support ? "enabled" : "disabled");

	return 0;
}

static int cnss_platform_features_open(struct inode *inode,
				 struct file *file)
{
	return single_open(file, cnss_platform_features_show,
			   inode->i_private);
}

static const struct file_operations cnss_platform_features_fops = {
	.read = seq_read,
	.write = cnss_platform_features_write,
	.open = cnss_platform_features_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};
#else
static ssize_t cnss_hds_support_write(struct file *fp,
				      const char __user *user_buf,
				      size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	int ret = 0;
	u32 val;

	ret = kstrtou32_from_user(user_buf, count, 0, &val);
	if (ret)
		return ret;

	plat_priv->hds_support = !!val;

	return count;
}

static int cnss_hds_support_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	seq_printf(s, "hds_support: %s\n",
		   plat_priv->hds_support ? "true" : "false");

	return 0;
}

static int cnss_hds_support_open(struct inode *inode,
				 struct file *file)
{
	return single_open(file, cnss_hds_support_show,
			   inode->i_private);
}

static const struct file_operations cnss_hds_support_fops = {
	.read = seq_read,
	.write = cnss_hds_support_write,
	.open = cnss_hds_support_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0) || \
		LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static ssize_t cnss_pci_write_switch_link(struct file *fp,
					   const char __user *user_buf,
					   size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv = fp->private_data;
	u16 link_speed = 0, link_width = 0, pci_set_val = 0;

	if (kstrtou16_from_user(user_buf, count, 0, &pci_set_val))
		return -EFAULT;

	if (!plat_priv)
		return -ENODEV;

	/* The first nibble in a byte represents both pci link speed
	 * width. The first 2[0,1] bits in a nibble represent link speed.
	 * The next 2[2,3] bits represents link width.
	 * e.g: echo 0xB > pci_switch_link will set the PCI Generation
	 * to 3 and PCI lane width to 2. The possible values are,
	 * 1 <= Link speed <= 3
	 * 1 <= Link width <= 2
	 */
	link_speed = pci_set_val & CNSS_PCI_SWITCH_LINK_MASK;

	link_width = (pci_set_val >> 2) & CNSS_PCI_SWITCH_LINK_MASK;

	if (!link_speed || !link_width) {
		cnss_pr_info("Invalid data\n");
		return -EFAULT;
	}

	cnss_set_pci_link_speed_width(&plat_priv->plat_dev->dev, link_speed,
				link_width);

	return count;
}

static ssize_t cnss_pci_read_switch_link(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	const char buf[] =
	"PCI SWITCH LINk USAGE :\n"
	"The first 2[0,1] bits in a nibble represent link speed.\n"
	"The next 2[2,3] bits represents link width.\n"
	"echo 0xB > pci_switch_link ,will set the PCI Generation\n"
	"to 3 and PCI lane width to 2. The possible values are,\n"
	"1 <= Link speed <= 3\n"
	"1 <= Link width <= 2\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
}

static const struct file_operations cnss_pci_switch_link_fops = {
	.read		= cnss_pci_read_switch_link,
	.write		= cnss_pci_write_switch_link,
	.release	= single_release,
	.open		= simple_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,

};
#endif

static ssize_t cnss_mlo_config_read(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct cnss_mlo_group_info *mlo_group_info;
	struct cnss_mlo_chip_info *chip_info;
	int i, j, k, ret, num_groups = 0;
	int len = 0, l = 0, pos = 0;
	const int size = SZ_1K;
	unsigned long bitmap;
	char *buf, *cur;

	if (!enable_mlo_support) {
		pr_err("#MLO is disabled!\n");
		return 0;
	}

	for (i = 0; i < CNSS_MAX_MLO_GROUPS; i++) {
		mlo_group_info = &g_mlo_group_info[i];
		if (mlo_group_info->group_id == 0xff)
			continue;
		num_groups++;
	}

	buf = kzalloc(SZ_1K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	cur = kzalloc(SZ_64, GFP_KERNEL);
	if (!cur) {
		kfree(buf);
		return -ENOMEM;
	}

	len += scnprintf(buf, size, "mlo_max_num_groups=%u\n", num_groups);

	for (i = 0; i < num_groups; i++) {
		mlo_group_info = &g_mlo_group_info[i];
		len += scnprintf(buf + len, size - len,
				 "\n[MLO_GROUP_%u]\n",
				 mlo_group_info->group_id);
		len += scnprintf(buf + len, size - len,
				 "mlo_max_num_peers=%u\n",
				 mlo_group_info->max_num_peers);
		len += scnprintf(buf + len, size - len,
				 "mlo_num_chips=%u\n",
				 mlo_group_info->num_chips);

		bitmap = (unsigned long)mlo_group_info->soc_chip_bitmap;
		for_each_set_bit(pos, &bitmap, MAX_MLO_SOC_BITMASK)
			l += scnprintf(cur + l, SZ_64 - l, "%u,", pos);
		cur[strlen(cur) - 1] = '\0';
		len += scnprintf(buf + len, size - len,
				 "mlo_soc_chip_ids=%s\n", cur);

		for (j = 0; j < mlo_group_info->num_chips; j++) {
			chip_info = &mlo_group_info->chip_info[j];
			len += scnprintf(buf + len, size - len,
					 "\n[MLO_SOC_CHIP_%u]\n",
					 chip_info->soc_id);
			len += scnprintf(buf + len, size - len,
					 "mlo_chip_idx=%u\n",
					 chip_info->soc_id);
			len += scnprintf(buf + len, size - len,
					 "mlo_num_adj_chip=%u\n",
					 chip_info->num_adj_chips);

			if (!chip_info->num_adj_chips) {
				len += scnprintf(buf + len, size - len,
						 "mlo_adj_chip_idx=0\n");
				continue;
			}

			memset(cur, 0, SZ_64);
			for (k = 0, l = 0; k < chip_info->num_adj_chips; k++) {
				l += scnprintf(cur + l, SZ_64 - l, "%u,",
					       chip_info->adj_chip_ids[k]);
			}
			cur[strlen(cur) - 1] = '\0';
			len += scnprintf(buf + len, size - len,
					 "mlo_adj_chip_idx=%s\n", cur);
		}
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	kfree(cur);
	return ret;
}

const struct file_operations cnss_mlo_config_ini_debug_fops = {
	.read		= cnss_mlo_config_read,
	.open		= simple_open,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

int cnss_create_debug_only_node(struct cnss_plat_data *plat_priv)
{
	struct dentry *root_dentry = plat_priv->root_dentry;

	debugfs_create_file("dev_boot", 0600, root_dentry, plat_priv,
			    &cnss_dev_boot_debug_fops);
	debugfs_create_file("reg_read", 0600, root_dentry, plat_priv,
			    &cnss_reg_read_debug_fops);
	debugfs_create_file("reg_write", 0600, root_dentry, plat_priv,
			    &cnss_reg_write_debug_fops);
	debugfs_create_file("runtime_pm", 0600, root_dentry, plat_priv,
			    &cnss_runtime_pm_debug_fops);
	debugfs_create_file("control_params", 0600, root_dentry, plat_priv,
			    &cnss_control_params_debug_fops);
	debugfs_create_file("dynamic_feature", 0600, root_dentry, plat_priv,
			    &cnss_dynamic_feature_fops);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	debugfs_create_file("platform_features", 0600, root_dentry, plat_priv,
			    &cnss_platform_features_fops);
#else
	debugfs_create_file("hds_support", 0600, root_dentry, plat_priv,
			    &cnss_hds_support_fops);
#endif
	debugfs_create_file("ce_info", 0600, root_dentry, plat_priv,
			    &cnss_ce_reg_debug_fops);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0) || \
		LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	debugfs_create_file("pci_switch_link", 0600, root_dentry, plat_priv,
			    &cnss_pci_switch_link_fops);
#endif
	debugfs_create_file("pin_connect_result", 0644, root_dentry, plat_priv,
			    &cnss_pin_connect_fops);
	debugfs_create_file("stats", 0644, root_dentry, plat_priv,
			    &cnss_stats_fops);
	return 0;
}

#if IS_ENABLED(CONFIG_IPC_LOGGING)
int cnss_debug_init(void)
{
	struct cnss_plat_data *plat_priv = NULL;

	cnss_ipc_log_context = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
						      "cnss", 0);
	if (!cnss_ipc_log_context) {
		cnss_pr_info("IPC Logging is disabled!\n");
		return -EINVAL;
	}

	cnss_ipc_log_long_context = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
							   "cnss-long", 0);
	if (!cnss_ipc_log_long_context) {
		cnss_pr_info("IPC long logging is disabled!\n");
		ipc_log_context_destroy(cnss_ipc_log_context);
		return -EINVAL;
	}

	return 0;
}

void cnss_debug_deinit(void)
{
	if (cnss_ipc_log_long_context) {
		ipc_log_context_destroy(cnss_ipc_log_long_context);
		cnss_ipc_log_long_context = NULL;
	}

	if (cnss_ipc_log_context) {
		ipc_log_context_destroy(cnss_ipc_log_context);
		cnss_ipc_log_context = NULL;
	}
}
#else
int cnss_debug_init(void)
{
	return 0;
}
void cnss_debug_deinit(void) {}
#endif


