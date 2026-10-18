/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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