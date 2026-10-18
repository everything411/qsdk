/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_POLICER_DUMP_H
#define __PPE_POLICER_DUMP_H

/*
 * Buffer sizes
 */
#define PPE_POLICER_DUMP_PREFIX_SIZE 64
#define PPE_POLICER_DUMP_PREFIX_MAX 6
#define PPE_POLICER_DUMP_BUFFER_SIZE 3072000

/*
 * struct ppe_policer_dump_instance
 *	Structure used as an instance for policer dump
 */
struct ppe_policer_dump_instance {
	uint16_t acl_policer_cnt;				/* Number of ACL POLICER rules  */
	uint16_t port_policer_cnt;				/* Number of Port POLICER rules  */
	char prefix[PPE_POLICER_DUMP_PREFIX_SIZE];		/* Prefix added to every msg written */
	int prefix_levels[PPE_POLICER_DUMP_PREFIX_MAX];	/* How many nested prefixes supported */
	int prefix_level;				/* Prefix nest level */
	char msg[PPE_POLICER_DUMP_BUFFER_SIZE];		/* The message written / being returned to the reader */
	char *msgp;					/* Pointer to current position in msg buffer for output */
	int msg_len;					/* Length of the msg buffer still to be written out */
	bool dump_en;					/* Enable dump once the file is open */
};

extern bool ppe_policer_dump_init(struct dentry *dentry);
extern void ppe_policer_dump_exit(void);

#endif
