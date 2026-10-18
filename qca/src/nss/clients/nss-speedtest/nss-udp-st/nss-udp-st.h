/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#ifndef __NSS_UDP_ST_H
#define __NSS_UDP_ST_H

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdatomic.h>
#include "nss_udp_st_drv.h"

/*
 * NSS UDP speedtest path parameters
 */
#define NSS_UDP_ST_LOG "/tmp/nss-udp-st"
#define NSS_UDP_ST_TX_STATS "/tmp/nss-udp-st/tx_stats"
#define NSS_UDP_ST_RX_STATS "/tmp/nss-udp-st/rx_stats"
#define NSS_UDP_ST_RULES "/tmp/nss-udp-st/rules"

/*
 * nss_udp_st_cfg
 *  NSS UDP speedtest common parameters
 */
struct nss_udp_st_cfg {
	int time;                       /* time for speedtest */
	int handle;                     /* handle for NSS_UDP_ST_DEV */
	int type;                       /* type ( tx/rx ) */
	char mode[NSS_UDP_ST_MODESZ];   /* mode for speedtest */
};

/*
 * nss_udp_st_pkt_stats
 *  packet stats
 */
struct nss_udp_st_pkt_stats {
	atomic_llong tx_packets;	/* Number of packets transmitted */
	atomic_llong tx_bytes;		/* Number of bytes transmitted */
	atomic_llong rx_packets;	/* Number of packets received */
	atomic_llong rx_bytes;		/* Number of bytes received */
	atomic_llong ooo;		/* Out of order packets */
	atomic_llong dropped;		/* Dropped packets */
	atomic_llong max_latency;	/* Max Packet Delay */
	atomic_llong min_latency;	/* Min Packet Delay */
};

/*
 * nss_udp_st_ppe_stats
 *  PPE hardware flow stats (direction-neutral; TX and RX never run simultaneously)
 */
struct nss_udp_st_ppe_stats {
	atomic_llong ppe_packets;	/* PPE flow packets (cumulative) */
	atomic_llong ppe_bytes;		/* PPE flow bytes (cumulative) */
	atomic_llong ppe_pkts_per_sec;	/* PPE packets per second */
	atomic_llong ppe_bytes_per_sec;	/* PPE bytes per second (wire-rate) */
	atomic_llong ppe_sample_count;	/* Number of 1-second samples collected */
};

/*
 * nss_udp_st_stat
 *  stats for tx/rx test
 */
struct nss_udp_st_stat {
	struct nss_udp_st_pkt_stats p_stats;			/* Packet statistics */
	struct nss_udp_st_ppe_stats ppe_stats;			/* PPE hardware flow statistics */
	atomic_llong timer_stats[NSS_UDP_ST_STATS_TIME_MAX];	/* Time statistics */
	atomic_llong errors[NSS_UDP_ST_ERROR_MAX];		/* Error statistics */
	atomic_llong total_latency;				/* Total Latency */
	bool first_pkt;						/* First packet flag */
};

#endif /*__NSS_UDP_ST_H*/
