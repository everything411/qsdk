/*
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

static const char *ipmc_status[] = {
	"status",
};

static const char *ipmc_globalcfg[] = {
	"mc_dmac_check_en",
	"vlan_mode",
	"mismatch_action",
	"hash_mode0",
	"hash_mode1",
};

static const char *ipmc_ucastfwd[] = {
	"ucast_fwd_en_port",
	"dest_vp",
	"service_code",
	"bypass_bitmap",
};

static const char *ipmc_entry[] = {
	"mode",
	"entry_id",
	"key_type",
	"vlan_valid",
	"vlan_mode",
	"vlan_id",
	"sip_addr",
	"gip_addr",
	"src_port_check_en",
	"src_port",
	"vlan_fmt_check_en",
	"vlan_fmt",
	"fwd_cmd",
	"dest_info_type",
	"dest_info_value",
	"syn_toggle",
	"ucast_fwd_en",
};

int parse_ipmc(const char *command_name, struct switch_val *val)
{
	int rv = -1;

	if (!strcmp(command_name, "Status")) {
		rv = parse_uci_option(val, ipmc_status,
				ARRAY_SIZE(ipmc_status));
	} else if (!strcmp(command_name, "Globalcfg")) {
		rv = parse_uci_option(val, ipmc_globalcfg,
				ARRAY_SIZE(ipmc_globalcfg));
	} else if (!strcmp(command_name, "Entry")) {
		rv = parse_uci_option(val, ipmc_entry,
				ARRAY_SIZE(ipmc_entry));
	} else if (!strcmp(command_name, "Ucastfwd")) {
		rv = parse_uci_option(val, ipmc_ucastfwd,
				ARRAY_SIZE(ipmc_ucastfwd));
	}

	return rv;
}

/**
 * @}
 */
