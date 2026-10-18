/*
 * Copyright (c) 2012, 2015, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @defgroup fal_nat FAL_NAT
 * @{
 */

#include "sw.h"
#include "fal_nat.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_nat_add(a_uint32_t dev_id, fal_nat_entry_t * nat_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_add, dev_id, nat_entry)

sw_error_t fal_nat_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_nat_entry_t * nat_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_del, dev_id, del_mode, nat_entry)

sw_error_t fal_nat_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_nat_entry_t * nat_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_get, dev_id, get_mode, nat_entry)

sw_error_t fal_nat_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_nat_entry_t * nat_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_next, dev_id, next_mode, nat_entry)

sw_error_t fal_nat_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_counter_bind, dev_id, entry_id, cnt_id, enable)

sw_error_t fal_napt_add(a_uint32_t dev_id, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_add, dev_id, napt_entry)

sw_error_t fal_napt_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_del, dev_id, del_mode, napt_entry)

sw_error_t fal_napt_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_get, dev_id, get_mode, napt_entry)

sw_error_t fal_napt_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_next, dev_id, next_mode, napt_entry)

sw_error_t fal_napt_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_counter_bind, dev_id, entry_id, cnt_id, enable)

sw_error_t fal_flow_add(a_uint32_t dev_id, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_add, dev_id, napt_entry)

sw_error_t fal_flow_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_del, dev_id, del_mode, napt_entry)

sw_error_t fal_flow_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_get, dev_id, get_mode, napt_entry)

sw_error_t fal_flow_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_napt_entry_t * napt_entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_next, dev_id, next_mode, napt_entry)

sw_error_t fal_flow_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_counter_bind, dev_id, entry_id, cnt_id, enable)

sw_error_t fal_nat_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_status_set, dev_id, enable)

sw_error_t fal_nat_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_status_get, dev_id, enable)

sw_error_t fal_nat_hash_mode_set(a_uint32_t dev_id, a_uint32_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_hash_mode_set, dev_id, mode)

sw_error_t fal_nat_hash_mode_get(a_uint32_t dev_id, a_uint32_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_hash_mode_get, dev_id, mode)

sw_error_t fal_napt_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_status_set, dev_id, enable)

sw_error_t fal_napt_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_status_get, dev_id, enable)

sw_error_t fal_napt_mode_set(a_uint32_t dev_id, fal_napt_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_mode_set, dev_id, mode)

sw_error_t fal_napt_mode_get(a_uint32_t dev_id, fal_napt_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(napt_mode_get, dev_id, mode)

sw_error_t fal_nat_prv_base_addr_set(a_uint32_t dev_id, fal_ip4_addr_t addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_prv_base_addr_set, dev_id, addr)

sw_error_t fal_nat_prv_base_addr_get(a_uint32_t dev_id, fal_ip4_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_prv_base_addr_get, dev_id, addr)

sw_error_t fal_nat_prv_base_mask_set(a_uint32_t dev_id, fal_ip4_addr_t addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_prv_base_mask_set, dev_id, addr)

sw_error_t fal_nat_prv_base_mask_get(a_uint32_t dev_id, fal_ip4_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_prv_base_mask_get, dev_id, addr)

sw_error_t fal_nat_prv_addr_mode_set(a_uint32_t dev_id, a_bool_t map_en)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_prv_addr_mode_set, dev_id, map_en)

sw_error_t fal_nat_prv_addr_mode_get(a_uint32_t dev_id, a_bool_t * map_en)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_prv_addr_mode_get, dev_id, map_en)

sw_error_t fal_nat_pub_addr_add(a_uint32_t dev_id, fal_nat_pub_addr_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_pub_addr_add, dev_id, entry)

sw_error_t fal_nat_pub_addr_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_nat_pub_addr_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_pub_addr_del, dev_id, del_mode, entry)

sw_error_t fal_nat_pub_addr_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_nat_pub_addr_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_pub_addr_next, dev_id, next_mode, entry)

sw_error_t fal_nat_unk_session_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_unk_session_cmd_set, dev_id, cmd)

sw_error_t fal_nat_unk_session_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(nat_unk_session_cmd_get, dev_id, cmd)


sw_error_t fal_flow_cookie_set(a_uint32_t dev_id, fal_flow_cookie_t * flow_cookie)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_cookie_set, dev_id, flow_cookie)

sw_error_t fal_flow_rfs_set(a_uint32_t dev_id, a_uint8_t action, fal_flow_rfs_t * rfs)
    DEFINE_FAL_FUNC_HSL_EXPORT(flow_rfs_set, dev_id, action, rfs)


static sw_error_t
_fal_nat_global_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t portbmp)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_global_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_global_set(dev_id, enable, portbmp);
    return rv;
}

int nf_athrs17_hnat_sync_counter_en = 0;
sw_error_t
fal_nat_global_set(a_uint32_t dev_id, a_bool_t enable,
		a_bool_t sync_cnt_enable, a_uint32_t portbmp)
{
    sw_error_t rv;

    FAL_API_LOCK;
	nf_athrs17_hnat_sync_counter_en = (int)sync_cnt_enable;
    rv = _fal_nat_global_set(dev_id, enable, portbmp);
    FAL_API_UNLOCK;
    return rv;
}

