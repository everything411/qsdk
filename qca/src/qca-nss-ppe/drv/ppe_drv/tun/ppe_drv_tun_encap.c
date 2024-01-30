/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/ip.h>
#include <linux/udp.h>
#include <net/gre.h>
#include <net/vxlan.h>
#include <linux/if_tunnel.h>
#include <fal_tunnel.h>
#include <ppe_drv/ppe_drv.h>
#include "ppe_drv_tun.h"

/*
 * ppe_drv_tun_encap_dump
 *	Dump contents of EG_TUN_CTRL table instance
 */
static void ppe_drv_tun_encap_dump(struct ppe_drv_tun_encap *ptec)
{
	sw_error_t err;
	fal_tunnel_encap_cfg_t encap_cfg;

	memset((void *)&encap_cfg, 0, sizeof(fal_tunnel_encap_cfg_t));

	err = fal_tunnel_encap_entry_get(PPE_DRV_SWITCH_ID, ptec->tun_idx, &encap_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Failed to dump encap entry at %u", ptec, ptec->tun_idx);
		return;
	}

	ppe_drv_trace("%p: EG_TUN_CTRL_IDX : [%d], TYPE[%d], RULE_ID[%d], RULE_TARGET[%d], DATA_LEN[%d], VLAN_OFFSET[%d], L3_OFFSET[%d]",
			ptec, ptec->tun_idx,
			encap_cfg.encap_type,
			encap_cfg.edit_rule_id,
			encap_cfg.encap_target,
			encap_cfg.tunnel_len,
			encap_cfg.vlan_offset,
			encap_cfg.l3_offset);

	ppe_drv_trace("%p: DF_MODE[%d], PPPOE_EN[%d], IP_VER[%d], DSCP_MODE[%d]",
			ptec,
			encap_cfg.ipv4_df_mode_ext,
			encap_cfg.pppoe_en,
			encap_cfg.ip_ver,
			encap_cfg.dscp_mode);

	ppe_drv_trace("%p: L4_OFFSET[%d], TUNNEL_OFFSET[%d], STAG_FMT[%d], CTAG_FMT[%d], L4_TYPE[%d]",
			ptec,
			encap_cfg.l4_offset,
			encap_cfg.tunnel_offset,
			encap_cfg.svlan_fmt,
			encap_cfg.cvlan_fmt,
			encap_cfg.l4_proto);

	ppe_drv_trace("%p: SPCP_MODE[%d], SDEI_MODE[%d], CPCP_MODE[%d], CDEI_MODE[%d]",
			ptec,
			encap_cfg.spcp_mode,
			encap_cfg.sdei_mode,
			encap_cfg.cpcp_mode,
			encap_cfg.cdei_mode);
}

/*
 * ppe_drv_tun_encap_free
 *	Deconfigure EG_XLAT_TUN_CTRL table and free entry
 */
void ppe_drv_tun_encap_free(struct kref *kref)
{
	sw_error_t err;
	struct ppe_drv_tun_encap *ptec = container_of(kref, struct ppe_drv_tun_encap, ref);

	err = fal_tunnel_encap_entry_del(PPE_DRV_SWITCH_ID, ptec->tun_idx);
	if (err != SW_OK) {
		ppe_drv_warn("%p: encap entry delete failed for tunnel index %u", ptec, ptec->tun_idx);
		return;
	}
	memset(ptec, 0, sizeof(*ptec));
}

/*
 * ppe_drv_tun_encap_deref
 *	Taken reference of tunnel encap instance
 */
bool ppe_drv_tun_encap_deref(struct ppe_drv_tun_encap *ptec)
{
	uint8_t tun_idx = ptec->tun_idx;

	ppe_drv_assert(kref_read(&ptec->ref), "%p: ref count under run for ptec", ptec);

	if (kref_put(&ptec->ref, ppe_drv_tun_encap_free)) {
		/*
		 * Deconfigure EG_TUN_CTRL entry
		 */
		ppe_drv_trace("%p: reference count is 0 at index: %u", ptec, tun_idx);
		return true;
	}

	ppe_drv_trace("%p: tun_idx: %u ref dec:%u", ptec, tun_idx, kref_read(&ptec->ref));
	return false;
}

/*
 * ppe_drv_tun_encap_ref
 *	Taken reference of tunnel encap instance
 */
struct ppe_drv_tun_encap *ppe_drv_tun_encap_ref(struct ppe_drv_tun_encap *ptec)
{
	kref_get(&ptec->ref);

	ppe_drv_assert(kref_read(&ptec->ref), "%p: ref count rollover for ptec at index:%d", ptec, ptec->tun_idx);
	ppe_drv_trace("%p: ptec %u ref inc:%u", ptec, ptec->tun_idx, kref_read(&ptec->ref));
	return ptec;
}

/*
 * ppe_drv_tun_encap_set_rule_id
 *	Set EG EDIT rule id in eg_tun_ctrl instance
 */
void ppe_drv_tun_encap_set_rule_id(struct ppe_drv_tun_encap *ptec, uint8_t rule_id)
{
	ptec->rule_id = rule_id;
}

/*
 * ppe_drv_tun_encap_hdr_set
 *	Configure EG header data given tunnel header
 */
static void ppe_drv_tun_encap_hdr_set(struct ppe_drv_tun_encap *ptec,
					struct ppe_drv_tun_cmn_ctx *th,
					struct ppe_drv_tun_cmn_ctx_l2 *l2_hdr)
{
	/*
	 * Create EG header buffer as seen on wire
	 *	<DMAC><SMAC<VLAN_HDR><TYPE><IP_HDR>{<GRE_HDR>, <UDP_HDR><VxLAN_HDR>}
	 */
	struct vlan_ethhdr *pri_vlan;
	bool l4_offset_valid = false;
	struct vlan_hdr *sec_vlan;
	uint8_t l3_offset = 0;
	uint8_t l4_offset = 0;
	uint8_t tun_len = 0;
	struct ethhdr *eth;
	uint8_t *tun_hdr;

	tun_hdr = (uint8_t *)&ptec->hdr[0];
	memset((void *)tun_hdr, 0, sizeof(ptec->hdr));

	if (l2_hdr->flags & PPE_DRV_TUN_CMN_CTX_L2_SVLAN_VALID) {
		/*
		 * Fill the SVLAN (primary VLAN)
		 */
		pri_vlan = (struct vlan_ethhdr *)tun_hdr;
		memcpy((void *)pri_vlan->h_dest, (void *)&l2_hdr->dmac, ETH_ALEN);
		memcpy((void *)pri_vlan->h_source, (void *)&l2_hdr->smac, ETH_ALEN);
		pri_vlan->h_vlan_proto = htons(l2_hdr->vlan[0].tpid);
		pri_vlan->h_vlan_TCI = htons(l2_hdr->vlan[0].tci);
		pri_vlan->h_vlan_encapsulated_proto = htons(l2_hdr->vlan[1].tpid);
		tun_hdr += sizeof(*pri_vlan);
		tun_len += sizeof(*pri_vlan);

		/*
		 * Fill the CVLAN (Secondary VLAN)
		 */
		sec_vlan = (struct vlan_hdr *)tun_hdr;
		sec_vlan->h_vlan_TCI = htons(l2_hdr->vlan[1].tci);
		sec_vlan->h_vlan_encapsulated_proto = htons(l2_hdr->eth_type);
		tun_hdr += sizeof(*sec_vlan);
		tun_len += sizeof(*sec_vlan);
	} else if (l2_hdr->flags & PPE_DRV_TUN_CMN_CTX_L2_CVLAN_VALID) {
		/*
		 * fill the CVLAN (primary VLAN)
		 */
		pri_vlan = (struct vlan_ethhdr *)tun_hdr;
		memcpy((void *)pri_vlan->h_dest, (void *)&l2_hdr->dmac, ETH_ALEN);
		memcpy((void *)pri_vlan->h_source, (void *)&l2_hdr->smac, ETH_ALEN);
		pri_vlan->h_vlan_proto = htons(l2_hdr->vlan[0].tpid);
		pri_vlan->h_vlan_TCI = htons(l2_hdr->vlan[0].tci);
		pri_vlan->h_vlan_encapsulated_proto = htons(l2_hdr->eth_type);
		tun_hdr += sizeof(*pri_vlan);
		tun_len += sizeof(*pri_vlan);
	} else {
		/*
		 * Copy MAC header
		 */
		eth = (struct ethhdr *)tun_hdr;
		memcpy((void *)eth->h_dest, (void *)&l2_hdr->dmac, ETH_ALEN);
		memcpy((void *)eth->h_source, (void *)&l2_hdr->smac, ETH_ALEN);
		eth->h_proto = htons(l2_hdr->eth_type);
		tun_len += ETH_HLEN;
		tun_hdr += ETH_HLEN;
	}

	if (l2_hdr->flags & PPE_DRV_TUN_CMN_CTX_L2_PPPOE_VALID) {
		memcpy((void *)tun_hdr, (void *)&l2_hdr->pppoe.ph, sizeof(l2_hdr->pppoe.ph));
		tun_hdr += sizeof(l2_hdr->pppoe.ph);
		memcpy((void *)tun_hdr, (void *)&l2_hdr->pppoe.ppp_proto, sizeof(l2_hdr->pppoe.ppp_proto));
		tun_len += PPPOE_SES_HLEN;
		tun_hdr += sizeof(l2_hdr->pppoe.ppp_proto);
	}

	l3_offset = tun_len;

	/*
	 * Copy L3 header
	 */
	if (th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_IPV4) {
		struct iphdr iph;

		memset((void *)&iph, 0, sizeof(iph));
		iph.version = 0x4;
		iph.ihl = 0x5;
		iph.id = 0;
		iph.saddr = th->l3.saddr[0];
		iph.daddr = th->l3.daddr[0];
		iph.protocol = th->l3.proto;

		if (!(th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL)) {
			iph.ttl = th->l3.ttl;
		}

		if (!(th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP)) {
			iph.tos = (th->l3.dscp << 2);
		}


		memcpy((void *)tun_hdr, (void *)&iph, sizeof(iph));
		tun_hdr += sizeof(iph);
		tun_len += sizeof(iph);
		l4_offset = l3_offset + sizeof(iph);
	} else {
		struct ipv6hdr ip6h = {0};
		uint8_t tclass = 0;

		memset((void *)&ip6h, 0, sizeof(ip6h));

		memcpy((void *)&ip6h.saddr, (void *)&th->l3.saddr, sizeof(struct in6_addr));
		memcpy((void *)&ip6h.daddr, (void *)&th->l3.daddr, sizeof(struct in6_addr));
		ip6_flow_hdr(&ip6h, 0, 0);
		ip6h.nexthdr = (th->l3.proto);

		if (!(th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL)) {
			ip6h.hop_limit = th->l3.ttl;
		}

		if (!(th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP)) {
			tclass = th->l3.dscp;
		}

		ip6_flow_hdr(&ip6h, tclass, 0);

		ip6h.payload_len = htons(sizeof(struct ipv6hdr));
		memcpy((void *)tun_hdr, (void *)&ip6h, sizeof(struct ipv6hdr));

		tun_hdr += sizeof(struct ipv6hdr);
		tun_len += sizeof(struct ipv6hdr);
		l4_offset = l3_offset + sizeof(struct ipv6hdr);
	}

	if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
		struct gre_base_hdr greh;
		uint16_t gre_hdr_flags = 0;
		uint32_t gre_key = 0;

		memset((void *)&greh, 0, sizeof(greh));

		if (th->tun.gre.flags & PPE_DRV_TUN_CMN_CTX_GRE_L_KEY) {
			gre_hdr_flags |= GRE_KEY;
			gre_key = th->tun.gre.local_key;

			ppe_drv_trace("%p: GRE Local key: %d", ptec, gre_key);
		}

		greh.protocol = htons(ETH_P_TEB);
		greh.flags = gre_hdr_flags;

		memcpy((void *)tun_hdr, (void *)&greh, sizeof(greh));
		tun_hdr += sizeof(greh);
		tun_len += sizeof(greh);
		if (gre_key) {
			memcpy((void *)tun_hdr, (void *)&gre_key, sizeof(gre_key));
			tun_hdr += sizeof(gre_key);
			tun_len += sizeof(gre_key);
		}
		l4_offset_valid = true;
	} else if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN) {
		struct udphdr udph;
		struct vxlanhdr vxh;
		struct vxlanhdr_gbp vxh_gbp;

		memset(&udph, 0, sizeof(struct udphdr));
		memset(&vxh, 0, sizeof(struct vxlanhdr));
		memset(&vxh_gbp, 0, sizeof(struct vxlanhdr_gbp));

		udph.dest = th->tun.vxlan.dest_port;
		memcpy((void *)tun_hdr, (void *)&udph, sizeof(udph));
		tun_hdr += sizeof(udph);
		tun_len += sizeof(udph);

		vxh.vx_flags = th->tun.vxlan.flags;
		vxh.vx_vni = th->tun.vxlan.vni;

		if (th->tun.vxlan.flags & VXLAN_F_GBP) {
			vxh_gbp.policy_id = th->tun.vxlan.policy_id;
			vxh_gbp.vx_flags = th->tun.vxlan.flags;
			vxh_gbp.vx_vni = th->tun.vxlan.vni;
			vxh_gbp.policy_id = th->tun.vxlan.policy_id;
			memcpy((void *)tun_hdr, (void *)&vxh_gbp, sizeof(vxh_gbp));
			tun_hdr += sizeof(vxh_gbp);
			tun_len += sizeof(vxh_gbp);
		}

		memcpy((void *)tun_hdr, (void *)&vxh, sizeof(vxh));
		tun_hdr += sizeof(vxh);
		tun_len += sizeof(vxh);
		l4_offset_valid = true;
	}


	ptec->tun_len = tun_len;
	ptec->l3_offset = l3_offset;
	ptec->l4_offset = l4_offset;
	ptec->l4_offset_valid = l4_offset_valid;

	ppe_drv_trace("%p: PPE_DRV_EG_HEADER_DATA_TBL Entry tun_len: %d", ptec, tun_len);
}

/*
 * ppe_drv_tun_encap_tun_idx_set
 *	Set tunnel_idx in port
 */
bool ppe_drv_tun_encap_tun_idx_configure(struct ppe_drv_tun_encap *ptec, uint32_t port_num, bool tunnel_id_valid)
{
	sw_error_t err;
	fal_tunnel_id_t encap_tun_id = {0};

	/*
	 * configure encap_tun_idx
	 */
	encap_tun_id.tunnel_id_valid = tunnel_id_valid;
	encap_tun_id.tunnel_id = ptec->tun_idx;

	err = fal_tunnel_encap_port_tunnelid_set(PPE_DRV_SWITCH_ID, port_num, &encap_tun_id);
	if (err != SW_OK) {
		ppe_drv_warn("%p: tunnel encapsulation idx set failed for port %u", ptec, port_num);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_encap_get_len
 *	Get tunnel encap length
 */
uint16_t ppe_drv_tun_encap_get_len(struct ppe_drv_tun_encap *ptec)
{
	return ptec->tun_len;
}

/*
 * ppe_drv_tun_encap_configure
 *	Configure EG_XLAT_TUN_CTRL table
 */
bool ppe_drv_tun_encap_configure(struct ppe_drv_tun_encap *ptec,
					struct ppe_drv_tun_cmn_ctx *th,
					struct ppe_drv_tun_cmn_ctx_l2 *l2_hdr)
{
	sw_error_t err;
	fal_tunnel_encap_cfg_t encap_cfg = {0};
	fal_tunnel_encap_header_ctrl_t header_ctrl = {0};

	/*
	 * Update the tunnel encapsulation header
	 */

	ppe_drv_tun_encap_hdr_set(ptec, th, l2_hdr);

	if (th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_IPV4) {
		encap_cfg.ipv4_df_mode_ext = FAL_TUNNEL_ENCAP_EXT_DF_MODE_FIX; /* Fixed value */
		encap_cfg.ipv4_df_mode = FAL_TUNNEL_ENCAP_DF_MODE_COPY; /* Copy from inner */
		encap_cfg.ipv4_id_mode = 1; /* Random value */
	} else {
		encap_cfg.ip_ver = FAL_TUNNEL_IP_VER_6; /* IPV6 ; 0 is for IPV4 */

		/*
		 * TODO: Should we have configuration for below flags
		 */
		encap_cfg.ipv6_flowlable_mode = FAL_TUNNEL_ENCAP_FLOWLABLE_MODE_COPY; /* Copy from inner */
	}

	if (th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP) {
		encap_cfg.dscp_mode =  FAL_TUNNEL_UNIFORM_MODE; /* uniform dscp mode */
	}

	if (th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL) {
		encap_cfg.ttl_mode = FAL_TUNNEL_UNIFORM_MODE; /* uniform ttl mode */
	}

	/*
	 * TODO: check if DEI and PCP needs to be set to uniform mode
	 */
	if (l2_hdr->flags & PPE_DRV_TUN_CMN_CTX_L2_SVLAN_VALID) {
		encap_cfg.svlan_fmt = 1;
	}

	if (l2_hdr->flags & PPE_DRV_TUN_CMN_CTX_L2_CVLAN_VALID) {
		encap_cfg.cvlan_fmt = 1;
	}

	/*
	 * Set the VLAN offset if SVLAN or CVLAN is enabled
	 */
	if (encap_cfg.svlan_fmt || encap_cfg.cvlan_fmt) {
		encap_cfg.vlan_offset = 12;
	}

	if (ptec->l4_offset) {
		encap_cfg.l4_offset = ptec->l4_offset;
	}

	if (l2_hdr->flags & PPE_DRV_TUN_CMN_CTX_L2_PPPOE_VALID) {
		encap_cfg.pppoe_en = true;
	}

	/*
	 * Set tunnel specific bits
	 */
	if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6) {
		encap_cfg.payload_inner_type = FAL_TUNNEL_INNER_IP;

	/*
	 * PPE currently supports only the default source port range (49152 to 65535)
	 */
	} else if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN) {
		encap_cfg.l4_proto = FAL_TUNNEL_ENCAP_L4_PROTO_UDP; /* 0:Non;1:TCP;2:UDP;3:UDP-Lite;4:Reserved (ICMP);5:GRE; */
		encap_cfg.sport_entry_en = 1;  /* TODO: FAL API should be entropy */
		encap_cfg.payload_inner_type = FAL_TUNNEL_INNER_ETHERNET;
		header_ctrl.udp_sport_base = FAL_TUNNEL_UDP_ENTROPY_SPORT_BASE;
		header_ctrl.udp_sport_mask = FAL_TUNNEL_UDP_ENTROPY_SPORT_MASK;

		if (!(th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM_TX)) {
			encap_cfg.l4_checksum_en = true;
		}

		err = fal_tunnel_encap_header_ctrl_set(0, &header_ctrl);
		if (err != SW_OK) {
			ppe_drv_warn("%p VXLAN: failed to configure encap header err: %d", ptec, err);
			return false;
		}

	} else if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
		encap_cfg.payload_inner_type = FAL_TUNNEL_INNER_ETHERNET;
		encap_cfg.l4_proto = 5; /* 0:Non;1:TCP;2:UDP;3:UDP-Lite;4:Reserved (ICMP);5:GRE; */

	} else if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_MAPT) {
		encap_cfg.ip_proto_update = 1;
		encap_cfg.l4_checksum_en = true;
		encap_cfg.payload_inner_type = FAL_TUNNEL_INNER_TRANSPORT;

	}

	if (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_MAPT) {
		encap_cfg.edit_rule_id = ptec->rule_id;
		encap_cfg.encap_target = FAL_TUNNEL_ENCAP_TARGET_DIP;

	}

	encap_cfg.tunnel_len = ptec->tun_len;
	encap_cfg.l3_offset = ptec->l3_offset;
	if (ptec->l4_offset_valid) {
		encap_cfg.l4_offset = ptec->l4_offset;
	}

	/*
	 * Configure vport information in encap if xmit port is vp
	 */
	if (PPE_DRV_VIRTUAL_PORT_CHK(l2_hdr->xmit_port)) {
		encap_cfg.vport_en = 1;
		encap_cfg.vport = l2_hdr->xmit_port;
	}

	/*
	 * Copy tunnel header information
	 */
	memcpy((uint8_t *)&encap_cfg.pkt_header, (uint8_t *)&ptec->hdr[0], FAL_TUNNEL_ENCAP_HEADER_MAX_LEN);

	/*
	 * Configure encap entry into HW
	 */
	err = fal_tunnel_encap_entry_add(PPE_DRV_SWITCH_ID, ptec->tun_idx, &encap_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p failed to configure encap entry at %u", ptec, ptec->tun_idx);
		return false;
	}

	ppe_drv_tun_encap_dump(ptec);
	return true;
}

/*
 * ppe_drv_tun_encap_alloc
 *	Allocate PPE tunnel and return.
 */
struct ppe_drv_tun_encap *ppe_drv_tun_encap_alloc(struct ppe_drv *p)
{
	uint8_t tun_idx;

	/*
	 * Return first free instance
	 */
	for (tun_idx = 0; tun_idx < PPE_DRV_TUN_ENCAP_ENTRIES; tun_idx++) {
		struct ppe_drv_tun_encap *ptec = &p->ptun_ec[tun_idx];

		if (kref_read(&ptec->ref)) {
			continue;
		}

		ptec->tun_idx = tun_idx;
		kref_init(&ptec->ref);
		ppe_drv_trace("%p: Free tunnel encap instance found, index: %d", ptec, tun_idx);
		return ptec;
	}

	ppe_drv_warn("%p: Free tunnel encap instance is not found", p);
	return NULL;
}

/*
 * ppe_drv_tun_encap_entries_free
 *	Free memory allocated for tunnel encap instances.
 */
void ppe_drv_tun_encap_entries_free(struct ppe_drv_tun_encap *ptun_ec)
{
	vfree(ptun_ec);
}

/*
 * ppe_drv_tun_encap_entries_alloc
 *	Allocate and initialize tunnel encap table entries.
 */
struct ppe_drv_tun_encap *ppe_drv_tun_encap_entries_alloc(struct ppe_drv *p)
{
	uint16_t index;
	struct ppe_drv_tun_encap *ptun_ec;

	ptun_ec = vzalloc(sizeof(struct ppe_drv_tun_encap) * PPE_DRV_TUN_ENCAP_ENTRIES);
	if (!ptun_ec) {
		ppe_drv_warn("%p: failed to allocate ptun encap entries", p);
		return NULL;
	}

	for (index = 0; index < PPE_DRV_TUN_ENCAP_ENTRIES; index++) {
		ptun_ec[index].tun_idx = index;
	}

	return ptun_ec;
}
