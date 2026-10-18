// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <mach/ipq.h>
#include <asm/io.h>
#include <asm/io.h>
#include <string.h>
#include <stdlib.h>
#include <fdtdec.h>
#include <config.h>
#include <mtd_node.h>

#define IM_SLEEP_CLK				0x1834020
#define GCC_GPLL0_USER_CTL                      0x1820018
#define PLLOUT_LV_AUX_EN                        (BIT(1)|BIT(2))
#define MACH_TYPE_IPQ5424_RDP464_C2		0x8070000
#define MACH_TYPE_IPQ5424_RDP464		0x8070001
#define MACH_TYPE_IPQ5424_RDP464_C3		0x8070002
#define MACH_TYPE_IPQ5424_RDP492		0x8070004
#define MACH_TYPE_IPQ5424_RDP464_QCE2204	0x8070005
#define MACH_TYPE_IPQ5424_RDP508		0x8070003
#define MACH_TYPE_IPQ5424_RDP466_C2		0x8070100
#define MACH_TYPE_IPQ5424_RDP466_C3		0x8070102
#define MACH_TYPE_IPQ5424_RDP466		0x8071100
#define MACH_TYPE_IPQ5424_RDP496		0x8071110
#define MACH_TYPE_IPQ5424_RDP466_RFFE		0x8070110
#define MACH_TYPE_IPQ5424_RDP485_C3		0x8070103
#define MACH_TYPE_IPQ5424_RDP485_C4		0x8070104
#define MACH_TYPE_IPQ5424_RDP485_C2		0x8070101
#define MACH_TYPE_IPQ5424_RDP485		0x8071101
#define MACH_TYPE_IPQ5424_RDP485_RFFE_C2	0x8071111
#define MACH_TYPE_IPQ5424_RDP485_RFFE		0x8070111
#define MACH_TYPE_IPQ5424_RDP487		0x8070200
#define MACH_TYPE_IPQ5424_RDP487_C2		0x8070201
#define MACH_TYPE_IPQ5424_RDP487_C3		0x8070202
#define MACH_TYPE_IPQ5424_DB_MR01_1		0x1070000

#define TIMEOUT_MS				30000
#define CLK_SRC					32000
#define WDT_ENABLE_REG				0xf410008
#define WDT_RST_REG				0xf410004
#define WDT_BARK_TIME_REG			0xf410010
#define WDT_BITE_TIME_REG			0xf410014

/* USB softsku fuse */
#define USB_SOFTSKU_STATUS			0xA628C
#define USB_SOFTSKU_STATUS_DISABLE		BIT(0)

struct dts_fixup ipq5424_mmc_fixup[] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq5424_mmc_fixup;

struct dts_fixup ipq5424_usb_fixup[] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{ "/soc@0/usb2@1e00000/dwc3@1e00000/",
		{ "/soc@0/usb2@1e00000/dwc3@1e00000%dr_mode%?peripheral",
		"/soc@0/usb2@1e00000/dwc3@1e00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq5424_usb_fixup;

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "spansion,s25fs128s1", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq5424-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ5424_RDP464,
		"ipq5424-rdp464",
		"rdp464",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP464_C2,
		"ipq5424-rdp464-c2",
		"rdp464-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP464_C3,
		"ipq5424-rdp464-c3",
		"rdp464-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP492,
		"ipq5424-rdp492",
		"rdp492",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP464_QCE2204,
		"ipq5424-rdp464-qce2204",
		"rdp464-qce2204",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP508,
		"ipq5424-rdp508",
		"rdp508",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466,
		"ipq5424-rdp466",
		"rdp466",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466_C2,
		"ipq5424-rdp466-c2",
		"rdp466-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466_C3,
		"ipq5424-rdp466",
		"rdp466-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485,
		"ipq5424-rdp485",
		"rdp485",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_C2,
		"ipq5424-rdp485-c2",
		"rdp485-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_C3,
		"ipq5424-rdp485",
		"rdp485-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_C4,
		"ipq5424-rdp485",
		"rdp485-c4",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP487,
		"ipq5424-rdp487",
		"rdp487",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP487_C2,
		"ipq5424-rdp487",
		"rdp487-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP487_C3,
		"ipq5424-rdp487-c3",
		"rdp487-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP466_RFFE,
		"ipq5424-rdp466",
		"rdp466-rffe",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP496,
		"ipq5424-rdp466",
		"rdp496",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_RFFE,
		"ipq5424-rdp485",
		"rdp485-rffe",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_RDP485_RFFE_C2,
		"ipq5424-rdp485-c2",
		"rdp485-rffe-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ5424_DB_MR01_1,
		"ipq5424-db-mr01.1",
		"db-mr01.1",
		NULL
	},
};

struct multidtb_config ipq5424_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq5424_dtb_info;

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
		.size = 0x00001000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false
	},
	{
		.name = "TZ_LOG.BIN",
		.start_addr = 0x0860C000,
		.size = 0x00003000,
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

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ5424_RDP466_C3:
		strlcpy(dtb->dts_name, "ipq5424-rdp466-c3",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_C3:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-c3",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_C4:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-c4",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP466_RFFE:
		strlcpy(dtb->dts_name, "ipq5424-rdp466-rffe",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP496:
		strlcpy(dtb->dts_name, "ipq5424-rdp496",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_RFFE:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-rffe",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP485_RFFE_C2:
		strlcpy(dtb->dts_name, "ipq5424-rdp485-rffe-c2",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5424_RDP487_C2:
		strlcpy(dtb->dts_name, "ipq5424-rdp487-c2",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

}

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

void ipq_board_early_init_f(void)
{
	/*
	 * Enable the IM_SLEEP clock
	 * This clk require for Block control reset
	 */
	writel((readl(IM_SLEEP_CLK) | BIT(0)), IM_SLEEP_CLK);

	/*
	 * enable gpll0 aux clock
	 */
	writel(readl(GCC_GPLL0_USER_CTL) | PLLOUT_LV_AUX_EN,
	       GCC_GPLL0_USER_CTL);
}

bool is_atf_enbled(void)
{
	uint32_t atf_status = 0;

	ipq_smem_get_item((void *)&atf_status, SMEM_ATF_ENABLE, 0,
				sizeof(uint32_t));

	return atf_status ? true : false;
}

uint32_t is_board_support_image_auth(void)
{

	uint32_t board_type = gd->board_type;
	uint32_t ret = 0;

	switch (gd->ram_size) {
	case SZ_128M:
		break;
	default:
		ret = (board_type & SECURE_BOARD);
		break;
	}

	return ret;
}

int execute_dprv3(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned long loadaddr, filesize;
	unsigned long default_hex_val = 0xFFFFFFFF;
	uint32_t dpr_status = 0;
	struct scm_param param;

	memset(&param, 0, sizeof(struct scm_param));
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
		IPQ_SCM_EXECUTE_DPR(param, loadaddr, filesize, 0, 0);
		param.get_ret = true;
		ret = ipq_scm_call(&param);
		dpr_status = param.res.result[0];

		if (ret || dpr_status) {
			printf("Error in DPR Processing ret : %d, " \
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

#if defined(CONFIG_SPL)
void reset_cpu(void) {}
#else
void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V2);
#endif
	psci_sys_reset(SYSRESET_COLD);
}
#endif

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

	fdt_find_and_setprop(blob, "/reserved-memory/smem@8a800000/",
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

	fdt_find_and_setprop(blob, "/reserved-memory/smem_region@8a800000",
			"reg", reg, sizeof(reg), 0);
	return 0;
}

void ipq_uboot_fdt_fixup_usb(void *blob)
{
	int ret = 0;

	if (!(readl(USB_SOFTSKU_STATUS) & USB_SOFTSKU_STATUS_DISABLE))
		return;

	ret = fdt_status_okay_by_pathf(blob, "/soc@0/usb2@8af8800/");
	if (ret <0) {
		printf("failed to disable the usb2@8af8800"
				" node, err: %d \n", ret);
		return;
	}

	ret = fdt_status_disabled_by_pathf(blob, "/soc@0/usb@8af8800/");
	if (ret <0) {
		printf("failed to enable the usb@8af8800"
				" node, err: %d \n", ret);
	}
}

int ipq_uboot_fdt_fixup(void *blob, enum fixup_type type)
{
	switch(type) {
	case UBOOT_FIXUP_SMEM:
		ipq_uboot_fdt_fixup_smem(blob);
		break;
	case UBOOT_FIXUP_USB:
		ipq_uboot_fdt_fixup_usb(blob);
		break;
	default:
		break;
	}

	return 0;
}

void ipq_fdt_fixup_sku_based_usb_config(void *blob)
{
	if (!(readl(USB_SOFTSKU_STATUS) & USB_SOFTSKU_STATUS_DISABLE))
		return;

	parse_fdt_fixup("/soc@0/phy@7b000/%phandle%0xe0", blob);
	parse_fdt_fixup("/soc@0/usb3@8a00000/dwc3@8a00000/%phys%0xe0", blob);
	parse_fdt_fixup("/soc@0/usb3@8a00000/dwc3@8a00000/%phy-names%?usb2-phy", blob);
	parse_fdt_fixup("/soc@0/usb3@8a00000/%qcom,select-utmi-as-pipe-clk%1", blob);
}

void ipq_fdt_fixup_atf(void *blob)
{
	int ret = 0;
	if (!(gd->board_type & ATF_ENABLED))
		return;

	ret = fdt_status_okay_by_pathf(blob, "/reserved-memory/atf@8a832000");
	if (ret <0) {
		printf("failed to enable the atf node, err: %d \n", ret);
		return;
	}

	fdt_status_disabled_by_pathf(blob, "/reserved-memory/tz@0x8a600000");
}
