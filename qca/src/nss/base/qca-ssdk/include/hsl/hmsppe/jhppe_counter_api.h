/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_COUNTER_API_H_
#define _JHPPE_COUNTER_API_H_


sw_error_t
jhppe_eg_drop_counter_get(
		a_uint32_t dev_id,
		union eg_drop_counter_u *value);

sw_error_t
jhppe_eg_drop_counter_set(
		a_uint32_t dev_id,
		union eg_drop_counter_u *value);

sw_error_t
jhppe_eg_drop_counter_pkt_num_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_eg_drop_counter_pkt_num_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
