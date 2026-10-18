/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/if_ether.h>
#include <linux/igmp.h>
#include <linux/random.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <net/ip.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_bridge.h>
#include <net/genetlink.h>
#include <linux/nl80211.h>
#include "mc_osdep.h"
#include "mc_private.h"
#include "mc_netlink.h"
#include "mc_snooping.h"
#include "mc_forward.h"
#include <linux/in6.h>

struct sta_leave_event {
	struct task_struct *thread;
	struct socket *sock;
};

#define MCS_NL80211_MC_GROUP_INVALID_ID	-1
#define MC_MESSAGE_SIZE		4096
#define OUI_QCA 0x001374
#ifdef WLAN_FEATURE_VBSS
#include <cfg80211_external.h>
static const struct nla_policy
vbss_req_policy[QCA_WLAN_VENDOR_ATTR_VBSS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_VBSS_HW_LINK_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_INFO] = {.type = NLA_NESTED },
};

static const struct nla_policy
vbss_ops_info_policy[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_INFO_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_VBSS_STA_MAC_ADDR] = {.type = NLA_BINARY,
	.len = ETH_ALEN },
	[QCA_WLAN_VENDOR_ATTR_VBSS_STA_TX_PN] = {.type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_VBSS_STA_PER_TID_RX_CTX] = {.type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_VBSS_STA_PER_TID_SN] = {.type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_VBSS_STA_NUM_SN_TIDS] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_VBSS_STA_TX_GSN] = {.type = NLA_U64 },
};
#endif
static struct sta_leave_event __ewn;

/*
 * mc_event_recv()
 *	Receive netlink message from socket
 */
static int mc_event_recv(struct socket *sock, struct sockaddr_nl *addr, unsigned char *buf, int len)
{
	struct msghdr msg;
	struct kvec iov;

	iov.iov_base = buf;
	iov.iov_len  = len;

	msg.msg_flags = 0;
	msg.msg_name  = addr;
	msg.msg_namelen = sizeof(struct sockaddr_nl);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	return kernel_recvmsg(sock, &msg, &iov, 1, len, msg.msg_flags);
}

/*
 * mc_sta_leave_event()
 *	Delete the mac entry of sta from bridge snooping table
 */
void mc_sta_leave_event(struct net_device *dev,__u8 *from_mac)
{

	struct mc_struct *mc;
	struct net_device *pdev;
	/*
	 * netlink message handler expects rtnl_lock to be taken
	 */
	rtnl_lock();

	/*
	 *Get the master (upper) network device of the given interface.
	 */
	pdev = netdev_master_upper_dev_get(dev);
	if (!pdev) {
		rtnl_unlock();
		printk(KERN_DEBUG "%s: STA leave received for interface without a master \n", __func__);
		return;
	}

	if (!netif_is_bridge_master(pdev)) {
		rtnl_unlock();
		printk(KERN_DEBUG "%s: pdev is not a bridge netdevice\n", __func__);
		return;
	}

	mc = MC_DEV(pdev);
	if (!mc) {
		rtnl_unlock();
		printk(KERN_DEBUG "%s: MC_DEV returned NULL for device %s\n", __func__, pdev->name);
		return;
	}

	MC_PRINT("%s: STA leave event received on port %s for MAC %pM\n", __func__, dev->name, from_mac);
	/*
	 * Iterate over the multicast database to find groups containing this port
	 */
	for (int i = 0; i < MC_HASH_SIZE; i++) {
		struct mc_mdb_entry *mdb;
		struct hlist_node *mdbh;
		struct mc_fdb_group *fg;

		/*Iterate over all multicast group entries*/
		os_hlist_for_each_entry_rcu(mdb, mdbh, &mc->hash[i], hlist) {
			struct mc_port_group *pg;
			struct hlist_node *pgh;

			if (hlist_empty(&mdb->pslist))
				continue;

			/*Iterate over all port group entries*/
			os_hlist_for_each_entry_rcu(pg, pgh, &mdb->pslist, pslist) {
				if (hlist_empty(&pg->fslist) || pg->port != dev){
					continue;
				}

				/*Delete entries*/
				fg = mc_fdb_group_find(&pg->fslist, from_mac);
				if (fg) {
					mc_fdb_group_destroy(fg);
					MC_PRINT(KERN_DEBUG" mc fdb entry is deleted\n");
					break;
				}
			}
		}
	}
	rtnl_unlock();
}

/*
 * mc_process_sta_leave_event()
 * 	Parse and process sta_leave event
 */
void mc_process_sta_leave_event(struct nlmsghdr *nlh, int cmd)
{
	struct nlattr **tb = NULL;
	uint8_t mac[ETH_ALEN] = {0};
	struct net_device *dev;
	u_int32_t ifidx;
	int res;

	if (nlh->nlmsg_len < nlmsg_msg_size(GENL_HDRLEN)) {
		printk(KERN_DEBUG "%px: Invalid NL response message header length\n", nlh);
		return;
	}
	tb = (struct nlattr **)kzalloc((sizeof(struct nlattr *) * (NL80211_ATTR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
	if (!tb) {
		printk(KERN_DEBUG"%px: Not able to allocate array to parse the events \n", nlh);
		return;
	}

	/*
	 * Parse the event coming from Wi-Fi.
	 */
	res = nla_parse(tb, NL80211_ATTR_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), NULL, NULL);
	if (res < 0) {
		printk(KERN_DEBUG"%px: Error in parsing the Wi-Fi event \n", nlh);
		goto free;
	}

	/*
	 * Get the MAC address of the peer and process the event.
	 */
	if (tb[NL80211_ATTR_MAC])
		memcpy(mac, nla_data(tb[NL80211_ATTR_MAC]), ETH_ALEN);
	else
		goto free;

	if (tb[NL80211_ATTR_IFINDEX])
		ifidx = *(u_int32_t *)nla_data(tb[NL80211_ATTR_IFINDEX]);
	else
		goto free;

	dev = dev_get_by_index(&init_net, ifidx);
	if (!dev) {
		printk(KERN_WARNING "Invalid network device index: %u\n", ifidx);
		goto free;
	}

	mc_sta_leave_event(dev,mac);
	dev_put(dev);
free:
	kfree(tb);
}

#ifdef WLAN_FEATURE_VBSS
/*
 * mc_vbss_txrx_stopped_event()
 *	Wrapper API for mc_sta_leave_event()
 */
void mc_vbss_txrx_stopped_event(struct net_device *dev,__u8 *from_mac){

	mc_sta_leave_event(dev, from_mac);
}

/* sk_buff *mc_vbss_ipv6_alloc_query()
 * 	Wrapper for mc_ipv6_alloc_query to set
 * 	response interval for query to 100 milliseconds
 */
static struct sk_buff *mc_vbss_ipv6_alloc_query(struct mc_struct *mc, struct in6_addr *group, u8 *mac, int is_v2){

	struct sk_buff *skb;
	struct ipv6hdr *ip6h;
	/* Setting the query_response_interval as 100 milliseconds for vbss because
	 * for unicast query, respose should be immediate
	 */
	unsigned long interval = 0.1 * HZ; // 100 milliseconds
	void *icmp6;
	skb = mc_ipv6_alloc_query(mc, group, mac, is_v2);

	if (!skb)
		return NULL;

	ip6h = ipv6_hdr(skb);
	icmp6 = icmp6_hdr(skb);
	if (is_v2) {
		struct mld2_query *mld2q = (struct mld2_query *)icmp6;
		mld2q->mld2q_mrc = htons((u16)jiffies_to_msecs(interval));
		mld2q->mld2q_cksum = 0;
		mld2q->mld2q_cksum = csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr, sizeof(*mld2q), IPPROTO_ICMPV6, csum_partial(mld2q, sizeof(*mld2q), 0));
        } else {
		struct mld_msg *mldq = (struct mld_msg *)icmp6;
		mldq->mld_maxdelay = htons((u16)jiffies_to_msecs(interval));
		mldq->mld_cksum = 0;
		mldq->mld_cksum = csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr, sizeof(*mldq), IPPROTO_ICMPV6, csum_partial(mldq, sizeof(*mldq), 0));
	}

	return skb;
}

/* sk_buff *mc_vbss_ipv4_alloc_query()
 * 	Wrapper for mc_ipv4_alloc_query to set
 * 	response interval for query to 100 milliseconds
 */
static struct sk_buff *mc_vbss_ipv4_alloc_query(struct mc_struct *mc, __be32 group, __u8 *mac, int is_v3){

	struct sk_buff *skb;
	struct iphdr *iph;
	void *igmp;
	/* Setting the query_response_interval as 100 milliseconds for vbss because
	 * for unicast query, respose should be immediate
	 */
	unsigned int interval = 0.1 * HZ / IGMP_TIMER_SCALE; // 100 millisecond in IGMP units
							     //
	skb = mc_ipv4_alloc_query(mc, group, mac, is_v3);
	if (!skb)
		return NULL;

	iph = ip_hdr(skb);
	igmp = igmp_hdr(skb);

	if (is_v3) {
		struct igmpv3_query *ih3 = (struct igmpv3_query *)igmp;
		ih3->code = interval;
		ih3->csum = 0;
		ih3->csum = ip_compute_csum((void *)ih3, sizeof(struct igmpv3_query));

	} else {
		struct igmphdr *ih = (struct igmphdr *)igmp;
		ih->code = interval;
		ih->csum = 0;
		ih->csum = ip_compute_csum((void *)ih, sizeof(struct igmphdr));

	}

	return skb;
}

/*
 * mc_VBSS_resume_event_v6()
 * 	VBSS resume event triggered: preparing to send MLDv1 query
 * 	Constructing and sending IPv6 multicast query to the resumed client
 */
void mc_vbss_txrx_started_event_v6(struct mc_struct *mc, void *port, __u8 *mac){
	struct sk_buff *skb;

	if (!mc)
		return;

	skb = mc_vbss_ipv6_alloc_query(mc, (struct in6_addr *)&in6addr_any, mac, 0);
	if (!skb){
		MC_PRINT(KERN_DEBUG "Failed to allocate skb for ipv6 query\n");
		return;
	}
	skb->dev = port;
	MC_PRINT("%s: Send query to port %s to client %pM \n", __func__,  skb->dev->name, mac);
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, dev_net(skb->dev), NULL, skb, NULL, skb->dev, br_dev_queue_push_xmit);
}

/*
 * mc_VBSS_resume_event_v4()
 * 	VBSS resume event triggered: preparing to send IGMPv2 query
 * 	Constructing and sending IPv4 multicast query to the resumed client
 */
void mc_vbss_txrx_started_event_v4(struct mc_struct *mc, void *port, __u8 *mac){
	struct sk_buff *skb;

	if (!mc)
		return;
	skb = mc_vbss_ipv4_alloc_query(mc, 0, mac, 0);

	if (!skb){
		MC_PRINT(KERN_DEBUG "Failed to allocate skb for ipv4 query\n");
		return;
	}

	skb->dev = port;
	MC_PRINT("%s: Send query to port %s to client %pM \n", __func__,  skb->dev->name, mac);
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, dev_net(skb->dev), NULL, skb, NULL, skb->dev, br_dev_queue_push_xmit);
}

/*
 * mc_vbss_txrx_started_event()
 * 	Parse and process VBSS resume event
 */
void mc_vbss_txrx_started_event(struct mc_struct *mc, struct net_device *dev,__u8 *from_mac){

	MC_PRINT("%s: TX/RX started event received on port %s for MAC %pM\n", __func__, dev->name, from_mac);
	mc_vbss_txrx_started_event_v4(mc, dev, from_mac);

	mc_vbss_txrx_started_event_v6(mc, dev, from_mac);

}

/* mc_handle_vbss_event()
 * 	Handles the vbss events
 */
void mc_handle_vbss_event(struct net_device *dev, u8 *data, size_t len){

	struct mc_struct *mc;
	struct net_device *pdev;

        /*
         * Added rtnl_lock() to prevent assertion errors
         */
        rtnl_lock();
        /*
         *Get the master (upper) network device of the given interface.
         */
        pdev = netdev_master_upper_dev_get(dev);
        if (!pdev) {
                rtnl_unlock();
                printk(KERN_DEBUG "%s: VBSS event received for interface without a master \n", __func__);
                return;
        }

        if (!netif_is_bridge_master(pdev)) {
                rtnl_unlock();
                printk(KERN_DEBUG "%s: pdev is not a bridge netdevice\n", __func__);
                return;
        }

        mc = MC_DEV(pdev);
        if (!mc) {
                rtnl_unlock();
                printk(KERN_DEBUG "%s: MC_DEV returned NULL for device %s\n", __func__, pdev->name);
                return;
        }

	rtnl_unlock();

	MC_PRINT(KERN_DEBUG "MCS: Processing VBSS event");
	uint8_t mac[ETH_ALEN];
	uint8_t opid;
	struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_VBSS_MAX + 1] = {0};
	struct nlattr *attr_tb[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_INFO_MAX + 1] = {0};

	if (nla_parse(attr, QCA_WLAN_VENDOR_ATTR_VBSS_MAX, (const struct nlattr *)data, len, vbss_req_policy, NULL)) {
		MC_PRINT(KERN_DEBUG "MCS: Failed to parse top-level attributes\n");
		return;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_ID]){
		MC_PRINT(KERN_DEBUG "MCS: Operation ID attribute missing\n");
		return;
	}
	opid = nla_get_u8(attr[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_ID]);

	if (opid != QCA_WLAN_VENDOR_VBSS_OPS_NOTIFY_STA_TXRX_STOPPED && opid != QCA_WLAN_VENDOR_VBSS_OPS_NOTIFY_STA_TXRX_STARTED){
		MC_PRINT(KERN_DEBUG "MCS: Unsupported operation ID value: %u\n", opid);
		return;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_INFO]){
		MC_PRINT(KERN_DEBUG "MCS: Operation info attribute missing\n");
		return;
	}

	if (nla_parse_nested(attr_tb, QCA_WLAN_VENDOR_ATTR_VBSS_OPS_INFO_MAX,attr[QCA_WLAN_VENDOR_ATTR_VBSS_OPS_INFO],vbss_ops_info_policy, NULL)){
		MC_PRINT(KERN_DEBUG "MCS: Failed to parse nested operation info attributes\n");
		return;
	}

	if (!attr_tb[QCA_WLAN_VENDOR_ATTR_VBSS_STA_MAC_ADDR]){
		MC_PRINT(KERN_DEBUG "MCS: Station MAC address attribute missing\n");
		return;
	}

	/*
	 * Get the MAC address of the peer and process the event.
	 */
	memcpy(mac, nla_data(attr_tb[QCA_WLAN_VENDOR_ATTR_VBSS_STA_MAC_ADDR]), ETH_ALEN);
	if (opid == QCA_WLAN_VENDOR_VBSS_OPS_NOTIFY_STA_TXRX_STOPPED){
		MC_PRINT(KERN_DEBUG "MCS: Handling TX/RX stopped event\n");
		mc_vbss_txrx_stopped_event(dev, mac);
	}
	else if(opid == QCA_WLAN_VENDOR_VBSS_OPS_NOTIFY_STA_TXRX_STARTED){
		MC_PRINT(KERN_DEBUG "MCS: Handling TX/RX started event\n");
		mc_vbss_txrx_started_event(mc, dev, mac);
	}
}
#endif

/* mc_process_vendor_event()
 *	Process the vendor events
 */
void mc_process_vendor_event(struct nlmsghdr *nlh, int cmd)
{
	struct net_device *dev;
	u_int32_t ifidx;
	int res;
	u_int32_t vendor_id;
	u_int32_t sub_cmd;
	struct nlattr **tb=NULL;

	if (nlh->nlmsg_len < nlmsg_msg_size(GENL_HDRLEN)) {
		printk(KERN_DEBUG "%px: Invalid NL response message header length\n", nlh);
		return;
	}

	/*
	 * Parse the event coming from Wi-Fi.
	 */
	tb = (struct nlattr **)kzalloc((sizeof(struct nlattr *) * (NL80211_ATTR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
	if (!tb) {
    		printk(KERN_DEBUG"Failed to allocate memory for nlattr array\n");
    		return;
	}

	res = nla_parse(tb, NL80211_ATTR_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), NULL, NULL);
	if (res < 0) {
		printk(KERN_DEBUG"%px: Error in parsing the Wi-Fi event \n", nlh);
		goto free;
	}

	/*
	 * Get the vendor ID
 	 */
	if (!tb[NL80211_ATTR_VENDOR_ID]){
		printk(KERN_DEBUG "Vendor ID attribute missing\n");
		goto free;
	}

	vendor_id = nla_get_u32(tb[NL80211_ATTR_VENDOR_ID]);
	if (vendor_id != OUI_QCA){
		printk(KERN_DEBUG "Unsupported vendor ID: 0x%x\n", vendor_id);
		goto free;
	}

	if (!tb[NL80211_ATTR_IFINDEX]) {
		printk(KERN_DEBUG "Interface index attribute missing\n");
		goto free;
	}

	ifidx = *(u_int32_t *)nla_data(tb[NL80211_ATTR_IFINDEX]);
	dev = dev_get_by_index(&init_net, ifidx);
	if (!dev) {
		printk(KERN_WARNING "Invalid network device index: %u\n", ifidx);
		goto free;
	}

	if (tb[NL80211_ATTR_VENDOR_SUBCMD])
		sub_cmd = nla_get_u32(tb[NL80211_ATTR_VENDOR_SUBCMD]);
	else
		goto out;

#ifdef WLAN_FEATURE_VBSS
	if (sub_cmd == QCA_NL80211_VENDOR_SUBCMD_VBSS_DATA) {
		u8 *data = NULL;
		size_t len = 0;
		if (tb[NL80211_ATTR_VENDOR_DATA]) {
			data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
			len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);
		}

		mc_handle_vbss_event(dev, data, len);
	}
#endif
out:
	dev_put(dev);
free:
	kfree(tb);

	return;
}

/*
 * mc_event_handler()
 *	Netlink event handler
 */
static int mc_event_handler(void *buf, int len)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *hdr;
	int left;

	nlh = (struct nlmsghdr *) buf;
	left = len;
	/*
	 * Check the command type and parse the message accordingly.
	 */
	while (NLMSG_OK(nlh, left)) {
		hdr = NLMSG_DATA(nlh);
		switch (hdr->cmd) {
		case NL80211_CMD_DEL_STATION:
			printk(KERN_DEBUG "MCS: Processing station leave event\n");
			mc_process_sta_leave_event(nlh, hdr->cmd);
			break;

		case NL80211_CMD_VENDOR:
			mc_process_vendor_event(nlh, hdr->cmd);
			break;
		}
		nlh = NLMSG_NEXT(nlh, left);
	}

	return 0;
}

/*
 * mc_get_event_id()
 * 	Parse the generic control family response message and return
 * 	the multicast id of MLME and VENDOR multicast groups of nl80211 family.
 */
int mc_get_event_id(struct nlmsghdr *nlh, const char *event_name, int *event_id)
{
    struct nlattr *tb[CTRL_ATTR_MAX+1];
    struct nlattr *mcgrp;
    char data[16];
    int res = -1;
    int i;

    res = nla_parse(tb, CTRL_ATTR_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
                    nlmsg_attrlen(nlh, GENL_HDRLEN), NULL, NULL);
    if (res < 0) {
        printk(KERN_WARNING"%px: Error in parsing NL message %d err\n", nlh, res);
        return res;
    }

    if (!tb[CTRL_ATTR_MCAST_GROUPS]) {
        printk(KERN_WARNING"%px: Failed to fetch the multicast groups \n", nlh);
        return -1;
    }

    nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], i) {
        struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];

        res = nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, (struct nlattr *)nla_data(mcgrp), nla_len(mcgrp), NULL, NULL);
        if (res < 0) {
            printk(KERN_WARNING"%px: Error in parsing NL message multicast group %d res\n", nlh, res);
            return res;
        }

        if (tb2[CTRL_ATTR_MCAST_GRP_NAME]) {
            nla_strscpy(data, tb2[CTRL_ATTR_MCAST_GRP_NAME], sizeof(data));
            if (strcmp(data, event_name) == 0) {
                if (tb2[CTRL_ATTR_MCAST_GRP_ID]) {
                    *event_id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
                    printk(KERN_INFO"%px: Successfully fetched the %s group ID %d\n", nlh, event_name, *event_id);
                    return 0;
                }
            }
        }
    }

    printk(KERN_WARNING"%px: Failed to get the %s group from ctrl msg response \n", nlh, event_name);

    return -1;
}

/*
 * mc_parse_genl_ctrl_response()
 *	Get the multicast id of MLME and VENDOR multicast groups of nl80211 family.
 */

int mc_parse_genl_ctrl_response(struct nlmsghdr *nlh, int *mlme_id, int *vendor_id)
{
	int res_mlme, res_vendor;

	res_mlme = mc_get_event_id(nlh, "mlme", mlme_id);
	if (res_mlme < 0) {
		printk(KERN_WARNING "MLME group not found\n");
		*mlme_id = MCS_NL80211_MC_GROUP_INVALID_ID;
	}

	res_vendor = mc_get_event_id(nlh, "vendor", vendor_id);
	if (res_vendor < 0) {
		printk(KERN_WARNING "Vendor group not found\n");
		*vendor_id = MCS_NL80211_MC_GROUP_INVALID_ID;
	}

	if (res_mlme < 0 && res_vendor < 0) {
	        printk(KERN_ERR "Both MLME and Vendor multicast groups not found\n");
	        return -1;
	}
	if (res_mlme >= 0)
		printk(KERN_DEBUG"MLME group found");

	if (res_vendor >= 0)
		printk(KERN_DEBUG"VENDOR group found");

	return 0;
}

/*
 * mc_construct_nl_message()
 *	Construct a message to generic control family to resolve the
 *	nl80211 multicast groups.
 */
struct nlmsghdr *mc_construct_nl_message(void)
{
	int flags = GFP_ATOMIC;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct genlmsghdr *ghdr;
	int res = 0;

	/*
	 * Allocate a genl message structure to send a resolution
	 * request to generic control family.
	 */
	skb = genlmsg_new(NLMSG_DEFAULT_SIZE, flags);
	if (!skb) {
		printk(KERN_WARNING"Not enough space to allocate genl message !\n");
		return NULL;
	}

	/*
	 * Construct the nl header with command addressing to control
	 * family.
	 */
	nlh = nlmsg_put(skb, 0, 0, GENL_ID_CTRL, GENL_HDRLEN, 0);
	if (!nlh) {
		printk(KERN_WARNING"%px: Error in constructing nl header !\n", skb);
		nlmsg_free(skb);
		return NULL;
	}

	/*
	 * Construct the genl header.
	 */
	nlh->nlmsg_flags |= NLM_F_REQUEST;

	ghdr = nlmsg_data(nlh);
	ghdr->cmd = CTRL_CMD_GETFAMILY;
	ghdr->version = 1;
	ghdr->reserved = 0;

	/*
	 * Add the family name attribute need to resolve
	 */
	res = nla_put_string(skb, CTRL_ATTR_FAMILY_NAME, "nl80211");
	if (res) {
		printk(KERN_WARNING"%px: Failed to put family name attribute \n", skb);
		goto err;
	}

	nlh->nlmsg_len = skb->len;

	return nlh;
err:
	genlmsg_cancel(skb, ghdr);
	nlmsg_free(skb);

	return NULL;
}

/*
 *  mc_resolve_nl80211_family()
 *	Resolve the nl80211 family multicast groups to
 *	receive Wi-Fi specific events.
 */
int mc_resolve_nl80211_family(struct socket *sock, struct sockaddr_nl *addr)
{
	struct nlmsghdr *nlh;
	struct kvec iov = {0};
	struct msghdr mhdr = {0};
	unsigned char *buf;
	int mlme_id = MCS_NL80211_MC_GROUP_INVALID_ID;
	int vendor_id = MCS_NL80211_MC_GROUP_INVALID_ID;
	int len = MC_MESSAGE_SIZE;
	int ret = -1;
	int size = 0;

	/*
	 * Construct the NL message to generic control family to resolve the
	 * nl80211 multicast groups.
	 */
	nlh = (struct nlmsghdr *)mc_construct_nl_message();
	if (!nlh) {
		printk(KERN_WARNING"%px: Failed to construct the NL message\n", sock);
		return ret;
	}

	/*
	 * Fill the message buffer and send the control message.
	 */
	iov.iov_base = (void *) nlh;
	iov.iov_len = nlh->nlmsg_len;

	mhdr.msg_name = 0;
	mhdr.msg_namelen = 0;

	iov_iter_kvec(&mhdr.msg_iter, WRITE, &iov, 1, iov.iov_len);

	ret = sock_sendmsg(sock, &mhdr);
	if (ret < 0) {
		printk(KERN_WARNING"%px: Failed to send the NL ctrl message\n", sock);
		return ret;
	}

	/*
	 * Allocate a buffer to receive the reply message from
	 * generic control family having information about the
	 * multicast groups of nl80211.
	 */
	buf = (char *)kzalloc(len, GFP_ATOMIC | __GFP_NOWARN);
	if (!buf) {
		printk(KERN_WARNING"%px: Failed to allocate a buffer to receive message!\n", sock);
		return -1;
	}

	size = mc_event_recv(sock, addr, buf, len);
	if (size < 0) {
		printk(KERN_WARNING"%px: Netlink RX error !\n", sock);
		kfree(buf);
		return size;
	}

	/*
	 * Parse the NL message response received from kernel.
	 * This has all the information about the family, its multicast groups,
	 * callbacks etc.
	 */
	nlh = (struct nlmsghdr *)buf;
	while (NLMSG_OK(nlh, size)) {
		printk(KERN_DEBUG"%px: Received an NL response, length %d type %d\n", nlh, nlh->nlmsg_len, nlh->nlmsg_type);
		ret = mc_parse_genl_ctrl_response(nlh, &mlme_id, &vendor_id);
		if (ret < 0) {
			printk(KERN_WARNING"%px: Failed to parse the multicast group message %d\n", sock, ret);
			kfree(buf);
			return ret;
		}

		nlh = NLMSG_NEXT(nlh, size);
	}

	/*
	 * Release the buffer allocated for receiving the message.
	 */
	kfree(buf);

	/*
	 * Add this socket as a memeber to the MLME and VENDOR multicast groups of the
	 * nl80211 family.
	 */
	if (mlme_id != MCS_NL80211_MC_GROUP_INVALID_ID) {
		ret = sock->ops->setsockopt(sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, KERNEL_SOCKPTR((void *)&mlme_id), sizeof(mlme_id));
		if (ret < 0) {
			printk(KERN_WARNING"%px: Failed to set the multicast membership %d ret %d\n", sock, mlme_id, ret);
			return ret;
		}

		printk(KERN_DEBUG"%px: Adding a this sokcet as a member to NL80211 MLME multicast group %d \n", sock, mlme_id);
	}

	if (vendor_id != MCS_NL80211_MC_GROUP_INVALID_ID) {
		ret = sock->ops->setsockopt(sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, KERNEL_SOCKPTR((void *)&vendor_id), sizeof(vendor_id));
		if (ret < 0) {
			printk(KERN_WARNING"%px: Failed to set the multicast membership %d ret %d\n", sock, vendor_id, ret);
			return ret;
		}

		printk(KERN_DEBUG"%px: Adding a this sokcet as a member to NL80211 VENDOR multicast group %d \n", sock, vendor_id);
	}

	return 0;
}

/*
 * mc_event_thread()
 *     This function creates a socket to listen for events from the nl80211 family,
 *     resolves the netlink family, allocates a buffer, and processes incoming events in a loop.
 */
static void mc_event_thread(void)
{
	int err;
	int size;
	struct sockaddr_nl saddr;
	unsigned char *buf;
	int len = MC_MESSAGE_SIZE;

	kernel_sigaction(SIGKILL, SIG_DFL);

	/*
	 * Create a socket to listen to the events coming from nl80211 family.
	 */
	err = sock_create(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC, &__ewn.sock);
	if (err < 0) {
		printk(KERN_ERR"failed to create sock err %d\n", err);
		goto exit1;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.nl_family = AF_NETLINK;
	saddr.nl_pid    = current->pid;

	err = __ewn.sock->ops->bind(__ewn.sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr));
	if (err < 0) {
		printk(KERN_ERR"failed to bind sock err %d\n", err);
		goto exit2;
	}

	/*
	 * We need to listen to the multicast events sent from the nl80211 family.
	 * So resolve the family and the multicast event.
	 */
	err = mc_resolve_nl80211_family(__ewn.sock, &saddr);
	if (err < 0) {
		printk(KERN_ERR"Failed to resolve the nl80211 generic netlink family err %d\n", err);
		goto exit2;
	}

	buf = (char *)kzalloc(len, GFP_ATOMIC | __GFP_NOWARN);
	if (!buf) {
		printk(KERN_ERR"Failed to allocate the buffer %d\n", err);
		goto exit2;
	}

	/*
	 * Start listening to the Wi-Fi events.
	 */
	printk(KERN_DEBUG"mc_event thread started\n");
	while (!kthread_should_stop()) {
		size = mc_event_recv(__ewn.sock, &saddr, buf, len);

		if (signal_pending(current))
			break;

		if (size < 0) {
			printk(KERN_WARNING"netlink rx error\n");
		} else {
			mc_event_handler((void *)buf, size);
		}
	}

	kfree(buf);
	printk(KERN_DEBUG"mc_event thread stopped\n");
exit2:
	sock_release(__ewn.sock);
exit1:
	__ewn.sock = NULL;
}

/*
 * mc_event_start()
 */
int mc_event_start(void)
{
	if (__ewn.thread) {
		return 0;
	}

	__ewn.thread = kthread_run((void *)mc_event_thread, NULL, "mc_event");
	if (IS_ERR(__ewn.thread)) {
		int err = PTR_ERR(__ewn.thread);
		printk(KERN_ERR "Unable to start kernel thread, error code: %d\n", err);
		return err;
	}

	return 0;
}

/*
 * mc_event_stop()
 */
int mc_event_stop(void)
{
	int err;

	if (__ewn.thread == NULL || __ewn.sock == NULL) {
		return 0;
	}

	printk(KERN_DEBUG"kill mc_event thread\n");
	send_sig(SIGKILL, __ewn.thread, 1);
	printk(KERN_DEBUG"Stopping kthread.\n");
	err = kthread_stop(__ewn.thread);
	__ewn.thread = NULL;
	printk(KERN_DEBUG"Stopped kthread.\n");

	return err;
}

