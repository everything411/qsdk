/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_GEMPORT_H__
#define __NSS_PPENL_GEMPORT_H__

/** @addtogroup chapter_nlgemport
 This chapter describes GEM PORT APIs in the user space.
 These APIs are wrapper functions for GEMPORT family specific operations.
*/

/**
 * Response callback for GEMPORT.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule GEM_PORT rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_gem_port_resp_cb_t)(void *user_ctx, struct nss_ppenl_gem_port_rule *rule, void *resp_ctx);

/**
 * Event callback for GEM_PORT.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule GEM_PORT rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_gem_port_event_cb_t)(void *user_ctx, struct nss_ppenl_gem_port_rule *rule);

/**
 * NSS NL GEM PORT response.
 */
struct nss_ppenl_gem_port_resp {
	void *data;		/**< Response context. */
	nss_ppenl_gem_port_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL GEM PORT context.
 */
struct nss_ppenl_gem_port_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_gem_port_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_gem_port_datatypes */
/** @addtogroup nss_ppenl_gem_port_functions @{ */

/**
 * Opens NSS NL GEM PORT socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_gem_port_sock_open(struct nss_ppenl_gem_port_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL GEM PORT socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_gem_port_sock_close(struct nss_ppenl_gem_port_ctx *ctx);

/**
 * Sends a GEM PORT rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL GEM PORT context.
 * @param[in] rule GEM PORT rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_gem_port_sock_send(struct nss_ppenl_gem_port_ctx *ctx, struct nss_ppenl_gem_port_rule *rule, nss_ppenl_gem_port_resp_cb_t cb);


/** @} *//* end_addtogroup nss_ppenl_gem_port_functions */

#endif /* __NSS_PPENL_GEMPORT_H__ */
