/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NSS_PPENL_ACL_H__
#define __NSS_PPENL_ACL_H__

/** @addtogroup chapter_nlACL
 This chapter describes ACL APIs in the user space.
 These APIs are wrapper functions for ACL family specific operations.
*/

/**
 * Response callback for ACL.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule ACL rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_acl_resp_cb_t)(void *user_ctx, struct nss_ppenl_acl_rule *rule, void *resp_ctx);

/**
 * Event callback for ACL.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule ACL rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_acl_event_cb_t)(void *user_ctx, struct nss_ppenl_acl_rule *rule);

/**
 * NSS NL ACL response.
 */
struct nss_ppenl_acl_resp {
	void *data;		/**< Response context. */
	nss_ppenl_acl_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL ACL context.
 */
struct nss_ppenl_acl_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_acl_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_acl_datatypes */
/** @addtogroup nss_ppenl_acl_functions @{ */

/**
 * Opens NSS NL ACL socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_acl_sock_open(struct nss_ppenl_acl_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL ACL socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_acl_sock_close(struct nss_ppenl_acl_ctx *ctx);

/**
 * Sends an ACL rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL ACL context.
 * @param[in] rule ACL rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_acl_sock_send(struct nss_ppenl_acl_ctx *ctx, struct nss_ppenl_acl_rule *rule, nss_ppenl_acl_resp_cb_t cb);


/** @} *//* end_addtogroup nss_ppenl_acl_functions */

#endif /* __NSS_PPENL_ACL_H__ */
