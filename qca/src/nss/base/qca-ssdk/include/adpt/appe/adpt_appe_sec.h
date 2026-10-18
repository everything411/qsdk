/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_APPE_SEC_H_
#define _ADPT_APPE_SEC_H_

#ifndef IN_SEC_MINI
sw_error_t
adpt_appe_sec_l2_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl);

sw_error_t
adpt_appe_sec_l2_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl);
#endif

sw_error_t
adpt_appe_sec_tunnel_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl);

#ifndef IN_SEC_MINI
sw_error_t
adpt_appe_sec_tunnel_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl);

sw_error_t
adpt_appe_sec_tunnel_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);

sw_error_t
adpt_appe_sec_tunnel_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);
#endif

sw_error_t
adpt_appe_sec_tunnel_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

#ifndef IN_SEC_MINI
sw_error_t
adpt_appe_sec_tunnel_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

sw_error_t
adpt_appe_sec_tunnel_flags_excep_parser_ctrl_set(a_uint32_t dev_id, a_uint32_t entry_index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl);

sw_error_t
adpt_appe_sec_tunnel_flags_excep_parser_ctrl_get(a_uint32_t dev_id, a_uint32_t entry_index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl);
#endif

#endif

