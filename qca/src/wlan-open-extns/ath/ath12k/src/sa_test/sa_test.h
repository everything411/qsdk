/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef ATH12K_SA_TEST_H
#define ATH12K_SA_TEST_H

#include "../sa_api_defs.h"

/* State Variable */
/* state where pretraining will happen */
#define SMARTANTENNA_PRETRAIN           0
/* state where training is in progress currently */
#define SMARTANTENNA_TRAIN_INPROGRESS   1
/* state where stats are processed in training */
#define SMARTANTENNA_TRAIN_HOLD         2
/* default state where nothing will happen */
#define SMARTANTENNA_DEFAULT            3

/* Number of radio supports in test module */
#define SA_MAX_RADIOS 5
/* MAX RSSI sample collected from training stats */
#define SA_MAX_RSSI_SAMPLES 10
/* MAX receive chains */
#define SA_MAX_RECV_CHAINS 4
/* Supported Antenna Combinations */
#define SA_ANTENNA_COMBINATIONS 16
/* 1 for Parallel mode and 0 for Serial Mode */
#define SA_PARALLEL_MODE 1

/* default retrain interval,2 - minutes*/
#define SA_DEFAULT_RETRAIN_INTERVAL 2
/* number of milli seconds in a minute */
#define SA_NUM_MILLISECONDS_IN_MINUTE 60000
/* default throughput threshold in percentage */
#define SA_DEFAULT_TP_THRESHOLD 10
/* Minimum goodput drop to trigger retraining
 * For rates below 50 Mbps, the difference between consecutive
 * rates is about 6.5 Mbps.
 */
#define MIN_GOODPUT_THRESHOLD 6
/* 2 seconds */
#define SA_PERF_TRAIN_INTERVEL 2000
/* max number of perf slots for which no traffic can be there */
#define SA_MAX_NOTRAFFIC_TIME 10
/* default gudput ignoring interval */
#define SA_GPUT_IGNORE_INTERVAL 1
/* default gudput averaging interval */
#define SA_GPUT_AVERAGE_INTERVAL 2

/* MAX Number of packets to be generated */
#define SA_MAX_PRETRAIN_PACKETS 600
/* default value of max number of ppdu's for training */
#define SA_NUM_MAX_PPDU 50
/* PER diff threshold to apply secondary metric */
#define SA_PERDIFF_THRESHOLD 3
/* Upper bound to move to next lower rate */
#define SA_PER_UPPER_BOUND 80
/* Lower bound to move to next higher rate */
#define SA_PER_LOWER_BOUND 20
/* 3 intervals */
#define SA_DEFALUT_HYSTERISYS 3
/* Upper bound for periodic retrain miss in case of station */
#define SA_MAX_PRD_RETRAIN_MISS 10
/* Default threshold for checking BW change */
#define SA_N_BW_PPDU_THRESHOLD 64
/* default packet length */
#define SA_DEFAULT_PKT_LEN 1536
/* default minimum throughput to consider as traffic (86 -- 2 mbps)*/
#define SA_DEFAULT_MIN_TP  344
/* indication to use self cts */
#define SA_CTS_PROT 0x10000

#define SA_MAX_HT_RATES 24
#define SA_MAX_VHT_RATES 40
/* number of training packets required to be self generated in an interval */
#define NUM_TRAIN_PKTS 32
/* Number of packets required for legacy rates */
#define SA_NUM_LEGACY_TRAIN_PKTS 200
/* Default number of packets required for ht,vht rates */
#define SA_NUM_DEFAULT_TRAIN_PKTS 640
/* maximum packets that can be used in case of intense training */
#define SA_MAX_INTENSETRAIN_PKTS 1000
/* Default antenna for RX and TX. VHVH polarization */
#define SA_DEFAULT_ANTENNA 5

/* Number of Antenna per Chain */
#define SA_ANTENNAS_PER_CHAIN 2

#define SA_MAX_PERCENTAGE 100

#define SA_FEEDBACK_RATE_POSITION 8
#define SA_GET_RATE_FEEDBACK(rate_word, bw) \
	((rate_word) >> ((bw) * SA_FEEDBACK_RATE_POSITION))
#define SA_GET_BW_FEEDBACK(rate_index) (rate_index & 3)
#define SA_GET_SERIES_FEEDBACK(rate_index) ((rate_index >> 2) & 0x1)
#define SA_GET_BW_COMB_FEEDBACK(bw_nfb) ((bw_nfb >> 4) & 0xF)
#define SA_GET_NFB_COMB_FEEDBACK(bw_nfb) (bw_nfb & 0xF)

#define SA_11B_MODE 0x40
#define SA_11G_MODE 0x00
#define SA_MODE_BITS 0xF0
#define IS_SA_11B_RATE(rate) ((rate & SA_MODE_BITS) == SA_11B_MODE)
#define IS_SA_11G_RATE(rate) ((rate & SA_MODE_BITS) == SA_11G_MODE)

#define SA_STAT_INDEX_11B(rate) (rate & 0x0f)
#define SA_STAT_INDEX_11G(rate) ((rate & 0x0f) + 4)
#define SA_GET_NSS_FROM_RATE(rate) ((rate >> 4) & 0x03)
#define SA_GET_MCS_FROM_RATE(rate) (rate & 0x0f)
#define SA_STAT_INDEX_11N_11AC(nss, mcs) ((nss * 10) + mcs)
#define SA_MAX_MCS_HT 8
#define SA_MAX_MCS_VHT 10
#define SA_RATE_INDEX_11N(rate) \
	((SA_GET_NSS_FROM_RATE(rate) * SA_MAX_MCS_HT) + \
	 SA_GET_MCS_FROM_RATE(rate))
#define SA_RATE_INDEX_11AC(rate) \
	((SA_GET_NSS_FROM_RATE(rate) * SA_MAX_MCS_VHT) + \
	 SA_GET_MCS_FROM_RATE(rate))

#define SA_NIBBLE_MASK 0xF
#define SA_BYTE_MASK   0xFF
#define SA_WORD_MASK   0xFFFF

/* Bandwidth mask used to extract BW from RateCode */
#define SA_MAX_BW_MASK 0x7
/* Lower 16bit have 11 bit rate code higher 16bit have 3 bit Bandwidth code */
#define SA_GET_BW_FROM_RC(x) ((x) & SA_MAX_BW_MASK)

/* will be in last bit0 - bit7 */
#define SA_0TH_POSITION 0
/* will be in last bit8 - bit15 */
#define SA_8TH_POSITION 8
/* will be in last bit16 - bit23 */
#define SA_16TH_POSITION 16
/* will be in last bit24 - bit31 */
#define SA_24TH_POSITION 24

#define SA_MASK_TXCHAIN 0x0F
#define SA_MASK_RXCHAIN 0xF0

#define SA_RADIOID_OFFSET 4
#define SA_GET_RADIO_ID(radio_id) \
	(((radio_id) >> SA_RADIOID_OFFSET) & SA_NIBBLE_MASK)

#define SA_DEBUG 1
#define not_yet 0
#define SA_DEBUG_LVL1 0x1
#define SA_DEBUG_LVL2 0x2
#define SA_DEBUG_LVL3 0x4
#define SA_DEBUG_LVL4 0x8
#define SA_DEBUG_LVL5 0x10
#define SA_DEBUG_LVL6 0x20

#define SA_DEBUG_MASK(mask, radio_id) \
	(((radio_id) < SA_MAX_RADIOS) && (g_sa.radio[radio_id].sa_dbg_level & (mask)))
#define SA_DEBUG_PRINT(mask, radio_id, fmt, ...) do { \
	if (SA_DEBUG_MASK(mask, radio_id)) { \
		pr_info("%s: " fmt, __func__, ##__VA_ARGS__); \
	} \
} while (0)

/* Train Enable bitmask to denote which type */
#define SA_INIT_TRAIN_EN    0x1
#define SA_PERIOD_TRAIN_EN  0x2
#define SA_PERF_TRAIN_EN    0x4
#define SA_RX_TRAIN_EN 0x10

/* Training uses double number of packets */
#define SA_CONFIG_INTENSETRAIN 0x1
/* Do extra training when first metric conflicts */
#define SA_CONFIG_EXTRATRAIN   0x2
/* Protect extra training frames with self CTS */
#define SA_CONFIG_SLECTSPROTEXTRA 0x4
/* Protect all training frames with self CTS */
#define SA_CONFIG_SLECTSPROTALL   0x8
#define SA_TX_FEEDBACK_CONFIG_DEFAULT 0xe4
#define SA_DBG_CODE_NXT_RATE_LLB 0x10
#define SA_DBG_CODE_CUR_RATE_LLB 0x20
#define SA_DBG_CODE_NXT_RATE_LUB 0x30
#define SA_DBG_CODE_RSSI_BETTER  0x40
#define SA_DBG_CODE_PER_BETTER   0x50
#define SA_DBG_CODE_ANT_SWITCH   0x01

/**
 * enum sa_ratecount_index - Rate count array index per bandwidth
 * @SA_RATECOUNT_IDX_LEGACY: Legacy CCK/OFDM rates count index.
 * @SA_RATECOUNT_IDX_20: 20 MHz bandwidth rates count index.
 * @SA_RATECOUNT_IDX_40: 40 MHz bandwidth rates count index.
 * @SA_RATECOUNT_IDX_80: 80 MHz bandwidth rates count index.
 * @SA_RATECOUNT_IDX_160: 160 MHz bandwidth rates count index.
 * @SA_RATECOUNT_IDX_320: 320 MHz bandwidth rates count index.
 */
enum sa_ratecount_index {
	SA_RATECOUNT_IDX_LEGACY = 0,
	SA_RATECOUNT_IDX_20,
	SA_RATECOUNT_IDX_40,
	SA_RATECOUNT_IDX_80,
	SA_RATECOUNT_IDX_160,
	SA_RATECOUNT_IDX_320,
};

/**
 * enum sa_bw_index - Smart Antenna bandwidth indices
 * @SA_BW_20: 20 MHz bandwidth.
 * @SA_BW_40: 40 MHz bandwidth.
 * @SA_BW_80: 80 MHz bandwidth.
 * @SA_BW_160: 160 MHz bandwidth.
 * @SA_BW_320: 320 MHz bandwidth.
 * @SA_NUM_BW: Number of supported bandwidth indices.
 */
enum sa_bw_index {
	SA_BW_20 = 0,
	SA_BW_40,
	SA_BW_80,
	SA_BW_160,
	SA_BW_320,
	SA_NUM_BW,
};

/**
 * enum sa_opmodes - Node operating modes
 * @SA_OPMODE_LEGACY: Legacy (11a/b/g) mode.
 * @SA_OPMODE_HT: 802.11n High Throughput mode.
 * @SA_OPMODE_VHT: 802.11ac Very High Throughput mode.
 * @SA_OPMODE_HE: 802.11ax High Efficiency mode.
 * @SA_OPMODE_EHT: 802.11be Extremely High Throughput mode.
 */
enum sa_opmodes {
	SA_OPMODE_LEGACY = 0,
	SA_OPMODE_HT,
	SA_OPMODE_VHT,
	SA_OPMODE_HE,
	SA_OPMODE_EHT,
};

/**
 * enum sa_per_bw_pkt_threshold - Min packet thresholds per bandwidth
 * @SA_NUM_PKT_THRESHOLD_20: Min packets to consider 20 MHz active.
 * @SA_NUM_PKT_THRESHOLD_40: Min packets to consider 40 MHz active.
 * @SA_NUM_PKT_THRESHOLD_80: Min packets to consider 80 MHz active.
 * @SA_NUM_PKT_THRESHOLD_160: Min packets to consider 160 MHz active.
 * @SA_NUM_PKT_THRESHOLD_320: Min packets to consider 320 MHz active.
 */
enum sa_per_bw_pkt_threshold {
	SA_NUM_PKT_THRESHOLD_20 = 20,
	SA_NUM_PKT_THRESHOLD_40 = 10,
	SA_NUM_PKT_THRESHOLD_80 = 5,
	SA_NUM_PKT_THRESHOLD_160 = 5,
	SA_NUM_PKT_THRESHOLD_320 = 5,
};

/**
 * enum sa_training_mode - Traning mode
 * @SA_TRAINING_MODE_EXISTING: Existing traffic only
 * @SA_TRAINING_MODE_MIXED: Self packet generation with existing
 */
enum sa_training_mode {
        SA_TRAINING_MODE_EXISTING = 0,
        SA_TRAINING_MODE_MIXED = 1,
};

/*
 * enum smart_ant_param - Smart Antenna control parameter IDs
 * @SMART_ANT_PARAM_HELP: Display available command help.
 * @SMART_ANT_PARAM_TRAIN_MODE: Select training mode (0 = existing traffic only,
 * 1 = self packet generation with existing)
 * @SMART_ANT_PARAM_TRAIN_PER_THRESHOLD: Configure lower/upper bounds and PER
 * difference threshold for training decisions.
 * @SMART_ANT_PARAM_PKT_LEN: Set training packet length in bytes.
 * @SMART_ANT_PARAM_NUM_PKTS: Set number of packets to send for training.
 * @SMART_ANT_PARAM_TRAIN_START: Start smart antenna training.
 * @SMART_ANT_PARAM_TRAFFIC_GEN_TIMER: Set self traffic generation timer in ms.
 * @SMART_ANT_PARAM_TRAIN_ENABLE: Enable/disable training triggers (initial,
 * periodic, performance); bitmask.
 * @SMART_ANT_PARAM_PERFTRAIN_INTERVAL: Set performance monitoring
 * interval in ms.
 * @SMART_ANT_PARAM_RETRAIN_INTERVAL: Set periodic retrain interval in minutes.
 * @SMART_ANT_PARAM_PERFTRAIN_THRESHOLD: Set goodput change threshold (percent)
 * to trigger performance training.
 * @SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD: Set minimum goodput drop (Mbps) to
 * trigger performance training.
 * @SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL: Set number of intervals to average
 * goodput for performance trigger.
 * @SMART_ANT_PARAM_DEFAULT_ANTENNA: Set default RX antenna.
 * @SMART_ANT_PARAM_DEFAULT_TX_ANTENNA: Set default TX antenna on association.
 * @SMART_ANT_PARAM_TX_ANTENNA: Set current TX antenna.
 * @SMART_ANT_PARAM_DBG_LEVEL: Set per-radio debug level bitmask.
 * @SMART_ANT_PARAM_PRETRAIN_PKTS: Set number of pretrain packets.
 * @SMART_ANT_PARAM_OTHER_BW_PKTS_TH: Set threshold for packets sent in
 * non-active bandwidth during training.
 * @SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL: Set number of intervals to ignore
 * goodput before averaging/triggers.
 * @SMART_ANT_PARAM_MIN_PKT_TH_BW20: Set minimum packet threshold to consider
 * 20 MHz bandwidth active.
 * @SMART_ANT_PARAM_MIN_PKT_TH_BW40: Set minimum packet threshold to consider
 * 40 MHz bandwidth active.
 * @SMART_ANT_PARAM_MIN_PKT_TH_BW80: Set minimum packet threshold to consider
 * 80 MHz bandwidth active.
 * @SMART_ANT_PARAM_MIN_PKT_TH_BW160: Set minimum packet threshold to consider
 * 160 MHz bandwidth active.
 * @SMART_ANT_PARAM_MIN_PKT_TH_BW320: Set minimum packet threshold to consider
 * 320 MHz bandwidth active.
 * @SMART_ANT_PARAM_DEBUG_INFO: Display debug information.
 * @SMART_ANT_PARAM_MAX_TRAIN_PPDU: Set maximum number of training PPDUs.
 * @SMART_ANT_PARAM_PERF_HYSTERESIS: Set hysteresis (intervals) for
 * performance-based trigger.
 * @SMART_ANT_PARAM_TX_FEEDBACK_CONFIG: Configure TX feedback rate (1 feedback
 * per N PPDUs).
 * @SMART_ANT_PARAM_BCN_ANTENNA: Set beacon antenna.
 * @SMART_ANT_PARAM_PARALLEL_MODE_GPIO_DRIVE: Set GPIO drive configuration for
 * parallel mode.
 * @SMART_ANT_PARAM_TRAIN_RATE_TESTMODE: Set training rate in test mode.
 * @SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE: Set training antenna in test mode.
 * @SMART_ANT_PARAM_TRAIN_PACKETS_TESTMODE: Set number of training packets in
 * test mode.
 * @SMART_ANT_PARAM_TRAIN_MINPKTS_TESTMODE: Set minimum packets to be trained
 * in test mode.
 * @SMART_ANT_PARAM_TRAIN_PERTH_TESTMODE: Set PER threshold to stop training in
 * test mode after minimum packets.
 * @SMART_ANT_PARAM_TRAIN_START_TESTMODE: Start training in test mode.
 * @SMART_ANT_PARAM_TRAIN_EXTN_FLAGS: Set extended training flags like disable
 *    latency traffic and enable QoS NULL frames in training.
 * @SMART_ANT_PARAM_MAX_MPDU_COUNT_FOR_TARINING: Maximum per Radio MPDU count
 *    for training.
 * @SMART_ANT_PARAM_MCS_MARGIN: MCS margin per Radio
 */
enum smart_ant_param  {
	SMART_ANT_PARAM_HELP = 0,
	SMART_ANT_PARAM_TRAIN_MODE,
	SMART_ANT_PARAM_TRAIN_PER_THRESHOLD,
	SMART_ANT_PARAM_PKT_LEN,
	SMART_ANT_PARAM_NUM_PKTS,
	SMART_ANT_PARAM_TRAIN_START,
	SMART_ANT_PARAM_TRAFFIC_GEN_TIMER,
	SMART_ANT_PARAM_TRAIN_ENABLE,
	SMART_ANT_PARAM_PERFTRAIN_INTERVAL,
	SMART_ANT_PARAM_RETRAIN_INTERVAL,
	SMART_ANT_PARAM_PERFTRAIN_THRESHOLD,
	SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD,
	SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL,
	SMART_ANT_PARAM_DEFAULT_ANTENNA,
	SMART_ANT_PARAM_DEFAULT_TX_ANTENNA,
	SMART_ANT_PARAM_TX_ANTENNA,
	SMART_ANT_PARAM_DBG_LEVEL,
	SMART_ANT_PARAM_PRETRAIN_PKTS,
	SMART_ANT_PARAM_OTHER_BW_PKTS_TH,
	SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL,
	SMART_ANT_PARAM_MIN_PKT_TH_BW20,
	SMART_ANT_PARAM_MIN_PKT_TH_BW40,
	SMART_ANT_PARAM_MIN_PKT_TH_BW80,
	SMART_ANT_PARAM_MIN_PKT_TH_BW160,
	SMART_ANT_PARAM_MIN_PKT_TH_BW320,
	SMART_ANT_PARAM_DEBUG_INFO,
	SMART_ANT_PARAM_MAX_TRAIN_PPDU,
	SMART_ANT_PARAM_PERF_HYSTERESIS,
	SMART_ANT_PARAM_TX_FEEDBACK_CONFIG,
	SMART_ANT_PARAM_BCN_ANTENNA,
	SMART_ANT_PARAM_PARALLEL_MODE_GPIO_DRIVE,
	SMART_ANT_PARAM_TRAIN_RATE_TESTMODE,
	SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE,
	SMART_ANT_PARAM_TRAIN_PACKETS_TESTMODE,
	SMART_ANT_PARAM_TRAIN_MINPKTS_TESTMODE,
	SMART_ANT_PARAM_TRAIN_PERTH_TESTMODE,
	SMART_ANT_PARAM_TRAIN_START_TESTMODE,
	SMART_ANT_PARAM_TRAIN_EXTN_FLAGS_TESTMODE,
	SMART_ANT_PARAM_MAX_MPDU_COUNT_FOR_TARINING,
	SMART_ANT_PARAM_MCS_MARGIN,
};

/*
 * struct smartantenna_params - Per-radio Smart Antenna configuration
 * @retrain_interval: Periodic retrain interval (minutes).
 * @perftrain_interval: Performance monitor interval (milliseconds).
 * @default_tx_antenna: Default TX antenna used when node connects.
 * @n_packets_to_train: Number of packets used for training.
 * @packetlen: Training packet length in bytes.
 * @num_pkt_threshold: Minimum traffic threshold (packets) to consider.
 * @num_pretrain_packets: Number of pre-training packets.
 * @num_other_bw_pkts_th: Threshold for packets sent in non-active bandwidth.
 * @max_train_ppdu: Maximum number of PPDUs for training.
 * @min_train_pkts: Minimum training packets.
 * @per_threshold: PER threshold for training.
 * @parallel_gpio_drive: Parallel mode GPIO drive configuration.
 * @num_min_pkt_threshod: Minimum packet thresholds to detect active
 *     bandwidth per BW.
 * @max_mpdu_train_cnt: Maximum MPDU count for training
 * @training_mode: Training mode selector (0 = existing traffic only,
 *     1 = self packet generation with existing traffic)
 * @lower_bound: Lower bound for rate drop in training.
 * @upper_bound: Upper bound for rate increase in training.
 * @per_diff_threshold: PER difference treated as equal when comparing.
 * @num_tx_antennas: Number of possible TX antenna combinations.
 * @num_recv_chains: Number of RX chains.
 * @max_throughput_change: Goodput change threshold (percent) for
 *     performance trigger.
 * @hysteresis: Hysteresis value (intervals) for throughput fluctuations.
 * @min_goodput_threshold: Minimum goodput drop (Mbps) to trigger retraining.
 * @goodput_avg_interval: Goodput averaging interval (number of slots).
 * @goodput_ignore_interval: Goodput ignoring interval before averaging.
 * @config: Algorithm configuration bitmask (see SA_CONFIG_*).
 * @train_enable: Master bitmap indicating training enable for init/perf/prd.
 * @antenna_change_ind: Global indication to change antenna in next feedback.
 * @en_rts_ant: Config to enable Separate antenna for RTS
 * @mcs_margin_en: Flag to hold MCS margin enable/disable
 * @mcs_margin_val: Holds mcs margin value
 */
struct smartantenna_params {
	u32 retrain_interval;
	u32 perftrain_interval;
	u32 default_tx_antenna;
	u16 n_packets_to_train;
	u16 packetlen;
	u16 num_pkt_threshold;
	u16 num_pretrain_packets;
	u16 num_other_bw_pkts_th;
	u16 max_train_ppdu;
	u16 min_train_pkts;
	u16 per_threshold;
	u16 parallel_gpio_drive;
	u16 num_min_pkt_threshod[SA_NUM_BW];
	u16 max_mpdu_train_cnt;
	u8 training_mode;
	u8 lower_bound;
	u8 upper_bound;
	u8 per_diff_threshold;
	u8 num_tx_antennas;
	u8 num_recv_chains;
	u8 max_throughput_change;
	u8 hysteresis;
	u8 min_goodput_threshold;
	u8 goodput_avg_interval;
	u8 goodput_ignore_interval;
	u8 config;
	u8 train_enable;
	u8 antenna_change_ind;
	u8 en_rts_ant;
	u8 mcs_margin_en;
	u8 mcs_margin_val;
};

/*
 * struct radio_config - Per-radio runtime configuration and state
 * @init_config: Initial SA config pushed to target.
 * @sa_params: Smart Antenna configuration parameters.
 * @gpio_cfg: User defined GPIO pins and functions as per board design
 * @default_antenna: Default RX/TX antenna.
 * @bcn_antenna: Beacon antenna.
 * @node_count_per_ant: Count of nodes per antenna combination.
 * @nodes_connected: Number of nodes connected.
 * @init_done: Initialization completed flag.
 * @sa_dbg_level: Debug level bitmask for this radio.
 */
struct radio_config {
	struct ath12k_sa_config init_config;
	struct smartantenna_params sa_params;
	struct ath12k_sa_gpio_config gpio_cfg;
	u32 default_antenna;
	u32 bcn_antenna;
	u8 node_count_per_ant[SA_ANTENNA_COMBINATIONS];
	u16 nodes_connected;

	u8 init_done;
	u8 sa_dbg_level;
};

/*
 * struct sa_global - Global Smart Antenna context
 * @radio: Per-radio configuration array.
 */
struct sa_global {
	struct radio_config radio[SA_MAX_RADIOS];
};

/*
 * struct sa_node_rate_stats - Per-node rate and goodput statistics
 * @npackets_legacy: Packet count per legacy rates.
 * @npackets: Packet count per BW and MCS.
 * @nppdu_bw: Training PPDU count per BW.
 * @nppdu_gp: Goodput PPDU count per BW.
 * @ins_goodput: Instantaneous goodput per BW.
 * @rate_mcs: Last packet MCS value.
 * @rate_maxphy: Last packet max PHY rate.
 */
struct sa_node_rate_stats {
	u32 npackets_legacy[SA_MAX_CCK_OFDM_RATES];
	u32 npackets[SA_NUM_BW][SA_MAX_MCS_RATES];
	u32 nppdu_bw[SA_NUM_BW];
	u32 nppdu_gp[SA_NUM_BW];
	u32 ins_goodput[SA_NUM_BW];
	u32 rate_mcs;
	u32 rate_maxphy;
};

/*
 * struct node_train_stats - Current training round metrics for a node
 * @rate: Rate code used for training.
 * @antenna: Training antenna.
 * @nbad: Failed frames for current training rate/antenna.
 * @nframes: Total frames sent for current training rate/antenna.
 * @per: Training PER.
 * @bw: Training bandwidth.
 * @rssi: ACK/BA RSSI samples per chain.
 * @nextant_per: PER for next antenna.
 * @nextant_nbad: Failed frames for next rate/antenna.
 * @nextant_nframes: Total frames sent for next rate/antenna.
 * @nextant_rssi: ACK/BA RSSI samples for next antenna.
 * @bw_change: Bandwidth changed flag.
 * @first_per: Flag for storing first PER sample.
 * @last_rate: Previous train rate index.
 * @last_train_dir: Last trained direction; 1 above rate, -1 below.
 * @ant_map: Antenna combinations index map.
 * @skipmask: Skip mask for antennas/rates.
 * @extra_stats: Collect extra stats flag.
 * @stats_dir: Direction of PER used for extra stats.
 * @double_train: Use double packets for training flag.
 * @extra_sel: Extra stats for selected antenna flag.
 * @extra_cmp: Extra stats for comparing antenna flag.
 * @extra_nbad: Failed frames during extra stats collection.
 * @extra_nframes: Total frames during extra stats collection.
 */
struct node_train_stats {
	u32 rate;
	u32 antenna;
	u16 nbad;
	u16 nframes;
	u8 per;
	u8 bw;
	u8 rssi[SA_MAX_RECV_CHAINS][SA_MAX_RSSI_SAMPLES];

	u8 nextant_per;
	u16 nextant_nbad;
	u16 nextant_nframes;
	s8 nextant_rssi[SA_MAX_RECV_CHAINS][SA_MAX_RSSI_SAMPLES];

	u8 bw_change;
	u8 first_per;
	u32 last_rate;
	s8 last_train_dir;
	u32 ant_map[SA_ANTENNA_COMBINATIONS];
	u32 skipmask;

	u8 extra_stats;
	u8 stats_dir;
	u8 double_train;
	u8 extra_sel;
	u8 extra_cmp;
	u16 extra_nbad;
	u16 extra_nframes;
};

/*
 * struct sa_perf_info - Performance trigger tracking info
 * @trigger_type: Trigger type (+1 positive, -1 negative).
 * @hysteresis: Hysteresis for throughput drop.
 * @gput_avg_interval: Number of intervals to ignore goodput for retraining.
 * @idle_time: Idle time counter.
 * @avg_goodput: Average goodput of previous interval.
 */
struct sa_perf_info {
	s8  trigger_type;
	u8 hysteresis;
	s8 gput_avg_interval;
	s8 idle_time;
	u32 avg_goodput;
};

/*
 * struct sa_node_train_info - Per-node training control and state
 * @is_training: Current training status flag.
 * @train_enable: Bitmask indicating which training is enabled.
 * @smartantenna_state: Node-specific smart antenna state.
 * @intense_train: Use more packets for training flag.
 * @selected_antenna: Currently used TX antenna.
 * @previous_selected_antenna: Previously used TX antenna.
 * @train_rate: Rate index currently used.
 * @prev_ratemax: Previous max rate.
 * @txed_packets: Unicast packets transmitted in retrain interval.
 * @last_train_bw: Last trained bandwidth.
 * @retrain_miss: Consecutive retrain intervals without retraining.
 * @long_retrain_count: Long retrain counter.
 * @traffic_none: Consecutive intervals without any traffic.
 * @perf_info: Performance info per bandwidth.
 * @ts_perfslot: Start timestamp of performance monitor slot.
 * @hybrid_train: Hybrid mode training flag.
 * @ts_trainstart: Training start timestamp.
 * @ts_traindone: Training end timestamp.
 * @prd_trigger_count: Periodic trigger count (debug).
 * @perf_trigger_count: Performance trigger count (debug).
 * @last_training_time: Last training time (debug).
 * @train_state: Current training state info.
 * @training_start: Non-1 indicates fixed rate training.
 * @antenna_change_ind: Change antenna in next feedback flag.
 * @nppdu_bw: Training PPDU count per bandwidth.
 * @nppdu: Training PPDU counter.
 * @tx_feedback_config: TX feedback configuration (1 feedback per N PPDUs).
 */
struct sa_node_train_info {
	u8 is_training;
	u8 train_enable;
	u8 smartantenna_state;
	u8 intense_train;
	u32 selected_antenna;
	u32 previous_selected_antenna;
	u32 train_rate;
	u32 prev_ratemax;
	u32 txed_packets;

	u8 last_train_bw;
	u8 retrain_miss;
	u8 long_retrain_count;
	u8 traffic_none;

	struct sa_perf_info perf_info[SA_NUM_BW];
	u32 ts_perfslot;

	u8 hybrid_train;
	u32 ts_trainstart;
	u32 ts_traindone;
#if SA_DEBUG
	u16 prd_trigger_count;
	u16 perf_trigger_count;
	u16 last_training_time;
#endif
	struct node_train_stats train_state;
	u8 training_start;
	u8 antenna_change_ind;
	u32 nppdu_bw[SA_NUM_BW];
	u32 nppdu;
	u8 tx_feedback_config;
};

/*
 * struct sa_node_train_data - Training data/control payload for a node
 * @antenna: Antenna for which stats are collected.
 * @ratecode: Rate code at which training is happening.
 * @nFrames: Total number of transmitted frames.
 * @nBad: Total number of failed frames.
 * @rssi: ACK/BA RSSI per chain for samples.
 * @last_nFrames: Packets sent in last interval.
 * @numpkts: Number of packets required for training.
 * @cts_prot: CTS protection flag.
 * @samples: Number of RSSI samples.
 */
struct sa_node_train_data {
	u32 antenna;
	u32 ratecode;
	u16 nFrames;
	u16 nBad;
	u16 last_nFrames;
	u16 numpkts;
	u8 cts_prot;
	u8 samples;
	s8 rssi[SA_MAX_RECV_CHAINS][SA_MAX_RSSI_SAMPLES];
};

#if SA_DEBUG
#define SA_MAX_DEBUG_TRAIN_CMDS 16
/*
 * struct sa_debug_train_cmd - Debug snapshot of a training decision
 * @antenna: Antenna used.
 * @ratecode: Rate code used.
 * @nframes: Frames transmitted.
 * @nbad: Failed frames.
 * @nppdu: PPDUs transmitted.
 * @rssi: RSSI per chain.
 * @per: PER observed.
 * @bw: Bandwidth used.
 * @ant_sel: Antenna selection code.
 * @last_train_dir: Last training direction.
 * @last_rate: Last rate index.
 */
struct sa_debug_train_cmd {
	u32 antenna;
	u32 ratecode;
	u32 last_rate;
	u16 nframes;
	u16 nbad;
	u16 nppdu;
	s8 rssi[SA_MAX_RECV_CHAINS];
	u8 per;
	u8 bw;
	u8 ant_sel;
	u8 last_train_dir;
};

/*
 * struct sa_node_debug_data - Per-node debug command buffer
 * @train_cmd: Circular buffer of recent training command snapshots.
 * @cmd_count: Number of commands recorded.
 */
struct sa_node_debug_data {
	struct sa_debug_train_cmd train_cmd[SA_MAX_DEBUG_TRAIN_CMDS];
	u32 cmd_count;
};
#endif

/*
 * struct sa_node_ccp - Composite per-node Smart Antenna context
 * @node_info: Node information.
 * @train_info: Training control and state.
 * @rate_stats: Per-node rate and goodput statistics.
 * @train_data: Training data/control payload.
 * @debug_data: Debug data (present when SA_DEBUG enabled).
 */
struct sa_node_ccp {
	struct ath12k_sa_node_info node_info;
	struct sa_node_train_info train_info;
	struct sa_node_rate_stats rate_stats;
	struct sa_node_train_data train_data;
#if SA_DEBUG
	struct sa_node_debug_data debug_data;
#endif
};

/*
 * struct sa_test_train_params - Test training configuration
 * @ratecode: Training rate per BW index (0=legacy/20,1=40,2=80,3=160,4=320).
 * @antenna: Training antenna.
 * @numpkts: Number of packets required for training.
 * @minpkts: Minimum packets to be trained before evaluating PER threshold.
 * @per_threshold: PER threshold for training stop.
 * @extn_train_flags: Extended flags for training. Like to disbale Latency traffic
 *    and enable QoS NULL frame in training.
 */
struct sa_test_train_params {
	u16 ratecode[SA_NUM_BW];
	u32 antenna;
	u16 numpkts;
	u16 minpkts;
	u16 per_threshold;
	u32 extn_train_flags;
};

#endif /* ATH12K_SA_TEST_H */
