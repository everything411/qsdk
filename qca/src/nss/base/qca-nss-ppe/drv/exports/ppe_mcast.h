/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_mcast.h
 *	NSS PPE Multicast definitions.
 */

#ifndef _PPE_MCAST_H_
#define _PPE_MCAST_H_

#include <linux/if.h>

/*
 * ppe_mcast_ret
 *	PPE Multicast return status types.
 */
typedef enum ppe_mcast_ret {
	PPE_MCAST_SUCCESS = 0,			/**< Success. */
	PPE_MCAST_MC_ENTRY_CREATE_FAIL,		/**< Failure in creation of multicast entry. */
	PPE_MCAST_MC_ENTRY_DELETE_FAIL,		/**< Failure in deletion of multicast entry. */
} ppe_mcast_ret_t;

/*
 * ppe_mcast_ip
 *	PPE multicast IPv4 and IPv6 address union.
 */
union ppe_mcast_ip {
	uint32_t v4;	/* IPv4 address */
	uint32_t v6[4];	/* IPv6 address */
};

/*
 * ppe_mcast_entry_info
 *	PPE multicast entry information.
 */
struct ppe_mcast_entry_info {
	char dev[IFNAMSIZ];		/* Device associated with multicast rule. */
	bool vlan_enabled;		/* Is VLAN enabled. */
	uint32_t vlan_id;		/* VLAN ID. */
	bool sip_enabled;		/* Is source IP of the sender enabled. */
	bool is_v4;			/* IPv4 or IPv6 entry. */
	union ppe_mcast_ip sip;		/* Source IP of the sender. */
	union ppe_mcast_ip gip;		/* Multicast group IP. */
};

/**
 * ppe_mcast_delete_entry
 *	Function to delete a multicast entry.
 *
 *
 * @param[in] info         PPE multicast entry information.
 * @return
 * PPE multicast request's return status.
 */
ppe_mcast_ret_t ppe_mcast_delete_entry(struct ppe_mcast_entry_info *info);

/**
 * ppe_mcast_create_entry
 *	Function to create a multicast entry.
 *
 * @param[in] info         PPE multicast entry information.
 * @return
 * PPE multicast request's return status.
 */
ppe_mcast_ret_t ppe_mcast_create_entry(struct ppe_mcast_entry_info *info);


#endif /* _PPE_MCAST_H_ */
