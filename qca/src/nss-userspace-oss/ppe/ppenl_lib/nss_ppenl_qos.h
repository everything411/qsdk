/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_QOS_H__
#define __NSS_PPENL_QOS_H__

/** @addtogroup chapter_nlQos
 This chapter describes Qos APIs in the user space.
 These APIs are wrapper functions for QOS family specific operations.
*/

/*
 * Response callback for Qos
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req Qos request.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_qos_resp_cb_t)(void *user_ctx, struct nss_ppenl_qos_req *req, void *resp_ctx);

/**
 * Event callback for QOS.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req QOS request.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_qos_event_cb_t)(void *user_ctx, struct nss_ppenl_qos_req *req);

/**
 * NSS NL QOS response.
 */
struct nss_ppenl_qos_resp {
	void *data;		/**< Response context. */
	nss_ppenl_qos_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL QOS context.
 */
struct nss_ppenl_qos_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_qos_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_qos_datatypes */
/** @addtogroup nss_ppenl_qos_functions @{ */

/**
 * Opens NSS NL Qos socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_qos_sock_open(struct nss_ppenl_qos_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL Qos socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_qos_sock_close(struct nss_ppenl_qos_ctx *ctx);

/**
 * Sends an Qos req synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL Qos context.
 * @param[in] req Qos request.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_qos_sock_send(struct nss_ppenl_qos_ctx *ctx, struct nss_ppenl_qos_req *req, nss_ppenl_qos_resp_cb_t cb);


/** @} *//* end_addtogroup nss_ppenl_qos_functions */

#endif /* __NSS_PPENL_QOS_H__ */
