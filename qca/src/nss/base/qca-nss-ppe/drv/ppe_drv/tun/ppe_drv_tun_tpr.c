/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <ppe_drv/ppe_drv.h>
#include <fal_tunnel.h>
#include "ppe_drv_tun_tpr.h"
#include "ppe_drv.h"
#include "ppe_drv_port.h"
#include <ppe_drv_tun_rps.h>
#include "ppe_drv_tun_prgm_prsr.h"
#include <ppe_drv_tun_public.h>
#include "ppe_drv_tun_l3_if.h"

/*
 * ppe_drv_tun_tpr_pppoe_l3_if_configure
 *	configure pppoe l3 if for tunnel TPR
 */
static struct ppe_drv_tun_l3_if *ppe_drv_tun_tpr_pppoe_l3_if_configure(int16_t xmit_port, uint16_t session_id, uint8_t *smac)
{
	fal_tunnel_port_intf_t port_tnl_cfg = {0};
	struct ppe_drv_tun_l3_if *tl_l3_if;
	struct ppe_drv_pppoe *pppoe = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = NULL;
	sw_error_t err;

	pp = ppe_drv_port_from_port_num(xmit_port);
	if (!pp) {
		ppe_drv_warn("Couldn't get destination port for port %d", xmit_port);
		return NULL;
	}

	if (pp->mac_valid) {
		memcpy(port_tnl_cfg.mac_addr.uc, &pp->mac_addr[0], ETH_ALEN);
	}


	pppoe = ppe_drv_pppoe_find_session(session_id, smac);
	if (!pppoe) {
		ppe_drv_warn("Could not find pppoe session %x mac %pM", session_id, smac);
		return NULL;
	}

	tl_l3_if = ppe_drv_pppoe_tl_l3_if_get(pppoe);
	if (tl_l3_if) {
		ppe_drv_trace("Reusing existing tl_l3_if for PPPoE session %d", session_id);
		goto configure_port;
	}

	tl_l3_if = ppe_drv_tun_l3_if_alloc(p);
	if (!tl_l3_if) {
		ppe_drv_warn("Failed to allocate tl l3 if for pppoe session id: %d", session_id);
		return NULL;
	}

	ppe_drv_tun_l3_if_configure(tl_l3_if);
	ppe_drv_pppoe_tl_l3_if_attach(pppoe, tl_l3_if);

configure_port:
	/*
	 * Configure xmit port with tl_l3_if
	 */
	err = fal_tunnel_port_intf_get(PPE_DRV_SWITCH_ID, xmit_port, &port_tnl_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("Unable to get xmit port config for port %d", xmit_port);
		if (tl_l3_if) {
			ppe_drv_tun_l3_if_deref(tl_l3_if);
			ppe_drv_pppoe_deref(pppoe);
		}
		return NULL;
	}

	port_tnl_cfg.pppoe_en = A_TRUE;
	err = fal_tunnel_port_intf_set(PPE_DRV_SWITCH_ID, xmit_port, &port_tnl_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("Unable to set xmit port config for port %d", xmit_port);
		if (tl_l3_if) {
			ppe_drv_tun_l3_if_deref(tl_l3_if);
			ppe_drv_pppoe_deref(pppoe);
		}
		return NULL;
	}

	return tl_l3_if;
}

/*
 * ppe_drv_tun_tpr_l3_if_configure
 *	Allocate and configure TL_L3_IF for RPS tunnel
 */
static struct ppe_drv_tun_l3_if *ppe_drv_tun_tpr_l3_if_configure(int16_t xmit_port)
{
	struct ppe_drv_tun_l3_if *tun_l3_if;
	fal_tunnel_port_intf_t port_tnl_cfg = {0};
	struct ppe_drv_port *pp = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t err;

	/*
	 * Get port structure
	 */
	pp = ppe_drv_port_from_port_num(xmit_port);
	if (!pp) {
		ppe_drv_warn("Couldn't get destination port for port number %d", xmit_port);
		return NULL;
	}

	/*
	 * Check if tl_l3_if already attached to port, if so reuse it
	 */
	tun_l3_if = ppe_drv_port_tl_l3_if_get_n_ref(pp);
	if (tun_l3_if) {
		ppe_drv_trace("Reuse TL-L3 IF at index %u for port %d\n", tun_l3_if->index, xmit_port);
		goto configure_port;
	}

	/*
	 * Allocate new tl_l3_if
	 */
	tun_l3_if = ppe_drv_tun_l3_if_alloc(p);
	if (!tun_l3_if) {
		ppe_drv_warn("Failed to allocate tl l3 if for port %d", xmit_port);
		return NULL;
	}

	/*
	 * Configure tl_l3_if
	 */
	if (!ppe_drv_tun_l3_if_configure(tun_l3_if)) {
		ppe_drv_warn("Tun l3 if configure failed for port %d", xmit_port);
		ppe_drv_tun_l3_if_deref(tun_l3_if);
		return NULL;
	}

	/*
	 * Attach tl_l3_if to port
	 */
	ppe_drv_port_tl_l3_if_attach(pp, tun_l3_if);
	ppe_drv_trace("TL_L3_IF successfully attached/configured for port %d", xmit_port);

configure_port:
	/*
	 * Configure xmit port with tl_l3_if
	 */
	err = fal_tunnel_port_intf_get(PPE_DRV_SWITCH_ID, xmit_port, &port_tnl_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("Unable to get xmit port config for port %d", xmit_port);
		ppe_drv_tun_l3_if_deref(tun_l3_if);
		return NULL;
	}

	port_tnl_cfg.l3_if.l3_if_valid = A_TRUE;
	port_tnl_cfg.l3_if.l3_if_index = tun_l3_if->index;
	if (pp->mac_valid) {
		memcpy(port_tnl_cfg.mac_addr.uc, &pp->mac_addr[0], ETH_ALEN);
	}

	err = fal_tunnel_port_intf_set(PPE_DRV_SWITCH_ID, xmit_port, &port_tnl_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("Unable to set xmit port config for port %d", xmit_port);
		ppe_drv_tun_l3_if_deref(tun_l3_if);
		return NULL;
	}

	return tun_l3_if;
}

/*
 * ppe_drv_tun_rps_l3_if_deconfigure
 *	Dereference and cleanup TL_L3_IF for RPS tunnel
 */
static bool ppe_drv_tun_rps_l3_if_deconfigure(struct ppe_drv_tun_l3_if *tl_l3_if, struct ppe_drv_pppoe *pppoe)
{
	if (!tl_l3_if) {
		ppe_drv_warn("Invalid NULL tl_l3_if");
		return false;
	}

	if (pppoe) {
		return ppe_drv_pppoe_l3_if_deref(pppoe);
	} else {
		return ppe_drv_tun_l3_if_deref(tl_l3_if);
	}
}

/*
 * ppe_drv_tun_tpr_is_ipv4_zero()
 *	Check if IPv4 address is zero (wildcard)
 */
static bool ppe_drv_tun_tpr_is_ipv4_zero(uint32_t addr)
{
	return (addr == 0);
}

/*
 * ppe_drv_tun_tpr_is_ipv6_zero()
 *	Check if IPv6 address is all-zero (wildcard)
 */
static bool ppe_drv_tun_tpr_is_ipv6_zero(uint32_t addr[4])
{
	return (addr[0] == 0 && addr[1] == 0 && addr[2] == 0 && addr[3] == 0);
}

/*
 * ppe_drv_tun_tpr_build_bitmap()
 *	Build TPR bitmap based on non-zero fields for partial 5-tuple matching
 */
static uint8_t ppe_drv_tun_tpr_build_bitmap(struct ppe_drv_tun_tpr_entry *tuple)
{
	uint8_t bitmap = 0;

	if (!tuple) {
		ppe_drv_warn("Invalid parameter: tuple is NULL");
		return 0;
	}

	/*
	 * Add SIP to bitmap only if non-zero
	 */
	if (tuple->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		if (!ppe_drv_tun_tpr_is_ipv4_zero(tuple->src_ip[0])) {
			bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_SIP_EN);
		}
		if (!ppe_drv_tun_tpr_is_ipv4_zero(tuple->dest_ip[0])) {
			bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_DIP_EN);
		}
	} else if (tuple->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		if (!ppe_drv_tun_tpr_is_ipv6_zero(tuple->src_ip)) {
			bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_SIP_EN);
		}
		if (!ppe_drv_tun_tpr_is_ipv6_zero(tuple->dest_ip)) {
			bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_DIP_EN);
		}
	}

	/*
	 * Add protocol to bitmap only if non-zero
	 */
	if (tuple->protocol != 0) {
		bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_L4PROTO_EN);
	}

	/*
	 * Add ports to bitmap only if non-zero
	 */
	if (tuple->src_port != 0) {
		bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_SPORT_EN);
	}
	if (tuple->dest_port != 0) {
		bitmap |= PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_DPORT_EN);
	}

	/*
	 * Log the partial matching configuration for debugging
	 */
	ppe_drv_info("Partial 5-tuple matching bitmap: 0x%02x (SIP=%s DIP=%s PROTO=%s SPORT=%s DPORT=%s)\n",
		     bitmap,
		     (bitmap & PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_SIP_EN)) ? "Y" : "N",
		     (bitmap & PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_DIP_EN)) ? "Y" : "N",
		     (bitmap & PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_L4PROTO_EN)) ? "Y" : "N",
		     (bitmap & PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_SPORT_EN)) ? "Y" : "N",
		     (bitmap & PPE_DRV_TUN_TPR_SET_BIT(PPE_DRV_TUN_TPR_KEY_DPORT_EN)) ? "Y" : "N");

	return bitmap;
}

/*
 * ppe_drv_tun_rps_dump_rule
 *      Dump tunnel RPS rule
 */
static void ppe_drv_tun_rps_dump_rule(void *rule, bool is_create)
{
	if (is_create) {
		struct ppe_drv_tun_rps_rule_create *prpsc = (struct ppe_drv_tun_rps_rule_create *)rule;
		ppe_drv_info("Tunnel RPS rule create dump from PPE driver layer:\n");
		ppe_drv_info("is_ipv6: %d\n", (prpsc->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6));
		ppe_drv_info("protocol: %d\n", prpsc->cmn.protocol);
		ppe_drv_info("sport: %d\n", prpsc->cmn.flow_ident);
		ppe_drv_info("dport: %d\n", prpsc->cmn.return_ident);
		ppe_drv_info("hdr_len_type: %d\n", prpsc->offset_type);
		ppe_drv_info("hdr_len: %d\n", prpsc->header_len);
		ppe_drv_info("inner_pkt_type: %d\n", prpsc->inner_type);
		ppe_drv_info("usr_data1_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsc->cmn.udf0_en, prpsc->cmn.udf0_offset, prpsc->cmn.udf0_val, prpsc->cmn.udf0_mask);
		ppe_drv_info("usr_data2_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsc->cmn.udf1_en, prpsc->cmn.udf1_offset, prpsc->cmn.udf1_val, prpsc->cmn.udf1_mask);
		ppe_drv_info("usr_data3_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsc->cmn.udf2_en, prpsc->cmn.udf2_offset, prpsc->cmn.udf2_val, prpsc->cmn.udf2_mask);

		/*
		 * Dump IP addresses
		 */
		if (prpsc->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
			ppe_drv_info("SIP IPv4: %pI4\n", &prpsc->cmn.flow_ip[PPE_DRV_TUN_TPR_IPV4_ADDR_INDEX]);
			ppe_drv_info("DIP IPv4: %pI4\n", &prpsc->cmn.return_ip[PPE_DRV_TUN_TPR_IPV4_ADDR_INDEX]);
		} else {
			ppe_drv_info("SIP IPv6: %pI6\n", prpsc->cmn.flow_ip);
			ppe_drv_info("DIP IPv6: %pI6\n", prpsc->cmn.return_ip);
		}

		/*
		 * Dump header match fields based on offset type
		 */
		switch (prpsc->offset_type) {
		case PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH:
			ppe_drv_info("Header Match Type: ETH\n");
			ppe_drv_info("  eth_protocol: 0x%04x\n", prpsc->cmn.header_match.l2_match.eth_protocol);
			ppe_drv_info("  eth_protocol_mask: 0x%04x\n", prpsc->cmn.header_match.l2_match.eth_protocol_mask);
			break;
		case PPE_DRV_TUN_RPS_OFFSET_TYPE_IP:
			ppe_drv_info("Header Match Type: IP\n");
			ppe_drv_info("  ip_protocol: %d\n", prpsc->cmn.header_match.l3_match.ip_protocol);
			ppe_drv_info("  ip_protocol_mask: 0x%02x\n", prpsc->cmn.header_match.l3_match.ip_protocol_mask);
			break;
		case PPE_DRV_TUN_RPS_OFFSET_TYPE_UDP:
			ppe_drv_info("Header Match Type: UDP\n");
			ppe_drv_info("  udp_sport: %d\n", prpsc->cmn.header_match.l4_match.udp_sport);
			ppe_drv_info("  udp_dport: %d\n", prpsc->cmn.header_match.l4_match.udp_dport);
			break;
		default:
			ppe_drv_info("Header Match Type: UNKNOWN (%d)\n", prpsc->offset_type);
			break;
		}

		/*
		 * Dump inner IP protocol fields when inner_pkt_type is IP
		 */
		if (prpsc->inner_type == PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP) {
			ppe_drv_info("Inner IP Proto: offset=%d, ipv4_val=0x%x, ipv4_mask=0x%x, ipv6_val=0x%x, ipv6_mask=0x%x\n",
					prpsc->inner_ip_proto.ip_proto_offset,
					prpsc->inner_ip_proto.ip_proto_ipv4_value,
					prpsc->inner_ip_proto.ip_proto_ipv4_mask,
					prpsc->inner_ip_proto.ip_proto_ipv6_value,
					prpsc->inner_ip_proto.ip_proto_ipv6_mask);
		}

		/*
		 * Dump PPPoE configuration
		 */
		if (prpsc->pppoe_en) {
			ppe_drv_info("PPPoE: enabled, session_id=%u, server_mac=%pM\n",
					prpsc->pppoe_session_id, prpsc->pppoe_server_mac);
		} else {
			ppe_drv_info("PPPoE: disabled\n");
		}
	} else {
		struct ppe_drv_tun_rps_rule_destroy *prpsd = (struct ppe_drv_tun_rps_rule_destroy *)rule;
		ppe_drv_info("Tunnel RPS rule destroy dump from PPE driver layer:\n");
		ppe_drv_info("is_ipv6: %d\n", (prpsd->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6));
		ppe_drv_info("protocol: %d\n", prpsd->cmn.protocol);
		ppe_drv_info("sport: %d\n", prpsd->cmn.flow_ident);
		ppe_drv_info("dport: %d\n", prpsd->cmn.return_ident);
		ppe_drv_info("usr_data1_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsd->cmn.udf0_en, prpsd->cmn.udf0_offset, prpsd->cmn.udf0_val, prpsd->cmn.udf0_mask);
		ppe_drv_info("usr_data2_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsd->cmn.udf1_en, prpsd->cmn.udf1_offset, prpsd->cmn.udf1_val, prpsd->cmn.udf1_mask);
		ppe_drv_info("usr_data3_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsd->cmn.udf2_en, prpsd->cmn.udf2_offset, prpsd->cmn.udf2_val, prpsd->cmn.udf2_mask);
		/*
		 * Dump IP addresses
		 */
		if (prpsd->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
			ppe_drv_info("SIP IPv4: %pI4\n", &prpsd->cmn.flow_ip[PPE_DRV_TUN_TPR_IPV4_ADDR_INDEX]);
			ppe_drv_info("DIP IPv4: %pI4\n", &prpsd->cmn.return_ip[PPE_DRV_TUN_TPR_IPV4_ADDR_INDEX]);
		} else {
			ppe_drv_info("SIP IPv6: %pI6\n", prpsd->cmn.flow_ip);
			ppe_drv_info("DIP IPv6: %pI6\n", prpsd->cmn.return_ip);
		}
	}
}

/*
 * ppe_drv_tun_tpr_entry_exists
 *	Check if tpr tuple configuration exists.
 *
 * Note: This function does not check for the context type here and only 3/5 tuple information
 * 	 is checked to see if an entry already exists. The output of the function can be used
 * 	 to determine if a new instance is required to be allocated or existing entry can be
 * 	 re-used based on requirement
 */
struct ppe_drv_tun_tpr *ppe_drv_tun_tpr_entry_exists(struct ppe_drv *p, struct ppe_drv_tun_tpr_entry *tpr)
{
	uint8_t index = 0;
	struct ppe_drv_tun_tpr *tun_tpr;
	struct ppe_drv_tun_tpr_entry *t_tpre = NULL;

	if (!tpr) {
		ppe_drv_warn("%p: Invalid parameter: tpr is NULL", p);
		return NULL;
	}

	if (tpr->ip_version != PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4 &&
	    tpr->ip_version != PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		ppe_drv_warn("%p: Invalid IP version: %d", tpr, tpr->ip_version);
		return NULL;
	}

	if (tpr->ctx_type != PPE_DRV_TUN_TPR_CONTEXT_TUNNEL &&
	    tpr->ctx_type != PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID) {
		ppe_drv_warn("%p: Invalid context type: %d", tpr, tpr->ctx_type);
		return NULL;
	}

	for (index = 0; index < PPE_DRV_TUN_TPR_ENTRY_MAX; index++) {
		tun_tpr = &p->tun_tpr[index];
		if (kref_read(&tun_tpr->ref)) {
			t_tpre = &tun_tpr->tpre;
			if (t_tpre->ip_version != tpr->ip_version) {
				continue;
			}

			if (t_tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
				if (memcmp(t_tpre->src_ip, tpr->src_ip, sizeof(uint32_t)) ||
						memcmp(t_tpre->dest_ip, tpr->dest_ip, sizeof(uint32_t))) {
					continue;
				}
			} else if (t_tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
				if (memcmp(t_tpre->src_ip, tpr->src_ip, sizeof(tpr->src_ip)) ||
						memcmp(t_tpre->dest_ip, tpr->dest_ip, sizeof(tpr->dest_ip))) {
					continue;
				}
			}

			if ((t_tpre->src_port != tpr->src_port) || (t_tpre->dest_port != tpr->dest_port) ||
					(t_tpre->protocol != tpr->protocol) || (t_tpre->tpr_bitmap != tpr->tpr_bitmap)) {
				continue;
			}

			return tun_tpr;
		}
	}

	return NULL;
}

/*
 * ppe_drv_tun_tpr_dump_tuple_entry
 *	Dumps contents of fal_tunnel_tuple_entry_t
 */
static void ppe_drv_tun_tpr_dump_tuple_entry(struct ppe_drv_tun_tpr_entry *tpre, fal_tunnel_tuple_entry_t *tuple_entry)
{
	ppe_drv_trace("%p: IP ver:%d, Key bitmap:0x%x, L4 Proto:%d", tpre, tuple_entry->ip_ver, tuple_entry->key_bmp, tuple_entry->l4_proto);
	ppe_drv_trace("%p: sport:%d, dport:%d",  tpre, tuple_entry->sport, tuple_entry->dport);

	if (tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		ppe_drv_trace("%p: SIP: %pI4, DIP: %pI4", tpre, &tuple_entry->sip.ip4_addr, &tuple_entry->dip.ip4_addr);
	} else {
		ppe_drv_trace("%p: SIP: %pI6, DIP: %pI6", tpre, &tuple_entry->sip.ip6_addr, &tuple_entry->dip.ip6_addr);
	}

	if (tpre->ctx_type == PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID) {
		ppe_drv_trace("%p: Context Type: Tuple ID, Value:%d", tpre, tuple_entry->context.tuple_id);
	} else {
		ppe_drv_trace("%p: Context Type: Tunnel Type, Value:%d", tpre, tuple_entry->context.tunnel_type);
	}
}

/*
 * ppe_drv_tun_tpr_entry_configure
 *	Configure tunnel tpr entry
 */
bool ppe_drv_tun_tpr_entry_configure(struct ppe_drv_tun_tpr_entry *tpre)
{
	fal_tunnel_tuple_entry_t tuple_entry = {0};
	sw_error_t err;

	if (!tpre) {
		ppe_drv_warn("Invalid parameter: tpre is NULL\n");
		return false;
	}

	if (tpre->ip_version != PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4 &&
	    tpre->ip_version != PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		ppe_drv_warn("%p: Invalid IP version: %d", tpre, tpre->ip_version);
		return false;
	}

	if (tpre->ctx_type != PPE_DRV_TUN_TPR_CONTEXT_TUNNEL &&
	    tpre->ctx_type != PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID) {
		ppe_drv_warn("%p: Invalid context type: %d", tpre, tpre->ctx_type);
		return false;
	}

	tuple_entry.ip_ver = tpre->ip_version;
	tuple_entry.key_bmp = tpre->tpr_bitmap;

	if (tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		tuple_entry.sip.ip4_addr = tpre->src_ip[0];
		tuple_entry.dip.ip4_addr = tpre->dest_ip[0];
	} else if (tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		tuple_entry.sip.ip6_addr.ul[0] = tpre->src_ip[0];
		tuple_entry.sip.ip6_addr.ul[1] = tpre->src_ip[1];
		tuple_entry.sip.ip6_addr.ul[2] = tpre->src_ip[2];
		tuple_entry.sip.ip6_addr.ul[3] = tpre->src_ip[3];

		tuple_entry.dip.ip6_addr.ul[0] = tpre->dest_ip[0];
		tuple_entry.dip.ip6_addr.ul[1] = tpre->dest_ip[1];
		tuple_entry.dip.ip6_addr.ul[2] = tpre->dest_ip[2];
		tuple_entry.dip.ip6_addr.ul[3] = tpre->dest_ip[3];
	}

	tuple_entry.context_type = (fal_tunnel_tuple_context_type_t)tpre->ctx_type;
	if (tpre->ctx_type == PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID) {
		tuple_entry.context.tuple_id = tpre->tuple_id;
	} else {
		tuple_entry.context.tunnel_type = tpre->tunnel_type;
	}

	tuple_entry.sport = tpre->src_port;
	tuple_entry.dport = tpre->dest_port;
	tuple_entry.l4_proto = tpre->protocol;

	ppe_drv_tun_tpr_dump_tuple_entry(tpre, &tuple_entry);

	/*
	 * Configure TPR-TUPLE table to match 5 tuple information
	 */
	err = fal_tunnel_tuple_entry_add(PPE_DRV_SWITCH_ID, &tuple_entry);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Tunnel TPR entry configuration failed with error %d", tpre, err);
		return false;
	}

	ppe_drv_trace("%p: TPR configuration successful", tpre);
	return true;
}

/*
 * ppe_drv_tun_tpr_entry_free
 *	free tunnel tpr entry
 */
static void ppe_drv_tun_tpr_entry_free(struct kref *kref)
{
	struct ppe_drv_tun_tpr  *tun_tpr = container_of(kref, struct ppe_drv_tun_tpr, ref);
	struct ppe_drv_tun_tpr_entry *tpre = &tun_tpr->tpre;
	fal_tunnel_tuple_entry_t tuple_entry = {0};
	sw_error_t err;

	if (tpre->ip_version != PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4 &&
			tpre->ip_version != PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		ppe_drv_warn("%p: Invalid IP version: %d during TPR entry free", tpre, tpre->ip_version);
		return;
	}

	if (tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		memcpy(&tuple_entry.sip.ip4_addr, tpre->src_ip, sizeof(uint32_t));
		memcpy(&tuple_entry.dip.ip4_addr, tpre->dest_ip, sizeof(uint32_t));
		tuple_entry.ip_ver = PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4;
	} else if (tpre->ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		memcpy(&tuple_entry.sip.ip6_addr, tpre->src_ip, sizeof(tpre->src_ip));
		memcpy(&tuple_entry.dip.ip6_addr, tpre->dest_ip, sizeof(tpre->dest_ip));
		tuple_entry.ip_ver = PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6;
	}

	if (tpre->ctx_type == PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID) {
		tuple_entry.context_type = FAL_TUNNEL_TUPLE_CONTEXT_TUPLE_ID;
		tuple_entry.context.tuple_id = tpre->tuple_id;
	} else {
		tuple_entry.context_type = FAL_TUNNEL_TUPLE_CONTEXT_TUNNEL_TYPE;
		tuple_entry.context.tunnel_type = tpre->tunnel_type;
	}

	tuple_entry.sport = tpre->src_port;
	tuple_entry.dport = tpre->dest_port;
	tuple_entry.l4_proto = tpre->protocol;
	tuple_entry.key_bmp = tpre->tpr_bitmap;

	ppe_drv_tun_tpr_dump_tuple_entry(tpre, &tuple_entry);

	err = fal_tunnel_tuple_entry_del(PPE_DRV_SWITCH_ID, &tuple_entry);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Tunnel TPR entry deletion failed with error %d", tpre, err);
		return;
	}

	/*
	 * Reset the memory of the entry.
	 */
	memset(tpre, 0, sizeof(*tpre));
	ppe_drv_trace("%p: TPR[%d] freed successfully", tpre, tun_tpr->tpr_index);
}

/*
 * ppe_drv_tun_tpr_entry_deref
 *	deref tunnel tpr entry
 */
bool ppe_drv_tun_tpr_entry_deref(struct ppe_drv_tun_tpr  *tun_tpr)
{
	int index __maybe_unused;

	if (!tun_tpr) {
		ppe_drv_warn("Invalid parameter: tun_tpr is NULL");
		return false;
	}

	index = tun_tpr->tpr_index;

	ppe_drv_assert(kref_read(&tun_tpr->ref), "ref count under run for tpr index: %d", index);
	if (kref_put(&tun_tpr->ref, ppe_drv_tun_tpr_entry_free)) {
		ppe_drv_trace("%p: reference count is 0 for tpr index: %d",tun_tpr, index);
		return true;
	}
	ppe_drv_trace("%p: index: %u ref dnc:%u", tun_tpr, index, kref_read(&tun_tpr->ref));

	return true;
}

/*
 * ppe_drv_tun_tpr_entry_ref
 *	take ref on tunnel tpr entry
 */
void ppe_drv_tun_tpr_entry_ref(struct ppe_drv_tun_tpr  *tun_tpr)
{
	/*
	 * Input validation
	 */
	if (!tun_tpr) {
		ppe_drv_warn("Invalid parameter: tunnel tpr context is NULL");
		return;
	}

	kref_get(&tun_tpr->ref);

	ppe_drv_assert(kref_read(&tun_tpr->ref), "%p: ref count rollover for tpr index:%d", tun_tpr, tun_tpr->tpr_index);
	ppe_drv_trace("%p: idx: %u ref inc:%u", tun_tpr, tun_tpr->tpr_index, kref_read(&tun_tpr->ref));
}

/*
 * ppe_drv_tun_tpr_entry_alloc
 *	Return first free tunnel tpr entry
 *
 * Note: This function does not take into account of duplicate entries.
 * 	 Its expected that the user of the function needs to manage duplicate entries
 * 	 as required based on use case.
 */
struct ppe_drv_tun_tpr *ppe_drv_tun_tpr_entry_alloc(struct ppe_drv *p)
{
	uint8_t index = 0;
	struct ppe_drv_tun_tpr *tpr;

	/*
	 * Return first free instance
	 */
	for (index = 0; index < PPE_DRV_TUN_TPR_ENTRY_MAX; index++) {
		tpr = &p->tun_tpr[index];
		if (kref_read(&tpr->ref)) {
			continue;
		}

		kref_init(&tpr->ref);
		ppe_drv_trace("%p: Free tunnel tpr instance found, index: %d", tpr, index);
		return tpr;
	}

	ppe_drv_warn("%p: Free tunnel tpr instance is not found", p);
	return NULL;
}

/*
 * ppe_drv_tun_tpr_free
 *	free tunnel tpr entries
 */
void ppe_drv_tun_tpr_free(struct ppe_drv_tun_tpr *tun_tpr)
{
	vfree(tun_tpr);
}

/*
 * ppe_drv_tun_tpr_alloc
 *	Allocate and Initialize tunnel TPR entries
 */
struct ppe_drv_tun_tpr *ppe_drv_tun_tpr_alloc(struct ppe_drv *p)
{
	struct ppe_drv_tun_tpr *tpr;
	int index;

	ppe_drv_assert(!p->tun_tpr, "%p: tunnel tpr entries already allocated", p);

	tpr = vzalloc(sizeof(struct ppe_drv_tun_tpr) * PPE_DRV_TUN_TPR_ENTRY_MAX);
	if (!tpr) {
		ppe_drv_warn("%p: failed to allocate tunnel tpr entries", p);
		return NULL;
	}

	for (index = 0; index < PPE_DRV_TUN_TPR_ENTRY_MAX; index++) {
		tpr[index].tpr_index = index;

		/*
		 * Initialize the tuple id generated by the TPR tuple configurations
		 * to index value. Valid only in case the context is set to tuple id.
		 */
		tpr[index].tpre.tuple_id = index;

		/*
		 * For context being set to tunnel type. The value can be any valid TUPLE tunnel type.
		 * During initialization its configured to "FAL_TUNNEL_TYPE_INVALID_TUNNEL" to avoid
		 * initializing to tunnel type 0(ie. FAL_TUNNEL_TYPE_GRE_TAP_OVER_IPV4)
		 */
		tpr[index].tpre.tunnel_type = FAL_TUNNEL_TYPE_INVALID_TUNNEL;
	}

	nss_ppe_drv_minidump_log(tpr, sizeof(struct ppe_drv_tun_tpr) * PPE_DRV_TUN_TPR_ENTRY_MAX, "ppe_drv_tun_tpr");

	return tpr;
}

/*
 * ppe_drv_tun_tpr_prsr_check_udf
 *      Compare Program UDF values configured in Parser with given udf configuration
 */
static bool ppe_drv_tun_tpr_prsr_check_udf(struct ppe_drv_tun_prgm_prsr *pgm, struct ppe_drv_tun_tpr_prsr_udf *udf)
{
	int i;
	struct ppe_drv_tun_prgm_prsr_prgm_udf_cfg *udf_cfg = &pgm->ctx.prsr_cfg.conf.udf;
	struct ppe_drv_tun_prgm_prsr_tpr *tpr = &pgm->ctx.data.rps;

	for (i = 0; i < PPE_DRV_TUN_PRGM_UDF_MAX; i++) {
		if (!udf->udf_en[i]) {
			continue;
		}

		if (udf->udf_offset[i] != udf_cfg->udf_offset[i]) {
			return false;
		}

		if (tpr->eth_udf_valid && ((tpr->eth_udf.udf_val[i] != udf->udf_val[i])  ||  (tpr->eth_udf.udf_mask[i] != udf->udf_mask[i]) )) {
			return false;
		}

		/*
		 * Check only for IPv4 udf as IPv6 UDF would be same values and differ only in inner header type check
		 */
		if (tpr->ipv4_udf_valid && ((tpr->ipv4_udf.udf_val[i] != udf->udf_val[i])  ||  (tpr->ipv4_udf.udf_mask[i] != udf->udf_mask[i]) )) {
			return false;
		}
	}

	return true;
}

/*
 * ppe_drv_tun_tpr_get_prgm_prsr
 *      Get program Parser attached to TPR instance
 */
static struct ppe_drv_tun_prgm_prsr *ppe_drv_tun_tpr_get_prgm_prsr(struct ppe_drv_tun_tpr *tpr, struct ppe_drv_tun_tpr_prsr_udf *udf_cfg)
{
	int i;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_prgm_prsr *pgm = p->pgm;

	for (i = 0; i < PPE_DRV_TUN_PRGM_PRSR_MAX; i++) {
		if (pgm[i].ctx.mode == PPE_DRV_TUN_PROGRAM_MODE_TPR_RPS) {
			if (pgm[i].ctx.data.rps.tpr == tpr &&
					ppe_drv_tun_tpr_prsr_check_udf(&pgm[i], udf_cfg)) {
				return &pgm[i];
			}
		}
	}

	return NULL;
}

/*
 * ppe_drv_tun_rps_prgm_prsr_deconfigure
 *      Deconfigure Program Parser instance configured for RPS mode
 */
bool ppe_drv_tun_rps_prgm_prsr_deconfigure(struct ppe_drv_tun_prgm_prsr *prsr)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_prgm_prsr_tpr *tpr = &prsr->ctx.data.rps;
	ppe_drv_assert((prsr->ctx.mode == PPE_DRV_TUN_PROGRAM_MODE_TPR_RPS), "program mode not RPS for program type : %d", prsr->parser_idx);

	if (!ppe_drv_tun_prgm_prsr_deconfigure(&prsr->ctx.prsr_cfg, prsr->parser_idx)) {
		ppe_drv_warn("%p: program entry delete failed for RPS Configuration", p);
		return false;
	}

	if (tpr->eth_udf_valid && !ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(prsr->parser_idx, &tpr->eth_udf)) {
		ppe_drv_warn("%p: ETH program UDF entry delete failed for TPR entry", p);
		return false;
	}

	if (tpr->ipv4_udf_valid && !ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(prsr->parser_idx, &tpr->ipv4_udf)) {
		ppe_drv_warn("%p: IPV4 program UDF entry delete failed for TPR entry", p);
		return false;
	}

	if (tpr->ipv6_udf_valid && !ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(prsr->parser_idx, &tpr->ipv6_udf)) {
		ppe_drv_warn("%p: IPV6 program UDF entry delete failed for TPR entry", p);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_rps_rule_destroy
 *      Destroy RPS rule pushed to PPE
 */
ppe_drv_ret_t ppe_drv_tun_rps_rule_destroy(struct ppe_drv_tun_rps_rule_destroy *rule_destroy)
{
	struct ppe_drv_tun_prgm_prsr *pgm;
	struct ppe_drv_tun_tpr_entry tuple_info = {0};
	struct ppe_drv_tun_tpr *tun_tpr = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_tpr_prsr_udf tpr_udf = {0};
	struct ppe_drv_pppoe *pppoe = NULL;
	struct ppe_drv_tun_l3_if *tun_l3_if = NULL;
	uint8_t i;

	ppe_drv_tun_rps_dump_rule(rule_destroy, false);

	/*
	 * Lock to protect TPR entry operations from concurrent access
	 */
	spin_lock_bh(&p->lock);

	tuple_info.ip_version = rule_destroy->cmn.rule_type;
	if (tuple_info.ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		tuple_info.src_ip[0] = rule_destroy->cmn.flow_ip[0];
		tuple_info.dest_ip[0] = rule_destroy->cmn.return_ip[0];
	} else if (tuple_info.ip_version == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		for (i = 0; i < PPE_DRV_TUN_TPR_IPV6_ADDR_WORDS; i++) {
			tuple_info.src_ip[i] = rule_destroy->cmn.flow_ip[i];
			tuple_info.dest_ip[i] = rule_destroy->cmn.return_ip[i];
		}
	}

	tuple_info.src_port = rule_destroy->cmn.flow_ident;
	tuple_info.dest_port = rule_destroy->cmn.return_ident;
	tuple_info.protocol = rule_destroy->cmn.protocol;

	/*
	 * Build TPR bitmap dynamically based on non-zero fields for partial 5-tuple matching.
	 */
	tuple_info.tpr_bitmap = ppe_drv_tun_tpr_build_bitmap(&tuple_info);

	tun_tpr = ppe_drv_tun_tpr_entry_exists(p, &tuple_info);
	if (!tun_tpr) {
		ppe_drv_warn("%p: TPR entry not found for RPS rule destroy", rule_destroy);
		spin_unlock_bh(&p->lock);
		return  PPE_DRV_RET_TUN_RPS_DESTROY_RULE_FAIL;
	}

	if (rule_destroy->cmn.udf0_en) {
		tpr_udf.udf_en[PPE_DRV_TUN_TPR_UDF0_INDEX] = true;
		tpr_udf.udf_offset[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_destroy->cmn.udf0_offset;
		tpr_udf.udf_val[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_destroy->cmn.udf0_val;
		tpr_udf.udf_mask[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_destroy->cmn.udf0_mask;
	}

	if (rule_destroy->cmn.udf1_en) {
		tpr_udf.udf_en[PPE_DRV_TUN_TPR_UDF1_INDEX] = true;
		tpr_udf.udf_offset[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_destroy->cmn.udf1_offset;
		tpr_udf.udf_val[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_destroy->cmn.udf1_val;
		tpr_udf.udf_mask[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_destroy->cmn.udf1_mask;
	}

	if (rule_destroy->cmn.udf2_en) {
		tpr_udf.udf_en[PPE_DRV_TUN_TPR_UDF2_INDEX] = true;
		tpr_udf.udf_offset[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_destroy->cmn.udf2_offset;
		tpr_udf.udf_val[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_destroy->cmn.udf2_val;
		tpr_udf.udf_mask[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_destroy->cmn.udf2_mask;
	}

	/*
	 * Traverse through all configured Parsers with mode "PPE_DRV_TUN_PROGRAM_MODE_RPS"
	 * and match 5-tuple information within pgm.ctx.data.rps with the destroy request to get
	 * matching parser instance
	 */
	pgm = ppe_drv_tun_tpr_get_prgm_prsr(tun_tpr, &tpr_udf);
	if (!pgm) {
		ppe_drv_warn("%p: Parser instance not found for destroy request", rule_destroy);
		spin_unlock_bh(&p->lock);
		return  PPE_DRV_RET_TUN_RPS_DESTROY_RULE_FAIL;
	}

	/*
	 * deref tun_l3_if
	 */
	if (pgm->ctx.data.rps.tun_l3_if) {
		if (pgm->ctx.data.rps.pppoe_en) {
			pppoe = ppe_drv_pppoe_find_session(pgm->ctx.data.rps.pppoe_session_id, pgm->ctx.data.rps.pppoe_server_mac);
			if (!pppoe) {
				ppe_drv_warn("Could not find PPPoE session %x mac %pM", pgm->ctx.data.rps.pppoe_session_id, pgm->ctx.data.rps.pppoe_server_mac );
			}
		}

		tun_l3_if = pgm->ctx.data.rps.tun_l3_if;
		pgm->ctx.data.rps.tun_l3_if = NULL;
	}

	/*
	 * Dereference the program parser instance. This will also deconfigure it
	 * if it's the last reference.
	 */
	if (!ppe_drv_tun_prgm_prsr_deref(pgm)) {
		ppe_drv_warn("%p: Failed to dereference program parser for RPS rule destroy", rule_destroy);
		spin_unlock_bh(&p->lock);
		return  PPE_DRV_RET_TUN_RPS_DESTROY_RULE_FAIL;
	}

	/*
	 * The ppe_drv_tun_tpr_entry_deref function handles fal_tunnel_tuple_entry_del
	 * within its free callback.
	 */
	if (!ppe_drv_tun_tpr_entry_deref(tun_tpr)) {
		ppe_drv_warn("%p: Failed to delete TPR entry for RPS rule destroy", rule_destroy);
		spin_unlock_bh(&p->lock);
		return  PPE_DRV_RET_TUN_RPS_DESTROY_RULE_FAIL;
	}

	spin_unlock_bh(&p->lock);

	/*
	 * Deconfigure TL_L3_IF outside the lock to avoid deadlock
	 */
	if (tun_l3_if) {
		if (!ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, pppoe)) {
			ppe_drv_warn("TL_L3_IF deref failed\n");
		}
	}

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_tun_rps_rule_destroy);

/*
 * ppe_drv_tun_rps_rule_create
 *      Create RPS rule in PPE
 */
ppe_drv_ret_t ppe_drv_tun_rps_rule_create(struct ppe_drv_tun_rps_rule_create *rule_create)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_tpr *tpr;
	struct ppe_drv_tun_tpr_entry *tuple;
	struct ppe_drv_tun_prgm_prsr *pgm;
	struct ppe_drv_tun_prgm_prsr_prgm_udf *eth_udf = NULL, *ipv4_udf = NULL, *ipv6_udf = NULL;
	uint8_t i;
	struct ppe_drv_tun_l3_if *tun_l3_if = NULL;
	bool ip_inner_proto_en = false;
	int16_t xmit_port;

	ppe_drv_tun_rps_dump_rule(rule_create, true);

	/*
	 * Get xmit port from interface name outside the lock to avoid deadlock
	 */
	xmit_port = ppe_drv_port_num_from_ifname(rule_create->wan_if);
	if (xmit_port == PPE_DRV_PORT_ID_INVALID) {
		ppe_drv_warn("Failed to get xmit port for dev %s", rule_create->wan_if);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	/*
	* Validate and adjust offsets if base is Ethernet header.
	* The hardware starts its offset counter from the EtherType field (byte 12).
	*/
	if (rule_create->offset_type == PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH) {
		if (rule_create->header_len < ETH_HLEN) {
			ppe_drv_warn("%p: Invalid header_len %d for ETH offset type (min %d)",
					rule_create, rule_create->header_len, ETH_HLEN);
			return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
		}

		/* Adjust header length */
		rule_create->header_len -= (2 * ETH_ALEN);

		/*
		 * Adjust UDF offsets with protection against unsigned underflow
		 */
		if (rule_create->cmn.udf0_en) {
			if (rule_create->cmn.udf0_offset < (2 * ETH_ALEN)) {
				ppe_drv_warn("%p: udf0_offset %d is less than 12", rule_create, rule_create->cmn.udf0_offset);
				return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
			}
			rule_create->cmn.udf0_offset -= (2 * ETH_ALEN);
		}
		if (rule_create->cmn.udf1_en) {
			if (rule_create->cmn.udf1_offset < (2 * ETH_ALEN)) {
				ppe_drv_warn("%p: udf1_offset %d is less than 12", rule_create, rule_create->cmn.udf1_offset);
				return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
			}
			rule_create->cmn.udf1_offset -= (2 * ETH_ALEN);
		}
		if (rule_create->cmn.udf2_en) {
			if (rule_create->cmn.udf2_offset < (2 * ETH_ALEN)) {
				ppe_drv_warn("%p: udf2_offset %d is less than 12", rule_create, rule_create->cmn.udf2_offset);
				return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
			}
			rule_create->cmn.udf2_offset -= (2 * ETH_ALEN);
		}

		/*
		 * Adjust Inner IP protocol offset with protection against unsigned underflow
		 */
		if (rule_create->inner_type == PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP) {
			if (rule_create->inner_ip_proto.ip_proto_offset < (2 * ETH_ALEN)) {
				ppe_drv_warn("%p: ip_proto_offset %d is less than 12", rule_create, rule_create->inner_ip_proto.ip_proto_offset);
				return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
			}
			rule_create->inner_ip_proto.ip_proto_offset -= (2 * ETH_ALEN);
		}
	}

	/*
	 * Lock to protect TPR entry operations from concurrent access
	 */
	spin_lock_bh(&p->lock);

	/*
	 * Allocate and ref TL_L3_IF for the tunnel port.
	 */
	if (rule_create->pppoe_en) {
		tun_l3_if = ppe_drv_tun_tpr_pppoe_l3_if_configure(xmit_port, rule_create->pppoe_session_id, rule_create->pppoe_server_mac);
	} else {
		tun_l3_if = ppe_drv_tun_tpr_l3_if_configure(xmit_port);
	}

	if (!tun_l3_if) {
		ppe_drv_warn("%p: Failed to get tl_l3_if for port %d", rule_create, xmit_port);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	tpr = ppe_drv_tun_tpr_entry_alloc(p);
	if (!tpr) {
		ppe_drv_warn("%p: failed to allocate TPR entry", rule_create);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	/*
	 * Allocate program Parser instance. Also takes initial ref on the parser allocated
	 */
	pgm = ppe_drv_tun_prgm_prsr_entry_alloc(PPE_DRV_TUN_PROGRAM_MODE_TPR_RPS, NULL);
	if (!pgm) {
		ppe_drv_warn("%p: failed to allocate program parser for RPS rule creation", rule_create);
		ppe_drv_tun_tpr_entry_deref(tpr);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	tuple = &tpr->tpre;

	tuple->ip_version = rule_create->cmn.rule_type;

	if (rule_create->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		tuple->src_ip[0] = rule_create->cmn.flow_ip[0];
		tuple->dest_ip[0] = rule_create->cmn.return_ip[0];
	} else if (rule_create->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		for (i = 0; i < PPE_DRV_TUN_TPR_IPV6_ADDR_WORDS; i++) {
			tuple->src_ip[i] = rule_create->cmn.flow_ip[i];
			tuple->dest_ip[i] = rule_create->cmn.return_ip[i];
		}
	}

	tuple->src_port = rule_create->cmn.flow_ident;
	tuple->dest_port = rule_create->cmn.return_ident;
	tuple->protocol = rule_create->cmn.protocol;
	tuple->ctx_type = PPE_DRV_TUN_TPR_CONTEXT_TUPLE_ID;

	/*
	 * Take tpr_index as Tuple ID
	 */
	tuple->tuple_id = tpr->tpr_index;

	/*
	 * Build TPR bitmap dynamically based on non-zero fields for partial 5-tuple matching.
	 */
	tuple->tpr_bitmap = ppe_drv_tun_tpr_build_bitmap(tuple);

	if (!ppe_drv_tun_tpr_entry_configure(tuple)) {
		ppe_drv_warn("%p: Failed to configure TPR entry for RPS rule creation", rule_create);
		ppe_drv_tun_tpr_entry_deref(tpr);
		ppe_drv_tun_prgm_prsr_deref(pgm);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	/*
	 * Configure Program Parser
	 */
	pgm->ctx.key.decap_en_action = true;
	pgm->ctx.key.service_code_en = true;
	pgm->ctx.key.service_code = PPE_DRV_SC_NOEDIT_TUN_RPS;

	if (rule_create->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
		pgm->ctx.prsr_cfg.ip_ver = PPE_DRV_TUN_PRGM_PRSR_IP_VER_IPV4;
	} else if (rule_create->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV6) {
		pgm->ctx.prsr_cfg.ip_ver = PPE_DRV_TUN_PRGM_PRSR_IP_VER_IPV6;
	}

	switch (rule_create->offset_type) {
	case PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH:
		pgm->ctx.prsr_cfg.outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_ETH;
		pgm->ctx.prsr_cfg.protocol = rule_create->cmn.header_match.l2_match.eth_protocol;
		pgm->ctx.prsr_cfg.protocol_mask = rule_create->cmn.header_match.l2_match.eth_protocol_mask;
		break;

	case PPE_DRV_TUN_RPS_OFFSET_TYPE_IP:
		if (rule_create->cmn.rule_type == PPE_DRV_TUN_TPR_ENTRY_TYPE_IPV4) {
			pgm->ctx.prsr_cfg.outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_IPV4;
		} else {
			pgm->ctx.prsr_cfg.outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_IPV6;
		}

		pgm->ctx.prsr_cfg.protocol = rule_create->cmn.header_match.l3_match.ip_protocol;
		pgm->ctx.prsr_cfg.protocol_mask = rule_create->cmn.header_match.l3_match.ip_protocol_mask;
		break;

	case PPE_DRV_TUN_RPS_OFFSET_TYPE_UDP:
		pgm->ctx.prsr_cfg.outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_UDP;
		pgm->ctx.prsr_cfg.protocol = ((uint32_t)rule_create->cmn.header_match.l4_match.udp_dport << 16) + rule_create->cmn.header_match.l4_match.udp_sport;
		pgm->ctx.prsr_cfg.protocol_mask = 0;
		if (rule_create->cmn.header_match.l4_match.udp_dport) {
			pgm->ctx.prsr_cfg.protocol_mask |= 0xffff0000;
		}

		if (rule_create->cmn.header_match.l4_match.udp_sport) {
			pgm->ctx.prsr_cfg.protocol_mask |= 0x0000ffff;
		}
		break;

	default:
		ppe_drv_warn("%p: Invalid offset type: %u", rule_create, rule_create->offset_type);
		ppe_drv_tun_tpr_entry_deref(tpr);
		ppe_drv_tun_prgm_prsr_deref(pgm);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	/*
	 * Configure Parser in udf mode.
	 */
	pgm->ctx.prsr_cfg.inner_mode = PPE_DRV_TUN_PRGM_PRSR_INNER_MODE_UDF;
	pgm->ctx.prsr_cfg.conf.udf.hdr_len = rule_create->header_len;
	pgm->ctx.prsr_cfg.conf.udf.len_unit = 0; /* length unit is 1 byte */
	pgm->ctx.prsr_cfg.pos_mode = PPE_DRV_TUN_PRGM_PRSR_POS_MODE_START;
	pgm->ctx.data.rps.tpr = tpr;

	/*
	 * Inner header type for needs to be IP/ETHERNET.
	 */
	if (rule_create->inner_type == PPE_DRV_TUN_RPS_INNER_PKT_TYPE_ETH) {
		eth_udf = &pgm->ctx.data.rps.eth_udf;
		pgm->ctx.data.rps.eth_udf_valid = true;

		/*
		 * If UDF fields are not configured and inner packet type is ethernet.
		 * Add a UDF entry to match only the inner packet type as ethernet
		 */
		if (!(rule_create->cmn.udf0_en || rule_create->cmn.udf1_en || rule_create->cmn.udf2_en)) {
			eth_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
			ppe_drv_tun_prgm_udf_action_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
		}
	} else if (rule_create->inner_type == PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP) {
		pgm->ctx.data.rps.ipv4_udf_valid = true;
		ipv4_udf = &pgm->ctx.data.rps.ipv4_udf;
		pgm->ctx.data.rps.ipv6_udf_valid = true;
		ipv6_udf = &pgm->ctx.data.rps.ipv6_udf;
		ip_inner_proto_en = true;
	} else {
		ppe_drv_warn("%p: Invalid inner header type %d for tunnel RPS rule create", rule_create, rule_create->inner_type);
		ppe_drv_tun_tpr_entry_deref(tpr);
		ppe_drv_tun_prgm_prsr_deref(pgm);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	if (rule_create->cmn.udf0_en) {
		pgm->ctx.prsr_cfg.conf.udf.udf_offset[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_offset;
		if (eth_udf) {
			ppe_drv_tun_prgm_udf_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
			eth_udf->udf_val[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_val;
			eth_udf->udf_mask[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_mask;
			eth_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
			ppe_drv_tun_prgm_udf_action_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
		}

		if (ipv4_udf && ipv6_udf) {
			ppe_drv_tun_prgm_udf_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
			ipv4_udf->udf_val[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_val;
			ipv4_udf->udf_mask[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_mask;
			ipv4_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV4;
			ppe_drv_tun_prgm_udf_action_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
			ppe_drv_tun_prgm_udf_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
			ipv6_udf->udf_val[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_val;
			ipv6_udf->udf_mask[PPE_DRV_TUN_TPR_UDF0_INDEX] = rule_create->cmn.udf0_mask;
			ipv6_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV6;
			ppe_drv_tun_prgm_udf_action_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);

		}
	}

	if (rule_create->cmn.udf1_en) {
		pgm->ctx.prsr_cfg.conf.udf.udf_offset[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_offset;
		if (eth_udf) {
			ppe_drv_tun_prgm_udf_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
			eth_udf->udf_val[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_val;
			eth_udf->udf_mask[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_mask;
			eth_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
			ppe_drv_tun_prgm_udf_action_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
		}

		if (ipv4_udf && ipv6_udf) {
			ppe_drv_tun_prgm_udf_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
			ipv4_udf->udf_val[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_val;
			ipv4_udf->udf_mask[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_mask;
			ipv4_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV4;
			ppe_drv_tun_prgm_udf_action_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
			ppe_drv_tun_prgm_udf_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
			ipv6_udf->udf_val[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_val;
			ipv6_udf->udf_mask[PPE_DRV_TUN_TPR_UDF1_INDEX] = rule_create->cmn.udf1_mask;
			ipv6_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV6;
			ppe_drv_tun_prgm_udf_action_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);

		}
	}

	if (rule_create->cmn.udf2_en || ip_inner_proto_en) {
		if (eth_udf) {
			pgm->ctx.prsr_cfg.conf.udf.udf_offset[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->cmn.udf2_offset;
			ppe_drv_tun_prgm_udf_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF2);
			eth_udf->udf_val[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->cmn.udf2_val;
			eth_udf->udf_mask[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->cmn.udf2_mask;
			eth_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
			ppe_drv_tun_prgm_udf_action_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
		}

		if (ip_inner_proto_en && ipv4_udf && ipv6_udf) {
			pgm->ctx.prsr_cfg.conf.udf.udf_offset[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->inner_ip_proto.ip_proto_offset;
			ppe_drv_tun_prgm_udf_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF2);
			ipv4_udf->udf_val[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->inner_ip_proto.ip_proto_ipv4_value;
			ipv4_udf->udf_mask[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->inner_ip_proto.ip_proto_ipv4_mask;
			ipv4_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV4;
			ppe_drv_tun_prgm_udf_action_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
			ppe_drv_tun_prgm_udf_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF2);
			ipv6_udf->udf_val[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->inner_ip_proto.ip_proto_ipv6_value;
			ipv6_udf->udf_mask[PPE_DRV_TUN_TPR_UDF2_INDEX] = rule_create->inner_ip_proto.ip_proto_ipv6_mask;
			ipv6_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV6;
			ppe_drv_tun_prgm_udf_action_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
		}
	}

	pgm->ctx.prsr_cfg.tuple_id_valid = true;
	pgm->ctx.prsr_cfg.tuple_id = tuple->tuple_id;

	if (tuple->tpr_bitmap & (1 << PPE_DRV_TUN_TPR_KEY_SIP_EN)) {
		pgm->ctx.key.key_bitmap |= (1 << PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_SRC_IP);
	}
	if (tuple->tpr_bitmap & (1 << PPE_DRV_TUN_TPR_KEY_DIP_EN)) {
		pgm->ctx.key.key_bitmap |= (1 << PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_DEST_IP);
	}
	if (tuple->tpr_bitmap & (1 << PPE_DRV_TUN_TPR_KEY_L4PROTO_EN)) {
		pgm->ctx.key.key_bitmap |= (1 << PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_L4_PROTO);
	}
	if (tuple->tpr_bitmap & (1 << PPE_DRV_TUN_TPR_KEY_SPORT_EN)) {
		pgm->ctx.key.key_bitmap |= (1 << PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_SPORT);
	}
	if (tuple->tpr_bitmap & (1 << PPE_DRV_TUN_TPR_KEY_DPORT_EN)) {
		pgm->ctx.key.key_bitmap |= (1 << PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_DPORT);
	}

	if (!ppe_drv_tun_prgm_prsr_configure(&pgm->ctx.prsr_cfg, &pgm->ctx.key, pgm->parser_idx)) {
		ppe_drv_warn("%p: Failed to configure program parser for RPS rule creation", rule_create);
		ppe_drv_tun_tpr_entry_deref(tpr);
		ppe_drv_tun_prgm_prsr_deref(pgm);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	/*
	 * Add Program Parser UDF entries if applicable
	 */
	if ((eth_udf && !ppe_drv_tun_prgm_prsr_prgm_udf_configure(pgm->parser_idx, eth_udf)) ||
			(ipv4_udf && !ppe_drv_tun_prgm_prsr_prgm_udf_configure(pgm->parser_idx, ipv4_udf)) ||
			(ipv6_udf && !ppe_drv_tun_prgm_prsr_prgm_udf_configure(pgm->parser_idx, ipv6_udf))) {
		ppe_drv_warn("%p: Failed to configure UDF for RPS rule", rule_create);
		ppe_drv_tun_tpr_entry_deref(tpr);
		ppe_drv_tun_prgm_prsr_deref(pgm);
		spin_unlock_bh(&p->lock);
		ppe_drv_tun_rps_l3_if_deconfigure(tun_l3_if, NULL);
		return PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL;
	}

	if (rule_create->pppoe_en) {
		pgm->ctx.data.rps.pppoe_en = true;
		memcpy(pgm->ctx.data.rps.pppoe_server_mac, rule_create->pppoe_server_mac, ETH_ALEN);
		pgm->ctx.data.rps.pppoe_session_id = rule_create->pppoe_session_id;
	}

	pgm->ctx.data.rps.tun_l3_if = tun_l3_if;
	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_tun_rps_rule_create);
