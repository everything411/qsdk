// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/inet.h>
#include "core.h"
#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "debugfs.h"
#include "debug.h"
#include "debugfs_htt_stats.h"
#include "qmi.h"
#include "wmi.h"
#include "dp_peer.h"
#include "peer.h"
#include "coredump.h"
#include "dp_mon.h"
#include "dp_mon_filter.h"
#include "dp_cmn.h"
#include "pktlog.h"
#include "dp_stats.h"
#include "mgmt_rx.h"
#include "telemetry_agent_if.h"
#include <linux/vmalloc.h>

#define SEGMENT_ID	GENMASK(1,0)
#define CHRIP_ID	BIT(2)
#define OFFSET		GENMASK(10,3)
#define DETECTOR_ID	GENMASK(12,11)
#define OFFSET_SIGN	BIT(13)
#define FHSS		BIT(14)

bool ath12k_compress_dump;

/**
 * ath12k_debugfs_open() - generic open callback for all ath12k debugfs files.
 *
 * Checks whether the device is in WSI-bypass state before allowing any file
 * operation.  On success it sets file->private_data to the original per-file
 * data pointer so that read/write handlers are unaffected.
 */
int ath12k_debugfs_open(struct inode *inode, struct file *file)
{
	struct ath12k_debugfs_ctx *ctx = inode->i_private;

	if (ctx->ab->is_bypassed) {
		ath12k_info(ctx->ab, "Device is in Bypass state, skipping cmd\n");
		return -EPERM;
	}

	if (test_bit(ATH12K_FLAG_RECOVERY, &ctx->ab->dev_flags)) {
		ath12k_info(ctx->ab, "Device is in recovery, skipping cmd\n");
		return -EBUSY;
	}

	file->private_data = ctx->data;
	return 0;
}
EXPORT_SYMBOL(ath12k_debugfs_open);

/**
 * ath12k_debugfs_create_file() - wrapper around debugfs_create_file() that
 *                                installs ath12k_debugfs_open() as the open
 *                                callback for every file.
 *
 * @name:   file name
 * @mode:   file permission bits
 * @parent: parent dentry
 * @ab:     base device pointer (used for the bypass pre-check)
 * @data:   per-file private data forwarded to read/write handlers
 * @fops:   file operations (must have .open == ath12k_debugfs_open)
 *
 * The context struct is allocated with devm_kzalloc() so it is freed
 * automatically when the device is torn down.
 */
struct dentry *
ath12k_debugfs_create_file(const char *name, umode_t mode,
			   struct dentry *parent,
			   struct ath12k_base *ab, void *data,
			   const struct file_operations *fops)
{
	struct ath12k_debugfs_ctx *ctx;

	ctx = devm_kzalloc(ab->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->ab   = ab;
	ctx->data = data;

	return debugfs_create_file(name, mode, parent, ctx, fops);
}
EXPORT_SYMBOL(ath12k_debugfs_create_file);

static ssize_t ath12k_read_sensitivity_level(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int len = 0;
	char buf[16];

	len = scnprintf(buf, sizeof(buf) - len, "%d\n", ar->sensitivity_level);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

#define ATH12K_SENS_LEVEL_MAX    -10
#define ATH12K_SENS_LEVEL_MIN    -95

static ssize_t ath12k_write_sensitivity_level(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	s32 level;

	if (kstrtos32_from_user(user_buf, count, 10, &level))
		return -EINVAL;

	if (level > ATH12K_SENS_LEVEL_MAX || level < ATH12K_SENS_LEVEL_MIN) {
		ath12k_warn(ar->ab, "invalid sensitivity level: %d (valid range: [%d, %d])\n",
			    level, ATH12K_SENS_LEVEL_MIN, ATH12K_SENS_LEVEL_MAX);
		return -EINVAL;
	}

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (ar->sensitivity_level == level) {
		ret = count;
		goto exit;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SENSITIVITY_LEVEL,
					level, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set sensitivity level: %d\n", ret);
		goto exit;
	}

	ar->sensitivity_level = level;
	ret = count;

exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_sensitivity_level = {
	.read = ath12k_read_sensitivity_level,
	.write = ath12k_write_sensitivity_level,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t
ath12k_debug_write_wsi_bypass_device(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	unsigned int value;
	u64 peer_timeout;
	int ret, i;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *partner_ab;
	char buf[128] = {0};

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);

	if (ret <= 0)
		goto exit;

	ret = sscanf(buf, "%u %llu", &value, &peer_timeout);

	if (ret < 1 || ret > 2) {
		return -EINVAL;
	} else if (ret == 2) {
		ath12k_info(ab, "Updating peer delete timeout to %llu", peer_timeout);
		ag->wsi_peer_clean_timeout = peer_timeout;
	} else {
		ag->wsi_peer_clean_timeout = ATH12K_MAC_PEER_CLEANUP_TIMEOUT_MSECS;
	}

	if (value > ATH12K_WSI_BYPASS_ADD_DEVICE ||
	    value <= ATH12K_WSI_BYPASS_DEFAULT) {
		ath12k_warn(ab, "Please enter: 1 = Bypass device,"
			    "2 = Re-add device\n");
		return -EINVAL;
	}

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_static_bypassed) {
			ath12k_err(ab, "Static WSI Bypass is enabled\n");
			ret = -EINVAL;
			goto exit;
		}
	}
	mutex_lock(&ab->core_lock);
	ret = ath12k_wsi_bypass_precheck(ab, value);

	if (ret) {
		ath12k_err(ab, "WSI Bypass precheck failed\n");
		mutex_unlock(&ab->core_lock);
		goto exit;
	}

	ab->wsi_remap_state = value;

	if (ab->is_bypassed && ab->wsi_remap_state == ATH12K_WSI_BYPASS_REMOVE_DEVICE) {
		ath12k_warn(ab, "Failed! Device is already in bypass state\n");
		ret = -EINVAL;
		mutex_unlock(&ab->core_lock);
		goto exit;
	}
	mutex_unlock(&ab->core_lock);

	ret = ath12k_mac_dynamic_wsi_remap(ab);

	if (ret) {
		/* Reset the flags if there is a failure */
		ath12k_err(ab, "WSI bypass has failed with error %d\n", ret);
		ab->ag->wsi_remap_in_progress = false;
		ab->wsi_remap_state = ATH12K_WSI_BYPASS_DEFAULT;
		ab->is_bypassed = false;
		goto exit;
	}

	ret = count;

exit:
	return ret;
}

static const struct file_operations fops_wsi_bypass_device = {
	.write = ath12k_debug_write_wsi_bypass_device,
	.open = ath12k_debugfs_open,
};

static int print_btcoex_stats(char *buf, int size, void *stats_ptr)
{
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats = stats_ptr;
	int len = 0;

	len += scnprintf(buf + len, size - len,
				"WMI_CTRL_PATH_BTCOEX_STATS:\n");
		len += scnprintf(buf + len, size - len,
				"pdev_id = %u\n",
				btcoex_stats->pdev_id);
		len += scnprintf(buf + len, size - len,
				"bt_tx_req_cntr = %u\n",
				btcoex_stats->bt_tx_req_cntr);
		len += scnprintf(buf + len, size - len,
				"bt_rx_req_cntr = %u\n",
				btcoex_stats->bt_rx_req_cntr);
		len += scnprintf(buf + len, size - len,
				"bt_req_nack_cntr = %u\n",
				btcoex_stats->bt_req_nack_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_schd_bt_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_schd_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_current_bt_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_current_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_other_wlan_tx_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_other_wlan_tx_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_in_tx_abort_cntr = %u\n",
				btcoex_stats->wl_in_tx_abort_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_auto_resp_req_cntr = %u\n",
				btcoex_stats->wl_tx_auto_resp_req_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_ack_cntr = %u\n",
				btcoex_stats->wl_tx_req_ack_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_cntr = %u\n",
				btcoex_stats->wl_tx_req_cntr);
	return len;
}

static int print_blanking_stats(char *buf, int size, void *stats_ptr)
{
	struct wmi_ctrl_path_blanking_stats *blanking_stats = stats_ptr;
	int len = 0;

	len += scnprintf(buf + len, size - len, "WMI_CTRL_PATH_BLANKING_STATS:\n");
	len += scnprintf(buf + len, size - len, "blanking_mode = %u\n",
			 le32_to_cpu(blanking_stats->blanking_mode));
	len += scnprintf(buf + len, size - len, "is_blanking_enabled = %u\n",
			 le32_to_cpu(blanking_stats->is_blanking_enabled));
	len += scnprintf(buf + len, size - len, "gate_2g_enabled = %u\n",
			 le32_to_cpu(blanking_stats->gate_2g_enabled));
	len += scnprintf(buf + len, size - len, "gate_5g_enabled = %u\n",
			 le32_to_cpu(blanking_stats->gate_5g_enabled));
	len += scnprintf(buf + len, size - len, "gate_6g_enabled = %u\n",
			 le32_to_cpu(blanking_stats->gate_6g_enabled));
	len += scnprintf(buf + len, size - len, "blanking_count = %u\n",
			 le32_to_cpu(blanking_stats->blanking_count));
	len += scnprintf(buf + len, size - len, "blanking_duration(us) = %u\n",
			 le32_to_cpu(blanking_stats->blanking_duration));
	return len;
}

static int print_mem_stats(char *buf, int size, void *stats_ptr)
{   struct wmi_ctrl_path_mem_stats_params *mem_stats = stats_ptr;
	int len = 0;

	len += scnprintf(buf + len, size - len, "WMI_CTRL_PATH_MEM_STATS:\n");

	if (mem_stats->total_bytes) {
		u32 arena_id = __le32_to_cpu(mem_stats->arena_id);
		len += scnprintf(buf + len, size - len,
				 "arena_id = %u\n",
				 arena_id);
		len += scnprintf(buf + len, size - len,
				 "arena = %s\n",
				 wmi_ctrl_path_fw_arena_id_to_name(arena_id));
		len += scnprintf(buf + len, size - len,
				 "total_bytes = %u\n",
				 __le32_to_cpu(mem_stats->total_bytes));
		len += scnprintf(buf + len, size - len,
				 "allocated_bytes = %u\n",
				 __le32_to_cpu(mem_stats->allocated_bytes));
	}

	return len;

}
static int print_awgn_stats(char *buf, int size, void *stats_ptr)
{
	struct wmi_ctrl_path_awgn_stats *awgn_stats = stats_ptr;
	int len = 0;

	len += scnprintf(buf + len, size - len, "WMI_CTRL_PATH_AWGN_STATS_TLV:\n");
	len += scnprintf(buf + len, size - len, "awgn_send_evt_cnt = %u\n",
			 awgn_stats->awgn_send_evt_cnt);
	len += scnprintf(buf + len, size - len, "awgn_pri_int_cnt = %u\n",
			 awgn_stats->awgn_pri_int_cnt);
	len += scnprintf(buf + len, size - len, "awgn_sec_int_cnt = %u\n",
			 awgn_stats->awgn_sec_int_cnt);
	len += scnprintf(buf + len, size - len, "awgn_pkt_drop_trigger_cnt = %u\n",
			 awgn_stats->awgn_pkt_drop_trigger_cnt);
	len += scnprintf(buf + len, size - len, "awgn_pkt_drop_trigger_reset_cnt = %u\n",
			 awgn_stats->awgn_pkt_drop_trigger_reset_cnt);
	len += scnprintf(buf + len, size - len, "awgn_bw_drop_cnt = %u\n",
			 awgn_stats->awgn_bw_drop_cnt);
	len += scnprintf(buf + len, size - len, "awgn_bw_drop_reset_cnt = %u\n",
			 awgn_stats->awgn_bw_drop_reset_cnt);
	len += scnprintf(buf + len, size - len, "awgn_cca_int_cnt = %u\n",
			 awgn_stats->awgn_cca_int_cnt);
	len += scnprintf(buf + len, size - len, "awgn_cca_int_reset_cnt = %u\n",
			 awgn_stats->awgn_cca_int_reset_cnt);
	len += scnprintf(buf + len, size - len, "awgn_cca_ack_blk_cnt = %u\n",
			 awgn_stats->awgn_cca_ack_blk_cnt);
	len += scnprintf(buf + len, size - len, "awgn_cca_ack_reset_cnt = %u\n",
			 awgn_stats->awgn_cca_ack_reset_cnt);
	len += scnprintf(buf + len, size - len, "awgn_int_bw_cnt-AWGN_20[0]: %u\n",
			 awgn_stats->awgn_int_bw_cnt[0]);
	len += scnprintf(buf + len, size - len, "AWGN_40[1]: %u\n",
			 awgn_stats->awgn_int_bw_cnt[1]);
	len += scnprintf(buf + len, size - len, "AWGN_80[2]: %u\n",
			 awgn_stats->awgn_int_bw_cnt[2]);
	len += scnprintf(buf + len, size - len, "AWGN_160[3]: %u\n",
			 awgn_stats->awgn_int_bw_cnt[3]);
	len += scnprintf(buf + len, size - len, "AWGN_320[5]: %u\n",
			 awgn_stats->awgn_int_bw_cnt[5]);

	return len;
}

static int print_cal_stats(char *buf, int size, void *stats_ptr)
{
	u8 cal_type_mask, cal_prof_mask, is_periodic_cal;
	struct wmi_ctrl_path_cal_stats *cal_stats = stats_ptr;
	int len = 0;

	len += scnprintf(buf + len, size - len, "WMI_CTRL_PATH_CAL_STATS\n");
	len += scnprintf(buf + len, size - len,
			"%-25s %-25s %-17s %-16s %-16s %-16s\n",
			"cal_profile", "cal_type",
			"cal_triggered_cnt", "cal_fail_cnt",
			"cal_fcs_cnt", "cal_fcs_fail_cnt");

	cal_prof_mask = FIELD_GET(WMI_CTRL_PATH_CAL_PROF_MASK,
				cal_stats->cal_info);
	if (cal_prof_mask == WMI_CTRL_PATH_STATS_CAL_PROFILE_INVALID)
		return len;

	cal_type_mask = FIELD_GET(WMI_CTRL_PATH_CAL_TYPE_MASK,
			cal_stats->cal_info);
	is_periodic_cal = FIELD_GET(WMI_CTRL_PATH_IS_PERIODIC_CAL,
				cal_stats->cal_info);


		if (!is_periodic_cal) {
			len += scnprintf(buf + len, size - len,
			   "%-25s %-25s %-17d %-16d %-16d %-16d\n",
			   wmi_ctrl_path_cal_prof_id_to_name(cal_prof_mask),
			   wmi_ctrl_path_cal_type_id_to_name(cal_type_mask),
			   cal_stats->cal_triggered_cnt,
			   cal_stats->cal_fail_cnt,
			   cal_stats->cal_fcs_cnt,
			   cal_stats->cal_fcs_fail_cnt);
		} else {
			len += scnprintf(buf + len, size - len,
			   "%-25s %-25s %-17d %-16d %-16d %-16d\n",
			   "PERIODIC_CAL",
			   wmi_ctrl_path_periodic_cal_type_id_to_name(cal_type_mask),
			   cal_stats->cal_triggered_cnt,
			   cal_stats->cal_fail_cnt,
			   cal_stats->cal_fcs_cnt,
			   cal_stats->cal_fcs_fail_cnt);
	}

	return len;
}

static int print_afc_stats(char *buf, int size, void *stats_ptr)
{
	struct wmi_ctrl_path_afc_stats *afc_stats = stats_ptr;
	int len = 0;

	len += scnprintf(buf + len, size - len, "WMI_CTRL_PATH_AFC_STATS_TLV:\n");
	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len, "****General AFC counters****\n");

	len += scnprintf(buf + len, size - len,
			 "Total request ID = %u\n",
			 __le32_to_cpu(afc_stats->request_id_count));

	len += scnprintf(buf + len, size - len,
			 "Total payload count = %u\n",
			 __le32_to_cpu(afc_stats->response_count));

	len += scnprintf(buf + len, size - len,
		"Total payload parser mem alloc failure count = %u\n",
		__le32_to_cpu(afc_stats->payload_parser_mem_alloc_failure_count));

	len += scnprintf(buf + len, size - len,
			 "Total invalid payload count = %u\n",
			 __le32_to_cpu(afc_stats->invalid_response_count));

	len += scnprintf(buf + len, size - len,
			 "Total AFC reset count = %u\n",
			 __le32_to_cpu(afc_stats->reset_count));

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len,
			 "****AFC Payload Response error counters****\n");

	len += scnprintf(buf + len, size - len,
			 "Payload id mismatch count = %u\n",
			 __le32_to_cpu(afc_stats->id_mismatch_count));

	len += scnprintf(buf + len, size - len,
			 "Local error code success = %u\n",
			 __le32_to_cpu(afc_stats->local_err_code_success));

	len += scnprintf(buf + len, size - len,
			 "Local error code failure = %u\n",
			 __le32_to_cpu(afc_stats->local_err_code_failure));

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len,
			 "****AFC Server Response error counters****\n");

	len += scnprintf(buf + len, size - len,
			 "Code_100 | Version not supported = %u\n",
			 __le32_to_cpu(afc_stats->serv_resp_code_100));

	len += scnprintf(buf + len, size - len,
			 "Code_101 | Device disallowed = %u\n",
			 __le32_to_cpu(afc_stats->serv_resp_code_101));

	len += scnprintf(buf + len, size - len,
			 "Code_102 | Missing Param = %u\n",
			 __le32_to_cpu(afc_stats->serv_resp_code_102));

	len += scnprintf(buf + len, size - len,
			 "Code_103 | Invalid value = %u\n",
			 __le32_to_cpu(afc_stats->serv_resp_code_103));

	len += scnprintf(buf + len, size - len,
			 "Code_106 | Unexpected param = %u\n",
			 __le32_to_cpu(afc_stats->serv_resp_code_106));

	len += scnprintf(buf + len, size - len,
			 "Code_300 | Unsupported spectrum = %u\n",
			 __le32_to_cpu(afc_stats->serv_resp_code_300));

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len,
			 "****AFC Compliance tracker****\n");

	len += scnprintf(buf + len, size - len,
			 "Proxy_standalone 0 = %u\n",
			 __le32_to_cpu(afc_stats->proxy_standalone_0));

	len += scnprintf(buf + len, size - len,
			 "Proxy_standalone 1 = %u\n",
			 __le32_to_cpu(afc_stats->proxy_standalone_1));

	len += scnprintf(buf + len, size - len,
			 "Successful power event sent count = %u\n",
			 __le32_to_cpu(afc_stats->power_event_counter));

	len += scnprintf(buf + len, size - len,
			 "Force LPI switch count = %u\n",
			 __le32_to_cpu(afc_stats->force_LPI_counter));

	len += scnprintf(buf + len, size - len,
			 "TPC WMI success count = %u\n",
			 __le32_to_cpu(afc_stats->tpc_wmi_success_count));

	len += scnprintf(buf + len, size - len,
			 "TPC WMI failure count = %u\n",
			 __le32_to_cpu(afc_stats->tpc_wmi_failure_count));

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len,
			 "****AFC Regulatory Compliance check counter****\n");

	len += scnprintf(buf + len, size - len,
			 "psd failure = %u\n",
			 __le32_to_cpu(afc_stats->psd_failure_count));

	len += scnprintf(buf + len, size - len,
			 "psd end freq failure = %u\n",
			 __le32_to_cpu(afc_stats->psd_end_freq_failure_count));

	len += scnprintf(buf + len, size - len,
			 "psd start freq failure = %u\n",
			 __le32_to_cpu(afc_stats->psd_start_freq_failure_count));

	len += scnprintf(buf + len, size - len,
			 "eirp failure = %u\n",
			 __le32_to_cpu(afc_stats->eirp_failure_count));

	len += scnprintf(buf + len, size - len,
			 "centre freq failure = %u\n",
			 __le32_to_cpu(afc_stats->cfreq_failure_count));

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len,
			 "****AFC Miscellaneous stats****\n");

	len += scnprintf(buf + len, size - len,
			 "Current request ID = %u\n",
			 __le32_to_cpu(afc_stats->request_id));

	len += scnprintf(buf + len, size - len,
			 "Grace timer count = %u\n",
			 __le32_to_cpu(afc_stats->grace_timer_count));

	len += scnprintf(buf + len, size - len,
			 "Current TTL timer = %u seconds\n",
			 __le32_to_cpu(afc_stats->cur_ttl_timer));

	len += scnprintf(buf + len, size - len,
			 "Deployment mode = %u (%s)\n",
			 __le32_to_cpu(afc_stats->deployment_mode),
			 (__le32_to_cpu(afc_stats->deployment_mode) == 1) ? "indoor" :
			 (__le32_to_cpu(afc_stats->deployment_mode) == 2) ? "outdoor" :
			 "(unknown)");

	len += scnprintf(buf + len, size - len,
			 "Total AFC-Response Payload clear count = %u\n",
			 __le32_to_cpu(afc_stats->payload_clear_count));

	return len;
}

int wmi_ctrl_path_btcoex_stat(struct ath12k *ar, char __user *ubuf,
			size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats;
	const int size = 2048;
	int len = 0, ret_val;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_for_each_entry(stats, &ar->debug.wmi_ctrl_path_stats.pdev_stats, list) {
		if (!stats)
			break;

		btcoex_stats = stats->stats_ptr;

		if (!btcoex_stats)
			break;

		len += scnprintf(buf + len, size - len,
				"WMI_CTRL_PATH_BTCOEX_STATS:\n");
		len += scnprintf(buf + len, size - len,
				"pdev_id = %u\n",
				btcoex_stats->pdev_id);
		len += scnprintf(buf + len, size - len,
				"bt_tx_req_cntr = %u\n",
				btcoex_stats->bt_tx_req_cntr);
		len += scnprintf(buf + len, size - len,
				"bt_rx_req_cntr = %u\n",
				btcoex_stats->bt_rx_req_cntr);
		len += scnprintf(buf + len, size - len,
				"bt_req_nack_cntr = %u\n",
				btcoex_stats->bt_req_nack_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_schd_bt_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_schd_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_current_bt_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_current_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_other_wlan_tx_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_other_wlan_tx_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_in_tx_abort_cntr = %u\n",
				btcoex_stats->wl_in_tx_abort_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_auto_resp_req_cntr = %u\n",
				btcoex_stats->wl_tx_auto_resp_req_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_ack_cntr = %u\n",
				btcoex_stats->wl_tx_req_ack_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_cntr = %u\n",
				btcoex_stats->wl_tx_req_cntr);
	}

	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret_val;
}

static ssize_t ath12k_dump_mgmt_stats(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	int len = 0, ret, i;
	int size = (TARGET_NUM_VDEVS - 1) * 1500;
	char *buf;
	const char *mgmt_frm_type[ATH12K_STATS_MGMT_FRM_TYPE_MAX-1] = {
		"assoc_req", "assoc_resp",
		"reassoc_req", "reassoc_resp",
		"probe_req", "probe_resp",
		"timing_advertisement", "reserved",
		"beacon", "atim", "disassoc",
		"auth", "deauth", "action", "action_no_ack"};
	/*
	 * Beacon Tx is offloaded to firmware in ath12k.
	 * Skip beacon Tx stats for all vdev types and beacon Rx stats for AP.
	 * Beacon Rx stats remain valid for Mesh/STA.
	 * Skip the reserved management subtype as well for all modes.
	 */
	int beacon_frm_type = IEEE80211_STYPE_BEACON >> 4;
	int reserved_frm_type = 0x0070 >> 4; /* mgmt reserved subtype */

	if (ar->ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	spin_lock_bh(&ar->data_lock);

	list_for_each_entry (arvif, &ar->arvifs, list) {
		if (!arvif)
			break;

		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
			continue;
		mgmt_stats = &arvif->ahvif->mgmt_stats;
		len += scnprintf(buf + len, size - len, "MGMT frame stats for vdev %u :\n", arvif->vdev_id);
		len += scnprintf(buf + len, size - len, "  TX stats :\n ");
		len += scnprintf(buf + len, size - len,
				 "  Total TX Mgmt frames = %llu\n",
				 mgmt_stats->aggr_tx_mgmt_cnt);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Mgmt success count = %llu\n",
				 mgmt_stats->aggr_tx_mgmt_success_cnt);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Mgmt failure count = %llu\n",
				 mgmt_stats->aggr_tx_mgmt_fail_cnt);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Cmpl success pkts = %llu\n",
				 mgmt_stats->tx_cmpl_succ_pkts);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Cmpl success bytes = %llu\n",
				 mgmt_stats->tx_cmpl_succ_bytes);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Cmpl retry pkts = %llu\n",
				 mgmt_stats->tx_cmpl_retry_pkts);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Cmpl retry bytes = %llu\n",
				 mgmt_stats->tx_cmpl_retry_bytes);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Cmpl error pkts = %llu\n",
				 mgmt_stats->tx_cmpl_err_pkts);
		len += scnprintf(buf + len, size - len,
				 "  Total TX Cmpl error bytes = %llu\n",
				 mgmt_stats->tx_cmpl_err_bytes);
		len += scnprintf(buf + len, size - len, "  Success frames:\n");
		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX - 1; i++) {
			if (i == beacon_frm_type || i == reserved_frm_type)
				continue;
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i],
					mgmt_stats->tx_succ_cnt[i]);
		}

		len += scnprintf(buf + len, size - len, "  Failed frames:\n");

		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX - 1; i++) {
			if (i == beacon_frm_type || i == reserved_frm_type)
				continue;
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->tx_fail_cnt[i]);
		}

		len += scnprintf(buf + len, size - len, "  RX stats :\n");
		len += scnprintf(buf + len, size - len,
				"  Total RX PN error count :%llu\n",
				mgmt_stats->rx_pn_err_cnt);

		len += scnprintf(buf + len, size - len,
				 "  Total RX Mgmt frames = %llu\n",
				 mgmt_stats->aggr_rx_mgmt);
		len += scnprintf(buf + len, size - len, "  Success frames:\n");
		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX - 1; i++) {
			/* Skip reserved frame types and Beacon frames in AP-only mode. */
			if (i == reserved_frm_type ||
			    (i == beacon_frm_type &&
			     (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
			      arvif->vdev_subtype != WMI_VDEV_SUBTYPE_MESH_11S)))
				continue;
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->rx_cnt[i]);
		}

		len += scnprintf(buf + len, size - len, " Tx completion stats :\n");
		len += scnprintf(buf + len, size - len, " success completions:\n");

		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX - 1; i++) {
			if (i == beacon_frm_type || i == reserved_frm_type)
				continue;
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->tx_compl_succ[i]);
		}

		len += scnprintf(buf + len, size - len, " failure completions:\n");

		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX - 1; i++) {
			if (i == beacon_frm_type || i == reserved_frm_type)
				continue;
			len += scnprintf(buf + len, size - len, "        %s: %d\n", mgmt_frm_type[i], mgmt_stats->tx_compl_fail[i]);
		}

		len += scnprintf(buf + len, size - len, "  Link Stats :\n ");
		len += scnprintf(buf + len, size - len,
				 "  Number of connected clients = %d\n",
				 arvif->num_stations);
		len += scnprintf(buf + len, size - len,
				 " Link %u RX PN error count = %llu\n",
				 arvif->link_id, arvif->rx_pn_err_cnt);
	}

	spin_unlock_bh(&ar->data_lock);

	if (len > size)
		len = size;

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	kfree(buf);
	return ret;
}

static const struct file_operations fops_dump_mgmt_stats = {
	.read = ath12k_dump_mgmt_stats,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_dump_chanctx_switch_stats(struct file *file,
						char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_chanctx_switch_stats *stats = &ar->chanctx_switch_stats;
	int len = 0, ret;
	const int size = 1024;

	char *buf __free(kfree) = kzalloc(size, GFP_KERNEL);

	if (ar->ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ar->data_lock);

	len += scnprintf(buf + len, size - len,
			 "Channel Switch Stats:\n");
	len += scnprintf(buf + len, size - len,
			 "  total_switches          = %llu\n",
			 stats->total_switches);
	len += scnprintf(buf + len, size - len,
			 "  nl_notify_count         = %llu\n",
			 stats->nl_notify_count);
	len += scnprintf(buf + len, size - len,
			 "  last_switch_time_us     = %llu\n",
			 stats->last_switch_time_us);
	len += scnprintf(buf + len, size - len,
			 "  min_switch_time_us      = %llu\n",
			 stats->min_switch_time_us);
	len += scnprintf(buf + len, size - len,
			 "  max_switch_time_us      = %llu\n",
			 stats->max_switch_time_us);
	len += scnprintf(buf + len, size - len,
			 "  avg_switch_time_us      = %llu\n",
			 stats->avg_switch_time_us);
	len += scnprintf(buf + len, size - len,
			 "\nTiming Breakdown (last switch):\n");
	len += scnprintf(buf + len, size - len,
			 "  entry_time_us           = %llu\n",
			 stats->entry_time_us);
	len += scnprintf(buf + len, size - len,
			 "  mvr_posting_time_us     = %llu\n",
			 stats->mvr_posting_time_us);
	len += scnprintf(buf + len, size - len,
			 "  mvr_resp_time_us        = %llu\n",
			 stats->mvr_resp_time_us);
	len += scnprintf(buf + len, size - len,
			 "Error Tracking:\n");
	len += scnprintf(buf + len, size - len,
			 "  mvr_timeout_count       = %llu\n",
			 stats->mvr_timeout_count);

	spin_unlock_bh(&ar->data_lock);

	if (len > size)
		len = size;

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	return ret;
}

static ssize_t ath12k_write_chanctx_switch_stats(struct file *file,
						 const char __user *ubuf,
						 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[20] = {0};
	int ret;

	if (count > sizeof(buf))
		return -EINVAL;

	ret = copy_from_user(buf, ubuf, count);
	if (ret)
		return -EFAULT;

	/* Ensure null termination */
	buf[count] = '\0';

	if (strstr(buf, "reset")) {
		spin_lock_bh(&ar->data_lock);
		memset(&ar->chanctx_switch_stats, 0,
		       sizeof(ar->chanctx_switch_stats));
		spin_unlock_bh(&ar->data_lock);
	}

	return count;
}

static const struct file_operations fops_chanctx_switch_stats = {
	.read = ath12k_dump_chanctx_switch_stats,
	.write = ath12k_write_chanctx_switch_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_debug_get_tt_stats_configs(struct file *file,
						 char __user *user_buf,
						 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int retval, i = 0;
	size_t len = 0;
	char *buf;
	const int size = 1024;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		retval = -ENETDOWN;
		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		return retval;
	}

	len += scnprintf(buf + len, size - len,
			 "Thermal levels are %d with pout enabled : %d and tx chainmask enabled : %d\n",
			 (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map) ? ENHANCED_THERMAL_LEVELS : THERMAL_LEVELS),
			 (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION, ar->ab->wmi_ab.svc_map) ? 1 : 0),
			 (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK, ar->ab->wmi_ab.svc_map) ? 1 : 0));

	len += scnprintf(buf + len, size - len, "Thermal config\n");
	for (i = 0; i < ar->tt_current_state.therm_throt_levels; i++) {
		len += scnprintf(buf + len, size - len,
				 "level:%d low threshold: %d, high threshold: %d, dcoffpercent: %d,",
				 i, ar->tt_level_configs[i].tmplwm,
				 ar->tt_level_configs[i].tmphwm,
				 ar->tt_level_configs[i].dcoffpercent);

		if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION, ar->ab->wmi_ab.svc_map))
			len += scnprintf(buf + len, size - len, " rpout[0.25db]: %d",
					 ar->tt_level_configs[i].pout_reduction_db);

		if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK, ar->ab->wmi_ab.svc_map))
			len += scnprintf(buf + len, size - len, " tx_chainmask: %d",
					 ar->tt_level_configs[i].tx_chain_mask);
		len += scnprintf(buf + len, size - len, " dc: %d\n",
				 ar->tt_level_configs[i].duty_cycle);
	}

	len += scnprintf(buf + len, size - len, "Thermal stats\n");

	len += scnprintf(buf + len, size - len, "Current temperature: %d, Current level: %d\n",
			 ar->tt_current_state.temp,
			 ar->tt_current_state.level);

	for (i = 0; i < ar->tt_current_state.therm_throt_levels; i++) {
		len += scnprintf(buf + len, size - len,
				 "level: %d, entry count: %d, duty cycle spent: %d\n",
				 i, ar->tt_level_stats[i].level_count,
				 ar->tt_level_stats[i].dc_count);
	}
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static ssize_t ath12k_debug_write_tt_configs(struct file *file,
					     const char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	unsigned int tx_chainmask, level, tmphwm, dcoffpercent, pout_reduction_db, duty_cycle;
	int tmplwm, ret;
	char buf[128] = {0};

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);

	if (ret <= 0)
		goto out;

	ret = sscanf(buf, "%d %d %d %d %d %d %d",
		     &level, &tmplwm, &tmphwm, &dcoffpercent,
		     &pout_reduction_db, &tx_chainmask, &duty_cycle);

	if (dcoffpercent < 0 || dcoffpercent > 100) {
		ath12k_err(ar->ab, "dcoffpercent should be between 0 and 100");
		ret = -EINVAL;
		goto out;
	}

	if (duty_cycle > 100 || duty_cycle < 10) {
		ath12k_err(ar->ab, "dc should be between 10 and 100");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK, ar->ab->wmi_ab.svc_map)) {
		if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION, ar->ab->wmi_ab.svc_map)) {
			if (ret != 7) {
				ath12k_err(ar->ab,
					   "7 arguments required usage: level tmplwm tmphwm dcoffpercent pout_reduction_db tx_chainmask duty_cycle");
				ret = -EINVAL;
				goto out;
			}
		} else {
			if (ret != 6) {
				ath12k_err(ar->ab,
					   "6 arguments required usage: level tmplwm tmphwm dcoffpercent tx_chainmask duty_cycle");
				ret = -EINVAL;
				goto out;
			}
		}
	} else {
		if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION, ar->ab->wmi_ab.svc_map)) {
			if (ret != 6) {
				ath12k_err(ar->ab,
					   "6 arguments required usage: level tmplwm tmphwm dcoffpercent pout_reduction_db duty_cycle");
				ret = -EINVAL;
				goto out;
			}
		} else {
			if (ret != 5) {
				ath12k_err(ar->ab,
					   "5 arguments required usage: level tmplwm tmphwm dcoffpercent duty_cycle");
				ret = -EINVAL;
				goto out;
			}
		}
	}

	if (pout_reduction_db > 100) {
		ath12k_err(ar->ab, "pout_reduction_db should be betweem 0 and 100");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map)) {
		if (level > 4) {
			ath12k_err(ar->ab, "level should be between 0 and 4");
			ret = -EINVAL;
			goto out;
		}
	} else {
		if (level > 3) {
			ath12k_err(ar->ab, "level should be between 0 and 3");
			ret = -EINVAL;
			goto out;
		}
	}

	if (tx_chainmask > ar->cfg_tx_chainmask || (tx_chainmask & (tx_chainmask + 1)) != 0) {
		ath12k_err(ar->ab, "tx_chainmask shoulb be 1/3/7/15");
		ret = -EINVAL;
		goto out;
	}

	ath12k_update_tt_configs(ar, level, tmplwm, tmphwm,
				 dcoffpercent, pout_reduction_db, tx_chainmask, duty_cycle);

	ret = count;
out:
	return ret;
}

static const struct file_operations tt_configs = {
	.read = ath12k_debug_get_tt_stats_configs,
	.write = ath12k_debug_write_tt_configs,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_debugfs_dump_device_dp_stats(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k *ar;
	struct ath12k_device_dp_stats *device_stats = &ab->dp->device_stats;
	int len = 0, i, j, retval;
	const int size = 16384;
	int tx_enqueued[MAX_TCL_RING];
	int non_fast_rx[DP_REO_DST_RING_MAX][ATH12K_MAX_SOCS];
	static const char *rxdma_err[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX] = {
			"Overflow", "MPDU_len", "FCS", "Decrypt", "TKIP_MIC",
			"Unencrypt", "MSDU_len", "MSDU_limit", "WiFi_parse",
			"AMSDU_parse", "SA_timeout", "DA_timeout",
			"Flow_timeout", "Flush_req", "AMSDU_frag", "Multicast_echo",
			"AMSDU_addr_mismatch", "Unauthorized_WDS",
			"Groupcast_AMSDU_or_WDS", "CFP_MIC", "CFP_PN_check"};
	static const char *reo_err[HAL_REO_DEST_RING_ERROR_CODE_MAX] = {
			"Desc_addr_zero", "Desc_inval", "AMPDU_in_non_BA",
			"Non_BA_dup", "BA_dup", "Frame_2k_jump", "BAR_2k_jump",
			"Frame_OOR", "BAR_OOR", "No_BA_session",
			"Frame_SN_equal_SSN", "PN_check_fail", "2k_err",
			"PN_err", "Desc_blocked"};
	static const char *wbm_rx_drop[WBM_ERR_DROP_MAX] = {
			"SW_desc_error", "SW_desc_from_cookie_error",
			"Invalid_Peer_id_error", "Desc_parse_error",
			"Invalid_Cookie", "Invalid_Push_reason",
			"Invalid_hw_id", "Null_Partner_dp", "Process_Null_Partner_dp",
			"Null_Pdev", "Null_ar", "CAC_Running", "Scatter_Gather",
			"Invalid_NWifi_Hdr_len", "REO_Generic", "RXDMA_Generic"};

	static const char *wbm_rel_src[HAL_WBM_REL_SRC_MODULE_MAX] = {
                        "TQM", "Rxdma", "Reo", "FW", "SW" };

	struct ath12k_pdev *pdev;
	struct ath12k_pdev_dp *dp_pdev;
	char *buf;
	u32 center_freq = 0;
	int tx_desc_in_use = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < MAX_TCL_RING; i++)
		tx_enqueued[i] = device_stats->tx_mcast[i] + device_stats->tx_unicast[i] +
				 device_stats->tx_eapol[i] +
				 device_stats->tx_null_frame[i] +
				 device_stats->tx_fast_unicast[i];

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		for (j = 0; j < ab->ag->num_devices; j++)
			non_fast_rx[i][j] = device_stats->non_fast_unicast_rx[i][j] +
				            device_stats->non_fast_mcast_rx[i][j];
	}

	len += scnprintf(buf + len, size - len,
			 "SOC_DP_STATS:\ntimestamp(ms)= %llu\n",
			 ktime_to_ms(ktime_get()));

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar) {
			spin_lock_bh(&ar->data_lock);
			if (ar->rx_channel)
				center_freq = ar->rx_channel->center_freq;
			spin_unlock_bh(&ar->data_lock);
			len += scnprintf(buf + len, size - len,
					 "\nradio_%u_centre_freq= %u\n",
					 i, center_freq);
		}
	}

	len += scnprintf(buf + len, size - len, "\nSOC_TX_STATS:\n");

	len += scnprintf(buf + len, size - len, "tx_enqueued=");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len, " %d:%u", j, tx_enqueued[j]);
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\ntx_wbm_rel_source:\n");
	for (j=0; j < MAX_TX_COMP_RING; j++)
		len += scnprintf(buf + len, size - len,
				 "Ring%d= 0:%u 1:%u 2:%u 3:%u 4:%u\n", j,
				 device_stats->tx_comp_stats[j].tx_wbm_rel_source[0],
				 device_stats->tx_comp_stats[j].tx_wbm_rel_source[1],
				 device_stats->tx_comp_stats[j].tx_wbm_rel_source[2],
				 device_stats->tx_comp_stats[j].tx_wbm_rel_source[3],
				 device_stats->tx_comp_stats[j].tx_wbm_rel_source[4]);

	len += scnprintf(buf + len, size - len, "\ntx_multicast=");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len, " %d:%u",
				 j, device_stats->tx_mcast[j]);
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\ntx_unicast=");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len, " %d:%u",
				 j, device_stats->tx_unicast[j]);
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\ntx_eapol=");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len, " %d:%u",
				 j, device_stats->tx_eapol[j]);
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\ntx_eapol_M1= ");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len,
				"%u ", device_stats->tx_eapol_type[0][j]);

	len += scnprintf(buf + len, size - len, "\ntx_eapol_M2= ");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len,
				 "%u ", device_stats->tx_eapol_type[1][j]);

	len += scnprintf(buf + len, size - len, "\ntx_eapol_M3= ");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len,
				 "%u ", device_stats->tx_eapol_type[2][j]);

	len += scnprintf(buf + len, size - len, "\ntx_eapol_M4= ");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len,
				 "%u ", device_stats->tx_eapol_type[3][j]);

	len += scnprintf(buf + len, size - len, "\ntx_eapol_G1= ");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len,
				"%u ", device_stats->tx_eapol_type[4][j]);

	len += scnprintf(buf + len, size - len, "\ntx_eapol_G2= ");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len,
				"%u ", device_stats->tx_eapol_type[5][j]);

	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\ntx_null_frame=");
	for (j = 0; j < MAX_TCL_RING; j++)
		len += scnprintf(buf + len, size - len, " %d:%u",
				 j, device_stats->tx_null_frame[j]);
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\ntqm_rel_reason:\n");

	for (j = 0; j < MAX_TX_COMP_RING; j++) {
		len += scnprintf(buf + len, size - len, "Ring%d=", j);
		for (i = 0; i < MAX_TQM_RELEASE_REASON; i++) {
			len += scnprintf(buf + len, size - len, " %u:%u", i,
				device_stats->tx_comp_stats[j].tqm_rel_reason[i]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}

	len += scnprintf(buf + len, size - len, "\nfw_tx_status:\n");
	for (j=0; j < MAX_TX_COMP_RING; j++)
		len += scnprintf(buf + len, size - len,
			"Ring%d= 0:%u 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u\n", j,
			device_stats->tx_comp_stats[j].fw_tx_status[0],
			device_stats->tx_comp_stats[j].fw_tx_status[1],
			device_stats->tx_comp_stats[j].fw_tx_status[2],
			device_stats->tx_comp_stats[j].fw_tx_status[3],
			device_stats->tx_comp_stats[j].fw_tx_status[4],
			device_stats->tx_comp_stats[j].fw_tx_status[5],
			device_stats->tx_comp_stats[j].fw_tx_status[6]);

	len += scnprintf(buf + len, size - len, "\ntx_completed=");
	for (j = 0; j < MAX_TX_COMP_RING; j++)
		len += scnprintf(buf + len, size - len, " %d:%u",
				 j, device_stats->tx_comp_stats[j].tx_completed);
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nTCL_Ring_Full_Failures:\n");

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		len += scnprintf(buf + len, size - len, "ring%d= %u\n",
				 i, device_stats->tx_err.desc_na[i]);

	len += scnprintf(buf + len, size - len, "\nTCL_Ring_Buffer_Alloc_Failures:\n");
	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		len += scnprintf(buf + len, size - len, "ring%d= %u\n",
			 i, device_stats->tx_err.txbuf_na[i]);

	len += scnprintf(buf + len, size - len,
			"\nTransmit_Threshold_limit= %d\n",
			device_stats->tx_err.threshold_limit);

	len += scnprintf(buf + len, size - len,
			 "\nMisc_Transmit_Failures= %d\n",
			 atomic_read(&device_stats->tx_err.misc_fail));

	len += scnprintf(buf + len, size - len, "\nFast_xmit_Tx_unicast_stats:\n");
	for (i = 0; i < MAX_TCL_RING; i++)
		len += scnprintf(buf + len, size - len, "ring%d= %u\n",
				 i, device_stats->tx_fast_unicast[i]);

	len += scnprintf(buf + len, size - len, "\nSOC_RX_STATS:\n");
	len += scnprintf(buf + len, size - len, "err_ring_pkts= %u\n",
			 device_stats->err_ring_pkts);
	len += scnprintf(buf + len, size - len, "Invalid_RBM= %u\n",
			 device_stats->invalid_rbm);
	len += scnprintf(buf + len, size - len, "free_excess_alloc_skb= %u\n\n",
			 device_stats->free_excess_alloc_skb);
	len += scnprintf(buf + len, size - len, "RXDMA_errors:\n");
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s= %u\n",
				 rxdma_err[i], device_stats->wbm_err.rxdma_error[i]);

	len += scnprintf(buf + len, size - len, "\nREO_errors:\n");
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s= %u\n",
				 reo_err[i], device_stats->wbm_err.reo_error[i]);

	len += scnprintf(buf + len, size - len, "\nWBM_Rx_Drop_Count:\n");
	for (i = 0; i < WBM_ERR_DROP_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s= %u\n",
				 wbm_rx_drop[i], device_stats->wbm_err.drop[i]);

	len += scnprintf(buf + len, size - len, "\nHAL_REO_routes: %u\n",
			 device_stats->wbm_err.hal_reo_route);

	len += scnprintf(buf + len, size - len, "\nREO_Rx_Received:");
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		len += scnprintf(buf + len, size - len, "\nRing%d= ", i + 1);
		for (j = 0; j < ab->ag->num_devices; j++)
			len += scnprintf(buf + len, size - len,
					 "%d:%u\t", j,
					 device_stats->reo_rx[i][j]);
	}

	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nREO_Fast_Rx:");
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		len += scnprintf(buf + len, size - len, "\nRing%d= ", i + 1);
		for (j = 0; j < ab->ag->num_devices; j++)
			len += scnprintf(buf + len, size - len,
					 "%d:%u\t", j,
					 device_stats->fast_rx[i][j]);
	}

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len, "\nREO_Non-Fast_Rx:");
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		len += scnprintf(buf + len, size - len, "\nRing%d= ", i + 1);
		for (j = 0; j < ab->ag->num_devices; j++)
			len += scnprintf(buf + len, size - len,
					 "%d:%u\t", j,
					 non_fast_rx[i][j]);
	}

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len, "\nMcast_Non-Fast_Rx:");
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		len += scnprintf(buf + len, size - len, "\nRing%d= ", i + 1);
		for (j = 0; j < ab->ag->num_devices; j++)
			len += scnprintf(buf + len, size - len,
					 "%d:%u\t", j,
					 device_stats->non_fast_mcast_rx[i][j]);
	}

	len += scnprintf(buf + len, size - len, "\n");
	len += scnprintf(buf + len, size - len, "\nUnicast_Non-Fast_Rx:");
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		len += scnprintf(buf + len, size - len, "\nRing%d= ", i + 1);
		for (j = 0; j < ab->ag->num_devices; j++)
			len += scnprintf(buf + len, size - len,
					 "%d:%u\t", j,
					 device_stats->non_fast_unicast_rx[i][j]);
	}

	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nRx_WBM_Rel_Eapol= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
				 device_stats->rx_eapol[i]);

	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nRx_eapol_M1= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
				 device_stats->rx_eapol_type[0][i]);

	len += scnprintf(buf + len, size - len, "\nRx_eapol_M2= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
				 device_stats->rx_eapol_type[1][i]);

	len += scnprintf(buf + len, size - len, "\nRx_eapol_M3= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
				 device_stats->rx_eapol_type[2][i]);

	len += scnprintf(buf + len, size - len, "\nRx_eapol_M4= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
                                 device_stats->rx_eapol_type[3][i]);

	len += scnprintf(buf + len, size - len, "\nRx_eapol_G1= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
				 device_stats->rx_eapol_type[4][i]);

	len += scnprintf(buf + len, size - len, "\nRx_eapol_G2= ");
	for (i = 0; i < ab->ag->num_devices; i++)
		len += scnprintf(buf + len, size - len, "%d:%u\t", i,
				 device_stats->rx_eapol_type[5][i]);

	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nNull_frame_Rx= %u\nRx_dropped= %u\n",
			 device_stats->rx_pkt_null_frame_handled,
			 device_stats->rx_pkt_null_frame_dropped);

	len += scnprintf(buf + len, size - len, "\nRx_WBM_REL_SRC_Errors:");
	for (i = 0; i < HAL_WBM_REL_SRC_MODULE_MAX; i++) {
		len += scnprintf(buf + len, size - len, "\n%s=\t", wbm_rel_src[i]);
		for (j = 0; j < ab->ag->num_devices; j++)
			len += scnprintf(buf + len, size - len,
					 "%d:%u\t", j,
					 device_stats->rx_wbm_rel_source[i][j]);
	}

	rcu_read_lock();
	if (ab->dp) {
		for (i = 0; i < MAX_RADIOS; i++) {
			dp_pdev = rcu_dereference(ab->dp->dp_pdevs[i]);
			if (dp_pdev)
				tx_desc_in_use +=
					atomic_read(&dp_pdev->num_tx_pending);
		}
	}
	rcu_read_unlock();
	len += scnprintf(buf + len, size - len,
			 "\nTx_desc_in_use= %d\n", tx_desc_in_use);

	if (ab->dp->arch_ops->dump_device_dp_stats)
		len += ab->dp->arch_ops->dump_device_dp_stats(ab->dp, buf + len,
							      size - len);

	if (len > size)
		len = size;
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static ssize_t
ath12k_debugfs_write_device_dp_stats(struct file *file,
                                 const char __user *user_buf,
                                 size_t count, loff_t *ppos)
{
       struct ath12k_base *ab = file->private_data;
       struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
       struct ath12k_device_dp_stats *device_stats = &dp->device_stats;
       char buf[20] = {0};
       int ret;

       if (count > 20)
               return -EFAULT;

       ret = copy_from_user(buf, user_buf, count);
       if (ret)
               return -EFAULT;

       if (strstr(buf, "reset"))
               memset(device_stats, 0, sizeof(struct ath12k_device_dp_stats));

	if (dp->arch_ops->reset_device_dp_stats)
		dp->arch_ops->reset_device_dp_stats(dp);

       return count;
}

static const struct file_operations fops_device_dp_stats = {
	.read = ath12k_debugfs_dump_device_dp_stats,
	.write = ath12k_debugfs_write_device_dp_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_block_radar(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	bool block_radar;
	u8 usenol = 1;
	int ret;

	if (kstrtobool_from_user(user_buf, count, &block_radar))
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (ar->dfs_block_radar_events == block_radar) {
		ret = count;
		goto exit;
	}

	if (block_radar == 1)
		usenol = 0;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_USE_NOL,
					usenol, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set usenol: %d\n", ret);
		goto exit;
	}

	ar->dfs_block_radar_events = block_radar;
	ret = count;

exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static inline bool ath12k_is_320mhz_offset_invalid(s32 freq_offset)
{
	return ((freq_offset < -ATH12K_CHWIDTH_160) ||
		(freq_offset > ATH12K_CHWIDTH_80));
}

static inline bool ath12k_is_below320mhz_offset_invalid(s32 freq_offset,
							u16 ch_width)
{
	u16 half_bw = ch_width / 2;

	return ((freq_offset < -half_bw) || (freq_offset > half_bw));
}

static bool ath12k_is_target_freq_non_dfs(struct ath12k_base *ab,
					  struct wiphy *wiphy,
					  u32 center_freq1, s32 freq_offset)
{
	u32 target_freq = center_freq1 + freq_offset;
	/* freq_offset may land between 20 MHz channel boundaries; round to the
	 * nearest channel center so ieee80211_get_channel() finds a valid entry.
	 */
	u32 chan_center = DIV_ROUND_CLOSEST(target_freq, 20) * 20;
	struct ieee80211_channel *chan = ieee80211_get_channel(wiphy, chan_center);

	if (!chan || !(chan->flags & IEEE80211_CHAN_RADAR)) {
		ath12k_warn(ab,
			    "dfs_simulate_radar: %u MHz is not DFS-required, ignoring\n",
			    target_freq);
		return true;
	}

	return false;
}

static bool ath12k_is_offset_invalid_for_bw(struct ath12k *ar,
					    s32 freq_offset, bool agile)
{
	struct ieee80211_chanctx_conf *ctx = NULL;
	struct ath12k_link_vif *arvif;
	bool sta_dfs_en = ath12k_ar_to_hw(ar)->wiphy->sta_dfs_en;
	u16 center_freq1;
	u16 ch_width = 0;

	if (agile) {
		ch_width = ath12k_mac_get_chan_width(ar->agile_chandef.width);
		center_freq1 = ar->agile_chandef.center_freq1;
	} else {
		list_for_each_entry(arvif, &ar->arvifs, list) {
			u32 vdev_type;

			if (!arvif->chanctx.def.chan)
				continue;

			vdev_type = arvif->ahvif->vdev_type;
			if (vdev_type != WMI_VDEV_TYPE_AP &&
			    vdev_type != WMI_VDEV_TYPE_STA)
				continue;

			if (vdev_type == WMI_VDEV_TYPE_STA && !sta_dfs_en)
				continue;

			ctx = &arvif->chanctx;
			break;
		}

		if (!ctx) {
			ath12k_warn(ar->ab, "ctx is NULL");
			return false;
		}

		ch_width = ath12k_mac_get_chan_width(ctx->def.width);
		center_freq1 = ctx->def.center_freq1;
	}

	if (ch_width != ATH12K_CHWIDTH_320 &&
	    ath12k_is_target_freq_non_dfs(ar->ab, ath12k_ar_to_hw(ar)->wiphy,
					  center_freq1, freq_offset))
		return true;

	switch (ch_width) {
	case ATH12K_CHWIDTH_320:
		return ath12k_is_320mhz_offset_invalid(freq_offset);
	case ATH12K_CHWIDTH_160:
	case ATH12K_CHWIDTH_80:
	case ATH12K_CHWIDTH_40:
	case ATH12K_CHWIDTH_20:
		return ath12k_is_below320mhz_offset_invalid(freq_offset,
				ch_width);
	default:
		ath12k_warn(ar->ab, "Invalid channel width: %d", ch_width);
		return true;
	}
}

static ssize_t ath12k_write_simulate_radar(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	u8 agile = 0, segment = 0, radar_type = 0, chirp = 0, fhss = 0;
	struct ath12k *ar = file->private_data;
	bool is_fw_bangradar_320_supp = false;
	char buf[64] = {0}, *token, *sptr;
	u32 radar_params;
	int offset = 0;
	int ret;
	int len;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len)) {
		return -EFAULT;
	}

	/* For backward compatibility */
	if (len <= 2)
		goto send_cmd;

	buf[len] = '\0';
	sptr = buf;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &segment))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &radar_type))
		return -EINVAL;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtoint(token, 10, &offset))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &agile))
		return -EINVAL;

	if ((segment > 1) || (radar_type > 2) || (agile > 2))
		return -EINVAL;

	if (agile && !ar->agile_chandef.chan)
		return -EINVAL;

	if (ath12k_is_offset_invalid_for_bw(ar, offset, (bool)agile))
		return -EINVAL;

send_cmd:
	/* radar_type 1 is for chirp, radar_type 2 is for FHSS */
	if (radar_type == 1)
		chirp = 1;
	if (radar_type == 2)
		fhss = 1;

	if (test_bit(WMI_TLV_SERVICE_BANG_RADAR_320_SUPPORT,
		     ar->ab->wmi_ab.svc_map))
		is_fw_bangradar_320_supp = true;

	radar_params = u32_encode_bits(segment, SEGMENT_ID) |
		       u32_encode_bits(chirp, CHRIP_ID) |
		       u32_encode_bits(agile, DETECTOR_ID) |
		       u32_encode_bits(fhss, FHSS) |
		       u32_encode_bits(offset, OFFSET);
	/*
	 * The 320 MHz offset ranges from -160 to 80.
	 * Only the negative offsets needs sign bit indication
	 */
	if (is_fw_bangradar_320_supp && offset < 0)
		radar_params |= OFFSET_SIGN;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	ret = ath12k_wmi_simulate_radar(ar, radar_params);
	if (ret)
		goto exit;

	ret = count;
exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_simulate_radar = {
	.write = ath12k_write_simulate_radar,
	.open = ath12k_debugfs_open
};

static const struct file_operations fops_dfs_block_radar = {
	.write = ath12k_write_block_radar,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_write_tpc_stats_type(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u8 type;
	int ret;

	ret = kstrtou8_from_user(user_buf, count, 0, &type);
	if (ret)
		return ret;

	if (type >= WMI_HALPHY_PDEV_TX_STATS_MAX)
		return -EINVAL;

	spin_lock_bh(&ar->data_lock);
	ar->debug.tpc_stats_type = type;
	spin_unlock_bh(&ar->data_lock);

	return count;
}

static int ath12k_debug_tpc_stats_request(struct ath12k *ar)
{
	enum wmi_halphy_ctrl_path_stats_id tpc_stats_sub_id;
	struct ath12k_base *ab = ar->ab;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->debug.tpc_complete);

	spin_lock_bh(&ar->data_lock);
	ar->debug.tpc_request = true;
	tpc_stats_sub_id = ar->debug.tpc_stats_type;
	spin_unlock_bh(&ar->data_lock);

	ret = ath12k_wmi_send_tpc_stats_request(ar, tpc_stats_sub_id);
	if (ret) {
		ath12k_warn(ab, "failed to request pdev tpc stats: %d\n", ret);
		spin_lock_bh(&ar->data_lock);
		ar->debug.tpc_request = false;
		spin_unlock_bh(&ar->data_lock);
		return ret;
	}

	return 0;
}

static int ath12k_get_tpc_ctl_mode_idx(struct wmi_tpc_stats_arg *tpc_stats,
				       enum wmi_tpc_pream_bw pream_bw, int *mode_idx)
{
	u32 chan_freq = le32_to_cpu(tpc_stats->tpc_config.chan_freq);
	u8 band;

	band = ((chan_freq > ATH12K_MIN_6GHZ_FREQ) ? NL80211_BAND_6GHZ :
		((chan_freq > ATH12K_MIN_5GHZ_FREQ) ? NL80211_BAND_5GHZ :
		NL80211_BAND_2GHZ));

	if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
		switch (pream_bw) {
		case WMI_TPC_PREAM_HT20:
		case WMI_TPC_PREAM_VHT20:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT_VHT20_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_HE20:
		case WMI_TPC_PREAM_EHT20:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT20_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_HT40:
		case WMI_TPC_PREAM_VHT40:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT_VHT40_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_HE40:
		case WMI_TPC_PREAM_EHT40:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT40_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_VHT80:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_VHT80_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_EHT60:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT80_SU_PUNC20;
			break;
		case WMI_TPC_PREAM_HE80:
		case WMI_TPC_PREAM_EHT80:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT80_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_VHT160:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_VHT160_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_EHT120:
		case WMI_TPC_PREAM_EHT140:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT160_SU_PUNC20;
			break;
		case WMI_TPC_PREAM_HE160:
		case WMI_TPC_PREAM_EHT160:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT160_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_EHT200:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC120;
			break;
		case WMI_TPC_PREAM_EHT240:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC80;
			break;
		case WMI_TPC_PREAM_EHT280:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC40;
			break;
		case WMI_TPC_PREAM_EHT320:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT320_5GHZ_6GHZ;
			break;
		/* TODO: UHR has distinct CTL powers from EHT at the HALPHY level.
		 * New CTL mode indices for UHR (UHR20/40/80/.../320 and their
		 * punctured variants) must be defined once FW confirms the index
		 * values it uses when populating the CTL table.  Until then UHR
		 * preambles default to the default LEGACY_5GHZ_6GHZ which
		 * is conservative but not accurate.
		 */
		default:
			/* for 5GHZ and 6GHZ, default case will be for OFDM */
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_LEGACY_5GHZ_6GHZ;
			break;
		}
	} else {
		switch (pream_bw) {
		case WMI_TPC_PREAM_OFDM:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_LEGACY_2GHZ;
			break;
		case WMI_TPC_PREAM_HT20:
		case WMI_TPC_PREAM_VHT20:
		case WMI_TPC_PREAM_HE20:
		case WMI_TPC_PREAM_EHT20:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT20_2GHZ;
			break;
		case WMI_TPC_PREAM_HT40:
		case WMI_TPC_PREAM_VHT40:
		case WMI_TPC_PREAM_HE40:
		case WMI_TPC_PREAM_EHT40:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT40_2GHZ;
			break;
		default:
			/* for 2GHZ, default case will be CCK */
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_CCK_2GHZ;
			break;
		}
	}

	return 0;
}

static s16 ath12k_tpc_get_rate(struct ath12k *ar,
			       struct wmi_tpc_stats_arg *tpc_stats,
			       u32 rate_idx, u32 num_chains, u32 rate_code,
			       enum wmi_tpc_pream_bw pream_bw,
			       enum wmi_halphy_ctrl_path_stats_id type,
			       u32 eht_rate_idx)
{
	u32 tot_nss, tot_modes, txbf_on_off, index_offset1, index_offset2, index_offset3;
	u8 chain_idx, stm_idx, num_streams;
	bool is_mu, txbf_enabled = 0;
	s8 rates_ctl_min, tpc_ctl;
	s16 rates, tpc, reg_pwr;
	u16 rate1, rate2;
	int mode, ret;

	num_streams = 1 + ATH12K_HW_NSS(rate_code);
	chain_idx = num_chains - 1;
	stm_idx = num_streams - 1;
	mode = -1;

	ret = ath12k_get_tpc_ctl_mode_idx(tpc_stats, pream_bw, &mode);
	if (ret) {
		ath12k_warn(ar->ab, "Invalid mode index received\n");
		tpc = TPC_INVAL;
		goto out;
	}

	if (num_chains < num_streams) {
		tpc = TPC_INVAL;
		goto out;
	}

	if (le32_to_cpu(tpc_stats->tpc_config.num_tx_chain) <= 1) {
		tpc = TPC_INVAL;
		goto out;
	}

	if (type == WMI_HALPHY_PDEV_TX_SUTXBF_STATS ||
	    type == WMI_HALPHY_PDEV_TX_MUTXBF_STATS)
		txbf_enabled = 1;

	if (type == WMI_HALPHY_PDEV_TX_MU_STATS ||
	    type == WMI_HALPHY_PDEV_TX_MUTXBF_STATS) {
		is_mu = true;
	} else {
		is_mu = false;
	}

	/* Below is the min calculation of ctl array, rates array and
	 * regulator power table. tpc is minimum of all 3.
	 * EHT and UHR share rates_array2 — FW sends one copy of MCS0-15
	 * target powers for both preamble types.
	 */
	if ((pream_bw >= WMI_TPC_PREAM_EHT20 && pream_bw <= WMI_TPC_PREAM_EHT320) ||
	    (pream_bw >= WMI_TPC_PREAM_UHR20 && pream_bw <= WMI_TPC_PREAM_UHR320)) {
		rate2 = tpc_stats->rates_array2.rate_array[eht_rate_idx];
		if (is_mu)
			rates = u32_get_bits(rate2, ATH12K_TPC_RATE_ARRAY_MU);
		else
			rates = u32_get_bits(rate2, ATH12K_TPC_RATE_ARRAY_SU);
	} else {
		rate1 = tpc_stats->rates_array1.rate_array[rate_idx];
		if (is_mu)
			rates = u32_get_bits(rate1, ATH12K_TPC_RATE_ARRAY_MU);
		else
			rates = u32_get_bits(rate1, ATH12K_TPC_RATE_ARRAY_SU);
	}

	if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_ARRAY) {
		tot_nss = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.d1);
		tot_modes = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.d2);
		txbf_on_off = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.d3);
		index_offset1 = txbf_on_off * tot_modes * tot_nss;
		index_offset2 = tot_modes * tot_nss;
		index_offset3 = tot_nss;

		tpc_ctl = *(tpc_stats->ctl_array.ctl_pwr_table +
			    chain_idx * index_offset1 + txbf_enabled * index_offset2
			    + mode * index_offset3 + stm_idx);
	} else {
		tpc_ctl = TPC_MAX;
		ath12k_warn(ar->ab,
			    "ctl array for tpc stats not received from fw\n");
	}

	rates_ctl_min = min_t(s16, rates, tpc_ctl);

	reg_pwr = tpc_stats->max_reg_allowed_power.reg_pwr_array[chain_idx];

	if (reg_pwr < 0)
		reg_pwr = TPC_INVAL;

	tpc = min_t(s16, rates_ctl_min, reg_pwr);

	/* MODULATION_LIMIT is the maximum power limit,tpc should not exceed
	 * modulation limit even if min tpc of all three array is greater
	 * modulation limit
	 */
	tpc = min_t(s16, tpc, MODULATION_LIMIT);

out:
	return tpc;
}

static u16 ath12k_get_ratecode(u16 pream_idx, u16 nss, u16 mcs_rate)
{
	u16 mode_type = ~0;

	/* Below assignments are just for printing purpose only */
	switch (pream_idx) {
	case WMI_TPC_PREAM_CCK:
		mode_type = WMI_RATE_PREAMBLE_CCK;
		break;
	case WMI_TPC_PREAM_OFDM:
		mode_type = WMI_RATE_PREAMBLE_OFDM;
		break;
	case WMI_TPC_PREAM_HT20:
	case WMI_TPC_PREAM_HT40:
		mode_type = WMI_RATE_PREAMBLE_HT;
		break;
	case WMI_TPC_PREAM_VHT20:
	case WMI_TPC_PREAM_VHT40:
	case WMI_TPC_PREAM_VHT80:
	case WMI_TPC_PREAM_VHT160:
		mode_type = WMI_RATE_PREAMBLE_VHT;
		break;
	case WMI_TPC_PREAM_HE20:
	case WMI_TPC_PREAM_HE40:
	case WMI_TPC_PREAM_HE80:
	case WMI_TPC_PREAM_HE160:
		mode_type = WMI_RATE_PREAMBLE_HE;
		break;
	case WMI_TPC_PREAM_EHT20:
	case WMI_TPC_PREAM_EHT40:
	case WMI_TPC_PREAM_EHT60:
	case WMI_TPC_PREAM_EHT80:
	case WMI_TPC_PREAM_EHT120:
	case WMI_TPC_PREAM_EHT140:
	case WMI_TPC_PREAM_EHT160:
	case WMI_TPC_PREAM_EHT200:
	case WMI_TPC_PREAM_EHT240:
	case WMI_TPC_PREAM_EHT280:
	case WMI_TPC_PREAM_EHT320:
		mode_type = WMI_RATE_PREAMBLE_EHT;
		if (mcs_rate == 0 || mcs_rate == 1)
			mcs_rate += 14;
		else
			mcs_rate -= 2;
		break;
	case WMI_TPC_PREAM_UHR20:
	case WMI_TPC_PREAM_UHR40:
	case WMI_TPC_PREAM_UHR60:
	case WMI_TPC_PREAM_UHR80:
	case WMI_TPC_PREAM_UHR120:
	case WMI_TPC_PREAM_UHR140:
	case WMI_TPC_PREAM_UHR160:
	case WMI_TPC_PREAM_UHR200:
	case WMI_TPC_PREAM_UHR240:
	case WMI_TPC_PREAM_UHR280:
	case WMI_TPC_PREAM_UHR320:
		mode_type = WMI_RATE_PREAMBLE_UHR;
		if (mcs_rate == 0 || mcs_rate == 1)
			mcs_rate += 14;
		else
			mcs_rate -= 2;
		break;
	default:
		return mode_type;
	}
	return ((mode_type << 8) | ((nss & 0x7) << 5) | (mcs_rate & 0x1F));
}

static bool ath12k_he_supports_extra_mcs(struct ath12k *ar, int freq)
{
	struct ath12k_pdev_cap *cap = &ar->pdev->cap;
	struct ath12k_band_cap *cap_band;
	bool extra_mcs_supported;

	if (freq <= ATH12K_2GHZ_MAX_FREQUENCY)
		cap_band = &cap->band[NL80211_BAND_2GHZ];
	else if (freq <= ATH12K_5GHZ_MAX_FREQUENCY)
		cap_band = &cap->band[NL80211_BAND_5GHZ];
	else
		cap_band = &cap->band[NL80211_BAND_6GHZ];

	extra_mcs_supported = u32_get_bits(cap_band->he_cap_info[1],
					   HE_EXTRA_MCS_SUPPORT);
	return extra_mcs_supported;
}

static int ath12k_tpc_fill_pream(struct ath12k *ar, char *buf, int buf_len, int len,
				 enum wmi_tpc_pream_bw pream_bw, u32 max_rix,
				 int max_nss, int max_rates, int pream_type,
				 enum wmi_halphy_ctrl_path_stats_id tpc_type,
				 int rate_idx, int eht_rate_idx)
{
	struct wmi_tpc_stats_arg *tpc_stats = ar->debug.tpc_stats;
	int nss, rates, chains;
	u8 active_tx_chains;
	u16 rate_code;
	s16 tpc;

	static const char *const pream_str[] = {
		[WMI_TPC_PREAM_CCK]     = "CCK",
		[WMI_TPC_PREAM_OFDM]    = "OFDM",
		[WMI_TPC_PREAM_HT20]    = "HT20",
		[WMI_TPC_PREAM_HT40]    = "HT40",
		[WMI_TPC_PREAM_VHT20]   = "VHT20",
		[WMI_TPC_PREAM_VHT40]   = "VHT40",
		[WMI_TPC_PREAM_VHT80]   = "VHT80",
		[WMI_TPC_PREAM_VHT160]  = "VHT160",
		[WMI_TPC_PREAM_HE20]    = "HE20",
		[WMI_TPC_PREAM_HE40]    = "HE40",
		[WMI_TPC_PREAM_HE80]    = "HE80",
		[WMI_TPC_PREAM_HE160]   = "HE160",
		[WMI_TPC_PREAM_EHT20]   = "EHT20",
		[WMI_TPC_PREAM_EHT40]   = "EHT40",
		[WMI_TPC_PREAM_EHT60]   = "EHT60",
		[WMI_TPC_PREAM_EHT80]   = "EHT80",
		[WMI_TPC_PREAM_EHT120]  = "EHT120",
		[WMI_TPC_PREAM_EHT140]  = "EHT140",
		[WMI_TPC_PREAM_EHT160]  = "EHT160",
		[WMI_TPC_PREAM_EHT200]  = "EHT200",
		[WMI_TPC_PREAM_EHT240]  = "EHT240",
		[WMI_TPC_PREAM_EHT280]  = "EHT280",
		[WMI_TPC_PREAM_EHT320]  = "EHT320",
		[WMI_TPC_PREAM_UHR20]   = "UHR20",
		[WMI_TPC_PREAM_UHR40]   = "UHR40",
		[WMI_TPC_PREAM_UHR60]   = "UHR60",
		[WMI_TPC_PREAM_UHR80]   = "UHR80",
		[WMI_TPC_PREAM_UHR120]  = "UHR120",
		[WMI_TPC_PREAM_UHR140]  = "UHR140",
		[WMI_TPC_PREAM_UHR160]  = "UHR160",
		[WMI_TPC_PREAM_UHR200]  = "UHR200",
		[WMI_TPC_PREAM_UHR240]  = "UHR240",
		[WMI_TPC_PREAM_UHR280]  = "UHR280",
		[WMI_TPC_PREAM_UHR320]  = "UHR320"};

	active_tx_chains = ar->num_tx_chains;

	for (nss = 0; nss < max_nss; nss++) {
		for (rates = 0; rates < max_rates; rates++, rate_idx++, max_rix++) {
			/* FW send extra MCS(10&11) for VHT and HE rates,
			 *  this is not used. Hence skipping it here
			 */
			if (pream_type == WMI_RATE_PREAMBLE_VHT &&
			    rates > ATH12K_VHT_MCS_MAX)
				continue;

			if (pream_type == WMI_RATE_PREAMBLE_HE &&
			    rates > ATH12K_HE_MCS_MAX)
				continue;

			if (pream_type == WMI_RATE_PREAMBLE_EHT &&
			    rates > ATH12K_EHT_MCS_MAX)
				continue;

			rate_code = ath12k_get_ratecode(pream_bw, nss, rates);
			len += scnprintf(buf + len, buf_len - len,
					 "%d\t %s\t 0x%03x\t", max_rix,
					 pream_str[pream_bw], rate_code);

			for (chains = 0; chains < active_tx_chains; chains++) {
				if (nss > chains) {
					len += scnprintf(buf + len,
							 buf_len - len,
							 "\t%s", "NA");
				} else {
					tpc = ath12k_tpc_get_rate(ar, tpc_stats,
								  rate_idx, chains + 1,
								  rate_code, pream_bw,
								  tpc_type,
								  eht_rate_idx);

					if (tpc == TPC_INVAL) {
						len += scnprintf(buf + len,
								 buf_len - len, "\tNA");
					} else {
						len += scnprintf(buf + len,
								 buf_len - len, "\t%d",
								 tpc);
					}
				}
			}
			len += scnprintf(buf + len, buf_len - len, "\n");

			if (pream_type == WMI_RATE_PREAMBLE_EHT)
				/*For fetching the next eht rates pwr from rates array2*/
				++eht_rate_idx;
		}
	}

	return len;
}

static int ath12k_tpc_stats_print(struct ath12k *ar,
				  struct wmi_tpc_stats_arg *tpc_stats,
				  char *buf, size_t len,
				  enum wmi_halphy_ctrl_path_stats_id type)
{
	u32 eht_idx = 0, uhr_idx = 0, pream_idx = 0, rate_pream_idx = 0, total_rates = 0;
	u32 max_rix = 0;
	u32 chan_freq, num_tx_chain, caps, i, j = 1;
	size_t buf_len = ATH12K_TPC_STATS_BUF_SIZE;
	u8 nss, active_tx_chains;
	bool he_ext_mcs;
	static const char *const type_str[WMI_HALPHY_PDEV_TX_STATS_MAX] = {
		[WMI_HALPHY_PDEV_TX_SU_STATS]		= "SU",
		[WMI_HALPHY_PDEV_TX_SUTXBF_STATS]	= "SU WITH TXBF",
		[WMI_HALPHY_PDEV_TX_MU_STATS]		= "MU",
		[WMI_HALPHY_PDEV_TX_MUTXBF_STATS]	= "MU WITH TXBF"};

	u8 max_rates[WMI_TPC_PREAM_MAX] = {
		[WMI_TPC_PREAM_CCK]     = ATH12K_CCK_RATES,
		[WMI_TPC_PREAM_OFDM]    = ATH12K_OFDM_RATES,
		[WMI_TPC_PREAM_HT20]    = ATH12K_HT_RATES,
		[WMI_TPC_PREAM_HT40]    = ATH12K_HT_RATES,
		[WMI_TPC_PREAM_VHT20]   = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_VHT40]   = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_VHT80]   = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_VHT160]  = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_HE20]    = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_HE40]    = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_HE80]    = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_HE160]   = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_EHT20]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT40]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT60]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT80]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT120]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT140]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT160]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT200]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT240]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT280]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT320]  = ATH12K_EHT_RATES,
		/* UHR shares same MCS0-15 rate count as EHT */
		[WMI_TPC_PREAM_UHR20]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR40]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR60]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR80]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR120]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR140]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR160]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR200]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR240]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR280]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_UHR320]  = ATH12K_EHT_RATES};
	static const u8 max_nss[WMI_TPC_PREAM_MAX] = {
		[WMI_TPC_PREAM_CCK]     = ATH12K_NSS_1,
		[WMI_TPC_PREAM_OFDM]    = ATH12K_NSS_1,
		[WMI_TPC_PREAM_HT20]    = ATH12K_NSS_4,
		[WMI_TPC_PREAM_HT40]    = ATH12K_NSS_4,
		[WMI_TPC_PREAM_VHT20]   = ATH12K_NSS_8,
		[WMI_TPC_PREAM_VHT40]   = ATH12K_NSS_8,
		[WMI_TPC_PREAM_VHT80]   = ATH12K_NSS_8,
		[WMI_TPC_PREAM_VHT160]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_HE20]    = ATH12K_NSS_8,
		[WMI_TPC_PREAM_HE40]    = ATH12K_NSS_8,
		[WMI_TPC_PREAM_HE80]    = ATH12K_NSS_8,
		[WMI_TPC_PREAM_HE160]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT20]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT40]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT60]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT80]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT120]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT140]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT160]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT200]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT240]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT280]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT320]  = ATH12K_NSS_4,
		/* UHR supports up to 5 chains on Trestles */
		[WMI_TPC_PREAM_UHR20]   = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR40]   = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR60]   = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR80]   = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR120]  = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR140]  = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR160]  = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR200]  = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR240]  = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR280]  = ATH12K_NSS_5,
		[WMI_TPC_PREAM_UHR320]  = ATH12K_NSS_5};

	u16 rate_idx[WMI_TPC_PREAM_MAX] = {}, eht_rate_idx[WMI_TPC_PREAM_MAX] = {};
	static const u8 pream_type[WMI_TPC_PREAM_MAX] = {
		[WMI_TPC_PREAM_CCK]     = WMI_RATE_PREAMBLE_CCK,
		[WMI_TPC_PREAM_OFDM]    = WMI_RATE_PREAMBLE_OFDM,
		[WMI_TPC_PREAM_HT20]    = WMI_RATE_PREAMBLE_HT,
		[WMI_TPC_PREAM_HT40]    = WMI_RATE_PREAMBLE_HT,
		[WMI_TPC_PREAM_VHT20]   = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_VHT40]   = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_VHT80]   = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_VHT160]  = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_HE20]    = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_HE40]    = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_HE80]    = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_HE160]   = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_EHT20]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT40]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT60]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT80]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT120]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT140]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT160]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT200]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT240]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT280]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT320]  = WMI_RATE_PREAMBLE_EHT,
		/* UHR uses EHT preamble type so rates_array2 lookup is shared */
		[WMI_TPC_PREAM_UHR20]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR40]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR60]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR80]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR120]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR140]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR160]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR200]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR240]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR280]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_UHR320]  = WMI_RATE_PREAMBLE_EHT};

	chan_freq = le32_to_cpu(tpc_stats->tpc_config.chan_freq);
	num_tx_chain = le32_to_cpu(tpc_stats->tpc_config.num_tx_chain);
	caps = le32_to_cpu(tpc_stats->tpc_config.caps);

	active_tx_chains = ar->num_tx_chains;
	he_ext_mcs = ath12k_he_supports_extra_mcs(ar, chan_freq);

	/* mcs 12&13 is sent by FW for certain HWs in rate array, skipping it as
	 * it is not supported
	 */
	if (he_ext_mcs) {
		for (i = WMI_TPC_PREAM_HE20; i <= WMI_TPC_PREAM_HE160; ++i)
			max_rates[i] = ATH12K_HE_RATES;
	}

	if (type == WMI_HALPHY_PDEV_TX_MU_STATS ||
	    type == WMI_HALPHY_PDEV_TX_MUTXBF_STATS) {
		pream_idx = WMI_TPC_PREAM_VHT20;

		for (i = WMI_TPC_PREAM_CCK; i <= WMI_TPC_PREAM_HT40; ++i)
			max_rix += max_nss[i] * max_rates[i];
	}
	/* Enumerate all the rate indices */
	for (i = rate_pream_idx + 1; i < WMI_TPC_PREAM_MAX; i++) {
		nss = (max_nss[i - 1] < num_tx_chain ?
		       max_nss[i - 1] : num_tx_chain);

		rate_idx[i] = rate_idx[i - 1] + max_rates[i - 1] * nss;

		if (pream_type[i] == WMI_RATE_PREAMBLE_EHT) {
			eht_rate_idx[j] = eht_rate_idx[j - 1] + max_rates[i] * nss;
			++j;
		}
	}

	for (i = 0; i < WMI_TPC_PREAM_MAX; i++) {
		nss = (max_nss[i] < num_tx_chain ?
		       max_nss[i] : num_tx_chain);
		total_rates += max_rates[i] * nss;
	}

	len += scnprintf(buf + len, buf_len - len,
			 "No.of rates-%d\n", total_rates);

	len += scnprintf(buf + len, buf_len - len,
			 "**************** %s ****************\n",
			 type_str[type]);
	len += scnprintf(buf + len, buf_len - len,
			 "\t\t\t\tTPC values for Active chains\n");
	len += scnprintf(buf + len, buf_len - len,
			 "Rate idx Preamble Rate code");

	for (i = 1; i <= active_tx_chains; ++i) {
		len += scnprintf(buf + len, buf_len - len,
				 "\t%d-Chain", i);
	}

	len += scnprintf(buf + len, buf_len - len, "\n");
	for (i = pream_idx; i < WMI_TPC_PREAM_MAX; i++) {
		if (chan_freq <= 2483) {
			if (i == WMI_TPC_PREAM_VHT80 ||
			    i == WMI_TPC_PREAM_VHT160 ||
			    i == WMI_TPC_PREAM_HE80 ||
			    i == WMI_TPC_PREAM_HE160 ||
			    (i >= WMI_TPC_PREAM_EHT60 &&
			     i <= WMI_TPC_PREAM_EHT320) ||
			    (i >= WMI_TPC_PREAM_UHR60 &&
			     i <= WMI_TPC_PREAM_UHR320)) {
				max_rix += max_nss[i] * max_rates[i];
				continue;
			}
		} else {
			if (i == WMI_TPC_PREAM_CCK) {
				max_rix += max_rates[i];
				continue;
			}
		}

		nss = (max_nss[i] < ar->num_tx_chains ? max_nss[i] : ar->num_tx_chains);

		if (!(caps & (1 << ATH12K_TPC_STATS_SUPPORT_BE_PUNC))) {
			if (i == WMI_TPC_PREAM_EHT60 || i == WMI_TPC_PREAM_EHT120 ||
			    i == WMI_TPC_PREAM_EHT140 || i == WMI_TPC_PREAM_EHT200 ||
			    i == WMI_TPC_PREAM_EHT240 || i == WMI_TPC_PREAM_EHT280 ||
			    i == WMI_TPC_PREAM_UHR60  || i == WMI_TPC_PREAM_UHR120 ||
			    i == WMI_TPC_PREAM_UHR140 || i == WMI_TPC_PREAM_UHR200 ||
			    i == WMI_TPC_PREAM_UHR240 || i == WMI_TPC_PREAM_UHR280) {
				max_rix += max_nss[i] * max_rates[i];
				continue;
			}
		}

		/* UHR and EHT share rates_array2. uhr_idx tracks position within
		 * UHR preambles (0=UHR20..10=UHR320), mapping to the same
		 * eht_rate_idx slots as EHT20..EHT320 respectively.
		 */
		if (i >= WMI_TPC_PREAM_UHR20 && i <= WMI_TPC_PREAM_UHR320) {
			len = ath12k_tpc_fill_pream(ar, buf, buf_len, len, i, max_rix,
						    nss, max_rates[i], pream_type[i],
						    type, rate_idx[i],
						    eht_rate_idx[uhr_idx]);
			++uhr_idx;
		} else {
			len = ath12k_tpc_fill_pream(ar, buf, buf_len, len, i, max_rix,
						    nss, max_rates[i], pream_type[i],
						    type, rate_idx[i],
						    eht_rate_idx[eht_idx]);

			if (pream_type[i] == WMI_RATE_PREAMBLE_EHT)
				/*For fetch the next index eht rates from rates array2*/
				++eht_idx;
		}

		max_rix += max_nss[i] * max_rates[i];
	}
	return len;
}

static void ath12k_tpc_stats_fill(struct ath12k *ar,
				  struct wmi_tpc_stats_arg *tpc_stats,
				  char *buf)
{
	size_t buf_len = ATH12K_TPC_STATS_BUF_SIZE;
	struct wmi_tpc_config_params *tpc;
	size_t len = 0;

	if (!tpc_stats) {
		ath12k_warn(ar->ab, "failed to find tpc stats\n");
		return;
	}

	spin_lock_bh(&ar->data_lock);

	tpc = &tpc_stats->tpc_config;
	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "*************** TPC config **************\n");
	len += scnprintf(buf + len, buf_len - len,
			 "* powers are in 0.25 dBm steps\n");
	len += scnprintf(buf + len, buf_len - len,
			 "reg domain-%d\t\tchan freq-%d\n",
			 tpc->reg_domain, tpc->chan_freq);
	len += scnprintf(buf + len, buf_len - len,
			 "power limit-%d\t\tmax reg-domain Power-%d\n",
			 le32_to_cpu(tpc->twice_max_reg_power) / 2, tpc->power_limit);
	len += scnprintf(buf + len, buf_len - len,
			 "No.of tx chain-%d\t",
			 ar->num_tx_chains);

	ath12k_tpc_stats_print(ar, tpc_stats, buf, len,
			       ar->debug.tpc_stats_type);
	spin_unlock_bh(&ar->data_lock);
}

static int ath12k_open_tpc_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	if (ar->ab->is_bypassed) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "device is in bypassed state\n");
		return -EPERM;
	}

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ah->state != ATH12K_HW_STATE_ON &&
	    ah->state != ATH12K_HW_STATE_TM) {
		ath12k_warn(ar->ab, "Interface not up\n");
		return -ENETDOWN;
	}

	void *buf __free(kfree) = kzalloc(ATH12K_TPC_STATS_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = ath12k_debug_tpc_stats_request(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request tpc stats: %d\n",
			    ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&ar->debug.tpc_complete, TPC_STATS_WAIT_TIME)) {
		spin_lock_bh(&ar->data_lock);
		ath12k_wmi_free_tpc_stats_mem(ar);
		ar->debug.tpc_request = false;
		spin_unlock_bh(&ar->data_lock);
		return -ETIMEDOUT;
	}

	ath12k_tpc_stats_fill(ar, ar->debug.tpc_stats, buf);
	file->private_data = no_free_ptr(buf);

	spin_lock_bh(&ar->data_lock);
	ath12k_wmi_free_tpc_stats_mem(ar);
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

static ssize_t ath12k_read_tpc_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static int ath12k_release_tpc_stats(struct inode *inode,
				    struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations fops_tpc_stats = {
	.open = ath12k_open_tpc_stats,
	.release = ath12k_release_tpc_stats,
	.read = ath12k_read_tpc_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_tpc_stats_type = {
	.write = ath12k_write_tpc_stats_type,
	.open = ath12k_debugfs_open,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_enable_extd_tx_stats(struct file *file,
						 const char __user *ubuf,
						 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	bool enable;
	int ret;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (ar->dp.dp_stats_mask & DP_ENABLE_EXT_TX_STATS) {
		ret = count;
		goto out;
	}

	if (enable)
		ar->dp.dp_stats_mask |= DP_ENABLE_EXT_TX_STATS;
	else
		ar->dp.dp_stats_mask &= ~DP_ENABLE_EXT_TX_STATS;

	ret = count;

out:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_read_enable_extd_tx_stats(struct file *file,
                                                char __user *ubuf,
                                                size_t count, loff_t *ppos)

{
        char buf[32] = {0};
        struct ath12k *ar = file->private_data;
        int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			(ar->dp.dp_stats_mask & DP_ENABLE_EXT_TX_STATS) ? 1 : 0);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_extd_tx_stats = {
        .read = ath12k_read_enable_extd_tx_stats,
        .write = ath12k_write_enable_extd_tx_stats,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_write_extd_rx_stats(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	bool enable;
	int ret = 0;
	enum dp_mon_stats_mode mode = 0;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	if (!ar->ab->hw_params->rxdma1_enable) {
		ret = count;
		goto exit;
	}

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (ar->dp.dp_stats_mask & DP_ENABLE_EXT_RX_STATS) {
		ret = count;
		goto exit;
	}

	mode = ATH12k_DP_MON_EXTD_STATS;
	if (enable) {
		ath12k_dp_mon_rx_stats_config(ar, true, mode);
		ar->dp.dp_stats_mask |= DP_ENABLE_EXT_RX_STATS;
	} else {
		ath12k_dp_mon_rx_stats_config(ar, false, mode);
		ar->dp.dp_stats_mask &= ~DP_ENABLE_EXT_RX_STATS;
	}

	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret) {
		ath12k_err(ar->ab, "failed to setup rx extd stats filters %d\n", ret);
		goto exit;
	}

	ret = count;
exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_read_extd_rx_stats(struct file *file,
					 char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[32];
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			(ar->dp.dp_stats_mask & DP_ENABLE_EXT_RX_STATS) ? 1 : 0);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_extd_rx_stats = {
	.read = ath12k_read_extd_rx_stats,
	.write = ath12k_write_extd_rx_stats,
	.open = ath12k_debugfs_open,
};

static int ath12k_reset_nrp_filter(struct ath12k *ar,
				   bool reset)
{
	int ret = 0;

	ath12k_dp_mon_rx_nrp_config(ar, reset);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret) {
		ath12k_err(ar->ab,
			   "failed to setup filter for monitor buf %d\n", ret);
	}
	return ret;
}

static int ath12k_reset_smart_mon_filter(struct ath12k *ar,
					 bool reset)
{
	int ret = 0;

	ath12k_dp_mon_rx_smart_mon_config(ar, reset);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret) {
		ath12k_err(ar->ab,
			   "failed to setup filter for monitor buf %d\n", ret);
	}
	return ret;
}

void ath12k_debugfs_nrp_cleanup_all(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_neighbor_peer *nrp, *tmp;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	spin_lock_bh(&dp->dp_lock);

	if (list_empty(&ar->ab->dp->neighbor_peers)) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	list_for_each_entry_safe(nrp, tmp, &dp->neighbor_peers, list) {
		list_del(&nrp->list);
		kfree(nrp);
	}

	dp->num_nrps = 0;
	dp_pdev->num_nrps = 0;
	spin_unlock_bh(&dp->dp_lock);

	debugfs_remove_recursive(ar->debug.debugfs_nrp);
	ar->debug.debugfs_nrp = NULL;
}

void ath12k_debugfs_nrp_clean(struct ath12k *ar, const u8 *addr, int num_nrp)
{
	int i, j;
	char fname[MAC_UNIT_LEN * ETH_ALEN] = {0};
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_vif *vif = NULL;

	for (i = 0, j = 0; i < (MAC_UNIT_LEN * ETH_ALEN); i += MAC_UNIT_LEN, j++) {
		if (j == ETH_ALEN - 1) {
			snprintf(fname + i, sizeof(fname) - i, "%02x", *(addr + j));
			break;
		}
		snprintf(fname + i, sizeof(fname) - i, "%02x:", *(addr + j));
	}

	debugfs_lookup_and_remove(fname, ar->debug.debugfs_nrp);
	if (!num_nrp) {
		debugfs_remove_recursive(ar->debug.debugfs_nrp);
		ar->debug.debugfs_nrp = NULL;
		if (!ath12k_dp_smart_mon_enabled(ar)) {
			ath12k_dp_mon_set_nrp(ar, false);
			ath12k_reset_nrp_filter(ar, true);
		} else {
			ath12k_reset_smart_mon_filter(ar, true);
			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR &&
				    arvif->is_started) {
					vif = arvif->ahvif->vif;
					ieee80211_enable_offchan_packet_capture(vif,
										FALSE);
					break;
				}
			}
		}
	}
}


static ssize_t ath12k_read_nrp_rssi(struct file *file,
				    char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_neighbor_peer *nrp = NULL;
	u8 macaddr[ETH_ALEN] = {0};
	loff_t file_pos = *ppos;
	struct path *fpath = &file->f_path;
	char *fname = fpath->dentry->d_iname;
	char buf[128] = {0};
	int i = 0;
	int j = 0;
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		return -ENETDOWN;
	}
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	if (file_pos > 0)
		return 0;

	for (i = 0, j = 0;  i < MAC_UNIT_LEN * ETH_ALEN; i += MAC_UNIT_LEN, j++) {
		if (sscanf(fname + i, "%hhX", &macaddr[j]) <= 0)
			return -EINVAL;
	}

	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry(nrp, &dp->neighbor_peers, list)
		if (ether_addr_equal(macaddr, nrp->addr))
			break;
	spin_unlock_bh(&dp->dp_lock);

	if (!nrp) {
		ath12k_warn(ab, "cannot find any NAC for mac addr [%pM]\n",
			    macaddr);
		return -EINVAL;
	}

	len = scnprintf(buf, sizeof(buf),
			"%-20s %-10s %-10s %s\n",
			"Neighbor Peer MAC", "RSSI", "Avg RSSI", "Time");
	len += scnprintf(buf + len, sizeof(buf) - len, "%-20pM %-10u %-10u %lld\n",
			 nrp->addr, nrp->rssi, nrp->avg_rssi, nrp->timestamp);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_read_nrp_rssi = {
	.read = ath12k_read_nrp_rssi,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_write_nrp_mac(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_neighbor_peer *nrp = NULL, *tmp = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_set_neighbor_rx_params *param = NULL;
	u8 mac[ETH_ALEN] = {0};
	char fname[MAC_UNIT_LEN * ETH_ALEN] = {0};
	char *str = NULL, *buf = NULL, *ptr = NULL;
	int i = 0;
	int j = 0;
	int ret = count, ret1 = -1;
	int action = 0, num_nrp;
	ssize_t rc = 0;
	bool del_nrp = false;
	bool smart_mon_enabled = false;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	buf = vmalloc(count);
	if (!buf) {
		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		return -ENOMEM;
	}

	ptr = buf;
	rc = simple_write_to_buffer(buf, count, ppos, ubuf, count);
	if (rc <= 0) {
		ret = rc;
		goto exit;
	}

	/* To remove '\n' at end of buffer */
	buf[count - 1] = '\0';

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	str = strsep(&buf, ",");
	if (!str) {
		ret = -EINVAL;
		goto exit;
	}

	if (!strcmp(str, "add"))
		action = WMI_FILTER_NRP_ACTION_ADD;
	else if (!strcmp(str, "del"))
		action = WMI_FILTER_NRP_ACTION_REMOVE;
	else {
		ath12k_err(ab, "error: invalid argument\n");
		ret = -EINVAL;
		goto exit;
	}

	memset(mac, 0, sizeof(mac));
	while ((str = strsep(&buf, ":")) != NULL) {
		if (i >= ETH_ALEN || kstrtou8(str, 16, mac + i)) {
			ath12k_warn(ab, "error: invalid mac address\n");
			ret = -EINVAL;
			goto exit;
		}
		i++;
	}

	if (i != ETH_ALEN) {
		ath12k_warn(ab, "error: invalid mac addr length\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!is_valid_ether_addr(mac)) {
		ath12k_err(ab, "error: invalid mac address\n");
		ret = -EINVAL;
		goto exit;
	}

	for (i = 0, j = 0; i < (MAC_UNIT_LEN * ETH_ALEN); i += MAC_UNIT_LEN, j++) {
		if (j == ETH_ALEN - 1) {
			snprintf(fname + i, sizeof(fname) - i, "%02x", mac[j]);
			break;
		}
		snprintf(fname + i, sizeof(fname) - i, "%02x:", mac[j]);
	}

	param = kzalloc(sizeof(*param), GFP_KERNEL);
	if (!param) {
		ret = -ENOMEM;
		goto exit;
	}

	param->action = action;

	if (ath12k_dp_ext_mon_is_enabled(ar)) {
		ath12k_err(ab, "ext mon enabled\n");
		ret = -EINVAL;
		goto exit;
	}

	switch (action) {
	case WMI_FILTER_NRP_ACTION_ADD:
		spin_lock_bh(&dp->dp_lock);
		if (dp_pdev->num_nrps >= (ATH12K_MAX_NRPS)) {
			spin_unlock_bh(&dp->dp_lock);
			ath12k_warn(ab, "max nrp reached, cannot create more\n");
			ret = -ENOMEM;
			goto err_free;
		}

		list_for_each_entry(nrp, &dp->neighbor_peers, list) {
			if (ether_addr_equal(nrp->addr, mac)) {
				spin_unlock_bh(&dp->dp_lock);
				ath12k_warn(ab, "cannot add existing neighbor peer %pM\n",
					    mac);
				ret = -EINVAL;
				goto err_free;
			}
		}
		spin_unlock_bh(&dp->dp_lock);

		spin_lock_bh(&ar->arsta_lock);
		if (ath12k_link_sta_find_by_addr(ar, mac)) {
			spin_unlock_bh(&ar->arsta_lock);
			ath12k_warn(ab,
				    "cannot add associated peer as neighbor peer %pM\n",
				     mac);
			ret = -EINVAL;
			goto err_free;
		}
		spin_unlock_bh(&ar->arsta_lock);

		nrp = kzalloc(sizeof(*nrp), GFP_KERNEL);
		if (!nrp) {
			ret = -ENOMEM;
			goto err_free;
		}

		nrp->vdev_id = -1;

		list_for_each_entry(arvif, &ar->arvifs, list) {
			if (ether_addr_equal(arvif->bssid, mac)) {
				ath12k_warn(ab,
					    "cannot add bssid as neighbor peer %pM\n",
					    mac);
				kfree(nrp);
				ret = -EINVAL;
				goto err_free;
			}
		}

		if (ar->monitor_started)
			smart_mon_enabled = ath12k_dp_smart_mon_enabled(ar);

		if (!smart_mon_enabled) {
			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
				    arvif->is_started) {
					nrp->vdev_id = arvif->vdev_id;
					break;
				}
			}
		} else {
			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR &&
				    arvif->is_started) {
					nrp->vdev_id = arvif->vdev_id;
					break;
				}
			}
		}

		if (nrp->vdev_id < 0) {
			ath12k_warn(ab,
				    "AP vap is not up, can't add this neighbor peer: %pM\n",
				    mac);
			kfree(nrp);
			ret = -EINVAL;
			goto err_free;
		}

		nrp->pdev_id = ar->pdev->pdev_id;
		ether_addr_copy(nrp->addr, mac);

		param->vdev_id = nrp->vdev_id;
		ether_addr_copy(param->nrp_addr, nrp->addr);

		spin_lock_bh(&dp->dp_lock);
		num_nrp = dp_pdev->num_nrps;
		spin_unlock_bh(&dp->dp_lock);
		if (!num_nrp) {
			ar->debug.debugfs_nrp = debugfs_create_dir("nrp_rssi",
								   ar->debug.debugfs_pdev);
			if (IS_ERR(ar->debug.debugfs_nrp)) {
				ath12k_err(ab, "failed to create nrp directory: %ld\n",
					   PTR_ERR(ar->debug.debugfs_nrp));
				kfree(nrp);
				ret = -ENOENT;
				goto err_free;
			} else if (!ar->debug.debugfs_nrp) {
				ath12k_err(ab, "failed to create nrp directory\n");
				kfree(nrp);
				ret = -ENOENT;
				goto err_free;
			}
			if (!smart_mon_enabled) {
				ath12k_dp_mon_set_nrp(ar, true);
				ath12k_reset_nrp_filter(ar, false);
			} else {
				ath12k_reset_smart_mon_filter(ar, false);
				ieee80211_enable_offchan_packet_capture(arvif->ahvif->vif,
									TRUE);
			}
		}
		spin_lock_bh(&dp->dp_lock);
		list_add_tail(&nrp->list, &dp->neighbor_peers);
		dp->num_nrps++;
		dp_pdev->num_nrps++;
		spin_unlock_bh(&dp->dp_lock);

		ath12k_debugfs_create_file(fname, 0644,
					   ar->debug.debugfs_nrp,
					   ar->ab, ar,
					   &fops_read_nrp_rssi);
		break;
	case WMI_FILTER_NRP_ACTION_REMOVE:
		spin_lock_bh(&dp->dp_lock);
		if (!dp_pdev->num_nrps) {
			spin_unlock_bh(&dp->dp_lock);
			ath12k_warn(ab,
				    "nrp list is empty, can't delete this mac: %pM for the pdev_id: %d\n",
				    mac, ar->pdev->pdev_id);
			ret = -EINVAL;
			goto err_free;
		}

		list_for_each_entry_safe(nrp, tmp, &dp->neighbor_peers, list) {
			if (ether_addr_equal(nrp->addr, mac) &&
			    nrp->pdev_id == ar->pdev->pdev_id) {
				list_del(&nrp->list);
				dp->num_nrps--;
				dp_pdev->num_nrps--;
				del_nrp = true;
				break;
			}
		}
		spin_unlock_bh(&dp->dp_lock);

		if (!del_nrp) {
			ath12k_warn(ab, "cannot delete %pM not added to this pdev: %d\n",
				    mac, ar->pdev->pdev_id);
			ret = -EINVAL;
			goto err_free;
		} else {
			spin_lock_bh(&dp->dp_lock);
			num_nrp = dp_pdev->num_nrps;
			spin_unlock_bh(&dp->dp_lock);
			ath12k_debugfs_nrp_clean(ar, mac, num_nrp);
			param->vdev_id = nrp->vdev_id;
			ether_addr_copy(param->nrp_addr, nrp->addr);
			kfree(nrp);
		}
		break;
	default:
		break;
	}

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "debugfs nrp neighbor params mac: %pM pdev_id: %d vdev_id: %d type: %d\n",
		   param->nrp_addr, ar->pdev->pdev_id, param->vdev_id, param->action);

	ret1 = ath12k_wmi_vdev_set_neighbor_rx_cmd(ar, param);
	if (ret1) {
		ath12k_err(ar->ab,
			   "debugfs nrp neighbor rx cmd failed, params vdev id %d action %d, nrp %pM",
			   param->vdev_id, param->action, param->nrp_addr);
		ret = ret1;
	}
err_free:
	kfree(param);
exit:
	 wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	vfree(ptr);
	return ret;
}


static const struct file_operations fops_write_nrp_mac = {
	.write = ath12k_write_nrp_mac,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_write_smart_mon_filter(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u8 filter_value;
	int ret;

	if (kstrtou8_from_user(ubuf, count, 0, &filter_value))
		return -EINVAL;

	/* Validate filter value (4-bit field: 0x0 to 0xF) */
	if (filter_value > 0xF) {
		ath12k_err(ar->ab, "Invalid smart_mon_filter value: 0x%x (valid range: 0x0-0xF)\n",
			   filter_value);
		return -EINVAL;
	}

	if (filter_value && !(filter_value & DP_SMART_MON_VALID)) {
		ath12k_err(ar->ab, "Invalid smart_mon_filter value: 0x%x Valid bit is not set\n",
			   filter_value);
		return -EINVAL;
	}

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dp_smart_mon_filter_type_set(ar, filter_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "smart_mon_filter set to 0x%x (V:%d D:%d M:%d C:%d)\n",
		   filter_value,
		   u32_get_bits(filter_value, SMART_MON_FILTER_V),
		   u32_get_bits(filter_value, SMART_MON_FILTER_D),
		   u32_get_bits(filter_value, SMART_MON_FILTER_M),
		   u32_get_bits(filter_value, SMART_MON_FILTER_C));

	ret = count;

	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_read_smart_mon_filter(struct file *file,
					    char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[128] = {0};
	int len = 0;
	u8 filter_value;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dp_smart_mon_filter_type_get(ar, &filter_value);

	len += scnprintf(buf + len, sizeof(buf) - len,
			 "smart_mon_filter: 0x%x\n", filter_value);
	len += scnprintf(buf + len, sizeof(buf) - len,
			 "  Valid bit (V): %d\n", u32_get_bits(filter_value,
							       SMART_MON_FILTER_V));
	len += scnprintf(buf + len, sizeof(buf) - len,
			 "  Data filter (D): %d\n", u32_get_bits(filter_value,
								 SMART_MON_FILTER_D));
	len += scnprintf(buf + len, sizeof(buf) - len,
			 "  Mgmt filter (M): %d\n", u32_get_bits(filter_value,
								 SMART_MON_FILTER_M));
	len += scnprintf(buf + len, sizeof(buf) - len,
			 "  Ctrl filter (C): %d\n", u32_get_bits(filter_value,
								 SMART_MON_FILTER_C));

	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_smart_mon_filter = {
	.read = ath12k_read_smart_mon_filter,
	.write = ath12k_write_smart_mon_filter,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath12k_debugfs_op_vif_add(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif)
{
}
EXPORT_SYMBOL(ath12k_debugfs_op_vif_add);




static ssize_t ath12k_fse_ops_write(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_pdev *pdev;
	struct rx_flow_info flow_info = {0};
	u8 buf[160] = {0};
	u32 sip_addr[4] = {0};
	u32 dip_addr[4] = {0};

	char ops[8], ipver[8], srcip[48], destip[48];
	u32 srcport, destport, protocol;

	int ret, i;
	bool radioup = false;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar) {
			radioup = true;
			break;
		}
	}

	if (!radioup) {
		ath12k_err(ab, "radio is not up\n");
		ret = -ENETDOWN;
		goto exit;
	}
	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		goto exit;

	buf[ret] = '\0';

	ret = sscanf(buf, "%7s %7s %47s %47s %d %d %d", ops, ipver,
		     srcip, destip, &srcport, &destport, &protocol);

	if (!(ret == 7 || ret == 1)) {
		ret = -EINVAL;
		goto exit;
	}
	if (ret == 1) {
		if (!strcmp(ops, "RST")) {
			ath12k_dp_rx_flow_delete_all_entries(ab);
			ret = count;
		} else {
			ret = -EINVAL;
		}
		goto exit;
	}

	if (!(protocol == IPPROTO_TCP || protocol == IPPROTO_UDP)) {
		ret = -EINVAL;
		goto exit;
	}

	if (!(srcport >= ATH12K_UDP_TCP_START_PORT &&
	      srcport <= ATH12K_UDP_TCP_END_PORT) ||
	    !(destport >= ATH12K_UDP_TCP_START_PORT &&
	      destport <= ATH12K_UDP_TCP_END_PORT)) {
		ret = -EINVAL;
		goto exit;
	}

	if (!strcmp(ipver, "IPV4")) {
		if (!in4_pton(srcip, -1, (u8 *)&sip_addr[0], -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
		if (!in4_pton(destip, -1, (u8 *)&dip_addr[0], -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
	} else if (!strcmp(ipver, "IPV6")) {
		if (!in6_pton(srcip, -1, (u8 *)sip_addr, -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
		if (!in6_pton(destip, -1, (u8 *)dip_addr, -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
	} else {
		ret = -EINVAL;
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "S_IP:%x:%x:%x:%x D_IP:%x:%x:%x:%x\n",
		   sip_addr[0], sip_addr[1], sip_addr[2], sip_addr[3],
		   dip_addr[0], dip_addr[1], dip_addr[2], dip_addr[3]);

	if (!strcmp(ops, "ADD") || !strcmp(ops, "DEL")) {
		flow_info.flow_tuple_info.src_port = srcport;
		flow_info.flow_tuple_info.dest_port = destport;
		flow_info.flow_tuple_info.l4_protocol = protocol;

		if (!strcmp(ipver, "IPV4")) {
			flow_info.is_addr_ipv4 = 1;
			flow_info.flow_tuple_info.src_ip_31_0 = sip_addr[0];
			flow_info.flow_tuple_info.dest_ip_31_0 = dip_addr[0];
		} else {
			flow_info.flow_tuple_info.src_ip_31_0 = sip_addr[3];
			flow_info.flow_tuple_info.src_ip_63_32 = sip_addr[2];
			flow_info.flow_tuple_info.src_ip_95_64 = sip_addr[1];
			flow_info.flow_tuple_info.src_ip_127_96 = sip_addr[0];

			flow_info.flow_tuple_info.dest_ip_31_0 = dip_addr[3];
			flow_info.flow_tuple_info.dest_ip_63_32 = dip_addr[2];
			flow_info.flow_tuple_info.dest_ip_95_64 = dip_addr[1];
			flow_info.flow_tuple_info.dest_ip_127_96 = dip_addr[0];
		}
		if (!strcmp(ops, "ADD")) {
			flow_info.fse_metadata = ATH12K_RX_FSE_FLOW_MATCH_DEBUGFS;
			ath12k_dp_rx_flow_add_entry(ab, &flow_info);
		} else {
			ath12k_dp_rx_flow_delete_entry(ab, &flow_info);
		}
		ret = count;
	}

exit:
	if (ret == -EINVAL) {
		ath12k_warn(ab, "Invalid input\nUsage:\n");
		ath12k_warn(ab, "<Case1> Cmd(RST)\n");
		ath12k_warn(ab, "<Example> RST\n");
		ath12k_warn(ab, "<Case2> Cmd(ADD/DEL) ipver(IPV4/IPV6) srcip destip srcport(0-65535) destport(0-65535) protocol(6-tcp 17-udp)\n");
		ath12k_warn(ab, "<Example1> ADD IPV4 192.168.1.1 192.168.1.2 15000 15500 17\n");
		ath12k_warn(ab, "<Example2> DEL IPV4 192.168.1.1 192.168.1.2 15000 15500 17\n");
		ath12k_warn(ab, "<Example3> ADD IPV6 ffaa:bbcc:1122:5566:ccbb:aadd:1122:5555 fefe:b1c1:1122:5566:ccbb:aadd:1122:5555 15000 15500 17\n");
		ath12k_warn(ab, "<Example4> DEL IPV6 ffaa:bbcc:1122:5566:ccbb:aadd:1122:5555 fefe:b1c1:1122:5566:ccbb:aadd:1122:5555 15000 15500 17\n");
	}
	return ret;
}

static ssize_t ath12k_read_fst_core_mask(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	char *buf;
	const int size = 256;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf + len, size - len,
			"\nFST core mask: %u\n",
			dp->fst_config.fst_core_mask);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static ssize_t ath12k_write_fst_core_mask(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev *pdev;
	u32 fst_core_mask;
	int ret, i;
	bool radioup = false;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar) {
			radioup = true;
			break;
		}
	}

	if (!radioup) {
		ath12k_err(ab, "radio is not up\n");
		return -ENETDOWN;
	}

	ret = kstrtou32_from_user(ubuf, count, 0, &fst_core_mask);
	if (ret)
		return -EINVAL;

	if (!dp->dp_hw_grp->fst) {
		ath12k_err(ab, "FST table is NULL\n");
		return -EINVAL;
	}

	if (fst_core_mask < ATH12K_DP_MIN_FST_CORE_MASK ||
	    fst_core_mask > ATH12K_DP_MAX_FST_CORE_MASK) {
		ath12k_err(ab, "Invalid FST core mask:0x%x\n",
			   fst_core_mask);
		return -EINVAL;
	}

	dp->fst_config.fst_core_mask = fst_core_mask;
	if (fst_core_mask)
		ath12k_dp_fst_core_map_init(ab);

	ret = count;
	return ret;
}

static ssize_t ath12k_dump_fst_flow_stats(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	char *buf;
	const int size = 1024;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			"\nNo of IPv4 Flow entries inserted: %u\n",
			fst->ipv4_fse_rule_cnt);

	len += scnprintf(buf + len, size - len,
			"\nNo of IPv6 Flow entries inserted: %u\n",
			fst->ipv6_fse_rule_cnt);

	len += scnprintf(buf + len, size - len,
			"\nFlow addition failure: %u\n",
			fst->flow_add_fail);

	len += scnprintf(buf + len, size - len,
			"\nFlow deletion failure: %u\n",
			fst->flow_del_fail);

	len += scnprintf(buf + len, size - len,
			"\nNo of Flows per reo:\n0:%u\t1:%u\t2:%u\t3:%u\n",
			fst->flows_per_reo[0],
			fst->flows_per_reo[1],
			fst->flows_per_reo[2],
			fst->flows_per_reo[3]);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static ssize_t ath12k_dump_fst_dump_table(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char *buf;
	const int size = 256 * 2048;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = ath12k_dp_dump_fst_table(ab, buf + len, size - len);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_fse = {
	.open = ath12k_debugfs_open,
	.write = ath12k_fse_ops_write,
	.owner = THIS_MODULE,
};

static const struct file_operations fops_fst_core_mask = {
	.open = ath12k_debugfs_open,
	.read = ath12k_read_fst_core_mask,
	.write = ath12k_write_fst_core_mask,
	.owner = THIS_MODULE,
};

static const struct file_operations fops_fst_dp_stats = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_fst_flow_stats,
	.owner = THIS_MODULE,
};

static const struct file_operations fops_fst_dump_table = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_fst_dump_table,
	.owner = THIS_MODULE,
};

void ath12k_fst_debugfs_init(struct ath12k_base *ab)
{
	struct dentry *fsestats_dir = debugfs_create_dir("fst_config", ab->debugfs_soc);

	ath12k_debugfs_create_file("fst_core_mask", 0600, fsestats_dir,
				   ab, ab,
				   &fops_fst_core_mask);

	ath12k_debugfs_create_file("fst_dp_stats", 0400, fsestats_dir,
				   ab, ab,
				   &fops_fst_dp_stats);

	ath12k_debugfs_create_file("fst_dump_table", 0400, fsestats_dir,
				   ab, ab,
				   &fops_fst_dump_table);

	ath12k_debugfs_create_file("fse", 0200, fsestats_dir,
				   ab, ab,
				   &fops_fse);
}

static ssize_t ath12k_dump_ce_stats_histogram(struct file *file,
					      char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_ce_pipe *pipe;
	struct ath12k_ce_stats *stats;
	char header[] = {"CENum\t0-0.5ms\t0.5-1ms\t1-2ms\t2-5ms\t5-10ms\t >10ms"};
	char *print_buff;
	int len = 0, size = 12288;
	int i, retval;

	print_buff = kzalloc(size, GFP_KERNEL);
	if (!print_buff)
		return -ENOMEM;

	len += scnprintf(print_buff + len, size - len, "\n----- Tasklet Scheduled  Bucket -----\n");
	len += scnprintf(print_buff + len, size - len, "%s\n", header);

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		pipe = &ab->ce.ce_pipe[i];
		stats = pipe->ce_stats;
		if (!stats)
			continue;

		len += scnprintf(print_buff + len, size - len,
				" CE%02d \t %04llu\t %04llu\t %04llu\t %04llu\t %04llu\t %04llu \t\n",
				pipe->pipe_num, stats->sched_bucket[CE_BUCKET_500_US],
				stats->sched_bucket[CE_BUCKET_1_MS],
				stats->sched_bucket[CE_BUCKET_2_MS],
				stats->sched_bucket[CE_BUCKET_5_MS],
				stats->sched_bucket[CE_BUCKET_10_MS],
				stats->sched_bucket[CE_BUCKET_BEYOND]
				);
	}

	len += scnprintf(print_buff + len, size - len, "\n----- Tasklet Execution Bucket -----\n");
	len += scnprintf(print_buff + len, size - len, "%s\n", header);

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		pipe = &ab->ce.ce_pipe[i];
		stats = pipe->ce_stats;
		if (!stats)
			continue;
		len += scnprintf(print_buff + len, size - len,
				" CE%02d \t %04llu\t %04llu\t %04llu\t %04llu\t %04llu\t %04llu \t\n",
				pipe->pipe_num, stats->exec_bucket[CE_BUCKET_500_US],
				stats->exec_bucket[CE_BUCKET_1_MS],
				stats->exec_bucket[CE_BUCKET_2_MS],
				stats->exec_bucket[CE_BUCKET_5_MS],
				stats->exec_bucket[CE_BUCKET_10_MS],
				stats->exec_bucket[CE_BUCKET_BEYOND]
				);
	}
	len += scnprintf(print_buff + len, size - len, "\n----- Scheduled Last Updated -----\n");
	len += scnprintf(print_buff + len, size - len, "%s\n", header);

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		pipe = &ab->ce.ce_pipe[i];
		stats = pipe->ce_stats;
		if (!stats)
			continue;
		len += scnprintf(print_buff + len, size - len,
				" CE%02d \t %lluus\t %lluus\t %lluus\t %lluus\t %lluus\t %lluus \t\n",
				pipe->pipe_num, stats->sched_last_update[CE_BUCKET_500_US],
				stats->sched_last_update[CE_BUCKET_1_MS],
				stats->sched_last_update[CE_BUCKET_2_MS],
				stats->sched_last_update[CE_BUCKET_5_MS],
				stats->sched_last_update[CE_BUCKET_10_MS],
				stats->sched_last_update[CE_BUCKET_BEYOND]
				);
	}

	len += scnprintf(print_buff + len, size - len, "\n----- Execution Last Updated -----\n");
	len += scnprintf(print_buff + len, size - len, "%s\n", header);

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		pipe = &ab->ce.ce_pipe[i];
		stats = pipe->ce_stats;
		if (!stats)
			continue;
		len += scnprintf(print_buff + len, size - len,
				" CE%02d \t %lluus\t %lluus\t %lluus\t %lluus\t %lluus\t %lluus \t\n",
				pipe->pipe_num, stats->exec_last_update[CE_BUCKET_500_US],
				stats->exec_last_update[CE_BUCKET_1_MS],
				stats->exec_last_update[CE_BUCKET_2_MS],
				stats->exec_last_update[CE_BUCKET_5_MS],
				stats->exec_last_update[CE_BUCKET_10_MS],
				stats->exec_last_update[CE_BUCKET_BEYOND]
				);
	}

	if (len > size)
		len = size;

	retval = simple_read_from_buffer(user_buf, count, ppos, print_buff, len);
	kfree(print_buff);

	return retval;
}

static ssize_t ath12k_dump_ce_stats_history(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_ce_pipe *pipe;
	struct ath12k_ce_stats *stats;
	char *print_buff;
	int len = 0, size = 16384;
	int i, retval, j, ret;

	print_buff = kzalloc(size, GFP_KERNEL);
	if (!print_buff)
		return -ENOMEM;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		pipe = &ab->ce.ce_pipe[i];
		stats = pipe->ce_stats;
		if (!stats)
			continue;

		ret = snprintf(print_buff + len, size - len,
			       "\n-----CE%d Last 20 timing records -----\n", pipe->pipe_num);
		if (ret < 0 || ret >= size - len)
			break;
		len += ret;

		for (j = 0; j < MAX_CE_STATS_RECORDS; j++) {
			ret = snprintf(print_buff + len, size - len,
				       "R%02d- schedule time %lldus   execution time %lldus\n",
				       j, stats->sched_time_record[j],
				       stats->exec_time_record[j]);

			if (ret < 0 || ret >= size - len)
				break;
			len += ret;
		}
	}

	if (len > size)
		len = size;

	retval = simple_read_from_buffer(user_buf, count, ppos, print_buff, len);
	kfree(print_buff);

	return retval;
}

static ssize_t ath12k_write_ce_stats_enable(struct file *file,
					    const char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	bool enable;

	if (kstrtobool_from_user(user_buf, count, &enable))
		return -EINVAL;

	ab->ce.enable_ce_stats = enable;

	return count;
}

static const struct file_operations fops_ce_stats_enable = {
	.open = ath12k_debugfs_open,
	.write = ath12k_write_ce_stats_enable,
};

static const struct file_operations fops_ce_stats_histogram = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_ce_stats_histogram,
};

static const struct file_operations fops_ce_stats_history = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_ce_stats_history,
};

void ath12k_ce_stats_debugfs_init(struct ath12k_base *ab)
{
	struct dentry *cestats_dir  = debugfs_create_dir("ce_stats", ab->debugfs_soc);

	ath12k_debugfs_create_file("ce_histogram", 0400, cestats_dir,
				   ab, ab,
				   &fops_ce_stats_histogram);

	ath12k_debugfs_create_file("ce_history", 0400, cestats_dir,
				   ab, ab,
				   &fops_ce_stats_history);

	ath12k_debugfs_create_file("enable_ce_stats", 0600, cestats_dir,
				   ab, ab,
				   &fops_ce_stats_enable);
}

void ath12k_debugfs_pdev_destroy(struct ath12k_base *ab)
{
}

static ssize_t ath12k_write_compress_fw_dump(struct file *file,
					     const char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	bool value;

	if (kstrtobool_from_user(user_buf, count, &value)) {
		ath12k_info(ab, "Enter 1 to enable, 0 to disable dump compression\n");
		return -EINVAL;
	}

	ath12k_compress_dump = value;
	return count;
}

static ssize_t ath12k_read_compress_fw_dump(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	char buf[32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ath12k_compress_dump);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_compress_fw_dump = {
	.open = ath12k_debugfs_open,
	.read = ath12k_read_compress_fw_dump,
	.write = ath12k_write_compress_fw_dump,
	.llseek = default_llseek,
};

void ath12k_debugfs_soc_create(struct ath12k_base *ab)
{
	bool dput_needed;
	char soc_name[64] = { 0 };
	struct dentry *debugfs_ath12k;
	struct dentry *debugfs_ath12k_compress_dump;

	debugfs_ath12k = debugfs_lookup("ath12k", NULL);
	if (debugfs_ath12k) {
		/* a dentry from lookup() needs dput() after we don't use it */
		dput_needed = true;
	} else {
		debugfs_ath12k = debugfs_create_dir("ath12k", NULL);
		if (IS_ERR_OR_NULL(debugfs_ath12k))
			return;
		dput_needed = false;
	}

	debugfs_ath12k_compress_dump = debugfs_lookup("compress_fw_dump", debugfs_ath12k);
	if (!debugfs_ath12k_compress_dump) {
		debugfs_ath12k_compress_dump =
				ath12k_debugfs_create_file("compress_fw_dump",
							   0600, debugfs_ath12k,
							   ab, ab,
							   &fops_compress_fw_dump);
		if (IS_ERR_OR_NULL(debugfs_ath12k_compress_dump))
			ath12k_err(ab, "failed to create compress_dump entry\n");
	} else {
		dput(debugfs_ath12k_compress_dump);
	}

	scnprintf(soc_name, sizeof(soc_name), "%s-%s", ath12k_bus_str(ab->hif.bus),
		  dev_name(ab->dev));

	ab->debugfs_soc = debugfs_create_dir(soc_name, debugfs_ath12k);

	if (dput_needed)
		dput(debugfs_ath12k);

	ath12k_fst_debugfs_init(ab);

	ath12k_ce_stats_debugfs_init(ab);
}

static ssize_t ath12k_write_wmi_ctrl_path_stats(struct file *file,
						const char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wmi_ctrl_path_stats_arg arg = {};
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	u8 buf[128] = {0};
	int ret;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';

	ret = sscanf(buf, "%u %u", &arg.stats_id, &arg.action);
	if (ret != 2)
		return -EINVAL;

	if (!arg.action)
		return -EINVAL;

	guard(mutex)(&ah->hw_mutex);
	ret = ath12k_wmi_send_wmi_ctrl_stats_cmd(ar, &arg);
	return ret ? ret : count;
}

static int print_pmlo_stats(char *buf, int size,
			    struct wmi_ctrl_path_pmlo_telemetry_stats *pmlo_stats)
{
	int len = 0;

	len += scnprintf(buf + len, size - len, "PMLO Telemetry Statistics:\n");
	len += scnprintf(buf + len, size - len, "pdev_id = %u\n",
			 le32_to_cpu(pmlo_stats->pdev_id));

	u32 estimated_air_time_per_ac =
		le32_to_cpu(pmlo_stats->estimated_air_time_per_ac);

	len += scnprintf(buf + len, size - len, "Estimated Air Time per AC:\n");
	len += scnprintf(buf + len, size - len, "BE: %u\n",
			 u32_get_bits(estimated_air_time_per_ac, GENMASK(7, 0)));
	len += scnprintf(buf + len, size - len, "BK: %u\n",
			 u32_get_bits(estimated_air_time_per_ac, GENMASK(15, 8)));
	len += scnprintf(buf + len, size - len, "VI: %u\n",
			 u32_get_bits(estimated_air_time_per_ac, GENMASK(23, 16)));
	len += scnprintf(buf + len, size - len, "VO: %u\n",
			 u32_get_bits(estimated_air_time_per_ac, GENMASK(31, 24)));

	return len;
}

static int print_pdev_stats(char *buf, int size, void *stats_ptr)
{
	char fw_tx_mgmt_subtype[WMI_MAX_STRING_LEN] = {0};
	char fw_rx_mgmt_subtype[WMI_MAX_STRING_LEN] = {0};
	u16 index_tx, index_rx;
	struct wmi_ctrl_path_pdev_stats *pdev_stats = stats_ptr;
	u8 i;
	int len = 0;

	LIST_HEAD(wmi_stats_list);
	index_tx = 0;
	index_rx = 0;

	for (i = 0; i < IEEE80211_MGMT_FRAME_SUBTYPE_MAX; i++) {
		index_tx += scnprintf(&fw_tx_mgmt_subtype[index_tx],
				WMI_MAX_STRING_LEN - index_tx,
				" %u:%u,", i,
				pdev_stats->tx_mgmt_subtype[i]);
		index_rx += scnprintf(&fw_rx_mgmt_subtype[index_rx],
				WMI_MAX_STRING_LEN - index_rx,
				" %u:%u,", i,
				pdev_stats->rx_mgmt_subtype[i]);
	}

	len += scnprintf(buf + len, size - len,
			"WMI_CTRL_PATH_PDEV_TX_STATS:\n");
	len += scnprintf(buf + len, size - len,
			"fw_tx_mgmt_subtype = %s\n",
			fw_tx_mgmt_subtype);
	len += scnprintf(buf + len, size - len,
			"fw_rx_mgmt_subtype = %s\n",
			fw_rx_mgmt_subtype);
	len += scnprintf(buf + len, size - len,
			"scan_fail_dfs_violation_time_ms = %u\n",
			pdev_stats->scan_fail_dfs_viol_time_ms);
	len += scnprintf(buf + len, size - len,
			"nol_chk_fail_last_chan_freq = %u\n",
			pdev_stats->nol_chk_fail_last_chan_freq);
	len += scnprintf(buf + len, size - len,
			"nol_chk_fail_time_stamp_ms = %u\n",
			pdev_stats->nol_chk_fail_time_stamp_ms);
	len += scnprintf(buf + len, size - len,
			"tot_peer_create_cnt = %u\n",
			pdev_stats->tot_peer_create_cnt);
	len += scnprintf(buf + len, size - len,
			"tot_peer_del_cnt = %u\n",
			pdev_stats->tot_peer_del_cnt);
	len += scnprintf(buf + len, size - len,
			"tot_peer_del_resp_cnt = %u\n",
			pdev_stats->tot_peer_del_resp_cnt);
	len += scnprintf(buf + len, size - len,
			"vdev_pause_fail_rt_to_sched_algo_fifo_full_cnt = %u\n",
			pdev_stats->sched_algo_fifo_full_cnt);
	return len;
}

int wmi_ctrl_path_mem_stat(struct ath12k *ar, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats, *tmp;
	struct wmi_ctrl_path_mem_stats_params *mem_stats;
	const int size = 2048;
	int len = 0, ret_val;
	char *buf;
	LIST_HEAD(wmi_stats_list);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			"WMI_CTRL_PATH_MEM_STATS:\n");

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_splice_tail_init(&ar->debug.wmi_ctrl_path_stats.pdev_stats, &wmi_stats_list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_for_each_entry_safe(stats, tmp, &wmi_stats_list, list) {
		if (!stats)
			break;

		mem_stats = stats->stats_ptr;

		if (!mem_stats)
			break;

		if (mem_stats->total_bytes){
			len += scnprintf(buf + len, size - len,
				"arena_id = %u\n",
				le32_to_cpu(mem_stats->arena_id));
			len += scnprintf(buf + len, size - len,
				"arena = %s\n",
				wmi_ctrl_path_fw_arena_id_to_name(le32_to_cpu(mem_stats->arena_id)));
			len += scnprintf(buf + len, size - len,
				"total_bytes = %u\n",
				le32_to_cpu(mem_stats->total_bytes));
			len += scnprintf(buf + len, size - len,
				"allocated_bytes = %u\n",
				le32_to_cpu(mem_stats->allocated_bytes));
		}

		kfree(stats->stats_ptr);
		list_del(&stats->list);
		kfree(stats);

	}

	ar->ctrl_mem_stats = false;
	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret_val;
}

static ssize_t ath12k_read_all_wmi_ctrl_path_stats(struct file *file,
					       char __user *ubuf,
					       size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats, *tmp;
	struct ath12k *ar = file->private_data;
	int ret_val = 0;
	const int size = 2048;
	int len = 0;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len, "Periodic Stats:\n");

	LIST_HEAD(periodic_stats_list);

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_splice_tail_init(&ar->debug.period_wmi_list, &periodic_stats_list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_for_each_entry_safe(stats, tmp, &periodic_stats_list, list) {
		switch (stats->tagid) {
		case WMI_CTRL_PATH_PMLO_STATS:
			len += print_pmlo_stats(buf + len, size - len, stats->stats_ptr);
			break;
		default:
			/* With current design and a common list,
			 * all the items are freed from the wmi_list when a
			 * periodic tlv is processed.
			 * Refer: ath12k_wmi_crl_path_stats_list_free
			 * To print wmi ctrl data, we need to re-design this
			 * path to support multiple periodic tlvs.
			 * Currently, only 1 tlv is enabled as periodic so it
			 * works with a new list keeping the same design in
			 * place.
			 */
			len += scnprintf(buf + len, size - len, "Unknown tagid: %u\n",
					 stats->tagid);
			break;
		}

		kfree(stats->stats_ptr);
		list_del(&stats->list);
		kfree(stats);
	}

	len += scnprintf(buf + len, size - len, "\nOn-Demand Stats:\n");
	LIST_HEAD(on_demand_stats_list);

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_splice_tail_init(&ar->debug.wmi_ctrl_path_stats.pdev_stats,
			      &on_demand_stats_list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_for_each_entry_safe(stats, tmp, &on_demand_stats_list, list) {
		if (!stats)
			break;

		switch (stats->tagid) {
		case WMI_TAG_CTRL_PATH_PDEV_STATS:
			len += print_pdev_stats(buf + len, size - len, stats->stats_ptr);
			break;
		case WMI_CTRL_PATH_CAL_STATS:
			len += print_cal_stats(buf + len, size - len, stats->stats_ptr);
			break;
		case WMI_CTRL_PATH_BTCOEX_STATS:
			len += print_btcoex_stats(buf + len, size - len,
						  stats->stats_ptr);
			break;
		case WMI_CTRL_PATH_AWGN_STATS:
			len += print_awgn_stats(buf + len, size - len, stats->stats_ptr);
			break;
		case WMI_CTRL_PATH_MEM_STATS:
			len += print_mem_stats(buf + len, size - len, stats->stats_ptr);
			break;
		case WMI_CTRL_PATH_AFC_STATS:
			len += print_afc_stats(buf + len, size - len, stats->stats_ptr);
			break;
		case WMI_CTRL_PATH_BLANKING_STATS:
			len += print_blanking_stats(buf + len, size - len,
							stats->stats_ptr);
			break;
		default:
			len += scnprintf(buf + len, size - len, "Unknown tagid: %u\n",
					 stats->tagid);
			break;
		}

		kfree(stats->stats_ptr);
		list_del(&stats->list);
		kfree(stats);
	}

	ret_val = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret_val;
}
static ssize_t ath12k_read_wmi_ctrl_path_stats(struct file *file,
		char __user *ubuf,
		size_t count, loff_t *ppos)
{
	return ath12k_read_all_wmi_ctrl_path_stats(file, ubuf, count, ppos);
}

static const struct file_operations ath12k_fops_wmi_ctrl_stats = {
	.write = ath12k_write_wmi_ctrl_path_stats,
	.open = ath12k_debugfs_open,
	.read = ath12k_read_wmi_ctrl_path_stats,
};

static void ath12k_debugfs_wmi_ctrl_stats_register(struct ath12k *ar)
{
	ath12k_debugfs_create_file("wmi_ctrl_stats", 0600,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &ath12k_fops_wmi_ctrl_stats);
	INIT_LIST_HEAD(&ar->debug.wmi_ctrl_path_stats.pdev_stats);
	INIT_LIST_HEAD(&ar->debug.period_wmi_list);
	spin_lock_init(&ar->debug.wmi_ctrl_path_stats_lock);
	init_completion(&ar->debug.wmi_ctrl_path_stats_rcvd);
	ar->debug.wmi_ctrl_path_stats_more_enabled = false;
}

static ssize_t ath12k_read_vdev_tid_stats(struct file *file,
					  char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	struct netdev_tid_stats *tstats;
	int len = 0;
	int size = 15000;
	char *buf;
	ssize_t ret;
	int cpu;
	u8 tid, reason;

	buf = vmalloc(size);
	if (!buf)
		return -ENOMEM;

	tstats = kzalloc(sizeof(*tstats), GFP_KERNEL);
	if (!tstats) {
		vfree(buf);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		struct pcpu_netdev_tid_stats *pstats = per_cpu_ptr(ahvif->tstats, cpu);

		u64_stats_update_begin(&pstats->syncp);
		for (u8 tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
			for (reason = 0; reason < ATH_RX_PKT_REASON_MAX; reason++) {
				tstats->tid_stats[tid].rx_pkt_stats[reason] +=
					pstats->tid_stats[tid].rx_pkt_stats[reason];
				tstats->tid_stats[tid].rx_pkt_bytes[reason] +=
					pstats->tid_stats[tid].rx_pkt_bytes[reason];
			}
			for (reason = 0; reason < ATH_RX_DROP_REASON_MAX; reason++) {
				tstats->tid_stats[tid].rx_drop_stats[reason] +=
					pstats->tid_stats[tid].rx_drop_stats[reason];
				tstats->tid_stats[tid].rx_drop_bytes[reason] +=
					pstats->tid_stats[tid].rx_drop_bytes[reason];
			}
			for (reason = 0; reason < ATH_TX_PKT_REASON_MAX; reason++) {
				tstats->tid_stats[tid].tx_pkt_stats[reason] +=
					pstats->tid_stats[tid].tx_pkt_stats[reason];
				tstats->tid_stats[tid].tx_pkt_bytes[reason] +=
					pstats->tid_stats[tid].tx_pkt_bytes[reason];
			}
			for (reason = 0; reason < ATH_TX_DROP_REASON_MAX; reason++) {
				tstats->tid_stats[tid].tx_drop_stats[reason] +=
					pstats->tid_stats[tid].tx_drop_stats[reason];
				tstats->tid_stats[tid].tx_drop_bytes[reason] +=
					pstats->tid_stats[tid].tx_drop_bytes[reason];
			}
		}
		u64_stats_update_end(&pstats->syncp);
	}

	len = scnprintf(buf + len, size - len, "\n\t\tath12k RX STATS\t\t\n");
	len += scnprintf(buf + len, size - len,
			 "TID \t packets bytes total_hw_pkts frag_pkts  reo_pkts  wbm_err_pkts reo_frag_pkts rxdma native  raw   eth   8023  ppe_vp  hw  sfe ppeds\n");
	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		len += scnprintf(buf + len, size - len, "TID%d\t", tid);
		len += scnprintf(buf + len, size - len, "%llu\t, %llu\t",
				 tstats->tid_stats[tid].rx_pkt_stats[0],
				 tstats->tid_stats[tid].rx_pkt_bytes[0]);
		for (reason = 1; reason < ATH_RX_PKT_REASON_MAX; reason++) {
			len += scnprintf(buf + len, size - len, "%d :%llu\t",
					 reason,
					 tstats->tid_stats[tid].rx_pkt_stats[reason]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}
	len += scnprintf(buf + len, size - len, "\n\t\tath12k RX DROP STATS\t\t\n");
	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		u64 total_drop = 0;
		u64 total_drop_bytes = 0;

		len += scnprintf(buf + len, size - len, "TID%d\t", tid);
		for (reason = 0; reason < ATH_RX_DROP_REASON_MAX; reason++) {
			total_drop += tstats->tid_stats[tid].rx_drop_stats[reason];
			total_drop_bytes += tstats->tid_stats[tid].rx_drop_bytes[reason];
		}
		len += scnprintf(buf + len, size - len, "%llu\t, %llu\t",
				 total_drop, total_drop_bytes);
		for (reason = 0; reason < ATH_RX_DROP_REASON_MAX; reason++) {
			len += scnprintf(buf + len, size - len, "%d:%llu\t",
					 reason,
					 tstats->tid_stats[tid].rx_drop_stats[reason]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}

	len += scnprintf(buf + len, size - len, "\n\t\tath12k TX STATS\t\t\n");
	len += scnprintf(buf + len, size - len,
			 "TID \t packets bytes sfe_pkts multicast  eapol  null_pkts  unicast null_complete fast_unicast wbm_rel fw_status  ppeds\n");
	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		len += scnprintf(buf + len, size - len, "TID%d\t", tid);
		len += scnprintf(buf + len, size - len, "%llu\t, %llu\t",
				 tstats->tid_stats[tid].tx_pkt_stats[0],
				 tstats->tid_stats[tid].tx_pkt_bytes[0]);
		for (reason = 1; reason < ATH_TX_PKT_REASON_MAX; reason++) {
			len += scnprintf(buf + len, size - len, "%d :%llu\t",
					 reason,
					 tstats->tid_stats[tid].tx_pkt_stats[reason]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}
	len += scnprintf(buf + len, size - len, "\n\t\tath12k TX DROP STATS\t\t\n");
	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		u64 total_drop = 0;
		u64 total_drop_bytes = 0;

		len += scnprintf(buf + len, size - len, "TID%d\t", tid);
		for (reason = 0; reason < ATH_TX_DROP_REASON_MAX; reason++) {
			total_drop += tstats->tid_stats[tid].tx_drop_stats[reason];
			total_drop_bytes += tstats->tid_stats[tid].tx_drop_bytes[reason];
		}
		len += scnprintf(buf + len, size - len, "%llu\t, %llu\t",
				 total_drop, total_drop_bytes);
		for (reason = 0; reason < ATH_TX_DROP_REASON_MAX; reason++) {
			len += scnprintf(buf + len, size - len, "%d:%llu\t",
					 reason,
					 tstats->tid_stats[tid].tx_drop_stats[reason]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	vfree(buf);
	kfree(tstats);
	return ret;
}

static const struct file_operations ath12k_fops_vdev_tid_stats = {
	.read = ath12k_read_vdev_tid_stats,
	.open = ath12k_debugfs_open,
};

static void ath12k_reset_vdev_tid_stats(struct ath12k_vif *ahvif)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct pcpu_netdev_tid_stats *pstats = per_cpu_ptr(ahvif->tstats, cpu);

		u64_stats_update_begin(&pstats->syncp);
		memset(pstats->tid_stats, 0, sizeof(pstats->tid_stats));
		u64_stats_update_end(&pstats->syncp);
	}
}

static ssize_t ath12k_write_reset_dp_tid_stats(struct file *file,
					       const char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	bool enable;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;
	if (enable)
		ath12k_reset_vdev_tid_stats(ahvif);
	else
		return -EINVAL;
	return count;
}

static const struct file_operations ath12k_fops_reset_dp_tid_stats = {
	.write = ath12k_write_reset_dp_tid_stats,
	.open = ath12k_debugfs_open,
};

void ath12k_debugfs_soc_destroy(struct ath12k_base *ab)
{
	struct dentry *debugfs_ath12k;
	struct dentry *debugfs_ath12k_compress_dump;

	if(!ab->debugfs_soc)
		return;

	debugfs_remove_recursive(ab->debugfs_soc);
	ab->debugfs_soc = NULL;

	/* Clean up compress_fw_dump file if it exists */
	debugfs_ath12k = debugfs_lookup("ath12k", NULL);
	if (debugfs_ath12k) {
		debugfs_ath12k_compress_dump = debugfs_lookup("compress_fw_dump",
							      debugfs_ath12k);
		debugfs_remove(debugfs_ath12k_compress_dump);
		dput(debugfs_ath12k);
	}

	/* We are not removing ath12k directory on purpose, even if it
	 * would be empty. This simplifies the directory handling and it's
	 * a minor cosmetic issue to leave an empty ath12k directory to
	 * debugfs.
	 */
}

int ath12k_update_dscp_tid_pdev(struct ath12k *ar, u8 id)
{
	struct ath12k_qos_map *qos_map;
	u8 dscp_low, dscp_high;
	u8 dscp;
	u8 tid, i;

	qos_map = ar->qos_map;

	for (i = 0; i < ATH12K_MAX_TID_VALUE; i++) {
		dscp_low = qos_map->up[i].low;
		dscp_high = qos_map->up[i].high;
		tid = i;

		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "dscp_low:%d, dscp_high:%d, tid:%d, id:%d\n",
				 dscp_low, dscp_high, tid, id);
		if (dscp_low == 0xFF || dscp_high == 0xFF)
			continue;

		for (dscp = dscp_low; dscp <= dscp_high; dscp++)
			ath12k_hal_tx_update_dscp_tid_map(ar->ab, id, dscp, tid);
	}

	if (qos_map->num_des > 0) {
		for (i = 0; i < qos_map->num_des; i++) {
			dscp = qos_map->dscp_exception[i].dscp;
			tid = qos_map->dscp_exception[i].up;

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					 "dscp:%d, tid:%d, id:%d\n", dscp, tid, id);
			if (dscp == 0xFF)
				continue;
			ath12k_hal_tx_update_dscp_tid_map(ar->ab, id, dscp, tid);
		}
	}
	return 0;
}

int ath12k_parse_qos_map(struct ath12k *ar, u8 *values, u8 len)
{
	struct ath12k_qos_map *qos_map;
	u8 num_des, des_len, i;
	int ret;

	qos_map = kzalloc(sizeof(*qos_map), GFP_KERNEL);
	if (!qos_map)
		return -ENOMEM;

	num_des = (len - ATH12K_QOS_MAP_LEN_MIN) >> 1;
	if (num_des) {
		des_len = num_des *
			  sizeof(struct ath12k_dscp_exception);
		memcpy(qos_map->dscp_exception, values, des_len);
		qos_map->num_des = num_des;
		for (i = 0; i < num_des; i++) {
			if (qos_map->dscp_exception[i].up > 7) {
				ret = -EINVAL;
				goto free_qos_map;
			}
		}
		values += des_len;
	}
	memcpy(qos_map->up, values, ATH12K_QOS_MAP_LEN_MIN);
	ar->qos_map = qos_map;
	return 0;

free_qos_map:
	kfree(qos_map);
	ar->qos_map = NULL;
	return ret;
}

static ssize_t ath12k_write_pdev_qos_map_set(struct file *file,
					     const char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_base *ab = ar->ab;
	char buf[256] = {0};
	char *token, *buf_ptr;
	u8 values[64];
	int value_count = 0;
	u8 id;
	int ret;
	int max_entries;

	if (count > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count - 1] = '\0';
	buf_ptr = buf;
	while ((token = strsep(&buf_ptr, ",")) != NULL) {
		ret = kstrtou8(token, 10, &values[value_count]);
		if (ret)
			return ret;
		value_count++;
		if (value_count >= ARRAY_SIZE(values))
			return -EINVAL;
	}

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	ret = ath12k_parse_qos_map(ar, values, value_count);
	if (ret)
		return ret;

	max_entries = ab->hal.hal_params->dscp_tid_map_tbl_max_entries;
	for (id = 0; id < max_entries; id++)
		ath12k_update_dscp_tid_pdev(ar, id);

	return count;
}

static ssize_t ath12k_read_pdev_qos_map_set(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[256] = {0};
	size_t len = 0;
	struct ath12k_qos_map *qos_map;
	int ret;
	u8 i;

	if (*ppos > 0)
		return 0;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	qos_map = ar->qos_map;

	if (!qos_map) {
		ath12k_warn(ar->ab, "qos_map_set is not set\n");
		return -EFAULT;
	}

	len += scnprintf(buf + len, sizeof(buf) - len, "%u,", qos_map->num_des);
	for (i = 0; i < qos_map->num_des; i++) {
		len += scnprintf(buf + len, sizeof(buf) - len, "%u,%u,",
				 qos_map->dscp_exception[i].dscp, qos_map->dscp_exception[i].up);
	}
	for (i = 0; i < 8; i++) {
		len += scnprintf(buf + len, sizeof(buf) - len, "%u,%u,",
				 qos_map->up[i].low, qos_map->up[i].high);
	}

	/* Remove the trailing comma */
	if (len > 0 && buf[len - 1] == ',') {
		buf[len - 1] = '\0';
		len--;
	}

	if (len < sizeof(buf) - 1)
		buf[len++] = '\n';

	ret = copy_to_user(user_buf, buf, len);
	if (ret)
		return -EFAULT;

	*ppos += len;

	return len;
}

static const struct file_operations fops_qos_map_set = {
	.read = ath12k_read_pdev_qos_map_set,
	.write = ath12k_write_pdev_qos_map_set,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_open_vdev_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_fw_stats_req_params param;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	void *buf;
	int ret;

	if (ar->ab->is_bypassed) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "device is in bypassed state\n");
		return -EPERM;
	}

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (!ah)
		return -ENETDOWN;

	if (ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	buf = vzalloc(ATH12K_FW_STATS_BUF_SIZE);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	/* VDEV stats is always sent for all active VDEVs from FW */
	param.vdev_id = 0;
	param.stats_id = WMI_REQUEST_VDEV_STAT;

	ret = ath12k_mac_get_fw_stats(ar, &param);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request fw vdev stats: %d\n", ret);
		vfree(buf);
		return ret;
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);

	file->private_data = buf;

	return 0;
}

static int ath12k_release_vdev_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_vdev_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_vdev_stats = {
	.open = ath12k_open_vdev_stats,
	.release = ath12k_release_vdev_stats,
	.read = ath12k_read_vdev_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_open_vdev_extd_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_fw_stats_req_params param;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	void *buf;
	int ret;

	if (ar->ab->is_bypassed) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "device is in bypassed state\n");
		return -EPERM;
	}

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (!ah)
		return -ENETDOWN;

	if (ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	buf = vzalloc(ATH12K_FW_STATS_BUF_SIZE);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	/* VDEV extd stats is always sent for all active VDEVs from FW */
	param.vdev_id = 0;
	param.stats_id = WMI_REQUEST_VDEV_EXTD_STAT;

	ret = ath12k_mac_get_fw_stats(ar, &param);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request fw vdev extd stats: %d\n", ret);
		vfree(buf);
		return ret;
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);

	file->private_data = buf;

	return 0;
}

static int ath12k_release_vdev_extd_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_vdev_extd_stats(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_vdev_extd_stats = {
	.open = ath12k_open_vdev_extd_stats,
	.release = ath12k_release_vdev_extd_stats,
	.read = ath12k_read_vdev_extd_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_open_bcn_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_link_vif *arvif;
	struct ath12k_fw_stats_req_params param;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	void *buf;
	int ret;

	if (ar->ab->is_bypassed) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "device is in bypassed state\n");
		return -EPERM;
	}

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ah && ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	buf = vzalloc(ATH12K_FW_STATS_BUF_SIZE);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	param.stats_id = WMI_REQUEST_BCN_STAT;

	ath12k_fw_stats_reset(ar);

	/* loop all active VDEVs for bcn stats */
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->is_up || ath12k_mac_is_bridge_vdev(arvif))
			continue;

		param.vdev_id = arvif->vdev_id;
		ret = ath12k_mac_get_fw_stats_per_vif(ar, &param);
		if (ret)
			ath12k_warn(ar->ab, "failed to request fw bcn stats: %d\n for vdev %d",
				    ret, arvif->vdev_id);
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);
	/* since beacon stats request is looped for all active VDEVs, saved fw
	 * stats is not freed for each request until done for all active VDEVs
	 */
	spin_lock_bh(&ar->data_lock);
	ath12k_fw_stats_bcn_free(&ar->fw_stats.bcn);
	spin_unlock_bh(&ar->data_lock);

	file->private_data = buf;

	return 0;
}

static int ath12k_release_bcn_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_bcn_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_bcn_stats = {
	.open = ath12k_open_bcn_stats,
	.release = ath12k_release_bcn_stats,
	.read = ath12k_read_bcn_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_open_pdev_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_base *ab = ar->ab;
	struct ath12k_fw_stats_req_params param;
	void *buf;
	int ret;

	if (ar->ab->is_bypassed) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "device is in bypassed state\n");
		return -EPERM;
	}

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ah && ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	buf = vzalloc(ATH12K_FW_STATS_BUF_SIZE);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	param.vdev_id = 0;
	param.stats_id = WMI_REQUEST_PDEV_STAT;

	ret = ath12k_mac_get_fw_stats(ar, &param);
	if (ret) {
		ath12k_warn(ab, "failed to request fw pdev stats: %d\n", ret);
		vfree(buf);
		return ret;
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);

	file->private_data = buf;

	return 0;
}

static int ath12k_release_pdev_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_pdev_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_pdev_stats = {
	.open = ath12k_open_pdev_stats,
	.release = ath12k_release_pdev_stats,
	.read = ath12k_read_pdev_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_enable_vdev_stats_offload(struct file *file,
						      const char __user *ubuf,
						      size_t count, loff_t *ppos)
{
       struct ath12k *ar = file->private_data;
       bool enable;
       int ret;

       if (kstrtobool_from_user(ubuf, count, &enable))
               return -EINVAL;

       guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

       if (enable == ar->fw_stats.en_vdev_stats_ol) {
               ret = count;
               goto out;
       }

       ar->fw_stats.en_vdev_stats_ol = enable;
       ret = count;

out:
       return ret;
}

static ssize_t ath12k_read_enable_vdev_stats_offload(struct file *file,
	       					     char __user *ubuf,
						     size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	struct ath12k *ar = file->private_data;
	int len = 0;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%u\n",
			ar->fw_stats.en_vdev_stats_ol);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_vdev_stats_offload = {
	.read = ath12k_read_enable_vdev_stats_offload,
	.write = ath12k_write_enable_vdev_stats_offload,
	.open = ath12k_debugfs_open
};

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static ssize_t ath12k_write_ppe_rfs_core_mask(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	struct ath12k_base *ab;
	u32 core_mask;
	int ret;

	if (ahvif && ahvif->deflink.ar)
		ab = ahvif->deflink.ar->ab;

	if (kstrtou32_from_user(user_buf, count, 0, &core_mask))
		return -EINVAL;

	if (core_mask > 0xF)
		return -EINVAL;

	wiphy_lock(ahvif->ah->hw->wiphy);

	if (core_mask == ahvif->dp_vif.ppe_core_mask)
		goto out;

	ret = ath12k_change_core_mask_for_ppe_rfs(ab, ahvif, core_mask);
	if (ret) {
		ath12k_warn(ab, "failed to change core_mask\n");
		goto out;
	}

out:
	ret = count;
	wiphy_unlock(ahvif->ah->hw->wiphy);
	return ret;
}

static ssize_t ath12k_read_ppe_rfs_core_mask(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	char buf[32] = {0};
	int len = 0;

	wiphy_lock(ahvif->ah->hw->wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%u\n",
			ahvif->dp_vif.ppe_core_mask);

	wiphy_unlock(ahvif->ah->hw->wiphy);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations ath12k_fops_rfs_core_mask = {
	.read = ath12k_read_ppe_rfs_core_mask,
	.write = ath12k_write_ppe_rfs_core_mask,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static inline char *get_ppe_str(int ppe_vp_type)
{
	char *type = NULL;

	switch (ppe_vp_type) {
	case PPE_VP_USER_TYPE_PASSIVE:
		type = "passive";
		break;
	case PPE_VP_USER_TYPE_ACTIVE:
		type = "active";
		break;
	case PPE_VP_USER_TYPE_DS:
		type = "ds";
		break;
	default:
		type = "unassigned";
		break;
	}
	return type;
}

int ath12k_print_arvif_link_stats(struct ath12k_vif *ahvif,
				  struct wireless_dev *wdev,
				  struct ath12k_link_vif *arvif,
				  struct ath12k_base *current_ab,
				  int link_id,
				  char *buf, int size)
{
	int len = 0;
	bool is_mldev;

	is_mldev = (hweight16(ahvif->vif->valid_links) > 1) ? true : false;

	len += scnprintf(buf + len, size - len, "%s netdev: %s vp_num %d vptype %s core_mask 0x%x addr %pM\n",
			is_mldev ? "ML" : "",
			wdev->netdev->name, ahvif->dp_vif.ppe_vp_num,
			get_ppe_str(ahvif->dp_vif.ppe_vp_type), ahvif->dp_vif.ppe_core_mask,
			arvif->bssid);

	if (is_mldev)
		len += scnprintf(buf + len, size - len, "Link ID:  %d %s\n",
				 link_id,
				 (arvif->ar->ab == current_ab) ? "current band" : "other band");

	len += scnprintf(buf + len, size - len, "\ttx_dropped: %d\n",
			arvif->link_stats.tx_dropped);
	len += scnprintf(buf + len, size - len, "\ttx_errors: %d\n",
			arvif->link_stats.tx_errors);
	len += scnprintf(buf + len, size - len, "\ttx_enqueued: %d\n",
			arvif->link_stats.tx_enqueued);
	len += scnprintf(buf + len, size - len, "\ttx_completed: %d\n",
			arvif->link_stats.tx_completed);
	len += scnprintf(buf + len, size - len, "\ttx_bcast_mcast: %d\n",
			arvif->link_stats.tx_bcast_mcast);
	len += scnprintf(buf + len, size - len, "\ttx_encap_type: %d %d %d\n",
			arvif->link_stats.tx_encap_type[0],
			arvif->link_stats.tx_encap_type[1],
			arvif->link_stats.tx_encap_type[2]);
	len += scnprintf(buf + len, size - len, "\ttx_desc_type: %d %d\n",
			arvif->link_stats.tx_desc_type[0],
			arvif->link_stats.tx_desc_type[1]);
	len += scnprintf(buf + len, size - len, "\trx_dropped: %d\n",
			arvif->link_stats.rx_dropped);
	len += scnprintf(buf + len, size - len, "\trx_errors: %d\n\n",
			arvif->link_stats.rx_errors);
	return len;
}

static ssize_t ath12k_debugfs_dump_ppeds_stats(struct file *file,
					       char __user *user_buf,
					       size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif, *arvif_partner;
	u8 i, j, link_id, index;
	bool interface_found;
	int *printed_if;
	int printedindex = 0;
	struct ath12k *ar;
	struct wireless_dev *wdev;
	struct ath12k_ppeds_stats *ppeds_stats = &ab->dp->ppe.ppeds_stats;
	int len = 0, retval;
	const int size = PAGE_SIZE;
	char *buf;
	u32 ppe2tcl_ring_id = dp->ppe.ppe2tcl_ring[0].ring_id;
	u32 reo2ppe_ring_id = dp->ppe.reo2ppe_ring[0].ring_id;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	printed_if = kcalloc(TARGET_NUM_VDEVS * ATH12K_MAX_SOCS,
			     sizeof(*printed_if), GFP_KERNEL);
	if (!printed_if) {
		kfree(buf);
		return -ENOMEM;
	}

	len += scnprintf(buf + len, size - len, "PPEDS_STATS:\n");
	len += scnprintf(buf + len, size - len, "tcl_prod_cnt= %u\n",
			 ppeds_stats->tcl_prod_cnt);
	len += scnprintf(buf + len, size - len, "tcl_cons_cnt= %u\n",
			 ppeds_stats->tcl_cons_cnt);
	len += scnprintf(buf + len, size - len, "reo_prod_cnt= %u\n",
			 ppeds_stats->reo_prod_cnt);
	len += scnprintf(buf + len, size - len, "reo_cons_cnt= %u\n",
			 ppeds_stats->reo_cons_cnt);
	len += scnprintf(buf + len, size - len, "get_tx_desc_cnt= %u\n",
			 ppeds_stats->get_tx_desc_cnt);
	len += scnprintf(buf + len, size - len, "enable_intr_cnt= %u\n",
			 ppeds_stats->enable_intr_cnt);
	len += scnprintf(buf + len, size - len, "disable_intr_cnt= %u\n",
			 ppeds_stats->disable_intr_cnt);
	len += scnprintf(buf + len, size - len, "release_tx_single_cnt= %u\n",
			 ppeds_stats->release_tx_single_cnt);
	len += scnprintf(buf + len, size - len, "release_rx_desc_cnt= %u\n",
			 ppeds_stats->release_rx_desc_cnt);
	len += scnprintf(buf + len, size - len, "tx_desc_allocated= %u\n",
			 ppeds_stats->tx_desc_allocated);
	len += scnprintf(buf + len, size - len, "tx_desc_alloc_fails= %u\n",
			 ppeds_stats->tx_desc_alloc_fails);
	len += scnprintf(buf + len, size - len, "tx_desc_freed= %u\n",
			 ppeds_stats->tx_desc_freed);
	len += scnprintf(buf + len, size - len, "fw2wbm_pkt_drops= %u\n",
			 ppeds_stats->fw2wbm_pkt_drops);
	len += scnprintf(buf + len, size - len, "num_rx_desc_freed= %u\n",
			 ppeds_stats->num_rx_desc_freed);
	len += scnprintf(buf + len, size - len, "num_rx_desc_realloc= %u\n",
			 ppeds_stats->num_rx_desc_realloc);

	len += scnprintf(buf + len, size - len, "\ntqm_rel_reason=");
	for (i = 0; i < HAL_WBM_TQM_REL_REASON_MAX; i++) {
		len += scnprintf(buf + len, size - len, " %u:%u", i,
				 ppeds_stats->tqm_rel_reason[i]);
	}
	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nSRNG_Ring_index_Dump:\n");

	srng = &ab->hal.srng_list[ppe2tcl_ring_id];
	if (srng && srng->initialized) {
		len += scnprintf(buf + len, size - len, "ppe2tcl_hp= 0x%x\n",
				srng->u.src_ring.hp);
		len += scnprintf(buf + len, size - len, "ppe2tcl_tp= 0x%x\n",
				 *(volatile u32 *)(srng->u.src_ring.tp_addr));
	}

	srng = &ab->hal.srng_list[reo2ppe_ring_id];
	if (srng && srng->initialized) {
		len += scnprintf(buf + len, size - len, "reo2ppe_hp= 0x%x\n",
				 *(volatile u32 *)(srng->u.dst_ring.hp_addr));
		len += scnprintf(buf + len, size - len, "reo2ppe_tp= 0x%x\n",
				 srng->u.dst_ring.tp);
	}

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		wiphy_lock(ar->ah->hw->wiphy);
		list_for_each_entry(arvif, &ar->arvifs, list) {
			interface_found = false;
			ahvif = arvif->ahvif;
			wdev = ieee80211_vif_to_wdev(ahvif->vif);
			if (!wdev || !wdev->netdev)
				continue;

			index = wdev->netdev->ifindex;

			/* Skip a netdevice if its information is already
			 * printed
			 */
			for (j = 0; j < printedindex; j++) {
				if (index == printed_if[j])
					interface_found = true;
			}

			if (interface_found)
				continue;

			printed_if[printedindex++] = index;
			if (hweight16(ahvif->vif->valid_links) <= 1)  {
				len += ath12k_print_arvif_link_stats(ahvif, wdev, arvif,
								     ab, 0, buf + len, size - len);
			} else {
				for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
					arvif_partner = ahvif->link[link_id];
					if (!arvif_partner)
						continue;

					if (arvif_partner->ar->ah != arvif->ar->ah)
						wiphy_lock(arvif_partner->ar->ah->hw->wiphy);

					len += ath12k_print_arvif_link_stats(ahvif, wdev, arvif_partner,
									     ab, link_id,
									     buf + len,
									     size - len);
					if (arvif_partner->ar->ah != arvif->ar->ah)
						wiphy_unlock(arvif_partner->ar->ah->hw->wiphy);
				}
			}
		}
		wiphy_unlock(ar->ah->hw->wiphy);
	}

	if (len > size)
		len = size;

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(printed_if);
	kfree(buf);

	return retval;
}

static ssize_t
ath12k_debugfs_write_ppeds_stats(struct file *file,
				 const char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_ppeds_stats *ppeds_stats = &ab->dp->ppe.ppeds_stats;
	char buf[20] = {0};
	int ret;

	if (count > sizeof(buf))
		return -EFAULT;

	ret = copy_from_user(buf, user_buf, count);
	if (ret)
		return -EFAULT;

	if (strstr(buf, "reset"))
		memset(ppeds_stats, 0, sizeof(struct ath12k_ppeds_stats));

	return count;
}

static const struct file_operations fops_ppeds_stats = {
	.read = ath12k_debugfs_dump_ppeds_stats,
	.write = ath12k_debugfs_write_ppeds_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};
#endif

static
void ath12k_debugfs_fw_stats_register(struct ath12k *ar)
{
	struct dentry *fwstats_dir = debugfs_create_dir("fw_stats",
							ar->debug.debugfs_pdev);

	/* all stats debugfs files created are under "fw_stats" directory
	 * created per PDEV
	 */
	debugfs_create_file("vdev_stats", 0600, fwstats_dir,
			    ar, &fops_vdev_stats);
	debugfs_create_file("beacon_stats", 0600, fwstats_dir,
			    ar, &fops_bcn_stats);
	debugfs_create_file("pdev_stats", 0600, fwstats_dir,
			    ar, &fops_pdev_stats);
	debugfs_create_file("en_vdev_stats_ol", 0600, fwstats_dir,
			    ar, &fops_vdev_stats_offload);
	debugfs_create_file("vdev_extd_stats", 0600, fwstats_dir,
			    ar, &fops_vdev_extd_stats);

	ath12k_fw_stats_init(ar);
}

static ssize_t ath12k_read_wmm_stats(struct file *file,
				     char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev_dp *dp_pdev = NULL;
	int len = 0;
	int size = 2048;
	char *buf;
	ssize_t retval;
	u64 total_wmm_sent_pkts = 0;
	u64 total_wmm_received_pkts = 0;
	u64 total_wmm_fail_sent = 0;
	u64 total_wmm_fail_received = 0;

	if (!dp) {
		ath12k_warn(ar->ab, "ath12k_dp not present%s\n", __func__);
		return -ENOMEM;
	}

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		ath12k_warn(dp, "failed to allocate the buffer%s\n", __func__);
		return -ENOMEM;
	}

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, ar->pdev_idx);
	if (!dp_pdev) {
		rcu_read_unlock();
		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		ath12k_warn(dp, "dp_pdev not present for pdev_idx %d in %s\n",
			    ar->pdev_idx, __func__);
		return -ENOMEM;
	}

	for (count = 0; count < WME_NUM_AC; count++) {
		total_wmm_sent_pkts += dp_pdev->wmm_stats.total_wmm_tx_pkts[count];
		total_wmm_received_pkts += dp_pdev->wmm_stats.total_wmm_rx_pkts[count];
		total_wmm_fail_sent += dp_pdev->wmm_stats.total_wmm_tx_drop[count];
		total_wmm_fail_received += dp_pdev->wmm_stats.total_wmm_rx_drop[count];
	}

	len += scnprintf(buf + len, size - len, "TEST Total number of wmm_sent: %llu\n",
			 total_wmm_sent_pkts);
	len += scnprintf(buf + len, size - len, "total number of wmm_received: %llu\n",
			 total_wmm_received_pkts);
	len += scnprintf(buf + len, size - len, "total number of wmm_fail_sent: %llu\n",
			 total_wmm_fail_sent);
	len += scnprintf(buf + len, size - len, "total number of wmm_fail_received: %llu\n",
			 total_wmm_fail_received);
	len += scnprintf(buf + len, size - len, "Num of BE wmm_sent: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "Num of BK wmm_sent: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "Num of VI wmm_sent: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "Num of VO wmm_sent: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_VO]);
	len += scnprintf(buf + len, size - len, "num of be wmm_received: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_received: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_received: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_received: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_VO]);
	len += scnprintf(buf + len, size - len, "num of be wmm_tx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_tx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_tx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_tx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_VO]);
	len += scnprintf(buf + len, size - len, "num of be wmm_rx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_rx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_rx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_rx_dropped: %llu\n",
			 dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_VO]);

	rcu_read_unlock();
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	if (len > size)
		len = size;
	retval = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_wmm_stats = {
	.read = ath12k_read_wmm_stats,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_athdiag_read(struct file *file,
				    char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	u8 *buf;
	int ret;

	if (*ppos <= 0)
		return -EINVAL;

	if (!count)
		return 0;

	wiphy_lock(wiphy);

	buf = vmalloc(count);
	if (!buf) {
		return -ENOMEM;
	}

	ret = ath12k_qmi_mem_read(ar->ab, *ppos, buf, count);
	if (ret < 0) {
		ath12k_warn(ar->ab, "failed to read address 0x%08x via diagnose window from debugfs: %d\n",
			    (u32)(*ppos), ret);
		goto exit;
	}

	ret = copy_to_user(user_buf, buf, count);
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	count -= ret;
	*ppos += count;
	ret = count;

exit:
	vfree(buf);
	wiphy_unlock(wiphy);
	return ret;
}

static ssize_t ath12k_athdiag_write(struct file *file,
				    const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	u8 *buf;
	int ret;

	if (*ppos <= 0)
		return -EINVAL;

	if (!count)
		return 0;

	wiphy_lock(wiphy);

	buf = vmalloc(count);
	if (!buf) {
		ret = -ENOMEM;
		goto error_unlock;
	}

	ret = copy_from_user(buf, user_buf, count);
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	ret = ath12k_qmi_mem_write(ar->ab, *ppos, buf, count);
	if (ret < 0) {
		ath12k_warn(ar->ab, "failed to write address 0x%08x via diagnose window from debugfs: %d\n",
			     (u32)(*ppos), ret);
		goto exit;
	}

	*ppos += count;
	ret = count;

exit:
	vfree(buf);

error_unlock:
	wiphy_unlock(wiphy);
	return ret;
}

static const struct file_operations fops_athdiag = {
	.read = ath12k_athdiag_read,
	.write = ath12k_athdiag_write,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_enable_m3_dump(struct file *file,
                                           const char __user *ubuf,
                                           size_t count, loff_t *ppos)
{
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        bool enable;
        int ret;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	wiphy_lock(wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (enable == ar->debug.enable_m3_dump) {
		ret = count;
		goto exit;
	}

	ret = ath12k_wmi_pdev_m3_dump_enable(ar, enable);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to enable m3 ssr dump %d\n",
			    ret);
		goto exit;
	}

	ar->debug.enable_m3_dump = enable;
	ret = count;

exit:
	wiphy_unlock(wiphy);
	return ret;
}

static ssize_t ath12k_read_enable_m3_dump(struct file *file,
					  char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[32];
	size_t len = 0;

	wiphy_lock(wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->debug.enable_m3_dump);
	wiphy_unlock(wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);

}

static const struct file_operations fops_enable_m3_dump = {
	.read = ath12k_read_enable_m3_dump,
	.write = ath12k_write_enable_m3_dump,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
};

static ssize_t ath12k_write_btcoex(struct file *file,
                                   const char __user *ubuf,
                                   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
	size_t buf_size;
	int ret = 0,coex = BTCOEX_CONFIGURE_DEFAULT, wlan_weight = 0,
	    wlan_prio_mask_value = 0;
	enum qca_wlan_priority_type wlan_prio_mask = QCA_WLAN_PRIORITY_BE;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto exit;
	}

	buf[buf_size] = '\0';
	ret = sscanf(buf, "%d %d %d" , &coex, &wlan_prio_mask_value, &wlan_weight);

	if (!ret) {
		ret = -EINVAL;
		goto exit;
	}

	if (coex != BTCOEX_ENABLE &&  coex != BTCOEX_CONFIGURE_DEFAULT && coex) {
		ret = -EINVAL;
		goto exit;
	}

	wiphy_lock(wiphy);

	switch (coex) {
		case BTCOEX_ENABLE:
			if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags))
				set_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags);
			break;
		case BTCOEX_CONFIGURE_DEFAULT:
			if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
				ret = -EINVAL;
				goto error_unlock;
			}
			break;
		case BTCOEX_DISABLE:
			clear_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags);
			break;
		default:
			ret = -EINVAL;
			goto error_unlock;
	}

	if ((wlan_weight < BTCOEX_CONFIGURE_DEFAULT) ||
			(wlan_prio_mask_value < BTCOEX_CONFIGURE_DEFAULT) ||
			(wlan_weight > BTCOEX_MAX_PKT_WEIGHT) ||
			(wlan_prio_mask_value > BTCOEX_MAX_WLAN_PRIORITY)) {
		ret = -EINVAL;
		goto error_unlock;
	}

	if (wlan_weight == BTCOEX_CONFIGURE_DEFAULT)
		wlan_weight = ar->coex.wlan_weight;

	wlan_prio_mask = ((wlan_prio_mask_value == BTCOEX_CONFIGURE_DEFAULT)?
			   ar->coex.wlan_prio_mask : wlan_prio_mask_value);

	if (ar->ah->state != ATH12K_HW_STATE_ON && ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
		ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		ret = -ENETDOWN;
		goto error_unlock;
	}

	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

	if (!arvif->is_started) {
		ret = -EINVAL;
		goto error_unlock;
	}

	ret = ath12k_mac_btcoex_config(ar, arvif, coex, wlan_prio_mask, wlan_weight);

	if (ret) {
		ath12k_warn(ar->ab,
				"failed to enable coex vdev_id %d ret %d\n",
				arvif->vdev_id, ret);
		goto error_unlock;
	}

	ar->coex.wlan_prio_mask = wlan_prio_mask;
	ar->coex.wlan_weight = wlan_weight;
	ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
	return ret;
}

static ssize_t ath12k_read_btcoex(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
	int len = 0;

	if (!ar)
		return -EINVAL;

	wiphy_lock(wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%u %u %u\n",
			test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags),
			ar->coex.wlan_prio_mask,
			ar->coex.wlan_weight);
	wiphy_unlock(wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex = {
	.read = ath12k_read_btcoex,
	.write = ath12k_write_btcoex,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_write_btcoex_duty_cycle(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	struct coex_config_arg coex_config;
	char buf[256] = {0};
	size_t buf_size;
	u32 duty_cycle = 0, wlan_duration = 0;
	int ret = 0;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto exit;
	}

	buf[buf_size] = '\0';
	ret = sscanf(buf, "%u %u", &duty_cycle, &wlan_duration);

	if (!ret) {
		ret = -EINVAL;
		goto exit;
	}

	/*Maximum duty_cycle period allowed is 100 Miliseconds*/
	if (duty_cycle < wlan_duration || !duty_cycle || !wlan_duration || duty_cycle > 100000) {
		ret = -EINVAL;
		goto exit;
	}

	wiphy_lock(wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON && ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
		ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		ret = -ENETDOWN;
		goto error_unlock;
	}

	if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
		ret = -EINVAL;
		goto error_unlock;
	}

	if (ar->coex.coex_algo_type != COEX_ALGO_OCS) {
		ath12k_err(ar->ab,"duty cycle algo is not enabled");
		ret = -EINVAL;
		goto error_unlock;
	}

	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

	if (!arvif->is_started) {
		ret = -EINVAL;
		goto error_unlock;
	}

	coex_config.vdev_id = arvif->vdev_id;
	coex_config.config_type = WMI_COEX_CONFIG_AP_TDM;
	coex_config.duty_cycle = duty_cycle;
	coex_config.wlan_duration = wlan_duration;
	wiphy_unlock(wiphy);

	ret = ath12k_send_coex_config_cmd(ar, &coex_config);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to set duty cycle vdev_id %d ret %d\n",
			    coex_config.vdev_id, ret);
		goto exit;
	}

	wiphy_lock(wiphy);
	ar->coex.duty_cycle = duty_cycle;
	ar->coex.wlan_duration = wlan_duration;
	ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
	return ret;
}

static ssize_t ath12k_read_btcoex_duty_cycle(struct file *file, char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
	int len = 0;

	if (!ar)
		return -EINVAL;

	wiphy_lock(wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d %d\n",
			ar->coex.duty_cycle,ar->coex.wlan_duration);
	wiphy_unlock(wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_duty_cycle = {
	.read = ath12k_read_btcoex_duty_cycle,
	.write = ath12k_write_btcoex_duty_cycle,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_write_btcoex_algo(struct file *file,
                                        const char __user *ubuf,
                                        size_t count, loff_t *ppos)
{
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        struct coex_config_arg coex_config;
        char buf[256] = {0};
        size_t buf_size;
        u32 pta_num = 0, coex_mode = 0, bt_txrx_time  = 0,
        bt_priority_time = 0, pta_algorithm = 0,
        pta_priority = 0;
        int ret = 0;

        if (!ar) {
                ret = -EINVAL;
                goto exit;
        }

        buf_size = min(count, (sizeof(buf) - 1));

        if (copy_from_user(buf, ubuf, buf_size)) {
                ret = -EFAULT;
                goto exit;
        }

        buf[buf_size] = '\0';
        ret = sscanf(buf, "%u 0x%x 0x%x 0x%x 0x%x 0x%x" , &pta_num, &coex_mode,
                     &bt_txrx_time, &bt_priority_time,
                     &pta_algorithm, &pta_priority);

        if (!ret) {
                ret = -EINVAL;
                goto exit;
        }

        if (coex_mode > BTCOEX_PTA_MODE ||
            coex_mode < BTCOEX_THREE_WIRE_MODE ||
            pta_algorithm >= COEX_ALGO_MAX_SUPPORTED) {
                ret = -EINVAL;
                goto exit;
        }

	wiphy_lock(wiphy);

        if (ar->ah->state != ATH12K_HW_STATE_ON &&
            ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
                ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
                ret = -ENETDOWN;
                goto error_unlock;
        }

        if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
                ret = -EINVAL;
                goto error_unlock;
        }

        arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

        if (!arvif->is_started) {
                ret = -EINVAL;
                goto error_unlock;
        }

        coex_config.vdev_id            = arvif->vdev_id;
        coex_config.config_type        = WMI_COEX_CONFIG_PTA_INTERFACE;
        coex_config.pta_num            = pta_num;
        coex_config.coex_mode          = coex_mode;
        coex_config.bt_txrx_time       = bt_txrx_time;
        coex_config.bt_priority_time   = bt_priority_time;
        coex_config.pta_algorithm      = pta_algorithm;
        coex_config.pta_priority       = pta_priority;
	wiphy_unlock(wiphy);

        ret = ath12k_send_coex_config_cmd(ar, &coex_config);

        if (ret) {
                ath12k_warn(ar->ab,
                            "failed to set coex algorithm vdev_id %d ret %d\n",
                            coex_config.vdev_id, ret);
                goto exit;
        }

	wiphy_lock(wiphy);
	ar->coex.pta_num                =   pta_num;
	ar->coex.coex_mode              =   coex_mode;
	ar->coex.bt_active_time_slot    =   bt_txrx_time;
	ar->coex.bt_priority_time_slot  =   bt_priority_time;
	ar->coex.pta_algorithm          =   pta_algorithm;
	ar->coex.pta_priority           =   pta_priority;
	ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
	return ret;
}

static ssize_t ath12k_read_btcoex_algo(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
        int len = 0;

	wiphy_lock(wiphy);
        len = scnprintf(buf, sizeof(buf) - len, "%u %u %u %u %u %u\n",
                        ar->coex.pta_num, ar->coex.coex_mode,
                        ar->coex.bt_active_time_slot,
                        ar->coex.bt_priority_time_slot,
                        ar->coex.pta_algorithm, ar->coex.pta_priority);
	wiphy_unlock(wiphy);

        return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_algo = {
	.read = ath12k_read_btcoex_algo,
	.write = ath12k_write_btcoex_algo,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_btcoex_pkt_priority_write(struct file *file,
                                          const char __user *ubuf,
                                          size_t count, loff_t *ppos)
{
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        struct ath12k_link_vif *arvif = NULL;
        struct coex_config_arg coex_config;
        char buf[128] = {0};
        size_t buf_size;
        enum qca_wlan_priority_type wlan_pkt_type = 0;
        u32 wlan_pkt_type_continued = 0, wlan_pkt_weight = 0,
        bt_pkt_weight = 0;
        int ret;

        buf_size = min(count, (sizeof(buf) - 1));

        if (copy_from_user(buf, ubuf, buf_size)) {
                ret = -EFAULT;
                goto exit;
        }

        buf[buf_size] = '\0';
        ret = sscanf(buf, "%u %u %u %u" , &wlan_pkt_type,
                     &wlan_pkt_type_continued, &wlan_pkt_weight,
                     &bt_pkt_weight);

        if (!ret) {
                ret = -EINVAL;
                goto exit;
        }

        if (wlan_pkt_type > QCA_WLAN_PRIORITY_MGMT ||
            wlan_pkt_weight > BTCOEX_MAX_PKT_WEIGHT  ||
            bt_pkt_weight > BTCOEX_MAX_PKT_WEIGHT) {
                ret = -EINVAL;
                goto exit;
        }

	wiphy_lock(wiphy);

        if (ar->ah->state != ATH12K_HW_STATE_ON &&
            ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
                ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
                ret = -ENETDOWN;
                goto error_unlock;
        }

        if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
                ret = -EINVAL;
                goto error_unlock;
        }

        arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

        if (!arvif->is_started) {
                ret = -EINVAL;
                goto error_unlock;
        }

        coex_config.vdev_id                  = arvif->vdev_id;
        coex_config.config_type              = WMI_COEX_CONFIG_WLAN_PKT_PRIORITY;
        coex_config.wlan_pkt_type            = wlan_pkt_type;
        coex_config.wlan_pkt_type_continued  = wlan_pkt_type_continued;
        coex_config.wlan_pkt_weight          = wlan_pkt_weight;
        coex_config.bt_pkt_weight            = bt_pkt_weight;
	wiphy_unlock(wiphy);

        ret = ath12k_send_coex_config_cmd(ar, &coex_config);

        if (ret) {
                ath12k_warn(ar->ab,
                            "failed to set coex pkt priority vdev_id %d ret %d\n",
                            coex_config.vdev_id, ret);
                goto exit;
        }

	wiphy_lock(wiphy);
        ar->coex.wlan_pkt_type              = wlan_pkt_type;
        ar->coex.wlan_pkt_type_continued    = wlan_pkt_type_continued;
        ar->coex.wlan_weight                = wlan_pkt_weight;
        ar->coex.bt_weight                  = bt_pkt_weight;

        ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
        return ret;
}

static ssize_t ath12k_btcoex_pkt_priority_read(struct file *file,
                                         char __user *ubuf,
                                         size_t count, loff_t *ppos)
{
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        u8 buf[128] = {0};
        size_t len = 0;

	wiphy_lock(wiphy);
        len = scnprintf(buf, sizeof(buf) - len,
                        "%u %u %u %u\n",ar->coex.wlan_pkt_type,
                        ar->coex.wlan_pkt_type_continued, ar->coex.wlan_weight,
                        ar->coex.bt_weight);
        wiphy_unlock(wiphy);

        return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_priority = {
	.read = ath12k_btcoex_pkt_priority_read,
	.write = ath12k_btcoex_pkt_priority_write,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

/**
 * ath12k_write_afc_grace_timer_value() - Read the user programmed afc
 * grace timer value and send it to firmware.
 * @file: file pointer
 * @user_buf: user buffer
 * @count: size of the buffer
 * @ppos: file position
 *
 * Return: 0 on success, negative error code on failure
 */
static ssize_t
ath12k_write_afc_grace_timer_value(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	u32 afc_grace_timer_expiry_value;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ath12k_warn(ar->ab, "Interface not up\n");
		ret = -ENETDOWN;
		goto exit;
	}

	if (kstrtou32_from_user(user_buf, count, 0, &afc_grace_timer_expiry_value)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = ath12k_wmi_set_afc_grace_timer(ar, afc_grace_timer_expiry_value);
	if (ret)
		goto exit;

	ret = count;

exit:
	return ret;
}

static ssize_t ath12k_write_simulate_awgn(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	u32 chan_bw_interference_bitmap;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ath12k_warn(ar->ab, "Interface not up\n");
		return -ENETDOWN;
	}

	if (kstrtou32_from_user(user_buf, count, 0, &chan_bw_interference_bitmap))
		return -EINVAL;

	ret = ath12k_wmi_simulate_awgn(ar, chan_bw_interference_bitmap);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_simulate_awgn = {
	.write = ath12k_write_simulate_awgn,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_read_scan_args_config(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int len = 0;
	char buf[64] = {0};

	len += scnprintf(buf + len, sizeof(buf) - len, "min_rest_time: %u\n",
			 ar->scan_min_rest_time);
	len += scnprintf(buf + len, sizeof(buf) - len, "max_rest_time: %u\n",
			 ar->scan_max_rest_time);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_scan_args_config(struct file *file,
					     const char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	unsigned int scan_params[2] = {0};
	u8 buf[64] = {0};

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';

	ret = sscanf(buf, "%u %u\n", &scan_params[0], &scan_params[1]);
	if (ret != 2)
		return -EINVAL;

	if (scan_params[0] == scan_params[1]) {
		ath12k_err(ar->ab, "min and max rest time shouldn't be same\n");
		return -EINVAL;
	}

	if (scan_params[0] < 50 || scan_params[0] > 500) {
		ath12k_err(ar->ab, "min rest time between 50 to 500\n");
		return -EINVAL;
	}

	if (scan_params[1] < scan_params[0] || scan_params[1] > 500) {
		ath12k_err(ar->ab, "max rest time between min rest time to 500\n");
		return -EINVAL;
	}

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	ar->scan_min_rest_time = scan_params[0];
	ar->scan_max_rest_time = scan_params[1];
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return count;
}

static const struct file_operations fops_scan_args_config = {
	.read = ath12k_read_scan_args_config,
	.write = ath12k_write_scan_args_config,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_configure_afc_grace_timer = {
	.write = ath12k_write_afc_grace_timer_value,
	.open = ath12k_debugfs_open
};

static ssize_t
ath12k_write_pktlog_remote_enable(struct file *file,
				  const char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u32 enable;
	int ret;

	if (kstrtouint_from_user(ubuf, count, 0, &enable))
		return -EINVAL;

	mutex_lock(&ar->ab->core_lock);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	ret = ath12k_pktlog_remote_enable(ar, enable);
	if (ret) {
		ath12k_warn(ar->ab, "failed to %s remote pktlog: %d\n",
			    enable ? "enable" : "disable", ret);
		goto out;
	}

	ret = count;

out:
	mutex_unlock(&ar->ab->core_lock);
	return ret;
}

static ssize_t
ath12k_read_pktlog_remote_enable(struct file *file,
				 char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *pl_info = &ar->debug.pktlog;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%d\n",
			!!(pl_info->filter & ATH12K_PKTLOG_REMOTE_ENABLE));

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_remote_enable = {
	.read = ath12k_read_pktlog_remote_enable,
	.write = ath12k_write_pktlog_remote_enable,
	.open = ath12k_debugfs_open
};

static ssize_t
ath12k_write_pktlog_remote_ip(struct file *file,
			      const char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *pl_info = &ar->debug.pktlog;
	char buf[ATH12K_IP_ADDR_STR_MAX];
	ssize_t ret;

	if (count >= ATH12K_IP_ADDR_STR_MAX)
		return -EINVAL;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	if (ret > 0 && buf[ret - 1] == '\n')
		buf[ret - 1] = '\0';

	if (buf[0] != '\0') {
		u8 ip[4];
		int parsed = sscanf(buf, "%hhu.%hhu.%hhu.%hhu",
				    &ip[0], &ip[1], &ip[2], &ip[3]);
		if (parsed != 4) {
			ath12k_warn(ar->ab, "Invalid IP address format: %s\n", buf);
			return -EINVAL;
		}
	}

	mutex_lock(&ar->ab->core_lock);
	spin_lock_bh(&pl_info->lock);
	memcpy(pl_info->ipaddr, buf, ATH12K_IP_ADDR_STR_MAX);
	pl_info->pktlog_remote_client = (buf[0] != '\0');
	spin_unlock_bh(&pl_info->lock);
	mutex_unlock(&ar->ab->core_lock);

	return count;
}

static ssize_t
ath12k_read_pktlog_remote_ip(struct file *file,
			     char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *pl_info = &ar->debug.pktlog;
	char buf[ATH12K_IP_ADDR_STR_MAX + 2];
	int len;

	len = scnprintf(buf, sizeof(buf), "%s\n", pl_info->ipaddr);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_remote_ip = {
	.read = ath12k_read_pktlog_remote_ip,
	.write = ath12k_write_pktlog_remote_ip,
	.open = ath12k_debugfs_open
};

static ssize_t
ath12k_read_pktlog_remote_port(struct file *file,
			       char __user *ubuf,
			       size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *pl_info = &ar->debug.pktlog;
	char buf[32];
	int len;

	if (!pl_info->rpktlog_svc) {
		ath12k_err(ar->ab, "Remote pktlog service not initialized\n");
		return -ENOMEM;
	}

	len = scnprintf(buf, sizeof(buf), "%u\n", pl_info->rpktlog_svc->port);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_remote_port = {
	.read = ath12k_read_pktlog_remote_port,
	.open = ath12k_debugfs_open
};

/**
 * ath12k_write_pktlog_filter() - This function is responsible for setting
 * the pktlog mode and htt tlv filter for different pktlog flavors.
 *
 * User needs to pass the combination of at least one mode and one filter
 * while configuring pktlog, such as:
 * {rx, full}/ {rx,lite}/ {tx,rx,lite}/ {tx,rx,lite,rcu,rcf} etc.
 *
 * pktlog modes:
 * Full: Pktlog full version can be used to capture all PPDU and MPDU TLVs
 * Lite: Pktlog lite is a lighter version of pktlog, which captures mostly
 *       PPDU TLVs and MPDU START TLV.
 *
 * pktlog filters: Rx/ Tx/ RCU/ RCF/ Hybrid
 *
 * EX:
 * 1. pktlog full rx:
 * echo "rx, full" > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog_filter
 * 2. pktlog lite tx/rx:
 * echo "lite" > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog_filter
 * 3. pktlog lite tx/rx/rcu/rcf:
 * echo "lite,rcu,rcf" > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog_filter
 *
 * To reset pktlog filters and mode:
 * echo "reset" > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog_filter
 */
static ssize_t ath12k_write_pktlog_filter(struct file *file,
                                          const char __user *ubuf,
                                          size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_base *ab = ar->ab;
	enum ath12k_pktlog_mode mode = ATH12K_PKTLOG_MODE_INVALID;
	u32 filter = 0;
	u8 buf[128] = {0};
	int ret = 0;
	ssize_t rc;
	bool enable = true;
	bool pktlog_buf_allocated = false;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!ath12k_ftm_mode &&
	    ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (rc < 0) {
		ret = rc;
		goto exit;
	}
	buf[rc] = '\0';

	if (strstr(buf, "reset")) {
		mode = ATH12K_PKTLOG_DISABLED;
		enable = false;
	} else if (strstr(buf, "full")) {
		mode = ATH12K_PKTLOG_MODE_FULL;
		if (strstr(buf, "tx"))
			filter |= ATH12K_PKTLOG_TX;
		if (strstr(buf, "rx"))
			filter |= ATH12K_PKTLOG_RX;
	} else if (strstr(buf, "lite"))
		mode = ATH12K_PKTLOG_MODE_LITE;
	else {
		ath12k_err(ab, "Invalid mode: %d", mode);
		ret = -EINVAL;
		goto exit;
	}

	if (enable) {
		if (strstr(buf, "rcf"))
			filter |= ATH12K_PKTLOG_RCFIND;
		if (strstr(buf, "rcu"))
			filter |= ATH12K_PKTLOG_RCUPDATE;
		if (strstr(buf, "hybrid"))
			filter |= ATH12K_PKTLOG_HYBRID;
		if (strstr(buf, "phy"))
			filter |= ATH12K_PKTLOG_PHY_LOGGING;
		if (strstr(buf, "cbf"))
			filter |= ATH12K_PKTLOG_CBF;
	}

	if (enable) {
		ret = ath12k_pktlog_buf_alloc_if_needed(ar, &pktlog_buf_allocated);
		if (ret) {
			ath12k_err(ab, "Failed to alloc pktlog buf: %d\n", ret);
			goto exit;
		}
	}

	ath12k_dp_mon_pktlog_config(ar, enable, mode, filter);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret) {
		ath12k_err(ab, "Failed to configure pktlog filters\n");
		ath12k_dp_mon_pktlog_config(ar, false, mode, filter);
		ath12k_pktlog_buf_release_if_allocated(ar, pktlog_buf_allocated);
		goto exit;
	}

	if (filter & ATH12K_PKTLOG_HYBRID) {
		ret = ath12k_dp_mon_tx_update_filter(ar);
		if (ret) {
			ath12k_err(ab, "Failed to configure pktlog filters\n");
			ath12k_dp_mon_pktlog_config(ar, false, mode, filter);
			ath12k_pktlog_buf_release_if_allocated(ar, pktlog_buf_allocated);
			goto exit;
		}
	}

	ar->debug.pktlog_filter = filter;
	ar->debug.pktlog_mode = mode;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "pktlog filter %d mode %s\n",
		   filter, ((mode == ATH12K_PKTLOG_MODE_FULL) ? "full" :
			    (mode == ATH12K_PKTLOG_MODE_LITE) ? "lite" :
			    "disabled"));
	ret = count;
exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_read_pktlog_filter(struct file *file,
                                         char __user *ubuf,
                                         size_t count, loff_t *ppos)

{
        u8 buf[32] = {0};
        struct ath12k *ar = file->private_data;
        int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!ath12k_ftm_mode && ar->ah->state != ATH12K_HW_STATE_ON) {
		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		return -ENETDOWN;
	}

        len = scnprintf(buf, sizeof(buf) - len, "%08x %08x\n",
                        ar->debug.pktlog_filter,
                        ar->debug.pktlog_mode);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

        return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_filter = {
        .read = ath12k_read_pktlog_filter,
        .write = ath12k_write_pktlog_filter,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_write_qos_stats(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar = NULL;
	struct ieee80211_hw *hw = NULL;
	u8 qos_stats, stats_categ, stats_lvl, cur_stats_lvl;
	int ret, i;

	if (kstrtou8_from_user(ubuf, count, 0, &qos_stats))
		return -EINVAL;

	if (qos_stats > ATH12K_QOS_STATS_MAX) {
		ath12k_err(NULL, "Invalid QoS stats\n");
		return -EINVAL;
	}

	if (!ah) {
		ath12k_err(NULL, "Radio references not available");
		return -ENOENT;
	}

	hw = ah->hw;
	if (!hw)
		return -ENOENT;

	wiphy_lock(hw->wiphy);

	ret = count;

	stats_lvl = qos_stats & ATH12K_QOS_STATS_COLLECTION_MASK;
	stats_categ = qos_stats & ATH12K_QOS_STATS_CATEG_MASK;

	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->ab->is_bypassed)
			continue;
		if (ar) {
			cur_stats_lvl = ar->dp.qos_stats &
					ATH12K_QOS_STATS_COLLECTION_MASK;
			if (ar->allocated_vdev_map &&
			    stats_lvl != cur_stats_lvl) {
				qos_stats = cur_stats_lvl | stats_categ;
				ath12k_err(ar->ab, "qos stats collection lvl not updated, interfaces up\n");
				break;
			}
		}
	}

	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->ab->is_bypassed)
			continue;
		if (ar)
			ar->dp.qos_stats = qos_stats;
	}

	wiphy_unlock(hw->wiphy);
	return ret;
}

static ssize_t ath12k_read_qos_stats(struct file *file,
				     char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar = NULL;
	struct ieee80211_hw *hw = NULL;
	int len = 0;
	char buf[32] = {0};

	if (!ah) {
		ath12k_err(NULL, "ah references not available");
		return -ENOENT;
	}

	hw = ah->hw;
	if (!hw)
		return -ENOENT;

	wiphy_lock(hw->wiphy);
	ar = &ah->radio[0];
	if (!ar) {
		ath12k_err(NULL, "Radio references not available");
		wiphy_unlock(hw->wiphy);
		return -ENOENT;
	}
	len = scnprintf(buf, sizeof(buf) - len, "%08x\n",
				ar->dp.qos_stats);

	wiphy_unlock(hw->wiphy);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_qos_stats = {
	.read = ath12k_read_qos_stats,
	.write = ath12k_write_qos_stats,
	.open = ath12k_debugfs_open
};

static void ath12k_dp_peer_reset_delay_stats(struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_peer_delay_stats *delay_stats;
	struct ath12k_dp_peer_delay_tid_stats *delay_tid_stats;
	struct ath12k_dp_peer_delay_tx_stats *tx_delay;
	struct ath12k_dp_peer_delay_rx_stats *rx_delay;
	u8 tid, ring_id;

	if (!dp_peer || !dp_peer->mld_stats.delay_stats)
		return;

	delay_stats = dp_peer->mld_stats.delay_stats;

	rcu_read_lock();
	for (tid = 0; tid < DP_TID_MAX; tid++) {
		for (ring_id = 0; ring_id < DP_REO_DST_RING_MAX; ring_id++) {
			delay_tid_stats =
				&delay_stats->delay_tid_stats[tid][ring_id];
			tx_delay = &delay_tid_stats->tx_delay;
			rx_delay = &delay_tid_stats->rx_delay;

			ath12k_dp_hist_init(&tx_delay->tx_swq_delay,
					    HIST_TYPE_SW_ENQEUE_DELAY);
			ath12k_dp_hist_init(&tx_delay->hwtx_delay,
					    HIST_TYPE_HW_COMP_DELAY);
			ath12k_dp_hist_init(&rx_delay->to_stack_delay,
					    HIST_TYPE_REAP_STACK);
		}
	}
	rcu_read_unlock();
}

static void ath12k_dp_peer_reset_jitter_stats(struct ath12k_dp_peer *dp_peer)
{
	if (!dp_peer)
		return;

	rcu_read_lock();
	if (dp_peer->mld_stats.jitter_stats)
		memset(dp_peer->mld_stats.jitter_stats, 0,
		       sizeof(struct ath12k_dp_peer_jitter_stats));
	rcu_read_unlock();
}

static void ath12k_dp_peer_reset_sojourn_stats(struct ath12k_dp_peer *dp_peer)
{
	if (!dp_peer)
		return;

	rcu_read_lock();
	if (dp_peer->mld_stats.sojourn_stats)
		memset(dp_peer->mld_stats.sojourn_stats, 0,
		       sizeof(struct ath12k_dp_peer_sojourn_stats));
	rcu_read_unlock();
}

static ssize_t ath12k_write_reset_latency_stats(struct file *file,
						const char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k_dp_peer *dp_peer;
	u32 reset;

	if (kstrtou32_from_user(ubuf, count, 0, &reset))
		return -EINVAL;

	if (!reset)
		return -EINVAL;

	wiphy_lock(ah->hw->wiphy);

	/* Reset protocol stats for all peers */
	spin_lock_bh(&ah->dp_hw.peer_list_lock);
	list_for_each_entry(dp_peer, &ah->dp_hw.peers, list) {
		ath12k_dp_peer_reset_delay_stats(dp_peer);
		ath12k_dp_peer_reset_jitter_stats(dp_peer);
		ath12k_dp_peer_reset_sojourn_stats(dp_peer);
	}
	spin_unlock_bh(&ah->dp_hw.peer_list_lock);

	wiphy_unlock(ah->hw->wiphy);
	return count;
}

static const struct file_operations fops_latency_stats = {
	.write = ath12k_write_reset_latency_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_configure_ofdma_feature(struct ath12k *ar,
					 bool *dl_enabled,
					 bool *ul_enabled,
					 bool *dlbf_enabled,
					 int value,
					 const char *feature,
					 const char *mode)
{
	if (!strcmp(feature, "default")) {
		*dl_enabled = value;
		*ul_enabled = value;
		*dlbf_enabled = value;
	} else if (!strcmp(feature, "dl")) {
		*dl_enabled = value;
		if (!value)
			*dlbf_enabled = 0;
	} else if (!strcmp(feature, "ul")) {
		*ul_enabled = value;
	} else if (!strcmp(feature, "dlbf")) {
		if (!*dl_enabled && value) {
			ath12k_warn(ar->ab, "%s DLBF requires DL to be enabled\n", mode);
			return -EINVAL;
		}
		*dlbf_enabled = value;
	}

	return 0;
}

static ssize_t ath12k_enable_ofdma_txbf(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_link_vif *arvif;
	int value, ret;
	char buf[32] = {0};
	char mode[4] = {'\0'};
	char feature[8] = {'\0'};
	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
					user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%d %3s %7s", &value, mode, feature);
	if (ret < 2 || ret > 3) {
		ath12k_err(ar->ab, "3 arguments required usage: enable/disable eht/he ul/dl/dlbf");
		return -EINVAL;
	}
	strim(mode);
	if (ret == 2) {
		if (strscpy(feature, "default", sizeof(feature)) < 0) {
			ath12k_err(ar->ab, "Failed to copy default feature string");
			return -EINVAL;
		}
	} else {
		strim(feature);
		if (strlen(feature) >= sizeof(feature) - 1) {
			ath12k_err(ar->ab, "Feature string too long");
			return -EINVAL;
		}
	}
	if (strcmp(mode, "eht") && strcmp(mode, "he")) {
		ath12k_err(ar->ab, "Mode should be eht/he");
		return -EINVAL;
	}
	if (strcmp(feature, "ul") && strcmp(feature, "dl") &&
		strcmp(feature, "dlbf") && strcmp(feature, "default")) {
		ath12k_err(ar->ab,
			  "Invalid feature: '%s'. Must be 'ul', 'dl','dlbf', or omitted\n",
			  feature);
		return -EINVAL;
	}

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!strcmp(mode, "eht")) {
		ret = ath12k_configure_ofdma_feature(ar,
						    &ar->eht_dl_enabled,
						    &ar->eht_ul_enabled,
						    &ar->eht_dlbf_enabled,
						    value, feature, "EHT");
	} else {
		ret = ath12k_configure_ofdma_feature(ar,
						    &ar->he_dl_enabled,
						    &ar->he_ul_enabled,
						    &ar->he_dlbf_enabled,
						    value, feature, "HE");
	}
	if (ret)
		goto unlock;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!strcmp(mode, "eht"))
			ath12k_mac_set_eht_txbf_conf(arvif, NULL, false);
		if (!strcmp(mode, "he"))
			ath12k_mac_set_he_txbf_conf(arvif, NULL, false);
	}
unlock:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret ? ret : count;
}

static ssize_t ath12k_show_ofdma_txbf(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[128];
	int len;

	len = scnprintf(buf, sizeof(buf),
		       "HE DL: %d\nHE UL: %d\nHE DLBF: %d\n"
		       "EHT DL: %d\nEHT UL: %d\nEHT DLBF: %d\n",
		       ar->he_dl_enabled,
		       ar->he_ul_enabled,
		       ar->he_dlbf_enabled,
		       ar->eht_dl_enabled,
		       ar->eht_ul_enabled,
		       ar->eht_dlbf_enabled);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations ofdma_txbf = {
	.write = ath12k_enable_ofdma_txbf,
	.open = ath12k_debugfs_open,
	.read = ath12k_show_ofdma_txbf,
};

static ssize_t ath12k_write_dp_stats_mask(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar;
	u32 debug_mask;
	u32 ppdu_mask;
	bool stats_disable;
	int i = 0, ret;
	enum dp_mon_stats_mode mode = 0;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_vif *dp_vif;

	wiphy_lock(ah->hw->wiphy);

	if (ah->state != ATH12K_HW_STATE_ON) {
		ath12k_err(NULL, "Interface not up\n");
		ret = -ENETDOWN;
		goto exit;
	}

	if (kstrtou32_from_user(ubuf, count, 0, &debug_mask)) {
		ret = -EINVAL;
		goto exit;
	}

	ath12k_dbg(ah->radio[0].ab, ATH12K_DBG_TELEMETRY,
		   "dp_stats_mask write: debug_mask=0x%x num_radio=%d\n",
		   debug_mask, ah->num_radio);

	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->ab->is_bypassed)
			continue;
		if (ar) {
			stats_disable = !(debug_mask & DP_ENABLE_STATS);

			ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY,
				   "radio[%d] stats_disable=%d ab->stats_disable=%d\n",
				   i, stats_disable, ar->ab->stats_disable);

			if (stats_disable != ar->ab->stats_disable) {
				mode = ATH12k_DP_MON_BASIC_STATS;
				ath12k_dp_mon_rx_stats_config(ar, !stats_disable,
							      mode);

				ret = ath12k_dp_mon_rx_update_filter(ar);
				if (ret)
					ath12k_warn(ar->ab,
						    "failed to configure basic stats monitor filters: %d\n",
						    ret);

				ppdu_mask = stats_disable ?
					    0 : HTT_PPDU_STATS_TAG_DEFAULT;

				ath12k_dp_tx_htt_h2t_ppdu_stats_req(ar,
								    ppdu_mask);

				/*
				 * perf_mode is checked by mac80211 in
				 * mesh configuration, stats_disable is
				 * checked in ppeds
				 */
				ar->ah->hw->perf_mode = stats_disable;
				ar->ab->stats_disable = stats_disable;
				ar->ab->dp->stats_disable = stats_disable;

				ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY,
					   "dp_stats_mask: perf_mode %d, Tx PPDU mask 0x%x Rx Mon mode : %u\n",
					    ar->ah->hw->perf_mode, ppdu_mask,
					    mode);

			} else {
				ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY,
					   "radio[%d] basic stats state unchanged, skipping...\n",
					   i);
			}

			/* Handle extended RX stats */
			mode = ATH12k_DP_MON_EXTD_STATS;
			if (debug_mask & DP_ENABLE_EXT_RX_STATS) {
				if (!ar->ab->hw_params->rxdma1_enable)
					debug_mask &= ~DP_ENABLE_EXT_RX_STATS;

				ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY,
					   "radio[%d] enabling ext RX stats : mode : %u\n",
					   i, mode);

				ath12k_dp_mon_rx_stats_config(ar, true, mode);

				ret = ath12k_dp_mon_rx_update_filter(ar);
				if (ret)
					ath12k_err(ar->ab,
						   "failed to setup rx extd stats filters %d\n",
						   ret);
			} else {
				ath12k_dp_mon_rx_stats_config(ar, false, mode);
			}

			ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY,
				   "radio[%d] dp_stats_mask updated: 0x%x -> 0x%x\n",
				   i, ar->dp.dp_stats_mask, debug_mask);

			ar->dp.dp_stats_mask = debug_mask;

			/* Sync dp_vif->dp_features bit-7 with dp_stats_mask bit-0 */
			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (!arvif || !arvif->ahvif)
					continue;
				dp_vif = &arvif->ahvif->dp_vif;

				if (debug_mask & DP_ENABLE_STATS)
					dp_vif->dp_features |= BIT(7);
				else
					dp_vif->dp_features &= ~BIT(7);

				ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY,
					   "dp_stats_mask 0x%x dp_features 0x%x\n",
					   ar->dp.dp_stats_mask,
					   dp_vif->dp_features);
			}
		}
	}
	ret = count;

exit:
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

static ssize_t ath12k_read_dp_stats_mask(struct file *file,
					 char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar;
	char buf[16];
	int len = 0, i;

	wiphy_lock(ah->hw->wiphy);
	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->ab->is_bypassed)
			continue;
		if (ar) {
			len = scnprintf(buf, sizeof(buf), "%X\n",
					ar->dp.dp_stats_mask);
			break;
		}
	}
	wiphy_unlock(ah->hw->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_dp_stats_mask = {
	.read = ath12k_read_dp_stats_mask,
	.write = ath12k_write_dp_stats_mask,
	.open = ath12k_debugfs_open,
};

static void ath12k_dp_vif_reset_proto_stats(struct ath12k_dp_vif *dp_vif)
{
	int ring_id;

	if (!dp_vif)
		return;

	/* Reset TX protocol stats for all rings */
	for (ring_id = 0; ring_id < DP_TCL_NUM_RING_MAX; ring_id++) {
		if (dp_vif->stats[ring_id].proto)
			memset(dp_vif->stats[ring_id].proto, 0,
			       sizeof(struct ath12k_dp_proto_stats_vif));
	}
}

static void ath12k_dp_peer_reset_proto_stats(struct ath12k_dp_peer *dp_peer)
{
	u8 index;

	if (!dp_peer)
		return;

	rcu_read_lock();
	for (index = 0; index < ATH12K_DP_PEER_MAX_MLO_LINKS; index++) {
		if (dp_peer->stats[index].proto)
			memset(dp_peer->stats[index].proto, 0,
			       sizeof(struct ath12k_dp_proto_stats_peer));
	}
	rcu_read_unlock();
}

static void ath12k_dp_peer_clear_qos_stats(struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_link_peer *link_peer;
	int link_id;

	/* Clear MLD QOS stats */
	if (dp_peer->mld_stats.mld_qos_stats)
		memset(dp_peer->mld_stats.mld_qos_stats, 0,
		       sizeof(*dp_peer->mld_stats.mld_qos_stats) *
		       QOS_TID_MAX * QOS_TID_MDSUQ_MAX);

	rcu_read_lock();
	/* Clear QOS stats for each link peer */
	for (link_id = 0; link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; link_id++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, link_id);
		if (link_peer && link_peer->peer_stats.link_qos_stats)
			memset(link_peer->peer_stats.link_qos_stats, 0,
			       sizeof(*link_peer->peer_stats.link_qos_stats));
	}
	rcu_read_unlock();
}

static void ath12k_dp_vif_reset_del_stats(struct ath12k_dp_vif *dp_vif,
					  unsigned long links_map)
{
	int link_id;
	struct ath12k_dp_link_vif *dp_link_vif;
	struct ath12k_dp_preserved_stats *link_peer_del_stats;
	/* Clear VIF-level preserved stats */
	ath12k_dp_clear_preserved_stats(&dp_vif->link_vif_delete_stats);

	/* Clear link VIF-level preserved stats */
	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		dp_link_vif = &dp_vif->dp_link_vif[link_id];
		if (dp_link_vif) {
			link_peer_del_stats = &dp_link_vif->link_peer_delete_stats;
			ath12k_dp_clear_preserved_stats(link_peer_del_stats);
		}
	}
}
static ssize_t ath12k_write_reset_dp_stats(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_pdev_dp *dp_pdev;
	u32 reset;
	int i = 0;

	if (kstrtou32_from_user(ubuf, count, 0, &reset))
		return -EINVAL;

	if (!reset)
		return -EINVAL;

	/*Clear only scan related stats*/
	if (reset == 2) {
		ath12k_dp_rx_scan_radio_stats_reset(ah);
		return count;
	}

	wiphy_lock(ah->hw->wiphy);

	spin_lock_bh(&ah->dp_hw.peer_list_lock);
	list_for_each_entry(dp_peer, &ah->dp_hw.peers, list) {
		memset(&dp_peer->stats, 0, sizeof(dp_peer->stats));
		/* Reset HW stats */
		if (dp_peer->mld_stats.hw_stats)
			memset(dp_peer->mld_stats.hw_stats, 0,
			       sizeof(*dp_peer->mld_stats.hw_stats));
		/* Clear peer-level preserved stats */
		ath12k_dp_clear_preserved_stats(&dp_peer->link_peer_delete_stats);
		ath12k_dp_peer_clear_qos_stats(dp_peer);

		ar = &ah->radio[0];
		if (ar && ath12k_proto_stats_enabled(&ar->dp))
			ath12k_dp_peer_reset_proto_stats(dp_peer);

		ath12k_telemetry_reset_peer_stats(dp_peer->addr);

		/* Reset mac80211-layer flow counters */
		{
			struct ieee80211_sta *sta;

			sta = ieee80211_find_sta_by_ifaddr(ah->hw, dp_peer->addr, NULL);
			if (sta) {
				atomic_set(&sta->tx_netif_pkts, 0);
				atomic_set(&sta->tx_drv_pkts, 0);
				atomic_set(&sta->rx_drv_pkts, 0);
				atomic_set(&sta->rx_netif_pkts, 0);
				atomic_set(&sta->rx_forwarded_pkts, 0);
			}
		}

		struct ath12k_dp_link_peer *tmp_peer = NULL;
		u8 link_id;

		rcu_read_lock();
		for (link_id = 0; link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; link_id++) {
			tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer,
									  link_id);
			if (!tmp_peer)
				continue;

			if (tmp_peer->peer_stats.tx_stats)
				memset(tmp_peer->peer_stats.tx_stats, 0,
				       sizeof(struct ath12k_htt_tx_stats));

			if (tmp_peer->peer_stats.tx_ppdu_stats)
				memset(tmp_peer->peer_stats.tx_ppdu_stats, 0,
				       sizeof(struct ath12k_htt_tx_ppdu_stats));

			if (tmp_peer->peer_stats.rx_ppdu_stats)
				memset(tmp_peer->peer_stats.rx_ppdu_stats, 0,
				       sizeof(struct ath12k_rx_ppdu_stats));

			if (tmp_peer->peer_stats.rx_stats)
				memset(tmp_peer->peer_stats.rx_stats, 0,
				       sizeof(struct ath12k_rx_peer_stats));

			/* Reset HW link stats */
			if (tmp_peer->peer_stats.hw_link_stats)
				memset(tmp_peer->peer_stats.hw_link_stats, 0,
				       sizeof(*tmp_peer->peer_stats.hw_link_stats));
		}

		dp_pdev = &ar->dp;
		if (dp_pdev && ath12k_dp_latency_stats_enabled(dp_pdev)) {
			ath12k_dp_peer_reset_delay_stats(dp_peer);
			ath12k_dp_peer_reset_jitter_stats(dp_peer);
			ath12k_dp_peer_reset_sojourn_stats(dp_peer);
		}

		rcu_read_unlock();
	}
	spin_unlock_bh(&ah->dp_hw.peer_list_lock);

	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->ab->is_bypassed)
			continue;
		dp_pdev = &ar->dp;
		if (dp_pdev && ath12k_dp_vow_stats_enabled(dp_pdev)) {
			memset(&dp_pdev->tid_stats, 0, sizeof(dp_pdev->tid_stats));
			dp_pdev->prev_tx_enq_tstamp = 0;
			dp_pdev->prev_rx_timestamp = 0;

			ath12k_dp_tid_tx_stats_hist_init(dp_pdev);
			ath12k_dp_tid_rx_stats_hist_init(dp_pdev);
		}
		list_for_each_entry(arvif, &ar->arvifs, list) {
			dp_vif = &arvif->ahvif->dp_vif;
			memset(&dp_vif->stats, 0, sizeof(dp_vif->stats));
			memset(&dp_vif->rx_stats, 0, sizeof(dp_vif->rx_stats));
			ath12k_dp_vif_reset_del_stats(dp_vif, arvif->ahvif->links_map);

			if (ath12k_proto_stats_enabled(&ar->dp))
				ath12k_dp_vif_reset_proto_stats(dp_vif);
		}
	}

	wiphy_unlock(ah->hw->wiphy);
	return count;
}

static const struct file_operations fops_reset_dp_stats = {
	.write = ath12k_write_reset_dp_stats,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_write_reset_proto_stats(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_vif *dp_vif;
	u32 reset;
	int i = 0;

	if (kstrtou32_from_user(ubuf, count, 0, &reset))
		return -EINVAL;

	if (!reset)
		return -EINVAL;

	wiphy_lock(ah->hw->wiphy);

	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (!ar || ar->ab->is_bypassed)
			continue;
		if (!ath12k_proto_stats_enabled(&ar->dp)) {
			wiphy_unlock(ah->hw->wiphy);
			return count;
		}
	}

	/* Reset protocol stats for all peers */
	spin_lock_bh(&ah->dp_hw.peer_list_lock);
	list_for_each_entry(dp_peer, &ah->dp_hw.peers, list) {
		ath12k_dp_peer_reset_proto_stats(dp_peer);
	}
	spin_unlock_bh(&ah->dp_hw.peer_list_lock);

	/* Reset protocol stats for all VIFs */
	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->ab->is_bypassed)
			continue;
		list_for_each_entry(arvif, &ar->arvifs, list) {
			dp_vif = &arvif->ahvif->dp_vif;
			ath12k_dp_vif_reset_proto_stats(dp_vif);
		}
	}

	wiphy_unlock(ah->hw->wiphy);
	return count;
}

static const struct file_operations fops_reset_proto_stats = {
	.write = ath12k_write_reset_proto_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_set_sta_primary_link(struct file *file,
					 const char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	char buf[IFNAMSIZ + 4]; /* ifname + space + up-to-3-digit number + NUL */
	char ifname[IFNAMSIZ];
	u8 hw_link_id;
	int i, free_slot = -1;
	size_t len;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	if (sscanf(buf, "%15s %hhu", ifname, &hw_link_id) != 2)
		return -EINVAL;

	if (hw_link_id >= ah->num_radio)
		return -EINVAL;

	ath12k_info(NULL, "ifname: %s, hw_link_id: %d", ifname, hw_link_id);

	mutex_lock(&ah->hw_mutex);
	for (i = 0; i < ATH12K_GROUP_MAX_RADIO; i++) {
		if (ah->pending_primary_link[i].valid &&
		    strncmp(ah->pending_primary_link[i].ifname, ifname, IFNAMSIZ) == 0) {
			/* Update existing entry for this ifname. */
			ah->pending_primary_link[i].hw_link_id = hw_link_id;
			ath12k_info(NULL, "Overriding with hw_link_id: %d\n",
				   hw_link_id);
			mutex_unlock(&ah->hw_mutex);
			return count;
		}

		if (free_slot < 0 && !ah->pending_primary_link[i].valid)
			free_slot = i;
	}

	if (free_slot < 0) {
		mutex_unlock(&ah->hw_mutex);
		return -ENOSPC;
	}

	strscpy(ah->pending_primary_link[free_slot].ifname, ifname, IFNAMSIZ);
	ah->pending_primary_link[free_slot].hw_link_id = hw_link_id;
	ah->pending_primary_link[free_slot].valid = true;
	mutex_unlock(&ah->hw_mutex);
	return count;
}

static const struct file_operations ath12k_fops_set_sta_primary_link = {
	.write = ath12k_write_set_sta_primary_link,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t
ath12k_write_reset_dp_tx_mon_stats(struct file *file,
						const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath12k_hw *ah = file->private_data;
	struct ath12k *ar;
	struct ath12k_base *ab, *prev_ab = NULL;
	struct ath12k_dp *dp;
	struct ath12k_pdev_dp *pdev;
	struct ath12k_dp_mon *mon_dp;
	struct ath12k_pdev_mon_dp *pdev_mon_dp;
	struct ath12k_dp_tx_mon_stats *tx_mon_stats;
	struct ath12k_pdev_tx_mon_stats *pdev_tx_mon_stats;
	char buf[ATH12K_DBG_RESET_STR_LEN + 1] = {0};
	int i, idx;

	if (!ah->num_radio)
		return -EINVAL;

	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	if (strncmp(buf, ATH12K_DBG_RESET_STR,
		    ATH12K_DBG_RESET_STR_LEN - 1) ||
	    (buf[ATH12K_DBG_RESET_STR_LEN - 1] != '\n' &&
	     buf[ATH12K_DBG_RESET_STR_LEN - 1] != '\0'))
		return -EINVAL;

	wiphy_lock(ah->hw->wiphy);

	for_each_ar(ah, ar, idx) {
		if (ar->ab == prev_ab)
			continue;
		prev_ab = ar->ab;
		ab = ar->ab;
		if (ab->is_bypassed)
			continue;
		dp = ath12k_ab_to_dp(ab);
		if (!ath12k_dp_tx_mon_feature_eval(dp))
			continue;

		mon_dp = dp->dp_mon;
		if (!mon_dp)
			continue;

		tx_mon_stats = &mon_dp->dp_tx_mon->tx_mon_stats;

		memset(tx_mon_stats, 0, sizeof(struct ath12k_dp_tx_mon_stats));

		for (i = 0; i < ah->num_radio; i++) {
			if (ah->radio[i].ab != ab)
				continue;
			if (ah->radio[i].ab->is_bypassed)
				continue;

			memset(&ah->radio[i].tx_mon_ssr_stats, 0,
			       sizeof(ah->radio[i].tx_mon_ssr_stats));

			pdev = &ah->radio[i].dp;
			pdev_mon_dp = pdev->dp_mon_pdev;
			if (unlikely(!pdev_mon_dp))
				continue;

			pdev_tx_mon_stats =
				&pdev_mon_dp->dp_pdev_tx_mon->pdev_tx_mon_stats;
			memset(pdev_tx_mon_stats, 0,
				sizeof(struct ath12k_pdev_tx_mon_stats));
		}
	}
	wiphy_unlock(ah->hw->wiphy);

	return count;
}

static const struct file_operations fops_reset_dp_tx_mon_stats = {
	.write = ath12k_write_reset_dp_tx_mon_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
};

void ath12k_hw_debugfs_register(struct ath12k_hw *ah)
{
	struct ieee80211_hw *hw = ah->hw;
	struct ath12k_base *ab = ah->radio[0].ab;

	ath12k_debugfs_create_file("set_sta_primary_link", 0200, hw->wiphy->debugfsdir,
				   ab, ah,
				   &ath12k_fops_set_sta_primary_link);

	ath12k_debugfs_create_file("dp_stats_mask", 0644, hw->wiphy->debugfsdir,
				   ab, ah,
				   &fops_dp_stats_mask);

	ath12k_debugfs_create_file("reset_dp_stats", 0644, hw->wiphy->debugfsdir,
				   ab, ah,
				   &fops_reset_dp_stats);

	ath12k_debugfs_create_file("reset_proto_stats", 0200, hw->wiphy->debugfsdir,
				   ab, ah,
				   &fops_reset_proto_stats);

	ath12k_debugfs_create_file("qos_stats", 0644, hw->wiphy->debugfsdir,
				   ab, ah,
				   &fops_qos_stats);

	ath12k_debugfs_create_file("reset_latency_stats", 0644, hw->wiphy->debugfsdir,
				   ab, ah,
				   &fops_latency_stats);

	ath12k_debugfs_create_file("reset_dp_tx_mon_stats", 0200, hw->wiphy->debugfsdir,
				   ab, ah,
				   &fops_reset_dp_tx_mon_stats);
}

static ssize_t ath12k_read_bcast_probe_rl_stats(struct file *file,
						char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pdev_telemetry_stats *stats = &ar->dp.stats.telemetry_stats;
	int len = 0, ret;
	const int size = 512;

	char *buf __free(kfree) = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ar->data_lock);

	len += scnprintf(buf + len, size - len,
			 "Broadcast Probe Request Rate-Limit Stats:\n");
	len += scnprintf(buf + len, size - len,
			 "  enabled                    = %u\n",
			 ar->bcast_probe_rl_enabled);
	len += scnprintf(buf + len, size - len,
			 "  rx_bcast_probe_req_total   = %u\n",
			 stats->rx_probe_req_bc);
	len += scnprintf(buf + len, size - len,
			 "  rx_bc_prb_req_drop = %u\n",
			 stats->rx_bc_prb_req_drop);
	len += scnprintf(buf + len, size - len,
			 "  suppress_window_ms         = %u\n",
			 ar->bcast_probe_rl_window_ms);
	len += scnprintf(buf + len, size - len,
			 "  table_entries_live         = %u\n",
			 ar->bcast_probe_rl_entries);
	len += scnprintf(buf + len, size - len,
			 "  table_entries_max          = %u\n",
			 ar->bcast_probe_rl_max_entries);

	spin_unlock_bh(&ar->data_lock);

	len += scnprintf(buf + len, size - len,
			 "Commands:\n"
			 "  echo enable/disable/reset > bcast_probe_rl_stats\n"
			 "  echo window_ms=<50-5000>  > bcast_probe_rl_stats\n"
			 "  echo max_entries=<64-4096> > bcast_probe_rl_stats\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	return ret;
}

static ssize_t ath12k_write_bcast_probe_rl_stats(struct file *file,
						 const char __user *ubuf,
						 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[32] = {0};
	u32 val;

	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;

	spin_lock_bh(&ar->data_lock);

	if (strncmp(buf, "enable", 6) == 0)
		ar->bcast_probe_rl_enabled = true;
	else if (strncmp(buf, "disable", 7) == 0)
		ar->bcast_probe_rl_enabled = false;
	else if (strncmp(buf, "reset", 5) == 0)
		ar->dp.stats.telemetry_stats.rx_bc_prb_req_drop = 0;
	else if (sscanf(buf, "window_ms=%u", &val) == 1 &&
		 val >= 50 && val <= 5000)
		ar->bcast_probe_rl_window_ms = val;
	else if (sscanf(buf, "max_entries=%u", &val) == 1 &&
		 val >= 64 && val <= 4096)
		ar->bcast_probe_rl_max_entries = val;

	spin_unlock_bh(&ar->data_lock);

	return count;
}

static const struct file_operations fops_bcast_probe_rl_stats = {
	.read  = ath12k_read_bcast_probe_rl_stats,
	.write = ath12k_write_bcast_probe_rl_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_rfa_temperature(struct file *file,
					   char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	bool rfa_temp_valid;
	int rfa_temperature;
	char buf[32];
	int len;

	spin_lock_bh(&ar->data_lock);
	rfa_temp_valid = ar->thermal.rfa_temp_valid;
	rfa_temperature = ar->thermal.rfa_temperature;
	ar->thermal.rfa_temp_valid = false;
	ar->thermal.rfa_temperature = 0;
	spin_unlock_bh(&ar->data_lock);

	/* display in millidegree celsius */
	len = scnprintf(buf, sizeof(buf), "%d\n",
			rfa_temp_valid ? rfa_temperature * 1000 : 0);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_rfa_temperature = {
	.read = ath12k_read_rfa_temperature,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath12k_debugfs_register(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_hw *hw = ar->ah->hw;
	char pdev_name[5];
	char buf[100] = {0};

	scnprintf(pdev_name, sizeof(pdev_name), "%s%d", "mac", ar->pdev_idx);

	ar->debug.debugfs_pdev = debugfs_create_dir(pdev_name, ab->debugfs_soc);

	/* Create a symlink under ieee80211/phy* */
	scnprintf(buf, sizeof(buf), "../../ath12k/%pd2", ar->debug.debugfs_pdev);
	if (!hw->wiphy->n_radio) {
		ar->debug.debugfs_pdev_symlink =
			debugfs_create_symlink("ath12k",
						hw->wiphy->debugfsdir,
						buf);
	} else {
		char dirname[32] = {0};

		snprintf(dirname, 32, "ath12k_hw%d", ar->radio_idx);
		ar->debug.debugfs_pdev_symlink =
			debugfs_create_symlink(dirname,
					       hw->wiphy->debugfsdir,
					       buf);
	}

	if (ar->mac.sbands[NL80211_BAND_5GHZ].channels) {
		ath12k_debugfs_create_file("dfs_simulate_radar", 0200,
					   ar->debug.debugfs_pdev,
					   ar->ab, ar,
					   &fops_simulate_radar);
		ath12k_debugfs_create_file("dfs_block_radar_events", 0200,
					   ar->debug.debugfs_pdev,
					   ar->ab, ar,
					   &fops_dfs_block_radar);
	}

	debugfs_create_file("tpc_stats", 0400, ar->debug.debugfs_pdev,
			    ar, &fops_tpc_stats);
	ath12k_debugfs_create_file("tpc_stats_type", 0200, ar->debug.debugfs_pdev,
				   ar->ab, ar, &fops_tpc_stats_type);
	init_completion(&ar->debug.tpc_complete);

	ath12k_debugfs_create_file("wmm_stats", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_wmm_stats);

	ath12k_debugfs_create_file("athdiag", 0600, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_athdiag);

	ath12k_debugfs_create_file("enable_m3_dump", 0600, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_enable_m3_dump);

	ath12k_debugfs_create_file("btcoex", 0644, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_btcoex);

	ath12k_debugfs_create_file("btcoex_duty_cycle", 0644, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_btcoex_duty_cycle);

	ath12k_debugfs_create_file("btcoex_algorithm", 0644, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_btcoex_algo);

	ath12k_debugfs_create_file("btcoex_priority", 0600, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_btcoex_priority);

	ath12k_debugfs_create_file("dump_mgmt_stats", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_dump_mgmt_stats);

	ath12k_debugfs_create_file("chanctx_switch_stats", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_chanctx_switch_stats);

	ath12k_debugfs_create_file("set_tt_configs", 0600, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &tt_configs);
	ath12k_debugfs_create_file("rfa_temperature", 0400, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_rfa_temperature);
	ath12k_debugfs_create_file("bcast_probe_rl_stats", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_bcast_probe_rl_stats);

	ath12k_debugfs_htt_stats_register(ar);
	ath12k_debugfs_fw_stats_register(ar);
	ath12k_init_pktlog(ar);

	if (test_bit(WMI_TLV_SERVICE_CTRL_PATH_STATS_REQUEST,
		     ar->ab->wmi_ab.svc_map))
		ath12k_debugfs_wmi_ctrl_stats_register(ar);

	ath12k_debugfs_create_file("ext_rx_stats", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_extd_rx_stats);

	ath12k_debugfs_create_file("ext_tx_stats", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_extd_tx_stats);

	if (ar->mac.sbands[NL80211_BAND_6GHZ].channels) {
		ath12k_debugfs_create_file("simulate_awgn", 0200,
					   ar->debug.debugfs_pdev,
					   ar->ab, ar,
					   &fops_simulate_awgn);
#ifdef CPTCFG_QCN_EXTN
		ath12k_debugfs_register_hw_blocklist_extn(ar);
#endif
		ath12k_debugfs_create_file("configure_afc_grace_timer", 0200,
					   ar->debug.debugfs_pdev,
					   ar->ab, ar,
					   &fops_configure_afc_grace_timer);
	}
	ath12k_debugfs_create_file("scan_args_config", 0600, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_scan_args_config);

	ath12k_debugfs_create_file("neighbor_peer", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_write_nrp_mac);

	ath12k_debugfs_create_file("smart_mon_filter", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_smart_mon_filter);

	ath12k_debugfs_create_file("qos_map_set", 0600, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_qos_map_set);

	ath12k_debugfs_create_file("pktlog_filter", 0644,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_pktlog_filter);

	ath12k_debugfs_create_file("ofdma_conf", 0600,
				   ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &ofdma_txbf);

	ath12k_debugfs_create_file("remote_enable", 0644, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_pktlog_remote_enable);
	ath12k_debugfs_create_file("remote_ip", 0644, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_pktlog_remote_ip);
	ath12k_debugfs_create_file("remote_port", 0444, ar->debug.debugfs_pdev,
				   ar->ab, ar,
				   &fops_pktlog_remote_port);
#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
	ath12k_debugfs_register_ipa_extn(ar);
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		ath12k_debugfs_create_file("ppeds_stats", 0600, ab->debugfs_soc,
					   ab, ab,
					   &fops_ppeds_stats);
#endif
}

struct ath12k_srng_stats_priv {
	char *buf;
	int len;
};

static int ath12k_debugfs_hal_dump_srng_stats_open(struct inode *inode, struct file *file)
{
	struct ath12k_base *ab = inode->i_private;
	struct ath12k_srng_stats_priv *priv;
	const int size = 4096 * 6;

	if (ab->is_bypassed) {
		ath12k_dbg(ab, ATH12K_DBG_MAC, "device is in bypassed state\n");
		return -EPERM;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->buf = vmalloc(size);
	if (!priv->buf) {
		kfree(priv);
		return -ENOMEM;
	}

	priv->len = ath12k_debugfs_hal_dump_srng_stats(ab, priv->buf, size);
	priv->len = min_t(int, priv->len, size);

	file->private_data = priv;

	return 0;
}

static ssize_t ath12k_debugfs_hal_dump_srng_stats_read(struct file *file,
                                               char __user *user_buf,
                                               size_t count, loff_t *ppos)
{
	struct ath12k_srng_stats_priv *priv = file->private_data;

	return simple_read_from_buffer(user_buf, count, ppos, priv->buf, priv->len);
}

static int ath12k_debugfs_hal_dump_srng_stats_release(struct inode *inode,
						      struct file *file)
{
	struct ath12k_srng_stats_priv *priv = file->private_data;

	vfree(priv->buf);
	kfree(priv);

	return 0;
}

static const struct file_operations fops_dump_hal_stats = {
	.read = ath12k_debugfs_hal_dump_srng_stats_read,
	.open = ath12k_debugfs_hal_dump_srng_stats_open,
	.release = ath12k_debugfs_hal_dump_srng_stats_release,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_umac_reset_stats(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *partner_ab;
	struct ath12k_dp_umac_reset *umac_reset;
	struct ath12k_umac_reset_ts *ts;
	char *buf;
	int len = 0, i, j, state;
	const int size = 8192;
	u64 state_times[ATH12K_UMAC_RESET_STATE_MAX];
	u32 state_counts[ATH12K_UMAC_RESET_STATE_MAX];
	u64 irq_ts[5];
	u64 overall_duration, step_duration;

	static const char * const state_names[] = {
		[ATH12K_UMAC_RESET_STATE_IDLE] = "IDLE",
		[ATH12K_UMAC_RESET_STATE_INIT] = "INIT",
		[ATH12K_UMAC_RESET_STATE_TRIGGER_SENT] = "TRIGGER_SENT",
		[ATH12K_UMAC_RESET_STATE_PRE_RESET_START] = "PRE_RESET_START",
		[ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE] = "PRE_RESET_DONE",
		[ATH12K_UMAC_RESET_STATE_POST_RESET_START] = "POST_RESET_START",
		[ATH12K_UMAC_RESET_STATE_POST_RESET_DONE] = "POST_RESET_DONE",
		[ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE] = "POST_RESET_COMPLETE",
		[ATH12K_UMAC_RESET_STATE_ERROR] = "ERROR",
	};

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			 "UMAC Reset Statistics for HW Group\n");
	len += scnprintf(buf + len, size - len,
			 "===================================\n\n");

	if (!ag) {
		len += scnprintf(buf + len, size - len,
				 "No HW group available\n");
		goto out;
	}

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab || partner_ab->is_bypassed)
			continue;

		umac_reset = &partner_ab->dp_umac_reset;
		ts = &umac_reset->ts;

		len += scnprintf(buf + len, size - len,
				 "Device %d (chip_id: %d):\n", i, partner_ab->device_id);
		len += scnprintf(buf + len, size - len,
				 "----------------------------------------\n");

		/* Copy IRQ timestamps */
		irq_ts[0] = ts->event_irq_init_umac_recovery;
		irq_ts[1] = ts->event_irq_init_target_recovery;
		irq_ts[2] = ts->event_irq_pre_reset;
		irq_ts[3] = ts->event_irq_post_reset_start;
		irq_ts[4] = ts->event_irq_post_reset_complete;

		/* Copy state transition times and counts */
		for (j = 0; j < ATH12K_UMAC_RESET_STATE_MAX; j++) {
			state_times[j] = umac_reset->state_entry_time[j];
			state_counts[j] = umac_reset->state_transition_count[j];
		}

		/* Display IRQ Event Timestamps */
		len += scnprintf(buf + len, size - len,
				 "  IRQ Event Timestamps:\n");
		len += scnprintf(buf + len, size - len,
				 "    Init UMAC Recovery      : %llu %s\n",
				 irq_ts[0], irq_ts[0] ? "" : " (not triggered)");
		len += scnprintf(buf + len, size - len,
				 "    Init Target Recovery    : %llu %s\n",
				 irq_ts[1], irq_ts[1] ? "" : " (not triggered)");
		len += scnprintf(buf + len, size - len,
				 "    Pre Reset               : %llu %s\n",
				 irq_ts[2], irq_ts[2] ? "" : " (not triggered)");
		len += scnprintf(buf + len, size - len,
				 "    Post Reset Start        : %llu %s\n",
				 irq_ts[3], irq_ts[3] ? "" : " (not triggered)");
		len += scnprintf(buf + len, size - len,
				 "    Post Reset Complete     : %llu %s\n\n",
				 irq_ts[4], irq_ts[4] ? "" : " (not triggered)");

		/* Display State Transition Timestamps */
		len += scnprintf(buf + len, size - len,
				 "  State Transition Timestamps:\n");
		for (j = 0; j < ATH12K_UMAC_RESET_STATE_MAX; j++) {
			if (state_times[j])
				len += scnprintf(buf + len, size - len,
						 "    %-24s: %llu\n",
						 state_names[j], state_times[j]);
		}
		len += scnprintf(buf + len, size - len, "\n");

		/* Calculate and display step durations */
		len += scnprintf(buf + len, size - len,
				 "  Step Durations:\n");

		/* IRQ to TRIGGER_SENT */
		state = ATH12K_UMAC_RESET_STATE_TRIGGER_SENT;
		if (irq_ts[0] && state_times[state]) {
			step_duration = state_times[state] - irq_ts[0];
			len += scnprintf(buf + len, size - len,
					 "    IRQ to TRIGGER_SENT     : %llu ms\n",
					 step_duration);
		}

		/* PRE_RESET processing (IRQ arrival to DONE sent) */
		state = ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE;
		if (irq_ts[2] && state_times[state]) {
			step_duration = state_times[state] - irq_ts[2];
			len += scnprintf(buf + len, size - len,
					 "    PRE_RESET processing    : %llu ms\n",
					 step_duration);
		}

		/* POST_RESET_START processing (IRQ arrival to DONE sent) */
		state = ATH12K_UMAC_RESET_STATE_POST_RESET_DONE;
		if (irq_ts[3] && state_times[state]) {
			step_duration = state_times[state] - irq_ts[3];
			len += scnprintf(buf + len, size - len,
					 "    POST_RESET_START process: %llu ms\n",
					 step_duration);
		}

		/* POST_RESET_COMPLETE processing (IRQ arrival to DONE sent) */
		state = ATH12K_UMAC_RESET_STATE_IDLE;
		if (irq_ts[4] && state_times[state]) {
			step_duration = state_times[state] - irq_ts[4];
			len += scnprintf(buf + len, size - len,
					 "    POST_RESET_COMPLETE proc: %llu ms\n",
					 step_duration);
		}

		/* Overall duration (INIT to IDLE) */
		if (state_times[ATH12K_UMAC_RESET_STATE_INIT] &&
		    state_times[ATH12K_UMAC_RESET_STATE_IDLE]) {
			overall_duration = state_times[ATH12K_UMAC_RESET_STATE_IDLE] -
					   state_times[ATH12K_UMAC_RESET_STATE_INIT];
			len += scnprintf(buf + len, size - len,
					 "\n  Overall Duration          : %llu ms (INIT to IDLE)\n",
					 overall_duration);
		}

		/* Display state transition counts */
		len += scnprintf(buf + len, size - len,
				 "\n  State Transition Counts:\n");
		for (j = 0; j < ATH12K_UMAC_RESET_STATE_MAX; j++) {
			if (state_counts[j])
				len += scnprintf(buf + len, size - len,
						 "    %-24s: %u\n",
						 state_names[j], state_counts[j]);
		}

		len += scnprintf(buf + len, size - len, "\n");
	}

out:
	if (len > size)
		len = size;

	i = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return i;
}

static const struct file_operations fops_umac_reset_stats = {
	.read = ath12k_read_umac_reset_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_simulate_host_crash(struct file *file,
					       char __user *user_buf,
					       size_t count, loff_t *ppos)
{
	const char buf[] =
		 "To simulate a Host crash write 1 to this file:\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf)); }

static ssize_t ath12k_write_simulate_host_crash(struct file *file,
						const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_hw_group *ag = ab->ag;
	char buf[32] = {0};
	int i;
	unsigned int val = 0;
	ssize_t rc;

	/* filter partial writes and invalid commands */
	if (*ppos != 0 || count >= sizeof(buf) || count == 0)
		return -EINVAL;

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (rc < 0)
		return rc;

	/* drop the possible '\n' from the end */
	if (buf[*ppos - 1] == '\n')
		buf[*ppos - 1] = '\0';

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	if (val && ag) {
		ath12k_info(ab, "simulating Host assert\n");
		for (i = 0; i < ag->num_devices; i++)
			ath12k_core_trigger_bug_on(ag->ab[i]);
	}

	return count;
}

static const struct file_operations fops_simulate_host_crash = {
	.read = ath12k_read_simulate_host_crash,
	.write = ath12k_write_simulate_host_crash,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_simulate_fw_crash(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	const char buf[] =
		 "To simulate firmware crash write one of the keywords to this file:\n"
		 "`assert` - send WMI_FORCE_FW_HANG_CMDID to firmware to cause assert.\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf)); }

static ssize_t ath12k_write_simulate_fw_crash(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_pdev *pdev;
	struct ath12k *ar = NULL;
	char buf[32] = {0};
	int i, j, ret;
	ssize_t rc;
	struct ath12k_base *cumac_ab = NULL;
	struct ath12k *cumac_ar = NULL;

	/* filter partial writes and invalid commands */
	if (*ppos != 0 || count >= sizeof(buf) || count == 0)
		return -EINVAL;

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (rc < 0)
		return rc;

	/* drop the possible '\n' from the end */
	if (buf[*ppos - 1] == '\n')
		buf[*ppos - 1] = '\0';

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar)
			break;
	}

	if (!ar || (ar->ah->state != ATH12K_HW_STATE_ON &&
	    !ath12k_ftm_mode))
		return -ENETDOWN;

	if (ath12k_hw_group_recovery_in_progress(ag)) {
		ath12k_err(ab, "Already in recovery\n");
		return -EPERM;
	}

	if (ag->wsi_remap_in_progress) {
		ath12k_err(ab, "WSI remap in progress, try later\n");
		return -EPERM;
	}

	if (!strcmp(buf, "assert")) {
		ath12k_info(ab, "simulating firmware assert crash\n");
		ret = ath12k_wmi_force_fw_hang_cmd(ar,
						   ATH12K_WMI_FW_HANG_ASSERT_TYPE,
						   ATH12K_WMI_FW_HANG_DELAY, false);
	} else if (!strcmp(buf, "cumac_q6_bcr_assert")) {
		for (i = 0; i < ag->num_devices; i++) {
			cumac_ab = ag->ab[i];
			if (!cumac_ab || cumac_ab->is_bypassed)
				continue;
			if (!cumac_ab->is_cumac_chip)
				continue;
			for (j = 0; j < cumac_ab->num_radios; j++) {
				cumac_ar = cumac_ab->pdevs[j].ar;
				if (cumac_ar)
					break;
			}
			break;
		}

		if (!cumac_ar) {
			ath12k_err(ab, "No CUMAC chip found in hw group\n");
			return -ENODEV;
		}

		ath12k_info(cumac_ab, "simulating CUMAC Q6 BCR assert crash\n");
		ret = ath12k_wmi_force_fw_hang_cmd(cumac_ar,
						   ATH12K_WMI_Q6_BCR_ASSERT_TYPE,
						   ATH12K_WMI_FW_HANG_DELAY, false);
	} else {
		return -EINVAL;
	}

	if (ret) {
		ath12k_warn(ab, "failed to simulate firmware crash: %d\n", ret);
		return ret;
	}

	return count;
}

static const struct file_operations fops_simulate_fw_crash = {
	.read = ath12k_read_simulate_fw_crash,
	.write = ath12k_write_simulate_fw_crash,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_debug_write_fw_recovery(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	unsigned int value;
	int ret;

	if (kstrtouint_from_user(user_buf, count, 0, &value))
		return -EINVAL;

	if (value < ATH12K_FW_RECOVERY_DISABLE ||
	    value > ATH12K_FW_RECOVERY_ENABLE_MODE2) {
		ath12k_warn(ab, "Please enter: 0 = Disable, 1 = Mode-0 recovery, 2/3 = Mode-2 or Mode-3 recovery based on HW support\n");
		ret = -EINVAL;
		goto exit;
	}

	if (ab && ab->ag && !ab->ag->mlo_capable &&
	    value > ATH12K_WMI_FW_HANG_RECOVERY_MODE0) {
		ath12k_err(ab, "Only mode 0 recovery supports for non-MLO devices\n");
		ret = -EINVAL;
		goto exit;
	}

	ath12k_core_send_fw_hang_cmd(ab, value);

	ret = count;

exit:
	return ret;
}

static ssize_t ath12k_debug_read_fw_recovery(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char buf[32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ab->fw_recovery_support);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_debug_read_recovery_in_progress(struct file *file,
						       char __user *user_buf,
						       size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char buf[8];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n",
			ath12k_hw_group_recovery_in_progress(ab->ag));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_recovery_in_progress = {
	.read = ath12k_debug_read_recovery_in_progress,
	.open = simple_open,
};



static const struct file_operations fops_fw_recovery = {
	.read = ath12k_debug_read_fw_recovery,
	.write = ath12k_debug_write_fw_recovery,
	.open = ath12k_debugfs_open,
};


#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
static ssize_t ath12k_debug_write_dbs_power_reduction(struct file *file,
						      const char __user *user_buf,
						      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_hw_group *ag;
	unsigned int value;

	if (kstrtouint_from_user(user_buf, count, 0, &value))
		return -EINVAL;

	ag = ab->ag;

	if (value > 15) {
		ath12k_warn(ab, "Dual band power reduction value should be between 0 - 15 dBm");
		return -EINVAL;
	}

	ag->dbs_power_reduction = value;

	return count;
}

static ssize_t ath12k_debug_read_dbs_power_reduction(struct file *file,
						     char __user *user_buf,
						     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char buf[ATH12K_BUF_SIZE_32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ab->ag->dbs_power_reduction);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations dbs_power_reduction = {
	.read = ath12k_debug_read_dbs_power_reduction,
	.write = ath12k_debug_write_dbs_power_reduction,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_debug_write_eth_power_reduction(struct file *file,
						      const char __user *user_buf,
						      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_hw_group *ag;
	unsigned int value;

	if (kstrtouint_from_user(user_buf, count, 0, &value))
		return -EINVAL;

	ag = ab->ag;

	if (value > 15) {
		ath12k_warn(ab, "Ethernet power reduction value should be between 0 - 15 dBm");
		return -EINVAL;
	}

	ag->eth_power_reduction = value;

	return count;
}

static ssize_t ath12k_debug_read_eth_power_reduction(struct file *file,
						     char __user *user_buf,
						     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char buf[ATH12K_BUF_SIZE_32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ab->ag->eth_power_reduction);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations eth_power_reduction = {
	.read = ath12k_debug_read_eth_power_reduction,
	.write = ath12k_debug_write_eth_power_reduction,
	.open = ath12k_debugfs_open,
};
#endif

static ssize_t ath12k_read_fw_dbglog(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	size_t len;
	char buf[128];

	len = scnprintf(buf, sizeof(buf), "%u 0x%016llx\n",
			ab->fw_dbglog_param, ab->fw_dbglog_val);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_fw_dbglog(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k *ar = ab->pdevs[0].ar;
	char buf[128] = {0};
	unsigned int param;
	u64 value;
	int ret;

	if (!ar)
		return -EINVAL;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);
	if (ret <= 0)
		goto out;

	ret = sscanf(buf, "%u %llx", &param, &value);

	if (ret != 2) {
		ret = -EINVAL;
		goto out;
	}

	ab->fw_dbglog_param = param;
	ab->fw_dbglog_val = value;
	ret = ath12k_wmi_dbglog_cfg(ar, param, value);
	if (ret) {
		ath12k_warn(ab, "dbglog cfg failed from debugfs: %d\n",
			    ret);
		goto out;
	}

	ret = count;

out:
	return ret;
}

static const struct file_operations fops_fw_dbglog = {
	.read = ath12k_read_fw_dbglog,
	.write = ath12k_write_fw_dbglog,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_debug_fw_reset_stats_read(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	int ret;
	size_t len = 0, buf_len = 500;

	void *buf __free(kfree) = kzalloc(buf_len, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ab->base_lock);
	len += scnprintf(buf + len, buf_len - len,
			 "fw_crash_counter\t\t%d\n", ab->stats.fw_crash_counter);
	len += scnprintf(buf + len, buf_len - len,
			 "last_recovery_time\t\t%d\n", ab->stats.last_recovery_time);
	spin_unlock_bh(&ab->base_lock);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	return ret;
}

static const struct file_operations fops_fw_reset_stats = {
	.open = ath12k_debugfs_open,
	.read = ath12k_debug_fw_reset_stats_read,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dump_dp_mon_pdev_stats(struct file *file, char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k *ar;
	struct ath12k_pdev_mon_dp_stats *mon_stats = NULL;
	struct ath12k_pdev_mon_stats *rx_mon_stats = NULL;
	struct ath12k_pdev *pdev;
	struct ath12k_dp_mon *dp_mon = ab->dp->dp_mon;
	u32 tot_used_frags = 0, tot_free_frags = dp_mon->num_frag_free;
	int len = 0, i, j, ret, size = 2048, ppdu_id;
	u8 *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf + len, size - len, "device mon specific stats:\n");

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (!ar)
			continue;

		mon_stats = &ar->dp.dp_mon_pdev->mon_stats;
		rx_mon_stats = &ar->dp.dp_mon_pdev->mon_data.rx_mon_stats;

		len += scnprintf(buf + len, size - len,
				 "*******************radio[%u]*****************\n", i);

		if (ab->hw_params->quad_ring_monitor_support) {
			len += scnprintf(buf + len, size - len,
					 "monitor type: quad ring\n");
			/* Status ring summary */
			len += scnprintf(buf + len, size - len, "\n");
			len += scnprintf(buf + len, size - len,
					 "STATUS RING:\n");
			len += scnprintf(buf + len, size - len,
					 "ppdu_done %u invalid_desc %u tlv_tag_err %u buf_done_war %u\n",
					 rx_mon_stats->status_ppdu_done,
					 rx_mon_stats->status_desc_invalid,
					 rx_mon_stats->status_tlv_tag_err,
					 rx_mon_stats->status_buf_done_war);
			len += scnprintf(buf + len, size - len,
					 "rx_err_desc_sanity_fail %u\n",
					 rx_mon_stats->rx_err_desc_sanity_fail);
			/* Destination ring summary */
			len += scnprintf(buf + len, size - len, "\n");
			len += scnprintf(buf + len, size - len,
					 "DEST RING:\n");
			len += scnprintf(buf + len, size - len,
					 "ppdu_done %u mpdu_done %u mpdu_drop %u\n",
					 rx_mon_stats->dest_ppdu_done,
					 rx_mon_stats->dest_mpdu_done,
					 rx_mon_stats->dest_mpdu_drop);
			len += scnprintf(buf + len, size - len,
					 "dup_linkdesc %u dup_buf %u empty_sw_desc %u\n",
					 rx_mon_stats->dup_mon_linkdesc_cnt,
					 rx_mon_stats->dup_mon_buf_cnt,
					 rx_mon_stats->empty_mon_sw_desc_cnt);
			len += scnprintf(buf + len, size - len,
					 "stuck %u not_reaped %u invalid_msdu %u\n",
					 rx_mon_stats->dest_mon_stuck,
					 rx_mon_stats->dest_mon_not_reaped,
					 rx_mon_stats->invalid_msdu_cnt);
			/* PPDU ID correlation */
			len += scnprintf(buf + len, size - len, "\n");
			len += scnprintf(buf + len, size - len,
					 "PPDU ID CORRELATION:\n");
			len += scnprintf(buf + len, size - len,
					 "match %u mismatch %u (hist_idx %u)\n",
					 rx_mon_stats->ppdu_id_match,
					 rx_mon_stats->ppdu_id_mismatch,
					 rx_mon_stats->ppdu_id_hist_idx);
			len += scnprintf(buf + len, size - len,
					 "status_ring_ppdu_id_hist (MISMATCH context):\n");
			for (j = 0; j < MAX_PPDU_ID_HIST; j++) {
				ppdu_id = rx_mon_stats->status_ring_ppdu_id_hist[j];
				len += scnprintf(buf + len, size - len, "%u%s",
						 ppdu_id,
						 ((j + 1) % 16) ? " " : "\n");
				if (len >= size - 64)
					break;
			}
			len += scnprintf(buf + len, size - len,
					 "\ndest_ring_ppdu_id_hist (MISMATCH context):\n");
			for (j = 0; j < MAX_PPDU_ID_HIST; j++) {
				ppdu_id = rx_mon_stats->dest_ring_ppdu_id_hist[j];
				len += scnprintf(buf + len, size - len, "%u%s",
						 ppdu_id,
						 ((j + 1) % 16) ? " " : "\n");
				if (len >= size - 64)
					break;
			}
			len += scnprintf(buf + len, size - len, "\n");
		} else {
			len += scnprintf(buf + len, size - len,
					 "monitor type: dual ring\n");
			len += scnprintf(buf + len, size - len,
					 "status frags reap: %u process: %u free: %u\n",
					 mon_stats->status_buf_reaped,
					 mon_stats->status_buf_processed,
					 mon_stats->status_buf_free);
			len += scnprintf(buf + len, size - len,
					 "pkt frags proc %u free %u to_mac80211 %u truncated %u\n"
					 , mon_stats->pkt_tlv_processed,
					 mon_stats->pkt_tlv_free,
					 mon_stats->pkt_tlv_to_mac80211,
					 mon_stats->pkt_tlv_truncated);
			len += scnprintf(buf + len, size - len,
					 "Ring desc empty: %u flush %u truncated %u droptlv %u\n",
					 mon_stats->ring_desc_empty,
					 mon_stats->ring_desc_flush,
					 mon_stats->ring_desc_trunc,
					 mon_stats->drop_tlv);
			len += scnprintf(buf + len, size - len,
					 "skb alloc: %u free: %u to_mac80211: %u\n",
					 mon_stats->num_skb_alloc,
					 mon_stats->num_skb_free,
					 mon_stats->num_skb_to_mac80211);
			len += scnprintf(buf + len, size - len,
					 "raw mode skb: %u frag %u eth mode skb: %u frag:%u\n",
					 mon_stats->num_skb_raw,
					 mon_stats->num_frag_raw,
					 mon_stats->num_skb_eth,
					 mon_stats->num_frag_eth);
			len += scnprintf(buf + len, size - len,
					 "Num of PPDU reaped %u processed %u\n",
					 mon_stats->num_ppdu_reaped,
					 mon_stats->num_ppdu_processed);
			len += scnprintf(buf + len, size - len,
					"Empty desc free list: %u\n",
					 mon_stats->ppdu_desc_free_list_empty_cnt);
			len += scnprintf(buf + len, size - len,
					 "Insufficient restitch frags cnt %u\n",
					 mon_stats->restitch_insuff_frags_cnt);

			tot_used_frags += mon_stats->status_buf_processed +
					  mon_stats->pkt_tlv_processed;

			tot_free_frags +=
				mon_stats->status_buf_free + mon_stats->pkt_tlv_free +
				mon_stats->pkt_tlv_to_mac80211;
		}
	}

	if (!ab->hw_params->quad_ring_monitor_support) {
		len += scnprintf(buf + len, size - len,
				 "frags replenished_cnt: %u used cnt %u tot_free_frags %u\n",
				 dp_mon->num_frag_replenish,
				 tot_used_frags,
				 tot_free_frags);

		tot_used_frags = mon_stats->status_buf_reaped +
				 mon_stats->pkt_tlv_processed +
				 dp_mon->num_frag_free;
		len += scnprintf(buf + len, size - len,
				 "\nFrags hold by HW: %u\n",
				 dp_mon->num_frag_replenish - tot_used_frags);

		tot_used_frags = mon_stats->status_buf_reaped +
				 mon_stats->pkt_tlv_processed;
		tot_free_frags = mon_stats->status_buf_free +
				 mon_stats->pkt_tlv_free +
				 mon_stats->pkt_tlv_to_mac80211;
		len += scnprintf(buf + len, size - len, "\nFrags hold by SW: %u\n",
				 (tot_used_frags - tot_free_frags));

		len += scnprintf(buf + len, size - len, "\n SKBs hold by SW: %u\n",
				 mon_stats->num_skb_alloc -
				 (mon_stats->num_skb_free +
				  mon_stats->num_skb_to_mac80211));

		len += scnprintf(buf + len, size - len, "\n ppdu_desc_used: %u\n",
				 mon_stats->ppdu_desc_used);

		len += scnprintf(buf + len, size - len, "\n ppdu_desc_proc: %u\n",
				 mon_stats->ppdu_desc_proc);

		len += scnprintf(buf + len, size - len, "\n ppdu_desc_free: %u\n",
				 mon_stats->ppdu_desc_free);
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t
ath12k_debugfs_write_dp_mon_stats(struct file *file, const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_pdev_mon_dp_stats *mon_stats;
	struct ath12k_pdev_mon_stats *rx_mon_stats;
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct ath12k_dp_mon *dp_mon = ab->dp->dp_mon;
	char buf[20] = {0};
	int ret, i;

	if (count > 20)
		return -EFAULT;

	ret = copy_from_user(buf, user_buf, count);
	if (ret)
		return -EFAULT;

	if (strstr(buf, "reset")) {
		dp_mon->num_frag_replenish = 0;
		dp_mon->num_frag_free = 0;
		for (i = 0; i < ab->num_radios; i++) {
			pdev = &ab->pdevs[i];
			ar = pdev->ar;
			if (ar) {
				mon_stats = &ar->dp.dp_mon_pdev->mon_stats;
				rx_mon_stats = &ar->dp.dp_mon_pdev->mon_data.rx_mon_stats;
				memset(mon_stats, 0, sizeof(*mon_stats));
				memset(rx_mon_stats, 0, sizeof(*rx_mon_stats));
			}
		}
	}

	return count;
}

static const struct file_operations fops_device_mon_stats = {
	.read = ath12k_dump_dp_mon_pdev_stats,
	.write = ath12k_debugfs_write_dp_mon_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

u32 ath12k_dbg_dump_qos_profile(struct ath12k_base *ab,
				char *buf, u8 qos_id, u32 size)
{
	struct ath12k_qos_params params;
	struct ath12k_qos_ctx *qos;
	u32 len = 0;

	qos = ath12k_get_qos(ab);
	if (!qos)
		return len;

	if (!ath12k_qos_configured(ab, qos_id))
		return len;

	spin_lock_bh(&qos->profile_lock);
	memcpy(&params, &qos->profiles[qos_id].params, sizeof(params));
	spin_unlock_bh(&qos->profile_lock);

	if (params.tid != QOS_PARAM_DEFAULT_TID)
		len += scnprintf(buf + len, size - len,
			  "TID: %u\n", params.tid);

	if (params.min_service_interval != QOS_PARAM_DEFAULT_SVC_INTERVAL)
		len += scnprintf(buf + len, size - len,
				 "Service Interval: %u ms\n",
				 params.min_service_interval);
	if (params.min_data_rate != QOS_PARAM_DEFAULT_MIN_THROUGHPUT)
		len += scnprintf(buf + len, size - len,
				 "Min Data Rate: %u Kbps\n",
				 params.min_data_rate);
	if (params.burst_size != QOS_PARAM_DEFAULT_BURST_SIZE)
		len += scnprintf(buf + len, size - len,
				 "Burst Size: %u Bytes\n",
				 params.burst_size);
	if (params.delay_bound != QOS_PARAM_DEFAULT_DELAY_BOUND)
		len += scnprintf(buf + len, size - len,
				 "Delay Bound: %u ms\n",
				 params.delay_bound);
	if (params.msdu_life_time != QOS_PARAM_DEFAULT_TIME_TO_LIVE)
		len += scnprintf(buf + len, size - len,
				 "MSDU Life Time: %u ms\n",
				 params.msdu_life_time);
	return len;
}

static ssize_t ath12k_debugfs_dump_device_mgmt_srng_stats(struct file *file,
							  char __user *user_buf,
							  size_t count,
							  loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_device_mgmt_srng_stats *device_stats;
	static const char *frm_stype[ATH12K_SRNG_STATS_MGMT_FRM_STYPE_MAX - 1] = {
			"Association request", "Association response",
			"Reassociation request", "Reassociation response",
			"Probe request", "Probe response", "Timing Advertisement",
			"Reserved", "Beacon", "ATIM", "Disassociation", "Authentication",
			"Deauthentication", "Action", "Action NoAck"};
	static const char *rxdma_err[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX] = {
			"Overflow", "MPDU len", "FCS", "Decrypt", "TKIP MIC",
			"Unencrypt", "MSDU len", "MSDU limit", "WiFi parse",
			"AMSDU parse", "SA timeout", "DA timeout",
			"Flow timeout", "Flush req", "AMSDU frag", "mcast echo",
			"AMSDU addr mismatch", "Unauth WDS", "Gcast AMSDU WDS"};
	static const char *reo_err[HAL_REO_DEST_RING_ERROR_CODE_MAX] = {
			"Desc addr zero", "Desc invalid", "AMPDU in non BA",
			"Non BA dup", "BA dup", "Frame 2k jump", "BAR 2k jump",
			"Frame OOR", "BAR OOR", "No BA session",
			"Frame SN equal SSN", "PN check", "2k err",
			"PN err", "Desc blocked"};
	int len = 0, i;
	const int size = 4096;

	char *buf __free(kfree) = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (!ab->mgmt)
		return -EINVAL;

	device_stats = &ab->mgmt->srng_stats;

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_cfg_get(ab, ATH12K_CFG_REO_MGMT_PATH_DISABLE))
		ath12k_info(ab, "REO2SW management path is disabled");
#endif /* CPTCFG_QCN_EXTN */

	len += scnprintf(buf + len, size - len, "SOC MGMT SRNG RX STATS:\n\n");

	len += scnprintf(buf + len, size - len, "Delivered packets:\n");
	for (i = 0; i < ATH12K_SRNG_STATS_MGMT_FRM_STYPE_MAX-1; i++)
		len += scnprintf(buf + len, size - len, "  %s: %u\n",
				 frm_stype[i], device_stats->rx_pkts[i]);

#ifdef CPTCFG_QCN_EXTN
	len = ath12k_debugfs_dump_device_mgmt_srng_stats_extn(ab, buf, len, size);
#endif

	len += scnprintf(buf + len, size - len, "Invalid push reason packets: %u\n",
			 device_stats->invalid_push_pkts);

	len += scnprintf(buf + len, size - len, "Invalid type packets: %u\n",
			 device_stats->invalid_pkts);

	len += scnprintf(buf + len, size - len, "BAR packets: %u\n",
			 device_stats->bar_pkts[ATH12K_MGMT_SRNG_PKT_TYPE_RX]);

	len += scnprintf(buf + len, size - len, "Error ring packets: %u\n",
			 device_stats->err_ring_pkts);

	len += scnprintf(buf + len, size - len, "Fragment packets: %u\n",
			 device_stats->frag_pkts);

	len += scnprintf(buf + len, size - len, "BAR error packets: %u\n",
			 device_stats->bar_pkts[ATH12K_MGMT_SRNG_PKT_TYPE_RX_ERR]);

	len += scnprintf(buf + len, size - len, "RXDMA errors:\n");
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "  %s: %u\n",
				 rxdma_err[i], device_stats->rxdma_err[i]);

	len += scnprintf(buf + len, size - len, "REO errors:\n");
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "  %s: %u\n",
				 reo_err[i], device_stats->reo_err[i]);

	len += scnprintf(buf + len, size - len, "REO errors (delivered):\n");
	for (i = 0; i < ATH12K_SRNG_STATS_MGMT_FRM_STYPE_MAX-1; i++)
		len += scnprintf(buf + len, size - len, "  %s: %u\n",
				 frm_stype[i], device_stats->reo_err_rx[i]);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_device_mgmt_srng_stats = {
	.read = ath12k_debugfs_dump_device_mgmt_srng_stats,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dump_svc_sorted_list(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	u8 ac_mask;
	char *buf;
	const int size = 2048;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ac_mask = ab->dp->svc_sort_stats.ac_mask;

	if (ab->dp->arch_ops->dump_svc_sorted_list) {
		len += scnprintf(buf + len, size - len,
				 "\nSC mask 0x%x sorted list:\n",
				 ac_mask);
		len += ab->dp->arch_ops->dump_svc_sorted_list(ab->dp,
							      ac_mask,
							      buf + len,
							      size - len);
	}

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);

	return retval;
}

static const struct file_operations fops_svc_sorted_list = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_svc_sorted_list,
};

static ssize_t ath12k_read_sorted_ac_mask(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	u8 ac_mask;
	char buf[32];
	size_t len;

	ac_mask = ab->dp->svc_sort_stats.ac_mask;

	len = scnprintf(buf, sizeof(buf), "0x%x\n", ac_mask);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_sorted_ac_mask(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	u8 ac_mask;
	char buf[64];
	int num_args;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';

	num_args = sscanf(buf, "0x%hhx", &ac_mask);
	if (num_args != 1)
		return -EINVAL;

	if (ac_mask > ((1 << (IEEE80211_AC_BK + 1)) - 1))
		ac_mask = (1 << (IEEE80211_AC_BK + 1)) - 1;

	ab->dp->svc_sort_stats.ac_mask = ac_mask;

	return count;
}

static const struct file_operations fops_svc_sorted_ac_mask = {
	.read = ath12k_read_sorted_ac_mask,
	.write = ath12k_write_sorted_ac_mask,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_update_tx_msdu_flow(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	u32 tid, service_category, flow_number, hard_drop_threshold;
	char buf[64];
	int num_args, ret;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';

	num_args = sscanf(buf, "0x%x %u %u %u\n",
			  &flow_number, &tid, &service_category, &hard_drop_threshold);
	if (num_args != 4)
		return -EINVAL;

	if (tid >= IEEE80211_MAX_NUM_TIDS)
		return -EINVAL;

	if (service_category > 3)
		return -EINVAL;

	ret = ath12k_dp_arch_update_tx_msdu_flow(ath12k_ab_to_dp(ab),
						 flow_number, tid,
						 service_category,
						 hard_drop_threshold);
	if (ret)
		ath12k_warn(ab, "debugfs update tx msdu flow failed %d flow_number 0x%x tid %d service_category %d\n",
			    ret, flow_number, tid, service_category);

	return count;
}

static const struct file_operations fops_update_tx_msdu_flow = {
	.open = ath12k_debugfs_open,
	.write = ath12k_update_tx_msdu_flow,
};

static ssize_t ath12k_dump_dp_congestion_ctrl_stats(struct file *file,
						    char __user *user_buf,
						    size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char *buf;
	const int size = 4096;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (ab->dp->arch_ops->dump_congestion_ctrl_stats)
		len += ab->dp->arch_ops->dump_congestion_ctrl_stats(ab->dp,
								    buf + len,
								    size - len);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_congestion_ctrl_stats = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_dp_congestion_ctrl_stats,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_congestion_ctrl_param(struct file *file,
						  const char __user *user_buf,
						  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	u32 type, value;
	char buf[64];
	int num_args, ret;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';

	num_args = sscanf(buf, "%u %u", &type, &value);
	if (!num_args || num_args > 2)
		return -EINVAL;

	ret = ath12k_dp_arch_set_congestion_ctrl_param(ath12k_ab_to_dp(ab),
						       type, value);
	if (ret) {
		ath12k_warn(ab,
			    "debugfs congestion ctrl set failed type=%u value=%u ret=%d\n",
			    type, value, ret);
		return ret;
	}

	return count;
}

static const struct file_operations fops_congestion_ctrl_set = {
	.open = ath12k_debugfs_open,
	.write = ath12k_write_congestion_ctrl_param,
	.owner = THIS_MODULE,
};

static ssize_t ath12k_dump_dp_congestion_recovery_history(struct file *file,
							  char __user *user_buf,
							  size_t count,
							  loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char *buf;
	int size = PAGE_SIZE;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (ab->dp->arch_ops->dump_congestion_recovery_hist)
		len += ab->dp->arch_ops->dump_congestion_recovery_hist(ab->dp,
								       buf + len,
								       size - len);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_congestion_recovery_history = {
	.open = ath12k_debugfs_open,
	.read = ath12k_dump_dp_congestion_recovery_history,
	.llseek = default_llseek,
};

static void ath12k_debugfs_dp_svc_sorted_flow_init(struct ath12k_base *ab)
{
	struct dentry *svc_sorted_flow_dir;

	svc_sorted_flow_dir = debugfs_create_dir("svc_sorted_flow",
						 ab->debugfs_soc);

	ath12k_debugfs_create_file("list", 0400, svc_sorted_flow_dir,
				   ab, ab,
				   &fops_svc_sorted_list);
	ath12k_debugfs_create_file("ac_mask", 0600, svc_sorted_flow_dir,
				   ab, ab,
				   &fops_svc_sorted_ac_mask);

	ab->dp->svc_sort_stats.ac_mask = (1 << (IEEE80211_AC_BK + 1)) - 1;

	ath12k_debugfs_create_file("update_tx_msdu_flow", 0200, svc_sorted_flow_dir,
				   ab, ab,
				   &fops_update_tx_msdu_flow);

	ath12k_debugfs_create_file("congestion_ctrl_stats", 0400, svc_sorted_flow_dir,
				   ab, ab,
				   &fops_congestion_ctrl_stats);

	ath12k_debugfs_create_file("congestion_ctrl_set", 0200, svc_sorted_flow_dir,
				   ab, ab,
				   &fops_congestion_ctrl_set);

	ath12k_debugfs_create_file("congestion_recovery_history", 0400,
				   svc_sorted_flow_dir,
				   ab, ab, &fops_congestion_recovery_history);
}

void ath12k_debugfs_pdev_create(struct ath12k_base *ab) {
	ath12k_debugfs_create_file("simulate_fw_crash", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_simulate_fw_crash);
	ath12k_debugfs_create_file("set_fw_recovery", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_fw_recovery);
	debugfs_create_file("recovery_in_progress", 0400, ab->debugfs_soc,
			    ab, &fops_recovery_in_progress);

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ath12k_debugfs_create_file("dbs_power_reduction", 0600, ab->debugfs_soc,
				   ab, ab,
				   &dbs_power_reduction);
	ath12k_debugfs_create_file("eth_power_reduction", 0600, ab->debugfs_soc,
				   ab, ab,
				   &eth_power_reduction);
#endif

	ath12k_debugfs_create_file("fw_dbglog_config", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_fw_dbglog);
	ath12k_debugfs_create_file("fw_reset_stats", 0400, ab->debugfs_soc,
				   ab, ab,
				   &fops_fw_reset_stats);
	ath12k_debugfs_create_file("umac_reset_stats", 0400, ab->debugfs_soc,
				   ab, ab,
				   &fops_umac_reset_stats);
	ath12k_debugfs_create_file("device_dp_stats", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_device_dp_stats);
	ath12k_debugfs_create_file("device_mon_stats", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_device_mon_stats);
	debugfs_create_file("dump_srng_stats", 0600, ab->debugfs_soc,
			    ab, &fops_dump_hal_stats);
	ath12k_debugfs_create_file("device_mgmt_srng_stats", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_device_mgmt_srng_stats);
	if (test_bit(WMI_TLV_SERVICE_DYNAMIC_WSI_REMAP_SUPPORT, ab->wmi_ab.svc_map))
		ath12k_debugfs_create_file("wsi_bypass_device", 0600, ab->debugfs_soc,
					   ab, ab,
					   &fops_wsi_bypass_device);

	ath12k_debugfs_create_file("simulate_host_crash", 0600, ab->debugfs_soc,
				   ab, ab,
				   &fops_simulate_host_crash);

	ath12k_debugfs_dp_svc_sorted_flow_init(ab);
}

void ath12k_debugfs_unregister(struct ath12k *ar)
{
	if (!ar->debug.debugfs_pdev)
		return;

	ath12k_deinit_pktlog(ar);
	ath12k_debugfs_nrp_cleanup_all(ar);
	/* Remove symlink under ieee80211/phy* */
	debugfs_remove(ar->debug.debugfs_pdev_symlink);
	debugfs_remove_recursive(ar->debug.debugfs_pdev);
	ar->debug.debugfs_pdev_symlink = NULL;
	ar->debug.debugfs_pdev = NULL;

	/* Remove wmi ctrl stats file */
	debugfs_remove(ar->wmi_ctrl_stat);
	ar->wmi_ctrl_stat = NULL;
}

static ssize_t ath12k_write_twt_add_dialog(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_add_dialog_params params = { 0 };
	u8 buf[128] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf,
		     "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u %u %u %hhu %hhu %hhu %hhu %hhu %u %u %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.wake_intvl_us,
		     &params.wake_intvl_mantis,
		     &params.wake_dura_us,
		     &params.sp_offset_us,
		     &params.twt_cmd,
		     &params.flag_bcast,
		     &params.flag_trigger,
		     &params.flag_flow_type,
		     &params.flag_protection,
		     &params.b_twt_persistence,
		     &params.b_twt_recommendation,
		     &params.link_id_bitmap,
		     &params.r_twt_dl_tid_bitmap,
		     &params.r_twt_ul_tid_bitmap);
	if (ret != 18 && ret != 21)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_add_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_del_dialog(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_del_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.b_twt_persistence,
		     &params.is_bcast_twt);
	if (ret != 7 && ret != 9)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_del_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_pause_dialog(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_pause_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id);
	if (ret != 7)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_pause_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_resume_dialog(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_resume_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.sp_offset_us,
		     &params.next_twt_size);
	if (ret != 9)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_resume_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_btwt_invite_sta(struct file *file,
						const char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_btwt_invite_sta_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.r_twt_dl_tid_bitmap,
		     &params.r_twt_ul_tid_bitmap);
	if (ret != 7 && ret != 9)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_btwt_invite_sta_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_btwt_remove_sta(struct file *file,
						const char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_btwt_remove_sta_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.r_twt_dl_tid_bitmap,
		     &params.r_twt_ul_tid_bitmap);
	if (ret != 7 && ret != 9)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_btwt_remove_sta_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations ath12k_fops_twt_add_dialog = {
	.write = ath12k_write_twt_add_dialog,
	.open = ath12k_debugfs_open
};

static const struct file_operations ath12k_fops_twt_del_dialog = {
	.write = ath12k_write_twt_del_dialog,
	.open = ath12k_debugfs_open
};

static const struct file_operations ath12k_fops_twt_pause_dialog = {
	.write = ath12k_write_twt_pause_dialog,
	.open = ath12k_debugfs_open
};

static const struct file_operations ath12k_fops_twt_resume_dialog = {
	.write = ath12k_write_twt_resume_dialog,
	.open = ath12k_debugfs_open
};

static const struct file_operations ath12k_fops_btwt_invite_sta = {
	.write = ath12k_write_twt_btwt_invite_sta,
	.open = ath12k_debugfs_open
};

static const struct file_operations ath12k_fops_btwt_remove_sta = {
	.write = ath12k_write_twt_btwt_remove_sta,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_write_primary_link(struct file *file,
					 const char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k *ar = ah->radio;
	struct ath12k_link_vif *arvif;
	u8 primary_link, link_id = 0;
	bool is_link_found = false;
	unsigned long links_map = ahvif->links_map;

	if (kstrtou8_from_user(user_buf, count, 0, &primary_link))
		return -EINVAL;

	/* arvif information not available at this point for STA mode.
	 * Hence, store the input value and set the primary_link in
	 * ath12k_mac_ahsta_get_pri_link_id().
	 */
	mutex_lock(&ah->hw_mutex);
	ahvif->overide_primary_umac = true;
	if (ahvif->vif->type == NL80211_IFTYPE_STATION) {
		ahvif->hw_link_id = primary_link;
		mutex_unlock(&ah->hw_mutex);
		return count;
	}

	for_each_set_bit_from(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		arvif = ahvif->link[link_id];
		if (!arvif || !arvif->ar)
			continue;

		ar = arvif->ar;
		if (primary_link == ar->radio_idx) {
			ahvif->hw_link_id = primary_link;
			is_link_found = true;
			break;
		}
	}

	/* If configured hw idx is not up then arvif information not available
	 * at this point. Hence storing the input value and set the primary_link
	 * in ath12k_mac_ahsta_get_pri_link_id().
	 */
	if (!is_link_found) {
		ahvif->hw_link_id = primary_link;
		mutex_unlock(&ah->hw_mutex);
		ath12k_warn(ar->ab,
			    "Link is not up primary link will be updated after sta association:%u\n",
			    primary_link);
		return count;
	}

	ahvif->primary_link_id = arvif->link_id;
	mutex_unlock(&ah->hw_mutex);
	return count;
}

static ssize_t ath12k_read_primary_link(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	struct ath12k_hw *ah = ahvif->ah;
	int len = 0;
	char buf[32] = {0};

	mutex_lock(&ah->hw_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "Primary link_id: %u\n",
			ahvif->primary_link_id);
	mutex_unlock(&ah->hw_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations ath12k_fops_primary_link = {
	.open = ath12k_debugfs_open,
	.write = ath12k_write_primary_link,
	.read = ath12k_read_primary_link,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_wmm_stats_vdev(struct file *file,
					  char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	struct ath12k_hw *ah;
	int len = 0;
	int size = 2048;
	char *buf;
	u64 total_wmm_sent_pkts = 0;
	u64 total_wmm_received_pkts = 0;
	u64 total_wmm_fail_sent = 0;
	u64 total_wmm_fail_received = 0;
	ssize_t retval, buf_len = PAGE_SIZE * 2;

	if (!ahvif)
		return -EINVAL;
	ah = ahvif->ah;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&ah->hw_mutex);
	for (count = 0; count < WME_NUM_AC; count++) {
		total_wmm_sent_pkts += ahvif->wmm_stats.total_wmm_tx_pkts[count];
		total_wmm_received_pkts += ahvif->wmm_stats.total_wmm_rx_pkts[count];
		total_wmm_fail_sent += ahvif->wmm_stats.total_wmm_tx_drop[count];
		total_wmm_fail_received += ahvif->wmm_stats.total_wmm_rx_drop[count];
	}

	len += scnprintf(buf + len, size - len, "Total Tx: %llu\n",
			 total_wmm_sent_pkts);
	len += scnprintf(buf + len, size - len, "Total Rx: %llu\n",
			 total_wmm_received_pkts);
	len += scnprintf(buf + len, size - len, "Total Tx_fail: %llu\n",
			 total_wmm_fail_sent);
	len += scnprintf(buf + len, size - len, "Total Rx_fai: %llu\n",
			 total_wmm_fail_received);

	len += scnprintf(buf + len, size - len,
			 "Tx Count: BE: %llu, BK: %llu, VI: %llu, VO: %llu\n",
			 ahvif->wmm_stats.total_wmm_tx_pkts[0],
			 ahvif->wmm_stats.total_wmm_tx_pkts[1],
			 ahvif->wmm_stats.total_wmm_tx_pkts[2],
			 ahvif->wmm_stats.total_wmm_tx_pkts[3]);
	len += scnprintf(buf + len, size - len,
			 "Tx Drop: BE: %llu, BK: %llu, VI: %llu, VO: %llu\n",
			 ahvif->wmm_stats.total_wmm_tx_drop[0],
			 ahvif->wmm_stats.total_wmm_tx_drop[1],
			 ahvif->wmm_stats.total_wmm_tx_drop[2],
			 ahvif->wmm_stats.total_wmm_tx_drop[3]);
	len += scnprintf(buf + len, size - len,
			 "Rx Count: BE: %llu, BK: %llu, VI: %llu, VO: %llu\n",
			 ahvif->wmm_stats.total_wmm_rx_pkts[0],
			 ahvif->wmm_stats.total_wmm_rx_pkts[1],
			 ahvif->wmm_stats.total_wmm_rx_pkts[2],
			 ahvif->wmm_stats.total_wmm_rx_pkts[3]);
	len += scnprintf(buf + len, size - len,
			 "Rx Drop: BE: %llu, BK: %llu, VI: %llu, VO: %llu\n",
			 ahvif->wmm_stats.total_wmm_rx_drop[0],
			 ahvif->wmm_stats.total_wmm_rx_drop[1],
			 ahvif->wmm_stats.total_wmm_rx_drop[2],
			 ahvif->wmm_stats.total_wmm_rx_drop[3]);

	mutex_unlock(&ah->hw_mutex);

	if (len > size)
		len = size;

	retval = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return retval;
}

static const struct file_operations fops_wmm_stats_vdev = {
	.read = ath12k_read_wmm_stats_vdev,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_write_reset_vdev_wmm_stats(struct file *file,
						 const char __user *ubuf,
						 size_t count, loff_t *ppos)
{
	struct ath12k_vif *ahvif = file->private_data;
	bool enable;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;
	if (enable)
		memset(&ahvif->wmm_stats, 0, sizeof(struct ath12k_wmm_stats));
	else
		return -EINVAL;

	return count;
}

static const struct file_operations ath12k_fops_reset_vdev_wmm_stats = {
	.write = ath12k_write_reset_vdev_wmm_stats,
	.open = ath12k_debugfs_open,
};

static ssize_t ath12k_write_power_save_gtx(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	bool value;

	if (kstrtobool_from_user(user_buf, count, &value))
		return -EINVAL;

	arvif->power_save_gtx = value;

	ath12k_wmi_vdev_set_param_cmd(arvif->ar, arvif->vdev_id,
				      WMI_VDEV_PARAM_GTX_ENABLE, value);

	return count;
}

static ssize_t ath12k_read_power_save_gtx(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	char buf[ATH12K_BUF_SIZE_32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", arvif->power_save_gtx);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations ath12k_power_save_gtx = {
	.open = ath12k_debugfs_open,
	.write = ath12k_write_power_save_gtx,
	.read = ath12k_read_power_save_gtx,
};

void ath12k_debugfs_add_interface(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_hw *hw = arvif->ar->ah->hw;
	struct ath12k_base *ab = arvif->ar->ab;
	struct ieee80211_vif *vif = ahvif->vif;
	u8 link_id = arvif->link_id;

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return;

	if (ahvif->vif->type == NL80211_IFTYPE_STATION)
		goto ap_and_sta_debugfs_file;

	if (vif->type != NL80211_IFTYPE_AP)
		return;

	if (arvif->debugfs_twt)
		return;

	if (!hw->wiphy->n_radio)
		arvif->debugfs_twt = debugfs_create_dir("twt",
							vif->debugfs_dir);
	else
		arvif->debugfs_twt = debugfs_create_dir("twt",
							vif->link_debugfs[link_id]);

	if (!arvif->debugfs_twt || IS_ERR(arvif->debugfs_twt)) {
		ath12k_warn(ab,
			    "failed to create directory %p\n",
			    arvif->debugfs_twt);
		arvif->debugfs_twt = NULL;
		return;
	}

	ath12k_debugfs_create_file("add_dialog", 0200, arvif->debugfs_twt,
				   ab, arvif, &ath12k_fops_twt_add_dialog);

	ath12k_debugfs_create_file("del_dialog", 0200, arvif->debugfs_twt,
				   ab, arvif, &ath12k_fops_twt_del_dialog);

	ath12k_debugfs_create_file("pause_dialog", 0200, arvif->debugfs_twt,
				   ab, arvif, &ath12k_fops_twt_pause_dialog);

	ath12k_debugfs_create_file("resume_dialog", 0200, arvif->debugfs_twt,
				   ab, arvif, &ath12k_fops_twt_resume_dialog);

	ath12k_debugfs_create_file("btwt_invite_sta", 0200, arvif->debugfs_twt,
				   ab, arvif, &ath12k_fops_btwt_invite_sta);

	ath12k_debugfs_create_file("btwt_remove_sta", 0200, arvif->debugfs_twt,
				   ab, arvif, &ath12k_fops_btwt_remove_sta);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (!ahvif->debugfs_rfs_core_mask) {
		ahvif->debugfs_rfs_core_mask =
				ath12k_debugfs_create_file("rfs_core_mask",
							   0644,
							   vif->debugfs_dir,
							   ab, ahvif,
							   &ath12k_fops_rfs_core_mask);
		if (IS_ERR(ahvif->debugfs_rfs_core_mask))
			ahvif->debugfs_rfs_core_mask = NULL;
	}
#endif

	arvif->debugfs_power_save_gtx =
				ath12k_debugfs_create_file("power_save_gtx", 0644,
							   vif->link_debugfs[link_id],
							   ab, arvif,
							   &ath12k_power_save_gtx);

#ifdef CPTCFG_QCN_EXTN
	ath12k_gpr_debugfs_register_extn(arvif, vif->link_debugfs[link_id]);
#endif /* CPTCFG_QCN_EXTN */

	/* Note: Add new AP mode only debugfs file before "ap_and_sta_debugfs_file" label.
	 * Add new debugfs file for both AP and STA mode after the "ap_and_sta_debugfs_file"
	 * label.
	 */
ap_and_sta_debugfs_file:

	if (ahvif->debugfs_primary_link)
		return;

	ahvif->debugfs_primary_link =
				ath12k_debugfs_create_file("primary_link",
							   0644,
							   vif->debugfs_dir,
							   ab, ahvif,
							   &ath12k_fops_primary_link);

	if (ahvif->debugfs_wmm_stats_vdev)
		return;

	ahvif->debugfs_wmm_stats_vdev = ath12k_debugfs_create_file("wmm_stats", 0644,
								   vif->debugfs_dir,
								   ab, ahvif,
								   &fops_wmm_stats_vdev);

	if (ahvif->debugfs_reset_wmm_stats)
		return;

	ahvif->debugfs_reset_wmm_stats =
			ath12k_debugfs_create_file("reset_wmm_stats",
						   0644,
						   vif->debugfs_dir,
						   ab, ahvif,
						   &ath12k_fops_reset_vdev_wmm_stats);

	if (ahvif->debugfs_vdev_tid_stats)
		return;

	ahvif->debugfs_vdev_tid_stats =
				ath12k_debugfs_create_file("dp_tid_stats",
							   0644,
							   vif->debugfs_dir,
							   ab, ahvif,
							   &ath12k_fops_vdev_tid_stats);

	if (ahvif->debugfs_reset_dp_tid_stats)
		return;

	ahvif->debugfs_reset_dp_tid_stats =
			ath12k_debugfs_create_file("reset_dp_tid_stats",
						   0644,
						   vif->debugfs_dir,
						   ab, ahvif,
						   &ath12k_fops_reset_dp_tid_stats);

	/* If debugfs_primary_link already exist, don't remove */
	if (IS_ERR(ahvif->debugfs_primary_link) &&
	    PTR_ERR(ahvif->debugfs_primary_link) != -EEXIST) {
		ath12k_warn(ab,
			    "failed to create primary_link file, vif %pM",
			    vif->addr);
		debugfs_remove(ahvif->debugfs_primary_link);
		ahvif->debugfs_primary_link = NULL;
	}
}

void ath12k_debugfs_remove_interface(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif;
	u8 link_id;

	if (!ahvif)
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		return;

	vif = ahvif->vif;
	link_id = arvif->link_id;

	if (!vif || link_id >= ATH12K_NUM_MAX_LINKS)
		return;

	if (vif->type == NL80211_IFTYPE_AP)
		arvif->debugfs_twt = NULL;
	arvif->debugfs_power_save_gtx = NULL;

	/* Per-vif debugfs entries - cleanup only when removing last link */
	if (hweight16(ahvif->links_map) <= 1) {
		if (ahvif->vif->type != NL80211_IFTYPE_MESH_POINT)
			ahvif->debugfs_primary_link = NULL;

		ahvif->debugfs_vdev_tid_stats = NULL;
		ahvif->debugfs_reset_dp_tid_stats = NULL;
		ahvif->debugfs_rfs_core_mask = NULL;
	}
}

struct dentry *ath12k_debugfs_erp_create(void)
{
	struct dentry *debugfs_ath12k, *erp_dir;
	bool dput_needed;

	debugfs_ath12k = debugfs_lookup("ath12k", NULL);
	if (debugfs_ath12k) {
		/* a dentry from lookup() needs dput() after we don't use it */
		dput_needed = true;
	} else {
		debugfs_ath12k = debugfs_create_dir("ath12k", NULL);
		if (IS_ERR_OR_NULL(debugfs_ath12k))
			return NULL;
		dput_needed = false;
	}

	erp_dir = debugfs_create_dir("erp", debugfs_ath12k);

	if (dput_needed)
		dput(debugfs_ath12k);

	return erp_dir;
}
