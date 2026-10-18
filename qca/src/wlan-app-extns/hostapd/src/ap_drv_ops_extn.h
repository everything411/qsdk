/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef AP_DRV_OPS_EXTN_H
#define AP_DRV_OPS_EXTN_H

#ifdef RDK_ONEWIFI

struct hostapd_data;
union wps_event_data;
enum wps_event;

int hostapd_drv_wps_event_notify_cb(struct hostapd_data *hapd, enum wps_event event,
				    union wps_event_data *data);
int hostapd_drv_sta_notify_deauth(struct hostapd_data *hapd,
				  const u8 *addr, int reason);
size_t hostapd_drv_eid_rnr_colocation_len(struct hostapd_data *hapd,
					  size_t *current_len);
u8 *hostapd_drv_eid_rnr_colocation(struct hostapd_data *hapd, u8 *eid,
				   size_t *current_len);
int hostapd_drv_get_sta_auth_type(struct hostapd_data *hapd, const u8 *addr,
				  const u8 *ies, size_t ies_len, int frame_type);
struct hostapd_data *hostapd_drv_mbssid_get_tx_bss(struct hostapd_data *hapd);
int hostapd_drv_mbssid_get_bss_index(struct hostapd_data *hapd);
size_t hostapd_drv_eid_mbssid_len(struct hostapd_data *hapd, u32 frame_type,
				  u8 *elem_count, const u8 *known_bss,
				  size_t known_bss_len, size_t *rnr_len);
u8 *hostapd_drv_eid_mbssid(struct hostapd_data *hapd, u8 *eid, u8 *end,
			   unsigned int frame_stype, u8 elem_count,
			   u8 **elem_offset,
			   const u8 *known_bss, size_t known_bss_len, u8 *rnr_eid,
			   u8 *rnr_count, u8 **rnr_offset, size_t rnr_len);
u8 *hostapd_drv_mbssid_config(struct hostapd_data *hapd, u8 *eid);

#endif /* RDK_ONEWIFI */

#endif /* AP_DRV_OPS_EXTN_H */
