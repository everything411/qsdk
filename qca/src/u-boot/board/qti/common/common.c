/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <asm/global_data.h>
#include <cpu_func.h>
#include <sysreset.h>
#include <common.h>
#include <command.h>
#include <smem.h>
#include <dm.h>
#include <malloc.h>
#include <memalign.h>
#include <bootm.h>
#ifdef CONFIG_IPQ_MMC
#include <mmc.h>
#endif
#ifdef CONFIG_IPQ_SPI_NOR
#include <spi.h>
#include <spi_flash.h>
#endif
#ifdef CONFIG_IPQ_NAND
#include <nand.h>
#endif
#ifdef CONFIG_CMD_UBI
#include <ubi_uboot.h>
#endif
#include <linux/psci.h>
#ifdef CONFIG_WDT
#include <dm/device-internal.h>
#include <wdt.h>
#include <asm/io.h>
#endif

#include "ipq_board.h"

DECLARE_GLOBAL_DATA_PTR;

#define MMC_MID_MICRON 0xFE
#define MMC_PNM_MICRON 0x4D4D43333247

#define MMC_GET_MID(CID0) (CID0 >> 24)
#define MMC_GET_PNM(CID0, CID1, CID2) (((long long int)(CID0 & 0xff) << 40) | \
                ((long long int)CID1 << 8) |                                  \
                (CID2 >> 24))

#define MMC_CMD_SET_WRITE_PROT		28
#define MMC_CMD_CLR_WRITE_PROT		29

#define MMC_ADDR_OUT_OF_RANGE(resp)	((resp >> 31) & 0x01)

/*
 * CSD fields
*/
#define WP_GRP_ENABLE(csd)		((csd[3] & 0x80000000) >> 31)
#define WP_GRP_SIZE(csd)		((csd[2] & 0x0000001f))
#define ERASE_GRP_MULT(csd)		((csd[2] & 0x000003e0) >> 5)
#define ERASE_GRP_SIZE(csd)		((csd[2] & 0x00007c00) >> 10)


#define EXT_CSD_BOOT_WP_B_PERM_WP_EN	(0x04)  /* permanent write-protect */

#ifndef CFG_UBI_FS_NAME
#define	CFG_UBI_FS_NAME		"fs"
#endif

#ifdef CONFIG_IPQ_MMC
int mmc_send_status(struct mmc *mmc, unsigned int *status);
int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value);
#endif

#ifdef CONFIG_SDX_ATTACH_SUPPORT
extern void ipq_board_power_cycle_sdx(void);
#endif

struct udevice *smem;
struct spi_flash *flash = NULL;

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
static int g_eth_initalized = 0;
extern int eth_initialize(void);
#endif

int gpt_find_which_flash(gpt_entry *p)
{
	/*
	 * bit 3 of gpt attribute denotes partition present in nand flash
	 */
	if (p->attributes.raw & CFG_IPQ_NAND_PART)
		return 1;

	return 0;
}

void *smem_get_item(unsigned int item)
{

	int ret = 0;
	struct udevice *smem_tmp;
	const char *name = "smem";
	size_t size;
	unsigned long int reloc_flag = (gd->flags & GD_FLG_RELOC);

	if (reloc_flag == 0)
		ret = uclass_get_device_by_name(UCLASS_SMEM, name, &smem_tmp);
	else if(!smem)
		ret = uclass_get_device_by_name(UCLASS_SMEM, name, &smem);

	if (ret < 0) {
		printf("Failed to find SMEM node. Check device tree %d\n",ret);
		return 0;
	}

	return smem_get(reloc_flag ? smem : smem_tmp, -1, item, &size);
}

void arch_preboot_os(void)
{

/*
 * restrict booting if image is not authenticated
 * in secure board.
 */
	uint32_t board_type = gd->board_type;

	if (!image_auth_check())
		return;

	if (board_type & KERNEL_AUTH_SUCCESS) {

		if (check_rootfs_authentication())
			if (board_type & ROOTFS_AUTH_SUCCESS)
				return;
			else
				reset_cpu();
		else
			return;
	} else {
		reset_cpu();
	}

	return;
}

#ifdef CONFIG_CMD_UBI
long long ubi_get_volume_size(char *volume)
{
	int i;
	struct ubi_device *ubi = ubi_get_device(0);
	struct ubi_volume *vol = NULL;

	if(NULL == ubi)
		return -ENODEV;

	for (i = 0; i < ubi->vtbl_slots; i++) {
		vol = ubi->volumes[i];
		if (vol && !strcmp(vol->name, volume))
			return vol->used_bytes;
	}

	watchdog_reset();

	printf("Volume %s not found!\n", volume);
	return -ENODEV;
}
#endif

__weak bool is_atf_enbled(void)
{
	return false;
}

#ifdef CONFIG_SCM_V1
bool is_secure_boot_v1(void)
{
	scm_param param;
	uint8_t *buff = NULL;
	int ret = -1;
	bool status = false;

	buff = (uint8_t *)malloc_cache_aligned(CONFIG_SYS_CACHELINE_SIZE);
	if(!buff) {
		printf("Unable allocate memory\n");
		return false;
	}

	watchdog_reset();

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_SECURE_BOOT(param, (uintptr_t)buff, sizeof(uint8_t));
		ret = ipq_scm_call(&param);

		/* invalidate cache to update latest value in buff */
		invalidate_dcache_range((unsigned long)buff,
					(unsigned long)buff +
					CONFIG_SYS_CACHELINE_SIZE);

		if(!ret && *(uint8_t *)buff == 1)
			status  = true;
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
	}

	if(buff)
		free(buff);

	watchdog_reset();

	return status;
}

#elif CONFIG_SCM_V2
bool is_secure_boot_v2(void)
{
	scm_param param;
	int ret = -1;
	struct fuse_payload {
		u32 fuse_addr;
		u32 lsb_val;
		u32 msb_val;
	};
	struct fuse_payload *fuse = NULL;
	size_t size = sizeof(struct fuse_payload);
	bool status = false;

	size = roundup(size, CONFIG_SYS_CACHELINE_SIZE);

	fuse = malloc_cache_aligned(size);
	if(!fuse)
		return false;

	memset(fuse, 0, sizeof(struct fuse_payload));

	watchdog_reset();

	fuse[0].fuse_addr = QFPROM_CORR_TME_OEM_ATE_ROW0_LSB;

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_READ_FUSE(param, (unsigned long)fuse,
					sizeof(struct fuse_payload));
		/* invalidate cache to update latest value in buff */
		flush_dcache_range((unsigned long)fuse,
					(unsigned long)fuse + size);
		ret = ipq_scm_call(&param);

		if(ret)
		{
			ret = -1;
			break;
		}

		if(fuse[0].lsb_val & OEM_SEC_BOOT_ENABLE)
		{
			status = true;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
	}

	if(fuse)
		free(fuse);

	watchdog_reset();

	return status;
}

#else
bool is_secure_boot_fake(void)
{
	return false;
}

#endif

#if CONFIG_IPQ_MMC
int mmc_send_wp_set_clr(struct mmc *mmc, unsigned int start,
			unsigned int size, int set_clr)
{
	unsigned int err;
	unsigned int wp_group_size, count, i;
	struct mmc_cmd cmd;
	unsigned int status;

	wp_group_size = (WP_GRP_SIZE(mmc->csd) + 1) * mmc->erase_grp_size;
	count = DIV_ROUND_UP(size, wp_group_size);

	if (set_clr)
		cmd.cmdidx = MMC_CMD_SET_WRITE_PROT;
	else
		cmd.cmdidx = MMC_CMD_CLR_WRITE_PROT;
	cmd.resp_type = MMC_RSP_R1b;

	for (i = 0; i < count; i++) {
		cmd.cmdarg = start + (i * wp_group_size);
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err) {
			printf("%s: Error at block 0x%x - %d\n", __func__,
			       cmd.cmdarg, err);
			return err;
		}

		if(MMC_ADDR_OUT_OF_RANGE(cmd.response[0])) {
			printf("%s: mmc block(0x%x) out of range", __func__,
			       cmd.cmdarg);
			return -EINVAL;
		}

		err = mmc_send_status(mmc, &status);
		if (err)
			return err;
	}

	watchdog_reset();

	return 0;
}

int mmc_write_protect(struct mmc *mmc, unsigned int start_blk,
		      unsigned int cnt_blk, int set_clr)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
	int err;
	unsigned int wp_group_size;

	if (!WP_GRP_ENABLE(mmc->csd))
		return -1; /* group write protection is not supported */

	err = mmc_send_ext_csd(mmc, ext_csd);

	if (err) {
		debug("ext_csd register cannot be retrieved\n");
		return err;
	}

	if ((ext_csd[EXT_CSD_USER_WP] & EXT_CSD_BOOT_WP_B_SEC_WP_SEL)
	    || (ext_csd[EXT_CSD_USER_WP] & EXT_CSD_BOOT_WP_B_PERM_WP_EN)) {
		printf("User power-on write protection is disabled. \n");
		return -1;
	}

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_USER_WP,
		         EXT_CSD_BOOT_WP_B_PWR_WP_EN);
	if (err) {
		printf("Failed to enable user power-on write protection\n");
		return err;
	}

	wp_group_size = (WP_GRP_SIZE(mmc->csd) + 1) * mmc->erase_grp_size;
	if ((MMC_GET_MID(mmc->cid[0]) == MMC_MID_MICRON) &&
		(MMC_GET_PNM(mmc->cid[0], mmc->cid[1],
					mmc->cid[2]) == MMC_PNM_MICRON))
		wp_group_size *= 2;

	if (!cnt_blk || start_blk % wp_group_size || cnt_blk % wp_group_size) {
		printf("Error: Unaligned offset/count. offset/count should be "
				"aligned to 0x%x blocks\n", wp_group_size);
		return -1;
	}

	err = mmc_send_wp_set_clr(mmc, start_blk, cnt_blk, set_clr);

	watchdog_reset();

	return err;
}

static int do_mmc_protect (struct cmd_tbl *cmdtp, int flag,
			   int argc, char * const argv[])
{
	struct mmc *mmc;
	unsigned int ret;
	unsigned int blk, cnt;
	int curr_device = -1;

	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			puts("No MMC device available\n");
			return CMD_RET_FAILURE;
		}
	}

	mmc = find_mmc_device(curr_device);
	if (!mmc) {
		printf("no mmc device at slot %x\n", curr_device);
		return -ENOMEM;
	}

	if (argc != 3)
		return CMD_RET_USAGE;

	blk = (unsigned int)simple_strtoul(argv[1], NULL, 16);
	cnt = (unsigned int)simple_strtoul(argv[2], NULL, 16);

	ret = mmc_write_protect(mmc, blk, cnt, 1);

	if (!ret)
		printf("Offset: 0x%x Count: %d blocks\nDone!\n", blk, cnt);

	watchdog_reset();

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	mmc_protect, 3, 0, do_mmc_protect,
	"MMC write protect",
	"mmc_protect start_blk cnt_blk\n"
);
#endif

#if defined (CONFIG_IPQ_SMP_CMD_SUPPORT) || (CONFIG_IPQ_SMP64_CMD_SUPPORT)
static int qti_invoke_psci_fn_smc
		(unsigned long function_id, unsigned long arg0,
		 unsigned long arg1, unsigned long arg2)
{
	struct arm_smccc_res res;
	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

int is_secondary_core_off(unsigned long cpuid)
{
#if defined (CONFIG_BASE_CPU_64BIT_BOOTUP)
	cpuid = cpuid << 8;
#endif
	return qti_invoke_psci_fn_smc(PSCI_0_2_FN_AFFINITY_INFO, cpuid, 0, 0);
}

void bring_secondary_core_down(unsigned long state)
{
	qti_invoke_psci_fn_smc(PSCI_0_2_FN_CPU_OFF, state, 0, 0);
}

int bring_secondary_core_up(unsigned long cpuid, unsigned long entry,
				unsigned long arg)
{
	int ret;
	unsigned long mpidr_cpuid = 0;
#if defined (CONFIG_BASE_CPU_64BIT_BOOTUP)
	mpidr_cpuid = cpuid << 8;
#else
	mpidr_cpuid = cpuid;
#endif
	ret = qti_invoke_psci_fn_smc(PSCI_0_2_FN_CPU_ON, mpidr_cpuid, entry,
					arg);
	if (ret) {
		printf("Enabling CPU%ld via psci failed! (ret : %d)\n",
								cpuid, ret);
		return CMD_RET_FAILURE;
	}

	printf("Enabled CPU%ld via psci successfully!\n", cpuid);
	return CMD_RET_SUCCESS;
}
#endif

#ifdef CONFIG_IPQ_SPI_NOR
struct spi_flash *ipq_spi_probe(void)
{
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct udevice *spi_dev;

	spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				&spi_dev);
	flash = dev_get_uclass_priv(spi_dev);
#else
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
#endif
	watchdog_reset();

	return flash;
}
#endif

uint64_t smem_get_flash_size(uint8_t flash_type)
{
	uint64_t flash_size = 0;

	switch(flash_type) {
	case 0: /* SPI_NOR_FLASH */
#ifdef CONFIG_IPQ_SPI_NOR
		struct spi_flash *flash = ipq_spi_probe();
		if (flash)
			flash_size = flash->size;
#endif
		break;
	case 1: /* NAND_FLASH*/
#ifdef CONFIG_IPQ_NAND
		struct mtd_info *mtd = get_nand_dev_by_index(0);
		if (mtd)
			flash_size = mtd->size;
#endif
		break;
	};

	watchdog_reset();

	return flash_size;
}

bool is_smem_part_exceed_flash_size(struct smem_ptn *p, uint64_t psize)
{
	bool ret = false;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	if (!p)
		goto exit;

	/* NOR and NOR + eMMC */
	if ((sfi->flash_type == SMEM_BOOT_SPI_FLASH) &&
		(part_which_flash(p) == 0)) {
		if (psize > smem_get_flash_size(0))
			ret = true;
	/* NAND and NOR + NAND */
	} else if ((sfi->flash_type == SMEM_BOOT_QSPI_NAND_FLASH) ||
		((sfi->flash_type == SMEM_BOOT_SPI_FLASH) &&
		(part_which_flash(p) == 1))) {
		if (psize > smem_get_flash_size(1))
			ret = true;
	}

exit:
	watchdog_reset();

	return ret;
}
/*
 * getpart_offset_size - retreive partition offset and size
 * @part_name - partition name
 * @offset - location where the offset of partition to be stored
 * @size - location where partition size to be stored
 *
 * Retreive partition offset and size in bytes with respect to the
 * partition specific flash block size
 */
int getpart_offset_size(char *part_name, uint32_t *offset, uint32_t *size)
{
	int i;
	uint32_t bsize;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	struct smem_ptable * ptable = get_ipq_part_table_info();
#ifdef CONFIG_IPQ_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return -ENODEV;
#endif
	for (i = 0; i < ptable->len; i++) {
		struct smem_ptn *p = &ptable->parts[i];
		loff_t psize;
		if (!strncmp(p->name, part_name, SMEM_PTN_NAME_MAX)) {
			bsize = get_part_block_size(p, sfi);
			if (p->size == (~0u)) {
				/*
				 * Partition size is 'till end of device',
				 * calculate appropriately
				 */
#ifdef CONFIG_IPQ_NAND
				psize = mtd->size - (((loff_t)p->start) \
								* bsize);
#else
				psize = 0;
#endif
			} else {
				psize = ((loff_t)p->size) * bsize;
			}

		*offset = ((loff_t)p->start) * bsize;
		*size = psize;
		break;
		}

		watchdog_reset();
	}

	if (i == ptable->len)
		return -ENOENT;

	watchdog_reset();

	return 0;
}

#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
gpt_entry* get_gpt_entry(struct blk_desc *dev_desc)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	int *ncount = NULL, ret = 0;
	gpt_entry **pp_gpt_pte;

	ALLOC_CACHE_ALIGN_BUFFER_PAD(gpt_header, gpt_head, 1, dev_desc->blksz);

	if (dev_desc->uclass_id == UCLASS_MMC) {
		pp_gpt_pte = &sfi->mmc_gpt_pte.gpt_pte;
		ncount = &sfi->mmc_gpt_pte.ncount;
	} else if (dev_desc->uclass_id == UCLASS_SPI) {
		pp_gpt_pte = &sfi->nor_gpt_pte.gpt_pte;
		ncount = &sfi->nor_gpt_pte.ncount;
	} else
		return NULL;

	watchdog_reset();

	if(*pp_gpt_pte)
#ifdef UPDATE_GPT_RUNTIME
		free(*pp_gpt_pte);
		*pp_gpt_pte = NULL;
#else
		return *pp_gpt_pte;
#endif

	ret = gpt_repair_headers(dev_desc);
	if (ret == 0) {
		/* This function validates
		 * AND fills in the GPT header and PTE
		 * This gpt header and pte from backup gpt in sucess case.
		 */
		ret = gpt_verify_headers(dev_desc, gpt_head, pp_gpt_pte);
		if(ret == 0 && ncount != NULL)
			*ncount = le32_to_cpu(gpt_head->num_partition_entries);
		else
			*pp_gpt_pte = NULL;
	}

	watchdog_reset();

	if(ret || !(*pp_gpt_pte))
		return NULL;
	else
		return *pp_gpt_pte;
}

static struct blk_desc *ipq_get_blk_dev(uint32_t flash_type, int devnum)
{
	enum uclass_id id;

	switch(flash_type) {
	case SMEM_BOOT_MMC_FLASH:
		id = UCLASS_MMC;
		break;
	case SMEM_BOOT_NORGPT_FLASH:
		id = UCLASS_SPI;
		break;
	default:
		printf("unsupported flash type \n");
		id = 0xFF;
	}

	return (id == 0xFF)? NULL : blk_get_devnum_by_uclass_id(id, devnum);
}

int ipq_gpt_getpart_from_offset(uint32_t offset, uint32_t *pstart,
					uint32_t *psize, uint32_t flash_type)
{
	struct blk_desc *dev = ipq_get_blk_dev(flash_type, 0);
	struct disk_partition info;
	uint32_t start = 0, size = 0;
	int p;

	if (dev == NULL)
		return -ENODEV;

	for (p = 1; p <= CONFIG_EFI_PARTITION_ENTRIES_NUMBERS; ++p) {
		if (part_get_info(dev, p, &info) != 0)
			break;

		start = (uint32_t)info.start * info.blksz;
		size = (uint32_t)info.size * info.blksz;

		if ((offset >= start) && (offset < (start + size))) {
			*pstart = start;
			*psize = size;
			return 0;
		}
	}

	return -ENODEV;
}

int ipq_part_get_info_by_name(blkpart_info_t *blkpart)
{
	struct blk_desc *dev;
	int ret;
#if defined(CONFIG_NOR_BLK) && defined(CONFIG_IPQ_NAND)
	gpt_entry *gpt_pte, *p;
#endif

	dev = ipq_get_blk_dev(blkpart->flash_type, blkpart->devnum);
	if (!dev) {
		printf("No such device \n");
		return -ENODEV;
	}

	watchdog_reset();

#ifdef CONFIG_EFI_PARTITION
	if((dev->part_type == PART_TYPE_UNKNOWN) &&
		(blkpart->flash_type == SMEM_BOOT_MMC_FLASH))
		dev->part_type = PART_TYPE_EFI;
#endif

	blkpart->desc = dev;

	ret = part_get_info_by_name(dev, blkpart->name, blkpart->info);
	if (ret < 0) {
		if (env_get("verbose"))
			printf(" %s Partition not found, ret %d !!!\n",
				blkpart->name, ret);

		watchdog_reset();

		return -ENODEV;
	}

#if defined(CONFIG_NOR_BLK) && defined(CONFIG_IPQ_NAND)
	if (blkpart->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		gpt_pte = get_gpt_entry(dev);
		if(!gpt_pte) {
			printf("Failed to get gpt table entry\n");
			return -ENOENT;
		}
		p = &gpt_pte[ret - 1];

		blkpart->isnand = gpt_find_which_flash(p);
	}
#else
	blkpart->isnand = 0;
#endif

	watchdog_reset();

	return 0;
}
#endif

void update_nand_training_partition(ipq_smem_flash_info_t *sfi)
{
	uint32_t offset, part_size;
	int ret = -1;
	ipq_part_entry_t *part = &sfi->training;
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info;
	blkpart_info_t  bpart_info;

	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		BLK_PART_GET_INFO_S(bpart_info, "0:TRAINING", &disk_info,
					sfi->flash_type);

		ret = ipq_part_get_info_by_name(&bpart_info);
	} else
#endif
		if (sfi->flash_type != SMEM_BOOT_NO_FLASH)
			ret = getpart_offset_size("0:TRAINING", &offset,
							&part_size);
	if (ret) {
		part->offset = 0xBAD0FF5E;
		part->size = 0xBAD0FF5E;
	} else {
#if defined(CONFIG_NOR_BLK)
		if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
			part->offset = (u32)disk_info.start * disk_info.blksz;
			part->size = (u32)disk_info.size * disk_info.blksz;
		} else
#endif
		{
			part->offset = offset;
			part->size = part_size;
		}
	}

	watchdog_reset();
}


int ipq_get_training_part_info(uint32_t *offset, uint32_t *size)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	ipq_part_entry_t *part = &sfi->training;

	if (part->offset == 0)
		update_nand_training_partition(sfi);

	if (part->offset == 0xBAD0FF5E)
		return 1;
	else {
		*offset = part->offset;
		*size = part->size;
	}

	watchdog_reset();

	return 0;
}
#ifdef CONFIG_IPQ_NAND
uint32_t get_nand_block_size(uint8_t dev_id)
{
	uint32_t block_size = 0;
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if (mtd)
		block_size = mtd->erasesize;
	return block_size;
}
#endif

uint32_t get_part_block_size(struct smem_ptn *p, ipq_smem_flash_info_t *sfi)
{
#ifdef CONFIG_IPQ_NAND
        return (part_which_flash(p) == 1) ?
		get_nand_block_size(0)
		: sfi->flash_block_size;
#else
	return sfi->flash_block_size;
#endif
}
/*
 * get flash block size based on partition name.
 */
static inline uint32_t get_flash_block_size(char *name,
					    ipq_smem_flash_info_t *smem)
{
#ifdef CONFIG_IPQ_NAND
	return (get_which_flash_param(name) == 1) ?
		get_nand_block_size(0)
		: smem->flash_block_size;
#else
	return smem->flash_block_size;
#endif
}

void get_kernel_fs_part_details(int flash_type)
{
	int ret, i;
	uint32_t start;         /* block number */
	uint32_t size;          /* no. of blocks */
	uint32_t bsize;
	ipq_part_entry_t *part;
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info;
	blkpart_info_t  bpart_info;
#endif
	ipq_smem_flash_info_t *smem = get_ipq_smem_flash_info();
#ifdef CONFIG_BOOTCONFIG_V3
	int active_part = (gd->board_type & ACTIVE_BOOT_SET) ? 1: 0;
#endif

	struct { char *name; ipq_part_entry_t *part; } entries[] = {
#ifdef CONFIG_BOOTCONFIG_V3
		{ active_part == 1 ? "0:HLOS_1" : "0:HLOS", &smem->hlos },
		{ active_part == 1 ? "rootfs_1" : "rootfs", &smem->rootfs },
#else
		{ "0:HLOS", &smem->hlos },
		{ "rootfs", &smem->rootfs },
#endif
	};

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		part = entries[i].part;
#if defined(CONFIG_NOR_BLK)
		if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
#if CONFIG_BOOTCONFIG_V3
		if (!strncmp(entries[i].name,
			active_part ? "0:HLOS_1" : "0:HLOS",
			active_part ? 8 :6))
#else
		if (!strncmp(entries[i].name, "0:HLOS", 6)
#endif
				continue;

			BLK_PART_GET_INFO_S(bpart_info, entries[i].name,
						&disk_info, flash_type);

			ret = ipq_part_get_info_by_name(&bpart_info);
		} else
#endif
		{
			ret = smem_getpart(entries[i].name, &start, &size);
		}

		if (ret) {
			debug("cdp: get part failed for %s\n",
				entries[i].name);
			part->offset = 0xBAD0FF5E;
			part->size = 0xBAD0FF5E;
		} else {
#if defined(CONFIG_NOR_BLK)
			if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
				bsize = disk_info.blksz;
				part->offset = (u32)disk_info.start * bsize;
				part->size = (u32)disk_info.size * bsize;
			} else
#endif
			{
				bsize = get_flash_block_size(entries[i].name,
								smem);
				part->offset = ((loff_t)start) * bsize;
				part->size = ((loff_t)size) * bsize;
			}
		}

		watchdog_reset();
	}

	return;
}
/*
 * smem_getpart - retreive partition start and size
 * @part_name: partition name
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * Retreive the start offset in blocks and size in blocks, of the
 * specified partition.
 */
int smem_getpart(char *part_name, uint32_t *start, uint32_t *size)
{
	unsigned i;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	struct smem_ptable *ptable = get_ipq_part_table_info();
	struct smem_ptn *p;
	uint32_t bsize;
#ifdef CONFIG_IPQ_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return -ENODEV;
#endif
	if (!ptable)
		return -ENODEV;

	for (i = 0; i < ptable->len; i++) {
		if (!strncmp(ptable->parts[i].name, part_name,
			     SMEM_PTN_NAME_MAX))
			break;
	}
	if (i == ptable->len)
		return -ENOENT;

	p = &ptable->parts[i];
	bsize = get_part_block_size(p, sfi);

	*start = p->start;

	if (p->size == (~0u)) {
		/*
		 * Partition size is 'till end of device', calculate
		 * appropriately
		 */
#ifdef CONFIG_IPQ_NAND
		*size = (mtd->size / bsize) - p->start;
#else
		*size = 0;
		bsize = bsize;
#endif
	} else {
		*size = p->size;
	}

	watchdog_reset();

	return 0;
}

/*
 * smem_getpart_from_offset - retreive partition start and size for given offset
 * belongs to.
 * @part_name: offset for which part start and size needed
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * Returns 0 at success or -ENOENT otherwise.
 */
int smem_getpart_from_offset(uint32_t offset, uint32_t *start, uint32_t *size)
{
	unsigned i;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	struct smem_ptable *ptable = get_ipq_part_table_info();
	struct smem_ptn *p;
	uint32_t bsize;
#ifdef CONFIG_IPQ_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return -ENODEV;
#endif

	if (!ptable)
		return -ENODEV;

	for (i = 0; i < ptable->len; i++) {
		p = &ptable->parts[i];
		bsize = get_part_block_size(p, sfi);
		*start = p->start;

		if (p->size == (~0u)) {
		/*
		 * Partition size is 'till end of device', calculate
		 * appropriately
		 */
#ifdef CONFIG_IPQ_NAND
			*size = (mtd->size / bsize) - p->start;
#else
			*size = 0;
			bsize = bsize;
#endif
		} else {
			*size = p->size;
		}
		*start = *start * bsize;
		*size = *size * bsize;
		if (*start <= offset && *start + *size > offset) {
			return 0;
		}
	}

	watchdog_reset();

	return -ENOENT;
}

#ifdef CONFIG_CMD_UBI
int init_ubi_part(void)
{
	int ret;
	uint32_t offset = 0;
	uint32_t part_size = 0;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	struct ubi_device *ubi = ubi_get_device(0);
	char env_strings[64];

	watchdog_reset();

	if(ubi == NULL) {
		offset = sfi->rootfs.offset;
		part_size = sfi->rootfs.size;

		if ((part_size == 0xBAD0FF5E) || (offset == 0xBAD0FF5E))
			return -ENOENT;

		snprintf(env_strings, sizeof(env_strings),
			"mtdparts=nand0:0x%x@0x%x(%s)", part_size, offset,
				CFG_UBI_FS_NAME);

		ret = env_set("mtdparts", env_strings);
		if (ret)
			return -EPERM;

		ret = ubi_part(CFG_UBI_FS_NAME, NULL);
		if (ret)
			return -EPERM;
	} else
		ubi_put_device(ubi);

	return 0;
}
#endif

static void ipq_set_part_entry(char *name, ipq_smem_flash_info_t *smem,
				ipq_part_entry_t *part, uint32_t start,
				uint32_t size)
{
	uint32_t bsize = get_flash_block_size(name, smem);
	part->offset = ((loff_t)start) * bsize;
	part->size = ((loff_t)size) * bsize;
}

int get_partition_data(char *part_name, uint32_t offset, uint8_t* buf,
			size_t size, uint32_t fl_type)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	int flash_type, ret = 0, isnand = 0;
#ifdef CONFIG_IPQ_SPI_NOR
	struct spi_flash *flash = NULL;
#endif
	uint32_t start_blk;
	uint32_t blk_cnt;
	ipq_part_entry_t part;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	blkpart_info_t bpart_info;
	struct disk_partition disk_info;
	uint32_t start_blk_no, end_blk_no, blksz;
#if defined(CONFIG_MMC)
	struct mmc *mmc;
	unsigned char *mmc_blk = NULL;
	int i, rdatacnt = 0, buf_cur_pos = 0;
#endif
#endif

	memset(&part, 0, sizeof(ipq_part_entry_t));

	if ((sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) &&
		((fl_type == SMEM_BOOT_QSPI_NAND_FLASH) ||
		(fl_type == SMEM_BOOT_NAND_FLASH))) {
			flash_type = SMEM_BOOT_NORGPT_FLASH;
			isnand = 1;
	} else {
		flash_type = fl_type;
	}

	watchdog_reset();

	switch(flash_type) {
	case SMEM_BOOT_NAND_FLASH:
	case SMEM_BOOT_QSPI_NAND_FLASH:
	case SMEM_BOOT_SPI_FLASH:
		ret = smem_getpart(part_name, &start_blk, &blk_cnt);
		if (ret < 0) {
			debug("cdp: get part failed for %s\n",
					part_name);
			ret = -ENXIO;
			goto exit;
		} else {
			ipq_set_part_entry(part_name, sfi, &part, start_blk,
						blk_cnt);
			part.offset += offset;
		}
		break;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
#if defined(CONFIG_MMC)
	case SMEM_BOOT_MMC_FLASH:
		mmc = find_mmc_device(0);
		if (!mmc) {
			printf("Failed to find MMC device \n");
			ret = -ENODEV;
			break;
		}
#endif
	case SMEM_BOOT_NORGPT_FLASH:
		BLK_PART_GET_INFO_S(bpart_info, part_name, &disk_info,
					flash_type);
		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret)
			goto exit;

		blksz = disk_info.blksz;

		if (bpart_info.isnand || isnand) {
			blksz = disk_info.blksz;
			part.offset = disk_info.start * blksz;
			part.offset += offset;
			flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
			break;
		}

		start_blk_no = (uint32_t) disk_info.start + (offset / blksz);
		end_blk_no = (uint32_t) disk_info.start +
				((offset + size) / blksz);

		if ((offset == 0) && (size % blksz == 0)) {
#ifdef CONFIG_BLK
			ret = blk_dread(bpart_info.desc, start_blk_no,
						size / blksz, buf);
			if (ret < 0) {
				printf("Blk read failed %d \n", ret);
				break;
			} else
				goto exit;
#endif
		}

		if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
			part.offset = disk_info.start * blksz;
			part.offset += offset;
			flash_type =  SMEM_BOOT_SPI_FLASH;
			break;
		}

#if defined(CONFIG_MMC)
		mmc_blk = (unsigned char*) malloc_cache_aligned(blksz);
		if (mmc_blk == NULL)
			return -ENOMEM;

		rdatacnt = size;
		for (i = start_blk_no; i <= end_blk_no; i++) {
#ifdef CONFIG_BLK
			ret = blk_dread(bpart_info.desc, i, 1, mmc_blk);
#else
			ret = mmc->block_dev.block_read(&mmc->block_dev,
						i, 1, mmc_blk);
#endif
			if (ret < 0) {
				printf("MMC: %s read failed %d\n", part_name,
					ret);
				break;
			}

			if (i == start_blk_no) {
				if (size <= blksz) {
					memcpy(buf, mmc_blk + (offset % blksz),
						size);
					if (start_blk_no == end_blk_no)
						break;
				} else {
					memcpy(buf, mmc_blk + (offset % blksz),
						blksz - (offset % blksz));
					buf_cur_pos +=
						(blksz - (offset % blksz));
					rdatacnt -= (blksz - (offset % blksz));
				}
			} else if (rdatacnt >= blksz) {
				memcpy(buf + buf_cur_pos, mmc_blk, blksz);
				rdatacnt -= blksz;
				buf_cur_pos += blksz;
			} else
				memcpy(buf + buf_cur_pos, mmc_blk, rdatacnt);
		}

		if (mmc_blk) {
			free(mmc_blk);
			mmc_blk = NULL;
		}
#endif
		break;
#endif
	default:
		printf("Unsupported BOOT flash type\n");
		ret = -ENXIO;
		break;
	}

	watchdog_reset();

#ifdef CONFIG_IPQ_SPI_NOR
	if ((flash_type == SMEM_BOOT_SPI_FLASH) ||
		(flash_type == SMEM_BOOT_NORGPT_FLASH)) {
		flash = ipq_spi_probe();
		if (flash == NULL){
			printf("No SPI flash device found\n");
			ret = -ENODEV;
		} else {
			ret = spi_flash_read(flash, part.offset, size, buf);
		}
	}
#endif
#ifdef CONFIG_IPQ_NAND
	if ((flash_type == SMEM_BOOT_NAND_FLASH) ||
		(flash_type == SMEM_BOOT_QSPI_NAND_FLASH)) {
		struct mtd_info *mtd = get_nand_dev_by_index(0);
		if (!mtd) {
			printf("No NAND flash device found\n");
			ret = -ENODEV;
		} else {
			ret = nand_read(mtd, part.offset, &size, buf);
		}
	}
#endif

exit:

#if defined(CONFIG_MMC) && defined(CONFIG_SYS_MMC_ENV_PART)
	if (mmc_blk) {
		free(mmc_blk);
		mmc_blk = NULL;
	}
#endif

	watchdog_reset();

	return ret;
}

int get_rootfs_active_partition(ipq_smem_flash_info_t *sfi)
{
	ipq_smem_bootconfig_info_t *binfo = NULL;
	int ret = 0;

	if (sfi != NULL)
		binfo = sfi->ipq_smem_bootconfig_info;

	if (binfo == NULL)
		return 0;

#ifdef CONFIG_BOOTCONFIG_V2
	for (int i = 0; i < binfo->numaltpart; i++) {
		if (strncmp("rootfs", binfo->per_part_entry[i].name,
			CONFIG_RAM_PART_NAME_LENGTH) == 0) {

			ret = binfo->per_part_entry[i].primaryboot;
		}
	}
#elif CONFIG_BOOTCONFIG_V3
	uint32_t *image_set_status = &(binfo->image_set_status);
	uint32_t *boot_set = &(binfo->boot_set);


	if (*image_set_status == DONT_USE_SET_AB) {
		ret = BOOT_SET_INVALID;
	} else if (BOOT_SET_A == *boot_set) {
		if(DONT_USE_SET_A == *image_set_status) {
			ret = BOOT_SET_B;
		} else {
			ret = *boot_set;
		}
	} else if (BOOT_SET_B == *boot_set) {
		if (DONT_USE_SET_B == *image_set_status) {
			ret = BOOT_SET_A;
		} else {
			ret = *boot_set;
		}
	}

	if ((*image_set_status != DONT_USE_SET_AB) &&
		(sfi->try_mode_inprogress))
		ret = !ret;

	printf("Booting [SET %s]\n", ret ? "B" : "A");
#endif

	return ret;
}

uint32_t cal_bootconf_crc(ipq_smem_bootconfig_info_t *binfo)
{
	uint32_t size = 0, crc = 0;

#ifdef CONFIG_BOOTCONFIG_V3
	size = sizeof(ipq_smem_bootconfig_info_t) - sizeof(binfo->crc);
#else
	size = sizeof(ipq_smem_bootconfig_info_t) - sizeof(binfo->magic_end);
#endif

#ifdef CONFIG_CRC32_BE
	crc = crc32_be((const unsigned char *)(uintptr_t)binfo, size);
#endif
	if (env_get("verbose"))
		printf("Calculated Bootconfig CRC = 0x%x\n", crc);

	return crc;
}

uint8_t is_valid_bootconfig(ipq_smem_bootconfig_info_t *binfo)
{
#ifdef CONFIG_BOOTCONFIG_V2
	if (IS_ERR_OR_NULL(binfo) ||
		((binfo->magic_start != _SMEM_DUAL_BOOTINFO_MAGIC_START) &&
		(binfo->magic_start != _SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_MODE) &&
		(binfo->magic_start != _SMEM_DUAL_BOOTINFO_MAGIC_START_UNIFIED_FAILSAFE) &&
		(binfo->magic_start != _SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_UNIFIED_FAILSAFE)) ||
		(binfo->magic_end != _SMEM_DUAL_BOOTINFO_MAGIC_END)) {

		return 0;
	}
#elif CONFIG_BOOTCONFIG_V3
	if (IS_ERR_OR_NULL(binfo) || (!binfo->crc) ||
		(cal_bootconf_crc(binfo) != binfo->crc)) {

		return 0;
	}
#endif
	return 1;
}

#ifdef CONFIG_CRC32_BE
#define DO_CRC_BE(x) crc = tab[ ((crc >> 24) ^ (x)) & 255] ^ (crc<<8)

static const u32 crc32table_be[] = {
	(0x00000000L), (0x04c11db7L), (0x09823b6eL), (0x0d4326d9L),
	(0x130476dcL), (0x17c56b6bL), (0x1a864db2L), (0x1e475005L),
	(0x2608edb8L), (0x22c9f00fL), (0x2f8ad6d6L), (0x2b4bcb61L),
	(0x350c9b64L), (0x31cd86d3L), (0x3c8ea00aL), (0x384fbdbdL),
	(0x4c11db70L), (0x48d0c6c7L), (0x4593e01eL), (0x4152fda9L),
	(0x5f15adacL), (0x5bd4b01bL), (0x569796c2L), (0x52568b75L),
	(0x6a1936c8L), (0x6ed82b7fL), (0x639b0da6L), (0x675a1011L),
	(0x791d4014L), (0x7ddc5da3L), (0x709f7b7aL), (0x745e66cdL),
	(0x9823b6e0L), (0x9ce2ab57L), (0x91a18d8eL), (0x95609039L),
	(0x8b27c03cL), (0x8fe6dd8bL), (0x82a5fb52L), (0x8664e6e5L),
	(0xbe2b5b58L), (0xbaea46efL), (0xb7a96036L), (0xb3687d81L),
	(0xad2f2d84L), (0xa9ee3033L), (0xa4ad16eaL), (0xa06c0b5dL),
	(0xd4326d90L), (0xd0f37027L), (0xddb056feL), (0xd9714b49L),
	(0xc7361b4cL), (0xc3f706fbL), (0xceb42022L), (0xca753d95L),
	(0xf23a8028L), (0xf6fb9d9fL), (0xfbb8bb46L), (0xff79a6f1L),
	(0xe13ef6f4L), (0xe5ffeb43L), (0xe8bccd9aL), (0xec7dd02dL),
	(0x34867077L), (0x30476dc0L), (0x3d044b19L), (0x39c556aeL),
	(0x278206abL), (0x23431b1cL), (0x2e003dc5L), (0x2ac12072L),
	(0x128e9dcfL), (0x164f8078L), (0x1b0ca6a1L), (0x1fcdbb16L),
	(0x018aeb13L), (0x054bf6a4L), (0x0808d07dL), (0x0cc9cdcaL),
	(0x7897ab07L), (0x7c56b6b0L), (0x71159069L), (0x75d48ddeL),
	(0x6b93dddbL), (0x6f52c06cL), (0x6211e6b5L), (0x66d0fb02L),
	(0x5e9f46bfL), (0x5a5e5b08L), (0x571d7dd1L), (0x53dc6066L),
	(0x4d9b3063L), (0x495a2dd4L), (0x44190b0dL), (0x40d816baL),
	(0xaca5c697L), (0xa864db20L), (0xa527fdf9L), (0xa1e6e04eL),
	(0xbfa1b04bL), (0xbb60adfcL), (0xb6238b25L), (0xb2e29692L),
	(0x8aad2b2fL), (0x8e6c3698L), (0x832f1041L), (0x87ee0df6L),
	(0x99a95df3L), (0x9d684044L), (0x902b669dL), (0x94ea7b2aL),
	(0xe0b41de7L), (0xe4750050L), (0xe9362689L), (0xedf73b3eL),
	(0xf3b06b3bL), (0xf771768cL), (0xfa325055L), (0xfef34de2L),
	(0xc6bcf05fL), (0xc27dede8L), (0xcf3ecb31L), (0xcbffd686L),
	(0xd5b88683L), (0xd1799b34L), (0xdc3abdedL), (0xd8fba05aL),
	(0x690ce0eeL), (0x6dcdfd59L), (0x608edb80L), (0x644fc637L),
	(0x7a089632L), (0x7ec98b85L), (0x738aad5cL), (0x774bb0ebL),
	(0x4f040d56L), (0x4bc510e1L), (0x46863638L), (0x42472b8fL),
	(0x5c007b8aL), (0x58c1663dL), (0x558240e4L), (0x51435d53L),
	(0x251d3b9eL), (0x21dc2629L), (0x2c9f00f0L), (0x285e1d47L),
	(0x36194d42L), (0x32d850f5L), (0x3f9b762cL), (0x3b5a6b9bL),
	(0x0315d626L), (0x07d4cb91L), (0x0a97ed48L), (0x0e56f0ffL),
	(0x1011a0faL), (0x14d0bd4dL), (0x19939b94L), (0x1d528623L),
	(0xf12f560eL), (0xf5ee4bb9L), (0xf8ad6d60L), (0xfc6c70d7L),
	(0xe22b20d2L), (0xe6ea3d65L), (0xeba91bbcL), (0xef68060bL),
	(0xd727bbb6L), (0xd3e6a601L), (0xdea580d8L), (0xda649d6fL),
	(0xc423cd6aL), (0xc0e2d0ddL), (0xcda1f604L), (0xc960ebb3L),
	(0xbd3e8d7eL), (0xb9ff90c9L), (0xb4bcb610L), (0xb07daba7L),
	(0xae3afba2L), (0xaafbe615L), (0xa7b8c0ccL), (0xa379dd7bL),
	(0x9b3660c6L), (0x9ff77d71L), (0x92b45ba8L), (0x9675461fL),
	(0x8832161aL), (0x8cf30badL), (0x81b02d74L), (0x857130c3L),
	(0x5d8a9099L), (0x594b8d2eL), (0x5408abf7L), (0x50c9b640L),
	(0x4e8ee645L), (0x4a4ffbf2L), (0x470cdd2bL), (0x43cdc09cL),
	(0x7b827d21L), (0x7f436096L), (0x7200464fL), (0x76c15bf8L),
	(0x68860bfdL), (0x6c47164aL), (0x61043093L), (0x65c52d24L),
	(0x119b4be9L), (0x155a565eL), (0x18197087L), (0x1cd86d30L),
	(0x029f3d35L), (0x065e2082L), (0x0b1d065bL), (0x0fdc1becL),
	(0x3793a651L), (0x3352bbe6L), (0x3e119d3fL), (0x3ad08088L),
	(0x2497d08dL), (0x2056cd3aL), (0x2d15ebe3L), (0x29d4f654L),
	(0xc5a92679L), (0xc1683bceL), (0xcc2b1d17L), (0xc8ea00a0L),
	(0xd6ad50a5L), (0xd26c4d12L), (0xdf2f6bcbL), (0xdbee767cL),
	(0xe3a1cbc1L), (0xe760d676L), (0xea23f0afL), (0xeee2ed18L),
	(0xf0a5bd1dL), (0xf464a0aaL), (0xf9278673L), (0xfde69bc4L),
	(0x89b8fd09L), (0x8d79e0beL), (0x803ac667L), (0x84fbdbd0L),
	(0x9abc8bd5L), (0x9e7d9662L), (0x933eb0bbL), (0x97ffad0cL),
	(0xafb010b1L), (0xab710d06L), (0xa6322bdfL), (0xa2f33668L),
	(0xbcb4666dL), (0xb8757bdaL), (0xb5365d03L), (0xb1f740b4L)
};

uint32_t crc32_be(uint8_t const *addr, phys_size_t size) {

	uint32_t i, crc = 0;
	const uint32_t *tab = crc32table_be;

	for (i = 0; i < size; ++i) {
		DO_CRC_BE(*addr++);
	}

	return crc;
}
#endif

int write_tcsr_boot_misc_reg(uint32_t mask, uint32_t value) {

	int ret = -1;
	scm_param param;
	unsigned int cookie = ipq_read_tcsr_boot_misc();

	cookie = (cookie & ~mask) | (value & mask);

	if (g_recovery_path) {
		writel(cookie, TCSR_BOOT_MISC_REG);
	} else {
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_WRITE(param, (uintptr_t)TCSR_BOOT_MISC_REG,
						cookie);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("Error in TCSR_BOOT_MISC_REG write\n");
				return ret;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
		}
	}

	return ret;
}

#ifdef CONFIG_GPIO_CONFIG
/*
 * NOP driver: only for GPIO configuration
 */
static const struct udevice_id gpio_ids[] = {
	{ .compatible = "gpio, config", },
	{ }
};

U_BOOT_DRIVER(gpio) = {
	.name		= "gpio",
	.id		= UCLASS_NOP,
	.of_match	= gpio_ids,
};

void ipq_board_gpio_config(int type)
{
	switch(type) {
#ifdef CONFIG_SDX_ATTACH_SUPPORT
	case SDX_POWER_CYCLE:
		ipq_board_power_cycle_sdx();
		break;
#endif
	default:
		break;
	}
}
#endif

#ifdef CONFIG_WDT
int ipq_wdt_expire(void) {
	int ret = 0;
	static struct udevice *wdt_dev;

	ret = uclass_get_device_by_seq(UCLASS_WDT, 0, &wdt_dev);
	if (ret) {
		printf("WDT disabled\n");
		return -ENODEV;
	}

	wdt_expire_now(wdt_dev, 0);
	return 0;
}
#endif

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
int initr_net(void)
{
	if (g_eth_initalized == 0) {
		puts("Net:   ");
		eth_initialize();
		g_eth_initalized = 1;
	} else {
		return 1;
	}

	return 0;
}
#endif
