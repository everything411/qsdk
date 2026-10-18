/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_COS_MAP_H__
#define __NSS_PPENL_COS_MAP_H__

/** @addtogroup chapter_nl_cos_map
 This chapter describes cos Map APIs in the user space.
 These APIs are wrapper functions for COS family specific operations.
*/

/*
 * Response callback for cos_map
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule cos map rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_cos_map_resp_cb_t)(void *user_ctx, struct nss_ppenl_cos_map_config *config, void *resp_ctx);

/**
 * Event callback for COS MAP.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule COS MAP rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_cos_map_event_cb_t)(void *user_ctx, struct nss_ppenl_cos_map_config *config);

/**
 * NSS NL cos Map response.
 */
struct nss_ppenl_cos_map_resp {
	void *data;		/**< Response context. */
	nss_ppenl_cos_map_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL Cos Map context.
 */
struct nss_ppenl_cos_map_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_cos_map_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_cos_map_datatypes */
/** @addtogroup nss_ppenl_cos_map_functions @{ */

/**
 * Opens NSS NL cos_map socket.
 *
 * @param[in] ctx NSS NL cos map context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_cos_map_sock_open(struct nss_ppenl_cos_map_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL cos_map socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_cos_map_sock_close(struct nss_ppenl_cos_map_ctx *ctx);

/**
 * Sends an cos_map rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL cos_map context.
 * @param[in] config cos_map rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_cos_map_sock_send(struct nss_ppenl_cos_map_ctx *ctx, struct nss_ppenl_cos_map_config *config, nss_ppenl_cos_map_resp_cb_t cb);


/** @} *//* end_addtogroup nss_ppenl_cos_map_functions */

#endif /* __NSS_PPENL_COS_MAP_H__ */
