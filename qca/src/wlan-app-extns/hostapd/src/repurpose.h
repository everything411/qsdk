// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef REPURPOSE_H
#define REPURPOSE_H

struct hostapd_config;
struct hostapd_bss_config;
struct hostapd_data;

enum repurpose_mode {
	REPURPOSE_11AC	= 1,
	REPURPOSE_11AX	= 2,
	REPURPOSE_11BE	= 3,

	/* Add new modes above and update MAX accordingly */
	REPURPOSE_MAX	= REPURPOSE_11BE,
};

#ifndef CONFIG_QCN_EXTN
static inline int
hostapd_config_check_bss_repurpose_mode_extn(const struct hostapd_config *conf,
					     const struct hostapd_bss_config *bss)
{
	return 0;
}

static inline bool
hostapd_is_repurpose_disabled_11ax_extn(const struct hostapd_bss_config *bss)
{
	return false;
}

static inline bool
hostapd_is_repurpose_disabled_11be_extn(const struct hostapd_bss_config *bss)
{
	return false;
}

static inline int
hostapd_drv_notify_link_repurpose_extn(struct hostapd_data *hapd, u8 link_id)
{
	return 0;
}

static inline u8
hostapd_get_repurposed_links_bitmap_extn(struct hostapd_data *hapd,
					 u16 *repurposed_links)
{
	if (repurposed_links)
		*repurposed_links = 0;

	return 0;
}

static inline int
hostapd_link_remove_repurposed_bss_extn(struct hostapd_data *hapd,
					u32 removal_type)
{
	return 0;
}

static inline struct hostapd_data *
hostapd_get_non_repurposed_link_of_mld_extn(struct hostapd_data *hapd)
{
	return hapd;
}

static inline int
hostapd_validate_mbssid_group_repurpose_mode_extn(struct hostapd_data *hapd)
{
	return 0;
}

static inline void
hostapd_set_repurpose_oper_chwidth_extn(struct hostapd_config *conf,
					enum oper_chan_width oper_chwidth)
{
}

static inline void
hostapd_get_oper_info_of_repurposed_bss_extn(struct hostapd_data *hapd,
					     enum oper_chan_width *oper_chwidth,
				             u8 *seg0, u8 *seg1)
{
}

static inline void
hostapd_get_csa_info_of_repurposed_bss_extn(struct hostapd_data *hapd,
						 u8 primary_channel,
						 int secondary_channel,
						 enum oper_chan_width *oper_chwidth,
						 u8 *seg0, u8 *seg1)
{
}

static inline bool
hostapd_config_check_repurpose_width_extn(struct hostapd_config *conf)
{
	return true;
}

static inline void
hostapd_repurpose_update_ht_capabilities_extn(struct hostapd_data *hapd,
					      struct ieee80211_ht_capabilities *cap)
{
}

static inline u8
hostapd_get_repurpose_width_extn(struct hostapd_data *hapd)
{
	return 0;
}

static inline bool
hostapd_repurpose_update_ht_operation_mode_extn(struct hostapd_data *hapd,
						le32 vht_capabilities_info,
						struct ieee80211_ht_operation *oper)
{
	return false;
}

static inline void
hostapd_repurpose_update_vht_capabilities_extn(struct hostapd_data *hapd,
					       u8 *chwidth,
					       struct ieee80211_vht_capabilities *cap)
{
}

static inline void
hostapd_repurpose_get_vht_legacy_chan_info_extn(struct hostapd_data *hapd,
						enum oper_chan_width *chwidth,
						u8 *seg0,
						u8 *seg1)
{
}
#else /* CONFIG_QCN_EXTN */
int
hostapd_config_check_bss_repurpose_mode_extn(const struct hostapd_config *conf,
					     const struct hostapd_bss_config *bss);

static inline bool
hostapd_is_valid_repurpose_mode_extn(int mode)
{
	return (mode >= REPURPOSE_11AC && mode <= REPURPOSE_MAX);
}

bool
hostapd_is_repurpose_disabled_11ax_extn(const struct hostapd_bss_config *bss);

bool
hostapd_is_repurpose_disabled_11be_extn(const struct hostapd_bss_config *bss);

int
hostapd_drv_notify_link_repurpose_extn(struct hostapd_data *hapd, u8 link_id);

u8 hostapd_get_repurposed_links_bitmap_extn(struct hostapd_data *hapd,
					    u16 *repurposed_links);

int
hostapd_link_remove_repurposed_bss_extn(struct hostapd_data *hapd,
					u32 removal_type);
int
hostapd_validate_mbssid_group_repurpose_mode_extn(struct hostapd_data *hapd);

struct hostapd_data *
hostapd_get_non_repurposed_link_of_mld_extn(struct hostapd_data *hapd);

void hostapd_set_repurpose_oper_chwidth_extn(struct hostapd_config *conf,
					     enum oper_chan_width oper_chwidth);

void hostapd_get_oper_info_of_repurposed_bss_extn(
			struct hostapd_data *hapd,
			enum oper_chan_width *oper_chwidth,
			u8 *seg0,
			u8 *seg1);
void hostapd_get_csa_info_of_repurposed_bss_extn(
				struct hostapd_data *hapd,
				u8 primary_channel,
				int secondary_channel,
				enum oper_chan_width *oper_chwidth,
				u8 *seg0,
				u8 *seg1);

bool hostapd_config_check_repurpose_width_extn(struct hostapd_config *conf);
void
hostapd_repurpose_update_ht_capabilities_extn(struct hostapd_data *hapd,
					      struct ieee80211_ht_capabilities *cap);
u8 hostapd_get_repurpose_width_extn(struct hostapd_data *hapd);
bool
hostapd_repurpose_update_ht_operation_mode_extn(struct hostapd_data *hapd,
						le32 vht_capabilities_info,
						struct ieee80211_ht_operation *oper);
void
hostapd_repurpose_update_vht_capabilities_extn(struct hostapd_data *hapd,
					       u8 *chwidth,
					       struct ieee80211_vht_capabilities *cap);
void
hostapd_repurpose_get_vht_legacy_chan_info_extn(struct hostapd_data *hapd,
						enum oper_chan_width *chwidth,
						u8 *seg0,
						u8 *seg1);

#endif /* CONFIG_QCN_EXTN */
#endif /* REPURPOSE_H */
