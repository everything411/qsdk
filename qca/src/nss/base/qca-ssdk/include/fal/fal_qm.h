/*
 * Copyright (c) 2016-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_qos FAL_QM
 * @{
 */
#ifndef _FAL_QM_H_
#define _FAL_QM_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

#define FAL_QM_PROFILE_COMMON_ID		0
#define FAL_QM_PROFILE_PO_ID			15
#define FAL_QM_PROFILE_CPU_CODE_ID		14
#define FAL_QM_PROFILE_EIP_SERVCODE_ID		13
#define FAL_QM_PROFILE_PO_RSS_HASH_MAX		256
#define FAL_QM_PROFILE_PO_RSS_HASH_CLASS	0

typedef enum {
	FAL_AC_QUEUE = 0,
	FAL_AC_GROUP
} fal_ac_type_t;

typedef struct {
	fal_ac_type_t type;
	a_uint32_t obj_id;
} fal_ac_obj_t;

typedef struct {
	a_bool_t ac_en; /* 0 for disable and 1 for enable*/
	a_bool_t ac_fc_en; /* ac for flow control packets */
} fal_ac_ctrl_t;

typedef struct {
	a_uint16_t prealloc_buffer; /* guareented buffer number */
	a_uint16_t total_buffer; /* total buffer number */
} fal_ac_group_buffer_t;

typedef struct {
	a_bool_t color_enable; /* 1 for color aware and 0 for color-blind */
	a_bool_t wred_enable; /* 1 for wred and 0 for tail drop */
	a_uint16_t green_max;
	a_uint16_t green_min_off; /*gap between green max and green min*/
	a_uint16_t yel_max_off; /*gap between green max and yel max*/
	a_uint16_t yel_min_off; /*gap between green max and yel min*/
	a_uint16_t red_max_off; /*gap between green max and red max*/
	a_uint16_t red_min_off; /*gap between green max and red min*/
	a_uint16_t green_resume_off; /* green resume offset */
	a_uint16_t yel_resume_off; /* yellow resume offset */
	a_uint16_t red_resume_off; /* red resume offset */
	a_bool_t status; /* static threshold enabled or not */
} fal_ac_static_threshold_t;

typedef struct {
	a_bool_t color_enable; /* 1 for color aware and 0 for color-blind */
	a_bool_t wred_enable; /* 1 for wred and 0 for tail drop */
	a_uint8_t shared_weight; /* weight in the shared group */
	a_uint16_t green_min_off; /*gap between green max and green min*/
	a_uint16_t yel_max_off; /*gap between green max and yel max*/
	a_uint16_t yel_min_off; /*gap between green max and yel min*/
	a_uint16_t red_max_off; /*gap between green max and red max*/
	a_uint16_t red_min_off; /*gap between green max and red min*/
	a_uint16_t green_resume_off; /* green resume offset */
	a_uint16_t yel_resume_off; /* yellow resume offset */
	a_uint16_t red_resume_off; /* red resume offset */
	a_uint16_t ceiling; /*shared ceiling*/
	a_bool_t status; /* dynamic threshold enabled or not */
} fal_ac_dynamic_threshold_t;

typedef enum {
	FAL_ESRAM_QUEUE = 0, /* egress sram queue */
	FAL_ISRAM_QUEUE, /* ingress sram queue */
} fal_sram_queue_type_t;

typedef struct {
	a_uint8_t src_profile; /* queue source profile */
	a_bool_t service_code_en;
	a_uint16_t service_code;
	a_bool_t cpu_code_en;
	a_uint16_t cpu_code;
	fal_port_t dst_port; /* destination physical or VP port */
	fal_sram_queue_type_t sram_queue_type; /* esram queue or isram queue */
} fal_ucast_queue_dest_t;

typedef enum {
	FAL_ENQUEUE_FLOW = 0,
	FAL_ENQUEUE_VSI,
	FAL_ENQUEUE_SERVCODE,
	FAL_ENQUEUE_INVALID,
} fal_enqueue_type_t;

typedef struct {
	fal_enqueue_type_t enqueue_type; /* enqueue type: 0: flow, 1: vsi+dst_port, 2 servcode */
	a_uint8_t flow_pri_profile; /* priority profile from flow match entry */
	struct vsi_dest_t {
		a_uint32_t vsi; /* post vsi */
		a_uint32_t phy_port; /* dst port id */
	} vsi_dest;
	a_uint32_t dst_port; /* enable enqueue service code on dest port */
} fal_enqueue_rule_t;

typedef struct {
	a_bool_t enqueue_en; /* enqueue enable or not */
	fal_port_t enqueue_vport; /* enqueu vp id when enqueue type: flow or vsi */
	struct servcode_dest_t {
		a_uint32_t service_code; /* enqueue service code when enqueu type: service code */
		a_uint32_t phy_port; /* destination phy port when enqueue type: service code */
		a_bool_t queue_select_en;
		/* queue selection based on service code enabled or not,
		 * which is newly added for HMSPPE and JHPPE.
		 */
	} enqueue_servcode;
} fal_enqueue_index_t;

typedef struct {
	fal_enqueue_rule_t rule_entry; /* enqueue rule */
	fal_enqueue_index_t index_entry; /* index for egress queue */
} fal_enqueue_cfg_t;

#define FAL_QM_DROP_ITEMS	6
typedef struct {
	a_uint32_t tx_packets;
	a_uint64_t tx_bytes;
	a_uint32_t pending_buff_num;
	a_uint32_t drop_packets[FAL_QM_DROP_ITEMS];
	a_uint64_t drop_bytes[FAL_QM_DROP_ITEMS];
	a_uint32_t ddrq_packets;
	a_uint64_t ddrq_bytes;
	a_uint32_t rfc_block;
	a_uint32_t rfc_status;
} fal_queue_stats_t;

typedef enum {
	  FAL_SRAM_UNIQ = 0,
	  FAL_SRAM_MULTIQ,
} fal_qm_queue_type_t;

typedef struct {
      a_bool_t cnt_en;
      a_uint32_t cnt_id; /* means cnt id for unicast queue, means queue offset for multicast queue*/
} fal_qm_monitor_map_t;

typedef struct {
      a_bool_t cnt_threshold_mode; /* 0 is byte mode, 1 is pkt mode*/ 
      a_uint32_t cnt_threshold;
} fal_qm_monitor_ctrl_t;

typedef struct {
      a_uint32_t packets;
      a_uint32_t bytes;
      a_uint32_t peak_packets;
      a_uint32_t peak_bytes;
} fal_qm_monitor_stats_t;

typedef struct {
	a_bool_t valid;
	a_uint32_t tcont_id;
} fal_queue_tcont_cfg_t;

typedef struct {
	a_uint64_t bytes;
	a_uint32_t credit;
} fal_queue_tcont_stat_t;

typedef enum {
	FAL_PASSTHROUGH_MODE_192_128 = 0, /* 192 or 128 header passthrough mode */
	FAL_PASSTHROUGH_MODE_FULL_2ND_PASS, /* full packet 2nd passthrough mode */
	FAL_PASSTHROUGH_MODE_NO, /* no header passthrough mode */
	FAL_PASSTHROUGH_MODE_FULL, /* full packet passthrough mode */
} fal_passthrough_mode_t;

typedef struct {
	a_bool_t esramq_src_profile_en;
	a_bool_t isramq_src_profile_en;
	a_uint32_t src_profile;
} fal_passthrough_src_profile_t;

typedef struct {
	a_uint32_t qid_mismatch_cpucode; /* cpu code for ddrq qid mismatch */
	a_uint32_t drop_cpucode; /* cpu code for drop */
	a_uint32_t cpucode[2]; /* cpu code for trapped to cpu packet and multicast
				* traffic, selected by service code
				*/
} fal_passthrough_cpucode_t;

typedef struct {
	a_bool_t ddrq_en; /* enable ddrq */
	a_bool_t qid_mismatch_check_en; /* enable qid mismatch check for ddrq
					 * second and first pass
					 */
} fal_ucast_queue_ddrq_en_t;

typedef struct {
	a_uint32_t bmp[10];
} fal_bp_queue_bmp_t;

typedef struct {
	a_uint16_t green_resume_thrd; /* green resume threshold */
	a_uint16_t yel_resume_thrd; /* yellow resume threshold */
	a_uint16_t red_resume_thrd; /* red resume threshold */
	a_uint8_t green_drop; /* green drop state */
	a_uint8_t yel_drop; /* green drop state */
	a_uint8_t red_drop; /* red drop state */
} fal_ac_drop_state_t;

sw_error_t
fal_qm_tcont_set(a_uint32_t dev_id,
		 a_uint32_t queue_id,
		 fal_queue_tcont_cfg_t *cfg);

sw_error_t
fal_qm_tcont_get(a_uint32_t dev_id,
		 a_uint32_t queue_id,
		 fal_queue_tcont_cfg_t *cfg);

sw_error_t
fal_qm_tcont_stat_get(a_uint32_t dev_id,
		      a_uint32_t tcont_id,
		      fal_queue_tcont_stat_t *stat);

sw_error_t
fal_qm_tcont_stat_set(a_uint32_t dev_id,
		      a_uint32_t tcont_id,
		      fal_queue_tcont_stat_t *stat);

sw_error_t
fal_ac_ctrl_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_ctrl_t *cfg);

sw_error_t
fal_ac_ctrl_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_ctrl_t *cfg);

sw_error_t
fal_ac_prealloc_buffer_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		a_uint16_t num);

sw_error_t
fal_ac_prealloc_buffer_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		a_uint16_t *num);

sw_error_t
fal_ac_queue_group_set(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		a_uint8_t group_id);

sw_error_t
fal_ac_queue_group_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		a_uint8_t *group_id);

sw_error_t
fal_ac_static_threshold_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg);

sw_error_t
fal_ac_static_threshold_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg);

sw_error_t
fal_ac_dynamic_threshold_set(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg);

sw_error_t
fal_ac_dynamic_threshold_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg);

sw_error_t
fal_ac_group_buffer_set(
		a_uint32_t dev_id,
		a_uint8_t group_id,
		fal_ac_group_buffer_t *cfg);

sw_error_t
fal_ac_group_buffer_get(
		a_uint32_t dev_id,
		a_uint8_t group_id,
		fal_ac_group_buffer_t *cfg);

sw_error_t
fal_ucast_queue_base_profile_set(
		a_uint32_t dev_id,
		fal_ucast_queue_dest_t *queue_dest,
		a_uint32_t queue_base, a_uint8_t profile);

sw_error_t
fal_ucast_queue_base_profile_get(
		a_uint32_t dev_id,
		fal_ucast_queue_dest_t *queue_dest,
		a_uint32_t *queue_base, a_uint8_t *profile);

sw_error_t
fal_ucast_priority_class_set(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t priority,
		a_uint8_t class);

sw_error_t
fal_ucast_hash_map_set(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t rss_hash,
		a_int8_t queue_hash);

sw_error_t
fal_ucast_priority_class_get(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t priority,
		a_uint8_t *class);

sw_error_t
fal_ucast_hash_map_get(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t rss_hash,
		a_int8_t *queue_hash);

#if !defined(IN_QM_MINI)
sw_error_t
fal_mcast_cpu_code_class_set(
		a_uint32_t dev_id,
		a_uint8_t cpu_code,
		a_uint8_t queue_class);

sw_error_t
fal_mcast_cpu_code_class_get(
		a_uint32_t dev_id,
		a_uint8_t cpu_code,
		a_uint8_t *queue_class);
#endif

sw_error_t
fal_port_mcast_priority_class_set(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint8_t priority,
		a_uint8_t queue_class);

sw_error_t
fal_port_mcast_priority_class_get(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint8_t priority,
		a_uint8_t *queue_class);

sw_error_t
fal_queue_flush(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint16_t queue_id);

#if !defined(IN_QM_MINI)
sw_error_t
fal_ucast_default_hash_set(
		a_uint32_t dev_id,
		a_uint8_t hash_value);

sw_error_t
fal_ucast_default_hash_get(
		a_uint32_t dev_id,
		a_uint8_t *hash_value);
#endif

sw_error_t
fal_queue_counter_ctrl_set(a_uint32_t dev_id, a_bool_t cnt_en);

sw_error_t
fal_queue_counter_ctrl_get(a_uint32_t dev_id, a_bool_t *cnt_en);

sw_error_t
fal_queue_counter_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_queue_stats_t *info);

sw_error_t
fal_queue_counter_cleanup(a_uint32_t dev_id, a_uint32_t queue_id);

sw_error_t
fal_qm_enqueue_ctrl_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable);

sw_error_t
fal_qm_enqueue_ctrl_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable);

sw_error_t
fal_qm_port_source_profile_set(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint32_t src_profile);

sw_error_t
fal_qm_port_source_profile_get(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint32_t *src_profile);

sw_error_t
fal_qm_enqueue_config_set(a_uint32_t dev_id,
		fal_enqueue_cfg_t *enqueue_cfg);

sw_error_t
fal_qm_enqueue_config_get(a_uint32_t dev_id,
		fal_enqueue_cfg_t *enqueue_cfg);

sw_error_t
fal_qm_threshold_reset(a_uint32_t dev_id, a_uint32_t queue_id);

sw_error_t
fal_qm_counter_monitor_en_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_qm_counter_monitor_en_get(a_uint32_t dev_id, a_bool_t *enable);

sw_error_t
fal_qm_counter_monitor_stats_cleanup(a_uint32_t dev_id);

/* cnt_id means counter id for unicast queue, means port id for multicast queue type */
sw_error_t
fal_qm_counter_monitor_status_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, a_bool_t *status);

/* map_id means queue id for unicast queue type, means port id for multicast queue type */
sw_error_t
fal_qm_counter_monitor_map_set(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map);

sw_error_t
fal_qm_counter_monitor_map_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map);

/* cnt_id means counter id for unicast queue type, means port id for multicast queue type */
sw_error_t
fal_qm_counter_monitor_ctrl_set(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl);

sw_error_t
fal_qm_counter_monitor_ctrl_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl);

/* cnt_id means counter id for unicast queue type, means port id for multicast queue type */
sw_error_t
fal_qm_counter_monitor_stats_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_stats_t *monitor_stats);

sw_error_t
fal_qm_cpucode_enqueue_get(a_uint32_t dev_id, a_uint32_t cpu_code, a_bool_t *enable);

sw_error_t
fal_qm_cpucode_enqueue_set(a_uint32_t dev_id, a_uint32_t cpu_code, a_bool_t enable);

sw_error_t
fal_ucast_queue_ddrq_en_set(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_ucast_queue_ddrq_en_t *ddrqen);

sw_error_t
fal_ucast_queue_ddrq_en_get(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_ucast_queue_ddrq_en_t *ddrqen);

sw_error_t
fal_qm_passthrough_source_profile_set(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		fal_passthrough_src_profile_t *profile);

sw_error_t
fal_qm_passthrough_source_profile_get(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		fal_passthrough_src_profile_t *profile);

sw_error_t
fal_qm_passthrough_direct_enqueue_set(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		a_bool_t enable);

sw_error_t
fal_qm_passthrough_direct_enqueue_get(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		a_bool_t *enable);

sw_error_t
fal_qm_passthrough_cpucode_set(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode);

sw_error_t
fal_qm_passthrough_cpucode_get(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode);

sw_error_t
fal_qm_passthrough_cpucode_en_set(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		a_bool_t enable);

sw_error_t
fal_qm_passthrough_cpucode_en_get(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		a_bool_t *enable);

sw_error_t
fal_qm_dequeue_drop_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable);

sw_error_t
fal_qm_dequeue_drop_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable);

sw_error_t
fal_qm_crosschip_bp_qmap_set(a_uint32_t dev_id, a_uint32_t lp_qid,
			     fal_bp_queue_bmp_t *queue_bmp);

sw_error_t
fal_qm_crosschip_bp_qmap_get(a_uint32_t dev_id, a_uint32_t lp_qid,
			     fal_bp_queue_bmp_t *queue_bmp);

sw_error_t
fal_qm_crosschip_bp_status_get(a_uint32_t dev_id,
			       fal_bp_queue_bmp_t *queue_bmp);

sw_error_t fal_qm_ac_drop_state_set(a_uint32_t dev_id, fal_ac_obj_t *obj,
				    fal_ac_drop_state_t *cfg);
sw_error_t fal_qm_ac_drop_state_get(a_uint32_t dev_id, fal_ac_obj_t *obj,
				    fal_ac_drop_state_t *cfg);

sw_error_t
fal_qm_mcast_enqueue_ctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t ucast_enqueue_en);

sw_error_t
fal_qm_mcast_enqueue_ctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *ucast_enqueue_en);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _PORT_QM_H_ */
/**
 * @}
 */
