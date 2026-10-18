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

/*
 * flow_auto_flowcookie.h
 *	Flow auto flowcookie
 */
#ifndef __NSS_FLOW_AUTO_FLOWCOOKIE_H
#define __NSS_FLOW_AUTO_FLOWCOOKIE_H

#include <netfn_auto.h>
#include <netfn_flow_cookie.h>

/*
 * netfn_auto_flowcookie_parse_tuple()
 * 	flowcookie tuple parse
 */
bool netfn_auto_flowcookie_parse_tuple(struct netfn_tuple *original, struct nlattr *tuple)
{
	return netfn_auto_parse_tuple(original, tuple);
}

/*
 * netfn_auto_flowcookie_get_cmd_index()
 * 	return the index of the flowcookie cmd
 */
int netfn_auto_flowcookie_get_cmd_index(const char *cmd)
{
	const char *flowcookie_cmds[] = {"RULE_ADD", "RULE_DEL", "RULE_INIT"};
	int i;

	for(i = 0; i < sizeof(flowcookie_cmds); i++) {
		if (strcmp(cmd, flowcookie_cmds[i]) == 0) {
			return i;
		}
	}

	return -1;
}
#endif
