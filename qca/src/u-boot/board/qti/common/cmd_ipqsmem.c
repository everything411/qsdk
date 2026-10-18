// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

/*
 * Copyright (c) 2013, 2015-2017, 2020 The Linux Foundation. All rights reserved.
 *
 * Based on smem.c from lk.
 *
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <common.h>
#include <command.h>
#include <smem.h>
#include <nand.h>
#include <asm/io.h>
#include <ubi_uboot.h>
#include <linux/sizes.h>
#include <fdtdec.h>

#include "ipq_board.h"

#ifdef CONFIG_CMD_UBI
static void print_ubi_vol_info(struct ubi_device *ubi)
{
	int i;
	int j=0;
	struct ubi_volume *vol;

	for (i = 0; i < (ubi->vtbl_slots + 1); i++) {
		vol = ubi->volumes[i];
		if (!vol)
			continue;	/* Empty record */
		if (vol->name_len <= UBI_VOL_NAME_MAX && strnlen(vol->name,
			vol->name_len + 1) == vol->name_len) {
			printf("\tubi vol %d %s\n", vol->vol_id, vol->name);
			j++;
		} else {
			printf("\tubi vol %d %c%c%c%c%c\n",
				vol->vol_id, vol->name[0], vol->name[1],
				vol->name[2], vol->name[3], vol->name[4]);
			j++;
		}
		if (j == ubi->vol_count - UBI_INT_VOL_COUNT)
			break;
	}
}
#endif

static int do_smeminfo(struct cmd_tbl *cmdtp, int flag, int argc,
			char * const argv[])
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	int i;
	uint32_t bsize;
	struct smem_ptable * ptable = get_ipq_part_table_info();
#if defined(CONFIG_NOR_BLK)
	struct blk_desc *dev;
#endif
#ifdef CONFIG_IPQ_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);
#endif
#ifdef CONFIG_CMD_UBI
	bool print_ubi = false;
	struct ubi_device *ubi = NULL;
	if (init_ubi_part() == 0)
		ubi = ubi_get_device(0);
#endif
	if(sfi->flash_density != 0) {
		printf(	"flash_type:		0x%x\n"
			"flash_index:		0x%x\n"
			"flash_chip_select:	0x%x\n"
			"flash_block_size:	0x%x\n"
			"flash_density:		0x%x\n"
			"partition table offset	0x%x\n",
				sfi->flash_type, sfi->flash_index,
				sfi->flash_chip_select, sfi->flash_block_size,
				sfi->flash_density, sfi->primary_mibib);
	} else {
		printf(	"flash_type:		0x%x\n"
			"flash_index:		0x%x\n"
			"flash_chip_select:	0x%x\n"
			"flash_block_size:	0x%x\n"
			"partition table offset	0x%x\n",
				sfi->flash_type, sfi->flash_index,
				sfi->flash_chip_select, sfi->flash_block_size,
				sfi->primary_mibib);
	}

	if (ptable && ptable->len > 0) {
		printf("%-3s: " smem_ptn_name_fmt " %10s %16s %16s\n",
			"No.", "Name", "Attributes", "Start", "Size");
	} else {
		printf("Partition information not available\n");
	}

	for (i = 0;ptable && i < ptable->len; i++) {
		struct smem_ptn *p = &ptable->parts[i];
		loff_t psize = 0;
		bsize = get_part_block_size(p, sfi);

		if (p->size == (~0u)) {
			/*
			 * Partition size is 'till end of device', calculate
			 * appropriately
			 */
#ifdef CONFIG_IPQ_NAND
			if (mtd)
				psize = mtd->size -
						(((loff_t) p->start) * bsize);
#else
			psize = 0;
#endif
		} else {
			psize = ((loff_t)p->size) * bsize;
		}

		if (is_smem_part_exceed_flash_size(p,
				((((loff_t)p->start) * bsize) + psize)))
			continue;

		printf("%3d: " smem_ptn_name_fmt " 0x%08x %#16llx %#16llx\n",
		       i, p->name, p->attr, ((loff_t)p->start) * bsize, psize);
#ifdef CONFIG_CMD_UBI
		if (!strncmp(p->name,
#ifdef CONFIG_BOOTCONFIG_V2
				ROOT_FS_PART_NAME,
#elif CONFIG_BOOTCONFIG_V3
				(gd->board_type & ACTIVE_BOOT_SET) ?
				ROOT_FS_ATL_PART_NAME :
				ROOT_FS_PART_NAME,
#endif
				SMEM_PTN_NAME_MAX) &&
				ubi) {
			print_ubi_vol_info(ubi);
			print_ubi = true;
		}
#endif
	}
#if defined(CONFIG_NOR_BLK)
	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		printf("\n");
		dev = blk_get_devnum_by_uclass_id(UCLASS_SPI, 0);
		if (dev != NULL && dev->type != DEV_TYPE_UNKNOWN) {
			part_print(dev);
		}
	}
#endif
#ifdef CONFIG_CMD_UBI
	if (ubi) {
		if (!print_ubi) {
			printf("\n Ubi volume list:\n");
			print_ubi_vol_info(ubi);
		}
		ubi_put_device(ubi);
	}
#endif
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	smeminfo,    1,    1,    do_smeminfo,
	"print SMEM FLASH information",
	"\n    - print flash partition layout\n"
);
