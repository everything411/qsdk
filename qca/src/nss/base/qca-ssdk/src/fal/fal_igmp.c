/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
* @defgroup fal_igmp FAL_IGMP
* @{
*/
#include "sw.h"
#include "fal_igmp.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_port_igmps_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_igmps_status_set, dev_id, port_id, enable)

sw_error_t fal_port_igmps_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_igmps_status_get, dev_id, port_id, enable)

sw_error_t fal_igmp_mld_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(igmp_mld_cmd_set, dev_id, cmd)

sw_error_t fal_igmp_mld_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(igmp_mld_cmd_get, dev_id, cmd)

sw_error_t fal_port_igmp_mld_join_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL(port_igmp_join_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_igmp_mld_join_set);

sw_error_t fal_port_igmp_mld_join_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL(port_igmp_join_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_igmp_mld_join_get);

sw_error_t fal_port_igmp_mld_leave_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL(port_igmp_leave_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_igmp_mld_leave_set);

sw_error_t fal_port_igmp_mld_leave_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL(port_igmp_leave_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_igmp_mld_leave_get);

sw_error_t fal_igmp_mld_rp_set(a_uint32_t dev_id, fal_pbmp_t pts)
    DEFINE_FAL_FUNC_HSL(igmp_rp_set, dev_id, pts)
    EXPORT_SYMBOL(fal_igmp_mld_rp_set);

sw_error_t fal_igmp_mld_rp_get(a_uint32_t dev_id, fal_pbmp_t * pts)
    DEFINE_FAL_FUNC_HSL(igmp_rp_get, dev_id, pts)
    EXPORT_SYMBOL(fal_igmp_mld_rp_get);

sw_error_t fal_igmp_mld_entry_creat_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_creat_set, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_creat_set);

sw_error_t fal_igmp_mld_entry_creat_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_creat_get, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_creat_get);

sw_error_t fal_igmp_mld_entry_static_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_static_set, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_static_set);

sw_error_t fal_igmp_mld_entry_static_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_static_get, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_static_get);

sw_error_t fal_igmp_mld_entry_leaky_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_leaky_set, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_leaky_set);

sw_error_t fal_igmp_mld_entry_leaky_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_leaky_get, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_leaky_get);

sw_error_t fal_igmp_mld_entry_v3_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_v3_set, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_v3_set);

sw_error_t fal_igmp_mld_entry_v3_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL(igmp_entry_v3_get, dev_id, enable)
    EXPORT_SYMBOL(fal_igmp_mld_entry_v3_get);

sw_error_t fal_igmp_mld_entry_queue_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t queue)
    DEFINE_FAL_FUNC_HSL(igmp_entry_queue_set, dev_id, enable, queue)
    EXPORT_SYMBOL(fal_igmp_mld_entry_queue_set);

sw_error_t fal_igmp_mld_entry_queue_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * queue)
    DEFINE_FAL_FUNC_HSL(igmp_entry_queue_get, dev_id, enable, queue)
    EXPORT_SYMBOL(fal_igmp_mld_entry_queue_get);

sw_error_t fal_port_igmp_mld_learn_limit_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable, a_uint32_t cnt)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_igmp_mld_learn_limit_set, dev_id, port_id, enable, cnt)

sw_error_t fal_port_igmp_mld_learn_limit_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable, a_uint32_t * cnt)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_igmp_mld_learn_limit_get, dev_id, port_id, enable, cnt)

sw_error_t fal_port_igmp_mld_learn_exceed_cmd_set(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_igmp_mld_learn_exceed_cmd_set, dev_id, port_id, cmd)

sw_error_t fal_port_igmp_mld_learn_exceed_cmd_get(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_igmp_mld_learn_exceed_cmd_get, dev_id, port_id, cmd)

sw_error_t fal_igmp_sg_entry_set(a_uint32_t dev_id, fal_igmp_sg_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(igmp_sg_entry_set, dev_id, entry)

sw_error_t fal_igmp_sg_entry_clear(a_uint32_t dev_id, fal_igmp_sg_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(igmp_sg_entry_clear, dev_id, entry)

sw_error_t fal_igmp_sg_entry_show(a_uint32_t dev_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(igmp_sg_entry_show, dev_id)

sw_error_t fal_igmp_sg_entry_query(a_uint32_t dev_id, fal_igmp_sg_info_t * info)
    DEFINE_FAL_FUNC_HSL_EXPORT(igmp_sg_entry_query, dev_id, info)

