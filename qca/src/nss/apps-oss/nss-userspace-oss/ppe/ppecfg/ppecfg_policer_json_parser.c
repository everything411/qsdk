/*
 * Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <stdlib.h>

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include "ppecfg_param.h"
#include "ppecfg_policer.h"
#include "ppecfg_policer_json_parser.h"

/*
 * ppecfg_policer_json_rule_add()
 * 	handle policer rule add
 */
int ppecfg_policer_json_rule_add(struct json_object *rule_obj)
{
	struct nss_ppenl_policer_rule nl_msg = {{0}};
	int error = 0;
	const char *val;

	nss_ppenl_policer_init_rule(&nl_msg, NSS_PPE_POLICER_CREATE_RULE_MSG);
	ppecfg_log_info("Policer rule\n");

	/*
	 * setting meter_mode, couple_enable, colour_aware as enable, user should pass 0 to disable
	 */
	nl_msg.config.meter_enable = 1;
	nl_msg.config.couple_enable = 1;
	nl_msg.config.colour_aware = 1;

	/*
	 * extracting PORT POLICER
	 */
	val = ppecfg_json_object_handler(rule_obj, "port_policer");
	if (val) {
		error = ppecfg_param_get_bool(val, &nl_msg.config.is_port_policer);
		if (error) {
			ppecfg_log_info("port_policer");
			goto done;
		}
		ppecfg_log_info("PORT POLICER : %s\n", val);
	} else {
		nl_msg.config.is_port_policer = false;
	}

	/*
	 * extracting COMMITTED RATE - Mandatory
	 */
	val = ppecfg_json_object_handler(rule_obj, "committed_rate");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint32_t), &nl_msg.config.committed_rate);
		if (error) {
			ppecfg_log_info("committed_rate is not given, this field is mandatory!!");
			goto done;
		}
		ppecfg_log_info("COMMITTED RATE : %s\n", val);
	} else {
		goto done;
	}

	/*
	 * extracting COMMITTED_BURST_SIZE - Mandatory
	 */
	val = ppecfg_json_object_handler(rule_obj, "committed_burst_size");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint32_t), &nl_msg.config.committed_burst_size);
		if (error) {
			ppecfg_log_info("committed_burst_size is not given, this field is mandatory!!");
			goto done;
		}
		ppecfg_log_info("COMMITTED_BURST_SIZE : %s\n", val);
	} else {
		goto done;
	}

	/*
	 * extracting DEV
	 */
	val = ppecfg_json_object_handler(rule_obj, "dev");
	if (val) {
		error = ppecfg_param_get_str(val, sizeof(nl_msg.config.dev), &nl_msg.config.dev);
		if (error) {
			ppecfg_log_error("dev");
			goto done;
		}
		ppecfg_log_info("DEV : %s\n", val);
	} else if (nl_msg.config.is_port_policer) {
		ppecfg_log_info("dev is not given, this field is mandatory for port policer!!");
		goto done;
	}

	/*
	 * extracting RULE ID
	 */
	val = ppecfg_json_object_handler(rule_obj, "rule_id");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint32_t), &nl_msg.config.policer_id);
		if (error) {
			ppecfg_log_error("rule_id");
			goto done;
		}
		ppecfg_log_info("RULE ID : %s\n", val);
	}

	/*
	 * extracting METER MODE
	 */
	val = ppecfg_json_object_handler(rule_obj, "meter_mode");
	if (val) {
		error = ppecfg_param_get_bool(val, &nl_msg.config.meter_mode);
		if (error) {
			ppecfg_log_error("meter_mode");
			goto done;
		}
		ppecfg_log_info("METER MODE : %s\n", val);
	}

	/*
	 * extracting METER UNIT
	 */
	val = ppecfg_json_object_handler(rule_obj, "meter_unit");
	if (val) {
		error = ppecfg_param_get_bool(val, &nl_msg.config.meter_unit);
		if (error) {
			ppecfg_log_error("meter_unit");
			goto done;
		}
		ppecfg_log_info("METER UNIT : %s\n", val);
	}

	/*
	 * extracting PEAK_RATE - Mandatory
	 */
	val = ppecfg_json_object_handler(rule_obj, "peak_rate");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint32_t), &nl_msg.config.peak_rate);
		if (error) {
			ppecfg_log_error("peak_rate is not given, this field is mandatory!!");
			goto done;
		}
		ppecfg_log_info("PEAK_RATE : %s\n", val);
	} else {
		nl_msg.config.peak_rate = PPECFG_POLICER_DEFAULT_PEAK_RATE;
	}

	/*
	 * extracting PEAK_BURST_SIZE
	 */
	val = ppecfg_json_object_handler(rule_obj, "peak_burst_size");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint32_t), &nl_msg.config.peak_burst_size);
		if (error) {
			ppecfg_log_error("peak_burst_size");
			goto done;
		}
		ppecfg_log_info("PEAK_BURST_SIZE : %s\n", val);
	} else {
		goto done;
	}

	/*
	 * extracting METER_ENABLE
	 */
	val = ppecfg_json_object_handler(rule_obj, "meter_enable");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.meter_enable);
		if (error) {
			ppecfg_log_error("meter_enable");
			goto done;
		}
		ppecfg_log_info("METER_ENABLE : %s\n", val);
	}

	/*
	 * extracting COUPLE_ENABLE
	 */
	val = ppecfg_json_object_handler(rule_obj, "couple_enable");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.couple_enable);
		if (error) {
			ppecfg_log_error("couple_enable");
			goto done;
		}
		ppecfg_log_info("COUPLE_ENABLE : %s\n", val);
	}

	/*
	 * extracting COLOUR_AWARE
	 */
	val = ppecfg_json_object_handler(rule_obj, "colour_aware_enable");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.colour_aware);
		if (error) {
			ppecfg_log_error("colour_aware_enable");
			goto done;
		}
		ppecfg_log_info("COLOUR_AWARE : %s\n", val);
	}

	/*
	 * extracting METER_FLAG
	 */
	val = ppecfg_json_object_handler(rule_obj, "meter_flag");
	if (val) {
		/*
		 * Parse optional meter_flag from user_config, default 0
		 * Accepts single or multiple flag values separated by underscore
		 *
		 * Valid packet types (5 types total):
		 *   - uc
		 *   - uuc
		 *   - mc
		 *   - umc
		 *   - bc
		 *
		 * Single type examples:
		 *   meter_flag=uc, meter_flag=mc, meter_flag=bc
		 *
		 * Multiple type examples (ALL combinations are supported):
		 *   meter_flag=bc_mc
		 *   meter_flag=uc_mc_bc
		 *   meter_flag=uc_uuc_mc_umc_bc
		 *   ... any combination of the 5 packet types is valid
		 */
		char meter_flag_str[128];
		char *token;
		char *saveptr;
		char temp_str[128];
		uint32_t combined_flags = 0;
		bool flag_found = false;
		uint32_t duplicate_check = 0;

		error = ppecfg_param_get_str(val, sizeof(meter_flag_str), meter_flag_str);
		if (error < 0) {
			ppecfg_log_error("meter_flag");
			goto done;
		}

		/*
		 * Make a copy of the input string for tokenization
		 * since strtok_r modifies the original string
		 */
		strlcpy(temp_str, meter_flag_str, sizeof(temp_str) - 1);
		temp_str[sizeof(temp_str) - 1] = '\0';

		/*
		 * Parse the string and combine multiple flags using underscore as delimiter
		 */
		token = strtok_r(temp_str, "_", &saveptr);
		while (token != NULL) {
			flag_found = false;

			if (strcasecmp(token, "uc") == 0) {
				if (duplicate_check & PPECFG_POLICER_METER_FLAG_UNICAST) {
					ppecfg_log_error("Duplicate meter_flag: 'uc' specified multiple times\n");
					error = -EINVAL;
					goto done;
				}
				combined_flags |= PPECFG_POLICER_METER_FLAG_UNICAST;
				duplicate_check |= PPECFG_POLICER_METER_FLAG_UNICAST;
				flag_found = true;
			} else if (strcasecmp(token, "uuc") == 0) {
				if (duplicate_check & PPECFG_POLICER_METER_FLAG_UNKNOWN_UNICAST) {
					ppecfg_log_error("Duplicate meter_flag: 'uuc' specified multiple times\n");
					error = -EINVAL;
					goto done;
				}
				combined_flags |= PPECFG_POLICER_METER_FLAG_UNKNOWN_UNICAST;
				duplicate_check |= PPECFG_POLICER_METER_FLAG_UNKNOWN_UNICAST;
				flag_found = true;
			} else if (strcasecmp(token, "umc") == 0) {
				if (duplicate_check & PPECFG_POLICER_METER_FLAG_UNKNOWN_MULTICAST) {
					ppecfg_log_error("Duplicate meter_flag: 'umc' specified multiple times\n");
					error = -EINVAL;
					goto done;
				}
				combined_flags |= PPECFG_POLICER_METER_FLAG_UNKNOWN_MULTICAST;
				duplicate_check |= PPECFG_POLICER_METER_FLAG_UNKNOWN_MULTICAST;
				flag_found = true;
			} else if (strcasecmp(token, "mc") == 0) {
				if (duplicate_check & PPECFG_POLICER_METER_FLAG_MULTICAST) {
					ppecfg_log_error("Duplicate meter_flag: 'mc' specified multiple times\n");
					error = -EINVAL;
					goto done;
				}
				combined_flags |= PPECFG_POLICER_METER_FLAG_MULTICAST;
				duplicate_check |= PPECFG_POLICER_METER_FLAG_MULTICAST;
				flag_found = true;
			} else if (strcasecmp(token, "bc") == 0) {
				if (duplicate_check & PPECFG_POLICER_METER_FLAG_BROADCAST) {
					ppecfg_log_error("Duplicate meter_flag: 'bc' specified multiple times\n");
					error = -EINVAL;
					goto done;
				}
				combined_flags |= PPECFG_POLICER_METER_FLAG_BROADCAST;
				duplicate_check |= PPECFG_POLICER_METER_FLAG_BROADCAST;
				flag_found = true;
			}

			if (!flag_found) {
				ppecfg_log_error("Invalid meter_flag token: '%s'\n", token);
				ppecfg_log_error("Valid packet types: 'uc', 'uuc', 'mc', 'umc', 'bc'\n");
				ppecfg_log_error("Combine multiple types using underscore (e.g., 'bc_mc')\n");
				error = -EINVAL;
				goto done;
			}

			token = strtok_r(NULL, "_", &saveptr);
		}

		/*
		 * Check if at least one valid flag was parsed
		 */
		if (combined_flags == 0) {
			ppecfg_log_error("No valid meter_flag found in: '%s'\n", meter_flag_str);
			ppecfg_log_error("Valid packet types: 'uc', 'uuc', 'mc', 'umc', 'bc'\n");
			ppecfg_log_error("Combine multiple types using underscore (e.g., 'bc_mc')\n");
			error = -EINVAL;
			goto done;
		}

		nl_msg.config.meter_flag = combined_flags;
		nl_msg.config.meter_flag_valid = 1;
		ppecfg_log_info("METER_FLAG : %s (0x%x)\n", meter_flag_str, combined_flags);
	}

	/*
	 * extracting YELLOW_DP
	 */
	val = ppecfg_json_object_handler(rule_obj, "yellow_dp");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.action_info.yellow_dp);
		if (error) {
			ppecfg_log_error("yellow_dp");
			goto done;
		}
		ppecfg_log_info("YELLOW_DP : %s\n", val);
	}

	/*
	 * extracting YELLOW_INT_PRI
	 */
	val = ppecfg_json_object_handler(rule_obj, "yellow_int_pri");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.action_info.yellow_int_pri);
		if (error) {
			ppecfg_log_error("yellow_int_pri");
			goto done;
		}
		ppecfg_log_info("YELLOW_INT_PRI : %s\n", val);
	}

	/*
	 * extracting YELLOW_PCP
	 */
	val = ppecfg_json_object_handler(rule_obj, "yellow_pcp");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.action_info.yellow_pcp);
		if (error) {
			ppecfg_log_error("yellow_pcp");
			goto done;
		}
		ppecfg_log_info("YELLOW_PCP : %s\n", val);
	}

	/*
	 * extracting YELLOW_DEI
	 */
	val = ppecfg_json_object_handler(rule_obj, "yellow_dei");
	if (val) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.action_info.yellow_dei);
		if (error) {
			ppecfg_log_error("yellow_dei");
			goto done;
		}
		ppecfg_log_info("YELLOW_DEI : %s\n", val);
	}

	/*
	 * extracting YELLOW_DSCP
	 */
	val = ppecfg_json_object_handler(rule_obj, "yellow_dscp");
	if (val && !nl_msg.config.is_port_policer) {
		error = ppecfg_param_get_int(val, sizeof(uint8_t), &nl_msg.config.action_info.yellow_dscp);
		if (error) {
			ppecfg_log_error("yellow_dscp");
			goto done;
		}
		ppecfg_log_info("YELLOW_DSCP : %s\n", val);
	}

	/*
	 * Checking Min Max values for CIR, EIR, CBS, EBS
	 */
	if (!nl_msg.config.meter_unit) {
		if (nl_msg.config.committed_rate < PPECFG_POLICER_MIN_INFO_RATE_BYTE) {
			ppecfg_log_error("Minimum committed rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_BYTE);
			goto done;
		}

		if (nl_msg.config.committed_rate > PPECFG_POLICER_MAX_INFO_RATE_BYTE) {
			ppecfg_log_error("Maximum committed rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_BYTE);
			goto done;
		}

		if (nl_msg.config.peak_rate < PPECFG_POLICER_MIN_INFO_RATE_BYTE) {
			ppecfg_log_error("Minimum peak rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_BYTE);
			goto done;
		}

		if (nl_msg.config.peak_rate > PPECFG_POLICER_MAX_INFO_RATE_BYTE) {
			ppecfg_log_error("Maximum peak rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_BYTE);
			goto done;
		}

	} else {
		if (nl_msg.config.committed_rate < PPECFG_POLICER_MIN_INFO_RATE_FRAME) {
			ppecfg_log_error("Minimum committed rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_FRAME);
			goto done;
		}

		if (nl_msg.config.committed_rate > PPECFG_POLICER_MAX_INFO_RATE_FRAME) {
			ppecfg_log_error("Maximum committed rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_FRAME);
			goto done;
		}

		if (nl_msg.config.peak_rate < PPECFG_POLICER_MIN_INFO_RATE_FRAME) {
			ppecfg_log_error("Minimum peak rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_FRAME);
			goto done;
		}

		if (nl_msg.config.peak_rate > PPECFG_POLICER_MAX_INFO_RATE_FRAME) {
			ppecfg_log_error("Maximum peak rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_FRAME);
			goto done;
		}
	}

	if (!nl_msg.config.meter_unit) {
		if ((nl_msg.config.peak_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_BYTE) || (nl_msg.config.committed_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_BYTE)) {
			ppecfg_log_error("Maximum burst size : %d\n", PPECFG_POLICER_MAX_BURST_SIZE_BYTE);
			goto done;
		}

	} else {
		if ((nl_msg.config.peak_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_FRAME) || (nl_msg.config.committed_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_FRAME)) {
			ppecfg_log_error("Maximum burst size : %d\n", PPECFG_POLICER_MAX_BURST_SIZE_FRAME);
				goto done;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_policer_rule_add(&nl_msg);
	if (error) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}

	return error;

done:
	ppecfg_log_info("\nError in extracting Policer params\n");
	return error;
}
