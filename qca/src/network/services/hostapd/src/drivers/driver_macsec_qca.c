/*
 * Wired Ethernet driver interface for QCA MACsec driver
 * Copyright (c) 2005-2009, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004, Gunter Burchardt <tira@isx.de>
 * Copyright (c) 2013-2014, Qualcomm Atheros, Inc.
 * Copyright (c) 2019, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <inttypes.h>
#ifdef __linux__
#include <netpacket/packet.h>
#include <net/if_arp.h>
#include <net/if.h>
#endif /* __linux__ */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#include <net/if_dl.h>
#include <net/if_media.h>
#endif /* defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__) */
#ifdef __sun__
#include <sys/sockio.h>
#endif /* __sun__ */

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/defs.h"
#include "common/ieee802_1x_defs.h"
#include "common/eapol_common.h"
#include "pae/ieee802_1x_kay.h"
#include "driver.h"
#include "driver_wired_common.h"
#ifdef HOSTAPD
#include "ap/sta_info.h"
#include "ap/ieee802_11_auth.h"
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#endif /* HOSTAPD */

#include "nss_macsec_secy.h"
#include "nss_macsec_secy_rx.h"
#include "nss_macsec_secy_tx.h"

#define MAXSC 16

#define SAK_128_LEN	16
#define SAK_256_LEN	32

/* TCI field definition */
#define TCI_ES                0x40
#define TCI_SC                0x20
#define TCI_SCB               0x10
#define TCI_E                 0x08
#define TCI_C                 0x04

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

struct channel_map {
	struct ieee802_1x_mka_sci sci;
};

struct macsec_qca_data {
	struct driver_wired_common_data common;

	int use_pae_group_addr;
	u32 secy_id;

	/* shadow */
	bool always_include_sci;
	bool use_es;
	bool use_scb;
	bool protect_frames;
	bool replay_protect;
	u32 replay_window;

#ifdef HOSTAPD
	/* ioctl sock */
	int ioctl_sock;
	/* genl sock */
	struct nl_cb *nl_cb;
	struct nl_sock *nl_event;
	struct nl_sock *nl;
	int genl_id;

	/* authorize_policy: 0 port-based authorize, 1 mac-based authorize
	 * used to backward compatibility with the macsec case where is wired
	 * point to point connection and once MKA success, the whole port is
	 * opened and all src mac traffic are encrypted. */
	u8 authorize_policy;
#endif /* HOSTAPD */

	struct channel_map receive_channel_map[MAXSC];
	struct channel_map transmit_channel_map[MAXSC];
};

#ifdef HOSTAPD
#define SW_SWITCH_IOCTL_DEV_NAME "/dev/switch_ssdk"
#define SW_API_ACL_MAC_ENTRY_SET 428
typedef struct
{
	u8 addr[ETH_ALEN];
	char ifname[IFNAMSIZ];
	u8 acl_policy; /* 0 deny, 1 accept */
} swMacEntry;

#define SSDK_GENL_FAMILY_NAME "ssdk_nl_family"
#define SSDK_GENL_MCAST_GRP_NAME "ssdk_nl_mcast"
enum ssdk_attrs {
       SSDK_ATTR_UNSPEC,
       SSDK_ATTR_IFNAME, /* interface name */
       SSDK_ATTR_MACADDR, /* mac address */
       SSDK_ATTR_MACPOLL, /* macpoll: 1 enable, 0 disable */
       SSDK_ATTR_MAX,
};
enum ssdk_nl_commands {
       SSDK_COMMAND_NEW_MAC,
       SSDK_COMMAND_EXPIRE_MAC,
       SSDK_COMMAND_POLL_MAC,
       SSDK_COMMAND_MAX,
};

static int switch_command_excute(void *priv, u32 api_id, int nrParam, ...)
{
	unsigned long nValue[12] = { 0 };
	unsigned long nRtn = 0;
	int idx;
	va_list vaArgs;
	struct macsec_qca_data *drv = priv;

	if (drv->ioctl_sock < 0) {
		wpa_printf(MSG_ERROR, "IOCTL interface is not opened");
		return -1;
	}
	nValue[0] = (unsigned long)api_id;
	nValue[1] = (unsigned long)&nRtn;

	if (nrParam > sizeof(nValue)/sizeof(unsigned long) - 2)
		nrParam = sizeof(nValue)/sizeof(unsigned long) - 2;

	va_start(vaArgs, nrParam);

	for (idx = 0; idx < nrParam; idx++) {
		nValue[idx + 2] = va_arg(vaArgs, unsigned long);
	}

	va_end(vaArgs);

	(void)ioctl(drv->ioctl_sock, SIOCDEVPRIVATE, nValue);

	return nRtn;
}

static int switch_set_mac_rule(void *priv, swMacEntry *entry)
{
	return switch_command_excute(priv, SW_API_ACL_MAC_ENTRY_SET, 2, 0, entry);
}

static int
macsec_qca_set_sta_acl_policy(void *priv, const u8 *addr, u8 acl_policy)
{
	struct macsec_qca_data *drv = priv;
	swMacEntry entry = {0};
	wpa_printf(MSG_DEBUG, "%s: addr=" MACSTR " acl_policy=%d",
		   __func__, MAC2STR(addr), acl_policy);

	if (drv->authorize_policy == 0) {
		/* port-based authorize */
		os_memset(entry.addr, 0xff, ETH_ALEN);
	}
	if (drv->authorize_policy == 1) {
		/* mac-based authorize */
		os_memcpy(entry.addr, addr, ETH_ALEN);
	}
	os_strlcpy(entry.ifname, drv->common.ifname, IFNAMSIZ);
	entry.acl_policy = acl_policy;

	return switch_set_mac_rule(drv, &entry);
}
#endif /* HOSTAPD */

static int macsec_qca_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	os_memset(capa, 0, sizeof(*capa));
	capa->flags = WPA_DRIVER_FLAGS_WIRED;
	capa->max_acl_mac_addrs = 8;
	return 0;
}

static void __macsec_drv_init(struct macsec_qca_data *drv)
{
	int ret = 0;
	fal_rx_ctl_filt_t rx_ctl_filt;
	fal_tx_ctl_filt_t tx_ctl_filt;

	wpa_printf(MSG_INFO, "%s: secy_id=%d", __func__, drv->secy_id);

	/* Enable Secy and Let EAPoL bypass */
	ret = nss_macsec_secy_en_set(drv->secy_id, true);
	if (ret)
		wpa_printf(MSG_ERROR, "nss_macsec_secy_en_set: FAIL");

	ret = nss_macsec_secy_sc_sa_mapping_mode_set(drv->secy_id,
						     FAL_SC_SA_MAP_1_4);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "nss_macsec_secy_sc_sa_mapping_mode_set: FAIL");

	os_memset(&rx_ctl_filt, 0, sizeof(rx_ctl_filt));
	rx_ctl_filt.bypass = 1;
	rx_ctl_filt.match_type = IG_CTL_COMPARE_ETHER_TYPE;
	rx_ctl_filt.match_mask = 0xffff;
	rx_ctl_filt.ether_type_da_range = 0x888e;
	ret = nss_macsec_secy_rx_ctl_filt_set(drv->secy_id, 0, &rx_ctl_filt);
	if (ret)
		wpa_printf(MSG_ERROR, "nss_macsec_secy_rx_ctl_filt_set: FAIL");

	os_memset(&tx_ctl_filt, 0, sizeof(tx_ctl_filt));
	tx_ctl_filt.bypass = 1;
	tx_ctl_filt.match_type = EG_CTL_COMPARE_ETHER_TYPE;
	tx_ctl_filt.match_mask = 0xffff;
	tx_ctl_filt.ether_type_da_range = 0x888e;
	ret = nss_macsec_secy_tx_ctl_filt_set(drv->secy_id, 0, &tx_ctl_filt);
	if (ret)
		wpa_printf(MSG_ERROR, "nss_macsec_secy_tx_ctl_filt_set: FAIL");
}


static void __macsec_drv_deinit(struct macsec_qca_data *drv)
{
	nss_macsec_secy_en_set(drv->secy_id, false);
	nss_macsec_secy_rx_sc_del_all(drv->secy_id);
	nss_macsec_secy_tx_sc_del_all(drv->secy_id);
}


#ifdef __linux__

static void macsec_qca_handle_data(void *ctx, unsigned char *buf, size_t len)
{
#ifdef HOSTAPD
	struct ieee8023_hdr *hdr;
	u8 *pos, *sa;
	size_t left;
	union wpa_event_data event;

	/* at least 6 bytes src macaddress, 6 bytes dst macaddress
	 * and 2 bytes ethertype
	*/
	if (len < 14) {
		wpa_printf(MSG_MSGDUMP,
			   "macsec_qca_handle_data: too short (%lu)",
			   (unsigned long) len);
		return;
	}
	hdr = (struct ieee8023_hdr *) buf;

	switch (ntohs(hdr->ethertype)) {
	case ETH_P_PAE:
		wpa_printf(MSG_MSGDUMP, "Received EAPOL packet");
		sa = hdr->src;
		os_memset(&event, 0, sizeof(event));
		event.new_sta.addr = sa;
		wpa_supplicant_event(ctx, EVENT_NEW_STA, &event);

		pos = (u8 *) (hdr + 1);
		left = len - sizeof(*hdr);
		drv_event_eapol_rx(ctx, sa, pos, left);
		break;
	default:
		wpa_printf(MSG_DEBUG, "Unknown ethertype 0x%04x in data frame",
			   ntohs(hdr->ethertype));
		break;
	}
#endif /* HOSTAPD */
}


static void macsec_qca_handle_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	int len;
	unsigned char buf[3000];

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: recv: %s", strerror(errno));
		return;
	}

	macsec_qca_handle_data(eloop_ctx, buf, len);
}

#endif /* __linux__ */


static int macsec_qca_init_sockets(struct macsec_qca_data *drv, u8 *own_addr)
{
#ifdef __linux__
	struct ifreq ifr;
	struct sockaddr_ll addr;

	drv->common.sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PAE));
	if (drv->common.sock < 0) {
		wpa_printf(MSG_ERROR, "socket[PF_PACKET,SOCK_RAW]: %s",
			   strerror(errno));
		return -1;
	}

	if (eloop_register_read_sock(drv->common.sock, macsec_qca_handle_read,
				     drv->common.ctx, NULL)) {
		wpa_printf(MSG_INFO, "Could not register read socket");
		goto fail;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->common.ifname, sizeof(ifr.ifr_name));
	if (ioctl(drv->common.sock, SIOCGIFINDEX, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "ioctl(SIOCGIFINDEX): %s",
			   strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	wpa_printf(MSG_DEBUG, "Opening raw packet socket for ifindex %d",
		   addr.sll_ifindex);

	if (bind(drv->common.sock, (struct sockaddr *) &addr,
		 sizeof(addr)) < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: bind: %s", strerror(errno));
		goto fail;
	}

	/* filter multicast address */
	if (wired_multicast_membership(drv->common.sock, ifr.ifr_ifindex,
				       pae_group_addr, 1) < 0) {
		wpa_printf(MSG_ERROR,
			"macsec_qca_init_sockets: Failed to add multicast group membership");
		goto fail;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->common.ifname, sizeof(ifr.ifr_name));
	if (ioctl(drv->common.sock, SIOCGIFHWADDR, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "ioctl(SIOCGIFHWADDR): %s",
			   strerror(errno));
		goto fail;
	}

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		wpa_printf(MSG_INFO, "Invalid HW-addr family 0x%04x",
			   ifr.ifr_hwaddr.sa_family);
		goto fail;
	}
	os_memcpy(own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;

fail:
	eloop_unregister_read_sock(drv->common.sock);
	close(drv->common.sock);
	drv->common.sock = -1;
	return -1;
#else /* __linux__ */
	return -1;
#endif /* __linux__ */
}


static int macsec_qca_secy_id_get(const char *ifname, u32 *secy_id)
{
#ifdef NSS_MACSEC_SECY_ID_GET_FUNC
	/* Get secy id from nss macsec driver */
	return nss_macsec_secy_id_get((u8 *) ifname, secy_id);
#else /* NSS_MACSEC_SECY_ID_GET_FUNC */
	/* Board specific settings */
	if (os_strcmp(ifname, "eth2") == 0) {
		*secy_id = 1;
	} else if (os_strcmp(ifname, "eth3") == 0) {
		*secy_id = 2;
	} else if (os_strcmp(ifname, "eth4") == 0 ||
		   os_strcmp(ifname, "eth0") == 0) {
		*secy_id = 0;
	} else if (os_strcmp(ifname, "eth5") == 0 ||
		   os_strcmp(ifname, "eth1") == 0) {
		*secy_id = 1;
	} else {
		*secy_id = -1;
		return -1;
	}

	return 0;
#endif /* NSS_MACSEC_SECY_ID_GET_FUNC */
}


static void * macsec_qca_init(void *ctx, const char *ifname)
{
	struct macsec_qca_data *drv;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;

	if (macsec_qca_secy_id_get(ifname, &drv->secy_id)) {
		wpa_printf(MSG_ERROR,
			   "macsec_qca: Failed to get secy_id for %s", ifname);
		os_free(drv);
		return NULL;
	}

	if (driver_wired_init_common(&drv->common, ifname, ctx) < 0) {
		os_free(drv);
		return NULL;
	}

	return drv;
}


static void macsec_qca_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;

	driver_wired_deinit_common(&drv->common);
	os_free(drv);
}

#ifdef HOSTAPD
static struct nla_policy ssdk_event_policy[SSDK_ATTR_MAX] = {
	[SSDK_ATTR_MACADDR] = {
		.minlen = ETH_ALEN,
		.maxlen = ETH_ALEN,
	},
};

static int process_genl_event(struct nl_msg *msg, void *arg)
{
	struct macsec_qca_data *drv = arg;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *genl_hdr;
	int error;
	u8 addr[ETH_ALEN] = {0};
	union wpa_event_data event;
	struct nlattr *attrs[SSDK_ATTR_MAX];

	nl_hdr = nlmsg_hdr(msg);
	genl_hdr = genlmsg_hdr(nl_hdr);

	error = genlmsg_parse(nl_hdr, 0, attrs, SSDK_ATTR_MAX - 1,
			      ssdk_event_policy);
	if (error < 0) {
		wpa_printf(MSG_DEBUG, "genlmsg_parse fail: %s", nl_geterror(error));
		return error;
	}
	if (nl_hdr->nlmsg_type == drv->genl_id) {
		if (genl_hdr->cmd == SSDK_COMMAND_NEW_MAC) {
			if (attrs[SSDK_ATTR_MACADDR]) {
				os_memcpy(addr, nla_data(attrs[SSDK_ATTR_MACADDR]),
					ETH_ALEN);
			}
			if (attrs[SSDK_ATTR_IFNAME] && os_memcmp(drv->common.ifname,
				nla_get_string(attrs[SSDK_ATTR_IFNAME]),
				nla_len(attrs[SSDK_ATTR_IFNAME])) == 0) {
				wpa_printf(MSG_DEBUG, "genl new mac event: addr "
					MACSTR " ifname %s", MAC2STR(addr),
					nla_get_string(attrs[SSDK_ATTR_IFNAME]));
				os_memset(&event, 0, sizeof(event));
				event.new_sta.addr = addr;
				event.new_sta.flags |= WIRED_STA_MAB;
				wpa_supplicant_event(drv->common.ctx, EVENT_NEW_STA, &event);
			}
		}
		if (genl_hdr->cmd == SSDK_COMMAND_EXPIRE_MAC) {
			if (attrs[SSDK_ATTR_MACADDR]) {
				os_memcpy(addr, nla_data(attrs[SSDK_ATTR_MACADDR]),
					ETH_ALEN);
			}
			if (attrs[SSDK_ATTR_IFNAME] && os_memcmp(drv->common.ifname,
				nla_get_string(attrs[SSDK_ATTR_IFNAME]),
				nla_len(attrs[SSDK_ATTR_IFNAME])) == 0) {
				wpa_printf(MSG_DEBUG, "genl expire mac event: addr "
					MACSTR " ifname %s", MAC2STR(addr),
					nla_get_string(attrs[SSDK_ATTR_IFNAME]));
				os_memset(&event, 0, sizeof(event));
				event.disassoc_info.addr = addr;
				wpa_supplicant_event(drv->common.ctx, EVENT_DISASSOC, &event);
			}
		}
	}
	return NL_SKIP;
}

static void wpa_driver_macsec_qca_event_receive(int sock, void *eloop_ctx,
					     void *handle)
{
	struct nl_cb *cb = eloop_ctx;
	int res;

	wpa_printf(MSG_MSGDUMP, "macsec_qca: Event message available");

	res = nl_recvmsgs(handle, cb);
	if (res < 0) {
		wpa_printf(MSG_INFO, "macsec_qca: %s->nl_recvmsgs failed: %d",
			   __func__, res);
	}
}

#if __WORDSIZE == 64
#define ELOOP_SOCKET_INVALID	(intptr_t) 0x8888888888888889ULL
#else
#define ELOOP_SOCKET_INVALID	(intptr_t) 0x88888889ULL
#endif

static void macsec_qca_register_eloop_read(struct nl_sock **handle,
					eloop_sock_handler handler,
					void *eloop_data, int persist)
{
	/*
	 * libnl uses a pretty small buffer (32 kB that gets converted to 64 kB)
	 * by default. It is possible to hit that limit in some cases where
	 * operations are blocked, e.g., with a burst of Deauthentication frames
	 * to hostapd and STA entry deletion. Try to increase the buffer to make
	 * this less likely to occur.
	 */
	int err;

	err = nl_socket_set_buffer_size(*handle, 262144, 0);
	if (err < 0) {
		wpa_printf(MSG_DEBUG,
			   "macsec_qca: Could not set nl_socket RX buffer size: %s",
			   nl_geterror(err));
		/* continue anyway with the default (smaller) buffer */
	}

	nl_socket_set_nonblocking(*handle);
	eloop_register_read_sock(nl_socket_get_fd(*handle), handler,
				 eloop_data, *handle);
	if (!persist)
		*handle = (void *) (((intptr_t) *handle) ^
				    ELOOP_SOCKET_INVALID);
}
static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static void macsec_qca_destroy_eloop_handle(struct nl_sock **handle, int persist)
{
	if (!persist)
		*handle = (void *) (((intptr_t) *handle) ^
				    ELOOP_SOCKET_INVALID);
	eloop_unregister_read_sock(nl_socket_get_fd(*handle));
	if (*handle) {
		nl_socket_free(*handle);
		*handle = NULL;
	}
}

static int nl_send_recv(struct nl_sock *sk, struct nl_msg *msg)
{
	int ret;

	ret = nl_send_auto_complete(sk, msg);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca %s: failed to send: %d (%s)",
			   __func__, ret, nl_geterror(-ret));
		return ret;
	}

	ret = nl_recvmsgs_default(sk);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca %s: failed to recv: %d (%s)",
			   __func__, ret, nl_geterror(-ret));
	}

	return ret;
}

static int nl_send_mac_poll(struct macsec_qca_data *drv, u8 poll)
{
	struct nl_msg *msg;
	int ret = -1;
	msg = nlmsg_alloc();
	if (!msg) {
		wpa_printf(MSG_ERROR, "macsec_qca: failed to alloc message");
		return ret;
	}

	if (!genlmsg_put(msg, 0, 0, drv->genl_id, 0, 0, SSDK_COMMAND_POLL_MAC, 0)) {
		wpa_printf(MSG_ERROR, "macsec_qca: failed to put header");
		goto nla_put_failure;
	}

	wpa_printf(MSG_DEBUG, "nl_send_mac_poll: ifname %s poll %d", drv->common.ifname, poll);
	NLA_PUT_STRING(msg, SSDK_ATTR_IFNAME, drv->common.ifname);
	NLA_PUT_U8(msg, SSDK_ATTR_MACPOLL, poll);

	ret = nl_send_recv(drv->nl, msg);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: failed to communicate: %d (%s)",
			ret, nl_geterror(-ret));
	}
nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

static struct nl_sock * nl_create_handle(struct nl_cb *cb, const char *dbg)
{
	struct nl_sock *handle;

	handle = nl_socket_alloc_cb(cb);
	if (handle == NULL) {
		wpa_printf(MSG_ERROR, "macsec_qca: Failed to allocate netlink "
			   "callbacks (%s)", dbg);
		return NULL;
	}

	if (genl_connect(handle)) {
		wpa_printf(MSG_ERROR, "macsec_qca: Failed to connect to generic "
			   "netlink (%s)", dbg);
		nl_socket_free(handle);
		return NULL;
	}

	return handle;
}

static int macsec_qca_init_genl(struct macsec_qca_data *drv)
{
	int group, error;
	drv->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (drv->nl_cb == NULL) {
		wpa_printf(MSG_ERROR, "macsec_qca: Failed to allocate netlink "
			   "callbacks");
		return -1;
	}
	drv->nl = nl_create_handle(drv->nl_cb, "nl");
	if (drv->nl == NULL)
		goto out_free;
	drv->nl_event = nl_create_handle(drv->nl_cb, "event");
	if (drv->nl_event == NULL)
		goto out_free;

	drv->genl_id = genl_ctrl_resolve(drv->nl, SSDK_GENL_FAMILY_NAME);
	if (drv->genl_id < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: genl resolve faimily id failed");
		goto out_free;
	}
	group = genl_ctrl_resolve_grp(drv->nl, SSDK_GENL_FAMILY_NAME, SSDK_GENL_MCAST_GRP_NAME);
	if (group < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: genl resolve group fail");
		goto out_free;
	}

	wpa_printf(MSG_DEBUG, "macsec_qca: genl id %d, group %d", drv->genl_id, group);

	error = nl_socket_add_membership(drv->nl_event, group);
	if (error) {
		wpa_printf(MSG_ERROR, "macsec_qca: genl add membership failed: %d", error);
		goto out_free;
	}

	nl_cb_set(drv->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  no_seq_check, NULL);
	nl_cb_set(drv->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
		  process_genl_event, drv);

	macsec_qca_register_eloop_read(&drv->nl_event,
				    wpa_driver_macsec_qca_event_receive,
				    drv->nl_cb, 0);

	nl_send_mac_poll(drv, 1);
	return 0;
out_free:
	if (drv->nl) {
		nl_socket_free(drv->nl);
		drv->nl = NULL;
	}
	if (drv->nl_event) {
		nl_socket_free(drv->nl_event);
		drv->nl_event = NULL;
	}
	if (drv->nl_cb) {
		nl_cb_put(drv->nl_cb);
		drv->nl_cb = NULL;
	}
	return -1;
}

static int macsec_qca_set_param(struct macsec_qca_data *drv, const char *param)
{
	if (param == NULL)
		return 0;
	wpa_printf(MSG_DEBUG, "macsec_qca: driver param='%s'", param);

	/* configurations in driver_params */
	if (os_strstr(param, "authorize_policy=0")) {
		drv->authorize_policy = 0;
	}
	if (os_strstr(param, "authorize_policy=1")) {
		drv->authorize_policy = 1;
	}
	return 0;
}
#endif /* HOSTAPD */

#ifdef HOSTAPD
static void macsec_qca_hapd_deinit(void *priv);
#endif /* HOSTAPD */

static void * macsec_qca_hapd_init(struct hostapd_data *hapd,
				   struct wpa_init_params *params)
{
	struct macsec_qca_data *drv;

	drv = os_zalloc(sizeof(struct macsec_qca_data));
	if (!drv) {
		wpa_printf(MSG_INFO,
			   "Could not allocate memory for macsec_qca driver data");
		return NULL;
	}

	if (macsec_qca_secy_id_get(params->ifname, &drv->secy_id)) {
		wpa_printf(MSG_ERROR,
			   "macsec_qca: Failed to get secy_id for %s",
			   params->ifname);
		os_free(drv);
		return NULL;
	}

	drv->common.ctx = hapd;
	os_strlcpy(drv->common.ifname, params->ifname,
		   sizeof(drv->common.ifname));
	drv->use_pae_group_addr = params->use_pae_group_addr;

#ifdef HOSTAPD
	drv->ioctl_sock = -1;

	if (macsec_qca_set_param(drv, params->driver_params) < 0) {
		os_free(drv);
		return NULL;
	}
#endif /* HOSTAPD */

	if (macsec_qca_init_sockets(drv, params->own_addr)) {
		os_free(drv);
		return NULL;
	}

#ifdef HOSTAPD
	if ((drv->authorize_policy == 1) && macsec_qca_init_genl(drv) < 0) {
		goto fail;
	}
	if ((drv->ioctl_sock = open(SW_SWITCH_IOCTL_DEV_NAME, O_RDWR)) < 0) {
		goto fail;
	}
	/* deny all mac address and accept eapol */
	u8 mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	macsec_qca_set_sta_acl_policy(drv, mac, 0);
#endif /* HOSTAPD */
	return drv;

#ifdef HOSTAPD
fail:
	macsec_qca_hapd_deinit(drv);
	return NULL;
#endif /* HOSTAPD */
}


static void macsec_qca_hapd_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;

	if (drv->common.sock >= 0) {
		eloop_unregister_read_sock(drv->common.sock);
		close(drv->common.sock);
	}
#ifdef HOSTAPD
	if (drv->ioctl_sock >= 0) {
		u8 mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		macsec_qca_set_sta_acl_policy(drv, mac, 1);
		close(drv->ioctl_sock);
	}
	if (drv->nl) {
		nl_send_mac_poll(drv, 0);
		nl_socket_free(drv->nl);
		drv->nl = NULL;
	}
	if (drv->nl_event) {
		macsec_qca_destroy_eloop_handle(&drv->nl_event, 0);
	}
	if (drv->nl_cb) {
		nl_cb_put(drv->nl_cb);
		drv->nl_cb = NULL;
	}
#endif /* HOSTAPD */
	os_free(drv);
}


static int macsec_qca_send_eapol(void *priv, const u8 *addr,
				 const u8 *data, size_t data_len, int encrypt,
				 const u8 *own_addr, u32 flags, int link_id)
{
	struct macsec_qca_data *drv = priv;
	struct ieee8023_hdr *hdr;
	size_t len;
	u8 *pos;
	int res;

	len = sizeof(*hdr) + data_len;
	hdr = os_zalloc(len);
	if (!hdr) {
		wpa_printf(MSG_INFO,
			   "malloc() failed for macsec_qca_send_eapol(len=%lu)",
			   (unsigned long) len);
		return -1;
	}

	os_memcpy(hdr->dest, drv->use_pae_group_addr ? pae_group_addr : addr,
		  ETH_ALEN);
	os_memcpy(hdr->src, own_addr, ETH_ALEN);
	hdr->ethertype = htons(ETH_P_PAE);

	pos = (u8 *) (hdr + 1);
	os_memcpy(pos, data, data_len);

	res = send(drv->common.sock, (u8 *) hdr, len, 0);
	os_free(hdr);

	if (res < 0) {
		wpa_printf(MSG_ERROR,
			   "macsec_qca_send_eapol - packet len: %lu - failed: send: %s",
			   (unsigned long) len, strerror(errno));
	}

	return res;
}

#ifdef HOSTAPD
static int
macsec_qca_sta_set_flags(void *priv, const u8 *addr,
		      unsigned int total_flags, unsigned int flags_or,
		      unsigned int flags_and)
{
	struct macsec_qca_data *drv = priv;
	if (hostapd_check_acl(drv->common.ctx, addr, NULL) != HOSTAPD_ACL_PENDING)
	{
		return 0;
	}

	if (total_flags & WPA_STA_AUTHORIZED)
		return macsec_qca_set_sta_acl_policy(priv, addr, 1);
	if (!(total_flags & WPA_STA_AUTHORIZED))
		return macsec_qca_set_sta_acl_policy(priv, addr, 0);
	return 0;
}

static int macsec_qca_set_acl(void *priv,
				 struct hostapd_acl_params *params)
{
	unsigned int i;

	/* set each mac address in the accept mac list */
	for (i = 0; i < params->num_mac_acl; i++) {
		if (macsec_qca_set_sta_acl_policy(priv, params->mac_acl[i].addr,
				params->acl_policy)) {
			return -1;
		}
	}
	return 0;
}

static int macsec_qca_set_radius_acl_auth(void *priv, const u8 *mac, int accepted,
				   u32 session_timeout)
{
	wpa_printf(MSG_DEBUG, "%s: accepted %d", __func__, accepted);
	return macsec_qca_set_sta_acl_policy(priv, mac, accepted);
}
#endif /* HOSTAPD */

static int macsec_qca_macsec_init(void *priv, struct macsec_init_params *params)
{
	struct macsec_qca_data *drv = priv;

	drv->always_include_sci = params->always_include_sci;
	drv->use_es = params->use_es;
	drv->use_scb = params->use_scb;

	wpa_printf(MSG_DEBUG, "%s: es=%d, scb=%d, sci=%d",
		   __func__, drv->use_es, drv->use_scb,
		   drv->always_include_sci);

	__macsec_drv_init(drv);

	return 0;
}


static int macsec_qca_macsec_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s", __func__);

	__macsec_drv_deinit(drv);

	return 0;
}


static int macsec_qca_get_capability(void *priv, enum macsec_cap *cap)
{
	wpa_printf(MSG_DEBUG, "%s", __func__);

	*cap = MACSEC_CAP_INTEG_AND_CONF_0_30_50;

	return 0;
}


static int macsec_qca_enable_protect_frames(void *priv, bool enabled)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	drv->protect_frames = enabled;

	return ret;
}


static int macsec_qca_set_replay_protect(void *priv, bool enabled,
					 unsigned int window)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d, win=%u",
		   __func__, enabled, window);

	drv->replay_protect = enabled;
	drv->replay_window = window;

	return ret;
}


static fal_cipher_suite_e macsec_qca_cs_type_get(u64 cs)
{
	if (cs == CS_ID_GCM_AES_128)
		return FAL_CIPHER_SUITE_AES_GCM_128;
	if (cs == CS_ID_GCM_AES_256)
		return FAL_CIPHER_SUITE_AES_GCM_256;
	return FAL_CIPHER_SUITE_MAX;
}


static int macsec_qca_set_current_cipher_suite(void *priv, u64 cs)
{
	struct macsec_qca_data *drv = priv;
	fal_cipher_suite_e cs_type;

	if (cs != CS_ID_GCM_AES_128 && cs != CS_ID_GCM_AES_256) {
		wpa_printf(MSG_ERROR,
			   "%s: NOT supported CipherSuite: %016" PRIx64,
			   __func__, cs);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: CipherSuite: %016" PRIx64, __func__, cs);

	cs_type = macsec_qca_cs_type_get(cs);
	return nss_macsec_secy_cipher_suite_set(drv->secy_id, cs_type);
}


static int macsec_qca_enable_controlled_port(void *priv, bool enabled)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enable=%d", __func__, enabled);

	ret += nss_macsec_secy_controlled_port_en_set(drv->secy_id, enabled);

	return ret;
}


static int macsec_qca_lookup_channel(struct channel_map *map,
				     struct ieee802_1x_mka_sci *sci,
				     u32 *channel)
{
	u32 i;

	for (i = 0; i < MAXSC; i++) {
		if (os_memcmp(&map[i].sci, sci,
			      sizeof(struct ieee802_1x_mka_sci)) == 0) {
			*channel = i;
			return 0;
		}
	}

	return -1;
}


static void macsec_qca_register_channel(struct channel_map *map,
					struct ieee802_1x_mka_sci *sci,
					u32 channel)
{
	os_memcpy(&map[channel].sci, sci, sizeof(struct ieee802_1x_mka_sci));
}


static int macsec_qca_lookup_receive_channel(struct macsec_qca_data *drv,
					     struct receive_sc *sc,
					     u32 *channel)
{
	return macsec_qca_lookup_channel(drv->receive_channel_map, &sc->sci,
					 channel);
}


static void macsec_qca_register_receive_channel(struct macsec_qca_data *drv,
						struct receive_sc *sc,
						u32 channel)
{
	macsec_qca_register_channel(drv->receive_channel_map, &sc->sci,
				    channel);
}


static int macsec_qca_lookup_transmit_channel(struct macsec_qca_data *drv,
					      struct transmit_sc *sc,
					      u32 *channel)
{
	return macsec_qca_lookup_channel(drv->transmit_channel_map, &sc->sci,
					 channel);
}


static void macsec_qca_register_transmit_channel(struct macsec_qca_data *drv,
						 struct transmit_sc *sc,
						 u32 channel)
{
	macsec_qca_register_channel(drv->transmit_channel_map, &sc->sci,
				    channel);
}


static int macsec_qca_get_receive_lowest_pn(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 next_pn = 0;
	bool enabled = false;
	u32 win;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	ret += nss_macsec_secy_rx_sa_next_pn_get(drv->secy_id, channel, sa->an,
						 &next_pn);
	ret += nss_macsec_secy_rx_sc_replay_protect_get(drv->secy_id, channel,
							&enabled);
	ret += nss_macsec_secy_rx_sc_anti_replay_window_get(drv->secy_id,
							    channel, &win);

	if (enabled)
		sa->lowest_pn = (next_pn > win) ? (next_pn - win) : 1;
	else
		sa->lowest_pn = next_pn;

	wpa_printf(MSG_DEBUG, "%s: lpn=0x%x", __func__, sa->lowest_pn);

	return ret;
}


static int macsec_qca_get_transmit_next_pn(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	ret += nss_macsec_secy_tx_sa_next_pn_get(drv->secy_id, channel, sa->an,
						 &sa->next_pn);

	wpa_printf(MSG_DEBUG, "%s: npn=0x%x", __func__, sa->next_pn);

	return ret;
}


static int macsec_qca_set_transmit_next_pn(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	ret += nss_macsec_secy_tx_sa_next_pn_set(drv->secy_id, channel, sa->an,
						 sa->next_pn);

	wpa_printf(MSG_INFO, "%s: npn=0x%x", __func__, sa->next_pn);

	return ret;
}


static int macsec_qca_get_available_receive_sc(void *priv, u32 *channel)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 sc_ch = 0;
	bool in_use = false;

	for (sc_ch = 0; sc_ch < MAXSC; sc_ch++) {
		ret = nss_macsec_secy_rx_sc_in_used_get(drv->secy_id, sc_ch,
							&in_use);
		if (ret)
			continue;

		if (!in_use) {
			*channel = sc_ch;
			wpa_printf(MSG_DEBUG, "%s: channel=%d",
				   __func__, *channel);
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG, "%s: no available channel", __func__);

	return -1;
}


static int macsec_qca_create_receive_sc(void *priv, struct receive_sc *sc,
					unsigned int conf_offset,
					int validation)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_rx_prc_lut_t entry;
	fal_rx_sc_validate_frame_e vf;
	enum validate_frames validate_frames = validation;
	u32 channel;
	const u8 *sci_addr = sc->sci.addr;
	u16 sci_port = be_to_host16(sc->sci.port);

	ret = macsec_qca_get_available_receive_sc(priv, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* rx prc lut */
	os_memset(&entry, 0, sizeof(entry));

	os_memcpy(entry.sci, sci_addr, ETH_ALEN);
	entry.sci[6] = (sci_port >> 8) & 0xff;
	entry.sci[7] = sci_port & 0xff;
	entry.sci_mask = 0xf;

	entry.valid = 1;
	entry.channel = channel;
	entry.action = FAL_RX_PRC_ACTION_PROCESS;
	entry.offset = conf_offset;

	/* rx validate frame  */
	if (validate_frames == Strict)
		vf = FAL_RX_SC_VALIDATE_FRAME_STRICT;
	else if (validate_frames == Checked)
		vf = FAL_RX_SC_VALIDATE_FRAME_CHECK;
	else
		vf = FAL_RX_SC_VALIDATE_FRAME_DISABLED;

	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_rx_sc_create(drv->secy_id, channel);
	ret += nss_macsec_secy_rx_sc_validate_frame_set(drv->secy_id, channel,
							vf);
	ret += nss_macsec_secy_rx_sc_replay_protect_set(drv->secy_id, channel,
							drv->replay_protect);
	ret += nss_macsec_secy_rx_sc_anti_replay_window_set(drv->secy_id,
							    channel,
							    drv->replay_window);

	macsec_qca_register_receive_channel(drv, sc, channel);

	return ret;
}


static int macsec_qca_delete_receive_sc(void *priv, struct receive_sc *sc)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_rx_prc_lut_t entry;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* rx prc lut */
	os_memset(&entry, 0, sizeof(entry));

	ret += nss_macsec_secy_rx_sc_del(drv->secy_id, channel);
	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);

	return ret;
}


static int macsec_qca_create_receive_sa(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_rx_sak_t rx_sak;
	int i = 0;
	u32 channel;
	fal_rx_prc_lut_t entry;
	u32 offset;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s, channel=%d, an=%d, lpn=0x%x",
		   __func__, channel, sa->an, sa->lowest_pn);

	os_memset(&rx_sak, 0, sizeof(rx_sak));
	rx_sak.sak_len = sa->pkey->key_len;
	if (sa->pkey->key_len == SAK_128_LEN) {
		for (i = 0; i < 16; i++)
			rx_sak.sak[i] = sa->pkey->key[15 - i];
	} else if (sa->pkey->key_len == SAK_256_LEN) {
		for (i = 0; i < 16; i++) {
			rx_sak.sak1[i] = sa->pkey->key[15 - i];
			rx_sak.sak[i] = sa->pkey->key[31 - i];
		}
	} else {
		return -1;
	}

	if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_0)
		offset = 0;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_30)
		offset = 30;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_50)
		offset = 50;
	else
		return -1;
	ret += nss_macsec_secy_rx_prc_lut_get(drv->secy_id, channel, &entry);
	entry.offset = offset;
	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_rx_sa_create(drv->secy_id, channel, sa->an);
	ret += nss_macsec_secy_rx_sak_set(drv->secy_id, channel, sa->an,
					  &rx_sak);

	return ret;
}


static int macsec_qca_enable_receive_sa(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_rx_sa_en_set(drv->secy_id, channel, sa->an,
					    true);

	return ret;
}


static int macsec_qca_disable_receive_sa(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_rx_sa_en_set(drv->secy_id, channel, sa->an,
					    false);

	return ret;
}


static int macsec_qca_get_available_transmit_sc(void *priv, u32 *channel)
{
	struct macsec_qca_data *drv = priv;
	u32 sc_ch = 0;
	bool in_use = false;

	for (sc_ch = 0; sc_ch < MAXSC; sc_ch++) {
		if (nss_macsec_secy_tx_sc_in_used_get(drv->secy_id, sc_ch,
						      &in_use))
			continue;

		if (!in_use) {
			*channel = sc_ch;
			wpa_printf(MSG_DEBUG, "%s: channel=%d",
				   __func__, *channel);
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG, "%s: no available channel", __func__);

	return -1;
}


static int macsec_qca_create_transmit_sc(void *priv, struct transmit_sc *sc,
					 unsigned int conf_offset)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_tx_class_lut_t entry;
	u8 psci[ETH_ALEN + 2];
	u32 channel;
	u16 sci_port = be_to_host16(sc->sci.port);

	ret = macsec_qca_get_available_transmit_sc(priv, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* class lut */
	os_memset(&entry, 0, sizeof(entry));

	entry.valid = 1;
	entry.action = FAL_TX_CLASS_ACTION_FORWARD;
	entry.channel = channel;

	os_memcpy(psci, sc->sci.addr, ETH_ALEN);
	psci[6] = (sci_port >> 8) & 0xff;
	psci[7] = sci_port & 0xff;

	ret += nss_macsec_secy_tx_class_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_tx_sc_create(drv->secy_id, channel, psci, 8);
	ret += nss_macsec_secy_tx_sc_protect_set(drv->secy_id, channel,
						 drv->protect_frames);
	ret += nss_macsec_secy_tx_sc_confidentiality_offset_set(drv->secy_id,
								channel,
								conf_offset);

	macsec_qca_register_transmit_channel(drv, sc, channel);

	return ret;
}


static int macsec_qca_delete_transmit_sc(void *priv, struct transmit_sc *sc)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_tx_class_lut_t entry;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* class lut */
	os_memset(&entry, 0, sizeof(entry));

	ret += nss_macsec_secy_tx_class_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_tx_sc_del(drv->secy_id, channel);

	return ret;
}


static int macsec_qca_create_transmit_sa(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u8 tci = 0;
	fal_tx_sak_t tx_sak;
	int i;
	u32 channel;
	u32 offset;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG,
		   "%s: channel=%d, an=%d, next_pn=0x%x, confidentiality=%d",
		   __func__, channel, sa->an, sa->next_pn, sa->confidentiality);

	if (drv->always_include_sci)
		tci |= TCI_SC;
	else if (drv->use_es)
		tci |= TCI_ES;
	else if (drv->use_scb)
		tci |= TCI_SCB;

	if (sa->confidentiality)
		tci |= TCI_E | TCI_C;

	os_memset(&tx_sak, 0, sizeof(tx_sak));
	tx_sak.sak_len = sa->pkey->key_len;
	if (sa->pkey->key_len == SAK_128_LEN) {
		for (i = 0; i < 16; i++)
			tx_sak.sak[i] = sa->pkey->key[15 - i];
	} else if (sa->pkey->key_len == SAK_256_LEN) {
		for (i = 0; i < 16; i++) {
			tx_sak.sak1[i] = sa->pkey->key[15 - i];
			tx_sak.sak[i] = sa->pkey->key[31 - i];
		}
	} else {
		return -1;
	}

	if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_0)
		offset = 0;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_30)
		offset = 30;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_50)
		offset = 50;
	else
		return -1;
	ret += nss_macsec_secy_tx_sc_confidentiality_offset_set(drv->secy_id,
								channel,
								offset);
	ret += nss_macsec_secy_tx_sa_next_pn_set(drv->secy_id, channel, sa->an,
						 sa->next_pn);
	ret += nss_macsec_secy_tx_sak_set(drv->secy_id, channel, sa->an,
					  &tx_sak);
	ret += nss_macsec_secy_tx_sc_tci_7_2_set(drv->secy_id, channel,
						 (tci >> 2));
	ret += nss_macsec_secy_tx_sc_an_set(drv->secy_id, channel, sa->an);

	return ret;
}


static int macsec_qca_enable_transmit_sa(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_tx_sa_en_set(drv->secy_id, channel, sa->an,
					    true);

	return ret;
}


static int macsec_qca_disable_transmit_sa(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_tx_sa_en_set(drv->secy_id, channel, sa->an,
					    false);

	return ret;
}


const struct wpa_driver_ops wpa_driver_macsec_qca_ops = {
	.name = "macsec_qca",
	.desc = "QCA MACsec Ethernet driver",
	.get_ssid = driver_wired_get_ssid,
	.get_bssid = driver_wired_get_bssid,
	.get_capa = macsec_qca_get_capa,
	.init = macsec_qca_init,
	.deinit = macsec_qca_deinit,
	.hapd_init = macsec_qca_hapd_init,
	.hapd_deinit = macsec_qca_hapd_deinit,
	.hapd_send_eapol = macsec_qca_send_eapol,
#ifdef HOSTAPD
	.sta_set_flags = macsec_qca_sta_set_flags,
	.set_acl = macsec_qca_set_acl,
	.set_radius_acl_auth = macsec_qca_set_radius_acl_auth,
#endif /* HOSTAPD */

	.macsec_init = macsec_qca_macsec_init,
	.macsec_deinit = macsec_qca_macsec_deinit,
	.macsec_get_capability = macsec_qca_get_capability,
	.enable_protect_frames = macsec_qca_enable_protect_frames,
	.set_replay_protect = macsec_qca_set_replay_protect,
	.set_current_cipher_suite = macsec_qca_set_current_cipher_suite,
	.enable_controlled_port = macsec_qca_enable_controlled_port,
	.get_receive_lowest_pn = macsec_qca_get_receive_lowest_pn,
	.get_transmit_next_pn = macsec_qca_get_transmit_next_pn,
	.set_transmit_next_pn = macsec_qca_set_transmit_next_pn,
	.create_receive_sc = macsec_qca_create_receive_sc,
	.delete_receive_sc = macsec_qca_delete_receive_sc,
	.create_receive_sa = macsec_qca_create_receive_sa,
	.enable_receive_sa = macsec_qca_enable_receive_sa,
	.disable_receive_sa = macsec_qca_disable_receive_sa,
	.create_transmit_sc = macsec_qca_create_transmit_sc,
	.delete_transmit_sc = macsec_qca_delete_transmit_sc,
	.create_transmit_sa = macsec_qca_create_transmit_sa,
	.enable_transmit_sa = macsec_qca_enable_transmit_sa,
	.disable_transmit_sa = macsec_qca_disable_transmit_sa,
};
