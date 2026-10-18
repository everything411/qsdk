/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_MCAST_H__
#define __NSS_PPENL_MCAST_H__

/** @addtogroup chapter_nl_mcast
 This chapter describes multicast APIs in the user space.
 These APIs are wrapper functions for multicast family specific operations.
*/

/*
 * Response callback for multicast
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req multicast request.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_mcast_resp_cb_t)(void *user_ctx, struct nss_ppenl_mcast_req *req, void *resp_ctx);

/**
 * Event callback for multicast.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req multicast request.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_mcast_event_cb_t)(void *user_ctx, struct nss_ppenl_mcast_req *req);

/**
 * NSS NL multicast response.
 */
struct nss_ppenl_mcast_resp {
	void *data;		/**< Response context. */
	nss_ppenl_mcast_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL multicast context.
 */
struct nss_ppenl_mcast_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_mcast_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_mcast_datatypes */
/** @addtogroup nss_ppenl_mcast_functions @{ */

/**
 * Opens NSS NL multicast socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_mcast_sock_open(struct nss_ppenl_mcast_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL multicast socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_mcast_sock_close(struct nss_ppenl_mcast_ctx *ctx);

/**
 * Sends an multicast req synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL multicast context.
 * @param[in] req multicast request.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_mcast_sock_send(struct nss_ppenl_mcast_ctx *ctx, struct nss_ppenl_mcast_req *req, nss_ppenl_mcast_resp_cb_t cb);


/** @} *//* end_addtogroup nss_ppenl_mcast_functions */

#endif /* __NSS_PPENL_MCAST_H__ */
