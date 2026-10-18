/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_HTTPPE_FDB_H_
#define _ADPT_HTTPPE_FDB_H_

sw_error_t
adpt_httppe_fdb_vport_maclimit_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_maclimit_ctrl_t * maclimit_ctrl);
sw_error_t
adpt_httppe_fdb_vport_maclimit_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_maclimit_ctrl_t * maclimit_ctrl);
sw_error_t
adpt_httppe_vport_fdb_learn_limit_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable, a_uint32_t cnt);
sw_error_t
adpt_httppe_vport_fdb_learn_limit_get(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t * enable, a_uint32_t * cnt);
sw_error_t
adpt_httppe_fdb_vport_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);
sw_error_t
adpt_httppe_fdb_vport_learn_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable);
sw_error_t
adpt_httppe_fdb_vport_newaddr_lrn_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable,
	fal_fwd_cmd_t cmd);
sw_error_t
adpt_httppe_fdb_vport_newaddr_lrn_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable,
	fal_fwd_cmd_t *cmd);
sw_error_t
adpt_httppe_fdb_vport_stamove_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable,
	fal_fwd_cmd_t cmd);
sw_error_t
adpt_httppe_fdb_vport_stamove_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable,
	fal_fwd_cmd_t *cmd);
sw_error_t
adpt_httppe_vport_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_fwd_cmd_t cmd);
sw_error_t
adpt_httppe_vport_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_fwd_cmd_t * cmd);
#endif
