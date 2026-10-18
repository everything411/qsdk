/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef AFC_H
#define AFC_H

#define QCA_NUM_6G_CHAN 1500

enum qca_6g_power_type {
	QCA_6G_PWR_LPI,
	QCA_6G_PWR_SP,
	QCA_6G_PWR_VLP,
	QCA_6G_PWR_MAX,
};

enum qca_6g_list_bw {
	QCA_6G_BW_20,
	QCA_6G_BW_40,
	QCA_6G_BW_80,
	QCA_6G_BW_160,
	QCA_6G_BW_320,
	QCA_6G_BW_MAX,
};

#endif
