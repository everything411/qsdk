/*
 * Copyright (c) 2012, 2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_qos_legacy FAL_QOS_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_qos_legacy.h"
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
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_mode_pri_set, dev_id, port_id, mode, pri)

sw_error_t fal_qos_port_mode_pri_get(a_uint32_t dev_id, fal_port_t port_id, fal_qos_mode_t mode, a_uint32_t * pri)
    DEFINE_FAL_FUNC_HSL_EXPORT(qos_port_mode_pri_get, dev_id, port_id, mode, pri)

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

/**
 * @}
 */
