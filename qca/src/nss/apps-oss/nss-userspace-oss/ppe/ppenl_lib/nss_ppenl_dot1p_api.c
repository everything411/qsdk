/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_dot1p_api.h>
#include "nss_ppenl_dot1p.h"

static struct nss_ppenl_dot1p_ctx nss_dot1p_ctx;
static void nss_ppenl_dot1p_resp(void *user_ctx, struct nss_ppenl_dot1p_rule *rule, void *resp_ctx) __attribute__((unused));

/*
 * nss_ppenl_dot1p_resp()
 *	log based on response from netlink
 */
static void nss_ppenl_dot1p_resp(void *user_ctx, struct nss_ppenl_dot1p_rule *dot1p_rule, void *resp_ctx)
{
	ppe_dot1p_ret_t ret = 0;

	if (!dot1p_rule) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&dot1p_rule->cm);

	switch (cmd) {
	case NSS_PPE_DOT1P_CREATE_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule create failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule create successful for rule_id %d\n", dot1p_rule->rule.rule_id);
		break;

	case NSS_PPE_DOT1P_DELETE_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule delete failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule delete successful for rule_id %d\n", dot1p_rule->rule.rule_id);
		break;

	case NSS_PPE_DOT1P_FLUSH_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule flush failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule flush successful\n");
		break;

	case NSS_PPE_DOT1P_CREATE_DEF_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule create default rule failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule create default succesful\n");
		break;

	case NSS_PPE_DOT1P_RESUME_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule resume failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule Resume successful\n");
		break;

	case NSS_PPE_DOT1P_PAUSE_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule pause failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule pause successful\n");
		break;

	case NSS_PPE_DOT1P_GET_STATE_RULE_MSG:
		ret = dot1p_rule->rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P rule get state failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P rule-state: 0::Resume 1::pause current rule state: %d\n", dot1p_rule->rule.rule_state);
		break;

	case NSS_PPE_DOT1P_POLICER_CREATE_RULE_MSG:
		ret = dot1p_rule->policer_rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P policer rule create failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P policer rule create successful for gemport  %d\n", dot1p_rule->policer_rule.gemport_id);
		break;

	case NSS_PPE_DOT1P_POLICER_DELETE_RULE_MSG:
		ret = dot1p_rule->policer_rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P policer rule delete failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P policer rule delete successful for gemport %d\n", dot1p_rule->policer_rule.gemport_id);
		break;

	case NSS_PPE_DOT1P_POLICER_FLUSH_RULE_MSG:
		ret = dot1p_rule->policer_rule.ret;
		if (ret != PPE_DOT1P_RET_SUCCESS) {
			nss_ppenl_sock_log_error("DOT1P policer rule flush failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("DOT1P policer rule flush successful\n");
		break;
	default:
		nss_ppenl_sock_log_error("unsupported message cmd type(%d)\n", cmd);
	}
}

/*
 * nss_ppenl_dot1p_sock_cb()
 *	NSS NL DOT1P callback
 */
int nss_ppenl_dot1p_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_dot1p_ctx *ctx = (struct nss_ppenl_dot1p_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_dot1p_rule *rule = nss_ppenl_sock_get_data(msg);
	if (!rule) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL DOT1P header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);

	switch (cmd) {
	case NSS_PPE_DOT1P_CREATE_RULE_MSG:
	case NSS_PPE_DOT1P_DELETE_RULE_MSG:
	case NSS_PPE_DOT1P_FLUSH_RULE_MSG:
	case NSS_PPE_DOT1P_CREATE_DEF_RULE_MSG:
	case NSS_PPE_DOT1P_RESUME_RULE_MSG:
	case NSS_PPE_DOT1P_PAUSE_RULE_MSG:
	case NSS_PPE_DOT1P_GET_STATE_RULE_MSG:
	case NSS_PPE_DOT1P_POLICER_CREATE_RULE_MSG:
	case NSS_PPE_DOT1P_POLICER_DELETE_RULE_MSG:
	case NSS_PPE_DOT1P_POLICER_FLUSH_RULE_MSG:
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
		struct nss_ppenl_dot1p_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_dot1p_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_ppenl_cmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no DOT1P response callback for cmd(%d)\n", pid, cmd);
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
 * nss_ppenl_dot1p_sock_open()
 *	this opens the NSS DOT1P NL socket for usage
 */
int nss_ppenl_dot1p_sock_open(struct nss_ppenl_dot1p_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;
	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_DOT1P_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_dot1p_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS DOT1P socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_dot1p_sock_close()
 *	close the NSS DOT1P NL socket
 */
void nss_ppenl_dot1p_sock_close(struct nss_ppenl_dot1p_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_dot1p_sock_send()
 *	register callback and send the DOT1P message synchronously through the socket
 */
int nss_ppenl_dot1p_sock_send(struct nss_ppenl_dot1p_ctx *ctx, struct nss_ppenl_dot1p_rule *rule, nss_ppenl_dot1p_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_dot1p_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!rule) {
		nss_ppenl_sock_log_error("%d:invalid NSS DOT1P rule\n", pid);
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
		nss_ppenl_sock_log_error("%d:failed to send NSS dot1p rule, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_dot1p_rule_send()
 * 	send rules in PPE
 */
int nss_ppenl_dot1p_rule_send(struct nss_ppenl_dot1p_rule *rule)
{
	int error;

	/*
	 * open the NSS NL DOT1P socket
	 */
	error = nss_ppenl_dot1p_sock_open(&nss_dot1p_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open DOT1P socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_dot1p_sock_send(&nss_dot1p_ctx, rule, nss_ppenl_dot1p_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message; error(%d)\n", error);
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_dot1p_sock_close(&nss_dot1p_ctx);
	return error;
}

/*
 * nss_ppenl_dot1p_init_rule()
 *	Init the rule message
 */
void nss_ppenl_dot1p_init_rule(struct nss_ppenl_dot1p_rule *rule, enum nss_ppe_dot1p_message_types type)
{
	if (type >= NSS_PPE_DOT1P_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect rule type\n");
		return;
	}

	nss_ppenl_dot1p_rule_init(rule, type);
}
