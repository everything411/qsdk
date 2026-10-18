/*
 **************************************************************************
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NSM_NL_FAM_H
#define __NSM_NL_FAM_H

/*
 * Name of the NSM netlink family.
 */
#define NSM_NL_NAME "NSM_NL_FAM"

/*
 * Precision of throughput calculation. Throughputs are
 * reported in terms of packets per NSM_NL_PREC seconds.
 * A value of 100 means throughput is packets per 100 seconds.
 */
#define NSM_NL_PREC 100

/*
 * nsm_nl_attr
 *	Attributes for the nsm_nl generic netlink family.
 */
enum nsm_nl_attr {
	NSM_NL_ATTR_UNSPEC,
	NSM_NL_ATTR_RX_PACKETS,
	NSM_NL_ATTR_RX_BYTES,
	NSM_NL_ATTR_SERVICE_ID,
	NSM_NL_ATTR_NET_DEVICE,
	NSM_NL_ATTR_LATENCY_MEAN,
	NSM_NL_ATTR_LATENCY_HIST0,
	NSM_NL_ATTR_LATENCY_HIST1,
	NSM_NL_ATTR_LATENCY_HIST2,
	NSM_NL_ATTR_LATENCY_HIST3,
	NSM_NL_ATTR_LATENCY_HIST4,
	NSM_NL_ATTR_LATENCY_HIST5,
	NSM_NL_ATTR_LATENCY_HIST6,
	NSM_NL_ATTR_LATENCY_HIST7,
	NSM_NL_ATTR_FLOW_IP_0,
	NSM_NL_ATTR_FLOW_IP_1,
	NSM_NL_ATTR_FLOW_IP_2,
	NSM_NL_ATTR_FLOW_IP_3,
	NSM_NL_ATTR_RETURN_IP_0,
	NSM_NL_ATTR_RETURN_IP_1,
	NSM_NL_ATTR_RETURN_IP_2,
	NSM_NL_ATTR_RETURN_IP_3,
	NSM_NL_ATTR_FLOW_PORT,
	NSM_NL_ATTR_RETURN_PORT,
	NSM_NL_ATTR_PROTOCOL,
	NSM_NL_ATTR_DROP_QUEUE_ID,
	NSM_NL_ATTR_PAD,
	NSM_NL_ATTR_MAX
};

/*
 * nsm_nl_cmd
 *	Commands for the nsm_nl generic netlink family.
 */
enum nsm_nl_cmd {
	NSM_NL_CMD_UNUSED,
	NSM_NL_CMD_GET_STATS,
	NSM_NL_CMD_GET_THROUGHPUT,
	NSM_NL_CMD_GET_LATENCY,
	NSM_NL_CMD_GET_PPE_SERVICE,
	NSM_NL_CMD_GET_V4_PPE_FLOW,
	NSM_NL_CMD_GET_V6_PPE_FLOW,
	NSM_NL_CMD_GET_PPE_QUEUE,
	NSM_NL_CMD_GET_PPE_DROP,
	NSM_NL_CMD_MAX
};

#endif
