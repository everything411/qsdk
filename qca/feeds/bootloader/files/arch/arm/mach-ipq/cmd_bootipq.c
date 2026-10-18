// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015-2017, 2020 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <mach/ipq.h>
#include <mach/smem_info.h>
#include <command.h>
#include <image.h>
#include <nand.h>
#include <errno.h>
#include <part.h>
#include <linux/mtd/ubi.h>
#include <mmc.h>
#include <part_efi.h>
#include <fdtdec.h>
#include <usb.h>
#include <elf.h>
#include <memalign.h>
#include <cpu_func.h>
#include <bootm.h>
#include <linux/bug.h>
#include <asm/io.h>
#ifdef CONFIG_IPQ_SPI_NOR
#include <spi.h>
#include <spi_flash.h>
#endif
#ifdef CONFIG_MMC
#include <mmc.h>
#endif
#ifdef CONFIG_IPQ_TMEL_IPC_SUPPORT
#include <mailbox.h>
#include <linux/tmelcom-qmp.h>
#endif

/******************************************************************
 * Globals constant & typedef
 *****************************************************************/
#define XMK_STR(x)			#x
#define MK_STR(x)			XMK_STR(x)
#define KERNEL_SEC_AUTH_SW_ID		0x71
#define ROOTFS_SEC_AUTH_SW_ID		0x67
#define SEC_AUTH_SW_ID			0x17
#define ROOTFS_IMAGE_TYPE		0x13
#define NO_OF_PROGRAM_HDRS		3
#define PRIMARY_PARTITION		1
#define SECONDARY_PARTITION		2

/* MVEW (Multi-Version ELF Wrapper) outer wrapper carries kernel v7/v8 +
 * itb + rootfs v7/v8 phdrs
 */
#define MAX_PROGRAM_HDRS		6

#define ELF_HDR_PLUS_PHDR_SIZE		(sizeof(Elf32_Ehdr) + \
				(MAX_PROGRAM_HDRS * sizeof(Elf32_Phdr)))

/* p_flags tags in ELF's PF_MASKOS range (0x0ff00000), see include/elf.h */
#define MVEW_WRAPPER_FLAG		(0xAu << 20)

#define HASH_TARGET_KERNEL		0u
#define HASH_TARGET_ROOTFS		1u
#define HASH_P_FLAG(target, ver)	(((target) << 24) | ((ver) << 20))

#define HASH_P_FLAG_MBN_V7		HASH_P_FLAG(HASH_TARGET_KERNEL, 7)
#define HASH_P_FLAG_MBN_V8		HASH_P_FLAG(HASH_TARGET_KERNEL, 8)
#define HASH_P_FLAG_ROOTFS_MBN_V7	HASH_P_FLAG(HASH_TARGET_ROOTFS, 7)
#define HASH_P_FLAG_ROOTFS_MBN_V8	HASH_P_FLAG(HASH_TARGET_ROOTFS, 8)

#define FIT_BOOTARGS_PROP		"append-bootargs"
#ifndef IPQ_NAND_BOOTARGS_PRI
#define IPQ_NAND_BOOTARGS_PRI		"ubi.mtd=rootfs root=mtd:ubi_rootfs "\
					"rootfstype=squashfs"
#endif
#ifndef IPQ_NAND_BOOTARGS_ALT
#define IPQ_NAND_BOOTARGS_ALT		"ubi.mtd=rootfs_1 root=mtd:ubi_rootfs "\
					"rootfstype=squashfs"
#endif

DECLARE_GLOBAL_DATA_PTR;

typedef int (*boot_stage)(void);

#ifdef CONFIG_IPQ_NAND
extern int ubi_volume_read(char *volume, char *buf, loff_t offset, size_t size);
#endif

extern int initr_net(void);

const char *stages[] = {"FIND ACTIVE PART", "READING KERNEL", "AUTHENTICATE",
			"GET CONFIG", "SET_BOOTARGS", "BOOTING"};

char g_config[32];
/**********************************************************************
 * Structure enum and static
 *********************************************************************/
enum boot_stage {
	BOOT_STAGE_READ_BC,
	BOOT_STAGE_READ,
	BOOT_STAGE_AUTH,
	BOOT_STAGE_GET_CONFIG,
	BOOT_STAGE_SET_BOOTARGS,
	BOOT_STAGE_BOOT,
};

enum boot_error {
	KERNEL_AUTH_FAILED = -1,
	ROOTFS_AUTH_FAILED = -2,
	INVALID_KERNEL_IMAGE = -3
};

struct boot_config {
#ifdef CONFIG_IPQ_SPI_NOR
	struct spi_flash *flash;
#endif
	const char *config;
	ulong load_address;
	ulong size;
	ulong meta_data_size;
#ifdef CONFIG_IPQ_MVEW
	ulong rootfs_meta_dram_addr; /* MVEW rootfs metadata DRAM addr */
#endif
	enum boot_stage stage;
	enum bank active_bank;
	bool debug;
};

static struct boot_config boot_info;

struct kernel_img_info {
	uint32_t kernel_load_addr;
	uint32_t kernel_load_size;
	uint32_t kernel_meta_data_size;
};

#ifdef CONFIG_IPQ_ELF_AUTH
struct ipq_image_info {
	unsigned int img_offset;
	unsigned int img_load_addr;
	unsigned int img_size;
#ifdef CONFIG_IPQ_MVEW
	/* MVEW-only fields; unused for legacy single-version images */
	bool is_mvew_format;
	unsigned int meta_flash_offset;
	unsigned int itb_flash_offset;
	bool has_rootfs_meta;
	unsigned int rootfs_meta_flash_offset;
	unsigned int rootfs_meta_size;
#endif
};
static struct ipq_image_info img_info;
#endif

/**********************************************************************
 * Function definition
 *********************************************************************/

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
__weak bool is_version_rollback_support(void)
{
	return true;
}
#endif

__weak uint32_t is_board_support_image_auth(void)
{
	return 0;
}

#ifdef CONFIG_IPQ_ELF_AUTH
void update_load_addr(struct ipq_image_info *img_info)
{
	boot_info.load_address = img_info->img_load_addr;
	boot_info.load_address -= img_info->img_offset;
	boot_info.meta_data_size = img_info->img_offset;
}

/* MVEW sibling of update_load_addr(); anchors off itb_flash_offset since
 * MVEW may have two metadata segments preceding .itb, not just one */
#ifdef CONFIG_IPQ_MVEW
void update_load_addr_mvew(struct ipq_image_info *img_info)
{
	boot_info.load_address = img_info->img_load_addr;
	boot_info.load_address -= img_info->itb_flash_offset;
	boot_info.meta_data_size = img_info->img_offset;
}
#endif
#endif
#ifdef CONFIG_MMC
int set_mmc_bootargs(char *boot_args, char *part_name, int buflen,
			bool gpt_flag)
{
	int ret;
	struct disk_partition disk_info;
	struct blkpart_info  bpart_info;

	BLK_PART_GET_INFO_S(bpart_info,
				part_name,
				&disk_info,
				SMEM_BOOT_MMC_FLASH,
				true);

	if (buflen <= 0 || buflen > CONFIG_SYS_MAXARGS)
		return -EINVAL;

	ret = ipq_part_get_info_by_name(&bpart_info);
	if (ret) {
		printf("bootipq: unsupported partition name %s\n", part_name);
		return -EINVAL;
	}
#ifdef CONFIG_PARTITION_UUIDS
	snprintf(boot_args, CONFIG_SYS_MAXARGS, "root=PARTUUID=%s%s",
			disk_info.uuid, (gpt_flag == true)?" gpt" : "");
#else
	snprintf(boot_args, CONFIG_SYS_MAXARGS, "rootfsname==%s gpt",
			part_name, (gpt_flag == true)?" gpt" : "");
#endif
	if (env_get("fsbootargs") == NULL)
		env_set("fsbootargs", boot_args);

	return 0;
}
#endif

int __validate_the_offset(const char *partition_name)
{
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	uint32_t offset;

	if (!sfi) {
		pr_debug("SMEM flash info not available\n");
		return CMD_RET_FAILURE;
	}

	if (!strcmp(partition_name, "rootfs"))
		offset = sfi->rootfs.offset;
	else if (!strcmp(partition_name, "hlos"))
		offset = sfi->hlos.offset;
	else
		return CMD_RET_FAILURE;

	if (offset == 0xBAD0FF5E) {
		pr_debug("bad offset for %s\n", partition_name);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
void set_crashdump_bootargs(char *bootargs, uint8_t pri_ftype,
				uint8_t sec_ftype)
{
	char *part_name = env_get("dump_to_nvmem");
	int ret;
	uint32_t *buf = NULL;
	uint8_t flash_type = sec_ftype ? sec_ftype : pri_ftype;

	if (bootargs == NULL)
		return;

	if (!part_name) {
		printf("%s: dump_to_nvmem env not available\n", __func__);
		return;
	}

	if (flash_type == SMEM_BOOT_QSPI_NAND_FLASH) {
#ifdef CONFIG_IPQ_NAND
		loff_t offset;
		size_t part_size, read_size = 64;
		struct mtd_info *mtd = get_nand_dev_by_index(0);

		if (!mtd)
			return;
#ifdef CONFIG_NOR_BLK
		if (pri_ftype == SMEM_BOOT_NORGPT_FLASH) {
			struct blkpart_info bpart_info;
			struct disk_partition disk_info;

			BLK_PART_GET_INFO_S(bpart_info,
						part_name,
						&disk_info,
						pri_ftype,
						true);

			ret = ipq_part_get_info_by_name(&bpart_info);
			if (ret)
				return;

			offset = (loff_t)bpart_info.info->start *
					(loff_t)bpart_info.desc->blksz;
			part_size = bpart_info.info->size *
					bpart_info.desc->blksz;
		} else
#endif /* CONFIG_NOR_BLK */
		{
			if (ipq_getpart_offset_size(part_name, (uint32_t *)&offset,
						(uint32_t *)&part_size))
				return;
		}

		buf = malloc(read_size);
		if (!buf) {
			debug("failed to allocate memory at %s\n", __func__);
			return;
		}

		ret = nand_read(mtd, (uint32_t)offset, (size_t *)&read_size,
				(void *)buf);
		if (ret)
			goto retn;
#endif
	} else {
#ifdef CONFIG_IPQ_MMC
		struct blkpart_info bpart_info;
		struct disk_partition disk_info;

		if (!find_mmc_device(0))
			return;

		BLK_PART_GET_INFO_S(bpart_info,
					part_name,
					&disk_info,
					flash_type,
					true);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret)
			return;

		buf = malloc_cache_aligned(disk_info.blksz);
		if (!buf) {
			debug("failed to allocate memory at %s\n", __func__);
			return;
		}

		ret = blk_dread(bpart_info.desc, disk_info.start,
					1, (void *)buf);
		if (ret < 0) {
			printf("Blk read failed %d\n", ret);
			goto retn;
		}
#endif
	}

	if (buf && (buf[0] == DUMP2MEM_MAGIC1_COOKIE) &&
			(buf[1] == DUMP2MEM_MAGIC2_COOKIE)) {
		if ((strlen(bootargs) + strlen(" collect_minidump")) >
						CONFIG_SYS_CBSIZE) {
			printf("%s: bootargs update failed, env size exceeds\n",
					__func__);
		} else {
			snprintf(bootargs, CONFIG_SYS_CBSIZE,
					"%s collect_minidump", bootargs);
		}
	}

retn:
	if (buf)
		free(buf);
}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

void set_fs_bootargs(char *bootargs)
{
	int len;
	char *fit_bootargs = (char *)fdt_getprop((void *)
					boot_info.load_address,
					0,
					FIT_BOOTARGS_PROP,
					&len);

	if (bootargs == NULL)
		return;

	if ((fit_bootargs != NULL) && len) {
		if ((strlen(bootargs) + len) > CONFIG_SYS_CBSIZE) {
			printf("%s: bootargs update failed, env size exceeds\n",
					__func__);
			return;
		}
	} else {
		fit_bootargs = env_get("fsbootargs");
		if (!fit_bootargs) {
			printf("%s: fsbootargs not available\n", __func__);
			return;
		}

		if ((strlen(bootargs) + strlen(fit_bootargs) +
				strlen("  rootwait")) > CONFIG_SYS_CBSIZE) {
			printf("%s: bootargs update failed, env size exceeds\n",
					__func__);
			return;
		}
	}

	snprintf(bootargs, CONFIG_SYS_CBSIZE, "%s %s rootwait",
			bootargs, fit_bootargs);
}

int set_bootargs(void)
{
	char *cmd_line, *strings = env_get("bootargs");
	int ret = CMD_RET_SUCCESS;
#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	struct ipq_smem_flash_info *sfi = NULL;
#endif

#ifdef CONFIG_MMC
	bool gpt_flag = true;
	char runcmd[CONFIG_SYS_MAXARGS];
	uint8_t	flash_type = gd->board_type & FLASH_TYPE_MASK;

	boot_info.stage = BOOT_STAGE_SET_BOOTARGS;

	if (flash_type  == SMEM_BOOT_MMC_FLASH)
		gpt_flag = true;
	else if (flash_type ==  SMEM_BOOT_NORPLUSEMMC)
		gpt_flag = false;

	if (boot_info.active_bank == 1)
		ret = set_mmc_bootargs(runcmd, ROOTFS_INACTIVE_LABEL,
				CONFIG_SYS_MAXARGS, gpt_flag);
	else if (!boot_info.active_bank)
		ret = set_mmc_bootargs(runcmd, ROOTFS_ACTIVE_LABEL,
				CONFIG_SYS_MAXARGS, gpt_flag);
	else
		return -EBADFD;

#elif CONFIG_IPQ_NAND
	if (env_get("fsbootargs") == NULL)
#if defined(CONFIG_BOOTCONFIG_V3) || defined(CONFIG_FAILSAFE_V2)
		ret = env_set("fsbootargs", boot_info.active_bank == 1 ?
				IPQ_NAND_BOOTARGS_ALT :
				IPQ_NAND_BOOTARGS_PRI);
#else
		ret = env_set("fsbootargs", boot_info.active_bank ?
				IPQ_NAND_BOOTARGS_PRI :
				IPQ_NAND_BOOTARGS_PRI);
#endif
#endif
		if (ret)
			return ret;
	else
		printf("Using fsbootargs from env\n");

	if (!strings) {
		printf("bootargs not available\n");
		printf("Please set bootargs or try env default -fa ");
		printf("to set default bootargs\n");

		return -ENXIO;
	}

	cmd_line = malloc(CONFIG_SYS_CBSIZE);
	if (!cmd_line) {
		printf("%s: Memory allocation failed\n", __func__);

		return -ENOMEM;
	}

	memset(cmd_line, 0, CONFIG_SYS_CBSIZE);
	strlcpy(cmd_line, strings, strlen(strings)+1);

#ifdef CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY
	sfi = ipq_get_smem_info();

	if (sfi && env_get("dump_to_nvmem")) {
		set_crashdump_bootargs(cmd_line, sfi->flash_type,
				sfi->flash_secondary_type);
	}
#endif /* CONFIG_IPQ_CRASHDUMP_TO_NVMEMORY */

#ifdef CONFIG_BOOT_BANK_FIXUP
	/* Append PARTLABEL based on booted bank */
	ret = append_partlabel_bootargs(cmd_line, CONFIG_SYS_CBSIZE);
	if (ret)
		printf("Failed to append PARTLABEL to bootargs\n");
#endif /* CONFIG_BOOT_BANK_FIXUP */

	set_fs_bootargs(cmd_line);
	env_set("bootargs", NULL);
	env_set("bootargs", cmd_line);

	if (cmd_line)
		free(cmd_line);

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_IPQ_ELF_AUTH
static int parse_elf_image_phdr(struct ipq_image_info *img_info, unsigned int addr)
{
	Elf32_Ehdr *ehdr; /* Elf header structure pointer */
	Elf32_Phdr *phdr; /* Program header structure pointer */
	int i;

	ehdr = (Elf32_Ehdr *)(uintptr_t)addr;
	phdr = (Elf32_Phdr *)(uintptr_t)(addr + ehdr->e_phoff);

	if (!IS_ELF(*ehdr)) {
		printf("It is not a elf image, support only 32-bit ELF\n");
		return -EINVAL;
	}

	if (ehdr->e_type != ET_EXEC) {
		printf("Not a valid elf image\n");
		return -EINVAL;
	}

	/* Load each program header */
	for (i = 0; i < NO_OF_PROGRAM_HDRS; ++i) {
		printf("Parsing phdr");
		printf("load addr 0x%x offset 0x%x size 0x%x type 0x%x\n",
			phdr->p_paddr, phdr->p_offset, phdr->p_filesz,
			phdr->p_type);
		if (phdr->p_type == PT_LOAD) {
			img_info->img_offset = phdr->p_offset;
			img_info->img_load_addr = phdr->p_paddr;
			img_info->img_size =  phdr->p_filesz;
			return 0;
		}
		++phdr;
	}

	return -EINVAL;
}

#ifdef CONFIG_IPQ_MVEW
/* True if outer wrapper at addr is MVEW-tagged; p_flags masked by
 * PF_MASKOS since only those bits are ours to define */
bool is_mvew_wrapper(unsigned int addr)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)(uintptr_t)addr;
	Elf32_Phdr *phdr0;

	if (!IS_ELF(*ehdr) || ehdr->e_type != ET_EXEC)
		return false;

	phdr0 = (Elf32_Phdr *)(uintptr_t)(addr + ehdr->e_phoff);

	return (phdr0->p_type == PT_NULL &&
		(phdr0->p_flags & PF_MASKOS) == MVEW_WRAPPER_FLAG);
}

/* MVEW sibling of parse_elf_image_phdr(); caller sets is_mvew_format */
static int parse_elf_image_phdr_mvew(struct ipq_image_info *img_info, unsigned int addr)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	Elf32_Phdr *load_phdr = NULL;
	Elf32_Phdr *hash_phdr = NULL;
	Elf32_Phdr *rootfs_hash_phdr = NULL;
	uint32_t wanted_hash_flag;
	uint32_t wanted_rootfs_hash_flag;
	uint32_t mbn_version;
	int i;

	ehdr = (Elf32_Ehdr *)(uintptr_t)addr;

	if (!IS_ELF(*ehdr)) {
		printf("It is not a elf image, support only 32-bit ELF\n");
		return -EINVAL;
	}

	if (ehdr->e_type != ET_EXEC) {
		printf("Not a valid elf image\n");
		return -EINVAL;
	}

	/* fixed MAX_PROGRAM_HDRS bound; never trust e_phnum pre-auth */
	phdr = (Elf32_Phdr *)(uintptr_t)(addr + ehdr->e_phoff);

	mbn_version = ipq_get_required_mbn_version();
	if (boot_info.debug)
		printf("[debug] parse_elf_image_phdr_mvew: required_mbn_version=%u\n",
			mbn_version);
	if (mbn_version == 8) {
		wanted_hash_flag = HASH_P_FLAG_MBN_V8;
		wanted_rootfs_hash_flag = HASH_P_FLAG_ROOTFS_MBN_V8;
	} else {
		wanted_hash_flag = HASH_P_FLAG_MBN_V7;
		wanted_rootfs_hash_flag = HASH_P_FLAG_ROOTFS_MBN_V7;
	}

	for (i = 0; i < MAX_PROGRAM_HDRS; ++i) {
		printf("Parsing phdr");
		printf("load addr 0x%x offset 0x%x size 0x%x type 0x%x flags 0x%x\n",
			phdr[i].p_paddr, phdr[i].p_offset, phdr[i].p_filesz,
			phdr[i].p_type, phdr[i].p_flags);

		if (phdr[i].p_type == PT_LOAD)
			load_phdr = &phdr[i];
		else if (phdr[i].p_type == PT_NULL &&
			 (phdr[i].p_flags & PF_MASKOS) == wanted_hash_flag)
			hash_phdr = &phdr[i];
		else if (phdr[i].p_type == PT_NULL &&
			 (phdr[i].p_flags & PF_MASKOS) == wanted_rootfs_hash_flag)
			rootfs_hash_phdr = &phdr[i];
	}

	if (!hash_phdr) {
		printf("MVEW: no metadata segment for required MBN version\n");
		return -EINVAL;
	}

	if (!load_phdr) {
		printf("MVEW: no .itb (PT_LOAD) segment found\n");
		return -EINVAL;
	}

	img_info->meta_flash_offset = hash_phdr->p_offset;
	img_info->img_offset = hash_phdr->p_filesz;
	/* covers both metadata segments, not just the chosen one */
	img_info->itb_flash_offset = load_phdr->p_offset;
	img_info->img_load_addr = load_phdr->p_paddr;
	img_info->img_size = load_phdr->p_filesz;

	img_info->has_rootfs_meta = (rootfs_hash_phdr != NULL);
	if (img_info->has_rootfs_meta) {
		img_info->rootfs_meta_flash_offset = rootfs_hash_phdr->p_offset;
		img_info->rootfs_meta_size = rootfs_hash_phdr->p_filesz;
	}

	return 0;
}
#endif /* CONFIG_IPQ_MVEW */
#endif

#ifdef CONFIG_MMC
static int read_from_mmc(void)
{
	struct disk_partition disk_info;
	int ret;
	uint32_t blk, cnt, n;
	void *addr;
	struct blkpart_info  bpart_info;

	BLK_PART_GET_INFO_S(bpart_info,
				NULL,
				&disk_info,
				SMEM_BOOT_MMC_FLASH,
				true);

	if (boot_info.active_bank)
		bpart_info.name = KERNEL_INACTIVE_LABEL;
	else
		bpart_info.name = KERNEL_ACTIVE_LABEL;

	if (boot_info.debug)
		printf("[debug]Reading %s\n", bpart_info.name);

	ret = ipq_part_get_info_by_name(&bpart_info);
	if (ret == 0) {
		if (is_board_support_image_auth()) {
#ifdef CONFIG_IPQ_ELF_AUTH
			addr = (void *)boot_info.load_address;
			blk = (uint32_t) disk_info.start;
			cnt = (uintptr_t)ELF_HDR_PLUS_PHDR_SIZE;

			printf("MMC read: block # %d count %d\n", blk, cnt);

			n = blk_dread(bpart_info.desc, blk, cnt, addr);

			printf("%d blocks read: %s\n", n,
				(n == cnt) ? "OK" : "ERROR");

			if (n != cnt)
				return CMD_RET_FAILURE;

#ifdef CONFIG_IPQ_MVEW
			if (is_mvew_wrapper(boot_info.load_address)) {
				img_info.is_mvew_format = true;
				if (parse_elf_image_phdr_mvew(&img_info,
						boot_info.load_address))
					return CMD_RET_FAILURE;

				update_load_addr_mvew(&img_info);
			} else {
				img_info.is_mvew_format = false;
				if (parse_elf_image_phdr(&img_info,
						boot_info.load_address))
					return CMD_RET_FAILURE;

				update_load_addr(&img_info);
			}
#else
			if (parse_elf_image_phdr(&img_info,
					boot_info.load_address))
				return CMD_RET_FAILURE;

			update_load_addr(&img_info);
#endif
#endif
		}

		boot_info.size = disk_info.size;
		boot_info.size *= disk_info.blksz;
		addr = (void *)boot_info.load_address;
		blk = (uint32_t) disk_info.start;
		cnt = (uint32_t) disk_info.size;

		printf("MMC read: block # %d, count %d\n", blk, cnt);

		n = blk_dread(bpart_info.desc, blk, cnt, addr);

		printf("%d blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");

		if (n != cnt)
			return CMD_RET_FAILURE;

		if (boot_info.debug)
			printf("[debug]loaded kernel @ 0x%lx\n",
					boot_info.load_address);

		return CMD_RET_SUCCESS;
	}

	return CMD_RET_FAILURE;
}
#endif

#ifdef CONFIG_IPQ_NAND
static int read_from_nand(void)
{
	int ret;

	if (__validate_the_offset("rootfs") != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;
	/*
	 * init ubi
	 */
	ret = ipq_init_ubi_part();
	if (ret) {
		printf(" Ubi error %d\n", ret);
		return CMD_RET_FAILURE;
	}

#ifdef CONFIG_IPQ_ELF_AUTH
	if (is_board_support_image_auth()) {
		ret = ubi_volume_read("kernel", (char *)boot_info.load_address,
				      0, (uintptr_t)ELF_HDR_PLUS_PHDR_SIZE);
		if (ret)
			return CMD_RET_FAILURE;

#ifdef CONFIG_IPQ_MVEW
		if (is_mvew_wrapper(boot_info.load_address)) {
			img_info.is_mvew_format = true;
			if (parse_elf_image_phdr_mvew(&img_info,
					boot_info.load_address))
				return CMD_RET_FAILURE;

			update_load_addr_mvew(&img_info);
		} else {
			img_info.is_mvew_format = false;
			if (parse_elf_image_phdr(&img_info, boot_info.load_address))
				return CMD_RET_FAILURE;

			update_load_addr(&img_info);
		}
#else
		if (parse_elf_image_phdr(&img_info, boot_info.load_address))
			return CMD_RET_FAILURE;

		update_load_addr(&img_info);
#endif
	}
#endif
	boot_info.size = ipq_ubi_get_volume_size("kernel");

	if (boot_info.size < 0) {
		printf("Invalid kernel size %lu\n", boot_info.size);
		return CMD_RET_FAILURE;
	}

	ret = ubi_volume_read("kernel", (char *)boot_info.load_address, 0, 0);
	if (ret)
		return CMD_RET_FAILURE;

	if (boot_info.debug)
		printf("[debug]loaded kernel @ 0x%lx\n",
			boot_info.load_address);

	return CMD_RET_SUCCESS;
}
#endif

#ifdef CONFIG_IPQ_SPI_NOR
static int read_from_nor(void)
{
	int ret;
	struct ipq_smem_flash_info *sfi;

	if (__validate_the_offset("hlos") != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	sfi = ipq_get_smem_info();

	boot_info.flash = ipq_spi_probe();
	if (!boot_info.flash) {
		printf("Failed to probe spi flash\n");
		return CMD_RET_FAILURE;
	}

#ifdef CONFIG_IPQ_ELF_AUTH
	if (is_board_support_image_auth()) {
		ret = spi_flash_read(boot_info.flash, sfi->hlos.offset,
				ELF_HDR_PLUS_PHDR_SIZE,
				(void *)boot_info.load_address);
		if (ret)
			return CMD_RET_FAILURE;

#ifdef CONFIG_IPQ_MVEW
		if (is_mvew_wrapper(boot_info.load_address)) {
			img_info.is_mvew_format = true;
			if (parse_elf_image_phdr_mvew(&img_info,
					boot_info.load_address))
				return CMD_RET_FAILURE;

			update_load_addr_mvew(&img_info);
		} else {
			img_info.is_mvew_format = false;
			if (parse_elf_image_phdr(&img_info, boot_info.load_address))
				return CMD_RET_FAILURE;

			update_load_addr(&img_info);
		}
#else
		if (parse_elf_image_phdr(&img_info, boot_info.load_address))
			return CMD_RET_FAILURE;

		update_load_addr(&img_info);
#endif
	}
#endif
	boot_info.size = sfi->hlos.size;

	ret = spi_flash_read(boot_info.flash,
				sfi->hlos.offset,
				sfi->hlos.size,
				(void *)boot_info.load_address);
	if (ret)
		return CMD_RET_FAILURE;

	if (boot_info.debug)
		printf("[debug]loaded kernel @ 0x%lx\n",
				boot_info.load_address);

	return CMD_RET_SUCCESS;
}
#endif

int config_select(void)
{
	/* Selecting a config name from the list of available
	 * config names by passing them to the fit_conf_get_node()
	 * function which is used to get the node_offset with the
	 * config name passed. Based on the return value index based
	 * or board name based config is used.
	 */
	int len, i, ret;
	const char *config = env_get("config_name");
	ulong request;
	bool ishypen = false;
	bool found = false;

	boot_info.stage = BOOT_STAGE_GET_CONFIG;

	if (boot_info.debug)
		printf("[debug]Get Config\n");

	if (!(gd->board_type & SECURE_BOARD) || !is_board_support_image_auth()) {
		request = boot_info.load_address;
		ret = genimg_get_format((void *)request);
		if ((ret != IMAGE_FORMAT_LEGACY) && (ret != IMAGE_FORMAT_FIT)) {
#ifdef CONFIG_IPQ_ELF_AUTH
			/* skip past metadata to reach FIT/legacy image */
#ifdef CONFIG_IPQ_MVEW
			if (is_mvew_wrapper(request)) {
				if (!parse_elf_image_phdr_mvew(&img_info, request)) {
					request += img_info.itb_flash_offset;
					boot_info.load_address = request;
				}
			} else if (!parse_elf_image_phdr(&img_info, request)) {
				request += img_info.img_offset;
				boot_info.load_address = request;
			}
#else
			if (!parse_elf_image_phdr(&img_info, request)) {
				request += img_info.img_offset;
				boot_info.load_address = request;
			}
#endif
#endif
		} else
			goto get_img_config;
	} else if (gd->board_type & SECURE_BOARD) {
#ifndef CONFIG_IPQ_ELF_AUTH
		request = boot_info.load_address + sizeof(struct mbn_header);
#else
		request = img_info.img_load_addr;
#endif
	}

	ret = genimg_get_format((void *)request);

get_img_config:
	if (ret == IMAGE_FORMAT_LEGACY) {

		if (boot_info.debug)
			printf("[debug]Image type - LEGACY\n");

		config = NULL;
		goto exit;
	} else if (ret == IMAGE_FORMAT_FIT) {

		if (boot_info.debug)
			printf("[debug]Image type - FIT\n");

		int noff = fit_image_get_node((void *)request,
							"kernel-1");

		if (noff < 0) {
			noff = fit_image_get_node((void *)request,
					"kernel@1");
			if (noff < 0)
				return CMD_RET_FAILURE;
		} else
			ishypen = true;

		if (!fit_image_check_arch((void *)request, noff,
					IH_ARCH_DEFAULT)) {

			printf("Cross Arch Kernel jump is not supported!!!\n");
			printf("Please use %d-bit kernel image.\n",
				((IH_ARCH_DEFAULT == IH_ARCH_ARM64) ? 64 : 32));

			return CMD_RET_FAILURE;

		}
	} else {
		printf("Unknown Image format- support only FIT & Legacy fmt\n");

		return CMD_RET_FAILURE;
	}

	if (config) {
		printf("Manual device tree config %s selected!\n", config);
		if (fit_conf_get_node((void *)request, config) >= 0)
			goto exit;
	} else {
		/*
		 * In upstream dts config_name entry not available
		 * so referring  statically declared config from table
		 */
		struct multidtb_config *c = g_board_dtb_info;
		char *string = ishypen ? "config-" : "config@";
		char *priconfig = c->list[c->index].priconfig;
		char *secconfig = c->list[c->index].secconfig;

		if ((priconfig != NULL) || (priconfig != NULL)) {
			snprintf(g_config, 64, "%s%s", string, priconfig);
			if (fit_conf_get_node((void *)request,
				g_config) >= 0) {
				found = true;
				goto exit;
			} else {
				if (secconfig) {
					snprintf(g_config, 64, "%s%s", string,
							secconfig);
					if (fit_conf_get_node((void *)request,
						g_config) >= 0) {
						found = true;
						goto exit;
					}
				}
			}
		}

		for (i = 0;
			(config = fdt_stringlist_get(gd->fdt_blob, 0,
					"config_name", i, &len)); ++i) {
			if (config == NULL)
				break;

			if (fit_conf_get_node((void *)request, config) >= 0)
				goto exit;
		}
	}

	printf("%s Configuration not available in image\n",
		config ? config : g_config);

	printf("Please upgrade the image with %s supported device tree\n",
		config ? config : g_config);

	return CMD_RET_FAILURE;
exit:
	boot_info.config = found == true ? g_config : config;

	return 0;
}

#ifdef CONFIG_IPQ_SECURE
static int copy_rootfs(uint32_t request, uint32_t size)
{
	int ret = 0;
#ifdef CONFIG_MMC
	int curr_device = -1;
	uint32_t blk, cnt, n;
	void *addr;
	struct disk_partition disk_info;
	struct blkpart_info  bpart_info;

	BLK_PART_GET_INFO_S(bpart_info, NULL, &disk_info, SMEM_BOOT_MMC_FLASH,
				true);
#endif
	uint8_t	flash_type = gd->board_type & FLASH_TYPE_MASK;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();

	if (!sfi) {
		printf("%s: Failed to get flash info\n", __func__);
		return CMD_RET_FAILURE;
	}

	switch (flash_type) {
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_NORPLUSNAND:
		fallthrough;
	case SMEM_BOOT_QSPI_NAND_FLASH:
		ret = ubi_volume_read("ubi_rootfs",
					(char *)(uintptr_t)request,
					0, 0);
		break;
#endif
#ifdef CONFIG_MMC
	case SMEM_BOOT_MMC_FLASH:
		fallthrough;
	case SMEM_BOOT_NORPLUSEMMC:
		if (sfi->binfo != NULL) {
			if (boot_info.active_bank == 1)
				bpart_info.name = ROOTFS_INACTIVE_LABEL;
			else if (boot_info.active_bank == 0)
				bpart_info.name = ROOTFS_ACTIVE_LABEL;
			else
				return -EBADFD;
		} else {
			bpart_info.name = ROOTFS_ACTIVE_LABEL;
		}

		if (boot_info.debug)
			printf("[debug]Reading %s\n", bpart_info.name);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret == 0) {
			addr = (void *)(uintptr_t)request;
			blk = (uint32_t) disk_info.start;
			cnt = (uintptr_t) (size / disk_info.blksz) + 1;

			printf("\nMMC read: dev# %d, block# %d, count %d ... ",
				curr_device, blk, cnt);

			n = blk_dread(bpart_info.desc, blk, cnt, addr);
			printf("%d blocks read: %s\n", n,
					(n == cnt) ? "OK" : "ERROR");
			if (n != cnt)
				return CMD_RET_FAILURE;
		}
		break;
#endif
	default:
#ifdef CONFIG_IPQ_SPI_NOR
		ret = spi_flash_read(boot_info.flash, sfi->rootfs.offset,
					sfi->rootfs.size,
					(void *) (uintptr_t)request);
#endif
		break;
	}

	if (ret) {
		printf("rootfs read failed\n");
		return CMD_RET_FAILURE;
	}

	return 0;
}
#ifndef CONFIG_IPQ_ELF_AUTH
static int authenticate_rootfs(uintptr_t kernel_addr,
				struct kernel_img_info kernel_img_info)
{
	unsigned int kernel_imgsize;
	unsigned int request;
	int ret;
	struct mbn_header *mbn_ptr;
	struct auth_cmd_buf rootfs_img_info;
	struct scm_param param;

	request = CFG_ROOTFS_LOAD_ADDR;
	rootfs_img_info.addr = CFG_ROOTFS_LOAD_ADDR;
	rootfs_img_info.type = SEC_AUTH_SW_ID;
	request += sizeof(struct mbn_header);/* space for mbn header */

	/* get , kernel size = header + kernel + certificate */
	mbn_ptr = (struct mbn_header *) (uintptr_t)kernel_addr;
	kernel_imgsize = mbn_ptr->image_size + sizeof(struct mbn_header);

	/* get rootfs MBN header and validate it */
	mbn_ptr = (struct mbn_header *) (uintptr_t)((uint32_t)
		(uintptr_t)mbn_ptr + kernel_imgsize);
	if (mbn_ptr->image_type != ROOTFS_IMAGE_TYPE &&
			(mbn_ptr->code_size + mbn_ptr->signature_size +
			 mbn_ptr->cert_chain_size != mbn_ptr->image_size))
		return CMD_RET_FAILURE;

	/* pack, MBN header + rootfs + certificate */
	/* copy rootfs from the boot device */
	copy_rootfs(request, mbn_ptr->code_size);

	/* copy rootfs MBN header */
	memcpy((void *)CFG_ROOTFS_LOAD_ADDR,
			(void *) (uintptr_t)kernel_addr + kernel_imgsize,
			sizeof(struct mbn_header));

	/* copy rootfs certificate */
	memcpy((void *) (uintptr_t)request + mbn_ptr->code_size,
		(void *) (uintptr_t)(kernel_addr + kernel_imgsize +
				sizeof(struct mbn_header)),
		mbn_ptr->signature_size + mbn_ptr->cert_chain_size);

	/* copy rootfs size */
	rootfs_img_info.size = sizeof(struct mbn_header) + mbn_ptr->image_size;

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_SECURE_AUTHENTICATE(param, rootfs_img_info.type,
						rootfs_img_info.size,
						rootfs_img_info.addr, 0, 0);
		param.get_ret = true;
		ret = ipq_scm_call(&param);
	} while (0);

	memset((void *)(uintptr_t)kernel_img_info.kernel_load_addr,  0,
	       sizeof(struct mbn_header));

	memset(mbn_ptr,  0,
		(sizeof(struct mbn_header) + mbn_ptr->signature_size +
			mbn_ptr->cert_chain_size));

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return CMD_RET_FAILURE;
	}

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}
#else
static int authenticate_rootfs_elf_scm(uint32_t rootfs_hdr)
{
	int ret = -1;
	uint32_t request;
	struct ipq_image_info img_info;
	struct auth_cmd_buf rootfs_img_info;
	struct scm_param param;
#ifdef CONFIG_SHA384
	struct image_region root_data = {0, 0};
	int len = 0;
	char hash_buff[SHA384_SUM_LEN] = {0};
#endif
	if (parse_elf_image_phdr(&img_info, rootfs_hdr))
		return CMD_RET_FAILURE;

	request = img_info.img_load_addr - img_info.img_offset;
	rootfs_img_info.addr = request;

	memcpy((void *) (uintptr_t)request, (void *) (uintptr_t)rootfs_hdr,
			img_info.img_offset);

	request += img_info.img_offset;

	/* copy rootfs from the boot device */
	copy_rootfs(request, img_info.img_size);
#ifdef CONFIG_SHA384
	root_data.data  = (void *)(uintptr_t)img_info.img_load_addr;
	root_data.size  = img_info.img_size;

	len = sizeof(root_data) / sizeof(struct image_region);
	ret = hash_calculate("sha384", &root_data, len, hash_buff);
	if (ret) {
		printf("hash_calculate failed, ret %d", ret);
		return CMD_RET_FAILURE;
	}
#endif

#if IS_ENABLED(CONFIG_SCM_V1)
	rootfs_img_info.type = SEC_AUTH_SW_ID;
	rootfs_img_info.size = img_info.img_offset + img_info.img_size;
#elif IS_ENABLED(CONFIG_SCM_V2)
	rootfs_img_info.type = ROOTFS_SEC_AUTH_SW_ID;
	rootfs_img_info.size = img_info.img_offset - 1;
#endif

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_SECURE_AUTHENTICATE(param, rootfs_img_info.type,
						rootfs_img_info.size,
						rootfs_img_info.addr, 0, 0);
		param.get_ret = true;

		ret = ipq_scm_call(&param);

		if (ret || (param.res.result[0] && !ret)) {
			printf("Rootfs Authentication is failed\n");
			ret = CMD_RET_FAILURE;
			goto exit;
		}

	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		ret = CMD_RET_FAILURE;
		goto exit;
	}

#if IS_ENABLED(CONFIG_SCM_V2) && IS_ENABLED(CONFIG_SHA384)
	do {
		ret = -ENOTSUPP;
		IPQ_SCM_VERIFY_HASH(param, rootfs_img_info.type,
						rootfs_img_info.addr,
						rootfs_img_info.size,
						(uintptr_t) hash_buff,
						SHA384_SUM_LEN);
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("Rootfs integrity check filed\n");
			ret = CMD_RET_FAILURE;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		ret =  CMD_RET_FAILURE;
	}
#endif
exit:
	memset((void *)(uintptr_t)rootfs_hdr, 0, img_info.img_offset);

	return ret == CMD_RET_FAILURE ? ret : CMD_RET_SUCCESS;
}

#ifdef CONFIG_IPQ_TMEL_IPC_SUPPORT
#ifdef CONFIG_SECURE_AUTH_V2
static int authenticate_rootfs_elf_ipc_v2(uint32_t rootfs_hdr)
{
	int ret;
	u32 request;
	uint32_t rootfs_blob_addr;
	struct ipq_image_info rootfs_blob_info;
	struct secure_auth_params auth_params = {0};

	/* img_info is the file-scope static set by read_from_*() */
#ifdef CONFIG_IPQ_MVEW
	if (img_info.is_mvew_format) {
		if (!img_info.has_rootfs_meta) {
			printf("Rootfs auth required but image carries no rootfs metadata\n");
			return CMD_RET_FAILURE;
		}
		rootfs_blob_addr = boot_info.rootfs_meta_dram_addr;
	} else {
		rootfs_blob_addr = rootfs_hdr;
	}
#else
	rootfs_blob_addr = rootfs_hdr;
#endif

	/* re-parse rootfs metadata's own inner ELF for authoritative size */
	if (parse_elf_image_phdr(&rootfs_blob_info, rootfs_blob_addr))
		return CMD_RET_FAILURE;

	request = rootfs_blob_info.img_load_addr - rootfs_blob_info.img_offset;

	memcpy((void *)(uintptr_t)request, (void *)(uintptr_t)rootfs_blob_addr,
	       rootfs_blob_info.img_offset);

	request += rootfs_blob_info.img_offset;

	copy_rootfs(request, rootfs_blob_info.img_size);

	auth_params.type = ROOTFS_SEC_AUTH_SW_ID;
	auth_params.addr = rootfs_blob_info.img_load_addr - rootfs_blob_info.img_offset;
	auth_params.size = rootfs_blob_info.img_offset - 1;
	auth_params.load_seg_buff = NULL;
	auth_params.load_seg_cnt = 0;
	auth_params.load_seg_info_size = 0;
	auth_params.relocate = 0;

	ret = ipq_comm_handler(FUNC_SECURE_AUTH, &auth_params);
	memset((void *)(uintptr_t)rootfs_blob_addr, 0, rootfs_blob_info.img_offset);
	if (ret) {
		printf("Rootfs Authentication is failed\n");
		ret = CMD_RET_FAILURE;
	} else {
		ret = CMD_RET_SUCCESS;
	}

	return ret;
}
#endif /* CONFIG_SECURE_AUTH_V2 */

#ifdef CONFIG_SECURE_AUTH_V3
static int authenticate_rootfs_elf_ipc_v3(uint32_t rootfs_hdr)
{
	int ret;
	u32 request;
	uint32_t rootfs_blob_addr;
	struct ipq_image_info rootfs_blob_info;
	struct secure_auth_params auth_params = {0};

	/* img_info is the file-scope static set by read_from_*() */
#ifdef CONFIG_IPQ_MVEW
	if (img_info.is_mvew_format) {
		if (!img_info.has_rootfs_meta) {
			printf("Rootfs auth required but image carries no rootfs metadata\n");
			return CMD_RET_FAILURE;
		}
		rootfs_blob_addr = boot_info.rootfs_meta_dram_addr;
	} else {
		rootfs_blob_addr = rootfs_hdr;
	}
#else
	rootfs_blob_addr = rootfs_hdr;
#endif

	/* re-parse rootfs metadata's own inner ELF for authoritative size */
	if (parse_elf_image_phdr(&rootfs_blob_info, rootfs_blob_addr))
		return CMD_RET_FAILURE;

	request = rootfs_blob_info.img_load_addr - rootfs_blob_info.img_offset;

	memcpy((void *)(uintptr_t)request, (void *)(uintptr_t)rootfs_blob_addr,
	       rootfs_blob_info.img_offset);

	request += rootfs_blob_info.img_offset;

	copy_rootfs(request, rootfs_blob_info.img_size);

	auth_params.type = ROOTFS_SEC_AUTH_SW_ID;
	auth_params.addr = rootfs_blob_info.img_load_addr - rootfs_blob_info.img_offset;
	auth_params.size = rootfs_blob_info.img_offset - 1;
	auth_params.load_seg_buff = NULL;
	auth_params.load_seg_cnt = 0;
	auth_params.load_seg_info_size = 0;
	auth_params.relocate = 0;
	auth_params.flags = 1;

	ret = ipq_comm_handler(FUNC_SECURE_AUTH, &auth_params);
	memset((void *)(uintptr_t)rootfs_blob_addr, 0, rootfs_blob_info.img_offset);
	if (ret) {
		printf("Rootfs Authentication is failed\n");
		ret = CMD_RET_FAILURE;
	} else {
		ret = CMD_RET_SUCCESS;
	}
	return ret;
}
#endif /* CONFIG_SECURE_AUTH_V3 */
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

/* Wrapper implementations for comm_handler */
int ipq_auth_rootfs_elf_tme_impl(void *params)
{
#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT) && defined(CONFIG_SECURE_AUTH_V2)
	u32 rootfs_hdr = *(u32 *)params;

	return authenticate_rootfs_elf_ipc_v2(rootfs_hdr);
#elif defined(CONFIG_IPQ_TMEL_IPC_SUPPORT) && defined(CONFIG_SECURE_AUTH_V3)
	u32 rootfs_hdr = *(u32 *)params;

	return authenticate_rootfs_elf_ipc_v3(rootfs_hdr);
#else
	(void)params;
	return -EOPNOTSUPP;
#endif
}

int ipq_auth_rootfs_elf_scm_impl(void *params)
{
	u32 rootfs_hdr = *(u32 *)params;

	return authenticate_rootfs_elf_scm(rootfs_hdr);
}

static int authenticate_rootfs_elf(uint32_t rootfs_hdr)
{
	return ipq_comm_handler(FUNC_AUTH_ROOTFS_ELF, &rootfs_hdr);
}
#endif /* CONFIG_IPQ_ELF_AUTH */
#endif /* CONFIG_IPQ_SECURE */

int ipq_check_rootfs_authentication(void)
{
	u32 rootfs_auth = 0;
#ifdef ROOTFS_AUTH_FUSE
	 rootfs_auth = readl(ROOTFS_AUTH_FUSE) & ROOTFS_AUTH_EN;
#endif
	if (rootfs_auth || IS_ENABLED(CONFIG_IPQ_ROOTFS_AUTH))
		return 1;
	else
		return 0;
}

#ifdef CONFIG_IPQ_SECURE
static int image_authentication_scm(void)
{
	int ret;
	struct scm_param param;
	struct kernel_img_info kernel_img_info = {0, 0, 0};
#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
	int active_part = (boot_info.active_bank == 1) ?
				SECONDARY_PARTITION : PRIMARY_PARTITION;
#endif
	int secure_boot = is_board_support_image_auth();

	boot_info.stage = BOOT_STAGE_AUTH;

	if (!secure_boot)
		return CMD_RET_SUCCESS;

	if (boot_info.debug)
		printf("[debug]Authenticating Image\n");

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
	if (is_version_rollback_support()) {
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_SET_ACTIVE_PARTITION(param, active_part);
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("Partition info auth failed\n");
				goto clear_mem;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			goto clear_mem;
		}
	}
#endif
	kernel_img_info.kernel_load_addr = boot_info.load_address;
	kernel_img_info.kernel_load_size = boot_info.size;
	kernel_img_info.kernel_meta_data_size = boot_info.meta_data_size;

#ifndef CONFIG_IPQ_ELF_AUTH
	struct mbn_header *mbn_ptr = (struct mbn_header *)boot_info.load_address;

	boot_info.load_address += sizeof(struct mbn_header);
#else
	boot_info.load_address = img_info.img_load_addr;
#endif

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_AUTHENTICATE_KERNEL(param,
					kernel_img_info.kernel_load_addr,
					kernel_img_info.kernel_meta_data_size,
					KERNEL_SEC_AUTH_SW_ID, 0, 0);

		ret = ipq_scm_call(&param);
	} while (0);

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
clear_mem:
#endif
#ifndef CONFIG_IPQ_ELF_AUTH
	if (mbn_ptr->signature_ptr)
		memset((void *)(uintptr_t)mbn_ptr->signature_ptr, 0,
		       (mbn_ptr->signature_size + mbn_ptr->cert_chain_size));
#else
	if (kernel_img_info.kernel_load_addr)
		memset((void *)(uintptr_t)kernel_img_info.kernel_load_addr,  0,
		       img_info.img_offset);
#endif

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return CMD_RET_FAILURE;
	}

	if (ret) {
		printf("Kernel image authentication failed\n");
		return KERNEL_AUTH_FAILED;
	}

	gd->board_type |= KERNEL_AUTH_SUCCESS;

	if (boot_info.debug)
		printf("[debug]Kernel authenticated successfully\n");

	if (ipq_check_rootfs_authentication()) {
#ifdef CONFIG_IPQ_ELF_AUTH
		if (authenticate_rootfs_elf(img_info.img_load_addr +
			img_info.img_size) != CMD_RET_SUCCESS) {
			printf("Rootfs elf image authentication failed\n");
			return ROOTFS_AUTH_FAILED;
		}
#else
		/* Rootfs's header and certificate at end of kernel image,
		 * copy from there and pack with rootfs image and
		 * authenticate rootfs
		 */
		if (authenticate_rootfs(boot_info.load_address, kernel_img_info)
			!= CMD_RET_SUCCESS) {
			printf("Rootfs image authentication failed\n");
			return ROOTFS_AUTH_FAILED;
		}

#endif
		gd->board_type |= ROOTFS_AUTH_SUCCESS;

		if (boot_info.debug)
			printf("[debug]Rootfs authenticated successfully\n");
	}

	return 0;
}

#ifdef CONFIG_IPQ_TMEL_IPC_SUPPORT

#ifdef CONFIG_SECURE_AUTH_V2
static int image_authenticate_ipc_v2(void)
{
	int ret;
	struct kernel_img_info kernel_img_info = {0, 0, 0};
	struct secure_auth_params auth_params = {0};
	int secure_boot = is_board_support_image_auth();

	boot_info.stage = BOOT_STAGE_AUTH;

	if (!secure_boot)
		return CMD_RET_SUCCESS;

	if (boot_info.debug)
		printf("[debug]Authenticating Image with TME V2\n");

	kernel_img_info.kernel_load_addr = boot_info.load_address;
	kernel_img_info.kernel_load_size = boot_info.size;
	kernel_img_info.kernel_meta_data_size = boot_info.meta_data_size;

#ifndef CONFIG_IPQ_ELF_AUTH
	struct mbn_header *mbn_ptr = (struct mbn_header *)boot_info.load_address;

	boot_info.load_address += sizeof(struct mbn_header);
#else
	boot_info.load_address = img_info.img_load_addr;
#endif

	/* Set up kernel authentication parameters */
	auth_params.type = KERNEL_SEC_AUTH_SW_ID;
	auth_params.addr = kernel_img_info.kernel_load_addr;
	auth_params.size = kernel_img_info.kernel_meta_data_size;
	auth_params.load_seg_buff = NULL;
	auth_params.load_seg_cnt = 0;
	auth_params.load_seg_info_size = 0;
	auth_params.relocate = 0;

	ret = ipq_comm_handler(FUNC_SECURE_AUTH, &auth_params);

#ifndef CONFIG_IPQ_ELF_AUTH
	if (mbn_ptr->signature_ptr)
		memset((void *)(uintptr_t)mbn_ptr->signature_ptr, 0,
		       (mbn_ptr->signature_size + mbn_ptr->cert_chain_size));
#else
	if (kernel_img_info.kernel_load_addr)
		memset((void *)(uintptr_t)kernel_img_info.kernel_load_addr,  0,
		       img_info.img_offset);
#endif

	if (ret) {
		printf("Kernel image authentication failed\n");
		return KERNEL_AUTH_FAILED;
	}

	gd->board_type |= KERNEL_AUTH_SUCCESS;

	if (boot_info.debug)
		printf("[debug]Kernel authenticated successfully with TME V2\n");

	if (ipq_check_rootfs_authentication()) {
#ifdef CONFIG_IPQ_ELF_AUTH
		if (authenticate_rootfs_elf(img_info.img_load_addr +
			img_info.img_size) != CMD_RET_SUCCESS) {
			printf("Rootfs elf image authentication failed\n");
			return ROOTFS_AUTH_FAILED;
		}
#else
		/* Rootfs's header and certificate at end of kernel image,
		 * copy from there and pack with rootfs image and
		 * authenticate rootfs
		 */
		if (authenticate_rootfs(boot_info.load_address, kernel_img_info)
			!= CMD_RET_SUCCESS) {
			printf("Rootfs image authentication failed\n");
			return ROOTFS_AUTH_FAILED;
		}

#endif
		gd->board_type |= ROOTFS_AUTH_SUCCESS;

		if (boot_info.debug)
			printf("[debug]Rootfs authenticated successfully with TME V2\n");
	}

	return 0;
}
#endif /* CONFIG_SECURE_AUTH_V2 */

#ifdef CONFIG_SECURE_AUTH_V3
static int image_authenticate_ipc_v3(void)
{
	int ret;
	struct kernel_img_info kernel_img_info = {0, 0, 0};
	struct secure_auth_params auth_params = {0};
	int secure_boot = is_board_support_image_auth();

	boot_info.stage = BOOT_STAGE_AUTH;

	if (!secure_boot)
		return CMD_RET_SUCCESS;

	if (boot_info.debug)
		printf("[debug]Authenticating Image with TME V3\n");

	kernel_img_info.kernel_load_addr = boot_info.load_address;
	kernel_img_info.kernel_load_size = boot_info.size;
	kernel_img_info.kernel_meta_data_size = boot_info.meta_data_size;

#ifndef CONFIG_IPQ_ELF_AUTH
	struct mbn_header *mbn_ptr = (struct mbn_header *)boot_info.load_address;

	boot_info.load_address += sizeof(struct mbn_header);
#else
	boot_info.load_address = img_info.img_load_addr;
#endif

	/* Set up kernel authentication parameters */
	auth_params.type = KERNEL_SEC_AUTH_SW_ID;
	auth_params.addr = kernel_img_info.kernel_load_addr;
	auth_params.size = kernel_img_info.kernel_meta_data_size;
	auth_params.load_seg_buff = NULL;
	auth_params.load_seg_cnt = 0;
	auth_params.load_seg_info_size = 0;
	auth_params.relocate = 0;
	auth_params.flags = 1;

	ret = ipq_comm_handler(FUNC_SECURE_AUTH, &auth_params);

#ifndef CONFIG_IPQ_ELF_AUTH
	if (mbn_ptr->signature_ptr)
		memset((void *)(uintptr_t)mbn_ptr->signature_ptr, 0,
		       (mbn_ptr->signature_size + mbn_ptr->cert_chain_size));
#else
	if (kernel_img_info.kernel_load_addr)
		memset((void *)(uintptr_t)kernel_img_info.kernel_load_addr,  0,
		       img_info.img_offset);
#endif

	if (ret) {
		printf("Kernel image authentication failed\n");
		return KERNEL_AUTH_FAILED;
	}

	gd->board_type |= KERNEL_AUTH_SUCCESS;

	if (boot_info.debug)
		printf("[debug]Kernel authenticated successfully with TME V3\n");

	if (ipq_check_rootfs_authentication()) {
#ifdef CONFIG_IPQ_ELF_AUTH
		if (authenticate_rootfs_elf(img_info.img_load_addr +
			img_info.img_size) != CMD_RET_SUCCESS) {
			printf("Rootfs elf image authentication failed\n");
			return ROOTFS_AUTH_FAILED;
		}
#else
		/* Rootfs's header and certificate at end of kernel image,
		 * copy from there and pack with rootfs image and
		 * authenticate rootfs
		 */
		if (authenticate_rootfs(boot_info.load_address, kernel_img_info)
			!= CMD_RET_SUCCESS) {
			printf("Rootfs image authentication failed\n");
			return ROOTFS_AUTH_FAILED;
		}

#endif
		gd->board_type |= ROOTFS_AUTH_SUCCESS;

		if (boot_info.debug)
			printf("[debug]Rootfs authenticated successfully with TME V3\n");
	}

	return 0;
}
#endif /* CONFIG_SECURE_AUTH_V3 */
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

/* Wrapper implementations for comm_handler */
int ipq_image_auth_tme_impl(void *params)
{
#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT) && defined(CONFIG_SECURE_AUTH_V2)
	return image_authenticate_ipc_v2();
#elif defined(CONFIG_IPQ_TMEL_IPC_SUPPORT) && defined(CONFIG_SECURE_AUTH_V3)
	return image_authenticate_ipc_v3();
#else
	return -EOPNOTSUPP;
#endif
}

int ipq_image_auth_scm_impl(void *params)
{
	return image_authentication_scm();
}

/* Wrapper function to call appropriate version based on config */
int image_authentication(void)
{
	return ipq_comm_handler(FUNC_IMAGE_AUTH, NULL);
}
#endif

int read_kernel(void)
{
	int ret;
	int flash_type = gd->board_type & FLASH_TYPE_MASK;

	boot_info.stage = BOOT_STAGE_READ;

	if (boot_info.debug)
		printf("[debug]Loading Kernel\n");

	switch (flash_type) {
#ifdef CONFIG_MMC
	case SMEM_BOOT_MMC_FLASH:
	case SMEM_BOOT_NORPLUSEMMC:
		ret = read_from_mmc();
		break;
#endif
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_QSPI_NAND_FLASH:
	case SMEM_BOOT_NORPLUSNAND:
		ret = read_from_nand();
		break;
#endif
#ifdef CONFIG_IPQ_SPI_NOR
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		ret = read_from_nor();
		break;
#endif
	default:
		printf("Unsupported BOOT flash type\n");
		printf("Booting not support in recovery mode\n");

		return -1;
	}

	return ret;
}

#ifdef CONFIG_IPQ_MVEW
/* boot_stage: relocate selected MVEW metadata next to .itb in DRAM,
 * using the authoritative size from the metadata blob's own inner ELF
 * rather than the outer wrapper's collage-authored p_filesz
 */
int relocate_mvew_metadata(void)
{
	struct ipq_image_info meta_info;
	int secure_boot = is_board_support_image_auth();

	if (!secure_boot)
		return CMD_RET_SUCCESS;

	if (!img_info.is_mvew_format)
		return CMD_RET_SUCCESS;

	/* capture before boot_info.load_address is reassigned below */
	if (img_info.has_rootfs_meta) {
		boot_info.rootfs_meta_dram_addr =
			boot_info.load_address +
				img_info.rootfs_meta_flash_offset;
	}

	if (parse_elf_image_phdr(&meta_info,
			boot_info.load_address + img_info.meta_flash_offset))
		return CMD_RET_FAILURE;

	img_info.img_offset = meta_info.img_offset;

	memmove((void *)(uintptr_t)(img_info.img_load_addr - img_info.img_offset),
		(uint8_t *)boot_info.load_address + img_info.meta_flash_offset,
		img_info.img_offset);

	boot_info.load_address = img_info.img_load_addr - img_info.img_offset;
	boot_info.meta_data_size = img_info.img_offset;

	if (boot_info.debug)
		printf("[debug]relocated MVEW metadata, kernel @ 0x%lx\n",
				boot_info.load_address);

	return CMD_RET_SUCCESS;
}
#endif

int boot_kernel(void)
{
	char boot_cmd[CONFIG_SYS_MAXARGS];

	boot_info.stage = BOOT_STAGE_BOOT;
	/*
	 * set fdt_high parameter so that u-boot will not load
	 * dtb above FDT_HIGH region.
	 */
	if (env_set("fdt_high", MK_STR(FDT_HIGH)))
		return CMD_RET_FAILURE;

	if (boot_info.config) {
		snprintf(boot_cmd, sizeof(boot_cmd),
			"bootm 0x%lx#%s\n",
			 boot_info.load_address, boot_info.config);
	} else {
		snprintf(boot_cmd, sizeof(boot_cmd),
			"bootm 0x%lx\n", boot_info.load_address);
	}

	return run_command(boot_cmd, 0);
}

int check_bootconfig(void)
{
	int active_part = ipq_get_valid_bank();
#ifdef CONFIG_BOOTCONFIG_V3
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	struct ipq_smem_bootconfig_info *binfo;

	if (!sfi) {
		printf("%s: smem flash info not found\n", __func__);
		return CMD_RET_FAILURE;
	}
	if (active_part < 0) {
		printf("INVALID BOOTCONFIG DATA %d!!!\n", -EINVAL);
		printf("Bootconfig will be restored on the next boot\n");

		return CMD_RET_FAILURE;
	}

	binfo = sfi->binfo;

	if ((binfo != NULL) &&
		(binfo->image_set_status == DONT_USE_SET_AB)) {
		printf("Invalid Kernel image on SET A & B\n");
		printf("Please Recover the setup or flash valid kernel\n");

		return CMD_RET_FAILURE;
	}
#endif

	boot_info.active_bank = active_part;

	return CMD_RET_SUCCESS;
}


static const boot_stage state_sequence[] = {
	check_bootconfig,
	read_kernel,
#ifdef CONFIG_IPQ_MVEW
	relocate_mvew_metadata,
#endif
#ifdef CONFIG_IPQ_SECURE
	image_authentication,
#endif
	config_select,
	set_bootargs,
	boot_kernel,
	NULL
};

static int do_bootipq(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	int state = 1, ret = CMD_RET_SUCCESS;
	const boot_stage *state_sequence_ptr = state_sequence;

	if (argc == 2 && strncmp(argv[1], "debug", 5) == 0)
		boot_info.debug = true;

#if defined(CONFIG_WDT) && !defined(CONFIG_IPQ_STOP_WDT)
	if (!boot_info.debug)
		ipq_wdt_start(true);
#endif

#ifdef CFG_CUSTOM_LOAD_ADDR
	boot_info.load_address = CFG_CUSTOM_LOAD_ADDR;
#else
	boot_info.load_address = CONFIG_SYS_LOAD_ADDR;
#endif

	boot_info.stage = BOOT_STAGE_READ_BC;

	for (; *state_sequence_ptr; ++state_sequence_ptr, state++) {
		ret = (*state_sequence_ptr)();
		if (ret)
			break;

#ifdef CONFIG_SKIP_RESET
	if (ipq_iscrashed() && boot_info.stage == BOOT_STAGE_AUTH)
		break;
#endif
	}

	if (ret) {
		switch (boot_info.stage) {
		case BOOT_STAGE_READ:
			printf("Failed to read from flash device %d\n", -EIO);
			printf("Please Check Flash device status\n");
			fallthrough;
		default:
			printf("Failed at state %d : %s\n",
				state, stages[boot_info.stage]);
			break;
		}
	}

#if !defined(CONFIG_FAILSAFE) && !defined(CONFIG_FAILSAFE_V2)
	if (ret == ROOTFS_AUTH_FAILED || ret == KERNEL_AUTH_FAILED)
		BUG();
#endif

#ifdef CONFIG_WDT
	if (ret) {
#ifdef CONFIG_FAILSAFE_V2
		struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
		if (sfi && sfi->smembootmode == BOOT_MODE_FORCE_INACTIVE)
			printf("force inactive failed\n");
#endif
		printf("Invoking watchdog!!!\n");
		ipq_wdt_expire();
	}
#endif

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bootipq, 2, 0, do_bootipq,
	"bootipq from flash device",
	"bootipq [debug] - Load image(s) and boots the kernel\n"
	);
