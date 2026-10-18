/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_PORT_MGMT_H__
#define __NSS_PPENL_PORT_MGMT_H__

/** @addtogroup chapter_nlPORT_MGMT
  This chapter describes PORT_MGMT APIs in the user space.
  These APIs are wrapper functions for PORT_MGMT family specific operations.
 */

/**
 * Response callback for PORT_MGMT.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] port_mgmt_info PORT_MGMT Info.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_port_mgmt_resp_cb_t)(void *user_ctx, struct nss_ppenl_port_mgmt_info *port_mgmt_info, void *resp_ctx);

/**
 * Event callback for PORT_MGMT.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] port_mgmt_info PORT_MGMT Info.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_port_mgmt_event_cb_t)(void *user_ctx, struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * NSS NL PORT_MGMT response.
 */
struct nss_ppenl_port_mgmt_resp {
	void *data;		/**< Response context. */
	nss_ppenl_port_mgmt_resp_cb_t cb;    /**< Response callback. */
};

/**
 * NSS NL PORT_MGMT context.
 */
struct nss_ppenl_port_mgmt_ctx {
	struct nss_ppenl_sock_ctx sock;		/**< NSS socket context. */
	nss_ppenl_port_mgmt_event_cb_t event;	     /**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_port_mgmt_datatypes */
/** @addtogroup nss_ppenl_port_mgmt_functions @{ */

/**
 * Opens NSS NL PORT_MGMT socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_port_mgmt_sock_open(struct nss_ppenl_port_mgmt_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL PORT_MGMT socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_port_mgmt_sock_close(struct nss_ppenl_port_mgmt_ctx *ctx);

/**
 * Sends an PORT_MGMT rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL PORT_MGMT context.
 * @param[in] port_mgmt_info PORT_MGMT Rule Info.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_port_mgmt_sock_send(struct nss_ppenl_port_mgmt_ctx *ctx, struct nss_ppenl_port_mgmt_info *port_mgmt_info, nss_ppenl_port_mgmt_resp_cb_t cb);

/** @} *//* end_addtogroup nss_ppenl_port_mgmt_functions */

#endif /* __NSS_PPENL_PORT_MGMT_H__ */
