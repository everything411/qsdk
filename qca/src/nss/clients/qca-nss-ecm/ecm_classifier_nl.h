/*
 **************************************************************************
 * Copyright (c) 2014-2015, The Linux Foundation.  All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/if_ether.h>

struct ecm_classifier_nl_instance;

struct ecm_classifier_nl_instance *
ecm_classifier_nl_instance_alloc(struct ecm_db_connection_instance *ci);

/*
 *  Callback for for conmark updates
 *
 *  Saves the new mark to the per-connection context and decelerates
 *  the connection if offloaded.
 */
void ecm_classifier_nl_process_mark(struct ecm_classifier_nl_instance *cnli,
				    uint32_t mark);

/*
 * Generic Netlink defines and structs
 */
#define ECM_CL_NL_GENL_VERSION	(1)			/* API version */
#define ECM_CL_NL_GENL_NAME	"ECMCLNL"		/* family name */
#define ECM_CL_NL_GENL_MCGRP	"ECMCLNL_MCGRP"		/* multicast group */

enum ECM_CL_NL_GENL_CMD {
	ECM_CL_NL_GENL_CMD_UNSPEC,
	ECM_CL_NL_GENL_CMD_ACCEL,
	ECM_CL_NL_GENL_CMD_ACCEL_OK,
	ECM_CL_NL_GENL_CMD_CONNECTION_CLOSED,
	ECM_CL_NL_GENL_CMD_COUNT,
};
#define ECM_CL_NL_GENL_CMD_MAX (ECM_CL_NL_GENL_CMD_COUNT - 1)

enum ECM_CL_NL_GENL_ATTR {
	ECM_CL_NL_GENL_ATTR_UNSPEC,
	ECM_CL_NL_GENL_ATTR_TUPLE,
	ECM_CL_NL_GENL_ATTR_COUNT,
};
#define ECM_CL_NL_GENL_ATTR_MAX (ECM_CL_NL_GENL_ATTR_COUNT - 1)

union ecm_cl_nl_genl_attr_ip {
	struct in_addr in;
	struct in6_addr in6;
};

/* network order */
struct ecm_cl_nl_genl_attr_tuple {
	uint16_t	af;
	uint8_t		proto;
	union ecm_cl_nl_genl_attr_ip src_ip;
	union ecm_cl_nl_genl_attr_ip dst_ip;
	uint16_t	src_port;
	uint16_t	dst_port;
	uint8_t		src_mac[ETH_ALEN];
	uint8_t		dest_mac[ETH_ALEN];
};

int ecm_classifier_nl_rules_init(struct dentry *dentry);
void ecm_classifier_nl_rules_exit(void);
