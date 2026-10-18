/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_dscp_api.h>
#include "nss_ppenl_dscp.h"

static struct nss_ppenl_dscp_ctx nss_dscp_ctx;
static void nss_ppenl_dscp_resp(void *user_ctx, struct nss_ppenl_dscp_rule *rule, void *resp_ctx) __attribute__((unused));

/*
 * ppecfg_dscp_resp()
 *	ppecfg log based on response from netlink
 */
static void nss_ppenl_dscp_resp(void *user_ctx, struct nss_ppenl_dscp_rule *dscp_rule, void *resp_ctx)
{
	ppe_dscp_ret_t ret = 0;

	if (!dscp_rule) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&dscp_rule->cm);

	switch (cmd) {
		case NSS_PPE_DSCP_CONFIG_RULE_MSG:
			ret = dscp_rule->rule.ret;
			if (ret != PPE_DSCP_RET_SUCCESS) {
				nss_ppenl_sock_log_error("DSCP_P_Bit rule create failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("DSCP_P_BIT rule create successful for dscp=%d\n", dscp_rule->rule.dscp_val);
			break;

		default:
			nss_ppenl_sock_log_error("unsupported message cmd type(%d)\n", cmd);
	}
}

/*
 * nss_ppenl_dscp_sock_cb()
 *	NSS NL DSCP callback
 */
int nss_ppenl_dscp_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_dscp_ctx *ctx = (struct nss_ppenl_dscp_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_dscp_rule *rule = nss_ppenl_sock_get_data(msg);

	if (!rule) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL DSCP header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);

	switch (cmd) {
		case NSS_PPE_DSCP_CONFIG_RULE_MSG:
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
				struct nss_ppenl_dscp_resp resp;
				memcpy(&resp, cb_data, sizeof(struct nss_ppenl_dscp_resp));

				/*
				 * clear the ownership of the CB so that callback user can
				 * use it if needed
				 */
				nss_ppenl_cmn_clr_cb_owner(&rule->cm);

				if (!resp.cb) {
					nss_ppenl_sock_log_info("%d:no DSCP response callback for cmd(%d)\n", pid, cmd);
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
 * nss_ppenl_dscp_sock_open()
 *	this opens the NSS DSCP NL socket for usage
 */
int nss_ppenl_dscp_sock_open(struct nss_ppenl_dscp_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;
	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_DSCP_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_dscp_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS DSCP socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_dscp_sock_close()
 *	close the NSS DSCP NL socket
 */
void nss_ppenl_dscp_sock_close(struct nss_ppenl_dscp_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_dscp_sock_send()
 *	register callback and send the DSCP message synchronously through the socket
 */
int nss_ppenl_dscp_sock_send(struct nss_ppenl_dscp_ctx *ctx, struct nss_ppenl_dscp_rule *rule, nss_ppenl_dscp_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_dscp_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("%d:invalid NSS DSCP rule\n", pid);
		return -EINVAL;
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
		nss_ppenl_sock_log_error("%d:failed to send NSS DSCP rule, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_dscp_rule_config()
 * 	Configure DSCP rule in PPE
 */
int nss_ppenl_dscp_rule_config(struct nss_ppenl_dscp_rule *rule)
{
	int error;

	/*
	 * open the NSS NL DSCP socket
	 */
	error = nss_ppenl_dscp_sock_open(&nss_dscp_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open DSCP socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_dscp_sock_send(&nss_dscp_ctx, rule, nss_ppenl_dscp_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_dscp_sock_close(&nss_dscp_ctx);
	return error;
}

/*
 * nss_ppenl_dscp_init_rule()
 *	Init the rule message
 */
void nss_ppenl_dscp_init_rule(struct nss_ppenl_dscp_rule *rule, enum nss_ppe_dscp_message_types type)
{

	if (type >= NSS_PPE_DSCP_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect rule type\n");
		return;
	}

	nss_ppenl_dscp_rule_init(rule, type);
}
