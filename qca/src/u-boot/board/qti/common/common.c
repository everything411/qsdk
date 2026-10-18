/*
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#endif
#include <asm/io.h>
#include <linux/iopoll.h>
#include <gzip.h>
#include <rand.h>
#include <time.h>

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

#ifdef CONFIG_IPQ_PCIE
struct pci_device_id device_table[] = {
	{QCN_VENDOR_ID, QCN9224_DEVICE_ID},
	{}
};
#endif

extern int ubi_volume_read(char *volume, char *buf, size_t size);

extern struct list_head *lhead;
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

	if (gd->board_type & RECOVERY_MODE) {
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
#ifdef CONFIG_IPQ_PCIE
void pci_select_window(uintptr_t base, u32 offset)
{
	u32 window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;
	u32 prev_window = 0, curr_window = 0, prev_cleared_window = 0;

	prev_window = readl(base + QCN9224_PCIE_REMAP_BAR_CTRL_OFFSET);

	/* Clear out last 6 bits of window register */
	prev_cleared_window = prev_window & ~(0x3f);

	/* Write the new last 6 bits of window register. Only window 1 values
	 * are changed. Window 2 and 3 are unaffected.
	 */
	curr_window = prev_cleared_window | window;

	writel(WINDOW_ENABLE_BIT | curr_window, base +
			QCN9224_PCIE_REMAP_BAR_CTRL_OFFSET);
}

void print_error_code(pci_addr_t addr, bool pbl_log)
{
	int i;
	u32 val;
	struct {
		char *name;
		u32 offset;
	} error_reg[] = {
		{ "ERROR_CODE", BHI_ERRCODE },
		{ "ERROR_DBG1", BHI_ERRDBG1 },
		{ "ERROR_DBG2", BHI_ERRDBG2 },
		{ "ERROR_DBG3", BHI_ERRDBG3 },
		{ "BHI_STATUS", BHI_STATUS },
		{ "BHI_IMGADDR_HIGH", BHI_IMGADDR_HIGH },
		{ "BHI_IMGADDR_LOW", BHI_IMGADDR_LOW },
		{ "BHI_IMGSIZE", BHI_IMGSIZE },
		{ "BHI_IMGTXDB", BHI_IMGTXDB },
		{ "PCIE_LOCAL_RSV0", PCIE_LOCAL_RSV0 },
		{ NULL },
	};

	for (i = 0; error_reg[i].name; i++) {
		val = readl(addr + error_reg[i].offset);
		printf("Reg: %s value: 0x%x\n", error_reg[i].name, val);
	}
	if (pbl_log) {
		pci_select_window(addr, QCN9224_TCSR_PBL_LOGGING_REG);
		val = readl(addr + WINDOW_START +
				(QCN9224_TCSR_PBL_LOGGING_REG &
					WINDOW_RANGE_MASK));
		printf("Reg: TCSR_PBL_LOGGING: 0x%x\n", val);
	}
}

void qcn92xx_global_soc_reset(uintptr_t bar0_base, bool force_reset)
{
	u32 val, ret, count = 0;
	uintptr_t reg;

	do {
		watchdog_reset();
		reg = bar0_base + PCIE_SOC_GLOBAL_RESET_ADDRESS;
		if (force_reset) {
			writel(PCIE_SOC_GLOBAL_RESET_FORCE_RESET_VALUE, reg);
			mdelay(1000);
		} else {
			writel(PCIE_SOC_GLOBAL_RESET_VALUE, reg);
		}
		reg = bar0_base + BHI_EXECENV;
		ret = readl_poll_sleep_timeout(reg, val, val == 0, 1 * 1000,
					       20 * 1000);
		if (ret == 0)
			break;

		++count;
	} while (count < MAX_SOC_GLOBAL_RESET_WAIT_CNT);

	if (val != 0)
		printf("SoC global reset failed! Reset count : %d\n", count);
}
#endif

#ifdef CONFIG_CB_CALIB
static int do_cal_qcn9224(struct cal_config *cfg, int index, int debug)
{
	struct udevice *dev;
	uintptr_t bar0_base, reg, load_addr = (uintptr_t)NULL;
	struct file_info *file;
	int ret, i, version, val;
	ulong vendor, device;
	struct uboot_cal_tlv *tlv = NULL;
	void *cal_fw, *bdf_addr, *regdb_addr, *rxgain_addr = NULL;
	void *fw_ini_addr = NULL;
	u32 cal_fw_size, bdf_size, rxgain_size = 0, regdb_size, fw_ini_size = 0;
	struct cal_fw_header *cal_fw_header;
	void *cal_fw_image;
	struct cal_per_dev_config *dev_cfg;
	int num_macs;

	if (!cfg)
		return -EINVAL;

	dev_cfg = &cfg->dev_cfg[index];

	dev = dev_cfg->dev;
	printf("%s: %d: Starting cal for %s index  %d slot %d\n", __func__, __LINE__,
		dev->name, index, dev_cfg->pci_slot_id);

	/* This slot doesn't have any EP attached */
	if (!dev)
		return 0;

	dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0, (u32 *)&bar0_base);
	bar0_base &= 0xFFF00000;

	dm_pci_read_config(dev, PCI_VENDOR_ID, &vendor, PCI_SIZE_16);
	dm_pci_read_config(dev, PCI_DEVICE_ID, &device, PCI_SIZE_16);

	/* Read QCN9224 version */
	pci_select_window(bar0_base, QCN9224_TCSR_SOC_HW_VERSION);

	version = readl(bar0_base + WINDOW_START +
			(QCN9224_TCSR_SOC_HW_VERSION & WINDOW_RANGE_MASK));

	version = (version & QCN9224_TCSR_SOC_HW_VERSION_MASK) >>
					QCN9224_TCSR_SOC_HW_VERSION_SHIFT;

	if (version == 1) {
		printf("Calibration not supported in QCN9224 V1 in slot %d\n",
		       dev_cfg->pci_slot_id);
		return 0;
	}

	cal_fw_header = cfg->cal_fw_header;
	cal_fw_image = (void *)(uintptr_t)cfg->cal_fw_header;

	if (debug) {
		for (i = 0; i < cal_fw_header->num_files; i++) {
			file = &cal_fw_header->file[i];
			printf("type: %x subtype: %x offset: %x size: %x\n",
			       file->type, file->sub_type,
			       file->offset, file->size);
		}
	}

	printf("%s: %d: board_id: 0x%x \n", __func__, __LINE__, dev_cfg->board_id);
	num_macs = ((dev_cfg->board_id & 0xF000) > 0xC) + 1;

	for (i = 0; i < cal_fw_header->num_files; i++) {
		file = &cal_fw_header->file[i];
		switch (file->type) {
		case CAL_FW:
			if (file->sub_type == 2 && file->sub_type == num_macs) {
				printf("Loading %d mac calfw for pci slot: %d\n",
				       num_macs, dev_cfg->pci_slot_id);
				memcpy(((void *)(uintptr_t)dev_cfg->cal_fw_image_addr),
				       (void *)(uintptr_t)(cal_fw_image + file->offset),
				       file->size);
				cal_fw = (void *)(uintptr_t)dev_cfg->cal_fw_image_addr;
				cal_fw_size = file->size;
				load_addr = (uintptr_t)cal_fw;
			} else if (file->sub_type == 1 && file->sub_type == num_macs) {
				printf("Loading %d mac calfw for pci slot: %d\n",
				       num_macs, dev_cfg->pci_slot_id);
				memcpy(((void *)(uintptr_t)dev_cfg->cal_fw_image_addr),
				       (void *)(uintptr_t)(cal_fw_image + file->offset),
				       file->size);
				cal_fw = (void *)(uintptr_t)dev_cfg->cal_fw_image_addr;
				cal_fw_size = file->size;
				load_addr = (uintptr_t)cal_fw;
			}
			break;
		case BDF:
			if (file->sub_type == dev_cfg->board_id) {
				bdf_addr = cal_fw_image + file->offset;
				bdf_size = file->size;
			}
			break;
		case RXGAIN:
			rxgain_addr = cal_fw_image + file->offset;
			rxgain_size = file->size;
			break;
		case REGDB:
			regdb_addr = cal_fw_image + file->offset;
			regdb_size = file->size;
			break;
		case FW_INI_CFG:
			fw_ini_addr = cal_fw_image + file->offset;
			fw_ini_size = file->size;
			break;
		default:
			printf("Unsupported image type: %d\n", file->type);
			break;
		}
	}

	if (!load_addr) {
		printf("CAL_FW not found\n");
		return -EINVAL;
	}

	/* FILL TLV */
	tlv = (struct uboot_cal_tlv *)memalign(SZ_4K, sizeof(struct uboot_cal_tlv));
	if (!tlv) {
		printf("Failed to allocate memory for cal tlv\n");
		return -ENOMEM;
	}

	memset(tlv, 0, sizeof(struct uboot_cal_tlv));
	tlv->magic = 0xCAFECACE;
	tlv->pci_slot = dev_cfg->pci_slot_id;
	tlv->caldb_addr = dev_cfg->caldb_addr;
	tlv->caldb_size = dev_cfg->caldb_size;
	tlv->host_ddr_status = dev_cfg->host_ddr_status;
	tlv->hremote_addr = dev_cfg->hremote_addr;
	tlv->hremote_size = dev_cfg->hremote_size;
	tlv->rddm_addr = dev_cfg->rddm_addr;
	tlv->rddm_size = dev_cfg->rddm_size;
	tlv->num_images = MAX_IMG_TYPE;

	for (i = 0; i < MAX_IMG_TYPE; i++) {
		switch (i) {
		case BDF:
			tlv->img[i].img_type = i;
			tlv->img[i].img_sram_addr = 0;
			tlv->img[i].img_host_addr = (u32)(uintptr_t)(bdf_addr);
			tlv->img[i].img_size = bdf_size;
			break;
		case CALDATA:
			tlv->img[i].img_type = i;
			tlv->img[i].img_sram_addr = 0;
			memcpy(((void *)(uintptr_t)dev_cfg->host_ddr_status + SZ_4K),
			       (void *)(uintptr_t)(cfg->caldata_addr + dev_cfg->caldata_offset),
			       dev_cfg->caldata_size);
			tlv->img[i].img_host_addr = dev_cfg->host_ddr_status + SZ_4K;
			tlv->img[i].img_size = dev_cfg->caldata_size;
			break;
		case RXGAIN:
			tlv->img[i].img_type = i;
			tlv->img[i].img_sram_addr = 0;
			tlv->img[i].img_host_addr = (u32)(uintptr_t)rxgain_addr;
			tlv->img[i].img_size = rxgain_size;
			break;
		case REGDB:
			tlv->img[i].img_type = i;
			tlv->img[i].img_sram_addr = 0;
			tlv->img[i].img_host_addr = (u32)(uintptr_t)regdb_addr;
			tlv->img[i].img_size = regdb_size;
			break;
		case FW_INI_CFG:
			tlv->img[i].img_type = i;
			tlv->img[i].img_sram_addr = 0;
			tlv->img[i].img_host_addr = (u32)(uintptr_t)fw_ini_addr;
			tlv->img[i].img_size = fw_ini_size;
			break;
		default:
			break;
		}
	}

	printf("Starting calibration on Vendor ID:0x%lx device ID:0x%lx slot:%d cal_fw_size:%d cal_fw: 0x%lx 0x%p\n",
	       vendor, device, dev_cfg->pci_slot_id, cal_fw_size, load_addr, tlv);

	if (debug) {
		printf("Dumping TLV:\n");
		printf("magic: 0x%x\n", tlv->magic);
		printf("pci_slot: 0x%x\n", tlv->pci_slot);
		printf("caldb_addr: 0x%x\n", tlv->caldb_addr);
		printf("caldb_size: 0x%x\n", tlv->caldb_size);
		printf("host_ddr_status: 0x%x\n", tlv->host_ddr_status);
		printf("hremote_addr: 0x%x\n", tlv->hremote_addr);
		printf("hremote_size: 0x%x\n", tlv->hremote_size);
		printf("rddm_addr: 0x%x\n", tlv->rddm_addr);
		printf("rddm_size: 0x%x\n", tlv->rddm_size);
		printf("num_images: 0x%x\n", tlv->num_images);

		for (i = 0; i < MAX_IMG_TYPE; i++) {
			printf("img_type:0x%x\n", tlv->img[i].img_type);
			printf("img_sram_addr:0x%x\n", tlv->img[i].img_sram_addr);
			printf("img_host_addr:0x%x\n", tlv->img[i].img_host_addr);
			printf("img_size:0x%x\n", tlv->img[i].img_size);
		}
	}

	writel(0xFF, (uintptr_t)dev_cfg->host_ddr_status);
	/*
	 *flush dcache
	 */
	flush_dcache_all();

	/* Check if the target is in PBL, else do a force reset */
	ret = readl(bar0_base + BHI_EXECENV);
	if (ret) {
		printf("Resetting target to start calibration\n");
		qcn92xx_global_soc_reset(bar0_base, true);
	}

	writel(lower_32_bits((uintptr_t)tlv), bar0_base + PCIE_LOCAL_RSV0);
	writel(0, bar0_base + BHI_STATUS);
	writel(upper_32_bits(load_addr), bar0_base + BHI_IMGADDR_HIGH);
	writel(lower_32_bits(load_addr), bar0_base + BHI_IMGADDR_LOW);
	writel(cal_fw_size, bar0_base + BHI_IMGSIZE);
	writel(1, bar0_base + BHI_IMGTXDB);

	printf("Waiting for calibration bin download...\n");

	reg = bar0_base + BHI_STATUS;
	ret = readl_poll_sleep_timeout(reg, val,
				       ((val & BHI_STATUS_MASK) >> BHI_STATUS_SHIFT) ==
				       BHI_STATUS_SUCCESS, 250 * 1000, 12500 * 1000);
	if (ret) {
		printf("Calibration bin Download failed, BHI_STATUS 0x%x, ret %d\n", val, ret);
		print_error_code(bar0_base, true);
		ret = -EINVAL;
		goto fail;
	}

	reg = bar0_base + BHI_EXECENV;
	ret = readl_poll_sleep_timeout(reg, val, (val & NO_MASK) == 1, 250 * 1000, 12500 * 1000);
	if (ret) {
		printf("EXECENV is not correct, BHI_EXECENV 0x%x, ret %d\n", val, ret);
		print_error_code(bar0_base, true);
		ret = -EINVAL;
		goto fail;
	}

	printf("Calibration bin loaded successfully\n");

	free(tlv);
	return 0;

fail:
	/* Target SoC global reset */
	qcn92xx_global_soc_reset(bar0_base, false);

	mdelay(1000);

	/* Target MHI reset */
	val = readl(bar0_base + MHICTRL);
	writel(val | MHICTRL_RESET_MASK, bar0_base + MHICTRL);
	free(tlv);
	return ret;
}

static int get_pci_dev_by_slot(int slot_id, struct udevice **devp)
{
	struct udevice *rc, *dev;
	int i;

	for (i = 0; i < CONFIG_IPQ_MAX_PCIE; i++) {
		if (pci_find_device_id(device_table, i, &dev))
			continue;

		rc = pci_get_controller(dev);
		if (!rc)
			continue;

		if (slot_id == dev_read_u32_default(rc, "id", 0)) {
			*devp = dev;
			return 0;
		}
	}

	printf("Failed to find device for slot_id %d\n", slot_id);
	return -ENODEV;
}

static int populate_cfg(struct cal_dt_config *dt_cfg, struct cal_config *cfg)
{
	static bool global_cfg_filled;
	struct cal_per_dev_config *dev_cfg;
	struct udevice *dev = NULL;

	if (dt_cfg->pci_slot_id >= CONFIG_IPQ_MAX_PCIE)
		return -EINVAL;

	if (!dt_cfg->rmem_base_addr)
		return -ENOMEM;

	if (get_pci_dev_by_slot(dt_cfg->pci_slot_id, &dev))
		return -ENODEV;

	/* Memory Layout for miniFW for calibration
	 *
	 * +==========+===============+=========+
	 * |  Region  | Start Offset  |   Size  |
	 * +----------+---------------+---------+
	 * | miniFW   |  0x00000000   |   4MB   |
	 * +----------+---------------+---------+
	 * | Hremote  |  0x00400000   |   4MB   |
	 * +----------+---------------+---------+
	 * | Host DDR |  0x00800000   |   1MB   |
	 * | Status   |               |         |
	 * |   +      |               |         |
	 * | Per Dev  |               |         |
	 * | CalData  |               |         |
	 * +----------+---------------+---------+
	 * | Cal Data |  0x00900000   |   1MB   |
	 * | ART Part |               |         |
	 * +----------+---------------+---------+
	 * | cal_fw   |  0x00A00000   |  10MB   |
	 * |from flash|               |         |
	 * +----------+---------------+---------+
	 * |  Caldb   |  0x01400000   |   8MB   |
	 * |          |(LM512 profile)|         |
	 * |          |  0x01C00000   |   8MB   |
	 * |          | (1GB profile) |         |
	 * +----------+---------------+---------+
	 * |  RDDM    |               |Last 6MB |
	 * +====================================+
	 *
	 */
	if (!global_cfg_filled) {
		cfg->ddr_base_addr = dt_cfg->rmem_base_addr;
		cfg->ddr_rmem_size = dt_cfg->rmem_size;
		cfg->caldata_addr = cfg->ddr_base_addr + (9 * SZ_1M);
		cfg->cal_fw_header =
			(struct cal_fw_header *)(uintptr_t)(cfg->ddr_base_addr + (10 * SZ_1M));
		global_cfg_filled = true;
	}

	dev_cfg = &cfg->dev_cfg[dt_cfg->pci_slot_id];

	dev_cfg->dev = dev;
	dev_cfg->pci_slot_id = dt_cfg->pci_slot_id;
	dev_cfg->board_id = dt_cfg->board_id;
	dev_cfg->caldata_offset = dt_cfg->caldata_offset;
	dev_cfg->caldata_size = MAX_CALDATA_SIZE;

	dev_cfg->cal_fw_image_addr = dt_cfg->rmem_base_addr;
	dev_cfg->hremote_addr = dt_cfg->rmem_base_addr + SZ_4M;
	dev_cfg->hremote_size = (4 * SZ_1M);

	dev_cfg->host_ddr_status = dt_cfg->rmem_base_addr + SZ_8M;

	dev_cfg->caldb_addr = dt_cfg->rmem_base_addr + dt_cfg->caldb_offset;
	dev_cfg->caldb_size = SZ_8M;

	dev_cfg->rddm_addr = dt_cfg->rmem_base_addr + dt_cfg->rmem_size - (6 * SZ_1M);
	dev_cfg->rddm_size = (6 * SZ_1M);

	return 0;
}

int cal_qcn9224(int debug)
{
	int ret, i;
	struct cal_config *cfg;
	struct cal_dt_config *dt_cfg;
	unsigned long len = SZ_2M;
	bool cal_part_present = false;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
#ifdef CONFIG_IPQ_NAND
	struct ubi_device *ubi;
#endif
#ifdef CONFIG_IPQ_MMC
	blkpart_info_t  bpart_info;
	struct disk_partition disk_info;
#endif

	if (!sfi)
		return -EINVAL;

	/* Check if cal_fw partition exists, if not bail out here */
	switch (gd->board_type & FLASH_TYPE_MASK) {
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_NORPLUSNAND:
	case SMEM_BOOT_QSPI_NAND_FLASH:
		ubi = ubi_get_device(0);
		if (!ubi)
			break;

		for (i = 0; i < (ubi->vtbl_slots + 1); i++) {
			if (!ubi->volumes[i])
				continue;	/* Empty record */

			if (ubi->volumes[i]->name_len <= UBI_VOL_NAME_MAX &&
			    strnlen(ubi->volumes[i]->name,
				    ubi->volumes[i]->name_len + 1) ==
						ubi->volumes[i]->name_len) {
				if (!strncmp("cal_fw", ubi->volumes[i]->name,
					     UBI_VOL_NAME_MAX)) {
					cal_part_present = true;
					break;
				}
			}
		}

		if (ubi)
			ubi_put_device(ubi);

		break;
#endif
	default:
#ifdef CONFIG_IPQ_MMC
		BLK_PART_GET_INFO_S(bpart_info, "0:CALFW", &disk_info,
				    sfi->flash_type);
		ret = ipq_part_get_info_by_name(&bpart_info);
		if (!ret)
			cal_part_present = true;
#endif
	}

	if (!cal_part_present) {
		printf("cal_fw partition not present\n");
		return -ENOTSUPP;
	}

	cfg = malloc(sizeof(*cfg));
	if (!cfg) {
		printf("failed to allocate memory for cal config\n");
		return -ENOMEM;
	}

	memset(cfg, 0, sizeof(*cfg));

	list_for_each_entry(dt_cfg, lhead, list) {
		ret = populate_cfg(dt_cfg, cfg);
		if (ret) {
			printf("Failed to populate cal config\n");
			goto out;
		}
	}

	if (!cfg->ddr_base_addr || !cfg->caldata_addr) {
		printf("Failed to get DDR base addr for cal\n");
		ret = -ENOMEM;
		goto out;
	}

	if (debug) {
		printf("Dumping the cal config\n");
		printf("cal_fw_header: 0x%lx\n", (uintptr_t)cfg->cal_fw_header);
		printf("ddr_base_addr: 0x%x\n", cfg->ddr_base_addr);
		printf("ddr_rmem_size: 0x%x\n", cfg->ddr_rmem_size);
		printf("caldata_addr: 0x%x\n", cfg->caldata_addr);
		for (i = 0; i < CONFIG_IPQ_MAX_PCIE; i++) {
			if (!cfg->dev_cfg[i].dev)
				continue;

			printf("dev_name[%d]: %s\n", i, cfg->dev_cfg[i].dev->name);
			printf("pci_slot_id[%d]: 0x%x\n", i, cfg->dev_cfg[i].pci_slot_id);
			printf("board_id[%d]: 0x%x\n", i, cfg->dev_cfg[i].board_id);
			printf("caldata_offset[%d]: 0x%x\n", i, cfg->dev_cfg[i].caldata_offset);
			printf("caldata_size[%d]: 0x%x\n", i, cfg->dev_cfg[i].caldata_size);
			printf("cal_fw_image_addr[%d]: 0x%x\n", i, cfg->dev_cfg[i].cal_fw_image_addr);
			printf("hremote_addr[%d]: 0x%x\n", i, cfg->dev_cfg[i].hremote_addr);
			printf("caldb_addr[%d]: 0x%x\n", i, cfg->dev_cfg[i].caldb_addr);
			printf("host_ddr_status[%d]: 0x%x\n", i, cfg->dev_cfg[i].host_ddr_status);
			printf("rddm_addr[%d]: 0x%x\n", i, cfg->dev_cfg[i].rddm_addr);
		}
	}

	ret = get_partition_data("0:ART", 0, (uint8_t *)(uintptr_t)cfg->caldata_addr,
				 SZ_1M, sfi->flash_type);
	if (ret < 0) {
		printf("Failed to read caldata from ART: %d\n", ret);
		goto out;
	}

	switch (gd->board_type & FLASH_TYPE_MASK) {
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_NORPLUSNAND:
	case SMEM_BOOT_QSPI_NAND_FLASH:
		ret = ubi_volume_read("cal_fw", (uint8_t *)(uintptr_t)cfg->ddr_base_addr,
				      (size_t)SZ_2M);
		if (ret) {
			ret = -ret;
			printf("Failed to read data from cal_fw volume %d\n",
			       ret);
		}
		break;
#endif
	default:
#ifdef CONFIG_IPQ_MMC
		ret = get_partition_data("0:CALFW", 0, (uint8_t *)(uintptr_t)cfg->ddr_base_addr,
					 SZ_2M, SMEM_BOOT_MMC_FLASH);
#endif
	}
	if (ret < 0) {
		printf("Failed to read from CAL FW %d\n", ret);
		goto out;
	}

	ret = gunzip((void *)cfg->cal_fw_header, (SZ_8M + SZ_2M),
		     (uint8_t *)(uintptr_t)cfg->ddr_base_addr, &len);
	if (ret < 0) {
		printf("Failed to uncompress CAL FW %d\n", ret);
		goto out;
	}

	if (cfg->cal_fw_header->magic != 0xCAFECACE) {
		printf("Magic mismatch\n");
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < CONFIG_IPQ_MAX_PCIE; i++) {
		if (!cfg->dev_cfg[i].dev)
			continue;

		ret = do_cal_qcn9224(cfg, i, debug);
		if (ret) {
			printf("failed to start cal on qcn9224[%d] %d\n",
			       cfg->dev_cfg[i].pci_slot_id, ret);
			goto out;
		}
	}

out:
	free(cfg);
	return ret;
}
#endif

/**
 * hex_string_to_binary() - Convert hex string to binary data
 * @hex_str: Input hex string
 * @binary_data: Output buffer for binary data
 * @max_len: Maximum length of output buffer
 *
 * Returns: Length of binary data on success, negative error code on failure
 */
int hex_string_to_binary(const char *hex_str, uint8_t *binary_data,
			uint32_t max_len)
{
	uint32_t hex_len;
	uint32_t binary_len;
	uint32_t i;

	/* Input parameter validation */
	if (!hex_str) {
		printf("Error: hex_str parameter is NULL\n");
		return -EINVAL;
	}

	if (!binary_data) {
		printf("Error: binary_data parameter is NULL\n");
		return -EINVAL;
	}

	if (max_len == 0) {
		printf("Error: max_len parameter is zero\n");
		return -EINVAL;
	}

	hex_len = strlen(hex_str);
	if (hex_len == 0) {
		printf("Error: hex_str is empty\n");
		return -EINVAL;
	}

	/* Validate hex_len is reasonable (max 2 * max_len) */
	if (hex_len > (max_len * 2)) {
		printf("Error: Hex string too long (length %d, max %d chars)\n",
			hex_len, max_len * 2);
		return -EINVAL;
	}

	if (hex_len % 2 != 0) {
		printf("Error: Hex string length must be even\n");
		return -EINVAL;
	}

	binary_len = hex_len / 2;
	if (binary_len > max_len) {
		printf("Error: Hex string too long (max %d bytes)\n", max_len);
		return -EINVAL;
	}

	/* Validate all characters are valid hex digits */
	for (i = 0; i < hex_len; i++) {
		char c = hex_str[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
				|| (c >= 'A' && c <= 'F'))) {
			printf("Error: Invalid hex character '%c' at position %d\n", c, i);
			return -EINVAL;
		}
	}

	for (i = 0; i < binary_len; i++) {
		char hex_byte[3];
		char *endptr;
		unsigned long byte_val;
		uint32_t char_pos1 = i * 2;
		uint32_t char_pos2 = i * 2 + 1;

		hex_byte[0] = hex_str[char_pos1];
		hex_byte[1] = hex_str[char_pos2];
		hex_byte[2] = '\0';

		byte_val = strtoul(hex_byte, &endptr, 16);

		if (*endptr != '\0' || byte_val > 0xFF) {
			printf("Error: Invalid hex value at position %d\n", char_pos1);
			return -EINVAL;
		}

		binary_data[i] = (uint8_t)byte_val;
	}

	/* Function returns actual bytes written - caller manages buffer */
	return (int)binary_len;
}

/**
 * generate_random_context() - Generate random context data
 * @context: Output buffer for random data
 * @size: Size of random data to generate
 */
void generate_random_context(uint8_t *context, uint32_t size)
{
	uint32_t i;
	uint32_t seed;

	/* Input parameter validation */
	if (!context) {
		printf("Error: context parameter is NULL\n");
		return;
	}

	if (size == 0) {
		printf("Error: size parameter is zero\n");
		return;
	}

	/* Additional safety check for reasonable size limits */
	if (size > (1024 * 1024)) {  /* 1MB limit */
		printf("Error: size parameter too large (%u bytes, max 1MB)\n", size);
		return;
	}

	seed = get_timer(0) ^ (ulong)&seed;
	srand(seed);

	for (i = 0; i < size; i++) {
		context[i] = (uint8_t)(rand() & 0xFF);
	}
}

