/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_DSCP_H__
#define __NSS_PPENL_DSCP_H__

/** @addtogroup chapter_nlDSCP
  This chapter describes DSCP APIs in the user space.
  These APIs are wrapper functions for DSCP family specific operations.
 */

/**
 * Response callback for DSCP.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule DSCP rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_dscp_resp_cb_t)(void *user_ctx, struct nss_ppenl_dscp_rule *rule, void *resp_ctx);

/**
 * Event callback for DSCP.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule DSCP rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_dscp_event_cb_t)(void *user_ctx, struct nss_ppenl_dscp_rule *rule);

/**
 * NSS NL DSCP response.
 */
struct nss_ppenl_dscp_resp {
	void *data;		/**< Response context. */
	nss_ppenl_dscp_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL DSCP context.
 */
struct nss_ppenl_dscp_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_dscp_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_dscp_datatypes */
/** @addtogroup nss_ppenl_dscp_functions @{ */

/**
 * Opens NSS NL DSCP socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_dscp_sock_open(struct nss_ppenl_dscp_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL DSCP socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_dscp_sock_close(struct nss_ppenl_dscp_ctx *ctx);

/**
 * Sends an DSCP rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL DSCP context.
 * @param[in] rule DSCP rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_dscp_sock_send(struct nss_ppenl_dscp_ctx *ctx, struct nss_ppenl_dscp_rule *rule, nss_ppenl_dscp_resp_cb_t cb);

/** @} *//* end_addtogroup nss_ppenl_dscp_functions */

#endif /* __NSS_PPENL_DSCP_H__ */
