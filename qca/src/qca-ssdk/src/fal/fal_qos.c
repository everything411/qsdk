/*
 * Copyright (c) 2012, 2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022,2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @defgroup fal_qos FAL_QOS
 * @{
 */
#include "sw.h"
#include "fal_qos.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_qos_sch_mode_set(a_uint32_t dev_id, fal_sch_mode_t mode, const a_uint32_t weight[])
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_sch_mode_set, dev_id, mode, weight)

sw_error_t fal_qos_sch_mode_get(a_uint32_t dev_id, fal_sch_mode_t * mode, a_uint32_t weight[])
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_sch_mode_get, dev_id, mode, weight)

sw_error_t fal_qos_queue_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_queue_tx_buf_status_set, dev_id, port_id, enable)

sw_error_t fal_qos_queue_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_queue_tx_buf_status_get, dev_id, port_id, enable)

sw_error_t fal_qos_queue_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id, fal_queue_t queue_id, a_uint32_t * number)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_queue_tx_buf_nr_get, dev_id, port_id, queue_id, number)

sw_error_t fal_qos_port_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_tx_buf_status_set, dev_id, port_id, enable)

sw_error_t fal_qos_port_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_tx_buf_status_get, dev_id, port_id, enable)

sw_error_t fal_qos_port_red_en_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t* enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_red_en_get, dev_id, port_id, enable)

sw_error_t fal_qos_port_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * number)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_tx_buf_nr_get, dev_id, port_id, number)

sw_error_t fal_qos_port_rx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * number, a_uint32_t * react_num)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_rx_buf_nr_get, dev_id, port_id, number, react_num)

sw_error_t fal_cosmap_up_queue_set(a_uint32_t dev_id, a_uint32_t up, fal_queue_t queue)
    DEFINE_FAL_FUNC_HSL_EXPORT(cosmap_up_queue_set, dev_id, up, queue)

sw_error_t fal_cosmap_up_queue_get(a_uint32_t dev_id, a_uint32_t up, fal_queue_t * queue)
    DEFINE_FAL_FUNC_HSL_EXPORT(cosmap_up_queue_get, dev_id, up, queue)

sw_error_t fal_cosmap_dscp_queue_set(a_uint32_t dev_id, a_uint32_t dscp, fal_queue_t queue)
    DEFINE_FAL_FUNC_HSL_EXPORT(cosmap_dscp_queue_set, dev_id, dscp, queue)

sw_error_t fal_cosmap_dscp_queue_get(a_uint32_t dev_id, a_uint32_t dscp, fal_queue_t * queue)
    DEFINE_FAL_FUNC_HSL_EXPORT(cosmap_dscp_queue_get, dev_id, dscp, queue)

sw_error_t fal_qos_queue_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id, fal_queue_t queue_id, a_uint32_t * number)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_queue_tx_buf_nr_set, dev_id, port_id, queue_id, number)

sw_error_t fal_qos_port_red_en_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_red_en_set, dev_id, port_id, enable)

sw_error_t fal_qos_port_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * number)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_tx_buf_nr_set, dev_id, port_id, number)

sw_error_t fal_qos_port_rx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * number, a_uint32_t * react_num)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_rx_buf_nr_set, dev_id, port_id, number, react_num)

sw_error_t fal_qos_port_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_qos_mode_t mode, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_mode_set, dev_id, port_id, mode, enable)

sw_error_t fal_qos_port_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_mode_t mode, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_mode_get, dev_id, port_id, mode, enable)

sw_error_t fal_qos_port_mode_pri_set(a_uint32_t dev_id, fal_port_t port_id, fal_qos_mode_t mode, a_uint32_t pri)
    DEFINE_FAL_FUNC_EXPORT(qos_port_mode_pri_set, dev_id, port_id, mode, pri)

sw_error_t fal_qos_port_mode_pri_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_mode_t mode, a_uint32_t * pri)
    DEFINE_FAL_FUNC_EXPORT(qos_port_mode_pri_get, dev_id, port_id, mode, pri)

sw_error_t fal_qos_port_default_up_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t up)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_default_up_set, dev_id, port_id, up)

sw_error_t fal_qos_port_default_up_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * up)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_default_up_get, dev_id, port_id, up)

sw_error_t fal_qos_port_sch_mode_set(a_uint32_t dev_id, a_uint32_t port_id, fal_sch_mode_t mode, const a_uint32_t weight[])
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_sch_mode_set, dev_id, port_id, mode, weight)

sw_error_t fal_qos_port_sch_mode_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sch_mode_t * mode, a_uint32_t weight[])
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_sch_mode_get, dev_id, port_id, mode, weight)

sw_error_t fal_qos_port_default_spri_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t spri)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_default_spri_set, dev_id, port_id, spri)

sw_error_t fal_qos_port_default_spri_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * spri)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_default_spri_get, dev_id, port_id, spri)

sw_error_t fal_qos_port_default_cpri_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t cpri)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_default_cpri_set, dev_id, port_id, cpri)

sw_error_t fal_qos_port_default_cpri_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * cpri)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_default_cpri_get, dev_id, port_id, cpri)

sw_error_t fal_qos_port_force_spri_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_force_spri_status_set, dev_id, port_id, enable)

sw_error_t fal_qos_port_force_spri_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t* enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_force_spri_status_get, dev_id, port_id, enable)

sw_error_t fal_qos_port_force_cpri_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_force_cpri_status_set, dev_id, port_id, enable)

sw_error_t fal_qos_port_force_cpri_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t* enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_force_cpri_status_get, dev_id, port_id, enable)

sw_error_t fal_qos_queue_remark_table_set(a_uint32_t dev_id, fal_port_t port_id, fal_queue_t queue_id, a_uint32_t tbl_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_queue_remark_table_set, dev_id, port_id, queue_id, tbl_id, enable)

sw_error_t fal_qos_queue_remark_table_get(a_uint32_t dev_id, fal_port_t port_id, fal_queue_t queue_id, a_uint32_t * tbl_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_queue_remark_table_get, dev_id, port_id, queue_id, tbl_id, enable)

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

#ifndef IN_QOS_MINI
sw_error_t fal_qos_cosmap_pcp_get(a_uint32_t dev_id, a_uint8_t group_id, a_uint8_t pcp, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_pcp_get, dev_id, group_id, pcp, cosmap)

sw_error_t fal_qos_cosmap_pcp_set(a_uint32_t dev_id, a_uint8_t group_id, a_uint8_t pcp, fal_qos_cosmap_t *cosmap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_cosmap_pcp_set, dev_id, group_id, pcp, cosmap)

sw_error_t fal_qos_port_remark_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_remark_enable_t *remark)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_port_remark_get, dev_id, port_id, remark)

sw_error_t fal_qos_port_remark_set(a_uint32_t dev_id, fal_port_t port_id, fal_qos_remark_enable_t *remark)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qos_port_remark_set, dev_id, port_id, remark)

sw_error_t fal_port_queues_get(a_uint32_t dev_id, fal_port_t port_id, fal_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_queues_get, dev_id, port_id, queue_bmp)

sw_error_t fal_reservedpool_scheduler_resource_get( a_uint32_t dev_id, fal_portscheduler_resource_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(reservedpool_scheduler_resource_get, dev_id, cfg)
#endif

