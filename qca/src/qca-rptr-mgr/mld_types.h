/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


#ifndef MLD_TYPES_H
#define MLD_TYPES_H

#include <net/if.h>
#include <stdint.h>
#include <string.h>

struct wpa_ctrl;

#define MAX_MLD_AP_VAPS          16
#define MAX_MLD_LINKS_PER_AP      4
#define MAX_MLD_STA_VAPS          1
#define IFNAMSIZE 32

struct multi_hw_info {
	uint8_t hw_idx;
	int start_freq;
	int end_freq;
};

struct chan_params_info {
	int freq;
	int c_freq1;
	int c_freq2;
	int sec_off;
	int width;
	int punc_bitmap;
	int is_dfs;
};

struct mld_link_iface {
	char ifname[IFNAMSIZE];
	int  index;
	struct wpa_ctrl *link_ctrl;
	struct multi_hw_info hw_info;
	struct chan_params_info ch_info;
};

struct mld_ap_vap {
	char ifname[IFNAMSIZE];
	struct mld_link_iface links[MAX_MLD_LINKS_PER_AP];
	int link_count;
	struct multi_hw_info hw_info;
	struct wpa_ctrl *ap_ctrl;
	struct chan_params_info ch_info;
};

struct mld_sta_vap {
	char ifname[IFNAMSIZE];
	struct wpa_ctrl *sta_ctrl;
	int target_hw[MAX_MLD_LINKS_PER_AP];
};

struct mld_config {
	struct mld_ap_vap  ap_vaps[MAX_MLD_AP_VAPS];
	int ap_vap_count;
	struct mld_sta_vap sta_vap;
	int csa_bitmap[MAX_MLD_LINKS_PER_AP];
	int pre_auth_active[MAX_MLD_LINKS_PER_AP];
	int rptr_mgr_comm_type;
	int athnewind;
	int skip_cac;
};

extern struct mld_config cfg;

/*
 * Initialize link interface entry with defaults and ifname
 */
static inline void mld_link_iface_init(struct mld_link_iface *link,
						const char *ifname, int idx)
{
	if (link) {
		strlcpy(link->ifname, ifname, IFNAMSIZE);
		link->ifname[IFNAMSIZE - 1] = '\0';
		link->index = idx;
		link->link_ctrl = NULL;
		link->hw_info.hw_idx = (uint8_t)-1;
		link->hw_info.start_freq = 0;
		link->hw_info.end_freq = 0;
		link->ch_info.freq = 0;
		link->ch_info.c_freq1 = 0;
		link->ch_info.c_freq2 = -1;
		link->ch_info.sec_off = 0;
		link->ch_info.width = 0;
		link->ch_info.punc_bitmap = 0;
		link->ch_info.is_dfs = 0;
	}
}

/*
 * Initialize AP VAP entry with defaults and ifname
 */
static inline void mld_ap_vap_init(struct mld_ap_vap *ap, const char *ifname)
{
	if (ap) {
		strlcpy(ap->ifname, ifname, IFNAMSIZE);
		ap->ifname[IFNAMSIZE - 1] = '\0';
		ap->link_count = 0;
		memset(ap->links, 0, sizeof(ap->links));
		ap->hw_info.hw_idx = (uint8_t)-1;
		ap->hw_info.start_freq = 0;
		ap->hw_info.end_freq = 0;
		ap->ap_ctrl = NULL;
		ap->ch_info.freq = 0;
		ap->ch_info.c_freq1 = 0;
		ap->ch_info.c_freq2 = -1;
		ap->ch_info.sec_off = 0;
		ap->ch_info.width = 0;
		ap->ch_info.punc_bitmap = 0;
		ap->ch_info.is_dfs = 0;
	}
}

/*
 * Initialize STA VAP entry with defaults and ifname
 */
static inline void mld_sta_vap_init(struct mld_sta_vap *sta, const char *ifname)
{
	if (sta) {
		strlcpy(sta->ifname, ifname, IFNAMSIZE);
		sta->ifname[IFNAMSIZE - 1] = '\0';
		sta->sta_ctrl = NULL;
		memset(sta->target_hw, 0, sizeof(sta->target_hw));
	}
}

/*
 * Initialize top-level MLD configuration container to safe defaults
 */
static inline void mld_config_init(struct mld_config *c)
{
	if (c) {
		c->ap_vap_count = 0;
		memset(c->ap_vaps, 0, sizeof(c->ap_vaps));
		memset(&c->sta_vap, 0, sizeof(c->sta_vap));
		memset(c->csa_bitmap, 0, sizeof(c->csa_bitmap));
		memset(c->pre_auth_active, 0, sizeof(c->pre_auth_active));
	}
}

#endif /* MLD_TYPES_H */
