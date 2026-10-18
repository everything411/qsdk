/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_VLAN_DUMP_H
#define __PPE_VLAN_DUMP_H

/*
 * Buffer sizes
 */
#define PPE_VLAN_DUMP_PREFIX_SIZE 64
#define PPE_VLAN_DUMP_PREFIX_MAX 6
#define PPE_VLAN_DUMP_BUFFER_SIZE 3072000

/*
 * struct ppe_vlan_dump_instance
 *	Structure used as an instance for vlan dump
 */
struct ppe_vlan_dump_instance {
	uint8_t vlan_cnt;				/* Number of VLAN rules  */
	char prefix[PPE_VLAN_DUMP_PREFIX_SIZE];		/* This is the prefix added to every message written */
	int prefix_levels[PPE_VLAN_DUMP_PREFIX_MAX];	/* How many nested prefixes supported */
	int prefix_level;				/* Prefix next level */
	char msg[PPE_VLAN_DUMP_BUFFER_SIZE];		/* The message written / being returned to the reader */
	char *msgp;					/* Points into the msg buffer as we output it to the reader piece by piece */
	int msg_len;					/* Length of the msg buffer still to be written out */
	bool dump_en;					/* Enable dump once the file is open */
};

extern bool ppe_vlan_dump_init(struct dentry *dentry);
extern void ppe_vlan_dump_exit(void);

#endif

