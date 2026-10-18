/*
 * Copyright (c) 2014, 2016-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_acl_legacy FAL_ACL_LEGACY
 * @{
 */
#ifndef _FAL_ACL_LEGACY_H_
#define _FAL_ACL_LEGACY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

sw_error_t
fal_acl_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_acl_status_get(a_uint32_t dev_id, a_bool_t * enable);

sw_error_t
fal_acl_list_dump(a_uint32_t dev_id);

sw_error_t
fal_acl_port_udf_profile_set(a_uint32_t dev_id, fal_port_t port_id, fal_acl_udf_type_t udf_type, a_uint32_t offset, a_uint32_t length);
sw_error_t
fal_acl_port_udf_profile_get(a_uint32_t dev_id, fal_port_t port_id, fal_acl_udf_type_t udf_type, a_uint32_t * offset, a_uint32_t * length);

sw_error_t
fal_acl_rule_active(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, a_uint32_t rule_nr);
sw_error_t
fal_acl_rule_deactive(a_uint32_t dev_id, a_uint32_t list_id, a_uint32_t rule_id, a_uint32_t rule_nr);
sw_error_t
fal_acl_rule_src_filter_sts_set(a_uint32_t dev_id, a_uint32_t rule_id, a_bool_t enable);
sw_error_t
fal_acl_rule_src_filter_sts_get(a_uint32_t dev_id, a_uint32_t rule_id, a_bool_t* enable);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_ACL_LEGACY_H_ */
/**
 * @}
 */
