/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_MCAST_DUMP_H
#define __PPE_MCAST_DUMP_H

/*
 * Buffer sizes
 */
#define PPE_MCAST_DUMP_PREFIX_SIZE 64
#define PPE_MCAST_DUMP_PREFIX_MAX 10
#define PPE_MCAST_DUMP_BUFFER_SIZE 3072000

/*
 * struct ppe_mcast_dump_instance
 *	An instance for Qos traffic management dump
 */
struct ppe_mcast_dump_instance {
	uint8_t mcgrp_cnt;				/* Number of multicast groups  */
	char prefix[PPE_MCAST_DUMP_PREFIX_SIZE];	/* This is the prefix added to every message written */
	int prefix_levels[PPE_MCAST_DUMP_PREFIX_MAX];	/* How many nested prefixes supported */
	int prefix_level;				/* Prefix next level */
	char msg[PPE_MCAST_DUMP_BUFFER_SIZE];	/* The message written / being returned to the reader */
	char *msgp;					/* Points into the msg buffer as we output it to the reader piece by piece */
	int msg_len;					/* Length of the msg buffer still to be written out */
	bool dump_en;					/* Enable dump once the file is open */
};

extern bool ppe_mcast_dump_init(struct dentry *dentry);
extern void ppe_mcast_dump_exit(void);

#endif
