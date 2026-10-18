/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ATH12K_DEBUG_H_
#define _ATH12K_DEBUG_H_

#include "trace.h"
#include "debugfs.h"

enum ath12k_debug_mask {
	/* Core (word 0) */
	ATH12K_DBG_AHB			= BIT(0),
	ATH12K_DBG_WMI			= BIT(1),
	ATH12K_DBG_HTC			= BIT(2),
	ATH12K_DBG_DP_HTT		= BIT(3),
	ATH12K_DBG_MAC			= BIT(4),
	ATH12K_DBG_BOOT			= BIT(5),
	ATH12K_DBG_QMI			= BIT(6),
	ATH12K_DBG_DATA			= BIT(7),
	ATH12K_DBG_MGMT			= BIT(8),
	ATH12K_DBG_REG			= BIT(9),
	ATH12K_DBG_TESTMODE		= BIT(10),
	ATH12K_DBG_HAL			= BIT(11),
	ATH12K_DBG_PCI			= BIT(12),
	ATH12K_DBG_DP_TX		= BIT(13),
	ATH12K_DBG_DP_RX		= BIT(14),
	ATH12K_DBG_WOW			= BIT(15),
	ATH12K_DBG_DP_FST		= BIT(16),
	ATH12K_DBG_PEER			= BIT(17),
	ATH12K_DBG_INI			= BIT(18),
	ATH12K_DBG_QOS			= BIT(19),
	ATH12K_DBG_DP_UMAC_RESET	= BIT(20),
	ATH12K_DBG_MODE1_RECOVERY	= BIT(21),
	ATH12K_DBG_TELEMETRY		= BIT(22),
	ATH12K_DBG_RM			= BIT(23),
	ATH12K_DBG_CFR			= BIT(24),
	ATH12K_DBG_CFR_DUMP		= BIT(25),
	ATH12K_DBG_AFC			= BIT(26),
	ATH12K_DBG_WSI_BYPASS		= BIT(27),
	ATH12K_DBG_PPE			= BIT(28),
	ATH12K_DBG_MLME			= BIT(29),
	ATH12K_DBG_EAPOL		= BIT(30),
	ATH12K_DBG_CFG			= BIT(31),

	/* Extended functionality */
	ATH12K_DBG_DP_MON		= BIT_ULL(32),
	ATH12K_DBG_SCAN			= BIT_ULL(33),
	ATH12K_DBG_ASSOC		= BIT_ULL(34),
	ATH12K_DBG_ROAM			= BIT_ULL(35),
	ATH12K_DBG_DFS			= BIT_ULL(36),
	ATH12K_DBG_CRYPTO		= BIT_ULL(37),
	ATH12K_DBG_OFFCHAN		= BIT_ULL(38),
	ATH12K_DBG_ACTION		= BIT_ULL(39),
	ATH12K_DBG_MLO			= BIT_ULL(40),
	ATH12K_DBG_POWER		= BIT_ULL(41),
	ATH12K_DBG_DP_MON_TX		= BIT_ULL(42),
	ATH12K_DBG_MMESH		= BIT_ULL(43),
	ATH12K_DBG_CU			= BIT_ULL(44),
	ATH12K_DBG_IPA			= BIT_ULL(45),
	ATH12K_DBG_SPECTRAL		= BIT_ULL(46),
	ATH12K_DBG_SMD			= BIT_ULL(47),
	ATH12K_DBG_RTT			= BIT_ULL(48),

	ATH12K_DBG_ANY			= GENMASK_ULL(63, 0),
};

enum ath12k_debug_mask_level {
	ATH12K_DBG_L0,
	ATH12K_DBG_L1,
	ATH12K_DBG_L2,
	ATH12K_DBG_L3,
};

__printf(2, 3) void ath12k_info(struct ath12k_base *ab, const char *fmt, ...);
__printf(2, 3) void ath12k_err(struct ath12k_base *ab, const char *fmt, ...);
__printf(2, 3) void __ath12k_warn(struct device *dev, const char *fmt, ...);

#define ath12k_warn(ab, fmt, ...) __ath12k_warn((ab)->dev, fmt, ##__VA_ARGS__)
#define ath12k_hw_warn(ah, fmt, ...) __ath12k_warn((ah)->dev, fmt, ##__VA_ARGS__)
#define ath12k_dbg_level(ab, dbg_mask, dbg_level, fmt, ...)				\
do {											\
	if (ath12k_debug_mask_level >= dbg_level)		\
		ath12k_dbg(ab, dbg_mask, fmt, ##__VA_ARGS__);				\
} while (0)

extern u64 ath12k_debug_mask;
extern unsigned int ath12k_debug_mask_level;
extern bool ath12k_ftm_mode;

static inline void ath12k_format_log_prefix(struct ath12k_base *ab,
					int radio_id,
					int vdev_id,
					char *buf,
					size_t buf_len)
{
	scnprintf(buf, buf_len,
		  "[radio_id : %d][vdev : %d]",
		  radio_id, vdev_id);
}
#ifdef CPTCFG_ATH12K_DEBUG
__printf(3, 4) void __ath12k_dbg(struct ath12k_base *ab,
				 u64 mask,
				 const char *fmt, ...);
void ath12k_dbg_dump(struct ath12k_base *ab,
		     u64 mask,
		     const char *msg, const char *prefix,
		     const void *buf, size_t len);

#define ath12k_dbg_tag(ab, dbg_mask, dbg_level, rid, vid, fmt, ...)            \
do {                                                                           \
	if (((ath12k_debug_mask & (dbg_mask)) != 0) &&                             \
	    (ath12k_debug_mask_level >= (unsigned int)(dbg_level))) {              \
		char __pfx[128];                                                       \
		ath12k_format_log_prefix((ab), (rid), (vid), __pfx, sizeof(__pfx));    \
		__ath12k_dbg((ab), (dbg_mask), "%s" fmt, __pfx, ##__VA_ARGS__);     \
	}                                                                           \
} while (0)

#else /* CPTCFG_ATH12K_DEBUG */
static inline void __ath12k_dbg(struct ath12k_base *ab,
				u64 dbg_mask,
				const char *fmt, ...)
{
}

static inline void ath12k_dbg_dump(struct ath12k_base *ab,
				   u64 mask,
				   const char *msg, const char *prefix,
				   const void *buf, size_t len)
{
}
#define ath12k_dbg_tag(ab, dbg_mask, dbg_level, rid, vid, fmt, ...)

#endif /* CPTCFG_ATH12K_DEBUG */

#define ath12k_dbg(ab, dbg_mask, fmt, ...)               \
do {                                                     \
	u64 __mask = (u64)(dbg_mask);                        \
	if (ath12k_debug_mask & __mask)                      \
		__ath12k_dbg(ab, __mask, fmt, ##__VA_ARGS__);    \
} while (0)

#define ath12k_generic_dbg(dbg_mask, dbg_level, fmt, ...)			\
do {										\
	if (ath12k_debug_mask_level >= dbg_level)				\
		ath12k_dbg(NULL, dbg_mask, fmt, ##__VA_ARGS__);			\
} while (0)

enum ath12k_crit_event {
	ATH12K_CRIT_VAP_FAILURE = 0,
	ATH12K_CRIT_CONNECTION_FAIL = 1,
	ATH12K_CRIT_PEER_FAILURE = 2,
	ATH12K_FW_LOAD_FAILURE = 3,
	ATH12K_BDF_LOAD_FAILURE = 4,
	ATH12K_CRIT_MVR_FAILURE = 5,
	ATH12K_CRIT_SCAN_FAILURE = 6,
};

#if defined(CPTCFG_ATHDEBUG) && defined(CPTCFG_ATHDEBUG_UIO_LOGGING)
void athdbg_uio_critical_failure_trigger(struct ath12k_base *ab, uint32_t crit_enum);

#define ath12k_critical_failure_trigger(ab, crit_enum) \
	athdbg_uio_critical_failure_trigger(ab, crit_enum)

#else

#define ath12k_critical_failure_trigger(ab, crit_enum) \
	do { \
		(void)(ab); \
		(void)(crit_enum); \
	} while (0)

#endif

#endif /* _ATH12K_DEBUG_H_ */
