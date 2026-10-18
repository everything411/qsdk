/*
 * Copyright (c) 2012, 2015-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_fdb_legacy FAL_FDB_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_fdb.h"
#include "hsl_api.h"
#include "adpt.h"
#include "ref_fdb.h"
#include "fal_fdb_legacy.h"

#include <linux/kernel.h>
#include <linux/module.h>

sw_error_t fal_fdb_vlan_ivl_svl_set(a_uint32_t dev_id, fal_fdb_smode smode)
    DEFINE_FAL_FUNC_HSL(vlan_ivl_svl_set, dev_id, smode)
    EXPORT_SYMBOL(fal_fdb_vlan_ivl_svl_set);

sw_error_t fal_fdb_vlan_ivl_svl_get(a_uint32_t dev_id, fal_fdb_smode* smode)
    DEFINE_FAL_FUNC_HSL(vlan_ivl_svl_get, dev_id, smode)
    EXPORT_SYMBOL(fal_fdb_vlan_ivl_svl_get);

sw_error_t fal_fdb_learn_limit_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t cnt)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_learn_limit_set, dev_id, enable, cnt)

sw_error_t fal_fdb_learn_limit_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * cnt)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_learn_limit_get, dev_id, enable, cnt)

sw_error_t fal_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_learn_exceed_cmd_set, dev_id, cmd)

sw_error_t fal_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_learn_exceed_cmd_get, dev_id, cmd)

sw_error_t fal_fdb_resv_add(a_uint32_t dev_id, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_resv_add, dev_id, entry)

sw_error_t fal_fdb_resv_del(a_uint32_t dev_id, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_resv_del, dev_id, entry)

sw_error_t fal_fdb_resv_find(a_uint32_t dev_id, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_resv_find, dev_id, entry)

sw_error_t fal_fdb_resv_iterate(a_uint32_t dev_id, a_uint32_t * iterator, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_resv_iterate, dev_id, iterator, entry)

static sw_error_t
_fal_fdb_sw_sync(a_uint32_t dev_id)
{
	struct qca_phy_priv *priv = NULL;

	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	if(!list_empty(&priv->sw_fdb_tbl))
		return ref_fdb_sw_sync_task(priv);

	return SW_OK;
}

sw_error_t fal_fdb_port_learn_static_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_ENDFUNC(fdb_port_learn_static_set, _fal_fdb_sw_sync, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_fdb_port_learn_static_set);

sw_error_t fal_fdb_port_learn_static_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_port_learn_static_get, dev_id, port_id, enable)

sw_error_t fal_fdb_rfs_set(a_uint32_t dev_id, const fal_fdb_rfs_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_rfs_set, dev_id, entry)

sw_error_t fal_fdb_rfs_del(a_uint32_t dev_id, const fal_fdb_rfs_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_rfs_del, dev_id, entry)

/**
 * @}
 */
