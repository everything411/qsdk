/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/bitfield.h"
#include "common/eapol_common.h"
#include "common/wpa_ctrl.h"
#include "ap/hostapd.h"
#include "ap/sta_info.h"
#include "ap/ieee802_11.h"
#include "ap/ieee802_1x.h"
#include "ap/ap_drv_ops.h"
#include "ap/wpa_auth.h"
#include "ap/wpa_auth_i.h"
#include "ap/pmksa_cache_auth.h"
#include "ap/beacon.h"
#include "ap/ap_mlme.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "eapol_auth/eapol_auth_sm_i.h"
#include "eap_server/eap.h"
#ifdef CONFIG_MQTT
#include "hostapd_if_mqtt.h"
#endif

#ifdef HOSTAPD_EXTERNAL_PLUGIN
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/hostapd_external_interface.h"
#endif /* CONFIG_QCN_EXTN */
#endif
#include "hostapd_if.h"
#include "radius/radius.h"
#ifdef HOSTAPD_EXTERNAL_PLUGIN
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/hostapd_if_eloop.h"
#endif /* CONFIG_QCN_EXTN */
#endif
#include <stdlib.h>


#ifdef CONFIG_QCN_EXTN

/*
 * State and declarations for ASYNC dispatch system
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "utils/eloop.h"

/*
 * Max MPDU size excluding FCS for management frames
 */
#ifndef IEEE80211_MAX_MGMT_LEN_NO_FCS
#define IEEE80211_MAX_MGMT_LEN_NO_FCS 2300
#endif

#define  HOSTAPD_INVALID_RSSI -128
#define  RADIUS_MSG_TYPE_INVALID ((uint32_t)-1)

static struct hapd_interfaces *hostapd_if_ifaces;

/*
 * Per-interface/MLD frame registration table, reference counted
 * for MLD sharing
 */
struct frame_reg_table {
	int ref_count;
	/*
	 * Per-mgmt registration
	 */
	enum hostapd_if_frame_policy mgmt[HOSTAPD_IF_FRAME_TYPE_MAX];

	/*
	 * Per-action registration
	 */
	enum hostapd_if_frame_policy action[HOSTAPD_IF_FRAME_TYPE_ACTION_MAX];
	/*
	 * Event registration bitfield
	 */
	struct bitfield *event_registration;
};

/*
 * Query API: Check if event notification is enabled for a specific
 * event type
 */
static bool hostapd_if_is_event_registered(struct hostapd_data *hapd,
				    enum hostapd_if_event_type type)
{
	struct frame_reg_table *table;
	struct bitfield *event_registration;

	if (!hapd || (type >= HOSTAPD_IF_EVENT_MAX))
		return false;

	table = (struct frame_reg_table *) hapd->hostapd_if_data;
	if(!table)
		return false;

	event_registration = table->event_registration;
	if (!event_registration)
		return false;

	return bitfield_is_set(event_registration, type);
}



#ifdef HOSTAPD_EXTERNAL_PLUGIN
static struct hostapd_external_app_object *hostapd_if_plugin;
#define HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt)			\
do {									\
	if (hostapd_if_plugin &&					\
	    hostapd_if_plugin->notify_event)				\
		hostapd_if_plugin->notify_event(hapd, &evt);		\
} while (0)

/*
 * inbound error event is triggered for all async inbound calls
 * those are not successfully carried out
 */
static void
__inbound_error_event(struct hostapd_data *hapd, const uint8_t *sta_mac,
		      enum HOSTAPD_IF_INBOUND_ERROR type,
		      const char *func,
		      int line_num)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(hapd,
				HOSTAPD_IF_EVENT_INBOUND_CALL_ERROR))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_INBOUND_CALL_ERROR;
	os_strlcpy((char *) evt.ifname, hapd->conf->iface, sizeof(evt.ifname));
	if (sta_mac)
		os_memcpy(evt.sta_mac, sta_mac, sizeof(evt.sta_mac));
	evt.data.inbound_call_error.type = type;
	evt.data.inbound_call_error.func = func;
	evt.data.inbound_call_error.line_num = line_num;

	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}
#else
#define HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt)
static void
__inbound_error_event(struct hostapd_data *hapd, const uint8_t *sta_mac,
		      const char *func,
		      int line_num)
{
}
#endif

struct hostapd_data *
__hostapd_get_link_iface(const char *ifname, int link_id)
{
	size_t i, j;

	for (i = 0; i < hostapd_if_ifaces->count; i++) {
		struct hostapd_iface *iface = hostapd_if_ifaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *hapd = iface->bss[j];

			if (os_strcmp(ifname, hapd->conf->iface) != 0)
				continue;

			if (link_id < 0)
				return hapd;

			if (hapd->mld_link_id != link_id)
				continue;

			return hapd;
		}
	}

	return NULL;
}


static struct sta_info *__get_sta(const char *ifname,
				       const uint8_t *sta_addr,
				       int link_id,
				       bool assoc_link_required,
				       struct hostapd_data **hapd)
{
	struct hostapd_iface *iface;
        struct hostapd_data *bss;
        unsigned int i, j;
	struct sta_info *sta;
	 *hapd = NULL;

        for (i = 0; i < hostapd_if_ifaces->count; i++) {
                iface = hostapd_if_ifaces->iface[i];
                if (!iface)
                        continue;

                for (j = 0; j < iface->num_bss; j++) {
                        bss = iface->bss[j];

			if (os_strcmp(ifname, bss->conf->iface) != 0)
				continue;

			if ((bss->mld_link_id != link_id) && (link_id > 0))
				continue;

			sta = ap_get_sta(bss, sta_addr);
			if (!sta)
				continue;

			if ((!sta->mld_info.mld_sta) ||
			    !assoc_link_required ||
			    (sta->mld_assoc_link_id == bss->mld_link_id)) {
				*hapd = bss;
				return sta;
			}
                }
        }
	return NULL;
}

/*
 * Southbound: plugin call to resume EAPOL M3 given ifname and STA MAC
 * In case the STA is a 11be STA, the input mac has to be MLD mac
 */
void __hostapd_if_trigger_eapol_m3(char *ifname, uint8_t *sta_mac)
{
	struct sta_info *sta;
	struct hostapd_data *hapd;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR "\n",
		   __func__, ifname, MAC2STR(sta_mac));

	sta = __get_sta(ifname, sta_mac, -1, true, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
				       HOSTAPD_IF_TRIGGER_EAPOL_M3_ERROR,
				       __func__, __LINE__);

		wpa_printf(MSG_ERROR, "%s: ERROR! No STA found with "MACSTR"\n",
				__func__, MAC2STR(sta_mac));
		goto __hostapd_if_trigger_eapol_m3_exit;
	}

	wpa_auth_trigger_m3(sta->wpa_sm);
__hostapd_if_trigger_eapol_m3_exit:
	return;
}

/*
 * Refactored TX path entry for OPEN authentication response.
 * Implemented in core hostapd; this file invokes it from auth_response.
 */
static int __send_open_auth_response(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     struct hostapd_if_frame_ctx *ctx)
{
	const u8 *dst = ctx->data.auth_resp.sta_assoc_link_mac;
	int reply_res;

	if (ctx->status_code == WLAN_STATUS_SUCCESS) {
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = WLAN_AUTH_OPEN;
		mlme_authenticate_indication(hapd, sta);
	}

	reply_res = send_auth_reply(hapd, sta, dst, WLAN_AUTH_OPEN,
				    ctx->data.auth_resp.auth_transaction,
				    ctx->status_code,
				    NULL, 0, "auth-open");

	if (sta && sta->added_unassoc &&
	    (ctx->status_code != WLAN_STATUS_SUCCESS ||
	     reply_res != WLAN_STATUS_SUCCESS)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}

	return reply_res;
}

/*
 * hostapd_if_register_frame implementation:
 * plugin "southbound" callback
 */
void hostapd_if_register_frame(void *ifname_ctx,
			       struct hostapd_if_frame_category *cat,
			       enum hostapd_if_frame_policy policy)
{
	/*
	 * Find correct hapd
	 */
	struct hostapd_data *hapd = NULL;
	struct frame_reg_table *table = NULL;
	const char *ifname;
	const char *type_str;

	hapd = (struct hostapd_data *)ifname_ctx;
	if (!hapd || !cat) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL params %p %p\n", __func__, ifname_ctx, cat);
		return;
	}

	ifname = hapd->conf ? hapd->conf->iface : "unknown";
	table = (struct frame_reg_table *) hapd->hostapd_if_data;
	if (!table) {
		wpa_printf(MSG_ERROR, "%s: ERROR! table not found\n", __func__);
		return;
	}

	if(cat->type == HOSTAPD_IF_FRAME_TYPE_ACTION) {
		type_str = hostapd_if_action_frame_type_string(cat->u.action_type);
		if (policy == HOSTAPD_IF_FRAME_INVOKE) {
			wpa_printf(MSG_ERROR, "%s: ERROR! policy invalid for "
				   "action frames\n", __func__);
			return;
		}
		if (cat->u.action_type >= HOSTAPD_IF_FRAME_TYPE_ACTION_MAX) {
			wpa_printf(MSG_ERROR, "%s: ERROR! action type invalid %s (%d)\n",
				   __func__, type_str, cat->u.action_type);
			return;
		}
		wpa_printf(MSG_DEBUG, "%s: registering action frame %s (%d) policy=%d ifname=%s\n",
			   __func__, type_str, cat->u.action_type, policy, ifname);
		table->action[cat->u.action_type] = policy;
		return;
	} else if (cat->type >= HOSTAPD_IF_FRAME_TYPE_MAX) {
		wpa_printf(MSG_ERROR, "%s: ERROR! type invalid %d", __func__, cat->type);
		return;
	}
	table->mgmt[cat->type] = policy;
}

/*
 * hostapd_if_register_event implementation:
 * plugin "southbound" callback
 */
void hostapd_if_register_event(void *ifname_ctx,
			       enum hostapd_if_event_type type,
			       bool set)
{
	struct hostapd_data *hapd = NULL;
	struct frame_reg_table *table = NULL;

	if (!ifname_ctx || type >= HOSTAPD_IF_EVENT_MAX) {
		wpa_printf(MSG_ERROR, "%s! ERROR! invalid params %p %d", __func__,
				   ifname_ctx, type);
		return;
	}

	hapd = (struct hostapd_data *)ifname_ctx;
	table = (struct frame_reg_table *) hapd->hostapd_if_data;
	if (!table || !table->event_registration) {
		wpa_printf(MSG_ERROR, "%s: ERROR! table not found %p\n", __func__,
				   table);
		return;
	}

	if (set)
		bitfield_set(table->event_registration, type);
	else
		bitfield_clear(table->event_registration, type);

	wpa_printf(MSG_DEBUG,
			   "hostapd_if: Event registration enabled for event type=%u",
			   type);
}

#ifdef HOSTAPD_EXTERNAL_PLUGIN
void hostapd_if_plugin_init(void *);
void hostapd_if_plugin_deinit(void);
#endif

const bool global_plugin_enable = false;
bool hostapd_if_plugin_enable = global_plugin_enable;
#ifdef CONFIG_MQTT
bool hostapd_if_mqtt_enable = true;
#endif /* CONFIG_MQTT */
/*
 * Call this once at startup (from hostapd_if_init)
 */
int hostapd_if_init(struct hapd_interfaces *interfaces, bool plugin_enable,
		    enum hostapd_if_eloop_type eloop_type)
{
	wpa_printf(MSG_ERROR, "%s", __func__);
	hostapd_if_ifaces = interfaces;

#ifdef CONFIG_QCN_EXTN
#ifdef HOSTAPD_EXTERNAL_PLUGIN
#ifdef HOSTAPD_EXTERNAL_PLUGIN_TESTAPP
	hostapd_if_plugin_enable = (plugin_enable || global_plugin_enable);
	if (hostapd_if_plugin_enable)
		hostapd_if_plugin_init(interfaces);
#endif
#ifdef CONFIG_MQTT
	if (!hostapd_if_plugin_enable && hostapd_if_mqtt_enable)
		hostapd_if_mqtt_init(interfaces);
#endif
	if (hostapd_if_eloop_init(eloop_type) < 0)
		return -1;
#endif
#endif

	return 0;
}

#ifdef CONFIG_QCN_EXTN
#ifdef HOSTAPD_EXTERNAL_PLUGIN
void hostapd_if_eloop_deinit(void);
#endif
#endif

int hostapd_if_deinit(void)
{
#ifdef CONFIG_QCN_EXTN
#ifdef HOSTAPD_EXTERNAL_PLUGIN
	hostapd_if_eloop_deinit();
#ifdef HOSTAPD_EXTERNAL_PLUGIN_TESTAPP
	if (hostapd_if_plugin_enable)
		hostapd_if_plugin_deinit();
#endif
#endif
#endif
	return 0;
}

static
struct frame_reg_table *__get_shared_mld_table(struct hostapd_data *hapd)
{
	struct hostapd_data *hapd_partner;

	if (!hapd->conf->mld_ap || !hapd->mld)
		return NULL;

	for_each_mld_link(hapd_partner, hapd) {

		if (!hapd_partner->hostapd_if_data)
			continue;
		return hapd_partner->hostapd_if_data;
	}
	return NULL;
}

static void __free_frame_reg_table(void *obj)
{
	bitfield_free(((struct frame_reg_table *) obj)->event_registration);
	os_free(obj);
}

#ifdef HOSTAPD_EXTERNAL_PLUGIN
static void __clear_shared_mld_table(struct hostapd_data *hapd)
{
	struct hostapd_data *hapd_partner;

	if (!hapd->conf->mld_ap || !hapd->mld || !hapd->hostapd_if_data)
		return;

	__free_frame_reg_table(hapd->hostapd_if_data);
	/*
	 * Clear self pointer separately in case of Legacy BSS
	 */
	hapd->hostapd_if_data = NULL;
	for_each_mld_link(hapd_partner, hapd)
		hapd_partner->hostapd_if_data = NULL;
}
#endif /* HOSTAPD_EXTERNAL_PLUGIN */

int hostapd_if_interface_create(struct hostapd_data *hapd)
{
	struct frame_reg_table *table = NULL;

	if (!hapd->conf->external_plugin_enable)
		return 0;

	if (hapd->hostapd_if_data)
		return 0;

	wpa_printf(MSG_DEBUG, "%s:%s link-id:%d", __func__, hapd->conf->iface,
		hapd->mld_link_id);

	table = __get_shared_mld_table(hapd);
	if (table) {

		wpa_printf(MSG_DEBUG, "%s: sharing table for %s link-id:%d\n",
			   __func__, hapd->conf->iface, hapd->mld_link_id);

		hapd->hostapd_if_data = (void *) table;
		++table->ref_count;
		goto hostapd_if_interface_create_plugin_call;
	}

	table = os_zalloc(sizeof(*table));
	if (!table) {
		wpa_printf(MSG_ERROR, "%s! ERROR!! TABLE allocation failed\n",
			__func__);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s:%s Allocating table link-id:%d\n", __func__,
		   hapd->conf->iface, hapd->mld_link_id);
	table->ref_count = 1;
	/*
	 * Allocate bitfield for event registration tracking
	 */
	table->event_registration = bitfield_alloc(HOSTAPD_IF_EVENT_MAX);
	if (!table->event_registration) {
		wpa_printf(MSG_ERROR, "%s! ERROR! Event reg table allocation failed\n",
			__func__);
		free(table);
		return -1;
	}

	hapd->hostapd_if_data = (void *) table;
hostapd_if_interface_create_plugin_call:
#ifdef HOSTAPD_EXTERNAL_PLUGIN
	/*
	 * plugin->interface_init → interface_create
	 */
	if (!hostapd_if_plugin || !hostapd_if_plugin->interface_create)
		return 0;

	if (hostapd_if_plugin->interface_create(hapd->conf->iface, hapd))
		/*
		 * The external client is unreachable, so the shared
		 * table is no longer valid for any MLD partner.
		 */
		__clear_shared_mld_table(hapd);
#endif

	return 0;
}

/*
 * On per-BSS teardown: remove reference, free table only if last
 */
void hostapd_if_interface_remove(struct hostapd_data *hapd)
{
	struct frame_reg_table *table =
		(struct frame_reg_table *) hapd->hostapd_if_data;

	if (!table)
		return;

	if (--table->ref_count == 0)
		__free_frame_reg_table(table);

	hapd->hostapd_if_data = NULL;

	if (!hapd->conf)
		return;

	wpa_printf(MSG_DEBUG, "%s:%s link-id:%d", __func__, hapd->conf->iface,
		   hapd->mld_link_id);

#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (hapd->conf->external_plugin_enable &&
	    hostapd_if_plugin && hostapd_if_plugin->interface_remove)
		hostapd_if_plugin->interface_remove(hapd->conf->iface, hapd);
#endif
}

static enum hostapd_if_frame_processing_decision
__get_frame_decision(enum hostapd_if_frame_policy *policy,
		     bool invoke_supported)
{
	if ((*policy == HOSTAPD_IF_FRAME_INVOKE) && !invoke_supported)
		*policy = HOSTAPD_IF_FRAME_NOTIFY;

	switch (*policy) {
	case HOSTAPD_IF_FRAME_INVOKE:
		return HOSTAPD_IF_FRAME_PROCESSING_WAIT;
	case HOSTAPD_IF_FRAME_OFFLOAD:
		return HOSTAPD_IF_FRAME_PROCESSING_OFFLOAD;
	default:
		return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE;
	}
}

static enum hostapd_if_frame_processing_decision
hostapd_if_notify_get_policy_decision(struct hostapd_data *hapd, u16 auth_alg,
				      enum hostapd_if_frame_policy *policy,
				      enum hostapd_if_frame_reg_type frame_type)
{
	bool invoke_supported = true;
	struct frame_reg_table *table;
	enum hostapd_if_frame_processing_decision decision =
		HOSTAPD_IF_FRAME_PROCESSING_CONTINUE;

	*policy = HOSTAPD_IF_FRAME_DO_NOTHING;
	table = (struct frame_reg_table *) hapd->hostapd_if_data;
	if (!table)
		return decision;

#ifdef CONFIG_FILS
	if (auth_alg == WLAN_AUTH_FILS_SK ||
	    auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    auth_alg == WLAN_AUTH_FILS_PK)
		invoke_supported = false;
#endif /* CONFIG_FILS */

	*policy = table->mgmt[frame_type];

	wpa_printf(MSG_DEBUG, "%s: policy-before:%d invoke_supported: %d",
		   __func__, *policy, invoke_supported);
	decision = __get_frame_decision(policy, invoke_supported);
	wpa_printf(MSG_DEBUG, "%s: policy-after:%d decision: %d",
		__func__, *policy, decision);

	return decision;
}

#ifdef CONFIG_SAE
#define SAE_PLUGIN_WAIT_TIMEOUT_SEC 1

static void __sae_plugin_wait_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	if (!sta->sae)
		return;

	wpa_printf(MSG_INFO,
		   "%s: SAE plugin response timeout for " MACSTR
		   " - sending deauth and removing STA",
		   __func__, MAC2STR(sta->addr));

	hostapd_drv_sta_deauth(hapd, sta->addr,
			       WLAN_REASON_PREV_AUTH_NOT_VALID);
	wpa_printf(MSG_DEBUG,
		   "%s: " MACSTR " - deauth sent, freeing STA",
		   __func__, MAC2STR(sta->addr));
	ap_free_sta(hapd, sta);
}


void hostapd_if_sae_clear_plugin_wait(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	if (!sta->sae)
	       return;

	wpa_printf(MSG_DEBUG,
		   "%s: " MACSTR " - plugin responded, cancelling guard timer",
		   __func__, MAC2STR(sta->addr));
	sta->sae->plugin_wait = 0;
	eloop_cancel_timeout(__sae_plugin_wait_timeout, hapd, sta);
}

#endif /* CONFIG_SAE */


enum hostapd_if_frame_processing_decision
hostapd_if_notify_auth(struct hostapd_data *hapd,
		       struct sta_info *sta,
		       const uint8_t *frame,
		       uint16_t frame_len,
		       int rssi,
		       u16 status_code,
		       u16 auth_transaction,
		       u8 allow_reuse,
		       u16 auth_alg,
		       const u8 *sa)
{
	enum hostapd_if_frame_policy policy;
	enum hostapd_if_frame_processing_decision decision;
	struct hostapd_if_frame_ctx ctx_req;

	decision =
	hostapd_if_notify_get_policy_decision(hapd, auth_alg, &policy,
					      HOSTAPD_IF_FRAME_TYPE_AUTH);

	/*
	 * Initialize and populate context structure
	 */

	os_memset(&ctx_req, 0, sizeof(ctx_req));
	ctx_req.status_code = status_code;
	ctx_req.data.auth_req.rssi = rssi;
	ctx_req.data.auth_req.auth_transaction = auth_transaction;
	ctx_req.data.auth_req.allow_reuse = allow_reuse;
	ctx_req.data.auth_req.auth_alg = auth_alg;

	/*
	 * rx_link_id selection: hw_idx for non-MLD, otherwise
	 * hapd->mld_link_id
	 */
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		ctx_req.rx_link_id = hapd->mld_link_id;
	else
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iface->current_hw_info)
		ctx_req.rx_link_id = hapd->iface->current_hw_info->hw_idx;
	else
		ctx_req.rx_link_id = -1;

	/*
	 * Associated link id and per-link peer MAC
	 */

	os_memcpy(ctx_req.data.auth_req.sta_assoc_link_mac,
		  sa, ETH_ALEN);

#ifdef CONFIG_SAE
	if (policy == HOSTAPD_IF_FRAME_INVOKE && auth_alg == WLAN_AUTH_SAE &&
	    auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM) {
		decision = HOSTAPD_IF_FRAME_PROCESSING_CONTINUE;
		policy = HOSTAPD_IF_FRAME_NOTIFY;
	}
#endif /* CONFIG_SAE */

	if (policy == HOSTAPD_IF_FRAME_NOTIFY) {
#ifdef HOSTAPD_EXTERNAL_PLUGIN
		if (hostapd_if_plugin && hostapd_if_plugin->notify_auth) {
			hostapd_if_plugin->notify_auth(hapd, hapd->conf->iface,
					(u8 *)(sta ? sta->addr : sa), frame, frame_len, &ctx_req);
		}
#endif
	} else if (policy == HOSTAPD_IF_FRAME_INVOKE) {
#ifdef HOSTAPD_EXTERNAL_PLUGIN
		/*
		 * External invoke, hostapd must pause until
		 * auth_response
		 */
		if (hostapd_if_plugin &&
		    hostapd_if_plugin->invoke_auth) {
#ifdef CONFIG_SAE
			if (auth_alg == WLAN_AUTH_SAE && sta && sta->sae) {
				wpa_printf(MSG_DEBUG,
					   "%s: SAE " MACSTR " - arming %ds wait timer before plugin invoke",
					   __func__, MAC2STR(sta->addr),
					   SAE_PLUGIN_WAIT_TIMEOUT_SEC);
				sta->sae->plugin_wait = 1;
				eloop_register_timeout(SAE_PLUGIN_WAIT_TIMEOUT_SEC,
							0,
							__sae_plugin_wait_timeout,
							hapd, sta);
			}
#endif /* CONFIG_SAE */
			/*
			 * Pass computed context by pointer
			 */
			hostapd_if_plugin->invoke_auth(hapd->conf->iface,
					(u8 *)(sta ? sta->addr : sa), frame, frame_len, &ctx_req);
		}
#endif
	}

	return decision;
}

enum hostapd_if_frame_processing_decision
hostapd_if_frame_fwd_decision(struct hostapd_data *hapd, u16 auth_alg,
			      enum hostapd_if_frame_reg_type frame_type)
{
	enum hostapd_if_frame_policy policy;
	return hostapd_if_notify_get_policy_decision(hapd, auth_alg, &policy,
						     frame_type);
}

enum hostapd_if_frame_processing_decision
hostapd_if_notify_remote_auth(struct hostapd_data *hapd, uint8_t *sta_mac,
			      const uint8_t *ies, uint16_t ies_len,
			      uint16_t status_code, bool is_ml)
{
	enum hostapd_if_frame_policy policy;
	enum hostapd_if_frame_processing_decision decision;
	struct hostapd_if_frame_ctx ctx_req;

	decision = hostapd_if_notify_get_policy_decision(hapd, 0, &policy,
							 HOSTAPD_IF_FRAME_TYPE_REMOTE_AUTH);

	os_memset(&ctx_req, 0, sizeof(ctx_req));
	ctx_req.status_code = status_code;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		ctx_req.rx_link_id = hapd->mld_link_id;
	else
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iface->current_hw_info)
		ctx_req.rx_link_id = hapd->iface->current_hw_info->hw_idx;
	else
		ctx_req.rx_link_id = -1;

	ctx_req.data.remote_auth_req.is_ml_sta = is_ml;

#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (!hostapd_if_plugin || !hostapd_if_plugin->notify_remote_auth)
		return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE;

	if (policy == HOSTAPD_IF_FRAME_NOTIFY) {
		hostapd_if_plugin->notify_remote_auth(hapd->conf->iface,
						      sta_mac, ies, ies_len,
						      &ctx_req);
	} else if (policy == HOSTAPD_IF_FRAME_INVOKE) {
		hostapd_if_plugin->invoke_remote_auth(hapd->conf->iface,
						      sta_mac, ies, ies_len,
						      &ctx_req);
	}
#endif

	return decision;
}

int hostapd_if_pull_pmk_r1(struct hostapd_data *hapd, uint8_t *sta_mac,
			   uint8_t *pmk_r1_name, uint8_t *pmk_r1,
			   size_t *pmk_r1_len, int *pairwise,
			   int *session_timeout, const uint8_t **identity,
			   size_t *identity_len, const uint8_t **radius_cui,
			   size_t *radius_cui_len)
{
	uint8_t *hapd_if_identity, *hapd_if_radius_cui;

	if (!hostapd_if_plugin || !hostapd_if_plugin->pull_pmk_r1)
		return -1;

	hapd_if_identity = os_zalloc(MAX_RADIUS_CUI_LEN);
	hapd_if_radius_cui = os_zalloc(MAX_RADIUS_CUI_LEN);

	if (!hapd_if_identity || !hapd_if_radius_cui) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory in pull_pmk_r1\n");
		goto hostapd_if_pull_pmk_r1_fail;
	}

	if (hostapd_if_plugin->pull_pmk_r1(hapd->conf->iface, sta_mac,pmk_r1_name, pmk_r1, pmk_r1_len,
				       pairwise,
				       session_timeout, hapd_if_identity,
				       identity_len, hapd_if_radius_cui,
				       radius_cui_len)) {
		goto hostapd_if_pull_pmk_r1_fail;
	}

	*identity = hapd_if_identity;
	*radius_cui = hapd_if_radius_cui;
	return 0;

hostapd_if_pull_pmk_r1_fail:
	if (hapd_if_identity)
		os_free(hapd_if_identity);

	if (hapd_if_radius_cui)
		os_free(hapd_if_radius_cui);

	return -1;
}

int hostapd_if_pull_pmk(struct hostapd_data *hapd, uint8_t *sta_mac,
			uint8_t *pmk, size_t *pmk_len, uint8_t *pmkid,
			int *session_timeout)
{
	if ((!hostapd_if_plugin) || (!hostapd_if_plugin->pull_pmk))
		return -1;

	return hostapd_if_plugin->pull_pmk(hapd->conf->iface, sta_mac, pmk,
					   pmk_len, pmkid, session_timeout);
}

/*
 * Notify/invoke external application for Association and return
 * processing decision.
 *
 * Policy:
 *  - DO_NOTHING: CONTINUE (no plugin call)
 *  - NOTIFY: CONTINUE (+ plugin->notify_assoc)
 *  - INVOKE: WAIT (+ plugin->invoke_assoc)
 */
enum hostapd_if_frame_processing_decision
hostapd_if_notify_assoc(struct hostapd_data *hapd,
			struct sta_info *sta,
			const uint8_t *frame,
			uint16_t frame_len,
			u16 status_code,
			int is_reassoc,
			int rssi,
			bool set_beacon,
			const u8 *sa)
{
	bool invoke_supported = true;
	struct frame_reg_table *table;
	enum hostapd_if_frame_policy policy =
		HOSTAPD_IF_FRAME_DO_NOTHING;
	enum hostapd_if_frame_processing_decision decision =
		HOSTAPD_IF_FRAME_PROCESSING_CONTINUE;
	struct hostapd_if_frame_ctx ctx_req;
	int i;

	table =
		(struct frame_reg_table *) hapd->hostapd_if_data;
	if (!table)
		return decision;

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK)
		invoke_supported = false;
#endif /* CONFIG_FILS */

	policy = table->mgmt[HOSTAPD_IF_FRAME_TYPE_ASSOC];

	/*
	 * Initialize and populate context structure
	 */
	os_memset(&ctx_req, 0, sizeof(ctx_req));
	ctx_req.status_code = status_code;
	ctx_req.data.assoc_req.is_reassoc = is_reassoc;
	ctx_req.data.assoc_req.rssi = rssi;

	/*
	 * Prepare context (compute rx_link_id and MLD fields)
	 */

	/*
	 * rx_link_id: -1 for non-MLD, else current BSS link id
	 */
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		ctx_req.rx_link_id = hapd->mld_link_id;
	else
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iface->current_hw_info)
		ctx_req.rx_link_id = hapd->iface->current_hw_info->hw_idx;
	else
		ctx_req.rx_link_id = -1;

	/*
	 * Fill assoc_req MLD topology if running as MLD AP
	 */
	os_memcpy(ctx_req.data.assoc_req.sta_assoc_link_mac, sa, ETH_ALEN);

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap) {


		ctx_req.data.assoc_req.valid_link_bitmap = 0;

		for (i = 0; i < MAX_MLO_LINKS; i++) {
			if (i < MAX_NUM_MLD_LINKS &&
			    sta->mld_info.links[i].valid) {
				ctx_req.data.assoc_req.valid_link_bitmap |=
					(1U << i);

				os_memcpy(ctx_req.data.assoc_req.sta_link_mac[i],
					  sta->mld_info.links[i].peer_addr,
					  ETH_ALEN);
			} else {
				os_memset(
					ctx_req.data.assoc_req.sta_link_mac[i],
					0, ETH_ALEN);
			}
		}
	} else
#endif /* CONFIG_IEEE80211BE */
	{
		ctx_req.data.assoc_req.valid_link_bitmap = 0;

		for (i = 0; i < MAX_MLO_LINKS; i++)
			os_memset(ctx_req.data.assoc_req.sta_link_mac[i],
				  0, ETH_ALEN);
	}

	decision = __get_frame_decision(&policy, invoke_supported);

	if (policy == HOSTAPD_IF_FRAME_NOTIFY) {
#ifdef HOSTAPD_EXTERNAL_PLUGIN
		if (hostapd_if_plugin &&
		    hostapd_if_plugin->notify_assoc) {
			hostapd_if_plugin->notify_assoc(hapd, hapd->conf->iface,
					sta->addr, frame, frame_len, &ctx_req);
		}
#endif
	} else if (policy == HOSTAPD_IF_FRAME_INVOKE) {
		if (set_beacon)
			ieee802_11_update_beacons(hapd->iface);
#ifdef HOSTAPD_EXTERNAL_PLUGIN
		/*
		 * Pause hostapd until assoc_response arrives
		 */
		if (hostapd_if_plugin &&
		    hostapd_if_plugin->invoke_assoc) {
			hostapd_if_plugin->invoke_assoc(hapd->conf->iface,
					sta->addr, frame, frame_len,
					&ctx_req);
		}
#endif
	}

	return decision;
}

void hostapd_if_notify_disassoc(struct hostapd_data *hapd,
				struct sta_info *sta,
				const void *frame,
				size_t frame_len)
{
	struct frame_reg_table *table;
	enum hostapd_if_frame_policy policy =
		HOSTAPD_IF_FRAME_DO_NOTHING;
	struct hostapd_if_frame_ctx ctx_req;

	table =
		(struct frame_reg_table *) hapd->hostapd_if_data;

	if (!table)
		return;

	policy = table->mgmt[HOSTAPD_IF_FRAME_TYPE_DISASSOC];

	/*
	 * Initialize and populate context structure
	 */
	os_memset(&ctx_req, 0, sizeof(ctx_req));

	/*
	 * rx_link_id: -1 for non-MLD, else current BSS link id
	 */
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		ctx_req.rx_link_id = hapd->mld_link_id;
	else
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iface->current_hw_info)
		ctx_req.rx_link_id = hapd->iface->current_hw_info->hw_idx;
	else
		ctx_req.rx_link_id = -1;

	if ((policy != HOSTAPD_IF_FRAME_NOTIFY) &&
		(policy != HOSTAPD_IF_FRAME_INVOKE))
		return;

#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (hostapd_if_plugin && hostapd_if_plugin->notify_disassoc) {
		hostapd_if_plugin->notify_disassoc(hapd, hapd->conf->iface,
			sta->addr, frame, frame_len, &ctx_req);
	}
#endif
}

void hostapd_if_notify_deauth(struct hostapd_data *hapd,
			      struct sta_info *sta,
			      const void *frame,
			      size_t frame_len)
{
	struct frame_reg_table *table;
	enum hostapd_if_frame_policy policy =
		HOSTAPD_IF_FRAME_DO_NOTHING;
	struct hostapd_if_frame_ctx ctx_req;

	table =
		(struct frame_reg_table *) hapd->hostapd_if_data;

	if (!table)
		return;

	policy = table->mgmt[HOSTAPD_IF_FRAME_TYPE_DEAUTH];

	/*
	 * Initialize and populate context structure
	 */
	os_memset(&ctx_req, 0, sizeof(ctx_req));

	/*
	 * rx_link_id: -1 for non-MLD, else current BSS link id
	 */
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		ctx_req.rx_link_id = hapd->mld_link_id;
	else
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iface->current_hw_info)
		ctx_req.rx_link_id = hapd->iface->current_hw_info->hw_idx;
	else
		ctx_req.rx_link_id = -1;

	if ((policy != HOSTAPD_IF_FRAME_NOTIFY) &&
	    (policy != HOSTAPD_IF_FRAME_INVOKE))
		return;

#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (hostapd_if_plugin && hostapd_if_plugin->notify_deauth) {
		hostapd_if_plugin->notify_deauth(hapd, hapd->conf->iface, sta->addr,
			frame, frame_len, &ctx_req);
	}
#endif
}

void hostapd_if_eapol_rx(struct hostapd_data *hapd, const u8 *sa,
			 const u8 *data, u16 data_len)
{
	int link_id = hapd->iface->current_hw_info ?
		hapd->iface->current_hw_info->hw_idx : -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif
#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (hostapd_if_plugin && hostapd_if_plugin->eapol_rx) {
		wpa_printf(MSG_DEBUG, "%s: executing EAPOL RX through plugin",
			   __func__);
		hostapd_if_plugin->eapol_rx(hapd->conf->iface, link_id, sa,
					    (u8 *) data, data_len);
	}
#endif
}

static enum hostapd_if_action_frame_type
__action_key_from_mgmt(const struct ieee80211_mgmt *mgmt, size_t frame_len,
		       enum hostapd_if_action_frame_type *category)
{
	u8 action = mgmt->u.action.u.wmm_action.action_code;

	switch (mgmt->u.action.category) {
	case WLAN_ACTION_VENDOR_SPECIFIC:
	case WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED:
		*category = HOSTAPD_IF_FRAME_TYPE_ACTION_VENDOR;
		return HOSTAPD_IF_FRAME_TYPE_ACTION_VENDOR;
	case WLAN_ACTION_RADIO_MEASUREMENT:
		*category = HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO;
		switch (action) {
		case WLAN_RRM_NEIGHBOR_REPORT_REQUEST:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO_NEIGHBOUR_REQ;
		default:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO;
		}
	case WLAN_ACTION_WNM:
		*category = HOSTAPD_IF_FRAME_TYPE_ACTION_WNM;
		switch (action) {
		case WNM_BSS_TRANS_MGMT_QUERY:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_QUERY;
		case WNM_BSS_TRANS_MGMT_RESP:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_RESP;
		case WNM_DMS_REQ:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_REQ;
		case WNM_DMS_RESP:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_RESP;
		default:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WNM;
		}
	case WLAN_ACTION_WMM:
		*category = HOSTAPD_IF_FRAME_TYPE_ACTION_WMM;
		switch (action) {
		case WMM_ACTION_CODE_ADDTS_REQ:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_ADDTS_REQ;
		case WMM_ACTION_CODE_DELTS:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_DELTS;
		default:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_WMM;
		}
	case WLAN_ACTION_FT:
		*category = HOSTAPD_IF_FRAME_TYPE_ACTION_FT;
		switch (action) {
		case 1:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_FT_REQ;
		case 2:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_FT_RESP;
		default:
			return HOSTAPD_IF_FRAME_TYPE_ACTION_FT;
		}
	default:
		return HOSTAPD_IF_FRAME_TYPE_ACTION_MAX;
	}
}

enum hostapd_if_frame_processing_decision
hostapd_if_notify_action(struct hostapd_data *hapd,
			 struct sta_info *sta,
			 const struct ieee80211_mgmt *mgmt,
			 size_t frame_len, int rssi)
{
	struct frame_reg_table *table;
	enum hostapd_if_action_frame_type key;
	enum hostapd_if_action_frame_type generic_key;
	enum hostapd_if_frame_policy policy = HOSTAPD_IF_FRAME_DO_NOTHING;
	enum hostapd_if_frame_processing_decision decision =
						HOSTAPD_IF_FRAME_PROCESSING_CONTINUE;
	int link_id;
	struct hostapd_if_frame_ctx ctx_req;

	const u8 *sta_mac = sta ? sta->addr : mgmt->sa;
	u16 plugin_frame_len = (u16) frame_len;

	table = (struct frame_reg_table *) hapd->hostapd_if_data;
	if (!table)
		return decision;

	key = __action_key_from_mgmt(mgmt, frame_len, &generic_key);
	if (key >= HOSTAPD_IF_FRAME_TYPE_ACTION_MAX)
		return decision;

	/* Look up the policy for the specific action frame subtype. */
	policy = table->action[key];
	if (policy == HOSTAPD_IF_FRAME_DO_NOTHING &&
	    generic_key < HOSTAPD_IF_FRAME_TYPE_ACTION_MAX)
		/* If no action-specific policy is configured (DO_NOTHING),
		 * fall back to the generic action category policy.
		 */
		policy = table->action[generic_key];

	if (!hapd->conf)
		return -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
	else
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iface->current_hw_info)
		link_id = hapd->iface->current_hw_info->hw_idx;
	else
		link_id = -1;

	os_memset(&ctx_req, 0, sizeof(ctx_req));
	ctx_req.rx_link_id = link_id;
	ctx_req.data.action.category = mgmt->u.action.category;
	ctx_req.data.action.action_code = mgmt->u.action.u.wmm_action.action_code;
	ctx_req.data.action.rssi = rssi;

	decision = __get_frame_decision(&policy, false);

#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (policy == HOSTAPD_IF_FRAME_NOTIFY) {
		if (hostapd_if_plugin && hostapd_if_plugin->notify_action) {
			hostapd_if_plugin->notify_action(hapd->conf->iface,
							 sta_mac,
							 (const u8 *) mgmt,
							 plugin_frame_len,
							 link_id,
							 &ctx_req);
		}
	} else if (policy == HOSTAPD_IF_FRAME_OFFLOAD) {
		if (hostapd_if_plugin && hostapd_if_plugin->offload_action) {
			hostapd_if_plugin->offload_action(hapd->conf->iface,
							  sta_mac,
							  (const u8 *) mgmt,
							  plugin_frame_len,
							  link_id,
							  &ctx_req);
		}
	}
#endif
	return decision;
}


void hostapd_if_eapol_key_rx(struct hostapd_data *hapd, const u8 *sa,
			     const u8 *data, u16 data_len)
{
	int link_id = hapd->iface->current_hw_info ?
		hapd->iface->current_hw_info->hw_idx : -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif
#ifdef HOSTAPD_EXTERNAL_PLUGIN
	if (hostapd_if_plugin && hostapd_if_plugin->eapol_key_rx) {
		wpa_printf(MSG_DEBUG,
			   "%s: executing EAPOL-Key RX through plugin",
			   __func__);
		hostapd_if_plugin->eapol_key_rx(hapd->conf->iface, link_id, sa,
						(u8 *) data, data_len);
	}
#endif
}

/*
 * External app resumes Association flow:
 * Compose and send (Re)Association Response using ctx->status_code
 * and assoc_resp params.
 *
 * sta_mac: MAC address of the STA. Use MLD mac in case of 11be STA
 */
void __hostapd_if_assoc_response(char *ifname, uint8_t *sta_mac,
				 struct hostapd_if_frame_ctx *ctx)
{
	struct sta_info *sta;
	int omit_rsnxe = 0;
	struct hostapd_data *hapd;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " %d %d\n",
		   __func__, ifname, MAC2STR(sta_mac),
		   ctx->data.assoc_resp.is_reassoc,
		   ctx->data.assoc_resp.rssi);

	wpa_hexdump(MSG_EXCESSIVE,
		    "hostapd_if_assoc_response additional_ies",
		    ctx->data.assoc_resp.additional_ies,
		    ctx->data.assoc_resp.additional_ies_len);

	sta = __get_sta(ifname, sta_mac, ctx->rx_link_id, false, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_ASSOC_RESPONSE_ERROR,
					__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: assoc_response - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_assoc_response_exit;
	}

	/*
	 * Stash a copy of plugin-provided additional IEs
	 * for Assoc Response into sta context. This copy
	 * will be appended exactly once in the TX path and
	 * then freed.
	 */
	if (ctx->data.assoc_resp.additional_ies &&
	    ctx->data.assoc_resp.additional_ies_len) {
		/*
		 * Use incoming buffer directly; TX path will free
		 * one-shot tail
		 */
		sta->ext_assoc_tail =
			ctx->data.assoc_resp.additional_ies;
		sta->ext_assoc_tail_len =
			ctx->data.assoc_resp.additional_ies_len;
	}

	if (hapd->conf->rsn_override_omit_rsnxe)
		omit_rsnxe = 1;

	if (ctx->data.assoc_resp.pmk.pmk) {
		wpa_auth_set_pmk_full(sta->wpa_sm,
				      ctx->data.assoc_resp.pmk.pmk,
				      ctx->data.assoc_resp.pmk.pmkid,
				      ctx->data.assoc_resp.pmk.pmk_len, 0,
				      NULL);
		os_free(ctx->data.assoc_resp.pmk.pmk);
		os_free(ctx->data.assoc_resp.pmk.pmkid);
	}

	initiate_assoc_response(hapd, sta, ctx->status_code,
				ctx->data.assoc_resp.is_reassoc,
				NULL, NULL, 0,
				omit_rsnxe,
				ctx->data.assoc_resp.sta_assoc_link_mac,
				ctx->data.assoc_resp.rssi,
				false);
	sta->ext_assoc_tail = NULL;
	sta->ext_assoc_tail_len = 0;
__hostapd_if_assoc_response_exit:
	os_free((void *)ctx->data.assoc_resp.additional_ies);
	/*
	 * Free ctx handed in from plugin
	 * (tail will be freed in TX path)
	 */
	os_free((void *)ctx);
}

static void
__send_sae_auth_response(struct hostapd_data *hapd, struct sta_info *sta,
			 struct hostapd_if_frame_ctx *ctx)
{
	int resp = WLAN_STATUS_SUCCESS;
	int sta_removed = 0;
	bool success_status;
	const u8 *dst = ctx->data.auth_resp.sta_assoc_link_mac;
	u16 auth_transaction = ctx->data.auth_resp.auth_transaction;
	/* Plugin responded - cancel the guard timer before processing. */
	hostapd_if_sae_clear_plugin_wait(hapd, sta);
	if (ctx->status_code != WLAN_STATUS_SUCCESS &&
	    ctx->status_code != WLAN_STATUS_SAE_HASH_TO_ELEMENT &&
	    ctx->status_code != WLAN_STATUS_SAE_PK) {
		sae_clear_data(sta->sae);
		os_free(sta->sae);
		sta->sae = NULL;
		send_auth_reply(hapd, sta, dst, WLAN_AUTH_SAE,
				auth_transaction, ctx->status_code, NULL, 0, "auth-sae");
		if (sta->added_unassoc) {
			hostapd_drv_sta_remove(hapd, sta->addr);
			sta->added_unassoc = 0;
		}
		return;
	}

	resp = sae_sm_step(hapd, sta, ctx->data.auth_resp.auth_transaction,
			   ctx->status_code, ctx->data.auth_resp.allow_reuse,
			   &sta_removed);

	if (!sta_removed && resp != WLAN_STATUS_SUCCESS) {

		send_auth_reply(hapd, sta, dst, WLAN_AUTH_SAE,
				ctx->data.auth_resp.auth_transaction,
				resp, NULL, 0, "auth-sae");
		sae_sme_send_external_auth_status(hapd, sta, resp);
	}

	if (auth_transaction == 1)
		success_status = sae_status_success(hapd, ctx->status_code);
	else
		success_status = ctx->status_code == WLAN_STATUS_SUCCESS;

	if (!sta_removed && sta->added_unassoc &&
	    (resp != WLAN_STATUS_SUCCESS || !success_status)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}


/*
 * Use MLD mac in sta_mac, in case the STA is 11be
 */
void __hostapd_if_auth_response(char *ifname, uint8_t *sta_mac,
				struct hostapd_if_frame_ctx *ctx)
{
	struct sta_info *sta;
	struct hostapd_data *hapd;


	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " status=%u auth_alg=%u "
		   "auth_transaction=%u\n",
		   __func__, ifname, MAC2STR(sta_mac), ctx->status_code,
		   ctx->data.auth_resp.auth_alg,
		   ctx->data.auth_resp.auth_transaction);

	wpa_hexdump(MSG_EXCESSIVE,
		    "hostapd_if_auth_response additional_ies",
		    ctx->data.auth_resp.additional_ies,
		    ctx->data.auth_resp.additional_ies_len);

	sta = __get_sta(ifname, sta_mac, ctx->rx_link_id, false, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_AUTH_RESPONSE_ERROR,
					__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: auth_response - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_auth_response_exit;
	}

	/*
	 * Stash a copy of plugin-provided additional IEs
	 * for Auth Response into sta context. This copy
	 * will be appended exactly once in the TX path and
	 * then freed.
	 */
	if (ctx->data.auth_resp.additional_ies &&
	    ctx->data.auth_resp.additional_ies_len) {
		/*
		 * Use incoming buffer directly; TX path will free
		 * one-shot tail
		 */
		if (sta->ext_auth_tail)
			os_free((void *) sta->ext_auth_tail);

		sta->ext_auth_tail =
			ctx->data.auth_resp.additional_ies;
		sta->ext_auth_tail_len =
			ctx->data.auth_resp.additional_ies_len;
	}

	/*
	 * Branch by current STA auth algorithm
	 */
	switch (ctx->data.auth_resp.auth_alg) {
	case WLAN_AUTH_OPEN:

		__send_open_auth_response(hapd, sta, ctx);
		break;
#ifdef CONFIG_SAE
	case WLAN_AUTH_SAE:

		__send_sae_auth_response(hapd, sta, ctx);
		break;
#endif /* CONFIG_SAE */
	case WLAN_AUTH_FT: {
		struct hostapd_if_pmk_r1 *r1 = ctx->data.auth_resp.pmk_r1;
		uint16_t status_code;

		if (r1) {
			wpa_ft_store_pmk_r1(hapd->wpa_auth,
					    wpa_auth_get_spa(sta->wpa_sm),
					    r1->pmk_r1, r1->pmk_r1_len,
					    r1->pmk_r1_name, r1->pairwise, NULL,
					    r1->expires_in, r1->session_timeout,
					    (r1->identity_len ?
					     r1->identity : NULL),
					    r1->identity_len,
					    (r1->radius_cui_len ?
					     r1->radius_cui : NULL),
					    r1->radius_cui_len);
			status_code = ctx->status_code;
		} else {
			/*
			 * If external APP set the status code as SUCCESS
			 * without passing a valid PMK-R1, override it
			 * to INVALID_PMKID
			 */
			if (ctx->status_code == WLAN_STATUS_SUCCESS)
				status_code = WLAN_STATUS_INVALID_PMKID;
			else
				status_code = ctx->status_code;
		}
		ft_finish_pull(sta->wpa_sm, status_code);
		break;
	}
	default:
		wpa_printf(MSG_ERROR, "%s: ERROR! Unsupported algorithm\n",
			__func__);
		/*
		 * For other algorithms, no-op for now
		 */
		break;
	}
__hostapd_if_auth_response_exit:
	/*
	 * Free ctx handed in from plugin
	 * (tail will be freed in TX path)
	 */
	os_free((void *)ctx);
}

/*
 * External app resumes FT over-DS remote authentication flow:
 * Compose and send remote authentication response using ctx->status_code.
 *
 * ifname: Interface name
 * sta_mac: MAC address of the STA
 * ctx: Frame context containing status code and remote auth response data
 */
void __hostapd_if_remote_auth_response(char *ifname, uint8_t *sta_mac,
				       struct hostapd_if_frame_ctx *ctx)
{
	struct hostapd_data *hapd;
	struct wpa_state_machine *wpa_sm;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " status=%u\n",
		   __func__, ifname, MAC2STR(sta_mac), ctx->status_code);

	/*
	 * Get the appropriate hapd using link_id from context
	 */
	hapd = __hostapd_get_link_iface(ifname, ctx->rx_link_id);
	if (!hapd) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: remote_auth_response - "
			   "interface %s not found with link-id %d",
			   ifname, ctx->rx_link_id);
		goto __hostapd_if_remote_auth_response_exit;
	}

	if (ctx->data.remote_auth_resp.is_ml_sta) {
		/*
		 * Get the wpa_state_machine from FT over-DS list
		 */
		wpa_sm = get_wpa_sm_from_ft_ds_list(hapd, sta_mac);
	} else {
		struct sta_info *sta;

		sta = ap_get_sta(hapd, sta_mac);
		if (sta)
			wpa_sm = sta->wpa_sm;
		else
			wpa_sm = NULL;
	}

	if (!wpa_sm) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_AUTH_RESPONSE_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: remote_auth_response - "
			   "wpa_sm not found for STA " MACSTR " on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_remote_auth_response_exit;
	}

	/*
	 * Resume FT over-DS authentication.
	 * If the plugin provided a PMK-R1, store it in hostapd's cache first.
	 * If no PMK-R1 was provided but the plugin signalled SUCCESS, override
	 * the status to INVALID_PMKID so the STA gets a proper rejection.
	 */
	{
		struct hostapd_if_pmk_r1 *r1 =
			ctx->data.remote_auth_resp.pmk_r1;
		uint16_t status_code;

		if (r1) {
			wpa_ft_store_pmk_r1(
				hapd->wpa_auth,
				sta_mac,
				r1->pmk_r1,
				r1->pmk_r1_len,
				r1->pmk_r1_name,
				r1->pairwise,
				NULL,
				r1->expires_in,
				r1->session_timeout,
				r1->identity_len ? r1->identity : NULL,
				r1->identity_len,
				r1->radius_cui_len ? r1->radius_cui : NULL,
				r1->radius_cui_len);
			status_code = ctx->status_code;
		} else {
			/*
			 * If external APP set the status code as SUCCESS
			 * without passing a valid PMK-R1, override it
			 * to INVALID_PMKID
			 */
			if (ctx->status_code == WLAN_STATUS_SUCCESS)
				status_code = WLAN_STATUS_INVALID_PMKID;
			else
				status_code = ctx->status_code;
		}
		ft_finish_pull(wpa_sm, status_code);
	}

__hostapd_if_remote_auth_response_exit:
	/*
	 * Free ctx handed in from plugin
	 */
	os_free(ctx->data.remote_auth_resp.pmk_r1);
	os_free((void *)ctx);
}

/*
 * Internal helper: build and transmit Deauth/Disassoc with optional
 * tail
 * for ML STA, pass the MLD mac of the STA as sta_mac
 */
static void __send_mgmt_disconnect(const char *ifname,
				  const u8 *sta_mac,
				  u16 reason_code,
				  int link_id,
				  const u8 *added_data,
				  size_t added_data_len,
				  int is_deauth)
{
	struct hostapd_data *hapd;
	struct sta_info *sta = NULL;
	struct ieee80211_mgmt *mgmt;
	size_t fixed_body_len;
	size_t send_len;
	u8 *buf;
	const u8 *own_addr;
	int drv_ret;
	bool is_bcast = false;

	if (sta_mac[0] == 0xff)
		is_bcast = true;

	sta = __get_sta(ifname, sta_mac, link_id, false, &hapd);
	if (!sta && !is_bcast) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_SEND_DISCONNECT_ERROR,
					__func__, __LINE__);

		wpa_printf(MSG_ERROR,
			   "hostapd_if: %s - STA " MACSTR " not found on %s",
			   __func__, MAC2STR(sta_mac), ifname);
		goto __send_mgmt_disconnect_exit;
	}

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " reason_code=%u is_deauth %d\n",
		   __func__, ifname, MAC2STR(sta_mac), reason_code, is_deauth);

	wpa_hexdump(MSG_EXCESSIVE, "__send_mgmt_disconnect",
			added_data, added_data_len);

	own_addr = hapd->own_addr;

	/*
	 * Calculate required buffer length
	 */
	if (is_deauth)
		fixed_body_len = sizeof(mgmt->u.deauth);
	else
		fixed_body_len = sizeof(mgmt->u.disassoc);

	send_len = IEEE80211_HDRLEN + fixed_body_len + added_data_len;
	if (send_len > IEEE80211_MAX_MGMT_LEN_NO_FCS) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_SEND_DISCONNECT_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: - frame too large (send_len=%zu > max=%d) for %s "
			   MACSTR " reason=%u", send_len, IEEE80211_MAX_MGMT_LEN_NO_FCS,
			   ifname, MAC2STR(sta_mac), reason_code);
		goto __send_mgmt_disconnect_exit;
	}

	/*
	 * Allocate buffer for the entire frame
	 */
	buf = os_malloc(send_len);
	if (!buf) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_SEND_DISCONNECT_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: %s - failed to allocate %zu bytes for frame",
			   __func__, send_len);
		goto __send_mgmt_disconnect_exit;
	}

	/*
	 * Point mgmt to the allocated buffer and fill headers directly
	 */
	mgmt = (struct ieee80211_mgmt *) buf;
	os_memset(mgmt, 0, IEEE80211_HDRLEN + fixed_body_len);

	if (is_deauth)
		mgmt->frame_control =
			IEEE80211_FC(WLAN_FC_TYPE_MGMT,
				     WLAN_FC_STYPE_DEAUTH);
	else
		mgmt->frame_control =
			IEEE80211_FC(WLAN_FC_TYPE_MGMT,
				     WLAN_FC_STYPE_DISASSOC);

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && ap_sta_is_mld(hapd, sta))
		own_addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

	os_memcpy(mgmt->da, sta_mac, ETH_ALEN);
	os_memcpy(mgmt->sa, own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, own_addr, ETH_ALEN);

	if (is_deauth)
		mgmt->u.deauth.reason_code = host_to_le16(reason_code);
	else
		mgmt->u.disassoc.reason_code = host_to_le16(reason_code);

	/*
	 * Append opaque tail (e.g., IEs)
	 */
	if (added_data_len > 0)
		os_memcpy(buf + IEEE80211_HDRLEN + fixed_body_len,
			  added_data, added_data_len);

	wpa_printf(MSG_DEBUG,
		   "hostapd_if: %s ifname=%s da=%02x:%02x:%02x:%02x:%02x:%02x "
		   "reason=%u tail_len=%zu send_len=%zu",
		   is_deauth ? "DEAUTH" : "DISASSOC", ifname,
		   sta_mac[0], sta_mac[1], sta_mac[2],
		   sta_mac[3], sta_mac[4], sta_mac[5],
		   reason_code, added_data_len, send_len);

	drv_ret = hostapd_drv_send_mlme(hapd, buf, send_len, 0, NULL, 0, 0, 0, 0);
	os_free(buf);
	if (drv_ret < 0) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_SEND_DISCONNECT_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: %s - send_mlme failed (ret=%d) for %s " MACSTR " reason=%u",
			   __func__, drv_ret, ifname, MAC2STR(sta_mac), reason_code);
		goto __send_mgmt_disconnect_exit;
	}

	if (sta) {
		if (is_deauth)
			ap_sta_deauthenticate(hapd, sta, reason_code);
		else
			ap_sta_disassociate(hapd, sta, reason_code);
	}

	if (is_bcast)
		hostapd_free_stas(hapd);
__send_mgmt_disconnect_exit:
	os_free((void *)added_data);
}

void __hostapd_if_send_deauth(char *ifname, uint8_t *sta_mac,
			      uint16_t reason_code, int link_id,
			      uint8_t *added_data,
			      uint8_t added_data_len)
{
	__send_mgmt_disconnect(ifname, sta_mac, reason_code, link_id,
			added_data, (size_t) added_data_len, 1);
}

void __hostapd_if_send_disassoc(char *ifname, uint8_t *sta_mac,
				uint16_t reason_code, int link_id,
				uint8_t *added_data,
				uint8_t added_data_len)
{
	__send_mgmt_disconnect(ifname, sta_mac, reason_code, link_id,
			added_data, (size_t) added_data_len, 0);
}

void __hostapd_if_set_beacon_probe_vendor_ies(char *ifname, uint8_t *buf,
					      size_t buf_len, int link_id)
{
	struct hostapd_data *hapd;
	struct wpabuf *new_ies = NULL;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, link_id=%d buf_len=%zu\n",
		   __func__, ifname, link_id, buf_len);

	wpa_hexdump(MSG_EXCESSIVE,
		    "hostapd_if_set_beacon_probe_vendor_ies buf",
		    buf, buf_len);

	/*
	 * Look up the interface
	 */
	hapd = __hostapd_get_link_iface(ifname, link_id);
	if (!hapd) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_beacon_probe_vendor_ies - "
			   "interface %s not found with link-id %d",
			   ifname, link_id);
		goto __hostapd_if_set_beacon_probe_vendor_ies_exit;
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap && link_id >= 0) {
		struct hostapd_data *link =
			hostapd_mld_get_link_bss(hapd, link_id);

		if (link)
			hapd = link;
		else {
			__inbound_error_event(hapd, NULL,
				HOSTAPD_IF_SET_BEACON_PROBE_VENDOR_IES_ERROR,
				__func__, __LINE__);
			wpa_printf(MSG_ERROR,
				   "hostapd_if: ERROR! No BSS found with "
				   "link-id %d\n",
				   link_id);
			goto __hostapd_if_set_beacon_probe_vendor_ies_exit;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	/*
	 * Allocate and copy the buffer
	 */
	if (buf) {
		new_ies = wpabuf_alloc_copy(buf, buf_len);
		if (!new_ies) {
			__inbound_error_event(hapd, NULL,
				HOSTAPD_IF_SET_BEACON_PROBE_VENDOR_IES_ERROR,
				__func__, __LINE__);
			wpa_printf(MSG_ERROR,
				   "hostapd_if: set_beacon_probe_vendor_ies - "
				   "failed to allocate buffer for %s",
				   ifname);
			goto __hostapd_if_set_beacon_probe_vendor_ies_exit;
		}
	}

	/*
	 * Free old buffer and set new one
	 */
	wpabuf_free(hapd->plugin_vendor_elements);
	hapd->plugin_vendor_elements = new_ies;

	if (hapd->conf && hapd->conf->mld_ap && link_id < 0) {
		unsigned int i, j;

		for (i = 0; i < hapd->iface->interfaces->count; ++i) {
			struct hostapd_iface *iface =
				hapd->iface->interfaces->iface[i];
			if (!iface)
				continue;

			for (j = 0; j < iface->num_bss; j++) {
				struct hostapd_data *partner =
					iface->bss[j];

				if (hapd == partner)
					continue;

				if (!hostapd_is_ml_partner(hapd, partner))
					continue;

				wpabuf_free(partner->plugin_vendor_elements);
				if (new_ies)
					partner->plugin_vendor_elements =
						wpabuf_dup(new_ies);
				else
					partner->plugin_vendor_elements = NULL;

				break;
			}
		}
	}

	wpa_printf(MSG_DEBUG,
		   "hostapd_if: set_beacon_probe_vendor_ies for %s, len=%zu",
		   ifname, buf_len);

	/*
	 * Trigger beacon update
	 */
	if (ieee802_11_set_beacon(hapd) < 0) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_beacon_probe_vendor_ies - "
			   "failed to update beacon for %s",
			   ifname);

	}
__hostapd_if_set_beacon_probe_vendor_ies_exit:
	os_free((void *)buf);
}

static
int __get_pmk_1x(struct sta_info *sta, int akm, uint8_t pmk[PMK_LEN_MAX],
		 size_t *pmk_len, uint8_t pmkid[PMKID_LEN])
{
	/*
	 * call 1x_get_key to get the key, then use rsn_pmkid
	 * to calculate the PMKID, then return.
	 * For suite-B, put the PMKID as all zeroes if KCK is not
	 * available
	 */
	const u8 *msk;
	size_t msk_len;
	const u8 *aa = wpa_auth_get_aa(sta->wpa_sm);
	const u8 *spa = wpa_auth_get_spa(sta->wpa_sm);

	msk = ieee802_1x_get_key(sta->eapol_sm, &msk_len);
	if (!msk || msk_len < PMK_LEN)
		return -1;

	/*
	 * Mirror the PMK/pmk_len adjustment done in wpa_auth_pmksa_add
	 * before PMKID derivation.
	 */
#ifdef CONFIG_IEEE80211R_AP
	if (msk_len >= 2 * PMK_LEN && wpa_key_mgmt_ft(akm) &&
	    wpa_key_mgmt_wpa_ieee8021x(akm) && !wpa_key_mgmt_sha384(akm)) {
		/* Use MPMK/XXKey (second PMK_LEN bytes of MSK) */
		msk = msk + PMK_LEN;
		msk_len = PMK_LEN;
	} else
#endif /* CONFIG_IEEE80211R_AP */
	if (wpa_key_mgmt_sha384(akm)) {
		if (msk_len > PMK_LEN_SUITE_B_192)
		    msk_len = PMK_LEN_SUITE_B_192;
	} else if (msk_len > PMK_LEN) {
		msk_len = PMK_LEN;
	}

	if (wpa_key_mgmt_suite_b(akm)) {
		/* KCK not available; return zero PMKID */
		if (wpa_auth_get_pmkid_suite_b(sta->wpa_sm, pmkid) < 0)
			return 0;
	} else {
		rsn_pmkid(msk, msk_len, aa, spa, pmkid, akm);
	}

	os_memcpy(pmk, msk, msk_len);
	*pmk_len = msk_len;

	return 0;
}

/*
 * Use MLD mac of STA in case of 11be STA
 * This API reflects the PMK/PMKID after Assoc request - response sequence
 */
int hostapd_if_get_pmk(char *ifname, uint8_t *sta_mac,
		       uint8_t pmk[PMK_LEN_MAX], size_t *pmk_len,
		       uint8_t pmkid[PMKID_LEN])
{
	struct hostapd_data *hapd;
	struct sta_info *sta;
	int akm;

	if (!ifname || !sta_mac || !pmk || !pmk_len || !pmkid) {
		wpa_printf(MSG_ERROR,
			   "NULL parameter(s) in %s:%s mac:%p pmk:%p pmk_len:%p"
			   "pmkid:%p\n",
			   __func__, ifname, sta_mac, pmk, pmk_len, pmkid);
		return -1;
	}

	sta = __get_sta(ifname, sta_mac, -1, true, &hapd);
	if (!sta) {
		wpa_printf(MSG_ERROR, "ERROR in fetching sta object %s "
			   MACSTR "\n", __func__, MAC2STR(sta_mac));
		return -1;
	}

	if (!sta->wpa_sm)
		return -1;

	os_memset(pmkid, 0, PMKID_LEN);
	/*
	 * wpa_sm->pmksa is cleared after Assoc request if the
	 * RSNIE in the assoc request does not have PMKID
	 * wpa_sm->pmksa will reflect the presence of PMKID
	 * in the most recent Assoc request received from the STA.
	 *
	 * In case PMKID is present in the Assoc request, return this to the
	 * caller (since this is what will be used in the 4-way handshake
	 * that follows this assoc request).
	 */
	if (sta->wpa_sm->pmksa) {
		struct rsn_pmksa_cache_entry *pmksa = sta->wpa_sm->pmksa;

		*pmk_len = pmksa->pmk_len;
		os_memcpy(pmk, pmksa->pmk, *pmk_len);
		os_memcpy(pmkid, pmksa->pmkid, PMKID_LEN);
		return 0;
	}

	akm = sta->wpa_sm->wpa_key_mgmt;

	if (wpa_key_mgmt_sae(akm)) {
		if (!sta->sae || !sta->sae->pmk_len) {
			wpa_printf(MSG_ERROR, "ERROR! SAE PMK not available\n");
			return -1;
		}
		*pmk_len = sta->sae->pmk_len;
		os_memcpy(pmk, sta->sae->pmk, sta->sae->pmk_len);
		os_memcpy(pmkid, sta->sae->pmkid, PMKID_LEN);
		return 0;
	} else if (wpa_key_mgmt_wpa_ieee8021x(akm)) {
		return __get_pmk_1x(sta, akm, pmk, pmk_len, pmkid);
	} else if (akm == WPA_KEY_MGMT_OWE) {
		if (!sta->owe_pmk)
			return -1;
		*pmk_len = sta->owe_pmk_len;
		os_memcpy(pmk, sta->owe_pmk, *pmk_len);
		os_memcpy(pmkid, sta->owe_pmkid, PMKID_LEN);
		return 0;
	}

	wpa_printf(MSG_ERROR, "Unsupported AKM for GET_PMK\n");
	return -1;
}

/*
 * Use MLD mac of STA in case of 11be STA
 */
int hostapd_if_get_ptk(char *ifname, uint8_t *sta_mac,
		       uint8_t kck[MAX_KCK_LEN], size_t *kck_len,
		       uint8_t kek[MAX_KEK_LEN], size_t *kek_len,
		       uint8_t tk[MAX_TK_LEN], size_t *tk_len)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;

	sta = __get_sta(ifname, sta_mac, -1, false, &hapd);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "ERROR in fetching sta object %s "
			   MACSTR "\n",
			   __func__, MAC2STR(sta_mac));
		return -1;
	}

	return wpa_auth_get_ptk_full(sta->wpa_sm, kck, kck_len, kek, kek_len,
				     tk, tk_len);
}


int hostapd_if_get_gtk(char *ifname, int link_id, int *gtk_idx,
		       const uint8_t gtk[MAX_GTK_LEN], size_t *gtk_len)
{
	struct hostapd_data *hapd;

	hapd = __hostapd_get_link_iface(ifname, link_id);
	if (!hapd) {
		wpa_printf(MSG_ERROR, "ERROR in getting hapd %s\n", __func__);
		return -1;
	}
	return wpa_auth_get_gtk(hapd->wpa_auth, gtk_idx,
				(uint8_t *) gtk, gtk_len);
}

/*
 * Helper function to populate radio information structure
 */
static void populate_radio_info(struct hostapd_if_radio_info *radio, struct hostapd_data *hapd,
				struct sta_info *sta, const struct hostap_sta_driver_data *data,
				int driver_data_valid, bool is_mld)
{
	int freq = hapd->iface->freq;
	enum hostapd_if_band band;

	/* Populate frequency and channel */
	radio->freq = freq;
	radio->channel = hapd->iconf->channel;

	/* Determine band */
	if (hapd->iface->current_mode) {
		switch (hapd->iface->current_mode->mode) {
		case HOSTAPD_MODE_IEEE80211B:
		case HOSTAPD_MODE_IEEE80211G:
			band = HOSTAPD_IF_BAND_2GHZ;
			break;
		case HOSTAPD_MODE_IEEE80211A:
			if (is_6ghz_freq(freq))
				band = HOSTAPD_IF_BAND_6GHZ;
			else
				band = HOSTAPD_IF_BAND_5GHZ;
			break;
		case HOSTAPD_MODE_IEEE80211AD:
			band = HOSTAPD_IF_BAND_60GHZ;
			break;
		default:
			band = HOSTAPD_IF_BAND_UNKNOWN;
			return;
		}
	} else {
		band = HOSTAPD_IF_BAND_UNKNOWN;
	}
	radio->band = band;

	/* Populate signal strength from driver data */
	radio->rssi = driver_data_valid ? (int8_t)data->signal : HOSTAPD_INVALID_RSSI;

	/* Compute capability flags */
	radio->cap_flags = 0;

	if (hapd->iface->current_mode) {

		/* Check OFDM support */
		if (hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211B)
			radio->cap_flags |= HOSTAPD_IF_STA_CAP_OFDM;

		/* Check 11g support (2.4 GHz OFDM) */
		if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
			radio->cap_flags |= HOSTAPD_IF_STA_CAP_11G;
	}

	/* Check HT support */
	if (sta->flags & WLAN_STA_HT) {
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_HT | HOSTAPD_IF_STA_CAP_11N;
	}

	/* Check HT40 support */
	if (sta->ht_capabilities &&
	    (sta->ht_capabilities->ht_capabilities_info &
	     HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_HT40;

	/* Check VHT support */
	if (sta->flags & WLAN_STA_VHT)
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_VHT;

	/* Check HE support */
	if (sta->flags & WLAN_STA_HE)
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_HE;

	/* Check EHT support */
	if (sta->flags & WLAN_STA_EHT)
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_EHT;

	/* Check 6 GHz support */
	if (sta->flags & WLAN_STA_6GHZ)
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_6GHZ;

	/* Check MLD support */
	if (is_mld)
		radio->cap_flags |= HOSTAPD_IF_STA_CAP_MLD;
}


/*
 * Get station information
 * Use MLD mac of STA in case of 11be STA
 */
int hostapd_if_get_sta_info(char *ifname, uint8_t *sta_mac, struct hostapd_if_sta_info *info)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;
	struct hostap_sta_driver_data data;
	int ret;

	if (!ifname || !sta_mac || !info) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR! NULL parameters ifname=%p sta_mac=%p"
			   " info=%p", __func__, ifname, sta_mac, info);
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG, "%s: %s, " MACSTR, __func__, ifname, MAC2STR(sta_mac));

	/* Initialize output structure */
	os_memset(info, 0, sizeof(*info));

	/* Find the station */
	sta = __get_sta(ifname, sta_mac, -1, false, &hapd);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "%s: STA " MACSTR " not found on %s",
			   __func__, MAC2STR(sta_mac), ifname);
		return -ENOENT;
	}

	/* Get driver data (RSSI, rates, etc.) */
	os_memset(&data, 0, sizeof(data));
	ret = hostapd_drv_read_sta_data(hapd, &data, sta->addr);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: Failed to read driver data for " MACSTR,
			   __func__, MAC2STR(sta->addr));
		/* Continue anyway - we can still populate from sta_info */
	}

	info->ht_caps_len = 0;

	/* Copy HT capabilities */
	if (sta->ht_capabilities) {
		info->ht_caps_len = sizeof(struct ieee80211_ht_capabilities);
		if (info->ht_caps_len > HOSTAPD_IF_HT_CAP_MAX_LEN)
			info->ht_caps_len = HOSTAPD_IF_HT_CAP_MAX_LEN;
		os_memcpy(info->ht_caps, sta->ht_capabilities,
			  info->ht_caps_len);
	} else {
	}

	info->vht_caps_len = 0;

	/* Copy VHT capabilities */
	if (sta->vht_capabilities) {
		info->vht_caps_len = sizeof(struct ieee80211_vht_capabilities);
		if (info->vht_caps_len > HOSTAPD_IF_VHT_CAP_MAX_LEN)
			info->vht_caps_len = HOSTAPD_IF_VHT_CAP_MAX_LEN;
		os_memcpy(info->vht_caps, sta->vht_capabilities, info->vht_caps_len);
	}

	info->he_caps_len = 0;

	/* Copy HE capabilities */
	if (sta->he_capab && sta->he_capab_len > 0) {
		info->he_caps_len = sta->he_capab_len;
		if (info->he_caps_len > HOSTAPD_IF_HE_CAP_MAX_LEN)
			info->he_caps_len = HOSTAPD_IF_HE_CAP_MAX_LEN;
		os_memcpy(info->he_caps, sta->he_capab, info->he_caps_len);
	}

	info->eht_caps_len = 0;

	/* Copy EHT capabilities */
	if (sta->eht_capab && sta->eht_capab_len > 0) {
		info->eht_caps_len = sta->eht_capab_len;
		if (info->eht_caps_len > HOSTAPD_IF_EHT_CAP_MAX_LEN)
			info->eht_caps_len = HOSTAPD_IF_EHT_CAP_MAX_LEN;
		os_memcpy(info->eht_caps, sta->eht_capab, info->eht_caps_len);
	}

	/* Populate radio/channel/signal information based on MLD status */
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		int i;

		/* MLD station: populate per-link information */
		info->is_mld_sta = true;
		os_memcpy(info->u.mld_info.mld_addr,
			  sta->mld_info.common_info.mld_addr, ETH_ALEN);
		info->u.mld_info.eml_capa = sta->mld_info.common_info.eml_capa;
		info->u.mld_info.mld_capa = sta->mld_info.common_info.mld_capa;

		/* Copy per-link information including radio/signal/cap_flags */
		info->u.mld_info.num_links = 0;
		int max_links = (MAX_NUM_MLD_LINKS < MAX_MLO_LINKS) ? MAX_NUM_MLD_LINKS : MAX_MLO_LINKS;
		for (i = 0; i < max_links; i++) {
			struct hostapd_data *link_hapd;

			if (!sta->mld_info.links[i].valid)
				continue;

			/* Get link-specific BSS */
			link_hapd = hostapd_mld_get_link_bss(hapd, i);
			if (!link_hapd) {
				wpa_printf(MSG_WARNING,
					   "%s: No BSS found for link_id %d, "
					   "skipping", __func__, i);
				continue;
			}

			/*
			 * Now populate the link information at index i
			 * (which is the link_id)
			 */
			os_memcpy(info->u.mld_info.links[i].local_addr,
				  sta->mld_info.links[i].local_addr, ETH_ALEN);
			os_memcpy(info->u.mld_info.links[i].peer_addr,
				  sta->mld_info.links[i].peer_addr, ETH_ALEN);

			/* Populate radio info for this link */
			populate_radio_info(&info->u.mld_info.links[i].radio, link_hapd, sta, &data,
					    (ret == 0), true);

			/*
			 * Mark link as valid only after successful population
			 */
			info->u.mld_info.links[i].valid = true;
			info->u.mld_info.num_links++;
		}

		wpa_printf(MSG_DEBUG,
			   "%s: Successfully retrieved MLD info for " MACSTR " (num_links=%d)",
			   __func__, MAC2STR(sta_mac), info->u.mld_info.num_links);
	} else
#endif /* CONFIG_IEEE80211BE */
	{
		/* Non-MLD station: populate single-link information */
		populate_radio_info(&info->u.non_mld, hapd, sta, &data, (ret == 0), false);

		wpa_printf(MSG_DEBUG,
			   "%s: Successfully retrieved non-MLD info for " MACSTR
			   " (freq=%d, band=%d, rssi=%d, cap_flags=0x%x)",
			   __func__, MAC2STR(sta_mac), info->u.non_mld.freq,
			   info->u.non_mld.band, info->u.non_mld.rssi,
			   info->u.non_mld.cap_flags);
	}

	return 0;
}


/*
 * ASYNC set hooks: route plugin requests to WPA authenticator
 */
/*
 * Use MLD mac of STA in case of 11be STA
 */
void __hostapd_if_set_pmk(char *ifname, uint8_t *sta_mac,
			  uint8_t *pmk, size_t pmk_len,
			  uint8_t *pmkid, int session_timeout,
			  struct dot1x_ctx *ctx, bool dot1x_done)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;
	struct eapol_state_machine *eapol = NULL;
	struct wpa_state_machine *wpa_sm = NULL;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " pmk_len=%zu\n",
		   __func__, ifname, MAC2STR(sta_mac), pmk_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			"hostapd_if_set_pmk pmk",
			pmk, pmk_len);

	wpa_hexdump(MSG_EXCESSIVE,
		    "hostapd_if_set_pmk pmkid",
		    pmkid, PMKID_LEN);

	sta = __get_sta(ifname, sta_mac, -1, true, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_SET_PMK_ERROR,
					__func__, __LINE__);

		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_pmk - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_set_pmk_exit;
	}
	if (!sta->wpa_sm) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_SET_PMK_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_pmk - wpa_sm not initialized for STA " MACSTR " on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_set_pmk_exit;
	}

	eapol = sta->eapol_sm;
	wpa_sm = sta->wpa_sm;

	if (!dot1x_done || !ctx)
		goto __hostapd_if_set_pmk_exit;

	if (!hapd->conf->plugin_eap_offload) {
		wpa_printf(MSG_ERROR, "SET PMK CALLED WITHOUT OFFLOAD CONF\n");
		goto __hostapd_if_set_pmk_exit;
	}

	if (!eapol || !eapol->eap_if) {
		wpa_printf(MSG_ERROR,
				"hostapd_if: set_pmk - eapol not ready for STA "
				MACSTR " on %s",
				MAC2STR(sta_mac), ifname);
		goto __hostapd_if_set_pmk_exit;
	}

	/* Set the MSK in the EAPOL key location so ieee802_1x_get_key
	 * can retrieve it, then signal keyRun and keyAvailable so the
	 * WPA PTK state machine can transition AUTHENTICATION2 ->
	 * INITPMK -> PTKSTART without a full RADIUS exchange.
	 *
	 * The external application provides either the xxkey (FT) or
	 * PMK(non-FT). We must reconstruct
	 * the MSK buffer layout that INITPMK expects:
	 *   - non-FT: PMK occupies the first half of the MSK
	 *   - FT: PMK occupies the second half of the MSK
	 *
	 * Total buffer must be 2*PMK_LEN (64) so the len >= 2*PMK_LEN guard in
	 * INITPMK passes and sm->xxkey gets set.
	 */
	bin_clear_free(eapol->eap_if->eapKeyData, eapol->eap_if->eapKeyDataLen);
	eapol->eap_if->eapKeyDataLen = 0;
	eapol->eap_if->eapKeyData = os_zalloc(2 * PMK_LEN);
	if (!eapol->eap_if->eapKeyData) {
		wpa_printf(MSG_ERROR,
				"hostapd_if: set_pmk - failed to set eapKeyData for STA "
				MACSTR " on %s",
				MAC2STR(sta_mac), ifname);
		eapol->eap_if->eapKeyDataLen = 0;
		goto __hostapd_if_set_pmk_exit;
	}
	if ((pmk_len <= PMK_LEN) &&
	    wpa_key_mgmt_ft(wpa_auth_sta_key_mgmt(sta->wpa_sm)))
		os_memcpy(eapol->eap_if->eapKeyData + PMK_LEN, pmk, pmk_len);
	else
		os_memcpy(eapol->eap_if->eapKeyData, pmk, pmk_len);
	eapol->eap_if->eapKeyDataLen = 2 * PMK_LEN;
	eapol->eap_if->eapKeyAvailable = true;
	eapol->keyRun = true;

	if (ctx->identity && ctx->identity_len) {
		os_free(eapol->identity);
		eapol->identity_len = 0;
		eapol->identity = (u8 *) dup_binstr(ctx->identity,
						    ctx->identity_len);
		if (eapol->identity)
			eapol->identity_len = ctx->identity_len;
	}
	if (ctx->cui && ctx->cui_len) {
		wpabuf_free(eapol->radius_cui);
		eapol->radius_cui = wpabuf_alloc_copy(ctx->cui, ctx->cui_len);
	}
	eapol->acct_multi_session_id = ctx->multi_session_id;


	wpa_auth_sm_notify(wpa_sm);

__hostapd_if_set_pmk_exit:
	os_free((void *)pmk);
	os_free((void *)pmkid);
	if (ctx) {
		if (ctx->identity)
			os_free(ctx->identity);
		if (ctx->cui)
			os_free(ctx->cui);
		os_free(ctx);
	}

}

/*
 * Use MLD mac of STA in case of 11be STA
 */
void __hostapd_if_set_ptk(char *ifname, uint8_t *sta_mac,
			  uint8_t *kck, size_t kck_len,
			  uint8_t *kek, size_t kek_len,
			  uint8_t *tk, size_t tk_len,
			  bool authorized)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " kck_len=%zu kek_len=%zu tk_len=%zu authorized=%d\n",
		   __func__, ifname, MAC2STR(sta_mac), kck_len, kek_len, tk_len, authorized);

	wpa_hexdump_key(MSG_EXCESSIVE,
			"hostapd_if_set_ptk kck",
			kck, kck_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			"hostapd_if_set_ptk kek",
			kek, kek_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			"hostapd_if_set_ptk tk",
			tk, tk_len);

	sta = __get_sta(ifname, sta_mac, -1, false, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_SET_PTK_ERROR,
					__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_ptk - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_set_ptk_exit;
	}
	if (!sta->wpa_sm) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_SET_PTK_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_ptk - wpa_sm not initialized for STA " MACSTR " on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_set_ptk_exit;
	}

	wpa_auth_set_ptk_full(sta->wpa_sm, kck, kck_len,
			      kek, kek_len, tk, tk_len);
	wpa_auth_set_sm_ptk_done(sta->wpa_sm);

	if (authorized)
		ieee802_1x_set_sta_authorized(hapd, sta, 1);

__hostapd_if_set_ptk_exit:
	os_free((void *)kck);
	os_free((void *)kek);
	os_free((void *)tk);
}

void __hostapd_if_set_gtk(char *ifname, int link_id,
			  int gtk_idx, uint8_t *gtk, size_t gtk_len)
{
	struct hostapd_data *hapd;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, link-id:%d gtk_idx=%d gtk_len=%zu\n",
		   __func__, ifname, link_id,
		   gtk_idx, gtk_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			"hostapd_if_set_gtk gtk",
			gtk, gtk_len);

	hapd = __hostapd_get_link_iface(ifname, link_id);
	if (!hapd) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_gtk - interface %s not found",
			   ifname);
		goto __hostapd_if_set_gtk_exit;
	}

	if (!hapd->wpa_auth) {
		__inbound_error_event(hapd, NULL,
				HOSTAPD_IF_SET_GTK_ERROR,
				__func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_gtk - wpa_auth not initialized for interface %s",
			   ifname);
		goto __hostapd_if_set_gtk_exit;
	}

	/*
	 * wpa_auth_set_gtk expects a pointer to the index
	 */
	wpa_auth_set_gtk(hapd->wpa_auth, gtk_idx, gtk, gtk_len);
__hostapd_if_set_gtk_exit:
	os_free((void *)gtk);
}

/*
 * Implementation of SA Query start entry point for plugin
 * Use MLD mac of STA in case of 11be STA
 */
void __hostapd_if_start_sa_query(char *ifname, uint8_t *sta_mac, int link_id)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR "\n",
		   __func__, ifname, MAC2STR(sta_mac));

	sta = __get_sta(ifname, sta_mac, link_id, false, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_START_SA_QUERY_ERROR,
					__func__, __LINE__);

		wpa_printf(MSG_ERROR,
			   "hostapd_if: start_sa_query - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);
		goto __hostapd_if_start_sa_query_exit;
	}

	/*
	 * Call into core to start unsolicited SA Query
	 */
	if (start_unsolicited_sa_query(hapd, sta)) {
		__inbound_error_event(hapd, sta_mac,
				HOSTAPD_IF_START_SA_QUERY_ERROR,
				__func__, __LINE__);
	}
__hostapd_if_start_sa_query_exit:
	return;
}


void __hostapd_if_eapol_key_tx(char *ifname,  uint8_t *sta_mac, uint8_t link_id,
		uint8_t *frame, uint16_t frame_len)
{
	struct hostapd_data *hapd = NULL;
	struct sta_info *sta = NULL;
	u32 flags = 0;
	int encrypt = 0;

	wpa_printf(MSG_MSGDUMP,
			"%s: %s, link_id=%d frame_len=%u\n",
			__func__, ifname, link_id, (unsigned int) frame_len);

	wpa_hexdump(MSG_EXCESSIVE,
			"hostapd_if_eapol_key_tx frame",
			frame, frame_len);

	sta = __get_sta(ifname, sta_mac, link_id, false, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
					HOSTAPD_IF_EAPOL_KEY_TX_ERROR,
					__func__, __LINE__);
		wpa_printf(MSG_ERROR,
				"hostapd_if: eapol_tx - STA " MACSTR " not found on %s",
				MAC2STR(sta_mac), ifname);

		goto __hostapd_if_eapol_key_tx_exit;
	}

	hostapd_sta_flags_to_drv(sta->flags, sta->flags_ext);
	if (wpa_auth_pairwise_set(sta->wpa_sm))
		encrypt = 1;

	hostapd_drv_hapd_send_eapol(hapd, sta->addr, frame, frame_len, encrypt,
			flags, link_id);

__hostapd_if_eapol_key_tx_exit:
	os_free((void *) frame);
}


/*
 * Resume EAPOL transmission from plugin.
 * Use MLD mac of STA in case of 11be STA.
 */
void __hostapd_if_eapol_tx(char *ifname, uint8_t *sta_mac, int link_id,
			   uint8_t type, uint8_t *data, uint16_t data_len,
			   bool with_header)
{
	struct hostapd_data *hapd = NULL;
	struct sta_info *sta;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " link_id=%d type=%u data_len=%u\n",
		   __func__, ifname, MAC2STR(sta_mac), link_id, type, data_len);
	wpa_hexdump(MSG_EXCESSIVE, "hostapd_if_eapol_tx data",
		    data, data_len);

	sta = __get_sta(ifname, sta_mac, link_id, false, &hapd);
	if (!sta) {
		if (hapd)
			__inbound_error_event(hapd, sta_mac,
				      HOSTAPD_IF_EAPOL_TX_ERROR,
				      __func__, __LINE__);
		wpa_printf(MSG_ERROR,
			   "hostapd_if: eapol_tx - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);

		goto  __hostapd_if_eapol_tx_exit;
	}

	if (with_header) {
		int link_id = -1;
#ifdef CONFIG_IEEE80211BE
		link_id = hapd->conf->mld_ap ? hapd->mld_link_id : -1;
#endif /* CONFIG_IEEE80211BE */
		hostapd_drv_hapd_send_eapol(hapd, sta->addr, data, data_len,
					    wpa_auth_pairwise_set(sta->wpa_sm) ? 1 : 0,
					    hostapd_sta_flags_to_drv(sta->flags, sta->flags_ext), link_id);
	} else
		ieee802_1x_send(hapd, sta, type, data, data_len);

 __hostapd_if_eapol_tx_exit:
	os_free((void *)data);
	return;
}


void __hostapd_if_set_authorized(char *ifname, uint8_t *sta_mac, int authorized)
{
	struct hostapd_data *hapd = NULL;
	struct sta_info *sta;

	wpa_printf(MSG_MSGDUMP, "%s: %s, " MACSTR " authorized=%d\n",
		   __func__, ifname, MAC2STR(sta_mac), authorized);

	sta = __get_sta(ifname, sta_mac, -1, false, &hapd);
	if (!sta) {
		if (hapd) {
			__inbound_error_event(hapd, sta_mac,
					      HOSTAPD_IF_SET_AUTHORIZED_ERROR,
					      __func__, __LINE__);
		}
		wpa_printf(MSG_ERROR,
			   "hostapd_if: set_authorized - STA " MACSTR " not found on %s",
			   MAC2STR(sta_mac), ifname);
		return;
	}

	ieee802_1x_set_sta_authorized(hapd, sta, !!authorized);
}

void __hostapd_if_send_frame(char *ifname, int tx_link_id, uint8_t *frame, uint16_t frame_len)
{
	struct hostapd_data *hapd;
	int ret;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, link_id=%d frame_len=%u\n",
		   __func__, ifname, tx_link_id, (unsigned int) frame_len);

	wpa_hexdump(MSG_EXCESSIVE, "hostapd_if_send_frame frame", frame, frame_len);

	hapd = __hostapd_get_link_iface(ifname, tx_link_id);
	if (!hapd) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: send_frame - interface %s not found with link-id %d",
			   ifname, tx_link_id);
		goto __hostapd_if_send_frame_exit;
	}

	ret = hostapd_drv_send_mlme(hapd, frame, frame_len, 0, NULL, 0, 0, 0, 0);

	if (ret < 0) {
		__inbound_error_event(hapd, NULL, HOSTAPD_IF_SEND_FRAME_ERROR,
				      __func__, __LINE__);
		wpa_printf(MSG_ERROR, "hostapd_if: send_frame - failed for %s link-id %d",
			   ifname, tx_link_id);
	}

__hostapd_if_send_frame_exit:
	os_free((void *)frame);
}


#ifdef HOSTAPD_EXTERNAL_PLUGIN
void hostapd_plugin_register(struct hostapd_external_app_object *plugin)
{
	hostapd_if_plugin = plugin;

	if (!plugin)
		return;

	/*
	 * Southbound API assignments
	 */
	plugin->register_frame = hostapd_if_register_frame;
	plugin->register_event = hostapd_if_register_event;
	plugin->get_pmk = hostapd_if_get_pmk;
	plugin->get_ptk = hostapd_if_get_ptk;
	plugin->get_gtk = hostapd_if_get_gtk;
	plugin->get_sta_info = hostapd_if_get_sta_info;

	/*
	 * ASYNC southbound operations wired to async serializers
	 */
	hostapd_if_eloop_inbound_handlers(plugin);
}
#endif

/*
 * Event notification wrappers: emit a compact hostapd_if_event
 * to the plugin
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_deauth(struct hostapd_data *hapd, struct sta_info *sta,
			     enum hostapd_if_disconnect_type type,
			     uint16_t reason_code, bool is_tx_status,
			     int tx_status_ok)
{
	struct hostapd_if_event evt;
	int link_id = hapd->iface->current_hw_info ?
		hapd->iface->current_hw_info->hw_idx : -1;

	if (!hostapd_if_is_event_registered(hapd,
					    HOSTAPD_IF_EVENT_DEAUTH))
		return;

	if (hapd->conf->mld_ap && hapd->mld)
		link_id = hapd->mld_link_id;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_DEAUTH;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, sta->addr, sizeof(evt.sta_mac));

	evt.data.deauth_disassoc.link_id = link_id;
	if (link_id >= 0)
		os_memcpy(evt.data.deauth_disassoc.link_mac,
			  sta->mld_info.links[link_id].peer_addr, ETH_ALEN);
	evt.data.deauth_disassoc.type = type;
	evt.data.deauth_disassoc.reason_code = reason_code;
	evt.data.deauth_disassoc.is_tx_status = is_tx_status;
	evt.data.deauth_disassoc.tx_status_ok = tx_status_ok;

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR" %d %d %d %d\n", __func__,
		__LINE__, hapd->conf->iface, MAC2STR(sta->addr), link_id, reason_code,
		is_tx_status, tx_status_ok);
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_disassoc(struct hostapd_data *hapd,
			       struct sta_info *sta,
			       enum hostapd_if_disconnect_type type,
			       uint16_t reason_code,
			       bool is_tx_status,
			       int tx_status_ok)
{
	struct hostapd_if_event evt;
	int link_id = hapd->iface->current_hw_info ?
		hapd->iface->current_hw_info->hw_idx : -1;

	if (!hostapd_if_is_event_registered(hapd,
					    HOSTAPD_IF_EVENT_DISASSOC))
		return;

	if (hapd->conf->mld_ap && hapd->mld)
		link_id = hapd->mld_link_id;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_DISASSOC;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, sta->addr, sizeof(evt.sta_mac));

	evt.data.deauth_disassoc.link_id = link_id;
	if (link_id >= 0)
		os_memcpy(evt.data.deauth_disassoc.link_mac,
			  sta->mld_info.links[link_id].peer_addr, ETH_ALEN);
	evt.data.deauth_disassoc.type = type;
	evt.data.deauth_disassoc.reason_code = reason_code;
	evt.data.deauth_disassoc.tx_status_ok = tx_status_ok;
	evt.data.deauth_disassoc.is_tx_status = is_tx_status;

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR" %d %d %d %d\n", __func__,
		__LINE__, hapd->conf->iface, MAC2STR(sta->addr), link_id, reason_code,
		is_tx_status, tx_status_ok);
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_auth_tx_complete(struct hostapd_data *hapd,
				       const u8 *addr)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR"\n", __func__, __LINE__,
		hapd->conf->iface, MAC2STR(addr));
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_assoc_tx_complete(struct hostapd_data *hapd,
					const u8 *addr, int ok, uint16_t status,
					uint16_t aid)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));
	evt.data.assoc_resp_completion.ok = ok;
	evt.data.assoc_resp_completion.status = status;
	evt.data.assoc_resp_completion.aid = aid;

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR"\n", __func__, __LINE__,
		hapd->conf->iface, MAC2STR(addr));
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

void hostapd_if_notify_radius_send_event(struct hostapd_data *hapd,
					 const u8 *addr, void *radius_msg,
					 uint32_t msg_type)
{
	struct hostapd_if_event evt;
	const u8 *attrs = NULL;
	size_t attrs_len = 0;
	struct radius_hdr *hdr;
	struct wpabuf *buf;
	struct radius_msg *msg;

	if (!hapd || !radius_msg)
		return;

	if (!hostapd_if_is_event_registered(hapd,
					    HOSTAPD_IF_EVENT_RADIUS_SEND))
		return;

	msg = radius_msg;
	buf = radius_msg_get_buf(msg);
	hdr = radius_msg_get_hdr(msg);
	if (buf && wpabuf_len(buf) > sizeof(struct radius_hdr)) {
		attrs = wpabuf_head_u8(buf) + sizeof(struct radius_hdr);
		attrs_len = wpabuf_len(buf) - sizeof(struct radius_hdr);
	}

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_RADIUS_SEND;
	os_strlcpy(evt.ifname, hapd->conf->iface, sizeof(evt.ifname));
	if (addr)
		os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));
	evt.data.radius_msg.attrs = attrs;
	evt.data.radius_msg.attrs_len = attrs_len;
	evt.data.radius_msg.hdr_code = hdr->code;
	evt.data.radius_msg.msg_type = msg_type;

	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

void hostapd_if_notify_radius_receive_event(struct hostapd_data *hapd, const u8 *addr,
					    void *msg_ptr,
					    const void *hdr_ptr,
					    uint32_t msg_type)
{
	struct radius_msg *msg = (struct radius_msg *)msg_ptr;
	const struct radius_hdr *hdr = (const struct radius_hdr *)hdr_ptr;
	struct hostapd_if_event evt;
	const u8 *attrs = NULL;
	size_t attrs_len = 0;
	struct wpabuf *msgbuf;

	if (!hapd || !msg)
		return;

	if (!hostapd_if_is_event_registered(hapd,
					    HOSTAPD_IF_EVENT_RADIUS_RECEIVE))
		return;

	msgbuf = radius_msg_get_buf(msg);
	if (msgbuf && wpabuf_len(msgbuf) > sizeof(struct radius_hdr)) {
		attrs = wpabuf_head_u8(msgbuf) + sizeof(struct radius_hdr);
		attrs_len = wpabuf_len(msgbuf) - sizeof(struct radius_hdr);
	}

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_RADIUS_RECEIVE;
	os_strlcpy(evt.ifname, hapd->conf->iface, sizeof(evt.ifname));
	if (addr && !is_zero_ether_addr(addr))
		os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));
	evt.data.radius_msg.attrs = attrs;
	evt.data.radius_msg.attrs_len = attrs_len;
	evt.data.radius_msg.hdr_code = hdr->code;
	evt.data.radius_msg.msg_type = msg_type;

	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

void hostapd_if_notify_radius_coa_event(struct hostapd_data *hapd, const u8 *addr,
					void *msg_ptr, u8 hdr_code)
{
	struct radius_msg *msg = (struct radius_msg *)msg_ptr;
	struct hostapd_if_event evt;
	const u8 *attrs = NULL;
	size_t attrs_len = 0;
	struct wpabuf *msgbuf;

	if (!hapd || !msg)
		return;

	if (!hostapd_if_is_event_registered(hapd,
					    HOSTAPD_IF_EVENT_RADIUS_COA))
		return;

	msgbuf = radius_msg_get_buf(msg);
	if (msgbuf && wpabuf_len(msgbuf) > sizeof(struct radius_hdr)) {
		attrs = wpabuf_head_u8(msgbuf) + sizeof(struct radius_hdr);
		attrs_len = wpabuf_len(msgbuf) - sizeof(struct radius_hdr);
	}

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_RADIUS_COA;
	os_strlcpy(evt.ifname, hapd->conf->iface, sizeof(evt.ifname));
	if (addr)
		os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));
	evt.data.radius_msg.attrs = attrs;
	evt.data.radius_msg.attrs_len = attrs_len;
	evt.data.radius_msg.hdr_code = hdr_code;
	// No valid msg_type for COA event
	evt.data.radius_msg.msg_type = RADIUS_MSG_TYPE_INVALID;

	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_dot1x_complete(struct hostapd_data *hapd,
				     const u8 *addr,
				     const u8 *identity,
				     size_t identity_len,
				     int success)
{
	struct hostapd_if_event evt;
	size_t copy_len = 0;

	if (!hostapd_if_is_event_registered(hapd, HOSTAPD_IF_EVENT_DOT1X_COMPLETE))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_DOT1X_COMPLETE;
	os_strlcpy(evt.ifname, hapd->conf->iface, sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));

	if (identity && identity_len) {
		copy_len = identity_len;
		if (copy_len > MAX_RADIUS_CUI_LEN)
			copy_len = MAX_RADIUS_CUI_LEN;

		os_memcpy(evt.data.dot1x_completion.identity, identity,
			  copy_len);
	}

	evt.data.dot1x_completion.identity_len = copy_len;
	evt.data.dot1x_completion.success = success;

	wpa_printf(MSG_MSGDUMP, "%s: %d %s " MACSTR " %d %zu\n", __func__,
		   __LINE__, hapd->conf->iface, MAC2STR(addr), success, copy_len);
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_action_completion(struct hostapd_data *hapd,
					const u8 *addr)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_ACTION_COMPLETION))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_ACTION_COMPLETION;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR"\n", __func__, __LINE__,
		hapd->conf->iface, MAC2STR(addr));
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

void hostapd_if_event_gtk_completion(struct hostapd_data *hapd)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_GTK_COMPLETION))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_GTK_COMPLETION;
	os_strlcpy(evt.ifname, hapd->conf->iface, sizeof(evt.ifname));

	wpa_printf(MSG_MSGDUMP, "%s: %d %s\n", __func__, __LINE__,
		hapd->conf->iface);
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_eapol_m2_received(struct hostapd_data *hapd,
					const u8 *addr)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_EAPOL_M2_RECEIVED))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_EAPOL_M2_RECEIVED;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR"\n", __func__, __LINE__,
		hapd->conf->iface, MAC2STR(addr));
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_authorize_completion(struct hostapd_data *hapd,
					   const u8 *addr, int authorized)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_AUTHORIZE_COMPLETION))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_AUTHORIZE_COMPLETION;
	evt.data.authorize_completion.authorized = authorized;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR" %d\n", __func__, __LINE__,
		hapd->conf->iface, MAC2STR(addr), authorized);
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Use MLD mac of STA (in addr parameter) in case of 11be STA
 */
void hostapd_if_event_sa_query_completion(struct hostapd_data *hapd,
					  const u8 *addr,
					  enum hostapd_if_sa_query_status
					  status)
{
	struct hostapd_if_event evt;

	if (!hostapd_if_is_event_registered(
		    hapd, HOSTAPD_IF_EVENT_SA_QUERY_COMPLETION))
		return;

	os_memset(&evt, 0, sizeof(evt));
	evt.type = HOSTAPD_IF_EVENT_SA_QUERY_COMPLETION;
	os_strlcpy(evt.ifname, hapd->conf->iface,
		   sizeof(evt.ifname));
	os_memcpy(evt.sta_mac, addr, sizeof(evt.sta_mac));
	evt.data.sa_query.status = status;

	wpa_printf(MSG_MSGDUMP, "%s: %d %s "MACSTR" %d\n", __func__, __LINE__,
		hapd->conf->iface, MAC2STR(addr), status);
	HOSTAPD_EXTERNAL_PLUGIN_NOTIFY_EVENT(hapd, evt);
}

/*
 * Helper function to validate 802.11 IE buffer format
 */
static int validate_ie_buffer(const uint8_t *ie_buf, size_t buf_len,
			      const char *func)
{
	size_t pos = 0;
	size_t calculated_len = 0;

	if ((ie_buf && buf_len == 0) || (buf_len && !ie_buf)) {
		wpa_printf(MSG_ERROR,
			   "%s: invalid buf and len combination %p %zu\n",
			   func, ie_buf, buf_len);
		return -1;
	}

	if (!ie_buf || buf_len == 0)
		return 0; /* Empty buffer is valid */

	/*
	 * Iterate through IEs and calculate total length
	 */
	while (pos < buf_len) {
		uint8_t id;
		uint8_t len;

		/*
		 * Need at least 2 bytes for IE header (ID + Length)
		 */
		if (pos + 2 > buf_len) {
			wpa_printf(MSG_ERROR,
				   "%s: hostapd_if: IE validation failed - "
				   "incomplete IE header at offset %zu "
				   "(buf_len=%zu)",
				   func, pos, buf_len);
			return -1;
		}

		id = ie_buf[pos];
		len = ie_buf[pos + 1];

		/*
		 * Check if IE data fits within buffer
		 */
		if (pos + 2 + len > buf_len) {
			wpa_printf(MSG_ERROR,
				   "%s hostapd_if: IE validation failed - IE "
				   "(id=%u, len=%u) at offset %zu exceeds "
				   "buffer (buf_len=%zu)",
				   func, id, len, pos, buf_len);
			return -1;
		}

		/*
		 * Move to next IE
		 */
		pos += 2 + len;
		calculated_len += 2 + len;
	}

	/*
	 * Verify that calculated length matches provided buffer length
	 */
	if (calculated_len != buf_len) {
		wpa_printf(MSG_ERROR,
			   "%s hostapd_if: IE validation failed - calculated "
			   "length %zu does not match buffer length %zu",
			   func, calculated_len, buf_len);
		return -1;
	}

	return 0;
}

/*
 * Validate input stubs for async serializers
 */
int hostapd_if_assoc_response_validate_inputs(
	char *ifname, uint8_t *sta_mac, struct hostapd_if_frame_ctx *ctx)
{
	if (!ifname || !sta_mac || !ctx) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p:%p\n",
				__func__, ifname, sta_mac, ctx);
		return -1;
	}

	if (ctx->data.assoc_resp.pmk.pmk &&
	    (!ctx->data.assoc_resp.pmk.pmkid ||
	     !ctx->data.assoc_resp.pmk.pmk_len)) {
		wpa_printf(MSG_ERROR, "%s: ERROR! Invalid PMK provided "
			   "%p %p %zu\n", __func__, ctx->data.assoc_resp.pmk.pmk,
			   ctx->data.assoc_resp.pmk.pmkid,
			   ctx->data.assoc_resp.pmk.pmk_len);
		return -1;
	}

	/*
	 * Validate additional IEs in assoc_resp
	 */
	return validate_ie_buffer(
			ctx->data.assoc_resp.additional_ies,
			ctx->data.assoc_resp.additional_ies_len, __func__);

}

int hostapd_if_auth_response_validate_inputs(
	char *ifname, uint8_t *sta_mac, struct hostapd_if_frame_ctx *ctx)
{
	if (!ifname || !sta_mac || !ctx) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p:%p\n",
				__func__, ifname, sta_mac, ctx);
		return -1;
	}

	/*
	 * Validate additional IEs in auth_resp
	 */
	return validate_ie_buffer(
			ctx->data.auth_resp.additional_ies,
			ctx->data.auth_resp.additional_ies_len, __func__);

}

int hostapd_if_send_deauth_validate_inputs(char *ifname, uint8_t *sta_mac,
					   uint16_t reason_code,
					   uint8_t *added_data,
					   uint8_t added_data_len)
{
	if (!ifname || !sta_mac) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p\n",
				__func__, ifname, sta_mac);
		return -1;
	}

	/*
	 * Validate added_data as 802.11 IEs
	 */
	return validate_ie_buffer(added_data, added_data_len, __func__);
}

int hostapd_if_send_disassoc_validate_inputs(char *ifname, uint8_t *sta_mac,
					     uint16_t reason_code,
					     uint8_t *added_data,
					     uint8_t added_data_len)
{
	if (!ifname || !sta_mac) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p\n",
				__func__, ifname, sta_mac);
		return -1;
	}

	if (added_data && added_data_len == 0) {
		wpa_printf(MSG_ERROR,
			"%s: ERROR! added_data specified with 0 length\n",
			__func__);
		return -1;
	}

	/*
	 * Validate added_data as 802.11 IEs
	 */
	return validate_ie_buffer(added_data, added_data_len, __func__);
}

int hostapd_if_set_beacon_probe_vendor_ies_validate_inputs(char *ifname,
							   uint8_t *buf,
							   size_t buf_len,
							   int link_id)
{
	if (!ifname || (buf && !buf_len) || (!buf && buf_len)) {
		wpa_printf(MSG_ERROR, "%s: ERROR! INVALID parameters %p:%p:%zu\n",
				__func__, ifname, buf, buf_len);
		return -1;
	}

	/*
	 * Validate buf as 802.11 IEs
	 */
	return validate_ie_buffer(buf, buf_len, __func__);
}

int hostapd_if_set_pmk_validate_inputs(char *ifname, uint8_t *sta_mac,
				       uint8_t *pmk, size_t pmk_len,
				       uint8_t *pmkid)
{
	if (!ifname || !sta_mac || !pmk || pmk_len == 0 || !pmkid) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR! NULL/invalid parameters ifname=%p sta_mac=%p pmk=%p pmk_len=%zu pmkid=%p",
			   __func__, ifname, sta_mac, pmk, pmk_len, pmkid);
		return -1;
	}
	return 0;
}

int hostapd_if_set_ptk_validate_inputs(char *ifname, uint8_t *sta_mac,
				       uint8_t *kck, size_t kck_len,
				       uint8_t *kek, size_t kek_len,
				       uint8_t *tk, size_t tk_len,
				       bool authorized)
{
	if (!ifname || !sta_mac || !kck || kck_len == 0 || !kek ||
			kek_len == 0 || !tk || tk_len == 0) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR: NULL/invalid parameters ifname=%p sta_mac=%p kck=%p kck_len=%zu kek=%p kek_len=%zu tk=%p tk_len=%zu authorized=%d",
			   __func__, ifname, sta_mac, kck, kck_len,
			   kek, kek_len, tk, tk_len, authorized);
		return -1;
	}
	return 0;
}

int hostapd_if_set_gtk_validate_inputs(char *ifname, int link_id,
				       int gtk_idx, uint8_t *gtk,
				       size_t gtk_len)
{
	const int GTK_INDEX_OFFSET = 1;

	if (gtk_idx != GTK_INDEX_OFFSET &&
	    gtk_idx != (GTK_INDEX_OFFSET + 1)) {
		wpa_printf(MSG_ERROR, "%s: ERROR! invalid GTK index %d",
			   __func__, gtk_idx);
		return -1;
	}

	if (!ifname || (link_id > 0xf) || !gtk || gtk_len == 0) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR! NULL/invalid parameters ifname=%p link_id=%d gtk=%p gtk_len=%zu",
			   __func__, ifname, link_id, gtk, gtk_len);
		return -1;
	}

	return 0;
}

int hostapd_if_start_sa_query_validate_inputs(char *ifname,
					      uint8_t *sta_mac,
					      int link_id)
{
	if (!ifname || !sta_mac || (link_id > 0xf)) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p %d",
			   __func__, ifname, sta_mac, link_id);
		return -1;
	}
	return 0;
}

int hostapd_if_trigger_eapol_m3_validate_inputs(char *ifname,
						uint8_t *sta_mac)
{
	if (!ifname || !sta_mac) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p",
			   __func__, ifname, sta_mac);
		return -1;
	}
	return 0;
}

int hostapd_if_eapol_tx_validate_inputs(char *ifname, uint8_t *sta_mac,
					int link_id, uint8_t *data,
					uint16_t data_len)
{
	static const size_t HOSTAPD_IF_MAX_EAP_DATA = 1500;
	if (!ifname || !sta_mac || (!data && data_len)) {
		wpa_printf(MSG_ERROR, "hostapd_if_eapol_tx: Invalid parameters");
		return -1;
	}

	if (link_id < -1 || link_id >= MAX_MLO_LINKS) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if_eapol_tx: Invalid link_id %d",
			   link_id);
		return -1;
	}

	if (data_len > HOSTAPD_IF_MAX_EAP_DATA) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if_eapol_tx: Data length %hu exceeds maximum %zu",
			   data_len, HOSTAPD_IF_MAX_EAP_DATA);
		return -1;
	}
	return 0;
}

int hostapd_if_set_authorized_validate_inputs(char *ifname, uint8_t *sta_mac,
					      int authorized)
{
	if (!ifname || !sta_mac || (authorized != 0 && authorized != 1)) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR: invalid parameters ifname=%p sta_mac=%p authorized=%d",
			   __func__, ifname, sta_mac, authorized);
		return -1;
	}
	return 0;
}

int hostapd_if_remote_auth_response_validate_inputs(char *ifname,
						    uint8_t *sta_mac,
						    struct hostapd_if_frame_ctx *ctx)
{
	if (!ifname || !sta_mac || !ctx) {
		wpa_printf(MSG_ERROR, "%s: ERROR! NULL parameters %p:%p:%p\n",
			   __func__, ifname, sta_mac, ctx);
		return -1;
	}
	return 0;
}

int hostapd_if_send_frame_validate_inputs(char *ifname, int link_id,
					  uint8_t *frame, uint16_t frame_len)
{
	if (!ifname || !frame || frame_len < IEEE80211_HDRLEN || (link_id > 0xf)) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR! invalid parameters ifname=%p link_id=%u frame=%p frame_len=%u",
			   __func__, ifname, link_id, frame,
			   (unsigned int) frame_len);
		return -1;
	}

	return 0;
}


int hostapd_if_eapol_key_tx_validate_inputs(char *ifname, uint8_t *sta_mac, uint8_t link_id,
					    uint8_t *frame,
					    uint16_t frame_len)
{
	if (!ifname || !sta_mac || !frame || frame_len <= ETH_HLEN ||
	    (link_id != 0xff && link_id > 0xf)) {
		wpa_printf(MSG_ERROR,
			   "%s: ERROR: invalid parameters ifname=%p link_id=%u frame=%p frame_len=%u",
			   __func__, ifname, link_id, frame, frame_len);
		return -1;
	}

	return 0;
}

void hostapd_if_assoc_response_dump_params(char *ifname, uint8_t *sta_mac,
					   struct hostapd_if_frame_ctx *ctx)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " %d %d\n",
		   __func__, ifname, MAC2STR(sta_mac),
		   ctx->data.assoc_resp.is_reassoc,
		   ctx->data.assoc_resp.rssi);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    ctx->data.assoc_resp.additional_ies,
		    ctx->data.assoc_resp.additional_ies_len);
}

void hostapd_if_auth_response_dump_params(char *ifname, uint8_t *sta_mac,
					  struct hostapd_if_frame_ctx *ctx)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " status=%u auth_alg=%u "
		   "auth_transaction=%u\n",
		   __func__, ifname, MAC2STR(sta_mac), ctx->status_code,
		   ctx->data.auth_resp.auth_alg,
		   ctx->data.auth_resp.auth_transaction);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    ctx->data.auth_resp.additional_ies,
		    ctx->data.auth_resp.additional_ies_len);
}

void hostapd_if_send_deauth_dump_params(char *ifname, uint8_t *sta_mac,
					uint16_t reason_code,
					int link_id,
					uint8_t *added_data,
					uint8_t added_data_len)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " reason_code=%u link_id = %d\n",
		   __func__, ifname, MAC2STR(sta_mac), reason_code, link_id);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    added_data, added_data_len);
}

void hostapd_if_send_disassoc_dump_params(char *ifname, uint8_t *sta_mac,
					  uint16_t reason_code,
					  uint8_t *added_data,
					  uint8_t added_data_len)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " reason_code=%u\n",
		   __func__, ifname, MAC2STR(sta_mac), reason_code);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    added_data, added_data_len);
}

void hostapd_if_set_beacon_probe_vendor_ies_dump_params(
	char *ifname, uint8_t *buf, size_t buf_len, int link_id)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, link_id=%d buf_len=%zu\n",
		   __func__, ifname, link_id, buf_len);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    buf, buf_len);
}

void hostapd_if_set_pmk_dump_params(char *ifname, uint8_t *sta_mac,
				    uint8_t *pmk, size_t pmk_len,
				    uint8_t *pmkid)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " pmk_len=%zu\n",
		   __func__, ifname, MAC2STR(sta_mac), pmk_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			__func__,
			pmk, pmk_len);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    pmkid, PMKID_LEN);
}

void hostapd_if_set_ptk_dump_params(char *ifname, uint8_t *sta_mac,
				    uint8_t *kck, size_t kck_len,
				    uint8_t *kek, size_t kek_len,
				    uint8_t *tk, size_t tk_len,
				    bool authorized)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " kck_len=%zu kek_len=%zu "
		   "tk_len=%zu authorized=%d\n",
		   __func__, ifname, MAC2STR(sta_mac),
		   kck_len, kek_len, tk_len, authorized);

	wpa_hexdump_key(MSG_EXCESSIVE,
			__func__,
			kck, kck_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			__func__,
			kek, kek_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			__func__,
			tk, tk_len);
}

void hostapd_if_set_gtk_dump_params(char *ifname, int link_id,
				    int gtk_idx, uint8_t *gtk, size_t gtk_len)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, link:%d gtk_idx=%d gtk_len=%zu\n",
		   __func__, ifname, link_id,
		   gtk_idx, gtk_len);

	wpa_hexdump_key(MSG_EXCESSIVE,
			__func__,
			gtk, gtk_len);
}

void hostapd_if_start_sa_query_dump_params(char *ifname, uint8_t *sta_mac, int link_id)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " %d\n",
		   __func__, ifname, MAC2STR(sta_mac), link_id);
}

void hostapd_if_set_authorized_dump_params(char *ifname, uint8_t *sta_mac,
					   int authorized)
{
	wpa_printf(MSG_MSGDUMP, "%s: %s, " MACSTR " authorized=%d\n",
		   __func__, ifname, MAC2STR(sta_mac), authorized);
}

void hostapd_if_trigger_eapol_m3_dump_params(char *ifname, uint8_t *sta_mac)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR "\n",
		   __func__, ifname, MAC2STR(sta_mac));
}

void hostapd_if_send_frame_dump_params(char *ifname, int tx_link_id,
				       uint8_t *frame, uint16_t frame_len)
{
	wpa_printf(MSG_MSGDUMP, "%s: %s, link_id=%d frame_len=%u\n",
		   __func__, ifname, tx_link_id, (unsigned int) frame_len);

	wpa_hexdump(MSG_EXCESSIVE, __func__, frame, frame_len);
}

void hostapd_if_eapol_key_tx_dump_params(char *ifname, uint8_t *sta_mac, uint8_t link_id,
					 uint8_t *frame,
					 uint16_t frame_len)
{
	int tx_link_id = (link_id == 0xff) ? -1 : link_id;

	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, link_id=%d frame_len=%u\n",
		   __func__, ifname, tx_link_id, (unsigned int) frame_len);

	wpa_hexdump(MSG_EXCESSIVE,
		    __func__,
		    frame, frame_len);
}

void hostapd_if_remote_auth_response_dump_params(char *ifname, uint8_t *sta_mac,
						 struct hostapd_if_frame_ctx *ctx)
{
	wpa_printf(MSG_MSGDUMP,
		   "%s: %s, " MACSTR " status=%u\n",
		   __func__, ifname, MAC2STR(sta_mac), ctx->status_code);
}

size_t hostapd_if_auth_reply_tail_len(struct sta_info *sta, size_t current_len)
{
	size_t tail_len;

	if (!sta)
		return 0;

	tail_len = sta->ext_auth_tail_len;

	if (!sta->ext_auth_tail || !tail_len) {
		wpa_printf(MSG_MSGDUMP, "%s: No tail %p %zu", __func__,
			sta->ext_auth_tail, tail_len);
		return 0;
	}

	/* Enforce max mgmt frame size for additional IEs; drop tail if it would overflow */
	if (current_len + tail_len > 2300) {
		wpa_printf(MSG_ERROR,
			"Auth Response additional IEs exceed max mgmt frame length; dropping");
		return 0;
	}
	return tail_len;
}

void hostapd_if_auth_reply_add_tail(struct sta_info *sta, size_t offset,
				    size_t tail_len,
				    struct ieee80211_mgmt *reply)
{
	/* Append external additional IEs, if any (always supported) */
	if (!tail_len || !sta || !sta->ext_auth_tail)
		return;

	os_memcpy(reply->u.auth.variable + offset, sta->ext_auth_tail,
		tail_len);
}

void hostapd_if_assoc_resp_tail(struct sta_info *sta, size_t buflen,
				size_t current_len, u8 **p)
{
	size_t tail_len;
	u8 *pos = *p;

	if (!sta)
		return;

	tail_len = sta->ext_assoc_tail_len;
	if (!sta->ext_assoc_tail || !tail_len) {
		wpa_printf(MSG_MSGDUMP, "%s: No tail %p %zu", __func__,
			sta->ext_assoc_tail, tail_len);
		return;
	}

	if ((current_len + tail_len) > buflen ||
	    (current_len + tail_len) > IEEE80211_MAX_MGMT_LEN_NO_FCS) {
		wpa_printf(MSG_ERROR,
			"Assoc Response additional IEs exceed max mgmt"
			" frame length; dropping (current=%zu tail=%zu"
			" buf=%zu)", current_len, tail_len, buflen);
		return;
	}

	os_memcpy(pos, sta->ext_assoc_tail, tail_len);
	*p = (pos + tail_len);
}

size_t hostapd_if_assoc_resp_tail_len(struct sta_info *sta)
{
	size_t tail_len;

	if (!sta)
		return 0;

	tail_len = sta->ext_assoc_tail_len;
	if (!sta->ext_assoc_tail || !tail_len) {
		wpa_printf(MSG_MSGDUMP, "%s: No tail %p %zu", __func__,
			sta->ext_assoc_tail, tail_len);
		return 0;
	}

	return tail_len;
}
#endif /* CONFIG_QCN_EXTN */
