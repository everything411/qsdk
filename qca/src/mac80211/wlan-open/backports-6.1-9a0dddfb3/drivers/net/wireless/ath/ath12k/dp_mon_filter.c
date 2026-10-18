// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "dp_mon_filter.h"
#include "debugfs.h"
#include "dp_rx.h"

int ath12k_dp_mon_rx_filter_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_rx_filter **rx_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	enum dp_mon_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev) {
		ath12k_warn(dp, "Monitor pdev is NULL\n");
		return -EINVAL;
	}

	rx_filter = (struct dp_mon_rx_filter **)kzalloc((
			sizeof(struct dp_mon_rx_filter *) * DP_MON_FILTER_MAX_MODE),
			GFP_KERNEL);

	if (!rx_filter) {
		ath12k_warn(dp, "rx filter alloc failed\n");
		return -ENOMEM;
	}

	dp_mon_pdev->rx_filter = rx_filter;

	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		rx_filter[mode] = (struct dp_mon_rx_filter *)
				kzalloc(sizeof(struct dp_mon_rx_filter) *
				DP_MON_FILTER_SRNG_TYPE_MAX, GFP_KERNEL);
		if (!rx_filter[mode])
			goto free_rx_filter;
	}

	return 0;

free_rx_filter:
	ath12k_dp_mon_rx_filter_free(dp_pdev);
	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_filter_alloc);

void ath12k_dp_mon_rx_filter_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_rx_filter **rx_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	enum dp_mon_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev) {
		ath12k_warn(dp, "Monitor pdev is NULL\n");
		return;
	}

	rx_filter = dp_mon_pdev->rx_filter;

	if (!rx_filter) {
		ath12k_warn(dp, "Monitor rx filter is NULL\n");
		return;
	}

	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		if (!rx_filter[mode])
			continue;
		kfree(rx_filter[mode]);
		rx_filter[mode] = NULL;
	}

	kfree(rx_filter);
	dp_mon_pdev->rx_filter = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_filter_free);

static void
ath12k_dp_mon_rx_display_tlv_filters(struct ath12k_dp *dp,
					struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct ath12k_base *ab =  dp->ab;
	ath12k_dbg(ab, ATH12K_DBG_DATA, "Enable: %d",
		   !tlv_filter->rxmon_disable);
	ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu start: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MPDU_START));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "msdu start: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MSDU_START));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "rx packet: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_RX_PACKET));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "msdu end: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MSDU_END));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu end: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MPDU_END));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "packet header: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "header per msdu: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "attention: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_ATTENTION));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu start: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_START));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end user stats: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end user stats ext: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end status done: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu start user info: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO));
}

void ath12k_dp_mon_rx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_filter_mode mode,
				      struct dp_mon_rx_filter *filter)
{
	struct ath12k_base *ab =  dp->ab;
	struct htt_rx_ring_tlv_filter *tlv_filter =
				&filter->rx_tlv_filter;

	ath12k_dbg(ab, ATH12K_DBG_DATA, "RX MON RING TLV FILTER CONFIG");
	ath12k_dbg(ab, ATH12K_DBG_DATA, "[Mode: %d]: Valid: %d",
		   mode, filter->valid);
	if (filter->valid) {
		ath12k_dp_mon_rx_display_tlv_filters(dp, tlv_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "offset valid: %d",
			   tlv_filter->offset_valid);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "drop threshold valid: %d",
			   tlv_filter->drop_threshold_valid);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "rx drop threshold: %d",
			   tlv_filter->rx_drop_threshold);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable fp: %d", tlv_filter->enable_fp);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable mo: %d", tlv_filter->enable_mo);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable md: %d", tlv_filter->enable_md);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable fpmo: %d",
			   tlv_filter->enable_fpmo);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable log mgmt type: %d",
			   tlv_filter->enable_log_mgmt_type);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable log ctrl type: %d",
			   tlv_filter->enable_log_ctrl_type);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable log data type: %d",
			   tlv_filter->enable_log_data_type);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "conf len mgmt: %d",
			   tlv_filter->conf_len_mgmt);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "conf len ctrl: %d",
			   tlv_filter->conf_len_ctrl);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "conf len data: %d",
			   tlv_filter->conf_len_data);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable rx tlv offset: %d",
			   tlv_filter->enable_rx_tlv_offset);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "rx tlv offset: %d",
			   tlv_filter->rx_tlv_offset);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_mgmt_filter: 0x%x",
			   tlv_filter->fp_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_ctrl_filter: 0x%x",
			   tlv_filter->fp_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_data_filter: 0x%x",
			   tlv_filter->fp_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_mgmt_filter: 0x%x",
			   tlv_filter->mo_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_ctrl_filter: 0x%x",
			   tlv_filter->mo_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_data_filter: 0x%x",
			   tlv_filter->mo_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_mgmt_filter: 0x%x",
			   tlv_filter->md_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_ctrl_filter: 0x%x",
			   tlv_filter->md_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_data_filter: 0x%x",
			   tlv_filter->md_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fpmo_mgmt_filter: 0x%x",
			   tlv_filter->fpmo_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fpmo_ctrl_filter: 0x%x",
			   tlv_filter->fpmo_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fpmo_data_filter: 0x%x",
			   tlv_filter->fpmo_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu start word mask: 0x%x",
			   tlv_filter->rx_mon_mpdu_start_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu end word mask: 0x%x",
			   tlv_filter->rx_mon_mpdu_end_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "msdu end word mask: 0x%x",
			   tlv_filter->rx_mon_msdu_end_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end usr stats word mask: 0x%x",
			   tlv_filter->rx_mon_ppdu_end_usr_stats_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable_fp_packet: %d",
			   tlv_filter->enable_fp_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_packet_mgmt_filter: 0x%x",
			   tlv_filter->fp_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_packet_ctrl_filter: 0x%x",
			   tlv_filter->fp_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_packet_data_filter: 0x%x",
			   tlv_filter->fp_packet_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable_mo_packet: %d",
			   tlv_filter->enable_mo_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_packet_mgmt_filter: 0x%x",
			   tlv_filter->mo_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_packet_ctrl_filter: 0x%x",
			   tlv_filter->mo_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_packet_data_filter: 0x%x",
			   tlv_filter->mo_packet_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable md packet: %d",
			   tlv_filter->enable_md_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_packet_mgmt_filter: 0x%x",
			   tlv_filter->md_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_packet_ctrl_filter: 0x%x",
			   tlv_filter->md_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_packet_data_filter: 0x%x",
			   tlv_filter->md_packet_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable_fpmo_packet: %d",
			   tlv_filter->enable_fpmo_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fpmo_packet_mgmt_filter: 0x%x",
			   tlv_filter->fpmo_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fpmo_packet_ctrl_filter: 0x%x",
			   tlv_filter->fpmo_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fpmo_packet_data_filter: 0x%x",
			   tlv_filter->fpmo_packet_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "rx_mon_enable_hdr_per_ppdu: %d",
			   tlv_filter->rx_mon_enable_hdr_per_ppdu);
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "fp_fpmo_data_mpdu_filter_in_en: %d",
			   tlv_filter->fp_fpmo_data_mpdu_filter_in_en);
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "fp_data_mpdu_tlv_filter0: 0x%x",
			   tlv_filter->fp_data_mpdu_tlv_filter0);
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "fpmo_data_mpdu_tlv_filter0: 0x%x",
			   tlv_filter->fpmo_data_mpdu_tlv_filter0);
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "fp_data_mpdu_tlv_filter1: 0x%x",
			   tlv_filter->fp_data_mpdu_tlv_filter1);
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "fpmo_data_mpdu_tlv_filter1: 0x%x",
			   tlv_filter->fpmo_data_mpdu_tlv_filter1);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_display_filters);

static void
ath12k_dp_mon_rx_setup_stats_filter(struct ath12k_dp *dp,
				    struct htt_rx_ring_tlv_filter *rx_tlv_filter,
				    enum dp_mon_stats_mode mode)
{
	u32 rx_filter = 0;

	rx_filter = HTT_RX_MON_FILTER_TLV_FLAGS;

	if (mode == ATH12k_DP_MON_EXTD_STATS)
		rx_filter = HTT_RX_MON_FILTER_TLV_EXTD_FLAGS;

	rx_tlv_filter->rx_filter = rx_filter;
	rx_tlv_filter->enable_fp = 1;
	rx_tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	rx_tlv_filter->fp_ctrl_filter = FILTER_CTRL_CTRLWRAP | FILTER_CTRL_BA_REQ |
					FILTER_CTRL_BA | FILTER_CTRL_PSPOLL |
					FILTER_CTRL_RTS | FILTER_CTRL_CTS |
					FILTER_CTRL_ACK | FILTER_CTRL_CFEND |
					FILTER_CTRL_CFEND_CFACK;
	rx_tlv_filter->fp_data_filter = FILTER_DATA_NULL | FILTER_DATA_MCAST |
					FILTER_DATA_UCAST;

	rx_tlv_filter->enable_fpmo = 1;
	rx_tlv_filter->fpmo_mgmt_filter = FILTER_MGMT_ALL;
	rx_tlv_filter->fpmo_ctrl_filter = FILTER_CTRL_CTRLWRAP | FILTER_CTRL_BA_REQ |
					  FILTER_CTRL_BA | FILTER_CTRL_PSPOLL |
					  FILTER_CTRL_RTS | FILTER_CTRL_CTS |
					  FILTER_CTRL_ACK | FILTER_CTRL_CFEND |
					  FILTER_CTRL_CFEND_CFACK;
	rx_tlv_filter->fpmo_data_filter = FILTER_DATA_NULL | FILTER_DATA_MCAST |
					  FILTER_DATA_UCAST;

	rx_tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	rx_tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	rx_tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	rx_tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);
}

void ath12k_dp_mon_rx_stats_config_filter(struct ath12k_pdev_dp *dp_pdev,
					  enum dp_mon_stats_mode stats_mode,
					  bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode filter_mode = DP_MON_FILTER_STATS_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_stats_filter(dp, rx_tlv_filter,
						    stats_mode);
		ath12k_dp_mon_rx_display_filters(dp, filter_mode, &rx_filter);
		dp_mon_pdev->rx_filter[filter_mode][srng_type] = rx_filter;
	} else {
		ath12k_dp_mon_rx_display_filters(dp, filter_mode, &rx_filter);
		dp_mon_pdev->rx_filter[filter_mode][srng_type] = rx_filter;
	}
}

void ath12k_dp_mon_rx_prepare_filter(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_filter_srng_type srng_type,
				     struct dp_mon_rx_filter *rx_mon_filter)
{
	u32 mode = 0;
	struct ath12k_base *ab = dp->ab;
	struct htt_rx_ring_tlv_filter *dst_tlv_filter = &rx_mon_filter->rx_tlv_filter;
	struct htt_rx_ring_tlv_filter *src_tlv_filter;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_rx_filter *src_rx_mon_filter;

	/*
	 * loop through all the modes
	 */
	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		src_rx_mon_filter = &dp_mon_pdev->rx_filter[mode][srng_type];
		src_tlv_filter = &src_rx_mon_filter->rx_tlv_filter;

		if (!src_rx_mon_filter->valid)
			continue;

		rx_mon_filter->valid = true;
		if (mode == DP_MON_FILTER_MONITOR_MODE ||
				mode == DP_MON_FILTER_EXT_MON_MODE)
			dst_tlv_filter->is_monitor_mode = true;

		dst_tlv_filter->offset_valid |= src_tlv_filter->offset_valid;
		dst_tlv_filter->rx_filter |= src_tlv_filter->rx_filter;
		dst_tlv_filter->drop_threshold_valid |=
					src_tlv_filter->drop_threshold_valid;
		dst_tlv_filter->rx_drop_threshold |=
					src_tlv_filter->rx_drop_threshold;
		dst_tlv_filter->rx_hdr_len |=
					src_tlv_filter->rx_hdr_len;
		dst_tlv_filter->enable_log_mgmt_type |=
					src_tlv_filter->enable_log_mgmt_type;
		dst_tlv_filter->enable_log_ctrl_type |=
					src_tlv_filter->enable_log_ctrl_type;
		dst_tlv_filter->enable_log_data_type |=
					src_tlv_filter->enable_log_data_type;
		dst_tlv_filter->conf_len_ctrl |= src_tlv_filter->conf_len_ctrl;
		dst_tlv_filter->conf_len_mgmt |= src_tlv_filter->conf_len_mgmt;
		dst_tlv_filter->conf_len_data |= src_tlv_filter->conf_len_data;
		dst_tlv_filter->enable_rx_tlv_offset |=
					src_tlv_filter->enable_rx_tlv_offset;
		dst_tlv_filter->rx_tlv_offset |= src_tlv_filter->rx_tlv_offset;

		dst_tlv_filter->enable_fp |= src_tlv_filter->enable_fp;
		dst_tlv_filter->fp_mgmt_filter |= src_tlv_filter->fp_mgmt_filter;
		dst_tlv_filter->fp_ctrl_filter |= src_tlv_filter->fp_ctrl_filter;
		dst_tlv_filter->fp_data_filter |= src_tlv_filter->fp_data_filter;
		dst_tlv_filter->enable_mo |= src_tlv_filter->enable_mo;
		dst_tlv_filter->mo_mgmt_filter |= src_tlv_filter->mo_mgmt_filter;
		dst_tlv_filter->mo_ctrl_filter |= src_tlv_filter->mo_ctrl_filter;
		dst_tlv_filter->mo_data_filter |= src_tlv_filter->mo_data_filter;
		dst_tlv_filter->enable_md |= src_tlv_filter->enable_md;
		dst_tlv_filter->md_mgmt_filter |= src_tlv_filter->md_mgmt_filter;
		dst_tlv_filter->md_ctrl_filter |= src_tlv_filter->md_ctrl_filter;
		dst_tlv_filter->md_data_filter |= src_tlv_filter->md_data_filter;
		dst_tlv_filter->enable_fpmo |= src_tlv_filter->enable_fpmo;
		dst_tlv_filter->fpmo_mgmt_filter |= src_tlv_filter->fpmo_mgmt_filter;
		dst_tlv_filter->fpmo_ctrl_filter |= src_tlv_filter->fpmo_ctrl_filter;
		dst_tlv_filter->fpmo_data_filter |= src_tlv_filter->fpmo_data_filter;

		dst_tlv_filter->rx_mon_mpdu_start_wmask |=
					src_tlv_filter->rx_mon_mpdu_start_wmask;
		dst_tlv_filter->rx_mon_mpdu_end_wmask |=
					src_tlv_filter->rx_mon_mpdu_end_wmask;
		dst_tlv_filter->rx_mon_msdu_end_wmask |=
					src_tlv_filter->rx_mon_msdu_end_wmask;
		dst_tlv_filter->rx_mon_ppdu_end_usr_stats_wmask |=
					src_tlv_filter->rx_mon_ppdu_end_usr_stats_wmask;
		dst_tlv_filter->enable_fp_packet |= src_tlv_filter->enable_fp_packet;
		dst_tlv_filter->fp_packet_mgmt_filter |=
					src_tlv_filter->fp_packet_mgmt_filter;
		dst_tlv_filter->fp_packet_ctrl_filter |=
					src_tlv_filter->fp_packet_ctrl_filter;
		dst_tlv_filter->fp_packet_data_filter |=
					src_tlv_filter->fp_packet_data_filter;
		dst_tlv_filter->enable_mo_packet |= src_tlv_filter->enable_mo_packet;
		dst_tlv_filter->mo_packet_mgmt_filter |=
					src_tlv_filter->mo_packet_mgmt_filter;
		dst_tlv_filter->mo_packet_ctrl_filter |=
					src_tlv_filter->mo_packet_ctrl_filter;
		dst_tlv_filter->mo_packet_data_filter |=
					src_tlv_filter->mo_packet_data_filter;
		dst_tlv_filter->enable_md_packet |= src_tlv_filter->enable_md_packet;
		dst_tlv_filter->md_packet_mgmt_filter |=
					src_tlv_filter->md_packet_mgmt_filter;
		dst_tlv_filter->md_packet_ctrl_filter |=
					src_tlv_filter->md_packet_ctrl_filter;
		dst_tlv_filter->md_packet_data_filter |=
					src_tlv_filter->md_packet_data_filter;
		dst_tlv_filter->enable_fpmo_packet |= src_tlv_filter->enable_fpmo_packet;
		dst_tlv_filter->fpmo_packet_mgmt_filter |=
					src_tlv_filter->fpmo_packet_mgmt_filter;
		dst_tlv_filter->fpmo_packet_ctrl_filter |=
					src_tlv_filter->fpmo_packet_ctrl_filter;
		dst_tlv_filter->fpmo_packet_data_filter |=
					src_tlv_filter->fpmo_packet_data_filter;

		dst_tlv_filter->rx_mon_fpmo_data_hdrlen |=
					src_tlv_filter->rx_mon_fpmo_data_hdrlen;
		dst_tlv_filter->rx_mon_fpmo_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_fpmo_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_fpmo_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_fpmo_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_fp_data_hdrlen |=
					src_tlv_filter->rx_mon_fp_data_hdrlen;
		dst_tlv_filter->rx_mon_fp_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_fp_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_fp_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_fp_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_mo_data_hdrlen |=
					src_tlv_filter->rx_mon_mo_data_hdrlen;
		dst_tlv_filter->rx_mon_mo_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_mo_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_mo_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_mo_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_md_data_hdrlen |=
					src_tlv_filter->rx_mon_md_data_hdrlen;
		dst_tlv_filter->rx_mon_md_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_md_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_md_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_md_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_enable_hdr_per_ppdu |=
					src_tlv_filter->rx_mon_enable_hdr_per_ppdu;
		dst_tlv_filter->fp_phy_err |= src_tlv_filter->fp_phy_err;
		dst_tlv_filter->fp_phy_err_buf_src |=
					src_tlv_filter->fp_phy_err_buf_src;
		dst_tlv_filter->fp_phy_err_buf_dest |=
					src_tlv_filter->fp_phy_err_buf_dest;
		dst_tlv_filter->phy_err_mask |= src_tlv_filter->phy_err_mask;
		dst_tlv_filter->phy_err_mask_cont |=
					src_tlv_filter->phy_err_mask_cont;
		dst_tlv_filter->phy_err_filter_valid |=
					src_tlv_filter->phy_err_filter_valid;
		dst_tlv_filter->fp_fpmo_data_mpdu_filter_in_en |=
					src_tlv_filter->fp_fpmo_data_mpdu_filter_in_en;
		dst_tlv_filter->fp_data_mpdu_tlv_filter0 |=
					src_tlv_filter->fp_data_mpdu_tlv_filter0;
		dst_tlv_filter->fpmo_data_mpdu_tlv_filter0 |=
					src_tlv_filter->fpmo_data_mpdu_tlv_filter0;
		dst_tlv_filter->fp_data_mpdu_tlv_filter1 |=
					src_tlv_filter->fp_data_mpdu_tlv_filter1;
		dst_tlv_filter->fpmo_data_mpdu_tlv_filter1 |=
					src_tlv_filter->fpmo_data_mpdu_tlv_filter1;

		ath12k_dbg(ab, ATH12K_DBG_DATA, "Updated Rx filters for mode: %d", mode);
		ath12k_dp_mon_rx_display_filters(dp, mode, rx_mon_filter);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_prepare_filter);

int
ath12k_dp_mon_rx_config_filters(struct ath12k_dp *dp,
				struct ath12k_pdev_dp *dp_pdev,
				enum dp_mon_filter_srng_type srng_type,
				struct htt_rx_ring_tlv_filter *rx_tlv_filter)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int i, ret;
	struct ath12k_base *ab = dp->ab;

#ifndef CPTCFG_QCN_EXTN
	(void)ab;
#endif

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		int ring_buf_size, mac_id, ring_id;
		enum hal_ring_type ring_type;

		switch (srng_type) {
		case DP_MON_FILTER_SRNG_TYPE_RXMON_DEST:
			ring_id = dp_mon_pdev->rxdma_mon_dst_ring[i].ring_id;
			ring_type = HAL_RXDMA_MONITOR_DST;
			mac_id = dp_pdev->mac_id + i;
			ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;
			break;
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF:
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ring_type = HAL_RXDMA_BUF;
			mac_id = i;
			ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;
			break;
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF:
			ring_id =
			dp_mon->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
			ring_type = HAL_RXDMA_MONITOR_BUF;
			mac_id = i;
			ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;
			break;
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS:
			ring_id =
			dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
			ring_type = HAL_RXDMA_MONITOR_STATUS;
			mac_id = i;
			ring_buf_size = RX_MON_STATUS_BUF_SIZE;
			break;
		default:
			return -EINVAL;
		}
		ret = ath12k_dp_tx_htt_rx_filter_setup(dp->ab, ring_id, mac_id,
						       ring_type, ring_buf_size,
						       dp_mon_pdev->rx_pktlog_mode,
						       rx_tlv_filter);
		if (ret) {
			ath12k_err(dp->ab,
				   "failed to setup filter for monitor ring id = %d"
				   "srng_type = %d ret = %d\n", ring_id, srng_type, ret);
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_config_filters);

int ath12k_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	int ret = 0;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_prepare_filter(dp, dp_pdev, srng_type, &rx_filter);
	if (rx_filter.valid)
		rx_tlv_filter->rxmon_disable = false;
	else
		rx_tlv_filter->rxmon_disable = true;

	ret = ath12k_dp_mon_rx_config_filters(dp, dp_pdev, srng_type,
					      rx_tlv_filter);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_ring_filter);

void
ath12k_dp_mon_rx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_rx_ring_tlv_filter *tlv_filter,
				       u32 rx_mon_tlv_filter_flags)
{
	tlv_filter->offset_valid = false;
	tlv_filter->rx_filter = rx_mon_tlv_filter_flags;
	tlv_filter->drop_threshold_valid = true;
	tlv_filter->rx_drop_threshold = HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE;
	tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;

	tlv_filter->enable_log_mgmt_type = true;
	tlv_filter->enable_log_ctrl_type = true;
	tlv_filter->enable_log_data_type = true;

	tlv_filter->conf_len_ctrl = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_mgmt = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_data = HTT_RX_RING_DEFAULT_DMA_LENGTH;

	tlv_filter->enable_rx_tlv_offset = true;
	tlv_filter->rx_tlv_offset = HTT_RX_RING_PKT_TLV_OFFSET;

	tlv_filter->enable_fp = 1;
	tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_data_filter = FILTER_DATA_ALL;
	tlv_filter->enable_mo = 1;
	tlv_filter->mo_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_data_filter = FILTER_DATA_ALL;

	tlv_filter->enable_fp_packet = 1;
	tlv_filter->fp_packet_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_packet_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_packet_data_filter = FILTER_DATA_ALL;
	tlv_filter->enable_mo_packet = 1;
	tlv_filter->mo_packet_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_packet_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_packet_data_filter = FILTER_DATA_ALL;

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);

	tlv_filter->rx_mon_fp_data_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fp_ctrl_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fp_mgmt_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_data_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_ctrl_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_mgmt_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_setup_mon_mode_filter);

void ath12k_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev,
					     bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	u32 rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_DEST_RING;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_mon_mode_filter(dp,
						       rx_tlv_filter,
						       rx_mon_tlv_filter_flags);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

void static
ath12k_dp_mon_rx_setup_nrp_filters(struct ath12k_dp *dp,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	u32 rx_filter = 0;

	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_MPDU_START;
	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_START;
	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END;
	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_MPDU_END;

	tlv_filter->rx_filter = rx_filter;
	tlv_filter->enable_md = 1;
	tlv_filter->md_data_filter = FILTER_DATA_ALL;

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);
}

void ath12k_dp_mon_rx_nrp_config_filter(struct ath12k_pdev_dp *dp_pdev,
					bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_NRP_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_nrp_filters(dp, rx_tlv_filter);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

static void
ath12k_dp_mon_rx_setup_smart_mon_filters(struct ath12k_dp *dp,
					 struct htt_rx_ring_tlv_filter *tlv_filter,
					 u8 smart_mon_filter)
{
	u32 rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_DEST_RING;

	tlv_filter->offset_valid = false;
	tlv_filter->rx_filter = rx_mon_tlv_filter_flags;
	tlv_filter->drop_threshold_valid = true;
	tlv_filter->rx_drop_threshold = HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE;
	tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;

	tlv_filter->enable_log_mgmt_type = true;
	tlv_filter->enable_log_ctrl_type = true;
	tlv_filter->enable_log_data_type = true;

	tlv_filter->conf_len_ctrl = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_mgmt = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_data = HTT_RX_RING_DEFAULT_DMA_LENGTH;

	tlv_filter->enable_rx_tlv_offset = true;
	tlv_filter->rx_tlv_offset = HTT_RX_RING_PKT_TLV_OFFSET;

	tlv_filter->enable_md = 1;
	tlv_filter->enable_md_packet = 1;

	if (!(smart_mon_filter & DP_SMART_MON_DATA_FILTER)) {
		tlv_filter->md_data_filter = FILTER_DATA_ALL;
		tlv_filter->md_packet_data_filter = FILTER_DATA_ALL;
	}

	if (!(smart_mon_filter & DP_SMART_MON_MGMT_FILTER)) {
		tlv_filter->md_mgmt_filter = FILTER_MGMT_ALL;
		tlv_filter->md_packet_mgmt_filter = FILTER_MGMT_ALL;
	}

	if (!(smart_mon_filter & DP_SMART_MON_CTRL_FILTER)) {
		tlv_filter->md_ctrl_filter = FILTER_CTRL_ALL;
		tlv_filter->md_packet_ctrl_filter = FILTER_CTRL_ALL;
	}

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);
}

void ath12k_dp_mon_rx_smart_mon_config_filter(struct ath12k_pdev_dp *dp_pdev,
					      bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_NRP_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_smart_mon_filters(dp, rx_tlv_filter,
							 dp_mon_pdev->smart_mon_filter);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

static void
ath12k_dp_ext_mon_setup_rx_filters(struct ath12k_pdev_dp *dp_pdev,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	enum ath12k_ext_mon_frame_len max_short_pkt_len;
	const struct ath12k_dp_arch_mon_ops *mon_ops = ath12k_dp_mon_ops_get(dp);
	u32 rx_mon_tlv_filter_flags = HTT_RX_EXT_MON_FILTER_TLV_FLAGS;
	bool mgmt_full_pkt, ctrl_full_pkt, data_full_pkt;

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;

	tlv_filter->offset_valid = false;
	tlv_filter->rx_filter = rx_mon_tlv_filter_flags;
	tlv_filter->drop_threshold_valid = true;
	tlv_filter->rx_drop_threshold = HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE;
	tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;

	tlv_filter->enable_log_mgmt_type = true;
	tlv_filter->enable_log_ctrl_type = true;
	tlv_filter->enable_log_data_type = true;

	if (rx_ext_mon->fp_enabled) {
		tlv_filter->enable_fp = 1;
		tlv_filter->fp_mgmt_filter =
			rx_ext_mon->fp.filter[ATH12K_EXT_MON_FRAME_MGMT];
		tlv_filter->fp_ctrl_filter =
			rx_ext_mon->fp.filter[ATH12K_EXT_MON_FRAME_CTRL];
		tlv_filter->fp_data_filter =
			rx_ext_mon->fp.filter[ATH12K_EXT_MON_FRAME_DATA];

		if (tlv_filter->fp_mgmt_filter &&
		    rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_MGMT] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->fp_packet_mgmt_filter =
					tlv_filter->fp_mgmt_filter;

		if (tlv_filter->fp_ctrl_filter &&
		    rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_CTRL] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->fp_packet_ctrl_filter =
					tlv_filter->fp_ctrl_filter;

		if (tlv_filter->fp_data_filter &&
		    rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_DATA] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->fp_packet_data_filter =
					tlv_filter->fp_data_filter;

		if (tlv_filter->fp_packet_mgmt_filter ||
		    tlv_filter->fp_packet_ctrl_filter ||
		    tlv_filter->fp_packet_data_filter)
			tlv_filter->enable_fp_packet = 1;
	}

	if (rx_ext_mon->fpmo_enabled) {
		tlv_filter->enable_fpmo = 1;
		tlv_filter->fpmo_mgmt_filter =
			rx_ext_mon->fpmo.filter[ATH12K_EXT_MON_FRAME_MGMT];
		tlv_filter->fpmo_ctrl_filter =
			rx_ext_mon->fpmo.filter[ATH12K_EXT_MON_FRAME_CTRL];
		tlv_filter->fpmo_data_filter =
			rx_ext_mon->fpmo.filter[ATH12K_EXT_MON_FRAME_DATA];

		if (tlv_filter->fpmo_mgmt_filter &&
		    rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_MGMT] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->fpmo_packet_mgmt_filter =
					tlv_filter->fpmo_mgmt_filter;

		if (tlv_filter->fpmo_ctrl_filter &&
		    rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_CTRL] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->fpmo_packet_ctrl_filter =
					tlv_filter->fpmo_ctrl_filter;

		if (tlv_filter->fpmo_data_filter &&
		    rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_DATA] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->fpmo_packet_data_filter =
					tlv_filter->fpmo_data_filter;

		if (tlv_filter->fpmo_packet_mgmt_filter ||
		    tlv_filter->fpmo_packet_ctrl_filter ||
		    tlv_filter->fpmo_packet_data_filter)
			tlv_filter->enable_fpmo_packet = 1;
	}

	if (rx_ext_mon->mo_enabled) {
		tlv_filter->enable_mo = 1;
		tlv_filter->mo_mgmt_filter =
			rx_ext_mon->mo.filter[ATH12K_EXT_MON_FRAME_MGMT];
		tlv_filter->mo_ctrl_filter =
			rx_ext_mon->mo.filter[ATH12K_EXT_MON_FRAME_CTRL];
		tlv_filter->mo_data_filter =
			rx_ext_mon->mo.filter[ATH12K_EXT_MON_FRAME_DATA];

		tlv_filter->mo_ctrl_filter |= tlv_filter->fp_ctrl_filter;

		if (tlv_filter->mo_mgmt_filter &&
		    rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_MGMT] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->mo_packet_mgmt_filter =
					tlv_filter->mo_mgmt_filter;

		if (tlv_filter->mo_ctrl_filter &&
		    rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_CTRL] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->mo_packet_ctrl_filter =
					tlv_filter->mo_ctrl_filter;

		if (tlv_filter->mo_data_filter &&
		    rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_DATA] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->mo_packet_data_filter =
					tlv_filter->mo_data_filter;

		if (tlv_filter->mo_packet_mgmt_filter ||
		    tlv_filter->mo_packet_ctrl_filter ||
		    tlv_filter->mo_packet_data_filter)
			tlv_filter->enable_mo_packet = 1;
	}

	if (rx_ext_mon->md_enabled) {
		tlv_filter->enable_md = 1;
		tlv_filter->md_mgmt_filter |=
			rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_MGMT];
		tlv_filter->md_ctrl_filter |=
			rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_CTRL];
		tlv_filter->md_data_filter |=
			rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_DATA];

		if (rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_MGMT] &&
		    rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_MGMT] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->md_packet_mgmt_filter |=
				rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_MGMT];

		if (rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_CTRL] &&
		    rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_CTRL] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->md_packet_ctrl_filter |=
				rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_CTRL];

		if (rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_DATA] &&
		    rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_DATA] ==
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->md_packet_data_filter |=
				rx_ext_mon->md.filter[ATH12K_EXT_MON_FRAME_DATA];

		if (tlv_filter->md_packet_mgmt_filter ||
		    tlv_filter->md_packet_ctrl_filter ||
		    tlv_filter->md_packet_data_filter)
			tlv_filter->enable_md_packet = 1;
	}

	mgmt_full_pkt = ath12k_dp_ext_mon_full_pkt_enabled(rx_ext_mon,
							   ATH12K_EXT_MON_FRAME_MGMT);
	ctrl_full_pkt = ath12k_dp_ext_mon_full_pkt_enabled(rx_ext_mon,
							   ATH12K_EXT_MON_FRAME_CTRL);
	data_full_pkt = ath12k_dp_ext_mon_full_pkt_enabled(rx_ext_mon,
							   ATH12K_EXT_MON_FRAME_DATA);

	if (mgmt_full_pkt || ctrl_full_pkt || data_full_pkt) {
		tlv_filter->enable_rx_tlv_offset = true;
		tlv_filter->rx_tlv_offset = HTT_RX_RING_PKT_TLV_OFFSET;
		tlv_filter->rx_filter |= HTT_RX_FILTER_TLV_FLAGS_MSDU_END |
					 HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER;

		tlv_filter->conf_len_mgmt = mgmt_full_pkt ?
			HTT_RX_RING_DEFAULT_DMA_LENGTH : HTT_RX_RING_64B_DMA_LENGTH;
		tlv_filter->conf_len_ctrl = ctrl_full_pkt ?
			HTT_RX_RING_DEFAULT_DMA_LENGTH : HTT_RX_RING_64B_DMA_LENGTH;
		tlv_filter->conf_len_data = data_full_pkt ?
			HTT_RX_RING_DEFAULT_DMA_LENGTH : HTT_RX_RING_64B_DMA_LENGTH;
	} else {
		tlv_filter->conf_len_mgmt = HTT_RX_RING_64B_DMA_LENGTH;
		tlv_filter->conf_len_ctrl = HTT_RX_RING_64B_DMA_LENGTH;
		tlv_filter->conf_len_data = HTT_RX_RING_64B_DMA_LENGTH;
	}

	max_short_pkt_len = ath12k_ext_mon_get_max_shortpkt_len(rx_ext_mon);
	if (max_short_pkt_len == ATH12K_EXT_MON_LEN_64B)
		tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;
	else if (max_short_pkt_len == ATH12K_EXT_MON_LEN_128B)
		tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_128_BYTES;
	else if (max_short_pkt_len == ATH12K_EXT_MON_LEN_256B)
		tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_256_BYTES;
	else
		tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;

	if (rx_ext_mon->level == ATH12K_EXT_MON_FILTER_LEVEL_MSDU)
		tlv_filter->rx_filter |= HTT_RX_FILTER_TLV_FLAGS_MSDU_END |
					 HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER;

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);

	if (mon_ops && mon_ops->ext_mon_setup_rx_filter)
		mon_ops->ext_mon_setup_rx_filter(tlv_filter, rx_ext_mon);

	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
}

void ath12k_dp_ext_mon_rx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_EXT_MON_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_ext_mon_setup_rx_filters(dp_pdev, rx_tlv_filter);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

void ath12k_dp_mon_rx_wmask_subscribe(void *ptr,
				      struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd =
				(struct htt_rx_ring_selection_cfg_cmd *)ptr;

	if (tlv_filter->rxmon_disable)
		return;

	if (tlv_filter->rx_mon_mpdu_start_wmask > 0 &&
	    tlv_filter->rx_mon_msdu_end_wmask > 0 &&
	    tlv_filter->rx_mon_ppdu_end_usr_stats_wmask > 0) {
		cmd->info2 |=
			le32_encode_bits(true,
					 HTT_RX_RING_SELECTION_CFG_WORD_MASK_COMPACT_SET);
		cmd->rx_mon_mpdu_start_end_mask =
		le32_encode_bits(tlv_filter->rx_mon_mpdu_start_wmask,
				 HTT_RX_RING_SELECTION_CFG_RX_MON_MPDU_START_MASK);
		cmd->rx_mon_mpdu_start_end_mask |=
			le32_encode_bits(tlv_filter->rx_mon_mpdu_end_wmask,
					 HTT_RX_RING_SELECTION_CFG_RX_MON_MPDU_END_MASK);
		cmd->rx_mon_msdu_end_word_mask =
			le32_encode_bits(tlv_filter->rx_mon_msdu_end_wmask,
					 HTT_RX_RING_SELECTION_CFG_RX_MON_MSDU_END_MASK);
		cmd->rx_mon_ppdu_end_usr_stats_wmask =
		le32_encode_bits(tlv_filter->rx_mon_ppdu_end_usr_stats_wmask,
				 HTT_RX_RING_SELECTION_CFG_RX_MON_PPDU_END_USR_STATS_MASK);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_wmask_subscribe);

void ath12k_dp_mon_rx_enable_packet_filters(void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd =
				(struct htt_rx_ring_selection_cfg_cmd *)ptr;
	u32 word;
	u32 fp_packet_mgmt_filter = tlv_filter->fp_packet_mgmt_filter;
	u32 fp_packet_ctrl_filter = tlv_filter->fp_packet_ctrl_filter;
	u32 fp_packet_data_filter = tlv_filter->fp_packet_data_filter;
	u32 mo_packet_mgmt_filter = tlv_filter->mo_packet_mgmt_filter;
	u32 mo_packet_ctrl_filter = tlv_filter->mo_packet_ctrl_filter;
	u32 mo_packet_data_filter = tlv_filter->mo_packet_data_filter;
	u32 md_packet_mgmt_filter = tlv_filter->md_packet_mgmt_filter;
	u32 md_packet_ctrl_filter = tlv_filter->md_packet_ctrl_filter;
	u32 md_packet_data_filter = tlv_filter->md_packet_data_filter;
	u32 fpmo_packet_mgmt_filter = tlv_filter->fpmo_packet_mgmt_filter;
	u32 fpmo_packet_ctrl_filter = tlv_filter->fpmo_packet_ctrl_filter;
	u32 fpmo_packet_data_filter = tlv_filter->fpmo_packet_data_filter;

	if (tlv_filter->rxmon_disable)
		return;

	word = 0;
	if (tlv_filter->enable_fp_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(&word,
							     fp_packet_mgmt_filter);
	if (tlv_filter->enable_mo_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(&word,
							     mo_packet_mgmt_filter);
	if (tlv_filter->enable_md_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(&word,
							     md_packet_mgmt_filter);
	cmd->pkt_type_en_data_flag0 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fp_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(&word,
							     fp_packet_mgmt_filter);
	if (tlv_filter->enable_mo_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(&word,
							     mo_packet_mgmt_filter);
	if (tlv_filter->enable_md_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(&word,
							     md_packet_mgmt_filter);
	cmd->pkt_type_en_data_flag1 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fp_packet)
		ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(&word,
							     fp_packet_ctrl_filter);
	if (tlv_filter->enable_mo_packet)
		ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(&word,
							     mo_packet_ctrl_filter);
	if (tlv_filter->enable_md_packet)
		ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(&word,
							     md_packet_ctrl_filter);
	cmd->pkt_type_en_data_flag2 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fp_packet) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(&word,
							     fp_packet_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(&word,
							     fp_packet_data_filter);
	}
	if (tlv_filter->enable_mo_packet) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(&word,
							     mo_packet_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(&word,
							     mo_packet_data_filter);
	}
	if (tlv_filter->enable_md_packet) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(&word,
							     md_packet_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(&word,
							     md_packet_data_filter);
	}
	cmd->pkt_type_en_data_flag3 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fpmo_packet) {
		ath12k_dp_tx_htt_rx_mgmt_fpmo_flag0_filter_set(&word,
							       fpmo_packet_mgmt_filter);
		ath12k_dp_tx_htt_rx_ctrl_fpmo_flag0_filter_set(&word,
							       fpmo_packet_ctrl_filter);
	}
	cmd->pkt_type_en_data_fpmo_flags0 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fpmo_packet)
		ath12k_dp_tx_htt_rx_data_fpmo_flag1_filter_set(&word,
							       fpmo_packet_data_filter);
	cmd->pkt_type_en_data_fpmo_flags1 = cpu_to_le32(word);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_enable_packet_filters);

static void
ath12k_dp_mon_rx_pktlog_cmn_status(struct htt_rx_ring_tlv_filter *tlv_filter)
{
	tlv_filter->enable_mo = 1;
	tlv_filter->enable_fp = 1;
	tlv_filter->offset_valid = false;
	tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_data_filter = FILTER_DATA_ALL;
	tlv_filter->mo_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_data_filter = FILTER_DATA_ALL;
}

static void
ath12k_dp_mon_rx_setup_pktlog_lite(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKTLOG_LITE_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter = &rx_filter.rx_tlv_filter;

	rx_filter.valid = true;
	rx_tlv_filter->rx_filter = HTT_RX_FILTER_TLV_PKTLOG_LITE;
	ath12k_dp_mon_rx_pktlog_cmn_status(rx_tlv_filter);

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

static void
ath12k_dp_mon_rx_setup_cfr_rcc_status(struct htt_rx_ring_tlv_filter *tlv_filter,
				      u8 rcc_type)
{
	tlv_filter->offset_valid = false;

	if (rcc_type & DP_MON_RCC_TYPE_M_ALL_FTM_ACK) {
		tlv_filter->enable_fp = 1;
		tlv_filter->enable_mo = 1;

		tlv_filter->fp_mgmt_filter = FILTER_MGMT_ACTION;
		tlv_filter->mo_mgmt_filter = FILTER_MGMT_ACTION;
	}

	if (rcc_type & DP_MON_RCC_TYPE_M_NDPA_NDP_ALL) {
		tlv_filter->enable_fp = 1;
		tlv_filter->enable_mo = 1;

		tlv_filter->fp_ctrl_filter |= FILTER_CTRL_VHT_NDP;
		tlv_filter->mo_ctrl_filter |= FILTER_CTRL_VHT_NDP;
	}

	if (rcc_type & DP_MON_RCC_TYPE_M_ALL_PACKET) {
		tlv_filter->enable_fp = 1;
		tlv_filter->enable_mo = 1;

		tlv_filter->fp_mgmt_filter |= FILTER_MGMT_ALL;
		tlv_filter->mo_mgmt_filter |= FILTER_MGMT_ALL;
		tlv_filter->fp_ctrl_filter |= FILTER_CTRL_ALL;
		tlv_filter->mo_ctrl_filter |= FILTER_CTRL_ALL;
		tlv_filter->fp_data_filter |= FILTER_DATA_ALL;
		tlv_filter->mo_data_filter |= FILTER_DATA_ALL;
	}

	if (rcc_type & DP_MON_RCC_TYPE_M_TA_RA_FILTER) {
		tlv_filter->enable_fp = 1;
		tlv_filter->enable_mo = 1;

		tlv_filter->fp_mgmt_filter |= FILTER_MGMT_ALL;
		tlv_filter->mo_mgmt_filter |= FILTER_MGMT_ALL;
		tlv_filter->fp_ctrl_filter |= FILTER_CTRL_ALL;
		tlv_filter->mo_ctrl_filter |= FILTER_CTRL_ALL;
		tlv_filter->fp_data_filter |= FILTER_DATA_ALL;
		tlv_filter->mo_data_filter |= FILTER_DATA_ALL;
	}

	if (rcc_type & DP_MON_RCC_TYPE_M_DIRECTED_FTM) {
		tlv_filter->enable_fp = 1;

		tlv_filter->fp_mgmt_filter |= FILTER_MGMT_ACTION;
	}

	if (rcc_type & DP_MON_RCC_TYPE_M_NDPA_NDP_DIRECTED) {
		tlv_filter->enable_fp = 1;

		tlv_filter->fp_ctrl_filter |= FILTER_CTRL_VHT_NDP;
	}
}

void ath12k_dp_mon_rx_cfr_rcc_config_filter(struct ath12k_pdev_dp *dp_pdev,
					    bool enable, u8 rcc_type)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_CFR_RCC_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter = &rx_filter.rx_tlv_filter;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter->rx_filter = HTT_RX_FILTER_TLV_CFR_RCC;
		ath12k_dp_mon_rx_setup_cfr_rcc_status(rx_tlv_filter,
						      rcc_type);
	}

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_cfr_rcc_config_filter);

bool ath12k_dp_mon_get_rx_cfr_rcc_filter_valid(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_CFR_RCC_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_rx_filter *rx_filter = &dp_mon_pdev->rx_filter[mode][srng_type];

	return rx_filter->valid;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_rx_cfr_rcc_filter_valid);

static void
ath12k_dp_mon_rx_pktlog_reset(struct ath12k_pdev_dp *dp_pdev,
			      enum dp_mon_filter_mode mode)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

static void
ath12k_dp_mon_rx_setup_pktlog_full(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKTLOG_FULL_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter = &rx_filter.rx_tlv_filter;

	rx_filter.valid = true;
	rx_tlv_filter->rx_filter = HTT_RX_FILTER_TLV_PKTLOG_FULL;
	ath12k_dp_mon_rx_pktlog_cmn_status(rx_tlv_filter);

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

static void
ath12k_dp_mon_rx_pktlog_cbf_status(struct htt_rx_ring_tlv_filter *tlv_filter)
{
	tlv_filter->enable_mo = true;
	tlv_filter->enable_fp = true;
	tlv_filter->offset_valid = false;
	tlv_filter->fp_mgmt_filter = HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION_NOACK;
	tlv_filter->mo_mgmt_filter = HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION_NOACK;
}

/**
 * ath12k_dp_mon_rx_setup_pktlog_cbf() - Setup RX monitor filter for CBF
 * @dp_pdev: DP pdev handle
 *
 * Configure RX monitor destination ring filters to capture Compressed
 * Beamforming (CBF) frames. CBF frames are management action no-ack frames
 * used for beamforming feedback in DL MU-MIMO and TxBF operations.
 *
 * This function configures the RX monitor destination ring filter for
 * management action no-ack frames used by CBF.
 *
 * The filter enables:
 * - MPDU/PPDU status TLVs for frame metadata
 * - Management action no-ack frame capture (type=0, subtype=14)
 * - Word masks for detailed frame information
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_mon_rx_setup_pktlog_cbf(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKTLOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;

	if (!dp || !dp->ab || !dp_mon_pdev || !dp_mon_pdev->rx_filter)
		return -EINVAL;

	if (dp_mon_pdev->rx_pktlog_cbf) {
		ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
			   "RX pktlog CBF already configured\n");
		return 0;
	}

	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	rx_filter.valid = true;

	rx_tlv_filter->rx_filter = HTT_RX_FILTER_TLV_PKTLOG_FULL;

	ath12k_dp_mon_rx_pktlog_cbf_status(rx_tlv_filter);

	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	dp_mon_pdev->rx_pktlog_cbf = true;

	ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
		   "mac %d: RX pktlog CBF mode enabled\n", dp_pdev->mac_id);
	return 0;
}

/**
 * ath12k_dp_mon_rx_reset_pktlog_cbf() - Reset RX monitor filter for CBF
 * @dp_pdev: DP pdev handle
 *
 * Disable CBF capture by resetting the destination ring filter.
 *
 */
static void
ath12k_dp_mon_rx_reset_pktlog_cbf(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKTLOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (!dp_mon_pdev->rx_pktlog_cbf)
		return;

	dp_mon_pdev->rx_pktlog_cbf = false;

	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
		   "Reset CBF destination ring filter\n");
}

/**
 * ath12k_dp_mon_tx_setup_pktlog_hybrid() - Setup TX monitor for hybrid mode
 * @dp_pdev: DP pdev handle
 *
 * Configure TX monitor destination ring filters for hybrid pktlog mode.
 * In hybrid mode, upstream TLVs (MPDU/MSDU START/END) are captured from
 * TX monitor ring while UMAC TLVs (FES_STATUS, PEER_ENTRY, etc.) come
 * via HTT path from firmware.
 *
 * This split is necessary for wifi7 platforms where TX TLVs are no longer
 * delivered entirely through HTT path due to hardware architecture changes.
 */
static void
ath12k_dp_mon_tx_setup_pktlog_hybrid(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_hal *hal = dp->hal;
	struct dp_mon_tx_filter tx_filter = {0};
	enum dp_mon_tx_filter_mode mode = DP_MON_TX_FILTER_PKTLOG_HYBRID;
	enum dp_mon_tx_filter_srng_type srng_type =
		DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;
	struct htt_tx_ring_tlv_filter *tx_tlv_filter;

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon->tx_mon_filter)
		return;

	if (dp_mon_pdev->dp_pdev_tx_mon->tx_pktlog_hybrid) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DATA,
			   "PKTLOG: Hybrid mode already configured\n");
		return;
	}

	tx_filter.valid = true;
	tx_tlv_filter = &tx_filter.filter;

	tx_tlv_filter->tx_mon_upstream_tlv_flags0 =
		HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG0 |
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_USER_RESPONSE;
	tx_tlv_filter->tx_mon_upstream_tlv_flags1 =
		HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG1;

	tx_tlv_filter->tx_mon_mgmt_filter = true;
	tx_tlv_filter->tx_mon_data_filter = true;
	tx_tlv_filter->tx_mon_ctrl_filter = true;

	tx_tlv_filter->mgmt_mpdu_end = 1;
	tx_tlv_filter->mgmt_msdu_end = 1;
	tx_tlv_filter->mgmt_msdu_start = 1;
	tx_tlv_filter->mgmt_mpdu_start = 1;
	tx_tlv_filter->ctrl_mpdu_end = 1;
	tx_tlv_filter->ctrl_msdu_end = 1;
	tx_tlv_filter->ctrl_msdu_start = 1;
	tx_tlv_filter->ctrl_mpdu_start = 1;
	tx_tlv_filter->data_mpdu_end = 1;
	tx_tlv_filter->data_msdu_end = 1;
	tx_tlv_filter->data_msdu_start = 1;
	tx_tlv_filter->data_mpdu_start = 1;

	tx_tlv_filter->mgmt_mpdu_msdu_log_en = true;
	tx_tlv_filter->ctrl_mpdu_msdu_log_en = true;
	tx_tlv_filter->data_mpdu_msdu_log_en = true;

	tx_tlv_filter->mgmt_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	tx_tlv_filter->ctrl_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	tx_tlv_filter->data_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	tx_tlv_filter->tx_mon_mgmt_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	tx_tlv_filter->tx_mon_data_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	tx_tlv_filter->tx_mon_ctrl_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	tx_tlv_filter->pkt_buf_cnt_en = ath12k_hal_mon_tx_pkt_buf_cnt_in_desc(hal);

	dp_mon_pdev->dp_pdev_tx_mon->tx_mon_filter[mode][srng_type] = tx_filter;

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Monitor: Hybrid mode filter configured\n");
}

/**
 * ath12k_dp_mon_tx_reset_pktlog_hybrid() - Reset hybrid mode filters
 * @dp_pdev: DP pdev handle
 *
 * Reset TX monitor filters for hybrid pktlog mode. This is called when
 * pktlog is stopped or when switching to a different pktlog mode.
 */
static void
ath12k_dp_mon_tx_reset_pktlog_hybrid(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_tx_filter tx_filter = {0};
	enum dp_mon_tx_filter_mode mode = DP_MON_TX_FILTER_PKTLOG_HYBRID;
	enum dp_mon_tx_filter_srng_type srng_type =
		DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon ||
	    !dp_mon_pdev->dp_pdev_tx_mon->tx_mon_filter)
		return;

	dp_mon_pdev->dp_pdev_tx_mon->tx_mon_filter[mode][srng_type] = tx_filter;

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Monitor: Hybrid mode filter reset\n");
}

void ath12k_dp_mon_pktlog_config_filter(struct ath12k_pdev_dp *dp_pdev,
					enum ath12k_pktlog_mode mode,
					u32 filter, bool enable)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = 0;

	if (enable) {
		if (dp_mon_pdev->rx_pktlog_mode == mode) {
			ath12k_err(dp->ab, "This mode is already configured\n");
			return;
		}

		if (filter & ATH12K_PKTLOG_CBF) {
			if (dp_pdev->ar->monitor_vdev_created) {
				dp_mon_pdev->rx_pktlog_cbf = true;
				ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
					   "CBF logging enabled on monitor VAP\n");
			} else {
				ret = ath12k_dp_mon_rx_setup_pktlog_cbf(dp_pdev);
				if (ret) {
					ath12k_err(dp->ab,
						   "Failed to setup CBF filter: %d\n",
						   ret);
					return;
				}
			}
		}

		if (filter & ATH12K_PKTLOG_HYBRID) {
			if (!dp_mon_pdev->dp_pdev_tx_mon)
				ath12k_warn(dp->ab, "TX mon pdev not initialized\n");
			else {
				ath12k_dp_mon_tx_setup_pktlog_hybrid(dp_pdev);
				if (!dp_mon_pdev->dp_pdev_tx_mon->tx_monitor_started) {
					ret = ath12k_dp_mon_tx_htt_src_ring_setup(dp);
					if (ret) {
						ath12k_warn(dp->ab, "Src ring fail:%d\n",
							    ret);
						ath12k_dp_mon_tx_reset_pktlog_hybrid(
										dp_pdev);
						dp_mon_pdev->dp_pdev_tx_mon
							->tx_pktlog_hybrid = false;
						return;
					}
				}
				dp_mon_pdev->dp_pdev_tx_mon->tx_pktlog_hybrid = true;
			}
		}

		switch (mode) {
		case ATH12K_PKTLOG_MODE_LITE:
			dp_mon_pdev->rx_pktlog_mode = ATH12K_PKTLOG_MODE_LITE;
			ret = ath12k_dp_tx_htt_h2t_ppdu_stats_req(dp_pdev->ar,
							HTT_PPDU_STATS_TAG_PKTLOG);
			if (ret)
				ath12k_err(dp->ab,
					   "failed to enable pktlog T2H: %d\n",
					   ret);

			ath12k_dp_mon_rx_setup_pktlog_lite(dp_pdev);
			break;
		case ATH12K_PKTLOG_MODE_FULL:
			if (!(filter & ATH12K_PKTLOG_RX))
				return;

			dp_mon_pdev->rx_pktlog_mode = ATH12K_PKTLOG_MODE_FULL;

			ath12k_dp_mon_rx_setup_pktlog_full(dp_pdev);
			break;

		default:
			ath12k_err(dp->ab, "Please set a valid mode\n");
			break;
		}
	} else {
		if (filter & ATH12K_PKTLOG_CBF) {
			ath12k_dp_mon_rx_reset_pktlog_cbf(dp_pdev);
			ath12k_dbg(dp->ab, ATH12K_DBG_DATA, "CBF logging disabled\n");
		}

		if (filter & ATH12K_PKTLOG_HYBRID) {
			ath12k_dp_mon_tx_reset_pktlog_hybrid(dp_pdev);

			if (dp_mon_pdev->dp_pdev_tx_mon) {
				dp_mon_pdev->dp_pdev_tx_mon->tx_pktlog_hybrid = false;

				if (!dp_mon_pdev->dp_pdev_tx_mon->tx_monitor_started)
					ath12k_dp_mon_tx_htt_src_ring_cleanup(dp);
			}
		}

		switch (mode) {
		case ATH12K_PKTLOG_MODE_LITE:
			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
						      DP_MON_FILTER_PKTLOG_LITE_MODE);
			break;
		case ATH12K_PKTLOG_MODE_FULL:
			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
						      DP_MON_FILTER_PKTLOG_FULL_MODE);
			break;
		case ATH12K_PKTLOG_DISABLED:
			if (dp_mon_pdev->rx_pktlog_mode == ATH12K_PKTLOG_DISABLED)
				return;

			ret = ath12k_dp_tx_htt_h2t_ppdu_stats_req(dp_pdev->ar,
							HTT_PPDU_STATS_TAG_DEFAULT);
			if (ret)
				ath12k_err(dp->ab,
					   "failed to reset htt ppdu stats: %d\n", ret);

			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
						      DP_MON_FILTER_PKTLOG_LITE_MODE);
			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
						      DP_MON_FILTER_PKTLOG_FULL_MODE);
			dp_mon_pdev->rx_pktlog_mode = ATH12K_PKTLOG_DISABLED;
			break;

		default:
			ath12k_err(dp->ab, "Please set a valid mode\n");
			break;
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_pktlog_config_filter);

void ath12k_dp_htt_rx_filter_rxmon_cfg(void *ptr,
				       struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd =
				(struct htt_rx_ring_selection_cfg_cmd *)ptr;

	cmd->info0 |= le32_encode_bits(!tlv_filter->rxmon_disable,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_EN_RXMON);
	cmd->info0 |=
		le32_encode_bits(!tlv_filter->rxmon_disable,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PKT_TYPE_EN_DATA);
	cmd->info0 |=
		le32_encode_bits(tlv_filter->is_monitor_mode,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_MON_MODE);
}
EXPORT_SYMBOL(ath12k_dp_htt_rx_filter_rxmon_cfg);
