/*
 **************************************************************************
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/**
 * @file netfn_types.h
 *	Netfn definitions.
 */

#include <linux/types.h>
#include<linux/in.h>
#include<linux/in6.h>

#ifndef __NETFN_TYPES_H
#define __NETFN_TYPES_H

/**
 * netfn_tuple_type
 *	Tuple type.
 */
typedef enum netfn_tuple_type {
	NETFN_TUPLE_3TUPLE,		/**< 3-tuple is valid */
	NETFN_TUPLE_4TUPLE,		/**< 4-tuple is valid */
	NETFN_TUPLE_5TUPLE,		/**< 5-tuple is valid */
} netfn_tuple_type_t;

/*
 * netfn_flowmgr_tuple_ip_version
 *	IP version inside a tuple.
 */
enum netfn_flowmgr_tuple_ip_version {
	NETFN_FLOWMGR_TUPLE_IP_VERSION_V4 = 4,		/**< IP version v4 >*/
	NETFN_FLOWMGR_TUPLE_IP_VERSION_V6 = 6,		/**< IP version v6 >*/
};

/**
 * netfn_l4_ident_type
 *	four tuple valid field
 */
typedef enum netfn_l4_ident_type {
	NETFN_4TUPLE_VALID_SRC_PORT = 1,	/**< Src Port is valid in 4tuple */
	NETFN_4TUPLE_VALID_DST_PORT,		/**< Dest Port is valid in 4tuple */
} netfn_l4_ident_type_t;

/**
 * netfn_tuple_3tuple
 *	three tuples structure
 */
typedef struct netfn_tuple_3tuple {
	union {
		struct in_addr ip4;	/**< Source IPv4 address */
		struct in6_addr ip6;	/**< Source IPv6 address */
	} src_ip;
	union {
		struct in_addr ip4;	/**< Dest IPv4 address */
		struct in6_addr ip6;	/**< Dest IPv6 address */
	} dest_ip;
	uint8_t protocol;		/**< Protocol*/
} netfn_tuple_type_3tuple_t;

/**
 * netfn_tuple_4tuple
 *	four tuples structure
 */
typedef struct netfn_tuple_4tuple {
	netfn_l4_ident_type_t ident_type;	/**< valid ident in 4 tuple */
	union {
		struct in_addr ip4;	/**< Source IPv4 address */
		struct in6_addr ip6;	/**< Source IPv6 address */
	} src_ip;
	union {
		struct in_addr ip4;	/**< Dest IPv4 address */
		struct in6_addr ip6;	/**< Dest IPv6 address */
	} dest_ip;
	__be16 l4_ident;		/**< Valid port - src port or dest port */
	uint8_t protocol;		/**< Protocol */
} netfn_tuple_type_4tuple_t;

/**
 * netfn_tuple_5tuple
 *	five tuples structure
 */
typedef struct netfn_tuple_5tuple {
	union {
		struct in_addr ip4;	/**< Source IPv4 address */
		struct in6_addr ip6;	/**< Source IPv6 address */
	} src_ip;
	union {
		struct in_addr ip4;	/**< Destination IPv4 address */
		struct in6_addr ip6;	/**< Destination IPv6 address */
	} dest_ip;
	__be16 l4_src_ident;		/**< Source L4 port, e.g., TCP or UDP port */
	__be16 l4_dest_ident;		/**< Destination L4 port, e.g., TCP or UDP port */
	uint8_t protocol;		/**< Protocol */
} netfn_tuple_type_5tuple_t;

/**
 * netfn_tuple
 *	Flow tuple info for IPv4 and IPv6.
 */
typedef struct netfn_tuple {
	uint8_t ip_version;			/**< IPv4 or IPv6 */
	netfn_tuple_type_t tuple_type;		/**< Valid tuple type */
	union {
		struct netfn_tuple_3tuple tuple_3;
		struct netfn_tuple_4tuple tuple_4;
		struct netfn_tuple_5tuple tuple_5;
	} tuples;
} netfn_tuple_t;

#endif /* __NETFN_TYPES_H */
