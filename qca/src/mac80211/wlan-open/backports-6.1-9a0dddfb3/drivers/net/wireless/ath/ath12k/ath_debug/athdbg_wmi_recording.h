/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef _ATHDBG_WMI_RECORDING_H_
#define _ATHDBG_WMI_RECORDING_H_

#include <linux/debugfs.h>
#include "athdbg_core.h"

struct dentry;
struct ath12k_base;
struct athdbg_request;

enum athdbg_wmi_request {
	ATHDBG_WMI_REQ_INVALID = 0,
	ATHDBG_WMI_REQ_ENABLE,
	ATHDBG_WMI_REQ_DUMP,
	ATHDBG_WMI_REQ_VERBOSITY,
};

enum athdbg_wmi_rec_verbosity {
	ATHDBG_WMI_REC_VERBOSE_L0  = 0,
	ATHDBG_WMI_REC_VERBOSE_L1  = 1,
	ATHDBG_WMI_REC_VERBOSE_L2  = 2,
};

enum athdbg_wmi_rec_print {
	ATHDBG_WMI_REC_CMD_PRINT  = 1,
	ATHDBG_WMI_REC_CMD_TX_PRINT  = 2,
	ATHDBG_WMI_REC_EVT_PRINT  = 3,
};

enum athdbg_wmi_rec_enable {
	ATHDBG_WMI_REC_DISABLE,
	ATHDBG_WMI_REC_ENABLE_DEF,
};

extern struct ath_debug_base *athdbg_base;
enum athdbg_wmi_request athdbg_wmi_get_req_from_name(const char *filename);
void athdbg_process_wmi_request(struct ath12k_base *ab,
				struct athdbg_request *dbg_req);
void athdbg_create_wmi_debugfs(struct dentry *dbg_dir, struct ath12k_base *drv_ab);

#endif /* _ATHDBG_WMI_RECORDING_H_ */

