
/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/netlink.h>
#include <net/mac80211.h>
#include <net/genetlink.h>
#include <net/cfg80211.h>
#include <linux/byteorder/generic.h>
#include <asm/unaligned.h>
#include <linux/inet.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/timekeeping.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include "../core.h"
#include "../peer.h"
#include "ath12k_cmn_extn.h"
#include "vendor_extn.h"
#include "../net/wireless/core.h"
#include "../net/wireless/nl80211.h"
#include "../debug.h"
#include "../mac.h"
#include "../erp.h"
#include "esp_extn.h"
#include "ini.h"
#include "../cmn_defs.h"
#include "../telemetry_agent_if.h"
#include "../reg.h"
#include "dcs_extn.h"
#include "cbs_extn.h"
#include "rropinfo.h"
#include "dp_stats_extn.h"
#include "reg_extn.h"
#include "dp_mon_extn.h"
#include "../dp_mon.h"
#include "../net/mac80211/ieee80211_i.h"

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
#include "mesh_util.h"
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

const struct nla_policy
ath12k_vendor_tdma_schedule_policy[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_RADIO_INDEX] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_ID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BSSID] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_START_TIME_TSF_LOW] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_NUM_BUSY_SLOTS] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_DUR] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_INTVL] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AIFSN] = { .type = NLA_BINARY,
						      .len = sizeof(u16) * WLAN_MAX_AC },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMIN] = { .type = NLA_BINARY,
						      .len = sizeof(u16) * WLAN_MAX_AC },
	[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMAX] = { .type = NLA_BINARY,
						      .len = sizeof(u16) * WLAN_MAX_AC },
};
#ifndef nla_nest_end_checked
#define nla_nest_end_checked(skb, nla)		\
	do {					\
		if ((skb) && (nla))		\
			nla_nest_end((skb), (nla)); \
	} while (0)
#endif

static int ath12k_vendor_display_band_chans(struct ath12k *ar);

#define LOW_BAND_MIN_FIVEG_FREQ   5180
#define LOW_BAND_MAX_FIVEG_FREQ   5320
#define MAX_FREQ_IN_TWOG          2484
#define LOW_BAND_MAX_SIXG_FREQ    6515
#define HIGH_BAND_MIN_SIXG_FREQ   6535

enum ath12k_radio_band_info {
	NO_BAND_INFORMATION_AVAILABLE	= 0,
	HIGH_BAND_5G_RADIO		= 1,
	FULL_BAND_5G_RADIO		= 2,
	LOW_BAND_5G_RADIO		= 3,
	NON_5G_RADIO			= 4,
	FULL_BAND_6G_RADIO		= 5,
	BAND_5G_6G_RADIO		= 6,
	LOW_BAND_6G_RADIO		= 7,
	HIGH_BAND_6G_RADIO		= 8,
};

static bool ath12k_vendor_get_freq_range_for_band_info(struct ath12k *ar,
							u32 *low_freq,
							u32 *high_freq)
{
	if (!ar->freq_range.start_freq || !ar->freq_range.end_freq)
		return false;

	*low_freq = KHZ_TO_MHZ(ar->freq_range.start_freq);
	*low_freq += ATH12K_HALF_20MHZ_BW;
	*high_freq = KHZ_TO_MHZ(ar->freq_range.end_freq);
	*high_freq -= ATH12K_HALF_20MHZ_BW;

	return true;
}

static u32 ath12k_vendor_classify_radio_band(u32 low_freq, u32 high_freq)
{
	if (low_freq >= ATH12K_MIN_6GHZ_FREQ && high_freq <= LOW_BAND_MAX_SIXG_FREQ)
		return LOW_BAND_6G_RADIO;

	if (low_freq >= HIGH_BAND_MIN_SIXG_FREQ && high_freq <= ATH12K_MAX_6GHZ_FREQ)
		return HIGH_BAND_6G_RADIO;

	if (low_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    low_freq <= LOW_BAND_MAX_SIXG_FREQ &&
	    high_freq >= HIGH_BAND_MIN_SIXG_FREQ &&
	    high_freq <= ATH12K_MAX_6GHZ_FREQ)
		return FULL_BAND_6G_RADIO;

	if (low_freq >= ATH12K_MIN_5GHZ_FREQ &&
	    low_freq <= ATH12K_MAX_5GHZ_FREQ &&
	    high_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    high_freq <= ATH12K_MAX_6GHZ_FREQ)
		return BAND_5G_6G_RADIO;

	if (low_freq >= LOW_BAND_MIN_FIVEG_FREQ && high_freq <= LOW_BAND_MAX_FIVEG_FREQ)
		return LOW_BAND_5G_RADIO;

	if (low_freq > LOW_BAND_MAX_FIVEG_FREQ &&
	    high_freq <= ATH12K_MAX_5GHZ_FREQ)
		return HIGH_BAND_5G_RADIO;

	if (low_freq >= ATH12K_MIN_5GHZ_FREQ &&
	    high_freq <= ATH12K_MAX_5GHZ_FREQ)
		return FULL_BAND_5G_RADIO;

	return NO_BAND_INFORMATION_AVAILABLE;
}

static u32 ath12k_vendor_get_radio_band_info(struct ath12k *ar)
{
	u32 low_freq = 0, high_freq = 0;

	if (!ath12k_vendor_get_freq_range_for_band_info(ar, &low_freq, &high_freq))
		return NO_BAND_INFORMATION_AVAILABLE;

	if (high_freq <= MAX_FREQ_IN_TWOG)
		return NON_5G_RADIO;

	return ath12k_vendor_classify_radio_band(low_freq, high_freq);
}

#define ATH12K_CONFIG_AGGR_MAX_AMPDU_SIZE 255
#define ATH12K_CONFIG_AGGR_MAX_AMPDU_SIZE_BE 1023
#define ATH12K_CONFIG_AGGR_MAX_AMSDU_SIZE 7
#define ATH12K_CONFIG_MAX_BA_BUFSIZE 6
#define ATH12K_CONFIG_MAX_TX_ENCAP_TYPE 2
#define ATH12K_CONFIG_MAX_RX_DECAP_TYPE 2
#define ATH12K_CHAN144_FREQ 5720

#define ATH12K_REGDOM_UPDATE_TIMEOUT_MS	5000
#define ATH12K_REGDOM_UPDATE_POLL_MS	20
#define ATH12K_IFACE_RELOAD_LINKID_REGDOM	0xFE

static int ath12k_vendor_wait_for_regdom_update(struct ath12k_hw *ah)
{
	unsigned long timeout;

	if (!ah)
		return -EINVAL;

	timeout = jiffies + msecs_to_jiffies(ATH12K_REGDOM_UPDATE_TIMEOUT_MS);

	while (time_before(jiffies, timeout)) {
		if (READ_ONCE(ah->regd_updated))
			return 0;

		msleep(ATH12K_REGDOM_UPDATE_POLL_MS);
	}

	return -ETIMEDOUT;
}

static void ath12k_vendor_sync_alpha2_cache(struct ath12k_hw *ah)
{
	const struct ieee80211_regdomain *regd;
	struct ath12k *tmp_ar;
	u8 alpha2[REG_ALPHA2_LEN];
	int i;

	if (!ah || !ah->hw || !ah->hw->wiphy)
		return;

	rcu_read_lock();
	regd = rcu_dereference(ah->hw->wiphy->regd);
	if (!regd || !regd->alpha2[0] || !regd->alpha2[1]) {
		rcu_read_unlock();
		return;
	}

	alpha2[0] = regd->alpha2[0];
	alpha2[1] = regd->alpha2[1];
	rcu_read_unlock();

	for_each_ar(ah, tmp_ar, i)
		memcpy(tmp_ar->alpha2, alpha2, REG_ALPHA2_LEN);
}

static bool ath12k_vendor_regdom_changes(struct ieee80211_hw *hw,
						 const char *alpha2)
{
	const struct ieee80211_regdomain *regd;

	regd = rcu_dereference_rtnl(hw->wiphy->regd);
	if (!regd)
		return true;

	return memcmp(regd->alpha2, alpha2, REG_ALPHA2_LEN) != 0;
}

static int ath12k_reg_set_country_locked(struct wiphy *wiphy,
					 const char *alpha2)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_wmi_init_country_arg arg = {};
	struct wmi_set_current_country_arg current_arg = {};
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	int i;
	int ret = 0;

	lockdep_assert_wiphy(wiphy);

	if (!alpha2)
		return -EINVAL;

	if (!IS_ENABLED(CPTCFG_ATH_REG_DYNAMIC_USER_REG_HINTS)) {
		ath12k_dbg(ar ? ar->ab : NULL, ATH12K_DBG_REG,
			   "Country Setting is not allowed\n");
		return -EOPNOTSUPP;
	}

	if (!ath12k_vendor_regdom_changes(hw, alpha2)) {
		ath12k_dbg(ar ? ar->ab : NULL, ATH12K_DBG_REG,
			   "Country is already set\n");
		return 0;
	}

	ah->regd_updated = false;

	for_each_ar(ah, ar, i) {
		int cmd_ret;

		if (ar->ab->hw_params->current_cc_support) {
			memcpy(&current_arg.alpha2, alpha2, REG_ALPHA2_LEN);
			memcpy(&ar->alpha2, &current_arg.alpha2, REG_ALPHA2_LEN);
			cmd_ret = ath12k_wmi_send_set_current_country_cmd(ar,
								  &current_arg);
			if (cmd_ret)
				ath12k_warn(ar->ab,
					    "failed set current country code: %d\n",
					    cmd_ret);
		} else {
			arg.flags = ALPHA_IS_SET;
			memcpy(&arg.cc_info.alpha2, alpha2, REG_ALPHA2_LEN);
			arg.cc_info.alpha2[REG_ALPHA2_LEN] = 0;

			cmd_ret = ath12k_wmi_send_init_country_cmd(ar, &arg);
			if (cmd_ret)
				ath12k_warn(ar->ab,
					    "failed set INIT Country code: %d\n",
					    cmd_ret);
		}

		ath12k_mac_11d_scan_stop(ar);
		ar->regdom_set_by_user = true;

		if (cmd_ret && !ret)
			ret = cmd_ret;
	}

	return ret;
}

static int ath12k_vendor_set_country_alpha2(struct wiphy *wiphy,
						     const u8 *country)
{
	struct ieee80211_hw *hw;
	struct ath12k_hw *ah;
	char alpha2[REG_ALPHA2_LEN];
	int ret = -1;

	if (!country)
		return -EINVAL;

	alpha2[0] = country[0];
	alpha2[1] = country[1];
	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = hw->priv;

	if (!ath12k_vendor_regdom_changes(hw, alpha2)) {
		ath12k_vendor_sync_alpha2_cache(ah);
		return 0;
	}

	ret = ath12k_reg_set_country_locked(wiphy, alpha2);
	if (ret) {
		ath12k_err(NULL, "Failed to set country code %c%c: %d\n",
			   alpha2[0], alpha2[1], ret);
		return ret;
	}

	ret = ath12k_vendor_wait_for_regdom_update(ah);
	if (ret)
		ath12k_err(NULL,
			   "Timed out waiting for regdom update for country %c%c: %d\n",
			   alpha2[0], alpha2[1], ret);
	else
		ath12k_vendor_sync_alpha2_cache(ah);

	return ret;
}

static int ath12k_vendor_set_country_id(struct ath12k *ar, u32 country_id)
{
	struct ath12k_wmi_init_country_arg arg = {};
	struct ath12k_hw *ah;
	struct ath12k *tmp_ar;
	u16 country_code;
	int cmd_ret;
	int i;
	int ret = 0;

	if (!ar || !ar->ah)
		return -EINVAL;

	if (country_id > U16_MAX)
		return -EINVAL;

	country_code = (u16)country_id;
	if (ar->country_id == country_code) {
		ath12k_vendor_sync_alpha2_cache(ar->ah);
		return 0;
}

	ah = ar->ah;
	ah->regd_updated = false;

	arg.flags = CC_IS_SET;
	arg.cc_info.country_code = country_code;

	for_each_ar(ah, tmp_ar, i) {
		cmd_ret = ath12k_wmi_send_init_country_cmd(tmp_ar, &arg);
		if (cmd_ret) {
			ath12k_warn(tmp_ar->ab,
				    "failed set INIT country id %u: %d\n",
				    country_id, cmd_ret);
			if (!ret)
				ret = cmd_ret;
			continue;
		}

		ath12k_mac_11d_scan_stop(tmp_ar);
		tmp_ar->regdom_set_by_user = true;
	}

	if (ret)
		return ret;

	ret = ath12k_vendor_wait_for_regdom_update(ah);
	if (ret)
		ath12k_err(NULL,
			   "Timed out waiting for regdom update for country id %u: %d\n",
			   country_id, ret);
	else
		ath12k_vendor_sync_alpha2_cache(ah);

	if (!ret)
		for_each_ar(ah, tmp_ar, i)
			tmp_ar->country_id = country_code;

	return ret;
}

static int ath12k_vendor_set_regdomain(struct wiphy *wiphy,
				       struct ath12k *ar,
				       u32 regdomain)
{
	struct ath12k_wmi_init_country_arg arg = {0};
	struct ath12k_hw *ah;
	struct ath12k *tmp_ar;
	u32 input_regdomain;
	u32 *old_regdomain;
	bool *old_regdom_set_by_user;
	bool *cache_valid;
	u16 reg_2g_5g_pair_id;
	u16 sixg_superdmn_id;
	int i;
	int ret = 0;
	bool radio_seen = false;

	if (!ar || !ar->ah)
		return -EINVAL;

	ah = ar->ah;
	if (!ah->num_radio)
		return -ENODATA;

	old_regdomain = kcalloc(ah->num_radio, sizeof(*old_regdomain), GFP_KERNEL);
	if (!old_regdomain)
		return -ENOMEM;

	old_regdom_set_by_user = kcalloc(ah->num_radio,
					 sizeof(*old_regdom_set_by_user),
					 GFP_KERNEL);
	if (!old_regdom_set_by_user) {
		ret = -ENOMEM;
		goto out;
	}

	cache_valid = kcalloc(ah->num_radio, sizeof(*cache_valid), GFP_KERNEL);
	if (!cache_valid) {
		ret = -ENOMEM;
		goto out;
	}

	input_regdomain = regdomain;
	reg_2g_5g_pair_id = input_regdomain & 0xFFFF;
	sixg_superdmn_id = (input_regdomain >> 16) & 0xFFFF;

	regdomain = ((u32)sixg_superdmn_id << 16) | reg_2g_5g_pair_id;

	arg.flags = REGDMN_IS_SET;
	arg.cc_info.regdom_id = regdomain;

	ah->regd_updated = false;
	for_each_ar(ah, tmp_ar, i) {
		if (i >= ah->num_radio)
			continue;

		cache_valid[i] = true;
		old_regdomain[i] = tmp_ar->radio_cfg.regdomain;
		old_regdom_set_by_user[i] = tmp_ar->regdom_set_by_user;

		ret = ath12k_wmi_send_init_country_cmd(tmp_ar, &arg);
		if (ret) {
			ath12k_err(NULL,
				   "Failed to send WMI_SET_INIT_COUNTRY_CMDID for regdomain on radio %d\n",
				   i);
			goto rollback;
		}

		radio_seen = true;
		ath12k_mac_11d_scan_stop(tmp_ar);
		tmp_ar->regdom_set_by_user = true;
		tmp_ar->radio_cfg.regdomain = regdomain;
	}

	if (!radio_seen) {
		ret = -ENODATA;
		goto out;
	}

	ret = ath12k_vendor_wait_for_regdom_update(ah);
	if (ret) {
		ath12k_err(NULL,
			   "Timed out waiting for regdom update for regdomain %u: %d\n",
			   input_regdomain, ret);
		goto rollback;
	}

	goto out;

rollback:
	for_each_ar(ah, tmp_ar, i) {
		if (i >= ah->num_radio || !cache_valid[i])
			continue;

		tmp_ar->radio_cfg.regdomain = old_regdomain[i];
		tmp_ar->regdom_set_by_user = old_regdom_set_by_user[i];
	}

out:
	kfree(cache_valid);
	kfree(old_regdom_set_by_user);
	kfree(old_regdomain);

	return ret;
}

const struct nla_policy
ath12k_240mhz_sta_info_policy[QCA_WLAN_VENDOR_ATTR_240MHZ_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS] = {
		.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO] = {
		.type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP] = {
		.type = NLA_BINARY, .len = 3 },
};

/* Maximum byte length of a TCLAS10 filter mask / value blob */
#define ATH12K_SCS_RULE_TCLAS10_FILTER_LEN_MAX	16
/* Byte length of an IPv6 flow label (3 octets per IEEE 802.11) */
#define ATH12K_SCS_RULE_TCLAS4_FLOW_LABEL_LEN	3

/**
 * struct ath12k_scs_rule_config_params - Parsed SCS rule configuration
 *
 * Carries all attributes from a %QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG
 * vendor command, as parsed by ath12k_vendor_rule_config_notify(), for
 * forwarding to userspace via ath12k_vendor_send_rule_config_notify().
 *
 * @mac_addr: Destination MAC address (mandatory).
 * @rule_id: SCS rule identifier.
 * @request_type: Add/remove/change request type.
 * @output_tid: Output TID for the classified flow.
 * @classifier_type: TCLAS classifier type (4 or 10).
 * @tclas4_version: IP version for TCLAS type 4.
 * @tclas4_src_ipv4: Source IPv4 address (4 bytes).
 * @tclas4_dst_ipv4: Destination IPv4 address (4 bytes).
 * @tclas4_src_ipv6: Source IPv6 address (16 bytes).
 * @tclas4_dst_ipv6: Destination IPv6 address (16 bytes).
 * @tclas4_src_port: Source UDP/TCP port.
 * @tclas4_dst_port: Destination UDP/TCP port.
 * @tclas4_dscp: DSCP value.
 * @tclas4_next_header: Next-header / protocol field.
 * @tclas4_flow_label: IPv6 flow label (3 bytes).
 * @tclas10_protocol_instance: TCLAS10 protocol instance.
 * @tclas10_next_header: TCLAS10 next-header field.
 * @tclas10_filter_mask: TCLAS10 filter mask blob.
 * @tclas10_filter_value: TCLAS10 filter value blob.
 * @tclas10_filter_len: Byte length of the TCLAS10 filter blobs.
 * @service_class_id: Service class identifier.
 * @netdev_if_index: Network device interface index.
 * @present: Bitmask; bit N is set when attribute N was present in the command.
 */
struct ath12k_scs_rule_config_params {
	u8  mac_addr[ETH_ALEN];
	u8  rule_id;
	u8  request_type;
	u8  output_tid;
	u8  classifier_type;
	u8  tclas4_version;
	u8  tclas4_src_ipv4[4];
	u8  tclas4_dst_ipv4[4];
	u8  tclas4_src_ipv6[16];
	u8  tclas4_dst_ipv6[16];
	u16 tclas4_src_port;
	u16 tclas4_dst_port;
	u8  tclas4_dscp;
	u8  tclas4_next_header;
	u8  tclas4_flow_label[ATH12K_SCS_RULE_TCLAS4_FLOW_LABEL_LEN];
	u8  tclas10_protocol_instance;
	u8  tclas10_next_header;
	u8  tclas10_filter_mask[ATH12K_SCS_RULE_TCLAS10_FILTER_LEN_MAX];
	u8  tclas10_filter_value[ATH12K_SCS_RULE_TCLAS10_FILTER_LEN_MAX];
	u8  tclas10_filter_len;
	u8  service_class_id;
	u32 netdev_if_index;
	u32 present;
};

const struct nla_policy
ath12k_rule_config_policy[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(4),
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(4),
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(16),
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(16),
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT] =
		{ .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT] =
		{ .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL] =
		NLA_POLICY_EXACT_LEN_WARN(ATH12K_SCS_RULE_TCLAS4_FLOW_LABEL_LEN),
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK] =
		{ .type = NLA_BINARY,
		  .len  = ATH12K_SCS_RULE_TCLAS10_FILTER_LEN_MAX },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE] =
		{ .type = NLA_BINARY,
		  .len  = ATH12K_SCS_RULE_TCLAS10_FILTER_LEN_MAX },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID] =
		{ .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX] =
		{ .type = NLA_U32 },
};

const struct nla_policy
ath12k_vendor_home_offchan_tx_rx_policy[
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_MAX + 1] = {
	/* Command attributes */
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN] = {.type = NLA_U16},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN_BAND] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SCAN_DUR] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_TRANSACTION_ID] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_BW_MODE] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SEC_CHAN_OFFSET] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_IS_MLD] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_LINK_ID] = {.type = NLA_U8},

	/* Event/Statistics attributes */
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_STATUS] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_NOISE_FLOOR] = {.type = NLA_S16},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_VALID] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_FRAME_COUNT] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_FRAME_COUNT] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_CLEAR_COUNT] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CYCLE_COUNT] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_DWELL_TIME] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_HTOF] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_FTOH] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_COUNT] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_DURATION] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_PKT_STATUS] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_IFINDEX] = {.type = NLA_U32},
};

const struct nla_policy
ath12k_vendor_home_offchan_tx_rx_frame_policy[
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX + 1] = {
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_RETRY] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_POWER] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_TX_BEAMFORMING] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA] = {.type = NLA_BINARY},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_ID] = {.type = NLA_U8},
};

const struct nla_policy
ath12k_vendor_home_offchan_tx_rx_pkt_status_policy[
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_MAX + 1] = {
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ID] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_RESULT] = {.type = NLA_U8},
};

const struct nla_policy
ath12k_wifi_mac_config_policy[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_INDEX] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_BSS_ID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAC_ADDR] = { .len = ETH_ALEN },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_IFTYPE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_FLAGS] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_GRP_ID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_GRP_SIZE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_ENABLED] = { .type = NLA_FLAG },
};

const struct nla_policy
ath12k_ctl_table_policy[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_BAND] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CTL_RADIO_INDEX] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_LENGTH] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_DATA] = { .type = NLA_BINARY,
						.len = QCA_WLAN_MAX_CTL_SIZE },
};

const struct nla_policy
ath12k_rtt_blob_policy[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_MAX + 1] = {
       [QCA_WLAN_VENDOR_ATTR_RTT_BLOB_BAND]      = { .type = NLA_U32 },
       [QCA_WLAN_VENDOR_ATTR_RTT_BLOB_RADIO_IDX] = { .type = NLA_U32 },
       [QCA_WLAN_VENDOR_ATTR_RTT_BLOB_LENGTH]    = { .type = NLA_U32 },
       [QCA_WLAN_VENDOR_ATTR_RTT_BLOB_DATA]      = { .type = NLA_BINARY,
                                                    .len = QCA_WLAN_MAX_RTT_BLOB_SIZE },
};

const struct nla_policy
ath12k_vendor_dcs_config_policy[QCA_WLAN_VENDOR_ATTR_DCS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_DCS_ENABLE] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_BITMAP] = {.type = NLA_U32},
};

const struct nla_policy
ath12k_rrop_info_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX] = {.type = NLA_U8 },
};

const struct nla_policy
ath12k_vendor_dcs_sim_policy[QCA_WLAN_VENDOR_ATTR_DCS_SIM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_DCS_SIM_TYPE] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_DCS_SIM_INTERFERENCE_BITMAP] = {.type = NLA_U32},
};

const struct nla_policy
ath12k_reg_params_policy[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CMD] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS_CHAN] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_LINKID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_DISABLE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CHAN_LIST] = { .type = NLA_NESTED },
};

enum rx_flow_tag_op {
	RX_FLOW_TAG_ADD,
	RX_FLOW_TAG_DEL,
	RX_FLOW_TAG_DUMP,
};

/* struct rx_flow_blob - Binary layout of the RX flow tag operation blob */
struct rx_flow_blob {
	u8     op_code;        /* [0]      operation: 0=add, 1=del, 2=dump */
	u8     ip_ver;         /* [1]      IP version: 4 or 6              */
	u8     proto;          /* [2]      L4 protocol number               */
	u8     _pad;           /* [3]      reserved / alignment             */
	u8     src_ip_str[40]; /* [4..43]  null-terminated source IP string */
	u8     dst_ip_str[40]; /* [44..83] null-terminated dest IP string   */
	__le16 sport;          /* [84..85] source port (little-endian)      */
	__le16 dport;          /* [86..87] destination port (little-endian) */
	__le16 tag;            /* [88..89] FSE metadata tag (little-endian) */
	u8     drop_flow;      /* [90]     1 = drop matching flows          */
	u8     ring_id;        /* [91]     target REO destination ring      */
} __packed;

#define ATH12K_RX_FLOW_TAG_DUMP_BUF_SIZE (PAGE_SIZE * 4)
#define ATH12K_HWBL_NL_CHAN_WIDTH_INVALID ((u32)-1)

static struct ath12k_hw_blocklist_state *
ath12k_vendor_hwbl_get_state_extn(struct ath12k_base *ab, int pdev_idx)
{
	if (!ab || pdev_idx < 0 || pdev_idx >= ab->num_radios)
		return NULL;

	return ab->ath12k_base_extn.hw_blocklist[pdev_idx];
}

static u8 ath12k_vendor_hwbl_pwr_mode_to_nl_extn(u8 pwr_mode)
{
	switch (pwr_mode) {
	case WMI_REG_INDOOR_AP:
		return NL80211_REG_AP_LPI;
	case WMI_REG_STD_POWER_AP:
		return NL80211_REG_AP_SP;
	case WMI_REG_VLP_AP:
		return NL80211_REG_AP_VLP;
	default:
		return NL80211_REG_NUM_POWER_MODES;
	}
}

static u32 ath12k_vendor_hwbl_bw_to_nl_extn(u8 max_bw)
{
	switch (max_bw) {
	case WMI_HOST_CHAN_WIDTH_20:
		return NL80211_CHAN_WIDTH_20;
	case WMI_HOST_CHAN_WIDTH_40:
		return NL80211_CHAN_WIDTH_40;
	case WMI_HOST_CHAN_WIDTH_80:
		return NL80211_CHAN_WIDTH_80;
	case WMI_HOST_CHAN_WIDTH_160:
		return NL80211_CHAN_WIDTH_160;
	case WMI_HOST_CHAN_WIDTH_320:
		return NL80211_CHAN_WIDTH_320;
	default:
		return ATH12K_HWBL_NL_CHAN_WIDTH_INVALID;
	}
}

static const struct nla_policy
ath12k_hw_blocked_chans_query_policy_extn[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX] = { .type = NLA_S32 },
};

static int ath12k_hw_blocked_chans_event_update_or_get_len_extn(
	struct sk_buff *vendor_event,
	const struct ath12k_hw_blocklist_state *st,
	u8 radio_id)
{
	struct nlattr *band_list = NULL;
	struct nlattr *band_info = NULL;
	struct nlattr *pwr_mode_list = NULL;
	struct nlattr *pwr_mode_info = NULL;
	struct nlattr *fb_chan_list = NULL;
	struct nlattr *pc_chan_list = NULL;
	struct nlattr *chan_info = NULL;
	int len = 0;
	enum wmi_reg_6g_ap_type pwr_mode;
	int i;

	if (vendor_event &&
	    nla_put_s32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_RADIO_INDEX,
			radio_id)) {
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event) {
		band_list = nla_nest_start(vendor_event,
					   QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_BAND_LIST);
		if (!band_list)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	if (vendor_event) {
		band_info = nla_nest_start(vendor_event, NL80211_BAND_6GHZ);
		if (!band_info)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_ID,
			NL80211_BAND_6GHZ))
		goto fail;
	else
		len += nla_total_size(sizeof(u32));

	if (vendor_event) {
		pwr_mode_list = nla_nest_start(vendor_event,
					       QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PWR_MODE_LIST);
		if (!pwr_mode_list)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	for (pwr_mode = 0; pwr_mode < WMI_REG_CURRENT_MAX_AP_TYPE; pwr_mode++) {
		const struct ath12k_hw_blocklist_chans *chans = &st->pwr[pwr_mode];
		u8 nl_pwr_mode;

		nl_pwr_mode = ath12k_vendor_hwbl_pwr_mode_to_nl_extn(pwr_mode);
		if (nl_pwr_mode >= NL80211_REG_NUM_POWER_MODES)
			continue;

		if (vendor_event) {
			pwr_mode_info = nla_nest_start(vendor_event, pwr_mode);
			if (!pwr_mode_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_ID,
			       nl_pwr_mode))
			goto fail;
		else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			fb_chan_list = nla_nest_start(vendor_event,
						      QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_FB_CHAN_LIST);
			if (!fb_chan_list)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		for (i = 0; i < chans->nfbchans; i++) {
			const struct ath12k_hw_blocklist_fb_chan *fb_chan = &chans->fb_chan[i];
			u32 nl_bw = ath12k_vendor_hwbl_bw_to_nl_extn(fb_chan->max_bw);

			if (nl_bw == ATH12K_HWBL_NL_CHAN_WIDTH_INVALID)
				continue;

			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, i);
				if (!chan_info)
					goto fail;
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    (nla_put_u32(vendor_event,
					 QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_BW,
					 nl_bw) ||
			     nla_put_u16(vendor_event,
					 QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_CENTER_FREQ,
					 fb_chan->center_freq) ||
			     nla_put_u16(vendor_event,
					 QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_PRI20_BITMAP,
					 fb_chan->pri20_bitmap)))
				goto fail;
			else {
				len += nla_total_size(sizeof(u32));
				len += nla_total_size(sizeof(u16)) * 2;
			}

			nla_nest_end_checked(vendor_event, chan_info);
			chan_info = NULL;
		}
		nla_nest_end_checked(vendor_event, fb_chan_list);
		fb_chan_list = NULL;

		if (vendor_event) {
			pc_chan_list = nla_nest_start(vendor_event,
						      QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_PC_CHAN_LIST);
			if (!pc_chan_list)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		for (i = 0; i < chans->npcchans; i++) {
			const struct ath12k_hw_blocklist_pc_chan *pc_chan = &chans->pc_chan[i];
			u32 nl_bw = ath12k_vendor_hwbl_bw_to_nl_extn(pc_chan->max_bw);

			if (nl_bw == ATH12K_HWBL_NL_CHAN_WIDTH_INVALID)
				continue;

			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, i);
				if (!chan_info)
					goto fail;
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    (nla_put_u32(vendor_event,
					 QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW,
					 nl_bw) ||
			     nla_put_u16(vendor_event,
					 QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_CENTER_FREQ,
					 pc_chan->center_freq) ||
			     nla_put_u32(vendor_event,
					 QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_PUNC_PATTERN_BITMAP,
					 /* Bitmap of puncture patterns (BIT(idx)). */
					 pc_chan->punc_pat_bmap)))
				goto fail;
			else {
				len += nla_total_size(sizeof(u32));
				len += nla_total_size(sizeof(u16));
				len += nla_total_size(sizeof(u32));
			}

			nla_nest_end_checked(vendor_event, chan_info);
			chan_info = NULL;
		}
		nla_nest_end_checked(vendor_event, pc_chan_list);
		pc_chan_list = NULL;

		nla_nest_end_checked(vendor_event, pwr_mode_info);
		pwr_mode_info = NULL;
	}

	nla_nest_end_checked(vendor_event, pwr_mode_list);
	nla_nest_end_checked(vendor_event, band_info);
	nla_nest_end_checked(vendor_event, band_list);
	return vendor_event ? 0 : len;

fail:
	nla_nest_end_checked(vendor_event, chan_info);
	nla_nest_end_checked(vendor_event, fb_chan_list);
	nla_nest_end_checked(vendor_event, pc_chan_list);
	nla_nest_end_checked(vendor_event, pwr_mode_info);
	nla_nest_end_checked(vendor_event, pwr_mode_list);
	nla_nest_end_checked(vendor_event, band_info);
	nla_nest_end_checked(vendor_event, band_list);
	return -EINVAL;
}

static int ath12k_vendor_validate_hw_blocked_chans_state_extn(struct ath12k *ar, u8 radio_id)
{
	struct ath12k_hw_blocklist_state *st;
	int ret = 0;

	if (ar->pdev_idx >= ar->ab->num_radios)
		return -EINVAL;

	spin_lock_bh(&ar->ab->base_lock);
	st = ath12k_vendor_hwbl_get_state_extn(ar->ab, ar->pdev_idx);
	if (!st || !st->valid)
		goto unlock;

	if (!st->complete) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "HW blocked channels query rejected: fragmented update in progress for radio %u",
			   radio_id);
		ret = -EAGAIN;
	}

unlock:
	spin_unlock_bh(&ar->ab->base_lock);
	return ret;
}

static int ath12k_vendor_send_hw_blocked_chans_reply_extn(struct wiphy *wiphy,
							  struct ath12k *ar,
							  u8 radio_id,
							  bool send_event)
{
	struct ath12k_hw_blocklist_state snapshot;
	struct sk_buff *reply_skb;
	struct nlattr *radio_list = NULL;
	struct nlattr *radio_info = NULL;
	int reply_len, ret;

	ret = ath12k_vendor_validate_hw_blocked_chans_state_extn(ar, radio_id);
	if (ret)
		return ret;

	/* Take a deep copy of the blocklist state. */
	ret = ath12k_reg_hw_blocklist_snapshot_extn(ar->ab, ar->pdev_idx, &snapshot);
	if (ret)
		return ret;

	reply_len = ath12k_hw_blocked_chans_event_update_or_get_len_extn(NULL, &snapshot,
									 radio_id);
	if (reply_len <= 0) {
		ret = -EINVAL;
		goto free_snapshot;
	}

	reply_len += nla_total_size(0); /* radio_list nest header */
	reply_len += nla_total_size(0); /* radio_info nest header */

	if (send_event)
		reply_skb = cfg80211_vendor_event_alloc(wiphy, NULL,
							NLMSG_HDRLEN + reply_len,
							QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS_EVENT_INDEX,
							GFP_ATOMIC);
	else
		reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_HDRLEN + reply_len);
	if (!reply_skb) {
		ret = -ENOMEM;
		goto free_snapshot;
	}

	radio_list = nla_nest_start(reply_skb,
				    QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_RADIO_LIST);
	if (!radio_list) {
		ret = -EINVAL;
		goto fail;
	}

	radio_info = nla_nest_start(reply_skb, 0);
	if (!radio_info) {
		ret = -EINVAL;
		goto fail;
	}

	ret = ath12k_hw_blocked_chans_event_update_or_get_len_extn(reply_skb, &snapshot,
								   radio_id);
	if (ret)
		goto fail;

	nla_nest_end_checked(reply_skb, radio_info);
	radio_info = NULL;
	nla_nest_end_checked(reply_skb, radio_list);
	radio_list = NULL;

	ath12k_reg_hw_blocklist_free_snapshot_extn(&snapshot);
	if (send_event) {
		cfg80211_vendor_event(reply_skb, GFP_ATOMIC);
		return 0;
	}
	return cfg80211_vendor_cmd_reply(reply_skb);

fail:
	nla_nest_end_checked(reply_skb, radio_info);
	nla_nest_end_checked(reply_skb, radio_list);
	kfree_skb(reply_skb);
free_snapshot:
	ath12k_reg_hw_blocklist_free_snapshot_extn(&snapshot);
	return ret;
}

static int ath12k_vendor_send_hw_blocked_chans_reply_all_extn(struct wiphy *wiphy)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_blocklist_state *hw_bl_snapshots;
	bool *hw_bl_snapshot_valid;
	struct sk_buff *reply_skb = NULL;
	struct nlattr *radio_list = NULL;
	struct nlattr *radio_info = NULL;
	struct ath12k *ar;
	int reply_len = 0;
	int radio_reply_len;
	u8 radio_id;
	bool has_6ghz_radio = false;
	int ret;

	hw_bl_snapshots = kcalloc(ah->num_radio, sizeof(*hw_bl_snapshots), GFP_KERNEL);
	if (!hw_bl_snapshots)
		return -ENOMEM;

	hw_bl_snapshot_valid = kcalloc(ah->num_radio, sizeof(*hw_bl_snapshot_valid), GFP_KERNEL);
	if (!hw_bl_snapshot_valid) {
		kfree(hw_bl_snapshots);
		return -ENOMEM;
	}

	/* Take snapshots upfront and reuse them in the fill pass below.
	 * A new WMI event between the two passes could grow the data beyond
	 * the already-allocated SKB, causing nla_put to silently fail.
	 */
	for (radio_id = 0; radio_id < ah->num_radio; radio_id++) {
		ar = &ah->radio[radio_id];
		if (!ar->supports_6ghz)
			continue;

		has_6ghz_radio = true;
		ret = ath12k_vendor_validate_hw_blocked_chans_state_extn(ar, radio_id);
		if (ret)
			goto free_snaps;

		ret = ath12k_reg_hw_blocklist_snapshot_extn(ar->ab, ar->pdev_idx,
							    &hw_bl_snapshots[radio_id]);
		if (ret)
			memset(&hw_bl_snapshots[radio_id], 0, sizeof(hw_bl_snapshots[radio_id]));
		hw_bl_snapshot_valid[radio_id] = true;

		radio_reply_len =
		    ath12k_hw_blocked_chans_event_update_or_get_len_extn(NULL,
									 &hw_bl_snapshots[radio_id],
									 radio_id);
		if (radio_reply_len <= 0) {
			ret = -EINVAL;
			goto free_snaps;
		}

		reply_len += nla_total_size(0); /* radio_info nest header */
		reply_len += radio_reply_len;
	}

	if (!has_6ghz_radio) {
		ret = -EOPNOTSUPP;
		goto free_snaps;
	}

	reply_len += nla_total_size(0); /* radio_list nest header */
	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_HDRLEN + reply_len);
	if (!reply_skb) {
		ret = -ENOMEM;
		goto free_snaps;
	}

	radio_list = nla_nest_start(reply_skb,
				    QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_RADIO_LIST);
	if (!radio_list) {
		ret = -EINVAL;
		goto fail;
	}

	/* Fill using the snapshots from above — no re-snapshot needed. */
	for (radio_id = 0; radio_id < ah->num_radio; radio_id++) {
		ar = &ah->radio[radio_id];
		if (!ar->supports_6ghz)
			continue;

		radio_info = nla_nest_start(reply_skb, radio_id);
		if (!radio_info) {
			ret = -EINVAL;
			goto fail;
		}

		ret = ath12k_hw_blocked_chans_event_update_or_get_len_extn(reply_skb,
									   &hw_bl_snapshots[radio_id],
									   radio_id);
		if (ret)
			goto fail;

		nla_nest_end_checked(reply_skb, radio_info);
		radio_info = NULL;
	}

	nla_nest_end_checked(reply_skb, radio_list);
	radio_list = NULL;

	for (radio_id = 0; radio_id < ah->num_radio; radio_id++)
		if (hw_bl_snapshot_valid[radio_id])
			ath12k_reg_hw_blocklist_free_snapshot_extn(&hw_bl_snapshots[radio_id]);

	kfree(hw_bl_snapshot_valid);
	kfree(hw_bl_snapshots);
	return cfg80211_vendor_cmd_reply(reply_skb);

fail:
	nla_nest_end_checked(reply_skb, radio_info);
	nla_nest_end_checked(reply_skb, radio_list);
	kfree_skb(reply_skb);
free_snaps:
	for (radio_id = 0; radio_id < ah->num_radio; radio_id++)
		if (hw_bl_snapshot_valid[radio_id])
			ath12k_reg_hw_blocklist_free_snapshot_extn(&hw_bl_snapshots[radio_id]);
	kfree(hw_bl_snapshot_valid);
	kfree(hw_bl_snapshots);
	return ret;
}

int ath12k_vendor_send_hw_blocklist_update_event_extn(struct ath12k_base *ab,
						      u8 phy_id)
{
	struct ath12k *ar;
	int hw_idx;

	if (phy_id >= ab->num_radios)
		return -EINVAL;

	ar = ab->pdevs[phy_id].ar;
	if (!ar || !ar->ah || !ar->ah->hw)
		return -EINVAL;

	if (ar->ah->hw->wiphy->n_radio <= 1) {
		hw_idx = 0;
	} else {
		hw_idx = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
						     ar->freq_range.start_freq);
		if (hw_idx < 0)
			return -ENODEV;
	}

	return ath12k_vendor_send_hw_blocked_chans_reply_extn(ar->ah->hw->wiphy, ar,
							      (u8)hw_idx, true);
}

int ath12k_vendor_get_hw_blocked_chans_extn(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_MAX + 1] = {};
	struct ath12k *ar;
	int radio_id = -1;

	(void)wdev;

	if (data_len) {
		if (!data)
			return -EINVAL;

		if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_MAX, data, data_len,
			      ath12k_hw_blocked_chans_query_policy_extn, NULL))
			return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX])
		return ath12k_vendor_send_hw_blocked_chans_reply_all_extn(wiphy);

	radio_id = nla_get_s32(tb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX]);
	if (radio_id < -1 || radio_id > U8_MAX)
		return -EINVAL;
	if (radio_id < 0)
		return ath12k_vendor_send_hw_blocked_chans_reply_all_extn(wiphy);

	ar = ath12k_get_radio_by_id(wiphy, (u8)radio_id);
	if (!ar)
		return -ENODATA;

	if (!ar->supports_6ghz)
		return -EOPNOTSUPP;

	return ath12k_vendor_send_hw_blocked_chans_reply_extn(wiphy, ar, (u8)radio_id, false);
}

static int ath12k_vendor_resolve_nol_path_from_data_extn(const void *data,
							 u32 data_len,
							 u8 radio_idx,
							 char **path);
static int ath12k_vendor_reply_u32_extn(struct wiphy *wiphy, u32 value);

/**
 * ath12k_vendor_send_rule_config_notify() - Send SCS rule config vendor event
 * @vif: Virtual interface associated with the BSS.
 * @params: Fully populated SCS rule configuration parameters.
 *
 * Allocates a vendor event SKB and populates it with every attribute that
 * was marked present in @params->present, then dispatches the event to
 * userspace via cfg80211_vendor_event().
 *
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_vendor_send_rule_config_notify(struct ieee80211_vif *vif,
					  const struct ath12k_scs_rule_config_params *params)
{
	struct wireless_dev *wdev;
	struct sk_buff *skb;
	u32 attr_mask;

	wdev = ieee80211_vif_to_wdev(vif);
	if (!wdev)
		return -EINVAL;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG_INDEX,
					  GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	attr_mask = params->present;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR,
		    ETH_ALEN, params->mac_addr))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID,
		       params->rule_id))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE,
		       params->request_type))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID,
		       params->output_tid))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE,
		       params->classifier_type))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION,
		       params->tclas4_version))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR,
		    sizeof(params->tclas4_src_ipv4), params->tclas4_src_ipv4))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR,
		    sizeof(params->tclas4_dst_ipv4), params->tclas4_dst_ipv4))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR,
		    sizeof(params->tclas4_src_ipv6), params->tclas4_src_ipv6))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR,
		    sizeof(params->tclas4_dst_ipv6), params->tclas4_dst_ipv6))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT)) &&
	    nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT,
			params->tclas4_src_port))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT)) &&
	    nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT,
			params->tclas4_dst_port))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP,
		       params->tclas4_dscp))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER,
		       params->tclas4_next_header))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL,
		    sizeof(params->tclas4_flow_label), params->tclas4_flow_label))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE,
		       params->tclas10_protocol_instance))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER,
		       params->tclas10_next_header))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK,
		    params->tclas10_filter_len, params->tclas10_filter_mask))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE)) &&
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE,
		    params->tclas10_filter_len, params->tclas10_filter_value))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID)) &&
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID,
		       params->service_class_id))
		goto nla_fail;

	if ((attr_mask & BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX)) &&
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX,
			params->netdev_if_index))
		goto nla_fail;

	ath12k_dbg(NULL, ATH12K_DBG_QOS,
		   "scs rule config notify: mac=%pM attr_mask=0x%x rule_id=%u req_type=%u tid=%u cls_type=%u svc_cls=%u ifidx=%u tclas4[ver=%u sport=%u dport=%u dscp=%u nhdr=%u src=%pI4 dst=%pI4 src6=%pI6 dst6=%pI6 flow=%3ph] tclas10[inst=%u nhdr=%u flen=%u mask=%*ph val=%*ph]",
		   params->mac_addr, attr_mask,
		   params->rule_id, params->request_type, params->output_tid,
		   params->classifier_type, params->service_class_id, params->netdev_if_index,
		   params->tclas4_version, params->tclas4_src_port, params->tclas4_dst_port,
		   params->tclas4_dscp, params->tclas4_next_header,
		   params->tclas4_src_ipv4, params->tclas4_dst_ipv4,
		   params->tclas4_src_ipv6, params->tclas4_dst_ipv6,
		   params->tclas4_flow_label,
		   params->tclas10_protocol_instance, params->tclas10_next_header,
		   params->tclas10_filter_len,
		   params->tclas10_filter_len, params->tclas10_filter_mask,
		   params->tclas10_filter_len, params->tclas10_filter_value);
	cfg80211_vendor_event(skb, GFP_ATOMIC);
	return 0;

nla_fail:
	kfree_skb(skb);
	return -ENOBUFS;
}

int ath12k_vendor_rule_config_notify(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX + 1];
	struct ath12k_scs_rule_config_params params = {};
	struct nlattr *mask_attr, *val_attr;
	int flen;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX, data,
			data_len, ath12k_rule_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL,
			   "Invalid attribute in rule config notify %d\n", ret);
		return ret;
	}

	/* Mandatory: destination MAC address */
	if (!tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR] ||
	    nla_len(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR]) !=
	    ETH_ALEN) {
		ath12k_err(NULL, "Invalid MAC address in rule config notify\n");
		return -EINVAL;
	}
	memcpy(params.mac_addr,
	       nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR]),
	       ETH_ALEN);

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID]) {
		params.rule_id =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE]) {
		params.request_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID]) {
		params.output_tid =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE]) {
		params.classifier_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION]) {
		params.tclas4_version =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR]) {
		memcpy(params.tclas4_src_ipv4,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR]),
		       sizeof(params.tclas4_src_ipv4));
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR]) {
		memcpy(params.tclas4_dst_ipv4,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR]),
		       sizeof(params.tclas4_dst_ipv4));
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR]) {
		memcpy(params.tclas4_src_ipv6,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR]),
		       sizeof(params.tclas4_src_ipv6));
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR]) {
		memcpy(params.tclas4_dst_ipv6,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR]),
		       sizeof(params.tclas4_dst_ipv6));
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT]) {
		params.tclas4_src_port =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT]) {
		params.tclas4_dst_port =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP]) {
		params.tclas4_dscp =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER]) {
		params.tclas4_next_header =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL]) {
		memcpy(params.tclas4_flow_label,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL]),
		       sizeof(params.tclas4_flow_label));
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE]) {
		params.tclas10_protocol_instance =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER]) {
		params.tclas10_next_header =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER);
	}

	/* Variable-length TCLAS10 filter blobs: mask and value must match */
	mask_attr = tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK];
	val_attr  = tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE];
	if (mask_attr && val_attr) {
		flen = nla_len(mask_attr);
		if (flen == nla_len(val_attr) &&
		    flen <= ATH12K_SCS_RULE_TCLAS10_FILTER_LEN_MAX) {
			params.tclas10_filter_len = flen;
			memcpy(params.tclas10_filter_mask,
			       nla_data(mask_attr), flen);
			memcpy(params.tclas10_filter_value,
			       nla_data(val_attr), flen);
			params.present |=
				BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK) |
				BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE);
		} else {
			ath12k_err(NULL,
				    "SCS rule config: TCLAS10 filter length mismatch or overflow (%d)\n",
				    flen);
		}
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID]) {
		params.service_class_id =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX]) {
		params.netdev_if_index =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX]);
		params.present |=
			BIT(QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX);
	}

	ret = ath12k_vendor_send_rule_config_notify(vif, &params);
	if (ret)
		return ret;

	ath12k_info(NULL,
		    "scs rule config notify: mac=%pM rule_id=%u req_type=%u tid=%u\n",
		    params.mac_addr, params.rule_id,
		    params.request_type, params.output_tid);

	return 0;
}

int ath12k_vendor_get_sta_240mhz_info(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)
{
	struct nlattr *vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_MAX + 1];
	struct ieee80211_240mhz_vendor_oper_extn params_240mhz_extn = {0};
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	u8 *mac_addr = NULL;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct genl_info *info;

	if (!vif || !rdev || !ahvif)
		return -1;

	info = rdev->cur_cmd_info;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL, "vif type: %d is invalid", vif->type);
		return -1;
	}

	if (nla_parse(vendor, QCA_WLAN_VENDOR_ATTR_240MHZ_MAX,
		      data, data_len,
		      ath12k_240mhz_sta_info_policy,
		      NULL)) {
		ath12k_err(NULL, "Failed to parse vednor attributes");
		return -1;
	}

	params_240mhz_extn.is5ghz240mhz = true;

	if (vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS])
		params_240mhz_extn.bfmess320mhz =
			nla_get_u8(vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS]);

	if (vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS])
		params_240mhz_extn.numsound320mhz =
			nla_get_u8(vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS]);

	if (vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO])
		params_240mhz_extn.nonofdmaulmumimo320mhz = 1;

	if (vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER])
		params_240mhz_extn.mubfmr320mhz = 1;

	if (vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP]) {
		memcpy(params_240mhz_extn.mcs_map_320mhz,
		       nla_data(vendor[QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP]),
		       3);
	}

	if (info->attrs[NL80211_ATTR_CENTER_FREQ1])
		params_240mhz_extn.ccfs0 =
			nla_get_u32(info->attrs[NL80211_ATTR_CENTER_FREQ1]);

	if (info->attrs[NL80211_ATTR_CENTER_FREQ2])
		params_240mhz_extn.ccfs1 =
			nla_get_u32(info->attrs[NL80211_ATTR_CENTER_FREQ2]);

	if (info->attrs[NL80211_ATTR_PUNCT_BITMAP])
		params_240mhz_extn.punctured =
			nla_get_u32(info->attrs[NL80211_ATTR_PUNCT_BITMAP]);

	if (info->attrs[NL80211_ATTR_MAC])
		mac_addr = nla_data(info->attrs[NL80211_ATTR_MAC]);

	if (!mac_addr ||
	    ath12k_add_sta_240mhz_info_extn(ahvif, mac_addr,
					    &params_240mhz_extn)) {
		ath12k_err(NULL, "Failed to add the 240MHz extn information");
		return -1;
	}

	return 0;
}

/* Send a 'reload' NL80211 event to userspace
 * link_id is 'invalid' for non-mlo wdev.
 * For mlo wdev, if 'invalid' link_id is passed, userspace will reload all
 * BSSes of the mlo
 * If valid link_id is passed for mlo wdev, only that BSS is reset
 * in userspace.
 */
static int ath12k_vendor_event_iface_reload_link(struct wiphy *wiphy,
                                            struct wireless_dev *wdev, u8 link_id)
{
        struct sk_buff *skb;

        skb = cfg80211_vendor_event_alloc(wiphy, wdev,
                                          NLMSG_DEFAULT_SIZE,
                                          QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX,
                                          GFP_KERNEL);
        if (!skb)
                return -ENOMEM;

        if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_LINKID, link_id)) {
                kfree_skb(skb);
                return -EINVAL;
        }

        ath12k_dbg(NULL, ATH12K_DBG_CFG,
                   "send event to userspace link_id %d\n", link_id);
        cfg80211_vendor_event(skb, GFP_KERNEL);

        return 0;
}

/**
 * ath12k_vendor_suspend_device_event - Send a vendor NL event notifying
 * userspace of a suspend_device operation outcome.
 *
 * @ar:     ath12k radio identified by radio_idx
 * @status: 0 on success, negative error code on failure
 *
 * Uses the same event index and attribute set as the bypass_device event
 * (QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_INDEX) so userspace can handle both
 * with a single listener:
 *   PARAM  = QCA_WLAN_VENDOR_WIFI_PARAM_SUSPEND_DEVICE (96)
 *   VALUE  = ar->pdev_suspend (1 = suspended, 0 = resumed)
 *   STATUS = 0 on success, non-zero on failure
 */
static int ath12k_vendor_suspend_device_event(struct ath12k *ar,
					      int status)
{
	struct ath12k_base *ab = ar->ab;
	struct sk_buff *skb;

	if (!ar->ah || !ar->ah->hw) {
		ath12k_err(ab,
			   "suspend_device_event: no valid ar/hw to send event\n");
		return -EINVAL;
	}

	skb = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy,
					  NULL,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_INDEX,
					  GFP_KERNEL);
	if (!skb) {
		ath12k_err(ab,
			   "suspend_device_event: failed to alloc event skb\n");
		return -ENOMEM;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SET_RADIO_PARAM,
			QCA_WLAN_VENDOR_WIFI_PARAM_SUSPEND_DEVICE)) {
		ath12k_err(ab,
			   "suspend_device_event: failed to put RADIO_PARAM\n");
		kfree_skb(skb);
		return -ENOBUFS;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SET_RADIO_VALUE,
			ar->pdev_suspend)) {
		ath12k_err(ab,
			   "suspend_device_event: failed to put RADIO_VALUE\n");
		kfree_skb(skb);
		return -ENOBUFS;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SET_RADIO_STATUS,
			(u32)status)) {
		ath12k_err(ab,
			   "suspend_device_event: failed to put RADIO_STATUS\n");
		kfree_skb(skb);
		return -ENOBUFS;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	ath12k_info(ab,
		    "suspend_device_event: sent value=%u status=%d\n",
		    ar->pdev_suspend, status);
	return 0;
}

static int ath12k_wsi_bypass_event(struct ath12k_base *ab, int status)
{
	struct ath12k *ar;
	struct sk_buff *skb;

	if (ab->num_radios > 0 && ab->pdevs[0].ar)
		ar = ab->pdevs[0].ar;

	if (!ar || !ar->ah || !ar->ah->hw)
		return -EINVAL;

	skb = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy,
					  NULL,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_INDEX,
					  GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SET_RADIO_PARAM,
	    QCA_WLAN_VENDOR_RADIO_PARAM_WSI_BYPASS)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SET_RADIO_VALUE,
	    ab->wsi_remap_state)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SET_RADIO_STATUS,
	    status)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	ath12k_info(ab, "wsi_bypass_event: cfg80211_vendor_event sent with state %d, status %d\n", ab->wsi_remap_state, status);
	return 0;
}

/* Find the wdevs corresponding to the radio index of the wiphy
 * and send the reload event to userspace
 */
static int ath12k_vendor_event_iface_reload(struct wiphy *wiphy, u8 radio_idx)
{
	struct ieee80211_hw *hw = NULL;
	struct ath12k_hw *ah = NULL;
	struct ath12k *ar = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct wireless_dev *wdev = NULL;
	int ret;

	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = hw->priv;

	if (radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index %u\n", radio_idx);
		return -EINVAL;
	}

	ar = &ah->radio[radio_idx];
	list_for_each_entry(arvif, &ar->arvifs, list) {
		wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
		if (!wdev) {
			ath12k_err(NULL, "Failed to find wdev\n");
			return -ENODATA;
		}
		ret = ath12k_vendor_event_iface_reload_link(wiphy, wdev, arvif->link_id);
		if (ret)
			return ret;
	}
	return 0;
}

static bool ath12k_vendor_is_be_vap(struct ath12k_link_vif *arvif)
{
	struct ieee80211_bss_conf *bss_conf;

	if (!arvif)
		return false;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);

	return bss_conf && bss_conf->eht_support;
}

static int ath12k_set_ampdu_aggr_size_arvif(struct ath12k_link_vif *arvif, u32 value)
{
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct set_custom_aggr_size_params params = {0};
	int ret;

	if (!arvif || !arvif->ar)
		return -ENODEV;

	ar = arvif->ar;
	ab = ar->ab;

	params.vdev_id = arvif->vdev_id;
	params.aggr_type = WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU;
	params.ac = 0;
	params.tx_aggr_size = value;
	params.rx_aggr_size = 0;
	params.tx_aggr_size_disable = false;
	params.rx_aggr_size_disable = true;
	params.tx_ac_enable = true;

	ret = ath12k_wmi_send_aggr_size_cmd(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab,
				"AMPDU aggr size set failed vdev_id %d value %u ret %d\n",
				params.vdev_id, value, ret);
		return ret;
	}

	return 0;
}

static int ath12k_set_amsdu_aggr_size_arvif(struct ath12k_link_vif *arvif, u32 value)
{
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct set_custom_aggr_size_params params = {0};
	int ret;

	if (!arvif || !arvif->ar)
		return -ENODEV;

	ar = arvif->ar;
	ab = ar->ab;

	params.vdev_id = arvif->vdev_id;
	params.aggr_type = WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU;
	params.ac = 0;
	params.tx_aggr_size = value;
	params.rx_aggr_size = 0;
	params.tx_aggr_size_disable = false;
	params.rx_aggr_size_disable = true;
	params.tx_ac_enable = true;

	ret = ath12k_wmi_send_aggr_size_cmd(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab,
				"AMSDU aggr size set failed vdev_id %d value %u ret %d\n",
				params.vdev_id, value, ret);
		return ret;
	}
	return 0;
}

/**
 * ath12k_is_valid_basic_rate - Validate if a bitrate is a basic rate
 * @arvif: pointer to ath12k_link_vif structure
 * @bitrate: bitrate value in units of 100 kbps (e.g., 60 = 6 Mbps)
 *
 * Returns: true if the bitrate is a valid basic rate, false otherwise
 */
static bool ath12k_is_valid_basic_rate(struct ath12k_link_vif *arvif, u32 bitrate)
{
	struct ieee80211_vif *vif = NULL;
	struct ieee80211_bss_conf *bss_conf = NULL;
	struct ieee80211_hw *hw = NULL;
	const struct ieee80211_supported_band *sband = NULL;
	struct cfg80211_chan_def def;
	u8 rate_idx;
	int i;

	if (!arvif || !arvif->ar)
		return false;

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif)
		return false;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf)
		return false;

	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		return false;

	hw = ath12k_ar_to_hw(arvif->ar);
	sband = ieee80211_get_link_sband(wiphy_dereference(hw->wiphy,
			vif_to_sdata(vif)->link[arvif->link_id]) ?:
			&vif_to_sdata(vif)->deflink);
	if (!sband)
		return false;

	/* Search for the bitrate in the supported rates table */
	for (i = 0; i < sband->n_bitrates; i++) {
		if (sband->bitrates[i].bitrate == bitrate) {
			rate_idx = i;
			/* Check if this rate index bit is set in basic_rates bitmap */
			return !!(bss_conf->basic_rates & BIT(rate_idx));
		}
	}

	/* Bitrate not found in supported rates */
	return false;
}

/**
 * ath12k_set_vdev_mcast_rate - Set multicast rate for a vdev
 * @arvif: pointer to ath12k_link_vif structure
 * @bitrate: bitrate in units of 100 kbps (e.g., 60 = 6 Mbps)
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_set_vdev_mcast_rate(struct ath12k_link_vif *arvif, u32 bitrate)
{
	struct ieee80211_vif *vif = NULL;
	struct ieee80211_bss_conf *bss_conf = NULL;
	struct ieee80211_hw *hw = NULL;
	const struct ieee80211_supported_band *sband = NULL;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	struct ath12k *ar;
	u32 rate;
	u8 rate_idx;
	int ret;

	if (!arvif || !arvif->ar)
		return -EINVAL;

	ar = arvif->ar;

	/* Validate that the provided bitrate is a basic rate */
	if (!ath12k_is_valid_basic_rate(arvif, bitrate)) {
		ath12k_err(ar->ab, "Invalid mcast rate %u not a basic rate\n", bitrate);
		return -EINVAL;
	}

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif) {
		ath12k_err(ar->ab, "unable to get vif\n");
		return -EINVAL;
	}

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf) {
		ath12k_err(ar->ab, "unable to access bss link conf\n");
		return -EINVAL;
	}

	/* Get the current operating band */
	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)) {
		ath12k_err(ar->ab, "unable to get channel def\n");
		return -EINVAL;
	}
	band = def.chan->band;

	hw = ath12k_ar_to_hw(ar);
	sband = ieee80211_get_link_sband(wiphy_dereference(hw->wiphy,
			vif_to_sdata(vif)->link[arvif->link_id]) ?:
			&vif_to_sdata(vif)->deflink);
	if (!sband) {
		ath12k_err(ar->ab, "supported band not found\n");
		return -EINVAL;
	}

	/* Convert bitrate to rate index using existing helper */
	rate_idx = ath12k_mac_bitrate_to_idx(sband, bitrate);

	/* Convert bitrate to firmware rate format using existing helper */
	rate = ath12k_mac_get_rate_hw_value(bitrate);
	if (rate == -EINVAL) {
		ath12k_err(ar->ab, "Invalid bitrate %u\n", bitrate);
		return -EINVAL;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac vdev %d setting mcast_rate %x (bitrate %u)\n",
		   arvif->vdev_id, rate, bitrate);

	/* Send multicast rate to firmware */
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_MCAST_DATA_RATE, rate);
	if (ret) {
		ath12k_err(ar->ab,
			    "failed to set mcast rate on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	} else {
		ath12k_err(ar->ab,"Set mcast rate successful for vdev %i",
				arvif->vdev_id);
	}

	/*
	 * Only update mac80211's mcast_rate after firmware successfully accepts it.
	 * mac80211 stores mcast_rate as (rate_index + 1), where 0 means disabled.
	 */
	bss_conf->mcast_rate[band] = rate_idx + 1;

	return 0;
}

/**
 * ath12k_get_vdev_mcast_rate - Get multicast rate for a vdev
 * @arvif: pointer to ath12k_link_vif structure
 * @value: pointer to store the retrieved bitrate (in units of 100 kbps)
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_get_vdev_mcast_rate(struct ath12k_link_vif *arvif, u64 *value)
{
	struct ieee80211_bss_conf *bss_conf;
	struct cfg80211_chan_def def;
	struct ieee80211_vif *vif;
	struct ieee80211_hw *hw;
	const struct ieee80211_supported_band *sband;
	enum nl80211_band band;
	int mcast_idx;

	if (!arvif || !arvif->ar || !value)
		return -EINVAL;

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif)
		return -EINVAL;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf)
		return -EINVAL;

	/* Get the current operating band */
	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		return -EINVAL;

	if (!def.chan || def.chan->band >= NUM_NL80211_BANDS)
		return -EINVAL;

	band = def.chan->band;
	mcast_idx = bss_conf->mcast_rate[band];

	/* mcast_idx of 0 means default/disabled */
	if (mcast_idx == 0) {
		*value = 0;
		return 0;
	}

	hw = ath12k_ar_to_hw(arvif->ar);
	if (!hw || !hw->wiphy)
		return -EINVAL;

	sband = ieee80211_get_link_sband(wiphy_dereference(hw->wiphy,
			vif_to_sdata(vif)->link[arvif->link_id]) ?:
			&vif_to_sdata(vif)->deflink);
	if (!sband || !sband->bitrates)
		return -EINVAL;

	/* mcast_rate is stored as (rate_idx + 1), so subtract 1 to get actual index */
	if ((mcast_idx - 1) >= sband->n_bitrates) {
		ath12k_err(arvif->ar->ab,
			   "Invalid mcast rate index %d (max=%d)\n",
			   mcast_idx - 1, sband->n_bitrates - 1);
		return -EINVAL;
	}

	*value = sband->bitrates[mcast_idx - 1].bitrate;

	ath12k_dbg(arvif->ar->ab, ATH12K_DBG_MAC,
		   "mac vdev %d get mcast_rate bitrate=%llu (index=%d)\n",
		   arvif->vdev_id, *value, mcast_idx - 1);

	return 0;
}

/**
 * ath12k_get_vdev_bcast_rate - Get broadcast rate for a vdev
 * @arvif: pointer to ath12k_link_vif structure
 * @value: pointer to store the retrieved bitrate (in units of 100 kbps)
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_get_vdev_bcast_rate(struct ath12k_link_vif *arvif, u64 *value)
{
	if (!arvif || !arvif->ar || !value)
		return -EINVAL;

	*value = arvif->bcast_rate;

	ath12k_err(NULL,
		       "mac vdev %d get bcast_rate bitrate=%llu (%s)\n",
		       arvif->vdev_id, *value,
		       arvif->bcast_rate_configured ? "user-configured" : "default");
	return 0;
}

/**
 * ath12k_vendor_get_mintxpower - Get min tx power supported @ar
 * @ar: pointer to ath12k structure
 * @value: pointer to store the retrieved min tx power
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_vendor_get_mintxpower(struct ath12k *ar, u64 *value)
{
	s32 svalue = (s32)ar->min_tx_power;

	if(svalue < S8_MIN || svalue > S8_MAX)
		return -ERANGE;

	*value = (s8)ar->min_tx_power;

	return 0;
}

/**
 * ath12k_vendor_get_maxtxpower - Get max tx power supported by @ar
 * @ar: pointer to ath12k structure
 * @value: pointer to store the retrieved max tx power
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_vendor_get_maxtxpower(struct ath12k *ar, u64 *value)
{
	s32 svalue = (s32)ar->max_tx_power;

	if(svalue < S8_MIN || svalue > S8_MAX)
		return -ERANGE;

	*value = (s8)ar->max_tx_power;

	return 0;
}

/**
 * ath12k_vendor_get_regtxpower - Get max reg tx power supported by current operating channel
 * @arvif: pointer to ath12k_link_vif structure
 * @value: pointer to store the retrieved max tx power
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_vendor_get_regtxpower(struct ath12k_link_vif *arvif, u64 *value)
{
	struct ieee80211_vif *vif;
	struct cfg80211_chan_def def;
	s32 svalue;

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif)
		return -EINVAL;

	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		return -ENOTCONN;

	svalue = (s32)def.chan->max_reg_power;
	if(svalue < S8_MIN || svalue > S8_MAX)
		return -ERANGE;

	*value = (s8)def.chan->max_reg_power;
	return 0;
}

/**
 * ath12k_vendor_get_ctlpwrscale - Get current ctl power scale value
 * @ar: pointer to ath12k structure
 * @value: pointer to store the retrieved ctl power scale value
 *
 * Returns: 0 on success
 */
static int ath12k_vendor_get_ctlpwrscale(struct ath12k *ar, u32 *value)
{
	*value = ar->radio_cfg.ctlpwrscale;
	return 0;
}

/**
 * ath12k_vendor_set_ctlpwrscale - Set ctl power scale value
 * @ar: pointer to ath12k structure
 * @value: pointer to store the user input ctl power scale value
 * Returns: 0 on success, negative error code on failure.
 */
static int ath12k_vendor_set_ctlpwrscale(struct ath12k *ar, u32 value)
{
	int ret;

	if (value >= WMI_HOST_TP_SCALE_SIZE)
		return -EINVAL;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_CUST_TXPOWER_SCALE,
						value, ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab,
			"Failed to send WMI_PDEV_PARAM_CUST_TXPOWER_SCALE to firmware: %d\n", ret);
		return ret;
	}

	ar->radio_cfg.ctlpwrscale = value;
	ath12k_info(ar->ab, "CTL Power scale set to %u\n", value);

	return 0;
}

/**
 * ath12k_vendor_get_txpower_resolution - Get effective per-link TX power cap
 * @arvif: pointer to ath12k_link_vif structure
 * @value: pointer to store the retrieved tx power in 0.5 dBm units
 *
 * Requests FW pdev stats via WMI and reads chan_tx_power directly in its
 * native 0.5 dBm units as reported by firmware, without any integer division.
 * Falls back to ieee80211_bss_conf.txpower (mac80211 regulatory cap, 1 dBm
 * resolution) scaled to 0.5 dBm units if FW stats are unavailable.
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_vendor_get_txpower_resolution(struct ath12k_link_vif *arvif, u64 *value)
{
	struct ath12k_fw_stats_req_params params = {};
	struct ath12k_fw_stats_pdev *pdev;
	struct ieee80211_bss_conf *bss_conf;
	struct ath12k *ar;
	s32 svalue;
	int ret;

	if (!arvif || !arvif->ar)
		return -EINVAL;

	ar = arvif->ar;
	params.pdev_id = ar->pdev->pdev_id;
	params.vdev_id = arvif->vdev_id;
	params.stats_id = WMI_REQUEST_PDEV_STAT;
	ret = ath12k_mac_get_fw_stats(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request fw pdev stats: %d\n", ret);
		goto err_fallback;
	}

	spin_lock_bh(&ar->data_lock);
	pdev = list_first_entry_or_null(&ar->fw_stats.pdevs,
					struct ath12k_fw_stats_pdev, list);
	if (!pdev) {
		spin_unlock_bh(&ar->data_lock);
		goto err_fallback;
	}

	svalue = (s32)pdev->chan_tx_power;
	spin_unlock_bh(&ar->data_lock);

	if (svalue < S8_MIN || svalue > S8_MAX)
		return -ERANGE;

	*value = (u64)(s8)svalue;
	ath12k_info(ar->ab, "txpower from FW pdev stats %d.%02d dBm\n",
		    svalue / 2, abs(svalue % 2) * 50);
	return 0;

err_fallback:
	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf)
		return -EINVAL;

	if (bss_conf->txpower == INT_MIN)
		return -ENOTCONN;

	svalue = (s32)bss_conf->txpower * 2;
	if (svalue < S8_MIN || svalue > S8_MAX)
		return -ERANGE;

	*value = (u64)(s8)svalue;
	ath12k_info(ar->ab, "txpower fallback from bss_conf %d.%02d dBm\n",
		    svalue / 2, abs(svalue % 2) * 50);
	return 0;
}

static struct ieee80211_channel *
ath12k_vendor_get_radio_chan144(struct wiphy *wiphy, struct ath12k *ar)
{
	struct ieee80211_channel *chan;
	u32 chan144_khz = MHZ_TO_KHZ(ATH12K_CHAN144_FREQ);

	if (ar->freq_range.start_freq && ar->freq_range.end_freq &&
	    (chan144_khz < ar->freq_range.start_freq ||
	     chan144_khz > ar->freq_range.end_freq)) {
		ath12k_err(NULL, "CHAN144: radio %u does not support center freq %d MHz\n",
			   ar->pdev_idx, ATH12K_CHAN144_FREQ);
		return NULL;
	}

	chan = ieee80211_get_channel(wiphy, ATH12K_CHAN144_FREQ);
	if (!chan) {
		ath12k_err(NULL, "CHAN144: freq %d MHz not found in wiphy\n",
			   ATH12K_CHAN144_FREQ);
		return NULL;
	}

	return chan;
}

static void ath12k_vendor_notify_reg_change_extn(struct wiphy *wiphy,
						 const char *reason)
{
	const struct ieee80211_regdomain *regd;
	int ret;

	regd = get_wiphy_regdom(wiphy);
	if (!regd)
		return;

	ret = regulatory_set_wiphy_regd(wiphy, (struct ieee80211_regdomain *)regd);
	if (ret)
		ath12k_err(NULL,
			   "%s: failed to notify userspace reg change, ret=%d\n",
			   reason ? reason : "REG", ret);
}

static void ath12k_vendor_chan144_notify_reg_change(struct wiphy *wiphy)
{
	ath12k_vendor_notify_reg_change_extn(wiphy, "CHAN144");
}

static bool reg_is_reg_unii_band_1_set(uint8_t unii_bitmap)
{
	return !!(unii_bitmap & BIT(REG_UNII_BAND_1));
}

static bool reg_is_reg_unii_band_2a_set(uint8_t unii_bitmap)
{
	return !!(unii_bitmap & BIT(REG_UNII_BAND_2A));
}

static bool is_chan_present_in_regd(struct ieee80211_channel *chan,
				    const struct ieee80211_regdomain *regd)
{
	u32 center_freq_khz = ieee80211_channel_to_khz(chan);
	const struct ieee80211_reg_rule *reg_rule;

	reg_rule = freq_reg_info_regd(center_freq_khz, regd, MHZ_TO_KHZ(20), 0);
	if (!IS_ERR(reg_rule))
		return true;

	ath12k_dbg(NULL, ATH12K_DBG_REG,
		   "chan_coex_disable: %d MHz not found in regd\n",
		   chan->center_freq);
	return false;
}

static inline bool ath12k_is_chan_dfs_and_not_in_NOL(struct ieee80211_channel *chan)
{
	return (chan->flags & IEEE80211_CHAN_RADAR) &&
	       (chan->dfs_state != NL80211_DFS_UNAVAILABLE);
}

static void ath12k_vendor_coex_apply_band(struct wiphy *wiphy,
					  const u16 *freqs, int nfreqs,
					  bool is_disable,
					  const struct ieee80211_regdomain *regd)
{
	int i;

	for (i = 0; i < nfreqs; i++) {
		struct ieee80211_channel *chan;

		chan = ieee80211_get_channel(wiphy, freqs[i]);
		if (!chan) {
			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "chan_coex_disable: %d MHz not found in wiphy\n",
				   freqs[i]);
			continue;
		}

		if (is_disable) {
			chan->flags |= IEEE80211_CHAN_DISABLED;
			if (ath12k_is_chan_dfs_and_not_in_NOL(chan))
				chan->dfs_state = NL80211_DFS_USABLE;
			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "chan_coex_disable: disabled %d MHz\n",
				   chan->center_freq);
		} else if (is_chan_present_in_regd(chan, regd)) {
			chan->flags &= ~IEEE80211_CHAN_DISABLED;
			if (ath12k_is_chan_dfs_and_not_in_NOL(chan))
				chan->dfs_state = NL80211_DFS_USABLE;
			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "chan_coex_disable: enabled %d MHz\n",
				   chan->center_freq);
		}
	}
}

/**
 * ath12k_vendor_coex_validate_radio() - validate radio for chan_coex_disable
 * @ar: ath12k radio instance to validate
 *
 * Checks that @ar is a 5 GHz radio (WMI_HOST_WLAN_5GHZ_CAP set) and that
 * its hal_reg_cap frequency range covers at least part of the UNII-1 or
 * UNII-2A bands.
 *
 * Return: 0 on success, -EOPNOTSUPP otherwise.
 */
static int ath12k_vendor_coex_validate_radio(struct ath12k *ar)
{
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *reg_cap;
	u8 phy_id;

	if (!(ar->pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP)) {
		ath12k_dbg(NULL, ATH12K_DBG_REG,
			   "chan_coex_disable: Radio %u is not a 5 GHz radio\n",
			   ar->pdev_idx);
		return -EOPNOTSUPP;
	}

	phy_id = ar->ab->hw_params->single_pdev_only ?
		 ar->pdev->cap.band[WMI_HOST_WLAN_5GHZ_CAP].phy_id :
		 ar->pdev_idx;

	reg_cap = &ar->ab->hal_reg_cap[phy_id];
	if (reg_cap->low_5ghz_chan > ATH12K_MIN_5G_HIGH_BAND_FREQ &&
	    reg_cap->high_5ghz_chan < ATH12K_5GHZ_MAX_CENTER) {
		ath12k_dbg(NULL, ATH12K_DBG_REG,
			   "chan_coex_disable: Radio %u does not cover UNII-1/2A\n",
			   ar->pdev_idx);
		return -EOPNOTSUPP;
	}

	return 0;
}

/**
 * ath12k_vendor_coex_apply_mask() - update channel flags for UNII-1/2A
 * @wiphy: wiphy whose channel list to update
 * @coex_mask: bitmask selecting bands to disable — bit 0 set means UNII-1
 *             (5180–5240 MHz) disabled; bit 1 set means UNII-2A (5260–5320 MHz)
 *             disabled.  A value of 0 restores both bands.
 * @regd: current regulatory domain used to gate re-enabling channels
 *
 * Updates IEEE80211_CHAN_DISABLED on the affected channels.  Re-enable is
 * gated on is_chan_present_in_regd() so a channel that is prohibited by the
 * current regulatory domain is never re-enabled.  UNII-2A channels that are
 * on the NOL (dfs_state == NL80211_DFS_UNAVAILABLE) are skipped in both
 * directions.
 */
static void ath12k_vendor_coex_apply_mask(struct wiphy *wiphy, u8 coex_mask,
					  const struct ieee80211_regdomain *regd)
{
	static const u16 unii1_freqs[]  = {5180, 5200, 5220, 5240};
	static const u16 unii2a_freqs[] = {5260, 5280, 5300, 5320};
	bool dis1  = reg_is_reg_unii_band_1_set(coex_mask);
	bool dis2a = reg_is_reg_unii_band_2a_set(coex_mask);

	ath12k_vendor_coex_apply_band(wiphy, unii1_freqs,
				      ARRAY_SIZE(unii1_freqs),
				      dis1, regd);
	ath12k_vendor_coex_apply_band(wiphy, unii2a_freqs,
				      ARRAY_SIZE(unii2a_freqs),
				      dis2a, regd);
}

/**
 * ath12k_vendor_coex_notify_chan_update() - notify cfg80211 of channel change
 * @wiphy: wiphy whose channel list was modified
 *
 * Notifies the driver's regulatory notifier (if WIPHY_FLAG_NOTIFY_REGDOM_BY_DRIVER
 * is set) and multicasts an NL80211_CMD_WIPHY_REG_CHANGE event to userspace.
 *
 * Safe to call with wiphy_lock held and RTNL not held.  nl80211_wiphy_reg_change_event()
 * takes the wiphy pointer directly, bypassing wiphy_idx_to_wiphy() which
 * carries ASSERT_RTNL(), so there is no risk of an AB-BA deadlock.
 */
static void ath12k_vendor_coex_notify_chan_update(struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *regd;
	struct regulatory_request request = {};

	regd = get_wiphy_regdom(wiphy);
	if (!regd)
		return;

	cfg80211_init_reg_request(&request, wiphy, regd->alpha2);

	if (wiphy->flags & WIPHY_FLAG_NOTIFY_REGDOM_BY_DRIVER)
		reg_call_notifier(wiphy, &request);

	nl80211_wiphy_reg_change_event(wiphy, &request);
}

static int ath12k_vendor_set_chan_coex_disable(struct ath12k *ar, u8 coex_mask)
{
	struct wiphy *wiphy = ath12k_ar_to_hw(ar)->wiphy;
	const struct ieee80211_regdomain *regd;
	int ret;

	ret = ath12k_vendor_coex_validate_radio(ar);
	if (ret != 0) {
		ath12k_err(NULL, "chan_coex_disable: Radio validation failed\n");
		return ret;
	}

	if (ar->chan_coex_disable_mask == coex_mask) {
		ath12k_dbg(NULL, ATH12K_DBG_REG,
			   "chan_coex_disable: mask unchanged (0x%x), skipping\n",
			   coex_mask);
		return 0;
	}

	regd = ath12k_get_current_regd(ar);
	if (!regd) {
		ath12k_err(NULL, "chan_coex_disable: regd not available\n");
		return -EINVAL;
	}

	ath12k_vendor_coex_apply_mask(wiphy, coex_mask, regd);
	ar->chan_coex_disable_mask = coex_mask;
	ath12k_vendor_coex_notify_chan_update(wiphy);

	return 0;
}

static int ath12k_vendor_chan144_apply(struct ath12k *ar, u8 enable)
{
	struct wiphy *wiphy = ath12k_ar_to_hw(ar)->wiphy;
	struct ieee80211_channel *chan;
	const struct ieee80211_reg_rule *reg_rule;
	u32 old_flags;
	int ret = 0;

	if (enable > CHAN_144_EN) {
		ath12k_err(NULL, "CHAN144: invalid input %u\n", enable);
		return -EINVAL;
	}

	chan = ath12k_vendor_get_radio_chan144(wiphy, ar);
	if (!chan)
		return -ENODATA;

	old_flags = chan->flags;
	if (!enable) {
		chan->flags |= IEEE80211_CHAN_DISABLED;
		chan->orig_flags |= IEEE80211_CHAN_DISABLED;
	} else {
		chan->flags &= ~IEEE80211_CHAN_DISABLED;
		chan->orig_flags &= ~IEEE80211_CHAN_DISABLED;
	}

	if (enable) {
		reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(ATH12K_CHAN144_FREQ));
		if (IS_ERR_OR_NULL(reg_rule)) {
			ath12k_err(NULL, "CHAN144: regulatory disallows enabling freq %d\n",
				   ATH12K_CHAN144_FREQ);
			return -EPERM;
		}
	}

	if (ar->ah->state == ATH12K_HW_STATE_ON) {
		ret = ath12k_reg_update_chan_list(ar, true);
		if (ret) {
			chan->flags = old_flags;
			chan->orig_flags = old_flags;
			ath12k_err(NULL, "CHAN144: failed to sync FW channel list, ret=%d\n",
				   ret);
			return ret;
		}
	}

	ar->radio_cfg.chan144_enabled = enable;
	ath12k_info(NULL, "CHAN144: policy %s applied on radio %u (flags 0x%x -> 0x%x)\n",
		    enable ? "enable" : "disable", ar->pdev_idx, old_flags, chan->flags);

	ath12k_vendor_chan144_notify_reg_change(wiphy);
	return 0;
}

int ath12k_vendor_get_en_chan144(struct ath12k *ar, u32 *value)
{
	*value = ar->radio_cfg.chan144_enabled;
	return 0;
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
int ath12k_mac_vif_setmhdr(struct ath12k_link_vif *arvif, uint32_t mhdr)
{
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;
	struct ath12k *ar;
	struct ath12k_base *ab;
	int preamble_type;

	/* Validate arvif, ar, and ab pointers first to get ab for logging */
	if (!arvif || !arvif->ar || !arvif->ar->ab) {
		pr_err("ath12k: invalid pointers in set_offload_mode\n");
		return -EINVAL;
	}

	ar = arvif->ar;
	ab = ar->ab;

	/* Validate ahvif pointer */
	ahvif = arvif->ahvif;
	if (!ahvif) {
		ath12k_err(ab, "ahvif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}


	/* Validate vif pointer */
	vif = ath12k_ahvif_to_vif(ahvif);
	if (!vif) {
		ath12k_err(ab, "vif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}

	preamble_type = (mhdr >> MESH_DBG_PRAMBLE_OFFSET) & MESH_NIBBLE_MASK;

	if (preamble_type > METAHDR_PREAMBLE_EHT) {
		pr_err("ERR: preamble type is greater than EHT\n");
		return -EINVAL;
	}

	ahvif->dp_vif.dp_extn.mhdr = mhdr;
	return 0;
}

int ath12k_mac_vif_set_mesh_dbg(struct ath12k_link_vif *arvif, uint32_t mdbg)
{
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;
	struct ath12k *ar;
	struct ath12k_base *ab;

	/* Validate arvif, ar, and ab pointers first to get ab for logging */
	if (!arvif || !arvif->ar || !arvif->ar->ab) {
		pr_err("ath12k: invalid pointers in set_offload_mode\n");
		return -EINVAL;
	}

	ar = arvif->ar;
	ab = ar->ab;

	/* Validate ahvif pointer */
	ahvif = arvif->ahvif;
	if (!ahvif) {
		ath12k_err(ab, "ahvif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}


	/* Validate vif pointer */
	vif = ath12k_ahvif_to_vif(ahvif);
	if (!vif) {
		ath12k_err(ab, "vif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}

	ahvif->dp_vif.dp_extn.mdbg = mdbg;
	return 0;
}

int ath12k_mac_vif_set_mesh_rx_filter(struct ath12k_link_vif *arvif, uint32_t rx_filter)
{
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;
	struct ath12k *ar;
	struct ath12k_base *ab;

	/* Validate arvif, ar, and ab pointers first to get ab for logging */
	if (!arvif || !arvif->ar || !arvif->ar->ab) {
		pr_err("ath12k: invalid pointers in set_offload_mode\n");
		return -EINVAL;
	}

	ar = arvif->ar;
	ab = ar->ab;

	/* Validate ahvif pointer */
	ahvif = arvif->ahvif;
	if (!ahvif) {
		ath12k_err(ab, "ahvif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}


	/* Validate vif pointer */
	vif = ath12k_ahvif_to_vif(ahvif);
	if (!vif) {
		ath12k_err(ab, "vif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}

	/* rx filter shouldn't have bits set beyond MESH_FILTER_OUT_TA */
	if (rx_filter > 0x1F)
		return -EINVAL;

	ahvif->dp_vif.dp_extn.rx_filter = rx_filter;
	return 0;
}

int ath12k_mac_vif_set_mesh_tx(struct ath12k_link_vif *arvif, uint32_t mesh_tx)
{
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;
	struct ath12k *ar;
	struct ath12k_base *ab;

	/* Validate arvif, ar, and ab pointers first to get ab for logging */
	if (!arvif || !arvif->ar || !arvif->ar->ab) {
		pr_err("ath12k: invalid pointers in set_offload_mode\n");
		return -EINVAL;
	}

	ar = arvif->ar;
	ab = ar->ab;

	/* Validate ahvif pointer */
	ahvif = arvif->ahvif;
	if (!ahvif) {
		ath12k_err(ab, "ahvif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}


	/* Validate vif pointer */
	vif = ath12k_ahvif_to_vif(ahvif);
	if (!vif) {
		ath12k_err(ab, "vif is NULL for vdev %d\n", arvif->vdev_id);
		return -EINVAL;
	}

	ahvif->dp_vif.dp_extn.mesh_tx = mesh_tx;
	return 0;
}

/**
 * ath12k_set_vdev_bcast_rate - Set broadcast rate for a vdev
 * @arvif: pointer to ath12k_link_vif structure
 * @bitrate: bitrate in units of 100 kbps (e.g., 60 = 6 Mbps)
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_set_vdev_bcast_rate(struct ath12k_link_vif *arvif, u32 bitrate)
{
	struct ath12k *ar;
	u32 rate;
	u32 prev_bcast_rate;
	int ret;
	bool prev_bcast_configured;

	if (!arvif || !arvif->ar)
		return -EINVAL;

	ar = arvif->ar;

	/* Validate that the provided bitrate is a basic rate */
	if (!ath12k_is_valid_basic_rate(arvif, bitrate)) {
		ath12k_err(ar->ab, "Invalid bcast rate %u not a basic rate\n", bitrate);
		return -EINVAL;
	}

	/* Convert bitrate to firmware rate format using existing helper */
	rate = ath12k_mac_get_rate_hw_value(bitrate);
	if (rate == -EINVAL) {
		ath12k_err(ar->ab, "Invalid bitrate %u\n", bitrate);
		return -EINVAL;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac vdev %d setting bcast_rate %x (bitrate %u)\n",
		   arvif->vdev_id, rate, bitrate);

	/* Save previous state for rollback */
	prev_bcast_configured = arvif->bcast_rate_configured;
	prev_bcast_rate = arvif->bcast_rate;

	/* Send broadcast rate to firmware */
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_BCAST_DATA_RATE, rate);
	if (ret) {
		ath12k_err(ar->ab,
			    "failed to set bcast rate on vdev %i %d\n",
			    arvif->vdev_id, ret);

		/* Attempt to restore previous broadcast rate if it was configured */
		if (prev_bcast_configured && prev_bcast_rate != 0) {
			u32 prev_rate = ath12k_mac_get_rate_hw_value(prev_bcast_rate);

			if (prev_rate != -EINVAL) {
				if (ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
								  WMI_VDEV_PARAM_BCAST_DATA_RATE,
								  prev_rate))
					ath12k_err(ar->ab,
						   "failed to restore previous bcast rate %u on vdev %i\n",
						   prev_bcast_rate, arvif->vdev_id);
			}
		}

		return ret;
	}

	/* Firmware accepted the rate - update our state */
	arvif->bcast_rate_configured = true;
	arvif->bcast_rate = bitrate;

	return 0;
}
#endif

/**
 * ath12k_chainmask_validate - Validate chainmask against capabilities
 * @ar: pointer to ath12k structure
 * @mask: chainmask value to validate
 * @is_tx: true for TX chainmask, false for RX chainmask
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_chainmask_validate(struct ath12k *ar, u32 mask, bool is_tx)
{
	u32 max_mask;

    	if (!ar || !ar->pdev) {
        	ath12k_err(NULL, "Invalid ar or pdev pointer\n");
        	return -EINVAL;
    	}

	max_mask = is_tx ? ar->pdev->cap.tx_chain_mask :
			       ar->pdev->cap.rx_chain_mask;

	if (!max_mask) {
		ath12k_err(ar->ab, "Invalid %s chainmask capability (0x%x)\n",
			is_tx ? "TX" : "RX", max_mask);
		return -EINVAL;
	}

	if (!mask) {
		ath12k_err(ar->ab, "Mask should be non-zero\n");
		return -EINVAL;
	}

	if((mask & max_mask) != mask){
		ath12k_err(ar->ab, "Mask invalid value=%u\t max_mask=%u\n",
			   mask, max_mask);
		return -EINVAL;
	}

	return 0;
}

/**
 * ath12k_set_chainmask - Validate and set TX/RX chainmask
 * @ar: pointer to ath12k structure
 * @value: chainmask value to set
 * @is_tx: true for TX chainmask, false for RX chainmask
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ath12k_set_chainmask(struct ath12k *ar, u32 value, bool is_tx)
{
	int ret;

	ret = ath12k_chainmask_validate(ar, value, is_tx);

	if (ret)
        	return ret;

    	if (is_tx)
        	return ath12k_mac_set_tx_antenna(ar, value);

    	return ath12k_mac_set_rx_antenna(ar, value);
}

static u8 ath12k_vendor_get_max_tx_nss(struct ath12k *ar)
{
	if (!ar || !ar->pdev)
		return 0;

	return hweight32(ar->pdev->cap.tx_chain_mask);
}

static int ath12k_vendor_get_link_band(struct ath12k_link_vif *arvif,
				       enum nl80211_band *band)
{
	struct ieee80211_vif *vif;
	struct cfg80211_chan_def def;

	if (!arvif || !band)
		return -EINVAL;

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif)
		return -EINVAL;

	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def) || !def.chan)
		return -EINVAL;

	*band = def.chan->band;
	return 0;
}

static int ath12k_vendor_resolve_nss(struct ath12k_link_vif *arvif,
				     u32 requested_nss, u8 *nss)
{
	struct ath12k *ar = arvif->ar;
	u8 max_nss;

	if (!ar || !nss)
		return -EINVAL;

	max_nss = ath12k_vendor_get_max_tx_nss(ar);
	if (!max_nss) {
		ath12k_err(ar->ab, "invalid tx chain capability for nss\n");
		return -EINVAL;
	}

	if (!requested_nss)
		requested_nss = max_nss;

	if (requested_nss > max_nss) {
		ath12k_err(ar->ab, "Invalid nss %u (max %u)\n",
			   requested_nss, max_nss);
		return -EINVAL;
	}

	*nss = requested_nss;
	return 0;
}

static int ath12k_vendor_validate_he_eht_mode(struct ath12k_link_vif *arvif,
					      bool eht)
{
	struct ieee80211_bss_conf *bss_conf;

	/* Allow pre-config before vdev is brought up. */
	if (!arvif->is_up)
		return 0;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf)
		return 0;

	if (eht && !bss_conf->eht_support) {
		ath12k_err(arvif->ar->ab, "EHT fixed rate is not allowed in current mode\n");
		return -EINVAL;
	}

	if (!eht && !bss_conf->he_support) {
		ath12k_err(arvif->ar->ab, "HE fixed rate is not allowed in current mode\n");
		return -EINVAL;
	}

	return 0;
}

static int
ath12k_vendor_set_fixed_rate(struct ath12k_link_vif *arvif, u32 value, u32 mode)
{
	struct ath12k *ar = arvif->ar;
	u32 rate_code = WMI_FIXED_RATE_NONE;
	u32 config_value = value;
	u8 nss, rate_idx;
	enum nl80211_band band;
	int ret;

	switch (mode) {
	case ATH12K_VENDOR_FIXED_RATE_LEGACY_HT:
		if (!value || value == WMI_FIXED_RATE_NONE) {
			break;
		}

		rate_idx = value & ATH12K_VENDOR_FIXED_RATE_RATE_IDX_MASK;

		if (value & ATH12K_VENDOR_FIXED_RATE_HT_MCS_BIT) {
			nss = ((value & ATH12K_VENDOR_FIXED_RATE_HT_NSS_MASK) >>
			       ATH12K_VENDOR_FIXED_RATE_HT_NSS_SHIFT) + 1;
			ret = ath12k_vendor_resolve_nss(arvif, nss, &nss);
			if (ret)
				return ret;

			rate_code = ATH12K_HW_RATE_CODE(rate_idx, nss - 1,
							WMI_RATE_PREAMBLE_HT, 0);
			break;
		}

		if (value & ATH12K_VENDOR_FIXED_RATE_LEGACY_CCK_BIT) {
			ret = ath12k_vendor_get_link_band(arvif, &band);
			if (!ret && band != NL80211_BAND_2GHZ) {
				ath12k_err(ar->ab,
					   "CCK fixed rate is valid only for 2.4 GHz\n");
				return -EINVAL;
			}

			if (rate_idx != ATH12K_VENDOR_FIXED_RATE_CCK_1MBPS_IDX)
				rate_idx |= ATH12K_VENDOR_FIXED_RATE_CCK_SHORT_PREAMBLE_BIT;

			rate_code = ATH12K_HW_RATE_CODE(rate_idx, 0,
							WMI_RATE_PREAMBLE_CCK, 0);
			break;
		}

		rate_code = ATH12K_HW_RATE_CODE(rate_idx, 0,
						WMI_RATE_PREAMBLE_OFDM, 0);
		break;
	case ATH12K_VENDOR_FIXED_RATE_VHT_MCS:
		if (value > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "VHT MCS %u exceeds max %u, clamping\n",
				    value, ATH12K_VHT_MCS_MAX);
			config_value = ATH12K_VHT_MCS_MAX;
		}

		ret = ath12k_vendor_resolve_nss(arvif, arvif->vap_cfg.nss, &nss);
		if (ret)
			return ret;

		rate_code = ATH12K_HW_RATE_CODE(config_value, nss - 1,
						WMI_RATE_PREAMBLE_VHT, 0);
		break;
	case ATH12K_VENDOR_FIXED_RATE_HE_MCS:
		ret = ath12k_vendor_validate_he_eht_mode(arvif, false);
		if (ret)
			return ret;

		if (value > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "HE MCS %u exceeds max %u, clamping\n",
				    value, ATH12K_HE_MCS_MAX);
			config_value = ATH12K_HE_MCS_MAX;
		}

		ret = ath12k_vendor_resolve_nss(arvif, arvif->vap_cfg.nss, &nss);
		if (ret)
			return ret;

		rate_code = ATH12K_HW_RATE_CODE(config_value, nss - 1,
						WMI_RATE_PREAMBLE_HE, 0);
		break;
	case ATH12K_VENDOR_FIXED_RATE_EHT_MCS:
		ret = ath12k_vendor_validate_he_eht_mode(arvif, true);
		if (ret)
			return ret;

		if (value > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "EHT MCS %u exceeds max %u, clamping\n",
				    value, ATH12K_EHT_MCS_MAX);
			config_value = ATH12K_EHT_MCS_MAX;
		}

		ret = ath12k_vendor_resolve_nss(arvif, arvif->vap_cfg.nss, &nss);
		if (ret)
			return ret;

		rate_code = ATH12K_HW_RATE_CODE(config_value, nss - 1,
						WMI_RATE_PREAMBLE_EHT, 0);
		break;
	default:
		ath12k_err(ar->ab, "Invalid fixed rate mode %u\n", mode);
		return -EINVAL;
	}

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_FIXED_RATE, rate_code);
	if (ret) {
		ath12k_err(ar->ab, "Failed to send WMI_VDEV_PARAM_FIXED_RATE to firmware\n");
		return ret;
	}

	arvif->vap_cfg.fixed_rate = config_value;
	switch (mode) {
	case ATH12K_VENDOR_FIXED_RATE_VHT_MCS:
		arvif->vap_cfg.fixed_vht_mcs = config_value;
		break;
	case ATH12K_VENDOR_FIXED_RATE_HE_MCS:
		arvif->vap_cfg.fixed_he_mcs = config_value;
		break;
	case ATH12K_VENDOR_FIXED_RATE_EHT_MCS:
		arvif->vap_cfg.fixed_eht_mcs = config_value;
		break;
	default:
		break;
	}

	return 0;
}

static int
ath12k_vendor_set_ul_fixed_rate(struct ath12k_link_vif *arvif, u32 value, u32 mode)
{
	struct ath12k *ar = arvif->ar;
	u32 rate_code = WMI_FIXED_RATE_NONE;
	u32 config_value = value;
	u32 requested_nss;
	u8 nss;
	int ret;

	switch (mode) {
	case ATH12K_VENDOR_UL_FIXED_RATE_HE:
		ret = ath12k_vendor_validate_he_eht_mode(arvif, false);
		if (ret)
			return ret;

		if (value != WMI_FIXED_RATE_NONE && value > ATH12K_HE_MCS_MAX) {
			ath12k_err(ar->ab,
				   "UL HE MCS value %u cannot be greater than %u\n",
				   value, ATH12K_HE_MCS_MAX);
			return -EINVAL;
		}

		requested_nss = arvif->vap_cfg.he_ul_nss ?: arvif->vap_cfg.nss;
		ret = ath12k_vendor_resolve_nss(arvif, requested_nss, &nss);
		if (ret)
			return ret;

		if (value != WMI_FIXED_RATE_NONE)
			rate_code = ATH12K_HW_RATE_CODE(config_value, nss - 1,
							WMI_RATE_PREAMBLE_HE, 0);

		break;
	case ATH12K_VENDOR_UL_FIXED_RATE_EHT:
		ret = ath12k_vendor_validate_he_eht_mode(arvif, true);
		if (ret)
			return ret;

		if (value != WMI_FIXED_RATE_NONE && value > ATH12K_EHT_MCS_MAX) {
			ath12k_err(ar->ab,
				   "UL EHT MCS value %u cannot be greater than %u\n",
				   value, ATH12K_EHT_MCS_MAX);
			return -EINVAL;
		}

		requested_nss = arvif->vap_cfg.eht_ul_nss ?: arvif->vap_cfg.nss;
		ret = ath12k_vendor_resolve_nss(arvif, requested_nss, &nss);
		if (ret)
			return ret;

		if (value != WMI_FIXED_RATE_NONE)
			rate_code = ATH12K_HW_RATE_CODE(config_value, nss - 1,
							WMI_RATE_PREAMBLE_EHT, 0);
		break;
	default:
		ath12k_err(ar->ab, "Invalid UL fixed rate mode %u\n", mode);
		return -EINVAL;
	}

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_UL_FIXED_RATE,
					    rate_code);
	if (ret) {
		ath12k_err(ar->ab, "Failed to send WMI_VDEV_PARAM_UL_FIXED_RATE to firmware\n");
		return ret;
	}

	arvif->vap_cfg.ul_fixed_rate = config_value;
	if (mode == ATH12K_VENDOR_UL_FIXED_RATE_HE) {
		arvif->vap_cfg.ul_fixed_he_mcs = config_value;
		if (!arvif->vap_cfg.he_ul_ldpc && value != WMI_FIXED_RATE_NONE &&
		    config_value > ATH12K_VHT_MCS_MAX)
			arvif->vap_cfg.he_ul_ldpc = 1;
	} else {
		arvif->vap_cfg.ul_fixed_eht_mcs = config_value;
		if (!arvif->vap_cfg.eht_ul_ldpc && value != WMI_FIXED_RATE_NONE &&
		    config_value > ATH12K_VHT_MCS_MAX)
			arvif->vap_cfg.eht_ul_ldpc = 1;
	}

	return 0;
}

static int ath12k_vendor_get_fixed_rate(struct ath12k_link_vif *arvif,
					u32 mode, u64 *value)
{
	switch (mode) {
	case ATH12K_VENDOR_FIXED_RATE_LEGACY_HT:
		*value = arvif->vap_cfg.fixed_rate;
		break;
	case ATH12K_VENDOR_FIXED_RATE_VHT_MCS:
		*value = arvif->vap_cfg.fixed_vht_mcs;
		break;
	case ATH12K_VENDOR_FIXED_RATE_HE_MCS:
		*value = arvif->vap_cfg.fixed_he_mcs;
		break;
	case ATH12K_VENDOR_FIXED_RATE_EHT_MCS:
		*value = arvif->vap_cfg.fixed_eht_mcs;
		break;
	default:
		ath12k_err(arvif->ar->ab, "Invalid fixed rate mode %u\n", mode);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_vendor_get_ul_fixed_rate(struct ath12k_link_vif *arvif,
					   u32 mode, u64 *value)
{
	switch (mode) {
	case ATH12K_VENDOR_UL_FIXED_RATE_HE:
		*value = arvif->vap_cfg.ul_fixed_he_mcs;
		break;
	case ATH12K_VENDOR_UL_FIXED_RATE_EHT:
		*value = arvif->vap_cfg.ul_fixed_eht_mcs;
		break;
	default:
		ath12k_err(arvif->ar->ab, "Invalid UL fixed rate mode %u\n", mode);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_vendor_validate_he_or_eht_mode(struct ath12k_link_vif *arvif)
{
	struct ieee80211_bss_conf *bss_conf;

	/* Allow pre-config before vdev is brought up. */
	if (!arvif->is_up)
		return 0;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf)
		return 0;

	if (!bss_conf->he_support && !bss_conf->eht_support) {
		ath12k_err(arvif->ar->ab,
			   "HE autorate GI/LTF is not allowed in current mode\n");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_vendor_set_he_ltf(struct ath12k_link_vif *arvif,
					    u32 value, bool ul_ltf)
{
	struct ath12k *ar = arvif->ar;
	u32 param = ul_ltf ? WMI_VDEV_PARAM_UL_HE_LTF : WMI_VDEV_PARAM_HE_LTF;
	int ret;

	ret = ath12k_vendor_validate_he_eht_mode(arvif, false);
	if (ret)
		return ret;

	if (value > IEEE80211_HE_LTF_4X) {
		ath12k_err(ar->ab,
			   "HE LTF value should be less than or equal to 0x%x\n",
			   IEEE80211_HE_LTF_4X);
		return -EINVAL;
	}

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param, value);
	if (ret) {
		ath12k_err(ar->ab, "Failed to send HE LTF param %u to firmware\n",
			   param);
		return ret;
	}

	if (ul_ltf)
		arvif->vap_cfg.he_ul_ltf = value;
	else
		arvif->vap_cfg.he_ltf = value;

	return 0;
}

static int ath12k_vendor_send_he_ar_cfg(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	u32 value = arvif->vap_cfg.he_ar_gi_ltf;
	int ret;

	if (arvif->vap_cfg.he_ar_ldpc != IEEE80211_HE_AR_LDPC_DEFAULT)
		value |= arvif->vap_cfg.he_ar_ldpc << IEEE80211_HE_AR_LDPC_SHIFT;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_AUTORATE_MISC_CFG,
					    value);
	if (ret)
		ath12k_err(ar->ab,
			   "Failed to send WMI_VDEV_PARAM_AUTORATE_MISC_CFG to firmware\n");

	return ret;
}

static int ath12k_vendor_set_he_ar_gi_ltf(struct ath12k_link_vif *arvif,
						 u32 value)
{
	u32 he_ar_ltf;
	u32 he_ar_sgi;
	int ret;

	ret = ath12k_vendor_validate_he_or_eht_mode(arvif);
	if (ret)
		return ret;

	if (!value || value & ~(IEEE80211_HE_AR_LTF_MASK | IEEE80211_HE_AR_SGI_MASK)) {
		ath12k_err(arvif->ar->ab, "Invalid HE autorate GI/LTF value 0x%x\n",
			   value);
		return -EINVAL;
	}

	he_ar_ltf = value & IEEE80211_HE_AR_LTF_MASK;
	he_ar_sgi = value & IEEE80211_HE_AR_SGI_MASK;
	if (!(he_ar_ltf | he_ar_sgi)) {
		ath12k_err(arvif->ar->ab,
			   "HE autorate GI and LTF masks cannot both be zero\n");
		return -EINVAL;
	}

	if (he_ar_ltf)
		arvif->vap_cfg.he_ar_gi_ltf =
			(arvif->vap_cfg.he_ar_gi_ltf & ~IEEE80211_HE_AR_LTF_MASK) |
			he_ar_ltf;

	if (he_ar_sgi)
		arvif->vap_cfg.he_ar_gi_ltf =
			(arvif->vap_cfg.he_ar_gi_ltf & ~IEEE80211_HE_AR_SGI_MASK) |
			he_ar_sgi;

	return ath12k_vendor_send_he_ar_cfg(arvif);
}

static int ath12k_vendor_set_he_ar_ldpc(struct ath12k_link_vif *arvif,
					       u32 value)
{
	int ret;

	ret = ath12k_vendor_validate_he_eht_mode(arvif, false);
	if (ret)
		return ret;

	if (value > 1) {
		ath12k_err(arvif->ar->ab, "HE autorate LDPC should be 0 or 1\n");
		return -EINVAL;
	}

	arvif->vap_cfg.he_ar_ldpc = value;

	return ath12k_vendor_send_he_ar_cfg(arvif);
}

static int ath12k_vendor_set_he_rts_threshold(struct ath12k_link_vif *arvif,
						     u32 value)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_bss_conf *bss_conf;
	u32 heop = 0;
	int ret;

	ret = ath12k_vendor_validate_he_eht_mode(arvif, false);
	if (ret)
		return ret;

	if (value > IEEE80211_HEOP_RTS_THRESHOLD_DISABLED) {
		ath12k_err(ar->ab,
			   "HE RTS threshold should be less than or equal to 0x%x\n",
			   IEEE80211_HEOP_RTS_THRESHOLD_DISABLED);
		return -EINVAL;
	}

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (bss_conf)
		heop = bss_conf->he_oper.params;

	heop &= ~IEEE80211_HE_OPERATION_RTS_THRESHOLD_MASK;
	heop |= (value << IEEE80211_HE_OPERATION_RTS_THRESHOLD_OFFSET) &
		IEEE80211_HE_OPERATION_RTS_THRESHOLD_MASK;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_HEOPS_0_31, heop);
	if (ret) {
		ath12k_err(ar->ab, "Failed to send WMI_VDEV_PARAM_HEOPS_0_31 to firmware\n");
		return ret;
	}

	arvif->vap_cfg.he_rtsthrshld = value;

	return 0;
}

static void ath12k_wsi_bypass_workfn(struct work_struct *work)
{
	struct ath12k_wsi_remap_work *w =
	    container_of(work, struct ath12k_wsi_remap_work, work);
	struct ath12k_base *ab = w->ab;
	int ret;

	ret = ath12k_mac_dynamic_wsi_remap(ab);
	if (ret) {
	        struct ath12k_hw_group *ag = ab->ag;
	        if (ag)
	                ag->wsi_remap_in_progress = false;
	        ab->wsi_remap_state = ATH12K_WSI_BYPASS_DEFAULT;
	        ab->is_bypassed = false;
	        ath12k_err(ab, "WSI bypass worker failed: %d\n", ret);
	}

	ath12k_wsi_bypass_event(ab, ret);
	kfree(w);
}

/* Set link-vif level parameters
 * set 'reload' to true to send reload event to userspace */
static int ath12k_vendor_set_arvif_params(struct ath12k_link_vif *arvif, u32 param,
                                          u32 value, bool *reload, u32 flags,
					  void *data, u32 data_len)
{
	struct ath12k *ar = arvif->ar;
	int ret = -1;
	bool old_ofdma_val;

	switch (param) {
        case QCA_WLAN_VENDOR_VDEV_PARAM_TEST_RELOAD:
                *reload = true;
                ret = 0;
                break;
	case PARAM_RADIO_TXCHAINSOFT:
		ret = ath12k_set_chainmask(ar, value, true);
		break;
   	case ACFG_PARAM_RADIO_RXCHAINMASK:
		ret = ath12k_set_chainmask(ar, value, false);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_DYN_BW_RTS:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_DISABLE_DYN_BW_RTS, value);
		if (!ret ) {
			arvif->vap_cfg.dyn_bw_rts = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_DISABLE_DYN_BW_RTS to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CWM_ENABLE:
		if (value >= 0) {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_PDEV_PARAM_DYNAMIC_BW, value);
			if (!ret ) {
				arvif->vap_cfg.cwm_enable = value;
			} else {
				ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_DYNAMIC_BW to firmware");
			}
		} else {
			ath12k_err(NULL, "Enable Channel width management with a value of 0 or more.");
			ret = -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RATE_DROPDOWN:
#define RATE_DROPDOWN_LIMIT 7 /* Maximum Value for Rate Drop Down Logic */
		if ((value >= 0) && (value <= RATE_DROPDOWN_LIMIT)) {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_VDEV_PARAM_RATE_DROPDOWN_BMAP, value);
			if (!ret ) {
				arvif->vap_cfg.rate_dropdown = value;
			} else {
				ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_RATE_DROPDOWN_BMAP to firmware");
			}
		} else {
			ath12k_err(NULL, "Rate Control Logic is [0-7]");
			ret = -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CTSPROT_DTIM_BCN:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_DTIM_ENABLE_CTS, value);
		if (!ret ) {
			arvif->vap_cfg.cts_dtim_bcn = !!value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_DTIM_ENABLE_CTS to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CABQ_MAXDUR:
		if (value > 0 && value < 100) {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_VDEV_PARAM_CABQ_MAXDUR, value);
			if (!ret ) {
				arvif->vap_cfg.cabq_maxdur = value;
			} else {
				ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_CABQ_MAXDUR to firmware");
			}
		} else {
			ath12k_err(NULL, "Percentage should be between 0 and 100.");
			ret = -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_MCAST_RC_STALE_PERIOD:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_MCAST_RC_STALE_PERIOD, value);
		if (!ret ) {
			arvif->vap_cfg.mcast_rc_stale_period = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_MCAST_RC_STALE_PERIOD to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_ENABLE_MCAST_RC:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_ENABLE_MCAST_RC, value);
		if (!ret ) {
			arvif->vap_cfg.mcast_rc = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_ENABLE_MCAST_RC to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RC_NUM_RETRIES:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_RC_NUM_RETRIES, value);
		if (!ret ) {
			arvif->vap_cfg.rc_num_retries = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_RC_NUM_RETRIES to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_DISABLE_CABQ:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_DISABLE_CABQ, value);
		if (!ret ) {
			arvif->vap_cfg.disable_cabq = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_DISABLE_CABQ to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_SOUNDING_MODE:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_SET_HE_SOUNDING_MODE, value);
		if (!ret ) {
			arvif->vap_cfg.he_snd_mode = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_SET_HE_SOUNDING_MODE to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_MAX_MTU_SIZE:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_MAX_MTU_SIZE, value);
		if (!ret ) {
			arvif->vap_cfg.max_mtu_size = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_MAX_MTU_SIZE to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_GTX_ENABLE:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_GTX_ENABLE, value);
		if (!ret ) {
			arvif->vap_cfg.gtx_enable = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_GTX_ENABLE to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HWCTS2SELF_OFDMA:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "Invalid value, it should be 0 or 1.");
		} else {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_VDEV_PARAM_HWCTS2SELF_OFDMA, value);
			if (!ret ) {
				arvif->vap_cfg.hwcts2self_ofdma = value;
			} else {
				ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_HWCTS2SELF_OFDMA to firmware");
			}
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_BCN_TX_POWER:
		if ((s32)value < 0 || (s32)value > 255) {
			ath12k_warn(ar->ab,
				    "Ignoring invalid BCN tx power %d (valid range 0..255)\n",
				    (s32)value);
			ret = -EINVAL;
			break;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_MGMT_TX_POWER, value);
		if (!ret) {
			arvif->vap_cfg.bcn_tx_power = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_MGMT_TX_POWER to firmware");
		}
		break;
        case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MIN_THRESH:
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MAX_THRESH:
	case QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MIN_THRESH:
	case QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MAX_THRESH:
		if ((int32_t)value > 0 || (int32_t)value < -100) {
			ath12k_err(NULL, "Invalid RSSI/ACKRSSI threshold value %d. Valid range: -100 to 0 dBm\n",
				   (int32_t)value);
			ret = -EINVAL;
		} else {
			if (param == QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MIN_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_RSSI_MIN, value);
			else if (param == QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MAX_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_RSSI_MAX, value);
			else if (param == QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MIN_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_ACKRSSI_MIN, value);
			else if (param == QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MAX_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_ACKRSSI_MAX, value);

			if (ret)
				ath12k_err(NULL, "Failed to set RSSI/ACKRSSI threshold, param: %d\n", param);
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MIN_THRESH:
	case QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MAX_THRESH:
	case QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MIN_THRESH:
	case QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MAX_THRESH:
		if ((int32_t)value < 0) {
			ath12k_err(NULL, "Invalid rate threshold value %d. Rate must be positive\n",
				   (int32_t)value);
			ret = -EINVAL;
		} else {
			if (param == QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MIN_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_TXRATE_MIN, value);
			else if (param == QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MAX_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_TXRATE_MAX, value);
			else if (param == QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MIN_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_RXRATE_MIN, value);
			else if (param == QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MAX_THRESH)
				ret = ath12k_telemetry_set_threshold(THRESHOLD_RXRATE_MAX, value);

			if (ret)
				ath12k_err(NULL, "Failed to set rate threshold, param: %d\n", param);
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_RATE_BREACH_MASK:
		ret = ath12k_telemetry_set_breach_mask(value);
		if (ret)
			ath12k_err(NULL, "Failed to set RSSI/Rate breach mask\n");
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_HYSTERESIS:
		ret = ath12k_telemetry_set_hysteresis(HYSTERESIS_TYPE_RSSI, value);
		if (ret)
			ath12k_err(NULL, "Failed to set RSSI hysteresis\n");
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RATE_HYSTERESIS:
		ret = ath12k_telemetry_set_hysteresis(HYSTERESIS_TYPE_RATE, value);
		if (ret)
			ath12k_err(NULL, "Failed to set rate hysteresis\n");
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_AMPDU:
	{
		u32 ampdu_max_size;
		bool is_be_vap;

		is_be_vap = ath12k_vendor_is_be_vap(arvif);
		ampdu_max_size = is_be_vap ?
				 (ATH12K_CONFIG_AGGR_MAX_AMPDU_SIZE_BE + 1) :
				 (ATH12K_CONFIG_AGGR_MAX_AMPDU_SIZE + 1);

		if (value > ampdu_max_size) {
			ath12k_err(NULL, "Valid AMPDU Aggregation Size for %s VAP is in the range 0-%u",
				   is_be_vap ? "BE" : "non-BE", ampdu_max_size);
			ret = -EINVAL;
			break;
		}

		ret = ath12k_set_ampdu_aggr_size_arvif(arvif, value);
		if (ret) {
			ath12k_err(NULL,
				   "Failed to send WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU to firmware");
			break;
		}

		arvif->vap_cfg.ampdu_aggr_size = value;
		*reload = true;
		break;
	}

	case QCA_WLAN_VENDOR_VDEV_PARAM_AMSDU:
		if (value > ATH12K_CONFIG_AGGR_MAX_AMSDU_SIZE) {
			ath12k_err(NULL, "Valid AMPDU Aggregation Size is in the range 0-7");
			return -EINVAL;
		} else {
			ret = ath12k_set_amsdu_aggr_size_arvif(arvif, value);
			if (!ret) {
				arvif->vap_cfg.amsdu_aggr_size = value;
			} else {
				ath12k_err(NULL,
						"Failed to send WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU to firmware");
			}
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_ENABLE_RX_AMSDU:
		{
			struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
			struct ieee80211_sub_if_data *sdata;
			struct ieee80211_link_data *link;

			if (!vif) {
				ath12k_err(NULL, "enable_rx_amsdu: vif is NULL\n");
				return -EINVAL;
			}
			sdata = vif_to_sdata(vif);
			if (!sdata) {
				ath12k_err(NULL, "enable_rx_amsdu: sdata is NULL\n");
				return -EINVAL;
			}
			if (value >= IEEE80211_FIRST_TSPEC_TSID) {
				ath12k_err(NULL, "enable_rx_amsdu: invalid TID %u (max %u)\n",
					   value, IEEE80211_FIRST_TSPEC_TSID - 1);
				return -EINVAL;
			}

			rcu_read_lock();
			link = ieee80211_get_rx_amsdu_link(sdata, arvif->link_id);
			if (!link) {
				rcu_read_unlock();
				ath12k_err(NULL, "enable_rx_amsdu: link %u is NULL\n",
					   arvif->link_id);
				return -EINVAL;
			}
			link->rx_amsdu_tid_bitmap |= BIT(value);
			rcu_read_unlock();

			ret = 0;
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_DISABLE_RX_AMSDU:
		{
			struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
			struct ieee80211_sub_if_data *sdata;
			struct ieee80211_link_data *link;

			if (!vif) {
				ath12k_err(NULL, "disable_rx_amsdu: vif is NULL\n");
				return -EINVAL;
			}
			sdata = vif_to_sdata(vif);
			if (!sdata) {
				ath12k_err(NULL, "disable_rx_amsdu: sdata is NULL\n");
				return -EINVAL;
			}
			/* Only TIDs 0-7 are standard data TIDs (< IEEE80211_FIRST_TSPEC_TSID = 8).
			 * rx_amsdu_tid_bitmap is a u8, so bits 0-7 only.
			 */
			if (value >= IEEE80211_FIRST_TSPEC_TSID) {
				ath12k_err(NULL, "disable_rx_amsdu: invalid TID %u (max %u)\n",
					   value, IEEE80211_FIRST_TSPEC_TSID - 1);
				return -EINVAL;
			}

			rcu_read_lock();
			link = ieee80211_get_rx_amsdu_link(sdata, arvif->link_id);
			if (!link) {
				rcu_read_unlock();
				ath12k_err(NULL, "disable_rx_amsdu: link %u is NULL\n",
					   arvif->link_id);
				return -EINVAL;
			}
			link->rx_amsdu_tid_bitmap &= ~BIT(value);
			rcu_read_unlock();

			ret = 0;
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_BA_BUFSIZE:
		if (value > ATH12K_CONFIG_MAX_BA_BUFSIZE) {
			ath12k_err(NULL, "Max buffer size is %d\n", ATH12K_CONFIG_MAX_BA_BUFSIZE);
			ret = -EINVAL;
		} else {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_VDEV_PARAM_BA_MODE,
					value);
			if (!ret) {
				arvif->vap_cfg.ba_bufsize = value;
			} else {
				ath12k_err(NULL,
						"Failed to send WMI_VDEV_PARAM_BA_MODE to firmware\n");
			}
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_TX_ENCAP_TYPE:
		if (value > ATH12K_CONFIG_MAX_TX_ENCAP_TYPE) {
			ath12k_err(NULL, "Value out of range\n");
			ret = -EINVAL;
		} else {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_VDEV_PARAM_TX_ENCAP_TYPE,
					value);
			if (!ret) {
				arvif->ahvif->dp_vif.tx_encap_type = value;
			} else {
				ath12k_err(NULL,
						"Failed to send WMI_VDEV_PARAM_TX_ENCAP_TYPE to firmware\n");
			}
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_RX_DECAP_TYPE:
		if (value > ATH12K_CONFIG_MAX_RX_DECAP_TYPE) {
			ath12k_err(NULL, "Value out of range\n");
			ret = -EINVAL;
		} else {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					WMI_VDEV_PARAM_RX_DECAP_TYPE,
					value);
			if (!ret) {
				arvif->ahvif->dp_vif.rx_decap_type = value;
			} else {
				ath12k_err(NULL,
						"Failed to send WMI_VDEV_PARAM_RX_DECAP_TYPE to firmware\n");
			}
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_PROTECTION_MODE:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_PROTECTION_MODE, value);
		if (!ret) {
			arvif->vap_cfg.protection_mode = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_PROTECTION_MODE to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_MCAST_RATE:
		ret = ath12k_set_vdev_mcast_rate(arvif, value);
		if (ret)
		    ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_MCAST_DATA_RATE to firmware\n");
		break;
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	case QCA_WLAN_VENDOR_VDEV_PARAM_BCAST_RATE:
		ret = ath12k_set_vdev_bcast_rate(arvif, value);
		if (ret)
		    ath12k_err(NULL, "Failed to set bcast rate\n");
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_PN_MGMT_RX_FILTER:
		ret = ath12k_wmi_vdev_set_pn_mgmt_rx_filter_cmd(ar, arvif->vdev_id, value);
		if (ret)
			ath12k_err(NULL, "Failed to set PN RX Filter");
		break;
	case QCA_WLAN_VENDOR_VDEV_MESH_MODE_HDR:
		ret = ath12k_mac_vif_setmhdr(arvif, value);
		if (ret) {
			ath12k_err(ar->ab, "failed to set mesh hdr 0x%x for vdev %d: %d\n",
				   value, arvif->vdev_id, ret);
			return ret;
		}
		ath12k_err(ar->ab,
			   "Vendor cmd: Set mesh hdr to 0x%x for vdev %d\n Success",
			   value, arvif->vdev_id);
	break;
	case QCA_WLAN_VENDOR_VDEV_MESH_MODE_DBG:
		ret = ath12k_mac_vif_set_mesh_dbg(arvif, value);
		if (ret) {
			ath12k_err(ar->ab, "failed to set mesh dbg %d for vdev %d: %d\n",
				   value, arvif->vdev_id, ret);
			return ret;
		}
		ath12k_err(ar->ab,
			   "Vendor cmd: Set mesh dbg to %d for vdev %d\n Success",
			   value, arvif->vdev_id);
	break;
	case QCA_WLAN_VENDOR_VDEV_RX_FILTER:
		ret = ath12k_mac_vif_set_mesh_rx_filter(arvif, value);
		if (ret) {
			ath12k_err(ar->ab, "failed to set mesh rx_filter 0x%x for vdev %d: %d\n",
				   value, arvif->vdev_id, ret);
			return ret;
		}
		ath12k_err(ar->ab,
			   "Vendor cmd: Set mesh rx_filter to 0x%x for vdev %d\n Success",
			   value, arvif->vdev_id);
	break;
	case QCA_WLAN_VENDOR_VDEV_TX_MESH:
		ret = ath12k_mac_vif_set_mesh_tx(arvif, value);
		if (ret) {
			ath12k_err(ar->ab, "failed to set mesh tx_%d for vdev %d: %d\n",
				   value, arvif->vdev_id, ret);
			return ret;
		}
		ath12k_err(ar->ab,
			   "Vendor cmd: Set mesh tx to %d for vdev %d\n Success",
			   value, arvif->vdev_id);
	break;
#endif
	case QCA_WLAN_VENDOR_VDEV_PARAM_BW_NSS_RATE:
		u_int32_t bw, nss, ratemask, max_bw;

		bw = (value & 0xff000000) >> 24;
		nss = (value & 0x00ff0000) >> 16;
		ratemask = (value & 0xffff);
		max_bw = 6;

		if (bw > max_bw) {
			ath12k_err(NULL, "Not yet supported value for Band Width: %d\n", bw);
			return -EINVAL;
		} else if ((bw <= max_bw) && (bw >= 3) && (ratemask > 0x3ff)) {
			ath12k_err(NULL, "Invalid ratemask for VHT Band: 0x%x\n", ratemask);
			return -EINVAL;
		} else if ((bw <= 2) && (bw >= 1) && (ratemask > 0xff)) {
			ath12k_err(NULL, "Invalid ratemask for HT Band: 0x%x\n", ratemask);
			return -EINVAL;
		} else if ((bw <= max_bw) && (bw >= 1) && ((nss > 4) || (nss == 0)))  {
			ath12k_err(NULL, "Invalid nss for VHT/HT Band: %d\n", nss);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_BW_NSS_RATEMASK, value);
		if (!ret) {
			arvif->vap_cfg.bw_nss_rate = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_BW_NSS_RATEMASK to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CHWIDTH:
		if (value > WMI_HOST_CHAN_WIDTH_MAX) {
			ath12k_err(ar->ab, "Invalid channel width value %u\n", value);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_CHWIDTH, value);
		if (!ret) {
			arvif->vap_cfg.chwidth = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_CHWIDTH to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_NSS:
		{
			u8 nss;

			ret = ath12k_vendor_resolve_nss(arvif, value, &nss);
			if (ret)
				return ret;

			value = nss;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_NSS, value);
		if (!ret) {
			arvif->vap_cfg.nss = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_NSS to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_RATE:
		ret = ath12k_vendor_set_fixed_rate(arvif, value,
						   ATH12K_VENDOR_FIXED_RATE_LEGACY_HT);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_VHT_MCS:
		ret = ath12k_vendor_set_fixed_rate(arvif, value,
						   ATH12K_VENDOR_FIXED_RATE_VHT_MCS);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_HE_MCS:
		ret = ath12k_vendor_set_fixed_rate(arvif, value,
						   ATH12K_VENDOR_FIXED_RATE_HE_MCS);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_EHT_MCS:
		ret = ath12k_vendor_set_fixed_rate(arvif, value,
						   ATH12K_VENDOR_FIXED_RATE_EHT_MCS);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_UL_FIXED_RATE:
		ret = ath12k_vendor_set_ul_fixed_rate(arvif, value,
						      ATH12K_VENDOR_UL_FIXED_RATE_HE);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_FIXED_RATE:
		ret = ath12k_vendor_set_ul_fixed_rate(arvif, value,
						      ATH12K_VENDOR_UL_FIXED_RATE_EHT);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_MCS:
		ret = ath12k_vendor_set_ul_fixed_rate(arvif, value,
						      ATH12K_VENDOR_UL_FIXED_RATE_HE);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_MCS:
		ret = ath12k_vendor_set_ul_fixed_rate(arvif, value,
						      ATH12K_VENDOR_UL_FIXED_RATE_EHT);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_LTF:
		ret = ath12k_vendor_set_he_ltf(arvif, value, true);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_LTF:
		ret = ath12k_vendor_set_he_ltf(arvif, value, false);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_AR_GI_LTF:
		ret = ath12k_vendor_set_he_ar_gi_ltf(arvif, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_AR_LDPC:
		ret = ath12k_vendor_set_he_ar_ldpc(arvif, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_RTSTHRSHLD:
		ret = ath12k_vendor_set_he_rts_threshold(arvif, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_LDPC:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "EHT UL LDPC setting should either 0 or 1.\n");
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.eht_ul_ldpc ) {
			ath12k_err(NULL, "value %d is already set for iv_eht_ul_ldpc", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_LDPC, value | WMI_HOST_VDEV_PARAM_EHT_MODE);
		if (!ret) {
			arvif->vap_cfg.eht_ul_ldpc = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_LDPC to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_LTF:
		if (value > IEEE80211_EHT_LTF_4X) {
			ath12k_err(NULL, "LTF value should be less than or equal to 0x%x\n", IEEE80211_EHT_LTF_4X);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_EHT_LTF, value);
		if (!ret) {
			arvif->vap_cfg.eht_ul_ltf = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_EHT_LTF to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_LTF:
		if (value > IEEE80211_EHT_LTF_4X) {
			ath12k_err(NULL, "LTF value should be less than or equal to 0x%x\n", IEEE80211_EHT_LTF_4X);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_EHT_LTF, value);
		if (!ret) {
			arvif->vap_cfg.eht_ltf = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_EHT_LTF to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EXTRA_EHT_LTF:
		ret = ath12k_wmi_vdev_set_param_cmd(ar,
						    arvif->vdev_id,
						    WMI_VDEV_PARAM_EXTRA_EHT_LTF,
						    value);
		if (!ret)
			arvif->vap_cfg.extra_eht_ltf = value;
		else
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_EXTRA_EHT_LTF to firmware");
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_BA_MODE:
		if (value > WMI_BA_MODE_BUFFER_SIZE_1024) {
			ath12k_err(NULL, "BA mode value should be between 0 and %d\n",
				   WMI_BA_MODE_BUFFER_SIZE_1024);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    WMI_VDEV_PARAM_BA_MODE, value);
		if (!ret)
			arvif->vap_cfg.ba_mode = value;
		else
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_BA_MODE to firmware");
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_NSS:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_NSS, value | WMI_HOST_VDEV_PARAM_EHT_MODE);
		if (!ret) {
			arvif->vap_cfg.eht_ul_nss = value;
			if (!(arvif->vap_cfg.eht_ul_ldpc) && (arvif->vap_cfg.eht_ul_nss > 4)) {
				arvif->vap_cfg.eht_ul_ldpc = 1;
			}
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_NSS to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_PPDU_BW:
		if (value > IEEE80211_EHT_BW_IDX_MAX) {
			ath12k_err(NULL, "Channel width value should be less than IEEE80211_EHT_BW_IDX_MAX %d", IEEE80211_EHT_BW_IDX_MAX);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_PPDU_BW, value | WMI_HOST_VDEV_PARAM_EHT_MODE);
		if (!ret) {
			arvif->vap_cfg.eht_ul_ppdu_bw = value;
			if (!(arvif->vap_cfg.eht_ul_ldpc) && (arvif->vap_cfg.eht_ul_ppdu_bw > 0)) {
				arvif->vap_cfg.eht_ul_ldpc = 1;
			}
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_PPDU_BW to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_SHORTGI:
		if ((value == IEEE80211_GI_0DOT4_US) || (value > IEEE80211_GI_3DOT2_US)) {
			ath12k_err(NULL, "%d not supported setting for EHT UL Shortgi\n", IEEE80211_GI_0DOT4_US);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_GI, value | WMI_HOST_VDEV_PARAM_EHT_MODE);
		if (!ret) {
			arvif->vap_cfg.eht_ul_shortgi = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_GI to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_STBC:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "EHT UL STBC setting should either 0 or 1.\n");
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.eht_ul_stbc) {
			ath12k_err(NULL, "value %d is already set for iv_eht_ul_stbc", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_STBC, value | WMI_HOST_VDEV_PARAM_EHT_MODE);
		if (!ret) {
			arvif->vap_cfg.eht_ul_stbc = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_STBC to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_ENABLERTSCTS:
		if (((value & (~0xff)) != 0) ||
		    (((value & IEEE80211_RTS_CTS_MASK) == 0) &&
		     (((value & IEEE80211_RTS_CTS_PROFILE_MASK) >> IEEE80211_RTS_CTS_PROFILE_SHIFT) != 0)) ||
		    (((value & IEEE80211_RTS_CTS_MASK) != 0) &&
		     (((value & IEEE80211_RTS_CTS_PROFILE_MASK) >> IEEE80211_RTS_CTS_PROFILE_SHIFT) == 0))) {
			ath12k_err(NULL, "Invalid value for RTS-CTS: %x\n", value);
			return -EINVAL;
		}
		if (((value & IEEE80211_RTS_CTS_MASK) > IEEE80211_RTSCTS_FLAG_MAX_VALUE) ||
		    (((value & IEEE80211_RTS_CTS_PROFILE_MASK) >> IEEE80211_RTS_CTS_PROFILE_SHIFT) > IEEE80211_RTSCTS_PROFILE_MAX_VALUE)) {
			ath12k_err(NULL, "Not yet supported value for RTS-CTS: %x\n", value);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_ENABLE_RTSCTS, value);
		if (!ret) {
			arvif->vap_cfg.enablertscts = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_ENABLE_RTSCTS to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_DCM:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "HE DCM setting should be 0 or 1 ");
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.he_dcm) {
			ath12k_err(NULL, "value %d is already set for he_dcm", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_HE_DCM, value);
		if (!ret) {
			arvif->vap_cfg.he_dcm = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_HE_DCM to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_EXTRANGE:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "HE Extended Range setting should be 0 or 1 ");
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.he_extrange) {
			ath12k_err(NULL, "value %d is already set for he_extrange", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_HE_RANGE_EXT, value);
		if (!ret) {
			arvif->vap_cfg.he_extrange = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_HE_RANGE_EXT to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_LDPC:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "UL LDPC setting should either 0 or 1.\n");
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.he_ul_ldpc) {
			ath12k_err(NULL, "value %d is already set for he_ul_ldpc", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_LDPC, value);
		if (!ret) {
			arvif->vap_cfg.he_ul_ldpc = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_LDPC to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_NSS:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_NSS, value);
		if (!ret) {
			arvif->vap_cfg.he_ul_nss = value;
			if (!(arvif->vap_cfg.he_ul_ldpc) && (arvif->vap_cfg.he_ul_nss > 4)) {
				arvif->vap_cfg.he_ul_ldpc = 1;
			}
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_NSS to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_PPDU_BW:
		if (value >  IEEE80211_HE_BW_IDX_MAX) {
			ath12k_err(NULL, "Channel width value should be less than 3.\n");
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_PPDU_BW, value);
		if (!ret) {
			arvif->vap_cfg.he_ul_ppdu_bw = value;
			if (!(arvif->vap_cfg.he_ul_ldpc) && (arvif->vap_cfg.he_ul_ppdu_bw > 0)) {
				arvif->vap_cfg.he_ul_ldpc = 1;
			}
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_PPDU_BW to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_SHORTGI:
		if ((value == IEEE80211_GI_0DOT4_US) ||
		    (value > IEEE80211_GI_3DOT2_US)) {
			ath12k_err(NULL, "%d not supported setting for HE UL Shortgi\n", IEEE80211_GI_0DOT4_US);
			return -EINVAL;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_GI, value);
		if (!ret) {
			arvif->vap_cfg.he_ul_shortgi = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_GI to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_STBC:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "UL STBC setting should either 0 or 1.\n");
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.he_ul_stbc) {
			ath12k_err(NULL, "value %d is already set for he_ul_stbc", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_UL_STBC, value);
		if (!ret) {
			arvif->vap_cfg.he_ul_stbc = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_UL_STBC to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RTSCTS_RATE:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_RTSCTS_RATE, value);
		if (!ret) {
			arvif->vap_cfg.rtscts_rate = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_RTSCTS_RATE to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT_SGIMASK:
		if (value > MAX_VHT_SGI_MASK) {
			ath12k_err(NULL, "value can not be more than %d", MAX_VHT_SGI_MASK);
			return -EINVAL;
		}
		if (value == arvif->vap_cfg.vht_sgimask) {
			ath12k_err(NULL, "vht_sgimask %d is already set", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_VHT_SGIMASK, value);
		if (!ret) {
			arvif->vap_cfg.vht_sgimask = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_VHT_SGIMASK to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT80_RATE:
		if (value > MAX_VHT80_RATE_MASK) {
			ath12k_err(NULL, "value can not be more than %d", MAX_VHT80_RATE_MASK);
			return -EINVAL;
		}
		if ((value & MAX_VHT80_RATE_MASK) == arvif->vap_cfg.vht80_rate) {
			ath12k_err(NULL, "vht80_ratemask %d already set", value);
			return 0;
		}
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_VHT80_RATEMASK, value);
		if (!ret) {
			arvif->vap_cfg.vht80_rate = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_VHT80_RATEMASK to firmware");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_DIS_LPI_ANT_OPTIMIZE:
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
				WMI_VDEV_PARAM_DISABLE_LPI_ANT_OPTIMIZATION, value);
		if (!ret) {
			arvif->vap_cfg.dis_lpi_ant_optimize = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_VDEV_PARAM_DISABLE_LPI_ANT_OPTIMIZATION to firmware");
		}
		break;
	/* Per-VAP EHT OFDMA/TXBF per-link overrides */
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_DL_OFDMA_TXBF:
		if (arvif->ar && arvif->is_created) {
			old_ofdma_val = arvif->vap_cfg.eht_dl_ofdma_txbf;
			arvif->vap_cfg.eht_dl_ofdma_txbf_configured = true;
			arvif->vap_cfg.eht_dl_ofdma_txbf = !!value;
			ret = ath12k_mac_set_eht_txbf_conf(arvif, NULL, false);
			if (ret) {
				arvif->vap_cfg.eht_dl_ofdma_txbf = old_ofdma_val;
				arvif->vap_cfg.eht_dl_ofdma_txbf_configured = false;
				ath12k_err(NULL, "Failed to set EHT_DL_OFDMA_TXBF");
			}
		} else {
			arvif->vap_cfg.eht_dl_ofdma_txbf_configured = true;
			arvif->vap_cfg.eht_dl_ofdma_txbf = !!value;
			ret = 0;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_DL_OFDMA:
		if (arvif->ar && arvif->is_created) {
			old_ofdma_val = arvif->vap_cfg.eht_dl_ofdma;
			arvif->vap_cfg.eht_dl_ofdma_configured = true;
			arvif->vap_cfg.eht_dl_ofdma = !!value;
			ret = ath12k_mac_set_eht_txbf_conf(arvif, NULL, false);
			if (ret) {
				arvif->vap_cfg.eht_dl_ofdma = old_ofdma_val;
				arvif->vap_cfg.eht_dl_ofdma_configured = false;
				ath12k_err(NULL, "Failed to set EHT_DL_OFDMA");
			}
		} else {
			arvif->vap_cfg.eht_dl_ofdma_configured = true;
			arvif->vap_cfg.eht_dl_ofdma = !!value;
			ret = 0;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_OFDMA:
		if (arvif->ar && arvif->is_created) {
			old_ofdma_val = arvif->vap_cfg.eht_ul_ofdma;
			arvif->vap_cfg.eht_ul_ofdma_configured = true;
			arvif->vap_cfg.eht_ul_ofdma = !!value;
			ret = ath12k_mac_set_eht_txbf_conf(arvif, NULL, false);
			if (ret) {
				arvif->vap_cfg.eht_ul_ofdma = old_ofdma_val;
				arvif->vap_cfg.eht_ul_ofdma_configured = false;
				ath12k_err(NULL, "Failed to set EHT_UL_OFDMA");
			}
		} else {
			arvif->vap_cfg.eht_ul_ofdma_configured = true;
			arvif->vap_cfg.eht_ul_ofdma = !!value;
			ret = 0;
		}
		break;
		/* Per-VAP HE OFDMA/TXBF per-link overrides */
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_DL_OFDMA_TXBF:
		if (arvif->ar && arvif->is_created) {
			old_ofdma_val = arvif->vap_cfg.he_dl_ofdma_txbf;
			arvif->vap_cfg.he_dl_ofdma_txbf_configured = true;
			arvif->vap_cfg.he_dl_ofdma_txbf = !!value;
			ret = ath12k_mac_set_he_txbf_conf(arvif, NULL, false);
			if (ret) {
				arvif->vap_cfg.he_dl_ofdma_txbf = old_ofdma_val;
				arvif->vap_cfg.he_dl_ofdma_txbf_configured = false;
				ath12k_err(NULL, "Failed to set HE_DL_OFDMA_TXBF");
			}
		} else {
			arvif->vap_cfg.he_dl_ofdma_txbf_configured = true;
			arvif->vap_cfg.he_dl_ofdma_txbf = !!value;
			ret = 0;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_DL_OFDMA:
		if (arvif->ar && arvif->is_created) {
			old_ofdma_val = arvif->vap_cfg.he_dl_ofdma;
			arvif->vap_cfg.he_dl_ofdma_configured = true;
			arvif->vap_cfg.he_dl_ofdma = !!value;
			ret = ath12k_mac_set_he_txbf_conf(arvif, NULL, false);
			if (ret) {
				arvif->vap_cfg.he_dl_ofdma = old_ofdma_val;
				arvif->vap_cfg.he_dl_ofdma_configured = false;
				ath12k_err(NULL, "Failed to set HE_DL_OFDMA");
			}
		} else {
			arvif->vap_cfg.he_dl_ofdma_configured = true;
			arvif->vap_cfg.he_dl_ofdma = !!value;
			ret = 0;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_OFDMA:
		if (arvif->ar && arvif->is_created) {
			old_ofdma_val = arvif->vap_cfg.he_ul_ofdma;
			arvif->vap_cfg.he_ul_ofdma_configured = true;
			arvif->vap_cfg.he_ul_ofdma = !!value;
			ret = ath12k_mac_set_he_txbf_conf(arvif, NULL, false);
			if (ret) {
				arvif->vap_cfg.he_ul_ofdma = old_ofdma_val;
				arvif->vap_cfg.he_ul_ofdma_configured = false;
				ath12k_err(NULL, "Failed to set HE_UL_OFDMA");
			}
		} else {
			arvif->vap_cfg.he_ul_ofdma_configured = true;
			arvif->vap_cfg.he_ul_ofdma = !!value;
			ret = 0;
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_TLV_LOGGER_MODE: {
		u8 tlv_logger_mode = (u8)value;
		struct ath12k_pdev_dp *dp_pdev;
		struct ath12k_pdev_mon_dp_extn *mon_extn = NULL;

		if (arvif->ahvif->vif->type != NL80211_IFTYPE_MONITOR) {
			ath12k_err(NULL, "requested interface is not a monitor interface!\n");
			return -EINVAL;
		}
		dp_pdev = &ar->dp;
		if (dp_pdev->dp_mon_pdev)
			mon_extn = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;

		if (tlv_logger_mode != ATH12K_TLV_LOGGER_TX_ENABLED &&
		    (!mon_extn || !mon_extn->rx_tlv_log))
			ath12k_info(NULL, "RX Tlv logger not initialized please check hw param");
		if (tlv_logger_mode != ATH12K_TLV_LOGGER_RX_ENABLED
		    && (!mon_extn || !mon_extn->tx_tlv_log))
			ath12k_info(NULL, "TX Tlv logger not initialized please check hw param");

		if (tlv_logger_mode == ATH12K_TLV_LOGGER_DISABLED) {
			if (mon_extn && mon_extn->rx_tlv_log)
				mon_extn->rx_tlv_log->tlv_logging_enable = 0;
			if (mon_extn && mon_extn->tx_tlv_log)
				mon_extn->tx_tlv_log->tlv_logging_enable = 0;
		}
		if (mon_extn && mon_extn->rx_tlv_log) {
			if (tlv_logger_mode & ATH12K_TLV_LOGGER_RX_ENABLED)
				mon_extn->rx_tlv_log->tlv_logging_enable = 1;
			else
				mon_extn->rx_tlv_log->tlv_logging_enable = 0;
		}
		if (mon_extn && mon_extn->tx_tlv_log) {
			if (tlv_logger_mode & ATH12K_TLV_LOGGER_TX_ENABLED)
				mon_extn->tx_tlv_log->tlv_logging_enable = 1;
			else
				mon_extn->tx_tlv_log->tlv_logging_enable = 0;
		}
		ret = 0;
		break;
	}
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT_MCS_10_11_SUPP:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "VHT MCS10/11 support should be 0 or 1\n");
			return -EINVAL;
		}
		if (!!value == arvif->arvif_extn.vht_mcs10_11_supp) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "VHT MCS10/11 support already set to %d\n", value);
			return 0;
		}
		if (value && !ar->pdev->cap.vht_higher_mcs_supp) {
			ath12k_err(NULL, "VHT MCS10/11 not supported by hardware\n");
			return -EOPNOTSUPP;
		}
		arvif->arvif_extn.vht_mcs10_11_supp = !!value;
		/* Note: existing connected peers are not retroactively updated.
		 * The new setting takes effect for subsequent associations.
		 */
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT_MCS_10_11_NQ2Q_PEER_SUPP:
		if (value < 0 || value > 1) {
			ath12k_err(NULL, "VHT MCS10/11 nq2q peer support should be 0 or 1\n");
			return -EINVAL;
		}
		if (!!value == arvif->arvif_extn.vht_mcs10_11_nq2q_peer_supp) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "VHT MCS10/11 nq2q peer support already set to %d\n", value);
			return 0;
		}
		arvif->arvif_extn.vht_mcs10_11_nq2q_peer_supp = !!value;
		ret = 0;
		break;
	/* PHY_ERR_MASK carries a 64-bit value: lo in data, hi in flags.
	 */
	case QCA_WLAN_VENDOR_VDEV_PARAM_UNDECODED_PHY_ERR_MASK: {
		const struct ath12k_dp_arch_mon_ops *mon_ops;
		struct ath12k_pdev_dp *dp_pdev;

		if (!ath12k_scan_radio_supported(ar->pdev)) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "undecoded: not a scan radio, param=%d\n", param);
			return -EINVAL;
		}
		dp_pdev = &ar->dp;
		if (!dp_pdev->dp_mon_pdev)
			return -EINVAL;
		mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
		if (mon_ops && mon_ops->rx_undecoded_phy_err_mask_set)
			ret = mon_ops->rx_undecoded_phy_err_mask_set(dp_pdev,
								value, flags);
		else
			ret = -EOPNOTSUPP;

		break;
	}
	case QCA_WLAN_VENDOR_VDEV_PARAM_UNDECODED_METADATA_CAPTURE: {
		const struct ath12k_dp_arch_mon_ops *mon_ops;
		struct ath12k_pdev_dp *dp_pdev;

		if (!ath12k_scan_radio_supported(ar->pdev)) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "undecoded: not a scan radio, param=%d\n", param);
			return -EINVAL;
		}
		dp_pdev = &ar->dp;
		if (!dp_pdev->dp_mon_pdev)
			return -EINVAL;
		mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
		if (mon_ops && mon_ops->rx_undecoded_metadata_capture_set) {
			ret = mon_ops->rx_undecoded_metadata_capture_set(dp_pdev, value);
			if (!ret)
				ret = ath12k_dp_mon_rx_update_filter(ar);
		} else {
			ret = -EOPNOTSUPP;
		}
		break;
	}
	case QCA_WLAN_VENDOR_VDEV_PARAM_CHMASK_PER_STA: {
		struct ath12k_link_sta *arsta;
		struct chmask_per_sta *chmask;
		u8 max_nss;

		if (!data || data_len != sizeof(struct chmask_per_sta)) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "chmask_per_sta: invalid data or len %u\n", data_len);
			return -EINVAL;
		}

		chmask = (struct chmask_per_sta *)data;
		if (!is_valid_ether_addr(chmask->mac)) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "chmask_per_sta: invalid MAC address\n");
			return -EINVAL;
		}

		max_nss = ath12k_vendor_get_max_tx_nss(ar);
		if (chmask->nss > max_nss) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "chmask_per_sta: nss %u exceeds max %u\n",
				   chmask->nss, max_nss);
			return -EINVAL;
		}

		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr_vdev_id(ar, chmask->mac,
							     arvif->vdev_id);
		spin_unlock_bh(&ar->arsta_lock);
		if (!arsta) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "chmask_per_sta: STA not found\n");
			return -ENOTCONN;
		}
		ret = ath12k_wmi_set_peer_param(ar, chmask->mac, arvif->vdev_id,
						WMI_PEER_NSS, chmask->nss);
		break;
	}
	case QCA_WLAN_VENDOR_VDEV_PARAM_FRAME_INJECTOR_EN: {
		struct ath12k_frame_injector_params *fi;
		struct wmi_frame_inject_arg arg = {};

		if (!data || data_len < sizeof(*fi)) {
			ath12k_err(NULL, "frame_inject: invalid data len %u\n", data_len);
			return -EINVAL;
		}

		fi = (struct ath12k_frame_injector_params *)data;

		if (fi->frametype != WMI_FRAME_INJECT_TYPE_QOS_NULL &&
		    fi->frametype != WMI_FRAME_INJECT_TYPE_CTS_TO_SELF) {
			ath12k_err(NULL, "frame_inject: unsupported frame type %u\n",
				   fi->frametype);
			return -EINVAL;
		}

		if (fi->enable > 1) {
			ath12k_err(NULL, "frame_inject: enable must be 0 or 1\n");
			return -EINVAL;
		}
		if(fi->enable) {
			if (fi->inject_period > arvif->beacon_interval) {
				ath12k_err(NULL,
					   "frame_inject: inject_period %u ms exceeds beacon interval %u ms\n",
					   fi->inject_period, arvif->beacon_interval);
				return -EINVAL;
			}

			if (fi->frame_duration > ATH12K_FRAME_INJECTOR_MAX_DUR) {
				ath12k_err(NULL,
					   "frame_inject: frame_duration %u usecs exceeds max %u\n",
					   fi->frame_duration, ATH12K_FRAME_INJECTOR_MAX_DUR);
				return -EINVAL;
			}
		}
		arg.vdev_id             = arvif->vdev_id;
		arg.enable              = fi->enable;
		arg.frame_type          = fi->frametype;
		if (fi->enable) {
			arg.frame_inject_period = fi->inject_period;
			ether_addr_copy(arg.dstmac, fi->dstmac);
			arg.fc_duration         = fi->frame_duration;
			arg.bw                  = fi->bandwidth;
		}
		ret = ath12k_wmi_send_pdev_frame_inject_cmd(ar, &arg);
		break;
	}
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Un-supported param: %d\n", param);
		break;
	}

	return ret;
}

static inline enum srtype ath12k_extact_sr_type(void *data, uint32_t data_len)
{
	char srtype[SR_TYPE_LENGTH];

	if (!data_len)
		goto err;

	if (data_len > SR_TYPE_LENGTH-1)
		data_len = SR_TYPE_LENGTH-1;

	memcpy(srtype, data, data_len);
	srtype[data_len] = '\0';

	if (strcmp(srtype, "non-srg") == 0)
		return SR_TYPE_NON_SRG_OBSS_PD;
	else if(strcmp(srtype, "srg") == 0)
		return SR_TYPE_SRG_OBSS_PD;
	else if(strcmp(srtype, "obss_pd") == 0)
		return SR_TYPE_OBSS_PD;
	else if(strcmp(srtype, "psr") == 0)
		return SR_TYPE_PSR;

err:
	ath12k_err(NULL, "Wrong Spatial reuse type");
	return SR_TYPE_MAX;
}

static inline int set_obss_pd_enable_bit(uint32_t *threshold,
					 enum srtype type,
					 uint8_t value)
{
	switch (type) {
	case SR_TYPE_NON_SRG_OBSS_PD:
		SET_SELF_OBSS_PD_ENABLE(NON_SRG, *threshold, value);
		break;

	case SR_TYPE_SRG_OBSS_PD:
		SET_SELF_OBSS_PD_ENABLE(SRG, *threshold, value);
		break;

	default:
		ath12k_err(NULL, "Invalid Spatial Reuse type");
		return -1;
	}

	return 0;
}

static inline int set_obss_pd_threshold(uint32_t *threshold,
					enum srtype type,
					uint8_t value)
{
	switch (type) {
	case SR_TYPE_NON_SRG_OBSS_PD:
		SET_SELF_OBSS_PD_THRESH(NON_SRG, *threshold, value);
		break;

	case SR_TYPE_SRG_OBSS_PD:
		SET_SELF_OBSS_PD_THRESH(SRG, *threshold, value);
		break;

	default:
		ath12k_err(NULL, "Invalid Spatial Reuse type");
		return -1;
	}

	return 0;
}

static inline int set_sr_per_ac(uint32_t *enable_bitmap,
				enum srtype type, uint8_t value)
{
	switch (type) {
	case SR_TYPE_OBSS_PD:
		SET_SELF_SR_PER_AC(OBSS_PD, *enable_bitmap, value);
		break;

	case SR_TYPE_PSR:
		SET_SELF_SR_PER_AC(PSR, *enable_bitmap, value);
	break;

	default:
		ath12k_err(NULL, "Invalid Spatial Reuse type");
		return -1;
	}

	return 0;
}

static inline void set_obss_pd_threshold_unit(uint32_t *threshold, uint8_t value)
{
	SR_SET_FIELD(*threshold, value, SELF_OBSS_PD_THRESH_UNITS);
}

static int ath12k_set_obss_pd_enable_bit(struct ath12k *ar, u32 enable,
					 enum srtype type)
{
	int ret;
	int temp_thresh;

	if (enable > 1) {
		ath12k_err(NULL, "OBSS_PD Threshold enable value should be either 0 or 1");
		return -EINVAL;
	}

	SR_SET_FIELD(temp_thresh,
		     IEEE80211_SELF_OBSS_PD_THRESHOLD_IN_DBM,
		     SELF_OBSS_PD_THRESH_UNITS); //check for global

	if (set_obss_pd_enable_bit(&temp_thresh, type, enable))
		return -EINVAL;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_CMD_OBSS_PD_THRESHOLD,
					temp_thresh, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "Could not set obss pd thresh enable val");
	} else {
		ar->sr_cfg.obss_pd_thresh = temp_thresh;
	}

	return ret;
}

static int ath12k_set_obss_pd_threshold(struct ath12k *ar, int32_t threshold,
					enum srtype type)
{
	int ret;
	int temp_thresh;

	/* OBSS Packet Detect threshold bounds for Spatial Reuse feature.
	 * The parameter value is programmed into the spatial reuse
	 * register, to specify how low the background signal strength
	 * from neighboring BSS cells must be, for this AP to
	 * employ spatial reuse.
	 */
	if (threshold > SELF_OBSS_PD_UPPER_THRESH ||
	    threshold < SELF_OBSS_PD_LOWER_THRESH) {
		ath12k_err(NULL, "Threshold must in the range [%d, %d] (both inclusive)",
			   SELF_OBSS_PD_LOWER_THRESH, SELF_OBSS_PD_UPPER_THRESH);
		return -EINVAL;
	}

	SR_SET_FIELD(temp_thresh,
		     IEEE80211_SELF_OBSS_PD_THRESHOLD_IN_DBM,
		     SELF_OBSS_PD_THRESH_UNITS); //check for gloabal

	if (set_obss_pd_threshold(&temp_thresh, type, threshold))
		return -EINVAL;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_CMD_OBSS_PD_THRESHOLD,
					temp_thresh, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI send for set cmd obss pd threshold failed");
	} else {
		ar->sr_cfg.obss_pd_thresh = temp_thresh;
	}

	return ret;
}

static int ath12k_set_sr_per_ac(struct ath12k *ar, u32 value,
				enum srtype type)
{
	int ret;
	u32 temp_enable_sr;

	if (value > 15) {
		ath12k_err(NULL, "Value: %d is not a valid value for setting SR ACs",
			   value);
		return -EINVAL;
	}

	temp_enable_sr = IEEE80211_SR_ENABLE_PER_AC;
	// check for global
	if (set_sr_per_ac(&temp_enable_sr, type, value)) {
		ath12k_err(NULL, "Unable to set SR per AC");
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_CMD_OBSS_PD_PER_AC,
					temp_enable_sr, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "Error sending WMI for SR per AC");
	} else {
		ar->sr_cfg.enable_per_ac = temp_enable_sr;
	}

	return ret;
}

static int ath12k_set_self_hesiga_sr15_enable(struct ath12k *ar,
					      u8 enable)
{
	int ret;

	if (enable > 1) {
		ath12k_err(NULL, "HESIGA SR15 enable field can either be 0 or 1");
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_SR_PROHIBIT,
					enable, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI send to set HESIGA SR15 failed, discarding configuration");
	} else {
		ar->sr_cfg.hesiga_sr15_enable = enable;
	}

	return ret;
}

static int ath12k_set_self_psr_tx_enable(struct ath12k *ar,
					 u8 enable)
{
	int ret;

	if (enable > 1) {
		ath12k_err(NULL, "PSR Tx enable field can either be 0 or 1");
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_SRP,
					enable, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI command failed, discarding the configuration");
	} else {
		ar->sr_cfg.psr_tx_enable = enable;
	}

	return ret;
}

static int ath12k_set_self_safety_margin_psr(struct ath12k *ar,
					     u8 margin)
{
	int ret;

	if (margin > 5) {
		ath12k_err(NULL, "Safety margin should be in the range 0-5 (both inclusive)");
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SR_TRIGGER_MARGIN,
					margin, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI command failed, discarding the configuration");
	} else {
		ar->sr_cfg.safety_margin_psr = margin;
	}

	return ret;
}

static int ath12k_configure_obss_pd_ctrl_frame(struct ath12k *ar,
					       u32 enable,
					       int32_t threshold,
					       enum srtype type)
{
	int ret;
	int temp_thresh;

	if (enable > 1) {
		ath12k_err(NULL, "OBSS_PD Threshold enable value should be either 0 or 1");
		return -EINVAL;
	}

	/* OBSS Packet Detect threshold bounds for Spatial Reuse feature.
	 * The parameter value is programmed into the spatial reuse
	 * register, to specify how low the background signal strength
	 * from neighboring BSS cells must be, for this AP to
	 * employ spatial reuse.
	 */
	if (threshold > SELF_OBSS_PD_UPPER_THRESH ||
	    threshold < SELF_OBSS_PD_LOWER_THRESH) {
		ath12k_err(NULL, "Threshold must in the range [%d, %d] (both inclusive)",
			   SELF_OBSS_PD_LOWER_THRESH, SELF_OBSS_PD_UPPER_THRESH);
        	return -EINVAL;
	}

	/* Read the existing configuration */
	temp_thresh  = ar->sr_cfg.obss_pd_thresh_ctrl_frame;

	/* Set the enable value */
	if (set_obss_pd_enable_bit(&temp_thresh, type, enable))
		return -EINVAL;

	/* Set the threshold */
	if (set_obss_pd_threshold(&temp_thresh, type, threshold))
		return -EINVAL;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_CTRL_FRAME_OBSS_PD_THRESHOLD,
					temp_thresh, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI send for set cmd obss pd threshold ctrl frame failed");
	} else {
		ar->sr_cfg.obss_pd_thresh_ctrl_frame = temp_thresh;
	}

	return ret;
}

static int ath12k_set_self_sr_enable_truncation(struct ath12k *ar,
						u8 enable)
{
	int ret;

	if (enable > 1) {
		ath12k_err(NULL, "SR truncate enable field can either be 0 or 1");
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TRUNCATE_SR,
					enable, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI command failed, discarding the configuration");
	} else {
		ar->sr_cfg.enable_sr_truncate = enable;
	}

	return ret;
}

static int ath12k_set_self_sr_obss_ppdu_min_dur(struct ath12k *ar,
						u32 duration)
{
	int ret;

	if (duration > MAX_SELF_SR_MIN_TXOP ) {
		ath12k_err(NULL, "Enter a valid sr duration < Min: 0 -  Max: 65535 >");
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_OBSS_MIN_DURATION_CHECK_FOR_SR,
					duration, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(NULL, "WMI command failed, discarding the configuration");
	} else {
		ar->sr_cfg.min_obss_ppdu_dur_sr = duration;
	}

	return ret;
}

static int ath12k_set_self_srg_bss_color_bitmap(struct ath12k *ar,
						u32 *bitmap)
{
	int ret;

	ret = ath12k_wmi_pdev_set_srg_bss_color_bitmap(ar, bitmap);
	if (ret) {
		ath12k_err(NULL, "WMI send for set self srg bitmap failed, discarding the configuration");
	} else {
		ar->sr_cfg.srg_bss_color_bitmap[0] = bitmap[0];
		ar->sr_cfg.srg_bss_color_bitmap[1] = bitmap[1];
	}

	return ret;
}

static int ath12k_set_self_srg_partial_bssid_bitmap(struct ath12k *ar,
						    u32 *bitmap)
{
	int ret;

	ret = ath12k_wmi_pdev_set_srg_patial_bssid_bitmap(ar, bitmap);
	if (ret) {
		ath12k_err(NULL, "WMI send for set self srg bitmap failed, discarding the configuration");
	} else {
		ar->sr_cfg.srg_partial_bssid_bitmap[0] = bitmap[0];
		ar->sr_cfg.srg_partial_bssid_bitmap[1] = bitmap[1];
	}

	return ret;
}

static int ath12k_set_self_srg_obss_color_enable_bitmap(struct ath12k *ar,
							u32 *bitmap)
{
	int ret;

	ret = ath12k_wmi_pdev_srg_obss_color_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_err(NULL, "WMI send for set_self_srg_obss_color_enable_bitmap failed, discarding the configuration");
	} else {
		ar->sr_cfg.srg_obss_color_enable_bitmap[0] = bitmap[0];
		ar->sr_cfg.srg_obss_color_enable_bitmap[1] = bitmap[1];
	}

	return ret;
}

static int ath12k_set_self_srg_obss_bssid_enable_bitmap(struct ath12k *ar,
							u32 *bitmap)
{
	int ret;

	ret = ath12k_wmi_pdev_srg_obss_bssid_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_err(NULL, "WMI send for set_self_srg_obss_bssid_enable_bitmap failed, discarding the configuration");
	} else {
		ar->sr_cfg.srg_obss_bssid_enable_bitmap[0] = bitmap[0];
		ar->sr_cfg.srg_obss_bssid_enable_bitmap[1] = bitmap[1];
	}

	return ret;
}

static int ath12k_set_self_non_srg_obss_color_enable_bitmap(struct ath12k *ar,
							    u32 *bitmap)
{
	int ret;

	ret = ath12k_wmi_pdev_non_srg_obss_color_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_err(NULL, "WMI send for set_self_non_srg_obss_color_enable_bitmap failed, discarding the configuration");
	} else {
		ar->sr_cfg.non_srg_obss_color_enable_bitmap[0] = bitmap[0];
		ar->sr_cfg.non_srg_obss_color_enable_bitmap[1] = bitmap[1];
	}

	return ret;
}

static int ath12k_set_self_non_srg_obss_bssid_enable_bitmap(struct ath12k *ar,
							    u32 *bitmap)
{
	int ret;

	ret = ath12k_wmi_pdev_non_srg_obss_bssid_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_err(NULL, "WMI send for set_self_non_srg_obss_bssid_enable_bitmap failed, discarding the configuration");
	} else {
		ar->sr_cfg.non_srg_obss_bssid_enable_bitmap[0] = bitmap[0];
		ar->sr_cfg.non_srg_obss_bssid_enable_bitmap[1] = bitmap[1];
	}

	return ret;
}

static int ath12k_vendor_set_sr_radio_params(struct ath12k *ar,
					     u32 param, void *data,
					     u32 data_len, u32 value,
					     u32 flags, bool *reload)
{
	int ret = -1;
	u32 bitmap[2];

	if (!data)
		return -EINVAL;

	switch (param) {
	case SR_SELF_OBSS_PD_TX_ENABLE:
		ret = ath12k_set_obss_pd_enable_bit(ar, value,
						    ath12k_extact_sr_type(data, data_len));
		break;
	case SR_SELF_OBSS_PD_THRESHOLD_DB:
		ret = ath12k_set_obss_pd_threshold(ar, value,
						   ath12k_extact_sr_type(data, data_len));
		break;
	case SR_SELF_OBSS_PD_THRESHOLD_DBM:
		ret = ath12k_set_obss_pd_threshold(ar, value,
						   ath12k_extact_sr_type(data, data_len));
		break;
	case SR_SELF_ENABLE_PER_AC:
		ret = ath12k_set_sr_per_ac(ar, value,
					   ath12k_extact_sr_type(data, data_len));
		break;
	case SR_SELF_HESIGA_SR15_ENABLE:
		if (data_len < sizeof(uint8_t))
			return -EINVAL;
		ret = ath12k_set_self_hesiga_sr15_enable(ar, *(uint8_t *)data);
		break;
	case SR_SELF_PSR_TX_ENABLE:
		if (data_len < sizeof(uint8_t))
			return -EINVAL;
		ret = ath12k_set_self_psr_tx_enable(ar, *(uint8_t *)data);
		break;
	case SR_SELF_SAFETY_MARGIN_PSR:
		if (data_len < sizeof(uint8_t))
			return -EINVAL;
		ret = ath12k_set_self_safety_margin_psr(ar, *(uint8_t *)data);
		break;
	case SR_SELF_OBSS_PD_CTRL_FRAME:
		ret = ath12k_configure_obss_pd_ctrl_frame(ar, value,
							  flags,
							  ath12k_extact_sr_type(data, data_len));
		break;
	case SR_SELF_ENABLE_TRUNCATION:
		if (data_len < sizeof(uint8_t))
			return -EINVAL;
		ret = ath12k_set_self_sr_enable_truncation(ar, *(uint8_t *)data);
		break;
	case SR_SELF_OBSS_PPDU_MIN_DUR:
		if (data_len < sizeof(uint8_t))
			return -EINVAL;
		ret = ath12k_set_self_sr_obss_ppdu_min_dur(ar, *(uint8_t *)data);
		break;
	case SR_SELF_SRG_BSS_COLOR_BITMAP:
		if (data_len < sizeof(u32))
			return -EINVAL;
		bitmap[0] = value;
		memcpy(&bitmap[1], data, sizeof(u32));
		ret = ath12k_set_self_srg_bss_color_bitmap(ar, bitmap);
		break;
	case SR_SELF_SRG_PARTIAL_BSSID_BITMAP:
		if (data_len < sizeof(u32))
			return -EINVAL;
		bitmap[0] = value;
		memcpy(&bitmap[1], data, sizeof(u32));
		ret = ath12k_set_self_srg_partial_bssid_bitmap(ar, bitmap);
		break;
	case SR_SELF_SRG_OBSS_COLOR_ENABLE_BITMAP:
		if (data_len < sizeof(u32))
			return -EINVAL;
		bitmap[0] = value;
		memcpy(&bitmap[1], data, sizeof(u32));
		ret = ath12k_set_self_srg_obss_color_enable_bitmap(ar, bitmap);
		break;
	case SR_SELF_SRG_OBSS_BSSID_ENABLE_BITMAP:
		bitmap[0] = value;
		memcpy(&bitmap[1], data, sizeof(u32));
		ret = ath12k_set_self_srg_obss_bssid_enable_bitmap(ar,
								   bitmap);
		break;
	case SR_SELF_NON_SRG_OBSS_COLOR_ENABLE_BITMAP:
		bitmap[0] = value;
		memcpy(&bitmap[1], data, sizeof(u32));
		ret = ath12k_set_self_non_srg_obss_color_enable_bitmap(ar,
								       bitmap);
		break;
	case SR_SELF_NON_SRG_OBSS_BSSID_ENABLE_BITMAP:
		bitmap[0] = value;
		memcpy(&bitmap[1], data, sizeof(u32));
		ret = ath12k_set_self_non_srg_obss_bssid_enable_bitmap(ar,
								       bitmap);
		break;
	default:
		ath12k_info(NULL, "Unhandled SR config command");
		return -EINVAL;
	}

	return ret;
}

/**
 * ath12k_get_current_cac_timeout - Get configured CAC timeout for this radio
 * @ar: pointer to ath12k structure
 * @timeout_sec: configured CAC timeout in seconds
 *
 * Return: 0 on success, -1 when no channel context/channel is available,
 * or -EINVAL for invalid input.
 */
static int ath12k_get_current_cac_timeout(struct ath12k *ar, u32 *timeout_sec)
{
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	u32 cac_time_ms;

	if (!timeout_sec)
		return -EINVAL;

	*timeout_sec = 0;

	chanctx_conf = ath12k_mac_get_first_active_arvif_chanctx(ar);
	if (!chanctx_conf)
		return -ENODEV;

	chandef = &chanctx_conf->def;
	if (!chandef->chan)
		return -ENODEV;

	if (ar->ar_extn.cac_timeout_override_ms != NO_CAC_OVERRIDE) {
		*timeout_sec = (u32)(ar->ar_extn.cac_timeout_override_ms /
				     ATH12K_MSEC_PER_SEC);
		return 0;
	}

	cac_time_ms = cfg80211_chandef_dfs_cac_time(hw->wiphy, chandef, false, false);
	*timeout_sec = cac_time_ms / ATH12K_MSEC_PER_SEC;

	return 0;
}

/**
 * ath12k_get_current_bgcac_timeout - Get the current BG CAC timeout
 * @ar: ath12k radio instance
 * @timeout_sec: output buffer for the BG CAC timeout in seconds
 *
 * Return the per-radio BG CAC timeout override when configured. Otherwise,
 * return the default BG CAC timeout for the active channel.
 *
 * Return: 0 on success, -ENODEV when no channel context is available,
 * -EINVAL for invalid input, or -EOPNOTSUPP when BG CAC timeout override
 * support is not compiled in.
 */
static int ath12k_get_current_bgcac_timeout(struct ath12k *ar, u32 *timeout_sec)
{
#ifdef CPTCFG_QCA_LAB_TEST_FEATURES
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	u32 timeout_ms;

	if (!timeout_sec)
		return -EINVAL;

	*timeout_sec = 0;
	timeout_ms = cfg80211_get_radio_bgcac_cac_time(hw->wiphy, ar->radio_idx);
	if (timeout_ms) {
		*timeout_sec = timeout_ms / ATH12K_MSEC_PER_SEC;
		return 0;
	}

	chanctx_conf = ath12k_mac_get_first_active_arvif_chanctx(ar);
	if (!chanctx_conf)
		return -ENODEV;

	chandef = &chanctx_conf->def;
	if (!chandef->chan)
		return -ENODEV;

	timeout_ms = cfg80211_chandef_dfs_cac_time(hw->wiphy, chandef, true, false);
	*timeout_sec = timeout_ms / ATH12K_MSEC_PER_SEC;

	return 0;
#else
	return -EOPNOTSUPP;
#endif
}

/**
 * ath12k_set_current_bgcac_timeout - Configure the BG CAC timeout override
 * @ar: ath12k radio instance
 * @timeout_ms: BG CAC timeout override in milliseconds, or 0 to clear it
 *
 * Store the per-radio BG CAC timeout override in cfg80211 radio configuration.
 * BG CAC timeout is consumed by cfg80211_chandef_dfs_cac_time(), so updating
 * cfg80211 avoids changing ieee80211_channel extension data and does not
 * require a regulatory rebuild that could reset DFS channel state.
 *
 * Return: 0 on success, a negative cfg80211 error code on failure, or
 * -EOPNOTSUPP when BG CAC timeout override support is not compiled in.
 */
static int ath12k_set_current_bgcac_timeout(struct ath12k *ar, u32 timeout_ms)
{
#ifdef CPTCFG_QCA_LAB_TEST_FEATURES
	struct wiphy *wiphy = ath12k_ar_to_hw(ar)->wiphy;
	int ret;

	ret = cfg80211_set_radio_bgcac_cac_time(wiphy, ar->radio_idx, timeout_ms);
	if (ret) {
		ath12k_err(ar->ab,
			   "failed to set BG CAC timeout for pdev %u: %d\n",
			   ar->pdev_idx, ret);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "BG CAC timeout is now %s (%u sec) for pdev %u\n",
		   timeout_ms ? "overridden" : "default",
		   timeout_ms / ATH12K_MSEC_PER_SEC, ar->pdev_idx);

	return 0;
#else
	return -EOPNOTSUPP;
#endif
}

/**
 * ath12k_is_timeout_sec_valid - Validate a vendor CAC timeout value
 * @ab: ath12k base instance used for logging
 * @timeout_sec: timeout value in seconds
 * @name: timeout name to include in validation errors
 * @allow_zero: true if 0 is accepted as a valid timeout value
 *
 * Check the user-provided CAC or BG CAC timeout before converting it to
 * milliseconds.  Non-zero values are capped at ATH12K_MAX_CAC_TIMEOUT_SEC to
 * keep the millisecond conversion within the supported range.
 *
 * Return: true if @timeout_sec is valid, false otherwise.
 */
static bool ath12k_is_timeout_sec_valid(struct ath12k_base *ab,
					u32 timeout_sec,
					const char *name,
					bool allow_zero)
{
	if (!timeout_sec && !allow_zero) {
		ath12k_err(ab, "Invalid %s timeout value: 0\n", name);
		return false;
	}

	if (timeout_sec > ATH12K_MAX_CAC_TIMEOUT_SEC) {
		ath12k_err(ab, "%s timeout value too large: %u (max %u sec)\n",
			   name, timeout_sec, ATH12K_MAX_CAC_TIMEOUT_SEC);
		return false;
	}

	return true;
}

/**
 * ath12k_is_ar_range_valid - Check whether a radio frequency range is valid
 * @ar: ath12k radio instance
 *
 * Validate that both ends of the per-radio frequency range are initialized.
 * The range is used to limit CAC timeout updates to regulatory rules that
 * belong to the current radio.
 *
 * Return: true if both low and high frequencies are configured, false
 * otherwise.
 */
static inline bool ath12k_is_ar_range_valid(struct ath12k *ar)
{
	return ar->chan_info.low_freq && ar->chan_info.high_freq;
}

/**
 * ath12k_is_rule_in_radio - Check if a regulatory rule overlaps a radio
 * @ar: ath12k radio instance
 * @rule: regulatory rule to test
 *
 * Compare @rule against the per-radio frequency range in @ar.  The helper uses
 * overlap instead of full containment so wide regulatory rules that partially
 * cover a radio still receive the radio-specific CAC timeout override.
 *
 * Return: true if the rule overlaps the radio frequency range, false otherwise.
 */
static inline bool ath12k_is_rule_in_radio(struct ath12k *ar,
					   const struct ieee80211_reg_rule *rule)
{
	u32 rule_low = KHZ_TO_MHZ(rule->freq_range.start_freq_khz);
	u32 rule_high = KHZ_TO_MHZ(rule->freq_range.end_freq_khz);

	return rule_high > ar->chan_info.low_freq &&
	       rule_low < ar->chan_info.high_freq;
}


/**
 * ath12k_apply_cac_timeout_override - Apply CAC timeout to active cfg80211 data
 * @ar: ath12k radio instance
 * @cac_timeout_ms: CAC timeout override in milliseconds
 *
 * Update the active cfg80211 regulatory domain and channel list for the radio's
 * frequency range.  This applies a newly configured CAC timeout immediately
 * without scheduling ath12k_regd_update_work(), which would rebuild channels
 * through handle_band_custom() and may reset DFS channel state.
 *
 * Return: 0 on success, a negative cfg80211 error code on failure, or
 * -EOPNOTSUPP when CAC timeout override support is not compiled in.
 */
static int ath12k_apply_cac_timeout_override(struct ath12k *ar,
					     u32 cac_timeout_ms)
{
#ifdef CPTCFG_QCA_LAB_TEST_FEATURES
	struct wiphy *wiphy = ath12k_ar_to_hw(ar)->wiphy;
	u32 start_freq = 0, end_freq = 0;
	int ret;

	if (ath12k_is_ar_range_valid(ar)) {
		start_freq = ar->chan_info.low_freq;
		end_freq = ar->chan_info.high_freq;
	}

	ret = cfg80211_update_dfs_cac_time(wiphy, start_freq, end_freq, cac_timeout_ms);
	if (ret)
		ath12k_err(ar->ab,
			   "failed to apply CAC timeout override for pdev %u: %d\n",
			   ar->pdev_idx, ret);

	return ret;
#else
	return -EOPNOTSUPP;
#endif
}

/**
 * ath12k_should_remove_dfs_reg_rule - Check if a DFS rule should be blocked
 * @ar: pointer to ath12k structure
 * @rule: regulatory rule to check
 *
 * Return: true if @rule is a DFS rule and overlaps this radio's frequency
 * range, false otherwise. If the radio range is unavailable, all DFS rules are
 * considered applicable to this radio.
 */
static bool
ath12k_should_remove_dfs_reg_rule(struct ath12k *ar,
				  const struct ieee80211_reg_rule *rule)
{
	if (!(rule->flags & NL80211_RRF_DFS))
		return false;

	if (!ath12k_is_ar_range_valid(ar))
		return true;

	return ath12k_is_rule_in_radio(ar, rule);
}

/**
 * ath12k_remove_dfs_reg_rules - Remove blocked DFS rules from a regdomain copy
 * @ar: pointer to ath12k structure
 * @regd: regulatory domain copy to update
 *
 * Return true if rules are removed, false otherwise
 *
 * Compacts @regd in place by dropping DFS rules selected by
 * ath12k_should_remove_dfs_reg_rule(). The stored firmware regdomain is not
 * modified, so unblocking can restore DFS operation by rebuilding from a fresh
 * firmware regdomain copy.
 */
static bool
ath12k_remove_dfs_reg_rules(struct ath12k *ar, struct ieee80211_regdomain *regd)
{
	unsigned int num_rules = 0;
	unsigned int removed_rules = 0;
	unsigned int i;

	for (i = 0; i < regd->n_reg_rules; i++) {
		struct ieee80211_reg_rule *rule = &regd->reg_rules[i];

		if (ath12k_should_remove_dfs_reg_rule(ar, rule)) {
			ath12k_info(ar->ab,
				    "blockdfslist: remove DFS rule [%u-%u MHz] for pdev %u\n",
				    KHZ_TO_MHZ(rule->freq_range.start_freq_khz),
				    KHZ_TO_MHZ(rule->freq_range.end_freq_khz),
				    ar->pdev_idx);
			removed_rules++;
			continue;
		}

		if (num_rules != i)
			regd->reg_rules[num_rules] = *rule;
		num_rules++;
	}

	regd->n_reg_rules = num_rules;

	ath12k_info(ar->ab,
		    "blockdfslist: pdev %u removed %u DFS rules, remaining %u rules\n",
		    ar->pdev_idx, removed_rules, regd->n_reg_rules);

	return removed_rules > 0;
}

/**
 * ath12k_update_regd_block_dfs_ext - Apply DFS block state into reg_rules
 * @ar: pointer to ath12k structure
 * @regd: pointer to the regulatory domain copy to update
 *
 * When block_dfs_channels is set, removes DFS rules for this radio from the
 * temporary regd copy passed to regulatory_set_wiphy_regd().  The original
 * firmware regulatory domain is never modified, so unblocking uses a fresh copy
 * with the original DFS rules intact.
 */
void ath12k_update_regd_block_dfs_ext(struct ath12k *ar,
				      struct ieee80211_regdomain *regd)
{
	if (!ar->ar_extn.block_dfs_channels)
		return;

	ath12k_info(ar->ab,
		    "blockdfslist: apply pdev %u block %u range %u-%u rules %u\n",
		    ar->pdev_idx, ar->ar_extn.block_dfs_channels,
		    ar->chan_info.low_freq, ar->chan_info.high_freq,
		    regd->n_reg_rules);

	if (!ath12k_remove_dfs_reg_rules(ar, regd))
		return;

	ath12k_info(ar->ab,
		    "blockdfslist: dfs_rules_removed set for pdev %u regd %p\n",
		    ar->pdev_idx, regd);
}

/**
 * ath12k_blockdfs_apply_channels - Apply blockdfslist state to DFS channels
 * @ar: pointer to ath12k structure
 *
 * Applies or removes the blockdfslist restriction for DFS channels within
 * this pdev's frequency range. Only channels belonging to the current radio
 * are updated; sibling radios sharing the same wiphy are left unchanged.
 *
 * When blockdfslist is enabled, DFS channels that are not already disabled
 * are marked with IEEE80211_CHAN_DISABLED and tracked in
 * @ar->ar_extn.blockdfs_disabled_chans. Channels that were previously
 * disabled by regulatory processing or any other mechanism are left
 * untouched.
 *
 * When blockdfslist is disabled, only channels recorded in
 * @ar->ar_extn.blockdfs_disabled_chans are restored. This distinction is
 * required because a channel may be disabled either by blockdfslist or by
 * regulatory rules. The bitmap allows the driver to identify and re-enable
 * only those channels that were disabled by blockdfslist, while preserving
 * existing regulatory restrictions.
 *
 * DFS state is reset only for channels affected by blockdfslist.
 */
static void ath12k_blockdfs_apply_channels(struct ath12k *ar)
{
	struct wiphy *wiphy = ath12k_ar_to_hw(ar)->wiphy;
	struct ieee80211_supported_band *sband;
	u32 lo = ar->chan_info.low_freq;
	u32 hi = ar->chan_info.high_freq;
	int i;

	sband = wiphy->bands[NL80211_BAND_5GHZ];
	if (!sband)
		return;

	for (i = 0; i < sband->n_channels; i++) {
		struct ieee80211_channel *chan = &sband->channels[i];

		if (chan->center_freq < lo || chan->center_freq > hi)
			continue;

		if (!(chan->flags & IEEE80211_CHAN_RADAR))
			continue;

		if (ar->ar_extn.block_dfs_channels) {
			if (chan->flags & IEEE80211_CHAN_DISABLED)
				continue;
			chan->flags |= IEEE80211_CHAN_DISABLED;
			set_bit(i, ar->ar_extn.blockdfs_disabled_chans);
			chan->dfs_state = NL80211_DFS_USABLE;
			chan->dfs_state_entered = jiffies;
			chan->beacon_found = false;
			ath12k_dbg(ar->ab, ATH12K_DBG_REG,
				   "blockdfslist: disabled %u MHz pdev %u\n",
				   chan->center_freq, ar->pdev_idx);
		} else {
		    if (!test_and_clear_bit(i, ar->ar_extn.blockdfs_disabled_chans)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_REG,
				   "blockdfslist: channel %u MHz was not disabled by blockdfs\n",
				   chan->center_freq);
			continue;
		    }

		    if (!(chan->flags & IEEE80211_CHAN_DISABLED)) {
			ath12k_warn(ar->ab,
				    "blockdfslist: inconsistent state for %u MHz pdev %u, channel tracked as blockdfs-disabled but IEEE80211_CHAN_DISABLED is not set\n",
				    chan->center_freq, ar->pdev_idx);
			continue;
		    }

		    chan->flags &= ~IEEE80211_CHAN_DISABLED;

		    ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			       "blockdfslist: restored %u MHz pdev %u\n",
			       chan->center_freq, ar->pdev_idx);
		}
	}
}

/**
 * ath12k_blockdfs_copy_regd - Duplicate a regdomain for blockdfslist mutation
 * @src:    source regdomain (caller holds the appropriate lock/rcu)
 * @flags:  GFP flags for the allocation
 *
 * Returns a kmemdup'd copy of @src, or NULL on allocation failure.
 */
static struct ieee80211_regdomain *
ath12k_blockdfs_copy_regd(const struct ieee80211_regdomain *src, gfp_t flags)
{
	int regd_len = sizeof(*src) +
		       src->n_reg_rules * sizeof(struct ieee80211_reg_rule);

	return kmemdup(src, regd_len, flags);
}

/**
* ath12k_block_dfs_channels - Block DFS channels for a pdev
* @ar: pointer to ath12k structure
*
* The primary purpose of this function is to disable DFS channels for the
* specified radio. It obtains a valid source regdomain, preserving any DFS
* restrictions already applied by sibling pdevs, removes DFS regulatory
* rules covering this radio's channel range, and updates the corresponding
* sband channels to the disabled state.
*
* The modified regdomain is then installed on the wiphy and a regulatory
* change notification is sent to userspace to trigger channel-list refresh.
*/
static int ath12k_block_dfs_channels(struct ath12k *ar)
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	struct wiphy *wiphy = hw->wiphy;
	const struct ieee80211_regdomain *src_regd;
	struct ieee80211_regdomain *regd_copy;

	/* Seed from wiphy->regd so that DFS rules already removed for sibling
	 * pdevs are not restored by this operation. */
	rcu_read_lock();
	src_regd = get_wiphy_regdom(wiphy);
	if (!src_regd) {
		rcu_read_unlock();
		spin_lock_bh(&ar->ab->base_lock);
		src_regd = ath12k_reg_get_fallback_regd(ar->ab, ar->pdev_idx);
		if (!src_regd) {
			spin_unlock_bh(&ar->ab->base_lock);
			return -EINVAL;
		}
		regd_copy = ath12k_blockdfs_copy_regd(src_regd, GFP_ATOMIC);
		spin_unlock_bh(&ar->ab->base_lock);
	} else {
		regd_copy = ath12k_blockdfs_copy_regd(src_regd, GFP_ATOMIC);
		rcu_read_unlock();
	}

	if (!regd_copy)
		return -ENOMEM;

	ath12k_update_regd_block_dfs_ext(ar, regd_copy);
	ath12k_blockdfs_apply_channels(ar);
	regulatory_update_wiphy_regd(wiphy, regd_copy);
	regulatory_notify_wiphy_change(wiphy);
	return 0;
}

/**
 * ath12k_unblock_dfs_channels - Restore DFS channels for a pdev
 * @ar: pointer to ath12k structure
 *
 * Starts from the original firmware regd (so DFS rules for this pdev are
 * present), re-applies blocking for every other pdev still in blocked state,
 * updates per-channel flags, installs the new regdomain, and notifies
 * userspace.
 */
static int ath12k_unblock_dfs_channels(struct ath12k *ar)
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	struct ath12k_hw *ah = ar->ah;
	struct wiphy *wiphy = hw->wiphy;
	const struct ieee80211_regdomain *src_regd;
	struct ieee80211_regdomain *regd_copy;
	struct ath12k *ar_tmp;
	int i;

	spin_lock_bh(&ar->ab->base_lock);
	src_regd = ath12k_reg_get_fallback_regd(ar->ab, ar->pdev_idx);
	if (!src_regd) {
		spin_unlock_bh(&ar->ab->base_lock);
		return -EINVAL;
	}
	regd_copy = ath12k_blockdfs_copy_regd(src_regd, GFP_ATOMIC);
	spin_unlock_bh(&ar->ab->base_lock);

	if (!regd_copy)
		return -ENOMEM;

	/* Re-apply blocking for every pdev still in blocked state.
	 * ar->ar_extn.block_dfs_channels was already cleared for the current
	 * ar by the caller, so it is skipped automatically here. */
	for_each_ar(ah, ar_tmp, i)
		ath12k_update_regd_block_dfs_ext(ar_tmp, regd_copy);

	ath12k_blockdfs_apply_channels(ar);
	regulatory_update_wiphy_regd(wiphy, regd_copy);
	regulatory_notify_wiphy_change(wiphy);
	return 0;
}

/**
 * ath12k_set_block_dfs_channels - Handle blockdfslist vendor command
 * @ar: pointer to ath12k structure
 * @value: 1 to block DFS channels, 0 to restore them
 *
 * Return 0 on success and -ve value on failure
 *
 * Thin dispatcher: updates the block state flag then delegates to
 * ath12k_block_dfs_channels() or ath12k_unblock_dfs_channels().
 */
static int ath12k_set_block_dfs_channels(struct ath12k *ar, u32 value)
{
	ar->ar_extn.block_dfs_channels = value ? 1 : 0;
	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "%s DFS channels for pdev %u\n",
		   value ? "Blocking" : "Unblocking", ar->pdev_idx);

	if (value)
		return ath12k_block_dfs_channels(ar);
	else
		return ath12k_unblock_dfs_channels(ar);
}

static int ath12k_vendor_set_radio_params(struct wiphy *wiphy,
					  struct ath12k *ar,
					  u32 param, u32 value,
					  struct ath12k_wifi_generic_params *params,
					  bool *reload)
{
        int ret = -1;
	u16 rx_ack_timeout = (value & 0xFFFF);
	u16 pre_rx_ack_timeout = (value >> 16);
	enum nl80211_band band;

        switch (param) {
        case QCA_WLAN_VENDOR_RADIO_PARAM_TEST_RELOAD:
                *reload = true;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_MGMT_RETRY_LIMIT:
		if (value < ATH12K_MGMT_TX_RETRY_LIMIT_MIN ||
		    value > ATH12K_MGMT_TX_RETRY_LIMIT_MAX) {
			ath12k_err(NULL, "Invalid tx retry limit %u (range %d to %d)\n",
				   value, ATH12K_MGMT_TX_RETRY_LIMIT_MIN,
				   ATH12K_MGMT_TX_RETRY_LIMIT_MAX);
			return -EINVAL;
		}

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_MGMT_RETRY_LIMIT,
						value, ar->pdev->pdev_id);
		if (ret) {
			ath12k_err(ar->ab, "Failed to set tx retry limit for mgmt frame: %d\n",
				   ret);
			return -EINVAL;
		}
		ar->mgmt_tx_retry_limit = value;
		break;
	case PARAM_RADIO_TXCHAINSOFT:
		ret = ath12k_set_chainmask(ar, value, true);
		break;
	case ACFG_PARAM_RADIO_RXCHAINMASK:
		ret = ath12k_set_chainmask(ar, value, false);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_RTS_CTS_RATE:
		if (value > 4) {
			ath12k_err(NULL, "Invalid setctsrate value %u (valid: 0-4)\n", value);
			return -EINVAL;
		}
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_RTS_FIXED_RATE,
						value, ar->pdev->pdev_id);
		if (!ret) {
			ar->radio_cfg.rts_cts_rate = value;
		} else {
			ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_RTS_FIXED_RATE to firmware");
		}
		break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_PS_STATE_CHANGE:
                ar->radio_cfg.ps_report = value;
                ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PEER_STA_PS_STATECHG_ENABLE,
                        value, ar->pdev->pdev_id);
		if (!ret ) {
		    ar->radio_cfg.ps_report = (u8) value;
		} else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PEER_STA_PS_STATECHG_ENABLE to firmware");
		}
           break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_NON_AGG_SW_RETRY_TH:
                ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_NON_AGG_SW_RETRY_TH,
                        value, ar->pdev->pdev_id);
		if (!ret) {
		    ar->radio_cfg.nonagg_swretry_th = value;
		} else {
		    ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_NON_AGG_SW_RETRY_TH to firmware");
		}
                break;
       case QCA_WLAN_VENDOR_RADIO_PARAM_AGG_SW_RETRY_TH:
                ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_AGG_SW_RETRY_TH,
                        value, ar->pdev->pdev_id);
		if (!ret) {
		    ar->radio_cfg.agg_swretry_th = value;
		} else {
		    ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_AGG_SW_RETRY_TH to firmware");
		}
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_STA_KICKOUT_TH:
                ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_STA_KICKOUT_TH,
                        value, ar->pdev->pdev_id);
		if (!ret) {
		    ar->radio_cfg.sta_kickout_th = value;
		} else {
		    ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_STA_KICKOUT_TH to firmware");
		}
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ARPDHCP_AC_OVERRIDE:
                if ((WME_AC_BE <= value) && (value <= WME_AC_VO)) {
                    ar->radio_cfg.arp_override = value;
                    ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ARP_AC_OVERRIDE,
                            value, ar->pdev->pdev_id);
		    if (!ret) {
			ar->radio_cfg.arp_override = (u8) value;
		    } else {
			ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ARP_AC_OVERRIDE to firmware");
		    }
                } else {
                    ret = -EINVAL;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANI_ENABLE:
		value = !!value;
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_ENABLE,
			value, ar->pdev->pdev_id);
		if (!ret) {
		    ar->radio_cfg.is_ani_enable = value;
		} else {
		    ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANI_ENABLE to firmware");
		}
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANI_POLL_PERIOD:
                if (value > 0) {
                    ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_POLL_PERIOD,
                            value, ar->pdev->pdev_id);
		    if (!ret) {
			ar->radio_cfg.ani_poll_period = value;
		    } else {
			ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANI_POLL_PERIOD to firmware");
		    }
                } else {
                    ret = -EINVAL;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANI_LISTEN_PERIOD:
                 if (value > 0) {
                     ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_LISTEN_PERIOD,
                             value, ar->pdev->pdev_id);
		     if (!ret) {
			 ar->radio_cfg.ani_listen_period = value;
		     } else {
			 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANI_LISTEN_PERIOD to firmware");
		     }
                 } else {
                     ret = -EINVAL;
		     }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANI_OFDM_LEVEL:
		 if ((s32)value < -128 || (s32)value > 127) {
		     ath12k_err(NULL, "value should between -128 to 127");
		     return -EINVAL;
		 }
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_OFDM_LEVEL,
                         value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.ani_ofdm_level = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANI_OFDM_LEVEL to firmware");
		 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANI_CCK_LEVEL:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_CCK_LEVEL,
                         value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.ani_cck_level = value;
		 } else {
		      ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANI_CCK_LEVEL to firmware");
		 }
                 break;
#define CCA_THRESHOLD_LIMIT_UPPER  -11
#define CCA_THRESHOLD_LIMIT_LOWER  -94
        case QCA_WLAN_VENDOR_RADIO_PARAM_CCA_THRESHOLD:
                 if ((int32_t)value && (int32_t)value > CCA_THRESHOLD_LIMIT_UPPER)
                     value = CCA_THRESHOLD_LIMIT_UPPER;
                 else if ((int32_t)value < CCA_THRESHOLD_LIMIT_LOWER)
                     value = CCA_THRESHOLD_LIMIT_LOWER;

                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_CCA_THRESHOLD,
                         value, ar->pdev->pdev_id);
                 if (!ret) {
		     ar->radio_cfg.cca_threshold = (int32_t)value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_CCA_THRESHOLD to firmware");
		 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_DYN_TX_CHAINMASK:
                 /****************************************
                  *Value definition:
                  * bit 0        dynamic TXCHAIN
                  * bit 1        single TXCHAIN
                  * bit 2        single TXCHAIN for ctrl frames
                  * For bit 0-1, if value =
                  * 0x1  ==>   Dyntxchain enabled,  single_txchain disabled
                  * 0x2  ==>   Dyntxchain disabled, single_txchain enabled
                  * 0x3  ==>   Both enabled
                  * 0x0  ==>   Both disabled
                  *
                  * bit 3-7      reserved
                  * bit 8-11     single txchain mask, only valid if bit 1 set
                  *
                  * For bit 8-11, the single txchain mask for this radio,
                  * only valid if single_txchain enabled, by setting bit 1.
                  * Single txchain mask need to be updated when txchainmask,
                  * is changed, e.g. 4x4(0xf) ==> 3x3(0x7)
                  ****************************************/
#define DYN_TXCHAIN         0x1
#define SINGLE_TXCHAIN      0x2
#define SINGLE_TXCHAIN_CTL  0x4
                 if( (value & SINGLE_TXCHAIN) ||
                         (value & SINGLE_TXCHAIN_CTL) ) {
                     value &= 0xf07;
                 } else {
                     value &= 0x1;
                 }

                 if (ar->radio_cfg.dtcs != value) {
                     ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DYNTXCHAIN,
                             value, ar->pdev->pdev_id);

                     if (!ret) {
			 ar->radio_cfg.dtcs = value;
		     } else {
			 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_DYNTXCHAIN to firmware");
		     }
		 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_ENABLE:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_ENABLE,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.ltr_enable = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_ENABLE to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_BE:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_AC_LATENCY_BE,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.ac_be = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_AC_LATENCY_BE to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_BK:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_AC_LATENCY_BK,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
                     ar->radio_cfg.ac_bk = value;
                 } else {
                     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_AC_LATENCY_BK to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_VI:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_AC_LATENCY_VI,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
                     ar->radio_cfg.ac_vi = value;
                 } else {
                     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_AC_LATENCY_VI to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_VO:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_AC_LATENCY_VO,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
                     ar->radio_cfg.ac_vo = value;
                 } else {
                     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_AC_LATENCY_VO to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_TIMEOUT:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_AC_LATENCY_TIMEOUT,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.ac_timeout = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_AC_LATENCY_TIMEOUT to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_TX_ACTIVITY_TIMEOUT:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_TX_ACTIVITY_TIMEOUT,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.tx_timeout = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_TX_ACTIVITY_TIMEOUT to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_SLEEP_OVERRIDE:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_SLEEP_OVERRIDE,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.sleep_override = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_SLEEP_OVERRIDE to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LTR_RX_OVERRIDE:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LTR_RX_OVERRIDE,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.rx_override = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LTR_RX_OVERRIDE to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_L1SS_ENABLE:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_L1SS_ENABLE,
			value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.l1ss_enable = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_L1SS_ENABLE to firmware");
		 }
		 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_DSLEEP_ENABLE:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DSLEEP_ENABLE,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.dsleep_enable = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_DSLEEP_ENABLE to firmware");
		 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SENS_LEVEL:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SENSITIVITY_LEVEL,
                         value, ar->pdev->pdev_id);
                 if (!ret) {
                     ar->radio_cfg.rxsop_sens_lvl = value;
                 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SENSITIVITY_LEVEL to firmware");
		 }
                 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DYN_GROUPING:
		 value = !!value;
		 if (ar->radio_cfg.dyngroup == value)
		     break;
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_MU_GROUP_POLICY,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.dyngroup = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_MU_GROUP_POLICY to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DPD_ENABLE:
		 value = !!value;
		 if (ar->radio_cfg.dpdenable == CLI_DPD_CMD_INPROGRES) {
		     ath12k_err(NULL, "Previous command is in progress");
		     break;
		 }
		 if (ar->radio_cfg.dpdenable == value) {
		     ath12k_err(NULL, "DPD enable already is in same state");
		 break;
		 }
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DPD_ENABLE,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.dpdenable = value;
		 } else {
		      ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_DPD_ENABLE to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_BURST_DUR:
		 if (value >= 0 && value <= 8192) {
		     ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_BURST_DUR,
			     value, ar->pdev->pdev_id);
		     if (!ret) {
			 ar->radio_cfg.burst_dur = (u16) value;
		     } else {
			 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_BURST_DUR to firmware");
		     }
		 } else {
		     return - EINVAL;
		 }
		 break;
#define DEFAULT_BURST_DURATION 8160
	case QCA_WLAN_VENDOR_RADIO_PARAM_BURST_ENABLE:
		 value = !!value;
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_BURST_ENABLE,
			 value, ar->pdev->pdev_id);
		 if (!ret)
		     ar->radio_cfg.burst_enable = (u8) value;
		 if (!ar->radio_cfg.burst_dur) {
		     ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_BURST_DUR,
			     DEFAULT_BURST_DURATION, ar->pdev->pdev_id);
		     if (!ret) {
			 ar->radio_cfg.burst_dur = (u16) DEFAULT_BURST_DURATION;
		     } else {
			 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_BURST_DURATION to firmware");
		     }
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DISABLE_LPI_ANT:
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DISABLE_LPI_ANT_OPTIMIZATION,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.disable_lpi_ant = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_DISABLE_LPI_ANT_OPTIMIZATION to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_EN_PROBE_ALL_BW:
		 value = !!value;
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_EN_PROBE_ALL_BW,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.en_probe_all_bw = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_EN_PROBE_ALL_BW to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_UL_OFDMA_RTD:
		 if (value > 600 || value < 0) {
		     ath12k_err(NULL, "Invalid value. Supported range is 0 to 600\n");
		     ret = -EINVAL;
		     break;
		 }
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_UL_OFDMA_RTD,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.ul_ofdma_rtd = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_UL_OFDMA_RTD to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_SMALL_MRU:
		 value = !!value;
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_SMALL_MRU,
			  value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.enable_small_mru = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ENABLE_SMALL_MRU to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_LARGE_MRU:
		 value = !!value;
		 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_LARGE_MRU,
			 value, ar->pdev->pdev_id);
		 if (!ret) {
		     ar->radio_cfg.enable_large_mru = value;
		 } else {
		     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ENABLE_LARGE_MRU to firmware");
		 }
		 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_RESET:
                 if (value > 0 && value < 6) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PDEV_RESET,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.pdev_reset = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SIGNED_TXPOWER_2G to firmware");
                         }
                 } else {
                         ath12k_dbg(NULL, ATH12K_DBG_CFG, "Invalid value : Use any one of the below values:\n"
                                         "    TX_FLUSH = 1\n"
                                         "    WARM_RESET = 2\n"
                                         "    COLD_RESET = 3\n"
                                         "    WARM_RESET_RESTORE_CAL = 4\n"
                                         "    COLD_RESET_RESTORE_CAL = 5\n");
                         ret = -EINVAL;
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_HW_MODE_CMDID:
                 if (!(value == WMI_HOST_HW_MODE_DBS ||
                                         value == WMI_HOST_HW_MODE_DBS_SBS)) {
                         ath12k_dbg(NULL, ATH12K_DBG_CFG,
                                         "HW mode %d not supported", value);
                         ret = -EINVAL;
                 } else {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_SET_HW_MODE_CMDID,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.current_mode = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_SET_HW_MODE_CMDID to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_LIMIT2G:
                 /*
                  * Check if the current regulatory channel list has any valid channel in the specified
                  * regulatory band, at least one channel in that band should be present/enabled in the
                  * current regulatory context, returns true.
                  */
                 band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
                 if (band == NL80211_BAND_2GHZ) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TXPOWER_LIMIT2G,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.txpowlimit2G = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_TXPOWER_LIMIT2G to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_LIMIT5G:
                 band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
                 if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TXPOWER_LIMIT5G,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.txpowlimit5G = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_TXPOWER_LIMIT5G to firmware");
                         }
                 }
                 break;
#define ANTENNA_GAIN_2G_MASK    0x0
#define ANTENNA_GAIN_5G_MASK    0x8000
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANTENNA_GAIN_2G:
                 if (value >= 0 && value <= 30) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANTENNA_GAIN,
                                         (value | ANTENNA_GAIN_2G_MASK), ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.antenna_gain_2g = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANTENNA_GAIN to firmware");
                         }
                 } else {
                         ath12k_dbg(NULL, ATH12K_DBG_CFG,
                                         "The value %d for ANTENNA_GAIN_2G is out of range.\n", value);
                         ret = -EINVAL;
                 }
                 break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_ANTENNA_GAIN_5G:
		 if (value >= 0 && value <= 30) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANTENNA_GAIN,
                                         (value | ANTENNA_GAIN_5G_MASK), ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.antenna_gain_5g = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANTENNA_GAIN to firmware");
                         }
                 } else {
                         ath12k_dbg(NULL, ATH12K_DBG_CFG,
                                         "The value %d for ANTENNA_GAIN_5G is out of range.\n", value);
                         ret = -EINVAL;
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_OFDM_LEVEL:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_OFDM_LEVEL,
                                 value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.ofdem_level = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANI_OFDM_LEVEL to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_SCALE:
                 if (WMI_HOST_TP_SCALE_MAX <= value && value <= WMI_HOST_TP_SCALE_MIN) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TXPOWER_SCALE,
                                         value, ar->pdev->pdev_id);
                         if(!ret) {
                                 ar->radio_cfg.txpower_scale = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_TXPOWER_SCALE to firmware");
                         }
                 } else {
                         ret = -EINVAL;
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_RX_FILTER:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_RX_FILTER,
                                 value, ar->pdev->pdev_id);
                 if(!ret) {
                         ar->radio_cfg.rx_filter = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_RX_FILTER to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_BLOCK_INTERBSS:
		 value = !!value;
                 if (value == ar->radio_cfg.block_interbss) {
                         ret = 0;
                 } else {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_BLOCK_INTERBSS,
                                         value, ar->pdev->pdev_id);
                         if (ret == 0) {
                                 ar->radio_cfg.block_interbss = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_BLOCK_INTERBSS to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_DISABLE_RESET_CMDID:
                 value = !!value;
                 if (ar->radio_cfg.fw_disable_reset != (u_int8_t)value) {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_DISABLE_RESET_CMDID,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.fw_disable_reset = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_DISABLE_RESET_CMDID to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PPDU_DURATION_CMDID:
                /* Get PPDU max duration based on countryISO. */
                /* Get PPDU min duration from IC, put sanity on value based on min and max. */
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_PPDU_DURATION_CMDID,
                                 value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.ppdu_dur = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_PPDU_DURATION_CMDID to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXBF_SOUND_PERIOD_CMDID:
                 if (value < 10 || value > 10000) {
                         ret = -EINVAL;
                 } else {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TXBF_SOUND_PERIOD_CMDID,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.txbf_sound_period = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_TXBF_SOUND_PERIOD_CMDID to firmware");
                         }
			 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PROMISC_MODE_CMDID:
		 value = !!value;
                 if (value == ar->radio_cfg.promisc_mode) {
                         ret = 0;
                 } else {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_PROMISC_MODE_CMDID,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.promisc_mode = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_PROMISC_MODE_CMDID to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_BURST_MODE_CMDID:
                 if (value < 0 || value >= 3) {
                         ath12k_dbg(NULL, ATH12K_DBG_CFG,
                                         "Usage: burst_mode <0:data-cts 1:data-data 2:data-(data/cts)\n");
                         ret = -EINVAL;
                 } else {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_BURST_MODE_CMDID,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.burst_mode = value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_BURST_MODE_CMDID to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_MCAST_BCAST_ECHO:
                 /* Set global Burst mode data-cts:0 data-ping-pong:1 data-cts-ping-pong:2. */
                 if (value < 0 || value > 1) {
                         ath12k_dbg(NULL, ATH12K_DBG_CFG,
                                         "Usage: Mcast Bcast Echo mode usage 0:disable 1:enable\n");
                         ret = -EINVAL;
                 } else {
                         ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_MCAST_BCAST_ECHO,
                                         value, ar->pdev->pdev_id);
                         if (!ret) {
                                 ar->radio_cfg.mcast_bcast_echo = (u_int8_t)value;
                         } else {
                                 ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_MCAST_BCAST_ECHO to firmware");
                         }
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANT_PLZN:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANT_PLZN, value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.ant_plzn = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANT_PLZN to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_AMSDU:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_PER_TID_AMSDU,
                                 value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.amsdu_mask = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ENABLE_PER_TID_AMSDU to firmware");
                 }
                 break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_AMPDU:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_PER_TID_AMPDU,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.ampdu_mask = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ENABLE_PER_TID_AMPDU to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_HE_MBSSID_CTRL_FRAME_CONFIG:
#define IEEE80211_MBSSID_CTRL_FRAME_MAX_VAL       0xF
                if(value > IEEE80211_MBSSID_CTRL_FRAME_MAX_VAL) {
                        ath12k_err(NULL, "Invalid input: 0x%x\n"
                                        "MBSSID Control frame config bit interpretation:\n"
                                        "B0: Basic Trigger setting\n"
                                        "B1: BSR Trigger setting\n"
                                        "B2: MU RTS setting\n"
                                        "B3: UL MUMIMO setting\n"
                                        "B4-B31: Reserved\n", value);
                        ret = -EINVAL;

                } else {
                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ENABLE_MBSSID_CTRL_FRAME,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.mbssid_en_ctrl_frame = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ENABLE_MBSSID_CTRL_FRAME to firmware");
                        }
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_PROBE_RESP_RETRY_LIMIT:
#define MAX_PRESP_RETRY_LIMIT 0x7F
                if (value > 0 && value <= MAX_PRESP_RETRY_LIMIT) {
                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PROBE_RESP_RETRY_LIMIT,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.resp_retry_limit = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_PROBE_RESP_RETRY_LIMIT to firmware");
                        }
                } else {
                        ath12k_err(NULL, "PROBERESP RETRY LIMIT should be between 1 and 127");
                        ret = -1;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CTS_TIMEOUT:
#define MAX_CTS_TIMEOUT 0xFF
                if (value > 0 && value <= MAX_CTS_TIMEOUT) {
                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_CTS_TIMEOUT,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.cts_timeout = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_CTS_TIMEOUT to firmware");
				}
                } else {
                        ath12k_err(NULL, "CTS Timeout value should be between 0 and 0xFF");
                        ret = -1;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SLOT_TIME:
#define MAX_SLOT_TIME 90
                if (value > 0 && value <= MAX_SLOT_TIME) {
                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SLOT_TIME,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.slot_time = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SLOT_TIME to firmware");
                        }
                } else {
                        ath12k_err(NULL, "Slot Time value should be between 0 and 90");
                        ret = -1;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ACK_TIMEOUT:
#define MIN_TX_ACK_TIMEOUT 0x40
#define MAX_TX_ACK_TIMEOUT 0xFF
                if (rx_ack_timeout >= MIN_TX_ACK_TIMEOUT &&
                                rx_ack_timeout <= (MAX_TX_ACK_TIMEOUT)) {
                        if (pre_rx_ack_timeout < MIN_TX_ACK_TIMEOUT ||
                                        pre_rx_ack_timeout > MAX_TX_ACK_TIMEOUT)
                                pre_rx_ack_timeout = rx_ack_timeout - 20;
                        value = (rx_ack_timeout | (pre_rx_ack_timeout << 16));
                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ACK_TIMEOUT,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.ack_timeout = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ACK_TIMEOUT to firmware");
                        }
                } else {
                        ath12k_err(NULL, "TX ACK Time-out value should be between 0x70 and 0xFF");
                        ret = -1;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CCK_TX_ENABLE:
		value = !!value;
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_CCK_TX_ENABLE,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.cck_enable = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_CCK_TX_ENABLE to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CAL_VER_CHECK:
		value = !!value;
                 ret = ath12k_wmi_send_pdev_check_cal_version_cmd(ar);
                 if (!ret) {
                         ar->radio_cfg.cal_ver_check = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_CHECK_CAL_VERSION to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_EQUAL_RU_ALLOCATION_ENABLE:
                if(value == 0 || value == 1) {
                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_EQUAL_RU_ALLOCATION_ENABLE,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.ru_alloc_en = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_EQUAL_RU_ALLOCATION_ENABLE to firmware");
                        }
                } else {
                        ath12k_err(NULL, "RU allocation enable value should be 0 or 1.");
                        ret = -1;
                }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANTENNA_GAIN_HALF_DB:
#define MAX_ANTENNA_GAIN 30
                if ((value >= 0) && (value <= (MAX_ANTENNA_GAIN * 2))) {
			band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
			if (band == NL80211_BAND_2GHZ) {
				value = value | ANTENNA_GAIN_2G_MASK;
			} else {
				value = value | ANTENNA_GAIN_5G_MASK;
			}

                        ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANTENNA_GAIN_HALF_DB,
                                        value, ar->pdev->pdev_id);
                        if (!ret) {
                                ar->radio_cfg.antenna_gain_half_db = value;
                        } else {
                                ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_ANTENNA_GAIN_HALF_DB to firmware");
                        }
                }
		break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_MGMT_TTL:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_MGMT_TTL,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.mgmt_ttl = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_MGMT_TTL to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PROBE_RESP_TTL:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_PROBE_RESP_TTL,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.prb_rsp_ttl = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_PROBE_RESP_TTL to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_MU_PPDU_DURATION:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_MU_PPDU_DURATION,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.mu_ppdu_dur = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_MU_PPDU_DURATION to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_TBTT_CTRL:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_TBTT_CTRL,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.tbtt_ctrl = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_TBTT_CTRL to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PREAM_PUNCT_BW:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_PREAM_PUNCT_BW,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.punct_bw = value;
                 } else {
                     ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_PREAM_PUNCT_BW to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_LOW_LATENCY_SCHED_MODE:
                 ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_LOW_LATENCY_SCHED_MODE,
                                                value, ar->pdev->pdev_id);
                 if (!ret) {
                         ar->radio_cfg.low_lat_mode = value;
                 } else {
                         ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_LOW_LATENCY_SCHED_MODE to firmware");
                 }
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_GET_TEMPERATURE:
        {
                struct ath12k_wmi_pdev *wmi = ar->wmi;
                struct sk_buff *skb;
                struct wmi_pdev_get_temperature_cmd {
                        __le32 tlv_header;
			__le32 param;
                        __le32 pdev_id;
                } __packed;
                struct wmi_pdev_get_temperature_cmd *cmd;

                /* Allocate WMI command buffer */
                skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
                if (!skb) {
                        ath12k_err(ar->ab, "Failed to allocate skb for temperature query\n");
                        return -ENOMEM;
                }

                /* Construct WMI command */
                cmd = (struct wmi_pdev_get_temperature_cmd *)skb->data;
                cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
                                            WMI_TAG_PDEV_GET_TEMPERATURE_CMD) |
                                 FIELD_PREP(WMI_TLV_LEN,
                                            sizeof(*cmd) - TLV_HDR_SIZE);
                cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

                /* Send WMI command directly */
                ret = ath12k_wmi_cmd_send(wmi, skb,
                                         WMI_PDEV_GET_TEMPERATURE_CMDID);
                if (ret) {
                        ath12k_err(ar->ab,
                                  "Failed to send WMI_PDEV_GET_TEMPERATURE_CMDID: %d\n",
                                  ret);
                        dev_kfree_skb(skb);
                        return ret;
                }

                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                          "Temperature query sent for pdev %d\n",
                          ar->pdev->pdev_id);

                /* Store that query was sent - response will come via event */
                ar->radio_cfg.temperature_query_pending = 1;
                ret = 0;
                break;
        }
        case QCA_WLAN_VENDOR_RADIO_PARAM_GPIO_CONFIG:
        {
                struct ath12k_wmi_pdev *wmi = ar->wmi;
                struct wmi_gpio_config_cmd *cmd;
                struct sk_buff *skb;
                u32 gpio_pin, gpio_dir, gpio_pull, gpio_function, gpio_intr;

                /* Value encoding:
                 * [31:24] - GPIO pin number (0-31)
                 * [23:16] - GPIO direction (0=input, 1=output)
                 * [15:8]  - GPIO pull type (0=none, 1=up, 2=down)
                 * [7:4]   - GPIO interrupt mode
                 * [3:0]   - GPIO function/mux
                 */
                gpio_pin      = (value >> 24) & 0xFF;
                gpio_dir      = (value >> 16) & 0xFF;
                gpio_pull     = (value >> 8)  & 0xFF;
                gpio_intr     = (value >> 4)  & 0x0F;
                gpio_function = value & 0x0F;

                /* Validate basic parameters */
                if (gpio_pin >= 32) {
                        ath12k_err(NULL, "Invalid GPIO pin number: %u\n", gpio_pin);
                        ret = -EINVAL;
                        break;
                }

                if (gpio_dir > QCA_WLAN_GPIO_OUTPUT) {
                        ath12k_err(NULL, "Invalid GPIO direction: %u\n", gpio_dir);
                        ret = -EINVAL;
                        break;
                }

                if (gpio_pull > QCA_WLAN_GPIO_PULL_DOWN) {
                        ath12k_err(NULL, "Invalid GPIO pull type: %u\n", gpio_pull);
                        ret = -EINVAL;
                        break;
                }

                /* Allocate SKB for WMI command */
                skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
                if (!skb) {
                        ath12k_err(NULL, "Failed to allocate SKB for GPIO config\n");
                        ret = -ENOMEM;
                        break;
                }

                /* Build WMI GPIO config command */
                cmd = (struct wmi_gpio_config_cmd *)skb->data;
                cmd->tlv_header =
                        FIELD_PREP(WMI_TLV_TAG, WMI_TAG_GPIO_CONFIG_CMD) |
                        FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
                cmd->gpio_num       = cpu_to_le32(gpio_pin);
                cmd->input          = cpu_to_le32(gpio_dir == QCA_WLAN_GPIO_INPUT ? 1 : 0);
                cmd->pull_type      = cpu_to_le32(gpio_pull);
                cmd->intr_mode      = cpu_to_le32(gpio_intr);
                cmd->mux_config_val = cpu_to_le32(gpio_function);

                /* Send WMI command */
                ret = ath12k_wmi_cmd_send(wmi, skb, WMI_GPIO_CONFIG_CMDID);
                if (ret) {
                        ath12k_err(NULL, "Failed to send GPIO config command: %d\n", ret);
                        dev_kfree_skb(skb);
                        break;
                }

                /* Cache configuration on success */
                ar->radio_cfg.gpio_cfg[gpio_pin].gpio_pin       = gpio_pin;
                ar->radio_cfg.gpio_cfg[gpio_pin].gpio_function  = gpio_function;
                ar->radio_cfg.gpio_cfg[gpio_pin].gpio_pull_type = gpio_pull;
                ar->radio_cfg.gpio_cfg[gpio_pin].gpio_dir       = gpio_dir;
                ar->radio_cfg.gpio_cfg[gpio_pin].gpio_intr_mode = gpio_intr;
                ar->radio_cfg.gpio_cfg[gpio_pin].configured     = true;

                ath12k_dbg(NULL, ATH12K_DBG_CFG,
                           "GPIO %u configured: dir=%u pull=%u intr=%u func=0x%x\n",
                           gpio_pin, gpio_dir, gpio_pull, gpio_intr, gpio_function);
                break;
        }
        case QCA_WLAN_VENDOR_RADIO_PARAM_GPIO_OUTPUT:
        {
                struct ath12k_wmi_pdev *wmi = ar->wmi;
                struct wmi_gpio_output_cmd *cmd;
                struct sk_buff *skb;
                u32 gpio_pin, gpio_value;

                /* Suggested encoding:
                 * [7:0]  - GPIO pin number (0-31)
                 * [8]    - value (0/1)
                 * others reserved
                 */
                gpio_pin   = value & 0xFF;
                gpio_value = (value >> 8) & 0x01;

                /* Validate pin */
                if (gpio_pin >= 32) {
                        ath12k_err(NULL, "Invalid GPIO pin number: %u\n", gpio_pin);
                        ret = -EINVAL;
                        break;
                }

                /* Check cached configuration */
                if (!ar->radio_cfg.gpio_cfg[gpio_pin].configured) {
                        ath12k_err(NULL, "GPIO %u not configured\n", gpio_pin);
                        ret = -EINVAL;
                        break;
                }

                if (ar->radio_cfg.gpio_cfg[gpio_pin].gpio_dir != QCA_WLAN_GPIO_OUTPUT) {
                        ath12k_err(NULL, "GPIO %u not configured as output\n", gpio_pin);
                        ret = -EINVAL;
                        break;
                }

                /* Allocate SKB */
                skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
                if (!skb) {
                        ath12k_err(NULL, "Failed to allocate SKB for GPIO output\n");
                        ret = -ENOMEM;
                        break;
                }

                /* Build WMI GPIO output command */
                cmd = (struct wmi_gpio_output_cmd *)skb->data;
                cmd->tlv_header =
                        FIELD_PREP(WMI_TLV_TAG, WMI_TAG_GPIO_OUTPUT_CMD) |
                        FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
                cmd->gpio_num = cpu_to_le32(gpio_pin);
                cmd->set      = cpu_to_le32(gpio_value);

                /* Send WMI command */
                ret = ath12k_wmi_cmd_send(wmi, skb, WMI_GPIO_OUTPUT_CMDID);
                if (ret) {
                        ath12k_err(NULL, "Failed to send GPIO output command: %d\n", ret);
                        dev_kfree_skb(skb);
                        break;
                }

                /* Cache value on success */
                ar->radio_cfg.gpio_cfg[gpio_pin].gpio_value = gpio_value;

                ath12k_dbg(NULL, ATH12K_DBG_CFG,
                           "GPIO %u output set to %u\n", gpio_pin, gpio_value);
                break;
        }
        case QCA_WLAN_VENDOR_RADIO_PARAM_MSDU_TTL:
                ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_MSDU_TTL_CMDID,
                                               value, ar->pdev->pdev_id);
                if (!ret) {
                        ar->radio_cfg.msdu_ttl = value;
                } else {
                        ath12k_err(NULL, "Failed to send WMI_PDEV_PARAM_SET_MSDU_TTL_CMDID to firmware");
                }
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DFS_NOL_SUBCHANNEL_MARKING:
	{
		bool enable;
		enable = !!value;
		if (ar->dfs_sub_channel_marking == enable) {
			ret = 0;
			break;
		}

		ar->dfs_sub_channel_marking = enable;
		ret = 0;

		if (ar->ah->state != ATH12K_HW_STATE_ON) {
			break;
		}

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SUB_CHANNEL_MARKING,
						enable ? 1 : 0, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set sub channel marking: %d\n", ret);
			break;
		}

		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_CTLPWRSCALE:
		ret = ath12k_vendor_set_ctlpwrscale(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_EN_CHAN_144:
		ret = ath12k_vendor_chan144_apply(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_CHAN_COEX_DISABLE:
		ret = ath12k_vendor_set_chan_coex_disable(ar, (u8)value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_TO_REO_DEST:
		if (value < ATH12K_REO2SW0_RING || value > ATH12K_REO_RELEASE_RING) {
			ath12k_err(NULL, "REO dest ring value should be between %d and %d\n",
					ATH12K_REO2SW0_RING, ATH12K_REO_RELEASE_RING);
                        ret = -EINVAL;
                } else {
                        ar->radio_cfg.pdev_to_reo_dest = value;
                        ret = 0;
                }
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_NOL_CHAN_LIST:
	{
		struct wiphy *wiphy;
		char *path;
		u32 updated = 0;

		if (!ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
			return -EINVAL;

		wiphy = ar->ah->hw->wiphy;
		ret = ath12k_vendor_resolve_nol_path_from_data_extn(params->data,
								    params->data_len,
								    params->radio_idx,
								    &path);
		if (ret)
			return ret;

		ret = ath12k_vendor_set_nol_chan_list_extn(wiphy,
							   params->radio_idx,
							   path, &updated);
		kfree(path);
		if (ret)
			return ret;

		ret = ath12k_vendor_reply_u32_extn(wiphy, updated);
		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_WSI_BYPASS:
	{
	        int i;
	        struct ath12k_hw_group *ag;
	        struct ath12k_base *ab;
	        struct ath12k_wsi_remap_work *w;

	        ath12k_err(ar->ab,
	                   "set_radio_params: WSI_BYPASS param=%u value=%u\n",
	                   param, value);

	        /* Expect val: 1 = bypass(remove) device, 2 = re-add device */
	        if (value != ATH12K_WSI_BYPASS_REMOVE_DEVICE &&
	            value != ATH12K_WSI_BYPASS_ADD_DEVICE) {
	                ath12k_err(ar->ab,
	                           "set_radio_params: invalid WSI_BYPASS value=%u\n",
	                           value);
	                return -EINVAL;
	        }

	        ab = ar->ab;
	        if (!ab) {
	                ath12k_err(NULL, "set_radio_params: ab is NULL\n");
	                return -EINVAL;
	        }

	        /* Reject if any device is statically bypassed */
	        ag = ab->ag;
	        if (!ag) {
	                ath12k_err(ab, "set_radio_params: ag is NULL\n");
	                return -EINVAL;
	        }

	        for (i = 0; i < ag->num_devices; i++) {
	                struct ath12k_base *partner_ab = ag->ab[i];
	                if (partner_ab && partner_ab->is_static_bypassed) {
	                        ath12k_err(ab,
	                                   "set_radio_params: reject, static bypassed dev idx=%d\n",
	                                   i);
	                        return -EINVAL;
	                }
	        }

	        mutex_lock(&ab->core_lock);
	        ret = ath12k_wsi_bypass_precheck(ab, value);
	        if (ret) {
	                ath12k_err(ab,
	                           "set_radio_params: wsi_bypass_precheck failed ret=%d value=%u state=%u is_bypassed=%d\n",
	                           ret, value, ab->wsi_remap_state, ab->is_bypassed);
	                mutex_unlock(&ab->core_lock);
	                return ret;
	        }

	        ab->wsi_remap_state = value;

	        /* Redundant safety: if already bypassed and request is remove */
	        if (ab->is_bypassed &&
	                ab->wsi_remap_state == ATH12K_WSI_BYPASS_REMOVE_DEVICE) {
	                ath12k_err(ab,
	                           "set_radio_params: reject redundant remove (is_bypassed=1)\n");
	                mutex_unlock(&ab->core_lock);
	                return -EINVAL;
	        }
	        mutex_unlock(&ab->core_lock);

	        /* Schedule bypass/remap asynchronously to avoid blocking cfg80211 */
	        w = kzalloc(sizeof(*w), GFP_KERNEL);
	        if (!w) {
	                ath12k_err(ab, "set_radio_params: kzalloc work failed\n");
	                return -ENOMEM;
	        }

	        INIT_WORK(&w->work, ath12k_wsi_bypass_workfn);
	        w->ab = ab;

	        queue_work(system_unbound_wq, &w->work);

	        return 0;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ALPHA2:
		u8 alpha2[REG_ALPHA2_LEN];
		bool regdom_changed;

		alpha2[0] = (value >> 8) & 0xFF;
		alpha2[1] = value & 0xFF;
		regdom_changed = ath12k_vendor_regdom_changes(
			wiphy_to_ieee80211_hw(wiphy), alpha2);
		ret = ath12k_vendor_set_country_alpha2(wiphy, alpha2);
		if (!ret && regdom_changed)
			*reload = true;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ID:
		bool country_changed = ar->country_id != (u16)value;

		ret = ath12k_vendor_set_country_id(ar, value);
		if (!ret && country_changed)
			*reload = true;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_CAC_TIMEOUT:
	{
		s32 cac_timeout_sec = value;
		s32 cac_timeout_ms;
		s32 prev_cac_timeout_ms;

		if (!ath12k_is_timeout_sec_valid(ar->ab, cac_timeout_sec, "CAC", false))
			return -EINVAL;

		cac_timeout_ms = cac_timeout_sec * ATH12K_MSEC_PER_SEC;
		prev_cac_timeout_ms = ar->ar_extn.cac_timeout_override_ms;
		ath12k_set_current_cac_timeout(ar, cac_timeout_ms);

		ret = ath12k_apply_cac_timeout_override(ar, (u32)cac_timeout_ms);
		if (ret) {
			ath12k_set_current_cac_timeout(ar, prev_cac_timeout_ms);
			return ret;
		}

		ret = 0;
		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_BGCAC_TIMEOUT:
	{
		u32 timeout_ms = value * ATH12K_MSEC_PER_SEC;

		if (!ath12k_is_timeout_sec_valid(ar->ab, value, "BG CAC", false))
			return -EINVAL;

		ret = ath12k_set_current_bgcac_timeout(ar, timeout_ms);
		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_RX_FLOW_TAG_OP:
	{
		struct ath12k_base *ab = ar->ab;
		struct rx_flow_info flow_info = {0};
		struct hal_flow_tuple_info *ti = &flow_info.flow_tuple_info;
		const struct rx_flow_blob *blob;
		u8 op_code, ip_ver, proto;
		char src_ip_str[41], dst_ip_str[41];
		u8 src_ip[16] = {0}, dst_ip[16] = {0};

		if (!params->data || params->data_len < sizeof(struct rx_flow_blob)) {
			ath12k_err(ab, "rx_flow_tag_op: invalid rx_flow_info_blob len %u (need %zu)\n",
				   params->data_len, sizeof(struct rx_flow_blob));
			return -EINVAL;
		}

		blob = (const struct rx_flow_blob *)params->data;

		/* Unpack header */
		op_code = blob->op_code;
		ip_ver  = blob->ip_ver;
		proto   = blob->proto;

		memcpy(src_ip_str, blob->src_ip_str, sizeof(blob->src_ip_str));
		src_ip_str[sizeof(blob->src_ip_str)] = '\0';
		memcpy(dst_ip_str, blob->dst_ip_str, sizeof(blob->dst_ip_str));
		dst_ip_str[sizeof(blob->dst_ip_str)] = '\0';

		/* Parse and validate IP addresses using kernel helpers */
		if (ip_ver == IP_VERSION_4) {
			if (!in4_pton(src_ip_str, -1, src_ip, -1, NULL) ||
			    !in4_pton(dst_ip_str, -1, dst_ip, -1, NULL)) {
				ath12k_err(ab, "rx_flow_tag_op: invalid IPv4 addr src='%s' dst='%s'\n",
					   src_ip_str, dst_ip_str);
				return -EINVAL;
			}
		} else if (ip_ver == IP_VERSION_6) {
			if (!in6_pton(src_ip_str, -1, src_ip, -1, NULL) ||
			    !in6_pton(dst_ip_str, -1, dst_ip, -1, NULL)) {
				ath12k_err(ab, "rx_flow_tag_op: invalid IPv6 addr src='%s' dst='%s'\n",
					   src_ip_str, dst_ip_str);
				return -EINVAL;
			}
		}

		/* Pack into flow_tuple_info */
		ti->l4_protocol  = proto;
		ti->src_port     = le16_to_cpu(blob->sport);
		ti->dest_port    = le16_to_cpu(blob->dport);
		flow_info.fse_metadata = le16_to_cpu(blob->tag);
		flow_info.drop   = blob->drop_flow ? 1 : 0;
		flow_info.ring_id = blob->ring_id;

		if (ip_ver == IP_VERSION_4) {
			flow_info.is_addr_ipv4 = 1;
			ti->src_ip_31_0  = get_unaligned_be32(src_ip);
			ti->dest_ip_31_0 = get_unaligned_be32(dst_ip);
		} else if (ip_ver == IP_VERSION_6) {
			ti->src_ip_127_96  = get_unaligned_be32(&src_ip[0]);
			ti->src_ip_95_64   = get_unaligned_be32(&src_ip[4]);
			ti->src_ip_63_32   = get_unaligned_be32(&src_ip[8]);
			ti->src_ip_31_0    = get_unaligned_be32(&src_ip[12]);
			ti->dest_ip_127_96 = get_unaligned_be32(&dst_ip[0]);
			ti->dest_ip_95_64  = get_unaligned_be32(&dst_ip[4]);
			ti->dest_ip_63_32  = get_unaligned_be32(&dst_ip[8]);
			ti->dest_ip_31_0   = get_unaligned_be32(&dst_ip[12]);
		}

		ath12k_dbg(ab, ATH12K_DBG_DP_FST,
			   "rx_flow_tag: op=%u ip_ver=%u sport=%u dport=%u proto=%u meta=0x%x drop=%u ring=%u\n",
			   op_code, ip_ver,
			   ti->src_port, ti->dest_port, ti->l4_protocol,
			   flow_info.fse_metadata, flow_info.drop, flow_info.ring_id);

		if (op_code == RX_FLOW_TAG_ADD) {
			ret = ath12k_dp_rx_flow_add_entry(ab, &flow_info);
			if (ret)
				ath12k_err(ab, "rx_flow_tag: add failed ret=%d\n", ret);
		} else if (op_code == RX_FLOW_TAG_DEL) {
			ret = ath12k_dp_rx_flow_delete_entry(ab, &flow_info);
			if (ret)
				ath12k_err(ab, "rx_flow_tag: delete failed ret=%d\n", ret);
		} else if (op_code == RX_FLOW_TAG_DUMP) {
			struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
			char *dump_buf, *line, *cursor;
			ssize_t dump_len;

			if (dp && dp->dp_hw_grp && dp->dp_hw_grp->fst)
				ath12k_dp_arch_rx_flow_dump_entry(dp, &flow_info);

			dump_buf = kzalloc(ATH12K_RX_FLOW_TAG_DUMP_BUF_SIZE, GFP_KERNEL);
			if (!dump_buf) {
				ath12k_err(ab, "rx_flow_tag_op: failed to allocate dump buffer\n");
				ret = -ENOMEM;
				break;
			}
			dump_len = ath12k_dp_dump_fst_table(ab, dump_buf,
							    ATH12K_RX_FLOW_TAG_DUMP_BUF_SIZE);
			if (dump_len > 0) {
				ath12k_info(ab, "rx_flow_tag_op: fst_dump_table:\n");
				cursor = dump_buf;
				while ((line = strsep(&cursor, "\n")) != NULL) {
					if (*line)
						ath12k_info(ab, "%s\n", line);
				}
			} else {
				ath12k_warn(ab,
					    "rx_flow_tag_op: fst_dump_table empty or failed len=%zd\n",
					    dump_len);
			}
			kfree(dump_buf);
			ret = 0;
		}
		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_BLOCK_DFS_LIST:
		ret = ath12k_set_block_dfs_channels(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_STATS_TIMER:
		ar->pdev_stats_timer_interval = value;
		if (value) {
			wiphy_delayed_work_queue(ath12k_ar_to_hw(ar)->wiphy,
						 &ar->pdev_stats_timer,
						 msecs_to_jiffies(value));
		} else {
			wiphy_delayed_work_cancel(ath12k_ar_to_hw(ar)->wiphy,
						  &ar->pdev_stats_timer);
		}
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "pdev stats timer interval set to %u ms for pdev %u\n",
			   value, ar->pdev_idx);
		ret = 0;
		break;
        default:
                ath12k_dbg(NULL, ATH12K_DBG_CFG,
                           "Un-supported param: %d\n", param);
                break;
        }

        return ret;
}

/* Find the link-vif of the wdev with the link id to set params
 * If link id is invalid use the default link
 */
int ath12k_vendor_set_wifi_params_extn(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif = NULL;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah = NULL;
        struct ath12k *ar;
        u32 *data;
        u32 param;
        bool reload = false;
        u32 value;
        int ret = -1;

        lockdep_assert_wiphy(wiphy);

	data = (u32 *)params->data;
	param = params->value;

        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif || !data)
                return -EINVAL;

        value = *data;

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif)
                return -EINVAL;

        ah = ahvif->ah;

        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else {
                if (params->link_id < ATH12K_NUM_MAX_LINKS)
                        arvif = rcu_dereference_protected(ahvif->link[params->link_id],
							  lockdep_is_held(&wiphy->mtx));
        }
        if (!arvif || !arvif->ar) {
                return -EINVAL;
        }
        ar = arvif->ar;

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
                   "vif: %p param: %d value: %d if: %d link: %d\n",
                   vif, param, value,
                   params->ifindex, params->link_id);

        ret = ath12k_vendor_set_arvif_params(arvif, param, value, &reload, params->flags,
					     params->data, params->data_len);

        if (!ret && reload)
                ath12k_vendor_event_iface_reload_link(wiphy, wdev, params->link_id);

        return ret;
}

/*
 *  Enable per-peer sounding interval via WMI
 */
int ath12k_vendor_enable_sounding_extn(struct wiphy *wiphy, struct wireless_dev *wdev, struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif = NULL;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah = NULL;
        struct ath12k *ar;
        int ret = -1;
        u8  mac_addr[ETH_ALEN];
        u32 enable;
        u32 vdev_id = 0;

        lockdep_assert_wiphy(wiphy);

        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif)
                return -EINVAL;

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif)
                return -EINVAL;

        ah = ahvif->ah;

        rcu_read_lock();
        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else {
                if (params->link_id < ATH12K_NUM_MAX_LINKS)
                        arvif = rcu_dereference(ahvif->link[params->link_id]);
        }
        if (!arvif || !arvif->ar) {
                rcu_read_unlock();
                return -EINVAL;
        }

        ar = arvif->ar;
        vdev_id = arvif->vdev_id;
        rcu_read_unlock();

       if (params->data_len != ETH_ALEN) {
        ath12k_err(NULL, "Invalid radio MAC address len: %u\n", params->data_len);
        return -EINVAL;
       }
       if (!is_valid_ether_addr(params->data)) {
               ath12k_err(NULL, "Invalid radio MAC address\n");
               return -EINVAL;
       }
       ether_addr_copy(mac_addr, (u8 *)params->data);

       enable = !!params->value;
       ath12k_dbg(NULL, ATH12K_DBG_CFG,
                "hwaddr: %p link: %d, vdev-id : %d, enable: %d\n",
                 mac_addr, params->link_id, vdev_id, enable);

       ret = ath12k_wmi_set_peer_param(ar, mac_addr, vdev_id, WMI_PEER_PARAM_SOUNDING_ENABLE, enable);
       if (ret) {
            ath12k_err(NULL, "WMI FAILED\n");
       }
       else {
            ath12k_err(NULL,"WMI OK\n");
       }
       return ret;
}

/*
 * Set per-peer SU sounding interval via WMI
 */
int ath12k_vendor_set_sounding_params_extn(struct wiphy *wiphy, struct wireless_dev *wdev, struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif = NULL;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah = NULL;
        struct ath12k *ar;
        int ret = -1;
        u8  mac_addr[ETH_ALEN];
        u32 interval_ms;
        u32 vdev_id = 0;

        lockdep_assert_wiphy(wiphy);
        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif)
                return -EINVAL;
        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif)
                return -EINVAL;

        ah = ahvif->ah;
        rcu_read_lock();
        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else {
                if (params->link_id < ATH12K_NUM_MAX_LINKS)
                        arvif = rcu_dereference(ahvif->link[params->link_id]);
        }
        if (!arvif || !arvif->ar) {
                rcu_read_unlock();
                return -EINVAL;
        }
        ar = arvif->ar;
        vdev_id = arvif->vdev_id;
        rcu_read_unlock();
       if (params->data_len != ETH_ALEN) {
        ath12k_err(NULL, "Invalid radio MAC address len: %u\n", params->data_len);
        return -EINVAL;
       }
       if (!is_valid_ether_addr(params->data)) {
               ath12k_err(NULL, "Invalid radio MAC address\n");
               return -EINVAL;
       }
       ether_addr_copy(mac_addr, (u8 *)params->data);
       interval_ms = params->value;
       ath12k_dbg(NULL, ATH12K_DBG_CFG,
                  "hwaddr: %p, link: %d, vdev-id : %d, interval_ms: %d\n",
                   mac_addr,  params->link_id, vdev_id, interval_ms);

       ret = ath12k_wmi_set_peer_param(ar, mac_addr, vdev_id, WMI_PEER_PARAM_SU_SET_SOUNDING_INTERVAL, interval_ms);
       if (ret) {
          ath12k_err(NULL, "WMI FAILED\n");
       }
       else {
       ath12k_err(NULL, "WMI OK\n");
       }
       return ret;
}

/*
 * Set per-peer MU sounding interval via WMI
 */
int ath12k_vendor_set_mu_sounding_params_extn(struct wiphy *wiphy, struct wireless_dev *wdev, struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif = NULL;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah = NULL;
        struct ath12k *ar;
        int ret = -1;
        u8  mac_addr[ETH_ALEN];
        u32 interval_ms;
        u32 vdev_id = 0;

        lockdep_assert_wiphy(wiphy);

        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif)
                return -EINVAL;

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif)
                return -EINVAL;

        ah = ahvif->ah;

        rcu_read_lock();
        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else {
                if (params->link_id < ATH12K_NUM_MAX_LINKS)
                        arvif = rcu_dereference(ahvif->link[params->link_id]);
        }
        if (!arvif || !arvif->ar) {
                rcu_read_unlock();
                return -EINVAL;
        }

        ar = arvif->ar;
        vdev_id = arvif->vdev_id;
        rcu_read_unlock();

       if (params->data_len != ETH_ALEN) {
        ath12k_err(NULL, "Invalid radio MAC address len: %u\n", params->data_len);
        return -EINVAL;
       }

       if (!is_valid_ether_addr(params->data)) {
               ath12k_err(NULL, "Invalid radio MAC address\n");
               return -EINVAL;
       }

       ether_addr_copy(mac_addr, (u8 *)params->data);
       interval_ms = params->value;

       ath12k_dbg(NULL, ATH12K_DBG_CFG,
                  "hwaddr: %p, link: %d, vdev-id : %d, interval_ms: %d\n",
                   mac_addr,  params->link_id, vdev_id, interval_ms);

       ret = ath12k_wmi_set_peer_param(ar, mac_addr, vdev_id, WMI_PEER_PARAM_MU_SET_SOUNDING_INTERVAL, interval_ms);

       if (ret) {
            ath12k_err(NULL,"WMI FAILED\n");
        }
       else {
           ath12k_err(NULL,"WMI OK\n");
       }
       return ret;
}

/*
 * Enable per-peer Sched mu via WMI
 */
int ath12k_vendor_sched_mu_enable(struct wiphy *wiphy,
                                  struct wireless_dev *wdev,
                                  struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah;
        struct ath12k *ar;
        u8  mac_addr[ETH_ALEN];
        u32 enable;
        u32 vdev_id;
        int ret;

        lockdep_assert_wiphy(wiphy);

        if (!wdev || !params) {
                ath12k_err(NULL, "Invalid args wdev=%p params=%p\n", wdev, params);
                return -EINVAL;
        }

        if (!params->data || params->data_len != ETH_ALEN) {
                ath12k_err(NULL, "Invalid MAC len=%u\n", params->data_len);
                return -EINVAL;
        }
        if (!is_valid_ether_addr(params->data)) {
            ath12k_err(NULL, "Invalid MAC %pM\n", (u8 *)params->data);
            return -EINVAL;
        }
        ether_addr_copy(mac_addr, (const u8 *)params->data);

        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif) {
                ath12k_err(NULL, "vif is NULL\n");
                return -EINVAL;
        }

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif) {
                ath12k_err(NULL, "ahvif is NULL\n");
                return -EINVAL;
        }

        ah = ahvif->ah;

        rcu_read_lock();
        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else if (params->link_id < ATH12K_NUM_MAX_LINKS) {
                arvif = rcu_dereference(ahvif->link[params->link_id]);
        }
        if (!arvif || !arvif->ar) {
                rcu_read_unlock();
                ath12k_err(NULL, "arvif/ar NULL (link=%d)\n", params->link_id);
                return -EINVAL;
        }

        ar = arvif->ar;
        vdev_id = arvif->vdev_id;
        rcu_read_unlock();

        enable = !!params->value;

        ath12k_dbg(NULL, ATH12K_DBG_CFG,
                 "hwaddr: %p link: %d, vdev-id : %d, enable: %d\n",
                 mac_addr, params->link_id, vdev_id, enable);

        ret = ath12k_wmi_set_peer_param(ar, mac_addr, vdev_id,
                                        WMI_PEER_PARAM_MU_SCHED_ENABLE,
                                        enable);
        if (ret) {
               ath12k_err(NULL,"WMI FAILED\n");
        }
        else {
            ath12k_err(NULL,"WMI OK\n");
        }
        return ret;
}

/*
 * Enable Sched ofdma enable via WMI
 */
int ath12k_vendor_sched_ofdma_enable(struct wiphy *wiphy,
                                  struct wireless_dev *wdev,
                                  struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah;
        struct ath12k *ar;
        u8  mac_addr[ETH_ALEN];
        u32 enable;
        u32 vdev_id;
        int ret;

        lockdep_assert_wiphy(wiphy);

        if (!wdev || !params) {
                ath12k_err(NULL, "Invalid args wdev=%p params=%p\n", wdev, params);
                return -EINVAL;
        }

        if (!params->data || params->data_len != ETH_ALEN) {
                ath12k_err(NULL, "Invalid MAC len=%u\n", params->data_len);
                return -EINVAL;
        }
        if (!is_valid_ether_addr(params->data)) {
            ath12k_err(NULL, "Invalid MAC %pM\n", (u8 *)params->data);
            return -EINVAL;
        }
        ether_addr_copy(mac_addr, (const u8 *)params->data);

        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif) {
                ath12k_err(NULL, "vif is NULL\n");
                return -EINVAL;
        }

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif) {
                ath12k_err(NULL, "ahvif is NULL\n");
                return -EINVAL;
        }

        ah = ahvif->ah;

        rcu_read_lock();
        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else if (params->link_id < ATH12K_NUM_MAX_LINKS) {
                arvif = rcu_dereference(ahvif->link[params->link_id]);
        }
        if (!arvif || !arvif->ar) {
                rcu_read_unlock();
                ath12k_err(NULL, "arvif/ar NULL (link=%d)\n", params->link_id);
                return -EINVAL;
        }

        ar = arvif->ar;
        vdev_id = arvif->vdev_id;
        rcu_read_unlock();

        enable = !!params->value;

        ath12k_dbg(NULL, ATH12K_DBG_CFG,
                   "MU-SCHED RESOLVED: ar=%p vdev_id=%u peer=%pM enable=%u ifidx=%d link=%d\n",
                   ar, vdev_id, mac_addr, enable, params->ifindex, params->link_id);

        /* Apply via WMI: MU scheduler enable/disable */
        ret = ath12k_wmi_set_peer_param(ar, mac_addr, vdev_id,
                                        WMI_PEER_PARAM_OFDMA_SCHED_ENABLE,
                                        enable);
        if (ret)
                ath12k_err(NULL,"WMI FAILED\n");
        else
                ath12k_err(NULL,"WMI OK\n");
        return ret;
}


/*
 *  Set per-peer Nav_Override_Config via WMI
 */
int ath12k_vendor_set_nav_override_config_extn(struct wiphy *wiphy,
                                               struct wireless_dev *wdev,
                                               struct ath12k_wifi_generic_params *params)
{
    struct ieee80211_vif *vif;
    struct ath12k_vif *ahvif;
    struct ath12k_link_vif *arvif = NULL;
    struct ath12k *ar;
    u32 status = 0, threshold = 0, cfg;
    int ret;


    if (!wdev || !params) {
        ath12k_err(NULL, "NAV-SET ERROR: wdev or params NULL\n");
        return -EINVAL;
    }

    /*
     * Correct decode for cfg80211tool mapping:
     *  - status is encoded in packed data (lower 2 bits)
     *  - threshold comes from params->value (CLI second argument)
     */
    if (params->data && params->data_len >= sizeof(u32)) {
        u32 v = *(const u32 *)params->data;
        status = v & 0x3;
        threshold = v >> 2;

        if (params->value)
            threshold = params->value;

    }
    else {
        ath12k_err(NULL, "NAV-SET ERROR: missing packed data\n");
        return -EINVAL;
    }

    if (status > 3) {
        ath12k_err(NULL, "NAV-SET ERROR: invalid status=%u\n", status);
        return -EINVAL;
    }

    cfg = (threshold << 2) | (status & 0x3);

    /* Resolve vif */
    vif = wdev_to_ieee80211_vif(wdev);
    if (!vif) {
        ath12k_err(NULL, "NAV-SET ERROR: vif NULL\n");
        return -EINVAL;
    }

    ahvif = (struct ath12k_vif *)vif->drv_priv;
    if (!ahvif) {
        ath12k_err(NULL, "NAV-SET ERROR: ahvif NULL\n");
        return -EINVAL;
    }

    /* Resolve MLO link */
    if (params->link_id == INVALID_LINK_ID) {
        arvif = &ahvif->deflink;
    } else if (params->link_id < IEEE80211_MLD_MAX_NUM_LINKS) {
        arvif = rcu_dereference_protected(ahvif->link[params->link_id],
					  lockdep_is_held(&wiphy->mtx));
    }

    if (!arvif || !arvif->ar) {
        ath12k_err(NULL,
            "NAV-SET ERROR: arvif/ar NULL for link=%u\n",
            params->link_id);
        return -EINVAL;
    }

    ar = arvif->ar;

    /* Send WMI */
    ret = ath12k_wmi_pdev_set_param(ar,
                                    WMI_PDEV_PARAM_NAV_OVERRIDE_CONFIG,
                                    cfg,
                                    ar->pdev->pdev_id);
    if (ret) {
        ath12k_err(NULL,"WMI FAILED\n");
        return ret;
    }
    else {
        ath12k_err(NULL, "WMI OK\n");
    }

    /* Cache for GET */
    arvif->nav_status = status;
    arvif->nav_threshold = threshold;
    return ret;
}

int ath12k_vendor_get_nav_override_config_extn(struct wiphy *wiphy,
                                               struct wireless_dev *wdev,
                                               struct ath12k_wifi_generic_params *params)
{
    struct ieee80211_vif *vif;
    struct ath12k_vif *ahvif;
    struct ath12k_link_vif *arvif = NULL;
    struct sk_buff *skb;
    u32 combined;
    u32 nav_status, nav_threshold;

    if (!wdev) {
        ath12k_err(NULL, "NAV-GET ERROR: wdev NULL\n");
        return -EINVAL;
    }

    /* Resolve vif */
    vif = wdev_to_ieee80211_vif(wdev);
    if (!vif) {
        ath12k_err(NULL, "vif NULL\n");
        return -EINVAL;
    }

    ahvif = (struct ath12k_vif *)vif->drv_priv;
    if (!ahvif) {
        ath12k_err(NULL, "ahvif NULL\n");
        return -EINVAL;
    }

    /* Resolve link */
    if (!params || params->link_id == INVALID_LINK_ID) {
        arvif = &ahvif->deflink;
        ath12k_err(NULL, "Using deflink\n");
    } else if (params->link_id < IEEE80211_MLD_MAX_NUM_LINKS) {
        arvif = rcu_dereference_protected(ahvif->link[params->link_id],
					  lockdep_is_held(&wiphy->mtx));
        ath12k_err(NULL, "Using link=%u\n", params->link_id);
    }

    if (!arvif) {
        ath12k_err(NULL, "arvif NULL\n");
        return -EINVAL;
    }

    nav_status = arvif->nav_status;
    nav_threshold = arvif->nav_threshold;
    combined = (nav_status & 0x3) | (nav_threshold << 2);

    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
    if (!skb) {
        ath12k_err(NULL, "Skb alloc failed\n");
        return -ENOMEM;
    }

    /* Generic reply (kernel‑correct) */
    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, combined) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, sizeof(u32)) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0)) {
        kfree_skb(skb);
        ath12k_err(NULL, "Generic nla_put failed\n");
        return -EINVAL;
    }

    /* Tool‑specific attributes (cfg80211tool expects these for ID 74) */
    if (nla_put_u32(skb, 3 , combined) ||             /* dparam0 */
        nla_put_u32(skb, 1 , nav_status) ||           /* value0 */
        nla_put_u32(skb, 2 , nav_threshold)) {        /* value1 */
        kfree_skb(skb);
        ath12k_err(NULL, "tool attr nla_put failed\n");
        return -EINVAL;
    }

    return cfg80211_vendor_cmd_reply(skb);
}

/*
 * Set Ulofdma_rtd via WMI
 */
int ath12k_vendor_peer_ulofdma_rtd_extn(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_vif *vif = NULL;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k_hw *ah = NULL;
        struct ath12k *ar;
        u8  mac_addr[ETH_ALEN];
        u32 rtd_ms;
        u32 vdev_id = 0;
        int ret = -1;

        lockdep_assert_wiphy(wiphy);

        if (!wdev || !params || !params->data) {
                ath12k_err(NULL, "Invalid input args\n");
                return -EINVAL;
        }

        /* Validate MAC */
        if (params->data_len != ETH_ALEN) {
                ath12k_err(NULL, "Invalid MAC address len: %u\n", params->data_len);
                return -EINVAL;
        }
        if (!is_valid_ether_addr(params->data)) {
                ath12k_err(NULL, "Invalid MAC address\n");
                return -EINVAL;
        }
        ether_addr_copy(mac_addr, (u8 *)params->data);

        /* Validate value (0 - 600) */
        rtd_ms = params->value;
        if (rtd_ms > 600) {
                ath12k_err(NULL, "Invalid value %u (range 0..600)\n", rtd_ms);
                return -EINVAL;
        }

        vif = wdev_to_ieee80211_vif(wdev);
        if (!vif)
                return -EINVAL;

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif)
                return -EINVAL;

        ah = ahvif->ah;

        rcu_read_lock();
        if (ah && params->link_id == INVALID_LINK_ID) {
                arvif = &ahvif->deflink;
        } else {
                if (params->link_id < ATH12K_NUM_MAX_LINKS)
                        arvif = rcu_dereference(ahvif->link[params->link_id]);
        }
        if (!arvif || !arvif->ar) {
                rcu_read_unlock();
                ath12k_err(NULL, "arvif/ar NULL (link=%d)\n", params->link_id);
                return -EINVAL;
        }

        ar = arvif->ar;
        vdev_id = arvif->vdev_id;
        rcu_read_unlock();

        ath12k_dbg(NULL, ATH12K_DBG_CFG,
                   "UL-RTD extn RESOLVED: ar=%p vdev_id=%u peer=%p rtd=%u ifidx=%d link=%d\n",
                   ar, vdev_id, mac_addr, rtd_ms, params->ifindex, params->link_id);

        /* Send via WMI */
        ret = ath12k_wmi_set_peer_param(ar, mac_addr, vdev_id,
                                        WMI_PDEV_PARAM_UL_OFDMA_RTD,
                                        rtd_ms);
        if (ret)
                ath12k_err(NULL,"WMI FAILED\n");
        else
                ath12k_err(NULL,"WMI OK\n");

        return ret;
}

/**
 * ath12k_vendor_get_he_mcs_12_13_cap - Read pdev HE MCS 12/13 capability.
 * @ah:        ath12k_hw instance
 * @tb:        parsed netlink attribute table
 * @skb:       reply skb to fill
 *
 * Reads the pdev HE MCS 12/13 hardware capability from
 * (U32 bits)band_cap->he_cap_info[1] bits[31:16] (the extended HE MAC cap
 * word populated by firmware.
 *
 * Format returned (2 bytes via GENERIC_DATA):
 *   byte[0] = bits[23:16] = L80 NSS bitmap (<=80 MHz)
 *   byte[1] = bits[31:24] = G80 NSS bitmap (>80 MHz)
 *
 * Returns 0 on success, negative error code on failure.
 */
static int ath12k_vendor_get_he_mcs_12_13_cap(struct ath12k_hw *ah, struct nlattr **tb,
					      u64 *value)
{
	struct ath12k *ar;
	struct ath12k_band_cap *cap_band;
	enum nl80211_band band;
	u8 radio_idx;

	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]) {
		ath12k_err(NULL, "HE MCS 12/13: radio index not specified\n");
		return -EINVAL;
	}

	radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);
	if (radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "HE MCS 12/13: invalid radio index %u\n", radio_idx);
		return -EINVAL;
	}

	ar = &ah->radio[radio_idx];
	if (ar->pdev->cap.supported_bands & BIT(NL80211_BAND_5GHZ))
		band = NL80211_BAND_5GHZ;
	else if (ar->pdev->cap.supported_bands & BIT(NL80211_BAND_6GHZ))
		band = NL80211_BAND_6GHZ;
	else
		band = NL80211_BAND_2GHZ;

	cap_band = &ar->pdev->cap.band[band];
	ar->ar_extn.he_mcs_12_13_radio_cap = (u16)(cap_band->he_cap_info[1] >> 16);
	*value = ar->ar_extn.he_mcs_12_13_radio_cap;
	ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
		   "HE MCS 12/13 capability = 0x%04x radio_idx = %u band = %d\n",
		   ar->ar_extn.he_mcs_12_13_radio_cap, radio_idx, band);

	return 0;
}

/**
 * ath12k_vendor_set_he_mcs_12_13_peer_cap - Store intersected peer HE MCS 12/13
 * NSS capability for the AP.
 *
 * @wiphy:       wiphy pointer
 * @wifi_params: parsed generic command parameters
 *
 * hostapd parses the associating STA's Association Request QCN vendor IE and
 * pushes the result down via NL vendor command. Stores the intersected peer
 * capability with the radio's own hardware capability for use during the
 * WMI peer-assoc command.
 *
 * Returns 0 on success, negative error code on failure.
 */
static int ath12k_vendor_set_he_mcs_12_13_peer_cap(struct wiphy *wiphy,
						   struct ath12k_wifi_generic_params *wifi_params)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	u8 radio_idx = wifi_params->radio_idx;
	__le16 peer_cap_le;

	if (radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "HE MCS 12/13 SET: invalid radio index %u\n", radio_idx);
		return -EINVAL;
	}

	ar = &ah->radio[radio_idx];
	if (!wifi_params->data || wifi_params->data_len < sizeof(peer_cap_le)) {
		ath12k_err(ar->ab, "HE MCS 12/13 SET: missing or short data\n");
		return -EINVAL;
	}
	memcpy(&peer_cap_le, wifi_params->data, sizeof(peer_cap_le));
	ar->ar_extn.he_mcs_12_13_peer_cap =
		ar->ar_extn.he_mcs_12_13_radio_cap & le16_to_cpu(peer_cap_le);

	ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
		   "HE MCS 12/13: peer capability = 0x%04x radio_idx=%u\n",
		   ar->ar_extn.he_mcs_12_13_peer_cap, radio_idx);

	return 0;
}

void ath12k_peer_assoc_h_he_mcs_12_13_extn(struct ath12k *ar,
					   struct ath12k_wmi_peer_assoc_arg *arg,
					   struct ieee80211_link_sta *link_sta)
{
	u16 radio_cap, peer_cap, intersected, rx_cap, tx_cap;
	u32 rx_packed, tx_packed;
	int i;

	if (!link_sta->he_mcs_12_13_peer_cap_valid) {
		link_sta->he_mcs_12_13_peer_cap = ar->ar_extn.he_mcs_12_13_peer_cap;
		link_sta->he_mcs_12_13_peer_cap_valid = true;
	}

	radio_cap = ar->ar_extn.he_mcs_12_13_radio_cap;
	peer_cap = link_sta->he_mcs_12_13_peer_cap;
	intersected = radio_cap & peer_cap;
	rx_cap = intersected;
	tx_cap = (ar->ab->hw_rev == ATH12K_HW_IPQ5332_HW10) ? 0 : intersected;
	rx_packed = WMI_HE_EXTRA_MCS_PACK(rx_cap);
	tx_packed = WMI_HE_EXTRA_MCS_PACK(tx_cap);

	for (i = 0; i < arg->peer_he_mcs_count; i++) {
		arg->peer_he_rx_mcs_set[i] &=
			~(WMI_HE_EXTRA_MCS_L80_MASK | WMI_HE_EXTRA_MCS_G80_MASK);
		arg->peer_he_rx_mcs_set[i] |= rx_packed;
		arg->peer_he_tx_mcs_set[i] &=
			~(WMI_HE_EXTRA_MCS_L80_MASK | WMI_HE_EXTRA_MCS_G80_MASK);
		arg->peer_he_tx_mcs_set[i] |= tx_packed;
	}
}

static int ath12k_vendor_set_bssid_extn(struct wireless_dev *wdev,
					struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;

	if (!params->data || params->data_len != ETH_ALEN)
		return -EINVAL;

	if (is_broadcast_ether_addr(params->data) ||
	    is_multicast_ether_addr(params->data))
		return -EINVAL;

	if (wdev->iftype != NL80211_IFTYPE_STATION)
		return -EOPNOTSUPP;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);

	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	if (is_zero_ether_addr(params->data)) {
		eth_zero_addr(ahvif->ath12k_vif_extn.bssid);
		ahvif->ath12k_vif_extn.bssid_set = false;
	} else {
		ether_addr_copy(ahvif->ath12k_vif_extn.bssid, params->data);
		ahvif->ath12k_vif_extn.bssid_set = true;
	}
	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
	return 0;
}

static struct ath12k *ath12k_get_scan_radio_ar(struct wiphy *wiphy)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	int i;

	for (i = 0; i < ah->num_radio; i++) {
		if (ath12k_scan_radio_blanking_supported(ah->radio[i].pdev))
			return &ah->radio[i];
	}
	return NULL;
}

static int ath12k_vendor_set_scan_blanking_mode_extn(
		struct wiphy *wiphy,
		struct ath12k_wifi_generic_params *params)
{
	struct ath12k *ar;
	u32 *data = (u32 *)params->data;
	u8 mode;
	int ret;

	if (!data)
		return -EINVAL;

	ar = ath12k_get_scan_radio_ar(wiphy);
	if (!ar) {
		ath12k_err(NULL, "No scan radio with blanking support found\n");
		return -EOPNOTSUPP;
	}

	mode = (u8)*data;
	if (mode >= 3) {
		ath12k_err(ar->ab, "Invalid scan blanking mode %u\n", mode);
		return -EINVAL;
	}

	if (ar->ar_extn.scan_blanking_mode == mode) {
		ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
			   "scan_blanking_mode already %u, skip\n", mode);
		return 0;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_SCAN_BLANKING_MODE,
					mode, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set scan blanking mode %u: %d\n",
			    mode, ret);
		return ret;
	}

	ar->ar_extn.scan_blanking_mode = mode;
	ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
		   "scan_blanking_mode set to %u on pdev %u\n",
		   mode, ar->pdev->pdev_id);
	return 0;
}

int ath12k_vendor_get_scan_blanking_mode_extn(struct wiphy *wiphy, u64 *value)
{
	struct ath12k *ar = ath12k_get_scan_radio_ar(wiphy);

	if (!ar)
		return -EOPNOTSUPP;

	*value = ar->ar_extn.scan_blanking_mode;
	ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
		   "scan_blanking_mode get: mode=%u\n",
		   ar->ar_extn.scan_blanking_mode);
	return 0;
}

int ath12k_vendor_wifi_config_handler_extn(struct wiphy *wiphy, struct wireless_dev *wdev,
					   struct ath12k_wifi_generic_params *wifi_params)
{
	int ret = 0;

	if (!wiphy || !wdev || !wifi_params) {
		ath12k_err(NULL, "NULL input params received\n");
		return -EINVAL;
	}

	switch (wifi_params->command) {
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	case QCA_NL80211_VENDOR_WIFI_GENERIC_SUBCMD_MESH_PARAMS:
		if (!wifi_params->data) {
			ath12k_err(NULL, "Invalid param command received\n");
			return -EINVAL;
		}
		if (!QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(wdev->vap_submode)) {
			ath12k_err(NULL, "Received command for vap_submode: %d\n"
				   "Command valid only for mesh vap\n",
				   wdev->vap_submode);
			return -EINVAL;
		}

		ret = ath12k_vendor_mesh_params_config_extn(wiphy, wdev, wifi_params);

		if (ret) {
			ath12k_err(NULL,"Failed to set mesh params \n");
			return ret;
		}
		break;
        case QCA_WLAN_VENDOR_WIFI_PARAM_ENABLE_SOUNDING:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_enable_sounding_extn(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
        case QCA_WLAN_VENDOR_WIFI_PARAM_SU_SND_INTERVAL:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_set_sounding_params_extn(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_WIFI_PARAM_MU_SND_INTERVAL:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_set_mu_sounding_params_extn(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_WIFI_PARAM_SCHED_MU_ENABLE:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_sched_mu_enable(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_WIFI_PARAM_SCHED_OFDMA_ENABLE:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_sched_ofdma_enable(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
	case QCA_WLAN_VENDOR_WIFI_PARAM_SET_NAV_OVERRIDE_CONFIG:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_set_nav_override_config_extn(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
         case QCA_WLAN_VENDOR_WIFI_PARAM_GET_NAV_OVERRIDE_CONFIG:
		struct ath12k_wifi_generic_params dummy = {0};
		dummy.link_id = INVALID_LINK_ID;
		ret = ath12k_vendor_get_nav_override_config_extn(wiphy, wdev, &dummy);
		if (ret) {
			ath12k_err(NULL,"Failed to set wifi params \n");
			return -EINVAL;
		}
		break;
         case QCA_WLAN_VENDOR_WIFI_PARAM_UL_OFDMA_RTD:
		if (!wifi_params->data) {
			ath12k_err(NULL,"Invalid param command received\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_peer_ulofdma_rtd_extn(wiphy, wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to ufodma rtd params \n");
			return ret;
		}
		break;
#endif
	case QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP:
		return ath12k_vendor_set_he_mcs_12_13_peer_cap(wiphy, wifi_params);
	case QCA_WLAN_VENDOR_WIFI_PARAM_ALLOW_SCAN_ON_DFS_CHAN:
		struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
		struct ieee80211_local *local = hw_to_local(hw);
		bool enable;

		if (!wifi_params->data || wifi_params->data_len < sizeof(u8)) {
			ath12k_err(NULL, "ALLOW_SCAN_ON_DFS_CHAN: missing data\n");
			return -EINVAL;
		}
		enable = !!*(u8 *)wifi_params->data;
		ieee80211_set_allow_scan_on_dfs_chan_extn(local, enable);
		break;
	case QCA_WLAN_VENDOR_WIFI_PARAM_BSSID:
		ret = ath12k_vendor_set_bssid_extn(wdev, wifi_params);
		if (ret) {
			ath12k_err(NULL,"Failed to set BSSID\n");
			return ret;
		}
		break;
	case ACFG_PARAM_RADIO_SCAN_BLANKING_MODE:
		return ath12k_vendor_set_scan_blanking_mode_extn(wiphy, wifi_params);

	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG, "Un-supported generic command\n");
		return -EOPNOTSUPP;
	}

	return ret;
}

#define ATH12K_VENDOR_NOL_LOG_LIMIT         64

/**
 * ath12k_vendor_nol_chan_matches_radio() - Check whether a channel
 * matches a requested radio
 * @wiphy: Wiphy that owns the channel table
 * @chan: Channel to evaluate
 * @radio_idx: Target radio index, or negative to match all radios
 *
 * Return: %true if @chan belongs to @radio_idx or no radio filter is set.
 */
bool
ath12k_vendor_nol_chan_matches_radio(struct wiphy *wiphy,
				     struct ieee80211_channel *chan,
				     int radio_idx)
{
	if (radio_idx < 0)
		return true;

	return cfg80211_get_hw_idx_by_chan(wiphy, chan) == radio_idx;
}

/**
 * ath12k_vendor_build_default_nol_path_extn() - Build the default NOL dump
 * path
 * @fallback: %true to build the fallback path, %false for the primary path
 * @radio_idx: Radio index encoded in the file name
 *
 * Return: Allocated path string on success, or %NULL on allocation failure.
 */
static char *ath12k_vendor_build_default_nol_path_extn(bool fallback, u8 radio_idx)
{
	return kasprintf(GFP_KERNEL,
			 fallback ? ATH12K_VENDOR_NOL_PERSIST_PATH_FALLBACK_FMT :
				    ATH12K_VENDOR_NOL_PERSIST_PATH_PRIMARY_FMT,
			 radio_idx);
}

/**
 * ath12k_vendor_validate_dump_path_extn() - Validate a NOL dump file path
 * @path: Absolute path provided for dump persistence
 *
 * Return: 0 if @path is acceptable for kernel file access, else -EINVAL.
 */
static int ath12k_vendor_validate_dump_path_extn(const char *path)
{
	if (!path || !path[0] || path[0] != '/')
		return -EINVAL;

	if (strstr(path, "/../") || strstr(path, "/..") == path ||
	    (strlen(path) >= 3 && !strcmp(path + strlen(path) - 3, "/..")))
		return -EINVAL;

	return 0;
}

/**
 * ath12k_vendor_alloc_nol_paths_extn() - Allocate default primary and
 * fallback paths
 * @radio_idx: Radio index encoded in the file names
 * @primary_path: Returned primary path string
 * @fallback_path: Returned fallback path string
 *
 * Return: 0 if both paths were allocated, or -ENOMEM on failure.
 */
static int ath12k_vendor_alloc_nol_paths_extn(u8 radio_idx,
					      char **primary_path,
					      char **fallback_path)
{
	*primary_path = ath12k_vendor_build_default_nol_path_extn(false,
								 radio_idx);
	*fallback_path = ath12k_vendor_build_default_nol_path_extn(true,
								  radio_idx);
	if (*primary_path && *fallback_path)
		return 0;

	kfree(*primary_path);
	kfree(*fallback_path);
	return -ENOMEM;
}

/**
 * ath12k_vendor_open_dump_write_file_extn() - Open a NOL dump file for
 * writing
 * @path: Requested destination path
 * @primary_path: Default primary path for this radio
 * @fallback_path: Default fallback path for this radio
 * @filp: Returned open file pointer
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_open_dump_write_file_extn(const char *path,
						   const char *primary_path,
						   const char *fallback_path,
						   struct file **filp)
{
	int ret;

	ret = ath12k_vendor_validate_dump_path_extn(path);
	if (ret)
		return ret;

	*filp = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(*filp) && PTR_ERR(*filp) == -ENOENT &&
	    !strcmp(path, primary_path)) {
		ret = ath12k_vendor_validate_dump_path_extn(fallback_path);
		if (ret)
			return ret;
		*filp = filp_open(fallback_path, O_WRONLY | O_CREAT | O_TRUNC,
				  0644);
	}

	return IS_ERR(*filp) ? PTR_ERR(*filp) : 0;
}

/**
 * ath12k_vendor_write_file_contents_extn() - Write a full buffer to an open
 * file
 * @filp: File to write and close
 * @buf: Buffer containing serialized NOL data
 * @len: Number of bytes to write from @buf
 *
 * Return: 0 if all bytes were written, otherwise a negative error code.
 */
static int ath12k_vendor_write_file_contents_extn(struct file *filp,
						  const char *buf, size_t len)
{
	loff_t pos = 0;
	ssize_t wrote;

	wrote = kernel_write(filp, buf, len, &pos);
	filp_close(filp, NULL);
	if (wrote < 0)
		return (int)wrote;

	return (size_t)wrote == len ? 0 : -ENOSPC;
}

static int ath12k_vendor_write_buf_to_file_extn(const char *path,
						u8 radio_idx,
						const char *buf, size_t len);

/**
 * ath12k_vendor_mirror_tmp_nol_dump_extn() - Mirror a /tmp dump to persistent
 * storage
 * @path: Path used for the original dump
 * @radio_idx: Radio index used to derive the persistent path
 * @buf: Serialized NOL data to mirror
 * @len: Number of bytes in @buf
 *
 * Return: 0 if mirroring is skipped or attempted, or -ENOMEM on failure.
 */
static int ath12k_vendor_mirror_tmp_nol_dump_extn(const char *path,
						  u8 radio_idx,
						  const char *buf, size_t len)
{
	char *persist_path;
	int ret;

	if (strncmp(path, "/tmp/", 5))
		return 0;

	persist_path = ath12k_vendor_build_default_nol_path_extn(false,
								 radio_idx);
	if (!persist_path)
		return -ENOMEM;

	ret = ath12k_vendor_write_buf_to_file_extn(persist_path, radio_idx, buf,
						       len);
	kfree(persist_path);
	if (ret)
		ath12k_err(NULL,
			   "NOL dump under /tmp is volatile; persistent mirror failed: %d\n",
			   ret);

	return 0;
}

/**
 * ath12k_vendor_write_buf_to_file_extn() - Persist a serialized NOL buffer to
 * disk
 * @path: Destination path requested by userspace
 * @radio_idx: Radio index used to resolve default paths
 * @buf: Buffer to persist
 * @len: Number of bytes in @buf
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_write_buf_to_file_extn(const char *path,
						u8 radio_idx,
						const char *buf, size_t len)
{
	struct file *filp;
	char *primary_path;
	char *fallback_path;
	int ret;

	ret = ath12k_vendor_alloc_nol_paths_extn(radio_idx, &primary_path,
						 &fallback_path);
	if (ret)
		return ret;

	ret = ath12k_vendor_open_dump_write_file_extn(path, primary_path,
						      fallback_path, &filp);
	if (!ret)
		ret = ath12k_vendor_write_file_contents_extn(filp, buf, len);

	kfree(primary_path);
	kfree(fallback_path);
	if (ret)
		return ret;

	return ath12k_vendor_mirror_tmp_nol_dump_extn(path, radio_idx, buf, len);
}

/**
 * ath12k_vendor_try_open_dump_file_extn() - Open a validated dump file
 * @path: Absolute path to open for reading
 *
 * Return: Open file pointer on success or an error pointer on failure.
 */
static struct file *ath12k_vendor_try_open_dump_file_extn(const char *path)
{
	int ret;

	ret = ath12k_vendor_validate_dump_path_extn(path);
	if (ret)
		return ERR_PTR(ret);

	return filp_open(path, O_RDONLY, 0);
}

/**
 * ath12k_vendor_open_dump_read_file_extn() - Open a dump file with fallbacks
 * @path: Requested source path
 * @primary_path: Default primary path for this radio
 * @fallback_path: Default fallback path for this radio
 *
 * Return: Open file pointer on success or an error pointer on failure.
 */
static struct file *
ath12k_vendor_open_dump_read_file_extn(const char *path,
				       const char *primary_path,
				       const char *fallback_path)
{
	struct file *filp;

	filp = ath12k_vendor_try_open_dump_file_extn(path);
	if (!IS_ERR(filp) || PTR_ERR(filp) != -ENOENT)
		return filp;
	if (!strcmp(path, primary_path))
		return ath12k_vendor_try_open_dump_file_extn(fallback_path);
	if (strncmp(path, "/tmp/", 5))
		return filp;

	filp = ath12k_vendor_try_open_dump_file_extn(primary_path);
	if (!IS_ERR(filp) || PTR_ERR(filp) != -ENOENT)
		return filp;

	return ath12k_vendor_try_open_dump_file_extn(fallback_path);
}

/**
 * ath12k_vendor_read_filp_to_buf_extn() - Read a dump file into memory
 * @filp: Open file to read from
 * @out_buf: Returned buffer holding the file contents
 * @out_len: Returned number of bytes stored in @out_buf
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_read_filp_to_buf_extn(struct file *filp,
					       char **out_buf, size_t *out_len)
{
	loff_t pos = 0;
	ssize_t read_len;
	size_t size;
	char *buf;

	size = i_size_read(file_inode(filp));
	if (!size)
		return -EINVAL;

	size = min_t(size_t, size, 128 * 1024);
	buf = vzalloc(size + 1);
	if (!buf)
		return -ENOMEM;

	read_len = kernel_read(filp, buf, size, &pos);
	if (read_len < 0) {
		vfree(buf);
		return (int)read_len;
	}

	buf[read_len] = '\0';
	*out_buf = buf;
	*out_len = (size_t)read_len;
	return 0;
}

/**
 * ath12k_vendor_read_file_to_buf_extn() - Read a dump file with fallbacks
 * @path: Requested source path
 * @radio_idx: Radio index used to resolve default paths
 * @out_buf: Returned buffer holding the file contents
 * @out_len: Returned number of bytes stored in @out_buf
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_read_file_to_buf_extn(const char *path,
					       u8 radio_idx,
					       char **out_buf, size_t *out_len)
{
	struct file *filp;
	char *primary_path;
	char *fallback_path;
	int ret;

	if (!out_buf || !out_len)
		return -EINVAL;

	ret = ath12k_vendor_validate_dump_path_extn(path);
	if (ret)
		return ret;

	ret = ath12k_vendor_alloc_nol_paths_extn(radio_idx, &primary_path,
						 &fallback_path);
	if (ret)
		return ret;

	filp = ath12k_vendor_open_dump_read_file_extn(path, primary_path,
							 fallback_path);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto out_free;
	}

	ret = ath12k_vendor_read_filp_to_buf_extn(filp, out_buf, out_len);
	filp_close(filp, NULL);
out_free:
	kfree(primary_path);
	kfree(fallback_path);
	return ret;
}

/**
 * ath12k_vendor_get_nol_left_ms_extn() - Compute remaining NOP time for a
 * channel
 * @chan: Channel whose DFS state is inspected
 * @nop_ms: Configured NOP timeout in milliseconds
 *
 * Return: Remaining unavailable time in milliseconds, or 0 if expired.
 */
static u32 ath12k_vendor_get_nol_left_ms_extn(const struct ieee80211_channel *chan,
					      u32 nop_ms)
{
	u32 elapsed_ms;

	if (chan->dfs_state != NL80211_DFS_UNAVAILABLE)
		return 0;

	elapsed_ms = jiffies_to_msecs(jiffies - chan->dfs_state_entered);
	return (elapsed_ms >= nop_ms) ? 0 : (nop_ms - elapsed_ms);
}

/**
 * ath12k_vendor_count_nol_entries_extn() - Count NOL channels for one radio
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to filter, or negative to count all radios
 * @nop_ms: NOP timeout used to age DFS state
 *
 * Return: Number of channels that are still in NOL.
 */
static u32 ath12k_vendor_count_nol_entries_extn(struct wiphy *wiphy,
						int radio_idx, u32 nop_ms)
{
	u32 entry_count = 0;
	int band, chan_idx;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		struct ieee80211_supported_band *sband = wiphy->bands[band];

		if (!sband || !sband->channels)
			continue;

		for (chan_idx = 0; chan_idx < sband->n_channels; chan_idx++) {
			struct ieee80211_channel *chan = &sband->channels[chan_idx];

			if (!ath12k_vendor_nol_chan_matches_radio(wiphy, chan,
								  radio_idx))
				continue;
			if (!ath12k_vendor_get_nol_left_ms_extn(chan, nop_ms))
				continue;
			entry_count++;
		}
	}

	return entry_count;
}

/**
 * ath12k_vendor_init_nol_bin_hdr_extn() - Initialize a serialized NOL dump
 * header
 * @hdr: Header buffer to initialize
 * @nop_ms: NOP timeout recorded in the dump
 * @entry_count: Number of NOL entries stored after the header
 * @alpha2: Two-character country code to embed in the header, or NULL to
 *	leave the alpha2 field zeroed
 *
 * Return: None.
 */
static void ath12k_vendor_init_nol_bin_hdr_extn(
	struct ath12k_vendor_nol_bin_hdr *hdr, u32 nop_ms, u32 entry_count,
	const u8 *alpha2)
{
	memset(hdr, 0, sizeof(*hdr));
	memcpy(hdr->magic, ATH12K_VENDOR_NOL_BIN_MAGIC, sizeof(hdr->magic));
	hdr->version = cpu_to_le16(1);
	hdr->hdr_len = cpu_to_le16(sizeof(*hdr));
	hdr->nop_ms = cpu_to_le32(nop_ms);
	hdr->dump_time_real_ns = cpu_to_le64(ktime_get_real_ns());
	hdr->entry_count = cpu_to_le32(entry_count);
	if (alpha2)
		memcpy(hdr->alpha2, alpha2, sizeof(hdr->alpha2));
}

/**
 * ath12k_vendor_fill_nol_entries_extn() - Fill serialized NOL entries for a
 * radio
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to filter, or negative to include all radios
 * @nop_ms: NOP timeout used to age DFS state
 * @entries: Output array for serialized NOL entries
 * @entry_count: Capacity of @entries in elements
 *
 * Return: Number of entries written to @entries.
 */
static u32 ath12k_vendor_fill_nol_entries_extn(
	struct wiphy *wiphy, int radio_idx, u32 nop_ms,
	struct ath12k_vendor_nol_bin_entry *entries, u32 entry_count)
{
	u32 entry_idx = 0;
	int band, chan_idx;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		struct ieee80211_supported_band *sband = wiphy->bands[band];

		if (!sband || !sband->channels)
			continue;

		for (chan_idx = 0; chan_idx < sband->n_channels; chan_idx++) {
			struct ieee80211_channel *chan = &sband->channels[chan_idx];
			u32 left_ms;

			if (!ath12k_vendor_nol_chan_matches_radio(wiphy, chan,
								  radio_idx))
				continue;

			left_ms = ath12k_vendor_get_nol_left_ms_extn(chan, nop_ms);
			if (!left_ms || entry_idx >= entry_count)
				continue;

			entries[entry_idx].freq_mhz =
				cpu_to_le32((u32)chan->center_freq);
			entries[entry_idx].left_ms = cpu_to_le32(left_ms);
			entries[entry_idx].bw_mhz = cpu_to_le32(20);
			entry_idx++;
		}
	}

	return entry_idx;
}

/**
 * ath12k_vendor_build_nol_chan_list_extn() - Build a serialized NOL snapshot
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to serialize
 * @out_buf: Returned serialized buffer
 * @out_len: Returned length of @out_buf in bytes
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_build_nol_chan_list_extn(struct wiphy *wiphy,
						  int radio_idx,
						  char **out_buf,
						  size_t *out_len)
{
	struct ath12k_vendor_nol_bin_hdr hdr;
	struct ath12k_vendor_nol_bin_entry *entries;
	char *buf;
	u32 entry_count;
	u32 nop_ms = IEEE80211_DFS_MIN_NOP_TIME_MS;
	u32 entry_idx;
	struct ieee80211_hw *hw;
	struct ath12k_hw *ah;
	struct ath12k *ar;

	hw = wiphy_to_ieee80211_hw(wiphy);
	if (!hw)
		return -EINVAL;

	ah = ath12k_hw_to_ah(hw);
	if (!ah)
		return -EINVAL;

	ar = &ah->radio[radio_idx];

	if (!out_buf || !out_len)
		return -EINVAL;

	lockdep_assert_wiphy(wiphy);

	entry_count = ath12k_vendor_count_nol_entries_extn(wiphy, radio_idx,
							    nop_ms);
	buf = kzalloc(sizeof(hdr) + ((size_t)entry_count * sizeof(*entries)),
		      GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ath12k_vendor_init_nol_bin_hdr_extn(&hdr, nop_ms, entry_count, ar->alpha2);
	memcpy(buf, &hdr, sizeof(hdr));
	entries = (void *)(buf + sizeof(hdr));
	entry_idx = ath12k_vendor_fill_nol_entries_extn(wiphy, radio_idx,
							 nop_ms, entries,
							 entry_count);
	*out_buf = buf;
	*out_len = sizeof(hdr) + ((size_t)entry_idx * sizeof(*entries));
	return 0;
}

/**
 * ath12k_vendor_log_nol_band_extn() - Log active NOL entries for one band
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to filter, or negative to include all radios
 * @tag: Prefix string used in log messages
 * @nop_ms: NOP timeout used to age DFS state
 * @band: nl80211 band index to scan
 * @total: Running count of matching NOL entries
 * @printed: Running count of entries already logged
 *
 * Return: None.
 */
static void ath12k_vendor_log_nol_band_extn(struct wiphy *wiphy, int radio_idx,
					    const char *tag, u32 nop_ms,
					    int band, int *total, int *printed)
{
	struct ieee80211_supported_band *sband = wiphy->bands[band];
	int chan_idx;

	if (!sband || !sband->channels)
		return;

	for (chan_idx = 0; chan_idx < sband->n_channels; chan_idx++) {
		struct ieee80211_channel *chan = &sband->channels[chan_idx];
		u32 left_ms;
		int ch;

		if (!ath12k_vendor_nol_chan_matches_radio(wiphy, chan,
							 radio_idx))
			continue;

		left_ms = ath12k_vendor_get_nol_left_ms_extn(chan, nop_ms);
		if (!left_ms)
			continue;

		(*total)++;
		if (*printed >= ATH12K_VENDOR_NOL_LOG_LIMIT)
			continue;

		ch = ieee80211_frequency_to_channel(chan->center_freq);
		wiphy_info(wiphy, "%s: freq=%d MHz chan=%d left=%u ms\n",
			   tag, chan->center_freq, ch, left_ms);
		(*printed)++;
	}
}

/**
 * ath12k_vendor_log_nol_chan_list_extn() - Log the current NOL state
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to filter, or negative to include all radios
 * @tag: Prefix string used in log messages
 *
 * Return: None.
 */
static void ath12k_vendor_log_nol_chan_list_extn(struct wiphy *wiphy,
						 int radio_idx, const char *tag)
{
	u32 nop_ms = IEEE80211_DFS_MIN_NOP_TIME_MS;
	int band;
	int total = 0;
	int printed = 0;

	if (!wiphy)
		return;

	if (!tag)
		tag = "nol";

	lockdep_assert_wiphy(wiphy);

	wiphy_info(wiphy, "%s: current NOL channels (NOP=%u ms)\n", tag,
		   nop_ms);
	for (band = 0; band < NUM_NL80211_BANDS; band++)
		ath12k_vendor_log_nol_band_extn(wiphy, radio_idx, tag, nop_ms,
						band, &total, &printed);

	if (!total) {
		wiphy_info(wiphy, "%s: NOL empty\n", tag);
		return;
	}

	if (total > printed)
		wiphy_info(wiphy, "%s: total=%d (truncated after %d)\n",
			   tag, total, printed);
	else
		wiphy_info(wiphy, "%s: total=%d\n", tag, total);
}

/**
 * ath12k_vendor_skip_spaces_extn() - Skip leading whitespace in a string
 * @str: String to advance in place
 *
 * Return: Pointer to the first non-space character in @str.
 */
static char *ath12k_vendor_skip_spaces_extn(char *str)
{
	while (*str && isspace(*str))
		str++;

	return str;
}

/**
 * ath12k_vendor_get_nol_dump_delta_ms_extn() - Age a dump timestamp in
 * milliseconds
 * @dump_ns: Real-time dump timestamp in nanoseconds
 *
 * Return: Elapsed milliseconds since @dump_ns, or 0 if unavailable.
 */
static u64 ath12k_vendor_get_nol_dump_delta_ms_extn(u64 dump_ns)
{
	u64 now_ns;

	if (!dump_ns)
		return 0;

	now_ns = ktime_get_real_ns();
	return div_u64((now_ns > dump_ns) ? (now_ns - dump_ns) : 0, 1000000ULL);
}

/**
 * ath12k_vendor_parse_nol_text_header_extn() - Parse metadata from a text
 * dump header
 * @line: Header line starting with '#'
 * @dump_ns: Returned dump timestamp in nanoseconds
 * @nop_ms: Returned NOP timeout in milliseconds
 *
 * Return: None.
 */
static void ath12k_vendor_parse_nol_text_header_extn(char *line, u64 *dump_ns,
						     u32 *nop_ms)
{
	char *tok;
	char *p = ath12k_vendor_skip_spaces_extn(line + 1);

	while ((tok = strsep(&p, " \t")) != NULL) {
		u64 tmp_u64;
		u32 tmp_u32;

		if (!*tok)
			continue;
		if (!strncmp(tok, "dump_time_real_ns=", 18)) {
			if (!kstrtou64(tok + 18, 0, &tmp_u64))
				*dump_ns = tmp_u64;
			continue;
		}
		if (strncmp(tok, "nop_ms=", 7) || kstrtou32(tok + 7, 0, &tmp_u32))
			continue;

		tmp_u32 = min_t(u32, tmp_u32,
				(u32)IEEE80211_DFS_MIN_NOP_TIME_MS);
		if (tmp_u32)
			*nop_ms = tmp_u32;
	}
}

/**
 * ath12k_vendor_nol_left_ms_token_extn() - Match a token carrying remaining
 * NOL time
 * @tok: Token from a text dump line
 *
 * Return: Pointer to the numeric substring if @tok matches, else %NULL.
 */
static const char *ath12k_vendor_nol_left_ms_token_extn(const char *tok)
{
	if (!strncmp(tok, "nol_left_ms=", 12))
		return tok + 12;
	if (!strncmp(tok, "nol_timeout_ms=", 15))
		return tok + 15;
	if (!strncmp(tok, "left_ms=", 8))
		return tok + 8;

	return NULL;
}

/**
 * ath12k_vendor_parse_nol_text_entry_extn() - Parse one text NOL entry
 * @line: Entry line to parse in place
 * @nop_ms: Default NOP timeout in milliseconds
 * @freq_mhz: Returned center frequency in MHz
 * @left_ms: Returned remaining NOL time in milliseconds
 *
 * Return: 0 if an entry was parsed, or -ENOENT if the line has no usable
 * entry.
 */
static int ath12k_vendor_parse_nol_text_entry_extn(char *line, u32 nop_ms,
						   int *freq_mhz, u32 *left_ms)
{
	char *tok;
	bool has_left = false;

	*freq_mhz = -1;
	*left_ms = nop_ms;

	while ((tok = strsep(&line, " \t")) != NULL) {
		const char *left_ms_ptr;
		u32 tmp_u32;

		if (!*tok)
			continue;
		if (!strncmp(tok, "freq=", 5)) {
			int tmp_freq;

			if (!kstrtoint(tok + 5, 0, &tmp_freq))
				*freq_mhz = tmp_freq;
			continue;
		}

		left_ms_ptr = ath12k_vendor_nol_left_ms_token_extn(tok);
		if (!left_ms_ptr || kstrtou32(left_ms_ptr, 0, &tmp_u32))
			continue;

		*left_ms = tmp_u32;
		has_left = true;
	}

	if (*freq_mhz <= 0)
		return -ENOENT;
	if (!has_left)
		*left_ms = nop_ms;

	*left_ms = min_t(u32, *left_ms, nop_ms);
	return *left_ms ? 0 : -ENOENT;
}

/**
 * ath12k_vendor_adjust_nol_left_ms_extn() - Age a saved NOL timeout
 * @left_ms: Saved remaining NOL time in milliseconds
 * @nop_ms: Maximum NOP timeout in milliseconds
 * @delta_ms: Elapsed wall-clock time since the dump
 * @remaining_ms: Returned remaining NOL time after aging
 *
 * Return: %true if the entry still has time left, otherwise %false.
 */
static bool ath12k_vendor_adjust_nol_left_ms_extn(u32 left_ms, u32 nop_ms,
						  u64 delta_ms, u32 *remaining_ms)
{
	left_ms = min_t(u32, left_ms, nop_ms);
	if (!left_ms || delta_ms >= left_ms)
		return false;

	*remaining_ms = left_ms - (u32)delta_ms;
	return true;
}

/**
 * ath12k_vendor_apply_nol_freq_extn() - Restore one channel into NOL state
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to filter, or negative to include all radios
 * @freq_mhz: Channel center frequency to restore
 * @left_ms: Saved remaining NOL time in milliseconds
 * @nop_ms: Maximum NOP timeout in milliseconds
 * @delta_ms: Elapsed wall-clock time since the dump
 *
 * Return: %true if a channel was updated, otherwise %false.
 */
static bool ath12k_vendor_apply_nol_freq_extn(struct wiphy *wiphy, int radio_idx,
					      u32 freq_mhz, u32 left_ms,
					      u32 nop_ms, u64 delta_ms)
{
	struct ieee80211_channel *chan;
	unsigned long delta_j;
	u32 remaining_ms;

	if (!freq_mhz || !ath12k_vendor_adjust_nol_left_ms_extn(left_ms, nop_ms,
								delta_ms,
								&remaining_ms))
		return false;

	chan = ieee80211_get_channel(wiphy, (int)freq_mhz);
	if (!chan || !ath12k_vendor_nol_chan_matches_radio(wiphy, chan,
							  radio_idx))
		return false;

	chan->dfs_state = NL80211_DFS_UNAVAILABLE;
	delta_j = msecs_to_jiffies(nop_ms - remaining_ms);
	chan->dfs_state_entered = jiffies - delta_j;
	return true;
}

/**
 * ath12k_vendor_apply_nol_from_text_buf_extn() - Restore NOL entries from a
 * text dump
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to restore
 * @buf: Writable text buffer containing dump contents
 *
 * Return: Number of channels restored on success or a negative error code.
 */
static int ath12k_vendor_apply_nol_from_text_buf_extn(struct wiphy *wiphy,
						      int radio_idx, char *buf)
{
	char *line;
	u64 dump_ns = 0;
	u64 delta_ms = 0;
	u32 nop_ms = IEEE80211_DFS_MIN_NOP_TIME_MS;
	int updated = 0;

	if (!wiphy || !buf)
		return -EINVAL;

	lockdep_assert_wiphy(wiphy);

	while ((line = strsep(&buf, "\n")) != NULL) {
		int freq_mhz;
		u32 left_ms;

		line = ath12k_vendor_skip_spaces_extn(line);
		if (!*line)
			continue;

		if (*line == '#') {
			ath12k_vendor_parse_nol_text_header_extn(line, &dump_ns,
								&nop_ms);
			delta_ms = ath12k_vendor_get_nol_dump_delta_ms_extn(dump_ns);
			continue;
		}

		if (ath12k_vendor_parse_nol_text_entry_extn(line, nop_ms, &freq_mhz,
							    &left_ms))
			continue;
		if (ath12k_vendor_apply_nol_freq_extn(wiphy, radio_idx, freq_mhz,
						      left_ms, nop_ms,
						      delta_ms))
			updated++;
	}

	cfg80211_schedule_dfs_chan_update(wiphy);

	return updated;
}

struct ath12k_vendor_nol_bin_info_extn {
	const struct ath12k_vendor_nol_bin_entry *entries;
	u32 entry_count;
	u32 nop_ms;
	u64 delta_ms;
	u8 alpha2[2];
};

/**
 * ath12k_vendor_parse_nol_bin_hdr_extn() - Validate a binary NOL dump header
 * @buf: Binary dump buffer
 * @len: Number of bytes available in @buf
 * @info: Returned parsed header metadata
 *
 * Return: 0 if the header is valid or a negative error code otherwise.
 */
static int ath12k_vendor_parse_nol_bin_hdr_extn(
	const u8 *buf, size_t len, struct ath12k_vendor_nol_bin_info_extn *info)
{
	const struct ath12k_vendor_nol_bin_hdr *hdr = (const void *)buf;
	u16 hdr_len;
	u16 version;
	size_t need_len;

	if (len < sizeof(*hdr))
		return -EINVAL;
	if (memcmp(hdr->magic, ATH12K_VENDOR_NOL_BIN_MAGIC, sizeof(hdr->magic)))
		return -EINVAL;

	version = le16_to_cpu(hdr->version);
	hdr_len = le16_to_cpu(hdr->hdr_len);
	if (version != 1 || hdr_len < sizeof(*hdr) || hdr_len > len)
		return -EINVAL;

	info->nop_ms = le32_to_cpu(hdr->nop_ms);
	if (!info->nop_ms)
		info->nop_ms = IEEE80211_DFS_MIN_NOP_TIME_MS;
	info->nop_ms = min_t(u32, info->nop_ms,
			     (u32)IEEE80211_DFS_MIN_NOP_TIME_MS);
	info->entry_count = le32_to_cpu(hdr->entry_count);
	memcpy(info->alpha2, hdr->alpha2, sizeof(info->alpha2));
	need_len = (size_t)hdr_len + (size_t)info->entry_count *
		   sizeof(*info->entries);
	if (need_len > len)
		return -EINVAL;

	info->delta_ms = ath12k_vendor_get_nol_dump_delta_ms_extn(
		le64_to_cpu(hdr->dump_time_real_ns));
	info->entries = (const void *)(buf + hdr_len);
	return 0;
}

/**
 * ath12k_vendor_apply_nol_from_bin_buf_extn() - Restore NOL entries from a
 * binary dump
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to restore
 * @buf: Binary dump buffer
 * @len: Number of bytes available in @buf
 *
 * Return: Number of channels restored on success or a negative error code.
 */
static int ath12k_vendor_apply_nol_from_bin_buf_extn(struct wiphy *wiphy,
						     int radio_idx,
						     const u8 *buf, size_t len)
{
	struct ath12k_vendor_nol_bin_info_extn info;
	u32 i;
	int updated = 0;
	int ret;
	struct ieee80211_hw *hw;
	struct ath12k_hw *ah;
	struct ath12k *ar;

	if (!wiphy || !buf)
		return -EINVAL;

	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = ath12k_hw_to_ah(hw);
	ar = &ah->radio[radio_idx];

	lockdep_assert_wiphy(wiphy);

	ret = ath12k_vendor_parse_nol_bin_hdr_extn(buf, len, &info);
	if (ret)
		return ret;

	/* If the dump was written under a different country code, do not apply
	 * it. Restoring NOL from a different domain would incorrectly block
	 * channels that are clean under the current regulatory domain.
	 */
	if (info.alpha2[0] &&
	    memcmp(ar->alpha2, info.alpha2, 2)) {
		wiphy_dbg(wiphy,
			  "NOL auto-restore skipped: CC mismatch dump=%.2s current=%.2s radio_idx=%d\n",
			  info.alpha2, ar->alpha2, radio_idx);
		return 0;
	}

	for (i = 0; i < info.entry_count; i++) {
		if (ath12k_vendor_apply_nol_freq_extn(
			    wiphy, radio_idx,
			    le32_to_cpu(info.entries[i].freq_mhz),
			    le32_to_cpu(info.entries[i].left_ms), info.nop_ms,
			    info.delta_ms))
			updated++;
	}

	cfg80211_schedule_dfs_chan_update(wiphy);

	return updated;
}

/**
 * ath12k_vendor_apply_nol_from_buf_extn() - Restore NOL entries from a dump
 * buffer
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to restore
 * @buf: Dump buffer, either text or binary
 * @len: Number of bytes available in @buf
 *
 * Return: Number of channels restored on success or a negative error code.
 */
static int ath12k_vendor_apply_nol_from_buf_extn(struct wiphy *wiphy,
						 int radio_idx, char *buf,
						 size_t len)
{
	if (wiphy && buf && len >= sizeof(struct ath12k_vendor_nol_bin_hdr) &&
	    !memcmp(buf, ATH12K_VENDOR_NOL_BIN_MAGIC,
		    sizeof(ATH12K_VENDOR_NOL_BIN_MAGIC)))
		return ath12k_vendor_apply_nol_from_bin_buf_extn(wiphy, radio_idx,
								 (const u8 *)buf,
								 len);

	return ath12k_vendor_apply_nol_from_text_buf_extn(wiphy, radio_idx, buf);
}

/**
 * ath12k_vendor_restore_nol_file_extn() - Restore NOL entries from a file
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to restore
 * @path: Dump file path to read
 *
 * Return: Number of channels restored on success or a negative error code.
 */
static int ath12k_vendor_restore_nol_file_extn(struct wiphy *wiphy,
					       u8 radio_idx, const char *path)
{
	char *file_buf = NULL;
	size_t file_len = 0;
	int ret;

	ret = ath12k_vendor_read_file_to_buf_extn(path, radio_idx, &file_buf,
						  &file_len);
	if (ret)
		return ret;

	ret = ath12k_vendor_apply_nol_from_buf_extn(wiphy, radio_idx, file_buf,
						    file_len);
	vfree(file_buf);
	return ret;
}

/**
 * ath12k_vendor_validate_radio_idx() - Validate a radio index for setnol
 * @wiphy: Wiphy exposing the radio count
 * @radio_idx: Radio index from userspace
 *
 * Return: 0 if @radio_idx is valid, otherwise -EINVAL.
 */
int ath12k_vendor_validate_radio_idx(struct wiphy *wiphy, u8 radio_idx)
{
	if (!wiphy || radio_idx == INVALID_RADIO_INDEX ||
	    radio_idx >= wiphy->n_radio)
		return -EINVAL;

	return 0;
}

/**
 * ath12k_vendor_resolve_nol_path_extn() - Resolve the dump path for setnol
 * @path: Optional userspace path
 * @radio_idx: Radio index used for the default path
 * @resolved_path: Returned resolved path string
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_resolve_nol_path_extn(const char *path, u8 radio_idx,
					       char **resolved_path)
{
	if (path && *path)
		*resolved_path = kstrdup(path, GFP_KERNEL);
	else
		*resolved_path = ath12k_vendor_build_default_nol_path_extn(false,
									  radio_idx);

	return *resolved_path ? 0 : -ENOMEM;
}

/**
 * ath12k_vendor_resolve_nol_path_from_data_extn() - Resolve the dump path for
 * getnol/setnol command payload
 * @data: Optional userspace path buffer
 * @data_len: Number of bytes available in @data
 * @radio_idx: Radio index used for the default path
 * @path: Returned resolved path string
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_resolve_nol_path_from_data_extn(const void *data,
							 u32 data_len,
							 u8 radio_idx,
							 char **path)
{
	if (!path)
		return -EINVAL;

	if (!data || !data_len)
		*path = ath12k_vendor_build_default_nol_path_extn(false,
								  radio_idx);
	else
		*path = kstrndup(data, min_t(u32, data_len, PATH_MAX - 1),
				 GFP_KERNEL);

	return *path ? 0 : -ENOMEM;
}

/**
 * ath12k_vendor_reply_u32_extn() - Send a 32-bit vendor command reply
 * @wiphy: Wiphy used to allocate the reply skb
 * @value: Reply value to return through generic params
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_reply_u32_extn(struct wiphy *wiphy, u32 value)
{
	struct sk_buff *reply;
	size_t reply_len = nla_total_size(sizeof(u32)) * 3;

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, reply_len);
	if (!reply)
		return -ENOMEM;

	if (nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, value) ||
	    nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
			sizeof(value)) ||
	    nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0)) {
		kfree_skb(reply);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(reply);
}

/**
 * ath12k_vendor_dump_nol_to_file_extn() - Dump the current NOL snapshot to
 * disk
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to serialize
 * @path: Destination file path
 * @dump_len: Returned serialized dump length in bytes
 *
 * Return: 0 if the NOL snapshot was persisted, else a negative error code.
 */
static int ath12k_vendor_dump_nol_to_file_extn(struct wiphy *wiphy,
					       u8 radio_idx, const char *path,
					       u32 *dump_len)
{
	char *out = NULL;
	size_t out_len = 0;
	int ret;

	ret = ath12k_vendor_build_nol_chan_list_extn(wiphy, radio_idx, &out,
						     &out_len);
	if (!ret)
		ret = ath12k_vendor_write_buf_to_file_extn(path, radio_idx, out,
							   out_len);
	if (!ret)
		ath12k_vendor_log_nol_chan_list_extn(wiphy, radio_idx, "getnol");
	if (!ret && dump_len)
		*dump_len = (u32)min_t(size_t, out_len, (size_t)~0U);

	kfree(out);
	return ret;
}

/**
 * ath12k_vendor_get_started_ap_arvif_extn() - Find the first started AP link
 * on a radio
 * @ar: Radio instance to inspect
 *
 * Return: AP arvif with a valid chanctx, or %NULL if none is active.
 */
static struct ath12k_link_vif *
ath12k_vendor_get_started_ap_arvif_extn(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;

	if (!ar)
		return NULL;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->is_started || !arvif->ahvif || !arvif->ahvif->vif)
			continue;

		if (arvif->ahvif->vif->type != NL80211_IFTYPE_AP)
			continue;

		if (!arvif->chanctx.def.chan)
			continue;

		return arvif;
	}

	return NULL;
}

/**
 * ath12k_vendor_trigger_radar_on_nol_ap_extn() - Force DFS recovery when
 * setnol restores NOL onto the currently operating AP channel
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index restored by setnol
 *
 * This is used to cover the case where setnol runs during CAC and mac80211
 * later transitions the operating channel back to available on CAC completion.
 */
static void ath12k_vendor_trigger_radar_on_nol_ap_extn(struct wiphy *wiphy,
						       u8 radio_idx)
{
	struct ieee80211_hw *hw;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;

	if (!wiphy)
		return;

	hw = wiphy_to_ieee80211_hw(wiphy);
	if (!hw || !hw->priv)
		return;

	ah = hw->priv;
	if (radio_idx >= ah->num_radio)
		return;

	ar = &ah->radio[radio_idx];
	arvif = ath12k_vendor_get_started_ap_arvif_extn(ar);
	if (!arvif || !cfg80211_chandef_valid(&arvif->chanctx.def))
		return;

	if (cfg80211_chandef_dfs_nol_clear(wiphy, &arvif->chanctx.def))
		return;

	ath12k_warn(arvif->ar->ab,
		    "setnol: AP vdev %d on radio %u is operating on a restored NOL channel, triggering radar event\n",
		    arvif->vdev_id, radio_idx);

	ath12k_mac_setnol_recovery_extn(arvif, &arvif->chanctx);
}

/**
 * ath12k_vendor_set_nol_chan_list_extn() - Restore saved NOL state for a
 * radio
 * @wiphy: Wiphy that owns the DFS channel tables
 * @radio_idx: Radio index to restore
 * @path: Optional dump file path
 * @num_updated: Returned number of channels restored
 *
 * Return: 0 if the dump was applied successfully, else a negative error code.
 */
int ath12k_vendor_set_nol_chan_list_extn(struct wiphy *wiphy, u8 radio_idx,
					 const char *path, u32 *num_updated)
{
	char *resolved_path = NULL;
	int updated;
	int ret;

	if (!wiphy)
		return -EINVAL;

	lockdep_assert_wiphy(wiphy);

	ret = ath12k_vendor_validate_radio_idx(wiphy, radio_idx);
	if (ret)
		return ret;

	ret = ath12k_vendor_resolve_nol_path_extn(path, radio_idx,
						 &resolved_path);
	if (ret)
		return ret;

	updated = ath12k_vendor_restore_nol_file_extn(wiphy, radio_idx,
						      resolved_path);
	if (updated < 0) {
		kfree(resolved_path);
		return updated;
	}

	if (num_updated)
		*num_updated = updated;

	if (updated > 0)
		ath12k_vendor_trigger_radar_on_nol_ap_extn(wiphy, radio_idx);

	wiphy_info(wiphy, "setnol: radio_idx=%u applied %d entries from %s\n",
		   radio_idx, updated, resolved_path);
	ath12k_vendor_log_nol_chan_list_extn(wiphy, radio_idx, "setnol");
	kfree(resolved_path);

	return 0;
}

int ath12k_vendor_nol_dump_to_file_extn(struct wiphy *wiphy, u8 radio_idx,
					const char *path, u32 *dump_len)
{
	char *resolved_path = NULL;
	int ret;

	if (!wiphy)
		return -EINVAL;

	lockdep_assert_wiphy(wiphy);

	ret = ath12k_vendor_validate_radio_idx(wiphy, radio_idx);
	if (ret)
		return ret;

	ret = ath12k_vendor_resolve_nol_path_extn(path, radio_idx,
						  &resolved_path);
	if (ret)
		return ret;

	ret = ath12k_vendor_dump_nol_to_file_extn(wiphy, radio_idx,
						  resolved_path, dump_len);
	kfree(resolved_path);
	return ret;
}

int ath12k_vendor_nol_restore_from_file_extn(struct wiphy *wiphy, u8 radio_idx,
					     const char *path,
					     u32 *num_updated)
{
	char *resolved_path = NULL;
	int updated;
	int ret;

	if (!wiphy)
		return -EINVAL;

	lockdep_assert_wiphy(wiphy);

	ret = ath12k_vendor_validate_radio_idx(wiphy, radio_idx);
	if (ret)
		return ret;

	ret = ath12k_vendor_resolve_nol_path_extn(path, radio_idx,
						  &resolved_path);
	if (ret)
		return ret;

	updated = ath12k_vendor_restore_nol_file_extn(wiphy, radio_idx,
						      resolved_path);
	kfree(resolved_path);

	if (updated < 0)
		return updated;

	if (num_updated)
		*num_updated = updated;

	return 0;
}

/* Find the 'ar' radio instance using the radio index of the wiphy
 * to set params
 */
int ath12k_vendor_set_wiphy_params_extn(struct wiphy *wiphy,
					struct ath12k_wifi_generic_params *params)
{
        struct ieee80211_hw *hw = NULL;
        struct ath12k_hw *ah = NULL;
        struct ath12k *ar = NULL;
        u32 param;
        u32 value = 0;
        u8 radio_idx;
        int ret = -1;
        bool reload = false;

        lockdep_assert_wiphy(wiphy);

	param = params->value;
	radio_idx = params->radio_idx;

        hw = wiphy_to_ieee80211_hw(wiphy);

        ah = hw->priv;

	if ((param == QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ALPHA2 ||
	     param == QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ID ||
	     param == QCA_WLAN_VENDOR_RADIO_PARAM_REGDOMAIN) &&
	    radio_idx == INVALID_RADIO_INDEX)
		radio_idx = 0;

	if (radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index %u (valid range: 0..%u)\n",
			   radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
		return -ENODATA;
	}

        ar = &ah->radio[radio_idx];
        if (!ar) {
		ath12k_err(NULL, "Failed to find ar\n");
                return -ENODATA;
        }

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ar: %p param: %d value: %d if: %d radio: %d\n",
		   ar, param, value,
		   params->ifindex, radio_idx);

	if (!params->data || !params->data_len) {
		ath12k_err(NULL, "Invalid params data for radio param: %d\n", param);
		return -EINVAL;
	}

	switch (param) {
	case QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ALPHA2:
		if (params->data_len < REG_ALPHA2_LEN) {
			ath12k_err(NULL, "Invalid country code data len: %u\n",
				   params->data_len);
			return -EINVAL;
		}

		value = ((u8 *)params->data)[0] << 8 | ((u8 *)params->data)[1];
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ID:
		if (params->data_len < sizeof(u32)) {
			ath12k_err(NULL, "Invalid country id data len: %u\n",
				   params->data_len);
			return -EINVAL;
		}

		memcpy(&value, params->data, sizeof(value));
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_REGDOMAIN:
		if (params->data_len < sizeof(u32)) {
			ath12k_err(NULL, "Invalid regdomain data len: %u\n",
				   params->data_len);
			return -EINVAL;
		}

		memcpy(&value, params->data, sizeof(value));
		ret = ath12k_vendor_set_regdomain(wiphy, ar, value);
		return ret;
	case QCA_WLAN_VENDOR_RADIO_PARAM_GET_NFCAL_POWER:
		ret = ath12k_wmi_send_pdev_get_nfcal_power_cmd(ar);
		if (ret)
			ath12k_warn(ar->ab,
				    "failed to send WMI_PDEV_GET_NFCAL_POWER cmd: %d\n",
				    ret);
		return ret;
	case QCA_WLAN_VENDOR_RADIO_PARAM_SCAN_STRICT_PASSIVE_PCH:
		if (params->data_len < sizeof(u32)) {
			ath12k_err(NULL, "Invalid strict passive scan data len: %u\n",
				   params->data_len);
			return -EINVAL;
		}

		memcpy(&value, params->data, sizeof(value));
		if (value > 1)
			return -EINVAL;

		ar->radio_cfg.radio_cfg_extn.scan_f_strict_passive_pch = value;
		return 0;
	case QCA_WLAN_VENDOR_WIFI_PARAM_SUSPEND_DEVICE:
		if (!params->data || params->data_len < sizeof(u32)) {
			ath12k_err(NULL, "suspend_device: missing op value\n");
			return -EINVAL;
		}
		memcpy(&value, params->data, sizeof(value));
		params->value = value;
		return ath12k_vendor_suspend_device_extn(wiphy, params);

	default:
		if (params->data_len < sizeof(u32)) {
			ath12k_err(NULL, "Invalid data len: %u for radio param: %d\n",
				   params->data_len, param);
			return -EINVAL;
		}

		memcpy(&value, params->data, sizeof(value));
		break;
	}

	ret = ath12k_vendor_set_radio_params(wiphy, ar, param, value, params,
					     &reload);
	if (!ret && reload)
		ath12k_vendor_event_iface_reload(wiphy, params->radio_idx);

        return ret;
}

int ath12k_vendor_set_wiphy_sr_params_extn(struct wiphy *wiphy,
					   struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_hw *hw = NULL;
	struct ath12k_hw *ah = NULL;
	struct ath12k *ar = NULL;
	u32 param;
	void *data;
	u32 data_len;
	u32 value;
	u32 flags;
	bool reload = false;
	int ret;

	lockdep_assert_wiphy(wiphy);

	param = params->value;
	data = params->data;
	data_len = params->data_len;
	value = params->length;
	flags = params->flags;

	hw = wiphy_to_ieee80211_hw(wiphy);

	ah = hw->priv;

	if (params->radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index %u (valid range: 0..%u)\n",
			   params->radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
		return -ENODATA;
	}

	ar = &ah->radio[params->radio_idx];
	if (!ar) {
		ath12k_err(NULL, "Failed to find ar\n");
		return -ENODATA;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ar: %p param: %d value: %d if: %d radio: %d\n",
		   ar, param, value,
		   params->ifindex, params->radio_idx);

	ret = ath12k_vendor_set_sr_radio_params(ar, param, data, data_len,
						value, flags, &reload);

	if (!ret && reload)
		ath12k_vendor_event_iface_reload(wiphy, params->radio_idx);

	return ret;
}


static int ath12k_vendor_get_rssi_rate_threshold(u8 type, u64 *value)
{
	int threshold = ath12k_get_rssi_rate_threshold(type);

	if (threshold == -ENOTCONN) {
		ath12k_err(NULL, "Telemetry agent not loaded\n");
		return -ENOTCONN;
	}

	*value = threshold;
	return 0;
}

static u32 ath12k_vendor_get_supported_band_bitmap(struct ath12k *ar)
{
	u32 bands = 0;

	if (!ar)
		return bands;

	if (ar->mac.sbands[NL80211_BAND_2GHZ].channels &&
	    ar->mac.sbands[NL80211_BAND_2GHZ].n_channels)
		bands |= QCA_WLAN_VENDOR_SUPPORTED_BAND_2GHZ;

	if (ar->mac.sbands[NL80211_BAND_5GHZ].channels &&
	    ar->mac.sbands[NL80211_BAND_5GHZ].n_channels)
		bands |= QCA_WLAN_VENDOR_SUPPORTED_BAND_5GHZ;

	if (ar->mac.sbands[NL80211_BAND_6GHZ].channels &&
	    ar->mac.sbands[NL80211_BAND_6GHZ].n_channels)
		bands |= QCA_WLAN_VENDOR_SUPPORTED_BAND_6GHZ;

	return bands;
}

static void ath12k_vendor_str_append_token(char *buf, size_t buf_sz,
				   const char *token)
{
	size_t len;

	if (!buf || !buf_sz || !token)
		return;

	len = strlen(buf);
	if (len >= buf_sz)
		return;

	if (len)
		len += scnprintf(buf + len, buf_sz - len, ", ");

	scnprintf(buf + len, buf_sz - len, "%s", token);
}

static void ath12k_vendor_log_supported_bands(u32 bands)
{
	static const struct ath12k_band_desc band_map[] = {
		{ QCA_WLAN_VENDOR_SUPPORTED_BAND_2GHZ, "2GHz:1" },
		{ QCA_WLAN_VENDOR_SUPPORTED_BAND_5GHZ, "5GHz:2" },
		{ QCA_WLAN_VENDOR_SUPPORTED_BAND_6GHZ, "6GHz:3" },
	};
	char band_str[64];
	int i;

	band_str[0] = '\0';
	for (i = 0; i < ARRAY_SIZE(band_map); i++) {
		if (bands & band_map[i].mask)
			ath12k_vendor_str_append_token(band_str, sizeof(band_str),
					       band_map[i].token);
	}

	ath12k_info(NULL, "Supported Frequency Bands: %s\n",
		    band_str[0] ? band_str : "none");
}

/*
 * Max-rate lookup table: ath12k_max_rates[mode][nss-1][gi_idx] (u32, Kbps)
 *   mode   : ATH12K_IEEE80211_MODE_* (0-63)
 *   nss-1  : 0-15 (1SS-16SS)
 *   gi_idx : 0=0.8us/LGI  1=0.4us/SGI  2=1.6us  3=3.2us
 */
/* PHY mode index constants for ath12k_max_rates table */
#define ATH12K_IEEE80211_MODE_AUTO             0
#define ATH12K_IEEE80211_MODE_11A              1
#define ATH12K_IEEE80211_MODE_11B              2
#define ATH12K_IEEE80211_MODE_11G              3
#define ATH12K_IEEE80211_MODE_FH               4
#define ATH12K_IEEE80211_MODE_TURBO_A          5
#define ATH12K_IEEE80211_MODE_TURBO_G          6
#define ATH12K_IEEE80211_MODE_11NA_HT20        7
#define ATH12K_IEEE80211_MODE_11NG_HT20        8
#define ATH12K_IEEE80211_MODE_11NA_HT40PLUS    9
#define ATH12K_IEEE80211_MODE_11NA_HT40MINUS   10
#define ATH12K_IEEE80211_MODE_11NG_HT40PLUS    11
#define ATH12K_IEEE80211_MODE_11NG_HT40MINUS   12
#define ATH12K_IEEE80211_MODE_11NG_HT40        13
#define ATH12K_IEEE80211_MODE_11NA_HT40        14
#define ATH12K_IEEE80211_MODE_11AC_VHT20       15
#define ATH12K_IEEE80211_MODE_11AC_VHT40PLUS   16
#define ATH12K_IEEE80211_MODE_11AC_VHT40MINUS  17
#define ATH12K_IEEE80211_MODE_11AC_VHT40       18
#define ATH12K_IEEE80211_MODE_11AC_VHT80       19
#define ATH12K_IEEE80211_MODE_11AC_VHT160      20
#define ATH12K_IEEE80211_MODE_11AC_VHT80_80    21
#define ATH12K_IEEE80211_MODE_11AXA_HE20       22
#define ATH12K_IEEE80211_MODE_11AXG_HE20       23
#define ATH12K_IEEE80211_MODE_11AXA_HE40PLUS   24
#define ATH12K_IEEE80211_MODE_11AXA_HE40MINUS  25
#define ATH12K_IEEE80211_MODE_11AXG_HE40PLUS   26
#define ATH12K_IEEE80211_MODE_11AXG_HE40MINUS  27
#define ATH12K_IEEE80211_MODE_11AXA_HE40       28
#define ATH12K_IEEE80211_MODE_11AXG_HE40       29
#define ATH12K_IEEE80211_MODE_11AXA_HE80       30
#define ATH12K_IEEE80211_MODE_11AXA_HE160      31
#define ATH12K_IEEE80211_MODE_11AXA_HE80_80    32
#define ATH12K_IEEE80211_MODE_11BEA_EHT20      33
#define ATH12K_IEEE80211_MODE_11BEG_EHT20      34
#define ATH12K_IEEE80211_MODE_11BEA_EHT40PLUS  35
#define ATH12K_IEEE80211_MODE_11BEA_EHT40MINUS 36
#define ATH12K_IEEE80211_MODE_11BEG_EHT40PLUS  37
#define ATH12K_IEEE80211_MODE_11BEG_EHT40MINUS 38
#define ATH12K_IEEE80211_MODE_11BEA_EHT40      39
#define ATH12K_IEEE80211_MODE_11BEG_EHT40      40
#define ATH12K_IEEE80211_MODE_11BEA_EHT80      41
#define ATH12K_IEEE80211_MODE_11BEA_EHT160     42
#define ATH12K_IEEE80211_MODE_11BEA_EHT320     43
#define ATH12K_IEEE80211_MODE_MAX              44

#define ATH12K_NG_VHT_MODES           3
#define ATH12K_VHT_1024_QAM_MODES     11

/* EHT puncturing rows (58–63) */
#define ATH12K_EHT_PUNC_BASE \
	(ATH12K_IEEE80211_MODE_MAX + ATH12K_NG_VHT_MODES + ATH12K_VHT_1024_QAM_MODES)
#define ATH12K_EHT_PUNC_80_20   (ATH12K_EHT_PUNC_BASE + 0)  /* = 58 */
#define ATH12K_EHT_PUNC_160_20  (ATH12K_EHT_PUNC_BASE + 1)  /* = 59 */
#define ATH12K_EHT_PUNC_160_40  (ATH12K_EHT_PUNC_BASE + 2)  /* = 60 */
#define ATH12K_EHT_PUNC_320_40  (ATH12K_EHT_PUNC_BASE + 3)  /* = 61 */
#define ATH12K_EHT_PUNC_320_80  (ATH12K_EHT_PUNC_BASE + 4)  /* = 62 */
#define ATH12K_EHT_PUNC_320_120 (ATH12K_EHT_PUNC_BASE + 5)  /* = 63 */
#define ATH12K_EHT_PUNC_MODES   6

#define ATH12K_MODE_TABLE_SIZE \
	(ATH12K_IEEE80211_MODE_MAX + ATH12K_NG_VHT_MODES + \
	 ATH12K_VHT_1024_QAM_MODES + ATH12K_EHT_PUNC_MODES)  /* = 64 */

#define ATH12K_MAX_NSS       16
#define ATH12K_MAX_GI_MODE    4

/* GI index constants */
#define ATH12K_RATE_GI_0DOT8_US  0  /* HE/EHT default 0.8us; HT/VHT LGI */
#define ATH12K_RATE_GI_0DOT4_US  1  /* HE 0.4us;              HT/VHT SGI */
#define ATH12K_RATE_GI_1DOT6_US  2  /* HE/EHT 1.6us only                 */
#define ATH12K_RATE_GI_3DOT2_US  3  /* HE/EHT 3.2us only                 */

static const u32 ath12k_max_rates[ATH12K_MODE_TABLE_SIZE][ATH12K_MAX_NSS][ATH12K_MAX_GI_MODE] = {
/* [0] AUTO */
{
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
},
/* [1] 11A, 5GHz OFDM */
{
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
},
/* [2] 11B, 2GHz CCK */
{
  {11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},
  {11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},
  {11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},
  {11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},{11000,11000,0,0},
},
/* [3] 11G, 2GHz OFDM */
{
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
  {54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},{54000,54000,0,0},
},
/* [4] FH */
{
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
},
/* [5] TURBO_A */
{
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
},
/* [6] TURBO_G */
{
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
},
/* [7] 11NA_HT20, 5GHz HT20 */
{
  { 65000, 72200,0,0},{130000,144400,0,0},{195000,216700,0,0},{260000,288900,0,0},
  {260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},
  {260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},
  {260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},
},
/* [8] 11NG_HT20, 2GHz HT20 */
{
  { 65000, 72200,0,0},{130000,144400,0,0},{195000,216700,0,0},{260000,288900,0,0},
  {260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},
  {260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},
  {260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},{260000,288900,0,0},
},
/* [9] 11NA_HT40PLUS */
{
  {135000,150000,0,0},{270000,300000,0,0},{405000,450000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
},
/* [10] 11NA_HT40MINUS */
{
  {135000,150000,0,0},{270000,300000,0,0},{405000,450000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
},
/* [11] 11NG_HT40PLUS */
{
  {135000,150000,0,0},{270000,300000,0,0},{405000,450000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
},
/* [12] 11NG_HT40MINUS */
{
  {135000,150000,0,0},{270000,300000,0,0},{405000,450000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
},
/* [13] 11NG_HT40 */
{
  {135000,150000,0,0},{270000,300000,0,0},{405000,450000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
},
/* [14] 11NA_HT40 */
{
  {135000,150000,0,0},{270000,300000,0,0},{405000,450000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
  {540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},{540000,600000,0,0},
},
/* [15] 11AC_VHT20 */
{
  { 78000, 86700,0,0},{156000,173300,0,0},{260000,288900,0,0},{312000,346700,0,0},
  {390000,433300,0,0},{520000,577800,0,0},{546000,606700,0,0},{624000,693300,0,0},
  {624000,693300,0,0},{624000,693300,0,0},{624000,693300,0,0},{624000,693300,0,0},
  {624000,693300,0,0},{624000,693300,0,0},{624000,693300,0,0},{624000,693300,0,0},
},
/* [16] 11AC_VHT40PLUS */
{
  { 180000, 200000,0,0},{ 360000, 400000,0,0},{ 540000, 600000,0,0},{ 720000, 800000,0,0},
  { 900000,1000000,0,0},{1080000,1200000,0,0},{1260000,1400000,0,0},{1440000,1600000,0,0},
  {1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},
  {1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},
},
/* [17] 11AC_VHT40MINUS */
{
  { 180000, 200000,0,0},{ 360000, 400000,0,0},{ 540000, 600000,0,0},{ 720000, 800000,0,0},
  { 900000,1000000,0,0},{1080000,1200000,0,0},{1260000,1400000,0,0},{1440000,1600000,0,0},
  {1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},
  {1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},
},
/* [18] 11AC_VHT40 */
{
  { 180000, 200000,0,0},{ 360000, 400000,0,0},{ 540000, 600000,0,0},{ 720000, 800000,0,0},
  { 900000,1000000,0,0},{1080000,1200000,0,0},{1260000,1400000,0,0},{1440000,1600000,0,0},
  {1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},
  {1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},{1440000,1600000,0,0},
},
/* [19] 11AC_VHT80 */
{
  { 390000, 433300,0,0},{ 780000, 866700,0,0},{1170000,1300000,0,0},{1560000,1733300,0,0},
  {1950000,2166700,0,0},{2106000,2340000,0,0},{2730000,3033300,0,0},{3120000,3466700,0,0},
  {3120000,3466700,0,0},{3120000,3466700,0,0},{3120000,3466700,0,0},{3120000,3466700,0,0},
  {3120000,3466700,0,0},{3120000,3466700,0,0},{3120000,3466700,0,0},{3120000,3466700,0,0},
},
/* [20] 11AC_VHT160 */
{
  { 780000, 866700,0,0},{1560000,1733300,0,0},{2106000,2340000,0,0},{3120000,3466700,0,0},
  {3900000,4333300,0,0},{4680000,5200000,0,0},{5460000,6066700,0,0},{6240000,6933300,0,0},
  {6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},
  {6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},
},
/* [21] 11AC_VHT80_80 */
{
  { 780000, 866700,0,0},{1560000,1733300,0,0},{2106000,2340000,0,0},{3120000,3466700,0,0},
  {3900000,4333300,0,0},{4680000,5200000,0,0},{5460000,6066700,0,0},{6240000,6933300,0,0},
  {6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},
  {6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},{6240000,6933300,0,0},
},
/* [22] 11AXA_HE20, 5GHz HE20 */
{
  { 143400, 147700, 135400, 121900},{ 286800, 295500, 270800, 243800},
  { 430100, 443100, 406300, 365600},{ 573500, 590900, 541700, 487500},
  { 716900, 738600, 677100, 609400},{ 860300, 886400, 812500, 731300},
  {1003700,1034100, 947900, 853100},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
},
/* [23] 11AXG_HE20, 2GHz HE20 */
{
  { 143400, 147700, 135400, 121900},{ 286800, 295500, 270800, 243800},
  { 430100, 443100, 406300, 365600},{ 573500, 590900, 541700, 487500},
  { 716900, 738600, 677100, 609400},{ 860300, 886400, 812500, 731300},
  {1003700,1034100, 947900, 853100},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
  {1147100,1181800,1083300, 975000},{1147100,1181800,1083300, 975000},
},
/* [24] 11AXA_HE40PLUS */
{
  { 286800, 295500, 270800, 243800},{ 573500, 590900, 541700, 487500},
  { 860300, 886400, 812500, 731300},{1147100,1181800,1083300, 975000},
  {1433800,1477300,1354200,1218800},{1720600,1772700,1625000,1462500},
  {2007400,2068200,1895800,1706300},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
},
/* [25] 11AXA_HE40MINUS */
{
  { 286800, 295500, 270800, 243800},{ 573500, 590900, 541700, 487500},
  { 860300, 886400, 812500, 731300},{1147100,1181800,1083300, 975000},
  {1433800,1477300,1354200,1218800},{1720600,1772700,1625000,1462500},
  {2007400,2068200,1895800,1706300},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
},
/* [26] 11AXG_HE40PLUS */
{
  { 286800, 295500, 270800, 243800},{ 573500, 590900, 541700, 487500},
  { 860300, 886400, 812500, 731300},{1147100,1181800,1083300, 975000},
  {1433800,1477300,1354200,1218800},{1720600,1772700,1625000,1462500},
  {2007400,2068200,1895800,1706300},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
},
/* [27] 11AXG_HE40MINUS */
{
  { 286800, 295500, 270800, 243800},{ 573500, 590900, 541700, 487500},
  { 860300, 886400, 812500, 731300},{1147100,1181800,1083300, 975000},
  {1433800,1477300,1354200,1218800},{1720600,1772700,1625000,1462500},
  {2007400,2068200,1895800,1706300},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
},
/* [28] 11AXA_HE40 */
{
  { 286800, 295500, 270800, 243800},{ 573500, 590900, 541700, 487500},
  { 860300, 886400, 812500, 731300},{1147100,1181800,1083300, 975000},
  {1433800,1477300,1354200,1218800},{1720600,1772700,1625000,1462500},
  {2007400,2068200,1895800,1706300},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
},
/* [29] 11AXG_HE40 */
{
  { 286800, 295500, 270800, 243800},{ 573500, 590900, 541700, 487500},
  { 860300, 886400, 812500, 731300},{1147100,1181800,1083300, 975000},
  {1433800,1477300,1354200,1218800},{1720600,1772700,1625000,1462500},
  {2007400,2068200,1895800,1706300},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
  {2294100,2363600,2166700,1950000},{2294100,2363600,2166700,1950000},
},
/* [30] 11AXA_HE80 */
{
  { 600400, 618600, 567100, 510400},{1201000,1237300,1134300,1020800},
  {1801500,1855900,1701400,1531300},{2401900,2474500,2268500,2041600},
  {3002400,3093200,2835600,2552100},{3602900,3711800,3402800,3062500},
  {4203400,4330500,3969900,3572900},{4803900,4949000,4537000,4083300},
  {4803900,4949000,4537000,4083300},{4803900,4949000,4537000,4083300},
  {4803900,4949000,4537000,4083300},{4803900,4949000,4537000,4083300},
  {4803900,4949000,4537000,4083300},{4803900,4949000,4537000,4083300},
  {4803900,4949000,4537000,4083300},{4803900,4949000,4537000,4083300},
},
/* [31] 11AXA_HE160 */
{
  {1201000,1237300,1134200,1020800},{2401900,2474700,2268500,2041600},
  {3602900,3712000,3402800,3062500},{4803900,4949400,4537000,4083300},
  {6004900,6186700,5671300,5104100},{7205900,7424000,6805600,6125000},
  {8406800,8661400,7939800,7145800},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
},
/* [32] 11AXA_HE80_80 */
{
  {1201000,1237300,1134200,1020800},{2401900,2474700,2268500,2041600},
  {3602900,3712000,3402800,3062500},{4803900,4949400,4537000,4083300},
  {6004900,6186700,5671300,5104100},{7205900,7424000,6805600,6125000},
  {8406800,8661400,7939800,7145800},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
  {9607800,9898800,9074000,8166600},{9607800,9898800,9074000,8166600},
},
/* [33] 11BEA_EHT20, 5G EHT20 */
{
  { 172100, 177300, 162500, 145300},{ 344200, 354600, 325000, 290660},
  { 516300, 531900, 487500, 435990},{ 688400, 709200, 650000, 581320},
  { 860500, 886500, 812500, 726650},{1032600,1063800, 975000, 871980},
  {1204700,1241100,1137500,1017310},{1376800,1418400,1300000,1162640},
  {1548900,1595700,1462500,1307970},{1721000,1773000,1625000,1453300},
  {1893100,1950300,1787500,1598630},{2065200,2127600,1950000,1743960},
  {2237300,2304900,2112500,1888900},{2409400,2482200,2275000,2034200},
  {2581500,2659500,2437500,2179500},{2753600,2836800,2600000,2325280},
},
/* [34] 11BEG_EHT20, 2.4G EHT20 */
{
  { 172100, 177300, 162500, 145300},{ 344200, 354600, 325000, 290660},
  { 516300, 531900, 487500, 435990},{ 688400, 709200, 650000, 581320},
  { 860500, 886500, 812500, 726650},{1032600,1063800, 975000, 871980},
  {1204700,1241100,1137500,1017310},{1376800,1418400,1300000,1162640},
  {1548900,1595700,1462500,1307970},{1721000,1773000,1625000,1453300},
  {1893100,1950300,1787500,1598630},{2065200,2127600,1950000,1743960},
  {2237300,2304900,2112500,1888900},{2409400,2482200,2275000,2034200},
  {2581500,2659500,2437500,2179500},{2753600,2836800,2600000,2325280},
},
/* [35] 11BEA_EHT40PLUS */
{
  { 344100, 354500, 325000, 292500},{ 688200, 709000, 650000, 585000},
  {1032300,1063500, 975000, 877500},{1376400,1418000,1300000,1170000},
  {1720500,1772500,1625000,1462500},{2064600,2127000,1950000,1755000},
  {2408700,2481500,2275000,2047500},{2752800,2836000,2600000,2340000},
  {3096900,3190500,2925000,2632500},{3441000,3545000,3250000,2925000},
  {3785100,3899500,3575000,3217500},{4129200,4254000,3900000,3510000},
  {4473300,4608500,4225000,2802500},{4817400,4963000,4550000,4095000},
  {5161500,5317500,4875000,4387500},{5505600,5672000,5200000,4680000},
},
/* [36] 11BEA_EHT40MINUS */
{
  { 344100, 354500, 325000, 292500},{ 688200, 709000, 650000, 585000},
  {1032300,1063500, 975000, 877500},{1376400,1418000,1300000,1170000},
  {1720500,1772500,1625000,1462500},{2064600,2127000,1950000,1755000},
  {2408700,2481500,2275000,2047500},{2752800,2836000,2600000,2340000},
  {3096900,3190500,2925000,2632500},{3441000,3545000,3250000,2925000},
  {3785100,3899500,3575000,3217500},{4129200,4254000,3900000,3510000},
  {4473300,4608500,4225000,2802500},{4817400,4963000,4550000,4095000},
  {5161500,5317500,4875000,4387500},{5505600,5672000,5200000,4680000},
},
/* [37] 11BEG_EHT40PLUS */
{
  { 344100, 354500, 325000, 292500},{ 688200, 709000, 650000, 585000},
  {1032300,1063500, 975000, 877500},{1376400,1418000,1300000,1170000},
  {1720500,1772500,1625000,1462500},{2064600,2127000,1950000,1755000},
  {2408700,2481500,2275000,2047500},{2752800,2836000,2600000,2340000},
  {3096900,3190500,2925000,2632500},{3441000,3545000,3250000,2925000},
  {3785100,3899500,3575000,3217500},{4129200,4254000,3900000,3510000},
  {4473300,4608500,4225000,2802500},{4817400,4963000,4550000,4095000},
  {5161500,5317500,4875000,4387500},{5505600,5672000,5200000,4680000},
},
/* [38] 11BEG_EHT40MINUS */
{
  { 344100, 354500, 325000, 292500},{ 688200, 709000, 650000, 585000},
  {1032300,1063500, 975000, 877500},{1376400,1418000,1300000,1170000},
  {1720500,1772500,1625000,1462500},{2064600,2127000,1950000,1755000},
  {2408700,2481500,2275000,2047500},{2752800,2836000,2600000,2340000},
  {3096900,3190500,2925000,2632500},{3441000,3545000,3250000,2925000},
  {3785100,3899500,3575000,3217500},{4129200,4254000,3900000,3510000},
  {4473300,4608500,4225000,2802500},{4817400,4963000,4550000,4095000},
  {5161500,5317500,4875000,4387500},{5505600,5672000,5200000,4680000},
},
/* [39] 11BEA_EHT40 */
{
  { 344100, 354500, 325000, 292500},{ 688200, 709000, 650000, 585000},
  {1032300,1063500, 975000, 877500},{1376400,1418000,1300000,1170000},
  {1720500,1772500,1625000,1462500},{2064600,2127000,1950000,1755000},
  {2408700,2481500,2275000,2047500},{2752800,2836000,2600000,2340000},
  {3096900,3190500,2925000,2632500},{3441000,3545000,3250000,2925000},
  {3785100,3899500,3575000,3217500},{4129200,4254000,3900000,3510000},
  {4473300,4608500,4225000,2802500},{4817400,4963000,4550000,4095000},
  {5161500,5317500,4875000,4387500},{5505600,5672000,5200000,4680000},
},
/* [40] 11BEG_EHT40 */
{
  { 344100, 354500, 325000, 292500},{ 688200, 709000, 650000, 585000},
  {1032300,1063500, 975000, 877500},{1376400,1418000,1300000,1170000},
  {1720500,1772500,1625000,1462500},{2064600,2127000,1950000,1755000},
  {2408700,2481500,2275000,2047500},{2752800,2836000,2600000,2340000},
  {3096900,3190500,2925000,2632500},{3441000,3545000,3250000,2925000},
  {3785100,3899500,3575000,3217500},{4129200,4254000,3900000,3510000},
  {4473300,4608500,4225000,2802500},{4817400,4963000,4550000,4095000},
  {5161500,5317500,4875000,4387500},{5505600,5672000,5200000,4680000},
},
/* [41] 11BEA_EHT80 */
{
  {  720600,  742400,  680600,  612500},{ 1441200, 1484800, 1361200, 1225000},
  { 2161800, 2227200, 2041800, 1837500},{ 2882400, 2969600, 2722400, 2450000},
  { 3603000, 3712000, 3403000, 3062500},{ 4323600, 4454400, 4083600, 3675000},
  { 5044200, 5196800, 4764200, 4287500},{ 5764800, 5939200, 5444800, 4900000},
  { 6485400, 6681600, 6125400, 5512500},{ 7206000, 7424000, 6806000, 6125000},
  { 7926600, 8166400, 7486600, 6737500},{ 8647200, 8908800, 8167200, 7350000},
  { 9367800, 9651200, 8847800, 7962500},{10088400,10393600, 9528400, 8575000},
  {10809000,11136000,10209000, 9187500},{11529600,11878400,10889600, 9800000},
},
/* [42] 11BEA_EHT160 */
{
  { 1441200, 1484800, 1361100, 1225000},{ 2882400, 2969600, 2722200, 2450000},
  { 4323600, 4454400, 4083300, 3675000},{ 5764800, 5939200, 5444400, 4900000},
  { 7206000, 7424000, 6805500, 6125000},{ 8647200, 8908800, 8166600, 7350000},
  {10088400,10393600, 9527700, 8575000},{11529600,11878400,10888800, 9800000},
  {12970800,13363200,12249900,11025000},{14412000,14848000,13611000,12250000},
  {15853200,16332800,14972100,13475000},{17294400,17817600,16333200,14700000},
  {18735600,19302400,17694300,15925000},{20176800,20787200,19055400,17150000},
  {21618000,22272000,20416500,18375000},{23059200,23756800,21777600,19600000},
},
/* [43] 11BEA_EHT320 */
{
  { 2882400, 2969700, 2722200, 2450000},{ 5764800, 5939400, 5444400, 4900000},
  { 8647200, 8909100, 8166600, 7350000},{11529600,11878800,10888800, 9800000},
  {14415000,14848500,13611000,12250000},{17294400,17818200,16333200,14700000},
  {20176800,20787900,19055400,17150000},{23059200,23757600,21777600,19600000},
  {25941600,26727300,24499800,22050000},{28824000,29697000,27222000,24500000},
  {31706400,32666700,29944200,26950000},{34588800,35636400,32666400,29400000},
  {37471200,38606100,35388600,31850000},{40353600,41575800,38110800,34300000},
  {43236000,44545500,40833000,36750000},{46118400,47515200,43555200,39200000},
},
/* [44–57] NG_VHT + VHT_1024_QAM — zero-filled, not indexed by UD translation */
{
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
},
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
{ {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0} },
/* [58] EHT_PUNC: 11BEA_EHT80 80-20MHz */
{
  { 516200, 531800, 487500, 438800},{1032400,1063600, 975000, 877600},
  {1548600,1595400,1462500,1316400},{2064800,2127200,1950000,1755200},
  {2581000,2659000,2437500,2194000},{3097200,3190800,2925000,2632800},
  {3613400,3722600,3412500,3071600},{4129600,4254400,3900000,3510400},
  {4645800,4786200,4387500,3949200},{5162000,5318000,4875000,4388000},
  {5678200,5849800,5362500,4826800},{6194400,6381600,5850000,5265600},
  {6710600,6913400,6337500,5704400},{7226800,7445200,6825000,6143200},
  {7743000,7977000,7312500,6582000},{8259200,8508800,7800000,7020800},
},
/* [59] EHT_PUNC: 11BEA_EHT160 160-20 */
{
  { 1236800, 1274200, 1168100, 1051300},{ 2473600, 2548400, 2336200, 2102600},
  { 3710400, 3822600, 3504300, 3159300},{ 4947200, 5096800, 4672400, 4205200},
  { 6184000, 6371000, 5840500, 5256500},{ 7620800, 7645200, 7008600, 6307800},
  { 8657600, 8919400, 8176700, 7359100},{ 9894400,10193600, 9344800, 8410400},
  {11131200,11467800,10512900, 9461700},{12368000,12742000,11681000,10513000},
  {13604800,14076200,12849100,11564300},{14841600,15290400,14017200,12615600},
  {16078400,16564600,15185300,13666900},{17315200,17838800,16353400,14718200},
  {18552000,19113000,17521500,15769500},{19788800,20387200,18689600,16820800},
},
/* [60] EHT_PUNC: 11BEA_EHT160 160-40 */
{
  { 1064700, 1097000, 1005600,  905000},{ 2129400, 2194000, 2011200, 1810000},
  { 3194100, 3291000, 3016800, 2715000},{ 4258800, 4388000, 4022400, 3620000},
  { 5323500, 5485000, 2011200, 4525000},{ 6388200, 6582000, 3016800, 5430000},
  { 7452900, 7679000, 6033600, 6335000},{ 8517600, 8776000, 7039200, 7240000},
  { 9582300, 9873000, 8044800, 8145000},{10647000,10970000, 9050400, 9050000},
  {11711700,12067000,10056000, 9955000},{12776400,13164000,11061600,10860000},
  {13841100,14261000,12067200,11765000},{14905800,15358000,13072800,12670000},
  {15970500,16455000,14078400,13575000},{17035200,17552000,15084000,14480000},
},
/* [61] EHT_PUNC: 11BEA_EHT320 320-40 */
{
  { 2505900, 2309000, 2366700, 2130000},{ 5011800, 4618000, 4733400, 4260000},
  { 7517700, 6927000, 7100100, 6390000},{10023600, 9236000, 9466800, 8520000},
  {12529500,11545000,11833500,10650000},{15035400,13854000,14200200,12780000},
  {17541300,16163000,16566900,14910000},{20047200,18472000,18933600,17040000},
  {22553100,20781000,21300300,19170000},{25059000,23090000,23667000,21300000},
  {27564900,25399000,26033700,23430000},{30070800,27708000,28400400,25560000},
  {32576700,30017000,30767100,27690000},{35082600,32326000,33133800,29820000},
  {37588500,34635000,35500500,31950000},{40094400,36944000,37867200,34080000},
},
/* [62] EHT_PUNC: 11BEA_EHT320 320-80 */
{
  { 2161800,  2227200,  2041700,  1837500},{ 4323600,  4454400,  4083400,  3675000},
  { 6485400,  6681600,  6125100,  5512500},{ 8647200,  8908800,  8166800,  7350000},
  {10809000, 11136000, 10208500,  9187500},{12970800, 13363200, 12250200, 11025000},
  {15132600, 15590400, 14291900, 12862500},{17294400, 17817600, 16333600, 14700000},
  {19456200, 20044800, 18375300, 16537500},{21618000, 22272000, 20417000, 18375000},
  {23779800, 24499200, 22458700, 20212500},{25941600, 26726400, 24500400, 22050000},
  {28103400, 28953600, 26542100, 23887500},{30265200, 31180800, 28583800, 25725000},
  {32427000, 33408000, 30625500, 27562500},{34588800, 35635200, 32667200, 29400000},
},
/* [63] EHT_PUNC: 11BEA_EHT320 320-120 */
{
  { 1785300,  1839300,  1686100,  1517500},{ 3570600,  3678600,  3372200,  3035000},
  { 5355900,  5517900,  5058300,  4552500},{ 7141200,  7357200,  6744400,  6070000},
  { 8926500,  9196500,  8430500,  7587500},{10711800, 11035800, 10116600,  9105000},
  {12497100, 12875100, 11802700, 10622500},{14282400, 14714400, 13488800, 12140000},
  {16067700, 16553700, 15174900, 13657500},{17853000, 18393000, 16861000, 15175000},
  {19638300, 20232300, 18547100, 16692500},{21423600, 22071600, 20233200, 18210000},
  {23208900, 23910900, 21919300, 19727500},{24994200, 25750200, 23605400, 21245000},
  {26779500, 27589500, 25291500, 22762500},{28564800, 29428800, 26977600, 24280000},
},
};

/**
 * ath12k_eht_punc_to_mode_idx - Map EHT puncturing bitmap to table row
 * @punc_bitmap: puncturing bitmap (each set bit = 20 MHz sub-channel punctured)
 * @bw: operating bandwidth as ieee80211_sta_rx_bw
 *
 * Returns ATH12K_EHT_PUNC_* row index on match, -1 for no puncturing
 * or an unrecognised pattern (caller falls through to non-punctured EHT row).
 */
static int ath12k_eht_punc_to_mode_idx(u32 punc_bitmap, u8 bw)
{
	int punc_mhz;

	if (!punc_bitmap)
		return -1;
	punc_mhz = hweight32(punc_bitmap) * 20;
	switch (bw) {
	case IEEE80211_STA_RX_BW_80:
		if (punc_mhz == 20)
			return ATH12K_EHT_PUNC_80_20;
		break;
	case IEEE80211_STA_RX_BW_160:
		if (punc_mhz == 20)
			return ATH12K_EHT_PUNC_160_20;
		if (punc_mhz == 40)
			return ATH12K_EHT_PUNC_160_40;
		break;
	case IEEE80211_STA_RX_BW_320:
		if (punc_mhz == 40)
			return ATH12K_EHT_PUNC_320_40;
		if (punc_mhz == 80)
			return ATH12K_EHT_PUNC_320_80;
		if (punc_mhz == 120)
			return ATH12K_EHT_PUNC_320_120;
		break;
	default:
		break;
	}
	return -1;
}

/**
 * ath12k_chan_width_to_sta_bw - Convert nl80211 channel width to sta RX bandwidth
 * @w: nl80211_chan_width from cfg80211_chan_def
 *
 * Returns the corresponding ieee80211_sta_rx_bw value; defaults to BW_20 for
 * any unrecognised width.
 */
static u8 ath12k_chan_width_to_sta_bw(enum nl80211_chan_width w)
{
	switch (w) {
	case NL80211_CHAN_WIDTH_40:
		return IEEE80211_STA_RX_BW_40;
	case NL80211_CHAN_WIDTH_80:
		return IEEE80211_STA_RX_BW_80;
	case NL80211_CHAN_WIDTH_80P80:
		/* fall through */
	case NL80211_CHAN_WIDTH_160:
		return IEEE80211_STA_RX_BW_160;
	case NL80211_CHAN_WIDTH_320:
		return IEEE80211_STA_RX_BW_320;
	default:
		return IEEE80211_STA_RX_BW_20;
	}
}

/**
 * ath12k_get_mode_table_idx - Resolve PHY mode to ath12k_max_rates table row
 * @bw: operating bandwidth (IEEE80211_STA_RX_BW_* cast to u8)
 * @band: NL80211_BAND_* cast to u8
 * @has_eht: AP is advertising EHT capabilities
 * @has_he: AP is advertising HE capabilities
 * @has_vht: radio supports VHT
 * @has_ht: radio supports HT
 * @punc_bitmap: EHT puncturing bitmap (0 if none)
 *
 * Priority: EHT with puncturing > EHT > HE > VHT > HT > Legacy.
 * Rows 44-57 (NG_VHT, VHT_1024_QAM) are never returned.
 *
 * Returns: row index into ath12k_max_rates[].
 */
static int ath12k_get_mode_table_idx(u8 bw, u8 band,
			       bool has_eht, bool has_he,
			       bool has_vht, bool has_ht,
			       u32 punc_bitmap)
{
	bool is_2g = (band == NL80211_BAND_2GHZ);

	if (has_eht && !is_2g && punc_bitmap) {
		int punc_idx = ath12k_eht_punc_to_mode_idx(punc_bitmap, bw);

		if (punc_idx >= 0)
			return punc_idx;
	}

	if (has_eht) {
		if (is_2g)
			return (bw >= IEEE80211_STA_RX_BW_40) ?
				ATH12K_IEEE80211_MODE_11BEG_EHT40 :
				ATH12K_IEEE80211_MODE_11BEG_EHT20;
		switch (bw) {
		case IEEE80211_STA_RX_BW_320:
			return ATH12K_IEEE80211_MODE_11BEA_EHT320;
		case IEEE80211_STA_RX_BW_160:
			return ATH12K_IEEE80211_MODE_11BEA_EHT160;
		case IEEE80211_STA_RX_BW_80:
			return ATH12K_IEEE80211_MODE_11BEA_EHT80;
		case IEEE80211_STA_RX_BW_40:
			return ATH12K_IEEE80211_MODE_11BEA_EHT40;
		default:
			return ATH12K_IEEE80211_MODE_11BEA_EHT20;
		}
	}

	if (has_he) {
		if (is_2g)
			return (bw >= IEEE80211_STA_RX_BW_40) ?
				ATH12K_IEEE80211_MODE_11AXG_HE40 :
				ATH12K_IEEE80211_MODE_11AXG_HE20;
		switch (bw) {
		case IEEE80211_STA_RX_BW_160:
			return ATH12K_IEEE80211_MODE_11AXA_HE160;
		case IEEE80211_STA_RX_BW_80:
			return ATH12K_IEEE80211_MODE_11AXA_HE80;
		case IEEE80211_STA_RX_BW_40:
			return ATH12K_IEEE80211_MODE_11AXA_HE40;
		default:
			return ATH12K_IEEE80211_MODE_11AXA_HE20;
		}
	}

	if (has_vht) {
		switch (bw) {
		case IEEE80211_STA_RX_BW_160:
			return ATH12K_IEEE80211_MODE_11AC_VHT160;
		case IEEE80211_STA_RX_BW_80:
			return ATH12K_IEEE80211_MODE_11AC_VHT80;
		case IEEE80211_STA_RX_BW_40:
			return ATH12K_IEEE80211_MODE_11AC_VHT40;
		default:
			return ATH12K_IEEE80211_MODE_11AC_VHT20;
		}
	}

	if (has_ht) {
		if (is_2g)
			return (bw >= IEEE80211_STA_RX_BW_40) ?
				ATH12K_IEEE80211_MODE_11NG_HT40 :
				ATH12K_IEEE80211_MODE_11NG_HT20;
		return (bw >= IEEE80211_STA_RX_BW_40) ?
			ATH12K_IEEE80211_MODE_11NA_HT40 :
			ATH12K_IEEE80211_MODE_11NA_HT20;
	}

	return is_2g ? ATH12K_IEEE80211_MODE_11G : ATH12K_IEEE80211_MODE_11A;
}

/**
 * ath12k_maxrate_lookup - Bounds-checked lookup into ath12k_max_rates
 * @mode_idx: PHY mode row (ATH12K_IEEE80211_MODE_*)
 * @nss: spatial streams (1-ATH12K_MAX_NSS)
 * @gi_idx: guard interval index (ATH12K_RATE_GI_*)
 *
 * Returns: rate in Kbps, or 0 if any parameter is out of range.
 */
static u32 ath12k_maxrate_lookup(int mode_idx, u8 nss, u8 gi_idx)
{
	if (mode_idx < 0 || mode_idx >= ATH12K_MODE_TABLE_SIZE ||
	    nss == 0 || nss > ATH12K_MAX_NSS ||
	    gi_idx >= ATH12K_MAX_GI_MODE)
		return 0;
	return ath12k_max_rates[mode_idx][nss - 1][gi_idx];
}

/**
 * ath12k_maxrate_for_vif - Get maximum PHY rate for a VAP link
 * @arvif: pointer to the per-link VAP structure
 * @value: pointer to store the maximum rate in Kbps
 *
 * Looks up the theoretical maximum rate from ath12k_max_rates[][][] using the
 * current operating channel width, hardware TX chain count, and configured GI.
 *
 * Note: bss_conf operational flags (eht_support, he_support) are used instead
 * of raw hardware capability flags.  On EHT-capable chips all radios report
 * has_eht=1 in hardware, so capability flags alone would always select EHT
 * regardless of the operating mode configured in hostapd.  bss_conf reflects
 * what the AP is actually advertising in its beacons.
 *
 * Returns: 0 on success, negative error code on failure.
 */
static int ath12k_maxrate_for_vif(struct ath12k_link_vif *arvif, u64 *value)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_vif *vif;
	struct ieee80211_bss_conf *bss_conf;
	struct cfg80211_chan_def def;
	struct cfg80211_bitrate_mask *mask = &arvif->bitrate_mask;
	enum nl80211_band band;
	u8 bw, gi_idx, nss;
	int mode_idx;
	u32 rate;

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif)
		return -EINVAL;
	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		return -ENOTCONN;

	band = def.chan->band;
	bw = ath12k_chan_width_to_sta_bw(def.width);

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	mode_idx = ath12k_get_mode_table_idx(bw, band,
					     bss_conf && bss_conf->eht_support,
					     bss_conf && bss_conf->he_support,
					     ar->mac.sbands[band].vht_cap.vht_supported,
					     ar->mac.sbands[band].ht_cap.ht_supported,
					     arvif->punct_bitmap);

	nss = ar->num_tx_chains;
	if (!nss)
		return -EINVAL;
	if (bw == IEEE80211_STA_RX_BW_160)
		nss = ath12k_get_nss_160mhz(ar, nss);
	else if (bw == IEEE80211_STA_RX_BW_320)
		nss = ath12k_get_nss_320mhz(ar, nss);
	if (!nss)
		nss = 1;

	/* GI: from bitrate_mask, or default to best rate */
	if (bss_conf && bss_conf->eht_support) {
		switch (mask->control[band].eht_gi) {
		case NL80211_RATE_INFO_EHT_GI_1_6:
			gi_idx = ATH12K_RATE_GI_1DOT6_US;
			break;
		case NL80211_RATE_INFO_EHT_GI_3_2:
			gi_idx = ATH12K_RATE_GI_3DOT2_US;
			break;
		default: /* NL80211_RATE_INFO_EHT_GI_0_8 */
			gi_idx = ATH12K_RATE_GI_0DOT8_US;
			break;
		}
	} else if (bss_conf && bss_conf->he_support) {
		switch (mask->control[band].he_gi) {
		case NL80211_RATE_INFO_HE_GI_1_6:
			gi_idx = ATH12K_RATE_GI_1DOT6_US;
			break;
		case NL80211_RATE_INFO_HE_GI_3_2:
			gi_idx = ATH12K_RATE_GI_3DOT2_US;
			break;
		default: /* NL80211_RATE_INFO_HE_GI_0_8 */
			gi_idx = ATH12K_RATE_GI_0DOT8_US;
			break;
		}
	} else {
		gi_idx = (mask->control[band].gi == NL80211_TXRATE_FORCE_LGI) ?
			  ATH12K_RATE_GI_0DOT8_US : ATH12K_RATE_GI_0DOT4_US;
	}

	rate = ath12k_maxrate_lookup(mode_idx, nss, gi_idx);
	if (!rate)
		return -ENODATA;

	*value = rate;
	return 0;
}

/* Get link-vif level parameters */
/*
 * NDBPS formula tables for fixed-rate path.
 * Values use NDBPS×1000 scaling for integer Kbps arithmetic.
 * Rate formulas: HT/VHT LGI: nbps*nss/4    SGI: nbps*nss*5/18
 *                HE/EHT 0.8us: nbps*nss*5/68  1.6us: *5/72  3.2us: /16
 */
enum ath12k_cwm_width {
	ATH12K_CWM_WIDTH20  = 0,
	ATH12K_CWM_WIDTH40  = 1,
	ATH12K_CWM_WIDTH80  = 2,
	ATH12K_CWM_WIDTH160 = 3,
	ATH12K_CWM_WIDTH320 = 4,
};

static enum ath12k_cwm_width
ath12k_nl80211_width_to_cwm(enum nl80211_chan_width w)
{
	switch (w) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:     return ATH12K_CWM_WIDTH20;
	case NL80211_CHAN_WIDTH_40:     return ATH12K_CWM_WIDTH40;
	case NL80211_CHAN_WIDTH_80:     return ATH12K_CWM_WIDTH80;
	case NL80211_CHAN_WIDTH_80P80:
	case NL80211_CHAN_WIDTH_160:    return ATH12K_CWM_WIDTH160;
	case NL80211_CHAN_WIDTH_320:    return ATH12K_CWM_WIDTH320;
	default:                        return ATH12K_CWM_WIDTH20;
	}
}

/* HT/VHT NDBPS table(BW × MCS0-11) */
static const int ath12k_vht_ht_rate_tbl[][3] = {
   /*BW                    MCS    NDBPS×1000*/
    {ATH12K_CWM_WIDTH20,    0,     26000},
    {ATH12K_CWM_WIDTH20,    1,     52000},
    {ATH12K_CWM_WIDTH20,    2,     78000},
    {ATH12K_CWM_WIDTH20,    3,    104000},
    {ATH12K_CWM_WIDTH20,    4,    156000},
    {ATH12K_CWM_WIDTH20,    5,    208000},
    {ATH12K_CWM_WIDTH20,    6,    234000},
    {ATH12K_CWM_WIDTH20,    7,    260000},
    {ATH12K_CWM_WIDTH20,    8,    312000},
    {ATH12K_CWM_WIDTH20,    9,    346680},
    {ATH12K_CWM_WIDTH20,   10,    390000},
    {ATH12K_CWM_WIDTH20,   11,    433000},
    {ATH12K_CWM_WIDTH40,    0,     54000},
    {ATH12K_CWM_WIDTH40,    1,    108000},
    {ATH12K_CWM_WIDTH40,    2,    162000},
    {ATH12K_CWM_WIDTH40,    3,    216000},
    {ATH12K_CWM_WIDTH40,    4,    324000},
    {ATH12K_CWM_WIDTH40,    5,    432000},
    {ATH12K_CWM_WIDTH40,    6,    486000},
    {ATH12K_CWM_WIDTH40,    7,    540000},
    {ATH12K_CWM_WIDTH40,    8,    648000},
    {ATH12K_CWM_WIDTH40,    9,    720000},
    {ATH12K_CWM_WIDTH40,   10,    810000},
    {ATH12K_CWM_WIDTH40,   11,    900000},
    {ATH12K_CWM_WIDTH80,    0,    117000},
    {ATH12K_CWM_WIDTH80,    1,    234000},
    {ATH12K_CWM_WIDTH80,    2,    351000},
    {ATH12K_CWM_WIDTH80,    3,    468000},
    {ATH12K_CWM_WIDTH80,    4,    702000},
    {ATH12K_CWM_WIDTH80,    5,    936000},
    {ATH12K_CWM_WIDTH80,    6,   1053000},
    {ATH12K_CWM_WIDTH80,    7,   1170000},
    {ATH12K_CWM_WIDTH80,    8,   1404000},
    {ATH12K_CWM_WIDTH80,    9,   1560000},
    {ATH12K_CWM_WIDTH80,   10,   1755000},
    {ATH12K_CWM_WIDTH80,   11,   1950000},
    {ATH12K_CWM_WIDTH160,   0,    234000},
    {ATH12K_CWM_WIDTH160,   1,    468000},
    {ATH12K_CWM_WIDTH160,   2,    702000},
    {ATH12K_CWM_WIDTH160,   3,    936000},
    {ATH12K_CWM_WIDTH160,   4,   1404000},
    {ATH12K_CWM_WIDTH160,   5,   1872000},
    {ATH12K_CWM_WIDTH160,   6,   2106000},
    {ATH12K_CWM_WIDTH160,   7,   2340000},
    {ATH12K_CWM_WIDTH160,   8,   2808000},
    {ATH12K_CWM_WIDTH160,   9,   3120000},
    {ATH12K_CWM_WIDTH160,  10,   3510000},
    {ATH12K_CWM_WIDTH160,  11,   3900000},
};

/* HE NDBPS table (BW × MCS0-13) */
static const int ath12k_he_rate_tbl[][3] = {
   /*BW                    MCS    NDBPS×1000*/
    {ATH12K_CWM_WIDTH20,    0,    117000},
    {ATH12K_CWM_WIDTH20,    1,    234000},
    {ATH12K_CWM_WIDTH20,    2,    351000},
    {ATH12K_CWM_WIDTH20,    3,    468000},
    {ATH12K_CWM_WIDTH20,    4,    702000},
    {ATH12K_CWM_WIDTH20,    5,    936000},
    {ATH12K_CWM_WIDTH20,    6,   1053000},
    {ATH12K_CWM_WIDTH20,    7,   1170000},
    {ATH12K_CWM_WIDTH20,    8,   1404000},
    {ATH12K_CWM_WIDTH20,    9,   1560000},
    {ATH12K_CWM_WIDTH20,   10,   1755000},
    {ATH12K_CWM_WIDTH20,   11,   1950000},
    {ATH12K_CWM_WIDTH20,   12,   2106000},
    {ATH12K_CWM_WIDTH20,   13,   2340000},
    {ATH12K_CWM_WIDTH40,    0,    234000},
    {ATH12K_CWM_WIDTH40,    1,    468000},
    {ATH12K_CWM_WIDTH40,    2,    702000},
    {ATH12K_CWM_WIDTH40,    3,    936000},
    {ATH12K_CWM_WIDTH40,    4,   1404000},
    {ATH12K_CWM_WIDTH40,    5,   1872000},
    {ATH12K_CWM_WIDTH40,    6,   2106000},
    {ATH12K_CWM_WIDTH40,    7,   2340000},
    {ATH12K_CWM_WIDTH40,    8,   2808000},
    {ATH12K_CWM_WIDTH40,    9,   3120000},
    {ATH12K_CWM_WIDTH40,   10,   3510000},
    {ATH12K_CWM_WIDTH40,   11,   3900000},
    {ATH12K_CWM_WIDTH40,   12,   4212000},
    {ATH12K_CWM_WIDTH40,   13,   4680000},
    {ATH12K_CWM_WIDTH80,    0,    490000},
    {ATH12K_CWM_WIDTH80,    1,    980000},
    {ATH12K_CWM_WIDTH80,    2,   1470000},
    {ATH12K_CWM_WIDTH80,    3,   1960000},
    {ATH12K_CWM_WIDTH80,    4,   2940000},
    {ATH12K_CWM_WIDTH80,    5,   3920000},
    {ATH12K_CWM_WIDTH80,    6,   4410000},
    {ATH12K_CWM_WIDTH80,    7,   4900000},
    {ATH12K_CWM_WIDTH80,    8,   5880000},
    {ATH12K_CWM_WIDTH80,    9,   6533000},
    {ATH12K_CWM_WIDTH80,   10,   7350000},
    {ATH12K_CWM_WIDTH80,   11,   8166000},
    {ATH12K_CWM_WIDTH80,   12,   8820000},
    {ATH12K_CWM_WIDTH80,   13,   9800000},
    {ATH12K_CWM_WIDTH160,   0,    980000},
    {ATH12K_CWM_WIDTH160,   1,   1960000},
    {ATH12K_CWM_WIDTH160,   2,   2940000},
    {ATH12K_CWM_WIDTH160,   3,   3920000},
    {ATH12K_CWM_WIDTH160,   4,   5880000},
    {ATH12K_CWM_WIDTH160,   5,   7840000},
    {ATH12K_CWM_WIDTH160,   6,   8820000},
    {ATH12K_CWM_WIDTH160,   7,   9800000},
    {ATH12K_CWM_WIDTH160,   8,  11760000},
    {ATH12K_CWM_WIDTH160,   9,  13066000},
    {ATH12K_CWM_WIDTH160,  10,  14700000},
    {ATH12K_CWM_WIDTH160,  11,  16333000},
    {ATH12K_CWM_WIDTH160,  12,  17640000},
    {ATH12K_CWM_WIDTH160,  13,  19600000},
};

/* EHT NDBPS table (BW × MCS0-15 × puncturing type) */
static const int ath12k_eht_rate_tbl[][4] = {
   /*BW                    MCS  PUNC  NDBPS×1000*/
    {ATH12K_CWM_WIDTH20,    0,  0,    117000},
    {ATH12K_CWM_WIDTH20,    1,  0,    234000},
    {ATH12K_CWM_WIDTH20,    2,  0,    351000},
    {ATH12K_CWM_WIDTH20,    3,  0,    468000},
    {ATH12K_CWM_WIDTH20,    4,  0,    702000},
    {ATH12K_CWM_WIDTH20,    5,  0,    936000},
    {ATH12K_CWM_WIDTH20,    6,  0,   1053000},
    {ATH12K_CWM_WIDTH20,    7,  0,   1170000},
    {ATH12K_CWM_WIDTH20,    8,  0,   1404000},
    {ATH12K_CWM_WIDTH20,    9,  0,   1560000},
    {ATH12K_CWM_WIDTH20,   10,  0,   1755000},
    {ATH12K_CWM_WIDTH20,   11,  0,   1950000},
    {ATH12K_CWM_WIDTH20,   12,  0,   2106000},
    {ATH12K_CWM_WIDTH20,   13,  0,   2340000},
    {ATH12K_CWM_WIDTH20,   14,  0,         0},
    {ATH12K_CWM_WIDTH20,   15,  0,     58000},
    {ATH12K_CWM_WIDTH40,    0,  0,    234000},
    {ATH12K_CWM_WIDTH40,    1,  0,    468000},
    {ATH12K_CWM_WIDTH40,    2,  0,    702000},
    {ATH12K_CWM_WIDTH40,    3,  0,    936000},
    {ATH12K_CWM_WIDTH40,    4,  0,   1404000},
    {ATH12K_CWM_WIDTH40,    5,  0,   1872000},
    {ATH12K_CWM_WIDTH40,    6,  0,   2106000},
    {ATH12K_CWM_WIDTH40,    7,  0,   2340000},
    {ATH12K_CWM_WIDTH40,    8,  0,   2808000},
    {ATH12K_CWM_WIDTH40,    9,  0,   3120000},
    {ATH12K_CWM_WIDTH40,   10,  0,   3510000},
    {ATH12K_CWM_WIDTH40,   11,  0,   3900000},
    {ATH12K_CWM_WIDTH40,   12,  0,   4212000},
    {ATH12K_CWM_WIDTH40,   13,  0,   4680000},
    {ATH12K_CWM_WIDTH40,   14,  0,         0},
    {ATH12K_CWM_WIDTH40,   15,  0,    117000},
    {ATH12K_CWM_WIDTH80,    0,  0,    490000},
    {ATH12K_CWM_WIDTH80,    1,  0,    980000},
    {ATH12K_CWM_WIDTH80,    2,  0,   1470000},
    {ATH12K_CWM_WIDTH80,    3,  0,   1960000},
    {ATH12K_CWM_WIDTH80,    4,  0,   2940000},
    {ATH12K_CWM_WIDTH80,    5,  0,   3920000},
    {ATH12K_CWM_WIDTH80,    6,  0,   4410000},
    {ATH12K_CWM_WIDTH80,    7,  0,   4900000},
    {ATH12K_CWM_WIDTH80,    8,  0,   5880000},
    {ATH12K_CWM_WIDTH80,    9,  0,   6533000},
    {ATH12K_CWM_WIDTH80,   10,  0,   7350000},
    {ATH12K_CWM_WIDTH80,   11,  0,   8166000},
    {ATH12K_CWM_WIDTH80,   12,  0,   8820000},
    {ATH12K_CWM_WIDTH80,   13,  0,   9800000},
    {ATH12K_CWM_WIDTH80,   14,  0,    117000},
    {ATH12K_CWM_WIDTH80,   15,  0,    245000},
    {ATH12K_CWM_WIDTH160,   0,  0,    980000},
    {ATH12K_CWM_WIDTH160,   1,  0,   1960000},
    {ATH12K_CWM_WIDTH160,   2,  0,   2940000},
    {ATH12K_CWM_WIDTH160,   3,  0,   3920000},
    {ATH12K_CWM_WIDTH160,   4,  0,   5880000},
    {ATH12K_CWM_WIDTH160,   5,  0,   7840000},
    {ATH12K_CWM_WIDTH160,   6,  0,   8820000},
    {ATH12K_CWM_WIDTH160,   7,  0,   9800000},
    {ATH12K_CWM_WIDTH160,   8,  0,  11760000},
    {ATH12K_CWM_WIDTH160,   9,  0,  13066000},
    {ATH12K_CWM_WIDTH160,  10,  0,  14700000},
    {ATH12K_CWM_WIDTH160,  11,  0,  16333000},
    {ATH12K_CWM_WIDTH160,  12,  0,  17640000},
    {ATH12K_CWM_WIDTH160,  13,  0,  19600000},
    {ATH12K_CWM_WIDTH160,  14,  0,    245000},
    {ATH12K_CWM_WIDTH160,  15,  0,    490000},
    {ATH12K_CWM_WIDTH320,   0,  0,   1960000},
    {ATH12K_CWM_WIDTH320,   1,  0,   3920000},
    {ATH12K_CWM_WIDTH320,   2,  0,   5880000},
    {ATH12K_CWM_WIDTH320,   3,  0,   7840000},
    {ATH12K_CWM_WIDTH320,   4,  0,  11760000},
    {ATH12K_CWM_WIDTH320,   5,  0,  15680000},
    {ATH12K_CWM_WIDTH320,   6,  0,  17640000},
    {ATH12K_CWM_WIDTH320,   7,  0,  19600000},
    {ATH12K_CWM_WIDTH320,   8,  0,  23520000},
    {ATH12K_CWM_WIDTH320,   9,  0,  26133000},
    {ATH12K_CWM_WIDTH320,  10,  0,  29400000},
    {ATH12K_CWM_WIDTH320,  11,  0,  32666000},
    {ATH12K_CWM_WIDTH320,  12,  0,  35280000},
    {ATH12K_CWM_WIDTH320,  13,  0,  39200000},
    {ATH12K_CWM_WIDTH320,  14,  0,    490000},
    {ATH12K_CWM_WIDTH320,  15,  0,    980000},
};

/**
 * ath12k_get_vht_ht_ndbps - Look up NDBPS for a VHT or HT MCS
 * @bw: channel width (ATH12K_CWM_WIDTH*)
 * @mcs: MCS index
 *
 * Searches ath12k_vht_ht_rate_tbl for a matching (bw, mcs) entry.
 * VHT and HT share the same table because their subcarrier counts and
 * coding rates follow the same OFDM symbol structure.
 *
 * Returns: NDBPS value (×1000), or 0 if not found.
 */
static u32 ath12k_get_vht_ht_ndbps(enum ath12k_cwm_width bw, u8 mcs)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(ath12k_vht_ht_rate_tbl); i++)
		if (ath12k_vht_ht_rate_tbl[i][0] == bw &&
		    ath12k_vht_ht_rate_tbl[i][1] == mcs)
			return ath12k_vht_ht_rate_tbl[i][2];
	return 0;
}

/**
 * ath12k_get_he_ndbps - Look up NDBPS for an HE (802.11ax) MCS
 * @bw: channel width (ATH12K_CWM_WIDTH*)
 * @mcs: MCS index (0–13; standard ceiling is 11, 12/13 for extended range)
 *
 * Searches ath12k_he_rate_tbl for a matching (bw, mcs) entry.
 *
 * Returns: NDBPS value (×1000), or 0 if not found.
 */
static u32 ath12k_get_he_ndbps(enum ath12k_cwm_width bw, u8 mcs)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(ath12k_he_rate_tbl); i++)
		if (ath12k_he_rate_tbl[i][0] == bw &&
		    ath12k_he_rate_tbl[i][1] == mcs)
			return ath12k_he_rate_tbl[i][2];
	return 0;
}

/**
 * ath12k_get_eht_ndbps - Look up NDBPS for an EHT (802.11be) MCS
 * @bw: channel width (ATH12K_CWM_WIDTH*)
 * @mcs: MCS index (0–13; MCS14/15 are EHT-Dup modes with lower NDBPS)
 *
 * Searches ath12k_eht_rate_tbl for a matching (bw, mcs, punc=0) entry.
 * Only non-punctured rows are queried here; punctured-EHT rates are served
 * from the ath12k_max_rates[58–63] table rows instead.
 *
 * Returns: NDBPS value (×1000), or 0 if not found.
 */
static u32 ath12k_get_eht_ndbps(enum ath12k_cwm_width bw, u8 mcs)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(ath12k_eht_rate_tbl); i++)
		if (ath12k_eht_rate_tbl[i][0] == bw &&
		    ath12k_eht_rate_tbl[i][1] == mcs &&
		    ath12k_eht_rate_tbl[i][2] == 0)
			return ath12k_eht_rate_tbl[i][3];
	return 0;
}

/**
 * ath12k_vendor_maxrate_eht_formula - Compute max EHT (802.11be) PHY rate
 * @band: operating band (nl80211_band)
 * @cwm_bw: channel width (ATH12K_CWM_WIDTH*)
 * @nss: spatial streams, already adjusted for BW constraints
 * @mask: bitrate constraints from cfg80211
 * @value: output — computed rate in Kbps
 *
 * Determines the highest allowed EHT MCS from mask->control[@band].eht_mcs[0],
 * caps at MCS13 (MCS14/15 are EHT-Dup modes with lower NDBPS), looks up the
 * corresponding NDBPS value, and applies the OFDM symbol duration formula for
 * the configured guard interval.  Falls back to narrower BW table rows when no
 * entry exists at the operating width (e.g. 320 MHz falls back to 160/80 MHz).
 *
 * Returns: 0 on success, -ENODATA if no NDBPS entry is found.
 */
static int ath12k_vendor_maxrate_eht_formula(enum nl80211_band band,
					     enum ath12k_cwm_width cwm_bw,
					     u8 nss,
					     const struct cfg80211_bitrate_mask *mask,
					     u64 *value)
{
	if (band >= NUM_NL80211_BANDS)
		return -EINVAL;

	u16 eht_mask = mask->control[band].eht_mcs[0];
	int mcs = (eht_mask == 0xFFFF) ? 13 : __fls(eht_mask);
	u32 nbps;
	u64 rate_kbps;

	/* MCS14/15 are EHT-Dup modes with very low NDBPS; MCS13 is the true ceiling */
	mcs = min(mcs, 13);

	nbps = ath12k_get_eht_ndbps(cwm_bw, mcs);
	if (!nbps && cwm_bw == ATH12K_CWM_WIDTH320)
		nbps = ath12k_get_eht_ndbps(ATH12K_CWM_WIDTH160, mcs);
	if (!nbps)
		nbps = ath12k_get_eht_ndbps(ATH12K_CWM_WIDTH80, mcs);
	if (!nbps)
		return -ENODATA;

	switch (mask->control[band].eht_gi) {
	case NL80211_RATE_INFO_EHT_GI_1_6:
		rate_kbps = (u32)(nbps * nss * 5) / 72; break;
	case NL80211_RATE_INFO_EHT_GI_3_2:
		rate_kbps = (u32)(nbps * nss) / 16; break;
	default: /* NL80211_RATE_INFO_EHT_GI_0_8 (smallest GI, highest rate) */
		rate_kbps = (u32)(nbps * nss * 5) / 68; break;
	}
	*value = rate_kbps;
	return 0;
}

/**
 * ath12k_vendor_maxrate_he_formula - Compute max HE (802.11ax) PHY rate
 * @band: operating band (nl80211_band)
 * @cwm_bw: channel width (ATH12K_CWM_WIDTH*)
 * @nss: spatial streams, already adjusted for BW constraints
 * @mask: bitrate constraints from cfg80211
 * @value: output — computed rate in Kbps
 *
 * Determines the highest allowed HE MCS from mask->control[@band].he_mcs[0],
 * caps at MCS13 for extended-range capable hardware (standard ceiling is
 * MCS11), and applies the OFDM symbol duration formula for the configured
 * guard interval.  Falls back to narrower BW rows when no table entry exists
 * at the operating width.
 *
 * Returns: 0 on success, -ENODATA if no NDBPS entry is found.
 */
static int ath12k_vendor_maxrate_he_formula(enum nl80211_band band,
					    enum ath12k_cwm_width cwm_bw,
					    u8 nss,
					    const struct cfg80211_bitrate_mask *mask,
					    u64 *value)
{
	if (band >= NUM_NL80211_BANDS)
		return -EINVAL;

	u16 he_mask = mask->control[band].he_mcs[0];
	int mcs = (he_mask == 0xFFFF) ? 11 : __fls(he_mask);
	u32 nbps;
	u64 rate_kbps;

	/* he_tbl has MCS12/13 entries for extended-range capability */
	mcs = min(mcs, 13);

	nbps = ath12k_get_he_ndbps(cwm_bw, mcs);
	if (!nbps && cwm_bw > ATH12K_CWM_WIDTH80)
		nbps = ath12k_get_he_ndbps(ATH12K_CWM_WIDTH80, mcs);
	if (!nbps)
		nbps = ath12k_get_he_ndbps(ATH12K_CWM_WIDTH40, mcs);
	if (!nbps)
		nbps = ath12k_get_he_ndbps(ATH12K_CWM_WIDTH20, mcs);
	if (!nbps)
		return -ENODATA;

	switch (mask->control[band].he_gi) {
	case NL80211_RATE_INFO_HE_GI_1_6:
		rate_kbps = (u32)(nbps * nss * 5) / 72; break;
	case NL80211_RATE_INFO_HE_GI_3_2:
		rate_kbps = (u32)(nbps * nss) / 16; break;
	default: /* NL80211_RATE_INFO_HE_GI_0_8 */
		rate_kbps = (u32)(nbps * nss * 5) / 68; break;
	}
	*value = rate_kbps;
	return 0;
}

/**
 * ath12k_vendor_maxrate_vht_formula - Compute max VHT (802.11ac) PHY rate
 * @band: operating band (nl80211_band)
 * @cwm_bw: channel width (ATH12K_CWM_WIDTH*)
 * @nss: spatial streams, already adjusted for BW constraints
 * @mask: bitrate constraints from cfg80211
 * @value: output — computed rate in Kbps
 *
 * Determines the highest allowed VHT MCS from mask->control[@band].vht_mcs[0],
 * caps at MCS11 (1024-QAM), and applies SGI or LGI formula.  An unconstrained
 * mask defaults to SGI to report the theoretical maximum.
 *
 * Returns: 0 on success, -ENODATA if no NDBPS entry is found.
 */
static int ath12k_vendor_maxrate_vht_formula(enum nl80211_band band,
					     enum ath12k_cwm_width cwm_bw,
					     u8 nss,
					     const struct cfg80211_bitrate_mask *mask,
					     u64 *value)
{
	u16 vht_mask;
	int mcs;
	u32 nbps;

	if (!mask || !value || band >= NUM_NL80211_BANDS)
		return -EINVAL;

	vht_mask = mask->control[band].vht_mcs[0];
	mcs = (vht_mask == 0xFFFF) ? 9 : __fls(vht_mask);

	mcs = min(mcs, 11);

	nbps = ath12k_get_vht_ht_ndbps(cwm_bw, mcs);
	if (!nbps && cwm_bw > ATH12K_CWM_WIDTH80)
		nbps = ath12k_get_vht_ht_ndbps(ATH12K_CWM_WIDTH80, mcs);
	if (!nbps)
		nbps = ath12k_get_vht_ht_ndbps(ATH12K_CWM_WIDTH40, mcs);
	if (!nbps)
		return -ENODATA;

	/* Auto-rate → SGI for true max; fixed → respect configured GI */
	if (vht_mask == 0xFFFF ||
	    mask->control[band].gi == NL80211_TXRATE_FORCE_SGI)
		*value = (u32)(nbps * nss * 5) / 18;  /* SGI */
	else
		*value = (u32)(nbps * nss) / 4;        /* LGI */
	return 0;
}

/**
 * ath12k_vendor_maxrate_ht_formula - Compute max HT (802.11n) PHY rate
 * @band: operating band (nl80211_band)
 * @cwm_bw: channel width (ATH12K_CWM_WIDTH*)
 * @max_nss: hardware TX chain count; used as the starting NSS tier to probe
 * @mask: bitrate constraints from cfg80211
 * @value: output — computed rate in Kbps
 *
 * HT MCS are organised per-NSS tier: ht_mcs[N] covers MCS for NSS=(N+1).
 * Walks from max_nss downward to find the highest tier that has any MCS bits
 * set, then applies the SGI or LGI formula for that tier's best MCS.
 *
 * Returns: 0 on success, -ENODATA if no active NSS tier or NDBPS entry found.
 */
static int ath12k_vendor_maxrate_ht_formula(enum nl80211_band band,
					    enum ath12k_cwm_width cwm_bw,
					    u8 max_nss,
					    const struct cfg80211_bitrate_mask *mask,
					    u64 *value)
{
	u8 ht_mask = 0;
	int ht_nss, mcs;
	u32 nbps;

	if (!mask || !value || band >= NUM_NL80211_BANDS)
		return -EINVAL;

	max_nss = min_t(u8, max_nss, ARRAY_SIZE(mask->control[band].ht_mcs));

	/* ht_mcs[n] covers MCS for NSS=(n+1); find highest tier with any MCS */
	for (ht_nss = max_nss; ht_nss >= 1; ht_nss--) {
		if (mask->control[band].ht_mcs[ht_nss - 1]) {
			ht_mask = mask->control[band].ht_mcs[ht_nss - 1];
			break;
		}
	}
	if (!ht_nss)
		return -ENODATA;

	mcs = (ht_mask == 0xFF) ? 7 : __fls(ht_mask);
	mcs = min(mcs, 7);

	nbps = ath12k_get_vht_ht_ndbps(cwm_bw, mcs);
	if (!nbps)
		nbps = ath12k_get_vht_ht_ndbps(ATH12K_CWM_WIDTH20, mcs);
	if (!nbps)
		return -ENODATA;

	if (ht_mask == 0xFF ||
	    mask->control[band].gi == NL80211_TXRATE_FORCE_SGI)
		*value = (u32)(nbps * ht_nss * 5) / 18;  /* SGI */
	else
		*value = (u32)(nbps * ht_nss) / 4;        /* LGI */
	return 0;
}

/**
 * ath12k_vendor_get_maxrate_vap_formula - Compute max PHY rate via NDBPS formula
 * @arvif: pointer to the per-link VAP structure
 * @value: pointer to store the computed rate in Kbps
 *
 * Entry point for the formula-based max-rate path, used when the bitrate_mask
 * is constrained to a specific MCS set.  Resolves channel, BW, and NSS from
 * @arvif, then dispatches to the per-standard helper in priority order:
 * EHT → HE → VHT → HT.
 *
 * Uses bss_conf operational flags (eht_support, he_support) for standard
 * selection rather than raw hardware capability flags.  On EHT-capable chips
 * all radios report has_eht=1 in hardware regardless of operating mode, so
 * bss_conf is the authoritative source for what the AP is actually advertising.
 *
 * Returns: 0 on success, negative error code on failure.
 */
static int ath12k_vendor_get_maxrate_vap_formula(struct ath12k_link_vif *arvif,
						 u64 *value)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_vif *vif;
	struct ieee80211_bss_conf *bss_conf;
	struct cfg80211_chan_def def;
	const struct cfg80211_bitrate_mask *mask = &arvif->bitrate_mask;
	enum ath12k_cwm_width cwm_bw;
	enum nl80211_band band;
	u8 nss;

	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	if (!vif)
		return -EINVAL;

	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		return -ENOTCONN;
	if (!def.chan || def.chan->band >= NUM_NL80211_BANDS)
		return -EINVAL;

	band   = def.chan->band;
	cwm_bw = ath12k_nl80211_width_to_cwm(def.width);

	/* Base NSS from hardware TX chains; reduce for wider BW if hardware
	 * supports fewer streams at 160/320 MHz (nss_ratio_info constraint) */
	nss = ar->num_tx_chains;
	if (!nss)
		return -EINVAL;
	if (cwm_bw == ATH12K_CWM_WIDTH160)
		nss = ath12k_get_nss_160mhz(ar, nss);
	else if (cwm_bw == ATH12K_CWM_WIDTH320)
		nss = ath12k_get_nss_320mhz(ar, nss);
	if (!nss)
		nss = 1;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (bss_conf && bss_conf->eht_support)
		return ath12k_vendor_maxrate_eht_formula(band, cwm_bw, nss, mask, value);
	if (bss_conf && bss_conf->he_support)
		return ath12k_vendor_maxrate_he_formula(band, cwm_bw, nss, mask, value);
	if (ar->mac.sbands[band].vht_cap.vht_supported)
		return ath12k_vendor_maxrate_vht_formula(band, cwm_bw, nss, mask, value);
	if (ar->mac.sbands[band].ht_cap.ht_supported)
		return ath12k_vendor_maxrate_ht_formula(band, cwm_bw, nss, mask, value);

	return -EOPNOTSUPP;
}

/**
 * ath12k_vendor_get_maxrate_vap - Get maximum PHY rate for a VAP
 * @arvif: pointer to the per-link VAP structure
 * @value: pointer to store the maximum rate in Kbps
 *
 * Dispatches to ath12k_maxrate_for_vif() when the bitrate_mask is unconstrained
 * (all MCS allowed), or to ath12k_vendor_get_maxrate_vap_formula() when a
 * specific MCS range is configured.
 *
 * Returns: 0 on success, negative error code on failure.
 */
static int ath12k_vendor_get_maxrate_vap(struct ath12k_link_vif *arvif,
					  u64 *value)
{
	struct cfg80211_bitrate_mask *mask = &arvif->bitrate_mask;
	struct cfg80211_chan_def def;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	enum nl80211_band band;

	if (!vif || ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		return -ENOTCONN;
	if (!def.chan || def.chan->band >= NUM_NL80211_BANDS)
		return -EINVAL;

	band = def.chan->band;

	/* All MCS allowed across every NSS tier: use max_rates table.
	 * Checking only mcs[0] misses per-NSS constraints on higher streams. */
	bool all_open = true;
	int i;

	for (i = 0; i < NL80211_HE_NSS_MAX && all_open; i++)
		if (mask->control[band].he_mcs[i] != 0xFFFF)
			all_open = false;
	for (i = 0; i < NL80211_EHT_NSS_MAX && all_open; i++)
		if (mask->control[band].eht_mcs[i] != 0xFFFF)
			all_open = false;
	for (i = 0; i < NL80211_VHT_NSS_MAX && all_open; i++)
		if (mask->control[band].vht_mcs[i] != 0xFFFF)
			all_open = false;
	for (i = 0; i < IEEE80211_HT_MCS_MASK_LEN && all_open; i++)
		if (mask->control[band].ht_mcs[i] != 0xFF)
			all_open = false;

	if (all_open)
		return ath12k_maxrate_for_vif(arvif, value);

	/* Specific MCS set: use NDBPS formula */
	return ath12k_vendor_get_maxrate_vap_formula(arvif, value);
}

/* Get link-vif level parameters */
static int ath12k_vendor_get_arvif_params(struct ath12k_link_vif *arvif,
                                          u32 param, u64 *value)
{
	struct ath12k *ar = arvif->ar;
        int ret = -1;

	switch (param) {
       case PARAM_RADIO_TXCHAINSOFT:
                *value = ar->cfg_tx_chainmask;
                ret = 0;
                break;
        case ACFG_PARAM_RADIO_RXCHAINMASK:
                *value = ar->cfg_rx_chainmask;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_TEST:
		*value = 0;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_DYN_BW_RTS:
		*value = arvif->vap_cfg.dyn_bw_rts;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CWM_ENABLE:
                *value = arvif->vap_cfg.cwm_enable;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RATE_DROPDOWN:
                *value = arvif->vap_cfg.rate_dropdown;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CTSPROT_DTIM_BCN:
                *value = arvif->vap_cfg.cts_dtim_bcn;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_MCAST_RC_STALE_PERIOD:
                *value = arvif->vap_cfg.mcast_rc_stale_period;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_ENABLE_MCAST_RC:
                *value = arvif->vap_cfg.mcast_rc;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RC_NUM_RETRIES:
                *value = arvif->vap_cfg.rc_num_retries;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_DISABLE_CABQ:
                *value = arvif->vap_cfg.disable_cabq;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_SOUNDING_MODE:
                *value = arvif->vap_cfg.he_snd_mode;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_MAX_MTU_SIZE:
                *value = arvif->vap_cfg.max_mtu_size;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_GET_MINTXPOWER:
		ret = ath12k_vendor_get_mintxpower(ar, value);
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_GET_MAXTXPOWER:
		ret = ath12k_vendor_get_maxtxpower(ar, value);
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_REGTXPOWER:
		ret = ath12k_vendor_get_regtxpower(arvif, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VDEV_TSF:
		if (!ar) {
			ath12k_err(NULL, "No radio, use link ID \n");
			return -EINVAL;
		}
		reinit_completion(&ar->tsf_report_done);
		ret = ath12k_wmi_vdev_tsf_tstamp_action_cmd(ar, arvif->vdev_id);
		if (ret) {
			ath12k_err(ar->ab,
					"Failed to send WMI_VDEV_TSF_TSTAMP_ACTION_CMDID to firmware: %d\n",
					ret);
			break;
		}
		ret = wait_for_completion_timeout(&ar->tsf_report_done,
                                                  msecs_to_jiffies(1000));
		if (!ret) {
			ath12k_err(ar->ab, "TSF report event timeout\n");
			reinit_completion(&ar->tsf_report_done);
			ret = -ETIMEDOUT;
			break;
                }
                *value = ar->tsf_report.tsf;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_BCN_TX_POWER:
		*value = arvif->vap_cfg.bcn_tx_power;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MIN_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_RSSI_MIN, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MAX_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_RSSI_MAX, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MIN_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_ACKRSSI_MIN, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MAX_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_ACKRSSI_MAX, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MIN_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_TXRATE_MIN, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MAX_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_TXRATE_MAX, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MIN_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_RXRATE_MIN, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MAX_THRESH:
		ret = ath12k_vendor_get_rssi_rate_threshold(THRESHOLD_RXRATE_MAX, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_RATE_BREACH_MASK:
		ret = ath12k_get_rssi_rate_breach_mask();
		if (ret == -ENOTCONN) {
			ath12k_err(NULL, "Telemetry agent not loaded\n");
		} else {
			*value = ret;
			ret = 0;
		}
		break;
        case QCA_WLAN_VENDOR_VDEV_PARAM_GET_RSSI_RATE_THRESHOLDS:
		ret = ath12k_telemetry_print_thresholds();
		if (ret)
			ath12k_err(NULL, "Failed to print RSSI/Rate thresholds\n");
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_HYSTERESIS:
		ret = ath12k_telemetry_get_hysteresis(HYSTERESIS_TYPE_RSSI, value);
		if (ret == -ENOTCONN) {
			ath12k_err(NULL, "Telemetry agent not loaded\n");
		} else if (ret == -EINVAL) {
			ath12k_err(NULL, "Failed to get RSSI hysteresis\n");
		}
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RATE_HYSTERESIS:
		ret = ath12k_telemetry_get_hysteresis(HYSTERESIS_TYPE_RATE, value);
		if (ret == -ENOTCONN) {
			ath12k_err(NULL, "Telemetry agent not loaded\n");
		} else if (ret == -EINVAL) {
			ath12k_err(NULL, "Failed to get rate hysteresis\n");
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_AMPDU:
		*value = arvif->vap_cfg.ampdu_aggr_size;
		ret = 0;
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_AMSDU:
		*value = arvif->vap_cfg.amsdu_aggr_size;
		ret = 0;
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_GET_RX_AMSDU_BITMAP:
		{
			struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
			struct ieee80211_sub_if_data *sdata;
			struct ieee80211_link_data *link;

			if (!vif) {
				ath12k_err(NULL, "get_rx_amsdu_bitmap: vif is NULL\n");
				return -EINVAL;
			}
			sdata = vif_to_sdata(vif);
			if (!sdata) {
				ath12k_err(NULL, "get_rx_amsdu_bitmap: sdata is NULL\n");
				return -EINVAL;
			}

			rcu_read_lock();
			link = ieee80211_get_rx_amsdu_link(sdata, arvif->link_id);
			if (!link) {
				rcu_read_unlock();
				ath12k_err(NULL, "get_rx_amsdu_bitmap: link %u is NULL\n",
					   arvif->link_id);
				return -EINVAL;
			}
			*value = link->rx_amsdu_tid_bitmap;
			rcu_read_unlock();

			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "get_rx_amsdu link_id %u bitmap=0x%x\n",
				   arvif->link_id, (u32)*value);
			ret = 0;
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_BA_BUFSIZE:
		*value = arvif->vap_cfg.ba_bufsize;
		ret = 0;
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_TX_ENCAP_TYPE:
		*value = arvif->ahvif->dp_vif.tx_encap_type;
		ret = 0;
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_RX_DECAP_TYPE:
		*value = arvif->ahvif->dp_vif.rx_decap_type;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_PROTECTION_MODE:
		*value = arvif->vap_cfg.protection_mode;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_SUPPORTED_BANDS:
		*value = ath12k_vendor_get_supported_band_bitmap(ar);
		ath12k_vendor_log_supported_bands(*value);
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_MCAST_RATE:
		ret = ath12k_get_vdev_mcast_rate(arvif, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_BCAST_RATE:
		ret = ath12k_get_vdev_bcast_rate(arvif, value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_GET_TXPOWER_RESOLUTION:
		ret = ath12k_vendor_get_txpower_resolution(arvif, value);
		break;
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	case QCA_WLAN_VENDOR_VDEV_MESH_MODE_HDR:
		*value = arvif->ahvif->dp_vif.dp_extn.mhdr;
		ret = 0;
	break;
	case QCA_WLAN_VENDOR_VDEV_MESH_MODE_DBG:
		*value = arvif->ahvif->dp_vif.dp_extn.mdbg;
		ret = 0;
	break;
	case QCA_WLAN_VENDOR_VDEV_RX_FILTER:
		*value = arvif->ahvif->dp_vif.dp_extn.rx_filter;
		ret = 0;
	break;
	case QCA_WLAN_VENDOR_VDEV_TX_MESH:
		*value = arvif->ahvif->dp_vif.dp_extn.mesh_tx;
		ret = 0;
	break;
#endif
	case QCA_WLAN_VENDOR_VDEV_PARAM_BW_NSS_RATE:
		*value = arvif->vap_cfg.bw_nss_rate;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_CHWIDTH:
		*value = arvif->vap_cfg.chwidth;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_NSS:
		*value = arvif->vap_cfg.nss;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_RATE:
		ret = ath12k_vendor_get_fixed_rate(arvif,
						   ATH12K_VENDOR_FIXED_RATE_LEGACY_HT,
						   value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_VHT_MCS:
		ret = ath12k_vendor_get_fixed_rate(arvif,
						   ATH12K_VENDOR_FIXED_RATE_VHT_MCS,
						   value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_HE_MCS:
		ret = ath12k_vendor_get_fixed_rate(arvif,
						   ATH12K_VENDOR_FIXED_RATE_HE_MCS,
						   value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_EHT_MCS:
		ret = ath12k_vendor_get_fixed_rate(arvif,
						   ATH12K_VENDOR_FIXED_RATE_EHT_MCS,
						   value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_UL_FIXED_RATE:
		ret = ath12k_vendor_get_ul_fixed_rate(arvif,
						      ATH12K_VENDOR_UL_FIXED_RATE_HE,
						      value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_FIXED_RATE:
		ret = ath12k_vendor_get_ul_fixed_rate(arvif,
						      ATH12K_VENDOR_UL_FIXED_RATE_EHT,
						      value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_MCS:
		ret = ath12k_vendor_get_ul_fixed_rate(arvif,
						      ATH12K_VENDOR_UL_FIXED_RATE_HE,
						      value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_MCS:
		ret = ath12k_vendor_get_ul_fixed_rate(arvif,
						      ATH12K_VENDOR_UL_FIXED_RATE_EHT,
						      value);
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_LTF:
		*value = arvif->vap_cfg.he_ul_ltf;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_LTF:
		*value = arvif->vap_cfg.he_ltf;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_AR_GI_LTF:
		*value = arvif->vap_cfg.he_ar_gi_ltf;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_AR_LDPC:
		*value = arvif->vap_cfg.he_ar_ldpc;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_RTSTHRSHLD:
		*value = arvif->vap_cfg.he_rtsthrshld;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_LDPC:
		*value = arvif->vap_cfg.eht_ul_ldpc;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_LTF:
		*value = arvif->vap_cfg.eht_ul_ltf;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_LTF:
		*value = arvif->vap_cfg.eht_ltf;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EXTRA_EHT_LTF:
		*value = arvif->vap_cfg.extra_eht_ltf;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_BA_MODE:
		*value = arvif->vap_cfg.ba_mode;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_NSS:
		*value = arvif->vap_cfg.eht_ul_nss;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_PPDU_BW:
		*value = arvif->vap_cfg.eht_ul_ppdu_bw;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_SHORTGI:
		*value = arvif->vap_cfg.eht_ul_shortgi;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_STBC:
		*value = arvif->vap_cfg.eht_ul_stbc;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_ENABLERTSCTS:
		*value = arvif->vap_cfg.enablertscts;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_DCM:
		*value = arvif->vap_cfg.he_dcm;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_EXTRANGE:
		*value = arvif->vap_cfg.he_extrange;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_LDPC:
		*value = arvif->vap_cfg.he_ul_ldpc;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_NSS:
		*value = arvif->vap_cfg.he_ul_nss;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_PPDU_BW:
		*value = arvif->vap_cfg.he_ul_ppdu_bw;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_SHORTGI:
		*value = arvif->vap_cfg.he_ul_shortgi;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_STBC:
		*value = arvif->vap_cfg.he_ul_stbc;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_RTSCTS_RATE:
		*value = arvif->vap_cfg.rtscts_rate;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT_SGIMASK:
		*value = arvif->vap_cfg.vht_sgimask;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT80_RATE:
		*value = arvif->vap_cfg.vht80_rate;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_DIS_LPI_ANT_OPTIMIZE:
		*value = arvif->vap_cfg.dis_lpi_ant_optimize;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_LIST_CHAN:
		ret = ath12k_vendor_display_band_chans(ar);
		*value = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_GET_MAXRATE:
		ret = ath12k_vendor_get_maxrate_vap(arvif, value);
		break;
	/* Per-VAP EHT OFDMA/TXBF per-link override read-back.
	 * If a per-VAP override has been configured, return that value.
	 * Otherwise fall back to the radio-level setting and log accordingly.
	 */
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_DL_OFDMA_TXBF:
		if (arvif->vap_cfg.eht_dl_ofdma_txbf_configured) {
			*value = arvif->vap_cfg.eht_dl_ofdma_txbf;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "EHT_DL_OFDMA_TXBF: no per-VAP override, return radio-level \n");
			*value = ar->eht_dlbf_enabled;
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_DL_OFDMA:
		if (arvif->vap_cfg.eht_dl_ofdma_configured) {
			*value = arvif->vap_cfg.eht_dl_ofdma;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "EHT_DL_OFDMA: no per-VAP override, return radio-level \n");
			*value = ar->eht_dl_enabled;
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_OFDMA:
		if (arvif->vap_cfg.eht_ul_ofdma_configured) {
			*value = arvif->vap_cfg.eht_ul_ofdma;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "EHT_UL_OFDMA: no per-VAP override, return radio-level \n");
			*value = ar->eht_ul_enabled;
		}
		ret = 0;
		break;
	/* Per-VAP HE OFDMA/TXBF per-link override read-back */
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_DL_OFDMA_TXBF:
		if (arvif->vap_cfg.he_dl_ofdma_txbf_configured) {
			*value = arvif->vap_cfg.he_dl_ofdma_txbf;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "HE_DL_OFDMA_TXBF: no per-VAP override, return radio-level \n");
			*value = ar->he_dlbf_enabled;
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_DL_OFDMA:
		if (arvif->vap_cfg.he_dl_ofdma_configured) {
			*value = arvif->vap_cfg.he_dl_ofdma;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "HE_DL_OFDMA: no per-VAP override, return radio-level \n");
			*value = ar->he_dl_enabled;
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_OFDMA:
		if (arvif->vap_cfg.he_ul_ofdma_configured) {
			*value = arvif->vap_cfg.he_ul_ofdma;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "HE_UL_OFDMA: no per-VAP override, return radio-level \n");
			*value = ar->he_ul_enabled;
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_TLV_LOGGER_MODE:
		struct ath12k_pdev_dp *dp_pdev;
		struct ath12k_pdev_mon_dp_extn *mon_extn = NULL;
		bool rx_en, tx_en;
		if (arvif->ahvif->vif->type != NL80211_IFTYPE_MONITOR) {
			ath12k_err(NULL, "requested interface is not a monitor interface!\n");
			return -EINVAL;
		}
		dp_pdev = &ar->dp;
		*value = ATH12K_TLV_LOGGER_DISABLED;

		if (dp_pdev->dp_mon_pdev)
			mon_extn = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;
		if (mon_extn) {
			rx_en = mon_extn->rx_tlv_log &&
				mon_extn->rx_tlv_log->tlv_logging_enable;
			tx_en = mon_extn->tx_tlv_log &&
				mon_extn->tx_tlv_log->tlv_logging_enable;
			if (rx_en)
				*value |= ATH12K_TLV_LOGGER_RX_ENABLED;
			if (tx_en)
				*value |= ATH12K_TLV_LOGGER_TX_ENABLED;
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_GET_MONITOR_VERSION:
		*value = ath12k_get_monitor_version(ar->ab);
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT_MCS_10_11_SUPP:
		*value = arvif->arvif_extn.vht_mcs10_11_supp ? 1 : 0;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_VHT_MCS_10_11_NQ2Q_PEER_SUPP:
		*value = arvif->arvif_extn.vht_mcs10_11_nq2q_peer_supp ? 1 : 0;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_VDEV_PARAM_UNDECODED_METADATA_CAPTURE: {
		struct ath12k_pdev_dp *dp_pdev;
		struct ath12k_pdev_mon_dp_extn *mon_extn = NULL;

		if (!ath12k_scan_radio_supported(ar->pdev))
			return -EINVAL;
		dp_pdev = &ar->dp;
		if (dp_pdev->dp_mon_pdev)
			mon_extn = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;
		*value = (mon_extn && mon_extn->undecoded_meta) ? mon_extn->undecoded_meta->undecoded_metadata_capture : 0;
		ret = 0;
		break;
	}
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Un-supported param: %d\n", param);
		break;
	}

	return ret;
}

static inline int get_obss_pd_enable_bit(uint32_t threshold,
					 enum srtype type,
					 uint8_t *value)
{
	switch (type) {
	case SR_TYPE_NON_SRG_OBSS_PD:
		*value = GET_SELF_OBSS_PD_ENABLE(NON_SRG, threshold);
	break;

	case SR_TYPE_SRG_OBSS_PD:
		*value = GET_SELF_OBSS_PD_ENABLE(SRG, threshold);
	break;

	default:
		ath12k_err(NULL, "Invalid Spatial Reuse type");
		return -1;
	}

	return 0;
}

static inline int get_obss_pd_threshold(uint32_t threshold,
					enum srtype type,
					uint8_t *value)
{
	switch (type) {
	case SR_TYPE_NON_SRG_OBSS_PD:
		*value = GET_SELF_OBSS_PD_THRESH(NON_SRG, threshold);
	break;

	case SR_TYPE_SRG_OBSS_PD:
		*value = GET_SELF_OBSS_PD_THRESH(SRG, threshold);
	break;

	default:
		ath12k_err(NULL, "Invalid Spatial Reuse type");
		return -1;
	}

	return 0;
}

static inline int get_sr_per_ac(uint32_t enable_bitmap,
				enum srtype type,
				uint8_t *value)
{
	switch (type) {
	case SR_TYPE_OBSS_PD:
		*value = GET_SELF_SR_PER_AC(OBSS_PD, enable_bitmap);
	break;

	case SR_TYPE_PSR:
		*value = GET_SELF_SR_PER_AC(PSR, enable_bitmap);
	break;

	default:
		ath12k_err(NULL, "Invalid Spatial Reuse type");
	return -1;
	}

	return 0;
}

static int ath12k_vendor_get_sr_radio_params(struct ath12k *ar,
					     u32 param,
					     char value[], size_t length)
{
	uint8_t srg_val = 0, non_srg_val = 0;
	uint8_t srg_enable = 0, non_srg_enable = 0;
	uint8_t enable_per_ac_obss_pd = 0, enable_per_ac_psr = 0;

	switch (param) {
	case SR_SELF_OBSS_PD_TX_ENABLE:
		if (get_obss_pd_enable_bit(ar->sr_cfg.obss_pd_thresh,
					   SR_TYPE_NON_SRG_OBSS_PD,
					   &non_srg_enable))
			return -EINVAL;

		if (get_obss_pd_enable_bit(ar->sr_cfg.obss_pd_thresh,
					   SR_TYPE_SRG_OBSS_PD,
					   &srg_enable))
			return -EINVAL;

		snprintf(value, length, " non-srg: %d, srg: %d",
			 non_srg_enable, srg_enable);
		break;

	case SR_SELF_OBSS_PD_THRESHOLD_DB:
		if (get_obss_pd_threshold(ar->sr_cfg.obss_pd_thresh,
					  SR_TYPE_NON_SRG_OBSS_PD,
					  &non_srg_val))
			return -EINVAL;

		if (get_obss_pd_threshold(ar->sr_cfg.obss_pd_thresh,
					  SR_TYPE_SRG_OBSS_PD,
					  &srg_val))
			return -EINVAL;

		snprintf(value, length, " non-srg: %d, srg: %d",
			 (int8_t)non_srg_val, (int8_t)srg_val);
		break;

	case SR_SELF_OBSS_PD_THRESHOLD_DBM:
		if (get_obss_pd_threshold(ar->sr_cfg.obss_pd_thresh,
					  SR_TYPE_NON_SRG_OBSS_PD,
					  &non_srg_val))
			return -EINVAL;

		if (get_obss_pd_threshold(ar->sr_cfg.obss_pd_thresh,
					  SR_TYPE_SRG_OBSS_PD, &srg_val))
			return -EINVAL;

		snprintf(value, length, " non-srg: %d, srg: %d",
			 (int8_t)non_srg_val, (int8_t)srg_val);
		break;

	case SR_SELF_ENABLE_PER_AC:
		if (get_sr_per_ac(ar->sr_cfg.enable_per_ac, SR_TYPE_OBSS_PD,
				  &enable_per_ac_obss_pd))
			return -EINVAL;

		if (get_sr_per_ac(ar->sr_cfg.enable_per_ac, SR_TYPE_PSR,
				  &enable_per_ac_psr))
			return -EINVAL;

		snprintf(value, length, " obss_pd: 0x%X psr: 0x%X",
			 enable_per_ac_obss_pd, enable_per_ac_psr);
		break;

	case SR_SELF_HESIGA_SR15_ENABLE:
		snprintf(value, length, "%d", ar->sr_cfg.hesiga_sr15_enable);
		break;

	case SR_SELF_PSR_TX_ENABLE:
		snprintf(value, length, "%d", ar->sr_cfg.psr_tx_enable);
		break;

	case SR_SELF_SAFETY_MARGIN_PSR:
		snprintf(value, length, "%d", ar->sr_cfg.safety_margin_psr);
		break;

	case SR_SELF_ENABLE_TRUNCATION:
		snprintf(value, length, "%d", ar->sr_cfg.enable_sr_truncate);
		break;

	case SR_SELF_OBSS_PPDU_MIN_DUR:
		snprintf(value, length, "%u", ar->sr_cfg.min_obss_ppdu_dur_sr);
		break;

	case SR_SELF_OBSS_PD_CTRL_FRAME:
		if (get_obss_pd_enable_bit(ar->sr_cfg.obss_pd_thresh_ctrl_frame,
					   SR_TYPE_NON_SRG_OBSS_PD, &non_srg_enable))
			return -EINVAL;

		if (get_obss_pd_threshold(ar->sr_cfg.obss_pd_thresh_ctrl_frame,
					  SR_TYPE_NON_SRG_OBSS_PD, &non_srg_val))
			return -EINVAL;

		if (get_obss_pd_enable_bit(ar->sr_cfg.obss_pd_thresh_ctrl_frame,
					   SR_TYPE_SRG_OBSS_PD, &srg_enable))
			return -EINVAL;

		if (get_obss_pd_threshold(ar->sr_cfg.obss_pd_thresh_ctrl_frame,
					  SR_TYPE_SRG_OBSS_PD, &srg_val))
			return -EINVAL;

		snprintf(value, length,
			 " non-srg:{enable:%d threshold:%d},"
			 " srg:{enable:%d threshold:%d}",
			 non_srg_enable, (int8_t)non_srg_val,
			 srg_enable, (int8_t)srg_val);
		break;
	case SR_SELF_SRG_BSS_COLOR_BITMAP:
		snprintf(value, length, "0x%08X %08X",
			 ar->sr_cfg.srg_bss_color_bitmap[1],
			 ar->sr_cfg.srg_bss_color_bitmap[0]);
		break;
	 case SR_SELF_SRG_PARTIAL_BSSID_BITMAP:
		snprintf(value, length, "0x%08X %08X",
			 ar->sr_cfg.srg_partial_bssid_bitmap[1],
			 ar->sr_cfg.srg_partial_bssid_bitmap[0]);
		break;
	case SR_SELF_SRG_OBSS_COLOR_ENABLE_BITMAP:
		snprintf(value, length, "0x%08X %08X",
			 ar->sr_cfg.srg_obss_color_enable_bitmap[1],
			 ar->sr_cfg.srg_obss_color_enable_bitmap[0]);
		break;
	case SR_SELF_SRG_OBSS_BSSID_ENABLE_BITMAP:
		snprintf(value, length, "0x%08X %08X",
			 ar->sr_cfg.srg_obss_bssid_enable_bitmap[1],
			 ar->sr_cfg.srg_obss_bssid_enable_bitmap[0]);
		break;
	case SR_SELF_NON_SRG_OBSS_COLOR_ENABLE_BITMAP:
		snprintf(value, length, "0x%08X %08X",
			 ar->sr_cfg.non_srg_obss_color_enable_bitmap[1],
			 ar->sr_cfg.non_srg_obss_color_enable_bitmap[0]);
		break;
	case SR_SELF_NON_SRG_OBSS_BSSID_ENABLE_BITMAP:
		snprintf(value, length, "0x%08X %08X",
			 ar->sr_cfg.non_srg_obss_bssid_enable_bitmap[1],
			 ar->sr_cfg.non_srg_obss_bssid_enable_bitmap[0]);
		break;
	default:
		ath12k_err(NULL, "Unhandled SR config command");
		return -EINVAL;
	}
	return 0;
}

static const char *ath12k_vendor_band_to_str(enum nl80211_band band)
{
	switch (band) {
	case NL80211_BAND_2GHZ:
		return "2GHz";
	case NL80211_BAND_5GHZ:
		return "5GHz";
	case NL80211_BAND_6GHZ:
		return "6GHz";
	default:
		return "unknown";
	}
}

/**
 * ath12k_vendor_get_bw_dir - Derive side-band direction for a channel.
 * @band: NL80211 operating band for the channel.
 * @chan: Primary channel number.
 *
 * Maps a primary channel to the side-band direction marker used by
 * display-band-channels output. For supported bands, it returns:
 * 'U' for upper and 'L' for lower side placement.
 *
 * Return: 'U'/'L' for supported 5 GHz and 6 GHz channels, space for others.
 */
static char ath12k_vendor_get_bw_dir(enum nl80211_band band, u16 chan)
{
	if (band == NL80211_BAND_6GHZ) {
		if (chan <= ATH12K_VENDOR_6G_MIN_CHAN || chan == 2)
			return 'U';

		return (((chan - ATH12K_VENDOR_6G_MIN_CHAN) /
			 ATH12K_VENDOR_CHAN_STEP) % 2) ? 'L' : 'U';
	}

	if (band == NL80211_BAND_5GHZ)
		return ((chan / ATH12K_VENDOR_CHAN_STEP) % 2) ? 'U' : 'L';

	return ' ';
}

/**
 * struct ath12k_vendor_chan_center_map - Primary to center channel mapping
 * @chan: Primary 20 MHz channel number.
 * @center: Center channel index used for the requested bandwidth.
 */
struct ath12k_vendor_chan_center_map {
	u16 chan;
	u16 center;
};

/**
 * struct ath12k_vendor_chan_320_center_map - 6 GHz 320 MHz center mapping
 * @chan: Primary 20 MHz channel number.
 * @center1: First valid 320 MHz center channel index for @chan.
 * @center2: Second valid overlapping 320 MHz center index (0 if none).
 */
struct ath12k_vendor_chan_320_center_map {
	u16 chan;
	u16 center1;
	u16 center2;
};

static const struct ath12k_vendor_chan_center_map
ath12k_vendor_5g_bw80_center_map[] = {
	{  36,  42 }, {  40,  42 }, {  44,  42 }, {  48,  42 },
	{  52,  58 }, {  56,  58 }, {  60,  58 }, {  64,  58 },
	{ 100, 106 }, { 104, 106 }, { 108, 106 }, { 112, 106 },
	{ 116, 122 }, { 120, 122 }, { 124, 122 }, { 128, 122 },
	{ 132, 138 }, { 136, 138 }, { 140, 138 }, { 144, 138 },
	{ 149, 155 }, { 153, 155 }, { 157, 155 }, { 161, 155 },
	{ 165, 171 }, { 169, 171 }, { 173, 171 }, { 177, 171 },
};

static const struct ath12k_vendor_chan_center_map
ath12k_vendor_5g_bw160_center_map[] = {
	{  36,  50 }, {  40,  50 }, {  44,  50 }, {  48,  50 },
	{  52,  50 }, {  56,  50 }, {  60,  50 }, {  64,  50 },
	{ 100, 114 }, { 104, 114 }, { 108, 114 }, { 112, 114 },
	{ 116, 114 }, { 120, 114 }, { 124, 114 }, { 128, 114 },
	{ 149, 163 }, { 153, 163 }, { 157, 163 }, { 161, 163 },
	{ 165, 163 }, { 169, 163 }, { 173, 163 }, { 177, 163 },
};

static const struct ath12k_vendor_chan_center_map
ath12k_vendor_6g_bw80_center_map[] = {
	{   1,   7 }, {   5,   7 }, {   9,   7 }, {  13,   7 },
	{  17,  23 }, {  21,  23 }, {  25,  23 }, {  29,  23 },
	{  33,  39 }, {  37,  39 }, {  41,  39 }, {  45,  39 },
	{  49,  55 }, {  53,  55 }, {  57,  55 }, {  61,  55 },
	{  65,  71 }, {  69,  71 }, {  73,  71 }, {  77,  71 },
	{  81,  87 }, {  85,  87 }, {  89,  87 }, {  93,  87 },
	{  97, 103 }, { 101, 103 }, { 105, 103 }, { 109, 103 },
	{ 113, 119 }, { 117, 119 }, { 121, 119 }, { 125, 119 },
	{ 129, 135 }, { 133, 135 }, { 137, 135 }, { 141, 135 },
	{ 145, 151 }, { 149, 151 }, { 153, 151 }, { 157, 151 },
	{ 161, 167 }, { 165, 167 }, { 169, 167 }, { 173, 167 },
	{ 177, 183 }, { 181, 183 }, { 185, 183 }, { 189, 183 },
	{ 193, 199 }, { 197, 199 }, { 201, 199 }, { 205, 199 },
	{ 209, 215 }, { 213, 215 }, { 217, 215 }, { 221, 215 },
};

static const struct ath12k_vendor_chan_center_map
ath12k_vendor_6g_bw160_center_map[] = {
	{   1,  15 }, {   5,  15 }, {   9,  15 }, {  13,  15 },
	{  17,  15 }, {  21,  15 }, {  25,  15 }, {  29,  15 },
	{  33,  47 }, {  37,  47 }, {  41,  47 }, {  45,  47 },
	{  49,  47 }, {  53,  47 }, {  57,  47 }, {  61,  47 },
	{  65,  79 }, {  69,  79 }, {  73,  79 }, {  77,  79 },
	{  81,  79 }, {  85,  79 }, {  89,  79 }, {  93,  79 },
	{  97, 111 }, { 101, 111 }, { 105, 111 }, { 109, 111 },
	{ 113, 111 }, { 117, 111 }, { 121, 111 }, { 125, 111 },
	{ 129, 143 }, { 133, 143 }, { 137, 143 }, { 141, 143 },
	{ 145, 143 }, { 149, 143 }, { 153, 143 }, { 157, 143 },
	{ 161, 175 }, { 165, 175 }, { 169, 175 }, { 173, 175 },
	{ 177, 175 }, { 181, 175 }, { 185, 175 }, { 189, 175 },
	{ 193, 207 }, { 197, 207 }, { 201, 207 }, { 205, 207 },
	{ 209, 207 }, { 213, 207 }, { 217, 207 }, { 221, 207 },
};

static const struct ath12k_vendor_chan_320_center_map
ath12k_vendor_6g_bw320_center_map[] = {
	{   1,  31,   0 }, {   5,  31,   0 }, {   9,  31,   0 },
	{  13,  31,   0 }, {  17,  31,   0 }, {  21,  31,   0 },
	{  25,  31,   0 }, {  29,  31,   0 }, {  33,  31,  63 },
	{  37,  31,  63 }, {  41,  31,  63 }, {  45,  31,  63 },
	{  49,  31,  63 }, {  53,  31,  63 }, {  57,  31,  63 },
	{  61,  31,  63 }, {  65,  63,  95 }, {  69,  63,  95 },
	{  73,  63,  95 }, {  77,  63,  95 }, {  81,  63,  95 },
	{  85,  63,  95 }, {  89,  63,  95 }, {  93,  63,  95 },
	{  97,  95, 127 }, { 101,  95, 127 }, { 105,  95, 127 },
	{ 109,  95, 127 }, { 113,  95, 127 }, { 117,  95, 127 },
	{ 121,  95, 127 }, { 125,  95, 127 }, { 129, 127, 159 },
	{ 133, 127, 159 }, { 137, 127, 159 }, { 141, 127, 159 },
	{ 145, 127, 159 }, { 149, 127, 159 }, { 153, 127, 159 },
	{ 157, 127, 159 }, { 161, 159, 191 }, { 165, 159, 191 },
	{ 169, 159, 191 }, { 173, 159, 191 }, { 177, 159, 191 },
	{ 181, 159, 191 }, { 185, 159, 191 }, { 189, 159, 191 },
	{ 193, 191,   0 }, { 197, 191,   0 }, { 201, 191,   0 },
	{ 205, 191,   0 }, { 209, 191,   0 }, { 213, 191,   0 },
	{ 217, 191,   0 }, { 221, 191,   0 },
};

static u16
ath12k_vendor_center_from_table(u16 chan,
				const struct ath12k_vendor_chan_center_map *table,
				size_t num_entries)
{
	size_t i;

	for (i = 0; i < num_entries; i++) {
		if (table[i].chan == chan)
			return table[i].center;
	}

	return 0;
}

static const struct ath12k_vendor_chan_320_center_map *
ath12k_vendor_320_centers_from_table(u16 chan)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(ath12k_vendor_6g_bw320_center_map); i++) {
		if (ath12k_vendor_6g_bw320_center_map[i].chan == chan)
			return &ath12k_vendor_6g_bw320_center_map[i];
	}

	return NULL;
}

static void ath12k_vendor_5g_center_freq_idx(u16 chan, u16 bw, u16 *center)
{
	if (!center)
		return;

	*center = 0;

	if (bw == ATH12K_VENDOR_BW_80_MHZ) {
		*center = ath12k_vendor_center_from_table(
				chan,
				ath12k_vendor_5g_bw80_center_map,
				ARRAY_SIZE(ath12k_vendor_5g_bw80_center_map));
	} else if (bw == ATH12K_VENDOR_BW_160_MHZ) {
		*center = ath12k_vendor_center_from_table(
				chan,
				ath12k_vendor_5g_bw160_center_map,
				ARRAY_SIZE(ath12k_vendor_5g_bw160_center_map));
	}
}

static void ath12k_vendor_6g_center_freq_idx(u16 chan, u16 bw, u16 *center)
{
	if (!center)
		return;

	*center = 0;

	if (chan < ATH12K_VENDOR_6G_MIN_CHAN || chan > ATH12K_VENDOR_6G_MAX_CHAN)
		return;

	if (bw == ATH12K_VENDOR_BW_80_MHZ) {
		*center = ath12k_vendor_center_from_table(
				chan,
				ath12k_vendor_6g_bw80_center_map,
				ARRAY_SIZE(ath12k_vendor_6g_bw80_center_map));
	} else if (bw == ATH12K_VENDOR_BW_160_MHZ) {
		*center = ath12k_vendor_center_from_table(
				chan,
				ath12k_vendor_6g_bw160_center_map,
				ARRAY_SIZE(ath12k_vendor_6g_bw160_center_map));
	}
}

static void ath12k_vendor_6g_320_centers(u16 chan, u16 *center1, u16 *center2)
{
	const struct ath12k_vendor_chan_320_center_map *centers;

	if (!center1 || !center2)
		return;

	*center1 = 0;
	*center2 = 0;

	if (chan < ATH12K_VENDOR_6G_MIN_CHAN || chan > ATH12K_VENDOR_6G_MAX_CHAN)
		return;

	centers = ath12k_vendor_320_centers_from_table(chan);
	if (!centers)
		return;

	*center1 = centers->center1;
	*center2 = centers->center2;
}

static void ath12k_vendor_center_freq_idx(enum nl80211_band band,
							  u16 chan,
							  u16 bw,
							  u16 *center)
{
	if (band == NL80211_BAND_5GHZ)
		ath12k_vendor_5g_center_freq_idx(chan, bw, center);

	if (band == NL80211_BAND_6GHZ)
		ath12k_vendor_6g_center_freq_idx(chan, bw, center);
}

static bool ath12k_vendor_is_displayable_chan(const struct ieee80211_channel *chan,
					      u32 range_start_mhz,
					      u32 range_end_mhz)
{
	if (chan->flags & IEEE80211_CHAN_DISABLED)
		return false;

	if ((chan->center_freq < range_start_mhz ||
	     chan->center_freq > range_end_mhz))
		return false;

	return true;
}

static void ath12k_vendor_fill_chan_caps(enum nl80211_band band,
					 const struct ieee80211_channel *chan,
					 struct ath12k_vendor_chan_caps *caps)
{
	caps->chan_num = ieee80211_frequency_to_channel(chan->center_freq);
	caps->dir = ath12k_vendor_get_bw_dir(band, caps->chan_num);
	caps->has_80 = (band != NL80211_BAND_2GHZ) &&
			!(chan->flags & IEEE80211_CHAN_NO_80MHZ);
	caps->has_160 = (band != NL80211_BAND_2GHZ) &&
			 !(chan->flags & IEEE80211_CHAN_NO_160MHZ);
	caps->has_320 = (band == NL80211_BAND_6GHZ) &&
			 !(chan->flags & IEEE80211_CHAN_NO_320MHZ);
	caps->ht40_plus = !(chan->flags & IEEE80211_CHAN_NO_HT40PLUS);
	caps->ht40_minus = !(chan->flags & IEEE80211_CHAN_NO_HT40MINUS);
	caps->center80 = 0;
	caps->center160 = 0;
	caps->center320_1 = 0;
	caps->center320_2 = 0;

	ath12k_vendor_center_freq_idx(band, caps->chan_num,
				      ATH12K_VENDOR_BW_80_MHZ,
				      &caps->center80);
	ath12k_vendor_center_freq_idx(band, caps->chan_num,
				      ATH12K_VENDOR_BW_160_MHZ,
				      &caps->center160);

	if (band == NL80211_BAND_6GHZ)
		ath12k_vendor_6g_320_centers(caps->chan_num,
					     &caps->center320_1,
					     &caps->center320_2);
}

static int ath12k_vendor_append_ht40_caps(enum nl80211_band band,
					  const struct ath12k_vendor_chan_caps *caps,
					  const char *upper,
					  const char *lower,
					  char cap[],
					  int cap_len)
{
	if (band == NL80211_BAND_2GHZ) {
		if (caps->ht40_plus)
			cap_len += scnprintf(cap + cap_len,
					     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
					     " %s", upper);

		if (caps->ht40_minus)
			cap_len += scnprintf(cap + cap_len,
					     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
					     " %s", lower);
	} else if (caps->ht40_plus || caps->ht40_minus) {
		if (caps->dir == 'U')
			cap_len += scnprintf(cap + cap_len,
					     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
					     " %s", upper);

		if (caps->dir == 'L')
			cap_len += scnprintf(cap + cap_len,
					     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
					     " %s", lower);
	}

	return cap_len;
}

static int ath12k_vendor_display_pre_he_chan_caps(enum nl80211_band band,
						  const struct ath12k_vendor_chan_caps *caps,
						  char cap[])
{
	const char *mode;
	int cap_len;

	if (band == NL80211_BAND_2GHZ)
		mode = "11ng";
	else if (band == NL80211_BAND_5GHZ)
		mode = "11na";
	else
		return 0;

	cap_len = scnprintf(cap, ATH12K_VENDOR_CAP_BUF_LEN, "%s", mode);
	cap_len += scnprintf(cap + cap_len,
			     ATH12K_VENDOR_CAP_BUF_LEN - cap_len, " C");
	cap_len = ath12k_vendor_append_ht40_caps(band, caps, "CU", "CL",
						 cap, cap_len);

	if (band == NL80211_BAND_5GHZ && caps->has_80) {
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     " V V%c", caps->dir);

		if (caps->center80)
			cap_len += scnprintf(cap + cap_len,
					     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
					     " V80-%3u", caps->center80);
	}

	return cap_len;
}

static int ath12k_vendor_display_he_chan_caps(enum nl80211_band band,
					      const struct ath12k_vendor_chan_caps *caps,
					      char cap[],
					      int cap_len)
{
	cap_len += scnprintf(cap + cap_len,
			     ATH12K_VENDOR_CAP_BUF_LEN - cap_len, " H");
	cap_len = ath12k_vendor_append_ht40_caps(band, caps, "HU", "HL",
						 cap, cap_len);

	if (caps->has_80 && caps->center80)
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     " H80-%3u", caps->center80);

	if (caps->has_160 && caps->center160)
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     " H160-%3u", caps->center160);

	return cap_len;
}

static int ath12k_vendor_display_eht_chan_caps(enum nl80211_band band,
					       const struct ath12k_vendor_chan_caps *caps,
					       char cap[],
					       int cap_len)
{
	cap_len += scnprintf(cap + cap_len,
			     ATH12K_VENDOR_CAP_BUF_LEN - cap_len, " E");
	cap_len = ath12k_vendor_append_ht40_caps(band, caps, "EU", "EL",
						 cap, cap_len);

	if (caps->has_80 && caps->center80)
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     " E80-%3u", caps->center80);

	if (caps->has_160 && caps->center160)
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     " E160-%3u", caps->center160);

	return cap_len;
}

static int ath12k_vendor_display_320mhz_caps(enum nl80211_band band,
					     const struct ath12k_vendor_chan_caps *caps,
					     bool has_eht,
					     char cap[],
					     int cap_len)
{
	if (has_eht && caps->has_320 && band == NL80211_BAND_6GHZ &&
	    caps->center320_1) {
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     " E320-%3u", caps->center320_1);
		cap_len += scnprintf(cap + cap_len,
				     ATH12K_VENDOR_CAP_BUF_LEN - cap_len,
				     ", %3u", caps->center320_2);
	}

	return cap_len;
}

static int ath12k_vendor_build_bw_str(enum nl80211_band band,
				      const struct ieee80211_channel *chan,
				      char *buf, size_t buf_len)
{
	static const struct {
		u32 flag;
		u16 bw;
		u8 min_band;
	} bw_table[] = {
		{ IEEE80211_CHAN_NO_10MHZ,  10,  NL80211_BAND_2GHZ },
		{ IEEE80211_CHAN_NO_20MHZ,  20,  NL80211_BAND_2GHZ },
		{ IEEE80211_CHAN_NO_HT40,   40,  NL80211_BAND_2GHZ },
		{ IEEE80211_CHAN_NO_80MHZ,  80,  NL80211_BAND_5GHZ },
		{ IEEE80211_CHAN_NO_160MHZ, 160, NL80211_BAND_5GHZ },
		{ IEEE80211_CHAN_NO_320MHZ, 320, NL80211_BAND_6GHZ },
	};
	const char *sep = "";
	bool any = false;
	int len;
	int i;

	len = scnprintf(buf, buf_len, "BW:[");

	for (i = 0; i < ARRAY_SIZE(bw_table); i++) {
		if (band < bw_table[i].min_band)
			continue;
		if ((chan->flags & bw_table[i].flag) == bw_table[i].flag)
			continue;
		len += scnprintf(buf + len, buf_len - len,
				 "%s%u", sep, bw_table[i].bw);
		sep = ",";
		any = true;
	}

	if (!any)
		len += scnprintf(buf + len, buf_len - len, "20");

	len += scnprintf(buf + len, buf_len - len, "]");
	return len;
}

static int ath12k_vendor_build_status_str(const struct ieee80211_channel *chan,
					  const struct ieee80211_chanctx_conf *chanctx,
					  char *buf, size_t buf_len)
{
	bool is_active = false;
	bool is_primary = false;
	bool is_punctured = false;
	int len = 0;

	if (chanctx && chanctx->def.chan) {
		int bw_mhz = cfg80211_chandef_get_width(&chanctx->def);
		u32 cf1 = chanctx->def.center_freq1;
		u32 chan_freq = chan->center_freq;

		if (bw_mhz > 20) {
			u32 home_start = cf1 - (u32)bw_mhz / 2 + 10;
			u32 home_end = cf1 + (u32)bw_mhz / 2 - 10;

			if (chan_freq >= home_start && chan_freq <= home_end)
				is_active = true;
		} else {
			if (chan_freq == cf1)
				is_active = true;
		}

		is_primary = (chan_freq == chanctx->def.chan->center_freq);

		if (is_active && chanctx->def.punctured && bw_mhz >= 80) {
			u32 home_start = cf1 - (u32)bw_mhz / 2 + 10;
			int sub_idx = (int)(chan_freq - home_start) / 20;

			if (sub_idx >= 0 &&
			    (chanctx->def.punctured & BIT(sub_idx)))
				is_punctured = true;
		}
	}

	buf[0] = '\0';
	if (is_active)
		len += scnprintf(buf + len, buf_len - len, " ACTIVE");
	if (is_primary)
		len += scnprintf(buf + len, buf_len - len, " PRIMARY");
	if (is_punctured)
		len += scnprintf(buf + len, buf_len - len, " PUNCTURED");
	if (chan->flags & IEEE80211_CHAN_NO_IR)
		len += scnprintf(buf + len, buf_len - len, " NO_IR");
	if (chan->flags & IEEE80211_CHAN_RADAR) {
		if (chan->dfs_state == NL80211_DFS_UNAVAILABLE)
			len += scnprintf(buf + len, buf_len - len, " In_NOL(RADAR_DETECTED)");
		else if (chan->dfs_state == NL80211_DFS_USABLE)
			len += scnprintf(buf + len, buf_len - len, " CAC_REQUIRED");
		else if (chan->dfs_state == NL80211_DFS_AVAILABLE)
			len += scnprintf(buf + len, buf_len - len, " CAC_CLEARED");
	}

	return len;
}

static void ath12k_vendor_display_channel_caps(struct ath12k *ar,
					       struct ieee80211_supported_band *sband,
					       enum nl80211_band band,
					       bool has_he,
					       bool has_eht,
					       u32 range_start_mhz,
					       u32 range_end_mhz,
					       const struct ieee80211_chanctx_conf *chanctx)
{
	struct ath12k_vendor_chan_caps chan_caps;
	int i;

	for (i = 0; i < sband->n_channels; i++) {
		const struct ieee80211_channel *chan;
		char cap[ATH12K_VENDOR_CAP_BUF_LEN];
		char bw_str[64];
		char status_str[128];
		int cap_len;

		chan = &sband->channels[i];
		if (!ath12k_vendor_is_displayable_chan(chan, range_start_mhz,
						       range_end_mhz))
			continue;

		ath12k_vendor_build_bw_str(band, chan, bw_str, sizeof(bw_str));
		ath12k_vendor_build_status_str(chan, chanctx,
					       status_str, sizeof(status_str));

		ath12k_vendor_fill_chan_caps(band, chan, &chan_caps);
		cap_len = ath12k_vendor_display_pre_he_chan_caps(band, &chan_caps, cap);

		if (has_he)
			cap_len = ath12k_vendor_display_he_chan_caps(band, &chan_caps,
								     cap, cap_len);
		if (has_eht)
			cap_len = ath12k_vendor_display_eht_chan_caps(band, &chan_caps,
								      cap, cap_len);
		cap_len = ath12k_vendor_display_320mhz_caps(band, &chan_caps, has_eht,
							    cap, cap_len);
		ath12k_info(NULL,
			    "Channel %3u : %4u MHz  %3d dBm  %s%s  %s\n",
			    chan_caps.chan_num,
			    chan->center_freq,
			    chan->max_reg_power,
			    bw_str,
			    status_str,
			    cap);
	}
}

static int ath12k_vendor_display_band_chans(struct ath12k *ar)
{
	struct wiphy *wiphy;
	struct ieee80211_supported_band *sband;
	enum nl80211_band band;
	u32 freq;
	u32 range_start_mhz = 0;
	u32 range_end_mhz = 0;
	bool use_freq_range = false;
	bool has_he;
	bool has_eht;
	struct ieee80211_chanctx_conf *chanctx;

	if (!ar || !ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
		return -EINVAL;

	freq = ar->chan_info.low_freq;
	if (!freq) {
		ath12k_err(NULL,
			   "display_band_chans: invalid current frequency for radio %u\n",
			   ar->radio_idx);
		return -ENODATA;
	}

	band = ath12k_get_band_based_on_freq(freq);
	if (band >= NUM_NL80211_BANDS)
		return -EINVAL;

	wiphy = ar->ah->hw->wiphy;
	sband = wiphy->bands[band];
	if (!sband) {
		ath12k_err(NULL,
			   "display_band_chans: no band info for radio %u band %s\n",
			   ar->radio_idx, ath12k_vendor_band_to_str(band));
		return -ENODATA;
	}

	has_he = ieee80211_get_he_iftype_cap(sband, NL80211_IFTYPE_AP) ||
		 ieee80211_get_he_iftype_cap(sband, NL80211_IFTYPE_STATION);

	has_eht = ieee80211_get_eht_iftype_cap(sband, NL80211_IFTYPE_AP) ||
		  ieee80211_get_eht_iftype_cap(sband, NL80211_IFTYPE_STATION);

	if (ar->freq_range.start_freq && ar->freq_range.end_freq &&
	    ar->freq_range.end_freq >= ar->freq_range.start_freq) {
		range_start_mhz = KHZ_TO_MHZ(ar->freq_range.start_freq);
		range_end_mhz = KHZ_TO_MHZ(ar->freq_range.end_freq);
		use_freq_range = true;
	}

	if (!use_freq_range) {
		ath12k_err(NULL,
			   "The [start = %u and end = %u] range is not valid. Returning",
			   range_start_mhz, range_end_mhz);
		return -1;
	}

	chanctx = ath12k_mac_get_first_active_arvif_chanctx(ar);
	ath12k_vendor_display_channel_caps(ar, sband, band, has_he, has_eht,
					   range_start_mhz, range_end_mhz,
					   chanctx);

	return 0;
}

static struct ieee80211_supported_band *ath12k_get_6g_band(struct ath12k *ar)
{
	struct ieee80211_supported_band *band_6g;

	band_6g = &ar->mac.sbands[NL80211_BAND_6GHZ];
	if (!band_6g || !band_6g->channels || !band_6g->n_channels)
		return NULL;

	return band_6g;
}

static u8 ath12k_super_chan_state(u32 channel_flags)
{
	if (channel_flags & IEEE80211_CHAN_DISABLED)
		return ATH12K_SUPER_CHAN_STATE_DISABLED;

	if (channel_flags & IEEE80211_CHAN_RADAR)
		return ATH12K_SUPER_CHAN_STATE_RADAR;

	if (channel_flags & IEEE80211_CHAN_NO_IR)
		return ATH12K_SUPER_CHAN_STATE_NO_IR;

	return ATH12K_SUPER_CHAN_STATE_ENABLED;
}

static u16 ath12k_super_chan_min_bw(u32 channel_flags)
{
	if (channel_flags & IEEE80211_CHAN_NO_20MHZ)
		return (channel_flags & IEEE80211_CHAN_NO_10MHZ) ? 40 : 10;

	return 20;
}

static u16 ath12k_super_chan_max_bw(u32 channel_flags)
{
	u16 max_bw = 320;

	if (channel_flags & IEEE80211_CHAN_NO_320MHZ)
		max_bw = 160;
	if (channel_flags & IEEE80211_CHAN_NO_160MHZ)
		max_bw = 80;
	if (channel_flags & IEEE80211_CHAN_NO_80MHZ)
		max_bw = 40;
	if (channel_flags & IEEE80211_CHAN_NO_HT40)
		max_bw = 20;
	if (channel_flags & IEEE80211_CHAN_NO_20MHZ)
		max_bw = (channel_flags & IEEE80211_CHAN_NO_10MHZ) ? 40 : 10;

	return max_bw;
}

static void ath12k_super_chan_power_caps(u32 channel_flags,
					 u16 *power_types,
					 u8 *best_power_mode)
{
	*power_types = 0;
	*best_power_mode = ATH12K_SUPER_CHAN_POWER_MODE_NONE;

	if (channel_flags & IEEE80211_CHAN_DISABLED)
		return;

	*power_types |= BIT(1);
	*best_power_mode = ATH12K_SUPER_CHAN_POWER_MODE_LPI;

	if (!(channel_flags & IEEE80211_CHAN_NO_6GHZ_AFC_CLIENT)) {
		*power_types |= BIT(2);
		*best_power_mode = ATH12K_SUPER_CHAN_POWER_MODE_STANDARD;
	}

	if (!(channel_flags & IEEE80211_CHAN_NO_6GHZ_VLP_CLIENT))
		*power_types |= BIT(3);
}

static const char *ath12k_super_chan_power_mode_to_str(u8 power_mode)
{
	switch (power_mode) {
	case ATH12K_SUPER_CHAN_POWER_MODE_NONE:
		return "NONE";
	case ATH12K_SUPER_CHAN_POWER_MODE_LPI:
		return "LPI";
	case ATH12K_SUPER_CHAN_POWER_MODE_STANDARD:
		return "STANDARD";
	case ATH12K_SUPER_CHAN_POWER_MODE_VLP:
		return "VLP";
	default:
		return "UNKNOWN";
	}
}

static bool ath12k_super_chan_mode_supported(u32 channel_flags, u8 power_mode)
{
	bool mode_supported;

	mode_supported = !(channel_flags & IEEE80211_CHAN_DISABLED);

	if (power_mode == ATH12K_SUPER_CHAN_POWER_MODE_STANDARD)
		mode_supported = mode_supported &&
			!(channel_flags & IEEE80211_CHAN_NO_6GHZ_AFC_CLIENT);

	if (power_mode == ATH12K_SUPER_CHAN_POWER_MODE_VLP)
		mode_supported = mode_supported &&
			!(channel_flags & IEEE80211_CHAN_NO_6GHZ_VLP_CLIENT);

	return mode_supported;
}

static void ath12k_super_chan_dump(struct ath12k *ar,
				   const struct ieee80211_channel *channel,
				   u16 power_types,
				   u8 best_power_mode)
{
	u8 power_mode;
	const struct ieee80211_supported_band *sband_6g = NULL;
	struct wiphy *wiphy = NULL;

	ath12k_info(ar->ab,
		    "Freq = %d\tPower types = 0x%x\tBest power mode = %s\n",
		    channel->center_freq, power_types,
		    ath12k_super_chan_power_mode_to_str(best_power_mode));

	if (ar && ar->ah && ar->ah->hw)
		wiphy = ar->ah->hw->wiphy;

	if (wiphy)
		sband_6g = wiphy->bands[NL80211_BAND_6GHZ];

	for (power_mode = ATH12K_SUPER_CHAN_POWER_MODE_LPI;
	     power_mode <= ATH12K_SUPER_CHAN_POWER_MODE_VLP;
	     power_mode++) {
		const struct ieee80211_channel *mode_chan = NULL;
		enum nl80211_regulatory_power_modes nl_mode = NL80211_REG_NUM_POWER_MODES;
		u32 mode_flags = IEEE80211_CHAN_DISABLED;
		u8 mode_state = ATH12K_SUPER_CHAN_STATE_DISABLED;
		u16 mode_min_bw = 0;
		u16 mode_max_bw = 0;
		bool mode_psd_flag = false;
		bool mode_afc_not_done = false;
		int mode_psd_power = 0;
		int mode_eirp_power = 0;
		bool mode_supported;

		switch (power_mode) {
		case ATH12K_SUPER_CHAN_POWER_MODE_LPI:
			nl_mode = NL80211_REG_AP_LPI;
			break;
		case ATH12K_SUPER_CHAN_POWER_MODE_STANDARD:
			nl_mode = NL80211_REG_AP_SP;
			break;
		case ATH12K_SUPER_CHAN_POWER_MODE_VLP:
			nl_mode = NL80211_REG_AP_VLP;
			break;
		default:
			break;
		}

		if (sband_6g &&
		    nl_mode < NL80211_REG_NUM_POWER_MODES &&
		    sband_6g->chan_6g[nl_mode]) {
			mode_chan = ieee80211_get_6g_channel_khz(wiphy,
								 ieee80211_channel_to_khz(channel),
								 nl_mode);
		}

		if (mode_chan) {
			mode_flags = mode_chan->flags;
			mode_state = ath12k_super_chan_state(mode_flags);
			mode_min_bw = ath12k_super_chan_min_bw(mode_flags);
			mode_max_bw = ath12k_super_chan_max_bw(mode_flags);
			mode_psd_flag = !!(mode_flags & IEEE80211_CHAN_PSD);
			mode_psd_power = mode_psd_flag ? mode_chan->psd : 0;
			mode_eirp_power = mode_chan->max_reg_power;
			mode_afc_not_done = !!(mode_flags & IEEE80211_CHAN_NO_6GHZ_AFC_CLIENT);
		}

		mode_supported = ath12k_super_chan_mode_supported(mode_flags,
								   power_mode);

		ath12k_info(ar->ab,
		   "Power mode = %s\tPSD flag = %d\tPSD power = %d\tEIRP power = %d\tChan flags = 0x%x\tChannel state = %d\tMin bw = %d\tMax bw = %d\tAFC_NOT_DONE = %d\n",
		   ath12k_super_chan_power_mode_to_str(power_mode),
		   mode_supported ? mode_psd_flag : 0,
		   mode_supported ? mode_psd_power : 0,
		   mode_supported ? mode_eirp_power : 0,
		   mode_supported ? mode_flags : IEEE80211_CHAN_DISABLED,
		   mode_supported ? mode_state : ATH12K_SUPER_CHAN_STATE_DISABLED,
		   mode_supported ? mode_min_bw : 0,
		   mode_supported ? mode_max_bw : 0,
		   (power_mode == ATH12K_SUPER_CHAN_POWER_MODE_STANDARD) ?
		   mode_afc_not_done : 0);
	}
}

static u32 ath12k_get_cac_half_bw(enum nl80211_chan_width width)
{
	switch (width) {
	case NL80211_CHAN_WIDTH_40:
		return 20;
	case NL80211_CHAN_WIDTH_80:
	case NL80211_CHAN_WIDTH_80P80:
		return 40;
	case NL80211_CHAN_WIDTH_160:
		return 80;
	default:
		return 10;
	}
}

/**
 * ath12k_get_rem_cac_sec_for_chan() - Compute remaining CAC time for a channel
 * @ar: Pointer to ath12k radio instance
 * @chan: Pointer to the ieee80211_channel being queried
 *
 * Returns remaining CAC time in seconds when CAC is in progress, or -1 when
 * no CAC is running on this channel.
 */
static s32
ath12k_get_rem_cac_sec_for_chan(struct ath12k *ar, struct ieee80211_channel *chan)
{
	struct wireless_dev *wdev;
	struct cfg80211_chan_def *chandef;
	unsigned long cac_start_time;
	u32 half_bw, elapsed_ms;
	int link_id;

	list_for_each_entry(wdev, &ath12k_ar_to_hw(ar)->wiphy->wdev_list, list) {
		if (wdev->iftype != NL80211_IFTYPE_AP)
			continue;

		for (link_id = 0; link_id < ARRAY_SIZE(wdev->links); link_id++) {
			if (!wdev->links[link_id].cac_started)
				continue;

			chandef = wdev_chandef(wdev, link_id);
			if (!chandef || !chandef->chan)
				continue;

			half_bw = ath12k_get_cac_half_bw(chandef->width);

			if (chan->center_freq < chandef->center_freq1 - half_bw + 10 ||
			    chan->center_freq > chandef->center_freq1 + half_bw - 10)
				continue;

			cac_start_time = wdev->links[link_id].cac_start_time;
			elapsed_ms = jiffies_to_msecs(jiffies - cac_start_time);

			if (wdev->links[link_id].cac_time_ms <= elapsed_ms)
				return 0;

			return (s32)((wdev->links[link_id].cac_time_ms -
					       elapsed_ms) / ATH12K_MSEC_PER_SEC);
		}
	}

	return -1;
}

/**
 * ath12k_get_dfs_chan_info() - Fill DFS state string and timing for a channel
 * @ar: Pointer to ath12k radio instance
 * @chan: Pointer to the ieee80211_channel being queried
 * @dfs_state_str: Output human-readable DFS state string
 * @rem_cac_sec: Output remaining CAC time in seconds
 * @cac_comp_time: Output CAC completion timestamp in 0.1 ms units since boot
 * @rem_nop_sec: Output remaining NOP time in seconds
 */
static void ath12k_get_dfs_chan_info(struct ath12k *ar,
				     struct ieee80211_channel *chan,
				     const char **dfs_state_str,
				     s32 *rem_cac_sec,
				     u64 *cac_comp_time,
				     s32 *rem_nop_sec)
{
	u32 elapsed_ms;

	*rem_cac_sec = -1;
	*cac_comp_time = 0;
	*rem_nop_sec = -1;

	if (!(chan->flags & IEEE80211_CHAN_RADAR)) {
		*dfs_state_str = "NON_DFS_CHANNEL";
		return;
	}

	switch (chan->dfs_state) {
	case NL80211_DFS_USABLE:
		*rem_cac_sec = ath12k_get_rem_cac_sec_for_chan(ar, chan);
		*dfs_state_str = *rem_cac_sec >= 0 ?
				 "DFS_CAC_STARTED" : "DFS_CAC_REQUIRED";
		break;
	case NL80211_DFS_AVAILABLE:
		*dfs_state_str = "DFS_CAC_COMPLETED";
		*cac_comp_time = (u64)jiffies_to_msecs(chan->dfs_state_entered -
							  INITIAL_JIFFIES);
		break;
	case NL80211_DFS_UNAVAILABLE:
		*dfs_state_str = "DFS_NOL";
		if (!chan->dfs_state_entered)
			break;

		elapsed_ms = jiffies_to_msecs(jiffies - chan->dfs_state_entered);
		if (elapsed_ms < IEEE80211_DFS_MIN_NOP_TIME_MS)
			*rem_nop_sec = (s32)((IEEE80211_DFS_MIN_NOP_TIME_MS -
					       elapsed_ms) / ATH12K_MSEC_PER_SEC);
		else
			*rem_nop_sec = 0;
		break;
	default:
		*dfs_state_str = "DFS_INVALID";
		break;
	}
}

/* Returns true if chan falls within ar's configured frequency range */
static inline bool ath12k_is_chan_in_ar(struct ath12k *ar,
					struct ieee80211_channel *chan)
{
	if (!ar->chan_info.low_freq || !ar->chan_info.high_freq)
		return true;
	return chan->center_freq >= ar->chan_info.low_freq &&
	       chan->center_freq <= ar->chan_info.high_freq;
}

#define ATH12K_BAND5_START_FREQ_MHZ  5000
#define ATH12K_BAND5_CHAN_SPACING_MHZ 5

/* Convert a 5 GHz center frequency to IEEE 802.11 channel number */
static inline u8 ath12k_5g_chan_to_ieee_num(struct ieee80211_channel *chan)
{
	return (chan->center_freq - ATH12K_BAND5_START_FREQ_MHZ) /
	       ATH12K_BAND5_CHAN_SPACING_MHZ;
}

/**
 * ath12k_display_5ghz_chan_info() - Print 5 GHz channel info for a radio
 * @ar: Pointer to ath12k radio instance
 *
 * Iterates over all 5 GHz channels in the radio's supported band and
 * prints remaining CAC/NOL time, opclass, channel state,
 * channel and frequency.
 *
 * Returns 0 on success, -ENODATA if no 5 GHz band is available for the
 * radio.
 */

static int ath12k_display_5ghz_chan_info(struct ath12k *ar)
{
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	struct ieee80211_supported_band *sband;
	int i;

	lockdep_assert_wiphy(wiphy);

	sband = wiphy->bands[NL80211_BAND_5GHZ];
	if (!sband || !sband->channels || !sband->n_channels) {
		ath12k_err(ar->ab,
			   "get_5ghz_chan_info: no 5GHz channels for radio %d\n",
			   ar->pdev_idx);
		return -ENODATA;
	}

	pr_info("5GHz channel info for radio %d (pdev %d, freq range %u-%u MHz):\n",
		ar->pdev_idx, ar->pdev->pdev_id,
		ar->chan_info.low_freq, ar->chan_info.high_freq);

	for (i = 0; i < sband->n_channels; i++) {
		struct ieee80211_channel *chan;
		const char *dfs_state_str;
		u64 cac_comp_time;
		s32 rem_cac_sec, rem_nop_sec;
		u8 chan_num, opclass;

		chan = &sband->channels[i];

		if (chan->flags & IEEE80211_CHAN_DISABLED)
			continue;

		if (!ath12k_is_chan_in_ar(ar, chan))
			continue;

		chan_num = ath12k_5g_chan_to_ieee_num(chan);
		opclass = ath12k_reg_get_opclass_from_freq_width(NULL,
								 chan->center_freq,
								 20,
								 BIT(BEHAV_NONE));
		ath12k_get_dfs_chan_info(ar, chan, &dfs_state_str,
					 &rem_cac_sec, &cac_comp_time,
					 &rem_nop_sec);
		pr_info("freq: %u chan_num: %u, opclass: %u, state: %s, rem_cac_time: %d cac_comp_time: %llu, rem_nop_time: %d\n",
			chan->center_freq, chan_num, opclass,
			dfs_state_str, rem_cac_sec,
			cac_comp_time, rem_nop_sec);
	}

	return 0;
}

/* Get radio level parameters */
static int ath12k_vendor_display_super_chan_list(struct ath12k *ar, u32 *value)
{
	struct ieee80211_supported_band *band_6g;
	unsigned int i;

	if (!value)
		return -EINVAL;

	band_6g = ath12k_get_6g_band(ar);
	if (!band_6g || !band_6g->channels || !band_6g->n_channels) {
		ath12k_err(ar->ab,
			   "display_super_chan_list is valid only for 6 GHz radio\n");
		return -EOPNOTSUPP;
	}

	for (i = 0; i < band_6g->n_channels; i++) {
		const struct ieee80211_channel *channel;
		u32 channel_flags;
		u16 power_types;
		u8 best_power_mode;

		channel = &band_6g->channels[i];
		channel_flags = channel->flags;
		ath12k_super_chan_power_caps(channel_flags, &power_types,
					     &best_power_mode);

		ath12k_super_chan_dump(ar, channel, power_types, best_power_mode);
	}

	*value = 0;

	return 0;
}

static int ath12k_vendor_get_radio_params(struct ath12k *ar,
					  u32 param,
					  struct ath12k_wifi_generic_params *params,
					  u32 *value)
{
        int ret = -1;

        switch (param) {
        case QCA_WLAN_VENDOR_RADIO_PARAM_TEST:
                *value = 0;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_MGMT_RETRY_LIMIT:
		*value = ar->mgmt_tx_retry_limit;
		ret = 0;
		break;
	case PARAM_RADIO_TXCHAINSOFT:
		*value = ar->cfg_tx_chainmask;
		ret = 0;
		break;
	case ACFG_PARAM_RADIO_RXCHAINMASK:
		*value = ar->cfg_rx_chainmask;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_RTS_CTS_RATE:
                *value = ar->radio_cfg.rts_cts_rate;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_PS_STATE_CHANGE:
                *value = ar->radio_cfg.ps_report;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ARPDHCP_AC_OVERRIDE:
                *value = ar->radio_cfg.arp_override;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ANI_ENABLE:
                *value = ar->radio_cfg.is_ani_enable;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CCA_THRESHOLD:
                *value = ar->radio_cfg.cca_threshold;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_DYN_TX_CHAINMASK:
                *value = ar->radio_cfg.dtcs;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SENS_LEVEL:
                *value = ar->radio_cfg.rxsop_sens_lvl;
                ret = 0;
                break;
	 case QCA_WLAN_VENDOR_RADIO_PARAM_DYN_GROUPING:
		*value = ar->radio_cfg.dyngroup;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DPD_ENABLE:
		{
		    *value = ar->radio_cfg.dpdenable;
		    switch(ar->radio_cfg.dpdenable) {
			case CLI_DPD_CMD_INPROGRES:
			    ath12k_dbg(NULL, ATH12K_DBG_CFG,
				    "DPD cal in progess");
			    break;
			case CLI_DPD_STATUS_FAIL:
			    ath12k_dbg(NULL, ATH12K_DBG_CFG,
				    "DPD cal failed Or DPD disabled BDF loaded");
			    break;
			case CLI_DPD_STATUS_DISABLED:
			    ath12k_dbg(NULL, ATH12K_DBG_CFG,
				    "DPD cal disabled");
			    break;
			case CLI_DPD_STATUS_PASS:
			    ath12k_dbg(NULL, ATH12K_DBG_CFG,
				    "DPD cal Passed !!");
			    break;
			case CLI_DPD_NA_STATE:
			    ath12k_dbg(NULL, ATH12K_DBG_CFG,
				    "INVALID!! DPD not triggered via CLI command");
			    break;
			default:
			    ath12k_dbg(NULL, ATH12K_DBG_CFG,
				    "unknown state");
		    }
		    ret = 0;
		}
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_BURST_DUR:
		*value = ar->radio_cfg.burst_dur;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_BURST_ENABLE:
		*value = ar->radio_cfg.burst_enable;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_EN_PROBE_ALL_BW:
		*value = ar->radio_cfg.en_probe_all_bw;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_SCAN_STRICT_PASSIVE_PCH:
		*value = ar->radio_cfg.radio_cfg_extn.scan_f_strict_passive_pch;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_SET_HW_MODE_CMDID:
                *value = ar->radio_cfg.current_mode;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_LIMIT2G:
                *value = ar->radio_cfg.txpowlimit2G;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_LIMIT5G:
		*value = ar->radio_cfg.txpowlimit5G;
		ret = 0;
		break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_SCALE:
                *value = ar->radio_cfg.txpower_scale;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_BLOCK_INTERBSS:
                *value = ar->radio_cfg.block_interbss;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_TXBF_SOUND_PERIOD_CMDID:
                *value = ar->radio_cfg.txbf_sound_period;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PROMISC_MODE_CMDID:
                *value = ar->radio_cfg.promisc_mode;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_BURST_MODE_CMDID:
                *value = ar->radio_cfg.burst_mode;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_MCAST_BCAST_ECHO:
		*value = ar->radio_cfg.mcast_bcast_echo;
		ret = 0;
		break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_AMSDU:
                *value = ar->radio_cfg.amsdu_mask;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_AMPDU:
                *value = ar->radio_cfg.ampdu_mask;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_HE_MBSSID_CTRL_FRAME_CONFIG:
                *value = ar->radio_cfg.mbssid_en_ctrl_frame;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_PROBE_RESP_RETRY_LIMIT:
                *value = ar->radio_cfg.resp_retry_limit;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CTS_TIMEOUT:
                *value = ar->radio_cfg.cts_timeout;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SLOT_TIME:
                *value = ar->radio_cfg.slot_time;
                ret = 0;
		break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_ACK_TIMEOUT:
                *value = ar->radio_cfg.ack_timeout;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CCK_TX_ENABLE:
                *value = ar->radio_cfg.cck_enable;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_CAL_VER_CHECK:
                *value = ar->radio_cfg.cal_ver_check;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_EQUAL_RU_ALLOCATION_ENABLE:
                *value = ar->radio_cfg.ru_alloc_en;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_MGMT_TTL:
                *value = ar->radio_cfg.mgmt_ttl;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PROBE_RESP_TTL:
                *value = ar->radio_cfg.prb_rsp_ttl;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_MU_PPDU_DURATION:
                *value = ar->radio_cfg.mu_ppdu_dur;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_TBTT_CTRL:
                *value = ar->radio_cfg.tbtt_ctrl;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_SET_PREAM_PUNCT_BW:
                *value = ar->radio_cfg.punct_bw;
                ret = 0;
                break;
        case QCA_WLAN_VENDOR_RADIO_PARAM_GPIO_INPUT:
        {
		u32 i = 0;

		for (i = 0; i < 32; i++) {
			if (ar->radio_cfg.gpio_cfg[i].configured) {
				ath12k_err(NULL, "GPIO %u get: value=%u dir=%u pull=%u intr=%u func=0x%x\n",
					   i, ar->radio_cfg.gpio_cfg[i].gpio_value,
					   ar->radio_cfg.gpio_cfg[i].gpio_dir,
					   ar->radio_cfg.gpio_cfg[i].gpio_pull_type,
					   ar->radio_cfg.gpio_cfg[i].gpio_intr_mode,
					   ar->radio_cfg.gpio_cfg[i].gpio_function);
					/* Return a packed status:
					* [31:24] - GPIO value (0 or 1)
					* [23:16] - GPIO direction
						* [15:8]  - GPIO pull type
					* [7:4]   - GPIO interrupt mode
					* [3:0]   - GPIO function
					*/

					*value =
					(ar->radio_cfg.gpio_cfg[i].gpio_value      << 24) |
					(ar->radio_cfg.gpio_cfg[i].gpio_dir        << 16) |
					(ar->radio_cfg.gpio_cfg[i].gpio_pull_type  << 8)  |
					(ar->radio_cfg.gpio_cfg[i].gpio_intr_mode  << 4)  |
					(ar->radio_cfg.gpio_cfg[i].gpio_function);
			}
		}
		ret = 0;
		break;
        }
        case QCA_WLAN_VENDOR_RADIO_PARAM_GET_TEMPERATURE:
                /* Return cached temperature value */
                *value = ar->thermal.temperature;
                ret = 0;

                ath12k_dbg(NULL, ATH12K_DBG_CFG,
                          "Temperature read: %d°C (0x%x)\n",
                          (s32)*value, *value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_MSDU_TTL:
                *value = ar->radio_cfg.msdu_ttl;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DFS_NOL_SUBCHANNEL_MARKING:
		*value = ar->dfs_sub_channel_marking;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DISPLAY_BAND_CHANS:
		ret = ath12k_vendor_display_band_chans(ar);
		*value = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_DISPLAY_SUPER_CHANNEL_LIST:
		ret = ath12k_vendor_display_super_chan_list(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_RADAR_DETECT_COUNT:
		*value = ar->radar_detect_count;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_CTLPWRSCALE:
		ret = ath12k_vendor_get_ctlpwrscale(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_EN_CHAN_144:
		ret = ath12k_vendor_get_en_chan144(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_CHAN_COEX_DISABLE:
		*value = ar->chan_coex_disable_mask;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_TO_REO_DEST:
                *value = ar->radio_cfg.pdev_to_reo_dest;
                ret = 0;
                break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_NOL_CHAN_LIST:
	{
		struct wiphy *wiphy;
		char *path;
		u32 dump_len = 0;

		if (!ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
			return -EINVAL;

		wiphy = ar->ah->hw->wiphy;
		ret = ath12k_vendor_resolve_nol_path_from_data_extn(params->data,
								    params->data_len,
								    params->radio_idx,
								    &path);
		if (ret)
			return ret;

		ret = ath12k_vendor_dump_nol_to_file_extn(wiphy,
							  params->radio_idx,
							  path, &dump_len);
		kfree(path);
		if (ret)
			return ret;

		*value = dump_len;
		ret = 0;
		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_WSI_BYPASS:
		/* Report 1 if device is currently bypassed, else 0 */
		*value = ar->ab->is_bypassed ? 1 : 0;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_WIFI_PARAM_SUSPEND_DEVICE:
		/* Report 1 if the radio's pdev is currently suspended, else 0.
		 * pdev_suspend is set by the WMI pdev suspend event handler
		 * after firmware confirms the suspend, and cleared by the WMI
		 * pdev resume event handler after firmware confirms the resume.
		 * This reflects the actual firmware-level suspend state of the
		 * radio identified by radio_idx.
		 */
		*value = ar->pdev_suspend ? 1 : 0;
		ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
			   "get_suspend_state: radio_idx=%u pdev_suspend=%d\n",
			   ar->radio_idx, ar->pdev_suspend);
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_BAND_INFO:
		*value = ath12k_vendor_get_radio_band_info(ar);
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ALPHA2:
		ath12k_vendor_sync_alpha2_cache(ar->ah);
		*value = ((u32)ar->alpha2[1] << 8) | ar->alpha2[0];
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ID:
		*value = ar->country_id;
		if (!*value && ar->ah) {
			struct ath12k *tmp_ar;
			int i;

			for_each_ar(ar->ah, tmp_ar, i) {
				if (tmp_ar->country_id) {
					*value = tmp_ar->country_id;
					break;
				}
			}
		}
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_REGDOMAIN:
		u32 source_regdomain;
		u32 regdomain;
		u32 low_5g;
		u32 high_5g;

		if (ar->radio_cfg.regdomain) {
			source_regdomain = ar->radio_cfg.regdomain;
		} else {
			source_regdomain = (u32)(ar->ab->hal_reg_cap[ar->pdev_idx].eeprom_reg_domain & 0xFFFF);
			source_regdomain |= ((u32)(ar->ab->hal_reg_cap[ar->pdev_idx].eeprom_reg_domain_ext & 0xFFFF) << 16);
		}

		regdomain = source_regdomain & 0xFFFF;
		low_5g = ar->ab->hal_reg_cap[ar->pdev_idx].low_5ghz_chan;
		high_5g = ar->ab->hal_reg_cap[ar->pdev_idx].high_5ghz_chan;

		if (low_5g <= ATH12K_MAX_6GHZ_FREQ && high_5g >= ATH12K_MIN_6GHZ_FREQ)
			regdomain |= (source_regdomain & 0xFFFF0000);

		*value = regdomain;
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_CAC_TIMEOUT:
		ret = ath12k_get_current_cac_timeout(ar, value);
		if (ret) {
			ath12k_err(ar->ab,
				   "failed to get CAC timeout for pdev %u: %d\n",
				   ar->pdev_idx, ret);
			break;
		}

		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "Getting CAC timeout for pdev %u: %u sec\n",
			   ar->pdev_idx, *value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_GET_CAC_STATE:
		*value = test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags);
		ret = 0;
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_LIST_5GHZ_CHAN_INFO:
		ret = ath12k_display_5ghz_chan_info(ar);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_BGCAC_TIMEOUT:
		ret = ath12k_get_current_bgcac_timeout(ar, value);
		break;
	case QCA_WLAN_VENDOR_RADIO_PARAM_BLOCK_DFS_LIST: {
		struct ath12k_base *ab = ar->ab;

		*value = ar->ar_extn.block_dfs_channels;

		ath12k_dbg(ab, ATH12K_DBG_REG,
			   "Getting block_dfs_channels for pdev %u: %u\n",
			   ar->pdev_idx, *value);

		ret = 0;
		break;
	}
	case QCA_WLAN_VENDOR_RADIO_PARAM_GET_CHAN_UTIL:
		ret = ath12k_vendor_get_chan_util(ar, value);
		if (ret)
			ath12k_warn(ar->ab,
				    "failed to get chan util: %d\n", ret);
		return ret;
	case QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_STATS_TIMER:
		*value = ar->pdev_stats_timer_interval;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "pdev stats timer interval for pdev %u: %u ms\n",
			   ar->pdev_idx, *value);
		ret = 0;
		break;
	default:
                ath12k_dbg(NULL, ATH12K_DBG_CFG,
                           "Un-supported param: %d\n", param);
                break;
        }

        return ret;
}

/* Find the link-vif of the wdev with the link id to get params
 * If link id is invalid use the default link
 */
int ath12k_vendor_get_wifi_params_extn(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       struct ath12k_wifi_generic_params *params,
				       u64 *value)

{
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	int param = params->value;
	int ret = -1;

	lockdep_assert_wiphy(wiphy);

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return -EINVAL;

	ah = ahvif->ah;

	if (ah && params->link_id == INVALID_LINK_ID) {
		arvif = &ahvif->deflink;
	} else {
		if (params->link_id < ATH12K_NUM_MAX_LINKS)
			arvif = wiphy_dereference(wiphy,
						   ahvif->link[params->link_id]);
	}
	if (!arvif)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "vif: %p param: %d if: %d link: %d\n",
		   vif, param, params->ifindex, params->link_id);
	ret = ath12k_vendor_get_arvif_params(arvif, param, value);

	return ret;
}

/* Find the 'ar' radio instance using the radio index of the wiphy
 * to get params
 */
int ath12k_vendor_get_wiphy_params_extn(struct wiphy *wiphy,
					struct ath12k_wifi_generic_params *params,
					u32 *value)
{
        struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
        struct ath12k_hw *ah = hw->priv;
        struct ath12k *ar = NULL;
        int param = params->value;
        u8 radio_idx = params->radio_idx;
        int ret = -1;

	if ((param == QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ALPHA2 ||
	     param == QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ID ||
	     param == QCA_WLAN_VENDOR_RADIO_PARAM_REGDOMAIN) &&
	    radio_idx == INVALID_RADIO_INDEX)
		radio_idx = 0;

	if (radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index %u (valid range: 0..%u)\n",
			   radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
		return -ENODATA;
	}

	ar = &ah->radio[radio_idx];
	if (!ar) {
                ath12k_err(NULL, "Failed to find ar\n");
                return -ENODATA;
        }

        ath12k_dbg(NULL, ATH12K_DBG_CFG,
                   "ar: %p param: %d if: %d radio: %d\n",
                   ar, param, params->ifindex, radio_idx);
        ret = ath12k_vendor_get_radio_params(ar, param, params, value);

        return ret;
}

int ath12k_vendor_get_wiphy_sr_params_extn(struct wiphy *wiphy,
					   struct ath12k_wifi_generic_params *params,
					   char value[], size_t length)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = NULL;
	uint8_t param = params->value;
	int ret = -1;

	if (params->radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index %u (valid range: 0..%u)\n",
			   params->radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
		return -ENODATA;
	}

	ar = &ah->radio[params->radio_idx];
	if (!ar) {
		ath12k_err(NULL, "Failed to find ar\n");
		return -ENODATA;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ar: %p param: %d if: %d radio: %d\n",
		   ar, param, params->ifindex, params->radio_idx);
	ret = ath12k_vendor_get_sr_radio_params(ar, param, value, length);

	return ret;
}

const struct nla_policy
ath12k_vendor_scan_policy[QCA_WLAN_VENDOR_ATTR_SCAN_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SCAN_RESTTIME]    = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SCAN_WAITTIME]    = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME]  = { .type = NLA_U64 },
	[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN]       = { .type = NLA_NESTED },
};

const struct nla_policy
ath12k_vendor_split_scan_params_policy[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELL]     = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELLREST] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE]      = { .type = NLA_U8 },
};

int ath12k_vendor_parse_cbs_scan_params(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SCAN_MAX + 1];
	struct nlattr *tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_MAX + 1];
	struct ath12k *ar;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_hw *ah;
	struct genl_info *info;
	struct nlattr *freq_attr;
	u8 link_id;
	u8 split_scan_type = 0;
	int num_freqs = 0;
	int rem, ret;

	if (!data || !data_len)
		return -EINVAL;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SCAN_MAX,
			data, data_len, ath12k_vendor_scan_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "CBS: failed to parse top-level attrs: %d\n", ret);
		return ret;
	}

	info = rdev->cur_cmd_info;
	if (info && info->attrs[NL80211_ATTR_MLO_LINK_ID]) {
		link_id = nla_get_u8(info->attrs[NL80211_ATTR_MLO_LINK_ID]);
	} else {
		ath12k_err(NULL, "CBS: MLO link ID attribute missing\n");
		return -EINVAL;
	}

	/* Get vif and arvif from wdev */
	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "CBS: failed to get vif from wdev\n");
		return -EINVAL;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_err(NULL, "CBS: failed to get ahvif from vif\n");
		return -EINVAL;
	}

	ah = ahvif->ah;

	rcu_read_lock();
	if (ah && link_id == INVALID_LINK_ID) {
		arvif = &ahvif->deflink;
	} else {
		if (link_id < ATH12K_NUM_MAX_LINKS)
			arvif = rcu_dereference(ahvif->link[link_id]);
	}
	if (!arvif || !arvif->ar) {
		rcu_read_unlock();
		ath12k_err(NULL, "CBS: failed to get arvif (link=%d)\n", link_id);
		return -EINVAL;
	}
	rcu_read_unlock();

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar) {
		ath12k_err(NULL, "CBS: failed to find ar\n");
		return -EINVAL;
	}

	spin_lock_bh(&ar->data_lock);
	ar->ar_extn.cbs.arvif = arvif;

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_RESTTIME])
		ar->ar_extn.cbs.rest_time =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SCAN_RESTTIME]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_WAITTIME])
		ar->ar_extn.cbs.wait_time =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SCAN_WAITTIME]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME])
		ar->ar_extn.cbs.cbs_total_dwell_time =
			(u32)nla_get_u64(tb[QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES]) {
		num_freqs = 0;
		nla_for_each_nested(freq_attr,
				    tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES],
				    rem) {
			if (num_freqs >= ATH12K_CBS_MAX_CHANNELS) {
				ath12k_err(NULL,
					   "CBS: too many scan frequencies (%d max %d)\n",
					   num_freqs + 1, ATH12K_CBS_MAX_CHANNELS);
				spin_unlock_bh(&ar->data_lock);
				return -EINVAL;
			}

			ar->ar_extn.cbs.chan_list[num_freqs++] =
				nla_get_u32(freq_attr);
		}

		if (!num_freqs) {
			ath12k_err(NULL, "CBS: empty scan frequency list\n");
			spin_unlock_bh(&ar->data_lock);
			return -EINVAL;
		}

		ar->ar_extn.cbs.nchans = (u8)num_freqs;
	}


	if (tb[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN]) {
		ret = nla_parse_nested(tb_split,
				       QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_MAX,
				       tb[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN],
				       ath12k_vendor_split_scan_params_policy,
				       NULL);
		if (ret) {
			ath12k_err(NULL,
				   "CBS: failed to parse SPLIT_SCAN nested attrs: %d\n",
				   ret);
			spin_unlock_bh(&ar->data_lock);
			return ret;
		}

		if (tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELL])
			ar->ar_extn.cbs.dwell_split_time =
				nla_get_u32(tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELL]);

		if (tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELLREST])
			ar->ar_extn.cbs.min_dwell_rest_time =
				nla_get_u32(tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELLREST]);

		if (tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE])
			split_scan_type =
				nla_get_u8(tb_split[QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE]);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS scan params: enable=%u resttime=%u dwellrest=%u waittime=%u dwellsplit=%u totaldwell=%u num_freqs=%u link_id=%u\n",
		   split_scan_type, ar->ar_extn.cbs.rest_time,
		   ar->ar_extn.cbs.min_dwell_rest_time,
		   ar->ar_extn.cbs.wait_time, ar->ar_extn.cbs.dwell_split_time,
		   ar->ar_extn.cbs.cbs_total_dwell_time,
		   ar->ar_extn.cbs.nchans,
		   link_id);


	ar->ar_extn.cbs.cbs_enable = split_scan_type;
	spin_unlock_bh(&ar->data_lock);
	ret = ath12k_cbs_start(ar);
	return ret;
}

void ath12k_vendor_send_cbs_scan_event(struct ath12k *ar,
				       struct wmi_scan_event *scan_ev)
{
	struct ath12k_cbs *cbs;
	struct ath12k_link_vif *arvif;
	struct nlattr *nla_freqs = NULL, *nla_split = NULL;
	struct sk_buff *skb;
	struct wireless_dev *wdev;
	u8 split_scan_type;
	u8 scan_complete;
	u8 link_id = INVALID_LINK_ID;
	u32 scan_complete_freq;

	if (!ar || !ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
		return;
	if (!scan_ev)
		return;

	rcu_read_lock();
	cbs = &ar->ar_extn.cbs;

	spin_lock_bh(&ar->data_lock);

	split_scan_type = ar->ar_extn.cbs.cbs_enable;

	if (!cbs->nchans || cbs->nchans > ATH12K_CBS_MAX_CHANNELS) {
		spin_unlock_bh(&ar->data_lock);
		goto out;
	}

	/* WMI scan_id is a single value shared across all radios, so
	 * scan_ev->channel_freq can reflect a scan submitted by a
	 * different pdev's CBS instance rather than this one. Use the
	 * driver's own chan_list_idx, maintained by the CBS state
	 * machine for this ar, instead of trusting the FW-echoed freq.
	 */
	if (cbs->chan_list_idx >= cbs->nchans) {
		spin_unlock_bh(&ar->data_lock);
		goto out;
	}

	scan_complete_freq = cbs->chan_list[cbs->chan_list_idx];

	if (cbs->chan_list_idx == cbs->nchans - 1)
		scan_complete = VENDOR_SCAN_STATUS_NEW_RESULTS;
	else
		scan_complete = VENDOR_SPLIT_SCAN_COMPLETE_PER_CHANNEL;

	wdev = NULL;
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->ahvif || !arvif->ahvif->vif)
			continue;

		if (arvif->vdev_id == le32_to_cpu(scan_ev->vdev_id)) {
			wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
			link_id = arvif->link_id;
			break;
		}
	}
	spin_unlock_bh(&ar->data_lock);

	if (!wdev)
		goto out;

	skb = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, wdev,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE_INDEX,
					  GFP_ATOMIC);
	if (!skb)
		goto out;

	nla_freqs = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES);
	if (!nla_freqs)
		goto fail;

	if (nla_put_u32(skb, 1, scan_complete_freq))
		goto fail;

	nla_nest_end(skb, nla_freqs);
	nla_freqs = NULL;

	nla_split = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN);
	if (!nla_split)
		goto fail;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE,
		       split_scan_type))
		goto fail;

	nla_nest_end(skb, nla_split);
	nla_split = NULL;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCAN_STATUS, scan_complete))
		goto fail;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID, link_id))
		goto fail;

	cfg80211_vendor_event(skb, GFP_ATOMIC);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "CBS scan done event sent: status=%u split_type=%u scan_complete_freq=%u link_id=%u fw_event_type=%u reason=%u\n",
		   scan_complete, split_scan_type, scan_complete_freq,
		   link_id,
		   le32_to_cpu(scan_ev->event_type),
		   le32_to_cpu(scan_ev->reason));

	goto out;

fail:
	if (nla_split)
		nla_nest_cancel(skb, nla_split);
	if (nla_freqs)
		nla_nest_cancel(skb, nla_freqs);
	kfree_skb(skb);
out:
	rcu_read_unlock();
}


int ath12k_vendor_set_wifi_config_extn(struct wiphy *wiphy, struct nlattr **tb,
				       struct wireless_dev *wdev)
{
	struct ath12k_dp_vif *dp_vif;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	u8 allow_3addr_mc;
	unsigned long links_map;
	u8 link_id;
	u8 peer_addr[ETH_ALEN];
	struct ieee80211_sta *sta;
	struct ath12k_link_sta *arsta;
	struct ath12k_sta *ahsta = NULL;
	int ret;

	ret = ath12k_vendor_set_esp_params_extn(wiphy, tb, wdev);
	if (ret) {
		ath12k_err(NULL, "Failed to set ESP params: %d\n", ret);
		return ret;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "wdev_to_ieee80211_vif returned NULL\n");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC]) {
		nla_memcpy(peer_addr, tb[QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC], ETH_ALEN);

		rcu_read_lock();
		sta = ieee80211_find_sta(vif, peer_addr);
		if (!sta) {
			ath12k_err(NULL, "ieee80211_find_sta failed for %pM\n",
				   peer_addr);
			rcu_read_unlock();
			return -ENOENT;
		}
		ahsta = ath12k_sta_to_ahsta(sta);
	} else if (vif->type == NL80211_IFTYPE_STATION) {
		/* STA mode: hostapd does not send PEER_MAC — use the AP
		 * BSSID from the VIF's BSS config to find the single peer.
		 */
		rcu_read_lock();
		sta = ieee80211_find_sta(vif, vif->bss_conf.bssid);
		if (sta)
			ahsta = ath12k_sta_to_ahsta(sta);
		else
			rcu_read_unlock();
	}

	if (ahsta && tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VHT_MCS_10_11_PEER_SUPP]) {
		u8 supp;

		links_map = ahsta->links_map;
		supp = nla_get_u8(
				tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VHT_MCS_10_11_PEER_SUPP]);
		/* Update higher_vhtmcs_supp on all active links for this peer.
		 * This handles both non-MLO (deflink only) and MLO stations.
		 */
		for_each_set_bit(link_id, &links_map,
				 IEEE80211_MLD_MAX_NUM_LINKS) {
			arsta = ahsta->link[link_id];
			if (arsta)
				arsta->arsta_extn.higher_vhtmcs_supp = supp ? 0xff : 0;
		}
	}

	if (ahsta && tb[QCA_WLAN_VENDOR_ATTR_CONFIG_HE_CAP_INFO_INTERNAL]) {
		u32 he_cap;

		links_map = ahsta->links_map;
		he_cap = nla_get_u32(
				tb[QCA_WLAN_VENDOR_ATTR_CONFIG_HE_CAP_INFO_INTERNAL]);

		for_each_set_bit(link_id, &links_map,
				 IEEE80211_MLD_MAX_NUM_LINKS) {
			arsta = ahsta->link[link_id];
			if (arsta)
				arsta->arsta_extn.he_cap_info_internal = he_cap;
		}
	}
	if (ahsta)
		rcu_read_unlock();

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ALLOW_3ADDR_MC]) {
		allow_3addr_mc = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ALLOW_3ADDR_MC]);

		if (allow_3addr_mc > 1) {
			ath12k_err(NULL, "Invalid allow_3addr_mc value: %u\n", allow_3addr_mc);
			return -EINVAL;
		}

		if (vif->type != NL80211_IFTYPE_STATION) {
			ath12k_dbg(NULL, ATH12K_DBG_PPE, "\n");
			return -EOPNOTSUPP;
		}
		ahvif = ath12k_vif_to_ahvif(vif);
		if (!ahvif) {
			ath12k_err(NULL, "ahvif is NULL\n");
			return -EINVAL;
		}

		dp_vif = &ahvif->dp_vif;
		dp_vif->dp_extn.allow_3addr_mc = !!allow_3addr_mc;
	}

	return 0;
}

int ath12k_vendor_get_wifi_config_handler_extn(struct wiphy *wiphy,
					       struct nlattr **tb,
					       u64 *value)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	u32 generic_cmd;

	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND])
		return -EOPNOTSUPP;

	generic_cmd = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]);
	switch (generic_cmd) {
	case QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP:
		return ath12k_vendor_get_he_mcs_12_13_cap(ah, tb, value);
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "GET_WIFI_CONFIG: unsupported generic_cmd %u\n",
			   generic_cmd);
		return -EOPNOTSUPP;
	}
}

void ath12k_vendor_get_wiphy_config_handler_extn(struct wiphy *wiphy,
						 struct nlattr **tb,
						 struct sk_buff *skb)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	u8 radio_idx = 0;
	u16 thresh_freq;
	int pdev_idx;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_6GHZ_VLP_PRIORITY_THRESH_FREQ]) {
		if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX])
			radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);

		if (radio_idx >= ah->num_radio) {
			ath12k_err(NULL,
				   "Invalid radio index %u (valid range: 0..%u)\n",
				   radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
			return;
		}

		ar = &ah->radio[radio_idx];
		if (ar->ab->is_bypassed) {
			ath12k_info(ar->ab, "Device is bypassed!");
			return;
		}
		pdev_idx = ar->pdev_idx;
		if (pdev_idx < 0 || pdev_idx >= MAX_RADIOS) {
			ath12k_err(NULL, "Invalid pdev index %d for radio index %u\n",
				   pdev_idx, radio_idx);
			return;
		}

		thresh_freq = ar->ab->ath12k_base_extn.reg_6ghz_thresh_priority_freq[pdev_idx];
		if (nla_put_u16(skb,
				QCA_WLAN_VENDOR_ATTR_CONFIG_6GHZ_VLP_PRIORITY_THRESH_FREQ,
				thresh_freq)) {
			ath12k_err(NULL, "failed to put VLP priority threshold frequency\n");
			return;
		}
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE]) {
		u8 adfs_capable;

		if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]) {
			radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);
		} else {
			/* No radio_idx given — find first 5 GHz radio */
			for (radio_idx = 0; radio_idx < ah->num_radio; radio_idx++) {
				ar = &ah->radio[radio_idx];
				if (ar->mac.sbands[NL80211_BAND_5GHZ].channels)
					break;
			}
		}

		if (radio_idx >= ah->num_radio) {
			ath12k_err(NULL,
				   "Invalid radio index %u (valid range: 0..%u)\n",
				   radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
			return;
		}

		ar = &ah->radio[radio_idx];
		if (ar->ab->is_bypassed) {
			ath12k_info(ar->ab, "Device is bypassed!");
			return;
		}
		adfs_capable = test_bit(ar->cfg_rx_chainmask,
					&ar->pdev->cap.adfs_chain_mask) ? 1 : 0;

		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE,
			       adfs_capable)) {
			ath12k_err(NULL, "failed to put agile capable\n");
			return;
		}
	}
}

int ath12k_vendor_get_rropinfo(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	u8 radio_id;
	struct ath12k *ar = NULL;
	struct sk_buff *skb;
	struct nlattr *nla_attr;
	struct nlattr *rtplinst_attr;
	bool info_available = false;
	int i;
	int count = 0;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_rrop_info_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX])
		return -EINVAL;

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio id %d", radio_id);
		return -EINVAL;
	}

	ar = ath12k_ah_to_ar(ah, radio_id);

	if (!ar) {
		ath12k_err(NULL, "RROPINFO: failed to resolve ar from wdev\n");
		return -EINVAL;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb) {
		ath12k_err(NULL, "RROPINFO: Reply skb alloc failed\n");
		return -ENOMEM;
	}

	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL);
	if (!nla_attr) {
		ath12k_err(NULL, "RROPINFO: nla nest start failed for RTPL info\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	for (i = 0; i < ATH12K_NUM_CHANS; i++) {
		const struct rtplinst_extn *ci = &ar->ar_extn.rtplinst[i];

		if ((ci->primary_freq == 0) &&
		    (ci->txpower_throughput == 0) &&
		    (ci->txpower_range == 0))
			continue;

		info_available = true;

		rtplinst_attr = nla_nest_start(skb, i);
		if (!rtplinst_attr) {
			ath12k_err(NULL, "RROPINFO: nla nest start failed for RTPL instance %d\n", i);
			kfree_skb(skb);
			return -EINVAL;
		}

		if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY,
				ci->primary_freq) ||
		    nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT,
				ci->txpower_throughput) ||
		    nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE,
				ci->txpower_range)) {
			ath12k_err(NULL, "RROPINFO: failed to put RTPL instance %d\n", i);
			kfree_skb(skb);
			return -EINVAL;
		}

		nla_nest_end(skb, rtplinst_attr);
		count++;
		ath12k_dbg(NULL, ATH12K_DBG_CFG, "RROPINFO: rtplinst[%d]: freq=%u th=%d range=%d\n",
			   i, ci->primary_freq, ci->txpower_throughput, ci->txpower_range);
	}

	nla_nest_end(skb, nla_attr);

	if (!info_available) {
		kfree_skb(skb);
		return -EINVAL;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG, "RROPINFO: reply sent with %d instances\n", count);
	return cfg80211_vendor_cmd_reply(skb);
}

/**
 * ath12k_vendor_fw_recovery_event - Send a firmware recovery
 * vendor NL80211 event to userspace
 * @wiphy: Pointer to the wireless PHY context
 * @wdev: Pointer to the wireless device context
 * @hw_link_id: Hardware link identifier
 * @event_type: Vendor-defined recovery event type
 * (e.g., ASSERT, RECOVERY_DONE, COREDUMP_COMPLETE)
 *
 * Return: 0 on success or a negative error code on failure.
 */
int ath12k_vendor_fw_recovery_event(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    u8 hw_link_id, u8 radio_idx, int event_type)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_event_alloc(wiphy, wdev,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_INDEX,
					  GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_FW_RECOVERY_HW_LINK_ID, hw_link_id)) {
		kfree_skb(skb);
		return -EINVAL;
	}
	if (nla_put_u8(skb, QCA_WLAN_VENDOR_FW_RECOVERY_RADIO_IDX, radio_idx)) {
		kfree_skb(skb);
		return -EINVAL;
	}
	if (nla_put_u8(skb, QCA_VENDOR_ATTR_FW_RECOVERY_EVENT_TYPE, event_type)) {
		kfree_skb(skb);
		return -EINVAL;
	}

	ath12k_info(NULL, "send event to userspace hw_link_id %u radio_idx %u event type is %d\n",
		    hw_link_id, radio_idx, event_type);
	cfg80211_vendor_event(skb, GFP_KERNEL);

	return 0;
}

int ath12k_vendor_set_wiphy_hwaddr_extn(struct wiphy *wiphy,
					struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = NULL;

	lockdep_assert_wiphy(wiphy);

	if (params->radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index\n");
		return -EINVAL;
	}

	if (params->data_len != ETH_ALEN) {
		ath12k_err(NULL, "Invalid radio MAC address len\n");
		return -EINVAL;
	}

	if (!is_valid_ether_addr(params->data)) {
		ath12k_err(NULL, "Invalid radio MAC address\n");
		return -EINVAL;
	}

	ar = &ah->radio[params->radio_idx];

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ar: %p hwaddr: %pM if: %d radio: %d\n",
		   ar, params->data,
		   params->ifindex, params->radio_idx);

	/* Configure the base MAC address of radio */
	if (wiphy->addresses && params->radio_idx < wiphy->n_addresses) {
		ether_addr_copy(ar->mac_addr, (u8 *)params->data);
		if (ar->ab->pdevs_macaddr_valid)
			ether_addr_copy(ar->pdev->mac_addr, ar->mac_addr);

		if (params->radio_idx == 0) {
			ether_addr_copy(ar->ab->mac_addr, ar->mac_addr);
			SET_IEEE80211_PERM_ADDR(hw, ar->mac_addr);
		}

		ether_addr_copy((u8 *)(&wiphy->addresses[params->radio_idx]),
				ar->mac_addr);
	}

	return 0;
}

/**
 * ath12k_vendor_nfcal_power_event - Send a noise floor calibration power
 * vendor NL80211 event to userspace.
 * @ar: Pointer to the ath12k radio context
 * @param: Pointer to the parsed nfcal power event data
 *
 * Sends QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT carrying nfdbr,
 * nfdbm and freqnum arrays as binary attributes.
 *
 * Return: 0 on success or a negative error code on failure.
 */

/**
 * ath12k_vendor_get_chan_util - Return cached channel utilization.
 * @ar: Pointer to the ath12k radio context
 * @value: Output pointer to store the computed channel utilization (0-100).
 *
 * Reads the pdev counters cached by the periodic pdev_stats_timer work
 * (ath12k_pdev_stats_timer_work).  When the timer is active the counters
 * are refreshed every pdev_stats_timer_interval ms, so no blocking WMI
 * round-trip is needed here.
 *
 * If the timer is disabled (interval == 0) or the counters have not yet
 * been populated, falls back to a synchronous WMI request.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int ath12k_vendor_get_chan_util(struct ath12k *ar, u32 *value)
{
	u32 rx_clear_count;
	u32 cycle_count;
	u32 prev_rx_clear_count;
	u32 prev_cycle_count;
	u32 delta_rx_clear;
	u32 delta_cycle;

	if (!ar || !value)
		return -EINVAL;

	spin_lock_bh(&ar->data_lock);
	rx_clear_count = ar->pdev_rx_clear_count;
	cycle_count    = ar->pdev_cycle_count;
	prev_rx_clear_count = ar->pdev_prev_rx_clear_count;
	prev_cycle_count    = ar->pdev_prev_cycle_count;
	spin_unlock_bh(&ar->data_lock);

	/* Fall back to a synchronous WMI request if the periodic timer has
	 * not yet populated the cache (both counters are still zero).
	 */
	if (!cycle_count) {
		struct ath12k_fw_stats_req_params params = {};
		struct ath12k_fw_stats_pdev *pdev;
		int ret;

		params.pdev_id  = ar->pdev->pdev_id;
		params.stats_id = WMI_REQUEST_PDEV_STAT;

		ret = ath12k_mac_get_fw_stats(ar, &params);
		if (ret) {
			ath12k_warn(ar->ab,
				    "failed to fetch pdev stats for chan util: %d\n",
				    ret);
			return ret;
		}

		spin_lock_bh(&ar->data_lock);
		pdev = list_first_entry_or_null(&ar->fw_stats.pdevs,
						struct ath12k_fw_stats_pdev, list);
		if (!pdev) {
			spin_unlock_bh(&ar->data_lock);
			ath12k_warn(ar->ab, "no pdev stats available for chan util\n");
			return -ENODATA;
		}
		rx_clear_count = pdev->rx_clear_count;
		cycle_count    = pdev->cycle_count;
		spin_unlock_bh(&ar->data_lock);
		/* Synchronous fallback: no previous snapshot available, so
		 * we cannot compute a meaningful delta.  Use the absolute
		 * ratio as a best-effort one-shot estimate for the cold-cache
		 * case only.
		 */
		if (!cycle_count) {
			ath12k_warn(ar->ab, "cycle_count is zero, cannot compute chan util\n");
			return -ENODATA;
		}
		*value = (u32)div_u64((u64)rx_clear_count * 100, cycle_count);
		if (*value > 100)
			*value = 100;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "chan util (fallback): hw_link_id=%u rx_clear_count=%u cycle_count=%u chan_util=%u%%\n",
			   ar->hw_link_id, rx_clear_count, cycle_count, *value);
		return 0;
	}

	/* Timer-driven path: compute utilization over the last interval using
	 * the delta between the two most recent counter snapshots.
	 * Both counters are free-running 32-bit HW registers; unsigned
	 * subtraction handles wrap-around correctly.
	 */
	delta_cycle    = cycle_count - prev_cycle_count;
	delta_rx_clear = rx_clear_count - prev_rx_clear_count;

	/* Guard: delta_cycle zero means the timer fired before firmware
	 * delivered a new snapshot (e.g. first interval or counter reset).
	 * Return -EAGAIN so the caller can retry or use a stale value.
	 */
	if (!delta_cycle) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "chan util: delta_cycle is zero, skipping update\n");
		return -EAGAIN;
	}

	*value = (u32)div_u64((u64)delta_rx_clear * 100, delta_cycle);
	if (*value > 100)
		*value = 100;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "chan util: hw_link_id=%u delta_rx_clear=%u delta_cycle=%u chan_util=%u%%\n",
		   ar->hw_link_id, delta_rx_clear, delta_cycle, *value);
	return 0;
}

int ath12k_vendor_nfcal_power_event(struct ath12k *ar,
				    const struct ath12k_wmi_nfcal_power_event *param)
{
	struct wiphy *wiphy;
	struct sk_buff *skb;
	u8 hw_link_id;

	if (!ar || !ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
		return -EINVAL;

	wiphy = ar->ah->hw->wiphy;
	hw_link_id = ar->hw_link_id;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT_INDEX,
					  GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_HW_LINK_ID,
		       hw_link_id))
		goto err;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBR,
		    param->num_nfdbr_dbm * sizeof(s8), param->nfdbr))
		goto err;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBM,
		    param->num_nfdbr_dbm * sizeof(s8), param->nfdbm))
		goto err;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_FREQNUM,
		    param->num_freq * sizeof(u32), param->freqnum))
		goto err;

	if (nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_NFDBR_DBM,
			param->num_nfdbr_dbm))
		goto err;

	if (nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_FREQ,
			param->num_freq))
		goto err;

	cfg80211_vendor_event(skb, GFP_ATOMIC);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "nfcal power vendor event sent: hw_link_id=%u num_nfdbr_dbm=%u num_freq=%u\n",
		   hw_link_id, param->num_nfdbr_dbm, param->num_freq);

	return 0;

err:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * ath12k_vendor_suspend_device_prechecks - Validate preconditions for
 * suspend/resume operations.
 * @ar:  ath12k radio identified by radio_idx
 * @op:  operation (1 = suspend, 0 = resume)
 *
 * Checks common to both suspend and resume:
 *  - op must be 1 (suspend) or 0 (resume)
 *  - ab must not be bypassed
 *  - no SSR in progress
 *  - ERP must be off
 *  - no WSI remap in progress
 *  - Q6 must not be powered down
 *
 * Additional checks for suspend (op == 1):
 *  - no active vdevs on the radio
 *  - radio not already in pdev_suspend state
 *
 * Additional checks for resume (op == 0):
 *  - radio must have pdev_user_suspend set
 *
 * Return: 0 if all prechecks pass, negative error code otherwise.
 */
static int ath12k_vendor_suspend_device_prechecks(struct ath12k *ar,
						  u32 op)
{
	struct ath12k_base *ab = ar->ab;
	int erp_state;

	if (op != 1 && op != 0) {
		ath12k_err(ab,
			   "suspend_device: invalid value %u (1=suspend, 0=resume)\n",
			   op);
		return -EINVAL;
	}

	/* Common prechecks for both suspend and resume */
	if (ab->is_bypassed) {
		ath12k_err(ab, "suspend_device: ab is bypassed\n");
		return -EPERM;
	}

	if (ath12k_hw_group_recovery_in_progress(ab->ag)) {
		ath12k_err(ab, "suspend_device: SSR is in progress\n");
		return -EBUSY;
	}

	erp_state = ath12k_erp_get_sm_state();
	if (erp_state != ATH12K_ERP_OFF) {
		ath12k_err(ab,
			   "suspend_device: ERP is active (state=%d), cannot suspend\n",
			   erp_state);
		return -EBUSY;
	}

	if (ab->ag->wsi_remap_in_progress) {
		ath12k_err(ab, "suspend_device: WSI bypass in progress\n");
		return -EBUSY;
	}

	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags)) {
		ath12k_err(ab, "suspend_device: Q6 is powered down\n");
		return -EPERM;
	}

	/* Suspend-specific prechecks */
	if (op == 1) {
		if (ar->num_created_vdevs > 0) {
			ath12k_err(ab,
				   "suspend_device: vaps active on radio_idx=%u, cannot suspend\n",
				   ar->radio_idx);
			return -EBUSY;
		}
		/* Reject suspend if pdev_suspend is already set.
		 * i.e the device is already in suspended state.
		 */
		if (ar->pdev_suspend) {
			ath12k_err(ab,
				   "suspend_device: radio_idx=%u already suspended (pdev_suspend=1), cannot suspend again\n",
				   ar->radio_idx);
			return -EALREADY;
		}
	} else {
		if (!ar->pdev_user_suspend) {
			ath12k_err(ab,
				   "suspend_device: radio_idx=%u not user-suspended, cannot resume\n",
				   ar->radio_idx);
			return -EALREADY;
		}
	}

	return 0;
}

/**
 * ath12k_vendor_suspend_device_extn - Suspend or resume a single ath12k_base.
 * @wiphy: wiphy pointer (used to derive ah and the radio array)
 * @params: generic vendor params; radio_idx identifies the target radio and
 *          thereby the ab to control; value selects the operation:
 *            1 - suspend: calls ath12k_mac_stop() on the radio identified by radio_idx
 *            0 - resume:  calls ath12k_mac_start() on the radio identified by radio_idx
 *
 * Prechecks performed (both operations):
 *  - params->value must be 1 (suspend) or 0 (resume)
 *  - radio_idx must be valid (not INVALID_RADIO_INDEX and within range)
 *  - ab must not be bypassed
 *  - no SSR in progress
 *  - ERP must be off
 *  - no WSI remap in progress
 *  - Q6 must not be powered down
 *  - hw state must be ATH12K_HW_STATE_ON
 *
 * Additional prechecks for suspend (op == 1):
 *  - no active vdevs on the radio
 *  - radio not already in pdev_suspend state
 *
 * Additional prechecks for resume (op == 0):
 *  - radio must have pdev_user_suspend set
 *
 * The caller must hold the wiphy mutex; hw_mutex is acquired internally
 * around the ath12k_mac_stop() / ath12k_mac_start() calls.
 *
 * Return: 0 on success, negative error code on failure.
 */
int ath12k_vendor_suspend_device_extn(struct wiphy *wiphy,
				     struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_hw *hw;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	struct ath12k_base *ab;
	u8 radio_idx;
	u32 op;
	int ret = 0;

	lockdep_assert_wiphy(wiphy);

	if (!params) {
		ath12k_err(NULL, "suspend_device: NULL params\n");
		return -EINVAL;
	}

	op = params->value;

	radio_idx = params->radio_idx;
	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = hw->priv;

	if (radio_idx == INVALID_RADIO_INDEX || radio_idx >= ah->num_radio) {
		ath12k_err(NULL,
			   "suspend_device: invalid radio_idx %u (valid range 0-%u)\n",
			   radio_idx, ah->num_radio ? ah->num_radio - 1 : 0);
		return -EINVAL;
	}

	ar = &ah->radio[radio_idx];
	ab = ar->ab;

	/* From this point ab is valid — all failures post an event. */

	ret = ath12k_vendor_suspend_device_prechecks(ar, op);
	if (ret)
		goto fail;

	mutex_lock(&ah->hw_mutex);

	if (ah->state != ATH12K_HW_STATE_ON) {
		ath12k_err(ab,
			   "suspend_device: hw not in ON state (state=%d)\n",
			   ah->state);
		ret = -EBUSY;
		goto unlock_fail;
	}

	if (op == 1) {
		/* Suspend: set pdev_user_suspend so that ath12k_mac_stop()
		 * calls ath12k_mac_pdev_suspend() unconditionally (subject to the
		 * allocated_vdev_map and pdev_suspend prechecks inside mac_stop).
		 */
		ar->pdev_user_suspend = true;
		ath12k_mac_stop(ar);
		ath12k_dbg(ab, ATH12K_DBG_CFG,
			   "suspend_device: radio_idx=%u stopped "
			   "pdev_user_suspend=%d pdev_suspend=%d\n",
			   ar->radio_idx, ar->pdev_user_suspend,
			   ar->pdev_suspend);

		/* Determine the actual suspend outcome: ath12k_mac_pdev_suspend()
		 * inside mac_stop may time out (logs a warning but does not fail
		 * mac_stop), in which case pdev_suspend stays false.  Set
		 * ret=-ETIMEDOUT if the radio did not confirm the suspend so the
		 * unified event at fail: reports the real outcome.
		 */
		if (!ar->pdev_suspend)
			ret = -ETIMEDOUT;

		ath12k_info(ab,
			    "suspend_device: ab suspended ret=%d\n", ret);
	} else {
		/* Resume: pdev_user_suspend is the authoritative flag that this
		 * radio was stopped by a suspend_device command.
		 */
		ret = ath12k_mac_start(ar);
		if (ret) {
			ath12k_err(ab,
				   "suspend_device: mac_start failed radio_idx=%u ret=%d "
				   "pdev_user_suspend=%d pdev_suspend=%d\n",
				   ar->radio_idx, ret,
				   ar->pdev_user_suspend, ar->pdev_suspend);
			ar->pdev_user_suspend = false;
			goto unlock_fail;
		}
		ar->pdev_user_suspend = false;
		ab->powerup_triggered = false;
		ath12k_info(ab, "suspend_device: ab resumed successfully\n");
	}

unlock_fail:
	mutex_unlock(&ah->hw_mutex);
fail:
	/* Single unified event for both success and failure paths.
	 * ar->pdev_suspend reflects the current firmware suspend state of
	 * the radio identified by radio_idx; ret is 0 on success, negative
	 * on failure or timeout.
	 */
	ath12k_vendor_suspend_device_event(ar, ret);
	return ret;
}

/*
 * ath12k_vendor_send_event - Driver helper to
 * send a device-wide firmware recovery vendor event
 * @ab: Pointer to the ath12k base device context
 * @event_flag: Vendor-defined recovery event type
 * (e.g., ASSERT, RECOVERY_DONE, COREDUMP_COMPLETE)
 */
void ath12k_vendor_send_event(struct ath12k_base *ab,
			      u8 event_flag)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;

	if (ab->num_radios > 0) {
		/*
		* Use the first radio's pdev context to send the vendor event.
		* This assumes that the recovery event is device-wide and not
		* specific to a particular radio.
		*/
		pdev = &ab->pdevs[0];
		ar = pdev->ar;

		if (ar && ar->ah && ar->ah->hw && ar->ah->hw->wiphy) {
			ath12k_vendor_fw_recovery_event(ar->ah->hw->wiphy, NULL,
							ar->hw_link_id,
							ar->radio_idx,
							event_flag);
			ath12k_info(ab, "FW recovery vendor event send: event_type: %u link: %u, radio_idx: %u\n",
					event_flag, ar->hw_link_id, ar->radio_idx);
		} else {
			ath12k_err(ab, "Cannot send recovery vendor event: invalid radio context\n");

		}
	} else {
		ath12k_err(ab, "Cannot send recovery vendor event: no radios available\n");
	}
}

/*
 * Only the 5th octet may vary when ATH12K_MBSSID_MAX_INTERFACES == 16.
 * Preserve the upper nibble and modify the lower nibble for each vif.
 */
void ath12k_compute_mbssid_mac(struct ath12k_base *ab, u8 mac[ETH_ALEN], u8 vif_id)
{
	if (vif_id >= ATH12K_MBSSID_MAX_INTERFACES) {
		ath12k_err(ab, "Invalid BSS ID for multiple BSSID group\n");
		return;
	}

	/* Ensure locally administered and unicast */
	mac[0] &= ~0x01;
	/* Currently vendor MAC address with MBSSID is supported only for 6 GHz
	 * hence the 0th byte has lower octet set to 0x02.*/
	mac[0] |= 0x02;

	mac[5] = (mac[5] & 0xF0) | (((mac[5] & 0x0F) + vif_id) & 0x0F);
}

void ath12k_compute_link_vif_mac(u8 *mac_addr,
				 const u8 *base,
				 u8 vif_id,
				 bool use_byte0)
{
	u8 cur, add, new;

	ether_addr_copy(mac_addr, base);

	/* For vif_id 0, return base mac; also set LA bit for cohost mode */
	if (!vif_id) {
		if (!use_byte0)
			mac_addr[0] |= 0x02;
		return;
	}

	if (use_byte0) {
		cur = (mac_addr[0] >> 2) & 0x3F;
		add = vif_id & 0x3F;
		new = (cur + add) & 0x3F;
		mac_addr[0] = (mac_addr[0] & ~0xFC) | (new << 2);
	} else {
		cur = mac_addr[5] & 0xF;
		add = vif_id & 0xF;
		new = (cur + add) & 0xF;
		mac_addr[5] = (mac_addr[5] & ~0xF) | new;
	}

	/* Ensure locally administered and unicast */
	mac_addr[0] &= ~0x01;
	mac_addr[0] |= 0x02;
}

#define ATH12K_VENDOR_MAC_CONFIG_FLAG_ALLOC_BSSID 0x00000001
#define ATH12K_VENDOR_MAC_CONFIG_FLAG_FREE_BSSID 0x00000002

int ath12k_vendor_derive_link_bss_addr_extn(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAX + 1];
	struct sk_buff *skb;
	struct ieee80211_hw *hw;
	struct ath12k_hw *ah;
	struct ath12k *ar = NULL;
	struct ath12k_base *ab = NULL;
	struct ath12k_link_vif *arvif;
	u8 mac_addr[ETH_ALEN], *base_addr;
	u8 radio_idx, vif_id = 0xff;
	u32 flags = 0;
	enum nl80211_iftype iftype = NL80211_IFTYPE_UNSPECIFIED;
	u8 pdev_bitmap;
	bool byte0 = true, mbssid_enabled = false;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAX,
			data, data_len,
			ath12k_wifi_mac_config_policy, NULL);
	if (ret)
		return ret;

	if (tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_INDEX])
		radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_INDEX]);
	else {
		ath12k_err(NULL, "Missing radio index\n");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_BSS_ID])
		vif_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_BSS_ID]);
	else {
		ath12k_err(NULL, "Missing BSS id\n");
		return -EINVAL;
	}

	/* Optional flags provided by userspace */
	if (tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_FLAGS])
		flags = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_FLAGS]);

	/* Optional interface type provided by userspace */
	if (tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_IFTYPE]) {
		iftype = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_IFTYPE]);
		if (iftype > NL80211_IFTYPE_MAX) {
			ath12k_err(NULL, "Invalid iftype\n");
			return -EINVAL;
		}
	}

	if (!wiphy->addresses || radio_idx >= wiphy->n_addresses) {
		ath12k_err(NULL, "No base addresses available\n");
		return -EINVAL;
	}

	/* Get hw/ah to know which byte index to use for VIF ID */
	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = hw ? hw->priv : NULL;
	if (!ah || radio_idx >= ah->num_radio)
		return -EINVAL;
	ar = &ah->radio[radio_idx];

	ab = ar->ab;

	if (vif_id > (TARGET_NUM_VDEVS - 1)) {
		ath12k_err(ab, "Invalid BSS id\n");
		return -EINVAL;
	}

	/* Optional MBSSID group parameters from userspace */
	if (tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_ENABLED]) {
		mbssid_enabled = nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_ENABLED]);
		if (vif_id >= ATH12K_MBSSID_MAX_INTERFACES) {
			ath12k_err(ab, "Invalid BSS ID for multiple BSSID group\n");
			return -EINVAL;
		}
	}

	base_addr = wiphy->addresses[radio_idx].addr;
	if (iftype == NL80211_IFTYPE_AP && mbssid_enabled) {
		if (!ar->supports_6ghz) {
			ath12k_err(ab,
				   "Static MAC address with MBSSID enabled is allowed only in 6 GHz\n");
			return -EINVAL;
		}
		ether_addr_copy(mac_addr, base_addr);
		ath12k_compute_mbssid_mac(ab, mac_addr, vif_id);

		vif_id = ((16 + (mac_addr[5] & 0x0F)) - (base_addr[5] & 0x0F)) % 16;
	} else {
		/* Select byte index to encode VIF ID: 0 or 5 */
		if (iftype == NL80211_IFTYPE_AP) {
			pdev_bitmap = ath12k_cfg_get(ar->ab,
					ATH12K_CFG_COHOSTED_BSS_IND_ENABLE);
			if (pdev_bitmap & (1 << ar->pdev_idx))
				byte0 = false;
		}

		ath12k_compute_link_vif_mac(mac_addr,
					    base_addr,
					    vif_id,
					    byte0);
	}

	/* Check and set the used MAC bit only when ALLOC flag is set */
	if (flags & ATH12K_VENDOR_MAC_CONFIG_FLAG_ALLOC_BSSID) {
		if (ar->vendor_mac_used_bitmap & (1 << vif_id)) {
			ath12k_err(NULL, "Requested id %d addr %pM in use\n",
				   vif_id, mac_addr);
			return -EADDRINUSE;
		}

		list_for_each_entry(arvif, &ar->arvifs, list) {
			if (!is_zero_ether_addr(arvif->bssid) &&
			    ether_addr_equal(arvif->bssid, mac_addr)) {
				ath12k_err(NULL, "Requested id %d addr %pM in use\n",
					   vif_id, mac_addr);
				return -EADDRINUSE;
			}
		}

		ar->vendor_mac_used_bitmap |= (1 << vif_id);
		ath12k_err(NULL, "Allocated BSS id %d MAC %pM \n",
			   vif_id, mac_addr);
	} else if (flags & ATH12K_VENDOR_MAC_CONFIG_FLAG_FREE_BSSID) {
		ar->vendor_mac_used_bitmap &= ~(1 << vif_id);
		ath12k_err(NULL, "Freed BSS id %d MAC %pM \n",
			   vif_id, mac_addr);
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, nla_total_size(ETH_ALEN));
	if (!skb)
		return -ENOMEM;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAC_ADDR, ETH_ALEN, mac_addr))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EMSGSIZE;
}

/*
 * ath12k_get_radio_by_index - Get ath12k instance by radio index
 * @wiphy: Pointer to wiphy
 * @tb: Array of parsed attributes
 *
 * This function retrieves the ath12k instance corresponding to the
 * specified radio index from the parsed attributes. It checks if the
 * radio index attribute is present and valid.
 * Returns a pointer to the ath12k instance or NULL if not found or invalid.
 */
static struct ath12k *ath12k_get_radio_by_index(struct wiphy *wiphy,
                                                struct nlattr *tb[])
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	u8 radio_id;

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CTL_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio id %d", radio_id);
		return NULL;
	}

	return &ah->radio[radio_id];
}

/*
 * ath12k_vendor_ctl_table- Set CTL table It provides
 * simplified power values intended for helping external Auto channel Selection
 * applications compare potential Tx power performance between channels, other
 * operating conditions remaining identical.
 * @wiphy: Pointer to wiphy
 * @wdev: wireless wdev
 * @data: vendor data
 * @data_len: length of data
 * Returns 0 if success.
 */
int ath12k_vendor_ctl_table(struct wiphy *wiphy,
		      	    struct wireless_dev *wdev,
			    const void *data,
			    int data_len)
{
	struct nlattr *vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_MAX + 1];
	struct wmi_pdev_set_ctl_table_cmd_fixed_param *cmd;
	u32 ctl_data_len, ctl_cmd_len, ctl_tlv_len, band, len;
	struct ath12k_wmi_pdev *wmi;
	const uint8_t *ctl_data;
	uint32_t *ctl_array;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	struct ath12k *ar;
	void *ptr;
	int ret;

	if (nla_parse(vendor, QCA_WLAN_VENDOR_ATTR_CTL_TABLE_MAX,
		      data, data_len,
		      ath12k_ctl_table_policy,
		      NULL)) {
		ath12k_err(NULL, "Failed to parse vendor attributes");
		return -1;
	}

	if (!vendor[QCA_WLAN_VENDOR_ATTR_CTL_RADIO_INDEX] ||
	    !vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_LENGTH] ||
	    !vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_BAND] ||
	    !vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_DATA]) {
		ath12k_err(NULL, "Invalid input");
		return -1;
	}

	ar = ath12k_get_radio_by_index(wiphy, vendor);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL in %s", __func__);
		return -ENODATA;
	}

	ctl_data_len = nla_get_u32(vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_LENGTH]);
	if (!ctl_data_len || ctl_data_len > QCA_WLAN_MAX_CTL_SIZE ||
	    ctl_data_len > nla_len(vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_DATA])) {
		ath12k_err(NULL, "Invalid CTL table length %u", ctl_data_len);
		return -EINVAL;
	}

	ctl_cmd_len = ctl_data_len + sizeof(uint32_t);
	ctl_tlv_len = TLV_HDR_SIZE + roundup(ctl_cmd_len, sizeof(u32));
	len = sizeof(*cmd) + ctl_tlv_len;
	wmi = ar->wmi;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_ctl_table_cmd_fixed_param*)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_CTL_TABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->ctl_len = cpu_to_le32(ctl_cmd_len);

	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, ctl_cmd_len);
	ptr += TLV_HDR_SIZE;
	ctl_array = (uint32_t *)ptr;
	band = nla_get_u32(vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_BAND]);
	ctl_data = nla_data(vendor[QCA_WLAN_VENDOR_ATTR_CTL_TABLE_DATA]);
	ctl_array[0] = cpu_to_le32(band);
	memcpy(&ctl_array[1], ctl_data, ctl_data_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI CTL TABLE: pdev %d ctl len %d\n",
		   cmd->pdev_id, cmd->ctl_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_CTL_TABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to send WMI_PDEV_SET_CTL_TABLE_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

/*
 * ath12k_vendor_rtt_blob - Download RTT blob fragment to firmware.
 * Receives the blob in fragments; header is prepended by userspace tool.
 * @wiphy: Pointer to wiphy
 * @wdev: wireless wdev
 * @data: vendor data
 * @data_len: length of data
 * Returns 0 on success, negative errno on failure.
 */
int ath12k_vendor_rtt_blob(struct wiphy *wiphy,
                          struct wireless_dev *wdev,
                          const void *data,
                          int data_len)
{
       struct ath12k_wmi_pdev *wmi;
       struct ath12k *ar;
       struct wmi_tlv *tlv;
       struct wmi_pdev_download_rtt_blob_cmd_fixed_param *cmd;
       struct sk_buff *skb;
       struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_MAX + 1];
       u32 rtt_data_len, rtt_cmd_len, rtt_tlv_len, len;
       u32 *rtt_array;
       const u8 *rtt_data;
       void *ptr;
       int ret;

       if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RTT_BLOB_MAX,
                     data, data_len, ath12k_rtt_blob_policy, NULL)) {
               ath12k_err(NULL, "Failed to parse RTT blob vendor attributes");
               return -EINVAL;
       }

       if (!tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_RADIO_IDX] ||
           !tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_LENGTH] ||
           !tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_BAND] ||
           !tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_DATA]) {
               ath12k_err(NULL, "Missing RTT blob attributes");
               return -EINVAL;
       }

       ar = ath12k_get_radio_by_index(wiphy, tb);
       if (!ar) {
               ath12k_err(NULL, "%s: ar is NULL", __func__);
               return -ENODATA;
       }

       rtt_data_len = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_LENGTH]);
       if (!rtt_data_len || rtt_data_len > QCA_WLAN_MAX_RTT_BLOB_SIZE ||
           rtt_data_len > nla_len(tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_DATA])) {
               ath12k_err(NULL, "Invalid RTT blob length %u", rtt_data_len);
               return -EINVAL;
       }

       /* rtt_cmd_len = data + one leading u32 for band, rounded to word */
       rtt_cmd_len = rtt_data_len + sizeof(u32);
       rtt_tlv_len = TLV_HDR_SIZE + roundup(rtt_cmd_len, sizeof(u32));
       len = sizeof(*cmd) + rtt_tlv_len;

       wmi = ar->wmi;
       skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
       if (!skb)
               return -ENOMEM;

       cmd = (struct wmi_pdev_download_rtt_blob_cmd_fixed_param *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_DOWNLOAD_RTT_BLOB_CMD,
				       sizeof(*cmd));
       cmd->pdev_id  = cpu_to_le32(ar->pdev->pdev_id);
       cmd->rtt_len  = cpu_to_le32(rtt_cmd_len);

       ptr = skb->data + sizeof(*cmd);
       tlv = ptr;
       tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, rtt_cmd_len);
       ptr += TLV_HDR_SIZE;
       rtt_array = (u32 *)ptr;
       rtt_array[0] = cpu_to_le32(nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_BAND]));
       rtt_data = nla_data(tb[QCA_WLAN_VENDOR_ATTR_RTT_BLOB_DATA]);
       memcpy(&rtt_array[1], rtt_data, rtt_data_len);

       ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                  "WMI RTT BLOB: pdev %d rtt_len %d\n",
                  ar->pdev->pdev_id, rtt_cmd_len);

       ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_DOWNLOAD_RTT_BLOB_CMDID);
       if (ret) {
               ath12k_warn(ar->ab,
                           "Failed to send WMI_PDEV_DOWNLOAD_RTT_BLOB_CMDID\n");
               dev_kfree_skb(skb);
               return ret;
       }

       return 0;
}

#define WMI_STA_MAX_LIMIT 40

const struct nla_policy
ath12k_sta_max_pwr_table_policy[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_RADIO_INDEX] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_NUM_PEERS]   = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_DATA]        = { .type = NLA_BINARY },
};

/**
 * ath12k_vendor_sta_max_pwr_table - Set per-STA maximum TX power table.
 * @wiphy: pointer to wiphy
 * @wdev: wireless wdev
 * @data: NL vendor data
 * @data_len: length of data
 *
 * Receives one batch of up to WMI_STA_MAX_LIMIT (40) peer entries from
 * acfgtool and sends them to firmware via WMI_PEER_BULK_SET_CMDID with
 * WMI_PEER_USE_FIXED_PWR.  The DATA blob is a flat packed array of
 * 10-byte entries: 6-byte MAC address followed by s32 power limit (LE).
 *
 * A RADIO_INDEX attribute selects which radio (pdev) to target.
 */
static int ath12k_wmi_send_peer_bulk_pwr_cmd(struct ath12k *ar,
					     struct ath12k_sta_peer_table_entry *peers,
					     u32 num_peers)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_bulk_set_cmd *cmd;
	struct wmi_peer_list *peer_entry;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u32 peers_len, total_len;
	u8 *ptr;
	int ret, i;

	peers_len = num_peers * sizeof(*peer_entry);
	total_len = sizeof(*cmd) + TLV_HDR_SIZE + peers_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, total_len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = (struct wmi_peer_bulk_set_cmd *)ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_BULK_SET_CMD_FIXED_PARAM) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_var = 0;
	ptr += sizeof(*cmd);

	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, peers_len);
	ptr += TLV_HDR_SIZE;

	peer_entry = (struct wmi_peer_list *)ptr;
	for (i = 0; i < num_peers; i++) {
		peer_entry->tlv_header =
			FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_LIST) |
			FIELD_PREP(WMI_TLV_LEN, sizeof(*peer_entry) - TLV_HDR_SIZE);
		ether_addr_copy(peer_entry->peer_macaddr.addr,
				peers[i].peer_macaddr);
		peer_entry->param_id    = cpu_to_le32(WMI_PEER_USE_FIXED_PWR);
		peer_entry->param_value = cpu_to_le32((u32)peers[i].peer_pwr_limit);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "sta_max_pwr_table: peer[%d] mac=%pM pwr_limit=%d\n",
			   i, peers[i].peer_macaddr,
			   peers[i].peer_pwr_limit);

		peer_entry++;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "sta_max_pwr_table: sending WMI_PEER_BULK_SET_CMDID num_peers=%u pdev_id=%d\n",
		   num_peers, ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_BULK_SET_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_BULK_SET_CMDID: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_vendor_sta_max_pwr_table(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_MAX + 1];
	struct ath12k_sta_peer_table_entry *peers;
	struct ath12k_sta_peer_table_entry *valid_peers;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	u32 num_peers, num_valid, j;
	u8 radio_id;
	int ret;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_MAX,
		      data, data_len, ath12k_sta_max_pwr_table_policy, NULL)) {
		ath12k_err(NULL, "Failed to parse sta_max_pwr_table attributes\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_RADIO_INDEX] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_NUM_PEERS] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_DATA]) {
		ath12k_err(NULL, "sta_max_pwr_table: missing required attributes\n");
		return -EINVAL;
	}

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "sta_max_pwr_table: invalid radio_id %u\n", radio_id);
		return -EINVAL;
	}
	ar = &ah->radio[radio_id];

	num_peers = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_NUM_PEERS]);
	if (!num_peers || num_peers > WMI_STA_MAX_LIMIT) {
		ath12k_err(ar->ab,
			   "sta_max_pwr_table: invalid num_peers %u (max %d)\n",
			   num_peers, WMI_STA_MAX_LIMIT);
		return -EINVAL;
	}

	if (nla_len(tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_DATA]) !=
	    (int)(num_peers * sizeof(struct ath12k_sta_peer_table_entry))) {
		ath12k_err(ar->ab,
			   "sta_max_pwr_table: data length mismatch (got %d, expected %zu)\n",
			   nla_len(tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_DATA]),
			   num_peers * sizeof(struct ath12k_sta_peer_table_entry));
		return -EINVAL;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "sta_max_pwr_table: radio_id=%u num_peers=%u\n",
		   radio_id, num_peers);

	peers = nla_data(tb[QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_DATA]);

	/*
	 * Filter to only associated peers before building the WMI buffer.
	 * Sending WMI_PEER_BULK_SET_CMDID for a peer not in firmware's peer
	 * table causes a firmware assert.
	 */
	valid_peers = kcalloc(num_peers, sizeof(*valid_peers), GFP_KERNEL);
	if (!valid_peers)
		return -ENOMEM;

	num_valid = 0;
	spin_lock_bh(&ar->arsta_lock);
	for (j = 0; j < num_peers; j++) {
		if (ath12k_link_sta_find_by_addr(ar, peers[j].peer_macaddr)) {
			valid_peers[num_valid++] = peers[j];
		} else {
			ath12k_warn(ar->ab,
				    "sta_max_pwr_table: skipping %pM — not associated on radio %u\n",
				    peers[j].peer_macaddr, radio_id);
		}
	}
	spin_unlock_bh(&ar->arsta_lock);

	if (!num_valid) {
		kfree(valid_peers);
		return 0;
	}

	ret = ath12k_wmi_send_peer_bulk_pwr_cmd(ar, valid_peers, num_valid);

	kfree(valid_peers);
	return ret;
}

const struct nla_policy
ath12k_power_table_policy[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_RADIO_INDEX]    = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_FREQ_BAND]      = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_IS_EXT]         = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_TARGET_TYPE]    = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_SUB_BAND]       = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_END_OF_UPDATE]  = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_LENGTH]         = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_DATA]           = { .type = NLA_BINARY,
							      .len = MAX_PWTAB_SIZE },
};

/**
 * ath12k_vendor_power_table - Set rate-to-power (R2P) table.
 * @wiphy: pointer to wiphy
 * @wdev: wireless wdev
 * @data: NL vendor data
 * @data_len: length of data
 *
 * Sends one sub-band slice of the rate-to-power table to firmware via
 * WMI_PDEV_SET_TGTR2P_TABLE_CMDID.  acfgtool loops over sub-bands and
 * sets end_of_update=1 on the last message for each frequency band.
 *
 * The RADIO_INDEX attribute selects the target pdev.
 * The DATA blob contains s8 power values in units of 0.25 dBm.
 */
static int ath12k_wmi_send_pdev_tgtr2p_cmd(struct ath12k *ar,
					    u32 freq_band, u32 sub_band,
					    u32 is_ext, u32 target_type,
					    u32 pwr_len, u32 end_of_update,
					    const int8_t *pwr_data)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_tgtr2p_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u32 pwr_tlv_len, total_len;
	u8 *ptr;
	int ret;

	pwr_tlv_len = TLV_HDR_SIZE + roundup(pwr_len, sizeof(u32));
	total_len = sizeof(*cmd) + pwr_tlv_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, total_len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_pdev_set_tgtr2p_table_cmd *)ptr;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_TGTR2P_TABLE_CMD_FIXED_PARAM) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id              = cpu_to_le32(ar->pdev->pdev_id);
	cmd->freq_band            = cpu_to_le32(freq_band);
	cmd->sub_band             = cpu_to_le32(sub_band);
	cmd->is_ext               = cpu_to_le32(is_ext);
	cmd->target_type          = cpu_to_le32(target_type);
	cmd->r2p_array_len        = cpu_to_le32(pwr_len);
	cmd->end_of_r2ptable_update = cpu_to_le32(end_of_update);

	ptr += sizeof(*cmd);

	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, pwr_len);
	ptr += TLV_HDR_SIZE;
	memcpy(ptr, pwr_data, pwr_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "power_table: sending WMI_PDEV_SET_TGTR2P_TABLE_CMDID pdev_id=%d "
		   "band=%u sub_band=%u len=%u eou=%u\n",
		   ar->pdev->pdev_id, freq_band, sub_band, pwr_len, end_of_update);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_TGTR2P_TABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_SET_TGTR2P_TABLE_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_vendor_power_table(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      const void *data,
			      int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_MAX + 1];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	const int8_t *pwr_data;
	u32 freq_band, sub_band, is_ext, target_type, end_of_update, pwr_len;
	u8 radio_id;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_POWER_TABLE_MAX,
		      data, data_len, ath12k_power_table_policy, NULL)) {
		ath12k_err(NULL, "Failed to parse power_table attributes\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_RADIO_INDEX] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_FREQ_BAND] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_IS_EXT] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_TARGET_TYPE] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_SUB_BAND] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_END_OF_UPDATE] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_LENGTH] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_DATA]) {
		ath12k_err(NULL, "power_table: missing required attributes\n");
		return -EINVAL;
	}

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "power_table: invalid radio_id %u\n", radio_id);
		return -EINVAL;
	}
	ar = &ah->radio[radio_id];

	freq_band      = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_FREQ_BAND]);
	is_ext         = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_IS_EXT]);
	target_type    = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_TARGET_TYPE]);
	sub_band       = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_SUB_BAND]);
	end_of_update  = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_END_OF_UPDATE]);
	pwr_len        = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_LENGTH]);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "power_table: radio_id=%u band=%u sub_band=%u is_ext=%u "
		   "target_type=0x%x pwr_len=%u end_of_update=%u\n",
		   radio_id, freq_band, sub_band, is_ext, target_type,
		   pwr_len, end_of_update);

	if (freq_band > 2) {
		ath12k_err(ar->ab, "power_table: invalid freq_band %u\n", freq_band);
		return -EINVAL;
	}

	if (!pwr_len || pwr_len > MAX_PWTAB_SIZE ||
	    nla_len(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_DATA]) < (int)pwr_len) {
		ath12k_err(ar->ab, "power_table: invalid pwr_len %u\n", pwr_len);
		return -EINVAL;
	}

	pwr_data = nla_data(tb[QCA_WLAN_VENDOR_ATTR_POWER_TABLE_DATA]);

	return ath12k_wmi_send_pdev_tgtr2p_cmd(ar, freq_band, sub_band,
						is_ext, target_type,
						pwr_len, end_of_update,
						pwr_data);
}

int ath12k_vendor_dcs_config_handler(struct wiphy *wihpy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	int ret, link_id = 0, cmd_type, dcs_enable;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DCS_MAX + 1];
	struct dcs_wlan_intr_user_cfg *cfg;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_DCS_MAX, data, data_len,
			ath12k_vendor_dcs_config_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "Invalid attribute in dcs_handler %d\n", ret);
		return ret;
	}

	if (wdev->valid_links) { /* MLO case */
		if (!tb[QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID])
			return -EINVAL;
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID]);
		if (!(wdev->valid_links & BIT(link_id)))
			return -ENOLINK;
	} else { /* NON-MLO case */
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID])
			return -EINVAL;
		link_id = 0;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE])
		return -EINVAL;

	cmd_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_DCS_ENABLE])
		return -EINVAL;

	dcs_enable = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_DCS_ENABLE]);

	if (dcs_enable & ~ATH12K_DCS_VALID_INTF_BITMAP) {
		ath12k_err(NULL, "Invalid DCS enable bitmap: 0x%x\n", dcs_enable);
		return -EINVAL;
	}

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	ath12k_mac_set_intf_detect(ar, dcs_enable);

	/* Store thresholds into wlan_intf_extn.dcs_cfg directly (hostapd manages defaults) */
	if (cmd_type == ATH12K_WLAN_INTR_CMD) {
		cfg = &ar->ar_extn.wlan_intf.dcs_cfg;

		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD])
			cfg->intr_detection_threshold =
				nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW])
			cfg->sample_size =
				nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY])
			cfg->phyerr_penalty =
				nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD])
			cfg->phyerr_threshold =
				nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD])
			cfg->radarerr_threshold =
				nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD])
			cfg->txerr_threshold =
				nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD])
			cfg->coch_intr_threshold =
				nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD]);
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU])
			cfg->user_max_cu =
				nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU]);
	}
	return 0;
}

int ath12k_get_dp_rx_scan_radio_stats_len(void)
{
	struct ath12k_dp_rx_scan_radio_stats stats;
	int total_size = 0;

	total_size += nla_total_size(sizeof(stats.rx_ok_bytes));
	total_size += nla_total_size(sizeof(stats.rx_err_bytes));
	total_size += nla_total_size(sizeof(stats.rx_ok_pkts));
	total_size += nla_total_size(sizeof(stats.rx_err_pkts));
	total_size += nla_total_size(sizeof(stats.rx_mgmt_pkts));
	total_size += nla_total_size(sizeof(stats.rx_ctrl_pkts));
	total_size += nla_total_size(sizeof(stats.rx_data_pkts));
	total_size = nla_total_size_nested(total_size);
	return total_size;
}

int ath12k_fill_rx_scan_radio_stats(
		struct sk_buff *vendor_event,
		struct ath12k_dp_rx_scan_radio_stats *rx_scan_radio_stats)
{
	struct nlattr *attr;

	if (!rx_scan_radio_stats)
		return -EINVAL;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_SCAN_STATS);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Device reo error");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_OK_PKTS,
			rx_scan_radio_stats->rx_ok_pkts)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_OK_PKTS);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_OK_BYTES,
			      rx_scan_radio_stats->rx_ok_bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_OK_BYTES);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_ERR_PKTS,
			rx_scan_radio_stats->rx_err_pkts)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_ERR_PKTS);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_ERR_BYTES,
			      rx_scan_radio_stats->rx_err_bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_ERR_BYTES);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_MGMT_PKTS,
			rx_scan_radio_stats->rx_mgmt_pkts)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_MGMT_PKTS);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_CTRL_PKTS,
			rx_scan_radio_stats->rx_ctrl_pkts)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_CTRL_PKTS);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_DATA_PKTS,
			rx_scan_radio_stats->rx_data_pkts)) {
		ath12k_err(NULL, "nla put failed: rx_scan_radio stats %d",
			   QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_DATA_PKTS);
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	nla_nest_end(vendor_event, attr);

	return 0;
}

int ath12k_get_cp_tx_scan_radio_stats_len(void)
{
	return nla_total_size_nested(6 * nla_total_size_64bit(sizeof(u64)));
}

int ath12k_fill_cp_tx_scan_radio_stats(struct sk_buff *vendor_event,
				       struct ath12k_mgmt_frame_stats *mgmt_stats)
{
	struct nlattr *attr;

	if (!mgmt_stats)
		return -EINVAL;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TELEMETRY_CP_TX_SCAN_RADIO_STATS);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: cp tx scan radio stats");
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_TX_OK_PKTS,
			      mgmt_stats->tx_cmpl_succ_pkts,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_INVALID) ||
	    nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_TX_OK_BYTES,
			      mgmt_stats->tx_cmpl_succ_bytes,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_INVALID) ||
	    nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_TX_RETRY_PKTS,
			      mgmt_stats->tx_cmpl_retry_pkts,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_INVALID) ||
	    nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_TX_RETRY_BYTES,
			      mgmt_stats->tx_cmpl_retry_bytes,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_INVALID) ||
	    nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_TX_ERR_PKTS,
			      mgmt_stats->tx_cmpl_err_pkts,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_INVALID) ||
	    nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_TX_ERR_BYTES,
			      mgmt_stats->tx_cmpl_err_bytes,
			      QCA_WLAN_VENDOR_ATTR_TX_MON_SCAN_STATS_INVALID)) {
		ath12k_err(NULL, "nla put failure: cp tx scan radio stats");
		nla_nest_cancel(vendor_event, attr);
		return -EMSGSIZE;
	}

	nla_nest_end(vendor_event, attr);

	return 0;
}

int ath12k_vendor_dcs_sim_handler(struct wiphy *wihpy,
				  struct wireless_dev *wdev,
				  const void *data,
				  int data_len)
{
	u16 intf_type_bitmap;
	u32 intf_bitmap = 0;
	int ret, link_id = 0;
	struct ath12k *ar;
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_MAX + 1];

	ath12k_err(NULL, "Inside ath12k_vendor_dcs_sim_handler");

	if (!wdev)
		return -EINVAL;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_DCS_SIM_MAX, data, data_len,
			ath12k_vendor_dcs_sim_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "Invalid attribute in dcs_sim handler %d\n", ret);
		return ret;
	}

	if (wdev->valid_links) { /* MLO case */
		if (!tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID])
			return -EINVAL;
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID]);
		if (!(wdev->valid_links & BIT(link_id)))
			return -ENOLINK;
	} else { /* NON-MLO case */
		if (tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID])
			return -EINVAL;
		link_id = 0;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_TYPE])
		return -EINVAL;

	intf_type_bitmap = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_TYPE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_INTERFERENCE_BITMAP])
		intf_bitmap = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_SIM_INTERFERENCE_BITMAP]);

	rcu_read_lock();
	ar = ath12k_get_ar_from_wdev(wdev, link_id);

	if (!ar) {
		ath12k_err(NULL, "Invalid ar from wdev\n");
		goto exit;
	}

	if (intf_type_bitmap == WMI_DCS_AWGN_INTF) {
		struct cfg80211_chan_def *chandef;

		arg.ar = ar;
		arg.chanctx_conf = NULL;
		ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
						    ath12k_mac_get_any_chanctx_conf_iter,
						    &arg);
		if (!arg.chanctx_conf) {
			ath12k_err(NULL,
				   "failed to get chanctx for dcs sim awgn\n");
			goto exit;
		}

		chandef = &arg.chanctx_conf->def;
		if (!chandef->chan ||
		    chandef->chan->band != NL80211_BAND_6GHZ) {
			ath12k_err(ar->ab,
				   "DCS SIM AWGN is supported only on 6 GHz (freq=%d band=%d)\n",
				   chandef->chan ? chandef->chan->center_freq : 0,
				   chandef->chan ? (int) chandef->chan->band : -1);
			goto exit;
		}

		spin_lock_bh(&ar->data_lock);
		ar->awgn_chandef = *chandef;
		ar->awgn_intf_handling_in_prog = true;
		ar->chan_bw_interference_bitmap = intf_bitmap;
		spin_unlock_bh(&ar->data_lock);
		ath12k_info(ar->ab,
			    "DCS SIM AWGN: pdev=%d freq=%d width=%d cf1=%d cf2=%d bitmap=0x%x\n",
			    ar->pdev->pdev_id,
			    chandef->chan ? chandef->chan->center_freq : 0,
			    chandef->width,
			    chandef->center_freq1,
			    chandef->center_freq2,
			    intf_bitmap);
		cfg80211_awgn_event(wihpy, chandef, GFP_ATOMIC, intf_bitmap);
		rcu_read_unlock();
		return 0;
	}

	ath12k_vendor_send_intf_notify_extn(ar, intf_type_bitmap, intf_bitmap);
	rcu_read_unlock();
	return 0;
exit:
	rcu_read_unlock();
	return -1;
}

/**
 * ath12k_vendor_is_valid_opclass_chan_extn - Validate channel for an opclass
 * @opclass: Operating class to validate against
 * @chan: Channel number to validate
 * @num_opclasses: Number of entries in @opclass_lst
 * @opclass_lst: List of operating classes
 * @chansize_lst: Number of channels per operating class in @channel_lists
 * @channel_lists: Per-opclass channel lists aligned with @opclass_lst
 *
 * Return: true if @chan belongs to @opclass, otherwise false.
 */
static bool
ath12k_vendor_is_valid_opclass_chan_extn(u8 opclass, u8 chan,
					 u8 num_opclasses,
					 u8 *opclass_lst,
					 u8 *chansize_lst,
					 u8 **channel_lists)
{
	int i;

	for (i = 0; i < num_opclasses; i++) {
		int j;

		if (opclass != opclass_lst[i])
			continue;

		for (j = 0; j < chansize_lst[i]; j++) {
			if (chan == channel_lists[i][j])
				return true;
		}
		return false;
	}

	return false;
}

static int
ath12k_vendor_parse_reg_params_chan_list_extn(struct nlattr *chan_list_attr,
					      u8 *chan_list,
					      size_t max_chan,
					      size_t *num_chan)
{
	struct nlattr *nl;
	int rem;
	size_t i = 0;

	if (!chan_list_attr || !chan_list || !num_chan || !max_chan)
		return -EINVAL;

	nla_for_each_nested(nl, chan_list_attr, rem) {
		if (i >= max_chan)
			return -E2BIG;

		if (nla_len(nl) < sizeof(u8))
			return -EINVAL;

		chan_list[i++] = nla_get_u8(nl);
	}

	if (!i)
		return -EINVAL;

	*num_chan = i;
	return 0;
}

static void ath12k_vendor_set_chan_disable_state_extn(
	struct ieee80211_channel *chan, bool is_disable)
{
	if (!chan)
		return;

	if (is_disable) {
		chan->flags |= IEEE80211_CHAN_DISABLED;
		chan->orig_flags |= IEEE80211_CHAN_DISABLED;
	} else {
		chan->flags &= ~IEEE80211_CHAN_DISABLED;
		chan->orig_flags &= ~IEEE80211_CHAN_DISABLED;
	}
}

static void ath12k_vendor_set_6ghz_chan_disable_state_extn(struct wiphy *wiphy,
							   int freq,
							   bool is_disable)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_6ghz_channel *chan_6g;
	int i, pwr_mode;

	if (!wiphy || !freq)
		return;

	sband = wiphy->bands[NL80211_BAND_6GHZ];
	if (!sband)
		return;

	for (pwr_mode = 0; pwr_mode < NL80211_REG_NUM_POWER_MODES; pwr_mode++) {
		chan_6g = sband->chan_6g[pwr_mode];
		if (!chan_6g || !chan_6g->channels || !chan_6g->n_channels)
			continue;

		for (i = 0; i < chan_6g->n_channels; i++) {
			struct ieee80211_channel *mode_chan;

			mode_chan = &chan_6g->channels[i];
			if (mode_chan->center_freq != freq)
				continue;

			ath12k_vendor_set_chan_disable_state_extn(mode_chan,
								  is_disable);
		}
	}
}

static int
ath12k_vendor_is_chan_disabled_extn(struct wiphy *wiphy,
				    enum nl80211_band band,
				    u8 chan_num)
{
	struct ieee80211_channel *chan;
	int freq;

	if (!wiphy || !chan_num)
		return 1;

	freq = ieee80211_channel_to_frequency(chan_num, band);
	if (!freq)
		return 1;

	chan = ieee80211_get_channel(wiphy, freq);
	if (!chan)
		return 1;

	return !!(chan->flags & IEEE80211_CHAN_DISABLED);
}

static int
ath12k_vendor_apply_disable_opclass_chan_extn(struct wiphy *wiphy,
					      enum nl80211_band band,
					      u8 chan_num,
					      bool is_disable)
{
	struct ieee80211_channel *chan;
	const struct ieee80211_reg_rule *reg_rule;
	int freq;

	if (!wiphy || !chan_num)
		return -EINVAL;

	freq = ieee80211_channel_to_frequency(chan_num, band);
	if (!freq)
		return -EINVAL;

	chan = ieee80211_get_channel(wiphy, freq);
	if (!chan)
		return -EINVAL;

	if (is_disable) {
		/* Keep user-disable state across regulatory reapply flows by
		 * updating both runtime and baseline flags.
		 */
		ath12k_vendor_set_chan_disable_state_extn(chan, is_disable);
		if (band == NL80211_BAND_6GHZ)
			ath12k_vendor_set_6ghz_chan_disable_state_extn(wiphy, freq,
								       is_disable);
		return 0;
	}

	if (!(chan->flags & IEEE80211_CHAN_DISABLED))
		return 0;

	reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(freq));
	if (IS_ERR_OR_NULL(reg_rule))
		return 0;

	ath12k_vendor_set_chan_disable_state_extn(chan, is_disable);
	if (band == NL80211_BAND_6GHZ)
		ath12k_vendor_set_6ghz_chan_disable_state_extn(wiphy, freq,
							       is_disable);
	return 0;
}

/**
 * ath12k_vendor_disable_opclass_chans_handler_extn - Handle disable_opclass_chans
 * @wiphy: wiphy pointer
 * @band: band used to resolve channel-to-frequency mapping
 * @tb: parsed REG_PARAMS vendor attributes
 * @opclass: operating class from vendor attributes
 *
 * Validates input attributes, verifies requested channels belong to @opclass,
 * applies disable/enable state per channel, and notifies regulatory change.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int
ath12k_vendor_disable_opclass_chans_handler_extn(struct wiphy *wiphy,
						 enum nl80211_band band,
						 struct nlattr **tb,
						 u8 opclass)
{
	u8 num_op = 0;
	u8 *op_lst = NULL, *chansize = NULL, **chan_lst = NULL;
	u8 disable = 0;
	u8 user_chan_list[256];
	size_t user_chan_count = 0, i;
	int ret_val;

	if (!tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_DISABLE] || !opclass ||
	    !tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CHAN_LIST])
		return -EINVAL;

	disable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_DISABLE]);
	if (disable != 0 && disable != 1)
		return -EINVAL;

	ret_val = ath12k_vendor_parse_reg_params_chan_list_extn(
		tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CHAN_LIST],
		user_chan_list, ARRAY_SIZE(user_chan_list), &user_chan_count);
	if (ret_val)
		return ret_val;

	ret_val = ath12k_get_opclasses_and_channels(NULL,
						    &num_op,
						    &op_lst,
						    &chansize,
						    &chan_lst,
						    band);
	if (ret_val) {
		ath12k_err(NULL,
			   "Failed to retrieve opclass list and channel\n");
		return ret_val;
	}

	for (i = 0; i < user_chan_count; i++) {
		ret_val = ath12k_vendor_is_valid_opclass_chan_extn(opclass,
								   user_chan_list[i],
								   num_op,
								   op_lst,
								   chansize, chan_lst);
		if (!ret_val) {
			ath12k_err(NULL,
				   "Invalid channel %u for opclass %u\n",
				   user_chan_list[i], opclass);
			ret_val = -EINVAL;
			goto clean;
		}

		ret_val = ath12k_vendor_apply_disable_opclass_chan_extn(wiphy, band,
									user_chan_list[i],
									disable);
		if (ret_val)
			goto clean;
	}

	ath12k_dbg(NULL, ATH12K_DBG_REG,
		   "opclass %u channels updated count=%zu disable=%u",
		   opclass, user_chan_count, disable);
	/* Re-notify current regulatory state so userspace refreshes
	 * channel list caches through normal REG_CHANGE handling.
	 */
	ath12k_vendor_notify_reg_change_extn(wiphy, "REG_PARAMS");
	ret_val = 0;

clean:
	ath12k_free_opclasses_and_channels(num_op, op_lst,
					   chansize, chan_lst);
	return ret_val;
}

static int ath12k_vendor_send_reg_params_update(struct wiphy *wiphy,
						enum nl80211_band band,
						u8 cmd, u8 opclass,
						u16 hw_value,
						int max_reg_power,
						u8 num_opclasses,
						u8 *opclass_lst,
						u8 *chansize_lst,
						u8 **channel_lists)
{
	struct sk_buff *skb;
	struct nlattr *nla_attr;
	int i, j, temp;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_NUM_OPCLASS) {
		if (nla_put_u8(skb,
			       QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_NUM_OPCLASS,
			       num_opclasses))
			goto fail;
		else
			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "Number of opclasses is %d",
				   num_opclasses);
	} else if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_CHAN_NUM) {
		if (nla_put_u8(skb,
			       QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_CHAN_NUM,
			       hw_value))
			goto fail;
		else
			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "hw_value is %d", hw_value);
	} else if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_TXPOWER) {
		if (nla_put_s32(skb,
				QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_TXPOWER,
				max_reg_power))
			goto fail;
		else
			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "max_reg_power is %d", max_reg_power);
	} else if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_OPCLASS_LIST) {
		temp = QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_OPCLASS_LIST;
		nla_attr = nla_nest_start(skb, temp);
		if (!nla_attr)
			goto fail;

		for (i = 0; i < num_opclasses; i++) {
			if (nla_put_u8(skb, i, opclass_lst[i]))
				goto fail;
			else
				ath12k_dbg(NULL, ATH12K_DBG_REG,
					   "opclasslist[%d] is %d",
					   i, opclass_lst[i]);
		}
		nla_nest_end(skb, nla_attr);
	}

	if (opclass) {
		int out_idx = 0;

		for (i = 0; i < num_opclasses; i++) {
			if (opclass == opclass_lst[i]) {
				j = i;
				break;
			}
		}

		if (i == num_opclasses)
			goto fail;

		temp = QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_OPCLASS_CHAN;
		nla_attr = nla_nest_start(skb, temp);
		if (!nla_attr)
			goto fail;

		for (i = 0; i < chansize_lst[j]; i++) {
			u8 chan = channel_lists[j][i];

			if (ath12k_vendor_is_chan_disabled_extn(wiphy, band, chan))
				continue;

			if (nla_put_u8(skb, out_idx, chan))
				goto fail;

			ath12k_dbg(NULL, ATH12K_DBG_REG,
				   "channellist[%d] is %d",
				   out_idx, chan);
			out_idx++;
		}
		nla_nest_end(skb, nla_attr);
	}

	return cfg80211_vendor_cmd_reply(skb);

fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

void ath12k_set_current_cac_timeout(struct ath12k *ar, s32 cac_timeout_ms)
{
	ar->ar_extn.cac_timeout_override_ms = cac_timeout_ms;
	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "CAC timeout is now %s (%d sec)\n",
		   (cac_timeout_ms == NO_CAC_OVERRIDE) ?
		   "default" : "overridden",
		   (cac_timeout_ms == NO_CAC_OVERRIDE) ?
		   NO_CAC_OVERRIDE : cac_timeout_ms / ATH12K_MSEC_PER_SEC);
}

void
ath12k_update_regd_cac_timeout_ext(struct ath12k *ar,
				   struct ieee80211_regdomain *regd)
{
	s32 cac_timeout_ms;
	int i;

	if (!ar || !regd)
		return;

	cac_timeout_ms = ar->ar_extn.cac_timeout_override_ms;

	if (cac_timeout_ms == NO_CAC_OVERRIDE)
		return;

	if (!ath12k_is_ar_range_valid(ar))
		return;

	for (i = 0; i < regd->n_reg_rules; i++) {
		struct ieee80211_reg_rule *rule = &regd->reg_rules[i];

		if (!(rule->flags & NL80211_RRF_DFS))
			continue;

		if (!ath12k_is_rule_in_radio(ar, rule))
			continue;

		rule->dfs_cac_ms = (u32)cac_timeout_ms;
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "regd update: DFS rule [%u-%u MHz] dfs_cac_ms -> %u ms\n",
			   KHZ_TO_MHZ(rule->freq_range.start_freq_khz),
			   KHZ_TO_MHZ(rule->freq_range.end_freq_khz),
			   (u32)cac_timeout_ms);
	}
}

int ath12k_vendor_reg_params_handler(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_MAX + 1];
	enum nl80211_band band;
	int ret_val = 0;
	u8 cmd = 0;
	u8 opclass = 0;
	u8 link_id, num_op = 0;
	u8 *op_lst  = NULL, *chansize = NULL, **chan_lst = NULL;
	u16 hw_value = 0;
	int max_reg_power = 0;
	int curr_reg_power;
	bool need_clean = false;

	if (!wdev || !data || !data_len) {
		ath12k_err(NULL, "Invalid input to REG PARAMS handler\n");
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_REG_PARAMS_MAX, data,
		      data_len, ath12k_reg_params_policy, NULL)) {
		ath12k_err(NULL, "Failed to parse REG PARAMS vendor attributes\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_LINKID]) {
		ath12k_err(NULL, "Missing mandatory Link_id attributes\n");
		return -EINVAL;
	}

	link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_LINKID]);
	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	if (!wdev->links[link_id].ap.chandef.chan) {
		ath12k_err(NULL, "No channel defined for link_id %u\n", link_id);
		return -EINVAL;
	}

	band = wdev->links[link_id].ap.chandef.chan->band;
	curr_reg_power = wdev->links[link_id].ap.chandef.chan->max_reg_power;

	if (tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CMD])
		cmd = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CMD]);

	if (tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS])
		opclass = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS]);
	else if (tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS_CHAN])
		opclass = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS_CHAN]);

	if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_DISABLE_OPCLASS_CHANS) {
		ret_val = ath12k_vendor_disable_opclass_chans_handler_extn(
			wiphy, band, tb, opclass);
		return ret_val;
	}

	if ((cmd >= QCA_WLAN_VENDOR_REG_PARAMS_NUM_OPCLASS &&
	     cmd <= QCA_WLAN_VENDOR_REG_PARAMS_OPCLASS_LIST) || opclass) {
		if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_CHAN_NUM) {
			hw_value = wdev->links[link_id].ap.chandef.chan->hw_value;
		}

		if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_TXPOWER) {
			max_reg_power = curr_reg_power;
		}

		if (cmd == QCA_WLAN_VENDOR_REG_PARAMS_NUM_OPCLASS ||
		    cmd == QCA_WLAN_VENDOR_REG_PARAMS_OPCLASS_LIST ||
		    opclass) {
			ret_val = ath12k_get_opclasses_and_channels(NULL,
								    &num_op,
								    &op_lst,
								    &chansize,
								    &chan_lst,
								    band);
			need_clean = true;
			if (ret_val) {
				ath12k_err(NULL,
					   "Failed to retrieve opclass list and channel\n");
				goto clean;
			}
		}

		ret_val = ath12k_vendor_send_reg_params_update(wiphy, band, cmd,
							       opclass, hw_value,
							       max_reg_power,
							       num_op, op_lst,
							       chansize,
							       chan_lst);

	} else {
		ret_val = -EINVAL;
	}

	if (need_clean)
		goto clean;
	return ret_val;

clean:
	ath12k_free_opclasses_and_channels(num_op, op_lst,
					   chansize, chan_lst);
	return ret_val;
}

/**
 * ath12k_vendor_tdma_schedule_config() - Configure TDMA schedule parameters
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: vendor command data
 * @data_len: vendor command data length
 *
 * This vendor command configures TDMA (Time Division Multiple Access)
 * scheduling parameters for a given radio. It parses the schedule type,
 * slot timing, BSSID, TSF start time, and optional EDCA parameters.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_vendor_tdma_schedule_config(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len)
{
	struct ieee80211_vif *vif;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_MAX + 1];
	struct ath12k_tdma_sched_info sched = {0};
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_MAX, data, data_len,
			ath12k_vendor_tdma_schedule_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Failed to parse TDMA schedule attributes: %d\n", ret);
		return ret;
	}

	 vif = wdev_to_ieee80211_vif(wdev);
        if (!vif) {
                ath12k_err(NULL, "vif is NULL for TDMA schedule config\n");
                return -EINVAL;
        }

        ahvif = ath12k_vif_to_ahvif(vif);
        if (!ahvif) {
                ath12k_err(NULL, "ahvif is NULL for TDMA schedule config\n");
                return -EINVAL;
        }
        /* VAP (vdev) interface is used as the entry point for user commands since
         * wlanconfig operates on netdev interfaces (wlanX).
         * The driver internally maps the given vdev to the corresponding pdev (radio)
         * and issues the required pdev-level WMI command to firmware.
         * This ensures correct radio selection and maintains proper driver abstraction.
         */
        arvif = &ahvif->deflink;
        if (!arvif || !arvif->ar) {
                ath12k_err(NULL, "arvif or ar is NULL for TDMA schedule config\n");
                return -EINVAL;
        }

        ar = arvif->ar;

	sched.pdev_id = ar->pdev->pdev_id;

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_TYPE])
		sched.sched_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_TYPE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_ID])
		sched.sched_id =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BSSID] &&
	    nla_len(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BSSID]) == ETH_ALEN)
		memcpy(sched.bssid,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BSSID]),
		       ETH_ALEN);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_START_TIME_TSF_LOW])
		sched.start_time_tsf_low =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_START_TIME_TSF_LOW]);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_NUM_BUSY_SLOTS])
		sched.num_busy_slots =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_NUM_BUSY_SLOTS]);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_DUR])
		sched.busy_slot_dur_ms =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_DUR]);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_INTVL])
		sched.busy_slot_intvl_ms =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_INTVL]);

	if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID])
		sched.edca_params_valid =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID]);

	if (sched.edca_params_valid) {
		if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AIFSN] &&
		    nla_len(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AIFSN]) ==
		    sizeof(u16) * WLAN_MAX_AC)
			memcpy(sched.aifsn,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AIFSN]),
			       sizeof(u16) * WLAN_MAX_AC);

		if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMIN] &&
		    nla_len(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMIN]) ==
		    sizeof(u16) * WLAN_MAX_AC)
			memcpy(sched.cwmin,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMIN]),
			       sizeof(u16) * WLAN_MAX_AC);

		if (tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMAX] &&
		    nla_len(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMAX]) ==
		    sizeof(u16) * WLAN_MAX_AC)
			memcpy(sched.cwmax,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMAX]),
			       sizeof(u16) * WLAN_MAX_AC);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "TDMA sched : pdev %u type %u id %u bssid %pM tsf %08x slots %u dur %u intvl %u edca %d\n",
		   sched.pdev_id, sched.sched_type, sched.sched_id, sched.bssid,
		   sched.start_time_tsf_low,
		   sched.num_busy_slots, sched.busy_slot_dur_ms,
		   sched.busy_slot_intvl_ms, sched.edca_params_valid);

	return ath12k_wmi_send_tdma_schedule_request(ar, &sched);;
}

const char *ath12k_phymode_str(enum wmi_phy_mode mode)
{
	switch (mode) {
	case MODE_11A:
		return "11a";
	case MODE_11G:
		return "11g";
	case MODE_11B:
		return "11b";
	case MODE_11GONLY:
		return "11gonly";
	case MODE_11NA_HT20:
		return "11na-ht20";
	case MODE_11NG_HT20:
		return "11ng-ht20";
	case MODE_11NA_HT40:
		return "11na-ht40";
	case MODE_11NG_HT40:
		return "11ng-ht40";
	case MODE_11AC_VHT20:
		return "11ac-vht20";
	case MODE_11AC_VHT40:
		return "11ac-vht40";
	case MODE_11AC_VHT80:
		return "11ac-vht80";
	case MODE_11AC_VHT160:
		return "11ac-vht160";
	case MODE_11AC_VHT80_80:
		return "11ac-vht80+80";
	case MODE_11AC_VHT20_2G:
		return "11ac-vht20-2g";
	case MODE_11AC_VHT40_2G:
		return "11ac-vht40-2g";
	case MODE_11AC_VHT80_2G:
		return "11ac-vht80-2g";
	case MODE_11AX_HE20:
		return "11ax-he20";
	case MODE_11AX_HE40:
		return "11ax-he40";
	case MODE_11AX_HE80:
		return "11ax-he80";
	case MODE_11AX_HE80_80:
		return "11ax-he80+80";
	case MODE_11AX_HE160:
		return "11ax-he160";
	case MODE_11AX_HE20_2G:
		return "11ax-he20-2g";
	case MODE_11AX_HE40_2G:
		return "11ax-he40-2g";
	case MODE_11AX_HE80_2G:
		return "11ax-he80-2g";
	case MODE_11BE_EHT20:
		return "11be-eht20";
	case MODE_11BE_EHT40:
		return "11be-eht40";
	case MODE_11BE_EHT80:
		return "11be-eht80";
	case MODE_11BE_EHT80_80:
		return "11be-eht80+80";
	case MODE_11BE_EHT160:
		return "11be-eht160";
	case MODE_11BE_EHT160_160:
		return "11be-eht160+160";
	case MODE_11BE_EHT320:
		return "11be-eht320";
	case MODE_11BE_EHT20_2G:
		return "11be-eht20-2g";
	case MODE_11BE_EHT40_2G:
		return "11be-eht40-2g";
	case MODE_11BN_UHR20:
		return "11bn-uhr20";
	case MODE_11BN_UHR40:
		return "11bn-uhr40";
	case MODE_11BN_UHR80:
		return "11bn-uhr80";
	case MODE_11BN_UHR80_80:
		return "11bn-uhr80+80";
	case MODE_11BN_UHR160:
		return "11bn-uhr160";
	case MODE_11BN_UHR160_160:
		return "11bn-uhr160+160";
	case MODE_11BN_UHR320:
		return "11bn-uhr320";
	case MODE_11BN_UHR20_2G:
		return "11bn-uhr20-2g";
	case MODE_11BN_UHR40_2G:
		return "11bn-uhr40-2g";
	case MODE_UNKNOWN:
		/* skip */
		break;
	}

	return "<unknown>";
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
/**
 * ath12k_vendor_mesh_peer_dump - Send mesh peer info as a binary vendor reply.
 *
 * @wiphy: wiphy pointer
 * @vif: virtual interface pointer
 * @peer_mld_mac: specific peer MAC to dump, or NULL for all peers
 *
 * Return: 0 on success, -ENOENT if peer not found, or negative error code.
 */
static int ath12k_vendor_mesh_peer_dump(struct wiphy *wiphy,
					struct ieee80211_vif *vif,
					const u8 *peer_mld_mac)
{
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	bool found = false;
	int num_peers = 0, idx = 0;
	int ret = 0;
	unsigned long now;
	size_t payload_size;
	struct ieee80211_mesh_peer_dump_entry *entries = NULL;
	struct sk_buff *skb;

	if (!vif) {
		ath12k_err(NULL, "mesh_peer_dump: vif is NULL\n");
		return -EINVAL;
	}

	sdata = vif_to_sdata(vif);

	if (!IEEE80211_EXTN_VAP_IS_MESH_MODE(sdata->wdev.vap_submode)) {
		ath12k_err(NULL, "mesh_peer_dump: not a mesh VAP\n");
		return -EINVAL;
	}

	lockdep_assert_wiphy(wiphy);

	/* Pass 1: count matching peers under rcu_read_lock. */
	rcu_read_lock();
	list_for_each_entry_rcu(sta, &sdata->local->sta_list, list) {
		if (sta->sdata != sdata)
			continue;
		if (!sta->sta.sta_extn.peer_info.is_mesh_peer)
			continue;
		if (peer_mld_mac && !ether_addr_equal(sta->sta.addr, peer_mld_mac))
			continue;
		num_peers++;
		if (peer_mld_mac)
			break;
	}
	rcu_read_unlock();

	payload_size = (size_t)num_peers * sizeof(*entries);
	if (payload_size) {
		entries = kvmalloc(payload_size, GFP_KERNEL);
		if (!entries) {
			ath12k_err(NULL, "mesh_peer_dump: failed to alloc %zu bytes\n",
				   payload_size);
			return -ENOMEM;
		}
	}

	/* Pass 2: snapshot jiffies once, then fill the entry array. */
	now = jiffies;
	rcu_read_lock();
	list_for_each_entry_rcu(sta, &sdata->local->sta_list, list) {
		struct mesh_peer_info *peer_info;
		struct ieee80211_mesh_peer_dump_entry *peer_entry;

		if (sta->sdata != sdata)
			continue;
		if (!sta->sta.sta_extn.peer_info.is_mesh_peer)
			continue;
		if (peer_mld_mac && !ether_addr_equal(sta->sta.addr, peer_mld_mac))
			continue;

		peer_info  = &sta->sta.sta_extn.peer_info;
		peer_entry = &entries[idx++];
		found = true;

		ether_addr_copy(peer_entry->mac, sta->sta.addr);
		peer_entry->aid                 = sta->sta.aid;
		peer_entry->is_mesh_peer        = (u8)peer_info->is_mesh_peer;
		peer_entry->phymode             = (u32)peer_info->mesh_peer_phymode;
		peer_entry->nss                 = peer_info->mesh_peer_nss;
		peer_entry->beacon_miss_count   = peer_info->beacon_miss_count;
		peer_entry->last_beacon_jiffies = (u64)peer_info->last_beacon_time;
		peer_entry->current_jiffies     = (u64)now;
		peer_entry->beacon_age_sec      = peer_info->last_beacon_time ?
						 (u64)((now - peer_info->last_beacon_time) / HZ)
						 : 0;
		peer_entry->intersection_done   = (u8)peer_info->intersection_done;
		peer_entry->intersect_attempts  = peer_info->intersect_attempts;
		peer_entry->intersect_successes = peer_info->intersect_successes;
		peer_entry->cap_update_failures  = peer_info->cap_update_failures;
		peer_entry->rsn_parse_failures   = peer_info->rsn_parse_failures;

		if (peer_mld_mac)
			break;
	}
	rcu_read_unlock();

	if (peer_mld_mac && !found) {
		ath12k_err(NULL, "mesh_peer_dump: peer %pM not found\n",
			   peer_mld_mac);
		kvfree(entries);
		return -ENOENT;
	}

	/* Pack entry array; skip PARAM_DATA when num_peers is zero. */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						   (payload_size ?
						    nla_total_size(payload_size) : 0) +
						   nla_total_size(sizeof(u32)));
	if (!skb) {
		ret = -ENOMEM;
		goto err;
	}

	if ((payload_size &&
	     nla_put(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
		     payload_size, entries))                            ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, num_peers)) {
		kfree_skb(skb);
		ret = -ENOBUFS;
		goto err;
	}

	kvfree(entries);
	return cfg80211_vendor_cmd_reply(skb);

err:
	ath12k_err(NULL, "mesh_peer_dump: failed to build reply skb, err: %d\n", ret);
	kvfree(entries);
	return ret;
}

int ath12k_vendor_mesh_params_config_extn(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_vif *vif = NULL;
	struct ieee80211_mesh_config_params_extn *mesh_params = NULL;
	int ret = -1;

	lockdep_assert_wiphy(wiphy);

	if (!params) {
		ath12k_err(NULL, "Mesh config params is NULL\n");
		return -EINVAL;
	}

	if (!params->data || !params->data_len) {
		ath12k_err(NULL, "Missing data for Mesh peer config\n");
		return -EINVAL;
	}

	if (params->data_len < sizeof(struct ieee80211_mesh_config_params_extn)) {
		ath12k_err(NULL, "Insufficient data length for mesh config\n");
		return -EINVAL;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "vif pointer is NULL\n");
		return -EINVAL;
	}

	mesh_params = (struct ieee80211_mesh_config_params_extn *)params->data;

	switch (mesh_params->cmd_type) {
	case ATH12K_EXTN_MESH_CONF_ADD_LOCAL_PEER:
		ret = ieee80211_vendor_mesh_add_local_peer(wiphy, vif, mesh_params);
		break;
	case ATH12K_EXTN_MESH_CONF_ALLOW_DATA:
		ret = ieee80211_vendor_mesh_authorize_peer(wiphy, vif, mesh_params->peer_mld_mac);
		break;
	case ATH12K_EXTN_MESH_CONF_DEL_LOCAL_PEER:
		ret = ieee80211_vendor_mesh_delete_local_peer(wiphy, vif, mesh_params->peer_mld_mac);
		break;
	case ATH12K_EXTN_MESH_CONF_SET_PEER_TIMEOUT_CNT:
		ret = ieee80211_vendor_mesh_set_peer_timeout_cnt(wiphy, vif,
								 mesh_params->mesh_peer_timeout_cnt);
		break;
	case ATH12K_EXTN_MESH_CONF_PEER_TIMEOUT_EN:
		ret = ieee80211_vendor_mesh_peer_timeout_en(wiphy, vif,
							    mesh_params->
							    peer_cleanup_timer_enable);
		break;
	case ATH12K_EXTN_MESH_CONF_PEER_DUMP:
		ret = ath12k_vendor_mesh_peer_dump(wiphy, vif,
						   is_zero_ether_addr(mesh_params->peer_mld_mac) ?
						   NULL : mesh_params->peer_mld_mac);
		break;
	default:
		ath12k_err(NULL, "Invalid cmd type: %d\n", mesh_params->cmd_type);
		return -EINVAL;
	}

	return ret;
}

/**
 * ath12k_vendor_sdwf_msduq_send_event - Build and send one SDWF MSDUQ NL event.
 * @ar: Radio instance.
 * @evt_data: Event payload.
 */
void
ath12k_vendor_sdwf_msduq_send_event(struct ath12k *ar,
				    const struct ath12k_sdwf_msduq_evt_data *evt_data)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_pdev_dp *dp_pdev;
	struct wireless_dev *wdev = NULL;
	struct sk_buff *skb;
	struct wiphy *wiphy;
	int buf_len;

	if (!ar || !ar->ah || !ar->ah->hw || !evt_data)
		return;

	wiphy = ar->ah->hw->wiphy;
	dp_pdev = &ar->dp;

	buf_len = NLMSG_HDRLEN;
	buf_len += nla_total_size(sizeof(u16));  /* HW_LINK_ID */
	buf_len += nla_total_size(ETH_ALEN);     /* MAC */
	if (evt_data->mlo)
		buf_len += nla_total_size(ETH_ALEN); /* MLD_MAC */
	buf_len += nla_total_size(sizeof(u8));   /* ID */
	buf_len += nla_total_size(sizeof(u8));   /* EVENT_TYPE */
	buf_len += nla_total_size(sizeof(u8));   /* SVC_ID */
	buf_len += nla_total_size(sizeof(u8));   /* SVC_TYPE */
	buf_len += nla_total_size(sizeof(u8));   /* SVC_PRIORITY */
	buf_len += nla_total_size(sizeof(u8));   /* TID */
	buf_len += nla_total_size(sizeof(u8));   /* AC */
	buf_len += nla_total_size(sizeof(u32));  /* MARK_METADATA */
	buf_len += nla_total_size(sizeof(u32));  /* SERVICE_INTERVAL */
	buf_len += nla_total_size(sizeof(u32));  /* BURST_SIZE */
	buf_len += nla_total_size(sizeof(u32));  /* DELAY_BOUND */
	buf_len += nla_total_size(sizeof(u32));  /* MIN_THROUGHPUT */

	rcu_read_lock();

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
						       evt_data->peer_id);
	if (dp_peer && dp_peer->vif)
		wdev = ieee80211_vif_to_wdev(dp_peer->vif);

	skb = cfg80211_vendor_event_alloc(wiphy, wdev, buf_len,
					  QCA_NL80211_VENDOR_SUBCMD_SDWF_PEER_MSDUQ_EVENT_INDEX,
					  GFP_ATOMIC);
	if (!skb) {
		ath12k_err(ar->ab, "failed to alloc skb for SDWF peer MSDUQ event\n");
		rcu_read_unlock();
		return;
	}

	if (nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_HW_LINK_ID,
			evt_data->hw_link_id) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MAC,
		    ETH_ALEN, evt_data->link_mac) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_ID,
		       evt_data->msduq_id) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_EVENT_TYPE,
		       QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_EVENT_ADD) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_ID,
		       evt_data->svc_id) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_TYPE,
		       evt_data->svc_type) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_PRIORITY,
		       evt_data->priority) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_TID,
		       evt_data->tid) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_AC,
		       evt_data->ac) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MARK_METADATA,
			evt_data->mark_metadata) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SERVICE_INTERVAL,
			evt_data->service_interval) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_BURST_SIZE,
			evt_data->burst_size) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_DELAY_BOUND,
			evt_data->delay_bound) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MIN_THROUGHPUT,
			evt_data->min_throughput)) {
		ath12k_err(ar->ab, "failed to fill SDWF peer MSDUQ event attrs\n");
		kfree_skb(skb);
		rcu_read_unlock();
		return;
	}

	/* MLD MAC is optional — only for MLO peers */
	if (evt_data->mlo) {
		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MLD_MAC,
			    ETH_ALEN, evt_data->mld_mac)) {
			ath12k_err(ar->ab, "failed to put MLD MAC in SDWF MSDUQ event\n");
			kfree_skb(skb);
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
	cfg80211_vendor_event(skb, GFP_ATOMIC);

	ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
		   "SDWF peer MSDUQ event: hw_link_id=%u mac=%pM mld_mac=%pM mlo=%u msduq_id=%u svc_id=%u svc_type=%u priority=%u tid=%u ac=%u mark=0x%x svc_interval=%u burst_size=%u delay_bound=%u min_tput=%u\n",
		   evt_data->hw_link_id, evt_data->link_mac, evt_data->mld_mac,
		   evt_data->mlo, evt_data->msduq_id, evt_data->svc_id,
		   evt_data->svc_type, evt_data->priority, evt_data->tid,
		   evt_data->ac, evt_data->mark_metadata,
		   evt_data->service_interval, evt_data->burst_size,
		   evt_data->delay_bound, evt_data->min_throughput);
}

#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

const struct nla_policy
ath12k_mapc_cotdma_policy[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_POLICY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_PRIMARY_AC] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_MAX_TXOP_DUR] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_MIN_TXOP_DUR] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_NBR_AP_PRIO] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_START] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_INTERVAL] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_END] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_LATENCY_THRESHOLD] = { .type = NLA_U16 },
};

const struct nla_policy
ath12k_mapc_peer_params_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAPC_PEER_MAC] = { .type = NLA_BINARY,
						       .len  = ETH_ALEN },
	[QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_TO] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_FROM] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = { .type = NLA_U8  },
};
EXPORT_SYMBOL(ath12k_mapc_peer_params_policy);

const struct nla_policy
ath12k_mapc_cotdma_txop_policy_attr_policy
		[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_TXOP_POLICY_PEER_MAC] = {
		.type = NLA_BINARY,
		.len  = ETH_ALEN,
	},
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_TXOP_POLICY_COTDMA_PARAMS] =
		NLA_POLICY_NESTED(ath12k_mapc_cotdma_policy),
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = { .type = NLA_U8 },
};
EXPORT_SYMBOL(ath12k_mapc_cotdma_txop_policy_attr_policy);

const struct nla_policy
ath12k_mapc_get_peer_params_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PEER_MAC] = {
		.type = NLA_BINARY, .len = ETH_ALEN },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = { .type = NLA_U8 },
};
EXPORT_SYMBOL(ath12k_mapc_get_peer_params_policy);

const struct nla_policy
ath12k_cotdma_e2e_entry_policy[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_CONFIG_MODE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_QMID] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_MAC] = { .type = NLA_BINARY,
								  .len  = ETH_ALEN },
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_BACKHAUL_MAC] = { .type = NLA_BINARY,
								  .len  = ETH_ALEN },
};

const struct nla_policy
ath12k_cotdma_e2e_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = { .type = NLA_U8  }
};
EXPORT_SYMBOL(ath12k_cotdma_e2e_config_policy);

static int ath12k_mapc_resolve_ar_arvif(struct wireless_dev *wdev,
					struct ath12k_vif *ahvif,
					u8 mlo_link_id,
					struct ath12k **ar_out,
					struct ath12k_link_vif **arvif_out)
{
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;

	if (mlo_link_id != 0xff) {
		ar = ath12k_get_ar_from_wdev(wdev, mlo_link_id);
		if (!ar)
			return -ENODEV;
		arvif = ath12k_get_arvif_from_link_id(ahvif, mlo_link_id);
		if (!arvif)
			return -ENODEV;
	} else {
		arvif = &ahvif->deflink;
		ar    = arvif->ar;
		if (!ar)
			return -ENODEV;
	}

	*ar_out    = ar;
	*arvif_out = arvif;
	return 0;
}

int ath12k_vendor_mapc_peer_params_extn(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct ath12k_wmi_peer_mapc_params_arg arg = {};
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	u8 peer_addr[ETH_ALEN];
	u8 mlo_link_id = 0xff;
	u32 q2q_apid_to = 0, q2q_apid_from = 0;
	int ret;
	struct ieee80211_sta *sta;
	struct ath12k_link_sta *arsta;

	if (WARN_ON(!vif))
		return -EINVAL;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL,
			   "MAPC Q2Q params: command applicable only for AP mode, vif type: %d\n",
			   vif->type);
		return -EOPNOTSUPP;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			data, data_len,
			ath12k_mapc_peer_params_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "MAPC vendor: failed to parse attrs: %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]) {
		mlo_link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
		if (mlo_link_id >= ATH12K_NUM_MAX_LINKS) {
			ath12k_err(NULL, "MAPC Q2Q params: invalid link_id %u\n",
				   mlo_link_id);
			return -EINVAL;
		}
	}

	ret = ath12k_mapc_resolve_ar_arvif(wdev, ath12k_vif_to_ahvif(vif),
					   mlo_link_id, &ar, &arvif);
	if (ret)
		return ret;

	ab = ar->ab;
	if (!test_bit(WMI_TLV_SERVICE_UHR_MAX_CTDMA_AP_PEERS_SUPPORT,
		      ab->wmi_ab.svc_map)) {
		ath12k_err(ab, "MAPC vendor: FW Co-TDMA svc bit absent, skipping\n");
		return -EOPNOTSUPP;
	}

	/* Peer MAC is mandatory */
	if (!tb[QCA_WLAN_VENDOR_ATTR_MAPC_PEER_MAC]) {
		ath12k_err(NULL, "MAPC vendor: missing peer MAC\n");
		return -EINVAL;
	}
	ether_addr_copy(peer_addr,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_MAPC_PEER_MAC]));

	if (tb[QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_TO])
		q2q_apid_to = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_TO]);
	if (tb[QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_FROM])
		q2q_apid_from = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_FROM]);

	ether_addr_copy(arg.peer_addr, peer_addr);
	arg.vdev_id                     = arvif->vdev_id;
	arg.param_set_mask              = ATH12K_WMI_MAPC_SET_Q2Q;
	arg.q2q_apid_to_neighbor_peer   = q2q_apid_to;
	arg.q2q_apid_from_neighbor_peer = q2q_apid_from;

	ret = ath12k_wmi_send_peer_set_mapc_params_cmd(ar, &arg);
	if (ret) {
		ath12k_err(ab,
			   "MAPC vendor: WMI cmd failed for %pM: %d\n",
			   arg.peer_addr, ret);
		return ret;
	}

	/* Cache Q2Q APID pair for GET_MAPC_PEER_PARAMS. */
	rcu_read_lock();
	sta = ieee80211_find_sta(vif, peer_addr);
	if (sta) {
		arsta = &ath12k_sta_to_ahsta(sta)->deflink;
		spin_lock_bh(&ar->data_lock);
		arsta->arsta_extn.mapc_cache.q2q_apid_to_neighbor_peer   = q2q_apid_to;
		arsta->arsta_extn.mapc_cache.q2q_apid_from_neighbor_peer = q2q_apid_from;
		arsta->arsta_extn.mapc_cache.param_sets_recvd++;
		spin_unlock_bh(&ar->data_lock);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "MAPC: peer %pM not found for Q2Q cache update\n",
			   peer_addr);
	}
	rcu_read_unlock();

	return 0;
}
EXPORT_SYMBOL(ath12k_vendor_mapc_peer_params_extn);

/*
 * ath12k_vendor_mapc_cotdma_txop_policy_extn -
 * QCA_NL80211_VENDOR_SUBCMD_MAPC_COTDMA_TXOP_POLICY.
 *
 * Sends WMI_PEER_SET_MAPC_PARAMS_CMDID carrying only the Co-TDMA TXOP
 * sharing policy fields for the named peer.  All other WMI fields (APIDs,
 * capability bitmap, channel params) are left at zero — FW preserves its
 * stored values for those fields across this partial update.
 *
 * This handler is intentionally self-contained and independent of the
 * QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_PARAMS handler.  It shares
 * ath12k_mapc_cotdma_policy[] for NLA parsing but has no code dependency
 * on that handler's TXOP path.  Removing TXOP config from
 * QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_PARAMS in the future does not
 * affect this handler.
 */
int
ath12k_vendor_mapc_cotdma_txop_policy_extn(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct ath12k_wmi_peer_mapc_params_arg arg = {};
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct nlattr *ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_POLICY_MAX + 1];
	u8 peer_addr[ETH_ALEN];
	u8 mlo_link_id = 0xff;
	u32 primary_ac = 0, max_txop = 0, min_txop = 0, nbr_prio = 0;
	u32 svc_start = 0, svc_interval = 0, svc_end = 0;
	u32 latency_threshold = 0;
	int ret;
	struct ieee80211_sta *sta;
	struct ath12k_link_sta *arsta;

	if (WARN_ON(!vif))
		return -EINVAL;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL,
			   "MAPC TXOP policy: command applicable only for AP mode, vif type: %d\n",
			   vif->type);
		return -EOPNOTSUPP;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			data, data_len,
			ath12k_mapc_cotdma_txop_policy_attr_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "MAPC TXOP policy: attr parse failed: %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]) {
		mlo_link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
		if (mlo_link_id >= ATH12K_NUM_MAX_LINKS) {
			ath12k_err(NULL, "MAPC TXOP policy: invalid link_id %u\n",
				   mlo_link_id);
			return -EINVAL;
		}
	}

	ret = ath12k_mapc_resolve_ar_arvif(wdev, ath12k_vif_to_ahvif(vif),
					   mlo_link_id, &ar, &arvif);
	if (ret)
		return ret;

	ab = ar->ab;
	if (!test_bit(WMI_TLV_SERVICE_UHR_MAX_CTDMA_AP_PEERS_SUPPORT,
		      ab->wmi_ab.svc_map)) {
		ath12k_err(ab, "MAPC TXOP policy: Co-TDMA svc bit absent, skipping\n");
		return -EOPNOTSUPP;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_TXOP_POLICY_PEER_MAC]) {
		ath12k_err(NULL, "MAPC TXOP policy: missing peer MAC\n");
		return -EINVAL;
	}
	ether_addr_copy(peer_addr,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_TXOP_POLICY_PEER_MAC]));

	if (!tb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_TXOP_POLICY_COTDMA_PARAMS]) {
		ath12k_err(NULL, "MAPC TXOP policy: missing COTDMA_PARAMS for %pM\n",
			   peer_addr);
		return -EINVAL;
	}

	ret = nla_parse_nested(ctb, QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_POLICY_MAX,
			       tb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_TXOP_POLICY_COTDMA_PARAMS],
			       ath12k_mapc_cotdma_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "MAPC TXOP policy: COTDMA_PARAMS parse failed for %pM: %d\n",
			   peer_addr, ret);
		return ret;
	}

	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_PRIMARY_AC])
		primary_ac = nla_get_u8(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_PRIMARY_AC]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_MAX_TXOP_DUR])
		max_txop =
			nla_get_u8(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_MAX_TXOP_DUR]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_MIN_TXOP_DUR])
		min_txop =
			nla_get_u8(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_MIN_TXOP_DUR]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_NBR_AP_PRIO])
		nbr_prio = nla_get_u8(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_NBR_AP_PRIO]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_START])
		svc_start =
			nla_get_u16(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_START]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_INTERVAL])
		svc_interval =
			nla_get_u16(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_INTERVAL]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_END])
		svc_end = nla_get_u16(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_SERVICE_END]);
	if (ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_LATENCY_THRESHOLD])
		latency_threshold =
			nla_get_u16(ctb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_LATENCY_THRESHOLD]);

	ether_addr_copy(arg.peer_addr, peer_addr);
	arg.vdev_id                = arvif->vdev_id;
	arg.param_set_mask         = ATH12K_WMI_MAPC_SET_TXOP;
	arg.primary_ac             = primary_ac;
	arg.max_shared_txop_dur_us = max_txop;
	arg.min_shared_txop_dur_us = min_txop;
	arg.nbr_ap_prio            = nbr_prio;
	arg.service_start_time     = svc_start;
	arg.service_interval       = svc_interval;
	arg.service_end_time               = svc_end;
	arg.critical_traffic_dur_thresh_us = latency_threshold;

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "MAPC TXOP policy: peer %pM vdev %d primary_ac=%u max_txop=%u min_txop=%u nbr_prio=%u svc_start=%u svc_interval=%u svc_end=%u latency_thresh=%u\n",
		   arg.peer_addr, arg.vdev_id,
		   arg.primary_ac, arg.max_shared_txop_dur_us, arg.min_shared_txop_dur_us,
		   arg.nbr_ap_prio, arg.service_start_time,
		   arg.service_interval, arg.service_end_time,
		   arg.critical_traffic_dur_thresh_us);

	ret = ath12k_wmi_send_peer_set_mapc_params_cmd(ar, &arg);
	if (ret) {
		ath12k_err(ab, "MAPC TXOP policy: WMI cmd failed for %pM: %d\n",
			   arg.peer_addr, ret);
		return ret;
	}

	/* Cache TXOP policy fields for GET_MAPC_PEER_PARAMS. */
	rcu_read_lock();
	sta = ieee80211_find_sta(vif, peer_addr);
	if (sta) {
		arsta = &ath12k_sta_to_ahsta(sta)->deflink;
		spin_lock_bh(&ar->data_lock);
		arsta->arsta_extn.mapc_cache.primary_ac = primary_ac;
		arsta->arsta_extn.mapc_cache.nbr_ap_prio = nbr_prio;
		arsta->arsta_extn.mapc_cache.service_start_time = svc_start;
		arsta->arsta_extn.mapc_cache.service_interval = svc_interval;
		arsta->arsta_extn.mapc_cache.service_end_time = svc_end;
		arsta->arsta_extn.mapc_cache.critical_traffic_dur_thresh_us =
			latency_threshold;
		arsta->arsta_extn.mapc_cache.max_shared_txop_dur_us = max_txop;
		arsta->arsta_extn.mapc_cache.min_shared_txop_dur_us = min_txop;
		arsta->arsta_extn.mapc_cache.param_sets_recvd++;
		spin_unlock_bh(&ar->data_lock);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "MAPC: peer %pM not found for TXOP cache update\n",
			   peer_addr);
	}
	rcu_read_unlock();

	return 0;
}
EXPORT_SYMBOL(ath12k_vendor_mapc_cotdma_txop_policy_extn);

static void mapc_e2e_cache_add(struct ath12k_mapc_peer_cache *cache,
			       const struct ath12k_cotdma_e2e_entry *entry)
{
	int i, slot = -1;

	for (i = 0; i < cache->e2e_entry_count; i++) {
		if (cache->e2e_entries[i].qmid == entry->qmid) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		slot = cache->e2e_entry_count++;

	cache->e2e_entries[slot].qmid         = entry->qmid;
	cache->e2e_entries[slot].config_mode  = entry->config_mode;
	cache->e2e_entries[slot].bsta_mac_valid = entry->bsta_mac_valid;
	if (entry->bsta_mac_valid)
		ether_addr_copy(cache->e2e_entries[slot].bsta_mac, entry->bsta_mac);
}

static void mapc_e2e_cache_remove(struct ath12k_mapc_peer_cache *cache,
				  u16 qmid)
{
	int i;

	for (i = 0; i < cache->e2e_entry_count; i++) {
		if (cache->e2e_entries[i].qmid == qmid) {
			cache->e2e_entries[i] =
				cache->e2e_entries[--cache->e2e_entry_count];
			break;
		}
	}
}

int
ath12k_vendor_cotdma_e2e_config_extn(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct nlattr *etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY_MAX + 1];
	struct nlattr *entry_attr;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct ath12k_cotdma_e2e_entry entry;
	struct ath12k_wmi_peer_mapc_params_arg arg = {};
	struct ieee80211_sta *sta;
	struct ath12k_link_sta *arsta;
	u8 parsed = 0;
	int ret, rem;
	u8 mlo_link_id = 0xff;

	if (WARN_ON(!vif))
		return -EINVAL;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL,
			   "MAPC E2E_CONFIG: command applicable only for AP mode, vif type: %d\n",
			   vif->type);
		return -EOPNOTSUPP;
	}

	if (!data || !data_len) {
		ath12k_err(NULL, "cotdma_e2e: data is NULL");
		return -EINVAL;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			data, data_len, ath12k_cotdma_e2e_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "cotdma_e2e: failed to parse top-level attrs: %d", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]) {
		mlo_link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
		if (mlo_link_id >= ATH12K_NUM_MAX_LINKS) {
			ath12k_err(NULL, "MAPC GET params: invalid link_id %u\n",
				   mlo_link_id);
			return -EINVAL;
		}
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY]) {
		ath12k_err(NULL, "cotdma_e2e: mandatory ENTRY attribute missing");
		return -EINVAL;
	}

	ret = ath12k_mapc_resolve_ar_arvif(wdev, ath12k_vif_to_ahvif(vif),
					   mlo_link_id, &ar, &arvif);
	if (ret)
		return ret;

	if (!test_bit(WMI_TLV_SERVICE_UHR_MAX_CTDMA_AP_PEERS_SUPPORT,
		      ar->ab->wmi_ab.svc_map)) {
		ath12k_err(ar->ab, "cotdma_e2e: Co-TDMA svc bit absent\n");
		return -EOPNOTSUPP;
	}

	nla_for_each_nested(entry_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY], rem) {
		memset(&arg, 0, sizeof(arg));
		ret = nla_parse_nested(etb,
				       QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY_MAX,
				       entry_attr,
				       ath12k_cotdma_e2e_entry_policy, NULL);
		if (ret) {
			ath12k_err(NULL, "cotdma_e2e: failed to parse entry: %d", ret);
			return ret;
		}

		if (!etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_CONFIG_MODE]   ||
		    !etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_QMID]          ||
		    !etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_MAC]) {
			ath12k_err(NULL, "cotdma_e2e: mandatory entry attribute missing");
			return -EINVAL;
		}

		memset(&entry, 0, sizeof(entry));

		entry.config_mode = nla_get_u8(
			etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_CONFIG_MODE]);

		if (entry.config_mode > 1) {
			ath12k_err(NULL, "cotdma_e2e: invalid config_mode %u\n",
				   entry.config_mode);
			return -EINVAL;
		}

		entry.qmid = nla_get_u16(
			etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_QMID]);
		nla_memcpy(entry.mapc_peer_mac,
			   etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_MAC],
			   ETH_ALEN);

		if (etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_BACKHAUL_MAC]) {
			entry.bsta_mac_valid = true;
			nla_memcpy(entry.bsta_mac,
				   etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_BACKHAUL_MAC],
				   ETH_ALEN);
		}

		/* Build WMI arg and send per-entry */
		ether_addr_copy(arg.peer_addr, entry.mapc_peer_mac);
		arg.vdev_id            = arvif->vdev_id;
		arg.param_set_mask     = ATH12K_WMI_MAPC_SET_E2E;
		arg.e2e_config_mode    = entry.config_mode;
		arg.e2e_qmid           = entry.qmid;
		arg.e2e_bsta_mac_valid = entry.bsta_mac_valid;
		if (entry.bsta_mac_valid)
			ether_addr_copy(arg.e2e_bsta_mac, entry.bsta_mac);

		/* Validate cache capacity before sending WMI (add path only) */
		if (entry.config_mode == 1) {
			rcu_read_lock();
			sta = ieee80211_find_sta(vif, entry.mapc_peer_mac);
			if (sta) {
				arsta = &ath12k_sta_to_ahsta(sta)->deflink;
				spin_lock_bh(&ar->data_lock);
				if (arsta->arsta_extn.mapc_cache.e2e_entry_count >=
				    ATH12K_MAX_COTDMA_E2E_QMIDS) {
					spin_unlock_bh(&ar->data_lock);
					rcu_read_unlock();
					ath12k_err(ar->ab,
						   "cotdma_e2e: peer %pM cache full, rejecting qmid=%u\n",
						   entry.mapc_peer_mac, entry.qmid);
					return -ENOSPC;
				}
				spin_unlock_bh(&ar->data_lock);
			}
			rcu_read_unlock();
		}

		ret = ath12k_wmi_send_peer_set_mapc_params_cmd(ar, &arg);
		if (ret) {
			ath12k_err(ar->ab,
				   "cotdma_e2e: WMI cmd failed for %pM qmid=%u: %d\n",
				   entry.mapc_peer_mac, entry.qmid, ret);
			return ret;
		}

		/* Update per-peer QMID list cache under data_lock */
		rcu_read_lock();
		sta = ieee80211_find_sta(vif, entry.mapc_peer_mac);
		if (sta) {
			struct ath12k_mapc_peer_cache *cache;

			arsta = &ath12k_sta_to_ahsta(sta)->deflink;
			cache = &arsta->arsta_extn.mapc_cache;
			spin_lock_bh(&ar->data_lock);
			if (entry.config_mode == 1)
				mapc_e2e_cache_add(cache, &entry);
			else
				mapc_e2e_cache_remove(cache, entry.qmid);
			spin_unlock_bh(&ar->data_lock);
		} else {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "cotdma_e2e: peer %pM not found for cache update\n",
				   entry.mapc_peer_mac);
		}
		rcu_read_unlock();

		parsed++;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_vendor_cotdma_e2e_config_extn);

/*
 * ath12k_vendor_mapc_peer_get_params_extn -
 * QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_GET_PARAMS.
 *
 * Returns all MAPC params for the named peer from the per-peer driver
 * cache (arsta->arsta_extn.mapc_cache).
 */
int
ath12k_vendor_mapc_peer_get_params_extn(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	u8 peer_addr[ETH_ALEN];
	u8 mlo_link_id = 0xff;
	struct sk_buff *reply_skb;
	struct ath12k_mapc_peer_cache cache = {};
	struct ieee80211_sta *sta;
	struct ath12k_link_sta *arsta;
	int ret;

	if (WARN_ON(!vif))
		return -EINVAL;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL,
			   "MAPC GET params: command applicable only for AP mode, vif type: %d\n",
			   vif->type);
		return -EOPNOTSUPP;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			data, data_len,
			ath12k_mapc_get_peer_params_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "MAPC GET PARAMS: attr parse failed: %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]) {
		mlo_link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
		if (mlo_link_id >= ATH12K_NUM_MAX_LINKS) {
			ath12k_err(NULL, "MAPC GET params: invalid link_id %u\n",
				   mlo_link_id);
			return -EINVAL;
		}
	}

	ret = ath12k_mapc_resolve_ar_arvif(wdev, ath12k_vif_to_ahvif(vif),
					   mlo_link_id, &ar, &arvif);
	if (ret)
		return ret;
	ab = ar->ab;

	if (!test_bit(WMI_TLV_SERVICE_UHR_MAX_CTDMA_AP_PEERS_SUPPORT,
		      ab->wmi_ab.svc_map)) {
		ath12k_err(ab, "MAPC GET PARAMS: Co-TDMA svc bit absent\n");
		return -EOPNOTSUPP;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PEER_MAC]) {
		ath12k_err(NULL, "MAPC GET PARAMS: missing peer MAC\n");
		return -EINVAL;
	}
	ether_addr_copy(peer_addr,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PEER_MAC]));

	/* Read per-peer cache under data_lock */
	rcu_read_lock();
	sta = ieee80211_find_sta(vif, peer_addr);
	if (!sta) {
		rcu_read_unlock();
		ath12k_err(ab, "MAPC GET PARAMS: peer %pM not found\n",
			   peer_addr);
		return -ENOENT;
	}
	arsta = &ath12k_sta_to_ahsta(sta)->deflink;
	spin_lock_bh(&ar->data_lock);
	cache = arsta->arsta_extn.mapc_cache;
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 512 +
			cache.e2e_entry_count * (nla_total_size(sizeof(u8)) +
						 nla_total_size(sizeof(u16)) +
						 nla_total_size(ETH_ALEN) * 2));
	if (!reply_skb)
		return -ENOMEM;

	if (nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PEER_MAC,
		    ETH_ALEN, peer_addr) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SETS_RECVD,
			cache.param_sets_recvd) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SCHEME_BMP,
			cache.scheme_enable_bitmap) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CAP_BITMAP,
			cache.mapc_capability_bitmap) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_APID_TO,
			cache.apid_to_neighbor_peer) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_APID_FROM,
			cache.apid_from_neighbor_peer) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_Q2Q_TO,
			cache.q2q_apid_to_neighbor_peer) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_Q2Q_FROM,
			cache.q2q_apid_from_neighbor_peer) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CH_WIDTH,
			cache.channel_width) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CCFS,
			cache.ccfs) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_BSS_COLOR,
			cache.bss_color) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_RX_TXOP,
			cache.rx_txop_return_support) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_DSB,
			cache.disable_subchannel_bitmap) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PRIMARY_AC,
			cache.primary_ac) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_NBR_PRIO,
			cache.nbr_ap_prio) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_START,
			cache.service_start_time) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_INTERVAL,
			cache.service_interval) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_END,
			cache.service_end_time) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_LATENCY_THRESHOLD,
			cache.critical_traffic_dur_thresh_us) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MAX_TXOP,
			cache.max_shared_txop_dur_us) ||
	    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MIN_TXOP,
			cache.min_shared_txop_dur_us)) {
		kfree_skb(reply_skb);
		return -EMSGSIZE;
	}

	if (cache.e2e_entry_count > 0) {
		u8 i;

		for (i = 0; i < cache.e2e_entry_count; i++) {
			const struct ath12k_cotdma_e2e_cache_entry *e =
				&cache.e2e_entries[i];
			struct nlattr *entry_nest;

			entry_nest = nla_nest_start(reply_skb,
					QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_E2E_ENTRIES);
			if (!entry_nest) {
				kfree_skb(reply_skb);
				return -EMSGSIZE;
			}
			if (nla_put_u8(reply_skb,
				       QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_CONFIG_MODE,
				       e->config_mode) ||
			    nla_put_u16(reply_skb,
					QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_QMID,
					e->qmid)) {
				nla_nest_cancel(reply_skb, entry_nest);
				kfree_skb(reply_skb);
				return -EMSGSIZE;
			}
			if (e->bsta_mac_valid &&
			    nla_put(reply_skb,
				    QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_BACKHAUL_MAC,
				    ETH_ALEN, e->bsta_mac)) {
				nla_nest_cancel(reply_skb, entry_nest);
				kfree_skb(reply_skb);
				return -EMSGSIZE;
			}
			nla_nest_end(reply_skb, entry_nest);
		}
	}

	return cfg80211_vendor_cmd_reply(reply_skb);
}
EXPORT_SYMBOL(ath12k_vendor_mapc_peer_get_params_extn);
