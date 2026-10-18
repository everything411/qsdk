/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <nss_ppenl_base.h>
#include "nss_ppenl_sock.h"
#include <nss_ppenl_port_mgmt_api.h>
#include "nss_ppenl_port_mgmt.h"

static struct nss_ppenl_port_mgmt_ctx nss_port_mgmt_ctx;
static void nss_ppenl_port_mgmt_resp(void *user_ctx, struct nss_ppenl_port_mgmt_info *port_mgmt_info, void *resp_ctx) __attribute__((unused));

/*
 * ppecfg_port_mgmt_resp()
 *	ppecfg log based on response from netlink
 */
static void nss_ppenl_port_mgmt_resp(void *user_ctx, struct nss_ppenl_port_mgmt_info *port_mgmt_info, void *resp_ctx)
{
	ppe_port_mgmt_ret_t ret = 0;

	if (!port_mgmt_info) {
		return;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&port_mgmt_info->cm);

	switch (cmd) {
		case NSS_PPE_PORT_MGMT_PORT_ISOL_SET_MSG:
			ret = port_mgmt_info->isol.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT port isolation set failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT port isolation set successful\n");
			break;

		case NSS_PPE_PORT_MGMT_ACT_CTRL_SET_MSG:
			ret = port_mgmt_info->isol.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("Action Control set failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("Action Control set successful\n");
			break;

		case NSS_PPE_PORT_MGMT_PORT_ISOL_DEF_MSG:
			ret = port_mgmt_info->isol.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("Isol default set failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("Action Control set successful\n");
			break;

		case NSS_PPE_PORT_MGMT_MAC_LRN_LIMIT_SET_MSG:
			ret = port_mgmt_info->mac_lrn_limit.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT mac learn limit set failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT mac learn limit set successful\n");
			break;

		case NSS_PPE_PORT_MGMT_MAC_FILTER_SET_MSG:
			ret = port_mgmt_info->mac_filter.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT mac filter set failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT mac filter set successful\n");
			break;

		case NSS_PPE_PORT_MGMT_MAC_FILTER_CLR_MSG:
			ret = port_mgmt_info->mac_filter.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT filter clear failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT mac filter clear successful\n");
			break;

		case NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG:
			ret = port_mgmt_info->omci_port.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT OMCI port add failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT OMCI port add successful\n");
			break;

		case NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG:
			ret = port_mgmt_info->omci_port.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT OMCI port del failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT OMCI port del successful\n");
			break;

		case NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG:
			ret = port_mgmt_info->omci_port.ret;
			if (ret != PPE_PORT_MGMT_RET_SUCCESS) {
				nss_ppenl_sock_log_error("PORT_MGMT OMCI port flush failed with error: %d\n", ret);
				return;
			}
			nss_ppenl_sock_log_info("PORT_MGMT OMCI port flush successful\n");
			break;

		default:
			nss_ppenl_sock_log_error("unsupported message cmd type(%d)\n", cmd);
	}
}

/*
 * nss_ppenl_port_mgmt_sock_cb()
 *	NSS NL PORT_MGMT callback
 */
int nss_ppenl_port_mgmt_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_ppenl_port_mgmt_ctx *ctx = (struct nss_ppenl_port_mgmt_ctx *)arg;
	struct nss_ppenl_sock_ctx *sock = &ctx->sock;

	struct nss_ppenl_port_mgmt_info *port_mgmt_info = nss_ppenl_sock_get_data(msg);

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("%d:failed to get NSS NL PORT_MGMT header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_ppenl_cmn_get_cmd_type(&port_mgmt_info->cm);
	switch (cmd) {
		case NSS_PPE_PORT_MGMT_PORT_ISOL_SET_MSG:
		case NSS_PPE_PORT_MGMT_ACT_CTRL_SET_MSG:
		case NSS_PPE_PORT_MGMT_PORT_ISOL_DEF_MSG:
		case NSS_PPE_PORT_MGMT_MAC_LRN_LIMIT_SET_MSG:
		case NSS_PPE_PORT_MGMT_MAC_FILTER_SET_MSG:
		case NSS_PPE_PORT_MGMT_MAC_FILTER_CLR_MSG:
		case NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG:
		case NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG:
		case NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG:
			{
				void *cb_data = nss_ppenl_cmn_get_cb_data(&port_mgmt_info->cm, sock->family_id);
				if (!cb_data) {
					return NL_SKIP;
				}

				/*
				 * Note: The callback user can modify the CB content so it
				 * needs to locally save the response data for further use
				 * after the callback is completed
				 */
				struct nss_ppenl_port_mgmt_resp resp;
				memcpy(&resp, cb_data, sizeof(struct nss_ppenl_port_mgmt_resp));

				/*
				 * clear the ownership of the CB so that callback user can
				 * use it if needed
				 */
				nss_ppenl_cmn_clr_cb_owner(&port_mgmt_info->cm);

				if (!resp.cb) {
					nss_ppenl_sock_log_info("%d:no PORT_MGMT response callback for cmd(%d)\n", pid, cmd);
					return NL_SKIP;
				}

				resp.cb(sock->user_ctx, port_mgmt_info, resp.data);

				return NL_OK;
			}

		default:
			nss_ppenl_sock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
			return NL_SKIP;
	}
}

/*
 * nss_ppenl_port_mgmt_sock_open()
 *	this opens the NSS PORT_MGMT NL socket for usage
 */
int nss_ppenl_port_mgmt_sock_open(struct nss_ppenl_port_mgmt_ctx *ctx, void *user_ctx)
{
	pid_t pid = getpid();
	int error;
	if (!ctx) {
		nss_ppenl_sock_log_error("%d: invalid parameters passed\n", pid);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	nss_ppenl_sock_set_family(&ctx->sock, NSS_PPENL_PORT_MGMT_FAMILY);
	nss_ppenl_sock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_ppenl_sock_open(&ctx->sock, nss_ppenl_port_mgmt_sock_cb);
	if (error) {
		nss_ppenl_sock_log_error("%d:unable to open NSS PORT_MGMT socket, error(%d)\n", pid, error);
		goto fail;
	}

	return 0;
fail:
	memset(ctx, 0, sizeof(*ctx));
	return error;
}

/*
 * nss_ppenl_port_mgmt_sock_close()
 *	close the NSS PORT_MGMT NL socket
 */
void nss_ppenl_port_mgmt_sock_close(struct nss_ppenl_port_mgmt_ctx *ctx)
{
	nss_ppenl_sock_close(&ctx->sock);
}

/*
 * nss_ppenl_port_mgmt_sock_send()
 *	register callback and send the PORT_MGMT message synchronously through the socket
 */
int nss_ppenl_port_mgmt_sock_send(struct nss_ppenl_port_mgmt_ctx *ctx, struct nss_ppenl_port_mgmt_info *port_mgmt_info, nss_ppenl_port_mgmt_resp_cb_t cb)
{
	int32_t family_id = ctx->sock.family_id;
	struct nss_ppenl_port_mgmt_resp *resp;
	pid_t pid = getpid();
	bool has_resp = false;
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("%d:invalid NSS PORT_MGMT info\n", pid);
		return -ENOMEM;
	}

	if (cb) {
		nss_ppenl_cmn_set_cb_owner(&port_mgmt_info->cm, family_id);

		resp = nss_ppenl_cmn_get_cb_data(&port_mgmt_info->cm, family_id);
		assert(resp);

		resp->data = NULL;
		resp->cb = cb;
		has_resp = true;
	}

	error = nss_ppenl_sock_send(&ctx->sock, &port_mgmt_info->cm, port_mgmt_info, has_resp);
	if (error) {
		nss_ppenl_sock_log_error("%d:failed to send NSS PORT_MGMT info, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_ppenl_port_mgmt_port_isolation_set()
 *	Set the PORT_MGMT port isolation
 */
int nss_ppenl_port_mgmt_port_isolation_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info)
{
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("Invalid isol_info\n");
		return -EINVAL;
	}

	/*
	 * open the NSS NL PORT_MGMT socket
	 */
	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

/*
 * nss_ppenl_port_mgmt_act_ctrl_set()
 *	Set the action control fields.
 */
int nss_ppenl_port_mgmt_act_ctrl_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info) {
	int error;

	/*
	 * open the NSS NL PORT_MGMT socket
	 */
	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

int nss_ppenl_port_mgmt_isol_default(struct nss_ppenl_port_mgmt_info *port_mgmt_info) {
	int error;

	/*
	 * open the NSS NL ACL socket
	 */
	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open Port Mgmt socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message; error(%d)\n", error);
		goto done;
	}

done:
	/*
	 * close the socket
	 */
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

/*
 * nss_ppenl_port_mgmt_mac_lrn_limit_set()
 *      Set the PORT_MGMT mac learn limit
 */
int nss_ppenl_port_mgmt_mac_lrn_limit_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info) {
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("Invalid mac_learn_info\n");
		return -EINVAL;
	}

	/*
	 * open the NSS NL PORT_MGMT socket
	 */
	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

/*
 * nss_ppenl_port_mgmt_mac_filter_set()
 *      Set the PORT_MGMT mac filter set
 */
int nss_ppenl_port_mgmt_mac_filter_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info) {
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("Invalid mac_filter_info\n");
		return -EINVAL;
	}

	/*
	 * open the NSS NL PORT_MGMT socket
	 */
	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send message\n");
		goto done;
	}
done:
	/*
	 * close the socket
	 */
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

/*
 * nss_ppenl_port_mgmt_mac_filter_clear()
 *      Set the PORT_MGMT mac filter clear
 */
int nss_ppenl_port_mgmt_mac_filter_clear(struct nss_ppenl_port_mgmt_info *port_mgmt_info) {
        int error;

        if (!port_mgmt_info) {
                nss_ppenl_sock_log_error("Invalid mac_filter_info\n");
                return -EINVAL;
        }

        /*
         * open the NSS NL PORT_MGMT socket
         */
        error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
        if (error < 0) {
                nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
                return error;
        }

        /*
         * send message
         */
        error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
        if (error < 0) {
                nss_ppenl_sock_log_error("Unable to send message\n");
                goto done;
        }
done:
        /*
         * close the socket
         */
        nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
        return error;
}

/*
 * nss_ppenl_port_mgmt_omci_port_add()
 *	Add a port to the OMCI-managed port list.
 */
int nss_ppenl_port_mgmt_omci_port_add(struct nss_ppenl_port_mgmt_info *port_mgmt_info)
{
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("Invalid port_mgmt_info\n");
		return -EINVAL;
	}

	nss_ppenl_rule_port_mgmt_init(port_mgmt_info, NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG);

	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send OMCI port add message\n");
		goto done;
	}
done:
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

/*
 * nss_ppenl_port_mgmt_omci_port_del()
 *	Remove a port from the OMCI-managed port list.
 */
int nss_ppenl_port_mgmt_omci_port_del(struct nss_ppenl_port_mgmt_info *port_mgmt_info)
{
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("Invalid port_mgmt_info\n");
		return -EINVAL;
	}

	nss_ppenl_rule_port_mgmt_init(port_mgmt_info, NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG);

	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send OMCI port del message\n");
		goto done;
	}
done:
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}

/*
 * nss_ppenl_port_mgmt_omci_port_flush()
 *	Flush all ports from the OMCI-managed port list.
 */
int nss_ppenl_port_mgmt_omci_port_flush(struct nss_ppenl_port_mgmt_info *port_mgmt_info)
{
	int error;

	if (!port_mgmt_info) {
		nss_ppenl_sock_log_error("Invalid port_mgmt_info\n");
		return -EINVAL;
	}

	nss_ppenl_rule_port_mgmt_init(port_mgmt_info, NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG);

	error = nss_ppenl_port_mgmt_sock_open(&nss_port_mgmt_ctx, NULL);
	if (error < 0) {
		nss_ppenl_sock_log_error("Failed to open PORT_MGMT socket; error(%d)\n", error);
		return error;
	}

	error = nss_ppenl_port_mgmt_sock_send(&nss_port_mgmt_ctx, port_mgmt_info, nss_ppenl_port_mgmt_resp);
	if (error < 0) {
		nss_ppenl_sock_log_error("Unable to send OMCI port flush message\n");
		goto done;
	}
done:
	nss_ppenl_port_mgmt_sock_close(&nss_port_mgmt_ctx);
	return error;
}
