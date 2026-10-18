/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_TUN_H_
#define _PPE_TUN_H_

#include <ppe_vp_public.h>
#include <linux/udp.h>

/*
 * ppe_drv_tun_custom_tun_stats
 *	custom tunnel stats structure
 */
struct ppe_drv_tun_custom_tun_stats {
	uint32_t rx_pkt_cnt;		/**< rx packet counter >*/
	uint32_t rx_drop_pkt_cnt;	/**< rx drop packet counter >*/
	uint32_t tx_pkt_cnt;		/**< tx packet counter >*/
	uint32_t tx_drop_pkt_cnt;	/**< tx drop packet counter >*/
	uint64_t rx_byte_cnt;		/**< rx byte counter >*/
	uint64_t rx_drop_byte_cnt;	/**< rx drop byte counter >*/
	uint64_t tx_byte_cnt;		/**< tx byte counter >*/
	uint64_t tx_drop_byte_cnt;	/**< tx drop byte counter >*/
};

/*
 * ppe_tun_custom_exp_method_t
 *	Exception method to handle custom tunnel exceptioned packets
 */
typedef bool (*ppe_tun_custom_exp_method_t)(struct sk_buff *skb, void *user_data);

/*
 * ppe_tun_custom_stats_method_t
 *	Stats update method to handle custom tunnel statistics
 */
typedef bool (*ppe_tun_custom_stats_method_t)(struct net_device *dev, struct ppe_drv_tun_custom_tun_stats *stats, void *user_data);

/*
 * ppe_tun_l2tp_info
 *	L2TP specific tunnel information
 */
struct ppe_tun_l2tp_info {
	uint16_t session_id;
	uint16_t tunnel_id;
};

/*
 * ppe_tun_custom_info
 *	custom L2/L3 tunnel info
 */
struct ppe_tun_custom_info {
	ppe_tun_custom_exp_method_t exp_cb;		/**< Exception callback for custom tunnel >*/
	ppe_tun_custom_stats_method_t stats_cb;		/**< Stats update callback for custom tunnel >*/
	void *custom_tunnel_data;			/**< Opaque data passed to callbacks >*/
};

/*
 * ppe_tun_data
 *	Tunnel specific information to be stored for a tunnel
 */
typedef union {
	struct ppe_tun_custom_info cust_info;	/**< Custom Tunnel information >*/
	struct ppe_tun_l2tp_info l2tp_info;	/**< L2TP tunnel information >*/
} ppe_tun_data;

typedef ppe_vp_hw_stats_t ppe_tun_hw_stats;
typedef bool(*ppe_tun_exception_method_t)(struct ppe_vp_cb_info *info, ppe_tun_data  *tun_data);
typedef bool (*ppe_tun_stats_method_t)(struct net_device *dev, ppe_tun_hw_stats *stats, ppe_tun_data *tun_data);

/*
 * ppe_tun_excp
 *	ppe_tun structure to hold callback data.
 */
struct ppe_tun_excp {
	ppe_tun_exception_method_t src_excp_method;	/**< callback for exception packets with src VP >*/
	ppe_tun_exception_method_t dest_excp_method;	/**< callback for exception packets with dest VP >*/
	ppe_tun_stats_method_t stats_update_method;	/**< callback for updating tunnel statistics >*/
	ppe_tun_data *tun_data;				/**< Tunnel specific data from client >*/
};

/*
 * ppe_tun_ret
 *	PPE tun return type
 */
typedef enum ppe_tun_ret {
	PPE_TUN_RET_SUCCESS = 0,			/**< Return Success >*/
	PPE_TUN_RET_CUSTOM_TUN_INVALID_INPUT,		/**< Invalid Input >*/
	PPE_TUN_RET_CUSTOM_TUN_ALLOC_FAILED,		/**< Tunnel Allocation failed >*/
	PPE_TUN_RET_CUSTOM_TUN_CONFIGURE_FAILED,	/**< Tunnel Configuration failed >*/
	PPE_TUN_RET_CUSTOM_TUN_ACTIVATE_FAILED,		/**< Tunnel Activation failed >*/
	PPE_TUN_RET_CUSTOM_TUN_FREE_FAILED,		/**< Tunnel Free failed >*/
	PPE_TUN_RET_CUSTOM_TUN_DECONFIGURE_FAILED,	/**< Tunnel Deconfiguration failed >*/
	PPE_TUN_RET_CUSTOM_TUN_DEACTIVATE_FAILED,	/**< Tunnel Deactivation failed >*/
} ppe_tun_ret_t;

/*
 * ppe_tun_custom_tun_vlan
 *      PPE custom L2/L3 tunnel VLAN information
 */
struct ppe_tun_custom_tun_vlan {
	uint16_t cvlan_tpid;		/**< C-VLAN tag protocol identifier >*/
	uint16_t cvlan_tci;		/**< C-VLAN tag control information field >*/
	uint16_t svlan_tpid;		/**< S-VLAN tag protocol identifier >*/
	uint16_t svlan_tci;		/**< S-VLAN tag control information field >*/
	bool	 svlan_valid;		/**< S-VLAN tag valid >*/
};

/*
 * ppe_tun_custom_tun_pppoe
 *      PPE custom L2/L3 tunnel PPPoE information
 */
struct ppe_tun_custom_tun_pppoe {
	struct pppoe_hdr ph;			/**< PPPoE header >*/
	uint16_t ppp_protocol;			/**< PPP protocol field value >*/
	uint8_t pppoe_server_mac[ETH_ALEN];	/**< PPPoE server MAC address >*/
};

/*
 * ppe_tun_custom_tun_l2
 *     PPE custom L2 header information
 */
struct ppe_tun_custom_tun_l2 {
	uint16_t smac[3];			/**< Source MAC address >*/
	uint16_t dmac[3];			/**< Destination MAC address >*/
	uint16_t eth_type;			/**< Ether type >*/
	char	 phy_port[IFNAMSIZ];		/**< Phy port on which tunnel was created ex: eth0>*/
	struct	ppe_tun_custom_tun_vlan vlan;	/**< Tunnel outer header vlan configuration >*/
	struct	ppe_tun_custom_tun_pppoe pppoe;	/**< Tunnel outer header pppoe configuration >*/
	bool     vlan_enabled;			/**< Tunnel header vlan valid flag >*/
	bool	pppoe_enabled;			/**< Tunnel header PPPoE valid flag >*/
};

/*
 * ppe_tun_l3_hdr_type
 *      PPE custom tunnel L3 header type
 */
enum ppe_tun_l3_hdr_type{
	PPE_TUN_L3_HDR_TYPE_IPV4,
	PPE_TUN_L3_HDR_TYPE_IPV6,
};

/*
 * ppe_tun_custom_tun_l3
 *      PPE custom tunnel L3 information
 */
struct ppe_tun_custom_tun_l3 {
	enum ppe_tun_l3_hdr_type l3_type;	/**< L3 header type 0 - IPv4, 1 - IPv6 >*/
	uint32_t saddr[4];			/**< Source address. Only index 0 is valid for IPv4 >*/
	uint32_t daddr[4];			/**< Destination address. Only index 0 is valid for IPv4 >*/
	uint16_t proto;				/**< IP protocol >*/
};

/*
 * ppe_tun_encap_proto_update
 *      PPE custom tunnel encap protocol information
 *
 * Note on proto_offset: the offset is measured from the start of the
 * Ethernet header of the *encapsulated* (outer) packet as it appears in
 * hardware after L2 encapsulation.  If the tunnel carries a VLAN or PPPoE
 * header, those headers are already present in the encap_hdr_data[] blob
 * and the offset must account for them.
 */
struct ppe_tun_encap_proto_update {
	uint16_t ipv4_proto;			/**< 16 bit protocol value to be updated if inner packet is IPv4 >*/
	uint16_t ipv6_proto;			/**< 16 bit protocol value to be updated if inner packet is IPv6 >*/
	bool proto_update_en;			/**< Enable protocol field update >*/
	uint8_t proto_offset;			/**< Byte offset from start of outer Ethernet header to the protocol field to be updated. >*/
	uint8_t proto_size;			/**< Width of the field in bits to be updated. Max 16 bits. >*/
};

/*
 * ppe_tun_encap_ecn_mode
 *      PPE custom tunnel encap ECN mode
 */
enum ppe_tun_encap_ecn_mode {
	PPE_TUN_ENCAP_ECN_MODE_NO_UPDATE,			 	/**< no update ecn >*/
	PPE_TUN_ENCAP_ECN_MODE_RFC3168_LIMIT_RFC6040_CMPAT_MODE,	/**< RFC3168 limitation mode
									  RFC6040 compatibility mode for encapsulation >*/
	PPE_TUN_ENCAP_ECN_MODE_RFC3168_FULL_MODE,		 	/**< RFC3168 full mode for encapsulation >*/
	PPE_TUN_ENCAP_ECN_MODE_RFC4301_RFC6040_NORMAL_MODE,	 	/**< RFC4301 mode and RFC6040 normal mode for encapsulation >*/
};

#define PPE_TUN_ENCAP_HDR_DATA_LEN 128		/**< Custom tunnel encap header data length >*/

/*
 * ppe_tun_custom_tun_encap
 *      PPE custom tunnel encap configuration
 */
struct ppe_tun_custom_tun_encap {
	bool	copy_ttl;			/**< Copy outer TTL from Inner packet TTL value >*/
	uint8_t ttl;				/**< Fixed TTL value. Valid only if copy_ttl is set to 0(false) >*/
	bool	copy_dscp;			/**< Copy Outer DSCP from Inner packet DSCP fields >*/
	uint8_t dscp;				/**< Fixed DSCP value. valid only if copy_dscp is set to 0(false) >*/
	bool	copy_df_bit;			/**< Copy DF bit from inner packet header >*/
	bool	df_bit;				/**< Fixed DF bit value. valid id copy_df_bit is set to 0(false) >*/
	bool udp_checksum_en;			/**< Enable udp checksum computation in hardware >*/
	uint8_t encap_hdr_data[PPE_TUN_ENCAP_HDR_DATA_LEN];	/**< Fixed encap header data to be added to every packet >*/
	uint8_t encap_hdr_data_len;		/**< Fixed encap header data length >*/
	struct ppe_tun_encap_proto_update proto_update;	/**< Encap Protocol field update configuration >*/
	enum ppe_tun_encap_ecn_mode ecn_mode;	/**< Encap ECN mode configuration >*/
};

/*
 * ppe_tun_inner_type
 *      PPE custom tunnel inner type
 */
enum ppe_tun_inner_type {
	PPE_TUN_INNER_TYPE_ETHER = 0,		/**< Inner packet type Ethernet >*/
	PPE_TUN_INNER_TYPE_IP,			/**< Inner packet type IP >*/
};

/*
 * ppe_tun_inner_pkt_type
 *      PPE custom tunnel inner packet type for program UDF matching
 */
enum ppe_tun_inner_pkt_type {
	PPE_TUN_INNER_PKT_TYPE_ETHERNET = 0,	/**< Inner packet is Ethernet >*/
	PPE_TUN_INNER_PKT_TYPE_IPV4 = 1,	/**< Inner packet is IPv4 >*/
	PPE_TUN_INNER_PKT_TYPE_IPV6 = 2,	/**< Inner packet is IPv6 >*/
};

#define PPE_TUN_PRGM_UDF_MAX_ENTRIES 16		/**< Maximum program UDF entries per parser >*/

/*
 * ppe_tun_custom_tun_decap_type_match_entry
 *      PPE custom tunnel decap type match entry (program UDF)
 */
struct ppe_tun_custom_tun_decap_type_match_entry {
	enum ppe_tun_inner_pkt_type inner_pkt_type;	/**< Inner packet type for this entry >*/
	uint16_t udf0_val;				/**< User defined field 0 value to match >*/
	uint16_t udf0_mask;				/**< User defined field 0 mask to match upto 16 bits >*/
	uint16_t udf1_val;				/**< User defined field 1 value to match >*/
	uint16_t udf1_mask;				/**< User defined field 1 mask to match upto 16 bits >*/
	uint16_t udf2_val;				/**< User defined field 2 value to match >*/
	uint16_t udf2_mask;				/**< User defined field 2 mask to match upto 16 bits >*/
	bool udf0_en;					/**< User defined field 0 check enable >*/
	bool udf1_en;					/**< User defined field 1 check enable >*/
	bool udf2_en;					/**< User defined field 2 check enable >*/
};

/*
 * ppe_tun_custom_tun_decap_type_match
 *      PPE custom tunnel decap type match configuration (program UDF entries)
 */
struct ppe_tun_custom_tun_decap_type_match {
	uint8_t num_entries;						/**< Number of type match entries (1-16) >*/
	struct ppe_tun_custom_tun_decap_type_match_entry entries[PPE_TUN_PRGM_UDF_MAX_ENTRIES];	/**< Type match entries array >*/
};

/*
 * ppe_tun_custom_tun_decap_instance_match
 *      PPE custom tunnel decap instance match configuration (tunnel UDF)
 */
struct ppe_tun_custom_tun_decap_instance_match {
	uint16_t instance_udf0_val;	/**< Instance UDF 0 value to match >*/
	uint16_t instance_udf0_mask;	/**< Instance UDF 0 mask for matching >*/
	uint16_t instance_udf1_val;	/**< Instance UDF 1 value to match >*/
	uint16_t instance_udf1_mask;	/**< Instance UDF 1 mask for matching >*/
	bool instance_udf0_en;		/**< Instance UDF 0 check enable (formerly udf3) >*/
	bool instance_udf1_en;		/**< Instance UDF 1 check enable (formerly udf4) >*/
};

/*
 * ppe_tun_custom_tun_dcap_exp_mode
 *      PPE custom tunnel decap exception mode
 */
enum ppe_tun_custom_tun_dcap_exp_mode {
	PPE_TUN_DECAP_EXP_MODE_FULL_PKT,	/**< Full packet with tunnel header is exceptioned on underlying WAN/LAN port netdevice >*/
	PPE_TUN_DECAP_EXP_MODE_DECAP_PKT,	/**< Decapsulated packet is exceptioned, Needs a callback registered to handle decapsulated pkts >*/
};

/*
 * ppe_tun_custom_tun_decap_tun_identifier
 *      PPE custom tunnel decap tunnel identifier to use
 */
enum ppe_tun_custom_tun_decap_tun_identifier {
	PPE_TUN_DECAP_TUN_IDENTIFIER_ETHERNET_PROTO,	/**< Ethernet Protocol field for tunnel identification >*/
	PPE_TUN_DECAP_TUN_IDENTIFIER_IP_PROTO,		/**< IP protocol field for tunnel identification >*/
	PPE_TUN_DECAP_TUN_IDENTIFIER_UDP_PORT,		/**< UDP port information for tunnel identification >*/
};

/*
 * ppe_tun_custom_tun_dcap_udf_offset_type
 *      PPE custom tunnel decap User Defined field offset type
 */
enum ppe_tun_custom_tun_dcap_udf_offset_type {
	PPE_TUN_CUSTOM_TUN_DECAP_UDF_OFFSET_L2_START,	/**< UDF offsets are from start of L2/ETH header >*/
	PPE_TUN_CUSTOM_TUN_DECAP_UDF_OFFSET_L3_START,	/**< UDF offsets are from start of L3/IP header >*/
	PPE_TUN_CUSTOM_TUN_DECAP_UDF_OFFSET_L4_START,	/**< UDF offsets are from start of L4/UDP header >*/
};


/*
 * ppe_tun_custom_tun_decap_control
 *      PPE custom tunnel decap control information
 */
struct ppe_tun_custom_tun_decap_control {
	uint8_t sip_match : 1;		/**< Match source IP address for tunnel >*/
	uint8_t dip_match : 1;		/**< Match destination IP address for tunnel >*/
	uint8_t proto_match : 1;	/**< Match IP protocol field >*/
	uint8_t sport_match : 1;	/**< Match L4 source port values >*/
	uint8_t dport_match : 1;	/**< Match L4 destination port values >*/
};

/*
 * ppe_tun_custom_tun_decap_ecn_mode
 *      PPE custom tunnel decap ECN mode
 */
enum ppe_tun_custom_tun_decap_ecn_mode {
	PPE_DRV_TUN_CUSTOM_TUN_DECAP_ECN_RFC3168_MODE = 0,	/**< RFC3168 mode for decapsulation >*/
	PPE_DRV_TUN_CUSTOM_TUN_DECAP_ECN_RFC4301_MODE = 1,	/**< RFC4301 mode for decapsulation >*/
	PPE_DRV_TUN_CUSTOM_TUN_DECAP_ECN_RFC6040_MODE = 2,	/**< RFC6040 mode for decapsulation >*/
};

/*
 * ppe_tun_custom_tun_decap_udf_offset
 *      PPE custom tunnel decap UDF offsets
 */
struct ppe_tun_custom_tun_decap_udf_offset {
	uint8_t udf0_offset;		/**< User Defined field 0 offset (type match) >*/
	uint8_t udf1_offset;		/**< User Defined field 1 offset (type match) >*/
	uint8_t udf2_offset;		/**< User Defined field 2 offset (type match) >*/
	uint8_t instance_udf0_offset;	/**< Instance UDF 0 offset >*/
	uint8_t instance_udf1_offset;	/**< Instance UDF 1 offset >*/
};

/*
 * ppe_tun_custom_tun_decap
 *     PPE custom tunnel decap configurations
 */
struct ppe_tun_custom_tun_decap {
	enum ppe_tun_custom_tun_decap_tun_identifier id;		/**< Tunnel identifier type >*/
	enum ppe_tun_custom_tun_dcap_exp_mode dcap_exp_mode;		/**< Decap exception mode >*/
	enum ppe_tun_custom_tun_dcap_udf_offset_type offset_type;	/**< Decap offset type >*/
	enum ppe_tun_custom_tun_decap_ecn_mode dcap_ecn;		/**< Decap ECN mode >*/
	struct ppe_tun_custom_tun_decap_type_match type_match;		/**< Type match configuration (program UDF entries for tunnel type identification) >*/
	struct ppe_tun_custom_tun_decap_instance_match instance_match;	/**< Instance match configuration (tunnel UDF for instance identification) >*/
	struct ppe_tun_custom_tun_decap_udf_offset udf_offset;		/**< Decap udf offset configurations >*/
	bool udp_csum_zero_en;						/**< UDP checksum zero enable >*/
	uint8_t outer_hdr_length;					/**< Tunnel outer header length (offset to inner header in bytes) >*/
	struct ppe_tun_custom_tun_decap_control ctrl;			/**< Tunnel decap match control >*/
};

/*
 * ppe_tun_custom_tun
 *      PPE custom tunnel structure
 */
struct ppe_tun_custom_tun {
	ppe_tun_custom_exp_method_t excp_cb;	/**< Exception method to handle exceptioned packets >*/
	ppe_tun_custom_stats_method_t stats_cb;	/**< Exception method to handle stats update >*/
	void *tunnel_data;			/**< Tunnel data pointer which needs to be passed with callbacks >*/
	struct ppe_tun_custom_tun_decap dcap;	/**< Decap configurations for User defined tunnel type >*/
	struct ppe_tun_custom_tun_encap ecap;	/**< Encap configurations for User defined tunnel type >*/
	struct ppe_tun_custom_tun_l3 l3;	/**< L3 header configurations >*/
	enum ppe_tun_inner_type	inner_type;	/**< Tunnel Inner packet type >*/
	struct udphdr udph;			/**< UDP header configurations valid only if udp_hdr_valid flag is set >*/
	struct ppe_tun_custom_tun_l2 l2;	/**< L2 Header configurations >*/
	bool udp_hdr_valid;			/**< Outer UDP header valid flag >*/
};

int16_t ppe_tun_hybrid_ol_ctx_get(struct net_device *dev);
ppe_tun_ret_t ppe_tun_destroy_custom_tunnel(struct net_device *dev);
ppe_tun_ret_t ppe_tun_create_custom_tunnel(struct net_device *dev, struct ppe_tun_custom_tun *ctun);

#endif /* _PPE_TUN_H_ */
