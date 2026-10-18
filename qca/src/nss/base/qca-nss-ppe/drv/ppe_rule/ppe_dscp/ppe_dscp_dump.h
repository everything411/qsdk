/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DSCP_DUMP_H_
#define _PPE_DSCP_DUMP_H_

#include "ppe_dscp.h"

/*
 * Buffer sizes
 */
#define PPE_DSCP_DUMP_BUFFER_SIZE 65536	/* 64KB Buffer size */

/*
 * struct ppe_dscp_dump_instance
 *	Structure used as an instance for dscp dump
 */
struct ppe_dscp_dump_instance {
	char msg[PPE_DSCP_DUMP_BUFFER_SIZE];		/* The message written / being returned to the reader */
	char *msgp;					/* Points into the msg buffer as we output it to the reader piece by piece */
	int msg_len;					/* Length of the msg buffer still to be written out */
	bool dump_en;					/* Enable dump once the file is open */
};

/*
 * ppe_dscp_dump_init()
 *	Initialize DSCP dump.
 */
bool ppe_dscp_dump_init(struct dentry *d_rule);

/*
 * ppe_dscp_dump_deinit()
 *	De-initialize DSCP dump.
 */
void ppe_dscp_dump_deinit(void);

#endif /* _PPE_DSCP_DUMP_H_ */
