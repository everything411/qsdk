/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __PPECFG_POLICER_H
#define __PPECFG_POLICER_H
#define PPECFG_POLICER_HDR_VERSION 4
#define PPECFG_POLICER_MIN_INFO_RATE_BYTE 0x1F40	/* HW supported byte based min rate 64 kbps*/
#define PPECFG_POLICER_MAX_INFO_RATE_BYTE 0x4A817C80	/* HW supported byte based max rate 10 Gbps */
#define PPECFG_POLICER_MIN_INFO_RATE_FRAME 0x6		/* HW supported frame based min rate 6 pps */
#define PPECFG_POLICER_MAX_INFO_RATE_FRAME 0xE310E8	/* HW supported frame based max rate 14881000 pps */
#define PPECFG_POLICER_MAX_BURST_SIZE_FRAME 0x1FF2B60	/* HW supported frame based max burst size 4.29 Gbyte */
#define PPECFG_POLICER_MAX_BURST_SIZE_BYTE 0x4A817C80	/* HW supported byte based max burst size 33.5 million packet */
/*
 * PPECFG POLICER commands
 */
enum ppecfg_policer_cmd {
	PPECFG_POLICER_ADD,	/* POLICER ADD*/
	PPECFG_POLICER_DEL,	/* POLICER DEL*/
	PPECFG_POLICER_FLUSH,	/* POLICER FLUSH*/
	PPECFG_POLICER_CMD_MAX
};

/*
 * PPECFG POLICER flow add
 */
enum ppecfg_policer_rule_add {
	PPECFG_POLICER_RULE_ADD_IS_PORT_POLICER = 0,	/* port policer */
	PPECFG_POLICER_RULE_ADD_DEV,	/*dev */
	PPECFG_POLICER_RULE_ADD_RULE_ID,	/* policer id */
	PPECFG_POLICER_RULE_ADD_METER_MODE,	/* meter mode */
	PPECFG_POLICER_RULE_ADD_METER_UNIT,	/* meter unit */
	PPECFG_POLICER_RULE_ADD_COMMITTED_RATE,	/* CIR */
	PPECFG_POLICER_RULE_ADD_COMMITTED_BURST_SIZE,	/* CBS */
	PPECFG_POLICER_RULE_ADD_PEAK_RATE,	/* EIR */
	PPECFG_POLICER_RULE_ADD_PEAK_BURST_SIZE,	/* EBS */
	PPECFG_POLICER_RULE_ADD_METER_ENABLE,	/* meter flag */
	PPECFG_POLICER_RULE_ADD_COUPLE_ENABLE,	/* coupling flag */
	PPECFG_POLICER_RULE_ADD_COLOUR_AWARE,	/* colour flag */
	PPECFG_POLICER_RULE_ADD_YELLOW_DP,	/* yellow dp */
	PPECFG_POLICER_RULE_ADD_YELLOW_INT_PRI,	/* yellow pri */
	PPECFG_POLICER_RULE_ADD_YELLOW_PCP,	/* yellow pcp */
	PPECFG_POLICER_RULE_ADD_YELLOW_DEI,	/* yellow dei */
	PPECFG_POLICER_RULE_ADD_YELLOW_DSCP,	/* yellow dscp */
	PPECFG_POLICER_RULE_ADD_MAX	/* max attribute */
};

/*
 * PPECFG POLICER flow del
 */
enum ppecfg_policer_rule_del {
	PPECFG_POLICER_RULE_DEL_IS_PORT_POLICER = 0,	/* port policer */
	PPECFG_POLICER_RULE_DEL_DEV,	/* dev */
	PPECFG_POLICER_RULE_DEL_RULE_ID,	/* rule id */
	PPECFG_POLICER_RULE_DEL_MAX	/* max attribute */
};
#endif /* __PPECFG_POLICER_H*/