// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2013, 2015-2017, 2020 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/byteorder.h>
#include <memalign.h>
#include <fdtdec.h>
#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <linux/delay.h>
#include <part.h>
#include <dm.h>
#include <smem.h>
#include <common.h>
#include <lmb.h>
#ifdef CONFIG_PHY_AQUANTIA
#include <u-boot/crc.h>
#include <miiphy.h>
#endif
#ifdef CONFIG_MMC
#include <mmc.h>
#include <sdhci.h>
#endif
#ifdef CONFIG_IPQ_NAND
#include <nand.h>
#endif
#ifdef CONFIG_CMD_UBI
#include <ubi_uboot.h>
#endif
#include <net.h>
#include <spi.h>
#include <spi_flash.h>
#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
#include <cpu_func.h>
#include <asm/cache.h>
#include <asm/io.h>

#include "ipq_board.h"

DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_MMC
int mmc_write_protect(struct mmc *mmc, unsigned int start_blk,
		      unsigned int cnt_blk, int set_clr);
#endif

uint32_t g_board_machid;
uint32_t g_load_addr;
#if defined(CONFIG_ENV_IS_IN_SPI_FLASH)
uint32_t g_env_offset __attribute__((section(".data"))) = 0;
#endif
char g_board_dts[BOARD_DTS_MAX_NAMELEN] = { 0 };
uint8_t g_recovery_path __attribute__((section(".data"))) = 0;

ipq_smem_flash_info_t ipq_smem_flash_info;
struct smem_ptable *ptable;
socinfo_t ipq_socinfo;

extern int ipq_smem_get_socinfo(void);
extern int part_get_info_efi(struct blk_desc *dev_desc, int part,
		struct disk_partition *info);

void set_ethmac_addr(void);

__weak int ipq_uboot_uart_fdt_fixup(uint32_t machid)
{
	return 0;
}

__weak int ipq_uboot_fdt_fixup_smem(void *blob)
{
	return 0;
}

__weak void ipq_uboot_fdt_fixup_usb(void *blob)
{
	return;
}

__weak void ipq_uboot_fdt_fixup(uint32_t machid)
{
	return;
}

__weak void set_flash_secondary_type(uint32_t flash_type)
{
	return;
}

__weak void ipq_config_cmn_clock(void)
{
	return;
}

__weak void board_cache_init(void)
{
	icache_enable();
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
	dcache_enable();
#endif
}

ipq_smem_flash_info_t * get_ipq_smem_flash_info(void)
{
	return &ipq_smem_flash_info;
}

struct smem_ptable * get_ipq_part_table_info(void)
{
	return ptable;
}

socinfo_t * get_socinfo(void)
{
	return &ipq_socinfo;
}

/*
 * This function should only be used when sfi->flash_type is
 * SMEM_BOOT_SPI_FLASH
 * retrieve the which_flash flag based on partition name.
 * flash_var is 1 if partition is in NAND.
 * flash_var is 0 if partition is in NOR.
 * flash_var is -1 if partition is in EMMC.
 */
unsigned int get_which_flash_param(char *part_name)
{
	int i;
	int flash_var = -1;

	for (i = 0; i < ptable->len; i++) {
		struct smem_ptn *p = &ptable->parts[i];
		if (strcmp(p->name, part_name) == 0)
			flash_var = part_which_flash(p);
	}

	return flash_var;
}

int get_current_board_flash_config(int flash_type)
{
	int ret;
	int board_type;
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info;
	blkpart_info_t  bpart_info;

	if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
		BLK_PART_GET_INFO_S(bpart_info, "rootfs", &disk_info,
					flash_type);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret) {
#ifdef CONFIG_IPQ_NAND
			/*
			 * Since unable to read GPT.
			 * Try NAND init to confirm secondary flash is nand
			 * otherwise is MMC
			 */
			nand_init();
			if (get_nand_dev_by_index(0)) {
				board_type = SMEM_BOOT_NORPLUSNAND;
			} else
#endif
			{
				board_type = SMEM_BOOT_NORPLUSEMMC;
			}
		} else {
			if (bpart_info.isnand)
				board_type = SMEM_BOOT_NORPLUSNAND;
			else
				board_type = SMEM_BOOT_NORPLUSEMMC;
		}
	} else
#endif
	{
		ret = get_which_flash_param("rootfs");
		if (ret == -1) {
			board_type = SMEM_BOOT_NORPLUSEMMC;
		} else if (ret) {
			board_type = SMEM_BOOT_NORPLUSNAND;
		} else {
			board_type = SMEM_BOOT_SPI_FLASH;
		}
	}

	return board_type;
}

#if CONFIG_IS_ENABLED(NAND_QTI)
void board_nand_init(void)
{
	struct udevice *dev;
	ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;
	int ret;

	/*
	 * Since the training partition info present in the gpt table
	 * which resides inside SPI-NOR flash so spi nor probe is must
	 * before nand init in NOTGPT case.
	 */
	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
#ifdef CONFIG_IPQ_SPI_NOR
		ipq_spi_probe();
#endif
	}

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(qti_nand), &dev);
	if (ret && ret != -ENODEV)
		pr_err("Failed to initialize %s. (error %d)\n",
				dev->name, ret);
}
#endif

__weak uint32_t get_soc_hw_version(void)
{
        return readl(CONFIG_SOC_HW_VERSION_REG);
}

void update_board_type(void)
{
	uint32_t board_type;

	board_type = gd->board_type;

	if(SMEM_BOOT_NO_FLASH == board_type)
		return;

	if(is_secure_boot())
		board_type |= SECURE_BOARD;

	if(is_atf_enbled())
		board_type |= ATF_ENABLED;

	gd->board_type = board_type;
}

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
void setup_arch_cntfreq(void)
{
	unsigned long freq = CONFIG_COUNTER_FREQUENCY /
		CFG_EMUL_FREQUENCY_DIVIDER;
	asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");
	return;
}
#endif

int fdtdec_board_setup(const void *fdt_blob)
{
	return ipq_uboot_fdt_fixup_smem((void*)fdt_blob);
}

#if defined(CONFIG_ENV_IS_IN_SPI_FLASH) && defined(CONFIG_BOARD_EARLY_INIT_R)
static void ipq_update_env_offset(int blk_sz)
{
	int i;

	if (IS_ERR_OR_NULL(ptable))
		return;

	for (i = 0; i < ptable->len; i++) {
		struct smem_ptn *p = &ptable->parts[i];
		if (IS_ERR_OR_NULL(p))
			continue;

		if (!strncmp(p->name, "0:APPSBLENV", SMEM_PTN_NAME_MAX)) {
			g_env_offset  = ((loff_t)p->start) * blk_sz;
		}
	}
}
#endif

#if defined(CONFIG_BOARD_EARLY_INIT_R)
int board_early_init_r(void)
{
	ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;

	if(sfi == NULL)
		return 0;

#if defined(CONFIG_IPQ_SPI_NOR) && defined(CONFIG_ENV_IS_IN_SPI_FLASH)
	ipq_spi_probe();
#endif

	switch(sfi->flash_type) {
#if defined(CONFIG_ENV_IS_IN_SPI_FLASH)
	case SMEM_BOOT_SPI_FLASH:
		ipq_update_env_offset(sfi->flash_block_size);
		break;
#if defined(CONFIG_NOR_BLK)
	case SMEM_BOOT_NORGPT_FLASH:
		struct disk_partition disk_info;
		blkpart_info_t  bpart_info;
		int ret;

		BLK_PART_GET_INFO_S(bpart_info, "0:APPSBLENV", &disk_info,
					sfi->flash_type);

		/*
		 * add flash details in sfi structure
		 */
		sfi->flash_block_size = (ipq_spi_probe())->sector_size;
		sfi->flash_density = (ipq_spi_probe())->size;

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (!ret)
			g_env_offset = (u32)disk_info.start * disk_info.blksz;
		break;
#endif
#endif
	default:
		sfi =  sfi;
		break;
	}

	return 0;
}
#endif

int board_init(void)
{
	ipq_smem_bootconfig_info_t *ipq_smem_bootconfig_info;
	ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;
	uint32_t *flash_type;
	uint32_t *flash_chip_select;
	uint32_t *primary_mibib;
	uint32_t *flash_index;
	uint32_t *flash_block_size;
	uint32_t *flash_density;
#ifdef CONFIG_BOOTCONFIG_V3
	uint32_t *try_mode_inprogress;
	uint32_t *edl_mode;
#endif
	gd->bd->bi_boot_params = BOOT_PARAMS_ADDR;

	flash_type = smem_get_item(SMEM_BOOT_FLASH_TYPE);
	if (IS_ERR_OR_NULL(flash_type)) {
		debug("Failed to get SMEM item: SMEM_BOOT_FLASH_TYPE\n");
		flash_type = NULL;
	}

	flash_index = smem_get_item(SMEM_BOOT_FLASH_INDEX);
	if (IS_ERR_OR_NULL(flash_index)) {
		debug("Failed to get SMEM item: SMEM_BOOT_FLASH_INDEX\n");
		flash_index = NULL;
	}

	flash_chip_select = smem_get_item(SMEM_BOOT_FLASH_CHIP_SELECT);
	if (IS_ERR_OR_NULL(flash_chip_select)) {
		debug("Failed to get SMEM item: SMEM_BOOT_FLASH_CHIP_SELECT\n");
		flash_chip_select = NULL;
	}

	flash_block_size = smem_get_item(SMEM_BOOT_FLASH_BLOCK_SIZE);
	if (IS_ERR_OR_NULL(flash_block_size)) {
		debug("Failed to get SMEM item: SMEM_BOOT_FLASH_BLOCK_SIZE\n");
		flash_block_size = NULL;
	}

	flash_density = smem_get_item(SMEM_BOOT_FLASH_DENSITY);
	if (IS_ERR_OR_NULL(flash_density)) {
		debug("Failed to get SMEM item: SMEM_BOOT_FLASH_DENSITY\n");
		flash_density = NULL;
	}

	primary_mibib = smem_get_item(SMEM_PARTITION_TABLE_OFFSET);
	if (IS_ERR_OR_NULL(primary_mibib)) {
		debug("Failed to get SMEM item: " \
				"SMEM_PARTITION_TABLE_OFFSET\n");
		primary_mibib = NULL;
	}

	ipq_smem_bootconfig_info = smem_get_item(SMEM_BOOT_DUALPARTINFO);
	if (!is_valid_bootconfig(ipq_smem_bootconfig_info)) {
		debug("Failed to get SMEM item: SMEM_BOOT_DUALPARTINFO\n");
		ipq_smem_bootconfig_info = NULL;
	}

#ifdef CONFIG_BOOTCONFIG_V3
	try_mode_inprogress = smem_get_item(SMEM_TRY_MODE_INPROGRESS);
	if (IS_ERR_OR_NULL(try_mode_inprogress)) {
		debug("Failed to get SMEM item: SMEM_TRY_MODE_INPROGRESS\n");
		try_mode_inprogress = NULL;
	}

	edl_mode = smem_get_item(SMEM_EDL_MODE);
	if (IS_ERR_OR_NULL(edl_mode)) {
		debug("Failed to get SMEM item: SMEM_EDL_MODE\n");
		edl_mode = NULL;
	}
#endif
	sfi->flash_type = (!flash_type ? SMEM_BOOT_NO_FLASH : *flash_type);
	sfi->flash_index = (!flash_index ? 0 : *flash_index);
	sfi->flash_chip_select = (!flash_chip_select ? 0 : *flash_chip_select);
	sfi->flash_block_size = (!flash_block_size ? 0: *flash_block_size);
	sfi->flash_density = (!flash_density ? 0 : *flash_density);
	sfi->primary_mibib = (!primary_mibib ? 0 : *primary_mibib);
	sfi->ipq_smem_bootconfig_info = ipq_smem_bootconfig_info;
#ifdef CONFIG_BOOTCONFIG_V3
	sfi->try_mode_inprogress = (!try_mode_inprogress ? 0 : *try_mode_inprogress);
	sfi->edl_mode = (!edl_mode ? 0 : *edl_mode);
#endif
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	sfi->mmc_gpt_pte.gpt_pte = NULL;
	sfi->nor_gpt_pte.gpt_pte = NULL;
#endif

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
	if ((current_el() == 3) && (sfi->flash_type != SMEM_BOOT_NO_FLASH))
		setup_arch_cntfreq();
#endif

	switch(sfi->flash_type) {
	case SMEM_BOOT_MMC_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
	case SMEM_BOOT_NO_FLASH:
		break;
	default:
		ptable = smem_get_item(SMEM_AARM_PARTITION_TABLE);
		if (IS_ERR_OR_NULL(ptable)) {
			debug("Failed to get SMEM item: " \
					"SMEM_AARM_PARTITION_TABLE\n");
			return -ENODEV;
		}

		if (ptable->magic[0] != _SMEM_PTABLE_MAGIC_1 ||
			ptable->magic[1] != _SMEM_PTABLE_MAGIC_2)
			return -ENOMSG;
	}

	return 0;
}

int ipq_smem_get_socinfo()
{
	union ipq_platform *platform_type;

	platform_type = smem_get_item(SMEM_HW_SW_BUILD_ID);
	if (IS_ERR_OR_NULL(platform_type)) {
		debug("Failed to get SMEM item: SMEM_HW_SW_BUILD_ID\n");
		return -ENODEV;
	}

	ipq_socinfo.cpu_type = platform_type->v1.id;
	ipq_socinfo.version = platform_type->v1.version;
	ipq_socinfo.soc_version_major =
				SOCINFO_VERSION_MAJOR(ipq_socinfo.version);
	ipq_socinfo.soc_version_minor =
				SOCINFO_VERSION_MINOR(ipq_socinfo.version);
	ipq_socinfo.machid = g_board_machid;

	return 0;

}

/**
 * mibib_ptable_init - initializes SMEM partition table
 *
 * Initialize partition table from MIBIB.
 */
int mibib_ptable_init(unsigned int* addr)
{
	struct smem_ptable* mib_ptable;

	mib_ptable = (struct smem_ptable*) addr;
	if (mib_ptable->magic[0] != _SMEM_PTABLE_MAGIC_1 ||
		mib_ptable->magic[1] != _SMEM_PTABLE_MAGIC_2)
		return -ENOMSG;

	/* In recovery & mmc boot, ptable will not be initialized.
	 * So, allocate ptable memory in recovery mode.
	 */
	if (!ptable) {
		ptable = malloc(sizeof(struct smem_ptable));
		if (!ptable)
			return -ENOMEM;
	}

	memcpy(ptable, addr, sizeof(struct smem_ptable));
	return 0;
}

__weak void board_early_clock_enable(void)
{
	return;
}

/*
 * This function is called in the very beginning.
 * Retreive the machtype info from SMEM and map the board specific
 * parameters. Shared memory region at Dram address
 * contains the machine id/ board type data polulated by SBL.
 */
int board_early_init_f(void)
{
#ifdef CONFIG_SMEM_VERSION_C
	union ipq_platform *platform_type;

	platform_type = smem_get_item(SMEM_HW_SW_BUILD_ID);
	if (IS_ERR_OR_NULL(platform_type)) {
		debug("Failed to get SMEM item: SMEM_HW_SW_BUILD_ID\n");
		return -ENODEV;
	}

	g_board_machid = ((platform_type->v1.hw_platform << 24) |
			  ((SOCINFO_VERSION_MAJOR(
				platform_type->v1.platform_version)) << 16) |
			  ((SOCINFO_VERSION_MINOR(
				platform_type->v1.platform_version)) << 8) |
			  (platform_type->v1.hw_platform_subtype));
#else
	struct smem_machid_info *machid_info;
	machid_info = smem_get_item(SMEM_MACHID_INFO_LOCATION);
	if (IS_ERR_OR_NULL(machid_info)) {
		debug("Failed to get SMEM item: SMEM_MACHID_INFO_LOCATION\n");
		return -ENODEV;
	}

	g_board_machid = machid_info->machid;
#endif
	ipq_uboot_uart_fdt_fixup(g_board_machid);

#ifdef CONFIG_EARLY_CLOCK_ENABLE
	board_early_clock_enable();
#endif

	return 0;
}

int board_fix_fdt(void *rw_fdt_blob)
{
	ipq_uboot_fdt_fixup_smem(rw_fdt_blob);
	ipq_uboot_fdt_fixup(g_board_machid);
	ipq_uboot_fdt_fixup_usb(rw_fdt_blob);
	return 0;
}

#ifdef CONFIG_MULTI_DTB_FIT
int board_fit_config_name_match(const char *name)
{
	if (!strcmp(name, g_board_dts)) {
		printf("Booting %s\n", name);
		return 0;
	}

	return -1;
}
#endif /* CONFIG_MULTI_DTB_FIT */

#ifdef CONFIG_DTB_RESELECT
int embedded_dtb_select(void)
{
	int rescan;
	unsigned int i;
	for (i=0; i<*machid_dts_entries; i++)
		if (machid_dts_info[i].machid == g_board_machid)
			strlcpy(g_board_dts, machid_dts_info[i].dts,
					BOARD_DTS_MAX_NAMELEN);

	fdtdec_resetup(&rescan);

	return 0;
}
#endif /* CONFIG_DTB_RESELECT */

void setup_board_default_env(void)
{
	uint32_t soc_hw_version;

	/*
	 * setup machid
	 */
	env_set_hex("machid", gd->bd->bi_arch_number);

#ifdef CONFIG_PREBOOT
	/*
	 * forceset preboot env to avoid SDI/crashdump path system bootup
	 */
	env_set("preboot", CONFIG_PREBOOT);
#endif
	/*
	 * set soc hw version in env
	 */
	soc_hw_version = get_soc_hw_version();
	if (soc_hw_version)
		env_set_hex("soc_hw_version", soc_hw_version);

	env_set_ulong("soc_version_major", ipq_socinfo.soc_version_major);
	env_set_ulong("soc_version_minor", ipq_socinfo.soc_version_minor);
#ifdef CFG_CUSTOM_LOAD_ADDR
	env_set_hex("loadaddr", CFG_CUSTOM_LOAD_ADDR);
#endif
}

__weak void board_update_RFA_settings(void)
{
	return;
}

#ifdef CONFIG_IPQ_MMC
static void init_mmc(void)
{
	struct mmc *mmc;
	mmc = find_mmc_device(0);
	if (!mmc) {
		printf("no mmc device at slot 0\n");
		return;
	}

	if (mmc_init(mmc))
		printf("mmc init failed\n");
#ifdef CONFIG_EFI_PARTITION
	struct blk_desc *dev;
	dev = blk_get_devnum_by_uclass_id(UCLASS_MMC, 0);
	if (dev != NULL && dev->part_type == PART_TYPE_UNKNOWN)
		dev->part_type = PART_TYPE_EFI;
#endif

	watchdog_reset();

	return;
}
#endif

#ifdef CONFIG_MMC_FLASH_PARTITION_WRITE_PROTECT
static inline int is_readonly(gpt_entry *p)
{
	/* bit 60 of gpt attribute denotes read-only flag */
	if (p->attributes.raw & ((unsigned long long)1 << 60))
		return 1;
	return 0;
}

void board_flash_protect(void)
{
	int num_part;
	struct mmc *mmc;
	struct blk_desc *mmc_dev;
	struct disk_partition info;
	int curr_device = -1;
	gpt_entry *gpt_pte = NULL;
	ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;

	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			puts("No MMC device available\n");
			goto out;
		}
	}

	mmc = find_mmc_device(curr_device);
	if (!mmc) {
		printf("no mmc device at slot %x\n", curr_device);
		goto out;
	}

	mmc_dev = mmc_get_blk_desc(mmc);

	if (mmc_dev != NULL && mmc_dev->type != DEV_TYPE_UNKNOWN) {
		gpt_pte = get_gpt_entry(mmc_dev);
		if(!gpt_pte) {
			printf("%s: Failed to get gpt table entry\n", __func__);
			goto out;
		}

		num_part = sfi->mmc_gpt_pte.ncount;

		if (num_part < 0) {
			printf("Both primary & backup GPT are invalid, "
					"skipping mmc write protection.\n");
			goto out;
		}

		for (uint8_t part = 1; part <= num_part; part++) {
			uint8_t readonly = is_readonly(&gpt_pte[part - 1]);
			if(readonly) {
				if(part_get_info_efi(mmc_dev, part, &info))
					continue;

				if(!mmc_write_protect(mmc,
						  info.start,
						  info.size, 1))
					printf("\"%s\""
						"-protected MMC partition\n",
						info.name);
				else
					printf("Write protect failed for "
							"\"%s\"", info.name);
			}
		}
	}
out:
	if (gpt_pte)
		free(gpt_pte);

	watchdog_reset();

	return;
}
#endif


__weak int read_bootconfig(void)
{
	return 1;
}

int board_late_init(void)
{
	ipq_smem_flash_info_t *sfi = &ipq_smem_flash_info;
	uint32_t board_type;
	int ret = 0, active_part = 0;

#ifdef CONFIG_IPQ_MMC
	init_mmc();
#endif
	switch(sfi->flash_type) {
	case SMEM_BOOT_NORGPT_FLASH:
	case SMEM_BOOT_SPI_FLASH:
		board_type = get_current_board_flash_config(sfi->flash_type);
		break;
	default:
		board_type = sfi->flash_type;
	}

	switch(board_type) {
	case SMEM_BOOT_NORPLUSEMMC:
		sfi->flash_secondary_type = SMEM_BOOT_MMC_FLASH;
		break;
	case SMEM_BOOT_NORPLUSNAND:
		sfi->flash_secondary_type = SMEM_BOOT_QSPI_NAND_FLASH;
		break;
	default:
		sfi->flash_secondary_type = 0;
	}

	/*
	 * To set SoC specific secondary flash type to
	 * eMMC/NAND device based on the one that is enabled.
	 */
	set_flash_secondary_type(sfi->flash_secondary_type);

	/*
	 * get soc_version, cpu_type, machid
	 */

	ipq_smem_get_socinfo();

	gd->board_type = board_type;

	update_board_type();

	if(SZ_256M == gd->ram_size && CONFIG_SYS_LOAD_ADDR > SZ_256M) {
		g_load_addr = CFG_SYS_SDRAM_BASE + SZ_64M;
	} else {
		g_load_addr = CONFIG_SYS_LOAD_ADDR;
	}

	/*
	 * Get active boot partition from bootconfig data
	 */
	if(sfi->ipq_smem_bootconfig_info == NULL)
		ret = read_bootconfig();

	active_part = get_rootfs_active_partition(sfi);
	if(active_part >= 0)
		gd->board_type |= active_part ? ACTIVE_BOOT_SET : 0;
#ifdef CONFIG_BOOTCONFIG_V3
	else
		gd->board_type |= INVALID_BOOT;
#endif

	switch(sfi->flash_type) {
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_QSPI_NAND_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		get_kernel_fs_part_details(sfi->flash_type);
		break;
	default:
		;
	}

	watchdog_reset();

#ifdef CONFIG_QTI_NSS_SWITCH
	/*
	 * configure CMN clock for ethernet
	 */
	ipq_config_cmn_clock();
#endif
	/*
	 * setup mac address
	 */
	set_ethmac_addr();

	watchdog_reset();
	/*
	 * setup default env
	 */
	setup_board_default_env();

#ifdef CONFIG_CAPIN_CAPOUT_SUPPORT
	/*
	 * Update RFA register based on caldata
	 */
	board_update_RFA_settings();

	watchdog_reset();
#endif

#ifdef CONFIG_MMC_FLASH_PARTITION_WRITE_PROTECT
	board_flash_protect();
#endif
	return 0;
}

int dram_init(void)
{
	int i, ret = CMD_RET_SUCCESS;
	int count = 0;
	struct usable_ram_partition_table *ram_ptable;
	struct ram_partition_entry *p;

	ram_ptable = smem_get_item(SMEM_USABLE_RAM_PARTITION_TABLE);
	if (IS_ERR_OR_NULL(ram_ptable)) {
		debug("Failed to get SMEM item: " \
				"SMEM_USABLE_RAM_PARTITION_TABLE\n");
		ret = -ENODEV;
	}

	gd->ram_size = 0;
	/* Check validy of RAM */
	for (i = 0; i < CONFIG_RAM_NUM_PART_ENTRIES; i++) {
		p = &ram_ptable->ram_part_entry[i];
		if (p->partition_category == RAM_PARTITION_SDRAM &&
				p->partition_type ==
				RAM_PARTITION_SYS_MEMORY) {
			gd->ram_size += p->length;
			debug("Detected memory bank %u: "
				"start: 0x%llx size: 0x%llx\n",
					count, p->start_address, p->length);
			count++;
		}
        }

	if (!count) {
		printf("Failed to detect any memory bank\n");
		ret = CMD_RET_FAILURE;
	}

	return ret;
}

phys_size_t get_effective_memsize(void)
{
	phys_size_t ram_size = min(gd->ram_size,
			(phys_size_t)CFG_SYS_SDRAM_BASE0_SIZE);

#ifndef CONFIG_ARM64
	if (((uint64_t)gd->ram_base + ram_size) > ULONG_MAX)
		ram_size = ULONG_MAX - gd->ram_base;
#endif
	return ram_size;
}

int dram_init_banksize(void)
{
	uint8_t i = 0, bidx = 0;
	struct usable_ram_partition_table *ram_ptable;
	struct ram_partition_entry *p;

	ram_ptable = smem_get_item(SMEM_USABLE_RAM_PARTITION_TABLE);
	if (IS_ERR_OR_NULL(ram_ptable)) {
		printf("Failed to get SMEM item: " \
				"SMEM_USABLE_RAM_PARTITION_TABLE\n");
		return -ENODEV;
	}

	/* Check validy of RAM */
	for (i = 0; i < CONFIG_RAM_NUM_PART_ENTRIES; i++) {
		p = &ram_ptable->ram_part_entry[i];
		if (p->partition_category == RAM_PARTITION_SDRAM &&
				p->partition_type == RAM_PARTITION_SYS_MEMORY)
		{
			gd->bd->bi_dram[bidx].start = p->start_address;
			gd->bd->bi_dram[bidx].size = p->length;
			debug("Detected memory bank %u: "
				"start: 0x%llx size: 0x%llx\n",
					bidx, p->start_address, p->length);
			bidx++;
		}
        }

	return 0;
}

void *env_sf_get_env_addr(void)
{
        return NULL;
}

enum env_location env_get_location(enum env_operation op, int prio)
{
	int ret = ENVL_NOWHERE;
	uint32_t *flash_type;

	if (prio)
		return ENVL_UNKNOWN;

	flash_type = smem_get_item(SMEM_BOOT_FLASH_TYPE);
	if (IS_ERR_OR_NULL(flash_type))
		return ret;

	switch(*flash_type) {
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		ret = ENVL_SPI_FLASH;
		break;
	case SMEM_BOOT_MMC_FLASH:
		ret = ENVL_MMC;
		break;
	case SMEM_BOOT_QSPI_NAND_FLASH:
	case SMEM_BOOT_NAND_FLASH:
		ret = ENVL_NAND;
		break;
	default:
		ret = ENVL_NOWHERE;
	}

	return ret;
}

#ifdef CONFIG_MMC
int mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr)
{
	int ret;
	struct disk_partition disk_info;
	blkpart_info_t  bpart_info;

	BLK_PART_GET_INFO_S(bpart_info, "0:APPSBLENV", &disk_info,
					SMEM_BOOT_MMC_FLASH);

	ret = ipq_part_get_info_by_name(&bpart_info);
	if (!ret) {
		*env_addr = (u32)disk_info.start * disk_info.blksz;
	}

	return ret;
}
#endif

void board_lmb_reserve(struct lmb *lmb)
{
	if (lmb)
		lmb->reserved.region[0].size = (CONFIG_TEXT_BASE -
				lmb->reserved.region[0].base);
}

#ifdef CONFIG_PHY_AQUANTIA
static int ipq_aquantia_load_memory(struct phy_device *phydev, u32 addr,
				const u8 *data, size_t len)
{
	size_t pos;
	u16 crc = 0, up_crc;

	phy_write(phydev, MDIO_MMD_VEND1, 0x200, BIT(12));
	phy_write(phydev, MDIO_MMD_VEND1, 0x202, addr >> 16);
	phy_write(phydev, MDIO_MMD_VEND1, 0x203, addr & 0xfffc);

	for (pos = 0; pos < len; pos += min(sizeof(u32), len - pos)) {
		u32 word = 0;

		memcpy(&word, &data[pos], min(sizeof(u32), len - pos));

		phy_write(phydev, MDIO_MMD_VEND1, 0x204,
			  (word >> 16));
		phy_write(phydev, MDIO_MMD_VEND1, 0x205,
			  word & 0xffff);

		phy_write(phydev, MDIO_MMD_VEND1, 0x200,
			  BIT(15) | BIT(14));

		/* keep a big endian CRC to match the phy processor */
		word = cpu_to_be32(word);
		crc = crc16_ccitt(crc, (u8 *)&word, sizeof(word));
	}

	up_crc = phy_read(phydev, MDIO_MMD_VEND1, 0x201);
	if (crc != up_crc) {
		printf("%s CRC Mismatch: Calculated 0x%04hx PHY 0x%04hx\n",
		       phydev->dev->name, crc, up_crc);
		return -EINVAL;
	}

	watchdog_reset();

	return 0;
}

static int ipq_aquantia_upload_firmware(struct phy_device *phydev,
		uint8_t *addr,	uint32_t file_size)
{
	int ret;
	uint8_t *buf = addr;
	uint32_t primary_header_ptr = 0x00000000;
	uint32_t primary_iram_ptr = 0x00000000;
	uint32_t primary_dram_ptr = 0x00000000;
	uint32_t primary_iram_sz = 0x00000000;
	uint32_t primary_dram_sz = 0x00000000;
	uint32_t phy_img_hdr_off = 0x300;
	uint16_t recorded_ggp8_val, daisy_chain_dis;
	u16 computed_crc, file_crc;

	phy_write(phydev, MDIO_MMD_VEND1, 0x300, 0xdead);
	phy_write(phydev, MDIO_MMD_VEND1, 0x301, 0xbeaf);
	if ((phy_read(phydev, MDIO_MMD_VEND1, 0x300) != 0xdead) &&
			(phy_read(phydev, MDIO_MMD_VEND1, 0x301) != 0xbeaf)) {
		printf("PHY::Scratchpad Read/Write test fail\n");
		ret = -EIO;
		goto exit;
	}

	file_crc = buf[file_size - 2] << 8 | buf[file_size - 1];
	computed_crc = crc16_ccitt(0, buf, file_size -2);
	if (file_crc != computed_crc) {
		printf("CRC check failed on phy fw file\n");
		ret = -EIO;
		goto exit;
	}

	printf("CRC check good on PHY FW (0x%04X)\n", computed_crc);
	daisy_chain_dis = phy_read(phydev, MDIO_MMD_VEND1, 0xc452);
	if (!(daisy_chain_dis & 0x1))
		phy_write(phydev, MDIO_MMD_VEND1, 0xc452, 0x1);

	phy_write(phydev, MDIO_MMD_VEND1, 0xc471, 0x40);
	recorded_ggp8_val = phy_read(phydev, MDIO_MMD_VEND1, 0xc447);
	if ((recorded_ggp8_val & 0x1f) != phydev->addr)
		phy_write(phydev, MDIO_MMD_VEND1, 0xc447, phydev->addr);

	phy_write(phydev, MDIO_MMD_VEND1, 0xc441, 0x4000);
	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x41);

	primary_header_ptr = (((buf[0x9] & 0x0F) << 8) | buf[0x8]) << 12;

	primary_iram_ptr = (buf[primary_header_ptr +
		phy_img_hdr_off + 0x4 + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0x4 + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0x4];
	primary_iram_sz = (buf[primary_header_ptr +
		phy_img_hdr_off + 0x7 + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0x7 + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0x7];
        primary_dram_ptr = (buf[primary_header_ptr +
		phy_img_hdr_off + 0xA + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0xA + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0xA];
        primary_dram_sz = (buf[primary_header_ptr +
		phy_img_hdr_off + 0xD + 2] << 16) |
		(buf[primary_header_ptr + phy_img_hdr_off + 0xD + 1] << 8) |
		buf[primary_header_ptr + phy_img_hdr_off + 0xD];
	primary_iram_ptr += primary_header_ptr;
	primary_dram_ptr += primary_header_ptr;

	phy_write(phydev, MDIO_MMD_VEND1, 0x200, 0x1000);
	phy_write(phydev, MDIO_MMD_VEND1, 0x200, 0x0);
	computed_crc = 0;

	printf("PHYFW:Loading IRAM...........");
	ret = ipq_aquantia_load_memory(phydev, 0x40000000,
			&buf[primary_iram_ptr], primary_iram_sz);
	if (ret < 0)
		goto exit;
	printf("done.\n");

	printf("PHYFW:Loading DRAM..............");
	ret = ipq_aquantia_load_memory(phydev, 0x3ffe0000,
			&buf[primary_dram_ptr], primary_dram_sz);
	if (ret < 0)
		goto exit;
	printf("done.\n");

	phy_write(phydev, MDIO_MMD_VEND1, 0x0, 0x0);
	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x41);
	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x8041);
	mdelay(100);

	phy_write(phydev, MDIO_MMD_VEND1, 0xc001, 0x40);
	mdelay(100);
	printf("PHYFW loading done.\n");
exit:
	watchdog_reset();

	return ret;
}

int ipq_aquantia_load_fw(struct phy_device *phydev)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	u8 *fw_load_addr = NULL;
	int ret = 0;
	mbn_header_t *fwimg_header;

	fw_load_addr = (u8 *)malloc_cache_aligned(IPQ_ETH_FW_PART_SIZE);
	/* We only need memory equivalent to max size ETHPHYFW
	 * which is currently assumed as 512 KB.
	 */
	if (fw_load_addr == NULL) {
		printf("ETHPHYFW Loading failed, size = 0x%x\n",
			IPQ_ETH_FW_PART_SIZE);
		ret = -ENOMEM;
		goto exit;
	}

	memset(fw_load_addr, 0, IPQ_ETH_FW_PART_SIZE);

	ret = get_partition_data(IPQ_ETH_FW_PART_NAME, 0, fw_load_addr,
					IPQ_ETH_FW_PART_SIZE, sfi->flash_type);
	if (ret < 0)
		goto free_nd_exit;

	fwimg_header = (mbn_header_t *)(fw_load_addr);

	if (fwimg_header->image_type == 0x13 &&
			fwimg_header->header_vsn_num == 0x3) {
		ret = ipq_aquantia_upload_firmware(phydev,
				(uint8_t*)((uint32_t)sizeof(mbn_header_t)
				+ fw_load_addr),
				(uint32_t)(fwimg_header->image_size));
		if (ret != 0)
			goto free_nd_exit;
	} else {
		printf("bad magic on ETHPHYFW partition\n");
		ret = -1;
		goto free_nd_exit;
	}

free_nd_exit:
	free(fw_load_addr);
exit:
	watchdog_reset();

	return ret;
}
#endif /* CONFIG_PHY_AQUANTIA */

int get_eth_mac_address(uint8_t *enetaddr, int no_of_macs)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	return get_partition_data("0:ART", 0, enetaddr, no_of_macs * 6,
					sfi->flash_type);
}

void set_ethmac_addr(void)
{
	int i, ret;
	uchar enetaddr[CONFIG_ETH_MAX_MAC * 6] = { 0 };
	uchar *mac_addr;
	char ethaddr[16] = "ethaddr";
	char mac[64];
	/* Get the MAC address from ART partition */
	ret = get_eth_mac_address(enetaddr, CONFIG_ETH_MAX_MAC);
	for (i = 0; (ret >= 0) && (i < CONFIG_ETH_MAX_MAC); i++) {
		mac_addr = &enetaddr[i * 6];
		if (!is_valid_ethaddr(mac_addr)) {
			printf("MAC%d Address from ART is not valid\n", i);
		} else {
			/*
			 * U-Boot uses these to patch the 'local-mac-address'
			 * dts entry for the ethernet entries, which in turn
			 * will be picked up by the HLOS driver
			 */
			snprintf(mac, sizeof(mac), "%x:%x:%x:%x:%x:%x",
					mac_addr[0], mac_addr[1],
					mac_addr[2], mac_addr[3],
					mac_addr[4], mac_addr[5]);
			env_set(ethaddr, mac);
		}
		snprintf(ethaddr, sizeof(ethaddr), "eth%daddr", (i + 1));
	}
}

void enable_caches(void)
{
#ifdef CONFIG_ARM64
	int i;
	uint8_t bidx = 0;

	/* Now Update the real DDR size based on Board configuration */
	for (i = 0; mem_map[i].size || mem_map[i].attrs; i++) {
		if (mem_map[i].size == 0x0UL) {
			if (!bidx)	/* For DDR bank 0 */
				mem_map[i].size =
					gd->ram_top - mem_map[i].virt;
			else		/* For remaining DDR banks */
				mem_map[i].size =
					gd->bd->bi_dram[bidx].size;
			bidx++;
		}
	}
#endif
	board_cache_init();

#ifndef CONFIG_MULTI_DTB_FIT_NO_COMPRESSION
	if (gd->new_fdt) {
		memcpy(gd->new_fdt, gd->fdt_blob, fdt_totalsize(gd->fdt_blob));
		flush_cache((ulong) gd->new_fdt, ALIGN(
					fdt_totalsize(gd->fdt_blob),
					ARCH_DMA_MINALIGN));
		gd->fdt_blob = gd->new_fdt;
	}
#endif
}
