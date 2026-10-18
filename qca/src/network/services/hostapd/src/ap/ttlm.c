/*
 * hostapd / Tid-to-link Mapping(TTLM)
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "sta_info.h"
#include "hostapd.h"
#include "ap_drv_ops.h"
#include "drivers/driver.h"
#include "ttlm.h"
#include "eloop.h"

int hostapd_get_ttlm_elem_len(struct ttlm_info *ttlm)
{
	u8 tid, num_tids;
	size_t elem_len;

	if (!ttlm || ttlm->direction == TTLM_DIRECTION_INVALID)
		return 0;

	elem_len = sizeof(struct tid_to_link_mapping_elem);

	if (ttlm->default_link_mapping) {
		elem_len += sizeof(u8);
	} else {
		elem_len += sizeof(u16);

		num_tids = 0;
		for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
			if (!ttlm->ieee_link_map_tid[tid])
				continue;
			num_tids++;
		}

		elem_len += num_tids * (ttlm->link_mapping_size ? sizeof(u8) : sizeof(u16));

		if (ttlm->mapping_switch_time_present)
			elem_len += sizeof(u16);

		if (ttlm->expected_duration_present)
			elem_len += TTLM_EXPECTED_DURATION_SIZE * sizeof(u8);
	}

	return elem_len;
}


u8 *hostapd_add_ttlm_info_elem(u8 *pos, struct ttlm_info *ttlm, struct hostapd_data *hapd)
{
	struct tid_to_link_mapping_elem *ttlm_elem;
	u8 link_mapping_presence_indicator = 0;
	u8 *link_mapping_of_tids;
	u8 tid, len;
	u16 ttlm_control = 0;
	u16 *ttlm_control_field;

	if (ttlm->mapping_switch_time_present && hapd && !hapd->mapping_switch_time) {
		wpa_printf(MSG_DEBUG, "TTLM MST TSF update not received. Skip adding TTLM IE");
		return pos;
	}

	if (ttlm->direction >= TTLM_DIRECTION_MAX) {
		wpa_printf(MSG_DEBUG, "TTLM Invalid direction. Skip adding TTLM IE");
		return pos;
	}

	ttlm_elem = (struct tid_to_link_mapping_elem *)pos;
	ttlm_elem->elem_id = WLAN_EID_EXTENSION;
	ttlm_elem->elem_id_extn = WLAN_EID_EXT_TID_TO_LINK_MAPPING;
	len = hostapd_get_ttlm_elem_len(ttlm);
	ttlm_elem->elem_len = len - sizeof(struct elem_header);
	ttlm_control_field = (u16 *)(void *)ttlm_elem->data;

	ttlm_control |= (ttlm->direction << TTLM_CONTROL_DIRECTION_IDX)
			& TTLM_CONTROL_DIRECTION_MASK;

	ttlm_control |= (ttlm->default_link_mapping << TTLM_CONTROL_DEFAULT_LINK_MAPPING_IDX)
			& TTLM_CONTROL_DEFAULT_LINK_MAPPING_MASK;

	if (ttlm->default_link_mapping) {
		/* Link mapping of TIDs are not present when default mapping is
		 * set. Hence, the size of TID-To-Link mapping control is one
		 * octet.
		 */
		*ttlm_control_field = (u8)ttlm_control;

		wpa_printf(MSG_DEBUG, "TTLM IE added, dir %d default_link_mapping %d",
			   ttlm->direction, ttlm->default_link_mapping);
		pos += sizeof(*ttlm_elem) + sizeof(u8);

		return pos;
	}

	ttlm_control |= (ttlm->link_mapping_size << TTLM_CONTROL_LINK_MAPPING_SIZE_IDX)
			& TTLM_CONTROL_LINK_MAPPING_SIZE_MASK;

	ttlm_control |=
		(ttlm->mapping_switch_time_present << TTLM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_IDX)
		& TTLM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_MASK;

	ttlm_control |=
		(ttlm->expected_duration_present << TTLM_CONTROL_EXPECTED_DURATION_PRESENT_IDX)
		& TTLM_CONTROL_EXPECTED_DURATION_PRESENT_MASK;

	for (tid = 0; tid < NUM_MAX_TIDS; tid++)
		if (ttlm->ieee_link_map_tid[tid])
			link_mapping_presence_indicator |= BIT(tid);

	ttlm_control |= (link_mapping_presence_indicator <<
			 TTLM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_IDX)
			& TTLM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_MASK;

	/* The size of TID-To-Link mapping control is two octets when
	 * default link mapping is not set.
	 */
	*ttlm_control_field = host_to_le16(ttlm_control);
	pos += sizeof(*ttlm_elem) + sizeof(u16);

	if (hapd && ttlm->mapping_switch_time_present) {
		/* Mapping switch time is different for each vdevs. Hence,
		 * populate the mapping switch time from hapd.
		 */

		*(u16 *)pos = host_to_le16(hapd->mapping_switch_time);
		wpa_printf(MSG_DEBUG, "mapping_switch_time %d",
			   ttlm->mapping_switch_time);
		pos += sizeof(u16);
	}

	if (ttlm->expected_duration_present) {
		memcpy(pos, &ttlm->expected_duration,
		       TTLM_EXPECTED_DURATION_SIZE *
		       sizeof(u8));
		wpa_printf(MSG_DEBUG, "expected_duration %u",
			   ttlm->expected_duration);
		pos += TTLM_EXPECTED_DURATION_SIZE * sizeof(u8);
	}

	wpa_printf(MSG_DEBUG, "TTLM IE added, dir %d link_mapping_presence_indicator 0x%x",
		   ttlm->direction, link_mapping_presence_indicator);

	link_mapping_of_tids = pos;

	for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
		if (!ttlm->ieee_link_map_tid[tid])
			continue;
		if (!ttlm->link_mapping_size) {
			*(u16 *)link_mapping_of_tids =
				host_to_le16(ttlm->ieee_link_map_tid[tid]);
			wpa_printf(MSG_DEBUG, "link mapping of TID%d is %x",
				   tid, host_to_le16(ttlm->ieee_link_map_tid[tid]));
			link_mapping_of_tids += sizeof(u16);
		} else {
			*(u8 *)link_mapping_of_tids =
				ttlm->ieee_link_map_tid[tid];
			wpa_printf(MSG_DEBUG, "link mapping of TID%d is %x",
				   tid, ttlm->ieee_link_map_tid[tid]);
			link_mapping_of_tids += sizeof(u8);
		}
	}

	return link_mapping_of_tids;
}


int hostapd_build_ttlm_elem(struct ttlm_ongoing_negotiation_info *ttlm,
			    u8 **ttlm_elem, size_t *ttlm_elem_len)
{
	u8 *pos;
	u8 dir;

	if (!ttlm || !ttlm_elem || !ttlm_elem_len)
		return -1;

	*ttlm_elem_len = 0;

	if ((ttlm->ttlm_info[TTLM_DIRECTION_DL].direction == TTLM_DIRECTION_DL ||
	     ttlm->ttlm_info[TTLM_DIRECTION_UL].direction == TTLM_DIRECTION_UL) &&
	    ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction == TTLM_DIRECTION_BIDI) {
		wpa_printf(MSG_DEBUG, "Both DL/UL and BIDI TTLM IEs cannot exist at same time");
		return -1;
	}

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		if (ttlm->ttlm_info[dir].direction != TTLM_DIRECTION_INVALID)
			*ttlm_elem_len += hostapd_get_ttlm_elem_len(&ttlm->ttlm_info[dir]);
	}

	if (*ttlm_elem_len == 0)
		return -1;

	*ttlm_elem = os_zalloc(*ttlm_elem_len);
	if (!*ttlm_elem)
		return -1;

	pos = *ttlm_elem;
	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		if (ttlm->ttlm_info[dir].direction != TTLM_DIRECTION_INVALID)
			pos = hostapd_add_ttlm_info_elem(pos, &ttlm->ttlm_info[dir], NULL);
	}

	return 0;
}


static void hostapd_copy_configured_ttlm_to_sta_info(struct sta_info *sta,
						     struct hostapd_data *hapd,
						     struct ttlm_ongoing_negotiation_info
						     *ttlm_info, u8 dialog_token)
{
	struct ttlm_ongoing_negotiation_info *current_ttlm_info = NULL;
	struct ttlm_ongoing_negotiation_info *partner_ttlm_info = NULL;
	struct tid_to_link_map_info *partner_tid_map = NULL;
	struct hostapd_data *lhapd;
	struct sta_info *lsta;
	u8 dir;
	u8 tid;

	current_ttlm_info = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
	os_memcpy(current_ttlm_info, ttlm_info, sizeof(*current_ttlm_info));
	current_ttlm_info->dialog_token = dialog_token;

	for_each_mld_link(lhapd, hapd) {
		lsta = ap_get_sta(lhapd, sta->addr);
		if (lsta && lsta == sta)
			continue;

		if (lsta && lsta->mld_info.mld_sta) {
			partner_tid_map = &lsta->mld_info.tid_map_info;
			partner_ttlm_info = &partner_tid_map->ttlm_ongoing_negotiation_info;
			os_memcpy(partner_ttlm_info, ttlm_info, sizeof(*partner_ttlm_info));
			partner_ttlm_info->dialog_token = dialog_token;
		}
	}

	wpa_printf(MSG_DEBUG, "Ongoing TTLM:dialog_token:%d",
		   current_ttlm_info->dialog_token);

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		wpa_printf(MSG_DEBUG, "dir:%d default_link_mapping:%d",
			   current_ttlm_info->ttlm_info[dir].direction,
			   current_ttlm_info->ttlm_info[dir].default_link_mapping);
		for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
			wpa_printf(MSG_DEBUG, "ttlm_links[%d]:0x%x", tid,
				   current_ttlm_info->ttlm_info[dir].ieee_link_map_tid[tid]);
		}
	}
}


int
hostapd_fill_ttlm_params(struct ttlm_info *upcoming_info,
			 struct ttlm_info *established_info,
			 struct drv_adv_ttlm_params *upcoming_ttlm_params,
			 struct drv_adv_ttlm_params *established_ttlm_params)
{
	if (!upcoming_info || !established_info ||
	    !upcoming_ttlm_params || !established_ttlm_params) {
		wpa_printf(MSG_DEBUG, "TTLM: Invalid args to fill ttlm params");
		return -EINVAL;
	}

	upcoming_ttlm_params->default_link_mapping =
		upcoming_info->default_link_mapping;
	upcoming_ttlm_params->link_mapping_size =
		upcoming_info->link_mapping_size;
	upcoming_ttlm_params->mapping_switch_time_present =
		upcoming_info->mapping_switch_time_present;
	upcoming_ttlm_params->expected_duration_present =
		upcoming_info->expected_duration_present;
	upcoming_ttlm_params->mapping_switch_time =
		upcoming_info->mapping_switch_time;
	upcoming_ttlm_params->expected_duration =
		upcoming_info->expected_duration;
	os_memcpy(upcoming_ttlm_params->ieee_link_map_tid,
		  upcoming_info->ieee_link_map_tid,
		  sizeof(u16) * NUM_MAX_TIDS);

	established_ttlm_params->default_link_mapping =
		established_info->default_link_mapping;
	established_ttlm_params->link_mapping_size =
		established_info->link_mapping_size;
	established_ttlm_params->mapping_switch_time_present =
		established_info->mapping_switch_time_present;
	established_ttlm_params->expected_duration_present =
		established_info->expected_duration_present;
	established_ttlm_params->mapping_switch_time =
		established_info->mapping_switch_time;
	established_ttlm_params->expected_duration =
		established_info->expected_duration;
	os_memcpy(established_ttlm_params->ieee_link_map_tid,
		  established_info->ieee_link_map_tid,
		  sizeof(u16) * NUM_MAX_TIDS);

	return 0;
}


void hostapd_ttlm_handle_mapping_switch_time_expiry(struct ttlm_context *ttlm_ctx,
						    u8 link_id)
{
	struct ttlm_info *ttlm;

	wpa_printf(MSG_INFO, "TTLM: Mapping switch time expired for link id:%d ",
		   link_id);

	os_memcpy(&ttlm_ctx->established_ttlm, &ttlm_ctx->upcoming_ttlm,
		  sizeof(struct mlo_ttlm_ie));

	ttlm_ctx->established_ttlm.ttlm.mapping_switch_time_present = false;
	ttlm_ctx->established_ttlm.ttlm.mapping_switch_time = 0;

	ttlm = &ttlm_ctx->established_ttlm.ttlm;
	wpa_printf(MSG_INFO, "TTLM: Established mapping: disabled_link_bitmap:%x "
		   "dir:%d default_map:%d MSTP:%d EDP:%d MST:%d ED:%d ieee_link_map:%x",
		   ttlm_ctx->established_ttlm.disabled_link_bitmap,
		   ttlm->direction, ttlm->default_link_mapping,
		   ttlm->mapping_switch_time_present,
		   ttlm->expected_duration_present,
		   ttlm->mapping_switch_time, ttlm->expected_duration,
		   ttlm->ieee_link_map_tid[0]);

	os_memset(&ttlm_ctx->upcoming_ttlm, 0, sizeof(struct mlo_ttlm_ie));
	ttlm_ctx->upcoming_ttlm.ttlm.direction = TTLM_DIRECTION_INVALID;
}


void hostapd_ttlm_handle_expected_duration_expiry(struct ttlm_context *ttlm_ctx,
						  u8 link_id)
{
	wpa_printf(MSG_INFO, "TTLM: Expected duration expired for link id:%d ",
		   link_id);

	if (ttlm_ctx->upcoming_ttlm.ttlm.mapping_switch_time_present) {
		/* Copy the new non-default ongoing mapping to established
		 * mapping if expected duration expires for the established
		 * mapping.
		 */
		hostapd_ttlm_handle_mapping_switch_time_expiry(ttlm_ctx,
							       link_id);
		return;
	}

	/* Use the default mapping when expected duration expires for the
	 * established mapping and no new non-default TTLM announcement is
	 * ongoing.
	 */
	os_memset(&ttlm_ctx->established_ttlm, 0, sizeof(struct mlo_ttlm_ie));

	ttlm_ctx->established_ttlm.ttlm.direction = TTLM_DIRECTION_BIDI;
	ttlm_ctx->established_ttlm.ttlm.default_link_mapping = 1;
	ttlm_ctx->established_ttlm.disabled_link_bitmap = 0;
	ttlm_ctx->established_ttlm.ttlm.link_mapping_size = 0;
#ifdef CONFIG_QCN_EXTN
	ttlm_ctx->established_t2lm_ed_modified_in_case_of_cac = false;
#endif /* CONFIG_QCN_EXTN */
	wpa_printf(MSG_INFO, "TTLM: Set established mapping to default mapping");
}


int
hostapd_offload_set_adv_ttlm_multi_mbssid(struct hostapd_data *hapd)
{
	struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;
	struct hostapd_data *bss;
	int ret = 0;

	dl_list_for_each(bss, &group->bss_list, struct hostapd_data, mbssid_bss) {
#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(bss->conf))
			continue;
#endif /* CONFIG_QCN_EXTN */

		if (bss != hapd && bss->conf->mld_ap && bss->mld &&
		    !bss->disabled && bss->beacon_set_done) {
			struct drv_adv_ttlm_params upcoming_params;
			struct drv_adv_ttlm_params established_params;
			struct ttlm_context *ctx = &bss->mld->ttlm_ctx;
			bool send_def_mapping =
				!ctx->upcoming_ttlm.ttlm.mapping_switch_time_present;

			if (hostapd_fill_ttlm_params(&ctx->upcoming_ttlm.ttlm,
						     &ctx->established_ttlm.ttlm,
						     &upcoming_params,
						     &established_params)) {
				wpa_printf(MSG_DEBUG,
					   "Fail to fill TTLM params on non-tx bss");
				return -EINVAL;
			}
			ret =  hostapd_drv_set_advertised_ttlm_params(bss,
								      &upcoming_params,
								      &established_params,
								      send_def_mapping);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "Failed to set advertised TTLM configs for non-tx BSS");
				return -EINVAL;
			}
		}
	}

	return ret;
}


int
hostapd_offload_set_adv_ttlm_mbssid_enhanced(struct hostapd_data *hapd)
{
	struct hostapd_data *bss;
	int ret = 0;
	int i;

	for (i = 1; i < hapd->iface->num_bss; i++) {
		struct drv_adv_ttlm_params upcoming_ttlm_params, established_ttlm_params;
		bool send_default_mapping;
		struct ttlm_context *ctx;

		bss = hapd->iface->bss[i];
		if (!bss->conf->mld_ap || !bss->mld || bss->disabled ||
		    !bss->beacon_set_done)
			continue;
#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(bss->conf))
			continue;
#endif /* CONFIG_QCN_EXTN */

		ctx = &bss->mld->ttlm_ctx;
		send_default_mapping =
			!ctx->upcoming_ttlm.ttlm.mapping_switch_time_present;
		if (hostapd_fill_ttlm_params(&ctx->upcoming_ttlm.ttlm,
					     &ctx->established_ttlm.ttlm,
					     &upcoming_ttlm_params,
					     &established_ttlm_params)) {
			wpa_printf(MSG_DEBUG,
				   "Fail to fill TTLM params on non-tx bss");
			return -EINVAL;
		}

		ret =  hostapd_drv_set_advertised_ttlm_params(bss,
							      &upcoming_ttlm_params,
							      &established_ttlm_params,
							      send_default_mapping);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "Fail to set advertised TTLM configs for non-tx BSS");
			return -EINVAL;
		}
	}

	return ret;
}


int
hostapd_offload_set_advertised_ttlm(struct hostapd_data *hapd,
				    struct mlo_ttlm_ie *upcoming_ttlm,
				    struct drv_adv_ttlm_params *upcoming_ttlm_params,
				    struct drv_adv_ttlm_params *established_ttlm_params)
{
	struct hostapd_data *link_bss;

	/* Get the 6 GHz bss */
	for_each_mld_link(link_bss, hapd) {
		if (is_6ghz_freq(link_bss->iface->freq)) {
			hapd = link_bss;
			break;
		}
	}

	/* Non-Tx vaps which are part of 6GHz Tx vap inherits the T2LM IE from Tx
	 * vap. The newly configured mapping may not be applicable for non-tx vaps
	 * as they are part of some other MLD. Hence, send the T2LM WMI comamnd to
	 * non-tx vap first with the already established, ongoing T2LM of the
	 * non-tx vaps and then send the newly configured T2LM for Tx vap.
	 */
	if (is_6ghz_freq(hapd->iface->freq) &&
	    hapd->iface->conf->mbssid != MBSSID_DISABLED &&
	    hapd == hostapd_mbssid_get_tx_bss(hapd)) {
		int err;

		if (hapd->iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			err = hostapd_offload_set_adv_ttlm_multi_mbssid(hapd);
		else
			err = hostapd_offload_set_adv_ttlm_mbssid_enhanced(hapd);

		if (err)
			return err;
	}

	return hostapd_drv_set_advertised_ttlm_params(hapd,
						      upcoming_ttlm_params,
						      established_ttlm_params,
						      false);
}


static int
hostapd_fill_upcoming_established_ttlm(struct mlo_ttlm_ie *ttlm_conf,
				       struct mlo_ttlm_ie *upcoming_ttlm,
				       struct mlo_ttlm_ie *established_ttlm)
{
	struct ttlm_info *ttlm_info = &ttlm_conf->ttlm;
	struct ttlm_info *upcoming_info = &upcoming_ttlm->ttlm;
	struct ttlm_info *established_info = &established_ttlm->ttlm;

	/* If AP is advertising the new TTLM (mapping_switch_time_present true),
	 * then modify the TTLM IE and send the updated TTLM IE to lower layer. Else,
	 * populate the upcoming TTLM IE in the TTLM context and send the TTLM IE to
	 * lower layer.
	 */
	if (upcoming_info->mapping_switch_time_present) {
		upcoming_ttlm->disabled_link_bitmap &= ttlm_conf->disabled_link_bitmap;
		upcoming_info->default_link_mapping = ttlm_info->default_link_mapping;
		upcoming_info->link_mapping_size = ttlm_info->link_mapping_size;

		/* Update mapping switch time only if it is provided in the new config
		 * else use the mapping switch time from the upcoming TTLM.
		 */
		if (ttlm_info->mapping_switch_time_present) {
			upcoming_info->mapping_switch_time_present = 1;
			upcoming_info->mapping_switch_time =
				ttlm_info->mapping_switch_time;
		}

		/* Update expected duration only if it is provided in the new config
		 * else use the expected duration from upcoming TTLM.
		 */
		if (upcoming_info->expected_duration_present) {
			upcoming_info->expected_duration_present = 1;
			upcoming_info->expected_duration =
				ttlm_info->expected_duration;
		}

		if (upcoming_info->default_link_mapping) {
			/* Clear the ieee_link_map_tid if the new
			 * mapping is default mapping.
			 */
			os_memset(upcoming_info->ieee_link_map_tid, 0,
				  sizeof(uint16_t) * NUM_MAX_TIDS);
		} else {
			int i;

			/* Update the ieee_link_map_tid with the mapping
			 * from the new config.
			 */
			for (i = 0; i < NUM_MAX_TIDS; i++) {
				upcoming_info->ieee_link_map_tid[i] |=
					ttlm_info->ieee_link_map_tid[i];
			}
		}
	} else {
		os_memcpy(upcoming_ttlm, ttlm_conf, sizeof(struct mlo_ttlm_ie));
	}

	/* Update the expected duration of established TTLM IE */
	if (established_info->expected_duration_present) {
		if (upcoming_info->mapping_switch_time <
		    established_info->expected_duration) {
			established_info->expected_duration =
				upcoming_info->mapping_switch_time;
		} else {
			wpa_printf(MSG_ERROR, "TTLM: Mapping switch time(%d) of the new mapping"
				   " can not be greater than the expected duration(%d) of the"
				   " old mapping",
				   upcoming_info->mapping_switch_time,
				   established_info->expected_duration);
			os_memset(upcoming_ttlm, 0, sizeof(struct mlo_ttlm_ie));
			return -1;
		}
	}

	return 0;
}


int hostapd_send_advertised_ttlm(struct hostapd_data *hapd, struct mlo_ttlm_ie *ttlm_conf)
{
	struct drv_adv_ttlm_params upcoming_ttlm_params, established_ttlm_params;
	u16 ieee_link_id_mask = 0, link_map_value = 0;
	struct mlo_ttlm_ie *established_ttlm;
	struct mlo_ttlm_ie *upcoming_ttlm;
	struct ttlm_context *ttlm_ctx;
	struct hostapd_data *link_bss;
	bool beacon_offload;
	u32 min_dtim = 0;
	int i, j;
	int err;

	for_each_mld_link(link_bss, hapd) {
		if (!min_dtim || link_bss->conf->dtim_period < min_dtim)
			min_dtim = link_bss->conf->dtim_period;
		ieee_link_id_mask |= BIT(link_bss->mld_link_id);
	}

	if (!min_dtim) {
		wpa_printf(MSG_ERROR, "TTLM: Min DTIM cannot be 0");
		return -1;
	}

	/* Mapping switch time and expected duration should be multiple of min DITM */
	if (ttlm_conf->ttlm.mapping_switch_time &&
	    (ttlm_conf->ttlm.mapping_switch_time % min_dtim) &&
	    (ttlm_conf->ttlm.expected_duration % min_dtim)) {
		wpa_printf(MSG_ERROR, "TTLM: Mapping switch time (%d), expected duration (%d)"
			   " should be multiple of DTIM (%d)",
			   ttlm_conf->ttlm.mapping_switch_time,
			   ttlm_conf->ttlm.expected_duration,
			   min_dtim);
		return -1;
	}

	for (i = 0; i < NUM_MAX_TIDS; i++) {
		link_map_value = ttlm_conf->ttlm.ieee_link_map_tid[i];

		if ((ieee_link_id_mask & link_map_value) != link_map_value) {
			wpa_printf(MSG_ERROR, "TTLM: Invalid link mask");
			return -1;
		}

		/* Get the disabled link bitmap to update RNR IE */
		ttlm_conf->disabled_link_bitmap |= ~link_map_value;
	}

	for (j = 0; j < NUM_MAX_TIDS; j++) {
		if (ttlm_conf->ttlm.ieee_link_map_tid[j] == ieee_link_id_mask) {
			ttlm_conf->ttlm.default_link_mapping = true;
		} else {
			ttlm_conf->ttlm.default_link_mapping = false;
			break;
		}
	}

	/* The number of links in the disabled link bitmap should not be greater
	 * than the configured number of links
	 */
	ttlm_conf->disabled_link_bitmap &= ieee_link_id_mask;
	wpa_printf(MSG_INFO, "TTLM: disabled_link_bitmap:%x", ttlm_conf->disabled_link_bitmap);

	ttlm_ctx = &hapd->mld->ttlm_ctx;
	upcoming_ttlm = &ttlm_ctx->upcoming_ttlm;
	established_ttlm = &ttlm_ctx->established_ttlm;

	err = hostapd_fill_upcoming_established_ttlm(ttlm_conf, upcoming_ttlm,
						     established_ttlm);

	if (err)
		return err;

	if (upcoming_ttlm->disabled_link_bitmap == ieee_link_id_mask) {
		wpa_printf(MSG_ERROR, "TTLM: Do not send the TTLM WMI command to"
			   " disable all the links");
		goto free_upcoming_ttlm;
	}

	beacon_offload = hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_TTLM_BEACON_OFFLOAD;
	if (beacon_offload) {
		if (hostapd_fill_ttlm_params(&upcoming_ttlm->ttlm,
					     &established_ttlm->ttlm,
					     &upcoming_ttlm_params,
					     &established_ttlm_params)) {
			wpa_printf(MSG_DEBUG,
				   "TTLM: Fail to fill ttlm params for adv ttlm");
			return -EINVAL;
		}

		return hostapd_offload_set_advertised_ttlm(hapd, upcoming_ttlm,
							   &upcoming_ttlm_params,
							   &established_ttlm_params);
	} else {
		wpa_printf(MSG_ERROR, "TTLM: TTLM offload mode is not supported by the driver "
			   "& non-offload mode is currently unavailable");
		goto free_upcoming_ttlm;
	}

	return 0;

free_upcoming_ttlm:
	os_memset(upcoming_ttlm, 0, sizeof(struct mlo_ttlm_ie));
	return -1;
}


int hostapd_send_ttlm_req(struct hostapd_data *hapd, struct ttlm_ongoing_negotiation_info *ttlm,
			  struct sta_info *sta)
{
	struct hostapd_data *lhapd;
	size_t ttlm_elem_len;
	struct wpabuf *buf;
	u8 dialog_token;
	u8 *ttlm_elem;
	int ret;

	if (!ttlm)
		return -1;

	dialog_token = ++sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info.dialog_token;

	if (hostapd_build_ttlm_elem(ttlm, &ttlm_elem, &ttlm_elem_len) < 0)
		return -1;

	/* Allocate action frame buffer (3 bytes header + IE data) */
	buf = wpabuf_alloc(sizeof(u8) + sizeof(u16) + ttlm_elem_len);
	if (!buf) {
		os_free(ttlm_elem);
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_PROTECTED_EHT);
	wpabuf_put_u8(buf, WLAN_PROT_EHT_T2L_MAPPING_REQUEST);
	wpabuf_put_u8(buf, dialog_token);
	wpabuf_put_data(buf, ttlm_elem, ttlm_elem_len);

	if (hapd->mld_link_id != sta->mld_assoc_link_id) {
		for_each_mld_link(lhapd, hapd) {
			if (lhapd->mld_link_id != sta->mld_assoc_link_id)
				continue;
			hapd = lhapd;
			break;
		}
	}

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, sta->addr,
				      wpabuf_head(buf), wpabuf_len(buf));

	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "TTLM request frame is sent");
		hostapd_copy_configured_ttlm_to_sta_info(sta, hapd, ttlm, dialog_token);
	} else
		wpa_printf(MSG_ERROR, "Failed to send TTLM request frame");

	wpabuf_free(buf);
	os_free(ttlm_elem);

	return ret;
}


static void hostapd_reset_ttlm_info(struct ttlm_info *ttlm)
{
	u8 dir;

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		ttlm[dir].default_link_mapping = true;
		os_memset(ttlm[dir].ieee_link_map_tid, 0,
			  sizeof(u16) * NUM_MAX_TIDS);
	}
}


static void hostapd_copy_negotiated_ttlm_info_to_sta(struct hostapd_data *hapd,
						     struct sta_info *sta,
						     struct ttlm_ongoing_negotiation_info
						     *ongoing_ttlm)
{
	struct ttlm_prev_negotiated_info *negotiated_ttlm = NULL;
	struct ttlm_info *negotiated_ttlm_of_tids = NULL;
	struct ttlm_info *ongoing_ttlm_of_tids = NULL;
	struct ttlm_ongoing_negotiation_info *lsta_ttlm;
	struct hostapd_data *lhapd;
	struct sta_info *lsta;
	int i, dir, tid;

	negotiated_ttlm = &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info;
	negotiated_ttlm->dialog_token = ongoing_ttlm->dialog_token;

	for (i = 0; i < TTLM_DIRECTION_MAX; i++) {
		negotiated_ttlm->ttlm_info[i].direction = TTLM_DIRECTION_INVALID;
		if (ongoing_ttlm->ttlm_info[i].direction == TTLM_DIRECTION_INVALID)
			continue;

		dir = ongoing_ttlm->ttlm_info[i].direction;
		if (dir < 0 || dir >= TTLM_DIRECTION_MAX) {
			wpa_printf(MSG_ERROR, "Invalid TTLM direction index: %d", dir);
			continue;
		}

		/* Populate the ongoing TTLM info into negotiated TTLM directions UL
		 * and DL when direction is BIDI
		 */
		if (dir == TTLM_DIRECTION_BIDI) {
			for (int j = 0; j < TTLM_DIRECTION_BIDI; j++) {
				negotiated_ttlm_of_tids = &negotiated_ttlm->ttlm_info[j];
				ongoing_ttlm_of_tids = &ongoing_ttlm->ttlm_info[dir];

				if (j == TTLM_DIRECTION_DL)
					negotiated_ttlm_of_tids->direction =
						TTLM_DIRECTION_DL;
				else
					negotiated_ttlm_of_tids->direction =
						TTLM_DIRECTION_UL;

				negotiated_ttlm_of_tids->default_link_mapping =
					ongoing_ttlm_of_tids->default_link_mapping;

				for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
					negotiated_ttlm_of_tids->ieee_link_map_tid[tid] =
						ongoing_ttlm_of_tids->ieee_link_map_tid[tid];
				}

				negotiated_ttlm_of_tids->link_mapping_size =
					ongoing_ttlm_of_tids->link_mapping_size;
			}
		} else {
			negotiated_ttlm_of_tids = &negotiated_ttlm->ttlm_info[dir];
			ongoing_ttlm_of_tids = &ongoing_ttlm->ttlm_info[dir];

			negotiated_ttlm_of_tids->direction = ongoing_ttlm_of_tids->direction;
			negotiated_ttlm_of_tids->default_link_mapping =
				ongoing_ttlm_of_tids->default_link_mapping;

			for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
				negotiated_ttlm_of_tids->ieee_link_map_tid[tid] =
					ongoing_ttlm_of_tids->ieee_link_map_tid[tid];
			}

			negotiated_ttlm_of_tids->link_mapping_size =
				ongoing_ttlm_of_tids->link_mapping_size;
		}
	}

	if ((negotiated_ttlm->ttlm_info[TTLM_DIRECTION_DL].direction ==
	     TTLM_DIRECTION_DL) ||
	    (negotiated_ttlm->ttlm_info[TTLM_DIRECTION_UL].direction ==
	     TTLM_DIRECTION_UL)) {
		os_memset(&negotiated_ttlm->ttlm_info[TTLM_DIRECTION_BIDI], 0,
			  sizeof(struct ttlm_info));
		negotiated_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction =
			TTLM_DIRECTION_INVALID;
	}

	for_each_mld_link(lhapd, hapd) {
		lsta = ap_get_sta(lhapd, sta->addr);
		if (lsta && lsta->mld_info.mld_sta) {
			lsta_ttlm = &lsta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
			lsta_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_INVALID;
			lsta_ttlm->dialog_token = 0;
			hostapd_reset_ttlm_info(lsta_ttlm->ttlm_info);
		}
	}
}


static void hostapd_fill_ttlm_nl_params(struct driver_ttlm_info *driver_ttlm_info,
					struct ttlm_prev_negotiated_info *negotiated_ttlm)
{
	u8 dir_mask[3] = {BIT(TTLM_DIRECTION_DL), BIT(TTLM_DIRECTION_UL),
			  BIT(TTLM_DIRECTION_BIDI)};
	int i, dir;

	for (i = 0; i < TTLM_DIRECTION_MAX; i++) {
		if (negotiated_ttlm->ttlm_info[i].direction == TTLM_DIRECTION_INVALID)
			continue;

		dir = negotiated_ttlm->ttlm_info[i].direction;
		if (dir < 0 || dir >= ARRAY_SIZE(dir_mask)) {
			wpa_printf(MSG_ERROR, "Invalid TTLM direction: %d", dir);
			continue;
		}
		driver_ttlm_info->dir_bmap |= dir_mask[dir];

		/* As either DLINK or ULINK values can be sent via NL,
		 * when the direction is BIDI populated the ttlm info
		 * in both DL and UL directions and hence checking only
		 * UL/DL directions here to fill driver ttlm info params.
		 */
		if (dir == TTLM_DIRECTION_DL)
			os_memcpy(driver_ttlm_info->dlink,
				  negotiated_ttlm->ttlm_info[dir].ieee_link_map_tid,
				  sizeof(driver_ttlm_info->dlink));
		else if (dir == TTLM_DIRECTION_UL)
			os_memcpy(driver_ttlm_info->ulink,
				  negotiated_ttlm->ttlm_info[dir].ieee_link_map_tid,
				  sizeof(driver_ttlm_info->ulink));
	}
}


int hostapd_apply_ttlm_mapping_to_driver(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
	struct driver_ttlm_info driver_ttlm_info = {};
	int ret;

	ongoing_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
	hostapd_copy_negotiated_ttlm_info_to_sta(hapd, sta, ongoing_ttlm);
	hostapd_fill_ttlm_nl_params(&driver_ttlm_info,
				    &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info);

	ret = hostapd_drv_set_ttlm_link_mapping(hapd, &driver_ttlm_info, sta->addr);
	if (ret)
		wpa_printf(MSG_ERROR, "Failed to send ttlm params to driver");

	return ret;
}


bool hostapd_is_mapping_homogeneous(struct ttlm_ongoing_negotiation_info *ongoing_ttlm)
{
	u8 tid, i;

	for (i = 0; i < TTLM_DIRECTION_MAX; i++) {
		for (tid = 1; tid < NUM_MAX_TIDS; tid++) {
			if (ongoing_ttlm->ttlm_info[i].direction == TTLM_DIRECTION_INVALID)
				break;

			if (ongoing_ttlm->ttlm_info[i].ieee_link_map_tid[tid] !=
			    ongoing_ttlm->ttlm_info[i].ieee_link_map_tid[0]) {
				wpa_printf(MSG_DEBUG, "Mapping is not homogeneous");
				return false;
			}
		}
	}

	return true;
}


bool is_sta_ttlm_capable(struct sta_info *sta)
{
	u16 mld_sta_capa;
	int sta_ttlm_cap;

	if (!sta || !sta->mld_info.mld_sta)
		return false;

	mld_sta_capa = sta->mld_info.common_info.mld_capa;
	sta_ttlm_cap = (mld_sta_capa & EHT_ML_MLD_CAPA_TID_TO_LINK_MAP_NEG_SUPP_MSK) >> 5;
	if (!sta_ttlm_cap)
		return false;

	return true;
}


bool hostapd_is_ttlm_active(struct sta_info *sta)
{
	struct ttlm_prev_negotiated_info *negotiated_ttlm;

	if (!is_sta_ttlm_capable(sta))
		return false;

	negotiated_ttlm = &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info;

	if (negotiated_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction ==
	    TTLM_DIRECTION_BIDI)
		return !negotiated_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].default_link_mapping;

	if (negotiated_ttlm->ttlm_info[TTLM_DIRECTION_DL].direction ==
	    TTLM_DIRECTION_DL &&
	    !negotiated_ttlm->ttlm_info[TTLM_DIRECTION_DL].default_link_mapping)
		return true;

	if (negotiated_ttlm->ttlm_info[TTLM_DIRECTION_UL].direction ==
	    TTLM_DIRECTION_UL &&
	    !negotiated_ttlm->ttlm_info[TTLM_DIRECTION_UL].default_link_mapping)
		return true;

	return false;
}


int hostapd_handle_ttlm_resp(struct hostapd_data *hapd, struct sta_info *sta,
			     const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
	int ret = 0;

	if (!sta) {
		wpa_printf(MSG_ERROR, "Station is not found");
		return -1;
	}

	if (is_sta_ttlm_capable(sta) == false) {
		wpa_printf(MSG_ERROR, "%s: STA not TTLM capable", __func__);
		return -1;
	}

	ongoing_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;

	if (ongoing_ttlm->dialog_token != mgmt->u.action.u.ttlm_resp.dialog_token) {
		wpa_printf(MSG_ERROR, "TTLM dialog token mismatch: expected:%d, received:%d",
			   ongoing_ttlm->dialog_token, mgmt->u.action.u.ttlm_resp.dialog_token);
		hostapd_send_ttlm_teardown(hapd, sta);
		return -1;
	}

	ongoing_ttlm->ttlm_resp_type = mgmt->u.action.u.ttlm_resp.status_code;
	wpa_printf(MSG_DEBUG, "TTLM response received: dialog_token:%d response_code:%d",
		   ongoing_ttlm->dialog_token, ongoing_ttlm->ttlm_resp_type);

	if (ongoing_ttlm->ttlm_resp_type == TTLM_RESP_TYPE_SUCCESS)
		ret = hostapd_apply_ttlm_mapping_to_driver(hapd, sta);
	else if (ongoing_ttlm->ttlm_resp_type == TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING)
		wpa_printf(MSG_DEBUG, "Preferred mapping is suggested");
	else
		wpa_printf(MSG_DEBUG, "Denied Tid to link mapping");

	return ret;
}


static int hostapd_parse_ttlm_elem(struct hostapd_data *hapd,
				   const struct ieee80211_ttlm_elem *ttlm,
				   struct ttlm_info *ttlm_info)
{
	u8 control, tid, link_mapping_presence_ind, map_size;
	u8 *pos;
	enum ttlm_dir dir;

	if (!ttlm) {
		wpa_printf(MSG_ERROR, "IE buffer is NULL");
		return -1;
	}

	pos = (void *)ttlm->optional;
	control = ttlm->control;

	if (control & (TTLM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_MASK |
		       TTLM_CONTROL_EXPECTED_DURATION_PRESENT_MASK)) {
		wpa_printf(MSG_ERROR, "Invalid TTLM element");
		return -1;
	}

	dir = control & TTLM_CONTROL_DIRECTION_MASK;

	if (dir >= TTLM_DIRECTION_INVALID) {
		wpa_printf(MSG_ERROR, "Invalid direction");
		return -1;
	}

	ttlm_info->direction = dir;
	ttlm_info->default_link_mapping = control & TTLM_CONTROL_DEFAULT_LINK_MAPPING_MASK;

	if (ttlm_info->default_link_mapping) {
		wpa_printf(MSG_DEBUG, "Default link mapping");
		return 0;
	}

	ttlm_info->link_mapping_size = control & TTLM_CONTROL_LINK_MAPPING_SIZE_MASK;

	link_mapping_presence_ind = *pos;
	pos++;

	if (ttlm_info->link_mapping_size) {
		/* Link mapping of TIDs is 1 octet if link_mapping_size is set to 1*/
		map_size = 1;
	} else {
		/* Link mapping of TIDs is 2 octet if link_mapping_size is set to 0*/
		map_size = 2;
	}

	for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
		if (!(link_mapping_presence_ind & BIT(tid)))
			continue;

		if (map_size == 1)
			ttlm_info->ieee_link_map_tid[tid] = *pos;
		else
			ttlm_info->ieee_link_map_tid[tid] = host_to_le16(*pos);

		pos += map_size;
	}

	return 0;
}

bool is_valid_negotiated_ttlm(struct mlo_ttlm_ie *established_ttlm,
			      struct ttlm_ongoing_negotiation_info *neg_info)
{
	int dir, tid;
	struct ttlm_info *ttlm_info;

	if (established_ttlm->ttlm.default_link_mapping)
		return true;

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		ttlm_info = &neg_info->ttlm_info[dir];
		if (ttlm_info->direction == TTLM_DIRECTION_INVALID)
			continue;

		if (ttlm_info->default_link_mapping)
			continue;

		for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
			if ((established_ttlm->ttlm.ieee_link_map_tid[0] &
			     ttlm_info->ieee_link_map_tid[tid]) !=
			    ttlm_info->ieee_link_map_tid[tid])
				return false;
		}
	}

	return true;
}

void copy_established_ttlm_to_ongoing(struct ttlm_ongoing_negotiation_info *ongoing_ttlm,
				      struct hostapd_mld *mld)
{
	int dir;

	memset(ongoing_ttlm, 0, sizeof(*ongoing_ttlm));

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++)
		ongoing_ttlm->ttlm_info[dir].direction = TTLM_DIRECTION_INVALID;

	if (mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present) {
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction = TTLM_DIRECTION_BIDI;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].default_link_mapping = 0;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].mapping_switch_time_present = 0;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].mapping_switch_time = 0;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].expected_duration_present = 1;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].expected_duration =
			mld->ttlm_ctx.established_ttlm.ttlm.expected_duration;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].link_mapping_size =
			mld->ttlm_ctx.established_ttlm.ttlm.link_mapping_size;
		memcpy(ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].ieee_link_map_tid,
		       mld->ttlm_ctx.established_ttlm.ttlm.ieee_link_map_tid,
		       sizeof(ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].ieee_link_map_tid));
	} else {
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction = TTLM_DIRECTION_BIDI;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].default_link_mapping = 1;
	}
}

int hostapd_handle_ttlm_assoc_req(struct hostapd_data *hapd, const struct ieee80211_mgmt *mgmt,
				  size_t len, struct sta_info *sta, const u8 *elem,
				  size_t elem_len)
{
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
	struct ieee802_11_elems elems;
	struct hostapd_mld *mld = hapd->mld;
	struct hostapd_data *lhapd;
	struct sta_info *lsta;
	struct ttlm_info ttlm_info;
	bool homogeneous_map;
	enum ttlm_dir dir;
	int retval;
	enum ttlm_resp_type resp_type = TTLM_RESP_TYPE_SUCCESS;
	u8 i;
#ifdef CONFIG_QCN_EXTN
	u16 repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->conf->mld_ap)
		return -1;

#ifdef CONFIG_QCN_EXTN
	/* TTLM not applicable on repurposed link BSS */
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return -1;

	hostapd_get_repurposed_links_bitmap_extn(hapd, &repurposed_links);
#endif /* CONFIG_QCN_EXTN */

	/* initialize all partner stas */
	for_each_mld_link(lhapd, hapd) {
		lsta = ap_get_sta(lhapd, sta->addr);
		if (lsta && lsta->mld_info.mld_sta) {
			struct ttlm_ongoing_negotiation_info *neg;

			neg = &lsta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
			os_memset(neg, 0, sizeof(*neg));
			neg->ttlm_resp_type = -1;
			for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
				neg->ttlm_info[dir].direction = dir;
				neg->ttlm_info[dir].default_link_mapping = 1;
			}
		}
	}

	if (!hapd->conf->ttlm_enable)
		return WLAN_STATUS_REQUEST_DECLINED;

	if (ieee802_11_parse_elems(elem, elem_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_ERROR, "Could not parse assocReq from " MACSTR,
			   MAC2STR(mgmt->sa));
		return WLAN_STATUS_INVALID_IE;
	}

	if (elems.ttlm_num && !is_sta_ttlm_capable(sta)) {
		wpa_printf(MSG_ERROR,
			   "%s: STA not TTLM capable, but has ttlm elems in assoc req",
			   __func__);
		return WLAN_STATUS_REQUEST_DECLINED;
	}

	ongoing_ttlm = os_zalloc(sizeof(struct ttlm_ongoing_negotiation_info));
	if (!ongoing_ttlm) {
		wpa_printf(MSG_ERROR, "Memory allocation for ongoing_ttlm failed");
		return -1;
	}

	if (!elems.ttlm_num) {
		wpa_printf(MSG_ERROR, "No TTLM elements present");
		/* if an advertised ttlm already established, add TTLM element with the advertised
		 * mapping in assoc response frame. The ongoing_ttlm_info of sta is used while
		 * constructing the assoc response frame, hence update it with established mapping.
		 */
		if (mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present) {
			copy_established_ttlm_to_ongoing(ongoing_ttlm, mld);
			resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
			goto copy_info;
		} else {
			os_free(ongoing_ttlm);
			return -1;
		}
	}

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++)
		ongoing_ttlm->ttlm_info[dir].direction = TTLM_DIRECTION_INVALID;

	for (i = 0; i < elems.ttlm_num; i++) {
		memset(&ttlm_info, 0, sizeof(struct ttlm_info));
		retval = hostapd_parse_ttlm_elem(hapd, elems.ttlm[i], &ttlm_info);
		if (!retval && ttlm_info.direction < TTLM_DIRECTION_MAX) {
			os_memcpy(&ongoing_ttlm->ttlm_info[ttlm_info.direction],
				  &ttlm_info, sizeof(struct ttlm_info));
		} else {
			wpa_printf(MSG_ERROR, "Failed to parse TTLM IE");
			/* if an advertised ttlm already established, add TTLM element in assoc
			 * response with advertised mapping, else add default ttlm element in assoc
			 * response frame. The ongoing_ttlm_info of sta is used while constructing
			 * the assoc response frame, hence update it accordingly.
			 */
			copy_established_ttlm_to_ongoing(ongoing_ttlm, mld);
			resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
			goto copy_info;
		}
	}

	if ((ongoing_ttlm->ttlm_info[TTLM_DIRECTION_DL].direction == TTLM_DIRECTION_DL ||
	     ongoing_ttlm->ttlm_info[TTLM_DIRECTION_UL].direction == TTLM_DIRECTION_UL) &&
	    ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction == TTLM_DIRECTION_BIDI) {
		wpa_printf(MSG_DEBUG, "Both DL/UL and BIDI TTLM IEs cannot exist at same time");
		/* if an advertised ttlm already established, add TTLM element in assoc
		 * response with advertised mapping, else add default ttlm element in assoc
		 * response frame. The ongoing_ttlm_info of sta is used while constructing
		 * the assoc response frame, hence update it accordingly.
		 */
		copy_established_ttlm_to_ongoing(ongoing_ttlm, mld);
		resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
		goto copy_info;
	}

	homogeneous_map = hostapd_is_mapping_homogeneous(ongoing_ttlm);
	if (homogeneous_map == false) {
		wpa_printf(MSG_DEBUG, "Assoc request is with disjoint mapping");
		copy_established_ttlm_to_ongoing(ongoing_ttlm, mld);
		resp_type = WLAN_STATUS_DENIED_TID_TO_LINK_MAPPING;
		goto copy_info;
	}
	/* if established ttlm present and the requested negotiation is not subset of established
	 * ttlm, deny the negotiation with established ttlm element included in assoc response
	 */
	if (mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present &&
	    !is_valid_negotiated_ttlm(&mld->ttlm_ctx.established_ttlm, ongoing_ttlm)) {
		copy_established_ttlm_to_ongoing(ongoing_ttlm, mld);
		resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
	}

	/* TODO: check if assoc link is disabled in requested negotiation mapping. If yes
	 * add default TTLM element in assoc response frame.
	 */

	/* TODO: if ttlm_enable is homogeneous && user configured default_resp_code==success &&
	 * negotiated mapping is homogeneous, allow the negotiation. If any of these condition fails
	 * add default TTLM element in assoc response frame.
	 */

#ifdef CONFIG_QCN_EXTN
	if (resp_type == TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING)
		goto copy_info;

	if (!repurposed_links)
		goto copy_info;

	/* If requested negotiation includes repurposed links, deny the req */
	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		struct ttlm_info *ttlm = &ongoing_ttlm->ttlm_info[dir];
		int tid;

		if (ttlm->direction == TTLM_DIRECTION_INVALID)
			continue;

		if (ttlm->default_link_mapping)
			continue;

		for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
			if (ttlm->ieee_link_map_tid[tid] & repurposed_links) {
				copy_established_ttlm_to_ongoing(ongoing_ttlm, mld);
				resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
				wpa_printf(MSG_ERROR,
					   "Deny assoc ttlm as repurposed link requested");
				break;
			}
		}

		/* skip checking remaining directions even if one fails */
		if (resp_type == TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING)
			break;
	}
#endif /* CONFIG_QCN_EXTN */

copy_info:
	ongoing_ttlm->ttlm_resp_type = resp_type;
	hostapd_copy_configured_ttlm_to_sta_info(sta, hapd, ongoing_ttlm, 0);

	os_free(ongoing_ttlm);
	wpa_printf(MSG_DEBUG, "TTLM IE in assoc request has been parsed successfully");
	return WLAN_STATUS_SUCCESS;
}


int hostapd_send_ttlm_resp_action(struct hostapd_data *hapd,
				  struct sta_info *sta)
{
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
	struct hostapd_data *lhapd;
	size_t ttlm_elem_len;
	struct wpabuf *buf;
	u8 *ttlm_elem;
	int ret;

	buf = wpabuf_alloc(sizeof(u32) + sizeof(u8));
	if (!buf)
		return -1;

	ongoing_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
	wpabuf_put_u8(buf, WLAN_ACTION_PROTECTED_EHT);
	wpabuf_put_u8(buf, WLAN_PROT_EHT_T2L_MAPPING_RESPONSE);
	wpabuf_put_u8(buf, ongoing_ttlm->dialog_token);
	wpabuf_put_le16(buf, ongoing_ttlm->ttlm_resp_type);

	if (ongoing_ttlm->ttlm_resp_type == TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING) {
		if (hostapd_build_ttlm_elem(ongoing_ttlm, &ttlm_elem, &ttlm_elem_len) < 0 ||
		    ttlm_elem_len == 0) {
			wpabuf_free(buf);
			return -1;
		}

		if (wpabuf_resize(&buf, ttlm_elem_len) != 0) {
			os_free(ttlm_elem);
			wpabuf_free(buf);
			return -1;
		}
		wpabuf_put_data(buf, ttlm_elem, ttlm_elem_len);
		os_free(ttlm_elem);
	}

	if (hapd->mld_link_id != sta->mld_assoc_link_id) {
		for_each_mld_link(lhapd, hapd) {
			if (lhapd->mld_link_id != sta->mld_assoc_link_id)
				continue;
			hapd = lhapd;
			break;
		}
	}

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, sta->addr,
				      wpabuf_head(buf), wpabuf_len(buf));

	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "TTLM response frame is sent");
		hostapd_copy_configured_ttlm_to_sta_info(sta, hapd, ongoing_ttlm,
							 ongoing_ttlm->dialog_token);
	} else
		wpa_printf(MSG_ERROR, "Failed to send TTLM response frame");

	wpabuf_free(buf);
	return ret;

}


void hostapd_handle_ttlm_req(struct hostapd_data *hapd, struct sta_info *sta,
			     const u8 *buf, size_t len)
{
	struct hostapd_data *lhapd;
	struct sta_info *lsta;
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm, *configured_ttlm;
	struct ieee802_11_elems elems;
	struct ttlm_info ttlm_info = {};
	bool homogeneous_map;
	enum ttlm_dir dir;
	const u8 *pos;
	size_t ie_len;
	int retval, i;
	u16 enabled_links_bitmap = 0;
#ifdef CONFIG_QCN_EXTN
	u16 repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM negotiation support is disabled");
		return;
	}

	ongoing_ttlm = os_zalloc(sizeof(struct ttlm_ongoing_negotiation_info));
	if (!ongoing_ttlm) {
		wpa_printf(MSG_ERROR, "Memory allocation for ongoing_ttlm failed");
		return;
	}

	if (is_sta_ttlm_capable(sta) == false) {
		wpa_printf(MSG_ERROR, "%s: STA not TTLM capable", __func__);
		return;
	}

	ongoing_ttlm->dialog_token = mgmt->u.action.u.ttlm_req.dialog_token;
	pos = mgmt->u.action.u.ttlm_req.variable;
	ie_len = buf + len - pos;

	if (ieee802_11_parse_elems(pos, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_ERROR, "Could not parse TTLM request frame received "
			   MACSTR, MAC2STR(mgmt->sa));
		os_free(ongoing_ttlm);
		return;
	}

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++)
		ongoing_ttlm->ttlm_info[dir].direction = TTLM_DIRECTION_INVALID;

	for (i = 0; i < elems.ttlm_num; i++) {
		retval = hostapd_parse_ttlm_elem(hapd, elems.ttlm[i], &ttlm_info);
		if (!retval && ttlm_info.direction < TTLM_DIRECTION_MAX) {
			ongoing_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_SUCCESS;
			os_memcpy(&ongoing_ttlm->ttlm_info[ttlm_info.direction],
				  &ttlm_info, sizeof(struct ttlm_info));
			/* consider TID0 for now as we support only homogeneous mapping */
			enabled_links_bitmap |= ttlm_info.ieee_link_map_tid[0];
		} else {
			wpa_printf(MSG_ERROR, "Failed to parse TTLM IE");
			os_free(ongoing_ttlm);
			return;
		}
	}

	homogeneous_map = hostapd_is_mapping_homogeneous(ongoing_ttlm);
	if (homogeneous_map == false) {
		wpa_printf(MSG_DEBUG, "Request is with disjoint mapping");
		os_free(ongoing_ttlm);
		return;
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_get_repurposed_links_bitmap_extn(hapd, &repurposed_links);

	if (repurposed_links & enabled_links_bitmap) {
		wpa_printf(MSG_ERROR,
			   "Reject as TTLM req has repurposed links 0x%x",
			   repurposed_links & enabled_links_bitmap);
		os_free(ongoing_ttlm);
		return;
	}
#endif /* CONFIG_QCN_EXTN */

	/* cancel disassoc timer */
	for_each_mld_link(lhapd, hapd) {
		lsta = ap_get_sta(lhapd, sta->addr);
		if (lsta && lsta->mld_info.mld_sta &&
		    lsta->timeout_next == STA_DISASSOC_FROM_CLI) {
			eloop_cancel_timeout(ap_handle_timer, lhapd, lsta);
			if (BIT(lhapd->mld_link_id) & enabled_links_bitmap) {
				lsta->timeout_next = STA_NULLFUNC;
				eloop_register_timeout(lhapd->conf->ap_max_inactivity, 0,
						       ap_handle_timer, lhapd, lsta);
			}
			wpa_printf(MSG_DEBUG, "BTM timer cancelled for the client");
		}
	}

	if ((ongoing_ttlm->ttlm_info[TTLM_DIRECTION_DL].direction == TTLM_DIRECTION_DL ||
	     ongoing_ttlm->ttlm_info[TTLM_DIRECTION_UL].direction == TTLM_DIRECTION_UL) &&
	    ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction == TTLM_DIRECTION_BIDI) {
		wpa_printf(MSG_DEBUG, "Both DL/UL and BIDI TTLM IEs cannot exist at same time");
		os_memset(ongoing_ttlm, 0, sizeof(*ongoing_ttlm));
		for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++)
			ongoing_ttlm->ttlm_info[dir].direction = TTLM_DIRECTION_INVALID;
		ongoing_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
	}

	if (!is_valid_negotiated_ttlm(&hapd->mld->ttlm_ctx.established_ttlm,
				      ongoing_ttlm)) {
		ongoing_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
	}

	configured_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
	if (configured_ttlm->ttlm_resp_type !=
	    TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING && configured_ttlm->ttlm_resp_type !=
	    TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING) {
		os_memcpy(configured_ttlm, ongoing_ttlm,
			  sizeof(struct ttlm_ongoing_negotiation_info));
	}

	sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info.dialog_token =
		ongoing_ttlm->dialog_token;

	wpa_printf(MSG_DEBUG, "TTLM request has been parsed successfully");
	hostapd_send_ttlm_resp_action(hapd, sta);
	os_free(ongoing_ttlm);
}


int hostapd_ttlm_resp_tx_status(struct hostapd_data *hapd, struct sta_info *sta,
				int ok)
{
	int ret = 0;
	enum ttlm_resp_type status;

	if (!sta) {
		wpa_printf(MSG_ERROR, "Station is not found");
		return -1;
	}

	status = sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info.ttlm_resp_type;
	wpa_printf(MSG_DEBUG, "TTLM response: TX status: ok=%d ttlm_resp_type=%d",
		   ok, status);
	if (ok &&
	    status == TTLM_RESP_TYPE_SUCCESS) {
		ret = hostapd_apply_ttlm_mapping_to_driver(hapd, sta);
		if (ret)
			wpa_printf(MSG_ERROR, "Failed to send ttlm params to driver");
	}

	return ret;
}


int hostapd_send_ttlm_teardown(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
	struct hostapd_data *lhapd;
	struct wpabuf *buf;
	int ret;

	buf = wpabuf_alloc(sizeof(u16));
	if (!buf)
		return -1;

	ongoing_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
	wpabuf_put_u8(buf, WLAN_ACTION_PROTECTED_EHT);
	wpabuf_put_u8(buf, WLAN_PROT_EHT_T2L_MAPPING_TEARDOWN);

	ongoing_ttlm->dialog_token = 0;
	ongoing_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_INVALID;
	hostapd_reset_ttlm_info(ongoing_ttlm->ttlm_info);

	if (hapd->mld_link_id != sta->mld_assoc_link_id) {
		for_each_mld_link(lhapd, hapd) {
			if (lhapd->mld_link_id != sta->mld_assoc_link_id)
				continue;
			hapd = lhapd;
			break;
		}
	}

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, sta->addr,
				      wpabuf_head(buf), wpabuf_len(buf));

	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "TTLM teardown frame is sent");
		hostapd_copy_configured_ttlm_to_sta_info(sta, hapd, ongoing_ttlm,
							 ongoing_ttlm->dialog_token);
	} else
		wpa_printf(MSG_ERROR, "Failed to send TTLM teardown frame");

	wpabuf_free(buf);
	return ret;
}


int hostapd_ttlm_teardown_tx_status(struct hostapd_data *hapd, struct sta_info *sta, int ok)
{
	int ret = 0;

	if (!sta) {
		wpa_printf(MSG_ERROR, "Station is not found");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "TTLM teardown: TX status: ok=%d peer_mac_addr:" MACSTR, ok,
		   MAC2STR(sta->addr));
	if (ok) {
		ret = hostapd_apply_ttlm_mapping_to_driver(hapd, sta);
		if (ret)
			wpa_printf(MSG_ERROR, "Failed to send ttlm params to driver");
	}

	return ret;
}


int hostapd_handle_ttlm_teardown(struct hostapd_data *hapd, struct sta_info *sta,
				 const u8 *buf, size_t len)
{
	struct ttlm_prev_negotiated_info *negotiated_ttlm;
	struct driver_ttlm_info driver_ttlm_info;
	int ret;

	if (!hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM Negotiation support is disabled");
		return -1;
	}

	if (is_sta_ttlm_capable(sta) == false) {
		wpa_printf(MSG_ERROR, "%s: STA not TTLM capable", __func__);
		return -1;
	}

	negotiated_ttlm = &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info;
	negotiated_ttlm->dialog_token = 0;
	hostapd_reset_ttlm_info(negotiated_ttlm->ttlm_info);
	hostapd_fill_ttlm_nl_params(&driver_ttlm_info,
				    &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info);
	ret = hostapd_drv_set_ttlm_link_mapping(hapd, &driver_ttlm_info, sta->addr);
	if (ret)
		wpa_printf(MSG_ERROR, "Failed to send ttlm params to driver");

	return ret;
}
