/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_GRE_MGR_STATS_H
#define __PPE_GRE_MGR_STATS_H

#include <nss_ppe_tun_drv.h>

bool ppe_gre_mgr_stats_dentry_create(struct ppe_gre_mgr_ctx *ppe_ctx, struct net_device *dev);
bool ppe_gre_mgr_stats_dentry_free(struct ppe_gre_mgr_ctx *ppe_ctx, struct net_device *dev);
bool ppe_gre_mgr_dev_stats_update(struct net_device *dev, ppe_tun_hw_stats *stats, ppe_tun_data *tun_data);
struct dentry *ppe_gre_mgr_stats_debugfs_init(struct ppe_gre_mgr_ctx *ppe_ctx);

#endif
