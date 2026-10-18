/*
 * wpa_supplicant / ubus support
 * Copyright (c) 2018, Daniel Golle <daniel@makrotopia.org>
 * Copyright (c) 2013, Felix Fietkau <nbd@nbd.name>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/wpabuf.h"
#include "common/ieee802_11_defs.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"
#include "wps_supplicant.h"
#include "ubus.h"

static struct ubus_context *ctx;
static struct blob_buf b;
static int ctx_ref;

static inline struct wpa_global *get_wpa_global_from_object(struct ubus_object *obj)
{
	return container_of(obj, struct wpa_global, ubus_global);
}

static inline struct wpa_supplicant *get_wpas_from_object(struct ubus_object *obj)
{
	return container_of(obj, struct wpa_supplicant, ubus.obj);
}

static void ubus_reconnect_timeout(void *eloop_data, void *user_ctx)
{
	if (ubus_reconnect(ctx, NULL)) {
		eloop_register_timeout(1, 0, ubus_reconnect_timeout, ctx, NULL);
		return;
	}

	ubus_add_uloop(ctx);
}

static void wpas_ubus_connection_lost(struct ubus_context *ctx)
{
	uloop_fd_delete(&ctx->sock);
	eloop_register_timeout(1, 0, ubus_reconnect_timeout, ctx, NULL);
}

static bool wpas_ubus_init(void)
{
	if (ctx)
		return true;

	eloop_add_uloop();
	ctx = ubus_connect(NULL);
	if (!ctx)
		return false;

	ctx->connection_lost = wpas_ubus_connection_lost;
	ubus_add_uloop(ctx);

	return true;
}

static void wpas_ubus_ref_inc(void)
{
	ctx_ref++;
}

static void wpas_ubus_ref_dec(void)
{
	ctx_ref--;
	if (!ctx)
		return;

	if (ctx_ref)
		return;

	uloop_fd_delete(&ctx->sock);
	ubus_free(ctx);
	ctx = NULL;
}

enum {
	HOSTAPD_PHY_AP_STATUS_RUNNING,
	__HOSTAPD_PHY_AP_STATUS_MAX,
};

static const struct blobmsg_policy hostapd_phy_ap_status_policy[] = {
	[HOSTAPD_PHY_AP_STATUS_RUNNING] = { "running", BLOBMSG_TYPE_BOOL },
};

struct wpas_hostapd_phy_ap_status {
	bool valid;
	bool running;
};

static void
wpas_hostapd_phy_ap_status_cb(struct ubus_request *req, int type,
			      struct blob_attr *msg)
{
	struct wpas_hostapd_phy_ap_status *status = req->priv;
	struct blob_attr *tb[__HOSTAPD_PHY_AP_STATUS_MAX];

	if (!status)
		return;

	status->valid = false;
	status->running = false;

	if (!msg)
		return;

	blobmsg_parse(hostapd_phy_ap_status_policy,
		      __HOSTAPD_PHY_AP_STATUS_MAX, tb, blob_data(msg),
		      blob_len(msg));

	if (tb[HOSTAPD_PHY_AP_STATUS_RUNNING])
		status->running =
			blobmsg_get_bool(tb[HOSTAPD_PHY_AP_STATUS_RUNNING]);

	status->valid = true;
}

static int
wpas_ubus_get_phy_radio(struct wpa_supplicant *wpa_s, const char **phy,
			int *radio)
{
	int freq = 0;

	if (!wpa_s || !phy || !radio)
		return -1;

	*phy = wpa_driver_get_radio_name(wpa_s);
	if ((!*phy || !(*phy)[0]) && wpa_s->radio && wpa_s->radio->name[0])
		*phy = wpa_s->radio->name;
	if (!*phy || !(*phy)[0])
		return -1;

	if (wpa_s->current_ssid && wpa_s->current_ssid->frequency > 0)
		freq = wpa_s->current_ssid->frequency;
	if (!freq && wpa_s->assoc_freq > 0)
		freq = wpa_s->assoc_freq;

	if (freq > 0) {
		*radio = wpa_get_hw_idx_by_freq(wpa_s, freq);
		if (*radio >= 0)
			return 0;
	}

	if (wpa_s->num_multi_hws <= 1) {
		*radio = 0;
		return 0;
	}

	return -1;
}

int wpas_ubus_has_hostapd_iface_same_radio(struct wpa_supplicant *wpa_s)
{
	struct wpas_hostapd_phy_ap_status status;
	const char *phy = NULL;
	bool temporary_ctx = false;
	uint32_t id;
	int radio;
	int ret;

	if (!ctx)
		temporary_ctx = true;

	if (!wpas_ubus_init()) {
		wpa_printf(MSG_DEBUG,
			   "ubus: failed to connect while checking hostapd AP presence");
		return -1;
	}

	ret = wpas_ubus_get_phy_radio(wpa_s, &phy, &radio);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG,
			   "ubus: could not resolve phy/radio for %s",
			   wpa_s ? wpa_s->ifname : "unknown");
		ret = -1;
		goto out;
	}

	ret = ubus_lookup_id(ctx, "hostapd", &id);
	if (ret == UBUS_STATUS_NOT_FOUND) {
		wpa_printf(MSG_DEBUG,
			   "ubus: hostapd root object not present for phy %s radio %d",
			   phy, radio);
		ret = 0;
		goto out;
	}
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "ubus: failed to look up hostapd root object for phy %s radio %d (ret=%d)",
			   phy, radio, ret);
		ret = -1;
		goto out;
	}

	os_memset(&status, 0, sizeof(status));
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "phy", phy);
	blobmsg_add_u32(&b, "radio", radio);

	ret = ubus_invoke(ctx, id, "phy_ap_status", b.head,
			  wpas_hostapd_phy_ap_status_cb, &status, 3000);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "ubus: phy_ap_status failed for phy %s radio %d (ret=%d)",
			   phy, radio, ret);
		ret = -1;
		goto out;
	}

	if (!status.valid) {
		wpa_printf(MSG_DEBUG,
			   "ubus: phy_ap_status returned no data for phy %s radio %d",
			   phy, radio);
		ret = -1;
		goto out;
	}

	wpa_printf(MSG_DEBUG,
		   "ubus: phy %s radio %d hostapd AP running=%d",
		   phy, radio, status.running);

	ret = status.running ? 1 : 0;

out:
	if (temporary_ctx && !ctx_ref && ctx) {
		uloop_fd_delete(&ctx->sock);
		ubus_free(ctx);
		ctx = NULL;
	}

	return ret;
}

static int
wpas_bss_get_features(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct wpa_supplicant *wpa_s = get_wpas_from_object(obj);

	blob_buf_init(&b, 0);
	blobmsg_add_u8(&b, "ht_supported", ht_supported(wpa_s->hw.modes));
	blobmsg_add_u8(&b, "vht_supported", vht_supported(wpa_s->hw.modes));
	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
wpas_bss_reload(struct ubus_context *ctx, struct ubus_object *obj,
		struct ubus_request_data *req, const char *method,
		struct blob_attr *msg)
{
	struct wpa_supplicant *wpa_s = get_wpas_from_object(obj);

	if (wpa_supplicant_reload_configuration(wpa_s))
		return UBUS_STATUS_UNKNOWN_ERROR;
	else
		return 0;
}

#ifdef CONFIG_WPS
enum {
	WPS_START_MULTI_AP,
	__WPS_START_MAX
};

static const struct blobmsg_policy wps_start_policy[] = {
	[WPS_START_MULTI_AP] = { "multi_ap", BLOBMSG_TYPE_BOOL },
};

static int
wpas_bss_wps_start(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int rc;
	struct wpa_supplicant *wpa_s = get_wpas_from_object(obj);
	struct blob_attr *tb[__WPS_START_MAX];
	int multi_ap = 0;

	blobmsg_parse(wps_start_policy, __WPS_START_MAX, tb, blobmsg_data(msg), blobmsg_data_len(msg));

	if (tb[WPS_START_MULTI_AP])
		multi_ap = blobmsg_get_bool(tb[WPS_START_MULTI_AP]);

	rc = wpas_wps_start_pbc(wpa_s, NULL, 0, multi_ap);

	if (rc != 0)
		return UBUS_STATUS_NOT_SUPPORTED;

	return 0;
}

static int
wpas_bss_wps_cancel(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int rc;
	struct wpa_supplicant *wpa_s = get_wpas_from_object(obj);

	rc = wpas_wps_cancel(wpa_s);

	if (rc != 0)
		return UBUS_STATUS_NOT_SUPPORTED;

	return 0;
}
#endif

static const struct ubus_method bss_methods[] = {
	UBUS_METHOD_NOARG("reload", wpas_bss_reload),
	UBUS_METHOD_NOARG("get_features", wpas_bss_get_features),
#ifdef CONFIG_WPS
	UBUS_METHOD_NOARG("wps_start", wpas_bss_wps_start),
	UBUS_METHOD_NOARG("wps_cancel", wpas_bss_wps_cancel),
#endif
};

static struct ubus_object_type bss_object_type =
	UBUS_OBJECT_TYPE("wpas_bss", bss_methods);

void wpas_ubus_add_bss(struct wpa_supplicant *wpa_s)
{
	struct ubus_object *obj = &wpa_s->ubus.obj;
	char *name;

	if (!wpas_ubus_init())
		return;

	if (asprintf(&name, "wpa_supplicant.%s", wpa_s->ifname) < 0)
		return;

	obj->name = name;
	obj->type = &bss_object_type;
	obj->methods = bss_object_type.methods;
	obj->n_methods = bss_object_type.n_methods;
	ubus_add_object(ctx, obj);
	wpas_ubus_ref_inc();
}

void wpas_ubus_free_bss(struct wpa_supplicant *wpa_s)
{
	struct ubus_object *obj = &wpa_s->ubus.obj;
	char *name = (char *) obj->name;

	if (!ctx)
		return;

	if (obj->id) {
		ubus_remove_object(ctx, obj);
		wpas_ubus_ref_dec();
	}

	free(name);
}

#ifdef CONFIG_WPS
void wpas_ubus_notify(struct wpa_supplicant *wpa_s, const struct wps_credential *cred)
{
	u16 auth_type;
	char *ifname, *encryption, *ssid, *key;
	size_t ifname_len;

	if (!cred)
		return;

	auth_type = cred->auth_type;

	if (auth_type == (WPS_AUTH_WPAPSK | WPS_AUTH_WPA2PSK))
		auth_type = WPS_AUTH_WPA2PSK;

	if (auth_type != WPS_AUTH_OPEN &&
	    auth_type != WPS_AUTH_WPAPSK &&
	    auth_type != WPS_AUTH_WPA2PSK) {
		wpa_printf(MSG_DEBUG, "WPS: Ignored credentials for "
			   "unsupported authentication type 0x%x",
			   auth_type);
		return;
	}

	if (auth_type == WPS_AUTH_WPAPSK || auth_type == WPS_AUTH_WPA2PSK) {
		if (cred->key_len < 8 || cred->key_len > 2 * PMK_LEN) {
			wpa_printf(MSG_ERROR, "WPS: Reject PSK credential with "
				   "invalid Network Key length %lu",
				   (unsigned long) cred->key_len);
			return;
		}
	}

	blob_buf_init(&b, 0);

	ifname_len = strlen(wpa_s->ifname);
	ifname = blobmsg_alloc_string_buffer(&b, "ifname", ifname_len + 1);
	memcpy(ifname, wpa_s->ifname, ifname_len + 1);
	ifname[ifname_len] = '\0';
	blobmsg_add_string_buffer(&b);

	switch (auth_type) {
		case WPS_AUTH_WPA2PSK:
			encryption = "sae-mixed";
			break;
		case WPS_AUTH_WPAPSK:
			encryption = "psk";
			break;
		default:
			encryption = "none";
			break;
	}

	blobmsg_add_string(&b, "encryption", encryption);

	ssid = blobmsg_alloc_string_buffer(&b, "ssid", cred->ssid_len + 1);
	memcpy(ssid, cred->ssid, cred->ssid_len);
	ssid[cred->ssid_len] = '\0';
	blobmsg_add_string_buffer(&b);

	if (cred->key_len > 0) {
		key = blobmsg_alloc_string_buffer(&b, "key", cred->key_len + 1);
		memcpy(key, cred->key, cred->key_len);
		key[cred->key_len] = '\0';
		blobmsg_add_string_buffer(&b);
	}

//	ubus_notify(ctx, &wpa_s->ubus.obj, "wps_credentials", b.head, -1);
	ubus_send_event(ctx, "wps_credentials", b.head);
}
#endif /* CONFIG_WPS */
