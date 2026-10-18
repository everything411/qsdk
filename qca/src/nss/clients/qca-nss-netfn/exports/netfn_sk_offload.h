/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * @file netfn_sk_offload.h
 * Netfn Sock Offload Exports File.
 */
#ifndef __NETFN_SK_OFFLOAD_H
#define __NETFN_SK_OFFLOAD_H

/**
 * netfn_sk_offload_apps
 *	Socket Offload Apps.
 */
enum netfn_sk_offload_apps {
	NSS_SK_OFFLOAD_NONE	 = 0,	/**< No Offload */
	NSS_SK_OFFLOAD_IPERF_32  = 1,	/**< Offload option for IPERF 32 bit */
	NSS_SK_OFFLOAD_IPERF_64  = 2,	/**< Offload option for IPERF 64 bit */
	NSS_SK_OFFLOAD_MAX       = 3,	/**< Max limit */
};

#endif 	/* __NETFN_SK_OFFLOAD_H */
