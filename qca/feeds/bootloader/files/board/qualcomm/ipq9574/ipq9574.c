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

/* MACH IDs for various RDPs */
#define MACH_TYPE_IPQ9574_RDP417		0x8050000
#define MACH_TYPE_IPQ9574_RDP418		0x8050001
#define MACH_TYPE_IPQ9574_RDP418_EMMC		0x8050101
#define MACH_TYPE_IPQ9574_RDP437		0x8050201
#define MACH_TYPE_IPQ9574_RDP433		0x8050301
#define MACH_TYPE_IPQ9574_RDP449		0x8050501
#define MACH_TYPE_IPQ9574_RDP433_MHT_PHY	0x8050601
#define MACH_TYPE_IPQ9574_RDP453		0x8050701
#define MACH_TYPE_IPQ9574_RDP453_QCE1204	0x8050702
#define MACH_TYPE_IPQ9574_RDP454		0x8050801
#define MACH_TYPE_IPQ9574_RDP433_MHT_SWT	0x8050901
#define MACH_TYPE_IPQ9574_RDP467		0x8051301
#define MACH_TYPE_IPQ9574_RDP455_C11		0x8050A01
#define MACH_TYPE_IPQ9574_RDP455_C12		0x8050B01
#define MACH_TYPE_IPQ9574_RDP459		0x8050C01
#define MACH_TYPE_IPQ9574_RDP457		0x8050E01
#define MACH_TYPE_IPQ9574_RDP456		0x8050F01
#define MACH_TYPE_IPQ9574_RDP458		0x8050102
#define MACH_TYPE_IPQ9574_RDP469		0x8051001
#define MACH_TYPE_IPQ9574_RDP461		0x8051201
#define MACH_TYPE_IPQ9574_RDP475		0x8050003
#define MACH_TYPE_IPQ9574_RDP475_QCA81XX	0x8050103
#define MACH_TYPE_IPQ9574_RDP475_QCA81XX_I2C	0x8050203
#define MACH_TYPE_IPQ9574_RDP475_QCE1204	0x8050303
#define MACH_TYPE_IPQ9574_RDP476		0x8050004
#define MACH_TYPE_IPQ9574_DB_AL01_C1		0x1050000
#define MACH_TYPE_IPQ9574_DB_AL01_C2		0x1050100
#define MACH_TYPE_IPQ9574_DB_AL01_C3		0x1050200
#define MACH_TYPE_IPQ9574_DB_AL02_C1		0x1050001
#define MACH_TYPE_IPQ9574_DB_AL02_C2		0x1050101
#define MACH_TYPE_IPQ9574_DB_AL02_C3		0x1050201
#define MACH_TYPE_IPQ9574_RDP433_SFP		0x8051101

#define TIMEOUT_MS				30000
#define CLK_SRC					32000
#define WDT_ENABLE_REG				0xb017008
#define WDT_RST_REG				0xb017004
#define WDT_BARK_TIME_REG			0xb017010
#define WDT_BITE_TIME_REG			0xb017014

#define LINUX_6_x_SERIAL2_DTS_NODE		"/soc@0/serial@78b2000/"
#define STATUS_DISABLED				"status%?disabled"
#define LINUX_6_x_ROOTFS_AUTH_DTS_NODE		"/soc@0/qfprom"
#define LINUX_6_x_ROOTFS_AUTH_FIXUP	"/soc@0/qfprom/%rootfs_auth_enable%1"
#define LINUX_5_4_CRYPTO_BAM_NODE		"/soc/dma@704000"
#define LINUX_5_4_CRYPTO_BAM_PIPE_TRUST_FIXUP	"/soc/dma@704000%qti,config-pipe-trust-reg%2"
#define LINUX_5_4_CRYPTO_BAM_CTRL_REMOTE_FIXUP	"/soc/dma@704000%qcom,controlled-remotely%0"
#define LINUX_6_x_CRYPTO_BAM_NODE		"/soc@0/dma@704000"
#define LINUX_6_x_CRYPTO_BAM_PIPE_TRUST_FIXUP	"/soc@0/dma@704000/%qti,config-pipe-trust-reg%2"
#define LINUX_6_x_CRYPTO_BAM_CTRL_REMOTE_FIXUP	"/soc@0/dma@704000%delete%qcom,controlled-remotely"

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq9574-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info *fnodes = ipq_fnodes ;
int *fnode_entires = &ipq_fnode_entires;
#endif

struct dts_fixup ipq9574_mmc_fixup[] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc/nand@79b0000/", {"/soc/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc/sdhci@7804000/", {"/soc/sdhci@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq9574_mmc_fixup;

struct dts_fixup ipq9574_usb_fixup[] = {
	{ "/soc@0/usb3@8a00000/dwc3@8a00000/",
		{"/soc@0/usb3@8a00000/dwc3@8a00000%dr_mode%?peripheral",
		"/soc@0/usb3@8a00000/dwc3@8a00000%maximum-speed%?high-speed"},
		2},
	{ "/soc/usb3@8A00000/dwc3@8A00000/",
		{ "/soc/usb3@8A00000/dwc3@8A00000%dr_mode%?peripheral",
		"/soc/usb3@8A00000/dwc3@8A00000%maximum-speed%?high-speed"},
		2},
	{NULL}
};

struct dts_fixup *usb_fixup = ipq9574_usb_fixup;

struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ9574_RDP417,
		"ipq9574-rdp417",
		"rdp417",
		"al01-c1"
	},
	{
		MACH_TYPE_IPQ9574_RDP418,
		"ipq9574-rdp418",
		"rdp418",
		"al02-c1"
	},
	{
		MACH_TYPE_IPQ9574_RDP418_EMMC,
		"ipq9574-rdp418",
		"rdp418",
		"al02-c2"
	},
	{
		MACH_TYPE_IPQ9574_RDP437,
		"ipq9574-rdp437",
		"rdp437",
		"al02-c3"
	},
	{
		MACH_TYPE_IPQ9574_RDP433,
		"ipq9574-rdp433",
		"rdp433",
		"al02-c4"
	},
	{
		MACH_TYPE_IPQ9574_RDP433_SFP,
		"ipq9574-rdp433-sfp",
		"rdp433-sfp",
		"al02-c18"
	},
	{
		MACH_TYPE_IPQ9574_RDP449,
		"ipq9574-rdp449",
		"rdp449",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_RDP433_MHT_PHY,
		"ipq9574-rdp433-mht-phy",
		"rdp433-mht-phy",
		"al02-c7"
	},
	{
		MACH_TYPE_IPQ9574_RDP453,
		"ipq9574-rdp453",
		"rdp453",
		"al02-c8"
	},
	{
		MACH_TYPE_IPQ9574_RDP453_QCE1204,
		"ipq9574-rdp453-qce1204",
		"rdp453-qce1204",
		"al02-c8-qce1204"
	},
	{
		MACH_TYPE_IPQ9574_RDP454,
		"ipq9574-rdp454",
		"rdp454",
		"al02-c9"
	},
	{
		MACH_TYPE_IPQ9574_RDP433_MHT_SWT,
		"ipq9574-rdp433-mht-switch",
		"rdp433-mht-switch",
		"al02-c10"
	},
	{
		MACH_TYPE_IPQ9574_RDP467,
		"ipq9574-rdp467",
		"rdp467",
		"al02-c20"
	},
	{
		MACH_TYPE_IPQ9574_RDP455_C11,
		"ipq9574-rdp455",
		"rdp455-c11",
		"al02-c11"
	},
	{
		MACH_TYPE_IPQ9574_RDP455_C12,
		"ipq9574-rdp455",
		"rdp455-c12",
		"al02-c12"
	},
	{
		MACH_TYPE_IPQ9574_RDP459,
		"ipq9574-rdp459",
		"rdp459",
		"al02-c13"
	},
	{
		MACH_TYPE_IPQ9574_RDP457,
		"ipq9574-rdp457",
		"rdp457",
		"al02-c15"
	},
	{
		MACH_TYPE_IPQ9574_RDP456,
		"ipq9574-rdp456",
		"rdp456",
		"al02-c16"
	},
	{
		MACH_TYPE_IPQ9574_RDP458,
		"ipq9574-rdp458",
		"rdp458",
		"al03-c2"
	},
	{
		MACH_TYPE_IPQ9574_RDP469,
		"ipq9574-rdp469",
		"rdp469",
		"al02-c17"
	},
	{
		MACH_TYPE_IPQ9574_RDP461,
		"ipq9574-rdp461",
		"rdp461",
		"al02-c19"
	},
	{
		MACH_TYPE_IPQ9574_RDP475,
		"ipq9574-rdp475",
		"rdp475",
		"al05"
	},
	{
		MACH_TYPE_IPQ9574_RDP475_QCA81XX,
		"ipq9574-rdp475-qca81xx",
		"rdp475-qca81xx",
		"al05-qca81xx"
	},
	{
		MACH_TYPE_IPQ9574_RDP475_QCA81XX_I2C,
		"ipq9574-rdp475-qca81xx-i2c",
		"rdp475-qca81xx-i2c",
		"al05-qca81xx-i2c"
	},
	{
		MACH_TYPE_IPQ9574_RDP475_QCE1204,
		"ipq9574-rdp475-qce1204",
		"rdp475-qce1204",
		"al05-qce1204"
	},
	{
		MACH_TYPE_IPQ9574_RDP476,
		"ipq9574-rdp476",
		"rdp476",
		"al06"
	},
	{
		MACH_TYPE_IPQ9574_DB_AL01_C1,
		"ipq9574-db-al01-c1",
		"db-al01-c1",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL01_C2,
		"ipq9574-db-al01-c1",
		"db-al01-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL01_C3,
		"ipq9574-db-al01-c3",
		"db-al01-c3",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL02_C1,
		"ipq9574-db-al02-c1",
		"db-al02-c1",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL02_C2,
		"ipq9574-db-al02-c1",
		"db-al02-c2",
		NULL
	},
	{
		MACH_TYPE_IPQ9574_DB_AL02_C3,
		"ipq9574-db-al02-c3",
		"db-al02-c3",
		NULL
	},
};

struct multidtb_config ipq9574_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq9574_dtb_info;

static struct crashdump_infos dumpinfo_n[] = {
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

struct crashdump_infos *board_dumpinfo = dumpinfo_n;

uint8_t *board_dump_entries = &dump_entries_n;

void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V1);
#endif
	psci_sys_reset(SYSRESET_COLD);
	return;
}

void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ9574_RDP455_C11:
		strlcpy(dtb->dts_name, "ipq9574-rdp455-c11",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ9574_RDP455_C12:
		strlcpy(dtb->dts_name, "ipq9574-rdp455-c12",
			BOARD_DTS_MAX_NAMELEN);
	break;
	default:
		strlcpy(dtb->dts_name, dtb->dts_base, BOARD_DTS_MAX_NAMELEN);
	}

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
}
#endif

void lowlevel_init(void)
{
#ifdef CONFIG_IPQ_EARLY_WDT
	ipq_enable_non_sec_watchdog();
#endif
}

uint32_t ipq_get_soc_hw_version(void)
{
	return 0;
}

#if defined(CONFIG_SCM)
bool is_atf_enbled(void)
{
	enum atf_status_t {
		ATF_STATE_DISABLED,
		ATF_STATE_ENABLED,
		ATF_STATE_UNKNOWN,
	} atf_status = ATF_STATE_UNKNOWN;
	struct scm_param param;
	int ret = -1;

	if (likely(atf_status != ATF_STATE_UNKNOWN))
		return (atf_status == ATF_STATE_ENABLED);

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_CHECK_SCM_SUPPORT(param, SCM_SIP_FNID(QCOM_SCM_SVC_INFO,
						QCOM_GET_SECURE_STATE_CMD));
		param.get_ret = true;
		ret = ipq_scm_call(&param);

		if (!ret && (le32_to_cpu(param.res.result[0]) > 0)) {
			do {
				ret = -ENOTSUPP;
				check_atf_support(param);
				param.get_ret = true;

				ret = ipq_scm_call(&param);
				if (ret == 0 && (param.res.result[0] & 0x80))
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

int execute_dprv1(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned long loadaddr;
	unsigned long default_hex_val = 0xFFFFFFFF;
	uint32_t dpr_status = 0;
	struct scm_param param;

	memset(&param, 0, sizeof(struct scm_param));
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
#endif

void ipq_fdt_serial_fixup(void *blob)
{
	int serial_nodeoff = -EINVAL;
	uint32_t flash_type = gd->board_type & FLASH_TYPE_MASK;

	if (flash_type != SMEM_BOOT_MMC_FLASH)
		return;

#ifdef LINUX_6_x_SERIAL2_DTS_NODE
	serial_nodeoff = fdt_path_offset(blob, LINUX_6_x_SERIAL2_DTS_NODE);
#endif

	if (serial_nodeoff > 0) {
#ifdef LINUX_6_x_SERIAL2_DTS_NODE
		parse_fdt_fixup(LINUX_6_x_SERIAL2_DTS_NODE"%"\
					STATUS_DISABLED,blob);
#endif
		}
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

	if (!attr_value)
		return;

	c1_pos = strstr(attr_value, "AL02-C1");
	if (c1_pos)
		c1_pos[6] = '2';
}

void ipq_fdt_fixup_board(void *blob)
{
	switch (gd->bd->bi_arch_number) {
	case MACH_TYPE_IPQ9574_RDP418_EMMC:
		ipq_fdt_serial_fixup(blob);
		ipq_fdt_board_model_fixup(blob);
		break;
	default:
		break;
	}

	if (is_board_support_image_auth() && ipq_check_rootfs_authentication())
		ipq_fdt_rootfs_auth_fixup(blob);
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

#ifdef CONFIG_BOOT_BANK_FIXUP
int ipq_uboot_fdt_fixup_booted_bank(void *blob)
{
	return fdt_set_booted_bank_property(blob);
}
#endif

int ipq_uboot_fdt_fixup(void *blob, enum fixup_type type)
{
	switch(type) {
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

uint32_t is_board_support_image_auth(void)
{
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
