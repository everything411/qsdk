/*
 * Copyright (c) 2012, 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_acl_legacy FAL_ACL_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_acl.h"
#include "hsl_api.h"
#include "adpt.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include "fal_acl_legacy.h"

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

sw_error_t fal_acl_list_dump(a_uint32_t dev_id)
	DEFINE_FAL_FUNC_HSL_EXPORT(acl_list_dump, dev_id)

/**
 * @}
 */
