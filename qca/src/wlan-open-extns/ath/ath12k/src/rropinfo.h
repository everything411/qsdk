/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */


#ifndef RROPINFO_H
#define RROPINFO_H

/* Represents a single RTPL (Representative Tx Power List) instance */
struct rtplinst_extn {
	u32 primary_freq;
	int txpower_throughput;
	int txpower_range;
};

#endif /* RROPINFO_H */
