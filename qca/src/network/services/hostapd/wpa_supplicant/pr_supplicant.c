/*
 * Proxmity Ranging
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/proximity_ranging.h"
#include "p2p/p2p.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "notify.h"
#include "driver_i.h"
#include "pr_supplicant.h"

#ifdef CONFIG_PASN
static void wpas_pr_pasn_timeout(void *eloop_ctx, void *timeout_ctx);
#endif /* CONFIG_PASN */


static int wpas_pr_edca_get_bw(enum edca_format_and_bw_value format_and_bw)
{
	switch (format_and_bw) {
	case EDCA_FORMAT_AND_BW_VHT20:
		return 20;
	case EDCA_FORMAT_AND_BW_HT40:
	case EDCA_FORMAT_AND_BW_VHT40:
		return 40;
	case EDCA_FORMAT_AND_BW_VHT80:
		return 80;
	case EDCA_FORMAT_AND_BW_VHT80P80:
	case EDCA_FORMAT_AND_BW_VHT160_DUAL_LO:
	case EDCA_FORMAT_AND_BW_VHT160_SINGLE_LO:
		return 160;
	default:
		return 0;
	}
}


static int wpas_pr_ntb_get_bw(enum ntb_format_and_bw_value format_and_bw)
{
	switch (format_and_bw) {
	case NTB_FORMAT_AND_BW_HE20:
		return 20;
	case NTB_FORMAT_AND_BW_HE40:
		return 40;
	case NTB_FORMAT_AND_BW_HE80:
		return 80;
	case NTB_FORMAT_AND_BW_HE80P80:
	case NTB_FORMAT_AND_BW_HE160_DUAL_LO:
	case NTB_FORMAT_AND_BW_HE160_SINGLE_LO:
		return 160;
	default:
		return 0;
	}
}


static bool
wpas_pr_edca_is_valid_op_class(enum edca_format_and_bw_value format_and_bw,
			       const struct oper_class_map *op_class_map)
{
	int bw = 0, op_class_bw = 0;

	if (!op_class_map)
		return false;

	op_class_bw = oper_class_bw_to_int(op_class_map);
	bw = wpas_pr_edca_get_bw(format_and_bw);

	if (!op_class_bw || !bw)
		return false;

	if (format_and_bw <= EDCA_FORMAT_AND_BW_VHT80 &&
	    format_and_bw >= EDCA_FORMAT_AND_BW_VHT20 &&
	    op_class_bw <= bw)
		return true;

	if (format_and_bw == EDCA_FORMAT_AND_BW_VHT80P80 &&
	    (op_class_bw < bw || op_class_map->bw == BW80P80))
		return true;

	if ((format_and_bw == EDCA_FORMAT_AND_BW_VHT160_DUAL_LO ||
	     format_and_bw == EDCA_FORMAT_AND_BW_VHT160_SINGLE_LO) &&
	    (op_class_bw < bw || op_class_map->bw == BW160))
		return true;

	return false;
}


static bool
wpas_pr_ntb_is_valid_op_class(enum ntb_format_and_bw_value format_and_bw,
			      const struct oper_class_map *op_class_map)
{
	int bw = 0, op_class_bw = 0;

	if (!op_class_map)
		return false;

	op_class_bw = oper_class_bw_to_int(op_class_map);
	bw = wpas_pr_ntb_get_bw(format_and_bw);

	if (!op_class_bw || !bw)
		return false;

	if (format_and_bw <= NTB_FORMAT_AND_BW_HE80 &&
	    format_and_bw >= NTB_FORMAT_AND_BW_HE20 &&
	    op_class_bw <= bw)
		return true;

	if (format_and_bw == NTB_FORMAT_AND_BW_HE80P80 &&
		   (op_class_bw < bw || op_class_map->bw == BW80P80))
		return true;

	if ((format_and_bw == NTB_FORMAT_AND_BW_HE160_DUAL_LO ||
	     format_and_bw == NTB_FORMAT_AND_BW_HE160_SINGLE_LO) &&
	    (op_class_bw < bw || op_class_map->bw == BW160))
		return true;

	return false;
}


static void
wpas_pr_setup_edca_channels(struct wpa_supplicant *wpa_s,
			    struct pr_channels *chan,
			    enum edca_format_and_bw_value format_and_bw)
{
	struct hostapd_hw_modes *mode;
	int cla = 0, i;

	for (i = 0; global_op_class[i].op_class; i++) {
		unsigned int ch;
		struct pr_op_class *op = NULL;
		const struct oper_class_map *o = &global_op_class[i];

		mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, o->mode,
				is_6ghz_op_class(o->op_class));
		if (!mode || is_6ghz_op_class(o->op_class) ||
		    !wpas_pr_edca_is_valid_op_class(format_and_bw, o))
			continue;

		for (ch = o->min_chan; ch <= o->max_chan; ch += o->inc) {
			enum chan_allowed res;

			/* Check for non-continuous jump in channel index
			 * increment.
			 */
			if (o->op_class >= 128 && o->op_class <= 130 &&
			    ch < 149 && ch + o->inc > 149)
				ch = 149;

			res = verify_channel(mode, o->op_class, ch, o->bw);

			if (res == ALLOWED) {
				if (!op) {
					if (cla == PR_MAX_OP_CLASSES)
						continue;

					wpa_printf(MSG_DEBUG,
						   "PR: Add operating class: %u (EDCA)",
						   o->op_class);
					op = &chan->op_class[cla];
					cla++;
					op->op_class = o->op_class;
				}
				if (op->channels == PR_MAX_OP_CLASS_CHANNELS)
					continue;
				op->channel[op->channels] = ch;
				op->channels++;
			}
		}

		if (op)
			wpa_hexdump(MSG_DEBUG, "PR: Channels (EDCA)",
				    op->channel, op->channels);
	}

	chan->op_classes = cla;
}


static void
wpas_pr_setup_ntb_channels(struct wpa_supplicant *wpa_s,
			   struct pr_channels *chan,
			   enum ntb_format_and_bw_value format_and_bw,
			   bool allow_6ghz)
{
	int cla = 0, i;
	struct hostapd_hw_modes *mode;

	for (i = 0; global_op_class[i].op_class; i++) {
		unsigned int ch;
		struct pr_op_class *op = NULL;
		const struct oper_class_map *o = &global_op_class[i];

		mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, o->mode,
				is_6ghz_op_class(o->op_class));
		if (!mode || (!allow_6ghz && is_6ghz_op_class(o->op_class)) ||
		    !wpas_pr_ntb_is_valid_op_class(format_and_bw, o))
			continue;

		for (ch = o->min_chan; ch <= o->max_chan; ch += o->inc) {
			enum chan_allowed res;

			/* Check for non-continuous jump in channel index
			 * increment.
			 */
			if (o->op_class >= 128 && o->op_class <= 130 &&
			    ch < 149 && ch + o->inc > 149)
				ch = 149;

			res = verify_channel(mode, o->op_class, ch, o->bw);

			if (res == ALLOWED) {
				if (!op) {
					if (cla == PR_MAX_OP_CLASSES)
						continue;
					wpa_printf(MSG_DEBUG,
						   "PR: Add operating class: %u (NTB)",
						   o->op_class);
					op = &chan->op_class[cla];
					cla++;
					op->op_class = o->op_class;
				}
				if (op->channels == PR_MAX_OP_CLASS_CHANNELS)
					continue;
				op->channel[op->channels] = ch;
				op->channels++;
			}
		}
		if (op) {
			wpa_hexdump(MSG_DEBUG, "PR: Channels (NTB)",
				    op->channel, op->channels);
		}
	}

	chan->op_classes = cla;
}


static int wpas_pr_pasn_send_mgmt(void *ctx, const u8 *data, size_t data_len,
				  int noack, unsigned int freq,
				  unsigned int wait)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_send_mlme(wpa_s, data, data_len, noack, freq, wait);
}


static void wpas_pr_pasn_result(void *ctx, u8 role, u8 protocol_type,
				u8 op_class, u8 op_channel, const char *country)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_pr_pasn_result(wpa_s, role, protocol_type, op_class,
				   op_channel, country);
}


static void wpas_pr_ranging_params(void *ctx, const u8 *dev_addr,
				   const u8 *peer_addr, u8 ranging_role,
				   u8 protocol_type, u8 op_class, u8 op_channel,
				   u8 self_format_bw, u8 peer_format_bw)
{
	struct wpa_supplicant *wpa_s = ctx;
	int bw, format_bw, freq;

	bw = oper_class_bw_to_int(get_oper_class(NULL, op_class));
	format_bw = self_format_bw < peer_format_bw ?
		self_format_bw : peer_format_bw;
	freq = ieee80211_chan_to_freq(NULL, op_class, op_channel);

	wpas_notify_pr_ranging_params(wpa_s, dev_addr, peer_addr, ranging_role,
				      protocol_type, freq, op_channel, bw,
				      format_bw);
}


static void wpas_pr_pasn_set_keys(void *ctx, const u8 *own_addr,
				  const u8 *peer_addr, int cipher, int akmp,
				  struct wpa_ptk *ptk)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_printf(MSG_DEBUG, "PR PASN: Set secure ranging context for " MACSTR,
		   MAC2STR(peer_addr));
	wpa_drv_set_secure_ranging_ctx(wpa_s, own_addr, peer_addr, cipher,
				       ptk->tk_len, ptk->tk,
				       ptk->ltf_keyseed_len,
				       ptk->ltf_keyseed, 0);
}


static void wpas_pr_pasn_clear_keys(void *ctx, const u8 *own_addr,
				    const u8 *peer_addr)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_printf(MSG_DEBUG, "PR PASN: Clear secure ranging context for "
		   MACSTR, MAC2STR(peer_addr));
	wpa_drv_set_secure_ranging_ctx(wpa_s, own_addr, peer_addr, 0, 0, NULL,
				       0, NULL, 1);
}


struct wpabuf * wpas_pr_usd_elems(struct wpa_supplicant *wpa_s)
{
	return pr_prepare_usd_elems(wpa_s->global->pr);
}


void wpas_pr_process_usd_elems(struct wpa_supplicant *wpa_s, const u8 *buf,
			       u16 buf_len, const u8 *peer_addr,
			       unsigned int freq)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr)
		return;
	pr_process_usd_elems(pr, buf, buf_len, peer_addr, freq);
}


int wpas_pr_init(struct wpa_global *global, struct wpa_supplicant *wpa_s,
		 const struct wpa_driver_capa *capa)
{
	struct pr_config pr;

	if (global->pr)
		return 0;

	os_memset(&pr, 0, sizeof(pr));

	os_memcpy(pr.dev_addr, wpa_s->own_addr, ETH_ALEN);
	pr.cb_ctx = wpa_s;
	pr.dev_name = wpa_s->conf->device_name;
	pr.pasn_type = wpa_s->conf->pr_pasn_type;
	pr.preferred_ranging_role = wpa_s->conf->pr_preferred_role;

	pr.edca_ista_support = wpa_s->drv_flags2 &
		WPA_DRIVER_FLAGS2_FTM_INITIATOR;
	pr.edca_rsta_support = wpa_s->drv_flags &
		WPA_DRIVER_FLAGS_FTM_RESPONDER;
	pr.edca_format_and_bw = capa->edca_format_and_bw;
	pr.max_rx_antenna = capa->max_rx_antenna;
	pr.max_tx_antenna = capa->max_tx_antenna;

	wpas_pr_setup_edca_channels(wpa_s, &pr.edca_channels,
				    pr.edca_format_and_bw);

	pr.ntb_ista_support = wpa_s->drv_flags2 &
		WPA_DRIVER_FLAGS2_NON_TRIGGER_BASED_INITIATOR;
	pr.ntb_rsta_support = wpa_s->drv_flags2 &
		WPA_DRIVER_FLAGS2_NON_TRIGGER_BASED_RESPONDER;
	pr.ntb_format_and_bw = capa->ntb_format_and_bw;
	pr.max_tx_ltf_repetations = capa->max_tx_ltf_repetations;
	pr.max_rx_ltf_repetations = capa->max_rx_ltf_repetations;
	pr.max_tx_ltf_total = capa->max_tx_ltf_total;
	pr.max_rx_ltf_total = capa->max_rx_ltf_total;
	pr.max_rx_sts_le_80 = capa->max_rx_sts_le_80;
	pr.max_rx_sts_gt_80 = capa->max_rx_sts_gt_80;
	pr.max_tx_sts_le_80 = capa->max_tx_sts_le_80;
	pr.max_tx_sts_gt_80 = capa->max_tx_sts_gt_80;

	pr.support_6ghz = wpas_is_6ghz_supported(wpa_s, true);

	pr.pasn_send_mgmt = wpas_pr_pasn_send_mgmt;
	pr.pasn_result = wpas_pr_pasn_result;
	pr.get_ranging_params = wpas_pr_ranging_params;
	pr.set_keys = wpas_pr_pasn_set_keys;
	pr.clear_keys = wpas_pr_pasn_clear_keys;

	pr.secure_he_ltf = wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA;

	wpas_pr_setup_ntb_channels(wpa_s, &pr.ntb_channels,
				   pr.ntb_format_and_bw,
				   pr.support_6ghz);

	if (wpa_s->conf->country[0] && wpa_s->conf->country[1]) {
		os_memcpy(pr.country, wpa_s->conf->country, 2);
		pr.country[2] = 0x04;
	} else {
		os_memcpy(pr.country, "XX\x04", 3);
	}

	if (wpa_s->conf->dik &&
	    wpabuf_len(wpa_s->conf->dik) <= DEVICE_IDENTITY_KEY_LEN) {
		pr.dik_cipher = wpa_s->conf->dik_cipher;
		pr.dik_len = wpabuf_len(wpa_s->conf->dik);
		os_memcpy(pr.dik_data, wpabuf_head(wpa_s->conf->dik),
			  pr.dik_len);
		pr.expiration = 24; /* hours */
	} else {
		pr.dik_cipher = DIRA_CIPHER_VERSION_128;
		pr.dik_len = DEVICE_IDENTITY_KEY_LEN;
		pr.expiration = 24; /* hours */
		if (os_get_random(pr.dik_data, pr.dik_len) < 0)
			return -1;

		wpa_s->conf->dik =
			wpabuf_alloc_copy(pr.dik_data, pr.dik_len);
		if (!wpa_s->conf->dik)
			return -1;

		wpa_s->conf->dik_cipher = pr.dik_cipher;

		wpa_printf(MSG_DEBUG, "PR: PR init new DIRA set");

		if (wpa_s->conf->update_config &&
		    wpa_config_write(wpa_s->confname, wpa_s->conf))
			wpa_printf(MSG_DEBUG,
				   "PR: Failed to update configuration");
	}

	global->pr = pr_init(&pr);
	if (!global->pr) {
		wpa_printf(MSG_DEBUG, "PR: Failed to init PR");
		return -1;
	}
	global->pr_init_wpa_s = wpa_s;

	return 0;
}


void wpas_pr_flush(struct wpa_supplicant *wpa_s)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (pr)
		pr_flush(pr);
}

void wpas_pr_deinit(struct wpa_supplicant *wpa_s)
{
	if (wpa_s == wpa_s->global->pr_init_wpa_s) {
		pr_deinit(wpa_s->global->pr);
		wpa_s->global->pr = NULL;
		wpa_s->global->pr_init_wpa_s = NULL;
	}

#ifdef CONFIG_PASN
	eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);
#endif /* CONFIG_PASN */
}


void wpas_pr_update_dev_addr(struct wpa_supplicant *wpa_s)
{
	pr_set_dev_addr(wpa_s->global->pr, wpa_s->own_addr);
}


void wpas_pr_clear_dev_iks(struct wpa_supplicant *wpa_s)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr)
		return;

	pr_clear_dev_iks(pr);
}


void wpas_pr_set_dev_ik(struct wpa_supplicant *wpa_s, const u8 *dik,
			const char *password, const u8 *pmk, size_t pmk_len,
			bool own)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr || !dik)
		return;

	pr_add_dev_ik(pr, dik, password, pmk, pmk_len, own);
}


#ifdef CONFIG_PASN

struct wpa_pr_pasn_auth_work {
	u8 peer_addr[ETH_ALEN];
	u8 auth_mode;
	int freq;
	enum pr_pasn_role role;
	u8 ranging_role;
	u8 ranging_type;
	u8 *ssid;
	size_t ssid_len;
	u8 bssid[ETH_ALEN];
	int forced_pr_freq;
};


static void wpas_pr_pasn_free_auth_work(struct wpa_pr_pasn_auth_work *awork)
{
	if (!awork)
		return;
	os_free(awork->ssid);
	os_free(awork);
}


static void wpas_pr_pasn_cancel_auth_work(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "PR PASN: Cancel pr-pasn-start-auth work");

	/* Remove pending/started work */
	radio_remove_works(wpa_s, "pr-pasn-start-auth", 0);
}


static void wpas_pr_pasn_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->pr_pasn_auth_work) {
		wpas_pr_pasn_cancel_auth_work(wpa_s);
		wpa_s->pr_pasn_auth_work = NULL;
	}
	wpa_printf(MSG_DEBUG, "PR: PASN timed out");
}


static void wpas_pr_pasn_auth_start_cb(struct wpa_radio_work *work, int deinit)
{
	int ret;
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_pr_pasn_auth_work *awork = work->ctx;
	struct pr_data *pr = wpa_s->global->pr;
	const u8 *peer_addr = NULL;

	if (deinit) {
		if (!work->started)
			eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);

		wpas_pr_pasn_free_auth_work(awork);
		return;
	}

	if (!is_zero_ether_addr(awork->peer_addr))
		peer_addr = awork->peer_addr;

	ret = pr_initiate_pasn_auth(pr, peer_addr, awork->freq,
				    awork->auth_mode, awork->ranging_role,
				    awork->ranging_type, awork->forced_pr_freq);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Failed to start PASN authentication");
		goto fail;
	}

	eloop_cancel_timeout(wpas_pr_pasn_timeout, wpa_s, NULL);
	eloop_register_timeout(2, 0, wpas_pr_pasn_timeout, wpa_s, NULL);
	wpa_s->pr_pasn_auth_work = work;
	return;

fail:
	wpas_pr_pasn_free_auth_work(awork);
	work->ctx = NULL;
	radio_work_done(work);
}


int wpas_pr_initiate_pasn_auth(struct wpa_supplicant *wpa_s,
			       const u8 *peer_addr, int freq, u8 auth_mode,
			       u8 ranging_role, u8 ranging_type,
			       int forced_pr_freq)
{
	struct wpa_pr_pasn_auth_work *awork;

	wpas_pr_pasn_cancel_auth_work(wpa_s);
	wpa_s->pr_pasn_auth_work = NULL;

	awork = os_zalloc(sizeof(*awork));
	if (!awork)
		return -1;

	awork->freq = freq;
	os_memcpy(awork->peer_addr, peer_addr, ETH_ALEN);
	awork->ranging_role = ranging_role;
	awork->ranging_type = ranging_type;
	awork->auth_mode = auth_mode;
	awork->forced_pr_freq = forced_pr_freq;

	if (radio_add_work(wpa_s, freq, "pr-pasn-start-auth", 1,
			   wpas_pr_pasn_auth_start_cb, awork) < 0) {
		wpas_pr_pasn_free_auth_work(awork);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "PR PASN: Authentication work successfully added");
	return 0;
}


int wpas_pr_pasn_auth_tx_status(struct wpa_supplicant *wpa_s, const u8 *data,
				size_t data_len, bool acked)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!wpa_s->pr_pasn_auth_work)
		return -1;

	return pr_pasn_auth_tx_status(pr, data, data_len, acked);
}


int wpas_pr_pasn_auth_rx(struct wpa_supplicant *wpa_s,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 int freq)
{
	struct pr_data *pr = wpa_s->global->pr;

	if (!pr)
		return -2;
	return pr_pasn_auth_rx(pr, mgmt, len, freq);
}

#endif /* CONFIG_PASN */
