// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
#include <asm-generic/gpio.h>
#include <dm.h>
#include <linux/err.h>

#define GCC_GPLL0_USER_CTL			0x1820018
#define PLLOUT_LV_AUX_EN			(BIT(1)|BIT(2))
#define PLL_POWER_ON_AND_RESET			0x9B780
#define PLL_REFERENCE_CLOCK			0x9B784
#define FREQUENCY_MASK				0xfffffdf0
#define INTERNAL_48MHZ_CLOCK			0x7
#define CONFIG_NAME_MAX_LEN			128

#define TIMEOUT_MS				30000
#define CLK_SRC					32000
#define WDT_ENABLE_REG				0xf410008
#define WDT_RST_REG				0xf410004
#define WDT_BARK_TIME_REG			0xf410010
#define WDT_BITE_TIME_REG			0xf410014

DECLARE_GLOBAL_DATA_PTR;

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

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{ MACH_TYPE_IPQ5424_EMU, "ipq5424-emulation"},
	{ MACH_TYPE_IPQ5424_EMU_FBC, "ipq5424-emulation"},
	{ MACH_TYPE_IPQ5424_RDP464, "ipq5424-rdp464"},
	{ MACH_TYPE_IPQ5424_RDP464_C2, "ipq5424-rdp464-c2"},
	{ MACH_TYPE_IPQ5424_RDP464_C3, "ipq5424-rdp464-c3"},
	{ MACH_TYPE_IPQ5424_RDP508, "ipq5424-rdp508"},
	{ MACH_TYPE_IPQ5424_RDP466, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP466_C2, "ipq5424-rdp466-c2"},
	{ MACH_TYPE_IPQ5424_RDP466_C3, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP485, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP485_C2, "ipq5424-rdp485-c2"},
	{ MACH_TYPE_IPQ5424_RDP485_C3, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP485_C4, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP487, "ipq5424-rdp487"},
	{ MACH_TYPE_IPQ5424_RDP487_C2, "ipq5424-rdp487"},
	{ MACH_TYPE_IPQ5424_RDP487_C3, "ipq5424-rdp487-c3"},
	{ MACH_TYPE_IPQ5424_RDP466_RFFE, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP496, "ipq5424-rdp466"},
	{ MACH_TYPE_IPQ5424_RDP485_RFFE, "ipq5424-rdp485"},
	{ MACH_TYPE_IPQ5424_RDP485_RFFE_C2, "ipq5424-rdp485-c2"},
	{ MACH_TYPE_IPQ5424_DB_MR01_1, "ipq5424-db-mr01.1"},
};

int machid_dts_nos = ARRAY_SIZE(machid_dts);

struct machid_dts_map * machid_dts_info = machid_dts;
int * machid_dts_entries = &machid_dts_nos;
#endif /* CONFIG_DTB_RESELECT */

static crashdump_infos_t dumpinfo_n[] = {
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

crashdump_infos_t *board_dumpinfo = dumpinfo_n;
uint8_t *board_dump_entries = &dump_entries_n;

void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V2);
#endif
	psci_sys_reset(SYSRESET_COLD);
	return;
}

int print_cpuinfo(void)
{
        return 0;
}

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

	return;
}
#endif

void lowlevel_init(void)
{
#ifdef CONFIG_IPQ_EARLY_WDT
	ipq_enable_non_sec_watchdog();
#endif
	return;
}

#ifndef CFG_EMULATION
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

	/*
	 * enable gpll0 aux clock
	 */
	reg_val = readl(GCC_GPLL0_USER_CTL);
	writel(reg_val | PLLOUT_LV_AUX_EN, GCC_GPLL0_USER_CTL);
}
#endif /* CFG_EMULATION */

int board_get_smem_target_info(ipq_smem_target_info_t *smem_tinfo_ptr)
{
	uint32_t tcsr_wonce0_val = readl(TCSR_TZ_WONCE0);
	uint32_t tcsr_wonce1_val = readl(TCSR_TZ_WONCE1);
	uint64_t ipq_smem_target_info_addr;
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

	fdt_find_and_setprop(blob, "/reserved-memory/smem@8a800000/",
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

	fdt_find_and_setprop(blob, "/reserved-memory/smem_region@8A800000",
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
		return;
	}
}

#ifdef CONFIG_ARM64
/*
 * Set XN (PTE_BLOCK_PXN | PTE_BLOCK_UXN)bit for all dram regions
 * and Peripheral block except uboot code region
 */
static struct mm_region ipq5424_mem_map[] = {
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
		 * added dummy 0x0UL,
		 * will update the actual DDR limit
		 */
		.virt = CONFIG_TEXT_BASE + CONFIG_TEXT_SIZE,
		.phys = CONFIG_TEXT_BASE + CONFIG_TEXT_SIZE,
		.size = 0x0UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
#if (CONFIG_NR_DRAM_BANKS > 1)
		.virt = CFG_SYS_SDRAM_BASE1_ADDR,
		.phys = CFG_SYS_SDRAM_BASE1_ADDR,
		.size = 0x0UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
#endif
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = ipq5424_mem_map;
#endif

int execute_dprv3(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
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
		IPQ_SCM_EXECUTE_DPR(param, loadaddr, filesize, 0, 0);
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

void ipq_fdt_fixup_sku_based_usb_config(void *blob)
{
	if (!(readl(USB_SOFTSKU_STATUS) & USB_SOFTSKU_STATUS_DISABLE))
		return;

	parse_fdt_fixup("/soc@0/phy@7b000/%phandle%0xe0", blob);
	parse_fdt_fixup("/soc@0/usb3@8a00000/dwc3@8a00000/%phys%0xe0", blob);
	parse_fdt_fixup("/soc@0/usb3@8a00000/dwc3@8a00000/%phy-names%?usb2-phy", blob);
	parse_fdt_fixup("/soc@0/usb3@8a00000/%qcom,select-utmi-as-pipe-clk%1", blob);
}

int ipq_uboot_uart_fdt_fixup(uint32_t machid)
{
	const char uart0[50] = "/soc@0/geniqup@1ac0000/serial@1a80000";

	if (machid == MACH_TYPE_IPQ5424_EMU_FBC ||
		machid == MACH_TYPE_IPQ5424_EMU)
		fdt_find_and_setprop((void *)gd->fdt_blob, "/aliases/",
				"console", uart0, strlen(uart0) + 1, 1);

	return 0;
}

void ipq_uboot_fdt_fixup(uint32_t machid)
{
	int ret, len = 0, config_nos = 0;
	char config[CONFIG_NAME_MAX_LEN];
	char *config_list[6] = { NULL };

	switch (machid) {
		case MACH_TYPE_IPQ5424_RDP466_RFFE:
			config_list[config_nos++] = "config-rdp466-rffe";
			break;
		case MACH_TYPE_IPQ5424_RDP485_RFFE:
			config_list[config_nos++] = "config-rdp485-rffe";
			break;
		case MACH_TYPE_IPQ5424_RDP496:
			config_list[config_nos++] = "config-rdp496";
			break;
		case MACH_TYPE_IPQ5424_RDP485_RFFE_C2:
			config_list[config_nos++] = "config-rdp485-rffe-c2";
			break;
		case MACH_TYPE_IPQ5424_RDP466_C3:
			config_list[config_nos++] = "config-rdp466-c3";
			break;
		case MACH_TYPE_IPQ5424_RDP485_C3:
			config_list[config_nos++] = "config-rdp485-c3";
			break;
		case MACH_TYPE_IPQ5424_RDP485_C4:
			config_list[config_nos++] = "config-rdp485-c4";
			break;
		case MACH_TYPE_IPQ5424_RDP487_C2:
			config_list[config_nos++] = "config-rdp487-c2";
			break;
	}

	if (config_nos) {
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

bool is_atf_enbled(void)
{
	uint32_t *atf_status = smem_get_item(SMEM_ATF_ENABLE);
	if (IS_ERR_OR_NULL(atf_status)) {
		printf("Failed to get SMEM item: SMEM_ATF_ENABLE\n");
		return 0;
	}

	return (*atf_status ? true : false);
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
	return;
}

#ifdef CONFIG_SDX_ATTACH_SUPPORT
void ipq_board_power_cycle_sdx(void)
{
	/*
	 * sdx reset during crashdump path
	 */
	struct udevice *dev = NULL;
	struct gpio_desc pwr_gpio;
	struct gpio_desc rst_gpio;
	struct gpio_desc e911_gpio;
	int err;

	uclass_get_device_by_driver(UCLASS_NOP, DM_DRIVER_GET(gpio), &dev);

	if (dev == NULL) {
		return;
	}

	err = gpio_request_by_name_nodev(dev_ofnode(dev), "e911_gpio", 0,
					 &e911_gpio, GPIOD_IS_IN);
	if (err) {
		printf("%s: sdx e911_gpio not found in DT!\n", __func__);
	} else if (!err && dm_gpio_get_value(&e911_gpio)) {
		printf("SDX on e911 call, skipping sdx reset\n");
		return;
	}

	err = gpio_request_by_name_nodev(dev_ofnode(dev), "power_gpio", 0,
					 &pwr_gpio, GPIOD_IS_OUT);
	if (err) {
		printf("%s: sdx power_gpio not found in DT!,\n", __func__);
		return;
	}

	err = gpio_request_by_name_nodev(dev_ofnode(dev), "reset_gpio", 0,
					 &rst_gpio, GPIOD_IS_OUT);
	if (err) {
		printf("%s: sdx reset_gpio not found in DT!\n", __func__);
		return;
	}

	dm_gpio_set_value(&pwr_gpio, 1);
	dm_gpio_set_value(&rst_gpio, 1);
	mdelay(100);
	dm_gpio_set_value(&pwr_gpio, 0);
	dm_gpio_set_value(&rst_gpio, 0);
}
#endif

#ifdef CONFIG_EARLY_CLOCK_ENABLE
void board_early_clock_enable(void) {
	/* Enable the IM_SLEEP clock */
	writel((readl(GCC_BASE + GCC_IM_SLEEP_CBCR) | 0x1),
		GCC_BASE + GCC_IM_SLEEP_CBCR);
}
#endif

int read_bootconfig(void)
{
	int ret = 0;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();

	sfi->ipq_smem_bootconfig_info =
		(ipq_smem_bootconfig_info_t *)malloc(
				sizeof(ipq_smem_bootconfig_info_t));

	if(sfi->ipq_smem_bootconfig_info == NULL) {
		printf("No Enough Memory\n");
		return 1;
	}

	ret = get_partition_data("0:BOOTCONFIG", 0,
			( uint8_t*)sfi->ipq_smem_bootconfig_info,
			sizeof(ipq_smem_bootconfig_info_t), sfi->flash_type);

	if (ret < 0)
		return !!ret;

	if(!is_valid_bootconfig(sfi->ipq_smem_bootconfig_info)) {
		printf("Invalid Bootconfig\n");
		sfi->ipq_smem_bootconfig_info = NULL;
		ret = 0;
	} else {
		ret = 0;
	}

	return ret;
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

int do_edl_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	puts ("resetting to EDL...\n");
	flush();

#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V2);
#endif
	psci_sys_reset(SYSRESET_EDL);

	/*NOTREACHED*/
	return 0;
}
U_BOOT_CMD(edl_reset, 1, 0, do_edl_reset,
	   "Reset to Emergency download mode\n", "");
