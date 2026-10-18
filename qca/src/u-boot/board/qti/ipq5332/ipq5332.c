// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <fdt_support.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <jffs2/load_kernel.h>
#include <mtd_node.h>
#include <sysreset.h>
#include <linux/psci.h>
#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif

#include "../common/ipq_board.h"

#include <asm/io.h>
#include <linux/delay.h>

#define PLL_POWER_ON_AND_RESET			0x9B780
#define PLL_REFERENCE_CLOCK			0x9B784
#define FREQUENCY_MASK				0xfffffdf0
#define INTERNAL_48MHZ_CLOCK			0x7
#define CONFIG_NAME_MAX_LEN			128

DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq5332-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{ MACH_TYPE_IPQ5332_RDP468, "ipq5332-rdp468"},
	{ MACH_TYPE_IPQ5332_RDP441, "ipq5332-rdp441"},
	{ MACH_TYPE_IPQ5332_RDP441_QCA81XX, "ipq5332-rdp441-qca81xx"},
	{ MACH_TYPE_IPQ5332_RDP441_QCA81XX_I2C, "ipq5332-rdp441-qca81xx-i2c"},
	{ MACH_TYPE_IPQ5332_RDP442, "ipq5332-rdp442"},
	{ MACH_TYPE_IPQ5332_RDP446, "ipq5332-rdp446"},
	{ MACH_TYPE_IPQ5332_RDP474, "ipq5332-rdp474"},
	{ MACH_TYPE_IPQ5332_RDP473, "ipq5332-rdp480"},
	{ MACH_TYPE_IPQ5332_RDP472, "ipq5332-rdp472"},
	{ MACH_TYPE_IPQ5332_RDP477, "ipq5332-rdp477"},
	{ MACH_TYPE_IPQ5332_RDP478, "ipq5332-rdp478"},
	{ MACH_TYPE_IPQ5332_RDP479, "ipq5332-rdp479"},
	{ MACH_TYPE_IPQ5332_RDP480, "ipq5332-rdp480"},
	{ MACH_TYPE_IPQ5332_RDP481, "ipq5332-rdp481"},
	{ MACH_TYPE_IPQ5332_RDP483, "ipq5332-rdp483"},
	{ MACH_TYPE_IPQ5332_RDP484, "ipq5332-rdp484"},
	{ MACH_TYPE_IPQ5332_RDP486, "ipq5332-rdp442"},
	{ MACH_TYPE_IPQ5332_RDP477_256M, "ipq5332-rdp477-256m"},
	{ MACH_TYPE_IPQ5332_RDP478_256M, "ipq5332-rdp478-256m"},
	{ MACH_TYPE_IPQ5332_DB_MI01_1, "ipq5332-db-mi01.1"},
	{ MACH_TYPE_IPQ5332_DB_MI02_1, "ipq5332-db-mi02.1"},
	{ MACH_TYPE_IPQ5332_DB_MI03_1, "ipq5332-db-mi03.1"},
	{ MACH_TYPE_IPQ5332_TB_MI03_1, "ipq5332-tb-mi03.1"},
	{ MACH_TYPE_IPQ5332_TB_MI05_1, "ipq5332-tb-mi05.1"},
};

int machid_dts_nos = ARRAY_SIZE(machid_dts);

struct machid_dts_map * machid_dts_info = machid_dts;
int * machid_dts_entries = &machid_dts_nos;
#endif /* CONFIG_DTB_RESELECT */

static crashdump_infos_t dumpinfo_n[] = {
	{
		.name = "EBICS.BIN",
		.start_addr = CFG_SYS_SDRAM_BASE,
		.size = 0xBAD0FF5E,
		.dump_level = FULLDUMP,
		.split_bin_sz = SZ_1G,
		.is_aligned_access = false,
		.compression_support = true,
		.dumptoflash_support = false
	},
	{
		.name = "IMEM.BIN",
		.start_addr = 0x08600000,
		.size = 0x00001000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "IMEM2.BIN",
		.start_addr = 0x860F000,
		.size = 0x00001000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false,
		.check_dump_support = true
	},
	{
		.name = "CPU_INFO.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = true
	},
	{
		.name = "UNAME.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = true
	},
	{
		.name = "DMESG.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "PT.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "WLAN_MOD.BIN",
		.start_addr = 0x0,
		.size = 0xBAD0FF5E,
		.dump_level = MINIDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
};

static uint8_t dump_entries_n = ARRAY_SIZE(dumpinfo_n);

crashdump_infos_t *board_dumpinfo = dumpinfo_n;
uint8_t *board_dump_entries = &dump_entries_n;

void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V1);
#endif
	psci_sys_reset(SYSRESET_COLD);
	return;
}

int print_cpuinfo(void)
{
        return 0;
}

void board_cache_init(void)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	icache_enable();
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
	/* Disable L2 as TCM in recovery mode */
	if (!sfi->flash_type)
		writel(0x08000000, 0xB110010);

	dcache_enable();
#endif
}

void lowlevel_init(void)
{
	return;
}

void ipq_fdt_rootfs_auth_fixup(void *blob)
{
	int auth_nodeoff = -EINVAL;

#ifdef LINUX_6_x_ROOTFS_AUTH_DTS_NODE
	auth_nodeoff = fdt_path_offset(blob, LINUX_6_x_ROOTFS_AUTH_DTS_NODE);

	if (auth_nodeoff > 0) {
		parse_fdt_fixup(LINUX_6_x_ROOTFS_AUTH_FIXUP, blob);
	}
#endif

	return;
}

void ipq_fdt_fixup_board(void *blob)
{
	int secure_boot = image_auth_check();

	if(secure_boot && check_rootfs_authentication())
		ipq_fdt_rootfs_auth_fixup(blob);

	return;
}

int board_get_smem_target_info(ipq_smem_target_info_t *smem_tinfo_ptr)
{
	uint32_t tcsr_wonce0_val;
	uint32_t tcsr_wonce1_val;
	uint64_t ipq_smem_target_info_addr;
#ifdef CONFIG_SCM
	int feat_avail = 0;
	scm_param param;
	int ret;

	if (!g_recovery_path)
	{
		/* The TCSR WONCE register is protected in latest TZ.
		 * Old TZ will allow direct read.
		 * Use the CHECK_FEATURE call to know if TZ supports
		 * direct or scm read. Based on return value, read the
		 * TCSR WONCE register appropriately.
		 */
		do {
			ret = -ENOTSUPP;
			CHECK_FEATURE(param, 0x6);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("Feature check scm failed\n");
				return -EFAULT;
			}
			feat_avail = le32_to_cpu(param.res.result[0]);
		} while(0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return -EFAULT;
		}
	}

	if (feat_avail == 0x401000)
	{
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_READ(param, (uintptr_t)TCSR_TZ_WONCE0);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("TCSR WONCE0 read failed\n");
				return -EFAULT;
			}
			tcsr_wonce0_val = le32_to_cpu(param.res.result[0]);
		} while(0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return -EFAULT;
		}

		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_READ(param, (uintptr_t)TCSR_TZ_WONCE1);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("TCSR WONCE1 read failed\n");
				return -EFAULT;
			}
			tcsr_wonce1_val = le32_to_cpu(param.res.result[0]);
		} while(0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return -EFAULT;
		}
	}
	else
	{
		tcsr_wonce0_val = readl(TCSR_TZ_WONCE0);
		tcsr_wonce1_val = readl(TCSR_TZ_WONCE1);
	}
#else
	tcsr_wonce0_val = readl(TCSR_TZ_WONCE0);
	tcsr_wonce1_val = readl(TCSR_TZ_WONCE1);
#endif

	ipq_smem_target_info_t *ipq_smem_target_info_ptr;

	ipq_smem_target_info_addr = tcsr_wonce0_val |
		(((uint64_t)(tcsr_wonce1_val)) << 32);

	ipq_smem_target_info_ptr = (ipq_smem_target_info_t*)
		(uintptr_t)ipq_smem_target_info_addr;
	if (!ipq_smem_target_info_ptr)
		return -EFAULT;

	if (ipq_smem_target_info_ptr->identifier !=
			IPQ_SMEM_TARGET_INFO_IDENTIFIER)
		return -EFAULT;

	memcpy((void*)smem_tinfo_ptr,
			(void*)(uintptr_t)ipq_smem_target_info_ptr,
			sizeof(ipq_smem_target_info_t));
	return 0;
}

int ipq_read_tcsr_boot_misc(void)
{
	u32 dmagic;
#ifdef CONFIG_SCM
	scm_param param;
	int feat_avail = 0;
	int ret;

	if (!g_recovery_path)
	{
		/* The TCSR DLOAD register is protected in latest TZ
		 * for the IPQ5332 target.
		 * Old TZ will allow direct read.
		 * Use the qca_scm_is_feature_available() call to know
		 * if TZ supports direct or scm read. Based on return
		 * value, read the TCSR WONCE register appropriately.
		 */
		do {
			ret = -ENOTSUPP;
			CHECK_FEATURE(param, 0x6);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("Feature check scm failed\n");
				return 0;
			}
			feat_avail = le32_to_cpu(param.res.result[0]);
		} while(0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return 0;
		}
	}

	if (feat_avail == 0x401000)
	{
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_READ(param, (uintptr_t)TCSR_BOOT_MISC_REG);
			param.get_ret = true;
			ret = ipq_scm_call(&param);
			if (ret) {
				printf("dload magic read failed\n");
				return 0;
			}
			dmagic = le32_to_cpu(param.res.result[0]);
		} while(0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return 0;
		}
	}
	else {
		dmagic = *(TCSR_BOOT_MISC_REG);
	}
#else
	dmagic = *(TCSR_BOOT_MISC_REG);
#endif

	return dmagic;
}

void ipq_fdt_fixup_smem(void *blob)
{
	uint32_t reg[4];
	ipq_smem_target_info_t ipq_smem_target_info;
	ipq_smem_target_info_t *smem_tinfo_ptr = &ipq_smem_target_info;

	if (board_get_smem_target_info(&ipq_smem_target_info))
		return;

	reg[0] = 0;
	reg[1] = cpu_to_fdt32((uint32_t)smem_tinfo_ptr->smem_base_addr);
	reg[2] = 0;
	reg[3] = cpu_to_fdt32(smem_tinfo_ptr->smem_size);

	fdt_find_and_setprop(blob, "/reserved-memory/smem@4a800000/",
			"reg", reg, sizeof(reg), 0);
}

int ipq_uboot_fdt_fixup_smem(void *blob)
{
	uint32_t reg[2];
	ipq_smem_target_info_t ipq_smem_target_info;
	ipq_smem_target_info_t *smem_tinfo_ptr = &ipq_smem_target_info;

	if (board_get_smem_target_info(&ipq_smem_target_info))
		return -EFAULT;

	reg[0] = cpu_to_fdt32((uint32_t)smem_tinfo_ptr->smem_base_addr);
	reg[1] = cpu_to_fdt32(smem_tinfo_ptr->smem_size);

	fdt_find_and_setprop(blob, "/reserved-memory/smem_region@4A800000",
			"reg", reg, sizeof(reg), 0);
	return 0;
}

void ipq_uboot_fdt_fixup(uint32_t machid)
{
	int ret, len = 0, config_nos = 0;
	char config[CONFIG_NAME_MAX_LEN];
	char *config_list[6] = { NULL };

	switch (machid)
	{
		case MACH_TYPE_IPQ5332_RDP442:
			config_list[config_nos++] = "config-mi01.3";
			config_list[config_nos++] = "config@mi01.3";
			config_list[config_nos++] = "config@rdp442";
			config_list[config_nos++] = "config-rdp442";
			break;
		case MACH_TYPE_IPQ5332_RDP473:
			config_list[config_nos++] = "config-mi01.7";
			config_list[config_nos++] = "config@mi01.7";
			config_list[config_nos++] = "config@rdp473";
			config_list[config_nos++] = "config-rdp473";
			break;
		case MACH_TYPE_IPQ5332_RDP480:
			config_list[config_nos++] = "config-mi01.13";
			config_list[config_nos++] = "config@mi01.13";
			config_list[config_nos++] = "config@rdp480";
			config_list[config_nos++] = "config-rdp480";
			break;
		case MACH_TYPE_IPQ5332_RDP486:
			config_list[config_nos++] = "config-mi01.3-c3";
			config_list[config_nos++] = "config@mi01.3-c3";
			config_list[config_nos++] = "config@rdp486";
			config_list[config_nos++] = "config-rdp486";
			break;
	}

	if (config_nos)
	{
		while (config_nos--) {
			strlcpy(&config[len], config_list[config_nos],
					CONFIG_NAME_MAX_LEN - len);
			len += strnlen(config_list[config_nos],
					CONFIG_NAME_MAX_LEN) + 1;
			if (len > CONFIG_NAME_MAX_LEN) {
				printf("skipping uboot fdt fixup err: "
						"config name len overflow\n");
				return;
			}
		}

		/*
		 * Open in place with a new length.
		*/
		ret = fdt_open_into(gd->fdt_blob, (void *)gd->fdt_blob,
				fdt_totalsize(gd->fdt_blob) + len);
		if (ret)
			printf("uboot-fdt-fixup: Cannot expand FDT: %s\n",
					fdt_strerror(ret));

		ret = fdt_setprop((void *)gd->fdt_blob, 0, "config_name",
				config, len);
		if (ret)
			printf("uboot-fdt-fixup: unable to set "
					"config_name(%d)\n", ret);
	}
	return;
}

void ipq_config_cmn_clock(void)
{
	unsigned int reg_val;
	/*
	 * Init CMN clock for ethernet
	 */
	reg_val = readl(PLL_REFERENCE_CLOCK);
	reg_val = (reg_val & FREQUENCY_MASK) | INTERNAL_48MHZ_CLOCK;
	/*Select clock source*/
	writel(reg_val, PLL_REFERENCE_CLOCK);

	/* Soft reset to calibration clocks */
	reg_val = readl(PLL_POWER_ON_AND_RESET);
	reg_val &= ~BIT(6);
	writel(reg_val, PLL_POWER_ON_AND_RESET);
	mdelay(1);
	reg_val |= BIT(6);
	writel(reg_val, PLL_POWER_ON_AND_RESET);
	mdelay(1);
}

#ifdef CONFIG_ARM64
/*
 * Set XN (PTE_BLOCK_PXN | PTE_BLOCK_UXN)bit for all dram regions
 * and Peripheral block except uboot code region
 */
static struct mm_region ipq5332_mem_map[] = {
	{
		/* Peripheral block */
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = CFG_SYS_SDRAM_BASE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN

	}, {
		/* DDR region upto u-boot CONFIG_TEXT_BASE */
		.virt = CFG_SYS_SDRAM_BASE,
		.phys = CFG_SYS_SDRAM_BASE,
		.size = CONFIG_TEXT_BASE - CFG_SYS_SDRAM_BASE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* DDR region U-boot text base */
		.virt = CONFIG_TEXT_BASE,
		.phys = CONFIG_TEXT_BASE,
		.size = CONFIG_TEXT_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/*
		 * DDR region after u-boot text base
		 * added dummy 0xBAD0FF5EUL,
		 * will update the actual DDR limit
		 */
		.virt = CONFIG_TEXT_BASE + CONFIG_TEXT_SIZE,
		.phys = CONFIG_TEXT_BASE + CONFIG_TEXT_SIZE,
		.size = 0x0UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = ipq5332_mem_map;
#endif

void board_update_RFA_settings(void)
{
	int ret;
	int slotId = 0; /* Default slotId 0 */
	uint32_t reg_val;
	uint32_t calDataOffset;
	uint32_t calData;
	uint32_t CDACIN;
	uint32_t CDACOUT;
	scm_param param;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	/* Check for Q6 DISABLE bit 15 */
	if ((readl(QFPROM_RAW_FEATURE_CONFIG_ROW0_LSB) >> 15) & 0x1)
		return;

	calDataOffset  = (((slotId * 150) + 4) * 1024 + 0x66C4);
	ret = get_partition_data("0:ART", calDataOffset, (uint8_t*)&calData, 4,
					sfi->flash_type);
	if (ret < 0) {
		printf("Failed to read from ART : %d\n", ret);
		return;
	}

	CDACIN = calData & 0x3FF;
	CDACOUT = (calData >> 16) & 0x1FF;

	if(((CDACIN == 0x0) || (CDACIN == 0x3FF)) &&
			((CDACOUT == 0x0) || (CDACOUT == 0x1FF))) {
		CDACIN = 0x230;
		CDACOUT = 0xB0;
	}

	CDACIN = CDACIN << 22;
	CDACOUT = CDACOUT << 13;

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_READ_PHY_REG(param, PHYA0_RFA_RFA_RFA_OTP_OTP_OV_1);
		param.get_ret = true;
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("ipq_scm_call: PHYA0_RFA_RFA_RFA_OTP_OTP_OV_1"
				"read failed, ret : %d", ret);
			return;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return;
	}

	reg_val = param.res.result[0];

	reg_val = (reg_val & 0xFFF9FFFF) | (0x3 << 17u);

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_WRITE_PHY_REG(param, PHYA0_RFA_RFA_RFA_OTP_OTP_OV_1,
								reg_val);
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("ipq_scm_call: PHYA0_RFA_RFA_RFA_OTP_OTP_OV_1"
				"write failed, ret : %d", ret);
			return;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return;
	}

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_READ_PHY_REG(param, PHYA0_RFA_RFA_RFA_OTP_OTP_XO_0);
		param.get_ret = true;
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("ipq_scm_call: PHYA0_RFA_RFA_RFA_OTP_OTP_XO_0"
				"read failed, ret : %d", ret);
			return;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return;
	}

	reg_val = param.res.result[0];

	if((CDACIN == (reg_val & (0x3FF << 22))) &&
			(CDACOUT == (reg_val & (0x1FF << 13)))) {
		printf("ART data same as PHYA0_RFA_RFA_RFA_OTP_OTP_XO_0\n");
		return;
	}

	reg_val = ((reg_val & 0x1FFF) | ((CDACIN | CDACOUT) & (~0x1FFF)));

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_WRITE_PHY_REG(param, PHYA0_RFA_RFA_RFA_OTP_OTP_XO_0,
								reg_val);
		ret = ipq_scm_call(&param);

		if (ret) {
			printf("ipq_scm_call: PHYA0_RFA_RFA_RFA_OTP_OTP_XO_0"
				"write failed, ret : %d", ret);
			return;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return;
	}
}

int execute_dprv2(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned long loadaddr, filesize;
	unsigned long default_hex_val = 0xFFFFFFFF;
	uint32_t dpr_status = 0;
	scm_param param;

	memset(&param, 0, sizeof(scm_param));
	if (argc > cmdtp->maxargs || argc == 2)
		goto fail;

	if (argc == cmdtp->maxargs) {
		loadaddr = simple_strtoul(argv[1], NULL, 16);
		filesize = simple_strtoul(argv[2], NULL, 16);
	} else {
		loadaddr = env_get_hex("fileaddr", default_hex_val);
		if (loadaddr == default_hex_val)
			goto fail;

		filesize = env_get_hex("filesize", default_hex_val);
		if (filesize == default_hex_val)
			goto fail;
	}

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_EXECUTE_DPR(param, loadaddr, filesize);
		param.get_ret = true;
		ret = ipq_scm_call(&param);

		dpr_status = param.res.result[0];
		if (ret || dpr_status) {
			printf("Error in DPR Processing ret : %d, "
					"dpr_status : %d\n",
					ret, dpr_status);
		} else
			printf("DPR Process Successful\n");
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		goto fail;
	}


fail:
	return ret;
}

bool is_valid_dump(char *dump_name)
{
	bool skip_dump = false;
	int ret = -1;

	if(!strncmp("IMEM2.BIN", dump_name, 9))
	{
		scm_param param;

		do {
			ret = -ENOTSUPP;

			CHECK_FEATURE(param, TME_LOG_DUMP_FEATURE_ID);
			param.get_ret = true;
			ret = ipq_scm_call(&param);

			if(!ret && param.res.result[0] == \
					TME_LOG_DUMP_FEATURE_VERSION) {
				skip_dump = true;
			}
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
		}

	}
	return skip_dump;
}

bool is_atf_enbled(void)
{
	enum atf_status_t {
		ATF_STATE_DISABLED,
		ATF_STATE_ENABLED,
		ATF_STATE_UNKNOWN,
	} atf_status = ATF_STATE_UNKNOWN;
	scm_param param;
	int ret = -1;

	if (likely(atf_status != ATF_STATE_UNKNOWN))
		return (atf_status == ATF_STATE_ENABLED);

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_CHECK_SCM_SUPPORT(param, SCM_SIP_FNID(QCOM_SCM_SVC_INFO,
						QCOM_GET_SECURE_STATE_CMD));
		param.get_ret = true;
		ret = ipq_scm_call(&param);

		if(!ret && (le32_to_cpu(param.res.result[0]) > 0)) {
			do {
				ret = -ENOTSUPP;
				check_atf_support(param);
				param.get_ret = true;

				ret = ipq_scm_call(&param);
				if((ret == 0) && (param.res.result[0] & 0x80))
					atf_status = ATF_STATE_ENABLED;
			} while (0);

			if (ret == -ENOTSUPP) {
				printf("Unsupported SCM call\n");
				return false;
			}

		} else {
			return false;
		}

	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
		return false;
	}

	return atf_status == ATF_STATE_ENABLED;
}

uint32_t image_auth_check(void) {

	uint32_t board_type = gd->board_type;
	uint32_t ret = -1;

	switch (gd->ram_size) {
	case SZ_128M:
		ret = 0;
		break;
	default:
		ret = (board_type & SECURE_BOARD);
		break;
	}

	return ret;
}

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
bool is_version_rollback_support(void) {

	if (gd->board_type & ATF_ENABLED) {
		return false;
	}

	return true;
}
#endif
