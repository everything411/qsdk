/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @file nss_ppenl_exception_if.h
 * 	NSS PPE Netlink Exception
 */
#ifndef __NSS_PPENL_EXCEPTION_IF_H
#define __NSS_PPENL_EXCEPTION_IF_H

#include <ppe_drv_cc_usr.h>

/**
 * Exception Configure Family
 */
#define NSS_PPENL_EXCEPTION_FAMILY "nss_ppenl_exp"

/**
 * @brief Exception rule
 */
struct nss_ppenl_exception_rule {
	struct nss_ppenl_cmn cm;				/**< common message header */
	struct ppe_drv_cc_usr_exception_info info;			/**< Exception information */
};

/*
 * @brief Message types
 */
enum nss_ppenl_exception_message_types {
	NSS_PPENL_EXCEPTION_CONFIG_EXCEPTION_MSG,		/**< Exception rule config message */
	NSS_PPENL_EXCEPTION_MAX_MSG_TYPES,		/**< Maximum message type */
};

/**
 * @brief NETLINK EXCEPTION message init
 *
 * @param rule[IN] NSS NETLINK EXCEPTION rule
 * @param type[IN] EXCEPTION message type
 */
static inline void nss_ppenl_exception_rule_init(struct nss_ppenl_exception_rule *rule, enum nss_ppenl_exception_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_exception_rule), type);
}

#endif /* __NSS_PPENL_EXCEPTION_IF_H */
