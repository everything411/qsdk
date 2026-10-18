/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_PORT_MGMT_GEN_DUMP_H_
#define _PPE_PORT_MGMT_GEN_DUMP_H_

/*
 * PORT_MGMT ISOL table rule flags.
 */
#define PPE_PORT_MGMT_ISOL_RULE_FLAG_PORT_ID			0x00000001
#define PPE_PORT_MGMT_ISOL_RULE_FLAG_ISOL_TYPE			0x00000002

/*
 * Debugfs gen dump APIs
 */
void ppe_port_mgmt_gen_dump_init(struct dentry *d_rule);
void ppe_port_mgmt_gen_dump_exit(void);

#endif
