/* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/devcoredump.h>
#include <linux/platform_device.h>
#include <linux/dma-direction.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include "main.h"
#include "coredump.h"
#if defined CNSS_PCI_SUPPORT
#include "pci/pci.h"
#endif
#include "cnss_common/cnss_common.h"

static ssize_t cnss_coredump_read_q6dump(char *buffer, loff_t offset,
					 size_t count, void *data,
					 size_t datalen)
{
	struct cnss_dump_segment *segments = data;
	int ret = 0;

	ret = memory_read_from_buffer(buffer, count, &offset,
				      segments->vaddr, datalen);
	if (!ret)
		pr_info("Ramdump complete, %lld  bytes read\n", offset);

	return ret;
}

static void cnss_coredump_free_q6dump(void *data)
{
	struct cnss_dump_segment *segments = data;

	complete(&segments->dump_done);
}

void cnss_coredump_build_inline(struct cnss_plat_data *plat_priv,
				struct cnss_dump_segment *segments,
				size_t datalen)
{
	struct device *dev = &plat_priv->plat_dev->dev;

	init_completion(&segments->dump_done);

	dev_coredumpm(dev, THIS_MODULE, segments, datalen, GFP_KERNEL,
		      cnss_coredump_read_q6dump, cnss_coredump_free_q6dump);

	/* Wait until the dump is read and free is called */
	wait_for_completion(&segments->dump_done);
}

void cnss_coredump_qdss_dump(struct cnss_plat_data *plat_priv,
			     struct cnss_qmi_event_qdss_trace_save_data *event_data)
{
	struct cnss_fw_mem qdss_mem = plat_priv->qdss_mem;
	struct cnss_dump_segment *segment;
	int num_seg;
	void *dump = NULL;

	num_seg = event_data->mem_seg_len;
	segment = kzalloc(sizeof(*segment), GFP_KERNEL);
	if (!segment) {
		cnss_pr_err("fail to alloc memory for qdss\n");
		return;
	}

	if (event_data->total_size &&
	    event_data->total_size <= qdss_mem.size)
		dump = kzalloc(event_data->total_size, GFP_KERNEL);
	if (!dump) {
		kfree(segment);
		return;
	}

	if (num_seg == 1) {
		segment->len = event_data->mem_seg[0].size;
		segment->vaddr = qdss_mem.va;
		cnss_pr_dbg("seg vaddr is 0x%p len is 0x%x\n",
			    segment->vaddr, segment->len);
		segment->type = CNSS_FW_QDSS_DATA;
	} else if (num_seg == 2) {
		/*FW sends 2 segments with segment 0 and segment 1 */

		if (event_data->mem_seg[1].addr != qdss_mem.pa) {
			cnss_pr_err("Invalid seg 0 addr 0x%llx\n",
			    event_data->mem_seg[1].addr);
			goto out;
		}
		if (event_data->mem_seg[0].size + event_data->mem_seg[1].size !=
		    qdss_mem.size) {
			cnss_pr_err("Invalid total size 0x%x 0x%x\n",
				    event_data->mem_seg[0].size,
				    event_data->mem_seg[1].size);
			goto out;
		}

		cnss_pr_dbg("qdss mem seg0 addr 0x%llx size 0x%x\n",
			   event_data->mem_seg[0].addr,
			   event_data->mem_seg[0].size);
		cnss_pr_dbg("qdss mem seg1 addr 0x%llx size 0x%x\n",
			   event_data->mem_seg[1].addr,
			   event_data->mem_seg[1].size);

		memcpy(dump,
		       qdss_mem.va + event_data->mem_seg[1].size,
		       event_data->mem_seg[0].size);
		memcpy(dump + event_data->mem_seg[0].size,
		       qdss_mem.va, event_data->mem_seg[1].size);

		segment->len = event_data->mem_seg[0].size +
					event_data->mem_seg[1].size;
		segment->vaddr = dump;
		cnss_pr_dbg("seg vaddr is 0x%p and len is 0x%x\n",
			   segment->vaddr, segment->len);
		segment->type = CNSS_FW_QDSS_DATA;
	}

	cnss_coredump_build_inline(plat_priv, segment, segment->len);
out:
	kfree(segment);
	kfree(dump);
}

void cnss_coredump_m3_dump(struct cnss_plat_data *plat_priv,
			   struct cnss_qmi_event_m3_dump_upload_req_data *event_data)
{
	struct cnss_fw_mem *target_mem = plat_priv->fw_mem;
	struct cnss_dump_segment *segment;
	int i, ret = 0;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (target_mem[i].pa == event_data->addr &&
		    event_data->size <= target_mem[i].size)
			break;
	}

	if (i == plat_priv->fw_mem_seg_len) {
		cnss_pr_err("qmi invalid paddr from firmware for M3 dump\n");
		ret = -EINVAL;
		goto send_resp;
	}

	segment = kzalloc(sizeof(*segment), GFP_KERNEL);
	if (!segment) {
		cnss_pr_err("fail to alloc memory for m3\n");
		ret = -EINVAL;
		goto send_resp;
	}
	segment->len = event_data->size;
	segment->vaddr = target_mem[i].va;
	segment->type = CNSS_FW_M3_DUMP;

	cnss_coredump_build_inline(plat_priv, segment, segment->len);

send_resp:
	ret = cnss_wlfw_m3_dump_upload_done_send_sync(plat_priv,
						event_data->pdev_id,
						ret);
       if (ret < 0)
		cnss_pr_err("qmi M3 dump upload done failed\n");
	kfree(segment);
}

void cnss_coredump_dump_ddr_region(struct cnss_plat_data *plat_priv,
				   struct cnss_qmi_event_dump_ddr_region *event_data)
{
	struct cnss_dump_segment *segment, *seg_info;
	int num_seg, i;

	num_seg = event_data->mem_seg_len;
	segment = kcalloc(num_seg, sizeof(*segment), GFP_KERNEL);
	if (!segment) {
		cnss_pr_err("Failed to allocate memory for segment for ddr region download\n");
		return;
	}
	seg_info = segment;

	for (i = 0; i < num_seg; i++) {
		if (!event_data->mem_seg[i].pa)
			continue;

		seg_info->len = event_data->mem_seg[i].size;
		seg_info->vaddr = event_data->mem_seg[i].va;
		seg_info->addr = event_data->mem_seg[i].pa;
		seg_info->type = event_data->mem_seg[i].type;
		cnss_pr_dbg("seg vaddr: %p len: 0x%x type: %d\n",
			    seg_info->vaddr, seg_info->len, seg_info->type);
		seg_info++;
	}

	cnss_coredump_build_inline(plat_priv, segment, event_data->total_size);
	kfree(segment);
}
