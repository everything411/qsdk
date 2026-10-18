/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <qcatools_lib.h>
#include <wlan_stats_define.h>
#include <stats_lib.h>

#define FL "%s(): %d:"
#define NAME "wifitelemetry:"

#define STATS_ERR(fmt, args...) \
	fprintf(stderr, NAME "E:" FL ": "fmt, __func__, __LINE__, ## args)
#define STATS_WARN(fmt, args...) \
	fprintf(stdout, NAME "W:" FL ": "fmt, __func__, __LINE__, ## args)
#define STATS_MSG(fmt, args...) \
	fprintf(stdout, NAME "M:" FL ": "fmt, __func__, __LINE__, ## args)

#define STATS_PRINT(fmt, args...) \
	fprintf(stdout, fmt, ## args)

#define DESC_FIELD_PROVISIONING         31

#define STATS_UL(fp, descr, x) \
	fprintf(fp, "\t%-*s = %lu\n", DESC_FIELD_PROVISIONING, (descr), (x))

#define STATS_64(fp, descr, x) \
	fprintf(fp, "\t%-*s = %ju\n", DESC_FIELD_PROVISIONING, (descr), (x))

#define STATS_32(fp, descr, x) \
	fprintf(fp, "\t%-*s = %u\n", DESC_FIELD_PROVISIONING, (descr), (x))

#define STATS_16(fp, descr, x) \
	fprintf(fp, "\t%-*s = %hu\n", DESC_FIELD_PROVISIONING, (descr), (x))

#define STATS_16_SIGNED(fp, descr, x) \
	fprintf(fp, "\t%-*s = %hi\n", DESC_FIELD_PROVISIONING, (descr), (x))

#define STATS_8(fp, descr, x) \
	fprintf(fp, "\t%-*s = %hhu\n", DESC_FIELD_PROVISIONING, (descr), (x))

#define STATS_UNVLBL(fp, descr, msg) \
	fprintf(fp, "\t%-*s = %s\n", DESC_FIELD_PROVISIONING, (descr), (msg))

/* WLAN MAC Address length */
#define USER_MAC_ADDR_LEN     (ETH_ALEN * 3)

#define MAX_STRING_LEN        500

#define STATS_IF_MCS_VALID           1
#define STATS_IF_MCS_INVALID         0

#define STATS_IF_TX_FW_STATUS_OK     0
#define STATS_IF_TX_RR_FRAME_ACKED   0
#define STATS_IF_PEER_RIX_MASK       0xffff
#define STATS_IF_PEER_RIX_OFFSET     0
#define STATS_IF_GET_PEER_RIX(_val) \
	(((_val) >> STATS_IF_PEER_RIX_OFFSET) & STATS_IF_PEER_RIX_MASK)
#define STATS_IF_ASYNC_MODE          "async"

static bool g_run_loop;
static bool g_ipa_mode;

#if defined(WLAN_DEBUG_TELEMETRY) || defined(WLAN_ADVANCE_TELEMETRY)
#ifdef WLAN_FEATURE_11BE
static const struct stats_if_rate_debug rate_string[STATS_IF_DOT11_MAX]
						   [STATS_IF_MAX_MCS] = {
	{
		{"OFDM 48 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 24 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 12 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 6 Mbps ", STATS_IF_MCS_VALID},
		{"OFDM 54 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 36 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 18 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 9 Mbps ", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"CCK 11 Mbps Long  ", STATS_IF_MCS_VALID},
		{"CCK 5.5 Mbps Long ", STATS_IF_MCS_VALID},
		{"CCK 2 Mbps Long   ", STATS_IF_MCS_VALID},
		{"CCK 1 Mbps Long   ", STATS_IF_MCS_VALID},
		{"CCK 11 Mbps Short ", STATS_IF_MCS_VALID},
		{"CCK 5.5 Mbps Short", STATS_IF_MCS_VALID},
		{"CCK 2 Mbps Short  ", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"HT MCS 0 (BPSK 1/2)  ", STATS_IF_MCS_VALID},
		{"HT MCS 1 (QPSK 1/2)  ", STATS_IF_MCS_VALID},
		{"HT MCS 2 (QPSK 3/4)  ", STATS_IF_MCS_VALID},
		{"HT MCS 3 (16-QAM 1/2)", STATS_IF_MCS_VALID},
		{"HT MCS 4 (16-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HT MCS 5 (64-QAM 2/3)", STATS_IF_MCS_VALID},
		{"HT MCS 6 (64-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HT MCS 7 (64-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"VHT MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"VHT MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"VHT MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"VHT MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"VHT MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"VHT MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"VHT MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"HE MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"HE MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"HE MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"HE MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"HE MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"HE MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"HE MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"HE MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"EHT MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"EHT MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"EHT MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"EHT MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"EHT MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"EHT MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"EHT MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"EHT MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"EHT MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"EHT MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"EHT MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"EHT MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"EHT MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"EHT MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"EHT MCS 14 (BPSK-DCM 1/2)", STATS_IF_MCS_VALID},
		{"EHT MCS 15 (BPSK-DCM 1/2)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	}
};
#else /* WLAN_FEATURE_11BE */
static const struct stats_if_rate_debug rate_string[STATS_IF_DOT11_MAX]
						   [STATS_IF_MAX_MCS] = {
	{
		{"OFDM 48 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 24 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 12 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 6 Mbps ", STATS_IF_MCS_VALID},
		{"OFDM 54 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 36 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 18 Mbps", STATS_IF_MCS_VALID},
		{"OFDM 9 Mbps ", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"CCK 11 Mbps Long  ", STATS_IF_MCS_VALID},
		{"CCK 5.5 Mbps Long ", STATS_IF_MCS_VALID},
		{"CCK 2 Mbps Long   ", STATS_IF_MCS_VALID},
		{"CCK 1 Mbps Long   ", STATS_IF_MCS_VALID},
		{"CCK 11 Mbps Short ", STATS_IF_MCS_VALID},
		{"CCK 5.5 Mbps Short", STATS_IF_MCS_VALID},
		{"CCK 2 Mbps Short  ", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"HT MCS 0 (BPSK 1/2)  ", STATS_IF_MCS_VALID},
		{"HT MCS 1 (QPSK 1/2)  ", STATS_IF_MCS_VALID},
		{"HT MCS 2 (QPSK 3/4)  ", STATS_IF_MCS_VALID},
		{"HT MCS 3 (16-QAM 1/2)", STATS_IF_MCS_VALID},
		{"HT MCS 4 (16-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HT MCS 5 (64-QAM 2/3)", STATS_IF_MCS_VALID},
		{"HT MCS 6 (64-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HT MCS 7 (64-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"VHT MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"VHT MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"VHT MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"VHT MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"VHT MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"VHT MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"VHT MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"VHT MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"HE MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"HE MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"HE MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"HE MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"HE MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"HE MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"HE MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"HE MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	}
};
#endif /* WLAN_FEATURE_11BE */
#endif /* defined(WLAN_DEBUG_TELEMETRY) || defined(WLAN_ADVANCE_TELEMETRY) */

#if WLAN_DEBUG_TELEMETRY
#ifdef WLAN_FEATURE_11BE
static const struct stats_if_rate_debug
mu_rate_string[STATS_IF_TXRX_TYPE_MU_MAX][STATS_IF_MAX_MCS] = {
	{
		{"HE MU-MIMO MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"HE OFDMA MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	}
};
#else /* WLAN_FEATURE_11BE */
static const struct stats_if_rate_debug
mu_rate_string[STATS_IF_TXRX_TYPE_MU_MAX][STATS_IF_MAX_MCS] = {
	{
		{"HE MU-MIMO MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE MU-MIMO MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"HE OFDMA MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"HE OFDMA MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	}
};
#endif /* WLAN_FEATURE_11BE */
#endif /* WLAN_DEBUG_TELEMETRY */

#if WLAN_ADVANCE_TELEMETRY
#ifdef WLAN_FEATURE_11BE
static const struct stats_if_rate_debug
mu_be_rate_string[STATS_IF_TXRX_TYPE_MU_MAX][STATS_IF_MAX_MCS] = {
	{
		{"EHT MU-MIMO MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 14 (BPSK-DCM 1/2)", STATS_IF_MCS_VALID},
		{"EHT MU-MIMO MCS 15 (BPSK-DCM 1/2)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	},
	{
		{"EHT OFDMA MCS 0 (BPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 1 (QPSK 1/2)     ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 2 (QPSK 3/4)     ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 3 (16-QAM 1/2)   ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 4 (16-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 5 (64-QAM 2/3)   ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 6 (64-QAM 3/4)   ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 7 (64-QAM 5/6)   ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 8 (256-QAM 3/4)  ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 9 (256-QAM 5/6)  ", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 10 (1024-QAM 3/4)", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 11 (1024-QAM 5/6)", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 12 (4096-QAM 3/4)", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 13 (4096-QAM 5/6)", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 14 (BPSK-DCM 1/2)", STATS_IF_MCS_VALID},
		{"EHT OFDMA MCS 15 (BPSK-DCM 1/2)", STATS_IF_MCS_VALID},
		{"INVALID ", STATS_IF_MCS_INVALID},
	}
};
#endif /* WLAN_FEATURE_11BE */
#endif /* WLAN_ADVANCE_TELEMETRY */

#if WLAN_DEBUG_TELEMETRY
#ifdef WLAN_FEATURE_11BE
static const struct stats_if_ru_debug ru_string[STATS_IF_RU_INDEX_MAX] = {
	{ "RU_26" },
	{ "RU_52" },
	{ "RU_52_26" },
	{ "RU_106" },
	{ "RU_106_26" },
	{ "RU_242" },
	{ "RU_484" },
	{ "RU_484_242" },
	{ "RU_996" },
	{ "RU_996_484" },
	{ "RU_996_484_242" },
	{ "RU_2x996" },
	{ "RU_2x996_484" },
	{ "RU_3x996" },
	{ "RU_3x996_484" },
	{ "RU_4x996" },
};
#else
static const struct stats_if_ru_debug ru_string[STATS_IF_RU_INDEX_MAX] = {
	{ "RU_26" },
	{ "RU_52" },
	{ "RU_106" },
	{ "RU_242" },
	{ "RU_484" },
	{ "RU_996" }
};
#endif

static const char *proto_subtype_string[STATS_IF_PROTO_SUBTYPE_MAX] = {
	"INVALID",
	"EAPOL_M1",
	"EAPOL_M2",
	"EAPOL_M3",
	"EAPOL_M4",
	"DHCP_DISCOVER",
	"DHCP_REQUEST",
	"DHCP_OFFER",
	"DHCP_ACK",
	"DHCP_NACK",
	"DHCP_RELEASE",
	"DHCP_INFORM",
	"DHCP_DECLINE",
	"ARP_REQ",
	"ARP_RES",
	"ICMP_REQ",
	"ICMP_RES",
	"ICMPV6_REQ",
	"ICMPV6_RES",
	"ICMPV6_RS",
	"ICMPV6_RA",
	"ICMPV6_NS",
	"ICMPV6_NA",
	"IPV4_UDP",
	"IPV4_TCP",
	"IPV6_UDP",
	"IPV6_TCP",
	"MGMT_ASSOC",
	"MGMT_DISASSOC",
	"MGMT_AUTH",
	"MGMT_DEAUTH",
	"ROAM_SYNCH",
	"ROAM_COMPLETE",
	"ROAM_EVENTID",
	"DNS_QUERY",
	"DNS_RES",
};

const char *mu_reception_mode[STATS_IF_TXRX_TYPE_MU_MAX] = {
	"MU MIMO", "MU OFDMA"
};
#endif /* WLAN_DEBUG_TELEMETRY */

static const char *opt_string = "BADarvsdcf:i:l:m:t:RIMh?";

static const struct option long_opts[] = {
	{ "basic", no_argument, NULL, 'B' },
	{ "advance", no_argument, NULL, 'A' },
	{ "debug", no_argument, NULL, 'D' },
	{ "ap", no_argument, NULL, 'a' },
	{ "radio", no_argument, NULL, 'r' },
	{ "vap", no_argument, NULL, 'v' },
	{ "sta", no_argument, NULL, 's' },
	{ "data", no_argument, NULL, 'd' },
	{ "ctrl", no_argument, NULL, 'c' },
	{ "feature", required_argument, NULL, 'f' },
	{ "ifname", required_argument, NULL, 'i' },
	{ "linkpeer", required_argument, NULL, 'l'},
	{ "stamacaddr", required_argument, NULL, 'm' },
	{ "serviceid", required_argument, NULL, 't' },
	{ "recursive", no_argument, NULL, 'R' },
	{ "ipa", no_argument, NULL, 'I' },
	{ "mldlink", no_argument, NULL, 'M' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, no_argument, NULL, 0 },
};

#ifndef WLAN_CONFIG_TX_DELAY
const char *stats_if_fw_to_hw_delay_bucket[STATS_IF_DELAY_BUCKET_MAX + 1] = {
	"0 to 10 ms", "11 to 20 ms",
	"21 to 30 ms", "31 to 40 ms",
	"41 to 50 ms", "51 to 60 ms",
	"61 to 70 ms", "71 to 80 ms",
	"81 to 90 ms", "91 to 100 ms",
	"101 to 250 ms", "251 to 500 ms", "500+ ms"
};

const char *stats_if_sw_enq_delay_bucket[STATS_IF_DELAY_BUCKET_MAX + 1] = {
	"0 to 1 ms", "1 to 2 ms",
	"2 to 3 ms", "3 to 4 ms",
	"4 to 5 ms", "5 to 6 ms",
	"6 to 7 ms", "7 to 8 ms",
	"8 to 9 ms", "9 to 10 ms",
	"10 to 11 ms", "11 to 12 ms", "12+ ms"
};
#else
const char *stats_if_fw_to_hw_delay_bucket[STATS_IF_DELAY_BUCKET_MAX + 1] = {
	"0 to 250 us", "250 to 500 us",
	"500 to 750 us", "750 to 1000 us",
	"1000 to 1500 us", "1500 to 2000 us",
	"2000 to 2500 us", "2500 to 5000 us",
	"5000 to 6000 us", "6000 to 7000 us",
	"7000 to 8000 us", "8000 to 9000 us", "9000+ us"
};

const char *stats_if_sw_enq_delay_bucket[STATS_IF_DELAY_BUCKET_MAX + 1] = {
	"0 to 250 us", "250 to 500 us",
	"500 to 750 us", "750 to 1000 us",
	"1000 to 1500 us", "1500 to 2000 us",
	"2000 to 2500 us", "2500 to 5000 us",
	"5000 to 6000 us", "6000 to 7000 us",
	"7000 to 8000 us", "8000 to 9000 us", "9000+ us"
};
#endif

const char *stats_if_intfrm_delay_bucket[STATS_IF_DELAY_BUCKET_MAX + 1] = {
	"0 to 5 ms", "6 to 10 ms",
	"11 to 15 ms", "16 to 20 ms",
	"21 to 25 ms", "26 to 30 ms",
	"31 to 35 ms", "36 to 40 ms",
	"41 to 45 ms", "46 to 50 ms",
	"51 to 55 ms", "56 to 60 ms", "60+ ms"
};

const char *stats_if_hw_tx_comp_delay_bucket[STATS_IF_DELAY_BUCKET_MAX + 1] = {
	"0 to 250 us", "250 to 500 us",
	"500 to 750 us", "750 to 1000 us",
	"1000 to 1500 us", "1500 to 2000 us",
	"2000 to 2500 us", "2500 to 5000 us",
	"5000 to 6000 us", "6000 to 7000 us",
	"7000 to 8000 us", "8000 to 9000 us", "9000+ us"
};

static void display_help(void)
{
	STATS_PRINT("\nwifitelemetry : Displays Statistics of Access Point\n");
	STATS_PRINT("\nUsage:\n"
		    "Process Mode: wifitelemetry [Level] [Object] [StatsType] [FeatureName] [[-i interface_name] | [-m StationMACAddress]] [-R] [-I] [-M] [-h | ?]\n"
		    "Daemon Mode: wifitelemetry async\n"
		    "    Note: User must run wifitelemetry in background. Excecute another instance in process mode to trigger stats request.\n"
		    "\n"
		    "One of the 3 Levels can be selected. The Levels are as follows (catagorised based on purpose or\n"
		    "the level of details):\n"
		    "Basic, Advance and Debug. The default is Basic where it provides very minimal stats.\n"
		    "\n"
		    "One of the 4 Objects can be selected. The objects are as follows (from top to bottom of the hierarchy):\n"
		    "Entire Access Point, Radio, Vap and Station (STA). The default is Entire Access Point.\n"
		    "Stats for sub-levels below the selected Object can be displayed by choosing -R (recursive) option (not\n"
		    "applicable for the STA Object, where information for a single STA in displayed).\n"
		    "\n"
		    "StatsType specifies the stats category. Catagories are:\n"
		    "Control and Data. Default is Data category.\n"
		    "\n"
		    "Feature Name specifies various specific feature for which Stats is to be displayed. By default ALL will\n"
		    "be selected. List of feature flags are as follows:\n"
		    "ALL,RX,TX,AST,CFR,FWD,HTT,RAW,EXT,TSO,TWT,VOW,WDI,WMI,IGMP,LINK,MESH,RATE,DELAY,JITTER,ME,NAWDS,TXCAP,MONITOR,SAWFDELAY and SAWFTX.\n"
		    "\n"
		    "Levels:\n"
		    "    -B or --basic:   Only Basic level stats are displyed. This is the default level\n"
		    "    -A or --advance: Advance level stats are displayed.\n"
		    "    -D or --debug:   Debug level stats are displayed.\n"
		    "\n"
		    "Objects:\n"
		    "    -a or --ap:      Entire Access Point stats are displayed. No need to specify interface and STA MAC.\n"
		    "    -r or --radio:   Radio object stats are displayed. Radio interface Name needs to be provided.\n"
		    "    -v or --vap:     Vap object stats are displayed. Vap interface Name need to be provided.\n"
		    "    -s or --sta:     STA object stats are displayed. STA MAC Address need to be provided.\n"
		    "\n"
		    "Feature Names:\n"
		    "    -f <flag1[,flag2,...,flagN]> or --feature=<flag1[,flag2,...,flagN]>\n"
		    "        Feature flag carries the flags for requested features. Multiple flags can be added with ',' separation.\n"
		    "\n"
		    "Types:\n"
		    "    -d or --data:   Stats for data is displayed\n"
		    "    -c or --ctrl:   Stats for control is displayed\n"
		    "\n"
		    "Interface:\n"
		    "    -i wifiX or --ifname=wifiX:  For Radio\n"
		    "    -i athX or --ifname=athX:    For VAP\n"
		    "\n"
		    "STA MAC Address for STA stats:\n"
		    "    -m xx:xx:xx:xx:xx:xx or --stamacaddr xx:xx:xx:xx:xx:xx\n"
		    "\n"
		    "Service ID set only in STA mode for SAWFDELAY and SAWFTX features:\n"
		    "    -t <serviceid>\n"
		    "\n"
		    "OTHER OPTIONS:\n"
		    "    -R or --recursive:  Recursive display\n"
		    "    -I or --ipa is required to get Tx and Rx stats in IPA architecture\n"
		    "    -M or --mldlink indicates to get link stats for MLD\n"
		    "    -h or --help:       Usage display\n");
}

static char *macaddr_to_str(u_int8_t *addr)
{
	static char string[3 * ETHER_ADDR_LEN];

	memset(string, 0, sizeof(string));
	if (addr) {
		snprintf(string, sizeof(string),
			 "%02x:%02x:%02x:%02x:%02x:%02x",
			 addr[0], addr[1], addr[2],
			 addr[3], addr[4], addr[5]);
	}

	return string;
}

void print_basic_data_tx_stats(struct basic_data_tx_stats *tx)
{
	STATS_64(stdout, "Tx Success", tx->tx_success.num);
	STATS_64(stdout, "Tx Success Bytes", tx->tx_success.bytes);

	if (!g_ipa_mode) {
		STATS_64(stdout, "Tx Complete", tx->comp_pkt.num);
		STATS_64(stdout, "Tx Complete Bytes", tx->comp_pkt.bytes);
		STATS_64(stdout, "Tx Failed", tx->tx_failed);
		STATS_64(stdout, "Tx Dropped Count", tx->dropped_count);
	}
}

void print_basic_data_rx_stats(struct basic_data_rx_stats *rx)
{
	STATS_64(stdout, "Rx Total Packets", rx->total_rcvd.num);
	STATS_64(stdout, "Rx Total Bytes", rx->total_rcvd.bytes);
	STATS_64(stdout, "Rx Error Count", rx->rx_error_count);

	if (!g_ipa_mode) {
		STATS_64(stdout, "Rx To Stack", rx->to_stack.num);
		STATS_64(stdout, "Rx To Stack Bytes", rx->to_stack.bytes);
	}
}

void print_basic_sta_data_tx(struct basic_peer_data_tx *tx)
{
	print_basic_data_tx_stats(&tx->tx);
}

void print_basic_sta_data_rx(struct basic_peer_data_rx *rx)
{
	print_basic_data_rx_stats(&rx->rx);
}

void print_basic_sta_data_link(struct basic_peer_data_link *link)
{
	STATS_8(stdout, "SNR", link->snr);
	STATS_8(stdout, "Last SNR", link->last_snr);
	STATS_32(stdout, "Avrage SNR", link->avg_snr);
}

void print_basic_sta_data_rate(struct basic_peer_data_rate *rate)
{
	STATS_32(stdout, "Rx Rate", rate->rx_rate);
	STATS_32(stdout, "Avg Rx Rate", rate->last_rx_rate);
	STATS_32(stdout, "Tx Rate", rate->tx_rate);
	STATS_32(stdout, "Avg Tx Rate", rate->last_tx_rate);
}

void print_basic_sta_ctrl_tx(struct basic_peer_ctrl_tx *tx)
{
	STATS_32(stdout, "Tx Management", tx->cs_tx_mgmt);
	STATS_32(stdout, "Tx Not Ok", tx->cs_is_tx_not_ok);
}

void print_basic_sta_ctrl_rx(struct basic_peer_ctrl_rx *rx)
{
	STATS_32(stdout, "Rx Management", rx->cs_rx_mgmt);
	STATS_32(stdout, "Rx Decrypt Crc", rx->cs_rx_decryptcrc);
	STATS_32(stdout, "Rx Security Failure", rx->cs_rx_security_failure);
}

void print_basic_sta_ctrl_link(struct basic_peer_ctrl_link *link)
{
	STATS_8(stdout, "Rx Management SNR", link->cs_rx_mgmt_snr);
}

void print_basic_sta_ctrl_rate(struct basic_peer_ctrl_rate *rate)
{
	STATS_32(stdout, "Rx Management Rate", rate->cs_rx_mgmt_rate);
}

void print_basic_vap_data_tx(struct basic_vdev_data_tx *tx)
{
	struct pkt_info *pkt = NULL;

	print_basic_data_tx_stats(&tx->tx);

	if (!g_ipa_mode) {
		pkt = &tx->ingress;
		STATS_64(stdout, "Tx Ingress Received", pkt->num);
		STATS_64(stdout, "Tx Ingress Received Bytes", pkt->bytes);
		pkt = &tx->processed;
		STATS_64(stdout, "Tx Ingress Processed", pkt->num);
		STATS_64(stdout, "Tx Ingress Processed Bytes", pkt->bytes);
		pkt = &tx->dropped;
		STATS_64(stdout, "Tx Ingress Dropped", pkt->num);
		STATS_64(stdout, "Tx Ingress Dropped Bytes", pkt->bytes);
	}
}

void print_basic_vap_data_rx(struct basic_vdev_data_rx *rx)
{
	print_basic_data_rx_stats(&rx->rx);
}

void print_basic_vap_ctrl_tx(struct basic_vdev_ctrl_tx *tx)
{
	STATS_64(stdout, "Tx Management", tx->cs_tx_mgmt);
	STATS_64(stdout, "Tx Error Count", tx->cs_tx_error_counter);
	STATS_64(stdout, "Tx Discard", tx->cs_tx_discard);
}

void print_basic_vap_ctrl_rx(struct basic_vdev_ctrl_rx *rx)
{
	STATS_64(stdout, "Rx Management", rx->cs_rx_mgmt);
	STATS_64(stdout, "Rx Error Count", rx->cs_rx_error_counter);
	STATS_64(stdout, "Rx Management Discard", rx->cs_rx_mgmt_discard);
	STATS_64(stdout, "Rx Control", rx->cs_rx_ctl);
	STATS_64(stdout, "Rx Discard", rx->cs_rx_discard);
	STATS_64(stdout, "Rx Security Failure", rx->cs_rx_security_failure);
}

void print_basic_vap_ctrl_link(struct basic_vdev_ctrl_link *link)
{
	STATS_16(stdout, "Number of connected clients", link->cs_peer_count);
}

void print_basic_radio_data_tx(struct basic_pdev_data_tx *tx)
{
	struct pkt_info *pkt = NULL;

	print_basic_data_tx_stats(&tx->tx);

	if (!g_ipa_mode) {
		pkt = &tx->ingress;
		STATS_64(stdout, "Tx Ingress Received", pkt->num);
		STATS_64(stdout, "Tx Ingress Received Bytes", pkt->bytes);
		pkt = &tx->processed;
		STATS_64(stdout, "Tx Ingress Processed", pkt->num);
		STATS_64(stdout, "Tx Ingress Processed Bytes", pkt->bytes);
		pkt = &tx->dropped;
		STATS_64(stdout, "Tx Ingress Dropped", pkt->num);
		STATS_64(stdout, "Tx Ingress Dropped Bytes", pkt->bytes);
	}
}

void print_basic_radio_data_rx(struct basic_pdev_data_rx *rx)
{
	print_basic_data_rx_stats(&rx->rx);

	if (!g_ipa_mode) {
		STATS_64(stdout, "Dropped Count", rx->dropped_count);
		STATS_64(stdout, "Error Count", rx->err_count);
	}
}

void print_basic_radio_ctrl_tx(struct basic_pdev_ctrl_tx *tx)
{
	STATS_32(stdout, "Lithium_cycle_counts: Tx Frame Count",
		 tx->cs_tx_frame_count);
	STATS_32(stdout, "Tx Management", tx->cs_tx_mgmt);
}

void print_basic_radio_ctrl_rx(struct basic_pdev_ctrl_rx *rx)
{
	STATS_32(stdout, "Lithium_cycle_counts: Rx Frame Count",
		 rx->cs_rx_frame_count);
	STATS_32(stdout, "Rx Management", rx->cs_rx_mgmt);
	STATS_32(stdout, "Rx Number of Mnagement", rx->cs_rx_num_mgmt);
	STATS_32(stdout, "Rx Number of Control", rx->cs_rx_num_ctl);
	STATS_32(stdout, "Rx Error Sum", rx->cs_rx_error_sum);
}

void print_basic_radio_ctrl_link(struct basic_pdev_ctrl_link *link)
{
	STATS_32(stdout, "Channel Tx Power", link->cs_chan_tx_pwr);
	STATS_16_SIGNED(stdout, "Channel NF", link->cs_chan_nf);
	STATS_16_SIGNED(stdout, "Channel NF Sec80", link->cs_chan_nf_sec80);
	STATS_16(stdout, "Number of connected clients", link->cs_peer_count);
}

void print_basic_ap_data_tx(struct basic_psoc_data_tx *tx)
{
	STATS_64(stdout, "Tx Egress Pkts", tx->egress.num);
	STATS_64(stdout, "Tx Egress Bytes", tx->egress.bytes);
}

void print_basic_ap_data_rx(struct basic_psoc_data_rx *rx)
{
	STATS_64(stdout, "Rx Ingress Pkts", rx->ingress.num);
	STATS_64(stdout, "Rx Ingress Bytes", rx->ingress.bytes);
}

#if WLAN_ADVANCE_TELEMETRY
#if WLAN_FEATURE_11BE
void print_advance_data_be_punc_stats(u_int32_t *punc_cnt_array)
{
	STATS_PRINT("NO_PUNC %d 20MHz %d 40MHz %d 80MHz %d 120MHz %d\n",
		    punc_cnt_array[STATS_IF_NO_PUNCTURE],
		    punc_cnt_array[STATS_IF_PUNCTURED_20MHZ],
		    punc_cnt_array[STATS_IF_PUNCTURED_40MHZ],
		    punc_cnt_array[STATS_IF_PUNCTURED_80MHZ],
		    punc_cnt_array[STATS_IF_PUNCTURED_120MHZ]);
}

void print_advance_data_su_ppdu_be_stats(struct pkt_type *su_be_ppdu_cnt)
{
	uint8_t mcs;

	for (mcs = 0; mcs < STATS_IF_MAX_MCS; mcs++) {
		if (!rate_string[STATS_IF_DOT11_BE][mcs].valid)
			continue;
		STATS_PRINT("\t\t%s = %d\n",
			    rate_string[STATS_IF_DOT11_BE][mcs].mcs_type,
			    su_be_ppdu_cnt->mcs_count[mcs]);
	}
}

void print_advance_data_mu_ppdu_be_stats(struct pkt_type *mu_ppdu_array)
{
	uint8_t mcs, pkt_type;

	for (pkt_type = 0; pkt_type < STATS_IF_TXRX_TYPE_MU_MAX; pkt_type++) {
		for (mcs = 0; mcs < STATS_IF_MAX_MCS; mcs++) {
			if (!mu_be_rate_string[pkt_type][mcs].valid)
				continue;
			STATS_PRINT("\t\t%s = %d\n",
				    mu_be_rate_string[pkt_type][mcs].mcs_type,
				    mu_ppdu_array[pkt_type].mcs_count[mcs]);
		}
	}
}

#define print_advance_data_be_stats(src, str)                                  \
do {                                                                           \
	STATS_PRINT("\t" str " Punctured BW Counts = ");                       \
	print_advance_data_be_punc_stats(&src->punc_bw[0]);                    \
	STATS_PRINT("\t" str " SU PPDU BE Counts\n");                          \
	print_advance_data_su_ppdu_be_stats(&src->su_be_ppdu_cnt);             \
	STATS_PRINT("\t" str " MU PPDU BE Counts\n");                          \
	print_advance_data_mu_ppdu_be_stats(&src->mu_be_ppdu_cnt[0]);          \
} while (0)
#else
#define print_advance_data_be_stats(src, str) {}
#endif /* WLAN_FEATURE_11BE */

void print_advance_data_tx_stats(struct advance_data_tx_stats *tx)
{
	u_int8_t inx = 0;

	STATS_64(stdout, "Tx Unicast Packets", tx->ucast.num);
	STATS_64(stdout, "Tx Unicast Bytes", tx->ucast.bytes);
	STATS_64(stdout, "Tx Multicast Packets", tx->mcast.num);
	STATS_64(stdout, "Tx Multicast Bytes", tx->mcast.bytes);
	STATS_64(stdout, "Tx Broadcast Packets", tx->bcast.num);
	STATS_64(stdout, "Tx Broadcast Bytes", tx->bcast.bytes);
	STATS_PRINT("\tTx SGI = 0.8us %u, 0.4us %u, 1.6us %u, 3.2us %u\n",
		    tx->sgi_count[0], tx->sgi_count[1],
		    tx->sgi_count[2], tx->sgi_count[3]);
	STATS_PRINT("\tTx NSS (1-%d)\n\t", STATS_IF_SS_COUNT);
	for (inx = 0; inx < STATS_IF_SS_COUNT; inx++)
		STATS_PRINT(" %u = %u ", (inx + 1), tx->nss[inx]);
	STATS_PRINT("\n\tTx BW Counts = 20MHZ %u 40MHZ %u 80MHZ %u 160MHZ %u\n",
		    tx->bw[0], tx->bw[1], tx->bw[2], tx->bw[3]);
	STATS_32(stdout, "Tx Retries", tx->retries);
	STATS_PRINT("\tTx Aggregation:\n");
	STATS_32(stdout, "MSDU's Part of AMPDU", tx->ampdu_cnt);
	STATS_32(stdout, "MSDU's With No MPDU Level Aggregation",
		 tx->non_ampdu_cnt);
	STATS_PRINT("\tTx Data Packets per AC\n");
	STATS_32(stdout, "     Best effort",
		 tx->wme_ac_type[STATS_IF_WME_AC_BE]);
	STATS_32(stdout, "      Background",
		 tx->wme_ac_type[STATS_IF_WME_AC_BK]);
	STATS_32(stdout, "           Video",
		 tx->wme_ac_type[STATS_IF_WME_AC_VI]);
	STATS_32(stdout, "           Voice",
		 tx->wme_ac_type[STATS_IF_WME_AC_VO]);

	STATS_PRINT("\tTx  Data Bytes per AC\n");
	STATS_64(stdout, "     Best effort",
		 tx->wme_ac_type_bytes[STATS_IF_WME_AC_BE]);
	STATS_64(stdout, "      Background",
		 tx->wme_ac_type_bytes[STATS_IF_WME_AC_BK]);
	STATS_64(stdout, "           Video",
		 tx->wme_ac_type_bytes[STATS_IF_WME_AC_VI]);
	STATS_64(stdout, "           Voice",
		 tx->wme_ac_type_bytes[STATS_IF_WME_AC_VO]);

	if (!g_ipa_mode) {
		STATS_32(stdout, "Tx PER", tx->per);
		STATS_32(stdout, "Tx Rate", tx->tx_rate);
		STATS_32(stdout, "MSDU's Part of AMSDU", tx->amsdu_cnt);
		STATS_32(stdout, "MSDU's With No MSDU Level Aggregation",
			 tx->non_amsdu_cnt);
		print_advance_data_be_stats(tx, "Tx");
	}
}

void print_advance_data_rx_stats(struct advance_data_rx_stats *rx)
{
	u_int8_t inx = 0;

	STATS_64(stdout, "Rx Unicast Packets", rx->unicast.num);
	STATS_64(stdout, "Rx Unicast Bytes", rx->unicast.bytes);
	STATS_64(stdout, "Rx Multicast Packets", rx->multicast.num);
	STATS_64(stdout, "Rx Multicast Bytes", rx->multicast.bytes);
	STATS_64(stdout, "Rx Broadcast Packets", rx->bcast.num);
	STATS_64(stdout, "Rx Broadcast Bytes", rx->bcast.bytes);
	STATS_32(stdout, "Rx MPDU FCS Ok Count", rx->mpdu_cnt_fcs_ok);
	STATS_32(stdout, "Rx MPDU FCS Error Count", rx->mpdu_cnt_fcs_err);
	STATS_PRINT("\tRx PPDU Counts\n");
	for (inx = 0; inx < STATS_IF_MAX_MCS; inx++) {
		if (!rate_string[STATS_IF_DOT11_AX][inx].valid)
			continue;
		STATS_PRINT("\t\t%s = %u\n",
			    rate_string[STATS_IF_DOT11_AX][inx].mcs_type,
			    rx->su_ax_ppdu_cnt[inx]);
	}
	STATS_PRINT("\tRx SGI = 0.8us %u, 0.4us %u, 1.6us %u, 3.2us %u\n",
		    rx->sgi_count[0], rx->sgi_count[1],
		    rx->sgi_count[2], rx->sgi_count[3]);
	STATS_PRINT("\tRx MSDU Counts for NSS (1-%u)\n\t", STATS_IF_SS_COUNT);
	for (inx = 0; inx < STATS_IF_SS_COUNT; inx++)
		STATS_PRINT(" %u = %u ", (inx + 1), rx->nss[inx]);
	STATS_PRINT("\n\tRx PPDU Counts for NSS (1-%u) in SU mode\n\t",
		    STATS_IF_SS_COUNT);
	for (inx = 0; inx < STATS_IF_SS_COUNT; inx++)
		STATS_PRINT(" %u = %u ", (inx + 1), rx->ppdu_nss[inx]);
	STATS_PRINT("\n\tRx BW Counts = 20MHZ %u 40MHZ %u 80MHZ %u 160MHZ %u\n",
		    rx->bw[0], rx->bw[1], rx->bw[2], rx->bw[3]);
	STATS_PRINT("\tRx Data Packets per AC\n");
	STATS_32(stdout, "     Best effort",
		 rx->wme_ac_type[STATS_IF_WME_AC_BE]);
	STATS_32(stdout, "      Background",
		 rx->wme_ac_type[STATS_IF_WME_AC_BK]);
	STATS_32(stdout, "           Video",
		 rx->wme_ac_type[STATS_IF_WME_AC_VI]);
	STATS_32(stdout, "           Voice",
		 rx->wme_ac_type[STATS_IF_WME_AC_VO]);

	STATS_PRINT("\tRx Data Bytes per AC\n");
	STATS_64(stdout, "     Best effort",
		 rx->wme_ac_type_bytes[STATS_IF_WME_AC_BE]);
	STATS_64(stdout, "      Background",
		 rx->wme_ac_type_bytes[STATS_IF_WME_AC_BK]);
	STATS_64(stdout, "           Video",
		 rx->wme_ac_type_bytes[STATS_IF_WME_AC_VI]);
	STATS_64(stdout, "           Voice",
		 rx->wme_ac_type_bytes[STATS_IF_WME_AC_VO]);
	STATS_PRINT("\tRx Aggregation:\n");
	STATS_32(stdout, "MSDU's Part of AMPDU", rx->ampdu_cnt);
	STATS_32(stdout, "MSDU's With No MPDU Level Aggregation",
		 rx->non_ampdu_cnt);
	if (!g_ipa_mode) {
		STATS_32(stdout, "MSDU's Part of AMSDU", rx->amsdu_cnt);
		STATS_32(stdout, "MSDU's With No MSDU Level Aggregation",
			 rx->non_amsdu_cnt);
		STATS_32(stdout, "Rx Multipass Packet Drop",
			 rx->multipass_rx_pkt_drop);
		STATS_32(stdout, "Rx BAR Reaceive Count", rx->bar_recv_cnt);
		STATS_32(stdout, "Rx Retries", rx->rx_retries);
		print_advance_data_be_stats(rx, "Rx");
	}
}

void print_advance_sta_data_tx(struct advance_peer_data_tx *tx)
{
	print_basic_sta_data_tx(&tx->b_tx);
	print_advance_data_tx_stats(&tx->adv_tx);
}

void print_advance_sta_data_rx(struct advance_peer_data_rx *rx)
{
	print_basic_sta_data_rx(&rx->b_rx);
	print_advance_data_rx_stats(&rx->adv_rx);
}

void print_advance_sta_data_fwd(struct advance_peer_data_fwd *fwd)
{
	STATS_64(stdout, "Intra BSS Packets", fwd->pkts.num);
	STATS_64(stdout, "Intra BSS Bytes", fwd->pkts.bytes);
	STATS_64(stdout, "Intra BSS Fail Packets", fwd->fail.num);
	STATS_64(stdout, "Intra BSS Fail Bytes", fwd->fail.bytes);
	STATS_32(stdout, "Intra BSS MDNS No FWD", fwd->mdns_no_fwd);
}

void print_advance_sta_data_raw(struct advance_peer_data_raw *raw)
{
	STATS_64(stdout, "Raw Packets", raw->raw.num);
	STATS_64(stdout, "Raw Bytes", raw->raw.bytes);
}

void print_sta_rdk_rx_rate_stats(struct advance_peer_data_rdk *rdk)
{
	struct stats_if_rdk_rx_rate_stats *rx_rate;
	uint8_t i, chain, bw;

	STATS_PRINT("======================================================\n");
	STATS_PRINT("\tPEER Cookie    = %016"PRIx64"\n", rdk->peer_cookie);
	STATS_PRINT("\n\t%10s | %10s | %10s | %10s | %10s | %10s|",
		    "rate", "rix", "bytes", "msdus", "mpdus", "ppdus");
	STATS_PRINT(" %10s | %10s | %10s\n", "retries", "sgi", "rssi");
	for (i = 0; i < STATS_IF_CACHE_SIZE; i++) {
		rx_rate = &rdk->rx_rate[i];
		if (rx_rate->ratecode == STATS_IF_INVALID_CACHE_IDX)
			continue;

		STATS_PRINT("\t%10u | %10u | %10u | %10u | %10u | %10u |",
			    rx_rate->rate,
			    STATS_IF_GET_PEER_RIX(rx_rate->ratecode),
			    rx_rate->num_bytes, rx_rate->num_msdus,
			    rx_rate->num_mpdus, rx_rate->num_ppdus);
		STATS_PRINT(" %10u | %10u | %10lu\n", rx_rate->num_retries,
			    rx_rate->num_sgi, rx_rate->avg_rssi);
	}

	STATS_PRINT("\n\t%10s | %10s | %10s | %10s | %10s |", "rate",
		    "rssi 1 p20", "rssi 1 e20", "rssi 1 e40 low20",
		    "rssi 1 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 1 ext80 low20",
		    "rssi 1 ext80 low_high20", "rssi 1 ext80 high_low20",
		    "rssi 1 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 2 p20",
		    "rssi 2 e20", "rssi 2 e40 low20", "rssi 2 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 2 ext80 low20",
		    "rssi 2 ext80 low_high20", "rssi 2 ext80 high_low20",
		    "rssi 2 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 3 p20",
		    "rssi 3 e20", "rssi 3 e40 low20", "rssi 3 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 3 ext80 low20",
		    "rssi 3 ext80 low_high20", "rssi 3 ext80 high_low20",
		    "rssi 3 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 4 p20",
		    "rssi 4 e20", "rssi 4 e40 low20", "rssi 4 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 4 ext80 low20",
		    "rssi 4 ext80 low_high20", "rssi 4 ext80 high_low20",
		    "rssi 4 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 5 p20",
		    "rssi 5 e20", "rssi 5 e40 low20", "rssi 5 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 5 ext80 low20",
		    "rssi 5 ext80 low_high20", "rssi 5 ext80 high_low20",
		    "rssi 5 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 6 p20",
		    "rssi 6 e20", "rssi 6 e40 low20", "rssi 6 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 6 ext80 low20",
		    "rssi 6 ext80 low_high20", "rssi 6 ext80 high_low20",
		    "rssi 6 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 7 p20",
		    "rssi 7 e20", "rssi 7 e40 low20", "rssi 7 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n", "rssi 7 ext80 low20",
		    "rssi 7 ext80 low_high20", "rssi 7 ext80 high_low20",
		    "rssi 7 ext80 high20");
	STATS_PRINT("\t\t     %10s | %10s | %10s | %10s |", "rssi 8 p20",
		    "rssi 8 e20", "rssi 8 e40 low20", "rssi 8 e40 high20");
	STATS_PRINT(" %10s | %10s | %10s | %10s\n\n", "rssi 8 ext80 low20",
		    "rssi 8 ext80 low_high20", "rssi 8 ext80 high_low20",
		    "rssi 8 ext80 high20");
	for (i = 0; i < STATS_IF_CACHE_SIZE; i++) {
		rx_rate = &rdk->rx_rate[i];
		if (rx_rate->ratecode == STATS_IF_INVALID_CACHE_IDX)
			continue;

		STATS_PRINT("\t%10u", rx_rate->rate);
		for (chain = 0; chain < STATS_IF_MAX_CHAIN; chain++) {
			for (bw = 0; bw < STATS_IF_MAX_BW; bw++)
				STATS_PRINT(" | %10lu",
					    rx_rate->avg_rssi_ant[chain][bw]);
			STATS_PRINT("\n\t\t  ");
		}
		STATS_PRINT("\n");
	}
	STATS_PRINT("======================================================\n");
}

void print_sta_rdk_tx_sojourn_stats(struct advance_peer_data_rdk *rdk)
{
	struct stats_if_rdk_tx_sojourn_stats *tx_sojourn = &rdk->tx_sojourn;
	uint8_t tid;

	STATS_PRINT("======================================================\n");
	STATS_PRINT("\n\tSojourn statistics:\n");
	STATS_PRINT("\t%10s %10s %20s %20s\n", "tid", "ave", "sum", "num");
	for (tid = 0; tid < STATS_IF_DATA_TID_MAX; tid++)
		STATS_PRINT("\t%10d %10u %20u %20u\n",
			    tid, tx_sojourn->avg_sojourn_msdu[tid],
			    tx_sojourn->sum_sojourn_msdu[tid],
			    tx_sojourn->num_msdus[tid]);
	STATS_PRINT("\tsizeof(avg) : %10u\n",
		    (uint32_t)sizeof(tx_sojourn->avg_sojourn_msdu[tid]));
	STATS_PRINT("======================================================\n");
}

void print_sta_rdk_tx_rate_stats(struct advance_peer_data_rdk *rdk)
{
	struct stats_if_rdk_tx_rate_stats *tx_rate;
	uint8_t i;

	STATS_PRINT("======================================================\n");
	STATS_PRINT("\tPEER Cookie    = %016"PRIx64"\n", rdk->peer_cookie);
	STATS_PRINT("\t%10s | %10s | %10s | %10s | %10s |", "rate", "rix",
		    "attempts", "success", "ppdus");
	STATS_PRINT(" %10s | %10s | %10s\n", "msdus", "bytes", "retries");
	for (i = 0; i < STATS_IF_CACHE_SIZE; i++) {
		tx_rate = &rdk->tx_rate[i];
		if (tx_rate->ratecode == STATS_IF_INVALID_CACHE_IDX)
			continue;
		STATS_PRINT("\t%10u | %10u | %10u | %10u | %10u |",
			    tx_rate->rate,
			    STATS_IF_GET_PEER_RIX(tx_rate->ratecode),
			    tx_rate->mpdu_attempts, tx_rate->mpdu_success,
			    tx_rate->num_ppdus);
		STATS_PRINT(" %10u | %10u | %10u\n", tx_rate->num_msdus,
			    tx_rate->num_bytes, tx_rate->num_retries);
	}
	STATS_PRINT("======================================================\n");
	print_sta_rdk_tx_sojourn_stats(rdk);
}

#ifdef WLAN_FEATURE_11BE
void print_sta_11be_tx_link_stats(struct stats_if_rdk_tx_link_stats *tx_link)
{
	STATS_PRINT("\tpunc_bw_usage_packets: NO_PUNCTURE: %u",
		    tx_link->punc_bw.usage_counter[0]);
	STATS_PRINT(" PUNCTURED_20MHZ: %u PUNCTURED_40MHZ: %u",
		    tx_link->punc_bw.usage_counter[1],
		    tx_link->punc_bw.usage_counter[2]);
	STATS_PRINT(" PUNCTURED_80MHZ: %u PUNCTURED_120MHZ: %u\n",
		    tx_link->punc_bw.usage_counter[3],
		    tx_link->punc_bw.usage_counter[4]);
	STATS_32(stdout, "punc_bw_usage_avg (MHz)",
		 tx_link->punc_bw.usage_avg);
	STATS_PRINT("\tpunc_bw_usage_max                   = %u%%\n",
		    tx_link->punc_bw.usage_max);
	STATS_PRINT("\tbw_usage_packets: 20MHz: %u 40MHz: %u",
		    tx_link->bw.usage_counter[0],
			tx_link->bw.usage_counter[1]);
	STATS_PRINT(" 80MHz: %u 160MHz: %u",
		    tx_link->bw.usage_counter[2],
			tx_link->bw.usage_counter[3]);
	STATS_PRINT(" 320MHz: %u\n", tx_link->bw.usage_counter[5]);
}

void print_sta_11be_rx_link_stats(struct stats_if_rdk_rx_link_stats *rx_link)
{
	STATS_PRINT("\tpunc_bw_usage_packets: NO_PUNCTURE: %u",
		    rx_link->punc_bw.usage_counter[0]);
	STATS_PRINT(" PUNCTURED_20MHZ: %u PUNCTURED_40MHZ: %u",
		    rx_link->punc_bw.usage_counter[1],
		    rx_link->punc_bw.usage_counter[2]);
	STATS_PRINT(" PUNCTURED_80MHZ: %u PUNCTURED_120MHZ: %u\n",
		    rx_link->punc_bw.usage_counter[3],
		    rx_link->punc_bw.usage_counter[4]);
	STATS_32(stdout, "punc_bw_usage_avg (MHz)",
		 rx_link->punc_bw.usage_avg);
	STATS_PRINT("\tpunc_bw_usage_max               = %u%%\n",
		    rx_link->punc_bw.usage_max);
	STATS_PRINT("\tbw_usage_packets: 20MHz: %u 40MHz: %u",
		    rx_link->bw.usage_counter[0],
		    rx_link->bw.usage_counter[1]);
	STATS_PRINT(" 80MHz: %u 160MHz: %u",
		    rx_link->bw.usage_counter[2],
		    rx_link->bw.usage_counter[3]);
	STATS_PRINT(" 320MHz: %u\n", rx_link->bw.usage_counter[5]);
}
#else
void print_sta_11be_tx_link_stats(struct stats_if_rdk_tx_link_stats *tx_link)
{ }

void print_sta_11be_rx_link_stats(struct stats_if_rdk_rx_link_stats *rx_link)
{ }
#endif

void print_sta_rdk_tx_link_stats(struct advance_peer_data_rdk *rdk)
{
	struct stats_if_rdk_tx_link_stats *tx_link = &rdk->tx_link;

	STATS_PRINT("======================================================\n");
	STATS_32(stdout, "num_ppdus", tx_link->num_ppdus);
	STATS_64(stdout, "bytes", tx_link->bytes);
	STATS_32(stdout, "mpdu_success", tx_link->mpdu_success);
	STATS_32(stdout, "mpdus_failed", tx_link->mpdu_failed);
	STATS_32(stdout, "phy_rate_actual_su (kbps)",
		 tx_link->phy_rate_actual_su);
	STATS_32(stdout, "phy_rate_actual_mu (kbps)",
		 tx_link->phy_rate_actual_mu);
	STATS_32(stdout, "ofdma_usage", tx_link->ofdma_usage);
	STATS_32(stdout, "mu_mimo_usage", tx_link->mu_mimo_usage);
	STATS_32(stdout, "bw_usage_avg (MHz)", tx_link->bw.usage_avg);
	if (!tx_link->is_lithium) {
	    print_sta_11be_tx_link_stats(tx_link);
	} else {
	    STATS_PRINT("\tbw_usage_packets: 20MHz: %u 40MHz: %u",
			tx_link->bw.usage_counter[0],
			tx_link->bw.usage_counter[1]);
	    STATS_PRINT(" 80MHz: %u 160MHz: %u\n",
			tx_link->bw.usage_counter[2],
			tx_link->bw.usage_counter[3]);
	}
	STATS_PRINT("\tbw_usage_max                   = %u%%\n",
		    tx_link->bw.usage_max);
	STATS_PRINT("\tack_rssi                       = %lu\n",
		    tx_link->ack_rssi);
	STATS_PRINT("\tpkt_error_rate                 = %u%%\n",
		    tx_link->pkt_error_rate);
	STATS_PRINT("======================================================\n");
}

void print_sta_rdk_rx_link_stats(struct advance_peer_data_rdk *rdk)
{
	struct stats_if_rdk_rx_link_stats *rx_link = &rdk->rx_link;

	STATS_PRINT("======================================================\n");
	STATS_32(stdout, "num_ppdus", rx_link->num_ppdus);
	STATS_64(stdout, "bytes", rx_link->bytes);
	STATS_32(stdout, "num_mpdus", rx_link->num_mpdus);
	STATS_32(stdout, "mpdu_retries", rx_link->mpdu_retries);
	STATS_32(stdout, "phy_rate_actual_su (kbps)",
		 rx_link->phy_rate_actual_su);
	STATS_32(stdout, "phy_rate_actual_mu (kbps)",
		 rx_link->phy_rate_actual_mu);
	STATS_32(stdout, "ofdma_usage", rx_link->ofdma_usage);
	STATS_32(stdout, "mu_mimo_usage", rx_link->mu_mimo_usage);
	STATS_32(stdout, "bw_usage_avg (MHz)", rx_link->bw.usage_avg);
	if (!rx_link->is_lithium) {
	    print_sta_11be_rx_link_stats(rx_link);
	} else {
	    STATS_PRINT("\tbw_usage_packets: 20MHz: %u 40MHz: %u",
			rx_link->bw.usage_counter[0],
			rx_link->bw.usage_counter[1]);
	    STATS_PRINT(" 80MHz: %u 160MHz: %u\n", rx_link->bw.usage_counter[2],
			rx_link->bw.usage_counter[3]);
	}
	STATS_PRINT("\tbw_usage_max                   = %u%%\n",
		    rx_link->bw.usage_max);
	STATS_PRINT("\tsu_rssi                        = %lu\n",
		    rx_link->su_rssi);
	STATS_PRINT("\tpkt_error_rate                 = %u%%\n",
		    rx_link->pkt_error_rate);
	STATS_PRINT("======================================================\n");
}

void print_sta_rdk_avg_rate_stats(struct advance_peer_data_rdk *rdk)
{
	struct stats_if_rdk_avg_rate_stats *stats = &rdk->avg_rate;
	enum stats_if_wlan_rate_ppdu_type type;
	char *type2str[STATS_IF_WLAN_RATE_MAX] = {
		"SU",
		"MU-MIMO",
		"MU-OFDMA",
		"MU-MIMO-OFDMA",
	};
	uint32_t mpdu;
	uint32_t psr;
	uint32_t avg_mbps;
	uint32_t avg_snr;

	STATS_PRINT("======================================================\n");
	STATS_PRINT("\tAvg tx stats:\n");
	STATS_PRINT("\t%20s %15s %15s %15s %15s %15s\n", "mode", "rate(mbps)",
		    "total ppdu", "snr value", "snr count", "psr value");
	for (type = 0; type < STATS_IF_WLAN_RATE_MAX; type++) {
		psr = 0;
		avg_mbps = 0;
		avg_snr = 0;
		if (stats->tx[type].num_ppdu > 0)
			avg_mbps = stats->tx[type].sum_mbps /
					stats->tx[type].num_ppdu;
		if (stats->tx[type].num_snr > 0)
			avg_snr = stats->tx[type].sum_snr /
					stats->tx[type].num_snr;
		mpdu = stats->tx[type].num_mpdu +
			stats->tx[type].num_retry;
		if (mpdu > 0)
			psr = (100 * stats->tx[type].num_mpdu) / mpdu;
		STATS_PRINT("\t%20s %15u %15u %15u %15u %15u\n",
			    type2str[type], avg_mbps, stats->tx[type].num_ppdu,
			    avg_snr, stats->tx[type].num_snr, psr);
	}
	STATS_PRINT("\tAvg rx stats:\n");
	STATS_PRINT("\t%20s %15s %15s %15s %15s %15s\n", "mode", "rate(mbps)",
		    "total ppdu", "snr value", "snr count", "psr value");
	for (type = 0; type < STATS_IF_WLAN_RATE_MAX; type++) {
		psr = 0;
		avg_mbps = 0;
		avg_snr = 0;
		if (stats->rx[type].num_ppdu > 0)
			avg_mbps = stats->rx[type].sum_mbps /
					stats->rx[type].num_ppdu;
		if (stats->rx[type].num_snr > 0)
			avg_snr = stats->rx[type].sum_snr /
					stats->rx[type].num_snr;
		mpdu = stats->rx[type].num_mpdu +
			stats->rx[type].num_retry;
		if (mpdu > 0)
			psr = (100 * stats->rx[type].num_mpdu) / mpdu;
		STATS_PRINT("\t%20s %15u %15u %15u %15u %15u\n",
			    type2str[type], avg_mbps, stats->rx[type].num_ppdu,
			    avg_snr, stats->rx[type].num_snr, psr);
	}
	STATS_PRINT("======================================================\n");
}

void print_advance_sta_data_rdk(struct advance_peer_data_rdk *rdk,
				uint8_t *peer_mac, char *pif_name)
{
	switch (rdk->cache_type) {
	case STATS_IF_PEER_RX_RATE_STATS:
		STATS_PRINT("Rx Rate Statistics for Peer %s (Radio %s):\n",
			    macaddr_to_str(peer_mac), pif_name);
		print_sta_rdk_rx_rate_stats(rdk);
		break;
	case STATS_IF_PEER_TX_RATE_STATS:
		STATS_PRINT("Tx Rate Statistics for Peer %s (Radio %s):\n",
			    macaddr_to_str(peer_mac), pif_name);
		print_sta_rdk_tx_rate_stats(rdk);
		break;
	case STATS_IF_PEER_TX_LINK_STATS:
		STATS_PRINT("Tx Link Quality Metrics for Peer %s (Radio %s):\n",
			    macaddr_to_str(peer_mac), pif_name);
		print_sta_rdk_tx_link_stats(rdk);
		break;
	case STATS_IF_PEER_RX_LINK_STATS:
		STATS_PRINT("Rx Link Quality Metrics for Peer %s (Radio %s):\n",
			    macaddr_to_str(peer_mac), pif_name);
		print_sta_rdk_rx_link_stats(rdk);
		break;
	case STATS_IF_PEER_AVG_RATE_STATS:
		STATS_PRINT("Avrage Rate statistics for Peer %s (Radio %s):\n",
			    macaddr_to_str(peer_mac), pif_name);
		print_sta_rdk_avg_rate_stats(rdk);
		break;
	}
}

void print_advance_sta_data_twt(struct advance_peer_data_twt *twt)
{
	STATS_64(stdout, "Rx TWT Packets", twt->to_stack_twt.num);
	STATS_64(stdout, "Rx TWT Bytes", twt->to_stack_twt.bytes);
	STATS_64(stdout, "Tx TWT Packets", twt->tx_success_twt.num);
	STATS_64(stdout, "Tx TWT Bytes", twt->tx_success_twt.bytes);
}

void print_advance_sta_data_link(struct advance_peer_data_link *link)
{
	print_basic_sta_data_link(&link->b_link);
	STATS_UL(stdout, "Rx SNR Time", link->rx_snr_measured_time);
}

void print_advance_sta_data_rate(struct advance_peer_data_rate *rate)
{
	print_basic_sta_data_rate(&rate->b_rate);
	STATS_32(stdout, "Avg ppdu Rx rate(kbps)", rate->rnd_avg_rx_rate);
	STATS_32(stdout, "Avg ppdu Tx rate(kbps)", rate->rnd_avg_tx_rate);
}

void print_advance_sta_data_nawds(struct advance_peer_data_nawds *nawds)
{
	STATS_64(stdout, "Multicast Packets", nawds->nawds_mcast.num);
	STATS_64(stdout, "Multicast Bytes", nawds->nawds_mcast.bytes);
	STATS_32(stdout, "NAWDS Tx Drop Count", nawds->nawds_mcast_tx_drop);
	STATS_32(stdout, "NAWDS Rx Drop Count", nawds->nawds_mcast_rx_drop);
}

static void print_advance_hist_stats(struct stats_if_hist_stats *hstats,
				     enum stats_if_hist_types hist_type)
{
	uint8_t index = 0;
	uint64_t count = 0;

	for (index = 0; index < STATS_IF_HIST_BUCKET_MAX; index++) {
		count = hstats->hist.freq[index];
		if (!count)
			continue;
		if (index > STATS_IF_DELAY_BUCKET_MAX) {
			STATS_PRINT("%s: Packets = %ju ",
				    "Invalid index", count);
			continue;
		}
		switch (hist_type) {
		case STATS_IF_HIST_TYPE_SW_ENQEUE_DELAY:
			STATS_PRINT("%s: Packets = %ju ",
				    stats_if_sw_enq_delay_bucket[index],
				    count);
			break;
		case STATS_IF_HIST_TYPE_HW_COMP_DELAY:
			STATS_PRINT("%s: Packets = %ju ",
				    stats_if_fw_to_hw_delay_bucket[index],
				    count);
			break;
		case STATS_IF_HIST_TYPE_REAP_STACK:
			STATS_PRINT("%s: Packets = %ju ",
				    stats_if_intfrm_delay_bucket[index],
				    count);
			break;
		case STATS_IF_HIST_TYPE_HW_TX_COMP_DELAY:
			STATS_PRINT("%s: Packets = %ju\n",
				    stats_if_hw_tx_comp_delay_bucket[index],
				    count);
			break;
		default:
			break;
		}
	}

	STATS_PRINT("Min = %d us\n", hstats->min);
	STATS_PRINT("Max = %d us\n", hstats->max);
	STATS_PRINT("Avg = %d us\n", hstats->avg);
}

static void print_advance_sta_data_delay(struct advance_peer_data_delay *delay)
{
	uint8_t tid;

	STATS_PRINT("Tx Delay Stats:\n");
	for (tid = 0; tid < STATS_IF_MAX_DATA_TIDS; tid++) {
		STATS_PRINT("----TID: %d----", tid);
		STATS_PRINT(" Software Enqueue Delay: ");
		print_advance_hist_stats(&delay->delay_stats[tid].tx_delay.tx_swq_delay,
					 STATS_IF_HIST_TYPE_SW_ENQEUE_DELAY);
		STATS_PRINT("\t\tHardware Transmission Delay: ");
		print_advance_hist_stats(&delay->delay_stats[tid].tx_delay.hwtx_delay,
					 STATS_IF_HIST_TYPE_HW_COMP_DELAY);
		STATS_PRINT("\t\tNW Delay Average: %d",
				delay->delay_stats[tid].tx_delay.nwdelay_avg);
		STATS_PRINT("\t\tSW Delay Average: %d",
				delay->delay_stats[tid].tx_delay.swdelay_avg);
		STATS_PRINT("\t\tHW Delay Average: %d\n",
				delay->delay_stats[tid].tx_delay.hwdelay_avg);
	}
	STATS_PRINT("\nRx Delay Stats:\n");
	for (tid = 0; tid < STATS_IF_MAX_DATA_TIDS; tid++) {
		STATS_PRINT("----TID: %d---- ", tid);
		STATS_PRINT("Reap2stack Deliver Delay: ");
		print_advance_hist_stats(&delay->delay_stats[tid].rx_delay
					 .to_stack_delay,
					 STATS_IF_HIST_TYPE_REAP_STACK);
	}
}

static void
print_advance_sta_data_jitter(struct advance_peer_data_jitter *jitter)
{
	uint8_t tid;

	for (tid = 0; tid < STATS_IF_MAX_DATA_TIDS; tid++) {
		STATS_PRINT("----TID: %d---- ", tid);
		STATS_PRINT("\tavg_jitter = %u ",
			    jitter->jitter_stats[tid].tx_avg_jitter);
		STATS_PRINT("\tavg_delay  = %u ",
			    jitter->jitter_stats[tid].tx_avg_delay);
		STATS_PRINT("\tavg_err  = %ju ",
			    jitter->jitter_stats[tid].tx_avg_err);
		STATS_PRINT("\ttotal_success = %ju ",
			    jitter->jitter_stats[tid].tx_total_success);
		STATS_PRINT("\tdrop  = %ju\n", jitter->jitter_stats[tid].tx_drop);
	}
}

static void
print_advance_sta_data_sawf_delay(struct advance_peer_data_sawfdelay *data,
				  uint8_t svc_id)
{
	if (svc_id > 0) {
		STATS_PRINT("----TIDX: %d----   ", data->tid);
		STATS_PRINT("----QUEUE: %d---- \n", data->msduq);
		STATS_PRINT("\nDelay Bins\n");
		print_advance_hist_stats(&data->delay[0][0].delay_hist,
					 STATS_IF_HIST_TYPE_HW_TX_COMP_DELAY);
		STATS_PRINT("NwDelay Moving average = %u us\n",
			    data->delay[0][0].nwdelay_avg);
		STATS_PRINT("SwDelay Moving average = %u us\n",
			    data->delay[0][0].swdelay_avg);
		STATS_PRINT("HwDelay Moving average = %u us\n",
			    data->delay[0][0].hwdelay_avg);
		STATS_PRINT("Delay bound success = %ju \n",
			    data->delay[0][0].delay_bound_success);
		STATS_PRINT("Delay bound failure = %ju \n",
			    data->delay[0][0].delay_bound_failure);
	} else {
		uint8_t tidx = 0, queues = 0;
		uint8_t max_queue = STATS_IF_MAX_SAWF_DATA_QUEUE;
		struct stats_if_sawf_delay_stats *dly = NULL;
		uint32_t hw_comp = STATS_IF_HIST_TYPE_HW_TX_COMP_DELAY;

		for (tidx = 0; tidx < STATS_IF_MAX_SAWF_DATA_TIDS; tidx++) {
			for (queues = 0; queues < max_queue; queues++) {
				dly = &data->delay[tidx][queues];
				STATS_PRINT("----TID: %d----   ", tidx);
				STATS_PRINT("----QUEUES: %d----\n", queues);

				STATS_PRINT("\nDelay Bins\n");
				print_advance_hist_stats(&dly->delay_hist,
							 hw_comp);
				STATS_PRINT("NwDelay Moving average = %u us\n",
					    data->delay[tidx][queues].
					    nwdelay_avg);
				STATS_PRINT("SwDelay Moving average = %u us\n",
					    data->delay[tidx][queues].
					    swdelay_avg);
				STATS_PRINT("HwDelay Moving average = %u us\n",
					    data->delay[tidx][queues].
					    hwdelay_avg);
				STATS_PRINT("Delay bound success = %ju\n",
					    data->delay[tidx][queues].
					    delay_bound_success);
				STATS_PRINT("Delay bound failure = %ju\n",
					    data->delay[tidx][queues].
					    delay_bound_failure);
			}
		}
	}
}

static void
print_advance_sta_data_sawf_tx(struct advance_peer_data_sawftx *data,
			       uint8_t svc_id)
{
	if (svc_id > 0) {
		STATS_PRINT("----TIDX: %d----   ", data->tid);
		STATS_PRINT("----QUEUE: %d----\n", data->msduq);

		STATS_PRINT("Tx_info_success_num        = %ju\n",
			    data->tx[0][0].tx_success.num);
		STATS_PRINT("Tx_info_success_bytes      = %ju\n",
			    data->tx[0][0].tx_success.bytes);
		STATS_PRINT("Tx_info_ingress_num        = %ju\n",
			    data->tx[0][0].tx_ingress.num);
		STATS_PRINT("Tx_info_ingress_bytes      = %ju\n",
			    data->tx[0][0].tx_ingress.bytes);
		STATS_PRINT("Tx_info_Throughput         = %u\n",
			    data->tx[0][0].throughput);
		STATS_PRINT("Tx_info_ingress_rate       = %u\n",
			    data->tx[0][0].ingress_rate);

		STATS_PRINT("Tx_info_drop_num           = %ju\n",
			    data->tx[0][0].dropped.fw_rem.num);
		STATS_PRINT("Tx_info_drop_bytes         = %ju\n",
			     data->tx[0][0].dropped.fw_rem.bytes);
		STATS_PRINT("Tx_info_drop_fw_rem_notx   = %u\n",
			    data->tx[0][0].dropped.fw_rem_notx);
		STATS_PRINT("Tx_info_drop_Tx_fw_rem_tx  = %u\n",
			    data->tx[0][0].dropped.fw_rem_tx);
		STATS_PRINT("Tx_info_drop_Tx_age_out    = %u\n",
			    data->tx[0][0].dropped.age_out);

		STATS_PRINT("Tx_info_drop_fw_reason1    = %u\n",
			    data->tx[0][0].dropped.fw_reason1);
		STATS_PRINT("Tx_info_drop_fw_reason2    = %u\n",
			    data->tx[0][0].dropped.fw_reason2);
		STATS_PRINT("Tx_info_drop_fw_reason3    = %u\n",
			    data->tx[0][0].dropped.fw_reason3);
		STATS_PRINT("Tx_info_tx_failed          = %u\n",
			    data->tx[0][0].tx_failed);
		STATS_PRINT("Tx_info_queue_depth        = %u\n",
			    data->tx[0][0].queue_depth);
		STATS_PRINT("Service_intvl_success_cnt  = %ju\n",
			    data->tx[0][0].svc_intval_stats.success_cnt);
		STATS_PRINT("Service_intvl_failure_cnt  = %ju\n",
			    data->tx[0][0].svc_intval_stats.failure_cnt);
		STATS_PRINT("Burst_size_success_cnt     = %ju\n",
			    data->tx[0][0].burst_size_stats.success_cnt);
		STATS_PRINT("Burst_size_failure_cnt     = %ju\n",
			    data->tx[0][0].burst_size_stats.failure_cnt);
	} else {
		uint8_t tidx = 0, queues = 0;
		uint8_t max_queue = STATS_IF_MAX_SAWF_DATA_QUEUE;
		struct stats_if_sawf_tx_stats *sawftx;

		for (tidx = 0; tidx < STATS_IF_MAX_SAWF_DATA_TIDS; tidx++) {
			for (queues = 0; queues < max_queue; queues++) {
				sawftx = &data->tx[tidx][queues];
				STATS_PRINT("----TIDX: %d----   ", tidx);
				STATS_PRINT("----QUEUE: %d----\n", queues);
				STATS_PRINT("Tx_info_success_num        = %ju\n",
					    sawftx->tx_success.num);
				STATS_PRINT("Tx_info_success_bytes      = %ju\n",
					    sawftx->tx_success.bytes);
				STATS_PRINT("Tx_info_ingress_num        = %ju\n",
					    sawftx->tx_ingress.num);
				STATS_PRINT("Tx_info_ingress_bytes      = %ju\n",
					    sawftx->tx_ingress.bytes);
				STATS_PRINT("Tx_info_Throughput         = %u\n",
					    sawftx->throughput);
				STATS_PRINT("Tx_info_ingress_rate       = %u\n",
					    sawftx->ingress_rate);

				STATS_PRINT("Tx_info_drop_num           = %ju\n",
					    sawftx->dropped.fw_rem.num);
				STATS_PRINT("Tx_info_drop_bytes         = %ju\n",
					    sawftx->dropped.fw_rem.bytes);
				STATS_PRINT("Tx_info_drop_fw_rem_notx   = %u\n",
					    sawftx->dropped.fw_rem_notx);
				STATS_PRINT("Tx_info_drop_Tx_fw_rem_tx  = %u\n",
					    sawftx->dropped.fw_rem_tx);
				STATS_PRINT("Tx_info_drop_Tx_age_out    = %u\n",
					    sawftx->dropped.age_out);


				STATS_PRINT("Tx_info_drop_fw_reason1    = %u\n",
					    sawftx->dropped.fw_reason1);
				STATS_PRINT("Tx_inf_drop_Tx_fw_reason2  = %u\n",
					    sawftx->dropped.fw_reason2);
				STATS_PRINT("Tx_info_drop_Tx_fw_reason3 = %u\n",
					    sawftx->dropped.fw_reason3);
				STATS_PRINT("Tx_info_tx_failed          = %u\n",
					    sawftx->tx_failed);
				STATS_PRINT("Tx_info_queue_depth        = %u\n",
					    sawftx->queue_depth);
				STATS_PRINT("Service_intvl_success_cnt  = %ju\n",
					sawftx->svc_intval_stats.success_cnt);
				STATS_PRINT("Service_intvl_failure_cnt  = %ju\n",
					sawftx->svc_intval_stats.failure_cnt);
				STATS_PRINT("Burst_size_success_cnt     = %ju\n",
					sawftx->burst_size_stats.success_cnt);
				STATS_PRINT("Burst_size_failure_cnt     = %ju\n",
					sawftx->burst_size_stats.failure_cnt);
			}
		}
	}
}
void print_advance_sta_ctrl_tx(struct advance_peer_ctrl_tx *tx)
{
	print_basic_sta_ctrl_tx(&tx->b_tx);
	STATS_32(stdout, "Assocition Tx Count", tx->cs_tx_assoc);
	STATS_32(stdout, "Assocition Tx Failed Count", tx->cs_tx_assoc_fail);
}

void print_advance_sta_ctrl_rx(struct advance_peer_ctrl_rx *rx)
{
	print_basic_sta_ctrl_rx(&rx->b_rx);
}

void print_advance_sta_ctrl_twt(struct advance_peer_ctrl_twt *twt)
{
	STATS_32(stdout, "TWT Event Type", twt->cs_twt_event_type);
	STATS_32(stdout, "TWT Flow ID", twt->cs_twt_flow_id);
	STATS_32(stdout, "Broadcast TWT", twt->cs_twt_bcast);
	STATS_32(stdout, "TWT Trigger", twt->cs_twt_trig);
	STATS_32(stdout, "TWT Announcement", twt->cs_twt_announ);
	STATS_32(stdout, "TWT Dialog ID", twt->cs_twt_dialog_id);
	STATS_32(stdout, "TWT Wake duration (us)", twt->cs_twt_wake_dura_us);
	STATS_32(stdout, "TWT Wake interval (us)", twt->cs_twt_wake_intvl_us);
	STATS_32(stdout, "TWT SP offset (us)", twt->cs_twt_sp_offset_us);
}

void print_advance_sta_ctrl_link(struct advance_peer_ctrl_link *link)
{
	print_basic_sta_ctrl_link(&link->b_link);
}

void print_advance_sta_ctrl_rate(struct advance_peer_ctrl_rate *rate)
{
	print_basic_sta_ctrl_rate(&rate->b_rate);
}

void print_advance_vap_data_me(struct advance_vdev_data_me *me)
{
	STATS_64(stdout, "Multicast Packets", me->mcast_pkt.num);
	STATS_64(stdout, "Multicast Bytes", me->mcast_pkt.bytes);
	STATS_32(stdout, "Unicast Packets", me->ucast);
}

void print_advance_vap_data_tx(struct advance_vdev_data_tx *tx)
{
	print_basic_vap_data_tx(&tx->b_tx);
	print_advance_data_tx_stats(&tx->adv_tx);

	if (!g_ipa_mode) {
		STATS_64(stdout, "Tx Reinject Packets", tx->reinject_pkts.num);
		STATS_64(stdout, "Tx Reinject Bytes", tx->reinject_pkts.bytes);
		STATS_64(stdout, "Tx Inspect Packets", tx->inspect_pkts.num);
		STATS_64(stdout, "Tx Inspect Bytes", tx->inspect_pkts.bytes);
		STATS_32(stdout, "Tx CCE Classified", tx->cce_classified);
	}
}

void print_advance_vap_data_rx(struct advance_vdev_data_rx *rx)
{
	print_basic_vap_data_rx(&rx->b_rx);
	print_advance_data_rx_stats(&rx->adv_rx);
}

void print_advance_vap_data_raw(struct advance_vdev_data_raw *raw)
{
	STATS_64(stdout, "RAW Rx Packets", raw->rx_raw.num);
	STATS_64(stdout, "RAW Rx Bytes", raw->rx_raw.bytes);
	STATS_64(stdout, "RAW Tx Packets", raw->tx_raw_pkt.num);
	STATS_64(stdout, "RAW Tx Bytes", raw->tx_raw_pkt.bytes);
	STATS_32(stdout, "RAW Tx Classified by CCE", raw->cce_classified_raw);
}

void print_advance_vap_data_tso(struct advance_vdev_data_tso *tso)
{
	STATS_64(stdout, "SG Packets", tso->sg_pkt.num);
	STATS_64(stdout, "SG Bytess", tso->sg_pkt.bytes);
	STATS_64(stdout, "Non-SG Packets", tso->non_sg_pkts.num);
	STATS_64(stdout, "Non-SG Bytess", tso->non_sg_pkts.bytes);
	STATS_64(stdout, "TSO Packets", tso->num_tso_pkts.num);
	STATS_64(stdout, "TSO Bytess", tso->num_tso_pkts.bytes);
}

void print_advance_vap_data_igmp(struct advance_vdev_data_igmp *igmp)
{
	STATS_32(stdout, "IGMP Received", igmp->igmp_rcvd);
	STATS_32(stdout, "IGMP Converted Unicast", igmp->igmp_ucast_converted);
}

void print_advance_vap_data_mesh(struct advance_vdev_data_mesh *mesh)
{
	STATS_32(stdout, "MESH FW Completion Count", mesh->completion_fw);
	STATS_32(stdout, "MESH FW Exception Count", mesh->exception_fw);
}

void print_advance_vap_data_nawds(struct advance_vdev_data_nawds *nawds)
{
	STATS_64(stdout, "Multicast Tx Packets", nawds->tx_nawds_mcast.num);
	STATS_64(stdout, "Multicast Tx Bytes", nawds->tx_nawds_mcast.bytes);
	STATS_32(stdout, "NAWDS Tx Drop Count", nawds->nawds_mcast_tx_drop);
	STATS_32(stdout, "NAWDS Rx Drop Count", nawds->nawds_mcast_rx_drop);
}

void print_advance_vap_ctrl_tx(struct advance_vdev_ctrl_tx *tx)
{
	print_basic_vap_ctrl_tx(&tx->b_tx);
	STATS_64(stdout, "Tx Off Channel Management Count",
		 tx->cs_tx_offchan_mgmt);
	STATS_64(stdout, "Tx Off Channel Data Count", tx->cs_tx_offchan_data);
	STATS_64(stdout, "Tx Off Channel Fail Count", tx->cs_tx_offchan_fail);
	STATS_64(stdout, "Tx Beacon Count", tx->cs_tx_bcn_success);
	STATS_64(stdout, "Tx Beacon Outage Count", tx->cs_tx_bcn_outage);
	STATS_64(stdout, "Tx FILS Frame Sent Count", tx->cs_fils_frames_sent);
	STATS_64(stdout, "Tx FILS Frame Sent Fail",
		 tx->cs_fils_frames_sent_fail);
	STATS_64(stdout, "Tx Offload Probe Response Success Count",
		 tx->cs_tx_offload_prb_resp_succ_cnt);
	STATS_64(stdout, "Tx Offload Probe Response Fail Count",
		 tx->cs_tx_offload_prb_resp_fail_cnt);
}

void print_advance_vap_ctrl_rx(struct advance_vdev_ctrl_rx *rx)
{
	print_basic_vap_ctrl_rx(&rx->b_rx);
	STATS_64(stdout, "Rx Action Frame Count", rx->cs_rx_action);
	STATS_64(stdout, "Rx MLME Auth Attempt", rx->cs_mlme_auth_attempt);
	STATS_64(stdout, "Rx MLME Auth Success", rx->cs_mlme_auth_success);
	STATS_64(stdout, "Authorization Attempt", rx->cs_authorize_attempt);
	STATS_64(stdout, "Authorization Success", rx->cs_authorize_success);
	STATS_64(stdout, "Probe Request Drops", rx->cs_prob_req_drops);
	STATS_64(stdout, "OOB Probe Requests", rx->cs_oob_probe_req_count);
	STATS_64(stdout, "Wildcard probe requests drops",
		 rx->cs_wc_probe_req_drops);
	STATS_64(stdout, "Connections refuse Radio limit",
		 rx->cs_sta_xceed_rlim);
	STATS_64(stdout, "Connections refuse Vap limit", rx->cs_sta_xceed_vlim);
}

void print_advance_vap_ctrl_link(struct advance_vdev_ctrl_link *link)
{
	print_basic_vap_ctrl_link(&link->b_link);
}

void print_advance_radio_data_me(struct advance_pdev_data_me *me)
{
	STATS_64(stdout, "Multicast Packets", me->mcast_pkt.num);
	STATS_64(stdout, "Multicast Bytes", me->mcast_pkt.bytes);
	STATS_32(stdout, "Unicast Packets", me->ucast);
}

void print_histogram_stats(struct histogram_stats *hist)
{
	STATS_32(stdout, " Single Packets", hist->pkts_1);
	STATS_32(stdout, "   2-20 Packets", hist->pkts_2_20);
	STATS_32(stdout, "  21-40 Packets", hist->pkts_21_40);
	STATS_32(stdout, "  41-60 Packets", hist->pkts_41_60);
	STATS_32(stdout, "  61-80 Packets", hist->pkts_61_80);
	STATS_32(stdout, " 81-100 Packets", hist->pkts_81_100);
	STATS_32(stdout, "101-200 Packets", hist->pkts_101_200);
	STATS_32(stdout, "   200+ Packets", hist->pkts_201_plus);
}

void print_advance_radio_data_tx(struct advance_pdev_data_tx *tx)
{
	print_basic_radio_data_tx(&tx->b_tx);
	print_advance_data_tx_stats(&tx->adv_tx);

	if (!g_ipa_mode) {
		STATS_64(stdout, "Tx Reinject Packets", tx->reinject_pkts.num);
		STATS_64(stdout, "Tx Reinject Bytes", tx->reinject_pkts.bytes);
		STATS_64(stdout, "Tx Inspect Packets", tx->inspect_pkts.num);
		STATS_64(stdout, "Tx Inspect Bytes", tx->inspect_pkts.bytes);
		STATS_32(stdout, "Tx CCE Classified", tx->cce_classified);
		STATS_PRINT("\tTx packets sent per interrupt\n");
		print_histogram_stats(&tx->tx_hist);
	}
}

void print_advance_radio_data_rx(struct advance_pdev_data_rx *rx)
{
	print_basic_radio_data_rx(&rx->b_rx);
	print_advance_data_rx_stats(&rx->adv_rx);

	if (!g_ipa_mode) {
		STATS_PRINT("\tRx packets sent per interrupt\n");
		print_histogram_stats(&rx->rx_hist);
	}
}

void print_advance_radio_data_raw(struct advance_pdev_data_raw *raw)
{
	STATS_64(stdout, "RAW Rx Packets", raw->rx_raw.num);
	STATS_64(stdout, "RAW Rx Bytes", raw->rx_raw.bytes);
	STATS_64(stdout, "RAW Tx Packets", raw->tx_raw_pkt.num);
	STATS_64(stdout, "RAW Tx Bytes", raw->tx_raw_pkt.bytes);
	STATS_32(stdout, "RAW Tx Classified by CCE", raw->cce_classified_raw);
	STATS_32(stdout, "RAW Rx Packets", raw->rx_raw_pkts);
}

void print_advance_radio_data_tso(struct advance_pdev_data_tso *tso)
{
	STATS_64(stdout, "SG Packets", tso->sg_pkt.num);
	STATS_64(stdout, "SG Bytess", tso->sg_pkt.bytes);
	STATS_64(stdout, "Non-SG Packets", tso->non_sg_pkts.num);
	STATS_64(stdout, "Non-SG Bytess", tso->non_sg_pkts.bytes);
	STATS_64(stdout, "TSO Packets", tso->num_tso_pkts.num);
	STATS_64(stdout, "TSO Bytess", tso->num_tso_pkts.bytes);
	STATS_32(stdout, "TSO Complete", tso->tso_comp);
	STATS_PRINT("\tTSO Histogram\n");
	STATS_64(stdout, "    Single", tso->segs_1);
	STATS_64(stdout, "  2-5 segs", tso->segs_2_5);
	STATS_64(stdout, " 6-10 segs", tso->segs_6_10);
	STATS_64(stdout, "11-15 segs", tso->segs_11_15);
	STATS_64(stdout, "16-20 segs", tso->segs_16_20);
	STATS_64(stdout, "  20+ segs", tso->segs_20_plus);
}

void print_advance_radio_data_vow(struct advance_pdev_data_vow *vow)
{
	struct stats_if_tid_tx_stats *total_tx;
	struct stats_if_tid_rx_stats *total_rx;
	uint8_t tid, idx;
	uint64_t count;

	STATS_64(stdout, "Packets received in hardstart", vow->ingress_stack);
	STATS_64(stdout, "Packets dropped in osif layer", vow->osif_drop);

	STATS_PRINT("\n\tPer TID Video Stats:\n");
	for (tid = 0; tid < STATS_IF_MAX_DATA_TIDS; tid++) {
		total_tx = &vow->tx_stats[tid];
		total_rx = &vow->rx_stats[tid];
		STATS_PRINT("\t------------TID: %d------------\n", tid);
		STATS_64(stdout, "Tx TQM Success Count",
			 total_tx->tqm_status_cnt[STATS_IF_TX_RR_FRAME_ACKED]);
		for (idx = 1; idx < STATS_IF_MAX_TX_TQM_STATUS; idx++) {
			if (total_tx->tqm_status_cnt[idx])
				STATS_PRINT("\t\tTx TQM Drop Count[%u]: %ju\n",
					    idx, total_tx->tqm_status_cnt[idx]);
		}
		STATS_64(stdout, "Tx HTT Success Count",
			 total_tx->htt_status_cnt[STATS_IF_TX_FW_STATUS_OK]);
		for (idx = 1; idx < STATS_IF_MAX_TX_HTT_STATUS; idx++) {
			if (total_tx->htt_status_cnt[idx])
				STATS_PRINT("\t\tTx HTT Drop Count[%u]: %ju\n",
					    idx, total_tx->htt_status_cnt[idx]);
		}
		STATS_64(stdout, "Tx Hardware Drop Count",
			 total_tx->swdrop_cnt[STATS_IF_TX_HW_ENQUEUE]);
		STATS_64(stdout, "Tx Software Drop Count",
			 total_tx->swdrop_cnt[STATS_IF_TX_SW_ENQUEUE]);
		STATS_64(stdout, "Tx Descriptor Error Count",
			 total_tx->swdrop_cnt[STATS_IF_TX_DESC_ERR]);
		STATS_64(stdout, "Tx HAL Ring Error Count",
			 total_tx->swdrop_cnt[STATS_IF_TX_HAL_RING_ACCESS_ERR]);
		STATS_64(stdout, "Tx Dma Map Error Count",
			 total_tx->swdrop_cnt[STATS_IF_TX_DMA_MAP_ERR]);

		STATS_64(stdout, "Rx Delievered Count",
			 total_rx->delivered_to_stack);
		STATS_64(stdout, "Rx Software Enqueue Drop Count",
			 total_rx->fail_cnt[STATS_IF_RX_ENQUEUE_DROP]);
		STATS_64(stdout, "Rx Intrabss Drop Count",
			 total_rx->fail_cnt[STATS_IF_RX_INTRABSS_DROP]);
		STATS_64(stdout, "Rx Msdu Done Failure Count",
			 total_rx->fail_cnt[STATS_IF_RX_MSDU_DONE_FAILURE]);
		STATS_64(stdout, "Rx Invalid Peer Count",
			 total_rx->fail_cnt[STATS_IF_RX_INVALID_PEER_VDEV]);
		STATS_64(stdout, "Rx Policy Check Drop Count",
			 total_rx->fail_cnt[STATS_IF_RX_POLICY_CHECK_DROP]);
		STATS_64(stdout, "Rx Mec Drop Count",
			 total_rx->fail_cnt[STATS_IF_RX_MEC_DROP]);
		STATS_64(stdout, "Rx Nawds Mcast Drop Count",
			 total_rx->fail_cnt[STATS_IF_RX_NAWDS_MCAST_DROP]);
		STATS_64(stdout, "Rx Mesh Filter Drop Count",
			 total_rx->fail_cnt[STATS_IF_RX_MESH_FILTER_DROP]);
		STATS_64(stdout, "Rx Intra Bss Deliver Count",
			 total_rx->intrabss_cnt);
		STATS_64(stdout, "Rx MSDU Count",
			 total_rx->msdu_cnt);
		STATS_64(stdout, "Rx Multicast MSDU Count",
			 total_rx->mcast_msdu_cnt);
		STATS_64(stdout, "Rx Broadcast MSDU Count",
			 total_rx->bcast_msdu_cnt);
	}
	STATS_PRINT("\n\tPer TID Delay Non-Zero Stats:\n");
	for (tid = 0; tid < STATS_IF_MAX_DATA_TIDS; tid++) {
		total_tx = &vow->tx_stats[tid];
		total_rx = &vow->rx_stats[tid];
		STATS_PRINT("\t-------------TID: %d------------\n", tid);
		STATS_PRINT("\tSoftware Enqueue Delay:\n");
		for (idx = 0; idx < STATS_IF_DELAY_BUCKET_MAX; idx++) {
			count = total_tx->swq_delay.delay_bucket[idx];
			if (count)
				STATS_PRINT("\t\t%s:  Packets = %ju\n",
					    stats_if_sw_enq_delay_bucket[idx],
					    count);
		}
		STATS_32(stdout, "Min", total_tx->swq_delay.min_delay);
		STATS_32(stdout, "Max", total_tx->swq_delay.max_delay);
		STATS_32(stdout, "Avg", total_tx->swq_delay.avg_delay);

		STATS_PRINT("\tHardware Transmission Delay:\n");
		for (idx = 0; idx < STATS_IF_DELAY_BUCKET_MAX; idx++) {
			count = total_tx->hwtx_delay.delay_bucket[idx];
			if (count)
				STATS_PRINT("\t\t%s:  Packets = %ju\n",
					    stats_if_fw_to_hw_delay_bucket[idx],
					    count);
		}
		STATS_32(stdout, "Min", total_tx->hwtx_delay.min_delay);
		STATS_32(stdout, "Max", total_tx->hwtx_delay.max_delay);
		STATS_32(stdout, "Avg", total_tx->hwtx_delay.avg_delay);

		STATS_PRINT("\tTx Interframe Delay:\n");
		for (idx = 0; idx < STATS_IF_DELAY_BUCKET_MAX; idx++) {
			count = total_tx->intfrm_delay.delay_bucket[idx];
			if (count)
				STATS_PRINT("\t\t%s:  Packets = %ju\n",
					    stats_if_intfrm_delay_bucket[idx],
					    count);
		}
		STATS_32(stdout, "Min", total_tx->intfrm_delay.min_delay);
		STATS_32(stdout, "Max", total_tx->intfrm_delay.max_delay);
		STATS_32(stdout, "Avg", total_tx->intfrm_delay.avg_delay);

		STATS_PRINT("\tRx Interframe Delay:\n");
		for (idx = 0; idx < STATS_IF_DELAY_BUCKET_MAX; idx++) {
			count = total_rx->intfrm_delay.delay_bucket[idx];
			if (count)
				STATS_PRINT("\t\t%s:  Packets = %ju\n",
					    stats_if_intfrm_delay_bucket[idx],
					    count);
		}
		STATS_32(stdout, "Min", total_rx->intfrm_delay.min_delay);
		STATS_32(stdout, "Max", total_rx->intfrm_delay.max_delay);
		STATS_32(stdout, "Avg", total_rx->intfrm_delay.avg_delay);

		STATS_PRINT("\tRx Reap to Stack Delay:\n");
		for (idx = 0; idx < STATS_IF_DELAY_BUCKET_MAX; idx++) {
			count = total_rx->to_stack_delay.delay_bucket[idx];
			if (count)
				STATS_PRINT("\t\t%s:  Packets = %ju\n",
					    stats_if_intfrm_delay_bucket[idx],
					    count);
		}
		STATS_32(stdout, "Min", total_rx->to_stack_delay.min_delay);
		STATS_32(stdout, "Max", total_rx->to_stack_delay.max_delay);
		STATS_32(stdout, "Avg", total_rx->to_stack_delay.avg_delay);
	}
	STATS_PRINT("\n\tPer TID RX Error Stats:\n");
	for (tid = 0; tid < STATS_IF_MAX_VOW_TID; tid++) {
		total_rx = &vow->rx_stats[tid];
		STATS_PRINT("\t------------TID: %d------------\n", tid + 4);
		STATS_PRINT("\tRx REO Error stats:\n");
		STATS_64(stdout, "err_src_reo_code_inv",
			 total_rx->reo_err.err_src_reo_code_inv);
		for (idx = 0; idx < STATS_IF_REO_ERR_MAX; idx++) {
			STATS_PRINT("\t\terr src reo codes: %u = %ju\n",
				    idx, total_rx->reo_err.err_reo_codes[idx]);
		}
		STATS_PRINT("\tRx Rxdma Error stats:\n");
		STATS_64(stdout, "err_src_rxdma_code_inv",
			 total_rx->rxdma_err.err_src_rxdma_code_inv);
		for (idx = 0; idx < STATS_IF_RXDMA_ERR_MAX; idx++) {
			STATS_PRINT("\t\terr src dma codes: %u = %ju\n",
				    idx,
				    total_rx->rxdma_err.err_dma_codes[idx]);
		}
	}
}

void print_advance_radio_data_igmp(struct advance_pdev_data_igmp *igmp)
{
	STATS_32(stdout, "IGMP Received", igmp->igmp_rcvd);
	STATS_32(stdout, "IGMP Converted Unicast", igmp->igmp_ucast_converted);
}

void print_advance_radio_data_mesh(struct advance_pdev_data_mesh *mesh)
{
	STATS_32(stdout, "MESH FW Completion Count", mesh->completion_fw);
	STATS_32(stdout, "MESH FW Exception Count", mesh->exception_fw);
}

void print_advance_radio_data_nawds(struct advance_pdev_data_nawds *nawds)
{
	STATS_64(stdout, "Multicast Tx Packets", nawds->tx_nawds_mcast.num);
	STATS_64(stdout, "Multicast Tx Bytes", nawds->tx_nawds_mcast.bytes);
	STATS_32(stdout, "NAWDS Tx Drop Count", nawds->nawds_mcast_tx_drop);
	STATS_32(stdout, "NAWDS Rx Drop Count", nawds->nawds_mcast_rx_drop);
}

void print_advance_radio_ctrl_tx(struct advance_pdev_ctrl_tx *tx)
{
	print_basic_radio_ctrl_tx(&tx->b_tx);
	STATS_64(stdout, "Tx Beacon Count", tx->cs_tx_beacon);
}

void print_advance_radio_ctrl_rx(struct advance_pdev_ctrl_rx *rx)
{
	print_basic_radio_ctrl_rx(&rx->b_rx);
	STATS_32(stdout, "Rx Mgmt Frames dropped (RSSI too low)",
		 rx->cs_rx_mgmt_rssi_drop);
}

void print_advance_radio_ctrl_link(struct advance_pdev_ctrl_link *link)
{
	print_basic_radio_ctrl_link(&link->b_link);
	STATS_8(stdout, "DCS Tx Utilized", link->dcs_ap_tx_util);
	STATS_8(stdout, "DCS Rx Utilized", link->dcs_ap_rx_util);
	STATS_8(stdout, "DCS Self BSS Utilized", link->dcs_self_bss_util);
	STATS_8(stdout, "DCS OBSS Utilized", link->dcs_obss_util);
	STATS_8(stdout, "DCS OBSS Rx Utilised", link->dcs_obss_rx_util);
	STATS_8(stdout, "DCS Free Medium", link->dcs_free_medium);
	STATS_8(stdout, "DCS Non-WiFi Utilized", link->dcs_non_wifi_util);
	STATS_32(stdout, "DCS Under SS Utilized", link->dcs_ss_under_util);
}

void print_advance_ap_data_tx(struct advance_psoc_data_tx *tx)
{
	print_basic_ap_data_tx(&tx->b_tx);
}

void print_advance_ap_data_rx(struct advance_psoc_data_rx *rx)
{
	print_basic_ap_data_rx(&rx->b_rx);
	STATS_32(stdout, "Rx Ring Error Packets", rx->err_ring_pkts);
	STATS_32(stdout, "Rx Fragments", rx->rx_frags);
	STATS_32(stdout, "Rx HW Reinject", rx->rx_hw_reinject);
	STATS_32(stdout, "Rx BAR Frames", rx->bar_frame);
	STATS_32(stdout, "Rx Rejected", rx->rejected);
	STATS_32(stdout, "Rx RAW Frame Drop", rx->raw_frm_drop);
}
#endif /* WLAN_ADVANCE_TELEMETRY */

void print_basic_sta_data(struct stats_obj *sta)
{
	struct basic_peer_data *data = sta->stats;

	STATS_PRINT("Basic Data STATS For STA %s (Parent %s)\n",
		    macaddr_to_str(sta->u_id.mac_addr), sta->pif_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_sta_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_sta_data_rx(data->rx);
	}
	if (data->link) {
		STATS_PRINT("Link Stats\n");
		print_basic_sta_data_link(data->link);
	}
	if (data->rate) {
		STATS_PRINT("Rate Stats\n");
		print_basic_sta_data_rate(data->rate);
	}
}

void print_basic_sta_ctrl(struct stats_obj *sta)
{
	struct basic_peer_ctrl *ctrl = sta->stats;

	STATS_PRINT("Basic Control STATS For STA %s (Parent %s)\n",
		    macaddr_to_str(sta->u_id.mac_addr), sta->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_sta_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_sta_ctrl_rx(ctrl->rx);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_basic_sta_ctrl_link(ctrl->link);
	}
	if (ctrl->rate) {
		STATS_PRINT("Rate Stats\n");
		print_basic_sta_ctrl_rate(ctrl->rate);
	}
}

void print_basic_vap_data(struct stats_obj *vap)
{
	struct basic_vdev_data *data = vap->stats;

	STATS_PRINT("Basic Data STATS For VAP %s (Parent %s)\n",
		    vap->u_id.if_name, vap->pif_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_vap_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_vap_data_rx(data->rx);
	}
}

void print_basic_vap_ctrl(struct stats_obj *vap)
{
	struct basic_vdev_ctrl *ctrl = vap->stats;

	STATS_PRINT("Basic Control STATS For VAP %s (Parent %s)\n",
		    vap->u_id.if_name, vap->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_vap_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_vap_ctrl_rx(ctrl->rx);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_basic_vap_ctrl_link(ctrl->link);
	}
}

void print_basic_radio_data(struct stats_obj *radio)
{
	struct basic_pdev_data *data = radio->stats;

	STATS_PRINT("Basic Data STATS for Radio %s (Parent %s)\n",
		    radio->u_id.if_name, radio->pif_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_radio_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_radio_data_rx(data->rx);
	}
}

void print_basic_radio_ctrl(struct stats_obj *radio)
{
	struct basic_pdev_ctrl *ctrl = radio->stats;

	STATS_PRINT("Basic Control STATS for Radio %s (Parent %s)\n",
		    radio->u_id.if_name, radio->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_radio_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_radio_ctrl_rx(ctrl->rx);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_basic_radio_ctrl_link(ctrl->link);
	}
}

void print_basic_ap_data(struct stats_obj *ap)
{
	struct basic_psoc_data *data = ap->stats;

	STATS_PRINT("Basic Data STATS for AP %s\n", ap->u_id.if_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_basic_ap_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_basic_ap_data_rx(data->rx);
	}
}

void print_basic_stats(struct stats_obj *obj)
{
	switch (obj->obj_type) {
	case STATS_OBJ_STA:
		if (obj->type == STATS_TYPE_DATA)
			print_basic_sta_data(obj);
		else
			print_basic_sta_ctrl(obj);
		break;
	case STATS_OBJ_VAP:
		if (obj->type == STATS_TYPE_DATA)
			print_basic_vap_data(obj);
		else
			print_basic_vap_ctrl(obj);
		break;
	case STATS_OBJ_RADIO:
		if (obj->type == STATS_TYPE_DATA)
			print_basic_radio_data(obj);
		else
			print_basic_radio_ctrl(obj);
		break;
	case STATS_OBJ_AP:
		if (obj->type == STATS_TYPE_DATA)
			print_basic_ap_data(obj);
		break;
	default:
		STATS_ERR("Invalid object option\n");
	}
}

#if WLAN_ADVANCE_TELEMETRY
void print_advance_sta_data(struct stats_obj *sta)
{
	struct advance_peer_data *data = sta->stats;

	if (data->rdk)
		print_advance_sta_data_rdk(data->rdk, sta->u_id.mac_addr,
					   sta->pif_name);
	else
		STATS_PRINT("Advance Data STATS For STA %s (Parent %s)\n",
			    macaddr_to_str(sta->u_id.mac_addr), sta->pif_name);

	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_sta_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_sta_data_rx(data->rx);
	}
	if (data->fwd) {
		STATS_PRINT("Intra Bss (FWD) Stats\n");
		print_advance_sta_data_fwd(data->fwd);
	}
	if (data->raw) {
		STATS_PRINT("RAW Stats\n");
		print_advance_sta_data_raw(data->raw);
	}
	if (data->twt) {
		STATS_PRINT("TWT Stats\n");
		print_advance_sta_data_twt(data->twt);
	}
	if (data->link) {
		STATS_PRINT("Link Stats\n");
		print_advance_sta_data_link(data->link);
	}
	if (data->rate) {
		STATS_PRINT("Rate Stats\n");
		print_advance_sta_data_rate(data->rate);
	}
	if (data->nawds) {
		STATS_PRINT("NAWDS Stats\n");
		print_advance_sta_data_nawds(data->nawds);
	}
	if (data->delay) {
		STATS_PRINT("DELAY Stats\n");
		print_advance_sta_data_delay(data->delay);
	}
	if (data->jitter) {
		STATS_PRINT("JITTER Stats\n");
		print_advance_sta_data_jitter(data->jitter);
	}
	if (data->sawfdelay) {
		STATS_PRINT("SAWFDELAY Stats\n");
		print_advance_sta_data_sawf_delay(data->sawfdelay, sta->serviceid);
	}
	if (data->sawftx) {
		STATS_PRINT("SAWFTX Stats\n");
		print_advance_sta_data_sawf_tx(data->sawftx, sta->serviceid);
	}
}

void print_advance_sta_ctrl(struct stats_obj *sta)
{
	struct advance_peer_ctrl *ctrl = sta->stats;

	STATS_PRINT("Advance Control STATS For STA %s (Parent %s)\n",
		    macaddr_to_str(sta->u_id.mac_addr), sta->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_sta_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_sta_ctrl_rx(ctrl->rx);
	}
	if (ctrl->twt) {
		STATS_PRINT("TWT Stats\n");
		print_advance_sta_ctrl_twt(ctrl->twt);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_advance_sta_ctrl_link(ctrl->link);
	}
	if (ctrl->rate) {
		STATS_PRINT("Rate Stats\n");
		print_advance_sta_ctrl_rate(ctrl->rate);
	}
}

void print_advance_vap_data(struct stats_obj *vap)
{
	struct advance_vdev_data *data = vap->stats;

	STATS_PRINT("Advance Data STATS for Vap %s (Parent %s)\n",
		    vap->u_id.if_name, vap->pif_name);
	if (data->me) {
		STATS_PRINT("ME Stats\n");
		print_advance_vap_data_me(data->me);
	}
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_vap_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_vap_data_rx(data->rx);
	}
	if (data->raw) {
		STATS_PRINT("RAW Stats\n");
		print_advance_vap_data_raw(data->raw);
	}
	if (data->tso) {
		STATS_PRINT("TSO Stats\n");
		print_advance_vap_data_tso(data->tso);
	}
	if (data->igmp) {
		STATS_PRINT("IGMP Stats\n");
		print_advance_vap_data_igmp(data->igmp);
	}
	if (data->mesh) {
		STATS_PRINT("MESH Stats\n");
		print_advance_vap_data_mesh(data->mesh);
	}
	if (data->nawds) {
		STATS_PRINT("NAWDS Stats\n");
		print_advance_vap_data_nawds(data->nawds);
	}
}

void print_advance_vap_ctrl(struct stats_obj *vap)
{
	struct advance_vdev_ctrl *ctrl = vap->stats;

	STATS_PRINT("Advance Control STATS for Vap %s (Parent %s)\n",
		    vap->u_id.if_name, vap->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_vap_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_vap_ctrl_rx(ctrl->rx);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_advance_vap_ctrl_link(ctrl->link);
	}
}

void print_advance_radio_data(struct stats_obj *radio)
{
	struct advance_pdev_data *data = radio->stats;

	STATS_PRINT("Advance Data STATS for Radio %s (Parent %s)\n",
		    radio->u_id.if_name, radio->pif_name);
	if (data->me) {
		STATS_PRINT("ME Stats\n");
		print_advance_radio_data_me(data->me);
	}
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_radio_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_radio_data_rx(data->rx);
	}
	if (data->raw) {
		STATS_PRINT("RAW Stats\n");
		print_advance_radio_data_raw(data->raw);
	}
	if (data->tso) {
		STATS_PRINT("TSO Stats\n");
		print_advance_radio_data_tso(data->tso);
	}
	if (data->vow) {
		STATS_PRINT("VOW Stats\n");
		print_advance_radio_data_vow(data->vow);
	}
	if (data->igmp) {
		STATS_PRINT("IGMP Stats\n");
		print_advance_radio_data_igmp(data->igmp);
	}
	if (data->mesh) {
		STATS_PRINT("MESH Stats\n");
		print_advance_radio_data_mesh(data->mesh);
	}
	if (data->nawds) {
		STATS_PRINT("NAWDS Stats\n");
		print_advance_radio_data_nawds(data->nawds);
	}
}

void print_advance_radio_ctrl(struct stats_obj *radio)
{
	struct advance_pdev_ctrl *ctrl = radio->stats;

	STATS_PRINT("Advance Control STATS for Radio %s (Parent %s)\n",
		    radio->u_id.if_name, radio->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_radio_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_radio_ctrl_rx(ctrl->rx);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_advance_radio_ctrl_link(ctrl->link);
	}
}

void print_advance_ap_data(struct stats_obj *ap)
{
	struct advance_psoc_data *data = ap->stats;

	STATS_PRINT("Advance Data STATS for AP %s\n", ap->u_id.if_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_advance_ap_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_advance_ap_data_rx(data->rx);
	}
}

void print_advance_stats(struct stats_obj *obj)
{
	switch (obj->obj_type) {
	case STATS_OBJ_STA:
		if (obj->type == STATS_TYPE_DATA)
			print_advance_sta_data(obj);
		else
			print_advance_sta_ctrl(obj);
		break;
	case STATS_OBJ_VAP:
		if (obj->type == STATS_TYPE_DATA)
			print_advance_vap_data(obj);
		else
			print_advance_vap_ctrl(obj);
		break;
	case STATS_OBJ_RADIO:
		if (obj->type == STATS_TYPE_DATA)
			print_advance_radio_data(obj);
		else
			print_advance_radio_ctrl(obj);
		break;
	case STATS_OBJ_AP:
		if (obj->type == STATS_TYPE_DATA)
			print_advance_ap_data(obj);
		break;
	default:
		STATS_ERR("Invalid object option\n");
	}
}
#endif /* WLAN_ADVANCE_TELEMETRY */

#if WLAN_DEBUG_TELEMETRY
#ifdef VDEV_PEER_PROTOCOL_COUNT
static void stats_if_print_tx_proto_trace(struct debug_data_tx_stats *tx)
{
	STATS_PRINT("\tTx Data Packets PROTOCOL Based:\n");
	STATS_16(stdout, "ICMP tx ingress",
		 tx->protocol_trace_cnt[STATS_IF_TRACE_ICMP].ingress_cnt);
	STATS_16(stdout, "ICMP tx egress",
		 tx->protocol_trace_cnt[STATS_IF_TRACE_ICMP].egress_cnt);
	STATS_16(stdout, "ARP tx ingress",
		 tx->protocol_trace_cnt[STATS_IF_TRACE_ARP].ingress_cnt);
	STATS_16(stdout, "ARP tx egress",
		 tx->protocol_trace_cnt[STATS_IF_TRACE_ARP].egress_cnt);
	STATS_16(stdout, "EAP tx ingress",
		 tx->protocol_trace_cnt[STATS_IF_TRACE_EAP].ingress_cnt);
	STATS_16(stdout, "EAP tx egress",
		 tx->protocol_trace_cnt[STATS_IF_TRACE_EAP].egress_cnt);
}

static void stats_if_print_rx_proto_trace(struct debug_data_rx_stats *rx)
{
	STATS_PRINT("\tRx Data Packets PROTOCOL Based:\n");
	STATS_16(stdout, "ICMP rx ingress",
		 rx->protocol_trace_cnt[STATS_IF_TRACE_ICMP].ingress_cnt);
	STATS_16(stdout, "ICMP rx egress",
		 rx->protocol_trace_cnt[STATS_IF_TRACE_ICMP].egress_cnt);
	STATS_16(stdout, "ARP rx ingress",
		 rx->protocol_trace_cnt[STATS_IF_TRACE_ARP].ingress_cnt);
	STATS_16(stdout, "ARP rx egress",
		 rx->protocol_trace_cnt[STATS_IF_TRACE_ARP].egress_cnt);
	STATS_16(stdout, "EAP rx ingress",
		 rx->protocol_trace_cnt[STATS_IF_TRACE_EAP].ingress_cnt);
	STATS_16(stdout, "EAP rx egress",
		 rx->protocol_trace_cnt[STATS_IF_TRACE_EAP].egress_cnt);
}
#else
static void stats_if_print_tx_proto_trace(struct debug_data_tx_stats *tx)
{
}

static void stats_if_print_rx_proto_trace(struct debug_data_rx_stats *rx)
{
}
#endif /* VDEV_PEER_PROTOCOL_COUNT */

static void stats_if_print_ru_loc(struct debug_data_tx_stats *tx)
{
	uint8_t i;

	STATS_PRINT("\tRU Locations:\n");
	STATS_PRINT("\t\tMSDU:\n");
	for (i = 0; i < STATS_IF_RU_INDEX_MAX; i++) {
		STATS_PRINT("\t\t%s:  %u\n", ru_string[i].ru_type,
			    tx->ru_loc[i].num_msdu);
	}
	STATS_PRINT("\t\tMPDU:\n");
	for (i = 0; i < STATS_IF_RU_INDEX_MAX; i++) {
		STATS_PRINT("\t\t%s:  %u\n", ru_string[i].ru_type,
			    tx->ru_loc[i].num_mpdu);
	}
	STATS_PRINT("\t\tMPDU Tried:\n");
	for (i = 0; i < STATS_IF_RU_INDEX_MAX; i++) {
		STATS_PRINT("\t\t%s:  %u\n", ru_string[i].ru_type,
			    tx->ru_loc[i].mpdu_tried);
	}
}

void print_debug_data_tx_stats(struct debug_data_tx_stats *tx)
{
	char *mu_group_id;
	uint8_t i, mcs, ptype;
	uint32_t j, index;

	STATS_32(stdout, "Inactive time in seconds", tx->inactive_time);
	STATS_32(stdout, "Total packets as Ofdma", tx->ofdma);
	STATS_32(stdout, "Total packets in STBC", tx->stbc);
	STATS_32(stdout, "Total packets in LDPC", tx->ldpc);
	STATS_32(stdout, "Number of PPDU's with Punctured Preamble",
		 tx->pream_punct_cnt);
	STATS_32(stdout, "Number of stand alone received",
		 tx->num_ppdu_cookie_valid);
	STATS_PRINT("\tPackets dropped on the Tx side:\n");
	STATS_64(stdout, "Firmware discarded packets", tx->fw_rem.num);
	STATS_64(stdout, "Firmware discarded bytes", tx->fw_rem.bytes);
	STATS_32(stdout, "Firmware discard untransmitted", tx->fw_rem_notx);
	STATS_32(stdout, "Firmware discard transmitted", tx->fw_rem_tx);
	STATS_32(stdout, "Aged out in mpdu/msdu queues", tx->age_out);
	STATS_32(stdout, "Firmware discard untransmitted fw_reason1",
		 tx->fw_reason1);
	STATS_32(stdout, "Firmware discard untransmitted fw_reason2",
		 tx->fw_reason2);
	STATS_32(stdout, "Firmware discard untransmitted fw_reason3",
		 tx->fw_reason3);
	STATS_PRINT("\tLast Packet RU index [%u], Size [%u]\n",
		    tx->ru_start, tx->ru_tones);
	STATS_PRINT("\tTx Data Packets per AC\n");
	STATS_32(stdout, "     Best effort",
		 tx->wme_ac_type[STATS_IF_WME_AC_BE]);
	STATS_32(stdout, "      Background",
		 tx->wme_ac_type[STATS_IF_WME_AC_BK]);
	STATS_32(stdout, "           Video",
		 tx->wme_ac_type[STATS_IF_WME_AC_VI]);
	STATS_32(stdout, "           Voice",
		 tx->wme_ac_type[STATS_IF_WME_AC_VO]);
	STATS_PRINT("\tExcess Retries per AC\n");
	STATS_32(stdout, "     Best effort",
		 tx->excess_retries_per_ac[STATS_IF_WME_AC_BE]);
	STATS_32(stdout, "      Background",
		 tx->excess_retries_per_ac[STATS_IF_WME_AC_BK]);
	STATS_32(stdout, "           Video",
		 tx->excess_retries_per_ac[STATS_IF_WME_AC_VI]);
	STATS_32(stdout, "           Voice",
		 tx->excess_retries_per_ac[STATS_IF_WME_AC_VO]);
	STATS_PRINT("\tTransmit Type:\n");
	STATS_PRINT("\tMSDU:SU %u, MU_MIMO %u, MU_OFDMA %u, MU_MIMO_OFDMA %u\n",
		    tx->transmit_type[STATS_IF_SU].num_msdu,
		    tx->transmit_type[STATS_IF_MU_MIMO].num_msdu,
		    tx->transmit_type[STATS_IF_MU_OFDMA].num_msdu,
		    tx->transmit_type[STATS_IF_MU_MIMO_OFDMA].num_msdu);
	STATS_PRINT("\tMPDU:SU %u, MU_MIMO %u, MU_OFDMA %u, MU_MIMO_OFDMA %u\n",
		    tx->transmit_type[STATS_IF_SU].num_mpdu,
		    tx->transmit_type[STATS_IF_MU_MIMO].num_mpdu,
		    tx->transmit_type[STATS_IF_MU_OFDMA].num_mpdu,
		    tx->transmit_type[STATS_IF_MU_MIMO_OFDMA].num_mpdu);
	STATS_PRINT("\tMPDU Tried:");
	STATS_PRINT("SU %u, MU_MIMO %u, MU_OFDMA %u, MU_MIMO_OFDMA %u\n",
		    tx->transmit_type[STATS_IF_SU].mpdu_tried,
		    tx->transmit_type[STATS_IF_MU_MIMO].mpdu_tried,
		    tx->transmit_type[STATS_IF_MU_OFDMA].mpdu_tried,
		    tx->transmit_type[STATS_IF_MU_MIMO_OFDMA].mpdu_tried);
	mu_group_id = malloc(MAX_STRING_LEN);
	if (!mu_group_id) {
		STATS_PRINT("Failed to allocate byffer of %d bytes!\n",
			    MAX_STRING_LEN);
		return;
	}
	for (i = 0; i < STATS_IF_MAX_MU_GROUP_ID;) {
		index = 0;
		memset(mu_group_id, 0, MAX_STRING_LEN);
		for (j = 0; j < STATS_IF_MU_GROUP_SHOW &&
		     i < STATS_IF_MAX_MU_GROUP_ID; j++) {
			index += snprintf(&mu_group_id[index],
					  MAX_STRING_LEN - index,
					  " %d", tx->mu_group_id[i]);
			i++;
		}
		STATS_PRINT("\tUser position list for GID %02u->%u: [%s]\n",
			    i - STATS_IF_MU_GROUP_SHOW, i - 1, mu_group_id);
	}
	free(mu_group_id);
	STATS_PRINT("\tTx MCS stats:\n");
	for (ptype = 0; ptype < STATS_IF_DOT11_MAX; ptype++) {
		for (mcs = 0; mcs < STATS_IF_MAX_MCS; mcs++) {
			if (rate_string[ptype][mcs].valid)
				STATS_PRINT("\t\t %s = %u\n",
					    rate_string[ptype][mcs].mcs_type,
					    tx->pkt_type[ptype].mcs_count[mcs]);
		}
	}

	stats_if_print_ru_loc(tx);
	stats_if_print_tx_proto_trace(tx);

	STATS_PRINT("\tTx Ack not recevied counter:\n");
	for (ptype = STATS_IF_PROTO_EAPOL_M1;
	     ptype < STATS_IF_PROTO_SUBTYPE_MAX; ptype++) {
		if (tx->no_ack_count[ptype])
			STATS_PRINT("\t\t%s = %u\n",
				    proto_subtype_string[ptype],
				    tx->no_ack_count[ptype]);
	}
}

void print_debug_data_rx_stats(struct debug_data_rx_stats *rx)
{
	uint32_t *pnss;
	char *nss;
	uint8_t i, mcs, ptype;
	uint32_t index;

	STATS_32(stdout, "Rx Dropped", rx->rx_discard);
	STATS_32(stdout, "Rx MIC Error", rx->mic_err);
	STATS_32(stdout, "Rx Decrypt Error", rx->decrypt_err);
	STATS_32(stdout, "Rx FCS Error", rx->fcserr);
	STATS_32(stdout, "Rx PN Error", rx->pn_err);
	STATS_32(stdout, "Rx Out of Order Error", rx->oor_err);
	STATS_64(stdout, "Rx MEC drop packets", rx->mec_drop.num);
	STATS_64(stdout, "Rx MEC drop bytes", rx->mec_drop.bytes);
	STATS_PRINT("\tRx MSDU Reception Type:\n");
	STATS_PRINT("\t\tSU %u MU_MIMO %u MU_OFDMA %u MU_OFDMA_MIMO %u\n",
		    rx->reception_type[STATS_IF_SU],
		    rx->reception_type[STATS_IF_MU_MIMO],
		    rx->reception_type[STATS_IF_MU_OFDMA],
		    rx->reception_type[STATS_IF_MU_MIMO_OFDMA]);
	STATS_PRINT("\tRx PPDU Reception Type:\n");
	STATS_PRINT("\t\tSU %u MU_MIMO %u MU_OFDMA %u MU_OFDMA_MIMO %u\n",
		    rx->ppdu_cnt[STATS_IF_SU],
		    rx->ppdu_cnt[STATS_IF_MU_MIMO],
		    rx->ppdu_cnt[STATS_IF_MU_OFDMA],
		    rx->ppdu_cnt[STATS_IF_MU_MIMO_OFDMA]);
	STATS_PRINT("\tRx PPDU MU Counts:\n");
	for (ptype = 0; ptype < STATS_IF_TXRX_TYPE_MU_MAX; ptype++) {
		for (mcs = 0; mcs < STATS_IF_MAX_MCS; mcs++) {
			if (!mu_rate_string[ptype][mcs].valid)
				continue;
			STATS_PRINT("\t\t%s = %d\n",
				    mu_rate_string[ptype][mcs].mcs_type,
				    rx->rx_mu[ptype].ppdu.mcs_count[mcs]);
		}
	}
	nss = malloc(MAX_STRING_LEN);
	if (!nss) {
		STATS_PRINT("Failed to allocate buffer of %d zize!\n",
			    MAX_STRING_LEN);
		return;
	}
	for (ptype = 0; ptype < STATS_IF_TXRX_TYPE_MU_MAX; ptype++) {
		STATS_PRINT("\tReception mode %s\n", mu_reception_mode[ptype]);
		pnss = &rx->rx_mu[ptype].ppdu_nss[0];
		index = 0;
		memset(nss, 0, MAX_STRING_LEN);
		for (i = 0; i < STATS_IF_SS_COUNT; i++) {
			index += snprintf(&nss[index],
					  MAX_STRING_LEN - index,
					  " %u", *(pnss + i));
		}
		STATS_PRINT("\t\t\tNSS(1-8) = %s\n", nss);
		STATS_PRINT("\t\t\tMPDU OK = %u, MPDU Fail = %u\n",
			    rx->rx_mu[ptype].mpdu_cnt_fcs_ok,
			    rx->rx_mu[ptype].mpdu_cnt_fcs_err);
	}
	free(nss);
	STATS_PRINT("\tRx MCS stats:\n");
	for (ptype = 0; ptype < STATS_IF_DOT11_MAX; ptype++) {
		for (mcs = 0; mcs < STATS_IF_MAX_MCS; mcs++) {
			if (rate_string[ptype][mcs].valid)
				STATS_PRINT("\t\t %s = %u\n",
					    rate_string[ptype][mcs].mcs_type,
					    rx->pkt_type[ptype].mcs_count[mcs]);
		}
	}

	stats_if_print_rx_proto_trace(rx);

	for (i = 0; i <  STATS_IF_MAX_RX_RINGS; i++) {
		STATS_PRINT("\tRing Id = %u", i);
		STATS_64(stdout, "Packets Received", rx->rcvd_reo[i].num);
		STATS_64(stdout, "\t\tBytes Received", rx->rcvd_reo[i].bytes);
	}
}

void print_debug_sta_data_tx(struct debug_peer_data_tx *tx)
{
	print_basic_sta_data_tx(&tx->b_tx);
	STATS_PRINT("\tTx Stats in Last One Second:\n");
	STATS_32(stdout, "Tx success packets", tx->tx_data_success_last);
	STATS_32(stdout, "Tx success bytes", tx->tx_bytes_success_last);
	STATS_32(stdout, "Tx packets", tx->tx_data_rate);
	STATS_32(stdout, "Tx bytes", tx->tx_byte_rate);
	STATS_32(stdout, "Tx Packet Error Rate (PER)", tx->last_per);
	STATS_32(stdout, "Tx unicast data", tx->tx_data_ucast_last);
	STATS_32(stdout, "Tx unicast data rate", tx->tx_data_ucast_rate);
	STATS_PRINT("\n");
	print_debug_data_tx_stats(&tx->dbg_tx);
}

void print_debug_sta_data_rx(struct debug_peer_data_rx *rx)
{
	print_basic_sta_data_rx(&rx->b_rx);
	STATS_PRINT("\tRx Stats in Last One Second:\n");
	STATS_32(stdout, "Rx packets", rx->rx_data_success_last);
	STATS_32(stdout, "Rx bytes", rx->rx_bytes_success_last);
	STATS_32(stdout, "Rx packets", rx->rx_data_rate);
	STATS_32(stdout, "Rx bytes", rx->rx_byte_rate);
	STATS_PRINT("\n");
	print_debug_data_rx_stats(&rx->dbg_rx);
}

void print_debug_sta_data_link(struct debug_peer_data_link *link)
{
	print_basic_sta_data_link(&link->b_link);
	STATS_32(stdout, "Last ack rssi", link->last_ack_rssi);
}

void print_debug_sta_data_rate(struct debug_peer_data_rate *rate)
{
	print_basic_sta_data_rate(&rate->b_rate);
	STATS_32(stdout, "Last Tx rate for unicast Packets(mcs)",
		 rate->last_tx_rate_mcs);
	STATS_32(stdout, "Last Tx rate for multicast Packets",
		 rate->mcast_last_tx_rate);
	STATS_32(stdout, "Last Tx rate for multicast Packets(mcs)",
		 rate->mcast_last_tx_rate_mcs);
}

void print_debug_sta_data_txcap(struct debug_peer_data_txcap *txcap)
{
	uint8_t tid;

	for (tid = 0; tid < STATS_IF_MAX_TIDS; tid++) {
		STATS_PRINT("\tTID[%u]: defer_msdu_q[%ju] ", tid,
			    txcap->defer_msdu_len[tid]);
		STATS_PRINT("msdu_comp_q[%ju] pending_ppdu_q[%ju]\n",
			    txcap->tasklet_msdu_len[tid],
			    txcap->pending_q_len[tid]);
	}
	STATS_PRINT("\tMSDU SUCC:%u ENQ:%u DEQ:%u FLUSH:%u DROP:%u XRETRY:%u\n",
		    txcap->msdu[STATS_IF_MSDU_SUCC],
		    txcap->msdu[STATS_IF_MSDU_ENQ],
		    txcap->msdu[STATS_IF_MSDU_DEQ],
		    txcap->msdu[STATS_IF_MSDU_FLUSH],
		    txcap->msdu[STATS_IF_MSDU_DROP],
		    txcap->msdu[STATS_IF_MSDU_XRETRY]);
	STATS_PRINT("\tMPDU TRI:%u SUCC:%u RESTITCH:%u ARR:%u CLONE:%u",
		    txcap->mpdu[STATS_IF_MPDU_TRI],
		    txcap->mpdu[STATS_IF_MPDU_SUCC],
		    txcap->mpdu[STATS_IF_MPDU_RESTITCH],
		    txcap->mpdu[STATS_IF_MPDU_ARR],
		    txcap->mpdu[STATS_IF_MPDU_CLONE]);
	STATS_PRINT(" TO STACK:%u\n", txcap->mpdu[STATS_IF_MPDU_TO_STACK]);
}

void print_debug_sta_deter_stats(struct debug_peer_data_deter *deter)
{
	uint8_t tid, msdu;
	struct stats_if_deter *deter_stats;

	for (tid = 0; tid < STATS_IF_DATA_TID_MAX; tid++) {
		deter_stats = &deter->deter_stats[tid];
		STATS_PRINT("----TID: %u----\n", tid);
		STATS_PRINT(" Downlink: \n");
		for (msdu = 0; msdu < STATS_IF_MSDUQ_MAX; msdu++) {
			STATS_PRINT("MSDUQ[%d]:\n", msdu);
			STATS_PRINT("DL_SU: \n");
			STATS_32(stdout, "cnt",
				 deter_stats->dl[msdu][0].mode_cnt);
			STATS_64(stdout, "avg rate",
				 deter_stats->dl[msdu][0].avg_rate);
			STATS_PRINT("DL_MUOFDMA: \n");
			STATS_32(stdout, "cnt",
				 deter_stats->dl[msdu][1].mode_cnt);
			STATS_64(stdout, "avg rate",
				    deter_stats->dl[msdu][1].avg_rate);
			STATS_PRINT("DL_MUMIMO: \n");
			STATS_32(stdout, "cnt",
				 deter_stats->dl[msdu][2].mode_cnt);
			STATS_64(stdout, "avg rate",
				 deter_stats->dl[msdu][2].avg_rate);
		}
		STATS_PRINT("Uplink: \n");
		STATS_PRINT("UL MUOFDMA: \n");
		STATS_32(stdout, "cnt",
			 deter_stats->ul[0].mode_cnt);
		STATS_64(stdout, "avg rate",
			 deter_stats->ul[0].avg_rate);
		STATS_32(stdout, "trigger success",
			 deter_stats->ul[0].trigger_success);
		STATS_32(stdout, "trigger fail",
			 deter_stats->ul[0].trigger_fail);
		STATS_PRINT("UL MUMIMO: \n");
		STATS_32(stdout, "cnt",
			 deter_stats->ul[1].mode_cnt);
		STATS_64(stdout, "avg rate",
			 deter_stats->ul[1].avg_rate);
		STATS_32(stdout, "trigger success",
			 deter_stats->ul[1].trigger_success);
		STATS_32(stdout, "trigger fail",
			 deter_stats->ul[1].trigger_fail);
		STATS_PRINT("UL MUBAR: \n");
		STATS_32(stdout, "cnt",
			 deter_stats->ul[2].mode_cnt);
		STATS_64(stdout, "avg rate",
			 deter_stats->ul[2].avg_rate);
		STATS_32(stdout, "trigger success",
			 deter_stats->ul[2].trigger_success);
		STATS_32(stdout, "trigger fail",
			 deter_stats->ul[2].trigger_fail);
		STATS_PRINT("RX: \n");
		STATS_PRINT("SU: \n");
		STATS_32(stdout, "cnt",
			 deter_stats->rx_det.mode_cnt);
		STATS_64(stdout, "avg rate",
			 deter_stats->rx_det.avg_rate);
	}
}

void print_debug_sta_ctrl_tx(struct debug_peer_ctrl_tx *tx)
{
	print_basic_sta_ctrl_tx(&tx->b_tx);
	STATS_32(stdout, "Tx Packets Discard as Power save aged",
		 tx->cs_ps_discard);
	STATS_32(stdout, "Tx Packets dropped form PS queue", tx->cs_psq_drops);
	STATS_32(stdout, "Tx RTS Success Count", tx->rts_success);
	STATS_32(stdout, "Tx RTS Failure Count", tx->rts_failure);
	STATS_32(stdout, "Tx Block Ack Request(BAR) Count", tx->bar_cnt);
	STATS_32(stdout, "Tx NDP Announcement Count", tx->ndpa_cnt);
}

void print_debug_sta_ctrl_rx(struct debug_peer_ctrl_rx *rx)
{
	print_basic_sta_ctrl_rx(&rx->b_rx);
	STATS_32(stdout, "Rx Block Ack Request(BAR) Count", rx->bar_cnt);
	STATS_32(stdout, "Rx NDP Announcement Count", rx->ndpa_cnt);
}

void print_debug_sta_ctrl_link(struct debug_peer_ctrl_link *link)
{
	print_basic_sta_ctrl_link(&link->b_link);
}

void print_debug_sta_ctrl_rate(struct debug_peer_ctrl_rate *rate)
{
	print_basic_sta_ctrl_rate(&rate->b_rate);
}

void print_debug_sta_data(struct stats_obj *sta)
{
	struct debug_peer_data *data = sta->stats;

	STATS_PRINT("Debug Data STATS For STA %s (Parent %s)\n",
		    macaddr_to_str(sta->u_id.mac_addr), sta->pif_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_sta_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_sta_data_rx(data->rx);
	}
	if (data->link) {
		STATS_PRINT("Link Stats\n");
		print_debug_sta_data_link(data->link);
	}
	if (data->rate) {
		STATS_PRINT("Rate Stats\n");
		print_debug_sta_data_rate(data->rate);
	}
	if (data->txcap) {
		STATS_PRINT("Tx Capture Stats\n");
		print_debug_sta_data_txcap(data->txcap);
	}
	if (data->deter) {
		STATS_PRINT("Deterministic Stats\n");
		print_debug_sta_deter_stats(data->deter);
	}
}

void print_debug_sta_ctrl(struct stats_obj *sta)
{
	struct debug_peer_ctrl *ctrl = sta->stats;

	STATS_PRINT("Debug Control STATS For STA %s (Parent %s)\n",
		    macaddr_to_str(sta->u_id.mac_addr), sta->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_sta_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_sta_ctrl_rx(ctrl->rx);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_debug_sta_ctrl_link(ctrl->link);
	}
	if (ctrl->rate) {
		STATS_PRINT("Rate Stats\n");
		print_debug_sta_ctrl_rate(ctrl->rate);
	}
}

void print_debug_vap_data_me(struct debug_vdev_data_me *me)
{
	STATS_32(stdout, "Packets dropped due to map error",
		 me->dropped_map_error);
	STATS_32(stdout, "Packets dropped due to self Mac address",
		 me->dropped_self_mac);
	STATS_32(stdout, "Packets dropped due to send fail",
		 me->dropped_send_fail);
	STATS_32(stdout, "Segment allocation failure", me->fail_seg_alloc);
	STATS_32(stdout, "NBUF clone failure", me->clone_fail);
}

void print_debug_vap_data_tx(struct debug_vdev_data_tx *tx)
{
	print_basic_vap_data_tx(&tx->b_tx);
	print_debug_data_tx_stats(&tx->dbg_tx);
	STATS_PRINT("\tPackets dropped on the Tx ingress side:\n");
	STATS_64(stdout, "Packets dropped Desc Not Available", tx->desc_na.num);
	STATS_64(stdout, "Bytes dropped Desc Not Available", tx->desc_na.bytes);
	STATS_64(stdout, "Packets dropped Descriptor alloc fail",
		 tx->desc_na_exc_alloc_fail.num);
	STATS_64(stdout, "Bytes dropped Descriptor alloc fail",
		 tx->desc_na_exc_alloc_fail.bytes);
	STATS_64(stdout, "Packets dropped Tx outstanding too many",
		 tx->desc_na_exc_outstand.num);
	STATS_64(stdout, "Bytes dropped Tx outstanding too many",
		 tx->desc_na_exc_outstand.bytes);
	STATS_64(stdout, "Packets dropped as Exception pkts more than limit",
		 tx->exc_desc_na.num);
	STATS_64(stdout, "Bytes dropped as Exception pkts more than limit",
		 tx->exc_desc_na.bytes);
	STATS_64(stdout, "Packets dropped due to sniffer received",
		 tx->sniffer_rcvd.num);
	STATS_64(stdout, "Bytes dropped due to sniffer received",
		 tx->sniffer_rcvd.bytes);
	STATS_32(stdout, "Packets dropped due to Ring full", tx->ring_full);
	STATS_32(stdout, "Packets dropped due to HW enqueue failed",
		 tx->enqueue_fail);
	STATS_32(stdout, "Packets dropped due to DMA error", tx->dma_error);
	STATS_32(stdout, "Packets dropped due to Resource Full", tx->res_full);
	STATS_32(stdout, "Packets dropped due to Insufficient Headroom",
		 tx->headroom_insufficient);
	STATS_32(stdout, "Packets dropped due to per Pkt vdev id check fail",
		 tx->fail_per_pkt_vdev_id_check);
}

void print_debug_vap_data_rx(struct debug_vdev_data_rx *rx)
{
	print_basic_vap_data_rx(&rx->b_rx);
	print_debug_data_rx_stats(&rx->dbg_rx);
}

void print_debug_vap_data_raw(struct debug_vdev_data_raw *raw)
{
	STATS_32(stdout, "DMA map error", raw->dma_map_error);
	STATS_32(stdout, "Packet type not data error",
		 raw->invalid_raw_pkt_datatype);
	STATS_32(stdout, "Frags count overflow error",
		 raw->num_frags_overflow_err);
}

void print_debug_vap_data_tso(struct debug_vdev_data_tso *tso)
{
	STATS_64(stdout, "Packets dropped by host", tso->dropped_host.num);
	STATS_64(stdout, "Bytes dropped by host", tso->dropped_host.bytes);
	STATS_32(stdout, "TSO DMA map error", tso->dma_map_error);
	STATS_32(stdout, "Packets dropped by target", tso->dropped_target);
}

void print_debug_vap_ctrl_tx(struct debug_vdev_ctrl_tx *tx)
{
	print_basic_vap_ctrl_tx(&tx->b_tx);
	STATS_64(stdout, "Tx SWBA Beacon interval Counter", tx->cs_tx_bcn_swba);
	STATS_64(stdout, "Tx failed due to no Node", tx->cs_tx_nonode);
	STATS_64(stdout, "Tx Not ok set in descriptor", tx->cs_tx_not_ok);
}

void print_debug_vap_ctrl_rx(struct debug_vdev_ctrl_rx *rx)
{
	print_basic_vap_ctrl_rx(&rx->b_rx);
	STATS_64(stdout, "Invalid mac address: node alloc failure",
		 rx->cs_invalid_macaddr_nodealloc_fail);
	STATS_64(stdout, "Packets due to wrong direction",
		 rx->cs_rx_wrongdir);
	STATS_64(stdout, "Packets dropped due to sta not associated",
		 rx->cs_rx_not_assoc);
	STATS_64(stdout, "Packets dropped due to rateset truncated",
		 rx->cs_rx_rs_too_big);
	STATS_64(stdout, "Packets dropped due to required element missing",
		 rx->cs_rx_elem_missing);
	STATS_64(stdout, "Packets dropped due to element too big",
		 rx->cs_rx_elem_too_big);
	STATS_64(stdout, "Packets with invalid channel dropped",
		 rx->cs_rx_chan_err);
	STATS_64(stdout, "Packets dropped due to node allocation failure",
		 rx->cs_rx_node_alloc);
	STATS_64(stdout, "Packets with unsupported Auth algo dropped",
		 rx->cs_rx_auth_unsupported);
	STATS_64(stdout, "STA Auth failure", rx->cs_rx_auth_fail);
	STATS_64(stdout, "Auth discard due to counter measure",
		 rx->cs_rx_auth_countermeasures);
	STATS_64(stdout, "Assoc from wrong BSS", rx->cs_rx_assoc_bss);
	STATS_64(stdout, "Auth without assoc", rx->cs_rx_assoc_notauth);
	STATS_64(stdout, "Assoc with Capabilities mismatch",
		 rx->cs_rx_assoc_cap_mismatch);
	STATS_64(stdout, "Assoc with no rate match", rx->cs_rx_assoc_norate);
	STATS_64(stdout, "Assoc with bad WPA IE", rx->cs_rx_assoc_wpaie_err);
	STATS_64(stdout, "Bad auth request", rx->cs_rx_auth_err);
	STATS_64(stdout, "Rx discarded due to ACL policy", rx->cs_rx_acl);
	STATS_64(stdout, "Rx 4-addressed pkts while WDS disable",
		 rx->cs_rx_nowds);
	STATS_64(stdout, "Rx from wrong BSSID", rx->cs_rx_wrongbss);
	STATS_64(stdout, "Rx packet length too short", rx->cs_rx_tooshort);
	STATS_64(stdout, "Rx SSID mismatch", rx->cs_rx_ssid_mismatch);
	STATS_64(stdout, "Rx Ucast Dycrypt Ok", rx->cs_rx_decryptok_u);
	STATS_64(stdout, "Rx Mcast Dycrypt Ok", rx->cs_rx_decryptok_m);
}

void print_debug_vap_ctrl_wmi(struct debug_vdev_ctrl_wmi *wmi)
{
	STATS_64(stdout, "Peer delete req", wmi->cs_peer_delete_req);
	STATS_64(stdout, "Peer delete resp", wmi->cs_peer_delete_resp);
	STATS_64(stdout, "Peer delete all req", wmi->cs_peer_delete_all_req);
	STATS_64(stdout, "Peer delete all resp", wmi->cs_peer_delete_all_resp);
}

void print_debug_vap_ctrl_link(struct debug_vdev_ctrl_link *link)
{
	print_basic_vap_ctrl_link(&link->b_link);
}

void print_debug_vap_data(struct stats_obj *vap)
{
	struct debug_vdev_data *data = vap->stats;

	STATS_PRINT("Debug Data STATS for Vap %s (Parent %s)\n",
		    vap->u_id.if_name, vap->pif_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_vap_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_vap_data_rx(data->rx);
	}
	if (data->me) {
		STATS_PRINT("ME Stats\n");
		print_debug_vap_data_me(data->me);
	}
	if (data->raw) {
		STATS_PRINT("RAW Stats\n");
		print_debug_vap_data_raw(data->raw);
	}
	if (data->tso) {
		STATS_PRINT("TSO Stats\n");
		print_debug_vap_data_tso(data->tso);
	}
}

void print_debug_vap_ctrl(struct stats_obj *vap)
{
	struct debug_vdev_ctrl *ctrl = vap->stats;

	STATS_PRINT("Debug Control STATS for Vap %s (Parent %s)\n",
		    vap->u_id.if_name, vap->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_vap_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_vap_ctrl_rx(ctrl->rx);
	}
	if (ctrl->wmi) {
		STATS_PRINT("WMI Stats\n");
		print_debug_vap_ctrl_wmi(ctrl->wmi);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_debug_vap_ctrl_link(ctrl->link);
	}
}

void print_debug_radio_data_me(struct debug_pdev_data_me *me)
{
	STATS_32(stdout, "Packets dropped due to map error",
		 me->dropped_map_error);
	STATS_32(stdout, "Packets dropped due to self Mac address",
		 me->dropped_self_mac);
	STATS_32(stdout, "Packets dropped due to send fail",
		 me->dropped_send_fail);
	STATS_32(stdout, "Segment allocation failure", me->fail_seg_alloc);
	STATS_32(stdout, "NBUF clone failure", me->clone_fail);
}

void print_debug_radio_data_tx(struct debug_pdev_data_tx *tx)
{
	print_basic_radio_data_tx(&tx->b_tx);
	print_debug_data_tx_stats(&tx->dbg_tx);
}

void print_debug_radio_data_rx(struct debug_pdev_data_rx *rx)
{
	print_basic_radio_data_rx(&rx->b_rx);
	print_debug_data_rx_stats(&rx->dbg_rx);
	STATS_PRINT("\tRx Packets dropped:\n");
	STATS_64(stdout, "Total Packets replenished", rx->replenished_pkts.num);
	STATS_64(stdout, "Total Bytes replenished", rx->replenished_pkts.bytes);
	STATS_32(stdout, "Rx DMA Error replenished", rx->rxdma_err);
	STATS_32(stdout, "NBUG allocation failed", rx->nbuf_alloc_fail);
	STATS_32(stdout, "Frag allocation failed", rx->frag_alloc_fail);
	STATS_32(stdout, "MAP error", rx->map_err);
	STATS_32(stdout, "x86 failure", rx->x86_fail);
	STATS_32(stdout, "Low threshold interrupts", rx->low_thresh_intrs);
	STATS_32(stdout, "Packets dropped because msdu_done bit not set",
		 rx->msdu_not_done);
	STATS_32(stdout, "Multicast Echo check", rx->mec);
	STATS_32(stdout, "Mesh Filtered packets", rx->mesh_filter);
	STATS_32(stdout, "Wifi Parse failed", rx->wifi_parse);
	STATS_32(stdout, "Mon Rx drop", rx->mon_rx_drop);
	STATS_32(stdout, "Mon Radio Tap error drop",
		 rx->mon_radiotap_update_err);
	STATS_32(stdout, "Descriptor alloc fail", rx->desc_alloc_fail);
	STATS_32(stdout, "IP checksum error", rx->ip_csum_err);
	STATS_32(stdout, "TCP/UDP checksum error", rx->tcp_udp_csum_err);
	STATS_32(stdout, "Rx HW error", rx->reo_error);
	STATS_PRINT("\n");
	STATS_32(stdout, "Buffer added back in freelist", rx->buf_freelist);
	STATS_PRINT("\tSent To Stack:\n");
	STATS_32(stdout, "Vlan tagged stp packets in wifi parse error",
		 rx->vlan_tag_stp_cnt);
}

void print_debug_radio_data_raw(struct debug_pdev_data_raw *raw)
{
	STATS_32(stdout, "DMA map error", raw->dma_map_error);
	STATS_32(stdout, "Packet type not data error",
		 raw->invalid_raw_pkt_datatype);
	STATS_32(stdout, "Frags count overflow error",
		 raw->num_frags_overflow_err);
}

void print_debug_radio_data_tso(struct debug_pdev_data_tso *tso)
{
	STATS_64(stdout, "Packets dropped by host", tso->dropped_host.num);
	STATS_64(stdout, "Bytes dropped by host", tso->dropped_host.bytes);
	STATS_64(stdout, "Packets dropped due to no memory",
		 tso->tso_no_mem_dropped.num);
	STATS_64(stdout, "Bytes dropped due to no memory",
		 tso->tso_no_mem_dropped.bytes);
	STATS_32(stdout, "Packets dropped by target", tso->dropped_target);
}

static const char *capture_status_to_str(enum stats_if_chan_capture_status type)
{
	switch (type) {
	case STATS_IF_CAPTURE_IDLE:
		return "CAPTURE_IDLE";
	case STATS_IF_CAPTURE_BUSY:
		return "CAPTURE_BUSY";
	case STATS_IF_CAPTURE_ACTIVE:
		return "CAPTURE_ACTIVE";
	case STATS_IF_CAPTURE_NO_BUFFER:
		return "CAPTURE_NO_BUFFER";
	default:
		return "INVALID";
	}
}

static const
char *freeze_reason_to_str(enum stats_if_freeze_capture_reason type)
{
	switch (type) {
	case STATS_IF_FREEZE_REASON_TM:
		return "FREEZE_REASON_TM";
	case STATS_IF_FREEZE_REASON_FTM:
		return "FREEZE_REASON_FTM";
	case STATS_IF_FREEZE_REASON_ACK_RESP_TO_TM_FTM:
		return "FREEZE_REASON_ACK_RESP_TO_TM_FTM";
	case STATS_IF_FREEZE_REASON_TA_RA_TYPE_FILTER:
		return "FREEZE_REASON_TA_RA_TYPE_FILTER";
	case STATS_IF_FREEZE_REASON_NDPA_NDP:
		return "FREEZE_REASON_NDPA_NDP";
	case STATS_IF_FREEZE_REASON_ALL_PACKET:
		return "FREEZE_REASON_ALL_PACKET";
	default:
		return "INVALID";
	}
}

void print_debug_radio_data_cfr(struct debug_pdev_data_cfr *cfr)
{
	uint8_t cnt;

	STATS_64(stdout, "bb_captured_channel_cnt",
		 cfr->bb_captured_channel_cnt);
	STATS_64(stdout, "bb_captured_timeout_cnt",
		 cfr->bb_captured_timeout_cnt);
	STATS_64(stdout, "rx_loc_info_valid_cnt",
		 cfr->rx_loc_info_valid_cnt);
	STATS_PRINT("\tChannel capture status:\n");
	for (cnt = 0; cnt < STATS_IF_CAPTURE_MAX; cnt++)
		STATS_PRINT("\t\t%s = %ju\n", capture_status_to_str(cnt),
			    cfr->chan_capture_status[cnt]);
	STATS_PRINT("\tFreeze reason:\n");
	for (cnt = 0; cnt < STATS_IF_FREEZE_REASON_MAX; cnt++)
		STATS_PRINT("\t\t%s = %ju\n", freeze_reason_to_str(cnt),
			    cfr->reason_cnt[cnt]);
}

void print_debug_radio_data_wdi(struct debug_pdev_data_wdi *wdi)
{
	uint8_t i;

	for (i = 0; i < STATS_IF_WDI_EVENT_LAST; i++) {
		if (wdi->wdi_event[i])
			STATS_PRINT("\tWdi msgs received from fw [%u]: [%u]\n",
				    i, wdi->wdi_event[i]);
	}
}

void print_debug_radio_data_mesh(struct debug_pdev_data_mesh *mesh)
{
	STATS_32(stdout, "Mesh Rx Stats Alloc fail", mesh->mesh_mem_alloc);
}

void print_debug_radio_data_txcap(struct debug_pdev_data_txcap *txcap)
{
	uint8_t i, j;

	STATS_32(stdout, "BA not received for delayed_ba",
		 txcap->delayed_ba_not_recev);
	STATS_32(stdout, "Last received ppdu stats", txcap->last_rcv_ppdu);
	STATS_32(stdout, "PPDU stats queue depth",
		 txcap->ppdu_stats_queue_depth);
	STATS_32(stdout, "PPDU stats defer queue depth",
		 txcap->ppdu_stats_defer_queue_depth);
	STATS_32(stdout, "PPDU dropped", txcap->ppdu_dropped);
	STATS_32(stdout, "Pending PPDU dropped", txcap->pend_ppdu_dropped);
	STATS_32(stdout, "PPDU peer flush counter", txcap->ppdu_flush_count);
	STATS_32(stdout, "PPDU MSDU threshold drop",
		 txcap->msdu_threshold_drop);
	STATS_64(stdout, "Peer mismatch", txcap->peer_mismatch);
	STATS_64(stdout, "defer_msdu_q length", txcap->defer_msdu_len);
	STATS_64(stdout, "msdu_comp_q length", txcap->tasklet_msdu_len);
	STATS_64(stdout, "pending_ppdu_q length", txcap->pending_q_len);
	STATS_64(stdout, "tx_ppdu_proc", txcap->tx_ppdu_proc);
	STATS_64(stdout, "ACK BA comes twice", txcap->ack_ba_comes_twice);
	STATS_64(stdout, "PPDU dropped because of incomplete tlv",
		 txcap->ppdu_drop);
	STATS_64(stdout, "PPDU dropped because of wrap around",
		 txcap->ppdu_wrap_drop);
	STATS_PRINT("\tMgmt control enqueue stats:\n");
	for (i = 0; i < STATS_IF_TXCAP_MAX_TYPE; i++) {
		for (j = 0; j < STATS_IF_TXCAP_MAX_SUBTYPE; j++) {
			if (!txcap->ctl_mgmt_q_len[i][j])
				continue;
			STATS_PRINT("\t\tctl_mgmt_q[%u][%u].len = %ju\n", i, j,
				    txcap->ctl_mgmt_q_len[i][j]);
		}
	}
	STATS_PRINT("\tMgmt control retry queue stats:\n");
	for (i = 0; i < STATS_IF_TXCAP_MAX_TYPE; i++) {
		for (j = 0; j < STATS_IF_TXCAP_MAX_SUBTYPE; j++) {
			if (!txcap->retries_ctl_mgmt_q_len[i][j])
				continue;
			STATS_PRINT("\t\tretries_ctl_mgmt_q[%u][%u].len = %ju",
				    i, j, txcap->retries_ctl_mgmt_q_len[i][j]);
			STATS_PRINT("\n");
		}
	}
	STATS_PRINT("\tHTT Frame Type Stats:\n");
	for (i = 0; i < STATS_IF_TX_CAP_HTT_MAX_FTYPE; i++) {
		if (txcap->htt_frame_type[i])
			STATS_PRINT("\t\tsgen htt frame type[%d] = %d\n",
				    i, txcap->htt_frame_type[i]);
	}
	STATS_PRINT("\tPPDU stats counter:\n");
	for (i = 0; i < STATS_IF_PPDU_STATS_MAX_TAG; i++)
		STATS_PRINT("\t\tTag[%u] = %ju\n",
			    i, txcap->ppdu_stats_counter[i]);
}

void print_debug_radio_data_monitor(struct debug_pdev_data_monitor *monitor)
{
	uint32_t i, idx;
	char *str_buf = malloc(MAX_STRING_LEN);

	if (!monitor->status_ppdu_state)
		STATS_UNVLBL(stdout, "PPDU State", "Start");
	else
		STATS_UNVLBL(stdout, "PPDU State", "End");
	STATS_32(stdout, "ul_ofdma_data_rx_ppdu", monitor->data_rx_ppdu);
	for (i = 0; i < STATS_IF_OFDMA_NUM_USERS; i++) {
		STATS_PRINT("\t\tul_ofdma data %u user = %u\n",
			    i, monitor->data_users[i]);
	}
	STATS_32(stdout, "status_ppdu_start_cnt", monitor->status_ppdu_start);
	STATS_32(stdout, "status_ppdu_end_cnt", monitor->status_ppdu_end);
	STATS_32(stdout, "status_ppdu_start_mis_cnt",
		 monitor->status_ppdu_compl);
	STATS_32(stdout, "status_ppdu_start_mis_cnt",
		 monitor->status_ppdu_start_mis);
	STATS_32(stdout, "status_ppdu_end_mis_cnt",
		 monitor->status_ppdu_end_mis);
	STATS_32(stdout, "status_ppdu_done_cnt", monitor->status_ppdu_done);
	STATS_32(stdout, "dest_ppdu_done_cnt", monitor->dest_ppdu_done);
	STATS_32(stdout, "dest_mpdu_done_cnt", monitor->dest_mpdu_done);
	STATS_32(stdout, "dest_mpdu_drop_cnt", monitor->dest_mpdu_drop);
	STATS_32(stdout, "dup_mon_linkdesc_cnt", monitor->dup_mon_linkdesc_cnt);
	STATS_32(stdout, "dup_mon_buf_cnt", monitor->dup_mon_buf_cnt);
	idx = monitor->ppdu_id_hist_idx;
	STATS_PRINT("\tPPDU Id history:\n");
	STATS_PRINT("\t\tstat_ring_ppdu_ids\t dest_ring_ppdu_ids\n");
	for (i = 0; i < STATS_IF_MAX_PPDU_ID_HIST; i++) {
		idx = (idx + 1) & (STATS_IF_MAX_PPDU_ID_HIST - 1);
		STATS_PRINT("\t\t%*u\t%*u\n", 16,
			    monitor->stat_ring_ppdu_id_hist[idx], 16,
			    monitor->dest_ring_ppdu_id_hist[idx]);
	}
	STATS_32(stdout, "mon_rx_dest_stuck", monitor->mon_rx_dest_stuck);
	STATS_32(stdout, "tlv_tag_status_err_cnt", monitor->tlv_tag_status_err);
	STATS_32(stdout, "mon status DMA not done WAR count",
		 monitor->status_buf_done_war);
	STATS_32(stdout, "mon_rx_buf_replenished",
		 monitor->mon_rx_bufs_replenished_dest);
	STATS_32(stdout, "mon_rx_buf_reaped", monitor->mon_rx_bufs_reaped_dest);
	STATS_32(stdout, "ppdu_id_mismatch", monitor->ppdu_id_mismatch);
	STATS_32(stdout, "mpdu_ppdu_id_match_cnt", monitor->ppdu_id_match);
	STATS_32(stdout, "ppdus dropped frm status ring",
		 monitor->status_ppdu_drop);
	STATS_32(stdout, "ppdus dropped frm dest ring",
		 monitor->dest_ppdu_drop);
	STATS_32(stdout, "mon_link_desc_invalid",
		 monitor->mon_link_desc_invalid);
	STATS_32(stdout, "mon_rx_desc_invalid", monitor->mon_rx_desc_invalid);
	STATS_32(stdout, "mon_nbuf_sanity_err", monitor->mon_nbuf_sanity_err);
	STATS_32(stdout, "mpdu_ppdu_id_mismatch_drop", monitor->mpdu_ppdu_id_mismatch_drop);
	if (str_buf) {
		uint32_t index = 0;

		memset(str_buf, 0, MAX_STRING_LEN);
		for (i = 0; i < STATS_IF_OFDMA_NUM_RU_SIZE; i++) {
			index += snprintf(&str_buf[index],
					  MAX_STRING_LEN - index,
					  " %u:%u,", i,
					  monitor->data_rx_ru_size[i]);
		}
		STATS_UNVLBL(stdout, "ul_ofdma_data_rx_ru_size", str_buf);
		index = 0;
		memset(str_buf, 0, MAX_STRING_LEN);
		for (i = 0; i < STATS_IF_OFDMA_NUM_RU_SIZE; i++) {
			index += snprintf(&str_buf[index],
					  MAX_STRING_LEN - index,
					  " %u:%u,", i,
					  monitor->nondata_rx_ru_size[i]);
		}
		STATS_UNVLBL(stdout, "ul_ofdma_nodata_rx_ru_size", str_buf);

		free(str_buf);
	}
}

void print_debug_radio_deter_stats(struct debug_pdev_data_deter *deter)
{
	uint8_t user;

	STATS_PRINT("----Mode Count: ----\n");
	STATS_64(stdout, "DL SU:", deter->dl_mode_cnt[STATS_IF_TXDL_SU_DATA]);
	STATS_64(stdout, "DL MUOFDMA:", deter->dl_mode_cnt[STATS_IF_TXDL_OFDMA_DATA]);
	STATS_64(stdout, "DL MUMIMO:", deter->dl_mode_cnt[STATS_IF_TXDL_MUMIMO_DATA]);
	STATS_64(stdout, "UL MUOFDMA:", deter->ul_mode_cnt[STATS_IF_TXUL_OFDMA_BASIC_TRIGGER_DATA]);
	STATS_64(stdout, "UL_MUMIMO:", deter->ul_mode_cnt[STATS_IF_TXUL_MUMIMO_BASIC_TRIGGER_DATA]);
	STATS_64(stdout, "UL BAR:", deter->ul_mode_cnt[STATS_IF_TXUL_OFDMA_MU_BAR]);
	STATS_64(stdout, "RX SU:", deter->rx_su_cnt);

	STATS_PRINT("----DL OFDMA Num Users: ----\n");
	for (user = 0; user < STATS_IF_MAX_USERS; user++)
		STATS_PRINT("Num Users [%u]: %ju\n", user + 1, deter->dl_ofdma_usr[user]);
	STATS_PRINT("----UL OFDMA Num Users: ----\n");
	for (user = 0; user < STATS_IF_MAX_USERS; user++)
		STATS_PRINT("Num Users [%u]: %ju\n", user + 1, deter->ul_ofdma_usr[user]);
	STATS_PRINT("----DL MIMO Num Users: ----\n");
	for (user = 0; user < STATS_IF_MAX_MIMO_USERS; user++)
		STATS_PRINT("Num Users [%u]: %ju\n", user + 1, deter->dl_mimo_usr[user]);
	STATS_PRINT("----UL MIMO Num Users: ----\n");
	for (user = 0; user < STATS_IF_MAX_MIMO_USERS; user++)
		STATS_PRINT("Num Users [%u]: %ju\n", user + 1, deter->ul_mimo_usr[user]);

	STATS_PRINT("----Trigger Status: ----\n");
	STATS_64(stdout, "UL OFDMA Success:", deter->ts[STATS_IF_TXUL_OFDMA_BASIC_TRIGGER_DATA].trigger_success);
	STATS_64(stdout, "UL OFDMA fail:", deter->ts[STATS_IF_TXUL_OFDMA_BASIC_TRIGGER_DATA].trigger_fail);
	STATS_64(stdout, "UL MIMO Success:", deter->ts[STATS_IF_TXUL_MUMIMO_BASIC_TRIGGER_DATA].trigger_success);
	STATS_64(stdout, "UL MIMO fail:", deter->ts[STATS_IF_TXUL_MUMIMO_BASIC_TRIGGER_DATA].trigger_fail);
	STATS_64(stdout, "UL BAR Success:", deter->ts[STATS_IF_TXUL_OFDMA_MU_BAR].trigger_success);
	STATS_64(stdout, "UL BAR fail:", deter->ts[STATS_IF_TXUL_OFDMA_MU_BAR].trigger_fail);

	STATS_PRINT("----Channel Utilization: ----\n");
	STATS_32(stdout, "Total_cu:", deter->ch_util.ap_chan_util);
	STATS_32(stdout, "TX Util:", deter->ch_util.ap_tx_util);
	STATS_32(stdout, "RX Util:", deter->ch_util.ap_rx_util);
	STATS_PRINT("----Channel ACCESS DELAY: ----\n");
	STATS_32(stdout, "delay BE:", deter->ch_access_delay[STATS_IF_WME_AC_BE]);
	STATS_32(stdout, "delay BK:", deter->ch_access_delay[STATS_IF_WME_AC_BK]);
	STATS_32(stdout, "delay VI:", deter->ch_access_delay[STATS_IF_WME_AC_VI]);
	STATS_32(stdout, "delay VO:", deter->ch_access_delay[STATS_IF_WME_AC_VO]);
}

void print_debug_radio_ctrl_tx(struct debug_pdev_ctrl_tx *tx)
{
	print_basic_radio_ctrl_tx(&tx->b_tx);
	STATS_32(stdout, "Tx no buffer", tx->cs_be_nobuf);
	STATS_32(stdout, "Tx buffer count", tx->cs_tx_buf_count);
	STATS_64(stdout, "Tx HW retries", tx->cs_tx_hw_retries);
	STATS_64(stdout, "Tx HW failures", tx->cs_tx_hw_failures);
	STATS_8(stdout, "AP stats tx cal enable status",
		tx->cs_ap_stats_tx_cal_enable);
}

void print_debug_radio_ctrl_rx(struct debug_pdev_ctrl_rx *rx)
{
	print_basic_radio_ctrl_rx(&rx->b_rx);
	STATS_32(stdout, "Rx RTS success count", rx->cs_rx_rts_success);
	STATS_32(stdout, "Rx clear count", rx->cs_rx_clear_count);
	STATS_32(stdout, "Rx overrun frames", rx->cs_rx_overrun);
	STATS_32(stdout, "Rx phy error", rx->cs_rx_phy_err);
	STATS_32(stdout, "Rx ack error", rx->cs_rx_ack_err);
	STATS_32(stdout, "Rx RTS error", rx->cs_rx_rts_err);
	STATS_32(stdout, "Rx No beacon count", rx->cs_no_beacons);
	STATS_32(stdout, "Rx phy error count", rx->cs_phy_err_count);
	STATS_32(stdout, "Rx FCS error count", rx->cs_fcsbad);
	STATS_32(stdout, "Rx loop limit Start", rx->cs_rx_looplimit_start);
	STATS_32(stdout, "Rx loop limit End", rx->cs_rx_looplimit_end);
}

void print_debug_radio_ctrl_wmi(struct debug_pdev_ctrl_wmi *wmi)
{
	STATS_64(stdout, "Tx Mgmt count", wmi->cs_wmi_tx_mgmt);
	STATS_64(stdout, "Tx Mgmt completions count",
		 wmi->cs_wmi_tx_mgmt_completions);
	STATS_32(stdout, "Tx Mgmt completions error",
		 wmi->cs_wmi_tx_mgmt_completion_err);
}

void print_debug_radio_ctrl_link(struct debug_pdev_ctrl_link *link)
{
	print_basic_radio_ctrl_link(&link->b_link);
}

void print_debug_radio_data(struct stats_obj *radio)
{
	struct debug_pdev_data *data = radio->stats;

	STATS_PRINT("Debug Data STATS for Radio %s (Parent %s)\n",
		    radio->u_id.if_name, radio->pif_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_radio_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_radio_data_rx(data->rx);
	}
	if (data->me) {
		STATS_PRINT("ME Stats\n");
		print_debug_radio_data_me(data->me);
	}
	if (data->raw) {
		STATS_PRINT("RAW Stats\n");
		print_debug_radio_data_raw(data->raw);
	}
	if (data->tso) {
		STATS_PRINT("TSO Stats\n");
		print_debug_radio_data_tso(data->tso);
	}
	if (data->cfr) {
		STATS_PRINT("CFR Stats\n");
		print_debug_radio_data_cfr(data->cfr);
	}
	if (data->wdi) {
		STATS_PRINT("WDI Stats\n");
		print_debug_radio_data_wdi(data->wdi);
	}
	if (data->mesh) {
		STATS_PRINT("MESH Stats\n");
		print_debug_radio_data_mesh(data->mesh);
	}
	if (data->txcap) {
		STATS_PRINT("Tx Capture Stats\n");
		print_debug_radio_data_txcap(data->txcap);
	}
	if (data->monitor) {
		STATS_PRINT("Monitor mode Stats\n");
		print_debug_radio_data_monitor(data->monitor);
	}
	if (data->deter) {
		STATS_PRINT("Deterministic Stats\n");
		print_debug_radio_deter_stats(data->deter);
	}
}

void print_debug_radio_ctrl(struct stats_obj *radio)
{
	struct debug_pdev_ctrl *ctrl = radio->stats;

	STATS_PRINT("Debug Control STATS for Radio %s (Parent %s)\n",
		    radio->u_id.if_name, radio->pif_name);
	if (ctrl->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_radio_ctrl_tx(ctrl->tx);
	}
	if (ctrl->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_radio_ctrl_rx(ctrl->rx);
	}
	if (ctrl->wmi) {
		STATS_PRINT("WMI Stats\n");
		print_debug_radio_ctrl_wmi(ctrl->wmi);
	}
	if (ctrl->link) {
		STATS_PRINT("Link Stats\n");
		print_debug_radio_ctrl_link(ctrl->link);
	}
}

void print_debug_ap_data_tx(struct debug_psoc_data_tx *tx)
{
	uint8_t i;

	print_basic_ap_data_tx(&tx->b_tx);
	STATS_64(stdout, "Tx Invalid peer Packets", tx->tx_invalid_peer.num);
	STATS_64(stdout, "Tx Invalid peer Bytes", tx->tx_invalid_peer.bytes);
	STATS_PRINT("\tPackets Queued in HW Ring:\n");
	for (i = 0; i < STATS_IF_MAX_TX_DATA_RINGS; i++)
		STATS_PRINT("\t\tRing ID %u = %u\n", i, tx->tx_hw_enq[i]);
	STATS_PRINT("\tPackets dropped due to HW Ring full = %u %u %u\n",
		    tx->tx_hw_ring_full[0], tx->tx_hw_ring_full[1],
		    tx->tx_hw_ring_full[2]);
	STATS_32(stdout, "Tx Descriptor in use", tx->desc_in_use);
	STATS_32(stdout, "Tx packets dropped due to FW removed",
		 tx->dropped_fw_removed);
	STATS_PRINT("\tTx comp wifi internal error = %u : [%u %u %u %u]\n",
		    tx->wifi_internal_error[0], tx->wifi_internal_error[1],
		    tx->wifi_internal_error[2], tx->wifi_internal_error[3],
		    tx->wifi_internal_error[4]);
	STATS_32(stdout, "Tx invalid completion release",
		 tx->invalid_release_source);
	STATS_32(stdout, "Tx comp non wifi internal error",
		 tx->non_wifi_internal_err);
	STATS_32(stdout, "Tx comp loop pkt limit hit",
		 tx->tx_comp_loop_pkt_limit_hit);
	STATS_32(stdout, "Tx comp HP out of sync2", tx->hp_oos2);
	STATS_32(stdout, "Tx comp exception", tx->tx_comp_exception);
}

void print_debug_ap_data_rx(struct debug_psoc_data_rx *rx)
{
	uint8_t ring;
	uint16_t core;
	uint32_t err_code;
	uint64_t total_pkts;

	print_basic_ap_data_rx(&rx->b_rx);
	STATS_PRINT("\tPackets per Core per Ring:\n");
	for (ring = 0; ring < STATS_IF_MAX_RX_DEST_RINGS; ring++) {
		total_pkts = 0;
		STATS_PRINT("\t\tPackets on ring %u:\n", ring);
		for (core = 0; core < rx->rx_packets.num_cpus; core++) {
			if (rx->rx_packets.pkts[core][ring]) {
				STATS_PRINT("\t\t\tPackets on core%u: %ju\n",
					    core,
					    rx->rx_packets.pkts[core][ring]);
				total_pkts += rx->rx_packets.pkts[core][ring];
			}
		}
		STATS_PRINT("\t\t\tTotal Packets on Ring %u: %ju\n",
			    ring, total_pkts);
	}
	STATS_64(stdout, "Rx Invalid Peer Packets", rx->rx_invalid_peer.num);
	STATS_64(stdout, "Rx Invalid Peer Bytes", rx->rx_invalid_peer.bytes);
	STATS_64(stdout, "Rx Invalid SW Peer Id Packets",
		 rx->rx_invalid_peer_id.num);
	STATS_64(stdout, "Rx Invalid SW Peer Id Bytes",
		 rx->rx_invalid_peer_id.bytes);
	STATS_64(stdout, "Rx Invalid length Packets",
		 rx->rx_invalid_pkt_len.num);
	STATS_64(stdout, "Rx Invalid length Bytes",
		 rx->rx_invalid_pkt_len.bytes);
	STATS_32(stdout, "Rx Frag Length error", rx->rx_frag_err_len_error);
	STATS_32(stdout, "Rx Frag No Peer error", rx->rx_frag_err_no_peer);
	STATS_32(stdout, "Rx Frag Wait", rx->rx_frag_wait);
	STATS_32(stdout, "Rx Frag Error", rx->rx_frag_err);
	STATS_32(stdout, "Rx Frag Out of order", rx->rx_frag_oor);
	STATS_32(stdout, "Rx Reap Loop Pkt Limit Hit",
		 rx->reap_loop_pkt_limit_hit);
	STATS_32(stdout, "Rx HP out_of_sync", rx->hp_oos2);
	STATS_32(stdout, "Rx HW near full", rx->near_full);
	STATS_32(stdout, "Rx wait completed msdu break",
		 rx->msdu_scatter_wait_break);
	STATS_32(stdout, "Rx SW2rel route drop", rx->rx_sw_route_drop);
	STATS_32(stdout, "Rx HW2rel route drop", rx->rx_hw_route_drop);
	STATS_32(stdout, "Phy ring access fail: msdus",
		 rx->phy_ring_access_fail);
	STATS_32(stdout, "Phy ring access full fail: msdus",
		 rx->phy_ring_access_full_fail);
	STATS_32(stdout, "Phy Rx HW DUP DESC", rx->phy_rx_hw_dest_dup);
	STATS_32(stdout, "Phy Rx HW REL DUP DESC", rx->phy_wifi_rel_dup);
	STATS_32(stdout, "Phy Rx SW ERR DUP DESC", rx->phy_rx_sw_err_dup);
	STATS_32(stdout, "rbm error: msdus", rx->invalid_rbm);
	STATS_32(stdout, "Invalid vdev", rx->invalid_vdev);
	STATS_32(stdout, "Invalid pdev", rx->invalid_pdev);
	STATS_32(stdout, "pkts delivered no peer", rx->pkt_delivered_no_peer);
	STATS_32(stdout, "defrag peer uninit", rx->defrag_peer_uninit);
	STATS_32(stdout, "sa or da idx invalid", rx->invalid_sa_da_idx);
	STATS_32(stdout, "MSDU Done failures", rx->msdu_done_fail);
	STATS_32(stdout, "Rx DESC invalid magic", rx->rx_desc_invalid_magic);
	STATS_32(stdout, "Rx HW CMD SEND FAIL", rx->rx_hw_cmd_send_fail);
	STATS_32(stdout, "Rx HW CMD SEND Drain", rx->rx_hw_cmd_send_drain);
	STATS_32(stdout, "Rx scatter msdu", rx->scatter_msdu);
	STATS_32(stdout, "RX invalid cookie", rx->invalid_cookie);
	STATS_32(stdout, "RX stale cookie", rx->stale_cookie);
	STATS_32(stdout, "2k jump delba sent", rx->rx_2k_jump_delba_sent);
	STATS_32(stdout, "2k jump msdu to stack", rx->rx_2k_jump_to_stack);
	STATS_32(stdout, "2k jump msdu drop", rx->rx_2k_jump_drop);
	STATS_32(stdout, "Rx HW Error MSDU buffer recved",
		 rx->rx_hw_err_msdu_buf_rcved);
	STATS_32(stdout, "Rx HW Error MSDU buffer invalid cookie",
		 rx->rx_hw_err_msdu_buf_invalid_cookie);
	STATS_32(stdout, "Rx HW oor msdu drop", rx->rx_hw_err_oor_drop);
	STATS_32(stdout, "Rx HW oor msdu to stack", rx->rx_hw_err_oor_to_stack);
	STATS_32(stdout, "Rx HW Out of Order sg count",
		 rx->rx_hw_err_oor_sg_count);
	STATS_32(stdout, "Rx MSDU count mismatch", rx->msdu_count_mismatch);
	STATS_32(stdout, "Rx stale link desc cookie", rx->invalid_link_cookie);
	STATS_32(stdout, "Rx nbuf sanity fails", rx->nbuf_sanity_fail);
	STATS_32(stdout, "Rx refill duplicate link desc",
		 rx->dup_refill_link_desc);
	STATS_32(stdout, "Rx err msdu continuation err",
		 rx->msdu_continuation_err);
	STATS_32(stdout, "ssn update count", rx->ssn_update_count);
	STATS_32(stdout, "bar handle update fail count",
		 rx->bar_handle_fail_count);
	STATS_32(stdout, "Intra-bss EAPOL drops", rx->intrabss_eapol_drop);
	STATS_32(stdout, "PN-in-Dest error frame pn-check fail",
		 rx->pn_in_dest_check_fail);
	STATS_32(stdout, "MSDU Length Error", rx->msdu_len_err);
	STATS_32(stdout, "Rx Flush count", rx->rx_flush_count);
	for (err_code = 0; err_code < STATS_IF_WIFI_ERR_MAX; err_code++) {
		if (rx->rx_sw_error[err_code])
			STATS_PRINT("\tRx SW error number (%u): %u msdus\n",
				    err_code, rx->rx_sw_error[err_code]);
	}
	for (err_code = 0; err_code < STATS_IF_RX_ERR_MAX; err_code++) {
		if (rx->rx_hw_error[err_code])
			STATS_PRINT("\tRx HW error number (%u): %u msdus\n",
				    err_code, rx->rx_hw_error[err_code]);
	}
	for (err_code = 0; err_code < STATS_IF_MAX_RX_DEST_RINGS; err_code++) {
		if (rx->phy_rx_hw_error[err_code])
			STATS_PRINT("\tPhy Rx SW error number (%u): %u msdus\n",
				    err_code, rx->phy_rx_hw_error[err_code]);
	}
}

void print_debug_ap_data_ast(struct debug_psoc_data_ast *ast)
{
	STATS_32(stdout, "Entries Added", ast->ast_added);
	STATS_32(stdout, "Entries Deleted", ast->ast_deleted);
	STATS_32(stdout, "Entries Agedout", ast->ast_aged_out);
	STATS_32(stdout, "Entries MAP ERR", ast->ast_map_err);
	STATS_32(stdout, "Entries Mismatch ERR", ast->ast_mismatch);
	STATS_32(stdout, "MEC Added", ast->mec_added);
	STATS_32(stdout, "MEC Deleted", ast->mec_deleted);
}

void print_debug_ap_data(struct stats_obj *ap)
{
	struct debug_psoc_data *data = ap->stats;

	STATS_PRINT("Debug Data STATS for AP %s\n", ap->u_id.if_name);
	if (data->tx) {
		STATS_PRINT("Tx Stats\n");
		print_debug_ap_data_tx(data->tx);
	}
	if (data->rx) {
		STATS_PRINT("Rx Stats\n");
		print_debug_ap_data_rx(data->rx);
	}
	if (data->ast) {
		STATS_PRINT("AST Stats\n");
		print_debug_ap_data_ast(data->ast);
	}
}

void print_debug_stats(struct stats_obj *obj)
{
	switch (obj->obj_type) {
	case STATS_OBJ_STA:
		if (obj->type == STATS_TYPE_DATA)
			print_debug_sta_data(obj);
		else
			print_debug_sta_ctrl(obj);
		break;
	case STATS_OBJ_VAP:
		if (obj->type == STATS_TYPE_DATA)
			print_debug_vap_data(obj);
		else
			print_debug_vap_ctrl(obj);
		break;
	case STATS_OBJ_RADIO:
		if (obj->type == STATS_TYPE_DATA)
			print_debug_radio_data(obj);
		else
			print_debug_radio_ctrl(obj);
		break;
	case STATS_OBJ_AP:
		if (obj->type == STATS_TYPE_DATA)
			print_debug_ap_data(obj);
		break;
	default:
		STATS_ERR("Invalid object option\n");
	}
}
#endif /* WLAN_DEBUG_TELEMETRY */

void print_response(struct reply_buffer *reply)
{
	struct stats_obj *obj = reply->obj_head;

	while (obj) {
		switch (obj->lvl) {
		case STATS_LVL_BASIC:
			print_basic_stats(obj);
			break;
#if WLAN_ADVANCE_TELEMETRY
		case STATS_LVL_ADVANCE:
			print_advance_stats(obj);
			break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
		case STATS_LVL_DEBUG:
			print_debug_stats(obj);
			break;
#endif /* WLAN_DEBUG_TELEMETRY */
		default:
			STATS_ERR("Invalid Level!\n");
		}
		obj = obj->next;
	}
}

void stats_async_response_handler(struct stats_command *cmd, char *ifname)
{
	if (!cmd || !cmd->reply)
		return;

	print_response(cmd->reply);
}

void stop_handler(int val)
{
	g_run_loop = false;
}

int handle_async_request(int argc, char *argv[])
{
	struct stats_command cmd = {0};

	if (argc > 2)
		return -EINVAL;

	/* Check for Async mode, otherwise allow further parsing */
	if (!argv[1] || strncmp(argv[1], STATS_IF_ASYNC_MODE,
				strlen(STATS_IF_ASYNC_MODE)))
		return -EINVAL;

	cmd.lvl = STATS_LVL_ADVANCE;
	cmd.obj = STATS_OBJ_RADIO;
	cmd.async_callback = stats_async_response_handler;

	cmd.reply = malloc(sizeof(struct reply_buffer));
	if (!cmd.reply) {
		STATS_ERR("Failed to allocate memory\n");
		return -ENOMEM;
	}
	memset(cmd.reply, 0, sizeof(struct reply_buffer));

	if (libstats_request_async_start(&cmd)) {
		STATS_ERR("Unable to start Async stats!\n");
		libstats_request_async_stop(&cmd);
		free(cmd.reply);
		cmd.reply = NULL;
		return 0;
	}
	signal(SIGINT, stop_handler);
	g_run_loop = true;
	while (g_run_loop)
		/* sleep for 1 ms */
		usleep(1000);

	libstats_request_async_stop(&cmd);
	if (cmd.reply)
		free(cmd.reply);
	cmd.reply = NULL;

	return 0;
}

int main(int argc, char *argv[])
{
	struct stats_command cmd;
	struct reply_buffer *reply = NULL;
	enum stats_level_e level_temp = STATS_LVL_BASIC;
	enum stats_object_e obj_temp = STATS_OBJ_AP;
	enum stats_type_e type_temp = STATS_TYPE_DATA;
	u_int64_t feat_temp = STATS_FEAT_FLG_ALL;
	int ret = 0;
	int long_index = 0;
	u_int8_t is_obj_set = 0;
	u_int8_t is_type_set = 0;
	u_int8_t is_feat_set = 0;
	u_int8_t is_level_set = 0;
	u_int8_t is_ifname_set = 0;
	u_int8_t is_stamacaddr_set = 0;
	u_int8_t is_serviceid_set = 0;
	u_int8_t is_option_selected = 0;
	u_int8_t is_ipa_stats_set = 0;
	u_int8_t is_peer_type_set = 0;
	enum stats_peer_type peer_type = STATS_WILD_PEER_TYPE;
	bool is_mld_link = false;
	bool recursion_temp = false;
	char feat_flags[128] = {'\0'};
	char ifname_temp[IFNAME_LEN] = {'\0'};
	char stamacaddr_temp[USER_MAC_ADDR_LEN] = {'\0'};
	struct ether_addr *ret_eth_addr = NULL;
	u_int8_t servid_temp = 0;

	memset(&cmd, 0, sizeof(struct stats_command));

	ret = handle_async_request(argc, argv);
	if (!ret)
		return ret;

	ret = getopt_long(argc, argv, opt_string, long_opts, &long_index);
	while (ret != -1) {
		switch (ret) {
		case 'B':
			if (is_level_set) {
				STATS_ERR("Multiple Stats Level Arguments\n");
				display_help();
				return -EINVAL;
			}
			level_temp = STATS_LVL_BASIC;
			is_level_set = 1;
			is_option_selected = 1;
			break;
		case 'A':
			if (is_level_set) {
				STATS_ERR("Multiple Stats Level Arguments\n");
				display_help();
				return -EINVAL;
			}
			level_temp = STATS_LVL_ADVANCE;
			is_level_set = 1;
			is_option_selected = 1;
			break;
		case 'D':
			if (is_level_set) {
				STATS_ERR("Multiple Stats Level Arguments\n");
				display_help();
				return -EINVAL;
			}
			level_temp = STATS_LVL_DEBUG;
			is_level_set = 1;
			is_option_selected = 1;
			break;
		case 'i':
			if (is_ifname_set) {
				STATS_ERR("Multiple Stats ifname Arguments\n");
				display_help();
				return -EINVAL;
			}
			memset(ifname_temp, '\0', sizeof(ifname_temp));
			if (strlcpy(ifname_temp, optarg, sizeof(ifname_temp)) >=
			    sizeof(ifname_temp)) {
				STATS_ERR("Error creating ifname\n");
				return -EINVAL;
			}
			is_ifname_set  = 1;
			is_option_selected = 1;
			break;
		case 's':
			if (is_obj_set) {
				STATS_ERR("Multiple Stats Object Arguments\n");
				display_help();
				return -EINVAL;
			}
			obj_temp = STATS_OBJ_STA;
			is_obj_set = 1;
			is_option_selected = 1;
			break;
		case 'v':
			if (is_obj_set) {
				STATS_ERR("Multiple Stats Object Arguments\n");
				display_help();
				return -EINVAL;
			}
			obj_temp = STATS_OBJ_VAP;
			is_obj_set = 1;
			is_option_selected = 1;
			break;
		case 'r':
			if (is_obj_set) {
				STATS_ERR("Multiple Stats Object Arguments\n");
				display_help();
				return -EINVAL;
			}
			obj_temp = STATS_OBJ_RADIO;
			is_obj_set = 1;
			is_option_selected = 1;
			break;
		case 'a':
			if (is_obj_set) {
				STATS_ERR("Multiple Stats Object Arguments\n");
				display_help();
				return -EINVAL;
			}
			obj_temp = STATS_OBJ_AP;
			is_obj_set = 1;
			is_option_selected = 1;
			break;
		case 'm':
			if (is_stamacaddr_set) {
				STATS_ERR("Multiple STA MAC Arguments\n");
				display_help();
				return -EINVAL;
			}
			memset(stamacaddr_temp, '\0', sizeof(stamacaddr_temp));
			if (strlcpy(stamacaddr_temp, optarg,
				    sizeof(stamacaddr_temp)) >=
				    sizeof(stamacaddr_temp)) {
				STATS_ERR("Error copying macaddr\n");
				return -EINVAL;
			}
			is_stamacaddr_set  = 1;
			is_option_selected = 1;
			break;
		case 'l':
			if (is_peer_type_set) {
				STATS_ERR("Multiple Feature flag Arguments\n");
				display_help();
				return -EINVAL;
			}
			if ((bool)atoi(optarg)) {
				peer_type = STATS_LINK_PEER_TYPE;
			} else if ((bool)atoi(optarg) == 0) {
				peer_type = STATS_MLD_PEER_TYPE;
			} else {
				STATS_ERR("Invalid Argument Value\n");
				return -EINVAL;
			}
			is_peer_type_set = 1;
			break;
		case 't':
			if (is_serviceid_set) {
				STATS_ERR("Multiple Serviceid Arguments\n");
				display_help();
				return -EINVAL;
			}
			servid_temp = atoi(optarg);
			is_serviceid_set = 1;
			is_option_selected = 1;
			break;
		case 'I':
			if (is_ipa_stats_set) {
				STATS_ERR("Multiple Arguments\n");
				display_help();
				return -EINVAL;
			}
			g_ipa_mode = 1;
			is_ipa_stats_set = 1;
			break;
		case 'f':
			if (is_feat_set) {
				STATS_ERR("Multiple Feature flag Arguments\n");
				display_help();
				return -EINVAL;
			}
			memset(feat_flags, '\0', sizeof(feat_flags));
			if (strlcpy(feat_flags, optarg, sizeof(feat_flags)) >=
			    sizeof(feat_flags)) {
				STATS_ERR("Error copying feature flags\n");
				return -EINVAL;
			}
			is_feat_set = 1;
			is_option_selected = 1;
			break;
		case 'c':
			if (is_type_set) {
				STATS_ERR("Multiple Type Arguments\n");
				display_help();
				return -EINVAL;
			}
			type_temp = STATS_TYPE_CTRL;
			is_type_set = 1;
			is_option_selected = 1;
			break;
		case 'd':
			if (is_type_set) {
				STATS_ERR("Multiple Type Arguments\n");
				display_help();
				return -EINVAL;
			}
			type_temp = STATS_TYPE_DATA;
			is_type_set = 1;
			is_option_selected = 1;
			break;
		case 'M':
			is_mld_link = true;
			break;
		case 'h':
		case '?':
			display_help();
			is_option_selected = 1;
			return 0;
		case 'R':
			recursion_temp = true;
			break;
		default:
			STATS_ERR("Unrecognized option\n");
			display_help();
			is_option_selected = 1;
			return -EINVAL;
		}
		ret = getopt_long(argc, argv, opt_string,
				  long_opts, &long_index);
	}

	if (!is_option_selected)
		STATS_WARN("No valid option selected\n"
			   "Will display only default stats\n");

	if (feat_flags[0])
		feat_temp = libstats_get_feature_flag(feat_flags);
	if (!feat_temp)
		return -EINVAL;

	cmd.lvl = level_temp;
	cmd.obj = obj_temp;
	cmd.type = type_temp;
	cmd.feat_flag = feat_temp;
	cmd.recursive = recursion_temp;
	cmd.serviceid = servid_temp;
	cmd.mld_link = is_mld_link;
	cmd.peer_type = peer_type;

	strlcpy(cmd.if_name, ifname_temp, IFNAME_LEN);

	if (stamacaddr_temp[0] != '\0') {
		ret_eth_addr = ether_aton_r(stamacaddr_temp, &cmd.sta_mac);
		if (!ret_eth_addr) {
			STATS_ERR("STA MAC address not valid.\n");
			return -EINVAL;
		}
	}

	reply = (struct reply_buffer *)malloc(sizeof(struct reply_buffer));
	if (!reply) {
		STATS_ERR("Failed to allocate memory\n");
		return -ENOMEM;
	}
	memset(reply, 0, sizeof(struct reply_buffer));
	cmd.reply = reply;

	ret = libstats_request_handle(&cmd);

	/* Print Output */
	if (!ret)
		print_response(cmd.reply);

	/* Cleanup */
	libstats_free_reply_buffer(&cmd);
	if (cmd.reply)
		free(cmd.reply);
	cmd.reply = NULL;

	return 0;
}
