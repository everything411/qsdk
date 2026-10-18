/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef ATH12K_SA_API_DEFS_H
#define ATH12K_SA_API_DEFS_H

#include <net/cfg80211.h>

/**
 * Max rate series
 * 32-bit holds 2 BW\'s Rate Codes. We are supporting 5 BWs.
 * 2 series of Rate Code can be set.
 * So, 2 * 3 DWORDS = 6
 **/
#define SA_MAX_RATE_SERIES		6
/* Max chains */
#define SA_MAX_SA_CHAINS		4
/* Max RSSI chains */
#define SA_MAX_RSSI_CHAINS		8
/* Max bandwidth */
#define SA_MAX_BW			8
/* Max pilot EVMs */
#define SA_MAX_EVM_SIZE			32
/**
 * Max retries index
 * Re-tries index representation
 * index0 - s0_bw20, index1 - s0_bw40
 * index4 - s1_bw20 ... index7: s1_bw160.
 */
#define SA_MAX_RETRIES_INDEX		8

/* Flag for Rx Antenna configuration */
#define SA_RX_CONFIG_REQUIRED		0x01
/* Flag for Tx Antenna configuration */
#define SA_TX_CONFIG_REQUIRED		0x02
/* Flag for Default Tx Antenna configuration */
#define SA_DEFAULT_TX_CONFIG_REQUIRED	0x04
/* Flag for Beacon antenna configuration */
#define SA_BEACON_ANT_CONFIG_REQUIRED	0x08
/* Flag for Parallel mode GPIO configuration */
#define SA_PARALLEL_GPIO_CFG_REQUIRED	0x10
/* Flag for Training */
#define SA_TRAINING_REQUIRED		0x20
/* Flag for Tx feedback configuration */
#define SA_TX_FEEDBACK_CONFIG_REQUIRED	0x40
/* Flag to set Maximum MPDU count and MCS margin for training */
#define SA_MPDU_CNT_MCS_MRG_REQUIRED	0x80

/* Flag used to get Tx feedback configuration for training */
#define SA_TX_FEEDBACK_CONFIG		0x01

/* Antenna mask. */
#define SA_ANTENNA_MASK			0x00FFFFFF

/* Maximum CCK, OFDM rate codes supported */
#define SA_MAX_CCK_OFDM_RATES		12
/* Maximum MCS rate codes supported
 * (Number of stream * Number of MCS code) = 8*12;
 * 11-bit ratecode packed in 16 bits
 */
#define SA_MAX_MCS_RATES		96
/* Total valid rates */
#define SA_MAX_VALID_RATES (SA_MAX_MCS_RATES + SA_MAX_CCK_OFDM_RATES)
/* Number of Rate Code for each BW */
#define SA_MAX_RATE_COUNTERS		6
/* Max PHY rate count */
#define SA_MAX_PHYRATE_COUNT		5

/* BSS mode AP */
#define SA_BSS_MODE_AP			0x00
/* BSS mode STA */
#define SA_BSS_MODE_STA			0x01

/* Tx feedback enable mask */
#define SA_TX_FEEDBACK_MASK		0x10
/* Rx feedback enable mask */
#define SA_RX_FEEDBACK_MASK		0x20

/* Maximum GPIO pins */
#define SA_MAX_GPIO_PINS		4
/* Maximum GPIO Functions */
#define SA_MAX_GPIO_FUNCS		4

/* Smart Antenna Extented training flags */
#define SA_LATENCY_TRAFFIC_DISABLE	0x1
#define SA_QOS_NULL_ENABLE		0x2

/**
 * struct ath12k_sa_config - Smart Antenna configuration.
 * @band: band to configure.
 * @txrx_feedback: Tx/Rx feedback.
 * @radio_id: radio id.
 * @chan_num: channel number.
 * @bss_mode: BSS mode.
 * @nss: NSS.
 * @txrx_chainmask: Tx/Rx chainmask.
 */
struct ath12k_sa_config {
	enum nl80211_band band;
	u32 txrx_feedback;
	u8 radio_id;
	u8 chan_num;
	u8 bss_mode;
	u8 nss;
	u8 txrx_chainmask;
};

/**
 * struct ath12k_sa_gpio_config - Smart Antenna GPIO configuration
 * @gpio_pin: Platform specific GPIO numbers
 * @gpio_func: GPIO fucntions
 */
struct ath12k_sa_gpio_config {
	u32 gpio_pin[SA_MAX_GPIO_PINS];
	u32 gpio_func[SA_MAX_GPIO_FUNCS];
};

/**
 * struct ath12k_sa_rate_cap - Smart Antenna rate capabilities.
 * @rc_legacy: Rate code array for CCK OFDM.
 * @rc_20: Rate code array for 20MHz BW.
 * @rc_40: Rate code array for 40MHz BW.
 * @rc_80: Rate code array for 80MHz BW.
 * @rc_160: Rate code array for 160MHz BW.
 * @rc_320: Rate code array for 320MHz BW.
 * @num_rc: Number of Rate Code for each BW.
 */
struct ath12k_sa_rate_cap {
	u16 rc_legacy[SA_MAX_CCK_OFDM_RATES];
	u16 rc_20[SA_MAX_MCS_RATES];
	u16 rc_40[SA_MAX_MCS_RATES];
	u16 rc_80[SA_MAX_MCS_RATES];
	u16 rc_160[SA_MAX_MCS_RATES];
	u16 rc_320[SA_MAX_MCS_RATES];
	u8 num_rc[SA_MAX_RATE_COUNTERS];
};

/**
 * struct ath12k_sa_node_info - Smart Antenna node information.
 * @mac_addr: MAC address.
 * @radio_id: radio id.
 * @max_ch_bw: 0-20 MHz, 1-40 MHz, 2-80 MHz, 3-160 MHz and 4-320 MHz.
 * @opmode: 0-Legacy, 1-11n, 2-11ac, 3-11ax, 4-11be Mode.
 * @chan_num: operating channel number.
 * @nss_tx: Number of Tx streams
 * @nss_rx: Number of Rx streams
 * @ni_cap: node capabilities.
 * @band: band corresponding to channel number.
 * @rate_cap: rate capabilities.
 */
struct ath12k_sa_node_info {
	u8 mac_addr[ETH_ALEN];
	u8 radio_id;
	u8 max_ch_bw;
	u8 opmode;
	u8 chan_num;
	u32 nss_tx;
	u32 nss_rx;
	u32 ni_cap;
	enum nl80211_band band;
	struct ath12k_sa_rate_cap rate_cap;
};

/**
 * struct ath12k_sa_tx_feedback - TX feedback from Smart Antenna transmissions.
 * @tx_antenna: Antenna mask per rate series used for TX.
 * @rssi: RSSI values of ACK/Block ACK per chain.
 * @rate_mcs: Rate series (MCS/legacy) used for TX.
 * @goodput: Measured goodput for the transmission window.
 * @num_packets: Number of packets covered by this feedback.
 * @num_bad: Number of failed/bad packets within @num_packets.
 * @nshort_retries: Short retry counts per series/BW index.
 * @nlong_retries: Long retry counts per series/BW index.
 * @ratemaxphy: Maximum PHY rate per bandwidth.
 * @rate_index: Index of the successfully transmitted rate in @rate_mcs.
 * @is_trainpkt: Set if the packet was sent for training.
 * @num_comb_feedback: Number of distinct combined feedback entries.
 */
struct ath12k_sa_tx_feedback {
	u32 tx_antenna[SA_MAX_RATE_SERIES];
	u32 rssi[SA_MAX_RSSI_CHAINS];
	u32 rate_mcs[SA_MAX_RATE_SERIES];
	u32 goodput;

	u16 num_packets;
	u16 num_bad;
	u16 nshort_retries[SA_MAX_RETRIES_INDEX];
	u16 nlong_retries[SA_MAX_RETRIES_INDEX];
	u16 ratemaxphy[SA_MAX_PHYRATE_COUNT];

	u8 rate_index;
	u8 is_trainpkt;
	u8 num_comb_feedback;
};

/**
 * struct ath12k_sa_rx_feedback - RX feedback from received frames.
 * @rx_rate_mcs: Received MCS/legacy rate code.
 * @rx_antenna: Received antenna mask.
 * @rx_evm: Pilot EVM values per stream.
 * @rx_rssi: RSSI per chain per bandwidth for the received packet.
 * @npackets: Number of successfully received packets (aggregate or non-aggregate).
 * @rx_rate_index: Index of the received rate.
 */
struct ath12k_sa_rx_feedback {
	u32 rx_rate_mcs;
	u32 rx_antenna;
	u32 rx_evm[SA_MAX_EVM_SIZE];
	s8 rx_rssi[SA_MAX_RSSI_CHAINS][SA_MAX_BW];
	u16 npackets;
	u8 rx_rate_index;
};

/**
 * struct ath12k_sa_config_params - Smart Antenna Configuraton parameters
 * @radio_id: Radio ID
 * @param_type: 0 - radio config param; 1 - node config param
 * @param_id: custom configuared parameter id
 * @param_value: custom configured parameter value
 * @ccp: node specific configuration, for radio config params set to NULL
 */
struct ath12k_sa_config_params {
	u8  radio_id;
	u32 param_type;
	u32 param_id;
	u32 param_value;
	void *ccp;
};

/**
 * struct ath12k_smart_ant_ops - Smart Antenna algorithm hooks.
 * @sa_algo_init: Initialize Smart Antenna algorithm for a radio.
 * @sa_algo_deinit: De-initialize Smart Antenna algorithm for a radio.
 * @sa_peer_connect: Notify algorithm about a peer connect; returns per-peer context in @ccp.
 * @sa_peer_disconnect: Notify algorithm about a peer disconnect; frees per-peer context.
 * @sa_update_txfeedback: Provide TX feedback to algorithm; sets @status on processing outcome.
 * @sa_update_rxfeedback: Provide RX feedback to algorithm; sets @status on processing outcome.
 * @sa_get_txantenna: Get per-rate-series TX antenna selection for a peer.
 * @sa_get_txdefaultantenna: Get radio-wide default TX antenna.
 * @sa_get_rxantenna: Get radio-wide RX antenna selection.
 * @sa_get_traininginfo: Get training rate and antenna arrays and number of training packets.
 * @sa_get_bcn_txantenna: Get TX antenna selection for beacon frames.
 * @sa_set_param: Set a configuration parameter (radio or node specific).
 * @sa_get_param: Get a configuration parameter (radio or node specific).
 * @sa_get_node_config: Get per-node smart antenna configuration by ID.
 * @sa_get_ext_traininginfo: Get extended training parameters (minimum packets, PER threshold).
 *    The 4th parameter can carry flags to disable latency traffic and enable QoS NULL frames
 *    in training.
 * @sa_get_gpio_config: Get GPIO configuration for smart antenna enablement
 * @sa_get_enable_rts_ant: Get separate RTS antenna enable flag
 * @sa_get_max_mpdu_count_for_training: Get Max MPDU count configured for training
 * @sa_get_mcs_margin: Get mcs margin enable/disbale along with margin value
 */
struct ath12k_smart_ant_ops {
	int (*sa_algo_init)(struct ath12k_sa_config *sa_config, int new_init);
	int (*sa_algo_deinit)(u8 radio_id);
	int (*sa_peer_connect)(void **ccp, struct ath12k_sa_node_info *node_info);
	int (*sa_peer_disconnect)(void **ccp);
	int (*sa_update_txfeedback)(void *ccp, struct ath12k_sa_tx_feedback *fb, u32 *status);
	int (*sa_update_rxfeedback)(void *ccp, struct ath12k_sa_rx_feedback *fb, u32 *status);
	int (*sa_get_txantenna)(void *ccp, u32 *antenna_array);
	int (*sa_get_txdefaultantenna)(u8 radio_id, u32 *antenna);
	int (*sa_get_rxantenna)(u8 radio_id, u32 *rx_antenna);
	int (*sa_get_traininginfo)(void *ccp, u32 *rate_array, u32 *antenna_array, u32 *numpkts);
	int (*sa_get_bcn_txantenna)(u8 radio_id, u32 *bcn_txantenna);
	int (*sa_set_param)(struct ath12k_sa_config_params *params);
	int (*sa_get_param)(struct ath12k_sa_config_params *params);
	int (*sa_get_node_config)(void *ccp, u32 id, u32 *config);
	int (*sa_get_ext_traininginfo)(void *ccp, u16 *minpkts, u16 *per_th, u32 *extn_flags);
	int (*sa_get_gpio_config)(u8 radio_id, struct ath12k_sa_gpio_config *gpio_cfg);
	int (*sa_get_enable_rts_ant)(u8 radio_id, u32 *en_rts);
	int (*sa_get_max_mpdu_count_for_training)(u8 radio_id, u32 *value);
	int (*sa_get_mcs_margin)(u8 radio_id, u32 *value);
};

#endif /* ATH12K_SA_API_DEFS_H */
