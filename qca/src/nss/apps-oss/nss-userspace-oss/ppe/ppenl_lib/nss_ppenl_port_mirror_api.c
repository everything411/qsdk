/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include <nss_ppenl_port_mirror_if.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_port_mirror_api.h>
#include "nss_ppenl_port_mirror.h"

static struct nss_ppenl_port_mirror_ctx nss_port_mirror_ctx;

/*
 * nss_ppenl_port_mirror_resp()
 *	Log the response received from the kernel.
 */
static void nss_ppenl_port_mirror_resp(void *user_ctx,
				       struct nss_ppenl_port_mirror_rule *rule,
				       void *resp_ctx)
{
	if (!rule) {
		return;
	}

	if (rule->cfg.ret) {
		nss_ppenl_sock_log_error("PORT_MIRROR cfg failed with error: %d\n",
					 rule->cfg.ret);
		return;
	}

	nss_ppenl_sock_log_info("PORT_MIRROR cfg successful\n");
}

/*
 * nss_ppenl_port_mirror_sock_cb()
 *	NSS NL port-mirror receive callback.
 */
int nss_ppenl_port_mirror_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();
	struct nss_ppenl_port_mirror_ctx *ctx =
		(struct nss_ppenl_port_mirror_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;
	struct nss_ppenl_port_mirror_rule *rule =
		nss_ppenl_sock_get_data(msg);

	if (!rule) {
		nss_ppenl_sock_log_error("%d: failed to get port-mirror header\n",
					 pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);
	switch (cmd) {
	case NSS_PPE_PORT_MIRROR_CFG_MSG: {
		void *cb_data = nss_ppenl_cmn_get_cb_data(&rule->cm,
							   sock->family_id);
		if (!cb_data) {
			return NL_SKIP;
		}

		struct nss_ppenl_port_mirror_resp resp;
		memcpy(&resp, cb_data, sizeof(resp));
		nss_ppenl_cmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info(
				"%d: no port-mirror response callback\n", pid);
			return NL_SKIP;
		}

		resp.cb(sock->user_ctx, rule, resp.data);
		return NL_OK;
	}

	default:
		nss_ppenl_sock_log_error(
			"%d: unsupported port-mirror cmd type(%d)\n", pid, cmd);
		return NL_SKIP;
	}
}

/*
 * nss_ppenl_port_mirror_sock_open()
 *	Open the port-mirror NL socket.
 */
int nss_ppenl_port_mirror_sock_open(struct nss_ppenl_port_mirror_ctx *ctx,
				    void *user_ctx)
{
	pid_t pid = getpid();
	int error;

	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));
	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_PORT_MIRROR_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_port_mirror_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error(
			"%d: unable to open port-mirror socket, error(%d)\n",
			pid, error);
		memset(ctx, 0, sizeof(*ctx));
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_port_mirror_sock_close()
 *	Close the port-mirror NL socket.
 */
void nss_ppenl_port_mirror_sock_close(struct nss_ppenl_port_mirror_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_port_mirror_sock_send()
 *	Send a port-mirror message synchronously.
 */
int nss_ppenl_port_mirror_sock_send(struct nss_ppenl_port_mirror_ctx *ctx,
				    struct nss_ppenl_port_mirror_rule *rule,
				    nss_ppenl_port_mirror_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_port_mirror_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("%d: invalid port-mirror rule\n", pid);
		return -ENOMEM;
	}

	if (cb) {
		nss_ppenl_cmn_set_cb_owner(&rule->cm, family_id);
		resp = nss_ppenl_cmn_get_cb_data(&rule->cm, family_id);
		assert(resp);
		resp->data = NULL;
		resp->cb = cb;
		has_resp = true;
	}

	error = nss_ppenl_sock_send(&ctx->sock, &rule->cm, rule, has_resp);
	if (error) {
		nss_ppenl_sock_log_error(
			"%d: failed to send port-mirror rule, error(%d)\n",
			pid, error);
	}

	return error;
}

/*
 * nss_ppenl_port_mirror_cfg()
 *	Enable or disable egress mirroring on a named port.
 */
int nss_ppenl_port_mirror_cfg(struct nss_ppenl_port_mirror_rule *rule)
{
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("Invalid port-mirror rule\n");
		return -EINVAL;
	}

	error = nss_ppenl_port_mirror_sock_open(&nss_port_mirror_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error(
			"Failed to open port-mirror socket; error(%d)\n", error);
		return error;
	}

	error = nss_ppenl_port_mirror_sock_send(&nss_port_mirror_ctx, rule,
						nss_ppenl_port_mirror_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send port-mirror message\n");
	}

	nss_ppenl_port_mirror_sock_close(&nss_port_mirror_ctx);
	return error;
}
