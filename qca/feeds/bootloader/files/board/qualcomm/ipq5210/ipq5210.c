// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/io.h>
#include <string.h>
#include <stdlib.h>
#include <mach/ipq.h>
#include <fdtdec.h>
#include <config.h>
#include <init.h>
#include <mtd_node.h>
#include <jffs2/load_kernel.h>
#include <irq_func.h>
#include <hang.h>

#include <dm/lists.h>
#include <dm/root.h>

/* External declarations for SPL functions */
extern u8 get_boot_mode(void);

#define IM_SLEEP_CLK				0x1834020
/* MACH IDs for various RDPs */
#define MACH_TYPE_IPQ5210_EMULATION		0xf060000
#define MACH_TYPE_IPQ5210_DB_HM01_1		0x1080000
#define MACH_TYPE_IPQ5210_DB_HM01_C1		0x1080001
#define MACH_TYPE_IPQ5210_DB_HM02_1		0x1080100
#define MACH_TYPE_IPQ5210_DB_HM02_C1		0x1080101
#define MACH_TYPE_IPQ5210_RDP497		0x8080000
#define MACH_TYPE_IPQ5210_RDP498		0x8080100
#define MACH_TYPE_IPQ5210_RDP499		0x8080200
#define MACH_TYPE_IPQ5210_RDP500		0x8080101
#define MACH_TYPE_IPQ5210_RDP501		0x8080102
#define MACH_TYPE_IPQ5210_RDP502		0x8080201
#define MACH_TYPE_IPQ5210_RDP503		0x8080202
#define MACH_TYPE_IPQ5210_RDP504		0x8080203
#define MACH_TYPE_IPQ5210_RDP505		0x8080300
#define MACH_TYPE_IPQ5210_RDP511		0x8080400
#define MACH_TYPE_IPQ5210_RDP512		0x8080401
#define TIMEOUT_MS				30000
#define CLK_SRC					32000
#define WDT_ENABLE_REG				0xb017008
#define WDT_RST_REG				0xb017004
#define WDT_BARK_TIME_REG			0xb017010
#define WDT_BITE_TIME_REG			0xb017014

struct dts_fixup ipq5210_mmc_fixup [] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"},1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc@0/dma-controller@7984000/", {"/soc@0/dma-controller@7984000/%status%?disabled"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq5210_mmc_fixup;

struct dts_fixup ipq5210_usb_fixup [] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq5210_usb_fixup;

#ifdef CONFIG_IPQ_EARLY_WDT
void ipq_enable_non_sec_watchdog(void)
{
	/*
	 * Enabling non-secure WDT for early failure recovery support
	 */
	ulong bark_timeout_s = ((TIMEOUT_MS - 1)  * CLK_SRC) / 1000;
	ulong bite_timeout_s = (TIMEOUT_MS * CLK_SRC) / 1000;

	writel(0, WDT_ENABLE_REG);
	writel(BIT(0), WDT_RST_REG);
	writel(bark_timeout_s, WDT_BARK_TIME_REG);
	writel(bite_timeout_s, WDT_BITE_TIME_REG);
	writel(BIT(0), WDT_ENABLE_REG);
}
#endif

#if !defined(CONFIG_SPL)
void lowlevel_init(void)
{
#ifdef CONFIG_IPQ_EARLY_WDT
	ipq_enable_non_sec_watchdog();
#endif
}
#endif /* !CONFIG_SPL */

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "spansion,s25fs128s1", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq5210-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ5210_EMULATION,
		"ipq5210-emulation-fbc",
		"emulation-fbc",
		"1",
	},
	{
		MACH_TYPE_IPQ5210_DB_HM01_1,
		"ipq5210-db-hm01.1",
		"db-hm01.1",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_DB_HM01_C1,
		"ipq5210-db-hm01-c1",
		"db-hm01-c1",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_DB_HM02_1,
		"ipq5210-db-hm02.1",
		"db-hm02.1",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_DB_HM02_C1,
		"ipq5210-db-hm02-c1",
		"db-hm02-c1",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP497,
		"ipq5210-rdp497",
		"rdp497",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP498,
		"ipq5210-rdp498",
		"rdp498",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP499,
		"ipq5210-rdp499",
		"rdp499",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP500,
		"ipq5210-rdp500",
		"rdp500",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP501,
		"ipq5210-rdp501",
		"rdp501",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP502,
		"ipq5210-rdp502",
		"rdp502",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP503,
		"ipq5210-rdp503",
		"rdp503",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP504,
		"ipq5210-rdp504",
		"rdp504",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP505,
		"ipq5210-rdp505",
		"rdp505",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP511,
		"ipq5210-rdp511",
		"rdp511",
		"1"
	},
	{
		MACH_TYPE_IPQ5210_RDP512,
		"ipq5210-rdp512",
		"rdp512",
		"1"
	},
};

struct multidtb_config ipq5210_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq5210_dtb_info;

/* Board-Specific Communication Type Mapping */
const u8 comm_type_map[FUNC_MAX] = {
	[FUNC_LIST_FUSE]	 = COMM_TYPE_TME,
	[FUNC_DUMP_FUSE]	 = COMM_TYPE_TME,
	[FUNC_SECURE_AUTH]	 = COMM_TYPE_TME,
	[FUNC_CHECK_SECURE_BOOT] = COMM_TYPE_TME,
	[FUNC_IMAGE_AUTH]	 = COMM_TYPE_TME,
	[FUNC_AUTH_ROOTFS_ELF] 	 = COMM_TYPE_TME,
	[FUNC_FUSEIPQ]		 = COMM_TYPE_OPTEE,
	[FUNC_AES_ENCRYPT]       = COMM_TYPE_TME,
	[FUNC_AES_DECRYPT]       = COMM_TYPE_TME,
	[FUNC_AES_DERIVE_KEY]    = COMM_TYPE_TME,
	[FUNC_AES_CLEAR_KEY]     = COMM_TYPE_TME,
	[FUNC_AES_DERIVE_KEY_MAX_CTXT] = COMM_TYPE_TME
};

static struct crashdump_infos dumpinfo_n[] = {
	{
		/* DDR Bank 0 */
		.name = "EBICS.BIN",
		.start_addr = CFG_SYS_SDRAM_BASE,
		.size = 0xBAD0FF5E,
		.dump_level = FULLDUMP,
		.split_bin_sz = SZ_1G,
		.is_aligned_access = false,
		.compression_support = true
	},
#if (CONFIG_NR_DRAM_BANKS > 1)
	{
		/* DDR Bank 1 */
		.name = "EBICS.BIN",
		.start_addr = 0xBAD0FF5E,
		.size = 0xBAD0FF5E,
		.dump_level = FULLDUMP,
		.split_bin_sz = SZ_1G,
		.is_aligned_access = false,
		.compression_support = true
	},
#endif
	{
		.name = "IMEM.BIN",
		.start_addr = 0x08600000,
		.size = 0x18000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false
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

struct crashdump_infos *board_dumpinfo = dumpinfo_n;

uint8_t *board_dump_entries = &dump_entries_n;

#ifdef CONFIG_DTB_RESELECT
void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ5210_EMULATION:
		strlcpy(dtb->dts_name, "ipq5210-emulation",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */

#if defined(CONFIG_SPL)
void ipq_spl_board_early_init_f(void)
{
	/*
	 * Enable the IM_SLEEP clock
	 * This clk require for Block control reset
	 */
	writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);
}
#endif

void ipq_board_early_init_f(void)
{
	/*
	 * Enable the IM_SLEEP clock
	 * This clk require for Block control reset
	 */
	writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);
}

#if !defined(CONFIG_SPL)
void reset_cpu(void)
{
#ifdef CONFIG_FAILSAFE_V2
	/* Clear the bootcnt value for every cold reset */
	if (!ipq_iscrashed())
		writel(0, IMEM_ADDR_BOOTCNT);
#endif
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V2);
#endif
	psci_sys_reset(SYSRESET_COLD);
}
#endif /* !CONFIG_SPL */

#if defined(CONFIG_SPL_BUILD)
void ipq_spl_pre_reset_seq(void)
{
	static int exception_count;
	u8 boot_mode;

	/**
	 * Disable interrupts to prevent race conditions
	 */
	disable_interrupts();

	/**
	 * Flush all caches to ensure data consistency
	 */
	flush_dcache_all();

	/**
	 * Increment exception counter
	 */
	exception_count++;

	/**
	 * Check for nested exceptions
	 */
	if (exception_count > 1) {
		printf("Nested exception detected (count=%d)\n", exception_count);
		pr_err("System entered hang state\n");
		hang();
	} else {
		boot_mode = get_boot_mode();
		switch (boot_mode) {
		case IPQ_SPL_BOOT_MODE_FORCE_INACIVE:
		case IPQ_SPL_BOOT_MODE_INTERMEDIATE_RESET:
		case IPQ_SPL_BOOT_MODE_FAILOVER_EN:
			printf("Resetting CPU ...\n");
			break;

		default:
			printf("Entering EDL ...\n");
			IPQ_SPL_SET_TCSR_EDL();
		}
	}

	/*
	 * Check and clear the FSM status before reset
	 */
	if (IPQ_SPL_GCC_FSM_STATE() == IPQ_SPL_FSM_SP_PRE_DDR_INIT_ST_VAL) {
		setbits_le32(IPQ_SPL_GCC_FSM_CTRL_ADDR,
				IPQ_SPL_FSM_DDR_OUT_OF_SELF_RFRSH_BIT |
				IPQ_SPL_FSM_SECOND_PASS_COMPLETE_BIT);

		setbits_le32(IPQ_SPL_GCC_RESET_DEBUG_ADDR,
				IPQ_SPL_GCC_ALLOW_OTHER_RST_DBG_EN);
	} else {
		/*
		 * Clear the GCC RESET DBG REG
		 */
		writel(0, IPQ_SPL_GCC_RESET_DEBUG_ADDR);
	}

	/*
	 * Reset the GCC reset status reg, before PS_HOLD
	 */
	writel(U32_MAX, IPQ_SPL_GCC_RESET_STATUS_ADDR);
}
#endif /* CONFIG_SPL_BUILD */

#if defined(CONFIG_ARCH_32BIT_SUPPORT) && defined(CONFIG_IPQ_DYNAMIC_RELOCATION)
#define IPQ5210_TINY_USABLE_RAM_TOP	0x87c00000

/*
 * On 128MB DDR, clamp ram_top so dynamic relocation does not
 * overlap the fixed secure firmware regions near the top of DRAM.
 */
phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	phys_addr_t top = gd->ram_base + get_effective_memsize();

	switch (gd->ram_size) {
	case SZ_128M:
		if (IPQ5210_TINY_USABLE_RAM_TOP < top)
			top = IPQ5210_TINY_USABLE_RAM_TOP;
		break;
	default:
		break;
	}

	return top;
}
#endif

uint32_t is_board_support_image_auth(void)
{
	u32 board_type = gd->board_type;
	u32 ret = 0;

	switch (gd->ram_size) {
	case SZ_128M:
		break;
	default:
		ret = (board_type & SECURE_BOARD);
		break;
	}

	return ret;
}

#ifdef CONFIG_IPQ_MVEW
#define SOC_VER_MBN_V8_MAJOR		1
#define SOC_VER_MBN_V8_MINOR		2

uint32_t ipq_get_required_mbn_version(void)
{
	struct soc_info *socinfo = ipq_get_socinfo();

	if (socinfo && socinfo->soc_version_major == SOC_VER_MBN_V8_MAJOR &&
			socinfo->soc_version_minor == SOC_VER_MBN_V8_MINOR)
		return 8;

	return MVEW_MBN_VERSION_DEFAULT;
}
#endif

void ipq_update_comm_type(void)
{
	struct ipq_board_info *bdinfo = ipq_get_bdinfo();

	if (bdinfo)
		bdinfo->comm_type_map = comm_type_map;
}

int board_get_smem_target_info(struct ipq_smem_target_info *smem_tinfo_ptr)
{
	uint32_t tcsr_wonce0_val = readl(TCSR_TZ_WONCE0);
	uint32_t tcsr_wonce1_val = readl(TCSR_TZ_WONCE1);
	uint64_t ipq_smem_target_info_addr;
	struct ipq_smem_target_info *ipq_smem_target_info_ptr;

	ipq_smem_target_info_addr = tcsr_wonce0_val |
		(((uint64_t)(tcsr_wonce1_val)) << 32);

	ipq_smem_target_info_ptr = (struct ipq_smem_target_info*)
		(uintptr_t)ipq_smem_target_info_addr;
	if (!ipq_smem_target_info_ptr)
		return -EFAULT;

	if (ipq_smem_target_info_ptr->identifier !=
			IPQ_SMEM_TARGET_INFO_IDENTIFIER)
		return -EFAULT;

	memcpy((void*)smem_tinfo_ptr,
			(void*)(uintptr_t)ipq_smem_target_info_ptr,
			sizeof(struct ipq_smem_target_info));
	return 0;
}

void ipq_fdt_fixup_smem(void *blob)
{
	uint32_t reg[4];
	struct ipq_smem_target_info ipq_smem_target_info;
	struct ipq_smem_target_info *smem_tinfo_ptr = &ipq_smem_target_info;

	if (board_get_smem_target_info(&ipq_smem_target_info))
		return;

	reg[0] = 0;
	reg[1] = cpu_to_fdt32((uint32_t)smem_tinfo_ptr->smem_base_addr);
	reg[2] = 0;
	reg[3] = cpu_to_fdt32(smem_tinfo_ptr->smem_size);

	fdt_find_and_setprop(blob, "/reserved-memory/smem@8a500000/",
			"reg", reg, sizeof(reg), 0);
}

int ipq_uboot_fdt_fixup_smem(void *blob)
{
	uint32_t reg[4];
	struct ipq_smem_target_info ipq_smem_target_info;
	struct ipq_smem_target_info *smem_tinfo_ptr = &ipq_smem_target_info;

	if (board_get_smem_target_info(&ipq_smem_target_info))
		return -EFAULT;

	reg[0] = 0;
	reg[1] = cpu_to_fdt32((uint32_t)smem_tinfo_ptr->smem_base_addr);
	reg[2] = 0;
	reg[3] = cpu_to_fdt32(smem_tinfo_ptr->smem_size);

	fdt_find_and_setprop(blob, "/reserved-memory/smem_region@8a500000",
			"reg", reg, sizeof(reg), 0);
	return 0;
}

void ipq_uboot_fdt_fixup_usb(void *blob)
{
	return;
}

#ifdef CONFIG_BOOT_BANK_FIXUP
int ipq_uboot_fdt_fixup_booted_bank(void *blob)
{
	return fdt_set_booted_bank_property(blob);
}
#endif

int ipq_uboot_fdt_fixup(void *blob, enum fixup_type type)
{
	switch(type) {
	case UBOOT_FIXUP_SMEM:
		ipq_uboot_fdt_fixup_smem(blob);
		break;
	case UBOOT_FIXUP_USB:
		ipq_uboot_fdt_fixup_usb(blob);
		break;
#ifdef CONFIG_BOOT_BANK_FIXUP
	case UBOOT_FIXUP_BOOTED_BANK:
		ipq_uboot_fdt_fixup_booted_bank(blob);
		break;
#endif
	default:
		break;
	}

	return 0;
}

void ipq_bind_optee_driver(void)
{
	struct udevice *dev;
	ofnode node = ofnode_path("/firmware/optee");
	int ret;

	ret = device_bind_driver_to_node(dm_root(), "optee", "optee", node, &dev);
	if (ret)
		printf("optee: bind failed (%d)\n", ret);
}
