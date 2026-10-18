/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv_mcast.h
 *	PPE QoS specific definitions.
 */

#ifndef _PPE_DRV_MCAST_H_
#define _PPE_DRV_MCAST_H_

/**
 * @addtogroup ppe_drv_mcast_subsystem
 * @{
 */

/**
 * ppe_drv_mcast_fwd_cmd
 *	Multicast forward command.
 */
typedef enum ppe_drv_mcast_fwd_cmd {
	PPE_DRV_MCAST_FWD_CMD_FWD,	/**< Forward command to allow regular forwarding. */
	PPE_DRV_MCAST_FWD_CMD_DROP,	/**< Forward command to drop matching packets. */
	PPE_DRV_MCAST_FWD_CMD_COPY,	/**< Forward command to copy the matching packets. */
	PPE_DRV_MCAST_FWD_CMD_REDIR	/**< Forward command to redirecting the matching packets. */
} ppe_drv_mcast_fwd_cmd_t;

typedef enum ppe_drv_mcast_fwd_cmd ppe_drv_mcast_fwd_cmd_t;

/*
 * ppe_drv_mcast_entry
 *	PPE multicast entry information
 */
struct ppe_drv_mcast_entry {
	bool is_v4;		/* Is IP v4? */
	bool sip_valid;		/* Source IP match valid? */
	union {
		uint32_t v4;	/* Source IPv4 address */
		uint32_t v6[4];	/* Source IPv6 address */
	} sip;			/* Union of IPv4 and IPv6 source IP address */

	union {
		uint32_t v4;	/* Source IPv4 address */
		uint32_t v6[4];	/* Source IPv6 address */
	} gip;			/* Union of IPv4 and IPv6 source IP address */

	bool vlan_valid;	/* VLAN ID valid? */
	uint32_t vid;		/* VLAN ID */

	ppe_drv_mcast_fwd_cmd_t fwd_cmd;	/* Forward action for matched packets */
	uint16_t port;				/* Destination port information */
};

/*
 * ppe_drv_mcast_entry_delete
 * 	Delete multicast entry.
 *
 * @param[IN] entry multicast entry
 *
 * @return
 * success or error code.
 */
 ppe_drv_ret_t ppe_drv_mcast_entry_delete(struct ppe_drv_mcast_entry *entry);

/*
 * ppe_drv_mcast_entry_add
 * 	Add multicast entry.
 *
 * @param[IN] entry multicast entry
 *
 * @return
 * success or error code.
 */
 ppe_drv_ret_t ppe_drv_mcast_entry_add(struct ppe_drv_mcast_entry *entry);

/** @} */ /* end_addtogroup ppe_drv_mcast_subsystem */

#endif /* _PPE_DRV_MCAST_H_ */
