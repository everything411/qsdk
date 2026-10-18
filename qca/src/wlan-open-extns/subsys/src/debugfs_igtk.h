/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __MAC80211_DEBUGFS_IGTK_H
#define __MAC80211_DEBUGFS_IGTK_H

struct ieee80211_sub_if_data;

#ifdef CPTCFG_QCN_EXTN
void ieee80211_debugfs_igtk_add(struct ieee80211_sub_if_data *sdata);
#else
static inline void
ieee80211_debugfs_igtk_add(struct ieee80211_sub_if_data *sdata) {}
#endif /* CPTCFG_QCN_EXTN */

#endif /* __MAC80211_DEBUGFS_IGTK_H */