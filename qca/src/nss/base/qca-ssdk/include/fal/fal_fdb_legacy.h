/*
 * Copyright (c) 2012, 2015-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_fdb_legacy FAL_FDB_LEGACY
 * @{
 */
#ifndef _FAL_FDB_LEGACY_H_
#define _FAL_FDB_LEGACY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

    typedef enum
    {
        INVALID_VLAN_SVL=0,
        INVALID_VLAN_IVL
    } fal_fdb_smode;

#if defined(IN_RFS)
    sw_error_t
    fal_fdb_rfs_set(a_uint32_t dev_id, const fal_fdb_rfs_t * entry);

    sw_error_t
    fal_fdb_rfs_del(a_uint32_t dev_id, const fal_fdb_rfs_t * entry);
#endif

    sw_error_t
    fal_fdb_port_learn_static_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    sw_error_t
    fal_fdb_port_learn_static_get(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t * enable);

#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_vlan_ivl_svl_set(a_uint32_t dev_id, fal_fdb_smode smode);

    sw_error_t
    fal_fdb_vlan_ivl_svl_get(a_uint32_t dev_id, fal_fdb_smode * smode);

    sw_error_t
    fal_fdb_learn_limit_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t cnt);

    sw_error_t
    fal_fdb_learn_limit_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * cnt);

    sw_error_t
    fal_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);

    sw_error_t
    fal_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);

    sw_error_t
    fal_fdb_resv_add(a_uint32_t dev_id, fal_fdb_entry_t * entry);

    sw_error_t
    fal_fdb_resv_del(a_uint32_t dev_id, fal_fdb_entry_t * entry);

    sw_error_t
    fal_fdb_resv_find(a_uint32_t dev_id, fal_fdb_entry_t * entry);

    sw_error_t
    fal_fdb_resv_iterate(a_uint32_t dev_id, a_uint32_t * iterator, fal_fdb_entry_t * entry);
#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_FDB_LEGACY_H_ */
/**
 * @}
 */
