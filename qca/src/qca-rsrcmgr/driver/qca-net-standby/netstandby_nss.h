/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __NETSTANDBY_NSS_H
#define __NETSTANDBY_NSS_H

#include <ppe_acl.h>

/*
 * netstandby_status
 *	Network standby operation status
 */
enum netstandby_status {
	NETSTANDBY_SUCCESS = 0,
	NETSTANDBY_CB_REGISTER_FAIL = 1,
	NETSTANDBY_DESIGNATED_IFACE_INVALID = 2,
	NETSTANDBY_ACL_REGISTER_FAIL = 3,
	NETSTANDBY_MEM_ALLOC_FAIL = 4,
};

struct netstandby_nss_data {
	uint32_t flags;
	ppe_acl_rule_id_t acl_id;
};

/*
 * netstandby_acl_process_buf()
 *	Process acl index notification from ppe driver
 */
bool netstandby_acl_process_buf(void *app_data, void *skb);

/*
 * netstandby_acl_unregister()
 *	Unregister ACL callback with PPE driver
 */
void netstandby_acl_unregister(ppe_acl_rule_id_t acl_id);

/*
 * netstandby_acl_register()
 *	register ACL callback with PPE driver
 */
enum netstandby_status netstandby_acl_register(ppe_acl_rule_id_t acl_id);

/*
 * netstandby_acl_rule_destroy()
 *	Destroy default ACL rule
 */
enum netstandby_status netstandby_acl_rule_destroy(ppe_acl_rule_id_t acl_id);

/*
 * netstandby_acl_rule_create()
 *	Create default ACL rule
 */
enum netstandby_status netstandby_acl_rule_create(struct netstandby_trigger_rule *trigger_rule, struct net_device *dev, int rule_idx, bool default_acl);
#endif
