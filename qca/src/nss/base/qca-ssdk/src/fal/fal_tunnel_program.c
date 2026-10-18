/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_tunnel_program FAL_TUNNEL_PROGRAM
 * @{
 */
#include "sw.h"
#include "fal_tunnel_program.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_tunnel_program_entry_add(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_entry_add, dev_id, type, entry)

sw_error_t fal_tunnel_program_entry_del(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_entry_del, dev_id, type, entry)

#ifndef IN_TUNNEL_PROGRAM_MINI
sw_error_t fal_tunnel_program_entry_getfirst(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_entry_getfirst, dev_id, type, entry)

sw_error_t fal_tunnel_program_entry_getnext(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_entry_getnext, dev_id, type, entry)
#endif

sw_error_t fal_tunnel_program_cfg_set(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_cfg_t * cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_cfg_set, dev_id, type, cfg)

#ifndef IN_TUNNEL_PROGRAM_MINI
sw_error_t fal_tunnel_program_cfg_get(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_cfg_t * cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_cfg_get, dev_id, type, cfg)
#endif

sw_error_t fal_tunnel_program_udf_add(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_add, dev_id, type, udf)

sw_error_t fal_tunnel_program_udf_del(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_del, dev_id, type, udf)

#ifndef IN_TUNNEL_PROGRAM_MINI
sw_error_t fal_tunnel_program_udf_getfirst(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_getfirst, dev_id, type, udf)

sw_error_t fal_tunnel_program_udf_getnext(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_getnext, dev_id, type, udf)
#endif
