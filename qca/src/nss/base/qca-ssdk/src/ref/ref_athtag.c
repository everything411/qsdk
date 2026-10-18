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

static const char *athtag_primapping[] = {
	"direction",
	"ath_pri",
	"int_pri",
};

static const char *athtag_portmapping[] = {
	"direction",
	"ath_port",
	"int_port",
};

static const char *athtag_rx[] = {
	"port_id",
	"athtag_en",
	"athtag_type",
#if defined(JHPPE) || defined(HTTPPE)
	"athtag_version",
#endif
};

static const char *athtag_tx[] = {
	"port_id",
	"athtag_en",
	"athtag_type",
	"athtag_version",
	"athtag_action",
	"athtag_bypass_fwd_en",
	"athtag_field_disable",
};

#if defined(JHPPE) || defined(HTTPPE)
static const char *athtag_rx_src_port_mapping[] = {
	"ath_src_port",
	"int_dest_info_type",
	"int_dest_info_value",
};

static const char *athtag_rx_dest_port_mapping[] = {
	"ath_dest_port",
	"int_dest_info_type",
	"int_dest_info_value",
};

static const char *athtag_rx_servcode_mapping[] = {
	"ath_servcode",
	"int_servcode",
};

static const char *athtag_tx_src_port_mapping[] = {
	"int_src_port",
	"ath_src_port",
};
#endif

int parse_athtag(const char *command_name, struct switch_val *val)
{
	int rv = -1;

	if (!strcmp(command_name, "Primapping")) {
		rv = parse_uci_option(val, athtag_primapping,
				sizeof(athtag_primapping)/sizeof(char *));
	} else if (!strcmp(command_name, "Portmapping")) {
		rv = parse_uci_option(val, athtag_portmapping,
				sizeof(athtag_portmapping)/sizeof(char *));
	} else if (!strcmp(command_name, "Rx")) {
		rv = parse_uci_option(val, athtag_rx,
				sizeof(athtag_rx)/sizeof(char *));
	} else if (!strcmp(command_name, "Tx")) {
		rv = parse_uci_option(val, athtag_tx,
				sizeof(athtag_tx)/sizeof(char *));
#if defined(JHPPE) || defined(HTTPPE)
	} else if (!strcmp(command_name, "Rxsrcportmapping")) {
		rv = parse_uci_option(val, athtag_rx_src_port_mapping,
				sizeof(athtag_rx_src_port_mapping)/sizeof(char *));
	} else if (!strcmp(command_name, "Rxdestportmapping")) {
		rv = parse_uci_option(val, athtag_rx_dest_port_mapping,
				sizeof(athtag_rx_dest_port_mapping)/sizeof(char *));
	} else if (!strcmp(command_name, "Rxservcodemapping")) {
		rv = parse_uci_option(val, athtag_rx_servcode_mapping,
				sizeof(athtag_rx_servcode_mapping)/sizeof(char *));
	} else if (!strcmp(command_name, "Txsrcportmapping")) {
		rv = parse_uci_option(val, athtag_tx_src_port_mapping,
				sizeof(athtag_tx_src_port_mapping)/sizeof(char *));
#endif
	}

	return rv;
}

/**
 * @}
 */
