/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <nss_ppenl_policer_api.h>
#include "nss_ppenl_policer.h"

static struct nss_ppenl_policer_ctx nss_policer_ctx;
static void nss_ppenl_policer_resp(void *user_ctx, struct nss_ppenl_policer_rule *rule, void *resp_ctx) __attribute__((unused));
/*
 * ppecfg_policer_resp()
 * 	ppecfg log based on response from netlink
 */
static void nss_ppenl_policer_resp(void *user_ctx, struct nss_ppenl_policer_rule *policer_rule, void *resp_ctx)
{
	int ret = 0;

	if (!policer_rule) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&policer_rule->cm);

	switch (cmd) {
		case NSS_PPE_POLICER_CREATE_RULE_MSG:
			ret = policer_rule->config.ret;
			if ((ret != PPECFG_POLICER_RET) && (ret != PPECFG_POLICER_RET_NON_PORT)) {
				nss_ppenl_sock_log_error("Policer rule create failed with error: %d\n", ret);
				return;
			}

			if (!policer_rule->config.is_port_policer) {
				nss_ppenl_sock_log_info("Policer rule create successful for rule_id %d\n", policer_rule->config.policer_id);
			} else {
				nss_ppenl_sock_log_info("Policer rule create successful for dev %s\n", policer_rule->config.dev);
			}

			break;
		case NSS_PPE_POLICER_DESTROY_RULE_MSG:
			ret = policer_rule->config.ret;
			if (ret != PPECFG_POLICER_RET) {
				nss_ppenl_sock_log_error("Policer rule delete failed with error:%d\n",ret);
				return;
			}

			if (!policer_rule->config.is_port_policer) {
				nss_ppenl_sock_log_info("Policer rule delete successful for rule_id %d\n", policer_rule->config.policer_id);
			} else {
				nss_ppenl_sock_log_info("Policer rule delete successful for dev %s\n", policer_rule->config.dev);
			}

			break;
		case NSS_PPE_POLICER_FLUSH_RULE_MSG:
			ret = policer_rule->config.ret;
			if (ret != PPECFG_POLICER_RET) {
				nss_ppenl_sock_log_error("Policer rule flush failed with error:%d\n", ret);
				return;
			}

			if (!policer_rule->config.is_port_policer) {
				nss_ppenl_sock_log_info("Policer rule flush successful\n");
			} else {
				nss_ppenl_sock_log_info("Policer rule flush successful for dev\n");
			}

			break;
		default:
			nss_ppenl_sock_log_error("unsupported message cmd type(%d)", cmd);
	}
}

/*
 * nss_ppenl_policer_sock_cb()
 *	NSS NL POLICER callback
 */
int nss_ppenl_policer_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_policer_ctx *ctx = (struct nss_ppenl_policer_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_policer_rule *rule = nss_ppenl_sock_get_data(msg);
	if (!rule) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL POLICER header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);

	switch (cmd) {
	case NSS_PPE_POLICER_CREATE_RULE_MSG:
	case NSS_PPE_POLICER_DESTROY_RULE_MSG:
	case NSS_PPE_POLICER_FLUSH_RULE_MSG:
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
		struct nss_ppenl_policer_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_policer_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_ppenl_cmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no POLICER response callback for cmd(%d)\n", pid, cmd);
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
 * nss_ppenl_policer_sock_open()
 *	this opens the NSS POLICER NL socket for usage
 */
int nss_ppenl_policer_sock_open(struct nss_ppenl_policer_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;

	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_POLICER_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_policer_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS POLICER socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_policer_sock_close()
 *	close the NSS POLICER NL socket
 */
void nss_ppenl_policer_sock_close(struct nss_ppenl_policer_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_policer_sock_send()
 *	register callback and send the POLICER message synchronously through the socket
 */
int nss_ppenl_policer_sock_send(struct nss_ppenl_policer_ctx *ctx, struct nss_ppenl_policer_rule *rule, nss_ppenl_policer_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_policer_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("%d:invalid NSS POLICER rule\n", pid);
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
		nss_ppenl_sock_log_error("%d:failed to send NSS POLICER rule, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_policer_rule_del
 *  Delete Policer rule in PPE
 */
int nss_ppenl_policer_rule_del(struct nss_ppenl_policer_rule *rule) {

	int error;
	/*
	 * open the NSS NL POLICER socket
	 */
	error = nss_ppenl_policer_sock_open(&nss_policer_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open POLICER socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_policer_sock_send(&nss_policer_ctx, rule, nss_ppenl_policer_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_policer_sock_close(&nss_policer_ctx);
	return error;
}

/*
 * nss_ppenl_policer_rule_add()
 * Add Policer rule in PPE
 */
int nss_ppenl_policer_rule_add(struct nss_ppenl_policer_rule *rule) {

	int error;

	/*
	 * open the NSS NL POLICER socket
	 */
	error = nss_ppenl_policer_sock_open(&nss_policer_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open POLICER socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_policer_sock_send(&nss_policer_ctx, rule, nss_ppenl_policer_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_policer_sock_close(&nss_policer_ctx);
	return error;
}

/*
 * nss_ppenl_policer_rule_flush
 *	Flush Policer rules in PPE
 */
int nss_ppenl_policer_rule_flush(struct nss_ppenl_policer_rule *rule)
{
	int error;

	/*
	 * open the NSS NL POLICER socket
	 */
	error = nss_ppenl_policer_sock_open(&nss_policer_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open POLICER socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_policer_sock_send(&nss_policer_ctx, rule, nss_ppenl_policer_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message; error(%d)\n", error);
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_policer_sock_close(&nss_policer_ctx);
	return error;
}

/*
 * nss_ppenl_policer_init_rule()
 *	init the rule message
 */
void nss_ppenl_policer_init_rule(struct nss_ppenl_policer_rule *rule, enum nss_ppe_policer_message_types type)
{
	if (type >= NSS_PPE_POLICER_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect rule type\n");
		return;
	}

	nss_ppenl_policer_rule_init(rule, type);
}
