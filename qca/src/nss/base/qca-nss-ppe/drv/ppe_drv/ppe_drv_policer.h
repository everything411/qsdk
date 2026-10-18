/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <fal/fal_policer.h>
#include <fal/fal_api.h>
#include <fal/fal_flow.h>

#define PPE_DRV_PORT_POLICER_MAX 8
#ifdef NSS_PPE_IPQ53XX
#define PPE_DRV_ACL_POLICER_MAX 128
#else
#define PPE_DRV_ACL_POLICER_MAX 512
#endif

#define PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(delta) (((delta) + FAL_FLOW_PKT_CNT_MASK + 1) & FAL_FLOW_PKT_CNT_MASK)
#define PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(delta) (((delta) + FAL_FLOW_BYTE_CNT_MASK + 1) & FAL_FLOW_BYTE_CNT_MASK)

/*
 * ppe_drv_policer_port
 *	 Policer Port interface information
 */
struct ppe_drv_policer_port {
	uint16_t index;				/* Port policer index */
	bool in_use;				/* Entry in use */

	/*
	 * Hardware stats.
	 */
	fal_policer_counter_t pre_cntrs;	/* Previous hardware counters. */
	atomic64_t green_packet_counter;	/*green packet counter */
	atomic64_t green_byte_counter;		/*green byte counter */
	atomic64_t yellow_packet_counter;	/*yellow packet counter */
	atomic64_t yellow_byte_counter;		/*yellow byte counter */
	atomic64_t red_packet_counter;		/*red packet counter */
	atomic64_t red_byte_counter;		/*red byte counter */
};

/*
 * ppe_drv_policer_acl
 *	 Policer ACL interface information
 */
struct ppe_drv_policer_acl {
	uint32_t num_acl_reserved;		/* Number of ACL reserved */
	uint16_t acl_index;			/* Policer index */
	bool in_use;				/* Entry in use */
	bool is_configured;			/* Is ACL configured? */

	/*
	 * Hardware stats.
	 */
	fal_policer_counter_t pre_cntrs;	/* Previous hardware counters. */
	atomic64_t green_packet_counter;	/*green packet counter */
	atomic64_t green_byte_counter;		/*green byte counter */
	atomic64_t yellow_packet_counter;	/*yellow packet counter */
	atomic64_t yellow_byte_counter;		/*yellow byte counter */
	atomic64_t red_packet_counter;		/*red packet counter */
	atomic64_t red_byte_counter;		/*red byte counter */
};

/*
 * ppe_drv_policer
 *	Global context
 */
struct ppe_drv_policer_ctx {
	struct ppe_drv_policer_port port_pol[PPE_DRV_PORT_POLICER_MAX];
	struct ppe_drv_policer_acl acl_pol[PPE_DRV_ACL_POLICER_MAX];
	ppe_drv_policer_flow_callback_t flow_add_cb;
	ppe_drv_policer_flow_callback_t flow_del_cb;
	void *flow_app_data;
	int user2hw_map[PPE_DRV_ACL_POLICER_MAX];
};

/*
 * ppe_drv_policer_stat
 * 	Information for PPE Policer statistics
 */
struct ppe_drv_policer_stat {
	uint64_t green_pkts;		/* green packet counter. */
	uint64_t green_bytes;		/* green byte counter. */
	uint64_t yellow_pkts;		/* yellow packet counter. */
	uint64_t yellow_bytes;		/* yellow byte counter. */
	uint64_t red_pkts;		/* red packet counter. */
	uint64_t red_bytes;		/* red byte counter. */
};

/*
 * ppe_drv_policer_port_get_index
 *	Return port policer index
 */
static inline uint16_t ppe_drv_policer_port_get_index(struct ppe_drv_policer_port *pol)
{
	return pol->index;
}

/*
 * ppe_drv_policer_acl_get_index
 *	Return ACL policer index
 */
static inline uint16_t ppe_drv_policer_acl_get_index(struct ppe_drv_policer_acl *pol)
{
	return pol->acl_index;
}

/*
 * ppe_drv_port_policer_stats_add()
 *	Add counters to Port Policer stats atomically.
 */
static inline void ppe_drv_port_policer_stats_add(struct ppe_drv_policer_port *ctx, struct ppe_drv_policer_stat *delta)
{
	atomic64_add(delta->green_pkts, &ctx->green_packet_counter);
	atomic64_add(delta->green_bytes, &ctx->green_byte_counter);
	atomic64_add(delta->yellow_pkts, &ctx->yellow_packet_counter);
	atomic64_add(delta->yellow_bytes, &ctx->yellow_byte_counter);
	atomic64_add(delta->red_pkts, &ctx->red_packet_counter);
	atomic64_add(delta->red_bytes, &ctx->red_byte_counter);
}

/*
 * ppe_drv_acl_policer_stats_add()
 *	Add counters to ACL Policer stats atomically.
 */
static inline void ppe_drv_acl_policer_stats_add(struct ppe_drv_policer_acl *ctx, struct ppe_drv_policer_stat *delta)
{
	atomic64_add(delta->green_pkts, &ctx->green_packet_counter);
	atomic64_add(delta->green_bytes, &ctx->green_byte_counter);
	atomic64_add(delta->yellow_pkts, &ctx->yellow_packet_counter);
	atomic64_add(delta->yellow_bytes, &ctx->yellow_byte_counter);
	atomic64_add(delta->red_pkts, &ctx->red_packet_counter);
	atomic64_add(delta->red_bytes, &ctx->red_byte_counter);
}

int ppe_drv_policer_user2hw_id(int index);

uint16_t ppe_drv_policer_acl_get_index(struct ppe_drv_policer_acl *pol);
uint16_t ppe_drv_policer_port_get_index(struct ppe_drv_policer_port *pol);

void ppe_drv_policer_entries_free(struct ppe_drv_policer_ctx *pol);
struct ppe_drv_policer_ctx *ppe_drv_policer_entries_alloc(void);
void ppe_drv_port_policer_stats_update(struct ppe_drv_policer_port *ctx);
void ppe_drv_acl_policer_stats_update(struct ppe_drv_policer_acl *ctx);
