// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../../dp_mon.h"
#include "dp_mon.h"
#include "../../dp_mon_filter.h"
#include "../ath12k_cmn_extn.h"

void
ath12k_wifi6_dp_mon_rx_setup_mon_mode_filter(struct ath12k_dp *dp,
					     struct htt_rx_ring_tlv_filter *tlv_filter,
					     u32 rx_mon_tlv_filter_flags)
{
	tlv_filter->offset_valid = false;
	tlv_filter->rx_filter = rx_mon_tlv_filter_flags;
	tlv_filter->drop_threshold_valid = true;
	tlv_filter->rx_drop_threshold = HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE;

	tlv_filter->enable_fp = 1;
	tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_data_filter = FILTER_DATA_ALL;
	tlv_filter->enable_mo = 1;
	tlv_filter->mo_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_data_filter = FILTER_DATA_ALL;
}

void ath12k_wifi6_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type;
	u32 rx_mon_tlv_filter_flags = 0;

	/* Configure mon buffer ring if rxdma1 is enabled */
	if (ath12k_dp_mon_rxdma1_enable(dp))
		srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF;
	else
		srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_BUF_RING;
	rx_filter.valid = true;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_wifi6_dp_mon_rx_setup_mon_mode_filter(dp,
						     rx_tlv_filter,
						     rx_mon_tlv_filter_flags);
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;

	/* Configure status ring */
	memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
	rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_STATUS_RING;
	rx_mon_tlv_filter_flags |= HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO;
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	rx_filter.valid = true;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_wifi6_dp_mon_rx_setup_mon_mode_filter(dp,
						     rx_tlv_filter,
						     rx_mon_tlv_filter_flags);
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

void ath12k_wifi6_dp_mon_rx_mon_mode_reset_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type;

	 /* reset mon buffer ring if rxdma1 is enabled */
	if (ath12k_dp_mon_rxdma1_enable(dp))
		srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF;
	else
		srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;

	/* reset status ring */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

int ath12k_wifi6_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	int ret = 0;
	enum dp_mon_filter_srng_type srng_type;

	/* update destination ring */
        if (ath12k_dp_mon_rxdma1_enable(dp))
		srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF;
	else
		srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_prepare_filter(dp, dp_pdev, srng_type, &rx_filter);
	if (rx_filter.valid)
		rx_tlv_filter->rxmon_disable = false;
	else
		rx_tlv_filter->rxmon_disable = true;
	ret = ath12k_dp_mon_rx_config_filters(dp, dp_pdev, srng_type,
						  rx_tlv_filter);
	if (ret)
		return ret;

	/* update status ring */
	memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_prepare_filter(dp, dp_pdev, srng_type, &rx_filter);
	if (rx_filter.valid)
		rx_tlv_filter->rxmon_disable = false;
	else
		rx_tlv_filter->rxmon_disable = true;

	ret = ath12k_dp_mon_rx_config_filters(dp, dp_pdev, srng_type,
						  rx_tlv_filter);
	if (ret)
		return ret;

	return 0;
}

void
ath12k_wifi6_dp_mon_rx_undecoded_metadata_config_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn = &dp_mon_pdev->pdev_mon_dp_extn;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_UNDECODED_METADATA_CAPTURE_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;

	/* Start with the existing monitor mode filter as base, stored under
	 * dedicated undecoded mode slot to avoid overwriting MONITOR_MODE.
	 */
	rx_filter = dp_mon_pdev->rx_filter[DP_MON_FILTER_MONITOR_MODE][srng_type];
	rx_tlv_filter = &rx_filter.rx_tlv_filter;

	/* Setup the filter to subscribe to FP PHY status TLV */
	rx_tlv_filter->fp_phy_err = 1;
	rx_tlv_filter->fp_phy_err_buf_src = ATH12K_WIFI6_FP_PHY_ERR_BUF_SRC_SW2RXDMA;
	rx_tlv_filter->fp_phy_err_buf_dest = ATH12K_WIFI6_FP_PHY_ERR_BUF_DEST_RXDMA2SW;
	rx_tlv_filter->phy_err_mask = mon_dp_extn->undecoded_meta->phyrx_error_mask;
	rx_tlv_filter->phy_err_mask_cont = mon_dp_extn->undecoded_meta->phyrx_error_mask_cont;
	rx_tlv_filter->phy_err_filter_valid = true;

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}
EXPORT_SYMBOL(ath12k_wifi6_dp_mon_rx_undecoded_metadata_config_filter);

void
ath12k_wifi6_dp_mon_rx_undecoded_metadata_reset_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn = &dp_mon_pdev->pdev_mon_dp_extn;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_UNDECODED_METADATA_CAPTURE_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;

	/* Read from the dedicated undecoded mode slot */
	rx_filter = dp_mon_pdev->rx_filter[mode][srng_type];
	rx_tlv_filter = &rx_filter.rx_tlv_filter;

	/* Reset the PHY error filter fields */
	rx_tlv_filter->fp_phy_err = 0;
	rx_tlv_filter->fp_phy_err_buf_src = ATH12K_WIFI6_FP_PHY_ERR_BUF_SRC_NONE;
	rx_tlv_filter->fp_phy_err_buf_dest = ATH12K_WIFI6_FP_PHY_ERR_BUF_DEST_RELEASING;
	rx_tlv_filter->phy_err_mask = 0;
	rx_tlv_filter->phy_err_mask_cont = 0;
	rx_tlv_filter->phy_err_filter_valid = true;

	/* Reset the error masks in the extension struct */
	if (mon_dp_extn->undecoded_meta) {
		mon_dp_extn->undecoded_meta->phyrx_error_mask = 0;
		mon_dp_extn->undecoded_meta->phyrx_error_mask_cont = 0;
	}

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}
EXPORT_SYMBOL(ath12k_wifi6_dp_mon_rx_undecoded_metadata_reset_filter);
