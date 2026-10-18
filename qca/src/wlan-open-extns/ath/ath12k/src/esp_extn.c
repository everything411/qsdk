// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/netlink.h>
#include <net/mac80211.h>
#include <net/genetlink.h>
#include <net/cfg80211.h>
#include "../core.h"
#include "../wmi.h"
#include "../mac.h"
#include "../debug.h"
#include "../vendor.h"
#include "ath12k_cmn_extn.h"
#include "esp_extn.h"

/* ESP (Estimated Service Parameters) - 802.11mc */

#define ESP_DEFAULT_PERIODICITY	5

void ath12k_esp_init_extn(struct ath12k *ar)
{
	/* Initialize ESP parameters */
	memset(&ar->ar_extn.esp, 0, sizeof(ar->ar_extn.esp));
	wiphy_work_init(&ar->ar_extn.esp.airtime_update_work,
			ath12k_esp_airtime_update_work_extn);
}

void ath12k_esp_cleanup_extn(struct ath12k *ar)
{
	if (!ar)
		return;

	wiphy_work_cancel(ar->ah->hw->wiphy,
			  &ar->ar_extn.esp.airtime_update_work);
}

void ath12k_wmi_esp_estimate_event_extn(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	const struct wmi_esp_estimation_event *ev;
	struct ath12k *ar;
	const void **tb;
	u32 pdev_id;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "ESP: Failed to parse TLV: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_ESP_ESTIMATE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "ESP: Failed to fetch ESP air time estimate event\n");
		kfree(tb);
		return;
	}

	pdev_id = le32_to_cpu(ev->pdev_id);
	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "ESP: Air time estimate event for invalid pdev %d\n",
			   pdev_id);
		goto unlock;
	}

	ar->ar_extn.esp.fw_esp_air_time = le32_to_cpu(ev->ac_airtime_percentage);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "ESP: Air time estimate event - pdev:%d, airtime: %u\n",
		   pdev_id, ar->ar_extn.esp.fw_esp_air_time);

	ath12k_vendor_send_esp_airtime_update_extn(ar);

unlock:
	rcu_read_unlock();
	kfree(tb);
}

int ath12k_vendor_set_esp_params_extn(struct wiphy *wiphy,
				      struct nlattr **params,
				      struct wireless_dev *wdev)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_MAX + 1];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_hw *ah = hw->priv;
	u8 link_id = INVALID_LINK_ID;
	struct ieee80211_vif *vif;
	struct nlattr *esp_params;
	struct ath12k_vif *ahvif;
	struct ath12k *ar;
	u8 value;
	int ret;

	if (!params[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS])
		return 0;

	esp_params = params[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS];

	if (params[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		link_id = nla_get_u8(params[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);

	lockdep_assert_wiphy(wiphy);

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "ESP: Invalid VIF\n");
		return -EINVAL;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_err(NULL, "ESP: Invalid ahvif\n");
		return -EINVAL;
	}

	ah = ahvif->ah;
	if (ah && link_id == INVALID_LINK_ID) {
		arvif = &ahvif->deflink;
	} else {
		if (link_id < ATH12K_NUM_MAX_LINKS)
			arvif = ahvif->link[link_id];
	}

	if (!arvif) {
		ath12k_err(NULL, "ESP: Invalid arvif for link_id %d\n", link_id);
		return -EINVAL;
	}

	ar = arvif->ar;
	if (!ar) {
		ath12k_err(NULL, "ESP: No radio present\n");
		return -EINVAL;
	}

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_MAX,
			       esp_params, NULL, NULL);
	if (ret) {
		ath12k_err(ar->ab, "ESP: Failed to parse nested attributes: %d\n", ret);
		return ret;
	}

	/* Handle ESP Enable/Disable */
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_ENABLE]) {
		value = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_ENABLE]);
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "ESP: %s ESP functionality\n",
			   value ? "Enabling" : "Disabling");

		if (!value) {
			/* Disable ESP - clear all parameters */
			ar->ar_extn.esp.air_time_fraction = 0;
			ar->ar_extn.esp.ppdu_duration = 0;
			ar->ar_extn.esp.ba_window = 0;
			ar->ar_extn.esp.periodicity = 0;
		} else {
			/* Setting default periodicity */
			ar->ar_extn.esp.periodicity = ESP_DEFAULT_PERIODICITY;
		}

		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "ESP: Setting periodicity to %u\n",
			   ar->ar_extn.esp.periodicity);

		/* ESP indication period doesn't need service check to become
		 * compatible with legacy firmware.
		 */
		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_INDICATION_PERIOD,
						ar->ar_extn.esp.periodicity,
						ar->pdev->pdev_id);
		if (ret)
			ath12k_err(ar->ab,
				   "ESP: Failed to set periodicity: %d\n", ret);

		return ret;
	}

	if (!test_bit(WMI_TLV_SERVICE_ESP_SUPPORT, ar->ab->wmi_ab.svc_map)) {
		ath12k_err(ar->ab, "ESP support is not available\n");
		return -EOPNOTSUPP;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE]) {
		value = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE]);
		if (value > 255) {
			ath12k_err(ar->ab, "ESP: Invalid airtime %u (max 255)\n",
				   value);
			return -EINVAL;
		}
		ar->ar_extn.esp.air_time_fraction = value;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "ESP: Setting airtime fraction to %u\n", value);

		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_AIRTIME_FRACTION,
						value, ar->pdev->pdev_id);
		if (ret) {
			ath12k_err(ar->ab,
				   "ESP: Failed to set airtime fraction: %d\n", ret);
			return ret;
		}
	} else if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BE]) {
		value = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BE]);
		if (value > 255) {
			ath12k_err(ar->ab, "ESP: Invalid PPDU duration %u (max 255)\n",
				   value);
			return -EINVAL;
		}
		ar->ar_extn.esp.ppdu_duration = value;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "ESP: Setting PPDU duration to %u\n", value);

		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_PPDU_DURATION,
						value, ar->pdev->pdev_id);
		if (ret) {
			ath12k_err(ar->ab,
				   "ESP: Failed to set PPDU duration: %d\n", ret);
			return ret;
		}
	} else if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BE]) {
		value = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BE]);
		if (value > 7) {
			ath12k_err(ar->ab,
				   "ESP: Invalid BA window %u (max 7, 3 bits)\n",
				   value);
			return -EINVAL;
		}
		ar->ar_extn.esp.ba_window = value;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "ESP: Setting BA window to %u\n", value);

		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_BA_WINDOW,
						value, ar->pdev->pdev_id);
		if (ret) {
			ath12k_err(ar->ab,
				   "ESP: Failed to set BA window: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

void ath12k_esp_airtime_update_work_extn(struct wiphy *wiphy,
					 struct wiphy_work *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 ar_extn.esp.airtime_update_work);
	struct ath12k_link_vif *tmp_arvif = NULL, *arvif;
	struct sk_buff *vendor_event;
	struct wireless_dev *wdev;
	struct nlattr *esp_attr;
	int vendor_buffer_len;
	u8 airtime_be;
	int ret;

	if (list_empty(&ar->arvifs)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "arvifs list is empty\n");
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "ESP airtime update work handler\n");

	list_for_each_entry(arvif, &ar->arvifs, list) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "Checking arvif: is_started=%d ahvif=%p\n",
			   arvif->is_started, arvif->ahvif);
		if (!tmp_arvif && arvif->is_started &&
		    arvif->link_id < IEEE80211_MLD_MAX_NUM_LINKS &&
		    arvif->ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR) {
			tmp_arvif = arvif;
			break;
		}
	}

	if (!tmp_arvif || !tmp_arvif->ahvif) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "No valid arvif found for ESP update\n");
		return;
	}

	wdev = ieee80211_vif_to_wdev(tmp_arvif->ahvif->vif);
	if (!wdev) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "wdev is NULL for ESP update\n");
		return;
	}

	/* vendor_buffer_len - Outer header + link id attr + Inner nest attr */
	vendor_buffer_len = nla_total_size(0) + nla_total_size(sizeof(u8)) +
			    nla_total_size(sizeof(u32));
	vendor_event =
		cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, wdev, vendor_buffer_len,
					    QCA_NL80211_VENDOR_SUBCMD_ESP_ESTIMATE_INDEX,
					    GFP_KERNEL);
	if (!vendor_event) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "SKB alloc failed for ESP airtime update evt\n");
		return;
	}

	if (wdev->valid_links) {
		ret = nla_put_u8(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID,
				 tmp_arvif->link_id);
		if (ret) {
			ath12k_warn(ar->ab, "Failed to put link id\n");
			goto out;
		}
	}

	esp_attr = nla_nest_start(vendor_event, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS);
	if (!esp_attr) {
		ath12k_err(ar->ab, "NLA nest failure: QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS");
		goto out;
	}

	/*
	 * Bits 0 to 7 represents air time of BE AC. Strip that and
	 * scale it to 255.
	 */
	airtime_be = ar->ar_extn.esp.fw_esp_air_time & 0xFF;
	airtime_be = (airtime_be * 255) / 100;

	ret = nla_put_u32(vendor_event,
			  QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE,
			  airtime_be);
	if (ret) {
		ath12k_warn(ar->ab, "Error(%d): Failed to put ESP airtime.\n",
			    ret);
		goto out;
	}

	nla_nest_end(vendor_event, esp_attr);

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "Send ESP airtime update: airtime %u for link id %d\n",
		   ar->ar_extn.esp.fw_esp_air_time, tmp_arvif->link_id);
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	return;

out:
	ath12k_err(ar->ab, "Error sending airtime update vendor event");
	kfree_skb(vendor_event);
}

void ath12k_vendor_send_esp_airtime_update_extn(struct ath12k *ar)
{
	if (!ar) {
		ath12k_err(NULL, "ESP: Invalid ar structure for airtime update\n");
		return;
	}

	wiphy_work_queue(ar->ah->hw->wiphy, &ar->ar_extn.esp.airtime_update_work);
}

void ath12k_reconfig_esp_params_extn(struct ath12k *ar)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ar_extn.esp.periodicity) {
		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_INDICATION_PERIOD,
						ar->ar_extn.esp.periodicity,
						ar->pdev->pdev_id);
		if (ret)
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				    "ESP: failed to set periodicity %u on pdev %d: %d\n",
				    ar->ar_extn.esp.periodicity, ar->pdev->pdev_id, ret);
	}

	if (!test_bit(WMI_TLV_SERVICE_ESP_SUPPORT, ar->ab->wmi_ab.svc_map))
		return;

	if (ar->ar_extn.esp.air_time_fraction) {
		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_AIRTIME_FRACTION,
						ar->ar_extn.esp.air_time_fraction,
						ar->pdev->pdev_id);
		if (ret)
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				    "ESP: failed to set airtime fraction %u on pdev %d: %d\n",
				    ar->ar_extn.esp.air_time_fraction, ar->pdev->pdev_id, ret);
	}

	if (ar->ar_extn.esp.ppdu_duration) {
		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_PPDU_DURATION,
						ar->ar_extn.esp.ppdu_duration,
						ar->pdev->pdev_id);
		if (ret)
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				    "ESP: failed to set PPDU duration %u on pdev %d: %d\n",
				    ar->ar_extn.esp.ppdu_duration, ar->pdev->pdev_id, ret);
	}

	if (ar->ar_extn.esp.ba_window) {
		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_ESP_BA_WINDOW,
						ar->ar_extn.esp.ba_window,
						ar->pdev->pdev_id);
		if (ret)
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				    "ESP: failed to set BA window %u on pdev %d: %d\n",
				    ar->ar_extn.esp.ba_window, ar->pdev->pdev_id, ret);
	}
}
