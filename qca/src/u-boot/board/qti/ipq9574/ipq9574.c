// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <asm/cache.h>
#include <configs/ipq9574.h>
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

#define CMN_BLK_ADDR			0x0009B780
#define FREQUENCY_MASK			0xfffffdf0
#define INTERNAL_48MHZ_CLOCK		0x7
#define PORT_WRAPPER_MAX		0xFF
#define CONFIG_NAME_MAX_LEN		128

#define TIMEOUT_MS			30000
#define CLK_SRC				32000
#define WDT_ENABLE_REG			0xb017008
#define WDT_RST_REG			0xb017004
#define WDT_BARK_TIME_REG		0xb017010
#define WDT_BITE_TIME_REG		0xb017014

DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq9574-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info * fnodes = ipq_fnodes ;
int * fnode_entires = &ipq_fnode_entires;
#endif

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map machid_dts[] = {
	{ MACH_TYPE_IPQ9574_RDP417, "ipq9574-rdp417"},
	{ MACH_TYPE_IPQ9574_RDP418, "ipq9574-rdp418"},
	{ MACH_TYPE_IPQ9574_RDP418_EMMC, "ipq9574-rdp418"},
	{ MACH_TYPE_IPQ9574_RDP437, "ipq9574-rdp437"},
	{ MACH_TYPE_IPQ9574_RDP433, "ipq9574-rdp433"},
	{ MACH_TYPE_IPQ9574_RDP433_SFP, "ipq9574-rdp433-sfp"},
	{ MACH_TYPE_IPQ9574_RDP449, "ipq9574-rdp449" },
	{ MACH_TYPE_IPQ9574_RDP433_MHT_PHY, "ipq9574-rdp433-mht-phy"},
	{ MACH_TYPE_IPQ9574_RDP453, "ipq9574-rdp453"},
	{ MACH_TYPE_IPQ9574_RDP454, "ipq9574-rdp454"},
	{ MACH_TYPE_IPQ9574_RDP433_MHT_SWT, "ipq9574-rdp433-mht-switch"},
	{ MACH_TYPE_IPQ9574_RDP467, "ipq9574-rdp467" },
	{ MACH_TYPE_IPQ9574_RDP455_C11, "ipq9574-rdp455"},
	{ MACH_TYPE_IPQ9574_RDP455_C12, "ipq9574-rdp455"},
	{ MACH_TYPE_IPQ9574_RDP459, "ipq9574-rdp459"},
	{ MACH_TYPE_IPQ9574_RDP457, "ipq9574-rdp457" },
	{ MACH_TYPE_IPQ9574_RDP456, "ipq9574-rdp456" },
	{ MACH_TYPE_IPQ9574_RDP458, "ipq9574-rdp458" },
	{ MACH_TYPE_IPQ9574_RDP469, "ipq9574-rdp469"},
	{ MACH_TYPE_IPQ9574_RDP461, "ipq9574-rdp461"},
	{ MACH_TYPE_IPQ9574_RDP475, "ipq9574-rdp475"},
	{ MACH_TYPE_IPQ9574_RDP475_QCA81XX, "ipq9574-rdp475-qca81xx"},
	{ MACH_TYPE_IPQ9574_RDP475_QCA81XX_I2C, "ipq9574-rdp475-qca81xx-i2c"},
	{ MACH_TYPE_IPQ9574_RDP476, "ipq9574-rdp476"},
	{ MACH_TYPE_IPQ9574_DB_AL01_C1, "ipq9574-db-al01-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL01_C2, "ipq9574-db-al01-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL01_C3, "ipq9574-db-al01-c3"},
	{ MACH_TYPE_IPQ9574_DB_AL02_C1, "ipq9574-db-al02-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL02_C2, "ipq9574-db-al02-c1"},
	{ MACH_TYPE_IPQ9574_DB_AL02_C3, "ipq9574-db-al02-c3"},
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
		.name = "CODERAM.BIN",
		.start_addr = 0x00200000,
		.size = 0x00028000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "DATARAM.BIN",
		.start_addr = 0x00290000,
		.size = 0x00014000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = false,
		.compression_support = false,
		.dumptoflash_support = false
	},
	{
		.name = "MSGRAM.BIN",
		.start_addr = 0x00060000,
		.size = 0x00006000,
		.dump_level = FULLDUMP,
		.split_bin_sz = 0,
		.is_aligned_access = true,
		.compression_support = false,
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

void ipq_fdt_serial_fixup(void *blob)
{
	int serial_nodeoff = -EINVAL;
	uint32_t flash_type = gd->board_type & FLASH_TYPE_MASK;

#ifdef LINUX_6_x_SERIAL2_DTS_NODE
	serial_nodeoff = fdt_path_offset(blob, LINUX_6_x_SERIAL2_DTS_NODE);
#endif

	if (flash_type == SMEM_BOOT_MMC_FLASH) {
		if (serial_nodeoff > 0) {

#ifdef LINUX_6_x_SERIAL2_DTS_NODE
			parse_fdt_fixup(LINUX_6_x_SERIAL2_DTS_NODE"%"\
					STATUS_DISABLED,blob);
#endif
		}
	}

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

void ipq_fdt_board_model_fixup(void *blob)
{
	int node_offset;
	char *attr_name = "model";
	char *attr_value;
	int len;
	char *c1_pos = NULL;

	node_offset = fdt_path_offset(blob, "/");
	attr_value = (char *)fdt_getprop(blob, node_offset, attr_name, &len);
	if (!attr_value) {
		return;
	}

	c1_pos = strstr(attr_value, "AL02-C1");
	if (c1_pos) {
		c1_pos[6] = '2';
	}

	return;
}

void ipq_fdt_fixup_board(void *blob)
{
	unsigned long machid = gd->bd->bi_arch_number;
	int secure_boot = image_auth_check();

	switch (machid) {
	case MACH_TYPE_IPQ9574_RDP418_EMMC:
		ipq_fdt_serial_fixup(blob);
		ipq_fdt_board_model_fixup(blob);
		break;
	}

	if(secure_boot && check_rootfs_authentication())
		ipq_fdt_rootfs_auth_fixup(blob);

	return;
}

void ipq_uboot_fdt_fixup(uint32_t machid)
{
	int ret, len = 0, config_nos = 0;
	char config[CONFIG_NAME_MAX_LEN];
	char *config_list[6] = { NULL };

	switch (machid)
	{
		case MACH_TYPE_IPQ9574_DB_AL01_C2:
			config_list[config_nos++] = "config@db-al01-c2";
			config_list[config_nos++] = "config-db-al01-c2";
			break;
		case MACH_TYPE_IPQ9574_DB_AL02_C2:
			config_list[config_nos++] = "config@db-al02-c2";
			config_list[config_nos++] = "config-db-al02-c2";
			break;
		case MACH_TYPE_IPQ9574_RDP455_C11:
			config_list[config_nos++] = "config-al02-c11";
			config_list[config_nos++] = "config@al02-c11";
			config_list[config_nos++] = "config@rdp455-c11";
			config_list[config_nos++] = "config-rdp455-c11";
			break;
		case MACH_TYPE_IPQ9574_RDP455_C12:
			config_list[config_nos++] = "config-al02-c12";
			config_list[config_nos++] = "config@al02-c12";
			config_list[config_nos++] = "config@rdp455-c12";
			config_list[config_nos++] = "config-rdp455-c12";
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
	reg_val = readl(CMN_BLK_ADDR + 4);
	reg_val = (reg_val & FREQUENCY_MASK) | INTERNAL_48MHZ_CLOCK;
	writel(reg_val, CMN_BLK_ADDR + 0x4);
	reg_val = readl(CMN_BLK_ADDR);
	reg_val = reg_val | 0x40;
	writel(reg_val, CMN_BLK_ADDR);
	mdelay(1);
	reg_val = reg_val & (~0x40);
	writel(reg_val, CMN_BLK_ADDR);
	mdelay(1);
	writel(0xbf, CMN_BLK_ADDR);
	mdelay(1);
	writel(0xff, CMN_BLK_ADDR);
	mdelay(1);
}

uint32_t get_soc_hw_version(void)
{
        return 0;
}

#ifdef CONFIG_ARM64
/*
 * Set XN (PTE_BLOCK_PXN | PTE_BLOCK_UXN)bit for all dram regions
 * and Peripheral block except uboot code region
 */
static struct mm_region ipq9574_mem_map[] = {
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

struct mm_region *mem_map = ipq9574_mem_map;
#endif
int execute_dprv1(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned long loadaddr;
	unsigned long default_hex_val = 0xFFFFFFFF;
	uint32_t dpr_status = 0;
	scm_param param;

	memset(&param, 0, sizeof(scm_param));
	if (argc > cmdtp->maxargs)
		goto fail;

	if (argc == cmdtp->maxargs)
		loadaddr = simple_strtoul(argv[1], NULL, 16);
	else {
		loadaddr = env_get_hex("fileaddr", default_hex_val);
		if (loadaddr == default_hex_val)
			goto fail;
	}

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_EXECUTE_DPR(param, loadaddr);
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
				if(ret == 0 && (param.res.result[0] & 0x80))
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

void ipq_fdt_fixup_atf(void *blob)
{
	if (!(gd->board_type & ATF_ENABLED))
		return;

#ifdef LINUX_5_4_CRYPTO_BAM_NODE
	if (fdt_path_offset(blob, LINUX_5_4_CRYPTO_BAM_NODE) > 0) {
#ifdef LINUX_5_4_CRYPTO_BAM_PIPE_TRUST_FIXUP
		parse_fdt_fixup(LINUX_5_4_CRYPTO_BAM_PIPE_TRUST_FIXUP, blob);
#endif
#ifdef LINUX_5_4_CRYPTO_BAM_CTRL_REMOTE_FIXUP
		parse_fdt_fixup(LINUX_5_4_CRYPTO_BAM_CTRL_REMOTE_FIXUP, blob);
#endif
	}
#endif

#ifdef LINUX_6_x_CRYPTO_BAM_NODE
	if (fdt_path_offset(blob, LINUX_6_x_CRYPTO_BAM_NODE) > 0) {
#ifdef LINUX_6_x_CRYPTO_BAM_PIPE_TRUST_FIXUP
		parse_fdt_fixup(LINUX_6_x_CRYPTO_BAM_PIPE_TRUST_FIXUP, blob);
#endif
#ifdef LINUX_6_x_CRYPTO_BAM_CTRL_REMOTE_FIXUP
		parse_fdt_fixup(LINUX_6_x_CRYPTO_BAM_CTRL_REMOTE_FIXUP, blob);
#endif
	}
#endif
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

	if (dev == NULL)
		return;

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

uint32_t image_auth_check(void) {

	uint32_t board_type = gd->board_type;
	uint32_t ret = -1;

	switch (gd->ram_size) {
	case SZ_128M:
		ret = 0;
		break;
	default:
		ret = (board_type & SECURE_BOARD) &&
			!(board_type & ATF_ENABLED);
		break;
	}

	return ret;
}
