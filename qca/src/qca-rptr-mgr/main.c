/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <dirent.h>
#include <sys/un.h>

#include <pthread.h>
#include <includes.h>
#include "eloop.h"
#include "utils/common.h"
#include "wpa_ctrl.h"
#include <syslog.h>
#include "mld_types.h"
#include "mld_chan.h"
#include "mld_ctrl.h"
#include "ucode.h"
#include <fcntl.h>
#include "os.h"
#include "ubus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * Local logging macro for consistent debug output
 */
#ifndef rptr_mgr_printf
#define rptr_mgr_printf(fmt, args...) \
    fprintf(stderr, "rptr_mgr: %s:%d: " fmt "\n", __func__, __LINE__, ##args)
#endif

#define MAX_PID_LEN 6

/*
 * Global MLD configuration accessible across modules
 */
struct mld_config cfg;

/*
 * Forward declarations for event processors
 */
static void mld_wpa_s_ctrl_iface_process(const char *msg);
static void mld_hostapd_ctrl_iface_process(struct wpa_ctrl *src_ctrl, const char *msg);

/*
 * handle_chan_switch_event - Issue CHAN_SWITCH and manage CSA aggregation
 *
 * Builds CHAN_SWITCH for target frequency and sends to a single endpoint
 * that requires a change. Tracks all matching APs/links via CSA bitmap
 * to aggregate completion. If no endpoint needs a change and all are
 * already aligned, sends CHAN_SW_FINISHED_NOTIFY to STA and clears state.
 *
 * Args:
 *  - freq: center frequency (MHz)
 *  - c_freq1/c_freq2: center frequencies for CHAN_SWITCH (MHz)
 *  - bandwidth: 20/40/80/160
 *  - sec_channel_offset: -1/0/+1
 *  - hw_idx_override: target radio index or -1 to derive
 *  - punc_bitmap: puncturing bitmap or <0 to omit
 *  - is_dfs: whether target falls under DFS (for skip_cac flag)
 *  - set_bitmap: whether to mark CSA bits for aggregation
 */
static void handle_chan_switch_event(int freq, int c_freq1, int c_freq2,
			 int bandwidth, int sec_channel_offset,
			 int hw_idx_override, int punc_bitmap,
			 int is_dfs, int set_bitmap)
{
	char base[256];
	int skip_cac = is_dfs && cfg.skip_cac;
	struct wpa_ctrl *send_ctrl = NULL;
	int send_is_mlo = 0;
	char send_ifname[IFNAMSIZE] = { 0 };
	int same = 1;
	int hw_idx;
	int pos = snprintf(base, sizeof(base),
		"CHAN_SWITCH 10 %d center_freq1=%d bandwidth=%d "
		"sec_channel_offset=%d skip_cac=%d rptr_mgr=1",
		freq, c_freq1, bandwidth,
		sec_channel_offset, skip_cac);
	if (c_freq2 > 0 && pos < (int) sizeof(base))
		pos += snprintf(base + pos, sizeof(base) - pos,
			" center_freq2=%d", c_freq2);
	if (punc_bitmap >= 0 && pos < (int) sizeof(base))
		pos += snprintf(base + pos, sizeof(base) - pos,
			" punct_bitmap=%d", punc_bitmap);

	hw_idx = hw_idx_override >= 0 ?
		hw_idx_override : wpa_get_hw_idx_by_freq(freq);
	rptr_mgr_printf(
		"CHAN_SWITCH cmd='%s' hw_idx=%d freq=%d "
		"cf1=%d cf2=%d bw=%d sec_off=%d skip_cac=%d rptr_mgr=1",
		base, hw_idx, freq, c_freq1, c_freq2,
		bandwidth, sec_channel_offset, skip_cac);

	for (int i = 0; i < cfg.ap_vap_count; i++) {
		if (cfg.ap_vaps[i].link_count == 0) {
			/* Legacy AP */
			if (!cfg.ap_vaps[i].ap_ctrl)
				continue;

			if (hw_idx >= 0 &&
			    (int) cfg.ap_vaps[i].hw_info.hw_idx != hw_idx)
				continue;

			/* Set bitmap for this AP since it matches */
			if (set_bitmap && hw_idx >= 0)
				cfg.csa_bitmap[hw_idx] |= BIT(i);

			same = same && chan_params_equal(&cfg.ap_vaps[i].ch_info,
				freq, c_freq1, c_freq2,
				bandwidth, punc_bitmap);
			/* Choose first eligible endpoint to send the
			 * request based on param diff
			 */
			if (!send_ctrl) {
				if (!chan_params_equal(&cfg.ap_vaps[i].ch_info,
						freq, c_freq1, c_freq2,
						bandwidth, punc_bitmap)) {
					send_ctrl = cfg.ap_vaps[i].ap_ctrl;
					send_is_mlo = 0;
					strlcpy(send_ifname,
						cfg.ap_vaps[i].ifname,
						sizeof(send_ifname));
				}
			}
		} else {
			/* MLO AP: check its links */
			int link_matched = 0;
			for (int j = 0; j < cfg.ap_vaps[i].link_count; j++) {
				if (!cfg.ap_vaps[i].links[j].link_ctrl)
					continue;
				if (hw_idx >= 0 &&
				    cfg.ap_vaps[i].links[j].hw_info.hw_idx !=
				    hw_idx)
					continue;
				link_matched = 1;
				same = same &&
					chan_params_equal(
						&cfg.ap_vaps[i].links[j].ch_info,
						freq, c_freq1, c_freq2,
						bandwidth, punc_bitmap);
				/* Pick the first matching link to send if
				 * params differ
				 */
				if (!send_ctrl) {
					if (!chan_params_equal(
						&cfg.ap_vaps[i].links[j].ch_info,
						freq, c_freq1, c_freq2,
						bandwidth, punc_bitmap)) {
						send_ctrl =
							cfg.ap_vaps[i].links[j].link_ctrl;
						send_is_mlo = 1;
						strlcpy(send_ifname,
							cfg.ap_vaps[i].links[j].ifname,
							sizeof(send_ifname));
					}
				}
				/* Iterate all links to set bitmap for the AP */
			}
			if (link_matched && set_bitmap && hw_idx >= 0)
				cfg.csa_bitmap[hw_idx] |= BIT(i);
		}
	}

	if (send_ctrl) {
		char cmd[256];
		strlcpy(cmd, base, sizeof(cmd));
		strlcat(cmd, send_is_mlo ? " eht" : " he",
			sizeof(cmd));
		char reply[256];
		size_t rlen = sizeof(reply);
		int ret = wpa_ctrl_request(send_ctrl, cmd,
						strlen(cmd), reply,
						&rlen, NULL);

		if (ret) {
			rptr_mgr_printf("CHAN_SWITCH: cmd fails ret = %d", ret);
			return;
		}

		if (set_bitmap && hw_idx >= 0)
			rptr_mgr_printf(
				"sent CHAN_SWITCH once to %s ret=%d "
				"csa_bitmap=0x%x hw_idx=%d cmd='%s'",
				 send_ifname, ret,
				 cfg.csa_bitmap[hw_idx], hw_idx, cmd);
		else
			rptr_mgr_printf(
				"sent CHAN_SWITCH once to %s ret=%d "
				"hw_idx=%d cmd='%s'",
				 send_ifname, ret, hw_idx, cmd);
		if (set_bitmap)
			cfg.pre_auth_active[hw_idx] = 1;

		/* Update stored params for the endpoint we sent to */
		for (int i = 0; i < cfg.ap_vap_count; i++) {
			if (cfg.ap_vaps[i].link_count == 0) {
				if (cfg.ap_vaps[i].ap_ctrl == send_ctrl) {
					chan_params_update(
						&cfg.ap_vaps[i].ch_info,
						freq, c_freq1, c_freq2,
						sec_channel_offset,
						bandwidth, punc_bitmap,
						is_dfs);

					break;
				}
			} else {
				for (int j = 0; j < cfg.ap_vaps[i].link_count;
				     j++) {
					if (cfg.ap_vaps[i].links[j].link_ctrl ==
					    send_ctrl) {
						chan_params_update(
							&cfg.ap_vaps[i]
								.links[j].ch_info,
								freq, c_freq1, c_freq2,
								sec_channel_offset,
								bandwidth,
								punc_bitmap,
								is_dfs);

						break;
					}
				}
			}
		}
	} else {
		/* No endpoint required a CHAN_SWITCH. If we had matching
		 * endpoints and all were same, send CHAN_SW_FINISHED_NOTIFY
		 * to STA and clear pre-auth/bitmap.
		 */
		if (set_bitmap && same && cfg.csa_bitmap[hw_idx] &&
		    cfg.sta_vap.sta_ctrl) {
			char sw_done[64];
			snprintf(sw_done, sizeof(sw_done),
				 "CHAN_SW_FINISHED_NOTIFY freq =%d",
				 freq);
			char reply[128];
			size_t rlen = sizeof(reply);
			int ret = wpa_ctrl_request(cfg.sta_vap.sta_ctrl,
					       sw_done,
					       strlen(sw_done),
					       reply, &rlen,
					       NULL);
			if (ret) {
				rptr_mgr_printf("buf = %d ret = %d",
							sw_done, ret);
				return;
			}
			rptr_mgr_printf(
				"no CHAN_SWITCH needed; notified STA %s "
				"ret=%d msg='%s'",
				 cfg.sta_vap.ifname, ret, sw_done);
			if (hw_idx >= 0 && hw_idx < MAX_MLD_LINKS_PER_AP) {
				cfg.csa_bitmap[hw_idx] = 0;
				cfg.pre_auth_active[hw_idx] = 0;
			}
		} else {
			rptr_mgr_printf(
				"no eligible endpoint for CHAN_SWITCH (hw_idx=%d)",
				 hw_idx);
		}
	}
}

/*
 * ap_sock_handler - Process hostapd AP ctrl_iface messages
 *
 * Receives messages from AP VAPs (legacy or MLD links) and forwards
 * them to the MLD hostapd ctrl processor for handling. Drain all
 * pending messages from the control socket to avoid leaving queued
 * events.
 */
static void ap_sock_handler(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_ctrl *ctrl = (struct wpa_ctrl *) sock_ctx;

	if (!ctrl) {
		rptr_mgr_printf("ap_sock_handler: NULL ctrl\n");
		return;
	}

	while (wpa_ctrl_pending(ctrl) > 0) {
		char buf[2048];
		size_t len = sizeof(buf) - 1;
		int res = wpa_ctrl_recv(ctrl, buf, &len);

		if (res < 0) {
			perror("wpa_ctrl_recv");
			break;
		}
		buf[len] = '\0';
		mld_hostapd_ctrl_iface_process(ctrl, buf);
	}
}

/*
 * sta_sock_handler - Process wpa_supplicant STA ctrl_iface messages
 *
 * Receives messages from STA VAP and forwards them to the MLD wpa_s
 * ctrl processor for handling.
 */
static void sta_sock_handler(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_ctrl *ctrl = (struct wpa_ctrl *) sock_ctx;

	if (!ctrl) {
		rptr_mgr_printf("sta_sock_handler: NULL ctrl\n");
		return;
	}

	while (wpa_ctrl_pending(ctrl) > 0) {
		char buf[2048];
		size_t len = sizeof(buf) - 1;

		if (wpa_ctrl_recv(ctrl, buf, &len) == 0) {
			buf[len] = '\0';
			mld_wpa_s_ctrl_iface_process(buf);
		} else {
			perror("wpa_ctrl_recv");
			break;
		}
	}
}
/*
 * parse_kv_int - Helper function for parsing key value for int
 */
static int parse_kv_int(const char *s, const char *key, int def)
{
	const char *p = strstr(s, key);
	if (!p)
		return def;
	p += strlen(key);
	while (*p == ' ')
		p++;
	if (*p != '=') {
		p = strchr(p, '=');
		if (!p)
			return def;
	}
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	return atoi(p);
}

/*
 * parse_kv_token - Helper function for parsing key value for token
 */
static void parse_kv_token(const char *s, const char *key,
			char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;
	out[0] = '\0';
	const char *p = strstr(s, key);
	if (!p)
		return;
	p += strlen(key);
	while (*p == ' ')
		p++;
	if (*p != '=') {
		p = strchr(p, '=');
		if (!p)
			return;
	}
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	size_t n = 0;
	while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
	       n + 1 < outsz) {
		out[n++] = *p++;
	}
	out[n] = '\0';
}

/*
 * query_and_store_hw_info - Query and store the AP link hw_info
 *
 * Send cmd: GET_HW_INFO to hostapd AP link to get the hw_idx, start_freq and
 * end_freq. It helps to do the proper link mapping between hostapd(Rep AP) and
 * wpa_supp(Rep STA)
 */
static int query_and_store_hw_info(struct wpa_ctrl *ctrl,
			 struct multi_hw_info *out_info,
			 const char *ifname,
			 int legacy)
{
	char hwbuf[128];
	size_t hwlen = sizeof(hwbuf) - 1;
	int req;

        if (!out_info || !ifname)
                return -1;

	req = wpa_ctrl_request(ctrl,
				       "GET_HW_INFO",
				       strlen("GET_HW_INFO"),
				       hwbuf,
				       &hwlen,
				       NULL);

	if (req == 0) {
		if (hwlen >= sizeof(hwbuf))
			hwlen = sizeof(hwbuf) - 1;
		hwbuf[hwlen] = '\0';
		int parsed_hw_idx = parse_kv_int(hwbuf, "hw_idx", -1);
		int parsed_start = parse_kv_int(hwbuf, "start_freq", 0);
		int parsed_end = parse_kv_int(hwbuf, "end_freq", 0);
		out_info->hw_idx = parsed_hw_idx;
		out_info->start_freq = parsed_start;
		out_info->end_freq = parsed_end;
		if (legacy)
			rptr_mgr_printf(
				"\nStored HW_INFO (legacy) for %s: "
				"hw_idx=%d start_freq=%d end_freq=%d",
				ifname, out_info->hw_idx,
				out_info->start_freq,
				out_info->end_freq);
		else
			rptr_mgr_printf(
				"\nStored HW_INFO for %s: hw_idx=%d "
				"start_freq=%d end_freq=%d",
				ifname, out_info->hw_idx,
				out_info->start_freq,
				out_info->end_freq);
	} else {
		out_info->hw_idx = (u8) -1;
		out_info->start_freq = 0;
		out_info->end_freq = 0;
		if (legacy)
			rptr_mgr_printf(
				"\nGET_HW_INFO request failed (legacy) for %s "
				"(ret=%d)", ifname, req);
		else
			rptr_mgr_printf(
				"\nGET_HW_INFO request failed for %s (ret=%d)",
				ifname, req);
	}

	return req;
}

/*
 * query_get_channel_info - Query and get the Rep STA channel list
 *
 * Send cmd: STATUS to wpa_supp(Rep STA) to parse and get the configured
 * channel list. It helps to figure-out the hw_idx that is enabled for Rep STA
 * and do link mapping for Dependent Repeater handling
 */
static int query_get_channel_info(struct wpa_ctrl *ctrl,
			 struct chan_params_info *out_ch,
			 const char *ifname,
			 int legacy)
{
	char sbuf[2048];
	size_t slen = sizeof(sbuf) - 1;
	int req;
	int freq;
	int op_chwidth, width;
	int seg0, seg1;
	int band;
	int c_freq1, c_freq2;
	int sec_off;
	char pb[32];
	int punc_bitmap = 0;

	if (!ctrl || !out_ch || !ifname)
		return -1;

	req = wpa_ctrl_request(ctrl,
				       "STATUS",
				       strlen("STATUS"),
				       sbuf,
				       &slen,
				       NULL);
	if (req == 0) {
		if (slen >= sizeof(sbuf))
			slen = sizeof(sbuf) - 1;
		sbuf[slen] = '\0';

		/* Expected tokens in STATUS reply:
		 *  - freq=<MHz>
		 *  - eht_oper_chwidth or he_oper_chwidth
		 *    (0=20,1=80,2=160,3=320)
		 *  - eht_oper_centr_freq_seg0_idx or
		 *    he_oper_centr_freq_seg0_idx
		 *  - he_oper_centr_freq_seg1_idx (optional)
		 *  - punct_bitmap=<hex or int>
		 */
		freq = parse_kv_int(sbuf, "freq", 0);

		op_chwidth =
			parse_kv_int(sbuf, "eht_oper_chwidth", -1);
		if (op_chwidth < 0)
			op_chwidth =
				parse_kv_int(sbuf, "he_oper_chwidth", -1);
		width = 20;
		switch (op_chwidth) {
		case 0:
			width = 20;
			break;
		case 1:
			width = 80;
			break;
		case 2:
			width = 160;
			break;
		case 3:
			width = 320;
			break;
		default:
			/* leave width as 20 if unknown */
			break;
		}

		int seg0 = parse_kv_int(sbuf,
				       "eht_oper_centr_freq_seg0_idx",
				       -1);
		if (seg0 < 0)
			seg0 = parse_kv_int(
				sbuf, "he_oper_centr_freq_seg0_idx", -1);
		int seg1 = parse_kv_int(sbuf,
				       "eht_oper_centr_freq_seg1_idx",
				       -1);
		if (seg1 < 0)
			seg1 = parse_kv_int(
				sbuf, "he_oper_centr_freq_seg1_idx", -1);

		/* Determine band and compute center frequencies
		 * from segment indexes
		 */
		band = wifi_get_band_by_freq(freq);
		c_freq1 = (seg0 > 0) ?
			wifi_channel_to_freq(seg0, band) : 0;
		c_freq2 = (seg1 > 0) ?
			wifi_channel_to_freq(seg1, band) : -1;

		/* STATUS does not include sec_channel_offset;
		 * default to 0
		 */
		sec_off = 0;

		parse_kv_token(sbuf, "punct_bitmap", pb,
			      sizeof(pb));
		if (pb[0])
			punc_bitmap = (int) strtol(pb, NULL, 0);

		out_ch->freq = freq;
		out_ch->c_freq1 = c_freq1;
		out_ch->c_freq2 = c_freq2;
		out_ch->sec_off = sec_off;
		out_ch->width = width;
		out_ch->punc_bitmap = punc_bitmap;
		out_ch->is_dfs = 0;

		if (legacy)
			rptr_mgr_printf(
				"\nStored CH_INFO (legacy) for %s: "
				"freq=%d c_freq1=%d c_freq2=%d sec_off=%d "
				"width=%d punc_bitmap=%d",
				ifname, freq, c_freq1, c_freq2,
				sec_off, width, punc_bitmap);
		else
			rptr_mgr_printf(
				"\nStored CH_INFO for %s: freq=%d "
				"c_freq1=%d c_freq2=%d sec_off=%d "
				"width=%d punc_bitmap=%d",
				ifname, freq, c_freq1, c_freq2,
				sec_off, width, punc_bitmap);
	} else {
		if (legacy)
			rptr_mgr_printf(
				"\nSTATUS request failed (legacy) for %s "
				"(ret=%d)", ifname, req);
		else
			rptr_mgr_printf(
				"\nSTATUS request failed for %s (ret=%d)",
				ifname, req);
	}

	return req;
}

/*
 * disable_aps_for_sta_scan_or_auth - Disable AP links based on Rep STA state
 *
 * Dependent Repeater: Disable only AP endpoints (legacy or MLO links)
 * whose radios(hw_idx) match when Rep STA moves to Scan or Auth state
 */
static void disable_aps_for_sta_scan_or_auth(void)
{
	char fbuf[4096];
	size_t flen = sizeof(fbuf) - 1;
	int ret;
	char *copy;
	char *saveptr = NULL;
	char *tok;
	int freq_count = 0;

	if (!cfg.sta_vap.sta_ctrl) {
		rptr_mgr_printf(
			"SCAN-STARTED: STA ctrl unavailable; "
			"skipping targeted DISABLE\n");
		return;
	}

	ret = wpa_ctrl_request(cfg.sta_vap.sta_ctrl,
				       "GET_FREQ_LIST",
				       strlen("GET_FREQ_LIST"),
				       fbuf,
				       &flen,
				       NULL);
	if (ret != 0) {
		rptr_mgr_printf(
			"SCAN-STARTED: GET_FREQ_LIST failed ret=%d; "
			"skipping targeted DISABLE\n", ret);
		return;
	}
	if (flen >= sizeof(fbuf))
		flen = sizeof(fbuf) - 1;
	fbuf[flen] = '\0';

	memset(cfg.sta_vap.target_hw, 0,
	       sizeof(cfg.sta_vap.target_hw));

	/* Parse space-separated MHz values and mark matching hw_idx */
	copy = strdup(fbuf);
	if (!copy) {
		rptr_mgr_printf(
			"SCAN-STARTED: strdup failed; skipping "
			"targeted DISABLE\n");
		return;
	}

	tok= strtok_r(copy, " \n\t", &saveptr);
	while (tok) {
		int f = atoi(tok);
		if (f > 0) {
			int hw = wpa_get_hw_idx_by_freq(f);
			if (hw >= 0 && hw < MAX_MLD_LINKS_PER_AP)
				cfg.sta_vap.target_hw[hw] = 1;
			freq_count++;
		}
		tok = strtok_r(NULL, " \n\t", &saveptr);
	}
	free(copy);

	rptr_mgr_printf(
		"SCAN-STARTED: parsed %d freqs; target hw: "
		"[%d %d %d]\n",
		freq_count, cfg.sta_vap.target_hw[0],
		cfg.sta_vap.target_hw[1],
		cfg.sta_vap.target_hw[2]);

	/* Disable legacy APs that match target hw */
	for (int i = 0; i < cfg.ap_vap_count; i++) {
		if (cfg.ap_vaps[i].link_count == 0) {
			int hw_idx = (int) cfg.ap_vaps[i].hw_info.hw_idx;
			if (cfg.ap_vaps[i].ap_ctrl && hw_idx >= 0 &&
			    hw_idx < MAX_MLD_LINKS_PER_AP &&
			    cfg.sta_vap.target_hw[hw_idx]) {
				char reply[64];
				size_t rlen = sizeof(reply);
				int rc = wpa_ctrl_request(cfg.ap_vaps[i].ap_ctrl,
						       "DISABLE",
						       strlen("DISABLE"),
						       reply,
						       &rlen,
						       NULL);
				if (rc) {
					rptr_mgr_printf("Legacy: DISABLE AP cmd"
							"failed rc = %d", rc);
				} else {
					rptr_mgr_printf(
					"SCAN-STARTED: DISABLE legacy AP %s "
					"hw_idx=%d ret=%d\n",
					cfg.ap_vaps[i].ifname, hw_idx,
					rc);
				}
			}
		}
	}
	/* Disable MLO links that match target hw */
	for (int i = 0; i < cfg.ap_vap_count; i++) {
		for (int j = 0; j < cfg.ap_vaps[i].link_count; j++) {
			int hw_idx = (int) cfg.ap_vaps[i].links[j].hw_info.hw_idx;
			if (cfg.ap_vaps[i].links[j].link_ctrl &&
			    hw_idx >= 0 &&
			    hw_idx < MAX_MLD_LINKS_PER_AP &&
			    cfg.sta_vap.target_hw[hw_idx]) {
				char reply[64];
				size_t rlen = sizeof(reply);
				int rc = wpa_ctrl_request(
					cfg.ap_vaps[i].links[j].link_ctrl,
					"DISABLE",
					strlen("DISABLE"),
					reply,
					&rlen,
					NULL);
				if (rc) {
					rptr_mgr_printf("MLO: DISABLE AP cmd"
							"failed rc = %d", rc);
				} else {
					rptr_mgr_printf(
					"SCAN-STARTED: DISABLE AP Link %s "
					"hw_idx=%d ret=%d\n",
					cfg.ap_vaps[i].links[j].ifname,
					hw_idx, rc);
				}
			}
		}
	}
}

/*
 * send_rep_ap_enable - Enable AP links based on Rep STA connected state
 *
 * Dependent Repeater: Enable only AP endpoints (legacy or MLO links)
 * whose radios(hw_idx) match when Rep STA moves to Connected state
 */
static void send_rep_ap_enable(int hw_idx, int freq, int width,
			 int punct_bitmap, int c_freq1, int c_freq2)
{
	char cmd[256];
	int is_dfs = is_dfs_freq(freq);
	int sec_channel_offset =
		calc_sec_channel_offset(freq, c_freq1, width);

	snprintf(cmd, sizeof(cmd),
		 "REP_AP_ENABLE freq=%d width=%d punct_bitmap=%d "
		 "c_freq1=%d c_freq2=%d sec_off=%d is_dfs=%d",
		 freq, width, punct_bitmap, c_freq1, c_freq2,
		 sec_channel_offset, is_dfs);

	for (int i = 0; i < cfg.ap_vap_count; i++) {
		if (cfg.ap_vaps[i].link_count == 0) {
			if (cfg.ap_vaps[i].ap_ctrl &&
			    (int) cfg.ap_vaps[i].hw_info.hw_idx == hw_idx) {
				char reply[128];
				size_t rlen = sizeof(reply);
				int ret = wpa_ctrl_request(
					cfg.ap_vaps[i].ap_ctrl, cmd,
					strlen(cmd), reply, &rlen, NULL);
				if (ret) {
					rptr_mgr_printf("Legacy: ENABLE AP cmd"
							"failed ret = %d", ret);
				} else {
					rptr_mgr_printf(
					"REP_AP_ENABLE -> legacy AP %s "
					"ret=%d cmd=\"%s\"\n",
					cfg.ap_vaps[i].ifname, ret, cmd);
				}
				return;
			}
		} else {
			for (int j = 0; j < cfg.ap_vaps[i].link_count; j++) {
				if (cfg.ap_vaps[i].links[j].link_ctrl &&
				    (int) cfg.ap_vaps[i].links[j].hw_info.hw_idx ==
				    hw_idx) {
					char reply[128];
					size_t rlen = sizeof(reply);
					int ret = wpa_ctrl_request(
						cfg.ap_vaps[i].links[j].link_ctrl,
						cmd, strlen(cmd), reply,
						&rlen, NULL);
					if (ret) {
						rptr_mgr_printf("MLO: ENABLE AP cmd"
							"failed ret = %d", ret);
					} else {
						rptr_mgr_printf(
						"REP_AP_ENABLE -> AP Link %s "
						"ret=%d cmd=\"%s\"\n",
						cfg.ap_vaps[i].links[j].ifname,
						ret, cmd);
					}
					return;
				}
			}
		}
	}
}

/*
 * parse_signal_poll_and_enable_rep_ap - Parse signal poll and enable AP links
 *
 * Dependent Repeater: Send the cmd - MLO_SIGNAL_POLL/SIGNAL_POLL to
 * Rep STA(MLO/Legacy) in connected state to get the channel info.
 * Pass the parsed channel info to hostapd to bring up the corresponding
 * Rep AP links
 */
static void parse_signal_poll_and_enable_rep_ap(void)
{
	char rbuf[2048];
	size_t rlen = sizeof(rbuf) - 1;
	const char *cmd = "MLO_SIGNAL_POLL";
	int ret = wpa_ctrl_request(cfg.sta_vap.sta_ctrl, cmd,
			strlen(cmd), rbuf, &rlen, NULL);
	if (rlen >= sizeof(rbuf))
		rlen = sizeof(rbuf) - 1;
	rbuf[rlen] = '\0';

	if (strstr(rbuf, "FAIL") == NULL) {
		/* MLO case */
		if (rlen >= sizeof(rbuf))
			rlen = sizeof(rbuf) - 1;
		rbuf[rlen] = '\0';

		/* Parse per-link details for MLO and enable
		 * matching AP links
		 */
		char *copy = strdup(rbuf);
		rptr_mgr_printf("\nMLO_SIGNAL_POLL: copy = %s", copy);
		if (copy) {
			int link_id = -1;
			int rssi = 0, noise = 0;
			int freq = 0, linkspeed = 0;
			int width_mhz = 0, punc_bitmap = 0;
			int c_freq1 = 0, avg_rssi = 0;
			char *saveptr = NULL;
			char *line = strtok_r(copy, "\n", &saveptr);
			while (line) {
				if (!strncmp(line, "LINK_ID=", 8)) {
					if (link_id >= 0) {
						int hw_idx =
						wpa_get_hw_idx_by_freq(freq);
						if (hw_idx >= 0) {
							send_rep_ap_enable(
								hw_idx, freq,
								width_mhz,
								punc_bitmap,
								c_freq1, 0);
						}
						rptr_mgr_printf(
						"MLO_SIGNAL_POLL: link=%d "
						"RSSI=%d NOISE=%d FREQ=%d "
						"WIDTH=%d PUNCT=%d CF1=%d "
						"LINKSPEED=%d AVG_RSSI=%d\n",
						link_id, rssi, noise,
						freq, width_mhz,
						punc_bitmap, c_freq1,
						linkspeed, avg_rssi);
					}
					link_id = atoi(line + 8);
					rssi = noise = freq = linkspeed =
						width_mhz = punc_bitmap =
						c_freq1 = avg_rssi = 0;
				} else if (!strncmp(line, "RSSI=", 6)) {
					rssi = atoi(line + 6);
				} else if (!strncmp(line, "NOISE=", 7)) {
					noise = atoi(line + 7);
				} else if (!strncmp(line, "FREQUENCY=", 10)) {
					freq = atoi(line + 10);
				} else if (!strncmp(line, "LINKSPEED=", 10)) {
					linkspeed = atoi(line + 10);
				} else if (!strncmp(line, "WIDTH=", 6)) {
					width_mhz = atoi(line + 6);
				} else if (!strncmp(line, "PUNCT_BITMAP=", 13)) {
					punc_bitmap = (int) strtol(line + 13,
							      NULL, 0);
				} else if (!strncmp(line, "CENTER_FRQ1=", 12)) {
					c_freq1 = atoi(line + 12);
				} else if (!strncmp(line, "AVG_RSSI=", 9)) {
					avg_rssi = atoi(line + 9);
				}
				line = strtok_r(NULL, "\n", &saveptr);
			}
			if (link_id >= 0) {
				int hw_idx = wpa_get_hw_idx_by_freq(freq);
				if (hw_idx >= 0) {
					send_rep_ap_enable(hw_idx, freq,
								width_mhz,
								punc_bitmap,
								c_freq1, 0);
				}
				rptr_mgr_printf(
						"MLO_SIGNAL_POLL: link=%d RSSI=%d "
						"NOISE=%d FREQ=%d WIDTH=%d "
						"PUNCT=%d CF1=%d LINKSPEED=%d "
						"AVG_RSSI=%d\n",
						link_id, rssi, noise, freq,
						width_mhz, punc_bitmap,
						c_freq1, linkspeed,
						avg_rssi);
			}
			free(copy);
		}
	} else {
		/* Fallback to legacy SIGNAL_POLL */
		rlen = sizeof(rbuf) - 1;
		cmd = "SIGNAL_POLL";
		ret = wpa_ctrl_request(cfg.sta_vap.sta_ctrl, cmd,
						strlen(cmd), rbuf,
						&rlen, NULL);
		if (ret == 0) {
			if (rlen >= sizeof(rbuf))
				rlen = sizeof(rbuf) - 1;
			rbuf[rlen] = '\0';

			/* Parse legacy SIGNAL_POLL output to derive channel info */
			int freq = 0;
			int width_mhz = 0;
			int punc_bitmap = 0;
			int c_freq1 = 0;
			char *copy = strdup(rbuf);
			if (copy) {
				char *saveptr = NULL;
				char *line = strtok_r(copy, "\n", &saveptr);
				while (line) {
					if (!strncmp(line, "FREQUENCY=", 10)) {
						freq = atoi(line + 10);
					} else if (!strncmp(line, "WIDTH=", 6)) {
						width_mhz = atoi(line + 6);
					} else if (!strncmp(line, "PUNCT_BITMAP=", 13)) {
						punc_bitmap = (int) strtol(line + 13,
								NULL, 0);
					} else if (!strncmp(line, "CENTER_FRQ1=", 12)) {
						c_freq1 = atoi(line + 12);
					}
					line = strtok_r(NULL, "\n", &saveptr);
				}
				free(copy);
			}
			int hw_idx = wpa_get_hw_idx_by_freq(freq);
			if (hw_idx >= 0 && freq > 0) {
				send_rep_ap_enable(hw_idx, freq, width_mhz,
						punc_bitmap, c_freq1, 0);
				rptr_mgr_printf("SIGNAL_POLL: enabling APs with"
				"freq=%d width=%d punc=%d cf1=%d\n",
				freq, width_mhz, punc_bitmap, c_freq1);
			} else {
				rptr_mgr_printf("SIGNAL_POLL: invalid freq parsed;"
				"skipping REP_AP_ENABLE (freq=%d)\n", freq);
			}
		} else {
			rptr_mgr_printf("CTRL-EVENT-CONNECTED: SIGNAL_POLL"
					"request failed ret=%d\n", ret);
		}
	}
}

/*
 * mld_wpa_s_ctrl_iface_process - Recv events from Rep STA and process them
 *
 * - Recv events like CTRL-EVENT-SCAN-STARTED, AUTHENTICATING, CTRL-EVENT-CONNECTED
 *   and handle Dependent Repeater scenario
 * - Recv events like CTRL-EVENT-PRE-AUTHENTICATING and handle Independent
 *   Repeater scenario
 * - Recv events like CTRL-EVENT-STARTED-CHANNEL-SWITCH,
 *   CTRL-EVENT-STARTED-LINK-CHANNEL-SWITCH and handle CSA scenario
 */
static void mld_wpa_s_ctrl_iface_process(const char *msg)
{
	char links[512];
	char *ctx = NULL;
	char *link_tok;
	char *copy;

	 /* Skip optional priority prefix like "<3>" */
	const char *start = msg;
	if (start && start[0] == '<') {
		const char *gt = strchr(start, '>');
		if (gt && *(gt + 1))
			start = gt + 1;
	}

	if (!cfg.athnewind &&
	     ((os_strncmp(start, "CTRL-EVENT-PRE-CONNECTING ", 26) == 0) ||
	     (os_strncmp(start, "CTRL-EVENT-SCAN-STARTED ", 24) == 0) ||
	     (os_strncmp(start, "AUTHENTICATING", 14) == 0))) {
		/* Targeted disable: only AP endpoints whose radios appear
		 * in STA GET_FREQ_LIST
		 */
		disable_aps_for_sta_scan_or_auth();
	} else if (!cfg.athnewind &&
		   (os_strncmp(start, "CTRL-EVENT-CONNECTED ",
			       21) == 0)) {
		if (!cfg.sta_vap.sta_ctrl) {
			rptr_mgr_printf(
				"CTRL-EVENT-CONNECTED: STA ctrl unavailable; "
				"skipping SIGNAL_POLL\n");
		} else {
			parse_signal_poll_and_enable_rep_ap();
		}
	} else if (cfg.athnewind &&
		   (os_strncmp(start,
			       "CTRL-EVENT-PRE-CONNECTING ",
			       26) == 0)) {
		const char *kv = start + 26;
		while (*kv == ' ')
			kv++;

		/*
		 * One PRE-CONNECTING per MLD, with all
		 * link information aggregated in a single "links" key
		 * as a semicolon-separated list of tuples:
		 *   hw_idx,freq,c_freq1,c_freq2,width,punc_bitmap,is_dfs
		 */
		parse_kv_token(kv, "links", links, sizeof(links));
		if (links[0]) {
			copy = strdup(links);

			if (!copy)
				return;

			link_tok = strtok_r(copy, ";", &ctx);
			while (link_tok) {
				int hw_idx, freq, c_freq1, c_freq2;
				int width, punc_bitmap, is_dfs;
				int bandwidth = 20;
				int sec_channel_offset;
				int offset_mhz = 0;

				if (sscanf(link_tok, "%d,%d,%d,%d,%d,%d,%d",
					   &hw_idx, &freq, &c_freq1, &c_freq2,
					   &width, &punc_bitmap, &is_dfs) == 7) {
					switch (width) {
					case 1:
						bandwidth = 20;
						if (freq >= 2412 && freq <= 2472) {
							c_freq1 = freq;
						}
						break;
					case 2:
						bandwidth = 40;
						if (freq >= 2412 && freq <= 2472) {
							offset_mhz =
							(freq <= 2442) ? 10 : -10;
							c_freq1 = freq + offset_mhz;
						}
						break;
					case 3:
						bandwidth = 80;
						break;
					case 4:
						bandwidth = 80+80;
						break;
					case 5:
						bandwidth = 160;
						c_freq1 = c_freq2;
						c_freq2 = 0;
						break;
					case 10:
						bandwidth = 320;
						c_freq1 = c_freq2;
						c_freq2 = 0;
						break;
					default:
						bandwidth = 20;
						break;
					}

					sec_channel_offset = calc_sec_channel_offset(
						freq, c_freq1, bandwidth);
					/* Delegate common CHAN_SWITCH handling; set
					 * CSA bitmap for PRE-AUTH per link
					 */
					handle_chan_switch_event(freq, c_freq1, c_freq2,
								bandwidth,
								sec_channel_offset,
								hw_idx,
								punc_bitmap,
								is_dfs, 1);
				}

				link_tok = strtok_r(NULL, ";", &ctx);
			}
			free(copy);
		}

	} else if ((os_strncmp(start,
			       "CTRL-EVENT-STARTED-CHANNEL-SWITCH ",
			       34) == 0) ||
		   (os_strncmp(
			    start,
			    "CTRL-EVENT-STARTED-LINK-CHANNEL-SWITCH ",
			    39) == 0)) {
		const char *kv = start + 34;
		while (*kv == ' ')
			kv++;

		int freq = parse_kv_int(kv, "freq", 0);
		int ht_enabled = parse_kv_int(kv, "ht_enabled", 0);
		int ch_offset = parse_kv_int(kv, "ch_offset", 0);
		int cf1 = parse_kv_int(kv, "cf1", 0);
		int cf2 = parse_kv_int(kv, "cf2", 0);
		char ch_width[32];
		parse_kv_token(kv, "ch_width", ch_width,
			      sizeof(ch_width));

		int bandwidth = 20;
		if (ch_width[0]) {
			if (!os_strcmp(ch_width, "20") ||
			    !os_strcmp(ch_width, "HT20"))
				bandwidth = 20;
			else if (!os_strcmp(ch_width, "40") ||
				 !os_strcmp(ch_width, "HT40"))
				bandwidth = 40;
			else if (!os_strcmp(ch_width, "80"))
				bandwidth = 80;
			else if (!os_strcmp(ch_width, "160"))
				bandwidth = 160;
		}

		/* Handle both legacy and MLO in STARTED-CHAN-SWITCH */
		int hw_idx = wpa_get_hw_idx_by_freq(freq);
		handle_chan_switch_event(freq, cf1, cf2,
					bandwidth, ch_offset,
					hw_idx, -1, 0, 0);
	}
}

/*
 * mld_hostapd_ctrl_iface_process - Recv events from Rep AP/links and process them
 *
 * - Recv events like AP-CSA-FINISHED and DFS-CAC-COMPLETED handle skip_cac and
 *   Independent Repeater scenario to resume the Rep STA Authentication
 */
static void mld_hostapd_ctrl_iface_process(struct wpa_ctrl *src_ctrl,
			      const char *msg)
{
	/* Normalize message start by skipping optional "<N>" prefix */
	const char *start = msg;
	if (start && start[0] == '<') {
		const char *gt = strchr(start, '>');
		if (gt && *(gt + 1))
			start = gt + 1;
	}

	if (cfg.athnewind &&
		(os_strncmp(start, "CTRL-EVENT-CHANNEL-SWITCH ", 26) == 0)) {
		const char *kv = start + 26;
		int is_dfs = 0;

		while (*kv == ' ')
			kv++;
		int freq = parse_kv_int(kv, "freq", -1);
		is_dfs = is_dfs_freq(freq);
		int hw_idx = wpa_get_hw_idx_by_freq(freq);
		rptr_mgr_printf(
			"  -> parsed CSA freq=%d is_dfs=%d mapped "
			"hw_idx=%d\n",
			freq, is_dfs, hw_idx);

		/* Clear the AP-bit for this hw_idx as CSA finished for
		 * this interface
		 */
		if (hw_idx >= 0 && hw_idx < MAX_MLD_LINKS_PER_AP) {
			for (int i = 0; i < cfg.ap_vap_count; i++) {
				/* Legacy AP case: event from AP interface */
				if (cfg.ap_vaps[i].ap_ctrl == src_ctrl) {
					cfg.csa_bitmap[hw_idx] &= ~BIT(i);
					rptr_mgr_printf(
						"  -> cleared CSA bit for "
						"hw_idx=%d ap=%d (legacy); "
						"bitmap now=0x%x\n",
						hw_idx, i,
						cfg.csa_bitmap[hw_idx]);
					break;
				}
				/* MLO case: event from a link interface */
				for (int j = 0;
				     j < cfg.ap_vaps[i].link_count; j++) {
					if (cfg.ap_vaps[i].links[j].link_ctrl ==
					    src_ctrl) {
						cfg.csa_bitmap[hw_idx] &= ~BIT(i);
						rptr_mgr_printf(
							"  -> cleared CSA bit for "
							"hw_idx=%d ap=%d link=%d; "
							"bitmap now=0x%x\n",
							hw_idx, i, j,
							cfg.csa_bitmap[hw_idx]);
						break;
					}
				}
			}
		}

		/* If pre-auth is active for this hw_idx and all APs
		 * finished CSA, notify STA
		 */
		if (hw_idx >= 0 && hw_idx < MAX_MLD_LINKS_PER_AP &&
		    (!is_dfs || (is_dfs && cfg.skip_cac)) &&
		    cfg.pre_auth_active[hw_idx] &&
		    cfg.csa_bitmap[hw_idx] == 0) {
			if (cfg.sta_vap.sta_ctrl) {
				char sw_done[64];
				snprintf(sw_done, sizeof(sw_done),
					 "CHAN_SW_FINISHED_NOTIFY freq =%d",
					 freq);
				char reply[128];
				size_t rlen = sizeof(reply);
				int ret = wpa_ctrl_request(
					cfg.sta_vap.sta_ctrl, sw_done,
					strlen(sw_done), reply,
					&rlen, NULL);
				if (ret) {
					rptr_mgr_printf(
					"buf = %s cmd failed ret = %d",
					sw_done, ret);
					return;
				} else {
					rptr_mgr_printf(
					"  -> notified STA %s ret=%d "
					"(all CSA finished for hw_idx=%d) "
					"msg=\"%s\"\n",
					cfg.sta_vap.ifname, ret,
					hw_idx, sw_done);
				}
			}
			cfg.pre_auth_active[hw_idx] = 0;
		}
	} else if (cfg.athnewind &&
		   (os_strncmp(start, "DFS-CAC-COMPLETED ", 18) == 0)) {
		/* Received once per CAC completion; notify STA with freq */
		const char *kv = start + 18;
		while (*kv == ' ')
			kv++;
		int freq = parse_kv_int(kv, "freq", -1);
		int hw_idx = wpa_get_hw_idx_by_freq(freq);
		if (hw_idx >= 0 && hw_idx < MAX_MLD_LINKS_PER_AP &&
		    cfg.pre_auth_active[hw_idx]) {
			if (cfg.sta_vap.sta_ctrl && freq > 0) {
				char sw_done[64];
				int hw_idx2 = wpa_get_hw_idx_by_freq(freq);
				rptr_mgr_printf(
					"  -> DFS-CAC-COMPLETED parsed "
					"freq=%d hw_idx=%d\n",
					freq, hw_idx2);
				snprintf(sw_done, sizeof(sw_done),
					 "CHAN_SW_FINISHED_NOTIFY freq =%d",
					 freq);
				char reply[128];
				size_t rlen = sizeof(reply);
				int ret = wpa_ctrl_request(
					cfg.sta_vap.sta_ctrl, sw_done,
					strlen(sw_done), reply,
					&rlen, NULL);
				if (ret) {
					rptr_mgr_printf(
					"buf = %s cmd failed ret = %d",
					sw_done, ret);
					return;
				} else {
					rptr_mgr_printf(
					"  -> notified STA %s ret=%d "
					"(DFS-CAC-COMPLETED) msg=\"%s\"\n",
					cfg.sta_vap.ifname, ret, sw_done);
				}
				cfg.csa_bitmap[hw_idx] = 0;
			} else {
				rptr_mgr_printf(
					"  -> STA ctrl unavailable or invalid "
					"freq for DFS-CAC-COMPLETED (freq=%d)\n",
					freq);
			}
			cfg.pre_auth_active[hw_idx] = 0;
		}
	}
}

void print_rptr_interfaces(struct mld_config *cfg)
{
	rptr_mgr_printf("STA VAP: %s", cfg->sta_vap.ifname);
	for (int i = 0; i < cfg->ap_vap_count; i++) {
		rptr_mgr_printf("AP VAP %d: %s", i,
				cfg->ap_vaps[i].ifname);
		for (int j = 0; j < cfg->ap_vaps[i].link_count; j++) {
			rptr_mgr_printf("  Link %d: %s", j,
					cfg->ap_vaps[i].links[j].ifname);
		}
	}

	rptr_mgr_printf("cfg->rptr_mgr_comm_type = %d cfg->athnewind = %d "
			"cfg->skip_cac = %d", cfg->rptr_mgr_comm_type,
			cfg->athnewind, cfg->skip_cac);
}

/*
 * parse_update_mld_config - Parse the rptr_mgr conf and store them in global cfg
 *
 * Parse the /var/run/rptr_mgr.conf and fetch the Rep STA and Rep AP MLD/link
 * ifnames, rptr_mgr mode, athnewind, skip_cac, etc. Store them in global cfg
 * and use them for creating sockets and further processing cmds/events
 */
int parse_update_mld_config(struct mld_config *cfg)
{
	char line[256];
	int ap_index = -1;
	FILE *fp = fopen("/var/run/rptr_mgr.conf", "r");

	if (!fp) {
		perror("Failed to open /var/run/rptr_mgr.conf");
		return 1;
	}

	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "mld_ap=", 7) == 0) {
			ap_index++;
			if (ap_index >= MAX_MLD_AP_VAPS) {
				rptr_mgr_printf("Too many AP VAPs\n");
				break;
			}
			char *ap_name = line + 7;
			ap_name[strcspn(ap_name, "\n")] = '\0';
			mld_ap_vap_init(&cfg->ap_vaps[ap_index], ap_name);
		} else if (strncmp(line, "links=", 6) == 0 &&
			   ap_index >= 0) {
			char *saveptr = NULL;
			char *token = strtok_r(line + 6, " \n", &saveptr);
			int link_idx = 0;
			while (token && link_idx < MAX_MLD_LINKS_PER_AP) {
				mld_link_iface_init(
					&cfg->ap_vaps[ap_index].links[link_idx],
					token, link_idx);
				link_idx++;
				token = strtok_r(NULL, " \n", &saveptr);
			}
			cfg->ap_vaps[ap_index].link_count = link_idx;
		} else if (strncmp(line, "mld_sta=", 8) == 0) {
			char *sta_name = line + 8;
			sta_name[strcspn(sta_name, "\n")] = '\0';
			mld_sta_vap_init(&cfg->sta_vap, sta_name);
		} else if (strncmp(line, "comm_type=", 10) == 0) {
			cfg->rptr_mgr_comm_type = atoi(line + 10);
		} else if (strncmp(line, "athnewind=", 10) == 0) {
			cfg->athnewind = atoi(line + 10);
		} else if (strncmp(line, "skip_cac=", 9) == 0) {
			cfg->skip_cac = atoi(line + 9);
		}
	}

	cfg->ap_vap_count = ap_index + 1;
	fclose(fp);
	return 0;
}

/*
 * store_pid - Store the rptr_mgr daemon pid
 */
int store_pid(const char *pid_file)
{
	if (pid_file) {
		int f = open(pid_file, O_WRONLY | O_CREAT);
		int pid = getpid();
		char buf[MAX_PID_LEN] = { 0 };
		snprintf(buf, MAX_PID_LEN, "%d", pid);
		if (f) {
			if (write(f, buf, strlen(buf)) != strlen(buf)) {
				rptr_mgr_printf("Failed to write pid file");
				return -1;
			}
			close(f);
			return 0;
		}
	}
	return -1;
}

/*
 * rptr_mgr_terminate - Rptr mgr clean-up process
 *
 * Stop event loop to avoid further callbacks during cleanup
 * Perform MLD resource cleanup
 * Unregister sockets, detach/close wpa_ctrl, and clear cfg state
 */
static void rptr_mgr_terminate(int sig)
{
	rptr_mgr_printf("terminate: signal=%d, cleaning up resources", sig);

	eloop_terminate();

	for (int i = 0; i < cfg.ap_vap_count; i++) {
		if (cfg.ap_vaps[i].ap_ctrl) {
			int fd = wpa_ctrl_get_fd(cfg.ap_vaps[i].ap_ctrl);
			eloop_unregister_read_sock(fd);
			wpa_ctrl_detach(cfg.ap_vaps[i].ap_ctrl);
			wpa_ctrl_close(cfg.ap_vaps[i].ap_ctrl);
			cfg.ap_vaps[i].ap_ctrl = NULL;
		}
		for (int j = 0; j < cfg.ap_vaps[i].link_count; j++) {
			if (cfg.ap_vaps[i].links[j].link_ctrl) {
				int fd = wpa_ctrl_get_fd(cfg.ap_vaps[i].links[j].link_ctrl);
				eloop_unregister_read_sock(fd);
				wpa_ctrl_detach(cfg.ap_vaps[i].links[j].link_ctrl);
				wpa_ctrl_close(cfg.ap_vaps[i].links[j].link_ctrl);
				cfg.ap_vaps[i].links[j].link_ctrl = NULL;
			}
		}
	}
	if (cfg.sta_vap.sta_ctrl) {
		int fd = wpa_ctrl_get_fd(cfg.sta_vap.sta_ctrl);
		eloop_unregister_read_sock(fd);
		wpa_ctrl_detach(cfg.sta_vap.sta_ctrl);
		wpa_ctrl_close(cfg.sta_vap.sta_ctrl);
		cfg.sta_vap.sta_ctrl = NULL;
	}
	memset(cfg.csa_bitmap, 0, sizeof(cfg.csa_bitmap));
	memset(cfg.pre_auth_active, 0, sizeof(cfg.pre_auth_active));
	cfg.ap_vap_count = 0;

	exit(0);
}

void rptr_mgr_debug_open_syslog(void)
{
	openlog("rptr-mgr", LOG_PID | LOG_NDELAY, LOG_USER);
}

/*
 * rptr_mgr_initialize_sock_for_ap - Initialize sockets for
 * AP interfaces (legacy or MLO AP link)
 */
void rptr_mgr_initialize_sock_for_ap(void)
{
	for (int i = 0; i < cfg.ap_vap_count; i++) {
		cfg.ap_vaps[i].ap_ctrl =
			hostapd_ctrl_open_attach(cfg.ap_vaps[i].ifname);
		if (!cfg.ap_vaps[i].ap_ctrl) {
			rptr_mgr_printf(
				"Failed to initialize AP MLD socket for %s",
				cfg.ap_vaps[i].ifname);
		} else {
			int ap_fd =
				wpa_ctrl_get_fd(cfg.ap_vaps[i].ap_ctrl);
			rptr_mgr_printf(
				"Initialized AP MLD socket: if=%s fd=%d",
				cfg.ap_vaps[i].ifname, ap_fd);
			eloop_register_read_sock(ap_fd, ap_sock_handler,
					       NULL,
					       cfg.ap_vaps[i].ap_ctrl);
		}

		if (cfg.ap_vaps[i].link_count == 0) {
			/* Legacy AP: query hw info on interface */
			const char *ap_ifname = cfg.ap_vaps[i].ifname;
			if (cfg.ap_vaps[i].ap_ctrl) {
				(void) query_and_store_hw_info(
					cfg.ap_vaps[i].ap_ctrl,
					&cfg.ap_vaps[i].hw_info,
					ap_ifname, 1);
				(void) query_get_channel_info(
					cfg.ap_vaps[i].ap_ctrl,
					&cfg.ap_vaps[i].ch_info,
					ap_ifname, 1);
			}
		} else {
			/* MLO AP: setup link sockets and query per-link hw */
			for (int j = 0; j < cfg.ap_vaps[i].link_count; j++) {
				const char *link_ifname =
					cfg.ap_vaps[i].links[j].ifname;
				cfg.ap_vaps[i].links[j].link_ctrl =
					hostapd_ctrl_open_attach(link_ifname);
				if (!cfg.ap_vaps[i].links[j].link_ctrl) {
					rptr_mgr_printf(
						"Failed to initialize AP MLD Link "
						"socket for %s",
						link_ifname);
				} else {
					int link_fd = wpa_ctrl_get_fd(
						cfg.ap_vaps[i].links[j].link_ctrl);
					rptr_mgr_printf(
						"Initialized AP MLD Link socket: "
						"if=%s fd=%d",
						link_ifname, link_fd);
					eloop_register_read_sock(
						link_fd, ap_sock_handler,
						NULL,
						cfg.ap_vaps[i].links[j].link_ctrl);
					(void) query_and_store_hw_info(
						cfg.ap_vaps[i].links[j].link_ctrl,
						&cfg.ap_vaps[i].links[j].hw_info,
						link_ifname, 0);
					(void) query_get_channel_info(
						cfg.ap_vaps[i].links[j].link_ctrl,
						&cfg.ap_vaps[i].links[j].ch_info,
						link_ifname, 0);
				}
			}
		}
	}
}

/*
 * rptr_mgr_initialize_sock_for_sta - Initialize socket for STA VAP
 * via wpa_supplicant
 */
void rptr_mgr_initialize_sock_for_sta(void)
{
	if (cfg.sta_vap.ifname[0] != '\0') {
		cfg.sta_vap.sta_ctrl =
			wpa_supp_ctrl_open_attach(cfg.sta_vap.ifname);
		if (!cfg.sta_vap.sta_ctrl) {
			rptr_mgr_printf(
				"Failed to initialize STA MLD socket "
				"for %s",
				cfg.sta_vap.ifname);
		} else {
			int sta_fd =
				wpa_ctrl_get_fd(cfg.sta_vap.sta_ctrl);
			rptr_mgr_printf(
				"Initialized STA MLD socket: if=%s fd=%d",
				cfg.sta_vap.ifname, sta_fd);
			eloop_register_read_sock(
				sta_fd, sta_sock_handler, NULL,
				cfg.sta_vap.sta_ctrl);
		}
	}
}

int main(int argc, char *argv[])
{
	const char *pid_file = "/var/run/rptr_mgr.pid";
	int ret = 0;

	if (store_pid(pid_file))
		rptr_mgr_printf(
			"Warning: Unable to store PID in file %s",
			pid_file);

	if (eloop_init()) {
		rptr_mgr_printf("Failed to initialize event loop");
		return 1;
	}

	mld_config_init(&cfg);
	parse_update_mld_config(&cfg);

	if (cfg.rptr_mgr_comm_type == 1) {
		/* Initialize syslog, ubus, and ucode for rptr_mgr */
		rptr_mgr_printf(
			"%s: initializing rptr_mgr ubus + ucode",
			__func__);
		rptr_mgr_debug_open_syslog();
		rptr_mgr_ubus_init();
		rptr_mgr_ucode_init(NULL);
	} else if (cfg.rptr_mgr_comm_type == 2) {
		/* Socket-based communication */
		rptr_mgr_printf("initializing rptr_mgr Sock mode");
		print_rptr_interfaces(&cfg);
		rptr_mgr_initialize_sock_for_ap();
		rptr_mgr_initialize_sock_for_sta();
	} else {
		rptr_mgr_printf("Invalid mode of communication");
	}

	signal(SIGINT, rptr_mgr_terminate);
	signal(SIGTERM, rptr_mgr_terminate);

	/* Run event loop; returns when eloop_terminate() is called */
	eloop_run();

	/* Graceful cleanup after event loop exits */
	rptr_mgr_ucode_free();
	rptr_mgr_terminate(0);
	return 0;
}
