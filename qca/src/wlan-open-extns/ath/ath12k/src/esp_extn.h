/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_ESP_EXTN_H
#define ATH12K_ESP_EXTN_H

struct wmi_esp_estimation_event {
	__le32 pdev_id;
	__le32 ac_airtime_percentage;
} __packed;

#ifndef CPTCFG_QCN_EXTN

static inline void ath12k_esp_init_extn(struct ath12k *ar)
{
}

static inline void ath12k_esp_cleanup_extn(struct ath12k *ar)
{
}

static inline void
ath12k_wmi_esp_estimate_event_extn(struct ath12k_base *ab,
				   struct sk_buff *skb)
{
}

static inline int
ath12k_vendor_set_esp_params_extn(struct wiphy *wiphy,
				  struct nlattr **params,
				  struct wireless_dev *wdev)
{
	return -EOPNOTSUPP;
}

static inline void
ath12k_esp_airtime_update_work_extn(struct wiphy *wiphy,
				    struct wiphy_work *work)
{
}

static inline void
ath12k_vendor_send_esp_airtime_update_extn(struct ath12k *ar)
{
}

static inline void ath12k_reconfig_esp_params_extn(struct ath12k *ar)
{
}

#else

void ath12k_esp_init_extn(struct ath12k *ar);

void ath12k_esp_cleanup_extn(struct ath12k *ar);

void ath12k_wmi_esp_estimate_event_extn(struct ath12k_base *ab,
					struct sk_buff *skb);

int ath12k_vendor_set_esp_params_extn(struct wiphy *wiphy,
				      struct nlattr **params,
				      struct wireless_dev *wdev);

void ath12k_esp_airtime_update_work_extn(struct wiphy *wiphy,
					 struct wiphy_work *work);

void ath12k_vendor_send_esp_airtime_update_extn(struct ath12k *ar);


/**
 * ath12k_reconfig_esp_params_extn() - Re-apply cached ESP configuration
 * to FW.
 * @ar: pointer to the ath12k radio instance
 *
 * Reconfigures Estimated Service Parameter (ESP) related firmware settings
 * by re-sending the cached per-radio ESP parameters stored in
 * ar->ar_extn.esp as WMI PDEV parameters. This helper is intended to be
 * called when firmware state is reset and driver needs to restore
 * configuration after SSR recovery.
 *
 * Return: void
 */
void ath12k_reconfig_esp_params_extn(struct ath12k *ar);

#endif /* CPTCFG_QCN_EXTN */
#endif /* ATH12K_ESP_EXTN_H */
