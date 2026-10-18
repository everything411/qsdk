/*
 * netfn_tcpst_nl.h
 *	Netfn TCP Netlink header
 *
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
#ifndef __NETFN_TCPST_NL_H
#define __NETFN_TCPST_NL_H

#include "netfn_tcpst.h"

#define NETFN_TCPST_NL_FAMILY "tcpst"
#define NETFN_TCPST_NL_VER 1

/*
 * netfn_tcpst_nl_cmd
 *      TCPST cmds
 */
enum netfn_tcpst_nl_cmd {
	NETFN_TCPST_NL_CMD_UNSPEC,	/**< Must NOT use element 0. */
	NETFN_TCPST_NL_CMD_START,	/**< Command to start test. */
	NETFN_TCPST_NL_CMD_STATS,	/**< Command to get stats. */
	NETFN_TCPST_NL_CMD_STOP,	/**< Command to stop test. */
	NETFN_TCPST_NL_CMD_MAX,		/**< TCPST cmd MAX. */
};

/*
 * netfn_tcpst_nl_attr
 *      TCPST attributes
 */
enum netfn_tcpst_nl_attr {
	NETFN_TCPST_NL_ATTR_UNSPEC,	/**< Must NOT use element 0. */
	NETFN_TCPST_NL_ATTR_CFG,	/**< TCPST configuration attributes. */
	NETFN_TCPST_NL_ATTR_RES,	/**< TCPST result attribute. */
	NETFN_TCPST_NL_ATTR_STATS,	/**< TCPST stat attribute. */
	NETFN_TCPST_NL_ATTR_STATE,	/**< TCPST state attribute. */
	NETFN_TCPST_NL_ATTR_MAX,	/**< TCPST MAX attribute. */
};

#define NETFN_TCPST_NL_MAX (NETFN_TCPST_NL_ATTR_MAX + 1)

/*
 * netfn_tcpst_nl_register()
 *	Netlink handler init
 *
 * @void[in]
 *
 * @return
 * Status of netlink handler initialization
 */
bool netfn_tcpst_nl_register(void);

/*
 * netfn_tcpst_nl_unregister()
 *      Netlink handler deinit
 *
 * @void[in]
 *
 * @return
 * Status of netlink handler deinit
 */
bool netfn_tcpst_nl_unregister(void);

#endif
