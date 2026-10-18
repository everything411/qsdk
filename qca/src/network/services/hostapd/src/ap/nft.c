/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "utils/includes.h"
#include <libmnl/libmnl.h>
#include <linux/netlink.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nf_tables.h>
#include <linux/netfilter.h>
#include <linux/version.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include "utils/common.h"
#include "hostapd.h"
#include "nft.h"

#define NFT_SUPPORTED (LINUX_VERSION_CODE >= NFT_MIN_KERNEL_VERSION)

/* Global NFT context */
static struct nft_global *g_nft_global = NULL;

#if NFT_SUPPORTED
/* Full NFT implementation for kernel >= 5.16 */

bool hostapd_is_nft_initialized(void)
{

	if (!g_nft_global || !g_nft_global->nl)
		return false;
	else
		return true;
}


/**
 * parse_nft_table_attr - Parse table attributes from netlink message
 * @attr: Netlink attribute
 * @data: Pointer to attribute table array
 */
static int parse_nft_table_attr(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type;

	type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, NFTA_TABLE_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case NFTA_TABLE_NAME:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
			wpa_printf(MSG_DEBUG,
				   "NFT: Invalid table name attribute");
			return MNL_CB_ERROR;
		}
		break;
	case NFTA_TABLE_FLAGS:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			wpa_printf(MSG_DEBUG,
				   "NFT: Invalid table flags attribute");
			return MNL_CB_ERROR;
		}
		break;
	}

	tb[type] = attr;
	return MNL_CB_OK;
}


/**
 * parse_nft_chain_attr - Parse chain attributes from netlink message
 * @attr: Netlink attribute
 * @data: Pointer to attribute table array
 */
static int parse_nft_chain_attr(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type;

	type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, NFTA_CHAIN_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case NFTA_CHAIN_NAME:
	case NFTA_CHAIN_TABLE:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
			wpa_printf(MSG_DEBUG,
				   "NFT: Invalid chain attribute");
			return MNL_CB_ERROR;
		}
		break;
	case NFTA_CHAIN_HANDLE:
		if (mnl_attr_validate(attr, MNL_TYPE_U64) < 0) {
			wpa_printf(MSG_DEBUG,
				   "NFT: Invalid chain handle attribute");
			return MNL_CB_ERROR;
		}
		break;
	}

	tb[type] = attr;
	return MNL_CB_OK;
}


/* Parse attributes from netlink message for rules */
static int parse_nft_rule_attr(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type;

	type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, NFTA_RULE_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case NFTA_RULE_TABLE:
	case NFTA_RULE_CHAIN:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0)
			return MNL_CB_ERROR;
		break;
	case NFTA_RULE_HANDLE:
		if (mnl_attr_validate(attr, MNL_TYPE_U64) < 0)
			return MNL_CB_ERROR;
		break;
	}

	tb[type] = attr;
	return MNL_CB_OK;
}


/**
 * nl_cb_handle_table - Handle table-related netlink messages
 * @nlh: Netlink message header
 * @nfg: NFGen message
 * @msg_type: Message type
 */
static int nl_cb_handle_table(const struct nlmsghdr *nlh,
			      const struct nfgenmsg *nfg, uint8_t msg_type)
{
	struct nlattr *tb[NFTA_TABLE_MAX + 1] = {};
	const char *table_name;
	const char *operation;
	u32 flags = 0;
	int ret;

	ret = mnl_attr_parse(nlh, sizeof(*nfg), parse_nft_table_attr, tb);
	if (ret != MNL_CB_OK) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to parse table attributes (seq=%u)",
			   nlh->nlmsg_seq);
		return MNL_CB_ERROR;
	}

	operation = (msg_type == NFT_MSG_NEWTABLE) ? "created" : "deleted";

	if (tb[NFTA_TABLE_NAME]) {
		table_name = mnl_attr_get_str(tb[NFTA_TABLE_NAME]);

		if (tb[NFTA_TABLE_FLAGS])
			flags = ntohl(mnl_attr_get_u32(tb[NFTA_TABLE_FLAGS]));

		wpa_printf(MSG_INFO,
			   "NFT: Table '%s' %s successfully (family=%u, flags=0x%x, seq=%u)",
			   table_name, operation, nfg->nfgen_family, flags,
			   nlh->nlmsg_seq);
	} else {
		wpa_printf(MSG_WARNING,
			   "NFT: Table %s but name attribute missing (seq=%u)",
			   operation, nlh->nlmsg_seq);
	}

	return MNL_CB_OK;
}


/**
 * nl_cb_handle_chain - Handle chain-related netlink messages
 * @nlh: Netlink message header
 * @nfg: NFGen message
 * @msg_type: Message type
 */
static int nl_cb_handle_chain(const struct nlmsghdr *nlh,
			      const struct nfgenmsg *nfg, uint8_t msg_type)
{
	struct nlattr *tb[NFTA_CHAIN_MAX + 1] = {};
	const char *operation;
	const char *chain_name;
	const char *table_name;
	u64 handle = 0;
	int ret;

	ret = mnl_attr_parse(nlh, sizeof(*nfg), parse_nft_chain_attr, tb);
	if (ret != MNL_CB_OK) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to parse chain attributes (seq=%u)",
			   nlh->nlmsg_seq);
		return MNL_CB_ERROR;
	}

	operation = (msg_type == NFT_MSG_NEWCHAIN) ? "created" : "deleted";

	if (tb[NFTA_CHAIN_TABLE] && tb[NFTA_CHAIN_NAME]) {
		chain_name = mnl_attr_get_str(tb[NFTA_CHAIN_NAME]);
		table_name = mnl_attr_get_str(tb[NFTA_CHAIN_TABLE]);

		if (tb[NFTA_CHAIN_HANDLE])
			handle = be64toh(mnl_attr_get_u64(tb[NFTA_CHAIN_HANDLE]));

		wpa_printf(MSG_INFO,
			   "NFT: Chain '%s' in table '%s' %s successfully (family=%u, handle=%llu, seq=%u)",
			   chain_name, table_name, operation,
			   nfg->nfgen_family, (unsigned long long) handle,
			   nlh->nlmsg_seq);
	} else {
		wpa_printf(MSG_WARNING,
			   "NFT: Chain %s but required attributes missing (seq=%u) - table=%s, chain=%s",
			   operation, nlh->nlmsg_seq,
			   tb[NFTA_CHAIN_TABLE] ? "present" : "missing",
			   tb[NFTA_CHAIN_NAME] ? "present" : "missing");
	}

	return MNL_CB_OK;
}


/**
 * nl_cb_handle_rule - Handle rule-related netlink messages
 * @nlh: Netlink message header
 * @nfg: NFGen message
 * @msg_type: Message type
 * @rule_params: Optional rule params for handle capture (NULL if not needed)
 * Returns: MNL_CB_OK on success, MNL_CB_ERROR on failure
 */
static int nl_cb_handle_rule(const struct nlmsghdr *nlh,
			     const struct nfgenmsg *nfg, uint8_t msg_type,
			     struct hostapd_nft_rule_params *rule_params)
{
	struct nlattr *tb[NFTA_RULE_MAX + 1] = {};
	const char *operation;
	const char *table = NULL;
	const char *chain = NULL;
	u64 handle = 0;
	int ret;

	ret = mnl_attr_parse(nlh, sizeof(*nfg), parse_nft_rule_attr, tb);
	if (ret != MNL_CB_OK) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to parse rule attributes (seq=%u)",
			   nlh->nlmsg_seq);
		return MNL_CB_ERROR;
	}

	operation = (msg_type == NFT_MSG_NEWRULE) ? "created" : "deleted";

	if (tb[NFTA_RULE_TABLE])
		table = mnl_attr_get_str(tb[NFTA_RULE_TABLE]);
	if (tb[NFTA_RULE_CHAIN])
		chain = mnl_attr_get_str(tb[NFTA_RULE_CHAIN]);
	if (tb[NFTA_RULE_HANDLE])
		handle = be64toh(mnl_attr_get_u64(tb[NFTA_RULE_HANDLE]));

	if (msg_type == NFT_MSG_NEWRULE) {
		if (handle && rule_params) {
			rule_params->handle = handle;
			wpa_printf(MSG_DEBUG,
				   "NFT: Rule %s successfully - table='%s' chain='%s' handle=%llu (seq=%u)",
				   operation,
				   table ? table : "unknown",
				   chain ? chain : "unknown",
				   (unsigned long long) handle,
				   nlh->nlmsg_seq);
		} else {
			wpa_printf(MSG_WARNING,
				   "NFT: Rule creation notification received but handle not captured - table='%s' chain='%s' handle_present=%s rule_params=%s (seq=%u)",
				   table ? table : "unknown",
				   chain ? chain : "unknown",
				   handle ? "yes" : "no",
				   rule_params ? "valid" : "null",
				   nlh->nlmsg_seq);
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "NFT: Rule %s successfully - table='%s' chain='%s' handle=%llu (seq=%u)",
			   operation,
			   table ? table : "unknown",
			   chain ? chain : "unknown",
			   (unsigned long long) handle,
			   nlh->nlmsg_seq);
	}

	return MNL_CB_OK;
}


static int nl_cb(const struct nlmsghdr *nlh, void *data)
{
	struct hostapd_nft_rule_params *rule_params = data;
	const struct nlmsgerr *err;
	const struct nfgenmsg *nfg;
	u8 subsys, msg_type;

	if (!nlh) {
		wpa_printf(MSG_ERROR, "NFT: nl_cb invalid nlh input");
		return MNL_CB_ERROR;
	}

	wpa_printf(MSG_DEBUG, "NFT: nlmsg type=0x%04x seq=%u flags=0x%04x",
		   nlh->nlmsg_type, nlh->nlmsg_seq, nlh->nlmsg_flags);

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		err = mnl_nlmsg_get_payload(nlh);
		if (!err) {
			wpa_printf(MSG_ERROR,
				   "NFT: Failed to get error payload from netlink message (seq=%u)",
				   nlh->nlmsg_seq);
			return MNL_CB_ERROR;
		}

		if (err->error == 0) {
			/* ACK received - operation succeeded */
			wpa_printf(MSG_DEBUG,
				   "NFT: Netlink ACK received - operation successful (seq=%u)",
				   nlh->nlmsg_seq);
			return MNL_CB_OK;
		}

		/* Error occurred */
		wpa_printf(MSG_ERROR,
			   "NFT: Netlink operation failed - %s (errno=%d, seq=%u)",
			   strerror(-err->error), -err->error,
			   nlh->nlmsg_seq);

		return MNL_CB_ERROR;
	}

	/* Handle completion message */
	if (nlh->nlmsg_type == NLMSG_DONE) {
		wpa_printf(MSG_DEBUG,
			   "NFT: Netlink batch processing completed successfully (seq=%u)",
			   nlh->nlmsg_seq);
		return MNL_CB_STOP;
	}

	/* Parse nftables messages */
	subsys = NFNL_SUBSYS_ID(nlh->nlmsg_type);
	if (subsys != NFNL_SUBSYS_NFTABLES) {
		wpa_printf(MSG_DEBUG,
			   "NFT: Received non-nftables netlink message (subsys=%u, seq=%u)",
			   subsys, nlh->nlmsg_seq);
		return MNL_CB_OK;
	}

	nfg = mnl_nlmsg_get_payload(nlh);
	msg_type = NFNL_MSG_TYPE(nlh->nlmsg_type);

	wpa_printf(MSG_DEBUG,
		   "NFT: Processing nftables message (type=%u, seq=%u)",
		   msg_type, nlh->nlmsg_seq);

	switch (msg_type) {
	case NFT_MSG_NEWTABLE:
	case NFT_MSG_DELTABLE:
		return nl_cb_handle_table(nlh, nfg, msg_type);
	case NFT_MSG_NEWCHAIN:
	case NFT_MSG_DELCHAIN:
		return nl_cb_handle_chain(nlh, nfg, msg_type);
	case NFT_MSG_NEWRULE:
	case NFT_MSG_DELRULE:
		return nl_cb_handle_rule(nlh, nfg, msg_type, rule_params);
	default:
		wpa_printf(MSG_DEBUG,
			   "NFT: Unhandled nftables message type %u (seq=%u)",
			   msg_type, nlh->nlmsg_seq);
		break;
	}

	return MNL_CB_OK;
}


/**
 * nft_init - Initialize NFT netlink socket
 */
int nft_init(void)
{
	int ret = 0;

	if (g_nft_global) {
		wpa_printf(MSG_DEBUG, "NFT: Already initialized");
		return ret;
	}

	g_nft_global = os_zalloc(sizeof(*g_nft_global));
	if (!g_nft_global) {
		wpa_printf(MSG_ERROR, "NFT: Failed to allocate global context");
		return -ENOMEM;
	}

	g_nft_global->nl = mnl_socket_open(NETLINK_NETFILTER);
	if (!g_nft_global->nl) {
		ret = errno;
		wpa_printf(MSG_ERROR, "NFT: Failed to open netlink socket: %s",
			   strerror(ret));
		os_free(g_nft_global);
		g_nft_global = NULL;
		return -ret;
	}

	if (mnl_socket_bind(g_nft_global->nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		ret = errno;
		wpa_printf(MSG_ERROR, "NFT: Failed to bind netlink socket: %s",
			   strerror(ret));
		mnl_socket_close(g_nft_global->nl);
		os_free(g_nft_global);
		g_nft_global = NULL;
		return -ret;
	}

	g_nft_global->portid = mnl_socket_get_portid(g_nft_global->nl);
	g_nft_global->seq = 0;

	wpa_printf(MSG_INFO, "NFT: Initialized (portid=%u)",
		   g_nft_global->portid);
	return ret;
}


/**
 * nft_deinit - Deinitialize NFT netlink socket
 */
void nft_deinit(void)
{
	if (!g_nft_global)
		return;

	if (g_nft_global->nl) {
		mnl_socket_close(g_nft_global->nl);
		g_nft_global->nl = NULL;
	}

	os_free(g_nft_global);
	g_nft_global = NULL;

	wpa_printf(MSG_INFO, "NFT: Deinitialized");
}


/**
 * nft_send_and_receive - handler for send/receive table/chain/rule ops
 * @batch: Netlink message batch
 * @p: Optional rule params for handle capture (NULL for generic ops)
 *
 * If @p is non-NULL, nl_cb can capture the
 * rule handle via NEWRULE notifications.
 */
static int nft_send_and_receive(struct mnl_nlmsg_batch *batch,
				struct hostapd_nft_rule_params *p)
{
	char rcv_buf[NFT_RECEIVE_BUF_SIZE];
	int ret, recv_count = 0;
	size_t batch_size;
	u32 seq = 0;

	batch_size = mnl_nlmsg_batch_size(batch);

	wpa_printf(MSG_DEBUG,
		   "NFT: Sending batch portid=%u size=%zu",
		   g_nft_global->portid, batch_size);

	ret = mnl_socket_sendto(g_nft_global->nl,
				mnl_nlmsg_batch_head(batch),
				batch_size);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Batch send failed - %s errno=%d",
			   strerror(errno), errno);
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Batch sent bytes=%d portid=%u size=%zu",
		   ret, g_nft_global->portid, batch_size);

	mnl_nlmsg_batch_stop(batch);

	do {
		ret = mnl_socket_recvfrom(g_nft_global->nl, rcv_buf,
					  sizeof(rcv_buf));
		if (ret == -1) {
			if (errno == EINTR) {
				wpa_printf(MSG_DEBUG,
					   "NFT: Receive interrupted, retrying...");
				continue;
			}
			wpa_printf(MSG_ERROR,
				   "NFT: Failed to receive netlink response - %s errno=%d",
				   strerror(errno), errno);
			break;
		}

		recv_count++;
		wpa_printf(MSG_DEBUG,
			   "NFT: Received netlink message #%d (%d bytes)",
			   recv_count, ret);

		/*seq 0 is used to handle all the messages in nl_cb*/
		ret = mnl_cb_run(rcv_buf, ret, seq, g_nft_global->portid,
				 nl_cb, p);
		if (ret <= 0) {
			if (ret == MNL_CB_STOP) {
				wpa_printf(MSG_DEBUG,
					   "NFT: Processing complete received=%d",
					   recv_count);
			} else if (ret == MNL_CB_ERROR) {
				wpa_printf(MSG_ERROR,
					   "NFT: Callback returned error");
			}
			break;
		}
	} while (1);

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Operation failed received=%d",
			   recv_count);
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Operation completed successfully received=%d",
		   recv_count);

	return ret;
}


/**
 * hostapd_mnl_batch_begin - Start a netlink batch operation
 * @batch: Netlink message batch
 * @cur_seq: Current sequence number
 */
static void hostapd_mnl_batch_begin(struct mnl_nlmsg_batch *batch,
				    uint32_t cur_seq)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;
	uint16_t family = NFPROTO_NETDEV;

	nlh = mnl_nlmsg_put_header(mnl_nlmsg_batch_current(batch));
	nlh->nlmsg_type = NFNL_MSG_BATCH_BEGIN;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = cur_seq;

	/* nfgenmsg header */
	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(NFNL_SUBSYS_RES_ID);

	mnl_nlmsg_batch_next(batch);
}


/**
 * hostapd_mnl_batch_end - End a netlink batch operation
 * @batch: Netlink message batch
 * @cur_seq: Current sequence number
 */
static void hostapd_mnl_batch_end(struct mnl_nlmsg_batch *batch,
				  uint32_t cur_seq)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;
	uint16_t family = NFPROTO_NETDEV;

	nlh = mnl_nlmsg_put_header(mnl_nlmsg_batch_current(batch));
	nlh->nlmsg_type = NFNL_MSG_BATCH_END;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = cur_seq;

	/* nfgenmsg header */
	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(NFNL_SUBSYS_RES_ID);

	mnl_nlmsg_batch_next(batch);
}


/**
 * hostapd_mnl_prepare_nlmsghdr - Prepare netlink message header
 * @batch: Netlink message batch
 * @msg_type: Message type (e.g., NFT_MSG_NEWTABLE)
 * @flags: Additional netlink flags
 * @seq: Pointer to sequence number (will be incremented)
 */
static struct nlmsghdr *hostapd_mnl_prepare_nlmsghdr(struct mnl_nlmsg_batch *batch,
						     uint16_t msg_type,
						     uint16_t flags,
						     uint32_t *seq)
{
	uint16_t family = NFPROTO_NETDEV;
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	nlh = mnl_nlmsg_put_header(mnl_nlmsg_batch_current(batch));
	nlh->nlmsg_type = (NFNL_SUBSYS_NFTABLES << 8) | msg_type;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | flags;
	nlh->nlmsg_seq = ++(*seq);

	/* nfgenmsg header */
	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(NFNL_SUBSYS_RES_ID);

	return nlh;
}


/**
 * hostapd_set_nft_table - Create or delete an nft table
 * @table_name: Name of the table to create/delete
 * @add: true to create, false to delete
 */
static int hostapd_set_nft_table(char *table_name, bool add)
{
	char batch_buf[BATCH_BUF_SIZE];
	struct mnl_nlmsg_batch *batch;
	uint32_t table_flags = 0;
	struct nlmsghdr *nlh;
	uint16_t msg_type;
	uint16_t flags;

	batch = mnl_nlmsg_batch_start(batch_buf, sizeof(batch_buf));
	if (!batch) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to initialize netlink batch");
		return -ENOMEM;
	}

	hostapd_mnl_batch_begin(batch, ++g_nft_global->seq);

	/* Set message type and flags based on operation */
	if (add) {
		msg_type = NFT_MSG_NEWTABLE;
		flags = NLM_F_CREATE | NLM_F_EXCL | NLM_F_ECHO;
	} else {
		msg_type = NFT_MSG_DELTABLE;
		flags = NLM_F_EXCL | NLM_F_ECHO;
	}

	nlh = hostapd_mnl_prepare_nlmsghdr(batch, msg_type, flags, &g_nft_global->seq);

	mnl_attr_put_strz(nlh, NFTA_TABLE_NAME, table_name);
	mnl_attr_put_u32(nlh, NFTA_TABLE_FLAGS, htonl(table_flags));
	mnl_nlmsg_batch_next(batch);

	hostapd_mnl_batch_end(batch, ++g_nft_global->seq);

	return nft_send_and_receive(batch, NULL);
}


/**
 * hostapd_set_nft_chain - Create or delete a nft chain
 * @table_name: Name of the table
 * @chain_name: Name of the chain to create/delete
 * @iface_name: Network interface name
 * @add: true to create, false to delete
 */
static int hostapd_set_nft_chain(char *table_name, char *chain_name,
				 char *iface_name, bool add)
{
	char batch_buf[BATCH_BUF_SIZE];
	struct mnl_nlmsg_batch *batch;
	struct nlmsghdr *nlh;
	struct nlattr *hook_nest;
	uint16_t msg_type;
	uint16_t flags;

	batch = mnl_nlmsg_batch_start(batch_buf, sizeof(batch_buf));
	if (!batch) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to initialize netlink batch");
		return -ENOMEM;
	}

	hostapd_mnl_batch_begin(batch, ++g_nft_global->seq);

	/* Set message type and flags based on operation */
	if (add) {
		msg_type = NFT_MSG_NEWCHAIN;
		flags = NLM_F_CREATE | NLM_F_EXCL | NLM_F_ECHO;
	} else {
		msg_type = NFT_MSG_DELCHAIN;
		flags = NLM_F_EXCL | NLM_F_ECHO;
	}

	nlh = hostapd_mnl_prepare_nlmsghdr(batch, msg_type, flags, &g_nft_global->seq);

	mnl_attr_put_strz(nlh, NFTA_CHAIN_TABLE, table_name);
	mnl_attr_put_strz(nlh, NFTA_CHAIN_NAME, chain_name);

	/* Add hook attributes only when creating a chain */
	if (add) {
		hook_nest = mnl_attr_nest_start(nlh, NFTA_CHAIN_HOOK);
		mnl_attr_put_u32(nlh, NFTA_HOOK_HOOKNUM, htonl(NF_NETDEV_EGRESS));
		mnl_attr_put_u32(nlh, NFTA_HOOK_PRIORITY, htonl(0));
		mnl_attr_put_strz(nlh, NFTA_HOOK_DEV, iface_name);
		mnl_attr_nest_end(nlh, hook_nest);

		mnl_attr_put_u32(nlh, NFTA_CHAIN_POLICY, htonl(NF_ACCEPT));
	}

	mnl_nlmsg_batch_next(batch);

	hostapd_mnl_batch_end(batch, ++g_nft_global->seq);

	return nft_send_and_receive(batch, NULL);
}


/**
 * hostapd_config_nft_table - Create or delete a nft table
 * @table: Table name
 * @add: true to create, false to delete
 */
int hostapd_config_nft_table(char *table, bool add)
{
	int ret;

	if (!hostapd_is_nft_initialized()) {
		wpa_printf(MSG_ERROR,
			   "NFT: Table config failed - NFT not initialized");
		return -EPERM;
	}

	if (!table || !table[0]) {
		wpa_printf(MSG_ERROR, "NFT: Invalid table name");
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG, "NFT: %s table '%s'",
		   add ? "Creating" : "Deleting", table);

	ret = hostapd_set_nft_table(table, add);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "NFT: Failed to %s table '%s'",
			   add ? "create" : "delete", table);
		return ret;
	}

	wpa_printf(MSG_INFO, "NFT: Table '%s' %s successfully", table,
		   add ? "created" : "deleted");
	return ret;
}


/**
 * hostapd_config_nft_chain - Create or delete a nft chain
 * @hapd: Pointer to hostapd data
 * @table: Table name
 * @chain: Chain name
 * @add: true to create, false to delete
 */
int hostapd_config_nft_chain(struct hostapd_data *hapd,
			     char *table, char *chain,
			     bool add)
{
	char *iface_name;
	int ret;

	if (!hostapd_is_nft_initialized()) {
		wpa_printf(MSG_ERROR,
			   "NFT: Chain config failed - NFT not initialized");
		return -EPERM;
	}

	if (!table || !table[0] || !chain || !chain[0]) {
		wpa_printf(MSG_ERROR, "NFT: Invalid table or chain name");
		return -EINVAL;
	}

	if (!hapd || !hapd->conf) {
		wpa_printf(MSG_ERROR,
			   "NFT: Invalid hostapd data or configuration");
		return -EINVAL;
	}

	iface_name = hapd->conf->iface;
	if (!iface_name || !iface_name[0]) {
		wpa_printf(MSG_ERROR, "NFT: Interface name not available");
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: %s chain '%s' in table '%s' for interface %s",
		   add ? "Creating" : "Deleting", chain, table, iface_name);

	ret = hostapd_set_nft_chain(table, chain, iface_name, add);

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to %s chain '%s' in table '%s' for interface %s",
			   add ? "create" : "delete", chain, table,
			   iface_name);
		return ret;
	}

	wpa_printf(MSG_INFO,
		   "NFT: Chain '%s' in table '%s' %s successfully for interface %s",
		   chain, table, add ? "created" : "deleted", iface_name);
	return ret;
}

int hostapd_config_nft_chain_for_dev(char *table, char *chain,
				     char *dev_name, bool add)
{
	int ret;

	if (!hostapd_is_nft_initialized()) {
		wpa_printf(MSG_ERROR,
			   "NFT: Chain config failed - NFT not initialized");
		return -EPERM;
	}

	if (!table || !table[0] || !chain || !chain[0] ||
	    !dev_name || !dev_name[0]) {
		wpa_printf(MSG_ERROR,
			   "NFT: Invalid table, chain or device name");
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: %s chain '%s' in table '%s' for device %s",
		   add ? "Creating" : "Deleting", chain, table, dev_name);

	ret = hostapd_set_nft_chain(table, chain, dev_name, add);
	if (ret < 0)
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to %s chain '%s' for device %s: %d",
			   add ? "create" : "delete", chain, dev_name, ret);
	else
		wpa_printf(MSG_INFO,
			   "NFT: Chain '%s' %s successfully for device %s",
			   chain, add ? "created" : "deleted", dev_name);
	return ret;
}


/* hostapd_nft_add_cmp_expr - Add a comparison expression
 * @nlh: Netlink message header
 * @sreg: Source register
 * @op: Comparison operation
 * @data: Data to compare against
 * @data_len: Length of data
 */
void hostapd_nft_add_cmp_expr(struct nlmsghdr *nlh, uint32_t sreg,
			      uint32_t op, const void *data, size_t data_len)
{
	struct nlattr *expr;
	struct nlattr *cmp_data;
	struct nlattr *data_attr;
	uint32_t sreg_n = htonl(sreg);
	uint32_t op_n = htonl(op);

	expr = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
	mnl_attr_put_strz(nlh, NFTA_EXPR_NAME, "cmp");

	cmp_data = mnl_attr_nest_start(nlh, NFTA_EXPR_DATA);
	mnl_attr_put(nlh, NFTA_CMP_SREG, sizeof(sreg_n), &sreg_n);
	mnl_attr_put(nlh, NFTA_CMP_OP, sizeof(op_n), &op_n);

	data_attr = mnl_attr_nest_start(nlh, NFTA_CMP_DATA);
	mnl_attr_put(nlh, NFTA_DATA_VALUE, data_len, data);
	mnl_attr_nest_end(nlh, data_attr);

	mnl_attr_nest_end(nlh, cmp_data);
	mnl_attr_nest_end(nlh, expr);

}


/**
 * hostapd_nft_add_meta_store - Add a meta store expression
 * @nlh: Netlink message header
 * @meta_key: Meta key to store to
 * @sreg: Source register
 */
void hostapd_nft_add_meta_store(struct nlmsghdr *nlh, uint32_t meta_key,
			        uint32_t sreg)
{
	struct nlattr *expr;
	struct nlattr *meta_data;
	uint32_t meta_key_n = htonl(meta_key);
	uint32_t sreg_n = htonl(sreg);

	expr = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
	mnl_attr_put_strz(nlh, NFTA_EXPR_NAME, "meta");

	meta_data = mnl_attr_nest_start(nlh, NFTA_EXPR_DATA);
	mnl_attr_put(nlh, NFTA_META_KEY, sizeof(meta_key_n), &meta_key_n);
	mnl_attr_put(nlh, NFTA_META_SREG, sizeof(sreg_n), &sreg_n);
	mnl_attr_nest_end(nlh, meta_data);
	mnl_attr_nest_end(nlh, expr);
}


/**
 * hostapd_nft_add_payld_cmp_expr - Add a payload and compare expression
 * @nlh: Netlink message header
 * @base: Payload base
 * @offset: Offset in payload
 * @len: Length to load
 * @reg: Register to load into
 * @data: Data to compare (optional)
 * @data_len: Length of data
 */
void hostapd_nft_add_payld_cmp_expr(struct nlmsghdr *nlh, uint32_t base,
				    uint32_t offset, uint32_t len, uint32_t reg,
				    const void *data, size_t data_len)
{
	struct nlattr *expr;
	struct nlattr *payload_data;
	uint32_t base_n = htonl(base);
	uint32_t offset_n = htonl(offset);
	uint32_t len_n = htonl(len);
	uint32_t reg_n = htonl(reg);

	expr = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
	mnl_attr_put_strz(nlh, NFTA_EXPR_NAME, "payload");

	payload_data = mnl_attr_nest_start(nlh, NFTA_EXPR_DATA);
	mnl_attr_put(nlh, NFTA_PAYLOAD_BASE, sizeof(base_n), &base_n);
	mnl_attr_put(nlh, NFTA_PAYLOAD_OFFSET, sizeof(offset_n), &offset_n);
	mnl_attr_put(nlh, NFTA_PAYLOAD_LEN, sizeof(len_n), &len_n);
	mnl_attr_put(nlh, NFTA_PAYLOAD_DREG, sizeof(reg_n), &reg_n);
	mnl_attr_nest_end(nlh, payload_data);
	mnl_attr_nest_end(nlh, expr);

	if (data && data_len > 0)
		hostapd_nft_add_cmp_expr(nlh, reg, NFT_CMP_EQ, data,
						   data_len);
}


void hostapd_nft_add_payload_mac_daddr(struct nlmsghdr *nlh,
				       const uint8_t mac[ETH_ALEN])
{
	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_LL_HEADER,
				       NFT_ETH_HDR_DEST_MAC_OFFSET,
				       ETH_ALEN, NFT_REG_1, mac, ETH_ALEN);
}


void hostapd_nft_add_payload_ip_proto(struct nlmsghdr *nlh, uint8_t proto)
{
	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_NETWORK_HEADER,
				       offsetof(struct iphdr, protocol),
				       sizeof(uint8_t), NFT_REG_1,
				       &proto, sizeof(proto));
}


void hostapd_nft_add_payload_ip6_nexthdr(struct nlmsghdr *nlh, uint8_t proto)
{
	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_NETWORK_HEADER,
				       offsetof(struct ipv6hdr, nexthdr),
				       sizeof(uint8_t), NFT_REG_1,
				       &proto, sizeof(proto));
}


void hostapd_nft_add_payload_tcp_dport(struct nlmsghdr *nlh, uint16_t port)
{
	uint16_t port_n = htons(port);

	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_TRANSPORT_HEADER,
				       offsetof(struct tcphdr, dest),
				       sizeof(uint16_t), NFT_REG_1,
				       &port_n, sizeof(port_n));
}


void hostapd_nft_add_payload_udp_dport(struct nlmsghdr *nlh, uint16_t port)
{
	uint16_t port_n = htons(port);

	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_TRANSPORT_HEADER,
				       offsetof(struct udphdr, dest),
				       sizeof(uint16_t), NFT_REG_1,
				       &port_n, sizeof(port_n));
}


/**
 * hostapd_nft_add_lookup_expr - Add lookup expression for set matching
 * @nlh: Netlink message header
 * @sreg: Source register containing value to lookup
 * @set_id: Set ID to lookup in
 */
static void hostapd_nft_add_lookup_expr(struct nlmsghdr *nlh, uint32_t sreg,
					uint32_t set_id)
{
	struct nlattr *expr;
	struct nlattr *lookup_data;
	uint32_t sreg_n = htonl(sreg);
	uint32_t set_id_n = htonl(set_id);
	char set_name[32];

	/* Generate the same set name pattern used in NEWSET */
	snprintf(set_name, sizeof(set_name), "__set%u", set_id);

	expr = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
	mnl_attr_put_strz(nlh, NFTA_EXPR_NAME, "lookup");

	lookup_data = mnl_attr_nest_start(nlh, NFTA_EXPR_DATA);
	mnl_attr_put(nlh, NFTA_LOOKUP_SREG, sizeof(sreg_n), &sreg_n);
	mnl_attr_put_strz(nlh, NFTA_LOOKUP_SET, set_name);
	mnl_attr_put(nlh, NFTA_LOOKUP_SET_ID, sizeof(set_id_n), &set_id_n);
	mnl_attr_nest_end(nlh, lookup_data);
	mnl_attr_nest_end(nlh, expr);
}


/**
 * hostapd_nft_new_set_msg - Create NFT_MSG_NEWSET message
 * @buf: Buffer for message
 * @nf_family: Network family
 * @seq: Sequence number
 * Returns: Pointer to netlink message header
 */
static struct nlmsghdr *hostapd_nft_new_set_msg(void *buf, uint16_t nf_family,
						uint32_t seq)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = (NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_NEWSET;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_ECHO;
	nlh->nlmsg_seq = seq;

	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = nf_family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(NFNL_SUBSYS_RES_ID);

	return nlh;
}


/**
 * hostapd_nft_create_set_msg - Create NFT_MSG_NEWSET message for anonymous set
 * @batch: Netlink message batch
 * @table: Table name
 * @set_name: Set name
 * @set_id: Unique set ID
 * @seq: Pointer to sequence number (will be incremented)
 * @nf_family: Network family
 * @element_size: Size of each element in bytes
 * @num_elements: Number of elements in the set
 */
static void hostapd_nft_create_set_msg(struct mnl_nlmsg_batch *batch,
				       const char *table,
				       const char *set_name,
				       uint32_t set_id,
				       uint32_t *seq,
				       uint16_t nf_family,
				       size_t element_size,
				       size_t num_elements)
{
	struct nlmsghdr *nlh;
	struct nlattr *desc;
	uint32_t flags;
	uint32_t key_type;
	uint32_t key_len;
	uint32_t set_id_n;
	uint32_t size_n;

	nlh = hostapd_nft_new_set_msg(mnl_nlmsg_batch_current(batch),
				      nf_family, ++(*seq));

	mnl_attr_put_strz(nlh, NFTA_SET_TABLE, table);
	mnl_attr_put_strz(nlh, NFTA_SET_NAME, set_name);

	flags = htonl(NFT_SET_ANONYMOUS | NFT_SET_CONSTANT);
	mnl_attr_put(nlh, NFTA_SET_FLAGS, sizeof(flags), &flags);

	key_type = htonl(NFT_DATA_VALUE);
	mnl_attr_put(nlh, NFTA_SET_KEY_TYPE, sizeof(key_type), &key_type);

	key_len = htonl(element_size);
	mnl_attr_put(nlh, NFTA_SET_KEY_LEN, sizeof(key_len), &key_len);

	set_id_n = htonl(set_id);
	mnl_attr_put(nlh, NFTA_SET_ID, sizeof(set_id_n), &set_id_n);

	desc = mnl_attr_nest_start(nlh, NFTA_SET_DESC);
	size_n = htonl(num_elements);
	mnl_attr_put(nlh, NFTA_SET_DESC_SIZE, sizeof(size_n), &size_n);
	mnl_attr_nest_end(nlh, desc);

	mnl_nlmsg_batch_next(batch);
}


/**
 * hostapd_nft_add_set_elements_msg - Create NFT_MSG_NEWSETELEM message
 * @batch: Netlink message batch
 * @table: Table name
 * @set_name: Set name
 * @set_id: Unique set ID
 * @seq: Pointer to sequence number (will be incremented)
 * @nf_family: Network family
 * @elements: Pointer to array of elements
 * @num_elements: Number of elements in the array
 * @element_size: Size of each element in bytes
 */
static int hostapd_nft_add_set_elements_msg(struct mnl_nlmsg_batch *batch,
					    const char *table,
					    const char *set_name,
					    uint32_t set_id,
					    uint32_t *seq,
					    uint16_t nf_family,
					    const void *elements,
					    size_t num_elements,
					    size_t element_size)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;
	struct nlattr *elem_list;
	struct nlattr *elem;
	struct nlattr *key;
	uint32_t set_id_n;
	size_t i;

	if (element_size != 2 && element_size != 4) {
		wpa_printf(MSG_ERROR, "NFT: Invalid element size");
		return -EINVAL;
	}

	if (!elements || num_elements == 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Invalid parameters elements=%p, num=%zu",
			   elements, num_elements);
		return -EINVAL;
	}

	nlh = mnl_nlmsg_put_header(mnl_nlmsg_batch_current(batch));
	nlh->nlmsg_type = (NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_NEWSETELEM;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE;
	nlh->nlmsg_seq = ++(*seq);

	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = nf_family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(NFNL_SUBSYS_RES_ID);

	mnl_attr_put_strz(nlh, NFTA_SET_ELEM_LIST_TABLE, table);

	/* For anonymous sets, provide BOTH name and ID for consistency */
	mnl_attr_put_strz(nlh, NFTA_SET_ELEM_LIST_SET, set_name);
	set_id_n = htonl(set_id);
	mnl_attr_put(nlh, NFTA_SET_ELEM_LIST_SET_ID, sizeof(set_id_n), &set_id_n);

	elem_list = mnl_attr_nest_start(nlh, NFTA_SET_ELEM_LIST_ELEMENTS);

	/* Add elements with proper byte order conversion based on size */
	for (i = 0; i < num_elements; i++) {
		elem = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
		key = mnl_attr_nest_start(nlh, NFTA_SET_ELEM_KEY);

		if (element_size == 2) {
			uint16_t value = ((const uint16_t *)elements)[i];
			uint16_t value_n = htons(value);
			mnl_attr_put(nlh, NFTA_DATA_VALUE, sizeof(value_n), &value_n);
			wpa_printf(MSG_DEBUG,
				   "NFT: Added element %u to set (index %zu)", value, i);
		} else {
			uint32_t value = ((const uint32_t *)elements)[i];
			uint32_t value_n = htonl(value);
			mnl_attr_put(nlh, NFTA_DATA_VALUE, sizeof(value_n), &value_n);
			wpa_printf(MSG_DEBUG,
				   "NFT: Added element 0x%x to set (index %zu)", value, i);
		}

		mnl_attr_nest_end(nlh, key);
		mnl_attr_nest_end(nlh, elem);
	}

	mnl_attr_nest_end(nlh, elem_list);
	mnl_nlmsg_batch_next(batch);

	return 0;
}


/**
 * hostapd_nft_add_anonymous_set - Add anonymous set with elements to batch
 * @batch: Netlink message batch
 * @table: Table name
 * @set_id: Unique set ID
 * @seq: Pointer to sequence number (will be incremented)
 * @nf_family: Network family
 * @elements: Pointer to array of elements (uint16_t* or uint32_t*)
 * @num_elements: Number of elements in the array
 * @element_size: Size of each element in bytes (2 for uint16_t, 4 for uint32_t)
 *
 * Generic function to create an anonymous set with elements of specified size.
 * This adds both NFT_MSG_NEWSET and NFT_MSG_NEWSETELEM messages to the batch.
 * Supports uint16_t (2 bytes) and uint32_t (4 bytes) element types.
 */
static int hostapd_nft_add_anonymous_set(struct mnl_nlmsg_batch *batch,
					 const char *table,
					 uint32_t set_id,
					 uint32_t *seq,
					 uint16_t nf_family,
					 const void *elements,
					 size_t num_elements,
					 size_t element_size)
{
	char set_name[32];
	int ret;

	if (!elements || num_elements == 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Invalid parameters for set creation (elements=%p, num=%zu)",
			   elements, num_elements);
		return -EINVAL;
	}

	if (element_size != 2 && element_size != 4) {
		wpa_printf(MSG_ERROR,
			   "NFT: Unsupported element size %zu (only 2 and 4 bytes supported)",
			   element_size);
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Creating anonymous set with %zu elements of %zu bytes each (set_id=%u)",
		   num_elements, element_size, set_id);

	snprintf(set_name, sizeof(set_name), "__set%u", set_id);

	/* Step 1: Create the anonymous set (NFT_MSG_NEWSET) */
	hostapd_nft_create_set_msg(batch, table, set_name, set_id, seq,
				   nf_family, element_size, num_elements);

	/* Step 2: Add elements to the set (NFT_MSG_NEWSETELEM) */
	ret = hostapd_nft_add_set_elements_msg(batch, table, set_name, set_id,
					       seq, nf_family, elements,
					       num_elements, element_size);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to add set elements message (set_id=%u)",
			   set_id);
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Anonymous set and elements added to batch successfully");

	return ret;
}


void hostapd_nft_add_payload_tcp_sport(struct nlmsghdr *nlh, uint16_t port)
{
	uint16_t port_n = htons(port);

	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_TRANSPORT_HEADER,
				       offsetof(struct tcphdr, source),
				       sizeof(uint16_t), NFT_REG_1,
				       &port_n, sizeof(port_n));
}


void hostapd_nft_add_payload_udp_sport(struct nlmsghdr *nlh, uint16_t port)
{
	uint16_t port_n = htons(port);

	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_TRANSPORT_HEADER,
				       offsetof(struct udphdr, source),
				       sizeof(uint16_t), NFT_REG_1,
				       &port_n, sizeof(port_n));
}


void hostapd_nft_add_payload_ip_saddr(struct nlmsghdr *nlh, uint32_t addr)
{
	/* Address already in network byte order */
	uint32_t addr_n = addr;

	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_NETWORK_HEADER,
				       offsetof(struct iphdr, saddr),
				       sizeof(uint32_t), NFT_REG_1,
				       &addr_n, sizeof(addr_n));
}


void hostapd_nft_add_payload_ip_daddr(struct nlmsghdr *nlh, uint32_t addr)
{
	/* Address already in network byte order */
	uint32_t addr_n = addr;

	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_NETWORK_HEADER,
				       offsetof(struct iphdr, daddr),
				       sizeof(uint32_t), NFT_REG_1,
				       &addr_n, sizeof(addr_n));
}


void hostapd_nft_add_payload_ip6_saddr(struct nlmsghdr *nlh,
				       const uint8_t addr[16])
{
	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_NETWORK_HEADER,
				       offsetof(struct ipv6hdr, saddr),
				       sizeof(struct in6_addr), NFT_REG_1,
				       addr, sizeof(struct in6_addr));
}


void hostapd_nft_add_payload_ip6_daddr(struct nlmsghdr *nlh,
				       const uint8_t addr[16])
{
	hostapd_nft_add_payld_cmp_expr(nlh, NFT_PAYLOAD_NETWORK_HEADER,
				       offsetof(struct ipv6hdr, daddr),
				       sizeof(struct in6_addr), NFT_REG_1,
				       addr, sizeof(struct in6_addr));
}


void hostapd_nft_add_immediate_expr(struct nlmsghdr *nlh, uint32_t reg,
				    const void *data, size_t data_len)
{
	struct nlattr *expr;
	struct nlattr *imm_data;
	struct nlattr *data_attr;
	uint32_t reg_n = htonl(reg);

	expr = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
	mnl_attr_put_strz(nlh, NFTA_EXPR_NAME, "immediate");

	imm_data = mnl_attr_nest_start(nlh, NFTA_EXPR_DATA);
	mnl_attr_put(nlh, NFTA_IMMEDIATE_DREG, sizeof(reg_n), &reg_n);

	if (data && data_len > 0) {
		data_attr = mnl_attr_nest_start(nlh, NFTA_IMMEDIATE_DATA);
		mnl_attr_put(nlh, NFTA_DATA_VALUE, data_len, data);
		mnl_attr_nest_end(nlh, data_attr);
	}

	mnl_attr_nest_end(nlh, imm_data);
	mnl_attr_nest_end(nlh, expr);
}


void hostapd_nft_add_counter_expr(struct nlmsghdr *nlh)
{
	struct nlattr *expr;
	struct nlattr *counter_data;

	expr = mnl_attr_nest_start(nlh, NFTA_LIST_ELEM);
	mnl_attr_put_strz(nlh, NFTA_EXPR_NAME, "counter");

	counter_data = mnl_attr_nest_start(nlh, NFTA_EXPR_DATA);
	mnl_attr_nest_end(nlh, counter_data);
	mnl_attr_nest_end(nlh, expr);
}


/**
 * hostapd_nft_setup_rule - Setup nftables rule expressions
 * @nlh: Netlink message header
 * @p: Rule parameters containing match criteria and actions
 */
int hostapd_nft_setup_rule(struct nlmsghdr *nlh,
			   struct hostapd_nft_rule_params *p)
{
	char match_str[512] = "";
	uint32_t spi_offset = 0;
	struct nlattr *exprs;
	uint32_t mark_val;
	uint32_t reg_tmp;
	bool is_udp_encap = false;
	int ret = 0;
	int pos = 0;

	if (!nlh) {
		wpa_printf(MSG_ERROR,
			   "NFT: NULL netlink header in setup_rule");
		return -EINVAL;
	}

	if (!p) {
		wpa_printf(MSG_ERROR,
			   "NFT: NULL rule parameters in setup_rule");
		return -EINVAL;
	}

	exprs = mnl_attr_nest_start(nlh, NFTA_RULE_EXPRESSIONS);

	if (p->valid_flags & NFT_RULE_PARAM_DMAC) {
		hostapd_nft_add_payload_mac_daddr(nlh, p->dmac);
		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%sdmac=" MACSTR, pos > 0 ? ", " : "",
				MAC2STR(p->dmac));
	}

	if (p->valid_flags & NFT_RULE_PARAM_PROTO) {
		if (p->ip_family == 4)
			hostapd_nft_add_payload_ip_proto(nlh, p->proto);
		else if (p->ip_family == 6)
			hostapd_nft_add_payload_ip6_nexthdr(nlh, p->proto);
		else {
			wpa_printf(MSG_ERROR,
				   "NFT: Invalid IP family %u for protocol match",
				   p->ip_family);
			ret = -EINVAL;
			goto out;
		}
		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%sproto=%s(%u)", pos > 0 ? ", " : "",
				p->proto == IPPROTO_TCP ? "TCP" :
				p->proto == IPPROTO_UDP ? "UDP" :
				p->proto == IPPROTO_ESP ? "ESP" : "?",
				p->proto);
	}

	if (p->valid_flags & NFT_RULE_PARAM_SADDR) {
		char addr_str[INET6_ADDRSTRLEN];

		if (p->ip_family == 4) {
			inet_ntop(AF_INET, &p->saddr4, addr_str, INET_ADDRSTRLEN);
			hostapd_nft_add_payload_ip_saddr(nlh, p->saddr4);
		} else if (p->ip_family == 6) {
			inet_ntop(AF_INET6, p->saddr6, addr_str, INET6_ADDRSTRLEN);
			hostapd_nft_add_payload_ip6_saddr(nlh, p->saddr6);
		} else {
			wpa_printf(MSG_ERROR,
				   "NFT: Invalid IP family %u for source address",
				   p->ip_family);
			ret = -EINVAL;
			goto out;
		}
		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%ssaddr=%s",
				pos > 0 ? ", " : "", addr_str);
	}

	if (p->valid_flags & NFT_RULE_PARAM_DADDR) {
		char addr_str[INET6_ADDRSTRLEN];

		if (p->ip_family == 4) {
			inet_ntop(AF_INET, &p->daddr4, addr_str, INET_ADDRSTRLEN);
			hostapd_nft_add_payload_ip_daddr(nlh, p->daddr4);
		} else if (p->ip_family == 6) {
			inet_ntop(AF_INET6, p->daddr6, addr_str, INET6_ADDRSTRLEN);
			hostapd_nft_add_payload_ip6_daddr(nlh, p->daddr6);
		} else {
			wpa_printf(MSG_ERROR,
				   "NFT: Invalid IP family %u for dest address",
				   p->ip_family);
			ret = -EINVAL;
			goto out;
		}
		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%sdaddr=%s", pos > 0 ? ", " : "", addr_str);
	}

	if (p->valid_flags & NFT_RULE_PARAM_SPORT) {
		if (p->proto == IPPROTO_TCP)
			hostapd_nft_add_payload_tcp_sport(nlh, p->sport);
		else if (p->proto == IPPROTO_UDP)
			hostapd_nft_add_payload_udp_sport(nlh, p->sport);
		else {
			wpa_printf(MSG_ERROR,
				   "NFT: Invalid protocol %u for source port match",
				   p->proto);
			ret = -EINVAL;
			goto out;
		}
		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%ssport=%u", pos > 0 ? ", " : "", p->sport);
	}

	if (p->valid_flags & NFT_RULE_PARAM_DPORT) {
		/* Special case: UDP port 4500 with ESP SPI - use set lookup for UDP encap IPsec */
		if ((p->proto == IPPROTO_UDP) &&
		    (p->dport == NFT_UDP_ENCAP_IPSEC_PORT_STANDARD) &&
		    (p->valid_flags & NFT_RULE_PARAM_SPI) &&
		    (p->set_id != 0)) {

		/* Load UDP destination port into register */
		hostapd_nft_add_payld_cmp_expr(nlh,
					       NFT_PAYLOAD_TRANSPORT_HEADER,
					       offsetof(struct udphdr, dest),
					       sizeof(uint16_t),
					       NFT_REG_1,
					       NULL, 0);

		/* Add lookup expression referencing the set */
		hostapd_nft_add_lookup_expr(nlh, NFT_REG_1, p->set_id);

		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%sdport={%u,%u}[set:%u]", pos > 0 ? ", " : "",
				NFT_UDP_ENCAP_IPSEC_PORT_STANDARD,
				NFT_UDP_ENCAP_IPSEC_PORT_ALT,
				p->set_id);
		} else {
			/* Normal case: Single port match */
			if (p->proto == IPPROTO_TCP)
				hostapd_nft_add_payload_tcp_dport(nlh, p->dport);
			else if (p->proto == IPPROTO_UDP)
				hostapd_nft_add_payload_udp_dport(nlh, p->dport);
			else {
				wpa_printf(MSG_ERROR,
					   "NFT: Invalid protocol %u for dest port match",
					   p->proto);
				ret = -EINVAL;
				goto out;
			}
			pos += snprintf(match_str + pos, sizeof(match_str) - pos,
					"%sdport=%u", pos > 0 ? ", " : "",
					p->dport);
		}
	}

	if (p->valid_flags & NFT_RULE_PARAM_SPI) {
		if (!(p->valid_flags & NFT_RULE_PARAM_PROTO)) {
			wpa_printf(MSG_ERROR,
				   "NFT: ESP SPI match requires NFT_RULE_PARAM_PROTO flag to be set");
			ret = -EINVAL;
			goto out;
		}

		/* Check for UDP-encapsulated ESP*/
		if (p->proto == IPPROTO_UDP) {
			/* UDP encapsulated ESP requires destination port 4500 */
			if (!(p->valid_flags & NFT_RULE_PARAM_DPORT)) {
				wpa_printf(MSG_ERROR,
					   "NFT: UDP-encapsulated ESP requires NFT_RULE_PARAM_DPORT flag");
				ret = -EINVAL;
				goto out;
			}

			if (p->dport != 4500) {
				wpa_printf(MSG_ERROR,
					   "NFT: UDP-encapsulated ESP requires dport=4500 (got %u)",
					   p->dport);
				ret = -EINVAL;
				goto out;
			}

			/* For UDP-encapsulated ESP, SPI is after UDP header (8 bytes) */
			spi_offset = sizeof(struct udphdr);
			is_udp_encap = true;
		} else if (p->proto == IPPROTO_ESP) {
			/* Native ESP: SPI is at offset 0 in ESP header */
			spi_offset = 0;
		} else {
			wpa_printf(MSG_ERROR,
				   "NFT: ESP SPI match requires IPPROTO_ESP or IPPROTO_UDP protocol (proto=%u)",
				   p->proto);
			ret = -EINVAL;
			goto out;
		}

		/* Add ESP SPI match at the calculated offset */
		hostapd_nft_add_payld_cmp_expr(nlh,
						NFT_PAYLOAD_TRANSPORT_HEADER,
						spi_offset,
						sizeof(uint32_t),
						NFT_REG_1,
						&p->esp_spi,
						sizeof(p->esp_spi));
		pos += snprintf(match_str + pos, sizeof(match_str) - pos,
				"%sspi=0x%x[%s]", pos > 0 ? ", " : "",
				p->esp_spi, is_udp_encap ? "NAT-T" : "native");
	}

	/* Add packet mark action */
	reg_tmp = NFT_REG_1;
	mark_val = p->mark;
	hostapd_nft_add_immediate_expr(nlh, reg_tmp, &mark_val,
				       sizeof(mark_val));

	hostapd_nft_add_meta_store(nlh, NFT_META_MARK, reg_tmp);

	/* Add counter expression */
	hostapd_nft_add_counter_expr(nlh);

out:
	mnl_attr_nest_end(nlh, exprs);

	if (ret == 0 && match_str[0]) {
		wpa_printf(MSG_DEBUG,
			   "NFT: Rule expressions added - table='%s' chain='%s' mark=0x%x | Matches: %s",
			   p->table, p->chain, p->mark, match_str);
	}

	return ret;
}


static struct nlmsghdr *hostapd_nft_new_rule_msg(void *buf,
						 uint16_t nf_family,
						 uint32_t seq)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = (NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_NEWRULE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE |
			   NLM_F_APPEND | NLM_F_ECHO;
	nlh->nlmsg_seq = seq;

	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = nf_family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(NFNL_SUBSYS_RES_ID);

	return nlh;
}


/**
 * hostapd_create_nft_rule - Create a new nftables rule
 * @p: Rule parameters containing match criteria and actions
 */
int hostapd_create_nft_rule(struct hostapd_nft_rule_params *p)
{
	char batch_buf[BATCH_BUF_SIZE];
	struct mnl_nlmsg_batch *batch;
	struct nlmsghdr *nlh;
	int ret;
	bool is_udp_encap_ipsec = false;
	uint32_t set_id = 0;
	uint16_t udp_encap_ports[] = { NFT_UDP_ENCAP_IPSEC_PORT_STANDARD, NFT_UDP_ENCAP_IPSEC_PORT_ALT };

	if (!p) {
		wpa_printf(MSG_ERROR,
			   "NFT: Rule creation failed - NULL rule parameters provided");
		return -EINVAL;
	}

	if (!p->table[0] || !p->chain[0]) {
		wpa_printf(MSG_ERROR,
			   "NFT: Rule creation failed - empty %s name in rule parameters",
			   !p->table[0] ? "table" : "chain");
		return -EINVAL;
	}

	/* Auto-detect UDP encap IPsec: UDP + port 4500 + ESP SPI */
	if ((p->valid_flags & NFT_RULE_PARAM_PROTO) &&
	    (p->valid_flags & NFT_RULE_PARAM_DPORT) &&
	    (p->valid_flags & NFT_RULE_PARAM_SPI) &&
	    p->proto == IPPROTO_UDP &&
	    p->dport == NFT_UDP_ENCAP_IPSEC_PORT_STANDARD) {
		is_udp_encap_ipsec = true;
		/*use seq number as reference for unique set id*/
		set_id = g_nft_global->seq + 1000;
		p->set_id = set_id;
		wpa_printf(MSG_DEBUG,
			   "NFT: UDP encap IPsec rule detected - will create port set {%u, %u} with set_id=%u",
			   NFT_UDP_ENCAP_IPSEC_PORT_STANDARD, NFT_UDP_ENCAP_IPSEC_PORT_ALT, set_id);
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Creating rule - table='%s' chain='%s' nf_family=%u valid_flags=0x%x mark=0x%x%s",
		   p->table, p->chain, p->nf_family, p->valid_flags, p->mark,
		   is_udp_encap_ipsec ? " (UDP encap IPsec)" : "");

	/* Initialize rule handle to 0 */
	p->handle = 0;

	batch = mnl_nlmsg_batch_start(batch_buf, sizeof(batch_buf));
	if (!batch) {
		wpa_printf(MSG_ERROR,
			   "NFT: Rule creation failed - failed to initialize netlink batch (table='%s', chain='%s')",
			   p->table, p->chain);
		return -ENOMEM;
	}

	hostapd_mnl_batch_begin(batch, ++g_nft_global->seq);

	if (is_udp_encap_ipsec) {
		wpa_printf(MSG_DEBUG,
			   "NFT: Adding UDP encap IPsec port set for rule (table='%s', chain='%s', set_id=%u)",
			   p->table, p->chain, set_id);

		ret = hostapd_nft_add_anonymous_set(batch, p->table,
						    set_id,
						    &g_nft_global->seq,
						    p->nf_family,
						    udp_encap_ports,
						    2,
						    sizeof(udp_encap_ports[0]));
		if (ret < 0) {
			wpa_printf(MSG_ERROR,
				   "NFT: Rule creation failed - failed to add UDP encap IPsec port set (table='%s', chain='%s', set_id=%u)",
				   p->table, p->chain, set_id);
			mnl_nlmsg_batch_stop(batch);
			return ret;
		}

		wpa_printf(MSG_DEBUG,
			   "NFT: UDP encap IPsec port set added successfully (table='%s', chain='%s', set_id=%u)",
			   p->table, p->chain, set_id);
	}

	/* Build NEWRULE message */
	nlh = hostapd_nft_new_rule_msg(mnl_nlmsg_batch_current(batch),
				       p->nf_family, ++g_nft_global->seq);
	mnl_attr_put_strz(nlh, NFTA_RULE_TABLE, p->table);
	mnl_attr_put_strz(nlh, NFTA_RULE_CHAIN, p->chain);

	ret = hostapd_nft_setup_rule(nlh, p);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Rule creation failed - failed to setup rule expressions (table='%s', chain='%s', valid_flags=0x%x)",
			   p->table, p->chain, p->valid_flags);
		mnl_nlmsg_batch_stop(batch);
		return ret;
	}

	mnl_nlmsg_batch_next(batch);

	hostapd_mnl_batch_end(batch, ++g_nft_global->seq);

	ret = nft_send_and_receive(batch, p);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Rule creation failed - netlink communication error table='%s', chain='%s', valid_flags=0x%x",
			   p->table, p->chain, p->valid_flags);
		return ret;
	}

	if (p->handle == 0) {
		wpa_printf(MSG_WARNING,
			   "NFT: Rule created but handle not received table='%s', chain='%s'",
			   p->table, p->chain);
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Rule created successfully - table='%s' chain='%s' handle=%llu mark=0x%x%s",
		   p->table, p->chain, (unsigned long long) p->handle, p->mark,
		   is_udp_encap_ipsec ? " (UDP encap IPsec)" : "");

	return ret;
}


/**
 * hostapd_delete_nft_rule - Delete an existing nft rule
 * @p: Rule parameters, must include table, chain, and handle
 * Returns: 0 on success, ret on failure
 */
int hostapd_delete_nft_rule(const struct hostapd_nft_rule_params *p)
{
	char batch_buf[BATCH_BUF_SIZE];
	struct mnl_nlmsg_batch *batch;
	struct nlmsghdr *nlh;
	uint64_t handle_n;
	int ret;

	if (!p) {
		wpa_printf(MSG_ERROR, "NFT: NULL rule parameters for deletion");
		return -EINVAL;
	}

	if (!p->table[0] || !p->chain[0] || p->handle == 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Invalid parameters for rule deletion - table='%s' chain='%s' handle=%llu",
			   p->table[0] ? p->table : "(empty)",
			   p->chain[0] ? p->chain : "(empty)",
			   (unsigned long long) p->handle);
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Deleting rule - table='%s' chain='%s' handle=%llu",
		   p->table, p->chain, (unsigned long long) p->handle);

	batch = mnl_nlmsg_batch_start(batch_buf, sizeof(batch_buf));
	if (!batch) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to initialize netlink batch for rule deletion");
		return -ENOMEM;
	}

	hostapd_mnl_batch_begin(batch, ++g_nft_global->seq);

	nlh = hostapd_mnl_prepare_nlmsghdr(batch, NFT_MSG_DELRULE,
					   NLM_F_ECHO, &g_nft_global->seq);
	mnl_attr_put_strz(nlh, NFTA_RULE_TABLE, p->table);
	mnl_attr_put_strz(nlh, NFTA_RULE_CHAIN, p->chain);

	handle_n = htobe64(p->handle);
	mnl_attr_put(nlh, NFTA_RULE_HANDLE, sizeof(handle_n), &handle_n);

	mnl_nlmsg_batch_next(batch);
	hostapd_mnl_batch_end(batch, ++g_nft_global->seq);

	ret = nft_send_and_receive(batch, NULL);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "NFT: Failed to delete rule (table='%s', chain='%s', handle=%llu)",
			   p->table, p->chain, (unsigned long long) p->handle);
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: Rule deleted successfully - table='%s' chain='%s' handle=%llu",
		   p->table, p->chain, (unsigned long long) p->handle);

	return ret;
}


/**
 * hostapd_config_nft_rule - Create or delete an nftables rule
 * @rparams: Rule parameters
 * @add: true to create, false to delete
 */
int hostapd_config_nft_rule(struct hostapd_nft_rule_params *rparams,
			    bool add)
{
	int ret = 0;

	if (!hostapd_is_nft_initialized()) {
		wpa_printf(MSG_ERROR,
			   "NFT: Rule config failed - NFT not initialized");
		return -EINVAL;
	}

	if (!rparams) {
		wpa_printf(MSG_ERROR,
			   "NFT: NULL rule parameters in config_nft_rule");
		return -EINVAL;
	}

	wpa_printf(MSG_DEBUG,
		   "NFT: %s rule - table='%s' chain='%s'",
		   add ? "Creating" : "Deleting",
		   rparams->table, rparams->chain);

	if (add) {
		ret = hostapd_create_nft_rule(rparams);
		if (ret < 0) {
			wpa_printf(MSG_ERROR,
				   "NFT: Rule creation failed (table='%s', chain='%s')",
				   rparams->table, rparams->chain);
		}
	} else {
		ret = hostapd_delete_nft_rule(rparams);
		if (ret < 0) {
			wpa_printf(MSG_ERROR,
				   "NFT: Rule deletion failed (table='%s', chain='%s', handle=%llu)",
				   rparams->table, rparams->chain,
				   (unsigned long long) rparams->handle);
		}
	}
	return ret;
}

#else /* !NFT_SUPPORTED */

/* Stub implementation for kernel < 5.16 */
int nft_init(void)
{
	wpa_printf(MSG_INFO,
		   "NFT: Not supported on this kernel version (requires >= 5.16, current: %d.%d)",
		   (LINUX_VERSION_CODE >> 16) & 0xFF,
		   (LINUX_VERSION_CODE >> 8) & 0xFF);
	return 0;
}


void nft_deinit(void)
{
	wpa_printf(MSG_DEBUG, "NFT: Deinit called (not supported)");
}


int hostapd_config_nft_table(char *table, bool add)
{
	wpa_printf(MSG_INFO,
		   "NFT: Table operation '%s' not supported on kernel < 5.16",
		   add ? "create" : "delete");
	return -EOPNOTSUPP;
}


int hostapd_config_nft_chain(struct hostapd_data *hapd,
			     char *table, char *chain,
			     bool add)
{
	wpa_printf(MSG_INFO,
		   "NFT: Chain operation '%s' not supported on kernel < 5.16",
		   add ? "create" : "delete");
	return -EOPNOTSUPP;
}


int hostapd_config_nft_rule(struct hostapd_nft_rule_params *rparams,
			    bool add)
{
	wpa_printf(MSG_DEBUG,
		   "NFT: Rule operation '%s' not supported on kernel < 5.16",
		   add ? "add" : "delete");
	return -EOPNOTSUPP;
}


int hostapd_config_nft_chain_for_dev(char *table, char *chain,
				     char *dev_name, bool add)
{
	wpa_printf(MSG_INFO,
		   "NFT: Chain-for-dev operation '%s' not supported on kernel < 5.16",
		   add ? "create" : "delete");
	return -EOPNOTSUPP;
}
#endif
