// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <malloc.h>
#include <part.h>
#include <blk.h>
#include <mmc.h>
#include <spi_flash.h>
#include <asm/cache.h>
#include <smem.h>
#include <spl.h>
#include <mach/ipq_license.h>
#include <mach/ipq.h>
#include <nand.h>
#include <linux/mtd/mtd.h>
#include "qti_softsku_utils/include/LicenseStore.h"
#include "qti_softsku_utils/include/qwes_store.h"

DECLARE_GLOBAL_DATA_PTR;


/* License storage context */
struct license_storage_ctx {
	struct blk_desc *dev_desc;
	struct disk_partition part_info;
	u32 slot_size;
	u32 partition_size_in_slots;
	u32 bytes_per_sector;
	u32 total_sectors;
	/* NAND-specific fields */
	bool is_nand;
	struct mtd_info *nand_mtd;
	u32 nand_offset;
	u32 nand_size;
};

/*
 * features[] - target-specific SoftSKU HW feature array.
 * Built from IPQ_HW_FEATURES defined in the target's mach header
 * (e.g. arch/arm/mach-ipq/include/mach/ipq9650.h).
 * If the target does not define IPQ_HW_FEATURES the array is empty.
 */
#ifdef IPQ_HW_FEATURES
static struct sec_enforceHWFeatureId features[] = {
	IPQ_HW_FEATURES
};
#endif

/**
 * qwes_store_open_partition() - Open license partition (QWESStore interface)
 * @partition_name: Name of the partition
 * @client_handle: Pointer to store client handle
 *
 * Return: QWES_STORE_SUCCESS on success, error code on failure
 */
static int qwes_store_open_partition(u8 *partition_name, void **client_handle)
{
	struct license_storage_ctx *ctx;
	int ret;
	const char *part_name = (const char *)partition_name;
	u32 boot_dev;

	if (!partition_name || !client_handle) {
		pr_err("Invalid parameters for storage open\n");
		return QWES_STORE_INVALID_PARAM;
	}

	/* Allocate storage context */
	ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		pr_err("Failed to allocate storage context\n");
		return QWES_STORE_OUT_OF_RESOURCES;
	}

	memset(ctx, 0, sizeof(*ctx));

	/* Get boot device */
	boot_dev = spl_boot_device();

	/* Handle NAND boot device */
#ifdef CONFIG_IPQ_NAND
	if (boot_dev == BOOT_DEVICE_NAND) {
		u32 start_blk, blk_cnt;

		ctx->is_nand = true;
		ctx->nand_mtd = get_nand_dev_by_index(0);

		if (!ctx->nand_mtd) {
			pr_err("Failed to get NAND device\n");
			free(ctx);
			return QWES_STORE_NOT_FOUND;
		}

		/* Get partition info from MIBIB partition table */
		ret = ipq_spl_mibib_getpart(part_name, &start_blk, &blk_cnt);
		if (ret < 0) {
			pr_err("Failed to find NAND partition '%s' in MIBIB\n", part_name);
			free(ctx);
			return QWES_STORE_PARTI_NOT_FOUND;
		}

		/* Calculate NAND offset and size */
		ctx->nand_offset = start_blk * ctx->nand_mtd->erasesize;
		ctx->nand_size = blk_cnt * ctx->nand_mtd->erasesize;

		/* Set storage parameters for NAND - slot is the fixed unit (SLOT_SIZE bytes) */
		ctx->slot_size = SLOT_SIZE;
		ctx->bytes_per_sector = SLOT_SIZE;  /* sector = slot = fixed 4KB */
		ctx->total_sectors = ctx->nand_size / SLOT_SIZE;
		ctx->partition_size_in_slots = ctx->nand_size / SLOT_SIZE;

		*client_handle = ctx;

		return QWES_STORE_SUCCESS;
	}
#endif

	/* Handle block device (MMC/SPI) */
	switch (boot_dev) {
#ifdef CONFIG_IPQ_MMC
	case BOOT_DEVICE_MMC1:
		ctx->dev_desc = blk_get_devnum_by_uclass_id(UCLASS_MMC, 0);
		break;
#endif
#ifdef CONFIG_IPQ_SPI_NOR
	case BOOT_DEVICE_SPI:
		ctx->dev_desc = blk_get_devnum_by_uclass_id(UCLASS_SPI, 0);
		break;
#endif
	default:
		pr_err("Unsupported boot device for license storage\n");
		free(ctx);
		return QWES_STORE_NOT_FOUND;
	}

	if (!ctx->dev_desc) {
		pr_err("Failed to get block device\n");
		free(ctx);
		return QWES_STORE_NOT_FOUND;
	}

	/* Find partition by name */
	ret = part_get_info_by_name(ctx->dev_desc, part_name, &ctx->part_info);
	if (ret < 0) {
		pr_err("Failed to find partition '%s'\n", part_name);
		free(ctx);
		return QWES_STORE_PARTI_NOT_FOUND;
	}

	/* Set storage parameters - slot is the fixed unit (SLOT_SIZE bytes) */
	ctx->slot_size = SLOT_SIZE;
	ctx->bytes_per_sector = SLOT_SIZE;  /* sector = slot = fixed 4KB */
	ctx->total_sectors = (ctx->part_info.size * ctx->part_info.blksz) / SLOT_SIZE;
	ctx->partition_size_in_slots = (ctx->part_info.size * ctx->part_info.blksz) / SLOT_SIZE;

	*client_handle = ctx;

	return QWES_STORE_SUCCESS;
}

/**
 * qwes_store_client_get_info() - Get client info (QWESStore interface)
 * @client_handle: Client handle
 * @client_info: Pointer to client info structure
 *
 * Return: QWES_STORE_SUCCESS on success, error code on failure
 */
static int qwes_store_client_get_info(void *client_handle, void *client_info)
{
	struct license_storage_ctx *ctx = (struct license_storage_ctx *)client_handle;
	qwes_store_client_info_t *info = (qwes_store_client_info_t *)client_info;

	if (!ctx || !info) {
		pr_err("Invalid parameters for client get info\n");
		return QWES_STORE_INVALID_PARAM;
	}

	/* slot_size = SLOT_SIZE (fixed 4KB)
	 * partition_size_in_slots = partition_size / SLOT_SIZE
	 */
	info->slot_size = ctx->slot_size;
	info->partition_size_in_slots = ctx->partition_size_in_slots;

	return QWES_STORE_SUCCESS;
}

/**
 * qwes_store_read() - Read slots (QWESStore interface)
 * @client_handle: Client handle
 * @start_slot: Starting slot index
 * @num_slots: Number of slots to read
 * @data_buffer: Buffer to store read data
 *
 * Return: QWES_STORE_SUCCESS on success, error code on failure
 */
static int qwes_store_read(void *client_handle, u32 start_slot,
			   u32 num_slots, u8 *data_buffer)
{
	struct license_storage_ctx *ctx = (struct license_storage_ctx *)client_handle;
	lbaint_t lba;
	ulong n;
	int ret;

	if (!ctx || !data_buffer) {
		pr_err("Invalid parameters for read\n");
		return QWES_STORE_INVALID_PARAM;
	}

	/* Check bounds */
	if (start_slot + num_slots > ctx->partition_size_in_slots) {
		pr_err("Read beyond partition bounds\n");
		return QWES_STORE_INVALID_PARAM;
	}

#ifdef CONFIG_IPQ_NAND
	/* Handle NAND read */
	if (ctx->is_nand) {
		loff_t offset;
		size_t length;

		if (!ctx->nand_mtd) {
			pr_err("NAND MTD device not initialized\n");
			return QWES_STORE_ERROR;
		}

		/* Calculate byte offset and length from slot index */
		offset = ctx->nand_offset + ((loff_t)start_slot * SLOT_SIZE);
		length = (size_t)num_slots * SLOT_SIZE;

		/* Check bounds */
		if (offset + length > ctx->nand_offset + ctx->nand_size) {
			pr_err("NAND read beyond partition bounds\n");
			return QWES_STORE_INVALID_PARAM;
		}

		/* Read from NAND */
		ret = nand_read(ctx->nand_mtd, offset, &length, data_buffer);
		if (ret) {
			pr_err("NAND read failed: %d\n", ret);
			return QWES_STORE_ERROR;
		}

		/* Invalidate cache */
		invalidate_dcache_range((unsigned long)data_buffer,
					(unsigned long)data_buffer + length);

		return QWES_STORE_SUCCESS;
	}
#endif

	/* Handle block device read (MMC/SPI) */
	/* Convert slot index to LBA: each slot is SLOT_SIZE bytes */
	{
		lbaint_t slot_offset_bytes = (lbaint_t)start_slot * SLOT_SIZE;
		lbaint_t num_bytes = (lbaint_t)num_slots * SLOT_SIZE;
		ulong num_blks = num_bytes / ctx->part_info.blksz;

		lba = ctx->part_info.start + (slot_offset_bytes / ctx->part_info.blksz);

		/* Read blocks */
		n = blk_dread(ctx->dev_desc, lba, num_blks, data_buffer);
		if (n != num_blks) {
			pr_err("Failed to read (expected=%lu, actual=%lu)\n",
			       num_blks, n);
			return QWES_STORE_ERROR;
		}

		/* Invalidate cache */
		invalidate_dcache_range((unsigned long)data_buffer,
					(unsigned long)data_buffer + (ulong)num_bytes);
	}

	return QWES_STORE_SUCCESS;
}

/**
 * qwes_store_write() - Write slots (QWESStore interface)
 * @client_handle: Client handle
 * @start_slot: Starting slot
 * @num_slots: Number of slots to write
 * @data_buffer: Buffer containing data to write
 *
 * Return: QWES_STORE_SUCCESS on success, error code on failure
 */
static int qwes_store_write(void *client_handle, u32 start_slot,
			    u32 num_slots, u8 *data_buffer)
{
	/* No action needed in SPL*/
	return QWES_STORE_SUCCESS;
}

/**
 * qwes_store_close() - Close storage handle (QWESStore interface)
 * @client_handle: Client handle
 *
 * Return: QWES_STORE_SUCCESS on success, error code on failure
 */
static int qwes_store_close(void *client_handle)
{
	struct license_storage_ctx *ctx = (struct license_storage_ctx *)client_handle;

	if (!ctx) {
		return QWES_STORE_INVALID_PARAM;
	}

	free(ctx);
	return QWES_STORE_SUCCESS;
}

/**
 * ipq_license_is_valid() - Check if license blob is valid
 * @license: Pointer to license blob
 * @len: Length of license blob
 *
 * Return: true if valid, false otherwise
 */
static bool ipq_license_is_valid(const u8 *license, size_t len)
{
	u32 i;

	if (!license || len < 16) {
		return false;
	}

	/* Check if license is not all zeros or all 0xFF */
	for (i = 0; i < 16; i++) {
		if (license[i] != 0x00 && license[i] != 0xFF) {
			return true;
		}
	}

	return false;
}

/**
 * ipq_license_install_all() - Install all licenses from storage using LicenseStore
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq_license_install_all(void)
{
	LicenseStore license_store = {0};
	QWESStore qwes_store;
	int32_t ret;

	/* Initialize QWESStore backend with function pointers */
	memset(&qwes_store, 0, sizeof(qwes_store));
	qwes_store.open_partition = qwes_store_open_partition;
	qwes_store.client_get_info = qwes_store_client_get_info;
	qwes_store.read = qwes_store_read;
	qwes_store.write = qwes_store_write;
	qwes_store.close = qwes_store_close;

	/* Initialize LicenseStore with QWESStore backend */
	ret = LicenseStore_init(&license_store, qwes_store);
	if (ret != LICENSE_STORE_SUCCESS) {
		pr_err("Failed to initialize LicenseStore: %d\n", ret);
		return -EIO;
	}

	/* Install all licenses from storage */
	ret = LicenseStore_installAllLicenses(&license_store);
	if (ret != LICENSE_STORE_SUCCESS) {
		pr_err("Failed to install licenses: %d\n", ret);
		LicenseStore_deinit(&license_store);
		return -EIO;
	}

	/* Deinitialize LicenseStore */
	ret = LicenseStore_deinit(&license_store);
	if (ret != LICENSE_STORE_SUCCESS) {
		pr_err("Failed to deinitialize LicenseStore: %d\n", ret);
		return -EIO;
	}

	return 0;
}

/**
 * boot_license_install() - Install a single license blob
 * @ptr: Pointer to license blob
 * @len: Length of license blob
 * @index: License index (for identification)
 *
 * This function installs a single license blob via TME IPC.
 * It's called from the QTI SoftSKU utils layer.
 *
 * Return: 0 on success, negative error code on failure
 */
int boot_license_install(void *ptr, size_t len, uint16_t index)
{
#ifdef CONFIG_IPQ_SOFTSKU_SUPPORT
	int ret;
	u64 flags = 0;
	u8 identifier[64] = {0}; /* Buffer for license identifier */
	size_t identifier_len_out = 0;

	if (!ptr || len == 0) {
		pr_err("Invalid license parameters: ptr=%p, len=%zu\n", ptr, len);
		return -EINVAL;
	}

	/* Validate license blob */
	if (!ipq_license_is_valid((const u8 *)ptr, len)) {
		pr_err("Invalid license blob at index %u\n", index);
		return -EINVAL;
	}

	/* Install license via TME IPC */
	ret = ipq_license_install_tme(ptr, len, &flags, identifier,
				      sizeof(identifier), &identifier_len_out);
	if (ret) {
		pr_err("Failed to install license %u via TME: %d\n", index, ret);
		return ret;
	}

	printf("License %u installed successfully (flags=0x%llx, id_len=%zu)\n",
	       index, flags, identifier_len_out);

	return 0;
#else
	pr_err("License installation not supported (CONFIG_IPQ_SOFTSKU_SUPPORT not enabled)\n");
	return -ENOTSUP;
#endif /* CONFIG_IPQ_SOFTSKU_SUPPORT */
}

/**
 * ipq_spl_save_fids_smem() - Save FID information to SMEM
 * @smem: Pointer to the SMEM device (already initialized)
 *
 * This function populates FID information into SMEM using the SMEM_FID_LIST_INFO
 * item. It uses the platform-defined features[] array to populate the feature IDs
 * and their enforcement status.
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_spl_save_fids_smem(struct udevice *smem)
{
#if defined(CONFIG_IPQ_SOFTSKU_SUPPORT) && defined(IPQ_HW_FEATURES)
	int ret;
	struct sec_enforceHWFeatureId *fid_list;
	size_t smem_size;

	if (!smem) {
		pr_err("Invalid SMEM device handle\n");
		return -EINVAL;
	}

	/* Use sizeof(features) to get the exact size of the array */
	smem_size = sizeof(features);

	if (smem_size == 0) {
		pr_warn("No hardware features defined for this platform\n");
		return 0;
	}

	printf("Populating FID information to SMEM (%zu features, %zu bytes)\n",
	       ARRAY_SIZE(features), smem_size);

	/* Allocate SMEM using sizeof(features) */
	ret = smem_alloc(smem, -1, SMEM_FID_LIST_INFO, smem_size);
	if (ret && ret != -EEXIST) {
		pr_err("Failed to alloc SMEM_FID_LIST_INFO (ret=%d)\n", ret);
		return ret;
	}

	/* Get pointer to SMEM FID list */
	fid_list = (struct sec_enforceHWFeatureId *)smem_get(smem, -1, SMEM_FID_LIST_INFO, &smem_size);
	if (!fid_list) {
		pr_err("Failed to get SMEM_FID_LIST_INFO\n");
		return -ENOENT;
	}

	/* Copy features[] array directly to SMEM */
	memcpy(fid_list, features, sizeof(features));

	/* Flush cache to ensure data is written to SMEM */
	flush_dcache_range((unsigned long)fid_list,
			   (unsigned long)fid_list + sizeof(features));

	return 0;
#else
	pr_debug("SoftSKU support / HW features not enabled, skipping FID SMEM population\n");
	return 0;
#endif /* CONFIG_IPQ_SOFTSKU_SUPPORT && IPQ_HW_FEATURES */
}

/**
 * ipq_spl_license_init() - Main license initialization function
 * @ctx: SPL context (unused for now)
 *
 * This function is called from ipq_spl_qclib_fixup() after qclib_entry()
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_spl_license_init(void *ctx)
{
	int ret;

	printf("=== License Softsku Initialization ===\n");

	/* Step 1: Install all licenses from storage */
	ret = ipq_license_install_all();
	if (ret) {
		pr_err("License installation failed: %d\n", ret);
		/* Continue even if installation fails */
	}

#ifdef IPQ_HW_FEATURES
	size_t len_out;
	u32 hw_reg_version;

	/* Step 2: Enforce hardware features */
	ret = ipq_license_enforce_hw_features_tme(features,
						  sizeof(features),
						  &len_out, &hw_reg_version);
	if (ret) {
		pr_err("HW feature enforcement failed: %d\n", ret);
		/* Continue even if enforcement fails */
	}
#endif

	printf("=== License Softsku Initialization Complete ===\n");
	return 0;
}
