/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_cos_map_api.h>
#include "nss_ppenl_cos_map.h"

static struct nss_ppenl_cos_map_ctx nss_cos_map_ctx;
static void nss_ppenl_cos_map_resp(void *user_ctx, struct nss_ppenl_cos_map_config *config, void *resp_ctx) __attribute__((unused));

/*
 * ppecfg_cos_map_resp()
 * 	ppecfg log based on response from netlink
 */
static void nss_ppenl_cos_map_resp(void *user_ctx, struct nss_ppenl_cos_map_config *config, void *resp_ctx)
{
	int ret = 0;

	if (!config) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&config->cm);

	switch (cmd) {
	case NSS_PPE_COS_MAP_CREATE_RULE_MSG:
			ret = config->msg.rule.ret;
			if (ret == PPE_COS_MAP_CREATE_RULE_FAIL) {
				nss_ppenl_sock_log_error("cos map rule create failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("cos map rule create successful for rule id %d\n", config->msg.rule.rule_id);
			break;

	case NSS_PPE_COS_MAP_DESTROY_RULE_MSG:
			ret = config->msg.rule.ret;
			if (ret == PPE_COS_MAP_DESTROY_RULE_FAIL) {
				nss_ppenl_sock_log_error("cos map rule destroy failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("cos map rule destroy successful for rule id %d\n", config->msg.rule.rule_id);
			break;

	case NSS_PPE_COS_MAP_FLUSH_RULE_MSG:
			ret = config->msg.rule.ret;
			if (ret == PPE_COS_MAP_FLUSH_RULE_FAIL) {
				nss_ppenl_sock_log_error("cos map rules flush failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("cos map rules flush successful\n");
			break;

	case NSS_PPE_COS_MAP_PORT_GROUP_SET:
			ret = config->msg.config.ret;
			if (ret == PPE_COS_MAP_PORT_GROUP_SET_FAIL) {
				nss_ppenl_sock_log_error("port group config failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("port group config successful for dev %s\n", config->msg.config.dev);
			break;

	default:
			nss_ppenl_sock_log_error("unsupported message cmd type(%d)", cmd);
	}
}

/*
 * nss_ppenl_cos_map_sock_cb()
 *	NSS NL cos callback
 */
int nss_ppenl_cos_map_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_cos_map_ctx *ctx = (struct nss_ppenl_cos_map_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;
	struct nss_ppenl_cos_map_config *config = nss_ppenl_sock_get_data(msg);

	if (!config) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL cos header\n", pid);
		return NL_SKIP;
	}
	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&config->cm);

	switch (cmd) {
	case NSS_PPE_COS_MAP_CREATE_RULE_MSG:
	case NSS_PPE_COS_MAP_DESTROY_RULE_MSG:
	case NSS_PPE_COS_MAP_FLUSH_RULE_MSG:
	case NSS_PPE_COS_MAP_PORT_GROUP_SET:
	{
		void *cb_data = nss_ppenl_cmn_get_cb_data(&config->cm, sock->family_id);

		if (!cb_data) {
			return NL_SKIP;
		}

		/*
		 * Note: The callback user can modify the CB content so it
		 * needs to locally save the response data for further use
		 * after the callback is completed
		 */
		struct nss_ppenl_cos_map_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_cos_map_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_ppenl_cmn_clr_cb_owner(&config->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no cos map response callback for cmd(%d)\n", pid, cmd);
			return NL_SKIP;
		}

		resp.cb(sock->user_ctx, config, resp.data);
		return NL_OK;
	}

	default:
		nss_ppenl_sock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
		return NL_SKIP;
	}
}

/*
 * nss_ppenl_cos_map_sock_open()
 *	this opens the NSS cos NL socket for usage
 */
int nss_ppenl_cos_map_sock_open(struct nss_ppenl_cos_map_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;

	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));
	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_COS_MAP_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_cos_map_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS cos map socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_cos_map_sock_close()
 *	close the NSS cos NL socket
 */
void nss_ppenl_cos_map_sock_close(struct nss_ppenl_cos_map_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_cos_map_sock_send()
 *	register callback and send the cos message synchronously through the socket
 */
int nss_ppenl_cos_map_sock_send(struct nss_ppenl_cos_map_ctx *ctx, struct nss_ppenl_cos_map_config *config, nss_ppenl_cos_map_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_cos_map_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!config) {
		nss_ppenl_sock_log_error("%d:invalid NSS cos config\n", pid);
		return -ENOMEM;
	}

	if (cb) {
		nss_ppenl_cmn_set_cb_owner(&config->cm, family_id);

		resp = nss_ppenl_cmn_get_cb_data(&config->cm, family_id);
		assert(resp);

		resp->data = NULL;
		resp->cb = cb;
		has_resp = true;
	}

	error = nss_ppenl_sock_send(&ctx->sock, &config->cm, config, has_resp);
	if (error) {
		nss_ppenl_sock_log_error("%d:failed to send NSS cos map config, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_cos_map_send_config()
 * 	Send cos map config to PPE driver
 */
int nss_ppenl_cos_map_send_config(struct nss_ppenl_cos_map_config *config) {

	int error;

	/*
	 * open the NSS NL cos socket
	 */
	error = nss_ppenl_cos_map_sock_open(&nss_cos_map_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open cos map socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_cos_map_sock_send(&nss_cos_map_ctx, config, nss_ppenl_cos_map_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_cos_map_sock_close(&nss_cos_map_ctx);
	return error;
}

/*
 * nss_ppenl_cos_map_init_config()
 *	init the config message
 */
void nss_ppenl_cos_map_init_config(struct nss_ppenl_cos_map_config *config, enum nss_ppe_cos_map_message_types type)
{
	if (type >= NSS_PPE_COS_MAP_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect config type\n");
		return;
	}

	nss_ppenl_cos_map_rule_init(config, type);
}
