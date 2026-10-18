/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
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

#ifndef SPECTRAL_COMMON_H
#define SPECTRAL_COMMON_H

#define SPECTRAL_HT20_NUM_BINS		56
#define SPECTRAL_HT20_40_NUM_BINS		128

/* TODO: could possibly be 512, but no samples this large
 * could be acquired so far.
 */
#define SPECTRAL_ATH10K_MAX_NUM_BINS		256
#define SPECTRAL_ATH12K_MAX_NUM_BINS        512

/*
 * Please keep the type/length at the front position and change
 * other fields after adding another sample type
 */
enum ath_fft_sample_type {
	ATH_FFT_SAMPLE_HT20 = 1,
	ATH_FFT_SAMPLE_HT20_40,
	ATH_FFT_SAMPLE_ATH10K,
	ATH_FFT_SAMPLE_ATH11K,
	ATH_FFT_SAMPLE_ATH12K
};

struct fft_sample_tlv {
	u8 type;	/* see ath_fft_sample */
	__be16 length;
	/* type dependent data follows */
} __packed;

struct fft_sample_ht20 {
	struct fft_sample_tlv tlv;

	u8 max_exp;

	__be16 freq;
	s8 rssi;
	s8 noise;

	__be16 max_magnitude;
	u8 max_index;
	u8 bitmap_weight;

	__be64 tsf;

	u8 data[SPECTRAL_HT20_NUM_BINS];
} __packed;

struct fft_sample_ht20_40 {
	struct fft_sample_tlv tlv;

	u8 channel_type;
	__be16 freq;

	s8 lower_rssi;
	s8 upper_rssi;

	__be64 tsf;

	s8 lower_noise;
	s8 upper_noise;

	__be16 lower_max_magnitude;
	__be16 upper_max_magnitude;

	u8 lower_max_index;
	u8 upper_max_index;

	u8 lower_bitmap_weight;
	u8 upper_bitmap_weight;

	u8 max_exp;

	u8 data[SPECTRAL_HT20_40_NUM_BINS];
} __packed;

struct fft_sample_ath10k {
	struct fft_sample_tlv tlv;
	u8 chan_width_mhz;
	__be16 freq1;
	__be16 freq2;
	__be16 noise;
	__be16 max_magnitude;
	__be16 total_gain_db;
	__be16 base_pwr_db;
	__be64 tsf;
	s8 max_index;
	u8 rssi;
	u8 relpwr_db;
	u8 avgpwr_db;
	u8 max_exp;

	u8 data[];
} __packed;

struct fft_sample_ath11k {
	struct fft_sample_tlv tlv;
	u8 chan_width_mhz;
	s8 max_index;
	u8 max_exp;
	bool is_primary;
	__be16 freq1;
	__be16 freq2;
	__be16 max_magnitude;
	__be16 rssi;
	__be32 tsf;
	__be32 noise;

	u8 data[];
} __packed;

#define SPECTRAL_VERSION     (1)
#define SPECTRAL_SUB_VERSION (0)

/*
 * ioctl parameter types
 */
enum spectral_params {
	SPECTRAL_PARAM_FFT_PERIOD,
	SPECTRAL_PARAM_SCAN_PERIOD,
	SPECTRAL_PARAM_FFT_RECAPTURE,
	SPECTRAL_PARAM_SCAN_COUNT,
	SPECTRAL_PARAM_SHORT_REPORT,
	SPECTRAL_PARAM_SPECT_PRI,
	SPECTRAL_PARAM_FFT_SIZE,
	SPECTRAL_PARAM_GC_ENA,
	SPECTRAL_PARAM_RESTART_ENA,
	SPECTRAL_PARAM_NOISE_FLOOR_REF,
	SPECTRAL_PARAM_INIT_DELAY,
	SPECTRAL_PARAM_NB_TONE_THR,
	SPECTRAL_PARAM_STR_BIN_THR,
	SPECTRAL_PARAM_WB_RPT_MODE,
	SPECTRAL_PARAM_RSSI_RPT_MODE,
	SPECTRAL_PARAM_RSSI_THR,
	SPECTRAL_PARAM_PWR_FORMAT,
	SPECTRAL_PARAM_RPT_MODE,
	SPECTRAL_PARAM_BIN_SCALE,
	SPECTRAL_PARAM_DBM_ADJ,
	SPECTRAL_PARAM_CHN_MASK,
	SPECTRAL_PARAM_ACTIVE,
	SPECTRAL_PARAM_STOP,
	SPECTRAL_PARAM_ENABLE,
	SPECTRAL_PARAM_FREQUENCY,
	SPECTRAL_PARAM_CHAN_FREQUENCY,
	SPECTRAL_PARAM_CHAN_WIDTH,
	SPECTRAL_PARAM_COMPLETION_TIMEOUT,
	SPECTRAL_PARAM_MAX,
};

/**
 * enum spectral_scan_mode - Spectral scan mode
 * @SPECTRAL_SCAN_MODE_NORMAL: Normal mode
 * @SPECTRAL_SCAN_MODE_AGILE: Agile mode
 * @SPECTRAL_SCAN_MODE_MAX: Max number of Spectral modes
 * @SPECTRAL_SCAN_MODE_INVALID: Invalid Spectral mode
 */
enum spectral_scan_mode {
	SPECTRAL_SCAN_MODE_NORMAL,
	SPECTRAL_SCAN_MODE_AGILE,
	SPECTRAL_SCAN_MODE_MAX,
	SPECTRAL_SCAN_MODE_INVALID = 0xff,
};

/**
 * enum spectral_chan_width - Spectral-specific channel width enum
 * @SPECTRAL_CH_WIDTH_20MHZ: 20 MHz width
 * @SPECTRAL_CH_WIDTH_40MHZ: 40 MHz width
 * @SPECTRAL_CH_WIDTH_80MHZ: 80 MHz width
 * @SPECTRAL_CH_WIDTH_160MHZ: 160 MHz width
 * @SPECTRAL_CH_WIDTH_80P80MHZ: 80+80 MHz width
 * @SPECTRAL_CH_WIDTH_5MHZ: 5 MHz width
 * @SPECTRAL_CH_WIDTH_10MHZ: 10 MHz width
 * @SPECTRAL_CH_WIDTH_320MHZ: 320 MHz width
 * @SPECTRAL_CH_WIDTH_MAX: Max possible width
 * @SPECTRAL_CH_WIDTH_INVALID: invalid width
 */
enum spectral_chan_width {
	SPECTRAL_CH_WIDTH_20MHZ,
	SPECTRAL_CH_WIDTH_40MHZ,
	SPECTRAL_CH_WIDTH_80MHZ,
	SPECTRAL_CH_WIDTH_160MHZ,
	SPECTRAL_CH_WIDTH_80P80MHZ,
	SPECTRAL_CH_WIDTH_320MHZ,
	SPECTRAL_CH_WIDTH_5MHZ,
	SPECTRAL_CH_WIDTH_10MHZ,
	SPECTRAL_CH_WIDTH_MAX,
	SPECTRAL_CH_WIDTH_INVALID = 0xFF,
};

/**
 * struct spectral_config_frequency - Spectral scan frequency
 * @cfreq1: Center frequency of the primary span (in MHz)
 * @cfreq2: Center frequency of the secondary 80 MHz span (in MHz),
 *          applicable only for 80+80 MHz agile scan
 */
struct spectral_config_frequency {
	__u32 cfreq1;
	__u32 cfreq2;
};

/**
 * struct spectral_config - Spectral config parameters
 * @ss_fft_period:        Skip interval for FFT reports
 * @ss_period:            Spectral scan period
 * @ss_recapture:         Set to allow FFT recapture if scan period > 52us
 * @ss_count:             Number of reports to return from ss_active
 * @ss_short_report:      Set to report only 1 set of FFT results
 * @radar_bin_thresh_sel: Select threshold to classify strong bin for FFT
 * @ss_spectral_pri:      Priority, and are we doing a noise power cal?
 * @ss_fft_size:          Defines the number of FFT data points to compute
 * @ss_gc_ena:            Enable targeted gain change before spectral scan FFT
 * @ss_restart_ena:       Enable abort of receive frames when in high priority
 * @ss_noise_floor_ref:   Noise floor reference number (signed)
 * @ss_init_delay:        Disallow spectral scan triggers after tx/rx packets
 * @ss_nb_tone_thr:       Number of strong bins per sub-channel
 * @ss_str_bin_thr:       Bin/max_bin ratio threshold
 * @ss_wb_rpt_mode:       Report spectral scans as EXT_BLOCKER
 * @ss_rssi_rpt_mode:     Report spectral scans as EXT_BLOCKER if ADC RSSI low
 * @ss_rssi_thr:          ADC RSSI threshold (signed)
 * @ss_pwr_format:        Format of frequency bin magnitude
 * @ss_rpt_mode:          Format of per-FFT reports
 * @ss_bin_scale:         Number of LSBs to shift out to scale FFT bins
 * @ss_dbm_adj:           Report bin magnitudes converted to dBm power
 * @ss_chn_mask:          Per chain enable mask to select input ADC
 * @ss_nf_temp_data:      Temperature data taken during nf scan
 * @ss_frequency:         Frequency span for Spectral scan
 * @ss_bandwidth:         Spectral scan bandwidth
 * @ss_completion_timeout: Spectral scan completion timeout value
 */
struct spectral_config {
	__u16 ss_fft_period;
	__u16 ss_period;
	__u16 ss_recapture;
	__u16 ss_count;
	__u16 ss_short_report;
	__u8 radar_bin_thresh_sel;
	__u16 ss_spectral_pri;
	__u16 ss_fft_size;
	__u16 ss_gc_ena;
	__u16 ss_restart_ena;
	__u16 ss_noise_floor_ref;
	__u16 ss_init_delay;
	__u16 ss_nb_tone_thr;
	__u16 ss_str_bin_thr;
	__u16 ss_wb_rpt_mode;
	__u16 ss_rssi_rpt_mode;
	__u16 ss_rssi_thr;
	__u16 ss_pwr_format;
	__u16 ss_rpt_mode;
	__u16 ss_bin_scale;
	__u16 ss_dbm_adj;
	__u16 ss_chn_mask;
	__s32 ss_nf_temp_data;
	struct spectral_config_frequency ss_frequency;
	__u16 ss_bandwidth;
	__u32 ss_completion_timeout;
};

/**
 * struct spectral_caps - Spectral capabilities structure
 * @phydiag_cap:                  Phydiag capability
 * @radar_cap:                    Radar detection capability
 * @spectral_cap:                 Spectral capability
 * @advncd_spectral_cap:          Advanced spectral capability
 * @hw_gen:                       Spectral hw generation
 * @is_scaling_params_populated:  Whether scaling params is populated
 * @formula_id:                   Formula ID
 * @low_level_offset:             Low level offset
 * @high_level_offset:            High level offset
 * @rssi_thr:                     RSSI threshold
 * @default_agc_max_gain:         Default AGC max gain
 * @agile_spectral_cap:           Agile Spectral capability for 20/40/80
 * @agile_spectral_cap_160:       Agile Spectral capability for 160 MHz
 * @agile_spectral_cap_80p80:     Agile Spectral capability for 80p80
 * @agile_spectral_cap_320:       Agile Spectral capability for 320 MHz
 * @num_detectors_20mhz:          Number of Spectral detectors in 20 MHz
 * @num_detectors_40mhz:          Number of Spectral detectors in 40 MHz
 * @num_detectors_80mhz:          Number of Spectral detectors in 80 MHz
 * @num_detectors_160mhz:         Number of Spectral detectors in 160 MHz
 * @num_detectors_80p80mhz:       Number of Spectral detectors in 80p80 MHz
 * @num_detectors_320mhz:         Number of Spectral detectors in 320 MHz
 */
struct spectral_caps {
	__u8 phydiag_cap;
	__u8 radar_cap;
	__u8 spectral_cap;
	__u8 advncd_spectral_cap;
	__u32 hw_gen;
	bool is_scaling_params_populated;
	__u16 formula_id;
	__s16 low_level_offset;
	__s16 high_level_offset;
	__s16 rssi_thr;
	__u8 default_agc_max_gain;
	bool agile_spectral_cap;
	bool agile_spectral_cap_160;
	bool agile_spectral_cap_80p80;
	bool agile_spectral_cap_320;
	__u32 num_detectors_20mhz;
	__u32 num_detectors_40mhz;
	__u32 num_detectors_80mhz;
	__u32 num_detectors_160mhz;
	__u32 num_detectors_80p80mhz;
	__u32 num_detectors_320mhz;
};

#define MAX_SPECTRAL_CHAINS           (3)
#define MAX_NUM_BINS                  (2048)
#define SPECTRAL_MAC_ADDR_SIZE        (6)
#define MAX_SPECTRAL_PAYLOAD          (3028)

struct fft_sample_ath12k_detector_info {
	__be32 start_frequency;
	__be32 end_frequency;
	__be32 timestamp;
	__be32 last_tstamp;
	__be32 last_raw_timestamp;
	__be32 timestamp_war_offset;
	__be32 raw_timestamp;
	__be32 reset_delay;
	__be16 start_bin_idx;
	__be16 end_bin_idx;
	__be16 max_index;
	__be16 max_magnitude;
	__be16 noise_floor;
	u8 rssi;
	u8 agc_total_gain;
	u8 gainchange;
	u8 pri80ind;
	u8 is_sec80;
	u8 blanking_status;
} __packed;

struct fft_sample_ath12k {
	struct fft_sample_tlv tlv;
	__be32 signature;
	__be32 pri_freq;
	__be32 target_reset_count;
	__be32 cfreq1;
	__be32 cfreq2;
	__be32 sscan_cfreq1;
	__be32 sscan_cfreq2;
	__be32 bin_pwr_count;
	struct fft_sample_ath12k_detector_info detector_info;
	s8 spectral_lower_rssi;
	s8 spectral_upper_rssi;
	u8 macaddr[SPECTRAL_MAC_ADDR_SIZE];
	u8 spectral_mode;
	u8 operating_bw;
	u8 sscan_bw;
	u8 fft_width;
	u8 dcs_enabled;
	u8 int_type;
	u8 data[0];
} __packed;

/**
 * enum spectral_dma_debug - Spectral DMA debug
 * @SPECTRAL_DMA_RING_DEBUG:   Spectral DMA ring debug
 * @SPECTRAL_DMA_BUFFER_DEBUG: Spectral DMA buffer debug
 */
enum spectral_dma_debug {
	SPECTRAL_DMA_RING_DEBUG,
	SPECTRAL_DMA_BUFFER_DEBUG,
};

/**
 * struct spectral_diag_stats - spectral diag stats
 * @spectral_mismatch:             Spectral TLV signature mismatches
 * @spectral_sec80_sfft_insufflen: Insufficient length when parsing for
 *                                 Secondary 80 Search FFT report
 * @spectral_no_sec80_sfft:        Secondary 80 Search FFT report
 *                                 TLV not found
 * @spectral_vhtseg1id_mismatch:   VHT Operation Segment 1 ID
 *                                 mismatches in Search FFT report
 * @spectral_vhtseg2id_mismatch:   VHT Operation Segment 2 ID
 *                                 mismatches in Search FFT report
 * @spectral_invalid_detector_id:  Invalid detector id
 */
struct spectral_diag_stats {
	__u64 spectral_mismatch;
	__u64 spectral_sec80_sfft_insufflen;
	__u64 spectral_no_sec80_sfft;
	__u64 spectral_vhtseg1id_mismatch;
	__u64 spectral_vhtseg2id_mismatch;
	__u64 spectral_invalid_detector_id;
};

/**
 * struct spectral_scan_state - State of spectral scan
 * @is_active:  Is spectral scan active
 * @is_enabled: Is spectral scan enabled
 */
struct spectral_scan_state {
	__u8 is_active;
	__u8 is_enabled;
};

/**
 * enum spectral_scan_complete_status - Spectral scan completion status
 * @SPECTRAL_SCAN_COMPLETE_SUCCESS: Scan completed successfully
 * @SPECTRAL_SCAN_COMPLETE_TIMEOUT: Scan timed out
 * @SPECTRAL_SCAN_COMPLETE_MAX:     Max enumeration
 * @SPECTRAL_SCAN_COMPLETE_INVALID: Invalid status
 */
enum spectral_scan_complete_status {
	SPECTRAL_SCAN_COMPLETE_SUCCESS,
	SPECTRAL_SCAN_COMPLETE_TIMEOUT,
	SPECTRAL_SCAN_COMPLETE_MAX,
	SPECTRAL_SCAN_COMPLETE_INVALID = 0xff,
};

/**
 * enum spectral_scan_priority: Spectral scan priority
 * @SPECTRAL_SCAN_PRIORITY_LOW: Low priority Spectral scan
 * @SPECTRAL_SCAN_PRIORITY_HIGH: High priority Spectral scan
 */
enum spectral_scan_priority {
	SPECTRAL_SCAN_PRIORITY_LOW = 0,
	SPECTRAL_SCAN_PRIORITY_HIGH = 1,
};

#endif /* SPECTRAL_COMMON_H */
