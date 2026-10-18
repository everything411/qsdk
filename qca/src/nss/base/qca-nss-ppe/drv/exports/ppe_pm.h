/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_pm.h
 *	NSS PPE PM definitions.
 */

#ifndef _PPE_PM_H_
#define _PPE_PM_H_

#include <linux/if.h>
#include <linux/if_ether.h>

/*
 * PM Counter generation table rule flags.
 */
#define PPE_PM_GEN_RULE_FLAG_RULE_DIR			0x00000001
#define PPE_PM_GEN_RULE_FLAG_COUNTER_ID			0x00000002
#define PPE_PM_GEN_RULE_FLAG_PORT_TYPE			0x00000004
#define PPE_PM_GEN_RULE_FLAG_PORT_INFO			0x00000008
#define PPE_PM_GEN_RULE_FLAG_TAG_FORMAT			0x00000010
#define PPE_PM_GEN_RULE_FLAG_VID			0x00000020
#define PPE_PM_GEN_RULE_FLAG_PCP			0x00000040
#define PPE_PM_GEN_RULE_FLAG_IPMC			0x00000080
#define PPE_PM_GEN_RULE_FLAG_PM_DIR			0x00000100

/**
 * ppe_pm_rule_dir
 *	PM counter direction
 */
typedef enum ppe_pm_rule_dir {
	PPE_PM_RULE_DIR_INGRESS = 1,	/**< Ingress direction */
	PPE_PM_RULE_DIR_EGRESS,		/**< Egress direction */
} ppe_pm_rule_dir_t;

/**
 * ppe_pm_port_type
 *	PM port type
 */
typedef enum ppe_pm_port_type {
	PPE_PM_PORT_TYPE_BITMAP = 0,		/**< Port bitmap type */
	PPE_PM_PORT_TYPE_PORT,			/**< Port device type */
	PPE_PM_PORT_TYPE_GEMPORT,		/**< GEM port type */
} ppe_pm_port_type_t;

/**
 * ppe_pm_tag_format
 *	PM tag format
 */
typedef enum ppe_pm_tag_format {
	PPE_PM_TAG_FORMAT_UNTAGGED = 0,		/**< Untagged format */
	PPE_PM_TAG_FORMAT_PRIORITY,		/**< Priority tagged format */
	PPE_PM_TAG_FORMAT_TAGGED,		/**< Tagged format */
	PPE_PM_TAG_FORMAT_PRI_UNTAG,		/**< Priority + Untagged combination */
	PPE_PM_TAG_FORMAT_TAG_UNTAG,		/**< Tagged + Untagged combination */
	PPE_PM_TAG_FORMAT_PRI_TAG,		/**< Priority + Tagged combination */
	PPE_PM_TAG_FORMAT_ALL,			/**< All tag formats */
} ppe_pm_tag_format_t;

/**
 * ppe_pm_ipmc_type
 *	PM IPMC type
 */
typedef enum ppe_pm_ipmc_type {
	PPE_PM_IPMC_TYPE_NON_IPMC = 0,		/**< Non-IPMC */
	PPE_PM_IPMC_TYPE_IPV4_MC,		/**< IPv4 multicast */
	PPE_PM_IPMC_TYPE_IPV6_MC,		/**< IPv6 multicast */
	PPE_PM_IPMC_TYPE_NONIP_IPV4_MC,		/**< Non-IPMC + IPv4 multicast combination */
	PPE_PM_IPMC_TYPE_NONIP_IPV6_MC,		/**< Non-IPMC + IPv6 multicast combination */
	PPE_PM_IPMC_TYPE_IPV4_IPV6_MC,		/**< IPv4 + IPv6 multicast combination */
	PPE_PM_IPMC_TYPE_ALL,			/**< All IPMC types */
} ppe_pm_ipmc_type_t;

/**
 * ppe_pm_ret
 *	PM return code
 */
typedef enum ppe_pm_ret {
	PPE_PM_RET_SUCCESS = 0,				/**< Success */
	PPE_PM_RET_COUNTER_CTX_FAIL_OOM,		/**< Counter context create failed due to out of memory. */
	PPE_PM_RET_COUNTER_GEN_FAIL_OOM,		/**< Counter gen create failed due to OOM. */
	PPE_PM_RET_INVALID_COUNTER_ID,			/**< Invalid counter ID. */
	PPE_PM_RET_PM_COUNTER_GET_FAIL,			/**< PM counter get failed. */
	PPE_PM_RET_CREATE_FAIL_RULE_FILL,		/**< PM counter gen rule fill failed. */
	PPE_PM_RET_CREATE_FAIL_RULE,			/**< PM counter gen rule create failed. */
}ppe_pm_ret_t;

/*
 * ppe_pm_counter
 *	PM counter table fields
 */
struct ppe_pm_counter {
	uint8_t counter_id;		/**< Counter ID */

	/*
	 * Hardware Counters
	 */
	uint64_t ucast_packet;		/**< Unicast Packets */
	uint64_t bcast_packet;		/**< Broadcast Packets */
	uint64_t mcast_packet;		/**< Multicast Packets */
	uint64_t oversize;		/**< Oversized Packets */
	uint64_t octets;		/**< Total Bytes */
	uint64_t frame_64;		/**< Frames of size 64 bytes */
	uint64_t frame_65_127;		/**< Frames of size 65-127 bytes */
	uint64_t frame_128_255;		/**< Frames of size 128-255 bytes */
	uint64_t frame_256_511;		/**< Frames of size 256-511 bytes */
	uint64_t frame_512_1023;	/**< Frames of size 512-1023 bytes */
	uint64_t frame_1024_1518;	/**< Frames of size 1024-1518 bytes */

	/*
	 * Response
	 */
	ppe_pm_ret_t ret;		/**< PM return type */

};

/*
 * ppe_pm_counter_gen
 *	PM counter generation table fields
 */
struct ppe_pm_counter_gen {
	ppe_pm_rule_dir_t rule_dir;	/**< Rule Direction: INGRESS/EGRESS */
	ppe_pm_rule_dir_t pm_dir;	/**< PM counter direction: INGRESS/EGRESS */
	uint8_t counter_id;		/**< Counter ID */
	ppe_pm_port_type_t port_type;	/**< Port type field */
	union {
		uint32_t port_bitmap;
		char dev_name[IFNAMSIZ];
		uint32_t gem_port;
	} port;				/**< Port information */
	ppe_pm_tag_format_t tag_format;	/**< Tag format field */
	uint16_t vid;			/**< VID value */
	uint8_t pcp;			/**< PCP value */
	ppe_pm_ipmc_type_t ipmc;	/**< IPMC field */

	/*
	 * Rule control flags.
	 */
	uint32_t rule_flags;		/**< Rule control flags. */

	/*
	 * Response
	 */
	ppe_pm_ret_t ret;		/**< PM return type. */

};

/**
 * ppe_pm_counter_get()
 *	Fetch and fill counter statistics for a given counter ID.
 *
 * @datatypes
 * ppe_pm_counter
 *
 * @param[IN] counter_info            Pointer to ppe_pm_counter structure to store the stats.
 *
 * @return
 * Status of the operation, indicating success (PPE_PM_RET_SUCCESS) or failure.
 *
 * Retrieves hardware statistics for the specified counter ID via ppe_drv,
 * copying them into the provided counter_info structure if a valid context exists.
 */
ppe_pm_ret_t ppe_pm_counter_get(struct ppe_pm_counter *counter_info);

/**
 * ppe_pm_counter_alloc()
 *	Acquire a counter context for a given counter ID.
 *
 * @datatypes
 * ppe_drv_pm_counter_info
 *
 * @param[IN] counter_id              ID of the counter to acquire (0 to PPE_DRV_PM_COUNTER_CTX_MAX-1).
 *
 * @return
 * Hardware index on success, -1 on failure.
 *
 * Allocates or reuses a counter context in ppe_pm, incrementing its reference count.
 * If no context exists, requests one from ppe_drv. Fails if counter_id is invalid.
 */
int16_t ppe_pm_counter_alloc(uint8_t counter_id, ppe_pm_rule_dir_t rule_dir);

/**
 * ppe_pm_counter_deref()
 *	Release a counter context for a given counter ID.
 *
 * @datatypes
 * None
 *
 * @param[IN] counter_id              ID of the counter to release (0 to PPE_DRV_PM_COUNTER_CTX_MAX-1).
 *
 * @return
 * None
 *
 * Decrements the reference count of the counter context in ppe_pm. If the count
 * reaches zero, clears the local reference and releases it via ppe_drv. No-op if
 * counter_id is invalid or no context exists.
 */
void ppe_pm_counter_deref(uint8_t counter_id);

/**
 * ppe_pm_counter_gen_rule_create()
 *	Create a PM counter generation rule.
 *
 * @datatypes
 * ppe_pm_counter_gen
 *
 * @param[IN] rule                    Pointer to the structure containing the rule details.
 *
 * @return
 * Status of the operation, indicating success or failure.
 *
 * Initializes and creates a general counter rule in the PPE PM module.
 */
ppe_pm_ret_t ppe_pm_counter_gen_rule_create(struct ppe_pm_counter_gen *rule);

/**
 * ppe_pm_counter_gen_rule_destroy()
 *	Destroy a PM counter generation rule.
 *
 * @datatypes
 * None
 *
 * @param[IN] counter_id              ID of the counter rule to destroy.
 *
 * @return
 * Status of the operation, indicating success or failure.
 *
 * Releases the resources associated with a general counter rule identified by
 * the given counter ID.
 */
ppe_pm_ret_t ppe_pm_counter_gen_rule_destroy(uint8_t counter_id);

#endif // _PPE_PM_H_
