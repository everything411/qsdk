/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Buffer sizes
 */
#define NETFN_FLOW_COOKIE_DUMP_FILE_PREFIX_SIZE 128
#define NETFN_FLOW_COOKIE_DUMP_FILE_PREFIX_LEVELS_MAX 10
#define NETFN_FLOW_COOKIE_DUMP_FILE_BUFFER_SIZE 3072000

/*
 * struct netfn_flow_cookie_dump_file_instance
 *	Structure used as state per open instance of our db state file
 */
struct netfn_flow_cookie_dump_instance {
	char prefix[NETFN_FLOW_COOKIE_DUMP_FILE_PREFIX_SIZE];			/* This is the prefix added to every message written */
	int prefix_levels[NETFN_FLOW_COOKIE_DUMP_FILE_PREFIX_LEVELS_MAX];	/* How many nested prefixes supported */
	int prefix_level;                               			/* Prefix nest level */
	char msg[NETFN_FLOW_COOKIE_DUMP_FILE_BUFFER_SIZE];			/* The message written / being returned to the reader */
	char *msgp;								/* Points into the msg buffer as we output it to the reader piece by piece */
	int msg_len;								/* Length of the msg buffer still to be written out */
	bool dump_en;								/* Enable dump once file is open */
	uintptr_t fcdb_addr;							/* DB Address */
};

/*
 * Netfn Flow Cookie Dump operations.
 */
extern int netfn_flow_cookie_dump_init(struct netfn_flow_cookie_db *db);
extern void netfn_flow_cookie_dump_exit(struct netfn_flow_cookie_db *db);
