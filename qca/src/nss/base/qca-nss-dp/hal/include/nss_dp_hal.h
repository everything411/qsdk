/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NSS_DP_HAL_H__
#define __NSS_DP_HAL_H__

#include "nss_dp_dev.h"

/*
 * nss_dp_hal_get_gmac_ops()
 *	Returns gmac hal ops based on the GMAC type.
 */
static inline struct nss_gmac_hal_ops *nss_dp_hal_get_gmac_ops(uint32_t gmac_type)
{
	return dp_global_ctx.gmac_hal_ops[gmac_type];
}

/*
 * nss_dp_hal_set_gmac_ops()
 *	Sets dp global gmac hal ops based on the GMAC type.
 */
static inline void nss_dp_hal_set_gmac_ops(struct nss_gmac_hal_ops *hal_ops, uint32_t gmac_type)
{
	dp_global_ctx.gmac_hal_ops[gmac_type] = hal_ops;
}

/*
 * HAL functions implemented by SoC specific source files.
 */
extern struct nss_dp_data_plane_ops *nss_dp_hal_get_data_plane_ops(void);
extern bool nss_dp_hal_init(void);
extern void nss_dp_hal_cleanup(void);
extern struct nss_dp_ppeds_ops *nss_dp_ppeds_wifi_arch_mode_ops_get(uint32_t mode);
extern void nss_dp_hal_init_soc_priv_flags(struct nss_dp_dev *dp_priv);
extern void nss_dp_hal_deinit_soc_priv_flags(struct nss_dp_dev *dp_priv);

#ifdef NSS_DP_DDRQ_SUPPORT
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_cfg_get(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg, uint32_t count);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_cfg_set(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_grp_cfg_get(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_grp_cfg_set(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_enqueue_disable(nss_dp_ddrq_obj_id_t *obj, bool disable);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_dequeue_drop(nss_dp_ddrq_obj_id_t *obj, bool drop);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_reset(void);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_start(void);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_stop(void);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_restart(void);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_threshold_set(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_threshold_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_stats_t *ddrq_stats);
extern nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_status_get(uint32_t ddrq_id, bool *status);
#endif

#endif	/* __NSS_DP_HAL_H__ */
