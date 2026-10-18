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
#define MACH_TYPE_IPQ5332_RDP468		0x8060000
#define MACH_TYPE_IPQ5332_RDP441		0x8060001
#define MACH_TYPE_IPQ5332_RDP441_QCA81XX	0x8060301
#define MACH_TYPE_IPQ5332_RDP441_QCA81XX_I2C	0x8060401
#define MACH_TYPE_IPQ5332_RDP441_QCE2204	0x8060501
#define MACH_TYPE_IPQ5332_RDP442		0x8060002
#define MACH_TYPE_IPQ5332_RDP446		0x8060004
#define MACH_TYPE_IPQ5332_RDP474		0x8060006
#define MACH_TYPE_IPQ5332_RDP472		0x8060101
#define MACH_TYPE_IPQ5332_RDP473		0x8060008
#define MACH_TYPE_IPQ5332_RDP477		0x8060102
#define MACH_TYPE_IPQ5332_RDP477_256M		0x8060602
#define MACH_TYPE_IPQ5332_RDP478		0x8060007
#define MACH_TYPE_IPQ5332_RDP478_256M		0x8060207
#define MACH_TYPE_IPQ5332_RDP479		0x8060202
#define MACH_TYPE_IPQ5332_RDP480		0x8060402
#define MACH_TYPE_IPQ5332_RDP481		0x8060302
#define MACH_TYPE_IPQ5332_RDP483		0x8060107
#define MACH_TYPE_IPQ5332_RDP484		0x8060201
#define MACH_TYPE_IPQ5332_RDP486		0x8060502
#define MACH_TYPE_IPQ5332_RDP494		0x8060307
#define MACH_TYPE_IPQ5332_RDP509		0x8060702
#define MACH_TYPE_IPQ5332_DB_MI01_1		0x1060001
#define MACH_TYPE_IPQ5332_DB_MI02_1		0x1060003
#define MACH_TYPE_IPQ5332_DB_MI03_1		0x1060002
#define MACH_TYPE_IPQ5332_TB_MI03_1		0x1060102
#define MACH_TYPE_IPQ5332_TB_MI05_1		0x1060007

#define TIMEOUT_MS				30000
#define CLK_SRC					32000
#define WDT_ENABLE_REG				0xb017008
#define WDT_RST_REG				0xb017004
#define WDT_BARK_TIME_REG			0xb017010
#define WDT_BITE_TIME_REG			0xb017014

#define LINUX_6_x_ROOTFS_AUTH_DTS_NODE		"/soc@0/qfprom"
#define LINUX_6_x_ROOTFS_AUTH_FIXUP	"/soc@0/qfprom/%rootfs_auth_enable%1"


#define PHYA0_RFA_RFA_RFA_OTP_OTP_XO_0		0xC5D44AC
#define PHYA0_RFA_RFA_RFA_OTP_OTP_OV_1		0xC5D4484
#define QFPROM_RAW_FEATURE_CONFIG_ROW0_LSB	0xA0018

/*
 * TCSR Registers
 */
#define TCSR_TZ_WONCE0				0x193D000
#define TCSR_TZ_WONCE1				0x193D004

/*
 * TME DUMP
 */

#define TME_LOG_DUMP_FEATURE_ID			0x7
#define TME_LOG_DUMP_FEATURE_VERSION		0x401000

#if CONFIG_FDT_FIXUP_PARTITIONS
struct node_info ipq_fnodes[] = {
	{ "n25q128a11", MTD_DEV_TYPE_NOR},
	{ "micron,n25q128a11", MTD_DEV_TYPE_NOR},
	{ "qcom,ipq5332-nand", MTD_DEV_TYPE_NAND},
};

int ipq_fnode_entires = ARRAY_SIZE(ipq_fnodes);

struct node_info *fnodes = ipq_fnodes ;
int *fnode_entires = &ipq_fnode_entires;
#endif

extern uint32_t g_recovery_path;

struct dts_fixup ipq5332_mmc_fixup[] = {
	{ "/soc@0/nand@79b0000/", {"/soc@0/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc@0/mmc@7804000/", {"/soc@0/mmc@7804000/%status%?okay"}, 1},
	{ "/soc/nand@79b0000/", {"/soc/nand@79b0000/%status%?disabled"}, 1},
	{ "/soc/sdhci@7804000/", {"/soc/sdhci@7804000/%status%?okay"}, 1},
	{NULL}
};

struct dts_fixup *mmc_fixup = ipq5332_mmc_fixup;

struct dts_fixup ipq5332_usb_fixup[] = {
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

struct dts_fixup *usb_fixup = ipq5332_usb_fixup;

struct machid_dts_map machid_dts[] = {
	{
		MACH_TYPE_IPQ5332_RDP468,
		"ipq5332-rdp468",
		"rdp468",
		"mi01.6"
	},
	{
		MACH_TYPE_IPQ5332_RDP441,
		"ipq5332-rdp441",
		"rdp441",
		"mi01.2"
	},
	{
		MACH_TYPE_IPQ5332_RDP441_QCA81XX,
		"ipq5332-rdp441-qca81xx",
		"rdp441-qca81xx",
		"mi01.2-qca81xx"
	},
	{
		MACH_TYPE_IPQ5332_RDP441_QCA81XX_I2C,
		"ipq5332-rdp441-qca81xx-i2c",
		"rdp441-qca81xx-i2c",
		"mi01.2-qca81xx-i2c"
	},
	{
		MACH_TYPE_IPQ5332_RDP441_QCE2204,
		"ipq5332-rdp441-qce2204",
		"rdp441-qce2204",
		"mi01.2-qce2204"
	},
	{
		MACH_TYPE_IPQ5332_RDP442,
		"ipq5332-rdp442",
		"rdp442",
		"mi01.3"
	},
	{
		MACH_TYPE_IPQ5332_RDP446,
		"ipq5332-rdp446",
		"rdp446",
		"mi04.1"
	},
	{
		MACH_TYPE_IPQ5332_RDP474,
		"ipq5332-rdp474",
		"rdp474",
		"mi01.9"
	},
	{
		MACH_TYPE_IPQ5332_RDP473,
		"ipq5332-rdp480",
		"rdp473",
		"mi01.7"
	},
	{
		MACH_TYPE_IPQ5332_RDP472,
		"ipq5332-rdp472",
		"rdp472",
		"mi01.2-qcn9160-c1"
	},
	{
		MACH_TYPE_IPQ5332_RDP477,
		"ipq5332-rdp477",
		"rdp477",
		"mi01.3-c4"
	},
	{
		MACH_TYPE_IPQ5332_RDP478,
		"ipq5332-rdp478",
		"rdp478",
		"mi04.1-c3"
	},
	{
		MACH_TYPE_IPQ5332_RDP479,
		"ipq5332-rdp479",
		"rdp479",
		"ap-rdp479"
	},
	{
		MACH_TYPE_IPQ5332_RDP480,
		"ipq5332-rdp480",
		"rdp480",
		"mi01.13"
	},
	{
		MACH_TYPE_IPQ5332_RDP481,
		"ipq5332-rdp481",
		"rdp481",
		"ap-rdp481"
	},
	{
		MACH_TYPE_IPQ5332_RDP483,
		"ipq5332-rdp483",
		"rdp483",
		"mi04.3"
	},
	{
		MACH_TYPE_IPQ5332_RDP484,
		"ipq5332-rdp484",
		"rdp484",
		"mi01.2-c2"
	},
	{
		MACH_TYPE_IPQ5332_RDP486,
		"ipq5332-rdp442",
		"rdp442",
		"mi01.3-c3"
	},
	{
		MACH_TYPE_IPQ5332_RDP477_256M,
		"ipq5332-rdp477-256m",
		"rdp477-256m",
		"mi01.3-c4"
	},
	{
		MACH_TYPE_IPQ5332_RDP478_256M,
		"ipq5332-rdp478-256m",
		"rdp478-256m",
		"mi04.1-c3"
	},
	{
		MACH_TYPE_IPQ5332_RDP494,
		"ipq5332-rdp494",
		"rdp494",
		"mi04.5"
	},
	{
		MACH_TYPE_IPQ5332_RDP509,
		"ipq5332-rdp509",
		"rdp509",
		"mi01.3-c5"
	},
	{
		MACH_TYPE_IPQ5332_DB_MI01_1,
		"ipq5332-db-mi01.1",
		"db-mi01.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_DB_MI02_1,
		"ipq5332-db-mi02.1",
		"db-mi02.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_DB_MI03_1,
		"ipq5332-db-mi03.1",
		"db-mi03.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_TB_MI03_1,
		"ipq5332-tb-mi03.1",
		"tb-mi03.1",
		NULL
	},
	{
		MACH_TYPE_IPQ5332_TB_MI05_1,
		"ipq5332-tb-mi05.1",
		"tb-mi05.1",
		NULL
	},
};

struct multidtb_config ipq5332_dtb_info = {
	.list = machid_dts,
	.ncount = ARRAY_SIZE(machid_dts),
	.index = 0,
};

struct multidtb_config *g_board_dtb_info = &ipq5332_dtb_info;

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

struct crashdump_infos *board_dumpinfo = dumpinfo_n;
uint8_t *board_dump_entries = &dump_entries_n;


void ipq_update_board_name(int machid, struct multidtb_config *dtb)
{
	switch(machid) {
	case MACH_TYPE_IPQ5332_RDP473:
		strlcpy(dtb->dts_name, "ipq5332-rdp473",
			BOARD_DTS_MAX_NAMELEN);
	break;
	case MACH_TYPE_IPQ5332_RDP486:
		strlcpy(dtb->dts_name, "ipq5332-rdp486",
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

int ipq_read_tcsr_boot_misc(void)
{
	u32 dmagic;
#ifdef CONFIG_SCM
	struct scm_param param;
	int feat_avail = 0;
	int ret;

	if (!(gd->board_type & RECOVERY_MODE)) {
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
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return 0;
		}
	}

	if (feat_avail == 0x401000) {
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
		} while (0);

		if (ret == -ENOTSUPP) {
			printf("Unsupported SCM call\n");
			return 0;
		}
	} else {
		dmagic = *(TCSR_BOOT_MISC_REG);
	}
#else
	dmagic = *(TCSR_BOOT_MISC_REG);
#endif

	return dmagic;
}

#ifdef CONFIG_SCM
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
				if ((ret == 0) && (param.res.result[0] & 0x80))
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
#endif

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

int execute_dprv2(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
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
		IPQ_SCM_EXECUTE_DPR(param, loadaddr, filesize);
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

void board_cache_init(void)
{
	icache_enable();
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
	/* Disable L2 as TCM in recovery mode */
	if (gd->board_type & RECOVERY_MODE)
		writel(0x08000000, 0xB110010);

	dcache_enable();
#endif
}

void reset_cpu(void)
{
#ifdef CONFIG_IPQ_CRASHDUMP
	reset_crashdump(RESET_V1);
#endif
	psci_sys_reset(SYSRESET_COLD);
}

void ipq_fdt_rootfs_auth_fixup(void *blob)
{
#ifdef LINUX_6_x_ROOTFS_AUTH_DTS_NODE
	if (fdt_path_offset(blob, LINUX_6_x_ROOTFS_AUTH_DTS_NODE) > 0)
		parse_fdt_fixup(LINUX_6_x_ROOTFS_AUTH_FIXUP, blob);
#endif
}

void ipq_fdt_fixup_board(void *blob)
{
	if(is_board_support_image_auth() && ipq_check_rootfs_authentication())
		ipq_fdt_rootfs_auth_fixup(blob);
}

int board_get_smem_target_info(struct ipq_smem_target_info *smem_tinfo_ptr)
{
	uint32_t tcsr_wonce0_val;
	uint32_t tcsr_wonce1_val;
	uint64_t ipq_smem_target_info_addr;
#ifdef CONFIG_SCM
	int feat_avail = 0;
	struct scm_param param;
	int ret;

	if (g_recovery_path == 1) {
		gd->board_type |= RECOVERY_MODE;
	} else {
		g_recovery_path = readl(CRASH_DUMP_ADDR_IMEM) & 0xFFFFFFFF;
		gd->board_type |= (g_recovery_path == MAGIC_RECOVERY_PATH) ?
				   RECOVERY_MODE : 0;
	}

	if (!(gd->board_type & RECOVERY_MODE))
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

	fdt_find_and_setprop(blob, "/reserved-memory/smem@4a800000/",
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

	fdt_find_and_setprop(blob, "/reserved-memory/smem_region@4a800000",
			"reg", reg, sizeof(reg), 0);
	return 0;
}

int ipq_uboot_fdt_fixup(void *blob, enum fixup_type type)
{
	switch(type) {
	case UBOOT_FIXUP_SMEM:
		ipq_uboot_fdt_fixup_smem(blob);
		break;
	default:
		break;
	}

	return 0;
}

void ipq_board_update_RFA_settings(void)
{
	int ret;
	int slotId = 0; /* Default slotId 0 */
	uint32_t reg_val;
	uint32_t calDataOffset;
	uint32_t calData;
	uint32_t CDACIN;
	uint32_t CDACOUT;
	struct scm_param param;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();

	/* Check for Q6 DISABLE bit 15 */
	if ((readl(QFPROM_RAW_FEATURE_CONFIG_ROW0_LSB) >> 15) & 0x1)
		return;

	calDataOffset  = (((slotId * 150) + 4) * 1024 + 0x66C4);
	ret = ipq_get_partition_data("0:ART", calDataOffset,
					(uint8_t*)&calData, 4,
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
	}
}

bool is_valid_dump(char *dump_name)
{
	bool skip_dump = false;
	int ret = -1;

	if(!strncmp("IMEM2.BIN", dump_name, 9))
	{
		struct scm_param param;

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

#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
bool is_version_rollback_support(void) {

	if (gd->board_type & ATF_ENABLED) {
		return false;
	}

	return true;
}
#endif
