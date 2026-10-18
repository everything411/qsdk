// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include <linux/namei.h>

#include "athdbg_qmi.h"
#include "../athdbg_if.h"
#include "athdbg_core.h"
#include "../pci.h"
#include "../qmi.h"
#ifdef CONFIG_UPSTREAM_BUILD
#include <linux/devcoredump.h>
#include <linux/vmalloc.h>
#endif

#include <linux/vmalloc.h>

extern struct ath_debug_base *athdbg_base;

struct athdbg_qdss_trace_mem_seg {
	u64 addr;
	u32 size;
};

struct athdbg_qmi_event_qdss_trace_save_data {
	u32 total_size;
	u32 mem_seg_len;
	struct athdbg_qdss_trace_mem_seg mem_seg[QDSS_TRACE_SEG_LEN_MAX];
};

int athdbg_qmi_alloc_qdss_mem(struct athdbg_qmi *dbg_qmi)
{
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
#if defined(CONFIG_UPSTREAM_BUILD) || defined(ATH12K_CMA_SUPPORT)
	struct target_mem_chunk *chunk;
#endif
#if !defined(CONFIG_UPSTREAM_BUILD)
	struct reserved_mem *ddr_rmem = NULL;
	const struct athdbg_to_ath12k_ops *dbg_to_ath_ops = athdbg_base->dbg_to_ath_ops;
	bool do_ioremap = false;
#endif

	if (ab->dbg_qmi.qdss_mem_seg_len > 1) {
		pr_err("%s: FW requests %d segments, max allowed is 1\n",
			    __func__, ab->dbg_qmi.qdss_mem_seg_len);
		return -EINVAL;
	}

#if !defined(CONFIG_UPSTREAM_BUILD)
	if (!ab->qmi_mem_dev.rmem_inited) {
		if (dbg_to_ath_ops && dbg_to_ath_ops->get_reserved_mem_by_name)
			ddr_rmem = dbg_to_ath_ops->get_reserved_mem_by_name(ab,
									"host-ddr-mem");

		if (!ddr_rmem) {
			pr_err("host-ddr-mem not available in dts\n");
			return -ENODEV;
		}
	}
#endif

	switch (ab->dbg_qmi.qdss_mem[0].type) {
	case QDSS_ETR_MEM_REGION_TYPE:
#if defined(CONFIG_UPSTREAM_BUILD)
		chunk = &ab->dbg_qmi.qdss_mem[0];
		chunk->v.ioaddr = dma_alloc_coherent(ab->dev,
						     chunk->size,
						     &chunk->paddr,
						     GFP_KERNEL | __GFP_NOWARN);
		if (!chunk->v.ioaddr) {
			pr_err("Unable to allocate QDSS memory\n");
			return -ENOMEM;
		}
#elif defined(ATH12K_CMA_SUPPORT)
		if (ab->qmi_mem_dev.rmem_inited) {
			chunk = &ab->dbg_qmi.qdss_mem[0];
			chunk->v.ioaddr = dma_alloc_coherent(&ab->qmi_mem_dev.dev,
							     chunk->size,
							     &chunk->paddr,
							     GFP_KERNEL | __GFP_NOWARN);
			if (!chunk->v.ioaddr) {
				pr_err("Unable to allocate QDSS memory\n");
				return -ENOMEM;
			}
		} else {
			do_ioremap = true;
		}
#else
#ifdef PLATFORM_SDX
		if (ab->dbg_qmi.qdss_mem[0].size > QMI_Q6_QDSS_ETR_SIZE ||
		    ab->dbg_qmi.qdss_mem[0].size >
		    ddr_rmem->size - ab->host_ddr_fixed_mem_off) {
			pr_err("%s: FW requests more memory 0x%x\n",
				    __func__, ab->dbg_qmi.qdss_mem[0].size);
			return -ENOMEM;
		}
		ab->dbg_qmi.qdss_mem[0].v.ioaddr =
			dma_alloc_attrs(ab->dev,
					ab->dbg_qmi.qdss_mem[0].size,
					&ab->dbg_qmi.qdss_mem[0].paddr,
					GFP_KERNEL,
					DMA_ATTR_FORCE_CONTIGUOUS);
		if (!ab->dbg_qmi.qdss_mem[0].v.ioaddr) {
			pr_err("WARNING: DMA allocation failed for QDSS ETR\n");
			return -ENOMEM;
		}
#else
		do_ioremap = true;
#endif
#endif

#if !defined(CONFIG_UPSTREAM_BUILD)
		if (do_ioremap) {
			if (ab->dbg_qmi.qdss_mem[0].size > QMI_Q6_QDSS_ETR_SIZE ||
			    ab->dbg_qmi.qdss_mem[0].size >
			    ddr_rmem->size - ab->host_ddr_fixed_mem_off) {
				pr_err("%s: FW requests more memory 0x%x\n",
				       __func__, ab->dbg_qmi.qdss_mem[0].size);
				return -ENOMEM;
			}
			ab->dbg_qmi.qdss_mem[0].paddr =
				ddr_rmem->base + ab->host_ddr_fixed_mem_off;
			ab->dbg_qmi.qdss_mem[0].v.ioaddr =
				ioremap(ab->dbg_qmi.qdss_mem[0].paddr,
					ab->dbg_qmi.qdss_mem[0].size);
			if (!ab->dbg_qmi.qdss_mem[0].v.ioaddr) {
				pr_err("WARNING etr-addr remap failed\n");
				return -ENOMEM;
			}
		}
#endif
		break;
	default:
		pr_err("qmi ignore invalid qdss mem req type %d\n",
			    ab->dbg_qmi.qdss_mem[0].type);
		return -EINVAL;
	}

	return 0;
}

int athdbg_qmi_qdss_mem_alloc(struct athdbg_qmi *dbg_qmi)
{
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
	int ret = 0;

	switch (ab->hif.bus) {
	case ATH12K_BUS_AHB:
#ifndef CONFIG_UPSTREAM_BUILD
	case ATH12K_BUS_HYBRID:
#endif
	case ATH12K_BUS_PCI:
		ret = athdbg_qmi_alloc_qdss_mem(dbg_qmi);
		break;
	default:
		pr_err("invalid bus type: %d", ab->hif.bus);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void athdbg_qmi_event_qdss_trace_req_mem_hdlr(struct athdbg_qmi *dbg_qmi)
{
	int ret;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);

	ret = athdbg_qmi_qdss_mem_alloc(dbg_qmi);
	if (ret < 0) {
		pr_err("failed to allocate memory for qdss:%d\n", ret);
		return;
	}

	ret = athdbg_qmi_qdss_trace_mem_info_send_sync(ab);
	if (ret < 0) {
		pr_err("qdss trace mem info send sync failed:%d\n", ret);
		athdbg_qmi_qdss_mem_free(ab);
		return;
	}
	/* After qdss_trace_mem_info(QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01),
	 * the firmware will take one second at max
	 * for its configuration. We shouldn't send qdss_trace request
	 * before that.
	 */
	msleep(1000);
	ret = athdbg_send_qdss_trace_mode_req(ab, QMI_WLANFW_QDSS_TRACE_ON_V01, 0);
	if (ret < 0) {
		pr_err("Failed to enable QDSS trace: %d\n", ret);
		athdbg_qmi_qdss_mem_free(ab);
		return;
	}
	ab->is_qdss_tracing = true;
	pr_info("QDSS configuration is completed and trace started\n");
}

static bool athdbg_qmi_fw_exists(const char *filename)
{
	char fw_path[ATH12K_QMI_MAX_QDSS_CONFIG_FILE_NAME_SIZE];
	struct path path;
	int ret;

	snprintf(fw_path, sizeof(fw_path), "/lib/firmware/%s", filename);
	ret = kern_path(fw_path, LOOKUP_FOLLOW, &path);
	if (ret)
		return false;

	path_put(&path);
	return true;
}

static int athdbg_qmi_send_qdss_config(struct ath12k_base *ab)
{
	char filename[ATH12K_QMI_MAX_QDSS_CONFIG_FILE_NAME_SIZE];
	const struct firmware *fw_entry = NULL;
	struct device *dev = ab->dev;
	struct ath12k_pci *ab_pci;
	const char *fw_dir = NULL;
	char chip_name[32];
	int ret = -ENOENT;
	char *sep = NULL;
	u32 pci_id;

	filename[0] = '\0';
	if (ab->hif.bus == ATH12K_BUS_PCI) {
		ab_pci = ath12k_pci_priv(ab);
		fw_dir = ab->hw_params->fw.dir;

		strscpy(chip_name, fw_dir, sizeof(chip_name));
		sep = strchr(chip_name, '/');
		if (sep)
			*sep = '\0';

		if (ab_pci && ab_pci->pdev && chip_name[0]) {
			pci_id = pci_domain_nr(ab_pci->pdev->bus);
			snprintf(filename, sizeof(filename),
				 "%s/%s/qdss_trace_config_%s_PCI%u.bin",
				 ATH12K_FW_DIR, ab->hw_params->fw.dir,
				 chip_name, pci_id);
		}
		if (filename[0] == '\0' || !athdbg_qmi_fw_exists(filename)) {
			snprintf(filename, sizeof(filename), "%s/%s/%s",
				 ATH12K_FW_DIR, ab->hw_params->fw.dir,
				 ATH12K_QMI_DEFAULT_QDSS_CONFIG_FILE_NAME);
		}
	} else {
		snprintf(filename, sizeof(filename), "%s/%s/%s",
			 ATH12K_FW_DIR, ab->hw_params->fw.dir,
			 ATH12K_QMI_DEFAULT_QDSS_CONFIG_FILE_NAME);
	}

	ret = request_firmware(&fw_entry, filename, dev);
	if (ret) {
		/* for backward compatibility */
		snprintf(filename, sizeof(filename), "%s",
				 ATH12K_QMI_DEFAULT_QDSS_CONFIG_FILE_NAME);
		ret = request_firmware(&fw_entry, filename, dev);
		if (ret) {
			pr_err("qmi failed to load QDSS config: %s\n", filename);
			return ret;
		}
	}

	pr_info("boot firmware request %s size %zu\n", filename, fw_entry->size);
	ret = athdbg_qmi_send_qdss_trace_config_download_req((void *)ab,
								fw_entry->data,
								fw_entry->size);
	if (ret < 0)
		pr_err("qmi failed to load QDSS config to FW: %d\n", ret);

	release_firmware(fw_entry);
	return ret;
}

int athdbg_config_qdss(struct ath12k_base *ab)
{
	int ret;

	ret = athdbg_qmi_worker_init(ab);
	if (ret < 0) {
		pr_err("QDSS config:Failed to init dbg qmi worker");
		return ret;
	}

	ret = athdbg_qmi_send_qdss_config(ab);
	if (ret < 0)
		pr_err("Failed to download QDSS config to FW: %d\n",
			    ret);
	return ret;
}
EXPORT_SYMBOL(athdbg_config_qdss);

void athdbg_wlfw_qdss_trace_req_mem_ind_cb(struct qmi_handle *qmi_hdl,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn,
					const void *data)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;
	const struct qmi_wlanfw_request_mem_ind_msg_v01 *msg = data;
	int i;

	pr_info("qdss trace request memory from firmware\n");
	ab->dbg_qmi.qdss_mem_seg_len = msg->mem_seg_len;

	if (msg->mem_seg_len > 1) {
		pr_err("%s: FW requests %d segments, overwriting it with 1",
				__func__, msg->mem_seg_len);
		ab->dbg_qmi.qdss_mem_seg_len = 1;
	}

	for (i = 0; i < ab->dbg_qmi.qdss_mem_seg_len; i++) {
		ab->dbg_qmi.qdss_mem[i].type = msg->mem_seg[i].type;
		ab->dbg_qmi.qdss_mem[i].size = msg->mem_seg[i].size;
		pr_info("qmi mem seg type %d size %d\n",
				msg->mem_seg[i].type, msg->mem_seg[i].size);
	}
	athdbg_qmi_driver_event_post(qmi, ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_MEM, NULL);
}

void athdbg_wlfw_qdss_trace_save_ind_cb(struct qmi_handle *qmi_hdl,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn,
					const void *data)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi,
						handle);
	const struct qmi_wlanfw_qdss_trace_save_ind_msg_v01 *ind_msg = data;
	struct ath12k_qmi_event_qdss_trace_save_data *event_data;
	int i;

	pr_info("Received qdss trace save indication\n");
	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (!event_data)
		return;

	if (ind_msg->mem_seg_valid) {
		if (ind_msg->mem_seg_len > QDSS_TRACE_SEG_LEN_MAX) {
			pr_err("Invalid seg len %u\n", ind_msg->mem_seg_len);
			goto free_event_data;
		}

		event_data->mem_seg_len = ind_msg->mem_seg_len;
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			event_data->mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			event_data->mem_seg[i].size = ind_msg->mem_seg[i].size;
		}
	}

	pr_info("source %d, total size requested %u\n", ind_msg->source,
			ind_msg->total_size);
	event_data->total_size = ind_msg->total_size;
	if (ind_msg->source == 1) {
		athdbg_qmi_driver_event_post(qmi, ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_DATA,
				event_data);
		return;
	}
	athdbg_qmi_driver_event_post(qmi, ATHDBG_QMI_EVENT_QDSS_TRACE_SAVE,
				     event_data);
	return;

free_event_data:
	kfree(event_data);
}

void athdbg_coredump_qdss_dump(struct ath12k_base *ab,
			struct athdbg_qmi_event_qdss_trace_save_data *event_data)
{
	struct ath12k_dump_segment *segment;
	int len, num_seg;
	void *dump = NULL;

	if (ab->is_bypassed)
		return;

	num_seg = event_data->mem_seg_len;
	len = sizeof(*segment);
	segment = vzalloc(len);
	if (!segment) {
		pr_warn("fail to alloc memory for qdss\n");
		return;
	}

	if (event_data->total_size &&
		event_data->total_size <= ab->dbg_qmi.qdss_mem[0].size){
		dump = vzalloc(event_data->total_size);
		if (!dump) {
			vfree(segment);
			return;
		}
	}

	if (num_seg == 1) {
		segment->len = event_data->mem_seg[0].size;
		segment->vaddr = ab->dbg_qmi.qdss_mem[0].v.ioaddr;
		pr_err("seg vaddr is 0x%p len is 0x%x\n", segment->vaddr, segment->len);
	} else if (num_seg == 2) {
		/*FW sends 2 segments with segment 0 and segment 1 */
		if (event_data->mem_seg[1].addr != ab->dbg_qmi.qdss_mem[0].paddr) {
			pr_warn("Invalid seg 0 addr 0x%llx\n",
					event_data->mem_seg[1].addr);
			goto out;
		}
		if (event_data->mem_seg[0].size + event_data->mem_seg[1].size !=
		    ab->dbg_qmi.qdss_mem[0].size) {
			pr_warn("Invalid total size 0x%x 0x%x\n",
					event_data->mem_seg[0].size,
					event_data->mem_seg[1].size);
			goto out;
		}

		pr_err("qdss mem seg0 addr 0x%llx size 0x%x\n",
			   event_data->mem_seg[0].addr, event_data->mem_seg[0].size);
		pr_err("qdss mem seg1 addr 0x%llx size 0x%x\n",
			   event_data->mem_seg[1].addr, event_data->mem_seg[1].size);

		memcpy(dump,
		       ab->dbg_qmi.qdss_mem[0].v.ioaddr + event_data->mem_seg[1].size,
		       event_data->mem_seg[0].size);
		memcpy(dump + event_data->mem_seg[0].size,
		       ab->dbg_qmi.qdss_mem[0].v.ioaddr, event_data->mem_seg[1].size);

		segment->len = event_data->mem_seg[0].size +
			event_data->mem_seg[1].size;
		segment->vaddr = dump;
		pr_err("seg vaddr is 0x%p and len is 0x%x\n", segment->vaddr,
				segment->len);
	}

#ifndef CONFIG_UPSTREAM_BUILD
	segment->type = FW_CRASH_DUMP_QDSS_DATA;
	athdbg_base->dbg_to_ath_ops->coredump_dump_segment(ab, segment, segment->len);
#else
	/* dev_coredumpv() takes ownership of the buffer */
	dev_coredumpv(ab->dev, segment->vaddr, segment->len, GFP_KERNEL);
	/* Only free segment structure, NOT the dump buffer */
	vfree(segment);
	return;
#endif

out:
	vfree(segment);
	vfree(dump);
}
