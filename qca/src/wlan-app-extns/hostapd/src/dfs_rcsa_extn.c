/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include "common.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "cmn.h"
#include <ap/hostapd.h>
#include <ap/dfs.h>
#include <ap/hw_features.h>
#include "utils/wpa_debug.h"
#include "ucode_extn.h"
#include "ubus_extn.h"
#include "dfs_extn.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "wpa_supplicant_extn.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "qcn_ie_extn.h"

bool hostapd_rcsa_tx_bh_enabled(struct hostapd_iface *iface)
{
	if (IS_CSH_RCSA_TO_UPLINK_ENABLED(iface->conf->conf_extn.cswopts) &&
	    hostapd_is_bh_sta_link_connected_extn(iface))
		return true;

	return false;
}

static bool hostapd_rcsa_rx_bh_enabled(struct hostapd_iface *iface)
{
       if (IS_CSH_PROCESS_RCSA_ENABLED(iface->conf->conf_extn.cswopts) &&
           hostapd_is_bh_sta_link_connected_extn(iface))
               return true;

       return false;
}


void hostapd_set_rcsa_inprogress(struct hostapd_iface *iface, bool value)
{
	iface->iface_extn.rcsa_ctx.rcsa_inprogress = value;
}


static bool hostapd_is_rcsa_inprogress(struct hostapd_iface *iface)
{
	if (iface->iface_extn.rcsa_ctx.rcsa_inprogress)
		return true;

	return false;
}

bool optional_ml_info_ie_access(u8 *buf, size_t buf_len,
					s8 *link_id, bool set)
{
	u8 *pos = buf;
	size_t rem_len = buf_len;
	u16 link_id_bitmap;
	int i;

	while (pos && rem_len >= 2) {
		size_t ie_len = (size_t) pos[1] + 2;

		if (ie_len > rem_len)
			return false;

		if (hostapd_is_ml_info_ie(pos, ie_len)) {
			/*
			 * ML Info IE for RCSA:
			 *   Element ID   : Extension
			 *   Length       : 3
			 *   Extension ID : Multi-Link
			 *   Link bitmap  : 2-byte LE bitmap
			 */
			if (ie_len < 5)
				return false;

			if (!link_id)
				return true;

			if (set) {
				WPA_PUT_LE16(pos + 3, (BIT(*link_id)));
				return true;
			}
			link_id_bitmap = WPA_GET_LE16(pos + 3);
			for (i = 0; i < 16; i++) {
				if (link_id_bitmap & BIT(i)) {
					*link_id = i;
					return true;
				}
			}

			return true;
		}

		pos += ie_len;
		rem_len -= ie_len;
	}

	return false;
}

#ifdef UCODE_SUPPORT
static int hostapd_ucode_notify_rcsa_tx(struct hostapd_iface *iface, u8 channel,
					int freq, u8 switch_mode,
					const u8 *opt_ie, u8 opt_ie_len)
{
	uc_value_t *val = NULL;
	uc_value_t *iface_registry = NULL;
	struct uc_vm *vm = NULL;
	s8 hw_idx = 0;
	char *opt_ie_hex = NULL;
	struct hostapd_rcsa_ctx *rcsa_ctx;

	wpa_printf(MSG_DEBUG, "rcsa: TX notify: freq=%d channel=%d",
		   freq, channel);

	if (!iface->ucode.idx)
		return -1;

	iface_registry = ucode_ap_fetch_iface_reg_extn();
	if (!iface_registry)
		return -1;

	vm = ucode_ap_fetch_vm_extn();
	if (!vm)
		return -1;

	val = wpa_ucode_registry_get(iface_registry, iface->ucode.idx);
	if (!val)
		return -1;

	if (wpa_ucode_call_prepare("event"))
		return -1;

	uc_value_push(ucv_get(ucv_string_new(iface->phy)));

	if (iface->current_hw_info)
		hw_idx = iface->current_hw_info->hw_idx;

	wpa_printf(MSG_DEBUG, "rcsa: event with radio id %d %s\n",
		   hw_idx, iface->phy);
	uc_value_push(ucv_get(ucv_int64_new(hw_idx)));
	uc_value_push(ucv_get(val));
	uc_value_push(ucv_get(ucv_string_new(event_to_string(EVENT_DFS_RCSA_TX))));
	val = ucv_object_new(vm);
	uc_value_push(ucv_get(val));

	ucv_object_add(val, "channel", ucv_int64_new(channel));
	ucv_object_add(val, "frequency", ucv_int64_new(freq));
	ucv_object_add(val, "csa_count", ucv_int64_new(HOSTAPD_RCSA_TX_COUNT));
	ucv_object_add(val, "switch_mode", ucv_int64_new(switch_mode));

	if (opt_ie && opt_ie_len > 0) {
		opt_ie_hex = os_malloc((opt_ie_len * 2) + 1);
		if (!opt_ie_hex)
			return -1;

		wpa_snprintf_hex(opt_ie_hex, (opt_ie_len * 2) + 1,
				 opt_ie, opt_ie_len);
		ucv_object_add(val, "optional_ie_hex",
			       ucv_string_new(opt_ie_hex));
		os_free(opt_ie_hex);
	}

	ucv_put(wpa_ucode_call(5));
	ucv_gc(vm);

	if (!hostapd_is_rcsa_inprogress(iface) &&
	    !eloop_is_timeout_registered(hostapd_trigger_backhaul_sta_disconnect,
					 iface, NULL)) {
		eloop_register_timeout(1, HAPD_DFS_WAIT_FOR_RCSA_FROM_ROOT_DUR_US(100000),
				       hostapd_trigger_backhaul_sta_disconnect,
				       iface, NULL);
	}
	rcsa_ctx = &iface->iface_extn.rcsa_ctx;
	if (!hostapd_is_rcsa_inprogress(iface) &&
	    !eloop_is_timeout_registered(hostapd_trigger_rcsa_tx,
					 iface, NULL)) {
		rcsa_ctx->rcsa_tx_cnt = HOSTAPD_RCSA_TX_COUNT - 1;
		eloop_register_timeout(0, HOSTAPD_RCSA_INTVAL_US,
				       hostapd_trigger_rcsa_tx,
				       iface, NULL);
	}

	hostapd_set_rcsa_inprogress(iface, true);
	return 0;
}
#else
static int hostapd_ucode_notify_rcsa_tx(struct hostapd_iface *iface, u8 channel,
					int freq, u8 switch_mode,
					const u8 *opt_ie, u8 opt_ie_len)
{
	return -1;
}
#endif

static void hostapd_get_local_rcsa_ml_info(struct hostapd_iface *iface,
					   bool *include_ml_ie,
					   u16 *link_id_bitmap)
{
	*include_ml_ie = false;
	*link_id_bitmap = 0;

	if (!iface || !iface->bss[0])
		return;

	if (iface->bss[0]->conf->mld_ap &&
	    (iface->bss[0]->mld_link_id >= 0) &&
	    (iface->state == HAPD_IFACE_DFS)) {
		*include_ml_ie = true;
		*link_id_bitmap = BIT(iface->bss[0]->mld_link_id);
	}
}

size_t hostapd_build_rcsa_optional_ies(const u8 *nol_ie,
				       size_t nol_ie_len,
				       bool include_ml_ie,
				       u16 link_id_bitmap,
				       u8 *buf, size_t buf_len)
{
	u8 *pos = buf;
	u8 *ml_pos;

	if (!buf || !buf_len)
		return 0;

	if (nol_ie && nol_ie_len > 0) {
		if (nol_ie_len > (size_t) (buf + buf_len - pos))
			return 0;

		os_memcpy(pos, nol_ie, nol_ie_len);
		wpa_printf(MSG_DEBUG, "rcsa:copied nol");
		pos += nol_ie_len;
	}

	if (!include_ml_ie)
		return pos - buf;

	ml_pos = add_ml_link_info_ie(pos, buf_len - (pos - buf),
				     link_id_bitmap);
	if (!ml_pos)
		return 0;

	return ml_pos - buf;
}

static int hostapd_build_nol_ie(struct hostapd_iface *iface,
				u8 *buf, size_t buf_len)
{
	dfs_nol_ie_info *nol_info;
	int len;

	if (!iface->iface_extn.nol_info_valid)
		return -1;

	nol_info = &iface->iface_extn.nol_info;

	wpa_printf(MSG_DEBUG,
		   "rcsa: nol info : bw %u freq %u nol %hx",
		   nol_info->bandwidth, nol_info->freq,
		   nol_info->subchan_bitmap);

	len = dfs_encode_nol_ie(nol_info, buf, buf_len);
	if (len < 0)
		return -1;

	iface->iface_extn.nol_info_valid = false;

	return len;
}

static void hostapd_rcsa_store_optional_ie(struct hostapd_iface *iface,
					   const u8 *opt_ie,
					   size_t opt_ie_len)
{
	struct hostapd_rcsa_ctx *rcsa_ctx;

	rcsa_ctx = &iface->iface_extn.rcsa_ctx;
	rcsa_ctx->optional_ie_len = 0;

	if (!opt_ie || !opt_ie_len)
		return;

	if (opt_ie_len > sizeof(rcsa_ctx->optional_ie)) {
		wpa_printf(MSG_WARNING,
				"rcsa: optional IE too long (%zu > %zu)",
				opt_ie_len, sizeof(rcsa_ctx->optional_ie));
		return;
	}
	os_memcpy(rcsa_ctx->optional_ie, opt_ie, opt_ie_len);
	rcsa_ctx->optional_ie_len = opt_ie_len;
}

/**
 * hostapd_validate_rcsa_nol_info - Validate parsed RCSA NOL IE data
 * @iface: hostapd interface receiving the RCSA frame
 * @rcsa_nol: NOL information parsed from the RCSA optional IE
 * @nol_info: output NOL information cleared before bitmap extraction
 *
 * Validates the context and mandatory NOL fields before bitmap
 * extraction. Clears @nol_info to avoid stale data on failure.
 *
 * Return: 0 when the parsed NOL data can be processed, -1 otherwise.
 */
static int hostapd_validate_rcsa_nol_info(struct hostapd_iface *iface,
					  const dfs_nol_ie_info *rcsa_nol,
					  dfs_nol_ie_info *nol_info)
{
	if (!iface || !iface->conf || !rcsa_nol || !nol_info) {
		wpa_printf(MSG_ERROR, "RCSA: invalid NOL reverse context");
		return -1;
	}

	os_memset(nol_info, 0, sizeof(*nol_info));
	if (!rcsa_nol->freq || rcsa_nol->bandwidth != RCSA_MIN_DFS_SUBCHAN_BW ||
	    !rcsa_nol->subchan_bitmap) {
		wpa_printf(MSG_ERROR,
			   "RCSA: invalid NOL freq=%u bw=%u bitmap=0x%04x",
			   rcsa_nol->freq, rcsa_nol->bandwidth,
			   rcsa_nol->subchan_bitmap);
		return -1;
	}

	return 0;
}

/**
 * hostapd_get_rcsa_oper_chwidth_cf1 - Get RCSA width and center frequency
 * @iface: hostapd interface with current operating channel information
 * @oper_chwidth: output operating channel width
 * @cf1: output operating center frequency in MHz
 */
static void hostapd_get_rcsa_oper_chwidth_cf1(struct hostapd_iface *iface,
					       enum oper_chan_width *oper_chwidth,
					       int *cf1)
{
	*oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	*cf1 = iface->freq;
	if (*oper_chwidth == CONF_OPER_CHWIDTH_USE_HT &&
	    iface->conf->secondary_channel) {
		*cf1 += iface->conf->secondary_channel * 10;
	} else if (*oper_chwidth == CONF_OPER_CHWIDTH_80MHZ &&
		   hostapd_get_oper_centr_freq_seg1_idx(iface->conf)) {
		*oper_chwidth = CONF_OPER_CHWIDTH_160MHZ;
		*cf1 += (hostapd_get_oper_centr_freq_seg1_idx(iface->conf) -
			 iface->conf->channel) * 5;
	} else if (*oper_chwidth != CONF_OPER_CHWIDTH_USE_HT) {
		*cf1 += (hostapd_get_oper_centr_freq_seg0_idx(iface->conf) -
			 iface->conf->channel) * 5;
	}
}

/**
 * hostapd_get_rcsa_oper_bw - Get operating bandwidth for RCSA NOL bitmap
 * @iface: hostapd interface with current operating channel information
 * @bandwidth_mhz: output operating bandwidth in MHz
 *
 * Return: 0 on success, -1 otherwise.
 */
static int hostapd_get_rcsa_oper_bw(struct hostapd_iface *iface,
				    int *bandwidth_mhz)
{
	int cf1;
	enum oper_chan_width oper_chwidth;

	hostapd_get_rcsa_oper_chwidth_cf1(iface, &oper_chwidth, &cf1);

	if (dfs_nol_ie_chan_width_to_bw_mhz(oper_chwidth, iface->freq,
					    cf1, bandwidth_mhz)) {
		wpa_printf(MSG_ERROR, "RCSA: unsupported oper chwidth=%d",
			   oper_chwidth);
		return -1;
	}

	return 0;
}

/**
 * hostapd_get_rcsa_start_subchan - Find NOL start subchannel
 * @rcsa_nol: NOL information parsed from the RCSA frame
 * @base_freq: primary operating frequency in MHz
 * @n_subchans: number of 20 MHz subchannels in operating bandwidth
 * @start_subchan_idx: output start index in operation-channel bitmap
 *
 * Calculates where the received NOL frequency starts in the operating channel
 * bitmap so the RCSA NOL bitmap can be mapped to local channel state.
 *
 * Return: 0 on success, -1 otherwise.
 */
static int hostapd_get_rcsa_start_subchan(const dfs_nol_ie_info *rcsa_nol,
					  int base_freq, int n_subchans,
					  int *start_subchan_idx)
{
	if (!base_freq || rcsa_nol->freq < (u32) base_freq ||
	    ((int) rcsa_nol->freq - base_freq) % RCSA_MIN_DFS_SUBCHAN_BW) {
		wpa_printf(MSG_ERROR,
			   "RCSA: invalid NOL freq=%u base=%d",
			   rcsa_nol->freq, base_freq);
		return -1;
	}

	*start_subchan_idx = ((int) rcsa_nol->freq - base_freq) /
		RCSA_MIN_DFS_SUBCHAN_BW;
	if (*start_subchan_idx < 0 || *start_subchan_idx >= n_subchans) {
		wpa_printf(MSG_ERROR,
			   "RCSA: invalid NOL start=%d count=%d",
			   *start_subchan_idx, n_subchans);
		return -1;
	}

	return 0;
}

/**
 * hostapd_rcsa_idenitfy_nol_bitmap - Identify operating NOL bitmap
 * @rcsa_bitmap: bitmap parsed from the RCSA NOL IE
 * @rcsa_freq: first NOL subchannel frequency in MHz
 * @start_subchan_idx: start index in operation-channel bitmap
 * @n_subchans: number of 20 MHz subchannels in operating bandwidth
 *
 * Maps the RCSA NOL bitmap to the local operating-channel bitmap.
 *
 * Return: operation-channel NOL bitmap.
 */
static u16 hostapd_rcsa_idenitfy_nol_bitmap(u16 rcsa_bitmap,
					    u32 rcsa_freq,
					    int start_subchan_idx,
					    int n_subchans)
{
	u16 oper_bitmap = 0;
	int bit;

	for (bit = 0; bit < DFS_MAX_20M_SUB_CH; bit++) {
		int oper_bit;

		if (!(rcsa_bitmap & BIT(bit)))
			continue;

		oper_bit = start_subchan_idx + bit;
		if (oper_bit >= n_subchans) {
			wpa_printf(MSG_WARNING,
				   "RCSA: skip NOL bit=%d rcsa_freq=%u oper_bit=%d",
				   bit, rcsa_freq + bit * RCSA_MIN_DFS_SUBCHAN_BW,
				   oper_bit);
			continue;
		}

		oper_bitmap |= BIT(oper_bit);
	}

	return oper_bitmap;
}

/**
 * hostapd_extract_rcsa_nol_ie_bitmap - Extract NOL IE bitmap from RCSA
 * @iface: hostapd interface receiving the RCSA frame
 * @rcsa_nol: NOL IE information received from the RCSA message
 * @nol_info: output NOL information updated with extracted NOL IE data
 *
 * Extracts the received RCSA NOL IE, identifies the operating NOL bitmap,
 * and fills @nol_info with the extracted NOL IE information.
 *
 * Return: 0 on success, -1 otherwise.
 */
static int hostapd_extract_rcsa_nol_ie_bitmap(struct hostapd_iface *iface,
					      const dfs_nol_ie_info *rcsa_nol,
					      dfs_nol_ie_info *nol_info)
{
	u16 parsed_bitmap;
	u16 nol_ie_bitmap;
	int start_freq;
	int bandwidth_mhz;
	int cf1;
	int n_subchans;
	enum oper_chan_width oper_chwidth;
	int start_subchan_idx;

	if (hostapd_validate_rcsa_nol_info(iface, rcsa_nol, nol_info))
		return -1;

	parsed_bitmap = rcsa_nol->subchan_bitmap;
	if (hostapd_get_rcsa_oper_bw(iface, &bandwidth_mhz))
		return -1;

	hostapd_get_rcsa_oper_chwidth_cf1(iface, &oper_chwidth, &cf1);

	if (dfs_nol_ie_get_subchan_count(oper_chwidth, iface->freq, cf1,
					&n_subchans)) {
		wpa_printf(MSG_ERROR,
			   "RCSA: failed to get subchannel count bw=%d",
			   bandwidth_mhz);
		return -1;
	}

	if (n_subchans <= 0 || n_subchans > DFS_MAX_20M_SUB_CH) {
		wpa_printf(MSG_ERROR,
			   "RCSA: invalid subchannel count=%d bw=%d",
			   n_subchans, bandwidth_mhz);
		return -1;
	}

	start_freq = cf1 - (bandwidth_mhz / 2) + (RCSA_MIN_DFS_SUBCHAN_BW / 2);
	if (hostapd_get_rcsa_start_subchan(rcsa_nol, start_freq, n_subchans,
					   &start_subchan_idx))
		return -1;

	nol_ie_bitmap = hostapd_rcsa_idenitfy_nol_bitmap(parsed_bitmap,
							 rcsa_nol->freq,
							 start_subchan_idx,
							 n_subchans);

	if (!nol_ie_bitmap) {
		wpa_printf(MSG_ERROR,
			   "RCSA: empty NOL bitmap freq=%u start=%d bitmap=0x%04x",
			   rcsa_nol->freq, start_freq, parsed_bitmap);
		return -1;
	}

	nol_info->freq = rcsa_nol->freq - (start_subchan_idx * RCSA_MIN_DFS_SUBCHAN_BW);
	nol_info->bandwidth = bandwidth_mhz;
	nol_info->subchan_bitmap = nol_ie_bitmap;
	wpa_printf(MSG_DEBUG,
		   "RCSA: Parsed NOLIE freq: %d NOL bitmap =0x%04x Identified NOL bitmap =0x%04x NOL bw=%u",
		   nol_info->freq, parsed_bitmap, nol_info->subchan_bitmap, nol_info->bandwidth);

	return 0;
}

/**
 * hostapd_rcsa_notify_radar - Propagate radar event to driver based on
 *                             RCSA-derived NOL information
 * @hapd: hostapd BSS data
 *
 * Reuses the common DFS NOL bitmap processing so RCSA follows the same
 * per-20 MHz radar propagation path used by uplink_csa.
 */
static void hostapd_rcsa_notify_radar(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface;
	dfs_nol_ie_info *nol_info;

	if (!hapd || !hapd->iface || !hapd->iface->conf || !hapd->iconf) {
		wpa_printf(MSG_ERROR, "RCSA: invalid radar notify context");
		return;
	}

	iface = hapd->iface;
	nol_info = &iface->iface_extn.nol_info;

	if (!nol_info->freq || !nol_info->bandwidth ||
	    !nol_info->subchan_bitmap) {
		wpa_printf(MSG_ERROR,
			   "RCSA: invalid NOL info freq=%u bw=%u bitmap=0x%04x",
			   nol_info->freq, nol_info->bandwidth,
			   nol_info->subchan_bitmap);
		return;
	}

	if (hapd->iface->conf->use_ru_puncture_dfs &&
	    nol_info->bandwidth < DFS_NOL_IE_BW_80_MHZ) {
		wpa_printf(MSG_DEBUG,
			   "RCSA: Puncturing is not applicable for bandwidth less than 80 MHz");
		return;
	}

	if (dfs_process_nol_ie_bitmap_rcsa(hapd, nol_info))
		wpa_printf(MSG_WARNING,
			   "RCSA: failed to process parsed NOL IE freq=%u bw=%u bitmap=0x%04x",
			   nol_info->freq, nol_info->bandwidth,
			   nol_info->subchan_bitmap);
}

static bool hostapd_store_rcsa_nol_info(struct hostapd_iface *iface,
					const dfs_nol_ie_info *parsed_nol_info)
{
	dfs_nol_ie_info nol_info;

	if (hostapd_extract_rcsa_nol_ie_bitmap(iface, parsed_nol_info,
					       &nol_info))
		return false;

	os_memcpy(&iface->iface_extn.nol_info, &nol_info,
			sizeof(iface->iface_extn.nol_info));
	iface->iface_extn.nol_info_valid = true;

	return true;
}

/**
 * hostapd_parse_rcsa_nol_ie - Parse RCSA NOL IE from optional IEs
 * @iface: hostapd interface to update
 * @ies: optional IE buffer following the CSA IE
 * @ies_len: length of optional IE buffer
 *
 * Searches the received RCSA optional IEs for the NOL IE generated by
 * hostapd_build_nol_ie() by repeater. The NOL IE values which were prepared by
 * dfs_prepare_nol_ie_bitmap() from repeater is converted back to the
 * operation-channel bitmap by root AP.
 *
 * On success, updates iface->iface_extn.nol_info and sets
 * iface->iface_extn.nol_info_valid to true. Existing NOL state is cleared
 * before parsing to avoid using stale data. Missing NOL IE on the local
 * receiver is treated as full-bandwidth radar.
 *
 * Return: true when a valid NOL IE is parsed, false otherwise.
 */
static bool hostapd_parse_rcsa_nol_ie(struct hostapd_iface *iface,
				      const u8 *ies, size_t ies_len)
{
	const u8 *pos = ies;
	size_t rem_len = ies_len;
	dfs_nol_ie_info parsed_nol_info;
	int bandwidth_mhz = 0;
	int n_subchans = 0;
	int start_chan_idx = 0;
	int start_chan_idx1 = 0;
	int center_freq1 = 0;
	int oper_chwidth = 0;

	if (!iface)
		return false;

	iface->iface_extn.nol_info_valid = false;
	os_memset(&iface->iface_extn.nol_info, 0,
		  sizeof(iface->iface_extn.nol_info));
	if (!ies || !ies_len)
		rem_len = 0;

	while (rem_len >= 2) {
		size_t ie_len = (size_t) pos[1] + 2;

		if (ie_len > rem_len)
			return false;

		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC &&
		    dfs_decode_nol_ie(pos, ie_len, &parsed_nol_info) == 0) {
			wpa_hexdump(MSG_INFO, "RCSA: NOL IE parse raw", pos, ie_len);

			return hostapd_store_rcsa_nol_info(iface,
							   &parsed_nol_info);
		}

		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC &&
		    ie_len == RCSA_NOL_IE_TOTAL_LEN) {
			wpa_hexdump(MSG_INFO, "RCSA: legacy NOL IE parse raw",
				    pos, ie_len);
			os_memset(&parsed_nol_info, 0, sizeof(parsed_nol_info));
			parsed_nol_info.bandwidth = pos[2];
			parsed_nol_info.freq = WPA_GET_LE16(pos + 3);
			parsed_nol_info.subchan_bitmap = pos[5];

			return hostapd_store_rcsa_nol_info(iface,
							   &parsed_nol_info);
		}

		pos += ie_len;
		rem_len -= ie_len;
	}

	if (!iface->current_mode)
		return false;

	oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	center_freq1 = iface->freq;
	if (oper_chwidth == CONF_OPER_CHWIDTH_USE_HT &&
	    iface->conf->secondary_channel)
		center_freq1 += iface->conf->secondary_channel * 10;

	if (hostapd_get_rcsa_oper_bw(iface, &bandwidth_mhz))
		return false;

	if (dfs_nol_ie_get_subchan_count(oper_chwidth, iface->freq,
					 center_freq1,
					 &n_subchans))
		return false;

	if (n_subchans <= 0 || n_subchans > DFS_MAX_20M_SUB_CH)
		return false;

	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						oper_chwidth,
						iface->conf->channel, false);
	if (start_chan_idx < 0 ||
	    start_chan_idx >= iface->current_mode->num_channels)
		return false;

	iface->iface_extn.nol_info.freq =
		iface->current_mode->channels[start_chan_idx].freq;
	iface->iface_extn.nol_info.bandwidth = bandwidth_mhz;
	iface->iface_extn.nol_info.subchan_bitmap =
		DFS_NOL_IE_BITMAP_MASK(n_subchans);
	iface->iface_extn.nol_info_valid = true;
	wpa_printf(MSG_INFO,
		   "RCSA: Missing NOL IE treated as full-BW radar freq=%u bw=%u bitmap=0x%04x",
		   iface->iface_extn.nol_info.freq,
		   iface->iface_extn.nol_info.bandwidth,
		   iface->iface_extn.nol_info.subchan_bitmap);

	return true;
}

void hostapd_trigger_rcsa_tx(void *eloop_data, void *user_data)
{
	struct hostapd_iface *iface = eloop_data;
	struct hostapd_rcsa_ctx *rcsa_ctx;
	u8 chan;

	if (!iface)
		return;

	rcsa_ctx = &iface->iface_extn.rcsa_ctx;

	if (!hostapd_is_rcsa_inprogress(iface))
		return;

	if (hostapd_csa_in_progress(iface))
		return;

	if (rcsa_ctx->rcsa_tx_cnt <= 0)
		return;

	rcsa_ctx->rcsa_tx_cnt--;
	ieee80211_freq_to_chan(iface->freq, &chan);
	hostapd_ucode_notify_rcsa_tx(iface, chan, iface->freq,
				     IEEE80211_CSA_IE_MODE_OFFSET,
				     rcsa_ctx->optional_ie_len ? rcsa_ctx->optional_ie : NULL,
				     rcsa_ctx->optional_ie_len);

	if (rcsa_ctx->rcsa_tx_cnt >= 0)
		eloop_register_timeout(0, HOSTAPD_RCSA_INTVAL_US,
				       hostapd_trigger_rcsa_tx,
				       iface, NULL);
}

int hostapd_send_rcsa_extn(struct hostapd_iface *iface,
			   int channel, int freq,
			   int secondary_channel,
			   u8 current_vht_oper_chwidth,
			   u8 oper_centr_freq_seg0_idx,
			   u8 oper_centr_freq_seg1_idx,
			   u16 punct_bitmap)
{
	bool attach_nol_ie = false;
	bool attach_ml_ie = false;
	u16 link_id_bitmap = 0;
	u8 opt_ie[RCSA_MAX_OPTIONAL_IE_LEN];
	u8 nol_ie_buf[RCSA_MAX_OPTIONAL_IE_LEN];
	size_t opt_ie_len = 0;
	int nol_ie_len = 0;

	if (!iface)
		return -EINVAL;

	if (!hostapd_rcsa_tx_bh_enabled(iface)) {
		wpa_printf(MSG_INFO,
			   "BLOCKED — bh_enabled=NO sta_wpa_state=\"%.32s\" — RCSA will NOT be sent",
			   iface->iface_extn.sta_wpa_state);
		return -EINVAL;
	}

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_INFO,
			   "rcsa: defer TX because channel switch is already in progress");
		return 0;
	}

	if (hostapd_is_rcsa_inprogress(iface)) {
		wpa_printf(MSG_DEBUG,
			   "rcsa: inprogress");
		return -EINVAL;
	}

	nol_ie_len = hostapd_build_nol_ie(iface,
					  nol_ie_buf,
					  sizeof(nol_ie_buf));
	if (nol_ie_len > 0)
		attach_nol_ie = true;

	hostapd_get_local_rcsa_ml_info(iface, &attach_ml_ie, &link_id_bitmap);
	wpa_printf(MSG_DEBUG, "rcsa: attach_ml %u attach_nol %u",
		   attach_ml_ie, attach_nol_ie);

	opt_ie_len = hostapd_build_rcsa_optional_ies(
				attach_nol_ie ? nol_ie_buf : NULL,
				attach_nol_ie ? (size_t) nol_ie_len : 0,
				attach_ml_ie, link_id_bitmap,
				opt_ie, sizeof(opt_ie));

	hostapd_rcsa_store_optional_ie(iface, opt_ie, opt_ie_len);

	return hostapd_ucode_notify_rcsa_tx(iface, channel, freq,
					    HOSTAPD_RCSA_SWITCH_MODE,
					    opt_ie_len ? opt_ie : NULL,
					    opt_ie_len);
}

static int hostapd_parse_rcsa_frame(struct hostapd_data *hapd,
				    const u8 *buf, size_t len,
				    u8 *switch_mode, u8 *new_chan,
				    u8 *csa_count, const u8 *opt_ie,
				    size_t *opt_ie_len,
				    bool *mlinfo_present,
				    s8 *mlinfo_linkid)
{
	struct hostapd_data *target_hapd;
	struct hostapd_iface *iface;
	const u8 *pos;
	const u8 *end;
	const u8 *cs_ie;
	size_t rem_len;
	size_t copy_len;

	if (len < IEEE80211_HDRLEN + RCSA_VENDOR_ACTION_HDR_LEN) {
		wpa_printf(MSG_ERROR, "rcsa: frame too short");
		return -EINVAL;
	}

	end = buf + len;
	pos = buf + IEEE80211_HDRLEN + RCSA_VENDOR_ACTION_HDR_LEN;
	rem_len = end - pos;

	wpa_printf(MSG_DEBUG, "rcsa: received RCSA");
	cs_ie = get_ie(pos, rem_len, WLAN_EID_CHANNEL_SWITCH);
	if (!cs_ie || cs_ie[1] < IEEE80211_CSA_IE_MIN_LEN) {
		wpa_printf(MSG_ERROR, "rcsa: no valid CSA IE found");
		return -EINVAL;
	}

	*new_chan = cs_ie[IEEE80211_CSA_IE_NEW_CHANNEL_OFFSET];
	*csa_count = cs_ie[IEEE80211_CSA_IE_COUNT_OFFSET];
	*switch_mode = cs_ie[IEEE80211_CSA_IE_MODE_OFFSET];

	pos = cs_ie + IEEE80211_CSA_IE_TOTAL_LEN;
	rem_len = end - pos;

	if (rem_len >= 2 && opt_ie) {
		copy_len = rem_len < RCSA_MAX_OPTIONAL_IE_LEN ? rem_len : RCSA_MAX_OPTIONAL_IE_LEN;

		os_memcpy((u8 *)opt_ie, pos, copy_len);
		*opt_ie_len = copy_len;
	} else
		*opt_ie_len = 0;


	if (rem_len >= 2) {
		opt_ie = pos;
		*opt_ie_len = rem_len;
	}

	*mlinfo_present = optional_ml_info_ie_access((u8 *)opt_ie, *opt_ie_len,
						     mlinfo_linkid, 0);

	iface = hapd->iface;
	target_hapd = hapd;
	if (iface->bss[0]->conf->mld_ap &&
	    (*mlinfo_linkid != -1)) {
		target_hapd = switch_link_hapd(hapd, *mlinfo_linkid);
		if (!target_hapd)
			return 0;
	}

	iface = target_hapd->iface;
	if (!iface)
		return 0;

	hostapd_parse_rcsa_nol_ie(iface, pos, rem_len);
	wpa_printf(MSG_DEBUG,
		   "rcsa: chan %u, csa_cnt %u, switch_mode %u, chan %u, mlinfo_present %u, linkid %u",
		   *new_chan, *csa_count, *switch_mode, *new_chan,
		   *mlinfo_present, *mlinfo_linkid);

	return 0;
}

bool hostapd_rcsa_rx_hdl(struct hostapd_data *hapd,
			 const u8 *buf, size_t len)
{
	u8 opt_ie[RCSA_MAX_OPTIONAL_IE_LEN] = {0};
	u8 rebuilt_opt_ie[RCSA_MAX_OPTIONAL_IE_LEN] = {0};
	struct hostapd_iface *iface;
	struct hostapd_data *target_hapd;
	u8 switch_mode = 0;
	u8 new_chan = 0;
	u8 csa_count = 0;
	int ret = -1;
	s8 mlinfo_linkid = -1;
	size_t opt_ie_len = 0;
	bool mlinfo_present = false;
	size_t rebuilt_opt_ie_len = 0;
	bool include_ml_ie = false;
	u16 link_id_bitmap = 0;
	iface = hapd->iface;

	wpa_printf(MSG_DEBUG, "rcsa: received RCSA from repeater");

	if (hostapd_parse_rcsa_frame(hapd, buf, len,
				     &switch_mode, &new_chan,
				     &csa_count,
				     opt_ie, &opt_ie_len,
				     &mlinfo_present, &mlinfo_linkid))
		return 0;

	target_hapd = hapd;
	if (iface->bss[0]->conf->mld_ap &&
	    (mlinfo_linkid != -1)) {
		target_hapd = switch_link_hapd(hapd, mlinfo_linkid);
		if (!target_hapd)
			return 0;
	}

	iface = target_hapd->iface;
	if (!iface)
		return 0;

	if (!IS_CSH_PROCESS_RCSA_ENABLED(iface->conf->conf_extn.cswopts))
		return 1;

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_DEBUG,
			   "rcsa: defer forwarding because channel switch is already in progress");
		return 0;
	}

	if (hostapd_is_rcsa_inprogress(iface)) {
		wpa_printf(MSG_DEBUG, "rcsa: inprogress");
		return 1;
	}

	if (iface->iface_extn.nol_info_valid) {
		wpa_printf(MSG_DEBUG,
			   "RCSA: applying parsed NOL IE on local receiver");
		hostapd_rcsa_notify_radar(target_hapd);
		iface->iface_extn.nol_info_valid = false;
	}

	if (!hostapd_rcsa_rx_bh_enabled(iface))
		return 1;

	wpa_printf(MSG_INFO, "rcsa: proceeding with RCSA Tx");

	/* strip off ml-info iE */
	if (mlinfo_present &&  (opt_ie_len >= 5))
		opt_ie_len -= 5;

	hostapd_get_local_rcsa_ml_info(iface, &include_ml_ie, &link_id_bitmap);
	rebuilt_opt_ie_len = hostapd_build_rcsa_optional_ies(opt_ie_len ? opt_ie : NULL,
							     opt_ie_len,
							     include_ml_ie, link_id_bitmap,
							     rebuilt_opt_ie,
							     sizeof(rebuilt_opt_ie));

	hostapd_rcsa_store_optional_ie(iface, rebuilt_opt_ie, rebuilt_opt_ie_len);

	ret = hostapd_ucode_notify_rcsa_tx(iface, new_chan, iface->freq,
					   switch_mode,
					   rebuilt_opt_ie_len ? rebuilt_opt_ie : NULL,
					   rebuilt_opt_ie_len);

	return ret;
}

void hostapd_rcsa_trigger_channal_change(void *eloop_data, void *user_data)
{
	struct hostapd_iface *iface = eloop_data;
	struct hostapd_rcsa_ctx *rcsa_ctx;

	if (!iface)
		return;

	rcsa_ctx = &iface->iface_extn.rcsa_ctx;

	if (!hostapd_is_rcsa_inprogress(iface))
		return;


	if ((rcsa_ctx->bh_discon_wait_cnt <= 0) ||
	    (hostapd_csa_in_progress(iface))) {
		hostapd_set_rcsa_inprogress(iface, false);
		return;
	}

	rcsa_ctx->bh_discon_wait_cnt--;
	if (hostapd_is_bh_sta_link_connected_extn(iface)) {
		eloop_register_timeout(0, HOSTAPD_DFS_BH_DISCONNECT_WAIT_TIME_US,
				       hostapd_rcsa_trigger_channal_change,
				       iface, NULL);
		return;
	}
	wpa_printf(MSG_DEBUG, "rcsa: CSA timeout: trigger channel switch");
	hostapd_dfs_start_channel_switch(iface);
	hostapd_set_rcsa_inprogress(iface, false);
}

void hostapd_rcsa_handle_csa_timeout(struct hostapd_iface *iface)
{
	struct hostapd_rcsa_ctx *rcsa_ctx = &iface->iface_extn.rcsa_ctx;

	eloop_cancel_timeout(hostapd_trigger_rcsa_tx, iface, NULL);

	if(!hostapd_is_rcsa_inprogress(iface))
		return;

	if (!iface->conf->conf_extn.ind_rptr) {
		hostapd_set_rcsa_inprogress(iface, false);
		return;
	}

	rcsa_ctx->bh_discon_wait_cnt = 3;
	/* triggering CSA without waiting for BH disconnect would result
	 * in different chan ctx (BH's old chan ctx and new chan ctx) leading
	 * to csa failure in driver
	 */
	if (!eloop_is_timeout_registered(hostapd_rcsa_trigger_channal_change,
					 iface, NULL)) {
		eloop_register_timeout(0, HOSTAPD_DFS_BH_DISCONNECT_WAIT_TIME_US,
				       hostapd_rcsa_trigger_channal_change,
				       iface, NULL);
	}
}
