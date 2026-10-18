/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


/**
 * @defgroup fal_rss_hash FAL_RSS_HASH
 * @{
 */
#include "sw.h"
#include "fal_rss_hash.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_rss_hash_config_set(a_uint32_t dev_id, fal_rss_hash_mode_t mode, fal_rss_hash_config_t * config)
    DEFINE_FAL_FUNC_ADPT_EXPORT(rss_hash_config_set, dev_id, mode, config)

sw_error_t fal_rss_hash_config_get(a_uint32_t dev_id, fal_rss_hash_mode_t mode, fal_rss_hash_config_t * config)
    DEFINE_FAL_FUNC_ADPT_EXPORT(rss_hash_config_get, dev_id, mode, config)

sw_error_t fal_toeplitz_hash_secret_key_set(a_uint32_t dev_id, fal_toeplitz_secret_key_t *secret_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(toeplitz_hash_secret_key_set, dev_id, secret_key)

sw_error_t fal_toeplitz_hash_secret_key_get(a_uint32_t dev_id, fal_toeplitz_secret_key_t *secret_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(toeplitz_hash_secret_key_get, dev_id, secret_key)

sw_error_t fal_rsshash_algm_set(a_uint32_t dev_id, fal_rss_hash_algm_t *rsshash_algm)
    DEFINE_FAL_FUNC_ADPT_EXPORT(rsshash_algm_set, dev_id, rsshash_algm)

sw_error_t fal_rsshash_algm_get(a_uint32_t dev_id, fal_rss_hash_algm_t *rsshash_algm)
    DEFINE_FAL_FUNC_ADPT_EXPORT(rsshash_algm_get, dev_id, rsshash_algm)

sw_error_t fal_toeplitz_hash_config_add(a_uint32_t dev_id, fal_toeplitz_hash_config_t *toeplitz_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(toeplitz_hash_config_add, dev_id, toeplitz_cfg)

sw_error_t fal_toeplitz_hash_config_del(a_uint32_t dev_id, fal_toeplitz_hash_config_t *toeplitz_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(toeplitz_hash_config_del, dev_id, toeplitz_cfg)

sw_error_t fal_toeplitz_hash_config_getfirst(a_uint32_t dev_id, fal_toeplitz_hash_config_t *toeplitz_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(toeplitz_hash_config_getfirst, dev_id, toeplitz_cfg)

sw_error_t fal_toeplitz_hash_config_getnext(a_uint32_t dev_id, fal_toeplitz_hash_config_t *toeplitz_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(toeplitz_hash_config_getnext, dev_id, toeplitz_cfg)

