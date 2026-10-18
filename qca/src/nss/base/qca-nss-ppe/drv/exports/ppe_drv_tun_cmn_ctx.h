/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_TUN_CMN_CTX_H_
#define _PPE_DRV_TUN_CMN_CTX_H_

#include <linux/if_pppox.h>
#include <linux/udp.h>

/*
 * L2 flags
 */
#define PPE_DRV_TUN_CMN_CTX_L2_INHERIT_SPCP	0x01	/**< Inherit SVLAN PCP from inner to outer >*/
#define PPE_DRV_TUN_CMN_CTX_L2_INHERIT_CPCP	0x02	/**< Inherit CVLAN PCP from inner to outer >*/
#define PPE_DRV_TUN_CMN_CTX_L2_INHERIT_SDEI	0x04	/**< Inherit SVLAN DEI from inner to outer >*/
#define PPE_DRV_TUN_CMN_CTX_L2_INHERIT_CDEI	0x08	/**< Inherit CVLAN DEI from inner to outer >*/
#define PPE_DRV_TUN_CMN_CTX_L2_SVLAN_VALID	0x10	/**< SVLAN header valid >*/
#define PPE_DRV_TUN_CMN_CTX_L2_CVLAN_VALID	0x20	/**< CVLAN header valid >*/
#define PPE_DRV_TUN_CMN_CTX_L2_PPPOE_VALID	0x40	/**< PPPoE header valid >*/

/*
 * L3 flags
 */
#define PPE_DRV_TUN_CMN_CTX_L3_IPV4		0x01	/**< Tunnel header is IPv4 >*/
#define PPE_DRV_TUN_CMN_CTX_L3_IPV6		0x02	/**< Tunnel header is IPv6 >*/
#define PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP	0x04	/**< Inherit DSCP from inner to outer >*/
#define PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL	0x08	/**< Inherit TTL from inner to outer >*/
#define PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM_TX	0x10	/**< Set UDP Checksum to zero for VXLAN IPV4 and IPV6 tunnels >*/
#define PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM6_RX	0x20	/**< Allow zero UDP Checksum for VXLAN IPV6 tunnel only >*/
#define PPE_DRV_TUN_CMN_CTX_L3_DF_BIT_SET	0x40	/**< Set DF bit in outer tunnel header >*/
#define PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DF	0x80	/**< Copy DF bit from inner to outer tunnel header >*/

/*
 * GRE flags
 */
#define PPE_DRV_TUN_CMN_CTX_GRE_ENCAP_KEY		0x01	/**< Encap key is enabled >*/
#define PPE_DRV_TUN_CMN_CTX_GRE_DECAP_KEY		0x02	/**< Decap key is enabled >*/
#define PPE_DRV_TUN_CMN_CTX_GRE_ENCAP_CSUM		0x04	/**< Encap checksum is enabled >*/
#define PPE_DRV_TUN_CMN_CTX_GRE_DECAP_CSUM		0x08	/**< Decap checksum is enabled >*/

/*
 * Max Encap header length
 */
#define PPE_DRV_TUN_CMN_ENCAP_HDR_DATA_LEN 128			/**< Custom tunnel Encap header data length >*/

/*
 * ppe_drv_tun_cmn_ctx_cust_match_flags
 *	Bit positions for custom tunnel decap match flags.
 *	Use ppe_drv_tun_cust_match_flag_set() and ppe_drv_tun_cust_match_flag_test()
 *	to set and test bits in the match_bitmap field.
 */
enum ppe_drv_tun_cmn_ctx_cust_match_flags {
	PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_SIP_EN   = 0,	/**< Bit position: Enable Decap SRC IP check >*/
	PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_DIP_EN   = 1,	/**< Bit position: Enable Decap DEST IP check >*/
	PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_PROTO_EN = 2,	/**< Bit position: Enable Decap Protocol check >*/
	PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_SPORT_EN = 3,	/**< Bit position: Enable Decap SRC PORT check >*/
	PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_DPORT_EN = 4,	/**< Bit position: Enable Decap DEST PORT check >*/
};

/*
 * ppe_drv_tun_cust_match_flag_set
 *	Set a match flag bit in the bitmap using its bit-position enum value.
 */
static inline void ppe_drv_tun_cust_match_flag_set(uint8_t *bitmap,
						    enum ppe_drv_tun_cmn_ctx_cust_match_flags flag)
{
	*bitmap |= (uint8_t)(1 << flag);
}

/*
 * ppe_drv_tun_cust_match_flag_test
 *	Test whether a match flag bit is set in the bitmap.
 */
static inline bool ppe_drv_tun_cust_match_flag_test(uint8_t bitmap,
						     enum ppe_drv_tun_cmn_ctx_cust_match_flags flag)
{
	return !!(bitmap & (uint8_t)(1 << flag));
}

/*
 * ppe_drv_tun_cmn_ctx_cust_tun_id
 *	Custom tunnel identifier type.
 */
enum ppe_drv_tun_cmn_ctx_cust_tun_id {
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_ETH_PROTO = 0,	/**< Use Ethernet Protocol as tunnel identifier >*/
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_IP_PROTO  = 1,	/**< Use IP Protocol as tunnel identifier >*/
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_UDP_PORT  = 2,	/**< Use UDP Ports as tunnel identifier >*/
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_CUSTOM    = 3,	/**< Use Custom value as Identifier >*/
};

/*
 * ppe_drv_tun_cmn_ctx_cust_tun_offset
 *	Custom tunnel UDF offset reference point.
 */
enum ppe_drv_tun_cmn_ctx_cust_tun_offset {
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L2_START = 0,	/**< Offset is provided from Start of L2 header >*/
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L3_START = 1,	/**< Offset is provided from Start of L3 header >*/
	PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L4_START = 2,	/**< Offset is provided from Start of L4 header >*/
};

/*
 * ppe_drv_tun_cmn_ctx_encap_ecn
 *	PPE tunnel encap ecn mode
 */
enum ppe_drv_tun_cmn_ctx_encap_ecn {
	PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_NO_UPDATE,			 /**< no update ecn >*/
	PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC3168_LIMIT_RFC6040_CMPAT_MODE,	 /**< RFC3168 limitation mode
									      RFC6040 compatibility mode for encapsulation >*/
	PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC3168_FULL_MODE,		 /**< RFC3168 full mode for encapsulation >*/
	PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE,	 /**< RFC4301 mode and RFC6040 normal mode for encapsulation >*/
};

/*
 * ppe_drv_tun_cmn_ctx_decap_ecn
 *	PPE tunnel decap ecn mode
 */
enum ppe_drv_tun_cmn_ctx_decap_ecn {
	PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC3168_MODE = 0,	/**< RFC3168 mode for decapsulation >*/
	PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC4301_MODE = 1,	/**< RFC4301 mode for decapsulation >*/
	PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE = 2,	/**< RFC6040 mode for decapsulation >*/
};

/*
 * ppe_drv_tun_cmn_ctx_cust_type
 *	Custom tunnel type
 */
enum ppe_drv_tun_cmn_ctx_cust_type {
	PPE_DRV_TUN_CMN_CTX_CUST_TYPE_UDP_ST = 1, /**< Custom tunnel type udp speedtest >*/
};

/*
 * ppe_drv_tun_cmn_ctx_type
 *	PPE Tunnel types
 */
enum ppe_drv_tun_cmn_ctx_type {
	PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP = 1,	/**< PPE Tunnel header type GRETAP >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN,		/**< PPE Tunnel header type VxLAN >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6,		/**< PPE Tunnel header type DSLite/MAP-E >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_MAPT,		/**< PPE Tunnel header type MAP-T >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2,	/**< PPE Tunnel header type L2TP-V2 >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_CUST,		/**< Custom PPE Tunnel header >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE,	/**< PPE Tunnel header type VxLAN-GPE >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN,	/**< PPE Tunnel header type for L3 GRE Tunnel >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2,	/**< PPE custom L2 tunnel type >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3,	/**< PPE custom L3 tunnel type >*/
	PPE_DRV_TUN_CMN_CTX_TYPE_MAX
};

/*
 * ppe_drv_tun_cmn_ctx_flowlabel
 *	PPE flow label modes
 */
enum ppe_drv_tun_cmn_ctx_flowlabel {
	PPE_DRV_TUN_CMN_CTX_FLOW_LABEL_FIX = 0,	/**< PPE Tunnel flow label Fix mode >*/
	PPE_DRV_TUN_CMN_CTX_FLOW_LABEL_HASH,	/**< PPE Tunnel flow label Hash mode >*/
	PPE_DRV_TUN_CMN_CTX_FLOW_LABEL_COPY,	/**< PPE Tunnel flow label copy mode >*/
};

/*
 * ppe_drv_tun_cmn_ctx_l3
 *	Layer 3 header parameters
 */
struct ppe_drv_tun_cmn_ctx_l3 {
	uint32_t saddr[4];	/**< Source address >*/
	uint32_t daddr[4];	/**< Destination address >*/
	uint32_t flags;		/**< L3 header flags >*/
	uint16_t proto;		/**< IP protocol >*/
	uint8_t dscp;		/**< Static DSCP value for outer header >*/
	uint8_t ttl;		/**< Static TTL value for outer header >*/
	uint32_t flow_label_val;	/**< flow label fixed value >*/
	enum ppe_drv_tun_cmn_ctx_encap_ecn encap_ecn_mode;	/**< RFC for ECN in encap direction >*/
	enum ppe_drv_tun_cmn_ctx_decap_ecn decap_ecn_mode;	/**< RFC for ECN in decap direction >*/
	enum ppe_drv_tun_cmn_ctx_flowlabel flow_label;	/**< Flow label mode >*/
};

/*
 * ppe_drv_tun_cmn_ctx_pppoe
 *	PPPoE header parameters
 */
struct ppe_drv_tun_cmn_ctx_pppoe {
	struct pppoe_hdr ph;	/**< PPPoE header >*/
	__be16 ppp_proto;	/**< PPPoE payload protocol >*/
	uint8_t server_mac[ETH_ALEN];	/** < PPPoE server mac >*/
};

/*
 * ppe_drv_tun_cmn_ctx_vlan
 * 	VLAN information
 */
struct ppe_drv_tun_cmn_ctx_vlan {
	uint16_t tpid;		/**< TPID in VLAN header. */
	uint16_t tci;		/**< TCI in VLAN header. */
};

/*
 * ppe_drv_tun_cmn_ctx_l2
 *	Layer 2 header parameters
 */
struct ppe_drv_tun_cmn_ctx_l2 {
	struct ppe_drv_tun_cmn_ctx_pppoe pppoe;		/**< PPPoE header >*/
	struct ppe_drv_tun_cmn_ctx_vlan vlan[2];	/**< VLAN header >*/
	uint16_t smac[3];				/**< Source MAC address >*/
	uint16_t dmac[3];				/**< Destination MAC address >*/
	uint8_t xmit_port;				/**< Xmit port number >*/
	uint32_t flags;					/**< L2 header flags >*/
	uint16_t eth_type;				/**< Ether type >*/
};

/*
 * ppe_drv_tun_cmn_ctx_gre
 *	GRE header parameters
 */
struct ppe_drv_tun_cmn_ctx_gre {
	uint32_t encap_key;	/**< GRE key used for encapsulation of the packet >*/
	uint32_t decap_key;	/**< GRE key used for decapsulation of the packet>*/
	uint16_t flags;		/**< flags to set optional fields >*/
};

/*
 * ppe_drv_tun_cmn_ctx_vxlan
 *	VxLAN header parameters
 */
struct ppe_drv_tun_cmn_ctx_vxlan {
	uint32_t vni;		/**< VxLAN Network Identifier >*/
	uint32_t flags;		/**< Header flags >*/
	union {
		uint8_t next_proto;	/**< Next protocol used for VxLAN-GPE >*/
		uint16_t policy_id;	/**< Group Policy ID used for VxLAN-GPO >*/
	} u;
	uint16_t src_port_min;	/**< UDP source port min >*/
	uint16_t src_port_max;	/**< UDP source port max >*/
	uint16_t dest_port;	/**< UDP destination port >*/
};

/*
 * ppe_drv_tun_xlate_rule
 *	PPE tunnel transaltion rule for Map-t case
 */
struct ppe_drv_tun_cmn_ctx_xlate_rule {
	uint32_t ipv6_prefix[4];        /**< IPv6 prefix >*/
	uint32_t ipv4_prefix;           /**< IPv4 prefix >*/
	uint32_t ipv6_prefix_len;       /**< IPv6 prefix length >*/
	uint32_t ipv4_prefix_len;       /**< IPv4 prefix length >*/
	uint32_t ipv6_suffix[4];        /**< IPv6 suffix >*/
	uint32_t ipv6_suffix_len;       /**< IPv6 suffix length >*/
	uint32_t ea_len;                /**< EA bits length >*/
	uint32_t psid_offset;           /**< PSID offset >*/
};

/*
 * ppe_drv_tun_cmn_ctx_mapt
 *	MAP-T header parameters
 */
struct ppe_drv_tun_cmn_ctx_mapt {
	struct ppe_drv_tun_cmn_ctx_xlate_rule local;	/**< Local translation MAP rule >*/
	struct ppe_drv_tun_cmn_ctx_xlate_rule remote;	/**< Remote translation MAP rule >*/
};

/*
 * ppe_drv_tun_cmn_ctx_l2tp
 * 	L2TP header parameters
 */
struct ppe_drv_tun_cmn_ctx_l2tp {
	uint16_t sport;			/**< Source Port >*/
	uint16_t dport;			/**< Destination Port >*/
	uint32_t tunnel_id;		/**< Tunnel ID >*/
	uint32_t peer_tunnel_id;	/**< Peer Tunnel ID >*/
	uint32_t session_id;		/**< Session ID >*/
	uint32_t peer_session_id;	/**< Peer Session ID >*/
};

/*
 * ppe_drv_tun_cmn_ctx_cust_udp_st
 *	UDP Speedtest L4 params.
 */
struct ppe_drv_tun_cmn_ctx_cust_udp_st {
	uint16_t sport;		/**< Source Port >*/
	uint16_t dport;		/**< Destination Port >*/
};

/*
 * ppe_drv_tun_cmn_ctx_cust
 *	Custom tunnel context.
 */
struct ppe_drv_tun_cmn_ctx_cust {
	union {
		struct ppe_drv_tun_cmn_ctx_cust_udp_st udp_st;	/**< UDP ST context >*/
	} cust_tun;
	enum ppe_drv_tun_cmn_ctx_cust_type cust_type;		/**< Custom tunnel type >*/
};

/*
 * ppe_drv_tun_cmn_ctx_mape
 *	Mape tunnel context.
 */
struct ppe_drv_tun_cmn_ctx_mape_br {
	bool is_mape_br;		/**< 1 - BR, 0 - CE in MAP-E tunnel >*/
	bool is_mape_activate;		/**< Active MAP-E tunnel >*/
};

/*
 * ppe_drv_tun_cust_tun_ecap
 *	Custom tunnel encap context.
 */
struct ppe_drv_tun_cust_tun_ecap {
	uint8_t encap_hdr_data_len;		/**< Fixed encap header data length >*/
	uint8_t encap_hdr_data[PPE_DRV_TUN_CMN_ENCAP_HDR_DATA_LEN];	/**< Fixed encap header data to be added to every packet post ip or UDP header >*/
	bool proto_update_en;			/**< Enable protocol field update >*/
	uint8_t proto_offset;			/**< Offset in bytes from start of Ethernet header to the protocol field to be updated >*/
	uint8_t proto_size;			/**< width of the field in bits which needs to be updated. Max 16 bits update >*/
	uint16_t ipv4_proto;			/**< 16 bit protocol value to be updated if inner packet is IPv4 >*/
	uint16_t ipv6_proto;			/**< 16 bit protocol value to be updated if inner packet is IPv6 >*/
};

#define PPE_DRV_TUN_PRGM_UDF_MAX_ENTRIES 16	/**< Maximum program UDF entries per parser >*/

/*
 * ppe_drv_tun_cmn_ctx_inner_pkt_type
 *	Inner packet type for program UDF matching
 */
enum ppe_drv_tun_cmn_ctx_inner_pkt_type {
	PPE_DRV_TUN_CMN_CTX_INNER_PKT_TYPE_ETHERNET = 0,	/**< Inner packet is Ethernet >*/
	PPE_DRV_TUN_CMN_CTX_INNER_PKT_TYPE_IPV4 = 1,		/**< Inner packet is IPv4 >*/
	PPE_DRV_TUN_CMN_CTX_INNER_PKT_TYPE_IPV6 = 2,		/**< Inner packet is IPv6 >*/
};

/*
 * ppe_drv_tun_cmn_ctx_type_match_entry
 *	Type match entry (program UDF) for custom tunnel
 */
struct ppe_drv_tun_cmn_ctx_type_match_entry {
	enum ppe_drv_tun_cmn_ctx_inner_pkt_type inner_pkt_type;	/**< Inner packet type for this entry >*/
	uint16_t udf0_val;					/**< User defined field 0 value to match >*/
	uint16_t udf0_mask;					/**< User defined field 0 mask to match upto 16 bits >*/
	uint16_t udf1_val;					/**< User defined field 1 value to match >*/
	uint16_t udf1_mask;					/**< User defined field 1 mask to match upto 16 bits >*/
	uint16_t udf2_val;					/**< User defined field 2 value to match >*/
	uint16_t udf2_mask;					/**< User defined field 2 mask to match upto 16 bits >*/
	bool udf0_en;						/**< User defined field 0 check enable >*/
	bool udf1_en;						/**< User defined field 1 check enable >*/
	bool udf2_en;						/**< User defined field 2 check enable >*/
};

/*
 * ppe_drv_tun_cmn_ctx_type_match
 *	Type match configuration (program UDF entries)
 */
struct ppe_drv_tun_cmn_ctx_type_match {
	uint8_t num_entries;							/**< Number of type match entries (1-16) >*/
	struct ppe_drv_tun_cmn_ctx_type_match_entry entries[PPE_DRV_TUN_PRGM_UDF_MAX_ENTRIES];	/**< Type match entries array >*/
};

/*
 * ppe_drv_tun_cmn_ctx_instance_match
 *	Instance match configuration (tunnel UDF)
 */
struct ppe_drv_tun_cmn_ctx_instance_match {
	uint16_t instance_udf0_val;	/**< Instance UDF 0 value to match >*/
	uint16_t instance_udf0_mask;	/**< Instance UDF 0 mask to match >*/
	uint16_t instance_udf1_val;	/**< Instance UDF 1 value to match >*/
	uint16_t instance_udf1_mask;	/**< Instance UDF 1 mask to match >*/
	bool instance_udf0_en;		/**< Instance UDF 0 check enable >*/
	bool instance_udf1_en;		/**< Instance UDF 1 check enable >*/
};

/*
 * ppe_drv_tun_cust_tun_decap
 *	Custom tunnel decap context.
 */
struct ppe_drv_tun_cust_tun_decap {
	enum ppe_drv_tun_cmn_ctx_cust_tun_offset offset_type;		/**< Offset type >*/
	enum ppe_drv_tun_cmn_ctx_cust_tun_id tunnel_identifier;		/**< Tunnel identifier >*/
	struct ppe_drv_tun_cmn_ctx_type_match type_match;		/**< Type match configuration (program UDF entries) >*/
	struct ppe_drv_tun_cmn_ctx_instance_match instance_match;	/**< Instance match configuration (tunnel UDF) >*/
	uint8_t decap_data_len;						/**< Decap data length >*/
	uint8_t match_bitmap;						/**< Decap match bitmap (use flag_set/flag_test helpers) >*/
	uint8_t exp_mode;						/**< Exception mode >*/
	uint8_t udf0_offset;						/**< User defined field 0 offset (type match) >*/
	uint8_t udf1_offset;						/**< User defined field 1 offset (type match) >*/
	uint8_t udf2_offset;						/**< User defined field 2 offset (type match) >*/
	uint8_t instance_udf0_offset;					/**< Instance UDF 0 offset >*/
	uint8_t instance_udf1_offset;					/**< Instance UDF 1 offset >*/
	bool udp_csum_zero_en;						/**< Enable decap of pkts with UDP csum zero >*/
};

/*
 * ppe_drv_tun_cmn_ctx_custom_tun
 *	custom tunnel common context
 */
struct ppe_drv_tun_cmn_ctx_custom_tun {
	struct ppe_drv_tun_cust_tun_decap decap_data;	/**< Tunnel decap data >*/
	struct ppe_drv_tun_cust_tun_ecap encap_data;	/**< Tunnel encap data >*/
	struct udphdr udph;				/**< UDP header data >*/
	bool udp_hdr_valid;				/**< UDP header valid >*/
};

/*
 * ppe_drv_tun_cmn_ctx
 *	PPE tunnel header parameters
 */
struct ppe_drv_tun_cmn_ctx {
	struct ppe_drv_tun_cmn_ctx_l2 l2;	/**< L2 Header >*/
	struct ppe_drv_tun_cmn_ctx_l3 l3;	/**< L3 Header >*/
	union {
		struct ppe_drv_tun_cmn_ctx_gre gre;	/**< GRE tunnel configuration >*/
		struct ppe_drv_tun_cmn_ctx_vxlan vxlan;	/**< VxLAN tunnel configuration >*/
		struct ppe_drv_tun_cmn_ctx_mapt mapt;	/**< Map-T tunnel configuration >*/
		struct ppe_drv_tun_cmn_ctx_l2tp l2tp;	/**< L2TP tunnel configuration >*/
		struct ppe_drv_tun_cmn_ctx_cust cust;	/**< Custom tunnel configuration >*/
		struct ppe_drv_tun_cmn_ctx_mape_br mape;	/**< Map-E BR tunnel configuration >*/
		struct ppe_drv_tun_cmn_ctx_custom_tun user;	/**< Custom tunnel context >*/
	} tun;
	enum ppe_drv_tun_cmn_ctx_type type;
};

/*
 * ppe_drv_tun_cmn_ctx_tun_is_ipv6
 *	Check tunnel type is IPv6
 */
static inline bool ppe_drv_tun_cmn_ctx_tun_is_ipv6(struct ppe_drv_tun_cmn_ctx *th)
{
	return !!(th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_IPV6);
}

#endif /* _PPE_DRV_TUN_CMN_CTX_H_ */
