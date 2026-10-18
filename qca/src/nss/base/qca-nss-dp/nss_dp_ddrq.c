/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include "nss_dp_hal.h"

/*
 * nss_dp_ddrq_cfg_get()
 *	API to get DDRQ AC queue configurations
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_cfg_get(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg, uint32_t count)
{
	return nss_dp_hal_ddrq_cfg_get(obj, ddrq_cfg, count);
}
EXPORT_SYMBOL(nss_dp_ddrq_cfg_get);

/*
 * nss_dp_ddrq_cfg_set()
 *	API to set DDRQ AC queue configurations
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_cfg_set(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg)
{
	return nss_dp_hal_ddrq_cfg_set(obj, ddrq_cfg);
}
EXPORT_SYMBOL(nss_dp_ddrq_cfg_set);

/*
 * nss_dp_ddrq_grp_cfg_get()
 *	API to get DDRQ group configurations
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_grp_cfg_get(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg)
{
	return nss_dp_hal_ddrq_grp_cfg_get(ddrq_grp_id, ddrq_grp_cfg);
}
EXPORT_SYMBOL(nss_dp_ddrq_grp_cfg_get);

/*
 * nss_dp_ddrq_grp_cfg_set()
 *	API to set DDRQ group configurations
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_grp_cfg_set(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg)
{
	return nss_dp_hal_ddrq_grp_cfg_set(ddrq_grp_id, ddrq_grp_cfg);
}
EXPORT_SYMBOL(nss_dp_ddrq_grp_cfg_set);

nss_dp_ddrq_ret_t nss_dp_ddrq_enqueue_disable(nss_dp_ddrq_obj_id_t *obj, bool disable)
{
	return nss_dp_hal_ddrq_enqueue_disable(obj, disable);
}
EXPORT_SYMBOL(nss_dp_ddrq_enqueue_disable);

nss_dp_ddrq_ret_t nss_dp_ddrq_dequeue_drop(nss_dp_ddrq_obj_id_t *obj, bool drop)
{
	return nss_dp_hal_ddrq_dequeue_drop(obj, drop);
}
EXPORT_SYMBOL(nss_dp_ddrq_dequeue_drop);

/*
 * nss_dp_ddrq_occupancy_stats_reset()
 *	API to reset DDRQ occupancy stats
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_reset(void)
{
	return nss_dp_hal_ddrq_occupancy_stats_reset();
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_reset);

/*
 * nss_dp_ddrq_occupancy_stats_start()
 *	API to start DDRQ occupancy stats test instance
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_start(void)
{
	return nss_dp_hal_ddrq_occupancy_stats_start();
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_start);

/*
 * nss_dp_ddrq_occupancy_stats_stop()
 *	API to stop DDRQ occupancy stats test instance
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_stop(void)
{
	return nss_dp_hal_ddrq_occupancy_stats_stop();
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_stop);

/*
 * nss_dp_ddrq_occupancy_stats_restart()
 *	API to restart DDRQ occupancy stats test instance
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_restart(void)
{
	return nss_dp_hal_ddrq_occupancy_stats_restart();
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_restart);

/*
 * nss_dp_ddrq_occupancy_stats_threshold_set()
 *	API to set DDRQ occupancy threshold configuration
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_threshold_set(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold)
{
	return nss_dp_hal_ddrq_occupancy_stats_threshold_set(ddrq_id, threshold);
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_threshold_set);

/*
 * nss_dp_ddrq_occupancy_stats_threshold_get()
 *	API to get DDRQ occupancy threshold configuration
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_threshold_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold)
{
	return nss_dp_hal_ddrq_occupancy_stats_threshold_get(ddrq_id, threshold);
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_threshold_get);

/*
 * nss_dp_ddrq_occupancy_stats_get()
 *	API to get DDRQ occupancy stats
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_stats_t *ddrq_stats)
{
	return nss_dp_hal_ddrq_occupancy_stats_get(ddrq_id, ddrq_stats);
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_get);

/*
 * nss_dp_ddrq_occupancy_stats_status_get()
 *	API to get DDRQ occupancy test instance status
 */
nss_dp_ddrq_ret_t nss_dp_ddrq_occupancy_stats_status_get(uint32_t ddrq_id, bool *status)
{
	return nss_dp_hal_ddrq_occupancy_stats_status_get(ddrq_id, status);
}
EXPORT_SYMBOL(nss_dp_ddrq_occupancy_stats_status_get);
