//SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm Crashdump Filesystem Hardware Encryption Support Driver API
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 *
 * This header provides the API for crashdump-specific filesystem encryption
 * that automatically separates file data (encrypted) from metadata (unencrypted).
 *
 */

#ifndef __FS_CRASHDUMP_ENCRYPTED_H__
#define __FS_CRASHDUMP_ENCRYPTED_H__

#include <stdbool.h>
#include <linux/types.h>

/**
 * fs_crashdump_encrypted_init() - Initialize crashdump encryption system
 *
 * Call this once during crashdump initialization to set up the encryption system.
 * This must be called before using fs_crashdump_write_encrypted().
 *
 * Example:
 *   int do_crashdump(void) {
 *       fs_crashdump_encrypted_init();
 *       // ... write crashdump files ...
 *       fs_crashdump_encrypted_deinit();
 *   }
 *
 * Return: 0 on success, negative on error
 */
int fs_crashdump_encrypted_init(void);

/**
 * fs_crashdump_write_encrypted() - Write file with explicit encryption control
 * @filename: Name of file to write
 * @addr: Address of data to write
 * @offset: Offset in file to start writing
 * @len: Number of bytes to write
 * @actwrite: Actual bytes written (output)
 * @encrypt: true to encrypt this file, false for no encryption
 *
 * This is the main API that replaces fs_write() for crashdump. It provides
 * automatic separation of file data (encrypted) from metadata (unencrypted).
 *
 * When encrypt=true:
 * - Metadata writes (superblock, inodes, bitmaps) are unencrypted
 * - File data writes are encrypted using ICE hardware
 * - DUN (Data Unit Number) is tracked automatically
 * - ICE is enabled/disabled automatically via storage_crypto_config()
 *
 * When encrypt=false:
 * - Everything is written unencrypted
 * - Works exactly like standard fs_write()
 *
 * Usage Examples:
 *   loff_t written;
 *
 *   // Write encrypted crashdump file
 *   fs_crashdump_write_encrypted("/EBICS0.BIN", addr, 0, size, &written, true);
 *
 *   // Write unencrypted crypto context file
 *   fs_crashdump_write_encrypted("/CRYPTO_CONTEXT.BIN", addr, 0, size, &written, false);
 *
 * Return: 0 if OK with valid @actwrite, negative on error
 */
int fs_crashdump_write_encrypted(const char *filename, ulong addr, loff_t offset,
				 loff_t len, loff_t *actwrite, bool encrypt);

/**
 * fs_crashdump_encrypted_reset_dun() - Reset DUN to specified value
 * @dun: New DUN value
 *
 * Use this to reset the DUN counter, for example when starting a new
 * crashdump session or writing to a different partition.
 *
 * The DUN (Data Unit Number) represents the logical sector position
 * for encryption. It's automatically incremented as files are written.
 *
 * Example:
 *   // Reset DUN to 0 for new crashdump session
 *   fs_crashdump_encrypted_reset_dun(0);
 */
void fs_crashdump_encrypted_reset_dun(u64 dun);

/**
 * fs_crashdump_encrypted_get_dun() - Get current DUN value
 *
 * Returns the current DUN (Data Unit Number) value. Useful for debugging
 * or tracking encryption state.
 *
 * Return: Current DUN value
 */
u64 fs_crashdump_encrypted_get_dun(void);

/**
 * fs_crashdump_encrypted_get_state() - Get current encryption state
 *
 * Check if the encryption system has been initialized.
 *
 * Return: true if encryption system is initialized, false otherwise
 */
bool fs_crashdump_encrypted_get_state(void);

/**
 * fs_crashdump_encrypted_deinit() - Deinitialize encryption system
 *
 * Call this to clean up encryption context and ensure ICE is disabled.
 * Typically called at the end of crashdump collection.
 *
 * Example:
 *   fs_crashdump_encrypted_init();
 *   // ... write crashdump files ...
 *   fs_crashdump_encrypted_deinit();
 */
void fs_crashdump_encrypted_deinit(void);

/**
 * fs_crashdump_encrypted_notify_file_data() - Internal function for filesystem hooks
 * @is_file_data: true when writing file data, false when writing metadata
 * @sector_count: byte count to be converted to DUN (divided by 512)
 *
 * This function is called internally by the filesystem layer (ext4fs_write_file)
 * to notify the encryption system about the type of data being written.
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
 * DO NOT CALL THIS FUNCTION DIRECTLY - it's used by the filesystem hooks in
 * ext4_write.c. The hooks are automatically inserted at the right places to
 * ensure proper metadata/data separation.
 *
 * This is the key function that provides automatic metadata/data separation
 * following the reference patch design pattern.
 */
void fs_crashdump_encrypted_notify_file_data(bool is_file_data, u64 sector_count);

/**
 * fs_crashdump_encrypted_is_file_data() - Check if currently writing file data
 *
 * This is used internally to determine whether encryption should be enabled.
 *
 * Return: true if writing file data (encryption enabled), false if metadata
 */
bool fs_crashdump_encrypted_is_file_data(void);

#endif /* __FS_CRASHDUMP_ENCRYPTED_H__ */
