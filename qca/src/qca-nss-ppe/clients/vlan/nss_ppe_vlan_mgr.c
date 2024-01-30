/*
 **************************************************************************
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include <net/bonding.h>
#include <ppe_drv_public.h>
#include <nss_ppe_vlan_mgr.h>
#include <ref/ref_vsi.h>
#include "nss_ppe_vlan_mgr_priv.h"


static struct nss_ppe_vlan_mgr_context vlan_mgr_ctx;

/*
 * nss_ppe_vlan_mgr_update_ppe_tpid()
 *	Update tag protocol identifier.
 */
static int nss_ppe_vlan_mgr_update_ppe_tpid(void)
{
	ppe_drv_ret_t ret;
	uint32_t mask = FAL_TPID_CTAG_EN | FAL_TPID_STAG_EN;
	uint16_t ctpid = vlan_mgr_ctx.ctpid;
	uint16_t stpid = vlan_mgr_ctx.stpid;

#ifdef NSS_VLAN_MGR_PPE_VP_TUN_SUPPORT
	mask |= (FAL_TUNNEL_TPID_CTAG_EN | FAL_TUNNEL_TPID_STAG_EN);
#endif

	ret = ppe_drv_vlan_tpid_set(ctpid, stpid, mask);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_warn("failed to set ctpid %d stpid %d, error = %d\n", ctpid, stpid, ret);
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
	fal_port_qinq_role_t mode;

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
 * nss_ppe_vlan_mgr_instance_deref()
 *	Decreases the references of vlan_pvt instance.
 */
static void nss_ppe_vlan_mgr_instance_deref(struct nss_vlan_pvt *v)
{
	spin_lock(&vlan_mgr_ctx.lock);
	--v->refs;
	BUG_ON(!(v->refs));
	spin_unlock(&vlan_mgr_ctx.lock);
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
		return NULL;
	}

	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if (v->ifindex == dev->ifindex) {
			v->refs++;
			spin_unlock(&vlan_mgr_ctx.lock);
			return v;
		}
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	return NULL;
}

/*
 * nss_ppe_vlan_mgr_calculate_new_port_role()
 *	check if we can change this port to edge port
 */
static bool nss_ppe_vlan_mgr_calculate_new_port_role(int32_t port, int32_t portindex, struct net_device *dev)
{
	struct nss_vlan_pvt *v;
	bool to_edge_port = true;

	if (vlan_mgr_ctx.port_role[port] == FAL_QINQ_EDGE_PORT) {
		return false;
	}

	if (vlan_mgr_ctx.ctpid != vlan_mgr_ctx.stpid) {
		return false;
	}

	/*
	 * If no other double VLAN interface on the same physcial port,
	 * we set PPE port as edge port
	 */
	spin_lock(&vlan_mgr_ctx.lock);
	list_for_each_entry(v, &vlan_mgr_ctx.list, list) {
		if ((v->port[portindex] == port) && (v->parent)) {
			to_edge_port = false;
			break;
		}
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	if (to_edge_port) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, port, FAL_QINQ_EDGE_PORT)) {
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
			vlan_over_bond = v->bond_id ? true : false;
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
 * nss_ppe_vlan_mgr_bond_configure_ppe()
 *	Configure PPE for bond device
 */
static int nss_ppe_vlan_mgr_bond_configure_ppe(struct nss_vlan_pvt *v, struct net_device *bond_dev, struct net_device *dev)
{
	int res = 0;
	struct net_device *slave_dev;
	int32_t port_id;
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
	ret = ppe_drv_vlan_init(v->iface, base_dev, v->vid);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: failed to initialize, PPE updated, error = %d\n", dev->name, ret);
		goto free_iface;
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
		goto clear_mac_addr;
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
			goto clear_mac_addr;
		}

		v->xlate_info.port_id = v->port[port_id - 1];
		ret = ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
		if (ret != PPE_DRV_RET_SUCCESS) {
			rcu_read_unlock();
			nss_ppe_vlan_mgr_warn("%s: failed to set vlan translation, error = %d\n", slave_dev->name, ret);
			goto clear_mac_addr;
		}
	}
	rcu_read_unlock();

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

	return res;

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
 * nss_ppe_vlan_mgr_configure_ppe()
 *	Configure PPE for non-bond devices
 */
static int nss_ppe_vlan_mgr_configure_ppe(struct nss_vlan_pvt *v, struct net_device *dev)
{
	int res = 0;
	struct net_device *base_dev;
	ppe_drv_ret_t ret;

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

	ret = ppe_drv_vlan_init(v->iface, base_dev, v->vid);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_vlan_mgr_trace("%s: failed to initialize PPE, error = %d\n", dev->name, ret);
		goto free_iface;

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

	if ((v->ppe_svid != FAL_VLAN_INVALID) && (vlan_mgr_ctx.port_role[v->port[0]] != FAL_QINQ_CORE_PORT)) {
		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[0], FAL_QINQ_CORE_PORT)) {
			nss_ppe_vlan_mgr_warn("%s: failed to set %d as core port\n", dev->name, v->port[0]);
			goto delete_ppe_rule;
		}
		vlan_mgr_ctx.port_role[v->port[0]] = FAL_QINQ_CORE_PORT;
		res = NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED;
	}

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
static void nss_ppe_vlan_mgr_instance_free(struct nss_vlan_pvt *v, struct net_device *dev)
{
	int32_t i;
	ppe_drv_ret_t ret;

	spin_lock(&vlan_mgr_ctx.lock);
	--v->refs;
	BUG_ON(v->refs);
	if (!list_empty(&v->list)) {
		list_del(&v->list);
	}
	spin_unlock(&vlan_mgr_ctx.lock);

	if (v->iface) {

		v->xlate_info.port_id = 0;
		for (i = 0; i < NSS_PPE_VLAN_MGR_PORT_MAX; i++) {
			if (!v->port[i]) {
				continue;
			}

			v->xlate_info.port_id = v->port[i];
			ret = ppe_drv_vlan_del_xlate_rule(v->iface, &v->xlate_info);
			if (ret != PPE_DRV_RET_SUCCESS) {
				nss_ppe_vlan_mgr_warn("%s: failed to delete vlan translation, error = %d \n", dev->name, ret);
			}
			v->xlate_info.port_id = 0;
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
			if (nss_ppe_vlan_mgr_calculate_new_port_role(v->port[i], i, dev)) {
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
	kfree(v);
}

/*
 * nss_ppe_vlan_mgr_create_instance()
 *	Create vlan instance
 */
static struct nss_vlan_pvt *nss_ppe_vlan_mgr_create_instance(
						struct net_device *dev)
{
	struct nss_vlan_pvt *v;
	struct vlan_dev_priv *vlan;
	struct net_device *real_dev;
	struct net_device *slave_dev;
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
		if (!netif_is_bond_master(real_dev)) {
			v->port[0] = nss_ppe_vlan_mgr_get_port_id(real_dev);
			if (v->port[0] == NSS_PPE_VLAN_MGR_INVALID_PORT) {
				nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", real_dev, real_dev->name, v->port[0]);
				kfree(v);
				return NULL;
			}
		} else {
#if defined(BONDING_SUPPORT)
			bond_id = bond_get_id(real_dev);
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
	} else if (!v->parent->parent) {
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
	if ((NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_DOUBLE) &&
	    ((v->tpid != vlan_mgr_ctx.ctpid) || (v->parent->tpid != vlan_mgr_ctx.stpid))) {
		nss_ppe_vlan_mgr_warn("%s: double tag: tpid %04x not match global tpid(%04x, %04x)\n", dev->name, v->tpid, vlan_mgr_ctx.ctpid,
				vlan_mgr_ctx.stpid);
		nss_ppe_vlan_mgr_instance_deref(v->parent);
		kfree(v);
		return NULL;
	}

	if ((NSS_PPE_VLAN_MGR_TAG_CNT(v) == NSS_PPE_VLAN_MGR_TYPE_SINGLE) &&
	    ((v->tpid != vlan_mgr_ctx.ctpid) && (v->tpid != vlan_mgr_ctx.stpid))) {
		nss_ppe_vlan_mgr_warn("%s: single tag: tpid %04x not match global tpid(%04x, %04x)\n", dev->name, v->tpid, vlan_mgr_ctx.ctpid, vlan_mgr_ctx.stpid);
		kfree(v);
		return NULL;
	}

	v->mtu = dev->mtu;
	ether_addr_copy(v->dev_addr, dev->dev_addr);
	v->ifindex = dev->ifindex;
	v->refs = 1;
	return v;
}

/*
 * nss_ppe_vlan_mgr_changemtu_event()
 */
static int nss_ppe_vlan_mgr_changemtu_event(struct netdev_notifier_info *info)
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

/*
 * int nss_ppe_vlan_mgr_changeaddr_event()
 */
static int nss_ppe_vlan_mgr_changeaddr_event(struct netdev_notifier_info *info)
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

	ret = ppe_drv_iface_mac_addr_set(v->iface, dev->dev_addr);
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

	v = nss_ppe_vlan_mgr_create_instance(dev);
	if (!v) {
		return NOTIFY_DONE;
	}

	vlan = vlan_dev_priv(dev);
	real_dev = vlan->real_dev;
	if (is_vlan_dev(real_dev)) {
		vlan = vlan_dev_priv(real_dev);
		real_dev = vlan->real_dev;
	}

	is_bond_master = netif_is_bond_master(real_dev);
	if (!is_bond_master) {
		res = nss_ppe_vlan_mgr_configure_ppe(v, dev);
	} else {
		res = nss_ppe_vlan_mgr_bond_configure_ppe(v, real_dev, dev);
	}

	if (res < 0) {
		nss_ppe_vlan_mgr_instance_free(v, dev);
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
			nss_ppe_vlan_mgr_instance_free(v, dev);
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
	 * Free instance
	 */
	nss_ppe_vlan_mgr_instance_free(v, dev);
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
	{ }
};

/*
 * nss_vlan sysctl dir
 */
static struct ctl_table nss_vlan_dir[] = {
	{
		.procname		= "vlan_client",
		.mode			= 0555,
		.child			= nss_vlan_table,
	},
	{ }
};

/*
 * nss_vlan systel root dir
 */
static struct ctl_table nss_vlan_root_dir[] = {
	{
		.procname		= "ppe",
		.mode			= 0555,
		.child			= nss_vlan_dir,
	},
	{ }
};

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
		nss_ppe_vlan_mgr_warn("%px: %s:%d is not valid PPE port\n", real_dev, real_dev->name, port_id);
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
	nss_ppe_vlan_mgr_instance_deref(v);
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

		if (!nss_ppe_vlan_mgr_ppe_update_port_role(v->iface, v->port[port_id - 1], FAL_QINQ_EDGE_PORT)) {
			v->xlate_info.port_id = v->port[port_id - 1];
			ppe_drv_vlan_add_xlate_rule(v->iface, &v->xlate_info);
			spin_unlock(&vlan_mgr_ctx.lock);
			nss_ppe_vlan_mgr_warn("%px: Failed to update role\n", v);
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
	bond_id = bond_get_id(bond_dev);
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
 * nss_ppe_vlan_mgr_get_real_dev()
 *	Get real dev for vlan interface
 */
struct net_device *nss_ppe_vlan_mgr_get_real_dev(struct net_device *dev)
{
	struct vlan_dev_priv *vlan;

	if (!dev) {
		return NULL;
	}

	vlan = vlan_dev_priv(dev);
	return vlan->real_dev;
}
EXPORT_SYMBOL(nss_ppe_vlan_mgr_get_real_dev);

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

	INIT_LIST_HEAD(&vlan_mgr_ctx.list);
	spin_lock_init(&vlan_mgr_ctx.lock);

	vlan_mgr_ctx.ctpid = ETH_P_8021Q;
	vlan_mgr_ctx.stpid = ETH_P_8021Q;

	vlan_mgr_ctx.sys_hdr = register_sysctl_table(nss_vlan_root_dir);
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

	register_netdevice_notifier(&nss_ppe_vlan_mgr_netdevice_nb);

	nss_ppe_vlan_mgr_info("Module (Build %s) loaded\n", NSS_PPE_BUILD_ID);
	return 0;
}

module_init(nss_ppe_vlan_mgr_init_module);
module_exit(nss_ppe_vlan_mgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE vlan manager");
