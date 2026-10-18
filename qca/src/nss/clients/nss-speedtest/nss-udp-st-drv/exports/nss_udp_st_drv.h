/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_UDP_ST_DRV_H
#define __NSS_UDP_ST_DRV_H

#ifdef __KERNEL__ /* only kernel will use. */
#include <linux/sizes.h>
#endif

#define NSS_UDP_ST_IFNAMSZ	24
#define NSS_UDP_ST_IPNAMSZ	40
#define NSS_UDP_ST_MODESZ	24

#ifdef __KERNEL__ /* only kernel will use. */
#define NSS_UDP_ST_FLAG_IPV4	0x1
#define NSS_UDP_ST_FLAG_IPV6	0x2
#endif

/*
 * NSS UDP speedtest ioctl parameters
 */
#define NSS_UDP_ST_IOCTL_MAGIC			'n'
#define NSS_UDP_ST_IOCTL_INIT			_IOW(NSS_UDP_ST_IOCTL_MAGIC, 0, struct nss_udp_st_param* )
#define NSS_UDP_ST_IOCTL_START_TX		_IOW(NSS_UDP_ST_IOCTL_MAGIC, 1, int)
#define NSS_UDP_ST_IOCTL_START_RX		_IOW(NSS_UDP_ST_IOCTL_MAGIC, 2, int)
#define NSS_UDP_ST_IOCTL_STOP			_IO(NSS_UDP_ST_IOCTL_MAGIC, 3)
#define NSS_UDP_ST_IOCTL_RATE_CHANGE		_IOW(NSS_UDP_ST_IOCTL_MAGIC, 4, uint32_t)
#define NSS_UDP_ST_IOCTL_RESET_STATS		_IO(NSS_UDP_ST_IOCTL_MAGIC, 5)
#define NSS_UDP_ST_IOCTL_FINAL			_IO(NSS_UDP_ST_IOCTL_MAGIC, 6)
#define NSS_UDP_ST_DEV				"/dev/nss_udp_st"

#define NSS_UDP_ST_FLAGS_TIMESTAMP 0x1	/* Flag to enable timestamp */
#define NSS_UDP_ST_FLAGS_VP 0x2		/* Flag to enable VP */
#define NSS_UDP_ST_FLAGS_HW_OFFLOAD 0x4	/* Flag to enable HW offload */

#ifdef __KERNEL__ /* only kernel will use. */
#define NSS_UDP_ST_MAX_HEADROOM 32	/* Maximum headroom needed */
#define NSS_UDP_ST_MAX_TAILROOM 32	/* Maximum tailroom needed */
#define NSS_UDP_ST_BUFFER_SIZE_MIN 64	/* 64 bytes */
#define NSS_UDP_ST_BUFFER_SIZE_MAX 1500	/* 1500 bytes */
#define NSS_UDP_ST_RATE_MAX 20000000000	/* 20 Gbps */

extern struct nss_udp_st nust;
extern struct delayed_work nss_udp_st_tx_delayed_work;
extern struct workqueue_struct *work_queue;
extern void nss_udp_st_update_stats(size_t pkt_size, int64_t num_pkts);
extern uint64_t nss_udp_st_tx_num_pkt;
extern struct net_device *nust_dev;
extern struct nss_udp_st_rules *exception_dport_rules;
extern uint8_t exception_rules_cnt;
extern bool is_dummy_vp_exists;
#endif

/*
 * nss_udp_st_rule
 *  NSS UDP speedtest rules parameters
 */
enum nss_udp_st_rule {
	NSS_UDP_ST_SIP,		/* source IP */
	NSS_UDP_ST_DIP,		/* destination IP */
	NSS_UDP_ST_SPORT,	/* source port */
	NSS_UDP_ST_DPORT,	/* destination port */
	NSS_UDP_ST_FLAGS,	/* IP version flag */
};

/*
 * nss_udp_st_stats_stats
 *	time stats
 */
enum nss_udp_st_stats_time {
	NSS_UDP_ST_STATS_TIME_START,	/* Start time of the test */
	NSS_UDP_ST_STATS_TIME_CURRENT,	/* Current time of the running test */
	NSS_UDP_ST_STATS_TIME_ELAPSED,	/* Elapsed time of the current test */
	NSS_UDP_ST_STATS_TIME_MAX	/* Maximum timer statistics type */
};

/*
 * nss_udp_st_error
 *	error stats
 */
enum nss_udp_st_error {
	NSS_UDP_ST_ERROR_NONE,			/* no error */
	NSS_UDP_ST_ERROR_INCORRECT_RATE,	/* incorrect rate */
	NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE,	/* incorrect buffer size */
	NSS_UDP_ST_ERROR_MEMORY_FAILURE,	/* Memory allocation failed */
	NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION,	/* Incorrect IP version */
	NSS_UDP_ST_ERROR_PACKET_DROP,		/* Packet Drop */
	NSS_UDP_ST_ERROR_MAX			/* Maximum error statistics type */
};

/*
 * nss_udp_st_type
 *	tx/rx flags
 */
enum nss_udp_st_type {
	NSS_UDP_ST_TX,	/* Tx 0 */
	NSS_UDP_ST_RX,	/* Rx 1 */
};

/*
 * nss_udp_st_param
 *  config parameters for Tx test
 */
struct nss_udp_st_param {
	uint32_t rate;		/* target rate in Mbps */
	uint32_t buffer_sz;	/* buffer size of each packet */
	uint32_t dscp;		/* dscp flag for tx packet */
	uint8_t flags;		/* flags to enable features without fields */
	uint32_t cpu_bitmap;	/* CPU bitmap to be used */
	uint16_t burst_size;	/* Burst Size */
	uint16_t timer_freq;	/* Timer frequency, calculcated based on burst */
	char net_dev[NSS_UDP_ST_IFNAMSZ];
				/* net device interface */
};

/*
 * nss_udp_st_opt
 *  5 tuple config parameters
 */
struct nss_udp_st_opt {
	uint16_t sport;			/* source port */
	uint16_t dport;			/* destination port */
	uint16_t ip_version;		/* ip version flag */
	char sip[NSS_UDP_ST_IPNAMSZ];	/* source ip string */
	char dip[NSS_UDP_ST_IPNAMSZ];	/* dest ip string */
};

#ifdef __KERNEL__ /* only kernel will use. */
/*
 * nss_udp_st_pppoe_info
 *	PPPoE header information
 */
struct nss_udp_st_pppoe_info {
	struct net_device *dev;			/* Base Net device */
	uint16_t pppoe_session_id;		/* PPPoE session ID on this interface */
	uint8_t remote_mac[ETH_ALEN];		/* MAC Address of the PPPoE concentrator */
};

/*
 * nss_udp_st_mode
 *  start/stop flags
 */
enum nss_udp_st_mode {
	NSS_UDP_ST_STOP,	/* Stop 0 */
	NSS_UDP_ST_START,	/* Start 1 */
};

/*
 * nss_udp_st_ip
 *  ipv4/ipv6 params
 */
struct nss_udp_st_ip {
	union {
		uint32_t ipv4;	/* IPv4 address. */
		uint32_t ipv6[4];	/* IPv6 address. */
	} ip;
};

/*
 * nss_udp_st_pkt_stats
 *  packet stats
 */
struct nss_udp_st_pkt_stats {
	atomic64_t tx_packets;	/* Number of packets transmitted */
	atomic64_t tx_bytes;		/* Number of bytes transmitted */
	atomic64_t rx_packets;	/* Number of packets received */
	atomic64_t rx_bytes;		/* Number of bytes received */
	atomic64_t ooo;			/* Out of order packets */
	atomic64_t dropped;		/* Dropped packets */
	atomic64_t max_latency;		/* Max Packet Delay */
	atomic64_t min_latency;		/* Min Packet Delay */
};

/*
 * nss_udp_st_ppe_stats
 *	PPE hardware flow stats (direction-neutral; TX and RX never run simultaneously)
 */
struct nss_udp_st_ppe_stats {
	atomic64_t ppe_packets;		/* PPE flow packets (cumulative) */
	atomic64_t ppe_bytes;		/* PPE flow bytes (cumulative) */
	atomic64_t ppe_pkts_per_sec;	/* PPE packets per second */
	atomic64_t ppe_bytes_per_sec;	/* PPE bytes per second (wire-rate) */
	atomic64_t ppe_sample_count;	/* Number of 1-second samples collected */
};

/*
 * nss_udp_st_stats
 *	stats for tx/rx test
 */
struct nss_udp_st_stats {
	struct nss_udp_st_pkt_stats p_stats;			/* Packet statistics */
	struct nss_udp_st_ppe_stats ppe_stats;			/* PPE hardware flow statistics */
	atomic64_t timer_stats[NSS_UDP_ST_STATS_TIME_MAX];	/* Time statistics */
	atomic64_t errors[NSS_UDP_ST_ERROR_MAX];		/* Error statistics */
	atomic64_t total_latency;				/* Total Latency */
	atomic64_t rx_exception_pkt_cnt;			/* Exception rx packet without RFS rule match */
	bool first_pkt;						/* First packet flag */
};

/*
 * nss_udp_st_timestamp_info
 *	contents of payload during timestamp test
 */
struct nss_udp_st_timestamp_info {
	uint64_t seq;		/* sequence number assigned to pkt by tx driver */
	uint64_t timestamp;	/* timestamp assigned to pkt by tx driver */
};

/*
 * nss_udp_st_rules
 *	config rules configured for tx/rx test
 */
struct nss_udp_st_rules {
	struct list_head list;		/* kernel's list structure */
	struct nss_udp_st_ip sip;	/* source ip */
	struct nss_udp_st_ip dip;	/* dest ip */
	uint16_t sport;			/* source port */
	uint16_t dport;			/* dest port */
	uint8_t ip_version;		/* version of IP address */
	uint8_t dst_mac[ETH_ALEN];	/* dest mac */
	uint8_t src_mac[ETH_ALEN];	/* source mac */
	uint64_t seq;			/* sequence counter */
	uint64_t seq_greatest;		/* greatest pkt recieved */
	uint8_t cpu;			/* CPU that this connection runs */
	struct net_device *tun_dev;	/* Tunnel device mapped to this connection */
	int32_t vp_num;			/* VP num this connection is mapped to */
	struct net_device *ppe_dev;	/* Device used for ppe vp */
	void *policer_ctx;		/* Per-flow policer context */
	uint32_t policer_rule_id;	/* Per-flow policer rule ID */
	uint64_t policer_prev_rpc;	/* Previous cumulative red-packet count for delta calculation */
	uint16_t bridge_vlan_id;	/* VLAN ID if bridge port is a VLAN device (0 if not VLAN) */
	uint32_t rule_id;		/* rule id of the rule */
};

/*
 * nss_udp_st
 *	config/rules/stats for tx/rx test
 */
struct nss_udp_st {
	struct nss_udp_st_param config;	/* config params for tx */
	struct nss_udp_st_rules rules;	/* database for config rules */
	struct nss_udp_st_stats stats;	/* result statistics */
	struct nss_udp_st_pppoe_info pppoe_info;	/* PPPoE session information */
	uint32_t rule_count;		/* no of rules configured */
	uint32_t time;			/* duration of test */
	uint32_t bitmap_curr;		/* temp variable to see assignment map */
	bool mode;			/* start =0; stop=1 */
	bool dir;			/* tx=0; rx=1 */
	atomic_t xmit_idx;		/* Index for EDMA descriptor assignment for HW offload */
	int32_t dummy_vp_num;		/* dummy vp for configuring RFS at rx */
};
#endif

#endif /*NSS_UDP_ST_H*/
