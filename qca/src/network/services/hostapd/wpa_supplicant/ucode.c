#include "utils/includes.h"
#include "utils/common.h"
#include "utils/ucode.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "wpa_supplicant_i.h"
#include "wps_supplicant.h"
#include "bss.h"
#include "scan.h"
#include "ucode.h"
#include "driver_i.h"
#include "sme.h"
#include "config.h"
#include "ap.h"
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/cmn.h"
#endif
#include "eloop.h"

static struct wpa_global *wpa_global;
static uc_resource_type_t *global_type, *iface_type;
static uc_value_t *global, *iface_registry;
static uc_vm_t *vm;

void wpas_ucode_update_radio_id(const char *ifname, int id) {
	struct wpa_supplicant *wpa_s = NULL;

	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next)
		if (!strcmp(wpa_s->ifname, ifname))
			break;

	if (!wpa_s) {
		wpa_printf(MSG_ERROR, "%s: wpa_s data not found", __func__);
		return;
	}

	if (id == -1)
		wpa_s->ucode.radio_bitmap = 0;
	else
		wpa_s->ucode.radio_bitmap |= BIT(id);

	wpa_printf(MSG_INFO, "%s: updated radio_bitmap %d for %s", __func__,
		   wpa_s->ucode.radio_bitmap, wpa_s->ifname);
}

static uc_value_t *
wpas_ucode_iface_get_uval(struct wpa_supplicant *wpa_s)
{
	uc_value_t *val;

	if (wpa_s->ucode.idx)
		return wpa_ucode_registry_get(iface_registry, wpa_s->ucode.idx);

	val = uc_resource_new(iface_type, wpa_s);
	wpa_s->ucode.idx = wpa_ucode_registry_add(iface_registry, val);

	return val;
}

static bool
is_ml_interface_already_created(const char *ifname) {
	struct wpa_supplicant *wpa_s;
	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next)
		if (!strcmp(wpa_s->ifname, ifname))
			return true;
	return false;
}

static void
wpas_ucode_update_interfaces(void)
{
	uc_value_t *ifs = ucv_object_new(vm);
	struct wpa_supplicant *wpa_s;

	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next)
		ucv_object_add(ifs, wpa_s->ifname, ucv_get(wpas_ucode_iface_get_uval(wpa_s)));

	ucv_object_add(ucv_prototype_get(global), "interfaces", ucv_get(ifs));
	ucv_gc(vm);
}

static bool
uc_wpas_object_get_int(uc_value_t *obj, const char *name, int *out)
{
	uc_value_t *val;

	if (!obj || !out)
		return false;

	val = ucv_object_get(obj, name, NULL);
	if (!val || ucv_type(val) != UC_INTEGER)
		return false;

	*out = (int) ucv_int64_get(val);
	return true;
}

static bool
uc_wpas_object_get_bool(uc_value_t *obj, const char *name, bool *out)
{
	uc_value_t *val;

	if (!obj || !out)
		return false;

	val = ucv_object_get(obj, name, NULL);
	if (!val)
		return false;

	if (ucv_type(val) == UC_BOOLEAN) {
		*out = ucv_boolean_get(val);
		return true;
	}

	if (ucv_type(val) == UC_INTEGER) {
		*out = !!ucv_int64_get(val);
		return true;
	}

	return false;
}

#define WPAS_MESH_CSA_2_TU_MAX		127
#define WPAS_MESH_CSA_100_TU_FLAG	0x80
#define WPAS_MESH_CSA_100_TU_MAX	127

static u8
wpas_mesh_encode_csa_count(unsigned int switch_time_tu)
{
	unsigned int count;

	if (!switch_time_tu)
		return 1;

	if (switch_time_tu <= WPAS_MESH_CSA_2_TU_MAX * 2) {
		count = (switch_time_tu + 1) / 2;
		return count ? count : 1;
	}

	count = (switch_time_tu + 99) / 100;
	if (count > WPAS_MESH_CSA_100_TU_MAX)
		count = WPAS_MESH_CSA_100_TU_MAX;

	return WPAS_MESH_CSA_100_TU_FLAG | count;
}

static u8
wpas_mesh_csa_count_from_ap(unsigned int ap_csa_count,
			    unsigned int ap_beacon_int)
{
	unsigned int switch_time_tu;

	if (!ap_csa_count || !ap_beacon_int) {
		wpa_printf(MSG_DEBUG,
			   "mesh: invalid CSA parameters (count=%u, beacon_int=%u), using default",
			   ap_csa_count, ap_beacon_int);
		return ap_csa_count ? ap_csa_count : 5;
	}

	switch_time_tu = ap_csa_count * ap_beacon_int;

	return wpas_mesh_encode_csa_count(switch_time_tu);
}

void wpas_ucode_add_bss(struct wpa_supplicant *wpa_s)
{
	if (wpa_ucode_call_prepare("iface_add"))
		return;

	uc_value_push(ucv_get(ucv_string_new(wpa_s->ifname)));
	uc_value_push(ucv_get(wpas_ucode_iface_get_uval(wpa_s)));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

void wpas_ucode_free_bss(struct wpa_supplicant *wpa_s)
{
	uc_value_t *val;

	val = wpa_ucode_registry_remove(iface_registry, wpa_s->ucode.idx);
	if (!val)
		return;

	wpa_s->ucode.idx = 0;
	if (wpa_ucode_call_prepare("iface_remove"))
		return;

	uc_value_push(ucv_get(ucv_string_new(wpa_s->ifname)));
	uc_value_push(ucv_get(val));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

#ifdef CONFIG_QCN_EXTN
/**
 * wpas_ucode_update_pre_connect_state - Notify ucode about WPA_PRE_CONNECT
 * @wpa_s: Pointer to wpa_supplicant interface
 *
 * Send per-link (MLD) or legacy channel parameters and DFS information to
 * the ucode runtime when entering WPA_PRE_CONNECT state in Independent
 * Repeater mode. This allows ucode scripts to coordinate channel
 * selection and pre-connection behavior across links.
 */
void wpas_ucode_update_pre_connect_state(struct wpa_supplicant *wpa_s)
{
	const char *state;
	uc_value_t *val;
	uc_value_t *info;
	struct wpa_bss *bss = NULL;
	u8 i;
	u8 op_class, channel;
	int center_freq1 = 0, center_freq2 = 0;
	int offset_mhz = 0;
	int sec_chan_offset;
	s8 hw_idx;
	bool is_dfs = false;
	vap_type_t vap_type;

	val = wpa_ucode_registry_get(iface_registry, wpa_s->ucode.idx);
	if (!val)
		return;

#ifdef CONFIG_MESH
	vap_type = wpa_s->ifmsh ? VAP_TYPE_MESH : VAP_TYPE_STA;
#else
	vap_type = VAP_TYPE_STA;
#endif

	if (wpa_s->cache_cwork && wpa_s->cache_cwork->bss) {
		bss = wpa_s->cache_cwork->bss;
		if (!is_zero_ether_addr(bss->mld_addr)) {
			for_each_link(bss->valid_links, i) {
				if (bss->mld_links[i].freq == 0)
					continue;

				/* Reset per-link before computing center freqs */
				center_freq1 = 0;
				center_freq2 = 0;
				op_class = 0;
				channel = 0;
				sec_chan_offset = 0;
				ieee80211_freq_to_channel_ext(bss->mld_links[i].freq,
							      0, 1, &op_class, &channel);

				if (bss->mld_links[i].freq >= 2412 && bss->mld_links[i].freq <= 2472) {
					if (bss->mld_links[i].width == CHAN_WIDTH_40) {
						offset_mhz = (bss->mld_links[i].freq <= 2442) ? 10 : -10;
						center_freq1 = bss->mld_links[i].freq + offset_mhz;
					} else {
						center_freq1 = bss->mld_links[i].freq;
					}
				} else if (bss->mld_links[i].width == CHAN_WIDTH_20_NOHT ||
					   bss->mld_links[i].width == CHAN_WIDTH_20) {
					/*
					 * A stale VHT/HE Operation center freq
					 * index from the BSS entry must not be
					 * used once the operating width has
					 * narrowed to 20 MHz, or the resulting
					 * center_freq1 mismatches freq/width and
					 * the kernel rejects the channel
					 * switch with -EINVAL.
					 */
					center_freq1 = bss->mld_links[i].freq;
				} else if (bss->mld_links[i].width == CHAN_WIDTH_160 ||
					   bss->mld_links[i].width == CHAN_WIDTH_320) {
					center_freq1 = ieee80211_chan_to_freq(NULL, op_class,
									      bss->mld_links[i].center_freq2_idx);
				} else {
					center_freq1 = ieee80211_chan_to_freq(NULL, op_class,
									      bss->mld_links[i].center_freq1_idx);
					center_freq2 = ieee80211_chan_to_freq(NULL, op_class,
									      bss->mld_links[i].center_freq2_idx);
				}

				if (bss->mld_links[i].width == CHAN_WIDTH_UNKNOWN ||
				    center_freq1 <= 0) {
					wpa_printf(MSG_INFO,
						   "%s: skip PRE_CONNECT notify for MLO link %d due to incomplete "
						   "chandef freq=%d width=%d cf1_idx=%u cf2_idx=%u cf1=%d cf2=%d",
						   __func__, i, bss->mld_links[i].freq,
						   bss->mld_links[i].width,
						   bss->mld_links[i].center_freq1_idx,
						   bss->mld_links[i].center_freq2_idx,
						   center_freq1, center_freq2);
					continue;
				}

				if (wpas_link_uses_nol_channel_extn(wpa_s,
								    bss->mld_links[i].freq,
								    bss->mld_links[i].width,
								    center_freq1, center_freq2)) {
					wpa_printf(MSG_DEBUG,
						   "NOL: MLO pre-connect link %d freq=%d in NOL - skip",
						   i, bss->mld_links[i].freq);
					continue;
				}

				if (wpa_ucode_call_prepare("pre_connect_state"))
					return;

				state = wpa_supplicant_state_txt(wpa_s->wpa_state);
				uc_value_push(ucv_get(ucv_string_new(wpa_s->ifname)));
				hw_idx = wpa_get_hw_idx_by_freq(wpa_s, bss->mld_links[i].freq);
				if (hw_idx == -1)
					hw_idx = 0;
				uc_value_push(ucv_get(ucv_int64_new(hw_idx)));
				uc_value_push(ucv_get(val));
				uc_value_push(ucv_get(ucv_string_new(state)));
				info = ucv_object_new(vm);
				uc_value_push(ucv_get(info));
				uc_value_push(ucv_get(ucv_int64_new(vap_type)));
				ucv_object_add(info, "frequency",
					       ucv_int64_new(bss->mld_links[i].freq));
				ucv_object_add(info, "chan_width",
					       ucv_int64_new(bss->mld_links[i].width));
				ucv_object_add(info, "center_freq1",
					       ucv_int64_new(center_freq1));
				ucv_object_add(info, "center_freq2",
					       ucv_int64_new(center_freq2));
				ucv_object_add(info, "punct_bitmap",
				ucv_int64_new(bss->mld_links[i].punc_bitmap));
#ifdef CONFIG_QCN_EXTN
				is_dfs = wpas_ucode_is_dfs_chandef(bss->mld_links[i].freq,
								   bss->mld_links[i].width,
								   center_freq1,
								   center_freq2);
				ucv_object_add(info, "is_dfs", ucv_boolean_new(is_dfs));
#endif /* CONFIG_QCN_EXTN */
				ucv_object_add(info, "mcst",
					       ucv_int64_new(wpa_bss_get_mld_link_mcst_extn(bss, i)));
				sec_chan_offset = compute_sec_channel_offset_extn(bss->mld_links[i].freq,
										  center_freq1,
										  bss->mld_links[i].width);
				ucv_object_add(info, "sec_chan_offset",
					       ucv_int64_new(sec_chan_offset));
				wpa_printf(MSG_INFO, "%s: MLO i = %d state = %s"
					   "ifname = %s freq = %d center_freq1 = %d center_freq2 = %d"
					   "width = %d is_dfs = %d punc_bitmap = %d, sec_chan_offset = %d",
					   __func__, i, state, wpa_s->ifname, bss->mld_links[i].freq,
					   center_freq1, center_freq2, bss->mld_links[i].width, is_dfs,
					   bss->mld_links[i].punc_bitmap, sec_chan_offset);
				wpa_s->pre_connect_cnt++;
				ucv_put(wpa_ucode_call(6));
				ucv_gc(vm);
			}
		} else {
			if (wpa_ucode_call_prepare("pre_connect_state"))
				return;
			state = wpa_supplicant_state_txt(wpa_s->wpa_state);
			uc_value_push(ucv_get(ucv_string_new(wpa_s->ifname)));
			hw_idx = wpa_get_hw_idx_by_freq(wpa_s, bss->freq);
			if (hw_idx == -1)
				hw_idx = 0;
			uc_value_push(ucv_get(ucv_int64_new(hw_idx)));
			uc_value_push(ucv_get(val));
			uc_value_push(ucv_get(ucv_string_new(state)));
			info = ucv_object_new(vm);
			uc_value_push(ucv_get(info));
			uc_value_push(ucv_get(ucv_int64_new(vap_type)));
			ucv_object_add(info, "frequency",
				       ucv_int64_new(bss->freq));
			ucv_object_add(info, "chan_width",
				       ucv_int64_new(bss->max_cw));
			ieee80211_freq_to_channel_ext(bss->freq, 0, 1, &op_class, &channel);

			if (bss->freq >= 2412 && bss->freq <= 2472) {
				if (bss->max_cw == CHAN_WIDTH_40) {
					offset_mhz = (channel <= 2442) ? 10 : -10;
					center_freq1 = bss->freq + offset_mhz;
				} else {
					center_freq1 = bss->freq;
				}
			} else if (bss->max_cw == CHAN_WIDTH_20_NOHT ||
				   bss->max_cw == CHAN_WIDTH_20) {
				/*
				 * A stale VHT/HE Operation center freq index
				 * from the BSS entry must not be used once the
				 * operating width has narrowed to 20 MHz, or
				 * the resulting center_freq1 mismatches
				 * freq/width and the kernel rejects the
				 * channel switch with -EINVAL.
				 */
				center_freq1 = bss->freq;
			} else if (bss->max_cw == CHAN_WIDTH_160 ||
				   bss->max_cw == CHAN_WIDTH_320) {
				center_freq1 = ieee80211_chan_to_freq(NULL, op_class,
								      bss->center_freq2_idx);
			} else {
				center_freq1 = ieee80211_chan_to_freq(NULL, op_class,
								      bss->center_freq1_idx);
				center_freq2 = ieee80211_chan_to_freq(NULL, op_class,
								      bss->center_freq2_idx);
			}
			ucv_object_add(info, "center_freq1",
				       ucv_int64_new(center_freq1));
			ucv_object_add(info, "center_freq2",
				       ucv_int64_new(center_freq2));
			ucv_object_add(info, "punct_bitmap",
				       ucv_int64_new(bss->punc_bitmap));
#ifdef CONFIG_QCN_EXTN
	is_dfs = wpas_ucode_is_dfs_chandef(bss->freq,
							   bss->max_cw,
							   center_freq1,
							   center_freq2);
			ucv_object_add(info, "is_dfs",
				       ucv_boolean_new(is_dfs));
#endif /* CONFIG_QCN_EXTN */
			sec_chan_offset = compute_sec_channel_offset_extn(bss->freq,
									  center_freq1,
									  bss->max_cw);
			ucv_object_add(info, "sec_chan_offset",
				       ucv_int64_new(sec_chan_offset));
			wpa_printf(MSG_INFO, "%s: Non-MLO state = %s ifname = %s"
				   "freq = %d center_freq1 = %d center_freq2 = %d width = %d"
				   "is_dfs = %d punc_bitmap = %d, sec_chan_offset = %d",
				   __func__, state, wpa_s->ifname, bss->freq, center_freq1,
				   center_freq2, bss->max_cw, is_dfs, bss->punc_bitmap, sec_chan_offset);
			wpa_s->pre_connect_cnt++;
			ucv_put(wpa_ucode_call(6));
			ucv_gc(vm);
		}
	}
	return;
}
#endif

void wpas_ucode_update_state(struct wpa_supplicant *wpa_s)
{
	const char *state;
	uc_value_t *val;
	vap_type_t vap_type;

	wpa_printf(MSG_INFO, "%s: radio_bitmap:%d", __func__,wpa_s->ucode.radio_bitmap);

#ifdef CONFIG_QCN_EXTN
	if (wpa_s && (wpa_s->wpa_state == WPA_PRE_CONNECT)) {
		wpas_ucode_update_pre_connect_state(wpa_s);
		return;
	}
#endif

	val = wpa_ucode_registry_get(iface_registry, wpa_s->ucode.idx);
	if (!val)
		return;

	if (wpa_ucode_call_prepare("state"))
		return;

	state = wpa_supplicant_state_txt(wpa_s->wpa_state);
#ifdef CONFIG_MESH
	vap_type = wpa_s->ifmsh ? VAP_TYPE_MESH : VAP_TYPE_STA;
#else
	vap_type = VAP_TYPE_STA;
#endif
	uc_value_push(ucv_get(ucv_string_new(wpa_s->ifname)));
	uc_value_push(ucv_get(ucv_int64_new(wpa_s->ucode.radio_bitmap)));
	uc_value_push(ucv_get(val));
	uc_value_push(ucv_get(ucv_string_new(state)));
	uc_value_push(ucv_get(ucv_int64_new(vap_type)));
	ucv_put(wpa_ucode_call(5));
	ucv_gc(vm);
}

void wpas_ucode_event(struct wpa_supplicant *wpa_s, int event, union wpa_event_data *data)
{
	uc_value_t *val;
	s8 hw_idx;
	vap_type_t vap_type;
#ifdef CONFIG_QCN_EXTN
	bool is_dfs = false;
	const char *wpa_state = NULL;
#endif

	if (!((event == EVENT_CH_SWITCH_STARTED) || (event == EVENT_LINK_CH_SWITCH_STARTED)))
		return;

	val = wpa_ucode_registry_get(iface_registry, wpa_s->ucode.idx);
	if (!val)
		return;

	if (wpa_ucode_call_prepare("event"))
		return;

#ifdef CONFIG_MESH
	vap_type = wpa_s->ifmsh ? VAP_TYPE_MESH : VAP_TYPE_STA;
#else
	vap_type = VAP_TYPE_STA;
#endif
	uc_value_push(ucv_get(ucv_string_new(wpa_s->ifname)));

	hw_idx = wpa_get_hw_idx_by_freq(wpa_s, data->ch_switch.freq);
	if (hw_idx == -1)
		hw_idx = 0;

	wpa_printf(MSG_INFO, "%s: Channel switch event with radio id %d \n", __func__,hw_idx);
	uc_value_push(ucv_get(ucv_int64_new(hw_idx)));

	uc_value_push(ucv_get(val));
	uc_value_push(ucv_get(ucv_string_new(event_to_string(event))));
	val = ucv_object_new(vm);
	uc_value_push(ucv_get(val));
	uc_value_push(ucv_get(ucv_int64_new(vap_type)));
#ifdef CONFIG_QCN_EXTN
	is_dfs = wpas_ucode_is_dfs_chandef(data->ch_switch.freq,
					   data->ch_switch.ch_width,
					   data->ch_switch.cf1,
					   data->ch_switch.cf2);
	wpa_state = wpa_supplicant_state_txt(wpa_s->wpa_state);
#endif

	if ((event == EVENT_CH_SWITCH_STARTED) || (event == EVENT_LINK_CH_SWITCH_STARTED)) {
		ucv_object_add(val, "csa_count", ucv_int64_new(data->ch_switch.count));
		ucv_object_add(val, "frequency", ucv_int64_new(data->ch_switch.freq));
		ucv_object_add(val, "chan_width", ucv_int64_new(data->ch_switch.ch_width));
		ucv_object_add(val, "sec_chan_offset", ucv_int64_new(data->ch_switch.ch_offset));
		ucv_object_add(val, "center_freq1", ucv_int64_new(data->ch_switch.cf1));
		ucv_object_add(val, "center_freq2", ucv_int64_new(data->ch_switch.cf2));
		ucv_object_add(val, "link_id", ucv_int64_new(data->ch_switch.link_id));
		ucv_object_add(val, "punct_bitmap", ucv_int64_new(data->ch_switch.punct_bitmap));
#ifdef CONFIG_QCN_EXTN
		ucv_object_add(val, "is_dfs", ucv_boolean_new(is_dfs));
		ucv_object_add(val, "wpa_state", ucv_string_new(wpa_state));
		ucv_object_add(val, "mcst", ucv_int64_new(data->ch_switch.mcst));
#endif
	}

#ifdef CONFIG_QCN_EXTN
	wpa_printf(MSG_INFO, "%s: freq = %d is_dfs = %d wpa_state = %s mcst = %u", __func__,
		   data->ch_switch.freq, is_dfs, wpa_state, data->ch_switch.mcst);

	if (wpa_s->conf->ind_rptr && is_dfs &&
	    IS_CSH_IGNORE_CSA_DFS_ENABLED(wpa_s->conf->cswopts) &&
	    wpa_s->wpa_state != WPA_PRE_CONNECT) {
		wpa_s->hold_scan_csa = true;
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
	}
#endif

	ucv_put(wpa_ucode_call(6));
	ucv_gc(vm);
}

static uc_value_t *
uc_wpas_add_iface(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *info = uc_fn_arg(0);
	uc_value_t *driver = ucv_object_get(info, "driver", NULL);
	uc_value_t *ifname = ucv_object_get(info, "iface", NULL);
	uc_value_t *bridge = ucv_object_get(info, "bridge", NULL);
	uc_value_t *config = ucv_object_get(info, "config", NULL);
	uc_value_t *ctrl = ucv_object_get(info, "ctrl", NULL);
	uc_value_t *mld    = ucv_object_get(info, "mld", NULL);
	uc_value_t *radio_id = uc_fn_arg(1);
	struct wpa_interface iface;
	int id;
	int ret = -1;
	const char *mld_name;

	if (ucv_type(info) != UC_OBJECT)
		goto out;

	if (ucv_type(radio_id) != UC_INTEGER) {
		wpa_printf(MSG_ERROR, "%s: failed to fetch radio_id", __func__);
		goto out;
	}

        id = ucv_int64_get(radio_id);

	iface = (struct wpa_interface){
		.driver = "nl80211",
		.ifname = ucv_string_get(ifname),
		.bridge_ifname = ucv_string_get(bridge),
		.confname = ucv_string_get(config),
		.ctrl_interface = ucv_string_get(ctrl),
	};

	if (driver) {
		const char *drvname;
		if (ucv_type(driver) != UC_STRING)
			goto out;

		iface.driver = NULL;
		drvname = ucv_string_get(driver);
		for (int i = 0; wpa_drivers[i]; i++) {
			if (!strcmp(drvname, wpa_drivers[i]->name))
				iface.driver = wpa_drivers[i]->name;
		}

		if (!iface.driver)
			goto out;
	}

	if (!iface.ifname || !iface.confname)
		goto out;

	if (mld) {
		mld_name = ucv_string_get(mld);
		wpa_printf(MSG_INFO, "%s: mld is %s", __func__, mld_name);
		if (is_ml_interface_already_created(iface.ifname)) {
			wpa_printf(MSG_INFO, "going to update interface");
			ret = 0;
			goto update;
		}
	}

	ret = wpa_supplicant_add_iface(wpa_global, &iface, 0) ? 0 : -1;
	wpas_ucode_update_interfaces();

update:
	wpas_ucode_update_radio_id(iface.ifname, id);

out:
	return ucv_int64_new(ret);
}

static uc_value_t *
uc_wpas_remove_iface(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = NULL;
	uc_value_t *ifname_arg = uc_fn_arg(0);
	const char *ifname = ucv_string_get(ifname_arg);
	int ret = -1;

	if (!ifname)
		goto out;

	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next)
		if (!strcmp(wpa_s->ifname, ifname))
			break;

	if (!wpa_s)
		goto out;

	ret = wpa_supplicant_remove_iface(wpa_global, wpa_s, 0);
	wpas_ucode_update_interfaces();

out:
	return ucv_int64_new(ret);
}

static int wpas_get_sec_chan(struct wpa_bss *bss)
{
	int sec_chan = 0;
	const u8 *ie;

	ie = wpa_bss_get_ie(bss, WLAN_EID_HT_OPERATION);
	if (ie && ie[1] >= 2) {
		const struct ieee80211_ht_operation *ht_oper;
		int sec;

		ht_oper = (const void *) (ie + 2);
		sec = ht_oper->ht_param & HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK;
		if (sec == HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE)
			sec_chan = 1;
		else if (sec == HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW)
			sec_chan = -1;
	}
	return sec_chan;
}

static uc_value_t *
uc_wpas_iface_status(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = uc_fn_thisval("wpas.iface");
	struct wpa_bss *bss;
	uc_value_t *ret, *val;
	uc_value_t *radio_id = uc_fn_arg(0);
	s8 hw_idx;
	int freq, sec_chan, i, poll_ret = 0;
#ifdef CONFIG_QCN_EXTN
	bool is_dfs = false;
#endif
	struct wpa_signal_info si = {0};
	struct wpa_mlo_signal_info mlo_si = {0};

	if (!wpa_s)
		return NULL;

	ret = ucv_object_new(vm);

	val = ucv_string_new(wpa_supplicant_state_txt(wpa_s->wpa_state));
	ucv_object_add(ret, "state", ucv_get(val));

	hw_idx = ucv_int64_get(radio_id);

	if (wpa_s->wpa_state == WPA_COMPLETED && wpa_s->valid_links) {
		for_each_link(wpa_s->valid_links, i) {
			freq = wpa_s->links[i].freq;
			if (hw_idx == wpa_get_hw_idx_by_freq(wpa_s, freq)) {
#ifdef CONFIG_QCN_EXTN
				u32 mcst = wpas_ucode_get_link_mcst_extn(wpa_s, i);
#endif
				ucv_object_add(ret, "frequency", ucv_int64_new(freq));
				sec_chan = wpas_get_sec_chan(wpa_s->links[i].bss);
				ucv_object_add(ret, "sec_chan_offset", ucv_int64_new(sec_chan));
#ifdef CONFIG_QCN_EXTN
				ucv_object_add(ret, "mcst", ucv_int64_new(mcst));
#endif
				if ((poll_ret = wpa_drv_mlo_signal_poll(wpa_s,
									&mlo_si)) == 0) {
					if (mlo_si.links[i].chanwidth != CHAN_WIDTH_UNKNOWN) {
						ucv_object_add(ret, "chan_width", ucv_int64_new(mlo_si.links[i].chanwidth));
						ucv_object_add(ret, "center_freq1", ucv_int64_new(mlo_si.links[i].center_frq1));
						ucv_object_add(ret, "center_freq2", ucv_int64_new(mlo_si.links[i].center_frq2));
						ucv_object_add(ret, "punct_bitmap", ucv_int64_new(mlo_si.links[i].punct_bitmap));
#ifdef CONFIG_QCN_EXTN
						is_dfs = wpas_ucode_is_dfs_chandef(freq,
										   mlo_si.links[i].chanwidth,
										   mlo_si.links[i].center_frq1,
										   mlo_si.links[i].center_frq2);
						ucv_object_add(ret, "is_dfs", ucv_boolean_new(is_dfs));
#endif
					}
				} else {
					wpa_printf(MSG_INFO,
						   "%s: MLO_SIGNAL_POLL failed ifname=%s hw_idx=%hhd link_id=%d ret=%d",
						   __func__, wpa_s->ifname, hw_idx, i,
						   poll_ret);
					ucv_put(ret);
					return NULL;
				}
			}
		}
#ifdef CONFIG_QCN_EXTN
		if (wpa_s->conf->ind_rptr && !ucv_object_get(ret, "frequency", NULL)) {
			wpa_printf(MSG_ERROR,
				   "%s: hw_idx=%hhd not found in any MLO link (valid_links=0x%x)",
				   __func__, hw_idx, wpa_s->valid_links);
			ucv_put(ret);
			return NULL;
		}
#endif
		goto out;
	}

	bss = wpa_s->current_bss;
	if (bss) {
		if (wpa_s->num_multi_hws &&
		    hw_idx != wpa_get_hw_idx_by_freq(wpa_s, bss->freq)) {
			wpa_printf(MSG_ERROR, "%s hw_idx=%hhd is not compatible with freq=%d \n",
				   __func__, hw_idx, bss->freq);
			goto out;
		}
		sec_chan = wpas_get_sec_chan(bss);
		ucv_object_add(ret, "sec_chan_offset", ucv_int64_new(sec_chan));
		ucv_object_add(ret, "frequency", ucv_int64_new(bss->freq));
		if (wpa_s->wpa_state == WPA_COMPLETED &&
		    (poll_ret = wpa_drv_signal_poll(wpa_s, &si)) == 0) {
			if (si.chanwidth != CHAN_WIDTH_UNKNOWN) {
				ucv_object_add(ret, "chan_width",
					       ucv_int64_new(si.chanwidth));
				ucv_object_add(ret, "center_freq1",
					       ucv_int64_new(si.center_frq1));
				ucv_object_add(ret, "center_freq2",
					       ucv_int64_new(si.center_frq2));
				ucv_object_add(ret, "punct_bitmap", ucv_int64_new(si.punct_bitmap));
#ifdef CONFIG_QCN_EXTN
				is_dfs = wpas_ucode_is_dfs_chandef(bss->freq,
								   si.chanwidth,
								   si.center_frq1,
								   si.center_frq2);
				ucv_object_add(ret, "is_dfs", ucv_boolean_new(is_dfs));
#endif
			}
		} else if (wpa_s->wpa_state == WPA_COMPLETED) {
			wpa_printf(MSG_INFO,
				   "%s: SIGNAL_POLL failed ifname=%s hw_idx=%hhd freq=%d ret=%d",
				   __func__, wpa_s->ifname, hw_idx, bss->freq,
				   poll_ret);
			ucv_put(ret);
			return NULL;
		}
	}

#ifdef CONFIG_MESH
	if (wpa_s->ifmsh) {
		struct hostapd_iface *ifmsh = wpa_s->ifmsh;

		ucv_object_add(ret, "sec_chan_offset", ucv_int64_new(ifmsh->conf->secondary_channel));
		ucv_object_add(ret, "frequency", ucv_int64_new(ifmsh->freq));
		if (wpa_s->wpa_state == WPA_COMPLETED &&
		    wpa_drv_signal_poll(wpa_s, &si) == 0) {
			if (si.chanwidth != CHAN_WIDTH_UNKNOWN) {
				ucv_object_add(ret, "chan_width",
					       ucv_int64_new(si.chanwidth));
				ucv_object_add(ret, "center_freq1",
					       ucv_int64_new(si.center_frq1));
				ucv_object_add(ret, "center_freq2",
					       ucv_int64_new(si.center_frq2));
				ucv_object_add(ret, "punct_bitmap",
					       ucv_int64_new(si.punct_bitmap));
			}
		}
	}
#endif
out:
	return ret;
}

static uc_value_t *
uc_wpas_iface_switch_channel(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = uc_fn_thisval("wpas.iface");
	uc_value_t *info = uc_fn_arg(0);
	struct csa_settings csa;
	int val;
	bool boolval;

	if (!wpa_s || ucv_type(info) != UC_OBJECT)
		return ucv_boolean_new(false);

#ifndef CONFIG_MESH
	return ucv_boolean_new(false);
#else
	if (!wpa_s->ifmsh)
		return ucv_boolean_new(false);

	os_memset(&csa, 0, sizeof(csa));
	csa.cs_count = 5;
	csa.link_id = -1;
	csa.power_mode = -1;

	csa.freq_params.ht_enabled = wpa_s->ifmsh->conf->ieee80211n;
	csa.freq_params.vht_enabled = wpa_s->ifmsh->conf->ieee80211ac;
	csa.freq_params.he_enabled = wpa_s->ifmsh->conf->ieee80211ax;
	csa.freq_params.eht_enabled = wpa_s->ifmsh->conf->ieee80211be;
	csa.freq_params.uhr_enabled = wpa_s->ifmsh->conf->ieee80211bn;

	if (uc_wpas_object_get_int(info, "csa_count", &val))
		csa.cs_count = val;
	if (uc_wpas_object_get_int(info, "ap_beacon_int", &val) && val > 0)
		csa.cs_count = wpas_mesh_csa_count_from_ap(
			csa.cs_count, val);
	if (uc_wpas_object_get_bool(info, "block_tx", &boolval))
		csa.block_tx = boolval;

	if (!uc_wpas_object_get_int(info, "frequency", &csa.freq_params.freq))
		return ucv_boolean_new(false);
	if (!uc_wpas_object_get_int(info, "bandwidth", &csa.freq_params.bandwidth))
		return ucv_boolean_new(false);

	uc_wpas_object_get_int(info, "center_freq1",
			       &csa.freq_params.center_freq1);
	uc_wpas_object_get_int(info, "center_freq2",
			       &csa.freq_params.center_freq2);
	if (uc_wpas_object_get_int(info, "punct_bitmap", &val))
		csa.freq_params.punct_bitmap = val;
	uc_wpas_object_get_int(info, "bandwidth_device",
			       &csa.freq_params.bandwidth_device);
	uc_wpas_object_get_int(info, "center_freq_device",
			       &csa.freq_params.center_freq_device);

	if (uc_wpas_object_get_bool(info, "ht", &boolval))
		csa.freq_params.ht_enabled = boolval;
	if (uc_wpas_object_get_bool(info, "vht", &boolval))
		csa.freq_params.vht_enabled = boolval;
	if (uc_wpas_object_get_bool(info, "he", &boolval))
		csa.freq_params.he_enabled = boolval;
	if (uc_wpas_object_get_bool(info, "eht", &boolval))
		csa.freq_params.eht_enabled = boolval;
	if (uc_wpas_object_get_bool(info, "uhr", &boolval))
		csa.freq_params.uhr_enabled = boolval;
	if (uc_wpas_object_get_int(info, "power_mode", &val))
		csa.power_mode = val;

	if (!uc_wpas_object_get_int(info, "sec_chan_offset",
				    &csa.freq_params.sec_channel_offset)) {
#ifdef CONFIG_QCN_EXTN
		enum chan_width width;
		switch (csa.freq_params.bandwidth) {
		case 20:
			width = CHAN_WIDTH_20;
			break;
		case 40:
			width = CHAN_WIDTH_40;
			break;
		case 80:
			width = CHAN_WIDTH_80;
			break;
		case 160:
			width = CHAN_WIDTH_160;
			break;
		case 320:
			width = CHAN_WIDTH_320;
			break;
		default:
			width = CHAN_WIDTH_UNKNOWN;
			break;
		}
		csa.freq_params.sec_channel_offset =
			compute_sec_channel_offset_extn(
				csa.freq_params.freq,
				csa.freq_params.center_freq1,
				width);
		if (csa.freq_params.sec_channel_offset < 0)
			csa.freq_params.sec_channel_offset = 0;
#else
		csa.freq_params.sec_channel_offset = 0;
#endif
	}

	return ucv_boolean_new(wpas_ap_apply_channel_switch(wpa_s, &csa) == 0);
#endif /* CONFIG_MESH */
}

#ifdef CONFIG_QCN_EXTN
/**
 * uc_wpas_recvd_ch_sw_result_ev - Handle ucode channel switch result event
 * @vm: ucode VM context invoking the callback
 * @nargs: Number of ucode arguments passed to the function
 *
 * Called from ucode when hostapd/wpa_supplicant reports channel switch
 * result. The function locates interfaces waiting in WPA_PRE_CONNECT
 * state with cached connect work and schedules SME authentication radio
 * work on the matching frequency.
 *
 * Return: New ucode integer value (currently a placeholder return code).
 */
static uc_value_t *
uc_wpas_recvd_ch_sw_result_ev(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = NULL;
	uc_value_t *freq_arg = NULL;
	uc_value_t *ret_arg = NULL;
	int freq = 0;
	int ret = -1;
	int chsw_ret = 0;
	unsigned int i;
	bool is_target_freq;

	/* Validate global context */
	if (!wpa_global || !wpa_global->ifaces) {
		wpa_printf(MSG_ERROR, "Recv chan sw result: wpa_global or ifaces is NULL");
		return ucv_int64_new(ret);
	}

	/* Validate arguments */
	if (nargs < 1) {
		wpa_printf(MSG_ERROR, "Recv chan sw result: missing frequency argument");
		return ucv_int64_new(ret);
	}

	freq_arg = uc_fn_arg(0);
	if (!freq_arg) {
		wpa_printf(MSG_ERROR, "Recv chan sw result: NULL frequency argument");
		return ucv_int64_new(ret);
	}
	freq = ucv_int64_get(freq_arg);

	ret_arg = uc_fn_arg(1);
	if (!ret_arg) {
		wpa_printf(MSG_ERROR, "Recv chan sw result: NULL return argument");
		return ucv_int64_new(ret);
	}
	chsw_ret = ucv_int64_get(ret_arg);

	wpa_printf(MSG_INFO, "Recv chan sw result: freq=%d ret=%d", freq, chsw_ret);

	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (!wpa_s)
			continue;
		/*
		 * Non-DFS channel change on the Repeater AP side: clear the
		 * hold flag and kick off a fresh scan so the STA can follow
		 * the AP to its new channel.  Skip the WPA_PRE_CONNECT path
		 * for this interface.
		 */
		if (wpa_s->wpa_state != WPA_PRE_CONNECT &&
		    IS_CSH_IGNORE_CSA_DFS_ENABLED(wpa_s->conf->cswopts) &&
		    wpa_s->hold_scan_csa) {
			wpa_printf(MSG_INFO,
				   "%s: Resuming STA scan on %s after Repeater AP non-DFS channel change",
				   __func__, wpa_s->ifname);
			wpa_s->hold_scan_csa = false;
			eloop_cancel_timeout(wpa_supplicant_start_sta_scan, wpa_s, NULL);
			wpa_supplicant_req_scan(wpa_s, 0, 0);
			continue;
		}

		if (!wpa_s->cache_cwork)
			continue;
		if (!wpa_s->cache_cwork->bss)
			continue;
		/*
		 * Match on the frequencies snapshotted when this interface
		 * entered PRE_CONNECT, not on cache_cwork->bss->freq/
		 * mld_links: those are live values in the scan cache and a
		 * background scan can rewrite them in place while this
		 * connection attempt is still pending (e.g. during
		 * independent-repeater bring-up when the fronthaul AP's own
		 * CSA to the root AP's channel has not yet completed).
		 * Comparing against live values would then reject the
		 * legitimate match. On multi-radio setups, another interface
		 * may also hold an unrelated stale cache_cwork targeting a
		 * different frequency; without this check that interface can
		 * steal the event meant for the one actually waiting on this
		 * frequency, starving it until it hits
		 * SME_PRE_CONNECT_TIMEOUT. For an MLD BSS, a completion event
		 * arrives once per valid link at a distinct frequency, so
		 * match against membership in the snapshotted set rather
		 * than a single scalar; a single-scalar match would only
		 * ever satisfy one of the pre_connect_cnt link increments and
		 * the rest would starve until timeout.
		 */
		is_target_freq = false;
		for (i = 0; i < wpa_s->pre_connect_target_freq_count; i++) {
			if (wpa_s->pre_connect_target_freqs[i] == freq) {
				is_target_freq = true;
				break;
			}
		}
		if (!is_target_freq)
			continue;

		wpa_printf(MSG_INFO,
			   "Recv chan sw result: ifname=%s state=%d freq=%d",
			   wpa_s->ifname, wpa_s->wpa_state, freq);

		/* On failure, reset pre_connect_cnt and trigger a fresh scan */
		if (chsw_ret) {
			wpa_printf(MSG_INFO,
				   "Recv chan sw result: failure ret=%d, triggering scan for ifname=%s",
				   chsw_ret, wpa_s->ifname);
			wpa_s->cache_cwork = NULL;
			wpa_s->pre_connect_cnt = 0;
			/* Flush outdated BSS entries so the next scan uses fresh results */
			wpa_bss_flush(wpa_s, 0);
			wpa_supplicant_req_scan(wpa_s, 0, 0);
			break;
		}

		if (wpa_s->pre_connect_cnt > 0) {
			wpa_s->pre_connect_cnt--;

			wpa_printf(MSG_INFO,
				   "Recv chan sw result: ifname=%s pre_connect_cnt now=%d",
				   wpa_s->ifname, wpa_s->pre_connect_cnt);

			if (wpa_s->pre_connect_cnt == 0) {
				wpa_printf(MSG_INFO,
					   "Recv chan sw result: Scheduling auth for ifname=%s",
					   wpa_s->ifname);
				sme_schedule_auth_radio_work(wpa_s, wpa_s->cache_cwork);
				ret = 0;
			}
		}
		break; /* Only handle one iface per event */
	}

	return ucv_int64_new(ret);
}

/**
 * uc_wpas_abort_scan_for_acs - API to abort STA scan when repeater AP starts ACS
 * @vm: ucode VM context invoking the callback
 * @nargs: Number of ucode arguments passed to the function
 *
 * Called from ucode when hostapd starts ACS. The function locates interfaces
 * and aborts any ongoing scans, then sets acs_complete flag to 0 to prevent
 * new scans until ACS completes.
 *
 * Return: New ucode integer value (0 on success, -1 on error).
 */
static uc_value_t *
uc_wpas_abort_scan_for_acs(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s;

	wpa_printf(MSG_INFO, "%s: Aborting STA scans for ACS start",
		   __func__);

	/* Validate global context */
	if (!wpa_global || !wpa_global->ifaces) {
		wpa_printf(MSG_ERROR, "%s: wpa_global or ifaces is NULL",
			   __func__);
		return ucv_int64_new(-1);
	}

	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (wpa_s->conf && wpa_s->conf->ind_rptr &&
		    (wpa_s->acs_complete == 1)) {
			/* Abort any ongoing scan */
			wpas_abort_ongoing_scan(wpa_s);

			/* Set acs_complete to 0 to prevent new scans */
			wpa_s->acs_complete = 0;

			wpa_printf(MSG_DEBUG,
				   "%s: Aborted scan and set acs_complete=0 for %s",
				   __func__, wpa_s->ifname);
		}
	}

	return ucv_int64_new(0);
}

/**
 * uc_wpas_start_scan_post_acs - API to start STA scan post repeater AP ACS
 * @vm: ucode VM context invoking the callback
 * @nargs: Number of ucode arguments passed to the function
 *
 * Called from ucode when hostapd send ACS completion event post ACS in its
 * configured links. The function locates interfaces whose acs_complete flag
 * is not set and schedules it to start scanning post setting its acs_complete
 * flag to 1.
 *
 * Return: New ucode integer value (currently a placeholder return code).
 */
static uc_value_t *
uc_wpas_start_scan_post_acs(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s;

	wpa_printf(MSG_INFO, "%s: Resuming STA scan for all interfaces",
		   __func__);

	/* Validate global context */
	if (!wpa_global || !wpa_global->ifaces) {
		wpa_printf(MSG_ERROR, "%s: wpa_global or ifaces is NULL",
			   __func__);
		return ucv_int64_new(-1);
	}

	for (wpa_s = wpa_global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (wpa_s->conf && wpa_s->conf->ind_rptr &&
		    (wpa_s->acs_complete == 0)) {
			wpa_s->acs_complete = 1;
			wpa_printf(MSG_DEBUG, "%s: Setting acs_complete=1 for %s and resuming scan",
				   __func__, wpa_s->ifname);
			eloop_cancel_timeout(wpa_supplicant_start_sta_scan, wpa_s, NULL);
			wpa_supplicant_req_scan(wpa_s, 0, 0);
		}
	}

	return ucv_int64_new(0);
}
#endif

int wpas_ucode_init(struct wpa_global *gl)
{
	static const uc_function_list_t global_fns[] = {
		{ "printf",	uc_wpa_printf },
		{ "getpid", uc_wpa_getpid },
		{ "add_iface", uc_wpas_add_iface },
		{ "remove_iface", uc_wpas_remove_iface },
		{ "udebug_set", uc_wpa_udebug_set },
#ifdef CONFIG_QCN_EXTN
		{ "recvd_ch_sw_result_ev", uc_wpas_recvd_ch_sw_result_ev },
		{ "start_scan_post_acs", uc_wpas_start_scan_post_acs },
		{ "abort_scan_for_acs", uc_wpas_abort_scan_for_acs },
#endif
	};
	static const uc_function_list_t iface_fns[] = {
		{ "status", uc_wpas_iface_status },
		{ "switch_channel", uc_wpas_iface_switch_channel },
#ifdef CONFIG_QCN_EXTN
		{ "notify_uplink_csa", uc_wpas_notify_uplink_csa_extn },
		{ "reconnect", uc_wpas_iface_reconnect_extn },
		{ "notify_rcsa", uc_wpas_notify_rcsa_extn },
		{ "abort_scan_for_acs", uc_wpas_abort_scan_for_acs },
#endif
	};

	wpa_global = gl;
	vm = wpa_ucode_create_vm();

	global_type = uc_type_declare(vm, "wpas.global", global_fns, NULL);
	iface_type = uc_type_declare(vm, "wpas.iface", iface_fns, NULL);

	iface_registry = ucv_array_new(vm);
	uc_vm_registry_set(vm, "wpas.iface_registry", iface_registry);

	global = wpa_ucode_global_init("wpas", global_type);

	if (wpa_ucode_run(HOSTAPD_UC_PATH "wpa_supplicant.uc"))
		goto free_vm;

	ucv_gc(vm);
	return 0;

free_vm:
	wpa_ucode_free_vm();
	return -1;
}

void wpas_ucode_free(void)
{
	if (wpa_ucode_call_prepare("shutdown") == 0)
		ucv_put(wpa_ucode_call(0));
	wpa_ucode_free_vm();
}
