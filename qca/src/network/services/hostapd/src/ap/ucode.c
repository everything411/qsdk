#include <sys/un.h>

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/ucode.h"
#include "hostapd.h"
#include "beacon.h"
#include "hw_features.h"
#include "ap_drv_ops.h"
#include "dfs.h"
#include "acs.h"
#include "robust_av.h"
#include <libubox/uloop.h>
#include "sta_info.h"
#include "../atf/atf_offload.h"
#ifdef CONFIG_IEEE80211BN
#include "mapc.h"
#endif /* CONFIG_IEEE80211BN */
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif

static uc_resource_type_t *global_type, *bss_type, *iface_type;
static struct hapd_interfaces *interfaces;
static uc_value_t *global, *bss_registry, *iface_registry;
static uc_vm_t *vm;

#ifdef CONFIG_QCN_EXTN
struct uc_value *ucode_ap_fetch_iface_reg_extn(void)
{
	return iface_registry;
}

struct uc_vm *ucode_ap_fetch_vm_extn(void)
{
	return vm;
}
#endif

static uc_value_t *
hostapd_ucode_bss_get_uval(struct hostapd_data *hapd)
{
	uc_value_t *val;

	if (hapd->ucode.idx)
		return wpa_ucode_registry_get(bss_registry, hapd->ucode.idx);

	val = uc_resource_new(bss_type, hapd);
	hapd->ucode.idx = wpa_ucode_registry_add(bss_registry, val);

	return val;
}

static uc_value_t *
hostapd_ucode_iface_get_uval(struct hostapd_iface *hapd)
{
	uc_value_t *val;

	if (hapd->ucode.idx)
		return wpa_ucode_registry_get(iface_registry, hapd->ucode.idx);

	val = uc_resource_new(iface_type, hapd);
	hapd->ucode.idx = wpa_ucode_registry_add(iface_registry, val);

	return val;
}

static void
hostapd_ucode_update_bss_list(struct hostapd_iface *iface, uc_value_t *if_bss, uc_value_t *bss)
{
	uc_value_t *list;
	int i;
	char ifname[IFNAMSIZ + 10]; /* extra room for ":<phy>" suffix */

	list = ucv_array_new(vm);
	for (i = 0; iface->bss && i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];
		uc_value_t *val = hostapd_ucode_bss_get_uval(hapd);

		os_snprintf(ifname, sizeof(ifname), "%s:%s",
			    hapd->conf->iface, iface->phy);

		ucv_array_set(list, i, ucv_get(ucv_string_new(hapd->conf->iface)));
		ucv_object_add(bss, ifname, ucv_get(val));
	}
	ucv_object_add(if_bss, iface->phy, ucv_get(list));
}

void
hostapd_ucode_update_interfaces()
{
	uc_value_t *ifs, *if_bss, *bss;
	int i;

	if (!vm || !global)
		return;

	ifs = ucv_object_new(vm);
	if_bss = ucv_array_new(vm);
	bss = ucv_object_new(vm);

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];
		wpa_printf(MSG_INFO, "adding ucode index as %s\n", iface->phy);
		ucv_object_add(ifs, iface->phy, ucv_get(hostapd_ucode_iface_get_uval(iface)));
		hostapd_ucode_update_bss_list(iface, if_bss, bss);
	}
	ucv_object_add(ucv_prototype_get(global), "interfaces", ucv_get(ifs));
	ucv_object_add(ucv_prototype_get(global), "interface_bss", ucv_get(if_bss));
	ucv_object_add(ucv_prototype_get(global), "bss", ucv_get(bss));
	ucv_gc(vm);
}

char *hostapd_ucode_get_ifname(int id, char *ifname) {
        int i;

	wpa_printf(MSG_INFO, "get interface name for id %d : %s\n",id, ifname);

	if (id == -1)
		return ifname;

        for (i = 0; i < interfaces->count; i++) {
                struct hostapd_iface *iface = interfaces->iface[i];
                if (iface->ucode.radio_id == id) {
                        wpa_printf(MSG_INFO, "remove iface %s\n", iface->phy);
                        return iface->phy;
                }
        }
	return NULL;
}

void hostapd_ucode_update_radio_id(char *ifname, int id) {
	int i;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];
		if (!os_strcmp(iface->conf->bss[0]->iface, ifname)) {
			wpa_printf(MSG_INFO, "updating radio_id %d\n", id);
			iface->ucode.radio_id = id;
		}
	}
}

static uc_value_t *
uc_hostapd_add_iface(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *iface = uc_fn_arg(0);
	uc_value_t *radio_id = uc_fn_arg(1);
	uc_value_t *iface_name = uc_fn_arg(2);
	int id;
	char *data;
	char *ifname;
	int ret;

	if (ucv_type(iface) != UC_STRING)
		return ucv_int64_new(-1);

	 if (ucv_type(radio_id) != UC_INTEGER)
		wpa_printf(MSG_ERROR, "%s: failed to fetch radio_id", __func__);

	if (ucv_type(iface) != UC_STRING)
		wpa_printf(MSG_ERROR, "%s: failed to fetch ifname", __func__);

	id = ucv_int64_get(radio_id);
	ifname = ucv_string_get(iface_name);
	data = strdup(ucv_string_get(iface));

	if (data) {
		ret = hostapd_add_iface(interfaces, data);
		wpa_printf(MSG_INFO, "%s:  add interface %d: %s, ret:%d",
			   __func__, id, ifname, ret);
		free(data);
	} else {
		return ucv_int64_new(-1);
	}

	hostapd_ucode_update_radio_id(ifname, id);

	hostapd_ucode_update_interfaces();

	return ucv_int64_new(ret);
}

static uc_value_t *
uc_hostapd_remove_iface(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *iface = uc_fn_arg(0);
	char *ifname;

	if (ucv_type(iface) != UC_STRING)
		return NULL;

	ifname = ucv_string_get(iface);

	if (ifname) {
		wpa_printf(MSG_INFO, "removing interface %s", ifname);
		hostapd_remove_iface(interfaces, ifname);
		hostapd_ucode_update_interfaces();
	}

	return NULL;
}

static struct hostapd_vlan *
bss_conf_find_vlan(struct hostapd_bss_config *bss, int id)
{
	struct hostapd_vlan *vlan;

	for (vlan = bss->vlan; vlan; vlan = vlan->next)
		if (vlan->vlan_id == id)
			return vlan;

	return NULL;
}

static int
bss_conf_rename_vlan(struct hostapd_data *hapd, struct hostapd_vlan *vlan,
		     const char *ifname)
{
	if (!strcmp(ifname, vlan->ifname))
		return 0;

	hostapd_drv_if_rename(hapd, WPA_IF_AP_VLAN, vlan->ifname, ifname);
	os_strlcpy(vlan->ifname, ifname, sizeof(vlan->ifname));

	return 0;
}

static int
bss_reload_vlans(struct hostapd_data *hapd, struct hostapd_bss_config *bss)
{
	struct hostapd_bss_config *old_bss = hapd->conf;
	struct hostapd_vlan *vlan, *vlan_new, *wildcard;
	char ifname[IFNAMSIZ + 1], vlan_ifname[IFNAMSIZ + 1], *pos;
	int ret;

	vlan = bss_conf_find_vlan(old_bss, VLAN_ID_WILDCARD);
	wildcard = bss_conf_find_vlan(bss, VLAN_ID_WILDCARD);
	if (!!vlan != !!wildcard)
		return -1;

	if (vlan && wildcard && strcmp(vlan->ifname, wildcard->ifname) != 0)
		strcpy(vlan->ifname, wildcard->ifname);
	else
		wildcard = NULL;

	for (vlan = bss->vlan; vlan; vlan = vlan->next) {
		if (vlan->vlan_id == VLAN_ID_WILDCARD ||
		    vlan->dynamic_vlan > 0)
			continue;

		if (!bss_conf_find_vlan(old_bss, vlan->vlan_id))
			return -1;
	}

	for (vlan = old_bss->vlan; vlan; vlan = vlan->next) {
		if (vlan->vlan_id == VLAN_ID_WILDCARD)
			continue;

		if (vlan->dynamic_vlan == 0) {
			vlan_new = bss_conf_find_vlan(bss, vlan->vlan_id);
			if (!vlan_new)
				return -1;

			if (bss_conf_rename_vlan(hapd, vlan, vlan_new->ifname))
				return -1;

			continue;
		}

		if (!wildcard)
			continue;

		os_strlcpy(ifname, wildcard->ifname, sizeof(ifname));
		pos = os_strchr(ifname, '#');
		if (!pos)
			return -1;

		*pos++ = '\0';
		ret = os_snprintf(vlan_ifname, sizeof(vlan_ifname), "%s%d%s",
				  ifname, vlan->vlan_id, pos);
	        if (os_snprintf_error(sizeof(vlan_ifname), ret))
			return -1;

		if (bss_conf_rename_vlan(hapd, vlan, vlan_ifname))
			return -1;
	}

	return 0;
}

static uc_value_t *
uc_hostapd_bss_set_config(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_data *hapd = uc_fn_thisval("hostapd.bss");
	struct hostapd_bss_config *old_bss;
	struct hostapd_iface *iface;
	struct hostapd_config *conf;
	uc_value_t *file = uc_fn_arg(0);
	uc_value_t *index = uc_fn_arg(1);
	uc_value_t *files_only = uc_fn_arg(2);
	unsigned int i, idx = 0;
	int ret = -1;

	if (!hapd || ucv_type(file) != UC_STRING)
		goto out;

	if (ucv_type(index) == UC_INTEGER)
		idx = ucv_int64_get(index);

	iface = hapd->iface;
	conf = interfaces->config_read_cb(ucv_string_get(file));
	if (!conf)
		goto out;

	if (idx >= conf->num_bss || !conf->bss[idx])
		goto free;

	if (ucv_boolean_get(files_only)) {
		struct hostapd_bss_config *bss = conf->bss[idx];
		struct hostapd_bss_config *old_bss = hapd->conf;

#define swap_field(name)				\
	do {								\
		void *ptr = old_bss->name;		\
		old_bss->name = bss->name;		\
		bss->name = ptr;				\
	} while (0)

		swap_field(ssid.wpa_psk_file);
		ret = bss_reload_vlans(hapd, bss);
		goto done;
	}

#ifdef CONFIG_IEEE80211BE
	/*
	 * When the SSID changes, reload all MLD links before stop_ap while
	 * beacons are still running so SET_BEACON is used instead of
	 * START_AP (no cross-link SSID check on SET_BEACON).
	 */
	if (hapd->started && hapd->conf->mld_ap && hapd->mld &&
	    (conf->bss[idx]->ssid.ssid_len != hapd->conf->ssid.ssid_len ||
	     os_memcmp(conf->bss[idx]->ssid.ssid, hapd->conf->ssid.ssid,
		       conf->bss[idx]->ssid.ssid_len) != 0)) {
		struct hostapd_data *link;

		/* Swap new config into hapd so all links see the new SSID. */
		old_bss = hapd->conf;
		hapd->conf = conf->bss[idx];
		conf->bss[idx] = old_bss;

		dl_list_for_each(link, &hapd->mld->links,
				 struct hostapd_data, link) {
			if (!link->started)
				continue;
			link->conf->ssid.ssid_len = hapd->conf->ssid.ssid_len;
			os_memcpy(link->conf->ssid.ssid, hapd->conf->ssid.ssid,
				  hapd->conf->ssid.ssid_len);
			hostapd_reload_bss_only(link);
		}

		/* Restore hapd->conf for the teardown path below. */
		conf->bss[idx] = hapd->conf;
		hapd->conf = old_bss;
	}

	/*
	 * Stop non-TX BSS beacons (both MLD and non-MLD) before the TX BSS
	 * teardown.  REENABLE_REUSE_LINK preserves the kernel link/vdev so
	 * the driver does not see an inconsistent MBSSID group state.
	 */
	if (hapd->iconf->mbssid && hapd == hostapd_mbssid_get_tx_bss(hapd) &&
	    hapd->mbssid_group) {
		struct hostapd_data *non_tx;

		dl_list_for_each(non_tx, &hapd->mbssid_group->bss_list,
				 struct hostapd_data, mbssid_bss) {
			if (non_tx == hapd || !non_tx->started || !non_tx->conf)
				continue;
#ifdef CONFIG_QCN_EXTN
			hostapd_disable_bss(non_tx, 0, AP_EVENT_DISABLED);
#endif /* CONFIG_QCN_EXTN */
		}
	}
#endif /* CONFIG_IEEE80211BE */

	hostapd_bss_deinit_no_free(hapd);
	hostapd_drv_stop_ap(hapd);
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap && hapd != iface->bss[0])
		hostapd_bss_link_deinit(hapd);
#endif /* CONFIG_IEEE80211BE */
	hostapd_free_hapd_data(hapd);

	old_bss = hapd->conf;
	for (i = 0; i < iface->conf->num_bss; i++)
		if (iface->conf->bss[i] == hapd->conf)
			iface->conf->bss[i] = conf->bss[idx];
	hapd->conf = conf->bss[idx];
	conf->bss[idx] = old_bss;

	hostapd_setup_bss(hapd, hapd == iface->bss[0], true);

#ifdef CONFIG_IEEE80211BE
	/* Re-enable non-TX BSSes (both MLD and non-MLD) stopped above */
	if (hapd->iconf->mbssid && hapd == hostapd_mbssid_get_tx_bss(hapd) &&
	    hapd->mbssid_group) {
		struct hostapd_data *non_tx;

		dl_list_for_each(non_tx, &hapd->mbssid_group->bss_list,
				 struct hostapd_data, mbssid_bss) {
			if (non_tx == hapd ||
			    non_tx->reenable != REENABLE_REUSE_LINK)
				continue;
			hostapd_enable_bss(non_tx);
		}
	}
#endif /* CONFIG_IEEE80211BE */

	hostapd_ucode_update_interfaces();

done:
	ret = 0;
free:
	hostapd_config_free(conf);
out:
	return ucv_int64_new(ret);
}

static uc_value_t *
uc_hostapd_bss_delete(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_data *hapd = uc_fn_thisval("hostapd.bss");
	struct hostapd_iface *iface;
	int idx, ret;
	bool refresh_all = false;

	if (!hapd)
		return NULL;

	iface = hapd->iface;
	if (iface->num_bss == 1) {
		wpa_printf(MSG_ERROR, "trying to delete last bss of an iface: %s\n", hapd->conf->iface);
		return NULL;
	}

	for (idx = 0; idx < iface->num_bss; idx++)
		if (iface->bss[idx] == hapd)
			break;

	if (idx == iface->num_bss)
		return NULL;

	/*
	 * Mirror the MBSSID handling from hostapd_remove_iface():
	 * - If the TX BSS is being deleted, first remove all its associated
	 *   non-TX BSSes, then re-locate the TX BSS index in the (now
	 *   shorter) iface->bss[] array.
	 * - If a non-TX BSS is being deleted, all iface beacons must be
	 *   refreshed afterwards so the remaining BSSes advertise the
	 *   updated MBSSID set.
	 */
	if (hapd->iconf->mbssid) {
		if (hapd == hostapd_mbssid_get_tx_bss(hapd)) {
			if ((hapd->mbssid_group &&
			     iface->num_bss == (int)dl_list_len(
				     &hapd->mbssid_group->bss_list)) ||
			    (!hapd->mbssid_group &&
			     hapd == iface->bss[0])) {
				wpa_printf(MSG_DEBUG,
					   "Trying to delete last TX BSS "
					   "that would empty iface %s",
					   hapd->conf->iface);
				return NULL;
			}
			hostapd_remove_non_tx_bsses(hapd);
			/* Re-find idx: non-TX removals may have shifted the array */
			for (idx = 0; idx < iface->num_bss; idx++)
				if (iface->bss[idx] == hapd)
					break;
			if (idx == iface->num_bss)
				return NULL;
		} else {
			/* Non-TX BSS removal requires a full beacon refresh */
			refresh_all = true;
		}
	}

	/*
	 * hostapd_remove_bss() internally calls hostapd_bss_deinit() and
	 * handles additional cleanup missing from the previous open-coded path:
	 *   - hostapd_mld_ref_dec()            (MLD reference counting)
	 *   - hostapd_free_mbssid_idx()         (MBSSID index release)
	 *   - hostapd_multi_mbssid_remove_bss() (MBSSID group cleanup)
	 *   - NFT chain removal for SCS
	 *   - pre-beacon-state successor BSS preparation
	 *   - ML max-recommended-links update
	 *
	 * hostapd_drv_stop_ap() must still be called here because
	 * hostapd_remove_bss() does not call it.
	 *
	 * driver_ap_teardown controls whether the low-level station flush is
	 * skipped (matching the behaviour in hostapd_remove_iface()).
	 *
	 * After hostapd_remove_bss() has shifted iface->bss[], update the
	 * new first BSS so the driver knows which BSS is now primary.
	 */
	iface->driver_ap_teardown = !(iface->drv_flags &
				      WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);

	ret = hostapd_remove_bss(iface, idx);
	if (ret < 0)
		return NULL;
	if (ret == 1) {
		hostapd_ucode_update_interfaces();
		ucv_gc(vm);
		return NULL;
	}

	if (iface->num_bss > 0) {
		iface->bss[0]->interface_added = 0;
		hostapd_drv_set_first_bss(iface->bss[0]);
	}

	/*
	 * Refresh beacons on peer interfaces so they reflect the updated BSS
	 * set, mirroring the refresh_beacon logic in hostapd_remove_iface().
	 */
	if (refresh_all)
		hostapd_refresh_all_iface_beacons(iface);
	else
		hostapd_refresh_other_iface_beacons(iface);

	hostapd_ucode_update_interfaces();

	ucv_gc(vm);

	return NULL;
}

static uc_value_t *
uc_hostapd_iface_add_bss(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_iface *iface = uc_fn_thisval("hostapd.iface");
	struct hostapd_bss_config *bss;
	struct hostapd_config *conf = NULL;
	struct hostapd_data *hapd;
	uc_value_t *file = uc_fn_arg(0);
	uc_value_t *index = uc_fn_arg(1);
	unsigned int idx = 0;
	uc_value_t *ret = NULL;
	struct hostapd_bss_config **tmp_bss;
	struct hostapd_data **temp_bss;

	if (!iface || ucv_type(file) != UC_STRING)
		return NULL;

	if (ucv_type(index) == UC_INTEGER)
		idx = ucv_int64_get(index);

	conf = interfaces->config_read_cb(ucv_string_get(file));
	if (!conf || idx >= conf->num_bss || !conf->bss[idx])
		goto free_conf;

	bss = conf->bss[idx];

	if (!iface->num_bss || !iface->bss[0] ||
	    !iface->bss[0]->driver || !iface->bss[0]->drv_priv) {
		wpa_printf(MSG_ERROR,
				"%s: bss[0] driver/drv_priv not initialized, cannot add %s",
				__func__, bss->iface);
		goto free_conf;
	}

	/*
	 * Add the new BSS config to iface->conf->bss[] BEFORE calling
	 * hostapd_setup_bss().  Inside hostapd_setup_bss(), when the iface is
	 * already enabled, ieee802_11_set_beacon(tx_hapd) is called to update
	 * the TX BSS beacon so it includes the new non-TX BSS MBSSID profile.
	 * That beacon rebuild iterates iface->conf->bss[], so the new entry
	 * must be present at that point; otherwise the MBSSID element is built
	 * without the new non-TX BSS profile and the beacon/probe-response
	 * frame will be missing those 78 bytes.
	 */
	tmp_bss = os_realloc_array(iface->conf->bss,
					    iface->conf->num_bss + 1,
					    sizeof(*iface->conf->bss));
	if (!tmp_bss)
		goto free_conf;
	iface->conf->bss = tmp_bss;
	iface->conf->bss[iface->conf->num_bss] = bss;
	iface->conf->num_bss++;
	iface->conf->last_bss = bss;

	hapd = hostapd_alloc_bss_data(iface, iface->conf, bss);
	if (!hapd)
		goto remove_bss_conf;

	hapd->driver = iface->bss[0]->driver;
	hapd->drv_priv = iface->bss[0]->drv_priv;

	hostapd_bss_setup_multi_link(hapd, iface->interfaces);

	if (hostapd_set_ctrl_sock_iface(hapd))
		goto free_hapd;

	if (interfaces->ctrl_iface_init &&
	    interfaces->ctrl_iface_init(hapd) < 0)
		goto free_hapd;

	temp_bss = os_realloc_array(iface->bss, iface->num_bss + 1,
				      sizeof(*iface->bss));
	if (!temp_bss)
		goto deinit_ctrl;
	iface->bss = temp_bss;
	iface->bss[iface->num_bss++] = hapd;

	if (iface->state == HAPD_IFACE_ENABLED &&
	    hostapd_setup_bss(hapd, false, true))
		goto remove_bss;

	conf->bss[idx] = NULL;
	ret = hostapd_ucode_bss_get_uval(hapd);
	hostapd_ucode_update_interfaces();
	goto free_conf;

remove_bss:
	iface->num_bss--;
	iface->bss[iface->num_bss] = NULL;
deinit_ctrl:
	if (interfaces->ctrl_iface_deinit)
		interfaces->ctrl_iface_deinit(hapd);
free_hapd:
#ifdef CONFIG_IEEE80211BE
	/* Clean up any MLD link added by setup_bss() before it failed. */
	if (hapd->conf && hapd->conf->mld_ap)
		hostapd_bss_link_deinit(hapd);
#endif /* CONFIG_IEEE80211BE */
	hostapd_free_hapd_data(hapd);
	hostapd_free_mbssid_idx(hapd);
	hostapd_multi_mbssid_remove_bss(hapd);
#ifdef CONFIG_IEEE80211BE
	hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
	os_free(hapd);
remove_bss_conf:
	iface->conf->num_bss--;
	iface->conf->bss[iface->conf->num_bss] = NULL;
	iface->conf->last_bss = iface->conf->num_bss ?
		iface->conf->bss[iface->conf->num_bss - 1] : NULL;
free_conf:
	hostapd_config_free(conf);
	return ret;
}

static uc_value_t *
uc_hostapd_iface_set_bss_order(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_iface *iface = uc_fn_thisval("hostapd.iface");
	uc_value_t *bss_list = uc_fn_arg(0);
	struct hostapd_data **new_bss;
	struct hostapd_bss_config **new_conf;

	if (!iface)
		return NULL;

	if (ucv_type(bss_list) != UC_ARRAY ||
	    ucv_array_length(bss_list) != iface->num_bss)
		return NULL;

	new_bss = calloc(iface->num_bss, sizeof(*new_bss));
	new_conf = calloc(iface->num_bss, sizeof(*new_conf));
	for (size_t i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss;

		bss = ucv_resource_data(ucv_array_get(bss_list, i), "hostapd.bss");
		if (bss->iface != iface)
			goto free;

		for (size_t k = 0; k < i; k++)
			if (new_bss[k] == bss)
				goto free;

		new_bss[i] = bss;
		new_conf[i] = bss->conf;
	}

	new_bss[0]->interface_added = 0;
	for (size_t i = 1; i < iface->num_bss; i++)
		new_bss[i]->interface_added = 1;

	free(iface->bss);
	iface->bss = new_bss;

	free(iface->conf->bss);
	iface->conf->bss = new_conf;
	iface->conf->num_bss = iface->num_bss;
	hostapd_drv_set_first_bss(iface->bss[0]);

	return ucv_boolean_new(true);

free:
	free(new_bss);
	free(new_conf);
	return NULL;
}

static uc_value_t *
uc_hostapd_bss_ctrl(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_data *hapd = uc_fn_thisval("hostapd.bss");
	uc_value_t *arg = uc_fn_arg(0);
	struct sockaddr_storage from = {};
	static char reply[4096];
	int reply_len;

	if (!hapd || !interfaces->ctrl_iface_recv ||
	    ucv_type(arg) != UC_STRING)
		return NULL;

	reply_len = interfaces->ctrl_iface_recv(hapd, ucv_string_get(arg),
						reply, sizeof(reply),
						&from, sizeof(from));
	if (reply_len < 0)
		return NULL;

	if (reply_len && reply[reply_len - 1] == '\n')
		reply_len--;

	return ucv_string_new_length(reply, reply_len);
}

static void
uc_hostapd_disable_iface(struct hostapd_iface *iface)
{
	switch (iface->state) {
	case HAPD_IFACE_DISABLED:
#ifdef CONFIG_QCN_EXTN
	/* Do not disable an interface which is in NO IR for future recovery */
	case HAPD_IFACE_NO_IR:
#endif
		break;
#ifdef CONFIG_ACS
	case HAPD_IFACE_ACS:
		acs_cleanup(iface);
		iface->scan_cb = NULL;
		/* fallthrough */
#endif
	default:
		hostapd_disable_iface(iface);
		break;
	}
}

static uc_value_t *
uc_hostapd_iface_stop(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_iface *iface = uc_fn_thisval("hostapd.iface");
	uc_value_t *info = uc_fn_arg(0);
#ifdef CONFIG_QCN_EXTN
	uc_value_t *wpa_state_val;
	char *wpa_state = NULL;
	bool rpt_max_phy_override = false;
	uint64_t intval;
#endif
	int i;

	if (!iface || ucv_type(info) != UC_OBJECT)
		return NULL;

#ifdef CONFIG_QCN_EXTN
	iface->iface_extn.vap_type = ucv_int64_get(ucv_object_get(info, "vap_type", NULL));
	if (!errno) {
		wpa_printf(MSG_DEBUG, "%s: VAP type: %s", __func__,
				vap_type_to_string(iface->iface_extn.vap_type));
	}

	wpa_state_val = ucv_object_get(info, "wpa_state", NULL);
	wpa_state = ucv_string_get(wpa_state_val);
	if (wpa_state) {
		os_strlcpy(iface->iface_extn.sta_wpa_state, wpa_state,
				sizeof(iface->iface_extn.sta_wpa_state));
		hostapd_update_bh_sta_connected_extn(iface, wpa_state);
	}
	iface->iface_extn.dfs_available_from_sta = false;

	wpa_printf(MSG_INFO,
		   "%s: state=%d sta_wpa_state=\"%s\" ind_rptr=%d rpt_max_phy=%d",
		   __func__, iface->state,
		   iface->iface_extn.sta_wpa_state,
		   iface->conf->conf_extn.ind_rptr,
		   iface->conf->conf_extn.rpt_max_phy);

	intval = ucv_int64_get(ucv_object_get(info, "rpt_max_phy_override", NULL));
	if (!errno)
		rpt_max_phy_override = intval;

	wpa_printf(MSG_DEBUG, "%s: rpt_max_phy_override: %d", __func__, rpt_max_phy_override);
#endif

	atf_bh_join_leave_update(iface, false);

#ifdef CONFIG_QCN_EXTN
	if (iface->conf->conf_extn.ind_rptr && !rpt_max_phy_override)
		return NULL;
#endif

	if (iface->state != HAPD_IFACE_ENABLED
#ifdef CONFIG_QCN_EXTN
	|| iface->conf->conf_extn.rpt_max_phy
#endif
	)
		uc_hostapd_disable_iface(iface);

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];

		hostapd_bss_deinit_no_free(hapd);
		hostapd_drv_stop_ap(hapd);
		hapd->beacon_set_done = 0;

		if (hapd->csa_in_progress)
			hostapd_cleanup_cs_params(hapd);
	}

	iface->cac_type = 0;

#ifdef CONFIG_QCN_EXTN
	iface->bootup_cac_in_progress = 0;
#endif /* CONFIG_QCN_EXTN */
	return NULL;
}

static uc_value_t *
uc_hostapd_iface_start(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_iface *iface = uc_fn_thisval("hostapd.iface");
	uc_value_t *info = uc_fn_arg(0);
	struct hostapd_config *conf;
	bool changed = false;
#ifdef CONFIG_QCN_EXTN
	bool is_dfs = false, skip_cac_rep = false;
	u32 mcst = 0;
	uc_value_t *wpa_state_val;
	char *wpa_state = NULL;
	bool rpt_max_phy_override = false;
	int event_freq = 0;
#endif
	uint64_t intval;
	int i, ret;

	if (!iface)
		return NULL;

	if (!info) {
		conf = iface->conf;
		iface->freq = 0;
		goto out;
	}

	if (ucv_type(info) != UC_OBJECT)
		return NULL;

#ifdef CONFIG_QCN_EXTN
	iface->iface_extn.vap_type = ucv_int64_get(ucv_object_get(info, "vap_type", NULL));
	if (!errno) {
		wpa_printf(MSG_DEBUG, "%s: VAP type: %s", __func__,
				vap_type_to_string(iface->iface_extn.vap_type));
	}

	wpa_state_val = ucv_object_get(info, "wpa_state", NULL);
	wpa_state = ucv_string_get(wpa_state_val);
	if (wpa_state) {
		os_strlcpy(iface->iface_extn.sta_wpa_state, wpa_state,
				sizeof(iface->iface_extn.sta_wpa_state));
		hostapd_update_bh_sta_connected_extn(iface, wpa_state);
	}
	intval = ucv_int64_get(ucv_object_get(info, "rpt_max_phy_override", NULL));
	if (!errno)
		rpt_max_phy_override = intval;

	intval = ucv_int64_get(ucv_object_get(info, "frequency", NULL));
	if (!errno)
		event_freq = intval;

	wpa_printf(MSG_INFO,
		   "%s: freq=%d, state=%d sta_wpa_state=\"%s\" ind_rptr=%d rpt_max_phy=%d rpt_max_phy_override=%d",
		   __func__, event_freq, iface->state,
		   iface->iface_extn.sta_wpa_state,
		   iface->conf->conf_extn.ind_rptr,
		   iface->conf->conf_extn.rpt_max_phy, rpt_max_phy_override);

	/* Independent repeaters keep their configured fronthaul channel. */
	if (iface->conf->conf_extn.ind_rptr && !rpt_max_phy_override)
		return NULL;

	iface->freq = event_freq;

	if (info && !iface->freq) {
		conf = iface->conf;
		goto out;
	}

	intval = ucv_int64_get(ucv_object_get(info, "mcst", NULL));
	if (!errno)
		mcst = intval;

#endif
#define UPDATE_VAL(field, name)							\
	if ((intval = ucv_int64_get(ucv_object_get(info, name, NULL))) &&	\
		!errno && intval != conf->field) do {				\
		conf->field = intval;						\
		changed = true;							\
	} while(0)

	conf = iface->conf;
	if (!conf)
		return NULL;

	UPDATE_VAL(hw_mode, "hw_mode");
	UPDATE_VAL(channel, "channel");

	/*op_class for 5GHz 320MHz bw is not defined in spec. So unset op_class*/
	intval = ucv_int64_get(ucv_object_get(info, "op_class", NULL));
	if (!errno)
		conf->op_class = intval;

	intval = ucv_int64_get(ucv_object_get(info, "sec_channel", NULL));
	if (!errno) {
		conf->secondary_channel = intval;
		changed = true;
	}

	if (!changed &&
	    (iface->bss[0]->beacon_set_done ||
	     iface->state == HAPD_IFACE_DFS))
		return ucv_boolean_new(true);

	intval = ucv_int64_get(ucv_object_get(info, "center_seg0_idx", NULL));
	if (!errno)
		hostapd_set_oper_centr_freq_seg0_idx(conf, intval);

	intval = ucv_int64_get(ucv_object_get(info, "center_seg1_idx", NULL));
	if (!errno)
		hostapd_set_oper_centr_freq_seg1_idx(conf, intval);

	intval = ucv_int64_get(ucv_object_get(info, "oper_chwidth", NULL));
	if (!errno)
		hostapd_set_oper_chwidth(conf, intval);

	conf->acs = 0;

	intval = ucv_int64_get(ucv_object_get(info, "punct_bitmap", NULL));
	if (!errno)
		conf->punct_bitmap = intval;
	else
		conf->punct_bitmap = 0;

#ifdef CONFIG_QCN_EXTN
	is_dfs = ucv_boolean_get(ucv_object_get(info, "is_dfs", NULL));
	if (!errno && is_dfs && mcst) {
		iface->mcst = mcst;
		iface->cs_time = IEEE80211_TU_TO_MS(mcst) +
			(2 * conf->beacon_int);
		wpa_printf(MSG_INFO, "%s: using residual CAC mcst=%u TU for DFS iface start",
			   __func__, iface->mcst);
	} else {
		iface->mcst = 0;
		iface->cs_time = 0;
	}

	if (!errno && conf->conf_extn.skip_cac) {
		skip_cac_rep = iface->iface_extn.dfs_available_from_sta = is_dfs && !iface->mcst;
		if (skip_cac_rep) {
			/* Seed a non-CAC MCST for Dep Rep FH add-iface skip-CAC.
			 * This lets CAC/beacon setup advertise MCST while keeping skip_cac=1. */
			iface->cs_time =
				HOSTAPD_NON_CAC_SWITCH_TIME_MSEC_EXTN(conf->beacon_int);
			iface->mcst = IEEE80211_MS_TO_TU(iface->cs_time);
		}
	}
	wpa_printf(MSG_INFO, "%s: is_dfs=%d mcst=%u TU cs_time=%u ms skip_cac_rep=%d",
		   __func__, is_dfs, iface->mcst, iface->cs_time, skip_cac_rep);
#endif
out:
	switch (iface->state) {
	case HAPD_IFACE_ENABLED:
#ifdef CONFIG_QCN_EXTN
		if (!skip_cac_rep && (!hostapd_is_dfs_required(iface) ||
			hostapd_is_dfs_chan_available(iface)))
#else
		if (!hostapd_is_dfs_required(iface) ||
			hostapd_is_dfs_chan_available(iface))
#endif
			break;
		wpa_printf(MSG_INFO, "DFS CAC required on new channel, restart interface");
		/* fallthrough */
	default:
		uc_hostapd_disable_iface(iface);
		break;
	}

	if (conf->channel && !iface->freq)
		iface->freq = hostapd_hw_get_freq(iface->bss[0], conf->channel);

	if (iface->state != HAPD_IFACE_ENABLED) {
		hostapd_enable_iface(iface);
		return ucv_boolean_new(true);
	}

	hostapd_apply_6ghz_dynamic_puncturing(iface);
	if (is_6ghz_freq(iface->freq) && iface->conf->enable_best_power_mode) {
		u8 best_power_mode;

		best_power_mode = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
		if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
			iface->conf->he_6ghz_reg_pwr_type = best_power_mode;
			wpa_printf(MSG_INFO,
				   "%s: Best power mode for Freq %d is %d",
				   __func__,
				   iface->freq, best_power_mode);
		}
	}

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];

		hapd->conf->start_disabled = 0;

		ret = hostapd_set_freq(hapd, conf->hw_mode, iface->freq,
				       conf->channel,
				       conf->enable_edmg,
				       conf->edmg_channel,
				       conf->ieee80211n,
				       conf->ieee80211ac,
				       conf->ieee80211ax,
				       conf->ieee80211be,
				       conf->ieee80211bn,
				       conf->secondary_channel,
				       hostapd_get_oper_chwidth(conf),
				       hostapd_get_oper_centr_freq_seg0_idx(conf),
				       hostapd_get_oper_centr_freq_seg1_idx(conf),
#ifdef CONFIG_QCN_EXTN
				       skip_cac_rep,
#endif
				       conf->bandwidth_device,
				       conf->center_freq_device);

		wpa_printf(MSG_INFO, "set_freq called for bssid " MACSTR " ret %d ifname %s\n",
			   MAC2STR(hapd->own_addr), ret, hapd->conf->iface);
		ret = ieee802_11_set_beacon(hapd);
		wpa_printf(MSG_DEBUG, "set beacon called for bssid " MACSTR " ret %d \n",
			   MAC2STR(hapd->own_addr), ret);
#ifdef CONFIG_IEEE80211BN
		if (ret == 0 && !hapd->mapc_initialized) {
			if (mapc_init(hapd) < 0)
				wpa_printf(MSG_ERROR, "MAPC: init failed for BSS %s",
					   hapd->conf->iface);
		}
#endif /* CONFIG_IEEE80211BN */
	}

	atf_bh_join_leave_update(iface, true);
	return ucv_boolean_new(true);
}

static uc_value_t *
uc_hostapd_iface_switch_channel(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_iface *iface = uc_fn_thisval("hostapd.iface");
	uc_value_t *info = uc_fn_arg(0);
	struct hostapd_config *conf;
	struct csa_settings csa = {};
	uint64_t intval;
	int ret = 0;
	bool mesh_origin = false;
#ifdef CONFIG_QCN_EXTN
	bool is_dfs = false;
	char *wpa_state = NULL;
	uc_value_t *wpa_state_val;
#else
	int i;
#endif

	if (!iface || ucv_type(info) != UC_OBJECT)
		return NULL;
#ifdef CONFIG_QCN_EXTN
	iface->iface_extn.vap_type = ucv_int64_get(ucv_object_get(info, "vap_type", NULL));
	if (!errno) {
		wpa_printf(MSG_DEBUG, "%s: VAP type: %s", __func__,
				vap_type_to_string(iface->iface_extn.vap_type));
	}
#endif

	conf = iface->conf;
	/* Initialize power_mode from current configured type;
	 * explicit "power_mode" key in the ucode object overrides below. */
	if (is_6ghz_freq(iface->freq))
		csa.power_mode = conf->he_6ghz_reg_pwr_type;
	else
		csa.power_mode = -1;
	if ((intval = ucv_int64_get(ucv_object_get(info, "csa_count", NULL))) && !errno)
		csa.cs_count = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "sec_channel", NULL))) && !errno)
		csa.freq_params.sec_channel_offset = intval;

	csa.freq_params.ht_enabled = conf->ieee80211n;
	csa.freq_params.vht_enabled = conf->ieee80211ac;
	csa.freq_params.he_enabled = conf->ieee80211ax;
#ifdef CONFIG_IEEE80211BE
	csa.freq_params.eht_enabled = conf->ieee80211be;
#endif
#ifdef CONFIG_IEEE80211BN
	csa.freq_params.uhr_enabled = conf->ieee80211bn;
#endif
	intval = ucv_int64_get(ucv_object_get(info, "oper_chwidth", NULL));
	if (errno)
		intval = hostapd_get_oper_chwidth(conf);
	if (intval == CONF_OPER_CHWIDTH_320MHZ)
		csa.freq_params.bandwidth = 320;
	else if (intval)
		csa.freq_params.bandwidth = 40 << intval;
	else
		csa.freq_params.bandwidth = csa.freq_params.sec_channel_offset ? 40 : 20;

	if ((intval = ucv_int64_get(ucv_object_get(info, "frequency", NULL))) && !errno)
		csa.freq_params.freq = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "center_freq1", NULL))) && !errno)
		csa.freq_params.center_freq1 = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "center_freq2", NULL))) && !errno)
		csa.freq_params.center_freq2 = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "punct_bitmap", NULL))) && !errno)
		csa.freq_params.punct_bitmap = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "power_mode", NULL))) && !errno)
		csa.power_mode = intval;

	mesh_origin = ucv_boolean_get(ucv_object_get(info, "mesh_origin", NULL));
	if (!mesh_origin) {
		hostapd_ubus_mesh_switch_channel(iface, &csa);
	}

#ifdef CONFIG_QCN_EXTN
	if ((intval = ucv_int64_get(ucv_object_get(info, "mcst", NULL))) && !errno)
		csa.mcst = intval;
	csa.freq_params.mcst = csa.mcst;
	iface->mcst = csa.mcst;

	is_dfs = ucv_boolean_get(ucv_object_get(info, "is_dfs", NULL));
	wpa_state_val = ucv_object_get(info, "wpa_state", NULL);
	wpa_state = ucv_string_get(wpa_state_val);
	if (wpa_state) {
		os_strlcpy(iface->iface_extn.sta_wpa_state, wpa_state,
				sizeof(iface->iface_extn.sta_wpa_state));
		hostapd_update_bh_sta_connected_extn(iface, wpa_state);
	}

	ret = uc_hostapd_iface_switch_channel_extn(iface, is_dfs,
					iface->iface_extn.sta_wpa_state, &csa);
#else
	for (i = 0; i < iface->num_bss; i++)
		ret = hostapd_switch_channel(iface->bss[i], &csa);
#endif

	return ucv_boolean_new(!ret);
}

static uc_value_t *
uc_hostapd_bss_rename(uc_vm_t *vm, size_t nargs)
{
	struct hostapd_data *hapd = uc_fn_thisval("hostapd.bss");
	uc_value_t *ifname_arg = uc_fn_arg(0);
	char prev_ifname[IFNAMSIZ + 1];
	struct sta_info *sta;
	const char *ifname;
	int ret;

	if (!hapd || ucv_type(ifname_arg) != UC_STRING)
		return NULL;

	os_strlcpy(prev_ifname, hapd->conf->iface, sizeof(prev_ifname));
	ifname = ucv_string_get(ifname_arg);

	hostapd_ubus_free_bss(hapd);
	if (interfaces->ctrl_iface_deinit)
		interfaces->ctrl_iface_deinit(hapd);

	ret = hostapd_drv_if_rename(hapd, WPA_IF_AP_BSS, NULL, ifname);
	if (ret)
		goto out;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		char cur_name[IFNAMSIZ + 1], new_name[IFNAMSIZ + 1];

		if (!(sta->flags & WLAN_STA_WDS) || sta->pending_wds_enable)
			continue;

		snprintf(cur_name, sizeof(cur_name), "%s.sta%d", prev_ifname, sta->aid);
		snprintf(new_name, sizeof(new_name), "%s.sta%d", ifname, sta->aid);
		hostapd_drv_if_rename(hapd, WPA_IF_AP_VLAN, cur_name, new_name);
	}

	if (!strncmp(hapd->conf->ssid.vlan, hapd->conf->iface, sizeof(hapd->conf->ssid.vlan)))
		os_strlcpy(hapd->conf->ssid.vlan, ifname, sizeof(hapd->conf->ssid.vlan));
	os_strlcpy(hapd->conf->iface, ifname, sizeof(hapd->conf->iface));

	hostapd_set_ctrl_sock_iface(hapd);

	hostapd_ubus_add_bss(hapd);

	hostapd_ucode_update_interfaces();
out:
	if (interfaces->ctrl_iface_init)
		interfaces->ctrl_iface_init(hapd);

	return ret ? NULL : ucv_boolean_new(true);
}


int hostapd_ucode_init(struct hapd_interfaces *ifaces)
{
	static const uc_function_list_t global_fns[] = {
		{ "printf",	uc_wpa_printf },
		{ "getpid", uc_wpa_getpid },
		{ "sha1", uc_wpa_sha1 },
		{ "freq_info", uc_wpa_freq_info },
		{ "add_iface", uc_hostapd_add_iface },
		{ "remove_iface", uc_hostapd_remove_iface },
		{ "udebug_set", uc_wpa_udebug_set },
	};
	static const uc_function_list_t bss_fns[] = {
		{ "ctrl", uc_hostapd_bss_ctrl },
		{ "set_config", uc_hostapd_bss_set_config },
		{ "rename", uc_hostapd_bss_rename },
		{ "delete", uc_hostapd_bss_delete },
	};
	static const uc_function_list_t iface_fns[] = {
		{ "set_bss_order", uc_hostapd_iface_set_bss_order },
		{ "add_bss", uc_hostapd_iface_add_bss },
		{ "stop", uc_hostapd_iface_stop },
		{ "start", uc_hostapd_iface_start },
		{ "switch_channel", uc_hostapd_iface_switch_channel },
	};

	interfaces = ifaces;
	vm = wpa_ucode_create_vm();

	global_type = uc_type_declare(vm, "hostapd.global", global_fns, NULL);
	bss_type = uc_type_declare(vm, "hostapd.bss", bss_fns, NULL);
	iface_type = uc_type_declare(vm, "hostapd.iface", iface_fns, NULL);

	bss_registry = ucv_array_new(vm);
	uc_vm_registry_set(vm, "hostap.bss_registry", bss_registry);

	iface_registry = ucv_array_new(vm);
	uc_vm_registry_set(vm, "hostap.iface_registry", iface_registry);

	global = wpa_ucode_global_init("hostapd", global_type);

	if (wpa_ucode_run(HOSTAPD_UC_PATH "hostapd.uc"))
		goto free_vm;
	ucv_gc(vm);

	return 0;

free_vm:
	wpa_ucode_free_vm();
	return -1;
}

void hostapd_ucode_free(void)
{
	if (wpa_ucode_call_prepare("shutdown") == 0)
		ucv_put(wpa_ucode_call(0));
	wpa_ucode_free_vm();
}

void hostapd_ucode_free_iface(struct hostapd_iface *iface)
{
	wpa_ucode_registry_remove(iface_registry, iface->ucode.idx);
}

void hostapd_ucode_add_bss(struct hostapd_data *hapd)
{
	uc_value_t *val;
	char ifname[IFNAMSIZ + 10]; /* extra room for ":<phy>" suffix */

	if (wpa_ucode_call_prepare("bss_add"))
		return;

	val = hostapd_ucode_bss_get_uval(hapd);

	os_snprintf(ifname, sizeof(ifname), "%s:%s",
		    hapd->conf->iface, hapd->iface->phy);

	uc_value_push(ucv_get(ucv_string_new(ifname)));
	uc_value_push(ucv_get(val));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

void hostapd_ucode_reload_bss(struct hostapd_data *hapd)
{
	uc_value_t *val;
	char ifname[IFNAMSIZ + 10]; /* extra room for ":<phy>" suffix */

	if (wpa_ucode_call_prepare("bss_reload"))
		return;

	val = hostapd_ucode_bss_get_uval(hapd);

	os_snprintf(ifname, sizeof(ifname), "%s:%s",
		    hapd->conf->iface, hapd->iface->phy);

	uc_value_push(ucv_get(ucv_string_new(ifname)));
	uc_value_push(ucv_get(val));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

void hostapd_ucode_free_bss(struct hostapd_data *hapd)
{
	uc_value_t *val;

	val = wpa_ucode_registry_remove(bss_registry, hapd->ucode.idx);
	if (!val)
		return;

	hapd->ucode.idx = 0;
	if (wpa_ucode_call_prepare("bss_remove"))
		return;

	uc_value_push(ucv_string_new(hapd->conf->iface));
	uc_value_push(ucv_get(val));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

#ifdef CONFIG_IEEE80211AX
void hostapd_ucode_config_nft_table(char *table, bool add)
{
	uc_value_t *nft_add;

	if (wpa_ucode_call_prepare("config_nft_table"))
		return;

	wpa_printf(MSG_INFO, "%s: %s a NFT Table", __func__, add?"create":"delete");

	if (add)
		nft_add = ucv_boolean_new(true);
	else
		nft_add = ucv_boolean_new(false);

	uc_value_push(ucv_get(ucv_string_new(table)));
	uc_value_push(ucv_get(nft_add));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

void hostapd_ucode_config_nft_chain(struct hostapd_data *hapd, char *table,
				    char *chain, bool add)
{
	uc_value_t *nft_add;

	if (wpa_ucode_call_prepare("config_nft_chain"))
		return;

	if (add)
		nft_add = ucv_boolean_new(true);
	else
		nft_add = ucv_boolean_new(false);

	wpa_printf(MSG_INFO, "%s: Create a NFT Chain", __func__);

	uc_value_push(ucv_get(ucv_string_new(table)));
	uc_value_push(ucv_get(ucv_string_new(chain)));
	uc_value_push(ucv_get(ucv_string_new(hapd->conf->iface)));
	uc_value_push(ucv_get(nft_add));
	ucv_put(wpa_ucode_call(4));
	ucv_gc(vm);
}

void hostapd_ucode_config_nft_rule(struct hostapd_data *hapd,
				   struct hostapd_nft_rule_params *rparams,
				   bool add)
{
	uc_value_t *nft_add;

	char v4_src_addr[INET_ADDRSTRLEN] = {0};
	char v4_dst_addr[INET_ADDRSTRLEN] = {0};
	char v6_src_addr[INET6_ADDRSTRLEN] = {0};
	char v6_dst_addr[INET6_ADDRSTRLEN] = {0};
	char dst_mac_addr[18] = {0};

	if (rparams->ip_family == 4) {
		if ((rparams->valid_flags & NFT_RULE_PARAM_SADDR))
			inet_ntop(AF_INET, &rparams->saddr4, v4_src_addr,
				  sizeof(v4_src_addr));
		if ((rparams->valid_flags & NFT_RULE_PARAM_DADDR))
			inet_ntop(AF_INET, &rparams->daddr4, v4_dst_addr,
				  sizeof(v4_dst_addr));
	}

	if (rparams->ip_family == 6) {
		if ((rparams->valid_flags & NFT_RULE_PARAM_SADDR))
			inet_ntop(AF_INET6, &rparams->saddr6, v6_src_addr,
				  sizeof(v6_src_addr));
		if ((rparams->valid_flags & NFT_RULE_PARAM_DADDR))
			inet_ntop(AF_INET6, &rparams->daddr6, v6_dst_addr,
				  sizeof(v6_src_addr));
	}

	snprintf(dst_mac_addr, sizeof(dst_mac_addr),
		 "%02x:%02x:%02x:%02x:%02x:%02x",
		 rparams->dmac[0], rparams->dmac[1], rparams->dmac[2],
		 rparams->dmac[3], rparams->dmac[4], rparams->dmac[5]);

	if (wpa_ucode_call_prepare("config_nft_rule"))
		return;

	wpa_printf(MSG_INFO, "%s: Create a NFT Rule", __func__);

	if (add)
		nft_add = ucv_boolean_new(true);
	else
		nft_add = ucv_boolean_new(false);

	uc_value_push(ucv_get(ucv_string_new(rparams->table)));
	uc_value_push(ucv_get(ucv_string_new(rparams->chain)));
	uc_value_push(ucv_get(ucv_string_new(hapd->conf->iface)));
	uc_value_push(ucv_get(nft_add));

	uc_value_push(ucv_get(ucv_string_new(dst_mac_addr)));
	uc_value_push(ucv_get(ucv_int64_new(rparams->proto)));
	uc_value_push(ucv_get(ucv_string_new(v6_src_addr)));
	uc_value_push(ucv_get(ucv_string_new(v6_dst_addr)));
	uc_value_push(ucv_get(ucv_string_new(v4_src_addr)));
	uc_value_push(ucv_get(ucv_string_new(v4_dst_addr)));

	uc_value_push(ucv_get(ucv_int64_new(rparams->sport)));
	uc_value_push(ucv_get(ucv_int64_new(rparams->dport)));
	uc_value_push(ucv_get(ucv_int64_new(rparams->mark)));
	uc_value_push(ucv_get(ucv_int64_new(rparams->esp_spi)));
	uc_value_push(ucv_get(ucv_int64_new(rparams->dscp)));
	uc_value_push(ucv_get(ucv_int64_new(rparams->ip_family)));
	ucv_put(wpa_ucode_call(16));
	ucv_gc(vm);
}
#endif

bool hostapd_ucode_update_radio_mask(char *ifname, u8 hw_idx)
{
	if (wpa_ucode_call_prepare("update_radio_mask"))
		return false;

	uc_value_push(ucv_string_new(ifname));
	uc_value_push(ucv_int64_new(hw_idx));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);

	return true;
}

int hostapd_ucode_get_sta_channel_per_band(struct hostapd_iface *iface,
					   int band,
					   struct hostapd_freq_params *freq)
{
	uc_value_t *ret, *info;
	int64_t val;

	if (!iface || !freq)
		return -EINVAL;
	if (!vm)
		return -ENODEV;

	if (wpa_ucode_call_prepare("get_sta_channel_per_band"))
		return -ENOENT;

	uc_value_push(ucv_get(hostapd_ucode_iface_get_uval(iface)));
	uc_value_push(ucv_int64_new(band));
	ret = wpa_ucode_call(2);

	if (!ret)
		return -ENODATA;

	if (ucv_type(ret) != UC_OBJECT) {
		ucv_put(ret);
		return -ENODATA;
	}

	info = ucv_object_get(ret, "channel", NULL);
	if (!info)
		info = ucv_object_get(ret, "channel_info", NULL);

	if (!info) {
		wpa_printf(MSG_DEBUG,
			   "%s: No channel info in response", __func__);
		ucv_put(ret);
		return -ENODATA;
	}

	if (ucv_type(info) != UC_OBJECT) {
		wpa_printf(MSG_DEBUG,
			   "%s: Invalid channel info type", __func__);
		ucv_put(ret);
		return -ENODATA;
	}

	os_memset(freq, 0, sizeof(*freq));

	val = ucv_int64_get(ucv_object_get(info, "frequency", NULL));
	if (!errno)
		freq->freq = val;

	val = ucv_int64_get(ucv_object_get(info, "sec_channel_offset", NULL));
	if (!errno)
		freq->sec_channel_offset = val;

	val = ucv_int64_get(ucv_object_get(info, "center_freq1", NULL));
	if (!errno)
		freq->center_freq1 = val;

	val = ucv_int64_get(ucv_object_get(info, "center_freq2", NULL));
	if (!errno)
		freq->center_freq2 = val;

	val = ucv_int64_get(ucv_object_get(info, "bandwidth", NULL));
	if (!errno)
		freq->bandwidth = val;

	val = ucv_int64_get(ucv_object_get(info, "punct_bitmap", NULL));
	if (!errno)
		freq->punct_bitmap = val;

	ucv_put(ret);
	ucv_gc(vm);
	return 0;
}

#ifdef CONFIG_QCN_EXTN
/**
 * hostapd_ucode_chsw_result_ev_notify - Notify ucode about CSA/CAC result
 * @hapd: Pointer to hostapd BSS instance
 * @freq: Operating frequency (in MHz) on which CSA/CAC was requested
 * @ret:  Channel switch result (0 on success, otherwise failure)
 *
 * Send a notification event to the ucode runtime indicating that a channel
 * switch announcement (CSA) or CAC sequence has completed on this interface.
 * This is used by repeater logic to resume or restart STA connection flows
 * after the AP side has attempted to switch channels. The @ret parameter
 * allows ucode to distinguish between successful completion and failure.
 */
void hostapd_ucode_chsw_result_ev_notify(struct hostapd_data *hapd, int freq,
					 int ret)
{
	if (wpa_ucode_call_prepare("notify_chan_switch_result_event"))
		return;

	wpa_printf(MSG_INFO,
		   "Notify channel switch result on all links"
		   "wpa_supp to handle STA connection Freq = %d ret = %d",
		   freq, ret);
	uc_value_push(ucv_int64_new(freq));
	uc_value_push(ucv_int64_new(ret));
	ucv_put(wpa_ucode_call(2));
	ucv_gc(vm);
}

/**
 * hostapd_ucode_notify_acs_start - Notify ucode about ACS start event
 * @iface: Hostapd Interface
 */
void hostapd_ucode_notify_acs_start(struct hostapd_iface *iface)
{
	if (wpa_ucode_call_prepare("notify_acs_start"))
		return;

	iface->iface_extn.acs_success = 0;
	iface->iface_extn.acs_failed = 0;
	wpa_printf(MSG_INFO, "Notify ACS start event to ucode");
	uc_value_push(ucv_get(hostapd_ucode_iface_get_uval(iface)));
	ucv_put(wpa_ucode_call(1));
	ucv_gc(vm);
}

/* Notify ucode about ACS completed event */
void hostapd_ucode_notify_acs_completed(struct hostapd_iface *iface, int success)
{
	if (wpa_ucode_call_prepare("notify_acs_completed"))
		return;

	wpa_printf(MSG_INFO, "Notify ACS completed event to ucode: success=%d, channel=%d, freq=%d",
		   success, iface->conf ? iface->conf->channel : 0, iface->freq);
	uc_value_push(ucv_get(hostapd_ucode_iface_get_uval(iface)));
	uc_value_push(ucv_int64_new(success));
	uc_value_push(ucv_int64_new(iface->conf ? iface->conf->channel : 0));
	uc_value_push(ucv_int64_new(iface->freq));
	ucv_put(wpa_ucode_call(4));
	ucv_gc(vm);
}
#endif
