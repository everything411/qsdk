/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HOSTAPD_IF_H
#define HOSTAPD_IF_H

/*
 * Minimal standalone definitions to avoid relying on full hostapd
 * headers here. These should align with existing project types when
 * integrated.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
#include <stddef.h>
#include <stdint.h>
typedef uint16_t u16;

#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/hostapd_external_interface.h"
#endif /* CONFIG_QCN_EXTN */
#include "hostapd_if_common.h"
#include "utils/includes.h"
#include "utils/common.h"
#include "utils/bitfield.h"
#include "common/wpa_ctrl.h"
#include "ap/hostapd.h"

struct hostapd_data;
struct sta_info;
struct ieee80211_mgmt;

/*
 * Frame processing decision returned to RX handlers
 */
enum hostapd_if_frame_processing_decision {
	HOSTAPD_IF_FRAME_PROCESSING_CONTINUE = 0,
	HOSTAPD_IF_FRAME_PROCESSING_WAIT = 1,
	HOSTAPD_IF_FRAME_PROCESSING_OFFLOAD = 2
};

#ifdef CONFIG_QCN_EXTN
/*
 * IEEE 802.11 management subtype for Authentication
 */
#ifndef WLAN_FC_STYPE_AUTH
#define WLAN_FC_STYPE_AUTH 11
#endif

/*
 * Notify/invoke external application for Authentication and return
 * processing decision.
 *
 * Sidecar notify model:
 *  - DO_NOTHING: CONTINUE (no plugin call)
 *  - NOTIFY: CONTINUE (+ plugin->notify_auth)
 *  - INVOKE: WAIT (+ plugin->invoke_auth)
 */
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
		       const u8 *sa);

enum hostapd_if_frame_processing_decision
hostapd_if_frame_fwd_decision(struct hostapd_data *hapd, u16 auth_alg,
				      enum hostapd_if_frame_reg_type frame_type);

void hostapd_if_notify_deauth(struct hostapd_data *hapd,
			      struct sta_info *sta,
			      const void *frame,
			      size_t frame_len);

void hostapd_if_sae_clear_plugin_wait(struct hostapd_data *hapd,
				      struct sta_info *sta);
void hostapd_if_notify_disassoc(struct hostapd_data *hapd,
				struct sta_info *sta,
				const void *frame,
				size_t frame_len);

enum hostapd_if_frame_processing_decision
hostapd_if_notify_action(struct hostapd_data *hapd,
			 struct sta_info *sta,
			 const struct ieee80211_mgmt *mgmt,
			 size_t frame_len, int rssi);

enum hostapd_if_frame_processing_decision
hostapd_if_notify_assoc(struct hostapd_data *hapd,
			struct sta_info *sta,
			const uint8_t *frame,
			uint16_t frame_len,
			u16 status_code,
			int is_reassoc,
			int rssi,
			bool set_beacon,
			const u8 *sa);

int hostapd_if_init(struct hapd_interfaces *interfaces, bool plugin_enable,
		    enum hostapd_if_eloop_type eloop_type);
int hostapd_if_deinit(void);

void hostapd_if_interface_remove(struct hostapd_data *hapd);

int hostapd_if_interface_create(struct hostapd_data *hapd);

int hostapd_if_set_interfaces(struct hapd_interfaces *interfaces);
enum hostapd_if_frame_processing_decision
hostapd_if_notify_remote_auth(struct hostapd_data *hapd, uint8_t *sta_mac,
			      const uint8_t *ies, uint16_t ies_len,
			      uint16_t status_code, bool is_ml);

int hostapd_if_pull_pmk_r1(struct hostapd_data *hapd, uint8_t *sta_mac,
			   uint8_t *pmk_r1_name, uint8_t *pmk_r1,
			   size_t *pmk_r1_len, int *pairwise,
			   int *session_timeout, const uint8_t **identity,
			   size_t *identity_len, const uint8_t **radius_cui,
			   size_t *radius_cui_len);

int hostapd_if_pull_pmk(struct hostapd_data *hapd, uint8_t *sta_mac,
			uint8_t *pmk, size_t *pmk_len, uint8_t *pmkid,
			int *session_timeout);

void hostapd_if_eapol_rx(struct hostapd_data *hapd, const u8 *sa,
			 const u8 *data, u16 data_len);

void hostapd_if_eapol_key_rx(struct hostapd_data *hapd, const u8 *sa,
			     const u8 *data, u16 data_len);

/*
 * Event notification wrapper functions
 */
void hostapd_if_event_deauth(struct hostapd_data *hapd,
			     struct sta_info *sta,
			     enum hostapd_if_disconnect_type type,
			     uint16_t reason_code,
			     bool is_tx_status,
			     int tx_status_ok);

void hostapd_if_event_disassoc(struct hostapd_data *hapd,
			       struct sta_info *sta,
			       enum hostapd_if_disconnect_type type,
			       uint16_t reason_code,
			       bool is_tx_status,
			       int tx_status_ok);

void hostapd_if_notify_radius_send_event(struct hostapd_data *hapd,
					 const u8 *addr, void *radius_msg,
					 uint32_t msg_type);

void hostapd_if_notify_radius_receive_event(struct hostapd_data *hapd,
					    const u8 *addr,
					    void *msg,
					    const void *hdr,
					    uint32_t msg_type);

void hostapd_if_notify_radius_coa_event(struct hostapd_data *hapd, const u8 *addr,
					void *msg, u8 hdr_code);

void hostapd_if_event_assoc_tx_complete(struct hostapd_data *hapd,
					const u8 *addr, int ok, uint16_t status,
					uint16_t aid);

void hostapd_if_event_dot1x_complete(struct hostapd_data *hapd,
				     const u8 *addr,
				     const u8 *identity,
				     size_t identity_len,
				     int success);

void hostapd_if_event_auth_tx_complete(struct hostapd_data *hapd,
				       const u8 *addr);

void hostapd_if_event_action_completion(struct hostapd_data *hapd,
					const u8 *addr);

void hostapd_if_event_gtk_completion(struct hostapd_data *hapd);

void hostapd_if_event_eapol_m2_received(struct hostapd_data *hapd,
					const u8 *addr);

void hostapd_if_event_authorize_completion(struct hostapd_data *hapd,
					   const u8 *addr,
					   int authorized);

void hostapd_if_event_sa_query_completion(struct hostapd_data *hapd,
					  const u8 *addr,
					  enum hostapd_if_sa_query_status
					  status);

size_t hostapd_if_auth_reply_tail_len(struct sta_info *sta, size_t current_len);
void hostapd_if_auth_reply_add_tail(struct sta_info *sta, size_t offset,
				    size_t tail_len,
				    struct ieee80211_mgmt *reply);
void hostapd_if_assoc_resp_tail(struct sta_info *sta, size_t buflen,
				size_t current_len, u8 **p);
size_t hostapd_if_assoc_resp_tail_len(struct sta_info *sta);

struct hostapd_data *__hostapd_get_link_iface(const char *ifname, int link_id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#else
/* Stub implementations when CONFIG_QCN_EXTN is not defined (real hostapd_if lib absent) */
static inline enum hostapd_if_frame_processing_decision
hostapd_if_notify_auth(struct hostapd_data *hapd, struct sta_info *sta,
		       const uint8_t *frame, uint16_t frame_len, int rssi,
		       u16 status_code, u16 auth_transaction, u8 allow_reuse,
		       u16 auth_alg, const u8 *sa)
{ return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE; }
static inline void
hostapd_if_notify_deauth(struct hostapd_data *hapd, struct sta_info *sta,
			 const void *frame, size_t frame_len) {}
static inline void
hostapd_if_notify_disassoc(struct hostapd_data *hapd, struct sta_info *sta,
			   const void *frame, size_t frame_len) {}
static inline enum hostapd_if_frame_processing_decision
hostapd_if_notify_action(struct hostapd_data *hapd, struct sta_info *sta,
			 const struct ieee80211_mgmt *mgmt,
			 size_t frame_len, int rssi)
{ return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE; }
static inline enum hostapd_if_frame_processing_decision
hostapd_if_notify_assoc(struct hostapd_data *hapd, struct sta_info *sta,
			const uint8_t *frame, uint16_t frame_len,
			u16 status_code, int is_reassoc, int rssi,
			bool set_beacon, const u8 *sa)
{ return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE; }
static inline int
hostapd_if_init(struct hapd_interfaces *interfaces, bool plugin_enable)
{ return 0; }
static inline int hostapd_if_deinit(void) { return 0; }
static inline enum hostapd_if_frame_processing_decision
hostapd_if_frame_fwd_decision(struct hostapd_data *hapd, u16 auth_alg, int frame_type)
{ return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE; }
static inline void
hostapd_if_interface_remove(struct hostapd_data *hapd) {}
static inline int
hostapd_if_interface_create(struct hostapd_data *hapd) { return 0; }
static inline int
hostapd_if_set_interfaces(struct hapd_interfaces *interfaces) { return 0; }
static inline enum hostapd_if_frame_processing_decision
hostapd_if_notify_remote_auth(struct hostapd_data *hapd, uint8_t *sta_mac,
			      const uint8_t *ies, uint16_t ies_len,
			      uint16_t status_code, bool is_ml)
{ return HOSTAPD_IF_FRAME_PROCESSING_CONTINUE; }
static inline int
hostapd_if_pull_pmk_r1(struct hostapd_data *hapd, uint8_t *sta_mac,
		       uint8_t *pmk_r1_name, uint8_t *pmk_r1, size_t *pmk_r1_len,
		       int *pairwise, int *session_timeout,
		       const uint8_t **identity, size_t *identity_len,
		       const uint8_t **radius_cui, size_t *radius_cui_len)
{ return -1; }
static inline int
hostapd_if_pull_pmk(struct hostapd_data *hapd, uint8_t *sta_mac,
		    uint8_t *pmk, size_t *pmk_len, uint8_t *pmkid,
		    int *session_timeout) { return -1; }
static inline void
hostapd_if_eapol_rx(struct hostapd_data *hapd, const u8 *sa,
		    const u8 *data, u16 data_len) {}
static inline void
hostapd_if_eapol_key_rx(struct hostapd_data *hapd, const u8 *sa,
			 const u8 *data, u16 data_len) {}
static inline void
hostapd_if_event_deauth(struct hostapd_data *hapd, struct sta_info *sta,
			int type, uint16_t reason_code,
			bool is_tx_status, int tx_status_ok) {}
static inline void
hostapd_if_event_disassoc(struct hostapd_data *hapd, struct sta_info *sta,
			  int type, uint16_t reason_code,
			  bool is_tx_status, int tx_status_ok) {}
static inline void
hostapd_if_notify_radius_send_event(struct hostapd_data *hapd, const u8 *addr,
				    void *radius_msg, uint32_t msg_type) {}
static inline void
hostapd_if_notify_radius_receive_event(struct hostapd_data *hapd, const u8 *addr,
				       void *msg, const void *hdr,
				       uint32_t msg_type) {}
static inline void
hostapd_if_notify_radius_coa_event(struct hostapd_data *hapd, const u8 *addr,
				   void *msg, u8 hdr_code) {}
static inline void
hostapd_if_event_assoc_tx_complete(struct hostapd_data *hapd, const u8 *addr,
				   int ok, uint16_t status, uint16_t aid) {}
static inline void
hostapd_if_event_dot1x_complete(struct hostapd_data *hapd, const u8 *addr,
				const u8 *identity, size_t identity_len,
				int success) {}
static inline void
hostapd_if_event_auth_tx_complete(struct hostapd_data *hapd, const u8 *addr) {}
static inline void
hostapd_if_event_action_completion(struct hostapd_data *hapd, const u8 *addr) {}
static inline void hostapd_if_event_gtk_completion(struct hostapd_data *hapd) {}
static inline void
hostapd_if_event_eapol_m2_received(struct hostapd_data *hapd, const u8 *addr) {}
static inline void
hostapd_if_event_authorize_completion(struct hostapd_data *hapd, const u8 *addr,
				      int authorized) {}
static inline void
hostapd_if_event_sa_query_completion(struct hostapd_data *hapd, const u8 *addr,
				     int status) {}
static inline size_t
hostapd_if_auth_reply_tail_len(struct sta_info *sta, size_t current_len)
{ return 0; }
static inline void
hostapd_if_auth_reply_add_tail(struct sta_info *sta, size_t offset,
			       size_t tail_len,
			       struct ieee80211_mgmt *reply)
{
}
static inline void
hostapd_if_assoc_resp_tail(struct sta_info *sta, size_t buflen,
			   size_t current_len, u8 **p)
{
}
static inline size_t
hostapd_if_assoc_resp_tail_len(struct sta_info *sta)
{
	return 0;
}
static inline void
hostapd_if_sae_clear_plugin_wait(struct hostapd_data *hapd,
				 struct sta_info *sta) {}
#endif /* CONFIG_QCN_EXTN */

#endif /* HOSTAPD_IF_H */
