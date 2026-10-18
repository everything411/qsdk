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

#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_address.h>
#ifdef CONFIG_CNSS2_DMA_ALLOC
#include <linux/cma.h>
#endif
#ifdef CONFIG_CNSS2_KERNEL_IPQ
#include <asm/cacheflush.h>
#endif
#ifdef KERNEL_SUPPORTS_QGIC2M
#include <soc/qcom/qgic2m.h>
#endif

#include "../main.h"
#ifdef CNSS_DEBUG_SUPPORT
#include "debug/debug.h"
#endif
#include "bus/bus.h"
#include "cnss_common/cnss_common.h"
#include "legacyirq/legacyirq.h"
#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
#include <linux/devcoredump.h>
#include <linux/elf.h>
#else
#include <soc/qcom/ramdump.h>
#endif

static DEFINE_SPINLOCK(pci_remote_reg_window_lock);

static
struct device_node *cnss_get_m3dump_dev_node(struct cnss_plat_data *plat_priv)
{
	struct device_node *dev_node = NULL;
	char buf[M3_DUMP_NODE_LEN] = {0};

	if (plat_priv->device_id == QCN6122_DEVICE_ID) {
		snprintf(buf, M3_DUMP_NODE_LEN, "%s_%d",
				QCN6122_M3_DUMP_PREFIX, plat_priv->userpd_id);
		dev_node = of_find_node_by_name(NULL, buf);
	} else if (plat_priv->device_id == QCN9160_DEVICE_ID) {
		snprintf(buf, M3_DUMP_NODE_LEN, "%s_%d",
				QCN9160_M3_DUMP_PREFIX, plat_priv->userpd_id);
		dev_node = of_find_node_by_name(NULL, buf);
	} else if (plat_priv->device_id == QCN6432_DEVICE_ID) {
		snprintf(buf, M3_DUMP_NODE_LEN, "%s_%d",
				QCN6432_M3_DUMP_PREFIX, plat_priv->userpd_id);
		dev_node = of_find_node_by_name(NULL, buf);
	} else {
		dev_node = of_find_node_by_name(NULL, "m3_dump");
	}

	return dev_node;
}

static int cnss_read_node_array_size(struct device *dev, char *property_name)
{
	return of_property_count_elems_of_size(dev->of_node, property_name,
						sizeof(u32));
}

static void cnss_free_arr_addr_mem(unsigned int *arr_addr)
{
	if (arr_addr)
		kfree(arr_addr);
}

static int cnss_ahb_alloc_fw_mem(struct cnss_plat_data *plat_priv)
{
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	unsigned int *bdf_location = NULL, *caldb_location = NULL;
	unsigned int reg[4], mem_region_reserved_size;
	u32 caldb_size = 0;
	struct device *dev;
	int i, idx, mode, ret = 0, bdf_arr_size, caldb_arr_size;
	struct device_node *dev_node = NULL;
	struct device_node *mem_region_node = NULL;
	phandle mem_region_phandle;
	struct resource m3_dump;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	const char *mem_phandle_node_name = "memory-region";
#else
	const char *mem_phandle_node_name = "mem-region";
#endif

	dev = &plat_priv->plat_dev->dev;

	idx = 0;
	mode = plat_priv->tgt_mem_cfg_mode;
	if (mode >= MAX_TGT_MEM_MODES)
		CNSS_ASSERT(0);

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		memset(reg, 0, sizeof(reg));
		switch (fw_mem[i].type) {
		case QMI_WLFW_MEM_BDF_V01:
			bdf_arr_size = cnss_read_node_array_size(dev,
							"qcom,bdf-addr");
			if (!bdf_location) {
				bdf_location = kcalloc(bdf_arr_size,
							sizeof(unsigned int),
							GFP_KERNEL);
				if (!bdf_location) {
					cnss_pr_err("Error: Cannot allocate"
						" bdf arr memory\n");
					ret = -ENOMEM;
					goto err_ahb_fw_mem_alloc;
				}
			}
			if (of_property_read_u32_array(dev->of_node,
						"qcom,bdf-addr", bdf_location,
						bdf_arr_size)) {
				cnss_pr_err("Error: No bdf_addr"
						"in device_tree\n");
				ret = -ENOMEM;
				goto err_ahb_fw_mem_alloc;
			}
			fw_mem[idx].pa = *(bdf_location + mode);
			fw_mem[idx].va = NULL;
			fw_mem[idx].size = fw_mem[i].size;
			fw_mem[idx].type = fw_mem[i].type;
			idx++;
			break;
		case QMI_WLFW_MEM_CAL_V01:
			/* Return caldb address as 0 when FW requests for it
			 * when cold boot support is disabled.
			 */
			if (!plat_priv->cold_boot_support) {
				fw_mem[idx].pa = 0;
			} else {
				caldb_arr_size = cnss_read_node_array_size(dev,
							"qcom,caldb-addr");
				if (!caldb_location) {
					caldb_location = kcalloc(caldb_arr_size,
							sizeof(unsigned int),
							GFP_KERNEL);
					if (!caldb_location) {
						cnss_pr_err("Error: Cannot"
						" allocate caldb arr memory\n");
						ret = -ENOMEM;
						goto err_ahb_fw_mem_alloc;
					}
				}
				if (of_property_read_u32_array(dev->of_node,
						"qcom,caldb-addr",
						caldb_location,
						caldb_arr_size)) {
					cnss_pr_err("Error: Couldn't read"
						"caldb_addr from device_tree\n");
					ret = -EINVAL;
					goto err_ahb_fw_mem_alloc;
				}
				if (of_property_read_u32(dev->of_node,
							 "qcom,caldb-size",
							 &caldb_size)) {
					cnss_pr_err("Error: No caldb-size"
								"in dts\n");
					ret = -EINVAL;
					goto err_ahb_fw_mem_alloc;
				}
				if (fw_mem[i].size > caldb_size) {
					cnss_pr_err("Error: Need more memory"
						"for caldb, fw req:0x%x"
						"max:0x%x\n",
						(unsigned int)fw_mem[i].size,
						caldb_size);
					ret = -EINVAL;
					goto err_ahb_fw_mem_alloc;
				}
				fw_mem[idx].pa = *(caldb_location + mode);
			}
			fw_mem[idx].va = ioremap(fw_mem[idx].pa,
						 fw_mem[idx].size);
			fw_mem[idx].size = fw_mem[i].size;
			fw_mem[idx].type = fw_mem[i].type;
			idx++;
			break;
		case QMI_WLFW_MEM_TYPE_DDR_V01:
			if (of_property_read_u32(dev->of_node,
						 mem_phandle_node_name,
						 &mem_region_phandle)) {
				cnss_pr_err("could not get"
						"mem_region_phandle\n");
				ret = -EINVAL;
				goto err_ahb_fw_mem_alloc;
			}

			mem_region_node =
				of_find_node_by_phandle(mem_region_phandle);
			if (!mem_region_node) {
				cnss_pr_err("could not get mem_region_np\n");
				ret = -EINVAL;
				goto err_ahb_fw_mem_alloc;
			}

			if (of_property_read_u32_array(mem_region_node, "reg",
						       reg, ARRAY_SIZE(reg))) {
				cnss_pr_err("Error: %s node is not assigned\n",
					    mem_phandle_node_name);
				ret = -ENOMEM;
				goto err_ahb_fw_mem_alloc;
			}
			of_node_put(mem_region_node);
			mem_region_reserved_size  = reg[3];
			if (fw_mem[i].size > mem_region_reserved_size) {
				cnss_pr_err("Error: Need more memory %x\n",
					    (unsigned int)fw_mem[i].size);
				goto err_ahb_fw_mem_alloc;
			}
			if (fw_mem[i].size < mem_region_reserved_size) {
				cnss_pr_err("WARNING: More memory is reserved."
						"Reserved size 0x%x,"
						"Requested size 0x%x.\n",
						mem_region_reserved_size,
						(unsigned int)fw_mem[i].size);
			}
			fw_mem[idx].pa = reg[1];
			fw_mem[idx].va = NULL;
			fw_mem[idx].size = fw_mem[i].size;
			fw_mem[idx].type = fw_mem[i].type;
			idx++;
			break;
		case QMI_WLFW_AFC_MEM_V01:
		case QMI_WLFW_MEM_M3_V01:
			dev_node = cnss_get_m3dump_dev_node(plat_priv);
			if (!dev_node) {
				cnss_pr_err("%s: Unable to find m3_dump_region",
					    __func__);
				break;
			}
			if (of_address_to_resource(dev_node, 0, &m3_dump)) {
				cnss_pr_err("%s: Unable to get m3_dump_region",
					    __func__);
				break;
			}

			if (!fw_mem[i].size) {
				cnss_pr_err("FW requests size 0");
				break;
			}
			if (fw_mem[i].size > resource_size(&m3_dump)) {
				cnss_pr_err("Error: Need more memory %x\n",
					    (unsigned int)fw_mem[idx].size);
				goto err_ahb_fw_mem_alloc;
			}
			fw_mem[idx].size = fw_mem[i].size;
			fw_mem[idx].type = fw_mem[i].type;
			if (fw_mem[i].type == QMI_WLFW_MEM_M3_V01) {
				fw_mem[idx].pa = m3_dump.start;
				fw_mem[idx].va = ioremap(fw_mem[idx].pa,
						fw_mem[idx].size);
				if (!fw_mem[idx].va)
					cnss_pr_err("WARNING: M3 Dump addr"
							"remap failed\n");
			} else {
				/* For multi-pd dev, QMI_WLFW_AFC_MEM_V01 needs
				 * to be allocated from within the
				 * M3_DUMP_REGION.
				 * This is because they cannot access memory
				 * regions allocated outside FW reserved memory
				 */
				if (plat_priv->device_id != QCN6122_DEVICE_ID &&
				    plat_priv->device_id != QCN9160_DEVICE_ID &&
				    plat_priv->device_id != QCN6432_DEVICE_ID) {
					cnss_pr_err("Invalid AFC mem request"
							"from target");
					ret = -EINVAL;
					goto err_ahb_fw_mem_alloc;
				}

				if (fw_mem[i].size != AFC_MEM_SIZE) {
					cnss_pr_err("Error: less AFC mem req:"
						   "0x%x\n",
						   (unsigned int)fw_mem[i].size);
					goto err_ahb_fw_mem_alloc;
				}
				if (fw_mem[i].va) {
					afc_memset(plat_priv, fw_mem[i].va, 0,
						   fw_mem[i].size);
					idx++;
					break;
				}

				if (plat_priv->device_id == QCN6122_DEVICE_ID ||
				    plat_priv->device_id == QCN9160_DEVICE_ID ||
				    plat_priv->device_id == QCN6432_DEVICE_ID)
					fw_mem[idx].pa = m3_dump.start +
							 AFC_QCN6122_MEM_OFFSET;

				fw_mem[idx].va = ioremap(fw_mem[idx].pa,
							 fw_mem[idx].size);
				if (!fw_mem[i].va) {
					cnss_pr_err("AFC mem allocation"
							"failed\n");
					fw_mem[i].pa = 0;
					ret = -ENOMEM;
					goto err_ahb_fw_mem_alloc;
				}
			}
			idx++;
			break;
		case QMI_WLFW_MLO_GLOBAL_MEM_V01:
			cnss_mlo_mem_alloc(plat_priv, i);
			fw_mem[idx].pa = fw_mem[i].pa;
			fw_mem[idx].va = fw_mem[i].va;
			fw_mem[idx].size = fw_mem[i].size;
			fw_mem[idx].type = fw_mem[i].type;
			idx++;
			break;
		default:
			cnss_pr_err("Ignore mem req type %d\n", fw_mem[i].type);
			break;
		}
	}
	plat_priv->fw_mem_seg_len = idx;

err_ahb_fw_mem_alloc:
	cnss_free_arr_addr_mem(bdf_location);
	cnss_free_arr_addr_mem(caldb_location);
	if (ret)
		CNSS_ASSERT(0);
	return ret;
}

static void cnss_ahb_free_fw_mem(struct cnss_plat_data *plat_priv)
{
	struct cnss_fw_mem *fw_mem = NULL;
	int i;

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL\n", __func__);
		return;
	}

	fw_mem = plat_priv->fw_mem;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].va) {
			cnss_pr_dbg("Freeing FW mem of type %d\n",
				    fw_mem[i].type);
			if (fw_mem[i].type == QMI_WLFW_AFC_MEM_V01) {
				afc_memset(plat_priv, fw_mem[i].va, 0,
					   AFC_MEM_SIZE);
			} else if (fw_mem[i].type ==
				   QMI_WLFW_MLO_GLOBAL_MEM_V01) {
				/* Do not iounmap or reset */
			} else {
				iounmap(fw_mem[i].va);
				fw_mem[i].va = NULL;
				fw_mem[i].size = 0;
			}
		}
	}

	plat_priv->fw_mem_seg_len = 0;
}

static int cnss_ahb_alloc_qdss_mem(struct cnss_plat_data *plat_priv)
{
	struct device_node *dev_node = NULL;
	struct resource q6_etr;
	int ret;

	if (!plat_priv)
		return -ENODEV;

	if (cnss_check_be_target(plat_priv)) {
		if (!plat_priv->qdss_mem.va) {
			cnss_pr_err("QDSS memory is not allocated\n");
			return -ENOMEM;
		}
		plat_priv->qdss_mem.size = SZ_1M;
		plat_priv->qdss_mem.type = QMI_WLFW_MEM_QDSS_V01;
	} else {
		dev_node = cnss_get_etr_dev_node(plat_priv);
		if (!dev_node) {
			cnss_pr_err("No q6_etr_dump available in dts");
			return -ENOMEM;
		}

		ret = of_address_to_resource(dev_node, 0, &q6_etr);
		if (ret) {
			cnss_pr_err("Failed to get resource for q6_etr_dump");
			return -EINVAL;
		}

		plat_priv->qdss_mem.va = NULL;
		plat_priv->qdss_mem.pa = q6_etr.start;
		plat_priv->qdss_mem.size = resource_size(&q6_etr);
		plat_priv->qdss_mem.type = QMI_WLFW_MEM_QDSS_V01;
		if (plat_priv->device_id == QCN6122_DEVICE_ID ||
		    plat_priv->device_id == QCN9160_DEVICE_ID) {
			plat_priv->qdss_mem.va =
				ioremap(plat_priv->qdss_mem.pa,
					plat_priv->qdss_mem.size);
			if (!plat_priv->qdss_mem.va) {
				cnss_pr_err("WARNING etr-addr remap failed\n");
				return -ENOMEM;
			}
		}
	}

	cnss_pr_dbg("QDSS mem addr pa 0x%x va 0x%p, size 0x%x",
		    (unsigned int)plat_priv->qdss_mem.pa,
		    plat_priv->qdss_mem.va,
			    (unsigned int)plat_priv->qdss_mem.size);

	return 0;
}

static void cnss_ahb_free_qdss_mem(struct cnss_plat_data *plat_priv)
{
	struct cnss_fw_mem *qdss_mem;
	struct qdss_stream_data *qdss_stream;

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL\n", __func__);
		return;
	}

	qdss_mem = &plat_priv->qdss_mem;
	qdss_stream = &plat_priv->qdss_stream;

	if (plat_priv->qdss_etr_sg_mode) {
		cnss_etr_sg_tbl_free(
			(uint32_t *)qdss_stream->qdss_vaddr,
			plat_priv,
			DIV_ROUND_UP(qdss_mem->size, PAGE_SIZE));
	} else {
		if (plat_priv->qdss_mem.va) {
			if (cnss_check_be_target(plat_priv)) {
				/* When QDSS is stopped for 11BE chipsets
				 * only memset the memory,
				 * to retain the allocation for consective
				 * qdss start from cli. QDSS memory will be
				 * cleared only when the feature is disabled,
				 * as consecutive memory may not be available
				 * in runtime.
				 */
				cnss_pr_dbg("Clearing the QDSS data\n");
				memset(plat_priv->qdss_mem.va, 0, SZ_1M);
			} else {
				cnss_pr_dbg("Freeing QDSS Memory\n");
				iounmap(qdss_mem->va);
				qdss_mem->va = NULL;
				qdss_mem->size = 0;
			}
		}
	}
}

static int cnss_ahb_update_status(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status)
{
	struct cnss_wlan_driver *driver_ops;

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL", __func__);
		return -ENODEV;
	}

	driver_ops = plat_priv->driver_ops;
	if (!driver_ops || !driver_ops->update_status) {
		cnss_pr_err("%s: driver_ops is NULL", __func__);
		return -EINVAL;
	}

	cnss_pr_dbg("Update driver status: %d\n", status);

	if (status == CNSS_FW_DOWN)
		driver_ops->fatal((struct pci_dev *)plat_priv->plat_dev,
				  (const struct pci_device_id *)
				  plat_priv->plat_dev_id);

	return 0;
}

static
void cnss_pci_remote_select_window(struct cnss_plat_data *plat_priv, u32 addr)
{
	u32 window = (addr >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;
	u32 prev_window = 0, curr_window = 0, prev_cleared_window = 0;
	volatile u32 write_val, read_val = 0;
	void *bar = plat_priv->tgt_data.bar_addr_va;
	int retry = 0;

	prev_window = readl_relaxed(bar + QCN9000_PCIE_REMAP_BAR_CTRL_OFFSET);

	/* Clear out last 6 bits of window register */
	prev_cleared_window = prev_window & ~(0x3f);

	/* Write the new last 6 bits of window register. Only window 1 values
	 * are changed. Window 2 and 3 are unaffected.
	 */
	curr_window = prev_cleared_window | window;

	/* Skip writing into window register if the read value
	 * is same as calculated value.
	 */
	if (curr_window == prev_window)
		return;

	write_val = WINDOW_ENABLE_BIT | curr_window;
	writel_relaxed(write_val, bar + QCN9000_PCIE_REMAP_BAR_CTRL_OFFSET);

	read_val = readl_relaxed(bar + QCN9000_PCIE_REMAP_BAR_CTRL_OFFSET);

	/* If value written is not yet reflected, wait till it is reflected */
	while ((read_val != write_val) && (retry < 10)) {
		mdelay(1);
		read_val = readl_relaxed(bar +
					 QCN9000_PCIE_REMAP_BAR_CTRL_OFFSET);
		retry++;
	}
	cnss_pr_dbg("%s: retry count: %d", __func__, retry);
}


static int cnss_pci_remote_reg_read(struct cnss_plat_data *plat_priv,
			     u32 addr, u32 *val)
{
	unsigned long flags;
	void *bar = plat_priv->tgt_data.bar_addr_va;

	if (addr < MAX_UNWINDOWED_ADDRESS) {
		*val = readl_relaxed(bar + addr);
		return 0;
	}

	spin_lock_irqsave(&pci_remote_reg_window_lock, flags);
	cnss_pci_remote_select_window(plat_priv, addr);

	*val = readl_relaxed(bar + WINDOW_START +
			     (addr & WINDOW_RANGE_MASK));
	spin_unlock_irqrestore(&pci_remote_reg_window_lock, flags);

	return 0;
}

static int cnss_pci_remote_reg_write(struct cnss_plat_data *plat_priv, u32 addr,
			      u32 val)
{
	unsigned long flags;
	void *bar = plat_priv->tgt_data.bar_addr_va;

	if (addr < MAX_UNWINDOWED_ADDRESS) {
		writel_relaxed(val, bar + addr);
		return 0;
	}

	spin_lock_irqsave(&pci_remote_reg_window_lock, flags);
	cnss_pci_remote_select_window(plat_priv, addr);

	writel_relaxed(val, bar + WINDOW_START +
		       (addr & WINDOW_RANGE_MASK));
	spin_unlock_irqrestore(&pci_remote_reg_window_lock, flags);

	return 0;
}

static int cnss_ahb_reg_read(struct device *dev, u32 addr, u32 *val,
					void __iomem *base)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("Plat Priv is null\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCN6432_DEVICE_ID) {
		if (base) {
			*val = readl_relaxed(base);
			return 0;
		}
		return cnss_pci_remote_reg_read(plat_priv, addr, val);
	}

	if (base)
		*val = readl_relaxed(addr + base);
	else
		cnss_pr_err("Base addr is NULL\n");

	return 0;
}

static int cnss_ahb_reg_write(struct device *dev, u32 addr, u32 val,
					void __iomem *base)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("Plat Priv is null\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCN6432_DEVICE_ID) {
		if (base) {
			writel_relaxed(val, base);
			return 0;
		}
		return cnss_pci_remote_reg_write(plat_priv, addr, val);
	}

	writel_relaxed(val, addr + base);
	return 0;
}

static u64 cnss_ahb_get_q6_time(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("Plat Priv is null\n");
		return 0;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_dbg("Invalid state to get the Q6 timestamp: 0x%lx\n",
			    plat_priv->driver_state);
		return 0;
	}

	return cnss_get_host_timestamp(plat_priv);
}

static
int cnss_ahb_get_soc_info(struct device *dev, struct cnss_soc_info *info)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -ENODEV;

	if (plat_priv->device_id == QCN6122_DEVICE_ID ||
	    plat_priv->device_id == QCN9160_DEVICE_ID ||
	    plat_priv->device_id == QCN6432_DEVICE_ID) {
		info->va = plat_priv->tgt_data.bar_addr_va;
		info->pa = (phys_addr_t)plat_priv->tgt_data.bar_addr_pa;
	}

	memcpy(&info->device_version, &plat_priv->device_version,
	       sizeof(info->device_version));

	memcpy(&info->dev_mem_info, &plat_priv->dev_mem_info,
	       sizeof(info->dev_mem_info));

	return 0;
}

static int cnss_ahb_get_msi_irq(struct device *dev, unsigned int vector)
{
	int irq_num = 0;
	struct pci_dev *pci_dev = NULL;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
#ifdef CONFIG_CNSS2_QGIC2M
	struct qgic2_msi *qgic2_msi = NULL;
	struct cnss_msi_config *msi_config;
#endif

	if (!plat_priv) {
		pr_err("plat_priv NULL");
		return -ENODEV;
	}

	if (plat_priv->device_id != QCN6122_DEVICE_ID &&
	    plat_priv->device_id != QCN9160_DEVICE_ID &&
	    plat_priv->device_id != QCN6432_DEVICE_ID) {
		pci_dev = to_pci_dev(dev);
		irq_num = pci_irq_vector(pci_dev, vector);
		return irq_num;
	}
#ifdef CONFIG_CNSS2_QGIC2M
	qgic2_msi = plat_priv->tgt_data.qgic2_msi;
	if (!qgic2_msi) {
		cnss_pr_err("%s: %s qgic2_msi NULL", __func__,
						     plat_priv->device_name);
		return -EINVAL;
	}

	msi_config = cnss_get_msi_config(plat_priv);
	if (!msi_config) {
		cnss_pr_err("%s msi_config NULL", plat_priv->device_name);
		return -EINVAL;
	}

	if (vector > msi_config->total_vectors) {
		cnss_pr_err("%s: vector greater than max total vectors %d",
				__func__, msi_config->total_vectors);
		return -EINVAL;
	}

	irq_num = qgic2_msi->irq_num + vector;
#endif
	return irq_num;
}

static void cnss_ahb_get_msi_address(struct device *dev, u32 *msi_addr_low,
			  u32 *msi_addr_high)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
#ifdef CONFIG_CNSS2_QGIC2M
	struct qgic2_msi *qgic2_msi = NULL;
#endif

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL", __func__);
		return;
	}

	if (plat_priv->device_id == QCN6122_DEVICE_ID ||
	    plat_priv->device_id == QCN9160_DEVICE_ID ||
	    plat_priv->device_id == QCN6432_DEVICE_ID) {
#ifdef CONFIG_CNSS2_QGIC2M
		qgic2_msi = plat_priv->tgt_data.qgic2_msi;
		if (!qgic2_msi) {
			cnss_pr_err("%s: qgic2_msi NULL", __func__);
			return;
		}
		*msi_addr_low = qgic2_msi->msi_gicm_addr_lo;
		*msi_addr_high = qgic2_msi->msi_gicm_addr_hi;
#endif
	}

	/* Since q6 supports only 32 bit addresses, mask the msi_addr_high
	 * value. If this is programmed into the register, q6 interprets it
	 * as an internal address and causes unwanted writes/reads.
	 */
	*msi_addr_high = 0;
}

static int cnss_ahb_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	int idx;
	u32 msi_ep_base_data = 0;
	struct cnss_msi_config *msi_config = NULL;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
#ifdef CONFIG_CNSS2_QGIC2M
	struct qgic2_msi *qgic2_msi = NULL;
#endif

	if (!plat_priv)
		return -ENODEV;

	if (plat_priv->device_id != QCN9000_DEVICE_ID &&
	    plat_priv->device_id != QCN9224_DEVICE_ID &&
	    plat_priv->device_id != QCN6122_DEVICE_ID &&
	    plat_priv->device_id != QCN9160_DEVICE_ID &&
	    plat_priv->device_id != QCN6432_DEVICE_ID) {
		cnss_pr_dbg("MSI not supported on device 0x%lx",
			    plat_priv->device_id);
		return -EINVAL;
	}

	if (plat_priv->device_id == QCN6122_DEVICE_ID ||
	    plat_priv->device_id == QCN9160_DEVICE_ID ||
	    plat_priv->device_id == QCN6432_DEVICE_ID) {
#ifdef CONFIG_CNSS2_QGIC2M
		msi_config = cnss_get_msi_config(plat_priv);

		qgic2_msi = plat_priv->tgt_data.qgic2_msi;

		if (!qgic2_msi) {
			cnss_pr_err("qgic2_msi NULL");
			return -EINVAL;
		}
		msi_ep_base_data = qgic2_msi->msi_gicm_base_data;
#endif
	}

	if (!msi_config) {
		cnss_pr_err("msi_config NULL");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector +
					  msi_ep_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			cnss_pr_dbg("Assign MSI to user: %s, num_vectors: %d,"
				    "user_base_data: %u, base_vector: %u\n",
				    user_name, *num_vectors, *user_base_data,
				    *base_vector);

			return 0;
		}
	}

	cnss_pr_err("Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}

static struct cnss_bus_ops ahb_ops = {
	.cnss_bus_alloc_fw_mem = cnss_ahb_alloc_fw_mem,
	.cnss_bus_free_fw_mem = cnss_ahb_free_fw_mem,
	.cnss_bus_alloc_qdss_mem = cnss_ahb_alloc_qdss_mem,
	.cnss_bus_free_qdss_mem = cnss_ahb_free_qdss_mem,
	.cnss_bus_update_status = cnss_ahb_update_status,
	.cnss_bus_reg_read = cnss_ahb_reg_read,
	.cnss_bus_reg_write = cnss_ahb_reg_write,
	.cnss_bus_get_soc_info = cnss_ahb_get_soc_info,
	.cnss_bus_get_q6_time = cnss_ahb_get_q6_time,
	.cnss_bus_get_msi_irq = cnss_ahb_get_msi_irq,
	.cnss_bus_get_msi_address = cnss_ahb_get_msi_address,
	.cnss_bus_get_user_msi_assignment = cnss_ahb_get_user_msi_assignment,
};

struct cnss_bus_ops *cnss_ahb_get_ops(void)
{
	return &ahb_ops;
}
