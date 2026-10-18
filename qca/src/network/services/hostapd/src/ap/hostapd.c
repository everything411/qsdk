/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2021, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#ifdef CONFIG_SQLITE
#include <sqlite3.h>
#endif /* CONFIG_SQLITE */

#include <linux/netfilter.h>
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/crc32.h"
#include "common/ieee802_11_defs.h"
#include "common/qca-vendor.h"
#include "common/wpa_ctrl.h"
#include "common/hw_features_common.h"
#include "radius/radius_client.h"
#include "radius/radius_das.h"
#include "eap_server/tncs.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "eapol_auth/eapol_auth_sm_i.h"
#include "fst/fst.h"
#include "hostapd.h"
#include "hostapd_log.h"
#include "authsrv.h"
#include "sta_info.h"
#include "accounting.h"
#include "ap_list.h"
#include "beacon.h"
#include "ieee802_1x.h"
#include "ieee802_11_auth.h"
#include "vlan_init.h"
#include "wpa_auth.h"
#include "wps_hostapd.h"
#include "dpp_hostapd.h"
#include "nan_usd_ap.h"
#include "gas_query_ap.h"
#include "hw_features.h"
#ifdef CONFIG_MQTT
#include "hostapd_mqtt.h"
#endif /* CONFIG_MQTT */
#include "hostapd_if/hostapd_if.h"
#include "wpa_auth_glue.h"
#include "ap_drv_ops.h"
#include "../drivers/driver_nl80211.h"
#include "ap_config.h"
#include "p2p_hostapd.h"
#include "gas_serv.h"
#include "dfs.h"
#include "ieee802_11.h"
#include "bss_load.h"
#include "x_snoop.h"
#include "dhcp_snoop.h"
#include "ndisc_snoop.h"
#include "neighbor_db.h"
#include "rrm.h"
#include "fils_hlp.h"
#include "acs.h"
#include "hs20.h"
#include "airtime_policy.h"
#include "wpa_auth_kay.h"
#include "hw_features.h"
#include "interference.h"
#include "robust_av.h"
#include "atf/atf_offload.h"
#ifdef CONFIG_IEEE80211BN
#include "uhr_utils.h"
#include "eth_p_1905.h"
#include "uhr_neighbor_update.h"
#endif /* CONFIG_IEEE80211BN */
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#include "../../qcn_extns/dfs_extn.h"
#endif /* CONFIG_QCN_EXTN */
#include "nft.h"
#ifdef CONFIG_IEEE80211BN
#include "mapc.h"
#endif /* CONFIG_IEEE80211BN */

static int hostapd_flush_old_stations(struct hostapd_data *hapd, u16 reason);
#ifdef CONFIG_WEP
static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd);
static int hostapd_broadcast_wep_clear(struct hostapd_data *hapd);
#endif /* CONFIG_WEP */
static int setup_interface2(struct hostapd_iface *iface);
static void channel_list_update_timeout(void *eloop_ctx, void *timeout_ctx);
static void hostapd_interface_setup_failure_handler(void *eloop_ctx,
						    void *timeout_ctx);
int hostapd_mbssid_setup_bss(struct hostapd_data *hapd);

#ifdef CONFIG_IEEE80211AX
void hostapd_switch_color_timeout_handler(void *eloop_data,
					  void *user_ctx);
#endif /* CONFIG_IEEE80211AX */

int hostapd_setup_monitor_iface(struct hostapd_iface *iface)
{
	struct hostapd_config *conf = iface->conf;
	int ifindex = 0;

	if (!conf->monitor_iface_name[0])
		return 0;

	/* Prevent override if already configured */
	if (iface->monitor_iface_configured) {
		wpa_printf(MSG_WARNING,
			   "Monitor iface: Already configured for radio %s (current=%s)",
			   iface->phy, iface->monitor_iface);
		return 0;
	}

	if (hostapd_validate_monitor_iface(conf->monitor_iface_name,
					   &ifindex) < 0) {
		wpa_printf(MSG_ERROR,
			   "Monitor iface: Validation failed for %s on radio %s",
			   conf->monitor_iface_name, iface->phy);
		return -1;
	}

	os_strlcpy(iface->monitor_iface, conf->monitor_iface_name,
		   sizeof(iface->monitor_iface));
	iface->monitor_ifindex = ifindex;
	iface->monitor_iface_configured = true;

	wpa_printf(MSG_INFO,
		   "Monitor iface: %s configured for radio %s (ifindex=%d)",
		   iface->monitor_iface, iface->phy, iface->monitor_ifindex);

	return 0;
}

static void hostapd_cleanup_monitor_iface(struct hostapd_iface *iface)
{
	if (!iface->monitor_iface_configured)
		return;

	wpa_printf(MSG_INFO,
		   "Monitor iface: Cleaning up %s, must be managed by other userspace entity\n",
		   iface->monitor_iface);

	iface->monitor_iface[0] = '\0';
	iface->monitor_ifindex = 0;
	iface->monitor_iface_configured = false;
}

static int hostapd_adjust_legacy_beacon_rate(struct hostapd_data *hapd)
{
	int i, rate, best, req;
	int best_low, best_high;

	if (hapd->conf->rate_type != BEACON_RATE_LEGACY ||
	    !hapd->conf->beacon_rate)
		return 0;

	req = (int) hapd->conf->beacon_rate;
	best_high = 0;
	best_low = 0;

	for (i = 0; i < hapd->num_rates; i++) {
		if (!(hapd->current_rates[i].flags & HOSTAPD_RATE_BASIC))
			continue;

		rate = hapd->current_rates[i].rate;
		if (rate == req) {
			/* Configured beacon tx rate found in the basic rate */
			return 0;
		}

		if (req < rate && (!best_high || rate < best_high))
			best_high = rate;

		if (req > rate && (!best_low || rate > best_low))
			best_low = rate;
	}

	if (!best_low && !best_high) {
		wpa_printf(MSG_ERROR,
				"Unable to fit beacon tx rate %d Kbps within the basic rates",
				req * 100);
		return -1;
	}

	best = best_high ? best_high : best_low;

	wpa_printf(MSG_INFO,
			"Configured beacon tx rate %u Kbps is not in the basic rate set; "
			"adjusting to %d Kbps (%s basic rate)",
			req * 100, best * 100,
			best_high ? "next higher" : "nearest lower");
	hapd->conf->beacon_rate = best;

	return 0;
}

/* Prepare per-BSS rates from BSS config and current hw mode */
static int hostapd_prepare_rates(struct hostapd_data *hapd,
				 struct hostapd_hw_modes *mode)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int i, num_basic_rates = 0;
	int basic_rates_a[] = { 60, 120, 240, 0 };
	int basic_rates_b[] = { 10, 20, 0 };
	int basic_rates_g[] = { 10, 20, 55, 110, 0 };
	const int *basic_rates;

	if (conf->basic_rates)
		basic_rates = conf->basic_rates;
	else switch (mode->mode) {
		case HOSTAPD_MODE_IEEE80211A:
			basic_rates = basic_rates_a;
			break;
		case HOSTAPD_MODE_IEEE80211B:
			basic_rates = basic_rates_b;
			break;
		case HOSTAPD_MODE_IEEE80211G:
			basic_rates = basic_rates_g;
			break;
		case HOSTAPD_MODE_IEEE80211AD:
			return 0; /* No basic rates for 11ad */
		default:
			return -1;
	}

	os_free(hapd->basic_rates);
	hapd->basic_rates = int_array_dup(basic_rates);

	os_free(hapd->current_rates);
	hapd->num_rates = 0;
	hapd->current_rates = os_calloc(mode->num_rates,
					sizeof(struct hostapd_rate_data));
	if (!hapd->current_rates) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for rate "
			   "table.");
		return -1;
	}

	for (i = 0; i < mode->num_rates; i++) {
		struct hostapd_rate_data *rate;

		if (conf->supported_rates &&
		    !int_array_includes(conf->supported_rates, mode->rates[i]))
			continue;

		rate = &hapd->current_rates[hapd->num_rates];
		rate->rate = mode->rates[i];
		if (int_array_includes(basic_rates, rate->rate)) {
			rate->flags |= HOSTAPD_RATE_BASIC;
			num_basic_rates++;
		}

		wpa_printf(MSG_DEBUG, "BSS RATE[%d] rate=%d flags=0x%x",
			   hapd->num_rates, rate->rate, rate->flags);
		hapd->num_rates++;
	}

	if ((hapd->num_rates == 0 || num_basic_rates == 0) &&
	    (!hapd->iconf->ieee80211n ||
	     (!hapd->iconf->require_ht && !hapd->conf->bss_require_ht))) {
		wpa_printf(MSG_ERROR,
			   "No rates remaining in supported/basic rate sets (%d,%d).",
			   hapd->num_rates, num_basic_rates);
		return -1;
	}

	/* Legacy beacon_rate Validation: Match beacon_rate with available
	 * basic rate. If not present in the basic rate set, adjust to the
	 * nearest valid basic rate; fail if no suitable rate exists.*/
	if (hostapd_adjust_legacy_beacon_rate(hapd) < 0)
		return -1;

	return 0;
}


int hostapd_for_each_interface(struct hapd_interfaces *interfaces,
			       int (*cb)(struct hostapd_iface *iface,
					 void *ctx), void *ctx)
{
	size_t i;
	int ret;

	for (i = 0; i < interfaces->count; i++) {
		if (!interfaces->iface[i])
			continue;
		ret = cb(interfaces->iface[i], ctx);
		if (ret)
			return ret;
	}

	return 0;
}


static int hostapd_for_each_iface_on_phy(struct hapd_interfaces *interfaces,
					 const char *phy_name,
					 int (*cb)(struct hostapd_iface *iface,
						   void *ctx), void *ctx)
{
	size_t i;
	int ret;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];
		const char *name;

		if (!iface || !iface->num_bss || !iface->bss[0])
			continue;
		name = hostapd_drv_get_radio_name(iface->bss[0]);
		if (!name || os_strcmp(name, phy_name) != 0)
			continue;
		ret = cb(iface, ctx);
		if (ret)
			return ret;
	}

	return 0;
}


struct hostapd_data * hostapd_mbssid_get_tx_bss(struct hostapd_data *hapd)
{
	if (hapd->iconf->mbssid) {
		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			return hapd->mbssid_group? hapd->mbssid_group->txbss : hapd;
		else
			return hapd->iface->bss[0];
	}
	return hapd;
}


int hostapd_tx_bss_only(struct hostapd_data *hapd, const char *op_name)
{
	struct hostapd_data *tx = hostapd_mbssid_get_tx_bss(hapd);

	if (tx != hapd) {
		wpa_printf(MSG_ERROR, "%s not allowed on non-transmitting BSS",
			   op_name);
		return -1;
	}

	return 0;
}


void hostapd_free_mbssid_idx(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

	if (iface->conf->mbssid != MBSSID_DISABLED) {
		if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			if (group)
				group->mbssid_idx_bmap &= ~BIT(hapd->mbssid_idx);
		} else {
			iface->mbssid_idx_bmap &= ~BIT(hapd->mbssid_idx);
		}
	}
}

static int hostapd_get_bss_index(struct hostapd_data *bss)
{
	int i;

	for (i = 0; i < bss->iface->num_bss; i++) {
		if (bss->iface->bss[i] == bss)
			return i;
	}

	return -1;
}

unsigned int hostapd_mbssid_get_bss_index(struct hostapd_data *hapd)
{
	if (hapd->iconf->mbssid)
		return hapd->mbssid_idx;
	return 0;
}

struct hostapd_data *
hostapd_get_multi_group_bss(struct hostapd_multi_mbssid_group *group,
		int bss_idx)
{
	struct hostapd_iface *iface = NULL;

	if (!group)
		return NULL;

	if (group->txbss)
		iface = group->txbss->iface;
	if (!iface)
		return NULL;

	if (iface->conf->mbssid) {
		struct hostapd_data *bss;
		size_t i = 0;

		if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			dl_list_for_each(bss, &group->bss_list,
					struct hostapd_data, mbssid_bss) {
				if (i == bss_idx)
					return bss;
				i++;
			}
		}
	}
	return NULL;
}



bool hostapd_check_reenable_bss(struct hostapd_iface *iface,
				enum hostapd_reenable_mode reason)
{
	int b;

	for (b = 0; b < iface->num_bss; b++) {
		if (reason == REENABLE_NONE) {
			if (hapd_reenable_pending(iface->bss[b]))
				return true;
			continue;
		}
		if (iface->bss[b]->reenable == reason)
			return true;
	}

	return false;
}

bool
hostapd_switch_pending_bss(struct hostapd_iface *iface,
			   struct csa_settings *settings,
			   enum hostapd_reenable_mode reason)
{
	bool started = false;
	int b, err;

	for (b = 0; b < iface->num_bss; b++) {
		struct hostapd_data *hapd = iface->bss[b];

		if (!hapd)
			continue;
		/* REENABLE_NONE is a wildcard: match any pending BSS */
		if (reason == REENABLE_NONE) {
			if (!hapd_reenable_pending(hapd))
				continue;
		} else if (hapd->reenable != reason) {
			continue;
		}

		err = hostapd_switch_channel(hapd, settings);
		if (!err)
			started = true;
	}

	return started;
}


bool hostapd_enable_pending_bss(struct hostapd_iface *iface,
				enum hostapd_reenable_mode reason,
				bool dfs_cleanup)
{
	int b;

	for (b = 0; b < iface->num_bss; b++) {
		struct hostapd_data *hapd = iface->bss[b];

		if (reason == REENABLE_NONE) {
			if (!hapd_reenable_pending(hapd))
				continue;
		} else if (hapd->reenable != reason) {
			continue;
		}

		if (hostapd_enable_bss(hapd) < 0)
			wpa_printf(MSG_ERROR, "Enabling of BSS %s failed",
				   hapd->conf->iface);

		if (hapd->started)
			hostapd_set_state(iface, HAPD_IFACE_ENABLED);
	}

	if (dfs_cleanup && !iface->bss[0]->started &&
	    iface->bss[0]->reenable != REENABLE_REUSE_LINK) {
		ieee802_11_set_beacon(iface->bss[0]);
		hostapd_drv_stop_ap(iface->bss[0]);
	}

	return true;
}


u8 hostapd_max_bssid_indicator(struct hostapd_data *hapd)
{
	size_t num_bss_nontx;
	u8 max_bssid_ind = 0;
	unsigned int num_hws;

	if (!hapd->iconf->mbssid)
		return 0;

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		num_bss_nontx = hapd->iconf->group_size - 1;
	} else {
		num_hws = hapd->iface->num_multi_hws ? hapd->iface->num_multi_hws : 1;
		/* In single wiphy, maximum interfaces supported by each radio are
		 * added, hence divide by num_multi_hws to get per radio limit */
		num_bss_nontx = (hapd->iface->mbssid_max_interfaces /
				 num_hws) - 1;

		if (hapd->iface->mbssid_max_interfaces % num_hws)
			num_bss_nontx++;
	}

	while (num_bss_nontx > 0) {
		max_bssid_ind++;
		num_bss_nontx >>= 1;
	}
	return max_bssid_ind;
}

void hostapd_reconfig_encryption(struct hostapd_data *hapd)
{
	if (hapd->wpa_auth)
		return;

	hostapd_set_privacy(hapd, 0);
#ifdef CONFIG_WEP
	hostapd_setup_encryption(hapd->conf->iface, hapd);
#endif /* CONFIG_WEP */
}


static void hostapd_reload_bss(struct hostapd_data *hapd)
{
	struct hostapd_ssid *ssid;

	if (!hapd->started)
		return;

	if (hapd->conf->wmm_enabled < 0)
		hapd->conf->wmm_enabled = hapd->iconf->ieee80211n |
			hapd->iconf->ieee80211ax;

#ifndef CONFIG_NO_RADIUS
	radius_client_reconfig(hapd->radius, hapd->conf->radius);
#endif /* CONFIG_NO_RADIUS */

	ssid = &hapd->conf->ssid;
	if (!ssid->wpa_psk_set && ssid->wpa_psk && !ssid->wpa_psk->next &&
	    ssid->wpa_passphrase_set && ssid->wpa_passphrase) {
		/*
		 * Force PSK to be derived again since SSID or passphrase may
		 * have changed.
		 */
		hostapd_config_clear_wpa_psk(&hapd->conf->ssid.wpa_psk);
	}

	if (hostapd_setup_wpa_psk(hapd->conf)) {
		wpa_printf(MSG_ERROR, "Failed to re-configure WPA PSK "
			   "after reloading configuration");
	}

	if (hapd->conf->ieee802_1x || hapd->conf->wpa)
		hostapd_set_drv_ieee8021x(hapd, hapd->conf->iface, 1);
	else
		hostapd_set_drv_ieee8021x(hapd, hapd->conf->iface, 0);

	if (hapd->conf->wpa && hapd->wpa_auth == NULL) {
		hostapd_setup_wpa(hapd);
		if (hapd->wpa_auth)
			wpa_init_keys(hapd->wpa_auth);
	} else if (hapd->conf->wpa) {
		const u8 *wpa_ie;
		size_t wpa_ie_len;
		hostapd_reconfig_wpa(hapd);
		wpa_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &wpa_ie_len);
		if (hostapd_set_generic_elem(hapd, wpa_ie, wpa_ie_len))
			wpa_printf(MSG_ERROR, "Failed to configure WPA IE for "
				   "the kernel driver.");
	} else if (hapd->wpa_auth) {
		wpa_deinit(hapd->wpa_auth);
		hapd->wpa_auth = NULL;
		hostapd_set_privacy(hapd, 0);
#ifdef CONFIG_WEP
		hostapd_setup_encryption(hapd->conf->iface, hapd);
#endif /* CONFIG_WEP */
		hostapd_set_generic_elem(hapd, (u8 *) "", 0);
	}

	hostapd_neighbor_sync_own_report(hapd);

	if (hapd->iface->current_mode) {
		if (hostapd_prepare_rates(hapd, hapd->iface->current_mode)) {
			wpa_printf(MSG_ERROR, "Failed to prepare rates table.");
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
					HOSTAPD_LEVEL_WARNING,
					"Failed to prepare rates table.");
			return;
		}
	}

#ifdef CONFIG_IEEE80211BN
	if (hapd->smd_neighbor_update_ctx)
		smd_neighbor_update_notify_own_report_changed(hapd);

	mapc_deinit(hapd);
	if (mapc_init(hapd) < 0)
		wpa_printf(MSG_ERROR, "MAPC: reinit failed for BSS %s",
			   hapd->conf->iface);
#endif /* CONFIG_IEEE80211BN */
	ieee802_11_set_beacon(hapd);
	hostapd_update_wps(hapd);

	if (hapd->conf->ssid.ssid_set &&
	    hostapd_set_ssid(hapd, hapd->conf->ssid.ssid,
			     hapd->conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		/* try to continue */
	}
#ifdef CONFIG_DPP2
	hostapd_dpp_reload_controllers(hapd);
#endif /* CONFIG_DPP2 */
	wpa_printf(MSG_DEBUG, "Reconfigured interface %s", hapd->conf->iface);
}


void hostapd_clear_old_bss(struct hostapd_data *bss)
{
	wpa_printf(MSG_DEBUG, "BSS %s changed - clear old state",
		   bss->conf->iface);

	/*
	 * Deauthenticate all stations since the new configuration may not
	 * allow them to use the BSS anymore.
	 */
	hostapd_flush_old_stations(bss, WLAN_REASON_PREV_AUTH_NOT_VALID);
#ifdef CONFIG_WEP
	hostapd_broadcast_wep_clear(bss);
#endif /* CONFIG_WEP */

#ifndef CONFIG_NO_RADIUS
	/* TODO: update dynamic data based on changed configuration
	 * items (e.g., open/close sockets, etc.) */
	radius_client_flush(bss->radius, 0);
#endif /* CONFIG_NO_RADIUS */
}


static void hostapd_clear_old(struct hostapd_iface *iface)
{
	size_t j;

	for (j = 0; j < iface->num_bss; j++)
		hostapd_clear_old_bss(iface->bss[j]);
}


void hostapd_clear_local_tpe_bss(struct hostapd_data *hapd)
{
	ieee80211_tpe_config_user_params *tpe_conf;

	if (!hapd || !hapd->conf)
		return;

	tpe_conf = &hapd->conf->tpe_ie_config;
	if (!tpe_conf->local_tpe_config)
		return;

	os_memset(tpe_conf, 0, sizeof(*tpe_conf));
}


void hostapd_clear_local_tpe(struct hostapd_iface *iface)
{
	size_t i;

	if (!iface)
		return;

	for (i = 0; i < iface->num_bss; i++)
		hostapd_clear_local_tpe_bss(iface->bss[i]);
}


static int hostapd_iface_conf_changed(struct hostapd_config *newconf,
				      struct hostapd_config *oldconf)
{
	size_t i;

	if (newconf->num_bss != oldconf->num_bss)
		return 1;

	for (i = 0; i < newconf->num_bss; i++) {
		if (os_strcmp(newconf->bss[i]->iface,
			      oldconf->bss[i]->iface) != 0)
			return 1;
#ifdef CONFIG_IEEE80211BE
		if (newconf->bss[i]->mld_ap != oldconf->bss[i]->mld_ap)
			return 1;
#endif /* CONFIG_IEEE80211BE */
	}

	return 0;
}

int hostapd_iface_num_sta(struct hostapd_iface *iface)
{
	int num_sta = 0;
	int i;

	for (i = 0; i < iface->num_bss; i++)
		num_sta += iface->bss[i]->num_sta;

	return num_sta;
}

int hostapd_check_max_sta(struct hostapd_data *hapd)
{
	if (hapd->num_sta >= hapd->conf->max_num_sta)
		return 1;

	if (hapd->iconf->max_num_sta &&
	    hostapd_iface_num_sta(hapd->iface) >= hapd->iconf->max_num_sta)
		return 1;

	/* Driver capability enforcement: respect drv_max_stations if set */
	if (hapd->iface->drv_max_stations &&
	    hostapd_iface_num_sta(hapd->iface) >= hapd->iface->drv_max_stations)
		return 1;

	return 0;
}

int hostapd_reload_config_iface(struct hostapd_iface **ifacep)
{
	struct hostapd_iface *iface = *ifacep;
	struct hapd_interfaces *interfaces;
	struct hostapd_data *hapd;
	struct hostapd_config *newconf, *oldconf;
	size_t j;

	if (!iface)
		return -1;

	interfaces = iface->interfaces;
	hapd = iface->bss[0];

	hostapd_ucode_reload_bss(hapd);

	if (iface->config_fname == NULL) {
		/* Only in-memory config in use - assume it has been updated */
		hostapd_clear_old(iface);
		for (j = 0; j < iface->num_bss; j++)
			hostapd_reload_bss(iface->bss[j]);
		return 0;
	}

	if (iface->interfaces == NULL ||
	    iface->interfaces->config_read_cb == NULL)
		return -1;
	newconf = iface->interfaces->config_read_cb(iface->config_fname);
	if (newconf == NULL)
		return -1;

	oldconf = hapd->iconf;
	if (hostapd_iface_conf_changed(newconf, oldconf)) {
		char *fname;
		int res;

		hostapd_clear_old(iface);

		wpa_printf(MSG_DEBUG,
			   "Configuration changes include interface/BSS modification - force full disable+enable sequence");
		fname = os_strdup(iface->config_fname);
		if (!fname) {
			hostapd_config_free(newconf);
			return -1;
		}

		if (hostapd_remove_hapd_iface(iface) != 0) {
			os_free(fname);
			hostapd_config_free(newconf);
			return -1;
		}

		iface = hostapd_init(interfaces, fname);
		os_free(fname);
		hostapd_config_free(newconf);
		if (!iface) {
			wpa_printf(MSG_ERROR,
				   "Failed to initialize interface on config reload");
			return -1;
		}
		iface->interfaces = interfaces;
		interfaces->iface[interfaces->count] = iface;
		interfaces->count++;
		res = hostapd_enable_iface(iface);
		if (res < 0)
			wpa_printf(MSG_ERROR,
				   "Failed to enable interface on config reload");
		*ifacep = iface;
		return res;
	}

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (!hapd->conf->config_id || !newconf->bss[j]->config_id ||
		    os_strcmp(hapd->conf->config_id,
			      newconf->bss[j]->config_id) != 0)
			hostapd_clear_old_bss(hapd);
		hapd->iconf = newconf;
		hapd->iconf->channel = oldconf->channel;
		hapd->iconf->acs = oldconf->acs;
		hapd->iconf->secondary_channel = oldconf->secondary_channel;
		hapd->iconf->ieee80211n = oldconf->ieee80211n;
		hapd->iconf->ieee80211ac = oldconf->ieee80211ac;
		hapd->iconf->ht_capab = oldconf->ht_capab;
		hapd->iconf->vht_capab = oldconf->vht_capab;
		hostapd_set_oper_chwidth(hapd->iconf,
					 hostapd_get_oper_chwidth(oldconf));
		hostapd_set_oper_centr_freq_seg0_idx(
			hapd->iconf,
			hostapd_get_oper_centr_freq_seg0_idx(oldconf));
		hostapd_set_oper_centr_freq_seg1_idx(
			hapd->iconf,
			hostapd_get_oper_centr_freq_seg1_idx(oldconf));
		hapd->conf = newconf->bss[j];
		hapd->iconf->bandwidth_device = oldconf->bandwidth_device;
		hapd->iconf->center_freq_device = oldconf->center_freq_device;
		hostapd_reload_bss(hapd);
	}

	iface->conf = newconf;
#ifdef CONFIG_QCN_EXTN
	hostapd_periodic_acs_start(iface);
#endif
	hostapd_config_free(oldconf);


	return 0;
}


int hostapd_reload_config_bss(struct hostapd_iface *iface,
			      const char *iface_name,
			      char *buf)
{
	struct hapd_interfaces *interfaces = iface->interfaces;
	struct hostapd_data *hapd = NULL;
	struct hostapd_bss_config *bss = NULL;
	struct hostapd_config *newconf, *oldconf;
	char *conf_file = NULL;
	size_t j;

	wpa_printf(MSG_DEBUG,"Relaod config bss %s\n", iface_name);
	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (os_strcmp(hapd->conf->iface, iface_name) == 0)
			break;
	}

	if (!hapd) {
		wpa_printf(MSG_DEBUG,"%s does not exist\n", iface_name);
		return -1;
	}

	newconf = oldconf = hapd->iconf;

	if (os_strncmp(buf, "bss_config=", 11) == 0) {
		conf_file = buf + 11;
		if (!os_strlen(conf_file))
			return -1;

		newconf = interfaces->config_read_cb(conf_file);
		if (!newconf)
			return -1;
	}

	for (j = 0; j < newconf->num_bss; j++) {
		bss = newconf->bss[j];
		if (os_strcmp(bss->iface, iface_name) == 0)
			break;
	}

	if (!bss) {
		wpa_printf(MSG_ERROR,"%s does not exist in bss config \n", iface_name);
		return -1;
	}

	hostapd_clear_old_bss(hapd);
	hapd->iconf = newconf;
	hapd->iconf->channel = oldconf->channel;
	hapd->iconf->acs = oldconf->acs;
	hapd->iconf->secondary_channel = oldconf->secondary_channel;
	hapd->iconf->ieee80211n = oldconf->ieee80211n;
	hapd->iconf->ieee80211ac = oldconf->ieee80211ac;
	hapd->iconf->ht_capab = oldconf->ht_capab;
	hapd->iconf->vht_capab = oldconf->vht_capab;
	hostapd_set_oper_chwidth(hapd->iconf,
			hostapd_get_oper_chwidth(oldconf));
	hostapd_set_oper_centr_freq_seg0_idx(
			hapd->iconf,
			hostapd_get_oper_centr_freq_seg0_idx(oldconf));
	hostapd_set_oper_centr_freq_seg1_idx(
			hapd->iconf,
			hostapd_get_oper_centr_freq_seg1_idx(oldconf));
	hapd->conf = bss;
	hapd->iconf->bandwidth_device = oldconf->bandwidth_device;
	hapd->iconf->center_freq_device = oldconf->center_freq_device;
	hapd->reenable_beacon = 1;
	hostapd_reload_bss(hapd);
#ifdef CONFIG_QCN_EXTN
	hostapd_periodic_acs_start(iface);
#endif

	wpa_printf(MSG_DEBUG,"Relaod config bss %s completed\n", iface_name);

	return 0;
}


int hostapd_reload_config(struct hostapd_iface *iface)
{
	int ret, reload_err = 0;
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *link;
	size_t i, j;
#endif /* CONFIG_IEEE80211BE */

	if (!iface)
		return -1;

	ret = hostapd_reload_config_iface(&iface);
	if (ret)
		return ret;

	if (!iface || !iface->interfaces)
		return -1;

#ifdef CONFIG_IEEE80211BE
	for (i = 0; i < iface->interfaces->count; i++) {
		struct hostapd_iface *other = iface->interfaces->iface[i];
		bool mld_partner_found = false;

		if (other == iface || !other || !other->conf)
			continue;

		for (j = 0; j < iface->num_bss && !mld_partner_found; j++) {
			if (!iface->bss[j] || !iface->bss[j]->conf ||
			    !iface->bss[j]->conf->mld_ap)
				continue;

#ifdef CONFIG_QCN_EXTN
			if (hostapd_is_repurpose_disabled_11be_extn(iface->bss[j]->conf))
				continue;
#endif /* CONFIG_QCN_EXTN */

			for_each_mld_link(link, iface->bss[j]) {
				if (link->iface == other) {
					mld_partner_found = true;
					break;
				}
			}
		}

		if (!mld_partner_found)
			continue;

		ret = hostapd_reload_config_iface(&other);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Failed to reload MLO partner links");
			reload_err = ret;
		} else {
			wpa_printf(MSG_DEBUG, "Reloaded MLO partner links");
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return reload_err;
}

#ifdef CONFIG_WEP

static void hostapd_broadcast_key_clear_iface(struct hostapd_data *hapd,
					      const char *ifname)
{
	int i;

	if (!ifname || !hapd->drv_priv)
		return;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_NONE, NULL, i, 0,
					0, NULL, 0, NULL, 0, KEY_FLAG_GROUP)) {
			wpa_printf(MSG_DEBUG, "Failed to clear default "
				   "encryption keys (ifname=%s keyidx=%d)",
				   ifname, i);
		}
	}
	if (ap_pmf_enabled(hapd->conf)) {
		for (i = NUM_WEP_KEYS; i < NUM_WEP_KEYS + 2; i++) {
			if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_NONE,
						NULL, i, 0, 0, NULL,
						0, NULL, 0, KEY_FLAG_GROUP)) {
				wpa_printf(MSG_DEBUG, "Failed to clear "
					   "default mgmt encryption keys "
					   "(ifname=%s keyidx=%d)", ifname, i);
			}
		}
	}
}


static int hostapd_broadcast_wep_clear(struct hostapd_data *hapd)
{
	hostapd_broadcast_key_clear_iface(hapd, hapd->conf->iface);
	return 0;
}


static int hostapd_broadcast_wep_set(struct hostapd_data *hapd)
{
	int errors = 0, idx;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	idx = ssid->wep.idx;
	if (ssid->wep.default_len && ssid->wep.key[idx] &&
	    hostapd_drv_set_key(hapd->conf->iface,
				hapd, WPA_ALG_WEP, broadcast_ether_addr, idx, 0,
				1, NULL, 0, ssid->wep.key[idx],
				ssid->wep.len[idx],
				KEY_FLAG_GROUP_RX_TX_DEFAULT)) {
		wpa_printf(MSG_WARNING, "Could not set WEP encryption.");
		errors++;
	}

	return errors;
}

#endif /* CONFIG_WEP */


#ifdef CONFIG_IEEE80211BE

static void hostapd_link_remove_timeout_handler(void *eloop_data,
						void *user_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_data;

	if (hapd->eht_mld_link_removal_count == 0)
		return;
	hapd->eht_mld_link_removal_count--;

	wpa_printf(MSG_DEBUG, "MLD: Remove link_id=%u in %u beacons",
		   hapd->mld_link_id,
		   hapd->eht_mld_link_removal_count);

	ieee802_11_set_beacon(hapd);

	if (!hapd->eht_mld_link_removal_count) {
		hapd->eht_mld_link_removal_inprogress = false;
		hostapd_free_link_stas(hapd);
		hostapd_disable_iface(hapd->iface);
		return;
	}

	eloop_register_timeout(0, TU_TO_USEC(hapd->iconf->beacon_int),
			       hostapd_link_remove_timeout_handler,
			       hapd, NULL);
}

static bool hostapd_ttlm_info_check_non_default_mappings(int link_id,
							 struct ttlm_info *ttlm_info,
							 int ttlm_info_size)
{
	int dir, tid;

	for (dir = 0; dir < ttlm_info_size; dir++) {
		struct ttlm_info *info = &ttlm_info[dir];

		if (info->direction == TTLM_DIRECTION_INVALID)
			continue;
		if (info->default_link_mapping)
			continue;
		for (tid = 0; tid < NUM_MAX_TIDS; tid++)
			if (info->ieee_link_map_tid[tid] & BIT(link_id))
				return true;
	}
	return false;
}

static bool hostapd_validate_link_removal_ttlm_global(struct hostapd_data *hapd)
{
	if (!hapd->conf->mld_ap || !hapd->mld)
		return true;

	/* check the established mapping */
	if (hostapd_ttlm_info_check_non_default_mappings(hapd->mld_link_id,
							 &hapd->mld->ttlm_ctx.established_ttlm.ttlm,
							 1)) {
		return false;
	}

	/* check the upcoming mapping */
	if (hostapd_ttlm_info_check_non_default_mappings(hapd->mld_link_id,
							 &hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm,
							 1)) {
		return false;
	}

	return true;
}

static bool hostapd_validate_link_removal_ttlm(struct hostapd_data *hapd)
{
	if (hapd->iface->conf->mbssid != MBSSID_DISABLED) {
		struct hostapd_data *tx_bss = hostapd_mbssid_get_tx_bss(hapd);
		struct hostapd_data *bss;
		int i;

		if (hapd != tx_bss)
			goto check_hapd;

		if (hapd->iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

			if (!group)
				goto check_hapd;

			dl_list_for_each(bss, &group->bss_list, struct hostapd_data, mbssid_bss) {
				if (bss == hapd)
					continue;
				if (!bss->conf->mld_ap || !bss->mld || bss->disabled ||
				    !bss->beacon_set_done)
					continue;
#ifdef CONFIG_QCN_EXTN
				if (hostapd_is_repurpose_disabled_11be_extn(bss->conf))
					continue;
#endif /* CONFIG_QCN_EXTN */

				/* even if one non-tx bss fails validation, return false */
				if (!hostapd_validate_link_removal_ttlm_global(bss))
					return false;

				/* check for negotiated mapping as well */
			}
		} else {
			for (i = 1; i < hapd->iface->num_bss; i++) {
				bss = hapd->iface->bss[i];
				if (!bss->conf->mld_ap || !bss->mld || bss->disabled ||
				    !bss->beacon_set_done)
					continue;
#ifdef CONFIG_QCN_EXTN
				if (hostapd_is_repurpose_disabled_11be_extn(bss->conf))
					continue;
#endif /* CONFIG_QCN_EXTN */
				if (!hostapd_validate_link_removal_ttlm_global(bss))
					return false;

				/* check for negotited mapping as well */
			}
		}
	}
	/* If we are here means, all non-tx bss allows this link to be removed, now validate tx-bss
	 * OR the hapd is not same as tx_bss, hence validate for hapd,
	 * OR the hapd is non-mbssid bss, hence validate for hapd
	 */
check_hapd:
	if (!hostapd_validate_link_removal_ttlm_global(hapd))
		return false;
	/* check for negotiated mapping as well */
	return true;
}

static int hostapd_send_ml_reconfig_link_removal(struct hostapd_data *hapd,
						 u32 count)
{
	struct driver_reconfig_link_removal_params params;
	int ret;

	params.link_id = hapd->mld_link_id;
	params.removal_count = count;

	params.ml_reconfig_elem_len = hostapd_eid_eht_ml_reconfig_len(hapd);
	params.ml_reconfig_elem = os_zalloc(params.ml_reconfig_elem_len);

	if (!params.ml_reconfig_elem)
		return -1;

	/* check if the link being removed is currently enabled in ttlm,
	 * if so deny the link removal.
	 * This API considers MBSSID tx vap case as well
	 */
	if (hapd->conf->ttlm_enable &&
	    !hostapd_validate_link_removal_ttlm(hapd)) {
		hapd->eht_mld_link_removal_inprogress = false;
		os_free(params.ml_reconfig_elem);
		return -1;
	}

	hostapd_eid_eht_reconf_ml(hapd, params.ml_reconfig_elem);

	/*send NL with tbtt count and ml reconfig ie */
	ret = hostapd_drv_ml_reconfig_link_remove(hapd, WPA_IF_AP_BSS, &params);

	os_free(params.ml_reconfig_elem);

	return ret;
}


static bool is_link_reconfigure_allowed(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_data *link_bss, *bss;
	size_t i;
	u8 list_len;
#ifdef CONFIG_QCN_EXTN
	u8 num_repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->mld || !hapd->mld->num_links) {
		wpa_printf(MSG_ERROR, "mld_ap is NOT set\n");
		return false;
	}

	if (hapd->disabled || !hapd->beacon_set_done) {
		wpa_printf(MSG_ERROR, "AP MLD is already disabled or stopped\n");
		return false;
	}

#ifdef CONFIG_QCN_EXTN
	num_repurposed_links =
		hostapd_get_repurposed_links_bitmap_extn(hapd, NULL);
#endif /* CONFIG_QCN_EXTN */

	list_len = dl_list_len(&mld->links);
	if (!list_len || list_len == 1) {
		wpa_printf(MSG_INFO,
			   "link reconfigure is currently not applicable for this mld links:%u\n",
			   list_len);
		return false;
	}

#ifdef CONFIG_QCN_EXTN
	/* Above check ensures overall num links under mld. Does not consider if
	 * mld has repurposed links. Do not allow reconfig on this link if it is
	 * the only ML enabled/non-repurposed link under the MLD
	 */
	if ((list_len - num_repurposed_links) == 1) {
		wpa_printf(MSG_INFO,
			   "Do not allow reconfig as this is the only non-repurposed link under MLD");
		return false;
	}
#endif /* CONFIG_QCN_EXTN */

	if (iface->conf->mbssid != MBSSID_DISABLED &&
	    hapd == hostapd_mbssid_get_tx_bss(hapd)) {
		if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

			dl_list_for_each(bss, &group->bss_list,
					 struct hostapd_data, mbssid_bss) {
				if (bss == hapd)
					continue;
				if (!bss->conf->mld_ap || bss->disabled ||
				    !bss->beacon_set_done)
					continue;
				mld = bss->mld;

				list_len = dl_list_len(&mld->links);
				if (list_len <= 1) {
					wpa_printf(MSG_INFO, "link reconfigure is currently not applicable for this list:%u\n", list_len);
					return false;
				}
#ifdef CONFIG_QCN_EXTN
				/* Each VAP in the MBSSID group is either
				 * repurposed or not repurposed. So, no need of
				 * explicit repurpose state check on non-tx vap.
				 * Ensure, the non-tx vap is not the only 11be
				 * or non-repurposed link its MLD.
				 */
				num_repurposed_links =
					hostapd_get_repurposed_links_bitmap_extn
						(bss, NULL);

				if ((list_len - num_repurposed_links) <= 1) {
					wpa_printf(MSG_INFO,
						   "link reconfigure is not allowed on non tx vap as thats the only non-repurposed link");
					return false;
				}
#endif /* CONFIG_QCN_EXTN */
			}
		} else {
			for (i = 1; i < hapd->iface->num_bss; i++) {
				bss = hapd->iface->bss[i];
				if (!bss->conf->mld_ap || bss->disabled ||
				    !bss->beacon_set_done)
					continue;
				mld = bss->mld;

				list_len = dl_list_len(&mld->links);
				if (list_len <= 1) {
					wpa_printf(MSG_INFO, "link reconfigure is currently not applicable for this list:%u\n", list_len);
					return false;
				}
#ifdef CONFIG_QCN_EXTN
				num_repurposed_links =
					hostapd_get_repurposed_links_bitmap_extn
						(bss, NULL);

				if ((list_len - num_repurposed_links) <= 1) {
					wpa_printf(MSG_INFO,
						   "link reconfigure is not allowed on non tx vap as thats the only non-repurposed link");
					return false;
				}
#endif /* CONFIG_QCN_EXTN */
			}
		}
	}

	for_each_mld_link(link_bss, hapd) {
		if (link_bss == hapd)
			continue;

		/* Currently we support removing only one link
		 * at a time from a MLD
		 */
		if (link_bss->eht_mld_link_removal_inprogress) {
			wpa_printf(MSG_INFO, "Rejecting this request as Link reconfigure is already in-progress for" MACSTR,
				  MAC2STR(link_bss->own_addr));
			return false;
		}
	}

	return true;
}


int hostapd_link_remove(struct hostapd_data *hapd, u32 count,
			enum link_reconfigure_type removal_type)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t i;

	if (!hapd->conf->mld_ap)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "MLD: Remove link_id=%u in %u beacons",
		   hapd->mld_link_id, count);

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		goto non_repurpose_link_remove;

	/* disable_bss loops non-tx BSSes from the ctrl iface handler, hence
	 * skip looping here */
	if (removal_type == HAPD_LINK_REMOVAL &&
	    iface->conf->mbssid != MBSSID_DISABLED &&
	    hapd == hostapd_mbssid_get_tx_bss(hapd)) {
		if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			struct hostapd_data *bss;
			struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

			dl_list_for_each(bss, &group->bss_list,
					 struct hostapd_data, mbssid_bss) {
				if (bss != hapd) {
					if (hostapd_link_remove_repurposed_bss_extn
							(bss, removal_type)) {
						wpa_printf(MSG_ERROR,
							   "Failed to remove repurposed non-tx BSS");
						return -EINVAL;
					}
				}
			}
		} else {
			for (i = 1; i < hapd->iface->num_bss; i++) {
				struct hostapd_data *bss = hapd->iface->bss[i];

				if (hostapd_link_remove_repurposed_bss_extn
						(bss, removal_type)) {
					wpa_printf(MSG_ERROR,
						   "Failed to remove repurposed non-tx bss");
					return -EINVAL;
				}
			}
		}
	}

	return hostapd_link_remove_repurposed_bss_extn(hapd, removal_type);

non_repurpose_link_remove:
#endif /* CONFIG_QCN_EXTN */

	hapd->eht_mld_link_removal_count = count;

	if (iface->drv_flags2 & WPA_DRIVER_FLAG2_MLD_LINK_REMOVAL_OFFLOAD) {
	    if (!is_link_reconfigure_allowed(hapd)) {
		    hapd->eht_mld_link_removal_count = 0;
		    wpa_printf(MSG_INFO, "link reconfigure is currently not applicable\n");
		    return -1;
	    }
	    hapd->removal_type = removal_type;

	    /* Check if the link removal is scheduled for tx BSS
	     * If yes, schedule link removal for all non-tx BSS first
	     */
	    if (iface->conf->mbssid != MBSSID_DISABLED &&
		hapd == hostapd_mbssid_get_tx_bss(hapd)) {
		    if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			    size_t i = 0;
			    struct hostapd_data *bss;
			    struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

			    dl_list_for_each(bss, &group->bss_list,
					     struct hostapd_data, mbssid_bss) {
				    if (bss != hapd) {
					    if (!bss->conf->mld_ap || bss->disabled ||
						bss->eht_mld_link_removal_inprogress ||
						!bss->beacon_set_done)
						    continue;
					    bss->eht_mld_link_removal_inprogress = true;
					    bss->eht_mld_link_removal_count = count;
					    bss->removal_type = removal_type;
					    if (hostapd_send_ml_reconfig_link_removal(bss, count)) {
						    wpa_printf(MSG_DEBUG,
							       "Failed to send link removal non-tx BSS");
						    return -EINVAL;
					    }
				    }
				    i++;
			    }
		    } else {
			    for (i = 1; i < hapd->iface->num_bss; i++) {
				    struct hostapd_data *bss = hapd->iface->bss[i];

				    if (!bss->conf->mld_ap || bss->disabled ||
					bss->eht_mld_link_removal_inprogress ||
					!bss->beacon_set_done)
					    continue;
				    bss->eht_mld_link_removal_inprogress = true;
				    bss->eht_mld_link_removal_count = count;
				    bss->removal_type = removal_type;
				    if (hostapd_send_ml_reconfig_link_removal(bss, count)) {
					    wpa_printf(MSG_DEBUG,
						       "Failed to send link removal non-tx BSS");
					    return -EINVAL;
				    }
			    }
		    }
	    }

	    hapd->eht_mld_link_removal_inprogress = true;
	    return hostapd_send_ml_reconfig_link_removal(hapd, count);
	}

	hapd->eht_mld_link_removal_inprogress = true;
	eloop_register_timeout(0, TU_TO_USEC(hapd->iconf->beacon_int),
			       hostapd_link_remove_timeout_handler,
			       hapd, NULL);

	ieee802_11_set_beacon(hapd);
	return 0;
}

#endif /* CONFIG_IEEE80211BE */


void hostapd_free_hapd_data(struct hostapd_data *hapd)
{
	const bool skip_unstarted_bss_cleanup = !hapd->started;

	hostapd_clear_local_tpe_bss(hapd);

	os_free(hapd->probereq_cb);
	hapd->probereq_cb = NULL;
	hapd->num_probereq_cb = 0;

	os_free(hapd->current_rates);
	hapd->current_rates = NULL;
	os_free(hapd->basic_rates);
	hapd->basic_rates = NULL;

#ifdef CONFIG_P2P
	wpabuf_free(hapd->p2p_beacon_ie);
	hapd->p2p_beacon_ie = NULL;
	wpabuf_free(hapd->p2p_probe_resp_ie);
	hapd->p2p_probe_resp_ie = NULL;
#endif /* CONFIG_P2P */

	/* Skip unstarted cleanup unless reenable/deinit state requires it. */
	if (!hapd->started && hapd->reenable == REENABLE_NONE)
		return;

	if (!hapd->started &&
	    hapd->reenable == REENABLE_DEINIT)
		goto remove_if;

	hapd->started = 0;
	hapd->beacon_set_done = 0;

#ifdef CONFIG_MQTT
	/*
	 * Teardown the global MQTT connection when the primary BSS (first BSS
	 * of the first interface) is freed.  This is the last teardown point
	 * where hapd_interfaces is still valid and the connection is no longer
	 * needed by any remaining BSS.
	 */
	if (hapd->iface && hapd->iface->interfaces &&
	    hapd->iface->interfaces->count > 0 &&
	    hapd->iface->interfaces->iface[0] == hapd->iface &&
	    hapd->iface->num_bss > 0 &&
	    hapd->iface->bss[0] == hapd) {
		hostapd_mqtt_deinit(hapd->iface->interfaces);
	}
#endif /* CONFIG_MQTT */

	wpa_printf(MSG_DEBUG, "%s(%s)", __func__, hapd->conf->iface);
#ifdef CONFIG_IEEE80211BN
	if (hapd->eth_p_1905_ctx) {
		smd_neighbor_update_deinit(hapd);
		wpa_printf(MSG_DEBUG, "SMD: Deinitializing roaming transport");
		if (hostapd_mld_is_first_bss(hapd))
			eth_p_1905_deinit(hapd->eth_p_1905_ctx);
		hapd->eth_p_1905_ctx = NULL;
	}
#endif /* CONFIG_IEEE80211BN */
#ifdef CONFIG_QCN_EXTN
	hostapd_log_extn_deinit(hapd);
#endif /* CONFIG_QCN_EXTN */
	hostapd_ucode_free_bss(hapd);
	hostapd_ubus_free_bss(hapd);
	accounting_deinit(hapd);
	hostapd_deinit_wpa(hapd);

	vlan_cancel_cleanup_for_hapd(hapd);
	vlan_deinit(hapd);
	hostapd_acl_deinit(hapd);
#ifndef CONFIG_NO_RADIUS
	radius_client_deinit(hapd->radius);
	radius_das_deinit(hapd->radius_das);
	hapd->radius = NULL;
	hapd->radius_das = NULL;
#endif /* CONFIG_NO_RADIUS */

	hostapd_deinit_wps(hapd);
	ieee802_1x_dealloc_kay_sm_hapd(hapd);
#ifdef CONFIG_DPP
	hostapd_dpp_deinit(hapd);
	gas_query_ap_deinit(hapd->gas);
	hapd->gas = NULL;
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
	hostapd_nan_usd_deinit(hapd);
#endif /* CONFIG_NAN_USD */

	authsrv_deinit(hapd);

remove_if:
	/* For single drv, first bss would have interface_added flag set.
	 * Don't remove interface now. Driver deinit part will take care
	 */
	if (hapd->reenable != REENABLE_REUSE_LINK &&
	    hapd->interface_added &&
	    hapd->iface->bss[0] != hapd &&
	    hapd->drv_priv != hapd->iface->bss[0]->drv_priv) {
		hapd->interface_added = 0;
		if (hostapd_if_remove(hapd, WPA_IF_AP_BSS, hapd->conf->iface)) {
			wpa_printf(MSG_WARNING,
				   "Failed to remove BSS interface %s",
				   hapd->conf->iface);
			hapd->interface_added = 1;
		} else {
			/*
			 * Since this was a dynamically added interface, the
			 * driver wrapper may have removed its internal instance
			 * and hapd->drv_priv is not valid anymore.
			 */
			hapd->drv_priv = NULL;
		}
	}

#ifdef CONFIG_IEEE80211BE
	/* If the interface was not added as well as it is not the first BSS,
	 * at least the link should be removed here since deinit will take care
	 * of only the first BSS. */
	if (hapd->reenable != REENABLE_REUSE_LINK && hapd->conf &&
	    hapd->conf->mld_ap &&
	    !hapd->interface_added && hapd->iface->bss[0] != hapd &&
	    hapd->drv_priv != hapd->iface->bss[0]->drv_priv) {
		if (hostapd_if_link_remove(hapd, WPA_IF_AP_BSS,
					   hapd->conf->iface,
					   hapd->mld_link_id))
			wpa_printf(MSG_WARNING,
				   "Failed to remove link BSS interface %s",
				   hapd->conf->iface);
	}
#endif /* CONFIG_IEEE80211BE */

	if (skip_unstarted_bss_cleanup)
		return;

	wpabuf_free(hapd->time_adv);
	hapd->time_adv = NULL;

#if defined(CONFIG_INTERWORKING) || defined(CONFIG_DPP)
	gas_serv_deinit(hapd);
#endif /* CONFIG_INTERWORKING || CONFIG_DPP */

	bss_load_update_deinit(hapd);
	ndisc_snoop_deinit(hapd);
	dhcp_snoop_deinit(hapd);
	x_snoop_deinit(hapd);

#ifdef CONFIG_SQLITE
	bin_clear_free(hapd->tmp_eap_user.identity,
		       hapd->tmp_eap_user.identity_len);
	bin_clear_free(hapd->tmp_eap_user.password,
		       hapd->tmp_eap_user.password_len);
	os_memset(&hapd->tmp_eap_user, 0, sizeof(hapd->tmp_eap_user));
#endif /* CONFIG_SQLITE */

#ifdef CONFIG_MESH
	wpabuf_free(hapd->mesh_pending_auth);
	hapd->mesh_pending_auth = NULL;
	/* handling setup failure is already done */
	hapd->setup_complete_cb = NULL;
#endif /* CONFIG_MESH */

#ifndef CONFIG_NO_RRM
	hostapd_clean_rrm(hapd);
#endif /* CONFIG_NO_RRM */
	fils_hlp_deinit(hapd);

#ifdef CONFIG_OCV
	eloop_cancel_timeout(hostapd_ocv_check_csa_sa_query, hapd, NULL);
#endif /* CONFIG_OCV */

#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211AC)
	eloop_cancel_timeout(hostapd_mu_cap_war_kickout_timer_extn,
			     hapd, NULL);
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211AC */
#ifdef CONFIG_SAE
	{
		struct hostapd_sae_commit_queue *q;

		while ((q = dl_list_first(&hapd->sae_commit_queue,
					  struct hostapd_sae_commit_queue,
					  list))) {
			dl_list_del(&q->list);
			os_free(q);
		}
	}
	eloop_cancel_timeout(auth_sae_process_commit, hapd, NULL);
#endif /* CONFIG_SAE */

#ifdef CONFIG_IEEE80211AX
	eloop_cancel_timeout(hostapd_switch_color_timeout_handler, hapd, NULL);
#ifdef CONFIG_IEEE80211BE
	eloop_cancel_timeout(hostapd_link_remove_timeout_handler, hapd, NULL);
	hapd->eht_mld_link_removal_inprogress = false;
	hapd->eht_mld_link_removal_count = 0;
#endif /* CONFIG_IEEE80211BE */

#endif /* CONFIG_IEEE80211AX */
}


#ifdef CONFIG_IEEE80211BE
/* hostapd_mld_move_vlan_list - Move the VLAN list to the new first bss
 *
 * This function is used to copy the reference to the VLAN list from the old
 * first BSS to the new first BSS when the first BSS is removed.
 */
static void hostapd_mld_move_vlan_list(struct hostapd_data *old_fbss,
				       struct hostapd_data *new_fbss)
{
	new_fbss->conf->vlan = old_fbss->conf->vlan;
	old_fbss->conf->vlan = NULL;

}
#endif /* CONFIG_IEEE80211BE */

/* hostapd_bss_link_deinit - Per-BSS ML cleanup (deinitialization)
 * @hapd: Pointer to BSS data
 *
 * This function is used to unlink the BSS from the AP MLD.
 * If the BSS being removed is the first link, the next link becomes the first
 * link.
 */
void hostapd_bss_link_deinit(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	int i;

	if (!hapd->conf || !hapd->conf->mld_ap)
		return;

	/* Free per STA profiles */
	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		os_free(hapd->partner_links[i].resp_sta_profile);
		os_memset(&hapd->partner_links[i], 0,
			  sizeof(hapd->partner_links[i]));
	}

	/* Put all freeing logic above this */
	if (!hapd->mld || hapd_reenable_pending(hapd))
		return;

	/* The first BSS can also be only linked when at least driver_init() is
	 * executed. But if previous interface fails, it is not, and hence,
	 * safe to skip.
	 */
	if (hapd->iface->bss[0] == hapd && !hapd->drv_priv)
		return;

	/*
	 * hapd->mld being set here means hostapd_bss_setup_multi_link()
	 * already allocated a link ID for this BSS (it clears hapd->mld
	 * on allocation failure). That ID must always be released, even
	 * if this BSS was never started or never linked into the MLD's
	 * list -- hostapd_mld_remove_link() itself handles that case and
	 * frees the ID accordingly.
	 */
	hostapd_mld_remove_link(hapd);
#endif /* CONFIG_IEEE80211BE */
}


/**
 * hostapd_cleanup - Per-BSS cleanup (deinitialization)
 * @hapd: Pointer to BSS data
 *
 * This function is used to free all per-BSS data structures and resources.
 * Most of the modules that are initialized in hostapd_setup_bss() are
 * deinitialized here.
 */
static void hostapd_cleanup(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "%s(hapd=%p (%s))", __func__, hapd,
		   hapd->conf ? hapd->conf->iface : "N/A");
	if (hapd->iface->interfaces &&
	    hapd->iface->interfaces->ctrl_iface_deinit) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPA_EVENT_TERMINATING);
		hapd->iface->interfaces->ctrl_iface_deinit(hapd);
	}
#if defined(CONFIG_HOSTAPD_IF) && defined(CONFIG_QCN_EXTN)
	hostapd_if_interface_remove(hapd);
#endif

#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211AC)
	hostapd_mu_cap_war_sta_list_flush_extn(hapd);
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211BN
	mapc_deinit(hapd);
#endif /* CONFIG_IEEE80211BN */
	hostapd_free_hapd_data(hapd);
}


static void sta_track_deinit(struct hostapd_iface *iface)
{
	struct hostapd_sta_info *info;

	if (!iface->num_sta_seen)
		return;

	while ((info = dl_list_first(&iface->sta_seen, struct hostapd_sta_info,
				     list))) {
		dl_list_del(&info->list);
		iface->num_sta_seen--;
		sta_track_del(info);
	}
}


void hostapd_cleanup_iface_partial(struct hostapd_iface *iface)
{
	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	eloop_cancel_timeout(channel_list_update_timeout, iface, NULL);
	hostapd_cancel_agile_cac_restart(iface);
#ifdef NEED_AP_MLME
	hostapd_stop_setup_timers(iface);
	/* OCE 4.3.1/4.3.2: cancel periodic survey timer and clear scan_cb */
	hostapd_oce_survey_timer_cancel(iface);
#endif /* NEED_AP_MLME */
	if (iface->current_mode)
		acs_cleanup(iface);
	hostapd_free_hw_features(iface->hw_features, iface->num_hw_features);
	iface->hw_features = NULL;
	iface->num_hw_features = 0;
	iface->current_mode = NULL;
	iface->cac_started = 0;
	iface->cac_type = 0;
#ifdef CONFIG_QCN_EXTN
	iface->iface_extn.cac_abort = 0;
	iface->bootup_cac_in_progress = 0;
#endif
	ap_list_deinit(iface);
	sta_track_deinit(iface);
	airtime_policy_update_deinit(iface);

#ifdef CONFIG_ATF_OFFLOAD
	atf_timer_stop(iface);
#endif

	hostapd_free_multi_hw_info(iface->multi_hw_info);
	iface->multi_hw_info = NULL;
	iface->num_multi_hws = 0;
	iface->current_hw_info = NULL;
	iface->csa_pending_on_cac_abort = false;
	os_memset(&iface->csa_settings, 0, sizeof(struct csa_settings));
	os_memset(&iface->radar_background, 0, sizeof(iface->radar_background));
	/*
	 * channel == -1 is the sentinel the DFS background-radar code checks
	 * for "no cached channel"; os_memset() above leaves it 0, which is a
	 * channel number, not the sentinel.
	 */
	iface->radar_background.channel = -1;
#ifdef CONFIG_QCN_EXTN
	hostapd_iface_deinit_extn(iface);
#endif /* CONFIG_QCN_EXTN */
}


/**
 * hostapd_cleanup_iface - Complete per-interface cleanup
 * @iface: Pointer to interface data
 *
 * This function is called after per-BSS data structures are deinitialized
 * with hostapd_cleanup().
 */
static void hostapd_cleanup_iface(struct hostapd_iface *iface)
{
	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	hostapd_ucode_free_iface(iface);
	eloop_cancel_timeout(hostapd_interface_setup_failure_handler, iface,
			     NULL);

	hostapd_cleanup_iface_partial(iface);

	atf_deinit_algo(iface);

	hostapd_config_free(iface->conf);
	iface->conf = NULL;

	os_free(iface->config_fname);
	os_free(iface->bss);
	wpa_printf(MSG_DEBUG, "%s: free iface=%p", __func__, iface);
	os_free(iface);
}


#ifdef CONFIG_WEP

static void hostapd_clear_wep(struct hostapd_data *hapd)
{
	if (hapd->drv_priv && !hapd->iface->driver_ap_teardown && hapd->conf) {
		hostapd_set_privacy(hapd, 0);
		hostapd_broadcast_wep_clear(hapd);
	}
}


static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd)
{
	int i;

	hostapd_broadcast_wep_set(hapd);

	if (hapd->conf->ssid.wep.default_len) {
		hostapd_set_privacy(hapd, 1);
		return 0;
	}

	/*
	 * When IEEE 802.1X is not enabled, the driver may need to know how to
	 * set authentication algorithms for static WEP.
	 */
	hostapd_drv_set_authmode(hapd, hapd->conf->auth_algs);

	for (i = 0; i < 4; i++) {
		if (hapd->conf->ssid.wep.key[i] &&
		    hostapd_drv_set_key(iface, hapd, WPA_ALG_WEP, NULL, i, 0,
					i == hapd->conf->ssid.wep.idx, NULL, 0,
					hapd->conf->ssid.wep.key[i],
					hapd->conf->ssid.wep.len[i],
					i == hapd->conf->ssid.wep.idx ?
					KEY_FLAG_GROUP_RX_TX_DEFAULT :
					KEY_FLAG_GROUP_RX_TX)) {
			wpa_printf(MSG_WARNING, "Could not set WEP "
				   "encryption.");
			return -1;
		}
		if (hapd->conf->ssid.wep.key[i] &&
		    i == hapd->conf->ssid.wep.idx)
			hostapd_set_privacy(hapd, 1);
	}

	return 0;
}

#endif /* CONFIG_WEP */


static int hostapd_flush_old_stations(struct hostapd_data *hapd, u16 reason)
{
	int ret = 0;
	u8 addr[ETH_ALEN];

	if (hostapd_drv_none(hapd) || hapd->drv_priv == NULL) {
		hostapd_free_stas(hapd);
		return 0;
	}

	if (!hapd->iface->driver_ap_teardown) {
		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Flushing old station entries");

		if (hostapd_flush(hapd)) {
			wpa_msg(hapd->msg_ctx, MSG_WARNING,
				"Could not connect to kernel driver");
			ret = -1;
		}
	}
	if (hapd->conf && hapd->conf->broadcast_deauth) {
		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Deauthenticate all stations");
		os_memset(addr, 0xff, ETH_ALEN);
		hostapd_drv_sta_deauth(hapd, addr, reason);
	}
	hostapd_free_stas(hapd);

	return ret;
}


void hostapd_bss_deinit_no_free(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	ap_for_each_sta(hapd, hostapd_free_partner_link_stas, NULL);
#endif /* CONFIG_IEEE80211BE */
	/*
	 * Keep STA state intact until flush_old_stations sends broadcast
	 * deauth so PMF clients can process AP-driven disconnect before keys
	 * are removed.
	 */
	hostapd_flush_old_stations(hapd, WLAN_REASON_DEAUTH_LEAVING);
#ifdef CONFIG_WEP
	hostapd_clear_wep(hapd);
#endif /* CONFIG_WEP */

#ifdef CONFIG_QCN_EXTN
	hostapd_free_bss_index_extn(hapd);
#endif /* CONFIG_QCN_EXTN */
}


/**
 * hostapd_validate_bssid_configuration - Validate BSSID configuration
 * @iface: Pointer to interface data
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to validate that the configured BSSIDs are valid.
 */
static int hostapd_validate_bssid_configuration(struct hostapd_iface *iface)
{
	u8 mask[ETH_ALEN] = { 0 };
	struct hostapd_data *hapd = iface->bss[0];
	unsigned int i = iface->conf->num_bss, bits = 0, j;
	int auto_addr = 0;

	if (hostapd_drv_none(hapd))
		return 0;

	if (iface->conf->use_driver_iface_addr)
		return 0;

#ifdef CONFIG_QCN_EXTN
	if (iface->conf->use_driver_vendor_addr)
		return 0;
#endif /* CONFIG_QCN_EXTN */

	/* Generate BSSID mask that is large enough to cover the BSSIDs. */

	/* Determine the bits necessary to cover the number of BSSIDs. */
	for (i--; i; i >>= 1)
		bits++;

	/* Determine the bits necessary to any configured BSSIDs,
	   if they are higher than the number of BSSIDs. */
	for (j = 0; j < iface->conf->num_bss; j++) {
		if (is_zero_ether_addr(iface->conf->bss[j]->bssid)) {
			if (j)
				auto_addr++;
			continue;
		}

		for (i = 0; i < ETH_ALEN; i++) {
			mask[i] |=
				iface->conf->bss[j]->bssid[i] ^
				hapd->own_addr[i];
		}
	}

	if (!auto_addr)
		goto skip_mask_ext;

	for (i = 0; i < ETH_ALEN && mask[i] == 0; i++)
		;
	j = 0;
	if (i < ETH_ALEN) {
		j = (5 - i) * 8;

		while (mask[i] != 0) {
			mask[i] >>= 1;
			j++;
		}
	}

	if (bits < j)
		bits = j;

	if (bits > 40) {
		wpa_printf(MSG_ERROR, "Too many bits in the BSSID mask (%u)",
			   bits);
		return -1;
	}

	os_memset(mask, 0xff, ETH_ALEN);
	j = bits / 8;
	for (i = 5; i > 5 - j; i--)
		mask[i] = 0;
	j = bits % 8;
	while (j) {
		j--;
		mask[i] <<= 1;
	}

skip_mask_ext:
	wpa_printf(MSG_DEBUG, "BSS count %lu, BSSID mask " MACSTR " (%d bits)",
		   (unsigned long) iface->conf->num_bss, MAC2STR(mask), bits);

	if (!auto_addr)
		return 0;

	for (i = 0; i < ETH_ALEN; i++) {
		if ((hapd->own_addr[i] & mask[i]) != hapd->own_addr[i]) {
			wpa_printf(MSG_ERROR, "Invalid BSSID mask " MACSTR
				   " for start address " MACSTR ".",
				   MAC2STR(mask), MAC2STR(hapd->own_addr));
			wpa_printf(MSG_ERROR, "Start address must be the "
				   "first address in the block (i.e., addr "
				   "AND mask == addr).");
			return -1;
		}
	}

	return 0;
}


static int mac_in_conf(struct hostapd_config *conf, const void *a)
{
	size_t i;

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_mac_comp(conf->bss[i]->bssid, a) == 0) {
			return 1;
		}
	}

	return 0;
}


#ifndef CONFIG_NO_RADIUS

static int hostapd_das_nas_mismatch(struct hostapd_data *hapd,
				    struct radius_das_attrs *attr)
{
	if (attr->nas_identifier &&
	    (!hapd->conf->nas_identifier ||
	     os_strlen(hapd->conf->nas_identifier) !=
	     attr->nas_identifier_len ||
	     os_memcmp(hapd->conf->nas_identifier, attr->nas_identifier,
		       attr->nas_identifier_len) != 0)) {
		wpa_printf(MSG_DEBUG, "RADIUS DAS: NAS-Identifier mismatch");
		return 1;
	}

	if (attr->nas_ip_addr &&
	    (hapd->conf->own_ip_addr.af != AF_INET ||
	     os_memcmp(&hapd->conf->own_ip_addr.u.v4, attr->nas_ip_addr, 4) !=
	     0)) {
		wpa_printf(MSG_DEBUG, "RADIUS DAS: NAS-IP-Address mismatch");
		return 1;
	}

#ifdef CONFIG_IPV6
	if (attr->nas_ipv6_addr &&
	    (hapd->conf->own_ip_addr.af != AF_INET6 ||
	     os_memcmp(&hapd->conf->own_ip_addr.u.v6, attr->nas_ipv6_addr, 16)
	     != 0)) {
		wpa_printf(MSG_DEBUG, "RADIUS DAS: NAS-IPv6-Address mismatch");
		return 1;
	}
#endif /* CONFIG_IPV6 */

	return 0;
}


static struct sta_info * hostapd_das_find_sta(struct hostapd_data *hapd,
					      struct radius_das_attrs *attr,
					      int *multi)
{
	struct sta_info *selected, *sta;
	char buf[128];
	int num_attr = 0;
	int count;

	*multi = 0;

	for (sta = hapd->sta_list; sta; sta = sta->next)
		sta->radius_das_match = 1;

	if (attr->sta_addr) {
		num_attr++;
		sta = ap_get_sta(hapd, attr->sta_addr);
		if (!sta) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No Calling-Station-Id match");
			return NULL;
		}

		selected = sta;
		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (sta != selected)
				sta->radius_das_match = 0;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: Calling-Station-Id match");
	}

	if (attr->acct_session_id) {
		num_attr++;
		if (attr->acct_session_id_len != 16) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Acct-Session-Id cannot match");
			return NULL;
		}
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (!sta->radius_das_match)
				continue;
			os_snprintf(buf, sizeof(buf), "%016llX",
				    (unsigned long long) sta->acct_session_id);
			if (os_memcmp(attr->acct_session_id, buf, 16) != 0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after Acct-Session-Id check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: Acct-Session-Id match");
	}

	if (attr->acct_multi_session_id) {
		num_attr++;
		if (attr->acct_multi_session_id_len != 16) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Acct-Multi-Session-Id cannot match");
			return NULL;
		}
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (!sta->radius_das_match)
				continue;
			if (!sta->eapol_sm ||
			    !sta->eapol_sm->acct_multi_session_id) {
				sta->radius_das_match = 0;
				continue;
			}
			os_snprintf(buf, sizeof(buf), "%016llX",
				    (unsigned long long)
				    sta->eapol_sm->acct_multi_session_id);
			if (os_memcmp(attr->acct_multi_session_id, buf, 16) !=
			    0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after Acct-Multi-Session-Id check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: Acct-Multi-Session-Id match");
	}

	if (attr->cui) {
		num_attr++;
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			struct wpabuf *cui;

			if (!sta->radius_das_match)
				continue;
			cui = ieee802_1x_get_radius_cui(sta->eapol_sm);
			if (!cui || wpabuf_len(cui) != attr->cui_len ||
			    os_memcmp(wpabuf_head(cui), attr->cui,
				      attr->cui_len) != 0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after Chargeable-User-Identity check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: Chargeable-User-Identity match");
	}

	if (attr->user_name) {
		num_attr++;
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			u8 *identity;
			size_t identity_len;

			if (!sta->radius_das_match)
				continue;
			identity = ieee802_1x_get_identity(sta->eapol_sm,
							   &identity_len);
			if (!identity ||
			    identity_len != attr->user_name_len ||
			    os_memcmp(identity, attr->user_name, identity_len)
			    != 0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after User-Name check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: User-Name match");
	}

	if (num_attr == 0) {
		/*
		 * In theory, we could match all current associations, but it
		 * seems safer to just reject requests that do not include any
		 * session identification attributes.
		 */
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: No session identification attributes included");
		return NULL;
	}

	selected = NULL;
	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (sta->radius_das_match) {
			if (selected) {
				*multi = 1;
				return NULL;
			}
			selected = sta;
		}
	}

	return selected;
}


static int hostapd_das_disconnect_pmksa(struct hostapd_data *hapd,
					struct radius_das_attrs *attr)
{
	if (!hapd->wpa_auth)
		return -1;
	return wpa_auth_radius_das_disconnect_pmksa(hapd->wpa_auth, attr);
}


static enum radius_das_res
hostapd_das_disconnect(void *ctx, struct radius_das_attrs *attr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int multi;

	if (hostapd_das_nas_mismatch(hapd, attr))
		return RADIUS_DAS_NAS_MISMATCH;

	sta = hostapd_das_find_sta(hapd, attr, &multi);
	if (sta == NULL) {
		if (multi) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Multiple sessions match - not supported");
			return RADIUS_DAS_MULTI_SESSION_MATCH;
		}
		if (hostapd_das_disconnect_pmksa(hapd, attr) == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: PMKSA cache entry matched");
			return RADIUS_DAS_SUCCESS;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: No matching session found");
		return RADIUS_DAS_SESSION_NOT_FOUND;
	}

	wpa_printf(MSG_DEBUG, "RADIUS DAS: Found a matching session " MACSTR
		   " - disconnecting", MAC2STR(sta->addr));
	wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);

	hostapd_drv_sta_deauth(hapd, sta->addr,
			       WLAN_REASON_PREV_AUTH_NOT_VALID);
	ap_sta_deauthenticate(hapd, sta, WLAN_REASON_PREV_AUTH_NOT_VALID);

	return RADIUS_DAS_SUCCESS;
}


#ifdef CONFIG_HS20
static enum radius_das_res
hostapd_das_coa(void *ctx, struct radius_das_attrs *attr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int multi;

	if (hostapd_das_nas_mismatch(hapd, attr))
		return RADIUS_DAS_NAS_MISMATCH;

	sta = hostapd_das_find_sta(hapd, attr, &multi);
	if (!sta) {
		if (multi) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Multiple sessions match - not supported");
			return RADIUS_DAS_MULTI_SESSION_MATCH;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: No matching session found");
		return RADIUS_DAS_SESSION_NOT_FOUND;
	}

	wpa_printf(MSG_DEBUG, "RADIUS DAS: Found a matching session " MACSTR
		   " - CoA", MAC2STR(sta->addr));

	if (attr->hs20_t_c_filtering) {
		if (attr->hs20_t_c_filtering[0] & BIT(0)) {
			wpa_printf(MSG_DEBUG,
				   "HS 2.0: Unexpected Terms and Conditions filtering required in CoA-Request");
			return RADIUS_DAS_COA_FAILED;
		}

		hs20_t_c_filtering(hapd, sta, 0);
	}

	return RADIUS_DAS_SUCCESS;
}
#else /* CONFIG_HS20 */
#define hostapd_das_coa NULL
#endif /* CONFIG_HS20 */


#ifdef CONFIG_SQLITE

static int db_table_exists(sqlite3 *db, const char *name)
{
	char cmd[128];

	os_snprintf(cmd, sizeof(cmd), "SELECT 1 FROM %s;", name);
	return sqlite3_exec(db, cmd, NULL, NULL, NULL) == SQLITE_OK;
}


static int db_table_create_radius_attributes(sqlite3 *db)
{
	char *err = NULL;
	const char *sql =
		"CREATE TABLE radius_attributes("
		" id INTEGER PRIMARY KEY,"
		" sta TEXT,"
		" reqtype TEXT,"
		" attr TEXT"
		");"
		"CREATE INDEX idx_sta_reqtype ON radius_attributes(sta,reqtype);";

	wpa_printf(MSG_DEBUG,
		   "Adding database table for RADIUS attribute information");
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		wpa_printf(MSG_ERROR, "SQLite error: %s", err);
		sqlite3_free(err);
		return -1;
	}

	return 0;
}

#endif /* CONFIG_SQLITE */

#endif /* CONFIG_NO_RADIUS */


static int hostapd_start_beacon(struct hostapd_data *hapd,
				bool flush_old_stations)
{
	struct hostapd_bss_config *conf = hapd->conf;

#ifdef CONFIG_QCN_EXTN
	/*
	 * Dependent repeater FH AP: must not transmit beacons until the BH
	 * STA has completed association.
	 */
	if (hapd->iface->conf->conf_extn.repeater &&
	    !hapd->iface->conf->conf_extn.ind_rptr &&
	    os_strncmp(hapd->iface->iface_extn.sta_wpa_state, "COMPLETED", 9) != 0 &&
	    (!hapd->iface->interfaces ||
	     !hapd->iface->interfaces->interfaces_extn.bh_sta_connected)) {
		wpa_printf(MSG_INFO,
			   "%s: repeater FH AP, BH STA not connected"
			   " (sta_wpa_state=\"%s\" bh_sta_connected=%d), skipping start_ap for %s",
			   __func__, hapd->iface->iface_extn.sta_wpa_state,
			   hapd->iface->interfaces ?
			   hapd->iface->interfaces->interfaces_extn.bh_sta_connected : 0,
			   conf->iface);
		return 0;
	}
#endif /* CONFIG_QCN_EXTN */

	if (!conf->start_disabled && ieee802_11_set_beacon(hapd) < 0)
		return -1;

	if (flush_old_stations && !conf->start_disabled &&
	    conf->broadcast_deauth) {
		u8 addr[ETH_ALEN];

		/* Should any previously associated STA not have noticed that
		 * the AP had stopped and restarted, send one more
		 * deauthentication notification now that the AP is ready to
		 * operate. */
		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Deauthenticate all stations at BSS start");
		os_memset(addr, 0xff, ETH_ALEN);
		hostapd_drv_sta_deauth(hapd, addr,
				       WLAN_REASON_PREV_AUTH_NOT_VALID);
	}

#ifdef CONFIG_IEEE80211BN
	/* Initialize SMD Roaming transport if configured */
	if (conf->smd.enabled) {
		static const u16 iap_msg_types[] = {
			ETH_P_1905_IAP_MSG_REQUEST,
			ETH_P_1905_IAP_MSG_RESPONSE,
			ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG,
			ETH_P_1905_SMD_NEIGHBOR_FETCH_MSG,
			ETH_P_1905_SMD_ST_PREP_REQ_MSG,
			ETH_P_1905_SMD_ST_PREP_REP_MSG,
			ETH_P_1905_SMD_ST_EXEC_REQ_MSG,
			ETH_P_1905_SMD_ST_EXEC_REP_MSG,
			ETH_P_1905_SMD_ST_PREP_CTX_MSG,
			ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG,
			ETH_P_1905_SMD_ST_CTX_REQ_MSG,
			ETH_P_1905_SMD_ST_CTX_REP_MSG,
			ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG,
		};
		struct hostapd_data *f_bss;

		wpa_printf(MSG_DEBUG, "SMD: Initializing roaming transport");

		f_bss = hostapd_mld_is_first_bss(hapd) ? hapd :
			hostapd_mld_get_first_bss(hapd);
		if (!f_bss) {
			wpa_printf(MSG_ERROR, "SMD: First BSS context not initialized");
			return -1;
		}
		if (!f_bss->eth_p_1905_ctx) {
			f_bss->eth_p_1905_ctx = eth_p_1905_init(f_bss, iap_msg_types,
								 ARRAY_SIZE(iap_msg_types));
			if (!f_bss->eth_p_1905_ctx) {
				wpa_printf(MSG_ERROR, "SMD: Failed to initialize 1905 transport");
				return -1;
			}
		}
		eth_p_1905_load_partners(f_bss->eth_p_1905_ctx, f_bss);
		hapd->eth_p_1905_ctx = f_bss->eth_p_1905_ctx;
		if (hapd != f_bss)
			wpa_printf(MSG_DEBUG,
				   "SMD: Using 1905 context from first BSS (link_id=%d)",
				   f_bss->mld_link_id);

		if (hostapd_mld_is_first_bss(hapd) &&
		    conf->smd_neighbor_update_enabled &&
		    smd_neighbor_update_init(hapd) < 0) {
			wpa_printf(MSG_ERROR,
				   "SMD: Failed to initialize neighbor update");
			return -1;
		}
		if (!hostapd_mld_is_first_bss(hapd) &&
		    conf->smd_neighbor_update_enabled)
			smd_neighbor_update_notify_link_started(hapd);
	}
#endif /* CONFIG_IEEE80211BN */


	if (hapd->driver && hapd->driver->set_operstate)
		hapd->driver->set_operstate(hapd->drv_priv, 1);

	hostapd_ubus_add_bss(hapd);
	hostapd_ucode_add_bss(hapd);
#ifdef CONFIG_IEEE80211BN
	if (mapc_init(hapd) < 0) {
		wpa_printf(MSG_ERROR, "MAPC: init failed for BSS %s",
			   hapd->conf->iface);
		return -1;
	}
#endif /* CONFIG_IEEE80211BN */

	return 0;
}

static void hostapd_inherit_mbssid_cmn_params(struct hostapd_data *dest_hapd,
					      struct hostapd_data *src_hapd)
{
	dest_hapd->iconf->beacon_int = src_hapd->iconf->beacon_int;
	dest_hapd->conf->vht_capab = src_hapd->conf->vht_capab;
	dest_hapd->conf->vht_mcs_nss_set = src_hapd->conf->vht_mcs_nss_set;
	dest_hapd->conf->he_phy_capab.he_su_beamformer =
		src_hapd->conf->he_phy_capab.he_su_beamformer;
	dest_hapd->conf->he_phy_capab.he_su_beamformee =
		src_hapd->conf->he_phy_capab.he_su_beamformee;
	dest_hapd->conf->he_phy_capab.he_mu_beamformer =
		src_hapd->conf->he_phy_capab.he_mu_beamformer;
	dest_hapd->conf->he_phy_capab.he_ul_mumimo =
		src_hapd->conf->he_phy_capab.he_ul_mumimo;
	dest_hapd->iconf->he_op.he_basic_mcs_nss_set =
		src_hapd->iconf->he_op.he_basic_mcs_nss_set;
	dest_hapd->iconf->he_op.he_rts_threshold =
		src_hapd->iconf->he_op.he_rts_threshold;
	dest_hapd->conf->spp_amsdu = src_hapd->conf->spp_amsdu;
	dest_hapd->iconf->he_op.he_twt_responder =
		src_hapd->iconf->he_op.he_twt_responder;
	dest_hapd->iconf->he_6ghz_max_ampdu_len_exp =
		src_hapd->iconf->he_6ghz_max_ampdu_len_exp;
	dest_hapd->iconf->he_op.he_er_su_disable =
		src_hapd->iconf->he_op.he_er_su_disable;
	dest_hapd->conf->eht_phy_capab.su_beamformer =
		src_hapd->conf->eht_phy_capab.su_beamformer;
	dest_hapd->conf->eht_phy_capab.su_beamformee =
		src_hapd->conf->eht_phy_capab.su_beamformee;
	dest_hapd->conf->eht_phy_capab.mu_beamformer =
		src_hapd->conf->eht_phy_capab.mu_beamformer;
	dest_hapd->conf->eht_phy_capab.eht_bfme_ss_80 =
		src_hapd->conf->eht_phy_capab.eht_bfme_ss_80;
	dest_hapd->conf->eht_phy_capab.eht_bfme_ss_160 =
		src_hapd->conf->eht_phy_capab.eht_bfme_ss_160;
	dest_hapd->conf->eht_phy_capab.eht_bfme_ss_320 =
		src_hapd->conf->eht_phy_capab.eht_bfme_ss_320;
	dest_hapd->conf->eht_ltf = src_hapd->conf->eht_ltf;
	dest_hapd->iconf->enable_mcs15 = src_hapd->iconf->enable_mcs15;
#ifdef CONFIG_TESTING_OPTIONS
	dest_hapd->iconf->ecsa_ie_only = src_hapd->iconf->ecsa_ie_only;
#endif

	wpa_printf(MSG_DEBUG,
		   "MBSSID common parameters are successfully inherited for %s from %s",
		   dest_hapd->conf->iface, src_hapd->conf->iface);
}

static void hostapd_sync_mbssid_cmn_params(struct hostapd_data *hapd)
{
	struct hostapd_data *bss, *tx_hapd;
	size_t num_bss, i;

	tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	if (!tx_hapd)
		return;

	if (hapd != tx_hapd) {
		hostapd_inherit_mbssid_cmn_params(hapd, tx_hapd);
		return;
	}

	num_bss = hostapd_get_mbssid_max_num_bss(hapd);

	for (i = 0; i < num_bss; i++) {
		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			bss = hostapd_get_multi_group_bss(tx_hapd->mbssid_group, i);
		else
			bss = hapd->iface->bss[i];

		if (!bss || !bss->conf || !bss->started || bss == tx_hapd)
			continue;

		hostapd_inherit_mbssid_cmn_params(tx_hapd, bss);
		break;
	}
}

/**
 * hostapd_setup_bss - Per-BSS setup (initialization)
 * @hapd: Pointer to BSS data
 * @first: Whether this BSS is the first BSS of an interface; false = not first,
 *	but interface may exist
 * @start_beacon: Whether Beacon frame template should be configured and
 *	transmission of Beaconf rames started at this time. This is used when
 *	MBSSID element is enabled where the information regarding all BSSes
 *	should be retrieved before configuring the Beacon frame template. The
 *	calling functions are responsible for configuring the Beacon frame
 *	explicitly if this is set to false.
 *
 * This function is used to initialize all per-BSS data structures and
 * resources. This gets called in a loop for each BSS when an interface is
 * initialized. Most of the modules that are initialized here will be
 * deinitialized in hostapd_cleanup().
 */
int hostapd_setup_bss(struct hostapd_data *hapd, bool first, bool start_beacon)
{
	struct hostapd_bss_config *conf = hapd->conf;
	u8 ssid[SSID_MAX_LEN + 1];
	int ssid_len, set_ssid;
	char force_ifname[IFNAMSIZ];
	u8 if_addr[ETH_ALEN];
	int flush_old_stations = 1;
	bool is_mesh = false;

#ifdef CONFIG_MESH
	is_mesh = hapd->iface->mconf ? true : false;
#endif

	wpa_printf(MSG_DEBUG, "%s(hapd=%p (%s), first=%d reenable=%u)",
		   __func__, hapd, conf->iface, first, hapd->reenable);
#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_interface_create(hapd);
#endif

	/* prepare per-BSS rates early from BSS config and current mode */
	if (hapd->iface->current_mode) {
		if (hostapd_prepare_rates(hapd, hapd->iface->current_mode)) {
			wpa_printf(MSG_ERROR, "Failed to prepare rates table.");
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_WARNING,
				       "Failed to prepare rates table.");
			return -1;
		}
	}

#ifdef EAP_SERVER_TNC
	if (conf->tnc && tncs_global_init() < 0) {
		wpa_printf(MSG_ERROR, "Failed to initialize TNCS");
		return -1;
	}
#endif /* EAP_SERVER_TNC */

	if (hapd->started) {
		wpa_printf(MSG_ERROR, "%s: Interface %s was already started",
			   __func__, conf->iface);
		return -1;
	}
	hapd->started = 1;

	if (!first) {
		u8 *addr = hapd->own_addr;

		if (hapd_reenable_pending(hapd))
			goto setup_mld;

		if (!is_zero_ether_addr(conf->bssid)) {
			/* Allocate the configured BSSID. */
			os_memcpy(hapd->own_addr, conf->bssid, ETH_ALEN);

			if (hostapd_mac_comp(hapd->own_addr,
					     hapd->iface->bss[0]->own_addr) ==
			    0) {
				wpa_printf(MSG_ERROR, "BSS '%s' may not have "
					   "BSSID set to the MAC address of "
					   "the radio", conf->iface);
				return -1;
			}
		} else if (hapd->iconf->use_driver_iface_addr) {
			addr = NULL;
#ifdef CONFIG_QCN_EXTN
		} else if (hapd->iconf->use_driver_vendor_addr) {
			addr = NULL;
#endif /* CONFIG_QCN_EXTN */
		} else {
			/* Allocate the next available BSSID. */
			do {
				inc_byte_array(hapd->own_addr, ETH_ALEN);
			} while (mac_in_conf(hapd->iconf, hapd->own_addr));
		}

#ifdef CONFIG_IEEE80211BE
		if (conf->mld_ap) {
			struct hostapd_data *h_hapd;

			h_hapd = hostapd_mld_get_first_bss(hapd);
			if (h_hapd) {
				hapd->drv_priv = h_hapd->drv_priv;
				hapd->interface_added = h_hapd->interface_added;
				wpa_printf(MSG_DEBUG,
					   "Setup of non first link (%d) BSS of MLD %s",
					   hapd->mld_link_id, hapd->conf->iface);
				goto setup_mld;
			}
			/*
			 * Use the configured MLD MAC address
			 * as the interface hardware address
			 * if this AP is a part of an AP MLD.
			 */
			if (!is_zero_ether_addr(hapd->conf->mld_addr))
				addr = hapd->conf->mld_addr;
			else if (hapd->iconf->use_driver_iface_addr)
				addr = NULL;
#ifdef CONFIG_QCN_EXTN
			else if (hapd->iconf->use_driver_vendor_addr)
				addr = NULL;
#endif /* CONFIG_QCN_EXTN */
			else
				addr = hapd->own_addr;
		}
#endif /* CONFIG_IEEE80211BE */

		hapd->interface_added = 1;
		if (hostapd_if_add(hapd->iface->bss[0], WPA_IF_AP_BSS,
				   conf->iface, addr, hapd,
				   &hapd->drv_priv, force_ifname, if_addr,
				   conf->bridge[0] ? conf->bridge : NULL,
				   1, hapd->conf->ppe_vp_type)) {
			wpa_printf(MSG_ERROR, "Failed to add BSS (BSSID="
				   MACSTR ")", MAC2STR(hapd->own_addr));
			hapd->interface_added = 0;
			return -1;
		}

		if (!addr)
			os_memcpy(hapd->own_addr, if_addr, ETH_ALEN);

#ifdef CONFIG_QCN_EXTN
		/*
		 * When BSSID is not configured, try to derive a per-BSS address
		 * via vendor command for both Non-MLD and first MLD BSS.
		 * For Non-MLD, also update the netdev MAC to keep it in sync.
		 */
		if (is_zero_ether_addr(conf->bssid) &&
		    hapd->iconf->use_driver_vendor_addr) {
			if (hostapd_drv_fetch_and_set_vendor_bssid_extn(hapd))
				wpa_printf(MSG_DEBUG,
					   "fetch and set vendor BSSID failed");
		}
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap) {
			wpa_printf(MSG_DEBUG,
				   "Setup of first link (%d) BSS of MLD %s",
				   hapd->mld_link_id, hapd->conf->iface);
			os_memcpy(hapd->mld->mld_addr, addr ? addr : if_addr,
				  ETH_ALEN);
		}
#endif /* CONFIG_IEEE80211BE */
	}

#ifdef CONFIG_IEEE80211BE
setup_mld:
	if (hapd->conf->mld_ap && !first) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Set %s link_id=%u, mld_addr=" MACSTR
			   ", own_addr=" MACSTR, hapd->conf->iface,
			   hapd->mld_link_id, MAC2STR(hapd->mld->mld_addr),
			   MAC2STR(hapd->own_addr));

#ifdef CONFIG_QCN_EXTN
		/* Get per-link BSSID using vendor cmd (non-first links) */
		if (is_zero_ether_addr(conf->bssid) &&
		    hapd->iconf->use_driver_vendor_addr) {
			if (hostapd_drv_fetch_and_set_vendor_bssid_extn(hapd))
				wpa_printf(MSG_DEBUG,
					   "fetch and set vendor BSSID failed");
		}
#endif /* CONFIG_QCN_EXTN */

		if (!hapd_reenable_pending(hapd) &&
		    hostapd_drv_link_add(hapd, hapd->mld_link_id,
					 hapd->own_addr)) {
			wpa_printf(MSG_ERROR,
				   "MLD: Failed to add link %d in MLD %s",
				   hapd->mld_link_id, hapd->conf->iface);
#ifdef CONFIG_QCN_EXTN
			hostapd_free_bss_index_extn(hapd);
#endif /* CONFIG_QCN_EXTN */
			/* For non first link the drv_priv is already assigned from
			 * hhapd, hence, we have to reset in failure.
			 */
			hapd->interface_added = 0;
			hapd->drv_priv = NULL;
			return -1;
		}

#ifdef CONFIG_QCN_EXTN
		if (!hapd_reenable_pending(hapd)) {
			if (hostapd_notify_link_repurpose(hapd, "hostapd_setup_bss"))
				return -1;
		}
#endif /* CONFIG_QCN_EXTN */

		if (!hapd_reenable_pending(hapd)) {
			if (hostapd_mld_add_link(hapd) < 0) {
				hostapd_mld_remove_link(hapd);
				return -1;
			}
		}
		hostapd_validate_update_ml_max_rec_links(hapd);
	}
	if (!is_mesh && hapd->iface->current_hw_info &&
	    !hostapd_ucode_update_radio_mask(hapd->conf->iface,
					     hapd->iface->current_hw_info->hw_idx))
		wpa_printf(MSG_ERROR,
			   "Failed to update radio mask for %s",
			   hapd->conf->iface);
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_QCN_EXTN
	if (!first && !hapd_reenable_pending(hapd) &&
	    hostapd_drv_mark_vap_submode(hapd, hapd->conf->bss_extn.vap_submode)) {
		wpa_printf(MSG_ERROR, "vap_submode vendor command failed: %s",
			   hapd->conf->iface);
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */
	/* MBSSID setup already done during reenable*/
	if (!hapd_reenable_pending(hapd) &&
	    hostapd_mbssid_setup_bss(hapd))
		return -1;

	if (hapd->iconf->mbssid)
		hostapd_sync_mbssid_cmn_params(hapd);

#ifdef CONFIG_QCN_EXTN
	if (hapd->iconf->mbssid != MBSSID_DISABLED &&
	    hostapd_validate_mbssid_group_repurpose_mode_extn(hapd)){
		wpa_printf(MSG_ERROR,
			   "hapd:%s Mismatch in repurpose mode Failed to setup bss",
			   hapd->conf->iface);
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	struct hostapd_hw_modes *mode;
	struct uhr_capabilities *uhr_cap;

	if (hostapd_is_uhr_enabled(hapd) &&
	    ARRAY_SIZE(hapd->sta_aid) > 1) {
		/* Reserve AIDs 56 to 63 in UHR for Critical Update */
		hapd->sta_aid[1] |= 0xFF000000;
	}

	mode = hapd->iface->current_mode;
	if (!mode) {
		wpa_printf(MSG_ERROR,
			   "Current hw mode mode not found, disabling DPS Assist");
		hapd->conf->dps_assist = FEATURE_DISABLED;
	} else {
		uhr_cap = &mode->uhr_capab[IEEE80211_MODE_AP];

		if (!uhr_cap->uhr_supported ||
		    !(uhr_cap->mac_cap[0] & UHR_MACCAP_DPS_ASSIST)) {
			wpa_printf(MSG_DEBUG, "Driver does not support DPS Assist");
			hapd->conf->dps_assist = FEATURE_DISABLED;
		}
	}
#endif /* CONFIG_IEEE80211BN */

	if (conf->wmm_enabled < 0)
		conf->wmm_enabled = hapd->iconf->ieee80211n |
			hapd->iconf->ieee80211ax;

#ifdef CONFIG_IEEE80211R_AP
	if (is_zero_ether_addr(conf->r1_key_holder))
		os_memcpy(conf->r1_key_holder, hapd->own_addr, ETH_ALEN);
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_MESH
	if ((hapd->conf->mesh & MESH_ENABLED) && hapd->iface->mconf == NULL)
		flush_old_stations = 0;
#endif /* CONFIG_MESH */

	if (flush_old_stations)
		hostapd_flush(hapd);
	hostapd_set_privacy(hapd, 0);

#ifdef CONFIG_WEP
	if (!hostapd_drv_nl80211(hapd))
		hostapd_broadcast_wep_clear(hapd);
	if (hostapd_setup_encryption(conf->iface, hapd))
		return -1;
#endif /* CONFIG_WEP */

	/*
	 * Fetch the SSID from the system and use it or,
	 * if one was specified in the config file, verify they
	 * match.
	 */
	ssid_len = hostapd_get_ssid(hapd, ssid, sizeof(ssid));
	if (ssid_len < 0) {
		wpa_printf(MSG_ERROR, "Could not read SSID from system");
		return -1;
	}
	if (conf->ssid.ssid_set) {
		/*
		 * If SSID is specified in the config file and it differs
		 * from what is being used then force installation of the
		 * new SSID.
		 */
		set_ssid = (conf->ssid.ssid_len != (size_t) ssid_len ||
			    os_memcmp(conf->ssid.ssid, ssid, ssid_len) != 0);
	} else {
		/*
		 * No SSID in the config file; just use the one we got
		 * from the system.
		 */
		set_ssid = 0;
		conf->ssid.ssid_len = ssid_len;
		os_memcpy(conf->ssid.ssid, ssid, conf->ssid.ssid_len);
	}

	/*
	 * Short SSID calculation is identical to FCS and it is defined in
	 * IEEE Std 802.11-2024, 9.4.2.169.3 (Calculating the Short-SSID).
	 */
	conf->ssid.short_ssid = ieee80211_crc32(conf->ssid.ssid,
						conf->ssid.ssid_len);

	if (!hostapd_drv_none(hapd)) {
		wpa_printf(MSG_DEBUG, "Using interface %s with hwaddr " MACSTR
			   " and ssid \"%s\"",
			   conf->iface, MAC2STR(hapd->own_addr),
			   wpa_ssid_txt(conf->ssid.ssid, conf->ssid.ssid_len));
	}

	if (hostapd_setup_wpa_psk(conf)) {
		wpa_printf(MSG_ERROR, "WPA-PSK setup failed.");
		return -1;
	}

	/* Set SSID for the kernel driver (to be used in beacon and probe
	 * response frames) */
	if (set_ssid && hostapd_set_ssid(hapd, conf->ssid.ssid,
					 conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		return -1;
	}

	if (wpa_debug_level <= MSG_MSGDUMP)
		conf->radius->msg_dumps = 1;
#ifndef CONFIG_NO_RADIUS

#ifdef CONFIG_SQLITE
	if (conf->radius_req_attr_sqlite) {
		if (sqlite3_open(conf->radius_req_attr_sqlite,
				 &hapd->rad_attr_db)) {
			wpa_printf(MSG_ERROR, "Could not open SQLite file '%s'",
				   conf->radius_req_attr_sqlite);
			return -1;
		}

		wpa_printf(MSG_DEBUG, "Opening RADIUS attribute database: %s",
			   conf->radius_req_attr_sqlite);
		if (!db_table_exists(hapd->rad_attr_db, "radius_attributes") &&
		    db_table_create_radius_attributes(hapd->rad_attr_db) < 0)
			return -1;
	}
#endif /* CONFIG_SQLITE */

	hapd->radius = radius_client_init(hapd, conf->radius);
	if (!hapd->radius) {
		wpa_printf(MSG_ERROR,
			   "RADIUS client initialization failed.");
		return -1;
	}

	if (conf->radius_das_port) {
		struct radius_das_conf das_conf;

		os_memset(&das_conf, 0, sizeof(das_conf));
		das_conf.port = conf->radius_das_port;
		das_conf.nas_identifier = conf->nas_identifier;
		das_conf.shared_secret = conf->radius_das_shared_secret;
		das_conf.shared_secret_len =
			conf->radius_das_shared_secret_len;
		das_conf.client_addr = &conf->radius_das_client_addr;
		das_conf.time_window = conf->radius_das_time_window;
		das_conf.require_event_timestamp =
			conf->radius_das_require_event_timestamp;
		das_conf.require_message_authenticator =
			conf->radius_das_require_message_authenticator;
		das_conf.ctx = hapd;
		das_conf.disconnect = hostapd_das_disconnect;
		das_conf.coa = hostapd_das_coa;
		hapd->radius_das = radius_das_init(&das_conf);
		if (!hapd->radius_das) {
			wpa_printf(MSG_ERROR,
				   "RADIUS DAS initialization failed.");
			return -1;
		}
	}
#endif /* CONFIG_NO_RADIUS */

	if (hostapd_acl_init(hapd)) {
		wpa_printf(MSG_ERROR, "ACL initialization failed.");
		return -1;
	}
	if (hostapd_init_wps(hapd, conf))
		return -1;

#ifdef CONFIG_DPP
	hapd->gas = gas_query_ap_init(hapd, hapd->msg_ctx);
	if (!hapd->gas)
		return -1;
	if (hostapd_dpp_init(hapd))
		return -1;
#endif /* CONFIG_DPP */

#ifdef CONFIG_NAN_USD
	if (hostapd_nan_usd_init(hapd) < 0)
		return -1;
#endif /* CONFIG_NAN_USD */

	if (authsrv_init(hapd) < 0)
		return -1;

	if (ieee802_1x_init(hapd)) {
		wpa_printf(MSG_ERROR, "IEEE 802.1X initialization failed.");
		return -1;
	}
#ifdef CONFIG_IEEE8021X_AUTH
	hapd->send_eap_req = ieee80211_send_eap_req;
#endif /* CONFIG_IEEE8021X_AUTH */

	if (conf->wpa && hostapd_setup_wpa(hapd))
		return -1;

	if (accounting_init(hapd)) {
		wpa_printf(MSG_ERROR, "Accounting initialization failed.");
		return -1;
	}

#if defined(CONFIG_INTERWORKING) || defined(CONFIG_DPP)
	if (gas_serv_init(hapd)) {
		wpa_printf(MSG_ERROR, "GAS server initialization failed");
		return -1;
	}
#endif /* CONFIG_INTERWORKING || CONFIG_DPP */

	if (conf->qos_map_set_len &&
	    hostapd_drv_set_qos_map(hapd, conf->qos_map_set,
				    conf->qos_map_set_len)) {
		wpa_printf(MSG_ERROR, "Failed to initialize QoS Map");
		return -1;
	}

	if (conf->bss_load_update_period && bss_load_update_init(hapd)) {
		wpa_printf(MSG_ERROR, "BSS Load initialization failed");
		return -1;
	}

	if (conf->bridge[0]) {
		/* Set explicitly configured bridge parameters that might have
		 * been lost if the interface has been removed out of the
		 * bridge. */

		/* multicast to unicast on bridge ports */
		if (conf->bridge_multicast_to_unicast)
			hostapd_drv_br_port_set_attr(
				hapd, DRV_BR_PORT_ATTR_MCAST2UCAST, 1);

		/* hairpin mode */
		if (conf->bridge_hairpin)
			hostapd_drv_br_port_set_attr(
				hapd, DRV_BR_PORT_ATTR_HAIRPIN_MODE, 1);
	}

	if (conf->proxy_arp) {
		if (x_snoop_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "Generic snooping infrastructure initialization failed");
			return -1;
		}

		if (dhcp_snoop_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "DHCP snooping initialization failed");
			return -1;
		}

		if (ndisc_snoop_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "Neighbor Discovery snooping initialization failed");
			return -1;
		}
	}

	if (!hostapd_drv_none(hapd) && vlan_init(hapd)) {
		wpa_printf(MSG_ERROR, "VLAN initialization failed.");
		return -1;
	}

#ifdef CONFIG_IEEE80211AX
	char buf[128] = {0};

	if (hapd->conf->scs) {
		os_snprintf(buf, 128, "%s_%s", CHAIN_NAME, conf->iface);
		hostapd_config_nft_chain(hapd, TABLE_NAME, buf, true);
	}

	hapd->cca_count = (hapd->cca_count > 0) ?
			   hapd->cca_count :
			   HE_BSS_COLOR_CCA_COUNT_DEFAULT;
#endif

	/* If TX BSS is already beaconing, update it with newly added profile
	 */
	if (start_beacon)
		ieee802_11_update_beacon_mbssid(hapd);

	if (start_beacon && hostapd_start_beacon(hapd, flush_old_stations) < 0)
		return -1;

	if (hapd->wpa_auth && wpa_init_keys(hapd->wpa_auth) < 0)
		return -1;

	atf_offload_set_ssid_sched_policy(hapd);
	return 0;
}


static void hostapd_tx_queue_params(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int i;
	struct hostapd_tx_queue_params *p;

#ifdef CONFIG_MESH
	if ((hapd->conf->mesh & MESH_ENABLED) && iface->mconf == NULL)
		return;
#endif /* CONFIG_MESH */

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		p = &iface->conf->tx_queue[i];

		if (hostapd_set_tx_queue_params(hapd, i, p->aifs, p->cwmin,
						p->cwmax, p->burst,
						p->acm, p->noack)) {
			wpa_printf(MSG_DEBUG, "Failed to set TX queue "
				   "parameters for queue %d.", i);
			/* Continue anyway */
		}
	}
}


static int hostapd_set_acl_list(struct hostapd_data *hapd,
				struct mac_acl_entry *mac_acl,
				int n_entries, u8 accept_acl)
{
	struct hostapd_acl_params *acl_params;
	int i, err;

	acl_params = os_zalloc(sizeof(*acl_params) +
			       (n_entries * sizeof(acl_params->mac_acl[0])));
	if (!acl_params)
		return -ENOMEM;

	for (i = 0; i < n_entries; i++)
		os_memcpy(acl_params->mac_acl[i].addr, mac_acl[i].addr,
			  ETH_ALEN);

	acl_params->acl_policy = accept_acl;
	acl_params->num_mac_acl = n_entries;

	err = hostapd_drv_set_acl(hapd, acl_params);

	os_free(acl_params);

	return err;
}


int hostapd_set_acl(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int err = 0;
	u8 accept_acl;

	if (hapd->iface->drv_max_acl_mac_addrs == 0)
		return 0;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	/* Allow ACL to be configured if BSS is a repurposed link under AP MLD */
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap) {
		wpa_printf(MSG_DEBUG,
			   "Kernel doesn't support offloaded ACL for AP MLD. Use hostapd based ACL instead.");
		return 0;
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	if (conf->macaddr_acl == DENY_UNLESS_ACCEPTED) {
		accept_acl = 1;
		err = hostapd_set_acl_list(hapd, conf->accept_mac,
					   conf->num_accept_mac,
					   accept_acl);
		if (err) {
			wpa_printf(MSG_DEBUG, "Failed to set accept acl");
			return -1;
		}
	} else if (conf->macaddr_acl == ACCEPT_UNLESS_DENIED) {
		accept_acl = 0;
		err = hostapd_set_acl_list(hapd, conf->deny_mac,
					   conf->num_deny_mac,
					   accept_acl);
		if (err) {
			wpa_printf(MSG_DEBUG, "Failed to set deny acl");
			return -1;
		}
	}
	return err;
}


int hostapd_set_ctrl_sock_iface(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	int ret;

	if (hapd->conf->mld_ap) {
		ret = os_snprintf(hapd->ctrl_sock_iface,
				  sizeof(hapd->ctrl_sock_iface), "%s_%s%d",
				  hapd->conf->iface, WPA_CTRL_IFACE_LINK_NAME,
				  hapd->mld_link_id);
		if (os_snprintf_error(sizeof(hapd->ctrl_sock_iface), ret))
			return -1;
	} else {
		os_strlcpy(hapd->ctrl_sock_iface, hapd->conf->iface,
			   sizeof(hapd->ctrl_sock_iface));
	}
#endif /* CONFIG_IEEE80211BE */
	return 0;
}


static int start_ctrl_iface_bss(struct hostapd_data *hapd)
{
	if (!hapd->iface->interfaces ||
	    !hapd->iface->interfaces->ctrl_iface_init)
		return 0;

	if (hostapd_set_ctrl_sock_iface(hapd))
		return -1;

	if (hapd->iface->interfaces->ctrl_iface_init(hapd)) {
		wpa_printf(MSG_ERROR,
			   "Failed to setup control interface for %s",
			   hapd->conf->iface);
		return -1;
	}

	return 0;
}


static int start_ctrl_iface(struct hostapd_iface *iface)
{
	size_t i;

	if (!iface->interfaces || !iface->interfaces->ctrl_iface_init)
		return 0;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];

		if (hostapd_set_ctrl_sock_iface(hapd))
			return -1;

		if (iface->interfaces->ctrl_iface_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "Failed to setup control interface for %s",
				   hapd->conf->iface);
			return -1;
		}
	}

	return 0;
}


/* When NO_IR flag is set and AP is stopped, clean up BSS parameters without
 * deinitializing the driver and the control interfaces. A subsequent
 * REG_CHANGE event can bring the AP back up.
 */
void hostapd_no_ir_cleanup(struct hostapd_data *bss)
{
#ifdef CONFIG_IEEE80211BN
	mapc_deinit(bss);
#endif /* CONFIG_IEEE80211BN */
	hostapd_bss_deinit_no_free(bss);
	hostapd_bss_link_deinit(bss);
	hostapd_free_hapd_data(bss);
}

bool hostapd_is_6ghz_chan_txable(const struct hostapd_channel_data *c)
{
	if (c->flag & HOSTAPD_CHAN_NO_IR)
		return false;

	if (c->flag & HOSTAPD_CHAN_DISABLED)
		return false;

	return true;
}

/**
 * hostapd_enable_no_ir_bss_members - Enable selected BSS members under NO-IR
 * @iface: Pointer to hostapd interface context
 * @cat: BSS category selector
 * Return : -1 if failed to enable bsss
 */
static int hostapd_enable_no_ir_bss_members(struct hostapd_iface *iface,
					    enum hostapd_bss_category cat)
{
	int i;
	int failed = 0;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];
		int ret = 0;

		if (!hapd || hapd->started)
			continue;

		if (!hapd_reenable_pending(hapd))
			continue;

		if (!hostapd_is_bss_in_category(hapd, cat))
			continue;

		ret = hostapd_enable_bss(hapd);
		if (ret) {
			failed = -1;
			wpa_printf(MSG_ERROR,
				   "Failed to re-enable %s link %u after setting new transmitting profile",
				   hapd->conf->iface, hapd->mld_link_id);
			/* Continue attempting to enable remaining BSSes */
		}
	}

	return failed;
}

bool hostapd_is_bss_in_category(struct hostapd_data *hapd,
				enum hostapd_bss_category cat)
{
	struct hostapd_data *tx_bss;

	WPA_ASSERT(cat < CAT_MAX);
	switch (cat) {
		case CAT_ALL_BSS:
			return true;

		case CAT_TX_BSS:
			tx_bss = hostapd_mbssid_get_tx_bss(hapd);
			return tx_bss && hapd == tx_bss;

		case CAT_NON_TX_BSS:
			tx_bss = hostapd_mbssid_get_tx_bss(hapd);
			return tx_bss && hapd != tx_bss;

		case CAT_MAX:
		default:
			/*
			 * Fail-safe: do not disable anything on unexpected input.
			 * WPA_ASSERT() may be compiled out in some builds.
			 */
			return false;
	}
}

/* hostapd_enable_no_ir_mbssids - Enable Tx BSS members
 * when at least on channel in the iface can be used for Tx (non NO_IR)
 * @cat: BSS category selector
 * Return : -1 if failed to enable bsss
 */
static int hostapd_enable_no_ir_mbssids(struct hostapd_iface *iface)
{
	int ret;

	ret = hostapd_enable_no_ir_bss_members(iface, CAT_TX_BSS);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "Failed to re-enable for TX BSS");
		return -1;
	}
	ret = hostapd_enable_no_ir_bss_members(iface, CAT_NON_TX_BSS);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "Failed to re-enable for NON TX BSS");
		return -1;
	}

	return 0;

}

/* hostapd_enable_no_ir_nonmbssids - Enable Non Tx BSS members under NO-IR
 * @cat: BSS category selector
 * Return : -1 if failed to enable bsss
 */
static int hostapd_enable_no_ir_nonmbssids(struct hostapd_iface *iface)
{
	int ret;

	ret = hostapd_enable_no_ir_bss_members(iface, CAT_ALL_BSS);
	return ret;
}

/* hostapd_enable_no_ir_bsses - Enable BSS members under NO-IR
 * @cat: BSS category selector
 * Return : -1 if failed to enable bsss
 */
static int hostapd_enable_no_ir_bsses(struct hostapd_iface *iface)
{
	int ret = 0;

	if (iface->conf->mbssid != MBSSID_DISABLED)
		ret = hostapd_enable_no_ir_mbssids(iface);
	else
		ret = hostapd_enable_no_ir_nonmbssids(iface);

	if (ret)
		return ret;

	if (!hostapd_check_reenable_bss(iface, REENABLE_NONE))
		hostapd_set_state(iface, HAPD_IFACE_ENABLED);

	return ret;
}

int hostapd_no_ir_channel_list_updated(struct hostapd_iface *iface)
{
	bool all_no_ir, is_6ghz;
	int i, j, ret = 0;
	struct hostapd_hw_modes *mode = NULL;

	all_no_ir = true;
	is_6ghz = false;

	for (i = 0; i < iface->num_hw_features; i++) {
		mode = &iface->hw_features[i];

		if (mode->mode == iface->conf->hw_mode) {
			if (iface->freq > 0 &&
			    !hw_mode_get_channel(mode, iface->freq, NULL)) {
				mode = NULL;
				continue;
			}

			for (j = 0; j < mode->num_channels; j++) {
				if (!is_6ghz_freq(mode->channels[j].freq))
					continue;

				is_6ghz = true;
				if (!hostapd_is_6ghz_chan_txable(&mode->channels[j]))
					continue;

				all_no_ir = false;
				break;

			}
			break;
		}
	}

	if (!mode || !is_6ghz)
		return 0;

	if (iface->state == HAPD_IFACE_ENABLED) {
		if (!all_no_ir) {
			struct hostapd_channel_data *chan;

			chan = hw_get_channel_freq(iface->current_mode->mode,
						   iface->freq, NULL,
						   iface->hw_features,
						   iface->num_hw_features);

			if (!chan) {
				wpa_printf(MSG_ERROR,
					   "NO_IR: Could not derive chan from freq");
				return 0;
			}

			if (hostapd_is_6ghz_chan_txable(chan))
				return 0;

			/* Other Valid channels present. Dont Stop AP if retail AFC is supported
			 * Allow Channel Selection to choose a new channel.
			 */
			if (hostapd_drv_is_retail_afc_supported(iface->bss[0]) &&
			    iface->conf->enable_best_power_mode) {
				/*
				 * Do not trigger a channel change while the backhaul
				 * STA is connected. A channel change would tear down
				 * the dual-channel state while the backhaul link is
				 * still active and cause a crash. Defer power mode
				 * re-evaluation to hostapd_run_pending_repeater_afc_
				 * power_sync() which runs after the next successful
				 * AFC response and REGDOM_SET_BY_DRIVER event.
				 */
				if (hostapd_iface_has_connected_backhaul_sta(iface)) {
					wpa_printf(MSG_DEBUG,
						   "NO_IR: defer AFC channel selection for connected repeater iface=%s",
						   iface->phy);
					iface->is_afc_channel_change_pending = false;
					iface->is_afc_repeater_power_sync_pending = true;
					return 0;
				}

				wpa_printf(MSG_DEBUG,
					   "NO_IR: Enable retail AFC channel selection");
				iface->is_afc_channel_change_pending = true;
				return 0;
			}

			wpa_printf(MSG_DEBUG,
				   "NO_IR: The current channel has NO_IR flag now, stop AP.");
		} else {
			wpa_printf(MSG_DEBUG,
				   "NO_IR: All chan in new chanlist are NO_IR, stop AP.");
		}

		hostapd_set_no_ir_state(iface);
	} else if (iface->state == HAPD_IFACE_NO_IR) {
		if (all_no_ir) {
			wpa_printf(MSG_DEBUG,
				   "NO_IR: AP in NO_IR and all chan in the new chanlist are NO_IR. Ignore");
			return 0;
		}

		if (iface->freq) {
			struct hostapd_channel_data *chan;

			chan = hw_get_channel_freq(iface->current_mode->mode,
						   iface->freq, NULL,
						   iface->hw_features,
						   iface->num_hw_features);
			if (!chan) {
				wpa_printf(MSG_ERROR,
					   "NO_IR: Could not derive chan from freq");
				return 0;
			}

			/* If the last operating channel is NO_IR, trigger ACS.
			 */
			if (!hostapd_is_6ghz_chan_txable(chan)) {
				iface->freq = 0;
				iface->conf->channel = 0;
				if (!iface->conf->acs) {
					if (acs_init(iface) != HOSTAPD_CHAN_ACS)
						wpa_printf(MSG_ERROR,
							   "NO_IR: Could not start ACS");
					return 0;
				}
			}
		}

		wpa_printf(MSG_DEBUG,
			   "NO_IR: Re-enabling interface after channel list update");
		if (!hostapd_check_reenable_bss(iface, REENABLE_NONE))
			setup_interface2(iface);
		else
			ret = hostapd_enable_no_ir_bsses(iface);
	}

	return ret;
}


static void channel_list_update_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;

	if (!iface->wait_channel_update) {
		wpa_printf(MSG_INFO, "Channel list update timeout, but interface was not waiting for it");
		return;
	}

	/*
	 * It is possible that the existing channel list is acceptable, so try
	 * to proceed.
	 */
	wpa_printf(MSG_DEBUG, "Channel list update timeout - try to continue anyway");
	setup_interface2(iface);
}

#ifdef HOSTAPD
/**
 * hostapd_find_random_chan_and_switch() - Switch to a random 6 GHz channel
 *
 * This API find a random channel in the 6 GHz band and switches to it in
 * best power mode.
 *
 * @iface: Pointer to hostapd interface data
 *
 * Return: 0 on success, -1 on failure
 */
static int hostapd_find_random_chan_and_switch(struct hostapd_iface *iface)
{
	u8 best_power_mode;
	int ret = -1;

	if (!is_6ghz_freq(iface->freq)) {
		wpa_printf(MSG_DEBUG, "Not a 6GHz iface");
		return 0;
	}

	if (!iface->conf->enable_best_power_mode) {
		wpa_printf(MSG_DEBUG,
			   "%s: Best power mode not enabled", __func__);
		return -1;
	}

	/**
	 * (1) Find a random channel for the current operating BW based on the
	 *     new AFC payload and regulatory channel lists.
	 * (2) Calculate the best power mode for the channel.
	 * (3) Switch to the new channel in the computed power mode.
	 */
#ifdef CONFIG_QCN_EXTN
	ret = hostapd_intf_afc_received(iface);
	if (!ret)
		return ret;
#else
	return ret;
#endif /* CONFIG_QCN_EXTN */

	wpa_printf(MSG_ERROR, "Failed to select a random channel");
	best_power_mode = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
	if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
		wpa_printf(MSG_INFO, "%s: Best power mode for Freq %d is %d",
			   __func__, iface->freq, best_power_mode);

		iface->power_mode_6ghz_before_change = best_power_mode;
		if (hostapd_switch_power_mode(iface->bss[0])) {
			wpa_printf(MSG_ERROR, "Failed to switch to power mode %d",
				   best_power_mode);
			return -1;
		}
	} else {
		wpa_printf(MSG_ERROR,
			   "%s: Cannot determine best power mode for Freq %d",
			   __func__, iface->freq);
		return -1;
	}

	return 0;
}

/**
 * hostapd_is_current_afc_tuple_valid() - Validate current tuple in power mode
 * @iface: Pointer to hostapd interface data
 * @power_mode: 6 GHz AP power mode to validate
 *
 * Validate the currently operating 6 GHz channel, bandwidth, and puncturing
 * tuple against the requested AP power mode.
 *
 * Return: true when the current tuple is valid, false otherwise.
 */
static bool hostapd_is_current_afc_tuple_valid(struct hostapd_iface *iface,
						       u8 power_mode)
{
	enum chan_width ch_width;
	u8 center_chan_no;
	u16 center_freq;
	u16 bw;

	if (he_reg_is_sp(power_mode) && !iface->is_afc_power_event_received) {
		wpa_printf(MSG_INFO,
			   "AFC repeater power sync: SP unavailable without AFC iface=%s freq=%d mode=%u",
			   iface->phy, iface->freq, power_mode);
		return false;
	}

	ch_width = hostapd_get_chan_width_from_oper_chan_width(iface->conf);
	center_chan_no = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
	center_freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
					     center_chan_no);
	bw = channel_width_to_int(ch_width);

	return hostapd_validate_chan_bw_in_pwr_mode(iface, iface->freq,
						     center_freq, bw,
						     iface->conf->punct_bitmap,
						     power_mode);
}

/**
 * hostapd_get_afc_non_sp_fallback_power_mode() - Determine the non-SP fallback
 * power mode
 * @iface: Pointer to hostapd interface data
 * @current_power_mode: The 6 GHz regulatory power mode currently in use
 *
 * Returns the appropriate non-SP fallback power mode for a 6 GHz AP that must
 * leave Standard Power (SP) operation because AFC data is unavailable or has
 * expired.  The target is chosen using actual regulatory channel availability,
 * not deployment type, so the result is valid for both indoor and outdoor APs:
 *
 * 1. If current mode is INDOOR_SP (8) and LPI is valid for the current
 *    channel/BW/puncture tuple, return LPI.
 * 2. If enable_best_power_mode is set, ask
 *    hostapd_get_best_ap_6ghz_power_mode_for_iface() for the highest-EIRP
 *    non-SP mode that is valid for the current tuple, and return it.
 * 3. Otherwise try LPI then VLP in order, returning the first mode whose
 *    tuple is valid per hostapd_is_current_afc_tuple_valid().
 * 4. If no valid non-SP mode exists, return NL80211_REG_NUM_POWER_MODES.
 *
 * The caller is responsible for acting on NL80211_REG_NUM_POWER_MODES as a
 * no-valid-fallback sentinel (e.g. disconnect backhaul STA or set NO_IR).
 *
 * Return: NL80211_REG_AP_LPI, NL80211_REG_AP_VLP, or
 *         NL80211_REG_NUM_POWER_MODES if no valid non-SP mode exists or
 *         @iface/@current_power_mode preconditions are not met.
 */
static u8
hostapd_get_afc_non_sp_fallback_power_mode(struct hostapd_iface *iface,
					   u8 current_power_mode)
{
	u8 best;

	if (!iface || !iface->conf || !he_reg_is_sp(current_power_mode))
		return NL80211_REG_NUM_POWER_MODES;

	/*
	 * INDOOR_SP (value 8) is set explicitly in config.  Prefer LPI when
	 * the current tuple is valid for it; otherwise fall through to the
	 * generic path below.
	 */
	if (current_power_mode == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP &&
	    hostapd_is_current_afc_tuple_valid(iface, NL80211_REG_AP_LPI))
		return NL80211_REG_AP_LPI;

	/*
	 * When best-power-mode is enabled, use the highest-EIRP non-SP mode
	 * that is valid for the current channel/BW/puncture tuple.  This is
	 * deployment-agnostic and handles both "LPI not present" and
	 * "VLP not present" naturally.
	 */
	if (iface->conf->enable_best_power_mode) {
		best = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
		if (best != NL80211_REG_AP_SP &&
		    best != NL80211_REG_NUM_POWER_MODES &&
		    hostapd_is_current_afc_tuple_valid(iface, best)) {
			wpa_printf(MSG_DEBUG,
				   "AFC fallback: iface=%s bpm best=%u",
				   iface->phy, best);
			return best;
		}
	}

	/* BPM disabled or no BPM result: try LPI then VLP by tuple validity */
	if (hostapd_is_current_afc_tuple_valid(iface, NL80211_REG_AP_LPI)) {
		wpa_printf(MSG_DEBUG,
			   "AFC fallback: iface=%s -> LPI", iface->phy);
		return NL80211_REG_AP_LPI;
	}
	if (hostapd_is_current_afc_tuple_valid(iface, NL80211_REG_AP_VLP)) {
		wpa_printf(MSG_DEBUG,
			   "AFC fallback: iface=%s -> VLP", iface->phy);
		return NL80211_REG_AP_VLP;
	}

	wpa_printf(MSG_DEBUG,
		   "AFC fallback: iface=%s no valid non-SP mode", iface->phy);
	return NL80211_REG_NUM_POWER_MODES;
}

/**
 * hostapd_restore_6ghz_power_mode() - Roll back a failed 6 GHz power mode
 * change
 * @iface: Pointer to hostapd interface data
 * @old_power_mode: The 6 GHz regulatory power mode to restore
 *
 * Restores the interface to @old_power_mode after a failed attempt to switch
 * to a new power mode in hostapd_request_6ghz_power_mode().  The steps are:
 *
 *   1. Write @old_power_mode back into iface->conf->he_6ghz_reg_pwr_type.
 *   2. Clear the local TPE cache via hostapd_clear_local_tpe().
 *   3. Re-run hostapd_get_hw_features(), hostapd_select_hw_mode(), and
 *      hostapd_set_current_hw_info() to bring the hardware state back in
 *      line with @old_power_mode.
 *
 * If any of the three hardware calls in step 3 fail, an error is logged but
 * the function still returns normally.  There is no further recovery path;
 * the caller (hostapd_request_6ghz_power_mode()) will return
 * %HOSTAPD_AFC_PWR_SYNC_ERROR regardless.
 *
 * This function intentionally does not call hostapd_switch_power_mode() or
 * ieee802_11_update_beacons().  It is invoked only when the forward transition
 * has already failed before or during the CSA step, so no over-the-air
 * announcement of the new mode was ever made and no beacon update is needed.
 *
 * iface->power_mode_6ghz_before_change is not modified here.  It is written
 * by hostapd_request_6ghz_power_mode() only after steps 2-4 of that function
 * succeed, so on any rollback path it retains its pre-call value.
 *
 * Context: Called exclusively from hostapd_request_6ghz_power_mode() as an
 *          error-path rollback.  Must not be called in any other context.
 */
static void
hostapd_restore_6ghz_power_mode(struct hostapd_iface *iface, u8 old_power_mode)
{
	iface->conf->he_6ghz_reg_pwr_type = old_power_mode;
	hostapd_clear_local_tpe(iface);
	if (hostapd_get_hw_features(iface) ||
	    hostapd_select_hw_mode(iface) ||
	    hostapd_set_current_hw_info(iface, iface->freq))
		wpa_printf(MSG_ERROR,
			   "Failed to restore 6 GHz power mode iface=%s mode=%u",
			   iface->phy, old_power_mode);
}

/**
 * hostapd_request_6ghz_power_mode() - Execute a 6 GHz AP power mode transition
 * @iface: Pointer to hostapd interface data
 * @hapd:  Pointer to the primary BSS hostapd_data for @iface
 * @current_power_mode: The 6 GHz regulatory power mode the AP is leaving
 *                      (used only for log messages)
 * @new_power_mode: The target 6 GHz regulatory power mode to switch to
 * @reason: Short human-readable string identifying the caller, written to the
 *          log alongside the mode transition (e.g. "fallback", "sync")
 *
 * Drives the full sequence required to move a 6 GHz AP from one regulatory
 * power mode to another on the same operating channel.  The steps are:
 *
 *   1. Write @new_power_mode into iface->conf->he_6ghz_reg_pwr_type and clear
 *      the local TPE cache via hostapd_clear_local_tpe().
 *   2. Refresh the per-mode channel list from the driver with
 *      hostapd_get_hw_features().
 *   3. Re-select the hardware mode for the new power mode with
 *      hostapd_select_hw_mode().
 *   4. Update the cached hardware info for the current frequency with
 *      hostapd_set_current_hw_info().
 *   5. Record @new_power_mode in iface->power_mode_6ghz_before_change so that
 *      the CSA completion path knows which mode was requested.
 *   6. Issue the power-mode Channel Switch Announcement via
 *      hostapd_switch_power_mode().
 *   7. Rebuild and push updated beacon frames via ieee802_11_update_beacons().
 *      A failure here is logged but does not cause the function to return
 *      ERROR; the mode switch itself has already been committed.
 *
 * If any of steps 2-4 or 6 fail, hostapd_restore_6ghz_power_mode() is called
 * to roll back iface->conf->he_6ghz_reg_pwr_type and re-run steps 2-4 with
 * the original mode before returning ERROR.
 * iface->power_mode_6ghz_before_change is not written on a rollback path, so
 * the in-flight marker is not left in an inconsistent state.
 *
 * The caller is responsible for ensuring that the target channel/bandwidth/
 * puncturing tuple is valid in @new_power_mode before calling this function.
 * Use hostapd_is_current_afc_tuple_valid() to perform that check.
 *
 * Context: Called only from hostapd_force_afc_non_sp_power_mode() and
 *          hostapd_sync_current_afc_power_mode().  Must be called with the
 *          interface in a state where a CSA can be issued (i.e. no CSA already
 *          in progress and no pending power mode change).
 *
 * Return:
 * * %HOSTAPD_AFC_PWR_SYNC_UPDATED - all steps succeeded and the power mode
 *   CSA has been initiated; iface->power_mode_6ghz_before_change is set to
 *   @new_power_mode.
 * * %HOSTAPD_AFC_PWR_SYNC_ERROR - one of the hardware refresh, mode selection,
 *   hardware info update, or CSA steps failed; the power mode has been rolled
 *   back to the value it held on entry.
 */
static enum hostapd_afc_power_sync_result
hostapd_request_6ghz_power_mode(struct hostapd_iface *iface,
				struct hostapd_data *hapd,
				u8 current_power_mode,
				u8 new_power_mode,
				const char *reason)
{
	u8 old_power_mode;
	int ret;

	wpa_printf(MSG_INFO,
		   "AFC repeater power mode request: %s iface=%s freq=%d mode=%u->%u",
		   reason, iface->phy, iface->freq, current_power_mode,
		   new_power_mode);
	old_power_mode = iface->conf->he_6ghz_reg_pwr_type;
	iface->conf->he_6ghz_reg_pwr_type = new_power_mode;
	hostapd_clear_local_tpe(iface);
	ret = hostapd_get_hw_features(iface);
	if (ret) {
		hostapd_restore_6ghz_power_mode(iface, old_power_mode);
		wpa_printf(MSG_ERROR,
			   "AFC repeater power mode request failed: hw refresh iface=%s mode=%u ret=%d",
			   iface->phy, new_power_mode, ret);
		return HOSTAPD_AFC_PWR_SYNC_ERROR;
	}

	ret = hostapd_select_hw_mode(iface);
	if (ret) {
		hostapd_restore_6ghz_power_mode(iface, old_power_mode);
		wpa_printf(MSG_ERROR,
			   "AFC repeater power mode request failed: hw mode iface=%s mode=%u ret=%d",
			   iface->phy, new_power_mode, ret);
		return HOSTAPD_AFC_PWR_SYNC_ERROR;
	}

	ret = hostapd_set_current_hw_info(iface, iface->freq);
	if (ret) {
		hostapd_restore_6ghz_power_mode(iface, old_power_mode);
		wpa_printf(MSG_ERROR,
			   "AFC repeater power mode request failed: hw info iface=%s freq=%d mode=%u ret=%d",
			   iface->phy, iface->freq, new_power_mode, ret);
		return HOSTAPD_AFC_PWR_SYNC_ERROR;
	}

	iface->power_mode_6ghz_before_change = new_power_mode;
	if (hostapd_switch_power_mode(hapd)) {
		hostapd_restore_6ghz_power_mode(iface, old_power_mode);
		wpa_printf(MSG_ERROR,
			   "AFC repeater power mode request failed: switch iface=%s freq=%d mode=%u",
			   iface->phy, iface->freq, new_power_mode);
		return HOSTAPD_AFC_PWR_SYNC_ERROR;
	}

	if (ieee802_11_update_beacons(iface))
		wpa_printf(MSG_ERROR,
			   "AFC repeater power mode request: beacon update failed iface=%s mode=%u",
			   iface->phy, new_power_mode);

	return HOSTAPD_AFC_PWR_SYNC_UPDATED;
}

/**
 * hostapd_force_afc_non_sp_power_mode() - Fall back from SP to a non-SP power
 * mode
 * @iface: Pointer to hostapd interface data
 *
 * Attempts to switch a 6 GHz SP AP to the best available non-SP fallback
 * mode (LPI or VLP) when AFC data is no longer available or valid.  The
 * fallback target is chosen deployment-agnostically by checking actual
 * regulatory tuple validity:
 *
 *   1. If INDOOR_SP (8) and LPI tuple is valid -> LPI.
 *   2. If BPM enabled: best non-SP mode from EIRP, revalidated by tuple.
 *   3. LPI tuple valid -> LPI; VLP tuple valid -> VLP.
 *   4. No valid mode -> INVALID_CURRENT.
 *
 * The non-SP mode check is performed BEFORE the transient-defer guards so
 * a non-SP AP with a concurrent CSA never incorrectly returns DEFERRED.
 *
 * Before attempting the switch the function checks for two conditions that
 * require the fallback to be deferred (current mode must be SP for these
 * to apply):
 *
 *   1. A power mode change is already in flight
 *      (iface->power_mode_6ghz_before_change != -1 and differs from the
 *      current mode).  The pending flag is set and DEFERRED is returned.
 *      If power_mode_6ghz_before_change matches the current mode the stale
 *      marker is cleared and evaluation continues.
 *
 *   2. A Channel Switch Announcement (CSA) is in progress.  The pending
 *      flag is set and DEFERRED is returned.
 *
 * On success the power mode switch is driven through
 * hostapd_request_6ghz_power_mode(), which refreshes the hardware channel
 * list, selects the hardware mode, updates the hardware info, issues a
 * power-mode CSA, and updates the beacons.
 *
 * This function is a no-op for non-6 GHz interfaces and for interfaces that
 * are not currently in an SP power mode.
 *
 * Context: Called from the AFC event handlers and the pending repeater sync
 *          path.  Must not be called while holding any lock that
 *          hostapd_request_6ghz_power_mode() or hostapd_csa_in_progress()
 *          may also acquire.
 *
 * Return:
 * * %HOSTAPD_AFC_PWR_SYNC_NOOP - iface is not 6 GHz, is not in SP mode, or
 *   the fallback target equals the current mode; no action taken.
 * * %HOSTAPD_AFC_PWR_SYNC_UPDATED - fallback power mode switch was
 *   successfully initiated via CSA; beacons have been updated.
 * * %HOSTAPD_AFC_PWR_SYNC_DEFERRED - fallback blocked by a transient
 *   condition (CSA or pending power switch); is_afc_repeater_power_sync_pending
 *   is set. Callers with a retry path re-arm retry on the next
 *   REGDOM_SET_BY_DRIVER event; root AP + BPM-disabled fail-safe
 *   callers consume this as NO_IR.
 * * %HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT - the current channel/BW/puncture
 *   tuple has no valid non-SP fallback; the caller must disconnect the
 *   backhaul STA or set NO_IR.
 * * %HOSTAPD_AFC_PWR_SYNC_ERROR - hard internal failure (hw refresh,
 *   mode select, or driver switch failed); no retry is armed.
 */
enum hostapd_afc_power_sync_result
hostapd_force_afc_non_sp_power_mode(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd;
	u8 current_power_mode;
	u8 fallback_power_mode;

	if (!iface || !iface->bss || !iface->bss[0])
		return HOSTAPD_AFC_PWR_SYNC_ERROR;

	if (!is_6ghz_freq(iface->freq))
		return HOSTAPD_AFC_PWR_SYNC_NOOP;

	hapd = iface->bss[0];
	current_power_mode = iface->conf->he_6ghz_reg_pwr_type;

	/*
	 * Clear a stale power_mode_6ghz_before_change marker before the
	 * non-SP check so it does not linger if the AP has already
	 * transitioned out of SP (e.g. after a successful fallback).
	 */
	if (iface->power_mode_6ghz_before_change > -1 &&
	    iface->power_mode_6ghz_before_change == current_power_mode) {
		wpa_printf(MSG_INFO,
			   "AFC repeater fallback: clear stale pending mode iface=%s mode=%u",
			   iface->phy, current_power_mode);
		iface->power_mode_6ghz_before_change = -1;
	}

	/*
	 * Non-SP iface: nothing to fall back from.  Check this before the
	 * transient-defer guards so a non-SP AP with an in-progress CSA
	 * never incorrectly returns DEFERRED (which root callers map to
	 * NO_IR).
	 */
	if (!he_reg_is_sp(current_power_mode))
		return HOSTAPD_AFC_PWR_SYNC_NOOP;

	if (iface->power_mode_6ghz_before_change > -1) {
		/*
		 * Stale same-value marker already cleared above; only a
		 * mismatched pending mode reaches here.
		 */
		wpa_printf(MSG_INFO,
			   "AFC repeater fallback deferred: iface=%s current=%u pending_mode=%d",
			   iface->phy, current_power_mode,
			   iface->power_mode_6ghz_before_change);
		iface->is_afc_repeater_power_sync_pending = true;
		return HOSTAPD_AFC_PWR_SYNC_DEFERRED;
	}

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_INFO,
			   "AFC repeater fallback deferred: iface=%s csa=1",
			   iface->phy);
		iface->is_afc_repeater_power_sync_pending = true;
		return HOSTAPD_AFC_PWR_SYNC_DEFERRED;
	}

	fallback_power_mode = hostapd_get_afc_non_sp_fallback_power_mode
							(iface,
							 current_power_mode);
	/*
	 * NUM_POWER_MODES: current mode is SP but no valid non-SP mode
	 * exists for the current tuple.  Return INVALID_CURRENT so callers
	 * can disconnect the backhaul STA or call hostapd_set_no_ir_state().
	 */
	if (fallback_power_mode == NL80211_REG_NUM_POWER_MODES)
		return HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT;
	if (fallback_power_mode == current_power_mode)
		return HOSTAPD_AFC_PWR_SYNC_NOOP;

	if (!hostapd_is_current_afc_tuple_valid(iface, fallback_power_mode)) {
		wpa_printf(MSG_ERROR,
			   "AFC repeater fallback invalid: iface=%s freq=%d mode=%u punct=0x%x",
			   iface->phy, iface->freq, fallback_power_mode,
			   iface->conf->punct_bitmap);
		return HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT;
	}

	if (iface->state != HAPD_IFACE_ENABLED)
		wpa_printf(MSG_INFO,
			   "AFC repeater fallback while iface state=%u iface=%s",
			   iface->state, iface->phy);

	return hostapd_request_6ghz_power_mode(iface, hapd, current_power_mode,
					       fallback_power_mode, "fallback");
}

enum hostapd_afc_power_sync_result
hostapd_sync_current_afc_power_mode(struct hostapd_iface *iface,
				    bool ignore_best_mode_config)
{
	struct hostapd_data *hapd;
	u8 best_power_mode;
	u8 current_power_mode;
	u8 fallback_power_mode;

	if (!iface || !iface->bss || !iface->bss[0])
		return HOSTAPD_AFC_PWR_SYNC_ERROR;

	if (!is_6ghz_freq(iface->freq))
		return HOSTAPD_AFC_PWR_SYNC_NOOP;

	if (!ignore_best_mode_config && !iface->conf->enable_best_power_mode)
		return HOSTAPD_AFC_PWR_SYNC_NOOP;

	if (iface->state != HAPD_IFACE_ENABLED)
		return HOSTAPD_AFC_PWR_SYNC_NOOP;

	/*
	 * CSA or pending power switch still in progress: cannot evaluate now.
	 * Clear a stale marker (pending == current) before checking, mirroring
	 * hostapd_force_afc_non_sp_power_mode(), so a resolved switch does not
	 * cause repeated DEFERRED returns and stall the retry loop.
	 */
	if (iface->power_mode_6ghz_before_change > -1) {
		u8 cur = iface->conf->he_6ghz_reg_pwr_type;

		if (iface->power_mode_6ghz_before_change == cur)
			iface->power_mode_6ghz_before_change = -1;
		else
			return HOSTAPD_AFC_PWR_SYNC_DEFERRED;
	}
	if (hostapd_csa_in_progress(iface))
		return HOSTAPD_AFC_PWR_SYNC_DEFERRED;

	hapd = iface->bss[0];
	current_power_mode = iface->conf->he_6ghz_reg_pwr_type;

	hostapd_apply_6ghz_dynamic_puncturing(iface);
	best_power_mode = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
	wpa_printf(MSG_INFO,
		   "AFC repeater power sync: iface=%s freq=%d current=%u best=%u punct=0x%x",
		   iface->phy, iface->freq, current_power_mode, best_power_mode,
		   iface->conf->punct_bitmap);

	if (best_power_mode == NL80211_REG_AP_SP &&
	    hostapd_is_current_afc_tuple_valid(iface, NL80211_REG_AP_SP)) {
		if (current_power_mode == NL80211_REG_AP_SP)
			return HOSTAPD_AFC_PWR_SYNC_NOOP;

		return hostapd_request_6ghz_power_mode(iface, hapd,
						       current_power_mode,
						       NL80211_REG_AP_SP, "sync");
	}

	if (!hostapd_is_current_afc_tuple_valid(iface, current_power_mode)) {
		fallback_power_mode =
			hostapd_get_afc_non_sp_fallback_power_mode
						(iface, current_power_mode);

		if (fallback_power_mode != NL80211_REG_NUM_POWER_MODES &&
		    fallback_power_mode != current_power_mode &&
		    hostapd_is_current_afc_tuple_valid
					(iface, fallback_power_mode)) {
			return hostapd_request_6ghz_power_mode(iface, hapd,
						       current_power_mode,
						       fallback_power_mode,
						       "sync fallback");
		}

		wpa_printf(MSG_ERROR,
			   "AFC repeater power sync: current tuple invalid iface=%s freq=%d mode=%u punct=0x%x",
			   iface->phy, iface->freq, current_power_mode,
			   iface->conf->punct_bitmap);
		return HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT;
	}

	return HOSTAPD_AFC_PWR_SYNC_NOOP;
}

int hostapd_handle_afc_channel_change(struct hostapd_iface *iface)
{
	int ret;

	if (iface->state != HAPD_IFACE_ENABLED) {
		wpa_printf(MSG_ERROR, "iface state: %u, not handling afc channel change\n",
			   iface->state);
		return -EOPNOTSUPP;
	}

	iface->is_afc_channel_change_pending = 0;
	eloop_cancel_timeout(afc_channel_change_timeout, iface, NULL);
	ret = hostapd_find_random_chan_and_switch(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to switch to a new channel, moving to NOIR");
		hostapd_set_no_ir_state(iface);
		return -EINVAL;
	}
	return 0;

}

bool hostapd_iface_has_connected_backhaul_sta(struct hostapd_iface *iface)
{
	if (!iface)
		return false;

	if (!hostapd_is_backhaul_sta_conn(iface))
		return false;

	return true;
}

int hostapd_disconnect_backhaul_sta(struct hostapd_iface *iface)
{
	char ctrl_path[128];
	char reply[32];
	char *ifname;
	size_t reply_len;
	struct wpa_ctrl *ctrl;
	int ret;

	if (!iface) {
		wpa_printf(MSG_ERROR, "Backhaul STA disconnect failed: iface is NULL");
		return -1;
	}

	ifname = hostapd_ubus_bhsta_ifname(iface);
	if (!ifname) {
		wpa_printf(MSG_ERROR,
			   "Backhaul STA disconnect failed: missing ifname iface=%s",
			   iface->phy);
		return -1;
	}

	ret = os_snprintf(ctrl_path, sizeof(ctrl_path),
			  "/var/run/wpa_supplicant/%s", ifname);
	if (os_snprintf_error(sizeof(ctrl_path), ret)) {
		wpa_printf(MSG_ERROR,
			   "Backhaul STA disconnect failed: ctrl path too long ifname=%s",
			   ifname);
		free(ifname);
		return -1;
	}

	ctrl = wpa_ctrl_open(ctrl_path);
	if (!ctrl) {
		wpa_printf(MSG_ERROR,
			   "Backhaul STA disconnect failed: cannot open %s",
			   ctrl_path);
		free(ifname);
		return -1;
	}

	reply_len = sizeof(reply) - 1;
	ret = wpa_ctrl_request(ctrl, "DISCONNECT", 2, reply, &reply_len, NULL);
	wpa_ctrl_close(ctrl);
	free(ifname);
	if (ret < 0) {
		/*
		 * A timeout here is expected when the STA is already in the
		 * process of disconnecting (e.g. AFC expiry triggered a kernel
		 * deauth before this command arrived). Treat it as success.
		 */
		wpa_printf(MSG_INFO,
			   "Backhaul STA disconnect: control request timed out, STA likely already disconnecting");
		return 0;
	}

	reply[reply_len] = '\0';
	if (os_strncmp(reply, "FAIL", 4) == 0) {
		wpa_printf(MSG_INFO,
			   "Backhaul STA disconnect: supplicant rejected request, STA likely already disconnected");
		return 0;
	}

	if (ret)
		return ret;

	wpa_printf(MSG_INFO, "Backhaul STA disconnect requested");
	return 0;
}
#endif

static int configured_fixed_chan_to_freq(struct hostapd_iface *iface)
{
	int freq, i, j;

	if (!iface->conf->channel)
		return 0;
	if (iface->conf->op_class) {
		freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
					      iface->conf->channel);
		if (freq < 0) {
			wpa_printf(MSG_INFO,
				   "Could not convert op_class %u channel %u to operating frequency",
				   iface->conf->op_class, iface->conf->channel);
			return -1;
		}
		iface->freq = freq;
		return 0;
	}

	/* Old configurations using only 2.4/5/60 GHz bands may not specify the
	 * op_class parameter. Select a matching channel from the configured
	 * mode using the channel parameter for these cases.
	 */
	for (j = 0; j < iface->num_hw_features; j++) {
		struct hostapd_hw_modes *mode = &iface->hw_features[j];

		if (iface->conf->hw_mode != HOSTAPD_MODE_IEEE80211ANY &&
		    iface->conf->hw_mode != mode->mode)
			continue;
		for (i = 0; i < mode->num_channels; i++) {
			struct hostapd_channel_data *chan = &mode->channels[i];

			if (chan->chan == iface->conf->channel &&
			    !is_6ghz_freq(chan->freq)) {
				iface->freq = chan->freq;
				return 0;
			}
		}
	}

	wpa_printf(MSG_INFO, "Could not determine operating frequency");
	return -1;
}

/**
 * hostapd_handle_regchannel_update - Handle channel list update.
 *
 * Initiate a channel change upon receiving the NL8011_WIPHY_REG_CHANGE event.
 * If afc_channel_change_pending is set (set only for 6 GHz iface which
 * received AFC power event), then invoke random channel and switch to it.
 * If not, then invoke no_ir_channel_list_updated() to handle
 * NO_IR channel list update.
 *
 * Country change detection, channel re-selection, and regdom restore are
 * delegated to the extn layer via hostapd_handle_regchannel_update_extn().
 * The extn returns non-zero when it has fully handled the event (country
 * change or forced-down recovery); in that case the NO_IR / AFC paths below
 * are skipped.
 *
 * @param iface: Pointer to hostapd interface data
 * @param ctx:   Pointer to context
 *
 * Return: 0 on success, negative value on failure
 */
static int hostapd_handle_regchannel_update(struct hostapd_iface *iface,
					    void *ctx)
{
	int ret;
#ifdef CONFIG_QCN_EXTN
	bool afc_regd = false;

	if (iface->iface_extn.is_waiting_for_afc_regd) {
		afc_regd = true;
		iface->iface_extn.is_waiting_for_afc_regd = false;
	}
#endif /* CONFIG_QCN_EXTN */

	ret = hostapd_get_hw_features(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to get hardware features (%d)", ret);
		return ret;
	}

	if (!iface->freq) {
		ret = configured_fixed_chan_to_freq(iface);
		if (ret) {
			wpa_printf(MSG_ERROR, "Configured channel is not valid (%d)",
				   ret);
			return ret;
		}
	}

	ret = hostapd_select_hw_mode(iface);
	if (ret < 0 && !iface->is_no_ir) {
		wpa_printf(MSG_ERROR, "Failed to select hardware mode (%d)", ret);
#ifdef CONFIG_QCN_EXTN
		ret = hostapd_handle_regchannel_update_extn(iface, afc_regd);
		return ret > 0 ? 0 : ret;
#else
		return ret;
#endif /* CONFIG_QCN_EXTN */
	}

	if (ret == 1)
		return 0; /* ACS will run and complete asynchronously */

	ret = hostapd_set_current_hw_info(iface, iface->freq);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to set current hw info (%d)", ret);
#ifdef CONFIG_QCN_EXTN
		hostapd_regdom_bringup_failed_extn(iface);
#endif /* CONFIG_QCN_EXTN */
		return ret;
	}

#ifdef CONFIG_QCN_EXTN
	ret = hostapd_handle_regchannel_update_extn(iface, afc_regd);
	if (ret)
		return ret > 0 ? 0 : ret;
#endif /* CONFIG_QCN_EXTN */

	wpa_printf(MSG_DEBUG, "Handling NOIR Channel List Update");
	ret = hostapd_no_ir_channel_list_updated(iface);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "Failed to handle NO IR Chan List Update (%d)", ret);
		return ret;
	}

	if (iface->is_afc_channel_change_pending) {
		wpa_printf(MSG_DEBUG, "Handling AFC channel change");
		ret = hostapd_handle_afc_channel_change(iface);
	}

	return ret;
}

#if defined(CONFIG_QCN_EXTN) && defined(HOSTAPD)
static int
hostapd_run_pending_repeater_afc_power_sync(struct hostapd_iface *iface,
					    void *ctx)
{
	enum hostapd_afc_power_sync_result sync_result;

	(void) ctx;

	if (!iface || !iface->is_afc_repeater_power_sync_pending)
		return 0;

	iface->is_afc_repeater_power_sync_pending = false;

	if (!hostapd_iface_has_connected_backhaul_sta(iface)) {
		wpa_printf(MSG_INFO,
			   "AFC repeater power sync skipped: backhaul STA not connected iface=%s",
			   iface->phy);
		return 0;
	}

	if (!iface->is_afc_power_event_received)
		sync_result = hostapd_force_afc_non_sp_power_mode(iface);
	else
		sync_result = hostapd_sync_current_afc_power_mode(iface, true);
	switch (sync_result) {
	case HOSTAPD_AFC_PWR_SYNC_UPDATED:
		wpa_printf(MSG_INFO,
			   "AFC repeater power sync complete: update started iface=%s",
			   iface->phy);
		break;
	case HOSTAPD_AFC_PWR_SYNC_NOOP:
		wpa_printf(MSG_INFO,
			   "AFC repeater power sync complete: no update needed iface=%s",
			   iface->phy);
		break;
	case HOSTAPD_AFC_PWR_SYNC_DEFERRED:
		/*
		 * Transient condition (CSA or pending power switch).
		 * Re-arm the pending flag so this retries on the next
		 * REGDOM_SET_BY_DRIVER event.
		 */
		iface->is_afc_repeater_power_sync_pending = true;
		wpa_printf(MSG_INFO,
			   "AFC repeater power sync deferred: will retry iface=%s",
			   iface->phy);
		break;
	case HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT:
		wpa_printf(MSG_ERROR,
			   "AFC repeater power sync failed: no valid non-SP fallback, disconnect backhaul STA iface=%s",
			   iface->phy);
		if (hostapd_disconnect_backhaul_sta(iface))
			wpa_printf(MSG_ERROR,
				   "AFC repeater power sync failed: backhaul disconnect failed iface=%s",
				   iface->phy);
		break;
	case HOSTAPD_AFC_PWR_SYNC_ERROR:
	default:
		/*
		 * Hard internal failure; no retry possible.
		 * Disconnect backhaul STA so repeater can re-associate cleanly.
		 */
		wpa_printf(MSG_ERROR,
			   "AFC repeater power sync failed: internal error, disconnect backhaul STA iface=%s",
			   iface->phy);
		if (hostapd_disconnect_backhaul_sta(iface))
			wpa_printf(MSG_ERROR,
				   "AFC repeater power sync failed: backhaul disconnect failed iface=%s",
				   iface->phy);
		break;
	}

	return 0;
}
#endif

void hostapd_channel_list_updated(struct hostapd_iface *iface, int initiator)
{
	if (initiator == REGDOM_SET_BY_DRIVER) {
		const char *phy_name = hostapd_drv_get_radio_name(iface->bss[0]);

		if (!phy_name)
			return;

		wpa_printf(MSG_DEBUG, "Reg change event received for phy %s through %s",
			   phy_name, iface->phy);
		hostapd_for_each_iface_on_phy(iface->interfaces, phy_name,
					      hostapd_handle_regchannel_update, NULL);
#if defined(CONFIG_QCN_EXTN) && defined(HOSTAPD)
		hostapd_for_each_iface_on_phy(iface->interfaces, phy_name,
					      hostapd_run_pending_repeater_afc_power_sync,
					      NULL);
#endif /* CONFIG_QCN_EXTN */
		return;
	}

	if (!iface->wait_channel_update || initiator != REGDOM_SET_BY_USER)
		return;

	wpa_printf(MSG_DEBUG, "Channel list updated - continue setup");
	eloop_cancel_timeout(channel_list_update_timeout, iface, NULL);
	setup_interface2(iface);
}

static int setup_interface(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	size_t i;

	/*
	 * It is possible that setup_interface() is called after the interface
	 * was disabled etc., in which case driver_ap_teardown is possibly set
	 * to 1. Clear it here so any other key/station deletion, which is not
	 * part of a teardown flow, would also call the relevant driver
	 * callbacks.
	 */
	iface->driver_ap_teardown = 0;

	if (!iface->phy[0]) {
		const char *phy = hostapd_drv_get_radio_name(hapd);
		if (phy) {
			wpa_printf(MSG_DEBUG, "phy: %s", phy);
			os_strlcpy(iface->phy, phy, sizeof(iface->phy));
		}
	}

	/*
	 * Make sure that all BSSes get configured with a pointer to the same
	 * driver interface.
	 */
	for (i = 1; i < iface->num_bss; i++) {
		iface->bss[i]->driver = hapd->driver;
		iface->bss[i]->drv_priv = hapd->drv_priv;
	}

	if (hostapd_validate_bssid_configuration(iface))
		return -1;

#ifdef CONFIG_QCN_EXTN
	/*
	 * Update acs_success to 1 if fixed channel is configured
	 */
	if (hapd->iface && hapd->iface->conf &&
	    hapd->iface->conf->conf_extn.ind_rptr &&
	    hapd->iface->conf->channel)
		hapd->iface->iface_extn.acs_success = 1;
#endif /* CONFIG_QCN_EXTN */

	/*
	 * Initialize control interfaces early to allow external monitoring of
	 * channel setup operations that may take considerable amount of time
	 * especially for DFS cases.
	 */
	if (start_ctrl_iface(iface))
		return -1;

	if (hapd->iconf->country[0] && hapd->iconf->country[1]) {
		char country[4], previous_country[4];

		hostapd_set_state(iface, HAPD_IFACE_COUNTRY_UPDATE);
		if (hostapd_get_country(hapd, previous_country) < 0)
			previous_country[0] = '\0';

		os_memcpy(country, hapd->iconf->country, 3);
		country[3] = '\0';
		if (hostapd_set_country(hapd, country) < 0) {
			wpa_printf(MSG_ERROR, "Failed to set country code");
			return -1;
		}

		wpa_printf(MSG_DEBUG, "Previous country code %s, new country code %s",
			   previous_country, country);

		if (os_strncmp(previous_country, country, 2) != 0) {
			wpa_printf(MSG_DEBUG, "Continue interface setup after channel list update");
			iface->wait_channel_update = 1;
			eloop_register_timeout(5, 0,
					       channel_list_update_timeout,
					       iface, NULL);
			return 0;
		}
	}

	return setup_interface2(iface);
}


#ifdef CONFIG_QCN_EXTN
int configured_fixed_chan_to_freq_helper(struct hostapd_iface *iface)
{
	return configured_fixed_chan_to_freq(iface);
}
#endif

static void hostapd_set_6ghz_sec_chan(struct hostapd_iface *iface)
{
	int bw;

	if (!is_6ghz_op_class(iface->conf->op_class))
		return;

	bw = op_class_to_bandwidth(iface->conf->op_class);
	/* Assign the secondary channel if absent in config for
	 * bandwidths > 20 MHz */
	if (bw >= 40 && !iface->conf->secondary_channel) {
		if (((iface->conf->channel - 1) / 4) % 2)
			iface->conf->secondary_channel = -1;
		else
			iface->conf->secondary_channel = 1;
	}
}

static int setup_interface2(struct hostapd_iface *iface)
{
	struct hostapd_multi_hw_info *hw_info;
	bool is_mesh = false;
	int i;
#ifdef CONFIG_MESH
	is_mesh = iface->mconf ? true : false;
#endif
	iface->wait_channel_update = 0;
	iface->is_afc_channel_change_pending = false;
	iface->is_afc_repeater_power_sync_pending = false;
	iface->is_no_ir = false;
	iface->power_mode_6ghz_before_change = -1;
	iface->rnr_psd = CHAN_MIN_TX_POWER;
	hostapd_clear_local_tpe(iface);

	if (hostapd_get_hw_features(iface)) {
		/* Not all drivers support this yet, so continue without hw
		 * feature data. */
	} else {
		int ret;

		if (iface->conf->acs && !iface->is_ch_switch_dfs) {
			iface->freq = 0;
			iface->conf->channel = 0;
		}
		iface->is_ch_switch_dfs = false;

		ret = configured_fixed_chan_to_freq(iface);
		if (ret < 0)
			goto fail;

		if (iface->conf->op_class) {
			enum oper_chan_width ch_width;

			ch_width = op_class_to_ch_width(iface->conf->op_class);
			hostapd_set_oper_chwidth(iface->conf, ch_width);
			hostapd_set_6ghz_sec_chan(iface);
		}

		ret = hostapd_select_hw_mode(iface);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "Could not select hw_mode and "
				   "channel. (%d)", ret);
			goto fail;
		}
		if (ret == 1) {
			wpa_printf(MSG_DEBUG, "Interface initialization will be completed in a callback (ACS)");
			return 0;
		}
		ret = hostapd_check_edmg_capab(iface);
		if (ret < 0)
			goto fail;
		ret = hostapd_check_he_6ghz_capab(iface);
		if (ret < 0)
			goto fail;

		if (!is_mesh) {
			hw_info = hostapd_get_current_hw_info(iface, iface->freq);

			if (hw_info &&
			    !hostapd_ucode_update_radio_mask(iface->conf->bss[0]->iface,
							     hw_info->hw_idx))
				wpa_printf(MSG_ERROR,
					   "Failed to update radio mask for %s",
					   iface->conf->bss[0]->iface);
		}

		ret = hostapd_check_ht_capab(iface);
		if (ret < 0)
			goto fail;
		for (i = 0; i < iface->num_bss; i++) {
			if (hostapd_validate_bss_capab(iface->bss[i]) < 0) {
				wpa_printf(MSG_ERROR,
					   "BSS capability validation failed for %s",
					   iface->bss[i]->conf->iface);
				goto fail;
			}
		}
		if (ret == 1) {
			wpa_printf(MSG_DEBUG, "Interface initialization will "
				   "be completed in a callback");
			return 0;
		}

		if (iface->conf->ieee80211h)
			wpa_printf(MSG_DEBUG, "DFS support is enabled");
	}
	return hostapd_setup_interface_complete(iface, 0);

fail:
	if (iface->is_no_ir) {
		/* If AP is in NO_IR state, it can be reenabled by the driver
		 * regulatory update and EVENT_CHANNEL_LIST_CHANGED.
		 * Note: is_no_ir is only set for 6 GHz NO_IR channels; 5 GHz
		 * unavailable channels return HAPD_CHAN_INVALID (not NO_IR). */
		hostapd_set_state(iface, HAPD_IFACE_NO_IR);
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
		return 0;
	}

#ifdef CONFIG_QCN_EXTN
	/*
	 * Bring-up failed — the configured channel may be regulatory-disabled
	 * in the current country. Park to NO_IR and mark is_regdom_forced_down
	 * so the next REGULATORY event triggers full channel selection and come
	 * back up. Without this, the iface would stay DISABLED with no recovery.
	 */
	if (!regdom_subchans_valid(iface) &&
	    hostapd_regdom_bringup_failed_extn(iface))
		return 0;
#endif /* CONFIG_QCN_EXTN */

	hostapd_set_state(iface, HAPD_IFACE_DISABLED);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
	if (iface->interfaces && iface->interfaces->terminate_on_error)
		eloop_terminate();
	return -1;
}


#ifdef CONFIG_FST

static const u8 * fst_hostapd_get_bssid_cb(void *ctx)
{
	struct hostapd_data *hapd = ctx;

	return hapd->own_addr;
}


static void fst_hostapd_get_channel_info_cb(void *ctx,
					    enum hostapd_hw_mode *hw_mode,
					    u8 *channel)
{
	struct hostapd_data *hapd = ctx;

	*hw_mode = ieee80211_freq_to_chan(hapd->iface->freq, channel);
}


static int fst_hostapd_get_hw_modes_cb(void *ctx,
				       struct hostapd_hw_modes **modes)
{
	struct hostapd_data *hapd = ctx;

	*modes = hapd->iface->hw_features;
	return hapd->iface->num_hw_features;
}


static void fst_hostapd_set_ies_cb(void *ctx, const struct wpabuf *fst_ies)
{
	struct hostapd_data *hapd = ctx;

	if (hapd->iface->fst_ies != fst_ies) {
		hapd->iface->fst_ies = fst_ies;
		if (ieee802_11_set_beacon(hapd))
			wpa_printf(MSG_WARNING, "FST: Cannot set beacon");
	}
}


static int fst_hostapd_send_action_cb(void *ctx, const u8 *da,
				      struct wpabuf *buf)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_send_action(hapd, hapd->iface->freq, 0, da,
				       wpabuf_head(buf), wpabuf_len(buf));
}


static const struct wpabuf * fst_hostapd_get_mb_ie_cb(void *ctx, const u8 *addr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);

	return sta ? sta->mb_ies : NULL;
}


static void fst_hostapd_update_mb_ie_cb(void *ctx, const u8 *addr,
					const u8 *buf, size_t size)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);

	if (sta) {
		struct mb_ies_info info;

		if (!mb_ies_info_by_ies(&info, buf, size)) {
			wpabuf_free(sta->mb_ies);
			sta->mb_ies = mb_ies_by_info(&info);
		}
	}
}


static const u8 * fst_hostapd_get_sta(struct fst_get_peer_ctx **get_ctx,
				      bool mb_only)
{
	struct sta_info *s = (struct sta_info *) *get_ctx;

	if (mb_only) {
		for (; s && !s->mb_ies; s = s->next)
			;
	}

	if (s) {
		*get_ctx = (struct fst_get_peer_ctx *) s->next;

		return s->addr;
	}

	*get_ctx = NULL;
	return NULL;
}


static const u8 * fst_hostapd_get_peer_first(void *ctx,
					     struct fst_get_peer_ctx **get_ctx,
					     bool mb_only)
{
	struct hostapd_data *hapd = ctx;

	*get_ctx = (struct fst_get_peer_ctx *) hapd->sta_list;

	return fst_hostapd_get_sta(get_ctx, mb_only);
}


static const u8 * fst_hostapd_get_peer_next(void *ctx,
					    struct fst_get_peer_ctx **get_ctx,
					    bool mb_only)
{
	return fst_hostapd_get_sta(get_ctx, mb_only);
}


void fst_hostapd_fill_iface_obj(struct hostapd_data *hapd,
				struct fst_wpa_obj *iface_obj)
{
	os_memset(iface_obj, 0, sizeof(*iface_obj));
	iface_obj->ctx = hapd;
	iface_obj->get_bssid = fst_hostapd_get_bssid_cb;
	iface_obj->get_channel_info = fst_hostapd_get_channel_info_cb;
	iface_obj->get_hw_modes = fst_hostapd_get_hw_modes_cb;
	iface_obj->set_ies = fst_hostapd_set_ies_cb;
	iface_obj->send_action = fst_hostapd_send_action_cb;
	iface_obj->get_mb_ie = fst_hostapd_get_mb_ie_cb;
	iface_obj->update_mb_ie = fst_hostapd_update_mb_ie_cb;
	iface_obj->get_peer_first = fst_hostapd_get_peer_first;
	iface_obj->get_peer_next = fst_hostapd_get_peer_next;
}

#endif /* CONFIG_FST */

#ifdef CONFIG_OWE

static int hostapd_owe_iface_iter(struct hostapd_iface *iface, void *ctx)
{
	struct hostapd_data *hapd = ctx;
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];

		if (os_strcmp(hapd->conf->owe_transition_ifname,
			      bss->conf->iface) != 0)
			continue;

		wpa_printf(MSG_DEBUG,
			   "OWE: ifname=%s found transition mode ifname=%s BSSID "
			   MACSTR " SSID %s",
			   hapd->conf->iface, bss->conf->iface,
			   MAC2STR(bss->own_addr),
			   wpa_ssid_txt(bss->conf->ssid.ssid,
					bss->conf->ssid.ssid_len));
		if (!bss->conf->ssid.ssid_set || !bss->conf->ssid.ssid_len ||
		    is_zero_ether_addr(bss->own_addr))
			continue;

		os_memcpy(hapd->conf->owe_transition_bssid, bss->own_addr,
			  ETH_ALEN);
		os_memcpy(hapd->conf->owe_transition_ssid,
			  bss->conf->ssid.ssid, bss->conf->ssid.ssid_len);
		hapd->conf->owe_transition_ssid_len = bss->conf->ssid.ssid_len;
		wpa_printf(MSG_DEBUG,
			   "OWE: Copied transition mode information");
		return 1;
	}

	return 0;
}


int hostapd_owe_trans_get_info(struct hostapd_data *hapd)
{
	if (hapd->conf->owe_transition_ssid_len > 0 &&
	    !is_zero_ether_addr(hapd->conf->owe_transition_bssid))
		return 0;

	/* Find transition mode SSID/BSSID information from a BSS operated by
	 * this hostapd instance. */
	if (!hapd->iface->interfaces ||
	    !hapd->iface->interfaces->for_each_interface)
		return hostapd_owe_iface_iter(hapd->iface, hapd);
	else
		return hapd->iface->interfaces->for_each_interface(
			hapd->iface->interfaces, hostapd_owe_iface_iter, hapd);
}


static int hostapd_owe_iface_iter2(struct hostapd_iface *iface, void *ctx)
{
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		int res;

		if (!bss->conf->owe_transition_ifname[0])
			continue;
		if (bss->iface->state != HAPD_IFACE_ENABLED) {
			wpa_printf(MSG_DEBUG,
				   "OWE: Interface %s state %s - defer beacon update",
				   bss->conf->iface,
				   hostapd_state_text(bss->iface->state));
			continue;
		}
		res = hostapd_owe_trans_get_info(bss);
		if (res == 0)
			continue;
		wpa_printf(MSG_DEBUG,
			   "OWE: Matching transition mode interface enabled - update beacon data for %s",
			   bss->conf->iface);
		ieee802_11_set_beacon(bss);
	}

	return 0;
}

#endif /* CONFIG_OWE */


static void hostapd_owe_update_trans(struct hostapd_iface *iface)
{
#ifdef CONFIG_OWE
	/* Check whether the enabled BSS can complete OWE transition mode
	 * configuration for any pending interface. */
	if (!iface->interfaces ||
	    !iface->interfaces->for_each_interface)
		hostapd_owe_iface_iter2(iface, NULL);
	else
		iface->interfaces->for_each_interface(
			iface->interfaces, hostapd_owe_iface_iter2, NULL);
#endif /* CONFIG_OWE */
}


static void hostapd_interface_setup_failure_handler(void *eloop_ctx,
						    void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	struct hostapd_data *hapd;

	if (iface->num_bss < 1 || !iface->bss || !iface->bss[0])
		return;
	hapd = iface->bss[0];
	if (hapd->setup_complete_cb)
		hapd->setup_complete_cb(hapd->setup_complete_cb_ctx);
}

/**
 * hostapd_is_pwr_mode_chans_available - Check if at least one tx-able
 * 6 GHz channel is available for the given power mode (a channel is
 * non-tx-able if the NO_IR flag is set on it).
 * @iface: Pointer to hostapd interface data
 * @pwr_mode: NL80211 regulatory AP power mode index
 * Return: true if channels are available, false otherwise
 */
static bool
hostapd_is_pwr_mode_chans_available(struct hostapd_iface *iface,
				    enum nl80211_regulatory_power_modes pwr_mode)
{
	int i;
	struct hostapd_hw_modes *mode = NULL;
	struct hostapd_channel_data *pwr_mode_chan_list;
	u8 num_6ghz_chans;

	if (!iface->num_hw_features) {
		wpa_printf(MSG_ERROR, "No hw features");
		return false;
	}

	for (i = 0; i < iface->num_hw_features; i++) {
		if (iface->hw_features[i].is_6ghz) {
			mode = &iface->hw_features[i];
			break;
		}
	}
	if (!mode) {
		wpa_printf(MSG_ERROR, "No 6 GHz mode");
		return false;
	}

	num_6ghz_chans = mode->channels_6ghz.num_channels_6ghz[pwr_mode];
	pwr_mode_chan_list = mode->channels_6ghz.chans_6ghz[pwr_mode];
	if (!num_6ghz_chans || !pwr_mode_chan_list) {
		wpa_printf(MSG_ERROR, "No 6 GHz channels for power mode %d",
			   pwr_mode);
		return false;
	}

	for (i = 0; i < num_6ghz_chans; i++) {
		if (!(pwr_mode_chan_list[i].flag & HOSTAPD_CHAN_NO_IR) &&
		    !(pwr_mode_chan_list[i].flag & HOSTAPD_CHAN_DISABLED)) {
			wpa_printf(MSG_DEBUG, "Channel available: %d MHz (pwr_mode %d)",
				   pwr_mode_chan_list[i].freq, pwr_mode);
			return true;
		}
	}
	return false;
}

static bool
hostapd_is_sp_chans_available(struct hostapd_iface *iface)
{
	return hostapd_is_pwr_mode_chans_available(iface, NL80211_REG_AP_SP);
}

bool
hostapd_is_lpi_chans_available(struct hostapd_iface *iface)
{
	return hostapd_is_pwr_mode_chans_available(iface, NL80211_REG_AP_LPI);
}

bool
hostapd_is_indoor_sp_pwr_mode(struct hostapd_data *hapd)
{
	return hapd->iconf->he_6ghz_reg_pwr_type == HE_REG_INFO_6GHZ_AP_TYPE_SP &&
	       hapd->iconf->enable_6ghz_composite_ap &&
	       hostapd_is_lpi_chans_available(hapd->iface);
}

/**
 * hostapd_fetch_afc_power_event - Fetch AFC power event from the driver
 * @hapd: Pointer to hostapd BSS data
 * Return: 0 on success, -ve value on failure
 */
static int hostapd_fetch_afc_power_event(struct hostapd_data *hapd)
{
#ifdef NEED_AP_MLME
	uint8_t radio_idx = NL80211_WIPHY_RADIO_ID_MAX;
	int ret = -1;

	if (hapd->iface->num_multi_hws) {
		if (hapd->iface->current_hw_info) {
			/* current_hw_info already set — use it directly */
			radio_idx = hapd->iface->current_hw_info->hw_idx;
		} else if (hapd->iface->freq != 0) {
			/* freq known — look up hw_info without modifying iface state */
			struct hostapd_multi_hw_info *hw_info;

			hw_info = hostapd_get_current_hw_info(hapd->iface,
							     hapd->iface->freq);
			if (!hw_info) {
				wpa_printf(MSG_ERROR,
					   "No multi_hw_info match for freq=%d",
					   hapd->iface->freq);
				return ret;
			}
			radio_idx = hw_info->hw_idx;
		} else if (hapd->iface->conf->radio_idx >= 0) {
			/* freq=0 (ACS): use radio_idx from conf */
			unsigned int i;

			for (i = 0; i < hapd->iface->num_multi_hws; i++) {
				if ((int)hapd->iface->multi_hw_info[i].hw_idx ==
				    hapd->iface->conf->radio_idx) {
					radio_idx =
						hapd->iface->multi_hw_info[i].hw_idx;
					break;
				}
			}
			if (radio_idx == NL80211_WIPHY_RADIO_ID_MAX) {
				wpa_printf(MSG_ERROR,
					   "No multi_hw_info match for radio_idx=%d",
					   hapd->iface->conf->radio_idx);
				return ret;
			}
		} else {
			wpa_printf(MSG_ERROR,
				   "No current_hw_info, freq=0, no radio_idx fallback");
			return ret;
		}
	}
	wpa_printf(MSG_DEBUG, "Fetching AFC power event from the driver for idx: %d\n", radio_idx);
	ret = hostapd_drv_fetch_afc_power_event(hapd, radio_idx);
	if (ret)
		wpa_printf(MSG_ERROR, "Failed to fetch AFC power event from driver: %d", ret);

	return ret;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


void hostapd_check_get_afc_details(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;

	if (iface->is_afc_power_event_received)
		return;

	if (!is_6ghz_op_class(iface->conf->op_class)) {
		wpa_printf(MSG_DEBUG,
			   "Skip AFC power fetch on non-6 GHz interface (op_class=%u)",
			   iface->conf->op_class);
		return;
	}

	if (!hostapd_is_sp_chans_available(iface)) {
		wpa_printf(MSG_DEBUG, "No SP Channels available");
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "SP channels available. Fetch AFC Payload from the driver");

	if (hostapd_fetch_afc_power_event(hapd))
		wpa_printf(MSG_DEBUG, "Failed to fetch AFC payload");
}


static int hostapd_setup_interface_complete_sync(struct hostapd_iface *iface,
						 int err)
{
	struct hostapd_data *hapd = iface->bss[0];
	size_t j;
	u8 *prev_addr;
	int delay_apply_cfg = 0;
	int res_dfs_offload = 0;
#ifdef CONFIG_QCN_EXTN
	bool skip_cac_start = false;
#endif

	if (err)
		goto fail;

#ifdef CONFIG_QCN_EXTN
	hostapd_iface_init_extn(iface);
	hostapd_ignorecac_init_iface_extn(iface);
#endif /* CONFIG_QCN_EXTN */

	hostapd_ubus_add_iface(iface);
	wpa_printf(MSG_DEBUG, "Completing interface initialization");
	if (iface->freq) {
#ifdef NEED_AP_MLME
		int res;
#endif /* NEED_AP_MLME */

		wpa_printf(MSG_DEBUG, "Mode: %s  Channel: %d  "
			   "Frequency: %d MHz",
			   hostapd_hw_mode_txt(iface->conf->hw_mode),
			   iface->conf->channel, iface->freq);

		if (hostapd_set_current_hw_info(iface, iface->freq)) {
			wpa_printf(MSG_ERROR,
				   "Failed to set current hardware info");
			goto fail;
		}

#ifdef CONFIG_QCN_EXTN
		/*
		 * If vendor BSSID was deferred (e.g., multiple radios and no
		 * channel at driver init), fetch and apply it now.
		 */
		if (hapd && is_zero_ether_addr(hapd->conf->bssid) &&
		    hapd->iconf->use_driver_vendor_addr) {
			if (hostapd_drv_fetch_and_set_vendor_bssid_extn(hapd))
				wpa_printf(MSG_DEBUG,
					   "fetch and set vendor BSSID failed");
		}
#endif /* CONFIG_QCN_EXTN */

		if (hostapd_setup_monitor_iface(iface) < 0)
			wpa_printf(MSG_WARNING,
				   "Monitor iface: Setup failed, continuing without monitor interface");

#ifdef NEED_AP_MLME
		/* Handle DFS only if it is not offloaded to the driver */
		if (!(iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)) {
			if (!hostapd_dfs_intercac_boot(iface)
#ifdef CONFIG_QCN_EXTN
			    && !hostapd_bootup_cac_start_extn(iface)
#endif /* CONFIG_QCN_EXTN */
			   )
			{
				/* Check DFS */
				res = hostapd_handle_dfs(iface);
				if (res <= 0) {
					if (res < 0)
						goto fail;
					return res;
				}
			}
		} else {
			/* If DFS is offloaded to the driver */
			res_dfs_offload = hostapd_handle_dfs_offload(iface);
			if (res_dfs_offload <= 0) {
				if (res_dfs_offload < 0)
					goto fail;
			} else {
				wpa_printf(MSG_DEBUG,
					   "Proceed with AP/channel setup");
				/*
				 * If this is a DFS channel, move to completing
				 * AP setup.
				 */
					if (res_dfs_offload == 1)
						goto dfs_offload;
				}
			}
#endif /* NEED_AP_MLME */

#ifdef CONFIG_QCN_EXTN
		if (hostapd_ignorecac_should_skip_cac_extn(iface)) {
			skip_cac_start = true;
			wpa_printf(MSG_DEBUG,
				   "%s: skip_cac_start set to true",
				   __func__);
		}
#endif /* CONFIG_QCN_EXTN */

		if (iface->radar_bit_pattern) {
			hapd->iconf->punct_bitmap |=
				iface->radar_bit_pattern;
		}

#ifdef CONFIG_MESH
		if (iface->mconf != NULL) {
			wpa_printf(MSG_DEBUG,
				   "%s: Mesh configuration will be applied while joining the mesh network",
				   iface->bss[0]->conf->iface);
			delay_apply_cfg = 1;
		}
#endif /* CONFIG_MESH */

#ifdef CONFIG_IEEE80211BN
		if (iface->conf->npca_enable)
			hostapd_npca_auto_select(iface);
#endif /* CONFIG_IEEE80211BN */

		if (is_6ghz_freq(iface->freq) && iface->conf->enable_best_power_mode) {
			u8 best_power_mode;
			enum chan_width ch_width;
			u8 center_chan_no;
			u16 center_freq;

			ch_width = hostapd_get_chan_width_from_oper_chan_width(iface->conf);
			center_chan_no = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
			center_freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
							     center_chan_no);
			hostapd_apply_6ghz_dynamic_puncturing(iface);
			best_power_mode = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
			if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
				iface->conf->he_6ghz_reg_pwr_type = best_power_mode;
				wpa_printf(MSG_INFO,
					   "%s: Best power mode for Freq %d is %d",
					   __func__,
					   iface->freq, best_power_mode);
				iface->conf->cur_chan_eirp =
					hostapd_get_eirp_pwr(iface,
							     iface->freq,
							     center_freq,
							     channel_width_to_int(ch_width),
							     iface->conf->punct_bitmap,
							     best_power_mode,
							     false,
							     NL80211_REG_NUM_POWER_MODES,
							     false);
			}
		}

		if (!delay_apply_cfg &&
		    hostapd_set_freq(hapd, hapd->iconf->hw_mode, iface->freq,
				     hapd->iconf->channel,
				     hapd->iconf->enable_edmg,
				     hapd->iconf->edmg_channel,
				     hapd->iconf->ieee80211n,
				     hapd->iconf->ieee80211ac,
				     hapd->iconf->ieee80211ax,
				     hapd->iconf->ieee80211be,
				     hapd->iconf->ieee80211bn,
				     hapd->iconf->secondary_channel,
				     hostapd_get_oper_chwidth(hapd->iconf),
				     hostapd_get_oper_centr_freq_seg0_idx(
					     hapd->iconf),
				     hostapd_get_oper_centr_freq_seg1_idx(
					     hapd->iconf),
#ifdef CONFIG_QCN_EXTN
				     skip_cac_start,
#endif
				     hapd->iconf->bandwidth_device,
				     hapd->iconf->center_freq_device)) {
			wpa_printf(MSG_ERROR, "Could not set channel for "
				   "kernel driver");
			goto fail;
		}
	}

	if (hapd->iconf->rts_threshold >= -1 &&
	    hostapd_set_rts(hapd, hapd->iconf->rts_threshold) &&
	    hapd->iconf->rts_threshold >= -1) {
		wpa_printf(MSG_ERROR, "Could not set RTS threshold for "
			   "kernel driver");
		goto fail;
	}

	if (hapd->iconf->fragm_threshold >= -1 &&
	    hostapd_set_frag(hapd, hapd->iconf->fragm_threshold) &&
	    hapd->iconf->fragm_threshold != -1) {
		wpa_printf(MSG_ERROR, "Could not set fragmentation threshold "
			   "for kernel driver");
		goto fail;
	}

	prev_addr = hapd->own_addr;

#ifdef CONFIG_IEEE80211BN
	mapc_iface_init(iface);
#endif /* CONFIG_IEEE80211BN */

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (j)
			os_memcpy(hapd->own_addr, prev_addr, ETH_ALEN);
		if (hapd->reenable == REENABLE_REUSE_LINK) {
			prev_addr = hapd->own_addr;
			continue;
		}
		if (hostapd_setup_bss(hapd, j == 0, !iface->conf->mbssid)) {
			for (;;) {
				hapd = iface->bss[j];
				hostapd_bss_deinit_no_free(hapd);
				hostapd_bss_link_deinit(hapd);
				hostapd_free_hapd_data(hapd);
				if (j == 0)
					break;
				j--;
			}
			goto fail;
		}
		if (is_zero_ether_addr(hapd->conf->bssid))
			prev_addr = hapd->own_addr;
	}

	if (hapd->iconf->mbssid) {
		for (j = 0; hapd->iconf->mbssid && j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			if (hostapd_start_beacon(hapd, true)) {
				/*
				 * Start cleanup from num_bss - 1 (not just j)
				 * so that BSSs above j which were already set
				 * up by hostapd_setup_bss() but not yet
				 * reached by hostapd_start_beacon() are also
				 * cleaned up. Without this, their wpa_auth
				 * allocations are leaked and tx_bss_auth
				 * pointers become dangling references into
				 * freed memory, causing use-after-free crashes
				 * (e.g. when the GTK rekey eloop timer fires).
				 */
				j = iface->num_bss - 1;
				for (;;) {
					hapd = iface->bss[j];
					hostapd_bss_deinit_no_free(hapd);
					hostapd_bss_link_deinit(hapd);
					hostapd_free_hapd_data(hapd);
					if (j == 0)
						break;
					j--;
				}
				goto fail;
			}
		}
	}

	hapd = iface->bss[0];

	hostapd_tx_queue_params(iface);

	ap_list_init(iface);

	hostapd_set_acl(hapd);

	if (hostapd_driver_commit(hapd) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
			   "configuration", __func__);
		goto fail;
	}

#ifdef CONFIG_QCN_EXTN
	if (iface->iface_extn.dcs_in_progress)
		hostapd_dcs_restore_extn(iface, "AP restart after DFS channel switch");
	else
		dcs_enable_init(hapd, iface->conf->conf_extn.dcs_conf.enable_bitmap);
	hostapd_set_he_mcs_12_13_cap_extn(hapd);
	hostapd_periodic_acs_start(iface);
#endif
	/*
	 * WPS UPnP module can be initialized only when the "upnp_iface" is up.
	 * If "interface" and "upnp_iface" are the same (e.g., non-bridge
	 * mode), the interface is up only after driver_commit, so initialize
	 * WPS after driver_commit.
	 */
	for (j = 0; j < iface->num_bss; j++) {
		if (hostapd_init_wps_complete(iface->bss[j]))
			goto fail;
	}

	if ((iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) &&
	    !res_dfs_offload) {
		/*
		 * If freq is DFS, and DFS is offloaded to the driver, then wait
		 * for CAC to complete.
		 */
		wpa_printf(MSG_DEBUG, "%s: Wait for CAC to complete", __func__);
		return res_dfs_offload;
	}

#ifdef NEED_AP_MLME
dfs_offload:
#endif /* NEED_AP_MLME */

#ifdef CONFIG_FST
	if (hapd->iconf->fst_cfg.group_id[0]) {
		struct fst_wpa_obj iface_obj;

		fst_hostapd_fill_iface_obj(hapd, &iface_obj);
		iface->fst = fst_attach(hapd->conf->iface, hapd->own_addr,
					&iface_obj, &hapd->iconf->fst_cfg);
		if (!iface->fst) {
			wpa_printf(MSG_ERROR, "Could not attach to FST %s",
				   hapd->iconf->fst_cfg.group_id);
			goto fail;
		}
	}
#endif /* CONFIG_FST */

#ifdef CONFIG_QCN_EXTN
	/*
	 * When boot-up CAC is in progress on a DFS channel, the interface
	 * state was already set to HAPD_IFACE_DFS by
	 * hostapd_bootup_cac_start_extn().  Transitioning to ENABLED here
	 * and printing AP-ENABLED is misleading because the radio is still
	 * performing CAC. The transition to ENABLED happens in
	 * hostapd_bootup_cac_complete_extn() once CAC finishes.
	 */
	if (!iface->bootup_cac_in_progress ||
	    hostapd_is_dfs_required(iface) <= 0) {
#endif /* CONFIG_QCN_EXTN */
	hostapd_set_state(iface, HAPD_IFACE_ENABLED);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_ENABLED);
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
	hostapd_owe_update_trans(iface);
	airtime_policy_update_init(iface);

	if (iface->conf->bgcac_en &&
	    iface->conf->enable_background_radar &&
	    !iface->radar_background.cac_started)
		hostapd_start_background_cac(iface);

	if (hapd->setup_complete_cb)
		hapd->setup_complete_cb(hapd->setup_complete_cb_ctx);

#ifdef CONFIG_MESH
	if (delay_apply_cfg && !iface->mconf) {
		wpa_printf(MSG_ERROR, "Error while completing mesh init");
		goto fail;
	}
#endif /* CONFIG_MESH */

	wpa_printf(MSG_DEBUG, "%s: Setup of interface done.",
		   iface->bss[0]->conf->iface);
	if (iface->interfaces && iface->interfaces->terminate_on_error > 0)
		iface->interfaces->terminate_on_error--;

	for (j = 0; j < iface->num_bss; j++)
		hostapd_neighbor_set_own_report(iface->bss[j]);

	/* OCE 4.3.1/4.3.2: start periodic channel survey timer */
	hostapd_oce_survey_timer_start(iface);

	hostapd_interface_update_fils_ubpr(iface, true);

	if (iface->interfaces && iface->interfaces->count > 1)
		ieee802_11_update_beacons(iface);

#ifdef CONFIG_MQTT
	/*
	 * Create the global MQTT connection on the first interface setup.
	 * hostapd_mqtt_init() is idempotent and no-ops if mqtt_enabled=0 or
	 * the connection already exists (guards against re-entry on config
	 * reload or multiple interface setups).
	 */
	hostapd_mqtt_init(iface);
#endif /* CONFIG_MQTT */

	atf_offload_send_feature_params(hapd);
	return 0;

fail:
	wpa_printf(MSG_ERROR, "Interface initialization failed");
#ifdef CONFIG_QCN_EXTN
	hostapd_log_trigger_emit(iface->bss[0], NULL,
				 HOSTAPD_LOG_TRIG_VAP_UP_FAIL);
#endif /* CONFIG_QCN_EXTN */
	hostapd_ubus_free_iface(iface);
#ifdef CONFIG_QCN_EXTN
	iface->bootup_cac_in_progress = 0;
#endif /* CONFIG_QCN_EXTN */

	if (iface->is_no_ir) {
		hostapd_set_state(iface, HAPD_IFACE_NO_IR);
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
		return 0;
	}

#ifdef CONFIG_QCN_EXTN
	/*
	 * Bring-up failed — the configured channel may be regulatory-disabled
	 * in the current country. Park to NO_IR and mark is_regdom_forced_down
	 * so the next REGULATORY event triggers full channel selection and come
	 * back up. Without this, the iface would stay DISABLED with no recovery.
	 */
	if (!regdom_subchans_valid(iface) &&
	    hostapd_regdom_bringup_failed_extn(iface))
		return 0;
#endif /* CONFIG_QCN_EXTN */

	hostapd_set_state(iface, HAPD_IFACE_DISABLED);
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
#ifdef CONFIG_FST
	if (iface->fst) {
		fst_detach(iface->fst);
		iface->fst = NULL;
	}
#endif /* CONFIG_FST */

	if (iface->interfaces && iface->interfaces->terminate_on_error) {
		eloop_terminate();
	} else if (hapd->setup_complete_cb) {
		/*
		 * Calling hapd->setup_complete_cb directly may cause iface
		 * deinitialization which may be accessed later by the caller.
		 */
		eloop_register_timeout(0, 0,
				       hostapd_interface_setup_failure_handler,
				       iface, NULL);
	}

	return -1;
}


/**
 * hostapd_setup_interface_complete - Complete interface setup
 *
 * This function is called when previous steps in the interface setup has been
 * completed. This can also start operations, e.g., DFS, that will require
 * additional processing before interface is ready to be enabled. Such
 * operations will call this function from eloop callbacks when finished.
 */
int hostapd_setup_interface_complete(struct hostapd_iface *iface, int err)
{
	struct hapd_interfaces *interfaces = iface->interfaces;
	struct hostapd_data *hapd = iface->bss[0];
	unsigned int i;
	int not_ready_in_sync_ifaces = 0;

	if (!iface->need_to_start_in_sync)
		return hostapd_setup_interface_complete_sync(iface, err);

	if (err) {
		wpa_printf(MSG_ERROR, "Interface initialization failed");
		iface->need_to_start_in_sync = 0;

		if (iface->is_no_ir) {
			hostapd_set_state(iface, HAPD_IFACE_NO_IR);
			wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
			return 0;
		}

#ifdef CONFIG_QCN_EXTN
		/*
		 * Bring-up failed — the configured channel may be regulatory-disabled
		 * in the current country. Park to NO_IR and mark is_regdom_forced_down
		 * so the next REGULATORY event triggers full channel selection and come
		 * back up. Without this, the iface would stay DISABLED with no recovery.
		 */
		if (!regdom_subchans_valid(iface) &&
		    hostapd_regdom_bringup_failed_extn(iface))
			return 0;
#endif /* CONFIG_QCN_EXTN */

		hostapd_set_state(iface, HAPD_IFACE_DISABLED);
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
		if (interfaces && interfaces->terminate_on_error)
			eloop_terminate();
		return -1;
	}

	if (iface->ready_to_start_in_sync) {
		/* Already in ready and waiting. should never happpen */
		return 0;
	}

	for (i = 0; i < interfaces->count; i++) {
		if (interfaces->iface[i]->need_to_start_in_sync &&
		    !interfaces->iface[i]->ready_to_start_in_sync)
			not_ready_in_sync_ifaces++;
	}

	/*
	 * Check if this is the last interface, if yes then start all the other
	 * waiting interfaces. If not, add this interface to the waiting list.
	 */
	if (not_ready_in_sync_ifaces > 1 && iface->state == HAPD_IFACE_DFS) {
		/*
		 * If this interface went through CAC, do not synchronize, just
		 * start immediately.
		 */
		iface->need_to_start_in_sync = 0;
		wpa_printf(MSG_INFO,
			   "%s: Finished CAC - bypass sync and start interface",
			   iface->bss[0]->conf->iface);
		return hostapd_setup_interface_complete_sync(iface, err);
	}

	if (not_ready_in_sync_ifaces > 1) {
		/* need to wait as there are other interfaces still coming up */
		iface->ready_to_start_in_sync = 1;
		wpa_printf(MSG_INFO,
			   "%s: Interface waiting to sync with other interfaces",
			   iface->bss[0]->conf->iface);
		return 0;
	}

	wpa_printf(MSG_INFO,
		   "%s: Last interface to sync - starting all interfaces",
		   iface->bss[0]->conf->iface);
	iface->need_to_start_in_sync = 0;
	hostapd_setup_interface_complete_sync(iface, err);
	for (i = 0; i < interfaces->count; i++) {
		if (interfaces->iface[i]->need_to_start_in_sync &&
		    interfaces->iface[i]->ready_to_start_in_sync) {
			hostapd_setup_interface_complete_sync(
				interfaces->iface[i], 0);
			/* Only once the interfaces are sync started */
			interfaces->iface[i]->need_to_start_in_sync = 0;
		}
	}

	return 0;
}


/**
 * hostapd_setup_interface - Setup of an interface
 * @iface: Pointer to interface data.
 * Returns: 0 on success, -1 on failure
 *
 * Initializes the driver interface, validates the configuration,
 * and sets driver parameters based on the configuration.
 * Flushes old stations, sets the channel, encryption,
 * beacons, and WDS links based on the configuration.
 *
 * If interface setup requires more time, e.g., to perform HT co-ex scans, ACS,
 * or DFS operations, this function returns 0 before such operations have been
 * completed. The pending operations are registered into eloop and will be
 * completed from eloop callbacks. Those callbacks end up calling
 * hostapd_setup_interface_complete() once setup has been completed.
 */
int hostapd_setup_interface(struct hostapd_iface *iface)
{
	int ret;

	if (!iface->conf)
		return -1;

	ret = setup_interface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "%s: Unable to setup interface.",
			   iface->conf->bss[0]->iface);
#ifdef CONFIG_QCN_EXTN
		hostapd_log_trigger_emit(iface->bss[0], NULL,
					 HOSTAPD_LOG_TRIG_VAP_UP_FAIL);
#endif /* CONFIG_QCN_EXTN */
		return -1;
	}

	return 0;
}


/**
 * hostapd_alloc_bss_data - Allocate and initialize per-BSS data
 * @hapd_iface: Pointer to interface data
 * @conf: Pointer to per-interface configuration
 * @bss: Pointer to per-BSS configuration for this BSS
 * Returns: Pointer to allocated BSS data
 *
 * This function is used to allocate per-BSS data structure. This data will be
 * freed after hostapd_cleanup() is called for it during interface
 * deinitialization.
 */
struct hostapd_data *
hostapd_alloc_bss_data(struct hostapd_iface *hapd_iface,
		       struct hostapd_config *conf,
		       struct hostapd_bss_config *bss)
{
	struct hostapd_data *hapd;

	hapd = os_zalloc(sizeof(*hapd));
	if (hapd == NULL)
		return NULL;

	hapd->new_assoc_sta_cb = hostapd_new_assoc_sta;
	hapd->iconf = conf;
	hapd->conf = bss;
	hapd->iface = hapd_iface;
	/* -1 = no per-module override; falls back to global wpa_debug_level */
	os_memset(hapd->log_module_level, 0xff,
		  sizeof(hapd->log_module_level));
	if (conf)
		hapd->driver = conf->driver;
	hapd->ctrl_sock = -1;
	dl_list_init(&hapd->ctrl_dst);
	dl_list_init(&hapd->nr_db);
	dl_list_init(&hapd->bcn_report_db);
	hapd->dhcp_sock = -1;
#ifdef CONFIG_IEEE80211R_AP
	dl_list_init(&hapd->l2_queue);
	dl_list_init(&hapd->l2_oui_queue);
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
	dl_list_init(&hapd->sae_commit_queue);
#endif /* CONFIG_SAE */
	dl_list_init(&hapd->erp_keys);
#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211AC)
	hostapd_mu_cap_war_state_init_extn(hapd);
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211AC */
#ifdef CONFIG_QCN_EXTN
	hostapd_log_extn_init(hapd);
#endif /* CONFIG_QCN_EXTN */

	if (conf && conf->ieee80211ax)
		hapd->parameter_set_count = conf->he_mu_edca.he_qos_info & 0xf;

	return hapd;
}


void hostapd_bss_deinit(struct hostapd_data *hapd)
{
	if (!hapd)
		return;
	wpa_printf(MSG_DEBUG, "%s: deinit bss %s", __func__,
		   hapd->conf ? hapd->conf->iface : "N/A");
#ifdef CONFIG_IEEE80211BN
	mapc_deinit(hapd);
#endif /* CONFIG_IEEE80211BN */
	hostapd_bss_deinit_no_free(hapd);
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
#ifdef CONFIG_SQLITE
	if (hapd->rad_attr_db) {
		sqlite3_close(hapd->rad_attr_db);
		hapd->rad_attr_db = NULL;
	}
#endif /* CONFIG_SQLITE */

	hostapd_bss_link_deinit(hapd);
	hostapd_cleanup(hapd);
}
/**
 * hostapd_free_chan_obj() - Free the AFC chan object and chan eirp object
 * information
 * @afc_chan_info: Pointer to afc_chan_info
 *
 * Return: void
 */
static void hostapd_free_chan_obj(struct afc_chan_obj *afc_chan_info)
{
	if (afc_chan_info->chan_eirp_info)
		os_free(afc_chan_info->chan_eirp_info);
}

void hostapd_free_afc_data(struct hostapd_iface *iface)
{
	u8 i;
	struct afc_sp_reg_info *afc_rsp;

	if (!iface->afc_rsp_info)
		return;

	afc_rsp = iface->afc_rsp_info;

	if (afc_rsp->afc_freq_info)
		os_free(afc_rsp->afc_freq_info);

	if (!afc_rsp->afc_chan_info)
		return;

	for (i = 0; i < afc_rsp->num_chan_objs; i++)
		hostapd_free_chan_obj(&afc_rsp->afc_chan_info[i]);

	if (afc_rsp->afc_chan_info)
		os_free(afc_rsp->afc_chan_info);

	os_free(afc_rsp);
	iface->afc_rsp_info = NULL;
}

void hostapd_interface_deinit(struct hostapd_iface *iface)
{
	int j;

	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	if (iface == NULL)
		return;

	hostapd_set_state(iface, HAPD_IFACE_DISABLED);

	hostapd_cleanup_monitor_iface(iface);

	eloop_cancel_timeout(channel_list_update_timeout, iface, NULL);

#ifdef CONFIG_QCN_EXTN
	hostapd_periodic_acs_stop(iface);
#endif
	iface->wait_channel_update = 0;
	iface->is_afc_channel_change_pending = 0;
	iface->is_afc_repeater_power_sync_pending = 0;
	iface->power_mode_6ghz_before_change = -1;
	iface->is_no_ir = false;
	hostapd_free_afc_data(iface);
	iface->is_afc_power_event_received = false;
	iface->rnr_psd = CHAN_MIN_TX_POWER;

#ifdef CONFIG_FST
	if (iface->fst) {
		fst_detach(iface->fst);
		iface->fst = NULL;
	}
#endif /* CONFIG_FST */

	for (j = (int) iface->num_bss - 1; j >= 0; j--) {
		if (!iface->bss)
			break;
		if (iface->bss[j] &&
		    iface->bss[j]->reenable != REENABLE_DEINIT &&
		    iface->bss[j]->reenable != REENABLE_REUSE_LINK)
			iface->bss[j]->reenable = REENABLE_DEINIT;
		hostapd_bss_deinit(iface->bss[j]);
	}

	hostapd_interface_update_fils_ubpr(iface, false);

#ifdef NEED_AP_MLME
	hostapd_stop_setup_timers(iface);
	eloop_cancel_timeout(ap_ht2040_timeout, iface, NULL);
#endif /* NEED_AP_MLME */

#ifdef CONFIG_IEEE80211BN
	/* All per-BSS mapc_deinit() calls have completed inside
	 * hostapd_bss_deinit() above; now tear down radio-level MAPC state. */
	mapc_iface_deinit(iface);
#endif /* CONFIG_IEEE80211BN */
}


#ifdef CONFIG_IEEE80211BE

static void hostapd_mld_ref_inc(struct hostapd_mld *mld)
{
	if (!mld)
		return;

	if (mld->refcount == HOSTAPD_MLD_MAX_REF_COUNT) {
		wpa_printf(MSG_ERROR, "AP MLD %s: Ref count overflow",
			   mld->name);
		return;
	}

	mld->refcount++;
}


void hostapd_mld_ref_dec(struct hostapd_mld *mld)
{
	if (!mld)
		return;

	if (!mld->refcount) {
		wpa_printf(MSG_ERROR, "AP MLD %s: Ref count underflow",
			   mld->name);
		return;
	}

	mld->refcount--;
}


int hostapd_parse_link_id(char *buf)
{
	char *arg, *sep = os_strchr(buf, ' ');
	int link_id;

	if (!sep)
		return -1;

	arg = sep + 1;

	while (*arg == ' ')
		arg++;

	if (*arg == '\0')
		return -1;

	link_id = atoi(arg);
	*sep = '\0';

	return link_id;
}

#endif /* CONFIG_IEEE80211BE */

void hostapd_multi_mbssid_remove_bss(struct hostapd_data *hapd)
{
	struct hostapd_data *next_txbss;
	struct hostapd_multi_mbssid_group *group;

	if (!hapd)
		return;

	group = hapd->mbssid_group;
	if (!group)
		return;

	if (hapd->iconf->mbssid != MULTI_MBSSID_GROUP_ENABLED)
		return;

	dl_list_del(&hapd->mbssid_bss);
	group->num_bss--;
	hapd->mbssid_group = NULL;

	wpa_printf(MSG_DEBUG, "BSS[%s] removed from MBSSID group %d",
		   hapd->conf->iface, group->group_id);
	if (group->txbss != hapd)
		return;

	/* If len is 0, all BSSes are removed */
	if (!dl_list_len(&group->bss_list)) {
		group->txbss = NULL;
	} else {
		next_txbss = dl_list_entry(group->bss_list.next, struct hostapd_data,
					   mbssid_bss);
		group->txbss = next_txbss;
		wpa_printf(MSG_DEBUG, "Set BSS[%s] as TX bss on group %d",
			   group->txbss->conf->iface, group->group_id);
	}
}


void hostapd_interface_free(struct hostapd_iface *iface)
{
	size_t j, num_groups;
	struct hostapd_multi_mbssid_group *group;
	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	for (j = 0; j < iface->num_bss; j++) {
		if (!iface->bss)
			break;
#ifdef CONFIG_IEEE80211BE
		if (iface->bss[j])
			hostapd_mld_ref_dec(iface->bss[j]->mld);
#endif /* CONFIG_IEEE80211BE */
		wpa_printf(MSG_DEBUG, "%s: free hapd %p",
			   __func__, iface->bss[j]);
		hostapd_multi_mbssid_remove_bss(iface->bss[j]);
		os_free(iface->bss[j]);
	}
	num_groups = iface->multi_mbssid.num_mbssid_groups;
	for (j = 0; j < iface->multi_mbssid.num_mbssid_groups; j++) {
		group = iface->multi_mbssid.group[j];
		if (!group)
			continue;
		wpa_printf(MSG_DEBUG, "free MBSSID group id %ld", (long unsigned int)j);
		os_free(group);
		iface->multi_mbssid.group[j] = NULL;
		num_groups--;
	}
	if (!num_groups) {
		iface->multi_mbssid.num_mbssid_groups = 0;
		os_free(iface->multi_mbssid.group);
		iface->multi_mbssid.group = NULL;
	}

	hostapd_cleanup_iface(iface);
}


struct hostapd_iface * hostapd_alloc_iface(void)
{
	struct hostapd_iface *hapd_iface;

	hapd_iface = os_zalloc(sizeof(*hapd_iface));
	if (!hapd_iface)
		return NULL;

	dl_list_init(&hapd_iface->sta_seen);

	hapd_iface->is_afc_power_event_received = false;
	hapd_iface->is_afc_repeater_power_sync_pending = false;

	/*
	 * channel == -1 is the sentinel the DFS background-radar code checks
	 * for "no cached channel"; os_zalloc() above leaves it 0, which is a
	 * channel number, not the sentinel.
	 */
	hapd_iface->radar_background.channel = -1;

#ifdef CONFIG_QCN_EXTN
	hostapd_iface_init_extn(hapd_iface);
	hapd_iface->vendor_bssid_used_mask = 0;
#endif /* CONFIG_QCN_EXTN */

	return hapd_iface;
}


#ifdef CONFIG_IEEE80211BE

static int hostapd_bss_alloc_link_id(struct hostapd_data *hapd)
{
	/* All links are exhausted */
	if (!hapd->mld->free_links)
		return -1;

	/* Reject mixed allocation modes within the same MLD */
	if (hapd->conf->mld_link_id >= 0 &&
	    hapd->mld->free_links != 0x7FFF && !hapd->mld->link_id_mode) {
		wpa_printf(MSG_ERROR,
			   "AP MLD: %s: iface=%s requests configured link ID but MLD uses sequential allocation",
			   hapd->mld->name, hapd->conf->iface);
		return -1;
	}
	if (hapd->conf->mld_link_id < 0 && hapd->mld->link_id_mode) {
		wpa_printf(MSG_ERROR,
			   "AP MLD: %s: iface=%s requests sequential link ID but MLD uses configured allocation",
			   hapd->mld->name, hapd->conf->iface);
		return -1;
	}

	/* Honor user-configured link ID if set */
	if (hapd->conf->mld_link_id >= 0) {
		int cfg_id = hapd->conf->mld_link_id;

		if (!(hapd->mld->free_links & BIT(cfg_id))) {
			wpa_printf(MSG_ERROR,
				   "AP MLD: %s: configured mld_link_id %d is not available (free_links=0x%x) for iface=%s",
				   hapd->mld->name, cfg_id,
				   hapd->mld->free_links, hapd->conf->iface);
			return -1;
		}
		wpa_printf(MSG_DEBUG,
			   "AP MLD: %s: Using configured mld_link_id=%d for iface=%s",
			   hapd->mld->name, cfg_id, hapd->conf->iface);
		hapd->mld_link_id = cfg_id;
		hapd->mld->free_links &= ~BIT(hapd->mld_link_id);
		hapd->mld->link_id_mode = 1;
		return 0;
	}

	hapd->mld_link_id = ffs(hapd->mld->free_links) - 1;
	hapd->mld->free_links &= ~BIT(hapd->mld_link_id);
	wpa_printf(MSG_DEBUG, "AP MLD: %s: Link ID %d assigned.",
		   hapd->mld->name, hapd->mld_link_id);

	return 0;
}

static inline void hostapd_mld_ttlm_ctx_init(struct hostapd_mld *mld)
{
	struct ttlm_info *ttlm;

	ttlm = &mld->ttlm_ctx.established_ttlm.ttlm;

	os_memset(&mld->ttlm_ctx, 0, sizeof(struct ttlm_context));

	ttlm->direction = TTLM_DIRECTION_BIDI;
	ttlm->default_link_mapping = 1;
	ttlm->link_mapping_size = 0;
}

#endif /* CONFIG_IEEE80211BE */


void hostapd_bss_setup_multi_link(struct hostapd_data *hapd,
				  struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_mld *mld, **all_mld;
	struct hostapd_bss_config *conf;
	size_t i;

	if (hapd->mld)
		return;

	conf = hapd->conf;

#ifndef CONFIG_QCN_EXTN

	if (!hapd->iconf || !conf->mld_ap || !hostapd_is_eht_enabled(hapd))
		return;

#else
	if (!hapd->iconf || !conf->mld_ap ||
	    (!hostapd_is_eht_enabled(hapd) &&
	     !hostapd_is_repurpose_disabled_11be_extn(conf)))
		return;
#endif /* CONFIG_QCN_EXTN */

	for (i = 0; i < interfaces->mld_count; i++) {
		mld = interfaces->mld[i];

		if (!mld || os_strcmp(conf->iface, mld->name) != 0)
			continue;

		hapd->mld = mld;
		hostapd_mld_ref_inc(mld);
		if (hostapd_bss_alloc_link_id(hapd)) {
			hostapd_mld_ref_dec(mld);
			hapd->mld = NULL;
			return;
		}
		break;
	}

	if (hapd->mld) {
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(conf)) {
#endif /* CONFIG_QCN_EXTN */
		struct ttlm_context *ttlm = &hapd->mld->ttlm_ctx;
		struct ttlm_info *info;

		/* Enable this new link by default if advertised ttlm is
		 * already in progress with few links marked disabled
		 */
		if (ttlm->established_ttlm.ttlm.expected_duration_present) {
			info = &ttlm->established_ttlm.ttlm;

			for (i = 0; i < NUM_MAX_TIDS; i++)
				info->ieee_link_map_tid[i] |=
					BIT(hapd->mld_link_id);
		}

		if (ttlm->upcoming_ttlm.ttlm.mapping_switch_time_present) {
			info = &ttlm->upcoming_ttlm.ttlm;
			for (i = 0; i < NUM_MAX_TIDS; i++)
				info->ieee_link_map_tid[i] |=
					BIT(hapd->mld_link_id);
		}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
		return;
	}

	mld = os_zalloc(sizeof(struct hostapd_mld));
	if (!mld)
		goto fail;

	os_strlcpy(mld->name, conf->iface, sizeof(conf->iface));
	dl_list_init(&mld->links);
	mld->free_links = 0x7FFF;
	mld->ctrl_sock = -1;
	if (hapd->conf->ctrl_interface)
		mld->ctrl_interface = os_strdup(hapd->conf->ctrl_interface);
	hostapd_mld_ttlm_ctx_init(mld);

	wpa_printf(MSG_DEBUG, "AP MLD %s created", mld->name);

	/* Initialize MLD control interfaces early to allow external monitoring
	 * of link setup operations. */
	if (interfaces->mld_ctrl_iface_init(mld))
		goto fail;

	hapd->mld = mld;
	hostapd_mld_ref_inc(mld);
	if (hostapd_bss_alloc_link_id(hapd))
		goto fail;

	all_mld = os_realloc_array(interfaces->mld, interfaces->mld_count + 1,
				   sizeof(struct hostapd_mld *));
	if (!all_mld)
		goto fail;

	interfaces->mld = all_mld;
	interfaces->mld[interfaces->mld_count] = mld;
	interfaces->mld_count++;
	dl_list_init(&hapd->mld->ft_ds_ml_stas);

	return;
fail:
	if (!mld)
		return;

	interfaces->mld_ctrl_iface_deinit(mld);
	wpa_printf(MSG_DEBUG, "AP MLD %s: free mld %p", mld->name, mld);
	os_free(mld->epcs_authorized_mac);
	os_free(mld);
	hapd->mld = NULL;
#endif /* CONFIG_IEEE80211BE */
}


u64 hostapd_addr_to_u64(const u8 *addr)
{
    u64 result = 0;
    int i;

    for (i = 0; i < ETH_ALEN; i++) {
        result = result << 8 | addr[i];
    }

    return result;
}


static void hostapd_multi_mbssid_set_mbssid_index(struct hostapd_data *hapd,
						  u8 group_size, u64 bssid_mask,
						  u32 *mbssid_idx_bmap)
{
	struct hostapd_data *tx_hapd;
	size_t bss_index;
	u8 bss_index_shift;

	tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	if (!tx_hapd || tx_hapd == hapd) {
		hapd->mbssid_idx = 0;
		*mbssid_idx_bmap |= BIT(hapd->mbssid_idx);

		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			hapd->mbssid_group->txbss = hapd;

		return;
	}

	bss_index_shift = hostapd_addr_to_u64(tx_hapd->own_addr) & bssid_mask;
	bss_index = hostapd_addr_to_u64(hapd->own_addr) & bssid_mask;
	if (bss_index < bss_index_shift)
		bss_index += group_size;
	bss_index -= bss_index_shift;

	hapd->mbssid_idx = bss_index;
	*mbssid_idx_bmap |= BIT(hapd->mbssid_idx);
}

static int hostapd_multi_mbssid_add_bss(struct hostapd_data *hapd)
{
	u64 addr, bssid_mask = 0, group_mask = 0, prefix_mask = UINT64_MAX;
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_multi_mbssid_group *group = NULL;
	struct hostapd_multi_mbssid *multi_mbssid = &iface->multi_mbssid;
	struct hostapd_data *bss = NULL;
	u8 cnt, max_bssid_indicator, group_index;
	unsigned int mbssid_max_interfaces;
	bool bss_added = false;
	size_t i, j;
	unsigned int num_hws;
#ifdef CONFIG_QCN_EXTN
	bool is_mesh_vap;
#endif

	if (!hapd || !hapd->iconf)
		return -1;

	if (hapd->iconf->mbssid == MBSSID_DISABLED)
		return 0;

	/*Skip mbssid_add_bss if mbssid_group already set*/
	if (hapd->mbssid_group != NULL) {
		wpa_printf(MSG_INFO,
			   "Bss[%s] already part of MBSSID group %d with bss_index:%zu",
			   hapd->conf->iface, hapd->mbssid_group->group_id,
			   hapd->mbssid_idx);
		return 0;
	}

	/* In single wiphy, maximum interfaces supported by each radio are
	 * added, hence divide by num_multi_hws to get per radio limit */
	num_hws = iface->num_multi_hws ? iface->num_multi_hws : 1;
	mbssid_max_interfaces = iface->mbssid_max_interfaces / num_hws;
	if (iface->num_bss > mbssid_max_interfaces) {
		wpa_printf(MSG_ERROR,
			   "Failed to add %s, driver can only support %u interfaces in MBSSID",
			   hapd->conf->iface, mbssid_max_interfaces);
		return -1;
	}

	max_bssid_indicator = hostapd_max_bssid_indicator(hapd);
	prefix_mask <<= max_bssid_indicator;
	bssid_mask = ~prefix_mask;

	addr = hostapd_addr_to_u64(hapd->own_addr);

#ifdef CONFIG_QCN_EXTN
	if (hostapd_validate_mbssid_configuration_extn(hapd)) {
		wpa_printf(MSG_ERROR, "Invalid MBSSID configuration");
		return -1;
	}

	is_mesh_vap = hostapd_is_mesh_vap_extn(hapd->conf);
#endif

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		if (!multi_mbssid->group) {
			multi_mbssid->num_mbssid_groups = mbssid_max_interfaces /
							  iface->conf->group_size;

			if (mbssid_max_interfaces % iface->conf->group_size)
				multi_mbssid->num_mbssid_groups++;

#ifdef CONFIG_QCN_EXTN
			if (is_mesh_vap)
				hostapd_mesh_mbssid_reserve_group_extn(multi_mbssid,
									max_bssid_indicator,
									&group_index,
									&prefix_mask);
#endif
			if (multi_mbssid->num_mbssid_groups > multi_mbssid->mbssid_max_ngroups) {
				wpa_printf(MSG_ERROR,
					   "Configured MBSSID group size results in more groups that supported by driver");
				return -1;
			}
		}

#ifdef CONFIG_QCN_EXTN
		if (is_mesh_vap &&
		    hostapd_mesh_mbssid_reject_duplicate_extn(hapd, multi_mbssid))
			return -1;

		if (!is_mesh_vap) {
#endif
			/* Calculate group ID mask which will decide the group a new
			 * interface will get added to */
			cnt = multi_mbssid->num_mbssid_groups - 1;

#ifdef CONFIG_QCN_EXTN
			/* If mesh MBSSID group is already present, we need to exclude
			 * it in below group assignment logic, as AP vaps should not be
			 * added to the mesh MBSSID group */
			if (hostapd_has_mesh_vap_in_group_extn(hapd, multi_mbssid))
				cnt = cnt - 1;
#endif
			while (cnt) {
				group_mask <<= 1;
				group_mask |= 0x1;
				cnt >>= 1;
			}

			group_mask <<= max_bssid_indicator;
			group_index = (addr & group_mask) >> max_bssid_indicator;

			prefix_mask &= (~group_mask);
#ifdef CONFIG_QCN_EXTN
		}
#endif

		if (!multi_mbssid->group) {
			multi_mbssid->group =
				os_zalloc(sizeof(struct hostapd_multi_mbssid_group *) *
					  multi_mbssid->num_mbssid_groups);
			if (!multi_mbssid->group)
				goto fail;
		}
#ifdef CONFIG_QCN_EXTN
		else if (is_mesh_vap) {
			if (hostapd_mesh_mbssid_grow_or_reuse_group_extn(hapd, multi_mbssid,
									 max_bssid_indicator,
									 &group_index,
									 &prefix_mask))
				return -1;
		}
#endif

		group = multi_mbssid->group[group_index];
		if (!group) {
			group = os_zalloc(sizeof(struct hostapd_multi_mbssid_group));
			if (!group)
				goto fail;

			multi_mbssid->group[group_index] = group;
			group->group_id = group_index;
			group->txbss = hapd;
			dl_list_init(&group->bss_list);
#ifdef CONFIG_QCN_EXTN
			if (is_mesh_vap)
				hostapd_mesh_mbssid_mark_group_extn(hapd, group);
#endif
		}

		hapd->mbssid_group = group;

		for (i = 0; i < multi_mbssid->num_mbssid_groups; i++) {
			if (!multi_mbssid->group[i])
				continue;

			for (j = 0; j < multi_mbssid->group[i]->num_bss; j++) {
				bss = hostapd_get_multi_group_bss(multi_mbssid->group[i],
								  j);
				if (bss && bss->started
#ifdef CONFIG_QCN_EXTN
				    && !hostapd_is_mesh_vap_extn(bss->conf)
#endif
				    )
					break;
			}
		}
	} else {
		for (i = 0; i < iface->num_bss; i++) {
			bss = iface->bss[i];
			if (bss)
				break;
		}
	}

	if (bss &&
#ifdef CONFIG_QCN_EXTN
	    !is_mesh_vap &&
#endif
	    (hostapd_addr_to_u64(bss->own_addr) & prefix_mask) != (addr & prefix_mask)) {
		wpa_printf(MSG_ERROR,
			   "New BSS (" MACSTR ") doesn't satisfy prefix requirement for the MBSSID groups",
			   MAC2STR(hapd->own_addr));
		goto fail;
	}

	if (hapd->iconf->mbssid != MULTI_MBSSID_GROUP_ENABLED) {
		hostapd_multi_mbssid_set_mbssid_index(hapd,
						      1 << max_bssid_indicator,
						      bssid_mask,
						      &iface->mbssid_idx_bmap);
		return 0;
	}

	/* Add to MBSSID group */
	group = hapd->mbssid_group;
	hostapd_multi_mbssid_set_mbssid_index(hapd, 1 << max_bssid_indicator,
					      bssid_mask,
					      &group->mbssid_idx_bmap);

	/* Add new BSS in the order of incrementing BSS indices */
	dl_list_for_each(bss, &group->bss_list, struct hostapd_data, mbssid_bss) {
		if (bss->mbssid_idx < hapd->mbssid_idx)
			continue;

		dl_list_add(bss->mbssid_bss.prev, &hapd->mbssid_bss);
		bss_added = true;
		break;
	}

	if (!bss_added)
		dl_list_add_tail(&group->bss_list, &hapd->mbssid_bss);

	wpa_printf(MSG_INFO,
		   "Bss[%s] added to MBSSID group %d with bss_index:%zu",
		   hapd->conf->iface, hapd->mbssid_group->group_id,
		   hapd->mbssid_idx);

	group->num_bss++;
	return 0;
fail:
	wpa_printf(MSG_ERROR, "Failed to add Bss[%s] to MBSSID group",
		   hapd->conf->iface);

	if (hapd->iconf->mbssid != MULTI_MBSSID_GROUP_ENABLED)
		return -1;

	if (group && !group->num_bss) {
		for (i = 0; i < multi_mbssid->num_mbssid_groups; i++) {
			if (multi_mbssid->group[i] &&
			    multi_mbssid->group[i]->group_id == group->group_id) {
				multi_mbssid->group[i] = NULL;
				break;
			}
		}
		os_free(group);
		group = NULL;
	}

	if (multi_mbssid->group) {
		for (i = 0; i < multi_mbssid->num_mbssid_groups; i++) {
			if (multi_mbssid->group[i])
				break;
		}

		if (i == multi_mbssid->num_mbssid_groups) {
			os_free(multi_mbssid->group);
			multi_mbssid->group = NULL;
		}
	}

	hapd->mbssid_group = NULL;
	return -1;
}

int hostapd_mbssid_setup_bss(struct hostapd_data *hapd)
{
	struct hostapd_data *tx_bss;
	size_t num_bss, i;

	if (hostapd_multi_mbssid_add_bss(hapd)) {
		wpa_printf(MSG_ERROR, "Failed to set MBSSID parameters for %s",
			   hapd->conf->iface);
		return -1;
	}

	/*
	 * When setting up multi bssid, reserve AIDs for group transmssion
	 * on Tx VAP
	 */
	tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	if (tx_bss != hapd)
		return 0;

	num_bss = (1 << hostapd_max_bssid_indicator(tx_bss));

	for (i = 0; i < num_bss; i++)
		tx_bss->sta_aid[0] |= BIT(i);

	return 0;
}

static void hostapd_cleanup_unused_mlds(struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_mld *mld, **all_mld;
	size_t i, j, num_mlds;
	bool forced_remove, remove;

	if (!interfaces->mld)
		return;

	num_mlds = interfaces->mld_count;

	for (i = 0; i < interfaces->mld_count; i++) {
		mld = interfaces->mld[i];
		if (!mld)
			continue;

		remove = false;
		forced_remove = false;

		if (!mld->refcount)
			remove = true;

		/* If MLD is still being referenced but the number of interfaces
		 * is zero, it is safe to force its deletion. Normally, this
		 * should not happen but even if it does, let us free the
		 * memory.
		 */
		if (!remove && !interfaces->count)
			forced_remove = true;

		if (!remove && !forced_remove)
			continue;

		interfaces->mld_ctrl_iface_deinit(mld);

		wpa_printf(MSG_DEBUG, "AP MLD %s: Freed%s", mld->name,
			   forced_remove ? " (forced)" : "");
		os_free(mld->epcs_authorized_mac);
		os_free(mld);
		interfaces->mld[i] = NULL;
		num_mlds--;
	}

	if (!num_mlds) {
		interfaces->mld_count = 0;
		os_free(interfaces->mld);
		interfaces->mld = NULL;
		return;
	}

	all_mld = os_zalloc(num_mlds * sizeof(struct hostapd_mld *));
	if (!all_mld) {
		wpa_printf(MSG_ERROR,
			   "AP MLD: Failed to re-allocate the MLDs. Expect issues");
		return;
	}

	for (i = 0, j = 0; i < interfaces->mld_count; i++) {
		mld = interfaces->mld[i];
		if (!mld)
			continue;

		all_mld[j++] = mld;
	}

	/* This should not happen */
	if (j != num_mlds) {
		wpa_printf(MSG_DEBUG,
			   "AP MLD: Some error occurred while reallocating MLDs. Expect issues.");
		os_free(all_mld);
		return;
	}

	os_free(interfaces->mld);
	interfaces->mld = all_mld;
	interfaces->mld_count = num_mlds;
#endif /* CONFIG_IEEE80211BE */
}

static int hostapd_require_tx_bss(struct hostapd_data *hapd, bool override_set,
				  const char *op_name)
{
	if (!override_set)
		return 0;

	return hostapd_tx_bss_only(hapd, op_name);
}


static int hostapd_validate_bss_tx_params(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211AC
	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->vht_mcs_nss_set,
				   "vht_mcs_nss_set") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->vht_capab_mask &
				   VHT_CAP_BSS_OVR_SU_BEAMFORMER,
				   "bss_vht_su_beamformer") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->vht_capab_mask &
				   VHT_CAP_BSS_OVR_SU_BEAMFORMEE,
				   "bss_vht_su_beamformee") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->vht_capab_mask &
				   VHT_CAP_BSS_OVR_MU_BEAMFORMER,
				   "bss_vht_mu_beamformer") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->vht_capab_mask &
				   VHT_CAP_BSS_OVR_SOUNDING_DIMENSION,
				   "bss_vht_sounding_dimension") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->vht_capab_mask &
				   VHT_CAP_BSS_OVR_STS_CAPABILITY,
				   "bss_vht_beamformee_sts") < 0)
		return -1;
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_SU_BEAMFORMER,
				   "bss_he_su_beamformer") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_SU_BEAMFORMEE,
				   "bss_he_su_beamformee") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MU_BEAMFORMER,
				   "bss_he_mu_beamformer") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_UL_MUMIMO,
				   "bss_he_ul_mumimo") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_BFEE_STS_LTEQ80,
				   "bss_he_bfee_sts_lteq80") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_BFEE_STS_GT80,
				   "bss_he_bfee_sts_gt80") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MULTI_TID_AGGR,
				   "bss_he_multi_tid_aggr") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MULTI_TID_AGGR_TX,
				   "bss_he_multi_tid_aggr_tx") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MAX_AMPDU_LEN_EXP,
				   "bss_he_max_ampdu_len_exp") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_SU_PPDU_1X_LTF_800NS_GI,
				   "bss_he_su_ppdu_1x_ltf_800ns_gi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_SU_MU_PPDU_4X_LTF_800NS_GI,
				   "bss_he_su_mu_ppdu_4x_ltf_800ns_gi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MAX_FRAG_MSDU,
				   "bss_he_max_frag_msdu") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MIN_FRAG_SIZE,
				   "bss_he_min_frag_size") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_OMI,
				   "bss_he_omi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_NDP_4X_LTF_3200NS_GI,
				   "bss_he_ndp_4x_ltf_3200ns_gi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_FRAGMENTATION,
				   "bss_he_fragmentation") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_AMSDU_IN_AMPDU_SUPRT,
				   "bss_he_amsdu_in_ampdu_suprt") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_MAX_NC_SUPRT,
				   "bss_he_max_nc_suprt") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_ER_SU_DISABLE,
				   "bss_he_er_su_disable") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_ER_SU_PPDU_1X_LTF_800NS_GI,
				   "bss_he_er_su_ppdu_1x_ltf_800ns_gi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_ER_SU_PPDU_4X_LTF_800NS_GI,
				   "bss_he_er_su_ppdu_4x_ltf_800ns_gi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_1024QAM_LT242RU_RX_ENABLE,
				   "bss_he_1024qam_lt242ru_rx_enable") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->he_phy_capab_mask &
				   HE_PHY_BSS_OVR_BSR_SUPPORT,
				   "bss_he_bsr_support") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hostapd_he_mcs_nss_set_is_set(
					hapd->conf->he_tx_mcs_nss_set),
				   "he_tx_mcs_nss_set") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hostapd_he_mcs_nss_set_is_set(
					hapd->conf->he_rx_mcs_nss_set),
				   "he_rx_mcs_nss_set") < 0)
		return -1;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_require_tx_bss(hapd,
				   hostapd_eht_mcs_nss_set_is_set(
					hapd->conf->eht_tx_mcs_nss_set),
				   "eht_tx_mcs_nss_set") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hostapd_eht_mcs_nss_set_is_set(
					hapd->conf->eht_rx_mcs_nss_set),
				   "eht_rx_mcs_nss_set") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_SU_BEAMFORMER,
				   "bss_eht_su_beamformer") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_SU_BEAMFORMEE,
				   "bss_eht_su_beamformee") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_MU_BEAMFORMER,
				   "bss_eht_mu_beamformer") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NDP_4X_EHT_LTF_AND_320NSGI,
				   "bss_eht_ndp_4x_eht_ltf_and_320nsgi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NUM_SD_LT80,
				   "bss_eht_num_sd_lt80") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NUM_SD_160,
				   "bss_eht_num_sd_160") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NUM_SD_320,
				   "bss_eht_num_sd_320") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_4X_EHT_LTF_AND_800NS_GI,
				   "bss_eht_4x_eht_ltf_and_800ns_gi") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_RX_1024_AND_4096_QAM_LS_242_TONE_RU,
				   "bss_eht_rx_1024_and_4096_qam_ls_242_tone_ru") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_DL_OFDMA_TXBF,
				   "bss_eht_dl_ofdma_txbf") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_TRIG_SU_BF_FB,
				   "bss_eht_trig_su_bf_fb") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_TRIG_CQI_FB,
				   "bss_eht_trig_cqi_fb") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NON_TRIG_CQI_FB,
				   "bss_eht_non_trig_cqi_fb") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_SUP_MCS15_IN_MRU,
				   "bss_eht_sup_mcs15_in_mru") < 0)
		return -1;

	if (hostapd_require_tx_bss(hapd,
				   hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_MCS14_DUP_IN_6GHZ,
				   "bss_eht_mcs14_dup_in_6ghz") < 0)
		return -1;
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


/**
 * hostapd_init - Allocate and initialize per-interface data
 * @config_file: Path to the configuration file
 * Returns: Pointer to the allocated interface data or %NULL on failure
 *
 * This function is used to allocate main data structures for per-interface
 * data. The allocated data buffer will be freed by calling
 * hostapd_cleanup_iface().
 */
struct hostapd_iface * hostapd_init(struct hapd_interfaces *interfaces,
				    const char *config_file)
{
	struct hostapd_iface *hapd_iface = NULL;
	struct hostapd_config *conf = NULL;
	struct hostapd_data *hapd;
	size_t i;

	hapd_iface = hostapd_alloc_iface();
	if (hapd_iface == NULL)
		goto fail;

	hapd_iface->config_fname = os_strdup(config_file);
	if (hapd_iface->config_fname == NULL)
		goto fail;

	conf = interfaces->config_read_cb(hapd_iface->config_fname);
	if (conf == NULL)
		goto fail;
	hapd_iface->conf = conf;

	hapd_iface->num_bss = conf->num_bss;
	hapd_iface->bss = os_calloc(conf->num_bss,
				    sizeof(struct hostapd_data *));
	if (hapd_iface->bss == NULL)
		goto fail;

	for (i = 0; i < conf->num_bss; i++) {
		hapd = hapd_iface->bss[i] =
			hostapd_alloc_bss_data(hapd_iface, conf,
					       conf->bss[i]);
		if (hapd == NULL)
			goto fail;
		hapd->msg_ctx = hapd;
		hostapd_bss_setup_multi_link(hapd, interfaces);
		if (hostapd_validate_bss_tx_params(hapd) < 0)
			goto fail;

		if (hapd->conf->ht_mcs_nss_set) {
			if (hostapd_tx_bss_only(hapd, "ht_mcs_nss_set") < 0)
				goto fail;
		}
#if defined(CONFIG_HOSTAPD_IF) && defined(CONFIG_QCN_EXTN)
		hostapd_if_interface_create(hapd);
#endif
	}

	hapd_iface->is_ch_switch_dfs = false;

	atf_init_algo(hapd_iface);

	return hapd_iface;

fail:
	wpa_printf(MSG_ERROR, "Failed to set up interface with %s",
		   config_file);
	if (conf)
		hostapd_config_free(conf);
	if (hapd_iface) {
		os_free(hapd_iface->config_fname);
		os_free(hapd_iface->bss);
		wpa_printf(MSG_DEBUG, "%s: free iface %p",
			   __func__, hapd_iface);
		os_free(hapd_iface);
	}
	return NULL;
}


/**
 * hostapd_check_dup_link - Check if an MLD link add request is a duplicate
 * @tmp_hapd: Existing hostapd_data for the ifname requested for the new link
 * @new_channel: Channel of new BSS
 * @hw_idx: Iface hw_idx of new BSS
 *
 * tmp_hapd may already be operating on a different frequency as another
 * link of the same MLD sharing this ifname - that is a legitimate
 * additional link, not a duplicate. This only flags a duplicate when an
 * existing link of the same MLD is already enabled on the very same
 * frequency that is being requested here.
 *
 * Returns: true if the requested link is a duplicate of an existing link,
 * false otherwise.
 */
static bool hostapd_check_dup_link(struct hostapd_data *tmp_hapd,
				   const u8 new_channel,
				   const u8 hw_idx)
{
	struct hostapd_data *phapd;
	int new_freq = 0;

	/*
	 * Look up the new link's frequency from an existing partner link's
	 * iface channel table, if the frequency is found in any of the partner
	 * link's and the hw_idx matches the new BSS iface, reject the link
	 * addition
	 */
#ifdef CONFIG_QCN_EXTN
	for_each_mld_link_include_repurposed(phapd, tmp_hapd) {
#else
	for_each_mld_link(phapd, tmp_hapd) {
#endif
		new_freq = hostapd_hw_get_freq(phapd, new_channel);
		if (new_freq && phapd->iface->current_hw_info &&
		    phapd->iface->current_hw_info->hw_idx == hw_idx) {
			wpa_printf(MSG_ERROR, "Found another link in the same MLD, rejecting the link addition freq %d hw_idx %u new_hw_idx:%u",
				   phapd->iface->freq,
				   phapd->iface->current_hw_info->hw_idx, hw_idx);
			return true;
		}
	}

	wpa_printf(MSG_DEBUG, "Proceed setup for ML AP link addition");
	return false;
}


/**
 * hostapd_interface_init_bss - Read configuration file and init BSS data
 *
 * This function is used to parse configuration file for a BSS. This BSS is
 * added to an existing interface sharing the same radio (if any) or a new
 * interface is created if this is the first interface on a radio. This
 * allocate memory for the BSS. No actual driver operations are started.
 *
 * This is similar to hostapd_interface_init(), but for a case where the
 * configuration is used to add a single BSS instead of all BSSes for a radio.
 */
struct hostapd_iface *
hostapd_interface_init_bss(struct hapd_interfaces *interfaces, const char *phy,
			   const char *config_fname, int debug)
{
	struct hostapd_iface *new_iface = NULL, *iface = NULL;
	struct hostapd_data *hapd;
	struct hostapd_config *conf;
	int k;
	size_t i, bss_idx;

	if (!phy || !*phy)
		return NULL;

	conf = interfaces->config_read_cb(config_fname);
	if (!conf)
		return NULL;

	for (i = 0; i < interfaces->count; i++) {
		if (os_strcmp(interfaces->iface[i]->phy, phy) == 0) {
			iface = interfaces->iface[i];
			break;
		}
	}

	wpa_printf(MSG_INFO, "Configuration file: %s (phy %s)%s",
		   config_fname, phy, iface ? "" : " --> new PHY");

	if (iface) {
		struct hostapd_bss_config **tmp_conf;
		struct hostapd_data **tmp_bss, *tmp_hapd;
		struct hostapd_bss_config *bss;
		const char *ifname;

		/* Add new BSS to existing iface */
		if (conf->num_bss > 1) {
			wpa_printf(MSG_ERROR, "Multiple BSSes specified in BSS-config");
			hostapd_config_free(conf);
			return NULL;
		}

		if (hostapd_csa_in_progress(iface)) {
			wpa_printf(MSG_ERROR, "Channel switch in progress, cannot add bss");
			hostapd_config_free(conf);
			return NULL;
		}

		ifname = conf->bss[0]->iface;
		if (ifname[0] == '\0') {
			wpa_printf(MSG_ERROR,
				   "Invalid interface name %s", ifname);
			hostapd_config_free(conf);
			return NULL;
		}
		tmp_hapd = hostapd_interfaces_get_hapd(interfaces, ifname);
		if (tmp_hapd) {
			wpa_printf(MSG_ERROR,
				   "Interface name %s already in use", ifname);
			if (!conf->bss[0]->mld_ap || !tmp_hapd->conf->mld_ap ||
			    (iface->current_hw_info &&
			     hostapd_check_dup_link(tmp_hapd, conf->channel,
						    iface->current_hw_info->hw_idx))) {
				hostapd_config_free(conf);
				return NULL;
			}
		}

		tmp_conf = os_realloc_array(
			iface->conf->bss, iface->conf->num_bss + 1,
			sizeof(struct hostapd_bss_config *));
		tmp_bss = os_realloc_array(iface->bss, iface->num_bss + 1,
					   sizeof(struct hostapd_data *));
		if (tmp_bss)
			iface->bss = tmp_bss;
		if (tmp_conf) {
			iface->conf->bss = tmp_conf;
			iface->conf->last_bss = tmp_conf[0];
		}
		if (tmp_bss == NULL || tmp_conf == NULL) {
			hostapd_config_free(conf);
			return NULL;
		}
		bss = iface->conf->bss[iface->conf->num_bss] = conf->bss[0];
		iface->conf->num_bss++;

		hapd = hostapd_alloc_bss_data(iface, iface->conf, bss);
		if (hapd == NULL) {
			iface->conf->num_bss--;
			hostapd_config_free(conf);
			return NULL;
		}
		iface->conf->last_bss = bss;
		iface->bss[iface->num_bss] = hapd;
		hapd->msg_ctx = hapd;
		hostapd_bss_setup_multi_link(hapd, interfaces);

		/* Validate BSS capabilities if driver is initialized */
		if (iface->current_mode &&
		    hostapd_validate_bss_capab(hapd) < 0) {
			wpa_printf(MSG_ERROR,
				   "BSS capability validation failed for %s",
				   hapd->conf->iface);
			iface->conf->num_bss--;
			hostapd_config_free(conf);
			return NULL;
		}

		bss_idx = iface->num_bss++;

		conf->num_bss--;
		conf->bss[0] = NULL;
		hostapd_config_free(conf);
	} else {
		hostapd_config_free(conf);

		/* Add a new iface with the first BSS */
		new_iface = iface = hostapd_init(interfaces, config_fname);
		if (!iface)
			return NULL;
		os_strlcpy(iface->phy, phy, sizeof(iface->phy));
		iface->interfaces = interfaces;
		bss_idx = 0;
	}

	for (k = 0; k < debug; k++) {
		if (iface->bss[bss_idx]->conf->logger_stdout_level > 0)
			iface->bss[bss_idx]->conf->logger_stdout_level--;
	}

	if (iface->conf->bss[bss_idx]->iface[0] == '\0' &&
	    !hostapd_drv_none(iface->bss[bss_idx])) {
		wpa_printf(MSG_ERROR, "Interface name not specified in %s",
			   config_fname);
		if (new_iface)
			hostapd_interface_deinit_free(new_iface);
		return NULL;
	}

	return iface;
}


static void hostapd_cleanup_driver(const struct wpa_driver_ops *driver,
				   void *drv_priv, struct hostapd_iface *iface)
{
	if (!driver || !driver->hapd_deinit || !drv_priv)
		return;

#ifdef CONFIG_IEEE80211BE
	if (!driver->is_drv_shared ||
	    !driver->is_drv_shared(drv_priv, iface->bss[0]->mld_link_id)) {
		driver->hapd_deinit(drv_priv);
		hostapd_mld_interface_freed(iface->bss[0]);
		iface->bss[0]->drv_priv = NULL;
		return;
	}

	if (iface->bss[0]->conf->mld_ap) {
		if (hostapd_if_link_remove(iface->bss[0],
					WPA_IF_AP_BSS,
					iface->bss[0]->conf->iface,
					iface->bss[0]->mld_link_id))
			wpa_printf(MSG_WARNING,
				   "Failed to remove link BSS interface %s",
				   iface->bss[0]->conf->iface);
	} else if (hostapd_if_remove(iface->bss[0], WPA_IF_AP_BSS,
				     iface->bss[0]->conf->iface)) {
		wpa_printf(MSG_WARNING, "Failed to remove BSS interface %s",
			   iface->bss[0]->conf->iface);
	}
#else /* CONFIG_IEEE80211BE */
	driver->hapd_deinit(drv_priv);
#endif /* CONFIG_IEEE80211BE */
	iface->bss[0]->drv_priv = NULL;
}


void hostapd_interface_deinit_free(struct hostapd_iface *iface)
{
	const struct wpa_driver_ops *driver;
	void *drv_priv;

	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	if (iface == NULL)
		return;
	wpa_printf(MSG_DEBUG, "%s: num_bss=%u conf->num_bss=%u",
		   __func__, (unsigned int) iface->num_bss,
		   (unsigned int) iface->conf->num_bss);
#ifdef CONFIG_QCN_EXTN
	hostapd_uplink_cancel_disconnect_timeout_extn(iface);
#endif /* CONFIG_QCN_EXTN */
	driver = iface->bss[0]->driver;
	atf_offload_disable_atf_stats(iface);
	hostapd_ubus_free_iface(iface);
	hostapd_interface_deinit(iface);
	drv_priv = iface->bss[0]->drv_priv;
	wpa_printf(MSG_DEBUG, "%s: driver=%p drv_priv=%p -> hapd_deinit",
		   __func__, driver, drv_priv);
	hostapd_cleanup_driver(driver, drv_priv, iface);
	hostapd_interface_free(iface);
}

void hostapd_deauthenticate_stations(struct hapd_interfaces *interfaces)
{
	int i, j;
	struct hostapd_iface *iface;
	struct hostapd_data *hapd;
	u8 addr[ETH_ALEN];
	int reason = WLAN_REASON_DEAUTH_LEAVING;

	for (i = 0; i < interfaces->count; i++) {
		if (!interfaces->iface[i])
			continue;

		iface = interfaces->iface[i];
		os_memset(addr, 0xff, ETH_ALEN);
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			if (!hapd)
				continue;
			wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
					"Sending deauth frame sa=" MACSTR "da=" MACSTR "reason=%d",
					MAC2STR(hapd->own_addr), MAC2STR(addr), reason);
			hostapd_drv_sta_deauth(hapd, addr, reason);
		}

	}
}

static void hostapd_deinit_driver(const struct wpa_driver_ops *driver,
				  void *drv_priv,
				  struct hostapd_iface *hapd_iface)
{
	size_t j;

	wpa_printf(MSG_DEBUG, "%s: driver=%p drv_priv=%p -> hapd_deinit",
		   __func__, driver, drv_priv);

	hostapd_cleanup_driver(driver, drv_priv, hapd_iface);

	if (driver && driver->hapd_deinit && drv_priv) {
		for (j = 0; j < hapd_iface->num_bss; j++) {
			wpa_printf(MSG_DEBUG, "%s:bss[%d]->drv_priv=%p",
				   __func__, (int) j,
				   hapd_iface->bss[j]->drv_priv);
			if (hapd_iface->bss[j]->drv_priv == drv_priv) {
				hapd_iface->bss[j]->drv_priv = NULL;
				hapd_iface->extended_capa = NULL;
				hapd_iface->extended_capa_mask = NULL;
				hapd_iface->extended_capa_len = 0;
			}
		}
	}
}


void hostapd_refresh_other_iface_beacons(struct hostapd_iface *hapd_iface)
{
	size_t j;

	if (!hapd_iface->interfaces || hapd_iface->interfaces->count <= 1)
		return;

	for (j = 0; j < hapd_iface->interfaces->count; j++) {
		if (hapd_iface->interfaces->iface[j] == hapd_iface)
			continue;

		ieee802_11_update_beacons(hapd_iface->interfaces->iface[j]);
	}
}


void hostapd_refresh_all_iface_beacons(struct hostapd_iface *hapd_iface)
{
	size_t j;

	if (!hapd_iface->interfaces)
		return;

	for (j = 0; j < hapd_iface->interfaces->count; j++)
		ieee802_11_update_beacons(hapd_iface->interfaces->iface[j]);
}


int hostapd_enable_iface(struct hostapd_iface *hapd_iface)
{
	size_t j;

	if (!hapd_iface)
		return -1;

	if (hapd_iface->enable_iface_cb)
		return hapd_iface->enable_iface_cb(hapd_iface);

	if (hapd_iface->bss[0]->drv_priv != NULL) {
		wpa_printf(MSG_ERROR, "Interface %s already enabled",
			   hapd_iface->conf->bss[0]->iface);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Enable interface %s",
		   hapd_iface->conf->bss[0]->iface);

	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_set_security_params(hapd_iface->conf->bss[j], 1);
	if (hostapd_config_check(hapd_iface->conf, 1) < 0) {
		wpa_printf(MSG_INFO, "Invalid configuration - cannot enable");
		return -1;
	}

	if (hapd_iface->interfaces == NULL ||
	    hapd_iface->interfaces->driver_init == NULL ||
	    hapd_iface->interfaces->driver_init(hapd_iface)) {
		hostapd_deinit_driver(hapd_iface->bss[0]->driver,
				      hapd_iface->bss[0]->drv_priv,
				      hapd_iface);
#ifdef CONFIG_QCN_EXTN
		hostapd_log_trigger_emit(hapd_iface->bss[0], NULL,
					 HOSTAPD_LOG_TRIG_VAP_UP_FAIL);
#endif /* CONFIG_QCN_EXTN */
		return -1;
	}

	if (hostapd_setup_interface(hapd_iface)) {
		hostapd_bss_link_deinit(hapd_iface->bss[0]);
		hostapd_deinit_driver(hapd_iface->bss[0]->driver,
				      hapd_iface->bss[0]->drv_priv,
				      hapd_iface);
		return -1;
	}

	hostapd_refresh_other_iface_beacons(hapd_iface);

	return 0;
}


int hostapd_reload_iface(struct hostapd_iface *hapd_iface)
{
	size_t j;

	wpa_printf(MSG_DEBUG, "Reload interface %s",
		   hapd_iface->conf->bss[0]->iface);
	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_set_security_params(hapd_iface->conf->bss[j], 1);
	if (hostapd_config_check(hapd_iface->conf, 1) < 0) {
		wpa_printf(MSG_ERROR, "Updated configuration is invalid");
		return -1;
	}
	hostapd_clear_old(hapd_iface);
	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_reload_bss(hapd_iface->bss[j]);

	return 0;
}


int hostapd_reload_bss_only(struct hostapd_data *bss)
{

	wpa_printf(MSG_DEBUG, "Reload BSS %s", bss->conf->iface);
	hostapd_set_security_params(bss->conf, 1);
	if (hostapd_config_check(bss->iconf, 1) < 0) {
		wpa_printf(MSG_ERROR, "Updated BSS configuration is invalid");
		return -1;
	}
	hostapd_clear_old_bss(bss);
	hostapd_reload_bss(bss);
	return 0;
}

int hostapd_disable_bss(struct hostapd_data *hapd, int tbtt, const char *event)
{
	size_t i;

#ifdef CONFIG_IEEE80211BE
	if (tbtt > 0 && hapd->iface &&
	    !hostapd_link_remove(hapd, tbtt, HAPD_LINK_DISABLE)) {
		wpa_printf(MSG_INFO,
			   "Reconfigure for ML BSS is started, will be disabled after %d TBTT",
			   tbtt);
		return 0;
	}
#endif /* CONFIG_IEEE80211BE */
	hapd->disabled = 1;
	wpa_msg(hapd->msg_ctx, MSG_INFO, "%s", event);

#ifdef CONFIG_IEEE80211BN
	mapc_deinit(hapd);
#endif /* CONFIG_IEEE80211BN */

	hostapd_bss_deinit_no_free(hapd);

	/* Stop AP at driver level: no more beacons/tx for this BSS. */
	hostapd_drv_stop_ap(hapd);

	/* Deinitialize higher-level BSS state but keep netdev/link. */
	hapd->reenable = REENABLE_REUSE_LINK;
	hostapd_bss_link_deinit(hapd);
	hostapd_free_hapd_data(hapd);
	hostapd_cleanup_cca_params(hapd);
	hostapd_cleanup_cs_params(hapd);

	for (i = 0; i < hapd->iface->num_bss; i++) {
		if (hapd->iface->bss[i]->started)
			break;
	}

	if (i == hapd->iface->num_bss) {
		hapd->iface->cac_type = 0;
		hapd->iface->csa_pending_on_cac_abort = false;
		hapd->iface->cac_started = 0;
#ifdef CONFIG_QCN_EXTN
		hapd->iface->bootup_cac_in_progress = 0;
#endif
		os_memset(&hapd->iface->csa_settings, 0, sizeof(struct csa_settings));
		os_memset(&hapd->iface->radar_background, 0,
			  sizeof(hapd->iface->radar_background));
		/*
		 * channel == -1 is the sentinel the DFS background-radar code
		 * checks for "no cached channel"; os_memset() above leaves it
		 * 0, which is a channel number, not the sentinel.
		 */
		hapd->iface->radar_background.channel = -1;
		hostapd_interface_update_fils_ubpr(hapd->iface, false);
	}
	ieee802_11_update_beacon_mbssid(hapd);

	ieee802_11_set_beacon(hapd);

	return 0;
}

enum bss_enable_state {
	ENABLE_BSS_OK,
	ENABLE_BSS_DEFER,
	ENABLE_BSS_SETUP,
	ENABLE_BSS_ERROR,
};

/* Decide whether HT scan is needed or enable should be deferred. */
static enum bss_enable_state
hostapd_enable_bss_handle_ht_scan(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	int res, b;

	for (b = 0; b < iface->num_bss; b++)
		if (iface->bss[b]->started)
			return ENABLE_BSS_SETUP;

	if (iface->state != HAPD_IFACE_HT_SCAN) {
		res = hostapd_check_ht_capab(iface);
		if (res < 0) {
			hapd->reenable = REENABLE_NONE;
			wpa_printf(MSG_INFO, "Failed to start HT Scan");
			return ENABLE_BSS_ERROR;
		}

		if (res == 1) {
			hapd->reenable = REENABLE_HT_SCAN;
			wpa_printf(MSG_DEBUG, "Interface initialization will "
				   "be completed in a callback");
			return ENABLE_BSS_DEFER;
		}
	}

	return ENABLE_BSS_OK;
}

/* Handle CAC/DFS once HT scan has completed. */
static enum bss_enable_state
hostapd_enable_bss_handle_cac(struct hostapd_data *hapd)
{
#ifdef NEED_AP_MLME
	struct hostapd_iface *iface = hapd->iface;
	int res;

	if (!is_5ghz_freq(iface->freq))
		return ENABLE_BSS_SETUP;

	/* Handle DFS only if it is not offloaded to the driver */
	if (!(iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)) {
#ifdef CONFIG_QCN_EXTN
		if (hostapd_bootup_cac_start_extn(iface))
			return ENABLE_BSS_SETUP;
#endif /* CONFIG_QCN_EXTN */

		/* Check DFS */
		/* Refresh hw features from the driver so that the channel DFS
		 * state reflects the kernel's authoritative view, including NOL
		 * entries, before running hostapd_handle_dfs().
		 */
		if (hostapd_get_hw_features(iface) ||
		    hostapd_select_hw_mode(iface) < 0 ||
		    hostapd_set_current_hw_info(iface, iface->freq)) {
			hapd->reenable = REENABLE_NONE;
			wpa_printf(MSG_ERROR,
				   "DFS: hw feature refresh failed for BSS %s",
				   hapd->conf->iface);
			return ENABLE_BSS_ERROR;
		}
		res = hostapd_handle_dfs(iface);
		if (res <= 0) {
			if (res < 0) {
				hapd->reenable = REENABLE_NONE;
				wpa_printf(MSG_ERROR,
					   "DFS handling failed for BSS %s",
					   hapd->conf->iface);
				return ENABLE_BSS_ERROR;
			}

			/*
			 * CAC/DFS continuation is expected only if CAC was actually
			 * started; otherwise proceed with BSS setup.
			 */
			if (!iface->cac_started)
				return ENABLE_BSS_SETUP;
			hapd->reenable = REENABLE_CAC;
			return ENABLE_BSS_DEFER;
		}
	} else {
		/* If DFS is offloaded to the driver */
		res = hostapd_handle_dfs_offload(iface);
		if (res <= 0) {
			if (res < 0) {
				hapd->reenable = REENABLE_NONE;
				wpa_printf(MSG_ERROR,
					   "DFS offload handling failed for BSS %s",
					   hapd->conf->iface);
				return ENABLE_BSS_ERROR;
			}
			hapd->reenable = REENABLE_CAC;
			return ENABLE_BSS_DEFER;
		}

		wpa_printf(MSG_DEBUG, "Proceed with AP/channel setup");
		/*
		 * If this is a DFS channel, move to completing
		 * AP setup.
		 */
			if (res == 1)
				return ENABLE_BSS_SETUP;
		}
#endif /* NEED_AP_MLME */

	return ENABLE_BSS_SETUP;
}

int hostapd_enable_bss(struct hostapd_data *hapd)
{
	struct hostapd_iface *hapd_iface = hapd->iface;
	enum bss_enable_state enable_state;
	size_t i;

	/*
	 * Channel list update pending - hw_features have not been fetched yet
	 * and current_mode is NULL. setup_interface2() will re-enable all
	 * BSSes via hostapd_setup_bss() once the update completes. No reenable
	 * state is needed here unlike CAC/HT_SCAN deferral since
	 * setup_interface2() unconditionally sets up all BSSes.
	 */
	if (hapd->iface->wait_channel_update) {
		wpa_printf(MSG_INFO, "Channel update pending, cannot enable BSS %s",
			   hapd->conf->iface);
		return 0;
	}

	if (hapd->started) {
#ifdef CONFIG_QCN_EXTN
		if (hostapd_bootup_cac_enabled_extn(hapd_iface) &&
		    hapd_iface->bootup_cac_in_progress) {
			wpa_printf(MSG_DEBUG,
				   "BSS %s already staged during CAC",
				   hapd->conf->iface);
			return 0;
		}
#endif /* CONFIG_QCN_EXTN */
		wpa_printf(MSG_INFO, "BSS %s already enabled",
			   hapd->conf->iface);
		return -1;
	}

	for (i = 0; i < hapd_iface->num_bss; i++) {
		if (hapd_iface->bss[i]->started)
			break;
	}

	wpa_printf(MSG_DEBUG, "Enable BSS %s", hapd->conf->iface);

	/* Enable flow: HT scan -> CAC/DFS -> setup_bss. */
#ifdef CONFIG_QCN_EXTN
	if (hostapd_bootup_cac_enabled_extn(hapd_iface) &&
	    hapd_iface->bootup_cac_in_progress) {
		wpa_printf(MSG_DEBUG,
			   "CAC in progress: setting up BSS %s",
			   hapd->conf->iface);
		goto setup_bss;
	}
#endif /* CONFIG_QCN_EXTN */

	if (hapd_iface->cac_started) {
		hapd->reenable = REENABLE_CAC;
		wpa_printf(MSG_INFO, "CAC in progress, cannot enable BSS");
		return 0;
	}

	/* HT scan in progress: defer enable until scan completes. */
	if (hapd_iface->state == HAPD_IFACE_HT_SCAN &&
	    hapd_iface->scan_cb && !hapd->started) {
		hapd->reenable = REENABLE_HT_SCAN;
		wpa_printf(MSG_INFO, "HT scan in progress, cannot enable BSS");
		return 0;
	}

	if (hapd->reenable == REENABLE_HT_SCAN)
		goto handle_cac;

	if (hapd->reenable == REENABLE_CAC) {
		/* If CSA is in progress, defer until the channel switch
		 * and subsequent CAC complete before setting up the BSS.
		 */
		if (hostapd_csa_in_progress(hapd_iface)) {
			wpa_printf(MSG_DEBUG,
				   "CSA in progress, defer enable BSS %s",
				   hapd->conf->iface);
			return 0;
		}
		/*
		 * If the BSS was stopped (beacon_set_done == 0) before the CAC
		 * started, it has no valid channel context. Run DFS/CAC on the
		 * new channel before setting up the BSS.
		 */
		if (!hapd->beacon_set_done)
			goto handle_cac;
		goto setup_bss;
	}

	enable_state = hostapd_enable_bss_handle_ht_scan(hapd);
	if (enable_state == ENABLE_BSS_DEFER)
		return 0;
	if (enable_state == ENABLE_BSS_ERROR)
		return -1;
	if (enable_state == ENABLE_BSS_SETUP)
		goto setup_bss;

handle_cac:
	enable_state = hostapd_enable_bss_handle_cac(hapd);
	if (enable_state == ENABLE_BSS_DEFER)
		return 0;
	if (enable_state == ENABLE_BSS_ERROR)
		return -1;

setup_bss:
	/* Configure security parameters for this BSS. */
	hostapd_set_security_params(hapd->conf, 1);
	if (hostapd_config_check(hapd->iconf, 1) < 0) {
		wpa_printf(MSG_ERROR, "Updated BSS configuration is invalid");
		return -1;
	}

	/* Re-setup this BSS without adding netdev/link again. */
	if (hostapd_setup_bss(hapd, false, true)) {
		hapd->reenable = REENABLE_NONE;
		wpa_printf(MSG_ERROR, "Failed to re-enable BSS %s",
			   hapd->conf->iface);
		return -1;
	}

	hapd->reenable = REENABLE_NONE;
	hapd->disabled = 0;
#ifdef CONFIG_QCN_EXTN
	if (!hapd_iface->bootup_cac_in_progress)
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_ENABLED);
#else
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_ENABLED);
#endif

	hostapd_neighbor_set_own_report(hapd);
	if (i == hapd_iface->num_bss)
		hostapd_interface_update_fils_ubpr(hapd_iface, true);

	return 0;
}

int hostapd_disable_iface(struct hostapd_iface *hapd_iface)
{
	size_t j;

	if (hapd_iface == NULL)
		return -1;

	if (hapd_iface->disable_iface_cb)
		return hapd_iface->disable_iface_cb(hapd_iface);

	if (hapd_iface->bss[0]->drv_priv == NULL) {
		wpa_printf(MSG_INFO, "Interface %s already disabled",
			   hapd_iface->conf->bss[0]->iface);
		return -1;
	}

	wpa_msg(hapd_iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);

	hapd_iface->driver_ap_teardown =
		!!(hapd_iface->drv_flags &
		   WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);

#ifdef NEED_AP_MLME
	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_cleanup_cs_params(hapd_iface->bss[j]);

	/* OCE 4.3.1/4.3.2: cancel survey timer before BSS teardown so a
	 * late EVENT_SCAN_RESULTS cannot fire into a deinitialized BSS */
	hostapd_oce_survey_timer_cancel(hapd_iface);
#endif /* NEED_AP_MLME */

	/* same as hostapd_interface_deinit without deinitializing ctrl-iface */
	for (j = 0; j < hapd_iface->num_bss; j++) {
		struct hostapd_data *hapd = hapd_iface->bss[j];
#ifdef CONFIG_IEEE80211BN
		mapc_deinit(hapd);
#endif /* CONFIG_IEEE80211BN */
		hapd->reenable = REENABLE_DEINIT;
		hostapd_bss_deinit_no_free(hapd);
		hostapd_bss_link_deinit(hapd);
		hostapd_free_hapd_data(hapd);
#ifdef CONFIG_IEEE80211BE
	/* Retain the link ID to ensure that the disabled interface link ID is not
	 * utilized during dynamic link addition.
	 */
		if (hapd->mld)
			hapd->mld->free_links &= ~BIT(hapd->mld_link_id);
#endif /* CONFIG_IEEE80211BE */
	}

	hostapd_deinit_driver(hapd_iface->bss[0]->driver,
			      hapd_iface->bss[0]->drv_priv, hapd_iface);

	/* From hostapd_cleanup_iface: These were initialized in
	 * hostapd_setup_interface and hostapd_setup_interface_complete
	 */
	hostapd_cleanup_iface_partial(hapd_iface);

	wpa_printf(MSG_DEBUG, "Interface %s disabled",
		   hapd_iface->bss[0]->conf->iface);
	hostapd_set_state(hapd_iface, HAPD_IFACE_DISABLED);
	hostapd_interface_update_fils_ubpr(hapd_iface, false);
	hostapd_refresh_other_iface_beacons(hapd_iface);
	return 0;
}


static struct hostapd_iface *
hostapd_iface_alloc(struct hapd_interfaces *interfaces)
{
	struct hostapd_iface **iface, *hapd_iface;

	iface = os_realloc_array(interfaces->iface, interfaces->count + 1,
				 sizeof(struct hostapd_iface *));
	if (iface == NULL)
		return NULL;
	interfaces->iface = iface;
	hapd_iface = interfaces->iface[interfaces->count] =
		hostapd_alloc_iface();
	if (hapd_iface == NULL) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory for "
			   "the interface", __func__);
		return NULL;
	}
	interfaces->count++;
	hapd_iface->interfaces = interfaces;

	return hapd_iface;
}


static struct hostapd_config *
hostapd_config_alloc(struct hapd_interfaces *interfaces, const char *ifname,
		     const char *ctrl_iface, const char *driver)
{
	struct hostapd_bss_config *bss;
	struct hostapd_config *conf;

	/* Allocates memory for bss and conf */
	conf = hostapd_config_defaults();
	if (conf == NULL) {
		 wpa_printf(MSG_ERROR, "%s: Failed to allocate memory for "
				"configuration", __func__);
		 return NULL;
	}

	if (driver) {
		int j;

		for (j = 0; wpa_drivers[j]; j++) {
			if (os_strcmp(driver, wpa_drivers[j]->name) == 0) {
				conf->driver = wpa_drivers[j];
				goto skip;
			}
		}

		wpa_printf(MSG_ERROR,
			   "Invalid/unknown driver '%s' - registering the default driver",
			   driver);
	}

	conf->driver = wpa_drivers[0];
	if (conf->driver == NULL) {
		wpa_printf(MSG_ERROR, "No driver wrappers registered!");
		hostapd_config_free(conf);
		return NULL;
	}

skip:
	bss = conf->last_bss = conf->bss[0];

	os_strlcpy(bss->iface, ifname, sizeof(bss->iface));
	bss->ctrl_interface = os_strdup(ctrl_iface);
	if (bss->ctrl_interface == NULL) {
		hostapd_config_free(conf);
		return NULL;
	}

	/* Reading configuration file skipped, will be done in SET!
	 * From reading the configuration till the end has to be done in
	 * SET
	 */
	return conf;
}


static int hostapd_data_alloc(struct hostapd_iface *hapd_iface,
			      struct hostapd_config *conf)
{
	size_t i;
	struct hostapd_data *hapd;

	hapd_iface->bss = os_calloc(conf->num_bss,
				    sizeof(struct hostapd_data *));
	if (hapd_iface->bss == NULL)
		return -1;

	for (i = 0; i < conf->num_bss; i++) {
		hapd = hapd_iface->bss[i] =
			hostapd_alloc_bss_data(hapd_iface, conf, conf->bss[i]);
		if (hapd == NULL) {
			while (i > 0) {
				i--;
				os_free(hapd_iface->bss[i]);
				hapd_iface->bss[i] = NULL;
			}
			os_free(hapd_iface->bss);
			hapd_iface->bss = NULL;
			return -1;
		}
		hapd->msg_ctx = hapd;
		hostapd_bss_setup_multi_link(hapd, hapd_iface->interfaces);
	}

	hapd_iface->conf = conf;
	hapd_iface->num_bss = conf->num_bss;

	hapd->cca_count = HE_BSS_COLOR_CCA_COUNT_DEFAULT;
	return 0;
}

int hostapd_add_iface(struct hapd_interfaces *interfaces, char *buf)
{
	struct hostapd_config *conf = NULL;
	struct hostapd_iface *hapd_iface = NULL, *new_iface = NULL;
	struct hostapd_data *hapd;
	bool setup_bss;
	char *ptr;
	size_t i, j;
	const char *conf_file = NULL, *phy_name = NULL;

	if (os_strncmp(buf, "bss_config=", 11) == 0) {
		char *pos;
		phy_name = buf + 11;
		pos = os_strchr(phy_name, ':');
		if (!pos)
			return -1;
		*pos++ = '\0';
		conf_file = pos;
		if (!os_strlen(conf_file))
			return -1;

		hapd_iface = hostapd_interface_init_bss(interfaces, phy_name,
							conf_file, 0);
		if (!hapd_iface)
			return -1;
		for (j = 0; j < interfaces->count; j++) {
			if (interfaces->iface[j] == hapd_iface)
				break;
		}
		if (j == interfaces->count) {
			struct hostapd_iface **tmp;
			tmp = os_realloc_array(interfaces->iface,
					       interfaces->count + 1,
					       sizeof(struct hostapd_iface *));
			if (!tmp) {
				hostapd_interface_deinit_free(hapd_iface);
				return -1;
			}
			interfaces->iface = tmp;
			interfaces->iface[interfaces->count++] = hapd_iface;
			new_iface = hapd_iface;
		}

		if (new_iface) {
			if (interfaces->driver_init(hapd_iface)) {
#ifdef CONFIG_QCN_EXTN
				hostapd_log_trigger_emit(hapd_iface->bss[0], NULL,
						 HOSTAPD_LOG_TRIG_VAP_UP_FAIL);
#endif /* CONFIG_QCN_EXTN */
				hostapd_deinit_driver(
					hapd_iface->bss[0]->driver,
					hapd_iface->bss[0]->drv_priv,
					hapd_iface);
				goto fail;
			}

#ifdef CONFIG_QCN_EXTN
			hostapd_iface_set_supplicant_channel_extn(hapd_iface);
#endif

			if (hostapd_setup_interface(hapd_iface)) {
				/* Deinit link for the first bss */
				hostapd_bss_link_deinit(hapd_iface->bss[0]);
				hostapd_deinit_driver(
					hapd_iface->bss[0]->driver,
					hapd_iface->bss[0]->drv_priv,
					hapd_iface);
				goto fail;
			}
		} else {
			int ret;

			/* Assign new BSS with bss[0]'s driver info */
			hapd = hapd_iface->bss[hapd_iface->num_bss - 1];
			hapd->driver = hapd_iface->bss[0]->driver;
			hapd->drv_priv = hapd_iface->bss[0]->drv_priv;
			os_memcpy(hapd->own_addr, hapd_iface->bss[0]->own_addr,
				  ETH_ALEN);

			setup_bss = hapd_iface->state == HAPD_IFACE_ENABLED;
#ifdef CONFIG_QCN_EXTN
			setup_bss |=
				hostapd_bootup_cac_enabled_extn(hapd_iface);
#endif /* CONFIG_QCN_EXTN */

			ret = start_ctrl_iface_bss(hapd);
			if (!ret && setup_bss) {
				ret = hostapd_setup_bss(hapd, false, true);
#ifdef CONFIG_QCN_EXTN
				if (ret)
					hostapd_free_bss_index_extn(hapd);
#endif /* CONFIG_QCN_EXTN */
			}

			if (ret) {
#ifdef CONFIG_QCN_EXTN
				hostapd_log_trigger_emit(hapd, NULL,
							 HOSTAPD_LOG_TRIG_VAP_CREATE_FAIL);
#endif /* CONFIG_QCN_EXTN */
				hostapd_bss_link_deinit(hapd);
				hostapd_cleanup(hapd);
				hapd_iface->bss[hapd_iface->num_bss - 1] = NULL;
				hapd_iface->conf->num_bss--;
				hapd_iface->num_bss--;
				wpa_printf(MSG_DEBUG, "%s: free hapd %p %s",
					   __func__, hapd, hapd->conf->iface);
				hostapd_free_mbssid_idx(hapd);
				hostapd_multi_mbssid_remove_bss(hapd);
				hostapd_config_free_bss(hapd->conf);
				hapd->conf = NULL;
#ifdef CONFIG_IEEE80211BE
				hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
				os_free(hapd);
				return -1;
			}

			if (hapd_iface->state == HAPD_IFACE_ENABLED)
				wpa_msg(hapd->msg_ctx, MSG_INFO,
					AP_EVENT_ENABLED);
		}
		hostapd_owe_update_trans(hapd_iface);
		return 0;
	}

	ptr = os_strchr(buf, ' ');
	if (ptr == NULL)
		return -1;
	*ptr++ = '\0';

	if (os_strncmp(ptr, "config=", 7) == 0)
		conf_file = ptr + 7;

	for (i = 0; i < interfaces->count; i++) {
		bool mld_ap = false;

#ifdef CONFIG_IEEE80211BE
		mld_ap = interfaces->iface[i]->conf->bss[0]->mld_ap;
#endif /* CONFIG_IEEE80211BE */

		if (!os_strcmp(interfaces->iface[i]->conf->bss[0]->iface,
			       buf) && !mld_ap) {
			wpa_printf(MSG_INFO, "Cannot add interface - it "
				   "already exists");
			return -1;
		}
	}

	hapd_iface = hostapd_iface_alloc(interfaces);
	if (hapd_iface == NULL) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory "
			   "for interface", __func__);
		goto fail;
	}
	new_iface = hapd_iface;

	if (conf_file && interfaces->config_read_cb) {
		conf = interfaces->config_read_cb(conf_file);
		if (conf && conf->bss)
			os_strlcpy(conf->bss[0]->iface, buf,
				   sizeof(conf->bss[0]->iface));
	} else {
		char *driver = os_strchr(ptr, ' ');

		if (driver)
			*driver++ = '\0';
		conf = hostapd_config_alloc(interfaces, buf, ptr, driver);
	}

	if (conf == NULL || conf->bss == NULL) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory "
			   "for configuration", __func__);
		goto fail;
	}

	if (hostapd_data_alloc(hapd_iface, conf) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory "
			   "for hostapd", __func__);
		goto fail;
	}
	conf = NULL;

	if (start_ctrl_iface(hapd_iface) < 0)
		goto fail;

	wpa_printf(MSG_INFO, "Add interface '%s'",
		   hapd_iface->conf->bss[0]->iface);

	return 0;

fail:
	if (conf)
		hostapd_config_free(conf);
	if (hapd_iface) {
		if (hapd_iface->bss) {
			for (i = 0; i < hapd_iface->num_bss; i++) {
				hapd = hapd_iface->bss[i];
				if (!hapd)
					continue;
				if (hapd_iface->interfaces &&
				    hapd_iface->interfaces->ctrl_iface_deinit)
					hapd_iface->interfaces->
						ctrl_iface_deinit(hapd);
				wpa_printf(MSG_DEBUG, "%s: free hapd %p (%s)",
					   __func__, hapd_iface->bss[i],
					   hapd->conf->iface);
				hostapd_bss_link_deinit(hapd);
				hostapd_cleanup(hapd);
#ifdef CONFIG_IEEE80211BE
				hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
				hostapd_free_mbssid_idx(hapd);
				hostapd_multi_mbssid_remove_bss(hapd);
				os_free(hapd);
				hapd_iface->bss[i] = NULL;
			}
			os_free(hapd_iface->bss);
			hapd_iface->bss = NULL;
		}
		if (new_iface) {
			interfaces->count--;
			interfaces->iface[interfaces->count] = NULL;
			hostapd_cleanup_unused_mlds(interfaces);
		}
		hostapd_cleanup_iface(hapd_iface);
	}
	return -1;
}


static bool hostapd_iface_in_pre_beacon_state(struct hostapd_iface *iface)
{
	switch (iface->state) {
	case HAPD_IFACE_ACS:
	case HAPD_IFACE_DFS:
	case HAPD_IFACE_HT_SCAN:
	case HAPD_IFACE_COUNTRY_UPDATE:
		return true;
	default:
		break;
	}

	return false;
}


static int hostapd_prepare_successor_pre_beacon(struct hostapd_iface *iface)
{
	struct hostapd_data *bss_succ;
	char force_ifname[IFNAMSIZ];
	u8 if_addr[ETH_ALEN];
	u8 *addr;
#ifdef CONFIG_IEEE80211BE
	bool iface_added = false;
#endif /* CONFIG_IEEE80211BE */

	if (iface->num_bss < 2)
		return -1;

	bss_succ = iface->bss[1];
	/*
	 * Nothing to do if there is no successor BSS
	 * or it already owns driver ctx.
	 */
	if (!bss_succ)
		return -1;

	if (bss_succ->drv_priv != NULL &&
	    bss_succ->drv_priv != iface->bss[0]->drv_priv) {
		if (bss_succ->started)
			return 1;
		return 0;
	}

	addr = bss_succ->own_addr;
	wpa_printf(MSG_INFO, "iface is in pre-beacon state, prepare successor BSS");
	if (!is_zero_ether_addr(bss_succ->conf->bssid)) {
		os_memcpy(bss_succ->own_addr, bss_succ->conf->bssid, ETH_ALEN);

		if (hostapd_mac_comp(bss_succ->own_addr,
				     bss_succ->iface->bss[0]->own_addr) == 0) {
			wpa_printf(MSG_ERROR, "BSS '%s' may not have BSSID set to the MAC address of the radio",
				   bss_succ->conf->iface);
			return -1;
		}
	} else if (bss_succ->iconf->use_driver_iface_addr) {
		addr = NULL;
#ifdef CONFIG_QCN_EXTN
	} else if (bss_succ->iconf->use_driver_vendor_addr) {
		addr = NULL;
#endif /* CONFIG_QCN_EXTN */
	} else {
		/* Allocate the next available BSSID. */
		do {
			inc_byte_array(bss_succ->own_addr, ETH_ALEN);
		} while (mac_in_conf(bss_succ->iconf, bss_succ->own_addr));
	}

#ifdef CONFIG_IEEE80211BE
	if (bss_succ->conf->mld_ap) {
		struct hostapd_data *h_hapd;

		h_hapd = hostapd_mld_get_first_bss(bss_succ);
		if (h_hapd) {
			bss_succ->drv_priv = h_hapd->drv_priv;
			bss_succ->interface_added = h_hapd->interface_added;
			wpa_printf(MSG_DEBUG,
				   "Setup of non first link (%d) BSS of MLD %s",
				   bss_succ->mld_link_id, bss_succ->conf->iface);
			goto setup_mld;
		}

		iface_added = true;
		if (!is_zero_ether_addr(bss_succ->conf->mld_addr))
			addr = bss_succ->conf->mld_addr;
		else if (bss_succ->iconf->use_driver_iface_addr)
			addr = NULL;
		else
			addr = bss_succ->own_addr;
	}
#endif /* CONFIG_IEEE80211BE */

	bss_succ->interface_added = 1;
	if (hostapd_if_add(iface->bss[0], WPA_IF_AP_BSS,
			   bss_succ->conf->iface, addr, bss_succ,
			   &bss_succ->drv_priv, force_ifname,
			   if_addr, bss_succ->conf->bridge[0] ?
			   bss_succ->conf->bridge : NULL, 1,
			   bss_succ->conf->ppe_vp_type)) {
		wpa_printf(MSG_WARNING,
			   "Failed to add successor BSS (BSSID=" MACSTR ")",
			   MAC2STR(bss_succ->own_addr));
		bss_succ->interface_added = 0;
		return -1;
	}

	if (!addr)
		os_memcpy(bss_succ->own_addr, if_addr, ETH_ALEN);

#ifdef CONFIG_IEEE80211BE
	if (bss_succ->conf->mld_ap) {
		wpa_printf(MSG_DEBUG, "Setup of first link (%d) BSS of MLD %s",
			   bss_succ->mld_link_id, bss_succ->conf->iface);
		os_memcpy(bss_succ->mld->mld_addr, addr ? addr : if_addr,
			  ETH_ALEN);
setup_mld:
		wpa_printf(MSG_DEBUG,
			   "MLD: Set %s link_id=%u, mld_addr=" MACSTR
			   ", own_addr=" MACSTR, bss_succ->conf->iface,
			   bss_succ->mld_link_id,
			   MAC2STR(bss_succ->mld->mld_addr),
			   MAC2STR(bss_succ->own_addr));
		if (hostapd_drv_link_add(bss_succ, bss_succ->mld_link_id,
					 bss_succ->own_addr)) {
			wpa_printf(MSG_ERROR,
				   "MLD: Failed to add link %d in MLD %s",
				   bss_succ->mld_link_id, bss_succ->conf->iface);
			if (iface_added)
				hostapd_if_remove(bss_succ, WPA_IF_AP_BSS,
						  bss_succ->conf->iface);
			bss_succ->interface_added = 0;
			bss_succ->drv_priv = NULL;
			return -1;
		}
		if (hostapd_mld_add_link(bss_succ) < 0) {
			hostapd_mld_remove_link(bss_succ);
			return -1;
		}
		hostapd_validate_update_ml_max_rec_links(bss_succ);
	}
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


int hostapd_remove_bss(struct hostapd_iface *iface, unsigned int idx)
{
	size_t i;
	bool successor_prepared = false;

	wpa_printf(MSG_INFO, "Remove BSS '%s'", iface->conf->bss[idx]->iface);

	/* Remove hostapd_data only if it has already been initialized */
	if (idx < iface->num_bss) {
		struct hostapd_data *hapd = iface->bss[idx];
#ifdef CONFIG_IEEE80211BE
		struct hostapd_data *phapd = NULL;
		u8 active_links = 0;

#ifdef CONFIG_QCN_EXTN
		if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
		/* Save one of the partner bss to update the beacon */
		if (hapd->conf->mld_ap) {
			struct hostapd_data *tmp;

			for_each_mld_link(tmp, hapd) {
				if (tmp == hapd || !tmp->started)
					continue;
				phapd = tmp;
				break;
			}
		}
		active_links = hostapd_get_active_links(hapd);
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */

#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
		char buf[128] = {0};

		if (hapd && hapd->conf && hapd->conf->scs) {
			os_snprintf(buf, sizeof(buf), "%s_%s", CHAIN_NAME,
				    hapd->conf->iface);
			hostapd_config_nft_chain(hapd, TABLE_NAME, buf,
						 false);
		}
#endif
		/*
		 * If the First BSS is being removed while the interface is
		 * in a pre‑beacon state (ACS/DFS/HT scan/country update),
		 * the driver context may not be fully initialized yet.
		 * In such cases, successor BSS #1 must be minimally prepared
		 * so that hostapd’s internal iface model and driver link
		 * state remain consistent during the transition.
		 */
		if (hapd->iface->bss[0] == hapd &&
		    hostapd_iface_in_pre_beacon_state(iface) &&
		    iface->num_bss > 1) {
			int prepare_ret;

			prepare_ret =
				hostapd_prepare_successor_pre_beacon(iface);
			if (prepare_ret == 0)
				successor_prepared = true;
			else if (prepare_ret < 0) {
				wpa_printf(MSG_ERROR,
					  "iface is in pre-beacon state & initialization of successor BSS failed. Hence, removing iface");
				if (hostapd_remove_hapd_iface(iface) == 0)
					return 1;
				return -1;
			}
		}
		hapd->reenable = REENABLE_DEINIT;
		hostapd_bss_deinit(hapd);
		wpa_printf(MSG_DEBUG, "%s: free hapd %p (%s)",
			   __func__, hapd, hapd->conf->iface);
		if (hapd->iface->bss[0] == hapd) {
#ifdef CONFIG_IEEE80211BE
			/* If first bss is removed, if_link_remove/hostapd_if_remove
			 * will not be called in hostapd_remove_bss, hence call
			 * hostapd_if_remove/hostapd_if_link_remove
			 * before calling the remove bss if the
			 * first bss is removed.
			 */
			if (hapd->conf->mld_ap) {
				if (hostapd_if_link_remove(hapd, WPA_IF_AP_BSS,
							   hapd->conf->iface,
							   hapd->mld_link_id))
					wpa_printf(MSG_WARNING,
						   "Failed to remove link BSS interface %s",
						   hapd->conf->iface);
			} else {
#endif /* CONFIG_IEEE80211BE */
				if (hostapd_if_remove(hapd, WPA_IF_AP_BSS,
						      hapd->conf->iface))
					wpa_printf(MSG_WARNING,
						   "Failed to remove BSS interface %s",
						   hapd->conf->iface);
			}
		}

		hostapd_free_mbssid_idx(hapd);
		hostapd_multi_mbssid_remove_bss(hapd);
		hostapd_config_free_bss(hapd->conf);
		hapd->conf = NULL;
#ifdef CONFIG_IEEE80211BE
		hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
		os_free(hapd);

		iface->num_bss--;

		for (i = idx; i < iface->num_bss; i++)
			iface->bss[i] = iface->bss[i + 1];
		iface->bss[iface->num_bss] = NULL;
#ifdef CONFIG_IEEE80211BE
		/* update ML Max recommended links */
		if (phapd && active_links < phapd->conf->ml_max_rec_links)
			hostapd_set_ml_max_rec_links(phapd,
						     active_links);
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BE
		/* BSS REMOVE can drop the last reference to an AP MLD.
		 * Clean up unused MLD objects so their ctrl socket is removed.
		 */
		hostapd_cleanup_unused_mlds(iface->interfaces);
#endif /* CONFIG_IEEE80211BE */

	} else {
		hostapd_config_free_bss(iface->conf->bss[idx]);
		iface->conf->bss[idx] = NULL;
	}

	iface->conf->num_bss--;
	for (i = idx; i < iface->conf->num_bss; i++)
		iface->conf->bss[i] = iface->conf->bss[i + 1];

	if (successor_prepared) {
		wpa_printf(MSG_INFO,
			   "Complete setup for successor bss of the pre_beacon_state iface");
		if (hostapd_setup_interface(iface)) {
			wpa_printf(MSG_ERROR,
				   "setup for successor bss of the pre_beacon_state iface BSS failed. Hence, removing iface");
			if (hostapd_remove_hapd_iface(iface) == 0)
				return 1;
			return -1;
		}
	}

	return 0;
}


int hostapd_remove_hapd_iface(struct hostapd_iface *hapd_iface)
{
	struct hapd_interfaces *interfaces;
	size_t i;

	if (hapd_iface == NULL)
		return -1;

	hapd_iface->driver_ap_teardown =
		!!(hapd_iface->drv_flags &
				WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);

	interfaces = hapd_iface->interfaces;
	for (i = 0; i < interfaces->count; i++) {

		if (interfaces->iface[i] == hapd_iface)
			break;
	}

	if (i >= interfaces->count)
		return -1;

	hostapd_interface_deinit_free(hapd_iface);

	while (i < (interfaces->count - 1)) {
		interfaces->iface[i] =
			interfaces->iface[i + 1];
		i++;
	}
	interfaces->count--;

#ifdef CONFIG_IEEE80211BE
	hostapd_cleanup_unused_mlds(interfaces);
#endif /* CONFIG_IEEE80211BE */
	return 0;
}


void hostapd_remove_non_tx_bsses(struct hostapd_data *tx_bss)
{
	struct hostapd_iface *iface = tx_bss->iface;
	struct hostapd_multi_mbssid_group *grp;
	struct hostapd_data *bss, *tmp;
	size_t k;

	if (iface->conf->mbssid == MBSSID_DISABLED)
		return;

	wpa_printf(MSG_DEBUG, "Remove non-tx bss for the tx bss %s",
		   tx_bss->conf->iface);

	if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		grp = tx_bss->mbssid_group;
		if (!grp) {
			wpa_printf(MSG_ERROR,
				   "MBSSID: No group allocated for %s, nothing to remove",
				   tx_bss->conf->iface);
			return;
		}

		dl_list_for_each_safe(bss, tmp, &grp->bss_list,
				      struct hostapd_data, mbssid_bss) {
			if (bss == tx_bss)
				continue;

			iface->driver_ap_teardown = !(iface->drv_flags &
						      WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);
			hostapd_remove_bss(iface,
					   hostapd_get_bss_index(bss));
		}
	} else {
		for (k = iface->num_bss - 1; k > 0; k--) {
			bss = iface->bss[k];

			if (bss == tx_bss)
				continue;

			iface->driver_ap_teardown = !(iface->drv_flags &
						      WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);
			hostapd_remove_bss(iface, k);
		}
	}
}


int hostapd_remove_iface(struct hapd_interfaces *interfaces, char *buf)
{
	struct hostapd_iface *hapd_iface, *refresh_ref = NULL;
	struct hostapd_data *bss = NULL;
	unsigned int i, j;
	int ret = -1;
	bool iface_remove = false, found_bss = false;
#ifdef CONFIG_IEEE80211BE
	/* Parse optional link ID from input string
	 * (format: "<iface_name> <link_id>")
	 * link_id = -1 (No link_id provided)
	 */
	int link_id = hostapd_parse_link_id(buf);

	if ((link_id != -1) && (link_id < 0 ||
				link_id >= MAX_NUM_MLD_LINKS)) {
		wpa_printf(MSG_ERROR, "Invalid link id %d",
			   link_id);
		return -EINVAL;
	}
#endif /* CONFIG_IEEE80211BE */

	for (i = 0; i < interfaces->count; i++) {
		hapd_iface = interfaces->iface[i];
		if (hapd_iface == NULL)
			return -1;

		if (!os_strcmp(hapd_iface->phy, buf)) {
			wpa_printf(MSG_INFO, "Remove interface '%s'", buf);

			ret = hostapd_remove_hapd_iface(hapd_iface);
			iface_remove = true;

			if (interfaces->count > 0)
				refresh_ref = interfaces->iface[0];

			goto refresh_beacon;
		}

		for (j = 0; j < hapd_iface->conf->num_bss; j++) {
			if (!os_strcmp(hapd_iface->conf->bss[j]->iface, buf)) {
				found_bss = true;
				break;
			}
		}
		/* If bss is not found in this iface, the BSS
		 * may belong to a different interface.
		 * Check the remaining interfaces.
		 */
		if (!found_bss)
			continue;

		bss = (j < hapd_iface->num_bss) ? hapd_iface->bss[j] : NULL;

		if (!bss || !bss->conf) {
#ifdef CONFIG_IEEE80211BE
			/* If bss or bss->conf is NULL and
			 * a link_id is provided, the BSS
			 * may belong to a different interface.
			 * Check the remaining interfaces.
			 */
			if (link_id >= 0) {
				bss = NULL;
				continue;
			}
#endif /* CONFIG_IEEE80211BE */
			wpa_printf(MSG_INFO,
				   "REMOVE: '%s' not started yet. Removing config.",
				   buf);
			return hostapd_remove_bss(hapd_iface, j);
		}
#ifdef CONFIG_IEEE80211BE
		if (bss->conf->mld_ap && link_id >= 0) {
			/* If a link ID is provided, fetch the corresponding
			 * link BSS and proceed with removal; otherwise,
			 * remove the default link (the first link of the
			 * MLD AP)
			 */
			bss = hostapd_mld_get_link_bss(bss,
						       (u8) link_id);
			if (!bss) {
				wpa_printf(MSG_ERROR,
					   "MLD: Invalid link ID = %d",
					   link_id);
				return -EINVAL;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		if (bss)
			break;
	}

	if (!bss) {
		wpa_printf(MSG_ERROR,
			   "Invalid interface name/radio identifier/link id '%s'",
			   buf);
		return -EINVAL;
	}

	/* Remove non-TX BSS if the BSS being removed is
	 * the TX BSS in a multi-BSS group
	 */
	if (bss->iconf->mbssid) {
		if (bss == hostapd_mbssid_get_tx_bss(bss))
			hostapd_remove_non_tx_bsses(bss);
		else {
			/* When non-tx bss is removed update
			 * beacons of all interfaces
			 */
			iface_remove = true;
		}
	}

	if (bss->iface->num_bss == 1) {
		wpa_printf(MSG_INFO, "Last BSS - Remove interface '%s'", buf);

		ret = hostapd_remove_hapd_iface(bss->iface);
		iface_remove = true;

		if (interfaces->count > 0)
			refresh_ref = interfaces->iface[0];

		goto refresh_beacon;
	}

	bss->iface->driver_ap_teardown = !(bss->iface->drv_flags &
			WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);
	refresh_ref = bss->iface;
	ret = hostapd_remove_bss(bss->iface, hostapd_get_bss_index(bss));

refresh_beacon:
	if (ret == 0 && refresh_ref) {
		if (iface_remove)
			hostapd_refresh_all_iface_beacons(refresh_ref);
		else
			hostapd_refresh_other_iface_beacons(refresh_ref);
	}

	return ret;
}


/**
 * hostapd_new_assoc_sta - Notify that a new station associated with the AP
 * @hapd: Pointer to BSS data
 * @sta: Pointer to the associated STA data
 * @reassoc: 1 to indicate this was a re-association; 0 = first association
 *
 * This function will be called whenever a station associates with the AP. It
 * can be called from ieee802_11.c for drivers that export MLME to hostapd and
 * from drv_callbacks.c based on driver events for drivers that take care of
 * management frames (IEEE 802.11 authentication and association) internally.
 */
void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc)
{
	if (hapd->tkip_countermeasures) {
		hostapd_drv_sta_deauth(hapd, sta->addr,
				       WLAN_REASON_MICHAEL_MIC_FAILURE);
		return;
	}

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    sta->mld_assoc_link_id != hapd->mld_link_id)
		return;
#endif /* CONFIG_IEEE80211BE */

	ap_sta_clear_disconnect_timeouts(hapd, sta);
	ap_sta_clear_assoc_timeout(hapd, sta);

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		struct hostapd_data *bss;
		struct sta_info *lsta;

		for_each_mld_link(bss, hapd) {
			if (bss == hapd)
				continue;
			lsta = ap_get_sta(bss, sta->addr);
			if (lsta)
				ap_sta_clear_assoc_timeout(bss, lsta);
		}
	}
#endif /* CONFIG_IEEE80211BE */

	sta->post_csa_sa_query = 0;

#ifdef CONFIG_P2P
	if (sta->p2p_ie == NULL && !sta->no_p2p_set) {
		sta->no_p2p_set = 1;
		hapd->num_sta_no_p2p++;
		if (hapd->num_sta_no_p2p == 1)
			hostapd_p2p_non_p2p_sta_connected(hapd);
	}
#endif /* CONFIG_P2P */

	airtime_policy_new_sta(hapd, sta);

	/* Start accounting here, if IEEE 802.1X and WPA are not used.
	 * IEEE 802.1X/WPA code will start accounting after the station has
	 * been authorized. */
	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa) {
		if (ap_sta_set_authorized(hapd, sta, 1)) {
			/* Update driver authorized flag for the STA to cover
			 * the case where AP SME is in the driver and there is
			 * no separate event for handling TX status event for
			 * the (Re)Association Response frame. */
			hostapd_set_sta_flags(hapd, sta);
		}
		os_get_reltime(&sta->connected_time);
		accounting_sta_start(hapd, sta);
	}

	/* Start IEEE 802.1X authentication process for new stations */
	ieee802_1x_new_station(hapd, sta);

#ifdef CONFIG_ENC_ASSOC
		if (ap_sta_is_epp(sta) && sta->wpa_sm) {
			const struct wpa_ptk *ptk;
			const u8 *pmk;
			size_t pmk_len;

			switch (sta->auth_alg) {
			case WLAN_AUTH_EPPKE:
				if (!sta->pasn) {
					wpa_printf(MSG_INFO,
						   "EPP: Missing PASN data");
					return;
				}
				ptk = &sta->pasn->ptk;
				pmk = sta->pasn->pmk;
				pmk_len = sta->pasn->pmk_len;
				break;
#ifdef CONFIG_IEEE8021X_AUTH
			case WLAN_AUTH_802_1X:
				ptk = &sta->eap_auth_data.ptk;
				pmk = sta->eap_auth_data.pmk;
				pmk_len = sta->eap_auth_data.pmk_len;
				break;
#endif /* CONFIG_IEEE8021X_AUTH */
			default:
				wpa_printf(MSG_INFO,
					   "EPP: Unsupported auth alg=%d for an EPP station",
					   sta->auth_alg);
				return;
			}
			wpa_store_eppke_pmk_ptk_sm(sta->wpa_sm, ptk, pmk,
						   pmk_len);
			wpa_auth_set_ptk_rekey_timer(sta->wpa_sm);
		}
#endif /* CONFIG_ENC_ASSOC */

	if (reassoc) {
		if (sta->auth_alg != WLAN_AUTH_FT &&
		    sta->auth_alg != WLAN_AUTH_FILS_SK &&
		    sta->auth_alg != WLAN_AUTH_FILS_SK_PFS &&
		    sta->auth_alg != WLAN_AUTH_FILS_PK &&
		    !(sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS))) {
			wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH);
		    }
	} else if (!(hapd->iface->drv_flags2 &
		     WPA_DRIVER_FLAGS2_4WAY_HANDSHAKE_AP_PSK)) {
		/* The 4-way handshake offloaded case will have this handled
		 * based on the port authorized event. */

		/*
		 * Do not initalize the state machine if
		 * if EAPOL-KEY (4-way) offload is enabled
		 */
		wpa_auth_sta_associated(hapd->wpa_auth, sta->wpa_sm,
					!hapd->conf->plugin_eapol_key_offload);
	}

	if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_WIRED) {
		if (eloop_cancel_timeout(ap_handle_timer, hapd, sta) > 0) {
			wpa_printf(MSG_DEBUG,
				   "%s: %s: canceled wired ap_handle_timer timeout for "
				   MACSTR,
				   hapd->conf->iface, __func__,
				   MAC2STR(sta->addr));
		}
	} else if (!(hapd->iface->drv_flags &
		     WPA_DRIVER_FLAGS_INACTIVITY_TIMER)) {
		wpa_printf(MSG_DEBUG,
			   "%s: %s: reschedule ap_handle_timer timeout for "
			   MACSTR " (%d seconds - ap_max_inactivity)",
			   hapd->conf->iface, __func__, MAC2STR(sta->addr),
			   hapd->conf->ap_max_inactivity);
		eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		eloop_register_timeout(hapd->conf->ap_max_inactivity, 0,
				       ap_handle_timer, hapd, sta);
	}

#ifdef CONFIG_MACSEC
	if (hapd->conf->wpa_key_mgmt == WPA_KEY_MGMT_NONE &&
	    hapd->conf->mka_psk_set)
		ieee802_1x_create_preshared_mka_hapd(hapd, sta);
	else
		ieee802_1x_alloc_kay_sm_hapd(hapd, sta);
#endif /* CONFIG_MACSEC */
}


const char * hostapd_state_text(enum hostapd_iface_state s)
{
	switch (s) {
	case HAPD_IFACE_UNINITIALIZED:
		return "UNINITIALIZED";
	case HAPD_IFACE_DISABLED:
		return "DISABLED";
	case HAPD_IFACE_COUNTRY_UPDATE:
		return "COUNTRY_UPDATE";
	case HAPD_IFACE_ACS:
		return "ACS";
	case HAPD_IFACE_HT_SCAN:
		return "HT_SCAN";
	case HAPD_IFACE_DFS:
		return "DFS";
	case HAPD_IFACE_ENABLED:
		return "ENABLED";
	case HAPD_IFACE_NO_IR:
		return "NO_IR";
	}

	return "UNKNOWN";
}

static u32 hostapd_get_bss_iface_mode(const struct hostapd_data *hapd)
{
	if (!hapd || !hapd->conf)
		return QCA_WLAN_VENDOR_IFACE_MODE_OPEN;

	return (hapd->conf->wpa || hapd->conf->ieee802_1x) ?
	       QCA_WLAN_VENDOR_IFACE_MODE_SECURED :
	       QCA_WLAN_VENDOR_IFACE_MODE_OPEN;
}

void hostapd_set_state(struct hostapd_iface *iface, enum hostapd_iface_state s)
{
	enum hostapd_iface_state old_state = iface->state;
	bool notify_enabled, notify_disabled;
	unsigned int i;

	wpa_printf(MSG_INFO, "%s: interface state %s->%s",
		   iface->conf ? iface->conf->bss[0]->iface : "N/A",
		   hostapd_state_text(old_state), hostapd_state_text(s));
	iface->state = s;

	notify_enabled = old_state != HAPD_IFACE_ENABLED &&
			 s == HAPD_IFACE_ENABLED;
	notify_disabled = old_state != HAPD_IFACE_DISABLED &&
			 s == HAPD_IFACE_DISABLED;
	if (!notify_enabled && !notify_disabled)
		return;

	if (!iface->bss)
		return;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];
		u32 bss_mode;

		if (!hapd || !hapd->conf || !hapd->drv_priv)
			continue;

		if (notify_disabled)
			bss_mode = QCA_WLAN_VENDOR_IFACE_MODE_CLEAR;
		else
			bss_mode = hostapd_get_bss_iface_mode(hapd);

		if (hostapd_drv_notify_iface_state(hapd, bss_mode))
			wpa_printf(MSG_ERROR,
				   "%s: Failed to notify iface enabled mode for %s: %s->%s",
				   __func__, hapd->conf->iface,
				   hostapd_state_text(old_state), hostapd_state_text(s));
	}
}


int hostapd_csa_in_progress(struct hostapd_iface *iface)
{
	unsigned int i;

	for (i = 0; i < iface->num_bss; i++)
		if (iface->bss[i]->csa_in_progress)
			return 1;
	return 0;
}


#ifdef NEED_AP_MLME

void free_beacon_data(struct beacon_data *beacon)
{
	os_free(beacon->head);
	beacon->head = NULL;
	os_free(beacon->tail);
	beacon->tail = NULL;
	os_free(beacon->probe_resp);
	beacon->probe_resp = NULL;
	os_free(beacon->beacon_ies);
	beacon->beacon_ies = NULL;
	os_free(beacon->proberesp_ies);
	beacon->proberesp_ies = NULL;
	os_free(beacon->assocresp_ies);
	beacon->assocresp_ies = NULL;
	os_free(beacon->mbssid.mbssid_elem);
	beacon->mbssid.mbssid_elem = NULL;
	os_free(beacon->mbssid.mbssid_elem_offset);
	beacon->mbssid.mbssid_elem_offset = NULL;
	os_free(beacon->mbssid.rnr_elem);
	beacon->mbssid.rnr_elem = NULL;
	os_free(beacon->mbssid.rnr_elem_offset);
	beacon->mbssid.rnr_elem_offset = NULL;
}


int hostapd_build_beacon_data(struct hostapd_data *hapd,
			      struct beacon_data *beacon)
{
	struct wpabuf *beacon_extra, *proberesp_extra, *assocresp_extra;
	struct wpa_driver_ap_params params;
	struct hostapd_data *tx_bss;
	u8 *mbssid_start_eid, *rnr_start_eid;
	size_t size = 0;
	int i;
	int ret;

	os_memset(beacon, 0, sizeof(*beacon));
	ret = ieee802_11_build_ap_params(hapd, &params);
	if (ret < 0)
		return ret;

	ret = hostapd_build_ap_extra_ies(hapd, &beacon_extra,
					 &proberesp_extra,
					 &assocresp_extra);
	if (ret)
		goto free_ap_params;

	ret = -1;
	beacon->head = os_memdup(params.head, params.head_len);
	if (!beacon->head)
		goto free_ap_extra_ies;

	beacon->head_len = params.head_len;

	beacon->tail = os_memdup(params.tail, params.tail_len);
	if (!beacon->tail)
		goto free_beacon;

	beacon->tail_len = params.tail_len;

	if (params.proberesp != NULL) {
		beacon->probe_resp = os_memdup(params.proberesp,
					       params.proberesp_len);
		if (!beacon->probe_resp)
			goto free_beacon;

		beacon->probe_resp_len = params.proberesp_len;
	}

	/* copy the extra ies */
	if (beacon_extra) {
		beacon->beacon_ies = os_memdup(beacon_extra->buf,
					       wpabuf_len(beacon_extra));
		if (!beacon->beacon_ies)
			goto free_beacon;

		beacon->beacon_ies_len = wpabuf_len(beacon_extra);
	}

	if (proberesp_extra) {
		beacon->proberesp_ies = os_memdup(proberesp_extra->buf,
						  wpabuf_len(proberesp_extra));
		if (!beacon->proberesp_ies)
			goto free_beacon;

		beacon->proberesp_ies_len = wpabuf_len(proberesp_extra);
	}

	if (assocresp_extra) {
		beacon->assocresp_ies = os_memdup(assocresp_extra->buf,
						  wpabuf_len(assocresp_extra));
		if (!beacon->assocresp_ies)
			goto free_beacon;

		beacon->assocresp_ies_len = wpabuf_len(assocresp_extra);
	}

	beacon->elemid_added_bmap = params.elemid_added_bmap;
	beacon->elemid_modified_bmap = params.elemid_modified_bmap;

	/* MBSSID element */
	if (!params.mbssid.mbssid_elem_len)
		goto done;

	tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	beacon->mbssid.mbssid_tx_iface = tx_bss->conf->iface;
	beacon->mbssid.mbssid_tx_iface_linkid =
		params.mbssid.mbssid_tx_iface_linkid;
	beacon->mbssid.mbssid_index = params.mbssid.mbssid_index;

	beacon->mbssid.mbssid_elem_len = params.mbssid.mbssid_elem_len;
	beacon->mbssid.mbssid_elem_count = params.mbssid.mbssid_elem_count;
	if (params.mbssid.mbssid_elem) {
		beacon->mbssid.mbssid_elem =
			os_memdup(params.mbssid.mbssid_elem,
				  params.mbssid.mbssid_elem_len);
		if (!beacon->mbssid.mbssid_elem)
			goto free_beacon;
	}
	beacon->mbssid.ema = params.mbssid.ema;

	if (params.mbssid.mbssid_elem_offset) {
		beacon->mbssid.mbssid_elem_offset =
			os_calloc(beacon->mbssid.mbssid_elem_count,
				  sizeof(u8 *));
		if (!beacon->mbssid.mbssid_elem_offset)
			goto free_beacon;

		mbssid_start_eid = beacon->mbssid.mbssid_elem;
		beacon->mbssid.mbssid_elem_offset[0] = mbssid_start_eid;
		for (i = 0; i < beacon->mbssid.mbssid_elem_count - 1; i++) {
			size = params.mbssid.mbssid_elem_offset[i + 1] -
				params.mbssid.mbssid_elem_offset[i];
			mbssid_start_eid = mbssid_start_eid + size;
			beacon->mbssid.mbssid_elem_offset[i + 1] =
				mbssid_start_eid;
		}
	}

	/* RNR element */
	if (!params.mbssid.rnr_elem_len)
		goto done;

	if (params.mbssid.rnr_elem) {
		beacon->mbssid.rnr_elem = os_memdup(params.mbssid.rnr_elem,
						    params.mbssid.rnr_elem_len);
		if (!beacon->mbssid.rnr_elem)
			goto free_beacon;
	}

	beacon->mbssid.rnr_elem_len = params.mbssid.rnr_elem_len;
	beacon->mbssid.rnr_elem_count = params.mbssid.rnr_elem_count;
	if (params.mbssid.rnr_elem_offset) {
		beacon->mbssid.rnr_elem_offset =
			os_calloc(beacon->mbssid.rnr_elem_count + 1,
				  sizeof(u8 *));
		if (!beacon->mbssid.rnr_elem_offset)
			goto free_beacon;

		rnr_start_eid = beacon->mbssid.rnr_elem;
		beacon->mbssid.rnr_elem_offset[0] = rnr_start_eid;
		for (i = 0; i < beacon->mbssid.rnr_elem_count - 1; i++) {
			size = params.mbssid.rnr_elem_offset[i + 1] -
				params.mbssid.rnr_elem_offset[i];
			rnr_start_eid = rnr_start_eid + size;
			beacon->mbssid.rnr_elem_offset[i + 1] = rnr_start_eid;
		}
	}

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		size_t bcn_len;

		bcn_len = params.head_len + params.tail_len + wpabuf_len(beacon_extra) +
			  params.mbssid.mbssid_elem_len;

		if (bcn_len > hapd->iface->multi_mbssid.max_beacon_size) {
			if (params.mbssid.mbssid_elem_count > 1) {
				wpa_printf(MSG_ERROR,
					   "Reduce MBSSID group size (%d) to accommodate within beacon size limit of %u bytes. Current beacon length is %zu",
					   hapd->iconf->group_size,
					   hapd->iface->multi_mbssid.max_beacon_size,
					   bcn_len);
			}
			goto free_beacon;
		}

		if (hapd->iface->multi_mbssid.num_mbssid_groups >
		    hapd->iface->multi_mbssid.mbssid_max_ngroups) {
			wpa_printf(MSG_ERROR,
				   "Created Multi MBSSID groups(%zu) exceeded max allowed groups(%d)\n",
				   hapd->iface->multi_mbssid.num_mbssid_groups,
				   hapd->iface->multi_mbssid.mbssid_max_ngroups);
			goto free_beacon;
		}
	}

done:
	ret = 0;
free_beacon:
	/* if the function fails, the caller should not free beacon data */
	if (ret)
		free_beacon_data(beacon);

free_ap_extra_ies:
	hostapd_free_ap_extra_ies(hapd, beacon_extra, proberesp_extra,
				  assocresp_extra);
free_ap_params:
	ieee802_11_free_ap_params(&params);
	return ret;
}


/*
 * TODO: This flow currently supports only changing channel and width within
 * the same hw_mode. Any other changes to MAC parameters or provided settings
 * are not supported.
 */
int hostapd_change_config_freq(struct hostapd_data *hapd,
			       struct hostapd_config *conf,
			       struct hostapd_freq_params *params,
			       struct hostapd_freq_params *old_params)
{
	u8 channel, op_class, seg0 = 0, seg1 = 0;
	struct hostapd_hw_modes *mode;

	if (!params->channel) {
		/* check if the new channel is supported by hw */
		params->channel = hostapd_hw_get_channel(hapd, params->freq);
	}

	channel = params->channel;
	if (!channel)
		return -1;

	hostapd_determine_mode(hapd->iface);
	mode = hapd->iface->current_mode;

	/* if a pointer to old_params is provided we save previous state */
	if (old_params &&
	    hostapd_set_freq_params(old_params, conf->hw_mode,
				    hostapd_hw_get_freq(hapd, conf->channel),
				    conf->channel, conf->enable_edmg,
				    conf->edmg_channel, conf->ieee80211n,
				    conf->ieee80211ac, conf->ieee80211ax,
				    conf->ieee80211be, conf->ieee80211bn,
				    conf->secondary_channel,
				    hostapd_get_oper_chwidth(conf),
				    hostapd_get_oper_centr_freq_seg0_idx(conf),
				    hostapd_get_oper_centr_freq_seg1_idx(conf),
				    conf->vht_capab,
				    mode ? &mode->he_capab[IEEE80211_MODE_AP] :
				    NULL,
				    mode ? &mode->eht_capab[IEEE80211_MODE_AP] :
				    NULL,
				    mode ? &mode->uhr_capab[IEEE80211_MODE_AP] :
				    NULL,
				    hostapd_get_punct_bitmap(hapd),
				    hapd->iconf->he_6ghz_reg_pwr_type,
#ifdef CONFIG_IEEE80211BN
				    hostapd_hw_get_freq(hapd,
					conf->npca_primary_channel),
				    conf->npca_punct_bitmap,
#else
				    0, 0,
#endif /* CONFIG_IEEE80211BN */
				    conf->bandwidth_device,
				    conf->center_freq_device))
		return -1;

	switch (params->bandwidth) {
	case 0:
	case 20:
		conf->ht_capab &= ~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		break;
	case 40:
	case 80:
	case 160:
	case 320:
		conf->ht_capab |= HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		break;
	default:
		return -1;
	}

	switch (params->bandwidth) {
	case 0:
	case 20:
	case 40:
		hostapd_set_oper_chwidth(conf, CONF_OPER_CHWIDTH_USE_HT);
		break;
	case 80:
		if (params->center_freq2)
			hostapd_set_oper_chwidth(conf,
						 CONF_OPER_CHWIDTH_80P80MHZ);
		else
			hostapd_set_oper_chwidth(conf,
						 CONF_OPER_CHWIDTH_80MHZ);
		break;
	case 160:
		hostapd_set_oper_chwidth(conf, CONF_OPER_CHWIDTH_160MHZ);
		break;
	case 320:
		hostapd_set_oper_chwidth(conf, CONF_OPER_CHWIDTH_320MHZ);
		break;
	default:
		return -1;
	}

	conf->channel = channel;
	conf->ieee80211n = params->ht_enabled;
	conf->ieee80211ac = params->vht_enabled;
	conf->secondary_channel = params->sec_channel_offset;
	if (params->center_freq1 &&
	    ieee80211_freq_to_chan(params->center_freq1, &seg0) ==
	    NUM_HOSTAPD_MODES)
		return -1;
	if (params->center_freq2 &&
	    ieee80211_freq_to_chan(params->center_freq2,
				   &seg1) == NUM_HOSTAPD_MODES)
		return -1;
	if (ieee80211_freq_to_channel_ext(params->freq,
					  conf->secondary_channel,
					  hostapd_get_oper_chwidth(hapd->iconf),
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "%s:Update op_class %d->%d and channel=%d based on freq=%d",
		   __func__, conf->op_class, op_class, channel, params->freq);
	conf->op_class = op_class;
	hostapd_set_oper_centr_freq_seg0_idx(conf, seg0);
	hostapd_set_oper_centr_freq_seg1_idx(conf, seg1);

#ifdef CONFIG_IEEE80211BE
	conf->punct_bitmap = params->punct_bitmap;
#endif /* CONFIG_IEEE80211BE */

	/* TODO: maybe call here hostapd_config_check here? */

	return 0;
}

#ifdef CONFIG_IEEE80211BN
bool hostapd_is_mapc_action(const struct ieee80211_mgmt *mgmt, size_t len)
{
	u8 category, action;

	if (len < IEEE80211_HDRLEN + 2)
		return false;

	category = mgmt->u.action.category;

	if (category != WLAN_ACTION_PUBLIC &&
		category != WLAN_ACTION_PROTECTED_DUAL)
		return false;

	action = mgmt->u.action.u.public_action.action;
	switch (action) {
	case WLAN_PA_MAPC_DISCOVERY_REQ:
	case WLAN_PA_MAPC_DISCOVERY_RESP:
	case WLAN_PA_MAPC_NEGOTIATION_REQ:
	case WLAN_PA_MAPC_NEGOTIATION_RESP:
		return true;
	default:
		return false;
    	}
}
#endif /* CONFIG_IEEE80211BN */


enum oper_chan_width
hostapd_chan_width_from_freq_params(struct hostapd_freq_params *freq_params)
{
	switch (freq_params->bandwidth) {
	case 80:
		if (freq_params->center_freq2)
			return CONF_OPER_CHWIDTH_80P80MHZ;
		else
			return CONF_OPER_CHWIDTH_80MHZ;
	case 160:
		return CONF_OPER_CHWIDTH_160MHZ;
	case 320:
		return CONF_OPER_CHWIDTH_320MHZ;
	default:
		return CONF_OPER_CHWIDTH_USE_HT;
	}
}


static int hostapd_fill_csa_settings(struct hostapd_data *hapd,
				     struct csa_settings *settings)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_freq_params old_freq;
	int ret;
	enum oper_chan_width chanwidth;
	u8 chan, old_reg_6ghz_power_mode;
	int sec_channel_offset = settings->freq_params.sec_channel_offset;
	u8 tpe_config = 0;
#ifdef CONFIG_IEEE80211BN
	struct hostapd_npca_state old_npca;
#endif /* CONFIG_IEEE80211BN */

	os_memset(&old_freq, 0, sizeof(old_freq));
	if (!iface || !iface->freq || hapd->csa_in_progress)
		return -1;

	chanwidth = hostapd_chan_width_from_freq_params(&settings->freq_params);
#ifdef CONFIG_IEEE80211BE
	/* When Switching to an 11BE channel, adjust the legacy BW and center
	 * frequencies accordingly
	 */
	if (chanwidth == CONF_OPER_CHWIDTH_320MHZ ||
#ifdef CONFIG_QCN_EXTN
	    hostapd_is_repurpose_disabled_11be_extn(hapd->conf) ||
#endif /* CONFIG_QCN_EXTN */
	    settings->freq_params.punct_bitmap) {
		enum oper_chan_width chan_op_bw = chanwidth;
		u8 oper_centr_freq0_idx = 0, oper_centr_freq1_idx = 0, pri_chan = 0;

		ieee80211_freq_to_chan(settings->freq_params.center_freq1,
				       &oper_centr_freq0_idx);
		ieee80211_freq_to_chan(settings->freq_params.center_freq2,
				       &oper_centr_freq1_idx);
		ieee80211_freq_to_chan(settings->freq_params.freq,
				       &pri_chan);

		punct_update_legacy_bw(settings->freq_params.punct_bitmap,
				       pri_chan,
				       &chan_op_bw,
				       &oper_centr_freq0_idx,
				       &oper_centr_freq1_idx);

		/* Downgrade the EHT 320 MHz BW to legacy 160 MHz BW and
		 * calculate the correspoding 160 MHz center freq for later
		 * ECSA opclass calculation which needs legacy BW and
		 * secondary channel offset.
		 */
		if (chan_op_bw == CONF_OPER_CHWIDTH_320MHZ) {
			chan_op_bw = CONF_OPER_CHWIDTH_160MHZ;

			if (pri_chan < oper_centr_freq0_idx)
				oper_centr_freq0_idx -= 16;
			else
				oper_centr_freq0_idx += 16;
		}

		chanwidth = chan_op_bw;
#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
			hostapd_get_csa_info_of_repurposed_bss_extn(
					hapd, pri_chan,
					settings->freq_params.sec_channel_offset,
					&chanwidth,
					&oper_centr_freq0_idx,
					&oper_centr_freq1_idx);
		}
#endif /* CONFIG_QCN_EXTN */

		if (oper_centr_freq0_idx == 0 || oper_centr_freq0_idx == pri_chan)
			sec_channel_offset = 0;
		else if (oper_centr_freq0_idx > pri_chan)
			sec_channel_offset = 1;
		else
			sec_channel_offset = -1;

		wpa_printf(MSG_DEBUG,
			   "Updated Legacy BW %d chan1 %d chan2 %d sec_chan %d",
			   chanwidth, oper_centr_freq0_idx, oper_centr_freq1_idx,
			   sec_channel_offset);
	}
#endif /* CONFIG_IEEE80211BE */

	if (ieee80211_freq_to_channel_ext(
		    settings->freq_params.freq,
		    sec_channel_offset,
		    chanwidth,
		    &hapd->iface->cs_oper_class,
		    &chan) == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG,
			   "invalid frequency for channel switch (freq=%d, sec_channel_offset=%d, vht_enabled=%d, he_enabled=%d, eht_enabled=%d, uhr_enabled=%d)",
			   settings->freq_params.freq,
			   sec_channel_offset,
			   settings->freq_params.vht_enabled,
			   settings->freq_params.he_enabled,
			   settings->freq_params.eht_enabled,
			   settings->freq_params.uhr_enabled);
		return -1;
	}

	settings->freq_params.channel = chan;

#ifdef CONFIG_QCN_EXTN
	ret = hostapd_validate_hw_blocklist_for_freq_params_extn(
		iface, &settings->freq_params, settings->power_mode, "channel switch");
	if (ret)
		return ret;
#endif /* CONFIG_QCN_EXTN */

	ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
					 &settings->freq_params,
					 &old_freq);
	if (ret)
		return ret;

	old_reg_6ghz_power_mode = iface->conf->he_6ghz_reg_pwr_type;
	if (settings->power_mode >= 0)
		iface->conf->he_6ghz_reg_pwr_type = settings->power_mode;

	if (hapd->conf->tpe_ie_config.local_tpe_config) {
		tpe_config =  hapd->conf->tpe_ie_config.local_tpe_config;
		hapd->conf->tpe_ie_config.local_tpe_config = 0;
	}

#ifdef CONFIG_IEEE80211BN
	hostapd_save_npca(iface->conf, &old_npca);
	hostapd_disable_npca(iface->conf);
#endif /* CONFIG_IEEE80211BN */

	ret = hostapd_build_beacon_data(hapd, &settings->beacon_after);
	if (settings->beacon_after.elemid_modified_bmap)
		settings->beacon_after_cu = 1;

	settings->bss_idx = hostapd_mbssid_get_bss_index(hapd);

	/* change back the configuration */
	hostapd_change_config_freq(iface->bss[0], iface->conf,
				   &old_freq, NULL);

#ifdef CONFIG_IEEE80211BN
	hostapd_restore_npca(iface->conf, &old_npca);
#endif /* CONFIG_IEEE80211BN */

	if (tpe_config)
		hapd->conf->tpe_ie_config.local_tpe_config = tpe_config;

	if (ret)
		return ret;

	/* set channel switch parameters for csa ie */
	hapd->cs_freq_params = settings->freq_params;
	hapd->cs_count = settings->cs_count;
	hapd->cs_block_tx = settings->block_tx;

	 /* reset MU-EDCA and WME EDCA parameter set count */
	 hapd->iface->conf->he_mu_edca.he_qos_info &= 0xfff0;
	 hapd->parameter_set_count = 0;

	iface->conf->he_6ghz_reg_pwr_type = old_reg_6ghz_power_mode;
	ret = hostapd_build_beacon_data(hapd, &settings->beacon_csa);
	if (ret) {
		free_beacon_data(&settings->beacon_after);
		return ret;
	}

	settings->counter_offset_beacon[0] = hapd->cs_c_off_beacon;
	settings->counter_offset_presp[0] = hapd->cs_c_off_proberesp;
	settings->counter_offset_beacon[1] = hapd->cs_c_off_ecsa_beacon;
	settings->counter_offset_presp[1] = hapd->cs_c_off_ecsa_proberesp;
	settings->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		settings->link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	settings->ubpr.unsol_bcast_probe_resp_tmpl =
		hostapd_unsol_bcast_probe_resp(hapd, &settings->ubpr);
#endif /* CONFIG_IEEE80211AX */

	return 0;
}


void hostapd_cleanup_cs_params(struct hostapd_data *hapd)
{
	os_memset(&hapd->cs_freq_params, 0, sizeof(hapd->cs_freq_params));
	hapd->cs_count = 0;
	hapd->cs_block_tx = 0;
	hapd->cs_c_off_beacon = 0;
	hapd->cs_c_off_proberesp = 0;
	hapd->csa_in_progress = 0;
	hapd->cs_c_off_ecsa_beacon = 0;
	hapd->cs_c_off_ecsa_proberesp = 0;
}


void hostapd_chan_switch_config(struct hostapd_data *hapd,
				struct hostapd_freq_params *freq_params)
{
	if (freq_params->uhr_enabled)
		hapd->iconf->ch_switch_uhr_config |= CH_SWITCH_UHR_ENABLED;
	else
		hapd->iconf->ch_switch_uhr_config |= CH_SWITCH_UHR_DISABLED;

	if (freq_params->eht_enabled)
		hapd->iconf->ch_switch_eht_config |= CH_SWITCH_EHT_ENABLED;
	else
		hapd->iconf->ch_switch_eht_config |= CH_SWITCH_EHT_DISABLED;

	if (freq_params->he_enabled)
		hapd->iconf->ch_switch_he_config |= CH_SWITCH_HE_ENABLED;
	else
		hapd->iconf->ch_switch_he_config |= CH_SWITCH_HE_DISABLED;

	if (freq_params->vht_enabled)
		hapd->iconf->ch_switch_vht_config |= CH_SWITCH_VHT_ENABLED;
	else
		hapd->iconf->ch_switch_vht_config |= CH_SWITCH_VHT_DISABLED;

	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "CHAN_SWITCH UHR config 0x%x EHT config 0x%x HE config 0x%x VHT config 0x%x",
		       hapd->iconf->ch_switch_uhr_config,
		       hapd->iconf->ch_switch_eht_config,
		       hapd->iconf->ch_switch_he_config,
		       hapd->iconf->ch_switch_vht_config);
}


int hostapd_update_monitor_channel(struct hostapd_data *hapd,
				   const struct hostapd_freq_params *freq_params)
{
	struct hostapd_iface *iface = hapd->iface;
	int ret;

	if (!iface->monitor_iface_configured)
		return 0;

	ret = hostapd_drv_update_monitor_channel(hapd, iface->monitor_ifindex,
						 freq_params);
	if (ret)
		wpa_printf(MSG_INFO,
			   "Monitor set_freq: Failed to set freq on %s err=%d",
			   iface->monitor_iface, ret);

	return ret;
}


int hostapd_switch_channel(struct hostapd_data *hapd,
			   struct csa_settings *settings)
{
	int ret;
	struct hostapd_data *link_bss;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA)) {
		wpa_printf(MSG_INFO, "CSA is not supported");
		return -1;
	}

	ret = hostapd_fill_csa_settings(hapd, settings);
	if (ret)
		return ret;

	if (hapd->iface->radar_bit_pattern) {
		hapd->iface->conf->punct_bitmap =  hapd->iface->conf->punct_bitmap |
						   hapd->iface->radar_bit_pattern;
	}

	/* interCAC handling: The interCAC feature may decide not to
	 * switch the channel. In such cases, it returns early without
	 * performing a channel switch.
	 *
	 * Any handling that assumes a channel switch will occur should
	 * be added after this condition.
	 */
	if (hostapd_dfs_intercac_defer_non_radar_switch(hapd->iface,
							settings)) {
		free_beacon_data(&settings->beacon_csa);
		free_beacon_data(&settings->beacon_after);
#ifdef CONFIG_IEEE80211AX
		os_free(settings->ubpr.unsol_bcast_probe_resp_tmpl);
		settings->ubpr.unsol_bcast_probe_resp_tmpl = NULL;
#endif /* CONFIG_IEEE80211AX */
		hostapd_cleanup_cs_params(hapd);
		return 0;
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_uplink_cancel_disconnect_timeout_extn(hapd->iface);
#ifdef CONFIG_IEEE80211BE
	hostapd_ttlm_restore_default_mapping_for_5g_cac(hapd, settings);
#endif /* CONFIG_IEEE80211BE */
	hostapd_ignorecac_switch_channel_extn(hapd, settings);
#endif /* CONFIG_QCN_EXTN */

	ret = hostapd_drv_switch_channel(hapd, settings);
	free_beacon_data(&settings->beacon_csa);
	free_beacon_data(&settings->beacon_after);
#ifdef CONFIG_IEEE80211AX
	os_free(settings->ubpr.unsol_bcast_probe_resp_tmpl);
#endif /* CONFIG_IEEE80211AX */

	if (ret) {
		/* if we failed, clean cs parameters */
		hostapd_cleanup_cs_params(hapd);
		return ret;
	}

	hapd->csa_in_progress = 1;
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap) {
		/* Generate per sta profiles for affiliated APs */
		for_each_mld_link(link_bss, hapd) {
			if (hapd == link_bss)
				continue;
			hostapd_gen_per_sta_profiles(link_bss);
		}
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */

	/* Switch monitor interface to the new channel if configured */
	if (hapd == hapd->iface->bss[0])
		if (hostapd_update_monitor_channel(hapd, &settings->freq_params))
			return -1;

	return 0;
}


int hostapd_force_channel_switch(struct hostapd_iface *iface,
				 struct csa_settings *settings)
{
	int ret = 0;

	if (!settings->freq_params.channel) {
		/* Check if the new channel is supported */
		settings->freq_params.channel = hostapd_hw_get_channel(
			iface->bss[0], settings->freq_params.freq);
		if (!settings->freq_params.channel)
			return -1;
	}

	ret = hostapd_disable_iface(iface);
	if (ret) {
		wpa_printf(MSG_DEBUG, "Failed to disable the interface");
		return ret;
	}

	hostapd_chan_switch_config(iface->bss[0], &settings->freq_params);
	ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
					 &settings->freq_params, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "Failed to set the new channel in config");
		return ret;
	}

	ret = hostapd_enable_iface(iface);
	if (ret)
		wpa_printf(MSG_DEBUG, "Failed to enable the interface");

	return ret;
}

void hostapd_get_channel_switch_time(struct hostapd_iface *iface,
				     struct hostapd_freq_params *freq_params)
{
	struct hostapd_data *hapd = NULL;
	unsigned int i;
	int ret;

	if (iface->bss == NULL || iface->num_bss == 0)
		return;

	iface->cs_time = 0;

	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->driver == NULL ||
		    iface->bss[i]->drv_priv == NULL)
			continue;

		hapd = iface->bss[i];
		break;
	}

	if (hapd == NULL) {
		wpa_printf(MSG_DEBUG,
			   "No valid BSS with driver found for channel switch time query");
		return;
	}

	if (hapd->driver->get_channel_switch_time) {
		ret = hapd->driver->get_channel_switch_time(hapd->drv_priv,
							    freq_params,
							    &iface->cs_time);
		if (ret == 0) {
			wpa_printf(MSG_DEBUG,
				   "channel switch time from driver: %u",
				   iface->cs_time);
		} else {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel switch time from driver: %d",
				   ret);
			/* Reset to safe default */
			iface->cs_time = 0;
		}
	}
}


/**
 * hostapd_abort_cac_for_channel_switch - Abort an ongoing CAC to perform a
 *                                        channel switch
 * @iface: Pointer to the hostapd interface on which CAC is running
 * @settings: Channel switch settings containing the target channel parameters
 *            (frequency, CSA count, block-TX flag, beacon data, etc.) to be
 *            applied once the CAC is aborted
 *
 * Called when a channel switch is requested while a Channel Availability Check
 * (CAC) is already in progress on @iface. It sets the
 * @csa_pending_on_cac_abort flag on the interface and saves the pending
 * channel-switch settings so that hostapd_deferred_csa_dispatch() can schedule
 * the Channel Switch Announcement (CSA) once the driver confirms the CAC abort
 * via the DFS CAC-aborted event or CAC complete via DFS CAC-finished event
 * (CAC completed in the Kernel before processing the CAC abort command). For
 * co-located SLO APs, the CSA beacon template is sent to the firmware together
 * with the MLD AP so that all APs switch channels simultaneously.
 *
 * Return: 0 on success; negative error code on failure.
 */
int hostapd_abort_cac_for_channel_switch(struct hostapd_iface *iface,
					 struct csa_settings *settings)
{
	struct hostapd_data *hapd = NULL;
	int i, ret;

	if (!iface->num_bss || !iface->bss)
		return -1;

	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		goto force_chan_switch;

	for (i = 0; i < iface->num_bss; i++) {
		if (!iface->bss[i]->driver || !iface->bss[i]->drv_priv)
			continue;

		hapd = iface->bss[i];
		break;
	}

	if (hapd) {
		iface->csa_pending_on_cac_abort = true;
		os_memcpy(&iface->csa_settings, settings, sizeof(struct csa_settings));
		wpa_printf(MSG_DEBUG,
			   "CAC is in progress - switching channel using CSA by aborting the CAC");

		ret = hostapd_drv_abort_cac(hapd);
		if (ret == 0)
			return ret;

		wpa_printf(MSG_DEBUG, "Aborting CAC failed");
		hapd->iface->csa_pending_on_cac_abort = false;
		os_memset(&hapd->iface->csa_settings, 0, sizeof(struct csa_settings));
	}

force_chan_switch:
	wpa_printf(MSG_DEBUG, "CAC is in progress - switching channel without CSA");
	return hostapd_force_channel_switch(iface, settings);
}


void
hostapd_switch_channel_fallback(struct hostapd_iface *iface,
				const struct hostapd_freq_params *freq_params)
{
	u8 seg0_idx = 0, seg1_idx = 0;
	enum oper_chan_width bw = CONF_OPER_CHWIDTH_USE_HT;
	u8 op_class, chan = 0;

	wpa_printf(MSG_DEBUG, "Restarting all CSA-related BSSes");

	if (freq_params->center_freq1)
		ieee80211_freq_to_chan(freq_params->center_freq1, &seg0_idx);
	if (freq_params->center_freq2)
		ieee80211_freq_to_chan(freq_params->center_freq2, &seg1_idx);

	switch (freq_params->bandwidth) {
	case 0:
	case 20:
	case 40:
		bw = CONF_OPER_CHWIDTH_USE_HT;
		break;
	case 80:
		if (freq_params->center_freq2) {
			bw = CONF_OPER_CHWIDTH_80P80MHZ;
		} else {
			bw = CONF_OPER_CHWIDTH_80MHZ;
		}
		break;
	case 160:
		bw = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case 320:
		bw = CONF_OPER_CHWIDTH_320MHZ;
		break;
	default:
		wpa_printf(MSG_WARNING, "Unknown CSA bandwidth: %d",
			   freq_params->bandwidth);
		break;
	}

	iface->freq = freq_params->freq;
	iface->conf->channel = freq_params->channel;
	iface->conf->secondary_channel = freq_params->sec_channel_offset;
	if (ieee80211_freq_to_channel_ext(freq_params->freq,
					  freq_params->sec_channel_offset, bw,
					  &op_class, &chan) ==
	    NUM_HOSTAPD_MODES ||
	    chan != freq_params->channel)
		wpa_printf(MSG_INFO, "CSA: Channel mismatch: %d -> %d",
			   freq_params->channel, chan);

	iface->conf->op_class = op_class;
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf, seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf, seg1_idx);
	hostapd_set_oper_chwidth(iface->conf, bw);
	iface->conf->ieee80211n = freq_params->ht_enabled;
	iface->conf->ieee80211ac = freq_params->vht_enabled;
	iface->conf->ieee80211ax = freq_params->he_enabled;
	iface->conf->ieee80211be = freq_params->eht_enabled;
	iface->conf->bandwidth_device = freq_params->bandwidth_device;
	iface->conf->center_freq_device = freq_params->center_freq_device;
	iface->conf->punct_bitmap = freq_params->punct_bitmap;
#ifdef CONFIG_IEEE80211BN
	hostapd_disable_npca(iface->conf);
#endif /* CONFIG_IEEE80211BN */

	/*
	 * cs_params must not be cleared earlier because the freq_params
	 * argument may actually point to one of these.
	 * These params will be cleared during interface disable below.
	 */
	hostapd_disable_iface(iface);
	hostapd_enable_iface(iface);
}


#ifdef CONFIG_IEEE80211AX

void hostapd_cleanup_cca_params(struct hostapd_data *hapd)
{
	hapd->cca_color = 0;
	hapd->cca_c_off_beacon = 0;
	hapd->cca_c_off_proberesp = 0;
	hapd->cca_in_progress = false;
}


int hostapd_fill_cca_settings(struct hostapd_data *hapd,
			      struct cca_settings *settings)
{
	struct hostapd_iface *iface = hapd->iface;
	u8 old_color;
	int ret;

	if (!iface || iface->conf->he_op.he_bss_color_disabled)
		return -1;

	settings->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		settings->link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	old_color = iface->conf->he_op.he_bss_color;
	iface->conf->he_op.he_bss_color = hapd->cca_color;
	ret = hostapd_build_beacon_data(hapd, &settings->beacon_after);
	if (ret)
		return ret;

	iface->conf->he_op.he_bss_color = old_color;

	settings->cca_count = hapd->cca_count;
	settings->cca_color = hapd->cca_color,
	hapd->cca_in_progress = true;

	ret = hostapd_build_beacon_data(hapd, &settings->beacon_cca);
	if (ret) {
		free_beacon_data(&settings->beacon_after);
		return ret;
	}

	settings->ubpr.unsol_bcast_probe_resp_tmpl =
		hostapd_unsol_bcast_probe_resp(hapd, &settings->ubpr);

	settings->counter_offset_beacon = hapd->cca_c_off_beacon;
	settings->counter_offset_presp = hapd->cca_c_off_proberesp;

	hostapd_interface_update_fils_ubpr(iface, true);

	settings->bss_idx = hostapd_mbssid_get_bss_index(hapd);

	return 0;
}


void hostapd_switch_color_timeout_handler(void *eloop_data,
					  void *user_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_data;
	os_time_t delta_t;
	unsigned int b;
	int i, r;
	u64 neighbor_color;
	struct hostapd_data *link_bss;

	delta_t = hapd->last_color_collision.sec -
		hapd->first_color_collision.sec;

	if (delta_t < hapd->iface->conf->he_bss_color_collision_ap_period && !hapd->no_free_color)
		return;

	neighbor_color = ap_list_get_color(hapd->iface);
	if (!hapd->no_free_color)
		 neighbor_color |= hapd->color_collision_bitmap;

	r = os_random() % HE_OPERATION_BSS_COLOR_MAX;
	for (i = 0; i < HE_OPERATION_BSS_COLOR_MAX; i++) {
		if (r && !(neighbor_color & (1ULL << r)))
			break;
		r = (r + 1) % HE_OPERATION_BSS_COLOR_MAX;
	}

	if (i == HE_OPERATION_BSS_COLOR_MAX) {
		/* There are no free colors so turn BSS coloring off */
		wpa_printf(MSG_INFO,
			   "No free colors left, turning off BSS coloring");
		hapd->iface->conf->he_op.he_bss_color_disabled = 1;
		hapd->iface->conf->he_op.he_bss_color = os_random() % 63 + 1;
		hapd->no_free_color = 1;
		for (b = 0; b < hapd->iface->num_bss; b++)
			ieee802_11_set_beacon(hapd->iface->bss[b]);

		 /* Enabling for next check after timeout*/
		 hapd->iface->conf->he_op.he_bss_color_disabled = 0;

		 /* start timer for color collision ap period, and check free color on timeout */
		 if (!eloop_is_timeout_registered(hostapd_switch_color_timeout_handler, hapd, NULL))
			eloop_register_timeout(hapd->iface->conf->he_bss_color_collision_ap_period,
					       0, hostapd_switch_color_timeout_handler, hapd,
					       NULL);

		return;
	}

	if (hapd->no_free_color)
		 hapd->no_free_color = 0;

	for (b = 0; b < hapd->iface->num_bss; b++) {
		struct hostapd_data *bss = hapd->iface->bss[b];
		struct cca_settings settings;
		int ret;

		os_memset(&settings, 0, sizeof(settings));
		hostapd_cleanup_cca_params(bss);
		bss->cca_color = r;

		if (hostapd_fill_cca_settings(bss, &settings)) {
			hostapd_cleanup_cca_params(bss);
			continue;
		}

		ret = hostapd_drv_switch_color(bss, &settings);
		if (ret)
			hostapd_cleanup_cca_params(bss);

		free_beacon_data(&settings.beacon_cca);
		free_beacon_data(&settings.beacon_after);
		os_free(settings.ubpr.unsol_bcast_probe_resp_tmpl);

#ifdef CONFIG_QCN_EXTN
		if (!hostapd_is_repurpose_disabled_11be_extn(bss->conf)) {
#endif /* CONFIG_QCN_EXTN */
		if (!ret && bss->conf->mld_ap) {
			/* Generate per sta profiles for affiliated APs */
			for_each_mld_link(link_bss, bss) {
				if (bss == link_bss)
					continue;
				hostapd_gen_per_sta_profiles(link_bss);
			}
		}
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */
	}
}

bool hostapd_is_cca_in_progress(struct hostapd_iface *iface)
{
	for (int i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->cca_in_progress)
			return true;
	}

	return false;
}

void hostapd_switch_color(struct hostapd_data *hapd, u64 bitmap)
{
	struct os_reltime now;

	if (hostapd_is_cca_in_progress(hapd->iface))
		return;

	if (os_get_reltime(&now))
		return;

	hapd->color_collision_bitmap = bitmap;
	hapd->last_color_collision = now;

	if (eloop_is_timeout_registered(hostapd_switch_color_timeout_handler,
					hapd, NULL))
		return;

	hapd->first_color_collision = now;

	wpa_printf(MSG_INFO,
		   "Scheduling color-switch timeout: %u + 10 s",
		   hapd->iface->conf->he_bss_color_collision_ap_period);

	/* 10 s window as margin for persistent color collision reporting */
	eloop_register_timeout(hapd->iface->conf->he_bss_color_collision_ap_period + 10, 0,
			       hostapd_switch_color_timeout_handler,
			       hapd, NULL);
}

#endif /* CONFIG_IEEE80211AX */

#endif /* NEED_AP_MLME */


struct hostapd_data * hostapd_get_iface(struct hapd_interfaces *interfaces,
					const char *ifname)
{
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *hapd = iface->bss[j];

			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				return hapd;
		}
	}

	return NULL;
}


void hostapd_periodic_iface(struct hostapd_iface *iface)
{
	size_t i;

	ap_list_timer(iface);

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];

		if (!hapd->started)
			continue;

#ifndef CONFIG_NO_RADIUS
		hostapd_acl_expire(hapd);
#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211AC)
		if (is_mu_cap_war_active(hapd))
			hostapd_mu_cap_war_expire_queries(hapd);
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211AC */
#endif /* CONFIG_NO_RADIUS */
	}
}

/* mac authentication timeout handler for wired station */
void hostapd_mac_auth_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	struct radius_sta out;

	if (sta->flags & WIRED_STA_MAB) {
		if (hostapd_allowed_address(hapd, sta->addr, NULL, 0, &out, false)
			 == HOSTAPD_ACL_REJECT && hapd->conf->ieee802_1x) {
				/* try to initiate 802.1x with eap-identity-request,
				 * this case happened when a wired 802.1x station is
				 * connected but do not initiate 802.1x with eapol-start*/
				sta->flags &= (~WIRED_STA_MAB);
				hostapd_new_assoc_sta(hapd, sta, 0);
		}
	}
}

#ifdef CONFIG_OCV
void hostapd_ocv_check_csa_sa_query(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "OCV: Post-CSA SA Query initiation check");

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (!sta->post_csa_sa_query)
			continue;

		wpa_printf(MSG_DEBUG, "OCV: OCVC STA " MACSTR
			   " did not start SA Query after CSA - disconnect",
			   MAC2STR(sta->addr));
		ap_sta_disconnect(hapd, sta, sta->addr,
				  WLAN_REASON_PREV_AUTH_NOT_VALID);
	}
}
#endif /* CONFIG_OCV */


/**
 * hostapd_interface_update_fils_ubpr - Update 6GHz In-band discovery
 * frames (FILS/UBPR) based on lower band interface state change.
 * @iface_enabled: Whether lower band AP is enabled or disabled
 *
 * This function iterates through interfaces list and updates all 6GHz
 * APs In-band discovery frames (enable/disable) based on state of lower
 * band interfaces.
 * Lower band interfaces going down: Enable FILS/UBPR for all 6GHz APs if config
 * has it enabled.
 * Lower band interfaces coming up: Disable FILS/UBPR for all 6GHz APs if not done
 * already.
 */
void
hostapd_interface_update_fils_ubpr(struct hostapd_iface *iface, bool iface_enabled)
{
	int i, j;

	if (!iface || (iface->interfaces == NULL))
		return;

#ifdef CONFIG_MESH
	if (iface->mconf != NULL)
		return;
#endif

	if (is_6ghz_op_class(iface->conf->op_class))
		return;

	for (i = 0; i < iface->interfaces->count; i++) {
		struct hostapd_iface *iface_6g = iface->interfaces->iface[i];
		if (iface == iface_6g || !iface_6g || !iface_6g->conf)
			continue;

		if (!is_6ghz_op_class(iface_6g->conf->op_class))
			continue;

		for (j = 0; j < iface_6g->num_bss; j++) {
			if (!iface_6g->bss[j] || !iface_6g->bss[j]->started)
				continue;

			if (!iface_6g->bss[j]->beacon_set_done)
				continue;

			/* fils/ubpr force disabling is not preferred for this BSS */
			if (!iface_6g->bss[j]->conf->force_disable_in_band_discovery)
				continue;

			/* Lower band interface coming up but fils/ubpr is already disabled */
			if (iface_enabled &&
			    (iface_6g->bss[j]->conf->fils_state != FILS_UBPR_ENABLED &&
			    iface_6g->bss[j]->conf->ubpr_state != FILS_UBPR_ENABLED)) {
				continue;
			}
			/* Lower band interface going down but fils/ubpr is not force disabled */
			if (!iface_enabled &&
			    (iface_6g->bss[j]->conf->fils_state != FILS_UBPR_FORCE_DISABLED &&
			    iface_6g->bss[j]->conf->ubpr_state != FILS_UBPR_FORCE_DISABLED)) {
				continue;
			}
			wpa_printf(MSG_DEBUG, "%s Interface getting %s, check and set 6GHz Interface(%s)"
				   "In-band discovery frames", iface->bss[0]->conf->iface,
				   iface_enabled ? "enabled" : "disabled", iface_6g->bss[j]->conf->iface);
			ieee802_11_set_beacon(iface_6g->bss[j]);
		}
	}
	return;
}


struct hostapd_data *
hostapd_interfaces_get_hapd(struct hapd_interfaces *interfaces,
			    const char *ifname)
{
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *hapd;

			hapd = iface->bss[j];
			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				return hapd;
		}
	}

	return NULL;
}


#ifdef CONFIG_IEEE80211BE
struct hostapd_data * hostapd_mld_get_link_bss(struct hostapd_data *hapd,
					       u8 link_id)
{
	struct hostapd_iface *iface;
	struct hostapd_data *bss;
	unsigned int i, j;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];
		if (!iface)
			continue;

		for (j = 0; j < iface->num_bss; j++) {
			bss = iface->bss[j];

			if (!bss->conf->mld_ap ||
			    !hostapd_is_ml_partner(hapd, bss))
				continue;

			if (!bss->drv_priv)
				continue;

			if (bss->mld_link_id == link_id)
				return bss;
		}
	}

	return NULL;
}


bool hostapd_is_ml_partner(struct hostapd_data *hapd1,
			   struct hostapd_data *hapd2)
{
	if (!hapd1->conf->mld_ap || !hapd2->conf->mld_ap)
		return false;

	return !os_strcmp(hapd1->conf->iface, hapd2->conf->iface);
}


u8 hostapd_get_mld_id(struct hostapd_data *hapd)
{
	if (!hapd->conf->mld_ap)
		return 255;

	return hostapd_mbssid_get_bss_index(hapd);
}


int hostapd_mld_link_config_check(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;
	struct hostapd_data *first = mld->fbss;

#ifdef CONFIG_QCN_EXTN
	/* skip check for repurposed link as it can have unique ssid */
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return 0;

	/* if fbss is repurposed, get first non-repurposed link */
	if (hostapd_is_repurpose_disabled_11be_extn(first->conf)) {
		first = hostapd_get_non_repurposed_link_of_mld_extn(hapd);

		/* if there are no non-repurposed links under ML skip
		 * validation
		 */
		if (!first)
			return 0;
	}
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	if (hapd->iconf->ieee80211bn != first->iconf->ieee80211bn) {
		wpa_printf(MSG_ERROR,
			   "AP MLD %s: Mixed Wi-Fi generation modes not allowed (link %d: ieee80211bn=%d, first link: ieee80211bn=%d)",
			   mld->name, hapd->mld_link_id,
			   hapd->iconf->ieee80211bn,
			   first->iconf->ieee80211bn);
		return -1;
	}
#endif /* CONFIG_IEEE80211BN */

	if (hapd->conf->ssid.ssid_len != first->conf->ssid.ssid_len ||
	    os_memcmp(hapd->conf->ssid.ssid, first->conf->ssid.ssid,
		      first->conf->ssid.ssid_len) != 0) {
		wpa_printf(MSG_ERROR, "AP MLD %s: Link SSID mismatch", mld->name);
		return -1;
	}

	return 0;
}


int hostapd_mld_add_link(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;

	if (!hapd->conf->mld_ap)
		return 0;

	/* Should not happen */
	if (!mld)
		return -1;

	if (mld->fbss && hostapd_mld_link_config_check(hapd) < 0)
		return -1;

	dl_list_add_tail(&mld->links, &hapd->link);
	mld->num_links++;

	wpa_printf(MSG_DEBUG, "AP MLD %s: Link ID %d added. num_links: %d",
		   mld->name, hapd->mld_link_id, mld->num_links);

	if (mld->fbss)
		return 0;

	mld->fbss = hapd;
	wpa_printf(MSG_DEBUG, "AP MLD %s: First link BSS set to %p",
		   mld->name, mld->fbss);
	return 0;
}


int hostapd_mld_remove_link(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;
	struct hostapd_data *next_fbss;

	if (!hapd->conf->mld_ap)
		return 0;

	/* Should not happen */
	if (!mld)
		return -1;

	/*
	 * If the link was never added to the MLD list (e.g., failure during
	 * interface add), we still need to release the allocated link ID.
	 * In that case, do not touch the link list or num_links, but ensure
	 * the ID becomes available again.
	 */
	if (hapd->link.next == NULL || hapd->link.prev == NULL) {
		mld->free_links |= BIT(hapd->mld_link_id);
		wpa_printf(MSG_DEBUG, "AP MLD %s: Link ID %d released (not linked)",
			   mld->name, hapd->mld_link_id);
		return 0;
	}

	dl_list_del(&hapd->link);
	mld->free_links |= BIT(hapd->mld_link_id);
	mld->num_links--;

	wpa_printf(MSG_DEBUG, "AP MLD %s: Link ID %d removed. num_links: %d",
		   mld->name, hapd->mld_link_id, mld->num_links);

	if (mld->fbss != hapd)
		return 0;

	/* If the list is empty, all links are removed */
	if (dl_list_empty(&mld->links)) {
		mld->fbss = NULL;
	} else {
		struct hostapd_data *last_link_bss;

		next_fbss = dl_list_entry(mld->links.next, struct hostapd_data,
					  link);
		last_link_bss = dl_list_last(&mld->links, struct hostapd_data,
					     link);

		if (next_fbss != last_link_bss)
			hostapd_mld_move_vlan_list(hapd, next_fbss);

		mld->fbss = next_fbss;
	}

	wpa_printf(MSG_DEBUG, "AP MLD %s: First link BSS set to %p",
		   mld->name, mld->fbss);
	return 0;
}


bool hostapd_mld_is_first_bss(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;

	if (!hapd->conf->mld_ap)
		return true;

	/* Should not happen */
	if (!mld)
		return false;

	/* If fbss is not set, it is safe to assume the caller is the first BSS.
	 */
	if (!mld->fbss)
		return true;

	return hapd == mld->fbss;
}


struct hostapd_data * hostapd_mld_get_first_bss(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;

	if (!hapd->conf->mld_ap)
		return NULL;

	/* Should not happen */
	if (!mld)
		return NULL;

	return mld->fbss;
}


void hostapd_mld_interface_freed(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss = NULL;

	if (!hapd || !hapd->conf || !hapd->conf->mld_ap)
		return;

#ifdef CONFIG_QCN_EXTN
	for_each_mld_link_include_repurposed(link_bss, hapd)
#else
	for_each_mld_link(link_bss, hapd)
#endif
		link_bss->drv_priv = NULL;
}


/* Return the number of currently active links, not counting the calling link
 * (i.e., a value that is suitable to be used as-is in fields that use encoding
 * of the value minus 1). */
u8 hostapd_get_active_links(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss;
	u8 active_links = 0;

	if (!hapd || !hapd->conf || !hapd->conf->mld_ap || !hapd->mld)
		return 0;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return 0;
#endif /* CONFIG_QCN_EXTN */

	for_each_mld_link(link_bss, hapd) {
		if (link_bss == hapd || !link_bss->started)
			continue;

		active_links++;
	}

	return active_links;
}

void hostapd_set_ml_max_rec_links(struct hostapd_data *hapd, u8 ml_max_rec_links)
{
	struct hostapd_data *link_bss;
	struct hostapd_data *tx_hapd;

	if (!hapd || !hapd->conf->mld_ap)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;
#endif /* CONFIG_QCN_EXTN */

	if (ml_max_rec_links == ML_IE_RSVD_MAX_REC_LINKS)
		ml_max_rec_links = ML_IE_NO_MAX_REC_LINKS;

	for_each_mld_link(link_bss, hapd) {
		link_bss->conf->ml_max_rec_links = ml_max_rec_links;

		wpa_printf(MSG_DEBUG, "enable_aal %d, ml_max_rec_links %u",
			   link_bss->conf->enable_aal, link_bss->conf->ml_max_rec_links);
	}

	wpa_printf(MSG_DEBUG, "link-%u: beacon update with max rec links",
		   hapd->mld_link_id);

	if (!hapd->beacon_set_done) {
		wpa_printf(MSG_DEBUG, "link-%u: is not beaconing, skipping set_beacon",
			   hapd->mld_link_id);
		return;
	}

	ieee802_11_set_beacon(hapd);

	/* If link BSS is non-TX BSS, Invoke set beacon for Tx BSS */
	for_each_mld_link(link_bss, hapd) {
		/* if link bss is non-tx bss, get Tx BSS */
		tx_hapd = hostapd_mbssid_get_tx_bss(link_bss);
		if ((link_bss != tx_hapd) && tx_hapd->beacon_set_done)
			ieee802_11_set_beacon(tx_hapd);
	}
}

void hostapd_validate_update_ml_max_rec_links(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss;

	if (!hapd || !hapd->conf->mld_ap)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;
#endif /* CONFIG_QCN_EXTN */

	for_each_mld_link(link_bss, hapd) {
		if (link_bss == hapd)
			continue;

		if (hapd->conf->ml_max_rec_links > link_bss->conf->ml_max_rec_links) {
			wpa_printf(MSG_DEBUG,
				   "Overwriting ml rec links %u with %u[link id %u]",
				   hapd->conf->ml_max_rec_links,
				   link_bss->conf->ml_max_rec_links,
				   link_bss->mld_link_id);

			hapd->conf->ml_max_rec_links = link_bss->conf->ml_max_rec_links;

			break;
		}
	}
}

#endif /* CONFIG_IEEE80211BE */


struct hostapd_channel_data *
hostapd_iface_get_6ghz_chan_list(struct hostapd_iface *iface, u16 freq,
				 u8 pwr_type, u8 *num_channels_6ghz, u8 *chan_idx)
{
	return hw_mode_get_6ghz_power_mode_channel(iface->hw_features,
						   iface->num_hw_features,
						   freq, pwr_type, num_channels_6ghz,
						   chan_idx, true);
}


/**
 * hapd_psd_to_eirp() - Convert PSD to EIRP
 * @psd: PSD Power
 * @psd_scale: PSD power scale
 * @ch_bw: Channel bandwidth
 * @eirp: Output pointer to EIRP
 *
 * Return: 0 on success, -1 on failure
 */
static int hapd_psd_to_eirp(s16 psd, u8 psd_scale, u16 ch_bw, s16 *eirp)
{
	s16 ten_log10_bw;
	u8 i;
	u8 num_bws;

	/* EIRP = PSD + (10 * log10(CH_BW)) */
	num_bws = ARRAY_SIZE(bw_to_10log10_map);
	for (i = 0; i < num_bws; i++) {
		if (ch_bw == bw_to_10log10_map[i].bw) {
			ten_log10_bw = bw_to_10log10_map[i].ten_l_ten_floor;
			*eirp = psd + ten_log10_bw * psd_scale;
			return 0;
		}
	}

	return -1;
}

/**
 * hapd_eirp_to_psd() - Convert EIRP to PSD
 * @eirp: EIRP Power
 * @eirp_scale: EIRP power scale
 * @ch_bw: Channel bandwidth
 * @psd: Output pointer to PSD
 *
 * Return: 0 on success, -1 on failure
 */
static int hapd_eirp_to_psd(s16 eirp, u8 eirp_scale, u16 ch_bw, s16 *psd)
{
	s16 ten_log10_bw;
	u8 i;
	u8 num_bws;

	/* PSD = EIRP - (10 * log10(CH_BW)) */
	num_bws = ARRAY_SIZE(bw_to_10log10_map);
	for (i = 0; i < num_bws; i++) {
		if (ch_bw == bw_to_10log10_map[i].bw) {
			ten_log10_bw = bw_to_10log10_map[i].ten_l_ten_ceil;
			*psd = eirp - ten_log10_bw * eirp_scale;
			return 0;
		}
	}

	return -1;
}

/**
 * reg_get_eirp_from_psd_and_reg_max_eirp() - Get PSD limited EIRP
 * @psd: PSD Power
 * @bw: Bandwidth in Mhz
 * @reg_eirp_pwr: Output pointer to reg_eirp_pwr
 *
 * This API returns the EIRP as minimum(Regulatory EIRP, EIRP from Regulatory PSD)
 *
 * Return: Void
 */
static void
reg_get_eirp_from_psd_and_reg_max_eirp(s16 psd, u16 bw, s16 *reg_eirp_pwr)
{
	s16 eirp_from_psd = CHAN_MAX_TX_POWER;

	if (hapd_psd_to_eirp(psd, 1, bw, &eirp_from_psd))
		wpa_printf(MSG_ERROR, "Failed to convert PSD to EIRP");

	*reg_eirp_pwr = MIN(*reg_eirp_pwr, eirp_from_psd);
}


/**
 * hostapd_reg_is_pwr_type_client() - Check if the power type is client
 * @pwr_type: Power type
 *
 * Return: True if the power type is client, false otherwise
 */
static bool
hostapd_reg_is_pwr_type_client(u8 pwr_type)
{
	return (pwr_type >= NL80211_REG_REGULAR_CLIENT_LPI &&
		pwr_type <= NL80211_REG_SUBORDINATE_CLIENT_VLP);
}


/**
 * hostapd_reg_find_non_punc_bw() - Find the non-punctured bandwidth
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 *
 * Return: Non-punctured bandwidth
 */
static u16
hostapd_reg_find_non_punc_bw(u16 bw, u16 in_punc_pattern)
{
	u8 num_punc_bw;

	if (!in_punc_pattern)
		return bw;

	num_punc_bw = 0;
	while (in_punc_pattern) {
		if (in_punc_pattern & 0x1)
			num_punc_bw++;
		in_punc_pattern >>= 1;
	}

	if (bw <= num_punc_bw * 20)
		return 0;

	return (bw - num_punc_bw * 20);
}


/**
 * hostapd_reg_get_eirp_from_chan_list() - Get the regulatory EIRP for the freq
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @is_client_lookup: Whether the lookup is for client
 * @is_twice_pwr: Flag to indicate twice power
 * @eirp_pwr: Output pointer to EIRP power
 *
 * Return: 0 on success, -1 on failure
 */
static int
hostapd_reg_get_eirp_from_chan_list(struct hostapd_iface *iface, u16 freq,
				    u16 center_freq, u16 bw, u16 in_punc_pattern,
				    u8 ap_pwr_type, u8 client_type,
				    bool is_client_lookup,
				    bool is_twice_pwr, s16 *eirp_pwr)
{
	struct hostapd_channel_data *chan_6ghz = NULL;
	s16 psd_pwr;
	u16 start_freq = (bw == 20) ? freq : center_freq - (bw / 2) + 10;
	u8 pwr_type = (is_client_lookup) ? client_type : ap_pwr_type;
	bool is_psd;
	u16 effective_bw;
	u8 num_channels_6ghz, chan_idx, i;
	u8 num_bw_chans;
	s16 tmp_eirp_pwr;

	chan_6ghz = hostapd_iface_get_6ghz_chan_list(iface, start_freq, pwr_type,
						     &num_channels_6ghz, &chan_idx);
	if (!chan_6ghz) {
		wpa_printf(MSG_ERROR,
			   "%s Error getting 6 GHz chan: power mode: %d freq: %d",
			   __func__, pwr_type, start_freq);
		return -1;
	}

	num_bw_chans = bw / 20;
	if (chan_idx + num_bw_chans > num_channels_6ghz) {
		wpa_printf(MSG_ERROR,
			   "%s Invalid channel index: %d bw: %d num chans: %d",
			   __func__, chan_idx, bw, num_channels_6ghz);
		return -1;
	}

	*eirp_pwr = CHAN_MAX_TX_POWER;
	effective_bw = hostapd_reg_find_non_punc_bw(bw, in_punc_pattern);
	is_psd = chan_6ghz->flag & HOSTAPD_CHAN_PSD;
	for (i = 0; i < num_bw_chans; i++) {
		if (in_punc_pattern & BIT(i)) {
			wpa_printf(MSG_INFO,
				   "Channel idx: %d is punctured. PP: 0x%x",
				   i, in_punc_pattern);
			chan_6ghz++;
			continue;
		}

		if ((chan_6ghz->flag & HOSTAPD_CHAN_DISABLED) ||
		    (chan_6ghz->flag & HOSTAPD_CHAN_NO_IR)) {
			wpa_printf(MSG_DEBUG,
				   "Freq [%d] is disabled. Flag: 0x%x, power type: %d",
				   chan_6ghz->freq, chan_6ghz->flag, pwr_type);
			return -1;
		}

		tmp_eirp_pwr = chan_6ghz->eirp_power;
		if (is_psd) {
			psd_pwr = chan_6ghz->psd_power;
			reg_get_eirp_from_psd_and_reg_max_eirp(psd_pwr,
							       effective_bw,
							       &tmp_eirp_pwr);
		}

		if (tmp_eirp_pwr < *eirp_pwr)
			*eirp_pwr = tmp_eirp_pwr;

		chan_6ghz++;
	}

	if (is_twice_pwr)
		*eirp_pwr *= 2;

	return 0;
}


/**
 * hostapd_reg_get_psd_from_chan_list() - Get the regulatory PSD for the freq
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @is_client_lookup: Whether the lookup is for client
 * @is_twice_pwr: Flag to indicate twice power
 * @psd_pwr: Output pointer to PSD power
 *
 * Return: 0 on success, -1 on failure
 */
int
hostapd_reg_get_psd_from_chan_list(struct hostapd_iface *iface, u16 freq,
				   u16 center_freq, u16 bw, u16 in_punc_pattern,
				   u8 ap_pwr_type, u8 client_type,
				   bool is_client_lookup, bool is_twice_pwr,
				   s16 *psd_pwr)
{
	struct hostapd_channel_data *chan_6ghz = NULL, *prim_chan;
	u16 start_freq = (bw == 20) ? freq : center_freq - (bw / 2) + 10;
	u8 pwr_type = (is_client_lookup) ? client_type : ap_pwr_type;
	bool is_psd;
	u16 effective_bw;
	u8 num_channels_6ghz, chan_idx, i;
	u8 num_bw_chans;
	s16 tmp_psd_pwr;

	chan_6ghz = hostapd_iface_get_6ghz_chan_list(iface, start_freq, pwr_type,
						     &num_channels_6ghz, &chan_idx);
	if (!chan_6ghz) {
		wpa_printf(MSG_ERROR,
			   "%s Error getting 6 GHz chan: power mode: %d freq: %d",
			   __func__, pwr_type, start_freq);
		return -1;
	}
	prim_chan = hostapd_iface_get_6ghz_chan_list(iface, freq, pwr_type,
						     NULL, NULL);
	if (!prim_chan) {
		wpa_printf(MSG_ERROR,
			   "%s Error getting prim 6 GHz chan: power mode: %d freq: %d",
			   __func__, pwr_type, freq);
		return -1;
	}

	num_bw_chans = bw / 20;
	if (chan_idx + num_bw_chans > num_channels_6ghz) {
		wpa_printf(MSG_ERROR,
			   "Invalid channel index: %d bw: %d num chans: %d",
			   chan_idx, bw, num_channels_6ghz);
		return -1;
	}

	*psd_pwr = CHAN_MAX_TX_POWER;
	effective_bw = hostapd_reg_find_non_punc_bw(bw, in_punc_pattern);
	is_psd = prim_chan->flag & HOSTAPD_CHAN_PSD;
	if (!is_psd) {
		s16 reg_eirp_pwr;
		int ret;

		wpa_printf(MSG_INFO,
			   "Channel %d does not support PSD, flag: 0x%x",
			   prim_chan->freq, prim_chan->flag);
		ret = hostapd_reg_get_eirp_from_chan_list(iface, freq,
							  center_freq,
							  bw,
							  in_punc_pattern,
							  ap_pwr_type,
							  client_type,
							  is_client_lookup,
							  is_twice_pwr,
							  &reg_eirp_pwr);
		if (ret) {
			wpa_printf(MSG_WARNING,
				   "Failed to get EIRP from channel list for freq %d",
				   freq);
			return -1;
		}
		ret = hapd_eirp_to_psd(reg_eirp_pwr, is_twice_pwr ? 2 : 1,
				       effective_bw, psd_pwr);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Failed to convert EIRP to PSD for freq %d",
				   prim_chan->freq);
			return -1;
		}

		if (is_twice_pwr)
			*psd_pwr *= 2;

		return 0;
	}

	for (i = 0; i < num_bw_chans; i++) {
		if (in_punc_pattern & BIT(i)) {
			wpa_printf(MSG_INFO,
				   "%s Channel idx: %d is punctured. PP: 0x%x",
				   __func__, i, in_punc_pattern);
			chan_6ghz++;
			continue;
		}

		if ((chan_6ghz->flag & HOSTAPD_CHAN_DISABLED) ||
		    (chan_6ghz->flag & HOSTAPD_CHAN_NO_IR)) {
			wpa_printf(MSG_ERROR,
				   "%s Freq [%d] is disabled. Flag: 0x%x, power type: %d",
				   __func__, chan_6ghz->freq, chan_6ghz->flag, pwr_type);
			return -1;
		}

		tmp_psd_pwr = chan_6ghz->psd_power;
		if (tmp_psd_pwr < *psd_pwr)
			*psd_pwr = tmp_psd_pwr;

		chan_6ghz++;
	}

	if (is_twice_pwr)
		*psd_pwr *= 2;

	return 0;
}


/**
 * hostapd_limit_afc_power_with_reg() - Limit the AFC EIRP
 *
 * This API limits AFC EIRP with SP regulatory EIRP power
 *
 * @reg_sp_eirp_pwr: Regulatory SP EIRP power
 * @afc_eirp_pwr: AFC EIRP power
 * @is_client_lookup: Whether the lookup is for client
 * @client_type: Client type
 * @is_twice_pwr: Flag to indicate twice power
 *
 * Return: AFC EIRP power limited with regulatory EIRP power
 */
static s16
hostapd_limit_afc_power_with_reg(s16 reg_sp_eirp_pwr, s16 afc_eirp_pwr,
				 bool is_client_lookup, u8 client_type,
				 bool is_twice_pwr)
{
	if (is_client_lookup && hostapd_reg_is_pwr_type_client(client_type))
		afc_eirp_pwr -= SP_AP_AND_CLIENT_POWER_DIFF_IN_SCALE;

	reg_sp_eirp_pwr *= EIRP_PWR_SCALE;
	afc_eirp_pwr = MIN(afc_eirp_pwr, reg_sp_eirp_pwr);

	if (is_twice_pwr)
		afc_eirp_pwr *= 2;

	return afc_eirp_pwr / EIRP_PWR_SCALE;
}


/**
 * hostapd_get_sp_punc_eirp() - Get the SP EIRP power for punctured channels.
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 * @client_type: Client type
 * @is_client_lookup: Whether the lookup is for client
 * @is_twice_pwr: Flag to indicate twice power
 * @reg_sp_eirp_pwr: Regulatory SP EIRP power
 * @sp_eirp_pwr: Output pointer to SP EIRP power, which is adjacent channel
 *	compliant
 *
 * Return: 0 on success, -1 on failure
 */
static int
hostapd_get_sp_punc_eirp(struct hostapd_iface *iface, u16 freq, u16 center_freq,
			 u16 bw, u16 in_punc_pattern, u8 client_type,
			 bool is_client_lookup, bool is_twice_pwr,
			 s16 reg_sp_eirp_pwr, s16 *sp_eirp_pwr)
{
	u16 effective_bw = hostapd_reg_find_non_punc_bw(bw, in_punc_pattern);
	s16 oobe_psd_pwr, afc_eirp_pwr, reg_psd_pwr;
	int ret;

	get_min_psd_values(iface->afc_rsp_info, freq, center_freq,
			   in_punc_pattern, bw, &oobe_psd_pwr);
	if (oobe_psd_pwr == CHAN_MAX_TWICE_TX_POWER * PSD_SCALE) {
		wpa_printf(MSG_ERROR, "Failed to calculate OOBE PSD");
		return -1;
	}

	/* Set is_client_lookup to false to get AP PSD value and to avoid
	 * subtracting 6dbm twice.
	 */
	ret = hostapd_reg_get_psd_from_chan_list(iface, freq, center_freq, bw,
						 in_punc_pattern, NL80211_REG_AP_SP,
						 client_type, false,
						 false, &reg_psd_pwr);
	if (ret)
		return ret;

	reg_psd_pwr *= PSD_SCALE;
	oobe_psd_pwr = MIN(oobe_psd_pwr, reg_psd_pwr);
	if (hapd_psd_to_eirp(oobe_psd_pwr, PSD_SCALE, effective_bw, &afc_eirp_pwr)) {
		wpa_printf(MSG_ERROR, "Unable to get EIRP from PSD");
		return -1;
	}

	*sp_eirp_pwr = hostapd_limit_afc_power_with_reg(reg_sp_eirp_pwr, afc_eirp_pwr,
							is_client_lookup, client_type,
							is_twice_pwr);

	return 0;
}


/**
 * hostapd_find_eirp_in_afc_eirp_obj() - Get the AFC eirp power
 *
 * This API gets the SP EIRP power from the AFC eirp object for the given channel
 * and operating class.
 *
 * @eirp_obj: Pointer to eirp_obj
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @op_class: Operating class
 * @afc_eirp: Output pointer to AFC EIRP power
 *
 * Return: 0 on success, -1 on failure
 */
static int
hostapd_find_eirp_in_afc_eirp_obj(struct chan_eirp_obj *eirp_obj, u16 freq,
				  u16 center_freq, u8 op_class, s16 *afc_eirp)
{
	u8 k, subchannels[MAX_NUM_20_MHZ_IN_CURR_BW], nchans;

	if (is_320_opclass(op_class)) {
		u16 cfi_freq = ieee80211_chan_to_freq(NULL, op_class, eirp_obj->cfi);

		if (cfi_freq == center_freq) {
			*afc_eirp = eirp_obj->eirp_power;
			return 0;
		}

		return -1;
	}

	nchans = get_subchannels_for_opclass(eirp_obj->cfi, op_class, subchannels);
	for (k = 0; k < nchans; k++)
		if (ieee80211_chan_to_freq(NULL, op_class, subchannels[k]) == freq) {
			*afc_eirp = eirp_obj->eirp_power;
			return 0;
		}

	return -1;
}

int
hostapd_find_eirp_in_afc_chan_obj(struct afc_chan_obj *chan_obj, u16 freq,
				  u16 center_freq, u8 op_class, s16 *afc_eirp)
{
	u8 j;
	int ret;

	for (j = 0; j < chan_obj->num_chans; j++) {
		struct chan_eirp_obj *eirp_obj = &chan_obj->chan_eirp_info[j];

		ret = hostapd_find_eirp_in_afc_eirp_obj(eirp_obj, freq, center_freq,
							op_class, afc_eirp);
		if (!ret)
			return 0;
	}

	return -1;
}


/**
 * hostapd_get_sp_eirp() - Get SP EIRP for the given channel parameters.
 * @iface: Pointer to iface
 * @freq: Primary Frequency in MHz
 * @cen320: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 * @client_type: Client power type
 * @is_client_lookup: Flag to indicate client lookup
 * @is_twice_pwr: Flag to indicate twice power
 * @sp_eirp_pwr: Output pointer to SP EIRP power
 *
 * Return: 0 on success, -1 on failure
 */
static int
hostapd_get_sp_eirp(struct hostapd_iface *iface, u16 freq, u16 cen_freq,
		    u16 bw, u16 in_punc_pattern, u8 client_type,
		    bool is_client_lookup, bool is_twice_pwr, s16 *sp_eirp_pwr)
{
	s16 afc_eirp_pwr, reg_sp_eirp_pwr;
	u8 i, op_class;
	struct afc_sp_reg_info *afc_info;
	int ret = -1;
	bool found = false;

	if (!iface->is_afc_power_event_received) {
		wpa_printf(MSG_DEBUG, "AFC power event not received");
		return -1;
	}

	afc_info = iface->afc_rsp_info;
	if (!afc_info || !afc_info->num_chan_objs || !afc_info->num_freq_objs) {
		wpa_printf(MSG_DEBUG, "afc info is NULL");
		return -1;
	}

	ret = hostapd_reg_get_eirp_from_chan_list(iface, freq, cen_freq, bw,
						  in_punc_pattern, NL80211_REG_AP_SP,
						  client_type, is_client_lookup,
						  false, &reg_sp_eirp_pwr);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "Unable to get reg EIRP for SP, Cli Type: %d ",
			   client_type);
		return ret;
	}

	if (in_punc_pattern) {
		return hostapd_get_sp_punc_eirp(iface, freq, cen_freq, bw,
						in_punc_pattern, client_type,
						is_client_lookup, is_twice_pwr,
						reg_sp_eirp_pwr, sp_eirp_pwr);
	}

	if (get_6ghz_opclass_from_bw(bw, freq, &op_class))
		return -1;

	afc_eirp_pwr = CHAN_MIN_TX_POWER * EIRP_PWR_SCALE;
	for (i = 0; i < afc_info->num_chan_objs; i++) {
		struct afc_chan_obj *chan_obj = &afc_info->afc_chan_info[i];

		if (chan_obj->global_opclass != op_class)
			continue;

		ret = hostapd_find_eirp_in_afc_chan_obj(chan_obj, freq, cen_freq,
							op_class, &afc_eirp_pwr);
		if (!ret) {
			found = true;
			break;
		}
	}

	if (ret || !found)
		return -1;

	*sp_eirp_pwr = hostapd_limit_afc_power_with_reg(reg_sp_eirp_pwr, afc_eirp_pwr,
							is_client_lookup, client_type,
							is_twice_pwr);

	return 0;
}


s16 hostapd_get_eirp_pwr(struct hostapd_iface *iface, u16 freq, u16 center_freq,
			 u16 bw, u16 in_punc_pattern, u8 ap_pwr_type,
			 bool is_client_lookup, u8 client_type, bool is_twice_pwr)
{
	int ret;
	s16 eirp_pwr;
	u16 start_freq = (bw == 20) ? freq : center_freq - (bw / 2) + 10;
	u16 pri_chan_bit = (freq - start_freq) / 20;

	if (bw >= 80 &&
	    !is_punct_bitmap_valid(bw, pri_chan_bit, in_punc_pattern)) {
		wpa_printf(MSG_ERROR,
			   "Invalid PP: 0x%x, bw: %d, pri_chan_bit: %d",
			   in_punc_pattern, bw, pri_chan_bit);
		return is_twice_pwr ? CHAN_MIN_TWICE_TX_POWER :
				      CHAN_MIN_TX_POWER;
	}

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_hw_blocklisted_combo_extn(
		    iface, freq, center_freq, bw, in_punc_pattern,
		    ap_pwr_type)) {
		wpa_printf(MSG_DEBUG,
			   "%s: skip blocked combo freq=%u center=%u bw=%u pp=0x%x pwr_mode=%u",
			   __func__, freq, center_freq, bw,
			   in_punc_pattern, ap_pwr_type);
		return is_twice_pwr ? CHAN_MIN_TWICE_TX_POWER :
					      CHAN_MIN_TX_POWER;
	}
#endif /* CONFIG_QCN_EXTN */

	if (ap_pwr_type == NL80211_REG_AP_SP)
		ret = hostapd_get_sp_eirp(iface, freq, center_freq, bw,
					  in_punc_pattern,
					  client_type,
					  is_client_lookup, is_twice_pwr,
					  &eirp_pwr);
	else
		ret = hostapd_reg_get_eirp_from_chan_list(iface, freq, center_freq, bw,
							  in_punc_pattern, ap_pwr_type,
							  client_type, is_client_lookup,
							  is_twice_pwr, &eirp_pwr);

	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "Unable to get EIRP for freq: %d, cf: %d, bw: %d, pp 0x%x"
			   " ap_pwr_type: %d, is_client_lookup: %d, client_type: %d",
			   freq, center_freq, bw, in_punc_pattern,
			   ap_pwr_type, is_client_lookup, client_type);
		return is_twice_pwr ? CHAN_MIN_TWICE_TX_POWER : CHAN_MIN_TX_POWER;
	}

	return eirp_pwr;
}


enum chan_width
hostapd_get_chan_width_from_oper_chan_width(struct hostapd_config *iconf)
{
	enum chan_width ch_width = CHAN_WIDTH_UNKNOWN;

	switch (hostapd_get_oper_chwidth(iconf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (iconf->secondary_channel == 0)
			ch_width = CHAN_WIDTH_20;
		else
			ch_width = CHAN_WIDTH_40;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		ch_width = CHAN_WIDTH_80;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
		ch_width = CHAN_WIDTH_160;
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		ch_width = CHAN_WIDTH_320;
		break;
	default:
		return CHAN_WIDTH_20;
	}

	return ch_width;
}


u8
hostapd_get_best_ap_6ghz_power_mode_for_iface(struct hostapd_iface *iface)
{
	enum chan_width ch_width = hostapd_get_chan_width_from_oper_chan_width(iface->conf);
	u8 center_chan_no = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
	u16 center_freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
						 center_chan_no);

	return hostapd_get_best_ap_6ghz_power_mode(iface, iface->freq,
						   center_freq, channel_width_to_int(ch_width),
						   iface->conf->punct_bitmap);
}


/**
 * hostapd_is_bonded_chan_freq() - Validate freq and BW for channel bonding
 * @freq: Primary frequency in MHz
 * @bonded_chan_entry: Bonded channel entry
 * @bw: Bandwidth in MHz
 * @center_freq_320_mhz: Center frequency for 320 MHz
 *
 * Return: True if the input freq, BW can form a bonded channel, false otherwise
 */
static bool
hostapd_is_bonded_chan_freq(u16 freq,
			    const struct bonded_channel_freq *bonded_chan_entry,
			    u16 bw, u16 center_freq_320_mhz)
{
	if (bw == 320 && center_freq_320_mhz) {
		u16 band_center;

		/*
		 * For the 5GHz 320/240 MHz channel, bonded pair ends are not
		 * symmetric around the center of the channel. Use the start
		 * frequency of the bonded channel to calculate the center
		 */
		if (is_5ghz_freq(freq))
			band_center = bonded_chan_entry->start_freq - 10 + bw / 2;
		else
			band_center = (bonded_chan_entry->start_freq +
					bonded_chan_entry->end_freq) >> 1;

		if (band_center != center_freq_320_mhz)
			return false;
	}

	if (freq >= bonded_chan_entry->start_freq &&
	    freq <= bonded_chan_entry->end_freq)
		return true;

	return false;
}

/**
 * hostapd_get_bonded_chan_entry() - Get the bonded channel entry
 *
 * This API returns the bonded channel entry for the given frequency and
 * bandwidth. For 320 MHz, if center_freq_320_mhz is 0, the function will return
 * the first bonded channel entry that matches the frequency and bandwidth.
 *
 * @freq: Frequency in MHz
 * @bw: Bandwidth in MHz
 * @center_freq_320_mhz: Center frequency for 320 MHz
 *
 * Return: Pointer to the bonded channel entry or NULL if not found.
 */
static const struct bonded_channel_freq *
hostapd_get_bonded_chan_entry(u16 freq, u16 bw, u16 center_freq_320_mhz)
{
	const struct bonded_channel_freq *bonded_chan_arr;
	u16 array_size, i, num_bws;

	num_bws = ARRAY_SIZE(bw_bonded_array_pair_map);
	for (i = 0; i < num_bws; i++) {
		if (bw == bw_bonded_array_pair_map[i].bw) {
			bonded_chan_arr =
				bw_bonded_array_pair_map[i].bonded_chan_arr;
			array_size = bw_bonded_array_pair_map[i].array_size;
			break;
		}
	}

	if (i == num_bws)
		return NULL;

	for (i = 0; i < array_size; i++) {
		if (hostapd_is_bonded_chan_freq(freq, &bonded_chan_arr[i],
						bw, center_freq_320_mhz))
			return &bonded_chan_arr[i];
	}

	return NULL;
}


u16
hostapd_get_bonded_chan_center_freq(u16 freq, u16 bw, u16 center_freq_320_mhz,
				    s8 sec_chan_offset)
{
	const struct bonded_channel_freq *bonded_chan_ptr;

	if (bw == 20)
		return freq;

	if (bw == 40 && (freq >= 2412 && freq <= 2472)) {
		if (sec_chan_offset > 0)
			return freq + 10;

		if (sec_chan_offset < 0)
			return freq - 10;

		wpa_printf(MSG_DEBUG, "Invalid secondary channel offset %d",
			   sec_chan_offset);
		return 0;
	}

	bonded_chan_ptr = hostapd_get_bonded_chan_entry(freq, bw, center_freq_320_mhz);
	if (!bonded_chan_ptr) {
		wpa_printf(MSG_ERROR,
			   "Invalid bonded channel freq: %d, bw: %d, center freq: %d",
			   freq, bw, center_freq_320_mhz);
		return 0;
	}

	return bonded_chan_ptr->start_freq - 10 + bw / 2;
}


/** hostapd_get_num_puncture_types() - Get the number of puncture types
 * @bw: Bandwidth in MHz
 * @num_punc_type1: Pointer to store the number of puncture patterns with
 *                  the smallest granularity supported for the given bandwidth
 * @num_punc_type2: Pointer to store the number of puncture patterns with
 *                  the second smallest granularity supported for the given
 *                  bandwidth, if applicable
 * @num_punc_type3: Pointer to store the number of puncture patterns with
 *                  the largest granularity supported for the given bandwidth,
 *                  if applicable
 *
 * Return: None
 */
static void hostapd_get_num_puncture_types(u16 bw, u8 *num_punc_type1,
					   u8 *num_punc_type2, u8 *num_punc_type3)
{
	*num_punc_type1 = 0;
	*num_punc_type2 = 0;
	*num_punc_type3 = 0;

	switch (bw) {
	case 320:
		*num_punc_type1 = NUM_40PP_PUNC_320MHZ;
		*num_punc_type2 = NUM_80PP_PUNC_320MHZ;
		*num_punc_type3 = NUM_40P80PP_PUNC_320MHZ;
		break;
	case 160:
		*num_punc_type1 = NUM_20PP_PUNC_160MHZ;
		*num_punc_type2 = NUM_40PP_PUNC_160MHZ;
		break;
	case 80:
		*num_punc_type1 = NUM_20PP_PUNC_80MHZ;
		break;
	}
}


/**
 * hostapd_get_valid_puncture_pattern_arr() - Get the valid puncture pattern array
 * @bw: Bandwidth in MHz
 * @num_pp: Output pointer to store the number of valid puncture patterns
 * @pp_mask: Output pointer to store the puncture pattern mask for the given bandwidth
 *
 * Return: Pointer to the valid puncture pattern array or NULL if not found
 */
const u16 *
hostapd_get_valid_puncture_pattern_arr(u16 bw, u16 *num_pp, u16 *pp_mask)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(bw_puncture_bitmap_pair_map); i++) {
		if (bw == bw_puncture_bitmap_pair_map[i].bw) {
			*num_pp = bw_puncture_bitmap_pair_map[i].array_size;
			*pp_mask = bw_puncture_bitmap_pair_map[i].puncture_mask;
			return bw_puncture_bitmap_pair_map[i].puncture_bitmap_arr;
		}
	}

	return NULL;
}


int hostapd_get_tpe_11ax_count(u8 tx_pwr_intrpn, u8 tx_pwr_count)
{
	switch (tx_pwr_intrpn) {
	case LOCAL_EIRP_PSD:
	case REGULATORY_CLIENT_EIRP_PSD:
	case REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD:
		if (tx_pwr_count > IEEE80211_TPE_PSD_MAX_POWER_COUNT_IN_11AX) {
			wpa_printf(MSG_ERROR,
				   "Invalid Tx Power count %d, Interpretation %d supports up to %d",
				   tx_pwr_count, tx_pwr_intrpn,
				   IEEE80211_TPE_PSD_MAX_POWER_COUNT_IN_11AX);
			return -1;
		}
		return tx_pwr_count ? 1 << (tx_pwr_count - 1) : 1;
	case LOCAL_EIRP:
	case REGULATORY_CLIENT_EIRP:
	case REGULATORY_CLIENT_ADDITIONAL_EIRP: {
		u8 eirp_max = IEEE80211_TPE_EIRP_MAX_POWER_COUNT_IN_11AX;
#ifdef CONFIG_IEEE80211BE
		eirp_max = IEEE80211_TPE_EIRP_MAX_POWER_COUNT_IN_11BE;
#endif
		if (tx_pwr_count > eirp_max) {
			wpa_printf(MSG_ERROR,
				   "Invalid Tx Power count %d, Interpretation %d supports up to %d",
				   tx_pwr_count, tx_pwr_intrpn, eirp_max);
			return -1;
		}
		return tx_pwr_count + 1;
	}
	default:
		wpa_printf(MSG_ERROR, "Invalid Tx power interpretation:%d", tx_pwr_intrpn);
		return -1;
	}
}
/**
 * hostapd_get_eirp_powers() - Get EIRP powers for all AP power modes
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 * @eirp_vals: Output array to store EIRP powers for each power mode
 *
 * This function retrieves the EIRP powers for all 6 GHz AP power modes and stores
 * them in the provided eirp_vals array.
 */
static void
hostapd_get_eirp_powers(struct hostapd_iface *iface, u16 freq,
			u16 center_freq, u16 bw, u16 in_punc_pattern,
			s16 *eirp_vals)
{
	u8 i;

	for (i = NL80211_REG_AP_LPI; i <= NL80211_REG_AP_VLP; i++) {
		eirp_vals[i] =
		    hostapd_get_eirp_pwr(iface, freq, center_freq, bw,
					 in_punc_pattern, i, false,
					 NL80211_REG_NUM_POWER_MODES, false);
		wpa_printf(MSG_DEBUG,
			   "EIRP for freq %d, center_freq %d, bw %d, pp 0x%x, power mode %d: %d",
			   freq, center_freq, bw, in_punc_pattern,
			   i, eirp_vals[i]);
	}
}


bool hostapd_allow_6ghz_dynamic_puncture(struct hostapd_iface *iface, u16 freq, u8 pwr_type)
{
	return (is_6ghz_freq(freq) && iface->conf->ieee80211be &&
		!iface->conf->puncture_strict_6ghz &&
		(pwr_type == NL80211_REG_AP_SP || iface->conf->enable_best_power_mode));
}


void hostapd_apply_6ghz_dynamic_puncturing(struct hostapd_iface *iface)
{
	u16 best_6ghz_pp, center_freq;
	enum chan_width width;
	u8 center_chan_no;
	s8 ret;

	if (!hostapd_allow_6ghz_dynamic_puncture(iface, iface->freq,
						 iface->conf->he_6ghz_reg_pwr_type))
		return;

	center_chan_no = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
	center_freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class, center_chan_no);
	width = hostapd_get_chan_width_from_oper_chan_width(iface->conf);
	best_6ghz_pp = iface->conf->punct_bitmap;
	ret = hostapd_get_6ghz_best_pp(iface, iface->freq,
				       center_freq, channel_width_to_int(width),
				       &best_6ghz_pp, iface->conf->enable_best_power_mode);
	if (!ret)
		iface->conf->punct_bitmap = best_6ghz_pp;

	return;
}


/**
 * hostapd_get_start_freq() - Get the start frequency for a given bandwidth
 * @freq: Primary frequency in MHz
 * @bw: Bandwidth in MHz
 * @center_freq: Band center frequency
 *
 * Return: The start frequency for the given bandwidth
 */
static inline u16
hostapd_get_start_freq(u16 freq, u16 bw, u16 center_freq)
{
	return (bw == 20) ? freq : center_freq - (bw / 2) + 10;
}


/**
 * hostapd_is_pp_subset_and_valid() - Check if the new pp is a subset of the input pp
 * @input_pp: Input puncture pattern
 * @new_pp: New puncture pattern to check
 * @pp_mask: Puncture pattern mask for the given bandwidth
 * @pri_chan_pos: Primary channel position
 *
 * Return: True if the new puncture pattern is a valid subset of the input
 * puncture pattern and does not puncture the primary channel.
 */
static inline bool
hostapd_is_pp_subset_and_valid(u16 input_pp, u16 new_pp,
			       u16 pp_mask, u16 pri_chan_pos)
{
	bool is_subset_pp = (new_pp == ((input_pp | new_pp) & pp_mask));
	bool is_pri_chan_punctured = (BIT(pri_chan_pos) & new_pp);

	return is_subset_pp && !is_pri_chan_punctured;
}


/**
 * hostapd_is_optimal_pp_found() - Check if the optimal puncture pattern is found
 * @n_punc_type1: Number of puncture patterns of type 1 for given BW
 * @n_punc_type2: Number of puncture patterns of type 2 for given BW
 * @n_punc_type3: Number of puncture patterns of type 3 for given BW
 * @i: Current index in the loop
 * @ref_eirp: Reference EIRP power
 * @initial_sp_eirp: Initial SP EIRP power
 *
 * This function checks if an optimal puncture pattern is found at the end of a
 * puncture type group for a given bandwidth. If the reference EIRP power
 * is greater than the initial SP EIRP power at the end of a puncture
 * type group, then stop further puncturing - no need to over-puncture.
 *
 * Return: True if the optimal puncture pattern is found at the end of a
 * puncture type group, false otherwise
 */
static inline bool
hostapd_is_optimal_pp_found(u8 n_punc_type1, u8 n_punc_type2, u8 n_punc_type3,
			     u16 i, s16 ref_eirp, s16 initial_sp_eirp)
{
	bool is_end_of_group = (i == (n_punc_type1 - 1)) ||
			       (i == (n_punc_type1 + n_punc_type2 - 1)) ||
			       (i == (n_punc_type1 + n_punc_type2 + n_punc_type3 - 1));

	return is_end_of_group && (ref_eirp > initial_sp_eirp);
}


/**
 * hostapd_get_valid_pp() - Get a valid puncture pattern
 * @pp: Input/output pointer to the puncture pattern
 * @bw: Bandwidth in MHz
 * @pri_chan_pos: Primary channel position
 *
 * Returns a valid puncture pattern that is a subset of the input
 * puncture pattern.
 * If no valid puncture pattern is found, return -1.
 */
static int
hostapd_get_valid_pp(u16 *pp, u16 bw, u16 pri_chan_pos)
{
	const u16 *bw_pp_arr;
	u16 num_pp, pp_mask;
	u16 i;

	bw_pp_arr = hostapd_get_valid_puncture_pattern_arr(bw, &num_pp, &pp_mask);
	if (!bw_pp_arr) {
		wpa_printf(MSG_ERROR,
			   "No valid puncture pattern array for bw %d", bw);
		return -1;
	}

	for  (i = 0; i < num_pp; i++) {
		if (hostapd_is_pp_subset_and_valid(*pp, bw_pp_arr[i],
						   pp_mask, pri_chan_pos)) {
			*pp = bw_pp_arr[i];
			return 0;
		}
	}

	wpa_printf(MSG_ERROR,
		   "Invalid PP: 0x%x, bw: %d, pri_chan_pos: %d",
		   *pp, bw, pri_chan_pos);
	return -1;
}

/**
 * hostapd_get_optimal_pp() - Get the optimal puncture pattern
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @pp: Input puncture pattern
 * @pri_chan_pos: Primary channel position
 * @initial_sp_eirp: Initial SP EIRP power
 * @ref_eirp: Output pointer to reference EIRP power
 * @out_pp: Output pointer to optimal puncture pattern
 *
 * This function finds the optimal puncture pattern that maximizes the EIRP power.
 */
static void
hostapd_get_optimal_pp(struct hostapd_iface *iface, u16 freq,
		       u16 center_freq, u16 bw, u16 pp, u16 pri_chan_pos,
		       s16 initial_sp_eirp, s16 *ref_eirp, u16 *out_pp)
{
	u8 n_punc_type1, n_punc_type2, n_punc_type3;
	u16 num_pp, pp_mask;
	const u16 *bw_pp_arr;
	s16 tmp_eirp_pwr;
	u16 i;

	bw_pp_arr = hostapd_get_valid_puncture_pattern_arr(bw, &num_pp, &pp_mask);
	if (!bw_pp_arr) {
		wpa_printf(MSG_ERROR, "No valid puncture pattern array found for bw %d", bw);
		return;
	}

	hostapd_get_num_puncture_types(bw, &n_punc_type1, &n_punc_type2,
				       &n_punc_type3);

	*ref_eirp = initial_sp_eirp;
	for  (i = 0; i < num_pp; i++) {
		if (!hostapd_is_pp_subset_and_valid(pp, bw_pp_arr[i], pp_mask, pri_chan_pos))
			continue;
		tmp_eirp_pwr = hostapd_get_eirp_pwr(iface, freq, center_freq, bw, bw_pp_arr[i],
						    NL80211_REG_AP_SP, false,
						    NL80211_REG_NUM_POWER_MODES, false);

		wpa_printf(MSG_DEBUG, "Trying PP %x freq %d, cf %d, bw %d EIRP: %d > ref EIRP: %d",
			   bw_pp_arr[i], freq, center_freq, bw, tmp_eirp_pwr, *ref_eirp);

		if (tmp_eirp_pwr > *ref_eirp) {
			*ref_eirp = tmp_eirp_pwr;
			*out_pp = bw_pp_arr[i];
		}

		if (hostapd_is_optimal_pp_found(n_punc_type1, n_punc_type2, n_punc_type3,
						 i, *ref_eirp, initial_sp_eirp)) {
			wpa_printf(MSG_INFO,
				   "SP Punc: Freq %d CF %d BW %d Input PP %x Final PP %x",
				   freq, center_freq, bw, pp, *out_pp);
			wpa_printf(MSG_INFO,
				   "SP Punc: EIRP %d Initial EIRP %d",
				   *ref_eirp, initial_sp_eirp);
			return;
		}
	}

	return;
}


s8 hostapd_get_6ghz_best_pp(struct hostapd_iface *iface, u16 freq,
			    u16 center_freq, u16 bw, u16 *pp,
			    bool is_bpm_enabled)
{
	u16 start_freq = hostapd_get_start_freq(freq, bw, center_freq);
	u16 pri_chan_pos = (freq - start_freq) / 20;
	s16 initial_sp_eirp, ref_eirp, non_sp_eirp;
	s16 eirp_vals[NL80211_REG_AP_VLP + 1];
	u16 out_pp = 0;

	if (!is_punct_bitmap_valid(bw, pri_chan_pos, *pp)) {
		if (hostapd_get_valid_pp(pp, bw, pri_chan_pos))
			return -1;
	}

	hostapd_get_eirp_powers(iface, freq, center_freq, bw, *pp, eirp_vals);
	initial_sp_eirp = eirp_vals[NL80211_REG_AP_SP];
	if (initial_sp_eirp == CHAN_MIN_TX_POWER)
		out_pp = PUNCTURE_INVALID;

	if (iface->conf->punc_eirp_thres_6ghz == CHAN_MIN_TX_POWER) {
		/*
		 * Threshold unset: skip puncturing if initial SP EIRP is
		 * already valid (> minimum). Try puncturing only when
		 * initial SP EIRP is not valid.
		 */
		if (initial_sp_eirp > CHAN_MIN_TX_POWER)
			return 0;
	} else {
		if (initial_sp_eirp > iface->conf->punc_eirp_thres_6ghz) {
			wpa_printf(MSG_INFO,
				   "AFC: Initial SP EIRP %d is greater than threshold %d",
				   initial_sp_eirp, iface->conf->punc_eirp_thres_6ghz);
			return 0;
		}

		initial_sp_eirp = iface->conf->punc_eirp_thres_6ghz;
	}
	hostapd_get_optimal_pp(iface, freq, center_freq, bw, *pp, pri_chan_pos,
			       initial_sp_eirp, &ref_eirp, &out_pp);
	if (out_pp == PUNCTURE_INVALID) {
		wpa_printf(MSG_ERROR,
			   "No valid SP PP found for freq %d, center_freq %d, bw %d",
			   freq, center_freq, bw);
		return -1;
	}

	/**
	 * If best power mode is enabled, compare if the derived power is
	 * greater than both the SP and non-SP EIRP powers.
	 * Else, compare only with the SP EIRP power.
	 */
	non_sp_eirp = eirp_vals[NL80211_REG_AP_LPI];
	if (eirp_vals[NL80211_REG_AP_VLP] > non_sp_eirp)
		non_sp_eirp = eirp_vals[NL80211_REG_AP_VLP];

	if ((!is_bpm_enabled && ref_eirp > initial_sp_eirp) ||
	    (is_bpm_enabled && ref_eirp > MAX(initial_sp_eirp, non_sp_eirp)))
		*pp = out_pp;

	return 0;
}


u8
hostapd_get_best_ap_6ghz_power_mode(struct hostapd_iface *iface,
				    u16 freq, u16 center_freq, u16 bw,
				    u16 in_punc_pattern)
{
	static const enum nl80211_regulatory_power_modes p_mode_order[] = {
		NL80211_REG_AP_LPI,
		NL80211_REG_AP_VLP,
		NL80211_REG_AP_SP,
	};
	int i;
	u8 best_ap_pwr_mode = NL80211_REG_NUM_POWER_MODES;
	s16 max_eirp_pwr = CHAN_MIN_TX_POWER;
	s16 eirp_vals[NL80211_REG_AP_VLP + 1];

	if (bw > 20 && !hostapd_get_bonded_chan_entry(freq, bw, center_freq)) {
		wpa_printf(MSG_ERROR,
			   "BPM: Invalid bonded channel freq %d, bw %d, center_freq %d",
			   freq, bw, center_freq);
		return best_ap_pwr_mode;
	}

	hostapd_get_eirp_powers(iface, freq, center_freq, bw, in_punc_pattern, eirp_vals);
	for (i = 0; i < ARRAY_SIZE(p_mode_order); i++) {
		s16 tmp_eirp_pwr = eirp_vals[p_mode_order[i]];

		wpa_printf(MSG_INFO,
			   "%s freq %d, cfreq %d, bw %d, pp %d, pwr_type %d, EIRP %d",
			   __func__, freq, center_freq, bw, in_punc_pattern, p_mode_order[i],
			   tmp_eirp_pwr);

		if (tmp_eirp_pwr > max_eirp_pwr) {
			max_eirp_pwr = tmp_eirp_pwr;
			best_ap_pwr_mode = p_mode_order[i];
		}
	}

	return best_ap_pwr_mode;
}

u16 hostapd_get_punct_bitmap(struct hostapd_data *hapd)
{
	u16 punct_bitmap = 0;

#ifdef CONFIG_IEEE80211BE
	punct_bitmap = hapd->iconf->punct_bitmap;
#ifdef CONFIG_TESTING_OPTIONS
	if (!punct_bitmap)
		punct_bitmap = hapd->conf->eht_oper_puncturing_override;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */

	return punct_bitmap;
}


size_t hostapd_get_mbssid_max_num_bss(struct hostapd_data *hapd)
{
	if (!hapd->iconf->mbssid || !hapd->iface)
		return 0;

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		if (!hapd->mbssid_group)
			return 0;
		return hapd->mbssid_group->num_bss;
	} else {
		return hapd->iface->num_bss;
	}
}


struct hostapd_data *
hostapd_get_mbssid_bss_by_idx(struct hostapd_data *hapd, size_t idx)
{
	struct hostapd_data *bss;

	if (!hapd->iface || !hapd->iconf ||
	    hapd->iconf->mbssid == MBSSID_DISABLED)
		return NULL;

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

		if (!group)
			return NULL;

		dl_list_for_each(bss, &group->bss_list, struct hostapd_data,
				 mbssid_bss) {
			if (!bss->conf || !bss->started ||
			    !bss->beacon_set_done)
				continue;

			if (bss->mbssid_idx == idx)
				return bss;
		}
	} else {
		size_t i = 0;

		for (i = 0; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];

			if (!bss || !bss->conf || !bss->started ||
			    !bss->beacon_set_done)
				continue;

			if (bss->mbssid_idx == idx)
				return bss;
		}
	}

	return NULL;
}


/**
 * is_afc_info_usable() - Check if AFC info is usable
 * @afc_info: Pointer to AFC info
 *
 * Return: true if usable, false otherwise
 */
static bool
is_afc_info_usable(struct afc_sp_reg_info *afc_info)
{
	if (!afc_info) {
		wpa_printf(MSG_WARNING, "AFC info is NULL or empty");
		return false;
	}
	if (!afc_info->num_chan_objs) {
		wpa_printf(MSG_WARNING, "AFC info has no channel objects");
		return false;
	}
	return true;
}

/**
 * ieee80211_validate_chan_bw_in_afc_response() - Validate channel
 * bandwidth in AFC response
 * @iface: Pointer to hostapd interface data
 * @freq: Frequency of the channel
 * @cen_freq: Center frequency of the channel
 * @bw: Bandwidth of the channel
 * @pp: Puncturing bitmap
 * @he_6ghz_pwr_mode: HE 6 GHz power mode
 *
 * Return: true if valid, false otherwise
 */
static bool
ieee80211_validate_chan_bw_in_afc_response(struct hostapd_iface *iface,
					   u16 freq, u16 cen_freq, u16 bw,
					   u16 pp, u8 he_6ghz_pwr_mode)
{
	struct afc_sp_reg_info *afc_info;
	bool valid;
	u8 i;
	u8 op_class;

	if (he_6ghz_pwr_mode != HE_REG_INFO_6GHZ_AP_TYPE_SP || pp)
		return true;

	afc_info = iface->afc_rsp_info;

	if (!is_afc_info_usable(afc_info)) {
		wpa_printf(MSG_WARNING, "AFC info is not usable");
		return false;
	}

	if (get_6ghz_opclass_from_bw(bw, freq, &op_class))
		return false;

	valid = false;
	for (i = 0; i < afc_info->num_chan_objs; i++) {
		struct afc_chan_obj *chan_obj = &afc_info->afc_chan_info[i];
		s16 afc_eirp_pwr = CHAN_MIN_TX_POWER;
		int ret;

		if (chan_obj->global_opclass != op_class)
			continue;

		ret = hostapd_find_eirp_in_afc_chan_obj(chan_obj, freq, cen_freq,
							op_class, &afc_eirp_pwr);
		if (!ret) {
			valid = true;
			wpa_printf(MSG_DEBUG,
				   "AFC EIRP power %d for freq %d, center freq %d, op class %d",
				   afc_eirp_pwr, freq, cen_freq, op_class);
			break;
		}
	}
	return valid;
}

static bool chan_phy_mode_valid(struct hostapd_iface *iface,
				struct hostapd_channel_data *chan)
{
#ifdef CONFIG_IEEE80211BN
	if (iface->conf->ieee80211bn && chan->flag & HOSTAPD_CHAN_NO_UHR) {
		wpa_printf(MSG_DEBUG,
			   "Channel %d MHz is not allowed for UHR operation",
			   chan->freq);
		return false;
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_IEEE80211BE
	if (iface->conf->ieee80211be && chan->flag & HOSTAPD_CHAN_NO_EHT) {
		wpa_printf(MSG_DEBUG,
			   "Channel %d MHz is not allowed for EHT operation",
			   chan->freq);
		return false;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	if (iface->conf->ieee80211ax && chan->flag & HOSTAPD_CHAN_NO_HE) {
		wpa_printf(MSG_DEBUG,
			   "Channel %d MHz is not allowed for HE operation",
			   chan->freq);
		return false;
	}
#endif /* CONFIG_IEEE80211BE */

	return true;
}

bool
hostapd_validate_non_6ghz_chan_bw(struct hostapd_iface *iface, u16 freq,
				  u16 center_freq, u16 bw, u16 pp)
{
	int start_freq, num_bw_chans;
	u8 i;
	s8 sec_chan_offset = 0;

	if (bw == 20) {
		start_freq = freq;
		num_bw_chans = 1;
	} else {
		start_freq = center_freq - (bw / 2) + 10;
		num_bw_chans = bw / 20;
	}

#ifdef CONFIG_QCN_EXTN
	if (bw == 320 &&
	    hostapd_get_n_chans_and_frequency_extn(CONF_OPER_CHWIDTH_320MHZ,
						   center_freq,
						   &num_bw_chans,
						   &start_freq))
		return false;
#endif

	if (bw == 40) {
		if (center_freq > freq)
			sec_chan_offset = 1;
		else if (center_freq < freq)
			sec_chan_offset = -1;
	}

	if (!hostapd_get_bonded_chan_center_freq(freq, bw, 0, sec_chan_offset)) {
		wpa_printf(MSG_ERROR,
			   "non-6ghz validate: freq %u no bonded center at bw %u",
			   freq, bw);
		return false;
	}

	for (i = 0; i < num_bw_chans; i++) {
		struct hostapd_channel_data *chan;
		int sub_freq = start_freq + i * 20;

		if (pp & BIT(i))
			continue;

		chan = hw_get_channel_freq(iface->conf->hw_mode, sub_freq, NULL,
					   iface->hw_features,
					   iface->num_hw_features);
		if (!chan) {
			wpa_printf(MSG_ERROR,
				   "non-6ghz validate: freq %u not found in hw_features",
				   sub_freq);
			return false;
		}

		if (!chan_pri_allowed(chan)) {
			wpa_printf(MSG_ERROR,
				   "non-6ghz validate: freq %u not pri-allowed flag=0x%x bw=0x%x",
				   sub_freq, chan->flag, chan->allowed_bw);
			return false;
		}

		if (!chan_bw_allowed(chan, bw, sec_chan_offset == 1, sub_freq == freq)) {
			wpa_printf(MSG_ERROR,
				   "non-6ghz validate: freq %u BW %u is not supported",
				   sub_freq, bw);
			return false;
		}

		if (!chan_phy_mode_valid(iface, chan))
			return false;

		if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_UNAVAILABLE) {
			wpa_printf(MSG_ERROR,
				   "non-6ghz validate: freq %u DFS_UNAVAILABLE flag=0x%x",
				   sub_freq, chan->flag);
			return false;
		}
	}

	return true;
}

bool
hostapd_validate_chan_bw_in_pwr_mode(struct hostapd_iface *iface, u16 freq,
				     u16 center_freq, u16 bw, u16 pp,
				     u8 pwr_type)
{
	u16 start_freq = (bw == 20) ? freq : center_freq - (bw / 2) + 10;
	u8 num_channels_6ghz, chan_idx, i, num_bw_chans = bw / 20;
	struct hostapd_channel_data *chan_6ghz = NULL;
	u16 pri_chan_pos = (freq - start_freq) / 20;
	int ht40_plus = 0;

	wpa_printf(MSG_INFO,
		   "Validating power mode: %d, Freq: %d, cf: %d, BW: %d, pp: 0x%x",
		   pwr_type, freq, center_freq, bw, pp);

	if (!is_punct_bitmap_valid(bw, pri_chan_pos, pp)) {
		wpa_printf(MSG_ERROR, "pp 0x%x invalid for freq %d BW %d",
			   pp, freq, bw);
		return false;
	}

	if (!is_6ghz_freq(freq))
		return hostapd_validate_non_6ghz_chan_bw(iface, freq,
							 center_freq, bw, pp);

	if (bw > 20) {
		u16 cen = hostapd_get_bonded_chan_center_freq(freq, bw,
							      center_freq, 0);

		if (!cen) {
			wpa_printf(MSG_ERROR,
				   "validate_chan_bw 6G: freq %u no bonded center at bw %u",
				   freq, bw);
			return false;
		}
	}

	chan_6ghz = hostapd_iface_get_6ghz_chan_list(iface,
						     start_freq,
						     pwr_type,
						     &num_channels_6ghz,
						     &chan_idx);

	if (!chan_6ghz) {
		wpa_printf(MSG_ERROR,
			   "Error getting 6 GHz chan: power mode: %d freq: %d",
			   pwr_type, start_freq);
		return false;
	}

	if (chan_idx + num_bw_chans > num_channels_6ghz) {
		wpa_printf(MSG_ERROR,
			   "Invalid channel index: %d bw: %d num chans: %d",
			   chan_idx, bw, num_channels_6ghz);
		return false;
	}

	if (bw == 40) {
		if (center_freq > freq)
			ht40_plus = 1;
		else if (center_freq < freq)
			ht40_plus = 0;
	}

	for (i = 0; i < num_bw_chans; i++) {
		int sub_freq = start_freq + i * 20;

		if (pp & BIT(i)) {
			wpa_printf(MSG_INFO,
				   "Channel idx: %d is punctured. PP: 0x%x", i, pp);
			chan_6ghz++;
			continue;
		}

		if ((chan_6ghz->flag & HOSTAPD_CHAN_DISABLED) ||
			(chan_6ghz->flag & HOSTAPD_CHAN_NO_IR)) {
			wpa_printf(MSG_ERROR,
				   "Freq [%d] is disabled. Flag: 0x%x, power type: %d",
				   chan_6ghz->freq, chan_6ghz->flag, pwr_type);
			return false;
		}

		if (!chan_bw_allowed(chan_6ghz, bw, ht40_plus, sub_freq == freq)) {
			wpa_printf(MSG_ERROR,
				   "validate_chan_bw 6G: freq %u BW %u is not supported",
				   sub_freq, bw);
			return false;
		}

		if (!chan_phy_mode_valid(iface, chan_6ghz))
			return false;

		chan_6ghz++;
	}

	if (!ieee80211_validate_chan_bw_in_afc_response(iface, freq, center_freq,
							bw, pp, pwr_type)) {
		wpa_printf(MSG_WARNING,
			   "Channel %d, bw: %d is not supported in power mode %d",
			   freq, bw, pwr_type);
		return false;
	}

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_hw_blocklisted_combo_extn(
		    iface, freq, center_freq, bw, pp, pwr_type)) {
		wpa_printf(MSG_DEBUG,
			   "%s: blocked combo freq=%u center=%u bw=%u pp=0x%x pwr_mode=%u",
			   __func__, freq, center_freq, bw, pp, pwr_type);
		return false;
	}
#endif /* CONFIG_QCN_EXTN */

	return true;
}

static int hostapd_remove_vendor_elements(struct hostapd_bss_config *conf,  struct wpabuf *buf,
					  bool remove)
{
	const u8 *needle = wpabuf_head_u8(buf);
	size_t needle_len = wpabuf_len(buf);
	int i;

	for (i = 0; i < conf->vendor_elements_count; i++) {
		struct wpabuf *entry;
		const u8 *entry_data;
		size_t entry_len;

		entry = conf->vendor_elements[i];
		entry_data = wpabuf_head_u8(entry);
		entry_len = wpabuf_len(entry);

		if (entry_len == needle_len &&
		    os_memcmp(entry_data, needle, needle_len) == 0) {
			conf->vendor_elements_len -= wpabuf_len(conf->vendor_elements[i]);
			wpabuf_free(entry);
			os_remove_in_array(conf->vendor_elements, conf->vendor_elements_count,
					   sizeof(struct wpabuf *), i);
			conf->vendor_elements_count--;
			wpa_printf(MSG_DEBUG, "Removed vendor element (new count=%zu)",
				   conf->vendor_elements_count);
			return 0;
		}
	}

	if (remove) {
		wpa_printf(MSG_ERROR, "Vendor elements entry not found count=%zu",
			   conf->vendor_elements_count);
		return -1;
	}

	return 0;
}


static bool hostapd_validate_vendor_elements(struct hostapd_bss_config *conf, struct wpabuf *buf)
{
	const u8 *data;
	size_t pos = 0, total;

	data = wpabuf_head_u8(buf);
	total = wpabuf_len(buf);

	while (pos < total) {
		pos += data[pos + 1] + IEEE80211_ELEM_HEADER_LEN;
		if (pos > total) {
			wpa_printf(MSG_ERROR, "Vendor IE Truncated: total=%zu ie_len=%zu",
				   total, pos);
			return false;
		}
	}
	return true;
}

static int hostapd_handle_vendor_elements_remove(struct hostapd_bss_config *conf,
						 struct wpabuf *buf)
{
	struct wpabuf *b;
	const u8 *data;
	size_t pos = 0, total;

	if (conf->vendor_elements_count == 0) {
		wpa_printf(MSG_ERROR, "No vendor elements");
		return -1;
	}

	if (!hostapd_validate_vendor_elements(conf, buf)) {
		wpa_printf(MSG_ERROR, "Vendor elements length mismatch");
		return -1;
	}

	data = wpabuf_head_u8(buf);
	total = wpabuf_len(buf);

	while (pos + IEEE80211_ELEM_HEADER_LEN < total) {
		size_t ie_total_len = data[pos + 1] + IEEE80211_ELEM_HEADER_LEN;

		b = wpabuf_alloc_copy(&data[pos], ie_total_len);
		if (!b)
			return -1;

		if (hostapd_remove_vendor_elements(conf, b, true) < 0) {
			wpabuf_free(b);
			return -1;
		}
		wpabuf_free(b);
		pos += ie_total_len;
	}
	return 0;
}

static int hostapd_handle_vendor_elements_add(struct hostapd_data *hapd,
					      struct hostapd_bss_config *conf,
					      struct wpabuf *buf,
					      bool is_bcn_update_needed)
{
	struct hostapd_data *tx_hapd;
	struct wpabuf *b;
	const u8 *data;
	size_t pos = 0, total;

	if (conf->vendor_elements_count >= MAX_VENDOR_ELEM_ALLOWED) {
		wpa_printf(MSG_ERROR, "Vendor elements limit exceeds max_count:%d",
			   MAX_VENDOR_ELEM_ALLOWED);
		return -1;
	}

	data = wpabuf_head_u8(buf);
	total = wpabuf_len(buf);

	if (hapd) {
		tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
		if (tx_hapd && (tx_hapd != hapd) &&
		    (total > conf->available_vendor_elem_size)) {
			wpa_printf(MSG_ERROR,
				   "Vendor element size(%zu) exceeds available space(%zu) ",
				   total, conf->available_vendor_elem_size);
			return -1;
		}
	}

	/*
	 * Skip vendor element validation when elements are added via
	 * the hostapd configuration file.
	 * Validation is performed only vendor elements are added hostapd cli
	 * command.
	 */
	if (is_bcn_update_needed && !hostapd_validate_vendor_elements(conf, buf)) {
		wpa_printf(MSG_ERROR, "Vendor elements add: Validation failed");
		return -1;
	}

	while (pos + IEEE80211_ELEM_HEADER_LEN < total) {
		size_t ie_total_len = data[pos + 1]  + IEEE80211_ELEM_HEADER_LEN;

		b = wpabuf_alloc_copy(&data[pos], ie_total_len);
		if (!b)
			return -1;

		hostapd_remove_vendor_elements(conf, b, false);
		if (conf->vendor_elements_count >= MAX_VENDOR_ELEM_ALLOWED) {
			wpa_printf(MSG_ERROR, "Vendor elements limit exceeds(%zu) max_count (%d)",
				   conf->vendor_elements_count, MAX_VENDOR_ELEM_ALLOWED);
			wpabuf_free(b);
			return -1;
		}
		conf->vendor_elements[conf->vendor_elements_count++] = b;
		conf->vendor_elements_len += ie_total_len;
		pos += ie_total_len;
	}
	return 0;
}

int hostapd_handle_vendor_elements_update(struct hostapd_data *hapd,
					  struct hostapd_bss_config *conf, struct wpabuf *data,
					  char *cmd, char *val, bool is_bcn_update_needed)
{
	struct wpabuf *buf;
	size_t len;
	int ret;

	if (!hapd && is_bcn_update_needed) {
		wpa_printf(MSG_ERROR, "hapd is NULL");
		return -1;
	}

	len = os_strlen(val);
	if (len & 0x01) {
		wpa_printf(MSG_ERROR, "Invalid length");
		return -1;
	}

	if (data) {
		len = wpabuf_len(data);
		if (len < MIN_VENDOR_ELEM_LEN) {
			wpa_printf(MSG_ERROR,
				   "Invalid vendor element length, Min:%d",
				   MIN_VENDOR_ELEM_LEN);
			return -1;
		}

		buf = wpabuf_dup(data);
		if (!buf) {
			wpa_printf(MSG_ERROR, "Failed to duplicate wpabuf");
			return -1;
		}
	} else {
		len /= 2;
		if (len < MIN_VENDOR_ELEM_LEN) {
			wpa_printf(MSG_ERROR,
				   "Invalid vendor element length, Min:%d", MIN_VENDOR_ELEM_LEN);
			return -1;
		}

		buf = wpabuf_alloc(len);
		if (!buf) {
			wpa_printf(MSG_ERROR, "Memory allocation failed");
			return -1;
		}

		if (hexstr2bin(val, wpabuf_put(buf, len), len)) {
			wpa_printf(MSG_ERROR, "Invalid hexa string");
			wpabuf_free(buf);
			return -1;
		}
	}

	if (os_strcmp(cmd, "vendor_elements_add") == 0) {
		ret = hostapd_handle_vendor_elements_add(hapd, conf, buf, is_bcn_update_needed);
		if (ret) {
			wpa_printf(MSG_ERROR, "Failed to add vendor elements");
			wpabuf_free(buf);
			return -1;
		}
	} else if (os_strcmp(cmd, "vendor_elements_remove") == 0) {
		ret = hostapd_handle_vendor_elements_remove(conf, buf);
		if (ret) {
			wpa_printf(MSG_ERROR, "Failed to remove vendor elements");
			wpabuf_free(buf);
			return -1;
		}
	} else {
		wpa_printf(MSG_ERROR, "Invalid vendor command");
		wpabuf_free(buf);
		return -1;
	}

	if (is_bcn_update_needed) {
		if (hapd->beacon_set_done && hapd->started &&
		    ieee802_11_set_beacon(hapd) < 0) {
			wpa_printf(MSG_ERROR, "Failed to update beacons with vendor elements");
			if (os_strcmp(cmd, "vendor_elements_add") == 0) {
				if (hostapd_handle_vendor_elements_remove(conf, buf) < 0)
					wpa_printf(MSG_ERROR,
						   "Rollback: failed to remove vendor elements");
			} else if (os_strcmp(cmd, "vendor_elements_remove") == 0) {
				if (hostapd_handle_vendor_elements_add(hapd, conf, buf, is_bcn_update_needed) < 0)
					wpa_printf(MSG_ERROR,
						   "Rollback: failed to add vendor elements");
			}
			wpabuf_free(buf);
			return -1;
		}
	}
	wpabuf_free(buf);
	return 0;
}


void hostapd_get_oper_chan_info_of_bss(struct hostapd_data *hapd,
				       enum oper_chan_width *width,
				       u8 *seg0, u8 *seg1)
{
	*width = hostapd_get_oper_chwidth(hapd->iconf);
	*seg0 = hostapd_get_oper_centr_freq_seg0_idx(hapd->iconf);
	*seg1 = hostapd_get_oper_centr_freq_seg1_idx(hapd->iconf);

#ifdef CONFIG_IEEE80211BE
	/* Re-derive the legacy channel width for EHT disabled BSS on an EHT
	 * capable interface for punctured channel / 320 MHz bandwidth case.
	 */
	if (hapd->iconf->ieee80211be && !hostapd_is_eht_enabled(hapd)) {
		u16 punct_bitmap = hostapd_get_punct_bitmap(hapd);

		if (punct_bitmap)
			punct_update_legacy_bw(punct_bitmap,
					       hapd->iconf->channel,
					       width, seg0, seg1);

		if (*width == CONF_OPER_CHWIDTH_320MHZ)
			*width = CONF_OPER_CHWIDTH_160MHZ;
	}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
			hostapd_get_oper_info_of_repurposed_bss_extn(
					hapd, width, seg0, seg1);
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
}


enum oper_chan_width
hostapd_get_oper_chan_width_of_bss(struct hostapd_data *hapd)
{
	enum oper_chan_width width;
	u8 seg0, seg1;

	hostapd_get_oper_chan_info_of_bss(hapd, &width, &seg0, &seg1);

	return width;
}


u8 hostapd_get_oper_class_of_bss(struct hostapd_data *hapd)
{
	u8 op_class = hapd->iconf->op_class;

#ifdef CONFIG_IEEE80211BE
	/* For EHT disabled BSS, re-derive the legacy operating class from
	 * operating frequency parameters as the bandwidth that it advertises
	 * in VHT/HE Operation element can be lower than EHT enabled BSS of the
	 * interface.
	 *
	 * If EHT is not disabled on the BSS, interface's operating class is
	 * returned without recomputation against operating frequency parameters
	 * as operating channel information is expected to be aligned with the
	 * interface's operating class configuration.
	 */
	if (hapd->iconf->ieee80211be && !hostapd_is_eht_enabled(hapd) &&
	    hapd->iface->freq) {
		enum oper_chan_width bss_chwidth, iface_chwidth;
		u8 seg0, seg1;
		u8 chan;

		iface_chwidth = hostapd_get_oper_chwidth(hapd->iconf);
		hostapd_get_oper_chan_info_of_bss(hapd, &bss_chwidth,
						  &seg0, &seg1);

		/* if BSS channel width is different from interface channel
		 * width, re-compute the operating class as per the BSS channel
		 * width.
		 */
		if (bss_chwidth != iface_chwidth) {
			int secondary_channel = hapd->iconf->secondary_channel;

			/* Check if legacy bandwidth is downgraded to 20MHz
			 * and derive operating class accordingly
			 */
			if (seg0 == hapd->iconf->channel &&
			    bss_chwidth == CONF_OPER_CHWIDTH_USE_HT)
				secondary_channel = 0;

			if (ieee80211_freq_to_channel_ext(
						hapd->iface->freq,
						secondary_channel,
						bss_chwidth, &op_class,
						&chan) == NUM_HOSTAPD_MODES ||
			    chan != hapd->iconf->channel)
				return hapd->iconf->op_class;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return op_class;
}

struct hostapd_data * hostapd_mbssid_get_bss(struct hostapd_data *hapd, size_t i)
{
	struct hostapd_data *bss, *tx_hapd;

	tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	if (!tx_hapd)
		return NULL;

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
		bss = hostapd_get_multi_group_bss(tx_hapd->mbssid_group, i);
	else
		bss = hapd->iface->bss[i];

	if (!bss || !bss->conf)
		return NULL;

	return bss;
}
