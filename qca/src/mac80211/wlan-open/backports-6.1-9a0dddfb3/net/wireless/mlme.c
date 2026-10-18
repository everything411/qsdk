// SPDX-License-Identifier: GPL-2.0
/*
 * cfg80211 MLME SAP interface
 *
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2015		Intel Deutschland GmbH
 * Copyright (C) 2019-2020, 2022-2025 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/nl80211.h>
#include <linux/slab.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <net/iw_handler.h>
#include "core.h"
#include "nl80211.h"
#include "rdev-ops.h"
#ifdef CPTCFG_QCN_EXTN
#include "cfg80211_dfs_extn.h"
#endif /* CPTCFG_QCN_EXTN */


#ifdef CPTCFG_QCA_LAB_TEST_FEATURES
static unsigned int cfg80211_dfs_nol_time;
module_param_named(dfs_nol_timeout, cfg80211_dfs_nol_time, uint, 0644);
MODULE_PARM_DESC(dfs_nol_timeout,
		 "DFS NOL timeout in seconds (0 uses default 30 minutes)");

static unsigned long cfg80211_get_nol_time(unsigned long default_time)
{
	u64 nol_time;

	if (!cfg80211_dfs_nol_time)
		return default_time;

	nol_time = (u64)cfg80211_dfs_nol_time * MSEC_PER_SEC;

	return (unsigned long)min_t(u64, nol_time, U32_MAX);
}
#endif

void cfg80211_rx_assoc_resp(struct net_device *dev,
			    const struct cfg80211_rx_assoc_resp_data *data)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)data->buf;
	struct cfg80211_connect_resp_params cr = {
		.timeout_reason = NL80211_TIMEOUT_UNSPECIFIED,
		.req_ie = data->req_ies,
		.req_ie_len = data->req_ies_len,
		.resp_ie = mgmt->u.assoc_resp.variable,
		.resp_ie_len = (data->len >= offsetof(struct ieee80211_mgmt,
						   u.assoc_resp.variable)) ?
			       data->len - offsetof(struct ieee80211_mgmt,
						   u.assoc_resp.variable) : 0,
		.status = le16_to_cpu(mgmt->u.assoc_resp.status_code),
		.ap_mld_addr = data->ap_mld_addr,
	};
	unsigned int link_id;

	for (link_id = 0; link_id < ARRAY_SIZE(data->links); link_id++) {
		cr.links[link_id].status = data->links[link_id].status;
		cr.links[link_id].bss = data->links[link_id].bss;

		WARN_ON_ONCE(cr.links[link_id].status != WLAN_STATUS_SUCCESS &&
			     (!cr.ap_mld_addr || !cr.links[link_id].bss));

		if (!cr.links[link_id].bss)
			continue;
		cr.links[link_id].bssid = data->links[link_id].bss->bssid;
		cr.links[link_id].addr = data->links[link_id].addr;
		/* need to have local link addresses for MLO connections */
		WARN_ON(cr.ap_mld_addr &&
			!is_valid_ether_addr(cr.links[link_id].addr));

		BUG_ON(!cr.links[link_id].bss->channel);

		if (cr.links[link_id].bss->channel->band == NL80211_BAND_S1GHZ) {
			WARN_ON(link_id);
			cr.resp_ie = (u8 *)&mgmt->u.s1g_assoc_resp.variable;
			cr.resp_ie_len = (data->len >=
					  offsetof(struct ieee80211_mgmt,
						   u.s1g_assoc_resp.variable)) ?
					 data->len -
					 offsetof(struct ieee80211_mgmt,
						  u.s1g_assoc_resp.variable) : 0;
		}

		if (cr.ap_mld_addr)
			cr.valid_links |= BIT(link_id);
	}

	trace_cfg80211_send_rx_assoc(dev, data);

	/*
	 * This is a bit of a hack, we don't notify userspace of
	 * a (re-)association reply if we tried to send a reassoc
	 * and got a reject -- we only try again with an assoc
	 * frame instead of reassoc.
	 */
	if (cfg80211_sme_rx_assoc_resp(wdev, cr.status)) {
		for (link_id = 0; link_id < ARRAY_SIZE(data->links); link_id++) {
			struct cfg80211_bss *bss = data->links[link_id].bss;

			if (!bss)
				continue;

			cfg80211_unhold_bss(bss_from_pub(bss));
			cfg80211_put_bss(wiphy, bss);
		}
		return;
	}

	nl80211_send_rx_assoc(rdev, dev, data, &cr);
	/* update current_bss etc., consumes the bss reference */
	__cfg80211_connect_result(dev, &cr, cr.status == WLAN_STATUS_SUCCESS);
}
EXPORT_SYMBOL(cfg80211_rx_assoc_resp);

static void cfg80211_process_auth(struct wireless_dev *wdev,
				  const u8 *buf, size_t len)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);

	nl80211_send_rx_auth(rdev, wdev->netdev, buf, len, GFP_KERNEL);
	cfg80211_sme_rx_auth(wdev, buf, len);
}

static void cfg80211_process_deauth(struct wireless_dev *wdev,
				    const u8 *buf, size_t len,
				    bool reconnect)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)buf;
	const u8 *bssid;
	u16 reason_code;
	bool from_ap;

	if (len < offsetof(struct ieee80211_mgmt, u.deauth.reason_code) +
		  sizeof(mgmt->u.deauth.reason_code))
		return;

	bssid = mgmt->bssid;
	reason_code = le16_to_cpu(mgmt->u.deauth.reason_code);
	from_ap = !ether_addr_equal(mgmt->sa, wdev->netdev->dev_addr);

	nl80211_send_deauth(rdev, wdev->netdev, buf, len, reconnect, GFP_KERNEL);

	if (!wdev->connected || !ether_addr_equal(wdev->u.client.connected_addr, bssid))
		return;

	__cfg80211_disconnected(wdev->netdev, NULL, 0, reason_code, from_ap);
	cfg80211_sme_deauth(wdev);
}

static void cfg80211_process_disassoc(struct wireless_dev *wdev,
				      const u8 *buf, size_t len,
				      bool reconnect)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)buf;
	const u8 *bssid;
	u16 reason_code;
	bool from_ap;

	if (len < offsetof(struct ieee80211_mgmt, u.disassoc.reason_code) +
		  sizeof(mgmt->u.disassoc.reason_code))
		return;

	bssid = mgmt->bssid;
	reason_code = le16_to_cpu(mgmt->u.disassoc.reason_code);
	from_ap = !ether_addr_equal(mgmt->sa, wdev->netdev->dev_addr);

	nl80211_send_disassoc(rdev, wdev->netdev, buf, len, reconnect,
			      GFP_KERNEL);

	if (WARN_ON(!wdev->connected ||
		    !ether_addr_equal(wdev->u.client.connected_addr, bssid)))
		return;

	__cfg80211_disconnected(wdev->netdev, NULL, 0, reason_code, from_ap);
	cfg80211_sme_disassoc(wdev);
}

void cfg80211_rx_mlme_mgmt(struct net_device *dev, const u8 *buf, size_t len)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct ieee80211_mgmt *mgmt = (void *)buf;

	lockdep_assert_wiphy(wdev->wiphy);

	trace_cfg80211_rx_mlme_mgmt(dev, buf, len);

	if (WARN_ON(len < 2))
		return;

	if (ieee80211_is_auth(mgmt->frame_control))
		cfg80211_process_auth(wdev, buf, len);
	else if (ieee80211_is_deauth(mgmt->frame_control))
		cfg80211_process_deauth(wdev, buf, len, false);
	else if (ieee80211_is_disassoc(mgmt->frame_control))
		cfg80211_process_disassoc(wdev, buf, len, false);
}
EXPORT_SYMBOL(cfg80211_rx_mlme_mgmt);

void cfg80211_auth_timeout(struct net_device *dev, const u8 *addr)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	trace_cfg80211_send_auth_timeout(dev, addr);

	nl80211_send_auth_timeout(rdev, dev, addr, GFP_KERNEL);
	cfg80211_sme_auth_timeout(wdev);
}
EXPORT_SYMBOL(cfg80211_auth_timeout);

void cfg80211_assoc_failure(struct net_device *dev,
			    struct cfg80211_assoc_failure *data)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	const u8 *addr = data->ap_mld_addr ?: data->bss[0]->bssid;
	int i;

	trace_cfg80211_send_assoc_failure(dev, data);

	if (data->timeout) {
		nl80211_send_assoc_timeout(rdev, dev, addr, GFP_KERNEL);
		cfg80211_sme_assoc_timeout(wdev);
	} else {
		cfg80211_sme_abandon_assoc(wdev);
	}

	for (i = 0; i < ARRAY_SIZE(data->bss); i++) {
		struct cfg80211_bss *bss = data->bss[i];

		if (!bss)
			continue;

		cfg80211_unhold_bss(bss_from_pub(bss));
		cfg80211_put_bss(wiphy, bss);
	}
}
EXPORT_SYMBOL(cfg80211_assoc_failure);

void cfg80211_tx_mlme_mgmt(struct net_device *dev, const u8 *buf, size_t len,
			   bool reconnect)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct ieee80211_mgmt *mgmt = (void *)buf;

	lockdep_assert_wiphy(wdev->wiphy);

	trace_cfg80211_tx_mlme_mgmt(dev, buf, len, reconnect);

	if (WARN_ON(len < 2))
		return;

	if (ieee80211_is_deauth(mgmt->frame_control))
		cfg80211_process_deauth(wdev, buf, len, reconnect);
	else
		cfg80211_process_disassoc(wdev, buf, len, reconnect);
}
EXPORT_SYMBOL(cfg80211_tx_mlme_mgmt);

void cfg80211_michael_mic_failure(struct net_device *dev, const u8 *addr,
				  enum nl80211_key_type key_type, int key_id,
				  const u8 *tsc, gfp_t gfp)
{
	struct wiphy *wiphy = dev->ieee80211_ptr->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
#ifdef CPTCFG_CFG80211_WEXT
	union iwreq_data wrqu;
	char *buf = kmalloc(128, gfp);

	if (buf) {
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length =
			sprintf(buf, "MLME-MICHAELMICFAILURE."
				"indication(keyid=%d %scast addr=%pM)",
				key_id, key_type == NL80211_KEYTYPE_GROUP
				? "broad" : "uni", addr);
		wireless_send_event(dev, IWEVCUSTOM, &wrqu, buf);
		kfree(buf);
	}
#endif

	trace_cfg80211_michael_mic_failure(dev, addr, key_type, key_id, tsc);
	nl80211_michael_mic_failure(rdev, dev, addr, key_type, key_id, tsc, gfp);
}
EXPORT_SYMBOL(cfg80211_michael_mic_failure);

/* some MLME handling for userspace SME */
int cfg80211_mlme_auth(struct cfg80211_registered_device *rdev,
		       struct net_device *dev,
		       struct cfg80211_auth_request *req)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	lockdep_assert_wiphy(wdev->wiphy);

	if (!req->bss)
		return -ENOENT;

	if (req->link_id >= 0 &&
	    !(wdev->wiphy->flags & WIPHY_FLAG_SUPPORTS_MLO))
		return -EINVAL;

	if (req->auth_type == NL80211_AUTHTYPE_SHARED_KEY) {
		if (!req->key || !req->key_len ||
		    req->key_idx < 0 || req->key_idx > 3)
			return -EINVAL;
	}

	if (wdev->connected &&
	    ether_addr_equal(req->bss->bssid, wdev->u.client.connected_addr))
		return -EALREADY;

	if (ether_addr_equal(req->bss->bssid, dev->dev_addr) ||
	    (req->link_id >= 0 &&
	     ether_addr_equal(req->ap_mld_addr, dev->dev_addr)))
		return -EINVAL;

	return rdev_auth(rdev, dev, req);
}

/*  Do a logical ht_capa &= ht_capa_mask.  */
void cfg80211_oper_and_ht_capa(struct ieee80211_ht_cap *ht_capa,
			       const struct ieee80211_ht_cap *ht_capa_mask)
{
	int i;
	u8 *p1, *p2;
	if (!ht_capa_mask) {
		memset(ht_capa, 0, sizeof(*ht_capa));
		return;
	}

	p1 = (u8*)(ht_capa);
	p2 = (u8*)(ht_capa_mask);
	for (i = 0; i < sizeof(*ht_capa); i++)
		p1[i] &= p2[i];
}

/*  Do a logical vht_capa &= vht_capa_mask.  */
void cfg80211_oper_and_vht_capa(struct ieee80211_vht_cap *vht_capa,
				const struct ieee80211_vht_cap *vht_capa_mask)
{
	int i;
	u8 *p1, *p2;
	if (!vht_capa_mask) {
		memset(vht_capa, 0, sizeof(*vht_capa));
		return;
	}

	p1 = (u8*)(vht_capa);
	p2 = (u8*)(vht_capa_mask);
	for (i = 0; i < sizeof(*vht_capa); i++)
		p1[i] &= p2[i];
}

static int
cfg80211_mlme_check_mlo_compat(const struct ieee80211_multi_link_elem *mle_a,
			       const struct ieee80211_multi_link_elem *mle_b,
			       struct netlink_ext_ack *extack)
{
	const struct ieee80211_mle_basic_common_info *common_a, *common_b;

	common_a = (const void *)mle_a->variable;
	common_b = (const void *)mle_b->variable;

	if (memcmp(common_a->mld_mac_addr, common_b->mld_mac_addr, ETH_ALEN)) {
		NL_SET_ERR_MSG(extack, "AP MLD address mismatch");
		return -EINVAL;
	}

	if (ieee80211_mle_get_eml_cap((const u8 *)mle_a) !=
	    ieee80211_mle_get_eml_cap((const u8 *)mle_b)) {
		NL_SET_ERR_MSG(extack, "link EML capabilities mismatch");
		return -EINVAL;
	}

	if (ieee80211_mle_get_mld_capa_op((const u8 *)mle_a) !=
	    ieee80211_mle_get_mld_capa_op((const u8 *)mle_b)) {
		NL_SET_ERR_MSG(extack, "link MLD capabilities/ops mismatch");
		return -EINVAL;
	}

	if (ieee80211_mle_get_ext_mld_capa_op((const u8 *)mle_a) !=
	    ieee80211_mle_get_ext_mld_capa_op((const u8 *)mle_b)) {
		NL_SET_ERR_MSG(extack,
			       "extended link MLD capabilities/ops mismatch");
		return -EINVAL;
	}

	return 0;
}

static int cfg80211_mlme_check_mlo(struct net_device *dev,
				   struct cfg80211_assoc_request *req,
				   struct netlink_ext_ack *extack)
{
	const struct ieee80211_multi_link_elem *mles[ARRAY_SIZE(req->links)] = {};
	int i;

	if (req->link_id < 0)
		return 0;

	if (req->link_id >= ARRAY_SIZE(req->links)) {
		NL_SET_ERR_MSG(extack, "invalid assoc link ID");
		return -EINVAL;
	}

	if (!req->links[req->link_id].bss) {
		NL_SET_ERR_MSG(extack, "no BSS for assoc link");
		return -EINVAL;
	}

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(req->links); i++) {
		const struct cfg80211_bss_ies *ies;
		const struct element *ml;

		if (!req->links[i].bss)
			continue;

		if (ether_addr_equal(req->links[i].bss->bssid, dev->dev_addr)) {
			NL_SET_ERR_MSG(extack, "BSSID must not be our address");
			req->links[i].error = -EINVAL;
			goto error;
		}

		ies = rcu_dereference(req->links[i].bss->ies);
		if (!ies) {
			NL_SET_ERR_MSG(extack, "BSS missing IEs");
			req->links[i].error = -EINVAL;
			goto error;
		}
		ml = cfg80211_find_ext_elem(WLAN_EID_EXT_EHT_MULTI_LINK,
					    ies->data, ies->len);
		if (!ml) {
			NL_SET_ERR_MSG(extack, "MLO BSS w/o ML element");
			req->links[i].error = -EINVAL;
			goto error;
		}

		if (!ieee80211_mle_type_ok(ml->data + 1,
					   IEEE80211_ML_CONTROL_TYPE_BASIC,
					   ml->datalen - 1)) {
			NL_SET_ERR_MSG(extack, "BSS with invalid ML element");
			req->links[i].error = -EINVAL;
			goto error;
		}

		mles[i] = (const void *)(ml->data + 1);

		if (ieee80211_mle_get_link_id((const u8 *)mles[i]) != i) {
			NL_SET_ERR_MSG(extack, "link ID mismatch");
			req->links[i].error = -EINVAL;
			goto error;
		}
	}

	if (WARN_ON(!mles[req->link_id]))
		goto error;

	for (i = 0; i < ARRAY_SIZE(req->links); i++) {
		if (i == req->link_id || !req->links[i].bss)
			continue;

		if (WARN_ON(!mles[i]))
			goto error;

		if (cfg80211_mlme_check_mlo_compat(mles[req->link_id], mles[i],
						   extack)) {
			req->links[i].error = -EINVAL;
			goto error;
		}
	}

	rcu_read_unlock();
	return 0;
error:
	rcu_read_unlock();
	return -EINVAL;
}

/* Note: caller must cfg80211_put_bss() regardless of result */
int cfg80211_mlme_assoc(struct cfg80211_registered_device *rdev,
			struct net_device *dev,
			struct cfg80211_assoc_request *req,
			struct netlink_ext_ack *extack)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	int err;

	lockdep_assert_wiphy(wdev->wiphy);

	err = cfg80211_mlme_check_mlo(dev, req, extack);
	if (err)
		return err;

	if (wdev->connected &&
	    (!req->prev_bssid ||
	     !ether_addr_equal(wdev->u.client.connected_addr, req->prev_bssid)))
		return -EALREADY;

	if ((req->bss && ether_addr_equal(req->bss->bssid, dev->dev_addr)) ||
	    (req->link_id >= 0 &&
	     ether_addr_equal(req->ap_mld_addr, dev->dev_addr)))
		return -EINVAL;

	cfg80211_oper_and_ht_capa(&req->ht_capa_mask,
				  rdev->wiphy.ht_capa_mod_mask);
	cfg80211_oper_and_vht_capa(&req->vht_capa_mask,
				   rdev->wiphy.vht_capa_mod_mask);

	err = rdev_assoc(rdev, dev, req);
	if (!err) {
		int link_id;

		if (req->bss) {
			cfg80211_ref_bss(&rdev->wiphy, req->bss);
			cfg80211_hold_bss(bss_from_pub(req->bss));
		}

		for (link_id = 0; link_id < ARRAY_SIZE(req->links); link_id++) {
			if (!req->links[link_id].bss)
				continue;
			cfg80211_ref_bss(&rdev->wiphy, req->links[link_id].bss);
			cfg80211_hold_bss(bss_from_pub(req->links[link_id].bss));
		}
	}
	return err;
}

int cfg80211_mlme_deauth(struct cfg80211_registered_device *rdev,
			 struct net_device *dev, const u8 *bssid,
			 const u8 *ie, int ie_len, u16 reason,
			 bool local_state_change)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_deauth_request req = {
		.bssid = bssid,
		.reason_code = reason,
		.ie = ie,
		.ie_len = ie_len,
		.local_state_change = local_state_change,
	};

	lockdep_assert_wiphy(wdev->wiphy);

	if (local_state_change &&
	    (!wdev->connected ||
	     !ether_addr_equal(wdev->u.client.connected_addr, bssid)))
		return 0;

	if (ether_addr_equal(wdev->disconnect_bssid, bssid) ||
	    (wdev->connected &&
	     ether_addr_equal(wdev->u.client.connected_addr, bssid)))
		wdev->conn_owner_nlportid = 0;

	return rdev_deauth(rdev, dev, &req);
}

int cfg80211_mlme_disassoc(struct cfg80211_registered_device *rdev,
			   struct net_device *dev, const u8 *ap_addr,
			   const u8 *ie, int ie_len, u16 reason,
			   bool local_state_change)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_disassoc_request req = {
		.reason_code = reason,
		.local_state_change = local_state_change,
		.ie = ie,
		.ie_len = ie_len,
		.ap_addr = ap_addr,
	};
	int err;

	lockdep_assert_wiphy(wdev->wiphy);

	if (!wdev->connected)
		return -ENOTCONN;

	if (memcmp(wdev->u.client.connected_addr, ap_addr, ETH_ALEN))
		return -ENOTCONN;

	err = rdev_disassoc(rdev, dev, &req);
	if (err)
		return err;

	/* driver should have reported the disassoc */
	WARN_ON(wdev->connected);
	return 0;
}

void cfg80211_mlme_down(struct cfg80211_registered_device *rdev,
			struct net_device *dev)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	u8 bssid[ETH_ALEN];

	lockdep_assert_wiphy(wdev->wiphy);

	if (!rdev->ops->deauth)
		return;

	if (!wdev->connected)
		return;

	memcpy(bssid, wdev->u.client.connected_addr, ETH_ALEN);
	cfg80211_mlme_deauth(rdev, dev, bssid, NULL, 0,
			     WLAN_REASON_DEAUTH_LEAVING, false);
}

struct cfg80211_mgmt_registration {
	struct list_head list;
	struct wireless_dev *wdev;

	u32 nlportid;

	int match_len;

	__le16 frame_type;

	bool multicast_rx;

	u8 match[];
};

static void cfg80211_mgmt_registrations_update(struct wireless_dev *wdev)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	struct wireless_dev *tmp;
	struct cfg80211_mgmt_registration *reg;
	struct mgmt_frame_regs upd = {};

	lockdep_assert_held(&rdev->wiphy.mtx);

	spin_lock_bh(&rdev->mgmt_registrations_lock);
	if (!wdev->mgmt_registrations_need_update) {
		spin_unlock_bh(&rdev->mgmt_registrations_lock);
		return;
	}

	rcu_read_lock();
	list_for_each_entry_rcu(tmp, &rdev->wiphy.wdev_list, list) {
		list_for_each_entry(reg, &tmp->mgmt_registrations, list) {
			u32 mask = BIT(le16_to_cpu(reg->frame_type) >> 4);
			u32 mcast_mask = 0;

			if (reg->multicast_rx)
				mcast_mask = mask;

			upd.global_stypes |= mask;
			upd.global_mcast_stypes |= mcast_mask;

			if (tmp == wdev) {
				upd.interface_stypes |= mask;
				upd.interface_mcast_stypes |= mcast_mask;
			}
		}
	}
	rcu_read_unlock();

	wdev->mgmt_registrations_need_update = 0;
	spin_unlock_bh(&rdev->mgmt_registrations_lock);

	rdev_update_mgmt_frame_registrations(rdev, wdev, &upd);
}

void cfg80211_mgmt_registrations_update_wk(struct work_struct *wk)
{
	struct cfg80211_registered_device *rdev;
	struct wireless_dev *wdev;

	rdev = container_of(wk, struct cfg80211_registered_device,
			    mgmt_registrations_update_wk);

	guard(wiphy)(&rdev->wiphy);

	list_for_each_entry(wdev, &rdev->wiphy.wdev_list, list)
		cfg80211_mgmt_registrations_update(wdev);
}

int cfg80211_mlme_register_mgmt(struct wireless_dev *wdev, u32 snd_portid,
				u16 frame_type, const u8 *match_data,
				int match_len, bool multicast_rx,
				struct netlink_ext_ack *extack)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	struct cfg80211_mgmt_registration *reg, *nreg;
	int err = 0;
	u16 mgmt_type;
	bool update_multicast = false;

	if (!wdev->wiphy->mgmt_stypes)
		return -EOPNOTSUPP;

	if ((frame_type & IEEE80211_FCTL_FTYPE) != IEEE80211_FTYPE_MGMT) {
		NL_SET_ERR_MSG(extack, "frame type not management");
		return -EINVAL;
	}

	if (frame_type & ~(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) {
		NL_SET_ERR_MSG(extack, "Invalid frame type");
		return -EINVAL;
	}

	mgmt_type = (frame_type & IEEE80211_FCTL_STYPE) >> 4;
	if (wdev->iftype >= NUM_NL80211_IFTYPES)
		return -EINVAL;

	if (!(wdev->wiphy->mgmt_stypes[wdev->iftype].rx & BIT(mgmt_type))) {
		NL_SET_ERR_MSG(extack,
			       "Registration to specific type not supported");
		return -EINVAL;
	}

	/*
	 * To support Pre Association Security Negotiation (PASN), registration
	 * for authentication frames should be supported. However, as some
	 * versions of the user space daemons wrongly register to all types of
	 * authentication frames (which might result in unexpected behavior)
	 * allow such registration if the request is for a specific
	 * authentication algorithm number.
	 */
	if (wdev->iftype == NL80211_IFTYPE_STATION &&
	    (frame_type & IEEE80211_FCTL_STYPE) == IEEE80211_STYPE_AUTH &&
	    !(match_data && match_len >= 2)) {
		NL_SET_ERR_MSG(extack,
			       "Authentication algorithm number required");
		return -EINVAL;
	}

	if (match_len < 0)
		return -EINVAL;

	nreg = kzalloc(sizeof(*reg) + match_len, GFP_KERNEL);
	if (!nreg)
		return -ENOMEM;

	spin_lock_bh(&rdev->mgmt_registrations_lock);

	list_for_each_entry(reg, &wdev->mgmt_registrations, list) {
		int mlen = min(match_len, reg->match_len);

		if (frame_type != le16_to_cpu(reg->frame_type))
			continue;

		if (memcmp(reg->match, match_data, mlen) == 0) {
			if (reg->multicast_rx != multicast_rx) {
				update_multicast = true;
				reg->multicast_rx = multicast_rx;
				break;
			}
			NL_SET_ERR_MSG(extack, "Match already configured");
			err = -EALREADY;
			break;
		}
	}

	if (err)
		goto out;

	if (update_multicast) {
		kfree(nreg);
	} else {
		memcpy(nreg->match, match_data, match_len);
		nreg->match_len = match_len;
		nreg->nlportid = snd_portid;
		nreg->frame_type = cpu_to_le16(frame_type);
		nreg->wdev = wdev;
		nreg->multicast_rx = multicast_rx;
		list_add(&nreg->list, &wdev->mgmt_registrations);
	}
	wdev->mgmt_registrations_need_update = 1;
	spin_unlock_bh(&rdev->mgmt_registrations_lock);

	cfg80211_mgmt_registrations_update(wdev);

	return 0;

 out:
	kfree(nreg);
	spin_unlock_bh(&rdev->mgmt_registrations_lock);

	return err;
}

void cfg80211_mlme_unregister_socket(struct wireless_dev *wdev, u32 nlportid)
{
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct cfg80211_mgmt_registration *reg, *tmp;

	spin_lock_bh(&rdev->mgmt_registrations_lock);

	list_for_each_entry_safe(reg, tmp, &wdev->mgmt_registrations, list) {
		if (reg->nlportid != nlportid)
			continue;

		list_del(&reg->list);
		kfree(reg);

		wdev->mgmt_registrations_need_update = 1;
		schedule_work(&rdev->mgmt_registrations_update_wk);
	}

	spin_unlock_bh(&rdev->mgmt_registrations_lock);

	if (nlportid && rdev->crit_proto_nlportid == nlportid) {
		rdev->crit_proto_nlportid = 0;
		rdev_crit_proto_stop(rdev, wdev);
	}

	if (nlportid == wdev->ap_unexpected_nlportid)
		wdev->ap_unexpected_nlportid = 0;
}

void cfg80211_mlme_purge_registrations(struct wireless_dev *wdev)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	struct cfg80211_mgmt_registration *reg, *tmp;

	spin_lock_bh(&rdev->mgmt_registrations_lock);
	list_for_each_entry_safe(reg, tmp, &wdev->mgmt_registrations, list) {
		list_del(&reg->list);
		kfree(reg);
	}
	wdev->mgmt_registrations_need_update = 1;
	spin_unlock_bh(&rdev->mgmt_registrations_lock);

	cfg80211_mgmt_registrations_update(wdev);
}

static bool cfg80211_allowed_address(struct wireless_dev *wdev, const u8 *addr)
{
	int i;

	for_each_valid_link(wdev, i) {
		if (ether_addr_equal(addr, wdev->links[i].addr))
			return true;
	}

	return ether_addr_equal(addr, wdev_address(wdev));
}

static bool cfg80211_allowed_random_address(struct wireless_dev *wdev,
					    const struct ieee80211_mgmt *mgmt)
{
	if (ieee80211_is_auth(mgmt->frame_control) ||
	    ieee80211_is_deauth(mgmt->frame_control)) {
		/* Allow random TA to be used with authentication and
		 * deauthentication frames if the driver has indicated support.
		 */
		if (wiphy_ext_feature_isset(
			    wdev->wiphy,
			    NL80211_EXT_FEATURE_AUTH_AND_DEAUTH_RANDOM_TA))
			return true;
	} else if (ieee80211_is_action(mgmt->frame_control) &&
		   mgmt->u.action.category == WLAN_CATEGORY_PUBLIC) {
		/* Allow random TA to be used with Public Action frames if the
		 * driver has indicated support.
		 */
		if (!wdev->connected &&
		    wiphy_ext_feature_isset(
			    wdev->wiphy,
			    NL80211_EXT_FEATURE_MGMT_TX_RANDOM_TA))
			return true;

		if (wdev->connected &&
		    wiphy_ext_feature_isset(
			    wdev->wiphy,
			    NL80211_EXT_FEATURE_MGMT_TX_RANDOM_TA_CONNECTED))
			return true;
	}

	return false;
}

int cfg80211_mlme_mgmt_tx(struct cfg80211_registered_device *rdev,
			  struct wireless_dev *wdev,
			  struct cfg80211_mgmt_tx_params *params, u64 *cookie)
{
	const struct ieee80211_mgmt *mgmt;
	u16 stype;

	lockdep_assert_wiphy(&rdev->wiphy);

	if (!wdev->wiphy->mgmt_stypes)
		return -EOPNOTSUPP;

	if (!rdev->ops->mgmt_tx)
		return -EOPNOTSUPP;

	if (params->len < 24 + 1)
		return -EINVAL;

	mgmt = (const struct ieee80211_mgmt *)params->buf;

	if (!ieee80211_is_mgmt(mgmt->frame_control))
		return -EINVAL;

	stype = le16_to_cpu(mgmt->frame_control) & IEEE80211_FCTL_STYPE;
	if (wdev->iftype >= NUM_NL80211_IFTYPES)
		return -EINVAL;

	if (!(wdev->wiphy->mgmt_stypes[wdev->iftype].tx & BIT(stype >> 4)))
		return -EINVAL;

	if (ieee80211_is_action(mgmt->frame_control) &&
	    mgmt->u.action.category != WLAN_CATEGORY_PUBLIC) {
		int err = 0;

		switch (wdev->iftype) {
		case NL80211_IFTYPE_ADHOC:
			/*
			 * check for IBSS DA must be done by driver as
			 * cfg80211 doesn't track the stations
			 */
			if (!wdev->u.ibss.current_bss ||
			    !ether_addr_equal(wdev->u.ibss.current_bss->pub.bssid,
					      mgmt->bssid)) {
				err = -ENOTCONN;
				break;
			}
			break;
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_P2P_CLIENT:
			if (!wdev->connected) {
				err = -ENOTCONN;
				break;
			}

			/* FIXME: MLD may address this differently */

			if (!ether_addr_equal(wdev->u.client.connected_addr,
					      mgmt->bssid)) {
				err = -ENOTCONN;
				break;
			}

			/* for station, check that DA is the AP */
			if (!ether_addr_equal(wdev->u.client.connected_addr,
					      mgmt->da)) {
				err = -ENOTCONN;
				break;
			}
			break;
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_P2P_GO:
		case NL80211_IFTYPE_AP_VLAN:
			if (!ether_addr_equal(mgmt->bssid, wdev_address(wdev)) &&
			    (params->link_id < 0 ||
			     params->link_id >= ARRAY_SIZE(wdev->links) ||
			     !ether_addr_equal(mgmt->bssid,
					       wdev->links[params->link_id].addr)))
				err = -EINVAL;
			break;
		case NL80211_IFTYPE_MESH_POINT:
			if (!ether_addr_equal(mgmt->sa, mgmt->bssid)) {
				err = -EINVAL;
				break;
			}
			/*
			 * check for mesh DA must be done by driver as
			 * cfg80211 doesn't track the stations
			 */
			break;
		case NL80211_IFTYPE_P2P_DEVICE:
			/*
			 * fall through, P2P device only supports
			 * public action frames
			 */
		case NL80211_IFTYPE_NAN:
		default:
			err = -EOPNOTSUPP;
			break;
		}

		if (err)
			return err;
	}

	if (!cfg80211_allowed_address(wdev, mgmt->sa) &&
	    !cfg80211_allowed_random_address(wdev, mgmt))
		return -EINVAL;

	/* Transmit the management frame as requested by user space */
	return rdev_mgmt_tx(rdev, wdev, params, cookie);
}

bool cfg80211_rx_mgmt_ext(struct wireless_dev *wdev,
			  struct cfg80211_rx_info *info)
{
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct cfg80211_mgmt_registration *reg;
	const struct ieee80211_txrx_stypes *stypes;
	struct ieee80211_mgmt *mgmt = (void *)info->buf;
	const u8 *data;
	int data_len;
	bool result = false;
	__le16 ftype;
	u16 stype;

	if (info->len < sizeof(mgmt->frame_control))
		return false;

	if (wdev->iftype >= NUM_NL80211_IFTYPES)
		return false;

	stypes = &wiphy->mgmt_stypes[wdev->iftype];

	trace_cfg80211_rx_mgmt(wdev, info);

	if (info->len < ieee80211_hdrlen(mgmt->frame_control))
		return false;

	ftype = mgmt->frame_control &
		cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE);
	stype = (le16_to_cpu(mgmt->frame_control) & IEEE80211_FCTL_STYPE) >> 4;

	if (!(stypes->rx & BIT(stype))) {
		trace_cfg80211_return_bool(false);
		return false;
	}

	data = info->buf + ieee80211_hdrlen(mgmt->frame_control);
	data_len = info->len - ieee80211_hdrlen(mgmt->frame_control);

	spin_lock_bh(&rdev->mgmt_registrations_lock);

	list_for_each_entry(reg, &wdev->mgmt_registrations, list) {
		if (reg->frame_type != ftype)
			continue;

		if (reg->match_len > data_len)
			continue;

		if (memcmp(reg->match, data, reg->match_len))
			continue;

		/* found match! */

		/* Indicate the received Action frame to user space */
		if (nl80211_send_mgmt(rdev, wdev, reg->nlportid, info,
				      GFP_ATOMIC))
			continue;

		result = true;
		break;
	}

	spin_unlock_bh(&rdev->mgmt_registrations_lock);

	trace_cfg80211_return_bool(result);
	return result;
}
EXPORT_SYMBOL(cfg80211_rx_mgmt_ext);

/**
 * cfg80211_schedule_dfs_chan_update() - Schedule DFS channel state updates
 * @wiphy: Wiphy whose DFS channels should be refreshed
 *
 * Return: None.
 */
void cfg80211_schedule_dfs_chan_update(struct wiphy *wiphy)
{
	cfg80211_sched_dfs_chan_update(wiphy_to_rdev(wiphy));
}
EXPORT_SYMBOL(cfg80211_schedule_dfs_chan_update);

void cfg80211_sched_dfs_chan_update(struct cfg80211_registered_device *rdev)
{
	cancel_delayed_work(&rdev->dfs_update_channels_wk);
	queue_delayed_work(cfg80211_wq, &rdev->dfs_update_channels_wk, 0);
}

void cfg80211_dfs_channels_update_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct cfg80211_registered_device *rdev;
	struct cfg80211_chan_def chandef;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *c;
	struct wiphy *wiphy;
	bool check_again = false;
	unsigned long timeout, next_time = 0;
	unsigned long time_dfs_update;
	enum nl80211_radar_event radar_event;
	int bandid, i;

	rdev = container_of(delayed_work, struct cfg80211_registered_device,
			    dfs_update_channels_wk);
	wiphy = &rdev->wiphy;

	rtnl_lock();
	for (bandid = 0; bandid < NUM_NL80211_BANDS; bandid++) {
		sband = wiphy->bands[bandid];
		if (!sband)
			continue;

		for (i = 0; i < sband->n_channels; i++) {
			c = &sband->channels[i];

			if (!(c->flags & IEEE80211_CHAN_RADAR))
				continue;

			if (c->dfs_state != NL80211_DFS_UNAVAILABLE &&
			    c->dfs_state != NL80211_DFS_AVAILABLE)
				continue;

			if (c->dfs_state == NL80211_DFS_UNAVAILABLE) {
				time_dfs_update = IEEE80211_DFS_MIN_NOP_TIME_MS;
#ifdef CPTCFG_QCA_LAB_TEST_FEATURES
				time_dfs_update = cfg80211_get_nol_time(time_dfs_update);
#endif
				radar_event = NL80211_RADAR_NOP_FINISHED;
			} else {
				if (regulatory_pre_cac_allowed(wiphy) ||
				    cfg80211_any_wiphy_oper_chan(wiphy, c))
					continue;

				if (cfg80211_any_wiphy_non_oper_device_chan(wiphy, c))
					continue;

				time_dfs_update = REG_PRE_CAC_EXPIRY_GRACE_MS;
				radar_event = NL80211_RADAR_PRE_CAC_EXPIRED;
			}

			timeout = c->dfs_state_entered +
				  msecs_to_jiffies(time_dfs_update);

			if (time_after_eq(jiffies, timeout)) {
				c->dfs_state = NL80211_DFS_USABLE;
				c->dfs_state_entered = jiffies;

				cfg80211_chandef_create(&chandef, c,
							NL80211_CHAN_NO_HT);

				if (radar_event == NL80211_RADAR_NOP_FINISHED) {
					wiphy_dbg(wiphy,
						  "DFS NOL completed for freq=%u",
						  c->center_freq);
					wiphy_lock(wiphy);
					rdev_start_punctured_cac(rdev, &chandef);
					wiphy_unlock(wiphy);
				}

				nl80211_radar_notify(rdev, &chandef,
						     radar_event, NULL,
						     GFP_ATOMIC);

				regulatory_propagate_dfs_state(wiphy, &chandef,
							       c->dfs_state,
							       radar_event);
				continue;
			}

			if (!check_again)
				next_time = timeout - jiffies;
			else
				next_time = min(next_time, timeout - jiffies);
			check_again = true;
		}
	}
	rtnl_unlock();

	/* reschedule if there are other channels waiting to be cleared again */
	if (check_again)
		queue_delayed_work(cfg80211_wq, &rdev->dfs_update_channels_wk,
				   next_time);
}

static int
cfg80211_background_radar_get_radio_idx(struct cfg80211_registered_device *rdev,
					const struct cfg80211_chan_def *chandef)
{
	int radio_idx;

	if (!chandef || !chandef->chan)
		return -EINVAL;

	if (rdev->wiphy.n_radio <= 0)
		return 0;

	radio_idx = cfg80211_get_hw_idx_by_chan(&rdev->wiphy, chandef->chan);
	if (radio_idx < 0 || radio_idx >= rdev->wiphy.n_radio)
		return -EINVAL;

	return radio_idx;
}

static struct cfg80211_bg_radar *
cfg80211_background_radar_get(struct cfg80211_registered_device *rdev,
			      int radio_idx)
{
	if (!rdev->wiphy.radio_cfg)
		return NULL;

	if (radio_idx < 0 || radio_idx >= max_t(int, 1, rdev->wiphy.n_radio))
		return NULL;

	return &rdev->wiphy.radio_cfg[radio_idx].bg_radar;
}

static struct cfg80211_bg_radar *
cfg80211_background_radar_find_active(struct cfg80211_registered_device *rdev,
				      const struct cfg80211_chan_def *chandef)
{
	int i;

	if (!chandef || !rdev->wiphy.radio_cfg)
		return NULL;

	for (i = 0; i < max_t(int, 1, rdev->wiphy.n_radio); i++) {
		struct cfg80211_bg_radar *bgr =
			&rdev->wiphy.radio_cfg[i].bg_radar;

		if (!bgr->active)
			continue;

		if (cfg80211_chandef_identical(&bgr->chandef, chandef))
			return bgr;
	}

	return NULL;
}

static bool
cfg80211_background_radar_link_available(struct wireless_dev *wdev,
					 unsigned int link_id)
{
	if (wdev->repurposed_links & BIT(link_id))
		return false;

	if ((wdev->iftype == NL80211_IFTYPE_AP ||
	     wdev->iftype == NL80211_IFTYPE_P2P_GO) &&
	    !wdev->links[link_id].ap.beacon_interval)
		return false;

	return true;
}

static int
cfg80211_background_radar_find_surviving_link(struct cfg80211_registered_device *rdev,
					      struct wireless_dev *wdev,
					      int removed_link_id,
					      int radio_idx)
{
	struct cfg80211_chan_def *link_chandef;
	unsigned int cand_link_id;
	int candidate_radio_idx;

	if (!wdev->netdev || wdev->is_netdev_going_down)
		return -1;

	if (!wdev->valid_links) {
		if (removed_link_id == 0)
			return -1;

		if (!cfg80211_background_radar_link_available(wdev, 0))
			return -1;

		link_chandef = wdev_chandef(wdev, 0);
		if (!link_chandef || !link_chandef->chan)
			return -1;

		candidate_radio_idx =
			cfg80211_background_radar_get_radio_idx(rdev,
								link_chandef);
		if (candidate_radio_idx == radio_idx)
			return 0;

		return -1;
	}

	for_each_valid_link(wdev, cand_link_id) {
		if ((int)cand_link_id == removed_link_id)
			continue;

		if (!cfg80211_background_radar_link_available(wdev, cand_link_id))
			continue;

		link_chandef = wdev_chandef(wdev, cand_link_id);
		if (!link_chandef || !link_chandef->chan)
			continue;

		candidate_radio_idx =
			cfg80211_background_radar_get_radio_idx(rdev,
								link_chandef);
		if (candidate_radio_idx == radio_idx)
			return cand_link_id;
	}

	return -1;
}

static bool
cfg80211_background_radar_rehome(struct cfg80211_registered_device *rdev,
				 struct cfg80211_bg_radar *bgr,
				 struct wireless_dev *removed_wdev)
{
	struct wireless_dev *cand_wdev;
	int cand_link_id;

	list_for_each_entry(cand_wdev, &rdev->wiphy.wdev_list, list) {
		if (cand_wdev == removed_wdev)
			continue;

		if (cand_wdev->iftype != removed_wdev->iftype)
			continue;

		cand_link_id =
			cfg80211_background_radar_find_surviving_link(rdev,
								      cand_wdev,
								     -1,
								     bgr->radio_idx);
		if (cand_link_id < 0)
			continue;

		bgr->wdev = cand_wdev;
		bgr->link_id = cand_link_id;
		return true;
	}

	return false;
}

bool cfg80211_radar_event_device(struct wiphy *wiphy, struct cfg80211_chan_def *chandef)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct cfg80211_bg_radar *bgr;
	struct ieee80211_channel *c;
	u32 freq, end_freq, start_freq;
	bool nop_in_progress = false, non_oper_event = false;
	int i;
	unsigned long time_nop_entered = 0;

	start_freq = cfg80211_get_start_freq_device(chandef);
	end_freq = cfg80211_get_end_freq_device(chandef);

	for (freq = start_freq, i = 0; freq <= end_freq; i++, freq += MHZ_TO_KHZ(20)) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c)
			continue;

		if (chandef->width_device == NL80211_CHAN_WIDTH_320 &&
		    (freq >= MHZ_TO_KHZ(5740) && freq <= MHZ_TO_KHZ(5800)))
			continue;

		if (i >= BITS_PER_TYPE(chandef->radar_bitmap))
			break;

		if (c->dfs_state == NL80211_DFS_UNAVAILABLE) {
			chandef->radar_bitmap &= ~BIT(i);
			nop_in_progress = true;
			time_nop_entered = c->dfs_state_entered;
			break;
		}
	}

	for (freq = start_freq, i = 0; freq <= end_freq; i++, freq += MHZ_TO_KHZ(20)) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (chandef->width_device == NL80211_CHAN_WIDTH_320 &&
		    (freq >= MHZ_TO_KHZ(5740) && freq <= MHZ_TO_KHZ(5800)))
			continue;
		if (!c || !(c->flags & IEEE80211_CHAN_RADAR))
			continue;

		if (!chandef->radar_bitmap)
			break;

		if (i >= BITS_PER_TYPE(chandef->radar_bitmap))
			break;

		if (cfg80211_is_freq_device_non_oper(chandef, freq)) {
			if (c->dfs_state != NL80211_DFS_UNAVAILABLE &&
			    chandef->radar_bitmap & BIT(i)) {
				non_oper_event = true;
				c->dfs_state = NL80211_DFS_UNAVAILABLE;
				if (wiphy->sta_dfs_en)
					c->flags |= IEEE80211_CHAN_NOL_HISTORY;
				if (time_nop_entered)
					c->dfs_state_entered = time_nop_entered;
				else {
					c->dfs_state_entered = jiffies;
					time_nop_entered = c->dfs_state_entered;
				}
			}
		} else if (chandef->radar_bitmap & BIT(i)) {
			bgr = cfg80211_background_radar_find_active(rdev, chandef);
			if (bgr)
				queue_work(cfg80211_wq, &bgr->cac_abort_wk);
			return false;
		}
	}

	if (non_oper_event && !nop_in_progress) {
		bgr = cfg80211_background_radar_find_active(rdev, chandef);
		if (bgr)
			queue_work(cfg80211_wq, &bgr->cac_abort_wk);
		return false;
	}

	chandef->radar_bitmap = 0;
	return true;
}

void __cfg80211_radar_event(struct wiphy *wiphy,
			    struct cfg80211_chan_def *chandef,
			    bool offchan, gfp_t gfp)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	if (chandef->radar_bitmap &&
	    cfg80211_chandef_device_present(chandef) &&
	    cfg80211_radar_event_device(wiphy, chandef) == true)
		return;

	trace_cfg80211_radar_event(wiphy, chandef, offchan);

	/* only set the chandef supplied channel to unavailable, in
	 * case the radar is detected on only one of multiple channels
	 * spanned by the chandef.
	 */
	cfg80211_set_dfs_state(wiphy, chandef, NL80211_DFS_UNAVAILABLE);

	if (offchan) {
		struct cfg80211_bg_radar *bgr;

		bgr = cfg80211_background_radar_find_active(rdev, chandef);
		if (bgr)
			queue_work(cfg80211_wq, &bgr->cac_abort_wk);
	}

	cfg80211_sched_dfs_chan_update(rdev);

	nl80211_radar_notify(rdev, chandef, NL80211_RADAR_DETECTED, NULL, gfp);

	memcpy(&rdev->radar_chandef, chandef, sizeof(struct cfg80211_chan_def));
	queue_work(cfg80211_wq, &rdev->propagate_radar_detect_wk);
	chandef->radar_bitmap = 0;
}
EXPORT_SYMBOL(__cfg80211_radar_event);

void cfg80211_awgn_event(struct wiphy *wiphy, struct cfg80211_chan_def *chandef,
			 gfp_t gfp, u32 chan_bw_interference_bitmap)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	nl80211_awgn_notify(rdev, chandef, NULL, gfp, chan_bw_interference_bitmap);
}
EXPORT_SYMBOL(cfg80211_awgn_event);

static void cfg80211_clear_cac_started(struct cfg80211_registered_device *rdev,
				       const struct cfg80211_chan_def *chandef,
				       unsigned int link_id,
				       const char *netdev_name)
{
	struct wireless_dev *tmp_wdev;
	unsigned int tmp_link_id;

	list_for_each_entry(tmp_wdev, &rdev->wiphy.wdev_list, list) {
		for_each_valid_link(tmp_wdev, tmp_link_id) {
			struct cfg80211_chan_def *tmp_chandef =
				wdev_chandef(tmp_wdev, tmp_link_id);

			if (!tmp_chandef || !tmp_chandef->chan)
				continue;

			if (tmp_chandef->chan != chandef->chan)
				continue;

#ifdef CPTCFG_QCN_EXTN
			if (tmp_wdev->iftype == NL80211_IFTYPE_MESH_POINT &&
			    cfg80211_support_bootup_cac(&rdev->wiphy))
				continue;
#endif

			tmp_wdev->links[tmp_link_id].cac_started = false;
		}
	}
}

void cfg80211_cac_event(struct net_device *netdev,
			const struct cfg80211_chan_def *chandef,
			enum nl80211_radar_event event, gfp_t gfp,
			unsigned int link_id)
{
	struct wireless_dev *wdev = netdev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	unsigned long timeout;

	if (WARN_ON(wdev->valid_links &&
		    !(wdev->valid_links & BIT(link_id))))
		return;

	trace_cfg80211_cac_event(netdev, event, link_id);

	if (WARN_ON(!wdev->links[link_id].cac_started &&
		    event != NL80211_RADAR_CAC_STARTED))
		return;

	switch (event) {
	case NL80211_RADAR_CAC_FINISHED:
		timeout = wdev->links[link_id].cac_start_time +
			  msecs_to_jiffies(wdev->links[link_id].cac_time_ms);
		WARN_ON(!time_after_eq(jiffies, timeout));
		cfg80211_set_dfs_state(wiphy, chandef, NL80211_DFS_AVAILABLE);
		memcpy(&rdev->cac_done_chandef, chandef,
		       sizeof(struct cfg80211_chan_def));
		queue_work(cfg80211_wq, &rdev->propagate_cac_done_wk);
		cfg80211_sched_dfs_chan_update(rdev);
		fallthrough;
	case NL80211_RADAR_CAC_ABORTED:
		cfg80211_clear_cac_started(rdev, chandef, link_id,
					   netdev->name);
		if (wdev->iftype == NL80211_IFTYPE_STATION)
			wdev->links[link_id].cac_started = false;
		break;
	case NL80211_RADAR_CAC_STARTED:
		wdev->links[link_id].cac_started = true;
		break;
	default:
		WARN_ON(1);
		return;
	}

	nl80211_radar_notify(rdev, chandef, event, netdev, gfp);
}
EXPORT_SYMBOL(cfg80211_cac_event);

void cfg80211_punct_cac_finished(struct net_device *netdev,
				 const struct cfg80211_chan_def *chandef,
				 gfp_t gfp)
{
	struct wireless_dev *wdev = netdev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	trace_cfg80211_cac_event(netdev, NL80211_RADAR_CAC_FINISHED, 0);
	cfg80211_set_dfs_state(wiphy, chandef, NL80211_DFS_AVAILABLE);

	wiphy_dbg(wiphy,
		  "DFS punctured CAC finished, send CAC FINISHED to hostapd chandef chan=%u width=%u cf1=%u cf2=%u punctured=0x%x radar_bitmap=0x%x\n",
		  chandef->chan ? chandef->chan->center_freq : 0,
		  chandef->width, chandef->center_freq1, chandef->center_freq2,
		  chandef->punctured, chandef->radar_bitmap);

	nl80211_radar_notify(rdev, chandef, NL80211_RADAR_CAC_FINISHED, netdev, gfp);
}
EXPORT_SYMBOL(cfg80211_punct_cac_finished);

static bool cfg80211_link_beaconing_enabled(struct wireless_dev *wdev,
					    unsigned int link_id)
{
	switch (wdev->iftype) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		return wdev->links[link_id].ap.beacon_interval;
	case NL80211_IFTYPE_ADHOC:
		return wdev->u.ibss.ssid_len;
	case NL80211_IFTYPE_MESH_POINT:
		return wdev->u.mesh.id_len;
	default:
		return false;
	}
}

void cfg80211_set_cac_started(struct cfg80211_registered_device *rdev,
			      struct wireless_dev *wdev,
			      unsigned int link_id,
			      const struct cfg80211_chan_def *chandef)
{
	struct wireless_dev *tmp_wdev;
	unsigned int tmp_link_id;
	unsigned long cac_start_time;
	unsigned int cac_time_ms;

	lockdep_assert_wiphy(&rdev->wiphy);

	cac_start_time = wdev->links[link_id].cac_start_time;
	cac_time_ms = wdev->links[link_id].cac_time_ms;

	list_for_each_entry(tmp_wdev, &rdev->wiphy.wdev_list, list) {
		for_each_valid_link(tmp_wdev, tmp_link_id) {
			struct cfg80211_chan_def *tmp_chandef;

			if (tmp_wdev == wdev &&
			    tmp_link_id == link_id)
				continue;

			if (!cfg80211_link_beaconing_enabled(tmp_wdev, tmp_link_id))
				continue;

			tmp_chandef = wdev_chandef(tmp_wdev, tmp_link_id);
			if (!tmp_chandef || !tmp_chandef->chan)
				continue;

			if (tmp_chandef->chan != chandef->chan)
				continue;

			tmp_wdev->links[tmp_link_id].cac_started = true;
			tmp_wdev->links[tmp_link_id].cac_start_time =
				cac_start_time;
			tmp_wdev->links[tmp_link_id].cac_time_ms = cac_time_ms;
		}
	}
}

static void
__cfg80211_background_cac_event(struct cfg80211_registered_device *rdev,
				struct cfg80211_bg_radar *bgr,
				enum nl80211_radar_event event)
{
	struct wiphy *wiphy = &rdev->wiphy;
	struct net_device *netdev;

	lockdep_assert_wiphy(&rdev->wiphy);

	if (!bgr || !bgr->active)
		return;

	if (!cfg80211_chandef_valid(&bgr->chandef))
		return;

	switch (event) {
	case NL80211_RADAR_CAC_FINISHED:
		cfg80211_set_dfs_state(wiphy, &bgr->chandef,
				       NL80211_DFS_AVAILABLE);
		memcpy(&rdev->cac_done_chandef, &bgr->chandef,
		       sizeof(bgr->chandef));
		queue_work(cfg80211_wq, &rdev->propagate_cac_done_wk);
		cfg80211_sched_dfs_chan_update(rdev);
		break;
	case NL80211_RADAR_CAC_ABORTED:
		if (!cancel_delayed_work(&bgr->cac_done_wk)) {
			cfg80211_sched_dfs_chan_update(rdev);
			/* For non-ETSI CAC monitoring will keep happening
			 * even if the work is not running
			 */
			if (reg_get_dfs_region(&rdev->wiphy) == NL80211_DFS_ETSI)
				return;
		}
		cfg80211_sched_dfs_chan_update(rdev);
		break;
	case NL80211_RADAR_CAC_STARTED:
		break;
	default:
		return;
	}

	/* Explicit radio-index monitors are radio scoped, not netdev scoped. */
	if (bgr->radio_idx_explicit)
		netdev = NULL;
	else
		netdev = bgr->wdev ? bgr->wdev->netdev : NULL;
	nl80211_radar_notify(rdev, &bgr->chandef, event, netdev, GFP_KERNEL);

	/*
	 * Release the radio's background radar state on abort, and on
	 * CAC-finished only for ETSI: for non-ETSI regions background CAC
	 * monitoring keeps happening on the channel even after the initial
	 * CAC time elapses, so the state must stay active to keep owning
	 * the radio.
	 */
	if (event == NL80211_RADAR_CAC_ABORTED ||
	    (event == NL80211_RADAR_CAC_FINISHED &&
	     reg_get_dfs_region(&rdev->wiphy) == NL80211_DFS_ETSI)) {
		bgr->active = false;
		bgr->wdev = NULL;
		bgr->link_id = -1;
		bgr->radio_idx_explicit = false;
		memset(&bgr->chandef, 0, sizeof(bgr->chandef));
	}
}

static void
cfg80211_background_cac_event(struct cfg80211_bg_radar *bgr,
			      enum nl80211_radar_event event)
{
	struct cfg80211_registered_device *rdev;

	if (!bgr)
		return;

	rdev = wiphy_to_rdev(bgr->wiphy);
	guard(wiphy)(&rdev->wiphy);

	__cfg80211_background_cac_event(rdev, bgr, event);
}

void cfg80211_background_cac_done_wk(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct cfg80211_bg_radar *bgr;

	bgr = container_of(delayed_work,
			   struct cfg80211_bg_radar,
			   cac_done_wk);
	cfg80211_background_cac_event(bgr,
				      NL80211_RADAR_CAC_FINISHED);
}

void cfg80211_background_cac_abort_wk(struct work_struct *work)
{
	struct cfg80211_bg_radar *bgr;

	bgr = container_of(work, struct cfg80211_bg_radar,
			   cac_abort_wk);
	cfg80211_background_cac_event(bgr,
				      NL80211_RADAR_CAC_ABORTED);
}

void cfg80211_background_cac_abort_by_chandef(struct wiphy *wiphy,
					      const struct cfg80211_chan_def *chandef)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct cfg80211_bg_radar *bgr;
	int i;

	if (!rdev->wiphy.radio_cfg)
		return;

	if (chandef) {
		bgr = cfg80211_background_radar_find_active(rdev, chandef);
		if (bgr)
			queue_work(cfg80211_wq, &bgr->cac_abort_wk);
		return;
	}

	for (i = 0; i < max_t(int, 1, rdev->wiphy.n_radio); i++) {
		bgr = &rdev->wiphy.radio_cfg[i].bg_radar;
		if (bgr->active)
			queue_work(cfg80211_wq, &bgr->cac_abort_wk);
	}
}
EXPORT_SYMBOL(cfg80211_background_cac_abort_by_chandef);

void cfg80211_background_cac_abort(struct wiphy *wiphy)
{
	/* Abort all active per-radio background CAC monitors. On multi-radio
	 * wiphys this aborts every active slot. Callers that need to abort a
	 * specific radio should use cfg80211_background_cac_abort_by_chandef()
	 * with a non-NULL chandef.
	 */
	cfg80211_background_cac_abort_by_chandef(wiphy, NULL);
}
EXPORT_SYMBOL(cfg80211_background_cac_abort);

static int
__cfg80211_stop_background_radar_detection_radio(
		struct cfg80211_registered_device *rdev,
		struct cfg80211_bg_radar *bgr)
{
	int err;

	if (!bgr || !bgr->active)
		return -EINVAL;

	err = rdev_abort_radar_background(rdev, &bgr->chandef);

	/*
	 * Always fire the abort event and clear the active state regardless
	 * of whether the driver abort succeeded. Leaving bgr->active true on
	 * a driver error would permanently lock this radio slot as EBUSY.
	 */
	__cfg80211_background_cac_event(rdev, bgr, NL80211_RADAR_CAC_ABORTED);

	return err;
}

int cfg80211_stop_background_radar_detection_for_radio(
		struct cfg80211_registered_device *rdev,
		int radio_idx)
{
	struct cfg80211_bg_radar *bgr;

	lockdep_assert_wiphy(&rdev->wiphy);

	bgr = cfg80211_background_radar_get(rdev, radio_idx);
	if (!bgr)
		return -EINVAL;

	return __cfg80211_stop_background_radar_detection_radio(rdev, bgr);
}

int
cfg80211_start_background_radar_detection(struct cfg80211_registered_device *rdev,
					  struct wireless_dev *wdev,
					  struct cfg80211_chan_def *chandef,
					  int link_id, int radio_idx)
{
	struct cfg80211_bg_radar *bgr;
	struct cfg80211_chan_def *current_chandef;
	int current_radio_idx;
	int resolved_radio_idx;
	unsigned int cac_time_ms;
	int err;

	lockdep_assert_wiphy(&rdev->wiphy);

	if (!wiphy_ext_feature_isset(&rdev->wiphy,
				     NL80211_EXT_FEATURE_RADAR_BACKGROUND))
		return -EOPNOTSUPP;

	if (!rdev->wiphy.radio_cfg)
		return -EINVAL;

	current_chandef = wdev_chandef(wdev, link_id);
	if (!current_chandef)
		return -EINVAL;

	resolved_radio_idx = cfg80211_background_radar_get_radio_idx(rdev, chandef);
	if (resolved_radio_idx < 0)
		return resolved_radio_idx;

	if (radio_idx >= 0 && radio_idx != resolved_radio_idx)
		return -EINVAL;

	current_radio_idx =
		cfg80211_background_radar_get_radio_idx(rdev, current_chandef);
	if (current_radio_idx < 0 || current_radio_idx != resolved_radio_idx)
		return -EINVAL;

	bgr = cfg80211_background_radar_get(rdev, resolved_radio_idx);
	if (!bgr)
		return -EINVAL;

	/*
	 * CAC is serialized per radio offchannel chain.
	 * If a background CAC is already active on the same chandef, the
	 * hardware is already monitoring. Treat it as a no-op and return
	 * success. Note: NL80211_RADAR_CAC_STARTED is not re-sent because
	 * no new CAC was initiated.
	 * If active on a different chandef (e.g. BW expanded after Progressive
	 * DFS NOL expiry), abort the old CAC and start the new one so the
	 * wider device bandwidth channels can be checked for availability.
	 * The abort unconditionally clears bgr->active regardless of driver
	 * error, so it is safe to proceed to rdev_set_radar_background().
	 */
	if (bgr->active) {
		if (cfg80211_chandef_identical(&bgr->chandef, chandef))
			return 0;
		/* Ignore abort error: state is cleared regardless (see comment
		 * in __cfg80211_stop_background_radar_detection_radio). */
		if (__cfg80211_stop_background_radar_detection_radio(rdev, bgr))
			printk("DFS: background radar abort failed, proceeding with restart\n");
	}

	err = rdev_set_radar_background(rdev, chandef);
	if (err)
		return err;

	if (current_chandef &&
	    cfg80211_chandef_identical(current_chandef, chandef) &&
	    cfg80211_chandef_device_present(chandef))
		cac_time_ms = cfg80211_chandef_dfs_cac_time(&rdev->wiphy, chandef,
							    true, true);
	else
		cac_time_ms = cfg80211_chandef_dfs_cac_time(&rdev->wiphy, chandef,
                                                           true, false);

	if (!cac_time_ms)
		cac_time_ms = IEEE80211_DFS_MIN_CAC_TIME_MS;

	bgr->wdev = wdev;
	bgr->link_id = link_id;
	bgr->chandef = *chandef;
	bgr->active = true;
	bgr->radio_idx_explicit = radio_idx >= 0;

	__cfg80211_background_cac_event(rdev, bgr,
					NL80211_RADAR_CAC_STARTED);
	queue_delayed_work(cfg80211_wq, &bgr->cac_done_wk,
			   msecs_to_jiffies(cac_time_ms));

	return 0;
}

void cfg80211_stop_background_radar_detection(struct wireless_dev *wdev,
					      int link_id)
{
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	int i;

	lockdep_assert_wiphy(wiphy);

	if (!rdev->wiphy.radio_cfg)
		return;

	for (i = 0; i < max_t(int, 1, rdev->wiphy.n_radio); i++) {
		struct cfg80211_bg_radar *bgr =
			&rdev->wiphy.radio_cfg[i].bg_radar;

		if (!bgr->active || bgr->wdev != wdev)
			continue;

		/* Keep legacy owner semantics when start omitted explicit radio index. */
		if (link_id >= 0 && bgr->radio_idx_explicit &&
		    bgr->link_id != link_id)
			continue;

		/* Only explicit-radio mode can move ownership across links/wdevs.
		 * When an MLO link goes down, rehome the background CAC to another
		 * surviving link on the same radio rather than aborting it — the
		 * hardware chain is still running and tearing it down unnecessarily
		 * wastes the CAC time already accumulated on that radio.
		 */
		if (bgr->radio_idx_explicit &&
		    cfg80211_background_radar_rehome(rdev, bgr,
						     wdev)) {
			continue;
		}

		__cfg80211_stop_background_radar_detection_radio(rdev, bgr);
	}
}

int cfg80211_assoc_ml_reconf(struct cfg80211_registered_device *rdev,
			     struct net_device *dev,
			     struct cfg80211_ml_reconf_req *req)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	int err;

	lockdep_assert_wiphy(wdev->wiphy);

	err = rdev_assoc_ml_reconf(rdev, dev, req);
	if (!err) {
		int link_id;

		for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS;
		     link_id++) {
			if (!req->u.add_links[link_id].bss)
				continue;

			cfg80211_ref_bss(&rdev->wiphy, req->u.add_links[link_id].bss);
			cfg80211_hold_bss(bss_from_pub(req->u.add_links[link_id].bss));
		}
	}

	return err;
}

void cfg80211_mlo_reconf_add_done(struct net_device *dev,
				  struct cfg80211_mlo_reconf_done_data *data)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	int link_id;

	lockdep_assert_wiphy(wiphy);

	trace_cfg80211_mlo_reconf_add_done(dev, data->added_links,
					   data->buf, data->len);

	if (WARN_ON(!wdev->valid_links))
		return;

	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION &&
		    wdev->iftype != NL80211_IFTYPE_P2P_CLIENT))
		return;

	/* validate that a BSS is given for each added link */
	for (link_id = 0; link_id < ARRAY_SIZE(data->links); link_id++) {
		struct cfg80211_bss *bss = data->links[link_id].bss;

		if (!(data->added_links & BIT(link_id)))
			continue;

		if (WARN_ON(!bss))
			return;
	}

	for (link_id = 0; link_id < ARRAY_SIZE(data->links); link_id++) {
		struct cfg80211_bss *bss = data->links[link_id].bss;

		if (!bss)
			continue;

		if (data->added_links & BIT(link_id)) {
			wdev->links[link_id].client.current_bss =
				bss_from_pub(bss);

			memcpy(wdev->links[link_id].addr,
			       data->links[link_id].addr,
			       ETH_ALEN);
		} else {
			cfg80211_unhold_bss(bss_from_pub(bss));
			cfg80211_put_bss(wiphy, bss);
		}
	}

	wdev->valid_links |= data->added_links;
	nl80211_mlo_reconf_add_done(dev, data);
}
EXPORT_SYMBOL(cfg80211_mlo_reconf_add_done);

int cfg80211_rx_send_mscs_tuple(struct net_device *dev, const u8 *addr,
				struct cfg80211_qm_tclas4_params flow_params,
				u8 tid)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	return nl80211_send_mscs_flow_info(rdev, addr, flow_params, tid);
}
EXPORT_SYMBOL(cfg80211_rx_send_mscs_tuple);

/* SMD BSS Transition - internal helpers */
#define CFG80211_SMD_MAX_PREP_TARGETS 4

/* SMD preparation state management helper functions - forward declarations */
static int cfg80211_smd_prep_state_alloc(struct wireless_dev *wdev, u8 max_prep_targets);
static int cfg80211_smd_find_target_slot(struct wireless_dev *wdev, const u8 *addr);
static void cfg80211_smd_prep_cleanup_target(struct wireless_dev *wdev, const u8 *addr);
static int cfg80211_smd_prep_add_target(struct wireless_dev *wdev,
					struct cfg80211_uhr_reconfig_done *done);
static void cfg80211_uhr_reconfig_handle_prep(struct wireless_dev *wdev,
					      struct cfg80211_uhr_reconfig_done *done);
static void cfg80211_uhr_reconfig_handle_exec(struct wireless_dev *wdev,
					      struct cfg80211_uhr_reconfig_done *done);

/* SMD preparation state management helper functions */
static int cfg80211_smd_prep_state_alloc(struct wireless_dev *wdev, u8 max_prep_targets)
{
	struct cfg80211_smd_prep_state *prep_state;
	size_t targets_size;

	if (wdev->smd_prep)
		return 0;
	if (max_prep_targets == 0 || max_prep_targets > 16)
		return -EINVAL;
	targets_size = flex_array_size(prep_state, targets, max_prep_targets);
	prep_state = kzalloc(sizeof(*prep_state) + targets_size, GFP_KERNEL);
	if (!prep_state)
		return -ENOMEM;
	prep_state->max_prep_targets = max_prep_targets;
	wdev->smd_prep = prep_state;
	return 0;
}

static void cfg80211_smd_prep_cleanup_target(struct wireless_dev *wdev, const u8 *addr)
{
	struct cfg80211_smd_prep_target *target;
	struct wiphy *wiphy = wdev->wiphy;
	int slot = cfg80211_smd_find_target_slot(wdev, addr);
	int link_id;

	if (slot < 0)
		return;

	target = &wdev->smd_prep->targets[slot];

	/* Release BSS references */
	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		if (target->prepared_bss[link_id]) {
			cfg80211_unhold_bss(target->prepared_bss[link_id]);
			cfg80211_put_bss(wiphy, &target->prepared_bss[link_id]->pub);
			target->prepared_bss[link_id] = NULL;
		}
	}

	/* Update global state */
	wdev->smd_prep->all_prepared_links &= ~target->prepared_links;
	if (wdev->smd_prep->num_prep_targets > 0)
		wdev->smd_prep->num_prep_targets--;

	if (wdev->smd_prep->num_prep_targets == 0) {
		wdev->smd_prep->in_smd_preparation = false;
		wdev->smd_prep->all_prepared_links = 0;
	}
	/* Clear target slot */
	memset(target, 0, sizeof(*target));
}

void cfg80211_smd_prep_state_free(struct wireless_dev *wdev)
{
	int i;

	if (!wdev->smd_prep)
		return;

	/* Cleanup all targets first */
	for (i = 0; i < wdev->smd_prep->max_prep_targets; i++) {
		if (wdev->smd_prep->targets[i].valid)
			cfg80211_smd_prep_cleanup_target(
				wdev,
				wdev->smd_prep->targets[i].target_mld_addr);
	}

	kfree(wdev->smd_prep);
	wdev->smd_prep = NULL;
}

static int cfg80211_smd_find_target_slot(struct wireless_dev *wdev, const u8 *addr)
{
	struct cfg80211_smd_prep_state *state = wdev->smd_prep;
	int i;

	if (!state)
		return -ENOENT;

	/* First look for existing target with this address */
	for (i = 0; i < state->max_prep_targets; i++) {
		if (state->targets[i].valid &&
		    ether_addr_equal(state->targets[i].target_mld_addr, addr))
			return i;
	}

	/* Then look for empty slot */
	for (i = 0; i < state->max_prep_targets; i++) {
		if (!state->targets[i].valid)
			return i;
	}

	return -ENOSPC;
}

static int cfg80211_smd_prep_add_target(struct wireless_dev *wdev,
					struct cfg80211_uhr_reconfig_done *done)
{
	struct cfg80211_smd_prep_target *target;
	int slot, link_id;

	slot = cfg80211_smd_find_target_slot(wdev, done->target_mld_addr);
	if (slot < 0)
		return slot;

	target = &wdev->smd_prep->targets[slot];
	ether_addr_copy(target->target_mld_addr, done->target_mld_addr);
	target->prepared_links = done->prepared_links;
	target->target_aid = done->aid;
	target->prep_timestamp = jiffies;
	/* valid=true required by cfg80211_smd_hold_prepared_bss; safe under wiphy lock */
	target->valid = true;

	wdev->smd_prep->num_prep_targets++;
	wdev->smd_prep->in_smd_preparation = true;
	wdev->smd_prep->all_prepared_links |= done->prepared_links;

	wiphy_dbg(wdev->wiphy, "smd: prep_add_target %pM prepared=0x%x transitioning=0x%x\n",
		  done->target_mld_addr, done->prepared_links, done->transitioning_links);

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		struct cfg80211_bss *bss;
		int ret;

		if (!(done->prepared_links & BIT(link_id)))
			continue;

		bss = done->links[link_id].bss;
		if (!bss)
			continue;

		ret = cfg80211_smd_hold_prepared_bss(wdev, done->target_mld_addr,
						     link_id, bss);
		if (ret) {
			cfg80211_smd_prep_cleanup_target(wdev,
							 done->target_mld_addr);
			return ret;
		}
	}

	return 0;
}

void cfg80211_smd_prep_done(struct net_device *dev,
			    struct cfg80211_uhr_reconfig_done *done)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	lockdep_assert_wiphy(wdev->wiphy);

	cfg80211_uhr_reconfig_handle_prep(wdev, done);
	nl80211_smd_prep_done(dev, done);
}
EXPORT_SYMBOL(cfg80211_smd_prep_done);

void cfg80211_smd_execution_done(struct net_device *dev,
				 struct cfg80211_uhr_reconfig_done *done)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	lockdep_assert_wiphy(wdev->wiphy);

	cfg80211_uhr_reconfig_handle_exec(wdev, done);
	cfg80211_smd_prep_state_free(wdev);
	nl80211_smd_exec_done(dev, done);
}
EXPORT_SYMBOL(cfg80211_smd_execution_done);

static void cfg80211_uhr_reconfig_handle_prep(struct wireless_dev *wdev,
					      struct cfg80211_uhr_reconfig_done *done)
{
	struct wiphy *wiphy = wdev->wiphy;
	int ret;

	trace_cfg80211_smd_prep_done(wdev->netdev, done->target_mld_addr,
				     done->status_code);

	wiphy_dbg(wiphy, "smd: handle_prep %pM status=%d prepared=0x%x\n",
		  done->target_mld_addr, done->status_code, done->prepared_links);

	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION &&
		    wdev->iftype != NL80211_IFTYPE_P2P_CLIENT))
		return;

	if (done->status_code == WLAN_STATUS_SUCCESS) {
		/* Allocate SMD preparation state on first successful preparation */
		if (!wdev->smd_prep) {
			/* Use a reasonable default if max_prep_targets not provided */
			/* No max_prep_targets field; use default */
			u8 max_prep_targets = CFG80211_SMD_MAX_PREP_TARGETS;

			ret = cfg80211_smd_prep_state_alloc(wdev, max_prep_targets);
			if (ret) {
				wiphy_err(wiphy,
					  "SMD prep state allocation failed: %d\n",
					  ret);
				done->status_code = WLAN_STATUS_UNSPECIFIED_FAILURE;
				return;
			}
		}

		/* Add target to preparation state - DON'T modify valid_links */
		ret = cfg80211_smd_prep_add_target(wdev, done);
		if (ret) {
			wiphy_err(wiphy, "Failed to add SMD target: %d\n", ret);
			done->status_code = WLAN_STATUS_UNSPECIFIED_FAILURE;
			/* First target allocation just happened; free it */
			if (wdev->smd_prep->num_prep_targets == 0)
				cfg80211_smd_prep_state_free(wdev);
		}
	} else {
		/* Clear any partial preparation state for this target */
		if (wdev->smd_prep)
			cfg80211_smd_prep_cleanup_target(wdev, done->target_mld_addr);
	}
}

static void cfg80211_uhr_reconfig_handle_exec(struct wireless_dev *wdev,
					      struct cfg80211_uhr_reconfig_done *done)
{
	struct cfg80211_smd_prep_target *target = NULL;
	unsigned long tmp;
	int i, link_id;

	trace_cfg80211_smd_prep_done(wdev->netdev, done->target_mld_addr,
				     done->status_code);

	wiphy_dbg(wdev->wiphy, "smd: handle_exec %pM status=%d\n",
		  done->target_mld_addr, done->status_code);

	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION &&
		    wdev->iftype != NL80211_IFTYPE_P2P_CLIENT))
		return;

	if (!wdev->smd_prep || !wdev->smd_prep->in_smd_preparation) {
		wiphy_dbg(wdev->wiphy, "smd: handle_exec %pM no prep state, ignoring\n",
			  done->target_mld_addr);
		return;
	}

	/* Find the target by MLD Address */
	for (i = 0; i < wdev->smd_prep->max_prep_targets; i++) {
		if (wdev->smd_prep->targets[i].valid &&
		    ether_addr_equal(wdev->smd_prep->targets[i].target_mld_addr,
				     done->target_mld_addr)) {
			target = &wdev->smd_prep->targets[i];
			break;
		}
	}

	if (!target)
		return;

	target->transitioning_links = done->transitioning_links;
	tmp = done->transitioning_links;
	for_each_set_bit(link_id, &tmp, IEEE80211_MLD_MAX_NUM_LINKS) {
		if (!target->prepared_bss[link_id])
			continue;
		cfg80211_smd_transfer_bss(wdev, done->target_mld_addr, link_id);
		if (done->links[link_id].addr &&
		    is_valid_ether_addr(done->links[link_id].addr))
			memcpy(wdev->links[link_id].addr,
			       done->links[link_id].addr, ETH_ALEN);
	}

	/* Do not free prep state - COMPLETE or ABORT will do that */
}

void cfg80211_uhr_reconfig_resp_done(struct net_device *dev,
				     struct cfg80211_uhr_reconfig_done *done)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	lockdep_assert_wiphy(wdev->wiphy);

	if (WARN_ON(!wdev->valid_links))
		return;

	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION &&
		    wdev->iftype != NL80211_IFTYPE_P2P_CLIENT))
		return;

	if (done->prepared_links)
		cfg80211_uhr_reconfig_handle_prep(wdev, done);
	else if (done->transitioning_links)
		cfg80211_uhr_reconfig_handle_exec(wdev, done);

	nl80211_uhr_reconf_done(dev, done);
}
EXPORT_SYMBOL(cfg80211_uhr_reconfig_resp_done);

void cfg80211_notify_smd_bss_transition(struct net_device *dev,
					const u8 *target_mld_addr,
					enum nl80211_smd_transition_type type,
					u16 status_code,
					const struct cfg80211_uhr_reconfig_done *done)
{
	struct cfg80211_smd_prep_target *target = NULL;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	int i;

	lockdep_assert_wiphy(wiphy);

	wiphy_dbg(wiphy, "smd: notify_transition %pM type=%u status=%u\n",
		  target_mld_addr, type, status_code);

	if (WARN_ON(!wdev->valid_links))
		return;
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION &&
		    wdev->iftype != NL80211_IFTYPE_P2P_CLIENT))
		return;
	if (!wdev->smd_prep)
		goto notify;
	for (i = 0; i < wdev->smd_prep->max_prep_targets; i++) {
		if (wdev->smd_prep->targets[i].valid &&
		    ether_addr_equal(wdev->smd_prep->targets[i].target_mld_addr,
				     target_mld_addr)) {
			target = &wdev->smd_prep->targets[i];
			break;
		}
	}
	if (!target)
		goto notify;
	if (type == NL80211_SMD_TRANSITION_COMPLETE) {
		u16 primary_links;
		u16 removed_links;
		u16 old_valid;
		u16 all_tap_links;   /* prepared | transitioning = full TAP link set */
		unsigned long tmp;
		int link_id;

		/* Primary link = prepared_links that were NOT transitioned
		 *
		 * This still have BSS held from PREP.
		 */
		primary_links = target->prepared_links &
				~target->transitioning_links;

		/*
		 * Reconstruct the full TAP link set. By the time we reach here:
		 *  - cfg80211_uhr_reconfig_handle_exec() has already called
		 *    cfg80211_smd_transfer_bss() for each transitioning link, which
		 *    does target->prepared_links &= ~BIT(link_id).
		 *  - target->prepared_links therefore contains only the primary
		 *    (non-transitioning) links.
		 *  - target->transitioning_links was saved by the exec handler.
		 * Reconstruct the full set by OR-ing both.
		 */
		all_tap_links = target->prepared_links | target->transitioning_links;

		tmp = primary_links;
		for_each_set_bit(link_id, &tmp, IEEE80211_MLD_MAX_NUM_LINKS) {
			if (!target->prepared_bss[link_id])
				continue;
			wiphy_dbg(wiphy, "smd: transfer_bss link_id=%d target=%pM\n",
				  link_id, target_mld_addr);
			cfg80211_smd_transfer_bss(wdev, target_mld_addr, link_id);
		}

		/*
		 * Unconditionally restore wdev->valid_links to the TAP link set,
		 * mirroring how __cfg80211_roamed() assigns
		 *   wdev->valid_links = info->valid_links
		 * after updating the connection state.
		 *
		 * NL80211_CMD_DEL_STATION for the old SAP typically fires before
		 * NL80211_CMD_SMD_TRANSITION_DONE is delivered to wpa_supplicant.
		 * wpa_supplicant's DEL_STATION handler calls
		 * __cfg80211_disconnected() which sets wdev->valid_links = 0.
		 * Without restoring it here, per-link group key installs
		 * (GTK/IGTK/BIGTK) after the transition fail with
		 * "link ID not allowed for non-MLO group key" (nl80211.c:5596).
		 *
		 * Use all_tap_links (saved before the loop) since
		 * cfg80211_smd_transfer_bss clears bits from
		 * target->prepared_links as it transfers each link.
		 */
		old_valid = wdev->valid_links;
		wdev->valid_links = all_tap_links;

		/* Sync per-link STA addresses for all TAP links. */
		if (done) {
			tmp = all_tap_links;
			for_each_set_bit(link_id, &tmp, IEEE80211_MLD_MAX_NUM_LINKS) {
				if (done->links[link_id].addr &&
				    is_valid_ether_addr(done->links[link_id].addr))
					memcpy(wdev->links[link_id].addr,
					       done->links[link_id].addr,
					       ETH_ALEN);
			}
		}

		wiphy_dbg(wiphy, "smd: transition_complete primary=0x%x tap=0x%x valid=0x%x\n",
			  primary_links, all_tap_links, wdev->valid_links);
		/* Clean up link addresses for SAP-only links absent from TAP */
		removed_links = old_valid & ~all_tap_links;
		if (removed_links) {
			tmp = removed_links;
			for_each_set_bit(link_id, &tmp, IEEE80211_MLD_MAX_NUM_LINKS)
				eth_zero_addr(wdev->links[link_id].addr);
		}
	}

	/* ABORT: clean releases held BSS without wdev update */
	cfg80211_smd_prep_cleanup_target(wdev, target_mld_addr);

	if (wdev->smd_prep->num_prep_targets == 0)
		cfg80211_smd_prep_state_free(wdev);

notify:
	nl80211_notify_smd_bss_transition(dev, target_mld_addr,
					  type, status_code);
}
EXPORT_SYMBOL(cfg80211_notify_smd_bss_transition);

/* Public API functions for mac80211 SMD BSS management */

/**
 * cfg80211_smd_hold_prepared_bss - Ref and hold BSS for an SMD prepared target
 * @wdev: wireless device
 * @target_mld_addr: Target MLD address
 * @link_id: Link ID
 * @bss: BSS to hold
 *
 * Called by mac80211 after PREP RESPONSE to hold prepared BSS refs in cfg80211.
 * Uses existing cfg80211_hold_bss() function.
 */
int cfg80211_smd_hold_prepared_bss(struct wireless_dev *wdev,
				   const u8 *target_mld_addr,
				   unsigned int link_id,
				   struct cfg80211_bss *bss)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	struct cfg80211_smd_prep_target *target;
	int slot;

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	if (!wdev->smd_prep)
		return -ENOENT;

	slot = cfg80211_smd_find_target_slot(wdev, target_mld_addr);
	if (slot < 0)
		return slot;

	target = &wdev->smd_prep->targets[slot];
	if (!target->valid)
		return -ENOENT;

	cfg80211_ref_bss(&rdev->wiphy, bss);
	cfg80211_hold_bss(bss_from_pub(bss));

	target->prepared_bss[link_id] = bss_from_pub(bss);
	target->prepared_links |= BIT(link_id);

	return 0;
}
EXPORT_SYMBOL(cfg80211_smd_hold_prepared_bss);

/**
 * cfg80211_smd_transfer_bss - Transfer BSS ownership during EXEC RESPONSE
 * @wdev: wireless device
 * @target_mld_addr: Target MLD address
 * @link_id: Link ID
 *
 * Transfers BSS from prepared state to active current_bss.
 * Uses existing cfg80211_unhold_bss() function.
 */
int cfg80211_smd_transfer_bss(struct wireless_dev *wdev,
			      const u8 *target_mld_addr,
			      unsigned int link_id)
{
	struct cfg80211_smd_prep_target *target;
	struct cfg80211_internal_bss *old_bss, *new_bss;
	struct wiphy *wiphy = wdev->wiphy;
	int slot;

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	if (!wdev->smd_prep)
		return -ENOENT;

	slot = cfg80211_smd_find_target_slot(wdev, target_mld_addr);
	if (slot < 0)
		return slot;

	target = &wdev->smd_prep->targets[slot];
	if (!target->valid)
		return -ENOENT;

	new_bss = target->prepared_bss[link_id];
	if (!new_bss)
		return -EINVAL;

	/* Get old BSS */
	old_bss = wdev->links[link_id].client.current_bss;

	/* Unhold and put old BSS (using existing functions) */
	if (old_bss) {
		cfg80211_unhold_bss(old_bss);
		cfg80211_put_bss(wiphy, &old_bss->pub);
	}

	/* Transfer new BSS (ownership transfer, BSS remains held) */
	wdev->links[link_id].client.current_bss = new_bss;
	target->prepared_bss[link_id] = NULL;
	target->prepared_links &= ~BIT(link_id);
	ether_addr_copy(wdev->u.client.connected_addr, target_mld_addr);
	wiphy_dbg(wdev->wiphy, "smd: transfer_bss link_id=%u connected_addr=%pM\n",
		  link_id, wdev->u.client.connected_addr);
	return 0;
}
EXPORT_SYMBOL(cfg80211_smd_transfer_bss);

void cfg80211_smd_link_established(struct wireless_dev *wdev,
				   unsigned int link_id)
{
	lockdep_assert_wiphy(wdev->wiphy);

	/* Only applicable for MLO connections */
	if (!wdev->valid_links)
		return;

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		WARN_ONCE(1, "%s: invalid link_id %u\n", __func__, link_id);
		return;
	}

	wdev->valid_links |= BIT(link_id);
}
EXPORT_SYMBOL(cfg80211_smd_link_established);

/**
 * cfg80211_smd_cleanup_target - Cleanup SMD target on ABORT/TIMEOUT
 * @wdev: wireless device
 * @target_mld_addr: Target MLD address
 *
 * Public wrapper for the existing static cleanup function.
 * Releases all prepared BSS references using existing functions.
 */
void cfg80211_smd_cleanup_target(struct wireless_dev *wdev,
				 const u8 *target_mld_addr)
{
	/* Use existing static function */
	cfg80211_smd_prep_cleanup_target(wdev, target_mld_addr);
}
EXPORT_SYMBOL(cfg80211_smd_cleanup_target);

void cfg80211_get_smd_ctx_done(struct wireless_dev *wdev,
			       const u8 *sta_addr,
			       struct cfg80211_smd_transition_info *st_info)
{
	struct cfg80211_smd_get_ctx_pending *pending = NULL, *iter;

	spin_lock_bh(&wdev->smd_get_ctx_lock);
	list_for_each_entry(iter, &wdev->smd_get_ctx_pending_list, list) {
		if (ether_addr_equal(iter->sta_addr, sta_addr)) {
			pending = iter;
			list_del(&pending->list);
			break;
		}
	}
	spin_unlock_bh(&wdev->smd_get_ctx_lock);

	if (!pending) {
		wiphy_err(wdev->wiphy, "No pending SMD ctx req for %pM found in %s\n",
			  sta_addr, __func__);
		return;
	}

	nl80211_notify_get_smd_ctx_done(wdev, sta_addr,
					st_info->ctx ? st_info : NULL);

	kfree(pending);
}
EXPORT_SYMBOL(cfg80211_get_smd_ctx_done);
