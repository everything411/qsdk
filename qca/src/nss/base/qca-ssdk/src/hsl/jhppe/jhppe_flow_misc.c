/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_flow_entry_host_op_ip_6tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
jhppe_flow_entry_host_op_ip_6tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
jhppe_flow_entry_host_op_ip_6tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
jhppe_flow_ip_6tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
jhppe_flow_ip_6tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
jhppe_flow_ip_6tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_6tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}
