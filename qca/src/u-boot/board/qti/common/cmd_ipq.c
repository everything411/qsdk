/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <memalign.h>
#include <cpu_func.h>
#include <linux/bug.h>
#include <linux/arm-smccc.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <elf.h>
#include <linux/iopoll.h>
#ifdef CONFIG_IPQ_QCN9224_FUSING
#include <init.h>
#include <pci.h>
#include <dt-bindings/pci/pci.h>
#include <asm/io.h>
#endif
#include <serial.h>
#ifdef CONFIG_QSPI_LAYOUT_SWITCH
#include <nand.h>
#include <ubi_uboot.h>
#include <ubifs_uboot.h>
#endif

#ifdef CONFIG_IPQ_MMC
#include <mmc.h>
#endif

#ifdef CONFIG_WDT
#include <dm/uclass-internal.h>
#include <wdt.h>
#endif

#include "ipq_board.h"

#if defined (CONFIG_IPQ_SMP_CMD_SUPPORT) || (CONFIG_IPQ_SMP64_CMD_SUPPORT)
#include <cli.h>
#include <console.h>

DECLARE_GLOBAL_DATA_PTR;

#define SECONDARY_CORE_STACKSZ	(8 * 1024)
#define CPU_POWER_DOWN		(1 << 16)

#if defined (CONFIG_IPQ_SMP_CMD_SUPPORT)
struct cpu_entry_arg {
	void *stack_ptr;
	volatile void *gd_ptr;
	void *arg_ptr;
	int  cpu_up;
	int cmd_complete;
	int cmd_result;
	void *stack_top_ptr;
};
#elif defined (CONFIG_IPQ_SMP64_CMD_SUPPORT)
struct cpu_entry_arg {
	void *stack_ptr;
	volatile void *gd_ptr;
	void *arg_ptr;
	int64_t  cpu_up;
	int64_t cmd_complete;
	int64_t cmd_result;
	void *stack_top_ptr;
};
#endif

extern void secondary_cpu_init(void);
extern void *global_core_array;

struct cpu_entry_arg core[CFG_NR_CPUS - 1];

#endif /* CONFIG_IPQ_SMP_CMD_SUPPORT || CONFIG_IPQ_SMP64_CMD_SUPPORT */

#define PRINT_BUF_LEN		0x400
#define MDT_SIZE		0x1B88
/* Region for loading test application */
#define TZT_LOAD_ADDR		0x49600000
/* Reserved size for application */
#define TZT_LOAD_SIZE		0x00200000

#define XPU_TEST_ID		0x80100004

static int tzt_loaded;

struct udevice *dev;

struct xpu_tzt {
	uint64_t test_id;
	uint64_t num_param;
	uint64_t param1;
	uint64_t param2;
	uint64_t param3;
	uint64_t param4;
	uint64_t param5;
	uint64_t param6;
	uint64_t param7;
	uint64_t param8;
	uint64_t param9;
	uint64_t param10;
};

struct resp {
	uint64_t status;
	uint64_t index;
	uint64_t total_tests;
};

struct log_buff {
	uint16_t wrap;
	uint16_t log_pos;
	char buffer[PRINT_BUF_LEN];
};

#ifdef CONFIG_IPQ_QCN9224_FUSING
struct jtag_ids {
        u32 id;
        char *name;
};

struct jtag_ids qcn9224_jtag_ids[] = {
        { 0x101D50E1, "QCN9274" },
        { 0x101D80E1, "QCN9272" },
        { 0x101ED0E1, "QCN6214" },
        { 0x101EE0E1, "QCN6224" },
        { 0x101EF0E1, "QCN6274" },
};

enum {
	PCI_LIST_QCN9224_FUSE = 0,
	PCI_FUSE_QCN9224,
	PCI_DETECT_QCN9224,
	PCI_CAL_QCN9224,
	PCI_CHECK_CAL_QCN9224,
	PCI_LIST
};
#endif /* CONFIG_IPQ_QCN9224_FUSING */

#define PRI_PARTITION	1
#define ALT_PARTITION	2

#define FUSEPROV_SUCCESS		0x0
#define FUSEPROV_INVALID_HASH		0x09
#define SEC_IMG_AUTH_FAILURE		0x101

#define MAX_FUSE_ADDR_SIZE		0x8

typedef struct load_seg_info {
	uint32_t startAddr;       /**< Region start address (SoC view) */
	uint32_t endAddr;         /**< Region end address (SoC view) */
} load_seg_info_t;

extern struct pci_device_id device_table[];
/*
 * Interpret the ELF-64bit program header to retrieve
 * the offset and filesize of the LOAD segment,
 * storing them into memory allocated at runtime.
 * Remember to clear this memory after it has been utilized
 */
load_seg_info_t *elf64_load_seg(void *img_addr, uint8_t* load_seg_cnt,
					ulong *meta_data_size)
{
	load_seg_info_t *load_seg_info = NULL;
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)img_addr;
	Elf64_Phdr *phdr = (Elf64_Phdr *)(uintptr_t)(img_addr + ehdr->e_phoff);
	uint8_t lds_cnt = 0;

	/*
	 * Considering that first two segment of
	 * program header is not a loadable segment,
	 * ignoring it for memory allocation
	 */
	load_seg_info = (load_seg_info_t*) malloc(sizeof(load_seg_info_t) *
						(ehdr->e_phnum - 2));
	if (!load_seg_info) {
		printf("Unable to allocate memory\n");
		return NULL;
	}
	memset(load_seg_info, 0, sizeof(load_seg_info_t) * (ehdr->e_phnum - 2));

	for (int i = 0; i < ehdr->e_phnum; ++phdr, i++) {
		if (phdr->p_type == PT_LOAD) {
			if (!lds_cnt && *meta_data_size == 0) {
				--phdr;
				*meta_data_size = phdr->p_offset +
						  phdr->p_filesz;
				++phdr;
			}

			if (!phdr->p_filesz && !phdr->p_memsz)
				continue;
			if (!phdr->p_filesz && phdr->p_memsz) {
				load_seg_info[lds_cnt].startAddr = 0;
				load_seg_info[lds_cnt++].endAddr = 0;
				continue;
			}

			load_seg_info[lds_cnt].startAddr = phdr->p_offset +
						(ulong)(uintptr_t) img_addr;
			load_seg_info[lds_cnt++].endAddr = phdr->p_filesz +
						(ulong)(uintptr_t) img_addr +
						phdr->p_offset;
		}
	}

	*load_seg_cnt = lds_cnt;

	return load_seg_info;

}

/*
 * Interpret the ELF-32bit program header to retrieve
 * the offset and filesize of the LOAD segment,
 * storing them into memory allocated at runtime.
 * Remember to clear this memory after it has been utilized
 */
load_seg_info_t *elf32_load_seg(void *img_addr, uint8_t* load_seg_cnt,
					ulong *meta_data_size)
{
	load_seg_info_t *load_seg_info = NULL;
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)(uintptr_t)img_addr;
	Elf32_Phdr *phdr = (Elf32_Phdr *)(uintptr_t)(img_addr + ehdr->e_phoff);
	uint8_t lds_cnt = 0;

	/*
	 * Considering that first two segment of
	 * program header is not a loadable segment,
	 * ignoring it for memory allocation
	 */
	load_seg_info = (load_seg_info_t*) malloc(sizeof(load_seg_info_t) *
						(ehdr->e_phnum - 2));
	if (!load_seg_info) {
		printf("Unable to allocate memory\n");
		return NULL;
	}

	memset(load_seg_info, 0, sizeof(load_seg_info_t) * (ehdr->e_phnum - 2));

	for (int i = 0; i < ehdr->e_phnum; ++phdr, i++) {
		if (phdr->p_type == PT_LOAD) {
			if (!lds_cnt && *meta_data_size == 0) {
				--phdr;
				*meta_data_size = phdr->p_offset +
						  phdr->p_filesz;
				++phdr;
			}

			if (!phdr->p_filesz && !phdr->p_memsz)
				continue;
			if (!phdr->p_filesz && phdr->p_memsz) {
				load_seg_info[lds_cnt].startAddr = 0;
				load_seg_info[lds_cnt++].endAddr = 0;
				continue;
			}

			load_seg_info[lds_cnt].startAddr = phdr->p_offset +
						(ulong)(uintptr_t) img_addr;
			load_seg_info[lds_cnt++].endAddr = phdr->p_filesz +
						(ulong)(uintptr_t) img_addr +
						phdr->p_offset;
		}
	}

	*load_seg_cnt = lds_cnt;

	return load_seg_info;

}

load_seg_info_t *parse_n_extract_ld_segment(void *img_addr,
						uint8_t* load_seg_cnt,
						ulong *meta_data_size)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)img_addr;
	load_seg_info_t *load_seg_info = NULL;

	if (ehdr->e_ident[EI_CLASS] == ELFCLASS64)
		load_seg_info = elf64_load_seg(img_addr, load_seg_cnt,
							meta_data_size);
	else if (ehdr->e_ident[EI_CLASS] == ELFCLASS32)
		load_seg_info = elf32_load_seg(img_addr, load_seg_cnt,
							meta_data_size);

	return load_seg_info;
}

static int do_secure(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret = CMD_RET_FAILURE;
	int scm_ret = 0;
	load_seg_info_t *load_seg_buff = NULL;
	uint8_t load_seg_cnt = 0;

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
	int active_part = PRI_PARTITION;
#endif /* CONFIG_VERSION_ROLLBACK_PARTITION_INFO */

	if (strncmp(argv[0], "is_sec_boot_enabled", 19) == 0 && argc == 1) {
		if (argc != 1)
			return CMD_RET_USAGE;

		printf("secure boot fuse is%senabled\n",
				(gd->board_type & SECURE_BOARD) ? " " :
							" not ");
		ret = 0;

	} else if(strncmp(argv[0], "secure_authenticate", 19) == 0) {

#ifdef CONFIG_SECURE_AUTH_V1
		if(argc != 4)
#elif CONFIG_SECURE_AUTH_V2
		if(argc != 3 && argc !=4)
#endif
			return CMD_RET_USAGE;

		auth_cmd_buf auth_buf = {0};
		scm_param param;

		auth_buf.type = simple_strtoul(argv[1], NULL, 16);
		auth_buf.addr = simple_strtoul(argv[2], NULL, 16);

		do {
			scm_ret = -ENOTSUPP;
			IPQ_SCM_CHECK_SCM_SUPPORT(param,
						SCM_SMC_FNID(QCOM_SCM_SVC_BOOT,
						QCOM_SCM_SEC_AUTH_CMD) |
						(ARM_SMCCC_OWNER_SIP <<
						ARM_SMCCC_OWNER_SHIFT));
			param.get_ret = true;
			scm_ret = ipq_scm_call(&param);

			if (scm_ret || (!scm_ret &&
				le32_to_cpu(param.res.result[0]) <= 0)) {
				printf("secure authentication scm call"
					" is not supported. ret = %d\n", ret);
				ret = CMD_RET_SUCCESS;
				goto exit;
			}
		} while(0);

		if (scm_ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			ret = CMD_RET_FAILURE;
			goto exit;
		}

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
		if (is_version_rollback_support()) {
			active_part = gd->board_type & ACTIVE_BOOT_SET?
						ALT_PARTITION : PRI_PARTITION;
			do {
				scm_ret = -ENOTSUPP;
				IPQ_SCM_SET_ACTIVE_PARTITION(param, active_part);
				scm_ret = ipq_scm_call(&param);

				if(scm_ret) {
					printf("Partition info authentication "
									"failed\n");
					BUG(); //:TODO check if BUG is necessary
				}
			} while(0);

			if (scm_ret == -ENOTSUPP) {
				printf("Unsupported SCM call\n");
				ret =  CMD_RET_FAILURE;
				goto exit;
			}
		}
#endif /* CONFIG_VERSION_ROLLBACK_PARTITION_INFO */

#ifdef CONFIG_SECURE_AUTH_V1
		auth_buf.size = simple_strtoul(argv[3], NULL, 16);
#elif CONFIG_SECURE_AUTH_V2
		void *load_addr = (void*)(uintptr_t)auth_buf.addr;
		if(argc == 4)
			auth_buf.size = simple_strtoul(argv[3], NULL, 16);

		if (!load_addr || !IS_ELF(*(Elf32_Ehdr *)load_addr)) {
			printf("It is not a elf image \n");
			goto exit;
		}

		if ((*(Elf32_Ehdr *)load_addr).e_ident[EI_CLASS] == ELFCLASS32
				&& ((Elf32_Ehdr *)load_addr)->e_phnum < 3) {
				printf("Invalid image\n");
				goto exit;
		}

		if ((*(Elf64_Ehdr *)load_addr).e_ident[EI_CLASS] == ELFCLASS64
				&& ((Elf64_Ehdr *)load_addr)->e_phnum < 3) {
				printf("Invalid image\n");
				goto exit;
		}


		load_seg_buff = parse_n_extract_ld_segment(
					(void *)(uintptr_t)auth_buf.addr,
					&load_seg_cnt, &auth_buf.size);

		if (!load_seg_buff)
			goto exit;

		load_seg_cnt = load_seg_cnt * sizeof(load_seg_info_t);
#endif

		do {
			scm_ret = -ENOTSUPP;
			IPQ_SCM_SECURE_AUTHENTICATE(param, auth_buf.type,
						auth_buf.size, auth_buf.addr,
						(uintptr_t)load_seg_buff,
						load_seg_cnt);
			param.get_ret = true;
			scm_ret = ipq_scm_call(&param);

			if(scm_ret || (param.res.result[0] && !scm_ret)) {
				printf("image authentication failed. "
							"ret  = %d\n",
							ret);
				ret = CMD_RET_FAILURE;
			} else {
				printf("image authentication success\n");
				ret = CMD_RET_SUCCESS;
			}
		} while (0);

		if (scm_ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			ret =  CMD_RET_FAILURE;
		}
	}
	else {
		return CMD_RET_USAGE;
	}

exit:
	if (load_seg_buff)
		free(load_seg_buff);

	return ret;
}

U_BOOT_CMD(is_sec_boot_enabled, 1, 0, do_secure,
		"check secure boot fuse is enabled or not\n",
		"is_sec_boot_enabled - check secure boot fuse "
		"is enabled or not\n");
U_BOOT_CMD(secure_authenticate, 4, 0, do_secure,
		"authenticate the signed image\n",
#ifdef CONFIG_SECURE_AUTH_V1
		"secure_authenticate <sw_id> <img_addr> <img_size>\n"
#elif CONFIG_SECURE_AUTH_V2
		"secure_authenticate <sw_id> <img_addr> [meta_data_size]\n"
#endif
		"	- authenticate the signed image\n");

#ifdef CONFIG_FUSE_IPQ

#ifdef CONFIG_FUSEIPQ_V1
#define FUSEIPQ_MAX_ARGS 3

char fuseipq_cmd_usage[] = ("\nfuseipq [address] [size]  - Load fuse(s) and"
				" blows in the qfprom\n\nDescription:\n"
				"[address] = load address\n"
				"[size] = size of the elf file\n");
#elif CONFIG_FUSEIPQ_V2
#define FUSEIPQ_MAX_ARGS 2
char fuseipq_cmd_usage[] = ("\nfuseipq [address]  - Load fuse(s) and blows"
				" in the qfprom\n\nDescription:\n"
				"[address] = load address\n");
#endif

static int
do_fuseipq(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_FAILURE;
	scm_param param;
	uint32_t fuse_status = 0;
	uint32_t fuse_bin_addr = 0;
#ifdef CONFIG_FUSEIPQ_V1
	uint64_t fuse_bin_size = 0;
#endif
	load_seg_info_t *load_seg_buff = NULL;
	uint8_t load_seg_cnt = 0;
	unsigned long meta_data_size = 0;
	void* load_addr = NULL;

	if (argc == 1)
		return CMD_RET_USAGE;

	watchdog_reset();

#ifdef CONFIG_FUSEIPQ_V2
	if (argc != 2)
		return CMD_RET_USAGE;
#endif

	fuse_bin_addr = simple_strtoul(argv[1], NULL, 16);
	load_addr = (void*)(uintptr_t)fuse_bin_addr;
#ifdef CONFIG_FUSEIPQ_V1
	if (IS_ELF(*(Elf32_Ehdr *)load_addr)) {
		if (argc != 3)
			return CMD_RET_USAGE;
		fuse_bin_size = simple_strtoul(argv[2], NULL, 16);
	} else {
		if (argc != 2)
			return CMD_RET_USAGE;
	}
#endif

#ifdef CONFIG_FUSEIPQ_V2
	if (!load_addr || !IS_ELF(*(Elf32_Ehdr *)load_addr)) {
		printf("It is not a elf image \n");
		goto exit;
	}

	if ((*(Elf32_Ehdr *)load_addr).e_ident[EI_CLASS] == ELFCLASS32
			&& ((Elf32_Ehdr *)load_addr)->e_phnum < 3) {
			printf("Invalid image\n");
			goto exit;
	}

	if ((*(Elf64_Ehdr *)load_addr).e_ident[EI_CLASS] == ELFCLASS64
			&& ((Elf64_Ehdr *)load_addr)->e_phnum < 3) {
			printf("Invalid image\n");
			goto exit;
	}


	load_seg_buff = parse_n_extract_ld_segment(
				(void *)(uintptr_t)fuse_bin_addr,
				&load_seg_cnt,
				&meta_data_size);
	if(!load_seg_buff)
		goto exit;

	load_seg_cnt = load_seg_cnt * sizeof(load_seg_info_t);
#endif
	do {
		ret = -ENOTSUPP;
		IPQ_SCM_FUSE_IPQ(param, (uint64_t) fuse_bin_addr,
					meta_data_size, 0x2B,
					(uintptr_t)load_seg_buff,
					load_seg_cnt);
#ifdef CONFIG_FUSEIPQ_V1
	if (IS_ELF(*(Elf32_Ehdr *)load_addr)) {
		param.type = SCM_FUSE_IPQ_UIE_KEY;
		param.buff[1] = fuse_bin_size;
		param.arg_type[1] = SCM_VAL;
		param.len = 2;
	}
#endif
		param.get_ret = true;
		/*
		 * Disable data cache to ensure direct memory access during
		 * fuse operation
		 */
		dcache_disable();
		ret = ipq_scm_call(&param);
		dcache_enable();
		invalidate_dcache_all();

		fuse_status = param.res.result[0];

		if (ret)
			printf("%s: Error in QFPROM write (%d)\n",
				__func__, ret);
		else {
			switch (fuse_status) {
			case FUSEPROV_SUCCESS:
				printf("Fuse Blow Success\n");
				break;
			case FUSEPROV_INVALID_HASH:
				printf("Invalid sec.dat\n");
				break;
#ifdef CONFIG_FUSEIPQ_V1
			case SEC_IMG_AUTH_FAILURE:
				printf("Image authentication failure\n");
				break;
#endif
			default:
				printf("Fuse blow failed with err code :"
					" 0x%x\n", fuse_status);
			}
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
	}

	ret = CMD_RET_SUCCESS;

#ifdef CONFIG_FUSEIPQ_V2
exit:
#endif
	if (load_seg_buff)
		free(load_seg_buff);

	return ret;
}

U_BOOT_CMD(fuseipq, FUSEIPQ_MAX_ARGS, 0, do_fuseipq,
		"fuse QFPROM registers from memory",
		fuseipq_cmd_usage);
#endif

#ifdef CONFIG_LIST_FUSE
static int do_list_fuse(struct cmd_tbl *cmdtp, int flag, int argc,
					char *const argv[])
{
	int ret;
	int index = 0;
	struct fuse_payload *fuse = NULL;
	scm_param param;
	uint8_t fuse_read_cnt = TME_OEM_ATE_FUSE_CNT +
				TME_OEM_MRC_HASH_FUSE_CNT;
	size_t size = sizeof(struct fuse_payload ) * fuse_read_cnt;

	size = roundup(size, CONFIG_SYS_CACHELINE_SIZE);

	fuse = malloc_cache_aligned(size);
	if (fuse == NULL) {
		return CMD_RET_FAILURE;
	}

	memset(fuse, 0, size);

	watchdog_reset();

	for (index = 0; index < fuse_read_cnt ; index++) {
		if (index < TME_OEM_ATE_FUSE_CNT) {
			fuse[index].fuse_addr = TME_OEM_ATE_FUSE_START +
					(TME_OEM_ATE_FUSE_READ_SIZE * index);
		} else {
			fuse[index].fuse_addr = TME_OEM_MRC_HASH_FUSE_START +
					(TME_OEM_MRC_HASH_FUSE_READ_SIZE *
					 (index - TME_OEM_ATE_FUSE_CNT));
		}
	}

	/* invalidate cache to update latest value in buff */
	do {
		ret = -ENOTSUPP;
		IPQ_SCM_READ_FUSE(param, (unsigned long)fuse,
			sizeof(struct fuse_payload ) * fuse_read_cnt);

		flush_dcache_range((unsigned long)fuse,
					(unsigned long)fuse +
					size);
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("Error (%d) failed to read fuse\n", ret);
			ret = CMD_RET_FAILURE;
		} else
			ret = CMD_RET_SUCCESS;

		printf("Fuse Name\tAddress\t\tValue\n");
		printf("------------------------------------------------\n");
#ifdef CONFIG_LIST_FUSE_V1
		printf("TME_AUTH_EN\t0x%08X\t0x%08X\n", fuse[0].fuse_addr,
				fuse[0].val & TME_AUTH_EN_MASK);
		printf("TME_OEM_ID\t0x%08X\t0x%08X\n", fuse[0].fuse_addr,
				fuse[0].val & TME_OEM_ID_MSK);
		printf("TME_PRODUCT_ID\t0x%08X\t0x%08X\n",
				fuse[1].fuse_addr,
				fuse[1].val & TME_PRODUCT_ID_MSK);

		for (index = TME_OEM_ATE_FUSE_CNT;
				index < fuse_read_cnt; index++) {
			printf("TME_MRC_HASH\t0x%08X\t0x%08X\n",
			fuse[index].fuse_addr, fuse[index].val);
		}
#elif CONFIG_LIST_FUSE_V2
		printf("tme_auth_en\t0x%08x\t0x%08x\n", fuse[0].fuse_addr,
				fuse[0].lsb_val & TME_AUTH_EN_MASK);
		printf("tme_oem_id\t0x%08x\t0x%08x\n", fuse[0].fuse_addr,
				fuse[0].lsb_val & TME_OEM_ID_MSK);
		printf("tme_product_id\t0x%08x\t0x%08x\n",
				fuse[0].fuse_addr + 0x4,
				fuse[0].msb_val & TME_PRODUCT_ID_MSK);

		for (index = TME_OEM_ATE_FUSE_CNT;
				index < fuse_read_cnt; index++) {
			printf("tme_mrc_hash\t0x%08x\t0x%08x\n",
			fuse[index].fuse_addr, fuse[index].lsb_val);
			printf("tme_mrc_hash\t0x%08x\t0x%08x\n",
			fuse[index].fuse_addr + 0x4, fuse[index].msb_val);
		}
#endif
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		ret = CMD_RET_FAILURE;
	}

	free(fuse);
	return ret;
}

U_BOOT_CMD(list_fuse, 1, 0, do_list_fuse,
		"fuse set of QFPROM registers from memory\n",
		"");

static int do_dump_fuse(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	size_t size = sizeof(struct fuse_payload);
	struct fuse_payload *fuse = NULL;
	scm_param param;
	u32 addr;
	int ret;

	size = roundup(size, CONFIG_SYS_CACHELINE_SIZE);

	if (argc != 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], NULL, 16);

	fuse = malloc_cache_aligned(size);
	if (fuse == NULL)
		return CMD_RET_FAILURE;

	memset(fuse, 0, size);
	fuse->fuse_addr = addr;

	watchdog_reset();

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_READ_FUSE(param, (unsigned long)fuse,
				  sizeof(struct fuse_payload));

		flush_dcache_range((unsigned long)fuse, (unsigned long)fuse +
				   size);
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("Error (%d) failed to read fuse\n", ret);
			ret = CMD_RET_FAILURE;
			goto exit;
		} else {
			ret = CMD_RET_SUCCESS;
		}

#ifdef CONFIG_LIST_FUSE_V1
		printf("TME_FUSE_ADDR: 0x%08X\tVALUE: 0x%08X\n",
			fuse->fuse_addr, fuse->val);
#elif CONFIG_LIST_FUSE_V2
		printf("TME_FUSE_ADDR: 0x%08X\tVALUE: 0x%08X%08X\n",
			fuse->fuse_addr, fuse->msb_val, fuse->lsb_val);
#endif
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		ret = CMD_RET_FAILURE;
	}

exit:
	free(fuse);
	return ret;
}

U_BOOT_CMD(dump_fuse, 2, 0, do_dump_fuse,
		"dump given QFPROM register from memory\n",
		"<0xaddress>");
#endif

#ifdef CONFIG_IPQ_QCN9224_FUSING
static int pci_cmd(const char *cmd)
{
	if (strcmp(cmd, "list_qcn9224_fuse") == 0)
		return PCI_LIST_QCN9224_FUSE;
	else if (strcmp(cmd, "fuse_qcn9224") == 0)
		return PCI_FUSE_QCN9224;
	else if (strcmp(cmd, "detect_qcn9224") == 0)
		return PCI_DETECT_QCN9224;
	else
		return PCI_LIST;
}

static int fuse_qcn9224(const struct pci_device_id *ids, int device_id)
{
	struct udevice *dev;
	ulong vendor, device;
	int version, val, ret = 0;
	uintptr_t bar0_base, reg;
	uint32_t load_addr, file_size;

	ret = pci_find_device_id(ids, device_id, &dev);
	if (ret) {
		printf("Device not found\n");
		return CMD_RET_FAILURE;
	}

	load_addr = env_get_ulong("fileaddr", 16, 0);
	file_size = env_get_ulong("filesize", 16, 0);

	if ((file_size == 0) || (load_addr == 0)) {
		printf("Fuse data not found\n");
		return CMD_RET_FAILURE;
	}

	ret = CMD_RET_FAILURE;

	dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0, (uint32_t *)&bar0_base);
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
		printk("Fusing not supported in QCN9224 V1\n");
		return CMD_RET_FAILURE;
	}

	printf("Fusing on Vendor ID:0x%lx device ID:0x%lx devbusfn:0x%x\n",
				vendor, device, dm_pci_get_bdf(dev));
	/*
	 *flush dcache
	 */
	flush_dcache_all();

	watchdog_reset();

	writel(0, bar0_base + BHI_STATUS);
	writel(upper_32_bits(load_addr), bar0_base + BHI_IMGADDR_HIGH);
	writel(lower_32_bits(load_addr), bar0_base + BHI_IMGADDR_LOW);
	writel(file_size, bar0_base + BHI_IMGSIZE);
	writel(1, bar0_base + BHI_IMGTXDB);

	printf("Waiting for fuse blower bin download...\n");

	reg = bar0_base + BHI_STATUS;
	ret = readl_poll_sleep_timeout(reg, val,
			((val & BHI_STATUS_MASK) >> BHI_STATUS_SHIFT) ==
				BHI_STATUS_SUCCESS, 250 * 1000, 12500 * 1000);
	if (ret) {
		printf("Fuse blower bin Download failed, "
				"BHI_STATUS 0x%x, ret %d\n", val, ret);
		print_error_code(bar0_base, true);
		ret = CMD_RET_FAILURE;
		goto fail;
	}

	reg = bar0_base + BHI_EXECENV;
	ret = readl_poll_sleep_timeout(reg, val, (val & NO_MASK ) == 1,
					250 * 1000, 12500 * 1000);
	if (ret) {
		printf("EXECENV is not correct, "
				"BHI_EXECENV 0x%x, ret %d\n",val, ret);
		print_error_code(bar0_base, true);
		ret = CMD_RET_FAILURE;
		goto fail;
	}

	printf("Fuse blower bin loaded sucessfully\n");

	reg = bar0_base + BHI_ERRCODE;
	ret = readl_poll_sleep_timeout(reg, val, (val & NO_MASK) == 0xCAFECACE,
					250 * 1000, 12500 * 1000);
	if (ret) {
		printf("Fusing failed, ret %d\n",ret);
		print_error_code(bar0_base, false);
		ret = CMD_RET_FAILURE;
		goto fail;
	}

	printf("Fusing completed sucessfully\n");
	ret = CMD_RET_SUCCESS;

fail:
	/* Target SoC global reset */
	qcn92xx_global_soc_reset(bar0_base, false);

	mdelay(1000);

	/* Target MHI reset */
	val = readl(bar0_base + MHICTRL);
	writel(val | MHICTRL_RESET_MASK, bar0_base + MHICTRL);
	return ret;
}

static void print_qcn9224_fuse(struct udevice *bus,
				const struct pci_device_id *ids)
{
	struct udevice *dev;
	int val, ret, i = 0;
	uintptr_t bar0_base;

	ret = pci_bus_find_devices(bus, ids, &i, &dev);
	if (ret)
		return;

	dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0, (uint32_t *)&bar0_base);
	bar0_base &= 0xFFF00000;

	printf("Slot id: %d\tPCIe Bus ID: %d\nFuse Name \t\t  Address\t "
		"Value\n", ((dev_seq(bus) - 1) >> 1), dev_seq(bus));
	printf("------------------------------------------------------\n");

	pci_select_window(bar0_base, QCN9224_SECURE_BOOT0_AUTH_EN);

	val = readl(bar0_base + WINDOW_START +
			(QCN9224_SECURE_BOOT0_AUTH_EN & WINDOW_RANGE_MASK));

	printf("SECURE_BOOT0_AUTH_EN\t   0x%x \t 0x%x \n",
		QCN9224_SECURE_BOOT0_AUTH_EN,
			(val & QCN9224_SECURE_BOOT0_AUTH_EN_MASK));

	pci_select_window(bar0_base, QCN9224_OEM_MODEL_ID);

	val = readl(bar0_base + WINDOW_START +
			(QCN9224_OEM_MODEL_ID & WINDOW_RANGE_MASK));

	printf("OEM ID\t\t\t   0x%x \t 0x%lx \n",QCN9224_OEM_MODEL_ID,
			(val & QCN9224_OEM_ID_MASK) >> QCN9224_OEM_ID_SHIFT);
	printf("MODEL ID\t\t   0x%x \t 0x%lx \n",QCN9224_OEM_MODEL_ID,
			(val & QCN9224_MODEL_ID_MASK));

	pci_select_window(bar0_base, QCN9224_ANTI_ROLL_BACK_FEATURE);

	val = readl(bar0_base + WINDOW_START +
			(QCN9224_ANTI_ROLL_BACK_FEATURE & WINDOW_RANGE_MASK));
	printf("ANTI_ROLL_BACK_FEATURE_EN  0x%x \t 0x%lx \n",
			QCN9224_ANTI_ROLL_BACK_FEATURE,
			(val & QCN9224_ANTI_ROLL_BACK_FEATURE_EN_MASK) >>
				QCN9224_ANTI_ROLL_BACK_FEATURE_EN_SHIFT);
	printf("TOTAL_ROT_NUM\t\t   0x%x \t 0x%lx \n",
			QCN9224_ANTI_ROLL_BACK_FEATURE,
			(val & QCN9224_TOTAL_ROT_NUM_MASK) >>
				QCN9224_TOTAL_ROT_NUM_SHIFT);
	printf("ROT_REVOCATION\t\t   0x%x \t 0x%lx \n",
			QCN9224_ANTI_ROLL_BACK_FEATURE,
			(val & QCN9224_ROT_REVOCATION_MASK) >>
				QCN9224_ROT_REVOCATION_SHIFT);
	printf("ROT_ACTIVATION\t\t   0x%x \t 0x%lx \n",
			QCN9224_ANTI_ROLL_BACK_FEATURE,
			(val & QCN9224_ROT_ACTIVATION_MASK) >>
			QCN9224_ROT_ACTIVATION_SHIFT);

	for(i = 0; i <= QCN9224_OEM_PK_HASH_SIZE ; i+=4) {
		pci_select_window(bar0_base, QCN9224_OEM_PK_HASH + i);

		val = readl(bar0_base + WINDOW_START +
				((QCN9224_OEM_PK_HASH + i) &
					WINDOW_RANGE_MASK));

		printf("OEM PK hash \t\t   0x%x \t 0x%x\n",
			QCN9224_OEM_PK_HASH + i, val);
	}

	pci_select_window(bar0_base, QCN9224_JTAG_ID);
	val = readl(bar0_base + WINDOW_START +
			(QCN9224_JTAG_ID & WINDOW_RANGE_MASK));

	for(i = 0; i < ARRAY_SIZE(qcn9224_jtag_ids); i++) {
		if(qcn9224_jtag_ids[i].id == val) {
			printf("JTAG ID\t\t\t   0x%x \t 0x%x(%s)\n",
					QCN9224_JTAG_ID, val,
					qcn9224_jtag_ids[i].name);
			break;
		}
	}

	if(i >= ARRAY_SIZE(qcn9224_jtag_ids))
		printf("JTAG ID\t\t\t   0x%x \t 0x%x\n",
			QCN9224_JTAG_ID, val);

	pci_select_window(bar0_base, QCN9224_SERIAL_NUM);
	val = readl(bar0_base + WINDOW_START +
			(QCN9224_SERIAL_NUM & WINDOW_RANGE_MASK));
	printf("Serial Number\t\t   0x%x \t 0x%x\n",
			QCN9224_SERIAL_NUM, val);

	pci_select_window(bar0_base, QCN9224_PART_TYPE_EXTERNAL);
	val = readl(bar0_base + WINDOW_START +
			(QCN9224_PART_TYPE_EXTERNAL & WINDOW_RANGE_MASK));
	val = (val & QCN9224_PART_TYPE_EXTERNAL_MASK) >>
			QCN9224_PART_TYPE_EXTERNAL_SHIFT;
	printf("Part Type\t\t   0x%x \t 0x%x(%s)\n",
			QCN9224_PART_TYPE_EXTERNAL, val, val?"EXT":"INT");

	printf("------------------------------------------------------\n\n");
}

static void detect_qcn9224(struct udevice *bus,
				const struct pci_device_id *ids)
{
	int ret;
	struct udevice *dev;
	int qcn9224_version, index = 0;
	uintptr_t bar0_base;

	ret = pci_bus_find_devices(bus, ids, &index, &dev);
	if (ret)
		return;

	dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0, (uint32_t *)&bar0_base);
	bar0_base &= 0xFFF00000;

	/* Read QCN9224 version */
	pci_select_window(bar0_base, QCN9224_TCSR_SOC_HW_VERSION);

	qcn9224_version = readl(bar0_base + WINDOW_START +
				(QCN9224_TCSR_SOC_HW_VERSION &
					WINDOW_RANGE_MASK));

	qcn9224_version = (qcn9224_version &
				QCN9224_TCSR_SOC_HW_VERSION_MASK) >>
					QCN9224_TCSR_SOC_HW_VERSION_SHIFT;

	env_set_ulong("qcn9224_version",(unsigned long)qcn9224_version);
}

static void list_pci_device(struct udevice *bus)
{
	struct udevice *dev;
	ulong vendor, device;
	uint32_t bar0_base;

	for (device_find_first_child(bus, &dev);
		dev;
		device_find_next_child(&dev)) {

		dm_pci_read_config(dev, PCI_VENDOR_ID, &vendor, PCI_SIZE_16);
		dm_pci_read_config(dev, PCI_DEVICE_ID, &device, PCI_SIZE_16);
		dm_pci_read_config32(dev->parent, PCI_BASE_ADDRESS_0,
					&bar0_base);

		printf("\t   %d  \t\t    %d    \t\t0x%x        \t0x%lx\n",
				((dev_seq(bus) - 1) >> 1),
				dev_seq(bus),
				bar0_base & 0xFF000000,
				PCI_VENDEV(vendor,device));

		watchdog_reset();
	}
}

static int do_pci_cmd(struct cmd_tbl *cmdtp, int flag, int argc,
                                          char *const argv[])
{
	struct udevice *bus;
	int busnum, cmd, device_id, ret = CMD_RET_SUCCESS;

	/*
	 * Init Pci
	 * disable console to avoid pci init logs
	 */
	gd->have_console = 0;
	pci_init();
	gd->have_console = 1;

	cmd = pci_cmd(argv[0]);

	switch (cmd) {
	case PCI_FUSE_QCN9224:
		if (argc != 2) {
			ret = CMD_RET_USAGE;
			goto fail;
		} else {
			device_id = simple_strtoul(argv[1], NULL, 16);
			if (device_id > CONFIG_IPQ_MAX_PCIE) {
				printf("Supported PCIe instances 0 to %d\n",
					CONFIG_IPQ_MAX_PCIE - 1);
				ret = CMD_RET_USAGE;
				goto fail;
			}
		}
		ret = fuse_qcn9224(device_table, device_id);
		break;
	case PCI_LIST:
		printf("\t Slotid\t\tBus Number\t\tBase Address\t\t"
			"Device ID \n");
	case PCI_LIST_QCN9224_FUSE:
	case PCI_DETECT_QCN9224:
		for (busnum = 0; busnum < CONFIG_IPQ_MAX_PCIE * 2; ++busnum) {
			/*
			 * avoid unwannted error logs, so disabling console
			 */
			gd->have_console = 0;

			if (uclass_get_device_by_seq(UCLASS_PCI,busnum, &bus))
				continue;

			if (!device_is_on_pci_bus(bus))
				continue;

			gd->have_console = 1;

			if (cmd == PCI_LIST)
				list_pci_device(bus);
			else if (cmd == PCI_DETECT_QCN9224)
				detect_qcn9224(bus, device_table);
			else
				print_qcn9224_fuse(bus, device_table);
		}
		break;
	default:
		;
	}
fail:
	gd->have_console = 1;

	return ret;
}

U_BOOT_CMD(list_pci, 1, 1, do_pci_cmd,
	   "Print the RC's PCIe details and attached device ID",
	   "If no attach is present, then nothing will be printed");

U_BOOT_CMD(list_qcn9224_fuse, 1, 1, do_pci_cmd,
	   "Print QCN9224 fuse details from attached PCIe slots",
	   "If there is no QCN9224 attach, then nothing will be printed");

U_BOOT_CMD(detect_qcn9224, 1, 1, do_pci_cmd,
	   "Detect qcn9224 version and populate it on qcn9224_version Env",
	   "qcn9224_version will be zero if not attached else one / two");

U_BOOT_CMD(fuse_qcn9224, 2, 1, do_pci_cmd,
	   "Fuse QCN9224 V2 fuses and argument is PCIe device ID",
	   "If not QCN9224 V2, then fuse blow will be skipped");
#endif /* CONFIG_IPQ_QCN9224_FUSING */

static int run_xpu_config_test(void)
{
	struct resp resp_buf __aligned(CONFIG_SYS_CACHELINE_SIZE);
	uint32_t passed = 0, failed = 0;
	int ret = CMD_RET_FAILURE;
	struct log_buff logbuff;
	struct xpu_tzt xputzt;
	scm_param param;
	int i = 0;

	memset(&xputzt, 0, sizeof(struct xpu_tzt));
	memset(&logbuff, 0, sizeof(struct log_buff));
	memset(&resp_buf, 0, sizeof(struct resp));
	xputzt.test_id = XPU_TEST_ID;
	xputzt.num_param = 0x3;
	xputzt.param3 = (uintptr_t)&resp_buf;

	printf("****** xPU Configuration Validation Test Begin ******\n");

	do {

		do {
			ret = -ENOTSUPP;
			IPQ_SCM_XPU_LOG(param, (uintptr_t)&logbuff,
							PRINT_BUF_LEN);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("\nipq_scm_call: SCM_XPU_LOG_BUFFER"
						" failed, ret : %d\n", ret);
				goto fail;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			goto fail;
		}

		xputzt.param2 = i++;
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_XPU_SEC_TEST_1(param, (uintptr_t)&xputzt,
						sizeof(struct xpu_tzt));
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("\nipq_scm_call: SCM_SEC_TEST_1"
						" failed, ret : %d\n", ret);
				goto fail;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			goto fail;
		}

		invalidate_dcache_range((unsigned long)&resp_buf,
					(unsigned long)&resp_buf +
					CONFIG_SYS_CACHELINE_SIZE);
		if (resp_buf.status == 0)
			passed++;
		else if (resp_buf.status == 1)
			failed++;

		logbuff.buffer[logbuff.log_pos] = '\0';
		printf("%s", logbuff.buffer);

	} while(i < resp_buf.total_tests);

	printf("******************************************************\n");
	printf("Test Result: Passed %u Failed %u (total %u)\n",
	       passed, failed, (uint32_t)resp_buf.total_tests);
	printf("****** xPU Configuration Validation Test End ******\n");

fail:
	return ret;
}

static int
do_tzt(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	uint32_t img_addr;
	uint32_t img_size;
	int ret = CMD_RET_FAILURE;
	scm_param param;

	/* at least two arguments should be there */
	if (argc < 2) {
		ret = CMD_RET_USAGE;
		goto fail;
	}

	if (strncmp(argv[1], "load", sizeof("load")) == 0) {
		if (argc < 4) {
			ret = CMD_RET_USAGE;
			goto fail;
		}

		do {
			ret = -ENOTSUPP;
			IPQ_SCM_TZT_REGION_NOTIFY(param, TZT_LOAD_ADDR,
							TZT_LOAD_SIZE);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("\nipq_scm_call: "
						"SCM_TZT_REGION_NOTIFICATION"
						" failed, ret : %d\n", ret);
				ret = CMD_RET_FAILURE;
				goto fail;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			goto fail;
		}

		watchdog_reset();

		img_addr = simple_strtoul(argv[2], NULL, 16);
		img_size = simple_strtoul(argv[3], NULL, 16);

		do {
			ret = -ENOTSUPP;
			IPQ_SCM_TZT_EXEC_IMG(param, MDT_SIZE,
						img_size - MDT_SIZE, img_addr);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("\nipq_scm_call: SCM_TZT_TESTEXEC_IMG"
						" failed, ret : %d\n", ret);
				ret = CMD_RET_FAILURE;
				goto fail;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			goto fail;
		}

		watchdog_reset();

		tzt_loaded = 1;
		return 0;
	}

	if (!tzt_loaded) {
		printf("load tzt image before running test cases\n");
		ret = CMD_RET_FAILURE;
		goto fail;
	}

	if (strncmp(argv[1], "xpu", sizeof("xpu")) == 0)
		ret = run_xpu_config_test();
fail:
	return ret;
}

U_BOOT_CMD(tzt, 4, 0, do_tzt,
	   "load and run tzt\n",
	   "tzt load address size - To load tzt image\n"
	   "tzt xpu - To run xpu config test\n");

#if defined(CONFIG_DPR_VERSION)
int do_dpr(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{

	return execute_dpr_fun(cmdtp, flag, argc, argv);
}

#ifdef CONFIG_DPR_VER_1_0
U_BOOT_CMD(dpr_execute, 2, 0, do_dpr,
                "Debug Policy Request processing\n",
                "dpr_execute [address] - Processing dpr\n");
#endif /* CONFIG_DPR_VER_1_0 */

#if  defined(CONFIG_DPR_VER_2_0) || defined(CONFIG_DPR_VER_3_0)
U_BOOT_CMD(dpr_execute, 3, 0, do_dpr,
                "Debug Policy Request processing\n",
                "dpr_execute [fileaddr] [filesize] - Processing dpr\n");
#endif /* CONFIG_DPR_VER_2_0 or CONFIG_DPR_VER_3_0*/
#endif /* CONFIG_DPR_VER_1_0 or CONFIG_DPR_VER_2_0 or CONFIG_DPR_VER_3_0 */

static void uart_read_data(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int val = 0;

	if (ops == NULL)
		return;

	for(;;) {
		do {
			val = ops->getc(dev);
			if (val == -EAGAIN)
				schedule();
		} while (val == -EAGAIN);

		if (val == 0x03)
			break;
		else
			serial_putc(val);
	}
}

static void uart_write_data(struct udevice *dev, const char *str)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int ret = 0;

	if(ops == NULL)
		return;

	while (*str != '\0') {
		do {
			ret = ops->putc(dev, *str);
		} while (ret == -EAGAIN);

		++str;
	}
}

static int do_uart(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	int ret = CMD_RET_USAGE;
	char node_name[8] = {0};
	int node, id = CONFIG_SECONDARY_UART_INDEX;

	if (argc < 2)
		return CMD_RET_USAGE;

	if ((strncmp(argv[1], "start", 5) == 0) && (argc == 2)) {
		printf("starting secondary UART %d...", id);
		snprintf(node_name, sizeof(node_name), "uart%d", id);
		node = fdt_path_offset(gd->fdt_blob, node_name);
		/*
		 * allow probe after reloc
		 */
		gd->flags &= ~GD_FLG_RELOC;
		uclass_get_device_by_of_offset(UCLASS_SERIAL, node, &dev);
		gd->flags |= GD_FLG_RELOC;
		if (dev == NULL) {
			printf(" No device found \n");
			ret = CMD_RET_FAILURE;
		} else {
			printf(" Success \n");
			ret = CMD_RET_SUCCESS;
		}
	}

	if ((strcmp(argv[1], "read") == 0) && (argc == 2)) {
		if (dev != NULL) {
			uart_read_data(dev);
			ret = CMD_RET_SUCCESS;
		} else {
			printf(" No device found... do start!!!\n");
			ret = CMD_RET_FAILURE;
		}
	}

	if ((strcmp(argv[1], "write") == 0) && (argc == 3)) {
		if (dev != NULL) {
			uart_write_data(dev, argv[2]);
			ret = CMD_RET_SUCCESS;
		} else {
			printf(" No device found... do start!!!\n");
			ret = CMD_RET_FAILURE;
		}
	}

	return ret;
}

U_BOOT_CMD(
	uart,	3,	0,	do_uart,
	"UART sub-system cli",
	"start - initialize secondary uart\n"
	"uart read - read strings from second UART\n"
	"uart write - write strings to second UART\n"
);

#if defined (CONFIG_IPQ_SMP_CMD_SUPPORT) || (CONFIG_IPQ_SMP64_CMD_SUPPORT)
asmlinkage void secondary_core_entry(char *argv, int *cmd_complete,
					int *cmd_result)
{
	*cmd_result = cli_simple_run_command(argv, 0);
	*cmd_complete = 1;

	bring_secondary_core_down(CPU_POWER_DOWN);
}

static void console_silent_enable(void)
{
	gd->flags |= GD_FLG_SILENT | GD_FLG_DISABLE_CONSOLE;
}

static void console_silent_disable(void)
{
	gd->flags &= ~(GD_FLG_SILENT | GD_FLG_DISABLE_CONSOLE);
}

int do_runmulticore(struct cmd_tbl *cmdtp,
			   int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_SUCCESS;
	int i, j, delay = 0, core_status = 0, core_on_status = 0;
	uint8_t *ptr = NULL;
#if defined(CONFIG_WDT)
	struct udevice *dev;
	u32 timeout = 0;
#endif

	if ((argc <= 1) || (argc > 4)) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

#if defined(CONFIG_WDT)
	if (uclass_find_device_by_seq(UCLASS_WDT, 0, &dev) == 0)
		wdt_stop(dev);
#endif

	for (i = 1; i < argc; i++) {
		if (!strncmp("runmulticore", argv[i],
			sizeof("runmulticore") - 1)) {
			printf("Restricted command 'runmulticore' for "
				"secondary core\n");
			ret = CMD_RET_USAGE;
			goto exit;
		}
	}

	dcache_disable();

	/* Setting up stack for secondary cores */
	memset(core, 0, sizeof(core));

	global_core_array = core;

	for (i = 1; i < argc; i++) {
		ptr = malloc_cache_aligned(SECONDARY_CORE_STACKSZ);
		if (!ptr) {
			j = i - 1;
			while (j >= 0) {
				if (core[i - 1].stack_ptr != NULL) {
					free(core[i - 1].stack_ptr);
					core[i - 1].stack_ptr = NULL;
				}
				j--;
			}
			printf("Memory allocation failure\n");
			ret = CMD_RET_FAILURE;
			goto exit;
		}
		/* 0xf0 is the padding length */
		core[i - 1].stack_top_ptr = ptr;
		core[i - 1].stack_ptr = (ptr + (SECONDARY_CORE_STACKSZ) - 0xf0);

		core[i - 1].cpu_up = 0;
		core[i - 1].cmd_complete = 0;
		core[i - 1].cmd_result = -1;
		core[i - 1].gd_ptr = gd;
		core[i - 1].arg_ptr = argv[i];
	}


	/* Bringing up the secondary cores */
	for (i = 1; i < argc; i++) {
		printf("Scheduling Core %d\n", i);
		delay = 0;
		console_silent_enable();
		ret = bring_secondary_core_up(i,
				(unsigned long)secondary_cpu_init,
				(uintptr_t)&core[i - 1]);
		if (ret) {
			panic("Some problem to getting core %d up\n", i);
		}

		while ((delay < 5) && (!(core[i - 1].cpu_up))) {
			mdelay(1000);
			delay++;
		}
		if (!(core[i - 1].cpu_up)) {
			panic("Can't bringup core %d\n",i);
		}
		console_silent_disable();

		core_status |= (BIT(i - 1));
		core_on_status |= (BIT(i - 1));
	}

	/* Waiting for secondary cores to complete the task */
	while (core_status) {
		for (i = 1; i < argc; i++) {
			if ((core_status & (BIT(i - 1))) &&
					(core[i - 1].cmd_complete)) {
				printf("Command on core %d is %s\n", i,
					core[i - 1].cmd_complete ?
					((core[i - 1].cmd_result == -1) ?
					"FAIL" : "PASS"):
					"INCOMPLETE");
				core_status &= (~BIT((i - 1)));
			}
		}
		if (ctrlc()) {
			reset();
		}
	}

	/* Waiting for cores to powerdown */
	delay = 0;
	while (core_on_status) {
		for (i = 1; i < argc; i++) {
			if (core_on_status & (BIT(i - 1))) {
				if (is_secondary_core_off(i) == 1) {
					printf("core %d powered off\n", i);
					core_on_status &= (~BIT((i - 1)));
				}
			}
		}
		mdelay(1000);
		delay++;
		if (delay > 5)
			panic("Some cores can't be powered off\n");
	}

	/* Free up all the stack */
	for (i = 1; i < argc; i++) {
		free(core[i - 1].stack_top_ptr);
	}

	printf("Status:\n");
	for (i = 1; i < argc; i++) {
		printf("Core %d: %s\n", i,
				core[i - 1].cmd_complete ?
				((core[i - 1].cmd_result == -1) ?
				 "FAIL" : "PASS"): "INCOMPLETE");
	}

exit:
	invalidate_dcache_all();
	dcache_enable();
#if defined(CONFIG_WDT)
	if (uclass_find_device_by_seq(UCLASS_WDT, 0, &dev) == 0) {
		timeout = dev_read_u32_default(dev, "timeout-sec", timeout);
		if (timeout) {
			ret = wdt_start(dev, timeout * 1000, 0);
			if (ret != 0)
				printf("WDT: Failed to start %s\n",
					dev->name);
		}
	}
#endif
	return ret;
}

U_BOOT_CMD(runmulticore, 4, 0, do_runmulticore,
	   "Enable and schedule secondary cores",
	   "runmulticore <\"command to core1\"> [core2 core3 ...]");
#endif /* CONFIG_IPQ_SMP_CMD_SUPPORT || CONFIG_IPQ_SMP64_CMD_SUPPORT */

#ifdef CONFIG_CMD_AES_256
enum tz_crypto_service_aes_type_t {
	TZ_CRYPTO_SERVICE_AES_SHK = 0x1,
	TZ_CRYPTO_SERVICE_AES_PHK = 0x2,
	TZ_CRYPTO_SERVICE_AES_TYPE_MAX,

};

enum tz_crypto_service_aes_mode_t {
	TZ_CRYPTO_SERVICE_AES_ECB = 0x0,
	TZ_CRYPTO_SERVICE_AES_CBC = 0x1,
	TZ_CRYPTO_SERVICE_AES_MODE_MAX,
};

#ifndef CONFIG_AES_256_DERIVE_KEY
struct crypto_aes_req_data_t {
	uint64_t type;
	uint64_t mode;
	uint64_t req_buf;
	uint64_t req_len;
	uint64_t ivdata;
	uint64_t iv_len;
	uint64_t resp_buf;
	uint64_t resp_len;
};
#else
#define MAX_CONTEXT_BUFFER_LEN_V1	64
#define MAX_CONTEXT_BUFFER_LEN_V2	128
#define DEFAULT_POLICY_DESTINATION	0
#define DEFAULT_KEY_TYPE		2
struct crypto_aes_operation_policy {
	uint32_t operations;
	uint32_t algorithm;
};

struct crypto_aes_hwkey_policy  {
	struct crypto_aes_operation_policy op_policy;
	uint32_t kdf_depth;
	uint32_t permissions;
	uint32_t key_type;
	uint32_t destination;
};

struct crypto_aes_hwkey_bindings_v1 {
	uint32_t bindings;
	uint32_t context_len;
	uint8_t context[MAX_CONTEXT_BUFFER_LEN_V1];
};

struct crypto_aes_derive_key_cmd_t_v1 {
	struct crypto_aes_hwkey_policy policy;
	struct crypto_aes_hwkey_bindings_v1 hw_key_bindings;
	uint32_t source;
	uint64_t mixing_key;
	uint64_t key;
};

struct crypto_aes_hwkey_bindings_v2 {
	uint32_t bindings;
	uint32_t context_len;
	uint8_t context[MAX_CONTEXT_BUFFER_LEN_V2];
};

struct crypto_aes_derive_key_cmd_t_v2 {
	struct crypto_aes_hwkey_policy policy;
	struct crypto_aes_hwkey_bindings_v2 hw_key_bindings;
	uint32_t source;
	uint64_t mixing_key;
	uint64_t key;
};

struct crypto_aes_req_data_t {
	uint64_t key_handle;
	uint64_t type;
	uint64_t mode;
	uint64_t req_buf;
	uint64_t req_len;
	uint64_t ivdata;
	uint64_t iv_len;
	uint64_t resp_buf;
	uint64_t resp_len;
};

/**
 * do_derive_aes_256_key() - Handle the "derive_key" command-line command
 * @cmdtp:	Command data struct pointer
 * @flag:	Command flag
 * @argc:	Command-line argument count
 * @argv:	Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 */
static int do_derive_aes_256_key(struct cmd_tbl *cmdtp, int flag,
				 int argc, char *const argv[])
{
	struct crypto_aes_derive_key_cmd_t_v1 *req_ptr = NULL;
	int ret = CMD_RET_USAGE;
	uintptr_t *key_handle = NULL;
	uint8_t *context_buf = NULL;
	int context_len = 0;
	int i = 0, j = 0;
	scm_param param;

	if (argc != 5)
		return ret;
	context_buf = (uint8_t *)simple_strtoul(argv[3], NULL, 16);;
	context_len = simple_strtoul(argv[4], NULL, 16);
	if (context_len > MAX_CONTEXT_BUFFER_LEN_V1) {
		printf("Error: context length should be less than %d\n",
			MAX_CONTEXT_BUFFER_LEN_V1);
		return ret;
	}
	req_ptr = (struct crypto_aes_derive_key_cmd_t_v1 *)memalign(
				ARCH_DMA_MINALIGN,
				sizeof(struct crypto_aes_derive_key_cmd_t_v1));
	if (!req_ptr) {
		printf("Error allocating memory for key handle request buf");
		return -ENOMEM;
	}

	req_ptr->policy.key_type = DEFAULT_KEY_TYPE;
	req_ptr->policy.destination = DEFAULT_POLICY_DESTINATION;
	req_ptr->source = simple_strtoul(argv[1], NULL, 16);
	req_ptr->hw_key_bindings.bindings = simple_strtoul(argv[2], NULL, 16);
	key_handle = (uintptr_t *)memalign(ARCH_DMA_MINALIGN,
					sizeof(uint64_t));
	if (!key_handle) {
		printf("Error allocating memory for key handle");
		ret = -ENOMEM;
		goto exit;
	}

	req_ptr->key = (uintptr_t) key_handle;
	req_ptr->mixing_key = 0;
	req_ptr->hw_key_bindings.context_len = context_len;
	while (i < context_len) {
		req_ptr->hw_key_bindings.context[j++] = context_buf[i++];
	}

	watchdog_reset();

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_GENERATE_AES_256_KEY(param, (uintptr_t)req_ptr,
				sizeof(struct crypto_aes_derive_key_cmd_t_v1));
		invalidate_dcache_all();
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("\nipq_scm_call: SCM_AES_256_GEN_KEY"
					" failed, ret : %d\n", ret);
			ret = CMD_RET_FAILURE;
		} else
			printf("Key handle is %u\n", (unsigned int)*key_handle);
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:
	if (key_handle)
		free(key_handle);
	if (req_ptr)
		free(req_ptr);

	return ret;
}

/***************************************************/
U_BOOT_CMD(
	derive_aes_256_key, 5, 1, do_derive_aes_256_key,
	"Derive AES 256 key before encrypt/decrypt in TME-L based systems",
	"Key Derivation: derive_aes_256_key <source_data> <bindings_data>"
	"<context_data address> <context data len>"
);

/**
 * do_derive_aes_256_max_ctxt_key() - Handle the "derive_key" command-line
 *                                    command for 128 byte context
 * @cmdtp:	Command data struct pointer
 * @flag:	Command flag
 * @argc:	Command-line argument count
 * @argv:	Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 */
static int do_derive_aes_256_max_ctxt_key(struct cmd_tbl *cmdtp, int flag,
				 int argc, char *const argv[])
{
	struct crypto_aes_derive_key_cmd_t_v2 *req_ptr = NULL;
	int ret = CMD_RET_USAGE;
	uintptr_t *key_handle = NULL;
	uint8_t *context_buf = NULL;
	int context_len = 0;
	int i = 0, j = 0;
	scm_param param;

	if (argc != 5)
		return ret;
	context_buf = (uint8_t *)simple_strtoul(argv[3], NULL, 16);;
	context_len = simple_strtoul(argv[4], NULL, 16);
	if (context_len > MAX_CONTEXT_BUFFER_LEN_V2) {
		printf("Error: context length should be less than %d\n",
			MAX_CONTEXT_BUFFER_LEN_V2);
		return ret;
	}
	req_ptr = (struct crypto_aes_derive_key_cmd_t_v2 *)memalign(
				ARCH_DMA_MINALIGN,
				sizeof(struct crypto_aes_derive_key_cmd_t_v2));
	if (!req_ptr) {
		printf("Error allocating memory for key handle request buf");
		return -ENOMEM;
	}

	req_ptr->policy.key_type = DEFAULT_KEY_TYPE;
	req_ptr->policy.destination = DEFAULT_POLICY_DESTINATION;
	req_ptr->source = simple_strtoul(argv[1], NULL, 16);
	req_ptr->hw_key_bindings.bindings = simple_strtoul(argv[2], NULL, 16);
	key_handle = (uintptr_t *)memalign(ARCH_DMA_MINALIGN,
					sizeof(uint64_t));
	if (!key_handle) {
		printf("Error allocating memory for key handle");
		ret = -ENOMEM;
		goto exit;
	}

	req_ptr->key = (uintptr_t) key_handle;
	req_ptr->mixing_key = 0;
	req_ptr->hw_key_bindings.context_len = context_len;
	while (i < context_len) {
		req_ptr->hw_key_bindings.context[j++] = context_buf[i++];
	}

	watchdog_reset();

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX(param,
				(uintptr_t)req_ptr,
				sizeof(struct crypto_aes_derive_key_cmd_t_v2));
		invalidate_dcache_all();
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("\nipq_scm_call: SCM_AES_256_MAX_CTXT_GEN_KEY"
					" failed, ret : %d\n", ret);
			ret = CMD_RET_FAILURE;
		} else
			printf("Key handle is %u\n",
					(unsigned int)*key_handle);
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:
	if (key_handle)
		free(key_handle);
	if (req_ptr)
		free(req_ptr);

	return ret;
}

/***************************************************/
U_BOOT_CMD(
	derive_aes_256_max_ctxt_key, 5, 1, do_derive_aes_256_max_ctxt_key,
	"Derive AES 256 key with 128 byte context before"
	"encrypt/decrypt in TME-L based systems",
	"Key Derivation: derive_aes_256_max_ctxt_key <source_data>"
	"<bindings_data> <context_data address> <context data len>"
);
#endif /* CONFIG_AES_256_DERIVE_KEY */

/**
 * do_aes_256() - Handle the "aes" command-line command
 * @cmdtp:	Command data struct pointer
 * @flag:	Command flag
 * @argc:	Command-line argument count
 * @argv:	Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 */
static int
do_aes_256(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	uint64_t src_addr, dst_addr, ivdata;
	uint64_t req_len, iv_len, resp_len, type, mode;
	struct crypto_aes_req_data_t *req_ptr = NULL;
	scm_param param = {0};
	int ret = CMD_RET_USAGE;

#ifndef CONFIG_AES_256_DERIVE_KEY
	if (argc != 10)
		return ret;
#else
	if (argc != 11)
		return ret;
#endif /* CONFIG_AES_256_DERIVE_KEY */

	if (strncmp(argv[1], "enc", 3) && strncmp(argv[1], "dec", 3))
		return ret;

	type = simple_strtoul(argv[2], NULL, 16);
	if (type >= TZ_CRYPTO_SERVICE_AES_TYPE_MAX) {
		printf("unkown type specified, use 0x1 - SHK, 0x2 - PHK\n");
		return ret;
	}

	mode = simple_strtoul(argv[3], NULL, 16);
	if (mode >= TZ_CRYPTO_SERVICE_AES_MODE_MAX) {
		printf("unkown mode specified, use 0x0 - ECB, 0x1 - CBC\n");
		return ret;
	}

	src_addr = simple_strtoull(argv[4], NULL, 16);
	req_len = simple_strtoul(argv[5], NULL, 16);
	if (req_len <= 0 || (req_len % 16) != 0) {
		printf("Invalid request buffer length, length "
			"should be multiple of AES block size (16)\n");
		return ret;
	}

	ivdata = simple_strtoull(argv[6], NULL, 16);
	iv_len = simple_strtoul(argv[7], NULL, 16);
	if (iv_len != 16) {
		printf("Error: iv length should be equal to AES block "
							"size (16)\n");
		return ret;
	}

	dst_addr = simple_strtoull(argv[8], NULL, 16);
	resp_len =  simple_strtoul(argv[9], NULL, 16);
	if (resp_len < req_len) {
		printf("Error: response buffer cannot be less then "
							"request buffer\n");
		return ret;
	}

	req_ptr = (struct crypto_aes_req_data_t *)memalign(ARCH_DMA_MINALIGN,
					sizeof(struct crypto_aes_req_data_t));
	if (!req_ptr) {
		printf("Error allocating memory");
		return -ENOMEM;
	}

#ifdef CONFIG_AES_256_DERIVE_KEY
	req_ptr->key_handle = simple_strtoul(argv[10], NULL, 16);
#endif /* CONFIG_AES_256_DERIVE_KEY */
	req_ptr->type = type;
	req_ptr->mode = mode;
	req_ptr->req_buf = (uint64_t)src_addr;
	req_ptr->req_len = req_len;
	req_ptr->ivdata = (mode == TZ_CRYPTO_SERVICE_AES_CBC) ?
							(uint64_t)ivdata : 0;
	req_ptr->iv_len = iv_len;
	req_ptr->resp_buf = (uint64_t)dst_addr;
	req_ptr->resp_len = resp_len;

	watchdog_reset();

	do {
		ret = -ENOTSUPP;
		if (!strncmp(argv[1], "enc", 3))
			IPQ_SCM_ENCRYPT_AES_256(param, (uintptr_t)req_ptr,
					sizeof(struct crypto_aes_req_data_t));
		else if (!strncmp(argv[1], "dec", 3))
			IPQ_SCM_DECRYPT_AES_256(param, (uintptr_t)req_ptr,
					sizeof(struct crypto_aes_req_data_t));

		invalidate_dcache_all();
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("\nipq_scm_call: %s failed, ret : %d\n", \
				(param.type == SCM_AES_256_ENC)? \
				"SCM_AES_256_ENC" : "SCM_AES_256_DEC", ret);
			ret = CMD_RET_FAILURE;
		} else
			printf("Encryption/Decryption successful\n");
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return CMD_RET_FAILURE;
	}

	if (req_ptr) {
		free(req_ptr);
		req_ptr = NULL;
	}
	return ret;
}

/***************************************************/
U_BOOT_CMD(
	aes_256, 11, 1, do_aes_256,
	"\nAES 256 CBC/ECB Encryption / Decryption",
	"\nEncryption: aes_256 enc <type> <mode> <plain data address> "
	"<plain data length> <iv data address> <iv length> "
	"<response buf address> <response buffer length>"
#ifdef CONFIG_AES_256_DERIVE_KEY
	" <key_handle>"
#endif /* CONFIG_AES_256_DERIVE_KEY */
	"\nDecryption: aes_256 dec <type> <mode> <encrypted buffer address> "
	"<encrypted buffer length> <iv data address> <iv length> "
	"<response buffer address> <response buffer length>"
#ifdef CONFIG_AES_256_DERIVE_KEY
	" <key_handle>"
#endif /* CONFIG_AES_256_DERIVE_KEY */
);

/**
 * do_clear_aes_key() - Handle the "clear_key" command-line command
 *
 * @cmdtp:      Command data struct pointer
 * @flag:       Command flag
 * @argc:       Command-line argument count
 * @argv:       Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 */

static int do_clear_aes_key(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret;
	uint32_t key_handle;
	scm_param param = {0};

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	key_handle = simple_strtoul(argv[1], NULL, 10);

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_CLEAR_AES_KEY(param, key_handle);
		ret = ipq_scm_call(&param);
		param.get_ret = true;

		if(!ret && !le32_to_cpu(param.res.result[0]))
			printf("AES key = %u cleared successfully\n",
					key_handle);
		else
			printf("AES key clear failed with err %d\n",ret);


	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return CMD_RET_FAILURE;
	}

	return ret ? CMD_RET_FAILURE:CMD_RET_SUCCESS;
}

/***************************************************/
U_BOOT_CMD(
        clear_aes_key, 2, 0, do_clear_aes_key,
	"Clear AES 256 key in TME-L based systems",
	"Clear key: clear_aes_key <key_handle>"
);

#endif /* CONFIG_CMD_AES_256 */

#ifdef CONFIG_QSPI_LAYOUT_SWITCH
static int do_qpic_switch_layout(struct cmd_tbl *cmdtp, int flag,
			   int argc, char * const argv[])
{
	int ret;
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	char *env_layout = NULL;
#ifdef CONFIG_CMD_UBI
	struct ubi_device *ubi = ubi_get_device(0);
#endif
	if(!mtd) {
		printf("%s: mtd device not available\n", __func__);
		return -ENOMEM;
	}

	if (argc != 2 || (mtd->writesize == 2048 &&
				!strcmp(argv[1], "sbl")))
		return CMD_RET_USAGE;

	env_layout = env_get("nand_layout");
	if(env_layout) {
		if(!strcmp(argv[1], env_layout)) {
			printf("Already in %s layout\n", env_layout);
			return CMD_RET_SUCCESS;
		}
	}

	if (!strcmp(argv[1], "sbl") || !strcmp(argv[1], "linux")) {
		env_set("nand_layout", argv[1]);
	} else {
		return CMD_RET_USAGE;
	}

#ifdef CONFIG_CMD_UBI
	if (ubi) {
		if (ubi->ref_count)
			ubi_put_device(ubi);
		ret = run_command("ubi detach", 0);
		if(ret)
			printf("Failed to detach ubi!!!\n");
	}
#endif
	watchdog_reset();

	ret = device_remove(mtd->dev, DM_REMOVE_NORMAL);
	if (ret)
		return CMD_RET_FAILURE;

	nand_curr_device = -1;
	board_nand_init();

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(qpic_nand, 2, 1, do_qpic_switch_layout,
	   "Switch between SBL and Linux kernel page on 4K NAND Flash.",
	   "qpic_nand (sbl | linux)");
#endif

#if defined(CONFIG_IPQ_MMC) && defined(CONFIG_SUPPORT_EMMC_BOOT)
static int do_switch_to_boot(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret = CMD_RET_FAILURE;
	char runcmd[32] = {0};
	int boot_sel = 0;
	struct mmc *mmc = find_mmc_device(0);

	if (!mmc) {
		printf("no mmc device at slot 0\n");
		return ret;
	}

	if (mmc->part_config == MMCPART_NOAVAILABLE) {
		printf("No part_config info for ver. 0x%x\n", mmc->version);
		return ret;
	}

	switch (argc) {
	case 1:
		boot_sel = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
		if (!((boot_sel == 1) || (boot_sel == 2))) {
			printf("BOOT0 / BOOT1 is not selected ...\n");
			return ret;
		}
		break;
	case 2:
		boot_sel = simple_strtoul(argv[1], NULL, 10) + 1;
		if (!((boot_sel == 1) || (boot_sel == 2)))
			return CMD_RET_USAGE;
		break;
	};

	snprintf(runcmd, sizeof(runcmd), "mmc dev 0 %d", boot_sel);
	ret = run_command(runcmd, 0);
	if (ret) {
		printf("switching to boot%d partition layout"
		" failed, ret %d\n", boot_sel, ret);
		goto exit;
	}

	snprintf(runcmd, sizeof(runcmd), "mmc partconf 0 0 %d %d",
		 boot_sel, boot_sel);
	ret = run_command(runcmd, 0);
	if (ret) {
		printf("set boot%d select failed, ret %d\n", boot_sel, ret);

#ifndef CONFIG_BLK
		snprintf(runcmd, sizeof(runcmd), "mmc dev 0 %d",
				mmc->block_dev.hwpart);
#else
		snprintf(runcmd, sizeof(runcmd), "mmc dev 0 %d",
				mmc_get_blk_desc(mmc)->hwpart);
#endif
		ret = run_command(runcmd, 0);
		if (ret) {
			printf("switching back to the existing partition layout"
			" failed, ret %d\n", ret);
			goto exit;
		}
	} else
		printf("Switched to boot%d partition layout successfully ...\n",
			boot_sel - 1);
exit:
	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(switch_to_boot, 2, 0, do_switch_to_boot,
	   "switch to the boot partition layout\n",
	   "- switch to the current boot partition layout\n"
	   "switch_boot 0 - switch to the boot0 layout\n"
	   "switch_boot 1 - switch to the boot1 layout\n");

static int do_switch_to_user(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret = CMD_RET_FAILURE;
	char runcmd[32] = {0};
	struct mmc *mmc = find_mmc_device(0);

	if (!mmc) {
		printf("no mmc device at slot 0\n");
		return ret;
	}

	if (mmc->part_config == MMCPART_NOAVAILABLE) {
		printf("No part_config info for ver. 0x%x\n", mmc->version);
		return ret;
	}

	snprintf(runcmd, sizeof(runcmd), "mmc dev 0 0");
	ret = run_command(runcmd, 0);
	if (!ret)
		printf("Switched to user partition layout successfully ...\n");

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(switch_to_user, 1, 0, do_switch_to_user,
	   "switch to the user partition layout\n",
	   "- switch to the user partition layout\n");
#endif

static int do_canary(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	char buffer[10] = {0};

	if (argc < 2)
		return CMD_RET_USAGE;

	strlcpy(buffer, argv[1], strlen(argv[1]));

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(canary, 2, 0, do_canary, "Test stack protection\n",
		"- canary <strings>\n");

static int do_clear_tcsr(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	uint32_t val = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	val = simple_strtoull(argv[1], NULL, 16);

	write_tcsr_boot_misc_reg(val, 0);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(clear_tcsr, 2, 0, do_clear_tcsr, "Clear specific bit in TCSR\n",
		"- clear_tcsr <val>\n");
