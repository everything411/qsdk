/*
 **************************************************************************
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * netfn_capwapmgr.c
 *	Legacy CAPWAP manager
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/if_pppox.h>
#include <linux/udp.h>

#include <ppe_drv_iface.h>
#include <netfn_capwapmgr.h>
#include <netfn_capwapmgr_legacy.h>
#include "netfn_capwapmgr_priv.h"
#include "netfn_capwapmgr_tun.h"
#include "netfn_capwapmgr_tunid.h"

/*
 * NETFN dtls offload only supports 32bit nounce value
 */
#define NETFN_CAPWAPMGR_DTLS_NONCE_LEN_MAX 4

/*
 * Max number of DTLS algorith supported.
 */
#define NETFN_CAPWAPMGR_DTLS_ALGO_MAX 5

/*
 * Max legth of dtls algoritgm string.
 */
#define NETFN_CAPWAPMGR_DTLS_ALGO_STRLEN_MAX 25

/*
 * netfn_capwapmgr_dtls_algo_str
 *	Supported DTLS algorithms.
 */
const char netfn_capwapmgr_dtls_algo_str[NETFN_CAPWAPMGR_DTLS_ALGO_MAX][NETFN_CAPWAPMGR_DTLS_ALGO_STRLEN_MAX] =
{
"eip-aes-cbc-sha1-hmac",
"eip-aes-cbc-sha256-hmac",
"eip-3des-cbc-sha1-hmac",
"eip-3des-cbc-sha256-hmac",
"eip-aes-gcm"
};

/*
 * TODO:Get this from sysfs.
 */
uint8_t netfn_capwapmgr_snap[NETFN_CAPWAP_SNAP_HDR_LEN];

/*
 * netfn_capwapmgr_dtls_version_enum_to_netfn.
 *	Legacy Enum to netfn config.
 */
static const uint16_t netfn_capwapmgr_dtls_version_enum_to_netfn[2] = {0xFFFE, 0xFDFE};

#if defined(NETFN_CAPWAPMGR_ONE_NETDEV)
struct net_device *netfn_capwapmgr_dev;
#endif

/*
 * netfn_capwapmgr_update_pppoe_rule()
 *      Updates PPPoE rule from PPPoE netdevice.
 */
static bool netfn_capwapmgr_update_pppoe_rule(struct net_device *dev, struct netfn_flowmgr_pppoe_rule *pppoe_rule)
{
        struct pppoe_opt addressing;
        struct ppp_channel *channel[1] = {NULL};
        int px_proto;
        int ppp_ch_count;
        bool status = true;

        if (ppp_is_multilink(dev)) {
                netfn_capwapmgr_warn("%px: channel is multilink PPP\n", dev);
                goto fail;
        }

        ppp_ch_count = ppp_hold_channels(dev, channel, 1);
        netfn_capwapmgr_info("%px: PPP hold channel ret %d\n", dev, ppp_ch_count);
        if (ppp_ch_count != 1) {
                netfn_capwapmgr_warn("%px: hold channel for netdevice failed\n", dev);
                goto fail;
        }

        px_proto = ppp_channel_get_protocol(channel[0]);
        if (px_proto != PX_PROTO_OE) {
                netfn_capwapmgr_warn("%px: session socket is not of type PX_PROTO_OE\n", dev);
                goto fail;
        }

        if (pppoe_channel_addressing_get(channel[0], &addressing)) {
                netfn_capwapmgr_warn("%px: failed to get addressing information\n", dev);
                goto fail;
        }

        /*
         * Update the PPPoE rule information and set PPPoE valid flag.
         */
        pppoe_rule->session_id = (uint16_t)ntohs((uint16_t)addressing.pa.sid);
        memcpy(pppoe_rule->server_mac, addressing.pa.remote, ETH_ALEN);

        netfn_capwapmgr_info("%px: Update PPE PPPoE flow rule with session_id = %u, remote_mac = %pM\n",
                        dev, pppoe_rule->session_id, pppoe_rule->server_mac);

        /*
         * pppoe_channel_addressing_get returns held device.
         */
        dev_put(addressing.dev);
        goto done;

fail:
        ppp_release_channels(channel, 1);
        status = false;
done:
        return status;
}

/*
 * netfn_capwapmgr_legacy2crypto
 *	API to convert legacy crypto config to netfn crypto.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_legacy2crypto(struct netfn_dtls_crypto *netfn_crypto, struct nss_dtlsmgr_crypto *legacy_crypto)
{
	const uint8_t *legacy_keys;
	uint16_t keylen;

	/*
	 * Convert Legacy Algorithm enum to netfn Algorithm string.
	 */
	netfn_crypto->algo_name = netfn_capwapmgr_dtls_algo_str[legacy_crypto->algo];

	legacy_keys = legacy_crypto->cipher_key.data;

	/*
	 * Copy the Cipher,auth key and len.
	 *
	 * We are just copying the pointer to the key as is.
	 * Since the memory for the key will be allocated by the caller.
	 * We can just copy the pointer to the key from legacy to netfn.
	 */
	memcpy(&netfn_crypto->cipher, &legacy_crypto->cipher_key, sizeof(struct nss_dtlsmgr_crypto_data));
	memcpy(&netfn_crypto->auth, &legacy_crypto->auth_key, sizeof(struct nss_dtlsmgr_crypto_data));

	/*
	 * In netfn DTLS offload, we only support 32bit nonce.
	 */
	keylen = legacy_crypto->nonce.len;
	legacy_keys = legacy_crypto->nonce.data;
	if (keylen > NETFN_CAPWAPMGR_DTLS_NONCE_LEN_MAX) {
		return NETFN_CAPWAPMGR_ERROR_MAX;
	}

	memcpy(&netfn_crypto->nonce, legacy_keys, keylen);

	return NETFN_CAPWAPMGR_SUCCESS;
}
EXPORT_SYMBOL(netfn_capwapmgr_legacy2crypto);

/*
 * netfn_capwapgr_legacy2dtls
 *	API to convert legacy DTLS config to netfn config.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_legacy2dtls(struct nss_dtlsmgr_config *dtls_data, struct netfn_dtls_cfg *enc, struct netfn_dtls_cfg *dec)
{
	netfn_capwapmgr_ret_t status;
	struct netfn_dtls_cfg *netfn_dtls_cfg = NULL;
	struct nss_dtlsmgr_crypto *legacy_crypto = NULL;
	struct netfn_dtls_crypto *netfn_crypto = NULL;

	/*
	 * Encap Specific configuration.
	 */
	netfn_dtls_cfg = enc;
	legacy_crypto = &dtls_data->encap.crypto;
	netfn_crypto = &netfn_dtls_cfg->base;

	status = netfn_capwapmgr_legacy2crypto(netfn_crypto, legacy_crypto);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		return status;
	}

	netfn_dtls_cfg->flags = dtls_data->flags | NETFN_DTLS_FLAG_CAPWAP;
	netfn_dtls_cfg->version = netfn_capwapmgr_dtls_version_enum_to_netfn[dtls_data->encap.ver];
	netfn_dtls_cfg->epoch = htons(dtls_data->encap.epoch);

	/*
	 * If DSCP copy from skb is enabled, update the corresponding field
	 * in the netfn configuration. Else update the default dscp value.
	 */
	if (dtls_data->encap.dscp_copy) {
		netfn_dtls_cfg->flags |= NETFN_DTLS_FLAG_CP_TOS;
	} else {
		netfn_dtls_cfg->tos = dtls_data->encap.dscp << 2;
	}

	/*
	 * Copy over the default IP DF and TTL values.
	 */
	netfn_dtls_cfg->df = dtls_data->encap.df;
	netfn_dtls_cfg->hop_limit = dtls_data->encap.ip_ttl;

	/*
	 * Update config specifying this is encap session configuration.
	 */
	netfn_dtls_cfg->flags |= NETFN_DTLS_FLAG_ENC;

	/*
	 * Decap Specific configuration.
	 */
	netfn_dtls_cfg = dec;
	legacy_crypto = &dtls_data->decap.crypto;
	netfn_crypto = &netfn_dtls_cfg->base;

	status = netfn_capwapmgr_legacy2crypto(netfn_crypto, legacy_crypto);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		return status;
	}

	/*
	 * TODO:Once netfn DTLS config structure is updated. Make sure this is still needed.
	 * Currently the netfn dtls config takes encap and deacp specific parameters for a
	 * decap session as well as for an encap session.
	 */
	netfn_dtls_cfg->flags = dtls_data->flags | NETFN_DTLS_FLAG_CAPWAP;
	netfn_dtls_cfg->version = netfn_capwapmgr_dtls_version_enum_to_netfn[dtls_data->encap.ver];
	netfn_dtls_cfg->epoch = htons(dtls_data->encap.epoch);
	netfn_dtls_cfg->replay_win = dtls_data->decap.window_size;

	/*
	 * Update config specifying this is decap session configuration.
	 */
	netfn_dtls_cfg->flags &= ~NETFN_DTLS_FLAG_ENC;
	return status;
}
EXPORT_SYMBOL(netfn_capwapmgr_legacy2dtls);

/*
 * netfn_capwapmgr_legacy_rule2tun_cfg
 *	Wrapper API to get tunnel config from legacy tunnel create rules.
 *
 * This API does sanity checks on the input while converting from legacy to netfn rule.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_legacy_rule2tun_cfg(struct nss_ipv4_create *v4, struct nss_ipv6_create *v6, struct nss_capwap_rule_msg *capwap_rule, struct nss_dtlsmgr_config *dtls_data, struct netfn_capwapmgr_tun_cfg *cfg, uint8_t tunnel_id)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
        bool dtls_enabled = !!(capwap_rule->enabled_features & NSS_CAPWAPMGR_FEATURE_DTLS_ENABLED);
	netfn_tuple_t *tuple;

	BUILD_BUG_ON(sizeof(struct nss_capwap_metaheader) != sizeof(struct netfn_capwap_prehdr));

	if (!v4 && !v6) {
		netfn_capwapmgr_warn("Invalid ip create rule for tunnel: %d\n", tunnel_id);
		return NETFN_CAPWAPMGR_ERROR_MAX;
	}

	if (tunnel_id > NETFN_CAPWAPMGR_TUNNELS_MAX) {
		netfn_capwapmgr_warn("Invalid tunnel_id: %d, max allowed %d\n", tunnel_id, NETFN_CAPWAPMGR_TUNNELS_MAX);
		return NETFN_CAPWAPMGR_ERROR_MAX;
	}

	if (!(capwap_rule->l3_proto == NSS_CAPWAP_TUNNEL_IPV4 ||
		capwap_rule->l3_proto == NSS_CAPWAP_TUNNEL_IPV6)) {
		netfn_capwapmgr_warn("tunnel %d: wrong argument for l3_proto\n", tunnel_id);
		return NETFN_CAPWAPMGR_ERROR_MAX;
	}

	if (!(capwap_rule->which_udp == NSS_CAPWAP_TUNNEL_UDP ||
		capwap_rule->which_udp == NSS_CAPWAP_TUNNEL_UDPLite)) {
		netfn_capwapmgr_warn("tunnel %d: wrong argument for which_udp\n", tunnel_id);
		return NETFN_CAPWAPMGR_ERROR_MAX;
	}

	if (dtls_enabled && !dtls_data) {
		netfn_capwapmgr_warn("tunnel %d: need to supply in_data if DTLS is enabled\n", tunnel_id);
		return NETFN_CAPWAPMGR_ERROR_MAX;
	}

	/*
	 * netfn tuple is for UL direction whereas the legacy tuple is passed for the DL direction.
	 */
	tuple = &cfg->tuple;
	if (v4) {

		/*
		 * Convert flow rule.
		 * Flow rule will be for UL direction similer to the tuple direction.
		 */
		cfg->flow.out_dev = ppe_drv_dev_get_by_iface_idx(v4->src_interface_num);
		cfg->flow.flow_mtu = v4->from_mtu;
		memcpy(cfg->flow.flow_dest_mac, v4->src_mac, ETH_ALEN);
		memcpy(cfg->flow.flow_src_mac, v4->dest_mac, ETH_ALEN);
		cfg->flow.top_outdev = v4->top_ndev;

		/*
		 * Convert vlan rule.
		 */
		if ((v4->in_vlan_tag[0] & 0xFFF) != 0xFFF) {
			cfg->ext_cfg.ext_valid_flags |= NETFN_CAPWAPMGR_EXT_VALID_VLAN;
			cfg->ext_cfg.vlan.inner.egress_vlan_tag = v4->in_vlan_tag[0];
			cfg->ext_cfg.vlan.inner.ingress_vlan_tag = 0xFFF;
			cfg->ext_cfg.vlan.outer.egress_vlan_tag = v4->in_vlan_tag[1];
			cfg->ext_cfg.vlan.outer.ingress_vlan_tag = 0xFFF;
		}

		/*
		 * Conver PPPoE rule.
		 */
		if (v4->flow_pppoe_if_exist) {
			/*
			 * Copy over the PPPOE rules and set PPPOE_VALID flag.
			 */
			if (!netfn_capwapmgr_update_pppoe_rule(v4->top_ndev, &cfg->ext_cfg.pppoe)) {
				netfn_capwapmgr_warn("PPPoE rule update failed\n");
				return NETFN_CAPWAPMGR_ERROR_MAX;
			}

			cfg->ext_cfg.ext_valid_flags |= NETFN_CAPWAPMGR_EXT_VALID_PPPOE;
	        }

		/*
		 * Copy over the 5 tuple.
		 */
		tuple->ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V4;
		tuple->tuple_type = NETFN_TUPLE_5TUPLE;
		tuple->tuples.tuple_5.src_ip.ip4.s_addr = htonl(v4->dest_ip);
		tuple->tuples.tuple_5.dest_ip.ip4.s_addr = htonl(v4->src_ip);
		tuple->tuples.tuple_5.l4_src_ident = htons((uint16_t)v4->dest_port);
		tuple->tuples.tuple_5.l4_dest_ident = htons((uint16_t)v4->src_port);
		tuple->tuples.tuple_5.protocol = (uint8_t)v4->protocol;
	} else {

		/*
		 * Convert flow rule.
		 */
		cfg->flow.out_dev = ppe_drv_dev_get_by_iface_idx(v6->src_interface_num);
		cfg->flow.flow_mtu = v6->from_mtu;
		memcpy(cfg->flow.flow_dest_mac, v6->src_mac, ETH_ALEN);
		memcpy(cfg->flow.flow_src_mac, v6->dest_mac, ETH_ALEN);
		cfg->flow.top_outdev = v6->top_ndev;

		if ((v6->in_vlan_tag[0] & 0xFFF) != 0xFFF) {
			cfg->ext_cfg.ext_valid_flags |= NETFN_CAPWAPMGR_EXT_VALID_VLAN;
			cfg->ext_cfg.vlan.inner.egress_vlan_tag = v6->in_vlan_tag[0];
			cfg->ext_cfg.vlan.inner.ingress_vlan_tag = 0xFFF;
			cfg->ext_cfg.vlan.outer.egress_vlan_tag = v6->in_vlan_tag[1];
			cfg->ext_cfg.vlan.outer.ingress_vlan_tag = 0xFFF;
		}

		/*
		 * Conver PPPoE rule.
		 */
		if (v6->flow_pppoe_if_exist) {
			/*
			 * Copy over the PPPOE rules and set PPPOE_VALID flag.
			 */
			if (!netfn_capwapmgr_update_pppoe_rule(v6->top_ndev, &cfg->ext_cfg.pppoe)) {
				netfn_capwapmgr_warn("PPPoE rule update failed\n");
				return NETFN_CAPWAPMGR_ERROR_MAX;
			}

			cfg->ext_cfg.ext_valid_flags |= NETFN_CAPWAPMGR_EXT_VALID_PPPOE;
	        }

		/*
		 * Copy over the 5 tuple.
		 */
		tuple->tuples.tuple_5.protocol = (uint8_t)v6->protocol;
		tuple->ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V6;
		tuple->tuple_type = NETFN_TUPLE_5TUPLE;
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[0] = htonl(v6->dest_ip[0]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[1] = htonl(v6->dest_ip[1]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[2] = htonl(v6->dest_ip[2]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[3] = htonl(v6->dest_ip[3]);
		tuple->tuples.tuple_5.l4_src_ident = htons((uint16_t)v6->dest_port);

		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[0] = htonl(v6->src_ip[0]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[1] = htonl(v6->src_ip[1]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[2] = htonl(v6->src_ip[2]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[3] = htonl(v6->src_ip[3]);
		tuple->tuples.tuple_5.l4_dest_ident = htons((uint16_t)v6->src_port);
	}

	/*
	 * Copy over the capwap rule.
	 */
	cfg->capwap.features = (uint8_t)capwap_rule->enabled_features;
	memcpy(cfg->capwap.enc.bssid, capwap_rule->bssid, ETH_ALEN);

	/*
	 * TODO: Make this a sysfs parameter.
	 */
	memcpy(cfg->capwap.enc.snap_hdr, netfn_capwapmgr_snap, NETFN_CAPWAP_SNAP_HDR_LEN);

	/*
	 * TODO:Check if CAPWAP offload can update this to network order.
	 */
	cfg->capwap.enc.ttl = capwap_rule->encap.ttl;
	cfg->capwap.enc.tos = capwap_rule->encap.tos;
	cfg->capwap.enc.mtu = cfg->flow.flow_mtu;

	/*
	 * If protocol is UDPLite checksum coverage is set to size of udphdr.
	 * In non-legacy mode it should be passed from user.
	 */
	if (tuple->tuples.tuple_5.protocol == IPPROTO_UDPLITE) {
		cfg->capwap.enc.csum_cov = sizeof(struct udphdr);
	}

	cfg->capwap.dec.max_frags = ntohs(capwap_rule->decap.max_fragments);
	cfg->capwap.dec.max_payload_sz = ntohs(capwap_rule->decap.max_buffer_size);

	/*
	 * If DTLS is enabled, convert the dtls rule as well.
	 */
	if (dtls_enabled) {
		/*
		 * Update DTLS enable flag;
		 */
		cfg->ext_cfg.ext_valid_flags |= NETFN_CAPWAPMGR_EXT_VALID_DTLS_ENC | NETFN_CAPWAPMGR_EXT_VALID_DTLS_DEC;

		status = netfn_capwapmgr_legacy2dtls(dtls_data, &cfg->ext_cfg.enc, &cfg->ext_cfg.dec);
	}

	return status;
}
EXPORT_SYMBOL(netfn_capwapmgr_legacy_rule2tun_cfg);

/*
 * netfn_capwapmgr_stats2legacy_stats.
 *	Wrapper API to get legacy stats from netfn stats.
 */
void netfn_capwapmgr_stats2legacy_stats(struct netfn_capwap_tun_stats *netfn_stats, struct nss_capwap_tunnel_stats *legacy_stats)
{
	/*
	 * Get pkt stats.
	 */
	memcpy(&legacy_stats->pnode_stats, &netfn_stats->pkts, sizeof(struct nss_capwap_pn_stats));

      /*
       * Convert decap stats.
       */
	legacy_stats->rx_segments = netfn_stats->dec.pkts_rcvd;
	legacy_stats->rx_control_pkts = netfn_stats->dec.control_pkts;
	legacy_stats->rx_keepalive_pkts = netfn_stats->dec.keepalive_pkts;
	legacy_stats->rx_fast_reasm = netfn_stats->dec.fast_reasm;
	legacy_stats->rx_slow_reasm = netfn_stats->dec.slow_reasm;
	legacy_stats->rx_drop_dec_failure = netfn_stats->dec.err_dec_failure;
	legacy_stats->rx_drop_max_frags = netfn_stats->dec.err_max_frags;
	legacy_stats->rx_drop_missing_frags = netfn_stats->dec.drop_missing_frags;
	legacy_stats->rx_oversize_drops = netfn_stats->dec.err_large_frags;
	legacy_stats->rx_csum_drops = netfn_stats->dec.err_csum_fail;
	legacy_stats->rx_malformed = netfn_stats->dec.err_malformed;

	/*
	 * Convert encap stats.
	 */
	legacy_stats->tx_segments = netfn_stats->enc.pkts_rcvd;
	legacy_stats->tx_keepalive_pkts = netfn_stats->enc.keepalive_pkts;
	legacy_stats->tx_queue_full_drops = netfn_stats->enc.drop_queue_full;
	legacy_stats->tx_mem_failure_drops = netfn_stats->enc.drop_mem_alloc;
	legacy_stats->tx_dropped_ver_mis = netfn_stats->enc.err_ver_mis;
	legacy_stats->tx_dropped_hroom = netfn_stats->enc.err_insufficient_hroom;
	legacy_stats->tx_dropped_dtls = netfn_stats->enc.err_direct_dtls;
	legacy_stats->tx_dropped_nwireless = netfn_stats->enc.err_nwireless_len;
}
EXPORT_SYMBOL(netfn_capwapmgr_stats2legacy_stats);

#if defined(NETFN_CAPWAPMGR_ONE_NETDEV)
/*
 * nss_capwapmgr_get_netdev()
 *      Returns net device used.
 */
struct net_device *nss_capwapmgr_get_netdev(void)
{
        return netfn_capwapmgr_dev;
}
EXPORT_SYMBOL(nss_capwapmgr_get_netdev);
#endif

/*
 * netfn_capwapmgr_status2legacy_status.
 *	Wrapper API to get legacy status from netfn status.
 */
nss_capwapmgr_status_t netfn_capwapmgr_status2legacy_status(netfn_capwapmgr_ret_t netfn_status)
{
	nss_capwapmgr_status_t nss_status = NSS_CAPWAPMGR_FAILURE_LEGACY_MAX;
	if (netfn_status) {
		return netfn_status + nss_status;
	}

	return NSS_CAPWAPMGR_SUCCESS;
}
EXPORT_SYMBOL(netfn_capwapmgr_status2legacy_status);
