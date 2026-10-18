/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/page_pool/helpers.h>
#include <ppe_drv.h>
#include "ppe_vp_base.h"
#include <linux/debug_mem_usage.h>

#define PPE_VP_FLOW_IDX_FOR_NO_QDISC	-1
extern struct ppe_vp_base vp_base;

/*
 * ppe_vp_rx_process_cb
 * 	Standard Rx handler for packets with Rx VP
 */
bool ppe_vp_rx_process_cb(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;
	struct net_device *dev = skb->dev;

	skb->protocol = eth_type_trans(skb, dev);
	skb->fast_xmit = 0;

	/*
	 * Reset the below flags in case any of DS flow is exceptioned.
	 */
	skb->fast_recycled = 0;
	skb->recycled_for_ds = 0;
	mem_debug_update_skb(skb);
	netif_receive_skb(skb);
	return true;
}

/*
 * __ppe_vp_rx_dp_list_cb
 *	Forward packets received from nss-dp.
 */
static void __ppe_vp_rx_dp_list_cb(struct sk_buff_head *head, struct nss_dp_vp_rx_info *rx_info)
{
	struct ppe_vp **vpa = &vp_base.vp_table.vp_allocator[0];
	ppe_vp_list_callback_t dst_list_cb;
	struct ppe_vp_rx_stats *rx_stats;
	struct ppe_vp *dest_vp;
	void *app_data;

	/*
	 * Forward to destination VP
	 */
	if (unlikely(rx_info->dvp < PPE_DRV_VIRTUAL_START) || unlikely(rx_info->dvp > PPE_DRV_VIRTUAL_MAX)) {
		atomic64_add(skb_queue_len(head), &vp_base.base_stats.rx_dvp_invalid);
		goto drop;
	}

	rcu_read_lock();
	dest_vp = rcu_dereference(vpa[PPE_VP_BASE_PORT_TO_IDX(rx_info->dvp)]);
	if (unlikely(!dest_vp || !(dest_vp->flags & PPE_VP_FLAG_VP_ACTIVE))) {
		atomic64_add(skb_queue_len(head), &vp_base.base_stats.rx_dvp_inactive);
		rcu_read_unlock();
		goto drop;
	}

	rx_stats = this_cpu_ptr(dest_vp->vp_stats.rx_stats);
	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_pkts += skb_queue_len(head);
	rx_stats->rx_bytes += rx_info->total_bytes;
	u64_stats_update_end(&rx_stats->syncp);

	dst_list_cb = dest_vp->dst_list_cb;
	app_data = dest_vp->dst_cb_data;
	rcu_read_unlock();

	/*
	 * Destination VP user would consume the skb.
	 * User's responsibility to update skb->dev.
	 */
	if (unlikely(!dst_list_cb)) {
		atomic64_add(skb_queue_len(head), &vp_base.base_stats.rx_dvp_no_listcb);
		goto drop;
	}

	dst_list_cb(dest_vp->netdev, head, app_data);
	return;
drop:
	skb_queue_purge(head);
	return;
}

/*
 * __ppe_vp_rx_dp_skb_cb
 *	Process packet received from nss-dp.
 */
static void __ppe_vp_rx_dp_skb_cb(struct sk_buff *skb, struct nss_dp_vp_rx_info *rxi)
{
	struct ppe_vp **vpa = &vp_base.vp_table.vp_allocator[0];
	struct ppe_vp_cb_info client_cb_info = {0};
	struct ppe_vp *svp, *dvp;
	int32_t flow_idx = rxi->flow_idx;
	int8_t flags;
	struct net_device *qdisc_dev;

	/*
	 * Forward to destination VP
	 */
	if (likely(rxi->dvp >= PPE_DRV_VIRTUAL_START)) {
		struct ppe_vp_rx_stats *rx_stats;
		struct net_device *dev;

		rcu_read_lock();
		dvp = rcu_dereference(vpa[PPE_VP_BASE_PORT_TO_IDX(rxi->dvp)]);
		if (unlikely(!dvp || !(dvp->flags & PPE_VP_FLAG_VP_ACTIVE))) {
			/*
			 * Drop this packet as destination VP is not active anymore.
			 */
			atomic64_inc(&vp_base.base_stats.rx_dvp_inactive);
			rcu_read_unlock();
			mem_debug_update_skb(skb);
			dev_kfree_skb_any(skb);
			ppe_vp_info("%px: Destination VP:%d is not active anymore, dropping skb:%p\n", dvp, rxi->dvp, skb);
			return;
		}

		dev = dvp->netdev;

		rx_stats = this_cpu_ptr(dvp->vp_stats.rx_stats);

		/*
		 * Make sure device is UP before handling the packets.
		 */
		if (unlikely(!(dev->flags & IFF_UP))) {
			rcu_read_unlock();

			u64_stats_update_begin(&rx_stats->syncp);
			rx_stats->rx_dev_not_up++;
			u64_stats_update_end(&rx_stats->syncp);

			mem_debug_update_skb(skb);
			dev_kfree_skb_any(skb);

			return;
		}

		/*
		 * Pull any fake MAC added by PPE for L3 interfaces.
		 */
		if (dvp->vp_type == PPE_VP_TYPE_SW_L3) {
			struct ethhdr *ethh;
			if (unlikely(!pskb_may_pull(skb, (sizeof(struct ethhdr))))) {
				rcu_read_unlock();

				u64_stats_update_begin(&rx_stats->syncp);
				rx_stats->rx_drops++;
				u64_stats_update_end(&rx_stats->syncp);

				mem_debug_update_skb(skb);
				dev_kfree_skb_any(skb);
				ppe_vp_info("%px: Tx VP:%d skb pull failed dropping skb:%p\n", dvp, rxi->dvp, skb);
				return;
			}

			ethh = (struct ethhdr *)skb->data;
			skb->protocol = ethh->h_proto;
			skb_pull(skb, (sizeof(struct ethhdr)));
		}
		if (rxi->svp >= PPE_DRV_VIRTUAL_START) {
			svp = rcu_dereference(vpa[PPE_VP_BASE_PORT_TO_IDX(rxi->svp)]);
			if (likely(svp && svp->flags & PPE_VP_FLAG_VP_ACTIVE)) {
				skb->skb_iif = svp->netdev_if_num;
				ppe_vp_info("%px: Destination VP:%d skb:%p received from an inactive Rx VP:%d\n", dvp, rxi->dvp, skb, rxi->svp);
			}
		}

		skb->dev = dev;

		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_pkts++;
		rx_stats->rx_bytes += skb->len;
		u64_stats_update_end(&rx_stats->syncp);

		/*
		 * If it can be, try forwarding through fast_xmit.
		 */
		if (likely(dvp->flags & PPE_VP_FLAG_VP_FAST_XMIT)) {
			struct ethhdr *ethh;

#if defined(CONFIG_SKB_EXTENSIONS) && defined(CONFIG_IPQ_PON)
			if (unlikely(skb->active_extensions)) {
				atomic64_inc(&vp_base.base_stats.rx_fastxmit_fails);

				/*
				 * Update the skb protocol field
				 */
				ethh = (struct ethhdr *)skb->data;
				skb->protocol = ethh->h_proto;
				skb_reset_mac_header(skb);
				skb_set_network_header(skb, rxi->l3offset);

				mem_debug_update_skb(skb);
				dev_queue_xmit(skb);

				rcu_read_unlock();
				return;
			}
#endif
			/*
			 * Check if valid flow index and valid Qdisc info is received
			 */
			if (likely((flow_idx == PPE_VP_FLOW_IDX_FOR_NO_QDISC))
					|| !(rxi->qdisc_valid)) {
				/*
				 * packets via GRE tap tunnel over wlan interface in ACTIVE mode
				 * arrive with no valid SAWF MLO assist tag.
				 * skb->mark[31:24] != 0xAA. dev_fast_xmit_vp()
				 * sets fast_xmit=1 unconditionally, but ath12k requires a
				 * valid MLO assist tag when fast_xmit=1. Fall back to
				 * dev_queue_xmit() for such packets to avoid the ath12k drop.
				 * This check is only needed for WiFi (ACTIVE/PASSIVE) VPs;
				 * Ethernet and other VP types are unaffected by fast_xmit=1.
				 */
				if (unlikely((dvp->vp_user_mode == PPE_VP_USER_TYPE_ACTIVE) &&
						(PPE_DRV_SAWF_GET_TAG(skb->mark) != PPE_DRV_SAWF_VALID_TAG))) {
					ppe_vp_info("mark=0x%x SAWF_TAG=0x%x != SAWF_VALID=0x%x "
					       "-> dev_queue_xmit (no MLO tag)\n",
					       skb->mark, PPE_DRV_SAWF_GET_TAG(skb->mark), PPE_DRV_SAWF_VALID_TAG);
					ethh = (struct ethhdr *)skb->data;
					skb->protocol = ethh->h_proto;
					skb_reset_mac_header(skb);
					skb_set_network_header(skb, rxi->l3offset);
					mem_debug_update_skb(skb);
					dev_queue_xmit(skb);
					rcu_read_unlock();
					return;
				}

				mem_debug_update_skb(skb);
				if (unlikely(!dev_fast_xmit_vp(skb, dev))) {
					atomic64_inc(&vp_base.base_stats.rx_fastxmit_fails);

					/*
					 * Update the skb protocol field
					 */
					ethh = (struct ethhdr *)skb->data;
					skb->protocol = ethh->h_proto;
					skb_reset_mac_header(skb);
					skb_set_network_header(skb, rxi->l3offset);

					mem_debug_update_skb(skb);
					dev_queue_xmit(skb);
				}

				rcu_read_unlock();
				return;
			}

			flags = ppe_drv_get_qdisc_rule_flag(flow_idx);

			/*
			 * This is the case of Qdisc on any one interface other than bottom
			 */
			if (likely(flags & PPE_DRV_HOST_QDISC_DEV_FAST_XMIT_QDISC)) {

				/*
				 * Update the skb protocol field, since host qdisc is present
				 */
				ethh = (struct ethhdr *)skb->data;
				skb->protocol = ethh->h_proto;
				skb_reset_mac_header(skb);
				skb_set_network_header(skb, rxi->l3offset);

				qdisc_dev = ppe_drv_get_and_hold_qdisc_netdev(flow_idx);
				if (likely(qdisc_dev)) {
					skb->priority = ppe_drv_get_qos_tag(flow_idx);
					mem_debug_update_skb(skb);
					if (likely(dev_fast_xmit_qdisc(skb, qdisc_dev, dev))) {
						dev_put(qdisc_dev);
						rcu_read_unlock();
						return;
					}
				}

				atomic64_inc(&vp_base.base_stats.rx_qdisc_fastxmit_fails);
				mem_debug_update_skb(skb);
				dev_queue_xmit(skb);
				if (unlikely(qdisc_dev)) {
					dev_put(qdisc_dev);
				}

				rcu_read_unlock();
				return;
			}

			/*
			 * When qdisc is on bottom interface, send dev_queue_xmit(bottom_dev)
			 */
			if (likely(flags & PPE_DRV_HOST_QDISC_DEV_QUEUE_XMIT)) {

				/*
				 * Update the skb protocol field, since host qdisc is present
				 */
				ethh = (struct ethhdr *)skb->data;
				skb->protocol = ethh->h_proto;
				skb_reset_mac_header(skb);
				skb_set_network_header(skb, rxi->l3offset);

				qdisc_dev = ppe_drv_get_and_hold_qdisc_netdev(flow_idx);
				if (likely(qdisc_dev)) {
					skb->dev = qdisc_dev;
					skb->priority = ppe_drv_get_qos_tag(flow_idx);
				}

				mem_debug_update_skb(skb);
				dev_queue_xmit(skb);
				if (likely(qdisc_dev)) {
					dev_put(qdisc_dev);
				}

				rcu_read_unlock();
				return;
			}
		}

		/*
		 * Destination VP user would consume the skb.
		 */

		if (unlikely(dvp->dst_cb)) {
			client_cb_info.skb = skb;
			client_cb_info.ip_summed = rxi->ip_summed;
			client_cb_info.napi = rxi->napi;
			client_cb_info.fake_mac_present = rxi->fake_mac;
			client_cb_info.flow_idx = rxi->flow_idx;

			/* Initialize metadata to NONE by default */
			client_cb_info.mdata_info.mdata_type = PPE_VP_CB_MDATA_TYPE_NONE;
#ifdef NSS_PPE_DRV_HW_GRO
			/*
			 * Verify that the DP and PPE HW GRO bit enums are kept in sync.
			 */
			BUILD_BUG_ON((int)NSS_DP_VP_RX_HW_GRO_MAX != (int)PPE_VP_RX_HW_GRO_MAX);
			if (unlikely(rxi->hw_gro_flags & PPE_VP_RX_HW_GRO_EN)) {
				client_cb_info.mdata_info.mdata_type = PPE_VP_CB_MDATA_TYPE_HW_GRO;
				client_cb_info.mdata_info.hw_gro_flags = rxi->hw_gro_flags;
			}
#endif
			if (unlikely(!dvp->dst_cb(&client_cb_info, dvp->dst_cb_data))) {
				ppe_vp_info("%px: Destination VP:%d  Tx dev:%s skb:%p \
						dropped by user\n", dvp, rxi->dvp, dev->name, skb);
			}
		} else {
			/*
			 * No registered callback with vp, forward through kernel.
			 */
			struct ethhdr *ethh;
			ethh = (struct ethhdr *)skb->data;
			skb->protocol = ethh->h_proto;
			skb_set_network_header(skb, rxi->l3offset);
			mem_debug_update_skb(skb);
			dev_queue_xmit(skb);
		}

		rcu_read_unlock();
		return;
	}

	/*
	 * VP less than 64 is invalid
	 */
	if (unlikely(rxi->dvp > 0)) {
		atomic64_inc(&vp_base.base_stats.rx_dvp_invalid);
		mem_debug_update_skb(skb);
		dev_kfree_skb_any(skb);
		return;
	}

	/*
	 * It's exceptioned packet, give it to its source to be given
	 * to stack for Rx handling.
	 */
	if (rxi->svp >= PPE_DRV_VIRTUAL_START) {
		struct ppe_vp_rx_stats *rx_stats;
		struct net_device *dev;

		rcu_read_lock();
		svp = rcu_dereference(vpa[PPE_VP_BASE_PORT_TO_IDX(rxi->svp)]);
		if (unlikely(!svp || !(svp->flags & PPE_VP_FLAG_VP_ACTIVE))) {
			/*
			 * Drop this packet as source VP is not valid anymore.
			 */
			atomic64_inc(&vp_base.base_stats.rx_svp_inactive);
			rcu_read_unlock();
			mem_debug_update_skb(skb);
			dev_kfree_skb_any(skb);
			ppe_vp_info("%px: Rx VP:%d is not active anymore, dropping skb:%p\n", svp, rxi->svp, skb);
			return;
		}

		dev = svp->netdev;

		rx_stats = this_cpu_ptr(svp->vp_stats.rx_stats);

		if (svp->vp_type == PPE_VP_TYPE_SW_L3) {
			u8 version;

			ppe_vp_trace("%px: Rx VP#%d, Rx dev:%p with name: %s: L3 VP \n", svp, rxi->svp, dev, dev->name);

			/*
			 * Pull any fake MAC + 1byte(ip version) added by PPE for L3 interfaces.
			 */
			if (unlikely(!pskb_may_pull(skb, rxi->l3offset + 1))) {
				ppe_vp_info("%px: rx vp %d l3 pull failed (offset %u), drop skb %px\n",
						svp, rxi->svp, rxi->l3offset, skb);
				goto sw_l3_drop;
			}

			skb_pull(skb, rxi->l3offset);
			skb_reset_network_header(skb);

			/*
			 * Detect IP version and set protocol.
			 */
			version = (*(u8 *)skb->data) >> 4;
			if (likely(version == 4)) {
				skb->protocol = htons(ETH_P_IP);
			} else if (likely(version == 6)) {
				skb->protocol = htons(ETH_P_IPV6);
			} else {
				ppe_vp_info("%px: rx vp %d invalid IP version %u, drop skb %px\n",
						svp, rxi->svp, version, skb);
				goto sw_l3_drop;
			}

			/*
			 * Successfully identified IP version; continue with common exception path.
			 */
			goto sw_l3_done;

sw_l3_drop:
			rcu_read_unlock();

			u64_stats_update_begin(&rx_stats->syncp);
			rx_stats->rx_drops++;
			u64_stats_update_end(&rx_stats->syncp);

			mem_debug_update_skb(skb);
			dev_kfree_skb_any(skb);
			return;
		}

sw_l3_done:
		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_excp_pkts++;
		rx_stats->rx_excp_bytes += skb->len;
		u64_stats_update_end(&rx_stats->syncp);

		skb_reset_mac_header(skb);
		skb->dev = dev;
		skb->skb_iif = svp->netdev_if_num;

		client_cb_info.skb = skb;
		client_cb_info.ip_summed = rxi->ip_summed;
		client_cb_info.napi = rxi->napi;
		client_cb_info.fake_mac_present = rxi->fake_mac;
		client_cb_info.service_code = rxi->service_code;

		/*
		 * If not processed successfully VP receive handler would free the skb
		 */
		mem_debug_update_skb(skb);
		if (unlikely(!svp->src_cb(&client_cb_info, svp->src_cb_data))) {
			rcu_read_unlock();
			ppe_vp_info("%px: Rx VP:%d Rx dev:%s skb:%p dropped by user\n", svp, rxi->svp, dev->name, skb);
			return;
		}

		/*
		 * skb successfully given to Source VP user.
		 */
		rcu_read_unlock();
		return;
	}

	/*
	 * Packet has neither source or destination VP set
	 */
	atomic64_inc(&vp_base.base_stats.rx_svp_invalid);
	mem_debug_update_skb(skb);
	dev_kfree_skb_any(skb);
	return;
}

/*
 * __ppe_vp_rx_dp_xdp_cb
 *	Handle XDP buffer packets received from nss-dp.
 */
static void __ppe_vp_rx_dp_xdp_cb(struct xdp_buff *xdp, struct nss_dp_vp_rx_info *vprxi)
{
	struct ppe_vp **vpa = &vp_base.vp_table.vp_allocator[0];
	struct ppe_vp_xdp_cb_info client_cb_info = {0};
	ppe_vp_xdp_callback_t dst_xdp_cb;
	struct ppe_vp_rx_stats *rx_stats;
	struct ppe_vp *dvp;
	void *dst_cb_data;

	if (unlikely(vprxi->dvp < PPE_DRV_VIRTUAL_START) || unlikely(vprxi->dvp > PPE_DRV_VIRTUAL_MAX)) {
		atomic64_inc(&vp_base.base_stats.rx_dvp_invalid);
		goto drop;
	}

	rcu_read_lock();
	dvp = rcu_dereference(vpa[PPE_VP_BASE_PORT_TO_IDX(vprxi->dvp)]);
	if (unlikely(!dvp || !(dvp->flags & PPE_VP_FLAG_VP_ACTIVE))) {
		atomic64_inc(&vp_base.base_stats.rx_dvp_inactive);
		rcu_read_unlock();
		goto drop;
	}

	rx_stats = this_cpu_ptr(dvp->vp_stats.rx_stats);
	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_pkts++;
	rx_stats->rx_bytes += vprxi->total_bytes;
	u64_stats_update_end(&rx_stats->syncp);

	dst_xdp_cb = dvp->dst_xdp_cb;
	dst_cb_data = dvp->dst_cb_data;
	rcu_read_unlock();

	if (unlikely(!dst_xdp_cb)) {
		atomic64_inc(&vp_base.base_stats.rx_dvp_no_xdpcb);
		goto drop;
	}

	xdp->data += sizeof(struct ethhdr);
	vprxi->total_bytes -= sizeof(struct ethhdr);

	/*
	 * Populate ppe_vp_xdp_cb_info from xdp and vprxi.
	 */
	client_cb_info.xdp = xdp;
	client_cb_info.total_bytes = vprxi->total_bytes;
	client_cb_info.flow_idx = vprxi->flow_idx;
	client_cb_info.mdata_info.mdata_type = PPE_VP_CB_MDATA_TYPE_NONE;
#ifdef NSS_PPE_DRV_HW_GRO
	client_cb_info.mdata_info.hw_gro_flags = vprxi->hw_gro_flags;
	if (unlikely(vprxi->hw_gro_flags & PPE_VP_RX_HW_GRO_EN))
		client_cb_info.mdata_info.mdata_type = PPE_VP_CB_MDATA_TYPE_HW_GRO;
#endif

	dst_xdp_cb(&client_cb_info, dst_cb_data);
	return;

drop:
	xdp_return_buff(xdp);
}

/*
 * ppe_vp_rx_dp_cb()
 *      Master DP callback demultiplexer
 */
void ppe_vp_rx_dp_cb(struct nss_dp_vp_rx_data *rx_data, struct nss_dp_vp_rx_info *vprxi)
{
	switch (rx_data->type) {
	case NSS_DP_VP_RX_TYPE_SKB:
		__ppe_vp_rx_dp_skb_cb(rx_data->skb, vprxi);
		break;
	case NSS_DP_VP_RX_TYPE_SKB_LIST:
		__ppe_vp_rx_dp_list_cb(rx_data->skb_head, vprxi);
		break;
	case NSS_DP_VP_RX_TYPE_XDP:
		__ppe_vp_rx_dp_xdp_cb(rx_data->xdp, vprxi);
		break;
	default:
		ppe_vp_warn("Invalid payload type %d received\n", rx_data->type);
		break;
	}
}
