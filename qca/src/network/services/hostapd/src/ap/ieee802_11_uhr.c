/*
 * hostapd / IEEE 802.11bn UHR
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "hostapd.h"
#include "sta_info.h"
#include "ieee802_11.h"
#include "common/hw_features_common.h"
#include "common/ieee802_11_common.h"
#include "ap/ieee802_11.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "uhr_utils.h"
#include "ap_drv_ops.h"
#include "wpa_auth.h"
#include "wpa_auth_i.h"
#include "ap_mlme.h"
#include "ieee802_1x.h"


u8 * hostapd_eid_uhr_capab(struct hostapd_data *hapd, u8 *eid,
			    enum ieee80211_op_mode opmode)
{
	struct hostapd_hw_modes *mode;
	struct uhr_capabilities *uhr_cap;
	struct ieee80211_uhr_capabilities *cap;
	u8 *pos = eid, *length_pos;

	mode = hapd->iface->current_mode;
	if (!mode)
		return eid;

	uhr_cap = &mode->uhr_capab[opmode];
	if (!uhr_cap->uhr_supported)
		return eid;

	*pos++ = WLAN_EID_EXTENSION;
	length_pos = pos++;
	*pos++ = WLAN_EID_EXT_UHR_CAPABILITIES;

	cap = (struct ieee80211_uhr_capabilities *)pos;
	os_memset(cap, 0, sizeof(*cap));
	os_memcpy(cap->mac_cap, uhr_cap->mac_cap, sizeof(cap->mac_cap));

	/* Driver supports DPS Assist Support but disabled by user */
	if ((uhr_cap->mac_cap[0] & UHR_MACCAP_DPS_ASSIST) &&
	    hapd->conf->dps_assist == FEATURE_DISABLED)
		cap->mac_cap[0] &= ~UHR_MACCAP_DPS_ASSIST;

	cap->mac_cap[3] =
		(cap->mac_cap[3] &
		 ~UHR_MACCAP3_PARAM_UPD_ADV_NOTIF_INTV_MASK) |
		((hapd->conf->uhr_params_update.adv_notification_interval <<
		  UHR_MACCAP3_PARAM_UPD_ADV_NOTIF_INTV_SHIFT) &
		 UHR_MACCAP3_PARAM_UPD_ADV_NOTIF_INTV_MASK);

	cap->mac_cap[3] =
		(cap->mac_cap[3] &
		 ~UHR_MACCAP3_UPD_IND_TIM_INTV_LOW_MASK) |
		((hapd->conf->uhr_params_update.update_in_tim_interval <<
		  UHR_MACCAP3_UPD_IND_TIM_INTV_LOW_SHIFT) &
		 UHR_MACCAP3_UPD_IND_TIM_INTV_LOW_MASK);
	cap->mac_cap[4] =
		(cap->mac_cap[4] &
		 ~UHR_MACCAP4_UPD_IND_TIM_INTV_HIGH_MASK) |
		(((hapd->conf->uhr_params_update.update_in_tim_interval >> 1) <<
		  UHR_MACCAP4_UPD_IND_TIM_INTV_HIGH_SHIFT) &
		 UHR_MACCAP4_UPD_IND_TIM_INTV_HIGH_MASK);

	os_memcpy(cap->phy_cap, uhr_cap->phy_cap, sizeof(cap->phy_cap));

	if ((hapd->conf->uhr_phy_capab_mask & UHR_PHY_BSS_OVR_2XLDPC_TX) &&
	    !hapd->conf->uhr_phy_capab.uhr_2xldpc_tx)
		cap->phy_cap[UHR_PHYCAP_2XLDPC_TX_SUPP_IDX] &= ~UHR_PHYCAP_2XLDPC_TX_SUPP;

	if ((hapd->conf->uhr_phy_capab_mask & UHR_PHY_BSS_OVR_2XLDPC_RX) &&
	    !hapd->conf->uhr_phy_capab.uhr_2xldpc_rx)
		cap->phy_cap[UHR_PHYCAP_2XLDPC_RX_SUPP_IDX] &= ~UHR_PHYCAP_2XLDPC_RX_SUPP;

	pos += sizeof(struct ieee80211_uhr_capabilities);

	*length_pos = pos - (eid + 2);
	return pos;
}

size_t hostapd_eid_tx_power_indication_len(void)
{
	/* EID(1) + Len(1) + EID_Extension(1) + AP Conducted Tx Power(1) */
	return 4;
}

u8 * hostapd_eid_tx_power_indication(struct hostapd_data *hapd, u8 *eid)
{
	u8 fval = UHR_TX_PWR_IND_DEFAULT_FVAL;

	/* PTX = -20 + 2*Fval  per S9.4.2.364 */
	*eid++ = WLAN_EID_EXTENSION;

	/* EID_Extension(1) + AP Conducted Tx Power(1) */
	*eid++ = 2;
	*eid++ = WLAN_EID_EXT_TX_POWER_INDICATION;

	/* bits 0-4: FVal[Range: 0-30: value 31 reserved];
	 * bits 5-7 reserved set to 0
	 */
	fval = fval > 30 ? 30: fval;
	*eid++ = fval & 0x1f;
	return eid;
}

u8 * hostapd_eid_uhr_operation(struct hostapd_data *hapd, u8 *eid, bool is_bcn)
{
	struct ieee80211_uhr_operation *oper;
	u8 *pos = eid, *length_pos;
	struct hostapd_hw_modes *mode;
	struct uhr_npca_info *npca_info;
	bool npca_present;

	mode = hapd->iface->current_mode;
	if (!mode)
		return eid;

	*pos++ = WLAN_EID_EXTENSION;
	length_pos = pos++;
	*pos++ = WLAN_EID_EXT_UHR_OPERATION;

	oper = (struct ieee80211_uhr_operation *)pos;
	os_memset(oper, 0, sizeof(*oper));

	npca_info = &mode->npca_info[IEEE80211_MODE_AP];
	npca_present = npca_info->npca_supported &&
		       hapd->iconf->npca_enable;
	if (npca_present && !hapd->iconf->npca_primary_channel) {
		wpa_printf(MSG_DEBUG,
			   "NPCA: no primary channel configured, skipping NPCA params");
		npca_present = false;
	}
	if (npca_present)
		oper->uhr_oper_params |= UHR_OPER_NPCA_ENABLED;

	pos += sizeof(struct ieee80211_uhr_operation);

	if (is_bcn) {
		*length_pos = pos - (eid + 2);
		return pos;
	}


	if (npca_present) {
		u32 npca_params = 0;

		oper->uhr_oper_params |=
			host_to_le16(UHR_OPER_NPCA_OPER_PRESENT);

		npca_params |= ((u32) hapd->iconf->npca_primary_channel <<
				UHR_OPER_PARAMS_NPCA_PRIMARY_CHAN_SHIFT) &
			       UHR_OPER_PARAMS_NPCA_PRIMARY_CHAN;
		npca_params |= ((u32)npca_info->npca_min_dur_threshold <<
				UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH_SHIFT) &
			       UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH;
		npca_params |= ((u32)npca_info->npca_switch_delay <<
				UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY_SHIFT) &
			       UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY;
		npca_params |= ((u32)npca_info->npca_switch_back_delay <<
				UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY_SHIFT) &
			       UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY;
		npca_params |= ((u32)npca_info->npca_initial_qsrc <<
				UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC_SHIFT) &
			       UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC;
		npca_params |= ((u32)npca_info->npca_moplen <<
				UHR_OPER_PARAMS_NPCA_MOPLEN_NPCA_SHIFT) &
			       UHR_OPER_PARAMS_NPCA_MOPLEN_NPCA;
		if (hapd->iconf->npca_punct_bitmap)
			npca_params |= UHR_OPER_PARAMS_NPCA_DIS_SUBCH_BITMAP_PRES;

		WPA_PUT_LE32(pos, npca_params);
		pos += sizeof(u32);

		if (hapd->iconf->npca_punct_bitmap) {
			WPA_PUT_LE16(pos, hapd->iconf->npca_punct_bitmap);
			pos += sizeof(u16);
		}
	}

	*length_pos = pos - (eid + 2);
	return pos;
}


void hostapd_get_uhr_capab(const struct ieee80211_uhr_capabilities *src,
			   struct ieee80211_uhr_capabilities *dest,
			   size_t len)
{
	if (!src || !dest)
		return;

	if (len > sizeof(*dest))
		len = sizeof(*dest);
	/* TODO: mask out unsupported features */

	os_memset(dest, 0, sizeof(*dest));
	os_memcpy(dest, src, len);
}


static void uhr_smd_ctx_dump(const struct sta_smd_ctx_info *smd_ctx,
			     const char *prefix)
{
	int i;

#define SMD_BLOCKACK_BUFSIZE(a, b) ((((u16) (b)) << 10) | (a))

	if (!smd_ctx)
		return;

	wpa_printf(MSG_DEBUG, "%s: SMD Context Dump:", prefix);
	wpa_printf(MSG_DEBUG, "%s: ST Type=%u",
		   prefix, smd_ctx->st_type);
	wpa_printf(MSG_DEBUG, "%s: valid_ctx_bmap=0x%02x",
		   prefix, smd_ctx->valid_ctx_bmap);
	wpa_printf(MSG_DEBUG, "%s: pn_len=%u", prefix, smd_ctx->pn_len);

	/* DL Context */
	if (smd_ctx->dl.valid_tid_bmap)
		wpa_printf(MSG_DEBUG, "%s: [TX CONTEXT] valid_tid_bmap=0x%02x",
			   prefix, smd_ctx->dl.valid_tid_bmap);

	/* SN */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_DL_SN) {
		for (i = 0; i < SMD_NUM_TIDS; i++) {
			if (!(smd_ctx->dl.valid_tid_bmap & BIT(i)))
				continue;

			wpa_printf(MSG_DEBUG, "%s:   TID=%u SN=%u", prefix, i,
				   smd_ctx->dl.sn[i]);
		}
	}

	/* PN */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_PN) {
		wpa_printf(MSG_DEBUG, "%s:   PN (len=%u):",
			   prefix, smd_ctx->pn_len);
		wpa_hexdump(MSG_DEBUG, "    ", smd_ctx->dl.pn,
			    smd_ctx->pn_len);
	}

	/* BlockAck Parameters */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_BA_PARAMS) {
		const struct sta_smd_ba_info *ba;

		for (i = 0; i < SMD_NUM_TIDS; i++) {
			if (!(smd_ctx->dl.valid_tid_bmap & BIT(i)))
				continue;

			ba = &smd_ctx->dl.ba[i];
			wpa_printf(MSG_DEBUG,
				   "%s:   BlockAck: TID=%u policy=%u "
				   "full_buf_size=%u timeout=%u ext_no_frag=%d"
				   "extfrag_level=%u",
				   prefix, i,
				   ba->ba_policy,
				   SMD_BLOCKACK_BUFSIZE(ba->buffer_size,
							ba->ext_buffer_size),
				   ba->timeout,
				   ba->ext_no_frag,
				   ba->extfrag_level);
		}
	}

	/* UL Context */
	if (smd_ctx->ul.valid_tid_bmap)
		wpa_printf(MSG_DEBUG, "%s: [RX CONTEXT] valid_tid_bmap=0x%02x",
			   prefix, smd_ctx->ul.valid_tid_bmap);

	/* SN */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_UL_SN) {
		for (i = 0; i < SMD_NUM_TIDS; i++) {
			if (!(smd_ctx->ul.valid_tid_bmap & BIT(i)))
				continue;

			wpa_printf(MSG_DEBUG, "%s:   TID=%u SN=%u",
				   prefix, i, smd_ctx->ul.sn[i]);
		}
	}

	/* PN */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_PN) {
		for (i = 0; i < SMD_NUM_TIDS; i++) {
			if (!(smd_ctx->ul.valid_tid_bmap & BIT(i)))
				continue;

			wpa_printf(MSG_DEBUG,
				   "%s:   PN[%d] (len=%u):",
				   prefix, i, smd_ctx->pn_len);
			wpa_hexdump(MSG_DEBUG, "    ",
				    (u8 *) smd_ctx->ul.pn[i],
				    smd_ctx->pn_len);
		}
	}

	/* Block Ack Parameters */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_BA_PARAMS) {
		const struct sta_smd_ba_info *ba;

		for (i = 0; i < SMD_NUM_TIDS; i++) {
			if (!(smd_ctx->ul.valid_tid_bmap & BIT(i)))
				continue;

			ba = &smd_ctx->ul.ba[i];
			wpa_printf(MSG_DEBUG,
				   "%s:   BlockAck: TID=%u policy=%u "
				   "full_buf_size=%u timeout=%u ext_no_frag=%d"
				   "extfrag_level=%u",
				   prefix, i,
				   ba->ba_policy,
				   SMD_BLOCKACK_BUFSIZE(ba->buffer_size,
							ba->ext_buffer_size),
				   ba->timeout,
				   ba->ext_no_frag,
				   ba->extfrag_level);
		}
	}

	/* QoS Context */
	if (smd_ctx->valid_ctx_bmap & SMD_CTX_VALID_QOS) {
		wpa_printf(MSG_DEBUG, "%s: [QOS CONTEXT]", prefix);

		/* SCS Descriptors */
		for (i = 0; i < SMD_NUM_SCSID; i++) {
			if (smd_ctx->qos.scs_descriptors[i]) {
				wpa_printf(MSG_DEBUG,
					   "%s:   SCS[%d]: present (ptr=%p)",
					   prefix, i,
					   smd_ctx->qos.scs_descriptors[i]);
			}
		}

		/* MSCS Descriptor */
		if (smd_ctx->qos.mscs_descriptor)
			wpa_printf(MSG_DEBUG, "%s:   MSCS: present (ptr=%p)",
				   prefix, smd_ctx->qos.mscs_descriptor);
	}

	/* Vendor Context */
	if (smd_ctx->vendor_ctx_len) {
		wpa_printf(MSG_DEBUG, "%s: [VENDOR CONTEXT]", prefix);
		wpa_printf(MSG_DEBUG, "%s:   length: %zu bytes",
			   prefix, smd_ctx->vendor_ctx_len);
		wpa_hexdump(MSG_DEBUG, "    ",
			    smd_ctx->vendor_ctx, smd_ctx->vendor_ctx_len);
	}
}



static bool ieee80211_invalid_uhr_cap_size(size_t len)
{
	return len < sizeof(struct ieee80211_uhr_capabilities);
}


u16 copy_sta_uhr_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *uhr_capab, size_t uhr_capab_len)
{
	if (!hostapd_is_uhr_enabled(hapd) ||
	    !uhr_capab ||
	    ieee80211_invalid_uhr_cap_size(uhr_capab_len)) {
		sta->flags &= ~WLAN_STA_UHR;
		os_free(sta->uhr_capab);
		sta->uhr_capab = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	os_free(sta->uhr_capab);
	sta->uhr_capab = os_memdup(uhr_capab, uhr_capab_len);
	if (!sta->uhr_capab) {
		sta->uhr_capab_len = 0;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_UHR;
	sta->uhr_capab_len = uhr_capab_len;

	return WLAN_STATUS_SUCCESS;
}

void hostapd_update_ecu_params(struct hostapd_data *hapd)
{
	if (!hapd->conf->uhr_params_update.mode_changed)
		return;

	if (hapd->conf->uhr_params_update.mode_changed &
	    BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA)) {
		const struct hostapd_uhr_npca_params *npca =
			&hapd->conf->uhr_params_update.npca;

		hapd->iconf->npca_enable = npca->enable;
		hapd->iconf->npca_primary_channel =
			hostapd_npca_get_primary_chan(hapd, npca);
		hapd->iconf->npca_punct_bitmap =
			(npca->params &
			 UHR_OPER_PARAMS_NPCA_DIS_SUBCH_BITMAP_PRES) ?
			npca->disabled_subchan_bitmap : 0;
	}

	/* TODO: update for other ECU features */
}

void hostapd_reset_uhr_cu_params(struct hostapd_data *hapd)
{
	if (!hapd->conf->uhr_params_update.mode_changed)
		return;

	os_memset(&hapd->conf->uhr_params_update.npca, 0,
		  sizeof(hapd->conf->uhr_params_update.npca));

	hostapd_update_ecu_params(hapd);

	hapd->conf->uhr_params_update.mode_changed = 0;
}

/* mode_ctrl(1) is always present; mode_len(1) is present when the mode
 * is enabled (mandatory even when mode_params_len == 0).
 * Per 9.4.2.362: Mode Length and Mode Specific Parameters fields are not
 * included if Mode Enable is 0 or the mode has no parameters.
 */
static size_t uhr_mode_tuple_hdr_len(bool enable, size_t mode_params_len)
{
	size_t len = 1; /* mode_ctrl */

	if (enable)
		len += 1; /* mode_len */

	return len;
}


static u8 * uhr_put_mode_tuple_hdr(u8 *pos, u8 mode_id, bool enable,
				    bool update, u8 mode_len)
{
	u8 mode_ctrl = mode_id & UHR_MODE_TUPLE_MODE_ID_MASK;

	if (enable)
		mode_ctrl |= UHR_MODE_TUPLE_MODE_ENABLE;
	if (enable && update)
		mode_ctrl |= UHR_MODE_TUPLE_MODE_UPDATE;
	*pos++ = mode_ctrl;

	if (enable)
		*pos++ = mode_len;

	return pos;
}


/* NPCA (Mode ID = 1): Mode Specific Parameters = NPCA Operation Parameters
 * field (9.4.2.355.2), 4 or 6 octets. Present only when Mode Enable = 1.
 */
static size_t uhr_npca_mode_tuple_len(const struct hostapd_bss_config *conf)
{
	const struct hostapd_uhr_npca_params *npca =
		&conf->uhr_params_update.npca;
	size_t params_len;

	params_len = 0;
	if (npca->enable) {
		params_len = 4;
		if (npca->params & UHR_OPER_PARAMS_NPCA_DIS_SUBCH_BITMAP_PRES)
			params_len += 2;
	}

	return uhr_mode_tuple_hdr_len(npca->enable, params_len) + params_len;
}


static u8 * uhr_put_npca_mode_tuple(u8 *pos,
				    const struct hostapd_bss_config *conf)
{
	const struct hostapd_uhr_npca_params *npca =
		&conf->uhr_params_update.npca;
	bool bitmap_present;
	u8 mode_len;

	bitmap_present = npca->enable &&
			 !!(npca->params &
				UHR_OPER_PARAMS_NPCA_DIS_SUBCH_BITMAP_PRES);
	mode_len = npca->enable ?
		4 + (bitmap_present ? 2 : 0) : 0;

	pos = uhr_put_mode_tuple_hdr(pos, UHR_PARAMS_UPDATE_MODE_ID_NPCA,
				     npca->enable, true, mode_len);
	if (npca->enable) {
		WPA_PUT_LE32(pos, npca->params);
		pos += 4;
		if (bitmap_present) {
			WPA_PUT_LE16(pos, npca->disabled_subchan_bitmap);
			pos += 2;
		}
	}

	return pos;
}


size_t hostapd_eid_uhr_params_update_len(struct hostapd_data *hapd,
					 bool skip_post_phase,
					 bool from_user)
{
	const struct hostapd_bss_config *conf = hapd->conf;
	size_t len;

	if (!from_user && hapd->uhr_ecu.state == UHR_ECU_IDLE)
		return 0;

	if (hapd->uhr_ecu.state == UHR_ECU_UPDATE_IND_IN_TIM)
		return 0;

	/*
	 * Per 37.30.2.2: during the post-notification phase the element is
	 * included only in Beacon and Probe Response frames, not in
	 * (Re)Association Response or Link Reconfiguration Response frames.
	 */
	if (skip_post_phase &&
	    hapd->uhr_ecu.state >= UHR_ECU_POST_ADVANCE_NOTIFY)
		return 0;

	if (!conf->uhr_params_update.mode_changed)
		return 0;

	/* EID(1) + Length(1) + EID_EXT(1) + Countdown Timer(1) */
	len = 4;

	if (conf->uhr_params_update.mode_changed & BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA))
		len += uhr_npca_mode_tuple_len(conf);
	/* TODO: Add length for DPS, DUO, P-EDCA, DBE, AP PUO, ELR modes */

	if (len == 4)
		return 0;

	return len;
}


u8 * hostapd_eid_uhr_params_update(struct hostapd_data *hapd, u8 *eid,
				   bool skip_post_phase, bool from_user)
{
	const struct hostapd_bss_config *conf = hapd->conf;
	u8 *pos = eid;
	u8 *length_pos;

	if (!from_user && hapd->uhr_ecu.state == UHR_ECU_IDLE)
		return eid;

	if (hapd->uhr_ecu.state == UHR_ECU_UPDATE_IND_IN_TIM)
		return eid;

	if (skip_post_phase &&
	    hapd->uhr_ecu.state >= UHR_ECU_POST_ADVANCE_NOTIFY)
		return eid;

	if (!conf->uhr_params_update.mode_changed)
		return eid;

	*pos++ = WLAN_EID_EXTENSION;
	length_pos = pos++;
	*pos++ = WLAN_EID_EXT_UHR_PARAMS_UPDATE;

	*pos++ = hapd->uhr_ecu.countdown_timer;

	/* Mode Tuple List (9.4.2.362) */
	/* TODO: Add Mode Tuple for DPS (Mode ID = 0) */
	if (conf->uhr_params_update.mode_changed & BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA))
		pos = uhr_put_npca_mode_tuple(pos, conf);
	/* TODO: Add Mode Tuple for DUO (Mode ID = 2) */
	/* TODO: Add Mode Tuple for P-EDCA (Mode ID = 3) */
	/* TODO: Add Mode Tuple for DBE (Mode ID = 4) */
	/* TODO: Add Mode Tuple for AP PUO (Mode ID = 5) */
	/* TODO: Add Mode Tuple for ELR Reception (Mode ID = 6) */

	*length_pos = pos - (eid + 2);
	return pos;
}

/**
 * hostapd_npca_primary_chan_validate - Validate and resolve a user-supplied
 * NPCA primary channel value to a channel number.
 *
 * @hapd: hostapd BSS data
 * @val_str: string containing a frequency in MHz (> 233) or a channel number
 *
 * The function validates that:
 *  - The BSS bandwidth is at least 80 MHz (NPCA requirement).
 *  - The resolved frequency falls on a 20 MHz subchannel boundary inside the
 *    BSS bandwidth.
 *  - The NPCA primary differs from the BSS primary channel.
 *  - The NPCA primary lies in the half of the BSS bandwidth that is opposite
 *    to the BSS primary channel (secondary half).
 *
 * Returns: NPCA primary channel number (1-255) on success, -1 on error.
 */
int hostapd_npca_primary_chan_validate(struct hostapd_data *hapd,
				       const char *val_str)
{
	int user_val = atoi(val_str);
	int target_freq;
	int target_chan;
	u8 center_chan_no = hostapd_get_oper_centr_freq_seg0_idx(hapd->iconf);
	int bss_freq = ieee80211_chan_to_freq(NULL, hapd->iconf->op_class,
					     center_chan_no);
	enum oper_chan_width chwidth;
	int bss_bw_mhz;
	int lowest_freq;
	int half;
	int bss_primary_freq;
	bool primary_in_lower;
	bool npca_in_lower;

	if (bss_freq < 0) {
		int bss_primary = hapd->iface->freq;

		if (is_6ghz_freq(bss_primary))
			bss_freq = 5950 + center_chan_no * 5;
		else if (is_5ghz_freq(bss_primary))
			bss_freq = 5000 + center_chan_no * 5;
		else
			bss_freq = 2407 + center_chan_no * 5;
	}

	chwidth = hostapd_get_oper_chwidth(hapd->iconf);

	/* Determine BSS bandwidth in MHz */
	switch (chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		bss_bw_mhz = 320;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		bss_bw_mhz = 160;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		bss_bw_mhz = 80;
		break;
	case CONF_OPER_CHWIDTH_40MHZ_6GHZ:
		bss_bw_mhz = 40;
		break;
	default: /* CONF_OPER_CHWIDTH_USE_HT = 20 or 40 MHz */
		bss_bw_mhz = hapd->iconf->secondary_channel ? 40 : 20;
		break;
	}

	/* Spec: NPCA requires >= 80 MHz BSS BW */
	if (bss_bw_mhz < 80) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: NPCA requires "
			   "at least 80 MHz BSS bandwidth "
			   "(current: %d MHz)",
			   bss_bw_mhz);
		return -1;
	}

	/*
	 * Convert channel number to frequency if needed.
	 * Values > 233 are unambiguously a frequency in MHz.
	 * Values <= 233 are a channel number; derive frequency using the VAP's
	 * operating class so that e.g. channel 6 on a 2.4 GHz VAP and channel 6
	 * on a 6 GHz VAP are handled correctly without ambiguity. If op_class is
	 * not configured (0), fall back to band-based conversion derived from the
	 * BSS primary channel frequency.
	 */
	if (user_val > 233) {
		target_freq = user_val;

		if (is_6ghz_freq(target_freq))
			target_chan = (target_freq - 5950) / 5;
		else if (is_5ghz_freq(target_freq))
			target_chan = (target_freq - 5000) / 5;
		else
			target_chan = (target_freq - 2407) / 5;
	} else {
		target_chan = user_val;
		target_freq = ieee80211_chan_to_freq(NULL, hapd->iconf->op_class,
						    (u8) user_val);
		if (target_freq < 0) {
			int bss_primary = hapd->iface->freq;

			if (is_6ghz_freq(bss_primary))
				target_freq = 5950 + user_val * 5;
			else if (is_5ghz_freq(bss_primary))
				target_freq = 5000 + user_val * 5;
			else
				target_freq = 2407 + user_val * 5;
		}
	}

	if (target_chan <= 0 || target_chan > 255) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: primary_chan resolves to "
			   "invalid channel number %d", target_chan);
		return -1;
	}

	/*
	 * Compute the lowest 20 MHz subchannel frequency of the BSS:
	 * center_freq - bss_bw/2 + 10 MHz.
	 */
	lowest_freq = bss_freq - bss_bw_mhz / 2 + 10;

	if (target_freq < lowest_freq ||
	    target_freq >= lowest_freq + bss_bw_mhz ||
	    (target_freq - lowest_freq) % 20 != 0) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: primary_chan freq %d MHz "
			   "is not a 20 MHz subchannel within the BSS "
			   "bandwidth (center %d MHz, %d MHz wide)",
			   target_freq, bss_freq, bss_bw_mhz);
		return -1;
	}

	/* Spec: NPCA primary must differ from BSS primary */
	if (target_freq == hapd->iface->freq) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: primary_chan freq "
			   "%d MHz is the BSS primary channel; "
			   "NPCA primary must be different",
			   target_freq);
		return -1;
	}

	/*
	 * Spec: NPCA primary must be in the secondary half of the BSS
	 * bandwidth:
	 *   80 MHz  -> secondary 40 MHz
	 *   160 MHz -> secondary 80 MHz
	 *   320 MHz -> secondary 160 MHz
	 *
	 * The BSS primary channel (iface->freq) sits in one half; the NPCA
	 * primary must be in the other.
	 * Half-bandwidth = bss_bw_mhz / 2.
	 * Primary half:   [lowest_freq, lowest_freq + half)
	 * Secondary half: [lowest_freq + half, lowest_freq + bss_bw_mhz)
	 */
	half = bss_bw_mhz / 2;
	bss_primary_freq = hapd->iface->freq;
	primary_in_lower = (bss_primary_freq >= lowest_freq &&
			    bss_primary_freq < lowest_freq + half);
	npca_in_lower = (target_freq >= lowest_freq &&
			 target_freq < lowest_freq + half);

	if (primary_in_lower == npca_in_lower) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: primary_chan "
			   "freq %d MHz is not in the secondary "
			   "%d MHz of the BSS (center %d MHz, "
			   "%d MHz wide); NPCA primary must be "
			   "in the half opposite to the BSS "
			   "primary channel (%d MHz)",
			   target_freq, half,
			   bss_freq, bss_bw_mhz,
			   bss_primary_freq);
		return -1;
	}

	return target_chan;
}

/**
 * hostapd_npca_get_primary_chan - Extract the NPCA primary channel number
 * from a received NPCA Operation Parameters field
 * @hapd: hostapd BSS data (unused; kept for API symmetry with callers that
 *	previously needed BSS bandwidth context to recover a subchannel index)
 * @npca: parsed NPCA parameters, with @npca->params holding the raw wire
 *	value of the NPCA Operation Parameters field
 *
 * Per IEEE P802.11bn/D1.5, the NPCA Primary Channel field carries the real
 * channel number directly (0-255), not a subchannel index, so this is a
 * straight bitfield extraction.
 *
 * Returns: the NPCA primary channel number
 */
u8 hostapd_npca_get_primary_chan(struct hostapd_data *hapd,
				 const struct hostapd_uhr_npca_params *npca)
{
	return (u8)(npca->params & UHR_OPER_PARAMS_NPCA_PRIMARY_CHAN);
}

/* Build and send an OTA ST Prep Response with failure status to the STA.
 * Used when the current AP rejects a new prep request due to an in-flight
 * operation for the same target AP. */
static void uhr_cur_ap_send_st_prep_fail(struct hostapd_data *hapd,
					  struct sta_info *sta,
					  const u8 *frame, size_t frame_len)
{
	/* hdr(24) + category(1) + action(1) + token(1) + type(1) + status(2) + count(1) */
	u8 resp_buf[IEEE80211_HDRLEN + 7];
	struct ieee80211_mgmt *mgmt;
	u8 *pos;
	const struct ieee80211_mgmt *req;

	if (frame_len < WLAN_ST_PREP_MIN_LEN) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: ST Prep fail frame too short (%zu)", frame_len);
		return;
	}

	req = (const struct ieee80211_mgmt *) frame;

	os_memset(resp_buf, 0, sizeof(resp_buf));
	mgmt = (struct ieee80211_mgmt *)resp_buf;
	mgmt->frame_control = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
					   (WLAN_FC_STYPE_ACTION << 4));
	os_memcpy(mgmt->da, sta->addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->mld->mld_addr, ETH_ALEN);

	pos = (u8 *)&mgmt->u.action;
	*pos++ = WLAN_ACTION_PROTECTED_UHR;
	*pos++ = WLAN_PROT_UHR_LINK_RECONFIG_RESPONSE; /* Action */
	*pos++ = req->u.action.u.uhr_link_reconf_req.dialog_token;
	*pos++ = UHR_LINK_RECONFIG_TYPE_PREP;          /* Type = ST Preparation */
	WPA_PUT_LE16(pos, WLAN_STATUS_UNSPECIFIED_FAILURE);
	pos += 2;
	*pos   = 0;               /* Count = 0 */

	hostapd_drv_send_mlme(hapd, resp_buf, sizeof(resp_buf),
			      0, NULL, 0, 0, 0, 0);
}


int uhr_handle_st_prep_req(struct hostapd_data *hapd,
				   struct sta_info *sta,
				   const u8 *frame, size_t frame_len,
				   struct sta_smd_ctx_info *smd_ctx)
{
	struct ieee802_11_elems elems;
	struct uhr_reconfig_mle mle;
	struct uhr_smd_bss_transition_element sbte;
	struct smd_roam_ap_info *ap_info;
	const u8 *ies;
	size_t ies_len;
	int is_new_ap = 0;

	if (!hapd || !sta || !frame || frame_len == 0) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Invalid parameters");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR Current AP: Processing request from STA " MACSTR " (frame_len=%zu)",
		   MAC2STR(sta->addr), frame_len);

	if (frame_len < WLAN_ST_PREP_MIN_LEN) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Frame too short (%zu < 28)",
			   frame_len);
		return -1;
	}

	ies = frame + WLAN_ST_PREP_MIN_LEN;
	ies_len = frame_len - WLAN_ST_PREP_MIN_LEN;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Failed to parse IEs");
		return -1;
	}

	if (uhr_parse_reconfig_mle(&elems, &mle) < 0) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Failed to parse ML-IE");
		return -1;
	}

	/* Parse SMD BSS Transition IE */
	if (uhr_parse_smd_bss_trans_elem(&elems, 0, &sbte) < 0) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Failed to parse SBTE");
		return -1;
	}

	if (!mle.has_target_ap_mld_addr) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: No target AP MLD address in ML-IE");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR Current AP: Target AP MLD " MACSTR ,
		   MAC2STR(mle.target_ap_mld_addr));

	/* Find target AP in roaming candidate list */
	ap_info = uhr_find_ap_in_list(sta, mle.target_ap_mld_addr);
	if (!ap_info) {
		wpa_printf(MSG_DEBUG,
			   "UHR Current AP: Target AP " MACSTR " not in list, creating entry",
			   MAC2STR(mle.target_ap_mld_addr));

		/* CREATE new ap_info entry for fresh ST preparation */
		ap_info = os_zalloc(sizeof(*ap_info));
		if (!ap_info) {
			wpa_printf(MSG_ERROR,
				   "UHR Current AP: Failed to allocate ap_info");
			return -1;
		}

		os_memcpy(ap_info->ap_mld_addr, mle.target_ap_mld_addr, ETH_ALEN);
		os_get_reltime(&ap_info->last_seen);

		/* ADD ap_info to list now so all error paths use uhr_remove_ap_from_list() */
		ap_info->next = sta->smd_info.ap_list;
		sta->smd_info.ap_list = ap_info;
		is_new_ap = 1;

		wpa_printf(MSG_DEBUG,
			   "UHR Current AP: Created ap_info for " MACSTR ,
			   MAC2STR(ap_info->ap_mld_addr));
	}

	/* Reject a new prep while a previous prep or exec for this target is
	 * still in flight.  ST_PREP_COMPLETE is the only existing state that
	 * is legal to re-enter (STA re-prep after full OTA round-trip). */
	if (!is_new_ap &&
	    ap_info->state != SMD_AP_STATE_IDLE &&
	    ap_info->state != SMD_AP_STATE_ST_PREP_COMPLETE) {
		wpa_printf(MSG_DEBUG,
			   "UHR Current AP: ST Prep for " MACSTR
			   " rejected, in-flight state %d",
			   MAC2STR(mle.target_ap_mld_addr), ap_info->state);
		uhr_cur_ap_send_st_prep_fail(hapd, sta, frame, frame_len);
		return 0;
	}
	if (smd_ctx) {
		uhr_smd_ctx_dump(smd_ctx, "UHR ST Prep");
		if (smd_ctx->st_type != 0) {
			wpa_printf(MSG_ERROR,
				   "UHR Current AP: ST Type invalid, "
				   "expected 0 (Prep) but got %u",
				   smd_ctx->st_type);
			if (is_new_ap)
				uhr_remove_ap_from_list(sta, ap_info->ap_mld_addr);
			return -1;
		} else {
			ap_info->smd_ctx_valid = 1;
			ap_info->smd_ctx = smd_ctx;
		}
	}

	wpa_printf(MSG_DEBUG, "UHR Current AP: Target AP validated, sending IAP request");

	sta->dl_sn_not_transferred = sbte.dl_sn_not_transferred;
	sta->ul_sn_not_transferred = sbte.ul_sn_not_transferred;

	u32 role = 1;
	u32 type = 0;
	u32 dl_sn_not_transferred = sta->dl_sn_not_transferred;
	u32 ul_sn_not_transferred = sta->ul_sn_not_transferred;
	u32 dl_drain_time = hapd->conf->smd.uhr_dl_drain_duration_tu;
	if (hostapd_smd_roam(hapd, sta, role, type, dl_sn_not_transferred, ul_sn_not_transferred, dl_drain_time)) {
		wpa_printf(MSG_DEBUG, "UHR Current AP: Failed to send WMI roam notification - not skipping for now.");
	}

	/* Send IAP request to target AP with complete frame */
	ap_info->state = SMD_AP_STATE_ST_PREP_STARTED;
	if (uhr_iap_send_st_prep_req(hapd, mle.target_ap_mld_addr, sta,
				 frame, frame_len) < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: Failed to send IAP request");
		ap_info->state = SMD_AP_STATE_IDLE;
		if (is_new_ap)
			uhr_remove_ap_from_list(sta, ap_info->ap_mld_addr);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR Current AP: IAP request sent, waiting for response");
	if (uhr_cur_start_iap_msg_timer(sta, mle.target_ap_mld_addr) < 0) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Failed to start ST prep timeout");
	}

	return 0;
}

/*
 * uhr_cur_ap_clone_ap_info_to_partners - Clone an ap_info entry to all partner link stations
 *
 * After a successful ST Prep, the prepared AP entry must be visible on every
 * MLD link so that an ST Execute can arrive on any link and find it.  The ST
 * prep timer stays registered only on the originating (sta, ap_info) pair; the
 * clones carry no timer state and act as read-only lookup entries.
 */
static void uhr_cur_ap_clone_ap_info_to_partners(struct hostapd_data *lhapd,
						  struct sta_info *sta,
						  struct smd_roam_ap_info *ap_info)
{
	struct hostapd_data *partner_hapd;
	struct sta_info *partner_sta;
	struct smd_roam_ap_info *clone;

	for_each_mld_link(partner_hapd, lhapd) {
		if (partner_hapd == lhapd)
			continue;

		partner_sta = ap_get_sta(partner_hapd, sta->addr);
		if (!partner_sta)
			continue;

		if (uhr_find_ap_in_list(partner_sta, ap_info->ap_mld_addr))
			continue;

		clone = os_zalloc(sizeof(*clone));
		if (!clone)
			continue;

		os_memcpy(clone, ap_info, sizeof(*clone));
		clone->uhr_st_iap_timer_ongoing = false;
		clone->uhr_st_prep_timeout_occurred = false;
		clone->next = partner_sta->smd_info.ap_list;
		partner_sta->smd_info.ap_list = clone;

		wpa_printf(MSG_DEBUG,
			   "UHR Current AP: Cloned ap_info for " MACSTR
			   " to partner link %u sta current_link %u " MACSTR,
			   MAC2STR(ap_info->ap_mld_addr),
			   partner_hapd->mld_link_id,
			   MAC2STR(partner_sta->addr),
			   lhapd->mld_link_id);
	}
}

void uhr_cur_ap_handle_st_prep_resp(struct hostapd_data *hapd,
				    const struct uhr_iap_frame *iap,
	    			    u16 frame_len)
{
	struct sta_info *sta;
	struct hostapd_data *lhapd;
	const u8 *frame;
	struct ieee80211_mgmt *mgmt_hdr = NULL;
	struct smd_roam_ap_info *ap_info;

	if (!hapd || !iap) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Invalid parameters for IAP response");
		return;
	}
	lhapd = hostapd_mld_get_link_bss(hapd, iap->current_link_id);
	if (!lhapd) {
		wpa_printf(MSG_ERROR, "UHR Current AP: Invalid link-id routing for IAP response");
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR Current AP: Received IAP response (txn=%u, status=%u, frame_len=%u)",
		   iap->iap_transaction_id, iap->status_code, frame_len);

	/* Find STA */
	sta = ap_get_sta(lhapd, iap->sta_addr);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: STA " MACSTR " not found",
			   MAC2STR(iap->sta_addr));
		return;
	}

	if (frame_len == 0) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: No response frame in IAP response");
		uhr_remove_ap_from_list(sta, iap->target_ap_mld_addr);
		return;
	}

	frame = iap->frame_ctx_data;
	if (frame_len < offsetof(struct ieee80211_mgmt, bssid) + ETH_ALEN) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: Frame too short for MAC header (%u < %zu)",
			   frame_len,
			   offsetof(struct ieee80211_mgmt, bssid) + ETH_ALEN);
		uhr_remove_ap_from_list(sta, iap->target_ap_mld_addr);
		return;
	}
	mgmt_hdr = (struct ieee80211_mgmt *)frame;
	os_memcpy(mgmt_hdr->sa, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(mgmt_hdr->bssid, hapd->mld->mld_addr, ETH_ALEN);
	wpa_hexdump(MSG_MSGDUMP, "UHR Current AP: Response frame",
		    frame, frame_len);

	ap_info = uhr_find_ap_in_list(sta, iap->target_ap_mld_addr);
	if (!ap_info)
		return;
	uhr_cancel_iap_timeout(sta, iap->target_ap_mld_addr);

	/* Check status code */
	if (iap->status_code != UHR_IAP_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: IAP request failed (status=%u)",
			   iap->status_code);

		/* Remove failed AP from candidate list */
		uhr_remove_ap_from_list(sta, iap->target_ap_mld_addr);

		if (hostapd_drv_send_mlme(lhapd, frame, frame_len, 0, NULL, 0, 0, 0, 0) < 0) {
			wpa_printf(MSG_ERROR,
				   "UHR Current AP: Failed to send response to STA");
			return;
		}

		return;
	} else
		wpa_printf(MSG_DEBUG,"UHR Current AP: IAP ST Prep Resp with success");

	/* Extract 802.11 response frame */
	

	wpa_printf(MSG_DEBUG,
		   "UHR Current AP: Forwarding response frame to STA " MACSTR " (len=%u)",
		   MAC2STR(sta->addr), frame_len);

	wpa_hexdump(MSG_MSGDUMP, "UHR Current AP: Response frame",
		    frame, frame_len);

	/* Forward response frame to STA
	 * The frame is a complete 802.11 UHR Link Reconfig Response
	 * Send it directly to the STA
	 */
	if (hostapd_drv_send_mlme(lhapd, frame, frame_len, 0, NULL, 0, 0, 0, 0) < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: Failed to send response to STA");
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR Current AP: Response forwarded successfully");

	u32 role = 1;
	u32 type = 1;
	u32 dl_sn_not_transferred = sta->dl_sn_not_transferred;
	u32 ul_sn_not_transferred = sta->ul_sn_not_transferred;
	u32 dl_drain_time = lhapd->conf->smd.uhr_dl_drain_duration_tu;
	if (hostapd_smd_roam(lhapd, sta, role, type, dl_sn_not_transferred, ul_sn_not_transferred, dl_drain_time)) {
		wpa_printf(MSG_DEBUG, "UHR Current AP: Failed to send WMI roam notification - not skipping for now.");
	}

	ap_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
	ap_info->st_prep_link_id = lhapd->mld_link_id;
	ap_info->st_prep_hapd = lhapd;

	if (uhr_cur_start_st_prep_timer(sta, iap->target_ap_mld_addr, hapd->conf->smd.smd_prep_timeout) < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: Failed to start ST prep timeout");
	}

	uhr_cur_ap_clone_ap_info_to_partners(lhapd, sta, ap_info);
}

int uhr_handle_st_exec_req(struct hostapd_data *hapd,
                                 struct sta_info *sta,
				  const u8 *buf, size_t len,
				 struct sta_smd_ctx_info *smd_ctx)
{
	struct ieee802_11_elems elems;
	struct uhr_reconfig_mle mle;
	const u8 *ies;
	size_t ies_len;
	struct smd_roam_ap_info *target_info;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "UHR ST EXEC: Processing Execute request from " MACSTR,
		   MAC2STR(sta->addr));

	if (len < IEEE80211_HDRLEN + 3) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC: Frame too short");
		return -1;
	}

	ies = buf + IEEE80211_HDRLEN + 3  + 1;
	ies_len = len - IEEE80211_HDRLEN - 3 - 1;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC: Failed to parse elements");
		return -1;
	}

	if (uhr_parse_reconfig_mle(&elems, &mle) < 0) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC: Failed to parse Reconfig ML IE");
		return -1;
	}

	if (!mle.has_target_ap_mld_addr) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC: No Target AP MLD address in ML IE");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR ST EXEC: Target AP " MACSTR,
		   MAC2STR(mle.target_ap_mld_addr));

	/* Find Target AP in ap_list */
	target_info = uhr_find_ap_in_list(sta, mle.target_ap_mld_addr);
	if (!target_info) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC: Target AP " MACSTR " not in list",
			   MAC2STR(mle.target_ap_mld_addr));
		return -1;
	}

	if (target_info->state != SMD_AP_STATE_ST_PREP_COMPLETE) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC: Invalid state %d (expected ST_PREP_COMPLETE)",
			   target_info->state);
		return -1;
	}

	target_info->state = SMD_AP_STATE_ST_EXEC_STARTED;
	os_memcpy(target_info->sta_addr, sta->addr, ETH_ALEN);

	if (smd_ctx) {
		uhr_smd_ctx_dump(smd_ctx, "UHR ST Exec");
		if (smd_ctx->st_type != 1) {
			wpa_printf(MSG_ERROR,
				   "UHR ST EXEC: ST Type invalid, "
				   "expected 1 (Exec) but got %u",
				   smd_ctx->st_type);
			return -1;
		} else {
			target_info->smd_ctx_valid = 1;
			target_info->smd_ctx = smd_ctx;
		}
	}

	wpa_printf(MSG_INFO,
		   "UHR ST EXEC: Target AP " MACSTR " state: ST_PREP_COMPLETE → ST_EXEC_STARTED",
		   MAC2STR(mle.target_ap_mld_addr));

	u32 role = 1; // TODO: Add support for target AP based execution eventually
	u32 type = 2;
	u32 dl_sn_not_transferred = sta->dl_sn_not_transferred;
	u32 ul_sn_not_transferred = sta->ul_sn_not_transferred;
	u32 dl_drain_time = hapd->conf->smd.uhr_dl_drain_duration_tu;
	if (hostapd_smd_roam(hapd, sta, role, type, dl_sn_not_transferred, ul_sn_not_transferred, dl_drain_time)) {
		wpa_printf(MSG_DEBUG, "UHR Current AP: Failed to send WMI roam notification - not skipping for now.");
	}


	ret = uhr_iap_send_st_exec_req(hapd, sta, mle.target_ap_mld_addr, buf, len);

	if (ret < 0) {
		target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC: IAP send failed, state: ST_EXEC_STARTED → ST_PREP_COMPLETE");
		return ret;
	}

	target_info->state = SMD_AP_STATE_ST_EXEC_IAP_PENDING;
	wpa_printf(MSG_INFO,
		   "UHR ST EXEC: Target AP " MACSTR " state: ST_EXEC_STARTED → ST_EXEC_IAP_PENDING",
		   MAC2STR(mle.target_ap_mld_addr));

	/* Timer is a watchdog only; failure is non-fatal. The IAP exchange
	 * proceeds but without automatic cleanup if no response arrives. */
	if (uhr_cur_start_iap_msg_timer(sta, mle.target_ap_mld_addr) < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR Current AP: Failed to start ST exec IAP timeout");
	}
	return ret;
}

/**
 * uhr_dl_drain_timeout - Handle DL Drain timeout
 * @eloop_ctx: hostapd data
 * @timeout_ctx: Target AP info
 *
 * Phase 9: Handle DL Drain timeout expiration.
 * Completes the transition by removing the Target AP from the list.
 */
static void uhr_dl_drain_timeout(void *eloop_ctx, void *timeout_ctx)
{
       struct hostapd_data *hapd = eloop_ctx;
       struct smd_roam_ap_info *target_info = timeout_ctx;
       struct sta_info *sta;

       wpa_printf(MSG_DEBUG,
                  "UHR DL DRAIN: Timeout expired for AP " MACSTR,
                  MAC2STR(target_info->ap_mld_addr));

       /* Find station */
       sta = ap_get_sta(hapd, target_info->sta_addr);
       if (!sta) {
               wpa_printf(MSG_ERROR, "UHR DL DRAIN: Station not found");
               return;
       }

       /* Verify state is DL_DRAIN_ACTIVE */
       if (target_info->state != SMD_AP_STATE_DL_DRAIN_ACTIVE) {
               wpa_printf(MSG_ERROR,
                          "UHR DL DRAIN: Invalid state %d (expected DL_DRAIN_ACTIVE)",
                          target_info->state);
       }

       /* STATE TRANSITION: DL_DRAIN_ACTIVE → TRANSITION_COMPLETE */
       target_info->state = SMD_AP_STATE_TRANSITION_COMPLETE;

       wpa_printf(MSG_INFO,
                  "UHR DL DRAIN: Target AP " MACSTR " state: DL_DRAIN_ACTIVE → TRANSITION_COMPLETE",
                  MAC2STR(target_info->ap_mld_addr));

       /* Remove Target AP from list — frees target_info */
       uhr_remove_ap_from_list(sta, target_info->ap_mld_addr);

       /* Transition COMPLETE - ap_list should now be empty */
       wpa_printf(MSG_INFO,
                  "UHR DL DRAIN: Station " MACSTR " fully transitioned to Target AP, deleting ML station", MAC2STR(sta->addr));
	/* Delete ML station from all serving AP links */
       ap_sta_remove_link_sta(hapd, sta, 0, false);
       ap_free_sta(hapd, sta);
}

/*
 * uhr_cur_ap_cancel_st_prep_for_entry - Cancel the ST prep timer for one ap_list entry
 *
 * Resolves the station that owns the timer via st_prep_link_id (which may be a
 * different link from the one that is processing the ST Execute) and cancels the
 * pending ST prep timeout on that station.
 */
static void uhr_cur_ap_cancel_st_prep_for_entry(struct hostapd_data *lhapd,
						struct sta_info *exec_sta,
						struct smd_roam_ap_info *ap_info)
{
	struct hostapd_data *prep_hapd;
	struct sta_info *prep_sta;

	prep_hapd = hostapd_mld_get_link_bss(lhapd, ap_info->st_prep_link_id);
	prep_sta = (prep_hapd == lhapd) ? exec_sta :
		   (prep_hapd ? ap_get_sta(prep_hapd, exec_sta->addr) : NULL);

	if (prep_sta)
		uhr_cancel_st_prep_timeout(prep_sta, ap_info->ap_mld_addr);
}


/*
 * uhr_cur_ap_purge_ap_list - Cancel all ST prep timers and clear the ap_list
 *
 * On a successful ST Execute, all non-active roaming candidates are removed.
 * Entries in DL_DRAIN_ACTIVE state are intentionally skipped: the DL drain
 * timer holds a live pointer to that ap_info and will remove it on expiry.
 * Partner link clone entries (always in ST_PREP_COMPLETE) are fully cleared.
 */
static void uhr_cur_ap_purge_ap_list(struct hostapd_data *lhapd,
				     struct sta_info *sta)
{
	struct smd_roam_ap_info *ap_info, *next;
	struct hostapd_data *partner_hapd;
	struct sta_info *partner_sta;

	ap_info = sta->smd_info.ap_list;
	while (ap_info) {
		next = ap_info->next;
		if (ap_info->state == SMD_AP_STATE_DL_DRAIN_ACTIVE) {
			ap_info = next;
			continue;
		}
		wpa_printf(MSG_DEBUG,
			   "UHR ST EXEC: Clearing AP " MACSTR " (state=%d) from ap_list",
			   MAC2STR(ap_info->ap_mld_addr), ap_info->state);
		if (uhr_iap_send_st_roam_cleanup(lhapd, ap_info->ap_mld_addr,
						  sta->addr) < 0)
			wpa_printf(MSG_DEBUG,
				   "UHR ST EXEC: Failed to send ROAM CLEANUP to "
				   MACSTR ", TAP will self-clean via timer",
				   MAC2STR(ap_info->ap_mld_addr));
		uhr_cur_ap_cancel_st_prep_for_entry(lhapd, sta, ap_info);
		uhr_remove_ap_from_list(sta, ap_info->ap_mld_addr);
		ap_info = next;
	}

	for_each_mld_link(partner_hapd, lhapd) {
		if (partner_hapd == lhapd)
			continue;
		partner_sta = ap_get_sta(partner_hapd, sta->addr);
		if (partner_sta)
			uhr_cleanup_sta_roam_contexts(partner_hapd, partner_sta);
	}

}

/**
 * uhr_handle_get_smd_ctx_done - Resume Via-TAP ST Exec after async ctx fetch
 *
 * Completion notification for the context request. Send an IAP response to the
 * TAP to resume roaming.
 */
void uhr_handle_get_smd_ctx_done(struct hostapd_data *hapd,
				 const u8 *sta_addr,
				 struct sta_smd_ctx_info *ctx)
{
	struct sta_info *sta;
	struct smd_get_ctx_pending *pending;
	struct smd_roam_ap_info *target_info;
	u8 status = UHR_IAP_STATUS_FAILURE;
	int ret;

	wpa_printf(MSG_INFO,
		   "UHR GET_SMD_CTX_DONE: ctx received for " MACSTR,
		   MAC2STR(sta_addr));
	sta = ap_get_sta(hapd, sta_addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG,
			   "UHR GET_SMD_CTX_DONE: STA " MACSTR " not found",
			   MAC2STR(sta_addr));
		os_free(ctx);
		return;
	}

	if (!sta->smd_info.get_ctx_pending.active) {
		wpa_printf(MSG_DEBUG,
			   "UHR GET_SMD_CTX_DONE: No pending request for " MACSTR,
			   MAC2STR(sta_addr));
		os_free(ctx);
		return;
	}

	eloop_cancel_timeout(uhr_cur_get_ctx_timeout, hapd, sta);
	pending = &sta->smd_info.get_ctx_pending;
	pending->active = false;

	if (ctx)
		uhr_smd_ctx_dump(ctx, "UHR GET_SMD_CTX_DONE");

	target_info = uhr_find_ap_in_list(sta, pending->target_ap_mld_addr);
	if (!target_info) {
		wpa_printf(MSG_ERROR,
			   "UHR GET_SMD_CTX_DONE: target AP " MACSTR " gone for " MACSTR,
			   MAC2STR(pending->target_ap_mld_addr),
			   MAC2STR(sta_addr));
		os_free(ctx);
		return;
	}

	if (target_info->state != SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_WAIT) {
		wpa_printf(MSG_ERROR,
			   "UHR GET_SMD_CTX_DONE: Invalid state %u", target_info->state);
		os_free(ctx);
		return;
	}

	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "UHR GET_SMD_CTX_DONE: ctx fetch failed for " MACSTR
			   " — rolling back to ST_PREP_COMPLETE",
			   MAC2STR(sta_addr));
		target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
	} else {
		status = UHR_IAP_STATUS_SUCCESS;
	}

	ret = uhr_iap_send_st_ctx_response(hapd,
					   pending->target_ap_mld_addr,
					   sta->addr,
					   pending->iap_transaction_id,
					   status,
					   ctx);
	os_free(ctx);

	if (ret) {
		wpa_printf(MSG_ERROR,
			   "UHR GET_SMD_CTX_DONE: IAP send failed, to ST_PREP_COMPLETE");
		target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
		return;
	}

	target_info->state = SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_RESP;
	wpa_printf(MSG_DEBUG,
		   "UHR GET_SMD_CTX_DONE: ST_EXEC_VIA_TGT_CURR_CTX_WAIT -> ST_EXEC_VIA_TGT_CURR_CTX_RESP");
}

void uhr_cur_ap_handle_st_exec_resp(struct hostapd_data *hapd,
                                    const struct uhr_iap_frame *iap,
                                    u16 frame_len)
{
       struct sta_info *sta;
       struct hostapd_data *lhapd;
       struct smd_roam_ap_info *target_info;
       const u8 *frame_buf;
       int ret;
       u32 dl_drain_duration_sec;
       u32 dl_drain_duration_usec;
	u32 role = 1;
	u32 type = 3;
	u32 dl_sn_not_transferred = 0;
	u32 ul_sn_not_transferred = 0;
	u32 dl_drain_time = 0;

       wpa_printf(MSG_DEBUG,
                  "UHR ST EXEC: Received IAP RESPONSE (txn=%u, status=%u, frame_len=%u)",
                  iap->iap_transaction_id, iap->status_code, frame_len);
       lhapd = hostapd_mld_get_link_bss(hapd, iap->current_link_id);
       if (!lhapd)
	       return;

       /* Find station */
       sta = ap_get_sta(lhapd, iap->sta_addr);
       if (!sta) {
               wpa_printf(MSG_ERROR, "UHR ST EXEC: Station not found");
               return;
       }

       /* Find Target AP */
       target_info = uhr_find_ap_in_list(sta, iap->target_ap_mld_addr);
       if (!target_info) {
               wpa_printf(MSG_ERROR, "UHR ST EXEC: Target AP not found");
               return;
       }

       uhr_cancel_iap_timeout(sta, iap->target_ap_mld_addr);

       /* Verify state is ST_EXEC_IAP_PENDING */
       if (target_info->state != SMD_AP_STATE_ST_EXEC_IAP_PENDING) {
               wpa_printf(MSG_ERROR,
                          "UHR ST EXEC: Invalid state %d (expected ST_EXEC_IAP_PENDING)",
                          target_info->state);
	       return;
       }

       /* Check IAP status */
       if (iap->status_code != 0) {
               wpa_printf(MSG_ERROR,
                          "UHR ST EXEC: IAP response failed, status=%u",
                          iap->status_code);

               /* Revert to ST_PREP_COMPLETE (can retry) */
               target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
               return;
       }

       /* Extract DL Drain Duration from frame_buf (last 4 bytes) */
	frame_buf = iap->frame_ctx_data;
       if (frame_len >= 4) {
               target_info->dl_drain_duration_tu = WPA_GET_LE32(frame_buf + frame_len - 4);

               wpa_printf(MSG_DEBUG,
                          "UHR ST EXEC: Extracted DL Drain Duration: %u TU",
                          target_info->dl_drain_duration_tu);
       }

	if (frame_len < offsetof(struct ieee80211_mgmt, bssid) + ETH_ALEN) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC: Frame too short for MAC header (%u < %zu)",
			   frame_len,
			   offsetof(struct ieee80211_mgmt, bssid) + ETH_ALEN);
		target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
		return;
	}
	struct ieee80211_mgmt *mgmt_hdr = (struct ieee80211_mgmt *)frame_buf;
	os_memcpy(mgmt_hdr->sa, lhapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(mgmt_hdr->bssid, lhapd->mld->mld_addr, ETH_ALEN);
	wpa_hexdump(MSG_MSGDUMP, "UHR Current AP: Response frame",
		    frame_buf, frame_len);

       /* Forward complete OTA response to station */
       ret = hostapd_drv_send_mlme(lhapd, frame_buf, frame_len, 0, NULL, 0, 0, 0, 0);
       if (ret < 0) {
               wpa_printf(MSG_ERROR, "UHR ST EXEC: Failed to send OTA response");
               target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
               return;
       }

	dl_sn_not_transferred = sta->dl_sn_not_transferred;
	ul_sn_not_transferred = sta->ul_sn_not_transferred;
	dl_drain_time = hapd->conf->smd.uhr_dl_drain_duration_tu;
	if (hostapd_smd_roam(lhapd, sta, role, type, dl_sn_not_transferred, ul_sn_not_transferred, dl_drain_time)) {
		wpa_printf(MSG_DEBUG, "UHR Current AP: Failed to send WMI roam notification - not skipping for now.");
	}

	/* Cancel the ST prep timer before it fires — exec succeeded. */
	uhr_cancel_st_prep_timeout(sta, iap->target_ap_mld_addr);

	target_info->state = SMD_AP_STATE_DL_DRAIN_ACTIVE;
       wpa_printf(MSG_INFO,
                  "UHR ST EXEC: Target AP " MACSTR " state: ST_EXEC_IAP_PENDING → DL_DRAIN_ACTIVE",
                  MAC2STR(iap->target_ap_mld_addr));
       os_get_reltime(&target_info->dl_drain_start);

	/* Convert TU (1 TU = 1024 us) to seconds + microseconds */
	dl_drain_duration_sec = (u32)((u64)dl_drain_time * 1024 / 1000000);
	dl_drain_duration_usec = (u32)((u64)dl_drain_time * 1024 % 1000000);
	/* coverity[overflow]: u64 intermediate prevents u32 wrap */
	if (dl_drain_duration_sec == 0 && dl_drain_duration_usec == 0)
		dl_drain_duration_sec = 1;
	wpa_printf(MSG_INFO, "UHR ST EXEC: DL Drain started = %u TU (%u.%06u sec)",
		   dl_drain_time, dl_drain_duration_sec, dl_drain_duration_usec);
	eloop_register_timeout(dl_drain_duration_sec, dl_drain_duration_usec, uhr_dl_drain_timeout, lhapd, target_info);
       /* Exec succeeded: cancel all ST prep timers and clear the full ap_list. */
       uhr_cur_ap_purge_ap_list(lhapd, sta);
       wpa_printf(MSG_INFO, "UHR ST EXEC: Waiting for TX STATUS with ACK=1...");
}




/* PMKID length constant */
#ifndef PMKID_LEN
#define PMKID_LEN 16
#endif


/**
 * struct uhr_link_reconf_req_list - Container for all Per-STA Profiles
 * 
 * Following EHT naming: link_reconf_req_list → uhr_link_reconf_req_list
 * This matches EHT's pattern for managing link reconfiguration requests
 */
struct uhr_link_reconf_req_list {
	u8 sta_mld_addr[ETH_ALEN];
	u8 dialog_token;
	u16 links_ok;            /* Bitmap of successfully processed links */
	u16 new_valid_links;     /* Bitmap of all valid links after reconfig */
	
	struct dl_list list;     /* List of uhr_link_reconf_req_info */
};


/**
 * struct uhr_link_reconf_req_info - Individual Per-STA Profile info
 * 
 * Following EHT naming: link_reconf_req_info → uhr_link_reconf_req_info
 * Each entry represents one link to be added to the target AP-MLD
 */
struct uhr_link_reconf_req_info {
	struct dl_list list;
	u8 link_id;
	u16 status;
	bool is_assoc_link;      /* First valid profile = assoc link */
	u8 peer_addr[ETH_ALEN];
	u8 local_addr[ETH_ALEN];
	u8 *sta_prof;            /* STA Profile IEs */
	size_t sta_prof_len;
	u16 capability;
};


/**
 * uhr_deinit_link_reconf_req - Cleanup request list
 * 
 * Pattern from: ml_deinit_link_reconf_req()
 * Following EHT naming convention
 */
void uhr_deinit_link_reconf_req(struct uhr_link_reconf_req_list **req_list_ptr)
{
	struct uhr_link_reconf_req_list *req_list;
	struct uhr_link_reconf_req_info *info, *tmp;
	
	if (!req_list_ptr || !*req_list_ptr)
		return;
	
	req_list = *req_list_ptr;
	
	/* Free each profile (EHT pattern) */
	dl_list_for_each_safe(info, tmp, &req_list->list,
			      struct uhr_link_reconf_req_info, list) {
		dl_list_del(&info->list);
		os_free(info);
	}
	
	os_free(req_list);
	*req_list_ptr = NULL;
}


/**
 * uhr_mark_smd_features - Mark SMD features in sta_info
 * @hapd: BSS data
 * @sta: Station info
 *
 * Populate sta->smd_info with TARGET AP's SMD capabilities.
 */
static void uhr_mark_smd_features(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  const u8 *current_ap_mld_addr)
{
	if (!hapd->conf->smd.enabled || !sta)
		return;

	/* Check if driver supports SMD */
	if (!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SMD)) {
		wpa_printf(MSG_DEBUG,
			   "SMD ST PREP Target AP: Driver does not support SMD");
		return;
	}

	/* Mark sta as SMD-capable */
	sta->smd_info.smd_sta = true;

	/* Copy TARGET AP's SMD identifier */
	os_memcpy(sta->smd_info.smd_identifier,
		  hapd->conf->smd.smd_identifier, ETH_ALEN);

	/* Copy TARGET AP's SMD capabilities */
	sta->smd_info.caps.dl_data_fwd = hapd->conf->smd.caps.dl_data_fwd;
	sta->smd_info.caps.max_prep_target_apmlds =
		hapd->conf->smd.caps.max_prep_target_apmlds;
	sta->smd_info.caps.smd_type = hapd->conf->smd.caps.smd_type;
	sta->smd_info.caps.ptk_mode = hapd->conf->smd.caps.ptk_mode;

	/* Copy TARGET AP's SMD timeout */
	sta->smd_info.smd_timeout = hapd->conf->smd.smd_prep_timeout;

	wpa_printf(MSG_INFO,
		   "SMD ST PREP Target AP: Marked SMD features for " MACSTR,
		   MAC2STR(sta->addr));

	if (current_ap_mld_addr)
		os_memcpy(sta->smd_info.current_ap_mld_addr,
			  current_ap_mld_addr, ETH_ALEN);
}



/**
 * uhr_target_ap_install_security_context - Install security context (Phase 4)
 * @hapd: hostapd data
 * @sta_addr: Station MAC address
 * @sec_ctx: Security context from IAP
 * Returns: 0 on success, -1 on error
 *
 * Phase 4: Install security context to wpa_sm AFTER all STA entries are created.
 */
static int uhr_target_ap_install_security_context(struct hostapd_data *hapd,
						  const u8 *sta_addr,
						  const struct uhr_iap_security_ctx *sec_ctx)
{
	struct sta_info *sta;
	struct wpa_state_machine *sm;
	
	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: Installing security context for STA " MACSTR,
		   MAC2STR(sta_addr));
	
	/* Find STA entry */
	sta = ap_get_sta(hapd, sta_addr);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: STA entry not found for security context");
		return -1;
	}
	
	/* Get or create WPA state machine */
	sm = sta->wpa_sm;
	if (!sm) {
		sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr, NULL);
		if (!sm) {
			wpa_printf(MSG_ERROR,
				   "SMD ST PREP Target AP: Failed to create WPA SM");
			return -1;
		}
		sta->wpa_sm = sm;
	}
	
	/* Install PMK */
	if (sec_ctx->pmk_len > 0 && sec_ctx->pmk_len <= PMK_LEN_MAX) {
		os_memcpy(sm->PMK, sec_ctx->pmk, sec_ctx->pmk_len);
		sm->pmk_len = sec_ctx->pmk_len;
		wpa_printf(MSG_DEBUG,
			   "SMD ST PREP Target AP: Installed PMK (len=%u)",
			   sec_ctx->pmk_len);
	}
	
	/* Install PMKID */
	os_memcpy(sm->pmkid, sec_ctx->pmkid, PMKID_LEN);
	sm->pmkid_set = 1;
	
	/* Install PTK components */
	if (sec_ctx->kck_len > 0 && sec_ctx->kek_len > 0 && sec_ctx->tk_len > 0) {
		if (sec_ctx->kck_len <= WPA_KCK_MAX_LEN) {
			os_memcpy(sm->PTK.kck, sec_ctx->kck, sec_ctx->kck_len);
			sm->PTK.kck_len = sec_ctx->kck_len;
		}
		if (sec_ctx->kek_len <= WPA_KEK_MAX_LEN) {
			os_memcpy(sm->PTK.kek, sec_ctx->kek, sec_ctx->kek_len);
			sm->PTK.kek_len = sec_ctx->kek_len;
		}
		if (sec_ctx->tk_len <= WPA_TK_MAX_LEN) {
			os_memcpy(sm->PTK.tk, sec_ctx->tk, sec_ctx->tk_len);
			sm->PTK.tk_len = sec_ctx->tk_len;
		}
		sm->PTK_valid = 1;
		wpa_printf(MSG_DEBUG,
			   "SMD ST PREP Target AP: Installed PTK components");
	}
	
	/* Install cipher suite information */
	sm->wpa_key_mgmt = WPA_GET_BE32(sec_ctx->akm);
	sm->pairwise = WPA_GET_BE32(sec_ctx->cipher);

	if (sec_ctx->wpa_ie_len > 0) {
		os_memcpy(sm->wpa_ie, sec_ctx->wpa_ie, sec_ctx->wpa_ie_len);
		sm->wpa_ie_len = sec_ctx->wpa_ie_len;
	}

	if (sec_ctx->rsnxe_len > 0) {
		os_memcpy(sm->rsnxe, sec_ctx->rsnxe, sec_ctx->rsnxe_len);
		sm->rsnxe_len = sec_ctx->rsnxe_len;
	}
	
	/*
	 * Populate PMKSA cache using the PMKID transferred from the serving AP.
	 *
	 * The STA's own PMKSA cache still holds this PMKID from the original
	 * authentication — it will present it on reconnect to this AP too.
	 * wpa_auth_pmksa_add2 stores it verbatim (no re-derivation) and handles
	 * both SAE and non-SAE AKMs with MLD awareness.
	 */
	{
		u32 akmp = WPA_GET_BE32(sec_ctx->akm);

		wpa_auth_pmksa_add2(hapd->wpa_auth, sta_addr,
				    sec_ctx->pmk, sec_ctx->pmk_len,
				    sec_ctx->pmkid,
				    0, akmp, NULL,
				    ap_sta_is_mld(hapd, sta));
	}

	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: Security context installation complete (pmk_len=%u, kck_len=%u, kek_len=%u, tk_len=%u)",
		   sec_ctx->pmk_len, sec_ctx->kck_len,
		   sec_ctx->kek_len, sec_ctx->tk_len);
	return 0;
}


/**
 * uhr_target_ap_install_ptk_to_driver - Install PTK to driver (Phase 5)
 * @hapd: hostapd data
 * @sta_addr: Station MAC address
 * @req_list: Request list with all profiles
 * @sec_ctx: Security context from IAP
 * Returns: 0 on success, -1 on error
 *
 * **Phase 5: Install PTK to driver at MLD level via assoc link**
 * 
 * CORRECTED in v24.3: Install PTK ONCE at MLD level via assoc link.
 * In normal association, PTK is installed once on the assoc link and
 * shared across all links in the MLD. We follow the same pattern here.
 * 
 * This is essential for actual data communication after roaming.
 * Without this, the driver won't have the keys to encrypt/decrypt frames.
 */
static int uhr_target_ap_install_ptk_to_driver(
	struct hostapd_data *hapd,
	const u8 *sta_addr,
	struct uhr_link_reconf_req_list *req_list,
	const struct uhr_iap_security_ctx *sec_ctx)
{
	struct uhr_link_reconf_req_info *info;
	struct hostapd_data *hapd_assoc = NULL;
	struct sta_info *sta;
	enum wpa_alg alg;
	int key_idx = 0;
	int set_tx = 1;
	u8 *key_rsc = NULL;
	size_t key_rsc_len = 0;
	u8 assoc_link_id = 0;
	int ret;
	
	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: Installing PTK to driver at MLD level");
	
	/* Determine cipher algorithm from security context */
	switch (WPA_GET_BE32(sec_ctx->cipher)) {
	case WPA_CIPHER_CCMP:
		alg = WPA_ALG_CCMP;
		break;
	case WPA_CIPHER_GCMP:
		alg = WPA_ALG_GCMP;
		break;
	case WPA_CIPHER_CCMP_256:
		alg = WPA_ALG_CCMP_256;
		break;
	case WPA_CIPHER_GCMP_256:
		alg = WPA_ALG_GCMP_256;
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Unsupported cipher 0x%x",
			   WPA_GET_BE32(sec_ctx->cipher));
		return -1;
	}
	
	/* Find the assoc link (first valid profile) */
	dl_list_for_each(info, &req_list->list,
			 struct uhr_link_reconf_req_info, list) {
		if (info->is_assoc_link && info->status == WLAN_STATUS_SUCCESS) {
			hapd_assoc = hostapd_mld_get_link_bss(hapd, info->link_id);
			assoc_link_id = info->link_id;
			break;
		}
	}
	
	if (!hapd_assoc) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: No assoc link found for PTK installation");
		return -1;
	}
	
	/* Find STA entry on assoc link */
	sta = ap_get_sta(hapd_assoc, sta_addr);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: No STA on assoc link for PTK installation");
		return -1;
	}
	
	/* **CORRECT: Install PTK ONCE via assoc link at MLD level** */
	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: Installing PTK via assoc link %u (alg=%d, len=%u)",
		   assoc_link_id, alg, sec_ctx->tk_len);
	
	ret = hostapd_drv_set_key(hapd_assoc->conf->iface, hapd_assoc,
				  alg, sta->addr, key_idx, 0, set_tx,
				  key_rsc, key_rsc_len,
				  sec_ctx->tk, sec_ctx->tk_len,
				  KEY_FLAG_PAIRWISE_RX_TX);
	
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Failed to install PTK to driver");
		return -1;
	}
	
	/* Mark PTK as installed */
	if (sta->wpa_sm)
		sta->wpa_sm->PTK.installed = 1;
	
	wpa_printf(MSG_INFO,
		   "SMD ST PREP Target AP: PTK installed to driver at MLD level via assoc link - " MACSTR,
		   MAC2STR(sta->addr));
	
	return 0;
}


static void uhr_tgt_st_prep_finalize(struct hostapd_data *hapd,
                                     struct sta_info *sta)
{

    sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;

    if (!hapd->conf->ieee802_1x && !hapd->conf->wpa) {
        ap_sta_set_authorized(hapd, sta, 1);
    }

    mlme_associate_indication(hapd, sta);

    ap_sta_set_sa_query_timeout(hapd, sta, 0);

    hostapd_set_sta_flags(hapd, sta);

    wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);

    hapd->new_assoc_sta_cb(hapd, sta, 0);

    wpa_printf(MSG_DEBUG,
               "SMD ST Prep: Finalized station " MACSTR " on link %u",
               MAC2STR(sta->addr), hapd->mld_link_id);
}

static void uhr_tgt_st_prep_finalize_ml(struct hostapd_data *assoc_hapd,
                                          struct sta_info *assoc_sta)
{
	struct hostapd_data *link_hapd;
	struct sta_info *link_sta;
	int link_id;

	assoc_sta->flags |= WLAN_STA_ASSOC;
	assoc_sta->wpa_sm->smd_info.flag = true;
	uhr_tgt_st_prep_finalize(assoc_hapd, assoc_sta);

	/* Finalize partner link stations */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!assoc_sta->mld_info.links[link_id].valid ||
			link_id == assoc_sta->mld_assoc_link_id)
				continue;

			link_hapd = 
				hostapd_mld_get_link_bss(assoc_hapd, link_id);

			if (!link_hapd)
				 continue;

			link_sta = ap_get_sta(link_hapd, assoc_sta->addr);
			if (!link_sta)
				continue;

			uhr_tgt_st_prep_finalize(link_hapd, link_sta);
	}
}




static struct uhr_link_reconf_req_info *
uhr_alloc_and_fill_reconf_info(struct hostapd_data *hapd,
                               const struct uhr_iap_security_ctx *sec_ctx,
                               struct uhr_link_reconf_req_list *req_list,
                               u8 link_id,
                               const u8 *peer_addr,
                               const struct ieee80211_eht_per_sta_profile *per_sta_prof,
                               size_t subelement_len,
                               const u8 *sta_info,
                               u8 sta_info_len,
                               const u8 *bmlie)
{
        struct uhr_link_reconf_req_info *info;
        size_t extra_ie_len = 0;
        const u8 *ies_start;
        size_t base_ie_len;
        u8 *append_pos;
        struct hostapd_data *link_hapd;

        if (sec_ctx) {
                if (sec_ctx->wpa_ie_len > 0)
                        extra_ie_len += sec_ctx->wpa_ie_len;
                if (sec_ctx->rsnxe_len > 0)
                        extra_ie_len += sec_ctx->rsnxe_len;

                /* bmlie[1] holds IE payload length */
                if (bmlie && bmlie[1] > 0)
                        extra_ie_len += bmlie[1] + 2; /* EID+LEN + payload */
        }

        info = os_zalloc(sizeof(*info) + subelement_len + extra_ie_len);
        if (!info)
                return NULL;

        info->link_id = link_id;
        info->status = WLAN_STATUS_SUCCESS;
        os_memcpy(info->peer_addr, peer_addr, ETH_ALEN);

        link_hapd = hostapd_mld_get_link_bss(hapd, link_id);
        if (link_hapd)
                os_memcpy(info->local_addr, link_hapd->own_addr, ETH_ALEN);

        /*
         * STA Info format (your usage):
         * sta_info points to length byte, then peer MAC follows.
         * ies_start = sta_info + sta_info_len
         */
        ies_start = sta_info + sta_info_len;

        if (subelement_len < sizeof(*per_sta_prof) + sta_info_len + 2)
                goto add_to_list; /* No capability/IEs present */

        info->capability = WPA_GET_LE16(ies_start);
        info->sta_prof = (u8 *)(info + 1);

        base_ie_len = subelement_len - sizeof(*per_sta_prof) - sta_info_len - 2;
        os_memcpy(info->sta_prof, ies_start + 2, base_ie_len);
        info->sta_prof_len = base_ie_len;

        if (sec_ctx && sec_ctx->wpa_ie_len > 0) {
                append_pos = info->sta_prof + info->sta_prof_len;
                os_memcpy(append_pos, sec_ctx->wpa_ie, sec_ctx->wpa_ie_len);
                info->sta_prof_len += sec_ctx->wpa_ie_len;
        }

        if (sec_ctx && sec_ctx->rsnxe_len > 0) {
                append_pos = info->sta_prof + info->sta_prof_len;
                os_memcpy(append_pos, sec_ctx->rsnxe, sec_ctx->rsnxe_len);
                info->sta_prof_len += sec_ctx->rsnxe_len;
        }

        if (bmlie && bmlie[1] > 0) {
                append_pos = info->sta_prof + info->sta_prof_len;
                os_memcpy(append_pos, bmlie, bmlie[1] + 2);
                info->sta_prof_len += bmlie[1] + 2;
        }

add_to_list:
        dl_list_add_tail(&req_list->list, &info->list);
        return info;
}


static int uhr_finalize_assoc_and_keys(
        struct hostapd_data *hapd,
        const u8 *sta_addr,
        const struct uhr_iap_security_ctx *sec_ctx,
        struct uhr_link_reconf_req_list *req_list)
{
        if (uhr_target_ap_install_security_context(hapd, sta_addr, sec_ctx) < 0)
                return -1;

        if (uhr_target_ap_install_ptk_to_driver(hapd, sta_addr,
                                                req_list, sec_ctx) < 0)
                return -1;

        return 0;
}


static int uhr_process_reconf_req_list(
        struct hostapd_data *hapd,
        const u8 *sta_addr,
        const u8 *current_ap_mld_addr,
        struct uhr_link_reconf_req_list *req_list,
        bool *assoc_link_found,
        u8 *assoc_link_id,
        struct hostapd_data **assoc_hapd,
        struct sta_info **assoc_sta)
{
        struct uhr_link_reconf_req_info *info;
        u16 status;

	dl_list_for_each(info, &req_list->list,
                         struct uhr_link_reconf_req_info, list) {
                struct hostapd_data *hapd_link;
                struct sta_info *old_sta;
                struct sta_info *sta = NULL;

                wpa_printf(MSG_DEBUG,
                           "SMD ST Prep Target AP: Processing link ID: %d",
                           info->link_id);

                hapd_link = hostapd_mld_get_link_bss(hapd, info->link_id);
                if (!hapd_link) {
                        info->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
                        continue;
                }

                if (!info->sta_prof || info->sta_prof_len == 0) {
                        info->status = WLAN_STATUS_INVALID_IE;
                        continue;
                }

                wpa_hexdump(MSG_DEBUG,
                            "STA Profile", info->sta_prof, info->sta_prof_len);

                old_sta = ap_get_sta(hapd_link, sta_addr);
                if (old_sta)
                        ap_free_sta(hapd_link, old_sta);
		// Iterate and free all the link peer in all hapds in target-ap

                if (!*assoc_link_found) {
                        sta = ap_sta_add(hapd_link, sta_addr);
                        if (!sta) {
                                info->status =
                                        WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
                                continue;
                        }

                        sta->wpa_sm = wpa_auth_sta_init(hapd_link->wpa_auth,
                                                       sta->addr, NULL);
                        if (!sta->wpa_sm) {
                                ap_free_sta(hapd_link, sta);
                                continue;
                        }

                        ap_sta_set_mld(sta, true);
                        os_memcpy(sta->mld_info.common_info.mld_addr,
                                  sta->addr, ETH_ALEN);
                        os_memcpy(sta->mld_info.links[info->link_id].peer_addr,
                                  info->peer_addr, ETH_ALEN);
                        os_memcpy(sta->mld_info.links[info->link_id].local_addr,
                                  info->local_addr, ETH_ALEN);
                        sta->mld_info.links[info->link_id].valid = true;

                        info->status = check_assoc_ies(
                                hapd_link, sta,
                                info->sta_prof, info->sta_prof_len,
                                LINK_PARSE_UHR_RECONF_ASSOC);

                        if (info->status != WLAN_STATUS_SUCCESS) {
                                ap_free_sta(hapd_link, sta);
                                continue;
                        }

                        sta->capability = info->capability;
                        uhr_mark_smd_features(hapd_link, sta,
					      current_ap_mld_addr);
                        sta->smd_info.roam_sta = true;

                        *assoc_link_found = true;
                        *assoc_link_id = info->link_id;
                        *assoc_hapd = hapd_link;
                        *assoc_sta = sta;
                        info->is_assoc_link = true;

			sta->mld_assoc_link_id = info->link_id;
                        sta->smd_info.state = SMD_STA_ST_PREP_DONE;

                        if (ap_sta_re_add(hapd_link, sta, 0) < 0) {
                                ap_free_sta(hapd_link, sta);
                                info->status =
                                        WLAN_STATUS_UNSPECIFIED_FAILURE;
                                continue;
                        }

                        sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
                        hostapd_set_sta_flags(hapd_link, sta);

                        if (hostapd_get_aid(hapd_link, sta) < 0)
                                return -1;

                        if (sta->wpa_sm && sta->smd_info.smd_sta) {
                                wpa_auth_set_smd_info(sta->wpa_sm, sta);
                        }
                } else {
                        info->is_assoc_link = false;

                        status = hostapd_ml_process_reconf_link(
                                hapd_link, *assoc_sta,
                                info->sta_prof, info->sta_prof_len,
                                info->link_id, info->peer_addr,
                                LINK_PARSE_UHR_RECONF_LINK);

                        if (status != WLAN_STATUS_SUCCESS) {
                                info->status = status;
                                continue;
                        }
                }

                req_list->links_ok |= BIT(info->link_id);
        }

        if (!*assoc_link_found)
                return -1;

        return 0;
}


static int uhr_build_reconf_req_list(struct hostapd_data *hapd,
                                     const u8 *sta_addr,
                                     const struct uhr_iap_security_ctx *sec_ctx,
                                     struct wpabuf *mlbuf,
                                     struct uhr_link_reconf_req_list **req_list_out)
{
        struct uhr_link_reconf_req_list *req_list = NULL;
        const u8 *pos;
	const u8 *end;
        u8 common_info_len;
        struct uhr_link_reconf_req_info *info;
        u8 bmlie[UHR_BMLIE_BUF_LEN];
	u16 ml_control;
	size_t ml_len;
	const struct ieee80211_eht_ml *ml;
	
	ml = wpabuf_head(mlbuf);
	ml_len = wpabuf_len(mlbuf);
	
        if (!hapd || !sta_addr || !mlbuf || !ml || !req_list_out)
                return -1;

	if (ml_len < sizeof(*ml))
		return -1;
	
	ml_control = le_to_host16(ml->ml_control);
	
	if ((ml_control & MULTI_LINK_CONTROL_TYPE_MASK) != MULTI_LINK_CONTROL_TYPE_RECONF)
		return -1;
	
	wpa_hexdump(MSG_DEBUG, "Full ML IE", ml, ml_len);

        *req_list_out = NULL;
        os_memset(bmlie, 0, sizeof(bmlie));

        req_list = os_zalloc(sizeof(*req_list));
        if (!req_list)
                return -1;

        dl_list_init(&req_list->list);
        os_memcpy(req_list->sta_mld_addr, sta_addr, ETH_ALEN);

        /* Skip to Link Info field */
	common_info_len = ml->variable[0];
        pos = ml->variable + common_info_len;
	end = ((const u8 *) ml) + ml_len;

        /* Parse all Per-STA Profiles */
        while (end - pos > 2) {
                u8 subelement_id;
                size_t subelement_len;
                int num_frag_subelems;
                const struct ieee80211_eht_per_sta_profile *per_sta_prof;
                u16 sta_control;
                u8 link_id;
                const u8 *sta_info;
                u8 sta_info_len;

                /* REUSE: EHT subelement defragmentation */
                num_frag_subelems = ieee802_11_defrag_mle_subelem(mlbuf, pos,
                                                                 &subelement_len);
                if (num_frag_subelems < 0)
                        goto fail;

                ml_len -= num_frag_subelems * 2;
                end = ((const u8 *) ml) + ml_len;

                subelement_id = *pos;
                wpa_hexdump(MSG_DEBUG, "Subelement:", pos, subelement_len);

                /* Only process Per-STA Profile subelements */
                if (subelement_id != MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
                        pos += 2 + subelement_len;
                        continue;
                }

                if (subelement_len < sizeof(*per_sta_prof) + 1) {
                        pos += 2 + subelement_len;
                        continue;
                }

                /* Parse Per-STA Profile */
                per_sta_prof = (const struct ieee80211_eht_per_sta_profile *) (pos + 2);
                sta_control = le_to_host16(per_sta_prof->sta_control);

                /* REUSE: EHT macros */
                link_id = sta_control & EHT_PER_STA_RECONF_CTRL_LINK_ID_MSK;

                /* Check MAC address present */
                if (!(sta_control & EHT_PER_STA_RECONF_CTRL_MAC_ADDR)) {
                        pos += 2 + subelement_len;
                        continue;
                }

                /* Get STA Info */
                sta_info = per_sta_prof->variable;
                sta_info_len = *sta_info;
                wpa_hexdump(MSG_DEBUG, "Per-STA Info", sta_info, sta_info_len);

                if (sta_info_len < 1 + ETH_ALEN) {
                        pos += 2 + subelement_len;
                        continue;
                }

                hostapd_uhr_eid_bmlie_from_rmlie(mlbuf, link_id, bmlie);

                info = uhr_alloc_and_fill_reconf_info(
                                hapd, sec_ctx, req_list,
                                link_id,
                                sta_info + 1,
                                per_sta_prof,
                                subelement_len,
                                sta_info,
                                sta_info_len,
                                bmlie);
                if (!info)
                        goto fail;

                pos += 2 + subelement_len;
        }

        *req_list_out = req_list;
        return 0;

fail:
        if (req_list)
                uhr_deinit_link_reconf_req(&req_list);
        *req_list_out = NULL;
        return -1;
}

/**
 * uhr_tgt_ap_parse_ml - Parse Reconfiguration ML-IE
 * @hapd: hostapd data
 * @sta_addr: MLD MAC address
 * @sec_ctx: Security context from IAP
 * @ml_ie: Pointer to ML-IE
 * @ml_ie_len: Length of ML-IE
 * @req_list_out: Output parameter for request list
 * @assoc_hapd_out: Output parameter for association link hostapd instance
 * @assoc_sta_out: Output parameter for association link STA entry
 * Returns: 0 on success, -1 on error
 */
static int uhr_tgt_ap_parse_ml(
	struct hostapd_data *hapd,
	const u8 *sta_addr,
	const u8 *current_ap_mld_addr,
	const struct uhr_iap_security_ctx *sec_ctx,
	const u8 *ml_ie,
	size_t ml_ie_len,
	struct uhr_link_reconf_req_list **req_list_out,
	struct hostapd_data **assoc_hapd_out,
	struct sta_info **assoc_sta_out)
{
	struct wpabuf *mlbuf;
	struct uhr_link_reconf_req_list *req_list = NULL;
	u8 assoc_link_id = 0;
	int ret = -1;
	bool assoc_link_found = false;
	struct hostapd_data *assoc_hapd = NULL;
	struct sta_info *assoc_sta = NULL;
	

	mlbuf = ieee802_11_defrag(ml_ie, ml_ie_len, true);
	if (!mlbuf) 
		return -1;
		
	if (uhr_build_reconf_req_list(hapd, sta_addr, sec_ctx,
				mlbuf, &req_list) < 0)
		goto out;

	if (uhr_process_reconf_req_list(hapd, sta_addr, current_ap_mld_addr,
				req_list, &assoc_link_found,
				&assoc_link_id, &assoc_hapd, &assoc_sta) < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: No valid assoc link found");
		goto out;
	}

	/* Add MLD link station to driver (reassoc=0) */
	if (!assoc_hapd || !assoc_sta) {
		wpa_printf(MSG_ERROR, "UHR: Assoc STA/HAPD not found after ML parse");
		goto out;
	}
	
	assoc_sta->listen_interval = 100;
	if (add_associated_sta(assoc_hapd, assoc_sta, 0) < 0)
		goto out;
	uhr_tgt_st_prep_finalize_ml(assoc_hapd, assoc_sta);

        if (uhr_finalize_assoc_and_keys(assoc_hapd, sta_addr,
                                        sec_ctx, req_list) < 0)
                goto out;

        *req_list_out = req_list;
        req_list = NULL; /* ownership transferred */
        *assoc_hapd_out = assoc_hapd;
        *assoc_sta_out = assoc_sta;
        ret = 0;
	
	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: ML-IE parsing complete");
	
out:
	wpabuf_free(mlbuf);
	if (ret && req_list)
		uhr_deinit_link_reconf_req(&req_list);
	return ret;
}


#define SMD_PRES_AID     BIT(0)
#define SMD_PRES_DL_BA   BIT(1)
#define SMD_PRES_UL_BA   BIT(2)
#define SMD_PRES_SCS     BIT(3)

/*
 * Append SMD BSS Transition IE (ST Preparation Response)
 *  - status: IEEE 802.11 status code (LE)
 *  - aid_present: set true to include AID
 *  - aid: Association ID (LE)
 *  - dl_ba/ul_ba/scs: optional blocks if present (can be NULL/0)
 * Returns: advanced pos
 */
static u8 * hostapd_eid_smd_bss_trans_prep_resp(u8 *pos,
                                                u16 status,
                                                bool aid_present,
                                                u16 aid,
                                                const u8 *dl_ba, u8 dl_ba_len,
                                                const u8 *ul_ba, u8 ul_ba_len,
                                                const u8 *scs,   u8 scs_len)
{
        u8 *len_pos;
        u8 presence = 0;

        /* Element ID (Extended) */
        *pos++ = WLAN_EID_EXTENSION;

        /* Reserve Length (1 byte) — fill later */
        len_pos = pos++;
	*len_pos = 0;

        /* Element ID Extension: SMD BSS Transition Parameters */
        *pos++ = WLAN_EID_EXT_SMD_BSS_TRANS_PARAMS;
	*len_pos += 1;

        /* ---- ST Info ---- */

        /* Presence Bitmap (1) */
        if (aid_present)
                presence |= SMD_PRES_AID;
        if (dl_ba && dl_ba_len)
                presence |= SMD_PRES_DL_BA;
        if (ul_ba && ul_ba_len)
                presence |= SMD_PRES_UL_BA;
        if (scs && scs_len)
                presence |= SMD_PRES_SCS;

        *pos++ = presence;
	*len_pos += 1;

        /* Conditionally present fields (in the order of bits) */
        if (aid_present) {
                WPA_PUT_LE16(pos, aid);
                pos += 2;
		*len_pos += 2;
        }
        if (dl_ba && dl_ba_len) {
                os_memcpy(pos, dl_ba, dl_ba_len);
                pos += dl_ba_len;
		*len_pos += dl_ba_len;
        }
        if (ul_ba && ul_ba_len) {
                os_memcpy(pos, ul_ba, ul_ba_len);
                pos += ul_ba_len;
		*len_pos += ul_ba_len;
        }
        if (scs && scs_len) {
                os_memcpy(pos, scs, scs_len);
                pos += scs_len;
		*len_pos += scs_len;
        }

        return pos;
}

static u8 *uhr_tgt_ap_st_prep_resp(struct hostapd_data *hapd,
			       const u8 *sta_addr,
			       u8 dialog_token,
			       u16 status_code,
			       struct uhr_link_reconf_req_list *req_list,
			       size_t *response_len)
{
	u8 *buf, *pos;
	size_t len;
	struct ieee80211_mgmt *mgmt;
	struct sta_info *sta;
	struct mld_info mld;
	size_t mle_len = 0;
	struct uhr_link_reconf_req_info *info;
	unsigned int status_list_count = 0;

	sta = ap_get_sta(hapd, sta_addr);

	/* Count links for status list */
	if (req_list) {
		dl_list_for_each(info, &req_list->list,
				 struct uhr_link_reconf_req_info, list) {
			status_list_count++;
		}
	}
	
	/* Calculate frame length */
	len = IEEE80211_HDRLEN + 1 + 1 + 1 + 1 + 2 + 1;  /* Header + Category + Action + Token + Type + Status Code + Count */
	
	/* **NEW: Reconfiguration Status List (ALWAYS present)** */
	len += status_list_count * 3;  /* link_id (1B) + status (2B) per link */
	/* **NEW: Conditional content based on links_ok** */
	if (status_code == 0 && sta && req_list && req_list->links_ok > 0) {
		
		/* REUSE: EHT ML-IE building */
		os_memset(&mld, 0, sizeof(mld));
		mld.mld_sta = true;
		
		/* Build mld_info for accepted links */
		dl_list_for_each(info, &req_list->list,
				 struct uhr_link_reconf_req_info, list) {
			if (info->status == WLAN_STATUS_SUCCESS) {
				os_memcpy(mld.links[info->link_id].local_addr, info->local_addr, ETH_ALEN);
				os_memcpy(mld.links[info->link_id].peer_addr, info->peer_addr, ETH_ALEN);


				struct mld_link_info *link = &mld.links[info->link_id];
				struct hostapd_data *lhapd;

				
				lhapd = hostapd_mld_get_link_bss(hapd, info->link_id);
				if (!lhapd)
					continue;
				
				link->valid = true;
				link->status = info->status;
				ieee80211_ml_build_assoc_resp(lhapd, NULL, sta, link);
			}
		}
		
		mle_len = hostapd_eid_eht_ml_len(hapd, &mld, false, true, 0, false,
						  false, false);
		len += mle_len;
	}

	// SMD BSS Transition IE:
	// Length = Element ID (1) + Length (1) + Element ID Extn (1) + ST Info (Variable)
	len += 3;
	// For ST Preparation Response, the ST Info contains the following:
	// 1   byte - Presence Bitmap	
	// 0/2 byte - AID
	// ??? byte - DL BA Info
	// ??? byte - UL BA Info
	// ??? byte - SCS List
	// ------------------------------------------------------------
	// NOTE:
	// For the time being, only the AID is present
	// ------------------------------------------------------------
	len += 1 + 2;

	buf = os_zalloc(len);
	if (!buf)
		return NULL;
	
	mgmt = (struct ieee80211_mgmt *) buf;
	
	/* Fill MAC header */
	mgmt->frame_control = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
					   (WLAN_FC_STYPE_ACTION << 4));
	os_memcpy(mgmt->da, sta_addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	
	/* Fill action frame */
	pos = (u8 *) &mgmt->u.action;
	*pos++ = WLAN_ACTION_PROTECTED_UHR;
	*pos++ = 1;
	*pos++ = dialog_token;
	*pos++ = 0; /* Type = ST Prepration */
        /* Status Code (2, LE) */
        WPA_PUT_LE16(pos, status_code); /* Status Code */;
        pos += 2;
	*pos++ = status_list_count;

	/* **NEW: Build Reconfiguration Status List (ALWAYS)** */
	if (req_list) {
		dl_list_for_each(info, &req_list->list,
				 struct uhr_link_reconf_req_info, list) {
			*pos++ = info->link_id;
			WPA_PUT_LE16(pos, info->status);
			pos += 2;
			wpa_printf(MSG_DEBUG,
				   "SMD ST PREP Target AP: Status list - link %u: status=%u",
				   info->link_id, info->status);
		}
	}
	
	/* **NEW: Conditional content (ONLY if links_ok > 0)** */
	if (status_code != 0 || !sta || !req_list || req_list->links_ok == 0)
		goto done;

	/* REUSE: EHT ML-IE building */
	if (mle_len) {
		pos = hostapd_eid_eht_basic_ml_common(hapd, pos, &mld,
						      false, true, 0, true, false,
						      false, false);
		wpa_printf(MSG_DEBUG,
			   "SMD ST PREP Target AP: Added ML-IE (len=%zu)",
			   mle_len);
	}
	
        bool aid_present = true;
        u16 aid = sta ? sta->aid : 0;

        pos = hostapd_eid_smd_bss_trans_prep_resp(
                pos,
                0,
                aid_present,
                aid,
                NULL, 0,   // DL BA Info not present
                NULL, 0,   // UL BA Info not present
                NULL, 0    // SCS List not present
        );

done:
	if (mld.mld_sta)
		ap_sta_free_sta_profile(&mld);
	
	*response_len = pos - buf;
	
	wpa_printf(MSG_INFO,
		   "SMD ST PREP Target AP: Generated response (len=%zu, status=%u, links_ok=0x%x)",
		   *response_len, status_code,
		   req_list ? req_list->links_ok : 0);
	
	return buf;
}

static int uhr_target_ap_set_smd_ctx(struct hostapd_data *hapd, const u8 *sta_addr,
				     struct sta_smd_ctx_info *smd_ctx)
{
	struct hostapd_data *link = NULL, *assoc_hapd;
	struct sta_info *sta = NULL, *assoc_sta;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "SMD: Target AP: Setting dynamic context for " MACSTR,
		   MAC2STR(sta_addr));

	for_each_mld_link(link, hapd) {
		sta = ap_get_sta(hapd, sta_addr);
		if (sta)
			break;
	}

	if (!sta) {
		wpa_printf(MSG_ERROR, "SMD: No STA to set context for " MACSTR,
			   MAC2STR(sta_addr));
		return -1;
	}

	assoc_sta = hostapd_ml_get_assoc_sta(link, sta, &assoc_hapd);
	if (!assoc_sta) {
		wpa_printf(MSG_ERROR,
			   "SMD: No Assoc STA to set context for " MACSTR,
			   MAC2STR(sta_addr));
		return -1;
	}

	uhr_smd_ctx_dump(smd_ctx, "UHR SET_SMD_CTX");
	ret = hostapd_drv_set_smd_ctx(assoc_hapd, assoc_sta, smd_ctx);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to send SET_SMD_CTX to driver=%d",
			   ret);
		return ret;
	}

	return 0;
}

/* Enable 4addr/WDS immediately in ST_PREP once target-side STA entry exists.
 * This avoids waiting for reactive NL80211_CMD_UNEXPECTED_4ADDR_FRAME flow
 * and ensures firmware sees 4addr programming before ST_EXEC. */
static int uhr_tgt_ap_enable_prep_wds(struct hostapd_data *assoc_hapd,
				      struct sta_info *assoc_sta)
{
	char ifname_wds[IFNAMSIZ + 1];
	int aid;
	int ret;

	if (!assoc_hapd || !assoc_sta)
		return -1;

	if (!assoc_hapd->conf->wds_sta) {
		wpa_printf(MSG_DEBUG,
			   "SMD ST PREP Target AP: WDS disabled in config; skipping prep-time 4addr for " MACSTR,
			   MAC2STR(assoc_sta->addr));
		return 0;
	}

	if (assoc_sta->flags & WLAN_STA_WDS)
		return 0;

	if (assoc_hapd->conf->mld_ap) {
		if (hostapd_get_wds_mld_sta_uid(assoc_hapd, assoc_sta) < 0) {
			wpa_printf(MSG_ERROR,
				   "SMD ST PREP Target AP: No WDS UID for prep-time 4addr STA " MACSTR,
				   MAC2STR(assoc_sta->addr));
			return -1;
		}
		aid = assoc_sta->wds_mld_uid;
	} else {
		aid = assoc_sta->aid;
	}

	assoc_sta->pending_wds_enable = 0;
	assoc_sta->flags |= WLAN_STA_WDS;
	hostapd_set_sta_flag_to_partner_links(assoc_hapd, assoc_sta);

	ret = hostapd_set_wds_sta(assoc_hapd, ifname_wds, assoc_sta->addr, aid, 1);
	if (ret) {
		assoc_sta->pending_wds_enable = 1;
		assoc_sta->flags &= ~WLAN_STA_WDS;
		hostapd_set_sta_flag_to_partner_links(assoc_hapd, assoc_sta);
		return ret;
	}

	return 0;
}

static bool hostapd_mld_find_assoc_sta(struct hostapd_data *rx_hapd,
				const u8 *addr,
				struct hostapd_data **assoc_hapd,
				struct sta_info **assoc_sta)
{
	struct hostapd_data *hapd;

	if (!rx_hapd || !rx_hapd->mld)
		return false;

	for_each_mld_link(hapd, rx_hapd) {
		*assoc_sta = ap_get_sta(hapd, addr);
		if (*assoc_sta && ((*assoc_sta)->flags & WLAN_STA_ASSOC)) {
			*assoc_hapd = hapd;
			return true;
		}
	}

	return false;
}


void uhr_tgt_ap_handle_st_roam_cleanup(struct hostapd_data *hapd,
					const struct uhr_iap_frame *iap)
{
	struct hostapd_data *assoc_hapd = NULL;
	struct sta_info *assoc_sta = NULL;
	struct hostapd_data *bss;

	wpa_printf(MSG_DEBUG,
		   "UHR ROAM CLEANUP: Received for STA " MACSTR " from " MACSTR,
		   MAC2STR(iap->sta_addr), MAC2STR(iap->current_ap_mld_addr));

	if (!hostapd_mld_find_assoc_sta(hapd, iap->sta_addr,
					&assoc_hapd, &assoc_sta)) {
		wpa_printf(MSG_DEBUG,
			   "UHR ROAM CLEANUP: STA " MACSTR " not found, nothing to do",
			   MAC2STR(iap->sta_addr));
		return;
	}

	/*
	 * A cleanup must proceed even when the STA is in ST_EXEC_DONE.
	 * In that state the target AP has already authorized the STA, but if
	 * the IAP response never reached the current AP the STA has not yet
	 * transitioned.  The current AP sends this cleanup to undo that
	 * authorization.  The normal success path never reaches here for the
	 * winning AP because uhr_cur_ap_purge_ap_list() skips DL_DRAIN_ACTIVE
	 * entries.
	 */
	if (assoc_sta->smd_info.state == SMD_STA_ST_EXEC_DONE)
		wpa_printf(MSG_DEBUG,
			   "UHR ROAM CLEANUP: STA " MACSTR " in EXEC_DONE, current AP timed out — undoing authorization",
			   MAC2STR(iap->sta_addr));

	uhr_tgt_cancel_st_prep_timer(assoc_hapd, iap->sta_addr);

	for_each_mld_link(bss, assoc_hapd) {
		struct sta_info *sta;

		sta = ap_get_sta(bss, iap->sta_addr);
		if (!sta)
			continue;

		wpa_printf(MSG_DEBUG,
			   "UHR ROAM CLEANUP: Freeing STA on link %u",
			   bss->mld_link_id);
		ap_free_sta(bss, sta);
	}

	wpa_printf(MSG_INFO, "UHR ROAM CLEANUP: Cleaned up STA " MACSTR,
		   MAC2STR(iap->sta_addr));
}


/* uhr_tgt_ap_handle_st_prep_ctx - Apply deferred prep SMD context on Target AP
 *
 * Handles UHR_IAP_MSG_ST_PREP_CTX from Current AP.  The STA entry must already
 * exist from prior processing of UHR_IAP_MSG_ST_PREP_REQUEST.  The context is
 * passed straight to the driver via uhr_target_ap_set_smd_ctx().
 *
 * Benign no-ops: STA not found (prep failed) or exec already completed.
 */
void uhr_tgt_ap_handle_st_prep_ctx(struct hostapd_data *hapd,
				    const struct uhr_iap_frame *iap)
{
	struct hostapd_data *assoc_hapd = NULL;
	struct sta_info *assoc_sta = NULL;
	struct sta_smd_ctx_info *smd_ctx;
	u16 smd_ctx_len;

	if (!hostapd_mld_find_assoc_sta(hapd, iap->sta_addr,
					&assoc_hapd, &assoc_sta)) {
		wpa_printf(MSG_DEBUG,
			   "UHR ST PREP CTX: STA " MACSTR " not found "
			   "(prep may have failed), dropping ctx",
			   MAC2STR(iap->sta_addr));
		return;
	}

	if (assoc_sta->smd_info.state == SMD_STA_ST_EXEC_DONE) {
		wpa_printf(MSG_DEBUG,
			   "UHR ST PREP CTX: STA " MACSTR " already exec-done, ignoring",
			   MAC2STR(iap->sta_addr));
		return;
	}

	smd_ctx_len = le_to_host16(iap->smd_ctx_len);

	if (smd_ctx_len < sizeof(struct sta_smd_ctx_info)) {
		wpa_printf(MSG_ERROR,
			   "UHR ST PREP CTX: smd_ctx too small (%u < %zu), dropping",
			   smd_ctx_len, sizeof(struct sta_smd_ctx_info));
		return;
	}

	/* frame_len == 0: smd_ctx sits at the start of frame_ctx_data[] */
	smd_ctx = (struct sta_smd_ctx_info *) iap->frame_ctx_data;

	/* Subtract is safe: smd_ctx_len >= sizeof(*smd_ctx) confirmed above */
	if (smd_ctx->vendor_ctx_len > (size_t)(smd_ctx_len - sizeof(*smd_ctx))) {
		wpa_printf(MSG_ERROR,
			   "UHR ST PREP CTX: vendor_ctx overflow, dropping");
		return;
	}

	if (uhr_target_ap_set_smd_ctx(assoc_hapd, iap->sta_addr, smd_ctx)) {
		wpa_printf(MSG_ERROR,
			   "UHR ST PREP CTX: Failed to set context for STA " MACSTR,
			   MAC2STR(iap->sta_addr));
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR ST PREP CTX: Context applied for STA " MACSTR,
		   MAC2STR(iap->sta_addr));
}
void uhr_tgt_ap_handle_st_prep_req(struct hostapd_data *hapd,
			       const struct uhr_iap_frame *iap,
			       u16 frame_len)
{
	const u8 *frame;
	struct ieee802_11_elems elems;
	u8 dialog_token = 0;
	u8 *response_frame = NULL;
	size_t response_len = 0;
	u16 status_code = 0;
	struct uhr_link_reconf_req_list *req_list = NULL;
	struct uhr_smd_bss_transition_element sbte;
	struct sta_info *sta = NULL;
	struct hostapd_data *assoc_hapd = NULL;
	struct sta_info *assoc_sta = NULL;
	int ret;
	u16 smd_ctx_len = 0;
	struct sta_smd_ctx_info *smd_ctx;
	
	if (!hapd || !iap) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Invalid parameters");
		return;
	}
	
	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: Received IAP request (txn=%u)",
		   iap->iap_transaction_id);
	
	/* Validate frame */
	if (frame_len == 0 || !(iap->flags & UHR_IAP_FLAG_HAS_SEC_CTX)) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Invalid IAP request");
		status_code = 1;
		goto send_response;
	}
	
	frame = iap->frame_ctx_data;

	if (frame_len < WLAN_ST_PREP_MIN_LEN) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Frame too short");
		status_code = 1;
		goto send_response;
	}

	if (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) {
		smd_ctx_len = le_to_host16(iap->smd_ctx_len);
		wpa_printf(MSG_DEBUG,
			   "SMD ST PREP Target AP: Dynamic context len=%u",
			   smd_ctx_len);
	}

	dialog_token = frame[26];
	
	/* Parse IEs */
	if (ieee802_11_parse_elems(frame + WLAN_ST_PREP_MIN_LEN,
		frame_len - WLAN_ST_PREP_MIN_LEN, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Failed to parse IEs");
		status_code = 1;
		goto send_response;
	}
	
	/* Check for ML-IE */
	if (!elems.reconf_mle || !elems.reconf_mle_len) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: No ML-IE");
		status_code = 1;
		goto send_response;
	}

       /* Parse SMD BSS Transition IE */
       if (uhr_parse_smd_bss_trans_elem(&elems, 0, &sbte) < 0) {
               wpa_printf(MSG_ERROR, "UHR Target AP: Failed to parse SBTE");
               status_code = 1;
		goto send_response;
       }
	
	/* Parse ML-IE and process profiles (includes MLD-level PTK installation) */
	if (uhr_tgt_ap_parse_ml(hapd, iap->sta_addr, iap->current_ap_mld_addr,
				&iap->sec_ctx,
			        elems.reconf_mle,
			        elems.reconf_mle_len,
			        &req_list,
				&assoc_hapd, &assoc_sta) < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Failed to parse ML-IE");
		status_code = 1;
		goto send_response;
	}

	sta = assoc_sta;
	if (!assoc_sta || !assoc_hapd) {
		wpa_printf(MSG_ERROR, "UHR Target AP: Failed to get assoc STA");
		status_code = 1;
		goto send_response;
       }

       /* Update flags based on the flags */
       sta->dl_sn_not_transferred = sbte.dl_sn_not_transferred;
       sta->ul_sn_not_transferred = sbte.ul_sn_not_transferred;

	/* Serving AP explicitly indicated 4addr intent in IAP metadata. */
	if (iap->flags & UHR_IAP_FLAG_STA_4ADDR) {
		if (uhr_tgt_ap_enable_prep_wds(assoc_hapd, assoc_sta)) {
			wpa_printf(MSG_ERROR,
				   "SMD ST PREP Target AP: prep-time WDS enable failed for " MACSTR,
				   MAC2STR(sta->addr));
			status_code = 1;
			goto send_response;
		}
	}

	if (smd_ctx_len) {
		if (smd_ctx_len < sizeof(struct sta_smd_ctx_info)) {
			wpa_printf(MSG_ERROR,
				   "SMD ST PREP Target AP: smd_ctx too small (%u < %zu), dropping",
				   smd_ctx_len, sizeof(struct sta_smd_ctx_info));
			status_code = 1;
			goto send_response;
		}
		smd_ctx = (struct sta_smd_ctx_info *)(iap->frame_ctx_data + frame_len);
		/* Subtract is safe: smd_ctx_len >= sizeof(*smd_ctx) confirmed above */
		if (smd_ctx->vendor_ctx_len > (size_t)(smd_ctx_len - sizeof(*smd_ctx))) {
			wpa_printf(MSG_ERROR,
				   "SMD ST PREP Target AP: vendor_ctx overflow, dropping");
			status_code = 1;
			goto send_response;
		}
		if (uhr_target_ap_set_smd_ctx(assoc_hapd, iap->sta_addr, smd_ctx)) {
			wpa_printf(MSG_ERROR, "SMD ST PREP Target AP: Failed to set ctx");
			status_code = 1;
			goto send_response;
		}
	}

send_response:
	response_frame = uhr_tgt_ap_st_prep_resp(assoc_hapd, iap->sta_addr,
					     dialog_token,
					     status_code,
					     req_list,
					     &response_len);
	
	/* Send IAP RESPONSE */
	ret = uhr_iap_send_st_prep_resp(hapd, iap->current_ap_mld_addr,
				    iap->sta_addr,
				    iap->iap_transaction_id,
				    le_to_host64(iap->sequence_number),
				    status_code == 0 ? UHR_IAP_STATUS_SUCCESS :
						       UHR_IAP_STATUS_FAILURE,
				    iap->current_link_id,
				    response_frame, response_len);

	if (!ret && sta) {
		ap_sta_clear_disconnect_timeouts(assoc_hapd, assoc_sta);
		ap_sta_clear_assoc_timeout(assoc_hapd, assoc_sta);
#ifdef CONFIG_IEEE80211BE
	        if (ap_sta_is_mld(assoc_hapd, assoc_sta)) {
	                struct hostapd_data *bss;
	                struct sta_info *lsta;
	
	                for_each_mld_link(bss, assoc_hapd) {
	                        if (bss == assoc_hapd)
	                                continue;
	                        lsta = ap_get_sta(bss, assoc_sta->addr);
	                        if (lsta)
	                                ap_sta_clear_assoc_timeout(bss, lsta);
	                }
	        }

#ifdef CONFIG_P2P
	        if (sta->p2p_ie == NULL && !sta->no_p2p_set) {
	                sta->no_p2p_set = 1;
	                hapd->num_sta_no_p2p++;
	                if (hapd->num_sta_no_p2p == 1)
	                        hostapd_p2p_non_p2p_sta_connected(hapd);
	        }
#endif /* CONFIG_P2P */

	        //airtime_policy_new_sta(hapd, sta);
#endif /* CONFIG_IEEE80211BE */
	}

	/* Notify firmware of the ST Prep outcome — only when pointers are valid */
	if (sta && assoc_hapd && assoc_sta &&
	    hostapd_smd_roam(assoc_hapd, assoc_sta,
			     2, /* role: Target AP */
			     1, /* type: ST Prep response */
			     sta->dl_sn_not_transferred,
			     sta->ul_sn_not_transferred,
			     assoc_hapd->conf->smd.uhr_dl_drain_duration_tu))
		wpa_printf(MSG_ERROR,
			   "UHR ST PREP Target AP: WMI roam notification failed");

	if (response_frame)
		os_free(response_frame);
	
	if (req_list)
		uhr_deinit_link_reconf_req(&req_list);
	
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD ST PREP Target AP: Failed to send IAP response");
		return;
	}
	
	wpa_printf(MSG_DEBUG,
		   "SMD ST PREP Target AP: IAP response sent (status=%u)",
		   status_code);
	
	if (status_code == 0) {
		/*
		 * Start prep timer only on success, anchored to the assoc link so that
		 * uhr_tgt_cancel_st_prep_timer (which also resolves via
		 * hostapd_mld_find_assoc_sta) cancels the correct eloop entry.
		 */
		wpa_printf(MSG_INFO,
			   "SMD ST PREP Target AP: Ready for STA " MACSTR " with MLD-level PTK and assoc link marking",
			   MAC2STR(iap->sta_addr));

		if (assoc_hapd && assoc_sta)
			uhr_tgt_start_st_prep_timer(assoc_hapd, iap->sta_addr);
	}
	return;
}

static u8 * hostapd_eid_smd_bss_trans_exec_resp(u8 *pos,
					        u16 dl_drain_time)
{
        u8 *len_pos;
        u8 st_control = 0;

        /* Element ID (Extended) */
        *pos++ = WLAN_EID_EXTENSION;

        /* Reserve Length (1 byte) — fill later */
        len_pos = pos++;
	*len_pos = 0;

        /* Element ID Extension: SMD BSS Transition Parameters */
        *pos++ = WLAN_EID_EXT_SMD_BSS_TRANS_PARAMS;
	*len_pos += 1;

        /* ---- ST Info ---- */

        /* Presence Bitmap (1) */
        st_control |= BIT(0); // DL Drain present

        *pos++ = st_control;
	*len_pos += 1;

        /* Conditionally present fields (in the order of bits) */
        WPA_PUT_LE16(pos, dl_drain_time);
        pos += 2;
	*len_pos += 2;

        return pos;
}


/*
 * uhr_tgt_build_st_exec_resp_frame - Build OTA ST Execution Response frame
 *
 * Shared between the IAP-forwarded path (uhr_tgt_ap_handle_st_exec_req) and
 * the via-Target path (uhr_tgt_ap_handle_st_ctx_response).
 *
 * Returns an os_malloc'd buffer of *out_len bytes on success, NULL on failure.
 * Caller must os_free() the returned buffer.
 */
static u8 *uhr_tgt_build_st_exec_resp_frame(struct hostapd_data *lhapd,
					    struct sta_info *sta,
					    u8 dialog_token,
					    u16 status,
					    size_t *out_len)
{
	u8 *resp_buf, *pos, *rcsl_count;
	struct ieee80211_mgmt *mgmt;
	int i, n = 0;
	size_t key_deliv_len;
	size_t len;

	if (!lhapd || !sta || !out_len)
		return NULL;

	*out_len = 0;

	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		if (sta->mld_info.links[i].valid)
			n++;
	}

	key_deliv_len = sta->wpa_sm ?
		wpa_auth_key_delivery_elem_len(sta->wpa_sm, 0xFFFF) : 0;

	len = IEEE80211_HDRLEN +
	      1 +		/* Category */
	      1 +		/* Action */
	      1 +		/* Dialog Token */
	      1 +		/* Type */
	      2 +		/* Status Code */
	      1 +		/* Count */
	      (3 * n) +		/* RCSL: Link ID(1) + Status(2) per link */
	      key_deliv_len +	/* Key Delivery element (9.4.2.184) */
	      6;		/* SMD BSS Transition IE */

	resp_buf = os_zalloc(len);
	if (!resp_buf)
		return NULL;

	mgmt = (struct ieee80211_mgmt *) resp_buf;
	mgmt->frame_control = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
					   (WLAN_FC_STYPE_ACTION << 4));
	os_memcpy(mgmt->da, sta->addr, ETH_ALEN);
	os_memcpy(mgmt->sa, lhapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, lhapd->mld->mld_addr, ETH_ALEN);

	pos = (u8 *) &mgmt->u.action;
	*pos++ = WLAN_ACTION_PROTECTED_UHR;
	*pos++ = 1;
	*pos++ = dialog_token;
	*pos++ = 1; /* Type = ST Execution */
	WPA_PUT_LE16(pos, status);
	pos += 2;

	rcsl_count = pos;
	*rcsl_count = 0;
	pos++;

	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		if (!sta->mld_info.links[i].valid)
			continue;
		*pos++ = (u8) i;
		WPA_PUT_LE16(pos, WLAN_STATUS_SUCCESS);
		pos += 2;
		(*rcsl_count)++;
	}

	if (key_deliv_len && sta->wpa_sm)
		pos = wpa_auth_build_key_delivery_elem(sta->wpa_sm, 0xFFFF, pos);

	pos = hostapd_eid_smd_bss_trans_exec_resp(
		pos, lhapd->conf->smd.uhr_dl_drain_duration_tu);

	*out_len = (size_t)(pos - resp_buf);
	return resp_buf;
}


void uhr_tgt_ap_handle_st_exec_req(struct hostapd_data *hapd,
                                const struct uhr_iap_frame *iap,
                                u16 frame_len)
{
	struct sta_info *sta = NULL;
	struct hostapd_data *lhapd = NULL;
	u8 *resp_buf;
	const u8 *frame;
	int ret;
	u16 smd_ctx_len = 0;
	struct sta_smd_ctx_info *smd_ctx;
	size_t resp_len;

        wpa_printf(MSG_DEBUG,
                  "UHR ST EXEC: Received IAP REQUEST (txn=%u)",
                  iap->iap_transaction_id);


        if (!hostapd_mld_find_assoc_sta(hapd, iap->sta_addr, &lhapd, &sta)) {
	       wpa_printf(MSG_DEBUG, "UHR ST EXEC: STA Fetch failed");
		return;
	}
	

       /* Find station (should already be prepped from ST Prep) */
       if (!sta) {
               wpa_printf(MSG_ERROR, "UHR ST EXEC: Station not found");
               uhr_iap_send_st_exec_resp(lhapd,
                                         iap->current_ap_mld_addr,
                                         iap->sta_addr,
                                         iap->iap_transaction_id,
                                         le_to_host64(iap->sequence_number),
					 WLAN_STATUS_UNSPECIFIED_FAILURE,
                                         iap->current_link_id, NULL, 0);
               return;
       }

	if (ap_sta_is_mld(hapd, sta)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Set ML info in RSN Authenticator");
		wpa_auth_set_ml_info(sta->wpa_sm,
				     sta->mld_assoc_link_id,
				     &sta->mld_info);
	}

	if (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) {
		smd_ctx_len = le_to_host16(iap->smd_ctx_len);
		wpa_printf(MSG_DEBUG,
			   "SMD ST EXEC Target AP: Dynamic context len=%u",
			   smd_ctx_len);
	}

	if (smd_ctx_len) {
		if (smd_ctx_len < sizeof(struct sta_smd_ctx_info)) {
			wpa_printf(MSG_ERROR,
				   "SMD ST EXEC Target AP: smd_ctx too small (%u < %zu), dropping",
				   smd_ctx_len, sizeof(struct sta_smd_ctx_info));
			uhr_iap_send_st_exec_resp(lhapd,
					  iap->current_ap_mld_addr,
					  iap->sta_addr,
					  iap->iap_transaction_id,
					  le_to_host64(iap->sequence_number),
					  WLAN_STATUS_UNSPECIFIED_FAILURE,
					  iap->current_link_id,
					  NULL, 0);
			return;
		}
		/* smd_ctx may already have been applied via UHR_IAP_MSG_ST_PREP_CTX
		 * (type 9); the driver call is idempotent. */
		smd_ctx = (struct sta_smd_ctx_info *)(iap->frame_ctx_data + frame_len);
		/* Subtract is safe: smd_ctx_len >= sizeof(*smd_ctx) confirmed above */
		if (smd_ctx->vendor_ctx_len > (size_t)(smd_ctx_len - sizeof(*smd_ctx))) {
			wpa_printf(MSG_ERROR,
				   "SMD ST EXEC Target AP: vendor_ctx overflow, dropping");
			uhr_iap_send_st_exec_resp(lhapd,
					  iap->current_ap_mld_addr,
					  iap->sta_addr,
					  iap->iap_transaction_id,
					  le_to_host64(iap->sequence_number),
					  WLAN_STATUS_UNSPECIFIED_FAILURE,
					  iap->current_link_id,
					  NULL, 0);
			return;
		}
		if (uhr_target_ap_set_smd_ctx(lhapd, iap->sta_addr, smd_ctx)) {
			wpa_printf(MSG_ERROR, "SMD ST EXEC Target AP: Failed to set ctx");
			uhr_iap_send_st_exec_resp(lhapd,
						  iap->current_ap_mld_addr,
						  iap->sta_addr,
						  iap->iap_transaction_id,
						  le_to_host64(iap->sequence_number),
						  WLAN_STATUS_UNSPECIFIED_FAILURE,
						  iap->current_link_id,
						  NULL, 0);
			return;
		}
	}

	if (ap_sta_set_authorized_flag(lhapd, sta, 1)) {
		sta->flags_ext |= WLAN_STA_SMD;
		hostapd_set_sta_flags(lhapd,sta);
		wpa_printf(MSG_DEBUG, "Authorized the STA");
	} else {
		wpa_printf(MSG_DEBUG, "Could not send authorize to the STA - sending failure (TBD)");
		
	}

	u32 role = 2;
	u32 type = 4;
	u32 dl_sn_not_transferred = sta->dl_sn_not_transferred;
	u32 ul_sn_not_transferred = sta->ul_sn_not_transferred;
	u32 dl_drain_time = hapd->conf->smd.uhr_dl_drain_duration_tu;
	if (hostapd_smd_roam(lhapd, sta, role, type, dl_sn_not_transferred, ul_sn_not_transferred, dl_drain_time)) {
		wpa_printf(MSG_DEBUG, "UHR Current AP: Failed to send WMI roam notification - not skipping for now.");
	}

	frame = iap->frame_ctx_data;
	resp_buf = uhr_tgt_build_st_exec_resp_frame(lhapd, sta, frame[26],
						    WLAN_STATUS_SUCCESS,&resp_len);
	if (!resp_buf) {
		uhr_iap_send_st_exec_resp(lhapd,
					  iap->current_ap_mld_addr,
					  iap->sta_addr,
					  iap->iap_transaction_id,
					  le_to_host64(iap->sequence_number),
					  WLAN_STATUS_UNSPECIFIED_FAILURE,
					  iap->current_link_id,
					  NULL, 0);
		return;
	}

	/* Frame is complete; advance state before sending */
	sta->smd_info.state = SMD_STA_ST_EXEC_DONE;
	sta->smd_info.roam_sta = false;

	ret = uhr_iap_send_st_exec_resp(lhapd,
					iap->current_ap_mld_addr,
					iap->sta_addr,
					iap->iap_transaction_id,
					le_to_host64(iap->sequence_number),
					0,
					iap->current_link_id,
					resp_buf, resp_len);

	os_free(resp_buf);

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC: Failed to send IAP RESPONSE");
		return;
	}

	wpa_printf(MSG_INFO,
		   "UHR ST EXEC: Sent IAP RESPONSE to Current AP " MACSTR " (len=%zu)",
		   MAC2STR(iap->current_ap_mld_addr), resp_len);

	uhr_tgt_cancel_st_prep_timer(lhapd, (u8 *) iap->sta_addr);
      /* TODO: Delete the peer if exec is not done */
}


int uhr_handle_st_exec_req_tgt(struct hostapd_data *hapd, struct sta_info *sta,
			       const u8 *buf, size_t len)
{
	struct ieee802_11_elems elems;
	struct uhr_reconfig_mle mle;
	struct hostapd_data *assoc_hapd = NULL;
	struct smd_roam_ap_info *ap_info;
	const u8 *ies;
	size_t ies_len;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "UHR ST EXEC via Target AP: Processing Execute from " MACSTR,
		   MAC2STR(sta->addr));

	if (!hostapd_mld_find_assoc_sta(hapd, sta->addr, &assoc_hapd, &sta) || !sta) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC (tgt): Assoc STA not found");
		return -1;
	}

	if (len < IEEE80211_HDRLEN + 4) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC (tgt): Frame too short");
		return -1;
	}

	/* Save dialog token for use in the async CTX_RESPONSE path */
	sta->smd_info.st_exec_dialog_token = buf[IEEE80211_HDRLEN + 2];

	ies = buf + IEEE80211_HDRLEN + 4;
	ies_len = len - IEEE80211_HDRLEN - 4;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC (tgt): Failed to parse IEs");
		return -1;
	}

	if (uhr_parse_reconfig_mle(&elems, &mle) < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC (tgt): Failed to parse Reconfig ML IE");
		return -1;
	}

	if (!mle.has_target_ap_mld_addr ||
	    os_memcmp(mle.target_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN)) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC (tgt): Target AP MLD " MACSTR
			   " does not match our MLD " MACSTR,
			   MAC2STR(mle.target_ap_mld_addr),
			   MAC2STR(hapd->mld->mld_addr));
		return -1;
	}

	ap_info = uhr_find_ap_in_list(sta, sta->smd_info.current_ap_mld_addr);
	if (ap_info) {
		wpa_printf(MSG_DEBUG,
			   "UHR ST EXEC (tgt): Current AP " MACSTR " already in list; ignore",
			   MAC2STR(sta->smd_info.current_ap_mld_addr));
		return -1;
	}

	ap_info = os_zalloc(sizeof(*ap_info));
	if (!ap_info) {
		wpa_printf(MSG_ERROR,
			   "UHR ST EXEC (tgt): Failed to allocate ap_info");
		return -1;
	}

	ap_info->state = SMD_AP_STATE_IDLE;
	os_memcpy(ap_info->ap_mld_addr,
		  sta->smd_info.current_ap_mld_addr, ETH_ALEN);
	os_get_reltime(&ap_info->last_seen);

	ap_info->next = sta->smd_info.ap_list;
	sta->smd_info.ap_list = ap_info;

	wpa_printf(MSG_INFO,
		   "UHR ST EXEC (tgt): STA " MACSTR
		   " — sending CTX_REQUEST to Current AP " MACSTR,
		   MAC2STR(sta->addr),
		   MAC2STR(sta->smd_info.current_ap_mld_addr));

	/*
	 * Cancel the prep timer before the async CTX_REQUEST/CTX_RESPONSE
	 * window opens.  If the timer were left running it could fire during
	 * the exchange, call ap_free_sta(), and leave the CTX_RESPONSE handler
	 * with a dangling sta_info pointer.
	 */
	uhr_tgt_cancel_st_prep_timer(assoc_hapd, sta->addr);

	ret = uhr_iap_send_st_ctx_request(hapd,
					  sta->smd_info.current_ap_mld_addr,
					  sta->addr, 0);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "UHR ST EXEC (tgt): Failed to send IAP Ctx Request");
		uhr_remove_ap_from_list(sta, sta->smd_info.current_ap_mld_addr);
		return -1;
	}

	ap_info->state = SMD_AP_STATE_ST_EXEC_VIA_TGT_STARTED;

	if (uhr_cur_start_iap_msg_timer(sta, sta->smd_info.current_ap_mld_addr) < 0)
		wpa_printf(MSG_ERROR, "UHR ST EXEC (tgt): Failed to start IAP timer");

	return 0;
}


void uhr_cur_ap_handle_st_ctx_request(struct hostapd_data *hapd,
				      const struct uhr_iap_frame *iap,
				      u16 frame_len)
{
	struct sta_info *sta;
	struct smd_roam_ap_info *target_info;
	struct sta_smd_ctx_info *smd_ctx = NULL;
	u8 status = UHR_IAP_STATUS_FAILURE;
	u8 valid_ctx_bmap = 0xff;
	int ret = 0;

	wpa_printf(MSG_DEBUG,
		   "UHR: ST CTX REQUEST from Target AP " MACSTR
		   " for STA " MACSTR,
		   MAC2STR(iap->target_ap_mld_addr),
		   MAC2STR(iap->sta_addr));

	sta = ap_get_sta(hapd, iap->sta_addr);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX REQ: STA " MACSTR " not found",
			   MAC2STR(iap->sta_addr));
		return;
	}

	target_info = uhr_find_ap_in_list(sta, iap->target_ap_mld_addr);
	if (!target_info) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX REQ: Target AP " MACSTR " not in list",
			   MAC2STR(iap->target_ap_mld_addr));
		return;
	}

	if (target_info->state != SMD_AP_STATE_ST_PREP_COMPLETE) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX REQ: Invalid state %d",
			   target_info->state);
		return;
	}

	target_info->state = SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_REQ;

	/*
	 * 1. if ret=0 and smd_ctx is valid, driver has sent an immediate response;
	 *	send the IAP response with the received context.
	 * 2. if ret=0 and smd_ctx is not valid, driver will send the response later;
	 *	return and wait for the event.
	 * 3. otherwise, it is a failure; send the response with empty context (failure).
	 */
	if (sta->dl_sn_not_transferred)
		valid_ctx_bmap &= ~SMD_CTX_VALID_DL_SN;
	if (sta->ul_sn_not_transferred)
		valid_ctx_bmap &= ~SMD_CTX_VALID_UL_SN;

	ret = hostapd_drv_get_smd_ctx(hapd, sta->addr, 1, valid_ctx_bmap, 0xff, 0xff, &smd_ctx);
	if (!ret && !smd_ctx) {
		wpa_printf(MSG_INFO,
			   "UHR CTX REQ: waiting for driver ctx completion for " MACSTR,
			   MAC2STR(sta->addr));
		target_info->state = SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_WAIT;

		eloop_cancel_timeout(uhr_cur_get_ctx_timeout, hapd, sta);
		eloop_register_timeout(0, UHR_ST_GET_CTX_TIMEOUT_USEC,
				       uhr_cur_get_ctx_timeout,
				       hapd, sta);

		sta->smd_info.get_ctx_pending.active = true;
		os_memcpy(sta->smd_info.get_ctx_pending.target_ap_mld_addr,
			  iap->target_ap_mld_addr,
			  ETH_ALEN);
		sta->smd_info.get_ctx_pending.iap_transaction_id =
			iap->iap_transaction_id;
		return;
	} else if (!ret && smd_ctx) {
		status = UHR_IAP_STATUS_SUCCESS;
		target_info->state = SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_RESP;
	}

	ret = uhr_iap_send_st_ctx_response(hapd,
					   iap->target_ap_mld_addr,
					   iap->sta_addr,
					   iap->iap_transaction_id,
					   status,
					   smd_ctx);

	os_free(smd_ctx);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "UHR CTX REQ: Failed to send CTX_RESPONSE. ST_EXEC_VIA_TGT_CURR_CTX_REQ -> ST_PREP_COMPLETE");
		target_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
		return;
	}

	wpa_printf(MSG_INFO,
		   "UHR CTX REQ: Sent CTX_RESPONSE to Target AP " MACSTR ", "
		   "ST_EXEC_VIA_TGT_CURR_CTX_REQ -> ST_EXEC_VIA_TGT_CURR_CTX_RESP",
		   MAC2STR(iap->target_ap_mld_addr));
}


void uhr_tgt_ap_handle_st_ctx_response(struct hostapd_data *hapd,
					const struct uhr_iap_frame *iap,
					u16 frame_len)
{
	struct sta_info *sta = NULL;
	struct hostapd_data *lhapd = NULL;
	struct smd_roam_ap_info *ap_info;
	struct sta_smd_ctx_info *smd_ctx;
	u8 *resp_buf;
	size_t resp_len;
	int ret;
	u8 dialog_token;

	wpa_printf(MSG_DEBUG,
		   "UHR: ST CTX RESPONSE from Current AP " MACSTR
		   " for STA " MACSTR " status=%u",
		   MAC2STR(iap->current_ap_mld_addr),
		   MAC2STR(iap->sta_addr), iap->status_code);

	if (!hostapd_mld_find_assoc_sta(hapd, iap->sta_addr, &lhapd, &sta) ||
	    !sta) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX RESP: STA " MACSTR " not found",
			   MAC2STR(iap->sta_addr));
		return;
	}

	uhr_cancel_iap_timeout(sta, sta->smd_info.current_ap_mld_addr);

	ap_info = uhr_find_ap_in_list(sta, sta->smd_info.current_ap_mld_addr);
	if (!ap_info) {
		wpa_printf(MSG_ERROR, "UHR CTX RESP: Current AP info not found for " MACSTR,
			   MAC2STR(sta->smd_info.current_ap_mld_addr));
		return;
	}

	if (ap_info->state != SMD_AP_STATE_ST_EXEC_VIA_TGT_STARTED) {
		wpa_printf(MSG_ERROR, "UHR CTX RESP: Invalid state %u", ap_info->state);
		goto send_exec_fail;
	}

	if (iap->status_code != UHR_IAP_STATUS_SUCCESS ||
	    !(iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX)) {
		wpa_printf(MSG_ERROR, "UHR CTX RESP: Context fetch failed");
		goto send_exec_fail;
	}

	smd_ctx = (struct sta_smd_ctx_info *) iap->frame_ctx_data;
	if (uhr_target_ap_set_smd_ctx(lhapd, iap->sta_addr, smd_ctx)) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX RESP: Failed to apply SMD context");
		goto send_exec_fail;
	}

	/* dialog token is in the STA's saved ST Exec request — use 0 if unavailable */
	dialog_token = sta->smd_info.st_exec_dialog_token;

	resp_buf = uhr_tgt_build_st_exec_resp_frame(lhapd, sta, dialog_token,
						    WLAN_STATUS_SUCCESS,
						    &resp_len);
	if (!resp_buf) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX RESP: Failed to build ST Exec Response");
		uhr_remove_ap_from_list(sta, sta->smd_info.current_ap_mld_addr);
		return;
	}

	os_memcpy(((struct ieee80211_mgmt *) resp_buf)->da,
		  iap->sta_addr, ETH_ALEN);

	/*
	 * Notify the firmware that the TAP-side transition is complete.
	 * role=2 (Target AP), type=4 (DYNAMIC_CONTEXT) — mirrors the call
	 * in uhr_tgt_ap_handle_st_exec_req() for the IAP-forwarded path.
	 * Without this command the firmware does not update its SMD context
	 * (BA sessions, PN counters, peer state) and cannot TX to the STA.
	 */
	if (hostapd_smd_roam(lhapd, sta,
			     2,   /* role: Target AP */
			     4,   /* type: DYNAMIC_CONTEXT */
			     sta->dl_sn_not_transferred,
			     sta->ul_sn_not_transferred,
			     lhapd->conf->smd.uhr_dl_drain_duration_tu)) {
		wpa_printf(MSG_WARNING,
			   "UHR CTX RESP: Failed to send DYNAMIC_CONTEXT WMI for STA "
			   MACSTR, MAC2STR(iap->sta_addr));
		return;
	}

	ret = hostapd_drv_send_mlme(lhapd, resp_buf, resp_len,
			    0, NULL, 0, 0, 0, 0);
	os_free(resp_buf);

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX RESP: Failed to send OTA ST Exec Response");
		uhr_remove_ap_from_list(sta, sta->smd_info.current_ap_mld_addr);
		return;
	}

	uhr_tgt_cancel_st_prep_timer(lhapd, iap->sta_addr);
	uhr_remove_ap_from_list(sta, sta->smd_info.current_ap_mld_addr);

	wpa_printf(MSG_INFO,
		   "UHR CTX RESP: Sent OTA ST Exec Response to STA " MACSTR,
		   MAC2STR(iap->sta_addr));

	if (ap_sta_set_authorized_flag(lhapd, sta, 1)) {
		sta->flags_ext |= WLAN_STA_SMD;
		hostapd_set_sta_flags(lhapd, sta);
	}

	uhr_tgt_cancel_st_prep_timer(lhapd, iap->sta_addr);

	uhr_iap_send_st_exec_via_tgt_done(lhapd,
					  iap->current_ap_mld_addr,
					  iap->sta_addr,
					  iap->iap_transaction_id);
	return;

send_exec_fail:
	uhr_remove_ap_from_list(sta, sta->smd_info.current_ap_mld_addr);

	dialog_token = sta->smd_info.st_exec_dialog_token;
	resp_buf = uhr_tgt_build_st_exec_resp_frame(lhapd, sta, dialog_token,
						    WLAN_STATUS_UNSPECIFIED_FAILURE,
						    &resp_len);
	if (!resp_buf) {
		wpa_printf(MSG_ERROR,
			   "UHR CTX RESP: Failed to build failure ST Exec Response");
		uhr_tgt_cancel_st_prep_timer(lhapd, iap->sta_addr);
		return;
	}

	os_memcpy(((struct ieee80211_mgmt *) resp_buf)->da,
		  iap->sta_addr, ETH_ALEN);

	hostapd_drv_send_mlme(lhapd, resp_buf, resp_len, 0, NULL, 0, 0, 0, 0);
	os_free(resp_buf);
}


void uhr_cur_ap_handle_st_exec_via_tgt_done(struct hostapd_data *hapd,
					     const struct uhr_iap_frame *iap,
					     u16 frame_len)
{
	struct sta_info *sta;
	struct smd_roam_ap_info *chosen;

	wpa_printf(MSG_DEBUG,
		   "UHR: ST EXEC VIA TGT DONE from Target AP " MACSTR
		   " for STA " MACSTR,
		   MAC2STR(iap->target_ap_mld_addr),
		   MAC2STR(iap->sta_addr));

	sta = ap_get_sta(hapd, iap->sta_addr);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "UHR VIA TGT DONE: STA " MACSTR " not found",
			   MAC2STR(iap->sta_addr));
		return;
	}

	chosen = uhr_find_ap_in_list(sta, iap->target_ap_mld_addr);
	if (!chosen ||
	    chosen->state != SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_RESP) {
		wpa_printf(MSG_ERROR,
			   "UHR VIA TGT DONE: Target AP " MACSTR
			   " not in expected state",
			   MAC2STR(iap->target_ap_mld_addr));
		return;
	}

	if (hostapd_smd_roam(hapd, sta, 1, 3,
			     sta->dl_sn_not_transferred,
			     sta->ul_sn_not_transferred,
			     hapd->conf->smd.uhr_dl_drain_duration_tu))
		wpa_printf(MSG_DEBUG,
			   "UHR VIA TGT DONE: WMI roam notification failed");

	uhr_cur_ap_cancel_st_prep_for_entry(hapd, sta, chosen);
	uhr_remove_ap_from_list(sta, iap->target_ap_mld_addr);
	uhr_cur_ap_purge_ap_list(hapd, sta);

	wpa_printf(MSG_INFO,
		   "UHR VIA TGT DONE: STA " MACSTR
		   " fully transitioned, deleting ML station",
		   MAC2STR(iap->sta_addr));
	ap_sta_remove_link_sta(hapd, sta, 0, false);
	ap_free_sta(hapd, sta);
}
