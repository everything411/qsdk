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
#include <mtd_node.h>
#include <jffs2/load_kernel.h>
#include <linux/delay.h>
#include <irq_func.h>
#include <hang.h>

#include <dm/lists.h>
#include <dm/root.h>

/* External declarations for SPL functions */
extern u8 get_boot_mode(void);

#define IM_SLEEP_CLK				0x1834020
/* MACH IDs for various RDPs */
#define MACH_TYPE_IPQ9650_EMULATION		0xF060000
#define MACH_TYPE_IPQ9650_DB_JU01		0x1090000
#define MACH_TYPE_IPQ9650_DB_JU01_C1		0x1090001
#define MACH_TYPE_IPQ9650_DB_JU01_C2		0x1090002
#define MACH_TYPE_IPQ9650_DB_JU02		0x1090100
#define MACH_TYPE_IPQ9650_RDP488		0x8090000
#define MACH_TYPE_IPQ9650_RDP506		0x8090001
#define MACH_TYPE_IPQ9650_RDP489		0x8090100
#define MACH_TYPE_IPQ9650_RDP490		0x8090200
#define MACH_TYPE_IPQ9650_RDP491		0x8090300
#define MACH_TYPE_IPQ9650_RDP507		0x8090002
#define MACH_TYPE_IPQ9650_RDP525		0x8090003
#define MACH_TYPE_IPQ9650_RDP529		0x8090004
#define MACH_TYPE_IPQ9650_RDP530		0x8090005
#define MACH_TYPE_IPQ9650_RDP532		0x8090006
#define MACH_TYPE_IPQ9650_TB_JU01_1		0x2090000

#define TIMEOUT_MS				30000
#define CLK_SRC					32000
#define WDT_ENABLE_REG				0xF410008
#define WDT_RST_REG				0xf410004
#define WDT_BARK_TIME_REG			0xf410010
#define WDT_BITE_TIME_REG			0xf410014

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
		.size = 0x00020000,
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

struct dts_fixup ipq9650_mmc_fixup [] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"},1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc@0/dma-controller@7984000/", {"/soc@0/dma-controller@7984000/%status%?disabled"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq9650_mmc_fixup;

struct dts_fixup ipq9650_usb_fixup [] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq9650_usb_fixup;

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "spansion,s25fs128s1", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq9650-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

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

struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ9650_EMULATION,
		"ipq9650-emulation",
		"emulation-fbc",
		"1",
	},
	{
		MACH_TYPE_IPQ9650_DB_JU01,
		"ipq9650-db-ju01",
		"db-ju01",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_DB_JU01_C1,
		"ipq9650-db-ju01-c1",
		"db-ju01-c1",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_DB_JU01_C2,
		"ipq9650-db-ju01-c2",
		"1",
		"db-ju01-c2"
	},
	{
		MACH_TYPE_IPQ9650_DB_JU02,
		"ipq9650-db-ju02",
		"db-ju02",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP488,
		"ipq9650-rdp488",
		"rdp488",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP506,
		"ipq9650-rdp506",
		"rdp506",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP507,
		"ipq9650-rdp507",
		"rdp507",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP525,
		"ipq9650-rdp525",
		"rdp525",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP529,
		"ipq9650-rdp529",
		"rdp529",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP530,
		"ipq9650-rdp530",
		"rdp530",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP532,
		"ipq9650-rdp532",
		"rdp532",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP489,
		"ipq9650-rdp489",
		"rdp489",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP490,
		"ipq9650-rdp490",
		"rdp490",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_RDP491,
		"ipq9650-rdp491",
		"rdp491",
		"1"
	},
	{
		MACH_TYPE_IPQ9650_TB_JU01_1,
		"ipq9650-tb-ju01.1",
		"1",
		"tb-ju01.1"
	},
};

struct multidtb_config ipq9650_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq9650_dtb_info;

/* Board-Specific Communication Type Mapping */
const u8 comm_type_map[FUNC_MAX] = {
	[FUNC_LIST_FUSE]       = COMM_TYPE_TME,
	[FUNC_DUMP_FUSE]       = COMM_TYPE_TME,
	[FUNC_SECURE_AUTH]     = COMM_TYPE_TME,
	[FUNC_CHECK_SECURE_BOOT] = COMM_TYPE_TME,
	[FUNC_IMAGE_AUTH]      = COMM_TYPE_TME,
	[FUNC_AUTH_ROOTFS_ELF] = COMM_TYPE_TME,
	[FUNC_FUSEIPQ]		 = COMM_TYPE_OPTEE,
	[FUNC_ICE_CONFIGURE]	 = COMM_TYPE_OPTEE,
	[FUNC_ICE_KEY_CONFIGURE] = COMM_TYPE_OPTEE,
	[FUNC_AES_ENCRYPT]       = COMM_TYPE_TME,
	[FUNC_AES_DECRYPT]       = COMM_TYPE_TME,
	[FUNC_AES_DERIVE_KEY]    = COMM_TYPE_TME,
	[FUNC_AES_CLEAR_KEY]     = COMM_TYPE_TME,
	[FUNC_AES_DERIVE_KEY_MAX_CTXT] = COMM_TYPE_TME
};

#ifdef CONFIG_DTB_RESELECT
void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}
#endif /* CONFIG_DTB_RESELECT */


static void ipq_enable_im_sleep_clk(void)
{
    /*
     * Enable the IM_SLEEP clock
     * This clk require for Block control reset
     */
    writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);
}

#if defined(CONFIG_SPL)
void ipq_spl_board_early_init_f(void)
{
    ipq_enable_im_sleep_clk();
}
#endif

void ipq_board_early_init_f(void)
{
    ipq_enable_im_sleep_clk();
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

	if (!smem_tinfo_ptr)
		return -EINVAL;

	ipq_smem_target_info_addr = tcsr_wonce0_val |
		(((uint64_t)(tcsr_wonce1_val)) << 32);

	if (!ipq_smem_target_info_addr)
		return -EFAULT;

	ipq_smem_target_info_ptr = (struct ipq_smem_target_info*)
		(uintptr_t)ipq_smem_target_info_addr;

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

	int ret = fdt_find_and_setprop(blob, "/reserved-memory/smem@8a500000/",
					"reg", reg, sizeof(reg), 0);
	if (ret < 0)
		debug("Warning: Failed to update SMEM FDT node: %d\n", ret);
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

