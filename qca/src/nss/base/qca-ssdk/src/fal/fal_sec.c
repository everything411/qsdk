/*
 * Copyright (c) 2012, 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_sec FAL_SEC
 * @{
 */
#include "sw.h"
#include "fal_sec.h"
#include "hsl_api.h"
#include "adpt.h"

#ifndef IN_SEC_MINI
sw_error_t fal_sec_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_l2_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l2_excep_ctrl_set, dev_id, excep_type, ctrl)

sw_error_t fal_sec_l2_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l2_excep_ctrl_get, dev_id, excep_type, ctrl)
#endif

sw_error_t fal_sec_tunnel_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_excep_ctrl_set, dev_id, excep_type, ctrl)

#ifndef IN_SEC_MINI
sw_error_t fal_sec_tunnel_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_excep_ctrl_get, dev_id, excep_type, ctrl)

sw_error_t fal_sec_tunnel_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l3_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_tunnel_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l3_excep_parser_ctrl_get, dev_id, ctrl)
#endif

sw_error_t fal_sec_tunnel_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l4_excep_parser_ctrl_set, dev_id, ctrl)

#ifndef IN_SEC_MINI
sw_error_t fal_sec_tunnel_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l4_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_tunnel_flags_excep_parser_ctrl_set(a_uint32_t dev_id, a_uint32_t index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_flags_excep_parser_ctrl_set, dev_id, index, ctrl)

sw_error_t fal_sec_tunnel_flags_excep_parser_ctrl_get(a_uint32_t dev_id, a_uint32_t index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_flags_excep_parser_ctrl_get, dev_id, index, ctrl)
#endif

sw_error_t fal_sec_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l4_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l4_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_l3_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_ctrl_set, dev_id, excep_type, ctrl)

sw_error_t fal_sec_l3_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_ctrl_get, dev_id, excep_type, ctrl)

