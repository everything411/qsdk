/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

#include <netlink/socket.h>
#include <netlink/netlink.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <stdbool.h>
#include "nss-tcp-st.h"

/*
 * nss_tcp_st_nl_attrib
 *	Netlink response from kernel
 */
struct nss_tcp_st_nl_attrib {
	struct netfn_tcpst_result res;
	struct netfn_tcpst_stats stats;
	enum netfn_tcpst_state state;
};

struct nss_tcp_st_sk g_tcpst;

/*
 * nss_tcp_st_nl_alloc
 *	Netlink socket alloc and init.
 */
static int nss_tcp_st_nl_alloc(void)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;

	tcpst->sk = nl_socket_alloc();
	if (!tcpst->sk) {
		nss_tcp_st_log_error("%px:Failed to allocate NL_Sock, family(%s)\n", tcpst, NETFN_TCPST_NL_FAMILY);
		return -1;;
	}

	if (genl_connect(tcpst->sk)) {
		nss_tcp_st_log_error("%px:Failed to connect GENL, family(%s)\n", tcpst, NETFN_TCPST_NL_FAMILY);
		goto free;
	}

	/*
	 * Resolve the generic nl family id
	 */
	tcpst->family_id = genl_ctrl_resolve(tcpst->sk, NETFN_TCPST_NL_FAMILY);
	if (tcpst->family_id < 0) {
		nss_tcp_st_log_error("%px Failed to connect family(%s, %d)\n", tcpst, NETFN_TCPST_NL_FAMILY, tcpst->family_id);

		goto close;
	}

	return 0;

close:
	nl_close(tcpst->sk);
free:
	nl_socket_free(tcpst->sk);
	return -1;
}

/*
 * nss_tcp_st_nl_dealloc
 *	Netlink socket dealloc.
 */
static void nss_tcp_st_nl_dealloc(void)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;

	nl_close(tcpst->sk);
	nl_socket_free(tcpst->sk);
}

/*
 * nss_tcp_st_nl_recv_attr
 *      Process the kernel space response
 */
static int nss_tcp_st_nl_recv_attr(struct nl_msg *msg, void *arg)
{
	struct nss_tcp_st_nl_attrib *resp = arg;
	struct genlmsghdr *genlh;
	struct nlmsghdr *nlh;
	struct nlattr *attr;
	int attr_len, rem;
	void *attr_head;

	nlh = nlmsg_hdr(msg);
	genlh = nlmsg_data(nlh);
	attr_head = genlmsg_attrdata(genlh, 0);
	attr_len = genlmsg_attrlen(genlh, 0);

	nla_for_each_attr(attr, attr_head ,attr_len, rem) {
		void *nl_data = nla_data(attr);
		int len = nla_len(attr);
		void *data;
		int data_len;

		switch(nla_type(attr)) {
			case NETFN_TCPST_NL_ATTR_RES:
				data = &resp->res;
				data_len = sizeof(resp->res);
				break;

			case NETFN_TCPST_NL_ATTR_STATE:
				data = &resp->state;
				data_len = sizeof(resp->state);
				break;

			case NETFN_TCPST_NL_ATTR_STATS:
				data = &resp->stats;
				data_len = sizeof(resp->stats);
				break;

			default:
				nss_tcp_st_log_error("%px: Unknown attribute\n", msg);
				return NL_STOP;
		}

		if (len != data_len) {
			nss_tcp_st_log_error("%px: Invalid attribute length\n", msg);
			return NL_STOP;
		}

		memcpy(data, nl_data, data_len);
	}

	return NL_OK;
}

/*
 * nss_tcp_st_nl_send
 *	Send netlink message.
 */
static int nss_tcp_st_nl_send(void *attr, int attrtype, int attrlen, uint8_t cmd)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;
	struct nl_msg *msg;
	void *hdr;
	int err;

	msg = nlmsg_alloc();
	if (!msg) {
		nss_tcp_st_log_error("%px Failed to allocate NL message buffer, family(%s)\n", tcpst, NETFN_TCPST_NL_FAMILY);
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, tcpst->family_id, 0, NLM_F_ACK, cmd, NETFN_TCPST_NL_VER);
	if (!hdr) {
		nss_tcp_st_log_error("%px Failed to put GENL message header (%s)\n", tcpst, NETFN_TCPST_NL_FAMILY);
		nlmsg_free(msg);
		return -ENOMEM;
	}

	nla_put(msg, attrtype, attrlen, attr);

	/*
	 * send netlink msg
	 */
	err = nl_send_auto(tcpst->sk, msg);
	if (err < 0) {
		nss_tcp_st_log_error("%px: Failed to send nl message, family(%s) with error(%d) %s cmd %d\n", tcpst, NETFN_TCPST_NL_FAMILY, err, nl_geterror(err), cmd);
		return err;
	}

	nlmsg_free(msg);

	return 0;
}

/*
 * nss_tcp_st_nl_recv
 *      Receive netlink message.
 */
static int nss_tcp_st_nl_recv(void *attr)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;
	struct nl_cb *cb;
	int err;

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb) {
		nss_tcp_st_log_error("%px:Failed to allocate cb\n", tcpst);
		return -ENOMEM;
	}

	err = nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nss_tcp_st_nl_recv_attr, attr);
	if (err) {
		nss_tcp_st_log_error("%px:Failed to set the callback\n", tcpst);
		nl_cb_put(cb);
		return -EINVAL;
	}

	err = nl_recvmsgs(tcpst->sk, cb);
	if (err) {
		nss_tcp_st_log_error("%px:nl_recvmsgs returned with error (%d), family(%s)\n", tcpst, err, NETFN_TCPST_NL_FAMILY);
		nl_cb_put(cb);
		return -EINVAL;
	}

	nl_cb_put(cb);
	return 0;
}

/*
 * nss_tcp_st_start
 *	Start TCP speed test.
 */
int nss_tcp_st_start(struct netfn_tcpst_cfg *cfg)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;
	struct nss_tcp_st_nl_attrib resp = {0};
	int error;

	error = nss_tcp_st_nl_alloc();
	if (error < 0) {
		nss_tcp_st_log_error("%px: Failed to initialize netlink socket\n", tcpst);
		return error;
	}

	error = nss_tcp_st_nl_send(cfg, NETFN_TCPST_NL_ATTR_CFG, sizeof(struct netfn_tcpst_cfg), NETFN_TCPST_NL_CMD_START);
	if (error < 0) {
		nss_tcp_st_log_error("%px: Family(%s) rule config failed(%d)\n", tcpst, NETFN_TCPST_NL_FAMILY, error);
		nss_tcp_st_nl_dealloc();
		return error;
	}

	error = nss_tcp_st_nl_recv(&resp);
	if (error < 0) {
		nss_tcp_st_log_error("%px: Family(%s) Failed to receive message from kernel(%d)\n", tcpst, NETFN_TCPST_NL_FAMILY, error);
		nss_tcp_st_nl_dealloc();
		return error;
	}

	nss_tcp_st_nl_dealloc();

	return (resp.state == NETFN_TCPST_STATE_INPROGRESS) ? 0 : -EFAULT;
}

/*
 * nss_tcp_st_stop
 *	Stop TCP speed test.
 */
int nss_tcp_st_stop(netfn_tcpst_comp_t completion_cb, void *app_data)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;
	struct nss_tcp_st_nl_attrib resp;
	int error;

	error = nss_tcp_st_nl_alloc();
	if (error < 0) {
		nss_tcp_st_log_error("%px: Failed to initialize netlink socket\n", tcpst);
		return error;
	}

	error = nss_tcp_st_nl_send(&resp.res, NETFN_TCPST_NL_ATTR_CFG, 0, NETFN_TCPST_NL_CMD_STOP);
	if (error < 0) {
		nss_tcp_st_log_error("%px: Family(%s) rule config failed(%d)\n", tcpst, NETFN_TCPST_NL_FAMILY, error);
		nss_tcp_st_nl_dealloc();
		return error;
	}

	error = nss_tcp_st_nl_recv(&resp);
	if (error < 0) {
		nss_tcp_st_log_error("%px: Family(%s) Failed to receive message from kernel(%d)\n", tcpst, NETFN_TCPST_NL_FAMILY, error);
		nss_tcp_st_nl_dealloc();
		return error;
	}

	nss_tcp_st_nl_dealloc();

	if (resp.state == NETFN_TCPST_STATE_STOPPED) {
		completion_cb(app_data, &resp.res);
		return 0;
	}

	return -EFAULT;
}

/*
 * nss_tcp_st_get_stats
 *	Get TCP speed test stats.
 */
int nss_tcp_st_get_stats(struct netfn_tcpst_stats *stats, enum netfn_tcpst_state *state)
{
	struct nss_tcp_st_sk *tcpst = &g_tcpst;
	struct nss_tcp_st_nl_attrib resp;
	int error;

	error = nss_tcp_st_nl_alloc();
	if (error < 0) {
		nss_tcp_st_log_error("%px: Failed to initialize netlink socket\n", tcpst);
		return error;
	}

	error = nss_tcp_st_nl_send(stats, NETFN_TCPST_NL_ATTR_CFG, sizeof(struct netfn_tcpst_stats), NETFN_TCPST_NL_CMD_STATS);
	if (error < 0) {
		nss_tcp_st_log_error("%px: Family(%s) rule config failed(%d)\n", tcpst, NETFN_TCPST_NL_FAMILY, error);
		nss_tcp_st_nl_dealloc();
		return error;
	}

	error = nss_tcp_st_nl_recv(&resp);
	if (error < 0) {
		nss_tcp_st_log_error("%px: Family(%s) Failed to receive message from kernel(%d)\n", tcpst, NETFN_TCPST_NL_FAMILY, error);
		nss_tcp_st_nl_dealloc();
		return error;
	}

	*stats = resp.stats;
	*state = resp.state;
	nss_tcp_st_nl_dealloc();

	return 0;
}
