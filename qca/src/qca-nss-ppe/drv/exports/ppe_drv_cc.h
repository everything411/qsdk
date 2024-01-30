/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * @addtogroup ppe_drv_cc_subsystem
 * @{
 */

#ifndef _PPE_DRV_CC_H_
#define _PPE_DRV_CC_H_

struct ppe_drv;

/*
 * ppe_drv_cc_type
 *	PPE CPU codes
 */
typedef enum ppe_drv_cc_type {
	PPE_DRV_CC_EXP_UNKNOWN_L2_PROT			= 0,	/**< Exception Due To Unknown L2 Prot */
	PPE_DRV_CC_EXP_PPPOE_WRONG_VER_TYPE		= 1,	/**< Exception Due To Pppoe Wrong Ver Type */
	PPE_DRV_CC_EXP_PPPOE_WRONG_CODE			= 2,	/**< Exception Due To Pppoe Wrong Code */
	PPE_DRV_CC_EXP_PPPOE_UNSUPPORTED_PPP_PROT	= 3,	/**< Exception Due To Pppoe Unsupported Ppp Prot */
	PPE_DRV_CC_EXP_IPV4_WRONG_VER			= 4,	/**< Exception Due To Ipv4 Wrong Ver */
	PPE_DRV_CC_EXP_IPV4_SMALL_IHL			= 5,	/**< Exception Due To Ipv4 Small Ihl */
	PPE_DRV_CC_EXP_IPV4_WITH_OPTION			= 6,	/**< Exception Due To Ipv4 With Option */
	PPE_DRV_CC_EXP_IPV4_HDR_INCOMPLETE		= 7,	/**< Exception Due To Ipv4 Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV4_BAD_TOTAL_LEN		= 8,	/**< Exception Due To Ipv4 Bad Total Len */
	PPE_DRV_CC_EXP_IPV4_DATA_INCOMPLETE		= 9,	/**< Exception Due To Ipv4 Data Incomplete */
	PPE_DRV_CC_EXP_IPV4_FRAG			= 10,	/**< Exception Due To Ipv4 Frag */
	PPE_DRV_CC_EXP_IPV4_PING_OF_DEATH		= 11,	/**< Exception Due To Ipv4 Ping Of Death */
	PPE_DRV_CC_EXP_IPV4_SMALL_TTL			= 12,	/**< Exception Due To Ipv4 Small Ttl */
	PPE_DRV_CC_EXP_IPV4_UNK_IP_PROT			= 13,	/**< Exception Due To Ipv4 Unk Ip Prot */
	PPE_DRV_CC_EXP_IPV4_CHECKSUM_ERR		= 14,	/**< Exception Due To Ipv4 Checksum Err */
	PPE_DRV_CC_EXP_IPV4_INV_SIP			= 15,	/**< Exception Due To Ipv4 Inv Sip */
	PPE_DRV_CC_EXP_IPV4_INV_DIP			= 16,	/**< Exception Due To Ipv4 Inv Dip */
	PPE_DRV_CC_EXP_IPV4_LAND_ATTACK			= 17,	/**< Exception Due To Ipv4 Land Attack */
	PPE_DRV_CC_EXP_IPV4_AH_HDR_INCOMPLETE		= 18,	/**< Exception Due To Ipv4 Ah Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV4_AH_HDR_CROSS_BORDER		= 19,	/**< Exception Due To Ipv4 Ah Hdr Cross Border */
	PPE_DRV_CC_EXP_IPV4_ESP_HDR_INCOMPLETE		= 20,	/**< Exception Due To Ipv4 Esp Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV6_WRONG_VER			= 21,	/**< Exception Due To Ipv6 Wrong Ver */
	PPE_DRV_CC_EXP_IPV6_HDR_INCOMPLETE		= 22,	/**< Exception Due To Ipv6 Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV6_BAD_PAYLOAD_LEN		= 23,	/**< Exception Due To Ipv6 Bad Payload Len */
	PPE_DRV_CC_EXP_IPV6_DATA_INCOMPLETE		= 24,	/**< Exception Due To Ipv6 Data Incomplete */
	PPE_DRV_CC_EXP_IPV6_WITH_EXT_HDR		= 25,	/**< Exception Due To Ipv6 With Ext Hdr */
	PPE_DRV_CC_EXP_IPV6_SMALL_HOP_LIMIT		= 26,	/**< Exception Due To Ipv6 Small Hop Limit */
	PPE_DRV_CC_EXP_IPV6_INV_SIP			= 27,	/**< Exception Due To Ipv6 Inv Sip */
	PPE_DRV_CC_EXP_IPV6_INV_DIP			= 28,	/**< Exception Due To Ipv6 Inv Dip */
	PPE_DRV_CC_EXP_IPV6_LAND_ATTACK			= 29,	/**< Exception Due To Ipv6 Land Attack */
	PPE_DRV_CC_EXP_IPV6_FRAG			= 30,	/**< Exception Due To Ipv6 Frag */
	PPE_DRV_CC_EXP_IPV6_PING_OF_DEATH		= 31,	/**< Exception Due To Ipv6 Ping Of Death */
	PPE_DRV_CC_EXP_IPV6_WITH_MORE_EXT_HDR		= 32,	/**< Exception Due To Ipv6 With More Ext Hdr */
	PPE_DRV_CC_EXP_IPV6_UNK_LAST_NEXT_HDR		= 33,	/**< Exception Due To Ipv6 Unk Last Next Hdr */
	PPE_DRV_CC_EXP_IPV6_MOBILITY_HDR_INCOMPLETE	= 34,	/**< Exception Due To Ipv6 Mobility Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV6_MOBILITY_HDR_CROSS_BORDER	= 35,	/**< Exception Due To Ipv6 Mobility Hdr Cross Border */
	PPE_DRV_CC_EXP_IPV6_AH_HDR_INCOMPLETE		= 36,	/**< Exception Due To Ipv6 Ah Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV6_AH_HDR_CROSS_BORDER		= 37,	/**< Exception Due To Ipv6 Ah Hdr Cross Border */
	PPE_DRV_CC_EXP_IPV6_ESP_HDR_INCOMPLETE		= 38,	/**< Exception Due To Ipv6 Esp Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV6_ESP_HDR_CROSS_BORDER	= 39,	/**< Exception Due To Ipv6 Esp Hdr Cross Border */
	PPE_DRV_CC_EXP_IPV6_OTHER_EXT_HDR_INCOMPLETE	= 40,	/**< Exception Due To Ipv6 Other Ext Hdr Incomplete */
	PPE_DRV_CC_EXP_IPV6_OTHER_EXT_HDR_CROSS_BORDER	= 41,	/**< Exception Due To Ipv6 Other Ext Hdr Cross Border */
	PPE_DRV_CC_EXP_TCP_HDR_INCOMPLETE		= 42,	/**< Exception Due To Tcp Hdr Incomplete */
	PPE_DRV_CC_EXP_TCP_HDR_CROSS_BORDER		= 43,	/**< Exception Due To Tcp Hdr Cross Border */
	PPE_DRV_CC_EXP_TCP_SAME_SP_DP			= 44,	/**< Exception Due To Tcp Same Sp Dp */
	PPE_DRV_CC_EXP_TCP_SMALL_DATA_OFFSET		= 45,	/**< Exception Due To Tcp Small Data Offset */
	PPE_DRV_CC_EXP_TCP_FLAGS_0			= 46,	/**< Exception Due To Tcp Flags 0 */
	PPE_DRV_CC_EXP_TCP_FLAGS_1			= 47,	/**< Exception Due To Tcp Flags 1 */
	PPE_DRV_CC_EXP_TCP_FLAGS_2			= 48,	/**< Exception Due To Tcp Flags 2 */
	PPE_DRV_CC_EXP_TCP_FLAGS_3			= 49,	/**< Exception Due To Tcp Flags 3 */
	PPE_DRV_CC_EXP_TCP_FLAGS_4			= 50,	/**< Exception Due To Tcp Flags 4 */
	PPE_DRV_CC_EXP_TCP_FLAGS_5			= 51,	/**< Exception Due To Tcp Flags 5 */
	PPE_DRV_CC_EXP_TCP_FLAGS_6			= 52,	/**< Exception Due To Tcp Flags 6 */
	PPE_DRV_CC_EXP_TCP_FLAGS_7			= 53,	/**< Exception Due To Tcp Flags 7 */
	PPE_DRV_CC_EXP_TCP_CHECKSUM_ERR			= 54,	/**< Exception Due To Tcp Checksum Err */
	PPE_DRV_CC_EXP_UDP_HDR_INCOMPLETE		= 55,	/**< Exception Due To Udp Hdr Incomplete */
	PPE_DRV_CC_EXP_UDP_HDR_CROSS_BORDER		= 56,	/**< Exception Due To Udp Hdr Cross Border */
	PPE_DRV_CC_EXP_UDP_SAME_SP_DP			= 57,	/**< Exception Due To Udp Same Sp Dp */
	PPE_DRV_CC_EXP_UDP_BAD_LEN			= 58,	/**< Exception Due To Udp Bad Len */
	PPE_DRV_CC_EXP_UDP_DATA_INCOMPLETE		= 59,	/**< Exception Due To Udp Data Incomplete */
	PPE_DRV_CC_EXP_UDP_CHECKSUM_ERR			= 60,	/**< Exception Due To Udp Checksum Err */
	PPE_DRV_CC_EXP_UDP_LITE_HDR_INCOMPLETE		= 61,	/**< Exception Due To Udp Lite Hdr Incomplete */
	PPE_DRV_CC_EXP_UDP_LITE_HDR_CROSS_BORDER	= 62,	/**< Exception Due To Udp Lite Hdr Cross Border */
	PPE_DRV_CC_EXP_UDP_LITE_SAME_SP_DP		= 63,	/**< Exception Due To Udp Lite Same Sp Dp */
	PPE_DRV_CC_EXP_UDP_LITE_CSM_COV_1_TO_7		= 64,	/**< Exception Due To Udp Lite Csm Cov 1 To 7 */
	PPE_DRV_CC_EXP_UDP_LITE_CSM_COV_TOO_LONG	= 65,	/**< Exception Due To Udp Lite Csm Cov Too Long */
	PPE_DRV_CC_EXP_UDP_LITE_CSM_COV_CROSS_BORDER	= 66,	/**< Exception Due To Udp Lite Csm Cov Cross Border */
	PPE_DRV_CC_EXP_UDP_LITE_CHECKSUM_ERR		= 67,	/**< Exception Due To Udp Lite Checksum Err */
	PPE_DRV_CC_EXP_FAKE_L2_PROT_ERR			= 68,	/**< Exception Due To Fake L2 Prot Err */
	PPE_DRV_CC_EXP_FAKE_MAC_HEADER_ERR		= 69,	/**< Exception Due To Fake Mac Header Err */
	PPE_DRV_CC_EXP_BITMAP_MAX			= 77,	/**< Exception Due To Bitmap Max */
	PPE_DRV_CC_L2_EXP_MRU_FAIL			= 78,	/**< L2 Exception Due To Mru Fail */
	PPE_DRV_CC_L2_EXP_MTU_FAIL			= 79,	/**< L2 Exception Due To Mtu Fail */
	PPE_DRV_CC_L3_EXP_IP_PREFIX_BC			= 80,	/**< L3 Exception Due To Ip Prefix Bc */
	PPE_DRV_CC_L3_EXP_MTU_FAIL			= 81,	/**< L3 Exception Due To Mtu Fail */
	PPE_DRV_CC_L3_EXP_MRU_FAIL			= 82,	/**< L3 Exception Due To Mru Fail */
	PPE_DRV_CC_L3_EXP_ICMP_RDT			= 83,	/**< L3 Exception Due To Icmp Rdt */
	PPE_DRV_CC_L3_EXP_IP_RT_TTL1_TO_ME		= 84,	/**< L3 Exception Due To Ip Rt Ttl1 To Me */
	PPE_DRV_CC_L3_EXP_IP_RT_TTL_ZERO		= 85,	/**< L3 Exception Due To Ip Rt Ttl Zero */
	PPE_DRV_CC_L3_FLOW_SERVICE_CODE_LOOP		= 86,	/**< L3 Flow Service Code Loop */
	PPE_DRV_CC_L3_FLOW_DE_ACCELEARTE		= 87,	/**< L3 Flow De Accelearte */
	PPE_DRV_CC_L3_EXP_FLOW_SRC_IF_CHK_FAIL		= 88,	/**< L3 Exception Due To Flow Src If Chk Fail */
	PPE_DRV_CC_L3_FLOW_SYNC_TOGGLE_MISMATCH		= 89,	/**< L3 Flow Sync Toggle Mismatch */
	PPE_DRV_CC_L3_EXP_MTU_DF_FAIL			= 90,	/**< L3 Exception Due To Mtu Df Fail */
	PPE_DRV_CC_L3_EXP_PPPOE_MULTICAST		= 91,	/**< L3 Exception Due To Pppoe Multicast */
	PPE_DRV_CC_L3_EXP_FLOW_MTU_FAIL			= 92,	/**< L3 Exception Due To Flow Mtu Fail */
	PPE_DRV_CC_L3_EXP_FLOW_MTU_DF_FAIL		= 93,	/**< L3 Exception Due To Flow Mtu Df Fail */
	PPE_DRV_CC_L3_UDP_CHECKSUM_0_EXP		= 94,	/**< L3 Udp Checksum 0 Exception Due To */
	PPE_DRV_CC_MGMT_OFFSET				= 95,	/**< Mgmt Offset */
	PPE_DRV_CC_MGMT_EAPOL				= 96,	/**< Mgmt Eapol */
	PPE_DRV_CC_MGMT_PPPOE_DIS			= 97,	/**< Mgmt Pppoe Dis */
	PPE_DRV_CC_MGMT_IGMP				= 98,	/**< Mgmt Igmp */
	PPE_DRV_CC_MGMT_ARP_REQ				= 99,	/**< Mgmt Arp Req */
	PPE_DRV_CC_MGMT_ARP_REP				= 100,	/**< Mgmt Arp Rep */
	PPE_DRV_CC_MGMT_DHCPv4				= 101,	/**< Mgmt Dhcpv4 */
	PPE_DRV_CC_MGMT_LINKOAM				= 103,	/**< Mgmt Linkoam */
	PPE_DRV_CC_MGMT_MLD				= 106,	/**< Mgmt Mld */
	PPE_DRV_CC_MGMT_NS				= 107,	/**< Mgmt Ns */
	PPE_DRV_CC_MGMT_NA				= 108,	/**< Mgmt Na */
	PPE_DRV_CC_MGMT_DHCPv6				= 109,	/**< Mgmt Dhcpv6 */
	PPE_DRV_CC_PTP_OFFSET				= 111,	/**< Ptp Offset */
	PPE_DRV_CC_PTP_SYNC				= 112,	/**< Ptp Sync */
	PPE_DRV_CC_PTP_FOLLOW_UP			= 113,	/**< Ptp Follow Up */
	PPE_DRV_CC_PTP_DELAY_REQ			= 114,	/**< Ptp Delay Req */
	PPE_DRV_CC_PTP_DELAY_RESP			= 115,	/**< Ptp Delay Resp */
	PPE_DRV_CC_PTP_PDELAY_REQ			= 116,	/**< Ptp Pdelay Req */
	PPE_DRV_CC_PTP_PDELAY_RESP			= 117,	/**< Ptp Pdelay Resp */
	PPE_DRV_CC_PTP_PDELAY_RESP_FOLLOW_UP		= 118,	/**< Ptp Pdelay Resp Follow Up */
	PPE_DRV_CC_PTP_ANNONCE				= 119,	/**< Ptp Annonce */
	PPE_DRV_CC_PTP_MANAGEMENT			= 120,	/**< Ptp Management */
	PPE_DRV_CC_PTP_SIGNALING			= 121,	/**< Ptp Signaling */
	PPE_DRV_CC_PTP_PKT_RSV_MSG			= 126,	/**< Ptp Pkt Rsv Msg */
	PPE_DRV_CC_IPV4_SG_UNKNOWN			= 135,	/**< Ipv4 Sg Unknown */
	PPE_DRV_CC_IPV6_SG_UNKNOWN			= 136,	/**< Ipv6 Sg Unknown */
	PPE_DRV_CC_ARP_SG_UNKNOWN			= 137,	/**< Arp Sg Unknown */
	PPE_DRV_CC_ND_SG_UNKNOWN			= 138,	/**< Nd Sg Unknown */
	PPE_DRV_CC_IPV4_SG_VIO				= 139,	/**< Ipv4 Sg Vio */
	PPE_DRV_CC_IPV6_SG_VIO				= 140,	/**< Ipv6 Sg Vio */
	PPE_DRV_CC_ARP_SG_VIO				= 141,	/**< Arp Sg Vio */
	PPE_DRV_CC_ND_SG_VIO				= 142,	/**< Nd Sg Vio */
	PPE_DRV_CC_L3_ROUTING_HOST_MISMATCH		= 147,	/**< L3 Routing Host Mismatch */
	PPE_DRV_CC_L3_FLOW_SNAT_ACTION			= 148,	/**< L3 Flow Snat Action */
	PPE_DRV_CC_L3_FLOW_DNAT_ACTION			= 149,	/**< L3 Flow Dnat Action */
	PPE_DRV_CC_L3_FLOW_RT_ACTION			= 150,	/**< L3 Flow Rt Action */
	PPE_DRV_CC_L3_FLOW_BR_ACTION			= 151,	/**< L3 Flow Br Action */
	PPE_DRV_CC_L3_MC_BRIDGE_ACTION			= 152,	/**< L3 Mc Bridge Action */
	PPE_DRV_CC_L3_ROUTE_PREHEAD_RT_ACTION		= 153,	/**< L3 Route Prehead Rt Action */
	PPE_DRV_CC_L3_ROUTE_PREHEAD_SNAPT_ACTION	= 154,	/**< L3 Route Prehead Snapt Action */
	PPE_DRV_CC_L3_ROUTE_PREHEAD_DNAPT_ACTION	= 155,	/**< L3 Route Prehead Dnapt Action */
	PPE_DRV_CC_L3_ROUTE_PREHEAD_SNAT_ACTION		= 156,	/**< L3 Route Prehead Snat Action */
	PPE_DRV_CC_L3_ROUTE_PREHEAD_DNAT_ACTION		= 157,	/**< L3 Route Prehead Dnat Action */
	PPE_DRV_CC_L3_NO_ROUTE_PREHEAD_NAT_ACTION	= 158,	/**< L3 No Route Prehead Nat Action */
	PPE_DRV_CC_L3_NO_ROUTE_PREHEAD_NAT_ERROR	= 159,	/**< L3 No Route Prehead Nat Error */
	PPE_DRV_CC_L3_ROUTE_ACTION			= 160,	/**< L3 Route Action */
	PPE_DRV_CC_L3_NO_ROUTE_ACTION			= 161,	/**< L3 No Route Action */
	PPE_DRV_CC_L3_NO_ROUTE_NH_INVALID_ACTION	= 162,	/**< L3 No Route Nh Invalid Action */
	PPE_DRV_CC_L3_NO_ROUTE_PREHEAD_ACTION		= 163,	/**< L3 No Route Prehead Action */
	PPE_DRV_CC_L3_BRIDGE_ACTION			= 164,	/**< L3 Bridge Action */
	PPE_DRV_CC_L3_FLOW_ACTION			= 165,	/**< L3 Flow Action */
	PPE_DRV_CC_L3_FLOW_MISS_ACTION			= 166,	/**< L3 Flow Miss Action */
	PPE_DRV_CC_L2_NEW_MAC_ADDRESS			= 167,	/**< L2 New Mac Address */
	PPE_DRV_CC_L2_HASH_COLLOSION			= 168,	/**< L2 Hash Collosion */
	PPE_DRV_CC_L2_STATION_MOVE			= 169,	/**< L2 Station Move */
	PPE_DRV_CC_L2_LEARN_LIMIT			= 170,	/**< L2 Learn Limit */
	PPE_DRV_CC_L2_SA_LOOKUP_ACTION			= 171,	/**< L2 Sa Lookup Action */
	PPE_DRV_CC_L2_DA_LOOKUP_ACTION			= 172,	/**< L2 Da Lookup Action */
	PPE_DRV_CC_APP_CTRL_ACTION			= 173,	/**< App Ctrl Action */
	PPE_DRV_CC_IN_VLAN_FILTER_ACTION		= 174,	/**< In Vlan Filter Action */
	PPE_DRV_CC_IN_VLAN_XLT_MISS			= 175,	/**< In Vlan Xlt Miss */
	PPE_DRV_CC_EG_VLAN_FILTER_DROP			= 176,	/**< Eg Vlan Filter Drop */
	PPE_DRV_CC_ACL_PRE_ACTION			= 177,	/**< Acl Pre Action */
	PPE_DRV_CC_ACL_POST_ACTION			= 178,	/**< Acl Post Action */
	PPE_DRV_CC_SERVICE_CODE_ACTION			= 179,	/**< Service Code Action */
	PPE_DRV_CC_L3_ROUTE_PRE_IPO_RT_ACTION		= 180,	/**< L3 Route Pre Ipo Rt Action */
	PPE_DRV_CC_L3_ROUTE_PRE_IPO_SNAPT_ACTION	= 181,	/**< L3 Route Pre Ipo Snapt Action */
	PPE_DRV_CC_L3_ROUTE_PRE_IPO_DNAPT_ACTION	= 182,	/**< L3 Route Pre Ipo Dnapt Action */
	PPE_DRV_CC_L3_ROUTE_PRE_IPO_SNAT_ACTION		= 183,	/**< L3 Route Pre Ipo Snat Action */
	PPE_DRV_CC_L3_ROUTE_PRE_IPO_DNAT_ACTION		= 184,	/**< L3 Route Pre Ipo Dnat Action */
	PPE_DRV_CC_TL_EXP_IF_CHECK_FAIL			= 185,	/**< Tl Exception Due To If Check Fail */
	PPE_DRV_CC_TL_EXP_VLAN_CHECK_FAIL		= 186,	/**< Tl Exception Due To Vlan Check Fail */
	PPE_DRV_CC_TL_EXP_PPPOE_MC_TERM			= 187,	/**< Tl Exception Due To Pppoe Mc Term */
	PPE_DRV_CC_TL_EXP_DE_ACCE			= 188,	/**< Tl Exception Due To De Acce */
	PPE_DRV_CC_TL_UDP_CSUM_ZERO			= 189,	/**< Tl Udp Csum Zero */
	PPE_DRV_CC_TL_TTL_EXCEED			= 190,	/**< Tl Ttl Exceed */
	PPE_DRV_CC_TL_EXP_LPM_IF_CHECK_FAIL		= 191,	/**< Tl Exception Due To Lpm If Check Fail */
	PPE_DRV_CC_TL_EXP_LPM_VLAN_CHECK_FAIL		= 192,	/**< Tl Exception Due To Lpm Vlan Check Fail */
	PPE_DRV_CC_TL_EXP_MAP_SRC_CHECK_FAIL		= 193,	/**< Tl Exception Due To Map Src Check Fail */
	PPE_DRV_CC_TL_EXP_MAP_DST_CHECK_FAIL		= 194,	/**< Tl Exception Due To Map Dst Check Fail */
	PPE_DRV_CC_TL_EXP_MAP_UDP_CSUM_ZERO		= 195,	/**< Tl Exception Due To Map Udp Csum Zero */
	PPE_DRV_CC_TL_EXP_MAP_NON_TCP_UDP		= 196,	/**< Tl Exception Due To Map Non Tcp Udp */
	PPE_DRV_CC_TL_FWD_CMD				= 197,	/**< Tl Fwd Cmd */
	PPE_DRV_CC_L2_PRE_IPO_ACTION			= 209,	/**< L2 Pre Ipo Action */
	PPE_DRV_CC_L2_TUNL_CONTEXT_INVALID		= 210,	/**< L2 Tunl Context Invalid */
	PPE_DRV_CC_EXP_RESERVE0				= 211,	/**< Exception Due To Reserve0 */
	PPE_DRV_CC_EXP_RESERVE1				= 212,	/**< Exception Due To Reserve1 */
	PPE_DRV_CC_EXP_TUNNEL_DECAP_ECN			= 213,	/**< Exception Due To Tunnel Decap Ecn */
	PPE_DRV_CC_EXP_INNER_PACKET_TOO_SHORT		= 214,	/**< Exception Due To Inner Packet Too Short */
	PPE_DRV_CC_EXP_VXLAN_HDR			= 215,	/**< Exception Due To Vxlan Hdr */
	PPE_DRV_CC_EXP_VXLAN_GPE_HDR			= 216,	/**< Exception Due To Vxlan Gpe Hdr */
	PPE_DRV_CC_EXP_GENEVE_HDR			= 217,	/**< Exception Due To Geneve Hdr */
	PPE_DRV_CC_EXP_GRE_HDR				= 218,	/**< Exception Due To Gre Hdr */
	PPE_DRV_CC_EXP_RESERVED				= 219,	/**< Exception Due To Reserved */
	PPE_DRV_CC_EXP_UNKNOWN_INNER_TYPE		= 220,	/**< Exception Due To Unknown Inner Type */
	PPE_DRV_CC_EXP_FLAG_VXLAN			= 221,	/**< Exception Due To Flag Vxlan */
	PPE_DRV_CC_EXP_FLAG_VXLAN_GPE			= 222,	/**< Exception Due To Flag Vxlan Gpe */
	PPE_DRV_CC_EXP_FLAG_GRE				= 223,	/**< Exception Due To Flag Gre */
	PPE_DRV_CC_EXP_FLAG_GENEVE			= 224,	/**< Exception Due To Flag Geneve */
	PPE_DRV_CC_EXP_PROGRAM0				= 225,	/**< Exception Due To Program0 */
	PPE_DRV_CC_EXP_PROGRAM1				= 226,	/**< Exception Due To Program1 */
	PPE_DRV_CC_EXP_PROGRAM2				= 227,	/**< Exception Due To Program2 */
	PPE_DRV_CC_EXP_PROGRAM3				= 228,	/**< Exception Due To Program3 */
	PPE_DRV_CC_EXP_PROGRAM4				= 229,	/**< Exception Due To Program4 */
	PPE_DRV_CC_EXP_PROGRAM5				= 230,	/**< Exception Due To Program5 */
	PPE_DRV_CC_CPU_CODE_EG_MIRROR			= 253,	/**< Cpu Code Eg Mirror */
	PPE_DRV_CC_CPU_CODE_IN_MIRROR			= 254,	/**< Cpu Code In Mirror */
	PPE_DRV_CC_MAX						/**< Max */
} ppe_drv_cc_t;

typedef bool (*ppe_drv_cc_callback_t)(void *app_data, struct sk_buff *skb);

/*
 * ppe_drv_cc_process_skbuff()
 *	Register callback for a specific CPU code
 *
 * @param[IN] cc   CPU code number.
 * @param[IN] skb  Socket buffer with CPU code.
 *
 * @return
 * true if packet is consumed by the API or false if the packet is not consumed.
 */
extern bool ppe_drv_cc_process_skbuff(uint8_t cc, struct sk_buff *skb);

/*
 * ppe_drv_cc_unregister_cb()
 *	Unregister callback for a specific CPU code
 *
 * @param[IN] cc   CPU code number.
 *
 * @return
 * void
 */
extern void ppe_drv_cc_unregister_cb(ppe_drv_cc_t cc);

/*
 * ppe_drv_cc_register_cb()
 *	Register callback for a specific CPU code
 *
 * @param[IN] cc   Service code number.
 * @param[IN] cb   Callback API.
 * @param[IN] app_data   Application data to be passed to callback.
 *
 * @return
 * void
 */
extern void ppe_drv_cc_register_cb(ppe_drv_cc_t cc, ppe_drv_cc_callback_t cb, void *app_data);

/** @} */ /* end_addtogroup ppe_drv_cc_subsystem */

#endif /* _PPE_DRV_CC_H_ */

