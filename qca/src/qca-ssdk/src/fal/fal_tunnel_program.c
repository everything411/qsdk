/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

sw_error_t fal_tunnel_program_entry_getfirst(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_entry_getfirst, dev_id, type, entry)

sw_error_t fal_tunnel_program_entry_getnext(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_entry_getnext, dev_id, type, entry)

sw_error_t fal_tunnel_program_cfg_set(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_cfg_t * cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_cfg_set, dev_id, type, cfg)

sw_error_t fal_tunnel_program_cfg_get(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_cfg_t * cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_cfg_get, dev_id, type, cfg)

sw_error_t fal_tunnel_program_udf_add(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_add, dev_id, type, udf)

sw_error_t fal_tunnel_program_udf_del(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_del, dev_id, type, udf)

sw_error_t fal_tunnel_program_udf_getfirst(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_getfirst, dev_id, type, udf)

sw_error_t fal_tunnel_program_udf_getnext(a_uint32_t dev_id, fal_tunnel_program_type_t type, fal_tunnel_program_udf_t * udf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_program_udf_getnext, dev_id, type, udf)

