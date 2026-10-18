// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/errno.h>

#include "core.h"
#include "dp.h"
#include "dp_tx.h"
#include "me.h"
#include "debug.h"

static void ath12k_me_stats_aggregate(struct ath12k_dp_vif *dp_vif,
				      struct ath12k_me_stats *agg)
{
	int ring;

	memset(agg, 0, sizeof(*agg));

	for (ring = 0; ring < DP_TCL_NUM_RING_MAX; ring++) {
		struct ath12k_me_stats *st = &dp_vif->stats[ring].tx_i.me;

		agg->total_mc += st->total_mc;
		agg->me_mcuc += st->me_mcuc;
		agg->me5_hits += st->me5_hits;
		agg->me6_hits += st->me6_hits;
		agg->me_fail += st->me_fail;
	}
}

int ath12k_me_info_print_extn(struct ath12k_me_db *me_db,
			struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_me_stats agg;
	u32 unicast_pkts;
	u32 bypass_pkts;
	bool is_igmp;
	u32 me_val;

	if (!me_db || !dp_vif)
		return -ENOENT;

	ath12k_me_stats_aggregate(dp_vif, &agg);

	if (me_db->me_flags & ATH12K_ME_FLAGS_BIT_ME5)
		me_val = 5;
	else if (me_db->me_flags & ATH12K_ME_FLAGS_BIT_ME6)
		me_val = 6;
	else
		me_val = 0;

	is_igmp = !!(me_db->me_flags & ATH12K_ME_FLAGS_BIT_IGMP_EN);

	bypass_pkts  = (agg.total_mc >= agg.me_mcuc) ? (agg.total_mc - agg.me_mcuc) : 0;
	unicast_pkts  = agg.me5_hits + agg.me6_hits;

	ath12k_info(NULL, "=== Info Dump ===\n");
	ath12k_info(NULL,
		    "ME Config: %u\nIGMP Enabled: %s\nGrp Limit: %u\n",
		    me_val,
		    is_igmp ? "Yes" : "No",
		    me_db->grp_limit);

	ath12k_info(NULL,
		    "Total Multicast packets: %u\n"
		    "ME M2U packets: %u\n"
		    "ME Unicast packets (ME5+ME6): %u\n"
		    "ME5 Hits: %u\n"
		    "ME6 Hits: %u\n"
		    "ME Bypass: %u\n"
		    "ME Fail: %u\n",
		    agg.total_mc, agg.me_mcuc,
		    unicast_pkts,
		    agg.me5_hits,
		    agg.me6_hits,
		    bypass_pkts,
		    agg.me_fail);

	return 0;
}

