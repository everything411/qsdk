/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _SMEM_INFO_H_
#define _SMEM_INFO_H_

/*********************************************************************
 * Global and constant
 ********************************************************************/
#define SECURE_BOARD			BIT(8)
#define ATF_ENABLED			BIT(9)
#define KERNEL_AUTH_SUCCESS		BIT(10)
#define ROOTFS_AUTH_SUCCESS		BIT(11)
#define ACTIVE_BOOT_SET			BIT(12)
#define INVALID_BOOT			BIT(13)
#define FLASH_TYPE_MASK			0xFF

#define _SMEM_RAM_PTABLE_MAGIC_1	0x9DA5E0A8
#define _SMEM_RAM_PTABLE_MAGIC_2	0xAF9EC4E2

#define RAM_PARTITION_SDRAM		14
#define RAM_PARTITION_SYS_MEMORY	1
#define SMEM_PTN_NAME_MAX               16
#define SMEM_PTABLE_PARTS_MAX           32

#define BUILD_ID_LEN			32
/*
 * SoC info
 */
#define SOCINFO_VERSION_MAJOR(ver)	((ver & 0xffff0000) >> 16)
#define SOCINFO_VERSION_MINOR(ver)	(ver & 0x0000ffff)
/******************************************************************
 * Structure enum and static
 *****************************************************************/

enum {
	SMEM_BOOT_NO_FLASH        = 0,
	SMEM_BOOT_NOR_FLASH       = 1,
	SMEM_BOOT_NAND_FLASH      = 2,
	SMEM_BOOT_ONENAND_FLASH   = 3,
	SMEM_BOOT_SDC_FLASH       = 4,
	SMEM_BOOT_MMC_FLASH       = 5,
	SMEM_BOOT_SPI_FLASH       = 6,
	SMEM_BOOT_NORPLUSNAND     = 7,
	SMEM_BOOT_NORPLUSEMMC     = 8,
	SMEM_BOOT_QSPI_NAND_FLASH  = 11,
	SMEM_BOOT_NORGPT_FLASH     = 12,
};

/*
 * SMEM supported item IDs
 */
enum smem_mem_type {
	SMEM_SPINLOCK_ARRAY = 7,
	SMEM_AARM_PARTITION_TABLE = 9,
	SMEM_HW_SW_BUILD_ID = 137,
	SMEM_USABLE_RAM_PARTITION_TABLE = 402,
	SMEM_POWER_ON_STATUS_INFO = 403,
	SMEM_MACHID_INFO_LOCATION = 425,
	SMEM_IMAGE_VERSION_TABLE = 469,
	SMEM_BOOT_FLASH_TYPE = 498,
	SMEM_BOOT_FLASH_INDEX = 499,
	SMEM_BOOT_FLASH_CHIP_SELECT = 500,
	SMEM_BOOT_FLASH_BLOCK_SIZE = 501,
	SMEM_BOOT_FLASH_DENSITY = 502,
	SMEM_BOOT_DUALPARTINFO = 503,
	SMEM_PARTITION_TABLE_OFFSET = 504,
	SMEM_SPI_FLASH_ADDR_LEN = 505,
	SMEM_TRY_MODE_INPROGRESS = 507,
	SMEM_ATF_ENABLE = 509,
	SMEM_EDL_MODE = 510,
	SMEM_BOOT_SET_INFO = 511,
	SMEM_FAILSAFE_BOOT_MODE = 512,
	SMEM_FID_LIST_INFO = 513,
	SMEM_SPL_CONSOLE_LOG = 514,
	SMEM_FIRST_VALID_TYPE = SMEM_SPINLOCK_ARRAY,
	SMEM_LAST_VALID_TYPE = SMEM_SPL_CONSOLE_LOG,
	SMEM_MAX_SIZE = SMEM_LAST_VALID_TYPE + 1,
};

/*
 * RAM partition info
 */
struct ram_partition_entry {
	char name[CONFIG_RAM_PART_NAME_LENGTH];
				/* Partition name, unused for now */
	u64 start_address;	/* Partition start address in RAM */
	u64 length;		/* Partition length in RAM in Bytes */
	u32 partition_attribute;/* Partition attribute */
	u32 partition_category;	/* Partition category */
	u32 partition_domain;	/* Partition domain */
	u32 partition_type;	/* Partition type */
	u32 num_partitions;	/* Number of partitions on device */
	u32 hw_info;		/* hw information such as type and freq */
	u8 highest_bank_bit;	/* Highest bit corresponding to a bank */
	u8 reserve0;		/* Reserved for future use */
	u8 reserve1;		/* Reserved for future use */
	u8 reserve2;		/* Reserved for future use */
	u32 reserved5;		/* Reserved for future use */
	u64 available_length;	/* Available Part length in RAM in Bytes */
};

struct usable_ram_partition_table {
	u32 magic1;	/* Magic number to identify valid RAM partition */
	u32 magic2;	/* Magic number to identify valid RAM partition */
	u32 version;	/* Version number to track structure
			 * definition changes and maintain
			 * backward compatibilities
			 */
	u32 reserved1;	/* Reserved for future use */
	u32 num_partitions;	/* Number of RAM partition table entries */
	u32 reserved2;	/* Added for 8 bytes alignment of header */
	/* RAM partition table entries */
	struct ram_partition_entry ram_part_entry[CONFIG_RAM_NUM_PART_ENTRIES];
};

struct smem_pmic_type {
	unsigned int pmic_model;
	unsigned int pmic_die_revision;
};

/*
 * SPL console log buffer, staged through SMEM so boot logs survive even
 * when UART output is disabled or SPL crashes before the console is seen.
 */
#define SMEM_SPL_CONSOLE_LOG_MAGIC	0x474F4C43	/* "CLOG" */
#define SMEM_SPL_CONSOLE_LOG_OVERFLOW	BIT(0)

struct smem_console_log {
	u32 magic;		/* SMEM_SPL_CONSOLE_LOG_MAGIC */
	u32 size;		/* Total data[] capacity in bytes */
	u32 write_offset;	/* Next write position within data[] */
	u32 flags;		/* SMEM_SPL_CONSOLE_LOG_* bits */
	char data[];
};

struct ipq_platform_v1 {
	unsigned int format;
	unsigned int id;
	unsigned int version;
	char     build_id[BUILD_ID_LEN];
	unsigned int raw_id;
	unsigned int raw_version;
	unsigned int hw_platform;
	unsigned int platform_version;
	unsigned int accessory_chip;
	unsigned int hw_platform_subtype;
};

struct ipq_platform_v2 {
	struct ipq_platform_v1 v1;
	struct smem_pmic_type pmic_info[3];
	unsigned int foundry_id;
};

struct ipq_platform_v3 {
	struct ipq_platform_v2 v2;
	unsigned int chip_serial;
};

union ipq_platform {
	struct ipq_platform_v1 v1;
	struct ipq_platform_v2 v2;
	struct ipq_platform_v3 v3;
};

struct smem_machid_info {
	unsigned int format;
	unsigned int machid;
};

struct soc_info {
	uint32_t cpu_type;
	uint32_t version;
	uint32_t soc_version_major;
	uint32_t soc_version_minor;
	unsigned int machid;
};

struct ipq_part_entry {
	loff_t offset;
	loff_t size;
};

struct per_part_info {
	char name[CONFIG_RAM_PART_NAME_LENGTH];
	uint32_t primaryboot;
};

#ifdef CONFIG_BOOTCONFIG_V2
struct __packed ipq_smem_bootconfig_info {
#define _SMEM_DUAL_BOOTINFO_MAGIC_START				0xA3A2A1A0
#define _SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_MODE		0xA3A2A1A1
#define _SMEM_DUAL_BOOTINFO_MAGIC_START_UNIFIED_FAILSAFE	0xA3A2A1A2
#define _SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_UNIFIED_FAILSAFE	0xA3A2A1A3
#define _SMEM_DUAL_BOOTINFO_MAGIC_END				0xB3B2B1B0
	/* Magic number for identification when reading from flash */
	uint32_t magic_start;
	/* upgradeinprogress indicates to attempting the upgrade */
	uint32_t    age;
	/* numaltpart indicate number of alt partitions */
	uint32_t    numaltpart;

	struct per_part_info per_part_entry[CONFIG_NUM_ALT_PARTITION];

	uint32_t magic_end;

};
#elif CONFIG_BOOTCONFIG_V3
struct __packed ipq_smem_bootconfig_info {
#define  _SMEM_DUAL_BOOTINFO_MAGIC_START		0x72637279
	/* Magic number for identification when reading from flash */
	uint32_t magic_start;
	/* Represents the health status of the Bank A & B*/
	uint32_t image_set_status;
	/* Indicates which component updated the health status of a Bank */
	uint32_t owner;
	/* Indicates the current active Bank*/
	uint32_t boot_set;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t crc;  /* CRC field for fields above */
};
#else
struct __packed ipq_smem_bootconfig_info {
	uint32_t fillers[7];
};
#endif

#define IMAGE_INDEX_LENGTH			2
#define IMAGE_SEP1_LENGTH			1
#define IMAGE_QC_VERSION_STRING_LENGTH		72
#define IMAGE_VARIANT_STRING_LENGTH		20
#define IMAGE_SEP2_LENGTH			1
#define IMAGE_OEM_VERSION_STRING_LENGTH		32

struct image_version_entry
{
	char image_index[IMAGE_INDEX_LENGTH];
	char image_colon_sep1[IMAGE_SEP1_LENGTH];
	char image_qc_version_string[IMAGE_QC_VERSION_STRING_LENGTH];
	char image_variant_string[IMAGE_VARIANT_STRING_LENGTH];
	char image_colon_sep2[IMAGE_SEP2_LENGTH];
	char image_oem_version_string[IMAGE_OEM_VERSION_STRING_LENGTH];
};

struct ipq_smem_flash_info {
	uint32_t		flash_type;
	uint32_t		flash_index;
	uint32_t		flash_chip_select;
	uint32_t		flash_block_size;
	uint32_t		flash_density;
	uint32_t		flash_secondary_type;
	uint32_t		primary_mibib;
#ifdef CONFIG_BOOTCONFIG_V3
	uint32_t		edl_mode;
	uint32_t		try_mode_inprogress;
#endif
#ifdef CONFIG_FAILSAFE_V2
	uint32_t		smembootset;
	uint32_t		smembootmode;
#endif
	struct ipq_part_entry	hlos;
	struct ipq_part_entry	hlos_1;
	struct ipq_part_entry	rootfs;
	struct ipq_part_entry	rootfs_1;
	struct ipq_part_entry	dtb;
	struct ipq_part_entry	training;
	struct ipq_smem_bootconfig_info *binfo;
};

struct ipq_smem_target_info {
	uint32_t identifier;
	uint32_t smem_size;
	uint64_t smem_base_addr;
	uint16_t smem_max_items;
	uint16_t smem_rsvd;
};

#define IPQ_SMEM_TARGET_INFO_IDENTIFIER		0x49494953

struct __packed smem_ptn {
	char name[SMEM_PTN_NAME_MAX];
	unsigned int start;
	unsigned int size;
	unsigned int attr;
};

struct __packed smem_ptable {
#define _SMEM_PTABLE_MAGIC_1			0x55ee73aa
#define _SMEM_PTABLE_MAGIC_2			0xe35ebddb
	unsigned int magic[2];
	unsigned int version;
	unsigned int len;
	struct smem_ptn parts[SMEM_PTABLE_PARTS_MAX];
};
#endif /* _SMEM_INFO_H_ */
