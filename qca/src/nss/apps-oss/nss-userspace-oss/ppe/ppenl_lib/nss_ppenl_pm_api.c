/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_pm_api.h>
#include "nss_ppenl_pm.h"

static struct nss_ppenl_pm_ctx nss_pm_ctx;
static void nss_ppenl_pm_resp(void *user_ctx, struct nss_ppenl_pm_info *pm_info, void *resp_ctx) __attribute__((unused));

/*
 * ppecfg_pm_resp()
 *	ppecfg log based on response from netlink
 */
static void nss_ppenl_pm_resp(void *user_ctx, struct nss_ppenl_pm_info *pm_info, void *resp_ctx)
{
	ppe_pm_ret_t ret = 0;

	if (!pm_info) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&pm_info->cm);

	switch (cmd) {
		case NSS_PPE_PM_COUNTER_GET_MSG:
			ret = pm_info->counter_info.ret;
			if (ret != PPE_PM_RET_SUCCESS) {
				nss_ppenl_sock_log_error("Get PM Counter stats failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("PM Counter stats fetched successfully\n"
					"Counter ID: %u\n"
					"Unicast Packets: %llu\n"
					"Broadcast Packets: %llu\n"
					"Multicast Packets: %llu\n"
					"Oversized Packets: %llu\n"
					"Total Bytes: %llu\n"
					"Frames of size 64 bytes: %llu\n"
					"Frames of size 65-127 bytes: %llu\n"
					"Frames of size 128-255 bytes: %llu\n"
					"Frames of size 256-511 bytes: %llu\n"
					"Frames of size 512-1023 bytes: %llu\n"
					"Frames of size 1024-1518 bytes: %llu\n",
					pm_info->counter_info.counter_id, (unsigned long long)pm_info->counter_info.ucast_packet,
					(unsigned long long)pm_info->counter_info.bcast_packet, (unsigned long long)pm_info->counter_info.mcast_packet,
					(unsigned long long)pm_info->counter_info.oversize, (unsigned long long)pm_info->counter_info.octets,
					(unsigned long long)pm_info->counter_info.frame_64, (unsigned long long)pm_info->counter_info.frame_65_127,
					(unsigned long long)pm_info->counter_info.frame_128_255, (unsigned long long)pm_info->counter_info.frame_256_511,
					(unsigned long long)pm_info->counter_info.frame_512_1023, (unsigned long long)pm_info->counter_info.frame_1024_1518);
			break;

		case NSS_PPE_PM_COUNTER_GEN_CREATE_RULE_MSG:
			ret = pm_info->counter_gen.ret;
			if (ret != PPE_PM_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PM counter generation rule create failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("PM counter generation rule create successful for counter_id=%d\n", pm_info->counter_gen.counter_id);
			break;

		case NSS_PPE_PM_COUNTER_GEN_DESTROY_RULE_MSG:
			ret = pm_info->counter_gen.ret;
			if (ret != PPE_PM_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PM counter generation rule destroy failed with error: %d\n", ret);
				return;
			}

			nss_ppenl_sock_log_info("PM counter generation rule destroy successful for counter_id=%d\n", pm_info->counter_gen.counter_id);
			break;

		default:
			nss_ppenl_sock_log_error("unsupported message cmd type(%d)\n", cmd);
	}
}

/*
 * nss_ppenl_pm_sock_cb()
 *	NSS NL PM callback
 */
int nss_ppenl_pm_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_pm_ctx *ctx = (struct nss_ppenl_pm_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_pm_info *pm_info = nss_ppenl_sock_get_data(msg);

	if (!pm_info) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL PM header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&pm_info->cm);
	switch (cmd) {
		case NSS_PPE_PM_COUNTER_GET_MSG:
		case NSS_PPE_PM_COUNTER_GEN_CREATE_RULE_MSG:
		case NSS_PPE_PM_COUNTER_GEN_DESTROY_RULE_MSG:
			{
				void *cb_data = nss_ppenl_cmn_get_cb_data(&pm_info->cm, sock->family_id);
				if (!cb_data) {
					return NL_SKIP;
				}

				/*
				 * Note: The callback user can modify the CB content so it
				 * needs to locally save the response data for further use
				 * after the callback is completed
				 */
				struct nss_ppenl_pm_resp resp;
				memcpy(&resp, cb_data, sizeof(struct nss_ppenl_pm_resp));

				/*
				 * clear the ownership of the CB so that callback user can
				 * use it if needed
				 */
				nss_ppenl_cmn_clr_cb_owner(&pm_info->cm);

				if (!resp.cb) {
					nss_ppenl_sock_log_info("%d:no PM response callback for cmd(%d)\n", pid, cmd);
					return NL_SKIP;
				}

				resp.cb(sock->user_ctx, pm_info, resp.data);

				return NL_OK;
			}

		default:
			nss_ppenl_sock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
			return NL_SKIP;
	}
}

/*
 * nss_ppenl_pm_sock_open()
 *	this opens the NSS PM NL socket for usage
 */
int nss_ppenl_pm_sock_open(struct nss_ppenl_pm_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;
	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_PM_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_pm_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS PM socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_pm_sock_close()
 *	close the NSS PM NL socket
 */
void nss_ppenl_pm_sock_close(struct nss_ppenl_pm_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_pm_sock_send()
 *	register callback and send the PM message synchronously through the socket
 */
int nss_ppenl_pm_sock_send(struct nss_ppenl_pm_ctx *ctx, struct nss_ppenl_pm_info *pm_info, nss_ppenl_pm_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_pm_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!pm_info) {
		nss_ppenl_sock_log_error("%d:invalid NSS PM info\n", pid);
		return -EINVAL;
	}

	if (cb) {
		nss_ppenl_cmn_set_cb_owner(&pm_info->cm, family_id);

		resp = nss_ppenl_cmn_get_cb_data(&pm_info->cm, family_id);
		assert(resp);

		resp->data = NULL;
		resp->cb = cb;
		has_resp = true;
	}

	error = nss_ppenl_sock_send(&ctx->sock, &pm_info->cm, pm_info, has_resp);
	if (error) {
		nss_ppenl_sock_log_error("%d:failed to send NSS PM info, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_pm_counter_get()
 *	Get the PM Counter Stats
 */
int nss_ppenl_pm_counter_get(struct nss_ppenl_pm_info *pm_info) {
	int error;

	/*
	 * open the NSS NL PM socket
	 */
	error = nss_ppenl_pm_sock_open(&nss_pm_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PM socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_pm_sock_send(&nss_pm_ctx, pm_info, nss_ppenl_pm_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_pm_sock_close(&nss_pm_ctx);
	return error;
}

/*
 * nss_ppenl_pm_counter_gen_rule_add()
 *	Rule addition for PM counter generation table.
 */
int nss_ppenl_pm_counter_gen_rule_add(struct nss_ppenl_pm_info *pm_info) {
	int error;

	/*
	 * open the NSS NL PM socket
	 */
	error = nss_ppenl_pm_sock_open(&nss_pm_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PM socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_pm_sock_send(&nss_pm_ctx, pm_info, nss_ppenl_pm_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_pm_sock_close(&nss_pm_ctx);
	return error;
}

/*
 * nss_ppenl_pm_counter_gen_rule_del
 *	Rule deletion for PM counter generation table.
 */
int nss_ppenl_pm_counter_gen_rule_del(struct nss_ppenl_pm_info *pm_info) {
	int error;

	/*
	 * open the NSS NL PM socket
	 */
	error = nss_ppenl_pm_sock_open(&nss_pm_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PM socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_pm_sock_send(&nss_pm_ctx, pm_info, nss_ppenl_pm_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_pm_sock_close(&nss_pm_ctx);
	return error;

}

/*
 * nss_ppenl_pm_counter_init()
 *	Init the pm message
 */
void nss_ppenl_pm_counter_init(struct nss_ppenl_pm_info *pm_info, enum nss_ppe_pm_message_types type)
{

	if (type >= NSS_PPE_PM_MAX_MSG_TYPES) {
		nss_ppenl_sock_log_error("Incorrect message type\n");
		return;
	}

	nss_ppenl_rule_pm_init(pm_info, type);
}
