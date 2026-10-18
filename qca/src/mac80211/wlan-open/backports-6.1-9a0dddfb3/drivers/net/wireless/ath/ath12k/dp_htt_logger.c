// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/ktime.h>
#include "core.h"
#include "debug.h"
#include "dp_htt.h"
#include "dp_htt_logger.h"

static int ath12k_dp_htt_debugfs_init(struct htt_logger *htt_logger_handle,
				      struct ath12k_base *ab, u8 psoc_id);

#define FILE_MODE 00660
#define DEBUGFS_FNAME_LEN 32

/**
 * DISABLE_ALL_COMMAND(): Disable all command
 * DISABLE_ALL_EVENT(): Disable all event
 * ENABLE_EVENT(): Enable specific event
 */
#define DISABLE_ALL_COMMAND(htt_logger_handle) \
	((htt_logger_handle)->log_info.htt_cmd_disable_list = 0xFFFFFFFFFFFFFFFF)
#define DISABLE_ALL_EVENT(htt_logger_handle) \
	((htt_logger_handle)->log_info.htt_event_disable_list = 0xFFFFFFFFFFFFFFFF)
#define ENABLE_ALL_EVENT(htt_logger_handle) \
	((htt_logger_handle)->log_info.htt_event_disable_list = 0)
#define ENABLE_HTT_LOGGING(htt_logger_handle) \
	((htt_logger_handle)->log_info.htt_logging_enable = 1)
#define ENABLE_EVENT(htt_logger_handle, eventid) \
do { \
	u64 htt_disable_mask = ~(0x1ULL << (eventid)); \
	(htt_logger_handle)->log_info.htt_event_disable_list &= \
		htt_disable_mask; \
} while (0)

#define ENABLE_COMMAND(htt_logger_handle, cmd_id) \
do { \
	u64 htt_disable_mask = ~(0x1ULL << (cmd_id)); \
	(htt_logger_handle)->log_info.htt_cmd_disable_list &= \
		htt_disable_mask; \
} while (0)

/**
 * DEFINE_HTT_LOG_FOPS() - Macro to generate file_operations for log files
 * @name: base name for the operations structure
 * @show_fn: show function for seq_file operations
 * @write_fn: write function for the file
 *
 * This macro generates the boilerplate code for debugfs file operations
 * including open, read, write, llseek, and release handlers.
 */
#define DEFINE_HTT_LOG_FOPS(name, show_fn, write_fn) \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, show_fn, inode->i_private); \
} \
static const struct file_operations name##_ops = { \
	.owner = THIS_MODULE, \
	.open = name##_open, \
	.read = seq_read, \
	.write = write_fn, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

/**
 * ath12k_dp_seq_print() - Print formatted output to seq_file
 * @m: seq_file structure for output
 * @fmt: format string
 * @...: variable arguments for format string
 *
 * Helper function to print formatted output to seq_file with variable
 * arguments support.
 *
 * Context: Any context
 * Return: None
 */
static void ath12k_dp_seq_print(struct seq_file *m, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (m)
		seq_vprintf(m, fmt, args);
	/**
	 * seq_printf expands to this call
	 */
	va_end(args);
}

/**
 * ath12k_dp_get_log_time() - Get current timestamp in nanoseconds
 *
 * Retrieves the current real time in nanoseconds for logging purposes.
 *
 * Context: Any context
 * Return: Current time in nanoseconds
 */
static inline u64 ath12k_dp_get_log_time(void)
{
	return ktime_get_real_ns();
}

/**
 * ath12k_dp_log_time_to_secs() - Convert nanosecond timestamp to seconds and microseconds
 * @time: pointer to timestamp in nanoseconds
 * @secs: pointer to store seconds component
 * @usecs: pointer to store microseconds component
 *
 * Converts a nanosecond timestamp into separate seconds and microseconds
 * components for human-readable display.
 *
 * Context: Any context
 * Return: None
 */
static void ath12k_dp_log_time_to_secs(u64 *time, u64 *secs, u64 *usecs)
{
	u64 total_usecs = div_u64(*time, 1000ul);
	*secs = div_u64(total_usecs, 1000000ul);
	*usecs = total_usecs - (*secs * 1000000ul);
}

/**
 * ath12k_dp_log_buf_init() - Initialize HTT log buffer
 * @buf: pointer to HTT log buffer structure
 *
 * Initializes the HTT log buffer by resetting length, tail index,
 * and setting the buffer size to the maximum entry count.
 *
 * Context: Any context
 * Return: None
 */
static inline void ath12k_dp_log_buf_init(struct htt_log_buf_t *buf)
{
	buf->length = 0;
	buf->buf_tail_idx = 0;
}

/**
 * htt_command_record() - Record HTT command to log buffer
 * @logger_handle: pointer to HTT logger handle
 * @msg_type: HTT command message type identifier
 * @msg_data: pointer to command data to be logged
 *
 * Records an HTT command message into the circular log buffer. The function
 * checks if logging is enabled and if the specific command type is not
 * disabled before recording. Uses atomic memory allocation and spinlock
 * protection for thread safety.
 *
 * Context: Atomic context, may be called from interrupt handlers
 * Return: 0 on success, -ENOMEM on memory allocation failure
 */
int ath12k_dp_htt_message_record(struct htt_logger *logger_handle,
				 u8 msg_type, u8 *msg_data,
				 enum dp_htt_logger_type log_type)
{
	struct htt_msg_debug *buf = NULL;
	u32 *p_buf_tail_idx = NULL;
	u8 *tdata = NULL;
	struct htt_log_buf_t *msg_log_buf;
	u64 disable_list;

	/* return 0 if:
	 * 1. Initialization failed.
	 * 2. htt_logging_enable is 0 -> can get disable from file ops
	 * 3. This specific msg_type is disabled
	 */
	if (!logger_handle)
		return 0;

	if (log_type == HTT_LOGGER_COMMAND)
		msg_log_buf = &logger_handle->log_info.htt_command_log_buf_info;
	else
		msg_log_buf = &logger_handle->log_info.htt_event_log_buf_info;

	if (!msg_data) {
		/* if msg_data is NULL then fill 0xFF in data field */
		tdata = kmalloc(HTT_MSG_DBG_ENTRY_DEF_LEN, GFP_ATOMIC);
		if (!tdata)
			return -ENOMEM;
		memset(tdata, 0xFF, HTT_MSG_DBG_ENTRY_DEF_LEN);
		msg_data = tdata;
	}

	spin_lock_bh(&msg_log_buf->record_lock);

	if (log_type == HTT_LOGGER_COMMAND)
		disable_list = logger_handle->log_info.htt_cmd_disable_list;
	else
		disable_list = logger_handle->log_info.htt_event_disable_list;

	if (logger_handle->log_info.htt_logging_enable == 0 ||
	    ((1ULL << msg_type) & disable_list)) {
		spin_unlock_bh(&msg_log_buf->record_lock);
		kfree(tdata);
		return 0;
	}

	p_buf_tail_idx = &msg_log_buf->buf_tail_idx;

	/* rewind pointer index if buffer became full */
	if (*p_buf_tail_idx >= HTT_MSG_DBG_MAX_ENTRY)
		*p_buf_tail_idx = 0;

	buf = msg_log_buf->buf;

	buf[*p_buf_tail_idx].message_id = msg_type;
	memcpy(buf[*p_buf_tail_idx].data, msg_data,
	       HTT_MSG_DBG_ENTRY_DEF_LEN);
	buf[*p_buf_tail_idx].time = ath12k_dp_get_log_time();
	buf[*p_buf_tail_idx].cpu_id = smp_processor_id();
	(*p_buf_tail_idx)++;
	msg_log_buf->length++;
	spin_unlock_bh(&msg_log_buf->record_lock);

	kfree(tdata);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_htt_message_record);

/**
 * ath12k_dp_dbg_htt_cmd_log_show() - Display HTT command log via debugfs
 * @file: seq_file structure for output
 * @arg: private data containing htt_logger_handle
 *
 * Displays the contents of the HTT command log buffer through debugfs.
 * order in a clean tabular format, including command ID, data bytes,
 * timestamp, and CPU ID.
 *
 * Context: Process context
 * Return: 0 on success
 */
int ath12k_dp_dbg_htt_cmd_log_show(struct seq_file *file, void *arg)
{
	struct htt_logger *htt_logger_handle;
	struct htt_log_buf_t *htt_log;
	struct htt_msg_debug *temp_buf = NULL;
	int pos, nread, i, entry_num;
	u64 secs, usecs;
	u32 total_entries;
	u32 buf_tail_idx;

	htt_logger_handle = (struct htt_logger *)file->private;

	if (!htt_logger_handle) {
		ath12k_dp_seq_print(file, "Logger not initialized\n");
		return -EINVAL;
	}

	htt_log = &htt_logger_handle->log_info.htt_command_log_buf_info;

	temp_buf = kcalloc(HTT_MSG_DBG_MAX_ENTRY,
			   sizeof(struct htt_msg_debug),
			   GFP_KERNEL);
	if (!temp_buf) {
		ath12k_dp_seq_print(file, "Memory allocation failed\n");
		return -ENOMEM;
	}

	spin_lock_bh(&htt_log->record_lock);

	if (!(htt_log->length)) {
		spin_unlock_bh(&htt_log->record_lock);
		kfree(temp_buf);
		ath12k_dp_seq_print(file, "No elements to read from ring buffer!\n");
		return 0;
	}
	if (htt_log->length <= HTT_MSG_DBG_MAX_ENTRY)
		nread = htt_log->length;
	else
		nread = HTT_MSG_DBG_MAX_ENTRY;

	total_entries = htt_log->length;
	buf_tail_idx = htt_log->buf_tail_idx;

	memcpy(temp_buf, htt_log->buf,
	       HTT_MSG_DBG_MAX_ENTRY * sizeof(struct htt_msg_debug));

	spin_unlock_bh(&htt_log->record_lock);

	if (buf_tail_idx == 0)
		pos = HTT_MSG_DBG_MAX_ENTRY - 1;
	else
		pos = buf_tail_idx - 1;

	/* All printing done without holding lock */
	ath12k_dp_seq_print(file, "HTT Command Log (Total entries: %d, Showing: %d)\n",
			    total_entries, nread);
	ath12k_dp_seq_print(file, "==============================================");
	ath12k_dp_seq_print(file, "================================\n");
	ath12k_dp_seq_print(file, "%-5s %-8s %-6s %-50s\n",
			    "Entry", "CMD_ID", "CPU", "Data");
	ath12k_dp_seq_print(file, "----------------------------------------------");
	ath12k_dp_seq_print(file, "------------------------------------\n");

	entry_num = 1;
	while (nread--) {
		struct htt_msg_debug *htt_record;

		htt_record = &temp_buf[pos];

		ath12k_dp_log_time_to_secs(&htt_record->time, &secs, &usecs);

		ath12k_dp_seq_print(file, "%-5d 0x%-6x %-6d ",
				    entry_num, htt_record->message_id,
				    htt_record->cpu_id);

		for (i = 0; i < HTT_MSG_DBG_ENTRY_DEF_LEN; i++) {
			ath12k_dp_seq_print(file, "%02x ", htt_record->data[i]);
			if ((i + 1) % 16 == 0 &&
			    i < HTT_MSG_DBG_ENTRY_DEF_LEN - 1)
				ath12k_dp_seq_print(file,
						    "\n%-5s %-8s %-6s ", "", "", "");
		}
		ath12k_dp_seq_print(file, "\n      Timestamp: [%llu.%06llu]\n",
				    secs, usecs);

		if (pos == 0)
			pos = HTT_MSG_DBG_MAX_ENTRY - 1;
		else
			pos--;
		entry_num++;
	}
	ath12k_dp_seq_print(file, "=============================================");
	ath12k_dp_seq_print(file, "=======================================\n");

	kfree(temp_buf);
	return 0;
}

/**
 * ath12k_dp_dbg_htt_event_log_show() - Display HTT event log via debugfs
 * @file: seq_file structure for output
 * @arg: private data containing htt_logger_handle
 *
 * Displays the contents of the HTT event log buffer through debugfs.
 * Shows up to HTT_MSG_DBG_MAX_ENTRY most recent entries in reverse chronological
 * order, including event ID, data bytes, timestamp, and CPU ID.
 *
 * Context: Process context
 * Return: 0 on success
 */
int ath12k_dp_dbg_htt_event_log_show(struct seq_file *file, void *arg)
{
	struct htt_logger *htt_logger_handle;
	struct htt_log_buf_t *htt_log = NULL;
	struct htt_msg_debug *temp_buf = NULL;
	int pos, nread, i, entry_num;
	u64 secs, usecs;
	u32 total_entries;
	u32 buf_tail_idx;

	htt_logger_handle = (struct htt_logger *)file->private;

	if (!htt_logger_handle) {
		ath12k_dp_seq_print(file, "Logger not initialized\n");
		return -EINVAL;
	}

	htt_log = &htt_logger_handle->log_info.htt_event_log_buf_info;
	temp_buf = kcalloc(HTT_MSG_DBG_MAX_ENTRY,
			   sizeof(struct htt_msg_debug),
			   GFP_KERNEL);
	if (!temp_buf) {
		ath12k_dp_seq_print(file, "Memory allocation failed\n");
		return -ENOMEM;
	}
	spin_lock_bh(&htt_log->record_lock);

	if (!(htt_log->length)) {
		spin_unlock_bh(&htt_log->record_lock);
		kfree(temp_buf);
		ath12k_dp_seq_print(file, "No elements to read from ring buffer!\n");
		return 0;
	}

	if (htt_log->length <= HTT_MSG_DBG_MAX_ENTRY)
		nread = htt_log->length;
	else
		nread = HTT_MSG_DBG_MAX_ENTRY;

	total_entries = htt_log->length;
	buf_tail_idx = htt_log->buf_tail_idx;

	memcpy(temp_buf, htt_log->buf,
	       HTT_MSG_DBG_MAX_ENTRY * sizeof(struct htt_msg_debug));

	spin_unlock_bh(&htt_log->record_lock);

	if (buf_tail_idx == 0)
		pos = HTT_MSG_DBG_MAX_ENTRY - 1;
	else
		pos = buf_tail_idx - 1;

	ath12k_dp_seq_print(file, "HTT Event Log (Total entries: %d, Showing: %d)\n",
			    total_entries, nread);
	ath12k_dp_seq_print(file, "==============================================");
	ath12k_dp_seq_print(file, "================================\n");
	ath12k_dp_seq_print(file, "%-5s %-8s %-6s %-50s\n",
			    "Entry", "EVT_ID", "CPU", "Data");
	ath12k_dp_seq_print(file, "----------------------------------------------");
	ath12k_dp_seq_print(file, "------------------------------------\n");

	entry_num = 1;
	while (nread--) {
		struct htt_msg_debug *htt_record;

		htt_record = &temp_buf[pos];

		ath12k_dp_log_time_to_secs(&htt_record->time, &secs, &usecs);

		ath12k_dp_seq_print(file, "%-5d 0x%-6x %-6d ",
				    entry_num, htt_record->message_id,
				    htt_record->cpu_id);

		for (i = 0; i < HTT_MSG_DBG_ENTRY_DEF_LEN; i++) {
			ath12k_dp_seq_print(file, "%02x ", htt_record->data[i]);
			if ((i + 1) % 16 == 0 &&
			    i < HTT_MSG_DBG_ENTRY_DEF_LEN - 1)
				ath12k_dp_seq_print(file,
						    "\n%-5s %-8s %-6s ", "", "", "");
		}
		ath12k_dp_seq_print(file, "\n      Timestamp: [%llu.%06llu]\n",
				    secs, usecs);

		if (pos == 0)
			pos = HTT_MSG_DBG_MAX_ENTRY - 1;
		else
			pos--;
		entry_num++;
	}
	ath12k_dp_seq_print(file, "=============================================");
	ath12k_dp_seq_print(file, "=======================================\n");

	kfree(temp_buf);
	return 0;
}

/**
 * ath12k_dp_htt_dbg_clear_logbuf() - Clear HTT log buffer contents
 * @log: pointer to HTT log buffer structure
 *
 * Clears the HTT log buffer by zeroing the buffer contents and resetting
 * the length and tail index. Must be called with record_lock held.
 *
 * Context: Any context (caller must hold record_lock)
 * Return: None
 */
static void ath12k_dp_htt_dbg_clear_logbuf(struct htt_log_buf_t *log)
{
	memset(log->buf, 0, HTT_MSG_DBG_MAX_ENTRY * sizeof(struct htt_msg_debug));
	log->length = 0;
	log->buf_tail_idx = 0;
}

/**
 * htt_dbgfs_log_clear_write_common() - Common handler for clearing log buffers
 * @file: file structure for the debugfs entry
 * @user_buf: user input buffer (expects "0" to clear)
 * @count: length of input buffer
 * @log: pointer to the specific log buffer to clear
 *
 * Common implementation for clearing HTT log buffers via debugfs.
 * Validates that the user writes exactly "0", then clears the specified
 * log buffer under spinlock protection.
 *
 * Context: Process context
 * Return: count on success, negative error code on failure
 */
static ssize_t ath12k_dp_htt_dbg_clear_write(struct file *file,
					     const char __user *user_buf,
					     size_t count,
					     struct htt_log_buf_t *log)
{
	int k, ret;

	/* Use kernel helper to read integer from user buffer */
	ret = kstrtoint_from_user(user_buf, count, 0, &k);
	if (ret)
		return ret;
	if (k != 0)
		return -EINVAL;

	spin_lock_bh(&log->record_lock);
	ath12k_dp_htt_dbg_clear_logbuf(log);
	spin_unlock_bh(&log->record_lock);

	return count;
}

/**
 * ath12k_do_dbg_htt_cmd_log_write() - Clear HTT command log buffer via debugfs
 * @file: file structure for the debugfs entry
 * @user_buf: user input buffer (expects "0" to clear)
 * @count: length of input buffer
 * @ppos: file position (unused)
 *
 * Clears the HTT command log buffer when user writes "0" to the debugfs
 * entry. Resets buffer length and tail index to zero.
 *
 * Context: Process context
 * Return: count on success, negative error code on failure
 */
static ssize_t ath12k_do_dbg_htt_cmd_log_write(struct file *file,
					       const char __user *user_buf,
					       size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct htt_logger *htt_logger_handle = s ? s->private : NULL;
	struct htt_log_buf_t *log;

	if (!htt_logger_handle)
		return -EINVAL;

	log = &htt_logger_handle->log_info.htt_command_log_buf_info;
	return ath12k_dp_htt_dbg_clear_write(file, user_buf, count, log);
}

/**
 * ath12k_dp_dbg_htt_event_log_write() - Clear HTT event log buffer via debugfs
 * @file: file structure for the debugfs entry
 * @user_buf: user input buffer (expects "0" to clear)
 * @count: length of input buffer
 * @ppos: file position (unused)
 *
 * Clears the HTT event log buffer when user writes "0" to the debugfs
 * entry. Resets buffer length and tail index to zero.
 *
 * Context: Process context
 * Return: count on success, negative error code on failure
 */
static ssize_t ath12k_dp_dbg_htt_event_log_write(struct file *file,
						 const char __user *user_buf,
						 size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct htt_logger *htt_logger_handle = s ? s->private : NULL;
	struct htt_log_buf_t *log;

	if (!htt_logger_handle)
		return -EINVAL;

	log = &htt_logger_handle->log_info.htt_event_log_buf_info;
	return ath12k_dp_htt_dbg_clear_write(file, user_buf, count, log);
}

/**
 * ath12k_dp_htt_enable_get() - Get HTT logging enable state
 * @data: pointer to htt_logger structure
 * @val: pointer to store the enable state value
 *
 * Retrieves the current enable/disable state of HTT logging.
 *
 * Context: Any context
 * Return: 0 on success
 */
static int ath12k_dp_htt_enable_get(void *data, u64 *val)
{
	struct htt_logger *htt_logger_handle = data;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	cmd_log_buf = &htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf = &htt_logger_handle->log_info.htt_event_log_buf_info;

	spin_lock_bh(&cmd_log_buf->record_lock);
	spin_lock_bh(&event_log_buf->record_lock);
	*val = htt_logger_handle->log_info.htt_logging_enable;
	spin_unlock_bh(&event_log_buf->record_lock);
	spin_unlock_bh(&cmd_log_buf->record_lock);

	return 0;
}

/**
 * ath12k_dp_htt_enable_set() - Set HTT logging enable state
 * @data: pointer to htt_logger structure
 * @val: new enable state value (0 or 1)
 *
 * Sets the enable/disable state of HTT logging. Acquires locks on both
 * command and event log buffers to ensure thread safety.
 *
 * Context: Any context
 * Return: 0 on success
 */
static int ath12k_dp_htt_enable_set(void *data, u64 val)
{
	struct htt_logger *htt_logger_handle = data;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	if (val != 0 && val != 1)
		return -EINVAL;

	cmd_log_buf = &htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf = &htt_logger_handle->log_info.htt_event_log_buf_info;

	spin_lock_bh(&cmd_log_buf->record_lock);
	spin_lock_bh(&event_log_buf->record_lock);
	htt_logger_handle->log_info.htt_logging_enable = (val != 0);
	spin_unlock_bh(&event_log_buf->record_lock);
	spin_unlock_bh(&cmd_log_buf->record_lock);

	return 0;
}

/* Generates htt_enable_fops automatically */
DEFINE_SIMPLE_ATTRIBUTE(htt_enable_fops, ath12k_dp_htt_enable_get,
			ath12k_dp_htt_enable_set, "%llu\n");

/**
 * ath12k_dp_htt_cmd_disable_get() - Get command disable list bitmap
 * @data: pointer to htt_logger structure
 * @val: pointer to store the disable list value
 *
 * Retrieves the bitmap of disabled HTT command IDs.
 *
 * Context: Any context
 * Return: 0 on success
 */
static int ath12k_dp_htt_cmd_disable_get(void *data, u64 *val)
{
	struct htt_logger *htt_logger_handle = data;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	cmd_log_buf = &htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf = &htt_logger_handle->log_info.htt_event_log_buf_info;

	spin_lock_bh(&cmd_log_buf->record_lock);
	spin_lock_bh(&event_log_buf->record_lock);
	*val = htt_logger_handle->log_info.htt_cmd_disable_list;
	spin_unlock_bh(&event_log_buf->record_lock);
	spin_unlock_bh(&cmd_log_buf->record_lock);

	return 0;
}

/**
 * ath12k_dp_htt_cmd_disable_set() - Set command disable list bitmap
 * @data: pointer to htt_logger structure
 * @val: new disable list bitmap value
 *
 * Sets the bitmap of disabled HTT command IDs. Acquires locks on both
 * command and event log buffers to ensure thread safety.
 *
 * Context: Any context
 * Return: 0 on success
 */
static int ath12k_dp_htt_cmd_disable_set(void *data, u64 val)
{
	struct htt_logger *htt_logger_handle = data;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	cmd_log_buf = &htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf = &htt_logger_handle->log_info.htt_event_log_buf_info;

	spin_lock_bh(&cmd_log_buf->record_lock);
	spin_lock_bh(&event_log_buf->record_lock);
	htt_logger_handle->log_info.htt_cmd_disable_list = val;
	spin_unlock_bh(&event_log_buf->record_lock);
	spin_unlock_bh(&cmd_log_buf->record_lock);

	return 0;
}

/* Generates htt_cmd_disable_fops automatically */
DEFINE_SIMPLE_ATTRIBUTE(htt_cmd_disable_fops, ath12k_dp_htt_cmd_disable_get,
			ath12k_dp_htt_cmd_disable_set, "%llu\n");

/**
 * ath12k_dp_htt_event_disable_get() - Get event disable list bitmap
 * @data: pointer to htt_logger structure
 * @val: pointer to store the disable list value
 *
 * Retrieves the bitmap of disabled HTT event IDs.
 *
 * Context: Any context
 * Return: 0 on success
 */
static int ath12k_dp_htt_event_disable_get(void *data, u64 *val)
{
	struct htt_logger *htt_logger_handle = data;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	cmd_log_buf = &htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf = &htt_logger_handle->log_info.htt_event_log_buf_info;

	spin_lock_bh(&cmd_log_buf->record_lock);
	spin_lock_bh(&event_log_buf->record_lock);
	*val = htt_logger_handle->log_info.htt_event_disable_list;
	spin_unlock_bh(&event_log_buf->record_lock);
	spin_unlock_bh(&cmd_log_buf->record_lock);
	return 0;
}

/**
 * ath12k_dp_htt_event_disable_set() - Set event disable list bitmap
 * @data: pointer to htt_logger structure
 * @val: new disable list bitmap value
 *
 * Sets the bitmap of disabled HTT event IDs. Acquires locks on both
 * command and event log buffers to ensure thread safety.
 *
 * Context: Any context
 * Return: 0 on success
 */
static int ath12k_dp_htt_event_disable_set(void *data, u64 val)
{
	struct htt_logger *htt_logger_handle = data;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	cmd_log_buf = &htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf = &htt_logger_handle->log_info.htt_event_log_buf_info;

	spin_lock_bh(&cmd_log_buf->record_lock);
	spin_lock_bh(&event_log_buf->record_lock);
	htt_logger_handle->log_info.htt_event_disable_list = val;
	spin_unlock_bh(&event_log_buf->record_lock);
	spin_unlock_bh(&cmd_log_buf->record_lock);

	return 0;
}

/* Generates htt_event_disable_fops automatically */
DEFINE_SIMPLE_ATTRIBUTE(htt_event_disable_fops, ath12k_dp_htt_event_disable_get,
			ath12k_dp_htt_event_disable_set, "%llu\n");

/* Structure to maintain debug information */
struct ath12k_dp_htt_debugfs_info {
	const char *name;
	const struct file_operations *ops;
	void *priv;
};

/* Define the log file operations using the macro */
DEFINE_HTT_LOG_FOPS(debug_htt_command_log, ath12k_dp_dbg_htt_cmd_log_show,
		    ath12k_do_dbg_htt_cmd_log_write);
DEFINE_HTT_LOG_FOPS(debug_htt_event_log, ath12k_dp_dbg_htt_event_log_show,
		    ath12k_dp_dbg_htt_event_log_write);

struct ath12k_dp_htt_debugfs_info htt_debugfs_infos[NUM_HTT_DEBUG_INFOS] = {
	{ .name = "htt_command_log", .ops = &debug_htt_command_log_ops },
	{ .name = "htt_event_log", .ops = &debug_htt_event_log_ops },
	{ .name = "htt_enable", .ops = &htt_enable_fops },
	{ .name = "htt_cmd_disable_list", .ops = &htt_cmd_disable_fops },
	{ .name = "htt_event_disable_list", .ops = &htt_event_disable_fops },
};

/**
 * ath12k_dp_htt_log_buffer_free() - Free all dynamically allocated log buffers
 * @htt_logger_handle: pointer to HTT logger handle
 *
 * Frees all dynamically allocated memory for HTT command and event log buffers.
 * Disables logging before freeing to prevent access to freed memory.
 * Takes record locks to ensure no concurrent access during buffer deallocation.
 *
 * Context: Process context
 * Return: None
 */
static inline void ath12k_dp_htt_log_buffer_free(struct htt_logger *htt_logger_handle)
{
	struct htt_debug_log_info *log_info_p = &htt_logger_handle->log_info;
	struct htt_log_buf_t *cmd_log_buf = &log_info_p->htt_command_log_buf_info;
	struct htt_log_buf_t *event_log_buf = &log_info_p->htt_event_log_buf_info;

	log_info_p->htt_logging_enable = 0;

	/* Free allocated buffers after releasing locks and disabling logging.
	 * At this point, no new records can be added (logging disabled),
	 * and all in-flight operations have completed (locks released).
	 */
	kfree(cmd_log_buf->buf);
	cmd_log_buf->buf = NULL;

	kfree(event_log_buf->buf);
	event_log_buf->buf = NULL;
}

/**
 * ath12k_dp_htt_log_buffer_alloc() - Allocate memory for all HTT log buffers
 * @htt_logger_handle: pointer to HTT logger handle
 *
 * Allocates memory for HTT command and event log buffers.
 * Each buffer can hold HTT_MSG_DBG_MAX_ENTRY entries. Disables
 * logging initially until initialization is complete.
 *
 * Context: Process context
 * Return: 0 on success, -ENOMEM on allocation failure
 */
static int ath12k_dp_htt_log_buffer_alloc(struct htt_logger *htt_logger_handle)
{
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	cmd_log_buf =
		&htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf =
		&htt_logger_handle->log_info.htt_event_log_buf_info;

	/* first disable logging and then free buffer and release lock */
	htt_logger_handle->log_info.htt_logging_enable = 0;

	/* Allocate data buffer */
	cmd_log_buf->buf = kcalloc(HTT_MSG_DBG_MAX_ENTRY,
				   sizeof(struct htt_msg_debug),
				   GFP_KERNEL);
	if (!cmd_log_buf->buf)
		goto error_mem_fail;

	event_log_buf->buf = kcalloc(HTT_MSG_DBG_MAX_ENTRY,
				     sizeof(struct htt_msg_debug),
				     GFP_KERNEL);
	if (!event_log_buf->buf)
		goto error_mem_fail;
	return 0;

error_mem_fail:
	ath12k_dp_htt_log_buffer_free(htt_logger_handle);
	return -ENOMEM;
}

/**
 * ath12k_dp_htt_log_lock_alloc() - Initialize spinlocks for HTT logging
 * @htt_logger_handle: pointer to HTT logger handle
 *
 * Initializes spinlocks for thread-safe access to HTT log buffers.
 * Creates separate locks for command and event buffers,
 * plus a global recording lock for debugfs read operations.
 *
 * Context: Any context
 * Return: None
 */
static inline void ath12k_dp_htt_log_lock_alloc(struct htt_logger *htt_logger_handle)
{
	struct htt_debug_log_info *log_info_p = &htt_logger_handle->log_info;
	struct htt_log_buf_t *cmd_log_buf =
		&htt_logger_handle->log_info.htt_command_log_buf_info;
	struct htt_log_buf_t *event_log_buf =
		&htt_logger_handle->log_info.htt_event_log_buf_info;

	/* Create lock for cmd/event */
	spin_lock_init(&cmd_log_buf->record_lock);
	spin_lock_init(&event_log_buf->record_lock);

	/* Create recording lock in read through debugfs */
	spin_lock_init(&log_info_p->htt_record_lock);
}

/**
 * ath12k_dp_htt_debugfs_remove() - Remove debugfs entries for HTT logging
 * @htt_logger_handle: pointer to HTT logger handle
 *
 * Removes all debugfs entries and the debugfs directory created for
 * HTT logging. Cleans up all file entries before removing the parent
 * directory.
 *
 * Context: Process context
 * Return: None
 */
static void ath12k_dp_htt_debugfs_remove(struct htt_logger *htt_logger_handle)
{
	int i;
	struct dentry *dentry = htt_logger_handle->log_info.htt_log_debugfs_dir;

	if (dentry) {
		for (i = 0; i < NUM_HTT_DEBUG_INFOS; ++i) {
			if (htt_logger_handle->debugfs_de[i])
				htt_logger_handle->debugfs_de[i] = NULL;
		}
	}

	debugfs_remove_recursive(dentry);
}

/**
 * ath12k_dp_htt_logging_deinit() - Deinitialize HTT interface logging framework
 * @htt_logger_handle: pointer to HTT logger handle
 *
 * Cleans up the HTT logging framework by:
 * - Removing debugfs entries
 * - Freeing all allocated log buffers
 * - Freeing the logger handle structure
 *
 * Safe to call with NULL handle (no-op in that case).
 *
 * Context: Process context
 * Return: None
 */
void ath12k_dp_htt_logging_deinit(struct htt_logger *htt_logger_handle)
{
	/**
	 * This case will hit in deinit path
	 *   if htt_initialization failed
	 */
	if (!htt_logger_handle)
		return;
	ath12k_dp_htt_debugfs_remove(htt_logger_handle);
	ath12k_dp_htt_log_buffer_free(htt_logger_handle);
	kfree(htt_logger_handle);
}

/**
 * ath12k_dp_htt_debugfs_init() - Initialize debugfs interface for HTT logging
 * @htt_logger_handle: pointer to HTT logger handle
 * @ab: pointer to ath12k_base structure
 * @psoc_id: physical SoC identifier
 *
 * Creates debugfs directory and file entries for HTT logging interface.
 * The directory name is formatted as "htt_ath<psoc_id>_logger". Creates
 * entries for command log, event log, enable/disable
 * controls, and configuration parameters.
 *
 * Context: Process context
 * Return: 0 on success, -EFAULT on failure
 */
static int ath12k_dp_htt_debugfs_init(struct htt_logger *htt_logger_handle,
				      struct ath12k_base *ab, u8 psoc_id)
{
	char buf[DEBUGFS_FNAME_LEN];
	int i;

	snprintf(buf, sizeof(buf), "htt_ath%d_logger", psoc_id);

	htt_logger_handle->log_info.htt_log_debugfs_dir =
		debugfs_create_dir(buf, NULL);

	if (!htt_logger_handle->log_info.htt_log_debugfs_dir) {
		ath12k_err(ab, "error while creating debugfs dir for %s\n", buf);
		return -EFAULT;
	}

	for (i = 0; i < NUM_HTT_DEBUG_INFOS; ++i) {
		struct dentry *dir;

		dir = htt_logger_handle->log_info.htt_log_debugfs_dir;
		htt_logger_handle->debugfs_de[i] =
			debugfs_create_file(htt_debugfs_infos[i].name,
					    FILE_MODE, dir,
					    htt_logger_handle,
					    htt_debugfs_infos[i].ops);

		if (!htt_logger_handle->debugfs_de[i])
			goto out;
	}

	return 0;
out:
	ath12k_err(ab, "debug Entry creation failed[htt_command_log]!\n");
	ath12k_dp_htt_debugfs_remove(htt_logger_handle);
	return -EFAULT;
}

/**
 * ath12k_dp_htt_logging_init() - Initialize HTT interface logging framework
 * @phtt_logger_handle: pointer to pointer for HTT logger handle
 * @ab: pointer to ath12k_base structure
 *
 * Initializes the complete HTT logging framework including:
 * - Memory allocation for logger handle and log buffers
 * - Initialization of circular buffers for commands, and events
 * - Configuration of default enabled/disabled message types
 * - Spinlock initialization for thread safety
 * - Debugfs interface creation
 *
 * By default, disables all commands and events, then selectively enables
 * specific message types of interest.
 *
 * Context: Process context
 * Return: None (sets *phtt_logger_handle to NULL on failure)
 */
void ath12k_dp_htt_logging_init(struct htt_logger **phtt_logger_handle,
				struct ath12k_base *ab)
{
	int ret;
	struct htt_logger *htt_logger_handle;
	struct htt_log_buf_t *cmd_log_buf;
	struct htt_log_buf_t *event_log_buf;

	*phtt_logger_handle = kzalloc(sizeof(**phtt_logger_handle), GFP_KERNEL);
	if (!*phtt_logger_handle) {
		ath12k_err(ab, "Memory allocation for HTT logger buffer failed\n");
		return;
	}
	/* Allocate Buffer */
	if (ath12k_dp_htt_log_buffer_alloc(*phtt_logger_handle) != 0) {
		ath12k_err(ab, "HTT log buffer allocation failed\n");
		kfree(*phtt_logger_handle);
		*phtt_logger_handle = NULL;
		return;
	}
	htt_logger_handle = *phtt_logger_handle;
	cmd_log_buf =
		&htt_logger_handle->log_info.htt_command_log_buf_info;
	event_log_buf =
		&htt_logger_handle->log_info.htt_event_log_buf_info;

	/* Initialize HTT Cmd/Event */
	ath12k_dp_log_buf_init(cmd_log_buf);
	ath12k_dp_log_buf_init(event_log_buf);
	/*
	 * Disable logging all commands and enable for specific
	 * commands
	 */
	DISABLE_ALL_COMMAND(htt_logger_handle);
	ENABLE_COMMAND(htt_logger_handle, HTT_H2T_MSG_TYPE_TX_MONITOR_CFG);
	ENABLE_COMMAND(htt_logger_handle, HTT_H2T_MSG_TYPE_SRING_SETUP);
	ENABLE_COMMAND(htt_logger_handle,
		       HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG);
	ENABLE_COMMAND(htt_logger_handle,
		       HTT_H2T_MSG_TYPE_TX_MONITOR_CFG);
	ENABLE_COMMAND(htt_logger_handle,
		       HTT_H2T_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_RESP);

	/*
	 *  Disable logging for all events and enable for specific
	 *  events
	 */
	DISABLE_ALL_EVENT(htt_logger_handle);

	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_PEER_MAP);
	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_PEER_UNMAP);
	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_RX_ADDBA);
	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_RX_ADDBA);
	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_PEER_MAP2);
	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_PEER_UNMAP2);
	ENABLE_EVENT(htt_logger_handle, HTT_T2H_MSG_TYPE_PEER_MAP3);
	ENABLE_EVENT(htt_logger_handle,
		     HTT_T2H_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_IND);

	/* Create lock for all event - MUST be done before debugfs init */
	ath12k_dp_htt_log_lock_alloc(htt_logger_handle);

	/* Enable HTT logging */
	ENABLE_HTT_LOGGING(htt_logger_handle);
	/* Initialize debugfs AFTER locks are created */
	ret = ath12k_dp_htt_debugfs_init(*phtt_logger_handle, ab,
					 ath12k_get_ab_device_id(ab));
	if (ret != 0)
		goto debugfs_init_failed;

	return;

debugfs_init_failed:
	ath12k_dp_htt_logging_deinit(*phtt_logger_handle);
	*phtt_logger_handle = NULL;
}
EXPORT_SYMBOL(ath12k_dp_htt_logging_init);
