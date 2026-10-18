/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_CBS_EXTN_H
#define ATH12K_CBS_EXTN_H

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

/* Continuous Background Scan (CBS) - State Machine Implementation
 * 
 * CBS performs background scanning on channels while maintaining normal
 * operation on the home channel. It uses dwell splitting to minimize
 * impact on active traffic.
 */

#define ATH12K_CBS_SCAN_ID				0xA001

/* CBS Dwell Time Options (in milliseconds) */
#define ATH12K_CBS_DWELL_TIME_20MS			20
#define ATH12K_CBS_DWELL_TIME_25MS			25
#define ATH12K_CBS_DWELL_TIME_50MS			50

/* CBS Total Dwell Time Options (in milliseconds) */
#define ATH12K_CBS_TOTAL_DWELL_TIME_20MS		20
#define ATH12K_CBS_TOTAL_DWELL_TIME_25MS		25
#define ATH12K_CBS_TOTAL_DWELL_TIME_50MS		50
#define ATH12K_CBS_TOTAL_DWELL_TIME_100MS		100
#define ATH12K_CBS_TOTAL_DWELL_TIME_200MS		200

/* CBS Array Sizes for dwell split configuration */
#define ATH12K_CBS_SCAN_OFFSET_ARRAY_SIZE		10

/* Maximum channels supported in scan list */
#define ATH12K_CBS_MAX_CHANNELS				256

#define ATH12K_CBS_RUN_ONCE		0x1
#define ATH12K_CBS_RUN_CONTINUOUS	0x2

/**
 * enum ath12k_cbs_state - CBS State Machine States
 * 
 * @ATH12K_CBS_STATE_INIT: Initial/idle state, CBS not active
 * @ATH12K_CBS_STATE_SCAN: Actively scanning a channel
 * @ATH12K_CBS_STATE_REST: Resting between scan operations
 * @ATH12K_CBS_STATE_WAIT: Waiting before next scan cycle
 */
enum ath12k_cbs_state {
	ATH12K_CBS_STATE_INIT = 0,
	ATH12K_CBS_STATE_SCAN,
	ATH12K_CBS_STATE_REST,
	ATH12K_CBS_STATE_WAIT,
};

/**
 * enum ath12k_cbs_event - CBS State Machine Events
 * 
 * @ATH12K_CBS_EVENT_SCAN_START: Start a new scan cycle
 * @ATH12K_CBS_EVENT_SCAN_NEXT: Move to next channel in scan list
 * @ATH12K_CBS_EVENT_SCAN_CONTINUE: Continue scanning after rest
 * @ATH12K_CBS_EVENT_DWELL_SPLIT: Handle dwell split timing
 * @ATH12K_CBS_EVENT_SCAN_CANCEL: Cancel scan
 * @ATH12K_CBS_EVENT_SCAN_COMPLETE: Scan cycle completed
 * @ATH12K_CBS_EVENT_SCAN_RESTART: Restart CBS after delay
 */
enum ath12k_cbs_event {
	ATH12K_CBS_EVENT_SCAN_START = 1,
	ATH12K_CBS_EVENT_SCAN_NEXT,
	ATH12K_CBS_EVENT_SCAN_CONTINUE,
	ATH12K_CBS_EVENT_DWELL_SPLIT,
	ATH12K_CBS_EVENT_SCAN_CANCEL,
	ATH12K_CBS_EVENT_SCAN_COMPLETE,
	ATH12K_CBS_EVENT_SCAN_RESTART,
};

/**
 * enum ath12k_cbs_status - CBS Status Types
 * 
 * @ATH12K_CBS_STATUS_STARTED: CBS scan started
 * @ATH12K_CBS_STATUS_COMPLETED: CBS scan completed successfully
 * @ATH12K_CBS_STATUS_CANCELLED: CBS scan was cancelled
 */
enum ath12k_cbs_status {
	ATH12K_CBS_STATUS_STARTED,
	ATH12K_CBS_STATUS_COMPLETED,
	ATH12K_CBS_STATUS_CANCELLED,
};

/* Forward declarations */
struct ath12k;
struct ath12k_link_vif;
struct wmi_scan_event;

/**
 * struct ath12k_cbs - CBS Context Structure
 * 
 * This structure maintains the complete state of the CBS state machine
 * including configuration, scan parameters, and runtime state.
 * 
 * @ar: Associated ath12k radio
 * @arvif: Associated virtual interface
 * @state: Current state machine state
 * @pending_event: Pending event to be processed
 * @lock: Spinlock for synchronization
 * @timer: Timer for state transitions
 * @work: Work queue for event processing
 * @scan_id: Current scan ID
 * @dwell_time: Dwell time per channel (ms)
 * @rest_time: Rest time between channels (ms)
 * @wait_time: Wait time after scan cycle (ms)
 * @scan_intvl_time: Scan interval time (ms)
 * @chan_list: List of channels to scan
 * @nchans: Number of channels in scan list
 * @chan_list_idx: Current index in channel list
 * @cbs_enable: CBS enable flag
 * @max_dwell_split_cnt: Maximum dwell split count
 * @dwell_split_cnt: Current dwell split count
 * @scan_offset: Array of scan offset times (ms)
 * @scan_dwell_rest: Array of dwell rest times (ms)
 * @min_dwell_rest_time: Minimum dwell rest time (ms)
 * @cbs_total_dwell_time: Total dwell time per channel (ms)
 * @dwell_split_time: Dwell split time (ms)
 * @max_arr_size_used: Maximum array index used
 * @scan_count: Number of scans performed
 * @cancel_count: Number of scan cancellations
 * @complete_count: Number of completed scans
 */
struct ath12k_cbs {
	/* Associated radio and vif */
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;

	/* State machine */
	enum ath12k_cbs_state state;
	enum ath12k_cbs_event pending_event;

	/* Synchronization */
	spinlock_t lock;
	struct timer_list timer;
	struct work_struct work;
	struct work_struct cbs_scan_work;

	/* Scan parameters */
	u32 scan_id;
	struct wmi_scan_event *cbs_scan_event;
	u32 dwell_time;
	u32 rest_time;
	u32 wait_time;
	u32 scan_intvl_time;

	/* Channel list */
	u32 chan_list[ATH12K_CBS_MAX_CHANNELS];
	u32 nchans;
	u32 chan_list_idx;

	/* CBS configuration */
	u8 cbs_enable;

	/* Dwell split configuration */
	u8 max_dwell_split_cnt;
	s8 dwell_split_cnt;
	u32 scan_offset[ATH12K_CBS_SCAN_OFFSET_ARRAY_SIZE];
	u32 scan_dwell_rest[ATH12K_CBS_SCAN_OFFSET_ARRAY_SIZE];
	u32 min_dwell_rest_time;
	u32 cbs_total_dwell_time;
	u32 dwell_split_time;
	u8 max_arr_size_used;
	u8 scan_offset_idx;

	/* Statistics */
	u32 scan_count;
	u32 cancel_count;
	u32 complete_count;
};

/* Function declarations */

/* Initialization and cleanup */
int ath12k_cbs_init(struct ath12k *ar);
void ath12k_cbs_deinit(struct ath12k *ar);

/* CBS control */
void ath12k_cbs_stop(struct ath12k *ar);
void ath12k_cbs_stop_async(struct ath12k *ar);
int ath12k_cbs_start(struct ath12k *ar);
int ath12k_cbs_allow_mac80211_scan(struct ath12k *ar);
void ath12k_cbs_resume_post_mac80211_scan(struct ath12k *ar);
void ath12k_cbs_recovery_handler(struct ath12k *ar);

/* Event handling */
void ath12k_cbs_post_event(struct ath12k_cbs *cbs, enum ath12k_cbs_event event);
int ath12k_cbs_event_handler(struct ath12k_cbs *cbs, enum ath12k_cbs_event event);

/* Dwell configuration */
int ath12k_cbs_init_dwell_params(struct ath12k_cbs *cbs);

/* Scan event callback */
void ath12k_cbs_scan_event_handler(struct ath12k *ar, u32 scan_id,
					   u32 event_type);

#endif /* ATH12K_CBS_EXTN_H */
