/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_TUN_RPS_JSON_PARSER_H
#define __PPECFG_TUN_RPS_JSON_PARSER_H

#include "ppecfg_tun_rps.h"

/*
 * ppecfg_tun_rps_json_rule_handler()
 *	Unified JSON rule handler - supports both add and delete operations
 */
int ppecfg_tun_rps_json_rule_handler(struct json_object *rule_obj);

#endif /* __PPECFG_TUN_RPS_JSON_PARSER_H */
