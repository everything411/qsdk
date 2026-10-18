/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "includes.h"
#include <sys/types.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/errqueue.h>
#include "common.h"
#include "eloop.h"
#include "common/qca-vendor.h"
#include "common/qca-vendor-attr.h"
#include "common/brcm_vendor.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "drivers/driver.h"
#include "drivers/driver_nl80211.h"
#include "ap/hostapd.h"
#include "ap/hw_features.h"
#include "esp.h"
#include "cbs.h"
#include "dcs.h"
#include "rropinfo.h"
#include "reg_extn.h"
#include "mapc_extn.h"

struct hostapd_sta_add_params;

static int mac_config_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wpabuf *buf = arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *ven_reply, *nl;
	int rem_len;

	if (!buf)
		return NL_SKIP;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	ven_reply = tb[NL80211_ATTR_VENDOR_DATA];
	if (!ven_reply)
		return NL_SKIP;

	if (nla_len(ven_reply) > wpabuf_tailroom(buf)) {
		wpa_printf(MSG_ERROR, "nl80211: no buffer space");
		return NL_SKIP;
	}

	nla_for_each_nested(nl, ven_reply, rem_len) {
		wpabuf_put_data(buf, nla_data(nl), nla_len(nl));
	}

	return NL_SKIP;
}

static int
nl80211_get_6ghz_thresh_priority_freq_handler(struct nl_msg *msg,
					      void *arg)
{
	u16 *val = arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *vendor_data;
	struct nlattr *thresh_attr;

	if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to parse netlink attributes");
		return NL_SKIP;
	}

	vendor_data = tb[NL80211_ATTR_VENDOR_DATA];
	if (vendor_data) {
		struct nlattr *vendor_tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];

		if (nla_parse(vendor_tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			      nla_data(vendor_data), nla_len(vendor_data),
			      NULL)) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to parse netlink attributes");
			return NL_SKIP;
		}

		thresh_attr = vendor_tb[QCA_WLAN_VENDOR_ATTR_CONFIG_6GHZ_VLP_PRIORITY_THRESH_FREQ];
		if (thresh_attr)
			*val = nla_get_u16(thresh_attr);
	}

	return NL_SKIP;
}

int hostapd_get_6ghz_thresh_priority_freq_extn(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd;
	struct i802_bss *bss;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg = NULL;
	struct nlattr *params;
	u8 radio_idx = NL80211_WIPHY_RADIO_ID_MAX;
	u16 val = 0;
	unsigned int i;
	int ret;

	if (!iface || !iface->bss || !iface->bss[0]) {
		wpa_printf(MSG_ERROR, "invalid iface for threshold fetch");
		return -EINVAL;
	}

	hapd = iface->bss[0];
	bss = hapd->drv_priv;
	if (!bss || !bss->drv) {
		wpa_printf(MSG_ERROR, "driver not initialized");
		return -ENODEV;
	}

	drv = bss->drv;

	if (iface->num_multi_hws) {
		hostapd_set_current_hw_info(iface, iface->freq);
		if (iface->current_hw_info) {
			radio_idx = iface->current_hw_info->hw_idx;
		} else {
			for (i = 0; i < iface->num_multi_hws; i++) {
				if (iface->multi_hw_info[i].start_freq >= 5945 &&
				    iface->multi_hw_info[i].end_freq <= 7125) {
					radio_idx = iface->multi_hw_info[i].hw_idx;
					wpa_printf(MSG_DEBUG,
						   "fallback selected first 6 GHz hw_idx=%u",
						   radio_idx);
					break;
				}
			}

			if (radio_idx == NL80211_WIPHY_RADIO_ID_MAX) {
				wpa_printf(MSG_DEBUG, "no radio_idx");
				return -EINVAL;
			}
		}
	}

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u16(msg,
			QCA_WLAN_VENDOR_ATTR_CONFIG_6GHZ_VLP_PRIORITY_THRESH_FREQ,
			0))
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX, radio_idx))
		goto fail;

	nla_nest_end(msg, params);

	ret = send_and_recv_resp(drv, msg,
				 nl80211_get_6ghz_thresh_priority_freq_handler,
				 &val);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "failed to get 6 GHz threshold freq ret=%d", ret);
		iface->iface_extn.vlp_threshold_freq = 0;
		return ret;
	}

	iface->iface_extn.vlp_threshold_freq = val;
	wpa_printf(MSG_DEBUG, "6 GHz VLP threshold freq=%u", val);

	return 0;

fail:
	if (msg)
		nlmsg_free(msg);
	iface->iface_extn.vlp_threshold_freq = 0;
	return -EINVAL;
}

static int nl80211_get_agile_capable_handler(struct nl_msg *msg, void *arg)
{
	u8 *val = arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *vendor_data;

	if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to parse netlink attributes");
		return NL_SKIP;
	}

	vendor_data = tb[NL80211_ATTR_VENDOR_DATA];
	if (vendor_data) {
		struct nlattr *vendor_tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];

		if (nla_parse(vendor_tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			      nla_data(vendor_data), nla_len(vendor_data),
			      NULL)) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to parse vendor attrs");
			return NL_SKIP;
		}

		if (vendor_tb[QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE])
			*val = nla_get_u8(vendor_tb[QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE]);
	}

	return NL_SKIP;
}

int hostapd_get_agile_capable_extn(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd;
	struct i802_bss *bss;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg = NULL;
	struct nlattr *params;
	u8 radio_idx = 0;
	u8 val = 1;
	int ret;

	if (!iface || !iface->bss || !iface->bss[0]) {
		wpa_printf(MSG_ERROR, "invalid iface for agile capable fetch");
		return -EINVAL;
	}

	hapd = iface->bss[0];
	bss = hapd->drv_priv;
	if (!bss || !bss->drv) {
		wpa_printf(MSG_ERROR, "driver not initialized");
		return -ENODEV;
	}

	drv = bss->drv;

	if (iface->num_multi_hws) {
		if (!iface->current_hw_info) {
			if (hostapd_set_current_hw_info(iface, iface->freq)) {
				wpa_printf(MSG_DEBUG,
					   "agile capable: no current_hw_info, skipping query");
				return 0;
			}
		}
		radio_idx = iface->current_hw_info->hw_idx;
	}
	/* else: single radio — query for radio_idx 0 */

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE, 0))
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX, radio_idx))
		goto fail;

	nla_nest_end(msg, params);

	ret = send_and_recv_resp(drv, msg, nl80211_get_agile_capable_handler,
				 &val);
	if (ret) {
		wpa_printf(MSG_ERROR, "failed to get agile capable ret=%d", ret);
		iface->iface_extn.agile_capable = true;
		return ret;
	}

	iface->iface_extn.agile_capable = !!val;
	wpa_printf(MSG_DEBUG, "agile capable=%u radio_idx=%u",
		   iface->iface_extn.agile_capable, radio_idx);

	return 0;

fail:
	if (msg)
		nlmsg_free(msg);
	iface->iface_extn.agile_capable = true;
	return -EINVAL;
}

/* This function sends a NL message only if extension parameters exist;
 * otherwise it just returns.
 */

void wpa_driver_nl80211_sta_add_extn(void *priv,
				     struct hostapd_sta_add_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_240mhz_vendor_oper_extn *oper;
	struct nl_msg *msg;
	struct nlattr *attr;
	int ret;

	/*currently added only for 240mhz operation*/

	if (!params->params_extn.params_240mhz.eht_240mhz_capab)
		return;

	oper = params->params_extn.params_240mhz.eht_240mhz_capab;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO))
		goto fail;


	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;

	/* Add the Vendor-specific attributes*/
	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS,
		       oper->bfmess320mhz) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS,
		       oper->numsound320mhz) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP, 3,
		    oper->mcs_map_320mhz) ||
	    (oper->nonofdmaulmumimo320mhz &&
	    nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO))
	    || (oper->mubfmr320mhz &&
	    nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER)))
		goto fail;

	nla_nest_end(msg, attr);

	if (nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ1, oper->ccfs0) ||
	    nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ2, oper->ccfs1) ||
	    nla_put_u32(msg, NL80211_ATTR_PUNCT_BITMAP, oper->punct_bitmap))
		goto fail;

	if (params->mld_link_addr) {
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN,
			    params->mld_link_addr))
			goto fail;
	} else {
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN,
			    params->addr))
			goto fail;
	}

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to send 240 MHz Vendor NL command: %s, %d",
			   strerror(-ret), ret);
		goto fail;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: 240MHz Vendor Oper: ccfs0=%u, ccfs1=%u, punctured=0x%04x, "
		   "is5ghz240mhz=%u, bfmess320mhz=%u, numsound320mhz=%u, "
		   "nonofdmaulmumimo320mhz=%u, mubfmr320mhz=%u, "
		   "mcs_map_320mhz=[0x%02x, 0x%02x, 0x%02x]",
		   oper->ccfs0,
		   oper->ccfs1,
		   oper->punct_bitmap,
		   oper->is5ghz240mhz,
		   oper->bfmess320mhz,
		   oper->numsound320mhz,
		   oper->nonofdmaulmumimo320mhz,
		   oper->mubfmr320mhz,
		   oper->mcs_map_320mhz[0],
		   oper->mcs_map_320mhz[1],
		   oper->mcs_map_320mhz[2]);

	return;

fail:
	nlmsg_free(msg);
	return;
}

static u16 nl80211_hw_blocked_chans_bw_to_mhz_extn(u32 max_bw)
{
	switch (max_bw) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		return 20;
	case NL80211_CHAN_WIDTH_40:
		return 40;
	case NL80211_CHAN_WIDTH_80:
		return 80;
	case NL80211_CHAN_WIDTH_160:
		return 160;
	case NL80211_CHAN_WIDTH_320:
		return 320;
	default:
		return 0;
	}
}

static u32 nl80211_hw_blocked_chans_puncture_mask_extn(u32 max_bw)
{
	switch (max_bw) {
	case NL80211_CHAN_WIDTH_80:
		return 0x0000000F;
	case NL80211_CHAN_WIDTH_160:
		return 0x00000FFF;
	case NL80211_CHAN_WIDTH_320:
		return 0x00FFFFFF;
	default:
		return 0;
	}
}

static bool nl80211_hw_blocked_chans_valid_pwr_mode_extn(u8 pwr_mode_id)
{
	return (pwr_mode_id == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR ||
		pwr_mode_id == HE_REG_INFO_6GHZ_AP_TYPE_SP ||
		pwr_mode_id == HE_REG_INFO_6GHZ_AP_TYPE_VLP);
}

static int
qca_nl80211_parse_hw_blocked_chans_fb_chans_extn(
	struct nlattr *fb_list,
	struct hostapd_hw_blocklist_pwr_mode *pwr_mode)
{
	struct nlattr *nl;
	struct nlattr *fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_MAX + 1];
	struct hostapd_hw_blocklist_fb_chan *fb_chans = NULL;
	int rem;
	u32 count = 0;

	nla_for_each_nested(nl, fb_list, rem) {
		struct hostapd_hw_blocklist_fb_chan *new_fb_chans;

		if (nla_parse_nested(fb,
				     QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_MAX,
				     nl, NULL)) {
			wpa_printf(MSG_ERROR,
				   "nl80211: Invalid HW blocked channels FB channel entry");
			os_free(fb_chans);
			return -EINVAL;
		}

		new_fb_chans = os_realloc_array(fb_chans, count + 1,
						sizeof(*new_fb_chans));
		if (!new_fb_chans) {
			os_free(fb_chans);
			return -ENOMEM;
		}

		fb_chans = new_fb_chans;
		os_memset(&fb_chans[count], 0, sizeof(fb_chans[count]));

		if (fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_BW]) {
			u32 max_bw =
				nla_get_u32(fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_BW]);

			if (!nl80211_hw_blocked_chans_bw_to_mhz_extn(max_bw)) {
				wpa_printf(MSG_ERROR,
					   "nl80211: Invalid HW blocked channels FB max_bw %u",
					   max_bw);
				os_free(fb_chans);
				return -EINVAL;
			}
			fb_chans[count].max_bw = max_bw;
		}
		if (fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_CENTER_FREQ])
			fb_chans[count].center_freq =
				nla_get_u16(fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_CENTER_FREQ]);
		if (fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_PRI20_BITMAP])
			fb_chans[count].pri20_bitmap =
				nla_get_u16(fb[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_PRI20_BITMAP]);
		wpa_printf(MSG_DEBUG,
			   "nl80211: HW blocked channels FB chan[%u]: pri20_bitmap=0x%x center_freq=%u max_bw=%u",
			   count, fb_chans[count].pri20_bitmap,
			   fb_chans[count].center_freq, fb_chans[count].max_bw);
		count++;
	}

	pwr_mode->fb_chans = fb_chans;
	pwr_mode->num_fb_chans = count;
	return 0;
}

static int
qca_nl80211_parse_hw_blocked_chans_pc_chans_extn(
	struct nlattr *pc_list,
	struct hostapd_hw_blocklist_pwr_mode *pwr_mode)
{
	struct nlattr *nl;
	struct nlattr *pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_MAX + 1];
	struct hostapd_hw_blocklist_pc_chan *pc_chans = NULL;
	int rem;
	u32 count = 0;

	nla_for_each_nested(nl, pc_list, rem) {
		struct hostapd_hw_blocklist_pc_chan *new_pc_chans;

		if (nla_parse_nested(pc,
				     QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_MAX,
				     nl, NULL)) {
			wpa_printf(MSG_ERROR,
				   "nl80211: Invalid HW blocked channels PC channel entry");
			os_free(pc_chans);
			return -EINVAL;
		}

		new_pc_chans = os_realloc_array(pc_chans, count + 1,
						sizeof(*new_pc_chans));
		if (!new_pc_chans) {
			os_free(pc_chans);
			return -ENOMEM;
		}

		pc_chans = new_pc_chans;
		os_memset(&pc_chans[count], 0, sizeof(pc_chans[count]));

		if (pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW]) {
			u32 max_bw =
				nla_get_u32(pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW]);

			if (!nl80211_hw_blocked_chans_bw_to_mhz_extn(max_bw)) {
				wpa_printf(MSG_ERROR,
					   "nl80211: Invalid HW blocked channels PC max_bw %u",
					   max_bw);
				os_free(pc_chans);
				return -EINVAL;
			}
			pc_chans[count].max_bw = max_bw;
		}
		if (pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_CENTER_FREQ])
			pc_chans[count].center_freq =
				nla_get_u16(pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_CENTER_FREQ]);
		if (pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_PUNC_PATTERN_BITMAP])
			/* Bitmap of puncture patterns (BIT(idx)). */
			pc_chans[count].punc_pat_bmap =
				nla_get_u32(pc[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_PUNC_PATTERN_BITMAP]);
		if (pc_chans[count].punc_pat_bmap &&
		    (pc_chans[count].punc_pat_bmap &
		     ~nl80211_hw_blocked_chans_puncture_mask_extn(
			     pc_chans[count].max_bw))) {
			wpa_printf(MSG_ERROR,
				   "nl80211: Invalid HW blocked channels PC puncture-pattern bitmap 0x%x bw=%u",
				   pc_chans[count].punc_pat_bmap,
				   pc_chans[count].max_bw);
			os_free(pc_chans);
			return -EINVAL;
		}

		wpa_printf(MSG_DEBUG,
			   "nl80211: HW blocked channels PC chan[%u]: center_freq=%u max_bw=%u puncture_pattern_bitmap=0x%x",
			   count, pc_chans[count].center_freq,
			   pc_chans[count].max_bw, pc_chans[count].punc_pat_bmap);
		count++;
	}

	pwr_mode->pc_chans = pc_chans;
	pwr_mode->num_pc_chans = count;
	return 0;
}

static void
qca_nl80211_free_hw_blocked_chans_info_extn(
	struct hostapd_hw_blocklist_info *hw_blocklist_info)
{
	u8 i;

	if (!hw_blocklist_info || !hw_blocklist_info->pwr_modes)
		return;

	for (i = 0; i < hw_blocklist_info->num_pwr_modes; i++) {
		os_free(hw_blocklist_info->pwr_modes[i].fb_chans);
		os_free(hw_blocklist_info->pwr_modes[i].pc_chans);
	}

	os_free(hw_blocklist_info->pwr_modes);
	hw_blocklist_info->pwr_modes = NULL;
	hw_blocklist_info->num_pwr_modes = 0;
}

static int
qca_nl80211_handle_hw_blocked_chans_info_extn(struct i802_bss *bss,
					      struct nlattr **attr,
					      bool check_first_bss)
{
	union wpa_event_data event;
	struct hostapd_hw_blocklist_info hw_blocklist_info;
	struct nlattr *band_nl;
	struct nlattr *nl;
	struct hostapd_hw_blocklist_pwr_mode *new_pwr_modes;
	u32 band_id;
	u32 num_pwr_modes = 0;
	int band_rem;
	int rem;
	int hw_idx;
	int ret = 0;

	if (!attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_RADIO_INDEX]) {
		wpa_printf(MSG_ERROR,
			   "nl80211: HW blocked channels payload missing radio index");
		return -EINVAL;
	}

	hw_idx = nla_get_s32(attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_RADIO_INDEX]);
	if (hw_idx < 0 || hw_idx > NL80211_WIPHY_RADIO_ID_MAX) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Invalid HW blocked channels radio index %d",
			   hw_idx);
		return -EINVAL;
	}
	if (check_first_bss && bss != bss->drv->first_bss) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Ignore HW blocked channels payload for hw_idx %u on %s",
			   hw_idx, bss->ifname);
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: Handling HW blocked channels payload for hw_idx %u on %s",
		   hw_idx, bss->ifname);

	os_memset(&hw_blocklist_info, 0, sizeof(hw_blocklist_info));
	hw_blocklist_info.hw_idx = (u8)hw_idx;

	if (!attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_BAND_LIST]) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: HW blocked channels payload has no band list");
		goto notify;
	}

	nla_for_each_nested(band_nl,
			    attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_BAND_LIST],
			    band_rem) {
		struct nlattr *band[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_MAX + 1];

		if (nla_parse_nested(band,
				     QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_MAX,
				     band_nl, NULL)) {
			wpa_printf(MSG_ERROR,
				   "nl80211: Invalid HW blocked channels band entry");
			ret = -EINVAL;
			goto fail;
		}

		if (!band[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_ID]) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Skip HW blocked channels band entry without band ID");
			continue;
		}

		band_id =
			nla_get_u32(band[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_ID]);
		if (band_id != NL80211_BAND_6GHZ) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Ignore HW blocked channels band %u (6 GHz only)",
				   band_id);
			continue;
		}

		if (!band[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PWR_MODE_LIST]) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: 6 GHz blocked channels band entry has no power-mode list");
			continue;
		}

		nla_for_each_nested(nl,
				    band[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PWR_MODE_LIST],
				    rem) {
			struct hostapd_hw_blocklist_pwr_mode *pwr_mode;
			struct nlattr *mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_MAX + 1];
			u8 pwr_mode_id;

			if (nla_parse_nested(mode,
					     QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_MAX,
					     nl, NULL)) {
				wpa_printf(MSG_ERROR,
					   "nl80211: Invalid HW blocked channels power-mode entry");
				ret = -EINVAL;
				goto fail;
			}

			if (!mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_ID]) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Skip HW blocked channels power-mode entry without power-mode ID");
				continue;
			}

			new_pwr_modes = os_realloc_array(hw_blocklist_info.pwr_modes,
							 num_pwr_modes + 1,
							 sizeof(*new_pwr_modes));
			if (!new_pwr_modes) {
				ret = -ENOMEM;
				goto fail;
			}

			hw_blocklist_info.pwr_modes = new_pwr_modes;
			pwr_mode = &hw_blocklist_info.pwr_modes[num_pwr_modes];
			os_memset(pwr_mode, 0, sizeof(*pwr_mode));
			hw_blocklist_info.num_pwr_modes = num_pwr_modes + 1;

			pwr_mode_id =
				nla_get_u8(mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_ID]);
			if (!nl80211_hw_blocked_chans_valid_pwr_mode_extn(pwr_mode_id)) {
				wpa_printf(MSG_ERROR,
					   "nl80211: Invalid HW blocked channels power-mode ID %u",
					   pwr_mode_id);
				ret = -EINVAL;
				goto fail;
			}
			pwr_mode->pwr_mode_id = pwr_mode_id;
			wpa_printf(MSG_DEBUG, "nl80211: HW blocked channels pwr_mode=%u",
				   pwr_mode_id);

			if (mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_FB_CHAN_LIST]) {
				ret = qca_nl80211_parse_hw_blocked_chans_fb_chans_extn(
					mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_FB_CHAN_LIST],
					pwr_mode);
				if (ret)
					goto fail;
			}

			if (mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_PC_CHAN_LIST]) {
				ret = qca_nl80211_parse_hw_blocked_chans_pc_chans_extn(
					mode[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_PC_CHAN_LIST],
					pwr_mode);
				if (ret)
					goto fail;
			}

			num_pwr_modes++;
		}
	}

	hw_blocklist_info.num_pwr_modes = num_pwr_modes;

notify:
	os_memset(&event, 0, sizeof(event));
	event.event_data_extn.hw_blocklist_info = hw_blocklist_info;
	wpa_supplicant_event(bss->ctx, EVENT_HW_BLOCKED_CHANS_NOTIFY, &event);
	qca_nl80211_free_hw_blocked_chans_info_extn(&hw_blocklist_info);
	return 0;

fail:
	qca_nl80211_free_hw_blocked_chans_info_extn(&hw_blocklist_info);
	return ret;
}

static int qca_nl80211_handle_hw_blocked_chans_events_extn(struct i802_bss *bss,
							   u8 *data,
							   size_t len,
							   bool check_first_bss)
{
	struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_MAX + 1];
	struct nlattr *nl;
	int rem;
	int ret;

	if (!(data && len)) {
		wpa_printf(MSG_ERROR, "nl80211: Invalid HW blocked channels payload");
		return -EINVAL;
	}

	if (nla_parse(attr, QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_MAX,
		      (struct nlattr *) data, len, NULL)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to parse HW blocked channels payload attributes");
		return -EINVAL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_RADIO_LIST]) {
		wpa_printf(MSG_ERROR,
			   "nl80211: HW blocked channels payload missing radio list");
		return -EINVAL;
	}

	nla_for_each_nested(nl, attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_RADIO_LIST],
			    rem) {
		struct nlattr *radio_attr[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_MAX + 1];

		if (nla_parse_nested(radio_attr,
				     QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_MAX,
				     nl, NULL)) {
			wpa_printf(MSG_ERROR,
				   "nl80211: Invalid HW blocked channels radio entry");
			return -EINVAL;
		}

		ret = qca_nl80211_handle_hw_blocked_chans_info_extn(bss, radio_attr,
								    check_first_bss);
		if (ret)
			return ret;
	}

	return 0;
}

int qca_nl80211_scan_done_event_extn(struct i802_bss *bss, u8 *data, size_t len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SCAN_MAX + 1] = {0};
	union wpa_event_data event = {};
	struct cbs_event *cbs_evt;
	struct nlattr *nl;
	int rem;

	cbs_evt = &event.event_data_extn.scan_results_event.cbs_event;
	cbs_evt->scan_complete_freq = 0;
	cbs_evt->status = 0;
	cbs_evt->link_id = 0;

	if (!bss) {
		wpa_printf(MSG_ERROR, "nl80211: bss is NULL!");
		return -EINVAL;
	}

	if (!(data && len)) {
		wpa_printf(MSG_ERROR, "nl80211: Invalid data for scan done event");
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SCAN_MAX,
		      (struct nlattr *) data, len, NULL)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to parse scan done attributes");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID]) {
		cbs_evt->link_id =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID]);
		bss->bss_extn.scan_link_extn =
			nl80211_get_link(bss, cbs_evt->link_id);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES]) {
		nla_for_each_nested(nl,
				    tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES],
				    rem) {
			if (nla_len(nl) >= (int) sizeof(u32)) {
				cbs_evt->scan_complete_freq
					= nla_get_u32(nl);
				break;
			}
		}
	}

	if (!cbs_evt->scan_complete_freq) {
		wpa_printf(MSG_DEBUG, "nl80211: CBS scan complete freq is not set");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_STATUS]) {
			cbs_evt->status = (enum scan_status)
				nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCAN_STATUS]);
	}

	wpa_printf(MSG_INFO,
		   "nl80211: scan done event: scan_complete_freq=%u status=%u link_id=%u",
		   cbs_evt->scan_complete_freq,
		   cbs_evt->status, cbs_evt->link_id);

	wpa_supplicant_event(bss->ctx, EVENT_SCAN_RESULTS_EXTN, &event);
	return 0;
}

void *wpa_driver_nl80211_get_survey_extn(struct i802_bss *bss, void *ctx)
{
	if (!bss)
		return ctx;

	if (bss->bss_extn.scan_link_extn && bss->bss_extn.scan_link_extn->ctx) {
		ctx = bss->bss_extn.scan_link_extn->ctx;
		bss->bss_extn.scan_link_extn = NULL;
		wpa_printf(MSG_DEBUG, "nl80211: survey results processing for scan_link: %p", ctx);
	}

	return ctx;
}

static int hw_blocked_chans_process_event_extn(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	u8 *data;
	size_t len;
	struct i802_bss *bss = arg;
	int ret;

	ret = nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to parse HW blocked channels attributes: %d",
			   ret);
		return NL_SKIP;
	}

	if (!tb[NL80211_ATTR_VENDOR_DATA]) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: No vendor data in HW blocked channels reply");
		return NL_SKIP;
	}

	data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
	len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);
	ret = qca_nl80211_handle_hw_blocked_chans_events_extn(bss, data, len, false);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to handle HW blocked channels reply: %d",
			   ret);
	}

	return NL_SKIP;
}

static int qca_nl80211_handle_wiphy_config_evt_extn(struct i802_bss *bss,
						    u8 *data, size_t len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {0};
	union wpa_event_data event;
	u8 radio_idx = 0;

	if (!bss) {
		wpa_printf(MSG_ERROR, "nl80211: bss is NULL!");
		return -EINVAL;
	}

	if (!(data && len)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Invalid data for wiphy config event");
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		      (struct nlattr *)data, len, NULL)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to parse wiphy config event attributes");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE])
		return 0;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX])
		radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);

	if (bss != bss->drv->first_bss) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Ignore AGILE_CAPABLE for radio_idx %u on %s",
			   radio_idx, bss->ifname);
		return 0;
	}

	os_memset(&event, 0, sizeof(event));
	event.event_data_extn.agile_capable.adfs_capable =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE]);
	event.event_data_extn.agile_capable.radio_idx = radio_idx;

	wpa_printf(MSG_DEBUG,
		   "nl80211: AGILE_CAPABLE event: radio_idx=%u adfs_capable=%d",
		   radio_idx, event.event_data_extn.agile_capable.adfs_capable);

	wpa_supplicant_event(bss->drv->ctx, EVENT_AGILE_CAPABLE, &event);
	return 0;
}

int nl80211_vendor_event_qca_extn(struct i802_bss *bss,
				  u32 subcmd, u8 *data, size_t len)
{
	switch (subcmd) {
	case QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION:
		qca_nl80211_handle_wifi_config_evt_extn(bss, data, len);
		break;
	case QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION:
		qca_nl80211_handle_wiphy_config_evt_extn(bss, data, len);
		break;
	case QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG:
		qca_nl80211_handle_dcs_config_evt_extn(bss, data, len);
		break;
	case QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS:
		qca_nl80211_handle_hw_blocked_chans_events_extn(bss, data, len,
								true);
		break;
#ifndef CONFIG_DRIVER_NL80211_QCA
	case QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE:
		qca_nl80211_scan_done_event_extn(bss, data, len);
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

int qca_nl80211_handle_wifi_config_evt_extn(struct i802_bss *bss,
					    u8 *data, size_t len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {0};
	u8 link_id = 0;

	if (!bss) {
		wpa_printf(MSG_ERROR, "nl80211: bss is NULL!");
		return -EINVAL;
	}

	if (!(data && len)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Invalid data for WiFi configuration event");
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		      (struct nlattr *)data, len, NULL)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to parse WiFi configuration attributes");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);

	/* Check if ESP params are present */
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS])
		return nl80211_parse_esp_params_extn(bss,
						     tb[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS],
						     link_id);

	return 0;
}

int qca_nl80211_handle_dcs_config_evt_extn(struct i802_bss *bss,
					   u8 *data, size_t len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DCS_MAX  + 1] = {0};
	union wpa_event_data event = {};
	struct dcs_intf_event *dcs_intf_event;

	if (!bss) {
		wpa_printf(MSG_ERROR, "nl80211: bss is NULL!");
		return -EINVAL;
	}

	if (!(data && len)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Invalid data for DCS configuration event");
		return -EINVAL;
	}

	dcs_intf_event = &event.event_data_extn.dcs_intf_event;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_DCS_MAX,
				(struct nlattr *)data, len, NULL)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to parse DCS configuration attributes");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID])
		dcs_intf_event->link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_DCS_ENABLE])
		dcs_intf_event->type = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_DCS_ENABLE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_BITMAP])
		dcs_intf_event->chan_bw_interference_bitmap = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_BITMAP]);

	wpa_printf(MSG_ERROR,
		   "nl80211: DCS config event parsed: link_id=%u type=0x%04x interference_bitmap=0x%08x (attr_enable_present=%d attr_bitmap_present=%d)",
		   dcs_intf_event->link_id, dcs_intf_event->type,
		   dcs_intf_event->chan_bw_interference_bitmap,
		   !!tb[QCA_WLAN_VENDOR_ATTR_DCS_ENABLE],
		   !!tb[QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_BITMAP]);

	wpa_supplicant_event(bss->ctx, EVENT_DCS_INTF, &event);

	return 0;
}

int nl80211_fetch_hw_blocked_chans_extn(void *priv, int radio_idx)
{
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nlattr *params;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Fetching HW blocked channels (radio_idx=%d)",
		   radio_idx);
	if (radio_idx < -1)
		return -EINVAL;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS))
		goto error;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto error;
	if (radio_idx >= 0 &&
	    nla_put_s32(msg, QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX,
			radio_idx))
		goto error;
	nla_nest_end(msg, params);

	ret = send_and_recv_resp(drv, msg, hw_blocked_chans_process_event_extn, bss);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: HW blocked channels fetch failed err=%d (%s)",
			   ret, strerror(-ret));
	}

	return ret;

error:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG,
		   "nl80211: Could not fetch HW blocked channels on radio: %d",
		   radio_idx);
	return ret;
}

bool nl80211_is_6ghz_hw_blocked_chans_supported_extn(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	return drv->vendor_6ghz_hw_blocked_chans_support;
}

void wiphy_info_qca_vendor_command_extn(struct wpa_driver_nl80211_data *drv,
					const struct nl80211_vendor_cmd_info *vinfo)
{
	if (!drv || !vinfo || vinfo->vendor_id != OUI_QCA)
		return;

	switch (vinfo->subcmd) {
	case QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS:
		drv->vendor_6ghz_hw_blocked_chans_support = 1;
		break;
	default:
		break;
	}
}

int wpa_driver_nl80211_vendor_bss_addr(void *priv, u8 radio_idx, u8 bss_id,
				       u32 mbssid_enabled,
				       enum nl80211_iftype iftype, u32 flags,
				       u8 *addr, const char *ifname)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *attr;
	struct wpabuf *buf = NULL;
	int ret = -1;

	if (!drv)
		return -EINVAL;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	if (!genlmsg_put(msg, 0, 0, drv->global->nl80211_id, 0, 0,
			 NL80211_CMD_VENDOR, 0))
		goto fail;

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(ifname)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DERIVE_LINK_BSS_ADDR))
		goto fail;

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_INDEX,
		       radio_idx) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_BSS_ID,
		       bss_id) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_FLAGS,
			flags) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_IFTYPE,
			iftype))
		goto fail;

	/* Optional MBSSID group details for 6 GHz AP */
	if (mbssid_enabled &&
	    nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_ENABLED))
		goto fail;

	nla_nest_end(msg, attr);

	buf = wpabuf_alloc(ETH_ALEN + 16);
	if (!buf)
		goto fail;

	ret = send_and_recv_resp(drv, msg, mac_config_handler, buf);
	msg = NULL;
	if (ret)
		goto out;

	if (wpabuf_len(buf) < ETH_ALEN) {
		ret = -EMSGSIZE;
		goto out;
	}
	os_memcpy(addr, wpabuf_head(buf), ETH_ALEN);
	ret = 0;
out:
	if (buf)
		wpabuf_free(buf);
	return ret;
fail:
	if (msg)
		nlmsg_free(msg);
	return -ENOBUFS;
}

#ifdef CONFIG_IEEE80211BE
int wpa_driver_nl80211_vendor_cmd_notify_link_repurpose(void *priv, u8 link_id)
{
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nlattr *params;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG, "nl80211: Indication of link repurpose");

	if (drv->nlmode != NL80211_IFTYPE_AP)
		return -EOPNOTSUPP;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		goto error;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_REPURPOSE_LINK_INDICATION))
		goto error;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto error;
	if (link_id != NL80211_DRV_LINK_ID_NA &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID, link_id))
		goto error;
	nla_nest_end(msg, params);

	ret = send_and_recv(drv, bss->nl_connect, msg, NULL, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: link repurpose indication failed err=%d (%s)",
			   ret, strerror(-ret));
	}
	return ret;
error:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG,
		   "nl80211: Could not indicate repurpose on link %d",
		   link_id);
	return ret;
}
#endif /* CONFIG_IEEE80211BE */

int wpa_driver_nl80211_dcs_config_extn(void *priv, u8 link_id,
				       struct driver_dcs_config *params)
{
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = NULL;
	struct nlattr *attr;
	int ret = 0;

	if (!bss) {
		wpa_printf(MSG_ERROR,
			   "nl80211: DCS config request with no driver handle");
		return -1;
	}

	drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Configure DCS (cmd_type=%u valid_mask=0x%x)",
		   params->cmd_type, params->valid_mask);

	if (drv->nlmode != NL80211_IFTYPE_AP)
		return -EOPNOTSUPP;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR)) ||
	     nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	     nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
	     QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG)) {
		goto error;
	}

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto error;
	if ((link_id != NL80211_DRV_LINK_ID_NA &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID, link_id)) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE,
		       params->cmd_type) ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_DCS_ENABLE,
			params->dcs_enable)) {
		wpa_printf(MSG_DEBUG,"nl80211: Failed to configure DCS params");
		goto error;
	}

	/* Optional params when SET */
	if (params->cmd_type == SET_DCS_CONFIG) {
		if (params->valid_mask & DCS_VALID_INTR_DET_THR)
			if (nla_put_u32(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD,
			    params->intr_detection_threshold))
				goto error;
		if (params->valid_mask & DCS_VALID_PHYERR_PENALTY)
			if (nla_put_u32(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY,
			    params->phyerr_penalty))
				goto error;
		if (params->valid_mask & DCS_VALID_PHYERR_THR)
			if (nla_put_u32(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD,
			    params->phyerr_threshold))
				goto error;
		if (params->valid_mask & DCS_VALID_RADARERR_THR)
			if (nla_put_u32(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD,
			    params->radarerr_threshold))
				goto error;
		if (params->valid_mask & DCS_VALID_TXERR_THR)
			if (nla_put_u32(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD,
			    params->txerr_threshold))
				goto error;
		if (params->valid_mask & DCS_VALID_SAMPLE_SIZE)
			if (nla_put_u32(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW,
			    params->sample_size))
				goto error;
		if (params->valid_mask & DCS_VALID_COCH_THR)
			if (nla_put_u8(msg,
			    QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD,
			    params->coch_intr_threshold))
				goto error;
		if (params->valid_mask & DCS_VALID_USER_MAX_CU)
			if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU,
			    params->user_max_cu))
				goto error;
	}
	nla_nest_end(msg, attr);

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_DEBUG,
				"nl80211: DCS config failed=%d (%s)",
				ret, strerror(-ret));
	}
	return 0;
error:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Could not configure DCS on link %d", link_id);
	return -1;
}

/**
 * nl80211_notify_radar_detected_extn - Notify cfg80211 of radar detection
 * via NL80211_CMD_NOTIFY_RADAR for a channel described by hostapd_freq_params.
 *
 * Called from DFS extn NOL processing path when an uplink CSA NOL IE
 * is processed, so that cfg80211 marks the channel DFS_UNAVAILABLE and
 * propagates the event to all registered radios.
 *
 * @priv: private driver data (struct i802_bss *)
 * @freq: channel parameters including frequency, bandwidth and center freqs
 *
 * Returns 0 on success, -1 on failure.
 */
int nl80211_notify_radar_detected_extn(void *priv,
				      struct hostapd_freq_params *freq)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg;
	int ret;

	if (!bss || !freq) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Invalid parameters for radar notification");
		return -1;
	}

	drv = bss->drv;
	if (!drv) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Driver data not available");
		return -1;
	}

	if (freq->freq <= 0) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Invalid frequency %d for radar notification",
			   freq->freq);
		return -1;
	}

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_RADAR)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Driver does not support radar detection");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: Notify radar detected on %d MHz (ht_enabled=%d, vht_enabled=%d, bandwidth=%d MHz, cf1=%d MHz, cf2=%d MHz)",
		   freq->freq, freq->ht_enabled, freq->vht_enabled,
		   freq->bandwidth, freq->center_freq1, freq->center_freq2);

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_NOTIFY_RADAR);
	if (!msg) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to allocate message for radar notification");
		return -1;
	}

	/*
	 * Build frequency attributes directly — nl80211_put_freq_params() is
	 * a static function in driver_nl80211.c and cannot be called here.
	 * For NOL IE processing we always notify about individual 20 MHz
	 * subchannels, so the attributes are straightforward.
	 */
	if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, freq->freq) ||
	    nla_put_u32(msg, NL80211_ATTR_CHANNEL_WIDTH, NL80211_CHAN_WIDTH_20) ||
	    nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ1, freq->freq)) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to add frequency attributes");
		nlmsg_free(msg);
		return -1;
	}

	if (nl80211_link_valid(bss->valid_links, freq->link_id)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Radar notification on link_id=%d",
			   freq->link_id);
		if (nla_put_u8(msg, NL80211_ATTR_MLO_LINK_ID, freq->link_id)) {
			nlmsg_free(msg);
			return -1;
		}
	}

	ret = send_and_recv_cmd(drv, msg);
	if (ret == 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Successfully notified radar on %d MHz",
			   freq->freq);
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: Failed to notify radar detection: %d (%s)",
		   ret, strerror(-ret));
	return -1;
}

static int rropinfo_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *nl = NULL;
	struct nlattr *nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_MAX + 1];
	int rem = 0, i = 0;
	u32 num_rtplinst = 0;
	struct nl80211_rropinfo *rropinfo = (struct nl80211_rropinfo *)arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_VENDOR_DATA])
		goto fail;

	struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_MAX + 1];

	nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_RROP_INFO_MAX,
		  nla_data(nl_vendor), nla_len(nl_vendor), NULL);

	nl = tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL];
	if (!nl)
		goto fail;

	num_rtplinst = 0;
	nla_for_each_nested(nl,
			    tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL],
			    rem) {
		num_rtplinst++;
	}

	wpa_printf(MSG_DEBUG, "nl80211: rropinfo_handler found %u RTPL instances",
		   num_rtplinst);
	if (!num_rtplinst)
		goto fail;

	rropinfo->num_rtplinst = (num_rtplinst > MAX_NUM_CHANNELS) ?
				 MAX_NUM_CHANNELS : num_rtplinst;

	i = 0;
	nla_for_each_nested(nl,
			    tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL],
			    rem) {
		if (i >= MAX_NUM_CHANNELS)
			break;
		if (nla_parse(nl_rtplinst,
			      QCA_WLAN_VENDOR_ATTR_RTPLINST_MAX,
			      nla_data(nl), nla_len(nl), NULL)) {
			wpa_printf(MSG_ERROR,
				   "nl80211: failed to parse RTPL");
			goto fail;
		}

		if (nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY])
			rropinfo->rtpl[i].primary_freq =
				nla_get_u32(
				nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY]);

		if (nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT])
			rropinfo->rtpl[i].txpower_throughput =
				(int)nla_get_u32(
				nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT]);

		if (nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE])
			rropinfo->rtpl[i].txpower_range =
				(int)nla_get_u32(
				nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE]);

		wpa_printf(MSG_DEBUG,
			   "nl80211: RTPL[%d] primary=%u throughput=%d range=%d",
			   i,
			   rropinfo->rtpl[i].primary_freq,
			   rropinfo->rtpl[i].txpower_throughput,
			   rropinfo->rtpl[i].txpower_range);
		i++;
	}

	return NL_SKIP;

fail:
	rropinfo->num_rtplinst = 0;
	return NL_SKIP;
}

int driver_nl80211_vendor_get_chan_rropinfo(void *ctx,
					    struct nl80211_rropinfo *rropinfo,
					    int radio_idx)
{
	int ret = -1;
	struct nl_msg *msg = NULL;
	struct i802_bss *bss = ctx;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nlattr *params;

	wpa_printf(MSG_DEBUG, "nl80211: driver_nl80211_vendor_get_chan_rropinfo start: radio_idx: %d", radio_idx);

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;
	if (radio_idx != NL80211_WIPHY_RADIO_ID_MAX &&
	    nla_put_u8(msg,QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX, radio_idx))
		goto fail;
	nla_nest_end(msg, params);

	ret = send_and_recv_resp(drv, msg, rropinfo_handler, rropinfo);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_ERROR, "nl80211: Vendor get RROP info request failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "nl80211: RROP info received, num_rtplinst=%u",
		   rropinfo ? rropinfo->num_rtplinst : 0);
	return 0;

fail:
	if (msg)
		nlmsg_free(msg);
	return ret;
}

int wpa_driver_nl80211_dcs_sim_extn(void *priv, u8 link_id,
				    struct driver_dcs_sim *params)
{
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = NULL;
	struct nlattr *attr;
	int ret = 0;

	if (!bss) {
		wpa_printf(MSG_ERROR,
			   "nl80211: DCS SIM request with no driver handle");
		return -1;
	}

	drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Configure DCS SIM");
	if (drv->nlmode != NL80211_IFTYPE_AP)
		return -EOPNOTSUPP;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DCS_SIM)) {
		goto error;
	}

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto error;

	if ((link_id != NL80211_DRV_LINK_ID_NA &&
	     nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID, link_id)) ||
	     nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_DCS_SIM_TYPE,
			 params->type))
		goto error;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DCS_SIM_INTERFERENCE_BITMAP, params->intf_bitmap))
		goto error;

	nla_nest_end(msg, attr);

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_DEBUG,
				"nl80211: DCS SIM failed=%d (%s)",
				ret, strerror(-ret));
	}
	return 0;
error:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Could not configure DCS SIM on link %d", link_id);
	return -1;
}

/**
 * nl80211_set_allow_3addr_mc_extn - Set ALLOW_3ADDR_MC via vendor command
 * @priv: Pointer to struct i802_bss (driver private data)
 * @val: Value to set (0 = disallow, 1 = allow 3-address multicast)
 *
 * Sends QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION with
 * QCA_WLAN_VENDOR_ATTR_CONFIG_ALLOW_3ADDR_MC attribute.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nl80211_set_allow_3addr_mc_extn(void *priv, u8 val)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;

	if (!bss) {
		wpa_printf(MSG_ERROR, "ALLOW_3ADDR_MC: BSS not initialized");
		return -1;
	}

	drv = bss->drv;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_ALLOW_3ADDR_MC,
		       val))
		goto fail;

	nla_nest_end(msg, params);

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "ALLOW_3ADDR_MC: vendor command failed: %s (%d)",
			   strerror(-ret), ret);
		return ret;
	}

	wpa_printf(MSG_DEBUG, "ALLOW_3ADDR_MC: set to %u", val);
	return 0;

fail:
	wpa_printf(MSG_ERROR,
		   "ALLOW_3ADDR_MC: Failed to build vendor command");
	nlmsg_free(msg);
	return -1;
}

int hostapd_drv_mark_vap_submode(struct hostapd_data *hapd,
				 enum qca_wlan_vendor_vap_submode_type submode)
{
	if (submode == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_NONE)
		return 0;
	else
		return hostapd_drv_mark_vap_submode_extn(hapd->drv_priv,
				OUI_QCA,
				QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
				hapd->conf->iface,
				submode);

}

int hostapd_drv_mark_vap_submode_extn(void *priv, unsigned int vendor_id,
					      unsigned int subcmd,
					      const char *ifname,
					      u8 vap_submode)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *attr;
	int ret;
	int ifidx;

	if (!bss || !drv)
		return -EINVAL;

	ifidx = if_nametoindex(ifname);

	msg = nlmsg_alloc();
	if (!msg)
		return -EINVAL;

	if (!genlmsg_put(msg, 0, 0, drv->global->nl80211_id,
			0,  0, NL80211_CMD_VENDOR, 0))
		goto fail;

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifidx) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, vendor_id) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, subcmd))
		goto fail;

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;
	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE,
		       vap_submode))
		goto fail;

	nla_nest_end(msg, attr);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR, "nl80211: vendor command - vap_submode: %dfailed err=%d",
			   vap_submode, ret);
	else {
		wpa_printf(MSG_INFO, "nl80211: vendorcmd vap_submode: ifname %s vap_submode %d",
			   ifname, vap_submode);
	}
	return ret;
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

int nl80211_set_he_mcs_12_13_peer_cap_extn(void *priv, u8 radio_idx,
					   u16 peer_cap)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = NULL;
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;

	if (!bss) {
		wpa_printf(MSG_ERROR,
			   "nl80211: set_he_mcs_12_13_peer_cap with no driver handle");
		return -1;
	}

	drv = bss->drv;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set HE_MCS_12_13 peer capability = 0x%04x for radio_idx: %d",
		   peer_cap, radio_idx);
	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
			QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX,
		       radio_idx) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
		    sizeof(peer_cap), &peer_cap)) {
		nla_nest_end(msg, params);
		goto fail;
	}

	nla_nest_end(msg, params);
	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "nl80211: Setting HE_MCS_12_13_PEER_CAP failed: %d (%s)",
			   ret, strerror(-ret));
	return ret;
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

/**
 * nl80211_set_vht_mcs10_11_and_he_cap_internal_extn - Notify driver of
 * negotiated VHT MCS 10/11 and HE 400ns SGI / 2xLTF 160/80+80 peer caps.
 *
 * Sends QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION carrying:
 *   PEER_MAC (attr 35)                     — 6-byte peer address
 *   VHT_MCS_10_11_PEER_SUPP (attr 154)     — u8: 1 supported / 0 not
 *   HE_CAP_INFO_INTERNAL (attr 155)        — u32 bitmap (optional, if non-zero)
 *
 * Called from both hostapd (AP mode) and wpa_supplicant (STA mode) after
 * association to inform ath12k so it can encode them in the WMI peer_assoc
 * command.  Replaces the manual wpabuf NLA construction in qcn_ie_extn.c.
 *
 * @priv:            driver private handle
 * @peer_addr:       peer MAC address (6 bytes)
 * @vht_mcs10_11:    1 if VHT MCS10/11 (1024-QAM) is supported, 0 otherwise
 * @he_cap_internal: HE capability bitmap (bits 0-1: 400ns SGI, bit 2: 2xLTF
 *                   160/80+80); pass 0 to omit the attribute
 *
 * Returns 0 on success, negative errno on failure.
 */
int nl80211_set_vht_mcs10_11_and_he_cap_internal_extn(void *priv,
						      const u8 *peer_addr,
						      u8 vht_mcs10_11,
						      u32 he_cap_internal)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;

	if (!bss || !bss->drv || !peer_addr)
		return -EINVAL;

	drv = bss->drv;
	wpa_printf(MSG_DEBUG,
		   "nl80211: Set VHT MCS10/11 peer_supp=%u he_cap_internal=0x%08x for " MACSTR,
		   vht_mcs10_11, he_cap_internal, MAC2STR(peer_addr));

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	/* In STA mode the driver identifies the single peer via the VIF —
	 * do not send PEER_MAC.  In AP mode send it so the driver can look
	 * up the correct per-station entry among multiple peers.
	 */
	if (drv->nlmode != NL80211_IFTYPE_STATION) {
		if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC,
			    ETH_ALEN, peer_addr))
			goto fail_nest;
	}

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_VHT_MCS_10_11_PEER_SUPP,
		       vht_mcs10_11))
		goto fail_nest;

	if (he_cap_internal &&
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_HE_CAP_INFO_INTERNAL,
			he_cap_internal))
		goto fail_nest;

	nla_nest_end(msg, params);
	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "nl80211: Set VHT MCS10/11 / HE cap internal failed: %d (%s)",
			   ret, strerror(-ret));
	return ret;

fail_nest:
	nla_nest_end(msg, params);
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

/**
 * nl80211_set_allow_scan_on_dfs_chan_extn - Enable/disable scanning on DFS
 *	channels in the mac80211 kernel layer.
 *
 * Uses QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION with:
 *   GENERIC_COMMAND = QCA_WLAN_VENDOR_WIFI_PARAM_ALLOW_SCAN_ON_DFS_CHAN
 *   GENERIC_DATA    = u8 (1 = enable, 0 = disable)
 *
 * Returns: 0 on success, negative on failure.
 */
int nl80211_set_allow_scan_on_dfs_chan_extn(void *priv, bool enable)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = NULL;
	u8 val = enable ? 1 : 0;
	struct nlattr *params;
	struct nl_msg *msg;
	int ret;

	if (!bss) {
		wpa_printf(MSG_ERROR,
			   "nl80211: allow_scan_on_dfs_chan with no driver handle");
		return -1;
	}

	drv = bss->drv;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set allow_scan_on_dfs_chan = %d", val);

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
			QCA_WLAN_VENDOR_WIFI_PARAM_ALLOW_SCAN_ON_DFS_CHAN) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
		    sizeof(val), &val)) {
		nla_nest_end(msg, params);
		goto fail;
	}

	nla_nest_end(msg, params);
	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "nl80211: allow_scan_on_dfs_chan set failed: %d", ret);
	return ret;
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

static int nl80211_get_he_mcs_12_13_handler(struct nl_msg *msg, void *arg)
{
	u16 *radio_cap = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct nlattr *data_attr;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_VENDOR_DATA])
		return NL_SKIP;

	if (nla_parse_nested(tb_vendor, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
			     tb[NL80211_ATTR_VENDOR_DATA], NULL))
		return NL_SKIP;

	data_attr = tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA];
	if (!data_attr || nla_len(data_attr) < (int)sizeof(u16))
		return NL_SKIP;

	*radio_cap = *(u16 *)nla_data(data_attr);
	return NL_SKIP;
}

int wpa_driver_nl80211_cbs_trigger_scan(void *priv,
				const struct cbs_params_extn *params,
				int *freq_list, int link_id)
{
	struct nl_msg *msg = NULL;
	struct nlattr *attr = NULL;
	struct nlattr *cbs = NULL;
	struct i802_bss *bss;
	struct wpa_driver_nl80211_data *drv;
	int ret = -ENOBUFS;

	if (!priv || !params) {
		wpa_printf(MSG_ERROR, "nl80211: Invalid args for CBS trigger scan");
		return -EINVAL;
	}

	bss = priv;
	drv = bss->drv;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN)) {
		goto fail;
	}

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;

	cbs = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN);
	if (!cbs)
		goto fail;

	if (params->dwellsplit &&
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELL,
			params->dwellsplit))
		goto fail;

	if (params->dwellrest &&
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELLREST,
			params->dwellrest))
		goto fail;

	if (params->cbs_enable &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE,
		       params->cbs_enable))
		goto fail;

	nla_nest_end(msg, cbs);

	if (params->resttime &&
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SCAN_RESTTIME,
			params->resttime))
		goto fail;

	if (params->waittime &&
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SCAN_WAITTIME,
			params->waittime))
		goto fail;

	if (freq_list) {
		struct nlattr *freqs;
		int i;

		freqs = nla_nest_start(msg,
				       QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES);
		if (!freqs)
			goto fail;
		for (i = 0; freq_list[i]; i++) {
			if (nla_put_u32(msg, i + 1, freq_list[i]))
				goto fail;
		}
		nla_nest_end(msg, freqs);
	}

	if (params->totaldwell &&
	    nla_put_u64(msg, QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME,
			params->totaldwell))
		goto fail;

	nla_nest_end(msg, attr);

	if (nla_put_u8(msg, NL80211_ATTR_MLO_LINK_ID, link_id))
		goto fail;

	ret = send_and_recv_cmd(drv, msg);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: CBS trigger scan vendor cmd failed: %s (%d)",
			   strerror(-ret), ret);
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: CBS trigger scan sent: cbs_enable=%d resttime=%d dwellrest=%d waittime=%d dwellsp=%d totaldwell=%d",
		   params->cbs_enable, params->resttime, params->dwellrest,
		   params->waittime, params->dwellsplit, params->totaldwell);
	return 0;

fail:
	if (msg)
		nlmsg_free(msg);
	return ret;
}

int nl80211_get_he_mcs_12_13_extn(void *priv, u8 radio_idx, u16 *radio_cap)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = NULL;
	struct nl_msg *msg;
	struct nlattr *params;
	u16 cap = 0;
	int ret;

	if (!bss) {
		wpa_printf(MSG_ERROR,
			   "nl80211: get_he_mcs_12_13 with no driver handle");
		return -1;
	}

	drv = bss->drv;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Get the HE_MCS_12_13 hardware capability for radio_idx: %d",
		   radio_idx);
	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
			QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX,
		       radio_idx)) {
		nla_nest_end(msg, params);
		goto fail;
	}

	nla_nest_end(msg, params);
	ret = send_and_recv_resp(drv, msg,
				 nl80211_get_he_mcs_12_13_handler, &cap);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: GET_HE_MCS_12_13 failed: %d (%s) for radio_idx: %d",
			   ret, strerror(-ret), radio_idx);
		return ret;
	}

	*radio_cap = cap;
	return 0;
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

int nl80211_disable_opclass_chans_extn(void *priv, u8 link_id, u8 is_disable, u8 opclass,
				       const u8 *chan_list, size_t chan_count)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg;
	struct nlattr *params, *chan_attr;
	size_t idx;
	int ret;

	if (!bss || !bss->drv || !chan_list || !chan_count ||
	    (is_disable != 0 && is_disable != 1) || !opclass)
		return -EINVAL;

	drv = bss->drv;
	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_REG_PARAMS))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CMD_EXTN,
		       QCA_WLAN_VENDOR_REG_PARAMS_DISABLE_OPCLASS_CHANS_EXTN) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_REG_PARAMS_LINKID_EXTN, link_id) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_REG_PARAMS_DISABLE_EXTN, is_disable) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS_EXTN, opclass))
		goto fail_params;

	chan_attr = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CHAN_LIST_EXTN);
	if (!chan_attr)
		goto fail_params;

	for (idx = 0; idx < chan_count; idx++) {
		if (nla_put_u8(msg, idx + 1, chan_list[idx]))
			goto fail_chan;
	}

	nla_nest_end(msg, chan_attr);
	nla_nest_end(msg, params);

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: DISABLE_OPCLASS_CHANS failed: %d (%s) disable=%u opclass=%u count=%zu",
			   ret, strerror(-ret), is_disable, opclass, chan_count);
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: DISABLE_OPCLASS_CHANS sent disable=%u opclass=%u count=%zu",
		   is_disable, opclass, chan_count);
	return 0;

fail_chan:
	nla_nest_end(msg, chan_attr);
fail_params:
	nla_nest_end(msg, params);
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

int netlink_increase_rcvbuf_extn(int sock)
{
	int rcvbuf = 1 * 1024 * 1024; /* 1 MB */

	/*
	 * Increase the socket receive buffer to avoid dropping RTM_NEWLINK
	 * events during large-scale interface bringup (e.g. 16-MLD setups).
	 * Each AP BSS generates ~20 RTM_NEWLINK events (created, UP, bridge
	 * add, AF_BRIDGE) averaging ~1 KB each; 16 BSSes produce ~320 KB of
	 * events which overflows the default 208 KB kernel netlink socket
	 * buffer. Lost events cause monitor interface tracking to fail,
	 * breaking AFC-triggered CSA. Use SO_RCVBUFFORCE so the request is
	 * not capped by rmem_max (hostapd has CAP_NET_ADMIN). Fall back to
	 * SO_RCVBUF if SO_RCVBUFFORCE is not available.
	 */
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUFFORCE,
		       &rcvbuf, sizeof(rcvbuf)) < 0 &&
	    setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		       &rcvbuf, sizeof(rcvbuf)) < 0) {
		wpa_printf(MSG_DEBUG,
			   "netlink: Could not increase SO_RCVBUF: %s",
			   strerror(errno));
		return -1;
	}

	return 0;
}

#ifdef CONFIG_IEEE80211BN
static int mapc_get_peer_params_reply_handler(struct nl_msg *msg, void *arg)
{
	struct mapc_peer_params_result *res = arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY_MAX + 1];
	struct mapc_cotdma_e2e_entry *entry;
	const struct nlattr *vendor_data;
	struct nlattr *pos;
	int rem_e2e;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_VENDOR_DATA])
		return NL_SKIP;

	nla_parse(vendor_tb, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MAX,
		  nla_data(tb[NL80211_ATTR_VENDOR_DATA]),
		  nla_len(tb[NL80211_ATTR_VENDOR_DATA]), NULL);

	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CAP_BITMAP])
		res->cap_bitmap   = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CAP_BITMAP]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_APID_TO])
		res->apid_to      = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_APID_TO]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_APID_FROM])
		res->apid_from    = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_APID_FROM]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_Q2Q_TO])
		res->q2q_to       = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_Q2Q_TO]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_Q2Q_FROM])
		res->q2q_from     = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_Q2Q_FROM]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CH_WIDTH])
		res->ch_width     = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CH_WIDTH]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CCFS])
		res->ccfs         = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_CCFS]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_BSS_COLOR])
		res->bss_color    = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_BSS_COLOR]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_RX_TXOP])
		res->rx_txop      = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_RX_TXOP]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_DSB])
		res->dsb          = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_DSB]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PRIMARY_AC])
		res->primary_ac   = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PRIMARY_AC]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_NBR_PRIO])
		res->nbr_prio     = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_NBR_PRIO]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_START])
		res->svc_start    = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_START]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_INTERVAL])
		res->svc_interval = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_INTERVAL]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_END])
		res->svc_end      = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_SVC_END]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_LATENCY_THRESHOLD])
		res->latency_threshold = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_LATENCY_THRESHOLD]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MAX_TXOP])
		res->max_txop     = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MAX_TXOP]);
	if (vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MIN_TXOP])
		res->min_txop     = nla_get_u32(vendor_tb[QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_MIN_TXOP]);

	res->e2e_entry_count = 0;
	vendor_data = tb[NL80211_ATTR_VENDOR_DATA];
	nla_for_each_attr(pos,
			  (struct nlattr *)nla_data(vendor_data),
			  nla_len(vendor_data), rem_e2e) {
		if (nla_type(pos) != QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_E2E_ENTRIES)
			continue;
		if (res->e2e_entry_count >= MAPC_MAX_E2E_ENTRIES)
			break;

		entry = &res->e2e_entries[res->e2e_entry_count];
		os_memset(entry, 0, sizeof(*entry));

		if (nla_parse_nested(etb,
				     QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_CONFIG_ENTRY_MAX,
				     pos, NULL))
			continue;

		if (etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_CONFIG_MODE])
			entry->config_mode = nla_get_u8(
				etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_CONFIG_MODE]);
		if (etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_QMID])
			entry->qmid = nla_get_u16(
				etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_QMID]);
		if (etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_MAC])
			os_memcpy(entry->peer_mac,
				  nla_data(etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_MAC]),
				  ETH_ALEN);
		if (etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_BACKHAUL_MAC]) {
			entry->bsta_mac_valid = true;
			os_memcpy(entry->bsta_mac,
				  nla_data(etb[QCA_WLAN_VENDOR_ATTR_MAPC_COTDMA_E2E_ENTRY_PEER_BACKHAUL_MAC]),
				  ETH_ALEN);
		}
		res->e2e_entry_count++;
	}

	return NL_SKIP;
}

int nl80211_set_mapc_vendor_params_extn(struct hostapd_data *hapd,
					const u8 *peer_addr,
					u16 apid_to, u16 apid_from)
{
	struct i802_bss *bss;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;

	if (!hapd || !hapd->drv_priv || !peer_addr)
		return -EINVAL;
	bss = hapd->drv_priv;
	drv = bss->drv;
	if (!drv)
		return -ENODEV;

	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_PARAMS))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (hapd->conf->mld_ap &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID,
		       hapd->mld_link_id))
		goto fail;

	if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_MAPC_PEER_MAC, ETH_ALEN, peer_addr) ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_TO,   apid_to)   ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_MAPC_Q2Q_APID_FROM, apid_from))
		goto fail;

	nla_nest_end(msg, params);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "MAPC: set vendor params NL cmd failed for " MACSTR
			   ": %d", MAC2STR(peer_addr), ret);
	return ret;

fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}

int nl80211_get_mapc_peer_params_extn(struct hostapd_data *hapd,
				      const u8 *peer_addr,
				      struct mapc_peer_params_result *res)
{
	struct i802_bss *bss;
	struct wpa_driver_nl80211_data *drv;
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;

	if (!hapd || !hapd->drv_priv || !peer_addr || !res)
		return -EINVAL;
	bss = hapd->drv_priv;
	drv = bss->drv;
	if (!drv)
		return -ENODEV;

	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOMEM;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_GET_PARAMS))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (hapd->conf->mld_ap &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID,
		       hapd->mld_link_id))
		goto fail;

	if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_PEER_MAC,
		    ETH_ALEN, peer_addr))
		goto fail;

	nla_nest_end(msg, params);

	ret = send_and_recv_resp(drv, msg,
				 mapc_get_peer_params_reply_handler, res);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "MAPC: get peer params NL cmd failed for " MACSTR
			   ": %d", MAC2STR(peer_addr), ret);
	return ret;

fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}
#endif /* CONFIG_IEEE80211BN */
