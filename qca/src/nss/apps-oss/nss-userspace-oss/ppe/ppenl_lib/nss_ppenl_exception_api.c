/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_exception_api.h>
#include "nss_ppenl_exception.h"

static struct nss_ppenl_exception_ctx nss_exception_ctx;
static void nss_ppenl_exception_resp(void *user_ctx, struct nss_ppenl_exception_rule *rule, void *resp_ctx) __attribute__((unused));

/*
 * ppecfg_exception_resp()
 *	ppecfg log based on response from netlink
 */
static void nss_ppenl_exception_resp(void *user_ctx, struct nss_ppenl_exception_rule *exception_rule, void *resp_ctx)
{
	ppe_drv_cc_usr_ret_t ret = 0;

	if (!exception_rule) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&exception_rule->cm);

	switch (cmd) {
	case NSS_PPENL_EXCEPTION_CONFIG_EXCEPTION_MSG:
		ret = exception_rule->info.ret;
		if (ret != PPE_DRV_CC_USR_RET_SUCCESS) {
			nss_ppenl_sock_log_error("Exception rule create failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("Exception rule create successful for code %d\n", exception_rule->info.code);
		break;
	default:
		nss_ppenl_sock_log_error("unsupported message cmd type(%d)\n", cmd);
	}
}

/*
 * nss_ppenl_exception_sock_cb()
 *	NSS NL Exception callback
 */
int nss_ppenl_exception_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_exception_ctx *ctx = (struct nss_ppenl_exception_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_exception_rule *rule = nss_ppenl_sock_get_data(msg);
	if (!rule) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL Exception header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);

	switch (cmd) {
	case NSS_PPENL_EXCEPTION_CONFIG_EXCEPTION_MSG:
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
		struct nss_ppenl_exception_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_exception_resp));

		/*
			* clear the ownership of the CB so that callback user can
			* use it if needed
			*/
		nss_ppenl_cmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no Exception response callback\n", pid);
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
 * nss_ppenl_exception_sock_open()
 *	this opens the NSS Exception NL socket for usage
 */
int nss_ppenl_exception_sock_open(struct nss_ppenl_exception_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;
	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_EXCEPTION_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_exception_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS exception socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_exception_sock_close()
 *	close the NSS exception NL socket
 */
void nss_ppenl_exception_sock_close(struct nss_ppenl_exception_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_exception_sock_send()
 *	register callback and send the exception message synchronously through the socket
 */
int nss_ppenl_exception_sock_send(struct nss_ppenl_exception_ctx *ctx, struct nss_ppenl_exception_rule *rule, nss_ppenl_exception_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_exception_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("%d:invalid NSS exception rule\n", pid);
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
		nss_ppenl_sock_log_error("%d:failed to send NSS exception rule, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_exception_rule_config()
 * 	Add exception rule in PPE
 */
int nss_ppenl_exception_rule_config(struct nss_ppenl_exception_rule *rule) {
	int error;

	/*
	 * open the NSS NL exception socket
	 */
	error = nss_ppenl_exception_sock_open(&nss_exception_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open exception socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_exception_sock_send(&nss_exception_ctx, rule, nss_ppenl_exception_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
	}

	/*
	 * close the socket
	 */
	nss_ppenl_exception_sock_close(&nss_exception_ctx);
	return error;
}

/*
 * nss_ppenl_exception_init_rule()
 *	Init the rule message
 */
void nss_ppenl_exception_init_rule(struct nss_ppenl_exception_rule *rule, enum nss_ppenl_exception_message_types type)
{
	if (type >= NSS_PPENL_EXCEPTION_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect rule type\n");
		return;
	}

	nss_ppenl_exception_rule_init(rule, type);
}
