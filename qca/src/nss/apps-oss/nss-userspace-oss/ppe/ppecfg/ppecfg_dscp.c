/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG DSCP handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include "nss_ppenl_dscp_if.h"
#include "ppecfg_param.h"
#include "ppecfg_dscp.h"

#define MANDATORY_DSCP_ADD_FIELDS_MASK 0x07  /* DSCP rule add mandatory field */

static int ppecfg_dscp_rule_config(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * dscp_p_bit rule pcp add parameters
 */
static struct ppecfg_param pcp_params[PPECFG_DSCP_PCP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DSCP_PCP0, "pcp0="),
	PPECFG_PARAM_INIT(PPECFG_DSCP_PCP1, "pcp1="),
};

/*
 * dscp_p_bit rule add parameters
 */
static struct ppecfg_param dscp_rule_add_params[PPECFG_DSCP_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DSCP_RULE_ADD_DIR, "rule_dir="),
	PPECFG_PARAM_INIT(PPECFG_DSCP_RULE_ADD_ECN, "ecn="),
	PPECFG_PARAM_INIT(PPECFG_DSCP_RULE_ADD_DSCP, "dscp="),
	PPECFG_PARAMARR_INIT(PPECFG_DSCP_RULE_ADD_PCP, "pcp", pcp_params, ppecfg_param_iter_tbl),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_dscp_cmd' should also get updated
 */
struct ppecfg_param ppecfg_dscp_params[PPECFG_DSCP_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=dscp_config", dscp_rule_add_params, ppecfg_dscp_rule_config),
};

/*
 * ppecfg_dscp_rule_config()
 *	Handle dscp to p-bit map table fields config.
 */
static int ppecfg_dscp_rule_config(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dscp_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
	uint8_t field_mask = 0;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_dscp_init_rule(&nl_msg, NSS_PPE_DSCP_CONFIG_RULE_MSG);

	for (int index = PPECFG_DSCP_RULE_ADD_DIR; index <= PPECFG_DSCP_RULE_ADD_PCP; index++) {
		sub_params = &param->sub_params[index];

		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
			case PPECFG_DSCP_RULE_ADD_DIR:
			{
				char dir[12];

				error = ppecfg_param_get_str(sub_params->data, sizeof(dir), dir);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				/*
				 * Compare string and set the appropriate enum value
				 */
				if (strcasecmp(dir, "upstream") == 0) {
					nl_msg.rule.dir = PPE_DSCP_RULE_UPSTREAM_DIR;
				} else if (strcasecmp(dir, "downstream") == 0) {
					nl_msg.rule.dir = PPE_DSCP_RULE_DOWNSTREAM_DIR;
				} else {
					ppecfg_log_error("Invalid direction: %s. Valid values are 'upstream' or 'downstream'\n", dir);
					error = -EINVAL;
					goto done;
				}

				nl_msg.rule.p_bit_flags |= PPE_DSCP_FLAG_DIR;

				field_mask |= 0x01;
				break;
			}

			case PPECFG_DSCP_RULE_ADD_ECN:
			{
				char ecn[8];

				error = ppecfg_param_get_str(sub_params->data, sizeof(ecn), ecn);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(ecn, "non_ecn") == 0) {
					nl_msg.rule.ecn = PPE_DSCP_NON_ECN;
				} else if (strcasecmp(ecn, "ect1") == 0) {
					nl_msg.rule.ecn = PPE_DSCP_ECT1;
				} else if (strcasecmp(ecn, "ect0") == 0) {
					nl_msg.rule.ecn = PPE_DSCP_ECT0;
				} else if (strcasecmp(ecn, "ce") == 0) {
					nl_msg.rule.ecn = PPE_DSCP_CE;
				} else {
					ppecfg_log_error("Invalid ECN: %s. Valid values: 'non_ecn', 'ect1', 'ect0', 'ce'\n", ecn);
					error = -EINVAL;
					goto done;
				}

				nl_msg.rule.p_bit_flags |= PPE_DSCP_FLAG_ECN;

				break;
			}

			case PPECFG_DSCP_RULE_ADD_DSCP:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.dscp_val);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				/*
				 * DSCP is a 6-bit field (0..63).
				 */
				if (nl_msg.rule.dscp_val > 63) {
					ppecfg_log_error("Invalid dscp=%u. Valid range is 0-63\n", nl_msg.rule.dscp_val);
					error = -EINVAL;
					goto done;
				}

				nl_msg.rule.p_bit_flags |= PPE_DSCP_FLAG_DSCP;

				field_mask |= 0x02;
				break;

			case PPECFG_DSCP_RULE_ADD_PCP:
				sub_params = param->sub_params[PPECFG_DSCP_RULE_ADD_PCP].sub_params;

				data = sub_params[PPECFG_DSCP_PCP0].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.pcp0);
					if (error < 0) {
						ppecfg_log_arg_error(sub_params);
						goto print_error;
					}

					/*
					 * PCP/P-bit is a 3-bit field (0..7).
					 */
					if (nl_msg.rule.pcp0 > 7) {
						ppecfg_log_error("Invalid pcp0=%u. Valid range is 0-7\n", nl_msg.rule.pcp0);
						error = -EINVAL;
						goto done;
					}

					nl_msg.rule.p_bit_flags |= PPE_DSCP_FLAG_PCP0;
					field_mask |= 0x04;
				}

				data = sub_params[PPECFG_DSCP_PCP1].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.pcp1);
					if (error < 0) {
						ppecfg_log_arg_error(sub_params);
						goto print_error;
					}

					/*
					 * PCP/P-bit is a 3-bit field (0..7).
					 */
					if (nl_msg.rule.pcp1 > 7) {
						ppecfg_log_error("Invalid pcp1=%u. Valid range is 0-7\n", nl_msg.rule.pcp1);
						error = -EINVAL;
						goto done;
					}

					nl_msg.rule.p_bit_flags |= PPE_DSCP_FLAG_PCP1;
					field_mask |= 0x04;
				}

				break;
		}
	}

	if ((field_mask & MANDATORY_DSCP_ADD_FIELDS_MASK) != MANDATORY_DSCP_ADD_FIELDS_MASK) {
		ppecfg_log_warn("Missing mandatory field: [rule_dir] [dscp] [pcp0/pcp1]\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_dscp_rule_config(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		return error;
	}

	return error;

print_error:
	ppecfg_log_data_error(sub_params);

done:
	return error;
}
