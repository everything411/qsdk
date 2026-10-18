/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_PM_H__
#define __NSS_PPENL_PM_H__

/** @addtogroup chapter_nlPM
  This chapter describes PM APIs in the user space.
  These APIs are wrapper functions for PM family specific operations.
 */

/**
 * Response callback for PM.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] pm_info PM Info.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_pm_resp_cb_t)(void *user_ctx, struct nss_ppenl_pm_info *pm_info, void *resp_ctx);

/**
 * Event callback for PM.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] pm_info PM Info.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_pm_event_cb_t)(void *user_ctx, struct nss_ppenl_pm_info *pm_info);

/**
 * NSS NL PM response.
 */
struct nss_ppenl_pm_resp {
	void *data;             /**< Response context. */
	nss_ppenl_pm_resp_cb_t cb;    /**< Response callback. */
};

/**
 * NSS NL PM context.
 */
struct nss_ppenl_pm_ctx {
	struct nss_ppenl_sock_ctx sock; 	/**< NSS socket context. */
	nss_ppenl_pm_event_cb_t event;        /**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_pm_datatypes */
/** @addtogroup nss_ppenl_pm_functions @{ */

/**
 * Opens NSS NL PM socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_pm_sock_open(struct nss_ppenl_pm_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL PM socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_pm_sock_close(struct nss_ppenl_pm_ctx *ctx);

/**
 * Sends an PM rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL PM context.
 * @param[in] pm_info PM Counter Info.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_pm_sock_send(struct nss_ppenl_pm_ctx *ctx, struct nss_ppenl_pm_info *pm_info, nss_ppenl_pm_resp_cb_t cb);

/** @} *//* end_addtogroup nss_ppenl_pm_functions */

#endif /* __NSS_PPENL_PM_H__ */
