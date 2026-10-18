/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/netdevice.h>
#include <net/vxlan.h>

#include "nss_ppe_vxlanmgr_priv.h"
#include "ppe_drv_tun_cmn_ctx.h"
#include "nss_ppe_tun_drv.h"

/*
 * VxLAN tunnel context hash table. Hash-key is the vni.
 */
DEFINE_HASHTABLE(nss_ppe_vxlanmgr_tunnel_tbl, NSS_PPE_VXLANMGR_HASH_TABLE_SIZE);
DEFINE_SPINLOCK(nss_ppe_vxlanmgr_tunnel_tbl_lock);

/*
 * nss_ppe_vxlanmgr_addr_equal()
 *	Return true if both the VXLAN address are equal.
 */
static bool nss_ppe_vxlanmgr_addr_equal(union vxlan_addr *a, union vxlan_addr *b)
{
	if (!a || !b) {
		nss_ppe_vxlanmgr_warn("VXLAN address is NULL a:%p b:%p", a, b);
		return false;
	}

	nss_ppe_vxlanmgr_trace("b-family: %u  a-faimly: %u ", b->sa.sa_family, a->sa.sa_family);

	if (a->sa.sa_family != b->sa.sa_family)
		return false;

	if (a->sa.sa_family == AF_INET6) {
		nss_ppe_vxlanmgr_trace("a-addr: %pI6h b-addr: %pI6h", &a->sin6.sin6_addr, &b->sin6.sin6_addr);
		return ipv6_addr_equal(&a->sin6.sin6_addr, &b->sin6.sin6_addr);
	}

	nss_ppe_vxlanmgr_trace("b-addr: %pI4h  a-addr: %pI4h", &b->sin.sin_addr.s_addr, &a->sin.sin_addr.s_addr);
	return a->sin.sin_addr.s_addr == b->sin.sin_addr.s_addr;
}

/*
 * nss_ppe_vxlanmgr_tunnel_ctx_attach()
 *	Add VxLAN tunnel context to database.
 */
void nss_ppe_vxlanmgr_tunnel_ctx_attach(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_add(nss_ppe_vxlanmgr_tunnel_tbl, &tun_ctx->node, tun_ctx->vni);
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_tunnel_ctx_get_and_dettach()
 *	Find VxLAN tunnel context using nss_netdev and remove from database.
 */
struct nss_ppe_vxlanmgr_tun_ctx *nss_ppe_vxlanmgr_tunnel_ctx_get_and_dettach(struct net_device* nss_dev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	struct hlist_node *temp;
	unsigned bkt;

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_safe(nss_ppe_vxlanmgr_tunnel_tbl, bkt, temp, curr_tun_ctx, node) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->remote_info.nss_netdev == nss_dev)) {
			hash_del(&curr_tun_ctx->node);
			spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
			return curr_tun_ctx;
		}
	}

	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	return NULL;
}

/*
 * nss_ppe_vxlanmgr_get_remote_count()
 *	Return the count of number of remotes.
 */
uint8_t nss_ppe_vxlanmgr_get_remote_count(struct net_device *dev, __be32 vni_key)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	uint8_t count = 0;

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key))
			count++;
	}

	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	nss_ppe_vxlanmgr_trace("%px: The number of remotes for the dev: %s is: %u", dev, dev->name, count);

	return count;
}

/*
 * nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip()
 *	Find VxLAN tunnel context using vni and the remote IP address
 */
struct nss_ppe_vxlanmgr_tun_ctx *nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(__be32 vni_key, union vxlan_addr *rip)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx = NULL;

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key) && (nss_ppe_vxlanmgr_addr_equal(&curr_tun_ctx->remote_info.remote_ip, rip))) {
			spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);

			/*
			* Not holding the lock as getting used for ref & deref in sequential workqeue context.
			*/
			return curr_tun_ctx;
		}
	}

	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	return NULL;
}

/*
 * nss_ppe_vxlanmgr_new_remote()
 *	Return true if it is a new remote.
 */
bool nss_ppe_vxlanmgr_new_remote(__be32 vni_key, union vxlan_addr *rip)
{
	if (nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(vni_key, rip)) {
		return false;
	}

	return true;
}

/*
 * nss_ppe_vxlanmgr_gpe_all_remotes_set_mtu()
 *	Set mtu for all the remotes of the base parent linux netdevice
 *
 * For VXLAN-GPE VNI is not bound with the netdevice, hence unlike vxlan
 * we need to iterate the database to find and update mtu for all the
 * nss netdev which have Linux netdev as parent netdev.
 */
void nss_ppe_vxlanmgr_gpe_all_remotes_set_mtu(struct net_device *pdev, unsigned int mtu)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	unsigned bkt;

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each(nss_ppe_vxlanmgr_tunnel_tbl, bkt, curr_tun_ctx, node) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) &&
				(curr_tun_ctx->parent_dev == pdev) &&
				(!ppe_tun_mtu_set(curr_tun_ctx->remote_info.nss_netdev, mtu))) {
			nss_ppe_vxlanmgr_warn("%px: Failed to set mtu for nss_netdev %s", pdev, curr_tun_ctx->remote_info.nss_netdev->name);
		}
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_read_tunnel_config()
 *	Read tunnel configs corresponding to the given parent netdevice.
 */
void nss_ppe_vxlanmgr_read_tunnel_config(struct net_device *pdev, struct seq_file *m)
{
	struct nss_ppe_vxlanmgr_remote_info *r_info;
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	unsigned bkt;

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);

	seq_printf(m, "\nnss dev details:\n\n");
	hash_for_each(nss_ppe_vxlanmgr_tunnel_tbl, bkt, tun_ctx, node) {
		if ((tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) &&
				(tun_ctx->parent_dev == pdev)) {
			/*
			 * Fill the config details.
			 */
			r_info = &tun_ctx->remote_info;
			seq_printf(m, "\tvni: %u\n", (be32_to_cpu(tun_ctx->vni) >> 8));
			seq_printf(m, "\ttos: %u\n", tun_ctx->tos);
			seq_printf(m, "\tttl: %u\n", tun_ctx->ttl);
			seq_printf(m, "\tnss dev: %s\n", r_info->nss_netdev->name);

			if (r_info->remote_ip.sa.sa_family == AF_INET) {
				seq_printf(m, "\tremote ip: %pI4\n\n", &r_info->remote_ip.sin.sin_addr.s_addr);
			} else {
				seq_printf(m, "\tremote ip: %pI6\n\n", &r_info->remote_ip.sin6.sin6_addr);
			}
		}
	}

	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_get_ifindex_and_vp_status()
 *	Find the parent/host netdevice using the parent ndetdevice and the remote IP address.
 */
enum nss_ppe_vxlanmgr_vp_creation nss_ppe_vxlanmgr_get_ifindex_and_vp_status(struct net_device *dev, union vxlan_addr *remote_ip, uint32_t vni, int *ifindex)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx, *remote_tun_ctx;
	struct vxlan_dev *priv;
	enum nss_ppe_vxlanmgr_vp_creation vp_status = NSS_PPE_VXLANMGR_VP_CREATION_INVALID;

	nss_ppe_vxlanmgr_assert(netif_is_vxlan(dev));

	if (!remote_ip) {
		nss_ppe_vxlanmgr_trace("%px: remote IP is null", dev);
		return NSS_PPE_VXLANMGR_VP_CREATION_INVALID;
	}

	priv = netdev_priv(dev);
	if ((dstport != ntohs(priv->cfg.dst_port)) && (dstport_gpe != ntohs(priv->cfg.dst_port))) {
		nss_ppe_vxlanmgr_warn("%px: VXLAN: configured PPE dport vxlan: %u / vxlan-gpe: %u is not-equal to user given dport:%d\n", dev, dstport, dstport_gpe, ntohs(priv->cfg.dst_port));
		return NSS_PPE_VXLANMGR_VP_CREATION_FAILED;
	}

	/*
	 * CASE 1: When the Remote IP (RIP) is found in the Data-Base (DB)
	 */
	*ifindex = -1;
	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni) {
		if (curr_tun_ctx->vni == vni && nss_ppe_vxlanmgr_addr_equal(&curr_tun_ctx->remote_info.remote_ip, remote_ip)) {
			vp_status = curr_tun_ctx->vp_status;
			remote_tun_ctx = curr_tun_ctx;
			if (vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) {
				*ifindex = curr_tun_ctx->remote_info.nss_netdev->ifindex;
			}

			nss_ppe_vxlanmgr_trace("%px: Found VP in the database", dev);
			break;
		}
	}

	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);

	/*
	 * We assume VP creation status as "in-progess" for the time needed to create the VP and add it into the hash table.
	 * VXLANMgr sends VP status as "failed" when ECM query has reached NSS_PPE_VXLANMGR_VP_STATUS_MAX for each remote.
	 * This is to ensure that, vp status is not stuck in "in-progess" state for long time.
	 * This will allow ECM to go-ahead with different acceleration engine if not PPE.
	 *
	 * CASE 2: When the Remote IP (RIP) is found in the Data-Base (DB) and the vp status is "IN_PROGRESS,
	 * then wait for NSS_PPE_VXLANMGR_VP_STATUS_MAX retries to transition the vp status to the "SUCCESS" state.
	 */
	if (vp_status == NSS_PPE_VXLANMGR_VP_CREATION_IN_PROGRESS) {
		remote_tun_ctx->remote_info.vp_status_nack++;
		if (remote_tun_ctx->remote_info.vp_status_nack > vxlan_ctx.nack_limit) {
			return NSS_PPE_VXLANMGR_VP_CREATION_FAILED;
		}

		nss_ppe_vxlanmgr_trace("%px: vp_status: %u vp_status_nack: %u", dev, vp_status, remote_tun_ctx->remote_info.vp_status_nack);
	}

	/*
	 * CASE 3: When the Remote IP (RIP) is NEVER found in the Data-Base (DB)
	 * a) Never found in the data-base so return "FAILED".
	 * b) Did not find in the data-base but it may be created soon, so set status as "IN_PROGRESS".
	 */
	if (vp_status == NSS_PPE_VXLANMGR_VP_CREATION_INVALID) {
		if (nss_ppe_vxlanmgr_get_remote_count(dev, vni) == NSS_PPE_VXLANMGR_MAX_REMOTES) {
			nss_ppe_vxlanmgr_warn("%px: VXLAN: The RIP is not found in the Data-Base/Hash table", dev);
			return NSS_PPE_VXLANMGR_VP_CREATION_FAILED;
		}

		vp_status = NSS_PPE_VXLANMGR_VP_CREATION_IN_PROGRESS;
		nss_ppe_vxlanmgr_trace("%px: VP is still being created. vp_status: %u dev->name:%s", dev, vp_status, dev->name);
	}

	/*
	 * CASE 1 & 2 & 3: Return the vp_status found.
	 */
	return vp_status;
}
EXPORT_SYMBOL(nss_ppe_vxlanmgr_get_ifindex_and_vp_status);
