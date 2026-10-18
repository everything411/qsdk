/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * syn_ptp.c
 *	Synopsys XGMAC PTP Hardware Clock (PHC) support
 *
 * This file implements PTP Hardware Clock support for the Synopsys XGMAC
 * driver, enabling precise time synchronization using IEEE 1588 PTP.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <fal/fal_port_ctrl.h>
#include "nss_dp_api_if.h"
#include "nss_dp_hal_if.h"
#include "syn_dev.h"
#include "edma.h"

/* Global singleton platform manager */
static struct syn_ptp_platform_mgr *g_platform_mgr = NULL;
static DEFINE_MUTEX(platform_mgr_mutex);

/* Global PTP debugfs directory */
static struct dentry *g_ptp_debugfs_dir = NULL;
static int g_ptp_debugfs_refcount = 0;
static DEFINE_MUTEX(ptp_debugfs_mutex);

/* Platform-specific register addresses for IPQ52XX */
#define IPQ52XX_TCSR_PPS_IN_REG		0x0196100C
#define IPQ52XX_TCSR_PPS_OUT_REG	0x01961010
#define IPQ52XX_GPIO_PPS		18	/* Reference only - GPIO now acquired from DTS */
#define IPQ52XX_PPS_IN_IRQ		359
#define IPQ52XX_PPS_OUT_IRQ		360

/* Platform-specific settings for IPQ96XX */
#define IPQ96XX_GPIO_PPS		50	/* Reference only - GPIO now acquired from DTS */
#define IPQ96XX_PPS_IN_IRQ		362
#define IPQ96XX_PPS_OUT_IRQ		363

/* SPARE2 register for PPS output enable control */
#define IPQ52XX_SPARE2_REG		0x01110020
#define IPQ52XX_SPARE2_PPS_EN		BIT(1)	/* PPS_EN: enable PPS output for IPQ5210 */
#define IPQ96XX_SPARE2_REG		0x01110020
#define IPQ96XX_SPARE2_TSN_EN		BIT(5)	/* TSN_EN: enable PPS output for IPQ9650 */

#define SYN_MAC_PTP_TXTS_FIFO_NUM	8

/*
 * SYN_PTP_PPS_BOUNDARY_THRESHOLD_NS - Distance from a second boundary above
 * which a hardware AUX timestamp is treated as a possible PPS glitch.
 *
 * A genuine PPS pulse should arrive very close to a second boundary.
 * If the captured timestamp is further than this threshold (and the clock
 * has already been synchronized), a dev_warn_ratelimited message is emitted,
 * aux_ts_glitch is incremented, and the timestamp is skipped (not delivered
 * to ts2phc).
 */
#define SYN_PTP_PPS_BOUNDARY_THRESHOLD_NS	10000000U	/* 10 ms */

static int syn_ptp_time_set(void __iomem *mac_base, u32 flag, u32 sec, u32 nsec)
{
	u32 value;

	/* Write seconds to MAC_System_Time_Seconds_Update register */
	hal_write_reg(mac_base, SYN_MAC_SYS_TIME_SECS_UPDATE, sec);

	/* Write nanoseconds to MAC_System_Time_Nanoseconds_Update register */
	hal_write_reg(mac_base, SYN_MAC_SYS_TIME_NSECS_UPDATE, nsec);

	/* Set TSINIT or TSUPDT bit to initialize the time */
	hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, flag);

	/* Poll TSINIT or TSUPDT bit until cleared by hardware */
	return readl_poll_timeout_atomic(mac_base + SYN_MAC_TS_CTL, value,
					 !(value & flag),
					 10, 100000);
}

static int syn_ptp_increment_set(void __iomem *mac_base, u32 ns)
{
	u32 value;

	/* Set integer nanosecond increment value per each clock cycle
	 * Note: SYN_MAC_SUB_SEC_INCR only supports integer nanosecond increments.
	 * Fractional nanosecond adjustments are handled via SYN_MAC_TS_ADDEND register.
	 */
	value = hal_read_reg(mac_base, SYN_MAC_SUB_SEC_INCR);

	value &= ~SYN_MAC_SUB_SEC_INCR_SSINC_MASK;
	value |= FIELD_PREP(SYN_MAC_SUB_SEC_INCR_SSINC_MASK, ns);

	hal_write_reg(mac_base, SYN_MAC_SUB_SEC_INCR, value);

	return 0;
}

/*
 * syn_ptp_pps_ctrl_set()
 *	Configure the PPS0 output frequency via MAC_PPS_Control register
 *
 * Sets PPSCTRL0 (bits 3:0) to the requested value while preserving all
 * other bits in the register (PPSEN0, TRGTMODSEL0, etc.).
 *
 * PPSCTRL0 encoding (MAC_PPS_Control, offset 0xD70):
 *   0000 - 1 narrow pulse per second (default, width = clk_ptp_ref_i)
 *   0001 - generated clock: binary 2 Hz / digital 1 Hz rollover
 *   0010 - generated clock: binary 4 Hz / digital 2 Hz rollover
 *   0011 - generated clock: binary 8 Hz / digital 4 Hz rollover
 *   0100 - generated clock: binary 16 Hz / digital 8 Hz rollover
 *
 * @mac_base: MMIO base address of the XGMAC instance
 * @ppsctrl:  PPSCTRL0 value to program (use SYN_MAC_PPS_CTL_PPSCTRL0_* defines)
 */
static void syn_ptp_pps_ctrl_set(void __iomem *mac_base, u32 ppsctrl)
{
	u32 pps_ctl;

	pps_ctl = hal_read_reg(mac_base, SYN_MAC_PPS_CTL);
	pps_ctl &= ~SYN_MAC_PPS_CTL_PPSCTRL_MASK;
	pps_ctl |= (ppsctrl & SYN_MAC_PPS_CTL_PPSCTRL_MASK);
	hal_write_reg(mac_base, SYN_MAC_PPS_CTL, pps_ctl);
}

static int syn_ptp_adjfine(void __iomem *mac_base, u32 addend)
{
	u32 value;

	/* Write addend value to MAC_Timestamp_Addend register */
	hal_write_reg(mac_base, SYN_MAC_TS_ADDEND, addend);

	/* Set TSADDREG bit to update the addend register */
	hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, SYN_MAC_TS_CTL_TSADDREG);

	/* Poll TSADDREG bit until cleared by hardware */
	return readl_poll_timeout_atomic(mac_base + SYN_MAC_TS_CTL, value,
					 !(value & SYN_MAC_TS_CTL_TSADDREG),
					 10, 100000);
}

/*
 * qcom_nss_ptp_adjfine()
 *	Adjust PHC frequency with SSINC compensation
 *
 * This function adjusts the clock frequency by modifying the addend register.
 * Unlike the standard adjust_by_scaled_ppm(), this implementation accounts for
 * the fact that SSINC may be rounded up (e.g., 3 ns instead of 2.666 ns for
 * 375 MHz clock), and the addend must compensate accordingly.
 *
 * The standard adjust_by_scaled_ppm() assumes:
 *   new_addend = default_addend × (1 + ppm/10^6)
 *
 * But with rounded SSINC, we need:
 *   Δaddend = (ptp_clock_rate × ppm × 2^32) / (10^9 × ssinc)
 *
 * This accounts for the SSINC multiplication factor in the hardware.
 */
static int qcom_nss_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	u32 addend;
	int ret;

	addend = (u32)adjust_by_scaled_ppm(ptp_priv->default_addend, scaled_ppm);

	dev_dbg(ptp_priv->dev,
		"adjfine: scaled_ppm=%ld default_addend=0x%08x new_addend=0x%08x\n",
		scaled_ppm, ptp_priv->default_addend, addend);

	/*
	 * Use ts_ctl_mutex (not the spinlock) because syn_ptp_adjfine() calls
	 * readl_poll_timeout_atomic() which may busy-wait up to 100 ms.
	 * Holding a spinlock with IRQs disabled for that duration would cause
	 * severe latency and potentially trigger the kernel watchdog.
	 */
	mutex_lock(&ptp_priv->ts_ctl_mutex);
	ret = syn_ptp_adjfine(mac_base, addend);
	mutex_unlock(&ptp_priv->ts_ctl_mutex);

	return ret;
}

/*
 * qcom_nss_ptp_adjtime()
 *	Adjust PHC time by offset
 *
 * This function adjusts the clock by adding or subtracting a time offset.
 */
static int qcom_nss_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	bool negative = false;
	u32 sec, nsec;
	int ret;

	/* Handle negative delta */
	if (delta < 0) {
		negative = true;
		delta = -delta;
	}

	/* Convert delta (nanoseconds) to seconds and nanoseconds */
	sec = div_u64_rem(delta, 1000000000ULL, &nsec);

	dev_dbg(ptp_priv->dev,
		"adjtime: delta=%lld ns (%s) -> sec=%u nsec=%u\n",
		negative ? -(s64)delta : (s64)delta,
		negative ? "subtract" : "add", sec, nsec);

	if (negative) {
		/*
		 * Per the XGMAC datasheet (MAC_System_Time_Seconds_Update /
		 * MAC_System_Time_Nanoseconds_Update), the hardware ALWAYS ADDS
		 * the update register values to the current time, regardless of
		 * the ADDSUB bit. To subtract, the values must be encoded as
		 * two's complement so that the addition wraps around:
		 *
		 *   TSS  = 2^32 - sec          (complement of seconds)
		 *   TSSS = 10^9 - nsec         (complement of nanoseconds,
		 *                               valid when TSCTRLSSR=1)
		 *
		 * Example from datasheet: to subtract 2.000000001 s,
		 *   TSS  = 0xFFFFFFFE (2^32 - 2)
		 *   TSSS = 0x3B9AC9FF (10^9 - 1)
		 *
		 * Note: the hardware adds TSS and TSSS independently with no
		 * carry between the two fields.
		 *
		 * Special case: if nsec == 0, the complement would be 10^9
		 * which overflows the 30-bit TSSS field; use 0 instead
		 * (adding 0 nanoseconds is correct when subtracting 0 ns).
		 */
		sec  = (u32)(0x100000000ULL - (u64)sec);
		nsec = (nsec != 0) ? (1000000000U - nsec) : 0U;
		nsec |= SYN_MAC_SYS_TIME_NSECS_UPDATE_ADDSUB;
	}

	/*
	 * Use ts_ctl_mutex (not the spinlock) because syn_ptp_time_set() calls
	 * readl_poll_timeout_atomic() which may busy-wait up to 100 ms.
	 */
	mutex_lock(&ptp_priv->ts_ctl_mutex);
	ret = syn_ptp_time_set(mac_base, SYN_MAC_TS_CTL_TSUPDT, sec, nsec);
	mutex_unlock(&ptp_priv->ts_ctl_mutex);

	return ret;
}

/*
 * __syn_ptp_read_sys_time_locked()
 *	Read XGMAC system time with rollover protection (caller must hold ptp_priv->lock)
 *
 * This is the single authoritative register-read primitive for the hardware PTP
 * clock. It must be called with ptp_priv->lock already held to prevent races
 * with concurrent adjtime/settime operations.
 *
 * Separating the lock-free core from the locking wrapper allows the IRQ handler
 * software fallback to reuse this logic without re-acquiring the spinlock
 * (which would deadlock, since the handler already holds the lock).
 *
 * @mac_base: MMIO base address of the XGMAC instance
 * @sec:  Output - seconds value from SYN_MAC_SYS_TIME_SECS
 * @nsec: Output - nanoseconds value from SYN_MAC_SYS_TIME_NSECS (masked to 31 bits)
 */
static void __syn_ptp_read_sys_time_locked(void __iomem *mac_base,
					   u32 *sec, u32 *nsec)
{
	u32 sec2;

	*sec  = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_SECS);
	*nsec = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_NSECS);
	sec2  = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_SECS);

	/* Re-read nanoseconds if seconds rolled over during the read sequence */
	if (*sec != sec2) {
		*sec  = sec2;
		*nsec = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_NSECS);
	}

	*nsec &= SYN_MAC_SYS_TIME_NSECS_MASK;
}

/*
 * syn_ptp_read_sys_time()
 *	Read current XGMAC system time with rollover protection
 *
 * Acquires ptp_priv->lock and delegates to __syn_ptp_read_sys_time_locked().
 * All external callers (gettimex64, debugfs stats) should use this wrapper.
 * The IRQ handler software fallback must call __syn_ptp_read_sys_time_locked()
 * directly because it already holds the lock.
 *
 * @ptp_priv: Pointer to PTP private data structure
 * @sec:  Output - seconds value from SYN_MAC_SYS_TIME_SECS
 * @nsec: Output - nanoseconds value from SYN_MAC_SYS_TIME_NSECS (masked to 31 bits)
 */
static void syn_ptp_read_sys_time(struct syn_ptp_priv *ptp_priv,
				  u32 *sec, u32 *nsec)
{
	void __iomem *mac_base = ptp_priv->shd->nghd.mac_base;
	unsigned long flags;

	spin_lock_irqsave(&ptp_priv->lock, flags);
	__syn_ptp_read_sys_time_locked(mac_base, sec, nsec);
	spin_unlock_irqrestore(&ptp_priv->lock, flags);
}

/*
 * qcom_nss_ptp_gettimex()
 *	Get current PHC time
 *
 * Reads the current hardware timestamp by delegating to syn_ptp_read_sys_time(),
 * which handles rollover protection and nanosecond masking.
 */
static int qcom_nss_ptp_gettimex(struct ptp_clock_info *ptp,
				 struct timespec64 *ts,
				 struct ptp_system_timestamp *sts)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	u32 sec, nsec;

	/*
	 * Capture the system clock immediately before and after reading the
	 * XGMAC hardware clock.  These timestamps are used by the kernel's
	 * PTP_SYS_OFFSET_PRECISE and PTP_SYS_OFFSET_EXTENDED ioctls to
	 * compute an accurate PHC-to-system-clock offset for phc2sys.
	 *
	 * Without these calls ptp_sys_offset_precise.sys_realtime is left as
	 * zero, causing phc2sys to compute a ~70-year offset and fail with
	 * "failed to step clock: Invalid argument".
	 *
	 * ptp_read_system_prets/postts are no-ops when sts == NULL (e.g.
	 * when called from the PTP_CLOCK_GETTIME ioctl path).
	 */
	ptp_read_system_prets(sts);
	syn_ptp_read_sys_time(ptp_priv, &sec, &nsec);
	ptp_read_system_postts(sts);

	ts->tv_sec  = sec;
	ts->tv_nsec = nsec;

	dev_dbg(ptp_priv->dev, "gettimex: sec=%u nsec=%u (ts=%llu ns)\n",
		sec, nsec, (u64)sec * NSEC_PER_SEC + nsec);

	return 0;
}

/*
 * qcom_nss_ptp_settime()
 *	Set PHC time
 *
 * This function initializes the hardware clock to a specific time.
 */
static int qcom_nss_ptp_settime(struct ptp_clock_info *ptp,
				const struct timespec64 *ts)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	int ret;

	/* Validate nanoseconds */
	if (ts->tv_nsec >= 1000000000ULL) {
		dev_err(ptp_priv->dev,
			"Invalid nanoseconds value: %ld\n",  ts->tv_nsec);
		return -EINVAL;
	}

	dev_dbg(ptp_priv->dev, "settime: sec=%lld nsec=%ld\n",
		ts->tv_sec, ts->tv_nsec);

	/*
	 * Use ts_ctl_mutex (not the spinlock) because syn_ptp_time_set() calls
	 * readl_poll_timeout_atomic() which may busy-wait up to 100 ms.
	 */
	mutex_lock(&ptp_priv->ts_ctl_mutex);
	ret = syn_ptp_time_set(mac_base, SYN_MAC_TS_CTL_TSINIT,
			       (u32)ts->tv_sec, (u32)ts->tv_nsec);
	mutex_unlock(&ptp_priv->ts_ctl_mutex);

	return ret;
}

/*
 * syn_ptp_spare2_pps_out_enable()
 *	Enable or disable PPS output via the SPARE2 register
 *
 * When PPS IN (EXTTS) is active the PPS signal must flow *into* the XGMAC,
 * so the SPARE2 PPS-output bit must be cleared.  When PPS OUT (PEROUT) is
 * active the bit must be set.
 *
 *   IPQ5210: controls PPS_EN  (bit 1) in SPARE2
 *   IPQ9650: controls TSN_EN  (bit 5) in SPARE2
 *
 * Acquires mgr->lock internally to protect the read-modify-write against
 * concurrent EXTTS and PEROUT calls.  Must not be called while holding a
 * spinlock (mgr->lock is a mutex and may sleep).
 *
 * @mgr:    Pointer to platform manager
 * @enable: true  -> set   the SPARE2 PPS-output bit (PPS OUT active)
 *          false -> clear the SPARE2 PPS-output bit (PPS IN  active / PPS OUT disabled)
 */
static void syn_ptp_spare2_pps_out_enable(struct syn_ptp_platform_mgr *mgr,
					  bool enable)
{
	u32 val, bit;

	if (!mgr->spare2)
		return;

	bit = (mgr->platform == PLATFORM_IPQ52XX)
	      ? IPQ52XX_SPARE2_PPS_EN : IPQ96XX_SPARE2_TSN_EN;

	/*
	 * mgr->lock ("Protects platform resources") serialises concurrent
	 * SPARE2 read-modify-write sequences from the EXTTS and PEROUT paths.
	 */
	mutex_lock(&mgr->lock);
	val = readl(mgr->spare2);
	if (enable)
		val |= bit;
	else
		val &= ~bit;
	writel(val, mgr->spare2);
	mutex_unlock(&mgr->lock);
}

static int qcom_nss_ptp_enable(struct ptp_clock_info *ptp,
			       struct ptp_clock_request *rq, int on)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	unsigned long flags;

	switch (rq->type) {
	case PTP_CLK_REQ_EXTTS: {
		struct syn_ptp_platform_mgr *mgr = ptp_priv->platform_mgr;
		unsigned long list_flags;

		spin_lock_irqsave(&ptp_priv->lock, flags);
		ptp_priv->pps_enabled = (on != 0);
		spin_unlock_irqrestore(&ptp_priv->lock, flags);

		if (on) {
			/*
			 * Reset the addend to the nominal default so the clock
			 * runs at its base rate when PPS IN is enabled.  ts2phc
			 * will fine-tune it via adjfine() as it synchronizes the
			 * XGMAC RTC to the PHY RTC.  Without this reset, a stale
			 * addend from a prior synchronization session could cause
			 * ts2phc to start from an already-drifted frequency
			 * baseline.
			 *
			 * Use ts_ctl_mutex (not the spinlock) because
			 * syn_ptp_adjfine() calls readl_poll_timeout_atomic()
			 * which may busy-wait up to 100 ms.
			 */
			mutex_lock(&ptp_priv->ts_ctl_mutex);
			syn_ptp_adjfine(mac_base, ptp_priv->default_addend);
			mutex_unlock(&ptp_priv->ts_ctl_mutex);

			/*
			 * PPS IN is being enabled: clear the SPARE2 PPS-output
			 * bit so the signal flows into the XGMAC rather than out.
			 * syn_ptp_spare2_pps_out_enable() acquires mgr->lock (a
			 * mutex) internally, so it must not be called while
			 * holding a spinlock.
			 */
			syn_ptp_spare2_pps_out_enable(mgr, false);
		}

		spin_lock_irqsave(&mgr->pps_list_lock, list_flags);
		if (on) {
			/*
			 * Add this instance to the platform-level PPS active list
			 * so the shared IRQ handler delivers events to it.
			 * list_add() is safe to call even if the node is already
			 * in the list only if we guard with list_empty(); use
			 * list_del_init() + list_add() to be idempotent.
			 */
			if (list_empty(&ptp_priv->pps_list_node))
				list_add(&ptp_priv->pps_list_node,
					 &mgr->pps_active_list);

			/* Enable XGMAC timestamping engine */
			hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, SYN_MAC_TS_CTL_TSENA);

			/*
			 * Enable auxiliary snapshot capture for trigger 0 (ATSEN0).
			 * The XGMAC will latch the system time into the AUX FIFO on
			 * each rising edge of the PHY PPS signal routed to AUX_IN0.
			 * Clear the FIFO first to discard any stale entries.
			 */
			hal_set_reg_bits(mac_base, SYN_MAC_AUX_CTRL, SYN_MAC_AUX_CTRL_ATSFC);
			hal_set_reg_bits(mac_base, SYN_MAC_AUX_CTRL, SYN_MAC_AUX_CTRL_ATSEN0);
		} else {
			/*
			 * Disable auxiliary snapshot capture for trigger 0 (ATSEN0).
			 */
			hal_clear_reg_bits(mac_base, SYN_MAC_AUX_CTRL, SYN_MAC_AUX_CTRL_ATSEN0);

			/*
			 * Remove this instance from the active list.
			 * list_del_init() is safe even if the node is not in any list.
			 */
			list_del_init(&ptp_priv->pps_list_node);

			/* Reset for next enable. */
			ptp_priv->aux_ts_was_synced = false;
		}
		spin_unlock_irqrestore(&mgr->pps_list_lock, list_flags);
		return 0;
	}

	case PTP_CLK_REQ_PEROUT:
		/*
		 * Called by ts2phc via PTP_PEROUT_REQUEST2 ioctl when /dev/ptp0
		 * is used as the PPS source (ts2phc -s /dev/ptp0 -c eth2).
		 */
		spin_lock_irqsave(&ptp_priv->lock, flags);

		/* Enable XGMAC timestamping engine (required for PPS output) */
		if (on)
			hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, SYN_MAC_TS_CTL_TSENA);

		spin_unlock_irqrestore(&ptp_priv->lock, flags);

		/*
		 * Enable/disable PPS output via SPARE2 register:
		 *   IPQ5210: set/clear PPS_EN (bit 1)
		 *   IPQ9650: set/clear TSN_EN (bit 5)
		 */
		syn_ptp_spare2_pps_out_enable(ptp_priv->platform_mgr, !!on);

		/* Configure PPE switch PPS output source */
		if (ptp_priv->platform_mgr->platform == PLATFORM_IPQ52XX ||
		    ptp_priv->platform_mgr->platform == PLATFORM_IPQ96XX) {
			fal_port_pps_ctrl_t pps_ctrl = {0};
			u32 mac_id = ptp_priv->shd->nghd.mac_id;

			/*
			 * On IPQ52XX, MAC IDs >= 4 are mapped to a different PPE
			 * port index. Subtract 3 to obtain the correct pps_out_sel
			 * index (e.g., mac_id=4 → index 0, mac_id=5 → index 1).
			 */
			if (ptp_priv->platform_mgr->platform == PLATFORM_IPQ52XX &&
			    mac_id >= 4)
				mac_id -= 3;

			if (WARN_ON(mac_id == 0))
				return -EINVAL;

			fal_port_pps_ctrl_get(0, &pps_ctrl);
			if (on) {
				/* Select this MAC as the PPS output source */
				pps_ctrl.pps_out_sel = mac_id - 1;
			} else {
				/* Release PPS output if this MAC currently owns it */
				if (pps_ctrl.pps_out_sel == mac_id - 1)
					pps_ctrl.pps_out_sel = 0;
			}
			fal_port_pps_ctrl_set(0, &pps_ctrl);
		}
		return 0;

	default:
		return -EOPNOTSUPP;
	}
}

/*
 * qcom_nss_ptp_verify()
 *	Verify pin configuration
 *
 * This function validates pin configuration requests.
 * We support pin 0 for both external timestamps (PPS input) and
 * periodic output (PPS output, used by ts2phc as PPS source).
 */
static int qcom_nss_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
			       enum ptp_pin_function func, unsigned int chan)
{
	/* Only accept pin 0 */
	if (pin != 0)
		return -EINVAL;

	switch (func) {
	case PTP_PF_EXTTS:
	case PTP_PF_PEROUT:
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * qcom_nss_ptp_irq_handler_thread()
 *	Threaded interrupt handler for PPS signals with hardware timestamp capture
 *
 * The PPS_IN IRQ is a single platform-level resource shared by all XGMAC
 * instances. It is registered once (with the platform manager as private data)
 * and iterates platform_mgr->pps_active_list to deliver an independent
 * PPS event to every XGMAC instance that has enabled PPS capture via
 * PTP_CLK_REQ_EXTTS (qcom_nss_ptp_enable()). Multiple instances can be
 * synchronized simultaneously.
 *
 * This function first attempts to read the hardware-captured auxiliary
 * timestamp from each XGMAC's AUX FIFO (SYN_MAC_AUX_TS_NSECS/SECS), which
 * was latched at the exact PPS edge. This provides nanosecond-accurate
 * timestamps for ts2phc to use when synchronizing each XGMAC RTC to the
 * PHY RTC.
 *
 * If no hardware snapshot is available (ATSEN0 not connected or FIFO empty),
 * it falls back to a software timestamp by reading the XGMAC system time
 * at interrupt handler execution time (~1-10 µs latency jitter).
 *
 * Hardware path (preferred):
 *   PPS edge → XGMAC latches time → FIFO → IRQ fires → read FIFO
 *
 * Software fallback:
 *   PPS edge → GPIO IRQ fires → read XGMAC system time now
 *
 * @irq: IRQ number
 * @priv: Pointer to platform manager (syn_ptp_platform_mgr)
 *
 * Returns: IRQ_HANDLED
 */
static irqreturn_t qcom_nss_ptp_irq_handler_thread(int irq, void *priv)
{
	struct syn_ptp_platform_mgr *mgr = (struct syn_ptp_platform_mgr *)priv;
	struct syn_ptp_priv *ptp_priv;
	unsigned long list_flags;

	/*
	 * Iterate all XGMAC instances that have PPS capture enabled and
	 * deliver an independent timestamp event to each one.
	 *
	 * Each XGMAC has its own AUX FIFO and system-time registers, so
	 * timestamps are read independently per instance.
	 *
	 * pps_list_lock is a spinlock so it is safe to acquire here in the
	 * threaded IRQ context. list_for_each_entry() is safe because
	 * qcom_nss_ptp_enable() and syn_ptp_cleanup() also hold
	 * pps_list_lock when modifying the list.
	 */
	spin_lock_irqsave(&mgr->pps_list_lock, list_flags);
	list_for_each_entry(ptp_priv, &mgr->pps_active_list, pps_list_node) {
		void __iomem *mac_base = ptp_priv->shd->nghd.mac_base;
		struct ptp_clock_event event;
		unsigned long flags;
		u32 status, nsec, sec, num_snapshots, dist;
		bool use_hw_ts = false;

		spin_lock_irqsave(&ptp_priv->lock, flags);

		if (!ptp_priv->pps_enabled) {
			spin_unlock_irqrestore(&ptp_priv->lock, flags);
			continue;
		}

		/*
		 * Read MAC_Timestamp_Status to check auxiliary snapshot availability.
		 * AUXTSTRIG (bit 2): set when at least one snapshot is in the FIFO.
		 * ATSNS (bits 29:25): number of snapshots currently in the FIFO.
		 * ATSSTM (bit 24): set if a snapshot was missed due to FIFO overflow.
		 */
		status = hal_read_reg(mac_base, SYN_MAC_TS_STATUS);

		/* Warn on FIFO overflow (missed snapshot) */
		if (status & SYN_MAC_TS_STATUS_ATSSTM) {
			ptp_priv->aux_ts_missed++;
			dev_warn_ratelimited(ptp_priv->dev,
					     "Auxiliary timestamp FIFO overflow (total missed=%u)\n",
					     ptp_priv->aux_ts_missed);
		}

		num_snapshots = (status & SYN_MAC_TS_STATUS_ATSNS_MASK)
				>> SYN_MAC_TS_STATUS_ATSNS_SHIFT;

		if ((status & SYN_MAC_TS_STATUS_AUXTSTRIG) && num_snapshots > 0) {
			/*
			 * Hardware auxiliary timestamp available.
			 *
			 * Read one entry from the AUX FIFO. Reading
			 * SYN_MAC_AUX_TS_NSECS pops the entry from the FIFO.
			 * Read order: NSECS first (pops the FIFO entry), then SECS.
			 */
			nsec = hal_read_reg(mac_base, SYN_MAC_AUX_TS_NSECS);
			nsec &= SYN_MAC_AUX_TS_NSECS_MASK;
			sec  = hal_read_reg(mac_base, SYN_MAC_AUX_TS_SECS);
			use_hw_ts = true;
			ptp_priv->aux_ts_hw_count++;

			/*
			 * Glitch detection (post-sync only):
			 *
			 * At startup the XGMAC clock is not yet aligned to wall
			 * time, so captures far from a second boundary are normal
			 * and must not be counted as glitches.
			 *
			 * Once aux_ts_was_synced is true (a prior capture was
			 * within SYN_PTP_PPS_BOUNDARY_THRESHOLD_NS of a second
			 * boundary), any capture far from the boundary is logged
			 * as a possible spurious GPIO glitch, counted, and skipped
			 * (not delivered to ts2phc).
			 */
			dist = (nsec > 500000000U) ? (1000000000U - nsec) : nsec;

			if (dist <= SYN_PTP_PPS_BOUNDARY_THRESHOLD_NS) {
				ptp_priv->aux_ts_was_synced = true;
			} else if (ptp_priv->aux_ts_was_synced) {
				dev_dbg(ptp_priv->dev,
					"PPS glitch: ts=%u.%09u dist=%u ns from boundary, skipped\n",
					sec, nsec, dist);
				ptp_priv->aux_ts_glitch++;
				spin_unlock_irqrestore(&ptp_priv->lock, flags);
				continue;
			}
		} else {
			/*
			 * No hardware snapshot available. Fall back to software
			 * timestamp: read this XGMAC's system time now.
			 * The lock is already held; call the lock-free helper.
			 */
			__syn_ptp_read_sys_time_locked(mac_base, &sec, &nsec);
			ptp_priv->aux_ts_sw_fallback++;
		}

		spin_unlock_irqrestore(&ptp_priv->lock, flags);

		/*
		 * Deliver external timestamp event to the PTP subsystem.
		 * ts2phc reads this via read(fd, &event, sizeof(event)) on the
		 * PHC fd and uses it to compute the offset between PHY and
		 * this XGMAC's RTC.
		 */
		event.type      = PTP_CLOCK_EXTTS;
		event.index     = 0;
		event.timestamp = (u64)sec * NSEC_PER_SEC + nsec;
		ptp_clock_event(ptp_priv->clock, &event);

		dev_dbg(ptp_priv->dev,
			"PPS captured at %u.%09u (%s timestamp, hw=%u sw=%u missed=%u)\n",
			sec, nsec,
			use_hw_ts ? "hardware" : "software",
			ptp_priv->aux_ts_hw_count,
			ptp_priv->aux_ts_sw_fallback,
			ptp_priv->aux_ts_missed);
	}
	spin_unlock_irqrestore(&mgr->pps_list_lock, list_flags);

	return IRQ_HANDLED;
}

/*
 * syn_ptp_platform_detect()
 *	Detect platform type (IPQ52XX/IPQ96XX/Unknown)
 *
 * Returns: enum platform_type indicating the detected platform
 */
static enum platform_type syn_ptp_platform_detect(void)
{
#if defined(NSS_DP_IPQ96XX)
	return PLATFORM_IPQ96XX;
#elif defined(NSS_DP_IPQ52XX)
	return PLATFORM_IPQ52XX;
#endif
	return PLATFORM_UNKNOWN;
}

/*
 * syn_ptp_ipq52xx_init()
 *	Initialize IPQ52XX platform-specific PPS configuration
 *
 * This function configures the ToP TCSR registers and GPIO for PPS
 * signal routing on the IPQ52XX platform.
 *
 * @mgr: Pointer to platform manager structure
 * @dev: Pointer to device structure for logging
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_ipq52xx_init(struct syn_ptp_platform_mgr *mgr, struct device *dev)
{
	/* Map ToP TCSR PPS_IN register (keep mapped for runtime access) */
	mgr->tcsr_pps_in = devm_ioremap(dev, IPQ52XX_TCSR_PPS_IN_REG, 4);
	if (!mgr->tcsr_pps_in) {
		dev_err(dev, "Failed to map TCSR PPS_IN register\n");
		return -ENOMEM;
	}

	/* Map ToP TCSR PPS_OUT register (keep mapped for runtime access) */
	mgr->tcsr_pps_out = devm_ioremap(dev, IPQ52XX_TCSR_PPS_OUT_REG, 4);
	if (!mgr->tcsr_pps_out) {
		dev_err(dev, "Failed to map TCSR PPS_OUT register\n");
		return -ENOMEM;
	}

	/* Configure TCSR PPS_IN register
	 * Set to 0 for external PHY PPS input (default)
	 */
	writel(0x0, mgr->tcsr_pps_in);
	mgr->pps_in_source = 0;

	/* Configure TCSR PPS_OUT register
	 * Set to 0 for NSS_PPS_OUT (PPE/XGMAC) (default)
	 */
	writel(0x0, mgr->tcsr_pps_out);
	mgr->pps_out_source = 0;

	/* Map SPARE2 register for PPS output enable control */
	mgr->spare2 = devm_ioremap(dev, IPQ52XX_SPARE2_REG, 4);
	if (!mgr->spare2)
		dev_warn(dev, "Failed to map SPARE2 register, PPS output control unavailable\n");

	return 0;
}

/*
 * syn_ptp_ipq96xx_init()
 *	Initialize IPQ96XX platform-specific PPS configuration
 *
 * This function maps the SPARE2 register for PPS output enable control
 * on the IPQ96XX platform.
 *
 * @mgr: Pointer to platform manager structure
 * @dev: Pointer to device structure for logging
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_ipq96xx_init(struct syn_ptp_platform_mgr *mgr, struct device *dev)
{
	/* Map SPARE2 register for PPS output enable control */
	mgr->spare2 = devm_ioremap(dev, IPQ96XX_SPARE2_REG, 4);
	if (!mgr->spare2)
		dev_warn(dev, "Failed to map SPARE2 register, PPS output control unavailable\n");

	return 0;
}

/*
 * syn_ptp_platform_mgr_get()
 *	Get or create the singleton platform manager
 *
 * This function implements the singleton pattern for platform_mgr.
 * On first call, it allocates and initializes the singleton, detects
 * the platform, and performs platform-specific initialization.
 * On subsequent calls, it increments the reference count.
 *
 * @pdev: Pointer to platform device structure
 *
 * Returns: Pointer to platform manager on success, NULL on failure
 */
static struct syn_ptp_platform_mgr *syn_ptp_platform_mgr_get(struct platform_device *pdev)
{
	struct syn_ptp_platform_mgr *mgr;
	struct device *dev = &pdev->dev;

	mutex_lock(&platform_mgr_mutex);

	if (g_platform_mgr) {
		/* Already exists, increment refcount */
		g_platform_mgr->refcount++;
		mutex_unlock(&platform_mgr_mutex);
		dev_dbg(dev, "Platform manager refcount incremented to %d\n",
			g_platform_mgr->refcount);
		return g_platform_mgr;
	}

	/* First instance - create the singleton */
	mgr = devm_kzalloc(dev, sizeof(*mgr), GFP_KERNEL);
	if (!mgr) {
		mutex_unlock(&platform_mgr_mutex);
		dev_err(dev, "Failed to allocate platform manager\n");
		return NULL;
	}

	/* Initialize singleton */
	mutex_init(&mgr->lock);
	mgr->refcount = 1;
	mgr->platform = PLATFORM_UNKNOWN;
	mgr->pps_in_irq = -1;
	mgr->tcsr_pps_in = NULL;
	mgr->tcsr_pps_out = NULL;
	mgr->pps_in_source = 0;
	mgr->pps_out_source = 0;
	mgr->spare2 = NULL;
	mgr->dev = dev;
	INIT_LIST_HEAD(&mgr->pps_active_list);
	spin_lock_init(&mgr->pps_list_lock);

	/* Get PPS_IN interrupt from DTS by name */
	mgr->pps_in_irq = platform_get_irq_byname_optional(pdev, "pps_in");

	/* Detect platform type */
	mgr->platform = syn_ptp_platform_detect();

	/* Initialize platform-specific PPS configuration */
	if (mgr->platform == PLATFORM_IPQ52XX) {
		int ret = syn_ptp_ipq52xx_init(mgr, &pdev->dev);
		if (ret) {
			dev_warn(&pdev->dev, "IPQ52XX platform init failed: %d, continuing without PPS\n", ret);
			/* Continue without PPS support */
		}
	} else if (mgr->platform == PLATFORM_IPQ96XX) {
		int ret = syn_ptp_ipq96xx_init(mgr, &pdev->dev);
		if (ret) {
			dev_warn(&pdev->dev, "IPQ96XX platform init failed: %d, continuing without PPS\n", ret);
			/* Continue without PPS support */
		}
	}

	g_platform_mgr = mgr;

	/* Store platform manager in device driver data for sysfs access */
	dev_set_drvdata(dev, mgr);

	mutex_unlock(&platform_mgr_mutex);
	return mgr;
}

/*
 * syn_ptp_platform_mgr_put()
 *	Release reference to platform manager
 *
 * This function decrements the reference count. When the count reaches
 * zero, it performs platform cleanup and frees the singleton.
 */
static void syn_ptp_platform_mgr_put(void)
{
	mutex_lock(&platform_mgr_mutex);

	if (!g_platform_mgr) {
		mutex_unlock(&platform_mgr_mutex);
		return;
	}

	g_platform_mgr->refcount--;

	if (g_platform_mgr->refcount == 0) {
		/*
		 * Last instance released — free the PPS_IN IRQ if it was
		 * registered. The IRQ was registered with the platform manager
		 * as private data (not with any individual ptp_priv), so it
		 * must be freed here rather than in syn_ptp_cleanup().
		 */
		if (g_platform_mgr->pps_in_irq_registered &&
		    g_platform_mgr->pps_in_irq >= 0) {
			free_irq(g_platform_mgr->pps_in_irq, g_platform_mgr);
			g_platform_mgr->pps_in_irq_registered = false;
			pr_info("nss-ptp: Unregistered PPS_IN interrupt (IRQ=%d)\n",
				g_platform_mgr->pps_in_irq);
		}
		g_platform_mgr = NULL;
	}

	mutex_unlock(&platform_mgr_mutex);
}

/*
 * pps_in_source_show()
 *	Show current PPS_IN source selection via debugfs
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int pps_in_source_show(struct seq_file *m, void *v)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	const char *source_str;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX) {
		seq_printf(m, "Not available on this platform\n");
		return 0;
	}

	switch (mgr->pps_in_source) {
	case 0:
	case 3:
		source_str = "external_phy";
		break;
	case 1:
		source_str = "pon_mac";
		break;
	case 2:
		source_str = "internal_phy";
		break;
	default:
		source_str = "unknown";
	}

	seq_printf(m, "%s\n", source_str);
	return 0;
}

/*
 * pps_in_source_open()
 *	Open callback for pps_in_source debugfs file
 */
static int pps_in_source_open(struct inode *inode, struct file *file)
{
	return single_open(file, pps_in_source_show, inode->i_private);
}

/*
 * pps_in_source_write()
 *	Write callback for pps_in_source debugfs file
 *
 * Valid values: "external_phy", "pon_mac", "internal_phy"
 */
static ssize_t pps_in_source_write(struct file *file, const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	char buf[32];
	size_t len;
	u32 value;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX)
		return -ENODEV;

	if (!mgr->tcsr_pps_in)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	/* Remove trailing newline if present */
	if (len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	/* Parse input string */
	if (strcmp(buf, "external_phy") == 0)
		value = 0;
	else if (strcmp(buf, "pon_mac") == 0)
		value = 1;
	else if (strcmp(buf, "internal_phy") == 0)
		value = 2;
	else {
		pr_err("PPS_IN source: invalid value '%s'. Valid values: external_phy, pon_mac, internal_phy\n",
		       buf);
		return -EINVAL;
	}

	/* Write to TCSR PPS_IN register */
	writel(value, mgr->tcsr_pps_in);

	/* Update cached value */
	mgr->pps_in_source = value;

	pr_info("PPS_IN source set to %s (value=%u)\n",
		value == 0 ? "external_phy" :
		value == 1 ? "pon_mac" : "internal_phy", value);

	return count;
}

/* File operations for pps_in_source debugfs file */
static const struct file_operations pps_in_source_fops = {
	.open = pps_in_source_open,
	.read = seq_read,
	.write = pps_in_source_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * pps_out_source_show()
 *	Show current PPS_OUT source selection via debugfs
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int pps_out_source_show(struct seq_file *m, void *v)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	const char *source_str;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX) {
		seq_printf(m, "Not available on this platform\n");
		return 0;
	}

	switch (mgr->pps_out_source) {
	case 0:
		source_str = "nss_pps";
		break;
	case 1:
		source_str = "external_pps";
		break;
	default:
		source_str = "unknown";
	}

	seq_printf(m, "%s\n", source_str);
	return 0;
}

/*
 * pps_out_source_open()
 *	Open callback for pps_out_source debugfs file
 */
static int pps_out_source_open(struct inode *inode, struct file *file)
{
	return single_open(file, pps_out_source_show, inode->i_private);
}

/*
 * pps_out_source_write()
 *	Write callback for pps_out_source debugfs file
 *
 * Valid values: "nss_pps", "external_pps"
 */
static ssize_t pps_out_source_write(struct file *file, const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	char buf[32];
	size_t len;
	u32 value;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX)
		return -ENODEV;

	if (!mgr->tcsr_pps_out)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	/* Remove trailing newline if present */
	if (len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	/* Parse input string */
	if (strcmp(buf, "nss_pps") == 0)
		value = 0;
	else if (strcmp(buf, "external_pps") == 0)
		value = 1;
	else {
		pr_err("PPS_OUT source: invalid value '%s'. Valid values: nss_pps, external_pps\n",
		       buf);
		return -EINVAL;
	}

	/* Write to TCSR PPS_OUT register */
	writel(value, mgr->tcsr_pps_out);

	/* Update cached value */
	mgr->pps_out_source = value;

	pr_info("PPS_OUT source set to %s (value=%u)\n",
		value == 0 ? "nss_pps" : "external_pps", value);

	return count;
}

/* File operations for pps_out_source debugfs file */
static const struct file_operations pps_out_source_fops = {
	.open = pps_out_source_open,
	.read = seq_read,
	.write = pps_out_source_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * calculate_ptp_parameters()
 *	Calculate PTP parameters based on actual PTP reference clock
 *
 * The XGMAC PTP hardware uses two registers to control timestamp increment:
 * 1. SSINC (Sub-Second Increment): Integer nanoseconds added per clock tick
 * 2. Addend: Controls fractional adjustment via accumulator
 *
 * The timestamp counter increments by SSINC nanoseconds each time the
 * internal accumulator overflows (reaches 2^32). The accumulator adds
 * 'addend' on each reference clock cycle.
 *
 * For accurate 1 second = 10^9 nanoseconds:
 * - Accumulator overflows per second = ptp_clock_rate × addend / 2^32
 * - Nanoseconds per second = overflows_per_sec × ssinc = 10^9
 *
 * Therefore: addend = (10^9 × 2^32) / (ssinc × ptp_clock_rate)
 *
 * IMPORTANT: SSINC must be an integer, so we use ceiling division to round up.
 * This ensures we never lose precision due to truncation. The addend value
 * automatically compensates by being smaller than 0xFFFFFFFF.
 *
 * Example for 375 MHz clock:
 * - Ideal: 1,000,000,000 / 375,000,000 = 2.666... ns per tick
 * - SSINC: 3 ns (rounded up)
 * - Addend: ~0xE38E38E3 (compensates for the 0.333 ns excess per tick)
 *
 * @ptp_clock_rate: PTP reference clock frequency in Hz (after division)
 * @ssinc: Output - sub-second increment in nanoseconds (rounded up)
 * @default_addend: Output - default addend value for timestamp accumulator
 *
 * Returns: 0 on success, -EINVAL on invalid parameters
 */
static int calculate_ptp_parameters(u32 ptp_clock_rate,
				    u32 *ssinc, u32 *default_addend)
{
	u64 temp;

	/* Validate input parameters */
	if (!ptp_clock_rate || !ssinc || !default_addend) {
		pr_err("Invalid parameters: ptp_clock_rate=%u, ssinc=%p, default_addend=%p\n",
		       ptp_clock_rate, ssinc, default_addend);
		return -EINVAL;
	}

	/* Calculate SSINC: nanoseconds per clock tick (rounded UP)
	 * Use ceiling division to ensure we never truncate and lose precision.
	 * The addend will compensate for any excess.
	 *
	 * For example, 375 MHz: ceil(10^9 / 375×10^6) = ceil(2.666) = 3 ns
	 */
	*ssinc = (u32)DIV_ROUND_UP_ULL(NSEC_PER_SEC, ptp_clock_rate);

	/* Calculate addend to achieve accurate 10^9 ns per second
	 * Formula: addend = (10^9 × 2^32) / (ssinc × ptp_clock_rate)
	 *
	 * This ensures: (addend × ptp_clock_rate / 2^32) × ssinc = 10^9 ns/sec
	 *
	 * With rounded-up SSINC, the addend will be less than 0xFFFFFFFF,
	 * which compensates for the extra nanoseconds per tick.
	 */
	temp = NSEC_PER_SEC;
	temp <<= 32;  /* 10^9 × 2^32 */
	temp = div_u64(temp, *ssinc);  /* ÷ ssinc (rounded up) */
	*default_addend = (u32)div_u64(temp, ptp_clock_rate);  /* ÷ ptp_clock_rate */

	/* Sanity check: addend should be non-zero */
	if (*default_addend == 0) {
		pr_err("Calculated addend is zero (clock_rate=%u Hz too high)\n",
		       ptp_clock_rate);
		return -EINVAL;
	}

	pr_debug("PTP params: clock=%u Hz, ssinc=%u ns, addend=0x%08x\n",
		 ptp_clock_rate, *ssinc, *default_addend);

	return 0;
}

/*
 * syn_ptp_stats_show()
 *	Display PTP statistics via debugfs
 *
 * This function displays comprehensive PTP statistics including:
 * - PHC information
 * - XGMAC system time
 * - TX timestamp statistics
 * - TX timestamp queue status
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int syn_ptp_stats_show(struct seq_file *m, void *v)
{
	struct syn_ptp_priv *ptp_priv = (struct syn_ptp_priv *)m->private;
	void __iomem *mac_base;
	unsigned long flags;
	u32 ts_sec, ts_nsec;
	struct tm tm;
	ktime_t now;
	int i;

	if (!ptp_priv) {
		seq_printf(m, "PTP not initialized\n");
		return 0;
	}

	/* Print banner */
	seq_printf(m, "========================================\n");
	seq_printf(m, "PTP Statistics for %s\n", ptp_priv->caps.name);
	seq_printf(m, "========================================\n\n");

	mac_base = ptp_priv->shd->nghd.mac_base;

	/* PHC Information */
	seq_printf(m, "PHC Information:\n");
	seq_printf(m, "  PHC Index: %d\n", ptp_clock_index(ptp_priv->clock));
	seq_printf(m, "  PTP Clock Rate: %u Hz\n", ptp_priv->ptp_clock_rate);
	seq_printf(m, "  SSINC: %u ns\n", ptp_priv->ssinc);
	seq_printf(m, "  Default Addend: 0x%08x\n", ptp_priv->default_addend);
	seq_printf(m, "  Current Addend: 0x%08x\n",
		   hal_read_reg(mac_base, SYN_MAC_TS_ADDEND));
	seq_printf(m, "  PPS Enabled: %s\n", ptp_priv->pps_enabled ? "Yes" : "No");
	seq_printf(m, "  Timestamping Enabled: %s\n",
		   (hal_read_reg(mac_base, SYN_MAC_TS_CTL) & SYN_MAC_TS_CTL_TSENA) ? "Yes" : "No");
	if (ptp_priv->platform_mgr->spare2) {
		u32 spare2_val = readl(ptp_priv->platform_mgr->spare2);
		u32 bit = (ptp_priv->platform_mgr->platform == PLATFORM_IPQ52XX)
			  ? IPQ52XX_SPARE2_PPS_EN : IPQ96XX_SPARE2_TSN_EN;
		seq_printf(m, "  PPS Output Enabled:   %s\n",
			   (spare2_val & bit) ? "Yes" : "No");
	}
	seq_printf(m, "\n");

	/* XGMAC System Time */
	syn_ptp_read_sys_time(ptp_priv, &ts_sec, &ts_nsec);
	time64_to_tm((time64_t)ts_sec, 0, &tm);
	seq_printf(m, "XGMAC System Time:\n");
	seq_printf(m, "  Seconds:     %u\n", ts_sec);
	seq_printf(m, "  Nanoseconds: %u\n", ts_nsec);
	seq_printf(m, "  Time:        %04ld-%02d-%02d %02d:%02d:%02d.%09u UTC\n",
		   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		   tm.tm_hour, tm.tm_min, tm.tm_sec, ts_nsec);
	seq_printf(m, "\n");

	/* TX Timestamp Statistics */
	seq_printf(m, "TX Timestamp Statistics:\n");
	seq_printf(m, "  FIFO Full Count: %u\n", ptp_priv->tx_ts_fifo_full_count);
	seq_printf(m, "  Match Success: %u\n", ptp_priv->tx_ts_match_success);
	seq_printf(m, "  Match Failures: %u\n", ptp_priv->tx_ts_match_fail);
	seq_printf(m, "  Stale Timestamps: %u\n", ptp_priv->tx_ts_stale_count);
	seq_printf(m, "  Slot Conflicts: %u\n", ptp_priv->tx_ts_slot_conflict_count);
	seq_printf(m, "\n");

	/* RX Timestamp Statistics */
	seq_printf(m, "RX Timestamp Statistics:\n");
	seq_printf(m, "  Success: %u\n", ptp_priv->rx_ts_success);
	seq_printf(m, "  Filtered: %u\n", ptp_priv->rx_ts_filtered);
	seq_printf(m, "\n");

	/* TX Timestamp Queue Status */
	seq_printf(m, "TX Timestamp Queue Status:\n");
	now = ktime_get();
	spin_lock_irqsave(&ptp_priv->tx_ts_lock, flags);
	for (i = 0; i < SYN_MAC_PTP_TXTS_FIFO_NUM; i++) {
		if (ptp_priv->tx_ts_queue[i].valid) {
			s64 age_ms = ktime_ms_delta(now, ptp_priv->tx_ts_queue[i].capture_time);
			seq_printf(m, "  Slot %d: [Valid] pkt_id=%u, timestamp=%llu ns, age=%lld ms\n",
				   i, ptp_priv->tx_ts_queue[i].pkt_id,
				   ptp_priv->tx_ts_queue[i].timestamp_ns, age_ms);
		} else {
			seq_printf(m, "  Slot %d: [Empty]\n", i);
		}
	}
	spin_unlock_irqrestore(&ptp_priv->tx_ts_lock, flags);
	seq_printf(m, "\n");

	/* Auxiliary Timestamp Statistics (for ts2phc PHY→XGMAC sync) */
	seq_printf(m, "Auxiliary Timestamp Statistics:\n");
	seq_printf(m, "  Hardware captures (ATSEN0 FIFO): %u\n",
		   ptp_priv->aux_ts_hw_count);
	seq_printf(m, "  Software fallbacks (FIFO empty): %u\n",
		   ptp_priv->aux_ts_sw_fallback);
	seq_printf(m, "  Missed (FIFO overflow, ATSSTM): %u\n",
		   ptp_priv->aux_ts_missed);
	seq_printf(m, "  Glitches (>%u ns from boundary): %u\n",
		   SYN_PTP_PPS_BOUNDARY_THRESHOLD_NS,
		   ptp_priv->aux_ts_glitch);
	seq_printf(m, "\n");

	/* Timestamp Configuration */
	seq_printf(m, "Timestamp Configuration:\n");
	seq_printf(m, "  TX Type: %d\n", ptp_priv->tstamp_config.tx_type);
	seq_printf(m, "  RX Filter: %d\n", ptp_priv->tstamp_config.rx_filter);
	seq_printf(m, "\n");

	return 0;
}

/*
 * syn_ptp_stats_open()
 *	Open callback for PTP stats debugfs file
 *
 * @inode: inode structure
 * @file: file structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, syn_ptp_stats_show, inode->i_private);
}

/*
 * syn_ptp_stats_fops
 *	File operations for PTP stats debugfs file
 */
static const struct file_operations syn_ptp_stats_fops = {
	.open = syn_ptp_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * syn_ptp_debugfs_init()
 *	Initialize PTP debugfs interface
 *
 * This function creates a debugfs file at /sys/kernel/debug/qca-nss-dp/ptp/stats_<mac_id>
 * to display PTP statistics for this XGMAC instance.
 *
 * @ptp_priv: Pointer to PTP private data structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_debugfs_init(struct syn_ptp_priv *ptp_priv)
{
	struct dentry *root_dentry;
	char filename[32];

	mutex_lock(&ptp_debugfs_mutex);

	/* Lookup the root debugfs directory created by EDMA */
	root_dentry = debugfs_lookup("qca-nss-dp", NULL);
	if (!root_dentry) {
		mutex_unlock(&ptp_debugfs_mutex);
		dev_warn(ptp_priv->dev, "EDMA root debugfs directory not found\n");
		return -ENOENT;
	}

	/* Create PTP subdirectory on first call */
	if (!g_ptp_debugfs_dir) {
		g_ptp_debugfs_dir = debugfs_create_dir("ptp", root_dentry);
		if (!g_ptp_debugfs_dir) {
			mutex_unlock(&ptp_debugfs_mutex);
			dev_err(ptp_priv->dev, "Failed to create PTP debugfs directory\n");
			return -ENOMEM;
		}
	}

	/* Increment refcount for PTP debugfs directory */
	g_ptp_debugfs_refcount++;

	mutex_unlock(&ptp_debugfs_mutex);

	/* Create PTP stats file inside ptp subdirectory */
	snprintf(filename, sizeof(filename), "stats_%d", ptp_priv->shd->nghd.mac_id);
	ptp_priv->ptp_dentry = debugfs_create_file(filename, S_IRUGO, g_ptp_debugfs_dir,
						   ptp_priv, &syn_ptp_stats_fops);
	if (!ptp_priv->ptp_dentry) {
		dev_err(ptp_priv->dev, "Failed to create PTP debugfs file\n");

		/* Decrement refcount and cleanup directory if this was the first instance */
		mutex_lock(&ptp_debugfs_mutex);
		g_ptp_debugfs_refcount--;
		if (g_ptp_debugfs_refcount == 0) {
			debugfs_remove(g_ptp_debugfs_dir);
			g_ptp_debugfs_dir = NULL;
		}
		mutex_unlock(&ptp_debugfs_mutex);

		return -ENOMEM;
	}

	/* Create PPS control files (only once, not per-MAC) */
	mutex_lock(&ptp_debugfs_mutex);
	if (g_ptp_debugfs_refcount == 1) {
		/* First instance - create PPS control files */
		struct dentry *pps_in_dentry, *pps_out_dentry;

		/* Create IPQ52XX-specific files */
		if (ptp_priv->platform_mgr->platform == PLATFORM_IPQ52XX) {
			pps_in_dentry = debugfs_create_file("pps_in_source", 0644, g_ptp_debugfs_dir,
							    NULL, &pps_in_source_fops);
			if (!pps_in_dentry) {
				dev_warn(ptp_priv->dev, "Failed to create pps_in_source debugfs file\n");
			}

			pps_out_dentry = debugfs_create_file("pps_out_source", 0644, g_ptp_debugfs_dir,
							     NULL, &pps_out_source_fops);
			if (!pps_out_dentry) {
				dev_warn(ptp_priv->dev, "Failed to create pps_out_source debugfs file\n");
			}
		}
	}
	mutex_unlock(&ptp_debugfs_mutex);

	return 0;
}

/*
 * syn_ptp_debugfs_exit()
 *	Cleanup PTP debugfs interface
 *
 * This function removes the PTP debugfs file and cleans up the PTP
 * subdirectory when the last instance is removed.
 *
 * @ptp_priv: Pointer to PTP private data structure
 */
static void syn_ptp_debugfs_exit(struct syn_ptp_priv *ptp_priv)
{
	if (ptp_priv->ptp_dentry) {
		debugfs_remove(ptp_priv->ptp_dentry);
		ptp_priv->ptp_dentry = NULL;
	}

	/* Decrement refcount and remove PTP directory if this is the last instance */
	mutex_lock(&ptp_debugfs_mutex);
	g_ptp_debugfs_refcount--;
	if (g_ptp_debugfs_refcount == 0 && g_ptp_debugfs_dir) {
		debugfs_remove(g_ptp_debugfs_dir);
		g_ptp_debugfs_dir = NULL;
	}
	mutex_unlock(&ptp_debugfs_mutex);
}

/*
 * syn_ptp_hw_init()
 *	Initialize PTP hardware following XGMAC datasheet guidelines
 *
 * This function implements the 7-step initialization sequence from the
 * XGMAC PTP datasheet (Section 14.26.1):
 * 1. Mask timestamp trigger interrupt
 * 2. Enable timestamping (TSENA)
 * 3. Program sub-second increment based on PTP clock frequency
 * 4. Select Fine correction method
 * 5-6. Initialize timestamp counter to 0
 * 6b. Initialize asymmetry correction registers
 *
 * @ptp_priv: Pointer to PTP private data structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_hw_init(struct syn_ptp_priv *ptp_priv)
{
	void __iomem *mac_base = ptp_priv->shd->nghd.mac_base;
	int ret;

	/* Step 1: Mask timestamp trigger interrupt (bit 12) */
	hal_clear_reg_bits(mac_base, SYN_MAC_INT_ENABLE, SYN_MAC_INT_ENABLE_TSIE);

	/* Step 2: Enable Timestamp, Fine mode, and Timestamp Digital Rollover Control */
	hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL,
			 SYN_MAC_TS_CTL_TSENA | SYN_MAC_TS_CTL_TSCFUPDT | SYN_MAC_TS_CTL_TSCTRLSSR);

	/* Step 3: Calculate PTP parameters using datasheet formula */
	ret = calculate_ptp_parameters(ptp_priv->ptp_clock_rate,
				       &ptp_priv->ssinc, &ptp_priv->default_addend);
	if (ret) {
		dev_err(ptp_priv->dev, "Failed to calculate PTP parameters: %d\n", ret);
		return ret;
	}

	/* Program sub-second increment register */
	syn_ptp_increment_set(mac_base, ptp_priv->ssinc);

	/* Step 4: Initialize addend register for fine frequency adjustment
	 * The addend value controls the rate at which the timestamp counter increments.
	 * Using the calculated default addend ensures accurate timekeeping.
	 */
	ret = syn_ptp_adjfine(mac_base, ptp_priv->default_addend);
	if (ret) {
		dev_err(ptp_priv->dev, "Failed to set default addend: %d\n", ret);
		return ret;
	}

	/* Step 5 & 6: Initialize timestamp counter to 0 */
	ret = syn_ptp_time_set(mac_base, SYN_MAC_TS_CTL_TSINIT, 0, 0);

	/* Step 6b: Initialize asymmetry correction registers to 0 */
	hal_write_reg(mac_base, SYN_MAC_TS_INGRESS_ASYM_CORR, 0);
	hal_write_reg(mac_base, SYN_MAC_TS_EGRESS_ASYM_CORR, 0);

	/* Step 7: Configure PPS0 output frequency to 1 Hz (digital rollover)
	 * PPSCTRL0 (bits 3:0) = 0x1: binary rollover 2 Hz, digital rollover 1 Hz.
	 * This configures the ptp_pps_o[0] output signal on the MAC_PPS_Control
	 * register (offset 0xD70).
	 */
	syn_ptp_pps_ctrl_set(mac_base, SYN_MAC_PPS_CTL_PPSCTRL0_1HZ);

	dev_info(ptp_priv->dev,
		 "Sub-second increment: %u ns, Addend: 0x%08x, Clock rate: %u Hz\n",
		 ptp_priv->ssinc, ptp_priv->default_addend, ptp_priv->ptp_clock_rate);

	return ret;
}

/*
 * syn_ptp_init()
 *	Initialize PTP Hardware Clock support
 *
 * This function allocates and initializes the PTP private data structure,
 * sets up the PHC capabilities, and registers the PHC device with the
 * Linux PTP subsystem.
 *
 * @shd: Pointer to syn_hal_dev structure
 * @pdev: Pointer to platform device structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_init(struct syn_hal_dev *shd, struct platform_device *pdev)
{
	struct syn_ptp_priv *ptp_priv;
	struct device *dev = &pdev->dev;
	struct clk *ptp_clk;
	int ret;

	/* Check PTP reference clock rate available or not. */
	ptp_clk = devm_clk_get_optional_enabled(dev, "ptp");
	if (IS_ERR(ptp_clk)) {
		dev_err(dev, "Failed to get PTP clock\n");
		return PTR_ERR(ptp_clk);
	}

	/* PTP is optional to be enabled. */
	if (!ptp_clk) {
		dev_dbg(dev, "PTP is not enabled\n");
		return 0;
	}

	/* Allocate PTP private data structure */
	ptp_priv = devm_kzalloc(dev, sizeof(*ptp_priv), GFP_KERNEL);
	if (!ptp_priv) {
		dev_err(dev, "Failed to allocate PTP private data\n");
		return -ENOMEM;
	}

	/* Get the PTP reference clock rate. */
	ptp_priv->ptp_clock_rate = clk_get_rate(ptp_clk);
	if (!ptp_priv->ptp_clock_rate) {
		dev_err(dev, "PTP clock rate is 0\n");
		return -EINVAL;
	}

	/* Store back pointer to HAL device */
	ptp_priv->shd = shd;

	/* Store platform device pointer */
	ptp_priv->dev = dev;

	/* Initialize spinlock for fast IRQ-context paths (gettimex, IRQ handler, pps_enabled) */
	spin_lock_init(&ptp_priv->lock);

	/* Initialize mutex for SYN_MAC_TS_CTL register operations (adjfine/adjtime/settime) */
	mutex_init(&ptp_priv->ts_ctl_mutex);

	/* Initialize TX timestamp queue spinlock */
	spin_lock_init(&ptp_priv->tx_ts_lock);

	/* Get reference to singleton platform manager */
	ptp_priv->platform_mgr = syn_ptp_platform_mgr_get(pdev);
	if (!ptp_priv->platform_mgr) {
		dev_err(dev, "Failed to get platform manager\n");
		return -ENOMEM;
	}

	/* Initialize PPS enabled flag */
	ptp_priv->pps_enabled = false;

	/*
	 * Initialize the PPS list node so list_del_init() is always safe
	 * to call in syn_ptp_cleanup() even if PPS was never enabled.
	 */
	INIT_LIST_HEAD(&ptp_priv->pps_list_node);

	/* Initialize PTP clock capabilities */
	ptp_priv->caps.owner = THIS_MODULE;
	snprintf(ptp_priv->caps.name, sizeof(ptp_priv->caps.name),
		 "xgmac-ptp-%d", shd->nghd.mac_id);
	ptp_priv->caps.max_adj = 500000;	/* Maximum frequency adjustment: ±500,000 ppb (±500 ppm) */
	ptp_priv->caps.n_alarm = 0;		/* No alarm support */
	ptp_priv->caps.n_ext_ts = 1;		/* One external timestamp channel */
	ptp_priv->caps.n_per_out = 1;		/* One periodic output channel */
	ptp_priv->caps.n_pins = 1;		/* One configurable pin */
	ptp_priv->caps.pps = 1;			/* PPS support enabled */

	/* Set callback function pointers (stub implementations for now) */
	ptp_priv->caps.adjfine = qcom_nss_ptp_adjfine;
	ptp_priv->caps.adjtime = qcom_nss_ptp_adjtime;
	ptp_priv->caps.gettimex64 = qcom_nss_ptp_gettimex;
	ptp_priv->caps.settime64 = qcom_nss_ptp_settime;
	ptp_priv->caps.enable = qcom_nss_ptp_enable;
	ptp_priv->caps.verify = qcom_nss_ptp_verify;
	snprintf(ptp_priv->pin.name, sizeof(ptp_priv->pin.name), "PPS_SYNC");
	ptp_priv->caps.pin_config = &ptp_priv->pin;

	/* Initialize PTP hardware following datasheet guidelines */
	ret = syn_ptp_hw_init(ptp_priv);
	if (ret) {
		dev_err(dev, "PTP hardware initialization failed: %d\n", ret);
		goto err_free_priv;
	}

	/* Register PHC device with PTP subsystem */
	ptp_priv->clock = ptp_clock_register(&ptp_priv->caps, dev);
	if (IS_ERR(ptp_priv->clock)) {
		ret = PTR_ERR(ptp_priv->clock);
		dev_err(dev, "Failed to register PHC device: %d\n", ret);
		goto err_free_priv;
	}

	dev_info(dev, "PHC device registered successfully (index=%d)\n",
		 ptp_clock_index(ptp_priv->clock));

	/* Store PTP private data pointer in HAL device */
	shd->ptp_priv = ptp_priv;

	/* Initialize debugfs interface for PTP statistics */
	ret = syn_ptp_debugfs_init(ptp_priv);
	if (ret) {
		dev_warn(dev, "Failed to initialize PTP debugfs: %d\n", ret);
		/* Continue without debugfs support */
	}

	/*
	 * Register the PPS_IN IRQ once for the entire platform (not per-instance).
	 *
	 * The PPS_IN signal is a single hardware wire shared by all XGMAC
	 * instances. Registering it per-instance would cause the second
	 * instance to fail with -EBUSY ("Flags mismatch") because the IRQ
	 * is already owned without IRQF_SHARED.
	 *
	 * The IRQ is registered with the platform manager as private data.
	 * The handler iterates platform_mgr->pps_active_list and delivers
	 * an independent PPS event to each XGMAC instance that has enabled
	 * PPS capture via PTP_CLK_REQ_EXTTS (qcom_nss_ptp_enable()).
	 *
	 * The IRQ is freed in syn_ptp_platform_mgr_put() when the last
	 * XGMAC instance releases its reference.
	 */
	mutex_lock(&platform_mgr_mutex);
	if (ptp_priv->platform_mgr->pps_in_irq >= 0 &&
	    !ptp_priv->platform_mgr->pps_in_irq_registered) {
		ret = request_threaded_irq(ptp_priv->platform_mgr->pps_in_irq,
					   NULL,
					   qcom_nss_ptp_irq_handler_thread,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   "nss-pps-in",
					   ptp_priv->platform_mgr);
		if (ret) {
			dev_err(dev, "Failed to register PPS_IN IRQ %d: %d\n",
				ptp_priv->platform_mgr->pps_in_irq, ret);
			/* Non-fatal: PHC still works, only PPS capture is unavailable */
		} else {
			ptp_priv->platform_mgr->pps_in_irq_registered = true;
			dev_info(dev, "Registered PPS_IN interrupt (IRQ=%d)\n",
				 ptp_priv->platform_mgr->pps_in_irq);
		}
		ret = 0;  /* Reset ret: IRQ failure is non-fatal */
	} else if (ptp_priv->platform_mgr->pps_in_irq >= 0) {
		dev_dbg(dev, "PPS_IN IRQ %d already registered by another instance\n",
			ptp_priv->platform_mgr->pps_in_irq);
	}
	mutex_unlock(&platform_mgr_mutex);

	return 0;

err_free_priv:
	syn_ptp_platform_mgr_put();
	shd->ptp_priv = NULL;
	return ret;
}
EXPORT_SYMBOL(syn_ptp_init);

/*
 * syn_ptp_get_ts_info()
 *	Get PTP timestamping information
 *
 * This function populates the ethtool_ts_info structure with PTP
 * timestamping capabilities supported by the hardware.
 *
 * @shd: Pointer to syn_hal_dev structure
 * @info: Pointer to ethtool_ts_info structure to populate
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_get_ts_info(struct syn_hal_dev *shd, struct ethtool_ts_info *info)
{
	struct syn_ptp_priv *ptp_priv;

	/* Retrieve ptp_priv from shd structure */
	ptp_priv = shd->ptp_priv;

	/* Check if PTP was initialized */
	if (!ptp_priv || !ptp_priv->clock) {
		/* PTP not available, return software timestamping only */
		info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
					SOF_TIMESTAMPING_RX_SOFTWARE |
					SOF_TIMESTAMPING_SOFTWARE;
		info->phc_index = -1;
		info->tx_types = (1 << HWTSTAMP_TX_OFF);
		info->rx_filters = (1 << HWTSTAMP_FILTER_NONE);
		return 0;
	}

	/* Populate timestamping capabilities */
	info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
				SOF_TIMESTAMPING_RX_SOFTWARE |
				SOF_TIMESTAMPING_SOFTWARE |
				SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;

	/* Set PHC index */
	info->phc_index = ptp_clock_index(ptp_priv->clock);

	/* Supported TX timestamp types */
	info->tx_types = (1 << HWTSTAMP_TX_OFF) |
			 (1 << HWTSTAMP_TX_ON) |
			 (1 << HWTSTAMP_TX_ONESTEP_SYNC);

	/* Supported RX filter types */
	info->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
			   (1 << HWTSTAMP_FILTER_ALL) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L2_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L2_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_EVENT);

	return 0;
}
EXPORT_SYMBOL(syn_ptp_get_ts_info);

/*
 * syn_ptp_hwtstamp_set()
 *	Configure hardware timestamping
 *
 * This function configures the XGMAC hardware for packet timestamping
 * based on the requested configuration from userspace (via SIOCSHWTSTAMP ioctl).
 *
 * @shd: Pointer to syn_hal_dev structure
 * @ifr: Pointer to ifreq structure containing hwtstamp_config
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_hwtstamp_set(void *hal_ctx, struct ifreq *ifr)
{
	struct syn_hal_dev *shd = (struct syn_hal_dev *)hal_ctx;
	struct syn_ptp_priv *ptp_priv = shd->ptp_priv;
	void __iomem *mac_base = shd->nghd.mac_base;
	struct hwtstamp_config config;
	unsigned long flags;
	u32 ts_ctl = 0;

	/* Check if PTP is initialized */
	if (!ptp_priv) {
		return -ENODEV;
	}

	/* Copy configuration from userspace */
	if (copy_from_user(&config, ifr->ifr_data, sizeof(config))) {
		return -EFAULT;
	}

	/* Validate TX timestamp mode */
	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
	case HWTSTAMP_TX_ONESTEP_SYNC:
		break;
	default:
		return -ERANGE;
	}

	/* Validate and normalize RX filter */
	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		break;

	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		/* PTP v2 over L4 (UDP) - upgrade to event messages */
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
		/* PTP v2 over L2 (Ethernet) - upgrade to event messages */
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
		break;

	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		/* PTP v2 over both L2 and L4 - upgrade to event messages */
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		break;

	case HWTSTAMP_FILTER_ALL:
		/* Timestamp all packets */
		break;

	default:
		return -ERANGE;
	}

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Read current timestamp control register */
	ts_ctl = hal_read_reg(mac_base, SYN_MAC_TS_CTL);

	/* Clear configuration bits using mask (upstream approach) */
	ts_ctl &= ~SYN_MAC_HWTS_CFG_MASK;

	/* Build new configuration */
	u32 new_config = 0;

	/* Configure TX timestamping */
	if (config.tx_type != HWTSTAMP_TX_OFF) {
		new_config |= SYN_MAC_TS_CTL_TSENA;

		/* Enable one-step sync if requested */
		if (config.tx_type == HWTSTAMP_TX_ONESTEP_SYNC) {
			new_config |= SYN_MAC_TS_CTL_CSC;  /* Checksum correction */
		}
	}

	if (config.rx_filter != HWTSTAMP_FILTER_NONE)
		new_config |= SYN_MAC_TS_CTL_TSENA;

	/* Configure RX filtering based on config.rx_filter */
	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		/* No RX timestamping */
		break;

	case HWTSTAMP_FILTER_ALL:
		/* Timestamp all packets */
		new_config |= SYN_MAC_TS_CTL_TSENALL;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
		/* PTP v2 over UDP (IPv4/IPv6), event messages only */
		new_config |= SYN_MAC_TS_CTL_TSVER2ENA |
			      SYN_MAC_TS_CTL_TSIPV4ENA |
			      SYN_MAC_TS_CTL_TSIPV6ENA |
			      SYN_MAC_TS_CTL_TSEVENTENA;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
		/* PTP v2 over Ethernet (L2), event messages only */
		new_config |= SYN_MAC_TS_CTL_TSVER2ENA |
			      SYN_MAC_TS_CTL_TSIPENA |
			      SYN_MAC_TS_CTL_TSEVENTENA;
		break;

	case HWTSTAMP_FILTER_PTP_V2_EVENT:
		/* PTP v2 over both L2 and L4, event messages only */
		new_config |= SYN_MAC_TS_CTL_TSVER2ENA |
			      SYN_MAC_TS_CTL_TSIPENA |
			      SYN_MAC_TS_CTL_TSIPV4ENA |
			      SYN_MAC_TS_CTL_TSIPV6ENA |
			      SYN_MAC_TS_CTL_TSEVENTENA;
		break;

	default:
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		return -ERANGE;
	}

	/* Apply new configuration (upstream mask-based approach) */
	ts_ctl |= new_config;
	hal_write_reg(mac_base, SYN_MAC_TS_CTL, ts_ctl);

	/* Store configuration */
	memcpy(&ptp_priv->tstamp_config, &config, sizeof(config));

	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Copy configuration back to userspace */
	if (copy_to_user(ifr->ifr_data, &config, sizeof(config))) {
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(syn_ptp_hwtstamp_set);

/*
 * syn_ptp_hwtstamp_get()
 *	Get current hardware timestamping configuration
 *
 * This function retrieves the current hardware timestamping configuration
 * (via SIOCGHWTSTAMP ioctl).
 *
 * @shd: Pointer to syn_hal_dev structure
 * @ifr: Pointer to ifreq structure to receive hwtstamp_config
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_hwtstamp_get(void *hal_ctx, struct ifreq *ifr)
{
	struct syn_hal_dev *shd = (struct syn_hal_dev *)hal_ctx;
	struct syn_ptp_priv *ptp_priv = shd->ptp_priv;
	struct hwtstamp_config config;
	unsigned long flags;

	/* Check if PTP is initialized */
	if (!ptp_priv) {
		return -ENODEV;
	}

	spin_lock_irqsave(&ptp_priv->lock, flags);
	memcpy(&config, &ptp_priv->tstamp_config, sizeof(config));
	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Copy configuration to userspace */
	if (copy_to_user(ifr->ifr_data, &config, sizeof(config))) {
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(syn_ptp_hwtstamp_get);

/*
 * syn_ptp_drain_tx_fifo()
 *	Drain all available TX timestamps from hardware FIFO
 *
 * This function reads all available timestamps from the 8-deep hardware FIFO
 * and stores them in the software queue indexed by packet ID. It uses the
 * TTSNS field (bits 14:10) in register 0xd20 to determine how many timestamps
 * are available.
 *
 * Called from TX completion handler to ensure timestamps don't get lost.
 *
 * @ptp_priv: Pointer to PTP private data structure
 */
static void syn_ptp_drain_tx_fifo(struct syn_ptp_priv *ptp_priv)
{
	void __iomem *mac_base = ptp_priv->shd->nghd.mac_base;
	unsigned long flags;
	u32 status, num_ts;
	int i, slot;
	struct {
		u32 pkt_id;
		u32 sec;
		u32 nsec;
		u64 timestamp_ns;
		bool valid;
	} temp_ts[SYN_MAC_PTP_TXTS_FIFO_NUM];
	int valid_count = 0;

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Read number of timestamps available in FIFO (bits 14:10) */
	status = hal_read_reg(mac_base, SYN_MAC_TS_STATUS);
	num_ts = (status & SYN_MAC_TS_STATUS_TTSNS_MASK) >> SYN_MAC_TS_STATUS_TTSNS_SHIFT;

	if (num_ts == 0) {
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		return;
	}

	/* Check for FIFO full condition */
	if (num_ts == SYN_MAC_PTP_TXTS_FIFO_NUM) {
		ptp_priv->tx_ts_fifo_full_count++;
		if (net_ratelimit()) {
			dev_warn(ptp_priv->dev,
				 "TX timestamp FIFO full (%u entries), possible timestamp loss\n",
				 num_ts);
		}
	}

	/* Read all timestamps from hardware FIFO into temporary buffer */
	for (i = 0; i < num_ts && valid_count < SYN_MAC_PTP_TXTS_FIFO_NUM; i++) {
		u32 nsec, sec, pkt_id;

		pkt_id = hal_read_reg(mac_base, SYN_MAC_TX_TS_STATUS_PKTID) & 0x3FF;
		sec = hal_read_reg(mac_base, SYN_MAC_TX_TS_STATUS_SECS);
		nsec = hal_read_reg(mac_base, SYN_MAC_TX_TS_STATUS_NSECS);

		/* Check for missed timestamp flag (bit 31) */
		if (nsec & SYN_MAC_TX_TS_STATUS_TXTSSMIS) {
			if (net_ratelimit()) {
				dev_warn(ptp_priv->dev,
					 "TX timestamp missed for pkt_id=%u\n", pkt_id);
			}
			continue;
		}

		/* Store in temporary buffer */
		temp_ts[valid_count].pkt_id = pkt_id;
		temp_ts[valid_count].sec = sec;
		temp_ts[valid_count].nsec = nsec & SYN_MAC_TX_TS_STATUS_NSECS_MASK;
		temp_ts[valid_count].timestamp_ns = ((u64)sec * NSEC_PER_SEC) + temp_ts[valid_count].nsec;
		temp_ts[valid_count].valid = true;
		valid_count++;
	}

	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Now update the queue with tx_ts_lock only */
	spin_lock_irqsave(&ptp_priv->tx_ts_lock, flags);
	for (i = 0; i < valid_count; i++) {
		slot = temp_ts[i].pkt_id % SYN_MAC_PTP_TXTS_FIFO_NUM;

		if (ptp_priv->tx_ts_queue[slot].valid == true) {
			int j;
			/* Try to find an unused slot */
			for (j = 0; j < SYN_MAC_PTP_TXTS_FIFO_NUM; j++) {
				if (!ptp_priv->tx_ts_queue[j].valid) {
					slot = j;
					break;
				}
			}

			if (ptp_priv->tx_ts_queue[slot].valid == true) {
				ptp_priv->tx_ts_slot_conflict_count++;
				dev_warn(ptp_priv->dev,
					 "TX timestamp slot conflict: slot=%d, old_pkt_id=%u, new_pkt_id=%u (conflicts=%u)\n",
					 slot, ptp_priv->tx_ts_queue[slot].pkt_id, temp_ts[i].pkt_id,
					 ptp_priv->tx_ts_slot_conflict_count);
			}
		}

		ptp_priv->tx_ts_queue[slot].timestamp_ns = temp_ts[i].timestamp_ns;
		ptp_priv->tx_ts_queue[slot].pkt_id = temp_ts[i].pkt_id;
		ptp_priv->tx_ts_queue[slot].capture_time = ktime_get();
		ptp_priv->tx_ts_queue[slot].valid = true;
	}
	spin_unlock_irqrestore(&ptp_priv->tx_ts_lock, flags);
}

/*
 * syn_ptp_get_tx_hwtstamp_by_id()
 *	Retrieve TX timestamp by packet ID
 *
 * This function looks up a TX timestamp in the software queue by packet ID.
 * It first drains any pending timestamps from the hardware FIFO, then
 * searches for a matching packet ID. Stale timestamps (older than 100ms)
 * are automatically discarded.
 *
 * @shd: Pointer to syn_hal_dev structure
 * @pkt_id: 10-bit packet ID (1-1023) to match
 * @shhwtstamps: Pointer to store the retrieved timestamp
 *
 * Returns: 0 on success, -ENODATA if timestamp not found or stale
 */
int syn_ptp_get_tx_hwtstamp_by_id(struct syn_hal_dev *shd,
				   u16 pkt_id,
				   struct skb_shared_hwtstamps *shhwtstamps)
{
	struct syn_ptp_priv *ptp_priv = shd->ptp_priv;
	unsigned long flags;
	int slot = pkt_id % SYN_MAC_PTP_TXTS_FIFO_NUM;
	ktime_t now;
	s64 age_ms;
	bool found = false;
	int i;

	if (!ptp_priv || ptp_priv->tstamp_config.tx_type == HWTSTAMP_TX_OFF)
		return -ENODEV;

	/* First, drain any pending timestamps from hardware FIFO */
	syn_ptp_drain_tx_fifo(ptp_priv);

	spin_lock_irqsave(&ptp_priv->tx_ts_lock, flags);

	/* Check if we have a valid timestamp for this packet ID */
	if (!ptp_priv->tx_ts_queue[slot].valid ||
	    ptp_priv->tx_ts_queue[slot].pkt_id != pkt_id) {
		/* Not in preferred slot, search the entire queue */
		for (i = 0; i < SYN_MAC_PTP_TXTS_FIFO_NUM; i++) {
			if (ptp_priv->tx_ts_queue[i].valid &&
			    ptp_priv->tx_ts_queue[i].pkt_id == pkt_id) {
				slot = i;
				break;
			}
		}
	}

	if (ptp_priv->tx_ts_queue[slot].valid &&
	    ptp_priv->tx_ts_queue[slot].pkt_id == pkt_id) {

		/* Check if timestamp is stale (older than 100ms) */
		now = ktime_get();
		age_ms = ktime_ms_delta(now, ptp_priv->tx_ts_queue[slot].capture_time);

		if (age_ms < 100) {
			/* Timestamp is fresh - use it */
			memset(shhwtstamps, 0, sizeof(*shhwtstamps));
			shhwtstamps->hwtstamp = ns_to_ktime(ptp_priv->tx_ts_queue[slot].timestamp_ns);
			ptp_priv->tx_ts_queue[slot].valid = false;  /* Mark as consumed */
			ptp_priv->tx_ts_match_success++;
			found = true;
		} else {
			/* Timestamp is stale - discard it */
			ptp_priv->tx_ts_stale_count++;
			ptp_priv->tx_ts_queue[slot].valid = false;
			if (net_ratelimit()) {
				dev_warn(ptp_priv->dev,
					"Stale TX timestamp for pkt_id=%u (age=%lld ms)\n",
					pkt_id, age_ms);
			}
		}
	}

	spin_unlock_irqrestore(&ptp_priv->tx_ts_lock, flags);

	if (!found) {
		ptp_priv->tx_ts_match_fail++;
		return -ENODATA;
	}

	return 0;
}
EXPORT_SYMBOL(syn_ptp_get_tx_hwtstamp_by_id);

/*
 * syn_ptp_cleanup()
 *	Cleanup PTP Hardware Clock support
 *
 * This function unregisters the PHC device and frees all allocated resources.
 * It is called during driver unload or when XGMAC interface is brought down.
 *
 * @shd: Pointer to syn_hal_dev structure
 */
void syn_ptp_cleanup(struct syn_hal_dev *shd)
{
	struct syn_ptp_priv *ptp_priv;

	/* Retrieve ptp_priv from shd structure */
	ptp_priv = shd->ptp_priv;

	/* Check if PTP was initialized */
	if (!ptp_priv) {
		return;
	}

	dev_info(ptp_priv->dev, "Cleaning up PTP support\n");

	/* Cleanup debugfs interface */
	syn_ptp_debugfs_exit(ptp_priv);

	/*
	 * Remove this instance from the platform-level PPS active list.
	 * list_del_init() is safe even if the node is not currently in any
	 * list (e.g., PPS was never enabled for this instance).
	 * The PPS_IN IRQ itself is freed in syn_ptp_platform_mgr_put() when
	 * the last XGMAC instance releases its reference — not here.
	 */
	spin_lock_irq(&ptp_priv->platform_mgr->pps_list_lock);
	list_del_init(&ptp_priv->pps_list_node);
	spin_unlock_irq(&ptp_priv->platform_mgr->pps_list_lock);

	/* Unregister PHC device */
	if (ptp_priv->clock) {
		ptp_clock_unregister(ptp_priv->clock);
		dev_info(ptp_priv->dev, "PHC device unregistered\n");
	}

	/* Release reference to platform manager (handles platform cleanup on last instance) */
	syn_ptp_platform_mgr_put();

	/* Clear pointer in HAL device structure */
	shd->ptp_priv = NULL;
}
EXPORT_SYMBOL(syn_ptp_cleanup);

MODULE_DESCRIPTION("Synopsys XGMAC PTP Hardware Clock support");
MODULE_LICENSE("Dual BSD/GPL");
