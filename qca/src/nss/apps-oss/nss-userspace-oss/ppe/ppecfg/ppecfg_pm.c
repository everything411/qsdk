/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG PM handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include "nss_ppenl_pm_if.h"
#include "ppecfg_param.h"
#include "ppecfg_pm.h"

#define FLOW_DIR_MASK		0x01
#define COUNTER_ID_MASK		0x02
#define PORT_TYPE_MASK		0x04
#define PORT_INFO_MASK		0x08
#define OPTIONAL_FIELDS_MASK	0xFC

static int ppecfg_pm_counter_get(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_pm_counter_gen_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_pm_counter_gen_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * PM counter_get parameters
 */
static struct ppecfg_param counter_get_params[PPECFG_PM_COUNTER_GET_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GET_COUNTER_ID, "counter_id="),
};

/*
 * PM counter_gen rule add parameters
 */
static struct ppecfg_param counter_gen_rule_add_params[PPECFG_PM_COUNTER_GEN_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_RULE_DIR, "rule_dir="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_PM_DIR, "pm_dir="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_COUNTER_ID, "counter_id="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_PORT_TYPE, "port_type="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_PORT_INFO, "port_info="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_TAG_FORMAT, "tag_format="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_VID, "vid="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_PCP, "pcp="),
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_ADD_IPMC, "ipmc="),
};

/*
 * PM counter_gen rule del parameters
 */
static struct ppecfg_param counter_gen_rule_del_params[PPECFG_PM_COUNTER_GEN_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PM_COUNTER_GEN_RULE_DEL_COUNTER_ID, "counter_id="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_pm_cmd' should also get updated
 */
struct ppecfg_param ppecfg_pm_params[PPECFG_PM_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=counter_get", counter_get_params, ppecfg_pm_counter_get),
	PPECFG_PARAMLIST_INIT("cmd=counter_gen_rule_add", counter_gen_rule_add_params, ppecfg_pm_counter_gen_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=counter_gen_rule_del", counter_gen_rule_del_params, ppecfg_pm_counter_gen_rule_del),
};

/*
 * ppecfg_pm_counter_get()
 *	Handle PM counter stats fetching based on rule_id
 */
static int ppecfg_pm_counter_get(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_pm_info nl_msg = {{0}};
	int error;

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
	nss_ppenl_pm_counter_init(&nl_msg, NSS_PPE_PM_COUNTER_GET_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_PM_COUNTER_GET_COUNTER_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.counter_info.counter_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_pm_counter_get(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;

}

/*
 * ppecfg_pm_counter_gen_rule_add()
 *	Handle PM counter generation table rule addition
 */
static int ppecfg_pm_counter_gen_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_pm_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
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
	nss_ppenl_pm_counter_init(&nl_msg, NSS_PPE_PM_COUNTER_GEN_CREATE_RULE_MSG);

	for (int index = PPECFG_PM_COUNTER_GEN_RULE_ADD_RULE_DIR; index < PPECFG_PM_COUNTER_GEN_RULE_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];

		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
			case PPECFG_PM_COUNTER_GEN_RULE_ADD_RULE_DIR:
			{
				char dir[11];

				error = ppecfg_param_get_str(sub_params->data, sizeof(dir), dir);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(dir, "upstream") == 0) {
					nl_msg.counter_gen.rule_dir = PPE_PM_RULE_DIR_INGRESS;
				} else if (strcasecmp(dir, "downstream") == 0) {
					nl_msg.counter_gen.rule_dir = PPE_PM_RULE_DIR_EGRESS;
				} else {
					ppecfg_log_error("Invalid rule_dir: %s. Valid: 'upstream', 'downstream'\n", dir);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_RULE_DIR;
				field_mask |= 0x01;
				break;
			}

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_PM_DIR:
			{
				char pm_dir[8];

				error = ppecfg_param_get_str(sub_params->data, sizeof(pm_dir), pm_dir);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(pm_dir, "ingress") == 0) {
					nl_msg.counter_gen.pm_dir = PPE_PM_RULE_DIR_INGRESS;
				} else if (strcasecmp(pm_dir, "egress") == 0) {
					nl_msg.counter_gen.pm_dir = PPE_PM_RULE_DIR_EGRESS;
				} else {
					ppecfg_log_error("Invalid pm_dir: %s. Valid: 'ingress', 'egress'\n", pm_dir);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_PM_DIR;
				break;
			}

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_COUNTER_ID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.counter_gen.counter_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_COUNTER_ID;

				field_mask |= 0x02;
				break;

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_PORT_TYPE:
			{
				char port_type[9];

				error = ppecfg_param_get_str(sub_params->data, sizeof(port_type), port_type);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(port_type, "bitmap") == 0) {
					nl_msg.counter_gen.port_type = PPE_PM_PORT_TYPE_BITMAP;
				} else if (strcasecmp(port_type, "port") == 0) {
					nl_msg.counter_gen.port_type = PPE_PM_PORT_TYPE_PORT;
				} else if (strcasecmp(port_type, "gem_port") == 0) {
					nl_msg.counter_gen.port_type = PPE_PM_PORT_TYPE_GEMPORT;
				} else {
					ppecfg_log_error("Invalid port_type: %s. Valid: 'bitmap', 'port', 'gem_port'\n", port_type);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_PORT_TYPE;
				field_mask |= 0x04;
				break;
			}

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_PORT_INFO:
				if (!(field_mask & PORT_TYPE_MASK)) {
					ppecfg_log_error("port_type must be provided before port_info\n");
					error = -EINVAL;
					goto done;
				}

				switch (nl_msg.counter_gen.port_type) {
				case PPE_PM_PORT_TYPE_BITMAP:
					error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.counter_gen.port.port_bitmap);
					break;
				case PPE_PM_PORT_TYPE_PORT:
					error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.counter_gen.port.dev_name), nl_msg.counter_gen.port.dev_name);
					break;
				case PPE_PM_PORT_TYPE_GEMPORT:
					error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.counter_gen.port.gem_port);
					break;
				default:
					ppecfg_log_error("Unhandled port_type %d\n", nl_msg.counter_gen.port_type);
					error = -EINVAL;
					break;
				}

				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_PORT_INFO;

				field_mask |= 0x08;
				break;

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_TAG_FORMAT:
			{
				char tag_format[12];

				error = ppecfg_param_get_str(sub_params->data, sizeof(tag_format), tag_format);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(tag_format, "untag") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_UNTAGGED;
				} else if (strcasecmp(tag_format, "pri") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_PRIORITY;
				} else if (strcasecmp(tag_format, "tag") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_TAGGED;
				} else if (strcasecmp(tag_format, "pri_untag") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_PRI_UNTAG;
				} else if (strcasecmp(tag_format, "tag_untag") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_TAG_UNTAG;
				} else if (strcasecmp(tag_format, "pri_tag") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_PRI_TAG;
				} else if (strcasecmp(tag_format, "all") == 0) {
					nl_msg.counter_gen.tag_format = PPE_PM_TAG_FORMAT_ALL;
				} else {
					ppecfg_log_error("Invalid tag_format: %s. Valid: 'untag', 'pri', 'tag', 'pri_untag', 'tag_untag', 'pri_tag', 'all'\n", tag_format);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_TAG_FORMAT;
				field_mask |= 0x10;
				break;
			}

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_VID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.counter_gen.vid);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (nl_msg.counter_gen.vid > 4095) {
					ppecfg_log_error("Invalid vid=%u. Valid range is 0-4095\n", nl_msg.counter_gen.vid);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_VID;

				field_mask |= 0x20;
				break;

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_PCP:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.counter_gen.pcp);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (nl_msg.counter_gen.pcp > 7) {
					ppecfg_log_error("Invalid pcp=%u. Valid range is 0-7\n", nl_msg.counter_gen.pcp);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_PCP;

				field_mask |= 0x40;
				break;

			case PPECFG_PM_COUNTER_GEN_RULE_ADD_IPMC:
			{
				char ipmc[16];

				error = ppecfg_param_get_str(sub_params->data, sizeof(ipmc), ipmc);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(ipmc, "non_ipmc") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_NON_IPMC;
				} else if (strcasecmp(ipmc, "ipv4_mc") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_IPV4_MC;
				} else if (strcasecmp(ipmc, "ipv6_mc") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_IPV6_MC;
				} else if (strcasecmp(ipmc, "nonip_ipv4_mc") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_NONIP_IPV4_MC;
				} else if (strcasecmp(ipmc, "nonip_ipv6_mc") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_NONIP_IPV6_MC;
				} else if (strcasecmp(ipmc, "ipv4_ipv6_mc") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_IPV4_IPV6_MC;
				} else if (strcasecmp(ipmc, "all") == 0) {
					nl_msg.counter_gen.ipmc = PPE_PM_IPMC_TYPE_ALL;
				} else {
					ppecfg_log_error("Invalid ipmc: %s. Valid: 'non_ipmc', 'ipv4_mc', 'ipv6_mc', 'nonip_ipv4_mc', 'nonip_ipv6_mc', 'ipv4_ipv6_mc', 'all'\n", ipmc);
					error = -EINVAL;
					goto done;
				}

				nl_msg.counter_gen.rule_flags |= PPE_PM_GEN_RULE_FLAG_IPMC;
				field_mask |= 0x80;
				break;
			}

		}
	}

	if (!(field_mask & FLOW_DIR_MASK) || !(field_mask & COUNTER_ID_MASK) || !(field_mask & OPTIONAL_FIELDS_MASK) || ((field_mask & PORT_TYPE_MASK) && !(field_mask & PORT_INFO_MASK))) {
		ppecfg_log_warn("Invalid configuration: counter_id and flow_dir are mandatory,\n"
				"At least one optional field (port_type, port_info, tag_format, vid, pcp, ipmc) is required,\n"
				"port_type/port_info must be provided together\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_pm_counter_gen_rule_add(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;

}

/*
 * ppecfg_pm_counter_gen_rule_del()
 *	Handle PM counter gen rule delete
 */
static int ppecfg_pm_counter_gen_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_pm_info nl_msg = {{0}};
	int error;

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

	nss_ppenl_pm_counter_init(&nl_msg, NSS_PPE_PM_COUNTER_GEN_DESTROY_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_PM_COUNTER_GEN_RULE_DEL_COUNTER_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.counter_gen.counter_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_pm_counter_gen_rule_del(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}
