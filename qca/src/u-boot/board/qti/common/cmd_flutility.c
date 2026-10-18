/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * FlashWrite command support
 */
#include <common.h>
#include <command.h>
#include <image.h>
#include <part.h>

#ifdef CONFIG_IPQ_MMC
#include <mmc.h>
#include <sdhci.h>
#endif

#ifdef CONFIG_GPT_UPDATE_PARAMS
#include <u-boot/crc.h>
#endif

#ifdef CONFIG_IPQ_NAND
#include <nand.h>
#include <ubi_uboot.h>
#include <linux/mtd/mtd.h>
#endif

#include <fdtdec.h>
#include <elf.h>

#ifdef CONFIG_GPT_UPDATE_PARAMS
#include <memalign.h>
#endif

#include "ipq_board.h"

DECLARE_GLOBAL_DATA_PTR;
#ifdef CONFIG_SDHCI_SUPPORT
extern struct sdhci_host mmc_host;
#endif

#ifndef GPT_PART_NAME
#define GPT_PART_NAME "0:GPT"
#endif
#ifndef GPT_BACKUP_PART_NAME
#define GPT_BACKUP_PART_NAME "0:GPTBACKUP"
#endif
#ifndef NOR_GPT_PART_NAME
#define NOR_GPT_PART_NAME "0:NORGPT"
#endif
#ifndef NOR_GPT_BACKUP_PART_NAME
#define NOR_GPT_BACKUP_PART_NAME "0:NORGPTBACKUP"
#endif

#define HEADER_MAGIC1 0xFE569FAC
#define HEADER_MAGIC2 0xCD7F127A
#define HEADER_VERSION 4

#define SHA1_SIG_LEN 41

#ifdef CONFIG_GPT_UPDATE_PARAMS
enum {
	GPT,
	GPTBACKUP
};
#endif

enum {
	CMD_FLASH = 1,
	CMD_FLERASE,
	CMD_FLREAD,
};

struct fl_info {
	int flash_type;
	uint32_t offset;
	uint32_t address;
	uint32_t file_size;
	uint32_t part_size;
	char* ubi_vol_name;
	bool is_ubi;
} __attribute__ ((__packed__));

#define UPDATE_FL_INFO(_dest, _type, _offset, _address, _part_size,	\
			_file_size,_vol_name, _ubi)		\
			do {						\
				(_dest)->flash_type = _type;		\
				(_dest)->offset = _offset;		\
				(_dest)->address = _address;		\
				(_dest)->file_size = _file_size;	\
				(_dest)->part_size = _part_size;	\
				(_dest)->ubi_vol_name = _vol_name;	\
				(_dest)->is_ubi = _ubi;			\
			} while(0)					\

struct header {
	unsigned magic[2];
	unsigned version;
} __attribute__ ((__packed__));

static int g_flash = 0;
int g_gptsel = -1;

#ifdef CONFIG_CMD_UBI
static void detach_ubi(void)
{
	char runcmd[32];
	struct ubi_device *ubi = NULL;

	ubi = ubi_get_device(0);
	if (ubi) {
		ubi_put_device(ubi);
		snprintf(runcmd, sizeof(runcmd), "ubi detach && ");
		run_command(runcmd, 0);
	}
}
#endif

int isvalid_appsbl_image(uintptr_t load_addr)
{
	uint64_t e_type;
	int ret = 0;
	unsigned long board_type = 0;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	char *flash_name[12] = { "no", "nor", "nand", "onenand", "sdc", "mmc",
				"spi_nor", "norplusnand", "norplusemmc",
				"dummy", "dummy", "qspi_nand" };

	if (((Elf32_Ehdr *)load_addr)->e_ident[EI_CLASS] == ELFCLASS64)
		e_type = ((Elf64_Ehdr *)load_addr)->e_type;
	else
		e_type = ((Elf32_Ehdr *)load_addr)->e_type;

	switch(sfi->flash_type) {
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		board_type = get_current_board_flash_config(sfi->flash_type);
		if (board_type == SMEM_BOOT_NORGPT_FLASH)
			board_type = SMEM_BOOT_SPI_FLASH;
		break;
	default:
		board_type = sfi->flash_type;
	}

	if ((e_type & ET_LOOS) != ET_LOOS) {
		goto skip;
	} else {
		if ((e_type & 0xF) != board_type) {
			printf("### Invalid Image %s != %s\n",
					flash_name[board_type],
					flash_name[e_type & 0xF]);
			ret = CMD_RET_USAGE;
		}
	}
skip:
	return ret;
}

static int write_to_flash(struct fl_info *fl)
{
	int flash_type = fl->flash_type;
	uint32_t address = fl->address;
	uint32_t offset = fl->offset;
	uint32_t part_size = fl->part_size;
	uint32_t file_size = fl->file_size;
	char runcmd[128] = { 0 };

	switch (flash_type) {
	case SMEM_BOOT_QSPI_NAND_FLASH:
		snprintf(runcmd, sizeof(runcmd),
			"nand erase 0x%x 0x%x && "
			"nand write 0x%x 0x%x 0x%x && ",
			offset, part_size,
			address, offset, file_size);
		break;
	case SMEM_BOOT_MMC_FLASH:
		snprintf(runcmd, sizeof(runcmd),
			"mmc erase 0x%x 0x%x && "
			"mmc write 0x%x 0x%x 0x%x && ",
			offset, part_size,
			address, offset, file_size);
		break;
#ifdef CONFIG_NOR_BLK
	case SMEM_BOOT_NORGPT_FLASH:
#ifdef CONFIG_BLOCK_CACHE
		{
		struct blk_desc *dev;

		dev = blk_get_devnum_by_uclass_id(UCLASS_SPI, 0);
		if (!dev)
			return CMD_RET_FAILURE;

		blkcache_invalidate(dev->uclass_id, dev->devnum);
		}
#endif
#endif
	case SMEM_BOOT_SPI_FLASH:
		snprintf(runcmd, sizeof(runcmd),
			"sf probe && "
			"sf erase 0x%x 0x%x && "
			"sf write 0x%x 0x%x 0x%x && ",
			(offset & ~(SZ_64K - 1)), part_size,
			address, offset, file_size);
		break;
	default:
		printf("Invalid flash type \n");
	}

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int fl_erase(struct fl_info *fl)
{
	int flash_type = fl->flash_type;
	uint32_t offset = fl->offset;
	uint32_t part_size = fl->part_size;
	char runcmd[128] = { 0 };
	char *verbose = env_get("verbose");
	int ret = CMD_RET_FAILURE;

	if (verbose)
		env_set("stdout", "serial");
	else
		env_set("stdout", "nulldev");

	switch (flash_type) {
	case SMEM_BOOT_QSPI_NAND_FLASH:
		snprintf(runcmd, sizeof(runcmd), "nand erase 0x%x 0x%x ",
					 offset, part_size);
		break;
	case SMEM_BOOT_MMC_FLASH:
		snprintf(runcmd, sizeof(runcmd), "mmc erase 0x%x 0x%x ",
				 offset, part_size);
		break;
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		snprintf(runcmd, sizeof(runcmd), "sf probe && "
				"sf erase 0x%x 0x%x ", offset, part_size);
		break;
	default:
		printf("Invalid flash type \n");
	}

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		ret = CMD_RET_FAILURE;
	else
		ret = CMD_RET_SUCCESS;

	if(!verbose) {
		env_set("stdout", "serial");
		printf("Eraseing %-30s %s\n", fl->ubi_vol_name,
				ret ? "[ failed ]" : "[ done ]");
	}

	return ret;
}

static int fl_read(struct fl_info *fl)
{
	int flash_type = fl->flash_type;
	uint32_t address = fl->address;
	uint32_t offset = fl->offset;
	uint32_t part_size = fl->part_size;
	char* vol_name = fl->ubi_vol_name;
	bool ubi = fl->is_ubi;
	char runcmd[128] = { 0 };

	switch (flash_type) {
	case SMEM_BOOT_QSPI_NAND_FLASH:
		if (ubi) {
			snprintf(runcmd, sizeof(runcmd),"ubi read 0x%x %s",
				address, vol_name);
		} else {

			snprintf(runcmd, sizeof(runcmd),
					"nand read 0x%x 0x%x 0x%x ",
					 address, offset, part_size);
		}
		break;
	case SMEM_BOOT_MMC_FLASH:
		snprintf(runcmd, sizeof(runcmd), "mmc read 0x%x 0x%x 0x%x ",
				 address, offset, part_size);
		break;
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		snprintf(runcmd, sizeof(runcmd),
				"sf probe && "
				"sf read 0x%x 0x%x 0x%x ",
				 address, offset, part_size);
		break;
	default:
		printf("Invalid flash type \n");
	}

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_CMD_UBI
int ubi_vol_present(char* ubi_vol_name)
{
	int i;
	int j=0;
	struct ubi_device *ubi = NULL;
	struct ubi_volume *vol;
	char runcmd[256];
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	get_kernel_fs_part_details(g_flash? g_flash : sfi->flash_type);

	watchdog_reset();

	if (init_ubi_part())
		goto ubi_detach;

	ubi = ubi_get_device(0);
	for (i = 0; ubi && i < (ubi->vtbl_slots + 1); i++) {
		vol = ubi->volumes[i];
		if (!vol)
			continue;	/* Empty record */
		if (vol->name_len <= UBI_VOL_NAME_MAX &&
		    strnlen(vol->name, vol->name_len + 1) == vol->name_len) {
			j++;
			if (!strncmp(ubi_vol_name, vol->name,
						UBI_VOL_NAME_MAX)) {
				ubi_put_device(ubi);
				return 1;
			}
		}

		if (j == ubi->vol_count - UBI_INT_VOL_COUNT)
			break;
	}


	printf("volume or partition %s not found\n", ubi_vol_name);
ubi_detach:
	if (ubi)
		ubi_put_device(ubi);

	snprintf(runcmd, sizeof(runcmd), "ubi detach && ");
	run_command(runcmd, 0);
	return 0;
}

int write_ubi_vol(char* ubi_vol_name, uint32_t load_addr, uint32_t file_size)
{
	char runcmd[256];
	int ret = CMD_RET_SUCCESS;
	if (!strncmp(ubi_vol_name, "ubi_rootfs", UBI_VOL_NAME_MAX)) {
		snprintf(runcmd, sizeof(runcmd),
			"ubi remove rootfs_data &&"
			"ubi remove %s &&"
			"ubi create %s 0x%x &&"
			"ubi write 0x%x %s 0x%x &&"
			"ubi create rootfs_data",
			 ubi_vol_name, ubi_vol_name, file_size,
			 load_addr, ubi_vol_name, file_size);
	} else {
		snprintf(runcmd, sizeof(runcmd),
			"ubi write 0x%x %s 0x%x ",
			 load_addr, ubi_vol_name, file_size);
	}

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		ret = CMD_RET_FAILURE;

	return ret;
}
#endif /* CONFIG_CMD_UBI */

#if defined(CONFIG_GPT_UPDATE_PARAMS)
static void recalc_primary_gpt_mbr_params(legacy_mbr *mbr,
						struct blk_desc *blk_dev)
{
	lbaint_t val = le64_to_cpu(blk_dev->lba - 1);
	mbr->partition_record[0].nr_sects =  cpu_to_le64(val);
}

static void recalc_gpt_header(uint8_t gpt_sel, gpt_header *gpt_h,
				struct blk_desc *blk_dev)
{
	u32 calc_crc32;
	lbaint_t val;

	switch (gpt_sel) {
	case GPT:
		val = le64_to_cpu(blk_dev->lba - 1);
		gpt_h->alternate_lba = cpu_to_le64(val);
		break;
	case GPTBACKUP:
		val = le64_to_cpu(blk_dev->lba - 1);
		gpt_h->my_lba = cpu_to_le64(val);

		val = le64_to_cpu(blk_dev->lba - 33);
		gpt_h->partition_entry_lba = cpu_to_le64(val);
		break;
	default:
		return;
	};

	val = le64_to_cpu(blk_dev->lba - 1 - 33);
	gpt_h->last_usable_lba = cpu_to_le64(val);

	gpt_h->header_crc32 = 0;

	if (blk_dev->uclass_id == UCLASS_MMC)
		calc_crc32 = crc32(0, (const unsigned char *)gpt_h,
			       le32_to_cpu(gpt_h->header_size));
	else
		calc_crc32 = crc32(0 ^ 0xFFFFFFFF, (const unsigned char *)gpt_h,
			       le32_to_cpu(gpt_h->header_size)) ^ 0xFFFFFFFF;

	gpt_h->header_crc32 = cpu_to_le32(calc_crc32);
}

ulong blk_derase_and_write(struct blk_desc *desc, lbaint_t start,
			   lbaint_t blkcnt, const void *buffer)
{
	int ret = 0;
	if (desc->uclass_id == UCLASS_MMC) {
		uint8_t *buf = NULL;

		buf = (uint8_t*) malloc_cache_aligned(desc->blksz);
		if (!buf) {
			printf("memory allocation failed ...");
			return -ENOMEM;
		}
		memset(buf, 0 , sizeof(desc->blksz));
		ret = blk_dwrite(desc, start, blkcnt, buf);

		free(buf);
		buf = NULL;
		if (ret != blkcnt) {
			printf("block write failed, ret %d", ret);
			return ret;
		}
	} else
		ret = blk_derase(desc, start, blkcnt);

	if (ret == blkcnt) {
		ret = blk_dwrite(desc, start, blkcnt, buffer);
		if (ret != blkcnt) {
			printf("block write failed, ret %d", ret);
			return ret;
		}
	} else
		printf("block erase failed, ret %d", ret);

	return ret;
}

static void gpt_update_params(struct blk_desc *blk_dev,	uint8_t gptsel)
{
	gpt_header gpt_h;
	legacy_mbr mbr;
	uint8_t *buf = NULL;

	buf = (uint8_t*) malloc_cache_aligned(blk_dev->blksz);
	if(!buf) {
		printf("memory allocation failed ...");
		return;
	}

	switch(gptsel) {
	case GPT:
		if (blk_dread(blk_dev, 0, 1, (void *)buf) != 1) {
			printf("gpt mbr read failed ...\n");
			break;
		}

		memcpy(&mbr, buf, sizeof(legacy_mbr));
		recalc_primary_gpt_mbr_params(&mbr, blk_dev);
		memcpy(buf, &mbr, sizeof(legacy_mbr));

		if (blk_derase_and_write(blk_dev, 0, 1, buf) != 1)
			break;

		if (blk_dread(blk_dev, 1, 1, (void *)buf) != 1) {
			printf("gpt header read failed ...\n");
			break;
		}

		memcpy(&gpt_h, buf, sizeof(gpt_header));
		recalc_gpt_header(GPT, &gpt_h, blk_dev);
		memcpy(buf, &gpt_h, sizeof(gpt_header));

		if (blk_derase_and_write(blk_dev, 1, 1, buf) != 1)
			break;
		break;
	case GPTBACKUP:
		if (blk_dread(blk_dev, blk_dev->lba - 1, 1, (void *)buf) != 1) {
			printf("gptbackup header read failed ...\n");
			break;
		}

		memcpy(&gpt_h, buf, sizeof(gpt_header));
		recalc_gpt_header(GPTBACKUP, &gpt_h, blk_dev);
		memcpy(buf, &gpt_h, sizeof(gpt_header));

		if (blk_derase_and_write(blk_dev, blk_dev->lba - 1,
					  1, buf) != 1)
			break;
	};

	if (buf) {
		free(buf);
		buf = NULL;
	}
}
#endif

#ifdef CONFIG_MMC
static int prepare_mmc_flash(char *part_name, uint32_t *offset,
				uint32_t *part_size, uint32_t* file_size)
{
	uint32_t fsize;
	int ret = 0;
	struct disk_partition disk_info;
	struct blk_desc *blk_dev;
	blkpart_info_t  bpart_info;

	blk_dev = blk_get_devnum_by_uclass_id(UCLASS_MMC, 0);
	if (!blk_dev)
		return -ENXIO;

	if (strncmp(GPT_PART_NAME, (const char *)part_name,
			sizeof(GPT_PART_NAME))  == 0) {
		fsize = *file_size / blk_dev->blksz;
		*offset = 0;
		*part_size = fsize;
		*file_size = fsize;
#if defined(CONFIG_GPT_UPDATE_PARAMS)
		g_gptsel = GPT;
#endif
	} else if (strncmp(GPT_BACKUP_PART_NAME, (const char *)part_name,
			sizeof(GPT_BACKUP_PART_NAME)) == 0) {
		fsize = *file_size / blk_dev->blksz;
		*offset = (ulong) blk_dev->lba - fsize;
		*part_size = fsize;
		*file_size = fsize;
#if defined(CONFIG_GPT_UPDATE_PARAMS)
		g_gptsel = GPTBACKUP;
#endif
	} else	{

		BLK_PART_GET_INFO_S(bpart_info, part_name, &disk_info,
					SMEM_BOOT_MMC_FLASH);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret)
			return ret;

		*offset = (uint32_t)disk_info.start;
		*part_size = (uint32_t)disk_info.size;
		if (disk_info.blksz) {
			fsize = *file_size / disk_info.blksz;
			if (*file_size % disk_info.blksz)
				fsize += 1;

			*file_size = fsize;
		}
	}

	return ret;
}
#endif

#ifdef CONFIG_NOR_BLK
static int prepare_nor_gpt(char *part_name, uint32_t *offset,
				uint32_t *part_size, uint32_t* file_size)
{
	uint32_t flash_size = smem_get_flash_size(0);
	uint32_t fsize;
	int ret = 0;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	if (strncmp(NOR_GPT_PART_NAME, (const char *)part_name,
			sizeof(NOR_GPT_PART_NAME))  == 0) {
		fsize = *file_size / SZ_64K;
		if (*file_size % SZ_64K)
			fsize += 1;
		*part_size = fsize * SZ_64K;
		*offset = 0;
		if (sfi->nor_gpt_pte.gpt_pte) {
			free(sfi->nor_gpt_pte.gpt_pte);
			sfi->nor_gpt_pte.gpt_pte = NULL;
		}
#if defined(CONFIG_GPT_UPDATE_PARAMS)
		g_gptsel = GPT;
#endif
	} else if (strncmp(NOR_GPT_BACKUP_PART_NAME, (const char *)part_name,
			sizeof(NOR_GPT_BACKUP_PART_NAME)) == 0) {
		fsize = *file_size / SZ_64K;
		if (*file_size % SZ_64K)
			fsize += 1;
		*part_size = fsize * SZ_64K;
		*offset = flash_size - *file_size;
#if defined(CONFIG_GPT_UPDATE_PARAMS)
		g_gptsel = GPTBACKUP;
#endif
	} else	{
		ret = -1;
	}

	return ret;
}
#endif

int do_flash(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	uint32_t flash_cmd = 0;
	uint32_t offset = 0, part_size = 0, load_addr = 0, file_size = 0;
	uint32_t size_block, start_block, file_size_cpy = 0;
	char *part_name = NULL, *filesize, *loadaddr;
	int flash_type = -1;
	int ret = CMD_RET_FAILURE;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	bool is_ubi = false;
	struct fl_info fl;
#if defined(CONFIG_GPT_UPDATE_PARAMS)
	struct blk_desc *blk_dev;
	uint32_t uclass_id, default_size;
#endif
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info = {0};
	blkpart_info_t  bpart_info;
#endif
#ifdef CONFIG_IPQ_NAND
	struct mtd_info *nand = get_nand_dev_by_index(0);
	if (!nand)
		return -ENODEV;
#endif
	if (strcmp(argv[0], "flash") == 0)
		flash_cmd = CMD_FLASH;
	else if (strcmp(argv[0], "flerase") == 0)
		flash_cmd = CMD_FLERASE;
	else
		flash_cmd = CMD_FLREAD;

	if (g_flash) {
		flash_type = g_flash;
	} else {
		flash_type = sfi->flash_type;
	}

	part_name = argv[1];

	if (flash_cmd == CMD_FLASH) {
		if ((argc < 2) || (argc > 5))
			goto usage_err;

		if (argc ==3 || argc == 5) {
			if(strncmp(argv[argc-1], "emmc", 4) == 0)
				flash_type = SMEM_BOOT_MMC_FLASH;
			else if (strncmp(argv[argc-1], "nand", 4) == 0)
				flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
			else if (strncmp(argv[argc-1], "nor-gpt", 7) == 0)
				flash_type = SMEM_BOOT_NORGPT_FLASH;
			else if (strncmp(argv[argc-1], "nor", 3) == 0)
				flash_type = SMEM_BOOT_SPI_FLASH;
			else
				goto usage_err;
		}

		if (argc == 2 || argc == 3) {
			loadaddr = env_get("fileaddr");
			if (loadaddr != NULL)
				load_addr = simple_strtoul(loadaddr, NULL, 16);
			else
				goto usage_err;

			filesize = env_get("filesize");
			if (filesize != NULL)
				file_size = simple_strtoul(filesize, NULL, 16);
			else
				goto usage_err;

		} else if (argc == 4 || argc ==5) {
			load_addr = simple_strtoul(argv[2], NULL, 16);
			file_size = simple_strtoul(argv[3], NULL, 16);

		} else
			goto usage_err;

		file_size_cpy = file_size;

		/*
		 * validate the APPSBL image
		 */
		if ((flash_type != SMEM_BOOT_NO_FLASH) &&
			(flash_type == sfi->flash_type) &&
			!strncmp(part_name, "0:APPSBL", strlen("0:APPSBL"))) {
			ret = isvalid_appsbl_image((uintptr_t)load_addr);
			if(ret)
				return CMD_RET_FAILURE;
		}
	} else {
		if (argc > 3 || argc < 2)
			goto usage_err;

		if (flash_cmd == CMD_FLREAD) {
			if (argc == 3)
				load_addr = simple_strtoul(argv[2], NULL, 16);
			else
#ifdef CFG_CUSTOM_LOAD_ADDR
				load_addr = CFG_CUSTOM_LOAD_ADDR;
#else
				load_addr = CONFIG_SYS_LOAD_ADDR;
#endif
		} else {
			if (argc != 2)
				goto usage_err;
		}
	}
	switch (flash_type) {
	case SMEM_BOOT_SPI_FLASH:
		ret = get_which_flash_param(part_name);
		switch (ret) {
		case 1:
			/*
			 * Nand flash
			 */
			ret = getpart_offset_size(part_name, &offset,
							&part_size);
			flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
			break;
		case -1:
#ifdef CONFIG_CMD_UBI
			is_ubi = ubi_vol_present(part_name);
			if (is_ubi) {
				flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
				ret = 0;
			}
#endif
#ifdef CONFIG_MMC
			if (sfi->rootfs.offset == 0xBAD0FF5E) {
				flash_type = SMEM_BOOT_MMC_FLASH;
				goto mmc;
			}
#endif
			break;
		default:
			ret = smem_getpart(part_name, &start_block,
						&size_block);
			if (!ret) {
				offset = sfi->flash_block_size * start_block;
				part_size = sfi->flash_block_size * size_block;
			}
		}
		break;
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_QSPI_NAND_FLASH:
		ret = getpart_offset_size(part_name, &offset, &part_size);
		if (ret) {
#ifdef CONFIG_CMD_UBI
			is_ubi = ubi_vol_present(part_name);
			if (is_ubi)
				ret = 0;
#endif
		}
		break;
#endif
#ifdef CONFIG_NOR_BLK
	case SMEM_BOOT_NORGPT_FLASH:
#if defined(CONFIG_GPT_UPDATE_PARAMS)
		uclass_id = UCLASS_SPI;
		default_size = DEFAULT_NOR_FLASH_SIZE;
#endif
		ret = prepare_nor_gpt(part_name, &offset, &part_size,
						&file_size);
		if (ret == 0)
			break;

		BLK_PART_GET_INFO_S(bpart_info, part_name, &disk_info,
					SMEM_BOOT_NORGPT_FLASH);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret) {
#ifdef CONFIG_CMD_UBI
			is_ubi = ubi_vol_present(part_name);
			if (is_ubi) {
				flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
				ret = 0;
			}
#endif
#ifdef CONFIG_MMC
			if (sfi->rootfs.offset == 0xBAD0FF5E) {
				flash_type = SMEM_BOOT_MMC_FLASH;
				goto mmc;
			}
#endif
		} else {
			if (bpart_info.isnand == 1)
				flash_type = SMEM_BOOT_QSPI_NAND_FLASH;

			offset = (uint32_t)disk_info.start * disk_info.blksz;
			part_size = (uint32_t)disk_info.size * disk_info.blksz;
		}
		break;
#endif
#ifdef CONFIG_MMC
	case SMEM_BOOT_MMC_FLASH:
mmc:
#if defined(CONFIG_GPT_UPDATE_PARAMS)
		uclass_id = UCLASS_MMC;
		default_size = DEFAULT_MMC_FLASH_SIZE;
#endif
		ret = prepare_mmc_flash(part_name, &offset, &part_size,
						&file_size);
		break;
#endif
	default:
		printf("unsupported flash type %d\n", flash_type);
	}

	if(ret)
		return CMD_RET_FAILURE;

	if (!is_ubi && (file_size > part_size)) {
		printf("Image size is greater than partition size\n");
		return CMD_RET_FAILURE;
	}

#ifdef CONFIG_IPQ_NAND
	if ((flash_type == SMEM_BOOT_QSPI_NAND_FLASH) &&
		(file_size % nand->writesize)) {
		file_size = file_size + (nand->writesize -
				(file_size % nand->writesize));
	}
#endif

#ifdef CONFIG_CMD_UBI
	if (!strncmp(part_name, "rootfs", strlen("rootfs")) ||
		!strncmp(part_name, "rootfs_1", strlen("rootfs_1")))
		detach_ubi();
#endif

	UPDATE_FL_INFO(&fl, flash_type, offset, load_addr, part_size,
			file_size, part_name, is_ubi);

	watchdog_reset();

	switch(flash_cmd) {
	case CMD_FLERASE:
		if (is_ubi) {
			printf("ubi volume erase not supported \n");
			goto usage_err;
		}
		ret = fl_erase(&fl);
		break;
	case CMD_FLREAD:
		ret = fl_read(&fl);
		break;
	default:
#ifdef CONFIG_CMD_UBI
		if (is_ubi)
			ret = write_ubi_vol(part_name, load_addr, file_size);
		else
#endif
			ret = write_to_flash(&fl);
	}

	if (ret)
		return CMD_RET_FAILURE;

#if defined(CONFIG_GPT_UPDATE_PARAMS)
	if (unlikely(g_gptsel != -1)) {
		blk_dev = blk_get_devnum_by_uclass_id(uclass_id, 0);
		if (!blk_dev) {
			printf("blk_desc uclass_id %d"
			" not found ...\n", uclass_id);
			return CMD_RET_FAILURE;
		}

		if (blk_dev->lba != default_size)
			gpt_update_params(blk_dev, g_gptsel);

		g_gptsel = -1;
	}

#endif
	return CMD_RET_SUCCESS;

usage_err:
	return CMD_RET_USAGE;
}

static int do_mibib_reload(struct cmd_tbl *cmdtp, int flag, int argc,
char * const argv[])
{
	uint32_t load_addr, file_size;
	uint32_t page_size;
	uint8_t flash_type;
	struct header* mibib_hdr;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	if (argc == 5) {
		flash_type = simple_strtoul(argv[1], NULL, 16);
		page_size = simple_strtoul(argv[2], NULL, 16);
		sfi->flash_block_size = simple_strtoul(argv[3], NULL, 16);
		sfi->flash_density = simple_strtoul(argv[4], NULL, 16);
		load_addr = env_get_ulong("fileaddr", 16, 0);
		file_size = env_get_ulong("filesize", 16, 0);
	} else
		return CMD_RET_USAGE;

	if (flash_type > 1) {
		printf("Invalid flash type \n");
		return CMD_RET_FAILURE;
	}

	if (file_size < 2 * page_size) {
		printf("Invalid filesize \n");
		return CMD_RET_FAILURE;
	}

	mibib_hdr = (struct header*)((uintptr_t) load_addr);
	if (mibib_hdr->magic[0] == HEADER_MAGIC1 &&
		mibib_hdr->magic[1] == HEADER_MAGIC2 &&
		mibib_hdr->version == HEADER_VERSION) {

		load_addr += page_size;
	}
	else {
		printf("Header magic/version is invalid\n");
		return CMD_RET_FAILURE;
	}

	if (mibib_ptable_init((unsigned int *)((uintptr_t) load_addr))) {
		printf("Table magic is invalid\n");
		return CMD_RET_FAILURE;
	}

	if (flash_type == 0) {
		/*NAND*/
		sfi->flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
	} else {
		/* NOR*/
		sfi->flash_type = SMEM_BOOT_SPI_FLASH;
	}

#ifdef CONFIG_CMD_UBI
	detach_ubi();
#endif
	get_kernel_fs_part_details(sfi->flash_type);

	return CMD_RET_SUCCESS;
}

void print_fl_msg(char *fname, bool started, int ret)
{
	printf("######################################## ");
	printf("Flashing %s %s\n", fname,
			started ? "Started" : ret ? "Failed" : "Done");
}

int do_xtract_n_flash(struct cmd_tbl *cmdtp, int flag, int argc,
char * const argv[])
{
	char runcmd[256], fname_stripped[256];
	char alt_part_name[SMEM_PTN_NAME_MAX] = {0};
	char *file_name, *part_name;
	uint32_t load_addr, verbose;
	int ret = CMD_RET_SUCCESS;
	u16 flash_type = -1;
	uint8_t redo = 0;

	if (argc < 4 || argc > 5)
		return CMD_RET_USAGE;

	verbose = env_get_ulong("verbose", 10, 0);
	load_addr = simple_strtoul(argv[1], NULL, 16);
	file_name = argv[2];
	part_name = argv[3];
	if (argc == 5) {
		if(strncmp(argv[4], "emmc", 4) == 0)
			flash_type = SMEM_BOOT_MMC_FLASH;
		else if (strncmp(argv[4], "nand", 4) == 0)
			flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
		else if (strncmp(argv[4], "nor-gpt", 7) == 0)
			flash_type = SMEM_BOOT_NORGPT_FLASH;
		else if (strncmp(argv[4], "nor", 3) == 0)
			flash_type = SMEM_BOOT_SPI_FLASH;
		else
			return CMD_RET_USAGE;
	}
redo_flash:
	snprintf(fname_stripped , sizeof(fname_stripped),
		"%.*s:",(int) (strlen(file_name) - SHA1_SIG_LEN), file_name);

	if (verbose)
		print_fl_msg(fname_stripped, 1, ret);
	else
		env_set("stdout", "nulldev");

	if (verbose && (genimg_get_format((void *)(uintptr_t)load_addr)
				== IMAGE_FORMAT_FIT)) {
		if (!fit_check_format((const void *)(uintptr_t)load_addr,
					IMAGE_SIZE_INVAL)) {
			char *desc;
			int noffset = fit_image_get_node((const void *)
					(uintptr_t)load_addr, file_name);
			if (noffset >= 0) {
				if (!fit_get_desc((const void *)(uintptr_t)
						load_addr, noffset, &desc))
					printf("image name: %s\n", desc);
			}
		}
	}

#ifdef CONFIG_BOOTCONFIG_V2
	if(!strncmp(part_name, "0:BOOTCONFIG", sizeof(char) * 12 ))
		write_tcsr_boot_misc_reg(BOOTCONFIG_HEALTH_MASK |
					BOOTCONFIG1_HEALTH_MASK, 0);
#endif

	if(5 == argc) {

		snprintf(runcmd , sizeof(runcmd),
			"imxtract 0x%x %s && "
			"flash %s %s",
			load_addr, file_name,
			part_name, argv[4]);
	} else if(4 == argc) {
		snprintf(runcmd , sizeof(runcmd),
			"imxtract 0x%x %s && "
			"flash %s",
			load_addr, file_name,
			part_name);
	}
	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		ret = CMD_RET_FAILURE;

	if (verbose)
		print_fl_msg(fname_stripped, 0, ret);
	else {
		env_set("stdout", "serial");
		printf("Flashing %-30s %s\n", fname_stripped,
				ret ? "[ failed ]" : "[ done ]");
	}

	if (unlikely(!redo && env_get("flash_alt_partition"))) {
		if (strstr(part_name,"GPT") ||
			!strncmp(part_name, "0:MIBIB", 7 ) ||
			!strncmp(part_name, "wifi_fw", 7 ) ||
			!strncmp(part_name, "0:BOOTCONFIG", 12))
			return ret;

		snprintf(alt_part_name, SMEM_PTN_NAME_MAX,
			"%s%s", part_name, "_1");
		part_name = alt_part_name;
		redo = 1;
		goto redo_flash;
	}

	if (redo && ret)
		ret = CMD_RET_SUCCESS;

	return ret;
}

static int do_flupdate(struct cmd_tbl *cmdtp, int flag, int argc,
			char * const argv[])
{
	int ret = CMD_RET_USAGE;

	if (argc < 2 || argc > 3)
		goto quit;

	if (!strncmp(argv[1], "set", 3)) {
		if(argc != 3)
			goto quit;

		if (!strncmp(argv[2], "mmc", 3))
			g_flash = SMEM_BOOT_MMC_FLASH;
		else if (!strncmp(argv[2], "nor-gpt", 7))
			g_flash = SMEM_BOOT_NORGPT_FLASH;
		else if (!strncmp(argv[2], "nor", 3))
			g_flash = SMEM_BOOT_SPI_FLASH;
		else if (!strncmp(argv[2], "nand", 4))
			g_flash = SMEM_BOOT_QSPI_NAND_FLASH;
		else
			goto quit;

	} else if (!strncmp(argv[1], "clear", 5)) /* set flash type to default */
		g_flash = 0;
	else
		goto quit;

	ret = CMD_RET_SUCCESS;
quit:
	return ret;
}

static int do_flash_init(struct cmd_tbl *cmdtp, int flag, int argc,
		char * const argv[]) {
	return 0;
}

U_BOOT_CMD(
	flash,       5,      0,      do_flash,
	"flash part_name \n"
	"\tflash part_name load_addr file_size \n"
	"\tflash part_name flash_type{emmc/nand/nor/nor-gpt} \n"
	"\tflash part_name load_addr file_size flash_type{emmc/nand/nor/nor-gpt}\n",
	"flash the image at load_addr, given file_size in hex"
);

U_BOOT_CMD(
	flerase,       2,      0,      do_flash,
	"flerase part_name \n",
	"erases on flash the given partition \n"
);

U_BOOT_CMD(
	flread,       3,      0,      do_flash,
	"flread part_name location(optional)\n",
	"read from flash the given partition \n"
);

U_BOOT_CMD(
	mibib_reload,       5,      0,      do_mibib_reload,
	"mibib_reload fl_type pg_size blk_size chip_size\n",
	"reloads the smem partition info from mibib \n"
);

U_BOOT_CMD(
	xtract_n_flash,       5,      0,      do_xtract_n_flash,
	"xtract_n_flash addr filename partname \n",
	"xtract the image and flash \n"
);

U_BOOT_CMD(
	flashinit,       2,      0,      do_flash_init,
	"flashinit nand/mmc \n",
	"Init the flash \n"
);

U_BOOT_CMD(
	flupdate,       3,       0,       do_flupdate,
	"flupdate set mmc/nand/nor/nor-gpt ; flupdate clear \n",
	"flash type update \n"
);
