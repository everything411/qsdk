/*
 * Copyright (c) 2012, 2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_qos FAL_QOS
 * @{
 */
#include "sw.h"
#include "fal_qos.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_qos_port_pri_precedence_set(a_uint32_t dev_id, fal_port_t port_id, fal_qos_pri_precedence_t *pri)
    DEFINE_FAL_FUNC_ADPT(qos_port_pri_set, dev_id, port_id, pri)
    EXPORT_SYMBOL(fal_qos_port_pri_precedence_set);

sw_error_t fal_qos_port_pri_precedence_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_pri_precedence_t *pri)
    DEFINE_FAL_FUNC_ADPT(qos_port_pri_get, dev_id, port_id, pri)
    EXPORT_SYMBOL(fal_qos_port_pri_precedence_get);

sw_error_t fal_queue_scheduler_set(a_uint32_t dev_id, a_uint32_t node_id, fal_queue_scheduler_level_t level, fal_port_t port_id, fal_qos_scheduler_cfg_t *scheduler_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_scheduler_set, dev_id, node_id, level, port_id, scheduler_cfg)

sw_error_t fal_queue_scheduler_get(a_uint32_t dev_id, a_uint32_t node_id, fal_queue_scheduler_level_t level, fal_port_t *port_id, fal_qos_scheduler_cfg_t *scheduler_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_scheduler_get, dev_id, node_id, level, port_id, scheduler_cfg)

sw_error_t fal_qos_cosmap_dscp_get(a_uint32_t dev_id, a_uint8_t group_id, a_uint8_t dscp, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_dscp_get, dev_id, group_id, dscp, cosmap)

sw_error_t fal_qos_cosmap_flow_set(a_uint32_t dev_id, a_uint8_t group_id, a_uint16_t flow, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_flow_set, dev_id, group_id, flow, cosmap)

sw_error_t fal_qos_port_group_set(a_uint32_t dev_id, fal_port_t port_id, fal_qos_group_t *group)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_port_group_set, dev_id, port_id, group)

sw_error_t fal_edma_ring_queue_map_set(a_uint32_t dev_id, a_uint32_t ring_id, fal_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT(ring_queue_map_set, dev_id, ring_id, queue_bmp)
    EXPORT_SYMBOL(fal_edma_ring_queue_map_set);

sw_error_t fal_qos_cosmap_dscp_set(a_uint32_t dev_id, a_uint8_t group_id, a_uint8_t dscp, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_dscp_set, dev_id, group_id, dscp, cosmap)

sw_error_t fal_qos_cosmap_flow_get(a_uint32_t dev_id, a_uint8_t group_id, a_uint16_t flow, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_flow_get, dev_id, group_id, flow, cosmap)

sw_error_t fal_qos_port_group_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_group_t *group)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_port_group_get, dev_id, port_id, group)

sw_error_t fal_edma_ring_queue_map_get(a_uint32_t dev_id, a_uint32_t ring_id, fal_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT(ring_queue_map_get, dev_id, ring_id, queue_bmp)
    EXPORT_SYMBOL(fal_edma_ring_queue_map_get);

sw_error_t fal_scheduler_dequeue_ctrl_set( a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(scheduler_dequeue_ctrl_set, dev_id, queue_id, enable)

sw_error_t fal_scheduler_dequeue_ctrl_get( a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(scheduler_dequeue_ctrl_get, dev_id, queue_id, enable)

sw_error_t fal_port_scheduler_cfg_reset( a_uint32_t dev_id, fal_port_t port_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_scheduler_cfg_reset, dev_id, port_id)

sw_error_t fal_port_scheduler_resource_get( a_uint32_t dev_id, fal_port_t port_id, fal_portscheduler_resource_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_scheduler_resource_get, dev_id, port_id, cfg)

sw_error_t fal_qos_port_pcp_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_qos_pcp_cfg_t *pcp_cfg)
    DEFINE_FAL_FUNC_ADPT(qos_port_pcp_cfg_set, dev_id, port_id, pcp_cfg)
sw_error_t fal_qos_port_pcp_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_pcp_cfg_t *pcp_cfg)
    DEFINE_FAL_FUNC_ADPT(qos_port_pcp_cfg_get, dev_id, port_id, pcp_cfg)

sw_error_t fal_qos_cosmap_pcp_get(a_uint32_t dev_id, a_uint8_t group_id, a_uint8_t pcp, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_pcp_get, dev_id, group_id, pcp, cosmap)

sw_error_t fal_qos_cosmap_pcp_set(a_uint32_t dev_id, a_uint8_t group_id, a_uint8_t pcp, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_pcp_set, dev_id, group_id, pcp, cosmap)

#ifndef IN_QOS_MINI
sw_error_t fal_port_queues_get(a_uint32_t dev_id, fal_port_t port_id, fal_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_queues_get, dev_id, port_id, queue_bmp)

sw_error_t fal_reservedpool_scheduler_resource_get( a_uint32_t dev_id, fal_portscheduler_resource_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(reservedpool_scheduler_resource_get, dev_id, cfg)
#endif

