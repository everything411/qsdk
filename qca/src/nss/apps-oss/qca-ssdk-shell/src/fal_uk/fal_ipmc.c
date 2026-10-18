/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw_ioctl.h"
#include "fal_ipmc.h"
#include "fal_uk_if.h"

sw_error_t fal_ipmc_status_set(a_uint32_t dev_id, a_bool_t enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_STATUS_SET, dev_id, enable);
	return rv;
}

sw_error_t fal_ipmc_status_get(a_uint32_t dev_id, a_bool_t *enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_STATUS_GET, dev_id, enable);
	return rv;
}

sw_error_t fal_ipmc_global_cfg_set(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_GLOBAL_CFG_SET, dev_id, cfg);
	return rv;
}

sw_error_t fal_ipmc_global_cfg_get(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_GLOBAL_CFG_GET, dev_id, cfg);
	return rv;
}

sw_error_t fal_ipmc_entry_add(a_uint32_t dev_id, fal_ipmc_op_mode_t add_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_ENTRY_ADD, dev_id, add_mode, ipmc_entry);
	return rv;
}

sw_error_t fal_ipmc_entry_del(a_uint32_t dev_id, fal_ipmc_op_mode_t del_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_ENTRY_DEL, dev_id, del_mode, ipmc_entry);
	return rv;
}

sw_error_t fal_ipmc_entry_get(a_uint32_t dev_id, fal_ipmc_op_mode_t get_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_ENTRY_GET, dev_id, get_mode, ipmc_entry);
	return rv;
}

sw_error_t fal_ipmc_entry_getnext(a_uint32_t dev_id, fal_ipmc_next_mode_t next_mode,
		fal_ipmc_entry_t *ipmc_entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_ENTRY_GETNEXT, dev_id, next_mode, ipmc_entry);
	return rv;
}

sw_error_t fal_ipmc_entry_flush(a_uint32_t dev_id)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_ENTRY_FLUSH, dev_id);
	return rv;
}

sw_error_t fal_ipmc_ucast_fwd_set(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_UCAST_FWD_SET, dev_id, ucast_fwd);
	return rv;
}

sw_error_t fal_ipmc_ucast_fwd_get(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_IPMC_UCAST_FWD_GET, dev_id, ucast_fwd);
	return rv;
}
