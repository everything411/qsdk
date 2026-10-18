/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/global_data.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <asm/cache.h>
#include <command.h>
#ifndef CONFIG_ARM64
#include <asm/system.h>
#include <asm/armv7.h>
#endif
#include <cpu_func.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <mach/smem_info.h>
#include <mach/scm.h>
#include <part.h>
#include <stddef.h>
#include <linux/err.h>
#include <asm/io.h>
#include <dm.h>
#include <memalign.h>
#include <smem.h>
#include <sysreset.h>
#include <fdt_support.h>

#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
#include <spi.h>
#include <spi_flash.h>
#endif
#if defined(CONFIG_MMC)
#include <mmc.h>
#endif
#ifdef CONFIG_IPQ_NAND
#include <nand.h>
#endif

#if defined(CONFIG_TARGET_IPQ9574)
#include <asm/arch/ipq9574.h>
#endif
#if defined(CONFIG_TARGET_IPQ5332)
#include <asm/arch/ipq5332.h>
#endif
#if defined(CONFIG_TARGET_IPQ5424)
#include <asm/arch/ipq5424.h>
#endif
#if defined(CONFIG_TARGET_IPQ5210)
#include <asm/arch/ipq5210.h>
#endif
#if defined(CONFIG_TARGET_IPQ9650)
#include <asm/arch/ipq9650.h>
#endif

#ifndef __IPQ_H__
#define __IPQ_H__

/***********************************************************************
 * Global and constant
 **********************************************************************/
#define BOARD_DTS_MAX_NAMELEN				32
#define BLK_PART_GET_INFO_S(_ptr, _name, _info, _fl, _verbose)	\
	do {							\
		(&(_ptr))->name = _name;			\
		(&(_ptr))->info = _info;			\
		(&(_ptr))->flash_type = _fl;			\
		(&(_ptr))->devnum = 0;				\
		(&(_ptr))->verbose = _verbose;			\
	} while (0)

#define WHICH_FLASH(p)			(((p)->attr & 0xff000000) >> 24)
#define SECURE_BOARD			BIT(8)
#define ATF_ENABLED			BIT(9)
#define KERNEL_AUTH_SUCCESS		BIT(10)
#define ROOTFS_AUTH_SUCCESS		BIT(11)
#define ACTIVE_BOOT_SET			BIT(12)
#define INVALID_BOOT			BIT(13)
#define RECOVERY_MODE			BIT(14)
#define FLASH_TYPE_MASK			0xFF

#define MAGIC_RECOVERY_PATH		0xAECDBDDE

#ifndef IPQ_NAND_FLASH_VALID_BIT
#define IPQ_NAND_FLASH_VALID_BIT	3
#endif
#define CFG_IPQ_NAND_PART	       BIT(IPQ_NAND_FLASH_VALID_BIT)

#define DUMP_NAME_STR_MAX_LEN		20
#define DUMP_PATH_STR_MAX_LEN		50
#define DUMP2MEM_MAGIC1_COOKIE		0x4D494E49
#define DUMP2MEM_MAGIC2_COOKIE		0x44554D50

#if defined(CONFIG_BOOTCONFIG_V3)
#define SET_AB_USABLE			(0x0UL)
#define DONT_USE_SET_A			(0x1UL)
#define DONT_USE_SET_B			(0x2UL)
#define DONT_USE_SET_AB			(0x3UL)
#define BOOT_SET_A			0x0
#define BOOT_SET_B			0x1
#define BOOT_SET_INVALID		0xFFFF
#define BC_UBOOT_OWNER			0x1
#define BOOTCONFIG_V3_MAGIC		(0x72637279UL)
#endif

/*
 * TCSR bit
 */
#define DLOAD_MAGIC_COOKIE		0x10
#define DLOAD_DISABLED			0x40
#define DLOAD_ENABLE			BIT(4)
#define DLOAD_DISABLE			(~BIT(4))
#define CRASHDUMP_RESET			BIT(11)
#ifndef MARK_UBOOT_MILESTONE
#define MARK_UBOOT_MILESTONE		BIT(8)
#endif

#if defined(CONFIG_BOOTCONFIG_V2)
#define BOOTCONFIG_HEALTH_MASK		BIT(13)
#define BOOTCONFIG1_HEALTH_MASK		BIT(14)
#endif

#define ROOTFS_AUTH_EN			0x20

/*
 * Execute DPR
 */
#ifdef CONFIG_DPR_VER_1_0
#define execute_dpr_fun(a, b, c, d)	execute_dprv1(a, b, c, d)
#elif CONFIG_DPR_VER_2_0
#define execute_dpr_fun(a, b, c, d)	execute_dprv2(a, b, c, d)
#elif CONFIG_DPR_VER_3_0
#define execute_dpr_fun(a, b, c, d)	execute_dprv3(a, b, c, d)
#endif

#define reset()				do_reset(NULL, 0, 0, NULL)

/*
 * QCN9224 fusing
 */
#define QCN_VENDOR_ID					0x17CB
#define QCN9224_DEVICE_ID				0x1109
#define QCN9000_DEVICE_ID				0x1104
#define MAX_UNWINDOWED_ADDRESS				0x80000
#define WINDOW_ENABLE_BIT				0x40000000
#define WINDOW_SHIFT					19
#define WINDOW_VALUE_MASK				0x3F
#define WINDOW_START					MAX_UNWINDOWED_ADDRESS
#define WINDOW_RANGE_MASK				0x7FFFF

#define QCN9224_PCIE_REMAP_BAR_CTRL_OFFSET		0x310C
#define PCIE_SOC_GLOBAL_RESET_ADDRESS			0x3008
#define QCN9224_TCSR_SOC_HW_VERSION			0x1B00000
#define QCN9224_TCSR_SOC_HW_VERSION_MASK		GENMASK(11,8)
#define QCN9224_TCSR_SOC_HW_VERSION_SHIFT		8
#define PCIE_SOC_GLOBAL_RESET_VALUE			0x5
#define PCIE_SOC_GLOBAL_RESET_FORCE_RESET_VALUE		0x1
#define MAX_SOC_GLOBAL_RESET_WAIT_CNT			50 /* x 20msec */

#define QCN9224_TCSR_PBL_LOGGING_REG			0x01B00094
#define QCN9224_SECURE_BOOT0_AUTH_EN			0x01e24010
#define QCN9224_OEM_MODEL_ID				0x01e24018
#define QCN9224_ANTI_ROLL_BACK_FEATURE			0x01e2401c
#define QCN9224_OEM_PK_HASH				0x01e24060
#define QCN9224_SECURE_BOOT0_AUTH_EN_MASK		(0x1)
#define QCN9224_OEM_ID_MASK				GENMASK(31,16)
#define QCN9224_OEM_ID_SHIFT				16
#define QCN9224_MODEL_ID_MASK				GENMASK(15,0)
#define QCN9224_ANTI_ROLL_BACK_FEATURE_EN_MASK		BIT(9)
#define QCN9224_ANTI_ROLL_BACK_FEATURE_EN_SHIFT		9
#define QCN9224_TOTAL_ROT_NUM_MASK			GENMASK(13,12)
#define QCN9224_TOTAL_ROT_NUM_SHIFT			12
#define QCN9224_ROT_REVOCATION_MASK			GENMASK(17,14)
#define QCN9224_ROT_REVOCATION_SHIFT			14
#define QCN9224_ROT_ACTIVATION_MASK			GENMASK(21,18)
#define QCN9224_ROT_ACTIVATION_SHIFT			18
#define QCN9224_OEM_PK_HASH_SIZE			36
#define QCN9224_JTAG_ID					0x01e22b3c
#define QCN9224_SERIAL_NUM				0x01e22b40
#define QCN9224_PART_TYPE_EXTERNAL			0x01f94090
#define QCN9224_PART_TYPE_EXTERNAL_MASK			BIT(3)
#define QCN9224_PART_TYPE_EXTERNAL_SHIFT		3

#define MHICTRL						(0x38)
#define BHI_STATUS					(0x12C)
#define BHI_IMGADDR_LOW					(0x108)
#define BHI_IMGADDR_HIGH				(0x10C)
#define BHI_IMGSIZE					(0x110)
#define BHI_ERRCODE					(0x130)
#define BHI_ERRDBG1					(0x134)
#define BHI_ERRDBG2					(0x138)
#define BHI_ERRDBG3					(0x13C)
#define BHI_IMGTXDB					(0x118)
#define BHI_EXECENV					(0x128)
#define PCIE_LOCAL_RSV0					(0x3164)

#define MHICTRL_RESET_MASK				(0x2)
#define BHI_STATUS_MASK					(0xC0000000)
#define BHI_STATUS_SHIFT				(30)
#define BHI_STATUS_SUCCESS				(2)

#define BHI_EE_PBL					0
#define BHI_EE_SBL					1

#define MAX_CALDATA_SIZE				0x30000

#define NO_MASK						(0xFFFFFFFF)

#define MTDPARTS_MAXLEN					1024

/*
 * Extern variables
 */
#if CONFIG_FDT_FIXUP_PARTITIONS
extern struct node_info *fnodes;
extern int *fnode_entires;
#endif
extern struct ipq_board_info *ipq_bdinfo;
extern struct multidtb_config *g_board_dtb_info;
extern struct dts_fixup *mmc_fixup;
extern struct dts_fixup *usb_fixup;

/**********************************************************************
 * Structure enum and static
 *********************************************************************/
enum bank {
	INVALID_BOOT_AB = -1,
	VALID_BOOT_A,
	VALID_BOOT_B,
};

enum {
	SECURE_SYS_UPGRADE = 0,
};

enum fixup_type{
	UBOOT_FIXUP_SMEM,
	UBOOT_FIXUP_USB,
#ifdef CONFIG_BOOT_BANK_FIXUP
	UBOOT_FIXUP_BOOTED_BANK,
#endif
};

enum debug_component {
	DBG_DISABLE = 0,
	DBG_CRASHDUMP,
};


enum {
	FULLDUMP= 0,
	MINIDUMP,
	MINIDUMP_AND_FULLDUMP,
};

enum {
	DUMP_TO_TFTP = 0,
	DUMP_TO_USB,
	DUMP_TO_MEM,
	DUMP_TO_NVMEM,
	DUMP_TO_FLASH,
	DUMP_TO_EMMC,
};

enum {
	RESET_V1 = 1,
	RESET_V2,
};

enum {
	SDX_POWER_CYCLE	= 0,		/* Power cycle the SDX in crash path */
};

enum cmd_function_id {
	FUNC_LIST_FUSE,
	FUNC_DUMP_FUSE,
	FUNC_CHECK_SECURE_BOOT,
	FUNC_SECURE_AUTH,
	FUNC_FUSE_IPQ,
	FUNC_TZT_OPERATIONS,
	FUNC_AES_ENCRYPT,
	FUNC_AES_DECRYPT,
	FUNC_AES_DERIVE_KEY,
	FUNC_AES_DERIVE_KEY_MAX_CTXT,
	FUNC_AES_CLEAR_KEY,
	FUNC_IMAGE_AUTH,
	FUNC_AUTH_ROOTFS_ELF,
	FUNC_FUSEIPQ,
	FUNC_ICE_CONFIGURE,
	FUNC_ICE_KEY_CONFIGURE,
	FUNC_MAX
};

/* Communication types */
enum comm_type_id {
	COMM_TYPE_TME,
	COMM_TYPE_SCM,
	COMM_TYPE_OPTEE,
	COMM_TYPE_MAX
};
#if defined(CONFIG_SPL)
/* SPL Boot Modes */
enum {
	IPQ_SPL_BOOT_MODE_DEFAULT		= 0x0,
	IPQ_SPL_BOOT_MODE_SPL_INACTIVE		= 0x1,
	IPQ_SPL_BOOT_MODE_FORCE_INACIVE		= 0x2,
	IPQ_SPL_BOOT_MODE_FAILOVER_EN		= 0x3,
	IPQ_SPL_BOOT_MODE_INTERMEDIATE_RESET	= 0x4,
	IPQ_SPL_BOOT_MODE_MAX,
};
#endif /* CONFIG_SPL */

struct machid_dts_map {
	int machid;
	char *dts;
	char *priconfig;
	char *secconfig;
};

struct multidtb_config {
	struct machid_dts_map *list;
	int ncount;
	int index;
	char dts_base[BOARD_DTS_MAX_NAMELEN];
	char dts_name[BOARD_DTS_MAX_NAMELEN];
};

#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
struct gpt_pte_info {
	gpt_entry *gpt_pte;
	int ncount;
};
#endif

struct ipq_board_info {
#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
	struct udevice *smem_dev;
	struct ipq_smem_flash_info smem_info;
	struct ipq_smem_bootconfig_info bconfig_info;
	struct smem_ptable *ptable;
	struct soc_info ipq_socinfo;
#endif
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct spi_flash *sf;
#endif
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	struct gpt_pte_info mmc_gpt_pte;
	struct gpt_pte_info nor_gpt_pte;
#endif
	const u8 *comm_type_map;  /* Board-specific communication type mapping */
};

#if IS_ENABLED(CONFIG_MMC) || IS_ENABLED(CONFIG_NOR_BLK)
/* BLK part info */
struct blkpart_info {
	struct disk_partition *info;
	struct blk_desc *desc;
	char *name;
	int flash_type;
	int devnum;
	int isnand;
	bool verbose;
};
#endif

struct mbn_header {
	unsigned int image_type;
	unsigned int header_vsn_num;
	unsigned int image_src;
	unsigned int image_dest_ptr;
	unsigned int image_size;
	unsigned int code_size;
	unsigned int signature_ptr;
	unsigned int signature_size;
	unsigned int cert_chain_ptr;
	unsigned int cert_chain_size;
};

struct auth_cmd_buf {
	unsigned long type;
	unsigned long size;
	unsigned long addr;
};

struct dts_fixup {
	char *path;
	char *fixup[5];
	int ncount;
};

struct crashdump_infos{
	char name[DUMP_NAME_STR_MAX_LEN];/* dump name */
	uint64_t start_addr;		/* dump start addr */
	uint64_t size;			/* dump size
					   0xBAD0FF5E - get ram_size runtime,
					   otherwise specify size */
	uint8_t dump_level;		/* dump level
					   refer crashdump_level_t */
	uint32_t split_bin_sz;		/* split bin size
					   if non-zero means, if size is
					   greater than split_bin_sz, it will
					   dump entire region as seperate bin
					   of size split_bin_sz */
	uint8_t is_aligned_access:1;	/* If this flag is set,
					   'start' is considered a unaligned
					   address, so content will be copied
					   to a aligned one and gets dumped */
	uint8_t compression_support:1;	/* does this binary need to be
					   compressed ? non-zero means true. */
	uint8_t dumptoflash_support:1;	/* does this binary need to be
					   dumped in flash ? non-zero
					   means true. */
	uint8_t check_dump_support:1;	/* If this flag is set, a check is
					   being made for dump-specific
					   skip conditions. */
};

extern struct crashdump_infos *board_dumpinfo;
extern uint8_t *board_dump_entries;

#ifdef CONFIG_CB_CALIB
enum image_type {
	CAL_FW,
	BDF,
	CALDATA,
	RXGAIN,
	REGDB,
	FW_INI_CFG,
	MAX_IMG_TYPE,
};

struct image {
	u32 img_type;
	u32 img_host_addr;
	u32 img_sram_addr;
	u32 img_size;
} __packed;

struct uboot_cal_tlv {
	u32 magic;
	u32 pci_slot;
	u32 caldb_addr;
	u32 caldb_size;
	u32 hremote_addr;
	u32 hremote_size;
	u32 host_ddr_status;
	u32 rddm_addr;
	u32 rddm_size;
	u32 num_images;
	struct image img[MAX_IMG_TYPE];
} __packed;

struct file_info {
	u32 type;
	u32 sub_type;
	u32 offset;
	u32 size;
} __packed;

struct cal_fw_header {
	u32 magic;
	u32 num_files;
	struct file_info file[];
} __packed;

struct cal_per_dev_config {
	struct udevice *dev;
	u32 pci_slot_id;
	u32 board_id;
	u32 caldata_offset;
	u32 caldata_size;
	phys_addr_t cal_fw_image_addr;
	phys_addr_t hremote_addr;
	u32 hremote_size;
	phys_addr_t rddm_addr;
	u32 rddm_size;
	phys_addr_t caldb_addr;
	u32 caldb_size;
	phys_addr_t host_ddr_status;
};

struct cal_config {
	struct cal_fw_header *cal_fw_header;
	phys_addr_t ddr_base_addr;
	u32 ddr_rmem_size;
	u32 archive_owner_slot_id;
	phys_addr_t caldata_addr;
	struct cal_per_dev_config dev_cfg[CONFIG_IPQ_MAX_PCIE];
};

struct cal_dt_config {
	phys_addr_t rmem_base_addr;
	u32 rmem_size;
	u32 board_id;
	u32 caldata_offset;
	u32 pci_slot_id;
	u32 caldb_offset;
	u32 caldb_size;
	struct list_head list;
};
#endif

struct load_seg_info {
	uint32_t startAddr;       /**< Region start address (SoC view) */
	uint32_t endAddr;	 /**< Region end address (SoC view) */
};

#ifdef CONFIG_OPTEE
/* 64-bit version of load_seg_info for OPTEE */
struct load_seg_info_64 {
	u64 startAddr;       /**< Region start address (64-bit) */
	u64 endAddr;	     /**< Region end address (64-bit) */
};
#endif

/* All CMD-related parameters should be declared below*/
struct fuseipq_params {
#ifdef CONFIG_OPTEE
	u64 addr;
#else
	u32 addr;
#endif
#if defined (CONFIG_FUSEIPQ_V1) || (CONFIG_FUSEIPQ_V3)
	u32 size;
#endif
	unsigned long meta_data_size;
	struct load_seg_info *load_seg_buff;
	uint8_t load_seg_cnt;
	u32 fuse_status;
#ifdef CONFIG_OPTEE
	/* For OPTEE targets, fuseipq needs 64-bit addresses */
	struct load_seg_info_64 *load_seg_buff_64;
	uint8_t load_seg_cnt_64;
#endif
};

struct list_fuse_params {
	struct fuse_payload *fuse;
	u8 fuse_read_cnt;
	size_t size;
	size_t fuse_payload_size;
};

struct dump_fuse_params {
	struct fuse_payload *fuse;
	u8 fuse_read_cnt;
	size_t size;
	size_t fuse_payload_size;
};

struct check_secure_boot_params {
	struct fuse_payload *fuse;
	size_t size;
	size_t fuse_payload_size;
	bool *result;
};

struct secure_auth_params {
	u32 type;
	u32 addr;
	u32 size;
	u32 load_seg_info_size;
	struct load_seg_info *load_seg_buff;
	u8 load_seg_cnt;
	u32 relocate;
#ifdef CONFIG_SECURE_AUTH_V3
	u32 flags;
#endif
};

#ifdef CONFIG_IPQ_INLINE_ENCRYPTION
/* ICE hardware configuration structure */
struct ice_config_sec {
	u32 index;
	u8 key_size;
	u8 algo_mode;
	u8 key_mode;
};

/* ICE crypto algorithm modes */
enum ice_cryto_algo_mode {
	ICE_CRYPTO_ALGO_MODE_HW_AES_ECB = 0x0,
	ICE_CRYPTO_ALGO_MODE_HW_AES_XTS = 0x3,
};

/* ICE crypto key sizes */
enum ice_crpto_key_size {
	ICE_CRYPTO_KEY_SIZE_HW_128 = 0x0,
	ICE_CRYPTO_KEY_SIZE_HW_256 = 0x2,
};

/* ICE configuration parameters - unified for SCM/OP-TEE */
struct ice_configure_params {
	struct ice_config_sec *ice;
	size_t ice_size;
};

/* ICE key configuration parameters - unified for SCM/OP-TEE */
struct ice_key_configure_params {
	u32 seedtype;
	u8 key_size;
	u8 algo_mode;
	u8 *hex_data_context;
	u64 hex_data_len;
	u8 *hex_salt_context;
	u64 hex_salt_len;
};
#endif /* CONFIG_IPQ_INLINE_ENCRYPTION */

#ifdef CONFIG_CMD_AES_256
/* AES encryption/decryption parameters */
struct aes_256_params {
	void *req_ptr;
	size_t req_size;
};

struct aes_clear_key_params {
	u32 key_handle;
};

#ifndef CONFIG_AES_256_DERIVE_KEY
/* AES encryption/decryption request structure (without key derivation) */
struct crypto_aes_req_data_t {
	uint64_t type;
	uint64_t mode;
	uint64_t req_buf;
	uint64_t req_len;
	uint64_t ivdata;
	uint64_t iv_len;
	uint64_t resp_buf;
	uint64_t resp_len;
};
#else
/* AES key derivation parameters */
struct aes_derive_key_params {
	void *req_ptr;
	size_t req_size;
	uintptr_t *key_handle;
};

struct aes_derive_key_max_ctxt_params {
	void *req_ptr;
	size_t req_size;
	uintptr_t *key_handle;
};

/* AES key derivation structures */
#define MAX_CONTEXT_BUFFER_LEN_V1  64
#define MAX_CONTEXT_BUFFER_LEN_V2  128

struct crypto_aes_operation_policy {
	uint32_t operations;
	uint32_t algorithm;
};

struct crypto_aes_hwkey_policy {
	struct crypto_aes_operation_policy op_policy;
	uint32_t kdf_depth;
	uint32_t permissions;
	uint32_t key_type;
	uint32_t destination;
};

struct crypto_aes_hwkey_bindings_v1 {
	uint32_t bindings;
	uint32_t context_len;
	uint8_t context[MAX_CONTEXT_BUFFER_LEN_V1];
};

struct crypto_aes_derive_key_cmd_t_v1 {
	struct crypto_aes_hwkey_policy policy;
	struct crypto_aes_hwkey_bindings_v1 hw_key_bindings;
	uint32_t source;
	uint64_t mixing_key;
	uint64_t key;
};

/* V2 structures for max context (128 bytes) */
struct crypto_aes_hwkey_bindings_v2 {
	uint32_t bindings;
	uint32_t context_len;
	uint8_t context[MAX_CONTEXT_BUFFER_LEN_V2];
};

struct crypto_aes_derive_key_cmd_t_v2 {
	struct crypto_aes_hwkey_policy policy;
	struct crypto_aes_hwkey_bindings_v2 hw_key_bindings;
	uint32_t source;
	uint64_t mixing_key;
	uint64_t key;
};

/* AES encryption/decryption request structure (with key derivation) */
struct crypto_aes_req_data_t {
	uint64_t key_handle;
	uint64_t type;
	uint64_t mode;
	uint64_t req_buf;
	uint64_t req_len;
	uint64_t ivdata;
	uint64_t iv_len;
	uint64_t resp_buf;
	uint64_t resp_len;
};
#endif /* CONFIG_AES_256_DERIVE_KEY */
#endif /* CONFIG_CMD_AES_256 */

/*********************************************************************
 * Function declaration
 ********************************************************************/
#if defined(CONFIG_MMC)
int mmc_write_protect(struct mmc *mmc, unsigned int start_blk,
		      unsigned int cnt_blk, int set_clr);
#endif

void board_default_flash_protect(int flash_type);
/*
 * Generic API
 */
/**
 * ipq_update_board_name() - Update rdp for non available dts rdps
 */
void ipq_update_board_name(int machid, struct multidtb_config *dtb);
/**
 * ipq_board_early_init_f() - Do board specific early init f
 */
void ipq_board_early_init_f(void);
#if defined(CONFIG_SPL)
void ipq_spl_board_early_init_f(void);
/**
 * ipq_spl_pre_reset_seq() - Board-specific SPL pre-reset sequence
 *
 * This function is called before SPL reset to perform board-specific
 * cleanup or state preservation. Boards can override this weak function
 * to implement custom pre-reset behavior.
 */
void ipq_spl_pre_reset_seq(void);
#endif
/**
 * ipq_board_read_smem_info() - read and save smem information
 *
 * @pbdinfo - Board context  pointer to save smem infrmation.
 */
void ipq_board_read_smem_info(struct ipq_board_info *pbdinfo);
/**
 * ipq_board_info() - Get board context pointer
 *
 * Return struct ipq_board_info global pointer
 */
struct ipq_board_info *ipq_get_bdinfo(void);
/**
 * ipq_get_smem_info() - Get smem information pointer
 *
 * Return struct ipq_smem_flash_info - ipq_bdinfo->smem_info
 */
struct ipq_smem_flash_info *ipq_get_smem_info(void);
/**
 * ipq_get_socinfo() - Get SoC information
 *
 * Return struct soc_info - ipq_bdinfo->ipq_socinfo
 */
struct soc_info *ipq_get_socinfo(void);
/**
 * ipq_get_part_table() - Get SMEM partition table
 *
 * Return struct smem_ptable - ipq_bdinfo->ptable
 */
struct smem_ptable *ipq_get_part_table(void);
/**
 * ipq_get_valid_bank() - Get valid bank details
 *
 * Return 1 for secondary , 0 for primary
 */
int ipq_get_valid_bank(void);
/**
 * ipq_runtime_sf_env_update() - Update the ENV offset dynamically.
 * Applicable only for NOR flash
 */
void ipq_runtime_sf_env_update(void);
/*
 * ipq_update_sfi_block_size() - update flash block size.
 * Applicable only for NOR flash
 */
void ipq_update_sfi_block_size(void);
#if IS_ENABLED(CONFIG_MMC) || IS_ENABLED(CONFIG_NOR_BLK)
/**
 * ipq_part_get_info_by_name() - Get partition information from GPT
 *
 * @blkpart this structure contails of class and name of blk device
 * Return 0 if Found , otherwise error code
 */
int ipq_part_get_info_by_name(struct blkpart_info *blkpart);
#endif

/**
 * ipq_smem_getpart_from_offset - retrieve partition start and size
 * for given offset belongs to.
 * @part_name: offset for which part start and size needed
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * Returns 0 at success or -ENOENT otherwise.
 */
int ipq_smem_getpart_from_offset(uint32_t offset, uint32_t *start,
				 uint32_t *size);
int ipq_gpt_getpart_from_offset(uint32_t offset, uint32_t *pstart,
				uint32_t *psize, uint32_t flash_type);
/**
 * get_gpt_entry() - Get the GPT entry table
 *
 * @dev_desc blk device descriptor details
 * Return valid secondary GPT table. else NULL if Invalid GPT
 */
gpt_entry *get_gpt_entry(struct blk_desc *dev_desc);
/**
 * ipq_spi_probe() - Probe SPI-NOR
 *
 * Return spi_flash pointer if success otherwise NULL if failed
 */
struct spi_flash *ipq_spi_probe(void);
/**
 * ipq_board_late_init() - Board specific late init seq
 *
 * Return 0
 */
int ipq_board_late_init(void);
/**
 * is_board_support_image_auth() -  Find current board config support Image
 * Authentication or not
 * Return 1 if support otherwise 0.
 */
uint32_t is_board_support_image_auth(void);
/**
 * is_part_exceed_flash_size() - Find if smem partition exceed flash size or not
 *
 * @p smem table pointer
 * @psize total size of partition , start + size
 * Return true if exceed otherwise false.
 */
bool is_part_exceed_flash_size(struct smem_ptn *p, uint64_t psize);
/**
 * ipq_smem_get_flash_size() -  Find flash size of NOR or NAND
 *
 * @flash_type 0 for NOR , 1 for NAND
 * Return size of flash if found otherwise 0
 */
uint64_t ipq_smem_get_flash_size(uint32_t flash_type);
/**
 * ipq_get_part_block_size() -  Find erase blk size of NOR or NAND
 *
 * @p SMEM table pointer
 * @sfi SMEM table info
 * Return block size based on NAND or NOR based on partition attribute.
 */
uint32_t ipq_get_part_block_size(struct smem_ptn *p,
				struct ipq_smem_flash_info *sfi);
/**
 * ipq_get_current_board_flash_config() -  Find current board flash config
 *
 * @flash_type SMEM based flash type
 * Return NOR PLUS varinat otheriwse input flash type.
 */
int ipq_get_current_board_flash_config(int flash_type);
/**
 * ipq_getpart_offset_size - retrieve partition offset and size
 * @part_name - partition name
 * @offset - location where the offset of partition to be stored
 * @size - location where partition size to be stored
 *
 * retrieve partition offset and size in bytes with respect to the
 * partition specific flash block size
 */
int ipq_getpart_offset_size(char *part_name, uint32_t *offset, uint32_t *size);
/**
 * This function should only be used when sfi->flash_type is
 * SMEM_BOOT_SPI_FLASH
 * retrieve the which_flash flag based on partition name.
 * flash_var is 1 if partition is in NAND.
 * flash_var is 0 if partition is in NOR.
 * flash_var is -1 if partition is in EMMC.
 */
uint32_t ipq_find_flash_by_name(char *part_name);
/**
 * ipq_smem_getpart - retrieve partition start and size
 * @part_name: partition name
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * retrieve the start offset in blocks and size in blocks, of the
 * specified partition.
 */
int ipq_smem_getpart(char *part_name, uint32_t *start, uint32_t *size);
/**
 * mibib_ptable_init - initializes SMEM partition table
 *
 * @addr smem address in DDR
 * Return 0 if success otherwise error code.
 */
int mibib_ptable_init(unsigned int *addr);
/**
 * ipq_get_kernel_fs_part_details() - Update rootfs and HLOS partition
 *					details in smem
 * @flash_type flash type like SPI-NOR etc
 */
void ipq_get_kernel_fs_part_details(int flash_type);
/**
 * gpt_find_which_flash() - Find partition in NOR or NAND in NOT-GPT flash type
 *
 * @p partition information
 * Return 1 for nand , 0 for nor
 */
int gpt_find_which_flash(gpt_entry *p);
/**
 * ipq_ft_board_setup() - IPQ board specific fixup
 *
 * @blob kernel dts for fixup
 * @bd board info
 * Return 0
 */
int ipq_ft_board_setup(void *blob, struct bd_info *bd);
/**
 * parse_fdt_fixup() - Parse fixup provided in ENV
 *
 * @buf buffer with fixup strings
 * @blob of kernel dts
 */
void parse_fdt_fixup(char *buf, void *blob);
/**
 * ipq_get_partition_data() - read the data from partition at given offset.
 *
 * @part_name Partition name
 * @offset offset at partition
 * @buf to copy the read data
 * @size size to be read
 * @fl_type flash type
 * Return 0 if success otherwise error code.
 */
int ipq_get_partition_data(char *part_name, uint32_t offset, uint8_t *buf,
			size_t size, uint32_t fl_type);
/**
 * set_bootargs() - Set bootargs for kernel boot up
 *
 * Return 0 if success otherwise error code.
 */
int set_bootargs(void);
/**
 * config_select() - Set config(dts) for kernel boot up
 *
 * Return 0 if success otherwise error code.
 */
int config_select(void);
/**
 * boot_kernel() - Boot kernel
 *
 * Return if not successful
 */
int boot_kernel(void);
/**
 * image_authentication() - Auth the kernel images and rootfs in secure board
 *
 * Return 0 if success otherwise error code
 */
int image_authentication(void);
/**
 * read_kernel() - Read kernel images for kernel boot up
 *
 * Return 0 if success otherwise error code
 */
int read_kernel(void);
/**
 * check_bootconfig() - Find bootconfig active partition for kernel boot up
 *
 * Return 0 if success otherwise error code.
 */
int check_bootconfig(void);
/**
 * ipq_get_soc_hw_version() - Read SoC HW version
 *
 * Return HW version from TCSR read only register
 */
uint32_t ipq_get_soc_hw_version(void);
/* Only MBN header version that exists for boards without real dual-MBN
 * hardware (i.e. without CONFIG_IPQ_MVEW).
 */
#define MVEW_MBN_VERSION_DEFAULT	7
#ifdef CONFIG_IPQ_MVEW
/**
 * ipq_get_required_mbn_version() - Board-specific SoC HW version ->
 * required MVEW MBN metadata wrapper version mapping
 *
 * Return 7 or 8, the MBN wrapper version this board's fused SoC HW
 * version requires
 */
uint32_t ipq_get_required_mbn_version(void);
#endif
/**
 * ipq_setup_board_default_env() - Setup board specific default env.
 *
 */
void ipq_setup_board_default_env(void);
/**
 * ipq_set_ethmac_addr() - Push MAC address to env
 *
 */
void ipq_set_ethmac_addr(void);
/**
 * ipq_get_eth_mac_address() - Read MAC from ART partition.
 *
 * @enetaddr buffer for save MAC
 * @no_of_macs count
 * Return 0 or higher for success , negative error for failure.
 */
int ipq_get_eth_mac_address(uint8_t *enetaddr, int no_of_macs);
/**
 * ipq_read_tcsr_boot_misc() - Read TCSR BOOT MISK register
 *
  Return value of regsiter
 */
int ipq_read_tcsr_boot_misc(void);
/**
 * write_tcsr_boot_misc_reg() - write TCSR BOOT MISK register
 *
 * @mask masking bit
 * value value to be written
 * Return 0 if success , othwerwise error code.
 */
int write_tcsr_boot_misc_reg(uint32_t mask, uint32_t value);
/**
 * is_atf_enbled() - check ATF enabled or not
 *
 * Return true if enabled otherwise flase
 */
bool is_atf_enbled(void);
/**
 * ipq_smem_get_item() - Get the data from smem memory
 *
 * @ptr for store the data
 * @type - which type of data required like item type
 * @def default value
 * size of memory to be read
 * Return true if enabled otherwise flase
 */
void ipq_smem_get_item(void *ptr, int type, int def, size_t size);
/**
 * ipq_smem_get_item() - Get the data from smem memory
 *
 * @ptr for store the data
 * @type - which type of data required like item type
 * @def default value
 * size of memory to be read
 * Return true if enabled otherwise flase
 */
void ipq_smem_get_itemv2(void **ptr, int type, int def, size_t size);
/**
 * ipq_update_lmb_reservation() - update the lmb reserved region.
 *
 */
void ipq_update_lmb_reservation(void);

#ifdef CONFIG_DPR_VER_1_0
/**
 * execute_dprv1() - executes dpr in u-boot
 * @cmdtp - u-boot cmd table
 * @argc - no of cmd line arguments
 * @argv - array of character pointers listing all the arguments
 * Returns 0 if success otherwise error code.
 */
int execute_dprv1(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[]);
#elif CONFIG_DPR_VER_2_0
int execute_dprv2(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[]);
#elif CONFIG_DPR_VER_3_0
int execute_dprv3(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[]);
#endif

/**
 * ipq_bring_secondary_core_down() - brings the secondary core down
 * @state - state of the secondary core
 */
void ipq_bring_secondary_core_down(unsigned long state);

/**
 * ipq_is_secondary_core_off() - checks whether the secondary core is off
 * @cpuid - core id
 * Returns value of register 0 from SMC/HVC call result
 */
int ipq_is_secondary_core_off(unsigned long cpuid);

/**
 * ipq_bring_secondary_core_up() - brings the secondary core up
 * @cpuid - core id
 * @entry - secondary_cpu_init
 * @arg - address of the corresponding core in cpu_entry_arg
 *
 * Returns 0 if success otherwise error code.
 */
int ipq_bring_secondary_core_up(unsigned long cpuid, unsigned long entry,
				unsigned long arg);
/**
 * ipq_init_ubi_part() - inits ubi part in mtd
 *
 * Returns 0 if success otherwise error code.
 */
int ipq_init_ubi_part(void);

/**
 * ipq_ubi_get_volume_size() - gets the ubi volume size
 *
 * @volume is name of the volume in ubi.
 * returns the size in bytes if exist , else -ENODEV.
 */
long long ipq_ubi_get_volume_size(char *volume);

/**
 * ipq_get_training_part_info() - gets the training partition information
 * @offset - offset to nand training partition
 * @size - size of nand training partition
 *
 * Returns 0 if success otherwise error code.
 */
int ipq_get_training_part_info(uint32_t *offset, uint32_t *size);
/**
 * ipq_uboot_fdt_fixup() - Fixup to the u-boot dtb
 *
 * @blob - u-boot fdt blob
 8 @fixup_type - fixup type
 *
 * Returns 0 if success otherwise error code.
 */
int ipq_uboot_fdt_fixup(void *blob, enum fixup_type);
/**
 * board_cache_init() - Enable the cache specific to SoC
 *
 */
void board_cache_init(void);
/**
 * board_cache_init() - Enable the cache specific to SoC
 *
 * @reset_version - reset version specific to SoC
 */
void reset_crashdump(int reset_version);
/**
 * ipq_check_rootfs_authentication() - Check rootfs auth enablement check
 *
 * Return 1 if enabled else 0
 */
int ipq_check_rootfs_authentication(void);
/**
 * ipq_fdt_fixup_board() - Soc specific fixup
 *
 */
void ipq_fdt_fixup_board(void *blob);
/**
 * ipq_fdt_rootfs_auth_fixup() - Fixup rootfs auth fixup
 *
 * @blob - kernel blob for fixup
 */
void ipq_fdt_rootfs_auth_fixup(void *blob);
/**
 * ipq_fdt_fixup_smem() - Fixup smem info to kernel dts
 *
 * @blob - kernel dtb blob
 */
void ipq_fdt_fixup_smem(void *blob);
/**
 * ipq_board_update_RFA_settings() - update board RFA settings
 *
 */
void ipq_board_update_RFA_settings(void);
/**
 * is_valid_dump() - check if dump info is valid.
 *
 *@dump_name - name of the dump
 * Return true if valid else false.
 */
bool is_valid_dump(char *dump_name);
/**
 * is_version_rollback_support() - check if version rollback is supported.
 *
 * Return true if supported else false.
 */
bool is_version_rollback_support(void);
/**
 * update_nand_training_partition() - update nand training partition
 *
 * @sfi - smem flash info
 */
void update_nand_training_partition(struct ipq_smem_flash_info *sfi);
/**
 * ipq_get_training_part_info() - Get training partition info
 *
 * @offset - offset of training partition
 * @size - size of training partition
 */
int ipq_get_training_part_info(uint32_t *offset, uint32_t *size);
/**
 * is_valid_bootconfig() - validate bootconfig
 *
 * @binfo - bdconfig info for validation
 * Return true if valid otheriwse false.
 */
bool is_valid_bootconfig(struct ipq_smem_bootconfig_info *binfo);
/**
 * ipq_wdt_start() - start or stop wdt
 *
 * @start - true for start wdt , false for stop
 */
void ipq_wdt_start(bool start);
/**
 * ipq_wdt_expire() - Trigger wdt
 *
 */
void ipq_wdt_expire(void);
/**
 * ipq_iscrashed() - Find out if board crashed ?
 *
 * Return true if crashed else false
 */
bool ipq_iscrashed(void);
/**
 * ipq_board_gpio_config() - Configure gpio without driver node.
 *
 * @type - Platform-specific implementation for SoC
 */
void ipq_board_gpio_config(int type);
#ifdef CONFIG_IPQ_NAND
/*
 * get_nand_block_size() - Return  nand device erase block size
 *
 * dev_id - NAND device ID
 */
uint32_t get_nand_block_size(uint8_t dev_id);
#endif
// Partition label macros for consistent naming across all files
#ifdef CONFIG_PRPL_MMC_LABEL
#define KERNEL_ACTIVE_LABEL    "kernel-active"
#define KERNEL_INACTIVE_LABEL  "kernel-inactive"
#define ROOTFS_ACTIVE_LABEL    "rootfs-active"
#define ROOTFS_INACTIVE_LABEL  "rootfs-inactive"
#else
#define KERNEL_ACTIVE_LABEL    "0:HLOS"
#define KERNEL_INACTIVE_LABEL  "0:HLOS_1"
#define ROOTFS_ACTIVE_LABEL    "rootfs"
#define ROOTFS_INACTIVE_LABEL  "rootfs_1"
#endif

#ifdef CONFIG_BOOT_BANK_FIXUP
#ifdef CONFIG_BOOT_BANK_IMEM_READ
/* Booted bank enum values from SBL */
#define BOOTED_BANK_ACTIVE          1
#define BOOTED_BANK_INACTIVE        2
#define BOOTED_BANK_INACTIVE_FORCED 3
#endif
#endif

#ifdef CONFIG_CB_CALIB
/**
 * cal_qcn9224() - Start calibration on QCN9224 PCIe attach
 *
 * @debug - Enable verbose logging
 * Return 0 if success, error code in case of failure.
 */
int cal_qcn9224(int debug);
#endif

#ifdef CONFIG_IPQ_PCIE
/**
 * pci_select_window() - Reprogram the BAR remap control register based
 *			 on the offset.
 *
 * @bar0_base - BAR address
 * @offset - Offset to calculate the window
 */
void pci_select_window(uintptr_t bar0_base, uint32_t offset);

/**
 * print_error_code() - Print common BHI error code registers
 *
 * @bar0_base - BAR address
 * @pbl_log - Enable/Disable printing of PBL error logs
 */
void print_error_code(uintptr_t bar0_base, bool pbl_log);

/**
 * qcn92xx_global_soc_reset - Trigger SOC global register on QCN9224
 *
 * @bar0_base - BAR address
 * @force_reset - Write 1 if set, else write 5 to soc global reset register
 */
void qcn92xx_global_soc_reset(uintptr_t bar0_base, bool force_reset);
#endif
#endif
/**
 * ipq_comm_handler() - Handle communication requests based on function ID
 * @func_id: Function ID from enum cmd_function_id
 * @params: Parameters for the communication function
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_comm_handler(enum cmd_function_id func_id, void *params);
#ifdef CONFIG_BOOT_BANK_FIXUP
/**
 * ipq_get_booted_bank_info - Fetch the booted bank information from IMEM
 *
 * @booted_bank_str: Output pointer for booted bank string
 * Return: 0 on success, negative error code on failure
 */

int ipq_get_booted_bank_info(const char **booted_bank_str);

/**
 * append_partlabel_bootargs - Append bootargs with partlabel information
 *
 * @bootargs: Bootargs to append the partlabel information
 * @buflen: Buffer length
 * Return: 0 on success, negative error code on failure
 */

int append_partlabel_bootargs(char *bootargs, size_t buflen);

/**
 * fdt_set_booted_bank_property - Set booted-bank property in device tree
 *
 * @blob: Pointer to device tree blob
 * Return: 0 on success, negative error code on failure
 */

int fdt_set_booted_bank_property(void *blob);
#endif /* CONFIG_BOOT_BANK_FIXUP */

#ifdef CONFIG_IPQ_MINIDUMP
/* Stack pointer register selection for different SoCs */
enum stack_ptr_register {
	EL0 = 0,
	EL1,
	EL2,
	EL3,
};
#endif /* CONFIG_IPQ_MINIDUMP */
