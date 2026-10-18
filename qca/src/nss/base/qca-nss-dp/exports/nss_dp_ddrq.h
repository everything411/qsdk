/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file nss_dp_ddrq.h
 * 	NSS-DP's DDRQ feature's exported structure/APIs
 */

#ifndef __NSS_DP_DDRQ_H__
#define __NSS_DP_DDRQ_H__

/*
 * DDRQ invalid value
 */
#define NSS_DP_DDRQ_INV_VAL	-1

/*
 * DDRQ maximum supported count
 */
#define NSS_DP_DDRQ_MAX_CNT	160

/**
 * nss_dp_ddrq_ret
 *	DDRQ API's return status
 */
typedef enum nss_dp_ddrq_ret {
	DDRQ_RET_SUCCESS = 0,		/**< Success */
	DDRQ_RET_BUSY,			/**< Busy */
	DDRQ_RET_ERR,			/**< Error */
	DDRQ_RET_INVAL,			/**< Invalid */
} nss_dp_ddrq_ret_t;

/**
 * nss_dp_ddrq_cfg_type
 *	DDRQ input type values
 */
typedef enum nss_dp_ddrq_cfg_type {
	NSS_DP_DDRQ_CFG_TYPE_NONE = 0,		/**< Configuration value is not valid */
	NSS_DP_DDRQ_CFG_TYPE_QUEUE,		/**< Configuration value is a queue id */
	NSS_DP_DDRQ_CFG_TYPE_PORT,		/**< Configuration value is a port value */
	NSS_DP_DDRQ_CFG_TYPE_LP,		/**< Configuration value is a loopback id */
} nss_dp_ddrq_cfg_type_t;


/**
 * nss_dp_ddrq_occupancy_stats
 *	DDRQ occupancy statistics structure
 */
typedef struct nss_dp_ddrq_occupancy_stats {
	uint32_t pkts;			/**< DDRQ packet count */
	uint32_t peak_pkts;		/**< DDRQ peak packet count */
	uint64_t bytes;			/**< DDRQ byte count */
	uint64_t peak_bytes;		/**< DDRQ peak byte count */
} nss_dp_ddrq_occupancy_stats_t;

/**
 * nss_dp_ddrq_occupancy_threshold
 *	DDRQ occupancy threshold structure
 */
typedef struct nss_dp_ddrq_occupancy_threshold {
	int64_t threshold_val;		/**< DDRQ occupancy threshold value */
	int8_t threshold_type;		/**< DDRQ occupancy threshold type */
} nss_dp_ddrq_occupancy_threshold_t;

/**
 * nss_dp_ddrq_obj_id
 *	DDRQ configuration structure
 */
typedef struct nss_dp_ddrq_obj_id {
	nss_dp_ddrq_cfg_type_t cfg_type;	/**< Configuration value's type */
	uint32_t cfg_id;			/**< Configuration value */
} nss_dp_ddrq_obj_id_t;

/**
 * nss_dp_ddrq_ac_queue_cfg_tbl
 *	DDRQ AC queue configuration structure
 */
typedef struct nss_dp_ddrq_ac_queue_cfg_tbl {
	int16_t  ac_cfg_gap_grn_grn_min;	/**< Gap between green max to green min */
	int16_t  ac_cfg_gap_grn_red_max;	/**< Gap between green max to red max */
	int16_t  ac_cfg_gap_grn_red_min;	/**< Gap between green max to red min */
	int16_t  ac_cfg_gap_grn_yel_max;	/**< Gap between green max to yellow max */
	int16_t  ac_cfg_gap_grn_yel_min;	/**< Gap between green max to yellow min */
	int16_t  ac_cfg_grn_resume_offset;	/**< Green resume offset */
	int16_t  ac_cfg_pre_alloc_limit;	/**< DDRQ's static buffer limit */
	int16_t  ac_cfg_red_resume_offset;	/**< Red resume offset */
	int16_t  ac_cfg_shared_ceiling;		/**< Shared ceiling */
	int16_t  ac_cfg_yel_resume_offset;	/**< Yellow resume offset */
	int8_t  ac_cfg_ac_en;			/**< Admission Control knob */
	int8_t  ac_cfg_bp_en;			/**< Backpressure knob */
	int8_t  ac_cfg_color_aware;		/**< DDRQ color aware knob */
	int8_t  ac_cfg_ecn_mark_en;		/**< ECN marking knob */
	int8_t  ac_cfg_grp_id;			/**< DDRQ group id */
	int8_t  ac_cfg_shared_dynamic;		/**< Shared static or dynamic*/
	int8_t  ac_cfg_shared_weight;		/**< Shared weight */
	int8_t  ac_cfg_wred_en;			/**< WRED knob */
	int8_t  ddrq_state;			/**< DDRQ enable/disable knob */
} nss_dp_ddrq_ac_queue_cfg_tbl_t;

/**
 * nss_dp_ddrq_ac_grp_cfg_tbl
 *	DDRQ AC group configuration structure
 */
typedef struct nss_dp_ddrq_ac_grp_cfg_tbl {
	int16_t  ac_grp_dp_thrd;		/**< DDRQ group drop threshold */
	int16_t  ac_grp_gap_grn_red;		/**< DDRQ group's gap between green and red */
	int16_t  ac_grp_gap_grn_yel;		/**< DDRQ group's gap between green and yellow */
	int16_t  ac_grp_grn_resume_offset;	/**< DDRQ group green resume offset */
	int16_t  ac_grp_red_resume_offset;	/**< DDRQ group red resume offset */
	int16_t  ac_grp_gap_shrd_limit;		/**< DDRQ group shared threshold limit */
	int16_t  ac_grp_yel_resume_offset;	/**< DDRQ group yellow resume offset */
	int8_t  ac_cfg_ac_en;			/**< DDRQ group admission control knob */
	int8_t  ac_cfg_color_aware;		/**< DDRQ group color aware knob */
} nss_dp_ddrq_ac_grp_cfg_tbl_t;

/**
 * nss_dp_ddrq_cfg_get
 *	DDRQ API to get the AC queue configuration values
 *
 * @datatypes
 * nss_dp_ddrq_obj_id_t
 * nss_dp_ddrq_ac_queue_cfg_tbl_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] obj DDRQ object pointer
 * @param[in] ddrq_cfg Pointer to the structure to get the DDRQ AC queue configuration
 * @param[in] count Count for number of DDRQ's configuration requested
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_cfg_get(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg, uint32_t count);

/**
 * nss_dp_ddrq_cfg_set
 *	DDRQ API to set the AC queue configuration values
 *
 * @datatypes
 * nss_dp_ddrq_obj_id_t
 * nss_dp_ddrq_ac_queue_cfg_tbl_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] obj DDRQ object pointer
 * @param[in] ddrq_cfg Pointer to the structure to set the DDRQ AC queue configuration
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_cfg_set(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg);

/**
 * nss_dp_ddrq_grp_cfg_get
 *	DDRQ API to get the AC group configuration values
 *
 * @datatypes
 * nss_dp_ddrq_ac_grp_cfg_tbl_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] ddrq_grp_id DDRQ's group id
 * @param[in] ddrq_grp_cfg Pointer to the structure to get the DDRQ AC group configuration
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_grp_cfg_get(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg);

/**
 * nss_dp_ddrq_grp_cfg_set
 *	DDRQ API to set the AC group configuration values
 *
 * @datatypes
 * nss_dp_ddrq_ac_grp_cfg_tbl_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] ddrq_grp_id DDRQ's group id
 * @param[in] ddrq_grp_cfg Pointer to the structure to set the DDRQ AC group configuration
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_grp_cfg_set(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg);

/**
 * nss_dp_ddrq_enqueue_disable
 *	DDRQ API for enqueue disable
 *
 * @datatypes
 * nss_dp_ddrq_obj_id_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] obj DDRQ object pointer
 * @param[in] disable Knob for enqueue disable set/reset
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_enqueue_disable(nss_dp_ddrq_obj_id_t *obj, bool disable);

/**
 * nss_dp_ddrq_dequeue_drop
 *	DDRQ API for dequeue drop
 *
 * @datatypes
 * nss_dp_ddrq_obj_id_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] obj DDRQ object pointer
 * @param[in] disable Knob for dequeue drop disable set/reset
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_dequeue_drop(nss_dp_ddrq_obj_id_t *obj, bool drop);

/**
 * nss_dp_ddrq_occupancy_stats_reset
 *	API to reset DDRQ occupancy statistics
 *
 * @datatypes
 * nss_dp_ddrq_ret_t
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_reset(void);

/**
 * nss_dp_ddrq_occupancy_stats_start
 *	API to start DDRQ occupancy statistic's capture
 *
 * @datatypes
 * nss_dp_ddrq_ret_t
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_start (void);

/**
 * nss_dp_ddrq_occupancy_stats_stop
 *	API to stop DDRQ occupancy statistic's capture
 *
 * @datatypes
 * nss_dp_ddrq_ret_t
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_stop (void);

/**
 * nss_dp_ddrq_occupancy_stats_restart
 *	API to restart DDRQ occupancy statistic's capture
 *
 * @datatypes
 * nss_dp_ddrq_ret_t
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_restart (void);

/**
 * nss_dp_ddrq_occupancy_stats_threshold_set
 *	API to set the DDRQ occupancy threshold value
 *
 * @datatypes
 * nss_dp_ddrq_occupancy_threshold_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] ddrq_id DDRQ id
 * @param[in] Pointer to the threshold DDRQ threshold value to be set
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_threshold_set(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold);

/**
 * nss_dp_ddrq_occupancy_stats_threshold_get
 *	API to get the DDRQ occupancy threshold value
 *
 * @datatypes
 * nss_dp_ddrq_occupancy_threshold_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] ddrq_id DDRQ id
 * @param[in] threshold DDRQ threshold pointer to get value in
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_threshold_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold);

/**
 * nss_dp_ddrq_occupancy_stats_get
 *	API to get the DDRQ occupancy statistics
 *
 * @datatypes
 * nss_dp_ddrq_occupancy_stats_t
 * nss_dp_ddrq_ret_t
 *
 * @param[in] ddrq_id DDRQ id
 * @param[in] ddrq_stats Pointer to the DDRQ occupancy statistics to get the value in
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_stats_t *ddrq_stats);

/**
 * nss_dp_ddrq_occupancy_stats_status_get
 *	API to get the DDRQ occupancy statistic's status
 *
 * @datatypes
 * nss_dp_ddrq_ret_t
 *
 * @param[in] ddrq_id DDRQ id
 * @param[in] status Pointer to get the status of the DDRQ occupancy statistics
 *
 * @return
 * API's completion's status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_status_get(uint32_t ddrq_id, bool *status);

#endif	/** __NSS_DP_DDRQ_H__ */
