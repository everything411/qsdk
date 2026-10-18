/*
 * Copyright (c) 2017, 2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "adpt_hppe.h"
#include "adpt.h"
#if defined(HTTPPE)
#include "adpt_httppe_misc.h"
#include "adpt_httppe_portctrl.h"
#endif

/* Reordered cpucode array - direct mapping: cpucode[cpu_code_value] */
/* Array size: 256 (0-255) to cover all CPU codes */
char *cpucode[] = {
	/* 0-70: Direct mapping */
	[0] = "Forwarding to CPU",
	[1] = "Unkown L2 protocol exception redirect/copy to CPU",
	[2] = "PPPoE wrong version or wrong type exception redirect/copy to CPU",
	[3] = "PPPoE wrong code exception redirect/copy to CPU",
	[4] = "PPPoE unsupported PPP protocol exception redirect/copy to CPU",
	[5] = "IPv4 wrong version exception redirect/copy to CPU",
	[6] = "IPv4 small IHL exception redirect/copy to CPU",
	[7] = "IPv4 with option exception redirect/copy to CPU",
	[8] = "IPv4 header incomplete exception redirect/copy to CPU",
	[9] = "IPv4 bad total length exception redirect/copy to CPU",
	[10] = "IPv4 data incomplete exception redirect/copy to CPU",
	[11] = "IPv4 fragment exception redirect/copy to CPU",
	[12] = "IPv4 ping of death exception redirect/copy to CPU",
	[13] = "IPv4 small TTL exception redirect/copy to CPU",
	[14] = "IPv4 unknown IP protocol exception redirect/copy to CPU",
	[15] = "IPv4 checksum error exception redirect/copy to CPU",
	[16] = "IPv4 invalid SIP exception redirect/copy to CPU",
	[17] = "IPv4 invalid DIP exception redirect/copy to CPU",
	[18] = "IPv4 LAND attack exception redirect/copy to CPU",
	[19] = "IPv4 AH header incomplete exception redirect/copy to CPU",
	[20] = "IPv4 AH header cross 128-byte exception redirect/copy to CPU",
	[21] = "IPv4 ESP header incomplete exception redirect/copy to CPU",
	[22] = "IPv6 wrong version exception redirect/copy to CPU",
	[23] = "IPv6 header incomplete exception redirect/copy to CPU",
	[24] = "IPv6 bad total length exception redirect/copy to CPU",
	[25] = "IPv6 data incomplete exception redirect/copy to CPU",
	[26] = "IPv6 with extension header exception redirect/copy to CPU",
	[27] = "IPv6 small hop limit exception redirect/copy to CPU",
	[28] = "IPv6 invalid SIP exception redirect/copy to CPU",
	[29] = "IPv6 invalid DIP exception redirect/copy to CPU",
	[30] = "IPv6 LAND attack exception redirect/copy to CPU",
	[31] = "IPv6 fragment exception redirect/copy to CPU",
	[32] = "IPv6 ping of death exception redirect/copy to CPU",
	[33] = "IPv6 with more than 2 extension headers exception redirect/copy to CPU",
	[34] = "IPv6 unknown last next header exception redirect/copy to CPU",
	[35] = "IPv6 mobility header incomplete exception redirect/copy to CPU",
	[36] = "IPv6 mobility header cross 128-byte exception redirect/copy to CPU",
	[37] = "IPv6 AH header incomplete exception redirect/copy to CPU",
	[38] = "IPv6 AH header cross 128-byte exception redirect/copy to CPU",
	[39] = "IPv6 ESP header incomplete exception redirect/copy to CPU",
	[40] = "IPv6 ESP header cross 128-byte exception redirect/copy to CPU",
	[41] = "IPv6 other extension header incomplete exception redirect/copy to CPU",
	[42] = "IPv6 other extension header cross 128-byte exception redirect/copy to CPU",
	[43] = "TCP header incomplete exception redirect/copy to CPU",
	[44] = "TCP header cross 128-byte exception redirect/copy to CPU",
	[45] = "TCP same SP and DP exception redirect/copy to CPU",
	[46] = "TCP small data offset redirect/copy to CPU",
	[47] = "TCP flags VALUE/MASK group 0 exception redirect/copy to CPU",
	[48] = "TCP flags VALUE/MASK group 1 exception redirect/copy to CPU",
	[49] = "TCP flags VALUE/MASK group 2 exception redirect/copy to CPU",
	[50] = "TCP flags VALUE/MASK group 3 exception redirect/copy to CPU",
	[51] = "TCP flags VALUE/MASK group 4 exception redirect/copy to CPU",
	[52] = "TCP flags VALUE/MASK group 5 exception redirect/copy to CPU",
	[53] = "TCP flags VALUE/MASK group 6 exception redirect/copy to CPU",
	[54] = "TCP flags VALUE/MASK group 7 exception redirect/copy to CPU",
	[55] = "TCP checksum error exception redirect/copy to CPU",
	[56] = "UDP header incomplete exception redirect/copy to CPU",
	[57] = "UDP header cross 128-byte exception redirect/copy to CPU",
	[58] = "UDP same SP and DP exception redirect/copy to CPU",
	[59] = "UDP bad length exception redirect/copy to CPU",
	[60] = "UDP data incomplete exception redirect/copy to CPU",
	[61] = "UDP checksum error exception redirect/copy to CPU",
	[62] = "UDP-Lite header incomplete exception redirect/copy to CPU",
	[63] = "UDP-Lite header cross 128-byte exception redirect/copy to CPU",
	[64] = "UDP-Lite same SP and DP exception redirect/copy to CPU",
	[65] = "UDP-Lite checksum coverage value 0-7 exception redirect/copy to CPU",
	[66] = "UDP-Lite checksum coverage value too big exception redirect/copy to CPU",
	[67] = "UDP-Lite checksum coverage value cross 128-byte exception redirect/copy to CPU",
	[68] = "UDP-Lite checksum error exception redirect/copy to CPU",
	[69] = "Fake L2 protocol packet redirect/copy to CPU",
	[70] = "Fake MAC header packet redirect/copy to CPU",

	/* 71-78: Reserved */
	[71] = "Reserved",
	[72] = "Reserved",
	[73] = "Reserved",
	[74] = "Reserved",
	[75] = "Reserved",
	[76] = "Reserved",
	[77] = "Reserved",
	[78] = "Reserved",

	/* 79-92: L2/L3 exceptions */
	[79] = "L2 MRU checking fail redirect/copy to CPU",
	[80] = "L2 MTU checking fail redirect/copy to CPU",
	[81] = "IP prefix broadcast redirect/copy to CPU",
	[82] = "L3 MTU checking fail redirect/copy to CPU",
	[83] = "L3 MRU checking fail redirect/copy to CPU",
	[84] = "ICMP redirect/copy to CPU",
	[85] = "IP to me routing TTL 1 redirect/copy to CPU",
	[86] = "IP to me routing TTL 0 redirect/copy to CPU",
	[87] = "Flow service code loop redirect/copy to CPU",
	[88] = "Flow de-accelearate redirect/copy to CPU",
	[89] = "Flow source interface check fail redirect/copy to CPU",
	[90] = "Flow sync toggle mismatch redirect/copy to CPU",
	[91] = "MTU check fail if DF set redirect/copy to CPU",
	[92] = "PPPoE multicast redirect/copy to CPU",

	/* 93-95: Flow MTU and UDP checksum */
	[93] = "L3 FLOW MTU CHECK FAIL",
	[94] = "L3 FLOW MTU CHECK DF FAIL",
	[95] = "L3 UDP CHECKSUM EXP",

	/* 96: Reserved */
	[96] = "Reserved",

	/* 97-102: Management packets */
	[97] = "EAPoL packet redirect/copy to CPU",
	[98] = "PPPoE discovery packet redirect/copy to CPU",
	[99] = "IGMP packet redirect/copy to CPU",
	[100] = "ARP request packet redirect/copy to CPU",
	[101] = "ARP reply packet redirect/copy to CPU",
	[102] = "DHCPv4 packet redirect/copy to CPU",

	/* 103-106: Reserved */
	[103] = "Reserved",
	[104] = "8023ah OAM packet redirect/copy to CPU",
	[105] = "Reserved",
	[106] = "Reserved",

	/* 107-110: IPv6 management */
	[107] = "MLD packet redirect/copy to CPU",
	[108] = "NS packet redirect/copy to CPU",
	[109] = "NA packet redirect/copy to CPU",
	[110] = "DHCPv6 packet redirect/copy to CPU",

	/* 111-112: Reserved */
	[111] = "Reserved",
	[112] = "Reserved",

	/* 113-127: PTP packets */
	[113] = "PTP sync packet redirect/copy to CPU",
	[114] = "PTP follow up packet redirect/copy to CPU",
	[115] = "PTP delay request packet redirect/copy to CPU",
	[116] = "PTP delay response packet redirect/copy to CPU",
	[117] = "PTP pdelay request packet redirect/copy to CPU",
	[118] = "PTP pdelay response packet redirect/copy to CPU",
	[119] = "PTP pdelay response follow up packet redirect/copy to CPU",
	[120] = "PTP announce packet redirect/copy to CPU",
	[121] = "PTP management packet redirect/copy to CPU",
	[122] = "PTP signaling packet redirect/copy to CPU",
	[123] = "PTP message reserved type 0 packet redirect/copy to CPU",
	[124] = "PTP message reserved type 1 packet redirect/copy to CPU",
	[125] = "PTP message reserved type 2 packet redirect/copy to CPU",
	[126] = "PTP message reserved type 3 packet redirect/copy to CPU",
	[127] = "PTP message reserved type packet redirect/copy to CPU",

	/* 128-135: Reserved */
	[128] = "Reserved",
	[129] = "Reserved",
	[130] = "Reserved",
	[131] = "Reserved",
	[132] = "Reserved",
	[133] = "Reserved",
	[134] = "Reserved",
	[135] = "Reserved",

	/* 136-143: Source guard */
	[136] = "IPv4 source guard unknown packet redirect/copy to CPU",
	[137] = "IPv6 source guard unknown packet redirect/copy to CPU",
	[138] = "ARP source guard unknown packet redirect/copy to CPU",
	[139] = "ND source guard unknown packet redirect/copy to CPU",
	[140] = "IPv4 source guard violation packet redirect/copy to CPU",
	[141] = "IPv6 source guard violation packet redirect/copy to CPU",
	[142] = "ARP source guard violation packet redirect/copy to CPU",
	[143] = "ND source guard violation packet redirect/copy to CPU",

	/* 144: Reserved */
	[144] = "Reserved",

	/* 145: dot1p mapper */
	[145] = "dot1p mapper action to CPU",

	/* 146-147: Reserved */
	[146] = "Reserved",
	[147] = "Reserved",

	/* 148-174: L3 routing and L2 actions */
	[148] = "L3 route host mismatch action redirect/copy to CPU",
	[149] = "L3 flow SNAT action redirect/copy to CPU",
	[150] = "L3 flow DNAT action redirect/copy to CPU",
	[151] = "L3 flow routing action redirect/copy to CPU",
	[152] = "L3 flow bridging action redirect/copy to CPU",
	[153] = "L3 multicast bridging action redirect/copy to CPU",
	[154] = "L3 route Preheader routing action redirect/copy to CPU",
	[155] = "L3 route Preheader SNAPT action redirect/copy to CPU",
	[156] = "L3 route Preheader DNAPT action redirect/copy to CPU",
	[157] = "L3 route Preheader SNAT action redirect/copy to CPU",
	[158] = "L3 route Preheader DNAT action redirect/copy to CPU",
	[159] = "L3 no route preheader NAT action redirect/copy to CPU",
	[160] = "L3 no route preheader NAT error redirect/copy to CPU",
	[161] = "L3 route action redirect/copy to CPU",
	[162] = "L3 no route action redirect/copy to CPU",
	[163] = "L3 no route next hop invalid action redirect/copy to CPU",
	[164] = "L3 no route preheader action redirect/copy to CPU",
	[165] = "L3 bridge action redirect/copy to CPU",
	[166] = "L3 flow action redirect/copy to CPU",
	[167] = "L3 flow miss action redirect/copy to CPU",
	[168] = "L2 new MAC address redirect/copy to CPU",
	[169] = "L2 hash violation redirect/copy to CPU",
	[170] = "L2 station move redirect/copy to CPU",
	[171] = "L2 learn limit redirect/copy to CPU",
	[172] = "L2 SA lookup action redirect/copy to CPU",
	[173] = "L2 DA lookup action redirect/copy to CPU",
	[174] = "APP_CTRL action redirect/copy to CPU",

	/* 175-177: Reserved */
	[175] = "Reserved",
	[176] = "Reserved",
	[177] = "Reserved",

	/* 178-180: ACL and Service */
	[178] = "Pre-IPO action",
	[179] = "Post-IPO action",
	[180] = "Service code action",

	/* 181-185: L3 route pre-IPO actions */
	[181] = "L3 ROUTE PRE IPO ROUTE ACTION",
	[182] = "L3 ROUTE PRE IPO SNAPT ACTION",
	[183] = "L3 ROUTE PRE IPO DNAPT ACTION",
	[184] = "L3 ROUTE PRE IPO SNAT ACTION",
	[185] = "L3_ROUTE PRE IPO DNAT ACTION",

	/* 186-198: Tunnel exceptions */
	[186] = "TUNNEL interface check fail",
	[187] = "TUNNEL vlan check fail",
	[188] = "TUNNEL PPPOE multicast term",
	[189] = "TUNNEL de-accelate",
	[190] = "TUNNEL UDP checksum zero",
	[191] = "TUNNEL TTL exceed",
	[192] = "TUNNEL LPM interface check fail",
	[193] = "TUNNEL LPM vlan check fail",
	[194] = "TUNNEL MAP source check fail",
	[195] = "TUNNEL MAP destination check fail",
	[196] = "TUNNEL MAP UDP checksum zero",
	[197] = "TUNNEL MAP non TCP and UDP",
	[198] = "TUNNEL forward command",

	/* 199-209: Reserved */
	[199] = "Reserved",
	[200] = "Reserved",
	[201] = "Reserved",
	[202] = "Reserved",
	[203] = "Reserved",
	[204] = "Reserved",
	[205] = "Reserved",
	[206] = "Reserved",
	[207] = "Reserved",
	[208] = "Reserved",
	[209] = "Reserved",

	/* 210-232: L2 pre-ACL and tunnel decap */
	[210] = "L2 PRE-ACL action",
	[211] = "TUNNEL L2 context invalid",
	[212] = "TUNNEL decap inner ipv4 padding",
	[213] = "TUNNEL decap inner ipv6 padding",
	[214] = "TUNNEL decap ECN",
	[215] = "TUNNEL inner packet too short",
	[216] = "TUNNEL VXLAN header",
	[217] = "TUNNEL VXLAN GPE header",
	[218] = "TUNNEL GENEVE header",
	[219] = "TUNNEL GRE header",
	[220] = "TUNNEL GRE checksum error",
	[221] = "TUNNEL unknow inner type",
	[222] = "TUNNEL VXLAN flag",
	[223] = "TUNNEL VXLAN GPE flag",
	[224] = "TUNNEL GRE flag",
	[225] = "TUNNEL GENEVE flag",
	[226] = "TUNNEL PROGRAM0",
	[227] = "TUNNEL PROGRAM1",
	[228] = "TUNNEL PROGRAM2",
	[229] = "TUNNEL PROGRAM3",
	[230] = "TUNNEL PROGRAM4",
	[231] = "TUNNEL PROGRAM5",
	[232] = "Bypass l2 flooding and redirect to CPU",

	/* 233-252: Reserved */
	[233] = "Reserved",
	[234] = "Reserved",
	[235] = "Reserved",
	[236] = "Reserved",
	[237] = "Reserved",
	[238] = "Reserved",
	[239] = "Reserved",
	[240] = "Reserved",
	[241] = "Reserved",
	[242] = "Reserved",
	[243] = "Reserved",
	[244] = "Reserved",
	[245] = "Reserved",
	[246] = "Reserved",
	[247] = "Reserved",
	[248] = "Reserved",
	[249] = "Reserved",
	[250] = "Reserved",
	[251] = "Reserved",
	[252] = "Reserved",

	/* 253-255: Isolation and mirror */
	[253] = "Isolation action to CPU",
	[254] = "Egress mirror to CPU",
	[255] = "Ingress mirror to CPU",
};

char *dropcode[] = {
"None",
"Unkown L2 protocol exception drop",
"PPPoE wrong version or wrong header exception drop",
"PPPoE unsupported PPP protocol exception drop",
"IPv4 wrong version exception drop",
"IPv4 wrong format exception drop",
"IPv4 with option exception drop",
"IPv4 bad total length exception drop",
"IPv4 fragment exception drop",
"IPv4 ping of death exception drop",
"IPv4 small TTL exception drop",
"IPv4 unknown IP protocol exception drop",
"IPv4 checksum error exception drop",
"IPv4 invalid IP exception drop",
"IPv4 LAND attack exception drop",
"IPv4 other exception drop",
"IPv6 wrong version exception drop",
"IPv6 wrong format exception drop",
"IPv6 bad payload length exception drop",
"IPv6 with extension header exception drop",
"IPv6 small hop limit exception drop",
"IPv6 invalid IP exception drop",
"IPv6 LAND attack exception drop",
"IPv6 fragment exception drop",
"IPv6 ping of death exception drop",
"IPv6 with more than 2 extension headers exception drop",
"IPv6 other exception drop",
"TCP format error exception drop",
"TCP same SP and DP exception drop",
"TCP flags exception drop",
"TCP checksum error exception drop",
"UDP format error exception drop",
"UDP same SP and DP exception drop",
"UDP bad length exception drop",
"UDP checksum error exception drop",
"UDP-Lite format error exception drop",
"UDP-Lite checksum error exception drop",
"L3 route PRE-IPO RT drop",
"L3 route PRE-IPO SNAPT drop",
"L3 route PRE-IPO DNAPT drop",
"L3 route PRE-IPO SNAT drop",
"L3 route PRE-IPO DNAT drop",
"TUNNEL interface check fail exception drop",
"TUNNEL vlan check fail exception drop",
"TUNNEL PPPOE multicast term exception drop",
"TUNNEL de-accelerate exception drop",
"TUNNEL UDP checksum zero drop",
"TUNNEL TTL exceed exception drop",
"TUNNEL LPM interface check fail drop",
"TUNNEL LPM VLAN check fail drop",
"TUNNEL MAP source check fail drop",
"TUNNEL MAP destination check fail drop",
"TUNNEL MAP UDP checksum zero drop",
"TUNNEL MAP non TCP UDP drop",
"Pre-ACL entry hit action drop",
"TUNNEL L2 invalid context exception drop",
"TUNNEL decap inner ipv4 padding exception drop",
"TUNNEL decap inner ipv6 padding exception drop",
"TUNNEL decap ECN drop",
"TUNNEL inner packet too short drop",
"TUNNEL VXLAN header exception drop",
"TUNNEL VXLAN GPE header exception drop",
"TUNNEL GENEVE header exception drop",
"TUNNEL GRE header exception drop",
"TUNNEL GRE checksum error exception drop",
"TUNNEL unknown inner type exception drop",
"TUNNEL flag exception drop",
"TUNNEL PROGRAM exception drop",
"TUNNEL forward command exception drop",
"L3 bridge action drop",
"L3 no route with Preheader NAT action",
"L3 no route with Preheader NAT action error configuration",
"L3 route action drop",
"L3 no route action drop",
"L3 no route next hop invalid action drop",
"L3 no route preheader action drop",
"L3 bridge action drop",
"L3 flow action drop",
"L3 flow miss action drop",
"L2 MRU checking fail drop",
"L2 MTU checking fail drop",
"L3 IP prefix broadcast drop",
"L3 MTU checking fail drop",
"L3 MRU checking fail drop",
"L3 ICMP redirect drop",
"Fake MAC header indicated packet not routing or bypass L3 edit drop",
"L3 IP route TTL zero drop",
"L3 flow service code loop drop",
"L3 flow de-accelerate drop",
"L3 flow source interface check fail drop",
"Flow toggle mismatch exception drop",
"MTU check exception if DF set drop",
"PPPoE multicast packet with IP routing enabled drop",
"IPv4 SG unkown drop",
"IPv6 SG unkown drop",
"ARP SG unkown drop",
"ND SG unkown drop",
"IPv4 SG violation drop",
"IPv6 SG violation drop",
"ARP SG violation drop",
"ND SG violation drop",
"L2 new MAC address drop",
"L2 hash violation drop",
"L2 station move drop",
"L2 learn limit drop",
"L2 SA lookup action drop",
"L2 DA lookup action drop",
"APP_CTRL action drop",
"Ingress VLAN filtering action drop",
"Ingress VLAN translation miss drop",
"Egress VLAN filtering drop",
"ACL-pre entry hit action drop",
"ACL-post entry hit action drop",
"Multicast SA or broadcast SA drop",
"No destination drop",
"STG ingress filtering drop",
"STG egress filtering drop",
"Source port filter drop",
"Trunk select fail drop",
"TX MAC disable drop",
"Ingress VLAN tag format drop",
"CRC error drop",
"PAUSE frame drop",
"Promisc drop",
"Isolation drop",
"Magagement packet APP_CTRL drop",
#if defined(JHPPE)
"Dot1p miss action drop",
#else
"Fake L2 protocol indicated packet not routing or bypass L3 edit drop",
#endif
"Policing drop",
};

static sw_error_t
adpt_hppe_debug_counter_set(a_uint32_t dev_id)
{
	union vlan_cnt_tbl_u vlan_cnt_tbl = {0};
	union pre_l2_cnt_tbl_u pre_l2_cnt_tbl = {0};
	union port_tx_drop_cnt_tbl_u port_tx_drop_cnt_tbl = {0};
	union eg_vsi_counter_tbl_u eg_vsi_counter_tbl = {0};
	union port_tx_counter_tbl_reg_u port_tx_counter_tbl = {0};
	union vp_tx_counter_tbl_reg_u vp_tx_counter_tbl = {0};
	union queue_tx_counter_tbl_u queue_tx_counter_tbl = {0};
	union vp_tx_drop_cnt_tbl_u vp_tx_drop_cnt_tbl = {0};
	union drop_cpu_cnt_tbl_u drop_cpu_cnt_tbl = {0};
	union port_rx_cnt_tbl_u port_rx_cnt_tbl = {0};
	union phy_port_rx_cnt_tbl_u phy_port_rx_cnt_tbl = {0};
	a_uint32_t i;

	/* clear PRX DROP_CNT */
	for (i = 0; i < PPE_BM_PHY_PORT_OFFSET; i++)
		hppe_drop_cnt_drop_cnt_set(dev_id, i, 0);

	/* clear PRX DROP_PKT_STAT */
	for (i = 0; i < DROP_STAT_NUM; i++) {
		hppe_drop_stat_pkts_set(dev_id, i, 0);
		hppe_drop_stat_bytes_set(dev_id, i, 0);
	}

	/* clear IPR_PKT_NUM */
	for (i = 0; i < IPR_PKT_NUM_TBL_REG_MAX_ENTRY; i++) {
		hppe_ipr_pkt_num_tbl_reg_packets_set(dev_id, i, 0);
		hppe_ipr_byte_low_reg_reg_bytes_set(dev_id, i, 0);
		hppe_ipr_byte_high_reg_bytes_set(dev_id, i, 0);
	}

	/* clear VLAN_CNT_TBL */
	for (i = 0; i < VLAN_CNT_TBL_MAX_ENTRY; i++)
		hppe_vlan_cnt_tbl_set(dev_id, i, &vlan_cnt_tbl);

	/* clear PRE_L2_CNT_TBL */
	for (i = 0; i < PRE_L2_CNT_TBL_MAX_ENTRY; i++)
		hppe_pre_l2_cnt_tbl_set(dev_id, i, &pre_l2_cnt_tbl);

	/* clear PORT_TX_DROP_CNT_TBL */
	for (i = 0; i < PORT_TX_DROP_CNT_TBL_MAX_ENTRY; i++)
		hppe_port_tx_drop_cnt_tbl_set(dev_id, i, &port_tx_drop_cnt_tbl);
	if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
		adpt_httppe_eg_vsi_counter_tbl_set(dev_id);
		adpt_httppe_port_tx_counter_tbl_set(dev_id);
		adpt_httppe_vp_tx_counter_tbl_set(dev_id);
		adpt_httppe_queue_tx_counter_tbl_set(dev_id);
#endif
	} else {
		/* clear EG_VSI_COUNTER_TBL */
		for (i = 0; i < EG_VSI_COUNTER_TBL_MAX_ENTRY; i++)
			hppe_eg_vsi_counter_tbl_set(dev_id, i, &eg_vsi_counter_tbl);

		/* clear PORT_TX_COUNTER_TBL */
		for (i = 0; i < PORT_TX_COUNTER_TBL_REG_MAX_ENTRY; i++)
			hppe_port_tx_counter_tbl_reg_set(dev_id, i, &port_tx_counter_tbl);

		/* clear VP_TX_COUNTER_TBL */
		for (i = 0; i < VP_TX_COUNTER_TBL_REG_MAX_ENTRY; i++)
			hppe_vp_tx_counter_tbl_reg_set(dev_id, i, &vp_tx_counter_tbl);

		/* clear QUEUE_TX_COUNTER_TBL */
		for (i = 0; i < QUEUE_TX_COUNTER_TBL_MAX_ENTRY; i++)
			hppe_queue_tx_counter_tbl_set(dev_id, i, &queue_tx_counter_tbl);
	}

	/* clear VP_TX_DROP_CNT_TBL */
	for (i = 0; i < VP_TX_DROP_CNT_TBL_MAX_ENTRY; i++)
		hppe_vp_tx_drop_cnt_tbl_set(dev_id, i, &vp_tx_drop_cnt_tbl);

	if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
		adpt_httppe_debug_drop_cpu_counter_set(dev_id);
#endif
	} else {
		/* clear DROP_CPU_CNT_TBL */
		for (i = 0; i < DROP_CPU_CNT_TBL_MAX_ENTRY; i++)
			hppe_drop_cpu_cnt_tbl_set(dev_id, i, &drop_cpu_cnt_tbl);
	}

	/* clear VP_RX_COUNTER_TBL and VP_RX_DROP_CNT_TBL */
	for (i = 0; i < PORT_RX_CNT_TBL_NUM; i++)
		appe_port_rx_cnt_tbl_set (dev_id, i, &port_rx_cnt_tbl);
	/* clear PORT_RX_COUNTER_TBL and PORT_RX_DROP_CNT_TBL */
	for (i = 0; i < PHY_PORT_RX_CNT_TBL_NUM; i++)
		appe_phy_port_rx_cnt_tbl_set (dev_id, i, &phy_port_rx_cnt_tbl);

	return SW_OK;
}

static void
adpt_hppe_debug_prx_drop_cnt_get(a_uint32_t dev_id, char **buf, ssize_t *count)
{
	a_uint32_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PRX_DROP_CNT RX:");
	for (i = 0; i < DROP_CNT_NUM; i++)
	{
		hppe_drop_cnt_drop_cnt_get(dev_id, i, &value);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15u(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_prx_drop_pkt_stat_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	a_uint32_t value32;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PRX_DROP_PKT_STAT OVERFLOW DROP:");
	for (i = 0; i < DROP_STAT_NUM / 2; i++)
	{
		if (show_type == A_FALSE)
		{
			hppe_drop_stat_pkts_get(dev_id, i, &value32);
			value = (a_uint64_t)value32;
		}
		else
			hppe_drop_stat_bytes_get(dev_id, i, &value);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PRX_DROP_PKT_STAT FC DROP:");
	for (i = DROP_STAT_NUM / 2; i < DROP_STAT_NUM; i++)
	{
		if (show_type == A_FALSE)
		{
			hppe_drop_stat_pkts_get(dev_id, i, &value32);
			value = (a_uint64_t)value32;
		}
		else
			hppe_drop_stat_bytes_get(dev_id, i, &value);

		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i - (DROP_STAT_NUM / 2));
			if (++tags % 3 == 0)
				sign = 1;
		}
	}

	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_ipx_pkt_num_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union ipr_pkt_num_tbl_reg_u ipr_pkt_num_tbl_reg;
	union ipr_byte_low_reg_reg_u ipr_byte_low_reg;
	union ipr_byte_high_reg_u ipr_byte_high_reg;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "IPR_PKT_NUM RX:");
	for (i = 0; i < IPR_PKT_NUM_TBL_REG_MAX_ENTRY; i++)
	{
		hppe_ipr_pkt_num_tbl_reg_get(dev_id, i, &ipr_pkt_num_tbl_reg);
		hppe_ipr_byte_low_reg_reg_get(dev_id, i, &ipr_byte_low_reg);
		hppe_ipr_byte_high_reg_get(dev_id, i, &ipr_byte_high_reg);
		if (show_type == A_FALSE)
	#ifdef JHPPE
			value = (a_uint64_t)ipr_pkt_num_tbl_reg.bf.ipr_pkt_num_tbl_reg;
		else
			value = ipr_byte_low_reg.bf.ipr_byte_low_reg_reg |\
				((a_uint64_t)ipr_byte_high_reg.bf.ipr_byte_high_reg << 32);
	#else
			value = (a_uint64_t)ipr_pkt_num_tbl_reg.bf.packets;
		else
			value = ipr_byte_low_reg.bf.bytes | ((a_uint64_t)ipr_byte_high_reg.bf.bytes << 32);
	#endif
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_vlan_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union vlan_cnt_tbl_u vlan_cnt_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "VLAN_CNT_TBL RX:");
	for (i = 0; i < VLAN_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_vlan_cnt_tbl_get(dev_id, i, &vlan_cnt_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)vlan_cnt_tbl.bf.rx_pkt_cnt;
		else
			value = vlan_cnt_tbl.bf.rx_byte_cnt_0 | ((a_uint64_t)vlan_cnt_tbl.bf.rx_byte_cnt_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(vsi=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_pre_l2_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union pre_l2_cnt_tbl_u pre_l2_cnt_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PRE_L2_CNT_TBL RX:");
	for (i = 0; i < PRE_L2_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_pre_l2_cnt_tbl_get(dev_id, i, &pre_l2_cnt_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)pre_l2_cnt_tbl.bf.rx_pkt_cnt;
		else
			value = pre_l2_cnt_tbl.bf.rx_byte_cnt_0 | ((a_uint64_t)pre_l2_cnt_tbl.bf.rx_byte_cnt_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(vsi=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PRE_L2_CNT_TBL RX_DROP:");
	for (i = 0; i < PRE_L2_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_pre_l2_cnt_tbl_get(dev_id, i, &pre_l2_cnt_tbl);
		if (show_type == A_FALSE)
			value = pre_l2_cnt_tbl.bf.rx_drop_pkt_cnt_0 | ((a_uint64_t)pre_l2_cnt_tbl.bf.rx_drop_pkt_cnt_1 << 24);
		else
			value = pre_l2_cnt_tbl.bf.rx_drop_byte_cnt_0 | ((a_uint64_t)pre_l2_cnt_tbl.bf.rx_drop_byte_cnt_1 << 24);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(vsi=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void adpt_hppe_debug_port_tx_drop_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union port_tx_drop_cnt_tbl_u port_tx_drop_cnt_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PORT_TX_DROP_CNT_TBL TX_DROP:");
	for (i = 0; i < PORT_TX_DROP_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_port_tx_drop_cnt_tbl_get(dev_id, i, &port_tx_drop_cnt_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)port_tx_drop_cnt_tbl.bf.tx_drop_pkt_cnt;
		else
			value = port_tx_drop_cnt_tbl.bf.tx_drop_byte_cnt_0 | ((a_uint64_t)port_tx_drop_cnt_tbl.bf.tx_drop_byte_cnt_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_eg_vsi_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union eg_vsi_counter_tbl_u eg_vsi_counter_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "EG_VSI_COUNTER_TBL TX:");
	for (i = 0; i < EG_VSI_COUNTER_TBL_MAX_ENTRY; i++)
	{
		hppe_eg_vsi_counter_tbl_get(dev_id, i, &eg_vsi_counter_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)eg_vsi_counter_tbl.bf.tx_packets;
		else
			value = eg_vsi_counter_tbl.bf.tx_bytes_0 | ((a_uint64_t)eg_vsi_counter_tbl.bf.tx_bytes_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(vsi=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_port_tx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union port_tx_counter_tbl_reg_u port_tx_counter_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PORT_TX_COUNTER_TBL TX:");
	for (i = 0; i < PORT_TX_COUNTER_TBL_REG_MAX_ENTRY; i++)
	{
		hppe_port_tx_counter_tbl_reg_get(dev_id, i, &port_tx_counter_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)port_tx_counter_tbl.bf.tx_packets;
		else
			value = port_tx_counter_tbl.bf.tx_bytes_0 | ((a_uint64_t)port_tx_counter_tbl.bf.tx_bytes_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_vp_tx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union vp_tx_counter_tbl_reg_u vp_tx_counter_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "VP_TX_COUNTER_TBL TX:");
	for (i = 0; i < VP_TX_COUNTER_TBL_REG_MAX_ENTRY; i++)
	{
		hppe_vp_tx_counter_tbl_reg_get(dev_id, i, &vp_tx_counter_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)vp_tx_counter_tbl.bf.tx_packets;
		else
			value = vp_tx_counter_tbl.bf.tx_bytes_0 | ((a_uint64_t)vp_tx_counter_tbl.bf.tx_bytes_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_queue_tx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union queue_tx_counter_tbl_u queue_tx_counter_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "QUEUE_TX_COUNTER_TBL TX:");
	for (i = 0; i < QUEUE_TX_COUNTER_TBL_MAX_ENTRY; i++)
	{
		hppe_queue_tx_counter_tbl_get(dev_id, i, &queue_tx_counter_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)queue_tx_counter_tbl.bf.tx_packets;
		else
			value = queue_tx_counter_tbl.bf.tx_bytes_0 | ((a_uint64_t)queue_tx_counter_tbl.bf.tx_bytes_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(queue=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_vp_tx_drop_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	union vp_tx_drop_cnt_tbl_u vp_tx_drop_cnt_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "VP_TX_DROP_CNT_TBL TX_DROP:");
	for (i = 0; i < VP_TX_DROP_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_vp_tx_drop_cnt_tbl_get(dev_id, i, &vp_tx_drop_cnt_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)vp_tx_drop_cnt_tbl.bf.tx_drop_pkt_cnt;
		else
			value = vp_tx_drop_cnt_tbl.bf.tx_drop_byte_cnt_0 | ((a_uint64_t)vp_tx_drop_cnt_tbl.bf.tx_drop_byte_cnt_1 << 32);
		if (value > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%04d)", value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_cpu_code_counter_get(a_uint32_t dev_id, a_bool_t show_type,
		char **buf, ssize_t *count, a_uint32_t sd_size)
{
	union drop_cpu_cnt_tbl_u drop_cpu_cnt_tbl;
	a_uint64_t value;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "CPU_CODE_CNT_TBL:");
	for (i = 0; i < CPU_CODE_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_drop_cpu_cnt_tbl_get(dev_id, i, &drop_cpu_cnt_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)drop_cpu_cnt_tbl.bf.pkt_cnt;
		else
			value = drop_cpu_cnt_tbl.bf.byte_cnt_0 | ((a_uint64_t)drop_cpu_cnt_tbl.bf.byte_cnt_1 << 32);
		if (value > 0)
		{
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			if (i >= 0 && i < sd_size)
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(%s),cpucode:%d", value, cpucode[i], i);
			else
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(Unknown),cpucode:%d", value, i);
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_hppe_debug_drop_cpu_counter_get(a_uint32_t dev_id, a_bool_t show_type,
		char **buf, ssize_t *count, a_uint32_t sd_size)
{
	union drop_cpu_cnt_tbl_u drop_cpu_cnt_tbl;
	a_uint64_t value;
	int i, tags, sign;
	int drop_code_idx;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "DROP_CPU_CNT_TBL:");
	for (i = CPU_CODE_CNT_TBL_MAX_ENTRY; i < DROP_CPU_CNT_TBL_MAX_ENTRY; i++)
	{
		hppe_drop_cpu_cnt_tbl_get(dev_id, i, &drop_cpu_cnt_tbl);
		if (show_type == A_FALSE)
			value = (a_uint64_t)drop_cpu_cnt_tbl.bf.pkt_cnt;
		else
			value = drop_cpu_cnt_tbl.bf.byte_cnt_0 | ((a_uint64_t)drop_cpu_cnt_tbl.bf.byte_cnt_1 << 32);
		if (value > 0)
		{
			drop_code_idx = (i - CPU_CODE_CNT_TBL_MAX_ENTRY) / SSDK_MAX_PORT_NUM;

			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
			*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");

			if (drop_code_idx >= 0 && drop_code_idx < sd_size)
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%d:%s),dropcode:%d", value,
					(i - CPU_CODE_CNT_TBL_MAX_ENTRY) % SSDK_MAX_PORT_NUM,
					dropcode[drop_code_idx], drop_code_idx);
			else
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port=%d:Unknown),dropcode:%d", value,
                    (i - CPU_CODE_CNT_TBL_MAX_ENTRY) % SSDK_MAX_PORT_NUM, drop_code_idx);
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_appe_debug_vp_rx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	a_uint64_t rx_value = 0;
	a_uint32_t rx_pkt = 0;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "VP_RX_COUNTER_TBL RX:");
	for (i = 0; i < PORT_RX_CNT_TBL_NUM; i++)
	{
		if (show_type == A_FALSE)
			appe_port_rx_cnt_tbl_rx_pkt_cnt_get(dev_id, i, &rx_pkt);
		else
			appe_port_rx_cnt_tbl_rx_byte_cnt_get(dev_id, i, &rx_value);
		if (rx_value > 0 || rx_pkt > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			if (show_type == A_FALSE)
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15u(port_id=%04d)", rx_pkt, i);
			else
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port_id=%04d)", rx_value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_appe_debug_vp_rx_drop_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	a_uint64_t rx_value = 0;
	a_uint32_t rx_pkt = 0;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "VP_RX_DROP_CNT_TBL RX_DROP:");
	for (i = 0; i < PORT_RX_CNT_TBL_NUM; i++)
	{
		if (show_type == A_FALSE)
			appe_port_rx_cnt_tbl_rx_drop_pkt_cnt_get(dev_id, i, &rx_pkt);
		else
			appe_port_rx_cnt_tbl_rx_drop_byte_cnt_get(dev_id, i, &rx_value);
		if (rx_value > 0 || rx_pkt > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			if (show_type == A_FALSE)
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15u(port_id=%04d)", rx_pkt, i);
			else
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port_id=%04d)", rx_value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_appe_debug_port_rx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	a_uint64_t rx_value = 0;
	a_uint32_t rx_pkt = 0;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PORT_RX_COUNTER_TBL RX:");
	for (i = 0; i < PHY_PORT_RX_CNT_TBL_NUM; i++)
	{
		if (show_type == A_FALSE)
			appe_phy_port_rx_cnt_tbl_rx_pkt_cnt_get(dev_id, i, &rx_pkt);
		else
			appe_phy_port_rx_cnt_tbl_rx_byte_cnt_get(dev_id, i, &rx_value);
		if (rx_value > 0 || rx_pkt > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			if (show_type == A_FALSE)
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15u(port_id=%04d)", rx_pkt, i);
			else
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port_id=%04d)", rx_value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

static void
adpt_appe_debug_port_rx_drop_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	a_uint64_t rx_value = 0;
	a_uint32_t rx_pkt = 0;
	int i, tags, sign;

	sign = tags = 0;
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "PORT_RX_DROP_CNT_TBL RX_DROP:");
	for (i = 0; i < PHY_PORT_RX_CNT_TBL_NUM; i++)
	{
		if (show_type == A_FALSE)
			appe_phy_port_rx_cnt_tbl_rx_drop_pkt_cnt_get(dev_id, i, &rx_pkt);
		else
			appe_phy_port_rx_cnt_tbl_rx_drop_byte_cnt_get(dev_id, i,
				&rx_value);
		if (rx_value > 0 || rx_pkt > 0)
		{
			if (sign) {
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%-35s", "");
			}
			sign = 0;
			if (show_type == A_FALSE)
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15u(port_id=%04d)", rx_pkt, i);
			else
				*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "%15llu(port_id=%04d)", rx_value, i);
			if (++tags % 3 == 0)
				sign = 1;
		}
	}
	*count += scnprintf(*buf + *count, SSDK_BUF_REMAINING(count), "\n");
}

/* if show_type = A_FALSE, show packets.
 * if show_type = A_TRUE, show bytes.
 */
static sw_error_t
adpt_hppe_debug_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count)
{
	/* show PRX DROP_CNT */
	adpt_hppe_debug_prx_drop_cnt_get(dev_id, buf, count);

	/* show PRX DROP_PKT_STAT */
	adpt_hppe_debug_prx_drop_pkt_stat_get(dev_id, show_type, buf, count);

	/* show IPR_PKT_NUM */
	adpt_hppe_debug_ipx_pkt_num_get(dev_id, show_type, buf, count);

	/* show VLAN_CNT_TBL */
	adpt_hppe_debug_vlan_counter_get(dev_id, show_type, buf, count);

	/* show PRE_L2_CNT_TBL */
	adpt_hppe_debug_pre_l2_counter_get(dev_id, show_type, buf, count);

	/* show PORT_TX_DROP_CNT_TBL */
	adpt_hppe_debug_port_tx_drop_counter_get(dev_id, show_type, buf, count);

	if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
		adpt_httppe_debug_eg_vsi_counter_get(dev_id, show_type, buf, count);

		adpt_httppe_debug_port_tx_counter_get(dev_id, show_type, buf, count);

		adpt_httppe_debug_vp_tx_counter_get(dev_id, show_type, buf, count);

		adpt_httppe_debug_queue_tx_counter_get(dev_id, show_type, buf, count);
#endif
	} else {
		/* show EG_VSI_COUNTER_TBL */
		adpt_hppe_debug_eg_vsi_counter_get(dev_id, show_type, buf, count);

		/* show PORT_TX_COUNTER_TBL */
		adpt_hppe_debug_port_tx_counter_get(dev_id, show_type, buf, count);

		/* show VP_TX_COUNTER_TBL */
		adpt_hppe_debug_vp_tx_counter_get(dev_id, show_type, buf, count);

		/* show QUEUE_TX_COUNTER_TBL */
		adpt_hppe_debug_queue_tx_counter_get(dev_id, show_type, buf, count);
	}

	/* show VP_TX_DROP_CNT_TBL */
	adpt_hppe_debug_vp_tx_drop_counter_get(dev_id, show_type, buf, count);

	/* show CPU_CODE_CNT */
	adpt_hppe_debug_cpu_code_counter_get(dev_id, show_type,
		buf, count, (sizeof(cpucode)/sizeof(cpucode[0])));

	if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
		adpt_httppe_debug_drop_cpu_counter_get(dev_id, show_type,
			buf, count, (sizeof(dropcode)/sizeof(dropcode[0])));
#endif
	} else {
		/* show DROP_CPU_CNT_TBL */
		adpt_hppe_debug_drop_cpu_counter_get(dev_id, show_type,
			buf, count, (sizeof(dropcode)/sizeof(dropcode[0])));
	}

	/* show VP_PORT_RX_COUNTER_TBL*/
	adpt_appe_debug_vp_rx_counter_get(dev_id, show_type, buf, count);
	/* show VP_PORT_RX_DROP_CNT_TBL*/
	adpt_appe_debug_vp_rx_drop_counter_get(dev_id, show_type, buf, count);
	/* show PORT_RX_COUNTER_TBL*/
	adpt_appe_debug_port_rx_counter_get(dev_id, show_type, buf, count);
	/* show PORT_RX_DROP_CNT_TBL*/
	adpt_appe_debug_port_rx_drop_counter_get(dev_id, show_type, buf, count);

	return SW_OK;
}

sw_error_t adpt_hppe_misc_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL) {
		return SW_FAIL;
	}

	p_adpt_api->adpt_debug_counter_set = adpt_hppe_debug_counter_set;
	p_adpt_api->adpt_debug_counter_get = adpt_hppe_debug_counter_get;

	return SW_OK;
}

/**
 * @}
 */
