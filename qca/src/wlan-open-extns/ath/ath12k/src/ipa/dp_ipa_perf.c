/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ipa.h"


/**
 * ath12k_dp_ipa_perf_set_perf_level() - Set IPA clock bandwidth based on data rates
 * @client: Client type
 * @max_supported_bw_mbps: Maximum bandwidth needed (in Mbps)
 * @hdl: IPA handle
 *
 * Return: int
 */
static int ath12k_dp_ipa_perf_set_perf_level(int client, u32 max_supported_bw_mbps,
					     ipa_wdi_hdl_t hdl)
{
	struct ipa_wdi_perf_profile profile;
	int result;

	profile.client = client;
	profile.max_supported_bw_mbps = max_supported_bw_mbps;

	printk("hdl:%d, client:%d bw:%d",hdl,client,max_supported_bw_mbps);
	result = ipa_wdi_set_perf_profile_per_inst(hdl, &profile);
	if (result) {
		printk("ipa_wdi_set_perf_profile fail, code %d", result);
		return -EFAULT;
	}
	printk("setting ipa perf is success");
	return 0;
}

/* BW voting support */
bool ath12k_dp_ipa_perf_set_perf_level_bw_enabled(struct ath12k_ipa *ipa_ctx)
{
	/*
	 * Do bandwidth-based IPA perf vote only when all below are met.
	 * a. IPA is enabled.
	 * b. IPA clk scaling is _not_ enabled.
	 * c. IPA force voting is enabled.
	 */
	return ATH12K_IPA_IS_CONFIG_ENABLED(ipa_ctx->config, ATH12K_IPA_ENABLE_MASK) &&
		!ATH12K_IPA_IS_CONFIG_ENABLED(ipa_ctx->config,
				ATH12K_IPA_CLK_SCALING_ENABLE_MASK) &&
		ipa_ctx->config->ipa_force_voting;
}

static inline bool
ath12k_dp_ipa_perf_is_clk_scaling_enabled(struct ath12k_ipa *ipa_ctx)
{
	return ATH12K_IPA_IS_CONFIG_ENABLED(ipa_ctx->config,
					  ATH12K_IPA_CLK_SCALING_ENABLE_MASK |
					  ATH12K_IPA_RM_ENABLE_MASK);
}

int ath12k_wlan_ipa_perf_set_perf_level(struct ath12k_ipa *ipa_ctx,
				      u64 tx_packets, u64 rx_packets)
{
	int ret;
	u32 next_bw;
	u64 total_packets = tx_packets + rx_packets;

	printk("ENABLING IPA PERF ");
	if ((!ATH12K_IPA_IS_CONFIG_ENABLED(ipa_ctx->config, ATH12K_IPA_ENABLE_MASK)) ||
		(!ath12k_dp_ipa_perf_is_clk_scaling_enabled(ipa_ctx)))
		return 0;

	printk("ENABLING IPA PERF level");
	if (total_packets > (ipa_ctx->config->bus_bw_high / 2))
		next_bw = ipa_ctx->config->ipa_bw_high;
	else if (total_packets > (ipa_ctx->config->bus_bw_medium / 2))
		next_bw = ipa_ctx->config->ipa_bw_medium;
	else
		next_bw = ipa_ctx->config->ipa_bw_low;

	if (ipa_ctx->curr_cons_bw != next_bw) {
		printk("Requesting IPA perf curr: %d, next: %d",
			  ipa_ctx->curr_cons_bw, next_bw);
		ret = ath12k_dp_ipa_perf_set_perf_level(IPA_CLIENT_WLAN1_CONS,
							next_bw, ipa_ctx->hdl);
		if (ret) {
			printk("RM CONS set perf profile failed: %d", ret);
			return -EFAULT;
		}
		ipa_ctx->stats.cons_perf_req++;
		ret = ath12k_dp_ipa_perf_set_perf_level(IPA_CLIENT_WLAN1_PROD,
							next_bw, ipa_ctx->hdl);
		if (ret) {
			printk("RM PROD set perf profile failed: %d", ret);
			return -EFAULT;
		}
		ipa_ctx->curr_cons_bw = next_bw;
		ipa_ctx->stats.prod_perf_req++;
	}

	return 0;
}


bool ath12k_dp_ipa_perf_get_radio_freq(enum nl80211_band band1,
				       enum WMI_HOST_WLAN_BAND band2)
{
	switch (band1) {
	case NL80211_BAND_2GHZ:
		if (band2 & WMI_HOST_WLAN_2GHZ_CAP)
			return true;
		break;
	case NL80211_BAND_5GHZ:
	case NL80211_BAND_6GHZ:
		if (band2 & WMI_HOST_WLAN_5GHZ_CAP)
			return true;
		break;
	default:
		return false;
	}

	return false;
}

static inline
int ath12k_dp_ipa_perf_update_perf_level(struct ath12k_ipa *ipa_ctx, int client)
{
	struct ath12k_base *ab;
	enum nl80211_band band;

	ab = (struct ath12k_base *)ipa_ctx->ab;
	if (!ab)
		return -EFAULT;

	/* TODO: Need to handle the case for split phy radio */
	band = NL80211_BAND_2GHZ;
	if (ath12k_dp_ipa_perf_get_radio_freq(band, ab->fw_pdev[0].supported_bands)) {
		return ath12k_dp_ipa_perf_set_perf_level(client,
							 ATH12K_IPA_MAX_BANDWIDTH_2G,
							 ipa_ctx->hdl);
	} else {
		return ath12k_dp_ipa_perf_set_perf_level(client, ATH12K_IPA_MAX_BANDWIDTH,
						    ipa_ctx->hdl);
	}
}

int ath12k_dp_ipa_perf_init_perf_level(struct ath12k_ipa *ipa_ctx)
{
	int ret;

	/* Set lowest bandwidth to start with */
	if (ath12k_dp_ipa_perf_is_clk_scaling_enabled(ipa_ctx))
		return ath12k_wlan_ipa_perf_set_perf_level(ipa_ctx, 0, 0);

	printk("IPA clk scaling disabled. Set perf level to maximum %d",
		  ATH12K_IPA_MAX_BANDWIDTH);

	ret = ath12k_dp_ipa_perf_update_perf_level(ipa_ctx, IPA_CLIENT_WLAN1_CONS);
	if (ret) {
		printk("CONS set perf profile failed: %d", ret);
		return -EFAULT;
	}

	ret = ath12k_dp_ipa_perf_update_perf_level(ipa_ctx, IPA_CLIENT_WLAN1_PROD);
	if (ret) {
		printk("PROD set perf profile failed: %d", ret);
		return -EFAULT;
	}

	return 0;
}
