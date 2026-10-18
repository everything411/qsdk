/*
 * sfe.h
 *	Shortcut forwarding engine.
 *
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __SFE_H
#define __SFE_H

/*
 * Maximum number of accelerated IPv4 or IPv6 connections.
 * SFE_MAX_CONNECTION_NUM can be overridden at build time by passing
 * -DSFE_MAX_CONNECTION_NUM=<value> via the Makefile. It must be a power of 2.
 * If not explicitly defined, it falls back to a profile-based default.
 */
#ifndef SFE_MAX_CONNECTION_NUM
#if defined(SFE_MEM_PROFILE_LOW)
#define SFE_MAX_CONNECTION_NUM 512
#elif defined(SFE_MEM_PROFILE_MEDIUM)
#define SFE_MAX_CONNECTION_NUM 2048
#else
#define SFE_MAX_CONNECTION_NUM 4096
#endif
#endif /* SFE_MAX_CONNECTION_NUM */

#define SFE_L2_PARSE_FLAGS_PPPOE_INGRESS 0x01		/* Indicates presence of a valid PPPoE header */
#define SFE_L2_PARSE_FLAGS_TRUSTSEC_INGRESS 0x02	/* Indicates presence of a valid trustsec header */
#define SFE_L2_PARSE_FLAGS_VLAN_HW_TAG_SET 0x04		/* Indicates presence of a skb tag set by linux */
#define SFE_L2_PARSE_FLAGS_VLAN_LINUX_UNTAGGED 0x08	/* Indicates vlan dev in skb set by linunx */

/*
 * Bit valid flags for RFS connection match, common for IPv4/IPv6.
 */
#define SFE_CONN_MATCH_RFS_VALID_FLAG_RFS_ENABLED			0x01
		/* Tells if RFS is enabled for this connection match direction. */
#define SFE_CONN_MATCH_RFS_VALID_FLAG_TUNNEL_OUTER_FLOW		0x02
		/* Tells if this connection match is an entry of tunnel outer flow. */
#define SFE_CONN_MATCH_RFS_VALID_FLAG_TUNNEL_FLOW			0x04
		/* Tells if this connection match is an entry of tunnel flow. */

/*
 * IPv6 address structure
 */
struct sfe_ipv6_addr {
	__be32 addr[4];
};

typedef union {
	__be32			ip;
	struct sfe_ipv6_addr	ip6[1];
} sfe_ip_addr_t;

/*
 * VLAN header (aka VLAN tag)
 */
struct sfe_vlan_hdr {
	u16 tpid;               /* Tag Protocol Identifier */
	u16 tci;                /* Tag Control Information */
};

/*
 * trustsec header
 */
struct sfe_trustsec_hdr {
	u16 sgt;		/* Tag Protocol Identifier */
};

/*
 * Structure used to store L2 information
 */
struct sfe_l2_info {
	u16 parse_flags;	/* Flags indicating L2.5 headers presence */
	u16 pppoe_session_id;	/* PPPOE header offset */
	u16 protocol;		/* L3 Protocol */
	u16 trimmed_len;	/* Trimmed length in the receiving path*/
	u16 trustsec_sgt;	/* trustsec SGT value */
	struct sfe_vlan_hdr vlan_hdr[SFE_MAX_VLAN_DEPTH];
				/* VLAN tag(s) of ingress packet */
	u8 vlan_hdr_cnt;        /* Number of VLAN tags in the ingress packet */
};

/*
 * Structure used to sync connection stats/state back within the system.
 *
 * NOTE: The addresses here are NON-NAT addresses, i.e. the true endpoint addressing.
 * 'src' is the creator of the connection.
 */
struct sfe_connection_sync {
	struct net_device *src_dev;
	struct net_device *dest_dev;
	int is_v6;			/* Is it for ipv6? */
	int protocol;			/* IP protocol number (IPPROTO_...) */
	sfe_ip_addr_t src_ip;		/* Non-NAT source address, i.e. the creator of the connection */
	sfe_ip_addr_t src_ip_xlate;	/* NATed source address */
	__be16 src_port;		/* Non-NAT source port */
	__be16 src_port_xlate;		/* NATed source port */
	sfe_ip_addr_t dest_ip;		/* Non-NAT destination address, i.e. to whom the connection was created */
	sfe_ip_addr_t dest_ip_xlate;	/* NATed destination address */
	__be16 dest_port;		/* Non-NAT destination port */
	__be16 dest_port_xlate;		/* NATed destination port */
	u32 src_td_max_window;
	u32 src_td_end;
	u32 src_td_max_end;
	u64 src_packet_count;
	u64 src_byte_count;
	u32 src_new_packet_count;
	u32 src_new_byte_count;
	u32 dest_td_max_window;
	u32 dest_td_end;
	u32 dest_td_max_end;
	u64 dest_packet_count;
	u64 dest_byte_count;
	u32 dest_new_packet_count;
	u32 dest_new_byte_count;
	u32 reason;                     /* reason for stats sync message, i.e. destroy, flush, period sync */
	u64 delta_jiffies;		/* Time to be added to the current timeout to keep the connection alive */
	u32 flow_rule_id;		/* Unique number defined in connection manager */
};

extern uint8_t sfe_fls_flush_on_hwm_enable;

struct sfe_fls {
	sfe_fls_conn_create_t create_cb;
	sfe_fls_conn_delete_t delete_cb;
	sfe_fls_conn_stats_update_t stats_update_cb;
};

typedef enum sfe_response {
	SFE_RET_SUCCESS,
	SFE_RET_IPPROTO_NON_UDP,
	SFE_RET_SRC_DEV_NOT_FOUND,
	SFE_RET_DEST_DEV_NOT_FOUND,
	SFE_RET_SRC_DEV_AND_DEST_DEV_UNREGISTERED,
	SFE_RET_CONN_ENTRY_MEM_ALLOC_FAILED,
	SFE_RET_CONN_MATCH_ENTRY_MEM_ALLOC_FAILED,
	SFE_RET_MAX_CONN_COUNT_REACHED,
	SFE_RET_CONN_ALREADY_EXISTS,
	SFE_RET_BOTTOM_INTERFACE_NOT_SET,
	SFE_RET_MORE_VLAN_AND_VLAN_FILTER_RULES_NOT_ALLOWED,
	SFE_RET_UNABLE_TO_TAKE_REF_FOR_SOCKET,
	SFE_RET_GRE_PROTO_HANDLER_NOT_REGISTERED,
	SFE_RET_L2TP_PROTO_HANDLER_NOT_REGISTERED,
	SFE_RET_TOP_INTERFACE_DEV_NOT_FOUND,
	SFE_RET_ESP_PROTO_HANDLER_NOT_REGISTERED,
	SFE_RET_DST_INIT_FOR_FLOW_MATCH_FAILED,
	SFE_RET_DST_INIT_FOR_RETURN_FLOW_MATCH_FAILED,
	SFE_RET_ETHERIP_PROTO_HANDLER_NOT_REGISTERED,
	SFE_RET_EXCEPTION_CONNECTION_INVALID,
	SFE_RET_EXCEPTION_TCP_INVALID,
	SFE_RET_EXCEPTION_PROTOCOL_NOT_SUPPORT,
	SFE_RET_EXCEPTION_NOT_SUPPORT_BRIDGE,
	SFE_RET_EXCEPTION_SRC_DEV_NOT_L3,
	SFE_RET_EXCEPTION_CFG_ERR,
	SFE_RET_EXCEPTION_DEST_DEV_NOT_L3,

	/* Tx command failure */
	SFE_RET_TX_FAILURE,
	SFE_RET_EXCEPTION_ENQUEUE_FAILED,
} sfe_response_t;

extern struct sfe_fls sfe_fls_info;

/*
 * Expose the hook for the receive processing.
 */
extern int (*athrs_fast_nat_recv)(struct sk_buff *skb);

/*
 * Expose what should be a static flag in the TCP connection tracker.
 */
extern int nf_ct_tcp_no_window_check;

/*
 * Check the fast transmit feasibility.
 */
bool sfe_fast_xmit_check(struct sk_buff *skb, netdev_features_t features);

/*
 * sfe_is_ppe_rfs_feature_enabled()
 */
#if defined(SFE_RFS_SUPPORTED)
bool sfe_is_ppe_rfs_feature_enabled(void);
#endif

/*
 * sfe_is_spi_based_esp_passthrough_enabled()
 */
#if defined(SFE_ESP_SPI_PASSTH_ENABLE)
bool sfe_is_spi_based_esp_passthrough_enabled(void);
#endif

/*
 * This callback will be called in a timer
 * at 100 times per second to sync stats back to
 * Linux connection track.
 *
 * A RCU lock is taken to prevent this callback
 * from unregistering.
 */
typedef void (*sfe_sync_rule_callback_t)(struct sfe_connection_sync *);
typedef void (*sfe_ipv4_many_sync_callback_t)(struct sfe_ipv4_msg *msg);
typedef void (*sfe_ipv6_many_sync_callback_t)(struct sfe_ipv6_msg *msg);

/*
 * IPv4 APIs used by connection manager
 */
int sfe_ipv4_recv(struct net_device *dev, struct sk_buff *skb, struct sfe_l2_info *l2_info, bool tun_outer);
sfe_response_t sfe_ipv4_create_rule(struct sfe_ipv4_rule_create_msg *msg);
void sfe_ipv4_destroy_rule(struct sfe_ipv4_rule_destroy_msg *msg);
void sfe_ipv4_destroy_all_rules_for_dev(struct net_device *dev);
sfe_response_t sfe_ipv4_create_mc_rule(struct sfe_ipv4_mc_rule_create_msg *msg);
void sfe_ipv4_destroy_mc_rule(struct sfe_ipv4_mc_rule_destroy_msg *msg);
void sfe_ipv4_register_sync_rule_callback(sfe_sync_rule_callback_t callback);
bool sfe_ipv4_rule_update(struct sfe_rule_update_msg *update_msg);
bool sfe_dev_has_hw_csum(struct net_device *dev);
bool sfe_dev_is_ipip6(struct net_device *dev);

bool sfe_ipv4_sync_invoke(uint16_t index);
void sfe_ipv4_register_many_sync_callback(sfe_ipv4_many_sync_callback_t cb);
void sfe_ipv4_stats_convert(struct sfe_ipv4_conn_sync *sync_msg, struct sfe_connection_sync *sis);
#ifdef SFE_SUPPORT_IPV6
/*
 * IPv6 APIs used by connection manager
 */
int sfe_ipv6_recv(struct net_device *dev, struct sk_buff *skb, struct sfe_l2_info *l2_info, bool tun_outer);
sfe_response_t sfe_ipv6_create_rule(struct sfe_ipv6_rule_create_msg *msg);
void sfe_ipv6_destroy_rule(struct sfe_ipv6_rule_destroy_msg *msg);
void sfe_ipv6_destroy_all_rules_for_dev(struct net_device *dev);
sfe_response_t sfe_ipv6_create_mc_rule(struct sfe_ipv6_mc_rule_create_msg *msg);
void sfe_ipv6_destroy_mc_rule(struct sfe_ipv6_mc_rule_destroy_msg *msg);
void sfe_ipv6_register_sync_rule_callback(sfe_sync_rule_callback_t callback);
bool sfe_ipv6_rule_update(struct sfe_rule_update_msg *update_msg);
bool sfe_ipv6_sync_invoke(uint16_t index);
void sfe_ipv6_register_many_sync_callback(sfe_ipv6_many_sync_callback_t cb);
void sfe_ipv6_stats_convert(struct sfe_ipv6_conn_sync *sync_msg, struct sfe_connection_sync *sis);
#else
static inline int sfe_ipv6_recv(struct net_device *dev, struct sk_buff *skb, struct sfe_l2_info *l2_info, bool tun_outer)
{
	return 0;
}

static inline int sfe_ipv6_create_rule(struct sfe_ipv6_rule_create_msg *msg)
{
	return 0;
}

static inline void sfe_ipv6_destroy_rule(struct sfe_ipv6_rule_destroy_msg *msg)
{
	return;
}

static inline void sfe_ipv6_destroy_all_rules_for_dev(struct net_device *dev)
{
	return;
}

static inline int sfe_ipv6_create_mc_rule(struct sfe_ipv6_mc_rule_create_msg *msg)
{
	return 0;
}

static inline int sfe_ipv6_destroy_mc__rule(struct sfe_ipv6_mc_rule_destroy_msg *msg)
{
	return;
}

static inline void sfe_ipv6_register_sync_rule_callback(sfe_sync_rule_callback_t callback)
{
	return;
}

static inline bool sfe_ipv6_rule_update(struct sfe_rule_update_msg *msg)
{
	return false;
}

static inline bool sfe_ipv6_sync_invoke(uint16_t index)
{
	return false;
}

static inline void sfe_ipv6_register_many_sync_callback(sfe_ipv6_many_sync_callback_t cb)
{
	return;
}

static inline void sfe_ipv6_stats_convert(struct sfe_ipv6_conn_sync *sync_msg, struct sfe_connection_sync *sis)
{
	return;
}
#endif

/*
 * sfe_ipv6_addr_equal()
 *	compare ipv6 address
 *
 * return: 1, equal; 0, no equal
 */
static inline int sfe_ipv6_addr_equal(struct sfe_ipv6_addr *a,
				      struct sfe_ipv6_addr *b)
{
	return a->addr[0] == b->addr[0] &&
	       a->addr[1] == b->addr[1] &&
	       a->addr[2] == b->addr[2] &&
	       a->addr[3] == b->addr[3];
}

/*
 * sfe_ipv4_addr_equal()
 *	compare ipv4 address
 *
 * return: 1, equal; 0, no equal
 */
#define sfe_ipv4_addr_equal(a, b) ((u32)(a) == (u32)(b))

/*
 * sfe_addr_equal()
 *	compare ipv4 or ipv6 address
 *
 * return: 1, equal; 0, no equal
 */
static inline int sfe_addr_equal(sfe_ip_addr_t *a,
				 sfe_ip_addr_t *b, int is_v4)
{
	return is_v4 ? sfe_ipv4_addr_equal(a->ip, b->ip) : sfe_ipv6_addr_equal(a->ip6, b->ip6);
}

/*
 * sfe_l2_parse_flag_set()
 *	Set L2 parse flag
 */
static inline void sfe_l2_parse_flag_set(struct sfe_l2_info *l2_info, u16 flag)
{
	l2_info->parse_flags |= flag;
}

/*
 * sfe_l2_parse_flag_get()
 *	Get L2 parse flag
 */
static inline u16 sfe_l2_parse_flag_get(struct sfe_l2_info *l2_info)
{
	return l2_info->parse_flags;
}

/*
 * sfe_l2_parse_flag_check()
 *	Check L2 parse flag
 */
static inline bool sfe_l2_parse_flag_check(struct sfe_l2_info *l2_info, u16 flag)
{
	return !!(l2_info->parse_flags & flag);
}

/*
 * sfe_l2_pppoe_session_id_get()
 *	Get PPPPoE session ID from l2_info
 */
static inline u16 sfe_l2_pppoe_session_id_get(struct sfe_l2_info *l2_info)
{
	return l2_info->pppoe_session_id;
}

/*
 * sfe_l2_pppoe_session_id_set()
 *	Set PPPoE session ID to l2_info
 */
static inline void sfe_l2_pppoe_session_id_set(struct sfe_l2_info *l2_info, u16 session_id)
{
	l2_info->pppoe_session_id = session_id;
}

/*
 * sfe_l2_protocol_get()
 *	Get L2 protocol
 */
static inline u16 sfe_l2_protocol_get(struct sfe_l2_info *l2_info)
{
	return l2_info->protocol;
}

/*
 * sfe_l2_protocol_set()
 *	Set L2 protocol
 */
static inline void sfe_l2_protocol_set(struct sfe_l2_info *l2_info, u16 proto)
{
	l2_info->protocol = proto;
}

/*
 * sfe_l2_trustsec_sgt_get()
 *	Get trustsec SGT from l2_info
 */
static inline u16 sfe_l2_trustsec_sgt_get(struct sfe_l2_info *l2_info)
{
	return l2_info->trustsec_sgt;
}

/*
 * sfe_l2_trustsec_sgt_set()
 *	Set trustsec SGT to l2_info
 */
static inline void sfe_l2_trustsec_sgt_set(struct sfe_l2_info *l2_info, u16 sgt)
{
	l2_info->trustsec_sgt = sgt;
}

/*
 * sfe_dev_is_bridge()
 *	Check if the net device is any kind of bridge
 */
static inline bool sfe_dev_is_bridge(struct net_device *dev)
{
	if (!dev) {
		return false;
	}

	if ((dev->priv_flags & IFF_EBRIDGE) || (dev->priv_flags & IFF_OPENVSWITCH)) {
		return true;
	}

	return false;
}

void sfe_recv_undo_parse_l2(struct net_device *dev, struct sk_buff *skb, struct sfe_l2_info *l2_info);
bool sfe_tcp_checksum_validate(struct sk_buff *skb, unsigned int ihl, bool is_v4);
bool sfe_udp_checksum_validate(struct sk_buff *skb, unsigned int ihl, bool is_v4);

int sfe_init_if(void);
void sfe_exit_if(void);

#endif /* __SFE_H */
