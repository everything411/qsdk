/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/if_bridge.h>
#include <linux/workqueue.h>
#include <linux/math64.h>
#include <net/neighbour.h>
#include <net/dst.h>
#include <net/route.h>
#include <net/ip6_route.h>
#include <linux/if_pppox.h>

#include <ppe_drv.h>
#include <ppe_drv_v4.h>
#include <ppe_drv_v6.h>
#include <ppe_drv_iface.h>
#include <ppe_vp_public.h>
#include <ppe_drv_policer.h>
#include <ppe_drv_sc.h>
#include <ppe_drv_port.h>
#ifdef NSS_UDP_ST_HGU
#include <ppe_drv_veip.h>
#endif
#include "nss_udp_st_public.h"

#define NSS_UDP_ST_PPE_POLICER_RULE_ID_BASE	1 /* Base user-level rule ID for UDP-ST ACL policers. */
#define NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED	0xFFF
#define NSS_UDP_ST_PPE_DEFAULT_CBS		32768	/* Committed burst size in bytes */
#define NSS_UDP_ST_PPE_DEFAULT_EBS		32768	/* Excess burst size in bytes */
#define NSS_UDP_ST_PPE_POLL_MS			50	/* PPE stats poll interval in milliseconds */

static struct delayed_work ppe_stats_work;
static struct workqueue_struct *ppe_stats_wq;
static uint32_t g_policer_rule_id_counter = NSS_UDP_ST_PPE_POLICER_RULE_ID_BASE;

static nss_udp_st_ppe_ctx_t g_udp_st_ppe_ctx = {
	.vp_dev = NULL,
	.vp_num = -1,
	.policer_acl_ctx = NULL,
	.policer_rule_id = 0,
};

/*
 * nss_udp_st_ppe_ipv6_addr_fill()
 *	Convert an IPv6 address to ntohl.
 */
static inline void nss_udp_st_ppe_ipv6_addr_fill(uint32_t *udp_st_addr, uint32_t *ppe_addr)
{
	uint32_t net_addr[4];

	nss_udp_st_get_ipv6_addr_hton(udp_st_addr, net_addr);
	ppe_addr[0] = ntohl(net_addr[0]);
	ppe_addr[1] = ntohl(net_addr[1]);
	ppe_addr[2] = ntohl(net_addr[2]);
	ppe_addr[3] = ntohl(net_addr[3]);
}

/*
 * nss_udp_st_ppe_vp_num_get()
 *	Return the VP number
 */
ppe_vp_num_t nss_udp_st_ppe_vp_num_get(void)
{
	return g_udp_st_ppe_ctx.vp_num;
}

/*
 * nss_udp_st_ppe_vp_dev_get()
 *	Return the VP netdevice pointer.
 */
struct net_device *nss_udp_st_ppe_vp_dev_get(void)
{
	return g_udp_st_ppe_ctx.vp_dev;
}

/*
 * nss_udp_st_ppe_netdev_setup()
 *	Setup callback for the VP dummy netdevice.
 */
static void nss_udp_st_ppe_netdev_setup(struct net_device *dev)
{
	dev->addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
	dev->needed_headroom = NSS_UDP_ST_MIN_HEADROOM;
	dev->needed_tailroom = NSS_UDP_ST_MIN_TAILROOM;
	dev->type = ARPHRD_VOID;
	dev->ethtool_ops = NULL;
	dev->header_ops = NULL;
	dev->netdev_ops = NULL;
	dev->priv_destructor = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	memcpy((void *)dev->dev_addr, "\x00\x03\x7F\xF9\x7E\xEF", dev->addr_len);
	memset(dev->broadcast, 0xff, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
#else
	dev_addr_set(dev, "\x00\x03\x7F\xF9\x7E\xEF");
	memset(dev->broadcast, 0xff, dev->addr_len);
#endif

	dev->flags |= IFF_UP;
}

/*
 * nss_udp_st_ppe_vp_dst_cb()
 *	VP destination callback: consume and free any skb delivered to our VP.
 */
static bool nss_udp_st_ppe_vp_dst_cb(struct ppe_vp_cb_info *info, void *cb_data)
{
	if (info->skb)
		dev_kfree_skb_any(info->skb);
	return true;
}

/*
 * nss_udp_st_policer_flow_add_cb()
 *	Custom flow_add_cb registered with the PPE driver for UDP-ST policer flows.
 */
static bool nss_udp_st_policer_flow_add_cb(void *app_data,
					    struct ppe_drv_policer_flow *info)
{
	/*
	 * Accept any rule_id >= NSS_UDP_ST_POLICER_RULE_ID_BASE
	 * This allows per-flow policers with unique rule IDs
	 */
	if (info->id < NSS_UDP_ST_PPE_POLICER_RULE_ID_BASE) {
		pr_warn("UDP-ST: flow_add_cb: unexpected rule_id %d (expected >= %d)\n",
			info->id, NSS_UDP_ST_PPE_POLICER_RULE_ID_BASE);
		return false;
	}

	/*
	 * No service code; hw_id applied via flow table
	 */
	info->sc_valid = false;
	pr_debug("UDP-ST: flow_add_cb: accepted rule_id %d\n", info->id);
	return true;
}

/*
 * nss_udp_st_policer_flow_del_cb()
 *	Custom flow_del_cb
 */
static bool nss_udp_st_policer_flow_del_cb(void *app_data,
				    struct ppe_drv_policer_flow *info)
{
	return true;
}

/*
 * nss_udp_st_ppe_policer_create_per_flow()
 *	Create a per-flow ACL policer to rate-limit a specific PPE flow.
 */
static struct ppe_drv_policer_acl *nss_udp_st_ppe_policer_create_per_flow(uint32_t rate_mbps, uint32_t *rule_id_out)
{
	struct ppe_drv_policer_rule_create create = {0};
	struct ppe_drv_policer_rule_create_acl_info *ai = &create.msg.acl_info;
	struct ppe_drv_policer_acl *policer_ctx;
	uint64_t committed_rate = 0;
	uint32_t rule_id;

	/*
	 * Generate unique rule ID for this flow
	 */
	rule_id = g_policer_rule_id_counter++;

	committed_rate = ((uint64_t)rate_mbps * 1000000ULL);
	committed_rate = (committed_rate / 8);	/* Mbps -> bytes/sec */
	/*
	 * Activate metering
	 */
	ai->meter_en = true;
	/*
	 * 0 = byte-based, 1 = packet based
	 */
	ai->meter_unit = 0;
	ai->colour_mode = true;
	ai->coupling_flag = true;
	ai->cir = committed_rate;
	ai->cbs = NSS_UDP_ST_PPE_DEFAULT_CBS;
	ai->eir = committed_rate;
	ai->ebs = NSS_UDP_ST_PPE_DEFAULT_EBS;

	/*
	 * Drop the red traffic
	 */
	ai->action.red_drop = true;
	/*
	 * Setting length mode to 1 for decap mode.
	 */
	ai->length_mode = 1;

	policer_ctx = ppe_drv_policer_acl_create(&create);
	if (!policer_ctx) {
		pr_err("UDP-ST: ppe_drv_policer_acl_create() failed for rate %u Mbps, rule_id %u\n",
		       rate_mbps, rule_id);
		return NULL;
	}

	ppe_drv_policer_user2hw_id_map(policer_ctx, rule_id);

	*rule_id_out = rule_id;

	pr_info("UDP-ST: Per-flow ACL policer created hw_id=%u rule_id=%u rate=%u Mbps\n",
		ppe_drv_policer_get_policer_id(policer_ctx),
		rule_id, rate_mbps);
	return policer_ctx;
}

/*
 * nss_udp_st_ppe_vp_alloc()
 *	Allocate a PPE Virtual Port for UDP-ST.
 */
bool nss_udp_st_ppe_vp_alloc(uint8_t core_mask)
{
	struct ppe_vp_ai vpai = {0};

	if (g_udp_st_ppe_ctx.vp_dev) {
		pr_warn("UDP-ST: PPE VP already allocated (dev=%s vp_num=%d)\n",
			g_udp_st_ppe_ctx.vp_dev->name, g_udp_st_ppe_ctx.vp_num);
		return true;
	}

	/*
	 * Allocate a dummy netdevice for the VP.
	 */
	g_udp_st_ppe_ctx.vp_dev = alloc_netdev(0, "udpst_ppe", NET_NAME_ENUM,
					    nss_udp_st_ppe_netdev_setup);
	if (!g_udp_st_ppe_ctx.vp_dev) {
		pr_err("UDP-ST: Failed to allocate VP netdevice\n");
		return false;
	}

	vpai.type = PPE_VP_TYPE_SW_L2;
	vpai.usr_type = PPE_VP_USER_TYPE_ACTIVE;
	vpai.core_mask = core_mask;
	vpai.dst_cb = nss_udp_st_ppe_vp_dst_cb;
	vpai.src_cb = NULL;
	vpai.dst_cb_data = &g_udp_st_ppe_ctx;
	vpai.src_cb_data = NULL;

	g_udp_st_ppe_ctx.vp_num = ppe_vp_alloc(g_udp_st_ppe_ctx.vp_dev, &vpai);
	if (g_udp_st_ppe_ctx.vp_num < 0) {
		pr_err("UDP-ST: Failed to allocate PPE VP, err(%d)\n",
		       g_udp_st_ppe_ctx.vp_num);
		free_netdev(g_udp_st_ppe_ctx.vp_dev);
		g_udp_st_ppe_ctx.vp_dev = NULL;
		return false;
	}

	pr_info("UDP-ST: PPE VP allocated: dev=%s vp_num=%d core_mask=0x%x\n",
		g_udp_st_ppe_ctx.vp_dev->name, g_udp_st_ppe_ctx.vp_num, core_mask);
	return true;
}

/*
 * nss_udp_st_ppe_policer_destroy()
 *	Destroy the global VP-level policer if it exists.
 */
void nss_udp_st_ppe_policer_destroy(void)
{
	if (g_udp_st_ppe_ctx.policer_acl_ctx) {
		pr_info("UDP-ST: Destroying VP-level policer rule_id=%u hw_id=%u\n",
			g_udp_st_ppe_ctx.policer_rule_id,
			ppe_drv_policer_get_policer_id(g_udp_st_ppe_ctx.policer_acl_ctx));
		ppe_drv_policer_acl_destroy(g_udp_st_ppe_ctx.policer_acl_ctx);
		g_udp_st_ppe_ctx.policer_acl_ctx = NULL;
		g_udp_st_ppe_ctx.policer_rule_id = 0;
	}
}

/*
 * nss_udp_st_ppe_vp_free()
 *	Free the PPE VP.
 */
void nss_udp_st_ppe_vp_free(void)
{
	if (g_udp_st_ppe_ctx.vp_num >= 0) {
		ppe_vp_free(g_udp_st_ppe_ctx.vp_num);
		g_udp_st_ppe_ctx.vp_num = -1;
	}

	if (g_udp_st_ppe_ctx.vp_dev) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
		memset((void *)g_udp_st_ppe_ctx.vp_dev->dev_addr, 0x00, g_udp_st_ppe_ctx.vp_dev->addr_len);
#else
		dev_addr_set(g_udp_st_ppe_ctx.vp_dev, "\x00\x00\x00\x00\x00\x00");
#endif
		free_netdev(g_udp_st_ppe_ctx.vp_dev);
		g_udp_st_ppe_ctx.vp_dev = NULL;
	}

	pr_info("UDP-ST: PPE VP freed\n");
}

/*
 * nss_udp_st_ppe_policer_init()
 *	Register policer flow callbacks if rate limiting is configured.
 */
void nss_udp_st_ppe_policer_init(void)
{
	if (!nust.config.rate)
		return;

	ppe_drv_policer_flow_register_cb(nss_udp_st_policer_flow_add_cb,
					 nss_udp_st_policer_flow_del_cb, NULL);
	pr_info("UDP-ST: Policer flow callbacks registered for per-flow policing\n");
}

/*
 * nss_udp_st_ppe_policer_detach()
 *	Unregister the policer flow callbacks after all UDP-ST PPE flows
 *	have been destroyed.
 */
void nss_udp_st_ppe_policer_detach(void)
{
	if (!nust.config.rate)
		return;

	ppe_drv_policer_flow_unregister_cb();

	/*
	 * Reset the policer rule ID counter so the next test starts from base
	 */
	g_policer_rule_id_counter = NSS_UDP_ST_PPE_POLICER_RULE_ID_BASE;

	pr_info("UDP-ST: policer flow callbacks unregistered (flows detached)\n");
}

/*
 * nss_udp_st_ppe_is_gem_port()
 *	Finding the real_dev is gem port or not
 */
bool nss_udp_st_ppe_is_gem_port(struct net_device *dev)
{
	struct ppe_drv_iface *port_iface;
	ppe_drv_iface_t port_iface_idx;
	uint16_t port_num;
	bool is_gem_port = false;

	/*
	 * Check if the found port is a GEM port (SFU/HGU case)
	 * If yes, we need to get the real device
	 */
	port_iface_idx = ppe_drv_iface_idx_get_by_dev(dev);
	if (port_iface_idx >= 0) {
		port_iface = ppe_drv_iface_get_by_idx(port_iface_idx);
		if (port_iface) {
			port_num = ppe_drv_iface_port_idx_get(port_iface);
			if (ppe_drv_port_is_gem(port_num)) {
				is_gem_port = true;
			}
		}
	}

	return is_gem_port;
}

/*
 * nss_udp_st_ppe_bridge_get_port()
 *	Resolve the physical bridge port for dest_mac by walking the bridge's
 *	bottom devices and querying the FDB.
 */
static struct net_device *nss_udp_st_ppe_bridge_get_port(struct net_device *br_dev,
							  const uint8_t *dest_mac,
							  uint16_t *vid_out)
{
	struct net_device *found_dev = NULL;
	struct net_device *real_dev = NULL;
	uint16_t vid = 0;
	bool is_gem_port = false;

	*vid_out = 0;

	rcu_read_lock();
	found_dev = br_fdb_find_vid_by_mac(br_dev, (u8 *)dest_mac, &vid);

	if (!found_dev) {
		pr_warn("UDP-ST: %s: no bridge port found in %s for MAC %pM\n",
		       __func__, br_dev->name, dest_mac);
		rcu_read_unlock();
		return NULL;
	}

	pr_info("UDP-ST: %s: resolved bridge %s port -> %s for MAC %pM (fdb_vid=%u)\n",
	       __func__, br_dev->name, found_dev->name, dest_mac, vid);

	/*
	 * Check if this is a SFU case
	 * In SFU, underlying device will be a vlan device and PPE rule will be with
	 * the real dev.
	 */
	if (is_vlan_dev(found_dev)) {
		vid = vlan_dev_vlan_id(found_dev);
		real_dev = vlan_dev_real_dev(found_dev);
#ifdef NSS_UDP_ST_PON
		is_gem_port = nss_udp_st_ppe_is_gem_port(real_dev);
#endif
		if (real_dev && is_gem_port) {
			pr_info("UDP-ST: %s: port %s is VLAN device, real_dev=%s vid=%u\n",
			       __func__, found_dev->name, real_dev->name, vid);
			dev_hold(real_dev);
			rcu_read_unlock();
			*vid_out = vid;
			return real_dev;
		} else {
			pr_err("UDP-ST: %s: VLAN device %s has no real device\n",
			       __func__, found_dev->name);
			rcu_read_unlock();
			return NULL;
		}
	}

	dev_hold(found_dev);
	rcu_read_unlock();
	return found_dev;
}

/*
 * nss_udp_st_ppe_get_wan_interface()
 *	Get WAN interface via ARP/route lookup on the destination IP.
 */
static struct net_device *nss_udp_st_ppe_get_wan_interface(uint32_t dest_ip, uint8_t *dest_mac)
{
	struct rtable *rt;
	struct neighbour *neigh;
	struct net_device *wan_dev = NULL;
	struct flowi4 fl4;
	uint32_t dest_ip_net;

	dest_ip_net = htonl(dest_ip);

	memset(&fl4, 0, sizeof(fl4));
	fl4.daddr = dest_ip_net;
	fl4.flowi4_proto = IPPROTO_UDP;

	/*
	 * Route lookup to find the output (WAN) interface.
	 */
	rt = ip_route_output_key(&init_net, &fl4);
	if (IS_ERR(rt)) {
		pr_err("Route lookup failed for IP: %pI4, error: %ld\n",
		       &dest_ip_net, PTR_ERR(rt));
		return NULL;
	}

	wan_dev = rt->dst.dev;
	if (!wan_dev) {
		pr_err("No output device found for IP: %pI4\n", &dest_ip_net);
		ip_rt_put(rt);
		return NULL;
	}

	/*
	 * ARP resolution to obtain the next-hop destination MAC address.
	 */
	neigh = dst_neigh_lookup(&rt->dst, &dest_ip_net);
	if (!neigh) {
		pr_err("ARP resolution failed for IP: %pI4\n", &dest_ip_net);
		ip_rt_put(rt);
		return NULL;
	}

	if (!(neigh->nud_state & NUD_VALID)) {
		pr_warn("Neighbor not reachable for IP: %pI4, state: 0x%x\n",
			&dest_ip_net, neigh->nud_state);
		neigh_release(neigh);
		ip_rt_put(rt);
		return NULL;
	}

	memcpy(dest_mac, neigh->ha, ETH_ALEN);
	pr_info("WAN interface: %s, Destination MAC: %pM\n", wan_dev->name, dest_mac);

	neigh_release(neigh);
	dev_hold(wan_dev);
	ip_rt_put(rt);
	return wan_dev;
}

/*
 * nss_udp_st_ppe_get_wan_interface_ipv6()
 *	Get WAN interface using NDP resolution for IPv6 destination.
 */
static struct net_device *nss_udp_st_ppe_get_wan_interface_ipv6(uint32_t *dest_ip, uint8_t *dest_mac)
{
	struct dst_entry *dst;
	struct rt6_info *rt;
	struct neighbour *neigh;
	struct net_device *wan_dev = NULL;
	struct in6_addr daddr;
	uint32_t addr_net[4];

	/*
	 * Convert from host byte order to network byte order.
	 */
	nss_udp_st_get_ipv6_addr_hton(dest_ip, addr_net);
	NSS_UDP_ST_IPV6_ADDR_TO_IN6_ADDR(daddr, addr_net);

	/*
	 * Route lookup
	 */
	rt = rt6_lookup(&init_net, &daddr, NULL, 0, NULL, 0);
	if (!rt) {
		pr_err("UDP-ST: IPv6 route lookup failed for IP: %pI6\n", &daddr);
		return NULL;
	}

	dst = (struct dst_entry *)rt;
	wan_dev = dst->dev;
	if (!wan_dev) {
		pr_err("UDP-ST: No output device found for IPv6: %pI6\n", &daddr);
		dst_release(dst);
		return NULL;
	}

	/*
	 * NDP resolution to get destination MAC address.
	 */
	neigh = dst_neigh_lookup(dst, &daddr);
	if (!neigh) {
		pr_err("UDP-ST: NDP resolution failed for IPv6: %pI6\n", &daddr);
		dst_release(dst);
		return NULL;
	}

	if (!(neigh->nud_state & NUD_VALID)) {
		pr_warn("UDP-ST: IPv6 neighbor not reachable: %pI6\n", &daddr);
		neigh_release(neigh);
		dst_release(dst);
		return NULL;
	}

	memcpy(dest_mac, neigh->ha, ETH_ALEN);
	pr_info("UDP-ST: WAN interface (v6): %s, Dest MAC: %pM\n",
		wan_dev->name, dest_mac);

	neigh_release(neigh);
	dst_release(dst);

	dev_hold(wan_dev);
	return wan_dev;
}

/*
 * nss_udp_st_ppe_fill_vlan_rule()
 *	Detect VLAN on WAN interface and fill the v4 rule context.
 */
static int nss_udp_st_ppe_fill_vlan_rule(struct net_device *wan_dev,
					  struct ppe_drv_vlan_rule *vlan_rule,
					  uint32_t *valid_flags,
					  uint32_t vlan_valid_flag,
					  nss_udp_st_ppe_dir_t dir,
					  ppe_drv_iface_t wan_iface_idx,
					  ppe_drv_iface_t *phy_iface_idx_out)
{
	uint32_t primary_vlan_tag;
	uint32_t secondary_vlan_tag;
	struct net_device *parent_dev;
	struct net_device *phy_dev;
	ppe_drv_iface_t phy_iface_idx;
	bool has_secondary = false;

	if (!is_vlan_dev(wan_dev))
		return 0;

	*phy_iface_idx_out = wan_iface_idx;

	primary_vlan_tag = (ntohs(vlan_dev_vlan_proto(wan_dev)) << 16) |
			    vlan_dev_vlan_id(wan_dev);
	parent_dev = vlan_dev_next_dev(wan_dev);

	if (parent_dev && is_vlan_dev(parent_dev)) {
		secondary_vlan_tag = (ntohs(vlan_dev_vlan_proto(parent_dev)) << 16) |
				      vlan_dev_vlan_id(parent_dev);
		has_secondary = true;
		phy_dev = vlan_dev_next_dev(parent_dev);
	} else {
		phy_dev = parent_dev;
	}

	if (!phy_dev) {
		pr_err("UDP-ST: VLAN WAN %s has no physical parent device\n",
		       wan_dev->name);
		return -EINVAL;
	}

	phy_iface_idx = ppe_drv_iface_idx_get_by_dev(phy_dev);
	if (phy_iface_idx < 0) {
		pr_err("UDP-ST: Failed to get PPE iface index for physical dev: %s\n",
		       phy_dev->name);
		return -EINVAL;
	}

	if (dir == NSS_UDP_ST_PPE_TX_DIR) {
		vlan_rule->primary_vlan.egress_vlan_tag = primary_vlan_tag;
		if (has_secondary) {
			vlan_rule->primary_vlan.egress_vlan_tag = secondary_vlan_tag;
			vlan_rule->secondary_vlan.egress_vlan_tag = primary_vlan_tag;
		}
	} else {
		vlan_rule->primary_vlan.ingress_vlan_tag = primary_vlan_tag;
		if (has_secondary) {
			vlan_rule->primary_vlan.ingress_vlan_tag = secondary_vlan_tag;
			vlan_rule->secondary_vlan.ingress_vlan_tag = primary_vlan_tag;
		}
	}

	*valid_flags |= vlan_valid_flag;

	pr_info("UDP-ST: WAN VLAN: dev=%s phy_dev=%s primary=0x%x%s\n",
		wan_dev->name, phy_dev->name, primary_vlan_tag,
		has_secondary ? " (QinQ)" : "");

	*phy_iface_idx_out = phy_iface_idx;
	return 0;
}

/*
 * nss_udp_st_ppe_pppoe_channel_get()
 *	Extract PPPoE channel information from the PPP layer.
 */
static int nss_udp_st_ppe_pppoe_channel_get(struct net_device *dev)
{
	struct ppp_channel *ppp_chan[1];
	int channel_count;
	int channel_protocol;
	struct pppoe_opt pppoe_info;

	/*
	 * Gets the PPPoE channel information from the PPP layer.
	 */
	channel_count = ppp_hold_channels(dev, ppp_chan, 1);
	if (channel_count != 1) {
		pr_warn("UDP-ST: Unable to get the channel for device: %s\n", dev->name);
		return -EINVAL;
	}

	/*
	 * Verify the channel protocol is PPPoE
	 */
	channel_protocol = ppp_channel_get_protocol(ppp_chan[0]);
	if (channel_protocol != PX_PROTO_OE) {
		pr_warn("UDP-ST: PPP channel protocol is not PPPoE for device: %s\n", dev->name);
		ppp_release_channels(ppp_chan, 1);
		return -EINVAL;
	}

	/*
	 * Extract PPPoE addressing information (session ID and remote MAC)
	 */
	if (pppoe_channel_addressing_get(ppp_chan[0], &pppoe_info)) {
		pr_warn("UDP-ST: Unable to get the PPPoE session information for device: %s\n", dev->name);
		ppp_release_channels(ppp_chan, 1);
		return -EINVAL;
	}

	/*
	 * Store PPPoE information in the global structure for use in flow creation
	 */
	nust.pppoe_info.dev = pppoe_info.dev;
	nust.pppoe_info.pppoe_session_id = (uint16_t)ntohs((uint16_t)pppoe_info.pa.sid);
	memcpy(nust.pppoe_info.remote_mac, pppoe_info.pa.remote, ETH_ALEN);

	pr_info("UDP-ST: PPPoE channel extracted - dev: %s, sid: 0x%x, remote_mac: %pM\n",
		pppoe_info.dev->name, pppoe_info.pa.sid, pppoe_info.pa.remote);

	dev_put(pppoe_info.dev);
	ppp_release_channels(ppp_chan, 1);
	return 0;
}

/*
 * nss_udp_st_ppe_fill_pppoe_rule()
 *	Detect PPPoE on WAN interface and fill the PPE v4 PPPoE rule fields.
 */
static int nss_udp_st_ppe_fill_pppoe_rule(struct net_device *wan_dev,
					   struct ppe_drv_pppoe_rule *pppoe_rule,
					   struct ppe_drv_vlan_rule *vlan_rule,
					   uint32_t *valid_flags,
					   uint32_t pppoe_valid_flag,
					   uint32_t vlan_valid_flag,
					   nss_udp_st_ppe_dir_t dir,
					   ppe_drv_iface_t wan_iface_idx,
					   ppe_drv_iface_t *phy_iface_idx_out)
{
	struct net_device *phy_dev = NULL;
	struct net_device *parent_dev = NULL;
	ppe_drv_iface_t phy_iface_idx;
	uint32_t vlan_tag = 0;
	bool has_vlan = false;

	if (wan_dev->type != ARPHRD_PPP)
		return 0;

	*phy_iface_idx_out = wan_iface_idx;

	parent_dev = nust.pppoe_info.dev;
	if (!parent_dev) {
		pr_err("UDP-ST: PPPoE device has no underlying device\n");
		return -EINVAL;
	}

	if (is_vlan_dev(parent_dev)) {
		has_vlan = true;
		vlan_tag = (ntohs(vlan_dev_vlan_proto(parent_dev)) << 16) |
			    vlan_dev_vlan_id(parent_dev);
		phy_dev = vlan_dev_next_dev(parent_dev);
		if (!phy_dev) {
			pr_err("UDP-ST: PPPoE over VLAN %s has no physical parent device\n",
			       parent_dev->name);
			return -EINVAL;
		}
	} else {
		phy_dev = parent_dev;
	}

	phy_iface_idx = ppe_drv_iface_idx_get_by_dev(phy_dev);
	if (phy_iface_idx < 0) {
		pr_err("UDP-ST: Failed to get PPE iface index for PPPoE physical dev: %s\n",
		       phy_dev->name);
		return -EINVAL;
	}

	if (dir == NSS_UDP_ST_PPE_TX_DIR) {
		pppoe_rule->return_session.session_id = nust.pppoe_info.pppoe_session_id;
		memcpy(pppoe_rule->return_session.server_mac, nust.pppoe_info.remote_mac, ETH_ALEN);
	} else {
		pppoe_rule->flow_session.session_id = nust.pppoe_info.pppoe_session_id;
		memcpy(pppoe_rule->flow_session.server_mac, nust.pppoe_info.remote_mac, ETH_ALEN);
	}
	*valid_flags |= pppoe_valid_flag;

	if (has_vlan) {
		if (dir == NSS_UDP_ST_PPE_TX_DIR)
			vlan_rule->primary_vlan.egress_vlan_tag = vlan_tag;
		else
			vlan_rule->primary_vlan.ingress_vlan_tag = vlan_tag;
		*valid_flags |= vlan_valid_flag;

		pr_info("UDP-ST: PPPoE over VLAN: pppoe_dev=%s vlan_dev=%s phy_dev=%s session_id=0x%x vlan_tag=0x%x server_mac=%pM\n",
			wan_dev->name, parent_dev->name, phy_dev->name,
			nust.pppoe_info.pppoe_session_id, vlan_tag,
			nust.pppoe_info.remote_mac);
	} else {
		pr_info("UDP-ST: PPPoE: pppoe_dev=%s phy_dev=%s session_id=0x%x server_mac=%pM phy_iface_idx=%d\n",
			wan_dev->name, phy_dev->name, nust.pppoe_info.pppoe_session_id,
			nust.pppoe_info.remote_mac, phy_iface_idx);
	}

	*phy_iface_idx_out = phy_iface_idx;
	return 0;
}

/*
 * nss_udp_st_ppe_create_flow_v4()
 *	Create PPE IPv4 flow using UDP-ST rule information.
 */
static int nss_udp_st_ppe_create_flow_v4(struct nss_udp_st_rules *rule, nss_udp_st_ppe_dir_t dir)
{
	struct ppe_drv_v4_rule_create *create;
	struct net_device *wan_dev;
	struct net_device *vp_dev;
	struct net_device *br_dev;
	struct net_device *port_dev = NULL;
	uint8_t dest_mac[ETH_ALEN];
	ppe_drv_iface_t vp_iface_idx, wan_iface_idx, phy_iface_idx, port_iface_idx;
	ppe_drv_ret_t ret;

	/*
	 * Use the dedicated PPE VP device
	 */
	vp_dev = g_udp_st_ppe_ctx.vp_dev;
	if (!vp_dev || g_udp_st_ppe_ctx.vp_num < 0) {
		pr_err("UDP-ST: PPE VP not allocated\n");
		return -EINVAL;
	}

	/*
	 * Get WAN interface via route/ARP lookup on the destination IP.
	 */
	wan_dev = nss_udp_st_ppe_get_wan_interface(rule->dip.ip.ipv4, dest_mac);
	if (!wan_dev) {
		pr_err("Failed to get WAN interface for destination IP: %pI4\n",
		       &rule->dip.ip.ipv4);
		return -EINVAL;
	}

	/*
	 * Get PPE interface indices for VP and WAN devices.
	 */
	vp_iface_idx = ppe_drv_iface_idx_get_by_dev(vp_dev);
	if (vp_iface_idx < 0) {
		pr_err("Failed to get VP interface index for dev: %s\n", vp_dev->name);
		dev_put(wan_dev);
		return -EINVAL;
	}

	wan_iface_idx = ppe_drv_iface_idx_get_by_dev(wan_dev);
	if (wan_iface_idx < 0) {
		pr_err("Failed to get WAN interface index for dev: %s\n", wan_dev->name);
		dev_put(wan_dev);
		return -EINVAL;
	}

	/*
	 * Finding the bottom interface in case of wan is bridge
	 */
	port_iface_idx = -1;
	port_dev = NULL;
	rule->bridge_vlan_id = 0;
	br_dev = wan_dev;

	if (netif_is_bridge_master(br_dev)) {
		uint16_t bridge_vid = 0;
		port_dev = nss_udp_st_ppe_bridge_get_port(br_dev, dest_mac, &bridge_vid);
		if (!port_dev) {
			pr_err("UDP-ST: bridge %s: no port found for MAC %pM\n",
			       br_dev->name, dest_mac);
			dev_put(wan_dev);
			return -EINVAL;
		}

		port_iface_idx = ppe_drv_iface_idx_get_by_dev(port_dev);
		if (port_iface_idx < 0) {
			pr_err("UDP-ST: failed to get PPE iface for bridge port %s\n",
			       port_dev->name);
			dev_put(port_dev);
			dev_put(wan_dev);
			return -EINVAL;
		}

		/*
		 * Store the bridge VLAN ID in the rule for use in TX path
		 */
		rule->bridge_vlan_id = bridge_vid;
		if (bridge_vid) {
			pr_info("UDP-ST: Bridge port has VLAN ID %u, will be used in TX path\n", bridge_vid);
		}
	}

	/*
	 * If WAN device is PPPoE, extract PPPoE channel information.
	 */
	if (wan_dev->type == ARPHRD_PPP && !nust.pppoe_info.dev) {
		if (nss_udp_st_ppe_pppoe_channel_get(wan_dev) < 0) {
			pr_err("UDP-ST: Failed to extract PPPoE channel for device: %s\n", wan_dev->name);
			if (port_dev)
				dev_put(port_dev);
			dev_put(wan_dev);
			return -EINVAL;
		}
	}

	/*
	 * Initialize the PPE flow create structure.
	 */
	create = kzalloc(sizeof(*create), GFP_KERNEL);
	if (!create) {
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -ENOMEM;
	}

	create->vlan_rule.primary_vlan.ingress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;
	create->vlan_rule.primary_vlan.egress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;
	create->vlan_rule.secondary_vlan.ingress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;
	create->vlan_rule.secondary_vlan.egress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;

	phy_iface_idx = wan_iface_idx;

	/*
	 * Detect VLAN on WAN interface (single or double tagged) and fill
	 * the VLAN rule.
	 */
	ret = nss_udp_st_ppe_fill_vlan_rule(wan_dev, &create->vlan_rule,
					    &create->valid_flags,
					    PPE_DRV_V4_VALID_FLAG_VLAN,
					    dir, wan_iface_idx,
					    &phy_iface_idx);
	if (ret < 0) {
		pr_err("UDP-ST: VLAN rule fill failed for WAN dev: %s\n",
		       wan_dev->name);
		kfree(create);
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -EINVAL;
	}

	if (dir == NSS_UDP_ST_PPE_TX_DIR) {
		/*
		 * Detect PPPoE on WAN interface
		 */
		ret = nss_udp_st_ppe_fill_pppoe_rule(wan_dev, &create->pppoe_rule,
							&create->vlan_rule,
							&create->valid_flags,
							PPE_DRV_V4_VALID_FLAG_RETURN_PPPOE,
							PPE_DRV_V4_VALID_FLAG_VLAN,
							dir, wan_iface_idx,
							&phy_iface_idx);
		if (ret < 0) {
			pr_err("UDP-ST: PPPoE rule fill failed for WAN dev: %s\n",
				wan_dev->name);
			kfree(create);
			if (port_dev)
				dev_put(port_dev);
			dev_put(wan_dev);
			return -EINVAL;
		}
		/*
		 * Fill 5-tuple information from UDP-ST rule.
		 */
		create->tuple.flow_ip = rule->sip.ip.ipv4;
		create->tuple.flow_ident = rule->sport;
		create->tuple.return_ip = rule->dip.ip.ipv4;
		create->tuple.return_ident = rule->dport;
		create->tuple.protocol = IPPROTO_UDP;

		create->conn_rule.rx_if = vp_iface_idx;
		create->conn_rule.tx_if = phy_iface_idx;
		create->conn_rule.flow_ip_xlate = rule->sip.ip.ipv4;
		create->conn_rule.flow_ident_xlate = rule->sport;
		create->conn_rule.return_ip_xlate = rule->dip.ip.ipv4;
		create->conn_rule.return_ident_xlate = rule->dport;
		create->conn_rule.flow_mtu = vp_dev->mtu;
		create->conn_rule.return_mtu = wan_dev->mtu;

		memcpy(create->conn_rule.flow_mac, vp_dev->dev_addr, ETH_ALEN);
		memcpy(create->conn_rule.return_mac, dest_mac, ETH_ALEN);

		create->top_rule.rx_if = vp_iface_idx;
		create->top_rule.tx_if = wan_iface_idx;

#ifdef NSS_UDP_ST_HGU
		/*
		 * Check if VEIP is enabled and adjust interface assignments
		 */
		if (ppe_drv_veip_is_enabled(wan_dev)) {
			create->top_rule.tx_if = phy_iface_idx;
			create->conn_rule.tx_if = wan_iface_idx;
		}
#endif

		/*
		 * Bridge case
		 */
		if (port_iface_idx >= 0) {
			create->top_rule.tx_if = wan_iface_idx;
			create->conn_rule.tx_if = port_iface_idx;
		}

		create->rule_flags |= PPE_DRV_V4_RULE_FLAG_FLOW_VALID |
					PPE_DRV_V4_RULE_FLAG_RETURN_VALID |
					PPE_DRV_V4_RULE_FLAG_UDP_ST_TX_FLOW;

		/*
		 * Set QoS if DSCP is configured.
		 */
		if (nust.config.dscp) {
			create->qos_rule.flow_qos_tag = nust.config.dscp >> 2;
			create->qos_rule.return_qos_tag = nust.config.dscp >> 2;
		}
	} else {
		/*
		 * Detect PPPoE on WAN interface
		 */
		ret = nss_udp_st_ppe_fill_pppoe_rule(wan_dev, &create->pppoe_rule,
							&create->vlan_rule,
							&create->valid_flags,
							PPE_DRV_V4_VALID_FLAG_FLOW_PPPOE,
							PPE_DRV_V4_VALID_FLAG_VLAN,
							dir, wan_iface_idx,
							&phy_iface_idx);
		if (ret < 0) {
			pr_err("UDP-ST: PPPoE rule fill failed for WAN dev: %s\n",
				wan_dev->name);
			kfree(create);
			if (port_dev)
				dev_put(port_dev);
			dev_put(wan_dev);
			return -EINVAL;
		}

		/*
		 * Fill 5-tuple information from UDP-ST rule.
		 */
		create->tuple.flow_ip = rule->dip.ip.ipv4;
		create->tuple.flow_ident = rule->dport;
		create->tuple.return_ip = rule->sip.ip.ipv4;
		create->tuple.return_ident = rule->sport;
		create->tuple.protocol = IPPROTO_UDP;

		create->conn_rule.rx_if = phy_iface_idx;
		create->conn_rule.tx_if = vp_iface_idx;
		create->conn_rule.flow_ip_xlate = rule->dip.ip.ipv4;
		create->conn_rule.flow_ident_xlate = rule->dport;
		create->conn_rule.return_ip_xlate = rule->sip.ip.ipv4;
		create->conn_rule.return_ident_xlate = rule->sport;
		create->conn_rule.flow_mtu = wan_dev->mtu;
		create->conn_rule.return_mtu = vp_dev->mtu;
		memcpy(create->conn_rule.flow_mac, dest_mac, ETH_ALEN);
		memcpy(create->conn_rule.return_mac, vp_dev->dev_addr, ETH_ALEN);

		create->top_rule.rx_if = wan_iface_idx;
		create->top_rule.tx_if = vp_iface_idx;

		/*
		 * Bridge case
		 */
		if (port_iface_idx >= 0) {
			create->top_rule.rx_if = wan_iface_idx;
			create->conn_rule.rx_if = port_iface_idx;
		}

		create->rule_flags = PPE_DRV_V4_RULE_FLAG_FLOW_VALID |
					PPE_DRV_V4_RULE_FLAG_RETURN_VALID |
					PPE_DRV_V4_RULE_FLAG_UDP_ST_RX_FLOW;

		if (nust.config.dscp) {
			create->qos_rule.flow_qos_tag = nust.config.dscp >> 2;
			create->qos_rule.return_qos_tag = nust.config.dscp >> 2;
		}
	}

	/*
	 * Create and attach per-flow policer if rate limiting is configured.
	 */
	if (nust.config.rate && nust.rule_count > 0) {
		struct ppe_drv_policer_acl *policer_ctx;
		uint32_t policer_rule_id;
		uint32_t per_flow_rate;

		per_flow_rate = nust.config.rate / nust.rule_count;

		policer_ctx = nss_udp_st_ppe_policer_create_per_flow(per_flow_rate, &policer_rule_id);
		if (policer_ctx) {
			pr_info("UDP-ST: attaching per-flow policer rule_id=%u hw_id=%u rate=%u Mbps\n",
				policer_rule_id, ppe_drv_policer_get_policer_id(policer_ctx), per_flow_rate);
			create->valid_flags |= PPE_DRV_V4_VALID_FLAG_ACL_POLICER;
			create->ap_rule.type = PPE_DRV_RULE_TYPE_FLOW_POLICER;
			create->ap_rule.rule_id.policer.flow_policer_id = policer_rule_id;
			create->ap_rule.rule_id.policer.flags |= PPE_DRV_VALID_FLAG_FLOW_POLICER;

			/* Store policer context in rule for cleanup */
			rule->policer_ctx = policer_ctx;
			rule->policer_rule_id = policer_rule_id;
		} else {
			pr_warn("UDP-ST: Failed to create per-flow policer for IPv4 flow\n");
		}
	}

	ret = ppe_drv_v4_create(create);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pr_err("Failed to create PPE IPv4 flow, ret: %d in %d direction\n", ret, dir);
		kfree(create);
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -EINVAL;
	}

	pr_info("UDP-ST: PPE IPv4 flow created:\n");
	pr_info("  5-tuple: %pI4:%u -> %pI4:%u (UDP)\n",
		&rule->sip.ip.ipv4, rule->sport,
		&rule->dip.ip.ipv4, rule->dport);
	pr_info("  VP  device: %s (iface_idx: %d, vp_num: %d)\n",
		vp_dev->name, vp_iface_idx, g_udp_st_ppe_ctx.vp_num);
	pr_info("  WAN device: %s (iface_idx: %d)\n", wan_dev->name, wan_iface_idx);
	if (port_dev)
		pr_info("  Bridge port: %s (iface_idx: %d)\n", port_dev->name, port_iface_idx);
	pr_info("  Dest MAC: %pM\n", dest_mac);

	kfree(create);
	if (port_dev)
		dev_put(port_dev);
	dev_put(wan_dev);
	return 0;
}

/*
 * nss_udp_st_ppe_create_flow_v6()
 *	Create PPE IPv6 flow using UDP-ST rule information.
 */
static int nss_udp_st_ppe_create_flow_v6(struct nss_udp_st_rules *rule, nss_udp_st_ppe_dir_t dir)
{
	struct ppe_drv_v6_rule_create *create;
	struct net_device *wan_dev;
	struct net_device *vp_dev;
	struct net_device *port_dev = NULL;
	uint8_t dest_mac[ETH_ALEN];
	ppe_drv_iface_t vp_iface_idx, wan_iface_idx, phy_iface_idx, port_iface_idx;
	ppe_drv_ret_t ret;

	vp_dev = g_udp_st_ppe_ctx.vp_dev;
	if (!vp_dev || g_udp_st_ppe_ctx.vp_num < 0) {
		pr_err("UDP-ST: PPE VP not allocated\n");
		return -EINVAL;
	}

	/*
	 * Get WAN interface using NDP resolution.
	 */
	wan_dev = nss_udp_st_ppe_get_wan_interface_ipv6(rule->dip.ip.ipv6, dest_mac);
	if (!wan_dev) {
		pr_err("Failed to get WAN interface for destination IPv6\n");
		return -EINVAL;
	}

	vp_iface_idx = ppe_drv_iface_idx_get_by_dev(vp_dev);
	if (vp_iface_idx < 0) {
		pr_err("Failed to get VP interface index for dev: %s\n", vp_dev->name);
		dev_put(wan_dev);
		return -EINVAL;
	}

	wan_iface_idx = ppe_drv_iface_idx_get_by_dev(wan_dev);
	if (wan_iface_idx < 0) {
		pr_err("Failed to get WAN interface index for dev: %s\n", wan_dev->name);
		dev_put(wan_dev);
		return -EINVAL;
	}

	/*
	 * Finding the bottom interface in case of wan is bridge
	 */
	port_iface_idx = -1;
	port_dev = NULL;
	rule->bridge_vlan_id = 0;

	if (netif_is_bridge_master(wan_dev)) {
		uint16_t bridge_vid = 0;
		port_dev = nss_udp_st_ppe_bridge_get_port(wan_dev, dest_mac, &bridge_vid);
		if (!port_dev) {
			pr_err("UDP-ST: bridge %s: no port found for MAC %pM (v6)\n",
			       wan_dev->name, dest_mac);
			dev_put(wan_dev);
			return -EINVAL;
		}

		port_iface_idx = ppe_drv_iface_idx_get_by_dev(port_dev);
		if (port_iface_idx < 0) {
			pr_err("UDP-ST: failed to get PPE iface for bridge port %s (v6)\n",
			       port_dev->name);
			dev_put(port_dev);
			dev_put(wan_dev);
			return -EINVAL;
		}

		/*
		 * Store the bridge VLAN ID in the rule for use in TX path
		 */
		rule->bridge_vlan_id = bridge_vid;
		if (bridge_vid) {
			pr_info("UDP-ST: Bridge port has VLAN ID %u, will be used in TX path (v6)\n", bridge_vid);
		}
	}

	/*
	 * If WAN device is PPPoE, extract PPPoE channel information.
	 */
	if (wan_dev->type == ARPHRD_PPP && !nust.pppoe_info.dev) {
		if (nss_udp_st_ppe_pppoe_channel_get(wan_dev) < 0) {
			pr_err("UDP-ST: Failed to extract PPPoE channel for device: %s\n",
			       wan_dev->name);
			if (port_dev)
				dev_put(port_dev);
			dev_put(wan_dev);
			return -EINVAL;
		}
	}

	create = kzalloc(sizeof(*create), GFP_KERNEL);
	if (!create) {
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -ENOMEM;
	}

	create->vlan_rule.primary_vlan.ingress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;
	create->vlan_rule.primary_vlan.egress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;
	create->vlan_rule.secondary_vlan.ingress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;
	create->vlan_rule.secondary_vlan.egress_vlan_tag = NSS_UDP_ST_PPE_VLAN_NOT_CONFIGURED;

	phy_iface_idx = wan_iface_idx;

	/*
	 * Detect VLAN on WAN interface and fill the v6 VLAN rule.
	 */
	ret = nss_udp_st_ppe_fill_vlan_rule(wan_dev, &create->vlan_rule,
					    &create->valid_flags,
					    PPE_DRV_V6_VALID_FLAG_VLAN,
					    dir, wan_iface_idx,
					    &phy_iface_idx);
	if (ret < 0) {
		pr_err("UDP-ST: v6 VLAN rule fill failed for WAN dev: %s\n", wan_dev->name);
		kfree(create);
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -EINVAL;
	}

	/*
	 * Detect PPPoE on WAN interface and fill the v6 PPPoE rule.
	 */
	ret = nss_udp_st_ppe_fill_pppoe_rule(wan_dev, &create->pppoe_rule,
					     &create->vlan_rule,
					     &create->valid_flags,
					     PPE_DRV_V6_VALID_FLAG_PPPOE_RETURN,
					     PPE_DRV_V6_VALID_FLAG_VLAN,
					     dir, wan_iface_idx,
					     &phy_iface_idx);
	if (ret < 0) {
		pr_err("UDP-ST: v6 PPPoE rule fill failed for WAN dev: %s\n", wan_dev->name);
		kfree(create);
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -EINVAL;
	}

	if (dir == NSS_UDP_ST_PPE_TX_DIR) {
		nss_udp_st_ppe_ipv6_addr_fill(rule->sip.ip.ipv6, create->tuple.flow_ip);
		create->tuple.flow_ident = rule->sport;
		nss_udp_st_ppe_ipv6_addr_fill(rule->dip.ip.ipv6, create->tuple.return_ip);
		create->tuple.return_ident = rule->dport;
		create->tuple.protocol = IPPROTO_UDP;

		create->conn_rule.rx_if = vp_iface_idx;
		create->conn_rule.tx_if = phy_iface_idx;
		nss_udp_st_ppe_ipv6_addr_fill(rule->sip.ip.ipv6, create->conn_rule.flow_ip_xlate);
		create->conn_rule.flow_ident_xlate = rule->sport;
		nss_udp_st_ppe_ipv6_addr_fill(rule->dip.ip.ipv6, create->conn_rule.return_ip_xlate);
		create->conn_rule.return_ident_xlate = rule->dport;
		create->conn_rule.flow_mtu = vp_dev->mtu;
		create->conn_rule.return_mtu = wan_dev->mtu;
		memcpy(create->conn_rule.flow_mac,   vp_dev->dev_addr, ETH_ALEN);
		memcpy(create->conn_rule.return_mac, dest_mac,         ETH_ALEN);

		create->top_rule.rx_if = vp_iface_idx;
		create->top_rule.tx_if = wan_iface_idx;

#ifdef NSS_UDP_ST_HGU
		/*
		 * Check if VEIP is enabled and adjust interface assignments
		 */
		if (ppe_drv_veip_is_enabled(wan_dev)) {
			create->top_rule.tx_if = phy_iface_idx;
			create->conn_rule.tx_if = wan_iface_idx;
		}
#endif

		/*
		 * Bridge case
		 */
		if (port_iface_idx >= 0) {
			create->top_rule.tx_if = wan_iface_idx;
			create->conn_rule.tx_if = port_iface_idx;
		}

		create->rule_flags = PPE_DRV_V6_RULE_FLAG_FLOW_VALID |
				     PPE_DRV_V6_RULE_FLAG_RETURN_VALID |
				     PPE_DRV_V6_RULE_FLAG_UDP_ST_TX_FLOW;

		if (nust.config.dscp) {
			create->qos_rule.flow_qos_tag = nust.config.dscp >> 2;
			create->qos_rule.return_qos_tag = nust.config.dscp >> 2;
			create->valid_flags |= PPE_DRV_V6_VALID_FLAG_QOS;
		}
	} else {
		nss_udp_st_ppe_ipv6_addr_fill(rule->dip.ip.ipv6, create->tuple.flow_ip);
		create->tuple.flow_ident = rule->dport;
		nss_udp_st_ppe_ipv6_addr_fill(rule->sip.ip.ipv6, create->tuple.return_ip);
		create->tuple.return_ident = rule->sport;
		create->tuple.protocol = IPPROTO_UDP;

		create->conn_rule.rx_if = phy_iface_idx;
		create->conn_rule.tx_if = vp_iface_idx;
		nss_udp_st_ppe_ipv6_addr_fill(rule->dip.ip.ipv6, create->conn_rule.flow_ip_xlate);
		create->conn_rule.flow_ident_xlate = rule->dport;
		nss_udp_st_ppe_ipv6_addr_fill(rule->sip.ip.ipv6, create->conn_rule.return_ip_xlate);
		create->conn_rule.return_ident_xlate = rule->sport;
		create->conn_rule.flow_mtu = wan_dev->mtu;
		create->conn_rule.return_mtu = vp_dev->mtu;
		memcpy(create->conn_rule.flow_mac,   dest_mac,         ETH_ALEN);
		memcpy(create->conn_rule.return_mac, vp_dev->dev_addr, ETH_ALEN);

		create->top_rule.rx_if = wan_iface_idx;
		create->top_rule.tx_if = vp_iface_idx;

		/*
		 * Bridge case
		 */
		if (port_iface_idx >= 0) {
			create->top_rule.rx_if = wan_iface_idx;
			create->conn_rule.rx_if = port_iface_idx;
		}

		create->rule_flags = PPE_DRV_V6_RULE_FLAG_FLOW_VALID |
				     PPE_DRV_V6_RULE_FLAG_RETURN_VALID |
				     PPE_DRV_V6_RULE_FLAG_UDP_ST_RX_FLOW;

		if (nust.config.dscp) {
			create->qos_rule.flow_qos_tag = nust.config.dscp >> 2;
			create->qos_rule.return_qos_tag = nust.config.dscp >> 2;
			create->valid_flags |= PPE_DRV_V6_VALID_FLAG_QOS;
		}
	}

	/*
	 * Create and attach per-flow policer if rate limiting is configured.
	 */
	if (nust.config.rate && nust.rule_count > 0) {
		struct ppe_drv_policer_acl *policer_ctx;
		uint32_t policer_rule_id;
		uint32_t per_flow_rate;

		per_flow_rate = nust.config.rate / nust.rule_count;

		policer_ctx = nss_udp_st_ppe_policer_create_per_flow(per_flow_rate, &policer_rule_id);
		if (policer_ctx) {
			pr_info("UDP-ST: attaching per-flow policer rule_id=%u hw_id=%u rate=%u Mbps (v6)\n",
				policer_rule_id, ppe_drv_policer_get_policer_id(policer_ctx), per_flow_rate);
			create->valid_flags |= PPE_DRV_V6_VALID_FLAG_ACL_POLICER;
			create->ap_rule.type = PPE_DRV_RULE_TYPE_FLOW_POLICER;
			create->ap_rule.rule_id.policer.flow_policer_id = policer_rule_id;
			create->ap_rule.rule_id.policer.flags |= PPE_DRV_VALID_FLAG_FLOW_POLICER;

			rule->policer_ctx = policer_ctx;
			rule->policer_rule_id = policer_rule_id;
		} else {
			pr_warn("UDP-ST: Failed to create per-flow policer for IPv6 flow\n");
		}
	}

	ret = ppe_drv_v6_create(create);
	kfree(create);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pr_err("Failed to create PPE IPv6 flow, ret: %d in %d direction\n", ret, dir);
		if (port_dev)
			dev_put(port_dev);
		dev_put(wan_dev);
		return -EINVAL;
	}

	pr_info("UDP-ST: PPE IPv6 flow created:\n");
	pr_info("  VP  device: %s (iface_idx: %d, vp_num: %d)\n",
		vp_dev->name, vp_iface_idx, g_udp_st_ppe_ctx.vp_num);
	pr_info("  WAN device: %s (iface_idx: %d)\n", wan_dev->name, wan_iface_idx);
	if (port_dev)
		pr_info("  Bridge port: %s (iface_idx: %d)\n", port_dev->name, port_iface_idx);
	pr_info("  Dest MAC: %pM\n", dest_mac);

	if (port_dev)
		dev_put(port_dev);
	dev_put(wan_dev);
	return 0;
}

/*
 * nss_udp_st_ppe_create_tx_flows()
 *	Create PPE flows for all UDP-ST rules.
 */
int nss_udp_st_ppe_create_flows(nss_udp_st_ppe_dir_t dir)
{
	struct nss_udp_st_rules *rule = NULL;
	struct nss_udp_st_rules *n = NULL;
	int count = 0;
	int ret;

	pr_info("UDP-ST: Creating PPE flows for all rules (VP dev=%s vp_num=%d)...\n",
		g_udp_st_ppe_ctx.vp_dev ? g_udp_st_ppe_ctx.vp_dev->name : "NULL",
		g_udp_st_ppe_ctx.vp_num);

	if (nust.config.rate && nust.rule_count > 0) {
		pr_info("UDP-ST: Total rate=%u Mbps will be divided among %u flows (%u Mbps per flow)\n",
			nust.config.rate, nust.rule_count, nust.config.rate / nust.rule_count);
	}

	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {
		if (count >= NSS_UDP_ST_PPE_MAX_FLOWS) {
			pr_warn("UDP-ST: Maximum MAX flow limit (%d) reached, stopping flow creation\n",
				NSS_UDP_ST_PPE_MAX_FLOWS);
			break;
		}

		if (rule->ip_version & NSS_UDP_ST_FLAG_IPV4) {
			ret = nss_udp_st_ppe_create_flow_v4(rule, dir);
		} else if (rule->ip_version & NSS_UDP_ST_FLAG_IPV6) {
			ret = nss_udp_st_ppe_create_flow_v6(rule, dir);
		} else {
			pr_err("UDP-ST: invalid rule type at rule %d\n", count);
			ret = -EINVAL;
		}

		if (ret < 0) {
			struct nss_udp_st_rules *pos;

			pr_err("UDP-ST: flow creation failed at rule %d, rolling back %d created flow(s)\n",
			       count, count);
			list_for_each_entry(pos, &nust.rules.list, list) {
				if (pos == rule)
					break;
				nss_udp_st_destroy_ppe_flow(pos);
			}
			return -EINVAL;
		}
		count++;
	}

	pr_info("UDP-ST: PPE flow creation complete: %d succeeded\n", count);

	return 0;
}

/*
 * nss_udp_st_destroy_ppe_flow()
 *	Destroy PPE flow rule and associated policer for UDP-ST (IPv4 and IPv6).
 */
void nss_udp_st_destroy_ppe_flow(struct nss_udp_st_rules *rules)
{
	ppe_drv_ret_t ret;

	if (rules->ip_version & NSS_UDP_ST_FLAG_IPV4) {
		struct ppe_drv_v4_rule_destroy destroy;

		memset(&destroy, 0, sizeof(destroy));
		destroy.tuple.protocol = IPPROTO_UDP;
		destroy.tuple.flow_ip = rules->sip.ip.ipv4;
		destroy.tuple.flow_ident = rules->sport;
		destroy.tuple.return_ip = rules->dip.ip.ipv4;
		destroy.tuple.return_ident = rules->dport;

		ret = ppe_drv_v4_destroy(&destroy);
		if (ret != PPE_DRV_RET_SUCCESS) {
			pr_err("Failed to destroy PPE IPv4 flow for UDP-ST: %d\n", ret);
		} else {
			pr_info("PPE IPv4 flow destroyed for UDP-ST: %pI4:%u -> %pI4:%u\n",
				&rules->sip.ip.ipv4, rules->sport,
				&rules->dip.ip.ipv4, rules->dport);
		}
	} else if (rules->ip_version & NSS_UDP_ST_FLAG_IPV6) {
		struct ppe_drv_v6_rule_destroy destroy;

		memset(&destroy, 0, sizeof(destroy));
		destroy.tuple.protocol = IPPROTO_UDP;
		nss_udp_st_ppe_ipv6_addr_fill(rules->sip.ip.ipv6, destroy.tuple.flow_ip);
		destroy.tuple.flow_ident = rules->sport;
		nss_udp_st_ppe_ipv6_addr_fill(rules->dip.ip.ipv6, destroy.tuple.return_ip);
		destroy.tuple.return_ident = rules->dport;

		ret = ppe_drv_v6_destroy(&destroy);
		if (ret != PPE_DRV_RET_SUCCESS) {
			pr_err("Failed to destroy PPE IPv6 flow for UDP-ST: %d\n", ret);
		} else {
			pr_info("PPE IPv6 flow destroyed for UDP-ST\n");
		}
	}

	/*
	 * Destroy the per-flow policer (after flow is destroyed)
	 */
	if (rules->policer_ctx) {
		pr_info("UDP-ST: Destroying per-flow policer rule_id=%u hw_id=%u\n",
			rules->policer_rule_id,
			ppe_drv_policer_get_policer_id((struct ppe_drv_policer_acl *)rules->policer_ctx));
		ppe_drv_policer_acl_destroy((struct ppe_drv_policer_acl *)rules->policer_ctx);
		rules->policer_ctx = NULL;
		rules->policer_rule_id = 0;
	}
}

/*
 * nss_udp_st_ppe_query_flow_stats_v4()
 *	Query PPE hardware counters for IPv4 flow.
 *	For RX the PPE flow tuple is reversed (dip/dport as flow, sip/sport as return).
 */
static int nss_udp_st_ppe_query_flow_stats_v4(struct nss_udp_st_rules *rule,
					       int64_t *delta_pkts,
					       nss_udp_st_ppe_dir_t dir)
{
	struct ppe_drv_v4_flow_conn_stats conn_stats;
	ppe_drv_ret_t ret;

	memset(&conn_stats, 0, sizeof(conn_stats));
	conn_stats.tuple.protocol = IPPROTO_UDP;

	if (dir == NSS_UDP_ST_PPE_TX_DIR) {
		conn_stats.tuple.flow_ip     = rule->sip.ip.ipv4;
		conn_stats.tuple.flow_ident  = rule->sport;
		conn_stats.tuple.return_ip   = rule->dip.ip.ipv4;
		conn_stats.tuple.return_ident = rule->dport;
	} else {
		conn_stats.tuple.flow_ip     = rule->dip.ip.ipv4;
		conn_stats.tuple.flow_ident  = rule->dport;
		conn_stats.tuple.return_ip   = rule->sip.ip.ipv4;
		conn_stats.tuple.return_ident = rule->sport;
	}

	ret = ppe_drv_v4_get_conn_stats(&conn_stats);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pr_debug("UDP-ST PPE: ppe_drv_v4_get_conn_stats failed ret=%d"
			 " for %pI4:%u -> %pI4:%u\n",
			 ret,
			 &rule->sip.ip.ipv4, rule->sport,
			 &rule->dip.ip.ipv4, rule->dport);
		return -EINVAL;
	}

	*delta_pkts = conn_stats.conn_sync.flow_rx_packet_count;
	return 0;
}

/*
 * nss_udp_st_ppe_query_flow_stats_v6()
 *	Query PPE hardware counters for IPv6 flow.
 *	For RX the PPE flow tuple is reversed (dip/dport as flow, sip/sport as return).
 */
static int nss_udp_st_ppe_query_flow_stats_v6(struct nss_udp_st_rules *rule,
					       uint64_t *delta_pkts,
					       nss_udp_st_ppe_dir_t dir)
{
	struct ppe_drv_v6_flow_conn_stats conn_stats;
	ppe_drv_ret_t ret;

	memset(&conn_stats, 0, sizeof(conn_stats));
	conn_stats.tuple.protocol = IPPROTO_UDP;

	if (dir == NSS_UDP_ST_PPE_TX_DIR) {
		nss_udp_st_ppe_ipv6_addr_fill(rule->sip.ip.ipv6, conn_stats.tuple.flow_ip);
		conn_stats.tuple.flow_ident  = rule->sport;
		nss_udp_st_ppe_ipv6_addr_fill(rule->dip.ip.ipv6, conn_stats.tuple.return_ip);
		conn_stats.tuple.return_ident = rule->dport;
	} else {
		nss_udp_st_ppe_ipv6_addr_fill(rule->dip.ip.ipv6, conn_stats.tuple.flow_ip);
		conn_stats.tuple.flow_ident  = rule->dport;
		nss_udp_st_ppe_ipv6_addr_fill(rule->sip.ip.ipv6, conn_stats.tuple.return_ip);
		conn_stats.tuple.return_ident = rule->sport;
	}

	ret = ppe_drv_v6_get_conn_stats(&conn_stats);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pr_debug("UDP-ST PPE: ppe_drv_v6_get_conn_stats failed ret=%d\n", ret);
		return -EINVAL;
	}

	*delta_pkts = conn_stats.conn_sync.flow_rx_packet_count;
	return 0;
}

/*
 * nss_udp_st_ppe_throughput_work_fn()
 *	Periodic PPE stats poll (every NSS_UDP_ST_PPE_POLL_MS ms).
 */
static void nss_udp_st_ppe_throughput_work_fn(struct work_struct *work)
{
	struct nss_udp_st_rules *pos, *n;
	struct ppe_drv_policer_hw_stats pol_stats = {};
	int64_t total_delta_pkts = 0;
	int64_t drop_delta_pkts = 0;
	int64_t delta_pkts = 0;
	nss_udp_st_ppe_dir_t ppe_dir = (nust.dir == NSS_UDP_ST_TX) ?
					NSS_UDP_ST_PPE_TX_DIR : NSS_UDP_ST_PPE_RX_DIR;

	if (nust.mode != NSS_UDP_ST_START)
		return;

	/*
	 * Query PPE hardware counters for every active flow and accumulate
	 * per-flow policer drop deltas in the same pass.
	 * ppe_drv_v4/v6_get_conn_stats returns delta (clears on read).
	 */
	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		/*
		 * TX only: accumulate policer-dropped packet count so we can
		 * subtract them from total_delta_pkts below.
		 * rpc is cumulative; subtract the stored baseline for the delta.
		 */
		if (pos->policer_ctx) {
			pol_stats.drv_ctx.acl_ctx =
				(struct ppe_drv_policer_acl *)pos->policer_ctx;
			ppe_drv_policer_acl_get_hw_stats(&pol_stats,
					(struct ppe_drv_policer_acl *)pos->policer_ctx,
					PPE_DRV_POLICER_DIRECTION_US);
			drop_delta_pkts += pol_stats.hw_cntrs.rpc - pos->policer_prev_rpc;
			pos->policer_prev_rpc = pol_stats.hw_cntrs.rpc;
		}

		if (pos->ip_version & NSS_UDP_ST_FLAG_IPV4) {
			if (nss_udp_st_ppe_query_flow_stats_v4(pos,
							       &delta_pkts,
							       ppe_dir) == 0) {
				total_delta_pkts += delta_pkts;
			}
		} else if (pos->ip_version & NSS_UDP_ST_FLAG_IPV6) {
			if (nss_udp_st_ppe_query_flow_stats_v6(pos,
							       &delta_pkts,
							       ppe_dir) == 0) {
				total_delta_pkts += delta_pkts;
			}
		}

	}

	/*
	 * For TX, total_delta_pkts includes packets seen at VP ingress before
	 * the policer drops them. Subtract policer drops to get only the
	 * packets that made it to the wire.
	 * For RX there is no policer so drop_delta_pkts is always 0.
	 */
	total_delta_pkts -= drop_delta_pkts;

	if (total_delta_pkts > 0) {
		nss_udp_st_update_stats(
			(size_t)(nust.config.buffer_sz + sizeof(struct ethhdr)),
			total_delta_pkts);
	}

	queue_delayed_work(ppe_stats_wq, to_delayed_work(work),
			   msecs_to_jiffies(NSS_UDP_ST_PPE_POLL_MS));
}

/*
 * nss_udp_st_ppe_reset_policer_stats()
 *	Snapshot each flow's current cumulative rpc so the next test run
 *	starts from a correct baseline (no stale drops from a prior run).
 */
void nss_udp_st_ppe_reset_policer_stats(void)
{
	struct nss_udp_st_rules *pos;
	struct ppe_drv_policer_hw_stats pol_stats = {};

	list_for_each_entry(pos, &nust.rules.list, list) {
		if (!pos->policer_ctx)
			continue;

		pol_stats.drv_ctx.acl_ctx =
			(struct ppe_drv_policer_acl *)pos->policer_ctx;
		ppe_drv_policer_acl_get_hw_stats(&pol_stats,
				(struct ppe_drv_policer_acl *)pos->policer_ctx,
				PPE_DRV_POLICER_DIRECTION_US);
		pos->policer_prev_rpc = pol_stats.hw_cntrs.rpc;
	}
}

/*
 * nss_udp_st_ppe_throughput_timer_start()
 *	Create the per-second PPE throughput stats workqueue
 */
bool nss_udp_st_ppe_throughput_timer_start(void)
{
	if (ppe_stats_wq) {
		pr_warn("UDP-ST PPE: stats timer already running\n");
		return true;
	}

	ppe_stats_wq = create_singlethread_workqueue("udpst_ppe_st");
	if (!ppe_stats_wq) {
		pr_err("UDP-ST PPE: failed to create stats workqueue\n");
		return false;
	}

	INIT_DELAYED_WORK(&ppe_stats_work, nss_udp_st_ppe_throughput_work_fn);

	/*
	 * Snapshot each flow's current cumulative rpc so the first poll
	 * delta starts from the correct baseline (no stale hardware counts).
	 */
	nss_udp_st_ppe_reset_policer_stats();

	/*
	 * Start the stats update with 1 packet so that there is lesser
	 */
	if (nust.dir == NSS_UDP_ST_TX) {
		nss_udp_st_update_stats(nust.config.buffer_sz + sizeof(struct ethhdr), 1);
	}

	queue_delayed_work(ppe_stats_wq, &ppe_stats_work,
			   msecs_to_jiffies(NSS_UDP_ST_PPE_POLL_MS));

	pr_info("UDP-ST PPE: stats timer started (%dms interval)\n",
		NSS_UDP_ST_PPE_POLL_MS);
	return true;
}

/*
 * nss_udp_st_ppe_throughput_timer_stop()
 *	Destroy the workqueue for PPE stats.
 */
void nss_udp_st_ppe_throughput_timer_stop(void)
{
	if (!ppe_stats_wq)
		return;

	cancel_delayed_work_sync(&ppe_stats_work);
	destroy_workqueue(ppe_stats_wq);
	ppe_stats_wq = NULL;

	pr_info("UDP-ST PPE: stats timer stopped\n");
}
