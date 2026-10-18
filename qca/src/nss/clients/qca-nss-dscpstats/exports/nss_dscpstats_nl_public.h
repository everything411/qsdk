/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _NSS_DSCPSTATS_NL_H_
#define _NSS_DSCPSTATS_NL_H_

/**
 * dscpstats netlink family name
 */
#define NSS_DSCPSTATS_FAMILY "nssdscpstats"

/**
 * dscpstats netlink version
 */
#define NSS_DSCPSTATS_VER 1

/**
 * nss_dscpstats_nl_stats_client
 *	Traffic statistics of each clients
 */
struct nss_dscpstats_nl_stats_client {
	uint8_t dscp;		/**< DSCP value */
	uint8_t reserved;	/**< reserved field for padding */
	uint8_t mac[6];		/**< ethernet mac address */
	uint64_t rxBytes;	/**< receive bytes */
	uint64_t txBytes;	/**< transmit bytes */
};

/**
 * nss_dscpstats_nl_read_msg
 *	Total client statistics information.
 */
struct nss_dscpstats_nl_read_msg {
	uint32_t num_clients;	/**< number of client statistics in this message*/
	uint32_t more;		/**< more statistics available */
	struct nss_dscpstats_nl_stats_client clients[0];	/**< Traffic statistics of each clients */
};

/**
 * nss_dscpstats_nl_conf_msg
 *	enable or disable traffic count.
 */
struct nss_dscpstats_nl_conf_msg {
	char name[16];		/**< Interface name */
	uint64_t dscp_mask;	/**< DSCP value bit mask 0 to 63 */
};

/**
 * nss_dscpstats_nl_rule
 *	Common message format
 */
struct nss_dscpstats_nl_rule {
	uint32_t cmd_len;	/**< length of the command */
	uint32_t cmd_type;	/**< command as in nss_dscpstats_message_types */
	uint32_t ret;		/**< status of command execution in driver */
	union {
		struct nss_dscpstats_nl_conf_msg conf_msg;  /**< enable or disable message */
	};
};

/**
 * nss_dscpstats_nl_msg
 *	Message types
 */
enum nss_dscpstats_nl_msg {
	NSS_DSCPSTATS_ENABLE_MSG = 1,	/**< ENABLE message type */
	NSS_DSCPSTATS_DISABLE_MSG,	/**< DISABLE message type */
	NSS_DSCPSTATS_RESET_MSG,	/**< RESET message type */
	NSS_DSCPSTATS_READ_MSG,		/**< READ message type */
	NSS_DSCPSTATS_MAX_MSG_TYPES,
};

#endif /* _NSS_DSCPSTATS_NL_H_ */
