/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include "nss_ppenl_tun_rps.h"

static struct nss_ppenl_tun_rps_ctx nss_tun_rps_ctx;
static void nss_ppenl_tun_rps_resp(void *user_ctx, struct nss_ppenl_tun_rps_rule *rule, void *resp_ctx) __attribute__((unused));

/*
 * nss_ppenl_tun_rps_resp()
 *	ppecfg log based on response from netlink
 */
static void nss_ppenl_tun_rps_resp(void *user_ctx, struct nss_ppenl_tun_rps_rule *tun_rps_rule, void *resp_ctx)
{
	int ret = 0;

	if (!tun_rps_rule) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&tun_rps_rule->cm);

	switch (cmd) {
	case NSS_PPE_TUN_RPS_CREATE_RULE_MSG:
		ret = tun_rps_rule->rule.ret;
		if (ret != 0) {
			nss_ppenl_sock_log_error("Tunnel RPS rule create failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("Tunnel RPS rule create successful\n");
		break;

	case NSS_PPE_TUN_RPS_DESTROY_RULE_MSG:
		ret = tun_rps_rule->rule.ret;
		if (ret != 0) {
			nss_ppenl_sock_log_error("Tunnel RPS rule delete failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("Tunnel RPS rule destroy successful\n");
		break;

	default:
		nss_ppenl_sock_log_error("unsupported message cmd type(%d)\n", cmd);
	}
}

/*
 * nss_ppenl_tun_rps_sock_cb()
 *	NSS NL Tunnel RPS callback
 */
int nss_ppenl_tun_rps_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_tun_rps_ctx *ctx = (struct nss_ppenl_tun_rps_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_tun_rps_rule *rule = nss_ppenl_sock_get_data(msg);
	if (!rule) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL Tunnel RPS header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);

	switch (cmd) {
	case NSS_PPE_TUN_RPS_CREATE_RULE_MSG:
	case NSS_PPE_TUN_RPS_DESTROY_RULE_MSG:
	{
		void *cb_data = nss_ppenl_cmn_get_cb_data(&rule->cm, sock->family_id);
		if (!cb_data) {
			return NL_SKIP;
		}

		/*
		 * Note: The callback user can modify the CB content so it
		 * needs to locally save the response data for further use
		 * after the callback is completed
		 */
		struct nss_ppenl_tun_rps_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_tun_rps_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_ppenl_cmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no Tunnel RPS response callback for cmd(%d)\n", pid, cmd);
			return NL_SKIP;
		}

		resp.cb(sock->user_ctx, rule, resp.data);

		return NL_OK;
	}

	default:
		nss_ppenl_sock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
		return NL_SKIP;
	}
}

/*
 * nss_ppenl_tun_rps_sock_open()
 *	this opens the NSS Tunnel RPS NL socket for usage
 */
int nss_ppenl_tun_rps_sock_open(struct nss_ppenl_tun_rps_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;
	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_TUN_RPS_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_tun_rps_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS Tunnel RPS socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_tun_rps_sock_close()
 *	close the NSS Tunnel RPS NL socket
 */
void nss_ppenl_tun_rps_sock_close(struct nss_ppenl_tun_rps_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_tun_rps_sock_send()
 *	register callback and send the Tunnel RPS message synchronously through the socket
 */
int nss_ppenl_tun_rps_sock_send(struct nss_ppenl_tun_rps_ctx *ctx, struct nss_ppenl_tun_rps_rule *rule, nss_ppenl_tun_rps_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_tun_rps_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("%d:invalid NSS Tunnel RPS rule\n", pid);
		return -ENOMEM;
	}

	if (cb) {
		nss_ppenl_cmn_set_cb_owner(&rule->cm, family_id);

		resp = nss_ppenl_cmn_get_cb_data(&rule->cm, family_id);
		if (!resp) {
			nss_ppenl_sock_log_error("%d:failed to get callback data\n", pid);
			return -EINVAL;
		}

		resp->data = NULL;
		resp->cb = cb;
		has_resp = true;
	}

	error = nss_ppenl_sock_send(&ctx->sock, &rule->cm, rule, has_resp);
	if (error) {
		nss_ppenl_sock_log_error("%d:failed to send NSS Tunnel RPS rule, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_tun_rps_rule_del
 * 	Delete Tunnel RPS rule in PPE
 */
int nss_ppenl_tun_rps_rule_del(struct nss_ppenl_tun_rps_rule *rule) {
	int error;

	/*
	 * open the NSS NL Tunnel RPS socket
	 */
	error = nss_ppenl_tun_rps_sock_open(&nss_tun_rps_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open Tunnel RPS socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_tun_rps_sock_send(&nss_tun_rps_ctx, rule, nss_ppenl_tun_rps_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_tun_rps_sock_close(&nss_tun_rps_ctx);
	return error;

}

/*
 * nss_ppenl_tun_rps_rule_add()
 * 	Add rule in PPE
 */
int nss_ppenl_tun_rps_rule_add(struct nss_ppenl_tun_rps_rule *rule) {
	int error;

	/*
	 * open the NSS NL Tunnel RPS socket
	 */
	error = nss_ppenl_tun_rps_sock_open(&nss_tun_rps_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open Tunnel RPS socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_tun_rps_sock_send(&nss_tun_rps_ctx, rule, nss_ppenl_tun_rps_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
	}

	/*
	 * close the socket
	 */
	nss_ppenl_tun_rps_sock_close(&nss_tun_rps_ctx);
	return error;
}

/*
 * nss_ppenl_tun_rps_init_rule()
 *	Init the rule message
 */
void nss_ppenl_tun_rps_init_rule(struct nss_ppenl_tun_rps_rule *rule, enum nss_ppe_tun_rps_message_types type)
{
	if (type >= NSS_PPE_TUN_RPS_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect rule type\n");
		return;
	}

	nss_ppenl_tun_rps_rule_init(rule, type);
}
