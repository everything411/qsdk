/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_mcast_api.h>
#include "nss_ppenl_mcast.h"

static struct nss_ppenl_mcast_ctx nss_mcast_ctx;
static void nss_ppenl_mcast_resp(void *user_ctx, struct nss_ppenl_mcast_req *req, void *resp_ctx) __attribute__((unused));

/*
 * nss_ppenl_mcast_resp()
 * 	log based on response from netlink
 */
static void nss_ppenl_mcast_resp(void *user_ctx, struct nss_ppenl_mcast_req *mcast_req, void *resp_ctx)
{
	int ret = 0;

	if (!mcast_req) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&mcast_req->cm);

	switch (cmd) {
	case NSS_PPE_MCAST_CREATE_ENTRY:
			ret = mcast_req->ret;
			if (ret != PPE_MCAST_SUCCESS) {
				nss_ppenl_sock_log_error("multicast entry creation failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("multicast entry create successful\n");
			break;

	case NSS_PPE_MCAST_DELETE_ENTRY:
			ret = mcast_req->ret;
			if (ret != PPE_MCAST_SUCCESS) {
				nss_ppenl_sock_log_error("multicast entry deletion failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("multicast entry deletion successful\n");
			break;

		default:
			nss_ppenl_sock_log_error("unsupported message cmd type(%d)", cmd);
	}
}

/*
 * nss_ppenl_mcast_sock_cb()
 *	NSS NL multicast callback
 */
int nss_ppenl_mcast_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_mcast_ctx *ctx = (struct nss_ppenl_mcast_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;
	struct nss_ppenl_mcast_req *req = nss_ppenl_sock_get_data(msg);

	if (!req) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL multicast header\n", pid);
		return NL_SKIP;
	}
	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&req->cm);

	switch (cmd) {
	case NSS_PPE_MCAST_CREATE_ENTRY:
	case NSS_PPE_MCAST_DELETE_ENTRY:
	{
		void *cb_data = nss_ppenl_cmn_get_cb_data(&req->cm, sock->family_id);

		if (!cb_data) {
			return NL_SKIP;
		}

		/*
		 * Note: The callback user can modify the CB content so it
		 * needs to locally save the response data for further use
		 * after the callback is completed
		 */
		struct nss_ppenl_mcast_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_ppenl_mcast_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_ppenl_cmn_clr_cb_owner(&req->cm);

		if (!resp.cb) {
			nss_ppenl_sock_log_info("%d:no multicast response callback for cmd(%d)\n", pid, cmd);
			return NL_SKIP;
		}

		resp.cb(sock->user_ctx, req, resp.data);
		return NL_OK;
	}

	default:
		nss_ppenl_sock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
		return NL_SKIP;
	}
}

/*
 * nss_ppenl_mcast_sock_open()
 *	this opens the NSS multicast NL socket for usage
 */
int nss_ppenl_mcast_sock_open(struct nss_ppenl_mcast_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;

	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));
	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_MCAST_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_mcast_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS multicast socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_mcast_sock_close()
 *	close the NSS multicast NL socket
 */
void nss_ppenl_mcast_sock_close(struct nss_ppenl_mcast_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_mcast_sock_send()
 *	register callback and send the multicast message synchronously through the socket
 */
int nss_ppenl_mcast_sock_send(struct nss_ppenl_mcast_ctx *ctx, struct nss_ppenl_mcast_req *req, nss_ppenl_mcast_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_mcast_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!req) {
		nss_ppenl_sock_log_error("%d:invalid NSS multicast req\n", pid);
		return -ENOMEM;
	}

	if (cb) {
		nss_ppenl_cmn_set_cb_owner(&req->cm, family_id);
		resp = nss_ppenl_cmn_get_cb_data(&req->cm, family_id);
		assert(resp);

		resp->data = NULL;
		resp->cb = cb;
		has_resp = true;
	}

	error = nss_ppenl_sock_send(&ctx->sock, &req->cm, req, has_resp);
	if (error) {
		nss_ppenl_sock_log_error("%d:failed to send NSS multicast req, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_mcast_send_req()
 * Send multicast req to PPE driver
 */
int nss_ppenl_mcast_send_req(struct nss_ppenl_mcast_req *req) {

	int error;

	/*
	 * open the NSS NL multicast socket
	 */
	error = nss_ppenl_mcast_sock_open(&nss_mcast_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open multicast socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_mcast_sock_send(&nss_mcast_ctx, req, nss_ppenl_mcast_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_mcast_sock_close(&nss_mcast_ctx);
	return error;
}

/*
 * nss_ppenl_mcast_init_req()
 *	init the req message
 */
void nss_ppenl_mcast_init_req(struct nss_ppenl_mcast_req *req, enum nss_ppe_mcast_message_types type)
{
	if (type >= NSS_PPE_MCAST_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect req type\n");
		return;
	}

	nss_ppenl_mcast_req_init(req, type);
}
