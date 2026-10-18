/*
 * Copyright (c) 2012, 2016-2017, The Linux Foundation. All rights reserved.
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
 * @defgroup fal_acl FAL_ACL
 * @{
 */
#include "sw.h"
#include "fal_acl.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_acl_list_creat(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t prio)
    DEFINE_FAL_FUNC_EXPORT(acl_list_creat, dev_id, list_id, prio)

sw_error_t fal_acl_list_destroy(a_uint32_t dev_id, a_uint32_t list_id)
    DEFINE_FAL_FUNC_EXPORT(acl_list_destroy, dev_id, list_id)

sw_error_t fal_acl_rule_add(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, a_uint32_t rule_nr, fal_acl_rule_t * rule)
    DEFINE_FAL_FUNC_EXPORT(acl_rule_add, dev_id, list_id, rule_id, rule_nr, rule)

sw_error_t fal_acl_rule_delete(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, a_uint32_t rule_nr)
    DEFINE_FAL_FUNC_EXPORT(acl_rule_delete, dev_id, list_id, rule_id, rule_nr)

sw_error_t fal_acl_rule_query(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, fal_acl_rule_t * rule)
    DEFINE_FAL_FUNC_EXPORT(acl_rule_query, dev_id, list_id, rule_id, rule)

sw_error_t fal_acl_list_bind(a_uint32_t dev_id, a_uint32_t list_id, fal_acl_direc_t direc, fal_acl_bind_obj_t obj_t, a_uint32_t obj_idx)
    DEFINE_FAL_FUNC_EXPORT(acl_list_bind, dev_id, list_id, direc, obj_t, obj_idx)

sw_error_t fal_acl_list_unbind(a_uint32_t dev_id, a_uint32_t list_id, fal_acl_direc_t direc, fal_acl_bind_obj_t obj_t, a_uint32_t obj_idx)
    DEFINE_FAL_FUNC_EXPORT(acl_list_unbind, dev_id, list_id, direc, obj_t, obj_idx)

sw_error_t fal_acl_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_status_set, dev_id, enable)

sw_error_t fal_acl_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_status_get, dev_id, enable)

sw_error_t fal_acl_port_udf_profile_set(a_uint32_t dev_id, fal_port_t port_id, fal_acl_udf_type_t udf_type, a_uint32_t offset, a_uint32_t length)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_port_udf_profile_set, dev_id, port_id, udf_type, offset, length)

sw_error_t fal_acl_port_udf_profile_get(a_uint32_t dev_id, fal_port_t port_id, fal_acl_udf_type_t udf_type, a_uint32_t * offset, a_uint32_t * length)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_port_udf_profile_get, dev_id, port_id, udf_type, offset, length)

sw_error_t fal_acl_rule_active(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, a_uint32_t rule_nr)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_rule_active, dev_id, list_id, rule_id, rule_nr)

sw_error_t fal_acl_rule_deactive(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, a_uint32_t rule_nr)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_rule_deactive, dev_id, list_id, rule_id, rule_nr)

sw_error_t fal_acl_rule_src_filter_sts_set(a_uint32_t dev_id, a_uint32_t rule_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_rule_src_filter_sts_set, dev_id, rule_id, enable)

sw_error_t fal_acl_rule_src_filter_sts_get(a_uint32_t dev_id, a_uint32_t rule_id, a_bool_t* enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(acl_rule_src_filter_sts_get, dev_id, rule_id, enable)

sw_error_t fal_acl_udf_profile_set(a_uint32_t dev_id, fal_acl_udf_pkt_type_t pkt_type,a_uint32_t udf_idx, fal_acl_udf_type_t udf_type, a_uint32_t offset)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_set, dev_id, pkt_type, udf_idx, udf_type, offset)

sw_error_t fal_acl_udf_profile_get(a_uint32_t dev_id, fal_acl_udf_pkt_type_t pkt_type,a_uint32_t udf_idx, fal_acl_udf_type_t *udf_type, a_uint32_t *offset)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_get, dev_id, pkt_type, udf_idx, udf_type, offset)

sw_error_t fal_acl_udf_profile_entry_add(a_uint32_t dev_id, a_uint32_t profile_id, fal_acl_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_entry_add, dev_id, profile_id, entry)

sw_error_t fal_acl_udf_profile_entry_del(a_uint32_t dev_id, a_uint32_t profile_id, fal_acl_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_entry_del, dev_id, profile_id, entry)

sw_error_t fal_acl_udf_profile_entry_getfirst(a_uint32_t dev_id, a_uint32_t profile_id, fal_acl_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_entry_getfirst, dev_id, profile_id, entry)

sw_error_t fal_acl_udf_profile_entry_getnext(a_uint32_t dev_id, a_uint32_t profile_id, fal_acl_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_entry_getnext, dev_id, profile_id, entry)

sw_error_t fal_acl_udf_profile_cfg_set(a_uint32_t dev_id, a_uint32_t profile_id, a_uint32_t udf_idx, fal_acl_udf_type_t udf_type, a_uint32_t offset)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_cfg_set, dev_id, profile_id, udf_idx, udf_type, offset)

sw_error_t fal_acl_udf_profile_cfg_get(a_uint32_t dev_id, a_uint32_t profile_id, a_uint32_t udf_idx, fal_acl_udf_type_t * udf_type, a_uint32_t * offset)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_udf_profile_cfg_get, dev_id, profile_id, udf_idx, udf_type, offset)

sw_error_t fal_acl_vpgroup_set(a_uint32_t dev_id, a_uint32_t vport_id, fal_vport_type_t vport_type, a_uint32_t vpgroup_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_vpgroup_set, dev_id, vport_id, vport_type, vpgroup_id)

sw_error_t fal_acl_vpgroup_get(a_uint32_t dev_id, a_uint32_t vport_id, fal_vport_type_t vport_type, a_uint32_t * vpgroup_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_vpgroup_get, dev_id, vport_id, vport_type, vpgroup_id)

sw_error_t fal_acl_counter_get(a_uint32_t dev_id, a_uint32_t entry_index, fal_entry_counter_t *acl_counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_counter_get, dev_id, entry_index, acl_counter)

sw_error_t fal_acl_list_dump(a_uint32_t dev_id)
	DEFINE_FAL_FUNC_HSL_EXPORT(acl_list_dump, dev_id)

sw_error_t fal_acl_rule_dump(a_uint32_t dev_id)
	DEFINE_FAL_FUNC_EXPORT(acl_rule_dump, dev_id)

