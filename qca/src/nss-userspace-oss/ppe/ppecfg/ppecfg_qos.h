/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_QOS_H
#define __PPECFG_QOS_H
#define PPECFG_QOS_HDR_VERSION 4
/*
 * PPECFG QOS commands
 */
enum ppecfg_qos_cmd {
	PPECFG_QOS_GET_INT_PRI,	/* GET INTRERNAL PRIORITY */
	PPECFG_QOS_CMD_MAX /* max attribute */
};

/*
 * PPECFG QOS GET INT_PRI
 */
enum ppecfg_qos_get_int_pri {
	PPECFG_QOS_DEV,	/*dev */
	PPECFG_QOS_HANDLE_ID,	/* class id or handle id*/
	PPECFG_QOS_MAX	/* max attribute */
};

#endif /* __PPECFG_QOS_H*/
