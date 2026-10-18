/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <fal/fal_pon_pm.h>
#include <ppe_drv_pm.h>

/*
 * PM Counter Gen HW ID macros.
 */
#define PPE_DRV_PM_COUNTER_GEN_ID_MAX		32
#define PPE_DRV_PM_COUNTER_GEN_ID_USED		1
#define PPE_DRV_PM_COUNTER_GEN_ID_FREE		2
#define PPE_DRV_PM_COUNTER_GEN_ID_START		0
#define PPE_DRV_PM_COUNTER_GEN_ID_END		(PPE_DRV_PM_COUNTER_GEN_ID_MAX - 1)

/*
 * PM Counter HW ID macros.
 */
#define PPE_DRV_PM_COUNTER_ID_MAX		32
#define PPE_DRV_PM_COUNTER_ID_USED		1
#define PPE_DRV_PM_COUNTER_ID_FREE		2
#define PPE_DRV_PM_COUNTER_ID_START		0
#define PPE_DRV_PM_COUNTER_ID_END		(PPE_DRV_PM_COUNTER_ID_MAX - 1)

/*
 * PM counter stats update macros.
 */
#define PPE_DRV_PM_PKT_CNTR_ROLLOVER(delta) ((delta + FAL_FLOW_PKT_CNT_MASK + 1) & FAL_FLOW_PKT_CNT_MASK)
#define PPE_DRV_PM_BYTE_CNTR_ROLLOVER(delta) ((delta + FAL_FLOW_BYTE_CNT_MASK + 1) & FAL_FLOW_BYTE_CNT_MASK)

/*
 * ppe_drv_pm_counter_gen_ctx
 *	PM counter gen table context
 */
struct ppe_drv_pm_counter_gen_ctx {
	/*
	 * Rule Shadow.
	 */
	fal_pon_pm_counter_entry_t fal_rule;	/* Fal rule structure. */

	ppe_drv_rule_dir_t rule_dir;		/* Flow direction. */
	uint8_t entry_index;			/* HW table entry Index. */
	uint8_t tbl_idx;			/* PM Gen table Index. */
	uint8_t idx_state;			/* PM Gen table Index state. */
};

/*
 * ppe_drv_pm_counter_ctx
 *	Internal PM counter table context
 */
struct ppe_drv_pm_counter_ctx {
	struct ppe_drv_pm_counter_info info;	/* Counter info -  to be exported */
	fal_pon_pm_counter_t fal_counter;	/* FAL Entry */
	uint8_t state;				/* FREE or USED */
};

/*
 * ppe_drv_pm_gen
 *	PM counter gen table memory management.
 */
struct ppe_drv_pm_gen {
	struct ppe_drv_pm_counter_gen_ctx pm_ctx[PPE_DRV_PM_COUNTER_GEN_ID_MAX];	/* Ingress PM counter gen shadow table */
	struct ppe_drv_pm_counter_gen_ctx eg_pm_ctx[PPE_DRV_PM_COUNTER_GEN_ID_MAX];	/* Egress PM counter gen shadow table */
};

/*
 * ppe_drv_pm
 *	PM-specific structure for counter management.
 */
struct ppe_drv_pm {
	struct ppe_drv_pm_counter_ctx in_ctx_array[PPE_DRV_PM_COUNTER_ID_MAX];	/* Ingress context array, indexed by hw_index */
	struct ppe_drv_pm_counter_ctx eg_ctx_array[PPE_DRV_PM_COUNTER_ID_MAX];	/* Egress context array, indexed by hw_index */
};

/*
 * Internal APIs.
 */

/**
 * ppe_drv_pm_gen_entries_alloc - Allocate and initialize the PM counter generation management structure.
 * @datatypes
 * None
 *
 * @return
 * Pointer to the allocated ppe_drv_pm_gen structure on success, NULL on failure.
 */
struct ppe_drv_pm_gen *ppe_drv_pm_gen_entries_alloc(void);

/**
 * ppe_drv_pm_entries_alloc - Allocate and initialize the PM counter management structure.
 * @datatypes
 * None
 *
 * @return
 * Pointer to the allocated ppe_drv_pm structure on success, NULL on failure.
 */
struct ppe_drv_pm *ppe_drv_pm_entries_alloc(void);

/**
 * ppe_drv_pm_counter_stats_update - Update statistics for a counter ID
 * @datatypes
 * None
 *
 * @param[in] ctx	PM counter context
 *
 * @return
 * None
 *
 * Updates the counter context with statistics for the specified counter_id by
 * querying the hardware (via fal_api, if implemented) or using hardcoded values
 * for testing. Requires a valid context to exist in ppe_drv.
 */
void ppe_drv_pm_counter_stats_update(struct ppe_drv_pm_counter_ctx *ctx);

/**
 * ppe_drv_pm_entries_free() - Free PM counter table or entry container
 * @pm: Pointer to a PM structure previously allocated by ppe_drv (e.g. via
 *      ppe_drv_pm_gen_entries_alloc() or another allocation routine).
 *
 * Releases memory associated with a PM counter table or list structure. This
 * function performs only a memory free operation and does not modify hardware
 * state nor update PM allocation bookkeeping.
 *
 *
 * Any pointer passed must have been allocated using the PPE driver's allocation
 * APIs (vmalloc/vzalloc/kzalloc). Passing an invalid or already-freed pointer
 * leads to undefined behavior.
 *
 * Return: None
 */
void ppe_drv_pm_entries_free(void *pm);
