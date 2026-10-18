/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef WPA_CLI_EXTN_H
#define WPA_CLI_EXTN_H

#ifdef CONFIG_QCN_EXTN
#define WPA_CLI_CMD_FIELDS_EXTN \
	"he_mcs_12_13_supp",
#else
#define WPA_CLI_CMD_FIELDS_EXTN
#endif /* CONFIG_QCN_EXTN */

#endif /* WPA_CLI_EXTN_H */
