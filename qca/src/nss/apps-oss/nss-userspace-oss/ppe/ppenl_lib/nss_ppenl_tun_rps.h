/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_TUN_RPS_H__
#define __NSS_PPENL_TUN_RPS_H__

#include "nss_ppenl_sock.h"

/** @addtogroup chapter_nlTUN_RPS
 This chapter describes Tunnel RPS APIs in the user space.
 These APIs are wrapper functions for Tunnel RPS family specific operations.
*/

/**
 * Response callback for Tunnel RPS.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule Tunnel RPS rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_tun_rps_resp_cb_t)(void *user_ctx, struct nss_ppenl_tun_rps_rule *rule, void *resp_ctx);

/**
 * Event callback for Tunnel RPS.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule Tunnel RPS rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_tun_rps_event_cb_t)(void *user_ctx, struct nss_ppenl_tun_rps_rule *rule);

/**
 * NSS NL Tunnel RPS response.
 */
struct nss_ppenl_tun_rps_resp {
	void *data;				/**< Response context. */
	nss_ppenl_tun_rps_resp_cb_t cb;		/**< Response callback. */
};

/**
 * NSS NL Tunnel RPS context.
 */
struct nss_ppenl_tun_rps_ctx {
	struct nss_ppenl_sock_ctx sock;		/**< NSS socket context. */
	nss_ppenl_tun_rps_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_tun_rps_datatypes */
/** @addtogroup nss_ppenl_tun_rps_functions @{ */

/**
 * Opens NSS NL Tunnel RPS socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_tun_rps_sock_open(struct nss_ppenl_tun_rps_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL Tunnel RPS socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_tun_rps_sock_close(struct nss_ppenl_tun_rps_ctx *ctx);

/**
 * Sends a Tunnel RPS rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL Tunnel RPS context.
 * @param[in] rule Tunnel RPS rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_tun_rps_sock_send(struct nss_ppenl_tun_rps_ctx *ctx, struct nss_ppenl_tun_rps_rule *rule, nss_ppenl_tun_rps_resp_cb_t cb);

/** @} *//* end_addtogroup nss_ppenl_tun_rps_functions */

#endif /* __NSS_PPENL_TUN_RPS_H__ */
