/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_dscp.h
 *	NSS PPE DSCP definitions.
 */

#ifndef _PPE_DSCP_H_
#define _PPE_DSCP_H_

#include <linux/if.h>
#include <linux/if_ether.h>

/*
 * P_bit table config flags.
 */
#define PPE_DSCP_FLAG_DIR			0x00000001
#define PPE_DSCP_FLAG_ECN			0x00000002
#define PPE_DSCP_FLAG_DSCP			0x00000004
#define PPE_DSCP_FLAG_PCP0			0x00000008
#define PPE_DSCP_FLAG_PCP1			0x00000010

/**
 * ppe_dscp_ret
 *	DSCP return code
 */
typedef enum ppe_dscp_ret {
	PPE_DSCP_RET_SUCCESS = 0,		/**< Success */
	PPE_DSCP_RET_CONFIG_FAIL_OOM,		/**< DSCP to Pbit config failed due to OOM. */
	PPE_DSCP_RET_CONFIG_FAIL_RULE,		/**< DSCP to Pbit Table config failed. */
} ppe_dscp_ret_t;

typedef enum ppe_dscp_dir {
	PPE_DSCP_RULE_UPSTREAM_DIR = 0,		/**< UPSTREAM: Direction on DSCP_p_bit rule. */
	PPE_DSCP_RULE_DOWNSTREAM_DIR,		/**< DOWNSTREAM: Direction on DSCP_p_bit rule. */
} ppe_dscp_dir_t;

typedef enum ppe_dscp_ecn {
	PPE_DSCP_NON_ECN = 0,			/**< Non-ECN value. */
	PPE_DSCP_ECT1,				/**< ECT1 value. */
	PPE_DSCP_ECT0,				/**< ECT0 value. */
	PPE_DSCP_CE,				/**< CE value. */
} ppe_dscp_ecn_t;

/*
 * ppe_dscp_rule
 *	Priority Bit Map Table
 */
struct ppe_dscp_rule {
	ppe_dscp_dir_t dir;			/**< Rule Direction. */
	ppe_dscp_ecn_t ecn;			/**< ECN values. */
	uint8_t dscp_val;			/**< DSCP value. */
	uint8_t pcp0;				/**< PCP0 value. */
	uint8_t pcp1;				/**< PCP1 value. */

	/*
	 * P_bit config control flags.
	 */
	uint32_t p_bit_flags;			/**< P_bit config control flags. */

	/*
	 * Response
	 */
	ppe_dscp_ret_t ret;			/**< DSCP return type. */
};

/**
 * ppe_dscp_p_tbl_configure()
 *	Configure DSCP_PBIT table in PPE.
 *
 * @datatypes
 * ppe_dscp_rule
 *
 * @param[IN] rule		DSCP_PBIT rule information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_dscp_ret_t ppe_dscp_p_tbl_configure(struct ppe_dscp_rule *rule);

#endif // _PPE_DSCP_H_
