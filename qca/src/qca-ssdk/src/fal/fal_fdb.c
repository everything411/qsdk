/*
 * Copyright (c) 2012, 2015-2018, The Linux Foundation. All rights reserved.
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
 * @defgroup fal_fdb FAL_FDB
 * @{
 */
#include "sw.h"
#include "fal_fdb.h"
#include "hsl_api.h"
#include "adpt.h"
#include "ref_fdb.h"

#include <linux/kernel.h>
#include <linux/module.h>

#define FDB_FIRST_ENTRY 0
#define FDB_NEXT_ENTRY  1

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

static sw_error_t
_fal_fdb_sw_entry_get_first_next(a_uint32_t dev_id, fal_fdb_op_t * option,
	a_uint32_t fn_sel, fal_fdb_entry_t * entry)
{
	ref_fdb_info_t *pfdb = NULL, *pfdb_next = NULL;
	struct qca_phy_priv *priv = NULL;
	a_bool_t entry_exist = A_FALSE;
	fal_fdb_entry_t entry_tmp = {0};

	SSDK_DEBUG("software fdb table get %s entry\n",
		fn_sel == FDB_FIRST_ENTRY?"first":"next");
	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	aos_lock_bh(&priv->fdb_sw_sync_lock);
	list_for_each_entry_safe(pfdb, pfdb_next, &priv->sw_fdb_tbl, list)
	{
		if(!entry_exist)
		{
			if(option->fid_en && (entry->fid != pfdb->entry.fid))
				continue;
			if(option->port_en && (entry->port.id != pfdb->entry.port.id))
				continue;
			if(fn_sel == FDB_FIRST_ENTRY) {
				aos_mem_copy(&entry_tmp, &pfdb->entry, sizeof(fal_fdb_entry_t));
				entry_exist = A_TRUE;
			}
			else
			{
				if(&pfdb_next->list != &priv->sw_fdb_tbl)
				{
					aos_mem_copy(&entry_tmp, &pfdb_next->entry,
						sizeof(fal_fdb_entry_t));
					entry_exist = A_TRUE;
				}
				else
				{
					aos_unlock_bh(&priv->fdb_sw_sync_lock);
					return SW_NO_MORE;
				}
			}
		}

		if(aos_mem_cmp(entry->addr.uc, pfdb->entry.addr.uc, ETH_ALEN))
			continue;
		if(fn_sel == FDB_FIRST_ENTRY)
		{
			aos_mem_copy(entry, &pfdb->entry, sizeof(fal_fdb_entry_t));
			aos_unlock_bh(&priv->fdb_sw_sync_lock);
			return SW_OK;
		}
		else
		{
			if(&pfdb_next->list != &priv->sw_fdb_tbl)
			{
				aos_mem_copy(entry, &pfdb_next->entry, sizeof(fal_fdb_entry_t));
				aos_unlock_bh(&priv->fdb_sw_sync_lock);
				return SW_OK;
			}
			else
			{
				aos_unlock_bh(&priv->fdb_sw_sync_lock);
				return SW_NO_MORE;
			}
		}
	}
	aos_unlock_bh(&priv->fdb_sw_sync_lock);

	if(!entry_exist)
		return SW_NO_MORE;

	aos_mem_copy(entry, &entry_tmp, sizeof(fal_fdb_entry_t));

	return SW_OK;
}

static sw_error_t
_fal_fdb_sw_entry_search(a_uint32_t dev_id, fal_fdb_entry_t *entry)
{
	struct qca_phy_priv *priv = NULL;
	ref_fdb_info_t *pfdb = NULL;

	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);
	SSDK_DEBUG("software fdb table is used\n");
	aos_lock_bh(&priv->fdb_sw_sync_lock);
	list_for_each_entry(pfdb, &(priv->sw_fdb_tbl), list)
	{
		if (!aos_mem_cmp(entry->addr.uc, pfdb->entry.addr.uc, ETH_ALEN) &&
			entry->fid == pfdb->entry.fid) {
			aos_mem_copy(entry, &pfdb->entry, sizeof(fal_fdb_entry_t));
			aos_unlock_bh(&priv->fdb_sw_sync_lock);
			return SW_OK;
		}
	}
	aos_unlock_bh(&priv->fdb_sw_sync_lock);

	return SW_NOT_FOUND;
}

sw_error_t fal_fdb_entry_add(a_uint32_t dev_id, const fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_add, fdb_add, _fal_fdb_sw_sync, dev_id, entry)
    EXPORT_SYMBOL(fal_fdb_entry_add);

sw_error_t fal_fdb_entry_flush(a_uint32_t dev_id, a_uint32_t flag)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_del_all, fdb_del_all, _fal_fdb_sw_sync, dev_id, flag)
    EXPORT_SYMBOL(fal_fdb_entry_flush);

sw_error_t fal_fdb_entry_del_byport(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flag)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_del_by_port, fdb_del_by_port, _fal_fdb_sw_sync, dev_id, port_id, flag)
    EXPORT_SYMBOL(fal_fdb_entry_del_byport);

sw_error_t fal_fdb_entry_del_bymac(a_uint32_t dev_id, const fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_del_by_mac, fdb_del_by_mac, _fal_fdb_sw_sync, dev_id, entry)
    EXPORT_SYMBOL(fal_fdb_entry_del_bymac);

sw_error_t fal_fdb_entry_getfirst(a_uint32_t dev_id, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC(fdb_first, dev_id, entry)
    EXPORT_SYMBOL(fal_fdb_entry_getfirst);

sw_error_t fal_fdb_entry_getnext(a_uint32_t dev_id, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC(fdb_next, dev_id, entry)
    EXPORT_SYMBOL(fal_fdb_entry_getnext);

sw_error_t fal_fdb_port_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_HSL_EXPORT(fdb_port_learn_set, port_learn_set, dev_id, port_id, enable)

sw_error_t fal_fdb_port_learning_ctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_ADPT(fdb_port_newaddr_lrn_set, dev_id, port_id, enable, cmd)
    EXPORT_SYMBOL(fal_fdb_port_learning_ctrl_set);

sw_error_t fal_fdb_port_learning_ctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable, fal_fwd_cmd_t *cmd)
    DEFINE_FAL_FUNC_ADPT(fdb_port_newaddr_lrn_get, dev_id, port_id, enable, cmd)
    EXPORT_SYMBOL(fal_fdb_port_learning_ctrl_get);

sw_error_t fal_fdb_port_stamove_ctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_ADPT(fdb_port_stamove_set, dev_id, port_id, enable, cmd)
    EXPORT_SYMBOL(fal_fdb_port_stamove_ctrl_set);

sw_error_t fal_fdb_port_stamove_ctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable, fal_fwd_cmd_t *cmd)
    DEFINE_FAL_FUNC_ADPT(fdb_port_stamove_get, dev_id, port_id, enable, cmd)
    EXPORT_SYMBOL(fal_fdb_port_stamove_ctrl_get);

sw_error_t fal_fdb_aging_ctrl_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_HSL(fdb_age_ctrl_set, age_ctrl_set, dev_id, enable)
    EXPORT_SYMBOL(fal_fdb_aging_ctrl_set);

sw_error_t fal_fdb_vlan_ivl_svl_set(a_uint32_t dev_id, fal_fdb_smode smode)
    DEFINE_FAL_FUNC_HSL(vlan_ivl_svl_set, dev_id, smode)
    EXPORT_SYMBOL(fal_fdb_vlan_ivl_svl_set);

sw_error_t fal_fdb_vlan_ivl_svl_get(a_uint32_t dev_id, fal_fdb_smode* smode)
    DEFINE_FAL_FUNC_HSL(vlan_ivl_svl_get, dev_id, smode)
    EXPORT_SYMBOL(fal_fdb_vlan_ivl_svl_get);

sw_error_t fal_fdb_aging_time_set(a_uint32_t dev_id, a_uint32_t * time)
    DEFINE_FAL_FUNC_ADPT_HSL(fdb_age_time_set, age_time_set, dev_id, time)
    EXPORT_SYMBOL(fal_fdb_aging_time_set);

sw_error_t fal_fdb_aging_time_get(a_uint32_t dev_id, a_uint32_t * time)
    DEFINE_FAL_FUNC_ADPT_HSL(fdb_age_time_get, age_time_get, dev_id, time)
    EXPORT_SYMBOL(fal_fdb_aging_time_get);

sw_error_t fal_port_fdb_learn_limit_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable, a_uint32_t cnt)
    DEFINE_FAL_FUNC_EXPORT(port_fdb_learn_limit_set, dev_id, port_id, enable, cnt)

sw_error_t fal_port_fdb_learn_limit_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable, a_uint32_t * cnt)
    DEFINE_FAL_FUNC_EXPORT(port_fdb_learn_limit_get, dev_id, port_id, enable, cnt)

sw_error_t fal_port_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_EXPORT(port_fdb_learn_exceed_cmd_set, dev_id, port_id, cmd)

sw_error_t fal_port_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_EXPORT(port_fdb_learn_exceed_cmd_get, dev_id, port_id, cmd)

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

sw_error_t fal_fdb_port_learn_static_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_ENDFUNC(fdb_port_learn_static_set, _fal_fdb_sw_sync, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_fdb_port_learn_static_set);

sw_error_t fal_fdb_port_learn_static_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_port_learn_static_get, dev_id, port_id, enable)

sw_error_t fal_fdb_rfs_set(a_uint32_t dev_id, const fal_fdb_rfs_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_rfs_set, dev_id, entry)

sw_error_t fal_fdb_rfs_del(a_uint32_t dev_id, const fal_fdb_rfs_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(fdb_rfs_del, dev_id, entry)

sw_error_t fal_fdb_learning_ctrl_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT(fdb_learn_ctrl_set, dev_id, enable)
    EXPORT_SYMBOL(fal_fdb_learning_ctrl_set);

sw_error_t fal_fdb_port_maclimit_ctrl_set(a_uint32_t dev_id, fal_port_t port_id, fal_maclimit_ctrl_t * maclimit_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(fdb_port_maclimit_ctrl_set, dev_id, port_id, maclimit_ctrl)

sw_error_t fal_fdb_port_maclimit_ctrl_get(a_uint32_t dev_id, fal_port_t port_id, fal_maclimit_ctrl_t * maclimit_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(fdb_port_maclimit_ctrl_get, dev_id, port_id, maclimit_ctrl)

sw_error_t fal_fdb_entry_del_byfid(a_uint32_t dev_id, a_uint16_t fid, a_uint32_t flag)
    DEFINE_FAL_FUNC_ADPT(fdb_del_by_fid, dev_id, fid, flag)
    EXPORT_SYMBOL(fal_fdb_entry_del_byfid);

sw_error_t fal_fdb_entry_getnext_byindex(a_uint32_t dev_id, a_uint32_t * iterator, fal_fdb_entry_t * entry)
    DEFINE_FAL_FUNC(fdb_iterate, dev_id, iterator, entry)
    EXPORT_SYMBOL(fal_fdb_entry_getnext_byindex);

#ifndef IN_FDB_MINI
sw_error_t fal_fdb_port_learn_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_ADPT_HSL_EXPORT(fdb_port_learn_get, port_learn_get, dev_id, port_id, enable)

sw_error_t fal_fdb_aging_ctrl_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_ADPT_HSL(fdb_age_ctrl_get, age_ctrl_get, dev_id, enable)
    EXPORT_SYMBOL(fal_fdb_aging_ctrl_get);

sw_error_t fal_fdb_entry_update_byport(a_uint32_t dev_id, fal_port_t old_port, fal_port_t new_port, a_uint32_t fid, fal_fdb_op_t * option)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_transfer, fdb_transfer, _fal_fdb_sw_sync, dev_id, old_port, new_port, fid, option)
    EXPORT_SYMBOL(fal_fdb_entry_update_byport);

sw_error_t fal_fdb_port_add(a_uint32_t dev_id, a_uint32_t fid, fal_mac_addr_t * addr, fal_port_t port_id)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_port_add, fdb_port_add, _fal_fdb_sw_sync, dev_id, fid, addr, port_id)
    EXPORT_SYMBOL(fal_fdb_port_add);

sw_error_t fal_fdb_port_del(a_uint32_t dev_id, a_uint32_t fid, fal_mac_addr_t * addr, fal_port_t port_id)
    DEFINE_FAL_FUNC_ADPT_HSL_ENDFUNC(fdb_port_del, fdb_port_del, _fal_fdb_sw_sync, dev_id, fid, addr, port_id)
    EXPORT_SYMBOL(fal_fdb_port_del);

sw_error_t fal_fdb_learning_ctrl_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_ADPT(fdb_learn_ctrl_get, dev_id, enable)
    EXPORT_SYMBOL(fal_fdb_learning_ctrl_get);

sw_error_t fal_fdb_port_learned_mac_counter_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * cnt)
    DEFINE_FAL_FUNC_ADPT(port_fdb_learn_counter_get, dev_id, port_id, cnt)
    EXPORT_SYMBOL(fal_fdb_port_learned_mac_counter_get);
#endif

static sw_error_t
_fal_fdb_entry_search(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;
    adpt_api_t *p_adpt_api;

    if((p_adpt_api = adpt_api_ptr_get(dev_id)) != NULL) {
        if (NULL == p_adpt_api->adpt_fdb_find)
            return SW_NOT_SUPPORTED;

        rv = p_adpt_api->adpt_fdb_find(dev_id, entry);
        return rv;
    }

    if(entry->type == SW_ENTRY)
        return _fal_fdb_sw_entry_search(dev_id, entry);

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->fdb_find)
        return SW_NOT_SUPPORTED;

    rv = p_api->fdb_find(dev_id, entry);
    return rv;
}

static sw_error_t
_fal_fdb_entry_extend_getnext(a_uint32_t dev_id, fal_fdb_op_t * option,
                     fal_fdb_entry_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;
    adpt_api_t *p_adpt_api;

    if((p_adpt_api = adpt_api_ptr_get(dev_id)) != NULL) {
        if (NULL == p_adpt_api->adpt_fdb_extend_next)
            return SW_NOT_SUPPORTED;

        rv = p_adpt_api->adpt_fdb_extend_next(dev_id, option, entry);
        return rv;
    }

    if(entry->type == SW_ENTRY)
        return _fal_fdb_sw_entry_get_first_next(dev_id, option, FDB_NEXT_ENTRY,
            entry);

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->fdb_extend_next)
        return SW_NOT_SUPPORTED;

    rv = p_api->fdb_extend_next(dev_id, option, entry);
    return rv;
}

static sw_error_t
_fal_fdb_entry_extend_getfirst(a_uint32_t dev_id, fal_fdb_op_t * option,
                      fal_fdb_entry_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;
    adpt_api_t *p_adpt_api;

    if((p_adpt_api = adpt_api_ptr_get(dev_id)) != NULL) {
        if (NULL == p_adpt_api->adpt_fdb_extend_first)
            return SW_NOT_SUPPORTED;

        rv = p_adpt_api->adpt_fdb_extend_first(dev_id, option, entry);
        return rv;
    }

    if(entry->type == SW_ENTRY)
        return _fal_fdb_sw_entry_get_first_next(dev_id, option, FDB_FIRST_ENTRY,
            entry);

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->fdb_extend_first)
        return SW_NOT_SUPPORTED;

    rv = p_api->fdb_extend_first(dev_id, option, entry);
    return rv;
}

sw_error_t
fal_fdb_entry_search(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_fdb_entry_search(dev_id, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_fdb_entry_extend_getnext(a_uint32_t dev_id, fal_fdb_op_t * option,
                    fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_fdb_entry_extend_getnext(dev_id, option, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_fdb_entry_extend_getfirst(a_uint32_t dev_id, fal_fdb_op_t * option,
                     fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_fdb_entry_extend_getfirst(dev_id, option, entry);
    FAL_API_UNLOCK;
    return rv;
}

EXPORT_SYMBOL(fal_fdb_entry_search);
EXPORT_SYMBOL(fal_fdb_entry_extend_getnext);
EXPORT_SYMBOL(fal_fdb_entry_extend_getfirst);

