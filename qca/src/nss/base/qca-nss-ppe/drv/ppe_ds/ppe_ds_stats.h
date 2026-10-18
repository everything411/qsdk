/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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


#ifndef _PPE_DS_STATS_H_
#define _PPE_DS_STATS_H_

#include <linux/atomic.h>
#include "ppe_ds.h"

#define PPE_DS_VLD_RING_ENTR_HYS_MAX 8
#define PPE_DS_IDX_MV_HYS_MAX 12
#define PPE_DS_TS_HYS_MAX 19
/*
 * PPE-DS stats per Node
 */
struct ppe_ds_stats {
	atomic64_t tx_pkts;
	atomic64_t rx_pkts;
	atomic64_t ppe2tcl_still_prod;
	atomic64_t ppe2tcl_still_cons;
	atomic64_t wlan_ppe2tcl_intr_cnt;
	atomic64_t prod_n_con_same;
	atomic64_t reo2ppe_still_cons;
	atomic64_t wlan_reo2ppe_intr_cnt;
};

/*
 * PPE-DS idx movement stats per Node
 */
struct ppe_ds_idx_hys {
	atomic_t edma_prod_mv[PPE_DS_IDX_MV_HYS_MAX];
	struct {
		atomic_t vld_ring_entr[PPE_DS_VLD_RING_ENTR_HYS_MAX];	/* #valid ring entries: diff of prod and cons idx  */
		atomic_t cons_idx_mv[PPE_DS_IDX_MV_HYS_MAX];		/* index movement of cons idx index in the ring */
		atomic_t prod_idx_mv[PPE_DS_IDX_MV_HYS_MAX];		/* index movement of prod idx index in the ring */
	} ppe2tcl, reo2ppe;
};

/*
 * PPE-DS timestamp stats per Node
 */
struct ppe_ds_ts_hys {
	struct {
		atomic64_t ts_bt_intr[PPE_DS_TS_HYS_MAX];
		atomic64_t waste_intr[PPE_DS_TS_HYS_MAX];
	} ppe2tcl, reo2ppe;
};

/*
 * PPE-DS range definition for stats.
 */
typedef struct {
	int end;
	int start;
	char tag[50];
} Range;

/*
 * Hysteresis ranges for PPE-DS index movements
 * for stats collection.
 */
struct ppe_ds_idx_hys_range_list {
	Range vld_ring_entr_range[PPE_DS_VLD_RING_ENTR_HYS_MAX];
	Range idx_mv_range[PPE_DS_IDX_MV_HYS_MAX];
};

/*
 * Hysteresis ranges for PPE-DS interrupt timestamps
 * for stats collection.
 */
struct ppe_ds_ts_hys_range_list {
	Range ppe_ds_ts_hys_range[PPE_DS_TS_HYS_MAX];
};

extern struct ppe_ds_stats ppe_ds_node_stats[PPE_DS_MAX_NODE];
extern struct ppe_ds_idx_hys ppe_ds_idx_hyst[PPE_DS_MAX_NODE];
extern struct ppe_ds_ts_hys ppe_ds_ts_hyst[PPE_DS_MAX_NODE];
extern struct ppe_ds_ts_hys_range_list ppe_ds_ts_hys_range;
extern struct ppe_ds_idx_hys_range_list ppe_ds_idx_hys_range;
extern uint32_t enable_ring_hyst_stats;
int ppe_ds_node_stats_debugfs_init(void);
void ppe_ds_node_stats_debugfs_exit(void);
#endif
