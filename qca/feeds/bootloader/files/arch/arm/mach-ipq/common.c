// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <version.h>
#include <mach/ipq.h>
#include <env.h>
#include <net.h>
#include <asm/io.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#ifdef CONFIG_LMB
#include <lmb.h>
#endif
#ifdef CONFIG_WDT
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <wdt.h>
#endif
#ifdef CONFIG_CMD_UBI
#include <ubi_uboot.h>
#endif
#if defined(CONFIG_IPQ_ART_UBI_SUPPORT)
#include <linux/ctype.h>
#include <linux/mtd/partitions.h>
#endif
#ifdef CONFIG_CB_CALIB
#include <gzip.h>
#endif

#ifdef CONFIG_IPQ_TMEL_IPC_SUPPORT
#include <mailbox.h>
#include <linux/tmelcom-qmp.h>
#endif

/***********************************************************************
 * Global and constant
 **********************************************************************/
DECLARE_GLOBAL_DATA_PTR;

#define MMC_MID_MICRON			0xFE
#define MMC_PNM_MICRON			0x4D4D43333247

#define MMC_GET_MID(CID0)		(CID0 >> 24)
#define MMC_GET_PNM(CID0, CID1, CID2)	(((long long int)(CID0 & 0xff) << 40) | \
					((long long int)CID1 << 8) |\
					(CID2 >> 24))

#define MMC_CMD_SET_WRITE_PROT	  28
#define MMC_CMD_CLR_WRITE_PROT	  29

#define MMC_ADDR_OUT_OF_RANGE(resp)     ((resp >> 31) & 0x01)


/*
 * CSD fields
*/
#define WP_GRP_ENABLE(csd)		((csd[3] & 0x80000000) >> 31)
#define WP_GRP_SIZE(csd)		((csd[2] & 0x0000001f))
#define ERASE_GRP_MULT(csd)		((csd[2] & 0x000003e0) >> 5)
#define ERASE_GRP_SIZE(csd)		((csd[2] & 0x00007c00) >> 10)

#define EXT_CSD_BOOT_WP_B_PERM_WP_EN	(0x04)  /* permanent write-protect */

#ifndef CFG_UBI_FS_NAME
#define CFG_UBI_FS_NAME			"fs"
#endif

uint32_t g_load_addr;

#ifdef CONFIG_MMC
extern int mmc_send_status(struct mmc *mmc, unsigned int *status);
extern int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value);
#endif

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
static int g_eth_initalized = 0;
extern int eth_initialize(void);
#endif

#ifdef CONFIG_IPQ_PCIE
struct pci_device_id device_table[] = {
	{QCN_VENDOR_ID, QCN9224_DEVICE_ID},
	{}
};
#endif

#ifdef CONFIG_CB_CALIB
extern struct list_head *cal_list_head;
#endif

/***********************************************************************
 * Structure enum and static
 **********************************************************************/
#ifdef CONFIG_CRC32_BE
#define DO_CRC_BE(x) (crc = (tab[(((crc >> 24) ^ (x)) & 255)] ^ (crc << 8)))
static const u32 crc32table_be[] = {
	(0x00000000L), (0x04c11db7L), (0x09823b6eL), (0x0d4326d9L),
	(0x130476dcL), (0x17c56b6bL), (0x1a864db2L), (0x1e475005L),
	(0x2608edb8L), (0x22c9f00fL), (0x2f8ad6d6L), (0x2b4bcb61L),
	(0x350c9b64L), (0x31cd86d3L), (0x3c8ea00aL), (0x384fbdbdL),
	(0x4c11db70L), (0x48d0c6c7L), (0x4593e01eL), (0x4152fda9L),
	(0x5f15adacL), (0x5bd4b01bL), (0x569796c2L), (0x52568b75L),
	(0x6a1936c8L), (0x6ed82b7fL), (0x639b0da6L), (0x675a1011L),
	(0x791d4014L), (0x7ddc5da3L), (0x709f7b7aL), (0x745e66cdL),
	(0x9823b6e0L), (0x9ce2ab57L), (0x91a18d8eL), (0x95609039L),
	(0x8b27c03cL), (0x8fe6dd8bL), (0x82a5fb52L), (0x8664e6e5L),
	(0xbe2b5b58L), (0xbaea46efL), (0xb7a96036L), (0xb3687d81L),
	(0xad2f2d84L), (0xa9ee3033L), (0xa4ad16eaL), (0xa06c0b5dL),
	(0xd4326d90L), (0xd0f37027L), (0xddb056feL), (0xd9714b49L),
	(0xc7361b4cL), (0xc3f706fbL), (0xceb42022L), (0xca753d95L),
	(0xf23a8028L), (0xf6fb9d9fL), (0xfbb8bb46L), (0xff79a6f1L),
	(0xe13ef6f4L), (0xe5ffeb43L), (0xe8bccd9aL), (0xec7dd02dL),
	(0x34867077L), (0x30476dc0L), (0x3d044b19L), (0x39c556aeL),
	(0x278206abL), (0x23431b1cL), (0x2e003dc5L), (0x2ac12072L),
	(0x128e9dcfL), (0x164f8078L), (0x1b0ca6a1L), (0x1fcdbb16L),
	(0x018aeb13L), (0x054bf6a4L), (0x0808d07dL), (0x0cc9cdcaL),
	(0x7897ab07L), (0x7c56b6b0L), (0x71159069L), (0x75d48ddeL),
	(0x6b93dddbL), (0x6f52c06cL), (0x6211e6b5L), (0x66d0fb02L),
	(0x5e9f46bfL), (0x5a5e5b08L), (0x571d7dd1L), (0x53dc6066L),
	(0x4d9b3063L), (0x495a2dd4L), (0x44190b0dL), (0x40d816baL),
	(0xaca5c697L), (0xa864db20L), (0xa527fdf9L), (0xa1e6e04eL),
	(0xbfa1b04bL), (0xbb60adfcL), (0xb6238b25L), (0xb2e29692L),
	(0x8aad2b2fL), (0x8e6c3698L), (0x832f1041L), (0x87ee0df6L),
	(0x99a95df3L), (0x9d684044L), (0x902b669dL), (0x94ea7b2aL),
	(0xe0b41de7L), (0xe4750050L), (0xe9362689L), (0xedf73b3eL),
	(0xf3b06b3bL), (0xf771768cL), (0xfa325055L), (0xfef34de2L),
	(0xc6bcf05fL), (0xc27dede8L), (0xcf3ecb31L), (0xcbffd686L),
	(0xd5b88683L), (0xd1799b34L), (0xdc3abdedL), (0xd8fba05aL),
	(0x690ce0eeL), (0x6dcdfd59L), (0x608edb80L), (0x644fc637L),
	(0x7a089632L), (0x7ec98b85L), (0x738aad5cL), (0x774bb0ebL),
	(0x4f040d56L), (0x4bc510e1L), (0x46863638L), (0x42472b8fL),
	(0x5c007b8aL), (0x58c1663dL), (0x558240e4L), (0x51435d53L),
	(0x251d3b9eL), (0x21dc2629L), (0x2c9f00f0L), (0x285e1d47L),
	(0x36194d42L), (0x32d850f5L), (0x3f9b762cL), (0x3b5a6b9bL),
	(0x0315d626L), (0x07d4cb91L), (0x0a97ed48L), (0x0e56f0ffL),
	(0x1011a0faL), (0x14d0bd4dL), (0x19939b94L), (0x1d528623L),
	(0xf12f560eL), (0xf5ee4bb9L), (0xf8ad6d60L), (0xfc6c70d7L),
	(0xe22b20d2L), (0xe6ea3d65L), (0xeba91bbcL), (0xef68060bL),
	(0xd727bbb6L), (0xd3e6a601L), (0xdea580d8L), (0xda649d6fL),
	(0xc423cd6aL), (0xc0e2d0ddL), (0xcda1f604L), (0xc960ebb3L),
	(0xbd3e8d7eL), (0xb9ff90c9L), (0xb4bcb610L), (0xb07daba7L),
	(0xae3afba2L), (0xaafbe615L), (0xa7b8c0ccL), (0xa379dd7bL),
	(0x9b3660c6L), (0x9ff77d71L), (0x92b45ba8L), (0x9675461fL),
	(0x8832161aL), (0x8cf30badL), (0x81b02d74L), (0x857130c3L),
	(0x5d8a9099L), (0x594b8d2eL), (0x5408abf7L), (0x50c9b640L),
	(0x4e8ee645L), (0x4a4ffbf2L), (0x470cdd2bL), (0x43cdc09cL),
	(0x7b827d21L), (0x7f436096L), (0x7200464fL), (0x76c15bf8L),
	(0x68860bfdL), (0x6c47164aL), (0x61043093L), (0x65c52d24L),
	(0x119b4be9L), (0x155a565eL), (0x18197087L), (0x1cd86d30L),
	(0x029f3d35L), (0x065e2082L), (0x0b1d065bL), (0x0fdc1becL),
	(0x3793a651L), (0x3352bbe6L), (0x3e119d3fL), (0x3ad08088L),
	(0x2497d08dL), (0x2056cd3aL), (0x2d15ebe3L), (0x29d4f654L),
	(0xc5a92679L), (0xc1683bceL), (0xcc2b1d17L), (0xc8ea00a0L),
	(0xd6ad50a5L), (0xd26c4d12L), (0xdf2f6bcbL), (0xdbee767cL),
	(0xe3a1cbc1L), (0xe760d676L), (0xea23f0afL), (0xeee2ed18L),
	(0xf0a5bd1dL), (0xf464a0aaL), (0xf9278673L), (0xfde69bc4L),
	(0x89b8fd09L), (0x8d79e0beL), (0x803ac667L), (0x84fbdbd0L),
	(0x9abc8bd5L), (0x9e7d9662L), (0x933eb0bbL), (0x97ffad0cL),
	(0xafb010b1L), (0xab710d06L), (0xa6322bdfL), (0xa2f33668L),
	(0xbcb4666dL), (0xb8757bdaL), (0xb5365d03L), (0xb1f740b4L)
};
#endif

/**********************************************************************
 * Function declaration
 **********************************************************************/

/* Communication handler weak implementations */
__weak int ipq_list_fuse_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_dump_fuse_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_check_secure_boot_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_check_secure_boot_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_secure_auth_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_image_auth_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_image_auth_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_auth_rootfs_elf_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_auth_rootfs_elf_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_fuseipq_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_fuseipq_optee_impl(void *p)
{
	debug("OPTEE communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_ice_configure_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_ice_key_configure_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_ice_configure_optee_impl(void *p)
{
	debug("OPTEE communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_ice_key_configure_optee_impl(void *p)
{
	debug("OPTEE communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_256_enc_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_256_dec_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_derive_key_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_derive_key_max_ctxt_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_clear_key_tme_impl(void *p)
{
	debug("TME communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_256_enc_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_256_dec_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_derive_key_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_derive_key_max_ctxt_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

__weak int ipq_aes_clear_key_scm_impl(void *p)
{
	debug("SCM communication not supported on this platform\n");
	return -EOPNOTSUPP;
}

/* Function Pointer Table for communication handlers */
static int (*const comm_functions[FUNC_MAX][COMM_TYPE_MAX])(void *) = {
	[FUNC_LIST_FUSE]	 = {ipq_list_fuse_tme_impl, ipq_list_fuse_scm_impl, NULL},
	[FUNC_DUMP_FUSE]	 = {ipq_dump_fuse_tme_impl, ipq_dump_fuse_scm_impl, NULL},
	[FUNC_CHECK_SECURE_BOOT] = {ipq_check_secure_boot_tme_impl, ipq_check_secure_boot_scm_impl, NULL},
	[FUNC_SECURE_AUTH]	 = {ipq_secure_auth_tme_impl, ipq_secure_auth_scm_impl, NULL},
	[FUNC_IMAGE_AUTH]	 = {ipq_image_auth_tme_impl, ipq_image_auth_scm_impl, NULL},
	[FUNC_AUTH_ROOTFS_ELF]	 = {ipq_auth_rootfs_elf_tme_impl, ipq_auth_rootfs_elf_scm_impl, NULL},
	[FUNC_FUSEIPQ]		 = { NULL, ipq_fuseipq_scm_impl, ipq_fuseipq_optee_impl},
	[FUNC_ICE_CONFIGURE]	 = {NULL, ipq_ice_configure_scm_impl,
				    ipq_ice_configure_optee_impl},
	[FUNC_ICE_KEY_CONFIGURE] = {NULL, ipq_ice_key_configure_scm_impl,
				    ipq_ice_key_configure_optee_impl},
	[FUNC_FUSEIPQ]		 = {NULL, ipq_fuseipq_scm_impl, ipq_fuseipq_optee_impl},
	[FUNC_AES_ENCRYPT]	 = {ipq_aes_256_enc_tme_impl, ipq_aes_256_enc_scm_impl, NULL},
	[FUNC_AES_DECRYPT]	 = {ipq_aes_256_dec_tme_impl, ipq_aes_256_dec_scm_impl, NULL},
	[FUNC_AES_DERIVE_KEY]	 = {ipq_aes_derive_key_tme_impl, ipq_aes_derive_key_scm_impl, NULL},
	[FUNC_AES_CLEAR_KEY]	 = {ipq_aes_clear_key_tme_impl, ipq_aes_clear_key_scm_impl, NULL},
	[FUNC_AES_DERIVE_KEY_MAX_CTXT] = {ipq_aes_derive_key_max_ctxt_tme_impl, ipq_aes_derive_key_max_ctxt_scm_impl, NULL}
};

/**
 * ipq_comm_handler - Communication handler dispatcher
 * @func_id: Function ID (FUNC_LIST_FUSE, FUNC_SECURE_AUTH, etc.)
 * @params: Parameters to pass to the handler function
 *
 * This function routes communication requests to the appropriate handler
 * (SCM or TME) based on board-specific configuration.
 *
 * Returns: 0 on success, negative error code on failure
 */
int ipq_comm_handler(enum cmd_function_id func_id, void *params)
{
	enum comm_type_id comm_type;
	int (*func_ptr)(void *args);
	struct ipq_board_info *bdinfo = ipq_get_bdinfo();

	if (func_id >= FUNC_MAX) {
		debug("Invalid function ID: %d\n", func_id);
		return -EINVAL;
	}

	/* Get communication type from board-specific mapping, or default to SCM */
	if (!bdinfo || !bdinfo->comm_type_map) {
		comm_type = COMM_TYPE_SCM;
		debug("No comm_type_map available, defaulting to SCM for func_id %d\n",
		      func_id);
	} else {
		comm_type = bdinfo->comm_type_map[func_id];

		if (comm_type >= COMM_TYPE_MAX) {
			debug("Invalid communication type %d for function ID: %d, "
			      "defaulting to SCM\n", comm_type, func_id);
			comm_type = COMM_TYPE_SCM;
		}
	}

	/* Get function pointer from table */
	func_ptr = comm_functions[func_id][comm_type];
	if (!func_ptr) {
		switch (comm_type) {
			case COMM_TYPE_SCM:
				printf("SCM communication not supported\n");
			break;
			case COMM_TYPE_TME:
				printf("TME communication not supported\n");
			break;
			case COMM_TYPE_OPTEE:
				printf("OPTEE communication not supported\n");
			break;
			case COMM_TYPE_MAX:
				printf("Invalid communication type\n");
			break;
		};
		return -EOPNOTSUPP;
	}

	return func_ptr(params);
}

__weak uint32_t ipq_get_soc_hw_version(void)
{
	return readl(CONFIG_SOC_HW_VERSION_REG);
}

#ifdef CONFIG_IPQ_MVEW
/*
 * Default: boards without a real dual-MBN-version fuse mapping always
 * require MBN v7 -- the only version that existed before MVEW. IPQ5210,
 * the only target with CONFIG_IPQ_MVEW, overrides this with a
 * strong definition in its own board .c.
 */
__weak uint32_t ipq_get_required_mbn_version(void)
{
	return MVEW_MBN_VERSION_DEFAULT;
}
#endif

__weak void ipq_board_update_RFA_settings(void) {};

__weak void ipq_bind_optee_driver(void) {};

#ifdef CONFIG_CRC32_BE
uint32_t crc32_be(uint8_t const *addr, phys_size_t size)
{
	uint32_t i, crc = 0;
	const uint32_t *tab = crc32table_be;

	for (i = 0; i < size; ++i)
		DO_CRC_BE(*addr++);

	return crc;
}
#endif

int ipq_get_valid_bank(void)
{
	int boot;

	if (gd->board_type & INVALID_BOOT)
		boot =  -1;
	else {
		if (gd->board_type & ACTIVE_BOOT_SET)
			boot = 1;
		else
			boot = 0;
	}

	return boot;
}

#if defined(CONFIG_BOOTCONFIG_V2) || defined(CONFIG_BOOTCONFIG_V3)
uint32_t cal_bootconf_crc(struct ipq_smem_bootconfig_info *binfo)
{
	uint32_t size = 0, crc = 0;

#if defined(CONFIG_BOOTCONFIG_V3)
	size = sizeof(struct ipq_smem_bootconfig_info) - sizeof(binfo->crc);
#elif defined(CONFIG_BOOTCONFIG_V2)
	size = sizeof(struct ipq_smem_bootconfig_info) -
			sizeof(binfo->magic_end);
#endif

	crc = crc32_be((const unsigned char *)(uintptr_t)binfo, size);

	if (env_get("verbose"))
		printf("Calculated Bootconfig CRC = 0x%x\n", crc);

	return crc;
}

bool is_valid_bootconfig(struct ipq_smem_bootconfig_info *binfo)
{
	bool fstatus = false;
	uint32_t crc;

	if (IS_ERR_OR_NULL(binfo))
		return fstatus;

#if defined(CONFIG_BOOTCONFIG_V2)
	if (((binfo->magic_start != _SMEM_DUAL_BOOTINFO_MAGIC_START) &&
		(binfo->magic_start !=
			_SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_MODE) &&
		(binfo->magic_start !=
			_SMEM_DUAL_BOOTINFO_MAGIC_START_UNIFIED_FAILSAFE) &&
		(binfo->magic_start !=
		_SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_UNIFIED_FAILSAFE)) ||
		(binfo->magic_end != _SMEM_DUAL_BOOTINFO_MAGIC_END)) {
		/*
		 * Check if crc matches as second try
		 */
		crc = binfo->magic_end;
		if (crc && cal_bootconf_crc(binfo) == crc)
			fstatus = true;
	} else {
		fstatus = true;
	}
#elif defined(CONFIG_BOOTCONFIG_V3)
	crc = binfo->crc;

	if (crc && cal_bootconf_crc(binfo) == crc)
		fstatus = true;
#endif

	return fstatus;
}
#endif

#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
void *_smem_get_item(unsigned int item)
{
	int ret = 0;
	struct udevice *smem;
	size_t size;
	struct ipq_board_info *_bdinfo = ipq_get_bdinfo();

	if ((IS_ERR_OR_NULL(_bdinfo)) ||
		(IS_ERR_OR_NULL(_bdinfo->smem_dev))) {
		ret = uclass_get_device(UCLASS_SMEM, 0, &smem);
		if (ret < 0) {
			printf("Failed to find SMEM node %d\n", ret);
			return NULL;
		}
	} else {
		smem = _bdinfo->smem_dev;
	}

	if ((gd->flags & GD_FLG_RELOC) && _bdinfo && !(_bdinfo->smem_dev))
		_bdinfo->smem_dev = smem;

	return smem_get(smem, -1, item, &size);
}

void ipq_smem_get_itemv2(void **ptr, int type, int def, size_t size)
{
	void *temp = _smem_get_item(type);
	bool error = IS_ERR_OR_NULL(temp);

	switch (type) {
#if defined(CONFIG_BOOTCONFIG_V2) || defined(CONFIG_BOOTCONFIG_V3)
	case SMEM_BOOT_DUALPARTINFO:
		if (error) {
			*ptr = NULL;
			break;
		}

		if (is_valid_bootconfig(
			(struct ipq_smem_bootconfig_info *)temp))
			*ptr = temp;
		else
			*ptr = NULL;
		break;
#endif
	case SMEM_AARM_PARTITION_TABLE:
		struct smem_ptable *ptable = (struct smem_ptable *)temp;

		if ((error) || ((ptable->magic[0] != _SMEM_PTABLE_MAGIC_1) ||
			(ptable->magic[1] != _SMEM_PTABLE_MAGIC_2))) {
			debug("Failed to get SMEM_AARM_PARTITION_TABLE\n");
			*ptr =  NULL;
		} else {
			*ptr =  temp;
		}
		break;
	default:
		break;
	}
}

void ipq_smem_get_item(void *ptr, int type, int def, size_t size)
{
	void *temp = _smem_get_item(type);
	bool error = IS_ERR_OR_NULL(temp);

	switch (type) {
	case SMEM_HW_SW_BUILD_ID: {
		union ipq_platform *platform_type = (union ipq_platform *)temp;
		struct soc_info *ipq_socinfo = (struct soc_info *)(ptr);

		if (error)
			break;

		ipq_socinfo->cpu_type = platform_type->v1.id;
		ipq_socinfo->version = platform_type->v1.version;
		ipq_socinfo->soc_version_major =
			SOCINFO_VERSION_MAJOR(ipq_socinfo->version);
		ipq_socinfo->soc_version_minor =
			SOCINFO_VERSION_MINOR(ipq_socinfo->version);
		ipq_socinfo->machid = CONFIG_MACH_TYPE;
		break;
		}

	case SMEM_IMAGE_VERSION_TABLE:
		if (error) {
			printf("%s temp is NULL\n", __func__);
			return;
		}

		struct image_version_entry *img_version =
				(struct image_version_entry*)temp + 9;
		/*
		 * APPSBL version details have to be stored at the 10th index
		 * of the array of struct image_version_entry
		 */

		if (!img_version) {
			pr_err("Failed to get SMEM item for image version\n");
			break;
		}

		memcpy(img_version->image_index, "09", 2);
		memcpy(img_version->image_colon_sep1, ":", 1);
		snprintf(img_version->image_qc_version_string,
			IMAGE_QC_VERSION_STRING_LENGTH, "%s", U_BOOT_VERSION);
		break;

	case SMEM_BOOT_FLASH_TYPE:
		if (!error) {
			if (*(int *)temp == SMEM_BOOT_NO_FLASH)
				printf("Booting via recovery mode!!!\n");
		}
		fallthrough;
	default:
		if (error)
			*((uint32_t *)ptr) = def;
		else
			memcpy(ptr, temp, size);
	}
}
void ipq_board_read_smem_info(struct ipq_board_info *pbdinfo)
{
	struct ipq_smem_flash_info *smem_info;

	if (!pbdinfo)
		return;

	smem_info = &pbdinfo->smem_info;

	ipq_smem_get_item(NULL, SMEM_IMAGE_VERSION_TABLE,
				0,
				sizeof(struct image_version_entry));

	ipq_smem_get_item((void *)&smem_info->flash_type,
				SMEM_BOOT_FLASH_TYPE,
				SMEM_BOOT_NO_FLASH,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->flash_index,
				SMEM_BOOT_FLASH_INDEX,
				0,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->flash_chip_select,
				SMEM_BOOT_FLASH_CHIP_SELECT,
				0,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->flash_block_size,
				SMEM_BOOT_FLASH_BLOCK_SIZE,
				0,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->flash_density,
				SMEM_BOOT_FLASH_DENSITY,
				0,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->primary_mibib,
				SMEM_PARTITION_TABLE_OFFSET,
				0,
				sizeof(uint32_t));

	ipq_smem_get_itemv2((void **)&smem_info->binfo,
				SMEM_BOOT_DUALPARTINFO,
				0,
				sizeof(struct ipq_smem_bootconfig_info));

#if defined(CONFIG_BOOTCONFIG_V3)
	ipq_smem_get_item((void *)&smem_info->try_mode_inprogress,
				SMEM_TRY_MODE_INPROGRESS,
				0,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->edl_mode,
				SMEM_EDL_MODE,
				0,
				sizeof(uint32_t));
#endif
	ipq_smem_get_item((void *)&pbdinfo->ipq_socinfo,
				SMEM_HW_SW_BUILD_ID,
				0,
				sizeof(pbdinfo->ipq_socinfo));

#ifdef CONFIG_FAILSAFE_V2
	ipq_smem_get_item((void *)&smem_info->smembootset,
				SMEM_BOOT_SET_INFO,
				0,
				sizeof(uint32_t));

	ipq_smem_get_item((void *)&smem_info->smembootmode,
				SMEM_FAILSAFE_BOOT_MODE,
				0,
				sizeof(uint32_t));
#endif

	switch (smem_info->flash_type) {
	case SMEM_BOOT_MMC_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
	case SMEM_BOOT_NO_FLASH:
		pbdinfo->ptable = NULL;
		break;
	default:
		ipq_smem_get_itemv2((void **)&pbdinfo->ptable,
				SMEM_AARM_PARTITION_TABLE,
				0,
				-1);
	}
}
#endif

#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
gpt_entry *get_gpt_entry(struct blk_desc *dev_desc)
{
	struct ipq_board_info *_bdinfo = ipq_get_bdinfo();
	int *ncount = NULL, ret = 0;
	gpt_entry **pp_gpt_pte;

	ALLOC_CACHE_ALIGN_BUFFER_PAD(gpt_header, gpt_head, 1, dev_desc->blksz);

	if (dev_desc->uclass_id == UCLASS_MMC) {
		pp_gpt_pte = &_bdinfo->mmc_gpt_pte.gpt_pte;
		ncount = &_bdinfo->mmc_gpt_pte.ncount;
	} else if (dev_desc->uclass_id == UCLASS_SPI) {
		pp_gpt_pte = &_bdinfo->nor_gpt_pte.gpt_pte;
		ncount = &_bdinfo->nor_gpt_pte.ncount;
	} else
		return NULL;

	if (*pp_gpt_pte)
#ifdef UPDATE_GPT_RUNTIME
		free(*pp_gpt_pte);
		*pp_gpt_pte = NULL;
#else
		return *pp_gpt_pte;
#endif

	ret = gpt_repair_headers(dev_desc);
	if (ret == 0) {
		/* This function validates
		 * AND fills in the GPT header and PTE
		 * This gpt header and pte from backup gpt in success case.
		 */
		ret = gpt_verify_headers(dev_desc, gpt_head, pp_gpt_pte);
		if (ret == 0 && ncount != NULL)
			*ncount = le32_to_cpu(gpt_head->num_partition_entries);
		else
			*pp_gpt_pte = NULL;
	}

	if (ret || !(*pp_gpt_pte))
		return NULL;
	else
		return *pp_gpt_pte;
}

static struct blk_desc *ipq_get_blk_dev(uint32_t flash_type, int devnum)
{
	enum uclass_id id;

	switch (flash_type) {
	case SMEM_BOOT_MMC_FLASH:
		id = UCLASS_MMC;
		break;
	case SMEM_BOOT_NORGPT_FLASH:
		id = UCLASS_SPI;
		break;
	default:
		printf("unsupported flash type\n");
		id = 0xFF;
	}

	return (id == 0xFF) ? NULL : blk_get_devnum_by_uclass_id(id, devnum);
}

int ipq_gpt_getpart_from_offset(uint32_t offset, uint32_t *pstart,
				uint32_t *psize, uint32_t flash_type)
{
	struct blk_desc *dev = ipq_get_blk_dev(flash_type, 0);
	struct disk_partition info;
	uint32_t start = 0, size = 0;
	int p;

	if (dev == NULL)
		return -ENODEV;

	for (p = 1; p <= CONFIG_EFI_PARTITION_ENTRIES_NUMBERS; ++p) {
		if (part_get_info(dev, p, &info) != 0)
			break;

		start = (uint32_t)info.start * info.blksz;
		size = (uint32_t)info.size * info.blksz;

		if ((offset >= start) && (offset < (start + size))) {
			*pstart = start;
			*psize = size;
			return 0;
		}
	}

	return -ENODEV;
}

int ipq_part_get_info_by_name(struct blkpart_info *blkpart)
{
	struct blk_desc *dev;
	int ret;
#if defined(CONFIG_IPQ_NAND)
	gpt_entry *gpt_pte, *p;
#endif

	dev = ipq_get_blk_dev(blkpart->flash_type, blkpart->devnum);
	if (!dev) {
		printf("No such device\n");
		return -ENODEV;
	}
#ifdef CONFIG_EFI_PARTITION
	if ((dev->part_type == PART_TYPE_UNKNOWN) &&
		(blkpart->flash_type == SMEM_BOOT_MMC_FLASH))
		dev->part_type = PART_TYPE_EFI;
#endif

	blkpart->desc = dev;

	ret = part_get_info_by_name(dev, blkpart->name, blkpart->info);
	if (ret < 0) {
		if (blkpart->verbose)
			printf(" %s Partition not found, ret %d !!!\n",
				blkpart->name, ret);
		return -ENODEV;
	}

#if defined(CONFIG_IPQ_NAND)
	if (blkpart->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		gpt_pte = get_gpt_entry(dev);
		if (!gpt_pte) {
			printf("Failed to get gpt table entry\n");
			return -ENOENT;
		}
		p = &gpt_pte[ret - 1];

		blkpart->isnand = gpt_find_which_flash(p);
	}
#else
	blkpart->isnand = 0;
#endif

	return 0;
}
#endif

#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
struct spi_flash *ipq_spi_probe(void)
{
	struct spi_flash *sf = NULL;
	struct udevice *dev;
	int ret;

	ret = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS,
				     CONFIG_SF_DEFAULT_CS,
				     &dev);
	if (ret) {
#if IS_ENABLED(CONFIG_SPL)
		pr_err("SPI flash probe failed: %d\n", ret);
#else
		pr_err("SPI flash probe failed: %d\n", ret);
		/* env_set_default("spi_flash_probe_bus_cs() failed", 0); */
#endif /* CONFIG_SPL */
		return NULL;
	}

	sf = dev_get_uclass_priv(dev);
	if (!sf) {
#if IS_ENABLED(CONFIG_SPL)
		pr_err("SPI flash uclass priv is NULL\n");
#else
		printf("SPI flash uclass priv is NULL\n");
#endif /* CONFIG_SPL */
		return NULL;
	}

#if !IS_ENABLED(CONFIG_SPL)
	ipq_get_bdinfo()->sf = sf;
#endif /* CONFIG_SPL */

	return sf;
}
#else
struct spi_flash *ipq_spi_probe(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_NOR_BLK)
void ipq_update_sfi_block_size(void)
{
	struct ipq_smem_flash_info *smem_info = ipq_get_smem_info();

	if (IS_ERR_OR_NULL(smem_info))
		return;

	if (smem_info->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		struct spi_flash *flash = ipq_spi_probe();
			if (!flash) {
			printf("Failed to probe SPI flash\n");
			return;
		}
		/*
		* add flash details in sfi structure
		*/
		smem_info->flash_block_size = flash->sector_size;
		smem_info->flash_density = flash->size;

	}
}
#else
void ipq_update_sfi_block_size(void) {}
#endif

#if defined(CONFIG_RUNTIME_SF_ENV_UPDATE) && defined(CONFIG_ENV_IS_IN_SPI_FLASH)
static void ipq_update_env_offset(void)
{
	int i;
	struct ipq_board_info *_bdinfo = ipq_get_bdinfo();
	struct ipq_smem_flash_info *smem_info = ipq_get_smem_info();

	if (!_bdinfo->ptable || !smem_info)
		return;

	for (i = 0; i < _bdinfo->ptable->len; i++) {
		struct smem_ptn *p = &_bdinfo->ptable->parts[i];

		if (IS_ERR_OR_NULL(p))
			continue;

		if (!strncmp(p->name, "0:APPSBLENV", SMEM_PTN_NAME_MAX)) {
			g_env_offset  = ((loff_t)p->start) *
						smem_info->flash_block_size;
		}
	}
}

void ipq_runtime_sf_env_update(void)
{
	struct ipq_smem_flash_info *smem_info = ipq_get_smem_info();

	if (IS_ERR_OR_NULL(smem_info))
		return;

	/*
	 * probe Spi early for Env case
	 */
	ipq_spi_probe();

	switch (smem_info->flash_type) {
	case SMEM_BOOT_SPI_FLASH:
		ipq_update_env_offset();
		break;
#if defined(CONFIG_NOR_BLK)
	case SMEM_BOOT_NORGPT_FLASH: {
		struct disk_partition disk_info;
		struct blkpart_info bpart_info;
		int ret;

		BLK_PART_GET_INFO_S(bpart_info, "0:APPSBLENV", &disk_info,
					SMEM_BOOT_NORGPT_FLASH, true);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (!ret)
			g_env_offset = (u32)disk_info.start * disk_info.blksz;
		break;
	}
#endif
	default:
		;
	}
}
#else
void ipq_runtime_sf_env_update(void) {}
#endif
#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)

#ifdef CONFIG_IPQ_NAND
uint32_t get_nand_block_size(uint8_t dev_id)
{
	uint32_t block_size = 0;
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (mtd)
		block_size = mtd->erasesize;

	return block_size;
}
#endif

/*
 * get flash block size based on partition name.
 */
static inline uint32_t ipq_get_flash_block_size(char *name,
	struct ipq_smem_flash_info *smem)
{
#ifdef CONFIG_IPQ_NAND
	return (ipq_find_flash_by_name(name) == 1) ?
		get_nand_block_size(0)
			: smem->flash_block_size;
#else
	return smem->flash_block_size;
#endif
}

uint32_t ipq_get_part_block_size(struct smem_ptn *p,
	struct ipq_smem_flash_info *sfi)
{
#ifdef CONFIG_IPQ_NAND
	return (WHICH_FLASH(p) == 1) ?
		get_nand_block_size(0) : sfi->flash_block_size;
#else
	return sfi->flash_block_size;
#endif
}

/*
 * ipq_smem_getpart - retrieve partition start and size
 * @part_name: partition name
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * retrieve the start offset in blocks and size in blocks, of the
 * specified partition.
 */
int ipq_smem_getpart(char *part_name, uint32_t *start, uint32_t *size)
{
	unsigned int i;
	struct smem_ptable *ptable = ipq_get_part_table();
	struct smem_ptn *p;
#ifdef CONFIG_IPQ_NAND
	uint32_t bsize;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!sfi)
		return -ENODEV;

	if (!mtd)
		return -ENODEV;
#endif
	if (!ptable)
		return -ENODEV;

	for (i = 0; i < ptable->len; i++) {
		if (!strncmp(ptable->parts[i].name, part_name,
			     SMEM_PTN_NAME_MAX))
			break;
	}
	if (i == ptable->len)
		return -ENOENT;

	p = &ptable->parts[i];
#ifdef CONFIG_IPQ_NAND
	bsize = ipq_get_part_block_size(p, sfi);
#endif
	*start = p->start;

	if (p->size == (~0u)) {
		/*
		 * Partition size is 'till end of device', calculate
		 * appropriately
		 */
#ifdef CONFIG_IPQ_NAND
		*size = (mtd->size / bsize) - p->start;
#else
		*size = 0;
#endif
	} else {
		*size = p->size;
	}

	return 0;
}

/*
 * ipq_smem_getpart_from_offset - retrieve partition start and size
 * for given offset belongs to.
 * @part_name: offset for which part start and size needed
 * @start: location where the start offset is to be stored
 * @size: location where the size is to be stored
 *
 * Returns 0 at success or -ENOENT otherwise.
 */
int ipq_smem_getpart_from_offset(uint32_t offset, uint32_t *start,
					uint32_t *size)
{
	unsigned int i;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	struct smem_ptable *ptable = ipq_get_part_table();
	struct smem_ptn *p;
	uint32_t bsize;

	if (!sfi)
		return -EINVAL;

#ifdef CONFIG_IPQ_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!mtd)
		return -ENODEV;
#endif

	if (!ptable)
		return -ENODEV;

	for (i = 0; i < ptable->len; i++) {
		p = &ptable->parts[i];
		bsize = ipq_get_part_block_size(p, sfi);
		*start = p->start;

		if (p->size == (~0u)) {
		/*
		 * Partition size is 'till end of device', calculate
		 * appropriately
		 */
#ifdef CONFIG_IPQ_NAND
			*size = (mtd->size / bsize) - p->start;
#else
			*size = 0;
#endif
		} else {
			*size = p->size;
		}
		*start = *start * bsize;
		*size = *size * bsize;
		if (*start <= offset && *start + *size > offset)
			return 0;
	}

	return -ENOENT;
}
#else
int ipq_smem_getpart(char *part_name, uint32_t *start, uint32_t *size)
{
	return -ENODEV;
}

int ipq_smem_getpart_from_offset(uint32_t offset, uint32_t *start,
					uint32_t *size)
{
	return -ENODEV;
}
#endif

/*
 * This function should only be used when sfi->flash_type is
 * SMEM_BOOT_SPI_FLASH
 * retrieve the which_flash flag based on partition name.
 * flash_var is 1 if partition is in NAND.
 * flash_var is 0 if partition is in NOR.
 * flash_var is -1 if partition is in EMMC.
 */
uint32_t ipq_find_flash_by_name(char *part_name)
{
	struct smem_ptable *ptable = ipq_get_part_table();
	int i;
	int flash_var = -1;

	if (!ptable) {
		printf("SMEM ptable not found\n");
		return -ENOENT;
	}

	for (i = 0; i < ptable->len; i++) {
		struct smem_ptn *p = &ptable->parts[i];

		if (strcmp(p->name, part_name) == 0)
			flash_var = WHICH_FLASH(p);
	}

	return flash_var;
}

void ipq_get_kernel_fs_part_details(int flash_type)
{
	struct ipq_smem_flash_info *smem = ipq_get_smem_info();

	if (!smem)
		return;

	struct { char *name; struct ipq_part_entry *part; } entries[] = {
		{ KERNEL_ACTIVE_LABEL, &smem->hlos },
		{ KERNEL_INACTIVE_LABEL, &smem->hlos_1 },
		{ ROOTFS_ACTIVE_LABEL, &smem->rootfs },
		{ ROOTFS_INACTIVE_LABEL, &smem->rootfs_1 },
	};
	int ret, i;
	uint32_t start, size, bsize;
	struct ipq_part_entry *part;
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info;
	struct blkpart_info bpart_info;
#endif

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (smem->flash_secondary_type != 0 && i < 2)
			continue;

		part = entries[i].part;
#if defined(CONFIG_NOR_BLK)
		if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
			BLK_PART_GET_INFO_S(bpart_info, entries[i].name,
					&disk_info, flash_type, false);

			ret = ipq_part_get_info_by_name(&bpart_info);
		} else
#endif
		{
			ret = ipq_smem_getpart(entries[i].name, &start, &size);
		}

		if (ret) {
			debug("cdp: get part failed for %s\n",
				entries[i].name);
			part->offset = 0xBAD0FF5E;
			part->size = 0xBAD0FF5E;
		} else {
#if defined(CONFIG_NOR_BLK)
			if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
				bsize = disk_info.blksz;
				part->offset = (u32)disk_info.start * bsize;
				part->size = (u32)disk_info.size * bsize;
			} else
#endif
			{
				bsize = ipq_get_flash_block_size(
						entries[i].name,
						smem);
				part->offset = ((loff_t)start) * bsize;
				part->size = ((loff_t)size) * bsize;
			}
		}
	}
}

int ipq_get_current_board_flash_config(int flash_type)
{
	int ret;
	int board_type = flash_type;
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info;
	struct blkpart_info bpart_info;

	if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
		BLK_PART_GET_INFO_S(bpart_info, ROOTFS_ACTIVE_LABEL, &disk_info,
					flash_type, false);

		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret) {
#if defined(CONFIG_IPQ_NAND)
			/*
			 * Since GPT cannot be read, attempt to initialize
			 * NAND to verify if the secondary flash is NAND;
			 * otherwise, it is MMC
			 */
			if (get_nand_dev_by_index(0)) {
				board_type = SMEM_BOOT_NORPLUSNAND;
			} else
#endif
			{
				board_type = SMEM_BOOT_NORPLUSEMMC;
			}
		} else {
			if (bpart_info.isnand)
				board_type = SMEM_BOOT_NORPLUSNAND;
			else
				board_type = SMEM_BOOT_NORPLUSEMMC;
		}
	} else
#endif
	{
		ret = ipq_find_flash_by_name(ROOTFS_ACTIVE_LABEL);
		if (ret == -1)
			board_type = SMEM_BOOT_NORPLUSEMMC;
		else if (ret == 1)
			board_type = SMEM_BOOT_NORPLUSNAND;
		else if (ret == 0)
			board_type = SMEM_BOOT_SPI_FLASH;
		else
			return ret;
	}

	return board_type;
}

#if defined (CONFIG_IPQ_SMP_CMD_SUPPORT) || (CONFIG_IPQ_SMP64_CMD_SUPPORT)
static int ipq_qti_invoke_psci_fn_smc
		(unsigned long function_id, unsigned long arg0,
		 unsigned long arg1, unsigned long arg2)
{
	struct arm_smccc_res res;
	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);

	return res.a0;
}

int ipq_is_secondary_core_off(unsigned long cpuid)
{
#if defined(CONFIG_BASE_CPU_64BIT_BOOTUP)
	cpuid = cpuid << 8;
#endif
	return ipq_qti_invoke_psci_fn_smc(PSCI_0_2_FN_AFFINITY_INFO, cpuid, 0, 0);
}

void ipq_bring_secondary_core_down(unsigned long state)
{
	ipq_qti_invoke_psci_fn_smc(PSCI_0_2_FN_CPU_OFF, state, 0, 0);
}

int ipq_bring_secondary_core_up(unsigned long cpuid, unsigned long entry,
				unsigned long arg)
{
	int ret;
	unsigned long mpidr_cpuid = 0;
#if defined(CONFIG_BASE_CPU_64BIT_BOOTUP)
	mpidr_cpuid = cpuid << 8;
#else
	mpidr_cpuid = cpuid;
#endif
	ret = ipq_qti_invoke_psci_fn_smc(PSCI_0_2_FN_CPU_ON, mpidr_cpuid, entry,
					arg);
	if (ret) {
		printf("Enabling CPU%ld via psci failed! (ret : %d)\n",
								cpuid, ret);
		return CMD_RET_FAILURE;
	}

	printf("Enabled CPU%ld via psci successfully!\n", cpuid);
	return CMD_RET_SUCCESS;
}
#endif

#if defined(CONFIG_BOOTCONFIG_V3)
__weak int ipq_read_bootconfig(struct ipq_smem_flash_info *sfi)
{
	int ret = 0;

	sfi->binfo = (struct ipq_smem_bootconfig_info *)malloc(
				sizeof(struct ipq_smem_bootconfig_info));

	if (sfi->binfo == NULL) {
		printf("No Enough Memory\n");
		return 1;
	}

	ret = ipq_get_partition_data("0:BOOTCONFIG", 0,
			(uint8_t *)sfi->binfo,
			sizeof(struct ipq_smem_bootconfig_info),
			sfi->flash_type);

	if (ret < 0)
		return !!ret;

	if (!is_valid_bootconfig(sfi->binfo)) {
		printf("Invalid Bootconfig\n");
		free(sfi->binfo);
		sfi->binfo = NULL;
		ret = 1;
	}

	return ret;
}
#else
__weak int ipq_read_bootconfig(struct ipq_smem_flash_info *sfi)
{
	return 1;
}
#endif

static int ipq_get_rootfs_active_partition(struct ipq_smem_flash_info *sfi)
{
	/*
	 * set primary 0 as initial
	 */
	int ret = 0;

#ifndef CONFIG_FAILSAFE_V2
	struct ipq_smem_bootconfig_info *binfo = NULL;
	if (sfi != NULL)
		binfo = sfi->binfo;

	/*
	 * if bdinfo is NULL , consider primary as default valid boot
	 */
	if (binfo == NULL)
		return ret;
#endif

#if defined(CONFIG_BOOTCONFIG_V2)
	for (int i = 0; i < binfo->numaltpart; i++) {
		if (strncmp(ROOTFS_ACTIVE_LABEL, binfo->per_part_entry[i].name,
			CONFIG_RAM_PART_NAME_LENGTH) == 0) {
			ret = binfo->per_part_entry[i].primaryboot;
			break;
		}
	}
#elif defined(CONFIG_BOOTCONFIG_V3)
	uint32_t *image_set_status = &(binfo->image_set_status);
	uint32_t *boot_set = &(binfo->boot_set);

	if (*image_set_status == DONT_USE_SET_AB) {
		ret = BOOT_SET_INVALID;
	} else if (*boot_set == BOOT_SET_A) {
		if (*image_set_status == DONT_USE_SET_A)
			ret = BOOT_SET_B;
		else
			ret = *boot_set;
	} else if (*boot_set == BOOT_SET_B) {
		if (*image_set_status == DONT_USE_SET_B)
			ret = BOOT_SET_A;
		else
			ret = *boot_set;
	}

	if ((*image_set_status != DONT_USE_SET_AB) &&
		(sfi->try_mode_inprogress))
		ret = !ret;

	printf("Booting [SET %s]\n", ret ? "B" : "A");
#endif

#ifdef CONFIG_FAILSAFE_V2
	ret = sfi->smembootset;
	printf("Booting from [%s]\n", ret ? "inactive" : "active" );
#endif

	return ret;
}

void ipq_find_board_bdconfig(struct ipq_smem_flash_info *sfi)
{
	int active_part = 0;

	if (sfi->binfo == NULL)
		ipq_read_bootconfig(sfi);

	active_part = ipq_get_rootfs_active_partition(sfi);
	if (active_part >= 0)
		gd->board_type |= active_part ? ACTIVE_BOOT_SET : 0;
#if defined(CONFIG_BOOTCONFIG_V3)
	else
		gd->board_type |= INVALID_BOOT;
#endif
}

#if defined(CONFIG_SCM)
#ifdef CONFIG_SCM_V1
static bool is_secure_boot_v1(void)
{
	struct scm_param param;
	uint8_t *buff = NULL;
	int ret = -1;
	bool status = false;

	buff = (uint8_t *)malloc_cache_aligned(CONFIG_SYS_CACHELINE_SIZE);
	if (!buff) {
		printf("Unable allocate memory\n");
		return false;
	}

	do {
		ret = -ENOTSUPP;
		IPQ_SCM_SECURE_BOOT(param, (uintptr_t)buff, sizeof(uint8_t));
		ret = ipq_scm_call(&param);

		/* invalidate cache to update latest value in buff */
		invalidate_dcache_range((unsigned long)buff,
					(unsigned long)buff +
					CONFIG_SYS_CACHELINE_SIZE);

		if (!ret && *(uint8_t *)buff == 1)
			status  = true;
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
	}

	if (buff)
		free(buff);

	return status;
}

#elif CONFIG_SCM_V2
static bool is_secure_boot_v2(void)
{
	int ret = -1;
	struct fuse_payload *fuse = NULL;
	size_t size = sizeof(struct fuse_payload);
	bool status = false;
	struct check_secure_boot_params params;

	size = roundup(size, CONFIG_SYS_CACHELINE_SIZE);

	fuse = malloc_cache_aligned(size);
	if (!fuse)
		return false;

	memset(fuse, 0, sizeof(struct fuse_payload));

	fuse[0].fuse_addr = QFPROM_CORR_TME_OEM_ATE_ROW0_LSB;

	do {
		ret = -ENOTSUPP;
		params.fuse = fuse;
		params.size = size;
		params.fuse_payload_size = sizeof(struct fuse_payload);
		params.result = &status;

		ret = ipq_comm_handler(FUNC_CHECK_SECURE_BOOT, &params);

		if (ret) {
			ret = -1;
			break;
		}
	} while (0);

	if (ret == -ENOTSUPP) {
		printf("Unsupported SCM call\n");
	}

	if (fuse)
		free(fuse);

	return status;
}
#else
static bool is_secure_boot_fake(void)
{
	return false;
}

#endif

__weak bool is_atf_enbled(void)
{
	return false;
}

static void update_board_type(void)
{
	uint32_t board_type;

	board_type = gd->board_type;

	if (board_type & RECOVERY_MODE)
		return;

	if (is_secure_boot())
		board_type |= SECURE_BOARD;

	if (is_atf_enbled())
		board_type |= ATF_ENABLED;

	gd->board_type |= board_type;
}
#else
void update_board_type(void) {}
#endif

#ifdef CONFIG_EFI_PARTITION
static void update_part_type(int flash_type)
{
	enum uclass_id uclass_id = UCLASS_INVALID;
	struct blk_desc *dev;

	switch(flash_type) {
#ifdef CONFIG_IPQ_MMC
	case SMEM_BOOT_MMC_FLASH:
		uclass_id = UCLASS_MMC;
		break;
#endif
	case SMEM_BOOT_NORGPT_FLASH:
		uclass_id = UCLASS_SPI;
		break;
	default:
		break;
	}

	dev = blk_get_devnum_by_uclass_id(uclass_id, 0);

	if (dev != NULL && dev->part_type == PART_TYPE_UNKNOWN)
		dev->part_type = PART_TYPE_EFI;

	return;
}
#endif

#ifdef CONFIG_MMC
static void init_mmc(void)
{
	struct mmc *mmc;
	mmc = find_mmc_device(0);
	if (!mmc) {
		printf("no mmc device at slot 0\n");
		return;
	}

	if (mmc_init(mmc))
		printf("mmc init failed\n");
#ifdef CONFIG_EFI_PARTITION
	struct blk_desc *dev;
	dev = blk_get_devnum_by_uclass_id(UCLASS_MMC, 0);
	if (dev != NULL && dev->part_type == PART_TYPE_UNKNOWN)
		dev->part_type = PART_TYPE_EFI;
#endif

	return;
}
#endif

int ipq_board_late_init(void)
{
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	uint32_t board_type;

	if (!sfi)
		return -EINVAL;

	switch (sfi->flash_type) {
	case SMEM_BOOT_NORGPT_FLASH:
#ifdef CONFIG_EFI_PARTITION
		update_part_type(sfi->flash_type);
#endif
		fallthrough;
	case SMEM_BOOT_SPI_FLASH:
		board_type =
			ipq_get_current_board_flash_config(sfi->flash_type);
		break;
	default:
		board_type = sfi->flash_type;
	}

	switch (board_type) {
	case SMEM_BOOT_NORPLUSEMMC:
#ifdef CONFIG_MMC
		init_mmc();
#endif
#ifdef CONFIG_EFI_PARTITION
		update_part_type(board_type);
#endif
		sfi->flash_secondary_type = SMEM_BOOT_MMC_FLASH;
		break;
	case SMEM_BOOT_NORPLUSNAND:
		sfi->flash_secondary_type = SMEM_BOOT_QSPI_NAND_FLASH;
		break;
	default:
		sfi->flash_secondary_type = 0;
	}
#ifdef CONFIG_BOARD_TYPES
	gd->board_type |= board_type;
#endif

	update_board_type();

	if (SZ_256M >= gd->ram_size && CONFIG_SYS_LOAD_ADDR > SZ_256M)
		g_load_addr = CFG_SYS_SDRAM_BASE + SZ_64M;
	else
		g_load_addr = CONFIG_SYS_LOAD_ADDR;

	switch (sfi->flash_type) {
	case SMEM_BOOT_SPI_FLASH:
		fallthrough;
	case SMEM_BOOT_QSPI_NAND_FLASH:
		fallthrough;
	case SMEM_BOOT_NORGPT_FLASH:
		ipq_get_kernel_fs_part_details(sfi->flash_type);
		break;
	default:
		break;
	}

	ipq_find_board_bdconfig(sfi);
	/*
	 * setup mac address
	 */
	ipq_set_ethmac_addr();
	/*
	 * setup default env
	 */
	ipq_setup_board_default_env();
	/*
	 * Update cal data in CAP IN/OUT register.
	 */
	ipq_board_update_RFA_settings();

#ifdef CONFIG_MMC_FLASH_PARTITION_WRITE_PROTECT
	board_default_flash_protect(SMEM_BOOT_MMC_FLASH);
#endif

	/*
	 * Bind the optee driver
	 */
	ipq_bind_optee_driver();

	return 0;
}

uint64_t ipq_smem_get_flash_size(uint32_t flash_type)
{
	uint64_t flash_size = 0;

	switch (flash_type) {
	case 0: /* SPI_NOR_FLASH */
	#ifdef CONFIG_IPQ_SPI_NOR
		struct spi_flash *flash = ipq_spi_probe();

		if (flash)
			flash_size = flash->size;
	#endif
		break;
	case 1: /* NAND_FLASH*/
	#ifdef CONFIG_IPQ_NAND
		struct mtd_info *mtd = get_nand_dev_by_index(0);

		if (mtd)
			flash_size = mtd->size;
	#endif
		break;
	};


	return flash_size;
}

bool is_part_exceed_flash_size(struct smem_ptn *p, uint64_t psize)
{
	bool ret = false;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();

	if (!p || !sfi)
		return ret;

	/* NOR and NOR + eMMC */
	if ((sfi->flash_type == SMEM_BOOT_SPI_FLASH) &&
		(WHICH_FLASH(p) == 0)) {
		if (psize > ipq_smem_get_flash_size(0))
			ret = true;
	/* NAND and NOR + NAND */
	} else if ((sfi->flash_type == SMEM_BOOT_QSPI_NAND_FLASH) ||
		((sfi->flash_type == SMEM_BOOT_SPI_FLASH) &&
		(WHICH_FLASH(p) == 1))) {
		if (psize > ipq_smem_get_flash_size(1))
			ret = true;
	}

	return ret;
}

/*
 * ipq_getpart_offset_size - retrieve partition offset and size
 * @part_name - partition name
 * @offset - location where the offset of partition to be stored
 * @size - location where partition size to be stored
 *
 * retrieve partition offset and size in bytes with respect to the
 * partition specific flash block size
 */
int ipq_getpart_offset_size(char *part_name, uint32_t *offset, uint32_t *size)
{
	int i;
	uint32_t bsize;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	struct smem_ptable *ptable = ipq_get_part_table();

	if (!sfi || !ptable) {
		printf("%s: Failed to get flash info\n", __func__);
		return CMD_RET_FAILURE;
	}

#ifdef CONFIG_IPQ_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!mtd)
		return -ENODEV;
#endif
	for (i = 0; i < ptable->len; i++) {
		struct smem_ptn *p = &ptable->parts[i];
		loff_t psize;

		if (!strncmp(p->name, part_name, SMEM_PTN_NAME_MAX)) {
			bsize = ipq_get_part_block_size(p, sfi);
			if (p->size == (~0u)) {
				/*
				 * Partition size is 'till end of device',
				 * calculate appropriately
				 */
#ifdef CONFIG_IPQ_NAND
				psize = mtd->size - (((loff_t)p->start)*
							bsize);
#else
				psize = 0;
#endif
			} else {
				psize = ((loff_t)p->size) * bsize;
			}

		*offset = ((loff_t)p->start) * bsize;
		*size = psize;
		break;
		}
	}

	if (i == ptable->len)
		return -ENOENT;

	return 0;
}

/**
 * mibib_ptable_init - initializes SMEM partition table
 *
 * Initialize partition table from MIBIB.
 */
int mibib_ptable_init(unsigned int *addr)
{
	struct smem_ptable *mib_ptable;
	struct smem_ptable *ptable = ipq_get_part_table();
	struct ipq_board_info *bdinfo = ipq_get_bdinfo();

	if (!bdinfo)
		return -ENXIO;

	mib_ptable = (struct smem_ptable *) addr;
	if (mib_ptable->magic[0] != _SMEM_PTABLE_MAGIC_1 ||
		mib_ptable->magic[1] != _SMEM_PTABLE_MAGIC_2)
		return -ENOMSG;

	/* In recovery & mmc boot, ptable will not be initialized.
	 * So, allocate ptable memory in recovery mode.
	 */
	if (!ptable) {
		ptable = malloc(sizeof(struct smem_ptable));
		if (!ptable)
			return -ENOMEM;
		bdinfo->ptable = ptable;
	}

	memcpy(ptable, addr, sizeof(struct smem_ptable));
	return 0;
}

int gpt_find_which_flash(gpt_entry *p)
{
	/*
	 * bit 3 of gpt attribute denotes partition present in nand flash
	 */
	if (p->attributes.raw & CFG_IPQ_NAND_PART)
		return 1;

	return 0;
}

static void ipq_set_part_entry(char *name, struct ipq_smem_flash_info *smem,
		struct ipq_part_entry *part, uint32_t start, uint32_t size)
{
	uint32_t bsize = ipq_get_flash_block_size(name, smem);

	part->offset = ((loff_t)start) * bsize;
	part->size = ((loff_t)size) * bsize;
}

#if defined(CONFIG_IPQ_ART_UBI_SUPPORT)
static int ipq_nand_is_ubi_partition(loff_t byte_offset, loff_t part_size)
{
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	struct ubi_ec_hdr ec_hdr;
	size_t rlen = sizeof(ec_hdr);
	uint32_t magic, crc, hdr_crc;

	if (!mtd)
		return 0;

	if (nand_read_skip_bad(mtd, byte_offset, &rlen, NULL,
			       part_size, (uint8_t *)&ec_hdr) < 0)
		return 0;

	magic = be32_to_cpu(ec_hdr.magic);
	if (magic != UBI_EC_HDR_MAGIC)
		return 0;

	crc = crc32(UBI_CRC32_INIT, &ec_hdr, UBI_EC_HDR_SIZE_CRC);
	hdr_crc = be32_to_cpu(ec_hdr.hdr_crc);

	return (crc == hdr_crc) ? 1 : 0;
}

static void ipq_part_name_to_vol_name(const char *part_name,
				      char *vol_name, size_t vol_name_sz)
{
	const char *src = strchr(part_name, ':');
	size_t i;

	src = src ? src + 1 : part_name;
	for (i = 0; i < vol_name_sz - 1 && src[i]; i++)
		vol_name[i] = tolower((unsigned char)src[i]);
	vol_name[i] = '\0';
}

#ifndef CFG_UBI_ART_VOL_NAME
#define CFG_UBI_ART_VOL_NAME		"art"
#endif

static const char *ipq_ubi_vol_name_for_part(const char *part_name,
					     char *fallback_buf,
					     size_t fallback_buf_sz)
{
	if (!strcmp(part_name, "0:ART"))
		return CFG_UBI_ART_VOL_NAME;

	ipq_part_name_to_vol_name(part_name, fallback_buf, fallback_buf_sz);
	return fallback_buf;
}

static int ipq_nand_ubi_attach(const char *part_name,
			       loff_t byte_offset, loff_t byte_size,
			       const char **prev_part_out)
{
	struct mtd_info *master;
	struct ubi_device *cur;
	char env_str[80];
	const char *saved;
	char *saved_copy = NULL;
	int ret;

	if (prev_part_out)
		*prev_part_out = NULL;

	master = get_nand_dev_by_index(0);
	if (!master)
		return -ENODEV;

	cur = ubi_get_device(0);
	if (cur) {
		bool already = cur->mtd && !strcmp(cur->mtd->name, part_name);

		if (!already && prev_part_out)
			*prev_part_out = cur->mtd ? cur->mtd->name : NULL;
		ubi_put_device(cur);
		if (already)
			return 0;
	}

	saved = env_get("mtdparts");
	if (saved) {
		saved_copy = strdup(saved);
		if (!saved_copy)
			return -ENOMEM;
	}

	snprintf(env_str, sizeof(env_str),
		 "mtdparts=nand0:0x%llx@0x%llx(%s)",
		 (unsigned long long)byte_size,
		 (unsigned long long)byte_offset,
		 part_name);

	ret = env_set("mtdparts", env_str);
	if (ret) {
		free(saved_copy);
		return -EPERM;
	}

	ret = ubi_part((char *)part_name, NULL);

	env_set("mtdparts", saved_copy);
	free(saved_copy);

	return ret;
}
#endif /* CONFIG_IPQ_ART_UBI_SUPPORT */

int ipq_get_partition_data(char *part_name, uint32_t offset, uint8_t *buf,
			size_t size, uint32_t fl_type)
{
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	int flash_type, ret = 0, isnand = 0;
#ifdef CONFIG_IPQ_SPI_NOR
	struct spi_flash *flash = NULL;
#endif
	uint32_t start_blk;
	uint32_t blk_cnt;
	struct ipq_part_entry part;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	struct blkpart_info bpart_info;
	struct disk_partition disk_info;
	uint32_t start_blk_no, end_blk_no, blksz;
#if defined(CONFIG_MMC)
	struct mmc *mmc;
	unsigned char *mmc_blk = NULL;
	int i, rdatacnt = 0, buf_cur_pos = 0;
#endif
#endif

	memset(&part, 0, sizeof(struct ipq_part_entry));

	if (!sfi) {
		printf("%s: Failed to get flash info\n", __func__);
		return CMD_RET_FAILURE;
	}

	if ((sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) &&
		((fl_type == SMEM_BOOT_QSPI_NAND_FLASH) ||
		(fl_type == SMEM_BOOT_NAND_FLASH))) {
		flash_type = SMEM_BOOT_NORGPT_FLASH;
		isnand = 1;
	} else
		flash_type = fl_type;

	switch (flash_type) {
	case SMEM_BOOT_NAND_FLASH:
	case SMEM_BOOT_QSPI_NAND_FLASH:
	case SMEM_BOOT_SPI_FLASH:
		ret = ipq_smem_getpart(part_name, &start_blk, &blk_cnt);
		if (ret < 0) {
			debug("cdp: get part failed for %s\n",
					part_name);
			ret = -ENXIO;
			goto exit;
		} else {
			ipq_set_part_entry(part_name, sfi, &part, start_blk,
						blk_cnt);
			part.offset += offset;
		}
		break;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
#if defined(CONFIG_MMC)
	case SMEM_BOOT_MMC_FLASH:
		mmc = find_mmc_device(0);
		if (!mmc) {
			printf("Failed to find MMC device\n");
			ret = -ENODEV;
			break;
		}
		fallthrough;
#endif
	case SMEM_BOOT_NORGPT_FLASH:
		BLK_PART_GET_INFO_S(bpart_info, part_name, &disk_info,
					flash_type, true);
		ret = ipq_part_get_info_by_name(&bpart_info);
		if (ret)
			goto exit;

		blksz = disk_info.blksz;

		if (bpart_info.isnand || isnand) {
			blksz = disk_info.blksz;
			part.offset = disk_info.start * blksz;
			part.offset += offset;
			flash_type = SMEM_BOOT_QSPI_NAND_FLASH;
			break;
		}

		start_blk_no = (uint32_t) disk_info.start + (offset / blksz);
		end_blk_no = (uint32_t) disk_info.start +
				((offset + size) / blksz);

		if ((offset == 0) && (size % blksz == 0)) {
#ifdef CONFIG_BLK
			ret = blk_dread(bpart_info.desc, start_blk_no,
						size / blksz, buf);
			if (ret < 0) {
				printf("Blk read failed %d\n", ret);
				break;
			}

			goto exit;
#endif
		}

		if (flash_type == SMEM_BOOT_NORGPT_FLASH) {
			part.offset = disk_info.start * blksz;
			part.offset += offset;
			flash_type =  SMEM_BOOT_SPI_FLASH;
			break;
		}

#if defined(CONFIG_MMC)
		mmc_blk = (unsigned char *) malloc_cache_aligned(blksz);
		if (mmc_blk == NULL)
			return -ENOMEM;

		rdatacnt = size;
		for (i = start_blk_no; i <= end_blk_no; i++) {
#ifdef CONFIG_BLK
			ret = blk_dread(bpart_info.desc, i, 1, mmc_blk);
#else
			ret = mmc->block_dev.block_read(&mmc->block_dev,
						i, 1, mmc_blk);
#endif
			if (ret < 0) {
				printf("MMC: %s read failed %d\n", part_name,
					ret);
				break;
			}

			if (i == start_blk_no) {
				if (size <= blksz) {
					memcpy(buf, mmc_blk + (offset % blksz),
						size);
					if (start_blk_no == end_blk_no)
						break;
				} else {
					memcpy(buf, mmc_blk + (offset % blksz),
						blksz - (offset % blksz));
					buf_cur_pos +=
						(blksz - (offset % blksz));
					rdatacnt -= (blksz - (offset % blksz));
				}
			} else if (rdatacnt >= blksz) {
				memcpy(buf + buf_cur_pos, mmc_blk, blksz);
				rdatacnt -= blksz;
				buf_cur_pos += blksz;
			} else
				memcpy(buf + buf_cur_pos, mmc_blk, rdatacnt);
		}

		if (mmc_blk) {
			free(mmc_blk);
			mmc_blk = NULL;
		}
#endif
		break;
#endif
	default:
		printf("Unsupported BOOT flash type\n");
		ret = -ENXIO;
		break;
	}

#ifdef CONFIG_IPQ_SPI_NOR
	if ((flash_type == SMEM_BOOT_SPI_FLASH) ||
		(flash_type == SMEM_BOOT_NORGPT_FLASH)) {
		flash = ipq_spi_probe();
		if (flash == NULL) {
			printf("No SPI flash device found\n");
			ret = -ENODEV;
		} else {
			ret = spi_flash_read(flash, part.offset, size, buf);
		}
	}
#endif
#ifdef CONFIG_IPQ_NAND
	if ((flash_type == SMEM_BOOT_NAND_FLASH) ||
		(flash_type == SMEM_BOOT_QSPI_NAND_FLASH)) {
		struct mtd_info *mtd = get_nand_dev_by_index(0);

		if (!mtd) {
			printf("No NAND flash device found\n");
			ret = -ENODEV;
		} else {
#ifdef CONFIG_IPQ_ART_UBI_SUPPORT
			loff_t part_base = part.offset - (loff_t)offset;
			const char *prev_part = NULL;
			char vol_name_buf[SMEM_PTN_NAME_MAX];
			const char *vol_name;

			if (ipq_nand_is_ubi_partition(part_base, part.size)) {
				ret = ipq_nand_ubi_attach(part_name,
							  part_base,
							  part.size,
							  &prev_part);
				if (!ret) {
					vol_name = ipq_ubi_vol_name_for_part(
							part_name, vol_name_buf,
							sizeof(vol_name_buf));
					ret = ubi_volume_read((char *)vol_name,
							      (char *)buf,
							      (loff_t)offset,
							      size);

					run_command("ubi detach", 0);
				}

				goto exit;
			}
			/* Not UBI-formatted — use raw nand_read() below */
#endif /* CONFIG_IPQ_ART_UBI_SUPPORT */
			ret = nand_read(mtd, part.offset, &size, buf);
		}
	}
#endif

exit:

#if defined(CONFIG_MMC) && defined(CONFIG_SYS_MMC_ENV_PART)
	if (mmc_blk) {
		free(mmc_blk);
		mmc_blk = NULL;
	}
#endif

	return ret;
}

int ipq_get_eth_mac_address(uint8_t *enetaddr, int no_of_macs)
{
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	if (!sfi) {
		printf("%s: Failed to get flash info\n", __func__);
		return -EINVAL;
	}

	return ipq_get_partition_data("0:ART", 0, enetaddr, no_of_macs * 6,
					sfi->flash_type);
}

static bool ipq_is_art_part_present(struct ipq_smem_flash_info *sfi)
{
	uint32_t start_blk, blk_cnt;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	struct blkpart_info bpart_info;
	struct disk_partition disk_info;
#endif

	switch (sfi->flash_type) {
	case SMEM_BOOT_NAND_FLASH:
	case SMEM_BOOT_QSPI_NAND_FLASH:
	case SMEM_BOOT_SPI_FLASH:
		return ipq_smem_getpart("0:ART", &start_blk, &blk_cnt) == 0;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	case SMEM_BOOT_MMC_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
		BLK_PART_GET_INFO_S(bpart_info, "0:ART", &disk_info,
					sfi->flash_type, false);
		return ipq_part_get_info_by_name(&bpart_info) == 0;
#endif
	default:
		return false;
	}
}

void ipq_random_ethaddr(uchar *addr)
{
	u64 seed = get_ticks();
	int i;

	for (i = 0; i < 6; i++) {
		seed = seed * 6364136223846793005ULL + 1;
		addr[i] = (uchar)(seed >> 33);
	}

	addr[0] &= 0xfe;	/* clear multicast bit */
	addr[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

void ipq_set_ethmac_addr(void)
{
	int i, ret;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	uchar enetaddr[CONFIG_ETH_MAX_MAC * 6] = { 0 };
	uchar *mac_addr;
	char ethaddr[16] = "ethaddr";
	char mac[64];

	if (!sfi) {
		printf("%s: Failed to get flash info\n", __func__);
		return;
	}

	if (!ipq_is_art_part_present(sfi))
		return;

	ret = ipq_get_eth_mac_address(enetaddr, CONFIG_ETH_MAX_MAC);
	if (ret < 0)
		printf("Failed to read MAC from flash %d\n", ret);

	for (i = 0; (i < CONFIG_ETH_MAX_MAC); i++) {
		mac_addr = &enetaddr[i * 6];

		if (!is_valid_ethaddr(mac_addr)) {
			printf("MAC%d Address from ART is not valid\n", i);
			ipq_random_ethaddr(mac_addr);
		}

		/*
		 * U-Boot uses these to patch the 'local-mac-address'
		 * dts entry for the ethernet entries, which in turn
		 * will be picked up by the HLOS driver
		 */
		snprintf(mac, sizeof(mac), "%x:%x:%x:%x:%x:%x",
				mac_addr[0], mac_addr[1], mac_addr[2],
				mac_addr[3], mac_addr[4], mac_addr[5]);
		env_set(ethaddr, mac);

		snprintf(ethaddr, sizeof(ethaddr), "eth%daddr", (i + 1));
	}
}

void ipq_setup_board_default_env(void)
{
	uint32_t soc_hw_version;
	struct soc_info *ipq_socinfo = NULL;

	/*
	 * setup machid
	 */
	env_set_hex("machid", gd->bd->bi_arch_number);

#ifdef CONFIG_PREBOOT
	/*
	 * forceset preboot env to avoid SDI/crashdump path system bootup
	 */
	env_set("preboot", CONFIG_PREBOOT);
#endif
	/*
	 * set soc hw version in env
	 */
	soc_hw_version = ipq_get_soc_hw_version();
	if (soc_hw_version)
		env_set_hex("soc_hw_version", soc_hw_version);

	ipq_socinfo = ipq_get_socinfo();
	if (ipq_socinfo) {
		env_set_ulong("soc_version_major", ipq_socinfo->soc_version_major);
		env_set_ulong("soc_version_minor", ipq_socinfo->soc_version_minor);
	}
#ifdef CFG_CUSTOM_LOAD_ADDR
	env_set_hex("loadaddr", CFG_CUSTOM_LOAD_ADDR);
#endif
}

void setup_board_default_env(void)
{
#if !IS_ENABLED(CONFIG_SPL)
	ipq_setup_board_default_env();
#endif /* !CONFIG_SPL */
}

int write_tcsr_boot_misc_reg(uint32_t mask, uint32_t value)
{
	int ret = 0;
#if defined(CONFIG_SCM)
	struct scm_param param;
#endif
	unsigned int cookie = ipq_read_tcsr_boot_misc();

	cookie = (cookie & ~mask) | (value & mask);

	if (gd->board_type & RECOVERY_MODE)
		writel(cookie, TCSR_BOOT_MISC_REG);
#if defined(CONFIG_SCM)
	else {
		do {
			ret = -ENOTSUPP;
			IPQ_SCM_IO_WRITE(param, (uintptr_t)TCSR_BOOT_MISC_REG,
						cookie);
			ret = ipq_scm_call(&param);

			if (ret) {
				printf("Error in TCSR_BOOT_MISC_REG write\n");
				return ret;
			}
		} while (0);

		if (ret == -ENOTSUPP)
			printf("Unsupported SCM call\n");
	}
#endif

	return ret;
}

#ifdef CONFIG_BOOT_BANK_FIXUP
#ifdef CONFIG_BOOT_BANK_IMEM_READ
/* ipq_get_booted_bank_info() - Get booted bank string from IMEM
 * @bank_str: Output pointer for bank string
 *
 * Reads booted bank enum from IMEM and converts to string:
 * - 0 -> "active"
 * - 1 -> "inactive"
 * - 2 -> "inactive,forced"
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_get_booted_bank_info(const char **booted_bank_str)
{
	static const char *booted_bank[] = {
		[BOOTED_BANK_ACTIVE] = "active",
		[BOOTED_BANK_INACTIVE] = "inactive",
		[BOOTED_BANK_INACTIVE_FORCED] = "inactive,forced",
	};
	uint32_t booted_bank_value;

	if (!booted_bank_str)
		return -EINVAL;

	booted_bank_value = readl(BOOTED_BANK_ADDR_IMEM);

	/* Validate enum value */
	if (booted_bank_value < BOOTED_BANK_ACTIVE ||
		booted_bank_value > BOOTED_BANK_INACTIVE_FORCED) {
		debug("Invalid booted bank value: %u\n", booted_bank_value);
		return -EINVAL;
	}

	*booted_bank_str = booted_bank[booted_bank_value];
	debug("Booted bank: %s (enum=%u)\n", *booted_bank_str, booted_bank_value);

	return 0;
}
#elif CONFIG_BOOT_BANK_SMEM_READ
/* ipq_get_booted_bank_info() - Get booted bank string from SMEM
 * @bank_str: Output pointer for bank string
 *
 * Reads booted bank enum from SMEM and converts to string:
 * - 0 -> "active"
 * - 1 -> "inactive"
 * - 2 -> "inactive,forced"
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_get_booted_bank_info(const char **booted_bank_str)
{
	static const char *booted_bank[] = {
		[BOOT_MODE_NORMAL] = "active",
		[BOOT_MODE_SPL_INACTIVE] = "inactive",
		[BOOT_MODE_FORCE_INACTIVE] = "inactive,forced",
	};
	uint32_t booted_bank_value;

	size_t size;
	struct udevice *dev;
	uint32_t smembootset, smembootmode;
	uint32_t *readsmemval = NULL;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	readsmemval = smem_get(dev, -1, SMEM_FAILSAFE_BOOT_MODE, &size);
	if (IS_ERR_OR_NULL(readsmemval)) {
		debug("Failed to get SMEM item: SMEM_FAILSAFE_BOOT_MODE\n");
		return -ENODEV;
	}
	smembootmode = *readsmemval;

	readsmemval = smem_get(dev, -1, SMEM_BOOT_SET_INFO, &size);
	if (IS_ERR_OR_NULL(readsmemval)) {
		debug("Failed to get SMEM item: SMEM_BOOT_SET_INFO\n");
		return -ENODEV;
	}
	smembootset = *readsmemval;

	if (!booted_bank_str)
		return -EINVAL;

	booted_bank_value = smembootset;

	if (smembootmode == BOOT_MODE_FORCE_INACTIVE)
		booted_bank_value = BOOT_MODE_FORCE_INACTIVE;

	/* Validate enum value */
	if (booted_bank_value < BOOT_MODE_NORMAL ||
		booted_bank_value > BOOT_MODE_FORCE_INACTIVE) {
		debug("Invalid booted bank value: %u\n", booted_bank_value);
		return -EINVAL;
	}

	*booted_bank_str = booted_bank[booted_bank_value];
	debug("Booted bank: %s (enum=%u)\n", *booted_bank_str, booted_bank_value);

	return 0;
}
#endif

#ifdef CONFIG_FAILSAFE
/* append_partlabel_bootargs() - Append bootargs with partlabel information
 * @bootargs: bootargs to append the partlabel information
 * @buflen: Buffer length
 *
 * Reads booted bank info from IMEM and append the corresponding partlabel:
 * - "active" -> rootfs-active
 * - "inactive" / "inactive,forced" -> rootfs-inactive
 *
 * Return: 0 on success, negative error code on failure
 */

int append_partlabel_bootargs(char *bootargs, size_t buflen)
{
	const char *booted_bank_str;
	const char *part_name;
	char append_str[64];
	size_t current_len, append_len, required_len;
	int ret;

	if (!bootargs || buflen == 0)
		return -EINVAL;

	ret = ipq_get_booted_bank_info(&booted_bank_str);
	if (ret)
		return ret;

	/* Determine partition name based on booted bank */
	if (strcmp(booted_bank_str, "inactive") == 0 ||
	    strcmp(booted_bank_str, "inactive,forced") == 0) {
		part_name = "rootfs-inactive";
	} else if (strcmp(booted_bank_str, "active") == 0) {
		part_name = "rootfs-active";
	} else {
		printf("Booted bank information not found\n");
		return -EINVAL;
	}

	/* Append PARTLABEL=<partition> to bootargs */
	snprintf(append_str, sizeof(append_str), " PARTLABEL=%s", part_name);

	/* Check if we have enough space before appending */
	current_len = strlen(bootargs);
	append_len = strlen(append_str);
	required_len = current_len + append_len + 1; /* +1 for null terminator */

	if (required_len > buflen) {
		printf("ERROR: bootargs buffer overflow!\n");
		debug("required: %zu, available:%zu\n", required_len, buflen);
		return -ENOSPC;
	}

	strlcat(bootargs, append_str, buflen);

	return 0;
}
#elif CONFIG_FAILSAFE_V2
/* append_partlabel_bootargs() - Append bootargs with partlabel information
 * @bootargs: bootargs to append the partlabel information
 * @buflen: Buffer length
 *
 * Reads booted bank info from SMEM and append the corresponding partlabel:
 * - "active" -> rootfs-active
 * - "inactive" -> rootfs-inactive
 *
 * Return: 0 on success, negative error code on failure
 */

int append_partlabel_bootargs(char *bootargs, size_t buflen)
{
	const char *part_name;
	char append_str[64];
	size_t current_len, append_len, required_len;
	size_t size;
	struct udevice *dev;
	uint32_t smembootset;
	uint32_t *readsmemval = NULL;

	if (!bootargs || buflen == 0)
		return -EINVAL;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	readsmemval = smem_get(dev, -1, SMEM_BOOT_SET_INFO, &size);
	if (IS_ERR_OR_NULL(readsmemval)) {
		debug("Failed to get SMEM item: SMEM_BOOT_SET_INFO\n");
		return -ENODEV;
	}
	smembootset = *readsmemval;

	if (smembootset == BOOT_FROM_SET_ACTIVE)
		part_name = "rootfs-active";
	else if (smembootset == BOOT_FROM_SET_INACTIVE)
		part_name = "rootfs-inactive";
	else {
		printf("Booted bank information not found\n");
		return -EINVAL;
	}

	/* Append PARTLABEL=<partition> to bootargs */
	snprintf(append_str, sizeof(append_str), " PARTLABEL=%s", part_name);

	/* Check if we have enough space before appending */
	current_len = strlen(bootargs);
	append_len = strlen(append_str);
	required_len = current_len + append_len + 1; /* +1 for null terminator */

	if (required_len > buflen) {
		printf("ERROR: bootargs buffer overflow!\n");
		debug("required: %zu, available:%zu\n", required_len, buflen);
		return -ENOSPC;
	}

	strlcat(bootargs, append_str, buflen);

	return 0;
}
#endif

/**
 * fdt_set_booted_bank_property() - Set booted-bank property in device tree
 * @blob: Pointer to device tree blob
 *
 * Reads booted bank information from SMEM and sets the u-boot,booted-bank
 * property under /chosen node.Creates the /chosen node if it doesn't exist.
 * This helper function is used by both kernel and U-Boot device tree
 * fixup functions.
 *
 * Return: 0 on success, -EFAULT on failure
 */
int fdt_set_booted_bank_property(void *blob)
{
	int ret;
	const char *booted_bank_str;

	if (!blob) {
		printf("Invalid FDT blob pointer\n");
		return -EINVAL;
	}

	ret = ipq_get_booted_bank_info(&booted_bank_str);
	if(ret)
		return ret;

	/* patch the booted-bank information in dt blob */
	ret = fdt_find_and_setprop(blob, "/chosen", "u-boot,booted-bank",
			booted_bank_str, strlen(booted_bank_str) + 1,
			1);
	if (ret) {
		printf("Failed to update u-boot,booted-bank: %d\n", ret);
		return ret;
	}

	debug("Successfully set u-boot,booted-bank = %s\n", booted_bank_str);
	return 0;
}
#endif /* CONFIG_BOOT_BANK_FIXUP */

#if defined(CONFIG_LMB)
void ipq_update_lmb_reservation(void)
{
	struct lmb *lmb = lmb_get();
	struct alist *lmb_rgn_lst;
	struct lmb_region *rgn;
	int i;

	if (!lmb)
		return;

	lmb_rgn_lst = &lmb->used_mem;
	rgn = lmb_rgn_lst->data;

	if (CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION) &&
	    !(gd->flags & GD_FLG_SKIP_RELOC)) {
		/*
		 * Normal relocation path: U-Boot image is at gd->relocaddr
		 * (top of highest DDR bank minus SZ_2M reserved for noncached
		 * DMA + alignment).  Trim each DDR no-overwrite reservation:
		 *   - If it contains gd->relocaddr: keep [relocaddr, ram_top)
		 *     so the image and noncached region are protected; this
		 *     also frees the stack/heap area below relocaddr for
		 *     lmb_alloc to use when loading the kernel.
		 *   - Otherwise (e.g. a wholesale Bank 1 reservation): remove
		 *     it entirely so the kernel can use the full bank.
		 * LMB_NOMAP regions (SMEM, TFA, OP-TEE) are left untouched.
		 */
		phys_addr_t uboot_start = gd->relocaddr;
		phys_addr_t uboot_end   = gd->ram_top;
		int j;

		for (i = 0; i < lmb_rgn_lst->count; ) {
			bool in_ddr = false;

			if (rgn[i].flags & LMB_NOMAP) {
				i++;
				continue;
			}

			for (j = 0; j < CONFIG_NR_DRAM_BANKS; j++) {
				if (!gd->bd->bi_dram[j].size)
					break;
				if (rgn[i].base >= gd->bd->bi_dram[j].start &&
				    rgn[i].base < (gd->bd->bi_dram[j].start +
						   gd->bd->bi_dram[j].size)) {
					in_ddr = true;
					break;
				}
			}

			if (!in_ddr) {
				i++;
				continue;
			}

			if (uboot_start >= rgn[i].base &&
			    uboot_start < rgn[i].base + rgn[i].size) {
				rgn[i].base = uboot_start;
				rgn[i].size = uboot_end - uboot_start;
				i++;
			} else {
				memmove(&rgn[i], &rgn[i + 1],
					(lmb_rgn_lst->count - i - 1) * sizeof(*rgn));
				lmb_rgn_lst->count--;
			}
		}
	} else {
		/*
		 * Skip-reloc (crashdump) path or non-dynamic build: U-Boot
		 * runs in-place at CONFIG_TEXT_BASE.  lmb_reserve_uboot_region
		 * reserved from rsv_start to bank_end, covering the entire top
		 * of RAM.  Trim the reservation that spans CONFIG_TEXT_BASE to
		 * free the memory above the in-place image for the kernel.
		 */
		for (i = 0; i < lmb_rgn_lst->count; i++) {
			if ((rgn[i].base < CONFIG_TEXT_BASE) &&
				((rgn[i].base + rgn[i].size - 1) > CONFIG_TEXT_BASE)) {
				rgn[i].size = ((CONFIG_TEXT_BASE + CONFIG_TEXT_SIZE +
						SZ_1M) - rgn[i].base);
				break;
			}
		}
	}
}
#else
void ipq_update_lmb_reservation(void) {}
#endif
#if defined(CONFIG_MMC)
static int mmc_send_wp_set_clr(struct mmc *mmc, unsigned int start,
					unsigned int size, int set_clr)
{
	unsigned int err;
	unsigned int wp_group_size, count, i;
	struct mmc_cmd cmd;
	unsigned int status;

	wp_group_size = (WP_GRP_SIZE(mmc->csd) + 1) * mmc->erase_grp_size;
	count = DIV_ROUND_UP(size, wp_group_size);

	if (set_clr)
		cmd.cmdidx = MMC_CMD_SET_WRITE_PROT;
	else
		cmd.cmdidx = MMC_CMD_CLR_WRITE_PROT;

	cmd.resp_type = MMC_RSP_R1b;

	for (i = 0; i < count; i++) {
		cmd.cmdarg = start + (i * wp_group_size);
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err) {
			printf("%s: Error at block 0x%x - %d\n",
					__func__,cmd.cmdarg, err);
			return err;
		}

		err = mmc_send_status(mmc, &status);
		if (err)
			return err;

		if (MMC_ADDR_OUT_OF_RANGE(status)) {
			printf("%s: mmc block(0x%x) out of range",
				__func__,cmd.cmdarg);
			return -EINVAL;
		}
	}

	return 0;
}

int mmc_write_protect(struct mmc *mmc, unsigned int start_blk,
		      unsigned int cnt_blk, int set_clr)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
	int err;
	unsigned int wp_group_size;

	if (!WP_GRP_ENABLE(mmc->csd))
		return -1; /* group write protection is not supported */

	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err) {
		debug("ext_csd register cannot be retrieved\n");
		return err;
	}

	if ((ext_csd[EXT_CSD_USER_WP] & EXT_CSD_BOOT_WP_B_SEC_WP_SEL) ||
		(ext_csd[EXT_CSD_USER_WP] & EXT_CSD_BOOT_WP_B_PERM_WP_EN)) {
		printf("User power-on write protection is disabled. \n");

		return -1;
	}

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_USER_WP,
				EXT_CSD_BOOT_WP_B_PWR_WP_EN);
	if (err) {
		printf("Failed to enable user power-on write protection\n");

		return err;
	}

	wp_group_size = (WP_GRP_SIZE(mmc->csd) + 1) * mmc->erase_grp_size;

	if ((MMC_GET_MID(mmc->cid[0]) == MMC_MID_MICRON) &&
		(MMC_GET_PNM(mmc->cid[0],
			mmc->cid[1],mmc->cid[2]) == MMC_PNM_MICRON))
		wp_group_size *= 2;

	if (!cnt_blk || start_blk % wp_group_size || cnt_blk % wp_group_size) {
		printf("Error: Unaligned offset/count. offset/count should be "
				"aligned to 0x%x blocks\n", wp_group_size);

		return -1;
	}

	err = mmc_send_wp_set_clr(mmc, start_blk, cnt_blk, set_clr);

	return err;
}
#endif
#ifdef CONFIG_MMC_FLASH_PARTITION_WRITE_PROTECT
static inline int is_readonly(gpt_entry *p)
{
	/* bit 60 of gpt attribute denotes read-only flag */
	if (p->attributes.raw & ((unsigned long long)1 << 60))
		return 1;

	return 0;
}

static void mmc_wp(void)
{
	int num_part;
	struct mmc *mmc;
	struct blk_desc *mmc_dev;
	struct disk_partition info;
	gpt_entry *gpt_pte = NULL;
	struct ipq_board_info *_bdinfo = ipq_get_bdinfo();

	mmc = find_mmc_device(0);
	if (!mmc) {
		printf("no mmc device found\n");
		goto out;
	}

	mmc_dev = mmc_get_blk_desc(mmc);

	if (mmc_dev != NULL && mmc_dev->type != DEV_TYPE_UNKNOWN) {
		gpt_pte = get_gpt_entry(mmc_dev);
		if (!gpt_pte) {
			printf("%s: Failed to get gpt table entry\n", __func__);
			goto out;
		}

		num_part = _bdinfo->mmc_gpt_pte.ncount;

		if (num_part < 0) {
			printf("Both primary & backup GPT are invalid,"
					" skipping mmc write protection.\n");
			goto out;
		}

#ifdef CONFIG_EFI_PARTITION
		for (uint8_t part = 1; part <= num_part; part++) {
			uint8_t readonly = is_readonly(&gpt_pte[part - 1]);
			if (readonly) {
				if (part_get_info(mmc_dev, part, &info))
					continue;

				if (!mmc_write_protect(mmc,
						  info.start,
						  info.size, 1))
					printf("\"%s\""
						"-protected MMC partition\n",
						info.name);
				else
					printf("Write protect failed for "
							"\"%s\"", info.name);
			}
		}
#endif
	}
out:
	if (gpt_pte)
		free(gpt_pte);

	return;
}
#endif

void board_default_flash_protect(int flash_type)
{
	switch(flash_type) {
#ifdef CONFIG_MMC_FLASH_PARTITION_WRITE_PROTECT
	case SMEM_BOOT_MMC_FLASH:
		mmc_wp();
		break;
#endif
	default:
		break;
	}
}

void arch_preboot_os(void)
{
/*
 * restrict booting if image is not authenticated
 * in secure board.
 */
	uint32_t board_type = gd->board_type;

	if (!is_board_support_image_auth())
		return;

	if (board_type & KERNEL_AUTH_SUCCESS) {
		if (ipq_check_rootfs_authentication())
			if (board_type & ROOTFS_AUTH_SUCCESS)
				return;
			else
				reset_cpu();
		else
			return;
	} else {
		reset_cpu();
	}

	return;
}

#ifdef CONFIG_CMD_UBI
long long ipq_ubi_get_volume_size(char *volume)
{
	int i;
	struct ubi_device *ubi = ubi_get_device(0);
	struct ubi_volume *vol = NULL;

	if(NULL == ubi)
		return -ENODEV;

	for (i = 0; i < ubi->vtbl_slots; i++) {
		vol = ubi->volumes[i];
		if (vol && !strcmp(vol->name, volume))
			return vol->used_bytes;
	}

	printf("Volume %s not found!\n", volume);

	return -ENODEV;
}
#endif

#ifdef CONFIG_IPQ_NAND
void update_nand_training_partition(struct ipq_smem_flash_info *sfi)
{
	uint32_t offset, part_size;
	int ret = -1;
	struct ipq_part_entry *part = &sfi->training;
#if defined(CONFIG_NOR_BLK)
	struct disk_partition disk_info;
	struct blkpart_info bpart_info;

	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		BLK_PART_GET_INFO_S(bpart_info, "0:TRAINING", &disk_info,
					sfi->flash_type, true);

		ret = ipq_part_get_info_by_name(&bpart_info);
	} else
#endif
		if (sfi->flash_type != SMEM_BOOT_NO_FLASH)
			ret = ipq_getpart_offset_size("0:TRAINING", &offset,
							&part_size);
	if (ret) {
		part->offset = 0xBAD0FF5E;
		part->size = 0xBAD0FF5E;
	} else {
#if defined(CONFIG_NOR_BLK)
		if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
			part->offset = (u32)disk_info.start * disk_info.blksz;
			part->size = (u32)disk_info.size * disk_info.blksz;
		} else
#endif
		{
			part->offset = offset;
			part->size = part_size;
		}
	}

}

int ipq_get_training_part_info(uint32_t *offset, uint32_t *size)
{
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();

	if (!sfi) {
		printf("%s: Failed to get flash info\n", __func__);
		return CMD_RET_FAILURE;
	}

	struct ipq_part_entry *part = &sfi->training;

	if (!part) {
		printf("%s: part info not found\n", __func__);
		return -EINVAL;
	}

	if (part->offset == 0)
		update_nand_training_partition(sfi);

	if (part->offset == 0xBAD0FF5E)
		return 1;
	else {
		*offset = part->offset;
		*size = part->size;
	}

	return 0;
}
#endif

#ifdef CONFIG_CMD_UBI
int ipq_init_ubi_part(void)
{
	int ret;
	uint32_t offset = 0;
	uint32_t part_size = 0;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	struct ubi_device *ubi = ubi_get_device(0);
	char env_strings[64];
	bool need_attach = true;

	if (!sfi) {
		printf("%s: smem flash info not found\n", __func__);
		return -EINVAL;
	}

	/*
	 * Already attached to the fs volume: nothing to do. Attached to
	 * something else (e.g. ART, displaced by ipq_nand_ubi_attach()):
	 * release our reference here, then (re)attach the fs volume below.
	 */
	if (ubi) {
		if (ubi->mtd && !strcmp(ubi->mtd->name, CFG_UBI_FS_NAME))
			need_attach = false;
		ubi_put_device(ubi);
	}

	if (need_attach) {
#if defined(CONFIG_BOOTCONFIG_V3) || defined(CONFIG_FAILSAFE_V2)
		if (gd->board_type & ACTIVE_BOOT_SET) {
			offset = sfi->rootfs_1.offset;
			part_size = sfi->rootfs_1.size;
		} else {
			offset = sfi->rootfs.offset;
			part_size = sfi->rootfs.size;
		}
#else
		offset = sfi->rootfs.offset;
		part_size = sfi->rootfs.size;
#endif

		if ((part_size == 0xBAD0FF5E) || (offset == 0xBAD0FF5E))
			return -ENOENT;

		snprintf(env_strings, sizeof(env_strings),
			"mtdparts=nand0:0x%x@0x%x(%s)", part_size, offset,
				CFG_UBI_FS_NAME);

		ret = env_set("mtdparts", env_strings);
		if (ret)
			return -EPERM;

		ret = ubi_part(CFG_UBI_FS_NAME, NULL);
		if (ret)
			return -EPERM;
	}

	return 0;
}
#endif

#ifdef CONFIG_STANDALONE_GPIO
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
		printf("%s: dev is NULL\n", __func__);
		return;
	}

	err = gpio_request_by_name_nodev(dev_ofnode(dev), "e911_gpio", 0,
					 &e911_gpio, GPIOD_IS_IN);
	if (err) {
		printf("%s: sdx e911_gpio not found in DT!\n", __func__);
	} else if (dm_gpio_get_value(&e911_gpio)) {
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

/*
 * NOP driver: only for GPIO configuration
 */
static const struct udevice_id gpio_ids[] = {
	{ .compatible = "gpio,config", },
	{ }
};

U_BOOT_DRIVER(gpio) = {
	.name		= "gpio",
	.id		= UCLASS_NOP,
	.of_match	= gpio_ids,
};

void ipq_board_gpio_config(int type)
{
	switch(type) {
#ifdef CONFIG_SDX_ATTACH_SUPPORT
	case SDX_POWER_CYCLE:
		ipq_board_power_cycle_sdx();
		break;
#endif
	default:
		break;
	}
}
#endif

#ifdef CONFIG_WDT
void ipq_wdt_expire(void)
{
	int ret = 0;
	static struct udevice *wdt_dev;

	ret = uclass_get_device_by_seq(UCLASS_WDT, 0, &wdt_dev);
	if (ret)
		printf("WDT disabled\n");
	else
		wdt_expire_now(wdt_dev, 0);
}
#endif

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
int initr_net(void)
{
	if (g_eth_initalized == 0) {
		puts("Net:   ");
		eth_initialize();
		g_eth_initalized = 1;
	} else {
		return 1;
	}

	return 0;
}
#endif

#if defined(CONFIG_WDT)
void ipq_wdt_start(bool start)
{
	u32 timeout = 0;
	struct udevice *dev;
	int ret;

	if (uclass_find_device_by_seq(UCLASS_WDT, 0, &dev))
		return;

	if (start) {
		timeout = dev_read_u32_default(dev, "timeout-sec", timeout);
		if (timeout) {
			ret = wdt_start(dev, timeout * 1000, 0);
			if (ret != 0)
				printf("WDT: Failed to start %s\n",
					dev->name);
		}
	} else
		wdt_stop(dev);
}
#endif

/**
 * ipq_read_tcsr_boot_misc() - read boot tcsr register
 */
__weak int ipq_read_tcsr_boot_misc(void)
{
	u32 *dmagic = TCSR_BOOT_MISC_REG;
	return *dmagic;
}

#if IS_ENABLED(CONFIG_SPL)
__weak void ipq_spl_pre_reset_seq(void) {}

void reset_cpu(void)
{
	ipq_spl_pre_reset_seq();

	/**
	 * Trigger PSHOLD reset by writing 0 to PSHOLD register
	 */
	writel(0, PSHOLD_CTL_BASE);

	/**
	 * Wait for reset to take effect
	 */
	mdelay(10000);

	/**
	 * Should never reach here - fallback to infinite loop
	 */
	printf("SPL: ERROR - Reset failed! System hanging...\n");

	while (1)
		asm volatile("wfi");
}
#endif /* CONFIG_SPL */

#ifdef CONFIG_IPQ_PCIE
void pci_select_window(uintptr_t bar0_base, u32 offset)
{
	u32 window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;
	u32 prev_window = 0, curr_window = 0, prev_cleared_window = 0;

	prev_window = readl(bar0_base + QCN9224_PCIE_REMAP_BAR_CTRL_OFFSET);

	/* Clear out last 6 bits of window register */
	prev_cleared_window = prev_window & ~(0x3f);

	/* Write the new last 6 bits of window register. Only window 1 values
	 * are changed. Window 2 and 3 are unaffected.
	 */
	curr_window = prev_cleared_window | window;

	writel(WINDOW_ENABLE_BIT | curr_window,
	       bar0_base + QCN9224_PCIE_REMAP_BAR_CTRL_OFFSET);
}

void print_error_code(uintptr_t bar0_base, bool pbl_log)
{
	int i;
	u32 val;
	struct {
		char *name;
		u32 offset;
	} error_reg[] = {
		{ "ERROR_CODE", BHI_ERRCODE },
		{ "ERROR_DBG1", BHI_ERRDBG1 },
		{ "ERROR_DBG2", BHI_ERRDBG2 },
		{ "ERROR_DBG3", BHI_ERRDBG3 },
		{ "BHI_STATUS", BHI_STATUS },
		{ "BHI_IMGADDR_HIGH", BHI_IMGADDR_HIGH },
		{ "BHI_IMGADDR_LOW", BHI_IMGADDR_LOW },
		{ "BHI_IMGSIZE", BHI_IMGSIZE },
		{ "BHI_IMGTXDB", BHI_IMGTXDB },
		{ "PCIE_LOCAL_RSV0", PCIE_LOCAL_RSV0 },
		{ NULL },
	};

	for (i = 0; error_reg[i].name; i++) {
		val = readl(bar0_base + error_reg[i].offset);
		printf("Reg: %s value: 0x%x\n", error_reg[i].name, val);
	}

	if (pbl_log) {
		pci_select_window(bar0_base, QCN9224_TCSR_PBL_LOGGING_REG);
		val = readl(bar0_base + WINDOW_START +
			    (QCN9224_TCSR_PBL_LOGGING_REG & WINDOW_RANGE_MASK));
		printf("Reg: TCSR_PBL_LOGGING: 0x%x\n", val);
	}
}

void qcn92xx_global_soc_reset(uintptr_t bar0_base, bool force_reset)
{
	u32 val = BHI_EE_SBL, count = 0;
	int ret = -EINVAL;
	uintptr_t reg;

	do {
		reg = bar0_base + PCIE_SOC_GLOBAL_RESET_ADDRESS;

		if (force_reset) {
			writel(PCIE_SOC_GLOBAL_RESET_FORCE_RESET_VALUE, reg);
			mdelay(1000);
		} else {
			writel(PCIE_SOC_GLOBAL_RESET_VALUE, reg);
		}

		reg = bar0_base + BHI_EXECENV;
		ret = readl_poll_sleep_timeout(reg, val, val == BHI_EE_PBL,
					       1 * 1000, 20 * 1000);
		if (ret == 0)
			break;

		++count;
	} while (count < MAX_SOC_GLOBAL_RESET_WAIT_CNT);

	if (val != 0)
		printf("SoC global reset failed! Reset count : %d\n", count);
}
#endif

int hex_string_to_binary(const char *hex_str, uint8_t *binary,
			 size_t binary_len)
{
	size_t hex_len;
	size_t i;

	if (!hex_str || !binary) {
		printf("Invalid input: NULL pointer\n");
		return -1;
	}

	hex_len = strlen(hex_str);

	/* Check if hex string length is valid
	 * (must be even and match binary length * 2) */
	if (hex_len != binary_len * 2) {
		printf("Invalid hex string length: %zu (expected %zu)\n",
		       hex_len, binary_len * 2);
		return -1;
	}

	/* Convert hex string to binary */
	for (i = 0; i < binary_len; i++) {
		char byte_str[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
		char *endptr;
		unsigned long byte_val = strtoul(byte_str, &endptr, 16);

		if (*endptr != '\0') {
			printf("Invalid hex character at position %zu\n", i * 2);
			return -1;
		}

		binary[i] = (uint8_t)byte_val;
	}

	return 0;
}

void generate_random_context(uint8_t *context, size_t context_len)
{
	size_t i;
	uint32_t random_val;

	for (i = 0; i < context_len; i += sizeof(uint32_t)) {
		/* Generate a pseudo-random value using timer */
		random_val = get_timer(0) + i;

		/* XOR with a shifting pattern for better randomness */
		random_val ^= (random_val << 13);
		random_val ^= (random_val >> 17);
		random_val ^= (random_val << 5);

		/* Copy bytes to context buffer */
		size_t bytes_to_copy = (context_len - i < sizeof(uint32_t)) ?
				       (context_len - i) : sizeof(uint32_t);

		memcpy(&context[i], &random_val, bytes_to_copy);
	}
}

#ifdef CONFIG_CB_CALIB
static int do_cal_qcn9224(struct cal_config *cfg,
			  struct cal_per_dev_config *dev_cfg, int debug)
{
	struct udevice *dev;
	uintptr_t bar0_base, reg, load_addr = (uintptr_t)NULL;
	struct file_info *file;
	int ret, i, val;
	struct uboot_cal_tlv *tlv = NULL;
	void *cal_fw, *bdf_addr, *regdb_addr, *rxgain_addr = NULL;
	void *fw_ini_addr = NULL;
	u32 cal_fw_size, bdf_size, rxgain_size = 0, regdb_size, fw_ini_size = 0;
	struct cal_fw_header *cal_fw_header;
	void *cal_fw_image;
	int num_macs;

	dev = dev_cfg->dev;

	dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0, (u32 *)&bar0_base);
	bar0_base &= 0xFFF00000;

	cal_fw_header = cfg->cal_fw_header;
	cal_fw_image = (void *)(uintptr_t)cfg->cal_fw_header;

	if (debug) {
		for (i = 0; i < cal_fw_header->num_files; i++) {
			file = &cal_fw_header->file[i];
			printf("type: %x subtype: %x offset: %x size: %x\n",
			       file->type, file->sub_type,
			       file->offset, file->size);
		}
	}

	/* For single mac, board_id will be of the format 0x00XY
	 * For dual mac, board_id will be of the format 0x10XY
	 * So read the most-significant nibble and +1 to get num_macs
	 */
	num_macs = ((dev_cfg->board_id & 0xF000) > 0xC) + 1;

	for (i = 0; i < cal_fw_header->num_files; i++) {
		file = &cal_fw_header->file[i];
		switch (file->type) {
		case CAL_FW:
			if (file->sub_type == num_macs) {
				memcpy(((void *)(uintptr_t)dev_cfg->cal_fw_image_addr),
				       (void *)(uintptr_t)(cal_fw_image + file->offset),
				       file->size);
				cal_fw = (void *)(uintptr_t)dev_cfg->cal_fw_image_addr;
				cal_fw_size = file->size;
				load_addr = (uintptr_t)cal_fw;
			}
			break;
		case BDF:
			if (file->sub_type == dev_cfg->board_id) {
				bdf_addr = cal_fw_image + file->offset;
				bdf_size = file->size;
			}
			break;
		case RXGAIN:
			rxgain_addr = cal_fw_image + file->offset;
			rxgain_size = file->size;
			break;
		case REGDB:
			regdb_addr = cal_fw_image + file->offset;
			regdb_size = file->size;
			break;
		case FW_INI_CFG:
			fw_ini_addr = cal_fw_image + file->offset;
			fw_ini_size = file->size;
			break;
		default:
			printf("Unsupported image type: %d\n", file->type);
			break;
		}
	}
	if(!load_addr){
		printf("Load address is NULL");
		return -EINVAL;
	}

	/* FILL TLV */
	tlv = (struct uboot_cal_tlv *)memalign(SZ_4K, sizeof(struct uboot_cal_tlv));
	if (!tlv) {
		printf("Failed to allocate memory for cal tlv\n");
		return -ENOMEM;
	}

	/*
	 * PCIE_LOCAL_RSV0 is a single 32-bit register — QCN9224 firmware
	 * reads the TLV via this 32-bit address. After U-Boot relocates above
	 * 4 GB the heap is also above 4 GB, so we need a lower-4-GB copy.
	 * This path is never taken in crashdump boot (skip-reloc) because the
	 * heap stays in 32-bit space there.
	 */
	struct uboot_cal_tlv *tlv_dma = tlv;
	phys_addr_t tlv_lmb_addr = 0;

	if ((uintptr_t)tlv > 0xFFFFFFFFUL) {
		tlv_lmb_addr = lmb_alloc_base(
			ALIGN(sizeof(*tlv), SZ_4K), SZ_4K,
			0xFFFFFFFFUL, LMB_NONE);
		if (!tlv_lmb_addr) {
			free(tlv);
			printf("Failed to alloc lower-4-GB buffer for cal tlv\n");
			return -ENOMEM;
		}
		tlv_dma = (struct uboot_cal_tlv *)(uintptr_t)tlv_lmb_addr;
	}

	memset(tlv_dma, 0, sizeof(struct uboot_cal_tlv));
	tlv_dma->magic = 0xCAFECACE;
	tlv_dma->pci_slot = dev_cfg->pci_slot_id;
	tlv_dma->caldb_addr = (u32)dev_cfg->caldb_addr;
	tlv_dma->caldb_size = dev_cfg->caldb_size;
	tlv_dma->host_ddr_status = (u32)dev_cfg->host_ddr_status;
	tlv_dma->hremote_addr = (u32)dev_cfg->hremote_addr;
	tlv_dma->hremote_size = dev_cfg->hremote_size;
	tlv_dma->rddm_addr = (u32)dev_cfg->rddm_addr;
	tlv_dma->rddm_size = dev_cfg->rddm_size;
	tlv_dma->num_images = MAX_IMG_TYPE;

	for (i = 0; i < MAX_IMG_TYPE; i++) {
		switch (i) {
		case BDF:
			tlv_dma->img[i].img_type = i;
			tlv_dma->img[i].img_sram_addr = 0;
			tlv_dma->img[i].img_host_addr = (u32)(uintptr_t)(bdf_addr);
			tlv_dma->img[i].img_size = bdf_size;
			break;
		case CALDATA:
			tlv_dma->img[i].img_type = i;
			tlv_dma->img[i].img_sram_addr = 0;
			memcpy(((void *)(uintptr_t)dev_cfg->host_ddr_status + SZ_4K),
			       (void *)(uintptr_t)(cfg->caldata_addr + dev_cfg->caldata_offset),
			       dev_cfg->caldata_size);
			tlv_dma->img[i].img_host_addr = (u32)(dev_cfg->host_ddr_status + SZ_4K);
			tlv_dma->img[i].img_size = dev_cfg->caldata_size;
			break;
		case RXGAIN:
			tlv_dma->img[i].img_type = i;
			tlv_dma->img[i].img_sram_addr = 0;
			tlv_dma->img[i].img_host_addr = (u32)(uintptr_t)rxgain_addr;
			tlv_dma->img[i].img_size = rxgain_size;
			break;
		case REGDB:
			tlv_dma->img[i].img_type = i;
			tlv_dma->img[i].img_sram_addr = 0;
			tlv_dma->img[i].img_host_addr = (u32)(uintptr_t)regdb_addr;
			tlv_dma->img[i].img_size = regdb_size;
			break;
		case FW_INI_CFG:
			tlv_dma->img[i].img_type = i;
			tlv_dma->img[i].img_sram_addr = 0;
			tlv_dma->img[i].img_host_addr = (u32)(uintptr_t)fw_ini_addr;
			tlv_dma->img[i].img_size = fw_ini_size;
			break;
		default:
			break;
		}
	}

	printf("Starting calibration on slot: %d fw: %s_mac board_id: 0x%x\n",
	       dev_cfg->pci_slot_id, (num_macs == 2) ? "dual" : "single",
	       dev_cfg->board_id);

	if (debug) {
		printf("Dumping TLV:\n");
		printf("magic: 0x%x\n", tlv_dma->magic);
		printf("pci_slot: 0x%x\n", tlv_dma->pci_slot);
		printf("caldb_addr: 0x%x\n", tlv_dma->caldb_addr);
		printf("caldb_size: 0x%x\n", tlv_dma->caldb_size);
		printf("host_ddr_status: 0x%x\n", tlv_dma->host_ddr_status);
		printf("hremote_addr: 0x%x\n", tlv_dma->hremote_addr);
		printf("hremote_size: 0x%x\n", tlv_dma->hremote_size);
		printf("rddm_addr: 0x%x\n", tlv_dma->rddm_addr);
		printf("rddm_size: 0x%x\n", tlv_dma->rddm_size);
		printf("num_images: 0x%x\n", tlv_dma->num_images);

		for (i = 0; i < MAX_IMG_TYPE; i++) {
			printf("img_type:0x%x\n", tlv_dma->img[i].img_type);
			printf("img_sram_addr:0x%x\n", tlv_dma->img[i].img_sram_addr);
			printf("img_host_addr:0x%x\n", tlv_dma->img[i].img_host_addr);
			printf("img_size:0x%x\n", tlv_dma->img[i].img_size);
		}
	}

	writel(0xFF, (uintptr_t)dev_cfg->host_ddr_status);
	flush_dcache_all();

	/* Check if the target is in PBL, else do a force reset */
	ret = readl(bar0_base + BHI_EXECENV);
	if (ret != BHI_EE_PBL) {
		printf("Resetting target to start calibration\n");
		qcn92xx_global_soc_reset(bar0_base, true);
	}

	writel(lower_32_bits((uintptr_t)tlv_dma), bar0_base + PCIE_LOCAL_RSV0);
	writel(0, bar0_base + BHI_STATUS);
	writel(upper_32_bits(load_addr), bar0_base + BHI_IMGADDR_HIGH);
	writel(lower_32_bits(load_addr), bar0_base + BHI_IMGADDR_LOW);
	writel(cal_fw_size, bar0_base + BHI_IMGSIZE);
	writel(1, bar0_base + BHI_IMGTXDB);

	printf("Waiting for calibration bin download...\n");

	reg = bar0_base + BHI_STATUS;
	ret = readl_poll_sleep_timeout(reg, val,
				       ((val & BHI_STATUS_MASK) >> BHI_STATUS_SHIFT) ==
				       BHI_STATUS_SUCCESS, 250 * 1000, 12500 * 1000);
	if (ret) {
		printf("Calibration bin Download failed, BHI_STATUS 0x%x, ret %d\n",
		       val, ret);
		print_error_code(bar0_base, true);
		ret = -EINVAL;
		goto fail;
	}

	reg = bar0_base + BHI_EXECENV;
	ret = readl_poll_sleep_timeout(reg, val, (val & NO_MASK) == BHI_EE_SBL,
				       250 * 1000, 12500 * 1000);
	if (ret) {
		printf("EXECENV is not correct, BHI_EXECENV 0x%x, ret %d\n",
		       val, ret);
		print_error_code(bar0_base, true);
		ret = -EINVAL;
		goto fail;
	}

	printf("Calibration bin loaded successfully\n");

	if (tlv_lmb_addr)
		lmb_free(tlv_lmb_addr, ALIGN(sizeof(*tlv), SZ_4K));
	free(tlv);

	return 0;

fail:
	/* Target SoC global reset */
	qcn92xx_global_soc_reset(bar0_base, false);

	mdelay(1000);

	/* Target MHI reset */
	val = readl(bar0_base + MHICTRL);
	writel(val | MHICTRL_RESET_MASK, bar0_base + MHICTRL);
	if (tlv_lmb_addr)
		lmb_free(tlv_lmb_addr, ALIGN(sizeof(*tlv), SZ_4K));
	free(tlv);

	return ret;
}

static int get_pci_dev_by_slot(int slot_id, struct udevice **devp)
{
	struct udevice *rc, *dev;
	int i;

	for (i = 0; i < CONFIG_IPQ_MAX_PCIE; i++) {
		if (pci_find_device_id(device_table, i, &dev))
			continue;

		rc = pci_get_controller(dev);
		if (!rc)
			continue;

		if (slot_id == dev_read_u32_default(rc, "id", 0)) {
			*devp = dev;
			return 0;
		}
	}

	printf("Failed to find device for slot_id %d\n", slot_id);

	return -ENODEV;
}

static int populate_cfg(struct cal_dt_config *dt_cfg, struct cal_config *cfg)
{
	static bool global_cfg_filled;
	struct cal_per_dev_config *dev_cfg;
	struct udevice *dev = NULL;

	if (dt_cfg->pci_slot_id >= CONFIG_IPQ_MAX_PCIE)
		return -EINVAL;

	if (!dt_cfg->rmem_base_addr)
		return -ENOMEM;

	if (get_pci_dev_by_slot(dt_cfg->pci_slot_id, &dev))
		return -ENODEV;

	if ((!dt_cfg->caldata_offset) || (!dt_cfg->caldb_offset))
		return -EINVAL;

	/* Check for integer overflow and validate caldb region bounds */
	if (dt_cfg->caldb_offset > dt_cfg->rmem_size ||
	    dt_cfg->caldb_size > dt_cfg->rmem_size ||
	    dt_cfg->caldb_offset > (dt_cfg->rmem_size - dt_cfg->caldb_size)) {
		printf("caldb region exceeds rmem for PCI slot %d!\n",
		       dt_cfg->pci_slot_id);
		return -ENOMEM;
	}

	/* Memory Layout for miniFW for calibration
	 *
	 * +==========+===============+=========+
	 * |  Region  | Start Offset  |   Size  |
	 * +----------+---------------+---------+
	 * | miniFW   |  0x00000000   |   4MB   |
	 * +----------+---------------+---------+
	 * | Hremote  |  0x00400000   |   4MB   |
	 * +----------+---------------+---------+
	 * | Host DDR |  0x00800000   |   1MB   |
	 * | Status   |               |         |
	 * |   +      |               |         |
	 * | Per Dev  |               |         |
	 * | CalData  |               |         |
	 * +----------+---------------+---------+
	 * | Cal Data |  0x00900000   |   1MB   |
	 * | ART Part |               |         |
	 * +----------+---------------+---------+
	 * | cal_fw   |  0x00A00000   |  10MB   |
	 * |from flash|               |         |
	 * +----------+---------------+---------+
	 * |  Caldb   |  0x01400000   |   8MB   |
	 * |          |(LM512 profile)|         |
	 * |          |  0x01C00000   |   8MB   |
	 * |          | (1GB profile) |         |
	 * +----------+---------------+---------+
	 * |  RDDM    |               |Last 6MB |
	 * +====================================+
	 *
	 */
	if (!global_cfg_filled) {
		cfg->ddr_base_addr = dt_cfg->rmem_base_addr;
		cfg->ddr_rmem_size = dt_cfg->rmem_size;
		cfg->caldata_addr = cfg->ddr_base_addr + (9 * SZ_1M);
		cfg->cal_fw_header =
			(struct cal_fw_header *)(uintptr_t)(cfg->ddr_base_addr + (10 * SZ_1M));
		/*
		 * This slot's own carveout hosts the shared cal_fw archive.
		 * Its caldb write can overlap the archive's tail, so it must
		 * be calibrated last, after every other slot has had a
		 * chance to read its BDF/REGDB/FW_INI_CFG data out of it.
		 */
		cfg->archive_owner_slot_id = dt_cfg->pci_slot_id;
		global_cfg_filled = true;
	}

	dev_cfg = &cfg->dev_cfg[dt_cfg->pci_slot_id];

	dev_cfg->dev = dev;
	dev_cfg->pci_slot_id = dt_cfg->pci_slot_id;
	dev_cfg->board_id = dt_cfg->board_id;
	dev_cfg->caldata_offset = dt_cfg->caldata_offset;
	dev_cfg->caldata_size = MAX_CALDATA_SIZE;

	dev_cfg->cal_fw_image_addr = dt_cfg->rmem_base_addr;
	dev_cfg->hremote_addr = dt_cfg->rmem_base_addr + SZ_4M;
	dev_cfg->hremote_size = (4 * SZ_1M);

	dev_cfg->host_ddr_status = dt_cfg->rmem_base_addr + SZ_8M;

	dev_cfg->caldb_addr = dt_cfg->rmem_base_addr + dt_cfg->caldb_offset;
	dev_cfg->caldb_size = SZ_8M;

	dev_cfg->rddm_addr = dt_cfg->rmem_base_addr + dt_cfg->rmem_size - (6 * SZ_1M);
	dev_cfg->rddm_size = (6 * SZ_1M);

	return 0;
}

int cal_qcn9224(int debug)
{
	int ret, i;
	struct cal_config *cfg;
	struct cal_dt_config *dt_cfg;
	unsigned long len = SZ_2M;
	bool cal_part_present = false;
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
#ifdef CONFIG_IPQ_NAND
	struct ubi_device *ubi;
#endif
#ifdef CONFIG_IPQ_MMC
	struct blkpart_info bpart_info;
	struct disk_partition disk_info;
#endif

	if (!sfi)
		return -EINVAL;

	/* Check if cal_fw partition exists, if not bail out here */
	switch (gd->board_type & FLASH_TYPE_MASK) {
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_NORPLUSNAND:
	case SMEM_BOOT_QSPI_NAND_FLASH:
		ubi = ubi_get_device(0);
		if (!ubi)
			break;

		for (i = 0; i < (ubi->vtbl_slots + 1); i++) {
			if (!ubi->volumes[i])
				continue;	/* Empty record */

			if (ubi->volumes[i]->name_len <= UBI_VOL_NAME_MAX &&
			    strnlen(ubi->volumes[i]->name,
				    ubi->volumes[i]->name_len + 1) ==
						ubi->volumes[i]->name_len) {
				if (!strncmp("cal_fw", ubi->volumes[i]->name,
					     UBI_VOL_NAME_MAX)) {
					cal_part_present = true;
					break;
				}
			}
		}

		if (ubi)
			ubi_put_device(ubi);

		break;
#endif
	default:
#ifdef CONFIG_IPQ_MMC
		BLK_PART_GET_INFO_S(bpart_info, "0:CALFW", &disk_info,
				    sfi->flash_type, false);
		ret = ipq_part_get_info_by_name(&bpart_info);
		if (!ret)
			cal_part_present = true;
#endif
	}

	if (!cal_part_present) {
		printf("cal_fw partition not present\n");
		return -ENOTSUPP;
	}

	cfg = malloc_cache_aligned(sizeof(*cfg));
	if (!cfg) {
		printf("failed to allocate memory for cal config\n");
		return -ENOMEM;
	}

	memset(cfg, 0, sizeof(*cfg));
	cfg->archive_owner_slot_id = CONFIG_IPQ_MAX_PCIE;

	list_for_each_entry(dt_cfg, cal_list_head, list) {
		ret = populate_cfg(dt_cfg, cfg);
		if (ret) {
			printf("Failed to populate cal config %d\n", ret);
			goto out;
		}
	}

	if (!cfg->ddr_base_addr || !cfg->caldata_addr) {
		printf("Failed to get DDR base addr for cal\n");
		ret = -ENOTSUPP;
		goto out;
	}

	if (debug) {
		printf("Dumping the cal config\n");
		printf("cal_fw_header: 0x%lx\n", (uintptr_t)cfg->cal_fw_header);
		printf("ddr_base_addr: 0x%llx\n",
		       (unsigned long long)cfg->ddr_base_addr);
		printf("ddr_rmem_size: 0x%x\n", cfg->ddr_rmem_size);
		printf("caldata_addr: 0x%llx\n",
		       (unsigned long long)cfg->caldata_addr);
		for (i = 0; i < CONFIG_IPQ_MAX_PCIE; i++) {
			if (!cfg->dev_cfg[i].dev)
				continue;

			printf("dev_name[%d]: %s\n", i,
			       cfg->dev_cfg[i].dev->name);
			printf("pci_slot_id[%d]: 0x%x\n", i,
			       cfg->dev_cfg[i].pci_slot_id);
			printf("board_id[%d]: 0x%x\n", i,
			       cfg->dev_cfg[i].board_id);
			printf("caldata_offset[%d]: 0x%x\n", i,
			       cfg->dev_cfg[i].caldata_offset);
			printf("caldata_size[%d]: 0x%x\n", i,
			       cfg->dev_cfg[i].caldata_size);
			printf("cal_fw_image_addr[%d]: 0x%llx\n", i,
			       (unsigned long long)cfg->dev_cfg[i].cal_fw_image_addr);
			printf("hremote_addr[%d]: 0x%llx\n", i,
			       (unsigned long long)cfg->dev_cfg[i].hremote_addr);
			printf("caldb_addr[%d]: 0x%llx\n", i,
			       (unsigned long long)cfg->dev_cfg[i].caldb_addr);
			printf("caldb_size[%d]: 0x%x\n", i,
			       cfg->dev_cfg[i].caldb_size);
			printf("host_ddr_status[%d]: 0x%llx\n", i,
			       (unsigned long long)cfg->dev_cfg[i].host_ddr_status);
			printf("rddm_addr[%d]: 0x%llx\n", i,
			       (unsigned long long)cfg->dev_cfg[i].rddm_addr);
		}
	}

	ret = ipq_get_partition_data("0:ART", 0,
				     (uint8_t *)(uintptr_t)cfg->caldata_addr,
				     SZ_1M, sfi->flash_type);
	if (ret < 0) {
		printf("Failed to read caldata from ART: %d\n", ret);
		goto out;
	}

	switch (gd->board_type & FLASH_TYPE_MASK) {
#ifdef CONFIG_IPQ_NAND
	case SMEM_BOOT_NORPLUSNAND:
	case SMEM_BOOT_QSPI_NAND_FLASH:
		ret = ubi_volume_read("cal_fw",
				      (uint8_t *)(uintptr_t)cfg->ddr_base_addr,
				      0, (size_t)SZ_2M);
		if (ret) {
			printf("Failed to read data from cal_fw volume %d\n",
			       ret);
			ret = -ret;
		}
		break;
#endif
	default:
#ifdef CONFIG_IPQ_MMC
		ret = ipq_get_partition_data("0:CALFW", 0,
					     (uint8_t *)(uintptr_t)cfg->ddr_base_addr,
					     SZ_2M, SMEM_BOOT_MMC_FLASH);
#endif
	}
	if (ret < 0) {
		printf("Failed to read from CAL FW %d\n", ret);
		goto out;
	}

	ret = gunzip((void *)cfg->cal_fw_header, (SZ_8M + SZ_2M),
		     (uint8_t *)(uintptr_t)cfg->ddr_base_addr, &len);
	if (ret < 0) {
		printf("Failed to uncompress CAL FW %d\n", ret);
		goto out;
	}

	if (!cfg->cal_fw_header) {
		ret = -EINVAL;
		goto out;
	}
	if (cfg->cal_fw_header->magic != 0xCAFECACE) {
		printf("Magic mismatch\n");
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < CONFIG_IPQ_MAX_PCIE; i++) {
		/* This slot doesn't have any EP attached */
		if (!cfg->dev_cfg[i].dev)
			continue;

		/*
		 * Defer the archive-owning slot to last so the other slots
		 * get a head start reading their cal data out of the shared
		 * cal_fw archive before this slot's caldb write can corrupt
		 * the archive's tail.
		 */
		if (i == cfg->archive_owner_slot_id)
			continue;

		ret = do_cal_qcn9224(cfg, &cfg->dev_cfg[i], debug);
		if (ret) {
			printf("failed to start cal on qcn9224[%d] %d\n",
			       cfg->dev_cfg[i].pci_slot_id, ret);
			goto out;
		}
	}

	if (cfg->archive_owner_slot_id < CONFIG_IPQ_MAX_PCIE &&
	    cfg->dev_cfg[cfg->archive_owner_slot_id].dev) {
		struct cal_per_dev_config *owner_dev_cfg =
			&cfg->dev_cfg[cfg->archive_owner_slot_id];

		ret = do_cal_qcn9224(cfg, owner_dev_cfg, debug);
		if (ret) {
			printf("failed to start cal on qcn9224[%d] %d\n",
			       owner_dev_cfg->pci_slot_id, ret);
			goto out;
		}
	}

out:
	free(cfg);

	return ret;
}
#endif
