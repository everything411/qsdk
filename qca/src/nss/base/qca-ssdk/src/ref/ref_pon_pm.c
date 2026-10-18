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

static const char *pm_counter_entry[] = {
	"entry_index",
	"direction",
	"port",
	"vlan_tag_fmt",
	"vlan_id_valid",
	"vlan_id",
	"vlan_pcp_valid",
	"vlan_pcp",
	"ipmc_type",
	"counter_id",
};

int parse_ponpm(const char *command_name, struct switch_val *val)
{
	int rv = -1;

	if (!strcmp(command_name, "CounterEntry")) {
		rv = parse_uci_option(val, pm_counter_entry,
				sizeof(pm_counter_entry)/sizeof(char *));
	}

	return rv;
}

/**
 * @}
 */
