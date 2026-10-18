/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_VLAN_H__
#define __NSS_PPENL_VLAN_H__

/** @addtogroup chapter_nlVLAN
  This chapter describes VLAN APIs in the user space.
  These APIs are wrapper functions for VLAN family specific operations.
 */

/**
 * Response callback for VLAN.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule VLAN rule.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_vlan_resp_cb_t)(void *user_ctx, struct nss_ppenl_vlan_rule *rule, void *resp_ctx);

/**
 * Event callback for VLAN.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule VLAN rule.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_vlan_event_cb_t)(void *user_ctx, struct nss_ppenl_vlan_rule *rule);

/**
 * NSS NL VLAN response.
 */
struct nss_ppenl_vlan_resp {
	void *data;		/**< Response context. */
	nss_ppenl_vlan_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL VLAN context.
 */
struct nss_ppenl_vlan_ctx {
	struct nss_ppenl_sock_ctx sock;	/**< NSS socket context. */
	nss_ppenl_vlan_event_cb_t event;	/**< NSS event callback function. */
};

/** @} *//* end_addtogroup nss_ppenl_vlan_datatypes */
/** @addtogroup nss_ppenl_vlan_functions @{ */

/**
 * Opens NSS NL VLAN socket.
 *
 * @param[in] ctx NSS NL socket context allocated by the caller.
 * @param[in] user_ctx User context stored per socket.
 *
 * @return
 * Status of the open call.
 */
int nss_ppenl_vlan_sock_open(struct nss_ppenl_vlan_ctx *ctx, void *user_ctx);

/**
 * Closes NSS NL VLAN socket.
 *
 * @param[in] ctx NSS NL context.
 *
 * @return
 * None.
 */
void nss_ppenl_vlan_sock_close(struct nss_ppenl_vlan_ctx *ctx);

/**
 * Sends an VLAN rule synchronously to NSS NETLINK.
 *
 * @param[in] ctx NSS NL VLAN context.
 * @param[in] rule VLAN rule.
 * @param[in] cb Response callback handler.
 *
 * @return
 * Send status:
 * - 0 -- Success.
 * - Negative version error (-ve) -- Failure.
 */
int nss_ppenl_vlan_sock_send(struct nss_ppenl_vlan_ctx *ctx, struct nss_ppenl_vlan_rule *rule, nss_ppenl_vlan_resp_cb_t cb);

/** @} *//* end_addtogroup nss_ppenl_vlan_functions */

#endif /* __NSS_PPENL_VLAN_H__ */

