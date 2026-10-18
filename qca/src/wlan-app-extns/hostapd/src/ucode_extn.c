// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#include "includes.h"
#include "common.h"
#include <sys/un.h>
#include "utils/includes.h"
#include "utils/ucode.h"
#include <ap/hostapd.h>
#include "ucode_extn.h"
#include "dfs_extn.h"
#include "ap/beacon.h"
#include "ap/ap_drv_ops.h"
#include "cmn.h"

#ifdef UCODE_SUPPORT
void hostapd_ucode_notify_uplink_csa(struct hostapd_iface *hapd, int event, u8 channel,
				     int freq, int csa_count, u8 new_ch_width,
				     u8 ch_seg_0, u8 ch_seg_1,
				     const dfs_nol_ie_info *nol_info)
{
	uc_value_t *val = NULL;
	uc_value_t *iface_registry = NULL;
	struct uc_vm *vm = NULL;
	s8 hw_idx = 0;

	if (event != EVENT_DFS_UPLINK_CHANNEL_SELECTED)
		return;

	if (!hapd || !hapd->ucode.idx)
		return;

	iface_registry = ucode_ap_fetch_iface_reg_extn();
	if (!iface_registry)
		return;

	vm = ucode_ap_fetch_vm_extn();
	if (!vm)
		return;

	val = wpa_ucode_registry_get(iface_registry, hapd->ucode.idx);
	if (!val)
		return;

	if (wpa_ucode_call_prepare("event"))
		return;

	uc_value_push(ucv_get(ucv_string_new(hapd->phy)));

	if (hapd->current_hw_info)
		hw_idx = hapd->current_hw_info->hw_idx;

	wpa_printf(MSG_INFO, "%s: Channel switch event with radio id %d %s",
		   __func__, hw_idx, hapd->phy);
	uc_value_push(ucv_get(ucv_int64_new(hw_idx)));

	uc_value_push(ucv_get(val));
	uc_value_push(ucv_get(ucv_string_new(event_to_string(event))));
	val = ucv_object_new(vm);
	uc_value_push(ucv_get(val));

	ucv_object_add(val, "frequency", ucv_int64_new(freq));
	ucv_object_add(val, "channel", ucv_int64_new(channel));
	ucv_object_add(val, "csa_count", ucv_int64_new(csa_count));
	ucv_object_add(val, "new_ch_width", ucv_int64_new(new_ch_width));
	ucv_object_add(val, "ch_seg_0", ucv_int64_new(ch_seg_0));
	ucv_object_add(val, "ch_seg_1", ucv_int64_new(ch_seg_1));
	ucv_object_add(val, "cac_abort",
		       ucv_int64_new(hapd->iface_extn.cac_abort ? 1 : 0));

	if (nol_info) {
		uc_value_t *nol_entry = ucv_object_new(vm);

		if (nol_entry) {
			ucv_object_add(nol_entry, "freq",
				       ucv_int64_new(nol_info->freq));
			ucv_object_add(nol_entry, "bandwidth",
				       ucv_int64_new(nol_info->bandwidth));
			ucv_object_add(nol_entry, "bitmap",
				       ucv_int64_new(nol_info->subchan_bitmap));
			ucv_object_add(val, "nol_channel", nol_entry);
			wpa_printf(MSG_INFO,
				   "%s: NOL entry freq=%u bw=%u bitmap=0x%04x",
				   __func__, nol_info->freq, nol_info->bandwidth,
				   nol_info->subchan_bitmap);
		}
	}

	ucv_put(wpa_ucode_call(5));
	ucv_gc(vm);
}

void hostapd_ucode_trigger_bhsta_disconnect(struct hostapd_iface *hapd)
{
	uc_value_t *val;
	s8 hw_idx = 0;
	uc_value_t *iface_registry = NULL;
	struct uc_vm *vm = NULL;

	if (!hapd || !hapd->ucode.idx)
		return;

	iface_registry = ucode_ap_fetch_iface_reg_extn();
	if (!iface_registry)
		return;

	vm = ucode_ap_fetch_vm_extn();
	if (!vm)
		return;

	val = wpa_ucode_registry_get(iface_registry, hapd->ucode.idx);

	if (!val) {
		wpa_printf(MSG_ERROR, "%s: ucode ref not found", __func__);
		return;
	}

	if (wpa_ucode_call_prepare("disconnect_backhaul"))
		return;

	uc_value_push(ucv_get(ucv_string_new(hapd->phy)));

	if (hapd->current_hw_info)
		hw_idx = hapd->current_hw_info->hw_idx;

	wpa_printf(MSG_INFO, "%s: disconnect backhaul event with radio id %d %s \n",
		   __func__, hw_idx, hapd->phy);
	uc_value_push(ucv_get(ucv_int64_new(hw_idx)));
	uc_value_push(ucv_get(val));
	ucv_put(wpa_ucode_call(3));
	ucv_gc(vm);
}


#else
void hostapd_ucode_notify_uplink_csa(struct hostapd_iface *hapd, int event, u8 channel,
				     int freq, int csa_count, u8 new_ch_width,
				     u8 ch_seg_0, u8 ch_seg_1,
				     const dfs_nol_ie_info *nol_info)
{
}

void hostapd_ucode_trigger_bhsta_disconnect(struct hostapd_iface *hapd)
{
}
#endif /* UCODE_SUPPORT */
