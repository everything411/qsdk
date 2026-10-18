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


/**
 * @defgroup
 * @{
 */
#include <linux/switch.h>
#include "sw.h"
#include "ref_uci.h"

static const char *pktedit_padding[] = {
	"strip_padding_en",
	"strip_padding_route_en",
	"strip_padding_bridge_en",
	"strip_padding_checksum_en",
	"strip_padding_snap_en",
	"strip_tunnel_inner_padding_en",
	"tunnel_inner_padding_exp_en",
	"tunnel_ip_len_gap_exp_en",
};

int parse_pktedit(const char *command_name, struct switch_val *val)
{
	int rv = -1;

	if (!strcmp(command_name, "Padding")) {
		rv = parse_uci_option(val, pktedit_padding,
				sizeof(pktedit_padding)/sizeof(char *));
	}
	return rv;
}

/**
 * @}
 */
