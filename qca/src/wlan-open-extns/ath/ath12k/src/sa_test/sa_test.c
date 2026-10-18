/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include "sa_test.h"

/*
 * These extern declarations are intentional: sa_test is a standalone
 * loadable module and must not include ath12k's internal headers.
 * The symbols are provided by the ath12k module at runtime.
 */
extern int ath12k_register_smart_ant_ops(struct ath12k_smart_ant_ops *ops);
extern int ath12k_unregister_smart_ant_ops(void);

struct sa_global g_sa;
struct sa_test_train_params g_train_params;
u8 sa_enable_test_mode = 1;
/* Bind ops and module entry points kept at the end for clarity */
static struct ath12k_smart_ant_ops sa_ops;

static u8 sa_get_antenna_combinations(u8 chains)
{
	u8 i = 0;
	u8 result = 1;

	for (i = 0; i < chains; i++)
		result *= (SA_ANTENNAS_PER_CHAIN);

	return result;
}

static int sa_init_radio_params(struct smartantenna_params *sa_params,
				struct ath12k_sa_config *sa_config)
{
	u8 maxchains = sa_config->nss;

	/* Both radios uses same default config params
	 * init the default values */
	sa_params->lower_bound = SA_PER_LOWER_BOUND;
	sa_params->upper_bound = SA_PER_UPPER_BOUND;
	sa_params->per_diff_threshold = SA_PERDIFF_THRESHOLD;
	sa_params->config = 0;
	sa_params->training_mode = SA_TRAINING_MODE_EXISTING;
	sa_params->packetlen = SA_DEFAULT_PKT_LEN;
	sa_params->n_packets_to_train = 0;
	sa_params->max_throughput_change = SA_DEFAULT_TP_THRESHOLD;
	sa_params->retrain_interval =
		SA_DEFAULT_RETRAIN_INTERVAL * SA_NUM_MILLISECONDS_IN_MINUTE;
	sa_params->perftrain_interval = SA_PERF_TRAIN_INTERVEL;
	sa_params->num_pkt_threshold = SA_DEFAULT_MIN_TP;
	sa_params->hysteresis = SA_DEFALUT_HYSTERISYS;
	sa_params->min_goodput_threshold = MIN_GOODPUT_THRESHOLD;
	sa_params->goodput_avg_interval = SA_GPUT_AVERAGE_INTERVAL;
	sa_params->num_pretrain_packets = SA_MAX_PRETRAIN_PACKETS;
	sa_params->num_other_bw_pkts_th = SA_N_BW_PPDU_THRESHOLD;
	sa_params->goodput_ignore_interval = SA_GPUT_IGNORE_INTERVAL;
	if (sa_enable_test_mode)
		sa_params->train_enable = 0;
	else
		sa_params->train_enable =
				SA_INIT_TRAIN_EN | SA_PERIOD_TRAIN_EN |
				SA_PERF_TRAIN_EN | SA_RX_TRAIN_EN;
	sa_params->num_min_pkt_threshod[SA_BW_20] = SA_NUM_PKT_THRESHOLD_20;
	sa_params->num_min_pkt_threshod[SA_BW_40] = SA_NUM_PKT_THRESHOLD_40;
	sa_params->num_min_pkt_threshod[SA_BW_80] = SA_NUM_PKT_THRESHOLD_80;
	sa_params->num_min_pkt_threshod[SA_BW_160] = SA_NUM_PKT_THRESHOLD_160;
	sa_params->num_min_pkt_threshod[SA_BW_320] = SA_NUM_PKT_THRESHOLD_320;
	sa_params->max_train_ppdu = SA_NUM_MAX_PPDU;
	sa_params->default_tx_antenna = SA_DEFAULT_ANTENNA;
	sa_params->num_tx_antennas = sa_get_antenna_combinations(maxchains);
	sa_params->num_recv_chains = maxchains;
	if (maxchains > SA_MAX_RECV_CHAINS) {
		sa_params->num_recv_chains = SA_MAX_RECV_CHAINS;
		SA_DEBUG_PRINT(SA_DEBUG_LVL1, sa_config->radio_id,
				"%s ERROR fatal, unsupported receive chains %d\n",
				__func__, maxchains);
	}
	sa_params->en_rts_ant = 0;
	sa_params->max_mpdu_train_cnt = 0;
	sa_params->mcs_margin_en = 0;
	sa_params->mcs_margin_val = 0;

	return SA_PARALLEL_MODE;
}

/* GPIO pins for Smart Antenna is specific to board design.
 * User can create own mechanism of feeding these GPIOs.
 * During Radio Init, driver requests to get GPIOs. That can be
 * packed and sent back in response.
 */
static void sa_set_gpio_config(struct ath12k_sa_gpio_config *cfg)
{
	cfg->gpio_pin[0] = 0;
	cfg->gpio_pin[1] = 0;
	cfg->gpio_pin[2] = 0;
	cfg->gpio_pin[3] = 0;
	cfg->gpio_func[0] = 0;
	cfg->gpio_func[1] = 0;
	cfg->gpio_func[2] = 0;
	cfg->gpio_func[3] = 0;
}

static void sa_reset_global_training_param(void)
{
	u8 i;

	for (i = 0; i < SA_NUM_BW; i++)
		g_train_params.ratecode[i] = 0;

	g_train_params.antenna = 0;
	g_train_params.numpkts = 0;
	g_train_params.minpkts = 0xffff;
	g_train_params.per_threshold = 0xffff;
}

static int sa_test_init(struct ath12k_sa_config *cfg, int new_init)
{
	struct radio_config *radio;

	if (!cfg) {
		SA_DEBUG_PRINT(0, 0, "Invalid cfg pointer!");
		return -1;
	}
	if (cfg->radio_id >= SA_MAX_RADIOS) {
		SA_DEBUG_PRINT(0, 0, "Invalid Radio ID %u",
			       cfg->radio_id);
		return -1;
	}

	radio = &g_sa.radio[cfg->radio_id];
	if (new_init) {
		memset(radio, 0, sizeof(struct radio_config));
		sa_init_radio_params(&radio->sa_params, cfg);
		radio->init_done = 1;
		radio->sa_dbg_level = SA_DEBUG_LVL1;
		radio->default_antenna = SA_DEFAULT_ANTENNA;
		radio->bcn_antenna = SA_DEFAULT_ANTENNA;
		cfg->txrx_feedback = SA_TX_FEEDBACK_MASK | SA_RX_FEEDBACK_MASK;
		sa_set_gpio_config(&radio->gpio_cfg);
		sa_reset_global_training_param();
	}

	memcpy(&radio->init_config, cfg, sizeof(struct ath12k_sa_config));

	SA_DEBUG_PRINT(SA_DEBUG_LVL1, cfg->radio_id, " %s: RadioId: %d channel:"
		" %d band: %d bss mode: %d nss: %d chainmask: 0x%02x\n",
		(new_init ? "New Init":"Re Init"), cfg->radio_id, cfg->chan_num,
		cfg->band, cfg->bss_mode, cfg->nss, cfg->txrx_chainmask);


	return SA_PARALLEL_MODE;
}

static int sa_test_deinit(u8 radio_id)
{
	struct radio_config *radio;

	if (radio_id >= SA_MAX_RADIOS) {
		SA_DEBUG_PRINT(0, 0, "Invalid Radio ID %u", radio_id);
		return -1;
	}
	radio = &g_sa.radio[radio_id];

	radio->init_done = 0;
	SA_DEBUG_PRINT(SA_DEBUG_LVL1, radio_id, " RadioId: %d\n", radio_id);

	return 0;
}

static void sa_node_init(struct sa_node_ccp *node)
{
	struct radio_config *radio =  &g_sa.radio[node->node_info.radio_id];

	node->train_info.selected_antenna = radio->sa_params.default_tx_antenna;
	node->train_info.previous_selected_antenna =
					radio->sa_params.default_tx_antenna;
	node->train_info.training_start = 0;
	node->train_info.last_train_bw = SA_BW_20;
	node->train_info.tx_feedback_config = SA_TX_FEEDBACK_CONFIG_DEFAULT;
	node->train_info.smartantenna_state = SMARTANTENNA_DEFAULT;

}

#define MAX_LOG_LEN 800
static void sa_print_ratecode(struct sa_node_ccp *node, u8 bw, bool legacy)
{
	u8 i, idx;
	u16 *ratecode = 0, index = 0;
	u8 log_buff[MAX_LOG_LEN] = {0};
	struct ath12k_sa_node_info *node_info;

	if (!node)
		return;

	node_info = &node->node_info;
	if (!legacy && (bw > node_info->max_ch_bw))
		return;

	if (legacy) {
		idx = SA_RATECOUNT_IDX_LEGACY;
		ratecode = &node_info->rate_cap.rc_legacy[0];
		SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id,
			" Legacy Rates: Count: %d: ", node_info->rate_cap.num_rc[idx]);
	} else {
		switch (bw) {
		case SA_BW_20:
			idx = SA_RATECOUNT_IDX_20;
			ratecode = &node_info->rate_cap.rc_20[0];
			SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id,
				" 20 MHz Ratecode Count: %d: ",
				node_info->rate_cap.num_rc[idx]);
			break;
		case SA_BW_40:
			idx = SA_RATECOUNT_IDX_40;
			ratecode = &node_info->rate_cap.rc_40[0];
			SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id,
				" 40 MHz Ratecode Count: %d: ",
				node_info->rate_cap.num_rc[idx]);
			break;
		case SA_BW_80:
			idx = SA_RATECOUNT_IDX_80;
			ratecode = &node_info->rate_cap.rc_80[0];
			SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id,
				" 80 MHz Ratecode Count: %d: ",
				node_info->rate_cap.num_rc[idx]);
			break;
		case SA_BW_160:
			idx = SA_RATECOUNT_IDX_160;
			ratecode = &node_info->rate_cap.rc_160[0];
			SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id,
				" 160 MHz Ratecode Count: %d: ",
				node_info->rate_cap.num_rc[idx]);
			break;
		case SA_BW_320:
			idx = SA_RATECOUNT_IDX_320;
			ratecode = &node_info->rate_cap.rc_320[0];
			SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id,
				" 320 MHz Ratecode Count: %d: ",
				node_info->rate_cap.num_rc[idx]);
			break;
		default:
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, node_info->radio_id,
				"Invalid BW %d!\n", bw);
			return;
		};
	}

	for (i = 0; i < node_info->rate_cap.num_rc[idx]; i++) {
		index += snprintf(&log_buff[index], MAX_LOG_LEN - index,
				"0x%x, ", ratecode[i]);
		if (i && !(i % 8))
			index += snprintf(&log_buff[index], MAX_LOG_LEN - index, "\n");
	}
	SA_DEBUG_PRINT(SA_DEBUG_LVL2, node_info->radio_id, "%s\n", log_buff);
}

static int sa_test_peer_connect(void **ccp, struct ath12k_sa_node_info *node_info)
{
	struct sa_node_ccp *node = NULL;
	struct radio_config *radio;
	u8 i;

	if (!node_info)
		return -1;

	if (!*ccp) {
		/* Called from IRQ context, hence Atomic */
		node = kzalloc(sizeof(*node), GFP_ATOMIC);
		*ccp = node;
		if (!node)
			return -1;
	} else {
		node = *ccp;
		SA_DEBUG_PRINT(SA_DEBUG_LVL1, node->node_info.radio_id,
			       "Called with non NULL ccp\n");
	}
	if (node_info->radio_id >= SA_MAX_RADIOS) {
		kfree(node);
		*ccp = NULL;
		return -EINVAL;
	}
	memcpy(&node->node_info, node_info, sizeof(*node_info));
	radio = &g_sa.radio[node->node_info.radio_id];

	sa_node_init(node);

	radio->nodes_connected++;
	if (node->train_info.previous_selected_antenna < SA_ANTENNA_COMBINATIONS)
		radio->node_count_per_ant[node->train_info.previous_selected_antenna]++;
	SA_DEBUG_PRINT(SA_DEBUG_LVL2, node->node_info.radio_id,
		"Mac: %pM RadioId: %d max bw: %d NSS(TX,RX): (%d,%d) opmode: %d "
		"channel: %d band: %d\n", node_info->mac_addr, node_info->radio_id,
		node_info->max_ch_bw, node_info->nss_tx, node_info->nss_rx,
		node_info->opmode, node_info->chan_num, node_info->band);
	sa_print_ratecode(node, 0, true);
	for (i = 0; i <= node_info->max_ch_bw; i++)
		sa_print_ratecode(node, i, false);

	return 0;
}

static int sa_test_peer_disconnect(void **ccp)
{
	struct sa_node_ccp *node;
	struct radio_config *radio;

	if (!ccp || !*ccp)
		return -1;
	node = *ccp;

	radio = &g_sa.radio[node->node_info.radio_id];
	if (radio->nodes_connected > 0)
		radio->nodes_connected--;
	if ((node->train_info.previous_selected_antenna < SA_ANTENNA_COMBINATIONS) &&
	    (radio->node_count_per_ant[node->train_info.previous_selected_antenna] > 0))
		radio->node_count_per_ant[node->train_info.previous_selected_antenna]--;
	SA_DEBUG_PRINT(SA_DEBUG_LVL2, node->node_info.radio_id,
		       "Mac: %pM RadioId: %d channel: %d band: %d\n",
		       node->node_info.mac_addr, node->node_info.radio_id,
		       node->node_info.chan_num, node->node_info.band);

	kfree(node);
	*ccp = NULL;

	return 0;
}

static int sa_test_update_txfeedback(void *ccp,
				     struct ath12k_sa_tx_feedback *fb,
				     u32 *status)
{
	struct sa_node_ccp *node = ccp;

	if (!node || !fb || !status)
		return -1;

	SA_DEBUG_PRINT(SA_DEBUG_LVL4, node->node_info.radio_id,
		       "SA Test: Mac: %pM nFrame:%d nBad:%d Rate:[0x%x] "
		       "RateIdx:%d TxAntenna:0x%06x RSSI:[%d %d %d %d %d] "
		       "Ratemaxphy:[0x%03x|0x%03x|0x%03x|0x%03x|0x%03x] "
		       "SR:[%d %d %d %d %d %d] LR:[%d %d %d %d %d %d] "
		       "Trainpkt:%d Goodput:%d\n", node->node_info.mac_addr,
		       fb->num_packets, fb->num_bad, fb->rate_mcs[0],
		       fb->rate_index, fb->tx_antenna[0], fb->rssi[0], fb->rssi[1],
		       fb->rssi[2], fb->rssi[3], fb->rssi[4], fb->ratemaxphy[0],
		       fb->ratemaxphy[1], fb->ratemaxphy[2], fb->ratemaxphy[3],
		       fb->ratemaxphy[4], fb->nshort_retries[0],
		       fb->nshort_retries[1], fb->nshort_retries[2],
		       fb->nshort_retries[3], fb->nshort_retries[4],
		       fb->nshort_retries[5], fb->nlong_retries[0],
		       fb->nlong_retries[1], fb->nlong_retries[2],
		       fb->nlong_retries[3], fb->nlong_retries[4],
		       fb->nlong_retries[5], fb->is_trainpkt, fb->goodput);

	return 0;
}

static int sa_test_update_rxfeedback(void *ccp,
				     struct ath12k_sa_rx_feedback *fb,
				     u32 *status)
{
	struct sa_node_ccp *node = ccp;
	u8 chain, bw;

	if (!node || !fb || !status)
		return -1;

	SA_DEBUG_PRINT(SA_DEBUG_LVL5, node->node_info.radio_id,
		       "SA Test: Mac: %pM Rate:0x%x RateIdx:%d Antenna:0x%06x npakcets:%d",
		       node->node_info.mac_addr, fb->rx_rate_mcs,
		       fb->rx_rate_index, fb->rx_antenna, fb->npackets);

	SA_DEBUG_PRINT(SA_DEBUG_LVL6, node->node_info.radio_id,
		       "Rssi   bw0 bw1 bw2 bw3");
	for (chain = 0; chain < SA_MAX_RSSI_CHAINS; chain++) {
		for (bw = 0; bw < SA_MAX_BW; bw++) {
			SA_DEBUG_PRINT(SA_DEBUG_LVL6, node->node_info.radio_id,
				       "\tChain%d [%d][%d][%d][%d]", chain,
				       fb->rx_rssi[chain][bw],
				       fb->rx_rssi[chain][bw + 1],
				       fb->rx_rssi[chain][bw + 2],
				       fb->rx_rssi[chain][bw + 3]);
			bw += 3;
		}
	}

	return 0;
}

static int sa_test_get_txantenna(void *ccp, u32 *antenna_array)
{
	int i;
	struct sa_node_ccp *node = ccp;
	struct sa_node_train_info *train_info;

	if (!node || !antenna_array)
		return -1;

	train_info = &node->train_info;

	for (i = 0; i < SA_MAX_SA_CHAINS; i++)
		antenna_array[i] = train_info->selected_antenna;

	return 0;
}

static int sa_test_get_txdefaultantenna(u8 radio_id, u32 *antenna)
{
	struct radio_config *radio;

	if (!antenna || radio_id >= SA_MAX_RADIOS)
		return -1;

	radio = &g_sa.radio[radio_id];

	if (radio->init_done)
		*antenna = radio->sa_params.default_tx_antenna;
	else
		*antenna = radio->default_antenna;

	return 0;
}

static int sa_test_get_rxantenna(u8 radio_id, u32 *rx_antenna)
{
	if (!rx_antenna || radio_id >= SA_MAX_RADIOS)
		return -1;

	*rx_antenna = g_sa.radio[radio_id].default_antenna;

	return 0;
}

static int sa_test_get_traininginfo(void *ccp, u32 *rate_array, u32 *antenna_array, u32 *numpkts)
{
	u8 i;

	if (!rate_array || !antenna_array || !numpkts)
		return -1;

	/* For 2 serieses of rate */
	rate_array[0] = (g_train_params.ratecode[0] |
			g_train_params.ratecode[1] << 16);
	rate_array[1] = (g_train_params.ratecode[2] |
			g_train_params.ratecode[3] << 16);
	rate_array[2] = (g_train_params.ratecode[4] & SA_WORD_MASK);
	rate_array[3] = (g_train_params.ratecode[0] |
			g_train_params.ratecode[1] << 16);
	rate_array[4] = (g_train_params.ratecode[2] |
			g_train_params.ratecode[3] << 16);
	rate_array[5] = (g_train_params.ratecode[4] & SA_WORD_MASK);

	/* For all chains same Antenna */
	for (i = 0; i < SA_MAX_SA_CHAINS; i++)
		antenna_array[i] = g_train_params.antenna;

	*numpkts = g_train_params.numpkts;

	return 0;
}

static int sa_test_get_bcn_txantenna(u8 radio_id, u32 *bcn_txantenna)
{
	if (!bcn_txantenna || radio_id >= SA_MAX_RADIOS)
		return -1;

	*bcn_txantenna = g_sa.radio[radio_id].bcn_antenna;

	return 0;
}

static void display_help(void)
{
	pr_info("    ParamName                   ParamId          Description");
	pr_info(" SMART_ANT_PARAM_HELP                    %d    "
		"Displays current available commands list",
		SMART_ANT_PARAM_HELP);
	pr_info(" SMART_ANT_PARAM_TRAIN_MODE              %d    "
		"Training mode self packet generation or existing traffic mode",
		SMART_ANT_PARAM_TRAIN_MODE);
	pr_info(" SMART_ANT_PARAM_TRAIN_PER_THRESHOLD     %d    "
		"Smart antenna lower, upper and per diff thresholds",
		SMART_ANT_PARAM_TRAIN_PER_THRESHOLD);
	pr_info(" SMART_ANT_PARAM_PKT_LEN                 %d    "
		"Packet length of the training packet",
		SMART_ANT_PARAM_PKT_LEN);
	pr_info(" SMART_ANT_PARAM_NUM_PKTS                %d    "
		"Number of packets requied for training",
		SMART_ANT_PARAM_NUM_PKTS);
	pr_info(" SMART_ANT_PARAM_TRAIN_START             %d    "
		"Training init for a particualr node",
		SMART_ANT_PARAM_TRAIN_START);
	pr_info(" SMART_ANT_PARAM_TRAIN_ENABLE            %d    "
		"Enable bitmap for init,periodic & performance triggers",
		SMART_ANT_PARAM_TRAIN_ENABLE);
	pr_info(" SMART_ANT_PARAM_PERFTRAIN_INTERVAL      %d    "
		"Performance monitoring interval",
		SMART_ANT_PARAM_PERFTRAIN_INTERVAL);
	pr_info(" SMART_ANT_PARAM_RETRAIN_INTERVAL        %d    "
		"Periodic retrain interval", SMART_ANT_PARAM_RETRAIN_INTERVAL);
	pr_info(" SMART_ANT_PARAM_PERFTRAIN_THRESHOLD     %d    "
		"Percentage change in goodput to trigger performance training",
		SMART_ANT_PARAM_PERFTRAIN_THRESHOLD);
	pr_info(" SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD   %d    "
		"Minimum Good put threshold to trigger performance training",
		SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD);
	pr_info(" SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL    %d    "
		"Goodput avg interval", SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL);
	pr_info(" SMART_ANT_PARAM_DEFAULT_ANTENNA         %d    "
		"Default antenna for RX", SMART_ANT_PARAM_DEFAULT_ANTENNA);
	pr_info(" SMART_ANT_PARAM_DEFAULT_TX_ANTENNA      %d    "
		"Default tx antenna when node connects",
		SMART_ANT_PARAM_DEFAULT_TX_ANTENNA);
	pr_info(" SMART_ANT_PARAM_TX_ANTENNA              %d    "
		"Selected antenna for TX", SMART_ANT_PARAM_TX_ANTENNA);
	pr_info(" SMART_ANT_PARAM_DBG_LEVEL               %d    "
		"Debug level for each radio", SMART_ANT_PARAM_DBG_LEVEL);
	pr_info(" SMART_ANT_PARAM_PRETRAIN_PKTS           %d    "
		"Number of pre train packets", SMART_ANT_PARAM_PRETRAIN_PKTS);
	pr_info(" SMART_ANT_PARAM_OTHER_BW_PKTS_TH        %d    "
		"Threshold for other bw packets",
		SMART_ANT_PARAM_OTHER_BW_PKTS_TH);
	pr_info(" SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL %d    "
		"Goodput ignore interval",
		SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL);
	pr_info(" SMART_ANT_PARAM_MIN_PKT_TH_BW20         %d    "
		"Min num packets in 20 MHz to indicate active bw",
		SMART_ANT_PARAM_MIN_PKT_TH_BW20);
	pr_info(" SMART_ANT_PARAM_MIN_PKT_TH_BW40         %d    "
		"Min num packets in 40 MHz to indicate active bw",
		SMART_ANT_PARAM_MIN_PKT_TH_BW40);
	pr_info(" SMART_ANT_PARAM_MIN_PKT_TH_BW80         %d    "
		"Min num packets in 80 MHz to indicate active bw",
		SMART_ANT_PARAM_MIN_PKT_TH_BW80);
	pr_info(" SMART_ANT_PARAM_MIN_PKT_TH_BW160         %d    "
		"Min num packets in 160 MHz to indicate active bw",
		SMART_ANT_PARAM_MIN_PKT_TH_BW160);
	pr_info(" SMART_ANT_PARAM_MIN_PKT_TH_BW320         %d    "
		"Min num packets in 320 MHz to indicate active bw",
		SMART_ANT_PARAM_MIN_PKT_TH_BW320);
	pr_info(" SMART_ANT_PARAM_DEBUG_INFO              %d    "
		"Displays node debug info", SMART_ANT_PARAM_DEBUG_INFO);
	pr_info(" SMART_ANT_PARAM_MAX_TRAIN_PPDU          %d    "
		"Max number of train ppdus", SMART_ANT_PARAM_MAX_TRAIN_PPDU);
	pr_info(" SMART_ANT_PARAM_PERF_HYSTERESIS         %d    "
		"Hysteresis for performance based trigger",
		SMART_ANT_PARAM_PERF_HYSTERESIS);
	pr_info(" SMART_ANT_PARAM_TX_FEEDBACK_CONFIG      %d    "
		"Tx Feedback config", SMART_ANT_PARAM_TX_FEEDBACK_CONFIG);
	pr_info(" SMART_ANT_PARAM_BCN_ANTENNA             %d    "
		"Beacon Antenna", SMART_ANT_PARAM_BCN_ANTENNA);
	pr_info(" SMART_ANT_PARAM_PARALLEL_MODE_GPIO_DRIVE %d   "
		"GPIO Drive config in parallel mode",
		SMART_ANT_PARAM_PARALLEL_MODE_GPIO_DRIVE);
	if (sa_enable_test_mode) {
		pr_info(" SMART_ANT_PARAM_TRAIN_RATE_TESTMODE     %d    "
			"Training Rate Code in test mode",
			SMART_ANT_PARAM_TRAIN_RATE_TESTMODE);
		pr_info(" SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE  %d    "
			"Training Antenna in test mode",
			SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE);
		pr_info(" SMART_ANT_PARAM_TRAIN_PACKETS_TESTMODE  %d    "
			"Number of training packets in test mode",
			SMART_ANT_PARAM_TRAIN_PACKETS_TESTMODE);
		pr_info(" SMART_ANT_PARAM_TRAIN_MINPKTS_TESTMODE  %d    "
			"Number of minimum packets to be trained in test mode",
			SMART_ANT_PARAM_TRAIN_MINPKTS_TESTMODE);
		pr_info(" SMART_ANT_PARAM_TRAIN_PERTH_TESTMODE    %d    "
			"PER threshold to stop training in test mode post min pkts",
			SMART_ANT_PARAM_TRAIN_PERTH_TESTMODE);
		pr_info(" SMART_ANT_PARAM_TRAIN_START_TESTMODE    %d    "
			"Training start in test mode",
			SMART_ANT_PARAM_TRAIN_START_TESTMODE);
		pr_info(" SMART_ANT_PARAM_TRAIN_EXTN_FLAGS_TESTMODE    %d    "
			"Enable extended traing flags (like Disable Latency Traffic 0x%x, Enable QoS NULL frames 0x%x",
			SMART_ANT_PARAM_TRAIN_EXTN_FLAGS_TESTMODE,
			SA_LATENCY_TRAFFIC_DISABLE, SA_QOS_NULL_ENABLE);
	}
	pr_info(" SMART_ANT_PARAM_MAX_MPDU_COUNT_FOR_TARINING    %d    "
		"MPDU count per training",
		SMART_ANT_PARAM_MAX_MPDU_COUNT_FOR_TARINING);
	pr_info(" SMART_ANT_PARAM_MCS_MARGIN    %d    "
		"Enable MCS margin value", SMART_ANT_PARAM_MCS_MARGIN);
	pr_info("\n");
}

static int sa_test_set_param(struct ath12k_sa_config_params *params)
{
	struct smartantenna_params *sa_params;
	struct radio_config *radio;
	struct sa_node_ccp *node;
	int retval = 0;
	u8 rate_index = 0;
	u8 i;

	if (!params)
		return -1;

	if (params->radio_id >= SA_MAX_RADIOS)
		return -1;

	radio = &g_sa.radio[params->radio_id];
	if (!radio->init_done)
		return -1;

	SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
		       "SA Test: set_param type=%u id=%u val=0x%x\n",
		       params->param_type, params->param_id, params->param_value);

	sa_params = &radio->sa_params;

	switch (params->param_id) {
	case SMART_ANT_PARAM_HELP:
		display_help();
		break;
	case SMART_ANT_PARAM_TRAIN_MODE:
		sa_params->training_mode = params->param_value;
		break;
	case SMART_ANT_PARAM_TRAIN_PER_THRESHOLD:
		sa_params->lower_bound = (params->param_value) & SA_BYTE_MASK;
		sa_params->upper_bound = (params->param_value >>
					SA_8TH_POSITION) & SA_BYTE_MASK;
		sa_params->per_diff_threshold = (params->param_value >>
					SA_16TH_POSITION) & SA_BYTE_MASK;
		sa_params->config = (params->param_value >> SA_24TH_POSITION) &
							SA_BYTE_MASK;
		break;
	case SMART_ANT_PARAM_PKT_LEN:
		sa_params->packetlen = params->param_value;
		break;
	case SMART_ANT_PARAM_NUM_PKTS:
		sa_params->n_packets_to_train = params->param_value;
		break;
	case SMART_ANT_PARAM_PERFTRAIN_INTERVAL:
		sa_params->perftrain_interval = params->param_value;
		break;
	case SMART_ANT_PARAM_RETRAIN_INTERVAL:
		sa_params->retrain_interval = params->param_value;
		break;
	case SMART_ANT_PARAM_PERFTRAIN_THRESHOLD:
		sa_params->max_throughput_change = params->param_value;
		break;
	case SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD:
		sa_params->min_goodput_threshold = params->param_value;
		break;
	case SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL:
		sa_params->goodput_avg_interval = params->param_value;
		break;
	case SMART_ANT_PARAM_DEFAULT_ANTENNA:
		radio->default_antenna = params->param_value;
		retval |= SA_RX_CONFIG_REQUIRED;
		sa_params->train_enable &= ~(SA_RX_TRAIN_EN);
		break;
	case SMART_ANT_PARAM_DEFAULT_TX_ANTENNA:
		retval |= SA_DEFAULT_TX_CONFIG_REQUIRED;
		sa_params->default_tx_antenna = params->param_value;
		break;
	case SMART_ANT_PARAM_TX_ANTENNA:
		if (sa_params->train_enable & (SA_INIT_TRAIN_EN |
		    SA_PERF_TRAIN_EN | SA_PERIOD_TRAIN_EN)) {
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
				"Fixing Antenna is not permitted when "
				"Smart Antenna algo is enabled %d\n",
				sa_params->train_enable);
			return -1;
		}
		if (params->param_type) {
			node = params->ccp;
			if (!node)
				return -1;
			node->train_info.selected_antenna = params->param_value;
			/*sa_update_ant_map(node,
					  node->train_info.selected_antenna);*/
			node->train_info.antenna_change_ind |=
						SA_TX_CONFIG_REQUIRED;
			retval |= SA_TX_CONFIG_REQUIRED;
		} else {
			if (params->param_value >= SA_ANTENNA_COMBINATIONS)
				return -1;
			sa_params->default_tx_antenna = params->param_value;
			if (radio->nodes_connected ==
			    radio->node_count_per_ant[params->param_value]) {
				sa_params->antenna_change_ind &=
						~SA_TX_CONFIG_REQUIRED;
				SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					"global tx antenna changed to %d\n",
					sa_params->default_tx_antenna);
			} else {
				sa_params->antenna_change_ind |=
						SA_TX_CONFIG_REQUIRED;
			}
		}
		break;
	case SMART_ANT_PARAM_TRAIN_START:
		if (params->param_type && params->ccp) {
			node = params->ccp;
			node->train_info.training_start = params->param_value;
		} else {
			return -1;
		}
		break;
	case SMART_ANT_PARAM_TRAIN_ENABLE:
		if (params->param_type) {
			node = params->ccp;
			if (!node)
				return -1;
			node->train_info.train_enable = params->param_value;
			if (node->train_info.train_enable &
			    (SA_INIT_TRAIN_EN | SA_PERF_TRAIN_EN |
			    SA_PERIOD_TRAIN_EN)) {
				node->train_info.antenna_change_ind &=
							~SA_TX_CONFIG_REQUIRED;
			}
		} else {
			sa_params->train_enable = params->param_value;
			if (sa_params->train_enable & (SA_INIT_TRAIN_EN |
			    SA_PERF_TRAIN_EN | SA_PERIOD_TRAIN_EN)) {
				sa_params->antenna_change_ind &=
							~SA_TX_CONFIG_REQUIRED;
			}
		}
		break;
	case SMART_ANT_PARAM_DBG_LEVEL:
		radio->sa_dbg_level = params->param_value;
		break;
	case SMART_ANT_PARAM_PRETRAIN_PKTS:
		sa_params->num_pretrain_packets = params->param_value;
		break;
	case SMART_ANT_PARAM_OTHER_BW_PKTS_TH:
		sa_params->num_other_bw_pkts_th = params->param_value;
		break;
	case SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL:
		sa_params->goodput_ignore_interval = params->param_value;
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW20:
		sa_params->num_min_pkt_threshod[SA_BW_20] = params->param_value;
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW40:
		sa_params->num_min_pkt_threshod[SA_BW_40] = params->param_value;
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW80:
		sa_params->num_min_pkt_threshod[SA_BW_80] = params->param_value;
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW160:
		sa_params->num_min_pkt_threshod[SA_BW_160] = params->param_value;
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW320:
		sa_params->num_min_pkt_threshod[SA_BW_320] = params->param_value;
		break;
	case SMART_ANT_PARAM_DEBUG_INFO:
		if (params->param_type && params->ccp) {
			node = params->ccp;
			node->train_info.last_training_time = 0;
			node->train_info.prd_trigger_count = 0;
			node->train_info.perf_trigger_count = 0;
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
				"Last training time :%d ,Periodic triggers: %d,"
				"performance triggers: %d\n",
				node->train_info.last_training_time,
				node->train_info.prd_trigger_count,
				node->train_info.perf_trigger_count);
		} else {
			return -1;
		}
		break;
	case SMART_ANT_PARAM_MAX_TRAIN_PPDU:
		sa_params->max_train_ppdu = params->param_value;
		break;
	case SMART_ANT_PARAM_PERF_HYSTERESIS:
		sa_params->hysteresis =  params->param_value;
		break;
	case SMART_ANT_PARAM_TX_FEEDBACK_CONFIG:
		if (params->param_type && params->ccp) {
			node = params->ccp;
			node->train_info.tx_feedback_config =
							params->param_value;
			retval |= SA_TX_FEEDBACK_CONFIG_REQUIRED;
		} else {
			return -1;
		}
		break;
	case SMART_ANT_PARAM_BCN_ANTENNA:
		radio->bcn_antenna = params->param_value;
		retval |= SA_BEACON_ANT_CONFIG_REQUIRED;
		break;
	case SMART_ANT_PARAM_PARALLEL_MODE_GPIO_DRIVE:
		sa_params->parallel_gpio_drive = params->param_value;
		retval |= SA_PARALLEL_GPIO_CFG_REQUIRED;
		break;
	case SMART_ANT_PARAM_TRAIN_RATE_TESTMODE:
		if (sa_enable_test_mode && params->param_type) {
			node = params->ccp;
			if (!node) {
				SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					"Node is null!\n");
				return -1;
			}
			rate_index =
				SA_GET_BW_FROM_RC(params->param_value >> 16);
			if (rate_index >= SA_NUM_BW) {
				SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					"Incorrect Param; RateCode: bit 0-11 "
					"Bandwidth: bit 16-17\n 0 - Legacy\\"
					"20MHz\n1 - 40MHz\n2 - 80MHz\n3 - "
					"160MHz\n4 - 320Mhz\n");
				return -1;
			}
			if (rate_index > node->node_info.max_ch_bw) {
				SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					"Incorrect BW Param; Peer connected in "
					"BW %d", node->node_info.max_ch_bw);
				return -1;
			}
			for (i = 0; i < SA_NUM_BW; i++)
				g_train_params.ratecode[i] = 0;
			g_train_params.ratecode[rate_index] =
					(params->param_value & SA_WORD_MASK);
		} else {
			return -1;
		}
		break;
	case SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE:
		if (sa_enable_test_mode)
			g_train_params.antenna = params->param_value;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_PACKETS_TESTMODE:
		if (sa_enable_test_mode)
			g_train_params.numpkts = params->param_value;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_MINPKTS_TESTMODE:
		if (sa_enable_test_mode)
			g_train_params.minpkts = params->param_value;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_PERTH_TESTMODE:
		if (sa_enable_test_mode)
			g_train_params.per_threshold = params->param_value;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_START_TESTMODE:
		if (sa_enable_test_mode)
			retval |= SA_TRAINING_REQUIRED;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_EXTN_FLAGS_TESTMODE:
		if (sa_enable_test_mode)
			g_train_params.extn_train_flags = params->param_value;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_MAX_MPDU_COUNT_FOR_TARINING:
		if (!(params->param_value & 0x3FF)) {
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					"Incorrect MPDU count; 0x%x",
					params->param_value);
			return -1;
		}
		sa_params->max_mpdu_train_cnt = params->param_value;
		retval |= SA_MPDU_CNT_MCS_MRG_REQUIRED;
		break;
	case SMART_ANT_PARAM_MCS_MARGIN:
		if (!sa_params->mcs_margin_en && !(params->param_value & 0x7)) {
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					"Incorrect margin value; %d",
					params->param_value);
			return -1;
		}
		sa_params->mcs_margin_val = params->param_value & 0x7;
		sa_params->mcs_margin_en = (sa_params->mcs_margin_val? 1 : 0);
		retval |= SA_MPDU_CNT_MCS_MRG_REQUIRED;
		break;
	default:
		display_help();
		return -1;
	}

	return retval;
}

static int sa_test_get_param(struct ath12k_sa_config_params *params)
{
	struct smartantenna_params *sa_params;
	struct radio_config *radio;
	struct sa_node_ccp *node;
	u32 value = 0;
	u8 i;

	if (!params)
		return -1;

	if (params->radio_id >= SA_MAX_RADIOS)
		return -1;

	SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
		       "SA Test: get_param type=%u id=%u\n",
		       params->param_type, params->param_id);

	radio = &g_sa.radio[params->radio_id];
	if (!radio->init_done)
		return -1;

	sa_params = &radio->sa_params;

	switch (params->param_id) {
	case SMART_ANT_PARAM_HELP:
		display_help();
		break;
	case SMART_ANT_PARAM_TRAIN_MODE:
		params->param_value = sa_params->training_mode;
		break;
	case SMART_ANT_PARAM_TRAIN_PER_THRESHOLD:
		value = 0;
		value |= (sa_params->lower_bound & SA_BYTE_MASK);
		value |= ((sa_params->upper_bound & SA_BYTE_MASK) <<
							SA_8TH_POSITION);
		value |= ((sa_params->per_diff_threshold & SA_BYTE_MASK) <<
							SA_16TH_POSITION);
		value |= ((sa_params->config & SA_BYTE_MASK) <<
							SA_24TH_POSITION);
		params->param_value = value;
		break;
	case SMART_ANT_PARAM_PKT_LEN:
		params->param_value = sa_params->packetlen;
		break;
	case SMART_ANT_PARAM_NUM_PKTS:
		params->param_value = sa_params->n_packets_to_train;
		break;
	case SMART_ANT_PARAM_PERFTRAIN_INTERVAL:
		params->param_value = sa_params->perftrain_interval;
		break;
	case SMART_ANT_PARAM_RETRAIN_INTERVAL:
		params->param_value = sa_params->retrain_interval;
		break;
	case SMART_ANT_PARAM_PERFTRAIN_THRESHOLD:
		params->param_value = sa_params->max_throughput_change;
		break;
	case SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD:
		params->param_value = sa_params->min_goodput_threshold;
		break;
	case SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL:
		params->param_value = sa_params->goodput_avg_interval;
		break;
	case SMART_ANT_PARAM_DEFAULT_ANTENNA:
		params->param_value = radio->default_antenna;
		break;
	case SMART_ANT_PARAM_DEFAULT_TX_ANTENNA:
		params->param_value = sa_params->default_tx_antenna;
		break;
	case SMART_ANT_PARAM_TX_ANTENNA:
		if (params->param_type) {
			node = params->ccp;
			if (!node)
				return -1;
			params->param_value = node->train_info.selected_antenna;
		} else {
			params->param_value = sa_params->default_tx_antenna;
		}
		break;
	case SMART_ANT_PARAM_TRAIN_START:
		if (params->param_type && params->ccp) {
			node = params->ccp;
			params->param_value = node->train_info.training_start;
		} else {
			return -1;
		}
		break;
	case SMART_ANT_PARAM_TRAIN_ENABLE:
		if (params->param_type) {
			node = params->ccp;
			if (!node)
				return -1;
			params->param_value = node->train_info.train_enable;
		} else {
			params->param_value = sa_params->train_enable;
		}
		break;
	case SMART_ANT_PARAM_DBG_LEVEL:
		params->param_value = radio->sa_dbg_level;
		break;
	case SMART_ANT_PARAM_PRETRAIN_PKTS:
		params->param_value = sa_params->num_pretrain_packets;
		break;
	case SMART_ANT_PARAM_OTHER_BW_PKTS_TH:
		params->param_value = sa_params->num_other_bw_pkts_th;
		break;
	case SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL:
		params->param_value = sa_params->goodput_ignore_interval;
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW20:
		params->param_value = sa_params->num_min_pkt_threshod[SA_BW_20];
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW40:
		params->param_value = sa_params->num_min_pkt_threshod[SA_BW_40];
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW80:
		params->param_value = sa_params->num_min_pkt_threshod[SA_BW_80];
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW160:
		params->param_value =
				sa_params->num_min_pkt_threshod[SA_BW_160];
		break;
	case SMART_ANT_PARAM_MIN_PKT_TH_BW320:
		params->param_value =
				sa_params->num_min_pkt_threshod[SA_BW_320];
		break;
	case SMART_ANT_PARAM_DEBUG_INFO:
		if (params->param_type && params->ccp) {
			node = params->ccp;
			params->param_value = node->train_info.train_enable;
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
				       "Last training time: %d, Periodic "
				       "triggers: %d, performance triggers: "
				       "%d\n",
				       node->train_info.last_training_time,
				       node->train_info.prd_trigger_count,
				       node->train_info.perf_trigger_count);
		} else {
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
				       "%s: nodes connected %d\n", __func__,
				       radio->nodes_connected);
			if (sa_params->num_tx_antennas > SA_ANTENNA_COMBINATIONS)
				return -1;
			for (i = 0; i < sa_params->num_tx_antennas; i++)
				SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
					      "    ant %d, nodes %d\n", i,
					      radio->node_count_per_ant[i]);
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
				       "antenna change ind %d\n",
				       sa_params->antenna_change_ind);
		}
		break;
	case SMART_ANT_PARAM_MAX_TRAIN_PPDU:
		params->param_value = sa_params->max_train_ppdu;
		break;
	case SMART_ANT_PARAM_PERF_HYSTERESIS:
		params->param_value = sa_params->hysteresis;
		break;
	case SMART_ANT_PARAM_TX_FEEDBACK_CONFIG:
		if (params->param_type && params->ccp) {
			node = params->ccp;
			params->param_value =
					node->train_info.tx_feedback_config;
		} else {
			return -1;
		}
		break;
	case SMART_ANT_PARAM_BCN_ANTENNA:
		params->param_value = radio->bcn_antenna;
		break;
	case SMART_ANT_PARAM_PARALLEL_MODE_GPIO_DRIVE:
		params->param_value = sa_params->parallel_gpio_drive;
		break;
	case SMART_ANT_PARAM_TRAIN_RATE_TESTMODE:
		if (sa_enable_test_mode)
			SA_DEBUG_PRINT(SA_DEBUG_LVL1, params->radio_id,
				       "Legacy\\20Mhz 0x%x 40Mhz 0x%x 80Mhz "
				       "0x%x 160Mhz 0x%x 320Mhz 0x%x\n",
				       g_train_params.ratecode[0],
				       g_train_params.ratecode[1],
				       g_train_params.ratecode[2],
				       g_train_params.ratecode[3],
				       g_train_params.ratecode[4]);
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE:
		if (sa_enable_test_mode)
			params->param_value = g_train_params.antenna;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_PACKETS_TESTMODE:
		if (sa_enable_test_mode)
			params->param_value = g_train_params.numpkts;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_MINPKTS_TESTMODE:
		if (sa_enable_test_mode)
			params->param_value = g_train_params.minpkts;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_PERTH_TESTMODE:
		if (sa_enable_test_mode)
			params->param_value = g_train_params.per_threshold;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_START_TESTMODE:
		/* No state to report here; return success in test mode */
		if (!sa_enable_test_mode)
			return -1;
		break;
	case SMART_ANT_PARAM_TRAIN_EXTN_FLAGS_TESTMODE:
		if (sa_enable_test_mode)
			params->param_value = g_train_params.extn_train_flags;
		else
			return -1;
		break;
	case SMART_ANT_PARAM_MAX_MPDU_COUNT_FOR_TARINING:
		params->param_value = sa_params->max_mpdu_train_cnt;
		break;
	case SMART_ANT_PARAM_MCS_MARGIN:
		if (sa_params->mcs_margin_en)
			params->param_value = sa_params->mcs_margin_val;
		else
			params->param_value = 0;
		break;
	default:
		display_help();
		return -1;
	}

	return 0;
}

static int sa_test_get_node_config(void *ccp, u32 id, u32 *config)
{
	struct sa_node_ccp *node = ccp;
	struct sa_node_train_info *train_info;

	if (!config || !node)
		return -1;

	train_info = &node->train_info;
	switch (id) {
	case SA_TX_FEEDBACK_CONFIG:
		*config = train_info->tx_feedback_config;
		break;
	default:
		return 0;
	}

	return 0;
}

static int sa_test_get_ext_traininginfo(void *ccp, u16 *minpkts,
					u16 *per_th, u32 *extn_flags)
{
	if (!minpkts || !per_th || !extn_flags)
		return -1;

	*minpkts = g_train_params.minpkts;
	*per_th = g_train_params.per_threshold;
	*extn_flags = g_train_params.extn_train_flags;

	return 0;
}

static int
sa_test_get_gpio_config(u8 radio_id, struct ath12k_sa_gpio_config *gpio_cfg)
{
	struct radio_config *radio;
	u8 i;

	if (!gpio_cfg)
		return -1;

	if (radio_id >= SA_MAX_RADIOS)
		return -1;

	radio = &g_sa.radio[radio_id];

	for (i = 0; i < SA_MAX_GPIO_PINS; i++)
		gpio_cfg->gpio_pin[i] = radio->gpio_cfg.gpio_pin[i];
	for (i = 0; i < SA_MAX_GPIO_FUNCS; i++)
		gpio_cfg->gpio_func[i] = radio->gpio_cfg.gpio_func[i];

	SA_DEBUG_PRINT(SA_DEBUG_LVL1, radio_id,
		       "SA Test: GIPOs To be fed by Customer Algo module;\n");

	return 0;
}

static int sa_test_get_enable_rts_ant(u8 radio_id, u32 *en_rts)
{
	struct radio_config *radio;

	if (!en_rts || radio_id >= SA_MAX_RADIOS)
		return -1;

	radio = &g_sa.radio[radio_id];

	*en_rts = radio->sa_params.en_rts_ant;

	return 0;
}

static int sa_test_get_max_mpdu_count_for_training(u8 radio_id, u32 *value)
{
	struct radio_config *radio;

	if (!value || radio_id >= SA_MAX_RADIOS)
		return -1;

	radio = &g_sa.radio[radio_id];
	*value = radio->sa_params.max_mpdu_train_cnt;

	return 0;
}

static int sa_test_get_mcs_margin(u8 radio_id, u32 *value)
{
	struct radio_config *radio;

	if (!value || radio_id >= SA_MAX_RADIOS)
		return -1;

	radio = &g_sa.radio[radio_id];
	if (radio->sa_params.mcs_margin_en)
		*value = (radio->sa_params.mcs_margin_val << 1) | 1;
	else
		*value = 0;

	return 0;
}

static int __init sa_test_module_init(void)
{
	int ret;

	SA_DEBUG_PRINT(SA_DEBUG_LVL1, 0, "SA Test module Init");

	/* bind test implementations */
	sa_ops.sa_algo_init = sa_test_init;
	sa_ops.sa_algo_deinit = sa_test_deinit;
	sa_ops.sa_peer_connect = sa_test_peer_connect;
	sa_ops.sa_peer_disconnect = sa_test_peer_disconnect;
	sa_ops.sa_update_txfeedback = sa_test_update_txfeedback;
	sa_ops.sa_update_rxfeedback = sa_test_update_rxfeedback;
	sa_ops.sa_get_txantenna = sa_test_get_txantenna;
	sa_ops.sa_get_txdefaultantenna = sa_test_get_txdefaultantenna;
	sa_ops.sa_get_rxantenna = sa_test_get_rxantenna;
	sa_ops.sa_get_traininginfo = sa_test_get_traininginfo;
	sa_ops.sa_get_bcn_txantenna = sa_test_get_bcn_txantenna;
	sa_ops.sa_set_param = sa_test_set_param;
	sa_ops.sa_get_param = sa_test_get_param;
	sa_ops.sa_get_node_config = sa_test_get_node_config;
	sa_ops.sa_get_ext_traininginfo = sa_test_get_ext_traininginfo;
	sa_ops.sa_get_gpio_config = sa_test_get_gpio_config;
	sa_ops.sa_get_enable_rts_ant = sa_test_get_enable_rts_ant;
	sa_ops.sa_get_max_mpdu_count_for_training =
					sa_test_get_max_mpdu_count_for_training;
	sa_ops.sa_get_mcs_margin = sa_test_get_mcs_margin;

	ret = ath12k_register_smart_ant_ops(&sa_ops);
	if (ret)
		SA_DEBUG_PRINT(SA_DEBUG_LVL1, 0,
			       "SA Test: register ops failed: %d", ret);

	return ret;
}

static void __exit sa_test_module_exit(void)
{
	int ret;

	ret = ath12k_unregister_smart_ant_ops();
	if (ret)
		SA_DEBUG_PRINT(SA_DEBUG_LVL1, 0,
			       "SA Test: unregister ops failed: %d", ret);

	SA_DEBUG_PRINT(SA_DEBUG_LVL1, 0, "SA Test module Exit");
}

module_init(sa_test_module_init);
module_exit(sa_test_module_exit);

MODULE_DESCRIPTION("Test module for Qualcomm Technologies Smart Antenna feature");
MODULE_LICENSE("Dual BSD/GPL");
