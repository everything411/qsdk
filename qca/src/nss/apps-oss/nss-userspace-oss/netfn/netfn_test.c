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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include "netfn_rule.h"
#include <linux/in6.h>
#include <arpa/inet.h>

int main(int argc, char** argv)
{
	struct netfn_rule_sk *nrs;
	const char *family = NULL;
	json_t *root, *sub_root;
	json_error_t json_err;
	int error = -EINVAL;

	/*
	 * This holds the reference count for the root
	 */
	root = json_load_file(argv[1], 0, &json_err);
	if(!root) {
		netfn_log_error("Error parsing json file(%s): %s\n", argv[1], json_err.text);
		return false;
	}

	sub_root = json_object_get(root, "FAMILY");
	if (!sub_root) {
		netfn_log_error("Family is not present(%s)\n", argv[1]);
		return -EINVAL;
	}

	if (!json_is_string(sub_root)) {
		netfn_log_error("Family name is not present in file(%s)\n", argv[1]);
		return -EINVAL;
	}

	family = json_string_value(sub_root);
	netfn_log_info("Initializing family(%s)\n", family);

	nrs = netfn_rule_init(family);
	if (!nrs) {
		netfn_log_error("Family(%s) is not enabled\n", family);
		return -EINVAL;
	}

	error = netfn_rule_config(nrs, root, family);
	if (error < 0) {
		netfn_log_error("Family(%s) rule config failed(%d)\n", family, error);
		netfn_rule_deinit(nrs);
		return -EINVAL;
	}

	json_decref(root);
	netfn_rule_deinit(nrs);
	return 0;
}
