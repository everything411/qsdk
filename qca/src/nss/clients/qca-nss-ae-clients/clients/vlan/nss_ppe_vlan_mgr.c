/*
 **************************************************************************
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * nss_ppe_vlan_mgr.c
 *	NSS PPE vlan Interface manager
 */
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/module.h>
#include <linux/version.h>
#include <net/bonding.h>
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
#include <linux/dsa/8021q.h>
#include <net/dsa.h>
#endif
#include <ppe_drv_public.h>
#include <ppe_vp_public.h>
#include <ppe_vp_tx.h>
#include <nss_ppe_vlan_mgr.h>
#include <ref/ref_vsi.h>
#include "nss_ppe_vlan_mgr_priv.h"

#define TPID_SIZE	4	/* TPID Array size */

static bool vlan_as_vp_invert = false;
module_param(vlan_as_vp_invert, bool, S_IRUGO);
MODULE_PARM_DESC(vlan_as_vp_invert, "When set, it indicates that the vlan_as_vp_interface parameter is the list of interfaces over which the VLAN as VP interface is not required");

static bool vlan_fdb_learn_dis = false;
module_param(vlan_fdb_learn_dis, bool, S_IRUGO);
MODULE_PARM_DESC(vlan_fdb_learn_dis, "When set, the FDB learning on related VSI will be disbaled");

static char vlan_as_vp_interface[NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX * IFNAMSIZ];
module_param_string(vlan_as_vp_interface, vlan_as_vp_interface, sizeof(vlan_as_vp_interface), 0644);
MODULE_PARM_DESC(vlan_as_vp_interface, "Interface list for the VLAN as VP feature. The meaning of this list is controlled by the vlan_as_vp_invert parameter");

static bool vp_fdb_learn_enabled = false;
module_param(vp_fdb_learn_enabled, bool, 0644);
MODULE_PARM_DESC(vp_fdb_learn_enabled, "VLAN-as-VP fdb learning is enabled");

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
#ifdef NSS_VLAN_DSA_VP_FDB_LEARN_EN
static bool dsa_vp_fdb_learn_enabled = true;
#else
static bool dsa_vp_fdb_learn_enabled = false;
#endif
module_param(dsa_vp_fdb_learn_enabled, bool, 0644);
MODULE_PARM_DESC(dsa_vp_fdb_learn_enabled, "VLAN-based DSA VP fdb learning is enabled");
#endif

static bool vlan_as_veip_enabled = true;
module_param(vlan_as_veip_enabled, bool, 0644);
MODULE_PARM_DESC(vlan_as_veip_enabled, "VLAN-as-VEIP feature is enabled");

/*
 * vlan_as_vp_dev_name contains the netdevice device names over which
 * the VLAN as VP interface feature is required.
 * This device list is fetched from the 'vlan_as_vp_interface' module param
 * given by the user during the initialization of the module
 */
static char vlan_as_vp_dev_name[NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX][IFNAMSIZ];
static struct nss_ppe_vlan_mgr_context vlan_mgr_ctx;

static bool nss_ppe_vlan_mgr_instance_deref(struct nss_vlan_pvt *v);

#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
static bool nss_ppe_vlan_mgr_veip_tx_to_pon_vp(struct net_device *dev, struct sk_buff *skb);

/*
 * Hardware offload transmit ops registered against a VLAN-as-VEIP netdevice.
 * This redirects accelerated upstream packets to the VEIP PON VP instead of
 * the normal VLAN netdevice transmit path. Receive offload is not used.
 */
static struct netdev_hw_offload_ops nss_ppe_vlan_mgr_veip_netdev_ops = {
	.xmit = nss_ppe_vlan_mgr_veip_tx_to_pon_vp,
	.recv = NULL,
};
#endif

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
/*
 * nss_ppe_vlan_mgr_add_vlan_as_vp()
 *	Add the device name to vlan_as_vp_dev_name[]
 */
static int nss_ppe_vlan_mgr_add_vlan_as_vp(char *dev_name)
{
	int len, i;

	if (vlan_as_vp_invert) {
		nss_ppe_vlan_mgr_warn("VLAN as VP invert enabled. Don't add: %s\n", dev_name);
		return -1;
	}

	for (i = 0; i < NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX; i++) {
		if (vlan_as_vp_dev_name[i][0] == '\0')
			break;
	}

	if (i == NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX) {
		nss_ppe_vlan_mgr_warn("VLAN as VP array is FULL. Can't add: %s\n", dev_name);
		return -1;
	}

	len = strlen(dev_name);
	if (len > IFNAMSIZ) {
		nss_ppe_vlan_mgr_info("Dev name %s too large to be added in VLAN as VP array\n", dev_name);
		return -1;
	}

	strscpy(vlan_as_vp_dev_name[i], dev_name, len + 1);
	nss_ppe_vlan_mgr_info("Added %s to VLAN as VP array at index:%d\n", dev_name, i);
	return 0;
}

/*
 * nss_ppe_vlan_mgr_del_vlan_as_vp()
 *	Remove the device name from vlan_as_vp_dev_name[], if present
 */
static void nss_ppe_vlan_mgr_del_vlan_as_vp(char *dev_name)
{
	int i, free_count = 0;

	for (i = 0; i < NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX; i++) {
		if (vlan_as_vp_dev_name[i][0] == '\0') {
			free_count++;
			continue;
		}

		if (free_count == NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX) {
			nss_ppe_vlan_mgr_info("Unable to remove %s from VLAN as VP array. List is empty.\n", dev_name);
			return;
		}

		if (!strncmp(dev_name, vlan_as_vp_dev_name[i], IFNAMSIZ)) {
			vlan_as_vp_dev_name[i][0] = '\0';
			nss_ppe_vlan_mgr_info("Deleted %s from VLAN as VP array from index:%d\n", dev_name, i);
			return;
		}
	}

	nss_ppe_vlan_mgr_info("Unable to remove %s from VLAN as VP array. Not found\n", dev_name);
	return;
}

/*
 * nss_ppe_vlan_mgr_dsa_get_real_dev()
 *	Get real dev for DSA interface
 */
static struct net_device *nss_ppe_vlan_mgr_dsa_get_real_dev(struct net_device *dev)
{
	struct dsa_port *dp = NULL;

	dp = dsa_port_from_netdev(dev);
	return dp ? dsa_port_to_master(dp) : NULL;
}

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
/*
 * nss_ppe_vlan_mgr_is_htt_dev()
 *	Returns true if dsa_dev's DSA master is dynamically confirmed (via PPE,
 *	sourced from qcom,htt-dev in DT) to be an HTT (Huntington) switch.
 */
static bool nss_ppe_vlan_mgr_is_htt_dev(struct net_device *dsa_dev)
{
	struct net_device *master_dev;
	struct ppe_drv_iface *master_iface;

	if (!dsa_dev || !dsa_slave_dev_check(dsa_dev)) {
		return false;
	}

	master_dev = nss_ppe_vlan_mgr_dsa_get_real_dev(dsa_dev);
	if (!master_dev) {
		return false;
	}

	master_iface = ppe_drv_iface_get_by_dev(master_dev);
	if (!master_iface) {
		return false;
	}

	return ppe_drv_iface_is_htt(master_iface);
}
#endif

/*
 * nss_ppe_vlan_mgr_dsa_interface_supported()
 * 	Returns true if VLAN over DSA is present and supported.
 */
static bool nss_ppe_vlan_mgr_dsa_interface_supported(struct net_device *dev)
{
	struct net_device *real_dev = NULL;
	struct ppe_drv_iface *real_iface = NULL;
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	struct net_device *dsa_dev = NULL;
#endif

	if (!is_vlan_dev(dev)) {
		nss_ppe_vlan_mgr_trace("%s is not VLAN interface\n", dev->name);
		return false;
	}

	real_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	real_iface = ppe_drv_iface_get_by_dev(real_dev);
	if (!real_iface) {
		return false;
	}

	/*
	 * For lan1.10.20, real_dev is lan1.10 (a VLAN device itself).
	 * Peel one more level so real_dev becomes lan1 (the DSA slave), but
	 * only if lan1's master is dynamically confirmed to be an HTT switch.
	 * Otherwise this path stays blocked and only single-level VLANs over
	 * DSA (lan1.10) are allowed.
	 */
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	if (is_vlan_dev(real_dev)) {
		dsa_dev = nss_ppe_vlan_mgr_get_real_dev(real_dev);
		if (nss_ppe_vlan_mgr_is_htt_dev(dsa_dev)) {
			real_dev = dsa_dev;
		}
	}
#endif

	if (!dsa_slave_dev_check(real_dev)) {
		nss_ppe_vlan_mgr_trace("%s: real_dev %s is not a DSA interface (or QinQ over DSA not enabled)\n",
				dev->name, real_dev ? real_dev->name : "NULL");
		return false;
	}

	/*
	 * Confirm the DSA master also has a PPE interface.
	 */
	real_dev = nss_ppe_vlan_mgr_dsa_get_real_dev(real_dev);
	real_iface = real_dev ? ppe_drv_iface_get_by_dev(real_dev) : NULL;
	if (!real_iface) {
		nss_ppe_vlan_mgr_trace("Master of %s doesn't have real interface\n", dev->name);
		return false;
	}

	return true;
}

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
/*
 * nss_ppe_vlan_mgr_is_htt_vlan_dsa()
 *	Returns true if dsa_dev uses the VLAN-based DSA tag protocol and its
 *	DSA master is dynamically confirmed to be an HTT (Huntington) switch.
 *
 * DSA_TAG_PROTO_QCA_8021Q identifies the DSA tagging mode. The actual
 * DSA tag TPID programmed in VLAN rules is ETH_P_DSA_8021Q.
 */
static bool nss_ppe_vlan_mgr_is_htt_vlan_dsa(struct net_device *dsa_dev)
{
	struct dsa_port *dp = NULL;

	if (!dsa_dev || !dsa_slave_dev_check(dsa_dev)) {
		return false;
	}

	dp = dsa_port_from_netdev(dsa_dev);
	if (!dp || !dp->cpu_dp || !dp->cpu_dp->tag_ops) {
		return false;
	}

	if (dp->cpu_dp->tag_ops->proto != DSA_TAG_PROTO_QCA_8021Q) {
		return false;
	}

	return nss_ppe_vlan_mgr_is_htt_dev(dsa_dev);
}
#endif
#endif

/*
 * nss_ppe_vlan_mgr_update_ppe_tpid()
 *	Update tag protocol identifier.
 */
static int nss_ppe_vlan_mgr_update_ppe_tpid(void)
{
	ppe_drv_ret_t ret;
	fal_qinq_port_role_t port_role = FAL_QINQ_EDGE_PORT;
	uint32_t mask = FAL_TPID_CTAG_EN | FAL_TPID_STAG_EN | FAL_EXT_TPID_CTAG_EN | FAL_EXT_TPID_STAG_EN | FAL_TPID_CTAG_MAP_EN | FAL_TPID_STAG_MAP_EN;
	uint16_t tpid_arr[TPID_SIZE] = {
		vlan_mgr_ctx.ctpid,
		vlan_mgr_ctx.stpid,
#ifdef NSS_EXT_VLAN_FEATURE_SUPPORT
		vlan_mgr_ctx.ctpid_ext,
		vlan_mgr_ctx.stpid_ext
#endif
	};

#ifdef NSS_VLAN_MGR_PPE_VP_TUN_SUPPORT
	mask |= (FAL_TUNNEL_TPID_CTAG_EN | FAL_TUNNEL_TPID_STAG_EN | FAL_EXT_TUNNEL_TPID_CTAG_EN | FAL_EXT_TUNNEL_TPID_STAG_EN | FAL_TUNNEL_TPID_CTAG_MAP_EN | FAL_TUNNEL_TPID_STAG_MAP_EN);
#endif

#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
	port_role = FAL_QINQ_CORE_PORT;
#endif
	ret = ppe_drv_vlan_tpid_set(tpid_arr, mask, port_role);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("failed to set TPIDs: [%d, %d, %d, %d], error = %d\n",
				tpid_arr[0], tpid_arr[1], tpid_arr[2], tpid_arr[3], ret);
		return -1;
	}

	return 0;
}

/*
 * nss_ppe_vlan_mgr_ppe_update_port_role()
 *	Update role of the port.
 *
 * both ingress and egress could be configured to edge or core.
 */
static bool nss_ppe_vlan_mgr_ppe_update_port_role(struct ppe_drv_iface *iface, int port_id, fal_qinq_port_role_t role)
{
	ppe_drv_ret_t ret;
	fal_port_qinq_role_t mode = {0};

	/*
	 * Update port role in PPE
	 */
	mode.mask = FAL_PORT_QINQ_ROLE_INGRESS_EN
			| FAL_PORT_QINQ_ROLE_EGRESS_EN;

#ifdef NSS_VLAN_MGR_PPE_VP_TUN_SUPPORT
	mode.mask |= FAL_PORT_QINQ_ROLE_TUNNEL_EN;
#endif

	mode.ingress_port_role = role;
	mode.egress_port_role = role;
#ifdef NSS_VLAN_MGR_PPE_VP_TUN_SUPPORT
	mode.tunnel_port_role = role;
#endif

	ret = ppe_drv_vlan_port_role_set(iface, port_id, &mode);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%px: Failed to set new mode to %d, error = %d\n",
					iface, port_id, ret);
		return false;
	}

	return true;
}

/*
 * nss_ppe_vlan_mgr_get_port_id()
 *	Returns corresponding iface and port_id for given net_device.
 */
static int32_t nss_ppe_vlan_mgr_get_port_id(struct net_device *dev)
{
	int32_t port_id;
	struct ppe_drv_iface *iface = ppe_drv_iface_get_by_dev(dev);
	if (!iface) {
		nss_ppe_vlan_mgr_warn("%px: %s: couldn't get PPE iface\n", dev, dev->name);
		return NSS_PPE_VLAN_MGR_INVALID_PORT;
	}

	port_id = ppe_drv_iface_port_idx_get(iface);
	if (port_id != NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_info("%px: %s:%d is valid port\n", dev, dev->name, port_id);
		return port_id;

	}

	nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", dev, dev->name, port_id);
	return NSS_PPE_VLAN_MGR_INVALID_PORT;
}

/*
 * nss_ppe_vlan_mgr_calculate_new_vp_port_role()
 *	check if we can change this base/VP port to edge port
 */
static void nss_ppe_vlan_mgr_calculate_new_vp_port_role(int32_t port, int32_t portindex)
{
	struct nss_vlan_pvt *v;
	bool to_edge_port = true;

	if (vlan_mgr_ctx.port_role[port] == FAL_QINQ_EDGE_PORT) {
		return;
	}

	if (vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) {
		return;
	}

	/*
	 * If no other VLAN as VP interface is configured on the same physcial
	 * port, lets set the PPE port as EDGE port
	 */
	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if ((v->port[portindex] == port) && (v->is_vlan_as_vp_iface)) {
			to_edge_port = false;
			break;
		}
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	if (to_edge_port) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, port, FAL_QINQ_EDGE_PORT)) {
			nss_ppe_vlan_mgr_warn("failed to set %d as edge port\n", port);
			return;
		}
		vlan_mgr_ctx.port_role[port] = FAL_QINQ_EDGE_PORT;
	}
}

/*
 * nss_ppe_vlan_mgr_calculate_new_port_role()
 *	check if we can change this port to edge port
 */
static bool nss_ppe_vlan_mgr_calculate_new_port_role(struct ppe_drv_iface *iface, int32_t port, int32_t portindex)
{
	struct nss_vlan_pvt *v;
	bool to_edge_port = true;

	if (vlan_mgr_ctx.port_role[port] == FAL_QINQ_EDGE_PORT) {
		return false;
	}

	if (vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) {
		return false;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
		/*
		 * If no other single/double VLAN on the same physcial port,
		 * we set PPE port as edge port
		 */
		if (v->port[portindex] == port) {
#else
		/*
		 * If no other double VLAN interface on the same physcial port,
		 * we set PPE port as edge port
		 */
		if ((v->port[portindex] == port) && (v->parent)) {
#endif
			to_edge_port = false;
			break;
		}
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	if (to_edge_port) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(iface, port, FAL_QINQ_EDGE_PORT)) {
			nss_ppe_vlan_mgr_warn("failed to set %d as edge port\n", port);
			return false;
		}

		vlan_mgr_ctx.port_role[port] = FAL_QINQ_EDGE_PORT;
	}

	return to_edge_port;
}

/*
 * nss_ppe_vlan_mgr_port_role_update()
 *	Update PPE port role between EDGE and CORE.
 */
static void nss_ppe_vlan_mgr_port_role_update(struct nss_vlan_pvt *v,
					uint32_t new_ppe_cvid,
					uint32_t new_ppe_svid,
					uint32_t port_id)
{
	ppe_drv_ret_t ret;
	v->xlate_info.br = v->bridge_iface;
	v->xlate_info.port_id = port_id;

	ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%px: Failed to delete old translation rule for port: %d, error = %d\n", v, port_id, ret);
	}

	/*
	 * Update ppe_civd and ppe_svid
	 */
	v->ppe_cvid = new_ppe_cvid;
	v->ppe_svid = new_ppe_svid;

	/*
	 * Add new egress vlan translation rule
	 */
	v->xlate_info.svid = new_ppe_svid;
	v->xlate_info.cvid = new_ppe_cvid;

	ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%px: Failed to add new translation rule for port: %d, error = %d\n", v, port_id, ret);
	}
}

/*
 * nss_ppe_vlan_mgr_port_role_over_bond_update()
 *	Update port role for bond slaves.
 */
static void nss_ppe_vlan_mgr_port_role_over_bond_update(struct nss_vlan_pvt *v,
					uint32_t new_ppe_cvid,
					uint32_t new_ppe_svid)
{
	int i;

	v->xlate_info.port_id = 0;

	/*
	 * For vlan over bond, the vif->eg_xlt_action will be modified while
	 * updating the first slave. Hence we need to store the old vif->eg_xlt_action
	 * and pass the same to modify the existing entry for the other ports/slaves. After
	 * modification of all ports we should update vif->eg_xlt_action with the
	 * new value which is passed to ssdk.
	 */
	for (i = 0; i < NSS_PPE_VLAN_MGR_PORT_MAX; i++) {
		if (!v->port[i]) {
			continue;
		}

		v->xlate_info.port_id = v->port[i];
		nss_ppe_vlan_mgr_port_role_update(v, new_ppe_cvid, new_ppe_svid, v->port[i]);
	}
}

/*
 * nss_ppe_vlan_mgr_port_role_event()
 *	Decide port role updation for bond or simple device
 */
static void nss_ppe_vlan_mgr_port_role_event(int32_t port, int portindex)
{
	struct nss_vlan_pvt *v;
	bool vlan_over_bond = false;

	if (vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) {
		return;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if ((v->port[portindex] == port) && (!v->parent)) {
			/*
			 * For VLAN as VP interface, this is not required.
			 */
			if (v->is_vlan_as_vp_iface) {
				continue;
			}

			vlan_over_bond = ((v->bond_id > 0) ? true : false);

			if ((vlan_mgr_ctx.port_role[port] == FAL_QINQ_EDGE_PORT) &&
			    (v->vid != v->ppe_cvid)) {
				if (!vlan_over_bond) {
					nss_ppe_vlan_mgr_port_role_update(v, v->vid, FAL_VLAN_INVALID, v->port[0]);
				} else {
					nss_ppe_vlan_mgr_port_role_over_bond_update(v, v->vid, FAL_VLAN_INVALID);
				}
			}

			if ((vlan_mgr_ctx.port_role[port] == FAL_QINQ_CORE_PORT) &&
			    (v->vid != v->ppe_svid)) {
				if (!vlan_over_bond) {
					nss_ppe_vlan_mgr_port_role_update(v, FAL_VLAN_INVALID, v->vid, v->port[0]);
				} else {
					nss_ppe_vlan_mgr_port_role_over_bond_update(v, FAL_VLAN_INVALID, v->vid);
				}
			}
		}
	}
	spin_unlock(&vlan_mgr_ctx.lock);
}

/*
 * nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_add()
 *	Wrapper API for adding the ingress in VLAN over bridge case
 */
static int nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_add(struct ppe_drv_iface *slave_iface,
								struct nss_vlan_pvt *v)
{
	int32_t port_id;
	bool update_port_role = false;

	if (ppe_drv_vlan_over_bridge_add_ig_rule(slave_iface, v->iface) != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%p: Add ingress xlate rule failed for slave\n", slave_iface);
		return -1;
	}

#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
	update_port_role = true;
#else
	/*
	 * Double VLAN case.
	 */
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE)
		update_port_role = true;
#endif

	if (update_port_role) {
		/*
		 * Getting port id from iface.
		 */
		port_id = ppe_drv_iface_port_idx_get(slave_iface);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			nss_ppe_vlan_mgr_warn("Port ID is invalid for %p slave iface\n", slave_iface);
			return -1;
		}

		/*
		 * Updating port role to CORE.
		 */
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, port_id, FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("failed to set %d as core port\n", port_id);
			return -1;
		}
	}

	return 0;
}

/*
 * nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_del()
 *	Wrapper API for deleting the ingress in VLAN over bridge case
 */
static int nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_del(struct ppe_drv_iface *slave_iface,
								struct nss_vlan_pvt *v)
{
	int32_t port_id;
	bool update_port_role = false;

	if (ppe_drv_vlan_over_bridge_del_ig_rule(slave_iface, v->iface) != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%p: Delete ingress xlate rule failed for slave\n", slave_iface);
		return -1;
	}

	nss_ppe_vlan_mgr_trace("Deleting ingress rule success for vid %d port %d\n", v->vid, v->port[0]);

#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
	update_port_role = true;
#else
	/*
	 * Double VLAN case.
	 */
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE)
		update_port_role = true;
#endif

	if (update_port_role) {
		/*
		 * Getting port id from iface.
		 */
		port_id = ppe_drv_iface_port_idx_get(slave_iface);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			nss_ppe_vlan_mgr_warn("Port ID is invalid for %p slave iface\n", slave_iface);
			return -1;
		}

		/*
		 * Updating port role to EDGE.
		 */
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, port_id, FAL_QINQ_EDGE_PORT)) {
			nss_ppe_vlan_mgr_warn("failed to set %d as core port\n", port_id);
			return -1;
		}
	}

	return 0;
}

/*
 * nss_ppe_vlan_mgr_bond_configure_ppe()
 *	Configure PPE for bond device
 */
static int nss_ppe_vlan_mgr_bond_configure_ppe(struct nss_vlan_pvt *v, struct net_device *bond_dev, struct net_device *dev)
{
	int res = 0;
	struct net_device *slave_dev;
	int32_t port_id;
	bool vlan_over_bridge = false;
	int vlan_mgr_bond_port_role = -1;
	ppe_drv_ret_t ret;

	struct net_device *base_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	if (!base_dev) {
		nss_ppe_vlan_mgr_warn("%s: failed to obtain bond_dev", dev->name);
		return -1;
	}

	v->iface = ppe_drv_iface_alloc(PPE_DRV_IFACE_TYPE_VLAN, dev);
	if (!v->iface) {
		nss_ppe_vlan_mgr_warn("%s: failed to allocate IFACE for vlan device", bond_dev->name);
		return -1;
	}

	/*
	 * PPE expects base_dev here. So, for bond0.10, base_dev will be bond0.
	 * Not the actual real device is needed.
	 */
	ret = ppe_drv_vlan_init(v->iface, base_dev, v->vid, vlan_over_bridge);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: failed to initialize, PPE updated, error = %d\n", dev->name, ret);
		goto free_iface;
	}

	/*
	 * Disable FDB learning based on module parameter configuration.
	 */
	if (vlan_fdb_learn_dis) {
		ret = ppe_drv_vlan_fdb_learn_disable(v->iface, true);
		if (ret != PPE_DRV_RET_SUCCESS) {
			nss_ppe_vlan_mgr_trace("%s: failed to configure VSI FDB learning, error = %d\n",
					dev->name, ret);
			goto deinit_iface;
		}
	}

	ret = ppe_drv_iface_mac_addr_set(v->iface, v->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: Failed to set MAC address, error = %d\n", dev->name, ret);
		goto deinit_iface;
	}

	ret = ppe_drv_iface_mtu_set(v->iface, v->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: Failed to set MTU, error = %d\n", dev->name, ret);
		goto clear_mac_addr;
	}

	/*
	 * Set vlan_mgr_bond_port_role and check
	 * if all the bond slaves are PPE ports
	 */
	rcu_read_lock();
	for_each_netdev_in_bond_rcu(bond_dev, slave_dev) {
		port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n",
									slave_dev, slave_dev->name, port_id);
			goto clear_mac_addr;
		}

		/*
		 * Each slave interface inside bond should get attached to
		 * vlan over bond interface
		 */
		ret = ppe_drv_vlan_lag_slave_join(v->iface, slave_dev);
		if (ret != PPE_DRV_RET_SUCCESS) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s:%d slave_dev failed to attach bond vlan iface\n",
					slave_dev, slave_dev->name, port_id);
			goto leave_lag_slaves;
		}

#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, port_id, FAL_QINQ_CORE_PORT)) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", slave_dev->name, port_id);
			goto leave_lag_slaves;
		}
		vlan_mgr_ctx.port_role[port_id] = FAL_QINQ_CORE_PORT;
#endif

		/*
		 * vlan_mgr_bond_port_role is same for all the slaves in the bond group
		 */
		if (vlan_mgr_bond_port_role == -1) {
			vlan_mgr_bond_port_role = vlan_mgr_ctx.port_role[port_id];
		}
	}
	rcu_read_unlock();

	/*
	 * In case the bond interface has no slaves, we do not want to proceed further
	 */
	if (vlan_mgr_bond_port_role == -1) {
		if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) {
			v->ppe_cvid = v->vid;
			v->ppe_svid = v->parent->vid;
		} else {
			if (v->tpid == vlan_mgr_ctx.stpid) {
				v->ppe_cvid = FAL_VLAN_INVALID;
				v->ppe_svid = v->vid;
			} else {
				v->ppe_cvid = v->vid;
				v->ppe_svid = FAL_VLAN_INVALID;
			}
		}

		v->xlate_info.br = NULL;
		v->xlate_info.svid = v->ppe_svid;
		v->xlate_info.cvid = v->ppe_cvid;
		return PPE_DRV_RET_SUCCESS;
	}

	/*
	 * Calculate ppe cvid and svid
	 */
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) {
		v->ppe_cvid = v->vid;
		v->ppe_svid = v->parent->vid;
	} else {
		if (((vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) && (v->tpid == vlan_mgr_ctx.ctpid)) ||
				((vlan_mgr_ctx.ctpid == vlan_mgr_ctx.stpid) &&
				 (vlan_mgr_bond_port_role == FAL_QINQ_EDGE_PORT))) {
			v->ppe_cvid = v->vid;
			v->ppe_svid = FAL_VLAN_INVALID;
		} else {
			v->ppe_cvid = FAL_VLAN_INVALID;
			v->ppe_svid = v->vid;
		}
	}

	v->xlate_info.br = NULL;
	v->xlate_info.svid = v->ppe_svid;
	v->xlate_info.cvid = v->ppe_cvid;

	/*
	 * Add ingress vlan translation rule
	 */
	rcu_read_lock();
	for_each_netdev_in_bond_rcu(bond_dev, slave_dev) {
		port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n",
									slave_dev, slave_dev->name, port_id);
			goto leave_lag_slaves;
		}

		v->xlate_info.port_id = v->port[port_id - 1];
		ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%s: failed to set vlan translation, error = %d\n", slave_dev->name, ret);
			goto leave_lag_slaves;
		}
	}
	rcu_read_unlock();

#ifndef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
	/*
	 * Update vlan port role
	 */
	if ((v->ppe_svid != FAL_VLAN_INVALID) && (vlan_mgr_bond_port_role != FAL_QINQ_CORE_PORT)) {
		rcu_read_lock();
		for_each_netdev_in_bond_rcu(bond_dev, slave_dev) {
			port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
			if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
				rcu_read_unlock();
				nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n",
									slave_dev, slave_dev->name, port_id);
				goto delete_ppe_rule;
			}

			if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, port_id, FAL_QINQ_CORE_PORT)) {
				rcu_read_unlock();
				nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", slave_dev->name, port_id);
				goto delete_ppe_rule;
			}
			vlan_mgr_ctx.port_role[port_id] = FAL_QINQ_CORE_PORT;
		}
		rcu_read_unlock();
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}
#endif

	return res;

#ifndef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
delete_ppe_rule:
	rcu_read_lock();
	for_each_netdev_in_bond_rcu(bond_dev, slave_dev) {
		port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n",
									slave_dev, slave_dev->name, port_id);
			rcu_read_unlock();
			v->iface = NULL;
			return -1;
		}

		v->xlate_info.port_id = port_id;
		ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			nss_ppe_vlan_mgr_warn("%s: failed to delete vlan translation, error = %d \n", slave_dev->name, ret);
		}
	}
	rcu_read_unlock();
#endif

leave_lag_slaves:
	rcu_read_lock();
	for_each_netdev_in_bond_rcu(bond_dev, slave_dev) {
		/*
		 * Each slave interface inside bond should get detached from
		 * vlan over bond interface if attached already
		 */
		ret = ppe_drv_vlan_lag_slave_leave(v->iface, slave_dev);
		if (ret != PPE_DRV_RET_SUCCESS) {
			nss_ppe_vlan_mgr_warn("%px: %s:%d slave_dev failed to detach from bond vlan iface\n",
					slave_dev, slave_dev->name, port_id);
			rcu_read_unlock();
			return -1;
		}
	}
	rcu_read_unlock();

clear_mac_addr:
	ppe_drv_iface_mac_addr_clear(v->iface);

deinit_iface:
	ppe_drv_vlan_deinit(v->iface);

free_iface:
	ppe_drv_iface_deref(v->iface);
	v->iface = NULL;
	return -1;
}

/*
 * nss_ppe_vlan_mgr_untag_and_send()
 *	Untag exception packet and send it to stack.
 */
bool nss_ppe_vlan_mgr_vp_src_exception(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;
	struct net_device *real_dev;
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	struct dsa_port *dp  __maybe_unused = NULL;
#endif

	real_dev = nss_ppe_vlan_mgr_get_real_dev(skb->dev);
	if (real_dev && is_vlan_dev(real_dev)) {
		real_dev = nss_ppe_vlan_mgr_get_real_dev(real_dev);
	}

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	/*
	 * DSA interface, if it's not a VLAN dev?
	 * Under QINQ, PPE strips the DSA tag in hardware so deliver directly to
	 * the DSA slave. Without QINQ, walk up to the master to present the tag.
	 */
	if (!real_dev && dsa_slave_dev_check(skb->dev)) {
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
		if (nss_ppe_vlan_mgr_is_htt_dev(skb->dev)) {
			real_dev = skb->dev;
		} else
#endif
		{
			dp = dsa_port_from_netdev(skb->dev);
			if (dp) {
				real_dev = dsa_port_to_master(dp);
			}
		}
	}

	/*
	 * VLAN over DSA interface? Walk to master unless real_dev is an
	 * HTT-switch DSA slave, where PPE already stripped the tag.
	 */
	if ((real_dev) && (dsa_slave_dev_check(real_dev)) && (real_dev != skb->dev)) {
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
		if (!nss_ppe_vlan_mgr_is_htt_dev(real_dev))
#endif
		{
			dp = dsa_port_from_netdev(real_dev);
			if (dp) {
				real_dev = dsa_port_to_master(dp);
			}
		}
	}
#endif

	if (!real_dev) {
		nss_ppe_vlan_mgr_warn("%s: failed to obtain real_dev", skb->dev->name);
		return false;
	}

	skb->dev = real_dev;
	skb->skb_iif = real_dev->ifindex;
	skb->protocol = eth_type_trans(skb, skb->dev);

	if (likely(real_dev->features & NETIF_F_RXCSUM)) {
		skb->ip_summed = info->ip_summed;
	}

	if (unlikely(real_dev->features & NETIF_F_GRO)) {
		napi_gro_receive(info->napi, skb);
	} else {
		netif_receive_skb(skb);
	}

	return true;
}

/*
 * nss_ppe_vlan_mgr_vp_dst_exception()
 *	Free the exception packet received from destination VP callback.
 */
bool nss_ppe_vlan_mgr_vp_dst_exception(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;

	nss_ppe_vlan_mgr_trace("VP dst exception handler: freeing the skb\n");
	dev_kfree_skb_any(skb);
	return false;
}

#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
/*
 * nss_ppe_vlan_mgr_veip_exception()
 *	VEIP VP callback: deliver to upper stack.
 *	Assumes skb->dev and skb->protocol were set by ppe_vp_rx_dp_cb.
 */
bool nss_ppe_vlan_mgr_veip_exception(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;

	/*
	 * If skb->dev is a VLAN device, switch to its base device and
	 * update protocol before delivering to the network stack.
	 */
	if (is_vlan_dev(skb->dev)) {
		struct net_device *real_dev = vlan_dev_real_dev(skb->dev);
		if (real_dev) {
			skb->dev = real_dev;
			skb->skb_iif = real_dev->ifindex;
			skb->protocol = eth_type_trans(skb, skb->dev);
		}
	}

	/* Directly hand to the network stack as requested. */
	netif_receive_skb(skb);

	return true;
}
#endif

/*
 * nss_ppe_vlan_mgr_deconfigure_vp()
 *	Deconfigure vlan instance configured as VP.
 */
static void nss_ppe_vlan_mgr_deconfigure_vp(struct nss_vlan_pvt *v)
{
	ppe_drv_ret_t ret;
	int i;

	ret = ppe_drv_vlan_as_vp_del_xlate_rules(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("failed to delete vlan translation rule for vp, error = %d \n", ret);
	}

	/*
	 * Need to revert the port role.
	 * While adding VLAN as VP, the role of the port(s) changed from
	 * EDGE to CORE. So, while removing VLAN as VP, the role of the
	 * port(s) should be changed back to EDGE.
	 */
	for (i = 0; i < NSS_PPE_VLAN_MGR_PORT_MAX; i++) {
		if (v->port[i]) {
			nss_ppe_vlan_mgr_calculate_new_vp_port_role(v->port[i], i);
		}
	}

	ppe_drv_iface_mtu_set(v->iface, 0);
	ppe_drv_iface_mac_addr_clear(v->iface);
}

/*
 * nss_ppe_vlan_mgr_free_vp()
 *	free the allocated vp for VLAN interface.
 */
static void nss_ppe_vlan_mgr_free_vp(int16_t vp_num)
{
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;

	status = ppe_vp_free(vp_num);
	if (status != PPE_VP_STATUS_SUCCESS) {
		nss_ppe_vlan_mgr_warn("failed to free VP object= %d \n", vp_num);
	}
}

/*
 * nss_ppe_vlan_mgr_is_wlan_dev()
 * 	API to check if the realdev is a WLAN dev.
 */
static bool nss_ppe_vlan_mgr_is_wlan_dev(struct net_device *dev)
{
	return !!dev->ieee80211_ptr;
}

#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
/*
 * nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_cleanup()
 *	Free translate context allocated to handle DL WIFI flows.
 */
static void nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_cleanup(struct kref *kref)
{
	struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *dst_xlate_ctx;

	dst_xlate_ctx = container_of(kref, struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx, ref);

	ppe_drv_vlan_wlanif_vp_tun_enc_destroy(dst_xlate_ctx->ptec);

	spin_lock(&vlan_mgr_ctx.lock);
	if (!list_empty(&dst_xlate_ctx->list)) {
		list_del(&dst_xlate_ctx->list);
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	nss_ppe_vlan_mgr_minidump_free(dst_xlate_ctx, "nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx");
	kfree(dst_xlate_ctx);
}

/*
 * nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_setup()
 *	Allocate and configure translate context required to handle DL flows.
 *
 * In the downlink direction after flow lookup and vlan translation the
 * destination translate contxt is used to update the DEST_INFO with real wlan dev.
 *
 * NOTE: This API is called under global spin lock.
 */
static struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_setup(struct net_device *dev, int16_t vp_num) {
	struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *xlate_ctx = NULL;

	xlate_ctx = kzalloc(sizeof(struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx), GFP_ATOMIC);
	if (!xlate_ctx) {
		nss_ppe_vlan_mgr_warn("%px:%s:Failed to allocate memory for destination translate context for wlan vlan vp\n", dev, dev->name);
		return NULL;
	}

	xlate_ctx->ptec = ppe_drv_vlan_wlanif_tun_enc_setup(dev, vp_num);
	if (!xlate_ctx->ptec) {
		nss_ppe_vlan_mgr_warn("%px:%s:Failed to setup destination xlate ctx\n", dev, dev->name);
		kfree(xlate_ctx);
		return NULL;
	}

	nss_ppe_vlan_mgr_minidump_log(xlate_ctx, sizeof(struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx), "nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx");

	INIT_LIST_HEAD(&xlate_ctx->list);
	xlate_ctx->vp_num = vp_num;
	kref_init(&xlate_ctx->ref);
	list_add_tail(&vlan_mgr_ctx.xlate_ctx_list, &xlate_ctx->list);
	return xlate_ctx;
}

/*
 * nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_put()
 *	Decrement reference on WLAN VP destination translate context.
 */
static void nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_put(struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *dst_xlate)
{
	kref_put(&dst_xlate->ref, nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_cleanup);
}

/*
 * nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_get()
 *	Allocate/get Custom WLAN VLAN VP destination translate context.
 */
static struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_get(struct net_device *dev)
{
	struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *dst_xlate = NULL;
	int16_t realdev_vp_num;

	realdev_vp_num = ppe_drv_port_num_from_dev(dev);
	if (realdev_vp_num == PPE_DRV_PORT_ID_INVALID) {
		nss_ppe_vlan_mgr_warn("%px: No Port in PPE associated with dev %s\n", dev, dev->name);
		return NULL;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(dst_xlate, &vlan_mgr_ctx.xlate_ctx_list, list) {
		if (dst_xlate->vp_num == realdev_vp_num) {
			/*
			 * Take reference on the destination translate context if already created.
			 */
			kref_get(&dst_xlate->ref);
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_trace("%px: Destination xlate contecxt found for WLAN  Parent ndev %s\n", dev, dev->name);
			return dst_xlate;
		}
	}

	/*
	 * Setup dst_xlate_ctx for the first VLAN created on top
	 * of Real WLAN netdev.
	 */
	dst_xlate = nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_setup(dev, realdev_vp_num);
	spin_unlock(&vlan_mgr_ctx.lock);

	nss_ppe_vlan_mgr_trace("%px: First VLAN on WLAN dev %s, setting up dest xlate ctx\n", dev, dev->name);

	return dst_xlate;
}
#endif

/*
 * nss_ppe_vlan_mgr_alloc_vp()
 *	allocate vp for vlan interface.
 */
static ppe_vp_num_t nss_ppe_vlan_mgr_alloc_vp(struct net_device *dev, struct net_device *vlan_as_vp_real_dev)
{
	int16_t queue_num = 0;
	uint32_t port_num = 0;
	struct ppe_vp_ai vpai = {0};
	ppe_vp_num_t vp_num = NSS_PPE_VLAN_MGR_INVALID_PORT;
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	struct dsa_port *dp = NULL;
	struct ppe_drv_iface *master_if = NULL;
	int16_t htt_queue;
#endif

	if (!nss_ppe_vlan_mgr_is_wlan_dev(vlan_as_vp_real_dev)) {

		port_num = nss_ppe_vlan_mgr_get_port_id(vlan_as_vp_real_dev);
		if (port_num == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			nss_ppe_vlan_mgr_warn("Invalid port number for vlan as vp real dev: %s\n", vlan_as_vp_real_dev->name);
			return NSS_PPE_VLAN_MGR_INVALID_PORT;
		}

		queue_num = ppe_drv_port_ucast_queue_get_by_port(port_num);
		if (queue_num < 0) {
			nss_ppe_vlan_mgr_warn("Invalid queue id for vlan as vp \
					real dev: %s\n", vlan_as_vp_real_dev->name);
			return NSS_PPE_VLAN_MGR_INVALID_PORT;
		}

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
		/*
		 * For DSA slaves under a Huntington switch, assign a per-port queue
		 * using the DSA switch port index (dp->index), same as nss_ppe_dsa_mgr.
		 */
		if (nss_ppe_vlan_mgr_is_htt_vlan_dsa(dev)) {
			dp = dsa_port_from_netdev(dev);
			master_if = ppe_drv_iface_get_by_dev(vlan_as_vp_real_dev);

			if (dp && master_if && ppe_drv_iface_is_htt(master_if)) {
				htt_queue = ppe_drv_port_htt_queue_base_get(port_num, dp->index);
				if (htt_queue < 0) {
					nss_ppe_vlan_mgr_warn("Invalid HTT queue for dev %s port %d swpt_id %d\n",
							dev->name, port_num, dp->index);
					return NSS_PPE_VLAN_MGR_INVALID_PORT;
				}
				queue_num = htt_queue;
			}
		}
#endif
		vpai.type = PPE_VP_TYPE_SW_L2;
		vpai.queue_num = queue_num;
		vpai.xmit_port = port_num;

		/*
		 * FDB learning for VLAN-based DSA VPs (dev is either the DSA slave
		 * itself, or a VLAN interface stacked over one, i.e. VLAN over DSA)
		 * is controlled by dsa_vp_fdb_learn_enabled, which defaults to
		 * enabled only on platforms that support FDB-based forwarding for
		 * these VPs; other platforms default it to disabled since they rely
		 * on a flow-rule-based multi Tx ring solution for HOLB that breaks
		 * with FDB rules. Plain VLAN-as-VP interfaces always follow the
		 * vp_fdb_learn_enabled module param.
		 */
		vpai.fdb_learn_enabled = vp_fdb_learn_enabled;
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
		if (dsa_slave_dev_check(dev) || nss_ppe_vlan_mgr_dsa_interface_supported(dev)) {
			vpai.fdb_learn_enabled = dsa_vp_fdb_learn_enabled;
		}
#endif
#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
	} else {

		int16_t vp_num = ppe_drv_port_num_from_dev(vlan_as_vp_real_dev);
		vpai.type = PPE_VP_TYPE_HW_L2TUN;
		vpai.usr_type = ppe_vp_user_type_get(vp_num);
		vpai.net_dev_type = PPE_VP_NET_DEV_TYPE_WIFI;
#endif
	}

	vpai.dst_cb = nss_ppe_vlan_mgr_vp_dst_exception;
	vpai.src_cb = nss_ppe_vlan_mgr_vp_src_exception;

	nss_ppe_vlan_mgr_trace("%s: port: %d, queue num: %d\n", dev->name, port_num, queue_num);

	/*
	 * Allocate VP for valid vlan interface.
	 */
	vp_num = ppe_vp_alloc(dev, &vpai);
	if (vp_num < 0) {
		nss_ppe_vlan_mgr_warn("vp alloc failed for dev %s status: %d", dev->name, vpai.status);
		return NSS_PPE_VLAN_MGR_INVALID_PORT;
	}

	return vp_num;
}

/*
 * nss_ppe_vlan_mgr_alloc_configure_ppe_vp()
 *	Configure PPE VP for VLAN devices.
 *
 * It will allocate VP object and VP interface.
 *
 * TODO: Fix cleanup of resources in error cases.
 */
static int nss_ppe_vlan_mgr_alloc_configure_ppe_vp(struct nss_vlan_pvt *v, struct net_device *dev,
			struct net_device *vlan_as_vp_real_dev)
{
	ppe_vp_num_t vp_num;
	int res = 0;

	/*
	 * nss_ppe_vlan_mgr_alloc_configure_ppe_vp() get called only when vlan_as_vp is enabled.
	 */
	vp_num = nss_ppe_vlan_mgr_alloc_vp(dev, vlan_as_vp_real_dev);
	if (vp_num == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("VP allocation failed for device: %s\n", dev->name);
		return -1;
	}

	v->iface = ppe_drv_iface_get_by_dev(dev);
	if (!v->iface) {
		nss_ppe_vlan_mgr_warn("%s: failed to get IFACE for vlan device", dev->name);
		return -1;
	}

#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
	/*
	 * For VP created on top of wlan netdevices,
	 * Create a destination xlate context to update the DEST_INFO
	 * with real WLAN VP after the vlan translation.
	 */
	if (nss_ppe_vlan_mgr_is_wlan_dev(vlan_as_vp_real_dev)) {
		struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *xlate_ctx;

		xlate_ctx = nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_get(vlan_as_vp_real_dev);
		if (!xlate_ctx) {
			nss_ppe_vlan_mgr_warn("WLAN dest xlate ctx get failed: %s\n", dev->name);
			return -1;
		}

		if (!ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach(xlate_ctx->ptec, vp_num)) {
			nss_ppe_vlan_mgr_warn("WLAN dest xlate setup failed: %s\n", dev->name);
			return -1;
		}

		v->xlate_ctx = xlate_ctx;
	}
#endif
	/*
	 * Setting src interface for VLAN packets as VP interface.
	 */
	v->port[vp_num - 1] = v->xlate_info.port_id = vp_num;
	v->is_vlan_as_vp_iface = true;

	/*
	 * Update the default port role to CORE PORT for both the base port as
	 * well the VLAN virtual port.
	 * The base port's role needs to be updated for ingress translation rule
	 * while the VLAN virtual port's role needs to be updated for the egress
	 * translation rule.
	 */
	if (vlan_mgr_ctx.port_role[v->port[0]] != FAL_QINQ_CORE_PORT) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[0], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[0]);
			return -1;
		}
		vlan_mgr_ctx.port_role[v->port[0]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}

	if (vlan_mgr_ctx.port_role[v->port[vp_num - 1]] != FAL_QINQ_CORE_PORT) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[vp_num - 1], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[vp_num - 1]);
			return -1;
		}
		vlan_mgr_ctx.port_role[v->port[vp_num - 1]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}

	/*
	 * Calculate the cvid and svid.
	 *
	 * TYPE_TRIPLE means: double user VLAN over DSA (e.g. lan1.10.20).
	 *   PPE HW sees 3 tags: DSA internal tag + S-tag (lan1.10 vid) + C-tag (lan1.10.20 vid).
	 *   v->vid       = C-tag (innermost user VLAN, lan1.10.20) → CVID
	 *   v->parent->vid = S-tag (outer user VLAN, lan1.10)      → SVID
	 *
	 * TYPE_DOUBLE means: either a regular eth0.10.20 stacked VLAN, or the first
	 *   user VLAN on a DSA interface (lan1.10) when DSA QinQ is enabled.
	 *   For lan1.10 the DSA internal tag is already handled by the VP; treat
	 *   this VLAN as single-tag using the normal TPID/port-role logic.
	 *
	 * TYPE_SINGLE: normal single-tag, TPID/port-role decides CVID vs SVID.
	 */
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_TRIPLE) {
		v->ppe_cvid = v->vid;
		v->ppe_svid = v->parent->vid;
	} else if ((NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) &&
		   (nss_ppe_vlan_mgr_is_htt_vlan_dsa(vlan_as_vp_real_dev))) {
		/*
		 * Single user VLAN over DSA (e.g. lan1.10): DSA tag is internal to PPE.
		 * Treat as single tag — follow port role to decide cvid vs svid,
		 * same as TYPE_SINGLE.
		 */
		if (((vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) && (v->tpid == vlan_mgr_ctx.ctpid)) ||
				((vlan_mgr_ctx.ctpid == vlan_mgr_ctx.stpid) &&
				 (vlan_mgr_ctx.port_role[v->port[0]] == FAL_QINQ_EDGE_PORT))) {
			v->ppe_cvid = v->vid;
			v->ppe_svid = FAL_VLAN_INVALID;
		} else {
			v->ppe_cvid = FAL_VLAN_INVALID;
			v->ppe_svid = v->vid;
		}
	} else {
#endif
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) {
		v->ppe_cvid = v->vid;
		v->ppe_svid = v->parent->vid;
	} else {
		if (((vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) && (v->tpid == vlan_mgr_ctx.ctpid)) ||
				((vlan_mgr_ctx.ctpid == vlan_mgr_ctx.stpid) &&
				 (vlan_mgr_ctx.port_role[v->port[0]] == FAL_QINQ_EDGE_PORT))) {
			v->ppe_cvid = v->vid;
			v->ppe_svid = FAL_VLAN_INVALID;
		} else {
			v->ppe_cvid = FAL_VLAN_INVALID;
			v->ppe_svid = v->vid;
		}
	}
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	}
#endif

	v->xlate_info.br = NULL;
	v->xlate_info.svid = v->ppe_svid;
	v->xlate_info.cvid = v->ppe_cvid;

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	/*
	 * For the base DSA slave (lan1), the VP carries the internal DSA tag.
	 * No VLAN translation rule is needed at this level; skip it.
	 * VLAN rules are only programmed for user VLANs on top (lan1.10, lan1.10.20).
	 */
	if (nss_ppe_vlan_mgr_is_htt_vlan_dsa(dev)) {
		nss_ppe_vlan_mgr_trace("%s: DSA slave — VP created, skipping VLAN xlate rules\n",
				dev->name);
		return res;
	}
#endif
	/*
	 * Add VLAN translation rules for:
	 * 1. Non-DSA interfaces
	 * 2. DSA without triple VLAN support
	 * 3. VLAN over DSA with QinQ support
	 */
	if (ppe_drv_vlan_as_vp_add_xlate_rules(v->iface, &v->xlate_info) != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: failed to set vlan translation\n", dev->name);
		return -1;
	}

	nss_ppe_vlan_mgr_trace("%s: v->port[0]: %d, v->port[vp_num - 1]: %d\n", dev->name,
			v->port[0], v->port[vp_num - 1]);
	return res;
}

#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
/*
 * nss_ppe_vlan_mgr_alloc_configure_ppe_veip()
 *	Configure PPE VEIP VP for VLAN devices using the upstream PON port.
 */
static int nss_ppe_vlan_mgr_alloc_configure_ppe_veip(struct nss_vlan_pvt *v, struct net_device *dev)
{
	struct net_device *base_dev;
	struct ppe_drv_veip_vp_info vp_info;
	struct ppe_vp_ai vpai = {0};
	ppe_drv_ret_t ret;
	int res = 0;

	/*
	 * PPE expects the underlying physical device (base_dev). For stacked VLANs (e.g., eth0.10.20),
	 * this refers to the intermediate interface (eth0.10).
	 */
	base_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	if (!base_dev) {
		nss_ppe_vlan_mgr_warn("%s: failed to obtain base_dev", dev->name);
		return -1;
	}

	/*
	 * Allocate and initialize VEIP interface with both VP ports
	 */
	v->iface = ppe_drv_iface_alloc(PPE_DRV_IFACE_TYPE_VEIP, dev);
	if (!v->iface) {
		nss_ppe_vlan_mgr_warn("%s: failed to allocate IFACE for vlan device", dev->name);
		return -1;
	}

	if (ppe_drv_veip_init(v->iface, base_dev, &vp_info) != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: failed to initialize VEIP interface\n", dev->name);
		ppe_drv_iface_deref(v->iface);
		v->iface = NULL;
		return -1;
	}

	/*
	 * Set MAC address for the VEIP interface
	 */
	ret = ppe_drv_iface_mac_addr_set(v->iface, v->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: Failed to set MAC address, error = %d\n", dev->name, ret);
		goto free_ppe_iface;
	}

	/*
	 * Set MTU for the VEIP interface
	 */
	ret = ppe_drv_iface_mtu_set(v->iface, v->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: Failed to set MTU, error = %d\n", dev->name, ret);
		goto clear_mac_addr;
	}

	/*
	 * Configure VP allocation info
	 */
	vpai.dst_cb = nss_ppe_vlan_mgr_veip_exception;
	vpai.src_cb = nss_ppe_vlan_mgr_veip_exception;
	vpai.type = PPE_VP_TYPE_SW_L2;

	/*
	 * Allocate VP structures for both GW and PON ports
	 */
	if (ppe_vp_veip_alloc_vps(v->iface, vp_info.gw_vp_num, vp_info.pon_vp_num, dev, &vpai) < 0) {
		nss_ppe_vlan_mgr_warn("%s: failed to allocate VP structures\n", dev->name);
		goto clear_mtu;
	}

	if (netdev_hw_offload_ops_register(dev, &nss_ppe_vlan_mgr_veip_netdev_ops, "ppe_veip") < 0) {
		nss_ppe_vlan_mgr_warn("%s: failed to register VEIP offload ops\n", dev->name);
		goto free_vp_structs;
	}

	/*
	 * Configure the source interface mapping
	 */
	v->is_vlan_as_veip_iface = true;
	v->port[vp_info.gw_vp_num - 1] = v->xlate_info.port_id = vp_info.gw_vp_num;
	v->port[vp_info.pon_vp_num - 1] = vp_info.pon_vp_num;

	/*
	 * Update the default port role to CORE PORT for both the base port as
	 * well the VLAN virtual port.
	 */
	if (vlan_mgr_ctx.port_role[v->port[0]] != FAL_QINQ_CORE_PORT) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[0], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[0]);
			goto free_vp_structs;
		}
		vlan_mgr_ctx.port_role[v->port[0]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}

	if (vlan_mgr_ctx.port_role[v->port[vp_info.gw_vp_num - 1]] != FAL_QINQ_CORE_PORT) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[vp_info.gw_vp_num - 1], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[vp_info.gw_vp_num - 1]);
			goto free_vp_structs;
		}
		vlan_mgr_ctx.port_role[v->port[vp_info.gw_vp_num - 1]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}

	if (vlan_mgr_ctx.port_role[v->port[vp_info.pon_vp_num - 1]] != FAL_QINQ_CORE_PORT) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[vp_info.pon_vp_num - 1], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[vp_info.pon_vp_num - 1]);
			goto free_vp_structs;
		}
		vlan_mgr_ctx.port_role[v->port[vp_info.pon_vp_num - 1]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}

	/*
	 * Calculate the cvid and svid
	 */
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) {
		v->ppe_cvid = v->vid;
		v->ppe_svid = v->parent->vid;
	} else {
		if (((vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) && (v->tpid == vlan_mgr_ctx.ctpid)) ||
				((vlan_mgr_ctx.ctpid == vlan_mgr_ctx.stpid) &&
				 (vlan_mgr_ctx.port_role[v->port[0]] == FAL_QINQ_EDGE_PORT))) {
			v->ppe_cvid = v->vid;
			v->ppe_svid = FAL_VLAN_INVALID;
		} else {
			v->ppe_cvid = FAL_VLAN_INVALID;
			v->ppe_svid = v->vid;
		}
	}

	v->xlate_info.br = NULL;
	v->xlate_info.svid = v->ppe_svid;
	v->xlate_info.cvid = v->ppe_cvid;

	ret = ppe_drv_vlan_as_veip_add_xlate_rules(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: failed to set vlan translation, error = %d \n", dev->name, ret);
		goto unregister_ol_ops;
	}

	return res;

unregister_ol_ops:
	netdev_hw_offload_ops_unregister(dev, &nss_ppe_vlan_mgr_veip_netdev_ops);
free_vp_structs:
	ppe_vp_veip_free_vps(v->iface);
clear_mtu:
	ppe_drv_iface_mtu_set(v->iface, 0);
clear_mac_addr:
	ppe_drv_iface_mac_addr_clear(v->iface);
free_ppe_iface:
	ppe_drv_veip_deinit(v->iface);
	ppe_drv_iface_deref(v->iface);
	v->iface = NULL;
	return -1;
}
#endif

/*
 * nss_ppe_vlan_mgr_configure_ppe()
 *	Configure PPE for non-bond devices
 */
static int nss_ppe_vlan_mgr_configure_ppe(struct nss_vlan_pvt *v, struct net_device *dev)
{
	int res = 0;
	struct net_device *base_dev, *lower_dev;
	ppe_drv_ret_t ret;
	struct list_head *iter;
	struct ppe_drv_iface *slave_iface;
	enum nss_ppe_vlan_mgr_vlan br_action = NSS_PPE_VLAN_MGR_BR_VLAN_INC;

	v->iface = ppe_drv_iface_alloc(PPE_DRV_IFACE_TYPE_VLAN, dev);
	if (!v->iface) {
		nss_ppe_vlan_mgr_warn("%s: failed to allocate IFACE for vlan device", dev->name);
		return -1;
	}
	/*
	 * PPE expects base_dev here. So, for eth0.10, base_dev will be eth0.
	 * For eth0.10.20, base_dev will be eth0.10.
	 * Not the actual real device is needed.
	 */
	base_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	if (!base_dev) {
		nss_ppe_vlan_mgr_warn("%s: failed to obtain base_dev", dev->name);
		goto free_iface;
	}

	ret = ppe_drv_vlan_init(v->iface, base_dev, v->vid, v->is_vlan_over_bridge);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: failed to initialize PPE, error = %d\n", dev->name, ret);
		goto free_iface;

	}

	/*
	 * Disable FDB learning based on module parameter configuration.
	 */
	if (vlan_fdb_learn_dis) {
		ret = ppe_drv_vlan_fdb_learn_disable(v->iface, true);
		if (ret != PPE_DRV_RET_SUCCESS) {
			nss_ppe_vlan_mgr_trace("%s: failed to configure VSI FDB learning, error = %d\n",
					dev->name, ret);
			goto deinit_iface;
		}
	}

	ret = ppe_drv_iface_mac_addr_set(v->iface, v->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: Failed to set MAC address, error = %d\n", dev->name, ret);
		goto deinit_iface;
	}

	ret = ppe_drv_iface_mtu_set(v->iface, v->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: Failed to set MTU, error = %d\n", dev->name, ret);
		goto clear_mac_addr;
	}

	if (v->is_vlan_over_bridge) {
		/*
		 * VLAN over bridge case
		 */
		netdev_for_each_lower_dev(v->br_net_dev, lower_dev, iter) {
			slave_iface = ppe_drv_iface_get_by_dev(lower_dev);
			if (slave_iface) {
				nss_ppe_vlan_mgr_trace("Installing ingress rule for %s master %s\n", lower_dev->name,
						       base_dev->name);
				if (!netif_is_bond_master(lower_dev)) {
					if (nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_add(slave_iface, v)) {
						nss_ppe_vlan_mgr_warn("Installing ingress xlate rule failed for "
								      "slave %s\n", lower_dev->name);
					}
				}
			} else {
				nss_ppe_vlan_mgr_warn("iface not found for %s", lower_dev->name);
			}
		}

		if (!vlan_mgr_ctx.vlan_over_bridge_cb(v->br_net_dev, br_action)) {
			nss_ppe_vlan_mgr_warn("Update to bridge mgr failed\n");
		}
		nss_ppe_vlan_mgr_trace("VLAN init success for VLAN over bridge case dev %p (%s) base %p (%s) res %d"
				       "vid %d br_net_dev %s\n", dev, dev->name, base_dev, base_dev->name, res, v->vid,
				       v->br_net_dev->name);

		return res;
	}

#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
	if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[0], FAL_QINQ_CORE_PORT)) {
		nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[0]);
		goto delete_ppe_rule;
	}
	vlan_mgr_ctx.port_role[v->port[0]] = FAL_QINQ_CORE_PORT;
#endif

	/*
	 * Calculate ppe cvid and svid
	 */
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) {
		v->ppe_cvid = v->vid;
		v->ppe_svid = v->parent->vid;
	} else {
		if (((vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) && (v->tpid == vlan_mgr_ctx.ctpid)) ||
		    ((vlan_mgr_ctx.ctpid == vlan_mgr_ctx.stpid) &&
		     (vlan_mgr_ctx.port_role[v->port[0]] == FAL_QINQ_EDGE_PORT))) {
			v->ppe_cvid = v->vid;
			v->ppe_svid = FAL_VLAN_INVALID;
		} else {
			v->ppe_cvid = FAL_VLAN_INVALID;
			v->ppe_svid = v->vid;
		}
	}

	v->xlate_info.port_id = v->port[0];
	v->xlate_info.br = NULL;
	v->xlate_info.svid = v->ppe_svid;
	v->xlate_info.cvid = v->ppe_cvid;

	ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: failed to set vlan translation, error = %d \n", dev->name, ret);
		goto clear_mac_addr;
	}

#ifndef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
	if ((v->ppe_svid != FAL_VLAN_INVALID) && (vlan_mgr_ctx.port_role[v->port[0]] != FAL_QINQ_CORE_PORT)) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[0], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[0]);
			goto delete_ppe_rule;
		}
		vlan_mgr_ctx.port_role[v->port[0]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}
#endif

	return res;

delete_ppe_rule:
	ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: failed to delete vlan translation, error = %d \n", dev->name, ret);
	}

clear_mac_addr:
	ppe_drv_iface_mac_addr_clear(v->iface);

deinit_iface:
	ppe_drv_vlan_deinit(v->iface);

free_iface:
	ppe_drv_iface_deref(v->iface);
	v->iface = NULL;
	return -1;
}

/*
 * nss_ppe_vlan_mgr_instance_free()
 *	Destroy vlan instance
 */
static void nss_ppe_vlan_mgr_instance_free(struct kref *kref)
{
	int32_t i;
	ppe_drv_ret_t ret;
	struct net_device *lower_dev;
	struct net_device *slave_dev;
	struct ppe_drv_iface *slave_iface;
	struct list_head *iter;
#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
	struct net_device *dev;
#endif
	enum nss_ppe_vlan_mgr_vlan br_action = NSS_PPE_VLAN_MGR_BR_VLAN_DEC;
	struct nss_vlan_pvt *v = container_of(kref, struct nss_vlan_pvt, ref);
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	uint32_t dsa_vid = 0;
#endif

	if (v->is_vlan_over_bridge) {
		if (!v->iface) {
			nss_ppe_vlan_mgr_minidump_free(v, "nss_vlan_pvt");
			kfree(v);
			return;
		}
		netdev_for_each_lower_dev(v->br_net_dev, lower_dev, iter) {
			slave_iface = ppe_drv_iface_get_by_dev(lower_dev);
			if (slave_iface) {
				nss_ppe_vlan_mgr_trace("Deleting ingress rule for slave %s master %s\n",
						lower_dev->name, v->br_net_dev->name);
				if (!netif_is_bond_master(lower_dev)) {
					if (nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_del(slave_iface, v)) {
						nss_ppe_vlan_mgr_warn("Deleting ingress xlate rule failed for "
								"slave %s\n", lower_dev->name);
					}
				}
			} else {
				nss_ppe_vlan_mgr_warn("iface not found for %s", lower_dev->name);
			}
		}
		if (!vlan_mgr_ctx.vlan_over_bridge_cb(v->br_net_dev, br_action)) {
			nss_ppe_vlan_mgr_warn("Update to bridge mgr failed\n");
		}
	}

	spin_lock(&vlan_mgr_ctx.lock);
	if (!list_empty(&v->list)) {
		list_del(&v->list);
	}
	spin_unlock(&vlan_mgr_ctx.lock);

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	/*
	 * Clear the egress VLAN-to-VP mapping that was set when this VLAN
	 * over DSA interface was created.
	 */
	if (v->parent) {
		if ((v->parent->tpid == ETH_P_DSA_8021Q) ||
				(v->parent->parent && v->parent->parent->tpid == ETH_P_DSA_8021Q)) {
			dsa_vid = (v->parent->tpid == ETH_P_DSA_8021Q) ?
				v->parent->vid : v->parent->parent->vid;
			ppe_drv_vlan_dsa_vp_egress_map_reset(v->xlate_info.port_id, dsa_vid);
		}
	}
#endif

	/*
	 * VP based vlan instance cleanup.
	 * VP free will cleanup the iface associated with vp.
	 */
	if (v->is_vlan_as_vp_iface) {
		nss_ppe_vlan_mgr_trace("Vlan as VP interface: %d\n", v->xlate_info.port_id);
		if (v->parent) {
			nss_ppe_vlan_mgr_instance_deref(v->parent);
		}

#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
		if (v->xlate_ctx) {
			ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach(v->xlate_ctx->ptec, v->xlate_info.port_id);
			nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx_put(v->xlate_ctx);
		}
#endif
		nss_ppe_vlan_mgr_deconfigure_vp(v);
		nss_ppe_vlan_mgr_free_vp(v->xlate_info.port_id);
		nss_ppe_vlan_mgr_minidump_free(v, "nss_vlan_pvt");
		kfree(v);
		return;
	}

#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
	/*
	 * VEIP based vlan instance cleanup.
	 */
	if (v->is_vlan_as_veip_iface && v->iface) {
		nss_ppe_vlan_mgr_trace("Vlan as VEIP interface: %d\n", v->xlate_info.port_id);
		if (v->parent) {
			nss_ppe_vlan_mgr_instance_deref(v->parent);
		}

		dev = dev_get_by_index(&init_net, v->ifindex);
		if (dev) {
			netdev_hw_offload_ops_unregister(dev, &nss_ppe_vlan_mgr_veip_netdev_ops);
			dev_put(dev);
		}

		ret = ppe_drv_vlan_as_veip_del_xlate_rules(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			nss_ppe_vlan_mgr_warn("%p: failed to delete vlan translation, error = %d \n", v, ret);
		}
		v->xlate_info.port_id = 0;

		if (v->iface) {
			/*
			 * Free VP structures
			 */
			ppe_vp_veip_free_vps(v->iface);

			/*
			 * Clear MAC addresses (both port-level and L3_IF)
			 */
			ppe_drv_iface_mac_addr_clear(v->iface);

			/*
			 * Free PPE driver resources
			 */
			ppe_drv_veip_deinit(v->iface);
			ppe_drv_iface_deref(v->iface);
			v->iface = NULL;
		}

		nss_ppe_vlan_mgr_minidump_free(v, "nss_vlan_pvt");
		kfree(v);
		return;
	}
#endif

	/*
	 * VSI based vlan instance cleanup.
	 */
	if (v->iface) {

		v->xlate_info.port_id = 0;
		for (i = 0; i < NSS_PPE_VLAN_MGR_PORT_MAX; i++) {
			if (!v->port[i]) {
				continue;
			}

			v->xlate_info.port_id = v->port[i];
			ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
			if (ret != PPE_DRV_RET_SUCCESS) {
				nss_ppe_vlan_mgr_warn("%p: failed to delete vlan translation, error = %d \n", v, ret);
			}
			v->xlate_info.port_id = 0;

			/*
			 * When vlan interface over bond is getting freed each associated slave interface
			 * should be also be detached.
			 */
			if (v->bond_id != -1) {
				slave_dev = ppe_drv_port_num_to_dev(v->port[i]);
				if (!slave_dev)
					continue;

				ret = ppe_drv_vlan_lag_slave_leave(v->iface, slave_dev);
				if (ret != PPE_DRV_RET_SUCCESS) {
					nss_ppe_vlan_mgr_warn("%p: failed to detach slave from bond vlan iface %s \n", v, slave_dev->name);
				}
			}
		}

		ppe_drv_iface_mac_addr_clear(v->iface);
	}

	/*
	 * Need to change the port role. While adding
	 * eth0.10.20/bond0.10.20, the role of the port(s) changed
	 * from EDGE to CORE. So, while removing eth0.10.20/bond0.10.20, the
	 * role of the port(s) should be changed from CORE to EDGE.
	 */
	for (i = 0; i < NSS_PPE_VLAN_MGR_PORT_MAX; i++) {
		if (v->port[i]) {
			if (nss_ppe_vlan_mgr_calculate_new_port_role(v->iface, v->port[i], i)) {
				nss_ppe_vlan_mgr_port_role_event(v->port[i], i);
			}
		}
	}

	if (v->parent) {
		nss_ppe_vlan_mgr_instance_deref(v->parent);
	}

	if (v->iface) {
		ppe_drv_vlan_deinit(v->iface);
		ppe_drv_iface_deref(v->iface);
		v->iface = NULL;
	}
	nss_ppe_vlan_mgr_minidump_free(v, "nss_vlan_pvt");
	kfree(v);
}

/*
 * nss_ppe_vlan_mgr_instance_deref()
 *	Decreases the references of vlan_pvt instance.
 */
static bool nss_ppe_vlan_mgr_instance_deref(struct nss_vlan_pvt *v)
{
	if (kref_put(&v->ref, nss_ppe_vlan_mgr_instance_free)) {
		nss_ppe_vlan_mgr_trace("%p: reference count is 0 for vlan ID: %u", v, v->vid);
		return true;
	}

	nss_ppe_vlan_mgr_trace("%p: reference count is %d for vlan ID: %u", v, kref_read(&v->ref), v->vid);
	return false;
}

/*
 * nss_ppe_vlan_mgr_interface_supported()
 *	Checks VLAN interface is supported
 */
static bool nss_ppe_vlan_mgr_interface_supported(struct net_device *dev)
{
	bool ret = true;
	struct nss_vlan_pvt *v;
	int32_t port_id;
	struct vlan_dev_priv *vlan;
	struct ppe_drv_iface *real_iface;
	struct net_device *lower_dev, *real_dev, *master_dev;
	struct list_head *iter;
	int vid;

	if (!is_vlan_dev(dev)) {
		nss_ppe_vlan_mgr_trace("%s is not VLAN interface\n", dev->name);
		return false;
	}

	vlan = vlan_dev_priv(dev);
	real_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	vid = vlan->vlan_id;

	if (!real_dev) {
		nss_ppe_vlan_mgr_trace("%s unable to get the real_dev\n", dev->name);
		return false;
	}

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	if (nss_ppe_vlan_mgr_dsa_interface_supported(dev)) {
		nss_ppe_vlan_mgr_trace("%s is supported as VLAN interface\n", dev->name);
		return true;
	}
#endif

	/*
	 * br-wan1.100 is already present and br-wan1 contains eth4.
	 * With above config, creating eth4.100 is not allowed.
	 * While trying to create eth4.100, in below code
	 * 1) Find the master dev of eth4
	 * 2) Iterate for all VLANs present in VLAN manager and check for below condns
	 * a) VLAN is created over bridge
	 * b) Bridge of bridge VLAN netdev matches with masterdev
	 * c) VID of bridge VLAN netdev is same as new physical VLAN (eth4.100)
	 * if above condns are satified, then VLAN interface is not supported and set ret as false
	 */
	spin_lock(&vlan_mgr_ctx.lock);
	master_dev = netdev_master_upper_dev_get(real_dev);
	if (master_dev) {
		nss_ppe_vlan_mgr_trace("Master dev %s real dev %s VLAN %s\n", master_dev->name,
				real_dev->name, dev->name);
		list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
			nss_ppe_vlan_mgr_trace("%px Iterating for VLAN interfaces vid %d vlan_over_bridge %d "
					" bridge name %s\n", v, v->vid, v->is_vlan_over_bridge,
					v->br_net_dev->name);
			if ((v->is_vlan_over_bridge) && (v->br_net_dev == master_dev) && (vid == v->vid)) {
				nss_ppe_vlan_mgr_trace("VLAN %s on %s is not supported\n", dev->name,
						real_dev->name);
				ret = false;
				goto result;
			}
		}
	}

	if (is_vlan_dev(real_dev)) {
		/*
		 * Changing the real dev for double VLAN case in VLAN over bridge scenario
		 * For instance, creating br-wan1.100.200 over br-wan1.100
		 */
		real_iface = ppe_drv_iface_get_by_dev(real_dev);

		/*
		 * If PPE representation is not present for br-wan1.100, the double VLAN is not supported
		 */
		if (!real_iface) {
			nss_ppe_vlan_mgr_warn("iface not present for %s double VLAN %s\n", real_dev->name, dev->name);
			ret = false;
			goto result;
		}

		real_dev = nss_ppe_vlan_mgr_get_real_dev(real_dev);
		if (!real_dev) {
			nss_ppe_vlan_mgr_trace("%s unable to get the real_dev for double VLAN\n", dev->name);
			return false;
		}

		nss_ppe_vlan_mgr_trace("Double VLAN case and updated real dev as %s for dev %s", real_dev->name,
				       dev->name);
	}

	/*
	 * eth4.100 is already present and br-wan1 contains eth4
	 * With above config, creating br-wan1.100 is not allowed.
	 * While trying to create br-wan1.100,
	 * 1) If real dev of VLAN is bridge master
	 * Loop for lower devs present in the bridge and derive port ID of lower devs
	 * 2) Iterate for all VLANs present in the VLAN manager and check for below condns
	 * a) Port ID VLAN is same as lower dev port ID
	 * b) VID of VLAN present in VLAN manager is same as the VLAN ID of new VLAN
	 * if above condns are satified, then VLAN interface is not supported and set ret as false
	 */
	if (netif_is_bridge_master(real_dev)) {
		/*
		 * If PPE representation is not present for br-wan1, then not allowing to create br-wan1.100
		 */
		nss_ppe_vlan_mgr_trace("VLAN %s Real %s vid %d bond real %d\n", dev->name, real_dev->name, vid,
				       netif_is_bond_master(real_dev));
		real_iface = ppe_drv_iface_get_by_dev(real_dev);

		if (!real_iface) {
			nss_ppe_vlan_mgr_warn("%px: iface not present for %s\n", real_dev, real_dev->name);
			ret = false;
			goto result;
		}

		netdev_for_each_lower_dev(real_dev, lower_dev, iter) {
			port_id = nss_ppe_vlan_mgr_get_port_id(lower_dev);
			/*
			 * If Port ID is invalid, continue for other slaves in the bridge
			 */
			if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
				nss_ppe_vlan_mgr_warn("Port_id is invalid for %s", lower_dev->name);
				continue;
			}
			/*
			 * If the slaves of the master contains lag (bond0), then bridge VLAN netdev (br-wan1.100)
			 * is not supported
			 */
			nss_ppe_vlan_mgr_trace("lower dev %s port_id %d bond flag %d\n", lower_dev->name, port_id,
					       netif_is_bond_master(lower_dev));
			if (netif_is_bond_master(lower_dev)) {
				nss_ppe_vlan_mgr_warn("Slave of %s contains bond %s hence VLAN over bridge %s in PPE is"
						      " not supported", real_dev->name, lower_dev->name, dev->name);
				ret = false;
				goto result;
			}

			list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
				nss_ppe_vlan_mgr_trace("Iterating v->port[0] %d v->vid %d\n", v->port[0], v->vid);
				if ((v->port[0] == port_id) && (vid == v->vid)) {
					ret = false;
					break;
				}
			}
		}
	}

result:
	if (nss_ppe_vlan_mgr_is_wlan_dev(real_dev) && !vlan_mgr_ctx.wlan_dst_xlate_en) {
		ret = false;
		nss_ppe_vlan_mgr_warn("WLAN VLAN VP support not enabled: real_dev %s\n", real_dev->name);
	}

	spin_unlock(&vlan_mgr_ctx.lock);
	nss_ppe_vlan_mgr_trace("VLAN(%s) on real dev %s with vid %d is %s\n", dev->name, real_dev->name, vid,
			       ret ? "supported" : "not supported");
	return ret;
}

/*
 * nss_ppe_vlan_mgr_instance_find_and_ref()
 *	Increases the references of vlan_pvt instance.
 */
static struct nss_vlan_pvt *nss_ppe_vlan_mgr_instance_find_and_ref(
						struct net_device *dev)
{
	struct nss_vlan_pvt *v;

	if (!is_vlan_dev(dev)) {
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
		if (!dsa_slave_dev_check(dev))
#endif
			return NULL;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if (v->ifindex == dev->ifindex) {
			kref_get(&v->ref);
			spin_unlock(&vlan_mgr_ctx.lock);
			return v;
		}
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	return NULL;
}

/*
 * nss_ppe_vlan_mgr_create_instance()
 *	Create vlan instance
 */
static struct nss_vlan_pvt *nss_ppe_vlan_mgr_create_instance(struct net_device *dev)
{
	struct nss_vlan_pvt *v;
	struct vlan_dev_priv *vlan;
	struct net_device *real_dev;
	struct net_device *slave_dev;
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	struct net_device *base_dev;
#endif
	int32_t port_id, bond_id = -1;

	if (!is_vlan_dev(dev)) {
		return NULL;
	}

	v = kzalloc(sizeof(*v), GFP_KERNEL);
	if (!v) {
		nss_ppe_vlan_mgr_warn("%px: Allocation to private structure failed: %s\n",
						dev, dev->name);
		return NULL;
	}

	INIT_LIST_HEAD(&v->list);

	vlan = vlan_dev_priv(dev);
	real_dev = vlan->real_dev;
	v->vid = vlan->vlan_id;
	v->tpid = ntohs(vlan->vlan_proto);
	v->bond_id = -1;

	/*
	 * Check if the vlan has any parent.
	 *
	 * 1. While adding eth0.10/bond0.10, the real_dev will become
	 * eth0/bond0. In this case, v->parent should be NULL and respective
	 * port numbers will be assigned to v->port.
	 *
	 * 2. While adding eth0.10.20/bond0.10.20, the real_dev will
	 * become eth0.10/bond0.10, so v->parent should be valid. But v->parent->parent
	 * should be NULL, as explained above. In this case, we need to copy the
	 * v->parent->port numbers to v->ports as the double vlan is created
	 * on the same port(s).
	 *
	 * 3. We ignore the remaining case as we support only 2 vlan tags.
	 *
	 * TODO: Consider creating inline APIs for below code.
	 */
	v->parent = nss_ppe_vlan_mgr_instance_find_and_ref(real_dev);
	if (!v->parent) {
		if (netif_is_bridge_master(real_dev)) {
			nss_ppe_vlan_mgr_trace("VLAN over bridge dev %p (%s) real_dev %p (%s)\n", dev, dev->name,
					       real_dev, real_dev->name);
			goto vlan_over_bridge;
		} else if (!netif_is_bond_master(real_dev)) {
			v->port[0] = nss_ppe_vlan_mgr_get_port_id(real_dev);

			if (v->port[0] == NSS_PPE_VLAN_MGR_INVALID_PORT) {
				nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", real_dev, real_dev->name, v->port[0]);
				kfree(v);
				return NULL;
			}
		} else {
#if defined(BONDING_SUPPORT)
			if (vlan_mgr_ctx.bond_id_get_cb) {
				bond_id = vlan_mgr_ctx.bond_id_get_cb(real_dev);
			}
#endif
			if (bond_id < 0) {
				nss_ppe_vlan_mgr_warn("%px: Invalid LAG group id 0x%x\n", v, bond_id);
				kfree(v);
				return NULL;
			}

			rcu_read_lock();
			for_each_netdev_in_bond_rcu(real_dev, slave_dev) {
				port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
				if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
					rcu_read_unlock();
					nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n",
									slave_dev, slave_dev->name, port_id);
					kfree(v);
					return NULL;
				}
				v->port[port_id - 1] = port_id;
			}
			rcu_read_unlock();
			v->bond_id = bond_id;
		}
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	} else if (v->parent && v->parent->parent) {
		/*
		 * Triple or deeper VLAN stacking (e.g. lan1.10.20.30) is not supported.
		 * Only double VLAN over DSA interfaces is supported at this depth.
		 */
		if (v->parent->parent->parent) {
			nss_ppe_vlan_mgr_warn("%s: triple VLAN stacking not supported\n", dev->name);
			nss_ppe_vlan_mgr_instance_deref(v->parent);
			kfree(v);
			return NULL;
		}

		/*
		 * lan1.10.20: double user VLAN over DSA (QinQ over DSA).
		 * real_dev is lan1.10 here; peel one level to reach lan1 (the DSA slave).
		 */
		base_dev = real_dev;
		if (is_vlan_dev(base_dev)) {
			vlan = vlan_dev_priv(base_dev);
			base_dev = vlan->real_dev;
		}

		if (!nss_ppe_vlan_mgr_is_htt_vlan_dsa(base_dev)) {
			nss_ppe_vlan_mgr_warn("%s: QinQ (double user VLAN) only supported on VLAN-based DSA interfaces\n",
					dev->name);
			nss_ppe_vlan_mgr_instance_deref(v->parent);
			kfree(v);
			return NULL;
		}

		v->port[0] = v->parent->port[0];
#endif
	} else if (v->parent) {
		if (is_vlan_dev(real_dev)) {
			vlan = vlan_dev_priv(real_dev);
			real_dev = vlan->real_dev;
		}
		if (!netif_is_bond_master(real_dev)) {
			v->port[0] = v->parent->port[0];
		} else {
			rcu_read_lock();
			for_each_netdev_in_bond_rcu(real_dev, slave_dev) {
				port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
				if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
					rcu_read_unlock();
					nss_ppe_vlan_mgr_instance_deref(v->parent);
					nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n",
									slave_dev, slave_dev->name, port_id);
					kfree(v);
					return NULL;
				}

				v->port[port_id - 1] = v->parent->port[port_id - 1];
			}
			rcu_read_unlock();
			v->bond_id = v->parent->bond_id;
		}
	} else {
		nss_ppe_vlan_mgr_warn("%s: don't support more than 2 vlans\n", dev->name);
		nss_ppe_vlan_mgr_instance_deref(v->parent);
		kfree(v);
		return NULL;
	}

	/*
	 * Check if TPID is permited
	 */
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	/*
	 * For QinQ over DSA (lan1.10.20):
	 *   v->tpid        = C-tag TPID (innermost user VLAN, lan1.10.20) — must match ctpid
	 *   v->parent->tpid = S-tag TPID (outer user VLAN, lan1.10)      — must match stpid
	 * The grandparent (lan1 DSA VP) uses a DSA-internal TPID; do not validate it here.
	 */
	if ((NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_TRIPLE) &&
	    ((v->tpid != vlan_mgr_ctx.ctpid) || (v->parent->tpid != vlan_mgr_ctx.stpid))) {
		nss_ppe_vlan_mgr_warn("%s: QinQ over DSA: tpid %04x/%04x not match global ctpid/stpid (%04x/%04x)\n",
				dev->name, v->tpid, v->parent->tpid,
				vlan_mgr_ctx.ctpid, vlan_mgr_ctx.stpid);
		nss_ppe_vlan_mgr_instance_deref(v->parent);
		kfree(v);
		return NULL;
	}
#endif
	if (NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) {
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
		/*
		 * For VLAN over DSA (lan2.20): accept ctpid or stpid for the user
		 * tag. The base DSA tag TPID is ETH_P_DSA_8021Q and is stripped
		 * by PPE HW before the xlate lookup.
		 */
		if (nss_ppe_vlan_mgr_is_htt_vlan_dsa(real_dev)) {
			if (((v->tpid != vlan_mgr_ctx.ctpid) && (v->tpid != vlan_mgr_ctx.stpid)) ||
					(v->parent->tpid != ETH_P_DSA_8021Q)) {
				nss_ppe_vlan_mgr_warn("%s: double tag over DSA: tpid %04x/%04x not match ctpid/stpid/dsa_tpid (%04x/%04x/%04x)\n",
						dev->name, v->tpid, v->parent->tpid,
						vlan_mgr_ctx.ctpid, vlan_mgr_ctx.stpid, ETH_P_DSA_8021Q);
				nss_ppe_vlan_mgr_instance_deref(v->parent);
				kfree(v);
				return NULL;
			}
		} else
#endif
		{
			/*
			 * Normal double tag: inner must match ctpid, outer must match stpid.
			 */
			if ((v->tpid != vlan_mgr_ctx.ctpid) || (v->parent->tpid != vlan_mgr_ctx.stpid)) {
				nss_ppe_vlan_mgr_warn("%s: double tag: tpid %04x/%04x not match global ctpid/stpid (%04x/%04x)\n",
						dev->name, v->tpid, v->parent->tpid,
						vlan_mgr_ctx.ctpid, vlan_mgr_ctx.stpid);
				nss_ppe_vlan_mgr_instance_deref(v->parent);
				kfree(v);
				return NULL;
			}
		}
	}

	if ((NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_SINGLE) &&
	    ((v->tpid != vlan_mgr_ctx.ctpid) && (v->tpid != vlan_mgr_ctx.stpid))) {
		nss_ppe_vlan_mgr_warn("%s: single tag: tpid %04x not match global tpid(%04x, %04x)\n", dev->name, v->tpid, vlan_mgr_ctx.ctpid, vlan_mgr_ctx.stpid);
		kfree(v);
		return NULL;
	}

vlan_over_bridge:
	nss_ppe_vlan_mgr_minidump_log(v, sizeof(*v), "nss_vlan_pvt");
	v->mtu = dev->mtu;
	ether_addr_copy(v->dev_addr, dev->dev_addr);
	v->ifindex = dev->ifindex;
	kref_init(&v->ref);
	return v;
}

/*
 * nss_ppe_vlan_mgr_changemtu_event()
 */
int nss_ppe_vlan_mgr_changemtu_event(struct netdev_notifier_info *info)
{
	ppe_drv_ret_t ret;
	struct net_device *dev = netdev_notifier_info_to_dev(info);
	struct nss_vlan_pvt *v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);
	uint32_t old_mtu;

	if (!v) {
		return NOTIFY_DONE;
	}

	old_mtu = v->mtu;

	spin_lock(&vlan_mgr_ctx.lock);
	if (v->mtu == dev->mtu) {
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_instance_deref(v);
		return NOTIFY_DONE;
	}

	v->mtu = dev->mtu;
	spin_unlock(&vlan_mgr_ctx.lock);
	ret = ppe_drv_iface_mtu_set(v->iface, dev->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_lock(&vlan_mgr_ctx.lock);
		v->mtu = old_mtu;
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_warn("%s: Failed to change MTU(%d) in PPE, error = %d\n", dev->name, dev->mtu, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return NOTIFY_BAD;
	}

	nss_ppe_vlan_mgr_trace("%s: MTU changed to %d, PPE updated\n", dev->name, dev->mtu);
	nss_ppe_vlan_mgr_instance_deref(v);
	return NOTIFY_DONE;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_changemtu_event);

/*
 * int nss_ppe_vlan_mgr_changeaddr_event()
 */
int nss_ppe_vlan_mgr_changeaddr_event(struct netdev_notifier_info *info)
{
	ppe_drv_ret_t ret;
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	struct nss_vlan_pvt *v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);
	if (!v) {
		nss_ppe_vlan_mgr_warn("%px: Interface not found name: %s\n",
						dev, dev->name);
		return NOTIFY_DONE;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	if (!memcmp(v->dev_addr, dev->dev_addr, ETH_ALEN)) {
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_instance_deref(v);
		return NOTIFY_DONE;
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	ret = ppe_drv_iface_mac_addr_clear(v->iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: Failed to clear MAC address, error = %d\n", dev->name, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return NOTIFY_BAD;
	}

	ret = ppe_drv_iface_mac_addr_set(v->iface, (uint8_t *)dev->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%s: Failed to change MAC address, error = %d\n", dev->name, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return NOTIFY_BAD;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	ether_addr_copy(v->dev_addr, dev->dev_addr);
	spin_unlock(&vlan_mgr_ctx.lock);
	nss_ppe_vlan_mgr_trace("%s: MAC changed to %pM, updated PPE\n", dev->name, dev->dev_addr);
	nss_ppe_vlan_mgr_instance_deref(v);
	return NOTIFY_DONE;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_changeaddr_event);

/*
 * nss_ppe_vlan_mgr_register_event()
 */
static int nss_ppe_vlan_mgr_register_event(struct netdev_notifier_info *info)
{
	struct net_device *dev = netdev_notifier_info_to_dev(info);
	struct nss_vlan_pvt *v;
	int res;
	struct net_device *slave_dev, *real_dev;
	int32_t port_id;
	struct vlan_dev_priv *vlan;
	bool is_bond_master = false;
	bool is_vlan_as_vp = false;
	int i = -1;
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	uint32_t dsa_vid = 0;
#endif

	if (!nss_ppe_vlan_mgr_interface_supported(dev)) {
		nss_ppe_vlan_mgr_warn("VLAN interface (%s) is not supported\n", dev->name);
		return NOTIFY_DONE;
	}

	v = nss_ppe_vlan_mgr_create_instance(dev);
	if (!v) {
		nss_ppe_vlan_mgr_warn("Vlan instance creation failed for dev:%s\n", dev->name);
		return NOTIFY_DONE;
	}

	vlan = vlan_dev_priv(dev);
	real_dev = vlan->real_dev;
	if (is_vlan_dev(real_dev)) {
		vlan = vlan_dev_priv(real_dev);
		real_dev = vlan->real_dev;
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
		/*
		 * For lan1.10.20, real_dev is still lan1 (a DSA slave/VP) after two peels.
		 * Peel one more level to reach ethx (the DSA master / PPE physical port)
		 * so VP allocation uses a valid physical port.
		 */
		if (is_vlan_dev(real_dev)) {
			vlan = vlan_dev_priv(real_dev);
			real_dev = vlan->real_dev;
		}
#endif
	}

	/*
	 * Check if VLAN as VP is enabled on selected VLAN interface real_dev.
	 * If vlan_as_vp_interface module param contains empty string,
	 * then VLAN as VP mode is not enabled.
	 *
	 * The 'vlan_as_vp_invert' parameter controls how the 'vlan_as_vp_dev_name'
	 * netdevices array should be interpreted.
	 * If the 'vlan_as_vp_invert' variable is false, then the 'vlan_as_vp_dev_name'
	 * array has the list of devices over which the VLAN as VP feature is required.
	 * But, if the 'vlan_as_vp_invert' variable is true, then 'vlan_as_vp_dev_name'
	 * array has the list of devices over which the VLAN as VP feature is not required.
	 *
	 * 'vlan_as_vp_dev_name' is filled in 2 ways. One from the module params, and
	 * another when DSA interfaces are created. It's done to support VLAN-as-VP
	 * for VLAN on DSA interfaces (eg: lan1.10). Also, this array can have holes
	 * due to creation/deletion sequences of DSA interfaces.
	 *
	 */
	for (i = 0; i < NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX; i++) {
		nss_ppe_vlan_mgr_warn("Matching <%s> in array[%d]=<%s>\n", real_dev->name, i, vlan_as_vp_dev_name[i]);
		if (!strncmp(real_dev->name, vlan_as_vp_dev_name[i], IFNAMSIZ)) {
			break;
		}
	}

	if ((i < 0) || (i == NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX)) {
		/*
		 * Case when the 'vlan_as_vp_interface' device array module param is empty or
		 * real device not found in the 'vlan_as_vp_interface' device array.
		 */
		if (vlan_as_vp_invert) {
			nss_ppe_vlan_mgr_info("is_vlan_as vp is true for %s dev\n", real_dev->name);
			is_vlan_as_vp = true;
		}
	} else {
		/*
		 * Case when real device found in the 'vlan_as_vp_interface' device array
		 */
		if (!vlan_as_vp_invert) {
			nss_ppe_vlan_mgr_info("is_vlan_as vp is true for %s dev\n", real_dev->name);
			is_vlan_as_vp = true;
		}
	}

	is_bond_master = netif_is_bond_master(real_dev);

	if (netif_is_bridge_master(real_dev)) {
		v->is_vlan_over_bridge = true;
		v->br_net_dev = real_dev;
		nss_ppe_vlan_mgr_trace("Updated br_net for bridge VLAN netdev %s base %s\n", dev->name, real_dev->name);
	}

	if (!is_bond_master) {
		if (is_vlan_as_vp) {
			res = nss_ppe_vlan_mgr_alloc_configure_ppe_vp(v, dev, real_dev);
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
			/*
			 * For VLAN over a DSA slave (e.g. lan3.10, lan3.10.20), set the
			 * egress VP-to-VLAN mapping using the base DSA interface's VID.
			 */
			if (res >= 0 && v->parent && nss_ppe_vlan_mgr_is_htt_vlan_dsa(real_dev)) {
				dsa_vid = (v->parent->tpid == ETH_P_DSA_8021Q) ?
					v->parent->vid : v->parent->parent->vid;
				if (ppe_drv_vlan_dsa_vp_egress_map_set(v->xlate_info.port_id, dsa_vid) < 0) {
					nss_ppe_vlan_mgr_warn("%s: failed to set DSA VP egress map\n", dev->name);
					nss_ppe_vlan_mgr_instance_deref(v);
					return NOTIFY_DONE;
				}
			}
#endif

#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
		} else if (vlan_as_veip_enabled &&
			   ((port_id = nss_ppe_vlan_mgr_get_port_id(real_dev)) != NSS_PPE_VLAN_MGR_INVALID_PORT) &&
			   ppe_drv_port_is_gem(port_id)) {
			res = nss_ppe_vlan_mgr_alloc_configure_ppe_veip(v, dev);
#endif
		} else {
			res = nss_ppe_vlan_mgr_configure_ppe(v, dev);
		}
	} else {
		res = nss_ppe_vlan_mgr_bond_configure_ppe(v, real_dev, dev);
	}

	if (res < 0) {
		nss_ppe_vlan_mgr_instance_deref(v);
		return NOTIFY_DONE;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_add(&v->list, &vlan_mgr_ctx.list);
	spin_unlock(&vlan_mgr_ctx.lock);

	if (res != NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED) {
		return NOTIFY_DONE;
	}

	if (!is_bond_master) {
		nss_ppe_vlan_mgr_port_role_event(v->port[0], 0);
		return NOTIFY_DONE;
	}

	rcu_read_lock();
	for_each_netdev_in_bond_rcu(real_dev, slave_dev) {
		port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_instance_deref(v);
			nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", slave_dev, slave_dev->name, port_id);
			return -1;
		}

		nss_ppe_vlan_mgr_port_role_event(v->port[port_id - 1], port_id - 1);
	}

	rcu_read_unlock();
	return NOTIFY_DONE;
}

/*
 * nss_ppe_vlan_mgr_unregister_event()
 */
static int nss_ppe_vlan_mgr_unregister_event(struct netdev_notifier_info *info)
{
	struct net_device *dev = netdev_notifier_info_to_dev(info);
	struct nss_vlan_pvt *v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);

	/*
	 * Do we have it on record?
	 */
	if (!v) {
		return NOTIFY_DONE;
	}

	/*
	 * Release reference got by "nss_ppe_vlan_mgr_instance_find_and_ref"
	 */
	nss_ppe_vlan_mgr_trace("Vlan %s unregistered.\n", dev->name);
	nss_ppe_vlan_mgr_instance_deref(v);

	/*
	 * Release reference got by "nss_ppe_vlan_mgr_create_instance"
	 */
	nss_ppe_vlan_mgr_instance_deref(v);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_vlan_mgr_netdevice_event()
 */
static int nss_ppe_vlan_mgr_netdevice_event(struct notifier_block *unused,
				unsigned long event, void *ptr)
{
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;

	switch (event) {
	case NETDEV_CHANGEADDR:
		return nss_ppe_vlan_mgr_changeaddr_event(info);
	case NETDEV_CHANGEMTU:
		return nss_ppe_vlan_mgr_changemtu_event(info);
	case NETDEV_REGISTER:
		return nss_ppe_vlan_mgr_register_event(info);
	case NETDEV_UNREGISTER:
		return nss_ppe_vlan_mgr_unregister_event(info);
	}

	/*
	 * Notify done for all the events we don't care
	 */
	return NOTIFY_DONE;
}

static struct notifier_block nss_ppe_vlan_mgr_netdevice_nb __read_mostly = {
	.notifier_call = nss_ppe_vlan_mgr_netdevice_event,
};

/*
 * nss_ppe_vlan_mgr_over_bond_leave_bridge()
 *	Leave bond interface from bridge
 */
static int nss_ppe_vlan_mgr_over_bond_leave_bridge(struct net_device *real_dev, struct nss_vlan_pvt *v)
{
	struct net_device *slave_dev;
	struct ppe_drv_iface *bridge_iface = v->bridge_iface;
	int32_t port_id;
	ppe_drv_ret_t ret;

	rcu_read_lock();
	for_each_netdev_in_bond_rcu(real_dev, slave_dev) {
		port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", slave_dev, slave_dev->name, port_id);
			return -1;
		}

		if (v->port[port_id - 1] != port_id) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s: Given port (%d) is not in this VLAN \n", slave_dev, slave_dev->name, port_id);
			return -1;
		}

		v->xlate_info.br = bridge_iface;
		v->xlate_info.port_id = port_id;
		ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: Failed to delete old translation rule for port: %d, error = %d\n", v, port_id, ret);
			return -1;
		}

		v->xlate_info.br = NULL;
		ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			v->xlate_info.br = bridge_iface;
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: Failed to add new translation rule for port: %d, error = %d\n", v, port_id, ret);
			return -1;
		}

		/*
		 * Set port STP state to forwarding after bond interfaces leave bridge
		 */
		ret = ppe_drv_br_stp_state_set(bridge_iface, slave_dev, FAL_STP_FORWARDING);
		if (ret != PPE_DRV_RET_SUCCESS) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: failed to set STP state to FORWARDING %s, error = %d\n", v, real_dev->name, ret);
			return -1;
		}

	}

	v->xlate_info.br = NULL;
	v->bridge_iface = NULL;
	rcu_read_unlock();
	return 0;
}

/*
 * nss_ppe_vlan_mgr_over_bond_join_bridge()
 *	Join bond interface to bridge
 */
static int nss_ppe_vlan_mgr_over_bond_join_bridge(struct nss_vlan_pvt *v, struct net_device *real_dev, struct ppe_drv_iface *bridge_iface)
{
	int32_t port_id;
	struct net_device *slave_dev;
	ppe_drv_ret_t ret;

	v->xlate_info.port_id = 0;

	rcu_read_lock();
	for_each_netdev_in_bond_rcu(real_dev, slave_dev) {
		port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
		if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", slave_dev, slave_dev->name, port_id);
			return -1;
		}

		if (v->port[port_id - 1] != port_id) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: %s: Given port (%d) is not in this VLAN \n", slave_dev, slave_dev->name, port_id);
			return -1;
		}

		/*
		 * Iteratively, first deleting existing rule for the given port
		 * Then, add the given port to the bridge.
		 */
		v->xlate_info.br = NULL;
		v->xlate_info.port_id = port_id;
		ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: Failed to delete old translation rule for port: %d, error = %d\n", v, port_id, ret);
			return -1;
		}

		v->xlate_info.br = bridge_iface;
		ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			v->xlate_info.br = NULL;
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%px: Failed to add new translation rule for port: %d, error = %d\n", v, port_id, ret);
			return -1;
		}
	}

	v->bridge_iface = bridge_iface;
	v->xlate_info.br = bridge_iface;
	rcu_read_unlock();
	return 0;
}

/*
 * nss_ppe_vlan_mgr_tpid_proc_handler()
 *	Sets customer TPID and service TPID
 */
static int nss_ppe_vlan_mgr_tpid_proc_handler(struct ctl_table *ctl,
					  int write, void __user *buffer,
					  size_t *lenp, loff_t *ppos)
{
	int ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (write)
		nss_ppe_vlan_mgr_update_ppe_tpid();

	return ret;
}

/*
 * nss_vlan sysctl table
 */
static struct ctl_table nss_vlan_table[] = {
	{
		.procname	= "ctpid",
		.data		= &vlan_mgr_ctx.ctpid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &nss_ppe_vlan_mgr_tpid_proc_handler,
	},
	{
		.procname	= "stpid",
		.data		= &vlan_mgr_ctx.stpid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &nss_ppe_vlan_mgr_tpid_proc_handler,
	},
#ifdef NSS_EXT_VLAN_FEATURE_SUPPORT
	{
		.procname       = "ctpid_ext",
		.data           = &vlan_mgr_ctx.ctpid_ext,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = &nss_ppe_vlan_mgr_tpid_proc_handler,
	},
	{
		.procname       = "stpid_ext",
		.data           = &vlan_mgr_ctx.stpid_ext,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = &nss_ppe_vlan_mgr_tpid_proc_handler,
	},
#endif
	{ }
};

/*
 * nss_ppe_vlan_mgr_vlan_over_bridge_unregister_cb()
 *	Un-register callback for VLAN over bridge
 */
void nss_ppe_vlan_mgr_vlan_over_bridge_unregister_cb(void)
{
	spin_lock(&vlan_mgr_ctx.lock);
	vlan_mgr_ctx.vlan_over_bridge_cb = NULL;
	spin_unlock(&vlan_mgr_ctx.lock);

	nss_ppe_vlan_mgr_trace("%px Un-registered the cb in VLAN mgr\n", vlan_mgr_ctx.vlan_over_bridge_cb);
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_vlan_over_bridge_unregister_cb);

/*
 * nss_ppe_vlan_mgr_register_bond_dev_get_id_cb()
 *	Register callback to get the bond_id of netdevice.
 */
void nss_ppe_vlan_mgr_register_bond_dev_get_id_cb(nss_ppe_bond_dev_get_id_cb_t cb)
{
	spin_lock(&vlan_mgr_ctx.lock);
	vlan_mgr_ctx.bond_id_get_cb = cb;
	spin_unlock(&vlan_mgr_ctx.lock);

	nss_ppe_vlan_mgr_trace("%px: Callback registered in VLAN mgr to get bond_id of bonded netdev\n",
			       vlan_mgr_ctx.bond_id_get_cb);
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_register_bond_dev_get_id_cb);

/*
 * nss_ppe_vlan_mgr_vlan_over_bridge_register_cb()
 *	Register callback for VLAN over bridge to increment and decrement no. of bridge VLAN netdev in bridge mgr
 */
void nss_ppe_vlan_mgr_vlan_over_bridge_register_cb(nss_ppe_vlan_mgr_br_vlan_cb_t cb)
{
	spin_lock(&vlan_mgr_ctx.lock);
	vlan_mgr_ctx.vlan_over_bridge_cb = cb;
	spin_unlock(&vlan_mgr_ctx.lock);

	nss_ppe_vlan_mgr_trace("%px: Callback registered in VLAN mgr to update bridge VLAN netdev cnt\n",
			       vlan_mgr_ctx.vlan_over_bridge_cb);
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_vlan_over_bridge_register_cb);

/*
 * nss_ppe_vlan_mgr_leave_bridge()
 *	update ingress and egress vlan translation rule to restore vlan
 */
int nss_ppe_vlan_mgr_leave_bridge(struct net_device *dev, struct ppe_drv_iface *bridge_iface)
{
	struct nss_vlan_pvt *v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);
	struct net_device *real_dev;
	int32_t port_id;
	ppe_drv_ret_t ret;
	int res;

	if (!v) {
		nss_ppe_vlan_mgr_warn("%px: Interface not found name: %s IFACE: %px\n",
								dev, dev->name, bridge_iface);
		return 0;
	}

	if (v->bridge_iface != bridge_iface) {
		nss_ppe_vlan_mgr_warn("%s is not in bridge IFACE %px, ignore\n", dev->name, bridge_iface);
		nss_ppe_vlan_mgr_instance_deref(v);
		return 0;
	}

	/*
	 * If real_dev is bond_master, update for all slaves
	 */
	real_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	if (real_dev && is_vlan_dev(real_dev)) {
		real_dev = nss_ppe_vlan_mgr_get_real_dev(real_dev);
	}

	if (!real_dev) {
		nss_ppe_vlan_mgr_warn("%px: real dev for the vlan: %s is NULL\n", v, dev->name);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	/*
	 * Check if real_dev is bond master
	 */
	if (netif_is_bond_master(real_dev)) {
		res = nss_ppe_vlan_mgr_over_bond_leave_bridge(real_dev, v);
		nss_ppe_vlan_mgr_instance_deref(v);
		if (res) {
			nss_ppe_vlan_mgr_warn("%px: Bond master: %s failed to leave bridge\n", v, real_dev->name);
			return -1;
		}

		return 0;
	}

	port_id = nss_ppe_vlan_mgr_get_port_id(real_dev);
	if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", real_dev, real_dev->name, port_id);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	/*
	 * real_dev is not bond but a PPE device
	 */
	spin_lock(&vlan_mgr_ctx.lock);

	/*
	 * If it is VLAN over VP then we don't need update rule
	 * So just update bridge to NULL and return.
	 */
	if (v->is_vlan_as_vp_iface) {
		v->xlate_info.br = NULL;
		v->bridge_iface = NULL;
		spin_unlock(&vlan_mgr_ctx.lock);
		goto done;
	}

	v->xlate_info.br = v->bridge_iface;
	v->xlate_info.port_id = port_id;
	ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_warn("%px: Failed vlan translation rule for port: %d, error = %d\n", dev, port_id, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	bridge_iface = v->bridge_iface;
	v->xlate_info.br = NULL;
	v->bridge_iface = NULL;
	ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_warn("%px: Failed to add new translation rule for port: %d, error = %d\n", dev, port_id, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	spin_unlock(&vlan_mgr_ctx.lock);

	/*
	 * Set port STP state to forwarding after vlan interface leaves bridge
	 * If this fails, not adding VLAN back to bridge.
	 */
	ret = ppe_drv_br_stp_state_set(bridge_iface, real_dev, FAL_STP_FORWARDING);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%px: failed to set STP state to FORWARDING %s, error = %d\n", v, real_dev->name, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

done:
	/*
	 * Release the reference taken in join
	 */
	nss_ppe_vlan_mgr_instance_deref(v);

	/*
	 * Release the reference taken above
	 */
	nss_ppe_vlan_mgr_instance_deref(v);
	return 0;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_leave_bridge);

/*
 * nss_ppe_vlan_mgr_join_bridge()
 *	update ingress and egress vlan translation rule to use bridge iface
 */
int nss_ppe_vlan_mgr_join_bridge(struct net_device *dev, struct ppe_drv_iface *bridge_iface)
{
	struct net_device *real_dev;
	int res;
	int32_t port_id;
	ppe_drv_ret_t ret;

	/*
	 * On successful bridge join release the reference on vlan instance, during bridge leave
	 */
	struct nss_vlan_pvt *v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);

	if (!v) {
		nss_ppe_vlan_mgr_warn("%px: Interface not found name: %s IFACE: %px\n",
						dev, dev->name, bridge_iface);
		return 0;
	}

	if ((v->bridge_iface == bridge_iface) || v->bridge_iface) {
		nss_ppe_vlan_mgr_warn("%s is already in bridge IFACE %px, can't change to %px\n",
								dev->name, v->bridge_iface, bridge_iface);
		nss_ppe_vlan_mgr_instance_deref(v);
		return 0;
	}

	real_dev = nss_ppe_vlan_mgr_get_real_dev(dev);
	if (real_dev && is_vlan_dev(real_dev)) {
		real_dev = nss_ppe_vlan_mgr_get_real_dev(real_dev);
	}

	if (!real_dev) {
		nss_ppe_vlan_mgr_warn("%px: real dev for the vlan: %s is NULL\n", v, dev->name);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	/*
	 * If vlan_as_vp is enabled and port_id is vlan_as_vp_interface_num
	 * then have created vp for vlan interface and ingress and egress rule
	 * are added while configuring vlan port. here we just need to update
	 * nss_vlan_pvt object with bridge information.
	 */
	if (v->is_vlan_as_vp_iface) {
		spin_lock(&vlan_mgr_ctx.lock);
		v->xlate_info.br = bridge_iface;
		v->bridge_iface = bridge_iface;
		spin_unlock(&vlan_mgr_ctx.lock);
		return 0;
	}

	/*
	 * If real_dev is bond_master, update for all slaves
	 * Check if real_dev is bond master
	 */
	if (netif_is_bond_master(real_dev)) {
		res = nss_ppe_vlan_mgr_over_bond_join_bridge(v, real_dev, bridge_iface);
		nss_ppe_vlan_mgr_instance_deref(v);
		if (res) {
			nss_ppe_vlan_mgr_warn("%px: Bond master: %s failed to join bridge\n", v, real_dev->name);
			return -1;
		}

		return 0;
	}

	port_id = nss_ppe_vlan_mgr_get_port_id(real_dev);
	if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is invalid port\n", real_dev, real_dev->name, port_id);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	/*
	 * real_dev is not bond but a PPE device
	 */
	spin_lock(&vlan_mgr_ctx.lock);
	v->xlate_info.br = NULL;
	v->xlate_info.port_id = port_id;
	ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_instance_deref(v);
		nss_ppe_vlan_mgr_warn("%px: Failed to delete old translation rule for port: %d, error = %d\n", v, port_id, ret);
		return -1;
	}

	v->xlate_info.br = bridge_iface;
	v->xlate_info.port_id = port_id;
	ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		v->xlate_info.br = NULL;
		spin_unlock(&vlan_mgr_ctx.lock);
		nss_ppe_vlan_mgr_warn("%px: Failed vlan translation rule for port: %d, error = %d\n", dev, port_id, ret);
		nss_ppe_vlan_mgr_instance_deref(v);
		return -1;
	}

	v->bridge_iface = bridge_iface;
	spin_unlock(&vlan_mgr_ctx.lock);
	return 0;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_join_bridge);

/*
 * nss_ppe_vlan_mgr_delete_bond_slave()
 *	Delete new slave port from bond_vlan
 */
int nss_ppe_vlan_mgr_delete_bond_slave(struct net_device *slave_dev)
{
	struct nss_vlan_pvt *v;
	int32_t port_id;
	ppe_drv_ret_t ret;

	port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
	if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", slave_dev, slave_dev->name, port_id);
		return -1;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if (v->port[port_id - 1] != port_id) {
			continue;
		}

		v->xlate_info.port_id = v->port[port_id - 1];
		ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("slave: %s: failed to remove VLAN rule, error = %d\n", slave_dev->name, ret);
			return -1;
		}

#ifndef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[port_id - 1], FAL_QINQ_EDGE_PORT)) {
			v->xlate_info.port_id = v->port[port_id - 1];
			ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("%px: Failed to update role\n", v);
			return -1;
		}
#endif

		/*
		 * Detach the slave dev also from vlan over bond instance
		 */
		ret = ppe_drv_vlan_lag_slave_leave(v->iface, slave_dev);
		if (ret != PPE_DRV_RET_SUCCESS) {
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("slave: %s: failed to detach, error = %d\n", slave_dev->name, ret);
			return -1;
		}

		vlan_mgr_ctx.port_role[port_id] = FAL_QINQ_EDGE_PORT;
		v->port[port_id - 1] = 0;
	}

	spin_unlock(&vlan_mgr_ctx.lock);
	return 0;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_delete_bond_slave);

/*
 * nss_ppe_vlan_mgr_add_bond_slave()
 *	Add new slave port to bond_vlan
 */
int nss_ppe_vlan_mgr_add_bond_slave(struct net_device *bond_dev,
			struct net_device *slave_dev)
{
	struct nss_vlan_pvt *v;
	int32_t port_id, bond_id = -1;
	ppe_drv_ret_t ret;

	BUG_ON(!netif_is_bond_master(bond_dev));

#if defined(BONDING_SUPPORT)
	if (vlan_mgr_ctx.bond_id_get_cb) {
		bond_id = vlan_mgr_ctx.bond_id_get_cb(bond_dev);
	}
#endif
	if (bond_id < 0) {
		nss_ppe_vlan_mgr_warn("%s: Invalid LAG group id 0x%x\n", bond_dev->name, bond_id);
		return -1;
	}

	port_id = nss_ppe_vlan_mgr_get_port_id(slave_dev);
	if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", slave_dev, slave_dev->name, port_id);
		return -1;
	}

	/*
	 * find all the vlan_pvt structure which has parent bond_dev
	 */
	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if (v->bond_id != bond_id) {
			continue;
		}

		/*
		 * Instantaneous info of xlate_info.port_id is not important.
		 * It only used to pass information to PPE. Since it is lock protected.
		 * It is fine to use.
		 */
		v->port[port_id - 1] = port_id;
		v->xlate_info.port_id = v->port[port_id - 1];
		ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("bond: %s -> slave: %s: failed to add VLAN rule, error = %d\n", bond_dev->name, slave_dev->name, ret);
			return -1;
		}

#ifdef NSS_VLAN_MGR_DEFAULT_ROLE_CORE
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[port_id - 1], FAL_QINQ_CORE_PORT)) {
			v->xlate_info.port_id = v->port[port_id - 1];
			ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("%px: Failed to update role\n", v);
			return -1;
		}

		vlan_mgr_ctx.port_role[v->port[port_id - 1]] = FAL_QINQ_CORE_PORT;
#else
		/*
		 * Update port role
		 */
		if ((v->ppe_svid != FAL_VLAN_INVALID) &&
				(vlan_mgr_ctx.port_role[v->port[port_id - 1]] != FAL_QINQ_CORE_PORT)) {

			/*
			 * If double tag, we should set the port as core port
			 */
			if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[port_id - 1], FAL_QINQ_CORE_PORT)) {
				v->xlate_info.port_id = v->port[port_id - 1];
				ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
				spin_unlock(&vlan_mgr_ctx.lock);
				nss_ppe_vlan_mgr_warn("%px: Failed to update role\n", v);
				return -1;
			}

			vlan_mgr_ctx.port_role[v->port[port_id - 1]] = FAL_QINQ_CORE_PORT;
		}
#endif

		/*
		 * Each slave dev should get attached to vlan over bond instance
		 */
		ret = ppe_drv_vlan_lag_slave_join(v->iface, slave_dev);
		if (ret != PPE_DRV_RET_SUCCESS) {
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("bond: %s failed to join slave dev %s, error = %d\n", bond_dev->name, slave_dev->name, ret);
			return -1;
		}
	}

	spin_unlock(&vlan_mgr_ctx.lock);
	return 0;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_add_bond_slave);

/*
 * nss_ppe_vlan_mgr_del_vlan_rule()
 *	Delete VLAN translation rule in PPE
 */
void nss_ppe_vlan_mgr_del_vlan_rule(struct net_device *dev, struct ppe_drv_iface *bridge_iface, int vid)
{
	struct ppe_drv_vlan_xlate_info xlate_info;
	int32_t port_id;
	ppe_drv_ret_t ret;

	nss_ppe_vlan_mgr_assert(!bridge_iface);
	port_id = nss_ppe_vlan_mgr_get_port_id(dev);
	if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", dev, dev->name, port_id);
		return;
	}

	/*
	 * In this use case, bridge_iface is always not NULL
	 * So, passed iface value is irrelevant.
	 */
	xlate_info.br = bridge_iface;
	xlate_info.port_id = port_id;
	xlate_info.svid = FAL_VLAN_INVALID;
	xlate_info.cvid = vid;

	ret = ppe_drv_vlan_del_xlate_rule(bridge_iface, &xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%px: Failed deleting vlan(%x) translation rule for port: %d, error = %d\n", dev, vid, port_id, ret);
		return;
	}

	nss_ppe_vlan_mgr_info("%px: Deleted vlan(%x) translation rule for port: %d\n", dev, vid, port_id);
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_del_vlan_rule);

/*
 * nss_ppe_vlan_mgr_add_vlan_rule()
 *	Add VLAN translation rule in PPE
 */
void nss_ppe_vlan_mgr_add_vlan_rule(struct net_device *dev, struct ppe_drv_iface *bridge_iface, int vid)
{
	struct ppe_drv_vlan_xlate_info xlate_info;
	int32_t port_id;
	ppe_drv_ret_t ret;

	nss_ppe_vlan_mgr_assert(!bridge_iface);
	port_id = nss_ppe_vlan_mgr_get_port_id(dev);
	if (port_id == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", dev, dev->name, port_id);
		return;
	}

	/*
	 * In this use case, bridge_iface is always not NULL
	 * So, passed iface value is irrelevant.
	 */
	xlate_info.br = bridge_iface;
	xlate_info.port_id = port_id;
	xlate_info.svid = FAL_VLAN_INVALID;
	xlate_info.cvid = vid;
	ret = ppe_drv_vlan_add_xlate_rule(bridge_iface, &xlate_info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("%px: Failed vlan(%x) translation rule for port: %d, error = %d\n", dev, vid, port_id, ret);
		return;
	}

	nss_ppe_vlan_mgr_info("%px: Added vlan(%x) translation rule for port: %d\n", dev, vid, port_id);
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_add_vlan_rule);

/*
 * nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule()
 * 	Add or Delete the ingress rule
 */
int nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule(struct ppe_drv_iface *slave_iface, struct net_device *bridge_dev,
						     enum nss_ppe_vlan_mgr_ingress_br_vlan_rule rule_action)
{
	struct nss_vlan_pvt *v;
	int ret = 0;

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		nss_ppe_vlan_mgr_trace("%px  bridge %s v->br_net_dev dev %s vid %d action %d\n", v, bridge_dev->name,
				       v->br_net_dev->name, v->vid, rule_action);

		if (v->br_net_dev != bridge_dev)
			continue;

		if (rule_action == NSS_PPE_VLAN_MGR_INGRESS_BR_VLAN_RULE_ADD) {
			if ((nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_add(slave_iface, v))
					!= PPE_DRV_RET_SUCCESS) {
				nss_ppe_vlan_mgr_warn("Add ingress xlate rule failed for slave %p\n",
							slave_iface);
				ret = -1;
			}
		} else if (rule_action == NSS_PPE_VLAN_MGR_INGRESS_BR_VLAN_RULE_DEL) {
			if ((nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule_del(slave_iface, v))
					!= PPE_DRV_RET_SUCCESS) {
				nss_ppe_vlan_mgr_warn("Delete ingress xlate rule failed for slave %p\n",
							slave_iface);
				ret = -1;
			}
		} else {
			nss_ppe_vlan_mgr_trace("Invalid action %d for VLAN over bridge\n", rule_action);
			ret = -1;
		}
	}

	spin_unlock(&vlan_mgr_ctx.lock);
	nss_ppe_vlan_mgr_trace("Ret %d for bridge %s action %d\n", ret, bridge_dev->name, rule_action);

	return ret;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_config_bridge_vlan_ingress_rule);

/*
 * nss_ppe_vlan_mgr_get_real_dev()
 *	Get real dev for vlan interface
 */
struct net_device *nss_ppe_vlan_mgr_get_real_dev(struct net_device *dev)
{
	struct vlan_dev_priv *vlan;

	if (!dev || !is_vlan_dev(dev)) {
		return NULL;
	}

	vlan = vlan_dev_priv(dev);
	return vlan->real_dev;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_get_real_dev);

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
/*
 * nss_ppe_vlan_mgr_dsa_create_instance()
 *	Create vlan instance for DSA interface.
 */
static struct nss_vlan_pvt *nss_ppe_vlan_mgr_dsa_create_instance(struct net_device *dev, struct net_device *real_dev)
{
	struct nss_vlan_pvt *v;
	struct dsa_port *dsa_port = NULL;

	v = kzalloc(sizeof(*v), GFP_KERNEL);
	if (!v) {
		nss_ppe_vlan_mgr_warn("%px: Allocation to private structure failed: %s\n",
						dev, dev->name);
		return NULL;
	}

	INIT_LIST_HEAD(&v->list);

	dsa_port = dsa_port_from_netdev(dev);
	if (dsa_port == NULL) {
		nss_ppe_vlan_mgr_warn("%px: DSA port not found for %s\n",dev, dev->name);
		kfree(v);
		return NULL;
	}

	/*
	 * VLAN used for representing DSA interface, has TPID always fixed to 802.1Q.
	 * This is even when there is a inner VLAN in the packet, where ideally it should be 802.1AD
	 */
	v->vid = dsa_tag_8021q_standalone_vid(dsa_port);
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	/*
	 * Huntington DSA uses a non-standard TPID (0xDADB) for the standalone
	 * DSA tag to avoid conflicting with the C/S VLAN tags. PPE must be
	 * programmed with the same TPID to recognise and strip the DSA tag.
	 */
	if (nss_ppe_vlan_mgr_is_htt_dev(dev)) {
		v->tpid = ETH_P_DSA_8021Q;
	} else
#endif
	{
		v->tpid = DSA_TAG_8021Q_VLAN_PROTO;
	}
	v->bond_id = -1;

	v->port[0] = nss_ppe_vlan_mgr_get_port_id(real_dev);
	if (v->port[0] == NSS_PPE_VLAN_MGR_INVALID_PORT) {
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", real_dev, real_dev->name, v->port[0]);
		kfree(v);
		return NULL;
	}

	/*
	 * In no scenario, we can reach to this point where we have DOUBLE VLAN.
	 * Hence v->parent is always NULL.
	 */
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	if (!nss_ppe_vlan_mgr_is_htt_dev(dev) &&
			(v->tpid != vlan_mgr_ctx.ctpid) && (v->tpid != vlan_mgr_ctx.stpid)) {
#else
	if ((v->tpid != vlan_mgr_ctx.ctpid) && (v->tpid != vlan_mgr_ctx.stpid)) {
#endif
		nss_ppe_vlan_mgr_warn("%s: single tag: tpid %04x not match global tpid(%04x, %04x)\n", dev->name, v->tpid, vlan_mgr_ctx.ctpid, vlan_mgr_ctx.stpid);
		kfree(v);
		return NULL;
	}

	nss_ppe_vlan_mgr_minidump_log(v, sizeof(*v), "nss_vlan_pvt");

	v->mtu = dev->mtu;
	ether_addr_copy(v->dev_addr, dev->dev_addr);
	v->ifindex = dev->ifindex;
	kref_init(&v->ref);
	return v;
}

/*
 * nss_ppe_vlan_mgr_dsa_vp_destroy()
 *	Destroy vlan instance for DSA interface
 */
int nss_ppe_vlan_mgr_dsa_vp_destroy(struct net_device *dev)
{
	struct nss_vlan_pvt *v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	int res;
#endif

	/*
	 * Do we have it on record?
	 */
	if (!v) {
		nss_ppe_vlan_mgr_del_vlan_as_vp(dev->name);
		nss_ppe_vlan_mgr_warn("DSA %s, vlan_pvt not found. Already removed?\n", dev->name);
		return -1;
	}

	nss_ppe_vlan_mgr_trace("Unregistering DSA dev: %s\n", dev->name);

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	/*
	 * Clean up the VLAN-to-PORT HW mapping programmed for DSA QinQ support.
	 */
	if (nss_ppe_vlan_mgr_is_htt_dev(dev)) {
		res = ppe_drv_vlan_dsa_cfg_reset(v->xlate_info.port_id, v->vid);
		if (res < 0) {
			nss_ppe_vlan_mgr_warn("DSA QinQ port map reset failed for dev:%s\n", dev->name);
		}
	}
#endif

	/*
	 * Release reference got by "nss_ppe_vlan_mgr_instance_find_and_ref"
	 */
	nss_ppe_vlan_mgr_instance_deref(v);

	/*
	 * Release reference got by "nss_ppe_vlan_mgr_dsa_create_instance".
	 * Also remove from the vlan_as_vp array.
	 */
	nss_ppe_vlan_mgr_del_vlan_as_vp(dev->name);
	nss_ppe_vlan_mgr_instance_deref(v);

	return 0;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_dsa_vp_destroy);

/*
 * nss_ppe_vlan_mgr_dsa_vp_create()
 *	Create vlan instance for DSA interface
 */
int nss_ppe_vlan_mgr_dsa_vp_create(struct net_device *dev, struct net_device *master_dev)
{
	struct nss_vlan_pvt *v;
	int res;

	WARN_ON(!dev);
	WARN_ON(!master_dev);

	/*
	 * We want to add DSA device to vlan_as_vp_dev_name[]
	 * This is required, so that VLAN on a DSA interface is also treated as VLAN-as-VP allowed interface.
	 */
	if (nss_ppe_vlan_mgr_add_vlan_as_vp(dev->name)) {
		nss_ppe_vlan_mgr_warn("DSA interface creation failed as dev %s couldn't be added to vlan_as_vp array.\n", dev->name);
		return -1;
	}

	if (!dsa_port_from_netdev(nss_ppe_vlan_mgr_dsa_get_real_dev(dev))) {
		nss_ppe_vlan_mgr_warn("DSA interface Port VLAN (%s) is not supported\n", dev->name);
		goto fail;
	}

	v = nss_ppe_vlan_mgr_dsa_create_instance(dev, master_dev);
	if (!v) {
		nss_ppe_vlan_mgr_warn("DSA Port Vlan instance creation failed for dev:%s\n", dev->name);
		goto fail;
	}

	res = nss_ppe_vlan_mgr_alloc_configure_ppe_vp(v, dev, master_dev);
	if (res < 0) {
		nss_ppe_vlan_mgr_instance_deref(v);
		nss_ppe_vlan_mgr_warn("DSA Port Vlan configure as PPE-VP failed for dev:%s\n", dev->name);
		goto fail;
	}

#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
	/*
	 * Configure VLAN-to-PORT HW mapping needed for DSA QinQ (lan1.10.20) support.
	 */
	if (nss_ppe_vlan_mgr_is_htt_dev(dev)) {
		res = ppe_drv_vlan_dsa_cfg_set(v->xlate_info.port_id, v->tpid, v->vid);
		if (res < 0) {
			nss_ppe_vlan_mgr_instance_deref(v);
			nss_ppe_vlan_mgr_warn("DSA QinQ port map config failed for dev:%s\n", dev->name);
			goto fail;
		}
	}
#endif
	spin_lock(&vlan_mgr_ctx.lock);
	list_add(&v->list, &vlan_mgr_ctx.list);
	spin_unlock(&vlan_mgr_ctx.lock);

	nss_ppe_vlan_mgr_warn("DSA Port Vlan configure success for :%s\n", dev->name);
	return 0;

fail:
	nss_ppe_vlan_mgr_del_vlan_as_vp(dev->name);
	return -1;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_dsa_vp_create);
#else
int nss_ppe_vlan_mgr_dsa_vp_destroy(struct net_device *dev)
{
	WARN_ON(1);
	nss_ppe_vlan_mgr_warn("nss_ppe_vlan_mgr_dsa_vp_destroy() undefined :%s\n", dev->name);
	return -1;

}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_dsa_vp_destroy);
int nss_ppe_vlan_mgr_dsa_vp_create(struct net_device *dev, struct net_device *master_dev)
{
	WARN_ON(1);
	nss_ppe_vlan_mgr_warn("nss_ppe_vlan_mgr_dsa_vp_create() undefined :%s\n", dev->name);
	return -1;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_dsa_vp_create);
#endif

#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
/*
 * nss_ppe_vlan_mgr_veip_tx_to_pon_vp()
 *      Queue SFE forwarded upstream packets to the VEIP PON VP.
 *
 * Registered as the hardware offload xmit op for VLAN-as-VEIP netdevices, so
 * SFE calls into this instead of the VLAN netdevice's normal transmit path
 * for accelerated packets.
 */
static bool nss_ppe_vlan_mgr_veip_tx_to_pon_vp(struct net_device *dev, struct sk_buff *skb)
{
	struct nss_vlan_pvt *v;
	int32_t pon_vp;

	v = nss_ppe_vlan_mgr_instance_find_and_ref(dev);
	if (!v) {
		nss_ppe_vlan_mgr_warn("%s: VEIP instance not found\n", dev->name);
		return false;
	}

	if (!v->is_vlan_as_veip_iface || !v->iface) {
		nss_ppe_vlan_mgr_warn("%s: VEIP instance invalid\n", dev->name);
		nss_ppe_vlan_mgr_instance_deref(v);
		return false;
	}

	/*
	 * Resolve the PON VP associated with this VEIP interface.
	 */
	pon_vp = ppe_drv_veip_get_port(v->iface, PPE_DRV_PORT_VIRTUAL_PON);
	if (pon_vp < 0) {
		nss_ppe_vlan_mgr_warn("%s: VEIP PON VP not found\n", dev->name);
		nss_ppe_vlan_mgr_instance_deref(v);
		return false;
	}

	ppe_vp_tx_to_ppe_by_sc(pon_vp, skb, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT);

	nss_ppe_vlan_mgr_instance_deref(v);
	return true;
}
#endif

/*
 * nss_ppe_vlan_mgr_exit_module()
 *	vlan_mgr module exit function
 */
void __exit nss_ppe_vlan_mgr_exit_module(void)
{
	unregister_netdevice_notifier(&nss_ppe_vlan_mgr_netdevice_nb);

	if (vlan_mgr_ctx.sys_hdr) {
		unregister_sysctl_table(vlan_mgr_ctx.sys_hdr);
	}

	nss_ppe_vlan_mgr_info("Module unloaded\n");
}

/*
 * nss_ppe_vlan_mgr_init_module()
 *	vlan_mgr module init function
 */
int __init nss_ppe_vlan_mgr_init_module(void)
{
	int idx;
	int len, i;
	char *start_ch_ptr = NULL;
	char *end_ch_ptr = NULL;
	INIT_LIST_HEAD(&vlan_mgr_ctx.list);
	spin_lock_init(&vlan_mgr_ctx.lock);

#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
	INIT_LIST_HEAD(&vlan_mgr_ctx.xlate_ctx_list);
	vlan_mgr_ctx.wlan_dst_xlate_en = true;
#else
	vlan_mgr_ctx.wlan_dst_xlate_en = false;
#endif
	vlan_mgr_ctx.ctpid = ETH_P_8021Q;
	vlan_mgr_ctx.stpid = ETH_P_8021Q;
#ifdef NSS_EXT_VLAN_FEATURE_SUPPORT
	vlan_mgr_ctx.ctpid_ext = ETH_P_8021AD;
	vlan_mgr_ctx.stpid_ext = ETH_P_8021AD;
#endif

	vlan_mgr_ctx.sys_hdr = register_sysctl("ppe/vlan_client", nss_vlan_table);
	if (!vlan_mgr_ctx.sys_hdr) {
		nss_ppe_vlan_mgr_warn("Unabled to register sysctl table for vlan manager\n");
		return -EFAULT;
	}

	if (nss_ppe_vlan_mgr_update_ppe_tpid()) {
		unregister_sysctl_table(vlan_mgr_ctx.sys_hdr);
		return -EFAULT;
	}

	for (idx = 0; idx < NSS_PPE_VLAN_MGR_PORT_MAX; idx++) {
		vlan_mgr_ctx.port_role[idx] = FAL_QINQ_EDGE_PORT;
	}

	/*
	 * If vlan_as_vp_interface module param contains empty string,
	 * then no parsing is required. Return from here.
	 */
	if (vlan_as_vp_interface[0] == '\0') {
		register_netdevice_notifier(&nss_ppe_vlan_mgr_netdevice_nb);
		nss_ppe_vlan_mgr_info("Module (Build %s) loaded\n", NSS_PPE_BUILD_ID);
		return 0;
	}

	/*
	 * Traverse the 'vlan_as_vp_interface' module parameter array, which contains the
	 * list of netdevices for the VLAN as VP feature and store the individual device names
	 * in the 'vlan_as_vp_dev_name' array for VLAN as VP feature enablement logic.
	 */
	start_ch_ptr = vlan_as_vp_interface + strspn(vlan_as_vp_interface, NSS_PPE_VLAN_MGR_WHITESPACE);
	for (i = 0; *start_ch_ptr; start_ch_ptr = end_ch_ptr + strspn(end_ch_ptr, NSS_PPE_VLAN_MGR_WHITESPACE), i++) {

		if (i == NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX) {
			nss_ppe_vlan_mgr_warn("The device names in VLAN as vp module param is more than %d\n",
					i);
			break;
		}

		end_ch_ptr = start_ch_ptr + strcspn(start_ch_ptr, NSS_PPE_VLAN_MGR_WHITESPACE);
		if (end_ch_ptr != start_ch_ptr) {
			len = end_ch_ptr - start_ch_ptr;
		} else {
			len = strlen(start_ch_ptr);
		}

		if (len <= IFNAMSIZ) {
			strscpy(vlan_as_vp_dev_name[i], start_ch_ptr, len + 1);
			nss_ppe_vlan_mgr_info("VLAN as VP interface name: %s, index: %d\n",
					vlan_as_vp_dev_name[i], i);
		}
	}

	for (i = 0; i < NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX; i++) {
		if (vlan_as_vp_dev_name[i][0] == '\0') {
			continue;
		}
	}

	register_netdevice_notifier(&nss_ppe_vlan_mgr_netdevice_nb);

	nss_ppe_vlan_mgr_info("Module (Build %s) loaded\n", NSS_PPE_BUILD_ID);
	return 0;
}

module_init(nss_ppe_vlan_mgr_init_module);
module_exit(nss_ppe_vlan_mgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE vlan manager");
