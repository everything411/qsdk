/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */


#ifndef RROPINFO_H
#define RROPINFO_H

#define MAX_NUM_CHANNELS 102

/* Represents a single RTPL instance parsed from vendor data */
struct nl80211_rtplinst {
	uint32_t primary_freq;
	int txpower_throughput;
	int txpower_range;
};

struct nl80211_rropinfo {
	int num_rtplinst;
	struct nl80211_rtplinst rtpl[MAX_NUM_CHANNELS];
};

#endif /* RROPINFO_H */

