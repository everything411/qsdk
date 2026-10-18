/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_HTTPPE_MISC_
#define _ADPT_HTTPPE_MISC_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

void
adpt_httppe_debug_queue_tx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count);

void
adpt_httppe_queue_tx_counter_tbl_set(a_uint32_t dev_id);

void
adpt_httppe_debug_drop_cpu_counter_get(a_uint32_t dev_id, a_bool_t show_type,
	char **buf, ssize_t *count, a_uint32_t sd_size);

void
adpt_httppe_debug_drop_cpu_counter_set(a_uint32_t dev_id);

void
adpt_httppe_debug_eg_vsi_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count);

void
adpt_httppe_debug_port_tx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count);

sw_error_t
adpt_httppe_eg_vsi_counter_tbl_set(a_uint32_t dev_id);

sw_error_t
adpt_httppe_port_tx_counter_tbl_set(a_uint32_t dev_id);

void
adpt_httppe_debug_vp_tx_counter_get(a_uint32_t dev_id, a_bool_t show_type, char **buf, ssize_t *count);

sw_error_t
adpt_httppe_vp_tx_counter_tbl_set(a_uint32_t dev_id);

 #ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
