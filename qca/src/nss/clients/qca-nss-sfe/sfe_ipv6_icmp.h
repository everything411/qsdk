/*
 * sfe_ipv6_icmp.h
 *	Shortcut forwarding engine header file for IPv6 ICMP
 *
 * Copyright (c) 2015-2016, 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

int sfe_ipv6_recv_icmp(struct sfe_ipv6 *si, struct sk_buff *skb, struct net_device *dev,
			      unsigned int len, struct ipv6hdr *iph, unsigned int ihl, struct sfe_l2_info *l2_info);
