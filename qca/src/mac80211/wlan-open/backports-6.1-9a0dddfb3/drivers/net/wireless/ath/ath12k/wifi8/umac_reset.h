/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_UMAC_RESET_H
#define ATH12K_WIFI8_UMAC_RESET_H

#include "../core.h"
#include "../umac_reset.h"
#include "hal.h"

/* WiFi8-specific UMAC reset function declarations */
void ath12k_wifi8_umac_reset_handle_pre_reset(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_post_reset_start(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_post_reset_complete(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_init_recovery(struct ath12k_base *ab);
int ath12k_cumac_hw_pre_reset(struct ath12k_base *ab);
int ath12k_cumac_hw_post_reset(struct ath12k_base *ab);
int ath12k_cumac_hw_reset(struct ath12k_base *ab);
void ath12k_wifi8_update_tqm_status_ring_tp(struct ath12k_base *ab, bool enable);
int ath12k_wifi8_umcmn_irq_config(struct ath12k_base *ab);
void ath12k_wifi8_umcmn_irq_free(struct ath12k_base *ab);
void ath12k_wifi8_umcmn_irq_enable(struct ath12k_base *ab);
void ath12k_wifi8_umcmn_irq_disable(struct ath12k_base *ab);
int ath12k_wifi8_umcmn_timer_config(struct ath12k_base *ab);
void ath12k_wifi8_umcmn_timer_enable(struct ath12k_base *ab);
void ath12k_wifi8_umcmn_timer_free(struct ath12k_base *ab);

#define ATH12K_UMCMN_TIMER_INTERVAL_MS 100
#define ATH12K_TQM_TP_TIMER_INTERVAL_MS 10

#define HAL_UMAC_UMCMN_R0_ISR_P			0xF1E034
#define HAL_UMAC_UMCMN_R0_ISR_S0		0xF1E038
#define HAL_UMAC_UMCMN_R0_ISR_S2		0xF1E03C
#define HAL_UMAC_UMCMN_R0_ISR_S3		0xF1E040
#define HAL_UMAC_UMCMN_R0_ISR_S4		0xF1E044
#define HAL_UMAC_UMCMN_R0_ISR_S5		0xF1E048
#define HAL_UMAC_UMCMN_R0_ISR_S6		0xF1E04C
#define HAL_UMAC_UMCMN_R0_ISR_S7		0xF1E050
#define HAL_UMAC_UMCMN_R0_ISR_S8		0xF1E054
#define HAL_UMAC_UMCMN_R0_ISR_S9		0xF1E058
#define HAL_UMAC_UMCMN_R0_ISR_S10		0xF1E05C
#define HAL_UMAC_UMCMN_R0_ISR_S11		0xF1E060
#define HAL_UMAC_UMCMN_R0_ISR_S12		0xF1E064
#define HAL_UMAC_UMCMN_R0_ISR_S13		0xF1E068
#define HAL_UMAC_UMCMN_R0_ISR_S14		0xF1E06C
#define HAL_UMAC_UMCMN_R0_ISR_S15		0xF1E070
#define HAL_UMAC_UMCMN_R0_ISR_S16		0xF1E074
#define HAL_UMAC_UMCMN_R0_ISR_S17		0xF1E078
#define HAL_UMAC_UMCMN_R0_ISR_S18		0xF1E230
#define HAL_UMAC_UMCMN_R0_ISR_S19		0xF1E234
#define HAL_UMAC_UMCMN_R0_ISR_S20		0xF1E238
#define HAL_UMAC_UMCMN_R0_ISR_S21		0xF1E23C
#define HAL_UMAC_UMCMN_R0_ISR_S22		0xF1E240
#define HAL_UMAC_UMCMN_R0_ISR_S23		0xF1E244
#define HAL_UMAC_UMCMN_R0_ISR_S24		0xF1E248
#define HAL_UMAC_UMCMN_R0_ISR_S25		0xF1E24C
#define HAL_UMAC_UMCMN_R0_ISR_S26		0xF1E250
#define HAL_UMAC_UMCMN_R0_ISR_S27		0xF1E254
#define HAL_UMAC_UMCMN_R0_ISR_S28		0xF1E258
#define HAL_UMAC_UMCMN_R0_ISR_S29		0xF1E25C
#define HAL_UMAC_UMCMN_R0_ISR_S30		0xF1E260

#define HAL_UMCMN_ISR_S14_INDEX			14
#define HAL_UMCMN_ISR_S14_TQM_WATCHDOG		BIT(2)

struct ath12k_tqm_sm_state {
	u32 sm;
	u32 bank;
};

/* CUMAC HW Reset Step Enumerations */

/**
 * enum cumac_hw_pre_reset_steps - Pre-reset sequence steps
 *
 * Defines the ordered steps for preparing hardware before UMAC reset
 */
enum {
	CUMAC_HW_PRE_RESET_START,
	CUMAC_HW_PRE_RESET_RXDMA_PREFETCH_DISABLE,
	CUMAC_HW_PRE_RESET_HALT_MLO_DOORBELLS,
	CUMAC_HW_PRE_RESET_PAUSE_GLOBAL_WSI,
	CUMAC_HW_PRE_RESET_DMAC_PMAC_DECOUPLE,
	CUMAC_HW_PRE_RESET_HALT_TCL,
	CUMAC_HW_PRE_RESET_DISABLE_SAM,
	CUMAC_HW_PRE_RESET_PAUSE_TQM,
	CUMAC_HW_PRE_RESET_DISABLE_WBM,
	CUMAC_HW_PRE_RESET_DISABLE_REO,
	CUMAC_HW_PRE_RESET_END,
	CUMAC_HW_PRE_RESET_MAX,
};

/**
 * enum cumac_hw_reset_steps - Reset sequence steps
 *
 * Defines the ordered steps for performing UMAC hardware reset
 */
enum {
	CUMAC_HW_RESET_START,
	CUMAC_HW_RESET_PREREQUISITES,
	CUMAC_HW_RESET_PRE_RING_RESET,
	CUMAC_HW_RESET_APPLY_SOFT_RESET,
	CUMAC_HW_RESET_POST_RING_RESET,
	CUMAC_HW_RESET_END,
	CUMAC_HW_RESET_MAX,
};

/**
 * enum cumac_hw_post_reset_steps - Post-reset sequence steps
 *
 * Defines the ordered steps for restoring hardware after UMAC reset
 */
enum {
	CUMAC_HW_POST_RESET_START,
	CUMAC_HW_POST_RESET_ENABLE_TQM,
	CUMAC_HW_POST_RESET_CLEAR_INTERRUPTS,
	CUMAC_HW_POST_RESET_ENABLE_SAM,
	CUMAC_HW_POST_RESET_ENABLE_WBM,
	CUMAC_HW_POST_RESET_ENABLE_REO,
	CUMAC_HW_POST_RESET_UNPAUSE_GLOBAL_WSI,
	CUMAC_HW_POST_RESET_UNHALT_MLO_DOORBELLS,
	CUMAC_HW_POST_RESET_ENABLE_RXDMA_PREFETCH,
	CUMAC_HW_POST_RESET_UNHALT_TCL,
	CUMAC_HW_POST_RESET_END,
	CUMAC_HW_POST_RESET_MAX,
};

/**
 * struct ath12k_cumac_hw_reset_timestamps - CUMAC HW reset timestamp tracking
 *
 * Stores per-step timestamps for each phase of the CUMAC HW reset process,
 * indexed by the corresponding step enum values.
 */
struct ath12k_cumac_hw_reset_timestamps {
	u64 cumac_hw_pre_reset_ts[CUMAC_HW_PRE_RESET_MAX];
	u64 cumac_hw_reset_ts[CUMAC_HW_RESET_MAX];
	u64 cumac_hw_post_reset_ts[CUMAC_HW_POST_RESET_MAX];
};

/**
 * struct ath12k_cumac_hw_reset_errors - CUMAC HW reset idle-check failure stats
 *
 * Stores error/idle check failure stats
 */
struct ath12k_cumac_hw_reset_errors {
	u32 pmac_idle_fail;
	u32 dmac_idle_fail;
	u32 umac_idle_fail;
	u32 wsi_idle_fail;
	u32 hwsch_flush_fail;
	u32 pmac_rx_flush_fail;
	u32 rxdma_flush_fail;
	u32 tcl_idle_fail;
	u32 sam_idle_fail;
	u32 tqm_prefetch_idle_fail;
	u32 tqm_sm_idle_fail;
	u32 mlo_ring_idle_fail;
	u32 ring_idle_fail;
};

#define ATH12K_CUMAC_RESET_ERR_INC(ab, field) \
	((ath12k_get_hal_wifi8(&(ab)->hal)->reset_errors.field)++)

/**
 * typedef cumac_hw_reset_fn - Function pointer type for reset step handlers
 * @ab: Pointer to ath12k_base structure
 *
 * Return: 0 on success, negative error code on failure
 */
typedef int (*cumac_hw_reset_fn)(struct ath12k_base *ab, u32 arg);

/**
 * struct cumac_hw_reset_step - Descriptor for a single reset step
 * @fn: Function pointer to execute for this step
 * @name: Human-readable name of the step for logging
 */
struct cumac_hw_reset_step {
	cumac_hw_reset_fn fn;
	u32 arg;
	const char *name;
};

#endif /* ATH12K_WIFI8_UMAC_RESET_H */
