/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_DOT1P_H__
#define __NSS_PPENL_DOT1P_H__

/** @addtogroup chapter_nldot1p
 This chapter describes DOT1P APIs in the user space.
 These APIs are wrapper functions for DOT1P family specific operations.
*/

/**
 * Response callback for DOT1P.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule DOT1P rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_dot1p_resp_cb_t)(void *user_ctx, struct nss_ppenl_dot1p_rule *rule, void *resp_ctx);

/**
 * Event callback for DOT1P.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule DOT1P rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_dot1p_event_cb_t)(void *user_ctx, struct nss_ppenl_dot1p_rule *rule);

/**
 * NSS NL DOT1P response.
 */
struct nss_ppenl_dot1p_resp {
	void *data;			/**< Response context. */
	nss_ppenl_dot1p_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL DOT1P context.
 */
struct nss_ppenl_dot1p_ctx {
	struct nss_ppenl_sock_ctx sock;		/**< NSS socket context. */
	nss_ppenl_dot1p_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_dot1p_datatypes */

/** @addtogroup nss_ppenl_dot1p_functions @{ */

/**
 * Opens NSS NL DOT1P socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_dot1p_sock_open(struct nss_ppenl_dot1p_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL DOT1P socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_dot1p_sock_close(struct nss_ppenl_dot1p_ctx *ctx);

/**
 * Sends a DOT1P rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL DOT1P context.
 * @param[in] rule DOT1P rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_dot1p_sock_send(struct nss_ppenl_dot1p_ctx *ctx, struct nss_ppenl_dot1p_rule *rule, nss_ppenl_dot1p_resp_cb_t cb);

/** @} *//* end_addtogroup nss_ppenl_dot1p_datatypes */

#endif /* __NSS_PPENL_DOT1P_H__ */
