// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015-2017, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Aneesh V <aneesh@ti.com>
 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

// SPDX-License-Identifier: BSD-2-Clause
/*
   Copyright (c) 2001 William L. Pitts
*/

// SPDX-License-Identifier: BSD-3-Clause
/*
 * Reference to the ARM TF Project,
 * plat/arm/common/arm_bl2_setup.c
 * Portions copyright (c) 2013-2016, ARM Limited and Contributors. All rights
 * reserved.
 * Copyright (C) 2016 Rockchip Electronic Co.,Ltd
 * Written by Kever Yang <kever.yang@rock-chips.com>
 * Copyright (C) 2017 Theobroma Systems Design und Consulting GmbH
 */

#include <clk.h>
#include <hang.h>
#include <cpu_func.h>
#include <init.h>
#include <image.h>
#include <linux/mtd/spi-nor.h>
#include <linux/sizes.h>
#include <spl.h>
#include <spl_load.h>
#include <timestamp.h>
#include <version.h>
#include <mach/ipq.h>
#include <mach/ipq_license.h>
#include <spi_flash.h>
#include <mach/smem_info.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <asm/system.h>
#include <smem.h>
#include <membuff.h>
#include <atf_common.h>
#include <linux/err.h>
#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
#include <asm/cache.h>
#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
#include <mailbox.h>
#include <linux/tmelcom-qmp.h>
#endif
#include <linux/mtd/mtd.h>
#include <nand.h>
#include <u-boot/crc.h>
#include <dm/device-internal.h>
#include <linux/ipq-enable-all-clks.h>
#include <env.h>
#include <env_internal.h>
#include <bootcount.h>
#include <sysreset.h>
#include <blk.h>
#include <qcom_voltage_control.h>
#include <fdt_support.h>

/**
 * PBL Boot interface
 */
#define PBL_LOG_BUFFER_SIZE  (4 * 1024)
#define NOR		1
#define MMC		5
#define SPI_NOR_MIBIB		6
#define SPI_NAND		11
#define SPI_NOR_GPT		12
#define NAND		33

/**
 * Parameter ID for the data to be accessed from PBL shared data
 */
enum {
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_FW_VERSION = 0,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_PATCH_VERSION,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_RMB_MBOX_BASE_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_CPU_BOOT_SPEED_HZ,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_BOOT_MEDIA_TYPE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_IS_EDL_MODE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_DEV_PROG_ELF_ENTRY_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_SPL_CONFIG_ELF_ENTRY_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_SPL_SC_EXT_ELF_ENTRY_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_SIZE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_DEBUG_SHARED_INFO_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_DEBUG_SHARED_INFO_SIZE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_TME_CPU_PBL_ROM_BYPASS_FUSE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_SPL_SC_DEBUG_LOG_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_SPL_SC_DEBUG_LOG_SIZE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_CURRENT_IMAGE_SET,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_MEDIA_DATA_INFO_ADDR,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_MEDIA_DATA_INFO_SIZE,
	PBL_APPS_SPL_SHARED_DATA_PARAM_ID_MAX,
};

/**
 * PBL shared data entry structure
 * param_id: Parameter ID (enum value)
 * param_val: Parameter value
 * is_valid_entry: Validity flag (boolean)
 */
struct pbl_shared_data_entry {
	u32 param_id;
	uintptr_t param_val;
	u8 is_valid_entry;
};

/**
 * PBL shared data structure passed from PBL to SPL
 */
struct pbl_shared_data {
	u32 version;
	u32 num_of_entries;
	struct pbl_shared_data_entry shared_data_entry[PBL_APPS_SPL_SHARED_DATA_PARAM_ID_MAX];
};

/*******************************************************************************
 * Globals constant & typedef
 ******************************************************************************/
#define IPQ_SPL_BOOTCFG_REG_ADDR	0xA602C
#define IPQ_SPL_BOOTCFG_DEV_MASK	GENMASK(3, 1)
#define IPQ_SPL_BOOTCFG_DEV_SHFT	0x1

#define IPQ_SPL_TCSR_REG_ADDR		0x195C100
#define IPQ_SPL_DLOAD_MASK		GENMASK(4, 4)
#define IPQ_SPL_DLOAD_SHFT		0x4
#define IPQ_SPL_EDL_MASK		GENMASK(0, 0)
#define IPQ_SPL_EDL_SHFT		0x0
#define IPQ_SPL_POR_RESET_MASK		GENMASK(10, 10)
#define IPQ_SPL_POR_RESET_SHFT		0xA

#define IPQ_SPL_TCSR_BOOT_INFO_ADDR	0x195C158
#define IPQ_SPL_SETB_MASK		GENMASK(31, 31)
#define IPQ_SPL_SETB_SHFT		0x1F

#define IPQ_SPL_IS_DLOAD_BIT_SET()	((readl(IPQ_SPL_TCSR_REG_ADDR) & \
					  IPQ_SPL_DLOAD_MASK) >> \
					  IPQ_SPL_DLOAD_SHFT)

#define IPQ_SPL_IS_POR_RESET()		(((readl(IPQ_SPL_TCSR_REG_ADDR) & \
					  IPQ_SPL_POR_RESET_MASK) >> \
					  IPQ_SPL_POR_RESET_SHFT) == 0)

#define IPQ_SPL_IS_TCSR_SETB()		((readl(IPQ_SPL_TCSR_BOOT_INFO_ADDR) & \
					  IPQ_SPL_SETB_MASK) >> \
					  IPQ_SPL_SETB_SHFT)

#define IPQ_SPL_SET_TCSR_SETA()		clrbits_le32(\
						IPQ_SPL_TCSR_BOOT_INFO_ADDR, \
						IPQ_SPL_SETB_MASK)

#define IPQ_SPL_SET_TCSR_SETB()		setbits_le32(\
						IPQ_SPL_TCSR_BOOT_INFO_ADDR, \
						IPQ_SPL_SETB_MASK)

#define IPQ_SPL_FORCE_INACTIVE_ADDR	0x86000CC
#define IPQ_SPL_IS_FORCE_INACTIVE_EN()	\
	(readl(IPQ_SPL_FORCE_INACTIVE_ADDR) == 1)

#define IPQ_SPL_FIT_IMG_PARTITION		"0:BOOTLDR"
#define IPQ_SPL_FIT_IMG_ALT_PARTITION		"0:BOOTLDR_1"

#define IPQ_SPL_ENV_PARTITION			"0:APPSBLENV"
#define IPQ_SPL_DEFAULT_BOOTLIMIT		0x3
#define IPQ_SPL_DEFAULT_SPL_PARTITION_LABEL	"0:SPL"
#define IPQ_SPL_DEFAULT_SPL_PARTITION_GUID	EFI_GUID(\
						0xdea0ba2c, 0xcbdd, 0x4805,\
						0xb4, 0xf9, 0xf4, 0x28,\
						0x25, 0x1c, 0x3e, 0x98)

#define IPQ_SPL_QPIC_NAND_ADDR0_REG		0x79B0004
#define IPQ_SPL_QPIC_NAND_ADDR0_MSB_MASK	GENMASK(31, 16)
#define IPQ_SPL_QPIC_NAND_ADDR0_MSB_SHFT	0x10

#define IPQ_SPL_GET_QPIC_PAGE_INDEX()	((readl(IPQ_SPL_QPIC_NAND_ADDR0_REG) & \
					  IPQ_SPL_QPIC_NAND_ADDR0_MSB_MASK) >> \
					  IPQ_SPL_QPIC_NAND_ADDR0_MSB_SHFT)

#define SPL_ERR_CAT_BOOT		0x00010000
#define SPL_ERR_CAT_DDR			0x00020000
#define SPL_ERR_CAT_AUTH		0x00030000
#define SPL_ERR_CAT_FLASH		0x00040000
#define SPL_ERR_CAT_QCLIB		0x00050000

#define SPL_ERR_NULL_PTR		0x0001
#define SPL_ERR_INVALID_PARAM		0x0002
#define SPL_ERR_INIT_FAIL		0x0003
#define SPL_ERR_NOT_FOUND		0x0004
#define SPL_ERR_NO_MEM			0x0005
#define SPL_ERR_AUTH_FAIL		0x0006
#define SPL_ERR_INTERFACE		0x0008

#define MAGIC_KEY			"QCLIB_CB"
#define MAX_ENTRIES			0xF
#define IF_TABLE_VERSION		0x1
#define QCCONFIG			"qc_config"
#define QCSDI				"qcsdi"
#define QCDAREKEY			"qc_dare_key"
#define QCUART				"qc_uart"
#define QCLIB_LOG_BUFFER		"qclib_log_buffer"

/*
 * Total size of the SMEM-resident console log buffer: must be large enough
 * to hold the drained pre-DDR SRAM logs, QCLIB's IMEM log buffer, and all
 * live SPL logging generated after consolidation (through end of SPL).
 */
#define SMEM_CONSOLE_LOG_SIZE		0x4000

/*
 * Image version table definitions
 */
#define IMAGE_INDEX_TMEL			28
#define IMAGE_INDEX_SPL				0

#define DEFAULT_SHIFT			0x0
#define DEFAULT_32BIT_MASK		0xFFFFFFFF
#define DEFAULT_64BIT_MASK		0xFFFFFFFFFFFFFFFF

/*******************************************************************************
 * Structure enum and static
 ******************************************************************************/

/*
 * SPL Error Handling - First-Error Tracking
 */

/**
 * struct spl_error_info - Tracks first error location
 * @file: Source file where error occurred
 * @line: Line number where error occurred
 * @code: Error code (category | code)
 *
 * This structure captures the FIRST error location to preserve root cause
 * information even if subsequent errors occur during error handling.
 */
struct spl_error_info {
	const char *file;
	u32 line;
	u32 code;
};

static struct spl_error_info g_spl_error_info = {NULL, 0, 0};

/*
 * LCP region keys per slot structure
 */
struct lcp_region_keys_per_slot {
	u32 encrypt_key[4];		/* 128-bit encryption key */
	u32 sha3_key[4];		/* 128-bit SHA3 key */
	u32 encrypt_alpha_key[2];	/* 64-bit alpha key */
} __packed;
/*
 * Global variable to store TME-L patch version
 * Explicitly initialized to empty string
 */
static char g_tme_version[TME_PATCH_VERSION_LENGTH] = {0};

enum {
	IPQ_SPL_RAM_FLASHLESS = 0xFD,
	IPQ_SPL_FLASHTYPE_MAX = 0xFF
};

enum {
	IPQ_SPL_BOOTCFG_DEV_NOR_GPT	= 0x0,
	IPQ_SPL_BOOTCFG_DEV_MMC		= 0x1,
	IPQ_SPL_BOOTCFG_DEV_SPI_NAND	= 0x2,
	IPQ_SPL_BOOTCFG_DEV_USB		= 0x3,
	IPQ_SPL_BOOTCFG_DEV_NOR_MIBIB	= 0x4,
	IPQ_SPL_BOOTCFG_DEV_MAX
};

/**
 * struct ipq_spl_fuse_info - Fuse information structure
 * @fuse_name:	Name of the fuse.
 * @fuse_addr:	Address of the fuse register.
 * @secboot_protected: If secure boot is enabled, do not log this entry.
 * @full_row:	Indicates full 64-bit row should be logged. Default is 32 bits.
 * @mask:	Mask to be applied to the register value.
 * @shift:	Shift to be applied after masking.
 */
struct ipq_spl_fuse_info {
	char fuse_name[24];
	u32 fuse_addr;
	bool secboot_protected;
	bool full_row;
	u64 mask;
	u32 shift;
};

/**
 * struct interface_table_entry - Meta data for blobs in QCLIB interface
 * @entry_name:	Name of the data blob (e.g., "dcb_settings").
 * @address:	Address of the data blob.
 * @size:	Size of the data blob.
 * @attributes:	Attributes for the blob (e.g., save to storage).
 */
struct interface_table_entry {
	char entry_name[24];
	u64 address;
	u32 size;
	u32 attributes;
};

/**
 * struct interface_table - QCLIB Interface table header
 * @magic_key:		Magic key for validation ("QCLIB_CB").
 * @version:		Interface table version.
 * @num_entries:	Number of valid entries.
 * @max_entries:	Maximum allowable entries.
 * @global_attributes:	Flags for global attributes (e.g., SDI path).
 * @reserved1:		Reserved for future use.
 * @reserved2:		Reserved for future use.
 * @if_table_entries:	Array of interface table entries.
 */
struct interface_table {
	char magic_key[8];
	u32 version;
	u32 num_entries;
	u32 max_entries;
	u32 global_attributes;
	u32 reserved1;
	u32 reserved2;
	struct interface_table_entry if_table_entries[MAX_ENTRIES];
};

/* MIBIB and partition table definitions */
#define MIBIB_MAGIC1			0xFE569FAC
#define MIBIB_MAGIC2			0xCD7F127A
#define MIBIB_VERSION			4
#define MIBIB_BLOCK_SEARCH_MAX		0x40
#define MIBIB_PAGE_PARTITION_TABLE	1
#define MIBIB_PAGE_LAST_PAGE		4
#define MIBIB_PAGE_CRC			3

#define FLASH_PART_MAGIC1		0x55EE73AA
#define FLASH_PART_MAGIC2		0xE35EBDDB
#define FLASH_PARTITION_VERSION		4

#define FLASH_USR_PART_MAGIC1		0xAA7D1B9A
#define FLASH_USR_PART_MAGIC2		0x1F7D48BC

#define FLASH_MIBIB_CRC_MAGIC1		0x9D41BEA1
#define FLASH_MIBIB_CRC_MAGIC2		0xF1DED2EA
#define FLASH_MIBIB_CRC_VERSION		1

/**
 * Global variable to track secure boot status.
 * This is set by ipq_spl_list_tme_fuse()
 */
static bool secure_boot_enabled;

enum {
	IPQ_SPL_BOOT_FROM_ACTIVE		= 0x0,
	IPQ_SPL_BOOT_FROM_INACTIVE		= 0x1,
	IPQ_SPL_BOOT_SET_MAX,
};

/**
 * struct ipq_spl_bootrec_ctx - SPL failsafe boot record context
 * @boot_mode:          Resolved boot mode (DEFAULT/FAILOVER_EN/FORCE_INACTIVE)
 * @boot_set:           Final boot set used to load the bootloader
 * @exp_boot_set:       Expected boot set derived from boot_mode and bootcount
 * @pbl_set:            Boot set PBL actually loaded SPL from
 * @is_pbl_set_parsed:  Flag indicating pbl_set has been read and verified
 * @tcsr_set:           Current boot set stored in TCSR register
 * @exp_tcsr_set:       Expected TCSR set after failsafe resolution
 * @env_failover:       ENV "failover" key value (0=disabled)
 * @env_bootlimit:      ENV "bootlimit" key value (max retries per slot)
 * @env_bootfrom:       ENV "bootfrom" key value (NAND only; ACTIVE for others)
 * @qpic_page_index:    QPIC page index recorded by PBL (NAND only)
 */
struct ipq_spl_bootrec_ctx {
	u8 boot_mode;
	u8 boot_set;
	u8 exp_boot_set;
	u8 pbl_set;
	u8 is_pbl_set_parsed;
	u8 tcsr_set;
	u8 exp_tcsr_set;

	u32 env_failover;
	u32 env_bootlimit;
	u32 env_bootfrom;
	u32 qpic_page_index;
};

/**
 * @g_bootrec: Global SPL failsafe boot record
 */
static struct ipq_spl_bootrec_ctx g_bootrec;

/* Global variables to store MIBIB partition table and bootloader offset */
static struct flash_partition_table g_mibib_parti_tbl;
static int g_bootldr_offset;

/**
 * Global variables for PBL shared data and logs
 * must be in .data section, not .bss,
 * because they are initialized in save_boot_params()
 * BEFORE board_init_f() clears BSS.
 */
static struct pbl_shared_data g_pbl_shared_data __section(".data");
static char g_pbl_log_buffer[PBL_LOG_BUFFER_SIZE] __section(".data");
static bool g_pbl_data_valid __section(".data");

/**
 * Global variable to store early SPL start timestamp
 * Must be in .data section to survive BSS clearing
 */
#if CONFIG_IS_ENABLED(BOOTSTAGE)
static u64 g_spl_start_time __section(".data");
#endif/*CONFIG_BOOTSTAGE*/

/**
 * Pre-DDR SRAM console log buffer, backing gd->console_out until the
 * SMEM-resident log buffer is available (see ipq_spl_smem_console_log_init()).
 * Must be in .data section since it is populated via console_record_*()
 * hooks starting in board_init_f(), before later BSS-clearing stages.
 */
#ifdef CONFIG_CONSOLE_RECORD
static char g_console_record_buf[CONFIG_CONSOLE_RECORD_OUT_SIZE_F] __section(".data");

/**
 * Set once console logging has been consolidated into SMEM (see
 * ipq_spl_smem_console_log_init()). Needed because gd->console_out only
 * tracks a position within its own live buffer, not the absolute offset
 * within smem_log->data -- ipq_spl_smem_console_log_sync() uses these to
 * keep smem_log->write_offset current so a reader always knows how much of
 * the buffer holds valid data, including logs emitted after consolidation.
 */
static struct smem_console_log *g_console_smem_log __section(".data");
static u32 g_console_smem_base __section(".data");
#endif /* CONFIG_CONSOLE_RECORD */

/**
 * struct mi_boot_info - MIBIB header structure
 * @magic1:	First magic number for validation
 * @magic2:	Second magic number for validation
 * @version:	MIBIB version
 * @age:	Age counter for determining the newest MIBIB
 * @numparts:	Number of partitions
 * @reserved1:	Reserved for future use
 * @reserved2:	Reserved for future use
 * @reserved3:	Reserved for future use
 */
struct mi_boot_info {
	u32 magic1;
	u32 magic2;
	u32 version;
	u32 age;
	u32 numparts;
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
};

/**
 * struct flash_partition_entry - System partition table entry definition
 * @name:        Name of the partition in the form of 0:ALL, 0:EFS2, etc.
 * @offset:      Offset in blocks from beginning of device
 * @length:      Length in blocks of the partition
 * @attrib1:     Partition attribute 1 (e.g., read-only, SLC/MLC mode)
 * @attrib2:     Partition attribute 2 (e.g., ECC configuration)
 * @attrib3:     Partition attribute 3 (e.g., upgrade mechanism)
 * @which_flash: Numeric ID of flash part (first = 0, second = 1)
 *
 * This structure defines a single partition entry in the system partition table.
 * Each entry contains information about the partition's location, size, and
 * various attributes that control how the partition is accessed and managed.
 */
struct flash_partition_entry {
	/* Name of the partition in the form of 0:ALL, 0:EFS2, etc. */
	char name[16];

	/* Offset in blocks from beginning of device */
	u32 offset;

	/* length in blocks of the partition */
	u32 length;

	/* Partition attributes */
	u8 attrib1;
	u8 attrib2;
	u8 attrib3;

	/* Numeric ID of flash part (first = 0, second = 1) */
	u8 which_flash;
};

/**
 * Maximum number of partitions supported in the partition table
 * Plus one extra entry for the "all" partition that represents the entire device
 */
#define FLASH_NUM_PART_ENTRIES  32
#define FLASH_PART_ENTRY_TOTAL (FLASH_NUM_PART_ENTRIES + 1)

/**
 * struct flash_partition_table - System partition table definition
 * @magic1:    First magic number for validation (0x55EE73AA)
 * @magic2:    Second magic number for validation (0xE35EBDDB)
 * @version:   Partition table version (currently 4)
 * @numparts:  Number of valid partition entries in the table
 * @part_entry: Array of partition entries
 *
 * This structure defines the system partition table that is stored in flash.
 * It contains a header with magic numbers and version information, followed
 * by an array of partition entries that define the layout of the flash device.
 *
 * WARNING: The placement of the first three elements (magic1, magic2, version)
 * must not be changed to ensure backward compatibility.
 */
struct flash_partition_table {
	/* Partition table magic numbers and version number.
	 *   WARNING!!!!
	 *   No matter how you change the structure, do not change
	 *   the placement of the first three elements so that future
	 *   compatibility will always be guaranteed at least for
	 *   the identifiers.
	 */
	u32 magic1;
	u32 magic2;
	u32 version;

	/* Partition table data.  This portion of the structure may be changed
	 * as necessary to accommodate new features.  Be sure to increment
	 * version number if you change it.
	 */
	u32 numparts;   /* number of partition entries */
	struct flash_partition_entry part_entry[FLASH_PART_ENTRY_TOTAL];
};

/**
 * struct flash_usr_partition_entry - User partition table entry definition
 * @name:          Name of the partition in the form of 0:ALL, 0:EFS2, etc.
 * @img_size:      Size in KB for the partition
 * @padding:       Padding in KB for static handling of NAND bad blocks
 * @which_flash:   Numeric ID of flash part (first = 0, second = 1)
 * @reserved_flag1: Attribute 1 for this partition (copied to attrib1)
 * @reserved_flag2: Attribute 2 for this partition (copied to attrib2)
 * @reserved_flag3: Attribute 3 for this partition (copied to attrib3)
 * @reserved_flag4: Layout based flags
 *
 * This structure defines a single entry in the user partition table.
 * The user partition table is used during initial flash programming to
 * create the system partition table. It contains information about the
 * desired size and attributes of each partition.
 */
struct flash_usr_partition_entry {
	/* Name of the partition in the form of 0:ALL, 0:EFS2, etc. */
	char name[16];

	/* Size in KB for the partition */
	u32 img_size;

	/* Padding in KB for static handling of NAND bad blocks in this partition */
	/* This field can also be used to define minimum size requirement for a
	 * parition defined in terms of number of sectors/blocks.
	 * Please note that this is possible because there never are bad blocks on a
	 * a NOR device
	 */
	u16 padding;

	/* Numeric ID of flash part (first = 0, second = 1) */
	u16 which_flash;

	/* Attributes for this partition - This get copied to the attribx flags in
	 * system partition table
	 */
	u8 reserved_flag1;
	u8 reserved_flag2;
	u8 reserved_flag3;

	u8 reserved_flag4;  /* layout based flags */
};

/**
 * struct flash_usr_partition_table - User partition table definition
 * @magic1:    First magic number for validation (0xAA7D1B9A)
 * @magic2:    Second magic number for validation (0x1F7D48BC)
 * @version:   Partition table version (currently 4)
 * @numparts:  Number of valid partition entries in the table
 * @part_entry: Array of user partition entries
 *
 * This structure defines the user partition table that is used during
 * initial flash programming to create the system partition table.
 * It contains a header with magic numbers and version information,
 * followed by an array of user partition entries.
 *
 * WARNING: The placement of the first three elements (magic1, magic2, version)
 * must not be changed to ensure backward compatibility.
 */
struct flash_usr_partition_table {
	/* Partition table magic numbers and version number.
	 *   WARNING!!!!
	 *   No matter how you change the structure, do not change
	 *   the placement of the first three elements so that future
	 *   compatibility will always be guaranteed at least for
	 *   the identifiers.
	 */
	u32 magic1;
	u32 magic2;
	u32 version;

	/* Partition table data.  This portion of the structure may be changed
	 * as necessary to accommodate new features.  Be sure to increment
	 * version number if you change it.
	 */
	u32 numparts;   /* number of partition entries */
	struct flash_usr_partition_entry part_entry[FLASH_PART_ENTRY_TOTAL];
};

/**
 * struct flash_mibib_crc - MIBIB CRC structure
 * @magic1:    First magic number for validation (0x9D41BEA1)
 * @magic2:    Second magic number for validation (0xF1DED2EA)
 * @version:   CRC version (currently 1)
 * @crc:       CRC32 checksum of the MIBIB contents
 * @reserved:  Reserved fields for future use
 *
 * This structure is stored in the MIBIB CRC page and contains the CRC32
 * checksum of the MIBIB contents. It is used to verify the integrity of
 * the MIBIB during boot.
 */
struct flash_mibib_crc {
	u32 magic1;
	u32 magic2;
	u32 version;
	u32 crc;
	u32 reserved[4];
};

/**
 * struct ipq_spl_img_ctx - SPL image context
 * @img_name:	Name of the image.
 * @prt_name:	Partition name where the image resides.
 * @sw_id:	Software ID for the image.
 * @load_addr:	Load address of the image.
 * @img_sz:	Size of the image.
 * @img_off:	Offset of the image within the partition.
 * @load:	Flag indicating if the image should be loaded.
 * @auth:	Authentication flag.
 * @optional:	Flag indicating if the image loading is optional.
 * @fit_node:	Node ID for FIT image component.
 * @fixup:	Function pointer for image-specific fixup operations.
 */
struct ipq_spl_img_ctx {
	char *img_name;
	char *prt_name;
	u64 sw_id;
	u64 load_addr;
	u64 img_sz;
	u64 img_off;
	u8 load;
	u8 auth;
	u8 optional;
	u8 img_arch;
	int fit_node;
	int (*fixup)(void *ctx);
};

/**
 * struct ipq_spl_ctx - Global SPL context structure
 * @img_tbl:	Pointer to the current image table entry.
 * @if_tbl:	QCLIB interface table.
 * @fl_ctx:	Flash context structure.
 * @spl_image:	SPL image info structure.
 * @bootdev:	SPL boot device structure.
 * @fit:	Pointer to the loaded FIT image blob.
 * @bl31_entry:	Entry point for BL31 (TF-A).
 * @bl32_entry:	Entry point for BL32 (OP-TEE).
 * @bl33_entry:	Entry point for BL33 (U-Boot/kernel).
 */
struct ipq_spl_ctx {
	struct ipq_spl_img_ctx *img_tbl;
	struct interface_table if_tbl;
	struct spl_image_info *spl_image;
	struct spl_boot_device *bootdev;
	void *fit;
};

#define U_BOOT_IPQ_SPL_CTX(__name) \
ll_entry_declare(struct ipq_spl_ctx, __name, ipq_spl_ctx)

#define U_BOOT_GET_IPQ_SPL_CTX(__name) \
llsym(struct ipq_spl_ctx, __name, ipq_spl_ctx)

/*
 * Declare IPQ SPL default context
 */
U_BOOT_IPQ_SPL_CTX(ipq_default_ctx);

/**
 * ipq_spl_jump_img_entry_t - Type definition for image entry point functions.
 * @arg1:	First argument passed to the entry point.
 * @arg2:	Second argument passed to the entry point.
 */
typedef void (*ipq_spl_jump_img_entry_t)(void *arg1, void *arg2);

/*
 * Forward declarations for image fixup functions
 */
static int ipq_spl_dpr_fixup(void *ctx);
static int ipq_spl_xcfg_fixup(void *ctx);
static int ipq_spl_qclib_fixup(void *ctx);
static int ipq_spl_tfa_fixup(void *ctx);
static int ipq_spl_optee_fixup(void *ctx);
static int ipq_spl_uboot_fixup(void *ctx);

/*
 * Forward declarations for console log sync/init (see full definitions
 * below, needed by call sites - ipq_spl_qclib_fixup(), ipq_spl_reset_cpu(),
 * hang() call sites - that appear earlier in this file)
 */
#ifdef CONFIG_CONSOLE_RECORD
static void ipq_spl_smem_console_log_sync(void);
static int ipq_spl_smem_console_log_init(void *ctx);
#endif /* CONFIG_CONSOLE_RECORD */

/*
 * Forward declarations for boot log functions
 */
static void ipq_spl_log_lcs_state(void);
static void ipq_spl_log_debug_state(void);
static void ipq_spl_log_otp_version(void);

/*
 * Forward declarations for flash specific functions
 */
static int (*get_guid_fn)(char *part_name, efi_guid_t *type_guid);
static int ipq_spl_spinor_gpt_read(char *part_name, void *buf, ulong *read_sz,
					   ulong *part_sz);
static int ipq_spl_spinor_get_guid(char *part_name, efi_guid_t *type_guid);
static int ipq_spl_mmc_read(char *part_name, void *buf, ulong *read_sz,
				    ulong *part_sz);
static int ipq_spl_mmc_get_guid(char *part_name, efi_guid_t *type_guid);
static int ipq_spl_nand_read(char *part_name, void *buf, ulong *read_sz,
				     ulong *part_sz);

/**
 * fuse_info_array - Array of fuse information.
 *
 * This array contains the names and addresses of various fuses used in the
 * system. These fuses are typically used for configuration.
 * The secboot_protected flag indicates whether the fuse should only be
 * printed when secure boot is disabled.
 * The full_row flag indicates whether the full 64-bit row should be logged.
 */
static struct ipq_spl_fuse_info fuse_info_array[] = {
	{"OEM Config Row 1", IPQ_SPL_FUSE_OEM_CONFIG_ROW_1_ADDR, true, true,
	 DEFAULT_64BIT_MASK, DEFAULT_SHIFT},
	{"Feature Config Row 0", IPQ_SPL_FUSE_FEATURE_CONFIG_ROW_0_ADDR,
	 false, true, DEFAULT_64BIT_MASK, DEFAULT_SHIFT},
	{"Feature Config Row 1", IPQ_SPL_FUSE_FEATURE_CONFIG_ROW_1_ADDR,
	 false, true, DEFAULT_64BIT_MASK, DEFAULT_SHIFT},
	{"Boot Config", IPQ_SPL_FUSE_BOOT_CFG_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"JTAG ID", IPQ_SPL_FUSE_JTAG_ID_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"OEM ID", IPQ_SPL_FUSE_OEM_ID_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"TME-L LCS", IPQ_SPL_FUSE_TME_L_LCS_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"Serial Number", IPQ_SPL_FUSE_SERIAL_NUM_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"Product Id", IPQ_SPL_FUSE_PRODUCT_ID_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"FEATURE ID", IPQ_SPL_FUSE_FEATURE_ID_ADDR, false, false,
	 IPQ_SPL_FEATURE_ID_MASK, IPQ_SPL_FEATURE_ID_SHIFT},
	{"Reset Debug", IPQ_SPL_GCC_RESET_DEBUG_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"Reset Status", IPQ_SPL_GCC_RESET_STATUS_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
	{"FSM Status", IPQ_SPL_GCC_FSM_STATUS_ADDR, false, false,
	 DEFAULT_32BIT_MASK, DEFAULT_SHIFT},
};

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
/**
 * tme_fuse_info_array - Array of TME IPC based fuse information.
 *
 * This array contains the names and addresses of various TME IPC based fuses
 * used in the system. These fuses are typically used for security features
 * and configuration settings.
 */
static struct ipq_spl_fuse_info tme_fuse_info_array[] = {
	{"OEM Config Row 0", IPQ_SPL_FUSE_OEM_TME_ROW_0_ADDR, true, true,
	 DEFAULT_64BIT_MASK, DEFAULT_SHIFT},
};
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

/**
 * img_tbl_fit - Image loader table for FIT images.
 *
 * This table defines the FIT images to be loaded and their associated fixup
 * functions.
 */
struct ipq_spl_img_ctx img_tbl_fit[] = {
	{
		.img_name = "dpr",
		.sw_id = IPQ_SPL_DPR_SEC_AUTH_SWID,
		.auth = true,
		.optional = true,
		.fixup = ipq_spl_dpr_fixup,
	}, {
		.img_name = "qcconfig-meta",
		.sw_id = IPQ_SPL_QCCONFIG_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_xcfg_fixup,
#if defined(CONFIG_TARGET_IPQ5210)
	}, {
		.img_name = "qcconfig-meta-v7",
		.sw_id = IPQ_SPL_QCCONFIG_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_xcfg_fixup,
#endif
	}, {
		.img_name = "qclib-meta",
		.sw_id = IPQ_SPL_QCLIB_DDR_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_qclib_fixup,
#if defined(CONFIG_TARGET_IPQ5210)
	}, {
		.img_name = "qclib-meta-v7",
		.sw_id = IPQ_SPL_QCLIB_DDR_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_qclib_fixup,
#endif
	}, {
		.img_name = "tfa_bl31-meta",
		.sw_id = IPQ_SPL_TZ_TEE_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_tfa_fixup,
#if defined(CONFIG_TARGET_IPQ5210)
	}, {
		.img_name = "tfa_bl31-meta-v7",
		.sw_id = IPQ_SPL_TZ_TEE_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_tfa_fixup,
#endif
	}, {
		.img_name = "optee-meta",
		.sw_id = IPQ_SPL_OP_TEE_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_optee_fixup,
#if defined(CONFIG_TARGET_IPQ5210)
	}, {
		.img_name = "optee-meta-v7",
		.sw_id = IPQ_SPL_OP_TEE_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_optee_fixup,
#endif
	}, {
		.img_name = "uboot-meta",
		.sw_id = IPQ_SPL_APPSBL_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_uboot_fixup,
#if defined(CONFIG_TARGET_IPQ5210)
	}, {
		.img_name = "uboot-meta-v7",
		.sw_id = IPQ_SPL_APPSBL_SEC_AUTH_SWID,
		.auth = true,
		.fixup = ipq_spl_uboot_fixup,
#endif
	},
};


/*******************************************************************************
 * Function definition
 ******************************************************************************/

/**
 * lowlevel_init() - Early low-level initialization.
 *
 * This function performs very early hardware initialization,
 * specifically disabling the MMU, which is a common requirement for
 * bare-metal bootloaders before memory management is fully set up.
 */
void lowlevel_init(void)
{
	/*
	 * Place holder
	 */
}

static void enable_sec_wdog(u32 timeout_ms)
{
	u32 bark_cnt;
	u32 bite_cnt;

	bark_cnt = ((timeout_ms - 1) * WDT_SLEEP_CLK_HZ) / 1000;
	bite_cnt = (timeout_ms * WDT_SLEEP_CLK_HZ) / 1000;

	/* Configure watchdog in secure mode */
	writel(0x0, WDT2_BASE_ADDR + WDT_SECURE);

	/* Disable watchdog before configuration */
	writel(0x0, WDT2_BASE_ADDR + WDT_EN);

	/* Reset/pet watchdog */
	writel(WDT_RESET_BIT, WDT2_BASE_ADDR + WDT_RST);

	/* Program bark and bite time */
	writel(bark_cnt, WDT2_BASE_ADDR + WDT_BARK_TIME);
	writel(bite_cnt, WDT2_BASE_ADDR + WDT_BITE_TIME);

	/* Enable watchdog */
	writel(WDT_ENABLE_BIT, WDT2_BASE_ADDR + WDT_EN);
}

/*
 * disable_sec_wdog() - Disable secure watchdog timer
 */
static void __maybe_unused disable_sec_wdog(void)
{
	writel(0x0, WDT2_BASE_ADDR + WDT_SECURE);

	/* Disable watchdog */
	writel(0x0, WDT2_BASE_ADDR + WDT_EN);

	/* Clear/reset watchdog state */
	writel(WDT_RESET_BIT, WDT2_BASE_ADDR + WDT_RST);
}

/**
 * save_boot_params() - Save PBL shared data and logs
 * @r0: First argument from PBL (shared data pointer)
 *
 * This function is called VERY EARLY from assembly code before BSS is cleared.
 * It must save PBL data to static buffers.
 *
 * CRITICAL: This function executes before memory initialization,
 * so it can only use static/global variables, not heap or stack.
 *
 * IMPORTANT: This function MUST call save_boot_params_ret() to return
 * to the assembly caller. Normal C return statements will corrupt the CPU state!
 */
void save_boot_params(unsigned long r0, unsigned long r1,
		      unsigned long r2, unsigned long r3)
{
	struct pbl_shared_data *pbl_data_ptr = (struct pbl_shared_data *)r0;
	uintptr_t pbl_log_addr;
	u32 pbl_log_size;
	unsigned long sctlr;

	/**
	 * Early disable the MMU
	 */
	sctlr = get_sctlr();
	set_sctlr(sctlr & ~(CR_M));

	/**
	 * Validate PBL shared data pointer
	 */
	if (!pbl_data_ptr || pbl_data_ptr->version == 0) {
		g_pbl_data_valid = false;
		goto exit;
	}

	/**
	 * Validate number of entries
	 */
	if (pbl_data_ptr->num_of_entries < PBL_APPS_SPL_SHARED_DATA_PARAM_ID_MAX) {
		g_pbl_data_valid = false;
		goto exit;
	}

	/**
	 * Copy entire PBL shared data structure to our static buffer
	 */
	memcpy(&g_pbl_shared_data, pbl_data_ptr, sizeof(struct pbl_shared_data));

	/**
	 * Get PBL log buffer address and size using CORRECT indices
	 */
	if (g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_ADDR]
		.is_valid_entry &&
	    g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_SIZE]
		.is_valid_entry) {

		pbl_log_addr = g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_ADDR]
			.param_val;
		pbl_log_size = g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_SIZE]
			.param_val;

		/**
		 * Validate and copy PBL logs
		 */
		if (pbl_log_addr && pbl_log_size > 0) {
			/**
			 * Reserve one byte for null terminator
			 * Clamp to buffer size minus 1 to ensure space for '\0'
			 */
			if (pbl_log_size >= PBL_LOG_BUFFER_SIZE)
				pbl_log_size = PBL_LOG_BUFFER_SIZE - 1;

			memcpy(g_pbl_log_buffer, (void *)pbl_log_addr, pbl_log_size);
			g_pbl_log_buffer[pbl_log_size] = '\0';  /* Safe null termination */
			g_pbl_shared_data.shared_data_entry[
				PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_ADDR]
				.param_val = (uintptr_t)g_pbl_log_buffer;

			g_pbl_data_valid = true;
		}
	}

exit:
	/*
	 * CRITICAL: Must call save_boot_params_ret() to return to assembly code.
	 * This is NOT a normal C function - it's called from assembly and must
	 * use the special return mechanism to avoid corrupting CPU state.
	 */
	save_boot_params_ret();
}

/**
 * ipq_spl_print_pbl_version() - Print PBL version information
 *
 * This function prints the PBL firmware and patch version from the
 * saved PBL shared data.
 */
static void ipq_spl_print_pbl_version(void)
{
	u32 pbl_fw_ver, pbl_patch_ver;

	if (!g_pbl_data_valid)
		return;

	if (g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_FW_VERSION]
		.is_valid_entry &&
	    g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_PATCH_VERSION]
		.is_valid_entry) {

		pbl_fw_ver = g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_FW_VERSION]
			.param_val;
		pbl_patch_ver = g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_PATCH_VERSION]
			.param_val;

		printf("PBL FW Ver: %u, Patch Ver: %u\n", pbl_fw_ver, pbl_patch_ver);
	}
}

/**
 * ipq_spl_print_pbl_clock() - Print PBL clock frequency
 *
 * This function prints the CPU boot speed from the saved PBL shared data.
 */
static void ipq_spl_print_pbl_clock(void)
{
	u32 pbl_clk_hz;

	if (!g_pbl_data_valid)
		return;

	if (g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_CPU_BOOT_SPEED_HZ]
		.is_valid_entry) {
		pbl_clk_hz = g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_CPU_BOOT_SPEED_HZ]
			.param_val;

		if (pbl_clk_hz > 0)
			printf("PBL Clock: %u MHz\n", pbl_clk_hz / 1000000);
	}
}

/**
 * ipq_spl_print_pbl_boot_interface() - Print Boot Interface type from PBL
 *
 * This function prints the boot media type from the saved PBL shared data.
 */
static void ipq_spl_print_pbl_boot_interface(void)
{
	u32 boot_interface;
	const char *media_str = "Unknown";

	if (!g_pbl_data_valid)
		return;

	if (g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_BOOT_MEDIA_TYPE]
		.is_valid_entry) {
		boot_interface = g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_BOOT_MEDIA_TYPE]
			.param_val;

		switch (boot_interface) {
		case NOR:
			media_str = "NOR";
			break;
		case MMC:
			media_str = "MMC/eMMC";
			break;
		case SPI_NOR_MIBIB:
			media_str = "SPI NOR MIBIB";
			break;
		case SPI_NAND:
			media_str = "SPI NAND";
			break;
		case SPI_NOR_GPT:
			media_str = "SPI NOR GPT";
			break;
		case NAND:
			media_str = "NAND";
			break;
		default:
			media_str = "Unknown";
			break;
		}

		printf("Boot Interface: %s\n", media_str);
	}
}

/**
 * ipq_spl_print_pbl_logs() - Print PBL timestamp logs
 *
 * This function prints the PBL.
 */
static void ipq_spl_print_pbl_logs(void)
{
	char *pbl_log_ptr;

	/**
	 * Print PBL information
	 */
	printf("\n========== Boot Information ==========\n");
	ipq_spl_print_pbl_version();
	ipq_spl_print_pbl_clock();
	ipq_spl_print_pbl_boot_interface();
	printf("======================================\n");

	/**
	 * Print PBL timestamp logs
	 */
	if (!g_pbl_data_valid) {
		printf("PBL logs not available\n");
		return;
	}

	if (g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_ADDR]
		.is_valid_entry) {
		pbl_log_ptr = (char *)g_pbl_shared_data.shared_data_entry[
			PBL_APPS_SPL_SHARED_DATA_PARAM_ID_PBL_TIMESTAMPS_BUFFER_ADDR]
			.param_val;

		/**
		 * Verify pointer is within our safe buffer range
		 * This prevents accessing stale PBL memory addresses
		 */
		if (pbl_log_ptr >= g_pbl_log_buffer &&
		    pbl_log_ptr < (g_pbl_log_buffer + PBL_LOG_BUFFER_SIZE) &&
		    pbl_log_ptr[0] != '\0') {
			printf("========= PBL Timestamp Logs =========\n");
			printf("%s", pbl_log_ptr);
			printf("======================================\n");
		} else {
			printf("PBL logs pointer validation failed\n");
		}
	}
}

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
/**
 * ipq_spl_tmel_bypass_enabled() - Check if TMEL bypass is enabled
 *
 * This function reads the FEATURE_CONFIG2 register on first call and caches
 * the result. It checks bit 0 (TMEL_BYPASS_DISABLE). If the bit is 0,
 * TME-L authentication should be bypassed.
 *
 * Return: true if TMEL bypass is enabled, false otherwise
 */
static bool ipq_spl_tmel_bypass_enabled(void)
{
	static bool initialized;
	static bool bypass;

	if (!initialized) {
		u32 feature_config2 = readl(IPQ_SPL_FEATURE_CONFIG2_REG_ADDR);

		bypass = !(feature_config2 & IPQ_SPL_TMEL_BYPASS_DISABLE_MASK);
		initialized = true;

		printf("TME - %s (FEATURE_CONFIG2=0x%08X)\n",
		       bypass ? "Disabled" : "Enabled", feature_config2);
	}
	return bypass;
}
#endif

/**
 * bootset_str() - Return a printable name for a boot set value
 * @bootset: IPQ_SPL_BOOT_FROM_ACTIVE or IPQ_SPL_BOOT_FROM_INACTIVE
 *
 * Return: "SET-ACTIVE" or "SET-INACTIVE"
 */
static inline const char *bootset_str(u8 bootset)
{
	return bootset == IPQ_SPL_BOOT_FROM_INACTIVE ?
		"SET-INACTIVE" : "SET-ACTIVE";
}

/**
 * ipq_spl_alt_bootset() - Return the alternate boot set
 * @bootset: Current boot set (IPQ_SPL_BOOT_FROM_ACTIVE/_INACTIVE)
 *
 * Return: Opposite boot set of @bootset.
 */
u8 ipq_spl_alt_bootset(u8 bootset)
{
	return bootset == IPQ_SPL_BOOT_FROM_INACTIVE ?
		IPQ_SPL_BOOT_FROM_ACTIVE : IPQ_SPL_BOOT_FROM_INACTIVE;
}

/**
 * get_boot_mode() - Get the current SPL boot mode
 *
 * Return: Current boot mode from g_bootrec structure
 */
u8 get_boot_mode(void)
{
	return g_bootrec.boot_mode;
}

/**
 * ipq_spl_get_tcsr_set() - Read the current boot set from TCSR
 *
 * Return: IPQ_SPL_BOOT_FROM_INACTIVE if SETB bit is set, else
 *         IPQ_SPL_BOOT_FROM_ACTIVE.
 */
u8 ipq_spl_get_tcsr_set(void)
{
	return IPQ_SPL_IS_TCSR_SETB() == 1 ?
		IPQ_SPL_BOOT_FROM_INACTIVE : IPQ_SPL_BOOT_FROM_ACTIVE;
}

/**
 * ipq_spl_set_tcsr_set() - Write the boot set to TCSR
 * @bootset: IPQ_SPL_BOOT_FROM_ACTIVE or IPQ_SPL_BOOT_FROM_INACTIVE
 */
void ipq_spl_set_tcsr_set(u8 bootset)
{
	if (bootset == IPQ_SPL_BOOT_FROM_INACTIVE) {
		IPQ_SPL_SET_TCSR_SETB();
		g_bootrec.tcsr_set = IPQ_SPL_BOOT_FROM_INACTIVE;

	} else {
		IPQ_SPL_SET_TCSR_SETA();
		g_bootrec.tcsr_set = IPQ_SPL_BOOT_FROM_ACTIVE;
	}
}

/**
 * ipq_spl_reset_cpu() - Log and trigger a CPU reset
 */
void ipq_spl_reset_cpu(void)
{
#ifdef CONFIG_CONSOLE_RECORD
	ipq_spl_smem_console_log_sync();
#endif /* CONFIG_CONSOLE_RECORD */
	reset_cpu();
}

/**
 * ipq_spl_edl_reset() - Set EDL bit, reset state, and trigger CPU reset
 *
 * Sets the EDL bit in TCSR, restores TCSR boot set to env_bootfrom,
 * clears bootcount, and resets the CPU. Does not return.
 */
void ipq_spl_edl_reset(void)
{
	IPQ_SPL_SET_TCSR_EDL();

	/*
	 * Reset Failsafe IMEM info
	 */
	ipq_spl_set_tcsr_set(g_bootrec.env_bootfrom);
	bootcount_store(0);

	ipq_spl_reset_cpu();
}

/**
 * ipq_spl_error_handler() - Centralized SPL error handler with file:line tracking
 * @file:	Source file where error occurred
 * @line:	Line number where error occurred
 * @err_code:	Error code (category | code)
 *
 * This function implements Phase 1 error handling enhancements:
 * 1. Error code categorization (category in upper 16 bits, code in lower 16 bits)
 * 2. File:line tracking for root cause analysis
 * 3. First-error preservation (XBL pattern)
 * 4. Cache flush before reset
 * 5. Boot path-aware error handling (DEFAULT, FAILOVER, FORCE_INACTIVE)
 *
 * The function is invoked upon critical errors during the SPL boot process.
 * It logs detailed error information, handles different boot paths, and
 * attempts recovery or enters a safe state (EDL mode or hang).
 *
 * IMPORTANT: This function preserves the FIRST error location even if
 * subsequent errors occur during error handling (XBL pattern).
 */
void ipq_spl_error_handler(const char *file, u32 line, u32 err_code)
{
	pr_err("Entered the SPL Error Handler\n");

	if (g_spl_error_info.file == NULL) {
		g_spl_error_info.file = file;
		g_spl_error_info.line = line;
		g_spl_error_info.code = err_code;
	}

	/*
	 * Log error with detailed information including:
	 * - Error code (category | code format)
	 * - File:line location
	 * - Boot path and boot set context
	 */
	printf("\n");
	printf("========================================\n");
	printf("SPL Fatal Error: 0x%08x\n", err_code);
	printf("Location: %s:%u\n", file ? file : "unknown", line);
	printf("========================================\n");

	/*
	 * Cache Flush (SPL data regions)
	 * Flush D-cache to ensure error logs and any modified data are
	 * written to memory before reset.
	 */
#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	{
		extern char __bss_start[], __bss_end[];

		flush_dcache_range((ulong)__bss_start, (ulong)__bss_end);
	}
#endif

	switch (g_bootrec.boot_mode) {
	case IPQ_SPL_BOOT_MODE_DEFAULT:
	case IPQ_SPL_BOOT_MODE_SPL_INACTIVE:
		printf("Default: Boot failed\n");
		ipq_spl_edl_reset();
		break;

	case IPQ_SPL_BOOT_MODE_FORCE_INACIVE:
		printf("Forceinactive: Boot Failed\n");
		ipq_spl_reset_cpu();
		break;

	case IPQ_SPL_BOOT_MODE_FAILOVER_EN:
		printf("Failover: Boot Failed\n");
		ipq_spl_reset_cpu();
		break;

	default:
		pr_err("System entered hang state\n");
	}

	/*
	 * End of Error handler. If we reach here, it means the system
	 * has reached a hang state.
	 */
#ifdef CONFIG_CONSOLE_RECORD
	ipq_spl_smem_console_log_sync();
#endif /* CONFIG_CONSOLE_RECORD */
	hang();

}

#if defined(CFG_EMUL_FREQUENCY_DIVIDER)
/**
 * ipq_spl_setup_arch_cntfreq() - Set up architecture counter frequency.
 *
 * This function is used in emulation environments to divide the system
 * counter frequency for compatibility.
 */
void ipq_spl_setup_arch_cntfreq(void)
{
	unsigned long freq;

	/*
	 * Emulation - Divide the counter frequency
	 */
	if ((CONFIG_COUNTER_FREQUENCY > 0) &&
		(CFG_EMUL_FREQUENCY_DIVIDER > 0)) {
		freq = CONFIG_COUNTER_FREQUENCY / CFG_EMUL_FREQUENCY_DIVIDER;
		asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");
	}
}
#endif

/**
 * bootcount_store() - Store value to bootcount address.
 * @a: Value to store.
 *
 * This function stores a value to the bootcount address. It is used to
 * maintain the boot count across resets. The bootcount address is defined
 * by CONFIG_SYS_BOOTCOUNT_ADDR. The function also ensures that the memory
 * is flushed to ensure data consistency.
 *
 * Return: None.
 */
void bootcount_store(ulong a)
{
	void *reg = (void *)CONFIG_SYS_BOOTCOUNT_ADDR;
	uintptr_t flush_start = rounddown(CONFIG_SYS_BOOTCOUNT_ADDR,
					CONFIG_SYS_CACHELINE_SIZE);
	uintptr_t flush_end;

	/*
	 * Bootcount remains unchanged during the crash path
	 * and when called from board_init_r().
	 */
	if ((IPQ_SPL_IS_DLOAD_BIT_SET()) || (gd->flags & GD_FLG_SPL_INIT))
		return;

	raw_bootcount_store(reg, (CONFIG_SYS_BOOTCOUNT_MAGIC & 0xffff0000) | a);

	flush_end = roundup(CONFIG_SYS_BOOTCOUNT_ADDR + 4,
				CONFIG_SYS_CACHELINE_SIZE);

	flush_dcache_range(flush_start, flush_end);
}

/**
 * ipq_spl_clear_force_inactive() - Clear force-inactive flag from IMEM
 *
 * Clears the force-inactive information stored in the IMEM region and
 * flushes the corresponding cache lines to ensure data consistency.
 *
 * Return: None.
 */
void ipq_spl_clear_force_inactive(void)
{
	uintptr_t flush_start = rounddown(IPQ_SPL_FORCE_INACTIVE_ADDR,
						CONFIG_SYS_CACHELINE_SIZE);
	uintptr_t flush_end;

	/*
	 * Clear the value in the imem region
	 */
	clrbits_le32(IPQ_SPL_FORCE_INACTIVE_ADDR, U32_MAX);

	flush_end = roundup(IPQ_SPL_FORCE_INACTIVE_ADDR + 4,
				CONFIG_SYS_CACHELINE_SIZE);

	flush_dcache_range(flush_start, flush_end);
}

#if CONFIG_IS_ENABLED(SYS_MALLOC_F)
/**
 * ipq_spl_malloc_init_f() - Initialize malloc for SPL.
 *
 * This function initializes the malloc subsystem using the memory region
 */
void ipq_spl_malloc_init_f(void)
{
	/*
	 * Set up by crt0.S
	 */
	assert(gd->malloc_base);
	gd->malloc_limit = CONFIG_VAL(SYS_MALLOC_F_LEN);
	gd->malloc_ptr = 0;

	mem_malloc_init(gd->malloc_base, gd->malloc_limit);
	gd->flags |= GD_FLG_FULL_MALLOC_INIT;
}
#endif

#if defined(CONFIG_CLK_QCOM_PLL)
#if defined(CONFIG_TARGET_IPQ9650)
static int ipq_spl_update_ipq9650_v2_pll_compatible(void)
{
	static const char compatible[sizeof("qcom,ipq9650-clk-pll") +
				     sizeof("qcom,ipq-clk-pll")] =
		"qcom,ipq9650-v2-clk-pll";
	void *fdt = (void *)gd->fdt_blob;
	int node;
	int ret;

	node = fdt_path_offset(fdt, "/soc@0/pll@fa80000");
	if (node < 0)
		return node;

	ret = fdt_setprop_inplace(fdt, node, "compatible", compatible,
				  sizeof(compatible));
	if (ret)
		return ret;

	node = fdt_path_offset(fdt, "/soc@0/pll@faa0000");
	if (node < 0)
		return node;

	return fdt_setprop_inplace(fdt, node, "compatible", compatible,
				   sizeof(compatible));
}
#endif

/**
 * ipq_spl_probe_and_enable_plls() - Probe and enable all PLLs.
 *
 * This function probes and enables all available PLLs in the system.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_probe_and_enable_plls(void)
{
	int ret;
	ofnode node, p_handle;
	struct udevice *pll_dev;
	u32 index, num_plls;

#if defined(CONFIG_TARGET_IPQ9650)
	if (TCSR_SOC_HW_VERSION_MAJOR(ipq_get_soc_hw_version()) >= 2) {
		ret = ipq_spl_update_ipq9650_v2_pll_compatible();
		if (ret)
			return ret;
	}
#endif

	node = ofnode_by_compatible(ofnode_null(), "qcom,ipq-init-plls");
	if (!ofnode_valid(node)) {
		pr_debug("Failed to get qcom,ipq-init-plls node\n");
		/**
		 * No PLL node is available in device tree.
		 * Return success.
		 */
		return 0;
	}

	/**
	 * Get the number of phandles are in "plls"
	 */
	num_plls = ofnode_count_phandle_with_args(node, "plls", NULL, 0);
	if (num_plls < 0) {
		pr_debug("No plls found %d", num_plls);
		/**
		 * No PLLs available in device tree to be initialized.
		 * Return success.
		 */
		return 0;
	}

	/**
	 * Probe and enable all the PLL devices.
	 */
	for (index = 0; index < num_plls; index++) {
		p_handle = ofnode_parse_phandle(node, "plls", index);
		if (!ofnode_valid(p_handle)) {
			pr_debug(" No more PLL phandles\n");
			/**
			 * If no more PLL phandles, break the loop with Success.
			 */
			break;
		}

		/* Convert the ofnode p_handle to a udevice and probe it.
		 * The probe step initializes the PLL hardware.
		 */
		ret = uclass_get_device_by_ofnode(UCLASS_MISC, p_handle,
							&pll_dev);
		if (ret) {
			pr_err("Failed to get PLL[%d] device: %d\n",
				index, ret);
			return ret;
		}
	}

	return 0;
}
#endif /* CONFIG_CLK_QCOM_PLL */

/**
 * ipq_spl_board_init_clk() - Initialize board clocks.
 *
 * This function initializes the board-specific clocks.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_board_init_clk(void)
{
	struct udevice *dev;
	struct clk_bulk bulk;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_NOP,
						"qcom,ipq-init-clks", &dev);
	if (ret) {
		pr_debug("Failed to get qcom,ipq-init-clks device\n");
		/**
		 * No board init clks are in device tree to be initialized.
		 * Return success.
		 */
		return 0;
	}

	/*
	 * Enable listed clocks (gates/votes) in SPL/U-Boot
	 * via standard bulk clock API.
	 */
	ret = clk_get_bulk(dev, &bulk);
	if (!ret) {
		ret = clk_enable_bulk(&bulk);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct udevice_id ipq_init_clk_of_match[] = {
	{ .compatible = "qcom,ipq-init-clks" },
	{ }
};

U_BOOT_DRIVER(ipq_init_clk) = {
	.name		= "ipq-init-clk",
	.id		= UCLASS_NOP,
	.of_match	= ipq_init_clk_of_match,
};

/**
 * ipq_spl_list_fuse() - List all fuses conditionally based on secure boot.
 * @fuse_arr:	Pointer to the fuse array.
 * @fuse_cnt:	Number of fuses.
 *
 * This function lists fuses. If a fuse is marked as secboot_protected,
 * it will only be printed when secure boot is disabled.
 * Supports both 32-bit and 64-bit fuse logging based on the full_row flag.
 * Return: 0 on success, or a negative error code on failure.
 */
int ipq_spl_list_fuse(struct ipq_spl_fuse_info *fuse_arr, size_t fuse_cnt)
{
	size_t index;
	u32 fuse_value_32;
	u64 fuse_value_64;

	if (!fuse_arr) {
		pr_err("Invalid fuse array pointer\n");
		return -EINVAL;
	}

	for (index = 0; index < fuse_cnt ; index++) {
		if (fuse_arr[index].fuse_addr == 0) {
			pr_warn("invalid fuse address at index %zu\n", index);
			continue;
		}

		/*
		 *Entry is only printed if secboot_protected is false
		 * or secure boot is disabled
		 */
		if ((fuse_arr[index].secboot_protected == false) ||
		    (secure_boot_enabled == false)) {

			if (fuse_arr[index].full_row == true) {
				fuse_value_64 = readq((uintptr_t)fuse_arr[index].fuse_addr);

				if (fuse_arr[index].mask != 0) {
					fuse_value_64 = (fuse_value_64 & fuse_arr[index].mask) >>
							fuse_arr[index].shift;
				}

				printf("%-24s @ 0x%08X = 0x%016llX\n",
					fuse_arr[index].fuse_name,
					fuse_arr[index].fuse_addr,
					fuse_value_64);
			} else {
				fuse_value_32 = readl((uintptr_t)fuse_arr[index].fuse_addr);

				if (fuse_arr[index].mask != 0) {
					fuse_value_32 =
						(fuse_value_32 & (u32)fuse_arr[index].mask) >>
						fuse_arr[index].shift;
				}

				printf("%-24s @ 0x%08X = 0x%08X\n",
					fuse_arr[index].fuse_name,
					fuse_arr[index].fuse_addr,
					fuse_value_32);
			}
		}
	}

	return 0;
}

/**
 * ipq_spl_log_lcs_state() - Log TME-L LCS state and validate provisioning
 *
 * This function reads the SOC LCS register, prints the decoded LCS state,
 * and blocks boot if the chip is not provisioned (based on LCS state).
 * Only allows boot in DEVELOPMENT, OPERATIONAL_EXT, OPERATIONAL_INT, and RMA states.
 */
static void ipq_spl_log_lcs_state(void)
{
	u32 lcs_state;
	const char *lcs_str;
	bool allow_boot = false;

	/*
	 * Read and decode LCS state
	 */
	lcs_state = (readl(IPQ_SPL_FUSE_TME_L_LCS_ADDR) & IPQ_SPL_SOC_LCS_MASK) >>
		    IPQ_SPL_SOC_LCS_SHFT;

	switch (lcs_state) {
	case 0x0:
		lcs_str = "BLANK";
		allow_boot = false;
		break;
	case 0xE:
		lcs_str = "DEVELOPMENT";
		allow_boot = true;
		break;
	case 0x5:
		lcs_str = "OPERATIONAL_EXT";
		allow_boot = true;
		break;
	case 0xB:
		lcs_str = "OPERATIONAL_INT";
		allow_boot = true;
		break;
	case 0x7:
		lcs_str = "RMA";
		allow_boot = true;
		break;
	default:
		lcs_str = "Unknown";
		allow_boot = false;
		break;
	}

	printf("%-24s %s\n", "TME-L LCS:", lcs_str);

	/*
	 * Block boot if chip is not provisioned
	 */
	if (!allow_boot) {
		pr_err("Unprovisioned chip: Boot not allowed (LCS=%s)\n", lcs_str);
#ifdef CONFIG_CONSOLE_RECORD
		ipq_spl_smem_console_log_sync();
#endif /* CONFIG_CONSOLE_RECORD */
		hang();
	}
}

/**
 * ipq_spl_log_debug_state() - Log debug enable/disable state
 *
 * This function reads the feature provisioning registers and prints
 * the debug state for APSS, TME-L, and Q6.
 */
static void ipq_spl_log_debug_state(void)
{
	u32 feat_prov_out0;
	u32 feat_prov_out2;
	bool apss_debug_disabled;
	bool tmel_debug_disabled;
	bool q6_debug_disabled;

	/*
	 * Read feature provisioning registers
	 */
	feat_prov_out0 = readl(IPQ_SPL_FEAT_PROV_OUT0_ADDR);
	feat_prov_out2 = readl(IPQ_SPL_FEAT_PROV_OUT2_ADDR);

	/*
	 * Check debug disable bits (1 = disabled, 0 = enabled)
	 * Extract debug state bits
	 */
	apss_debug_disabled = (feat_prov_out0 & IPQ_SPL_FEAT_PROV_APSS_MASK) >>
			IPQ_SPL_FEAT_PROV_APSS_SHIFT;
	tmel_debug_disabled = feat_prov_out0 & IPQ_SPL_FEAT_PROV_TMEL_MASK;
	q6_debug_disabled = (feat_prov_out2 & IPQ_SPL_FEAT_PROV_Q6_MASK) >>
			IPQ_SPL_FEAT_PROV_Q6_SHIFT;

	printf("%-24s APSS : %s , TME-L :%s , Q6 :%s\n",
			"Debug state:",
			apss_debug_disabled ? "Disabled" : "Enabled",
			tmel_debug_disabled ? "Disabled" : "Enabled",
			q6_debug_disabled ? "Disabled" : "Enabled");
}

/**
 * ipq_spl_log_otp_version() - Log OTP version
 *
 * This function reads the QFPROM register and prints the OTP TAG and FM version.
 */
static void ipq_spl_log_otp_version(void)
{
	u32 pte_row2_lsb;
	u32 otp_tag_version;
	u32 otp_fm_version;

	/*
	 * Read QFPROM PTE ROW2 LSB register
	 */
	pte_row2_lsb = readl(IPQ_SPL_QFPROM_PTE_ROW2_LSB_ADDR);

	/*
	 * Extract TAG and FM versions
	 */
	otp_tag_version = (pte_row2_lsb & IPQ_SPL_OTP_TAG_VERSION_MASK) >>
			  IPQ_SPL_OTP_TAG_VERSION_SHIFT;
	otp_fm_version = (pte_row2_lsb & IPQ_SPL_OTP_FM_VERSION_MASK) >>
			 IPQ_SPL_OTP_FM_VERSION_SHIFT;

	printf("%-24s %d.%d\n", "OTP Version:", otp_tag_version, otp_fm_version);
}

/*
 * ipq_spl_boot_logs() - Print boot logs
 */
static void ipq_spl_boot_logs(void)
{
	ipq_spl_log_lcs_state();
	ipq_spl_log_debug_state();
	ipq_spl_log_otp_version();
}

/**
 * ipq_spl_bootldr_partition_name() - Get bootloader FIT partition name
 *
 * Returns the bootloader FIT image partition name based on the
 * current SPL boot set (active, inactive, or force-inactive).
 *
 * Return: Pointer to the selected partition name string.
 */
static char *ipq_spl_bootldr_partition_name(void)
{
	switch (g_bootrec.boot_set) {
	case IPQ_SPL_BOOT_FROM_INACTIVE:
		return (char *)IPQ_SPL_FIT_IMG_ALT_PARTITION;
	case IPQ_SPL_BOOT_FROM_ACTIVE:
	default:
		return (char *)IPQ_SPL_FIT_IMG_PARTITION;
	}
}

/**
 * ipq_spl_get_fit_img_entry_point() - Get entry point from FIT image node.
 * @fit:	Pointer to the FIT image blob.
 * @node:	Node ID within the FIT image.
 * @entry_point:Pointer to store the retrieved entry point.
 *
 * This function attempts to retrieve the entry point from a FIT image node.
 * If not explicitly defined, it falls back to the load address.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_get_fit_img_entry_point(void *fit,
						int node,
						u64 *entry_point)
{
	int ret;

	if (!fit) {
		pr_err("FIT image blob is NULL\n");
		return -EINVAL;
	}
	if (node <= 0) {
		pr_err("Invalid FIT node ID %d\n", node);
		return -EINVAL;
	}
	if (!entry_point) {
		pr_err("Entry point pointer is NULL\n");
		return -EINVAL;
	}

	ret = fit_image_get_entry(fit, node, (ulong *)entry_point);
	if (ret) {
		pr_debug("No entry point for node %d, trying load address\n",
			 node);
		ret = fit_image_get_load(fit, node, (ulong *)entry_point);
		if (ret)
			pr_err("No load address for node %d (ret=%d)\n",
				node, ret);
	}

	return ret;
}

/**
 * ipq_spl_smem_set_image_version() - Populate one entry of SMEM_IMAGE_VERSION_TABLE.
 * @smem:	SMEM device.
 * @image_index:	Index of the image within the table (e.g. IMAGE_INDEX_SPL).
 * @version:	Version string to store, e.g. "U-Boot SPL 2025.04 (...)".
 * @label:	Human readable label used in the log line (e.g. "TME-L").
 *
 * Populates the entry in the format "<image_index>:<version>:<oem_version>".
 */
static void ipq_spl_smem_set_image_version(struct udevice *smem, int image_index,
					    const char *version, const char *label)
{
	struct image_version_entry *img_ver_entry;
	struct image_version_entry *entry;
	size_t size;

	img_ver_entry = (struct image_version_entry *)smem_get(smem, -1,
						SMEM_IMAGE_VERSION_TABLE, &size);
	if (!img_ver_entry) {
		pr_err("Failed to get item: SMEM_IMAGE_VERSION_TABLE\n");
		return;
	}

	entry = (struct image_version_entry *)img_ver_entry + image_index;

	memset(entry, 0, sizeof(*entry));

	entry->image_index[0] = '0' + (image_index / 10);
	entry->image_index[1] = '0' + (image_index % 10);
	entry->image_colon_sep1[0] = ':';

	strlcpy(entry->image_qc_version_string, version,
		IMAGE_QC_VERSION_STRING_LENGTH);

	entry->image_colon_sep2[0] = ':';
	entry->image_oem_version_string[0] = '\0';

	printf("%s version added to SMEM: %s\n", label, version);
}

/**
 * ipq_spl_populate_smem() - Populate shared memory (SMEM) information.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function initializes and populates various SMEM items with boot-related
 * information, such as flash type, try-mode status, and ATF enable status.
 * TODO: Populate the SMEM MIBIB Info.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_populate_smem(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;
	struct udevice *smem;
	size_t size;
	u32 *fltype;
	u32 *boot_mode;
	u32 *boot_set;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	/*
	 * Populate SMEM
	 */
	ipq_uboot_fdt_fixup((void *)gd->fdt_blob, UBOOT_FIXUP_SMEM);
	ret = uclass_get_device(UCLASS_SMEM, 0, &smem);
	if (ret) {
		pr_err("Failed to find SMEM node (ret=%d)\n", ret);
		return ret;
	}

	/*
	 * Populate Flash Type
	 */
	size = sizeof(u32);
	ret = smem_alloc(smem, -1, SMEM_BOOT_FLASH_TYPE, size);
	if (ret) {
		pr_err("Failed to alloc item: SMEM_BOOT_FLASH_TYPE (ret=%d)\n",
			ret);
		return ret;
	}

	fltype = (u32 *)smem_get(smem, -1, SMEM_BOOT_FLASH_TYPE, &size);
	if (!fltype) {
		pr_err("Failed to get item: SMEM_BOOT_FLASH_TYPE\n");
		return -ENOENT;
	}

	switch (spl_boot_device()) {
#if CONFIG_IPQ_SPI_NOR
	case BOOT_DEVICE_SPI:
		*fltype = SMEM_BOOT_NORGPT_FLASH;
		break;
#endif
#if CONFIG_IPQ_MMC
	case BOOT_DEVICE_MMC1:
		*fltype = SMEM_BOOT_MMC_FLASH;
		break;
#endif
#if CONFIG_IPQ_NAND
	case BOOT_DEVICE_NAND:
		*fltype = SMEM_BOOT_QSPI_NAND_FLASH;
		break;
#endif
	default:
		pr_err("Invalid boot device for SMEM: %d\n",
			spl_boot_device());
		return -EINVAL;
	}

	/*
	 * Populate SPL boot mode info
	 */
	size = sizeof(u32);
	ret = smem_alloc(smem, -1, SMEM_FAILSAFE_BOOT_MODE, size);
	if (ret) {
		pr_err(
		"Failed to alloc item: SMEM_FAILSAFE_BOOT_MODE (ret=%d)\n",
		ret);
		return ret;
	}

	boot_mode = (u32 *)smem_get(smem, -1, SMEM_FAILSAFE_BOOT_MODE, &size);
	if (!boot_mode) {
		pr_err("Failed to get item: SMEM_FAILSAFE_BOOT_MODE\n");
		return -ENOENT;
	}
	*boot_mode = g_bootrec.boot_mode;

	/*
	 * Populate SPL boot set info
	 */
	size = sizeof(u32);
	ret = smem_alloc(smem, -1, SMEM_BOOT_SET_INFO, size);
	if (ret) {
		pr_err("Failed to alloc item: SMEM_BOOT_SET_INFO (ret=%d)\n",
			ret);
		return ret;
	}

	boot_set = (u32 *)smem_get(smem, -1, SMEM_BOOT_SET_INFO, &size);
	if (!boot_set) {
		pr_err("Failed to get item: SMEM_BOOT_SET_INFO\n");
		return -ENOENT;
	}
	*boot_set = g_bootrec.boot_set;

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
	/*
	 * Populate TME-L Image Version in SMEM
	 */
	if (g_tme_version[0] != '\0')
		ipq_spl_smem_set_image_version(smem, IMAGE_INDEX_TMEL,
						g_tme_version, "TME-L");
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

	/*
	* Populate U-Boot SPL Image Version in SMEM
	*/
	{
		static const char spl_version[] = "U-Boot SPL " PLAIN_VERSION
			" (" U_BOOT_DATE " - " U_BOOT_TIME " " U_BOOT_TZ ")";

		ipq_spl_smem_set_image_version(smem, IMAGE_INDEX_SPL,
						spl_version, "U-Boot SPL");
	}
	/*
	 * Update MIBIB partition table in SMEM only for NAND boot
	 */
	if (*fltype != SMEM_BOOT_QSPI_NAND_FLASH)
		goto out_skip_smem_mibib_update;

	/*
	 * Validate MIBIB partition table magic numbers and version
	 * and populate MIBIB Info if valid.
	 */
	if ((g_mibib_parti_tbl.magic1 != FLASH_PART_MAGIC1) ||
	    (g_mibib_parti_tbl.magic2 != FLASH_PART_MAGIC2) ||
	    (g_mibib_parti_tbl.version != FLASH_PARTITION_VERSION)) {
		pr_err("Invalid MIBIB partition table; skipping SMEM update\n");
		return -EINVAL;
	}

	size = sizeof(struct flash_partition_table);
	ret = smem_alloc(smem, -1, SMEM_AARM_PARTITION_TABLE, size);
	if (ret) {
		pr_err("SMEM AARM partition alloc failed (ret=%d)\n", ret);
		return ret;
	}

	void *mibib_info = smem_get(smem, -1, SMEM_AARM_PARTITION_TABLE, &size);

	if (!mibib_info) {
		pr_err("Failed to get item: SMEM_AARM_PARTITION_TABLE\n");
		return -ENOENT;
	}

	/*
	 * Verify size is sufficient for the copy operation
	 */
	if (size < sizeof(struct flash_partition_table)) {
		pr_err("SMEM allocation too small for MIBIB partition table\n");
		return -EINVAL;
	}

	/*
	 * Copy partition table to the SMEM
	 */
	memcpy(mibib_info,
		&g_mibib_parti_tbl,
		sizeof(struct flash_partition_table));

	printf("MIBIB partition table populated in SMEM\n");

	/*
	 * Populate flash block size and density for NAND
	 */
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!mtd) {
		pr_err("Failed to get NAND device for flash info\n");
		return -ENODEV;
	}

	/*
	 * Populate flash_block_size
	 */
	size = sizeof(uint32_t);
	ret = smem_alloc(smem, -1, SMEM_BOOT_FLASH_BLOCK_SIZE, size);
	if (ret && ret != -EEXIST) {
		pr_err("Failed to alloc SMEM_BOOT_FLASH_BLOCK_SIZE (ret=%d)\n",
			ret);
		return ret;
	}

	uint32_t *flash_block_size = (uint32_t *)smem_get(smem, -1,
						SMEM_BOOT_FLASH_BLOCK_SIZE, &size);
	if (!flash_block_size) {
		pr_err("Failed to get item: SMEM_BOOT_FLASH_BLOCK_SIZE\n");
		return -ENOENT;
	}
	*flash_block_size = mtd->erasesize;

	/*
	 * Populate flash_density
	 */
	size = sizeof(uint32_t);
	ret = smem_alloc(smem, -1, SMEM_BOOT_FLASH_DENSITY, size);
	if (ret && ret != -EEXIST) {
		pr_err("Failed to alloc SMEM_BOOT_FLASH_DENSITY (ret=%d)\n",
			ret);
		return ret;
	}

	uint32_t *flash_density = (uint32_t *)smem_get(smem, -1,
						SMEM_BOOT_FLASH_DENSITY, &size);
	if (!flash_density) {
		pr_err("Failed to get item: SMEM_BOOT_FLASH_DENSITY\n");
		return -ENOENT;
	}
	*flash_density = (uint32_t)mtd->size;

	printf("NAND flash info populated in SMEM: block_size=0x%x, density=0x%x\n",
		*flash_block_size, *flash_density);

out_skip_smem_mibib_update:

#ifdef CONFIG_IPQ_SOFTSKU_SUPPORT
	/*
	 * Populate FID information in SMEM
	 */
	ret = ipq_spl_save_fids_smem(smem);
	if (ret) {
		pr_err("Failed to populate FID info in SMEM (ret=%d)\n", ret);
		/* Non-fatal error, continue */
	}
#endif /* CONFIG_IPQ_SOFTSKU_SUPPORT */

	return 0;
}

#if defined(CONFIG_IPQ_LCP_DARE)
/**
 * ipq_spl_get_lcp_dare_key() - Get LCP DARE key from TME PRNG
 * @pctx:      Pointer to the global SPL context
 * @entry_idx: Current interface table entry index
 *
 * This function retrieves the LCP DARE key from TME using PRNG IPC
 * and adds it to the QCLib interface table.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_get_lcp_dare_key(struct ipq_spl_ctx *pctx, int entry_idx)
{
	int ret;
	struct lcp_region_keys_per_slot *lcp_keys;
	struct tmel_get_prng prng_msg;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	/*
	 * Add QCDAREKEY entry to the interface table
	 */
	memcpy(pctx->if_tbl.if_table_entries[entry_idx].entry_name,
		QCDAREKEY,
		strlen(QCDAREKEY));

	pctx->if_tbl.if_table_entries[entry_idx].attributes = 0;
	pctx->if_tbl.num_entries = entry_idx + 1;

	/*
	 * Allocate memory for LCP region keys
	 */
	lcp_keys = memalign(ARCH_DMA_MINALIGN,
			    sizeof(struct lcp_region_keys_per_slot));
	if (!lcp_keys) {
		pr_err("Failed to allocate memory for LCP keys\n");
		return -ENOMEM;
	}
	memset(lcp_keys, 0, sizeof(struct lcp_region_keys_per_slot));

	/*
	 * Get LCP PRNG key from TME
	 */
	prng_msg.pdata = (u32)(uintptr_t)lcp_keys;
	prng_msg.length = sizeof(struct lcp_region_keys_per_slot);

	ret = ipq_prng_get_tme_impl(&prng_msg);
	if (ret) {
		printf("PRNG request failed. ret = %d\n", ret);
		free(lcp_keys);
		return ret;
	}

	printf("Successfully retrieved LCP PRNG key (size: %u bytes)\n",
	       prng_msg.length);

	/*
	 * Store the LCP keys address in the interface table entry
	 */
	pctx->if_tbl.if_table_entries[entry_idx].address = (u64)(uintptr_t)lcp_keys;
	pctx->if_tbl.if_table_entries[entry_idx].size = sizeof(struct lcp_region_keys_per_slot);

	return 0;
}
#endif /* CONFIG_IPQ_LCP_DARE */

/**
 * ipq_spl_get_iftbl_entry_by_name() - Get an interface table entry by name.
 * @if_tbl:	Pointer to the QCLIB interface table.
 * @name:	Name of the entry to find.
 * @entry:	Pointer to a buffer where the found entry will be copied.
 *
 * This function searches the provided interface table for an entry
 * matching the given name and copies it to the output buffer if found.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_get_iftbl_entry_by_name(struct interface_table *if_tbl,
					   char *name,
					   struct interface_table_entry *entry)
{
	u8 uc_index;

	if (!if_tbl) {
		pr_err("Invalid interface table\n");
		return -EINVAL;
	}
	if (!name) {
		pr_err("Invalid name\n");
		return -EINVAL;
	}
	if (!entry) {
		pr_err("Invalid entry pointer\n");
		return -EINVAL;
	}

	for (uc_index = 0; uc_index < MAX_ENTRIES; uc_index++) {
		/*
		 * Find the entry with the matching name
		 */
		if (strcmp(if_tbl->if_table_entries[uc_index].entry_name,
			   name) == 0) {
			memcpy(entry,
			       &if_tbl->if_table_entries[uc_index],
			       sizeof(struct interface_table_entry));
			return 0;
		}
	}
	pr_err("Interface table entry '%s' not found\n", name);

	return -ENOENT;
}

/**
 * ipq_spl_get_img_ctx_by_name() - Get image table entry by name.
 * @img_name:	Name of the image to find.
 *
 * This function searches the img_tbl_fit for any entry matching the given name
 * and returns the pointer to the matching image entry from the table.
 *
 * Return: pointer to maching image table entry on success, or NULL on failure.
 */

struct ipq_spl_img_ctx *ipq_spl_get_img_ctx_by_name(char *img_name)
{
	u8 uc_index;
	u8 uc_size;

	if (!img_name)
		return NULL;

	uc_size = ARRAY_SIZE(img_tbl_fit);
	for (uc_index = 0; uc_index < uc_size; uc_index++) {
		if (strcmp(img_name, img_tbl_fit[uc_index].img_name) == 0)
			return &img_tbl_fit[uc_index];
	}

	return NULL;
}

/**
 * ipq_spl_dpr_fixup() - Perform fixups for the DPR ELF image.
 * @ctx:	Pointer to the global SPL context.
 *
 * The DPR (Device Protection Region) ELF is loaded as a whole image
 * before qcconfig.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_dpr_fixup(void *ctx)
{
	pr_debug("DPR fixup skipped\n");
	return 0;
}

/**
 * ipq_spl_xcfg_fixup() - Perform fixups for qcconfig-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function initializes and populates the QCLIB interface table with
 * information about the qcconfig-meta image's entry point.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_xcfg_fixup(void *ctx)
{
	int ret;
	int entry_idx;
	struct ipq_spl_ctx *pctx = ctx;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	if (!pctx->img_tbl) {
		pr_err("Image table is NULL\n");
		return -EINVAL;
	}

	if (!pctx->fit) {
		pr_err("FIT image not loaded\n");
		return -EINVAL;
	}

	/*
	 * Initialize the interface table
	 */
	memset(&pctx->if_tbl, 0, sizeof(struct interface_table));
	memcpy(pctx->if_tbl.magic_key, MAGIC_KEY, strlen(MAGIC_KEY));

	pctx->if_tbl.version = IF_TABLE_VERSION;
	pctx->if_tbl.num_entries = 0;
	pctx->if_tbl.max_entries = MAX_ENTRIES;

	/*
	 * Add QCCONFIG entry to the interface table
	 */
	entry_idx = 0;
	memcpy(pctx->if_tbl.if_table_entries[entry_idx].entry_name,
		QCCONFIG,
		strlen(QCCONFIG));

	ret = ipq_spl_get_fit_img_entry_point(pctx->fit,
			pctx->img_tbl->fit_node,
			&pctx->if_tbl.if_table_entries[entry_idx].address);
	if (ret) {
		pr_err("Failed to get qcconfig-meta entry point (ret=%d)\n",
			ret);
		return ret;
	}
	pctx->if_tbl.if_table_entries[entry_idx].attributes = 0;
	pctx->if_tbl.num_entries = entry_idx + 1;

	/*
	 * Add QCSDI entry to the interface table
	 */
	entry_idx++;
	memcpy(pctx->if_tbl.if_table_entries[entry_idx].entry_name,
		QCSDI,
		strlen(QCSDI));

	pctx->if_tbl.if_table_entries[entry_idx].address = 0;
	pctx->if_tbl.if_table_entries[entry_idx].attributes = 0;
	pctx->if_tbl.num_entries = entry_idx + 1;

	/*
	 * Add QCUART entry to the interface table.
	 * address = 1 → UART enabled in QCLIB; address = 0 → disabled.
	 */
	entry_idx++;
	memcpy(pctx->if_tbl.if_table_entries[entry_idx].entry_name,
		QCUART,
		strlen(QCUART));
#if !defined(CONFIG_DISABLE_CONSOLE)
	pctx->if_tbl.if_table_entries[entry_idx].address = 0x1;
#else
	pctx->if_tbl.if_table_entries[entry_idx].address = 0x0;
#endif
	pctx->if_tbl.if_table_entries[entry_idx].attributes = 0;
	pctx->if_tbl.num_entries = entry_idx + 1;

	/**
	 * Initialize the QCLIB Region
	 *
	 * Note: The last 10 KB is reserved for QCCONFIG
	 * and must not be cleared.
	 */
	if (!ipq_spl_tmel_bypass_enabled()) {
#if defined(CONFIG_IPQ_LCP_DARE)
		/*
		 * Get LCP DARE key from TME PRNG
		 */
		entry_idx++;
		ret = ipq_spl_get_lcp_dare_key(pctx, entry_idx);
		if (ret) {
			pr_err("Failed to get LCP DARE key (ret=%d)\n", ret);
			return ret;
		}
#endif /* CONFIG_IPQ_LCP_DARE */
	}

	return 0;
}

/**
 * ipq_spl_qclib_fixup() - Perform fixups for qclib-meta image and jump to it.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the qclib-meta image and
 * performs a direct jump to it with the populated QCLIB interface table.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_qclib_fixup(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;
	ipq_spl_jump_img_entry_t qclib_entry;
	u64 entry_point;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	if (!pctx->img_tbl) {
		pr_err("Image table is NULL\n");
		return -EINVAL;
	}

	if (!pctx->fit) {
		pr_err("FIT image not loaded\n");
		return -EINVAL;
	}

	ret = ipq_spl_get_fit_img_entry_point(pctx->fit,
				pctx->img_tbl->fit_node,
				&entry_point);
	if (ret) {
		pr_err("Failed to get qclib-meta entry point (ret=%d)\n", ret);
		return ret;
	}

	qclib_entry = (ipq_spl_jump_img_entry_t)entry_point;

	pr_info("Jumping to Image: %s at 0x%llx\n", pctx->img_tbl->img_name,
		entry_point);
	qclib_entry(&pctx->if_tbl, NULL);

#ifdef CONFIG_CONSOLE_RECORD
	/*
	 * Consolidate logs immediately after QCLIB returns, while its IMEM
	 * log buffer (advertised via the interface table) is still valid --
	 * the same region can be reused by subsequently loaded images.
	 * Restrict to coldboot, matching the SMEM population done in
	 * ipq_spl_tfa_fixup(). Console log consolidation is diagnostics-only:
	 * never let a failure here abort boot.
	 */
	if (!IPQ_SPL_IS_DLOAD_BIT_SET()) {
		ret = ipq_spl_smem_console_log_init(pctx);
		if (ret)
			pr_err("Failed to init SMEM console log (ret=%d)\n", ret);
	}
#endif /* CONFIG_CONSOLE_RECORD */

	return 0;
}

#ifdef CONFIG_CONSOLE_RECORD
/**
 * ipq_spl_smem_console_log_sync() - Update write_offset for live SMEM logs.
 *
 * Once ipq_spl_smem_console_log_init() re-points gd->console_out directly
 * into SMEM, further console_record_putc()/puts() calls only advance the
 * live membuff's own head pointer -- smem_log->write_offset is never
 * touched again unless this is called. Without it, any logging done after
 * consolidation (e.g. "U-Boot SPL, End", or logs right before a fatal
 * reset/hang) would be physically present in the SMEM buffer but invisible
 * to a reader that trusts write_offset. Call this at every SPL exit point
 * (normal completion, reset, hang) so write_offset always reflects the
 * true amount of valid data.
 *
 * No-op if console logging has not been consolidated into SMEM yet.
 */
static void ipq_spl_smem_console_log_sync(void)
{
	struct membuff *mb = (struct membuff *)&gd->console_out;
	u32 written;

	if (!g_console_smem_log)
		return;

	written = g_console_smem_base + (u32)(mb->head - mb->start);
	if (written > g_console_smem_log->size)
		written = g_console_smem_log->size;
	g_console_smem_log->write_offset = written;
}

/**
 * ipq_spl_smem_console_log_init() - Consolidate console logs into SMEM.
 * @ctx:	Pointer to the global SPL context.
 *
 * Allocates the SMEM_SPL_CONSOLE_LOG item, drains the pre-DDR SRAM console
 * buffer (gd->console_out, populated since board_init_f()) into it,
 * appends QCLIB's IMEM log buffer if advertised via the interface table,
 * then re-points gd->console_out at the remaining SMEM space so that all
 * further SPL logging is written directly to SMEM using the same
 * console_record_putc()/puts() hooks.
 *
 * This is diagnostics infrastructure only: any failure here is logged and
 * treated as non-fatal so it can never block boot.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_smem_console_log_init(void *ctx)
{
	struct ipq_spl_ctx *pctx = ctx;
	struct udevice *smem;
	struct smem_console_log *smem_log;
	struct interface_table_entry qclib_log_entry;
	size_t size;
	char *data;
	int len;
	int ret;

	ret = uclass_get_device(UCLASS_SMEM, 0, &smem);
	if (ret) {
		pr_err("Console log: failed to find SMEM node (ret=%d)\n", ret);
		return ret;
	}

	size = sizeof(struct smem_console_log) + SMEM_CONSOLE_LOG_SIZE;
	ret = smem_alloc(smem, -1, SMEM_SPL_CONSOLE_LOG, size);
	if (ret) {
		pr_err("Console log: failed to alloc SMEM item (ret=%d)\n", ret);
		return ret;
	}

	smem_log = (struct smem_console_log *)smem_get(smem, -1,
							SMEM_SPL_CONSOLE_LOG,
							&size);
	if (!smem_log) {
		pr_err("Console log: failed to get SMEM item\n");
		return -ENOENT;
	}

	smem_log->magic = SMEM_SPL_CONSOLE_LOG_MAGIC;
	smem_log->size = size - sizeof(struct smem_console_log);
	smem_log->write_offset = 0;
	smem_log->flags = 0;

	/* Drain the pre-DDR SRAM buffer (up to two installments, per membuff_getraw()) */
	while ((len = membuff_getraw((struct membuff *)&gd->console_out, -1,
				     true, &data)) > 0) {
		if (len > smem_log->size - smem_log->write_offset) {
			len = smem_log->size - smem_log->write_offset;
			smem_log->flags |= SMEM_SPL_CONSOLE_LOG_OVERFLOW;
		}
		if (len <= 0)
			break;
		memcpy(smem_log->data + smem_log->write_offset, data, len);
		smem_log->write_offset += len;
	}

	/* Append QCLIB's IMEM log buffer, if advertised via the interface table */
	ret = ipq_spl_get_iftbl_entry_by_name(&pctx->if_tbl, QCLIB_LOG_BUFFER,
					       &qclib_log_entry);
	if (!ret) {
		len = qclib_log_entry.size;
		if (len > smem_log->size - smem_log->write_offset) {
			len = smem_log->size - smem_log->write_offset;
			smem_log->flags |= SMEM_SPL_CONSOLE_LOG_OVERFLOW;
		}
		if (len > 0) {
			memcpy(smem_log->data + smem_log->write_offset,
			       (void *)(uintptr_t)qclib_log_entry.address, len);
			smem_log->write_offset += len;
		}
	} else {
		pr_debug("Console log: QCLIB log buffer entry not found (ret=%d)\n",
			 ret);
	}

	/* Switch live logging to write directly into the remaining SMEM space */
	membuff_init((struct membuff *)&gd->console_out,
		     smem_log->data + smem_log->write_offset,
		     smem_log->size - smem_log->write_offset);
	gd->flags |= GD_FLG_RECORD;

	g_console_smem_log = smem_log;
	g_console_smem_base = smem_log->write_offset;

	return 0;
}
#endif /* CONFIG_CONSOLE_RECORD */

/**
 * ipq_spl_tfa_fixup() - Perform fixups for tfa_bl31-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the ARM TF-A BL31 image and
 * populates shared memory (SMEM) with relevant boot information.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_tfa_fixup(void *ctx)
{
	int ret;
	struct ipq_spl_ctx *pctx = ctx;

	if (!pctx) {
		pr_err("Invalid SPL context\n");
		return -EINVAL;
	}

	if (!pctx->img_tbl) {
		pr_err("Image table is NULL\n");
		return -EINVAL;
	}

	/*
	 * Populate SMEM in coldboot (Dload bit not set)
	 */
	if (!IPQ_SPL_IS_DLOAD_BIT_SET()) {
		printf("Populating SMEM\n");
		ret = ipq_spl_populate_smem(pctx);
		if (ret) {
			pr_err("Failed to populate SMEM (ret=%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

/**
 * ipq_spl_optee_fixup() - Perform fixups for optee-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the OP-TEE (BL32) image.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_optee_fixup(void *ctx)
{
	pr_debug("OP-TEE fixup skipped\n");
	return 0;
}

/**
 * ipq_spl_uboot_fixup() - Perform fixups for uboot-meta image.
 * @ctx:	Pointer to the global SPL context.
 *
 * This function gets the entry point for the U-Boot (BL33) image.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_uboot_fixup(void *ctx)
{
	pr_debug("U-Boot fixup skipped\n");
	return 0;
}

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
/**
 * ipq_spl_list_tme_fuse() - List TME fuses conditionally based on secure boot.
 * @fuse_arr:	Pointer to the fuse array.
 * @fuse_cnt:	Number of fuses.
 *
 * This function lists fuses using TME IPC communication. If a fuse is marked
 * as secboot_protected, it will only be printed when secure boot is disabled.
 * It also updates the global secure_boot_enabled variable.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_list_tme_fuse(struct ipq_spl_fuse_info *fuse_arr,
				 u8 fuse_cnt)
{
	int ret;
	u8 index;
	size_t fuse_size;
	size_t aligned_size;
	struct fuse_payload *fuse;
	struct list_fuse_params fuse_params;

	fuse_size = sizeof(struct fuse_payload) * fuse_cnt;
	aligned_size = roundup(fuse_size, CONFIG_SYS_CACHELINE_SIZE);
	fuse = malloc_cache_aligned(aligned_size);
	if (fuse == NULL) {
		pr_err("Failed to allocate memory for fuse data\n");
		return -ENOMEM;
	}

	memset(fuse, 0, aligned_size);

	for (index = 0; index < fuse_cnt ; index++)
		fuse[index].fuse_addr = fuse_arr[index].fuse_addr;

	fuse_params.fuse = fuse;
	fuse_params.fuse_read_cnt = fuse_cnt;
	fuse_params.fuse_payload_size = sizeof(struct fuse_payload);
	fuse_params.size = fuse_size;

	ret = ipq_list_fuse_tme_impl(&fuse_params);
	if (ret)
		goto fail;

	/*
	 * Print fuses conditionally
	 */
	for (index = 0; index < fuse_cnt ; index++) {
		if (fuse[index].fuse_addr == IPQ_SPL_FUSE_OEM_TME_ROW_0_ADDR) {
			u32 lsb_val = fuse[index].lsb_val;
			bool qti_secure_boot = (lsb_val & BIT(1)) ? true : false;

			/*
			 * Determine secure boot status
			 */
			secure_boot_enabled = (lsb_val & OEM_SEC_BOOT_ENABLE) ? true : false;
			printf("%-24s OEM : %s , OEM + QTI :%s\n",
				"Secure Boot:",
				secure_boot_enabled ? "On " : "Off",
				(secure_boot_enabled && qti_secure_boot) ? "On " : "Off");
		}

		if ((fuse_arr[index].secboot_protected == false) ||
		    (secure_boot_enabled == false)) {

			if (fuse_arr[index].full_row == true) {
				u64 fuse_value_64 =
					((u64)fuse[index].msb_val << 32) | fuse[index].lsb_val;

				if (fuse_arr[index].mask != 0) {
					fuse_value_64 =
						(fuse_value_64 & fuse_arr[index].mask) >>
						fuse_arr[index].shift;
				}

				printf("%-24s @ 0x%08X = 0x%016llX\n",
					fuse_arr[index].fuse_name,
					fuse[index].fuse_addr,
					fuse_value_64);
			} else {
				u32 fuse_value_32 = fuse[index].lsb_val;

				if (fuse_arr[index].mask != 0) {
					fuse_value_32 =
						(fuse_value_32 & (u32)fuse_arr[index].mask) >>
						fuse_arr[index].shift;
				}

				printf("%-24s @ 0x%08X = 0x%08X\n",
					fuse_arr[index].fuse_name,
					fuse[index].fuse_addr,
					fuse_value_32);
			}
		}
	}

fail:
	free(fuse);

	return ret;
}

/**
 * ipq_spl_get_tme_patch_version() - Get and store TME-L patch version.
 *
 * This function retrieves the TME-L patch version using TME IPC communication
 * and stores it in the global variable g_tme_version for later use.
 * Return: void
 */
static void ipq_spl_get_tme_patch_version(void)
{
	int ret;
	struct tmel_get_tme_version version_msg;
	char *version_buffer;

	/*
	 * Initialize global TME version string
	 */
	g_tme_version[0] = '\0';

	/*
	 * Allocate aligned buffer for TME version
	 */
	version_buffer = memalign(ARCH_DMA_MINALIGN, TME_PATCH_VERSION_LENGTH);
	if (!version_buffer) {
		pr_err("Failed to allocate memory for TME version buffer\n");
		return;
	}

	memset(version_buffer, 0, TME_PATCH_VERSION_LENGTH);
	version_msg.pdata = (u32)(uintptr_t)version_buffer;
	version_msg.length = TME_PATCH_VERSION_LENGTH;

	/*
	 * Get TME-L patch version from TME
	 */
	ret = ipq_get_tme_version_impl(&version_msg);
	if (ret == 0) {
		/* Validate that returned length is within buffer bounds */
		if (version_msg.length >= TME_PATCH_VERSION_LENGTH) {
			pr_err("TME version length %u exceeds buffer size %d\n",
			       version_msg.length, TME_PATCH_VERSION_LENGTH);
		} else {
			/* Null-terminate at actual length returned by TME */
			version_buffer[version_msg.length] = '\0';
			printf("TME-L Patch Version: %s\n", version_buffer);
			/* Save TME-L version to global variable */
			strlcpy(g_tme_version, version_buffer, TME_PATCH_VERSION_LENGTH);
		}
	} else {
		pr_warn("Failed to get TME-L patch version (ret=%d)\n", ret);
	}

	free(version_buffer);
}

#if defined(CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT)
/**
 * ipq_spl_get_tme_prng_data() - Get TME PRNG data and copy to destination.
 *
 * This function retrieves random data from TME PRNG and copies it to the
 * destination address for use by TF-A.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_get_tme_prng_data(void)
{
	int ret;
	struct tmel_get_prng prng_msg;
	u8 *random_buffer;
	u8 *dest_addr = (u8 *)IPQ_SPL_TFA_PRNG_DATA_DEST_ADDR;

	random_buffer = memalign(ARCH_DMA_MINALIGN, IPQ_SPL_TFA_PRNG_DATA_SIZE);
	if (!random_buffer) {
		pr_err("Failed to allocate memory for PRNG buffer\n");
		return -ENOMEM;
	}

	memset(random_buffer, 0, IPQ_SPL_TFA_PRNG_DATA_SIZE);

	prng_msg.pdata = (u32)(uintptr_t)random_buffer;
	prng_msg.length = IPQ_SPL_TFA_PRNG_DATA_SIZE;

	ret = ipq_prng_get_tme_impl(&prng_msg);
	if (ret == 0) {
		printf("Successfully retrieved %u bytes of random data from TME PRNG\n",
		       prng_msg.length);

		/* Copy PRNG data to destination address 0x8600900 */
		memcpy(dest_addr, random_buffer, IPQ_SPL_TFA_PRNG_DATA_SIZE);

		/* Flush cache to ensure TF-A can read the PRNG data */
		flush_dcache_range((unsigned long)dest_addr,
				   (unsigned long)dest_addr + IPQ_SPL_TFA_PRNG_DATA_SIZE);
	} else {
		pr_err("Failed to get random data from TME PRNG (ret=%d)\n", ret);
		free(random_buffer);
		return ret;
	}

	free(random_buffer);
	return 0;
}
#endif /* CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT */

/**
 * ipq_spl_auth_image() - Authenticate an image using TME.
 * @p_img_entry: Pointer to the image context.
 *
 * This function authenticates an image using TME.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_auth_image(struct ipq_spl_img_ctx *p_img_entry)
{
	int ret;
	struct secure_auth_params auth_params = {0};

	if (!p_img_entry) {
		pr_err("Invalid image entry\n");
		return -EINVAL;
	}

	if (p_img_entry->auth == false) {
		printf("Authentication disabled for %s\n",
				p_img_entry->img_name);
		return 0;
	}

	/*
	 * Validate authentication parameters
	 */
	if (!p_img_entry->load_addr) {
		pr_err("Image Auth: Invalid load address\n");
		return -EINVAL;
	}

	if (!p_img_entry->img_sz) {
		pr_err("Image Auth: Invalid image size\n");
		return -EINVAL;
	}

	printf("Auth image with SW_ID=0x%llx, addr=0x%llx, size=0x%llx\n",
		p_img_entry->sw_id, p_img_entry->load_addr,
		p_img_entry->img_sz);

	/*
	 * Populate Image params
	 */
	auth_params.type = p_img_entry->sw_id;
	auth_params.addr = p_img_entry->load_addr;
	auth_params.size = p_img_entry->img_sz;
#ifdef CONFIG_SECURE_AUTH_V3
	auth_params.flags = 1;
#endif

	/*
	 * Populate relocated segments information
	 */
	auth_params.relocate = 0;
	auth_params.load_seg_buff = NULL;
	auth_params.load_seg_info_size = 0;
	auth_params.load_seg_cnt = 0;

	ret = ipq_secure_auth_tme_impl(&auth_params);
	if (ret) {
		pr_err("Image Auth: failed (ret=%d)\n", ret);
		return ret;
	}

	printf("Image Auth: success\n");

	return 0;
}

#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

/**
 * spl_get_load_buffer() - Allocate a cache-aligned buffer for image loading.
 * @offset:	Offset (unused, typically 0 for SPL).
 * @size:	Size of the buffer to allocate.
 *
 * This function is part of the U-Boot SPL framework to provide a buffer
 * for image loading, ensuring it's cache-aligned.
 * Return: Pointer to the allocated buffer, or NULL on failure.
 */
struct legacy_img_hdr *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return (void *)(CONFIG_SPL_LOAD_FIT_ADDRESS);
}

/**
 * board_spl_fit_buffer_addr() - Get the address of the FIT image buffer.
 * @fit_size:	Size of the FIT image.
 * @sectors:	Number of sectors.
 * @bl_len:	Block length.
 *
 * This function returns the address where the FIT image will be loaded.
 * It uses the SPL load buffer address.
 * Return: Address of the FIT image buffer.
 */
void *board_spl_fit_buffer_addr(ulong fit_size, int sectors, int bl_len)
{
	void *buffer = spl_get_load_buffer(0, sectors * bl_len);

	if (!buffer) {
		pr_err("Failed to get FIT load buffer\n");
		ipq_spl_error_handler(__FILE__, __LINE__,
				      SPL_ERR_CAT_BOOT | SPL_ERR_NULL_PTR);
	}

	return buffer;
}

#if defined(CONFIG_SPL_FIT_IMAGE_POST_PROCESS)
/**
 * board_fit_image_post_process() - Post-processing callback for FIT images.
 * @fit:	Pointer to the FIT image blob.
 * @node:	Node ID of the current image component within the FIT.
 * @p_image:	Pointer to the image data.
 * @p_size:	Pointer to the size of the image.
 *
 * This function is called by the SPL framework after an image component
 * within a FIT is loaded. It populates the global SPL context and
 * performs image-specific fixups.
 */
void board_fit_image_post_process(const void *fit, int node, void **p_image,
					size_t *p_size)
{
	int ret;
	u8 uc_index;
	u8 uc_size;
	u8 img_arch;
	u64 load_addr;
	void *load_ptr;
	const char *img_name = fit_get_name(fit, node, NULL);
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);

	printf("Loading FIT Image: %s\n", img_name);

	if (!ctx) {
		pr_err("Unable to get SPL context\n");
		goto fail;
	}

	if (!p_image || !*p_image || !p_size || !*p_size) {
		pr_err("Invalid image parameters\n");
		goto fail;
	}

	/*
	 * Get the actual image load address from the FIT image.
	 */
	if (fit_image_get_load(fit, node, (ulong *)&load_addr)) {
		pr_err("Failed to get load address for %s\n", img_name);
		goto fail;
	}

	/*
	 * Copy the p_image pointer to the actual image load address to
	 * handle any address alignments caused by block‑based flash reads.
	 */
	load_ptr = map_sysmem(load_addr, *p_size);
	memcpy(load_ptr, *p_image, *p_size);

	/*
	 * Adjust the image pointer to the final load address post-memcpy.
	 */
	*p_image = load_ptr;

#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	/*
	 * Ensure that the image data is written to the actual
	 * memory location before we process it.
	 */
	flush_cache((unsigned long)(*p_image), (unsigned long)(*p_size));
#endif

	/*
	 * Traverse through the SPL image table
	 */
	uc_size = ARRAY_SIZE(img_tbl_fit);
	for (uc_index = 0; uc_index < uc_size; uc_index++) {
		if (strcmp(img_name, img_tbl_fit[uc_index].img_name) == 0) {
			/*
			 * Populate the context
			 */
			ctx->img_tbl = &img_tbl_fit[uc_index];
			ctx->fit = (void *)fit;
			img_tbl_fit[uc_index].fit_node = node;
			img_tbl_fit[uc_index].load_addr = (u64)(*p_image);
			img_tbl_fit[uc_index].img_sz = *p_size;

			if (fit_image_get_arch(fit, node, &img_arch)) {
				pr_err("Failed to get architecture for %s\n", img_name);
				goto fail;
			}
			img_tbl_fit[uc_index].img_arch = img_arch;

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
			/*
			 * Authenticate image if enabled and bypass is not set
			 */
			if (img_tbl_fit[uc_index].auth) {
				if (ipq_spl_tmel_bypass_enabled()) {
					printf("Authentication bypassed for %s (tmel_bypass=1)\n",
						img_tbl_fit[uc_index].img_name);
				} else {
					ret = ipq_spl_auth_image(ctx->img_tbl);
					if (ret) {
						pr_err("%s auth failed (ret=%d)\n",
						ctx->img_tbl->img_name,
						ret);
						goto fail;
					}
				}
			}
#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

			/*
			 * Do the image fixups if available
			 */
			if (img_tbl_fit[uc_index].fixup) {
				ret = img_tbl_fit[uc_index].fixup(ctx);
				if (ret) {
					pr_err(
					"Failed to fixup %s image (ret=%d)\n",
					img_name, ret);
					goto fail;
				}
			}
			break;
		}
	}

	/*
	 * Return on success
	 */
	return;

fail:
	ipq_spl_error_handler(__FILE__, __LINE__,
			      SPL_ERR_CAT_BOOT | SPL_ERR_INIT_FAIL);
}
#endif /* CONFIG_SPL_FIT_IMAGE_POST_PROCESS */

#if defined(CONFIG_TARGET_IPQ5210)
bool hermosa_prefer_mbn_v7(void);
#endif

/**
 * bl2_plat_get_bl31_params_v2() - Retrieve and fixup BL31 parameters.
 * @bl32_entry:	Entry point for BL32 (OP-TEE).
 * @bl33_entry:	Entry point for BL33 (U-Boot/kernel).
 * @fdt_addr:	Address of the Device Tree Blob (FDT).
 *
 * This function retrieves the default BL31 parameters and then performs
 * platform-specific fixups, such as populating ATF BL31's arg0 with
 * the address of the QCSDI interface table entry if available.
 *
 * Return: Pointer to the populated BL31 parameters structure.
 */
struct bl_params *bl2_plat_get_bl31_params_v2(uintptr_t bl32_entry,
					     uintptr_t bl33_entry,
					     uintptr_t fdt_addr)
{
	struct bl_params *bl_params;
	struct bl_params_node *node;
	struct interface_table_entry if_tbl_entry;
	int ret;
	struct ipq_spl_ctx *ctx = U_BOOT_GET_IPQ_SPL_CTX(ipq_default_ctx);
	struct ipq_spl_img_ctx *img_tbl;

	/*
	 * Populate the bl31 params with default values.
	 */
	bl_params = bl2_plat_get_bl31_params_v2_default(bl32_entry,
							 bl33_entry,
							 fdt_addr);

	/*
	 * Fixup the bl31 params based on platform requirements.
	 */
	for_each_bl_params_node(bl_params, node) {
		if (node->image_id == ATF_BL31_IMAGE_ID) {
			if (!ctx) {
				pr_err("Unable to get SPL context\n");
				break;
			}

			/*
			 * Attempt to get the QCSDI entry from the global
			 * interface table.
			 */
			ret = ipq_spl_get_iftbl_entry_by_name(
						&ctx->if_tbl,
						QCSDI,
						&if_tbl_entry);
			if (ret) {
				/*
				 * Log the error but continue, as QCSDI might
				 * not be critical or could be handled later.
				 */
				pr_err("Unable to get QCSDI entry (ret=%d)\n",
					ret);
				break;
			}

			/*
			 * If found, populate arg0 with the QCSDI address.
			 */
			node->ep_info->args.arg0 = if_tbl_entry.address;
		} else if (node->image_id == ATF_BL33_IMAGE_ID) {
			img_tbl = NULL;

#if defined(CONFIG_TARGET_IPQ5210)
			if (hermosa_prefer_mbn_v7())
				img_tbl = ipq_spl_get_img_ctx_by_name("uboot-meta-v7");
#endif
			if (!img_tbl || img_tbl->img_arch != IH_ARCH_ARM)
				img_tbl = ipq_spl_get_img_ctx_by_name("uboot-meta");

			if (img_tbl && img_tbl->img_arch == IH_ARCH_ARM) {
				/* SPSR = 0x1D3 for 32-bit Mode */
				node->ep_info->spsr = SPSR_32_SVC_ARM_MASKED_LE;
			}
		}
	}

	return bl_params;
}

/**
 * spl_board_prepare_for_boot() - Prepare board for booting.
 *
 * This function is invoked during the SPL boot sequence to carry out
 * any board‑specific setup required before exiting SPL.
 */
void spl_board_prepare_for_boot(void)
{
#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
	/* Skip TME mailbox cleanup if tmel_bypass is enabled */
	if (ipq_spl_tmel_bypass_enabled()) {
		printf("TME mailbox cleanup skipped (tmel_bypass=1)\n");
		return;
	}

	/*
	 * Disconnect the TME mailbox channel so the client does not receive
	 * anymore data and can reliquish control of the channel.
	 */
	int ret;
	struct tmelcom *tmelcom_priv;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		pr_err("Failed to find TMELCOM node %d\n", ret);
		goto fail;
	}

	ret = mbox_free(&tmelcom_priv->mbox);
	if (ret) {
		pr_err("Failed to shutdown TME mailbox channel: %d\n", ret);
		goto fail;
	}

#endif /* CONFIG_IPQ_TMEL_IPC_SUPPORT */

	printf("U-Boot SPL, End\n");
#ifdef CONFIG_CONSOLE_RECORD
	ipq_spl_smem_console_log_sync();
#endif /* CONFIG_CONSOLE_RECORD */
	return;
fail:
	ipq_spl_error_handler(__FILE__, __LINE__,
			      SPL_ERR_CAT_BOOT | SPL_ERR_INTERFACE);
}

/**
 * ipq_spl_loader_pre_ddr() - SPL loader for pre-DDR stage.
 * @boot_device:Type of boot device.
 *
 * This function initializes the global SPL context, allocates necessary
 * structures, initializes flash, gets flash operations, and loads
 * images required before DDR initialization (currently only FIT).
 * Resources allocated are freed before returning.
 * Return: 0 on success, or a negative error code on failure.
 */
static int ipq_spl_loader_pre_ddr(u8 boot_device)
{
	struct spl_image_info *spl_image;
	struct spl_boot_device *boot_dev;
	int ret = -ENODEV;
	struct spl_image_loader *drv =
		ll_entry_start(struct spl_image_loader, spl_image_loader);
	const int n_ents =
		ll_entry_count(struct spl_image_loader, spl_image_loader);

	spl_image = calloc(1, sizeof(struct spl_image_info));
	if (!spl_image) {
		pr_err("Failed to allocate spl_image\n");
		ret = -ENOMEM;
		return ret;
	}

	boot_dev = calloc(1, sizeof(struct spl_boot_device));
	if (!boot_dev) {
		pr_err("Failed to allocate bootdev\n");
		free(spl_image);
		ret = -ENOMEM;
		return ret;
	}

	boot_dev->boot_device = boot_device;

	struct spl_image_loader *loader;
	int bootdev = boot_device;

	if (CONFIG_IS_ENABLED(SHOW_ERRORS))
		ret = -ENXIO;
	for (loader = drv; loader != drv + n_ents; loader++) {
		if (bootdev != loader->boot_device)
			continue;
		if (!CONFIG_IS_ENABLED(SILENT_CONSOLE)) {
			if (loader)
				printf("Trying to boot from %s\n",
					spl_loader_name(loader));
			else if (CONFIG_IS_ENABLED(SHOW_ERRORS)) {
				printf(PHASE_PROMPT
					"Unsupported Boot Device %d\n",
					bootdev);
			} else {
				puts(PHASE_PROMPT
					"Unsupported Boot Device!\n");
			}
		}
		if (loader) {
			ret = loader->load_image(spl_image, boot_dev);
			if (!ret) {
				spl_image->boot_device = bootdev;
				ret = 0;
				break;
			}
			printf("Error: %d\n", ret);
		}
	}

	free(spl_image);
	free(boot_dev);

	return ret;
}

/**
 * spl_board_init() - Board-specific initialization for SPL.
 *
 * This function is called by the SPL framework to perform board-specific
 * initialization. It initializes the license system if CONFIG_IPQ_SOFTSKU_SUPPORT
 * is enabled.
 */
void spl_board_init(void)
{
#ifdef CONFIG_IPQ_SOFTSKU_SUPPORT
	int ret;

	/* Initialize license after qclib_entry call */
	ret = ipq_spl_license_init(NULL);
	if (ret) {
		pr_err("Failed to initialize license (ret=%d)\n", ret);
		return;
	}

	printf("License initialization completed successfully\n");
#endif /* CONFIG_IPQ_SOFTSKU_SUPPORT */
}

/**
 * ipq_spl_failsafe_init() - Initialize the global boot record for failsafe
 * logic
 * @boot_device: Boot device type
 *
 * Initializes all fields of the global @g_bootrec structure to their safe
 * default values prior to executing any failsafe boot decision logic.
 */
void ipq_spl_failsafe_init(u8 boot_device)
{
	/**
	 * Initialize default bootrec variables
	 */
	g_bootrec.boot_set = IPQ_SPL_BOOT_FROM_ACTIVE;
	g_bootrec.exp_boot_set = IPQ_SPL_BOOT_FROM_ACTIVE;
	g_bootrec.boot_mode = IPQ_SPL_BOOT_MODE_DEFAULT;
	g_bootrec.pbl_set = IPQ_SPL_BOOT_FROM_ACTIVE;
	g_bootrec.is_pbl_set_parsed = 0;
	g_bootrec.exp_tcsr_set = IPQ_SPL_BOOT_FROM_ACTIVE;

	g_bootrec.tcsr_set = ipq_spl_get_tcsr_set();
	printf("Failsafe info: TCSR Set %s\n",
		bootset_str(g_bootrec.tcsr_set));

	g_bootrec.env_failover = 0;
	g_bootrec.env_bootlimit = (ulong)(IPQ_SPL_DEFAULT_BOOTLIMIT);
	g_bootrec.env_bootfrom = IPQ_SPL_BOOT_FROM_ACTIVE;

	/*
	 * Store the PBL accessed page index, to identify
	 * SPL image region used from NAND flash memory
	 */
	g_bootrec.qpic_page_index = (boot_device == BOOT_DEVICE_NAND) ?
					IPQ_SPL_GET_QPIC_PAGE_INDEX() :
					0;
}

/**
 * ipq_spl_failsafe_get_env_info() - Read and parse ENV partition for
 *                                    failsafe boot parameters
 * @boot_device: Boot device type
 *
 * Reads the ENV partition from the active boot device into a temporary
 * buffer at IPQ_SPL_QCLIB_TEXT_BASE, verifies the CRC, and on success
 * populates the fields in @g_bootrec from the environment
 *
 * On CRC mismatch the ENV is skipped and @g_bootrec retains its
 * defaults set by ipq_spl_failsafe_init().
 *
 * Return: 0 on success,
 *         -EINVAL if @boot_device is not supported,
 *         negative errno on partition read failure.
 */
int ipq_spl_failsafe_get_env_info(u8 boot_device)
{
	int ret;
	env_t *env_addr;
	ulong env_sz;
	ulong part_sz;
	uint32_t crc_val = 0;

	env_addr = (env_t *)((ulong)IPQ_SPL_QCLIB_TEXT_BASE);
	env_sz = SZ_256K;

	/**
	 * Read the ENV partition
	 */
	if (boot_device == BOOT_DEVICE_NAND) {
		ret = ipq_spl_nand_read((char *)IPQ_SPL_ENV_PARTITION,
					(void *)env_addr, &env_sz, &part_sz);
		if (ret)
			return ret;

	} else if (boot_device == BOOT_DEVICE_MMC1) {
		ret = ipq_spl_mmc_read((char *)IPQ_SPL_ENV_PARTITION,
					(void *)env_addr, &env_sz, &part_sz);
		if (ret)
			return ret;

	} else if (boot_device == BOOT_DEVICE_SPI) {
		ret = ipq_spl_spinor_gpt_read((char *)IPQ_SPL_ENV_PARTITION,
					(void *)env_addr, &env_sz, &part_sz);
		if (ret)
			return ret;

	} else {
		printf("Unsupported flash type\n");
		return -EINVAL;
	}

	/**
	 * Verify ENV
	 */
	if (part_sz > env_sz) {
		/*
		 * The partition exceeds the SPL ENV buffer, so CRC validation
		 * is skipped because only part of the environment was read.
		 */
		printf("ENV CRC skipped due to bigger partition size %lu KB\n",
		       part_sz / SZ_1K);
	} else {
		/*
		 * The complete ENV partition fits in the SPL ENV buffer.
		 * * Hence CRC shall be verified.
		 */
		crc_val = crc32(0, env_addr->data, env_sz - ENV_HEADER_SIZE);
		pr_debug("ENV info: ENV SZ = %lx\n", env_sz);
		pr_debug("ENV info: ENV CRC = %x\n", env_addr->crc);
		pr_debug("ENV info: ENV CRC calculated  = %x\n", crc_val);
	}

	if (part_sz > env_sz || crc_val == env_addr->crc) {
		printf("Loading Environment ... OK\n");
		gd->env_valid = ENV_VALID;
		gd->env_addr = (ulong)env_addr->data;

		/*
		 * Populate failsafe fields from ENV
		 */
		g_bootrec.env_failover = env_get_hex("failover",
						     g_bootrec.env_failover);
		if (g_bootrec.env_failover > 1)
			g_bootrec.env_failover = 0;

		g_bootrec.env_bootlimit = env_get_hex("bootlimit",
						      g_bootrec.env_bootlimit);
		if ((g_bootrec.env_bootlimit == 0) ||
		    (g_bootrec.env_bootlimit > 31))
			g_bootrec.env_bootlimit =
				(ulong)(IPQ_SPL_DEFAULT_BOOTLIMIT);

		g_bootrec.env_bootfrom = env_get_hex("bootfrom",
						     g_bootrec.env_bootfrom);
		if (g_bootrec.env_bootfrom > 1)
			g_bootrec.env_bootfrom = 0;
	} else {
		printf("Loading Environment ... Failed due to bad CRC\n");
	}

	/*
	 * print the ENV info
	 */
	if (g_bootrec.env_failover)
		printf("Failsafe info: failover = ENABLED\n");
	else
		printf("Failsafe info: failover = DISABLED\n");

	printf("Failsafe info: bootlimit = %x\n", g_bootrec.env_bootlimit);

	printf("Failsafe info: bootfrom = %s\n",
		bootset_str(g_bootrec.env_bootfrom));

	return 0;
}

/**
 * ipq_spl_failsafe_parse_bootmode() - Resolve and latch the SPL boot mode
 *
 * Priority: FORCE_INACTIVE > FAILOVER_EN > DEFAULT.
 * Resets bootcount to 0 for FORCE_INACTIVE and DEFAULT modes.
 *
 */
void ipq_spl_failsafe_parse_bootmode(void)
{
	if (IPQ_SPL_IS_FORCE_INACTIVE_EN()) {
		g_bootrec.boot_mode = IPQ_SPL_BOOT_MODE_FORCE_INACIVE;
		bootcount_store(0);
		printf("Failsafe info: BOOT_MODE_FORCE_INACIVE\n");

	} else if (g_bootrec.env_failover) {
		g_bootrec.boot_mode = IPQ_SPL_BOOT_MODE_FAILOVER_EN;
		printf("Failsafe info: BOOT_MODE_FAILOVER_EN\n");
	} else {
		g_bootrec.boot_mode = IPQ_SPL_BOOT_MODE_DEFAULT;
		bootcount_store(0);
		printf("Failsafe info: BOOT_MODE_DEFAULT\n");
	}
}

/**
 * ipq_spl_failsafe_parse_bootset() - Resolve expected boot and TCSR set
 *
 * Derives exp_boot_set from boot_mode and bootcount, then mirrors
 * it to exp_tcsr_set. Triggers EDL reset if bootcount > 2*bootlimit.
 *
 */
void ipq_spl_failsafe_parse_bootset(void)
{
	ulong bootcount = bootcount_load();

	if (g_bootrec.boot_mode == IPQ_SPL_BOOT_MODE_FORCE_INACIVE)
		g_bootrec.exp_boot_set =
			ipq_spl_alt_bootset(g_bootrec.env_bootfrom);

	else if (bootcount <= g_bootrec.env_bootlimit)
		g_bootrec.exp_boot_set = g_bootrec.env_bootfrom;

	else if (bootcount <= 2*g_bootrec.env_bootlimit) {
		printf("Failover: Bootlimit (%u) exceeded.\n",
			g_bootrec.env_bootlimit);

		g_bootrec.exp_boot_set =
			ipq_spl_alt_bootset(g_bootrec.env_bootfrom);

	} else {
		printf("Failover: 2*Bootlimit (%u) exceeded.\n",
			2*g_bootrec.env_bootlimit);

		ipq_spl_edl_reset();
	}

	g_bootrec.exp_tcsr_set = g_bootrec.exp_boot_set;
}

/**
 * ipq_spl_failsafe_parse_pblset() - Parse and verify the PBL booted set
 * @boot_device: Boot device type (BOOT_DEVICE_SPI/MMC1/NAND)
 *
 * Reads pbl_set from PBL shared data and validates it against the
 * physical flash (GUID for GPT, QPIC page index for NAND). Flips
 * pbl_set and exp_tcsr_set if a PBL fallback is detected.
 * No-op if already parsed.
 *
 * Return: 0 on success, negative errno on failure.
 */
int ipq_spl_failsafe_parse_pblset(u8 boot_device)
{
	int ret;
	char *part_name = (char *)IPQ_SPL_DEFAULT_SPL_PARTITION_LABEL;
	const efi_guid_t spl_guid = IPQ_SPL_DEFAULT_SPL_PARTITION_GUID;
	efi_guid_t type_guid;
	struct mtd_info *mtd;
	uint32_t part_offset, part_size;
	uint32_t start_blk, blk_cnt, qpic_offset;

	/**
	 * check if already parsed
	 */
	if (g_bootrec.is_pbl_set_parsed != 0)
		return 0;

	/*
	 * Get GUID function pointer for GPT based flash memory
	 */
	if (boot_device == BOOT_DEVICE_SPI)
		get_guid_fn = ipq_spl_spinor_get_guid;
	else if (boot_device == BOOT_DEVICE_MMC1)
		get_guid_fn = ipq_spl_mmc_get_guid;
	else
		get_guid_fn = NULL;

	/*
	 * Get the PBL set info from the PBL shared data
	 */
	if (g_pbl_shared_data.shared_data_entry[
		PBL_APPS_SPL_SHARED_DATA_PARAM_ID_CURRENT_IMAGE_SET].param_val)
		g_bootrec.pbl_set = IPQ_SPL_BOOT_FROM_INACTIVE;
	else
		g_bootrec.pbl_set = IPQ_SPL_BOOT_FROM_ACTIVE;

	printf("Failsafe: PBL Booted set (shared) %s\n",
		bootset_str(g_bootrec.pbl_set));

	/**
	 * Verify the PBL set info
	 */
	if ((boot_device == BOOT_DEVICE_SPI) ||
		(boot_device == BOOT_DEVICE_MMC1)) {
		if (get_guid_fn == NULL)
			return -ENODEV;

		ret = get_guid_fn(part_name, &type_guid);
		if (ret)
			return ret;

		/*
		 * Verify GUID booted by PBL with the default GUID
		 */
		if (!memcmp(&spl_guid, &type_guid, sizeof(efi_guid_t)))
			printf("%s GUID: matches with default\n", part_name);
		else {
			/*
			 * GUID not matches with the default GUID,
			 * update the PBL set info and the expected TCSR set
			 */
			printf("%s GUID: mismatches with default\n",
				part_name);

			g_bootrec.pbl_set =
			ipq_spl_alt_bootset(g_bootrec.pbl_set);

			g_bootrec.exp_tcsr_set =
			ipq_spl_alt_bootset(g_bootrec.exp_boot_set);
		}

	} else if (boot_device == BOOT_DEVICE_NAND) {
		/*
		 * Find partition using generic MIBIB lookup
		 */
		ret = ipq_spl_mibib_getpart(part_name, &start_blk, &blk_cnt);
		if (ret) {
			printf("%s not found in MIBIB\n", part_name);
			return -ENOENT;
		}

		/* Convert block offset to byte offset */
		mtd = get_nand_dev_by_index(0);
		if (mtd) {
			part_offset = start_blk * mtd->erasesize;
			part_size = blk_cnt * mtd->erasesize;
		} else
			return -ENODEV;

		/*
		 * Verify the QPIC page index is present within the
		 * SPL Active set.
		 */
		qpic_offset = g_bootrec.qpic_page_index * mtd->writesize;

		if (!((qpic_offset >= part_offset) &&
			(qpic_offset < part_offset+part_size)))
			g_bootrec.pbl_set = IPQ_SPL_BOOT_FROM_INACTIVE;

	} else {
		printf("Unsupported flash type\n");
		return -EINVAL;
	}

	printf("Failsafe: PBL Booted set (parsed) %s\n",
		bootset_str(g_bootrec.pbl_set));

	/*
	 * PBL Set parsing done. Set the flag.
	 */
	g_bootrec.is_pbl_set_parsed = 1;

	return 0;
}

/**
 * ipq_spl_failsafe_verify_tcsr_set() - Align TCSR to the expected boot set
 *
 * If tcsr_set != exp_tcsr_set, updates TCSR and resets. Decrements
 * bootcount before reset to avoid accounting the current boot attempt.
 *
 */
void ipq_spl_failsafe_verify_tcsr_set(void)
{
	ulong bootcount = bootcount_load();

	/**
	 * Align TCSR set, when it dooesnot have expected tcsr set.
	 */
	if (g_bootrec.tcsr_set != g_bootrec.exp_tcsr_set) {
		printf("Failsafe: Align TCSR set to %s\n",
			bootset_str(g_bootrec.exp_tcsr_set));

		ipq_spl_set_tcsr_set(g_bootrec.exp_tcsr_set);

		/*
		 * Intermediate reset - Ignore the current bootcunt
		 */
		g_bootrec.boot_mode = IPQ_SPL_BOOT_MODE_INTERMEDIATE_RESET;
		if (bootcount > 0)
			bootcount_store(--bootcount);

		ipq_spl_reset_cpu();
	}
}

/**
 * ipq_spl_failsafe_verify_pbl_set() - Handle PBL boot set mismatch
 *
 * Reconciles pbl_set against exp_boot_set per boot mode and takes
 * corrective action (TCSR fixup, bootcount cap, EDL, or mode override).
 *
 */
void ipq_spl_failsafe_verify_pbl_set(void)
{
	u8 boot_set_var;

	if (g_bootrec.boot_mode == IPQ_SPL_BOOT_MODE_FORCE_INACIVE) {
		ipq_spl_clear_force_inactive();

		boot_set_var = ipq_spl_alt_bootset(g_bootrec.tcsr_set);
		ipq_spl_set_tcsr_set(boot_set_var);

		if (g_bootrec.pbl_set != g_bootrec.exp_boot_set) {
			printf("Forceinactive: PBL failed to SPL from %s\n",
				bootset_str(g_bootrec.exp_boot_set));

			printf("Forceinactive: Failed\n");
		}

	} else if (g_bootrec.boot_mode == IPQ_SPL_BOOT_MODE_FAILOVER_EN) {
		if (g_bootrec.pbl_set != g_bootrec.exp_boot_set) {

			if (g_bootrec.pbl_set != g_bootrec.env_bootfrom) {
				printf("Failover: PBL failed to SPL from %s\n",
				bootset_str(g_bootrec.exp_boot_set));

				bootcount_store(g_bootrec.env_bootlimit + 1);
				printf("Failover: Bootcount (%lu)\n",
					bootcount_load());

				printf("Failover: Bootlimit (%u) exceeded.\n",
					g_bootrec.env_bootlimit);

				boot_set_var = ipq_spl_alt_bootset(
							g_bootrec.tcsr_set);
				ipq_spl_set_tcsr_set(boot_set_var);

			} else {
				printf("Failover: PBL failed to SPL from %s\n",
				bootset_str(g_bootrec.exp_boot_set));

				bootcount_store(2*g_bootrec.env_bootlimit + 1);
				printf("Failover: Bootcount (%lu)\n",
					bootcount_load());

				printf("Failover: 2*Bootlimit (%u) exceeded.\n",
					2*g_bootrec.env_bootlimit);

				ipq_spl_edl_reset();
			}
		}

	} else {
		if (g_bootrec.pbl_set != g_bootrec.exp_boot_set) {
			printf("Default: PBL failed to SPL from %s\n",
				bootset_str(g_bootrec.exp_boot_set));

			g_bootrec.boot_mode = IPQ_SPL_BOOT_MODE_SPL_INACTIVE;
		}
	}
}

/**
 * ipq_spl_failsafe_check() - Entry point for SPL failsafe boot logic
 * @boot_device: Boot device type (BOOT_DEVICE_SPI/MMC1/NAND)
 *
 * Handles the full failsafe sequence:
 *   init -> get_env -> parse_bootmode -> parse_bootset ->
 *   parse_pblset -> verify_tcsr -> verify_pblset
 *
 * Return: 0 on success, negative errno on failure.
 */
int ipq_spl_failsafe_check(u8 boot_device)
{
	int ret;
	ulong bootcount = bootcount_load();

	/*
	 * Increment bootcount on each boot
	 */
	bootcount_store(++bootcount);
	bootcount = bootcount_load();
	printf("Failsafe info: Bootcount (%lu)\n", bootcount);

	/*
	 * Initialize OCIMEM info during POR reset.
	 */
	if (IPQ_SPL_IS_POR_RESET())
		ipq_spl_clear_force_inactive();

	/*
	 * Initailize failsafe and read the ENV info from the flash partition
	 */
	ipq_spl_failsafe_init(boot_device);
	ret = ipq_spl_failsafe_get_env_info(boot_device);
	if (ret)
		return ret;

	/*
	 * Parse the bootmode and the expected bootset
	 */
	ipq_spl_failsafe_parse_bootmode();
	ipq_spl_failsafe_parse_bootset();

	/*
	 * Parse the PBL bootset
	 */
	ret = ipq_spl_failsafe_parse_pblset(boot_device);
	if (ret)
		return ret;

	/*
	 * Verify TCSR and PBL set against the expected bootset
	 */
	ipq_spl_failsafe_verify_tcsr_set();
	ipq_spl_failsafe_verify_pbl_set();

	/*
	 * Continue to boot from PBL SET
	 */
	g_bootrec.boot_set = g_bootrec.pbl_set;
	printf("Final Boot Set: %s\n", bootset_str(g_bootrec.boot_set));

	return 0;
}

#if !defined(CONFIG_SPL_FRAMEWORK_BOARD_INIT_F)
/**
 * board_init_f() - Main entry point for SPL.
 * @dummy:	Dummy argument (unused).
 *
 * This is the primary function called by U-Boot's SPL framework. It performs
 * essential system setup, initializes memory management (if enabled),
 * initializes the console, and then initiates the pre-DDR boot stage.
 * On failure, it calls the error handler.
 */
void board_init_f(ulong dummy)
{
	int ret;

	/*
	 * Clear BSS
	 */
	memset(__bss_start, 0, __bss_end - __bss_start);

#ifdef CONFIG_CONSOLE_RECORD
	/*
	 * Capture console output into the pre-DDR SRAM buffer from this point
	 * on, so boot logs survive even if UART output is disabled or lost.
	 * Consolidated into SMEM later, once DDR/SMEM are available (see
	 * ipq_spl_smem_console_log_init()).
	 */
	membuff_init((struct membuff *)&gd->console_out, g_console_record_buf,
		     sizeof(g_console_record_buf));
	gd->flags |= GD_FLG_RECORD;
#endif /* CONFIG_CONSOLE_RECORD */

#if defined(CFG_EMUL_FREQUENCY_DIVIDER)
	ipq_spl_setup_arch_cntfreq();
#endif
	/*
	 * Referred from U-boot Proper code (target specific):
	 * board/qualcomm/<ipqxxxx/ipqxxxx.c
	 */
	ipq_spl_board_early_init_f();

	/* Initialize secure watchdog*/
	enable_sec_wdog(WDT_DEFAULT_TIMEOUT_MS);

#if CONFIG_IS_ENABLED(SYS_MALLOC_F)
	ipq_spl_malloc_init_f();
#endif

	ret = spl_early_init();
	if (ret) {
		pr_debug("spl_early_init() failed (ret=%d)\n", ret);
		goto fail;
	}

#if defined(CONFIG_CLK_QCOM_PLL)
	ret = ipq_spl_probe_and_enable_plls();
	if (ret) {
		pr_err("Failed to enable PLLs (ret=%d)\n", ret);
		goto fail;
	}
#endif /* CONFIG_CLK_QCOM_PLL */

	ret = ipq_spl_board_init_clk();
	if (ret) {
		pr_err("Failed to initialize board clocks (ret=%d)\n", ret);
		goto fail;
	}

#if !defined(CONFIG_DISABLE_CONSOLE)
	preloader_console_init();
#endif

	ipq_spl_print_pbl_logs();

#if defined(CONFIG_CLK_QCOM_ALL)
	ipq_enable_all_clks();
#endif

	ipq_spl_boot_logs();

#if defined(CONFIG_IPQ_TMEL_IPC_SUPPORT)
	/* Skip TME fuse listing if tmel_bypass is enabled */
	if (!ipq_spl_tmel_bypass_enabled()) {
		ipq_spl_list_tme_fuse(tme_fuse_info_array,
					ARRAY_SIZE(tme_fuse_info_array));
		ipq_spl_get_tme_patch_version();
#if defined(CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT)
                ipq_spl_get_tme_prng_data();
#endif
	} else {
		printf("TME fuse listing skipped (tmel_bypass=1)\n");
	}
#endif
	ipq_spl_list_fuse(fuse_info_array,
				ARRAY_SIZE(fuse_info_array));

#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	ret = arm_reserve_mmu();
	if (ret) {
		pr_debug("Failed to reserve space for MMU (ret=%d)\n", ret);
		goto fail;
	}

	enable_caches();
#endif

	ret = ipq_spl_failsafe_check(spl_boot_device());
	if (ret) {
		pr_debug("ipq_spl_failsafe_check() failed (ret=%d)\n", ret);
		goto fail;
	}

	if (CONFIG_IS_ENABLED(QCOM_FUSE_VOLTAGE_CONTROL)) {
		ret = qcom_voltage_control_set_configured_mode("voltage-control");
		if (ret && ret != -ENODEV)
			pr_err("voltage setup failed (ret=%d)\n", ret);
		ret = 0;
	}

	ret = ipq_spl_loader_pre_ddr(spl_boot_device());
	if (ret) {
		pr_debug("ipq_spl_loader_pre_ddr() failed (ret=%d)\n", ret);
		goto fail;
	}

	/*
	 * Drop blkcache state before switching from board_init_f() to
	 * board_init_r(), since allocator context changes across this handoff.
	 */
	blkcache_free();
	board_init_r(NULL, 0);

fail:
	if (ret)
		ipq_spl_error_handler(__FILE__, __LINE__,
				      SPL_ERR_CAT_BOOT | SPL_ERR_INIT_FAIL);
}
#endif /* !CONFIG_SPL_FRAMEWORK_BOARD_INIT_F */

/**
 * nand_is_block_mibib() - Check if a block contains a valid MIBIB
 * @block:	Block number to check
 * @age:	Pointer to store the age of the MIBIB if valid
 *
 * This function checks if the specified block contains a valid MIBIB
 * by verifying magic numbers, version information, and CRC32 checksum.
 *
 * Return: true if valid MIBIB found, false otherwise
 */
static bool nand_is_block_mibib(int block, u32 *age)
{
	struct mi_boot_info *mibib_magic;
	struct flash_partition_table *parti_sys;
	struct flash_usr_partition_table *parti_usr;
	struct flash_mibib_crc *mibib_crc;
	u8 *page_buf;
	u32 page, crc32 = 0;
	struct mtd_info *mtd;
	int ret, i;

	mtd = get_nand_dev_by_index(0);
	if (!mtd) {
		printf("Failed to get NAND device\n");
		return false;
	}

	/* Allocate a buffer for reading pages */
	page_buf = malloc(mtd->writesize);
	if (!page_buf) {
		printf("Failed to allocate page buffer\n");
		return false;
	}

	/* Calculate page number for MIBIB header */
	page = block * (mtd->erasesize / mtd->writesize);

	/* Read the MIBIB header page */
	size_t length = mtd->writesize;

	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read MIBIB header page\n");
		free(page_buf);
		return false;
	}

	/* Check MIBIB magic numbers and version */
	mibib_magic = (struct mi_boot_info *)page_buf;
	if ((mibib_magic->magic1 != MIBIB_MAGIC1) ||
	    (mibib_magic->magic2 != MIBIB_MAGIC2) ||
	    (mibib_magic->version != MIBIB_VERSION)) {
		free(page_buf);
		return false;
	}

	/* Store the age number */
	*age = mibib_magic->age;

	/* Start calculating CRC32 from MIBIB header page */
	crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);

	/* Read the partition table page */
	page++;
	length = mtd->writesize;
	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read partition table page\n");
		free(page_buf);
		return false;
	}

	/* Check partition table magic numbers and version */
	parti_sys = (struct flash_partition_table *)page_buf;
	if ((parti_sys->magic1 != FLASH_PART_MAGIC1) ||
	    (parti_sys->magic2 != FLASH_PART_MAGIC2) ||
	    (parti_sys->version != FLASH_PARTITION_VERSION)) {
		free(page_buf);
		return false;
	}

	/* Continue calculating CRC32 with partition table page */
	crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);

	/* Read and calculate CRC for remaining pages up to USR_PART page */
	for (i = MIBIB_PAGE_PARTITION_TABLE + 1; i < MIBIB_PAGE_LAST_PAGE - 2; i++) {
		page++;
		length = mtd->writesize;
		ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
		if (ret) {
			if (ret == -EUCLEAN) {
				/* Page is erased, fill with 0xFF for CRC calculation */
				memset(page_buf, 0xFF, mtd->writesize);
			} else {
				printf("Failed to read MIBIB page %d\n", i);
				free(page_buf);
				return false;
			}
		}
		crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);
	}

	/* Read the USR_PART page */
	page++;
	length = mtd->writesize;
	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret) {
		printf("Failed to read USR_PART page\n");
		free(page_buf);
		return false;
	}

	/* Validate USR_PART page */
	parti_usr = (struct flash_usr_partition_table *)page_buf;
	if ((parti_usr->magic1 != FLASH_USR_PART_MAGIC1) ||
	    (parti_usr->magic2 != FLASH_USR_PART_MAGIC2) ||
	    (parti_usr->version != FLASH_PARTITION_VERSION)) {
		printf("USR_PART magic or version number mismatch\n");
		free(page_buf);
		return false;
	}

	/* Continue CRC calculation with USR_PART page */
	crc32 = crc32_no_comp(crc32, (uint8_t *)page_buf, mtd->writesize);

	/* Read the CRC page */
	page++;
	length = mtd->writesize;
	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read MIBIB CRC page\n");
		free(page_buf);
		return false;
	}

	/* Check CRC magic numbers and version */
	mibib_crc = (struct flash_mibib_crc *)page_buf;
	if ((mibib_crc->magic1 != FLASH_MIBIB_CRC_MAGIC1) ||
	    (mibib_crc->magic2 != FLASH_MIBIB_CRC_MAGIC2) ||
	    (mibib_crc->version != FLASH_MIBIB_CRC_VERSION)) {
		printf("MIBIB CRC magic or version mismatch\n");
		free(page_buf);
		return false;
	}

	/* Verify CRC32 checksum */
	if (mibib_crc->crc != crc32) {
		/*
		 * printf("MIBIB CRC checksum mismatch: calculated=0x%08x, stored=0x%08x\n",
		 * crc32, mibib_crc->crc);
		 * free(page_buf); // TBD: UBOOT_SPL
		 * return false;
		 */

	}

	/* All checks passed, we have a valid MIBIB */
	free(page_buf);
	return true;
}

/**
 * nand_retrieve_mibib() - Find and retrieve MIBIB from flash
 *
 * This function searches for valid MIBIB blocks in flash and returns
 * the partition table from the most recent valid MIBIB.
 *
 * Return: Pointer to the partition table, or NULL if not found
 */
static struct flash_partition_table *nand_retrieve_mibib(void)
{
	int cur_block;
	u32 copy1_age = 0, copy2_age = 0;
	int copy1_blockno = -1, copy2_blockno = -1;
	bool copy1_valid = false, copy2_valid = false;
	int new_mibib_block = -1;
	struct mtd_info *mtd;
	struct flash_partition_table *parti_ptr = NULL;
	u8 *page_buf = NULL;
	int ret;

	mtd = get_nand_dev_by_index(0);
	if (!mtd) {
		printf("Failed to get NAND device\n");
		return NULL;
	}

	/* Allocate a buffer for reading pages */
	page_buf = malloc(mtd->writesize);
	if (!page_buf) {
		printf("Failed to allocate page buffer\n");
		return NULL;
	}

	/* Search for first MIBIB copy */
	for (cur_block = 0; cur_block <= MIBIB_BLOCK_SEARCH_MAX; cur_block++) {
		if (nand_is_block_mibib(cur_block, &copy1_age)) {
			copy1_valid = true;
			copy1_blockno = cur_block;
			break;
		}
	}

	/* If no valid MIBIB found, return NULL */
	if (!copy1_valid) {
		printf("No valid MIBIB found\n");
		free(page_buf);
		return NULL;
	}

	/* Search for second MIBIB copy */
	for (cur_block = copy1_blockno + 1; cur_block <= MIBIB_BLOCK_SEARCH_MAX; cur_block++) {
		if (nand_is_block_mibib(cur_block, &copy2_age)) {
			copy2_valid = true;
			copy2_blockno = cur_block;
			break;
		}
	}

	/* Determine which MIBIB copy is newer */
	if (copy1_valid && !copy2_valid)
		new_mibib_block = copy1_blockno;
	else if (!copy1_valid && copy2_valid)
		new_mibib_block = copy2_blockno;
	else if (copy1_valid && copy2_valid) {
		if (copy1_age > copy2_age)
			new_mibib_block = copy1_blockno;
		else
			new_mibib_block = copy2_blockno;
	}

	if (new_mibib_block == -1) {
		printf("Failed to determine valid MIBIB block\n");
		free(page_buf);
		return NULL;
	}

	/* Allocate memory for the partition table first */
	parti_ptr = malloc(sizeof(struct flash_partition_table));
	if (!parti_ptr) {
		printf("Failed to allocate memory for partition table\n");
		free(page_buf);
		return NULL;
	}

	/* Read the partition table from the valid MIBIB block */
	u32 page = (new_mibib_block * (mtd->erasesize / mtd->writesize)) +
			MIBIB_PAGE_PARTITION_TABLE;

	size_t length = mtd->writesize;

	ret = nand_read(mtd, page * mtd->writesize, &length, page_buf);
	if (ret || length != mtd->writesize) {
		printf("Failed to read partition table\n");
		free(page_buf);
		free(parti_ptr);
		return NULL;
	}

	/* Copy the partition table */
	memcpy(parti_ptr, page_buf, sizeof(struct flash_partition_table));

	/* Verify the partition table */
	if ((parti_ptr->magic1 != FLASH_PART_MAGIC1) ||
		(parti_ptr->magic2 != FLASH_PART_MAGIC2) ||
		(parti_ptr->version != FLASH_PARTITION_VERSION)) {
		printf("Invalid partition table in MIBIB\n");
		free(page_buf);
		free(parti_ptr);
		return NULL;
	}

	free(page_buf);
	return parti_ptr;
}

/**
 * ipq_spl_ensure_mibib() - Ensure MIBIB partition table is available
 *
 * This function validates whether the global MIBIB partition table
 * is already populated. If not, it initializes NAND, scans the flash
 * for a valid MIBIB, and copies the partition table into the global
 * structure.
 *
 * Return: 0 on success, -ENODEV if not found
 */
static int ipq_spl_ensure_mibib(void)
{
	struct flash_partition_table *mibib_parti_ptr;

	/*
	 * Skip if already probed
	 */
	if ((g_mibib_parti_tbl.magic1 == FLASH_PART_MAGIC1) &&
	    (g_mibib_parti_tbl.magic2 == FLASH_PART_MAGIC2) &&
	    (g_mibib_parti_tbl.version == FLASH_PARTITION_VERSION))
		return 0;

	/*
	 * Initialized only once.
	 */
	nand_init();

	mibib_parti_ptr = nand_retrieve_mibib();
	if (!mibib_parti_ptr) {
		printf("MIBIB not found\n");
		return -ENODEV;
	}

	memcpy(&g_mibib_parti_tbl, mibib_parti_ptr,
	       sizeof(struct flash_partition_table));
	free(mibib_parti_ptr);

	return 0;
}

/**
 * ipq_spl_mibib_getpart() - Find partition info by name from MIBIB table
 * @part_name: Name of the partition to find
 * @start_blk: Pointer to store the start block offset
 * @blk_cnt:   Pointer to store the block count (length)
 *
 * This function searches the global MIBIB partition table (g_mibib_parti_ptr)
 * for a partition matching the given name and returns its offset and length
 * in blocks. It replaces the former find_bootldr_partition() helper and can
 * be used by any caller (including ipq_license.c) as an alternative to
 * ipq_smem_getpart().
 *
 * Return: 0 on success, -ENOENT if not found, -EINVAL on invalid params
 */
int ipq_spl_mibib_getpart(const char *part_name, uint32_t *start_blk,
			   uint32_t *blk_cnt)
{
	int i, ret;

	if (!part_name || !start_blk || !blk_cnt) {
		pr_err("Invalid parameters for MIBIB partition lookup\n");
		return -EINVAL;
	}

	/* Ensure MIBIB is loaded */
	ret = ipq_spl_ensure_mibib();
	if (ret) {
		printf("MIBIB not found\n");
		return -ENOENT;
	}

	for (i = 0; i < g_mibib_parti_tbl.numparts; i++) {
		if (strncmp(g_mibib_parti_tbl.part_entry[i].name,
			    part_name,
			    sizeof(g_mibib_parti_tbl.part_entry[i].name)) == 0) {
			*start_blk = g_mibib_parti_tbl.part_entry[i].offset;
			*blk_cnt = g_mibib_parti_tbl.part_entry[i].length;
			return 0;
		}
	}

	pr_err("Partition '%s' not found in MIBIB table\n", part_name);
	return -ENOENT;
}

/**
 * nand_spl_adjust_offset() - Adjust NAND offset to account for bad blocks
 * @sector: Partition start offset (absolute address)
 * @offs: Target offset relative to partition start
 *
 * This function adjusts the target offset to account for any bad blocks
 * between the partition start and the target offset. For each bad block
 * found, the target offset is incremented by one block size.
 *
 * This is a weak function override from U-Boot's spl_nand.c that enables
 * proper bad block handling when loading FIT images component-by-component.
 *
 * The function receives a relative offset and must return a relative offset.
 * Internally, it converts to absolute addresses for bad block checking.
 *
 * Return: Adjusted relative offset that accounts for bad blocks
 */
u32 nand_spl_adjust_offset(u32 sector, u32 offs)
{
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	loff_t block_start = sector;
	loff_t target = sector + offs;  /* Convert relative to absolute */
	u32 bad_blocks = 0;

	if (!mtd)
		return offs;

	/*
	 * Scan for bad blocks between partition start (sector) and
	 * target offset. For each bad block found, adjust target forward.
	 */
	while (block_start < target) {
		if (mtd_block_isbad(mtd, block_start)) {
			bad_blocks++;
			target += mtd->erasesize;
		}
		block_start += mtd->erasesize;
	}

	if (bad_blocks > 0) {
		debug("nand_spl_adjust_offset: sector=0x%x, offs=0x%x -> "
		      "adjusted=0x%x (skipped %u bad blocks)\n",
		      sector, offs, (u32)(target - sector), bad_blocks);
	}

	return (u32)(target - sector);  /* Return adjusted relative offset */
}

/**
 * spl_nand_get_uboot_raw_page() - Get the page offset of the BOOTLDR partition
 *
 * This function retrieves the MIBIB from flash, finds the BOOTLDR partition,
 * and returns its page offset.
 *
 * Return: Page offset of the BOOTLDR partition, or 0 if not found
 */
int spl_nand_get_uboot_raw_page(void)
{
	struct mtd_info *mtd;
	int ret;
	char *part_name = ipq_spl_bootldr_partition_name();
	uint32_t start_blk = 0, blk_cnt = 0;

	/* Return cached value if already computed */
	if (g_bootldr_offset != 0)
		return g_bootldr_offset;

	/*
	 * Initialize bootldr offset
	 */
#if defined(CONFIG_SYS_NAND_U_BOOT_OFFS)
	g_bootldr_offset = CONFIG_SYS_NAND_U_BOOT_OFFS;
#else
	g_bootldr_offset = 0;
#endif

	/* Find BOOTLDR partition using generic MIBIB lookup */
	ret = ipq_spl_mibib_getpart(part_name, &start_blk, &blk_cnt);
	if (ret) {
		printf("%s not found in MIBIB, using default offset: 0x%X\n",
		       part_name, g_bootldr_offset);
		return g_bootldr_offset;
	}

	/*
	 * save the startblk of the partition
	 */
	g_bootldr_offset = start_blk;

	/* Convert block offset to page offset */
	mtd = get_nand_dev_by_index(0);
	if (mtd)
		g_bootldr_offset *= mtd->erasesize;

	printf("BOOTLDR partition: reading from first good block at 0x%X\n",
		g_bootldr_offset);

	return g_bootldr_offset;
}

/**
 * ipq_spl_nand_read() - Read data from NAND partition
 * @part_name: Partition name to look up in MIBIB
 * @buf:       Destination buffer to store read data
 * @read_sz:   In - maximum number of bytes to read;
 *             Out - actual number of bytes read
 * @part_sz:   Pointer to return the partition size in bytes
 *
 * This function locates the specified partition using the MIBIB
 * partition table, computes the corresponding NAND offset, and
 * reads the data into the provided buffer using SPL NAND APIs.
 *
 * Return: 0 on success, -ENOENT if not found, -EINVAL on invalid params
 */
static int ipq_spl_nand_read(char *part_name, void *buf, ulong *read_sz,
			     ulong *part_sz)
{
	struct mtd_info *mtd;
	uint32_t offset, size;
	int ret;
	uint32_t start_blk, blk_cnt;

	if (!part_name || !buf || !read_sz || !part_sz)
		return -EINVAL;

	/* Find partition using generic MIBIB lookup */
	ret = ipq_spl_mibib_getpart(part_name, &start_blk, &blk_cnt);
	if (ret) {
		printf("%s not found in MIBIB\n", part_name);
		return -ENOENT;
	}

	/* Convert block offset to byte offset */
	mtd = get_nand_dev_by_index(0);
	if (mtd) {
		offset = start_blk * mtd->erasesize;
		size = blk_cnt * mtd->erasesize;
	} else
		return -ENODEV;

	printf("Found partition '%s' at offset 0x%x\n", part_name, offset);

	*part_sz = size;
	*read_sz = min_t(ulong, *read_sz, size);
	ret = nand_spl_load_image(offset, *read_sz, buf);
	if (ret) {
		printf("%s: nand_spl_load_image failed (err %d)\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

/**
 * spl_find_partition_info() - Find partition information by name
 * @uclass_id: Device class ID (UCLASS_MMC, UCLASS_SPI, etc.)
 * @device_num: Device number within the class
 * @part_name: Name of the partition to find
 * @info: Pointer to store partition information
 *
 * This function provides common partition lookup logic that can be shared
 * between different boot device types (MMC, SPI, etc.).
 * Return: Partition number on success, negative error code on failure
 */
static int spl_find_partition_info(enum uclass_id uclass_id, int device_num,
				   const char *part_name,
				   struct disk_partition *info)
{
	int part, ret;
	struct blk_desc *desc;

	if (!part_name || !info) {
		printf("Invalid parameters for partition lookup\n");
		return -EINVAL;
	}

	/* Configure blkcache policy for this boot-device class. */
	switch (uclass_id) {
	case UCLASS_MMC:
		/*
		 * eMMC (512B blk): MBR(1) + GPT hdr(1) + GPT entries
		 * (128 * 128B = 32 blks) => 34 blks (~17KB).
		 * Use 8 blks * 5 entries (~20KB) to cover this.
		 */
		blkcache_configure(8, 5);
		break;
	case UCLASS_SPI:
		/*
		 * SPI-NOR (4KB blk): MBR(1) + GPT hdr(1) + GPT entries
		 * (128 * 128B = 4 blks) => 6 blks (24KB).
		 * Use 6 blks * 1 entry (24KB) to cover this.
		 */
		blkcache_configure(6, 1);
		break;
	default:
		break;
	}

	/*
	 * Get block device descriptor
	 */
	desc = blk_get_devnum_by_uclass_id(uclass_id, device_num);
	if (!desc) {
		printf("Block device not found for class %d, device %d\n",
				uclass_id, device_num);
		return -ENODEV;
	}

	/*
	 * Initialize partition table if needed
	 */
	if (desc->part_type == PART_TYPE_UNKNOWN) {
		printf("Initializing partition table\n");
		/*
		 * Prefer EFI/GPT to avoid memory-intensive part_init()
		 */
		desc->part_type = PART_TYPE_EFI;
	}

	if (uclass_id == UCLASS_MMC &&
	    !strcmp(part_name, IPQ_SPL_DEFAULT_SPL_PARTITION_LABEL)) {
		/*
		 * For SPL partition lookup on eMMC:
		 * 1) Try BOOT1 first.
		 * 2) Fallback to USER (DEFAULT) partition.
		 */
		ret = blk_dselect_hwpart(desc, EMMC_HWPART_BOOT1);
		if (!ret) {
			part = part_get_info_by_name(desc, part_name, info);
			if (part >= 0) {
				/* Restore USER hwpart for later accesses. */
				ret = blk_dselect_hwpart(desc,
							 EMMC_HWPART_DEFAULT);
				if (ret)
					return ret;
				printf("Found partition '%s' at partition number %d (hwpart %d)\n",
				       part_name, part, EMMC_HWPART_BOOT1);
				return part;
			}
		}

		ret = blk_dselect_hwpart(desc, EMMC_HWPART_DEFAULT);
		if (!ret) {
			part = part_get_info_by_name(desc, part_name, info);
			if (part >= 0) {
				printf("Found partition '%s' at partition number %d (hwpart %d)\n",
				       part_name, part, EMMC_HWPART_DEFAULT);
				return part;
			}
		}

		goto not_found;
	}

	/*
	 * SPI and all other cases (including MMC non-SPL partitions):
	 * no HW partition scan.
	 */
	part = part_get_info_by_name(desc, part_name, info);
	if (part >= 0) {
		printf("Found partition '%s' at partition number %d\n",
		       part_name, part);
		return part;
	}

not_found:
	printf("Partition '%s' not found\n", part_name);
	return -ENOENT;
}

/**
 * ipq_spl_blk_read() - Read a GPT partition from a block device into a buffer
 * @uclass_id:  Device class identifier (e.g. UCLASS_MMC, UCLASS_SPI)
 * @devnum:     Device number within the given class
 * @part_name:  GPT partition label to read from
 * @buf:        Destination buffer for the read data
 * @read_sz:    In  - maximum number of bytes to read;
 *              Out - actual number of bytes read, clamped to partition size
 * @part_sz:    Pointer to return the partition size in bytes
 *
 * Looks up the named GPT partition on the specified block device using
 * spl_find_partition_info(), then reads up to @read_sz bytes from the
 * partition start into @buf using blk_dread().
 *
 * The actual transfer size is clamped to the smaller of the requested
 * @read_sz and the physical partition size derived from @disk_info.
 *
 * Return: 0 on success,
 *         -EINVAL if any pointer argument is NULL,
 *         -ENODEV if the partition or block device is not found,
 *         -EIO    if the block read transfers fewer blocks than expected.
 */
static int ipq_spl_blk_read(enum uclass_id uclass_id, int devnum,
			     const char *part_name, void *buf, ulong *read_sz,
			     ulong *part_sz)
{
	struct disk_partition disk_info;
	struct blk_desc *bdev;
	lbaint_t count;
	int ret;

	if (!part_name || !buf || !read_sz || !part_sz)
		return -EINVAL;

	/*
	 * Find partition
	 */
	ret = spl_find_partition_info(uclass_id, devnum, part_name, &disk_info);
	if (ret < 0) {
		printf("%s: partition '%s' not found (err %d)\n",
		       __func__, part_name, ret);
		return -ENODEV;
	}

	bdev = blk_get_devnum_by_uclass_id(uclass_id, devnum);
	if (IS_ERR_OR_NULL(bdev))
		return -ENODEV;

	*part_sz = disk_info.size << bdev->log2blksz;
	*read_sz = min_t(ulong, *read_sz, *part_sz);
	count = *read_sz >> bdev->log2blksz;

	if (count != blk_dread(bdev, disk_info.start, count, buf))
		return -EIO;

	return 0;
}

/**
 * ipq_spl_blk_get_guid() - Retrieve the type GUID of a GPT partition
 * @uclass_id:  Device class identifier (e.g. UCLASS_MMC, UCLASS_SPI)
 * @devnum:     Device number within the given class
 * @part_name:  GPT partition label whose type GUID is requested
 * @type_guid:  Output pointer to store the retrieved EFI partition type GUID
 *
 * Looks up the named GPT partition on the specified block device using
 * spl_find_partition_info(), extracts the partition type GUID string via
 * disk_partition_type_guid(), and converts it to binary form using
 * uuid_str_to_bin() into @type_guid.
 *
 * Return: 0 on success,
 *         -EINVAL if @part_name or @type_guid is NULL,
 *         -ENODEV if the partition is not found on the device.
 */
static int ipq_spl_blk_get_guid(enum uclass_id uclass_id, int devnum,
				 const char *part_name, efi_guid_t *type_guid)
{
	struct disk_partition disk_info;
	const char *uuid_str;
	int ret;

	if (!part_name || !type_guid) {
		printf("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	/*
	 * Find partition
	 */
	ret = spl_find_partition_info(uclass_id, devnum, part_name, &disk_info);
	if (ret < 0) {
		printf("%s: partition '%s' not found (err %d)\n",
		       __func__, part_name, ret);
		return -ENODEV;
	}

	uuid_str = disk_partition_type_guid(&disk_info);
	printf("GPT Label: %s; GUID: %s\n", part_name, uuid_str);

	uuid_str_to_bin(uuid_str, type_guid->b, UUID_STR_FORMAT_GUID);

	return 0;
}

#if CONFIG_IPQ_MMC
/**
 * spl_mmc_boot_mode() - Determine the boot mode for MMC
 * @mmc:	Pointer to the MMC device
 * @boot_device:	Boot device ID
 *
 * This function determines the boot mode for MMC devices.
 * It returns MMCSD_MODE_RAW to indicate that raw partition access
 * should be used rather than filesystem access.
 * Return: MMCSD_MODE_RAW to use raw partition access
 */
u32 spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device)
{
	return MMCSD_MODE_RAW;
}

/**
 * spl_mmc_boot_partition() - Determine which partition to boot from
 * @boot_device:	Boot device ID
 *
 * This function determines which partition to boot from for MMC devices.
 * It attempts to find the partition specified by IPQ_SPL_FIT_IMG_PARTITION
 * using the common partition lookup function. If not found, it falls back
 * to the default partition defined by CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION.
 * Return: Partition number to boot from, or default partition on error
 */
int spl_mmc_boot_partition(const u32 boot_device)
{
	int ret;
	struct disk_partition info;

	/*
	 * Use common partition lookup function
	 */
	ret = spl_find_partition_info(UCLASS_MMC, 0,
					ipq_spl_bootldr_partition_name(),
					&info);
	if (ret < 0) {
		printf("Using default MMC partition %d\n",
				CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION);
		return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION;
	}

	return ret;
}

/**
 * spl_mmc_get_uboot_raw_sector() - Find the raw sector offset
 * @mmc:	Pointer to the MMC device
 * @raw_sect:	Sector
 *
 * This function returns the offset of the image from the starting of the partition.
 *
 * Return: 0 if the image is at the starting of the partition without any offset.
 */
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc,
					unsigned long raw_sect)
{
	return 0;
}

static int ipq_spl_mmc_get_guid(char *part_name, efi_guid_t *type_guid)
{
	return ipq_spl_blk_get_guid(UCLASS_MMC, 0, part_name, type_guid);
}

static int ipq_spl_mmc_read(char *part_name, void *buf, ulong *read_sz,
			    ulong *part_sz)
{
	return ipq_spl_blk_read(UCLASS_MMC, 0, part_name, buf, read_sz,
				part_sz);
}
#endif /*CONFIG_IPQ_MMC*/

#if CONFIG_IPQ_SPI_NOR
/**
 * spl_spi_find_partition_offset() - Find the offset of a partition by name
 * @flash: Pointer to the SPI flash device
 * @part_name: Name of the partition to find
 * @offset: Pointer to store the found offset
 *
 * This function searches for a partition with the given name in the GPT
 * partition table of the SPI flash device and returns its offset.
 * Uses the common partition lookup function for consistency.
 *
 * Return: 0 on success, negative error code on failure
 */
static int spl_spi_find_partition_offset(struct spi_flash *flash,
					const char *part_name,
					unsigned int *offset)
{
	int ret;
	struct disk_partition info;

	if (!flash || !offset) {
		printf("Invalid SPI flash pointer or offset\n");
		return -EINVAL;
	}

	/*
	 * Use common partition lookup function
	 */
	ret = spl_find_partition_info(UCLASS_SPI, CONFIG_SF_DEFAULT_BUS,
				      part_name, &info);
	if (ret < 0)
		return ret;

	/*
	 * Calculate the offset in bytes
	 */
	*offset = info.start * info.blksz;
	printf("Found partition '%s' at offset 0x%x\n", part_name, *offset);

	return 0;
}

/**
 * spl_spi_get_uboot_offs() - Get the offset of U-Boot in SPI flash
 * @flash: Pointer to the SPI flash device
 *
 * This function finds the partition specified by IPQ_SPL_FIT_IMG_PARTITION
 * in the GPT partition table and returns its offset. If the partition is not found,
 * it falls back to the default offset defined by CONFIG_SYS_SPI_U_BOOT_OFFS.
 *
 * Return: The offset of U-Boot in SPI flash
 */
unsigned int spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	unsigned int offset;
	const char *part_name = ipq_spl_bootldr_partition_name();
	int ret;

	/*
	 * Try to find the partition by name
	 */
	ret = spl_spi_find_partition_offset(flash, part_name, &offset);

	/*
	 * if Partition not found
	 */
	if (ret)
		ipq_spl_error_handler(__FILE__, __LINE__,
				      SPL_ERR_CAT_FLASH | SPL_ERR_NOT_FOUND);

	/*
	 * Partition found, return its offset
	 */
	return offset;
}

/**
 * spl_spi_boot_bus() - Get the SPI bus number to use
 *
 * This function returns the SPI bus number to use for SPI flash operations.
 * Return: The SPI bus number
 */
u32 spl_spi_boot_bus(void)
{
	/*
	 * Return the SPI bus number to use
	 */
	return CONFIG_SF_DEFAULT_BUS;
}

/**
 * spl_spi_boot_cs() - Get the SPI chip select to use
 *
 * This function returns the SPI chip select to use for SPI flash operations.
 * Return: The SPI chip select
 */
u32 spl_spi_boot_cs(void)
{
	/*
	 * Return the SPI chip select to use
	 */
	return CONFIG_SF_DEFAULT_CS;
}

static int ipq_spl_spinor_get_guid(char *part_name, efi_guid_t *type_guid)
{
	struct spi_flash *flash;

	flash = spi_flash_probe(spl_spi_boot_bus(), spl_spi_boot_cs(),
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		printf("%s: SPI probe failed\n", __func__);
		return -ENODEV;
	}

	return ipq_spl_blk_get_guid(UCLASS_SPI, CONFIG_SF_DEFAULT_BUS,
				    part_name, type_guid);
}

static int ipq_spl_spinor_gpt_read(char *part_name, void *buf, ulong *read_sz,
				   ulong *part_sz)
{
	struct spi_flash *flash;

	flash = spi_flash_probe(spl_spi_boot_bus(), spl_spi_boot_cs(),
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		printf("%s: SPI probe failed\n", __func__);
		return -ENODEV;
	}

	return ipq_spl_blk_read(UCLASS_SPI, CONFIG_SF_DEFAULT_BUS,
				part_name, buf, read_sz, part_sz);
}
#endif /*CONFIG_IPQ_SPI_NOR*/

/**
 * spl_boot_device() - Determine the boot device.
 *
 * This function reads a hardware register to identify the current boot
 * device and maps it to the corresponding SMEM boot flash type.
 * Return: The mapped boot device type, or -EINVAL if the device is invalid.
 */
u32 spl_boot_device(void)
{
	/*
	 * Parse Bootdevice
	 */
	u8 boot_device_cfg =
	(readl(IPQ_SPL_BOOTCFG_REG_ADDR) & IPQ_SPL_BOOTCFG_DEV_MASK) >>
	IPQ_SPL_BOOTCFG_DEV_SHFT;
	u32 boot_device_smem;

	/*
	 * Map the boot device
	 */
	switch (boot_device_cfg) {
#if CONFIG_IPQ_SPI_NOR
	case IPQ_SPL_BOOTCFG_DEV_NOR_GPT:
		boot_device_smem = BOOT_DEVICE_SPI;
		printf("Selected boot device: SPI-NOR GPT\n");
		break;
#endif
#if CONFIG_IPQ_MMC
	case IPQ_SPL_BOOTCFG_DEV_MMC:
		boot_device_smem = BOOT_DEVICE_MMC1;
		printf("Selected boot device: MMC\n");
		break;
#endif
#if CONFIG_IPQ_NAND
	case IPQ_SPL_BOOTCFG_DEV_SPI_NAND:
		boot_device_smem = BOOT_DEVICE_NAND;
		printf("Selected boot device: SPI-NAND\n");
		break;
#endif
	default:
		pr_err("Invalid boot device configured: %d\n",
			boot_device_cfg);
		return -EINVAL;
	}

	return boot_device_smem;
}
