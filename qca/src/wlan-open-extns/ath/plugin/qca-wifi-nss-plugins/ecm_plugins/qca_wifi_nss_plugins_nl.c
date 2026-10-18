/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <linux/wireless.h>
#include <net/genetlink.h>
#include <net/netevent.h>
#include <nl80211.h>
#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ecm.h>
#include <ecm_interface_nl.h>
#include <mac80211/ath/qca-vendor.h>

#define QCA_WIFI_NSS_PLUGINS_NL_NL80211_MC_GROUP_INVALID_ID       -1
#define QCA_WIFI_NSS_PLUGINS_NL_GENEL_MESSAGE_SIZE                4096

#define QCA_WIFI_NSS_PLUGINS_NL_QM_TYPE_SCS_REQUEST          0

#define QCA_WIFI_NSS_PLUGINS_NL_QM_SCS_REQ_ADD               0
#define QCA_WIFI_NSS_PLUGINS_NL_QM_SCS_REQ_REMOVE            1
#define QCA_WIFI_NSS_PLUGINS_NL_QM_SCS_REQ_CHANGE            2

/*
 * Wifi event handler structure.
 */
struct qca_wifi_nss_plugins_nl_event {
	struct task_struct *thread;
	struct socket *sock;
};

static struct qca_wifi_nss_plugins_nl_event __ewn;

/*
 * qca_wifi_nss_plugins_nl_event_rx()
 *	Receive netlink message from socket
 */
static int qca_wifi_nss_plugins_nl_event_rx(struct socket *sock, struct sockaddr_nl *addr, unsigned char *buf, int len)
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
 * qca_wifi_nss_plugins_nl_qos_mgmt_event()
 *	Parse and process qos mgmt events received from Wi-Fi.
 */
void qca_wifi_nss_plugins_nl_qos_mgmt_event(struct nlmsghdr *nlh, int cmd)
{
	u8 peer_mac[ETH_ALEN];
	u8 wifi_qm_type;
	int rem_qm_desc;
	struct genlmsghdr *gnlh;
	struct nlattr *tb_qos_mgmt_desc;
	struct nlattr *tb_qos_mgmt_desc_entry[NL80211_QM_DESC_ATTR_MAX + 1];
	struct nlattr **attrs = NULL;
	int err;
	uint8_t wifi_qm_id;
	uint8_t request_type;
	struct nlattr **tb_qos_mgmt = NULL;

	gnlh = nlmsg_data(nlh);

	qca_wifi_nss_plugins_trace("Received NL80211_CMD_QOS_MGMT");

	attrs = (struct nlattr **)kzalloc((sizeof(struct nlattr *) * (NL80211_ATTR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
	if (!attrs) {
		qca_wifi_nss_plugins_warning("%px: Not able to allocate array to parse the events \n", nlh);
		return;
	}

	tb_qos_mgmt = (struct nlattr **)kzalloc((sizeof(struct nlattr *) * (NL80211_ATTR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
	if (!tb_qos_mgmt) {
		qca_wifi_nss_plugins_warning("%px: Not able to allocate array to parse the events \n", nlh);
		kfree(attrs);
		return;
	}

	/*
	 * Parse the top-level nl80211 attributes into attrs
	 */
	err = nla_parse(attrs, NL80211_ATTR_MAX,
			nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), NULL,
			NULL);

	if (err) {
		qca_wifi_nss_plugins_warning("nla_parse failed: %d\n", err);
		goto end;
	}

	if (!attrs[NL80211_ATTR_QOS_MGMT]) {
		qca_wifi_nss_plugins_warning("NL80211_ATTR_QOS_MGMT is NULL: %d\n", err);
		goto end;
	}

	err = nla_parse_nested(tb_qos_mgmt, NL80211_QM_ATTR_MAX,
			 attrs[NL80211_ATTR_QOS_MGMT],
			 NULL, NULL);

	if (err) {
		qca_wifi_nss_plugins_warning("nla_parse_nested failed: %d\n", err);
		goto end;
	}

	if (!tb_qos_mgmt[NL80211_QM_ATTR_MAC_ADDR] ||
	    !tb_qos_mgmt[NL80211_QM_ATTR_QM_TYPE] ||
	    !tb_qos_mgmt[NL80211_QM_ATTR_DESCRIPTOR_PARAMS]) {
		qca_wifi_nss_plugins_warning("error parsing mac addr, qm_type and descriptor params\n");
		goto end;
	}

	ether_addr_copy(peer_mac, nla_data(tb_qos_mgmt[NL80211_QM_ATTR_MAC_ADDR]));

	wifi_qm_type = nla_get_u8(tb_qos_mgmt[NL80211_QM_ATTR_QM_TYPE]);

	qca_wifi_nss_plugins_trace("peer mac : %pM qm_type : %d\n", peer_mac, wifi_qm_type);

	if (wifi_qm_type != QCA_WIFI_NSS_PLUGINS_NL_QM_TYPE_SCS_REQUEST)
		goto end;

	nla_for_each_nested(tb_qos_mgmt_desc, tb_qos_mgmt[NL80211_QM_ATTR_DESCRIPTOR_PARAMS], rem_qm_desc) {
		err = nla_parse_nested(tb_qos_mgmt_desc_entry, NL80211_QM_DESC_ATTR_MAX, tb_qos_mgmt_desc, NULL, NULL);
		if (err) {
			qca_wifi_nss_plugins_warning("nla_parse_nested failed: %d\n", err);
			goto end;
		}

		/*
		 * Extract QM desc attributes
		 */
		if (!tb_qos_mgmt_desc_entry[NL80211_QM_DESC_ATTR_REQUEST_TYPE]) {
			qca_wifi_nss_plugins_warning("qm type is NULL\n");
			goto end;
		}

		if (!tb_qos_mgmt_desc_entry[NL80211_QM_DESC_ATTR_QM_ID]) {
			qca_wifi_nss_plugins_warning("qm id is NULL\n");
			goto end;
		}

		request_type = nla_get_u8(tb_qos_mgmt_desc_entry[NL80211_QM_DESC_ATTR_REQUEST_TYPE]);
		if (request_type == QCA_WIFI_NSS_PLUGINS_NL_QM_SCS_REQ_REMOVE ||
		    request_type == QCA_WIFI_NSS_PLUGINS_NL_QM_SCS_REQ_CHANGE) {
			wifi_qm_id = nla_get_u8(tb_qos_mgmt_desc_entry[NL80211_QM_DESC_ATTR_QM_ID]);
			qca_wifi_nss_plugins_trace("Defunct connections mac : %pM wifi_qm_type : %d wifi_qm_id : %d\n", peer_mac, wifi_qm_type, wifi_qm_id);
			ecm_interface_defunct_qm_connections(&peer_mac[0], wifi_qm_type, wifi_qm_id);
		} else if (request_type == QCA_WIFI_NSS_PLUGINS_NL_QM_SCS_REQ_ADD) {
		/*
		 * TODO: parse 5 tuple and mask from tclass element and defunct based on those
		 */
			ecm_interface_node_connections_defunct_by_mac_addr(&peer_mac[0]);
		}
	}

end:
	kfree(tb_qos_mgmt);
	kfree(attrs);
}

/*
 * qca_wifi_nss_plugins_nl_vendor_cmd_handle()
 *	Parse and process events within the vendor cmd received from Wi-Fi.
 */
static void qca_wifi_nss_plugins_nl_vendor_cmd_handle(struct nlmsghdr *nlh)
{
	struct nlattr **tb, **tb2;
	uint8_t mac[ETH_ALEN], newlink;
	int *subcmd;
	int res, len;
	struct nlattr *data;

	if (nlh->nlmsg_len < nlmsg_msg_size(GENL_HDRLEN)) {
		qca_wifi_nss_plugins_warning("%px: Invalid NL response message header length \n", nlh);
		return;
	}

	tb = (struct nlattr **)kzalloc((sizeof(struct nlattr *) * (NL80211_ATTR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
	if (!tb) {
		qca_wifi_nss_plugins_warning("%px: Not able to allocate array to parse the events \n", nlh);
		return;
	}

	/*
	 * Parse the event coming from Wi-Fi.
	 */
	res = nla_parse(tb, NL80211_ATTR_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), NULL, NULL);

	if (res < 0) {
		qca_wifi_nss_plugins_warning("%px: Error in parsing the Wi-Fi event \n", nlh);
		kfree(tb);
		return;
	}

	if (!tb[NL80211_ATTR_VENDOR_DATA]) {
		qca_wifi_nss_plugins_warning("%px: Not able to parse vendor data attribute.\n", nlh);
		goto free_mem;
	}

	subcmd = (int *) nla_data(tb[NL80211_ATTR_VENDOR_SUBCMD]);
	qca_wifi_nss_plugins_info("Netlink parsed sub command: %u\n", *subcmd);

	switch (*subcmd) {
	case QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE:
		/*
		 * Get the mac addr of last primary link and defunct all the connections by mac addr.
		 */
		tb2 = (struct nlattr **) kzalloc((sizeof(struct nlattr *) * (QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
		if (!tb2) {
			qca_wifi_nss_plugins_warning("%px: Not able to allocate array to parse the events \n", nlh);
			kfree(tb);
			return;
		}

		data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
		len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);
		if (nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX, (struct nlattr *) data, len, NULL, NULL)) {
			qca_wifi_nss_plugins_warning("%px: Error in parsing the Wi-Fi event \n", nlh);
			kfree(tb2);
			goto free_mem;
		}

		newlink = nla_get_u8(tb2[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID]);
		memcpy(mac, nla_data(tb2[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR]), ETH_ALEN);
		ecm_interface_node_connections_defunct_by_mac_addr((uint8_t *)mac);
		qca_wifi_nss_plugins_info("Deleted all entries corresponding to mac: %pM new link id: %u\n", mac, newlink);
		kfree(tb2);
		break;

	case QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG:
		tb2 = (struct nlattr **) kzalloc((sizeof(struct nlattr *) * (QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
		if (!tb2) {
			qca_wifi_nss_plugins_warning("%px: Not able to allocate array to parse the events \n", nlh);
			kfree(tb);
			return;
		}

		data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
		len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);
		if (nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX, (struct nlattr *) data, len, NULL, NULL)) {
			qca_wifi_nss_plugins_warning("%px: Error in parsing the Wi-Fi event \n", nlh);
			kfree(tb2);
			goto free_mem;
		}

		memcpy(mac, nla_data(tb2[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR]), ETH_ALEN);
		ecm_interface_node_connections_defunct_by_mac_addr((uint8_t *)mac);
		qca_wifi_nss_plugins_info("Deleted all entries corresponding to mac: %pM\n", mac);
		kfree(tb2);
		break;

	default:
		qca_wifi_nss_plugins_info("Netlink parsed sub command: %u\n", *subcmd);
		break;
	}
free_mem:
	kfree(tb);
}

/*
 * qca_wifi_nss_plugins_nl_process_link_events()
 *	Parse and process link add / delete events received from Wi-Fi.
 */
static void qca_wifi_nss_plugins_nl_process_link_events(struct nlmsghdr *nlh, int cmd)
{
	struct nlattr **tb;
	uint8_t mac[ETH_ALEN];
	int res;

	if (nlh->nlmsg_len < nlmsg_msg_size(GENL_HDRLEN)) {
		qca_wifi_nss_plugins_warning("%px: Invalid NL response message header length \n", nlh);
		return;
	}

	tb = (struct nlattr **)kzalloc((sizeof(struct nlattr *) * (NL80211_ATTR_MAX + 1)), GFP_ATOMIC | __GFP_NOWARN);
	if (!tb) {
		qca_wifi_nss_plugins_warning("%px: Not able to allocate array to parse the events \n", nlh);
		return;
	}

	/*
	 * Parse the event coming from Wi-Fi.
	 */
	res = nla_parse(tb, NL80211_ATTR_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), NULL, NULL);

	if (res < 0) {
		qca_wifi_nss_plugins_warning("%px: Error in parsing the Wi-Fi event \n", nlh);
		kfree(tb);
		return;
	}

	/*
	 * Get the MAC address of the peer and process the event.
	 */
	if (tb[NL80211_ATTR_MAC]) {
		ether_addr_copy(mac, nla_data(tb[NL80211_ATTR_MAC]));

		if (cmd == NL80211_CMD_NEW_STATION) {
			qca_wifi_nss_plugins_info("STA %pM joining\n", (uint8_t *)mac);
			ecm_interface_node_connections_defunct_by_type_sta_join((uint8_t *)mac);
		}

		if (cmd == NL80211_CMD_DEL_STATION) {
			qca_wifi_nss_plugins_info("STA %pM leaving\n", (uint8_t *)mac);
			ecm_interface_node_connections_defunct_by_mac_addr((uint8_t *)mac);
		}
	}

	kfree(tb);
}

/*
 * qca_wifi_nss_plugins_nl_event_handler()
 *	Netlink event handler
 */
static int qca_wifi_nss_plugins_nl_event_handler(void *buf, int len)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *hdr;
	int left;

	nlh = (struct nlmsghdr *)buf;
	left = len;

	/*
	 * Check the command type and parse the message accordingly.
	 */
	while (NLMSG_OK(nlh, left)) {
		hdr = NLMSG_DATA(nlh);

		switch (hdr->cmd) {
		case NL80211_CMD_NEW_STATION:
		case NL80211_CMD_DEL_STATION:
			qca_wifi_nss_plugins_nl_process_link_events(nlh, hdr->cmd);
			break;

		case NL80211_CMD_QOS_MGMT:
			qca_wifi_nss_plugins_nl_qos_mgmt_event(nlh, hdr->cmd);
			break;

		case NL80211_CMD_VENDOR:
			qca_wifi_nss_plugins_nl_vendor_cmd_handle(nlh);
			break;

		}

		nlh = NLMSG_NEXT(nlh, left);
	}

	return 0;
}

/*
 * qca_wifi_nss_plugins_nl_genl_ctrl_response()
 *	Parse and process the generic control family response message,
 *	get the mcast id of MLME/Vendor mcast group of nl80211 family and
 *	set the membership of the mcast group to the given socket.
 */
static bool qca_wifi_nss_plugins_nl_genl_ctrl_response(struct nlmsghdr *nlh, struct socket *sock)
{
	struct nlattr *tb[CTRL_ATTR_MAX+1];
	struct nlattr *mcgrp;
	char data[16];
	int mcast_id = QCA_WIFI_NSS_PLUGINS_NL_NL80211_MC_GROUP_INVALID_ID;
	int family_id;
	int res = -1;
	int i;

	if (nlh->nlmsg_len < nlmsg_msg_size(GENL_HDRLEN)) {
		qca_wifi_nss_plugins_warning("%px: Invalid NL response message header length \n", nlh);
		return false;
	}

	res = nla_parse(tb, CTRL_ATTR_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), NULL, NULL);
	if (res < 0) {
		qca_wifi_nss_plugins_warning("%px: Error in parsing NL message %d err\n", nlh, res);
		return false;
	}

	/*
	 * Get the family ID for nl80211 family.
	 */
	if (!tb[CTRL_ATTR_FAMILY_ID]) {
		qca_wifi_nss_plugins_info("%px: Failed to get the family ID of nl80211 \n", nlh);
		return false;
	}

	if (!tb[CTRL_ATTR_MCAST_GROUPS]) {
		qca_wifi_nss_plugins_warning("%px: Failed to fetch the multicast groups \n", nlh);
		return false;
	}

	/*
	 * Parse the multicast groups and get the ID of MLME/Vendor group.
	 */
	family_id = nla_get_u16(tb[CTRL_ATTR_FAMILY_ID]);
	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], i) {
		struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];

		res = nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, (struct nlattr *)nla_data(mcgrp), nla_len(mcgrp), NULL, NULL);
		if (res < 0) {
			qca_wifi_nss_plugins_warning("%px: Error in parsing NL message multicast group %d res\n", nlh, res);
			return false;
		}

		if (!tb2[CTRL_ATTR_MCAST_GRP_NAME]) {
			qca_wifi_nss_plugins_info("%px: Multicast group name not resolved.\n", nlh);
			continue;
		}

		nla_strscpy(data, tb2[CTRL_ATTR_MCAST_GRP_NAME], sizeof(data));

		if (!strcmp(data, "vendor") && !strcmp(data, "mlme")) {
			continue;
		}

		/*
		 * Set the membership to socket.
		 */
		if (!tb2[CTRL_ATTR_MCAST_GRP_ID]) {
			qca_wifi_nss_plugins_warning("%px: Parsed mcast id is invalid.\n", sock);
			return false;
		}

		mcast_id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
		/*
		 * Add this socket as a memeber to the MLME or Vendor multicast group of the
		 * nl80211 family.
		 */
		res = sock->ops->setsockopt(sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, KERNEL_SOCKPTR((void *)&mcast_id), sizeof(mcast_id));
		if (res < 0) {
			qca_wifi_nss_plugins_warning("%px: Failed to set the multicast membership %s(%d) res %d\n", sock, data, mcast_id, res);
			return false;
		}

		qca_wifi_nss_plugins_info("%px: Added the socket as a member to nl80211 %s(%d) multicast group \n", sock, data, mcast_id);
	}

	return true;
}

/*
 * qca_wifi_nss_plugins_nl_construct_message()
 *	Construct a message to generic control family to resolve the
 *	nl80211 multicast groups.
 */
struct nlmsghdr *qca_wifi_nss_plugins_nl_construct_message(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *ghdr;
	int res = 0;

	/*
	 * Construct the nl header with command addressing to control
	 * family.
	 */
	nlh = nlmsg_put(skb, 0, 0, GENL_ID_CTRL, GENL_HDRLEN, 0);
	if (!nlh) {
		qca_wifi_nss_plugins_warning("%px: Error in constructing nl header !\n", skb);
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
	 * Add the family name attribute that ECM wants to resolve.
	 */
	res = nla_put_string(skb, CTRL_ATTR_FAMILY_NAME, "nl80211");
	if (res) {
		qca_wifi_nss_plugins_warning("%px: Failed to put family name attribute \n", skb);
		goto err;
	}

	nlh->nlmsg_len = skb->len;

	return nlh;
err:
	genlmsg_cancel(skb, ghdr);

	return NULL;
}

/*
 * qca_wifi_nss_plugins_nl_resolve_nl80211_family()
 *	Resolve the nl80211 family multicast groups to
 *	receive Wi-Fi specific events.
 */
int qca_wifi_nss_plugins_nl_resolve_nl80211_family(struct socket *sock, struct sockaddr_nl *addr)
{
	struct nlmsghdr *nlh;
	struct kvec iov = {0};
	struct msghdr mhdr = {0};
	unsigned char *buf;
	struct sk_buff *skb;
	int flags = GFP_ATOMIC;
	int len = QCA_WIFI_NSS_PLUGINS_NL_GENEL_MESSAGE_SIZE;
	int ret = -1;
	int size = 0;

	/*
	 * Allocate a genl message structure to send a resolution
	 * request to generic control family.
	 */
	skb = genlmsg_new(NLMSG_DEFAULT_SIZE, flags);
	if (!skb) {
		qca_wifi_nss_plugins_warning("Not enough space to allocate genl message !\n");
		return -ENOBUFS;
	}

	/*
	 * Construct the NL message to generic control family to resolve the
	 * nl80211 multicast groups.
	 */
	nlh = (struct nlmsghdr *)qca_wifi_nss_plugins_nl_construct_message(skb);
	if (!nlh) {
		qca_wifi_nss_plugins_warning("%px: Failed to construct the NL message\n", sock);
		nlmsg_free(skb);
		return ret;
	}

	/*
	 * Fill the message buffer and send the control message.
	 */
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	mhdr.msg_name = 0;
	mhdr.msg_namelen = 0;

	iov_iter_kvec(&mhdr.msg_iter, WRITE, &iov, 1, iov.iov_len);

	ret = sock_sendmsg(sock, &mhdr);
	if (ret < 0) {
		qca_wifi_nss_plugins_warning("%px: Failed to send the NL ctrl message\n", sock);
		nlmsg_free(skb);
		return ret;
	}

	/*
	 * Allocate a buffer to receive the reply message from
	 * generic control family having information about the
	 * multicast groups of nl80211.
	 */
	buf = (char *)kzalloc(len, GFP_ATOMIC | __GFP_NOWARN);
	if (!buf) {
		qca_wifi_nss_plugins_warning("%px: Failed to allocate a buffer to receive message!\n", sock);
		nlmsg_free(skb);
		return -1;
	}

	size = qca_wifi_nss_plugins_nl_event_rx(sock, addr, buf, len);
	if (size < 0) {
		qca_wifi_nss_plugins_warning("%px: Netlink RX error !\n", sock);
		nlmsg_free(skb);
		kfree(buf);
		return -1;
	}

	/*
	 * Parse and process the NL message response received from kernel.
	 * This has all the information about the family, its multicast groups,
	 * callbacks etc.
	 */
	nlh = (struct nlmsghdr *)buf;
	while (NLMSG_OK(nlh, size)) {
		qca_wifi_nss_plugins_info("%px: Received an NL response, length %d type %d\n", nlh, nlh->nlmsg_len, nlh->nlmsg_type);

		if (!qca_wifi_nss_plugins_nl_genl_ctrl_response(nlh, sock)) {
			qca_wifi_nss_plugins_warning("%px: Failed to parse and process the multicast group message.\n", sock);
			nlmsg_free(skb);
			kfree(buf);
			return -1;
		}

		nlh = NLMSG_NEXT(nlh, size);
	}

	/*
	 * Release the NL msg and buffer allocated for receiving the message.
	 */
	nlmsg_free(skb);
	kfree(buf);
	return 0;
}

/*
 * qca_wifi_nss_plugins_nl_event_thread()
 */
static void qca_wifi_nss_plugins_nl_event_thread(void)
{
	int err;
	int size;
	struct sockaddr_nl saddr;
	unsigned char *buf;
	int len = QCA_WIFI_NSS_PLUGINS_NL_GENEL_MESSAGE_SIZE;

	kernel_sigaction(SIGKILL, SIG_DFL);

	/*
	 * Create a socket to listen to the events coming from nl80211 family.
	 */
	err = sock_create(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC, &__ewn.sock);
	if (err < 0) {
		qca_wifi_nss_plugins_err("failed to create sock err %d\n", err);
		goto exit1;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.nl_family = AF_NETLINK;
	saddr.nl_pid    = current->pid;

	err = __ewn.sock->ops->bind(__ewn.sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr));
	if (err < 0) {
		qca_wifi_nss_plugins_err("failed to bind sock err %d\n", err);
		goto exit2;
	}

	/*
	 * ECM is supposed to listen to the multicast events sent from the nl80211 family.
	 * So resolve the family and the multicast event.
	 */
	err = qca_wifi_nss_plugins_nl_resolve_nl80211_family(__ewn.sock, &saddr);
	if (err < 0) {
		qca_wifi_nss_plugins_err("Failed to resolve the nl80211 generic netlink family err %d\n", err);
		goto exit2;
	}

	buf = (char *)kzalloc(len, GFP_ATOMIC | __GFP_NOWARN);
	if (!buf) {
		qca_wifi_nss_plugins_err("Failed to allocate the buffer %d\n", err);
		goto exit2;
	}

	/*
	 * Start listening to the Wi-Fi events.
	 */
	qca_wifi_nss_plugins_info("qca_wifi_nss_plugins_nl_event thread started\n");
	while (!kthread_should_stop()) {
		size = qca_wifi_nss_plugins_nl_event_rx(__ewn.sock, &saddr, buf, len);
		qca_wifi_nss_plugins_trace("got a netlink msg with len %d\n", size);

		if (signal_pending(current))
			break;

		if (size < 0) {
			qca_wifi_nss_plugins_warning("netlink rx error\n");
		} else {
			qca_wifi_nss_plugins_nl_event_handler((void *)buf, size);
		}
	}

	kfree(buf);
	qca_wifi_nss_plugins_info("qca_wifi_nss_plugins_nl_event thread stopped\n");
exit2:
	sock_release(__ewn.sock);
exit1:
	__ewn.sock = NULL;
}

/*
 * qca_wifi_nss_plugins_nl_event_start()
 */
int qca_wifi_nss_plugins_nl_event_start(void)
{
	if (__ewn.thread) {
		return 0;
	}

	__ewn.thread = kthread_run((void *)qca_wifi_nss_plugins_nl_event_thread, NULL, "ECM_wifi_event");
	if (IS_ERR(__ewn.thread)) {
		qca_wifi_nss_plugins_err("Unable to start kernel thread\n");
		return -ENOMEM;
	}

	return 0;
}

/*
 * qca_wifi_nss_plugins_nl_event_stop()
 */
int qca_wifi_nss_plugins_nl_event_stop(void)
{
	int err;

	if (__ewn.thread == NULL || __ewn.sock == NULL) {
		return 0;
	}

	qca_wifi_nss_plugins_info("kill qca_wifi_nss_plugins_nl_event thread\n");

	send_sig(SIGKILL, __ewn.thread, 1);
	 if(__ewn.sock != NULL) {
		qca_wifi_nss_plugins_info("Stopping kthread.\n");
		err = kthread_stop(__ewn.thread);
		__ewn.thread = NULL;
		qca_wifi_nss_plugins_info("Stopped kthread.\n");
	}

	qca_wifi_nss_plugins_info("killing qca_wifi_nss_plugins_nl_event thread succeeded.\n");
	return err;
}
