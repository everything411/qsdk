/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_SPECTRAL_H
#define ATH12K_SPECTRAL_H

#include <linux/hrtimer.h>
#include <linux/workqueue.h>

#include "../spectral_common.h"
#include "dbring.h"
#include "vendor.h"
#include "qca-vendor.h"

#define ATH12K_SPECTRAL_NUM_DETECTORS		2
#define ATH12K_SPECTRAL_DETECTOR_NORMAL		0
#define ATH12K_SPECTRAL_DETECTOR_PHYA1_NORMAL	1
#define ATH12K_SPECTRAL_DETECTOR_AGILE		1

#define ATH12K_SPECTRAL_SCAN_COUNT_MAX_DEFAULT	512
#define ATH12K_SPECTRAL_SCAN_COUNT_MAX_HW_LIMIT	4095
#define ATH12K_SPECTRAL_ATH12K_MIN_BINS		32
#define ATH12K_SPECTRAL_RPT_MODE_0		0
#define ATH12K_SPECTRAL_RPT_MODE_1		1
#define ATH12K_SPECTRAL_RPT_MODE_2		2
#define ATH12K_SPECTRAL_RPT_MODE_3		3
#define ATH12K_SPECTRAL_RPT_MODE_MAX		3

#define ATH12K_SPECTRAL_FFT_SIZE_MIN			5
#define ATH12K_SPECTRAL_FFT_SIZE_MAX_20MHZ		9
#define ATH12K_SPECTRAL_FFT_SIZE_MAX_40MHZ		10


struct ath12k_spectral_param_min_max {
	u16 fft_size_min;
	u16 fft_size_max[ATH12K_SPECTRAL_NUM_BW_SLOTS];
	u16 scan_count_max;
};

/**
 * struct ath12k_spectral_params - parameters passed to WMI spectral scan config
 *
 * @scan_count:          number of FFT samples to capture; 0 = unlimited
 * @scan_period:         time between consecutive scan triggers (in TU)
 * @scan_priority:       scan priority relative to other HW operations
 * @scan_fft_size:       FFT size as log2 (e.g. 8 => 256 bins); validated
 *                       against per-chip capability at scan start
 * @scan_gc_ena:         enable gain change between scans
 * @scan_restart_ena:    auto-restart scan after each sweep
 * @scan_noise_floor_ref: reference noise floor in dBm
 * @scan_init_delay:     delay in OFDM symbols before the first scan
 * @scan_nb_tone_thr:    narrowband tone-detection threshold
 * @scan_str_bin_thr:    strong-bin count threshold used for reporting
 * @scan_wb_rpt_mode:    wideband report mode (0 = no report, 1 = summary)
 * @scan_rssi_rpt_mode:  RSSI-based report gating (0 = always, 1 = only above thr)
 * @scan_rssi_thr:       RSSI threshold for conditional reporting
 * @scan_pwr_format:     power format: 0 = linear magnitude, 1 = dBm
 * @scan_rpt_mode:       controls which FFT bins are included in the report
 * @scan_bin_scale:      scaling factor applied to FFT bins before reporting
 * @scan_dbm_adj:        dBm adjustment offset applied to reported power values
 * @scan_chn_mask:       bitmask of ADC chains to include in the scan
 * @fft_period:          interval between consecutive FFTs within a sweep
 * @short_report:        if non-zero, only the peak-magnitude bin is reported
 * @frequency:           primary channel center frequency (MHz)
 * @frequency2:          secondary channel center frequency (MHz); used in 80+80 mode
 * @bandwidth:           channel bandwidth
 * @fft_recapture:       allow HW to recapture FFT if the initial capture is invalid
 * @completion_timeout_us: maximum time to wait for scan completion (microseconds)
 */
struct ath12k_spectral_params {
	u32 scan_count;
	u32 scan_period;
	u32 scan_priority;
	u32 scan_fft_size;
	u32 scan_gc_ena;
	u32 scan_restart_ena;
	u32 scan_noise_floor_ref;
	u32 scan_init_delay;
	u32 scan_nb_tone_thr;
	u32 scan_str_bin_thr;
	u32 scan_wb_rpt_mode;
	u32 scan_rssi_rpt_mode;
	u32 scan_rssi_thr;
	u32 scan_pwr_format;
	u32 scan_rpt_mode;
	u32 scan_bin_scale;
	u32 scan_dbm_adj;
	u32 scan_chn_mask;
	u32 fft_period;
	u32 short_report;
	u32 frequency;
	u32 frequency2;
	u8  bandwidth;
	u8  fft_recapture;
	u32 completion_timeout_us;
};

struct ath12k_spectral_diag_stats {
	u64 sig_mismatch;
	u64 sec80_sfft_insufflen;
	u64 no_sec80_sfft;
	u64 vhtseg1id_mismatch;
	u64 vhtseg2id_mismatch;
};

struct ath12k_spectral {
	struct ath12k_dbring rx_ring;
	/* Protects enabled, mode, and scan_active */
	spinlock_t lock;
	struct rchan *rfs_scan;	/* relay(fs) channel for spectral scan */
	enum spectral_scan_mode mode;
	struct ath12k_spectral_params     params;
	struct ath12k_spectral_diag_stats diag;
	bool dbr_buff_debug;
	u32 prev_tstamp[ATH12K_SPECTRAL_NUM_DETECTORS];
	bool enabled;
	bool is_primary;
	bool scan_active;
	u32 ch_width;
	struct wmi_spectral_capabilities_event spectral_cap;
	u32 samples_done;
	u32 sub_buf_size;
	u32 num_sub_bufs;
	/* Runtime configurable max scan count from INI */
	u32 scan_count_max;
	/* fields populated from WMI_PDEV_SSCAN_FW_PARAM_EVENTID */
	u32 pri20_freq;
	/* center frequency of the home channel; stays fixed during agile scans */
	u32 oper_cfreq1;
	u32 sscan_cfreq1;
	u32 sscan_cfreq2;
	u8  sscan_bw;
	u32 start_freq;
	u32 end_freq;
	/* timestamp WAR state */
	u32 last_fft_timestamp[SPECTRAL_SCAN_MODE_MAX]; /* last raw FFT timestamp */
	u32 timestamp_war_offset[SPECTRAL_SCAN_MODE_MAX]; /* accumulated WAR offset */
	u32 target_reset_count;   /* number of target resets seen */
	/* one-shot hrtimer that fires when the FW fails to deliver scan_count
	 * FFT reports within params.completion_timeout_us.
	 */
	struct hrtimer scan_completion_timer;
	/* received-count snapshot saved by the hrtimer cb for the worker */
	u32 timeout_received_count;
	/* vdev_id cached at scan-start (under ->lock); used by timeout_work
	 * to send CLEAR+DISABLE without needing wiphy_lock - calling
	 * wiphy_lock() from the workqueue deadlocks when wiphy->mtx is
	 * already held by the nl80211 vendor-command dispatch path.
	 */
	u32 active_vdev_id;
	/* runs ath12k_spectral_send_complete_event from process context */
	struct work_struct scan_timeout_work;
	/* per-BW fft_size caps + scalar mins/maxs, populated at probe */
	struct ath12k_spectral_param_min_max param_min_max;
};

#ifdef CPTCFG_ATH12K_SPECTRAL

struct ath12k_link_vif *ath12k_spectral_get_vdev(struct ath12k *ar);
int ath12k_spectral_nl80211_bw_to_idx(enum nl80211_chan_width bw);
/**
 * ath12k_spectral_is_agile_capable() - check whether agile spectral is supported
 * @ar: ath12k radio instance
 *
 * Return: true if any agile spectral BW tier is supported. Use
 * ath12k_spectral_is_agile_bw_capable() for per-BW validation.
 */
bool ath12k_spectral_is_agile_capable(struct ath12k *ar);
/**
 * ath12k_spectral_is_agile_bw_capable() - check agile spectral BW support
 * @ar: ath12k radio instance
 * @width: nl80211 channel width to validate
 *
 * Return: true if agile spectral scan is supported for @width.
 */
bool ath12k_spectral_is_agile_bw_capable(struct ath12k *ar,
					 enum nl80211_chan_width width);

/**
 * ath12k_spectral_rollback_agile() - clear cached agile spectral state
 * @ar: ath12k radio instance
 *
 * Clears mode, scan_active, and cached agile frequency and bandwidth
 * parameters. Called from all agile rollback paths after a configure,
 * start, or scan failure.
 */
void ath12k_spectral_rollback_agile(struct ath12k *ar);

int ath12k_spectral_init(struct ath12k_base *ab);
void ath12k_spectral_deinit(struct ath12k_base *ab);
int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif);
void ath12k_spectral_reset_buffer(struct ath12k *ar);
struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar);
int ath12k_spectral_configure_scan_params(struct ath12k *ar,
						  enum spectral_scan_mode mode);
int ath12k_spectral_stop_scan(struct ath12k *ar);
int ath12k_spectral_start_scan(struct ath12k *ar);
int ath12k_spectral_send_complete_event(struct ath12k *ar,
					int status,
					u32 received_samples);

#else

static inline int ath12k_spectral_init(struct ath12k_base *ab)
{
	return 0;
}

static inline void ath12k_spectral_deinit(struct ath12k_base *ab)
{
}

static inline int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif)
{
	return 0;
}

static inline void ath12k_spectral_reset_buffer(struct ath12k *ar)
{
}

static inline
struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar)
{
	return NULL;
}

static inline int ath12k_spectral_configure_scan_params(struct ath12k *ar,
							enum spectral_scan_mode mode)
{
	return 0;
}

static inline int ath12k_spectral_stop_scan(struct ath12k *ar)
{
	return 0;
}

static inline int ath12k_spectral_start_scan(struct ath12k *ar)
{
	return 0;
}

static inline void
ath12k_spectral_rollback_agile(struct ath12k *ar)
{
}

static inline bool ath12k_spectral_is_agile_capable(struct ath12k *ar)
{
	return false;
}

static inline bool ath12k_spectral_is_agile_bw_capable(struct ath12k *ar,
						       enum nl80211_chan_width width)
{
	return false;
}

#endif /* CPTCFG_ATH12K_SPECTRAL */
#endif /* ATH12K_SPECTRAL_H */
