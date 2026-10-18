/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_PORT_MIRROR_H__
#define __NSS_PPENL_PORT_MIRROR_H__

#include <nss_ppenl_port_mirror_if.h>

/**
 * Response callback for port-mirror.
 *
 * @param[in] user_ctx   User context (provided at socket open).
 * @param[in] rule       Port-mirror rule info.
 * @param[in] resp_ctx   User data per callback.
 */
typedef void (*nss_ppenl_port_mirror_resp_cb_t)(void *user_ctx,
						struct nss_ppenl_port_mirror_rule *rule,
						void *resp_ctx);

/**
 * NSS NL port-mirror response.
 */
struct nss_ppenl_port_mirror_resp {
	void *data;				/**< Response context. */
	nss_ppenl_port_mirror_resp_cb_t cb;	/**< Response callback. */
};

/**
 * NSS NL port-mirror context.
 */
struct nss_ppenl_port_mirror_ctx {
	struct nss_ppenl_sock_ctx sock;		/**< NSS socket context. */
};

int nss_ppenl_port_mirror_sock_open(struct nss_ppenl_port_mirror_ctx *ctx, void *user_ctx);
void nss_ppenl_port_mirror_sock_close(struct nss_ppenl_port_mirror_ctx *ctx);
int nss_ppenl_port_mirror_sock_send(struct nss_ppenl_port_mirror_ctx *ctx,
				    struct nss_ppenl_port_mirror_rule *rule,
				    nss_ppenl_port_mirror_resp_cb_t cb);

#endif /* __NSS_PPENL_PORT_MIRROR_H__ */
