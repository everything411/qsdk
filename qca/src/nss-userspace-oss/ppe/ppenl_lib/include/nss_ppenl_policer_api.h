/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NSS_PPENL_POLICER_API_H__
#define __NSS_PPENL_POLICER_API_H__
#define PPECFG_POLICER_RET 0		/* Successful operation return value from ppe */
#define PPECFG_POLICER_RET_NON_PORT 10	/* Return value from ppe for a successful non port policer create */
void nss_ppenl_policer_init_rule(struct nss_ppenl_policer_rule *rule, enum nss_ppe_policer_message_types type);
int nss_ppenl_policer_rule_add(struct nss_ppenl_policer_rule *rule);
int nss_ppenl_policer_rule_del(struct nss_ppenl_policer_rule *rule);
int nss_ppenl_policer_rule_flush(struct nss_ppenl_policer_rule *rule);

#endif /* __NSS_PPENL_POLICER_API_H__ */
