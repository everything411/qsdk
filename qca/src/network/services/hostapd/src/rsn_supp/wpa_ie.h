/*
 * wpa_supplicant - WPA/RSN IE and KDE definitions
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPA_IE_H
#define WPA_IE_H

struct wpa_sm;

int wpa_gen_wpa_ie(struct wpa_sm *sm, u8 *wpa_ie, size_t wpa_ie_len);
int wpa_gen_rsnxe(struct wpa_sm *sm, u8 *rsnxe, size_t rsnxe_len);
u16 rsn_supp_capab(struct wpa_sm *sm);
u64 wpa_sm_get_rsnxe_capab(struct wpa_sm *sm);
int security_profile_akm_matches(int profile_num, int key_mgmt);
int security_profile_build_sta_ie(struct wpa_sm *sm,
				  int selected_profile_num,
				  u8 *buf, size_t buf_len);
int wpa_external_auth_add_rsne(u8 *rsne, size_t rsne_len, struct wpa_sm *sm,
			       int akmp, int pairwise_cipher,
			       int group_cipher, int group_mgmt_cipher,
			       u16 rsn_capab);

#endif /* WPA_IE_H */
