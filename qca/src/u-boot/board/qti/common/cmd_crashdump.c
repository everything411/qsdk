/*
 * Copyright (c) 2015-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <stdio.h>
#include <memalign.h>
#include <command.h>
#include <common.h>
#include <time.h>
#include <cpu_func.h>
#include <dm.h>
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
#include <usb.h>
#include <fat.h>
#endif
#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
#include <gzip.h>
#endif

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
#ifdef CONFIG_IPQ_MMC
#include <mmc.h>
#include <sdhci.h>
#endif

#ifdef CONFIG_IPQ_NAND
#include <nand.h>
#endif

#ifdef CONFIG_IPQ_SPI_NOR
#include <spi.h>
#include <spi_flash.h>
#endif
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

#ifdef CONFIG_WDT
#include <dm/uclass-internal.h>
#include <wdt.h>
#endif

#include "ipq_board.h"

#define TFTP_MAX_TRF_SZ_LIMIT			SZ_1G
#define DRAM_DUMP_NAME_PREFIX			"EBICS"

#if defined(CONFIG_IPQ_CRASHDUMP_TO_MEMORY) || \
	defined(CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY)

typedef struct {
	char name[DUMP_NAME_STR_MAX_LEN];
	uint64_t offset;
	uint64_t size;
} memdump_list_info_t;

typedef struct {
	uint32_t magic1;
	uint32_t magic2;
	uint32_t nos_memdumps;
	uint32_t total_dump_sz;
	uint64_t dump_list_info_offset;
	uint32_t reserved[2];
} memdump_hdr_t;
#endif
/* CONFIG_IPQ_CRASHDUMP_TO_MEMORY (or) CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP

#define TEMP_COMPRESS_BUF_NAME			"EBICS0.BIN.gz"
#define TEMP_COMPRESS_BUF_SZ			SZ_16M

#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */

#ifdef CONFIG_IPQ_MINIDUMP

#define	CPU_DUMP_NAME_PREFIX			"CPU_INFO"
#define	UNAME_DUMP_NAME_PREFIX			"UNAME"
#define	DMESG_DUMP_NAME_PREFIX			"DMESG"
#define	PT_DUMP_NAME_PREFIX			"PT"
#define	WLAN_MOD_DUMP_NAME_PREFIX		"WLAN_MOD"

#define CFG_QTI_KERN_WDT_ADDR			*((unsigned int *)0x08600658)

#define QTI_WDT_SCM_TLV_TYPE_SIZE		1
#define QTI_WDT_SCM_TLV_LEN_SIZE		2
#define QTI_WDT_SCM_TLV_TYPE_LEN_SIZE		(QTI_WDT_SCM_TLV_TYPE_SIZE +\
						QTI_WDT_SCM_TLV_LEN_SIZE)

typedef struct {
	uint8_t *msg_buf;
	uint8_t *cur_msg_buf;
	uint32_t buf_len;
} wdt_dump_tlv_infos_t;

typedef struct {
	uint64_t start;
	uint64_t size;
} st_tlv_data_t;

enum {
	/*Basic DDR segments */
	QTI_WDT_LOG_DUMP_TYPE_INVALID = 0,
	QTI_WDT_LOG_DUMP_TYPE_UNAME,
	QTI_WDT_LOG_DUMP_TYPE_DMESG,
	QTI_WDT_LOG_DUMP_TYPE_LEVEL1_PT,

	/* Module structures are in highmem zone*/
	QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD,
	QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD_DEBUGFS,
	QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD_INFO,
	QTI_WDT_LOG_DUMP_TYPE_WLAN_MMU_INFO,
	QTI_WDT_LOG_DUMP_TYPE_EMPTY,
} wdt_dump_types_t;
#endif /* CONFIG_IPQ_MINIDUMP */

typedef struct {
	char name[DUMP_NAME_STR_MAX_LEN];
	uint64_t start_addr;
	uint64_t size;
	uint8_t is_aligned_access:1;
	uint8_t compression_support:1;
	uint8_t dumptoflash_support:1;
	struct list_head list;
} crashdump_infos_int_t;

typedef struct {
	char *tftp_serverip;
	char *tftp_dumpdir;

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
	uint64_t comp_out_addr;
	uint64_t comp_out_size;
#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
	uint8_t usb_dev_idx;
	uint8_t usb_part_idx;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

#if defined(CONFIG_IPQ_CRASHDUMP_TO_FLASH) || \
	defined(CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY)
	void *crashdump_cnxt;
	uint64_t crashdump_offset;
	uint64_t dump_total_size;
	uint8_t flash_type;
	char *part_name;
	uint64_t part_size;
	uint64_t part_blksize;
	uint64_t init_dump_off;
#endif
/* CONFIG_IPQ_CRASHDUMP_TO_FLASH (or) CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_MEMORY
	uint64_t dump2mem_rsvd_addr;
	uint64_t dump2mem_rsvd_limit;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_MEMORY */

#if defined(CONFIG_IPQ_CRASHDUMP_TO_MEMORY) || \
	defined(CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY)
	uint64_t dump2mem_curr_addr;
	memdump_hdr_t memdump_hdr;
	memdump_list_info_t *memdump_list;
#endif
/* CONFIG_IPQ_CRASHDUMP_TO_MEMORY (or) CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
	char emmc_part_dev[5];
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

} crashdump_interface_cfg_t;

typedef struct {
	bool force_collect_dump;
	uint8_t debug;
	uint8_t dump_level;
	uint8_t dump_to;
	uint8_t is_compress_enabled;
	crashdump_interface_cfg_t iface_cfg;
	crashdump_infos_t *dump_infos;
	uint8_t nos_dumps;
	uint16_t actual_nos_dumps;
	uint64_t ram_top;
} crashdump_config_t;

static LIST_HEAD(actual_dumps_list);
static crashdump_config_t dump_config;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
#ifdef CONFIG_IPQ_NAND
/* Context for NAND Flash memory */
struct crashdump_flash_nand_cxt {
	uint64_t cur_crashdump_offset;
	int cur_page_data_len;
	int write_size;
	uint8_t* temp_data;
	uint32_t part_start;
	uint32_t part_size;
};
static struct crashdump_flash_nand_cxt crashdump_nand_cnxt;
#endif

#ifdef CONFIG_IPQ_SPI_NOR
/* Context for SPI NOR Flash memory */
struct crashdump_flash_spi_cxt {
	struct spi_flash *crashdump_spi_flash;
	uint64_t cur_crashdump_offset;
};
static struct crashdump_flash_spi_cxt crashdump_flash_spi_cnxt;
#endif

#ifdef CONFIG_IPQ_MMC
/* Context for EMMC Flash memory */
struct crashdump_flash_emmc_cxt {
	uint64_t cur_crashdump_offset;
	int cur_blk_data_len;
	int write_size;
	uint8_t *temp_data;
	struct mmc *mmc;
	struct blk_desc *desc;
};
static struct crashdump_flash_emmc_cxt crashdump_emmc_cnxt;
#endif

static int (*crashdump_flash_write)(void *cnxt, uint8_t *data, uint32_t size);
static int (*crashdump_flash_write_init)(void *cnxt, uint64_t offset,
						uint32_t size);
static int (*crashdump_flash_write_deinit)(void *cnxt);
static int crashdump_flash_get_args(uint8_t *flash_type, uint64_t *offset);
static int crashdump_flash_set_fn_ops(crashdump_config_t *dump_config);
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

extern int initr_net(void);
/**
 * add_entry_crashdump_table() - Adds an entry into dump table
 * &dump_config - crashdump ocnfiguration info
 * &dump_entry - entry that needs to be added into the dump table
 */
static int add_entry_crashdump_table(crashdump_config_t *dump_config,
		crashdump_infos_int_t *dump_entry)
{
	crashdump_infos_int_t *new_entry;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	if (dump_config->dump_to ==  DUMP_TO_FLASH) {
		if (dump_entry->dumptoflash_support)
			dump_config->iface_cfg.dump_total_size +=
							dump_entry->size;
		else
			return 0;
	}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

	new_entry = malloc(sizeof(crashdump_infos_int_t));
	if (!new_entry) {
		printf("failed to allocate memory to add dump entry \n");
		return -ENOMEM;
	}

	memset(new_entry, 0, sizeof(crashdump_infos_int_t));
	memcpy(new_entry, dump_entry, sizeof(crashdump_infos_int_t));
	list_add_tail(&new_entry->list, &actual_dumps_list);
	dump_config->actual_nos_dumps++;

	if (dump_config->debug)
		printf("%-20s\t 0x%010llX\t 0x%010llX\t %-10s\t %-10s\t\n",
			new_entry->name, new_entry->start_addr,
			new_entry->size,
			(new_entry->is_aligned_access ? "true":"false"),
			(new_entry->compression_support ? "true":"false"));

	return 0;
}

/**
 * delete_crashdump_table() - remove all the dump table entries and frees
 * the pointer.
 */
static void delete_crashdump_table(void)
{
	crashdump_infos_int_t *dump_entry, *tmp_entry;

	list_for_each_entry_safe(dump_entry, tmp_entry,
			&actual_dumps_list, list) {
		list_del(&dump_entry->list);
		free(dump_entry);
	}
}

#ifdef CONFIG_IPQ_MINIDUMP
/**
 * wdt_extract_tlv_info() - extract tlv header info
 * &tlv_info - tlv dump list head pointer
 * &type - its an o/p variable contains the tlv type
 * &size - its an o/p variable contains the tlv size
 */
static int wdt_extract_tlv_info(wdt_dump_tlv_infos_t *tlv_info,
		uint8_t *type, uint32_t *size)
{
	uint8_t *x = tlv_info->cur_msg_buf;
	uint8_t *y = tlv_info->msg_buf + tlv_info->buf_len;

	if ((x + QTI_WDT_SCM_TLV_TYPE_LEN_SIZE) >= y)
		return -EINVAL;

	*type = x[0];
	*size = x[1] | (x[2] << 8);
	return 0;
}

/**
 * wdt_extract_tlv_data() - extract tlv data
 * &tlv_info - tlv dump list head pointer
 * &data - pointer to the buffer where the extracted tlv data is copied
 * &size - size of the tlv data
 */
static int wdt_extract_tlv_data(wdt_dump_tlv_infos_t *tlv_info,
		uint8_t *data, uint32_t size)
{
	uint8_t *x = tlv_info->cur_msg_buf;
	uint8_t *y = tlv_info->msg_buf + tlv_info->buf_len;

	if ((x + QTI_WDT_SCM_TLV_TYPE_LEN_SIZE + size) >= y)
		return -EINVAL;

	memcpy(data, x + 3, size);

	tlv_info->cur_msg_buf += (size + QTI_WDT_SCM_TLV_TYPE_LEN_SIZE);
	return 0;
}

/**
 * wdt_extract_dump() - extract details of the requested tlv dump
 * &dump_config - crashdump configuration info
 * &dump_idx - buffer index points to the requested dump info
 * &dump_entry - pointer to store the extract tlv dump info
 */
static int wdt_extract_dump(crashdump_config_t *dump_config, int dump_idx,
		crashdump_infos_int_t *dump_entry)
{
	int ret = CMD_RET_FAILURE;
	uint8_t cur_type, i;
	uint8_t tlv_type = cur_type = QTI_WDT_LOG_DUMP_TYPE_INVALID;
	uint32_t cur_size = 0;
	wdt_dump_tlv_infos_t tlv_info;
	crashdump_infos_t *dump_infos = &dump_config->dump_infos[dump_idx];
	char *dumps[] = { "INVALID",
		UNAME_DUMP_NAME_PREFIX,
		DMESG_DUMP_NAME_PREFIX,
		PT_DUMP_NAME_PREFIX,
		WLAN_MOD_DUMP_NAME_PREFIX,
	};

	if (!strncmp(dump_infos->name, CPU_DUMP_NAME_PREFIX,
				strlen(CPU_DUMP_NAME_PREFIX))) {
		dump_entry->start_addr = CFG_QTI_KERN_WDT_ADDR;
		dump_entry->size = CFG_CPU_CONTEXT_DUMP_SIZE;
		snprintf(dump_entry->name, sizeof(dump_entry->name),
				"%llX.BIN", dump_entry->start_addr);
		ret = CMD_RET_SUCCESS;
		return ret;
	}

	for (i=0; i<=QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD; i++) {
		if (!strncmp(dump_infos->name, dumps[i], strlen(dumps[i]))) {
			tlv_type = i;
			break;
		}
	}

	if (tlv_type == QTI_WDT_LOG_DUMP_TYPE_INVALID)
		return ret;

	tlv_info.msg_buf = (uint8_t*)(uintptr_t)
		(CFG_QTI_KERN_WDT_ADDR + TLV_BUF_OFFSET);
	tlv_info.cur_msg_buf = tlv_info.msg_buf;
	tlv_info.buf_len = CFG_TLV_DUMP_SIZE;

	do {
		uint8_t buf[512] = { 0 };
		st_tlv_data_t *tlv_data = NULL;

		ret = wdt_extract_tlv_info(&tlv_info, &cur_type, &cur_size);
		if (ret)
			break;

		if ((tlv_type == QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD) &&
				(cur_type >= QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD)
				&& (cur_type <=
				QTI_WDT_LOG_DUMP_TYPE_WLAN_MMU_INFO)){

			ret = wdt_extract_tlv_data(&tlv_info, buf, cur_size);
			if (ret)
				break;

			tlv_data = (st_tlv_data_t*)&buf;
			dump_entry->start_addr = tlv_data->start;

			switch (cur_type) {
			case QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD_INFO:
				snprintf(dump_entry->name,
					sizeof(dump_entry->name),
					"MOD_INFO.txt");
				dump_entry->size = *(uint32_t *)(uintptr_t)
					tlv_data->size;
				break;
			case QTI_WDT_LOG_DUMP_TYPE_WLAN_MMU_INFO:
				snprintf(dump_entry->name,
					sizeof(dump_entry->name),
					"MMU_INFO.txt");
				dump_entry->size = *(uint32_t *)(uintptr_t)
					tlv_data->size;
				break;
			case QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD_DEBUGFS:
				snprintf(dump_entry->name,
					sizeof(dump_entry->name),
					"DEBUGFS_%X.BIN",
					(uint32_t)tlv_data->start);
				dump_entry->size = tlv_data->size;
				break;
			case QTI_WDT_LOG_DUMP_TYPE_WLAN_MOD:
				snprintf(dump_entry->name,
					sizeof(dump_entry->name),
					"%X.BIN", (uint32_t)tlv_data->start);
				dump_entry->size = tlv_data->size;
				break;
			}

			ret = add_entry_crashdump_table(dump_config,
					dump_entry);
			if (ret)
				break;
		} else if (cur_type == tlv_type) {
			ret = wdt_extract_tlv_data(&tlv_info, buf, cur_size);
			if (ret)
				break;

			switch (cur_type) {
			case QTI_WDT_LOG_DUMP_TYPE_UNAME:
				void * uname_buf = malloc(cur_size);
				if (!uname_buf)
					return -ENOMEM;
				else
					memcpy(uname_buf, buf, cur_size);
				dump_entry->start_addr = (uintptr_t)uname_buf;
				dump_entry->size = cur_size;
				break;
			case QTI_WDT_LOG_DUMP_TYPE_DMESG:
				tlv_data = (st_tlv_data_t*)&buf;
				dump_entry->start_addr = tlv_data->start;
				dump_entry->size = *(uint32_t *)(uintptr_t)
					tlv_data->size;
				break;
			case QTI_WDT_LOG_DUMP_TYPE_LEVEL1_PT:
				tlv_data = (st_tlv_data_t*)&buf;
				dump_entry->start_addr = tlv_data->start;
				dump_entry->size = tlv_data->size;
				break;
			}

			snprintf(dump_entry->name, sizeof(dump_entry->name),
					"%llX.BIN", dump_entry->start_addr);
			ret = CMD_RET_SUCCESS;
			break;
		} else {
			tlv_info.cur_msg_buf +=
				(cur_size + QTI_WDT_SCM_TLV_TYPE_LEN_SIZE);
		}
	} while (cur_type != QTI_WDT_LOG_DUMP_TYPE_INVALID);

	if (cur_type == QTI_WDT_LOG_DUMP_TYPE_INVALID)
		ret = -EINVAL;

	return ret;
}
#endif /* CONFIG_IPQ_MINIDUMP */

/**
 * ipq_read_tcsr_boot_misc() - read boot tcsr register
 */
__weak int ipq_read_tcsr_boot_misc(void)
{
	u32 *dmagic = TCSR_BOOT_MISC_REG;
	return *dmagic;
}

/**
 * ipq_iscrashed_crashdump_disabled() - to check whether crashdump is
 * enabled or disabled
 */
static int ipq_iscrashed_crashdump_disabled(void)
{
	u32 dmagic = ipq_read_tcsr_boot_misc();
	return ((dmagic & DLOAD_DISABLED) ? 1 : 0);
}

/**
 * ipq_iscrashed() - to check whether system is in crashdump path or not
 */
static int ipq_iscrashed(void)
{
	u32 dmagic = ipq_read_tcsr_boot_misc();
	return ((dmagic & DLOAD_MAGIC_COOKIE) ? 1 : 0);
}

/**
 * parse_crashdump_config() - parse the crashdump configurations from the
 * system environments
 * &dump_config - pointer where the parsed info is stored to
 */
static void parse_crashdump_config(crashdump_config_t * dump_config)
{
#ifdef CONFIG_IPQ_MINIDUMP
	if (env_get("dump_minimal_and_full"))
		dump_config->dump_level = MINIDUMP_AND_FULLDUMP;
	else if (env_get("dump_minimal") || env_get("dump_to_flash"))
		dump_config->dump_level = MINIDUMP;
	else
#endif /* CONFIG_IPQ_MINIDUMP */
		dump_config->dump_level = FULLDUMP;

	dump_config->dump_to = DUMP_TO_TFTP;
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
	if (env_get("dump_to_usb"))
		dump_config->dump_to = DUMP_TO_USB;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_MEMORY
	if (env_get("dump_to_mem"))
		dump_config->dump_to = DUMP_TO_MEM;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_MEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	if (env_get("dump_to_nvmem"))
		dump_config->dump_to = DUMP_TO_NVMEM;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	if (env_get("dump_to_flash"))
		dump_config->dump_to = DUMP_TO_FLASH;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
	if (env_get("dump_to_emmc"))
		dump_config->dump_to = DUMP_TO_EMMC;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

	dump_config->force_collect_dump =
		env_get("force_collect_dump") ? 1 : 0;

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
	dump_config->is_compress_enabled = env_get("dump_compressed") ? 1 : 0;
#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */
	return;
}

/**
 * verify_crashdump_config() - verify the given crashdump configuration and
 * check for conflicts and return failure when it conflicts.
 * &dump_config - crashdump configuration info
 */
static int verify_crashdump_config(crashdump_config_t * dump_config)
{
	int ret = CMD_RET_SUCCESS;

#ifdef CONFIG_IPQ_MINIDUMP
	if (dump_config->dump_level == MINIDUMP_AND_FULLDUMP) {
		if (dump_config->is_compress_enabled) {
			printf("Compression is not supported in both "
					"minimal and fulldump\n");
			return CMD_RET_FAILURE;
		}
	}
#endif

	switch (dump_config->dump_to) {
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
	case DUMP_TO_USB:
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_MEMORY
	case DUMP_TO_MEM:
		char *tmp = env_get("dump_to_mem");
		if (!tmp) {
			printf("%s: dump_to_mem env not available\n",
				__func__);
			ret = CMD_RET_FAILURE;
			break;
		}

		char *dump2mem_addr_s = NULL, *dump2mem_sz_s = NULL;

		dump2mem_addr_s = strsep(&tmp, " ");
		if (!dump2mem_addr_s || !str2long(dump2mem_addr_s,
				(ulong*)
				&dump_config->iface_cfg.dump2mem_rsvd_addr)) {
			printf("Failed to decode dump_to_mem reserved mem \n");
			ret = CMD_RET_FAILURE;
		}

		dump2mem_sz_s = strsep(&tmp, " ");
		if (!dump2mem_sz_s || !str2long(dump2mem_sz_s,
				(ulong*)
				&dump_config->iface_cfg.dump2mem_rsvd_limit)){
			printf("Failed to decode dump_to_mem size\n");
			ret = CMD_RET_FAILURE;
		}

		dump_config->iface_cfg.dump2mem_rsvd_limit =
			dump_config->iface_cfg.dump2mem_rsvd_addr +
			dump_config->iface_cfg.dump2mem_rsvd_limit;
		/* range check for the dump2mem_addr */
		if ((dump_config->iface_cfg.dump2mem_rsvd_addr <
				CFG_SYS_SDRAM_BASE) ||
				(dump_config->iface_cfg.dump2mem_rsvd_addr >
				 gd->ram_top) ||
				(dump_config->iface_cfg.dump2mem_rsvd_limit >
				 gd->ram_top)) {
			printf("Invalid dump_to_mem param\n");
			ret = CMD_RET_FAILURE;
		}

		if (dump_config->dump_level != MINIDUMP) {
			printf("Only Minidump is supported in dump_to_mem\n");
			ret = CMD_RET_FAILURE;
		}

		if (dump_config->is_compress_enabled) {
			printf("Compression is not supported "
					"in dump_to_mem\n");
			ret = CMD_RET_FAILURE;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_MEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	case DUMP_TO_NVMEM:
		char *part_name = env_get("dump_to_nvmem");
		if (!part_name) {
			printf("%s: dump_to_nvmem env not available\n",
				__func__);
			ret = CMD_RET_FAILURE;
			break;
		}
		ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

		dump_config->iface_cfg.part_name = part_name;
		dump_config->iface_cfg.flash_type =
			(sfi->flash_secondary_type ?
			 sfi->flash_secondary_type : sfi->flash_type);

		if ((dump_config->iface_cfg.flash_type != SMEM_BOOT_MMC_FLASH)
				&& (dump_config->iface_cfg.flash_type !=
				SMEM_BOOT_QSPI_NAND_FLASH)) {
			printf("Error: Invalid flash partition [NAND/EMMC]\n");
			ret = CMD_RET_FAILURE;
			break;
		}

#ifdef CONFIG_IPQ_NAND
		uint32_t off, size, valid_start_off;
		uint8_t valid_start_found = 0;
		struct mtd_info *mtd = get_nand_dev_by_index(0);
		if (!mtd) {
			printf("Error: %s Invalid partition\n", part_name);
			ret = CMD_RET_FAILURE;
			break;
		}

#ifdef CONFIG_NOR_BLK
		if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
			blkpart_info_t bpart_info;
			struct disk_partition disk_info;

			BLK_PART_GET_INFO_S(bpart_info, part_name, &disk_info,
					sfi->flash_type);
			ret = ipq_part_get_info_by_name(&bpart_info);
			if (ret)
				return ret;

			off = bpart_info.info->start *
					bpart_info.desc->blksz;
			size = bpart_info.info->size *
					bpart_info.desc->blksz;
		} else
#endif /* CONFIG_NOR_BLK */
		{
			ret = getpart_offset_size(part_name, &off, &size);
			if (ret) {
				printf("Error: %s Invalid partition\n",
						part_name);
				ret = CMD_RET_FAILURE;
				break;
			}
		}

		dump_config->iface_cfg.crashdump_offset
			= valid_start_off = off;
		dump_config->iface_cfg.part_size = size;
		dump_config->iface_cfg.part_blksize = mtd->erasesize;
		dump_config->iface_cfg.init_dump_off = mtd->erasesize;
		for (; off < (dump_config->iface_cfg.crashdump_offset + size);
				off += mtd->erasesize) {
			if (nand_block_isbad(mtd, off)) {
				dump_config->iface_cfg.part_size
					-= mtd->erasesize;
			} else if (!valid_start_found) {
				valid_start_off = off;
				valid_start_found = 1;
			}
		}

		if (!dump_config->iface_cfg.part_size) {
			printf("Error: %s bad partition\n", part_name);
			ret = CMD_RET_FAILURE;
			break;
		} else
			dump_config->iface_cfg.crashdump_offset
				= valid_start_off;
#endif

#ifdef CONFIG_IPQ_MMC
		struct disk_partition disk_info;
		blkpart_info_t bpart_info;
		if (!find_mmc_device(0)) {
			printf("Error: %s Invalid partition\n", part_name);
			ret = CMD_RET_FAILURE;
			break;
		}

		BLK_PART_GET_INFO_S(bpart_info, part_name, &disk_info,
					dump_config->iface_cfg.flash_type);
		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret) {
			printf("Error: %s Invalid partition\n", part_name);
			ret = CMD_RET_FAILURE;
			break;
		}

		dump_config->iface_cfg.crashdump_offset = disk_info.start;
		dump_config->iface_cfg.part_size =
					disk_info.size * disk_info.blksz;
		dump_config->iface_cfg.part_blksize = disk_info.blksz;
		dump_config->iface_cfg.init_dump_off = 1;
#endif

		if (dump_config->dump_level != MINIDUMP) {
			printf("Only Minidump is supported in dump_to_mem\n");
			ret = CMD_RET_FAILURE;
		}

		if (dump_config->is_compress_enabled) {
			printf("Compression is not supported "
					"in dump_to_mem\n");
			ret = CMD_RET_FAILURE;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

	case DUMP_TO_TFTP:
		dump_config->iface_cfg.tftp_serverip = env_get("serverip");
		if (dump_config->iface_cfg.tftp_serverip != NULL) {
			printf("Using Server IP from env %s\n",
					dump_config->iface_cfg.tftp_serverip);
		} else {
			printf("Server IP not found, run dhcp or configure\n");
			ret = CMD_RET_FAILURE;
		}

		dump_config->iface_cfg.tftp_dumpdir = env_get("dumpdir");
		if (dump_config->iface_cfg.tftp_dumpdir != NULL)
			printf("Using directory %s in TFTP server\n",
					dump_config->iface_cfg.tftp_dumpdir);
		else {
			dump_config->iface_cfg.tftp_dumpdir = "";
			printf("Env 'dumpdir' not set. "
					"Using / dir in TFTP server\n");
		}
		break;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	case DUMP_TO_FLASH:
		ret = crashdump_flash_get_args(
			&dump_config->iface_cfg.flash_type,
			&dump_config->iface_cfg.crashdump_offset);
		if (ret) {
			printf("Failed to collect crashdump in flash\n");
			return CMD_RET_FAILURE;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
	case DUMP_TO_EMMC:
		if (dump_config->dump_level != FULLDUMP) {
			printf("Only Fulldump is supported in dump_to_emmc\n");
			ret = CMD_RET_FAILURE;
		}

		if (!dump_config->is_compress_enabled) {
			printf("Only Compressed full dump allowed "
					"in dump_to_emmc\n");
			ret = CMD_RET_FAILURE;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

	default:
		ret = CMD_RET_FAILURE;
		break;
	}

	return ret;
}

#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
/**
 * find_usb_dev_for_crashdump() - find the first usb device & partition with
 * FAT filesystem.
 * &dev_idx - its an o/p variable points to USB device index
 * &part_idx - its an o/p variable points to USB partition index
 */
static int find_usb_dev_for_crashdump(uint8_t *dev_idx, uint8_t *part_idx)
{
	int ret = CMD_RET_FAILURE;

	struct udevice *dev;

	for (blk_first_device(UCLASS_USB, &dev); dev;
			blk_next_device(&dev)) {
		char dev_str[5] = { 0 };
		struct blk_desc *bdev = dev_get_uclass_plat(dev);
		struct disk_partition dpart_info;

		for (int pidx = 1; (bdev && (pidx <= MAX_SEARCH_PARTITIONS));
				pidx++) {
			struct blk_desc *bdesc = NULL;

			snprintf(dev_str, sizeof(dev_str), "%x:%x",
					bdev->devnum, pidx);
			ret = blk_get_device_part_str("usb", dev_str,
					&bdesc, &dpart_info, 1);
			if ((ret < 0) || !bdesc)
				continue;

			if (fat_set_blk_dev(bdesc, &dpart_info) == 0) {
				*dev_idx = bdev->devnum;
				*part_idx = pidx;

				printf("Selected Device:%d "
					"Partition:%d for USB dump:\n",
					*dev_idx, *part_idx);
				return CMD_RET_SUCCESS;
			}
		}
	}

	return ret;
}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

/**
 * verify_crashdump_iface() - verify the opted crashdump inferface
 * &dump_config - crashdump configuration info
 */
static int verify_crashdump_iface(crashdump_config_t * dump_config)
{
	int ret = CMD_RET_SUCCESS;
	char runcmd[50] = {0};
	uint8_t ping_status = 0;

	switch (dump_config->dump_to) {
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
	case DUMP_TO_USB:
		ret = usb_init();
		if (ret) {
			ret = CMD_RET_FAILURE;
			break;
		}

		ret = find_usb_dev_for_crashdump(
				&dump_config->iface_cfg.usb_dev_idx,
				&dump_config->iface_cfg.usb_part_idx);
		if (ret) {
			printf("No USB dev partition available for "
					"dump collection\n");
			ret = CMD_RET_FAILURE;
			break;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_MEMORY
	case DUMP_TO_MEM:
		memset((void*)(uintptr_t)
				dump_config->iface_cfg.dump2mem_rsvd_addr,
				0xffffffff, sizeof(memdump_hdr_t));
		memdump_hdr_t *hdr = &dump_config->iface_cfg.memdump_hdr;
		dump_config->iface_cfg.dump2mem_curr_addr =
			dump_config->iface_cfg.dump2mem_rsvd_addr;
		hdr->magic1 = DUMP2MEM_MAGIC1_COOKIE;
		hdr->magic2 = DUMP2MEM_MAGIC2_COOKIE;
		hdr->nos_memdumps = 0;
		hdr->total_dump_sz = 0;
		hdr->dump_list_info_offset = 0;
		dump_config->iface_cfg.dump2mem_curr_addr =
			roundup(dump_config->iface_cfg.dump2mem_curr_addr +
				sizeof(memdump_hdr_t), ARCH_DMA_MINALIGN);
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_MEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	case DUMP_TO_NVMEM:
		ret = crashdump_flash_set_fn_ops(dump_config);
		if (ret) {
			printf("failed to set crashdump "
					"flash function ops ...\n");
			ret = CMD_RET_FAILURE;
			break;
		}

		snprintf(runcmd, sizeof(runcmd), "flerase %s",
				dump_config->iface_cfg.part_name);
		ret = run_command(runcmd, 0);
		if (ret) {
			ret = CMD_RET_FAILURE;
			break;
		}

		memdump_hdr_t *nvhdr = &dump_config->iface_cfg.memdump_hdr;
		nvhdr->magic1 = DUMP2MEM_MAGIC1_COOKIE;
		nvhdr->magic2 = DUMP2MEM_MAGIC2_COOKIE;
		nvhdr->nos_memdumps = 0;
		nvhdr->total_dump_sz = 0;
		nvhdr->dump_list_info_offset = 0;

		dump_config->iface_cfg.dump2mem_curr_addr =
			dump_config->iface_cfg.part_blksize;
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEM */

	case DUMP_TO_TFTP:
		printf("Trying to ping server.....\n");
		uint8_t retry = 3;
		snprintf(runcmd, sizeof(runcmd), "ping %s",
				dump_config->iface_cfg.tftp_serverip);
		while (retry--) {
			if (run_command(runcmd, 0) == CMD_RET_SUCCESS) {
				ping_status = 1;
				break;
			}

			mdelay(500);
		}

		if (ping_status != 1) {
			printf("Ping failed\n");
			ret = CMD_RET_FAILURE;
		}
		break;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	case DUMP_TO_FLASH:
		ret = crashdump_flash_set_fn_ops(dump_config);
		if (ret) {
			printf("failed to set crashdump "
					"flash function ops ...\n");
			ret = CMD_RET_FAILURE;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
	case DUMP_TO_EMMC:
		char *tmp = env_get("dump_to_emmc");
		struct disk_partition disk_info;
		int part_idx = 0;
		struct blk_desc *blk_dev = blk_get_devnum_by_uclass_id(
							UCLASS_MMC, 0);
		if (!blk_dev) {
			printf("No EMMC Device \n");
			ret = CMD_RET_FAILURE;
			break;
		}

		if (!tmp) {
			ret = CMD_RET_FAILURE;
			break;
		}

		part_idx = part_get_info_by_name(blk_dev, tmp, &disk_info);
		if (part_idx < 0) {
			printf(" %s Partition not found, ret %d !!!\n",
					tmp, ret);
			ret = CMD_RET_FAILURE;
			break;
		}

		snprintf(dump_config->iface_cfg.emmc_part_dev,
				sizeof(dump_config->iface_cfg.emmc_part_dev),
				"0:%x", part_idx);
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

	default:
		ret = CMD_RET_FAILURE;
		break;
	}

	return ret;
}

/**
 * split_bin_dump() - split the given dump into the requested split size
 * &dump_config - crashdump configuration info
 * &dump_entry - requested dump to split
 * &split_size - split size of the dumps
 * &dump_name_prefix - prefix name used for the output dumps
 */
static int split_bin_dump(crashdump_config_t *dump_config,
		crashdump_infos_int_t *dump_entry, uint64_t split_size,
		char dump_name_prefix[DUMP_NAME_STR_MAX_LEN],
		uint8_t file_no_start)
{
	int ret = 0;
	uint64_t dump_sz = dump_entry->size;
	uint64_t end_addr = dump_entry->start_addr + dump_sz;
	uint8_t file_no = (dump_sz / split_size) + file_no_start -
				((dump_sz % split_size) ? 0 : 1);

	while (dump_sz > 0) {
		snprintf(dump_entry->name, sizeof(dump_entry->name),
				"%s%d.BIN", dump_name_prefix, file_no--);
		if (dump_sz > split_size)
			dump_entry->size = split_size;
		else
			dump_entry->size = dump_sz;

		end_addr -= dump_entry->size;
		dump_sz -= dump_entry->size;
		dump_entry->start_addr = end_addr;
		ret = add_entry_crashdump_table(dump_config, dump_entry);
		if (ret)
			break;
	}

	return ret;
}

__weak bool is_valid_dump(char *dump_name)
{
	return false;
}

/**
 * prepare_crashdump_level_table() - prepare the crashdump table based on the
 * dump level, dump configuration and with respect to the given into dump
 * table.
 * &dump_config - crashdump configuration info
 * &dump_level - requested dump level
 */
static int prepare_crashdump_level_table(crashdump_config_t *dump_config,
		uint8_t dump_level)
{
	int i, ret = 0;
	crashdump_infos_int_t dump_entry;
	crashdump_infos_t *dump_infos = dump_config->dump_infos;
	char dump_name_prefix[DUMP_NAME_STR_MAX_LEN] = { 0 };
	uint64_t split_bin_sz = 0;
	uint8_t file_no = 0;
#if (CONFIG_NR_DRAM_BANKS > 1)
	uint64_t total_dram_sz = gd->ram_size;
	uint8_t bidx = 0, last_dram_file_no = 0;
#endif

	for (i=0; i < dump_config->nos_dumps; i++) {
		if (dump_level != dump_infos[i].dump_level)
			continue;

		if(dump_infos[i].check_dump_support) {
			if(!is_valid_dump(dump_infos[i].name)) {
				printf("Skipping %s dump\n",
						dump_infos[i].name);
				continue;
			}
		}

		file_no = 0;
		memset(&dump_entry, 0, sizeof(crashdump_infos_int_t));
		memcpy(&dump_name_prefix, dump_infos[i].name,
				(strlen(dump_infos[i].name) - 4));
		dump_name_prefix[(strlen(dump_infos[i].name) - 4)] = '\0';

		strlcpy(dump_entry.name, dump_infos[i].name,
				DUMP_NAME_STR_MAX_LEN);
		dump_entry.start_addr = dump_infos[i].start_addr;
		dump_entry.size = dump_infos[i].size;
		dump_entry.is_aligned_access = dump_infos[i].is_aligned_access;
		split_bin_sz = dump_infos[i].split_bin_sz;
		dump_entry.dumptoflash_support =
					dump_infos[i].dumptoflash_support;

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
		if (dump_config->is_compress_enabled)
			dump_entry.compression_support =
				dump_infos[i].compression_support;
#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */

		if (dump_infos[i].size == 0xBAD0FF5E) {
			switch (dump_level) {
#ifdef CONFIG_IPQ_MINIDUMP
			case MINIDUMP:
				ret = wdt_extract_dump(dump_config,
						i, &dump_entry);
				if (ret == -EINVAL) {
					ret = 0;
					continue;
				} else if (ret)
					return ret;
				break;
#endif /* CONFIG_IPQ_MINIDUMP */
			case FULLDUMP:
				/* DDR dump */
				if (!strncmp(dump_infos[i].name,
						DRAM_DUMP_NAME_PREFIX,
						strlen(DRAM_DUMP_NAME_PREFIX)))
				{
#if (CONFIG_NR_DRAM_BANKS > 1)
					file_no = last_dram_file_no;
					if (!total_dram_sz)
						continue;
					snprintf(dump_entry.name,
						sizeof(dump_entry.name),
						"%s%d.BIN", dump_name_prefix,
						file_no);
					dump_entry.start_addr =
						gd->bd->bi_dram[bidx].start;
					dump_entry.size = min(total_dram_sz,
						gd->bd->bi_dram[bidx].size);
					bidx++;
					total_dram_sz -= dump_entry.size;
#else
					snprintf(dump_entry.name,
						sizeof(dump_entry.name),
						"%s0.BIN", dump_name_prefix);
					dump_entry.size = gd->ram_size;
#endif
				}
				break;
			}
		}

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
		if (dump_entry.compression_support) {
			if (dump_entry.size > TFTP_MAX_TRF_SZ_LIMIT)
				split_bin_sz = TFTP_MAX_TRF_SZ_LIMIT;
			else {
				if (gd->ram_size == dump_entry.size)
					split_bin_sz = (gd->ram_size / 2);
			}
		}
#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */

#if (CONFIG_NR_DRAM_BANKS > 1)
		if (!strncmp(dump_infos[i].name, DRAM_DUMP_NAME_PREFIX,
					strlen(DRAM_DUMP_NAME_PREFIX)))	{
			if (split_bin_sz && (dump_entry.size > split_bin_sz))
				last_dram_file_no =
					(dump_entry.size / split_bin_sz);
			else
				last_dram_file_no++;
		}
#endif

		if (split_bin_sz && (dump_entry.size > split_bin_sz)) {
			split_bin_dump(dump_config, &dump_entry,
					split_bin_sz, dump_name_prefix,
					file_no);
			continue;
		}

		ret = add_entry_crashdump_table(dump_config, &dump_entry);
		if (ret)
			break;
	}

	return ret;
}

/**
 * prepare_crashdump_table() - prepare crashdump table top level
 */
static int prepare_crashdump_table(crashdump_config_t *dump_config)
{
	int ret = 0;
	dump_config->actual_nos_dumps = 0;
	if (dump_config->debug)
		printf("%-20s\t %-10s\t %-10s\t %-10s\t %-10s\n",
			"Name", "Address", "Size", "Alignment", "Compressed");

	switch (dump_config->dump_level) {
#ifdef CONFIG_IPQ_MINIDUMP
	case MINIDUMP_AND_FULLDUMP:
		ret = prepare_crashdump_level_table(dump_config, FULLDUMP);
	case MINIDUMP:
		ret = prepare_crashdump_level_table(dump_config, MINIDUMP);
		break;
#endif /* CONFIG_IPQ_MINIDUMP */
	case FULLDUMP:
		ret = prepare_crashdump_level_table(dump_config, FULLDUMP);
		break;
	}

	return ret;
}

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
#ifdef CONFIG_IPQ_NAND
/*
* NAND flash check and write. Before writing into the nand flash
* this function checks if the block is non-bad, and skips if bad. While
* skipping, there is also possiblity of crossing the partition and corrupting
* next partition with crashdump data. So this function also checks whether
* offset is within the partition, where the configured offset belongs.
*
* Returns 0 on succes and 1 otherwise
*/
static int check_and_write_crashdump_nand_flash(
			struct crashdump_flash_nand_cxt *nand_cnxt,
			struct mtd_info * nand, unsigned char *data,
			unsigned int req_size)
{
	nand_erase_options_t nand_erase_options;
	uint32_t part_start = nand_cnxt->part_start;
	uint32_t part_end = nand_cnxt->part_start + nand_cnxt->part_size;
	size_t remaining_len = req_size;
	size_t write_length, data_offset = 0;
	uint64_t skipoff, skipoff_cmp, *offset;
	int ret = 0;
	static int first_erase = 1;

	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return -ENODEV;

	offset = &nand_cnxt->cur_crashdump_offset;
	memset(&nand_erase_options, 0, sizeof(nand_erase_options));
	nand_erase_options.length = nand->erasesize;

	while (remaining_len) {
		skipoff = *offset - (*offset & (nand->erasesize - 1));
		skipoff_cmp = skipoff;

		for (; skipoff < part_end; skipoff += nand->erasesize) {
			if (nand_block_isbad(nand, skipoff)) {
				printf("Skipping bad block at 0x%llx\n",
					skipoff);
				continue;
			} else
				break;
		}

		if (skipoff_cmp != skipoff)
			*offset = skipoff;

		if ((part_start > *offset) ||
				 ((*offset + remaining_len) >= part_end)) {
			printf("Failure: Attempt to write in next partition\n");
			return 1;
		}

		if ((*offset & (nand->erasesize - 1)) == 0 || first_erase) {
			nand_erase_options.offset = *offset;

			ret = nand_erase_opts(mtd, &nand_erase_options);
			if (ret)
				return ret;
			first_erase = 0;
		}

		if (remaining_len > nand->erasesize) {

			skipoff = (*offset & (nand->erasesize - 1));
			write_length = (skipoff != 0) ?
					(nand->erasesize - skipoff) :
					(nand->erasesize);
			ret = nand_write(nand, *offset, &write_length,
				data + data_offset);
			if (ret)
				return ret;

			remaining_len -= write_length;
			*offset += write_length;
			data_offset += write_length;
		} else {

			ret = nand_write(nand, *offset, &remaining_len,
				data + data_offset);

			*offset += remaining_len;
			remaining_len = 0;
		}
	}
	return ret;
}

/*
* Init function for NAND flash writing. It intializes its own context
* and erases the required sectors
*/
int init_crashdump_nand_flash_write(void *cnxt, uint64_t offset, uint32_t size)
{
	struct crashdump_flash_nand_cxt *nand_cnxt = cnxt;
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	int ret;

	if (!mtd)
		return -ENODEV;

#ifdef CONFIG_NOR_BLK
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		ret = ipq_gpt_getpart_from_offset(offset,
				&nand_cnxt->part_start,&nand_cnxt->part_size,
				sfi->flash_type);
	} else
#endif /* CONFIG_NOR_BLK */
	{
		ret = smem_getpart_from_offset(offset, &nand_cnxt->part_start,
				&nand_cnxt->part_size);
	}
	if (ret) {
		printf("smem_getpart_from_offset failed\n");
		return ret;
	}

	nand_cnxt->cur_crashdump_offset = offset;
	nand_cnxt->cur_page_data_len = 0;
	nand_cnxt->write_size = mtd->writesize;

	nand_cnxt->temp_data = malloc_cache_aligned(nand_cnxt->write_size);
	if (!nand_cnxt->temp_data)
		return -ENOMEM;

	return 0;
}

/*
* Deinit function for NAND flash writing. It writes the remaining data
* stored in temp buffer to NAND.
*/
int deinit_crashdump_nand_flash_write(void *cnxt)
{
	int ret = 0;
	struct crashdump_flash_nand_cxt *nand_cnxt = cnxt;
	uint32_t cur_nand_write_len = nand_cnxt->cur_page_data_len;
	int remaining_bytes = nand_cnxt->write_size -
			nand_cnxt->cur_page_data_len;

	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return -ENODEV;

	if (cur_nand_write_len) {
		/*
		* Make the write data in multiple of page write size
		* and write remaining data in NAND flash
		*/
		memset(nand_cnxt->temp_data + nand_cnxt->cur_page_data_len,
			0xFF, remaining_bytes);

		cur_nand_write_len = nand_cnxt->write_size;

		ret = check_and_write_crashdump_nand_flash(nand_cnxt,
					mtd, nand_cnxt->temp_data,
					cur_nand_write_len);

	}

	if (nand_cnxt->temp_data) {
		free(nand_cnxt->temp_data);
		nand_cnxt->temp_data = NULL;
	}
	return ret;
}

/*
* Write function for NAND flash. NAND writing works on page basis so
* this function writes the data in mulitple of page size and stores the
* remaining data in temp buffer. This temp buffer data will be appended
* with next write data.
*/
int crashdump_nand_flash_write_data(void *cnxt, uint8_t *data, uint32_t size)
{
	int ret;
	struct crashdump_flash_nand_cxt *nand_cnxt = cnxt;
	uint8_t *cur_data_pos = data;
	uint32_t remaining_bytes;
	uint32_t total_bytes;
	uint32_t cur_nand_write_len;
	uint32_t remaining_len_cur_page;
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!mtd)
		return -ENODEV;

	remaining_bytes = total_bytes = nand_cnxt->cur_page_data_len + size;

	/*
	* Check for minimum write size and store the data in temp buffer if
	* the total size is less than it
	*/
	if (total_bytes < nand_cnxt->write_size) {
		memcpy(nand_cnxt->temp_data + nand_cnxt->cur_page_data_len,
					data, size);
		nand_cnxt->cur_page_data_len += size;

		return 0;
	}

	/*
	* Append the remaining length of data for complete nand page write in
	* currently stored data and do the nand write
	*/
	remaining_len_cur_page = nand_cnxt->write_size -
			nand_cnxt->cur_page_data_len;
	cur_nand_write_len = nand_cnxt->write_size;

	memcpy(nand_cnxt->temp_data + nand_cnxt->cur_page_data_len, data,
			remaining_len_cur_page);

	ret = check_and_write_crashdump_nand_flash(nand_cnxt,
					mtd, nand_cnxt->temp_data,
					cur_nand_write_len);

	if (ret)
		return ret;

	cur_data_pos += remaining_len_cur_page;

	/*
	* Calculate the write length in multiple of page length and do the nand
	* write for same length
	*/
	cur_nand_write_len = ((data + size - cur_data_pos) /
				nand_cnxt->write_size) * nand_cnxt->write_size;

	if (cur_nand_write_len > 0) {
		ret = check_and_write_crashdump_nand_flash(nand_cnxt,
						mtd, cur_data_pos,
						cur_nand_write_len);

		if (ret)
			return ret;

	}

	cur_data_pos += cur_nand_write_len;

	/* Store the remaining data in temp data */
	remaining_bytes = data + size - cur_data_pos;

	memcpy(nand_cnxt->temp_data, cur_data_pos, remaining_bytes);

	nand_cnxt->cur_page_data_len = remaining_bytes;

	return 0;
}
#endif

#ifdef CONFIG_IPQ_SPI_NOR
/* Init function for SPI NOR flash writing. It erases the required sectors */
int init_crashdump_spi_flash_write(void *cnxt, uint64_t offset, uint32_t size)
{
	int ret;
	struct crashdump_flash_spi_cxt *spi_flash_cnxt = cnxt;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	spi_flash_cnxt->cur_crashdump_offset = offset;
	ret = spi_flash_erase(spi_flash_cnxt->crashdump_spi_flash, offset,
				roundup(size, sfi->flash_block_size));

	return ret;
}

/* Write function for SPI NOR flash */
int crashdump_spi_flash_write_data(void *cnxt, uint8_t *data, uint32_t size)
{
	int ret;
	struct crashdump_flash_spi_cxt *spi_flash_cnxt = cnxt;

	ret = spi_flash_write(spi_flash_cnxt->crashdump_spi_flash,
			spi_flash_cnxt->cur_crashdump_offset, size, data);
	if (!ret)
		spi_flash_cnxt->cur_crashdump_offset += size;

	return ret;
}

/* Deinit function for SPI NOR flash writing. */
int deinit_crashdump_spi_flash_write(void *cnxt)
{
	return 0;
}
#endif

#ifdef CONFIG_IPQ_MMC
/*
* Init function for EMMC flash writing. It initialzes its
* own context and EMMC
*/
int init_crashdump_emmc_flash_write(void *cnxt, uint64_t offset, uint32_t size)
{
	struct crashdump_flash_emmc_cxt *emmc_cnxt = cnxt;
	emmc_cnxt->cur_crashdump_offset = offset;
	emmc_cnxt->cur_blk_data_len = 0;
	emmc_cnxt->write_size =  emmc_cnxt->mmc->write_bl_len;

	emmc_cnxt->temp_data = malloc_cache_aligned(
					emmc_cnxt->mmc->write_bl_len);
	if (!emmc_cnxt->temp_data)
		return -ENOMEM;

	return 0;
}

/*
* Deinit function for EMMC flash writing. It writes the remaining data
* stored in temp buffer to EMMC
*/
int deinit_crashdump_emmc_flash_write(void *cnxt)
{
	struct crashdump_flash_emmc_cxt *emmc_cnxt = cnxt;
	uint32_t cur_blk_write_len = emmc_cnxt->cur_blk_data_len;
	int ret = 0;
	int n;
	uint32_t remaining_bytes = emmc_cnxt->write_size -
			emmc_cnxt->cur_blk_data_len;

	if (cur_blk_write_len) {
		/*
		* Make the write data in multiple of block length size
		* and write remaining data in emmc
		*/
		memset(emmc_cnxt->temp_data + emmc_cnxt->cur_blk_data_len,
			0xFF, remaining_bytes);

		cur_blk_write_len = emmc_cnxt->write_size;
#ifdef CONFIG_BLK
		n = blk_dwrite(emmc_cnxt->desc,
				emmc_cnxt->cur_crashdump_offset,
				1,
				(uint8_t *)emmc_cnxt->temp_data);
#else
		n = emmc_cnxt->mmc->block_dev.block_write(
					&emmc_cnxt->mmc->block_dev,
					emmc_cnxt->cur_crashdump_offset,
					1,
					(uint8_t *)emmc_cnxt->temp_data);
#endif

		ret = (n == 1) ? 0 : -ENOMEM;
	}

	if(emmc_cnxt->temp_data) {
		free(emmc_cnxt->temp_data);
		emmc_cnxt->temp_data = NULL;
	}

	return ret;
}

/*
* Write function for EMMC flash. EMMC writing works on block basis so
* this function writes the data in mulitple of block length and stores
* remaining data in temp buffer. This temp buffer data will be appended
* with next write data.
*/
int crashdump_emmc_flash_write_data(void *cnxt, uint8_t *data, uint32_t size)
{
	struct crashdump_flash_emmc_cxt *emmc_cnxt = cnxt;
	uint8_t *cur_data_pos = data;
	uint32_t remaining_bytes;
	uint32_t total_bytes;
	uint32_t cur_emmc_write_len;
	uint32_t cur_emmc_blk_len;
	uint32_t remaining_len_cur_page;
	int ret, n;

	remaining_bytes = total_bytes = emmc_cnxt->cur_blk_data_len + size;

	/*
	* Check for block size and store the data in temp buffer if
	* the total size is less than it
	*/
	if (total_bytes < emmc_cnxt->write_size) {
		memcpy(emmc_cnxt->temp_data + emmc_cnxt->cur_blk_data_len,
				data, size);
		emmc_cnxt->cur_blk_data_len += size;

		return 0;
	}

	/*
	* Append the remaining length of data for complete emmc block write in
	* currently stored data and do the block write
	*/
	remaining_len_cur_page = emmc_cnxt->write_size -
			emmc_cnxt->cur_blk_data_len;
	cur_emmc_write_len = emmc_cnxt->write_size;

	memcpy(emmc_cnxt->temp_data + emmc_cnxt->cur_blk_data_len, data,
			remaining_len_cur_page);
#ifdef CONFIG_BLK
	n = blk_dwrite(emmc_cnxt->desc, emmc_cnxt->cur_crashdump_offset,
					1,
					(uint8_t *)emmc_cnxt->temp_data);
#else
	n = emmc_cnxt->mmc->block_dev.block_write(&emmc_cnxt->mmc->block_dev,
					emmc_cnxt->cur_crashdump_offset,
					1,
					(uint8_t *)emmc_cnxt->temp_data);
#endif

	ret = (n == 1) ? 0 : -ENOMEM;
	if (ret)
		return ret;

	cur_data_pos += remaining_len_cur_page;
	emmc_cnxt->cur_crashdump_offset += 1;
	/*
	* Calculate the write length in multiple of block length and do the
	* emmc block write for same length
	*/
	cur_emmc_blk_len = ((data + size - cur_data_pos) /
				emmc_cnxt->write_size);
	cur_emmc_write_len = cur_emmc_blk_len * emmc_cnxt->write_size;

	if (cur_emmc_write_len > 0) {
#ifdef CONFIG_BLK
		n = blk_dwrite(emmc_cnxt->desc,
				emmc_cnxt->cur_crashdump_offset,
				cur_emmc_blk_len,
				(uint8_t *)cur_data_pos);
#else
		n = emmc_cnxt->mmc->block_dev.block_write(
					&emmc_cnxt->mmc->block_dev,
					emmc_cnxt->cur_crashdump_offset,
					cur_emmc_blk_len,
					(uint8_t *)cur_data_pos);
#endif
		ret = (n == cur_emmc_blk_len) ? 0 : -1;
		if (ret)
			return ret;
	}

	cur_data_pos += cur_emmc_write_len;
	emmc_cnxt->cur_crashdump_offset += cur_emmc_blk_len;

	/* Store the remaining data in temp data */
	remaining_bytes = data + size - cur_data_pos;
	memcpy(emmc_cnxt->temp_data, cur_data_pos, remaining_bytes);
	emmc_cnxt->cur_blk_data_len = remaining_bytes;

	return 0;
}
#endif

static int crashdump_flash_get_args(uint8_t *flash_type, uint64_t *offset)
{
	char *cmd, *crashdump_offset, *fltype;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	int ret = 0;

	cmd = env_get("dump_to_flash");
	if (cmd == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	crashdump_offset = strsep(&cmd, " ");
	if (!offset) {
		ret = -EINVAL;
		goto exit;
	}
	*offset = simple_strtoul(crashdump_offset, NULL, 16);

	*flash_type = sfi->flash_type;
	fltype = strsep(&cmd, " ");
	if (fltype) {
		*flash_type = 0;
#ifdef CONFIG_IPQ_NAND
		if (!strncmp(fltype, "NAND", sizeof("NAND")))
			*flash_type = SMEM_BOOT_NAND_FLASH;
#endif
#ifdef CONFIG_IPQ_MMC
		if (!strncmp(fltype, "EMMC", sizeof("EMMC")))
			*flash_type = SMEM_BOOT_MMC_FLASH;
#endif
		if (!*flash_type) {
			printf("Invalid flash type [NAND / EMMC] ...\n");
			return -EINVAL;
		}
	}

	if (*flash_type != SMEM_BOOT_MMC_FLASH) {
		if (*offset % sfi->flash_block_size) {
			printf("crashdump offset is not multiple of "
				"erase size\n");
			return -EINVAL;
		}
	}
exit:
	return ret;
}

static int crashdump_flash_set_fn_ops(crashdump_config_t *dump_config)
{
	int ret = 0;
	uint8_t flash_type = dump_config->iface_cfg.flash_type;
	void *crashdump_cnxt = NULL;

	/*
	 * Determine the flash type and initialize function pointer for flash
	 * operations and its context which needs to be passed to these
	 * functions.
	 */
	if (((flash_type == SMEM_BOOT_NAND_FLASH) ||
		(flash_type == SMEM_BOOT_QSPI_NAND_FLASH))) {
#ifdef CONFIG_IPQ_NAND
		crashdump_cnxt = (void *)&crashdump_nand_cnxt;
		crashdump_flash_write_init = init_crashdump_nand_flash_write;
		crashdump_flash_write = crashdump_nand_flash_write_data;
		crashdump_flash_write_deinit =
			deinit_crashdump_nand_flash_write;
#endif
#ifdef CONFIG_IPQ_SPI_NOR
	} else if ((flash_type == SMEM_BOOT_SPI_FLASH) ||
			(flash_type == SMEM_BOOT_NORGPT_FLASH)) {
		if (!crashdump_flash_spi_cnxt.crashdump_spi_flash) {
			crashdump_flash_spi_cnxt.crashdump_spi_flash =
					spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
						CONFIG_SF_DEFAULT_CS,
						CONFIG_SF_DEFAULT_SPEED,
						CONFIG_SF_DEFAULT_MODE);

			if (!crashdump_flash_spi_cnxt.crashdump_spi_flash) {
				printf("spi_flash_probe() failed\n");
				ret = -EIO;
				goto exit;
			}
		}

		crashdump_cnxt = (void *)&crashdump_flash_spi_cnxt;
		crashdump_flash_write = crashdump_spi_flash_write_data;
		crashdump_flash_write_init = init_crashdump_spi_flash_write;
		crashdump_flash_write_deinit =
			deinit_crashdump_spi_flash_write;
#endif
#ifdef CONFIG_IPQ_MMC
	} else if (flash_type == SMEM_BOOT_MMC_FLASH) {
		crashdump_emmc_cnxt.mmc = find_mmc_device(0);
		if (!crashdump_emmc_cnxt.mmc) {
			printf("no mmc device at slot 0\n");
			ret = -ENODEV;
			goto exit;
		}

		crashdump_emmc_cnxt.desc = mmc_get_blk_desc(
						crashdump_emmc_cnxt.mmc);
		if (!crashdump_emmc_cnxt.desc) {
			printf("Failed to find the desc\n");
			ret = -ENXIO;
			goto exit;
		}

		crashdump_cnxt = (void *)&crashdump_emmc_cnxt;
		crashdump_flash_write_init = init_crashdump_emmc_flash_write;
		crashdump_flash_write = crashdump_emmc_flash_write_data;
		crashdump_flash_write_deinit =
			deinit_crashdump_emmc_flash_write;
#endif
	} else {
		return -EINVAL;
	}

	dump_config->iface_cfg.crashdump_cnxt = crashdump_cnxt;
exit:
	return ret;
}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

/**
 * dump_to_dst() - Do the actual dumping based on the requested dump entry
 * &dump_config - crashdump configuration info
 * &dump_entry - requested dump entry
 */
static int dump_to_dst(crashdump_config_t *dump_config,
			crashdump_infos_int_t *dump_entry)
{
	char runcmd[256] = { 0 };
	crashdump_interface_cfg_t *iface_cfg = &dump_config->iface_cfg;

	if (dump_entry->is_aligned_access) {
		long unsigned int aligned_addr = (dump_config->ram_top -
				roundup(dump_entry->size, ARCH_DMA_MINALIGN));
		memcpy((void*)aligned_addr,
				(void*)(uintptr_t)dump_entry->start_addr,
				dump_entry->size);
		dump_entry->start_addr = aligned_addr;
	}

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
	if (dump_entry->compression_support) {
		phys_addr_t compressed_out_sz;

		if ((dump_entry->start_addr + dump_entry->size) ==
				dump_config->ram_top)
		{
			iface_cfg->comp_out_addr = dump_entry->start_addr
				- TEMP_COMPRESS_BUF_SZ;
			iface_cfg->comp_out_size = dump_entry->size;

			printf("Backing up temporary compress buffer\n");
			switch (dump_config->dump_to) {
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
			case DUMP_TO_USB:
				snprintf(runcmd, sizeof(runcmd),
					"fatwrite usb %x:%x 0x%llx %s 0x%x",
					iface_cfg->usb_dev_idx,
					iface_cfg->usb_part_idx,
					iface_cfg->comp_out_addr,
					TEMP_COMPRESS_BUF_NAME,
					TEMP_COMPRESS_BUF_SZ);
				break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

			case DUMP_TO_TFTP:
				snprintf(runcmd, sizeof(runcmd),
					"tftpput 0x%llx 0x%x %s/%s",
					iface_cfg->comp_out_addr,
					TEMP_COMPRESS_BUF_SZ,
					iface_cfg->tftp_dumpdir,
					TEMP_COMPRESS_BUF_NAME);
				break;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
			case DUMP_TO_EMMC:
				loff_t len;

				if (fs_set_blk_dev("mmc",
						iface_cfg->emmc_part_dev,
						FS_TYPE_EXT))
				{
					printf("failed to set block device "
							"to mmc\n");
					return CMD_RET_FAILURE;
				}

				if (fs_write("/EBICS0.BIN.gz",
						iface_cfg->comp_out_addr,
						0, SZ_16M, &len) < 0)
				{
					printf("failed to write temp "
							"compress buffer\n");
					return CMD_RET_FAILURE;
				}

				printf("temp compress buffer written of size "
						"%llu bytes\n", len);
				break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

			default:
				break;
			}

			if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
				return CMD_RET_FAILURE;
		}

		if ((iface_cfg->comp_out_addr == 0) ||
				(iface_cfg->comp_out_size == 0)) {
			printf("No temporary compression out buffer \n");
			return CMD_RET_FAILURE;
		}

		printf("Compressing %s... ", dump_entry->name);
		if (!strncmp(dump_entry->name, "EBICS0.BIN",
					strlen("EBICS0.BIN"))) {
			memcpy((void*)(uintptr_t)iface_cfg->comp_out_addr,
				(void*)(uintptr_t)dump_entry->start_addr,
					dump_entry->size);
			dump_entry->start_addr = iface_cfg->comp_out_addr;
			iface_cfg->comp_out_addr = dump_entry->start_addr
				- TEMP_COMPRESS_BUF_SZ;
			iface_cfg->comp_out_size = dump_entry->size;
		}

		compressed_out_sz = iface_cfg->comp_out_size;
		if (gzip((void *)(uintptr_t) iface_cfg->comp_out_addr,
				(void*)(uintptr_t) &compressed_out_sz,
				(void*)(uintptr_t) dump_entry->start_addr,
				dump_entry->size) != 0) {
			printf("failed\n");
			return CMD_RET_FAILURE;
		}
		printf("done!!\n");

		dump_entry->start_addr = iface_cfg->comp_out_addr;
		dump_entry->size = compressed_out_sz;
		snprintf(dump_entry->name, DUMP_NAME_STR_MAX_LEN,
				"%s.gz", dump_entry->name);
	}
#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */

	switch (dump_config->dump_to) {
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
	case DUMP_TO_USB:
		printf("Writing file %s into USB \n", dump_entry->name);
		snprintf(runcmd, sizeof(runcmd),
				"fatwrite usb %x:%x 0x%llx %s 0x%llx",
				iface_cfg->usb_dev_idx,
				iface_cfg->usb_part_idx,
				dump_entry->start_addr, dump_entry->name,
				dump_entry->size);
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_MEMORY
	case DUMP_TO_MEM:
		memdump_hdr_t *hdr = &iface_cfg->memdump_hdr;
		int i = hdr->nos_memdumps++;
		memdump_list_info_t *list = &iface_cfg->memdump_list[i];
		strlcpy(list->name, dump_entry->name, DUMP_NAME_STR_MAX_LEN);
		list->offset = iface_cfg->dump2mem_curr_addr -
			iface_cfg->dump2mem_rsvd_addr;
		list->size = dump_entry->size;

		if (roundup(iface_cfg->dump2mem_curr_addr + dump_entry->size,
				ARCH_DMA_MINALIGN) >
				iface_cfg->dump2mem_rsvd_limit) {
			printf("Error: Not enough memory in rsvd mem" \
				       " to save dumps\n");
			return CMD_RET_FAILURE;
		}

		printf("Dumping %s @ 0x%llX \n", dump_entry->name,
				iface_cfg->dump2mem_curr_addr);
		memcpy((void*)(uintptr_t)iface_cfg->dump2mem_curr_addr,
				(void*)(uintptr_t)dump_entry->start_addr,
				dump_entry->size);

		iface_cfg->dump2mem_curr_addr = roundup(
				iface_cfg->dump2mem_curr_addr +
				dump_entry->size, ARCH_DMA_MINALIGN);
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_MEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	case DUMP_TO_NVMEM:
		memdump_hdr_t *nvhdr = &iface_cfg->memdump_hdr;
		int nvidx = nvhdr->nos_memdumps++;
		memdump_list_info_t *nvlist = &iface_cfg->memdump_list[nvidx];
		strlcpy(nvlist->name, dump_entry->name, DUMP_NAME_STR_MAX_LEN);
		nvlist->offset = iface_cfg->dump2mem_curr_addr;
		nvlist->size = dump_entry->size;

		if (iface_cfg->dump2mem_curr_addr + dump_entry->size >
				iface_cfg->part_size) {
			printf("Error: Not enough memory in %s partition" \
					" to save dumps\n",
					iface_cfg->part_name);
			return CMD_RET_FAILURE;
		}

		printf("Writing %s in %s @ offset 0x%llx\n", dump_entry->name,
				iface_cfg->part_name,
				iface_cfg->dump2mem_curr_addr);
		if (crashdump_flash_write(iface_cfg->crashdump_cnxt,
						(void*)(uintptr_t)
						dump_entry->start_addr,
						dump_entry->size)) {
			printf("crashdump data writing in flash failure\n");
			return CMD_RET_FAILURE;
		}

		iface_cfg->dump2mem_curr_addr += dump_entry->size;
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

	case DUMP_TO_TFTP:
		snprintf(runcmd, sizeof(runcmd), "tftpput 0x%llx 0x%llx %s/%s",
				dump_entry->start_addr, dump_entry->size,
				iface_cfg->tftp_dumpdir, dump_entry->name);
		break;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	case DUMP_TO_FLASH:
		printf("Writing %s into FLASH \n", dump_entry->name);
		if (crashdump_flash_write(iface_cfg->crashdump_cnxt,
						(void*)(uintptr_t)
						dump_entry->start_addr,
						dump_entry->size)) {
			printf("crashdump data writing in flash failure\n");
			return CMD_RET_FAILURE;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
	case DUMP_TO_EMMC:
		loff_t len;
		int ret;
		char abs_file_path[DUMP_NAME_STR_MAX_LEN+1];
		snprintf(abs_file_path, DUMP_NAME_STR_MAX_LEN+1, "/%s",
				dump_entry->name);

		if (fs_set_blk_dev("mmc", iface_cfg->emmc_part_dev,
					FS_TYPE_EXT)) {
			printf("failed to set block device to mmc\n");
			return CMD_RET_FAILURE;
		}

		printf("Writing %s into MMC \n", dump_entry->name);
		ret = fs_write(abs_file_path, dump_entry->start_addr, 0,
					dump_entry->size, &len);
		if (ret < 0) {
			printf("failed to write %s file, error : %d\n",
					dump_entry->name, ret);
			return CMD_RET_FAILURE;
		}

		printf("%s written of size %llu bytes\n",
				dump_entry->name, len);
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

	}

	if (runcmd[0] != 0) {
		if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
			return CMD_RET_FAILURE;
	}

#ifdef CONFIG_IPQ_COMPRESSED_CRASHDUMP
	if (dump_entry->compression_support) {
		if (iface_cfg->comp_out_addr == dump_entry->start_addr)
		{
			iface_cfg->comp_out_addr += TEMP_COMPRESS_BUF_SZ;

			if (strncmp(dump_entry->name, "EBICS0.BIN",
						strlen("EBICS0.BIN")))
			{
				printf("Load back temporary compress buffer\n");
				switch (dump_config->dump_to) {
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
				case DUMP_TO_USB:
					snprintf(runcmd, sizeof(runcmd),
						"fatload usb %x:%x 0x%llx %s",
						iface_cfg->usb_dev_idx,
						iface_cfg->usb_part_idx,
						dump_entry->start_addr,
						TEMP_COMPRESS_BUF_NAME);
					break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

				case DUMP_TO_TFTP:
					snprintf(runcmd, sizeof(runcmd),
						"tftpboot 0x%llx %s/%s",
						dump_entry->start_addr,
						iface_cfg->tftp_dumpdir,
						TEMP_COMPRESS_BUF_NAME);
					break;

#ifdef CONFIG_IPQ_CRASHDUMP_TO_EMMC
				case DUMP_TO_EMMC:
					loff_t len;

					if (fs_set_blk_dev("mmc",
						iface_cfg->emmc_part_dev,
						FS_TYPE_EXT))
					{
						printf("failed to set block "
							"device to mmc\n");
						return CMD_RET_FAILURE;
					}


					if (fs_read("/EBICS0.BIN.gz",
						dump_entry->start_addr,
						0, 0, &len) < 0)
					{
						printf("failed to load temp "
							"compress buffer\n");
						return CMD_RET_FAILURE;
					}

					printf("temp compress buffer loaded "
							"of size %llu bytes\n",
							len);
					break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_EMMC */

				default:
					break;
				}

				if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
					return CMD_RET_FAILURE;
			}
		}
	}
#endif /* CONFIG_IPQ_COMPRESSED_CRASHDUMP */

	return CMD_RET_SUCCESS;
}

/**
 * ipq_do_dump_data() - call dump_to_dst based on the crashdump table
 * &dump_config - crashdump configuration info
 */
void ipq_do_dump_data(crashdump_config_t *dump_config)
{
	int ret = CMD_RET_SUCCESS;
	crashdump_infos_int_t *dump_entry;
	crashdump_interface_cfg_t *iface_cfg = &dump_config->iface_cfg;
	uint16_t dumped = 0;

#if defined(CONFIG_IPQ_CRASHDUMP_TO_MEMORY) || \
	defined(CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY)
	if ((dump_config->dump_to == DUMP_TO_MEM) ||
			(dump_config->dump_to == DUMP_TO_NVMEM)) {
		iface_cfg->memdump_list = malloc(sizeof(memdump_list_info_t) *
					dump_config->actual_nos_dumps);
		if (!iface_cfg->memdump_list) {
			printf("failed to alloc mem for memdump_list_info\n");
			return;
		}
	}
#endif
/* CONFIG_IPQ_CRASHDUMP_TO_MEMORY (or) CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	if (dump_config->dump_to == DUMP_TO_NVMEM) {
		ret = crashdump_flash_write_init(
			iface_cfg->crashdump_cnxt,
			iface_cfg->crashdump_offset +
			iface_cfg->init_dump_off, 0);
		if (ret) {
			printf("crashdump flash write init failed ...\n");
			return;
		}
	}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	if (dump_config->dump_to == DUMP_TO_FLASH) {
		ret = crashdump_flash_write_init(
			iface_cfg->crashdump_cnxt,
			iface_cfg->crashdump_offset,
			iface_cfg->dump_total_size);
		if (ret) {
			printf("crashdump flash write init failed ...\n");
			return;
		}
	}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */

	list_for_each_entry(dump_entry, &actual_dumps_list, list) {
		printf("Processing %s:\n", dump_entry->name);
		ret = dump_to_dst(dump_config, dump_entry);
		if (ret == CMD_RET_FAILURE)
			break;

		dumped++;
	}

	if (!ret)
		printf("Dumped %d files!!\n", dumped);

	switch (dump_config->dump_to) {
#ifdef CONFIG_IPQ_CRASHDUMP_TO_USB
	case DUMP_TO_USB:
		mdelay(10);
		usb_stop();
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_USB */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_MEMORY
	case DUMP_TO_MEM:
		memdump_hdr_t *hdr = &iface_cfg->memdump_hdr;
		if (ret)
			break;

		hdr->total_dump_sz = iface_cfg->dump2mem_curr_addr +
			(hdr->nos_memdumps * sizeof(memdump_list_info_t)) -
			iface_cfg->dump2mem_rsvd_addr;

		if ((iface_cfg->dump2mem_curr_addr + hdr->total_dump_sz)
				> iface_cfg->dump2mem_rsvd_limit) {
			printf("Error: Not enough memory in rsvd mem" \
				       " to save dumps\n");
			break;
		}

		memcpy((void*)(uintptr_t)iface_cfg->dump2mem_curr_addr,
				(void*)dump_config->iface_cfg.memdump_list,
				sizeof(memdump_list_info_t) *
				dump_config->actual_nos_dumps);
		free(dump_config->iface_cfg.memdump_list);

		hdr->dump_list_info_offset = iface_cfg->dump2mem_curr_addr -
			iface_cfg->dump2mem_rsvd_addr;

		memcpy((void*)(uintptr_t)iface_cfg->dump2mem_rsvd_addr,
				hdr, sizeof(memdump_hdr_t));
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_MEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	case DUMP_TO_NVMEM:
		memdump_hdr_t *nvhdr = &iface_cfg->memdump_hdr;
		if (ret)
			break;

		ret = crashdump_flash_write(iface_cfg->crashdump_cnxt,
				(void*)iface_cfg->memdump_list,
				sizeof(memdump_list_info_t) *
				dump_config->actual_nos_dumps);
		if (ret) {
			printf("crashdump data writing in flash failure\n");
			return;
		}

		if (crashdump_flash_write_deinit(iface_cfg->crashdump_cnxt)) {
			printf("crashdump data deinit in flash failure\n");
			return;
		}

		nvhdr->dump_list_info_offset = iface_cfg->dump2mem_curr_addr;
		nvhdr->total_dump_sz = iface_cfg->dump2mem_curr_addr +
			(nvhdr->nos_memdumps * sizeof(memdump_list_info_t));

		if (nvhdr->total_dump_sz > iface_cfg->part_size) {
			printf("Error: Not enough memory in %s partition" \
				       " to save dumps", iface_cfg->part_name);
			return;
		}

		/* updating memdump_hdr in flash at the first block */
		ret = crashdump_flash_write_init(iface_cfg->crashdump_cnxt,
				iface_cfg->crashdump_offset, 0);
		if (ret) {
			printf("crashdump data init in flash failure\n");
			return;
		}

		ret = crashdump_flash_write(iface_cfg->crashdump_cnxt,
				(uint8_t*)nvhdr, sizeof(memdump_hdr_t));
		if (ret) {
			printf("crashdump data writing in flash failure\n");
			return;
		}

		ret = crashdump_flash_write_deinit(iface_cfg->crashdump_cnxt);
		if (ret) {
			printf("crashdump data deinit in flash failure\n");
			return;
		}
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_IPQ_CRASHDUMP_TO_FLASH
	case DUMP_TO_FLASH:
		if (crashdump_flash_write_deinit(iface_cfg->crashdump_cnxt)) {
			printf("crashdump flash write deinit failed ...\n");
			return;
		}

		if (!ret && dumped)
			printf("crashdump data writing in flash successful\n");
		break;
#endif /* CONFIG_IPQ_CRASHDUMP_TO_FLASH */
	}

	return;
}

/**
 * ipq_dump_func() - Top level crashdump function used
 */
static void ipq_dump_func(crashdump_config_t *dump_config, uint8_t debug)
{
	uint64_t etime;
	int ret = CMD_RET_SUCCESS;
	bool skip_crashdump = 1;

	dump_config->debug = debug;
	dump_config->ram_top = ((gd->ram_top & BIT(0)) ? (gd->ram_top + 1) :
					gd->ram_top);
	parse_crashdump_config(dump_config);
	if (!dump_config->force_collect_dump) {
		etime = get_timer(0) + (10 * CONFIG_SYS_HZ);
		printf("\nHit any key within 10s to stop dump activity...");
		while (!tstc()) {       /* while no incoming data */
			if (get_timer(0) >= etime) {
				skip_crashdump = 0;
				printf("\n");
				break;
			}
		}
	} else
		skip_crashdump = 0;

	if (skip_crashdump) {
		printf("\nSkipping crashdump ... \n");
		goto reset;
	}

	ret = verify_crashdump_config(dump_config);
	if (ret == CMD_RET_FAILURE)
		goto reset;

	ret = verify_crashdump_iface(dump_config);
	if (ret == CMD_RET_FAILURE)
		goto reset;

	dump_config->dump_infos = board_dumpinfo;
	dump_config->nos_dumps = *board_dump_entries;

	/* Stage 1 - prepare internal dump table */
	ret = prepare_crashdump_table(dump_config);

	/* Stage 2 - dump bins as per the table */
	if (!ret)
		ipq_do_dump_data(dump_config);

	/* Stage 3 - delete internal dump table */
	delete_crashdump_table();

reset:
	reset();
	return;
}

/**
 * reset_crashdump() - clear crashdump magic in SDI path
 */
void reset_crashdump(int reset_version)
{
	int ret = -1;
	scm_param param;
	unsigned int cookie = ipq_read_tcsr_boot_misc();

	switch(reset_version) {
	case RESET_V1:

		do {
			ret = -ENOTSUPP;
			IPQ_SCM_ENABLE_SDI(param, 1, 0);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("Error in enabling SDI path\n");
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
		}

	case RESET_V2:
		if (cookie & DLOAD_ENABLE)
			cookie |= CRASHDUMP_RESET;
#ifdef CONFIG_FAILSAFE
		else if (!ipq_iscrashed_crashdump_disabled())
			cookie &= ~MARK_UBOOT_MILESTONE;
#endif

		cookie &= DLOAD_DISABLE;
	}

	if(cookie & CRASHDUMP_RESET
#ifdef CONFIG_FAILSAFE
		|| !(cookie & MARK_UBOOT_MILESTONE)
#endif
		)
	{
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_WRITE(param, (uintptr_t)TCSR_BOOT_MISC_REG,
						cookie);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("Error in reseting the Magic cookie\n");
				return;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
		}
	}
	return;
}

int do_crashdump(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
#if defined(CONFIG_WDT)
	struct udevice *dev;
#endif

	if (ipq_iscrashed()) {
#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
		/*
		 * Enabling ethernet for dump collection
		 */
		initr_net();
#endif

#ifdef CONFIG_SDX_ATTACH_SUPPORT
		ipq_board_gpio_config(SDX_POWER_CYCLE);
#endif

		ulong debug = env_get_ulong("debug", 10, 0);
		if ((debug != DBG_DISABLE) && (debug != DBG_CRASHDUMP))
			debug = 0;

#if defined(CONFIG_WDT)
	if (uclass_find_device_by_seq(UCLASS_WDT, 0, &dev) == 0)
		wdt_stop(dev);
#endif
		printf("Crashdump magic found, "
				"initializing dump activity..\n");
		ipq_dump_func(&dump_config, debug);
	}

	if (ipq_iscrashed_crashdump_disabled()) {
#ifdef CONFIG_SDX_ATTACH_SUPPORT
		ipq_board_gpio_config(SDX_POWER_CYCLE);
#endif
		printf("Crashdump disabled, resetting the board..\n");
		reset();
	}

	return 0;
}

U_BOOT_CMD(
	crashdump, 2, 0, do_crashdump,
	"Collect crashdump from ipq if crashed",
	"crashdump <debug>\n"
	);
