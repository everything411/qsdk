// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm Crashdump Filesystem Hardware Encryption Support Driver
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 *
 * This file provides crashdump-specific encryption support that automatically
 * handles hardware encryption enable/disable based on whether we're writing
 * file data or filesystem metadata.
 *
 *
 */

#include <fs.h>
#include <fs_crashdump_encrypted.h>
#include <mapmem.h>
#include <log.h>
#include <linux/errno.h>

extern void storage_crypto_config(u64 dun, bool enable);

static struct {
	bool initialized;
	bool encryption_enabled;
	bool is_file_data;
	u64 current_dun;
	u64 file_start_dun;
} g_crypto_ctx = {
	.initialized = false,
	.encryption_enabled = false,
	.is_file_data = false,
	.current_dun = 0,
	.file_start_dun = 0,
};

/**
 * fs_crashdump_encrypted_init - Initialize crashdump encryption system
 *
 * Call this once during crashdump initialization to set up the encryption system.
 *
 * Return: 0 on success, negative on error
 */
int fs_crashdump_encrypted_init(void)
{
	g_crypto_ctx.initialized = true;
	g_crypto_ctx.encryption_enabled = false;
	g_crypto_ctx.is_file_data = false;
	g_crypto_ctx.current_dun = 0;
	g_crypto_ctx.file_start_dun = 0;

	debug("Crashdump encryption initialized\n");
	return 0;
}

/**
 * fs_crashdump_encrypted_set_file_data - Internal function to control encryption
 * @is_file_data: true = file data (enable encryption), false = metadata (disable encryption)
 *
 * This function is called internally to control encryption based on what's being written.
 * It automatically calls storage_crypto_config() to enable/disable ICE hardware.
 */
static void fs_crashdump_encrypted_set_file_data(bool is_file_data)
{
	/* Only act if encryption is enabled for this file */
	if (!g_crypto_ctx.encryption_enabled || !g_crypto_ctx.initialized) {
		debug("ICE: Encryption not enabled\n");
		return;
	}

	/* Transitioning from metadata to file data - ENABLE encryption */
	if (is_file_data && !g_crypto_ctx.is_file_data) {
		storage_crypto_config(g_crypto_ctx.current_dun, true);
		debug("ICE: Enabled encryption for file data write (DUN=0x%llx)\n",
		      g_crypto_ctx.current_dun);
	}

	/* Transitioning from file data to metadata - DISABLE encryption */
	if (!is_file_data && g_crypto_ctx.is_file_data) {
		storage_crypto_config(g_crypto_ctx.current_dun, false);
		debug("ICE: Disabled encryption for metadata write\n");
	}

	g_crypto_ctx.is_file_data = is_file_data;
}

/**
 * fs_crashdump_write_encrypted - Write file with automatic hardware encryption control
 * @filename: Name of file to write
 * @addr: Address of data to write
 * @offset: Offset in file
 * @len: Number of bytes to write
 * @actwrite: Actual bytes written (output)
 * @encrypt: Enable hardware encryption for this file
 *
 * This is the main API that replaces fs_write() for crashdump. It automatically:
 * 1. Disables encryption before writing metadata (superblock, inodes, bitmaps)
 * 2. Enables encryption before writing file data (with DUN tracking)
 * 3. Disables encryption after write completes
 *
 * The ext4_write.c hooks call fs_crashdump_encrypted_notify_file_data() to
 * notify when file data is being written vs metadata.
 *
 * Return: 0 on success, negative on error
 */
int fs_crashdump_write_encrypted(const char *filename, ulong addr, loff_t offset,
				 loff_t len, loff_t *actwrite, bool encrypt)
{
	int ret;

	if (!actwrite) {
		log_err("actwrite parameter cannot be NULL\n");
		return -EINVAL;
	}

	if (!filename) {
		log_err("filename parameter cannot be NULL\n");
		return -EINVAL;
	}
	if (encrypt && !g_crypto_ctx.initialized) {
		log_err("Crashdump encryption not initialized. Call fs_crashdump_encrypted_init() first.\n");
		return -EINVAL;
	}

	/* Set encryption state for this file */
	g_crypto_ctx.encryption_enabled = encrypt;

	if (encrypt) {
		/* Calculate starting DUN based on offset (512-byte sectors) */
		g_crypto_ctx.file_start_dun = (u64)offset / 512;
		g_crypto_ctx.current_dun = g_crypto_ctx.file_start_dun;

		/* Start with metadata mode (encryption disabled) */
		fs_crashdump_encrypted_set_file_data(false);
		debug("Starting encrypted write of %s (DUN start=0x%llx)\n",
		      filename, g_crypto_ctx.current_dun);
	}

	/*
	 * Call the standard fs_write() function.
	 * For EXT4, this will call ext4_write_file() which calls ext4fs_write().
	 *
	 * During the write:
	 * - Metadata writes (superblock, inodes, bitmaps) happen with encryption disabled
	 * - File data writes happen with encryption enabled (via our hooks in ext4fs_write_file)
	 * - The hooks call fs_crashdump_encrypted_notify_file_data() to toggle encryption
	 */
	ret = fs_write(filename, addr, offset, len, actwrite);

	/* Ensure encryption is disabled after write */
	if (encrypt) {
		fs_crashdump_encrypted_set_file_data(false);

		if(ret != 0 && !actwrite && *actwrite < 0) {
			log_err("Encrypted write failed for %s, ICE disabled\n", filename);
		}
		/* After write, ensure ICE is explicitly disabled and dun is
		 * reset to 0
		 * */
		storage_crypto_config(0, false);
		fs_crashdump_encrypted_reset_dun(0);
	}

	/* Always reset encryption state after write completes */
	g_crypto_ctx.encryption_enabled = false;
	g_crypto_ctx.is_file_data = false;

	return ret;
}

/**
 * fs_crashdump_encrypted_reset_dun - Reset DUN to specified value
 * @dun: New DUN value
 *
 * Use this to reset the DUN counter, for example when starting a new
 * crashdump session or writing to a different partition.
 */
void fs_crashdump_encrypted_reset_dun(u64 dun)
{
	g_crypto_ctx.current_dun = dun;
	g_crypto_ctx.file_start_dun = dun;
	debug("DUN reset to 0x%llx\n", dun);
}

/**
 * fs_crashdump_encrypted_get_dun - Get current DUN value
 *
 * Return: Current DUN value
 */
u64 fs_crashdump_encrypted_get_dun(void)
{
	return g_crypto_ctx.current_dun;
}

/**
 * fs_crashdump_encrypted_get_state - Check if encryption is initialized
 *
 * Return: true if initialized, false otherwise
 */
bool fs_crashdump_encrypted_get_state(void)
{
	return g_crypto_ctx.initialized;
}

/**
 * fs_crashdump_encrypted_deinit - Cleanup crashdump encryption
 *
 * Call this to reset the encryption state and ensure ICE is disabled.
 */
void fs_crashdump_encrypted_deinit(void)
{
	/* Ensure encryption is disabled */
	if (g_crypto_ctx.initialized && g_crypto_ctx.is_file_data)
		storage_crypto_config(0, false);

	g_crypto_ctx.initialized = false;
	g_crypto_ctx.encryption_enabled = false;
	g_crypto_ctx.is_file_data = false;
	g_crypto_ctx.current_dun = 0;
	g_crypto_ctx.file_start_dun = 0;

	debug("Crashdump encryption deinitialized\n");
}

/**
 * fs_crashdump_encrypted_is_file_data - Check if currently writing file data
 *
 * This is used internally to determine whether encryption should be enabled.
 *
 * Return: true if writing file data (encryption enabled), false if metadata (encryption disabled)
 */
bool fs_crashdump_encrypted_is_file_data(void)
{
	return g_crypto_ctx.encryption_enabled && g_crypto_ctx.is_file_data;
}

/**
 * fs_crashdump_encrypted_notify_file_data - Notify that file data write is starting/ending
 * @is_file_data: true when starting file data write, false when ending
 * @sector_count: byte count to be converted to DUN (divided by 512)
 *
 * This function is called by the filesystem layer (ext4fs_write_file()) hooks
 * just before/after writing actual file data blocks.
 *
 * When is_file_data=true:
 * - Enables ICE encryption with DUN calculated from sector_count / 512
 * - File data will be encrypted by hardware
 * - sector_count represents the extent size in bytes
 *
 * When is_file_data=false:
 * - Disables ICE encryption
 * - Metadata will be written unencrypted
 * - sector_count parameter is ignored
 *
 * This is the key function that provides automatic metadata/data separation.
 */
void fs_crashdump_encrypted_notify_file_data(bool is_file_data, u64 sector_off)
{
	/* Update DUN based on sector_count when enabling encryption */
	if (!is_file_data && g_crypto_ctx.encryption_enabled) {
		g_crypto_ctx.current_dun += sector_off / 512;
		debug("Updated DUN from sector_count: 0x%llx (sector_count=%llu)\n",
		      g_crypto_ctx.current_dun, sector_off);
	}

	fs_crashdump_encrypted_set_file_data(is_file_data);
}
