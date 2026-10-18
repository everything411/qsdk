// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "athdbg_wmi_recording.h"
#include "athdbg_core.h"
#include "../wmi.h"

#define WMI_REC_MAX_ENTRY 65535
#define WMI_REC_DEF_ENTRY 1024

struct wmi_file_handler {
	const char *filename;
	umode_t permissions;
	enum athdbg_wmi_request wmi_req;
};

static struct wmi_file_handler athdbg_wmi_handlers[] = {
	{ "enable", 0644, ATHDBG_WMI_REQ_ENABLE },
	{ "dump", 0200, ATHDBG_WMI_REQ_DUMP },
	{ "verbosity", 0644, ATHDBG_WMI_REQ_VERBOSITY },
	{ NULL, 0, 0 },
};

enum athdbg_wmi_request athdbg_wmi_get_req_from_name(const char *filename)
{
	int handler_idx = 0;

	if (!filename)
		return ATHDBG_WMI_REQ_INVALID;

	while (athdbg_wmi_handlers[handler_idx].filename != NULL) {
		if (!strcmp(athdbg_wmi_handlers[handler_idx].filename, filename))
			return athdbg_wmi_handlers[handler_idx].wmi_req;
		handler_idx++;
	}
	return ATHDBG_WMI_REQ_INVALID;
}
EXPORT_SYMBOL(athdbg_wmi_get_req_from_name);

static int athdbg_wmi_resize_logs(struct ath12k_wmi_pdev *wmi, u32 new_entries)
{
	struct wmi_cmd_debug *cmd_log_new = NULL;
	struct wmi_cmd_comp_debug *tx_cmp_log_new = NULL;
	struct wmi_event_debug *evt_log_new = NULL;

	struct wmi_cmd_debug *cmd_log_old;
	struct wmi_cmd_comp_debug *tx_cmp_log_old;
	struct wmi_event_debug *evt_log_old;

	size_t cmd_bytes, txcmp_bytes, evt_bytes;

	if (!wmi)
		return -EINVAL;

	if (!new_entries || new_entries > WMI_REC_MAX_ENTRY)
		return -EINVAL;

	if (check_mul_overflow(new_entries, sizeof(*cmd_log_new), &cmd_bytes) ||
	    check_mul_overflow(new_entries, sizeof(*tx_cmp_log_new), &txcmp_bytes) ||
	    check_mul_overflow(new_entries, sizeof(*evt_log_new), &evt_bytes))
		return -EINVAL;

	cmd_log_new = kzalloc(cmd_bytes, GFP_KERNEL);
	if (!cmd_log_new)
		return -ENOMEM;

	tx_cmp_log_new = kzalloc(txcmp_bytes, GFP_KERNEL);
	if (!tx_cmp_log_new)
		goto err_free_cmd;

	evt_log_new = kzalloc(evt_bytes, GFP_KERNEL);
	if (!evt_log_new)
		goto err_free_tx_cmp;

	wmi->wmi_recording_enabled = false;
	cmd_log_old    = rcu_access_pointer(wmi->wmi_cmd_log);
	tx_cmp_log_old = rcu_access_pointer(wmi->wmi_cmd_tx_cmp_log);
	evt_log_old    = rcu_access_pointer(wmi->wmi_evt_log);
	rcu_assign_pointer(wmi->wmi_cmd_log, cmd_log_new);
	rcu_assign_pointer(wmi->wmi_cmd_tx_cmp_log, tx_cmp_log_new);
	rcu_assign_pointer(wmi->wmi_evt_log, evt_log_new);

	synchronize_rcu();

	wmi->dbg_cmd_tail_idx = 0;
	wmi->dbg_cmd_tx_cmp_tail_idx = 0;
	wmi->dbg_evt_tail_idx = 0;
	wmi->wmi_cmd_log_size = new_entries;
	wmi->wmi_cmd_tx_cmp_log_size = new_entries;
	wmi->wmi_evt_log_size = new_entries;
	wmi->wmi_recording_enabled = true;

	kfree(cmd_log_old);
	kfree(tx_cmp_log_old);
	kfree(evt_log_old);
	return 0;

err_free_tx_cmp:
	kfree(tx_cmp_log_new);
err_free_cmd:
	kfree(cmd_log_new);
	return -ENOMEM;
}

static void athdbg_wmi_dump_cmd_log_print(struct ath12k_wmi_pdev *wmi)
{
	struct wmi_cmd_debug *cmd_log = rcu_dereference(wmi->wmi_cmd_log);
	u32 tail_counter = wmi->dbg_cmd_tail_idx;
	u32 capacity = wmi->wmi_cmd_log_size;
	u32 verbosity = wmi->verbosity;
	u32 i;

	if (!cmd_log || !capacity)
		return;

	pr_info("WMI CMD dump: capacity=%u verbosity=%u\n", capacity, verbosity);

	for (i = 0; i < capacity; i++) {
		u32 idx = (tail_counter + i) % capacity;
		struct wmi_cmd_debug *entry = &cmd_log[idx];
		enum wmi_tlv_cmd_id cmd_id = entry->cmdid;
		u64 timestamp = READ_ONCE(entry->time);
		u32 data0, data1, data2, data3;

		if (!timestamp && !cmd_id)
			continue;

		if (verbosity == ATHDBG_WMI_REC_VERBOSE_L0) {
			pr_info("CMD idx=%u cmdid=0x%x time=%llu\n",
				idx, cmd_id, (unsigned long long)timestamp);
		} else if (verbosity == ATHDBG_WMI_REC_VERBOSE_L1) {
			data0 = entry->data[0];
			data1 = entry->data[1];
			pr_info("CMD idx=%u cmdid=0x%x time=%llu data=%08x %08x\n",
				idx, cmd_id, (unsigned long long)timestamp, data0, data1);
		} else {
			data0 = entry->data[0];
			data1 = entry->data[1];
			data2 = entry->data[2];
			data3 = entry->data[3];
			pr_info("CMD idx=%u cmdid=0x%x time=%llu data=%08x %08x %08x %08x\n",
				idx, cmd_id, (unsigned long long)timestamp, data0, data1,
				data2, data3);
		}
	}
}

static void athdbg_wmi_dump_cmd_tx_cmp_print(struct ath12k_wmi_pdev *wmi)
{
	struct wmi_cmd_comp_debug *tx_log = rcu_dereference(wmi->wmi_cmd_tx_cmp_log);
	u32 capacity = wmi->wmi_cmd_tx_cmp_log_size;
	u32 tail_counter = wmi->dbg_cmd_tx_cmp_tail_idx;
	u32 i;

	if (!tx_log || !capacity)
		return;

	pr_info("WMI CMD TX_CMP dump: capacity=%u\n", capacity);

	for (i = 0; i < capacity; i++) {
		u32 idx = (tail_counter + i) % capacity;
		struct wmi_cmd_comp_debug *entry = &tx_log[idx];
		enum wmi_tlv_cmd_id cmd_id = entry->cmdid;
		u64 timestamp = READ_ONCE(entry->time);

		if (!timestamp && !cmd_id)
			continue;

		pr_info("TX_CMP idx=%u cmdid=0x%x time=%llu\n",
			idx, cmd_id, (unsigned long long)timestamp);
	}
}

static void athdbg_wmi_dump_evt_log_print(struct ath12k_wmi_pdev *wmi)
{
	struct wmi_event_debug *evt_log = rcu_dereference(wmi->wmi_evt_log);
	u32 tail_counter = wmi->dbg_evt_tail_idx;
	u32 capacity = wmi->wmi_evt_log_size;
	u32 verbosity = wmi->verbosity;
	u32 i;

	if (!evt_log || !capacity)
		return;

	pr_info("WMI EVT dump: capacity=%u verbosity=%u\n", capacity, verbosity);

	for (i = 0; i < capacity; i++) {
		u32 idx = (tail_counter + i) % capacity;
		struct wmi_event_debug *entry = &evt_log[idx];
		enum wmi_tlv_event_id event_id = entry->eventid;
		u64 timestamp = READ_ONCE(entry->time);
		u32 data0, data1, data2, data3;

		if (!timestamp && !event_id)
			continue;

		if (verbosity == ATHDBG_WMI_REC_VERBOSE_L0) {
			pr_info("EVT idx=%u eventid=0x%x time=%llu\n",
				idx, event_id, (unsigned long long)timestamp);
		} else if (verbosity == ATHDBG_WMI_REC_VERBOSE_L1) {
			data0 = entry->data[0];
			data1 = entry->data[1];
			pr_info("EVT idx=%u eventid=0x%x time=%llu data=%08x %08x\n",
				idx, event_id, (unsigned long long)timestamp, data0,
				data1);
		} else {
			data0 = entry->data[0];
			data1 = entry->data[1];
			data2 = entry->data[2];
			data3 = entry->data[3];
			pr_info("EVT idx=%u eventid=0x%x time=%llu data=%08x %08x %08x %08x\n",
				idx, event_id, (unsigned long long)timestamp,
				data0, data1, data2, data3);
		}
	}
}

void athdbg_process_wmi_request(struct ath12k_base *ab,
				struct athdbg_request *dbg_req)
{
	struct ath12k_wmi_pdev *wmi;
	u32 val;
	int ret;

	if (!ab || !dbg_req || !dbg_req->input_buf)
		return;

	wmi = &ab->wmi_ab.wmi[0];
	ret = kstrtou32(dbg_req->input_buf, 0, &val);
	if (ret) {
		pr_err("athdbg_wmi_rec: invalid input '%s'\n", dbg_req->input_buf);
		goto out;
	}

	switch (dbg_req->req_type) {
	case ATH_DBG_REQ_WMI_ENABLE:
		if (val == ATHDBG_WMI_REC_DISABLE) {
			wmi->wmi_recording_enabled = false;
			pr_info("WMI: recording disabled\n");
		} else {
			u32 size = val;

			if (val == ATHDBG_WMI_REC_ENABLE_DEF)
				size = WMI_REC_DEF_ENTRY;
			ret = athdbg_wmi_resize_logs(wmi, size);
			if (ret)
				pr_err("athdbg_wmi_rec: resize failed: %d\n", ret);
			else
				pr_info("WMI: enabled with size %u\n", size);
		}
		break;

	case ATH_DBG_REQ_WMI_VERBOSITY:
		if (val > ATHDBG_WMI_REC_VERBOSE_L2)
			pr_err("athdbg_wmi_rec: Invalid verbosity level: %u\n", val);
		else {
			wmi->verbosity = val;
			pr_info("WMI: verbosity set to %u\n", val);
		}
		break;

	case ATH_DBG_REQ_WMI_DUMP:
		if (!wmi->wmi_recording_enabled) {
			pr_warn("WMI: recording disabled, skipping dump\n");
			break;
		}
		if (val == ATHDBG_WMI_REC_CMD_PRINT)
			athdbg_wmi_dump_cmd_log_print(wmi);
		else if (val == ATHDBG_WMI_REC_CMD_TX_PRINT)
			athdbg_wmi_dump_evt_log_print(wmi);
		else if (val == ATHDBG_WMI_REC_EVT_PRINT)
			athdbg_wmi_dump_cmd_tx_cmp_print(wmi);
		else
			pr_err("athdbg_wmi_rec: invalid dump option %u\n", val);
		break;

	default:
		pr_err("athdbg_wmi_rec: unknown request type %u\n", dbg_req->req_type);
		break;
	}

out:
	kfree(dbg_req->input_buf);
	dbg_req->input_buf = NULL;
}
EXPORT_SYMBOL(athdbg_process_wmi_request);
