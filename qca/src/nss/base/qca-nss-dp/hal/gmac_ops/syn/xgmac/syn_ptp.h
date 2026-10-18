/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __SYN_PTP_H__
#define __SYN_PTP_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>

struct syn_hal_dev;
struct device;
struct dentry;
struct platform_device;
struct ethtool_ts_info;
struct ifreq;

/*
 * Platform type enumeration for PTP support
 */
enum platform_type {
	PLATFORM_IPQ52XX,	/* IPQ52XX platform */
	PLATFORM_IPQ96XX,	/* IPQ96XX platform */
	PLATFORM_UNKNOWN	/* Unknown platform */
};

/*
 * Platform PTP Manager (Singleton)
 * Manages shared platform-level PTP resources (GPIO, interrupts, TCSR)
 * that are common across all XGMAC instances on the board
 */
struct syn_ptp_platform_mgr {
	struct mutex lock;		/* Protects platform resources */
	int refcount;			/* Number of registered XGMAC instances */
	/* Platform identification */
	enum platform_type platform;

	/* Shared interrupt resources */
	int pps_in_irq;
	bool pps_in_irq_registered;	/* True if PPS_IN IRQ has been registered */

	/*
	 * List of XGMAC instances that currently have PPS capture enabled.
	 * Each entry is a syn_ptp_priv whose pps_enabled flag is true.
	 * Managed by qcom_nss_ptp_enable() via PTP_CLK_REQ_EXTTS.
	 * The IRQ handler iterates this list and delivers a PTP_CLOCK_EXTTS
	 * event to every active instance independently (each XGMAC has its
	 * own AUX FIFO and system-time registers).
	 * Protected by pps_list_lock (spinlock, safe from threaded IRQ context).
	 */
	struct list_head pps_active_list;
	spinlock_t pps_list_lock;		/* Protects pps_active_list */

	/* IPQ52XX-specific TCSR registers and routing */
	void __iomem *tcsr_pps_in;	/* TCSR PPS_IN register mapping */
	void __iomem *tcsr_pps_out;	/* TCSR PPS_OUT register mapping */
	u32 pps_in_source;		/* PPS_IN source selection (0-3) */
	u32 pps_out_source;		/* PPS_OUT source selection (0-1) */

	/* SPARE2 register for PPS output enable control */
	void __iomem *spare2;		/* SPARE2 register mapping */

	/* Platform device for sysfs */
	struct device *dev;		/* First XGMAC's device for sysfs */
};

/*
 * TX timestamp FIFO entry
 * Stores timestamp read from hardware FIFO indexed by packet ID
 */
struct syn_ptp_tx_ts_entry {
	u64 timestamp_ns;	/* Combined seconds + nanoseconds */
	u16 pkt_id;		/* 10-bit packet ID (1-1023) */
	ktime_t capture_time;	/* When we read it from HW (for stale detection) */
	bool valid;		/* Entry contains valid timestamp */
};

/*
 * PTP private data structure (Per-XGMAC instance)
 * Each XGMAC has its own PHC device but shares platform resources
 */
struct syn_ptp_priv {
	/* PHC device (per-XGMAC) */
	struct ptp_clock *clock;	/* PTP clock device handle */
	struct ptp_clock_info caps;	/* PTP clock capabilities and callbacks */
	struct ptp_pin_desc pin;	/* PPS PIN */

	/* Back pointers */
	struct syn_hal_dev *shd;	/* Back pointer to HAL device */
	struct device *dev;		/* Platform device for device operations */

	/* Reference to platform manager */
	struct syn_ptp_platform_mgr *platform_mgr;

	/*
	 * Node for platform_mgr->pps_active_list.
	 * Added to the list when PPS capture is enabled (PTP_CLK_REQ_EXTTS on=1),
	 * removed when disabled or during cleanup.
	 * Initialized with INIT_LIST_HEAD so list_del_init() is always safe.
	 */
	struct list_head pps_list_node;

	/* Per-XGMAC state */
	spinlock_t lock;		/* Spinlock to protect fast IRQ-context paths (gettimex, IRQ handler, pps_enabled) */
	struct mutex ts_ctl_mutex;	/* Mutex to serialize SYN_MAC_TS_CTL register operations (adjfine/adjtime/settime) */
	bool pps_enabled;		/* PPS capture enabled flag */
	struct hwtstamp_config tstamp_config;	/* Current hardware timestamp configuration */
	u32 ptp_clock_rate;		/* PTP clock frequency in Hz (same as MAC clock) */
	u32 ssinc;			/* Sub-second increment in nanoseconds (may be rounded up) */
	u32 default_addend;		/* Default addend value for frequency adjustment */

	/* TX timestamp FIFO management (8 entries matching HW FIFO depth) */
	struct syn_ptp_tx_ts_entry tx_ts_queue[8];
	spinlock_t tx_ts_lock;		/* Protects TX timestamp queue */

	/* Statistics for monitoring and debugging */
	u32 tx_ts_fifo_full_count;	/* Number of times FIFO was full */
	u32 tx_ts_match_success;	/* Successful timestamp matches */
	u32 tx_ts_match_fail;		/* Failed timestamp matches */
	u32 tx_ts_stale_count;		/* Stale timestamps discarded */
	u32 tx_ts_slot_conflict_count;	/* Slot conflicts/overwrites */

	/* RX timestamp statistics */
	u32 rx_ts_success;		/* Successfully captured RX timestamps */
	u32 rx_ts_filtered;		/* Packets filtered out (not timestamped) */

	/* Auxiliary (external) timestamp statistics for ts2phc PHY→XGMAC sync */
	u32 aux_ts_hw_count;		/* PPS edges captured by hardware FIFO */
	u32 aux_ts_sw_fallback;		/* Fallbacks to software timestamp (FIFO empty) */
	u32 aux_ts_missed;		/* FIFO overflow: ATSSTM bit set */
	u32 aux_ts_glitch;		/* PPS captures far from second boundary after sync */
	bool aux_ts_was_synced;		/* True once a hw ts was seen near a second boundary */

	/* Debugfs support */
	struct dentry *ptp_dentry;	/* PTP debugfs entry */
};

/*
 * External PTP function declarations
 */
int syn_ptp_init(struct syn_hal_dev *shd, struct platform_device *pdev);
void syn_ptp_cleanup(struct syn_hal_dev *shd);
int syn_ptp_get_ts_info(struct syn_hal_dev *shd, struct ethtool_ts_info *info);
int syn_ptp_hwtstamp_set(void *hal_ctx, struct ifreq *ifr);
int syn_ptp_hwtstamp_get(void *hal_ctx, struct ifreq *ifr);

#endif /*__SYN_PTP_H__*/
