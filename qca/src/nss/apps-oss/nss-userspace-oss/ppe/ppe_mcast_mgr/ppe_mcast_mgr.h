/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_MCAST_MGR_H
#define __PPE_MCAST_MGR_H

#include <signal.h>

#define PPE_MCAST_MGR_COLOR_RST "\x1b[0m"
#define PPE_MCAST_MGR_COLOR_GRN "\x1b[32m"
#define PPE_MCAST_MGR_COLOR_RED "\x1b[31m"
#define PPE_MCAST_MGR_COLOR_MGT "\x1b[35m"

#define ppe_mcast_mgr_log_error(fmt, arg...) printf(PPE_MCAST_MGR_COLOR_RED"[ERR]"PPE_MCAST_MGR_COLOR_RST fmt, ## arg)
#define ppe_mcast_mgr_log_info(fmt, arg...) printf(PPE_MCAST_MGR_COLOR_GRN"[INF]"PPE_MCAST_MGR_COLOR_RST fmt, ## arg)
#define ppe_mcast_mgr_log_trace(fmt, arg...) printf(PPE_MCAST_MGR_COLOR_MGT"[TRC(<%s>)]"PPE_MCAST_MGR_COLOR_RST fmt, __func__, ## arg)
#define ppe_mcast_mgr_log_warn(fmt, arg...) printf(PPE_MCAST_MGR_COLOR_RED"[WARN]"PPE_MCAST_MGR_COLOR_RST fmt, ##arg)

#endif /* __PPE_MCAST_MGR_H*/

