/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include "../core.h"
#include "sa_api.h"
#include "../debug.h"

struct ath12k_smart_ant_ops __rcu *g_sa_ops;
static DEFINE_MUTEX(sa_ops_lock);

bool ath12k_smart_ant_is_enable(struct ath12k_base *ab)
{
	struct sa_base_extn *sa_base;

	if (!ab)
		return false;
	sa_base = &ab->ath12k_base_extn.sa_base;

	return sa_base->sa_enable;
}

int ath12k_vendor_get_wiphy_sa_configs(struct wiphy *wiphy,
				       struct ath12k_wifi_generic_params *params,
				       u32 *value)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = NULL;
	u32 configs[4] = {0};
	int ret;

	lockdep_assert_wiphy(wiphy);

	if (!params || !value || !params->data) {
		ath12k_err(NULL, "Invalid Parameters\n");
		return -EINVAL;
	}
	if (params->radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index\n");
		return -EINVAL;
	}
	ar = &ah->radio[params->radio_idx];

	configs[0] = params->value;
	configs[1] = get_unaligned_le32(params->data);
	configs[2] = params->length;

	ret = sa_core_get_configs(ar, configs);
	if (ret) {
		ath12k_err(ar->ab, "Operation failed; %d\n", ret);
		return -EOPNOTSUPP;
	}

	*value = configs[3];

	return 0;
}

int ath12k_vendor_set_wiphy_sa_configs(struct wiphy *wiphy,
				       struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = NULL;
	u32 configs[4] = {0};

	lockdep_assert_wiphy(wiphy);

	if (!params || !params->data) {
		ath12k_err(NULL, "Invalid Parameters\n");
		return -EINVAL;
	}
	if (params->radio_idx >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio index\n");
		return -EINVAL;
	}
	ar = &ah->radio[params->radio_idx];

	configs[0] = params->value;
	configs[1] = get_unaligned_le32(params->data);
	configs[2] = params->length;
	configs[3] = params->flags;

	return sa_core_set_configs(ar, configs);
}

bool ath12k_smart_ant_enable(struct ath12k_base *ab)
{
	u8 wmi_sa_sw, wmi_sa_hw;
	u8 en_sa = 0, sa_sw_test = 0;
	struct sa_base_extn *sa_base;

	if (!ab)
		return false;

	sa_base = &ab->ath12k_base_extn.sa_base;
	wmi_sa_sw =
		test_bit(WMI_TLV_SERVICE_SMART_ANTENNA_SW_SUPPORT, ab->wmi_ab.svc_map);
	wmi_sa_hw =
		test_bit(WMI_TLV_SERVICE_SMART_ANTENNA_HW_SUPPORT, ab->wmi_ab.svc_map);
	sa_get_cfg_flags(ab, &en_sa, &sa_sw_test);

	if ((en_sa && wmi_sa_sw && wmi_sa_hw) || (sa_sw_test && wmi_sa_sw)) {
		sa_base->sa_enable = true;
		ath12k_info(ab, "SA enabled; Host: en:%d test:%d  FW: sw:%d "
			    "hw:%d\n", en_sa, sa_sw_test, wmi_sa_sw, wmi_sa_hw);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_ANY,
			   "SA disabled; Host: en:%d test:%d FW: sw:%d hw:%d\n",
			   en_sa, sa_sw_test, wmi_sa_sw, wmi_sa_hw);
		sa_base->sa_enable = false;
	}
	sa_base->sa_support = en_sa;
	sa_base->sa_sw_test = sa_sw_test;

	return sa_base->sa_enable;
}

int ath12k_register_smart_ant_ops(struct ath12k_smart_ant_ops *ops)
{
	if (!ops)
		return -EINVAL;

	mutex_lock(&sa_ops_lock);
	if (rcu_access_pointer(g_sa_ops)) {
		mutex_unlock(&sa_ops_lock);
		return -EBUSY;
	}

	rcu_assign_pointer(g_sa_ops, ops);
	mutex_unlock(&sa_ops_lock);

	return 0;
}
EXPORT_SYMBOL(ath12k_register_smart_ant_ops);

int ath12k_unregister_smart_ant_ops(void)
{
	mutex_lock(&sa_ops_lock);
	rcu_assign_pointer(g_sa_ops, NULL);
	mutex_unlock(&sa_ops_lock);

	/*
	 * Wait for all in-flight RCU read-side critical sections (i.e. any
	 * ongoing sa_core callbacks that are currently executing code inside
	 * the sa_test module) to complete before returning.  Once this call
	 * returns the caller's module_exit() will proceed and the kernel will
	 * unmap the module text, so it is essential that no CPU is still
	 * executing a function pointer that lives in that module.
	 */
	synchronize_rcu();

	return 0;
}
EXPORT_SYMBOL(ath12k_unregister_smart_ant_ops);

int ath12k_smart_ant_api_channel_change(struct ath12k *ar, bool re_init)
{
	struct ath12k_link_vif *arvif, *sa_arvif = NULL;
	u8 init_flag = SA_NEW_RECONFIG;
	int ret;

	if (!ar)
		return -EINVAL;

	if (!ath12k_smart_ant_is_enable(ar->ab))
		return 0;

	if (!re_init)
		init_flag = SA_SSR_CONFIG;
	/*
	 * Find the first started AP or STA link vif on this radio.
	 * sa_core_init() uses the arvif only to read vdev_type, chanctx,
	 * vdev_id, and vif->addr, all of which are stable under the wiphy
	 * lock that the caller holds.
	 */
	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->is_started || !arvif->ahvif)
			continue;
		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP ||
		    arvif->ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
			sa_arvif = arvif;
			break;
		}
	}
	spin_unlock_bh(&ar->data_lock);

	if (!sa_arvif) {
		ath12k_dbg(ar->ab, ATH12K_DBG_ANY,
			   "SA channel change: no suitable vif found\n");
		return 0;
	}

	ret = sa_core_deinit(ar, init_flag);
	if (ret) {
		ath12k_dbg(ar->ab, ATH12K_DBG_ANY,
			   "SA channel change: deinit failed (%d)\n", ret);
		return ret;
	}

	ret = sa_core_init(ar, sa_arvif, init_flag);
	if (ret) {
		ath12k_dbg(ar->ab, ATH12K_DBG_ANY,
			   "SA channel change: init failed (%d)\n", ret);
		return ret;
	}

	/* Doesn't need to handle STA connection explicitly SSR enforces
	 * STAs do disconnection.
	 */
	if (re_init)
		sa_core_notify_all_peers_connected(ar);

	return 0;
}
