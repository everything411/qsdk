/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/msi.h>
#include <linux/of_reserved_mem.h>
#include <linux/irq.h>
#include <linux/module.h>
#include "include/cnss2.h"
#include <linux/debugfs.h>
#ifdef CONFIG_CNSS2_DMA_ALLOC
#include <linux/cma.h>
#endif
#ifdef CONFIG_CNSS2_KERNEL_IPQ
#include <asm/cacheflush.h>
#endif
#ifdef KERNEL_SUPPORTS_QGIC2M
#include <soc/qcom/qgic2m.h>
#endif

#include "cnss_common/cnss_common.h"
#include "../main.h"
#if defined CNSS_DEBUG_SUPPORT
#include "debug/debug.h"
#endif
#if defined CNSS_PCI_SUPPORT
#include "pci/pci.h"
#endif
#include "bus/bus.h"
#include "legacyirq/legacyirq.h"
#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
#include <linux/devcoredump.h>
#include <linux/elf.h>
#else
#include <soc/qcom/ramdump.h>
#endif

struct dentry *cnss_root_dentry;

/* pciX_num_msi_bmap needs to be defined in format of 0xDPCEQM
 * where 0xDP denotes the number of MSIs available for DP, 0xCE denotes
 * the number of MSIs available for CE , 0xQ denotes the number of MSIs
 * available for QDSS streaming and 0xM denotes the number of MSIs
 * available for MHI. Total number of MSIs will be DP + CE + Q + M
 */
static unsigned int pci0_num_msi_bmap;
module_param(pci0_num_msi_bmap, uint, 0644);
MODULE_PARM_DESC(pci0_num_msi_bmap,
		 "Bitmap to indicate number of available MSIs for PCI 0");

static unsigned int pci1_num_msi_bmap;
module_param(pci1_num_msi_bmap, uint, 0644);
MODULE_PARM_DESC(pci1_num_msi_bmap,
		 "Bitmap to indicate number of available MSIs for PCI 1");

static unsigned int pci2_num_msi_bmap;
module_param(pci2_num_msi_bmap, uint, 0644);
MODULE_PARM_DESC(pci2_num_msi_bmap,
		 "Bitmap to indicate number of available MSIs for PCI 2");

static unsigned int pci3_num_msi_bmap;
module_param(pci3_num_msi_bmap, uint, 0644);
MODULE_PARM_DESC(pci3_num_msi_bmap,
		 "Bitmap to indicate number of available MSIs for PCI 3");

int log_level = CNSS_LOG_LEVEL_INFO;
EXPORT_SYMBOL(log_level);
module_param(log_level, int, 0644);
MODULE_PARM_DESC(log_level, "CNSS2 Module Log Level");

static void *mlo_global_mem[CNSS_MAX_MLO_GROUPS];
phys_addr_t mlo_global_mem_phys[CNSS_MAX_MLO_GROUPS];

void pci_update_msi_vectors(struct cnss_msi_config *msi_config,
				   char *user_name, int num_vectors,
				   int *vector_idx)
{
	int idx;

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			msi_config->users[idx].num_vectors = num_vectors;
			msi_config->users[idx].base_vector = *vector_idx;
			*vector_idx += num_vectors;
			return;
		}
	}
}

#ifdef CONFIG_CNSS2_QGIC2M
static struct cnss_msi_config msi_config_qcn6122_pci0 = {
	.total_vectors = 13,
	.total_users = 2,
	.users = (struct cnss_msi_user[]) {
		{ .name = "CE", .num_vectors = 5, .base_vector = 0 },
		{ .name = "DP", .num_vectors = 8, .base_vector = 5 },
	},
};

static struct cnss_msi_config msi_config_qcn6122_pci1 = {
	.total_vectors = 13,
	.total_users = 2,
	.users = (struct cnss_msi_user[]) {
		{ .name = "CE", .num_vectors = 5, .base_vector = 0 },
		{ .name = "DP", .num_vectors = 8, .base_vector = 5 },
	},
};

static struct cnss_msi_config msi_config_qcn9160_pci0 = {
	.total_vectors = 13,
	.total_users = 2,
	.users = (struct cnss_msi_user[]) {
		{ .name = "CE", .num_vectors = 5, .base_vector = 0 },
		{ .name = "DP", .num_vectors = 8, .base_vector = 5 },
	},
};

static struct cnss_msi_config msi_config_qcn6432_pci0 = {
	.total_vectors = 14,
	.total_users = 3,
	.users = (struct cnss_msi_user[]) {
		{ .name = "QDSS", .num_vectors = 1, .base_vector = 0 },
		{ .name = "CE", .num_vectors = 5, .base_vector = 1 },
		{ .name = "DP", .num_vectors = 8, .base_vector = 6 },
	},
};

static struct cnss_msi_config msi_config_qcn6432_pci1 = {
	.total_vectors = 14,
	.total_users = 3,
	.users = (struct cnss_msi_user[]) {
		{ .name = "QDSS", .num_vectors = 1, .base_vector = 0 },
		{ .name = "CE", .num_vectors = 5, .base_vector = 1 },
		{ .name = "DP", .num_vectors = 8, .base_vector = 6 },
	},
};

void cnss_qgic2_disable_msi(struct cnss_plat_data *plat_priv)
{
	if ((plat_priv->device_id == QCN6122_DEVICE_ID ||
	     plat_priv->device_id == QCN9160_DEVICE_ID ||
	     plat_priv->device_id == QCN6432_DEVICE_ID) &&
				plat_priv->tgt_data.qgic2_msi) {
		platform_msi_domain_free_irqs(&plat_priv->plat_dev->dev);
		plat_priv->tgt_data.qgic2_msi = NULL;
	}

}

struct cnss_msi_config*
cnss_get_msi_config(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->device_id == QCN6122_DEVICE_ID) {
		if (plat_priv->userpd_id == USERPD_0)
			return &msi_config_qcn6122_pci0;
		else if (plat_priv->userpd_id == USERPD_1)
			return &msi_config_qcn6122_pci1;

	} else if (plat_priv->device_id == QCN9160_DEVICE_ID) {
		return &msi_config_qcn9160_pci0;
	} else if (plat_priv->device_id == QCN6432_DEVICE_ID) {
		if (plat_priv->userpd_id == USERPD_0)
			return &msi_config_qcn6432_pci0;
		else if (plat_priv->userpd_id == USERPD_1)
			return &msi_config_qcn6432_pci1;
	}
	cnss_pr_err("Unknown userpd_id 0x%X", plat_priv->userpd_id);
	return NULL;
}

static void cnss_qgic2m_msg_handler(struct msi_desc *desc, struct msi_msg *msg)
{
	desc->msg.address_lo = msg->address_lo;
	desc->msg.address_hi = msg->address_hi;
	desc->msg.data = msg->data;
}

static irqreturn_t dummy_irq_handler(int irq, void *context)
{
	return IRQ_HANDLED;
}

struct qgic2_msi *cnss_qgic2_enable_msi(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct qgic2_msi *qgic;
	struct msi_desc *msi_desc;
	struct cnss_msi_config *msi_config;
	struct device *dev = &plat_priv->plat_dev->dev;
	struct irq_data *irq_data;

	msi_config = cnss_get_msi_config(plat_priv);
	if (!msi_config) {
		cnss_pr_err("%s msi_config NULL", plat_priv->device_name);
		return NULL;
	}

	cnss_override_msi_assignment(plat_priv, msi_config);
	ret = platform_msi_domain_alloc_irqs(&plat_priv->plat_dev->dev,
					     msi_config->total_vectors,
					     cnss_qgic2m_msg_handler);
	if (ret) {
		cnss_pr_err("platform_msi_domain_alloc_irqs failed %d\n", ret);
		return NULL;
	}

	qgic = devm_kzalloc(&plat_priv->plat_dev->dev,
			    sizeof(*qgic), GFP_KERNEL);
	if (!qgic) {
		cnss_pr_err("qgic alloc failed\n");
		platform_msi_domain_free_irqs(&plat_priv->plat_dev->dev);
		return NULL;
	}

	if (plat_priv->device_id == QCN6122_DEVICE_ID ||
	    plat_priv->device_id == QCN9160_DEVICE_ID ||
	    plat_priv->device_id == QCN6432_DEVICE_ID)
		plat_priv->tgt_data.qgic2_msi = qgic;

#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	msi_desc = msi_first_desc(dev, MSI_DESC_ALL);
#else
	msi_desc = first_msi_entry(dev);
#endif
	if (!msi_desc) {
		cnss_pr_err("msi_desc is NULL\n");
		return NULL;
	}

	irq_data = irq_get_irq_data(msi_desc->irq);
	if (!irq_data) {
		cnss_pr_err("irq_desc_get_irq_data failed.\n");
		platform_msi_domain_free_irqs(&plat_priv->plat_dev->dev);
		return NULL;
	}

	/* For multi-pd device, to retrieve msi base address and irq data,
	 * request a dummy irq  store the base address and data in qgic
	 * private to provide the required base address/data info in
	 * cnss_get_msi_address and cnss_get_user_msi_assignment API calls.
	 */
	ret = request_irq(msi_desc->irq, dummy_irq_handler,
			  IRQF_SHARED, "dummy", qgic);
	if (ret) {
		cnss_pr_err("dummy request_irq fails %d\n", ret);
		return NULL;
	}

	qgic->irq_num = msi_desc->irq;
	qgic->msi_gicm_base_data = msi_desc->msg.data;
	qgic->msi_gicm_addr_lo = msi_desc->msg.address_lo;
	qgic->msi_gicm_addr_hi = msi_desc->msg.address_hi;

	cnss_pr_dbg("irq %d msi addr lo 0x%x addr hi 0x%x msi data %d",
		    qgic->irq_num, qgic->msi_gicm_addr_lo,
		    qgic->msi_gicm_addr_hi, qgic->msi_gicm_base_data);

	free_irq(msi_desc->irq, qgic);

	return qgic;
}
#endif

int cnss_set_bar_addr(struct device *dev, void __iomem *mem)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("Plat Priv is null\n");
		return -ENODEV;
	}
	plat_priv->bar = mem;

	return 0;
}
EXPORT_SYMBOL(cnss_set_bar_addr);

enum cnss_dev_bus_type cnss_get_bus_type(unsigned long device_id)
{
	switch (device_id) {
	case QCA6174_DEVICE_ID:
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		return CNSS_BUS_PCI;
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		return CNSS_BUS_AHB;
	default:
		pr_err("Unknown device_id: 0x%lx\n", device_id);
		return CNSS_BUS_NONE;
	}
}

struct device_node *cnss_get_etr_dev_node(struct cnss_plat_data *plat_priv)
{
	struct device_node *dev_node = NULL;
	char buf[ETR_DEV_NODE_LEN] = {0};

	if (plat_priv->device_id == QCN6122_DEVICE_ID) {
		snprintf(buf, ETR_DEV_NODE_LEN, "%s_%d",
			QCN6122_ETR_DEV_NODE_PREFIX, plat_priv->userpd_id);
		dev_node = of_find_node_by_name(NULL, buf);
	} else if (plat_priv->device_id == QCN9160_DEVICE_ID) {
		snprintf(buf, ETR_DEV_NODE_LEN, "%s_%d",
			QCN9160_ETR_DEV_NODE_PREFIX, plat_priv->userpd_id);
		dev_node = of_find_node_by_name(NULL, buf);
	} else if (plat_priv->device_id == QCN6432_DEVICE_ID) {
		snprintf(buf, ETR_DEV_NODE_LEN, "%s_%d",
		QCN6432_ETR_DEV_NODE_PREFIX, plat_priv->userpd_id);
		dev_node = of_find_node_by_name(NULL, buf);
	} else {
		dev_node = of_find_node_by_name(NULL, "q6_etr_dump");
	}

	return dev_node;
}


static enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev)
{
	if (!dev)
		return CNSS_BUS_NONE;

	if (!dev->bus)
		return CNSS_BUS_NONE;

	if (memcmp(dev->bus->name, "pci", 3) == 0)
		return CNSS_BUS_PCI;
	else if (memcmp(dev->bus->name, "platform", 8) == 0)
		return CNSS_BUS_AHB;
	else
		return CNSS_BUS_NONE;
}

void *cnss_bus_dev_to_bus_priv(struct device *dev)
{
	if (!dev)
		return NULL;

	switch (cnss_get_dev_bus_type(dev)) {
#ifdef CNSS_PCI_SUPPORT
	case CNSS_BUS_PCI:
		return cnss_get_pci_priv(to_pci_dev(dev));
#endif
	case CNSS_BUS_AHB:
		return NULL;
	default:
		return NULL;
	}
}

struct cnss_plat_data *cnss_bus_dev_to_plat_priv(struct device *dev)
{
#ifdef CNSS_PCI_SUPPORT
	void *bus_priv;
	struct pci_dev *pdev;
#endif
	if (!dev)
		return NULL;

	switch (cnss_get_dev_bus_type(dev)) {
#ifdef CNSS_PCI_SUPPORT
	case CNSS_BUS_PCI:
		pdev = to_pci_dev(dev);
		if (pdev->device != QCN9000_DEVICE_ID &&
		    pdev->device != QCN9224_DEVICE_ID)
			return NULL;

		bus_priv = cnss_bus_dev_to_bus_priv(dev);
		if (bus_priv)
			return cnss_pci_priv_to_plat_priv(bus_priv);
		else
			return NULL;
#endif
	case CNSS_BUS_AHB:
		return cnss_get_plat_priv(to_platform_device(dev));
	default:
		return NULL;
	}
}

int cnss_bus_init_by_type(int type)
{
	switch (type) {
#ifdef CNSS_PCI_SUPPORT
	case CNSS_BUS_PCI:
		return cnss_pci_init(NULL);
#endif
	case CNSS_BUS_AHB:
		return 0;
	default:
		pr_err("Unsupported bus type: %d\n",
		       type);
		return -EINVAL;
	}
}

void cnss_override_msi_assignment(struct cnss_plat_data *plat_priv,
					struct cnss_msi_config *msi_config)
{
	u32 num_mhi_vectors;
	u32 num_ce_vectors;
	u32 num_dp_vectors;
	u32 num_qdss_vectors = 0;
	u32 interrupt_bmap = 0;
	u32 bmap = 0;
	int vector_idx = 0;
	struct device *dev = &plat_priv->plat_dev->dev;

	if (plat_priv->qrtr_node_id == QCN9000_0 ||
	    plat_priv->userpd_id == USERPD_0 ||
	    plat_priv->qrtr_node_id == QCN9224_0)
		bmap = pci0_num_msi_bmap;

	if (plat_priv->qrtr_node_id == QCN9000_1 ||
	    plat_priv->userpd_id == USERPD_1 ||
	    plat_priv->qrtr_node_id == QCN9224_1)
		bmap = pci1_num_msi_bmap;

	if (plat_priv->qrtr_node_id == QCN9224_2 ||
	    plat_priv->qrtr_node_id == QCN9000_2)
		bmap = pci2_num_msi_bmap;

	if (plat_priv->qrtr_node_id == QCN9000_3 ||
	    plat_priv->qrtr_node_id == QCN9224_3)
		bmap = pci3_num_msi_bmap;

	if (bmap) {
		interrupt_bmap = bmap;
	} else if (!of_property_read_u32(dev->of_node, "interrupt-bmap",
		   &bmap)) {
		interrupt_bmap = bmap;
	} else {
		return;
	}

	num_mhi_vectors = (interrupt_bmap & MSI_MHI_VECTOR_MASK) >>
			   MSI_MHI_VECTOR_SHIFT;
	num_qdss_vectors = (interrupt_bmap & MSI_QDSS_VECTOR_MASK) >>
			   MSI_QDSS_VECTOR_SHIFT;
	num_ce_vectors = (interrupt_bmap & MSI_CE_VECTOR_MASK) >>
			  MSI_CE_VECTOR_SHIFT;
	num_dp_vectors = (interrupt_bmap & MSI_DP_VECTOR_MASK) >>
			  MSI_DP_VECTOR_SHIFT;

	if (num_mhi_vectors < MIN_MHI_VECTORS ||
	    num_mhi_vectors > MAX_MHI_VECTORS)
		num_mhi_vectors = DEFAULT_MHI_VECTORS;

	if (num_ce_vectors < MIN_CE_VECTORS ||
	    num_ce_vectors > MAX_CE_VECTORS)
		num_ce_vectors = DEFAULT_CE_VECTORS;

	if (num_dp_vectors < MIN_DP_VECTORS ||
	    num_dp_vectors > MAX_DP_VECTORS)
		num_dp_vectors = DEFAULT_DP_VECTORS;

	if (num_qdss_vectors < MIN_QDSS_VECTORS ||
	    num_qdss_vectors > MAX_QDSS_VECTORS)
		num_qdss_vectors = DEFAULT_QDSS_VECTORS;

	pci_update_msi_vectors(msi_config, "MHI", num_mhi_vectors, &vector_idx);
	pci_update_msi_vectors(msi_config, "QDSS", num_qdss_vectors,
				&vector_idx);
	pci_update_msi_vectors(msi_config, "CE", num_ce_vectors, &vector_idx);
	pci_update_msi_vectors(msi_config, "DP", num_dp_vectors, &vector_idx);
	msi_config->total_vectors = num_mhi_vectors + num_ce_vectors +
					num_dp_vectors + num_qdss_vectors;
	/* Linux only allows number of interrupts to be a power of 2 */
	msi_config->total_vectors =
			1U << get_count_order(msi_config->total_vectors);
}

#ifndef CONFIG_TARGET_SDX_WKK
static int cnss_mlo_mem_get(struct cnss_plat_data *plat_priv, int group_id,
			    phys_addr_t paddr, int idx, u32 mem_size)
{
	mlo_global_mem_phys[group_id] = paddr;
	mlo_global_mem[group_id] = ioremap(mlo_global_mem_phys[group_id],
					   mem_size);

	if (!mlo_global_mem[group_id])
		cnss_pr_err("WARNING: Host DDR remap failed\n");

	return 0;
}

static int get_mlo_pa(struct cnss_plat_data *plat_priv, int group_id, int idx)
{
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;

	fw_mem[idx].pa = mlo_global_mem_phys[group_id];
	return 0;
}

#else
static int cnss_mlo_mem_get(struct cnss_plat_data *plat_priv, int group_id,
			    phys_addr_t paddr, int idx, u32 mem_size)
{
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret;
	struct device *dev;
	struct page *page = NULL;

	dev = &plat_priv->plat_dev->dev;

	ret = of_reserved_mem_device_init_by_idx(dev,
		   plat_priv->plat_dev->dev.of_node, group_id);
	if (ret != 0) {
		cnss_pr_err("Error(%d): of_reserved_mem_device_init_by_idx failed.",
			    ret);
		return -ENOMEM;
	}

	page = cma_alloc(dev->cma_area,
			 DIV_ROUND_UP(fw_mem[idx].size, PAGE_SIZE), 0,
			 false);
	if (page == NULL) {
		cnss_pr_err("Error: cma alloc failed.\n");
		return -ENOMEM;
	}

	mlo_global_mem[group_id] = page_to_virt(page);
	mlo_global_mem_phys[group_id] = page_to_phys(page);
	of_reserved_mem_device_release(dev);

	return 0;
}

static int get_mlo_pa(struct cnss_plat_data *plat_priv, int group_id, int idx,
			unsigned int iova_base, int flag)
{
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	int ret;

	if (iova_base == 0) {
		cnss_pr_err("Error: invalid data: 0x%x\n", iova_base);
		return -EINVAL;
	}

	/*remap alocated mlo shared mem to pcie device*/
	if (mlo_global_mem_phys[group_id] != iova_base) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
		ret = iommu_map(pci_priv->iommu_domain, iova_base,
				mlo_global_mem_phys[group_id],
				fw_mem[idx].size, flag, GFP_KERNEL);
#else
		ret = iommu_map(pci_priv->iommu_domain, iova_base,
				mlo_global_mem_phys[group_id],
				fw_mem[idx].size, flag);
#endif
		if (ret < 0) {
			cnss_pr_err("Error: MLO memory map failed.\n");
			return -ENOMEM;
		}
	}

	fw_mem[idx].pa = iova_base;
	mlo_global_mem_phys[group_id] = iova_base;

	return 0;
}
#endif

int cnss_mlo_mem_alloc(struct cnss_plat_data *plat_priv, int index)
{
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret, group_id;
	char mlo_node_name[20];
	struct device_node *mlo_global_mem_node = NULL;
	struct reserved_mem *mlo_mem = NULL;
	unsigned int mlo_global_mem_size;
	int i = index;
#ifdef CONFIG_TARGET_SDX_WKK
	int flag = IOMMU_READ | IOMMU_WRITE;
	bool dma_coherent = false;
	static unsigned int mlo_iova_base[CNSS_MAX_MLO_GROUPS];
#endif

	group_id = plat_priv->mlo_group_info->group_id;
	if (!mlo_global_mem[group_id]) {
		snprintf(mlo_node_name, sizeof(mlo_node_name),
				"mlo_global_mem_%d", group_id);
		mlo_global_mem_node =
			of_find_node_by_name(NULL, mlo_node_name);
		if (!mlo_global_mem_node) {
			cnss_pr_err("could not get mlo_global_mem_node\n");
			CNSS_ASSERT(0);
			return -ENOMEM;
		}

		mlo_mem = of_reserved_mem_lookup(mlo_global_mem_node);
		if (!mlo_mem) {
			cnss_pr_err("%s: Unable to get mlo_mem",
					__func__);
			of_node_put(mlo_global_mem_node);
			CNSS_ASSERT(0);
			return -ENOMEM;
		}

#ifdef CONFIG_TARGET_SDX_WKK
		ret = of_property_read_u32(mlo_global_mem_node, "iova_base",
					   &mlo_iova_base[group_id]);
		if (ret)
			cnss_pr_err("Error(%d): Unable to get MLO iova base\n",
				    ret);
		dma_coherent =
			of_property_read_bool(mlo_global_mem_node,
						"dma-coherent");
		cnss_pr_dbg("MLO memory dma-coherent is %s\n",
				dma_coherent ? "enabled" : "disabled");
		if (dma_coherent)
			flag |= IOMMU_CACHE;
#endif

		of_node_put(mlo_global_mem_node);
		mlo_global_mem_size = mlo_mem->size;
		if (fw_mem[i].size > mlo_global_mem_size) {
			cnss_pr_err("Error: Need more memory 0x%x\n",
					(unsigned int)fw_mem[i].size);
			CNSS_ASSERT(0);
			return -ENOMEM;
		}

		if (fw_mem[i].size < mlo_global_mem_size) {
			cnss_pr_err("WARNING: More MLO global memory is reserved. Reserved size 0x%x, Requested size 0x%x.\n",
					mlo_global_mem_size,
					(unsigned int)fw_mem[i].size);
		}

		ret = cnss_mlo_mem_get(plat_priv, group_id, mlo_mem->base, i,
				       mlo_global_mem_size);
		if (ret != 0) {
			cnss_pr_err("Error(%d): cnss_mlo_mem_get failed.\n",
				    ret);
			CNSS_ASSERT(0);
		}

		fw_mem[i].va = mlo_global_mem[group_id];
	} else
		fw_mem[i].va = mlo_global_mem[group_id];

#ifdef CONFIG_TARGET_SDX_WKK
	ret = get_mlo_pa(plat_priv, group_id, i, mlo_iova_base[group_id], flag);
#else
	ret = get_mlo_pa(plat_priv, group_id, i);
#endif
	if (ret != 0) {
		cnss_pr_err("Error: get_mlo_pa failed.");
		CNSS_ASSERT(0);
	}

	if (mlo_global_mem[group_id])
		cnss_do_mlo_global_memset(plat_priv, fw_mem[i].size);

	if (!fw_mem[i].va) {
		cnss_pr_err("Failed to allocate memory for FW, size: 0x%zx, type: %u\n",
				fw_mem[i].size,
				fw_mem[i].type);
		return -ENOMEM;
	}

	return 0;
}

static bool cnss_get_mlo_group_master_chip(struct cnss_plat_data *plat_priv)
{
	int master_chip_idx = 0;
	struct cnss_mlo_group_info *mlo_group_info;

	if (!plat_priv || !plat_priv->mlo_support)
		return false;

	if (!plat_priv->mlo_capable || !plat_priv->mlo_chip_info ||
	    !plat_priv->mlo_group_info)
		return false;

	mlo_group_info = plat_priv->mlo_group_info;

	master_chip_idx = cnss_get_mlo_master_chip_id(mlo_group_info);

	cnss_pr_dbg("%s, Master chip idx %d\n", __func__, master_chip_idx);

	return plat_priv->mlo_chip_info->chip_id ==
		mlo_group_info->chip_info[master_chip_idx].chip_id;
}

void cnss_do_mlo_global_memset(struct cnss_plat_data *plat_priv, u64 mem_size)
{
	if ((plat_priv->recovery_mode == MODE_1_RECOVERY_MODE) ||
	    (plat_priv->standby_mode) || (plat_priv->wsi_remap_state) ||
	    (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)))
		return;

	if (!cnss_get_mlo_group_master_chip(plat_priv))
		return;

	cnss_pr_info("Resetting the MLO Global mem, memory size is %lld\n",
		     mem_size);
	/* Reset the Shared memory only for the first invocation */
	memset_io(mlo_global_mem[plat_priv->mlo_group_info->group_id], 0,
		  mem_size);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
void cnss_etr_sg_tbl_free(uint32_t *vaddr,
			  struct cnss_plat_data *plat_priv, uint32_t ents)
{
}
int cnss_etr_sg_tbl_alloc(struct cnss_plat_data *plat_priv)
{
	return 0;
}
#else
static void cnss_etr_sg_tbl_flush(uint32_t *vaddr,
				  struct cnss_plat_data *plat_priv)
{
	uint32_t i = 0, pte_n = 0, last_pte;
	uint32_t *virt_st_tbl, *virt_pte;
	void *virt_blk;
	phys_addr_t phys_pte;
	struct cnss_fw_mem qdss_mem = plat_priv->qdss_mem;
	int total_ents = DIV_ROUND_UP(qdss_mem.size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_st_tbl = vaddr;
	dmac_flush_range((void *)virt_st_tbl, (void *)virt_st_tbl + PAGE_SIZE);

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = virt_st_tbl + pte_n;
			phys_pte = CNSS_ETR_SG_ENT_TO_BLK(*virt_pte);
			virt_blk = phys_to_virt(phys_pte);

				dmac_flush_range(virt_blk, virt_blk +
					(2 * PAGE_SIZE));
			if ((last_pte - i) > 1) {
				pte_n++;
			} else if (last_pte != total_ents) {
				virt_st_tbl = (uint32_t *)virt_blk;
				pte_n = 0;
				break;
			}
			i++;
		}
	}
}

void cnss_etr_sg_tbl_free(uint32_t *vaddr,
				 struct cnss_plat_data *plat_priv,
				 uint32_t ents)
{
	uint32_t i = 0, pte_n = 0, last_pte;
	uint32_t *virt_st_tbl, *virt_pte;
	void *virt_blk;
	phys_addr_t phys_pte;
	struct cnss_fw_mem qdss_mem = plat_priv->qdss_mem;
	int total_ents = DIV_ROUND_UP(qdss_mem.size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	if (vaddr == NULL)
		return;

	virt_st_tbl = vaddr;

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = virt_st_tbl + pte_n;

			/* Do not go beyond number of entries allocated */
			if (i == ents) {
				free_page((unsigned long)virt_st_tbl);
				return;
			}

			phys_pte = CNSS_ETR_SG_ENT_TO_BLK(*virt_pte);
			virt_blk = phys_to_virt(phys_pte);

			if ((last_pte - i) > 1) {
				free_pages((unsigned long)virt_blk, 1);
				pte_n++;
			} else if (last_pte == total_ents) {
				free_pages((unsigned long)virt_blk, 1);
				free_page((unsigned long)virt_st_tbl);
			} else {
				free_page((unsigned long)virt_st_tbl);
				virt_st_tbl = (uint32_t *)virt_blk;
				pte_n = 0;
				break;
			}
			i++;
		}
	}
}

int cnss_etr_sg_tbl_alloc(struct cnss_plat_data *plat_priv)
{
	int ret;
	uint32_t i = 0, last_pte;
	uint32_t *virt_pgdir, *virt_st_tbl;
	void *virt_pte;
	struct cnss_fw_mem qdss_mem = plat_priv->qdss_mem;
	struct qdss_stream_data *qdss_stream = &plat_priv->qdss_stream;
	int total_ents = DIV_ROUND_UP(qdss_mem.size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_pgdir = (uint32_t *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 0);

	if (!virt_pgdir)
		return -ENOMEM;

	virt_st_tbl = virt_pgdir;

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = (void *)__get_free_pages(GFP_KERNEL |
				     __GFP_ZERO, 1);
			if (!virt_pte) {
				ret = -ENOMEM;
				goto err;
			}

			if ((last_pte - i) > 1) {
				*virt_st_tbl =
				    CNSS_ETR_SG_ENT(virt_to_phys(virt_pte));
				virt_st_tbl++;
			} else if (last_pte == total_ents) {
				*virt_st_tbl =
				    CNSS_ETR_SG_LST_ENT(virt_to_phys(virt_pte));
			} else {
				*virt_st_tbl =
				    CNSS_ETR_SG_NXT_TBL(virt_to_phys(virt_pte));
				virt_st_tbl = (uint32_t *)virt_pte;
				break;
			}
			i++;
		}
	}

	qdss_stream->qdss_vaddr = virt_pgdir;
	qdss_stream->qdss_paddr = virt_to_phys(virt_pgdir);

	/* Flush the dcache before proceeding */
	cnss_etr_sg_tbl_flush((uint32_t *)qdss_stream->qdss_vaddr, plat_priv);

	cnss_pr_dbg("%s: table starts at %#lx, total entries %d\n",
		__func__, (unsigned long)qdss_stream->qdss_paddr, total_ents);

	return 0;
err:
	cnss_etr_sg_tbl_free(virt_pgdir, plat_priv, i);
	return ret;
}
#endif

void cnss_free_soc_info(struct cnss_plat_data *plat_priv)
{
	/* Free SOC specific resources like memory remapped for PCI BAR */
	switch (plat_priv->device_id) {
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
		/* For PCI targets, BAR is freed from cnss_pci_disable_bus */
		break;
	case QCN6122_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCN6432_DEVICE_ID:
		/* QCN6122/QCN9160/QCN6432 are considered AHB targets from host
		 * but is actually a PCI target where enumeration is handled by
		 * the firmware PCI BAR is remmaped as part of QMI Device Info
		 * message.
		 * iounmap the PCI BAR memory here
		 */
		if (plat_priv->tgt_data.bar_addr_va) {
			cnss_pr_info("Freeing BAR Info for %s",
				     plat_priv->device_name);
			iounmap(plat_priv->tgt_data.bar_addr_va);
			plat_priv->tgt_data.bar_addr_va = NULL;
			plat_priv->tgt_data.bar_addr_pa = 0;
			plat_priv->tgt_data.bar_size = 0;
		}
		break;
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		/* PCI BAR not applicable for other AHB targets */
		break;
	default:
		break;
	}
}

void afc_memset(struct cnss_plat_data *plat_priv, void *s,
		       int c, size_t n)
{
	switch (plat_priv->device_id) {
	case QCN9160_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCN6432_DEVICE_ID:
		/* For QCN6122, QCN9160, AFC memory is ioremapped from
		 * M3_DUMP_REGION.
		 * Use memset_io for this.
		 */
		memset_io(s, c, n);
		break;
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
		memset(s, c, n);
		break;
	default:
		cnss_pr_err("Wrong target type for AFCMEM 0x%lX",
			    plat_priv->device_id);
		break;
	}

}

int cnss_send_buffer_to_afcmem(struct device *dev, char *afcdb, uint32_t len,
			       uint8_t slotid)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_fw_mem *fw_mem;
	void *mem = NULL;
	int i, ret;
	u32 *status;

	if (!plat_priv)
		return -EINVAL;

	fw_mem = plat_priv->fw_mem;
	if (slotid >= AFC_MAX_SLOT) {
		cnss_pr_err("Invalid slot id %d\n", slotid);
		ret = -EINVAL;
		goto err;
	}
	if (len > AFC_SLOT_SIZE) {
		cnss_pr_err("len %d greater than slot size", len);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].type == QMI_WLFW_AFC_MEM_V01) {
			mem = fw_mem[i].va;
			status = mem + (slotid * AFC_SLOT_SIZE);
			break;
		}
	}

	if (!mem) {
		cnss_pr_err("AFC mem is not available\n");
		ret = -ENOMEM;
		goto err;
	}

	status[AFC_AUTH_STATUS_OFFSET] = cpu_to_le32(AFC_AUTH_ERROR);
	afc_memset(plat_priv, mem + (slotid * AFC_SLOT_SIZE), 0, AFC_SLOT_SIZE);
	memcpy(mem + (slotid * AFC_SLOT_SIZE), afcdb, len);
	status[AFC_AUTH_STATUS_OFFSET] = cpu_to_le32(AFC_AUTH_SUCCESS);

	return 0;
err:
	return ret;
}
EXPORT_SYMBOL(cnss_send_buffer_to_afcmem);

int cnss_reset_afcmem(struct device *dev, uint8_t slotid)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_fw_mem *fw_mem;
	void *mem = NULL;
	int i, ret;

	if (!plat_priv)
		return -EINVAL;

	fw_mem = plat_priv->fw_mem;
	if (slotid >= AFC_MAX_SLOT) {
		cnss_pr_err("Invalid slot id %d\n", slotid);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].type == QMI_WLFW_AFC_MEM_V01) {
			mem = fw_mem[i].va;
			break;
		}
	}

	if (!mem) {
		cnss_pr_err("AFC mem is not available\n");
		ret = -ENOMEM;
		goto err;
	}

	afc_memset(plat_priv, mem + (slotid * AFC_SLOT_SIZE), 0, AFC_SLOT_SIZE);
	return 0;

err:
	return ret;
}
EXPORT_SYMBOL(cnss_reset_afcmem);

bool cnss_get_enable_intx(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return false;

	return plat_priv->enable_intx;
}
EXPORT_SYMBOL(cnss_get_enable_intx);

bool cnss_wait_for_rddm_complete(struct cnss_plat_data *plat_priv)
{
	int count = 0;

	if (!plat_priv)
		return true;

	if (test_bit(CNSS_RDDM_DUMP_IN_PROGRESS, &plat_priv->driver_state)) {
		cnss_pr_dbg("Waiting for RDDM collection for device 0x%lx\n",
			      plat_priv->device_id);
		while (test_bit(CNSS_RDDM_DUMP_IN_PROGRESS,
		       &plat_priv->driver_state)) {
			msleep(RDDM_DONE_DELAY);
			if (count++ > rddm_done_timeout * 10) {
				cnss_pr_err("RDDM collection timed-out %d seconds\n",
					    rddm_done_timeout);
				CNSS_ASSERT(0);
			}
		}
		cnss_pr_dbg("RDDM collection wait ended for device 0x%lx\n",
			     plat_priv->device_id);
	}

	return true;
}

static ssize_t cnss_qmi_record_debug_write(struct file *fp,
					   const char __user *user_buf,
					   size_t count, loff_t *off)
{
	char buf[4];

	if (copy_from_user(buf, user_buf, 4))
		return -EFAULT;
	qmi_record(buf[0], 0xD000 | buf[1], buf[2], buf[3]);
	return count;
}

static int cnss_qmi_record_debug_show(struct seq_file *s, void *data)
{
	cnss_dump_qmi_history();
	return 0;
}

static int cnss_qmi_record_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_qmi_record_debug_show, inode->i_private);
}

static const struct file_operations cnss_qmi_record_debug_fops = {
	.read		= seq_read,
	.write		= cnss_qmi_record_debug_write,
	.release	= single_release,
	.open		= cnss_qmi_record_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_mlo_config_debug_show(struct seq_file *s, void *data)
{
	cnss_print_mlo_config();
	return 0;
}

static int cnss_mlo_config_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_mlo_config_debug_show, inode->i_private);
}

static const struct file_operations cnss_mlo_config_debug_fops = {
	.read		= seq_read,
	.release	= single_release,
	.open		= cnss_mlo_config_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

int cnss_debugfs_create(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct dentry *root_dentry = NULL;

	if (!cnss_root_dentry) {
		cnss_root_dentry = debugfs_create_dir("cnss", 0);
		if (IS_ERR(cnss_root_dentry)) {
			ret = PTR_ERR(cnss_root_dentry);
			cnss_pr_err("Unable to create debugfs %d\n", ret);
			goto out;
		}

		/* Create qmi_record under /sys/kernel/debug/cnss2/ */
		debugfs_create_file("qmi_record", 0600, cnss_root_dentry, NULL,
				    &cnss_qmi_record_debug_fops);
		debugfs_create_file("mlo_config", 0600, cnss_root_dentry, NULL,
				    &cnss_mlo_config_debug_fops);
		debugfs_create_file("mlo_config_ini", 0600, cnss_root_dentry,
				    NULL, &cnss_mlo_config_ini_debug_fops);
	}

	root_dentry = debugfs_create_dir((char *)&plat_priv->device_name,
					 cnss_root_dentry);
	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		cnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}
	plat_priv->root_dentry = root_dentry;

#ifdef CNSS_DEBUG_SUPPORT
	cnss_create_debug_only_node(plat_priv);
#endif

out:
	return ret;
}

void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv)
{
	if (cnss_root_dentry) {
		debugfs_remove_recursive(cnss_root_dentry);
		cnss_root_dentry = NULL;
	}
	plat_priv->root_dentry = NULL;
}

int cnss_get_pci_slot(struct device *dev)
{
	struct cnss_plat_data *plat_priv =
		cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -ENODEV;

	return plat_priv->pci_slot_id;
}
EXPORT_SYMBOL(cnss_get_pci_slot);
