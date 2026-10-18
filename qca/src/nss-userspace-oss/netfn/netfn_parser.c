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

#include <ctype.h>
#include <string.h>
#include "netfn_rule.h"
#include "netfn_helper.h"
#include "netfn_parser.h"

/*
 * netfn_parse_json()
 * 	Recursively parse json file and fill netlink msg.
 */
bool netfn_parse_json(json_t *root, struct nl_msg *msg)
{
	const char *key;
	json_t *value;

	if (!json_is_object(root)) {
		netfn_log_error("\nInvalid JSON format\n");
		return false;
	}

	json_object_foreach(root, key, value) {
		/*
		 * We found a include key
		 */
		if (!strcmp(key, "included_files")) {
			const char *file_name;
			json_error_t error;
			json_t *sub_root;

			if (!json_is_string(value)) {
				netfn_log_error("\nInvalid include value\n");
				return false;
			}

			/*
			 * TODO: add support for multiple filename support
			 */
			file_name = json_string_value(value);
			sub_root = json_load_file(file_name, 0, &error);
			if(!sub_root) {
				netfn_log_error("Error parsing json file %s: %s\n", file_name, error.text);
				return false;
			}

			netfn_parse_json(sub_root, msg);
			continue;
		}

		/*
		 * We found flags type
		 */
		if (json_is_array(value)) {
			struct nlattr *data;
			const char *str_val;
			json_t *element;
			size_t index;

			data = nla_nest_start(msg, netfn_string_to_int(key));

			/*
			 * Parse the values for each flag
			 */
			json_array_foreach(value, index, element) {
				if (!json_is_string(element)) {
					netfn_log_error("\nInvalid value (%s, %ld)\n", key, index);
				}

				str_val = json_string_value(element);
				nla_put_string(msg, netfn_string_to_int(str_val), str_val);
			}

			nla_nest_end(msg, data);
			continue;

		}

		/*
		 * We found a sub scope object type
		 */
		if (json_is_object(value)) {
			struct nlattr *data = NULL;

			data = nla_nest_start(msg, netfn_string_to_int(key) | NLA_F_NESTED);

			/*
			 * Build the nested data from the sub scope
			 */
			netfn_parse_json(value, msg);

			nla_nest_end(msg, data);
			continue;
		}

		/*
		 * We found a string type
		 */
		if (json_is_string(value)) {
			const char *str_val = json_string_value(value);
			nla_put_string(msg, netfn_string_to_int(key), str_val);
			continue;

		}

		/*
		 * We found a integer type
		 */
		if (json_is_integer(value)) {
			uint32_t int_val = json_integer_value(value);
			nla_put_u32(msg, netfn_string_to_int(key), int_val);
			continue;
		}
	}

	return true;
}
