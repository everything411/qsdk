/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_edma_ddrq_if.h>
#include "nss_ppenl_edma_ddrq.h"

static struct nss_ppenl_edma_ddrq_ctx nss_edma_ctx;
static void nss_ppenl_edma_ddrq_resp(void *user_ctx, struct nss_ppenl_edma_ddrq_rule *rule, void *resp_ctx) __attribute__((unused));

/*
 * nss_ppenl_edma_ddrq_resp()
 *	EDMA DDRQ response from netlink
 */
static void nss_ppenl_edma_ddrq_resp(void *user_ctx, struct nss_ppenl_edma_ddrq_rule *edma_rule, void *resp_ctx)
{
	int ret = 0;

	if (!edma_rule) {
		nss_ppenl_sock_log_error("EDMA rule is NULL");
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&edma_rule->cm);

	switch (cmd) {
	case NSS_PPE_EDMA_DDRQ_CFG_RULE_MSG:
		ret = edma_rule->msg.ddrq_cfg.ret;
		if (ret != DDRQ_RET_SUCCESS) {
			nss_ppenl_sock_log_error("EDMA ddrq config failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("EDMA DDRQ configuration set successful\n");
		break;

	case NSS_PPE_EDMA_DDRQ_GRP_CFG_RULE_MSG:
		ret = edma_rule->msg.ddrq_grp_cfg.ret;
		if (ret != DDRQ_RET_SUCCESS) {
			nss_ppenl_sock_log_error("EDMA ddrq grp config failed with error: %d\n", ret);
			return;
		}

		nss_ppenl_sock_log_info("EDMA DDRQ group configuration set successful\n");
		break;

	default:
		nss_ppenl_sock_log_error("unsupported message cmd type(%d)", cmd);
	}
}

/*
 * nss_ppenl_edma_ddrq_sock_cb()
 *	NSS netlink EDMA DDRQ callback
 */
int nss_ppenl_edma_ddrq_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_edma_ddrq_ctx *ctx = (struct nss_ppenl_edma_ddrq_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;
	struct nss_ppenl_edma_ddrq_rule *rule = nss_ppenl_sock_get_data(msg);

	if (!rule) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL EDMA DDRQ rule\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&rule->cm);
	switch (cmd) {
	case NSS_PPE_EDMA_DDRQ_CFG_RULE_MSG:
	case NSS_PPE_EDMA_DDRQ_GRP_CFG_RULE_MSG:
		void *cb_data = nss_ppenl_cmn_get_cb_data(&rule->cm, sock->family_id);
		if (!cb_data) {
			nss_ppenl_sock_log_error("%d: failed to get cb data\n", pid);
			return NL_SKIP;
		}

		struct nss_ppenl_edma_ddrq_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_edma_ddrq_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_ppenl_cmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no EDMA DDRQ response callback for cmd(%d)\n", pid, cmd);
			return NL_SKIP;
		}

		resp.cb(sock->user_ctx, rule, resp.data);
		return NL_OK;

	default:
		nss_ppenl_sock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
		return NL_SKIP;
	}
}

/*
 * nss_ppenl_edma_ddrq_sock_open()
 *	API to open the NSS EDMA netlink socket
 */
int nss_ppenl_edma_ddrq_sock_open(struct nss_ppenl_edma_ddrq_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;

	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));
	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_EDMA_DDRQ_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_edma_ddrq_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS EDMA DDRQ socket, error(%d)\n", pid, error);
		goto fail;
	}

	nss_ppenl_sock_log_info("EDMA DDRQ sock open success\n");
	return 0;
fail:
	 memset(ctx, 0, sizeof(*ctx));
	 return error;
}

/*
 * nss_ppenl_edma_ddrq_sock_close()
 *	API to close the NSS EDMA DDRQ netlink socket
 */
void nss_ppenl_edma_ddrq_sock_close(struct nss_ppenl_edma_ddrq_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_edma_ddrq_sock_send()
 *	API to send the NSS EDMA DDRQ netlink socket message
 */
int nss_ppenl_edma_ddrq_sock_send(struct nss_ppenl_edma_ddrq_ctx *ctx, struct nss_ppenl_edma_ddrq_rule *rule, nss_ppenl_edma_ddrq_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_edma_ddrq_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	 if (!rule) {
		 nss_ppenl_sock_log_error("%d:invalid NSS EDMA DDRQ rule\n", pid);
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
		 nss_ppenl_sock_log_error("%d:failed to send NSS EDMA DDRQ rule, error(%d)\n", pid, error);
		 return error;
	 }

	 nss_ppenl_sock_log_info("EDMA DDRQ sock send success\n");
	 return 0;
}

/*
 * nss_ppenl_edma_ddrq_send_rule()
 *	API to send the NSS EDMA rule
 */
int nss_ppenl_edma_ddrq_send_rule(struct nss_ppenl_edma_ddrq_rule *rule)
{
	int error;

	/*
	 * open the NSS NL EDMA DDRQ socket
	 */
	error = nss_ppenl_edma_ddrq_sock_open(&nss_edma_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open EDMA DDRQ socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_edma_ddrq_sock_send(&nss_edma_ctx, rule, nss_ppenl_edma_ddrq_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

	nss_ppenl_sock_log_info("EDMA DDRQ send rule success\n");
done:
	/*
	 * close the socket
	 */
	nss_ppenl_edma_ddrq_sock_close(&nss_edma_ctx);
	return error;
}

/*
 * nss_ppenl_edma_ddrq_init_rule()
 *	API to init EDMA rule
 */
void nss_ppenl_edma_ddrq_init_rule(struct nss_ppenl_edma_ddrq_rule *rule, enum nss_ppe_edma_ddrq_message_types type)
{
	if (type >= NSS_PPE_EDMA_DDRQ_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect rule type\n");
		return;
	}

	nss_ppenl_edma_ddrq_rule_init(rule, type);
}
