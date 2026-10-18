/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_HTTPPE_CTRLPKT_H_
#define _ADPT_HTTPPE_CTRLPKT_H_
sw_error_t
adpt_httppe_mgmtctrl_vpgroup_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t vpgroup_id);

sw_error_t
adpt_httppe_mgmtctrl_vpgroup_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t *vpgroup_id);

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_add(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt);

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_del(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt);

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_getfirst(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt);

sw_error_t
adpt_httppe_mgmtctrl_ctrlpkt_profile_getnext(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt);
#endif
