/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _HTT_EVENT_LOGGING__
#define _HTT_EVENT_LOGGING__

#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/dcache.h>

/* Forward declaration */
struct ath12k_base;

#ifndef HTT_MSG_DBG_MAX_ENTRY
#define HTT_MSG_DBG_MAX_ENTRY (1024)
#endif

#ifndef HTT_MSG_DBG_ENTRY_DEF_LEN
#define HTT_MSG_DBG_ENTRY_DEF_LEN (64)
#endif

/**
 * define NUM_HTT_DEBUG_INFOS - Number of debugfs entries
 *
 * Total number of debugfs file entries created for HTT logging interface.
 */
#define NUM_HTT_DEBUG_INFOS 5

struct htt_msg_debug {
	u8 message_id;
	u8 data[HTT_MSG_DBG_ENTRY_DEF_LEN];
	u64 time;
	u8 cpu_id;
};

/**
 * struct htt_log_buf_t - HTT circular log buffer metadata
 * @buf: pointer to dynamically allocated array of log entries
 * @length: total number of entries logged (may exceed size for wraparound)
 * @buf_tail_idx: current tail index for next write (0 to size-1)
 * @record_lock: spinlock protecting concurrent access to this buffer
 *
 * Manages a circular log buffer for HTT messages. When the buffer fills,
 * new entries overwrite the oldest entries. The record_lock ensures
 * thread-safe access during concurrent logging operations.
 */
struct htt_log_buf_t {
	void *buf;
	u32 length;
	u32 buf_tail_idx;
	/* Spinlock protecting concurrent access to this buffer */
	spinlock_t record_lock;
};

/**
 * struct htt_debug_log_info - HTT logging framework metadata
 * @htt_command_log_buf_info: circular buffer metadata for HTT commands
 * @htt_event_log_buf_info: circular buffer metadata for HTT events
 * @htt_record_lock: global spinlock for debugfs read operations
 * @htt_logging_enable: master enable/disable flag for HTT logging
 * @htt_cmd_disable_list: bitmap of disabled command types (bit N = cmd type N)
 * @htt_event_disable_list: bitmap of disabled event types (bit N = event type N)
 * @htt_log_debugfs_dir: dentry pointer to debugfs directory
 *
 * Central structure containing all metadata for the HTT logging framework.
 * Manages three separate circular buffers (commands, events),
 * their enable/disable states, and the debugfs interface. The disable
 * bitmaps allow selective filtering of specific message types to reduce
 * log volume.
 */
struct htt_debug_log_info {
	struct htt_log_buf_t htt_command_log_buf_info;
	struct htt_log_buf_t htt_event_log_buf_info;
	/* Global spinlock for debugfs read operations */
	spinlock_t htt_record_lock;
	bool htt_logging_enable;
	u64 htt_cmd_disable_list;
	u64 htt_event_disable_list;
	struct dentry *htt_log_debugfs_dir;
};

/**
 * struct htt_logger - HTT logging framework handle
 * @debugfs_de: array of dentry pointers for debugfs file entries
 * @log_info: HTT logging framework metadata and buffer information
 *
 * Top-level handle for the HTT logging framework. Contains references
 * to all debugfs entries and the complete logging metadata. This handle
 * is passed to all HTT logging API functions.
 */
struct htt_logger {
	struct dentry *debugfs_de[NUM_HTT_DEBUG_INFOS];
	struct htt_debug_log_info log_info;
};

/**
 * ath12k_dp_htt_logging_init() - Initialize HTT interface logging framework
 * @htt_logger_handle: pointer to pointer for HTT logger handle (output)
 * @ab: pointer to ath12k_base structure
 *
 * Initializes the complete HTT logging framework including memory allocation,
 * buffer initialization, spinlock setup, and debugfs interface creation.
 * On success, allocates and initializes *htt_logger_handle. On failure,
 * sets *htt_logger_handle to NULL.
 *
 * The function configures default enabled/disabled message types:
 * - Disables all commands by default, enables specific ones
 * - Disables all events by default, enables specific ones
 *
 * Context: Process context
 * Return: None (check *htt_logger_handle for NULL on failure)
 */
void ath12k_dp_htt_logging_init(struct htt_logger **htt_logger_handle,
				struct ath12k_base *ab);

/**
 * ath12k_dp_htt_logging_deinit() - Deinitialize HTT interface logging framework
 * @htt_logger_handle: pointer to HTT logger handle
 *
 * Cleans up the HTT logging framework by removing debugfs entries,
 * freeing all allocated log buffers, and freeing the logger handle.
 * Safe to call with NULL handle (no-op in that case).
 *
 * Context: Process context
 * Return: None
 */
void ath12k_dp_htt_logging_deinit(struct htt_logger *htt_logger_handle);

/**
 * htt_command_record() - Record HTT command to log buffer
 * @h: pointer to HTT logger handle
 * @msg_type: HTT command message type identifier
 * @msg_data: pointer to command data to be logged (may be NULL)
 *
 * Records an HTT command message into the circular log buffer with timestamp
 * and CPU ID. Checks if logging is enabled and if the specific command type
 * is not disabled before recording. Uses atomic memory allocation and spinlock
 * protection for thread safety. If msg_data is NULL, fills buffer with 0xFF.
 *
 * Context: Atomic context, may be called from interrupt handlers
 * Return: 0 on success, -ENOMEM on memory allocation failure
 */
int ath12k_dp_htt_message_record(struct htt_logger *h, u8 msg_type,
				 u8 *msg_data,
				 enum dp_htt_logger_type log_type);

#endif
