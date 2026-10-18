/*
 * Airtime Fairness offload feature wrappers and apis.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.*
 */

#ifndef ATF_OFFLOAD_H
#define ATF_OFFLOAD_H

#include <sys/un.h>
#include "utils/includes.h"
#include "utils/common.h"
#include "utils/list.h"

#define ATF_INVALID_GROUP_ID 0xFF
#define ATF_MAX_SSID_GROUP 16
#define WLAN_SSID_MAX 16
#define WLAN_SSID_MAX_LEN SSID_MAX_LEN
#define ATF_MAX_SSID 16
#define ATF_MAX_PEER 512

/* Percentage value scaled by 10 to avoid decimals. (e.g., 10.5% → 105)
 * So, one digit after the decimal is accounted and represented as integer.
 */
#define ATF_RADIO_DEFAULT_AIRTIME 1000

/* To handle scalability in large client environments
 * (e.g., 512 clients), limit peer data transmission to batches of 50
 * clients per NL command to avoid exceeding netlink message size constraints.
 */
#define ATF_NUM_PEERS_DATA_PER_MSG 50
#define TAG_ATF_GROUP_WMM_AC_INFO 841

/**
 * enum atf_offload_update_flag - Airtime update flags for driver
 * @ATF_OFFLOAD_FULL_UPDATE: Full update and default update method
 * @ATF_OFFLOAD_PARTIAL_JOIN_UPDATE: Partial update method for peer join case
 * @ATF_OFFLOAD_PARTIAL_LEAVE_UPDATE: Partial update method for peer leave case
 * @ATF_OFFLOAD_NO_UPDATE: set when driver is updated with latest.
 */
enum atf_offload_update_flag {
	ATF_OFFLOAD_FULL_UPDATE,
	ATF_OFFLOAD_PARTIAL_JOIN_UPDATE,
	ATF_OFFLOAD_PARTIAL_LEAVE_UPDATE,
	ATF_OFFLOAD_NO_UPDATE,
};

#define ATF_OFFLOAD_SET_NO_UPDATE(algo) (algo->update_flag = ATF_OFFLOAD_NO_UPDATE)

#define ATF_OFFLOAD_SET_FULL_UPDATE(algo) (algo->update_flag = ATF_OFFLOAD_FULL_UPDATE)

#define ATF_OFFLOAD_IS_FULL_UPDATE(algo) (algo->update_flag == ATF_OFFLOAD_FULL_UPDATE)

#define ATF_OFFLOAD_IS_JOIN_UPDATE(algo)                                                 \
	(algo->update_flag == ATF_OFFLOAD_PARTIAL_JOIN_UPDATE)

#define ATF_OFFLOAD_IS_LEAVE_UPDATE(algo)                                                \
	(algo->update_flag == ATF_OFFLOAD_PARTIAL_LEAVE_UPDATE)

#define ATF_OFFLOAD_SET_JOIN_UPDATE(algo)                                                \
	do {                                                                             \
		if (!ATF_OFFLOAD_IS_FULL_UPDATE(algo))                                   \
			algo->update_flag = ATF_OFFLOAD_PARTIAL_JOIN_UPDATE;             \
	} while (0)

#define ATF_OFFLOAD_SET_LEAVE_UPDATE(algo)                                               \
	do {                                                                             \
		if (!ATF_OFFLOAD_IS_FULL_UPDATE(algo) &&                                 \
		    !ATF_OFFLOAD_IS_JOIN_UPDATE(algo))                                   \
			algo->update_flag = ATF_OFFLOAD_PARTIAL_LEAVE_UPDATE;            \
	} while (0)

#define ATF_SET_STA_TO_UPDATE(sta) (sta.is_updated = true)

#define ATF_CLEAR_STA_UPDATED(sta) (sta.is_updated = false)

#define ATF_IS_STA_UPDATED(sta) (sta.is_updated)

#define ATF_AC_IS_BIT_SET(bmap, ac) ((bmap) & ((u8)(1 << (ac))))

#define GENMASK(h, l) (((INT32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#define LEN GENMASK(15, 0)
#define TAG GENMASK(31, 16)

#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))
#define HDR_SIZE sizeof_field(struct build_header, header)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define PREP_LEN(val)     ((val) & 0xFFFF)
#define PREP_TAG(val)     (((val) & 0xFFFF) << 16)
#define PREP(tag, len)    (PREP_LEN(len) | PREP_TAG(tag))

#define TAG_ARRAY_STRUCT 0x12

struct build_header {
	__le32 header;
	u8 value[];
} __attribute__((__packed__));

struct atf_group_wmm_ac_info {
	__le32 header;
	__le32 atf_group_id;
	__le32 atf_units_be;
	__le32 atf_units_bk;
	__le32 atf_units_vi;
	__le32 atf_units_vo;
} __attribute__((__packed__));

struct atf_consumption {
	u32 consumption;
};

/**
 * @struct atf_airtime_consumption - airtime used
 */
struct atf_airtime_consumption {
	struct atf_consumption tx_consumption[WMM_AC_NUM];
	struct atf_consumption rx_consumption[WMM_AC_NUM];
};

/**
 * @struct atf_peer_config - per sta config.
 */

struct atf_peer_config {
	struct atf_group *group;
	struct atf_algo *algo;
	u8 link_id;
	u32 user_cfg_airtime;
	u8 addr[ETH_ALEN];
	bool calculated_for_airtime;
	struct atf_airtime_consumption peer_airtime;
	u8 actual_airtime;
	u8 ul_airtime;
	u32 actual_duration;
	u32 actual_ul_duration;

	/* add this node to atf_algo */
	struct dl_list list;
	char group_name[WLAN_SSID_MAX_LEN];
};

/**
 * @struct atf_peer - atf peer config reference used in sta_info.
 */

struct atf_peer {
	bool atf_configured;
	struct atf_group *group;
	struct atf_peer_config *peer_cfg_ref;
	struct sta_info *sta;
	u32 calculated_airtime;
	u32 calculated_ac_airtime[WMM_AC_NUM];
	struct hostapd_data *bss;
	struct atf_airtime_consumption peer_airtime;
	u8 actual_airtime;
	u8 ul_airtime;
	u32 actual_duration;
	u32 actual_ul_duration;

	/* Flag to indicate whether it is updated to driver */
	bool is_updated;
};

/**
 * @struct atf_bh_peer - atf backhaul peer config reference (Root AP BSSID)
 */
struct atf_bh_peer {
	struct dl_list list;
	u8 addr[ETH_ALEN];
	struct atf_peer atf_peer;
};

/**
 * @struct atf_ssid_config - per ssid config when group is not enabled.
 */
struct atf_ssid_config {
	struct atf_algo *algo;
	struct atf_group *group;
	char ifname[IFNAMSIZ + 1];
	char name[WLAN_SSID_MAX_LEN + 1];
	u32 user_cfg_airtime;

	/* add this node to atf_algo */
	struct dl_list list;
};

/**
 * @struct atf_group - configurations of each atf group which
 * 		       consist one or more ssids.
 */

struct atf_group {
	struct atf_algo *algo;
	u8 index;
	char name[WLAN_SSID_MAX_LEN + 1];

	u32 num_of_ssid;
	char ssidname[WLAN_SSID_MAX][WLAN_SSID_MAX_LEN + 1];
	u32 user_cfg_airtime;
	u32 sched_policy;
	u32 expl_peers_airtime;
	u8 actual_airtime;
	u8 ul_airtime;
	u32 actual_duration;
	u32 actual_ul_duration;

	/* ac_bitmap - bit N is set when AC N has been explicitly configured.
	 * AC index: 0=BE, 1=BK, 2=VI, 3=VO
	 */
	u8 ac_bitmap;
	u32 ac_airtime[WMM_AC_NUM];
	u32 ac_actual_airtime[WMM_AC_NUM];
	u32 ac_ul_airtime[WMM_AC_NUM];
	u32 calculated_ac_airtime[WMM_AC_NUM];

	/* add this node to atf_algo */
	struct dl_list list;

	/* used in ATF distribution logic */
	struct dl_list implicit_peers;
	u16 num_impl_peers;
	struct dl_list explicit_peers;
	u16 num_expl_peers;
	u16 num_bh_peers;
	u32 calculated_airtime;
	u32 total_explicit_airtime;

	/* set when ssid is valid and up and running */
	bool is_configured;
};

/**
 * @struct atf_algo - per-radio specific atf algorithm
 */

struct atf_algo {
	struct hostapd_iface *iface;

	/* This node will be added to atf_offload global structure's
	 * algo_list
	 */
	struct dl_list list;
	struct dl_list groups;
	u8 num_group_cfg;
	bool atf_enabled;
	struct dl_list ssid_cfgs;
	u8 num_ssid_cfg;
	struct dl_list peer_cfgs;
	u16 num_peer_cfg;
	struct dl_list bh_peers;

	/* Used to validate combined airtime of SSIDs/SSID
	 * groups is betweeon 0 and 1000.
	 */
	u32 user_cfg_airtime;

	/* Feature flags */
	bool ssid_group_enabled;
	bool atfstrictsched_enabled;
	bool atf_stats_enabled;
	u8 atf_stats_timeout;

	/* used in ATF distribution logic*/
	enum atf_offload_update_flag update_flag;
	/*Flag to indicate update task scheduled */
	bool atf_tasksched;

	/* first sta association should be sent as
	 * full update. This flag represents whether
	 * it is done or not
	 */
	bool init_update_done;

	/* no. of peers to be updated to driver*/
	u16 no_of_peers;

	/* used for ATF config parsing*/
	struct atf_group *last_group;
	struct atf_ssid_config *last_ssid_cfg;
	struct atf_peer_config *last_peer_cfg;

	/* Radio airtime stats */
	struct atf_airtime_consumption radio_airtime;
	u32 actual_duration;
	u32 actual_ul_duration;
};

/**
 * @struct atf_offload - Global structure to hold the state of atf_offload
 */
struct atf_offload {
	struct dl_list algo_list;
};


struct atf_peer_info {
	u8 peer_macaddr[6];
	u16 percentage_peer;
	u8 group_index;
	u8 explicit_peer_flag;
};

struct atf_peer_params {
	u16 num_peers;
	u8 full_update_flag;
	struct atf_peer_info *peer_info;
};

struct atf_group_param_info {
	u8 group_index;
	u16 group_airtime;
	u16 total_implicit_peers;
	u16 total_explicit_peers;
	u16 total_implicit_peer_units;
	u8 group_policy;
};

struct atf_group_params {
	u8 num_groups;
	struct atf_group_param_info *group_info;
};

struct atf_group_wmm_ac_config {
	u32 ac_be; /* Relative ATF % for BE */
	u32 ac_bk; /* Relative ATF % for BK */
	u32 ac_vi; /* Relative ATF % for VI */
	u32 ac_vo; /* Relative ATF % for VO */
	u32 reserved[2];
};

struct atf_group_wmm_ac_params {
	u32 num_groups;
	struct atf_group_wmm_ac_config *wmm_ac_cfg;
};

struct atf_group_info {
	u8 percentage_group;
	u8 implicit_peer;
	u8 explicit_peers;
	u8 implicit_peer_units;
	u8 group_units_reserved;
};

#ifdef CONFIG_ATF_OFFLOAD

void atf_offload_init(struct hapd_interfaces *ifaces);

void atf_offload_deinit(void);

void atf_init_algo(struct hostapd_iface *iface);

void atf_deinit_algo(struct hostapd_iface *iface);

struct atf_group *atf_allocate_group(const char *name, struct atf_algo *algo);

void atf_free_group(struct atf_group *group);

struct atf_group *atf_find_group_by_name(const char *name, struct atf_algo *algo);

void
atf_free_algo_configs(struct atf_algo *algo, bool skip_default);

struct atf_ssid_config *atf_find_ssid_config_by_name(char *name, struct atf_algo *algo);

struct atf_ssid_config *atf_allocate_ssid_config(char *name, struct atf_algo *algo);

void atf_free_ssid_config(struct atf_ssid_config *ssid);

struct atf_peer_config *atf_find_peer_config_by_mac(u8 *mac, struct atf_algo *algo);

int atf_add_ssid_to_group(struct atf_group *group, const char *name);

struct atf_peer_config *atf_allocate_peer_config(u8 *macaddr, struct atf_algo *algo);

void atf_free_peer_config(struct atf_peer_config *peer_config);

int atf_timer_start(struct hostapd_iface *iface);

void atf_reset_group_ac_config(struct atf_group *group);

void atf_timer_stop(struct hostapd_iface *iface);

void atf_trigger_config_timer(struct hostapd_iface *iface);

void atf_offload_initialize_peer(struct sta_info *sta);

void atf_offload_deinitialize_peer(struct sta_info *sta);

void atf_offload_send_feature_params(struct hostapd_data *hapd);

void atf_offload_set_ssid_sched_policy(struct hostapd_data *hapd);

void atf_join_leave_update(struct hostapd_iface *iface, struct sta_info *sta,
                           bool is_join);

u8 atf_get_hw_idx(struct hostapd_iface *iface);

int nl80211_atf_offload_enable_disable(void *priv, u8 radio_index, u8 value);

int nl80211_atf_offload_strict_scheduling_enable_disable(void *priv, u8 radio_index, u8 value);

int nl80211_atf_offload_ssid_sched_policy(void *priv, u8 radio_index, u8 value, int link_id);

void atf_delete_ssid_from_group(struct atf_algo *algo, const char *name);

struct atf_group *
atf_find_group(struct atf_algo *algo, const char *name);

int nl80211_atf_offload_stats_enable_disable(void *priv, u8 radio_index, u8 value);

void atf_offload_disable_atf_stats(struct hostapd_iface *ifaces);

int nl80211_atf_offload_stats_timeout(void *priv, u8 radio_index, u8 value);

int nl80211_atf_offload_showatfstats(void *priv, u8 radio_index, struct hostapd_data *hapd);

void atf_add_bhsta_to_bh_peers(struct hostapd_iface *iface);
void atf_remove_bhsta_from_bh_peers(struct hostapd_iface *iface);
void atf_bh_join_leave_update(struct hostapd_iface *iface, bool is_conn);
#else
static inline void atf_offload_disable_atf_stats(struct hostapd_iface *ifaces)
{
}

static inline void atf_offload_init(struct hapd_interfaces *ifaces)
{
}

static inline void atf_offload_deinit(void)
{
}

static inline void atf_init_algo(struct hostapd_iface *iface)
{
}

static inline void atf_deinit_algo(struct hostapd_iface *iface)
{
}

static inline void atf_join_leave_update(struct hostapd_iface *iface, struct sta_info *sta,
					 bool is_join)
{
}

static inline void atf_bh_join_leave_update(struct hostapd_iface *iface, bool is_conn)
{
}

static inline void atf_offload_initialize_peer(struct sta_info *sta)
{
}

static inline void atf_offload_deinitialize_peer(struct sta_info *sta)
{
}

static inline void atf_offload_send_feature_params(struct hostapd_data *hapd)
{
}

static inline void atf_offload_set_ssid_sched_policy(struct hostapd_data *hapd)
{
}
#endif /* CONFIG_ATF_OFFLOAD */
#endif /* ATF_OFFLOAD_H */
