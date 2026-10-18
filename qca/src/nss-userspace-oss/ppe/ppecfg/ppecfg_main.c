/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "ppecfg_hlos.h"
#include "ppecfg_param.h"
#include "ppecfg_family.h"
#include "ppecfg_json_parser.h"

/*
 * Checking the next parameter to know if it is for CLI or Json parser
 * if next parameter is family, the user is giving rule by CLI
 * if next parameter is config_type=json, the user is giving rule by json parser
 */
static struct ppecfg_param cfg_param[] = {
	PPECFG_PARAMLIST_INIT("family=acl", ppecfg_acl_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("family=policer", ppecfg_policer_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("family=qos", ppecfg_qos_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("config_type=json", ppecfg_json_parser_param, ppecfg_json_parser_handler),
};

static struct ppecfg_param root = PPECFG_PARAMLIST_INIT("ppecfg", cfg_param, NULL);

/*
 * main()
 */
int main(int argc, char *argv[])
{
	struct ppecfg_param_in match = {0};
	int error;

	match.total = argc;
	match.args = (char **)argv;

	if (argc < 2) {
		ppecfg_log_arg_error((struct ppecfg_param *)&root);
		match.cur_param = &root;
		error = -EINVAL;
		goto help;
	}

	error = ppecfg_param_iter_tbl(&root, &match);
	if (error < 0) {
		goto help;
	}

	return 0;
help:
	ppecfg_param_help(match.cur_param);

	return error;
}
