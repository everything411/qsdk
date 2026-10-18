/*
 * Copyright (c) 2012, 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_qos FAL_QOS
 * @{
 */
#ifndef _FAL_QOS_H_
#define _FAL_QOS_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"
#include "fal_qos_legacy.h"

typedef struct {
	a_uint8_t pcp_group; /* 0: group 0 1: group 1 */
	a_uint8_t dscp_group;
	a_uint8_t flow_group;
} fal_qos_group_t;

typedef struct {
	a_uint8_t pcp_pri; /* pcp precedence */
	a_uint8_t dscp_pri; /* dscp precedence */
	a_int8_t preheader_pri; /* preheader precedence */
	a_uint8_t flow_pri; /* flow precedence */
	a_uint8_t acl_pri; /* acl pre routing precedecne */
	a_uint8_t post_acl_pri; /* acl post routing precedence */
	a_bool_t pcp_pri_force; /* pcp priority force enabled or not */
	a_bool_t dscp_pri_force; /* dscp priority force enabled or not */
	a_uint8_t pre_acl_outer_pri; /* pre-acl for tunnel outter packet precedence,
				      * added for ipq95xx*/
	a_uint8_t pre_acl_inner_pri; /* pre-acl for tunnel inner packet precedence,
				      * added for ipq95xx*/
} fal_qos_pri_precedence_t;

typedef struct {
	a_uint8_t internal_pcp;
	a_uint8_t internal_dei;
	a_uint8_t internal_pri;
	a_uint8_t internal_dscp;
	a_uint8_t internal_dp;
	a_uint8_t dscp_mask;
	a_bool_t dscp_en;
	a_bool_t pcp_en;
	a_bool_t dei_en;
	a_bool_t pri_en;
	a_bool_t dp_en;
	a_uint8_t qos_prec; /* resolution precedence */
} fal_qos_cosmap_t;

typedef enum {
	FAL_DRR_IPG_PREAMBLE_FRAME_CRC = 0, /* IPG + Preamble + Frame + CRC */
	FAL_DRR_FRAME_CRC, /* Frame + CRC */
	FAL_DRR_L3_EXCLUDE_CRC  /* after Ethernet type excude CRC*/
} fal_qos_drr_frame_mode_t;

typedef struct {
	a_uint8_t sp_id; /* SP id L0:0~63 L1:0~7 */
	a_uint8_t e_pri; /*SP priority for E path:0~7 low to high */
	a_uint8_t c_pri; /* SP priority for C path: 0~7 low to high */
	a_uint8_t c_drr_id; /*C DRR ID L0:0~159 L1:0~35*/
	a_uint8_t e_drr_id; /*E DRR ID L0:0~159 L1:0~35*/
	a_uint16_t e_drr_wt; /* DRR weight in E DRR: 0~1023 */
	a_uint16_t c_drr_wt; /* DRR weight in C DRR: 0~1023 */
	a_uint8_t c_drr_unit; /* 0:byte based; 1:packet based */
	a_uint8_t e_drr_unit; /* 0:byte based; 1:packet based */
	fal_qos_drr_frame_mode_t drr_frame_mode;
} fal_qos_scheduler_cfg_t;

typedef struct {
	a_uint32_t en_scheduler_port_bmp; /* port bitmap of en-scheduler */
	a_uint32_t en_scheduler_port; /* port of en-scheduler */
	a_uint32_t de_scheduler_port; /* port of de-shceduler */
	a_uint32_t de_scheduler_2nd_port;
	a_bool_t de_scheduler_2nd_port_en;
} fal_port_scheduler_cfg_t;

typedef enum {
	FAL_QUEUE_SCHEDULER_LEVEL0 = 0,
	FAL_QUEUE_SCHEDULER_LEVEL1,
} fal_queue_scheduler_level_t;


typedef struct {
	a_uint32_t bmp[10];
} fal_queue_bmp_t;

typedef struct {
	a_uint16_t ucastq_start;
	a_uint16_t ucastq_num;
	a_uint16_t mcastq_start;
	a_uint16_t mcastq_num;
	a_uint16_t l0sp_start;
	a_uint16_t l0sp_num;
	a_uint16_t l0cdrr_start;
	a_uint16_t l0cdrr_num;
	a_uint16_t l0edrr_start;
	a_uint16_t l0edrr_num;
	a_uint16_t l1sp_start;
	a_uint16_t l1sp_num;
	a_uint16_t l1cdrr_start;
	a_uint16_t l1cdrr_num;
	a_uint16_t l1edrr_start;
	a_uint16_t l1edrr_num;
	a_uint16_t max_pri;
} fal_portscheduler_resource_t;

typedef struct {
	a_uint8_t pcp_mode;
	a_uint8_t default_pcp_dei; /* untag pcp + dei value */
} fal_qos_pcp_cfg_t;

#define FAL_DOT1P_MIN    0
#define FAL_DOT1P_MAX    7

#define FAL_DSCP_MIN     0
#define FAL_DSCP_MAX     63



sw_error_t
fal_qos_port_group_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_qos_group_t *group);

sw_error_t
fal_qos_port_group_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_qos_group_t *group);

sw_error_t
fal_qos_port_pri_precedence_set(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_pri_precedence_t *pri);

sw_error_t
fal_qos_port_pri_precedence_get(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_pri_precedence_t *pri);

sw_error_t
fal_qos_cosmap_pcp_set(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t pcp, fal_qos_cosmap_t *cosmap);

sw_error_t
fal_qos_cosmap_pcp_get(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t pcp, fal_qos_cosmap_t *cosmap);

sw_error_t
fal_qos_cosmap_flow_set(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint16_t flow, fal_qos_cosmap_t *cosmap);

sw_error_t
fal_qos_cosmap_flow_get(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint16_t flow, fal_qos_cosmap_t *cosmap);

sw_error_t
fal_qos_cosmap_dscp_set(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t dscp, fal_qos_cosmap_t *cosmap);

sw_error_t
fal_qos_cosmap_dscp_get(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t dscp, fal_qos_cosmap_t *cosmap);


sw_error_t
fal_queue_scheduler_set(a_uint32_t dev_id,
					a_uint32_t node_id, fal_queue_scheduler_level_t level,
					fal_port_t port_id,
					fal_qos_scheduler_cfg_t *scheduler_cfg);

sw_error_t
fal_queue_scheduler_get(a_uint32_t dev_id,
					a_uint32_t node_id, fal_queue_scheduler_level_t level,
					fal_port_t *port_id,
					fal_qos_scheduler_cfg_t *scheduler_cfg);

sw_error_t
fal_edma_ring_queue_map_get(a_uint32_t dev_id, 
					a_uint32_t ring_id, fal_queue_bmp_t *queue_bmp);

sw_error_t
fal_edma_ring_queue_map_set(a_uint32_t dev_id, 
					a_uint32_t ring_id, fal_queue_bmp_t *queue_bmp);
#ifndef IN_QOS_MINI
sw_error_t
fal_port_queues_get(a_uint32_t dev_id, 
				fal_port_t port_id, fal_queue_bmp_t *queue_bmp);
#endif

sw_error_t
fal_scheduler_dequeue_ctrl_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable);

sw_error_t
fal_scheduler_dequeue_ctrl_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable);

sw_error_t
fal_port_scheduler_cfg_reset(
		a_uint32_t dev_id,
		fal_port_t port_id);

sw_error_t
fal_port_scheduler_resource_get(
		a_uint32_t dev_id,
		fal_port_t port_id,
		fal_portscheduler_resource_t *cfg);

#ifndef IN_QOS_MINI
sw_error_t
fal_reservedpool_scheduler_resource_get(
		a_uint32_t dev_id,
		fal_portscheduler_resource_t *cfg);
#endif

sw_error_t
fal_qos_port_pcp_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_qos_pcp_cfg_t *pcp_cfg);

sw_error_t
fal_qos_port_pcp_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_qos_pcp_cfg_t *pcp_cfg);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _PORT_QOS_H_ */
/**
 * @}
 */
