/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
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

#if defined(JHPPE)
static const char *pktedit_en[] = {
	"cpucode",
	"enable",
};

#endif
int parse_pktedit(const char *command_name, struct switch_val *val)
{
	int rv = -1;

	if (!strcmp(command_name, "Padding")) {
		rv = parse_uci_option(val, pktedit_padding,
				sizeof(pktedit_padding)/sizeof(char *));
#if defined(JHPPE)
	} else if (!strcmp(command_name, "En")) {
		rv = parse_uci_option(val, pktedit_en,
				sizeof(pktedit_en)/sizeof(char *));
	} else if (!strcmp(command_name, "RxdescCpucodeEn")) {
		rv = parse_uci_option(val, pktedit_en,
				sizeof(pktedit_en)/sizeof(char *));
#endif
	}

	return rv;
}

/**
 * @}
 */
