/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_PM_H_
#define _PPE_DRV_PM_H_

#include <fal/fal_pon_pm.h>

/*
 * Max counter contexts (counter_id range: 0-63).
 */
#define PPE_DRV_PM_COUNTER_CTX_MAX			64

/*
 * Rule flags.
 */
#define PPE_DRV_PM_RULE_FLAG_PORT_ID			0x00000001
#define PPE_DRV_PM_RULE_FLAG_TAG_FORMAT			0x00000002
#define PPE_DRV_PM_RULE_FLAG_VID			0x00000004
#define PPE_DRV_PM_RULE_FLAG_PCP			0x00000008
#define PPE_DRV_PM_RULE_FLAG_IPMC			0x00000010

/*
 * Tag format for PM generation rule
 */
#define PPE_DRV_PM_TAG_FORMAT_UNTAGGED		(1 << 0)
#define PPE_DRV_PM_TAG_FORMAT_PRIORITY		(1 << 1)
#define PPE_DRV_PM_TAG_FORMAT_TAGGED		(1 << 2)
#define PPE_DRV_PM_TAG_FORMAT_ALL		(PPE_DRV_PM_TAG_FORMAT_UNTAGGED | PPE_DRV_PM_TAG_FORMAT_PRIORITY | PPE_DRV_PM_TAG_FORMAT_TAGGED)

/*
 * IPMC types for PM generation rule
 */
#define PPE_DRV_PM_IPMC_TYPE_NON_IPMC           (1 << 0)
#define PPE_DRV_PM_IPMC_TYPE_IPV4_MC            (1 << 1)
#define PPE_DRV_PM_IPMC_TYPE_IPV6_MC            (1 << 2)
#define PPE_DRV_PM_IPMC_TYPE_ALL                (PPE_DRV_PM_IPMC_TYPE_NON_IPMC | PPE_DRV_PM_IPMC_TYPE_IPV4_MC | PPE_DRV_PM_IPMC_TYPE_IPV6_MC)

/**
 * ppe_drv_pm_counter_gen_ctx
 *	Opaque context for PM counter generation rule
 */
struct ppe_drv_pm_counter_gen_ctx;
struct ppe_drv_pm_counter_ctx;

/*
 * ppe_drv_pm_port_type
 *	Port type.
 */
enum ppe_drv_pm_port_type
{
	PPE_DRV_PM_PORT_TYPE_BITMAP = 0,	/**< Port bitmap type */
	PPE_DRV_PM_PORT_TYPE_PORT,		/**< Port device type */
	PPE_DRV_PM_PORT_TYPE_GEMPORT,		/**< GEM port type */
};

/*
 * ppe_drv_pm_counter_gen_rule
 *	PM counter gen table rule
 */
struct ppe_drv_pm_counter_gen_rule {
	uint8_t counter_id;		/**< Counter ID */
	enum ppe_drv_pm_port_type port_type; /**< Port type field */
	union {
		uint8_t port_num;		/**< Port number */
		uint8_t port_bitmap;		/**< Port bitmap */
		uint8_t gemport;		/**< GEM port */
	} port_info;			/**< Port information */
	ppe_drv_rule_dir_t rule_dir;	/**< Rule direction */
	uint8_t tag_format;		/**< Tag format field: tag, untagged, all */
	uint16_t vid;			/**< VID value */
	uint8_t pcp;			/**< PCP value */
	uint8_t ipmc;			/**< IPMC field: Non_IPMC/IPv4_MC/IPv6_MC */

	/*
	 * Rule Flags
	 */
	uint32_t flags;			/**< Rule flags */

};

/*
 * ppe_drv_pm_counter_info
 *	PM counter table info
 */
struct ppe_drv_pm_counter_info {
	uint8_t hw_index;		/**< Hardware index */
	ppe_drv_rule_dir_t rule_dir;	/**< Rule direction */

	/*
	 * Hardware Counters
	 */
	atomic64_t ucast_packet;	/**< Unicast Packets */
	atomic64_t bcast_packet;	/**< Broadcast Packets */
	atomic64_t mcast_packet;	/**< Multicast Packets */
	atomic64_t oversize;		/**< Oversized Packets */
	atomic64_t octets;		/**< Total Bytes */
	atomic64_t frame_64;		/**< Frames of size 64 bytes */
	atomic64_t frame_65_127;	/**< Frames of size 65-127 bytes */
	atomic64_t frame_128_255;	/**< Frames of size 128-255 bytes */
	atomic64_t frame_256_511;	/**< Frames of size 256-511 bytes */
	atomic64_t frame_512_1023;	/**< Frames of size 512-1023 bytes */
	atomic64_t frame_1024_1518;	/**< Frames of size 1024-1518 bytes */
};

/*
 * ppe_drv_pm_gen_rule_create()
 *	Create and configure a PM counter generation rule.
 *
 * @datatypes
 * ppe_drv_pm_counter_gen_ctx
 * ppe_drv_pm_counter_gen_rule
 *
 * @param[in] ctx  Context for PM counter generation.
 * @param[in] rule  Rule to be created and configured.
 *
 * @return
 * A status code indicating success or failure.
 */
ppe_drv_ret_t  ppe_drv_pm_gen_rule_create(struct ppe_drv_pm_counter_gen_ctx *ctx, struct ppe_drv_pm_counter_gen_rule *rule);

/*
 * ppe_drv_pm_gen_alloc()
 *	Allocate context for PM counter generation based on flow direction.
 *
 * @datatypes
 * ppe_drv_rule_dir_t
 *
 * @param[in] rule_dir  Direction of the flow for PM counter generation.
 *
 * @return
 * A pointer to the allocated PM counter generation context.
 */
struct ppe_drv_pm_counter_gen_ctx *ppe_drv_pm_gen_alloc(ppe_drv_rule_dir_t rule_dir);

/*
 * ppe_drv_pm_gen_destroy()
 *	Destroy the PM counter generation context.
 *
 * @datatypes
 * ppe_drv_pm_counter_gen_ctx
 *
 * @param[in] ctx  Context to be destroyed.
 *
 * @return
 * None.
 */
void ppe_drv_pm_gen_destroy(struct ppe_drv_pm_counter_gen_ctx *ctx);

/**
 * ppe_drv_pm_alloc - Allocate a counter context based on rule direction.
 * @datatypes
 * struct ppe_drv_pm_counter_ctx
 *
 * @param[in] rule_dir        Rule direction for which context needs to be allocated.
 *
 * @return
 * Pointer to the allocated opaque ppe_drv_pm_counter_ctx on success, NULL on failure.
 *
 * This API finds a free hardware index in the driver for the given direction and allocates
 * an opaque context for it. The caller is responsible for managing this context and
 * freeing it when it's no longer needed.
 */
struct ppe_drv_pm_counter_ctx *ppe_drv_pm_alloc(ppe_drv_rule_dir_t rule_dir);

/**
 * ppe_drv_pm_free - Free a counter context.
 * @datatypes
 * struct ppe_drv_pm_counter_ctx
 *
 * @param[in] ctx        Opaque counter context pointer to be freed.
 *
 * @return
 * None.
 *
 * This API frees the driver context and releases the associated hardware index.
 * It should be called when the context is no longer in use by any rule.
 */
void ppe_drv_pm_free(struct ppe_drv_pm_counter_ctx *ctx);

/**
 * ppe_drv_pm_counter_info_get - Get counter info from context
 * @datatypes
 * struct ppe_drv_pm_counter_ctx
 *
 * @param[in] ctx        Counter context pointer.
 *
 * @return
 * Pointer to the ppe_drv_pm_counter_info.
 */
struct ppe_drv_pm_counter_info *ppe_drv_pm_counter_info_get(struct ppe_drv_pm_counter_ctx *ctx);

#endif /* _PPE_DRV_PM_H_ */
