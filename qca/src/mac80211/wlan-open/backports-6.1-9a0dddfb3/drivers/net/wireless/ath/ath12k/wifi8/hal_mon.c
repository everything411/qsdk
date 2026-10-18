// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_rx.h"
#include "hal_rx_desc.h"
#include "../hal_mon_cmn.h"
#include "../dp_mon.h"
#include "hal_mon.h"
#include "hal_qcn9625.h"

#include "dp_rx.h"
#include "../hw.h"

#define HAL_TX_MON_WMASK_COMPACT_EN_CFG			0x1
#define HAL_TX_MON_WMASK_PCU_PPDU_SETUP_INIT_CFG	0x1E800000
#define HAL_TX_MON_WMASK_FES_SETUP_CFG			0x3
#define HAL_TX_MON_WMASK_PEER_ENTRY_CFG			0x103
#define HAL_TX_MON_WMASK_QUEUE_EXT_CFG			0x1
#define HAL_TX_MON_WMASK_MSDU_START_CFG			0x1
#define HAL_TX_MON_WMASK_MPDU_START_CFG			0x3
#define HAL_TX_MON_WMASK_FES_STATUS_END_CFG		0x7
#define HAL_TX_MON_WMASK_RESPONSE_END_STATUS_CFG	0xD
#define HAL_TX_MON_WMASK_FES_STATUS_PROT_CFG		0x3
#define HAL_TX_MON_WMASK_RXPCU_USER_SETUP_CFG		0xFF

u32 ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get(void)
{
	return RX_MON_MPDU_START_WMASK;
}

u32 ath12k_wifi8_hal_mon_rx_mpdu_end_wmask_get(void)
{
	return RX_MON_MPDU_END_WMASK;
}

u32 ath12k_wifi8_hal_mon_rx_msdu_end_wmask_get(void)
{
	return RX_MON_MSDU_END_WMASK;
}

u32 ath12k_wifi8_hal_mon_rx_ppdu_end_usr_stats_wmask_get(void)
{
	return RX_MON_PPDU_END_USER_STATS_WMASK;
}

static __always_inline void
ath12k_wifi8_hal_mon_get_bb_info(const void *tlv_data, u16 tlv_len,
				 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_rx_ppdu_bb_capture_info *bb_info = tlv_data;
	struct hal_rx_ppdu_cfr_info *cfr = &ppdu_info->cfr_info;
	u32 capture_info = __le32_to_cpu(bb_info->info);

	cfr->bb_captured_channel =
		!!u32_get_bits(capture_info,
			       HAL_RX_PPDU_END_INFO_BB_CAPTURED_CHANNEL_MASK);
	cfr->bb_captured_timeout =
		!!u32_get_bits(capture_info,
			       HAL_RX_PPDU_END_INFO_BB_CAPTURED_TIMEOUT_MASK);
	cfr->bb_captured_reason =
		u32_get_bits(capture_info,
			     HAL_RX_PPDU_END_INFO_BB_CAPTURED_REASON_MASK);
}

static __always_inline void
ath12k_wifi8_hal_mon_get_rtt_info(const void *tlv_data, u16 tlv_len,
				  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_ppdu_cfr_info *cfr = &ppdu_info->cfr_info;
	const struct hal_rx_ppdu_rtt_info *rtt_info = tlv_data;
	u8 rtt_cfr_status;
	u32 info;

	info = __le32_to_cpu(rtt_info->info0);
	cfr->rx_location_info_valid =
		!!u32_get_bits(info, HAL_PHYRX_LOCATION_RX_LOCATION_INFO_VALID_MASK);

	info = __le32_to_cpu(rtt_info->info1);
	rtt_cfr_status = u32_get_bits(info, HAL_PHYRX_LOCATION_RTT_CFR_STATUS_MASK);
	cfr->chan_capture_status =
		(rtt_cfr_status & HAL_PHYRX_LOCATION_CHAN_CAPTURE_STATUS_MASK) >>
		HAL_PHYRX_LOCATION_CHAN_CAPTURE_STATUS_SHIFT;

	info = __le32_to_cpu(rtt_info->info2);
	cfr->rtt_che_buffer_pointer_low32 =
		u32_get_bits(info, HAL_PHYRX_LOCATION_RTT_CHE_BUFFER_POINTER_LOW32_MASK);

	info = __le32_to_cpu(rtt_info->info3);
	cfr->rtt_che_buffer_pointer_high8 =
		u32_get_bits(info, HAL_PHYRX_LOCATION_RTT_CHE_BUFFER_POINTER_HIGH8_MASK);
	cfr->mcs_rate = u32_get_bits(info, HAL_PHYRX_LOCATION_RTT_MCS_RATE_MASK);

	info = __le32_to_cpu(rtt_info->info4);
	cfr->rtt_cfo_measurement =
		(s16)u32_get_bits(info, HAL_PHYRX_LOCATION_RTT_CFO_MEASUREMENT_MASK);
	cfr->gi_type = u32_get_bits(info, HAL_PHYRX_LOCATION_RTT_GI_TYPE_MASK);

	info = __le32_to_cpu(rtt_info->info5);
	cfr->rx_start_ts = u32_get_bits(info, HAL_PHYRX_LOCATION_RX_START_TS_MASK);

	info = __le32_to_cpu(rtt_info->info6);
	cfr->agc_gain_info0 =
		u32_get_bits(info, HAL_PHYRX_LOCATION_GAIN_CHAIN0_MASK);
	cfr->agc_gain_info0 |=
		u32_get_bits(info, HAL_PHYRX_LOCATION_GAIN_CHAIN1_MASK) << 16;

	info = __le32_to_cpu(rtt_info->info7);
	cfr->agc_gain_info1 =
		u32_get_bits(info, HAL_PHYRX_LOCATION_GAIN_CHAIN2_MASK);
	cfr->agc_gain_info1 |=
		u32_get_bits(info, HAL_PHYRX_LOCATION_GAIN_CHAIN3_MASK) << 16;

	cfr->agc_gain_info2 = 0;
	cfr->agc_gain_info3 = 0;
}

static __always_inline void
ath12k_wifi8_hal_mon_get_nrp_mac_addr(u16 addr_l16, u32 addr_h32, u8 *addr)
{
	memcpy(addr, &addr_l16, 2);
	memcpy(addr + 2, &addr_h32, ETH_ALEN - 2);
}

static __always_inline void
ath12k_wifi8_hal_mon_handle_ofdma_info(u32 resp_ref, u32 resp_ref_ext,
				       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = resp_ref;
	rx_user_status->ul_ofdma_user_v0_word1 = resp_ref_ext;
}

static __always_inline void
ath12k_wifi8_hal_mon_populate_byte_count(u32 *info,
					 struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[3],
			     HAL_RX_PPDU_END_USER_STATS_INFO3_MPDU_OK_BYTE_CNT);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[4],
			     HAL_RX_PPDU_END_USER_STATS_INFO4_MPDU_ERR_BYTE_CNT);
}

static __always_inline void
ath12k_wifi8_hal_mon_handle_ofdma_info_compact(u32 resp_ref, u32 resp_ref_ext,
					       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = resp_ref;
	rx_user_status->ul_ofdma_user_v0_word1 = resp_ref_ext;
}

static __always_inline void
ath12k_wifi8_hal_mon_populate_byte_count_compact(
					u32 *info,
					struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[3],
			     HAL_RX_PPDU_END_USER_STATS_INFO3_MPDU_OK_BYTE_CNT_CMPCT);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[4],
			     HAL_RX_PPDU_END_USER_STATS_INFO4_MPDU_ERR_BYTE_CNT_CMPCT);
}

static __always_inline void
ath12k_wifi8_hal_mon_populate_mu_user_info(struct hal_rx_mon_ppdu_info *ppdu_info,
					   struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ast_index = ppdu_info->ast_index;
	rx_user_status->tid = ppdu_info->tid;
	rx_user_status->tcp_ack_msdu_count =
		ppdu_info->tcp_ack_msdu_count;
	rx_user_status->tcp_msdu_count =
		ppdu_info->tcp_msdu_count;
	rx_user_status->udp_msdu_count =
		ppdu_info->udp_msdu_count;
	rx_user_status->other_msdu_count =
		ppdu_info->other_msdu_count;
	rx_user_status->frame_control = ppdu_info->frame_control;
	rx_user_status->frame_control_info_valid =
		ppdu_info->frame_control_info_valid;
	rx_user_status->data_sequence_control_info_valid =
		ppdu_info->data_sequence_control_info_valid;
	rx_user_status->first_data_seq_ctrl =
		ppdu_info->first_data_seq_ctrl;
	rx_user_status->preamble_type = ppdu_info->preamble_type;
	rx_user_status->ht_flags = ppdu_info->ht_flags;
	rx_user_status->vht_flags = ppdu_info->vht_flags;
	rx_user_status->he_flags = ppdu_info->he_flags;
	rx_user_status->rs_flags = ppdu_info->rs_flags;

	rx_user_status->mpdu_cnt_fcs_ok =
		ppdu_info->num_mpdu_fcs_ok;
	rx_user_status->mpdu_cnt_fcs_err =
		ppdu_info->num_mpdu_fcs_err;
	rx_user_status->retried_msdu_count =
		ppdu_info->retried_msdu_count;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_vht_sig_a(const struct hal_rx_vht_sig_a_info *vht_sig,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 nsts, info0, info1;
	u8 gi_setting;

	info0 = __le32_to_cpu(vht_sig->info0);
	info1 = __le32_to_cpu(vht_sig->info1);

	ppdu_info->ldpc = u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_SU_MU_CODING);
	ppdu_info->mcs = u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_MCS);
	gi_setting = u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_GI_SETTING);

	switch (gi_setting) {
	case HAL_RX_VHT_SIG_A_NORMAL_GI:
		ppdu_info->gi = HAL_RX_GI_0_8_US;
		break;
	case HAL_RX_VHT_SIG_A_SHORT_GI:
	case HAL_RX_VHT_SIG_A_SHORT_GI_AMBIGUITY:
		ppdu_info->gi = HAL_RX_GI_0_4_US;
		break;
	}

	ppdu_info->is_stbc = u32_get_bits(info0, HAL_RX_VHT_SIG_A_INFO_INFO0_STBC);
	nsts = u32_get_bits(info0, HAL_RX_VHT_SIG_A_INFO_INFO0_NSTS);
	if (ppdu_info->is_stbc && nsts > 0)
		nsts = ((nsts + 1) >> 1) - 1;

	ppdu_info->nss = u32_get_bits(nsts, VHT_SIG_SU_NSS_MASK) + 1;
	ppdu_info->bw = u32_get_bits(info0, HAL_RX_VHT_SIG_A_INFO_INFO0_BW);
	ppdu_info->beamformed = u32_get_bits(info1,
					     HAL_RX_VHT_SIG_A_INFO_INFO1_BEAMFORMED);
	ppdu_info->vht_flag_values5 = u32_get_bits(info0,
						   HAL_RX_VHT_SIG_A_INFO_INFO0_GROUP_ID);
	ppdu_info->vht_flag_values3[0] = (((ppdu_info->mcs) << 4) |
					    ppdu_info->nss);
	ppdu_info->vht_flag_values2 = ppdu_info->bw;
	ppdu_info->vht_flag_values4 =
		u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_SU_MU_CODING);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_ht_sig(const struct hal_rx_ht_sig_info *ht_sig,
				  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(ht_sig->info0);
	u32 info1 = __le32_to_cpu(ht_sig->info1);

	ppdu_info->mcs = u32_get_bits(info0, HAL_RX_HT_SIG_INFO_INFO0_MCS);
	ppdu_info->bw = u32_get_bits(info0, HAL_RX_HT_SIG_INFO_INFO0_BW);
	ppdu_info->is_stbc = u32_get_bits(info1, HAL_RX_HT_SIG_INFO_INFO1_STBC);
	ppdu_info->ldpc = u32_get_bits(info1, HAL_RX_HT_SIG_INFO_INFO1_FEC_CODING);
	ppdu_info->gi = u32_get_bits(info1, HAL_RX_HT_SIG_INFO_INFO1_GI);
	ppdu_info->nss = (ppdu_info->mcs >> 3) + 1;
}

static __always_inline u8
ath12k_wifi8_hal_mon_map_legacy_rate_to_hw_rate(u8 rate)
{
	u8 ath12k_rate;

	/* Map hal_rx_legacy_rate to ath12k_hw_rate_cck */
	switch (rate) {
	case HAL_RX_LEGACY_RATE_LP_1_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_1M;
		break;
	case HAL_RX_LEGACY_RATE_LP_2_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_2M;
		break;
	case HAL_RX_LEGACY_RATE_LP_5_5_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_5_5M;
		break;
	case HAL_RX_LEGACY_RATE_LP_11_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_11M;
		break;
	case HAL_RX_LEGACY_RATE_SP_2_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_SP_2M;
		break;
	case HAL_RX_LEGACY_RATE_SP_5_5_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_SP_5_5M;
		break;
	case HAL_RX_LEGACY_RATE_SP_11_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_SP_11M;
		break;
	default:
		ath12k_rate = rate;
		break;
	}

	return ath12k_rate;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_l_sig_b(const struct hal_rx_lsig_b_info *lsigb,
				   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(lsigb->info0);
	u8 rate;

	rate = u32_get_bits(info0, HAL_RX_LSIG_B_INFO_INFO0_RATE);

	switch (rate) {
	case 1:
		rate = HAL_RX_LEGACY_RATE_LP_1_MBPS;
		break;
	case 2:
		rate = HAL_RX_LEGACY_RATE_LP_2_MBPS;
		break;
	case 3:
		rate = HAL_RX_LEGACY_RATE_LP_5_5_MBPS;
		break;
	case 4:
		rate = HAL_RX_LEGACY_RATE_LP_11_MBPS;
		break;
	case 5:
		rate = HAL_RX_LEGACY_RATE_SP_2_MBPS;
		break;
	case 6:
		rate = HAL_RX_LEGACY_RATE_SP_5_5_MBPS;
		break;
	case 7:
		rate = HAL_RX_LEGACY_RATE_SP_11_MBPS;
		break;
	default:
		rate = HAL_RX_LEGACY_RATE_INVALID;
		break;
	}

	ppdu_info->rate = ath12k_wifi8_hal_mon_map_legacy_rate_to_hw_rate(rate);
	ppdu_info->cck_flag = 1;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_l_sig_a(const struct hal_rx_lsig_a_info *lsiga,
				   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(lsiga->info0);
	u8 rate;

	rate = u32_get_bits(info0, HAL_RX_LSIG_A_INFO_INFO0_RATE);

	switch (rate) {
	case 8:
		rate = HAL_RX_LEGACY_RATE_OFDM_48_MBPS;
		break;
	case 9:
		rate = HAL_RX_LEGACY_RATE_OFDM_24_MBPS;
		break;
	case 10:
		rate = HAL_RX_LEGACY_RATE_OFDM_12_MBPS;
		break;
	case 11:
		rate = HAL_RX_LEGACY_RATE_OFDM_6_MBPS;
		break;
	case 12:
		rate = HAL_RX_LEGACY_RATE_OFDM_54_MBPS;
		break;
	case 13:
		rate = HAL_RX_LEGACY_RATE_OFDM_36_MBPS;
		break;
	case 14:
		rate = HAL_RX_LEGACY_RATE_OFDM_18_MBPS;
		break;
	case 15:
		rate = HAL_RX_LEGACY_RATE_OFDM_9_MBPS;
		break;
	default:
		rate = HAL_RX_LEGACY_RATE_OFDM_INVALID;
		break;
	}

	ppdu_info->rate = rate;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_b2_ofdma(
				const struct hal_rx_he_sig_b2_ofdma_info *ofdma,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, value;

	info0 = __le32_to_cpu(ofdma->info0);

	ppdu_info->he_data1 |= HE_MCS_KNOWN | HE_DCM_KNOWN | HE_CODING_KNOWN;

	/* HE-data2 */
	ppdu_info->he_data2 |= HE_TXBF_KNOWN;

	ppdu_info->mcs = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_MCS);
	value = ppdu_info->mcs << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_DCM);
	value = value << HE_DCM_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_CODING);
	ppdu_info->ldpc = value;
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	/* HE-data4 */
	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_ID);
	value = value << HE_STA_ID_SHIFT;
	ppdu_info->he_data4 |= value;

	ppdu_info->nss = u32_get_bits(info0,
				      HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_NSTS) + 1;
	ppdu_info->beamformed = u32_get_bits(info0,
					     HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_TXBF);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_b2_mu(
				const struct hal_rx_he_sig_b2_mu_info *he_sig_b2_mu,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, value;

	info0 = __le32_to_cpu(he_sig_b2_mu->info0);

	ppdu_info->he_data1 |= HE_MCS_KNOWN | HE_CODING_KNOWN;

	ppdu_info->mcs = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_MCS);
	value = ppdu_info->mcs << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_CODING);
	ppdu_info->ldpc = value;
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_ID);
	value = value << HE_STA_ID_SHIFT;
	ppdu_info->he_data4 |= value;

	ppdu_info->nss = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_NSTS) + 1;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_b1_mu(
				const struct hal_rx_he_sig_b1_mu_info *he_sig_b1_mu,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(he_sig_b1_mu->info0);
	u16 ru_tones;

	ru_tones = u32_get_bits(info0,
				HAL_RX_HE_SIG_B1_MU_INFO_INFO0_RU_ALLOCATION);
	ppdu_info->ru_alloc = ath12k_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	ppdu_info->he_RU[0] = ru_tones;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_mu(
				const struct hal_rx_he_sig_a_mu_dl_info *he_sig_a_mu_dl,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, info1, value;
	u16 he_gi = 0, he_ltf = 0;

	info0 = __le32_to_cpu(he_sig_a_mu_dl->info0);
	info1 = __le32_to_cpu(he_sig_a_mu_dl->info1);

	ppdu_info->he_mu_flags = 1;

	ppdu_info->he_data1 = HE_MU_FORMAT_TYPE;
	ppdu_info->he_data1 |=
			HE_BSS_COLOR_KNOWN |
			HE_DL_UL_KNOWN |
			HE_LDPC_EXTRA_SYMBOL_KNOWN |
			HE_STBC_KNOWN |
			HE_DATA_BW_RU_KNOWN |
			HE_DOPPLER_KNOWN;

	ppdu_info->he_data2 =
			HE_GI_KNOWN |
			HE_LTF_SYMBOLS_KNOWN |
			HE_PRE_FEC_PADDING_KNOWN |
			HE_PE_DISAMBIGUITY_KNOWN |
			HE_TXOP_KNOWN |
			HE_MIDABLE_PERIODICITY_KNOWN;

	/* data3 */
	ppdu_info->he_data3 = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_BSS_COLOR);
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_UL_FLAG);
	value = value << HE_DL_UL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_LDPC_EXTRA);
	value = value << HE_LDPC_EXTRA_SYMBOL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_STBC);
	value = value << HE_STBC_SHIFT;
	ppdu_info->he_data3 |= value;

	/* data4 */
	ppdu_info->he_data4 = u32_get_bits(info0,
					   HAL_RX_HE_SIG_A_MU_DL_INFO0_SPATIAL_REUSE);
	ppdu_info->he_data4 = value;

	/* data5 */
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_TRANSMIT_BW);
	ppdu_info->he_data5 = value;
	ppdu_info->bw = value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_CP_LTF_SIZE);

	switch (value) {
	case 0:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_4_X;
		break;
	case 1:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_2_X;
		break;
	case 2:
		he_gi = HE_GI_1_6;
		he_ltf = HE_LTF_2_X;
		break;
	case 3:
		he_gi = HE_GI_3_2;
		he_ltf = HE_LTF_4_X;
		break;
	}

	ppdu_info->gi = he_gi;
	hal_get_radiotap_gi_ltf(&he_gi, &he_ltf);
	value = he_gi << HE_GI_SHIFT;
	ppdu_info->he_data5 |= value;

	value = he_ltf << HE_LTF_SIZE_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_NUM_LTF_SYMB);
	value = (value << HE_LTF_SYM_SHIFT);
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_PKT_EXT_FACTOR);
	value = value << HE_PRE_FEC_PAD_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_PKT_EXT_PE_DISAM);
	value = value << HE_PE_DISAMBIGUITY_SHIFT;
	ppdu_info->he_data5 |= value;

	/*data6*/
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_DOPPLER_INDICATION);
	value = value << HE_DOPPLER_SHIFT;
	ppdu_info->he_data6 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_TXOP_DURATION);
	value = value << HE_TXOP_SHIFT;
	ppdu_info->he_data6 |= value;

	/* HE-MU Flags */
	/* HE-MU-flags1 */
	ppdu_info->he_flags1 =
		HE_SIG_B_MCS_KNOWN |
		HE_SIG_B_DCM_KNOWN |
		HE_SIG_B_COMPRESSION_FLAG_1_KNOWN |
		HE_SIG_B_SYM_NUM_KNOWN |
		HE_RU_0_KNOWN;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_MCS_OF_SIGB);
	ppdu_info->he_flags1 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_DCM_OF_SIGB);
	value = value << HE_DCM_FLAG_1_SHIFT;
	ppdu_info->he_flags1 |= value;

	/* HE-MU-flags2 */
	ppdu_info->he_flags2 = HE_BW_KNOWN;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_TRANSMIT_BW);
	ppdu_info->he_flags2 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_COMP_MODE_SIGB);
	value = value << HE_SIG_B_COMPRESSION_FLAG_2_SHIFT;
	ppdu_info->he_flags2 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_NUM_SIGB_SYMB);
	value = value - 1;
	value = value << HE_NUM_SIG_B_SYMBOLS_SHIFT;
	ppdu_info->he_flags2 |= value;

	ppdu_info->is_stbc = info1 &
			     HAL_RX_HE_SIG_A_MU_DL_INFO1_STBC;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_su(const struct hal_rx_he_sig_a_su_info *he_sig_a,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, info1, value;
	u32 dcm;
	u8 he_dcm = 0, he_stbc = 0;
	u16 he_gi = 0, he_ltf = 0;

	ppdu_info->he_flags = 1;

	info0 = __le32_to_cpu(he_sig_a->info0);
	info1 = __le32_to_cpu(he_sig_a->info1);

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_FORMAT_IND);
	if (value == 0)
		ppdu_info->he_data1 = HE_TRIG_FORMAT_TYPE;
	else
		ppdu_info->he_data1 = HE_SU_FORMAT_TYPE;

	ppdu_info->he_data1 |=
			HE_BSS_COLOR_KNOWN |
			HE_BEAM_CHANGE_KNOWN |
			HE_DL_UL_KNOWN |
			HE_MCS_KNOWN |
			HE_DCM_KNOWN |
			HE_CODING_KNOWN |
			HE_LDPC_EXTRA_SYMBOL_KNOWN |
			HE_STBC_KNOWN |
			HE_DATA_BW_RU_KNOWN |
			HE_DOPPLER_KNOWN;

	ppdu_info->he_data2 |=
			HE_GI_KNOWN |
			HE_TXBF_KNOWN |
			HE_PE_DISAMBIGUITY_KNOWN |
			HE_TXOP_KNOWN |
			HE_LTF_SYMBOLS_KNOWN |
			HE_PRE_FEC_PADDING_KNOWN |
			HE_MIDABLE_PERIODICITY_KNOWN;

	ppdu_info->he_data3 = u32_get_bits(info0,
					   HAL_RX_HE_SIG_A_SU_INFO_INFO0_BSS_COLOR);
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_BEAM_CHANGE);
	value = value << HE_BEAM_CHANGE_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_DL_UL_FLAG);
	value = value << HE_DL_UL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_MCS);
	ppdu_info->mcs = value;
	value = value << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_DCM);
	he_dcm = value;
	value = value << HE_DCM_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_CODING);
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_LDPC_EXTRA);
	value = value << HE_LDPC_EXTRA_SYMBOL_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_STBC);
	he_stbc = value;
	value = value << HE_STBC_SHIFT;
	ppdu_info->he_data3 |= value;

	/* data4 */
	ppdu_info->he_data4 = u32_get_bits(info0,
					   HAL_RX_HE_SIG_A_SU_INFO_INFO0_SPATIAL_REUSE);

	/* data5 */
	value = u32_get_bits(info0,
			     HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_BW);
	ppdu_info->he_data5 = value;
	ppdu_info->bw = value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_CP_LTF_SIZE);

	switch (value) {
	case 0:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_1_X;
		break;
	case 1:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_2_X;
		break;
	case 2:
		he_gi = HE_GI_1_6;
		he_ltf = HE_LTF_2_X;
		break;
	case 3:
		if (he_dcm && he_stbc) {
			he_gi = HE_GI_0_8;
			he_ltf = HE_LTF_4_X;
		} else {
			he_gi = HE_GI_3_2;
			he_ltf = HE_LTF_4_X;
		}
		break;
	}
	ppdu_info->gi = he_gi;
	ppdu_info->ltf_size = he_ltf;
	hal_get_radiotap_gi_ltf(&he_gi, &he_ltf);
	value = he_gi << HE_GI_SHIFT;
	ppdu_info->he_data5 |= value;
	value = he_ltf << HE_LTF_SIZE_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS);
	value = (value << HE_LTF_SYM_SHIFT);
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_PKT_EXT_FACTOR);
	value = value << HE_PRE_FEC_PAD_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXBF);
	value = value << HE_TXBF_SHIFT;
	ppdu_info->he_data5 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_PKT_EXT_PE_DISAM);
	value = value << HE_PE_DISAMBIGUITY_SHIFT;
	ppdu_info->he_data5 |= value;

	/* data6 */
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS);
	value++;
	ppdu_info->he_data6 = value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_DOPPLER_IND);
	value = value << HE_DOPPLER_SHIFT;
	ppdu_info->he_data6 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXOP_DURATION);
	value = value << HE_TXOP_SHIFT;
	ppdu_info->he_data6 |= value;

	ppdu_info->mcs =
		u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_MCS);
	ppdu_info->bw =
		u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_BW);
	ppdu_info->ldpc = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_CODING);
	ppdu_info->is_stbc = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_STBC);
	ppdu_info->beamformed = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXBF);
	dcm = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_DCM);
	ppdu_info->nss = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS) + 1;
	ppdu_info->dcm = dcm;
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_get_phy_version(const struct hal_mon_usig_cmn *cmn,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	ppdu_info->u_sig_info.phy_version =
			le32_get_bits(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_cmn(const struct hal_mon_usig_cmn *cmn,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 common;

	ppdu_info->u_sig_info.bw = le32_get_bits(cmn->info0,
						 HAL_RX_USIG_CMN_INFO0_BW);
	ppdu_info->u_sig_info.ul_dl = le32_get_bits(cmn->info0,
						    HAL_RX_USIG_CMN_INFO0_UL_DL);

	common = __le32_to_cpu(ppdu_info->u_sig_info.usig.common);
	common |= IEEE80211_RADIOTAP_EHT_USIG_COMMON_PHY_VER_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_BW_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_UL_DL_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_BSS_COLOR_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_TXOP_KNOWN |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_PHY_VER) |
		  u32_encode_bits(ppdu_info->u_sig_info.bw,
				  IEEE80211_RADIOTAP_EHT_USIG_COMMON_BW) |
		  u32_encode_bits(ppdu_info->u_sig_info.ul_dl,
				  IEEE80211_RADIOTAP_EHT_USIG_COMMON_UL_DL) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_BSS_COLOR,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_BSS_COLOR) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_TXOP,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_TXOP);
	ppdu_info->u_sig_info.usig.common = cpu_to_le32(common);

	switch (ppdu_info->u_sig_info.bw) {
	default:
		fallthrough;
	case HAL_EHT_BW_20:
		ppdu_info->bw = HAL_RX_BW_20MHZ;
		break;
	case HAL_EHT_BW_40:
		ppdu_info->bw = HAL_RX_BW_40MHZ;
		break;
	case HAL_EHT_BW_80:
		ppdu_info->bw = HAL_RX_BW_80MHZ;
		break;
	case HAL_EHT_BW_160:
		ppdu_info->bw = HAL_RX_BW_160MHZ;
		break;
	case HAL_EHT_BW_320_1:
	case HAL_EHT_BW_320_2:
		ppdu_info->bw = HAL_RX_BW_320MHZ;
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_cmn(const struct hal_mon_usig_cmn *cmn,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 common;

	ppdu_info->u_sig_info.phy_version =
			le32_get_bits(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION);
	ppdu_info->u_sig_info.bw = le32_get_bits(cmn->info0,
						 HAL_RX_USIG_CMN_INFO0_BW);
	ppdu_info->u_sig_info.ul_dl = le32_get_bits(cmn->info0,
						    HAL_RX_USIG_CMN_INFO0_UL_DL);

	common = __le32_to_cpu(ppdu_info->u_sig_info.uhr_usig.common);

	/* until UHR SIG radiotap header stabilizes, we will use EHT enums
	 * for common non-impacted fields
	 */
	common |= IEEE80211_RADIOTAP_UHR_USIG_COMMON_PHY_VER_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_BW_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_UL_DL_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_BSS_COLOR_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_TXOP_KNOWN |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_PHY_VER) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_BW,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_BW) |
		  u32_encode_bits(ppdu_info->u_sig_info.ul_dl,
				  IEEE80211_RADIOTAP_UHR_USIG_COMMON_UL_DL) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_BSS_COLOR,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_BSS_COLOR) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_TXOP,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_TXOP);

	ppdu_info->u_sig_info.uhr_usig.common = cpu_to_le32(common);

	switch (ppdu_info->u_sig_info.bw) {
	default:
		fallthrough;
	case HAL_EHT_BW_20:
		ppdu_info->bw = HAL_RX_BW_20MHZ;
		break;
	case HAL_EHT_BW_40:
		ppdu_info->bw = HAL_RX_BW_40MHZ;
		break;
	case HAL_EHT_BW_80:
		ppdu_info->bw = HAL_RX_BW_80MHZ;
		break;
	case HAL_EHT_BW_160:
		ppdu_info->bw = HAL_RX_BW_160MHZ;
		break;
	case HAL_EHT_BW_320_1:
	case HAL_EHT_BW_320_2:
		ppdu_info->bw = HAL_RX_BW_320MHZ;
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_tb(const struct hal_mon_usig_tb *usig_tb,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_eht_usig *usig = &ppdu_info->u_sig_info.usig;
	enum ieee80211_radiotap_eht_usig_tb spatial_reuse1, spatial_reuse2;
	u32 common, value, mask;

	spatial_reuse1 = IEEE80211_RADIOTAP_EHT_USIG2_TB_B3_B6_SPATIAL_REUSE_1;
	spatial_reuse2 = IEEE80211_RADIOTAP_EHT_USIG2_TB_B7_B10_SPATIAL_REUSE_2;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_tb->info0,
					      HAL_RX_USIG_TB_INFO0_PPDU_TYPE_COMP_MODE);

	common |=
		u32_encode_bits(!le32_get_bits(usig_tb->info0,
					       HAL_RX_USIG_TB_INFO0_RX_INTEG_CHECK_PASS),
				IEEE80211_RADIOTAP_EHT_USIG_COMMON_BAD_USIG_CRC);

	value |= IEEE80211_RADIOTAP_EHT_USIG1_TB_B20_B25_DISREGARD |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_EHT_USIG2_TB_B0_B1_PPDU_TYPE) |
		 IEEE80211_RADIOTAP_EHT_USIG2_TB_B2_VALIDATE |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_1,
				     spatial_reuse1) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_2,
				     spatial_reuse2) |
		 IEEE80211_RADIOTAP_EHT_USIG2_TB_B11_B15_DISREGARD |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_CRC,
				     IEEE80211_RADIOTAP_EHT_USIG2_TB_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_TAIL,
				     IEEE80211_RADIOTAP_EHT_USIG2_TB_B20_B25_TAIL);

	mask |= IEEE80211_RADIOTAP_EHT_USIG1_TB_B20_B25_DISREGARD |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B2_VALIDATE |
		spatial_reuse1 | spatial_reuse2 |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B11_B15_DISREGARD |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B16_B19_CRC |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_tb(const struct hal_mon_usig_tb *usig_tb,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_uhr_usig *usig = &ppdu_info->u_sig_info.uhr_usig;
	enum ieee80211_radiotap_uhr_usig_tb spatial_reuse1, spatial_reuse2;
	u32 common, value, mask;

	spatial_reuse1 = IEEE80211_RADIOTAP_UHR_USIG2_TB_B3_B6_SPATIAL_REUSE_1;
	spatial_reuse2 = IEEE80211_RADIOTAP_UHR_USIG2_TB_B7_B10_SPATIAL_REUSE_2;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_tb->info0,
					      HAL_RX_USIG_TB_INFO0_PPDU_TYPE_COMP_MODE);

	common |=
		u32_encode_bits(!le32_get_bits(usig_tb->info0,
					       HAL_RX_USIG_TB_INFO0_RX_INTEG_CHECK_PASS),
				IEEE80211_RADIOTAP_UHR_USIG_COMMON_BAD_USIG_CRC);

	value |= IEEE80211_RADIOTAP_UHR_USIG1_TB_B20_B25_DISREGARD |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_UHR_USIG2_TB_B0_B1_PPDU_TYPE) |
		 IEEE80211_RADIOTAP_UHR_USIG2_TB_B2_VALIDATE |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_1,
				     spatial_reuse1) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_2,
				     spatial_reuse2) |
		 IEEE80211_RADIOTAP_UHR_USIG2_TB_B11_B15_DISREGARD |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_CRC,
				     IEEE80211_RADIOTAP_UHR_USIG2_TB_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_TAIL,
				     IEEE80211_RADIOTAP_UHR_USIG2_TB_B20_B25_TAIL);

	mask |= IEEE80211_RADIOTAP_UHR_USIG1_TB_B20_B25_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B2_VALIDATE |
		spatial_reuse1 | spatial_reuse2 |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B11_B15_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_mu(const struct hal_mon_usig_mu *usig_mu,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_uhr_usig *usig = &ppdu_info->u_sig_info.uhr_usig;
	enum ieee80211_radiotap_uhr_usig_mu sig_symb, punc;
	u32 common, value, mask, bss_color2_mask;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.cosr_cobf_disable =
		le32_get_bits(usig_mu->info0,
			      HAL_RX_USIG_MU_INFO0_VALIDATE_1_OR_COBF_COSR_DISABLE);

	if (!ppdu_info->u_sig_info.cosr_cobf_disable) {
		bss_color2_mask = IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B25_BSS_COLOR_2;
		value |= ATH12K_LE32_DEC_ENC(usig_mu->info0,
					     HAL_RX_USIG_UHR_CMN_INFO0_BSS_COLOR_2,
					     bss_color2_mask);
	} else {
		value |= IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B24_DISREGARD |
			IEEE80211_RADIOTAP_UHR_USIG1_MU_B25_VALIDATE;
	}

	sig_symb = IEEE80211_RADIOTAP_UHR_USIG2_MU_B11_B15_NUM_UHR_SYG_SYM;
	punc = IEEE80211_RADIOTAP_UHR_USIG2_MU_B3_B7_PUNCTURED_INFO;

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);
	ppdu_info->u_sig_info.eht_sig_mcs =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_EHT_SIG_MCS);
	ppdu_info->u_sig_info.num_eht_sig_sym =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_NUM_EHT_SIG_SYM);

	common |=
		u32_encode_bits(!le32_get_bits(usig_mu->info0,
					       HAL_RX_USIG_MU_INFO0_RX_INTEG_CHECK_PASS),
				IEEE80211_RADIOTAP_UHR_USIG_COMMON_BAD_USIG_CRC);

	value |= u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_UHR_USIG2_MU_B0_B1_PPDU_TYPE) |
		 u32_encode_bits(ppdu_info->u_sig_info.cosr_cobf_disable,
				 IEEE80211_RADIOTAP_UHR_USIG2_MU_B2_COBF_OR_COSR) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_PUNC_CH_INFO,
				     punc) |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B8_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.eht_sig_mcs,
				 IEEE80211_RADIOTAP_UHR_USIG2_MU_B9_B10_SIG_MCS) |
		 u32_encode_bits(ppdu_info->u_sig_info.num_eht_sig_sym,
				 sig_symb) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_CRC,
				     IEEE80211_RADIOTAP_UHR_USIG2_MU_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_TAIL,
				     IEEE80211_RADIOTAP_UHR_USIG2_MU_B20_B25_TAIL);

	mask |=  IEEE80211_RADIOTAP_UHR_USIG2_MU_B0_B1_PPDU_TYPE |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B2_COBF_OR_COSR |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B3_B7_PUNCTURED_INFO |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B8_VALIDATE |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B9_B10_SIG_MCS |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B11_B15_NUM_UHR_SYG_SYM |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B16_B19_CRC |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B20_B25_TAIL |
		 IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_UHR_USIG1_MU_B25_VALIDATE;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_mu(const struct hal_mon_usig_mu *usig_mu,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_eht_usig *usig = &ppdu_info->u_sig_info.usig;
	enum ieee80211_radiotap_eht_usig_mu sig_symb, punc;
	u32 common, value, mask;

	sig_symb = IEEE80211_RADIOTAP_EHT_USIG2_MU_B11_B15_EHT_SIG_SYMBOLS;
	punc = IEEE80211_RADIOTAP_EHT_USIG2_MU_B3_B7_PUNCTURED_INFO;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);
	ppdu_info->u_sig_info.eht_sig_mcs =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_EHT_SIG_MCS);
	ppdu_info->u_sig_info.num_eht_sig_sym =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_NUM_EHT_SIG_SYM);

	common |=
		u32_encode_bits(!le32_get_bits(usig_mu->info0,
					       HAL_RX_USIG_MU_INFO0_RX_INTEG_CHECK_PASS),
				IEEE80211_RADIOTAP_EHT_USIG_COMMON_BAD_USIG_CRC);

	value |= IEEE80211_RADIOTAP_EHT_USIG1_MU_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_EHT_USIG1_MU_B25_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_EHT_USIG2_MU_B0_B1_PPDU_TYPE) |
		 IEEE80211_RADIOTAP_EHT_USIG2_MU_B2_VALIDATE |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_PUNC_CH_INFO,
				     punc) |
		 IEEE80211_RADIOTAP_EHT_USIG2_MU_B8_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.eht_sig_mcs,
				 IEEE80211_RADIOTAP_EHT_USIG2_MU_B9_B10_SIG_MCS) |
		 u32_encode_bits(ppdu_info->u_sig_info.num_eht_sig_sym,
				 sig_symb) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_CRC,
				     IEEE80211_RADIOTAP_EHT_USIG2_MU_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_TAIL,
				     IEEE80211_RADIOTAP_EHT_USIG2_MU_B20_B25_TAIL);

	mask |= IEEE80211_RADIOTAP_EHT_USIG1_MU_B20_B24_DISREGARD |
		IEEE80211_RADIOTAP_EHT_USIG1_MU_B25_VALIDATE |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B2_VALIDATE |
		punc |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B8_VALIDATE |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B9_B10_SIG_MCS |
		sig_symb |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B16_B19_CRC |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_elr(const struct hal_mon_usig_elr *elr,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_uhr_usig *usig = &ppdu_info->u_sig_info.uhr_usig;
	u32 common, value, mask;
	u32 elr_valdiate_mask, elr_crc_mask, elr_tail_mask;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	elr_valdiate_mask = IEEE80211_RADIOTAP_UHR_USIG2_ELR_B13_B15_ELR_VALIDATE;
	elr_crc_mask = IEEE80211_RADIOTAP_UHR_USIG2_ELR_B16_B19_CRC;
	elr_tail_mask = IEEE80211_RADIOTAP_UHR_USIG2_ELR_B20_B25_TAIL;

	ppdu_info->u_sig_info.sta_id =
		le32_get_bits(elr->info0,
			      HAL_RX_USIG_ELR_INFO0_STA_ID);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
		le32_get_bits(elr->info0,
			      HAL_RX_USIG_ELR_INFO0_PPDU_TYPE_COMP_MODE);

	value |= IEEE80211_RADIOTAP_UHR_USIG1_ELR_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_UHR_USIG1_ELR_B25_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_UHR_USIG2_ELR_B0_B1_PPDU_TYPE) |
		 u32_encode_bits(ppdu_info->u_sig_info.sta_id,
				 IEEE80211_RADIOTAP_UHR_USIG2_ELR_B2_B12_STA_ID) |
		 ATH12K_LE32_DEC_ENC(elr->info0,
				     HAL_RX_USIG_ELR_INFO0_VALIDATE_1,
				     elr_valdiate_mask) |
		 ATH12K_LE32_DEC_ENC(elr->info0,
				     HAL_RX_USIG_ELR_INFO0_CRC,
				     elr_crc_mask) |
		 ATH12K_LE32_DEC_ENC(elr->info0,
				     HAL_RX_USIG_ELR_INFO0_TAIL,
				     elr_tail_mask);

	mask |= IEEE80211_RADIOTAP_UHR_USIG1_ELR_B20_B24_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG1_ELR_B25_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B2_B12_STA_ID |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B13_B15_ELR_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_hdr(const struct hal_mon_usig_hdr *usig,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 comp_mode;

	ppdu_info->eht_usig = true;

	comp_mode = le32_get_bits(usig->non_cmn.mu.info0,
				  HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);

	if (comp_mode == 0 && ppdu_info->u_sig_info.ul_dl)
		ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_tb(&usig->non_cmn.tb, ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_parse_u_sig_mu(&usig->non_cmn.mu, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_hdr(const struct hal_mon_usig_hdr *usig,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 comp_mode;

	comp_mode = le32_get_bits(usig->non_cmn.mu.info0,
				  HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);

	if (comp_mode == 0 && ppdu_info->u_sig_info.ul_dl)
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_tb(&usig->non_cmn.tb, ppdu_info);
	else if (comp_mode == 3)
		ath12k_wifi8_hal_mon_rx_parse_u_sig_elr(&usig->non_cmn.elr, ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_mu(&usig->non_cmn.mu, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_hdr(const struct hal_mon_usig_hdr *usig,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	ath12k_wifi8_hal_mon_rx_get_phy_version(&usig->cmn, ppdu_info);

	if (ppdu_info->u_sig_info.phy_version == HAL_EHT_PHY) {
		ppdu_info->eht_usig = true;
		/* phy version = 0 is for EHT */
		ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_cmn(&usig->cmn, ppdu_info);
		ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_hdr(usig, ppdu_info);
	} else {
		ppdu_info->uhr_usig = true;
		/* phy version = 1 if for UHR */
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_cmn(&usig->cmn, ppdu_info);
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_hdr(usig, ppdu_info);
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_aggr_tlv(struct hal_rx_mon_ppdu_info *ppdu_info,
			      u16 tlv_len, const void *tlv_data)
{
	if (tlv_len <= HAL_RX_MON_MAX_AGGR_SIZE - ppdu_info->tlv_aggr.cur_len) {
		memcpy(ppdu_info->tlv_aggr.buf + ppdu_info->tlv_aggr.cur_len,
		       tlv_data, tlv_len);
		ppdu_info->tlv_aggr.cur_len += tlv_len;
	}
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_frame_type_ndp(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == 1 &&
	    usig_info->eht_sig_mcs == 0 &&
	    usig_info->num_eht_sig_sym == 0)
		return true;

	return false;
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_non_ofdma(const struct hal_rx_u_sig_info *usig_info)
{
	u32 ppdu_type_comp_mode = usig_info->ppdu_type_comp_mode;
	u32 ul_dl = usig_info->ul_dl;

	if ((ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_MU_MIMO && ul_dl == 0) ||
	    (ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_MU_OFDMA && ul_dl == 0) ||
	    (ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_MU_MIMO  && ul_dl == 1))
		return true;

	return false;
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_ofdma(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == 0 && usig_info->ul_dl == 0)
		return true;

	return false;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_ndp(const struct hal_eht_sig_ndp_cmn_eb *eht_sig_ndp,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	u32 known, data;

	known = __le32_to_cpu(eht->known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_EHT_KNOWN_EHT_LTF |
		 IEEE80211_RADIOTAP_EHT_KNOWN_NSS_S |
		 IEEE80211_RADIOTAP_EHT_KNOWN_BEAMFORMED_S |
		 IEEE80211_RADIOTAP_EHT_KNOWN_DISREGARD_S |
		 IEEE80211_RADIOTAP_EHT_KNOWN_CRC1 |
		 IEEE80211_RADIOTAP_EHT_KNOWN_TAIL1;
	eht->known = cpu_to_le32(known);

	data = __le32_to_cpu(eht->data[0]);
	data |= ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_EHT_DATA0_SPATIAL_REUSE);
	/* GI and LTF size are separately indicated in radiotap header
	 * and hence will be parsed from other TLV
	 */
	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_EHT_DATA0_EHT_LTF);

	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_CRC,
				    IEEE80211_RADIOTAP_EHT_DATA0_CRC1_O);

	data |= ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_DISREGARD,
				    IEEE80211_RADIOTAP_EHT_DATA0_DISREGARD_S);
	eht->data[0] = cpu_to_le32(data);

	data = __le32_to_cpu(eht->data[7]);
	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_NSS,
				    IEEE80211_RADIOTAP_EHT_DATA7_NSS_S);

	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_BEAMFORMED,
				    IEEE80211_RADIOTAP_EHT_DATA7_BEAMFORMED_S);
	eht->data[7] = cpu_to_le32(data);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_usig_overflow(const struct hal_eht_sig_usig_overflow *ovflow,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	u32 known, data;

	known = __le32_to_cpu(eht->known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_EHT_KNOWN_EHT_LTF |
		 IEEE80211_RADIOTAP_EHT_KNOWN_LDPC_EXTRA_SYM_OM |
		 IEEE80211_RADIOTAP_EHT_KNOWN_PRE_PADD_FACOR_OM |
		 IEEE80211_RADIOTAP_EHT_KNOWN_PE_DISAMBIGUITY_OM |
		 IEEE80211_RADIOTAP_EHT_KNOWN_DISREGARD_O;
	eht->known = cpu_to_le32(known);

	data = __le32_to_cpu(eht->data[0]);
	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_EHT_DATA0_SPATIAL_REUSE);

	/* GI and LTF size are separately indicated in radiotap header
	 * and hence will be parsed from other TLV
	 */
	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_EHT_DATA0_EHT_LTF);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM,
				    IEEE80211_RADIOTAP_EHT_DATA0_LDPC_EXTRA_SYM_OM);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR,
				    IEEE80211_RADIOTAP_EHT_DATA0_PRE_PADD_FACOR_OM);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_DISAMBIGUITY,
				    IEEE80211_RADIOTAP_EHT_DATA0_PE_DISAMBIGUITY_OM);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_DISREGARD,
				    IEEE80211_RADIOTAP_EHT_DATA0_DISREGARD_O);
	eht->data[0] = cpu_to_le32(data);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_non_ofdma_users(const struct hal_eht_sig_non_ofdma_cmn_eb *eb,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	u32 known, data;

	known = __le32_to_cpu(eht->known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_NR_NON_OFDMA_USERS_M;
	eht->known = cpu_to_le32(known);

	data = __le32_to_cpu(eht->data[7]);
	data |=	ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_EHT_SIG_NON_OFDMA_INFO0_NUM_USERS,
				    IEEE80211_RADIOTAP_EHT_DATA7_NUM_OF_NON_OFDMA_USERS);
	eht->data[7] = cpu_to_le32(data);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_mumimo_user(const struct hal_eht_sig_mu_mimo *user,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_eht_info *eht_info = &ppdu_info->eht_info;
	u32 user_idx, known, data;

	if (eht_info->num_user_info >= ARRAY_SIZE(eht_info->user_info))
		return;

	user_idx = eht_info->num_user_info++;

	eht_info->user_info[user_idx] |=
		IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_CODING_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_SPATIAL_CONFIG_KNOWN_M |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_CODING,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_CODING) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_MCS) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_SPATIAL_CODING,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_SPATIAL_CONFIG_M);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_MCS);

	data = __le32_to_cpu(eht_info->eht.data[7]);
	data |= ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_CRC,
				    IEEE80211_RADIOTAP_EHT_DATA7_USER_ENCODING_BLOCK_CRC);
	eht_info->eht.data[7] = cpu_to_le32(data);

	known = __le32_to_cpu(eht_info->eht.known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_ENCODING_BLOCK_CRC_M |
		 IEEE80211_RADIOTAP_EHT_KNOWN_ENCODING_BLOCK_TAIL_M;
	eht_info->eht.known = cpu_to_le32(known);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_non_mumimo_user(const struct hal_eht_sig_non_mu_mimo *user,
					       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_eht_info *eht_info = &ppdu_info->eht_info;
	u32 user_idx, known, data;

	if (eht_info->num_user_info >= ARRAY_SIZE(eht_info->user_info))
		return;

	user_idx = eht_info->num_user_info++;

	eht_info->user_info[user_idx] |=
		IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_CODING_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_NSS_KNOWN_O |
		IEEE80211_RADIOTAP_EHT_USER_INFO_BEAMFORMING_KNOWN_O |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_CODING,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_CODING) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_MCS) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_NSS,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_NSS_O) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_BEAMFORMED,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_BEAMFORMING_O);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_MCS);

	ppdu_info->nss = le32_get_bits(user->info0,
				       HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_NSS) + 1;

	data = __le32_to_cpu(eht_info->eht.data[7]);
	data |= ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_CRC,
				    IEEE80211_RADIOTAP_EHT_DATA7_USER_ENCODING_BLOCK_CRC);
	eht_info->eht.data[7] = cpu_to_le32(data);

	known = __le32_to_cpu(eht_info->eht.known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_ENCODING_BLOCK_CRC_M |
		 IEEE80211_RADIOTAP_EHT_KNOWN_ENCODING_BLOCK_TAIL_M;
	eht_info->eht.known = cpu_to_le32(known);
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_mu_mimo_user(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_SU &&
	    usig_info->ul_dl == 1)
		return true;

	return false;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_non_ofdma(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_eht_sig_non_ofdma_cmn_eb *eb = tlv;
	u32 user_field;

	ath12k_wifi8_hal_mon_parse_usig_overflow(tlv, ppdu_info);
	ath12k_wifi8_hal_mon_parse_non_ofdma_users(eb, ppdu_info);

	user_field =
		(u32)(le64_get_bits(eb->info0,
				    HAL_RX_EHT_SIG_NON_OFDMA_INFO0_USER_FIELD));

	if (ath12k_wifi8_hal_mon_is_mu_mimo_user(&ppdu_info->u_sig_info)) {
		struct hal_eht_sig_mu_mimo mu_mimo = {
			.info0 = cpu_to_le32(user_field),
		};
		ath12k_wifi8_hal_mon_parse_eht_mumimo_user(&mu_mimo,
							   ppdu_info);
	} else {
		struct hal_eht_sig_non_mu_mimo n_mu_mimo = {
			.info0 = cpu_to_le32(user_field),
		};
		ath12k_wifi8_hal_mon_parse_eht_non_mumimo_user(&n_mu_mimo,
							       ppdu_info);
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_ru_allocation(const struct hal_eht_sig_ofdma_cmn_eb *eb,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_eht_sig_ofdma_cmn_eb1 *ofdma_cmn_eb1 = &eb->eb1;
	const struct hal_eht_sig_ofdma_cmn_eb2 *ofdma_cmn_eb2 = &eb->eb2;
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	enum ieee80211_radiotap_eht_data ru_123, ru_124, ru_125, ru_126;
	enum ieee80211_radiotap_eht_data ru_121, ru_122, ru_112, ru_111;
	u32 data;

	ru_123 = IEEE80211_RADIOTAP_EHT_DATA4_RU_ALLOC_CC_1_2_3;
	ru_124 = IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_4;
	ru_125 = IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_5;
	ru_126 = IEEE80211_RADIOTAP_EHT_DATA6_RU_ALLOC_CC_1_2_6;
	ru_121 = IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_1;
	ru_122 = IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_2;
	ru_112 = IEEE80211_RADIOTAP_EHT_DATA2_RU_ALLOC_CC_1_1_2;
	ru_111 = IEEE80211_RADIOTAP_EHT_DATA1_RU_ALLOC_CC_1_1_1;

	switch (ppdu_info->u_sig_info.bw) {
	case HAL_EHT_BW_320_2:
	case HAL_EHT_BW_320_1:
		data = __le32_to_cpu(eht->data[4]);
		/* CC1 2::3 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA4_RU_ALLOC_CC_1_2_3_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_3,
					    ru_123);
		eht->data[4] = cpu_to_le32(data);

		data = __le32_to_cpu(eht->data[5]);
		/* CC1 2::4 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_4_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_4,
					    ru_124);

		/* CC1 2::5 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_5_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_5,
					    ru_125);
		eht->data[5] = cpu_to_le32(data);

		data = __le32_to_cpu(eht->data[6]);
		/* CC1 2::6 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA6_RU_ALLOC_CC_1_2_6_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_6,
					    ru_126);
		eht->data[6] = cpu_to_le32(data);

		fallthrough;
	case HAL_EHT_BW_160:
		data = __le32_to_cpu(eht->data[3]);
		/* CC1 2::1 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_1,
					    ru_121);
		/* CC1 2::2 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_2,
					    ru_122);
		eht->data[3] = cpu_to_le32(data);

		fallthrough;
	case HAL_EHT_BW_80:
		data = __le32_to_cpu(eht->data[2]);
		/* CC1 1::2 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA2_RU_ALLOC_CC_1_1_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB1_RU_ALLOC_1_2,
					    ru_112);
		eht->data[2] = cpu_to_le32(data);

		fallthrough;
	case HAL_EHT_BW_40:
		fallthrough;
	case HAL_EHT_BW_20:
		data = __le32_to_cpu(eht->data[1]);
		/* CC1 1::1 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA1_RU_ALLOC_CC_1_1_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB1_RU_ALLOC_1_1,
					    ru_111);
		eht->data[1] = cpu_to_le32(data);
		break;
	default:
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_ofdma(const void *tlv,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_eht_sig_ofdma_cmn_eb *ofdma = tlv;

	ath12k_wifi8_hal_mon_parse_usig_overflow(tlv, ppdu_info);
	ath12k_wifi8_hal_mon_parse_ru_allocation(ofdma, ppdu_info);

	ath12k_wifi8_hal_mon_parse_eht_non_mumimo_user(&ofdma->user_field.n_mu_mimo,
						       ppdu_info);
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_frame_type_elr(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == 3)
		return true;

	return false;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_elr(const void *tlv,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_elr_sig *elr = tlv;

	struct hal_rx_uhr_elr_info *elr_info = &ppdu_info->elr_info;
	u32 known, sig1, sig2;

	ppdu_info->is_uhr_elr = true;

	known = __le32_to_cpu(elr_info->known);
	known |= IEEE80211_RADIOTAP_UHR_ELR_ELR_VERSION_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_UL_DL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_MCS_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_CODING_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_LENGTH_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_LDPC_EXTRA_SYM_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_TAIL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_STA_ID_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_DISREGARD_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_TAIL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC_CHECKED |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC_CHECKED;

	elr_info->known = cpu_to_le32(known);

	sig1 = __le32_to_cpu(elr_info->sig1);
	sig1 |=	ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_ELR_VERSION,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_ELR_VERSION) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_UL_DL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_UL_DL) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_ELR_MCS,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_MCS) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_CODING,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_CODING) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_LENGTH,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_LENGTH) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_LDPC_EXTRA_SYM,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_LDPC_EXTRA_SYM) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_CRC,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_TAIL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_TAIL);

	elr_info->sig1 = cpu_to_le32(sig1);

	sig2 = __le32_to_cpu(elr_info->sig2);
	sig2 |=	ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_STA_ID,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_STA_ID) |
		ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_DISREGARD,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_DISREGARD) |
		ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_CRC,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC) |
		ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_TAIL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_TAIL);

	elr_info->sig2 = cpu_to_le32(sig2);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma_cmn(const void *tlv,
						 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_uhr_sig_non_ofdma_cmn_eb *eb =
			(struct hal_uhr_sig_non_ofdma_cmn_eb *)tlv;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	struct hal_rx_radiotap_uhr *uhr = &uhr_info->uhr;
	u32 known, data, data7, non_ofdma_users_mask;

	known = __le32_to_cpu(uhr->known);

	known |= IEEE80211_RADIOTAP_UHR_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_GI_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_UHR_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_LDPC_EXTRA_SYM |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRE_PAD_FACTOR |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PE_DISAMBIGUITY |
		 IEEE80211_RADIOTAP_UHR_KNOWN_CRC1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_INTF_MITG_NO |
		 IEEE80211_RADIOTAP_UHR_KNOWN_NR_NON_OFDMA_USERS_NO |
		 IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_CRC_NO |
		 IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_TAIL_NO;

	data = __le32_to_cpu(uhr->data[0]);

	data |=	ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_UHR_DATA0_SPATIAL_REUSE) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_GI_LTF,
				    IEEE80211_RADIOTAP_UHR_DATA0_GI) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_UHR_LTF) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_LDPC_EXTRA_SYM_OM) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR,
				    IEEE80211_RADIOTAP_UHR_DATA0_PRE_PAD_FACTOR_OM) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_DISAMBIGUITY,
				    IEEE80211_RADIOTAP_UHR_DATA0_PE_DISAMBIGUITY_OM);

	put_unaligned_le32(data, &uhr->data[0]);

	known |= (IEEE80211_RADIOTAP_UHR_KNOWN_DISREGARD_NO |
		  IEEE80211_RADIOTAP_UHR_KNOWN_INTF_MITG_NO |
		  IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_CRC_NO |
		  IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_TAIL_NO);

	put_unaligned_le32(known, &uhr->known);

	non_ofdma_users_mask = IEEE80211_RADIOTAP_UHR_DATA7_NUM_OF_NON_OFDMA_USERS;

	data7 = __le32_to_cpu(uhr->data[7]);

	data7 |= ATH12K_LE32_DEC_ENC(eb->info0,
				     HAL_RX_UHR_SIG_NON_OFDMA_INFO0_NUM_USERS,
				     non_ofdma_users_mask) |
		 ATH12K_LE32_DEC_ENC(eb->info0,
				     HAL_RX_UHR_SIG_NON_OFDMA_INFO0_IM_INDICATION,
				     IEEE80211_RADIOTAP_UHR_DATA7_INTF_MIT_NO) |
		 ATH12K_LE32_DEC_ENC(eb->info0,
				     HAL_RX_UHR_SIG_NON_OFDMA_INFO0_DISREGARD,
				     IEEE80211_RADIOTAP_UHR_DATA7_DISREGARD_NO);

	ppdu_info->num_non_ofdma_users =
		le32_get_bits(eb->info0,
			      HAL_RX_UHR_SIG_NON_OFDMA_INFO0_NUM_USERS);
	put_unaligned_le32(data7, &uhr->data[7]);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_mumimo_user(const struct hal_uhr_sig_mu_mimo *user,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	u32 user_idx, user_known = 0, user_data = 0;

	if (uhr_info->num_user_info >= UHR_MAX_USER_INFO)
		return;

	user_idx = uhr_info->num_user_info++;

	user_known = __le32_to_cpu(uhr_info->user_known[user_idx]);

	user_known |=
		IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_KNOWN_O |
		IEEE80211_RADIOTAP_UHR_USER_INFO_SPATIAL_CONFIG_KNOWN_NO |
		IEEE80211_RADIOTAP_UHR_USER_INFO_DISREGARD_KNOWN_NO |
		IEEE80211_RADIOTAP_UHR_USER_INFO_BSS_COLOR_IND_KNOWN_NO |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN;

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);

	user_data |=
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_SPATIAL_CONFIG,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_SPATIAL_CONFIG_NO) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_CODING,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_LDPC_MODE,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_NO);

	put_unaligned_le32(user_data, &uhr_info->user_info[user_idx]);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_MCS);
	ppdu_info->ldpc = le32_get_bits(user->info0,
					HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_LDPC_MODE);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_non_mumimo_user(const struct hal_uhr_sig_non_mu_mimo *user,
					       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	u32 user_idx, user_known = 0, user_data = 0;
	u8 ueqm, ldpc;
	u32 hal_mod_pat_m, mon_pat_mask, hal_uemq_m, uemq_mask;
	u32 hal_beamform_m, beamform_mask;
	u32 hal_coding_m, coding_mask;
	u32 hal_2xldpc_m, ldpc_mask;

	if (uhr_info->num_user_info >= UHR_MAX_USER_INFO)
		return;

	hal_mod_pat_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_MOD_PATTERN;
	mon_pat_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_O;
	hal_uemq_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_UNEQ_MOD;
	uemq_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_O;
	hal_beamform_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_BEAMFORMED;
	beamform_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_NO;
	hal_coding_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_CODING;
	coding_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO;
	hal_2xldpc_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_LDPC;
	ldpc_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_NO;

	user_idx = uhr_info->num_user_info++;

	user_known = __le32_to_cpu(uhr_info->user_known[user_idx]);

	user_known |=
		IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_KNOWN_O |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_CRC,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC);

	user_data |=
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		u32_encode_bits(le32_get_bits(user->info0,
					      HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_NSS)
					      + 1,
					      IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_O);

	ueqm = le32_get_bits(user->info0,
			     HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_UNEQ_MOD);

	if (ueqm) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_KNOWN_O;
		user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_mod_pat_m,
					    mon_pat_mask);
		user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_uemq_m,
					    uemq_mask);
	} else {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_KNOWN_O;

			user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_beamform_m,
					    beamform_mask);

		user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_coding_m,
					    coding_mask);
	}

	ldpc = le32_get_bits(user->info0,
			     HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_LDPC);

	if (ldpc) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_KNOWN_O;
		user_data |= ATH12K_LE32_DEC_ENC(user->info0,
						 hal_2xldpc_m,
						 ldpc_mask);
	}

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);
	put_unaligned_le32(user_data, &uhr_info->user_info[user_idx]);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_MCS);
	ppdu_info->nss = le32_get_bits(user->info0,
				       HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_NSS) + 1;
	ppdu_info->ldpc = le32_get_bits(user->info0,
					HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_LDPC);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_mumimo_su(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_uhr_sig_mumimo_su_eb *user = (struct hal_uhr_sig_mumimo_su_eb *)tlv;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	u32 user_idx, user_known = 0, user_data = 0;
	u32 ueqm, ldpc, sta_id, mod_pat, beamformed, beamformed_mask, coding, coding_mask;
	u32 ldpc_mask, mod_pat_mask, uemq_mask;
	u64 hal_mod_pat_m, hal_beamformed_m;
	u64 hal_coding_m, hal_ldpc_m;

	hal_mod_pat_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_MOD_PATTERN;
	mod_pat_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_O;
	hal_beamformed_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_BEAMFORMED;
	beamformed_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_NO;
	hal_ldpc_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_LDPC;
	ldpc_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_NO;
	uemq_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_O;
	hal_coding_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_CODING;
	coding_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO;

	if (uhr_info->num_user_info >= UHR_MAX_USER_INFO)
		return;

	user_idx = uhr_info->num_user_info++;

	user_known = __le32_to_cpu(uhr_info->user_known[user_idx]);

	user_known |=
		IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_KNOWN_O |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN;

	sta_id = le64_get_bits(user->info0,
			       HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_STA_ID);
	ppdu_info->mcs = le64_get_bits(user->info0,
				       HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_MCS);
	ppdu_info->nss = le64_get_bits(user->info0,
				       HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_NSS) + 1;
	ppdu_info->ldpc = le64_get_bits(user->info0,
					HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_LDPC);
	user_data |= u32_encode_bits(sta_id,
				     IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		u32_encode_bits(ppdu_info->mcs, IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		u32_encode_bits(le64_get_bits(user->info0,
					      HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_NSS)
					      + 1,
					      IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_O);

	ueqm = le64_get_bits(user->info0,
			     HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_UNEQ_MOD);

	if (ueqm) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_KNOWN_O;
		mod_pat = le64_get_bits(user->info0,
					hal_mod_pat_m);
		user_data |= u32_encode_bits(mod_pat, mod_pat_mask) |
			     u32_encode_bits(ueqm, uemq_mask);
	} else {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_KNOWN_O |
			IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_KNOWN_O;

			beamformed = le64_get_bits(user->info0,
						   hal_beamformed_m);
			coding = le64_get_bits(user->info0,
					       hal_coding_m);
			user_data |= u32_encode_bits(beamformed, beamformed_mask) |
				     u32_encode_bits(coding, coding_mask);
	}

	ldpc = le64_get_bits(user->info0,
			     HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_LDPC);

	if (ldpc) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_KNOWN_O;
		user_data |= u32_encode_bits(ldpc, ldpc_mask);
	}

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);
	put_unaligned_le32(user_data, &uhr_info->user_info[user_idx]);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_non_ofdma_cmn_eb *eb = tlv;

	ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma_cmn(tlv, ppdu_info);

	if (ppdu_info->num_non_ofdma_users)
		ath12k_wifi8_hal_mon_parse_uhr_mumimo_user(&eb->user_field.mu_mimo,
							   ppdu_info);
	else
		ath12k_wifi8_hal_mon_parse_uhr_sig_mumimo_su(eb,
							     ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma_cmn(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_ofdma_cmn_eb *eb = tlv;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	struct hal_rx_radiotap_uhr *uhr = &uhr_info->uhr;
	u32 known, data, data7;

	known = __le32_to_cpu(uhr->known);

	known |= IEEE80211_RADIOTAP_UHR_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_GI_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_UHR_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_LDPC_EXTRA_SYM |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRE_PAD_FACTOR |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PE_DISAMBIGUITY |
		 IEEE80211_RADIOTAP_UHR_KNOWN_DISREGARD_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_CRC1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_CRC2_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL2_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_RU_MRU_SIZE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_RU_MRU_INDEX |
		 IEEE80211_RADIOTAP_UHR_KNOWN_RU_ALLOC_TB_FMT_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRIMARY_80;

	data = __le32_to_cpu(uhr->data[0]);

	data |=	ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_UHR_DATA0_SPATIAL_REUSE) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_GI_LTF,
				    IEEE80211_RADIOTAP_UHR_DATA0_GI) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_UHR_LTF) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_LDPC_EXTRA_SYM_OM) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR,
				    IEEE80211_RADIOTAP_UHR_DATA0_PRE_PAD_FACTOR_OM) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_DISAMBIGUITY,
				    IEEE80211_RADIOTAP_UHR_DATA0_PE_DISAMBIGUITY_OM);

	put_unaligned_le32(data, &uhr->data[0]);

	data7 = __le32_to_cpu(uhr->data[7]);

	data7 |= ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_NON_OFDMA_INFO0_NUM_USERS,
				    IEEE80211_RADIOTAP_UHR_DATA7_NUM_OF_NON_OFDMA_USERS) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_NON_OFDMA_INFO0_IM_INDICATION,
				    IEEE80211_RADIOTAP_UHR_DATA7_INTF_MIT_NO);

	put_unaligned_le32(data7, &uhr->data[7]);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_ru_allocation(const struct hal_uhr_sig_ofdma_cmn_eb *eb,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_ofdma_cmn_eb1 *ofdma_cmn_eb1 = &eb->eb1;
	const struct hal_uhr_sig_ofdma_cmn_eb2 *ofdma_cmn_eb2 = &eb->eb2;
	struct hal_rx_radiotap_uhr *uhr = &ppdu_info->uhr_info.uhr;
	enum ieee80211_radiotap_uhr_data ru_123, ru_124, ru_125, ru_126;
	enum ieee80211_radiotap_uhr_data ru_121, ru_122, ru_112, ru_111;
	u32 data;

	ru_123 = IEEE80211_RADIOTAP_UHR_DATA4_RU_ALLOC_CC_1_2_3;
	ru_124 = IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_4;
	ru_125 = IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_5;
	ru_126 = IEEE80211_RADIOTAP_UHR_DATA6_RU_ALLOC_CC_1_2_6;
	ru_121 = IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_1;
	ru_122 = IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_2;
	ru_112 = IEEE80211_RADIOTAP_UHR_DATA2_RU_ALLOC_CC_1_1_2;
	ru_111 = IEEE80211_RADIOTAP_UHR_DATA1_RU_ALLOC_CC_1_1_1;

	switch (ppdu_info->u_sig_info.bw) {
	case HAL_EHT_BW_320_2:
	case HAL_EHT_BW_320_1:
		data = __le32_to_cpu(uhr->data[4]);
		/* CC1 2::3 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA4_RU_ALLOC_CC_1_2_3_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_3,
					    ru_123);
		uhr->data[4] = cpu_to_le32(data);

		data = __le32_to_cpu(uhr->data[5]);
		/* CC1 2::4 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_4_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_4,
					    ru_124);

		/* CC1 2::5 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_5_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_5,
					    ru_125);
		uhr->data[5] = cpu_to_le32(data);

		data = __le32_to_cpu(uhr->data[6]);
		/* CC1 2::6 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA6_RU_ALLOC_CC_1_2_6_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_6,
					    ru_126);
		uhr->data[6] = cpu_to_le32(data);

		fallthrough;
	case HAL_EHT_BW_160:
		data = __le32_to_cpu(uhr->data[3]);
		/* CC1 2::1 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_1,
					    ru_121);
		/* CC1 2::2 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_2,
					    ru_122);
		uhr->data[3] = cpu_to_le32(data);

		fallthrough;
	case HAL_EHT_BW_80:
		data = __le32_to_cpu(uhr->data[2]);
		/* CC1 1::2 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA2_RU_ALLOC_CC_1_1_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB1_RU_ALLOC_1_2,
					    ru_112);
		uhr->data[2] = cpu_to_le32(data);

		fallthrough;
	case HAL_EHT_BW_40:
		fallthrough;
	case HAL_EHT_BW_20:
		data = __le32_to_cpu(uhr->data[1]);
		/* CC1 1::1 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA1_RU_ALLOC_CC_1_1_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB1_RU_ALLOC_1_1,
					    ru_111);
		uhr->data[1] = cpu_to_le32(data);
		break;
	default:
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma(const void *tlv,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_ofdma_cmn_eb *ofdma = tlv;

	ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma_cmn(tlv, ppdu_info);
	ath12k_wifi8_hal_mon_parse_uhr_ru_allocation(ofdma, ppdu_info);

	ath12k_wifi8_hal_mon_parse_uhr_non_mumimo_user(&ofdma->user_field.n_mu_mimo,
						       ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_hdr(struct hal_rx_mon_ppdu_info *ppdu_info,
				       const void *tlv_data)
{
	ppdu_info->is_uhr = true;

	if (ath12k_wifi8_hal_mon_is_frame_type_elr(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_uhr_sig_elr(tlv_data, ppdu_info);
	else if (ath12k_wifi8_hal_mon_is_non_ofdma(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma(tlv_data, ppdu_info);
	else
		ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma(tlv_data, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_hdr(struct hal_rx_mon_ppdu_info *ppdu_info,
				       const void *tlv_data)
{
	ppdu_info->is_eht = true;

	if (ath12k_wifi8_hal_mon_is_frame_type_ndp(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_sig_ndp(tlv_data, ppdu_info);
	else if (ath12k_wifi8_hal_mon_is_non_ofdma(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_sig_non_ofdma(tlv_data, ppdu_info);
	else if (ath12k_wifi8_hal_mon_is_ofdma(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_sig_ofdma(tlv_data, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_uhr_sig_hdr(struct hal_rx_mon_ppdu_info *ppdu_info,
					   const void *tlv_data)
{
	if (ppdu_info->u_sig_info.phy_version == HAL_EHT_PHY)
		ath12k_wifi8_hal_mon_parse_eht_sig_hdr(ppdu_info, tlv_data);
	else
		ath12k_wifi8_hal_mon_parse_uhr_sig_hdr(ppdu_info, tlv_data);
}

static __always_inline enum ath12k_eht_ru_size
ath12k_wifi8_hal_mon_hal_ru_size_to_ath12k_ru_size(u32 hal_ru_size)
{
	switch (hal_ru_size) {
	case HAL_EHT_RU_26:
		return ATH12K_EHT_RU_26;
	case HAL_EHT_RU_52:
		return ATH12K_EHT_RU_52;
	case HAL_EHT_RU_78:
		return ATH12K_EHT_RU_52_26;
	case HAL_EHT_RU_106:
		return ATH12K_EHT_RU_106;
	case HAL_EHT_RU_132:
		return ATH12K_EHT_RU_106_26;
	case HAL_EHT_RU_242:
		return ATH12K_EHT_RU_242;
	case HAL_EHT_RU_484:
		return ATH12K_EHT_RU_484;
	case HAL_EHT_RU_726:
		return ATH12K_EHT_RU_484_242;
	case HAL_EHT_RU_996:
		return ATH12K_EHT_RU_996;
	case HAL_EHT_RU_996x2:
		return ATH12K_EHT_RU_996x2;
	case HAL_EHT_RU_996x3:
		return ATH12K_EHT_RU_996x3;
	case HAL_EHT_RU_996x4:
		return ATH12K_EHT_RU_996x4;
	case HAL_EHT_RU_NONE:
		return ATH12K_EHT_RU_INVALID;
	case HAL_EHT_RU_996_484:
		return ATH12K_EHT_RU_996_484;
	case HAL_EHT_RU_996x2_484:
		return ATH12K_EHT_RU_996x2_484;
	case HAL_EHT_RU_996x3_484:
		return ATH12K_EHT_RU_996x3_484;
	case HAL_EHT_RU_996_484_242:
		return ATH12K_EHT_RU_996_484_242;
	default:
		return ATH12K_EHT_RU_INVALID;
	}
}

static __always_inline u32
ath12k_wifi8_hal_mon_ul_ofdma_ru_size_to_width(enum ath12k_eht_ru_size ru_size)
{
	switch (ru_size) {
	case ATH12K_EHT_RU_26:
		return RU_26;
	case ATH12K_EHT_RU_52:
		return RU_52;
	case ATH12K_EHT_RU_52_26:
		return RU_52_26;
	case ATH12K_EHT_RU_106:
		return RU_106;
	case ATH12K_EHT_RU_106_26:
		return RU_106_26;
	case ATH12K_EHT_RU_242:
		return RU_242;
	case ATH12K_EHT_RU_484:
		return RU_484;
	case ATH12K_EHT_RU_484_242:
		return RU_484_242;
	case ATH12K_EHT_RU_996:
		return RU_996;
	case ATH12K_EHT_RU_996_484:
		return RU_996_484;
	case ATH12K_EHT_RU_996_484_242:
		return RU_996_484_242;
	case ATH12K_EHT_RU_996x2:
		return RU_2X996;
	case ATH12K_EHT_RU_996x2_484:
		return RU_2X996_484;
	case ATH12K_EHT_RU_996x3:
		return RU_3X996;
	case ATH12K_EHT_RU_996x3_484:
		return RU_3X996_484;
	case ATH12K_EHT_RU_996x4:
		return RU_4X996;
	default:
		return RU_INVALID;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_user_info(const struct hal_receive_user_info *rx_usr_info,
				     u16 user_id,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *mon_rx_user_status = NULL;
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	enum ath12k_eht_ru_size rtap_ru_size = ATH12K_EHT_RU_INVALID;
	u32 ru_width, reception_type, ru_index = HAL_EHT_RU_INVALID;
	u32 ru_type_80_0, ru_start_index_80_0;
	u32 ru_type_80_1, ru_start_index_80_1;
	u32 ru_type_80_2, ru_start_index_80_2;
	u32 ru_type_80_3, ru_start_index_80_3;
	u32 ru_size = 0, num_80mhz_with_ru = 0;
	u64 ru_index_320mhz = 0;
	u32 ru_index_per80mhz;

	reception_type = le16_get_bits(rx_usr_info->info0,
				       HAL_RX_USR_INFO0_RECEPTION_TYPE);

	switch (reception_type) {
	case HAL_RECEPTION_TYPE_SU:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_MIMO:
	case HAL_RECEPTION_TYPE_UL_MU_MIMO:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_MIMO;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_OFMA:
	case HAL_RECEPTION_TYPE_UL_MU_OFDMA:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_OFDMA;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO:
	case HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO;
	}

	ppdu_info->is_stbc = le16_get_bits(rx_usr_info->info0, HAL_RX_USR_INFO0_STBC);
	ppdu_info->ldpc = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_LDPC);
	ppdu_info->dcm = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_STA_DCM);
	ppdu_info->bw = le32_get_bits(rx_usr_info->info1, HAL_RX_USR_INFO1_RX_BW);
	ppdu_info->mcs = le32_get_bits(rx_usr_info->info1, HAL_RX_USR_INFO1_MCS);
	ppdu_info->nss = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_NSS) + 1;

	if (user_id < HAL_MAX_UL_MU_USERS) {
		mon_rx_user_status = &ppdu_info->userstats[user_id];
		mon_rx_user_status->mcs = ppdu_info->mcs;
		mon_rx_user_status->nss = ppdu_info->nss;
	}

	if (!(ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO))
		return;

	/* RU allocation present only for OFDMA reception */
	ru_type_80_0 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_0);
	ru_start_index_80_0 = le32_get_bits(rx_usr_info->info3,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_0);
	if (ru_type_80_0 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_0;
		ru_index_per80mhz = ru_start_index_80_0;
		ru_index = ru_index_per80mhz;
		if (ru_type_80_0 >= HAL_EHT_RU_484)
			ru_index_320mhz |= HAL_RU_PER80(ru_type_80_0, 0,
							ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	ru_type_80_1 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_1);
	ru_start_index_80_1 = le32_get_bits(rx_usr_info->info3,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_1);
	if (ru_type_80_1 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_1;
		ru_index_per80mhz = ru_start_index_80_1;
		ru_index = ru_index_per80mhz;
		if (ru_type_80_1 >= HAL_EHT_RU_484)
			ru_index_320mhz |= HAL_RU_PER80(ru_type_80_1, 1,
							ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	ru_type_80_2 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_2);
	ru_start_index_80_2 = le32_get_bits(rx_usr_info->info3,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_2);
	if (ru_type_80_2 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_2;
		ru_index_per80mhz = ru_start_index_80_2;
		ru_index = ru_index_per80mhz;
		if (ru_type_80_2 >= HAL_EHT_RU_484)
			ru_index_320mhz |= HAL_RU_PER80(ru_type_80_2, 2,
							ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	ru_type_80_3 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_3);
	ru_start_index_80_3 = le32_get_bits(rx_usr_info->info2,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_3);
	if (ru_type_80_3 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_3;
		ru_index_per80mhz = ru_start_index_80_3;
		ru_index = ru_index_per80mhz;
		if (ru_type_80_3 >= HAL_EHT_RU_484)
			ru_index_320mhz |= HAL_RU_PER80(ru_type_80_3, 3,
							ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	if (num_80mhz_with_ru > 1) {
		/* Calculate the MRU index */
		switch (ru_index_320mhz) {
		case HAL_EHT_RU_996_484_0:
		case HAL_EHT_RU_996x2_484_0:
		case HAL_EHT_RU_996x3_484_0:
			ru_index = 0;
			break;
		case HAL_EHT_RU_996_484_1:
		case HAL_EHT_RU_996x2_484_1:
		case HAL_EHT_RU_996x3_484_1:
			ru_index = 1;
			break;
		case HAL_EHT_RU_996_484_2:
		case HAL_EHT_RU_996x2_484_2:
		case HAL_EHT_RU_996x3_484_2:
			ru_index = 2;
			break;
		case HAL_EHT_RU_996_484_3:
		case HAL_EHT_RU_996x2_484_3:
		case HAL_EHT_RU_996x3_484_3:
			ru_index = 3;
			break;
		case HAL_EHT_RU_996_484_4:
		case HAL_EHT_RU_996x2_484_4:
		case HAL_EHT_RU_996x3_484_4:
			ru_index = 4;
			break;
		case HAL_EHT_RU_996_484_5:
		case HAL_EHT_RU_996x2_484_5:
		case HAL_EHT_RU_996x3_484_5:
			ru_index = 5;
			break;
		case HAL_EHT_RU_996_484_6:
		case HAL_EHT_RU_996x2_484_6:
		case HAL_EHT_RU_996x3_484_6:
			ru_index = 6;
			break;
		case HAL_EHT_RU_996_484_7:
		case HAL_EHT_RU_996x2_484_7:
		case HAL_EHT_RU_996x3_484_7:
			ru_index = 7;
			break;
		case HAL_EHT_RU_996x2_484_8:
			ru_index = 8;
			break;
		case HAL_EHT_RU_996x2_484_9:
			ru_index = 9;
			break;
		case HAL_EHT_RU_996x2_484_10:
			ru_index = 10;
			break;
		case HAL_EHT_RU_996x2_484_11:
			ru_index = 11;
			break;
		default:
			ru_index = HAL_EHT_RU_INVALID;
			break;
		}

		ru_size += 4;
	}

	rtap_ru_size = ath12k_wifi8_hal_mon_hal_ru_size_to_ath12k_ru_size(ru_size);
	if (rtap_ru_size != ATH12K_EHT_RU_INVALID) {
		u32 known, data;

		known = __le32_to_cpu(eht->known);
		known |= IEEE80211_RADIOTAP_EHT_KNOWN_RU_MRU_SIZE_OM;
		eht->known = cpu_to_le32(known);

		data = __le32_to_cpu(eht->data[1]);
		data |=	u32_encode_bits(rtap_ru_size,
					IEEE80211_RADIOTAP_EHT_DATA1_RU_SIZE);
		eht->data[1] = cpu_to_le32(data);
	}

	if (ru_index != HAL_EHT_RU_INVALID) {
		u32 known, data;

		known = __le32_to_cpu(eht->known);
		known |= IEEE80211_RADIOTAP_EHT_KNOWN_RU_MRU_INDEX_OM;
		eht->known = cpu_to_le32(known);

		data = __le32_to_cpu(eht->data[1]);
		data |=	u32_encode_bits(rtap_ru_size,
					IEEE80211_RADIOTAP_EHT_DATA1_RU_INDEX);
		eht->data[1] = cpu_to_le32(data);
	}

	if (mon_rx_user_status && ru_index != HAL_EHT_RU_INVALID &&
	    rtap_ru_size != ATH12K_EHT_RU_INVALID) {
		mon_rx_user_status->ul_ofdma_ru_start_index = ru_index;
		mon_rx_user_status->ul_ofdma_ru_size = rtap_ru_size;

		ru_width = ath12k_wifi8_hal_mon_ul_ofdma_ru_size_to_width(rtap_ru_size);

		mon_rx_user_status->ul_ofdma_ru_width = ru_width;
		mon_rx_user_status->ofdma_info_valid = 1;
	}
}

static void
ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode(struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 user_id = ppdu_info->user_id;

	/* set decap type to raw mode for management frames, control frames
	 * and NULL data frames
	 */
	if (ppdu_info->nrp_info.fc_valid && ppdu_info->mpdu_info[user_id].decap_type) {
		if (ieee80211_is_mgmt(ppdu_info->nrp_info.frame_control) ||
		    ieee80211_is_ctl(ppdu_info->nrp_info.frame_control) ||
		    ppdu_info->grp_id == HAL_MPDU_START_SW_FRAME_GRP_NULL_DATA) {
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		}
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get(const void *tlv_data, u32 userid,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mpdu_start *mpdu_start =
		(struct hal_rx_mpdu_start *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[5], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = le16_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);

	ppdu_info->grp_id = u32_get_bits(info[2],
					 HAL_RX_MPDU_START_INFO2_SW_GRP_ID);

	peer_id = le16_to_cpu(mpdu_start->sw_peer_id);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_TO_DS);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_FC_VALID);

	ppdu_info->mpdu_info[user_id].raw_mpdu =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_RAW_MPDU);
	ppdu_info->mpdu_info[user_id].decap_type =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_DECAP_TYPE);

	ppdu_info->mpdu_len += u32_get_bits(info[4],
					    HAL_RX_MPDU_START_INFO4_MPDU_LEN);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_MCAST_BCAST);
	ppdu_info->nrp_info.frame_control =
		le16_to_cpu(mpdu_start->mpdu_frame_control_field);
	addr_16 = le16_to_cpu(mpdu_start->mac_addr_ad2_15_0);
	addr_32 = le32_to_cpu(mpdu_start->mac_addr_ad2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr(addr_16, addr_32,
						      ppdu_info->nrp_info.mac_addr2);

	ppdu_info->userstats[user_id].sw_peer_id = ppdu_info->peer_id;
	ppdu_info->userstats[user_id].ampdu_id = le16_to_cpu(mpdu_start->phy_ppdu_id);
	ppdu_info->userstats[user_id].filter_category =
		u32_get_bits(info[2],
			     HAL_RX_MPDU_START_INFO2_FILTER_CAT);
	ppdu_info->userstats[user_id].mpdu_retry +=
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
	ppdu_info->userstats[user_id].frame_control_info_valid =
			ppdu_info->nrp_info.fc_valid;
	ppdu_info->userstats[user_id].frame_control =
			ppdu_info->nrp_info.frame_control;

	ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode(ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact(
					const void *tlv_data, u32 userid,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_mpdu_start_compact *mpdu_start =
		(struct hal_rx_mon_mpdu_start_compact *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[5], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = le16_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);

	ppdu_info->grp_id = u32_get_bits(info[2],
					 HAL_RX_MPDU_START_INFO2_SW_GRP_ID_CMPCT);

	peer_id = le16_to_cpu(mpdu_start->sw_peer_id);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID_CMPCT);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_TO_DS_CMPCT);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_FC_VALID_CMPCT);
	ppdu_info->mpdu_info[user_id].raw_mpdu =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_RAW_MPDU_CMPCT);
	ppdu_info->mpdu_info[user_id].decap_type =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_DECAP_TYPE_CMPCT);

	ppdu_info->mpdu_len += u32_get_bits(info[4],
					    HAL_RX_MPDU_START_INFO4_MPDU_LEN_CMPCT);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_MCAST_BCAST_CMPCT);
	ppdu_info->nrp_info.frame_control =
		le16_to_cpu(mpdu_start->mpdu_frame_control_field);

	addr_16 = le16_to_cpu(mpdu_start->mac_addr_ad2_15_0);
	addr_32 = le32_to_cpu(mpdu_start->mac_addr_ad2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr(addr_16, addr_32,
						      ppdu_info->nrp_info.mac_addr2);

	ppdu_info->userstats[user_id].sw_peer_id = ppdu_info->peer_id;
	ppdu_info->userstats[user_id].ampdu_id = le16_to_cpu(mpdu_start->phy_ppdu_id);
	ppdu_info->userstats[user_id].filter_category =
		u32_get_bits(info[2],
			     HAL_RX_MPDU_START_INFO2_FILTER_CAT_CMPCT);
	ppdu_info->userstats[user_id].mpdu_retry +=
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
	ppdu_info->userstats[user_id].frame_control_info_valid =
			ppdu_info->nrp_info.fc_valid;
	ppdu_info->userstats[user_id].frame_control =
			ppdu_info->nrp_info.frame_control;

	ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode(ppdu_info);
}

void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse(const void *tlv_data, u32 userid,
					      struct hal_rx_mon_ppdu_info *ppdu_info,
					      u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_MPDU_START_TLV_SIZE))
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact(tlv_data, userid,
								    ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get(tlv_data, userid,
							    ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_msdu_end_info_get(const void *tlv_data, u32 userid,
					  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_msdu_end *msdu_end =
		(struct hal_rx_msdu_end *)tlv_data;
	u32 info[3];

	info[0] = __le32_to_cpu(msdu_end->info0);
	info[1] = __le32_to_cpu(msdu_end->info1);
	info[2] = __le32_to_cpu(msdu_end->info2);

	ppdu_info->grp_id = u32_get_bits(info[0],
					 HAL_RX_MSDU_END_INFO0_SW_FRAME_GRP_ID);

	ppdu_info->decap_format = u32_get_bits(info[1],
					       HAL_RX_MSDU_END_INFO1_DECAP_FORMAT);
	ath12k_wifi8_hal_mon_parse_rx_msdu_end_err(info[2],
						   &ppdu_info->errmap);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_msdu_end_info_get_compact(const void *tlv_data, u32 userid,
						  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_msdu_end_compact *msdu_end =
		(struct hal_rx_mon_msdu_end_compact *)tlv_data;
	u32 info[3];

	info[0] = __le32_to_cpu(msdu_end->info0);
	info[1] = __le32_to_cpu(msdu_end->info1);
	info[2] = __le32_to_cpu(msdu_end->info2);

	ppdu_info->grp_id = u32_get_bits(info[0],
					 HAL_RX_MSDU_END_INFO0_SW_FRAME_GRP_ID_CMPCT);

	ppdu_info->decap_format = u32_get_bits(info[1],
					       HAL_RX_MSDU_END_INFO1_DECAP_FORMAT_CMPCT);
	ath12k_wifi8_hal_mon_parse_rx_msdu_end_err(info[2],
						   &ppdu_info->errmap);
}

void
ath12k_wifi8_hal_mon_rx_msdu_end_info_parse(const void *tlv_data, u32 userid,
					    struct hal_rx_mon_ppdu_info *ppdu_info,
					    u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_MSDU_END_TLV_SIZE))
		ath12k_wifi8_hal_mon_rx_msdu_end_info_get_compact(tlv_data, userid,
								  ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_msdu_end_info_get(tlv_data, userid,
							  ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get(const void *tlv_data, u32 userid,
					       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_ppdu_end_user_stats *ppdu_eu_stats =
		(struct hal_rx_ppdu_end_user_stats *)tlv_data;
	u32 tid_bitmap;
	u32 info[6];

	info[1] = __le32_to_cpu(ppdu_eu_stats->info1);
	info[2] = __le32_to_cpu(ppdu_eu_stats->info2);
	info[3] = __le32_to_cpu(ppdu_eu_stats->info3);
	info[4] = __le32_to_cpu(ppdu_eu_stats->info4);
	info[5] = __le32_to_cpu(ppdu_eu_stats->info5);

	ppdu_info->num_mpdu_fcs_err =
		u32_get_bits(info[1],
			     HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR);
	ppdu_info->peer_id = le16_to_cpu(ppdu_eu_stats->sw_peer_id);
	ppdu_info->fc_valid =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID);
	ppdu_info->preamble_type =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE);
	ppdu_info->num_mpdu_fcs_ok =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK);
	ppdu_info->ast_index = le16_to_cpu(ppdu_eu_stats->ast_index);
	ppdu_info->tcp_msdu_count = le16_to_cpu(ppdu_eu_stats->tcp_msdu_count);
	ppdu_info->udp_msdu_count = le16_to_cpu(ppdu_eu_stats->udp_msdu_count);
	ppdu_info->other_msdu_count = le16_to_cpu(ppdu_eu_stats->other_msdu_count);
	ppdu_info->tcp_ack_msdu_count = le16_to_cpu(ppdu_eu_stats->tcp_ack_msdu_count);
	tid_bitmap = le16_to_cpu(ppdu_eu_stats->received_qos_data_tid_bitmap);
	ppdu_info->tid = ffs(tid_bitmap) - 1;
	ppdu_info->mpdu_retry_cnt =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_MPDU_RETRY_CNT);
	ppdu_info->retried_msdu_count = le16_to_cpu(ppdu_eu_stats->retried_msdu_count);

	switch (ppdu_info->preamble_type) {
	case HAL_RX_PREAMBLE_11N:
		ppdu_info->ht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AC:
		ppdu_info->vht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AX:
		ppdu_info->he_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11BE:
		ppdu_info->is_eht = true;
		break;
	case HAL_RX_PREAMBLE_11BN:	/* UHR */
		ppdu_info->is_uhr = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];
		u32 resp_ref = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref);
		u32 resp_ref_ext = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref_ext);

		ath12k_wifi8_hal_mon_handle_ofdma_info(resp_ref, resp_ref_ext,
						       rxuser_stats);
		ath12k_wifi8_hal_mon_populate_byte_count(info,
							 rxuser_stats);
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get_compact(
						const void *tlv_data, u32 userid,
						struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_ppdu_end_user_stats_compact *ppdu_eu_stats =
		(struct hal_rx_mon_ppdu_end_user_stats_compact *)tlv_data;
	u32 tid_bitmap;
	u32 info[6];

	info[1] = __le32_to_cpu(ppdu_eu_stats->info1);
	info[2] = __le32_to_cpu(ppdu_eu_stats->info2);
	info[3] = __le32_to_cpu(ppdu_eu_stats->info3);
	info[4] = __le32_to_cpu(ppdu_eu_stats->info4);
	info[5] = __le32_to_cpu(ppdu_eu_stats->info5);

	ppdu_info->num_mpdu_fcs_err =
		u32_get_bits(info[1],
			     HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR_CMPCT);
	ppdu_info->peer_id = le16_to_cpu(ppdu_eu_stats->sw_peer_id);
	ppdu_info->fc_valid =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID_CMPCT);
	ppdu_info->preamble_type =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE_CMPCT);
	ppdu_info->num_mpdu_fcs_ok =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK_CMPCT);
	ppdu_info->ast_index = le16_to_cpu(ppdu_eu_stats->ast_index);
	ppdu_info->tcp_msdu_count = le16_to_cpu(ppdu_eu_stats->tcp_msdu_count);
	ppdu_info->udp_msdu_count = le16_to_cpu(ppdu_eu_stats->udp_msdu_count);
	ppdu_info->other_msdu_count = le16_to_cpu(ppdu_eu_stats->other_msdu_count);
	ppdu_info->tcp_ack_msdu_count = le16_to_cpu(ppdu_eu_stats->tcp_ack_msdu_count);
	tid_bitmap = le16_to_cpu(ppdu_eu_stats->received_qos_data_tid_bitmap);
	ppdu_info->tid = ffs(tid_bitmap) - 1;
	ppdu_info->mpdu_retry_cnt =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_MPDU_RETRY_CNT_CMPCT);
	ppdu_info->retried_msdu_count = le16_to_cpu(ppdu_eu_stats->retried_msdu_count);

	switch (ppdu_info->preamble_type) {
	case HAL_RX_PREAMBLE_11N:
		ppdu_info->ht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AC:
		ppdu_info->vht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AX:
		ppdu_info->he_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11BE:
		ppdu_info->is_eht = true;
		break;
	case HAL_RX_PREAMBLE_11BN:	/* UHR */
		ppdu_info->is_uhr = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];
		u32 resp_ref = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref);
		u32 resp_ref_ext = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref_ext);

		ath12k_wifi8_hal_mon_handle_ofdma_info_compact(resp_ref, resp_ref_ext,
							       rxuser_stats);
		ath12k_wifi8_hal_mon_populate_byte_count_compact(info,
								 rxuser_stats);
	}
}

void
ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_parse(const void *tlv_data, u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info,
						 u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_PPDU_EU_STATS_TLV_SIZE))
		ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get_compact(tlv_data,
								       userid,
								       ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get(tlv_data,
							       userid,
							       ppdu_info);
}

enum hal_rx_mon_status
ath12k_wifi8_hal_mon_rx_parse_status_tlv(struct ath12k_hal *hal,
					 struct hal_rx_mon_ppdu_info *ppdu_info,
					 struct hal_tlv_parsed_hdr *tlv_parsed_hdr)
{
	const void *tlv_data = tlv_parsed_hdr->data;
	u32 userid;
	u16 tlv_tag, tlv_len;

	tlv_tag = tlv_parsed_hdr->tag;
	tlv_len = tlv_parsed_hdr->len;
	userid = tlv_parsed_hdr->userid;

	if (userid < HAL_MAX_UL_MU_USERS)
		ppdu_info->user_id = userid;

	if (ppdu_info->tlv_aggr.in_progress && ppdu_info->tlv_aggr.tlv_tag != tlv_tag) {
		ath12k_wifi8_hal_mon_parse_eht_uhr_sig_hdr(ppdu_info,
							   ppdu_info->tlv_aggr.buf);

		ppdu_info->tlv_aggr.in_progress = false;
		ppdu_info->tlv_aggr.cur_len = 0;
	}

	switch (tlv_tag) {
	case HAL_RX_PPDU_START: {
		const struct hal_rx_ppdu_start *ppdu_start = tlv_data;
		u32 info[2];

		u64 ppdu_ts = ath12k_hal_le32hilo_to_u64(ppdu_start->ppdu_start_ts_63_32,
							 ppdu_start->ppdu_start_ts_31_0);

		info[0] = le16_to_cpu(ppdu_start->phy_ppdu_id);

		ppdu_info->ppdu_id = info[0];

		info[1] = __le32_to_cpu(ppdu_start->info0);
		ppdu_info->chan_num = u32_get_bits(info[1],
						   HAL_RX_PPDU_START_INFO0_CHAN_NUM);
		ppdu_info->freq = u32_get_bits(info[1],
					       HAL_RX_PPDU_START_INFO0_CHAN_FREQ);
		ppdu_info->ppdu_ts = ppdu_ts;

		if (ppdu_info->ppdu_id != ppdu_info->last_ppdu_id) {
			ppdu_info->last_ppdu_id = ppdu_info->ppdu_id;
			ppdu_info->num_users = 0;
		}
		break;
	}
	case HAL_RX_PPDU_END_USER_STATS: {
		ath12k_hal_mon_rx_ppdu_end_usr_stats_info_get(hal, tlv_data,
							      userid, ppdu_info,
							      tlv_len);

		if (userid < HAL_MAX_UL_MU_USERS) {
			struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];

			if (ppdu_info->num_mpdu_fcs_ok > 1 ||
			    ppdu_info->num_mpdu_fcs_err > 1)
				ppdu_info->userstats[userid].ampdu_present = true;

			ppdu_info->num_users += 1;

			ath12k_wifi8_hal_mon_populate_mu_user_info(ppdu_info,
								   rxuser_stats);
		}
		break;
	}
	case HAL_PHYRX_HT_SIG:
		ath12k_wifi8_hal_mon_parse_ht_sig(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_L_SIG_B:
		ath12k_wifi8_hal_mon_parse_l_sig_b(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_L_SIG_A:
		ath12k_wifi8_hal_mon_parse_l_sig_a(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_VHT_SIG_A:
		ath12k_wifi8_hal_mon_parse_vht_sig_a(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_A_SU:
		ath12k_wifi8_hal_mon_parse_he_sig_su(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_A_MU_DL:
		ath12k_wifi8_hal_mon_parse_he_sig_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B1_MU:
		ath12k_wifi8_hal_mon_parse_he_sig_b1_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B2_MU:
		ath12k_wifi8_hal_mon_parse_he_sig_b2_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B2_OFDMA:
		ath12k_wifi8_hal_mon_parse_he_sig_b2_ofdma(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_RSSI_LEGACY: {
		const struct hal_rx_phyrx_rssi_legacy_info *rssi = tlv_data;
		u32 info[2];

		info[0] = __le32_to_cpu(rssi->info0);
		info[1] = __le32_to_cpu(rssi->info1);

		/* TODO: Please note that the combined rssi will not be accurate
		 * in MU case. Rssi in MU needs to be retrieved from
		 * PHYRX_OTHER_RECEIVE_INFO TLV.
		 */
		ppdu_info->rssi_comb =
			u32_get_bits(info[1],
				     HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO1_RSSI_COMB);

		ppdu_info->bw = u32_get_bits(info[0],
					     HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO0_RX_BW);
		break;
	}
	case HAL_PHYRX_RSSI_LEGACY_PART2: {
		u8 *part2 = (u8 *)tlv_data;
		u8 *rssi_info_tlv = part2 +
				HAL_PHYRX_RSSI_LEGACY_PART2_PREAMBLE_RSSI_INFO_OFFSET;
		u32 need = HAL_PHYRX_RSSI_LEGACY_PART2_PREAMBLE_RSSI_INFO_OFFSET +
			  RECEIVE_RSSI_INFO_RSSI_PRI20_CHAIN4_OFFSET + sizeof(u32);

		if (tlv_len < need)
			break;

		HAL_RX_MON_UPDATE_PRI20_RSSI_PER_CHAIN(0, ppdu_info, rssi_info_tlv);
		HAL_RX_MON_UPDATE_PRI20_RSSI_PER_CHAIN(1, ppdu_info, rssi_info_tlv);
		HAL_RX_MON_UPDATE_PRI20_RSSI_PER_CHAIN(2, ppdu_info, rssi_info_tlv);
		HAL_RX_MON_UPDATE_PRI20_RSSI_PER_CHAIN(3, ppdu_info, rssi_info_tlv);
		HAL_RX_MON_UPDATE_PRI20_RSSI_PER_CHAIN(4, ppdu_info, rssi_info_tlv);
		break;
	}
	case HAL_PHYRX_COMMON_USER_INFO: {
		const struct hal_phyrx_common_user_info *cmn_usr_info = tlv_data;
		struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
		u32 known, data;
		u8 ltf_size;

		ppdu_info->gi = le32_get_bits(cmn_usr_info->info0,
					      HAL_RX_PHY_CMN_USER_INFO0_GI);
		ltf_size = le32_get_bits(cmn_usr_info->info0,
					 HAL_RX_PHY_CMN_USER_INFO0_LTF_SIZE);

		known = __le32_to_cpu(eht->known);
		known |= IEEE80211_RADIOTAP_EHT_KNOWN_GI;
		eht->known = cpu_to_le32(known);

		data = __le32_to_cpu(eht->data[0]);
		data |= u32_encode_bits(ath12k_eht_gi_to_nl80211_eht_gi(ppdu_info->gi),
					IEEE80211_RADIOTAP_EHT_DATA0_GI);
		data |= u32_encode_bits(ltf_size,
					IEEE80211_RADIOTAP_EHT_DATA0_LTF);
		eht->data[0] = cpu_to_le32(data);
		break;
	}
	case HAL_PHYRX_LOCATION:
		ath12k_wifi8_hal_mon_get_rtt_info(tlv_data, tlv_len, ppdu_info);
		break;
	case HAL_RX_PPDU_START_USER_INFO:
		ath12k_wifi8_hal_mon_parse_user_info(tlv_data, userid, ppdu_info);
		break;

	case HAL_RXPCU_PPDU_END_INFO: {
		const struct hal_rx_ppdu_end_duration *ppdu_rx_duration = tlv_data;
		u32 info[1];

		info[0] = __le32_to_cpu(ppdu_rx_duration->info0);
		ppdu_info->rx_antenna =
			u32_get_bits(info[0],
				     HAL_RX_PPDU_END_DURATION_INFO0_RX_ANTENNA);
		info[0] = __le32_to_cpu(ppdu_rx_duration->info1);
		ppdu_info->rx_duration =
			u32_get_bits(info[0],
				     HAL_RX_PPDU_END_DURATION_INFO1_RX_PPDU_DURATION);
		ppdu_info->tsft = __le32_to_cpu(ppdu_rx_duration->wb_timestamp_upper_32);
		ppdu_info->tsft = (ppdu_info->tsft << 32) |
				   __le32_to_cpu(ppdu_rx_duration->wb_timestamp_lower_32);
		ath12k_wifi8_hal_mon_get_bb_info(tlv_data, tlv_len, ppdu_info);
		break;
	}
	case HAL_RX_MPDU_START: {
		ath12k_hal_mon_rx_mpdu_start_info_get(hal, tlv_data,
						      userid, ppdu_info,
						      tlv_len);

		return HAL_RX_MON_STATUS_MPDU_START;
	}
	case HAL_RX_MSDU_START:
		/* TODO: add msdu start parsing logic */
		break;
	case HAL_MON_BUF_ADDR:
		return HAL_RX_MON_STATUS_BUF_ADDR;
	case HAL_RX_MSDU_END:

		ath12k_hal_mon_rx_msdu_end_info_get(hal, tlv_data, userid, ppdu_info,
						    tlv_len);

		if (ppdu_info->grp_id == RX_MSDU_END_INFO0_SW_FRAMEGROUP_UCAST_DATA ||
		    ppdu_info->grp_id == RX_MSDU_END_INFO0_SW_FRAMEGROUP_MCAST_DATA) {
			if (userid < HAL_MAX_UL_MU_USERS)
				ppdu_info->userstats[userid].errmap = ppdu_info->errmap;
		}

		return HAL_RX_MON_STATUS_MSDU_END;
	case HAL_RX_MPDU_END:
		const struct hal_rx_mpdu_end *mpdu_end = tlv_data;
		u32 info0;

		info0 = __le32_to_cpu(mpdu_end->info0);

		if (userid < HAL_MAX_UL_MU_USERS)
			ppdu_info->mpdu_info[userid].fcs_err =
				u32_get_bits(info0, HAL_RX_MPDU_END_INFO0_FCS_ERR);

		return HAL_RX_MON_STATUS_MPDU_END;
	case HAL_PHYRX_GENERIC_U_SIG:
		ath12k_wifi8_hal_mon_rx_parse_u_sig_hdr(tlv_data, ppdu_info);
		break;
	case HAL_PHYRX_GENERIC_EHT_OR_UHR_SIG:
		/* Handle the case where aggregation is in progress
		 * or the current TLV is one of the TLVs which should be
		 * aggregated
		 */
		if (!ppdu_info->tlv_aggr.in_progress) {
			ppdu_info->tlv_aggr.in_progress = true;
			ppdu_info->tlv_aggr.tlv_tag = tlv_tag;
			ppdu_info->tlv_aggr.cur_len = 0;
		}

		ppdu_info->is_eht = true;

		ath12k_wifi8_hal_mon_aggr_tlv(ppdu_info, tlv_len, tlv_data);
		break;
	case HAL_DUMMY:
		return HAL_RX_MON_STATUS_BUF_DONE;
	case HAL_RX_HEADER:
		const struct hal_mon_rx_hdr *rx_hdr = tlv_data;

		if (userid < HAL_MAX_UL_MU_USERS) {
			ppdu_info->mpdu_info[userid].raw_mpdu =
				le32_get_bits(rx_hdr->info0,
					      HAL_MON_RX_HDR_INFO0_RAW_MPDU);

			if (ppdu_info->mpdu_info[userid].raw_mpdu)
				ppdu_info->mpdu_info[userid].decap_type =
						DP_RX_DECAP_TYPE_RAW;
			else
				ppdu_info->mpdu_info[userid].decap_type =
					le32_get_bits(rx_hdr->info0,
						      HAL_MON_RX_HDR_INFO0_DECAP_TYPE);
		}
		return HAL_RX_MON_STATUS_RX_HDR;
	case HAL_MON_DROP:
		ppdu_info->is_drop_tlv = true;
		return HAL_RX_MON_STATUS_DROP_TLV;
	case HAL_RX_PPDU_END_STATUS_DONE:
		return HAL_RX_MON_STATUS_PPDU_DONE;
	case 0:
		return HAL_RX_MON_STATUS_PPDU_DONE;
	default:
		break;
	}

	return HAL_RX_MON_STATUS_PPDU_NOT_DONE;
}

u8 *ath12k_wifi8_hal_mon_rx_desc_get_msdu_payload(void *rx_desc)
{
	return ath12k_wifi8_hal_rx_desc_get_msdu_payload_qcn9625(rx_desc);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_fes_setup_info_get(const void *tlv_data,
					   u32 userid,
					   struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_fes_setup *tx_fes_setup =
				(struct hal_tx_mon_fes_setup *)tlv_data;
	u32 info = __le32_to_cpu(tx_fes_setup->info0);

	ppdu_info->ppdu_id = __le32_to_cpu(tx_fes_setup->schedule_id);
	ppdu_info->rx_status.ppdu_id = __le32_to_cpu(tx_fes_setup->schedule_id);
	ppdu_info->num_users =
		u32_get_bits(info,
			     HAL_TX_MON_FES_SETUP_INFO0_NUM_OF_USERS);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_fes_setup_info_get_compact(const void *tlv_data,
						   u32 userid,
						   struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_fes_setup_compact *tx_fes_setup =
				(struct hal_tx_mon_fes_setup_compact *)tlv_data;
	u32 info = __le32_to_cpu(tx_fes_setup->info0);

	ppdu_info->ppdu_id = __le32_to_cpu(tx_fes_setup->schedule_id);
	ppdu_info->rx_status.ppdu_id = __le32_to_cpu(tx_fes_setup->schedule_id);
	ppdu_info->num_users =
		u32_get_bits(info,
			     HAL_TX_MON_FES_SETUP_INFO0_NUM_OF_USERS_CMPCT);
}

void
ath12k_wifi8_hal_mon_tx_fes_setup_info_parse(const void *tlv_data, u32 userid,
					     struct hal_tx_mon_ppdu_info *ppdu_info,
					     u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_FES_SETUP_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_fes_setup_info_get_compact(tlv_data,
								   userid,
								   ppdu_info);
	else
		ath12k_wifi8_hal_mon_tx_fes_setup_info_get(tlv_data,
							   userid,
							   ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_peer_entry_info_get(const void *tlv_data,
					    u32 userid,
					    struct hal_tx_mon_ppdu_info *ppdu_info,
					    struct hal_tx_mon_status_info *status_info)
{
	struct hal_tx_mon_peer_entry *tx_peer_entry =
				(struct hal_tx_mon_peer_entry *)tlv_data;
	u32 info[5];
	u32 addr_32;
	u16 addr_16;

	info[0] = __le32_to_cpu(tx_peer_entry->info0);
	info[1] = __le32_to_cpu(tx_peer_entry->info1);
	info[2] = __le32_to_cpu(tx_peer_entry->info2);
	info[3] = __le32_to_cpu(tx_peer_entry->info3);
	info[4] = __le32_to_cpu(tx_peer_entry->info4);

	addr_32 = u32_get_bits(info[0],
			       HAL_TX_MON_PEER_ENTRY_INFO0_MAC_ADR_31_0);
	addr_16 = u32_get_bits(info[1],
			       HAL_TX_MON_PEER_ENTRY_INFO1_MAC_ADR_47_32);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    status_info->addr1,
				    true);

	addr_16 = u32_get_bits(info[1],
			       HAL_TX_MON_PEER_ENTRY_INFO1_MAC_ADR_15_0);
	addr_32 = u32_get_bits(info[2],
			       HAL_TX_MON_PEER_ENTRY_INFO2_MAC_ADR_47_16);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    status_info->addr2,
				    false);

	ppdu_info->rx_status.userstats[userid].enc_type =
		u32_get_bits(info[3],
			     HAL_TX_MON_PEER_ENTRY_INFO3_KEY_TYPE);
	ppdu_info->rx_status.userstats[userid].sw_peer_id =
		u32_get_bits(info[4],
			     HAL_TX_MON_PEER_ENTRY_INFO4_SW_PEER_ID);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_peer_entry_info_get_compact(const void *tlv_data,
						    u32 userid,
						    struct hal_tx_mon_ppdu_info *ppdu,
						    struct hal_tx_mon_status_info *status)
{
	struct hal_tx_mon_peer_entry_compact *tx_peer_entry =
				(struct hal_tx_mon_peer_entry_compact *)tlv_data;
	u32 info[5];
	u32 addr_32;
	u16 addr_16;

	info[0] = __le32_to_cpu(tx_peer_entry->info0);
	info[1] = __le32_to_cpu(tx_peer_entry->info1);
	info[2] = __le32_to_cpu(tx_peer_entry->info2);
	info[3] = __le32_to_cpu(tx_peer_entry->info3);
	info[4] = __le32_to_cpu(tx_peer_entry->info4);

	addr_32 = u32_get_bits(info[0],
			       HAL_TX_MON_PEER_ENTRY_INFO0_MAC_ADR_31_0_CMPCT);
	addr_16 = u32_get_bits(info[1],
			       HAL_TX_MON_PEER_ENTRY_INFO1_MAC_ADR_47_32_CMPCT);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    status->addr1,
				    true);

	addr_16 = u32_get_bits(info[1],
			       HAL_TX_MON_PEER_ENTRY_INFO1_MAC_ADR_15_0_CMPCT);
	addr_32 = u32_get_bits(info[2],
			       HAL_TX_MON_PEER_ENTRY_INFO2_MAC_ADR_47_16_CMPCT);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    status->addr2,
				    false);

	ppdu->rx_status.userstats[userid].enc_type =
		u32_get_bits(info[3],
			     HAL_TX_MON_PEER_ENTRY_INFO3_KEY_TYPE_CMPCT);
	ppdu->rx_status.userstats[userid].sw_peer_id =
		u32_get_bits(info[4],
			     HAL_TX_MON_PEER_ENTRY_INFO4_SW_PEER_ID_CMPCT);
}

void
ath12k_wifi8_hal_mon_tx_peer_entry_info_parse(const void *tlv_data, u32 userid,
					      struct hal_tx_mon_ppdu_info *ppdu_info,
					      struct hal_tx_mon_status_info *status_info,
					      u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_PEER_ENTRY_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_peer_entry_info_get_compact(tlv_data,
								    userid,
								    ppdu_info,
								    status_info);
	else
		ath12k_wifi8_hal_mon_tx_peer_entry_info_get(tlv_data,
							    userid,
							    ppdu_info,
							    status_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_queue_ext_info_get(const void *tlv_data,
					   u32 userid,
					   struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_queue_ext *tx_queue_ext =
				(struct hal_tx_mon_queue_ext *)tlv_data;
	u32 info = __le32_to_cpu(tx_queue_ext->info0);

	ppdu_info->rx_status.frame_control =
		u32_get_bits(info,
			     HAL_TX_MON_QUEUE_EXT_INFO0_FRAME_CTRL);
	ppdu_info->rx_status.frame_control_info_valid = true;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_queue_ext_info_get_compact(const void *tlv_data,
						   u32 userid,
						   struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_queue_ext_compact *tx_queue_ext =
				(struct hal_tx_mon_queue_ext_compact *)tlv_data;
	u32 info = __le32_to_cpu(tx_queue_ext->info0);

	ppdu_info->rx_status.frame_control =
		u32_get_bits(info,
			     HAL_TX_MON_QUEUE_EXT_INFO0_FRAME_CTRL_CMPCT);
	ppdu_info->rx_status.frame_control_info_valid = true;
}

void
ath12k_wifi8_hal_mon_tx_queue_ext_info_parse(const void *tlv_data, u32 userid,
					     struct hal_tx_mon_ppdu_info *ppdu_info,
					     u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_QUEUE_EXT_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_queue_ext_info_get_compact(tlv_data,
								   userid,
								   ppdu_info);
	else
		ath12k_wifi8_hal_mon_tx_queue_ext_info_get(tlv_data,
							   userid,
							   ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_mpdu_start_info_get(const void *tlv_data,
					    u32 userid,
					    struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_mpdu_start *tx_mpdu_start =
				(struct hal_tx_mon_mpdu_start *)tlv_data;
	u32 info = __le32_to_cpu(tx_mpdu_start->info0);

	ppdu_info->rx_status.userstats[userid].start_seq =
		u32_get_bits(info,
			     HAL_TX_MON_MPDU_START_INFO0_SEQ);
	ppdu_info->cur_usr_idx = userid;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_mpdu_start_info_get_compact(const void *tlv_data,
						    u32 userid,
						    struct hal_tx_mon_ppdu_info *ppdu)
{
	struct hal_tx_mon_mpdu_start_compact *tx_mpdu_start =
				(struct hal_tx_mon_mpdu_start_compact *)tlv_data;
	u32 info = __le32_to_cpu(tx_mpdu_start->info0);

	ppdu->rx_status.userstats[userid].start_seq =
		u32_get_bits(info,
			     HAL_TX_MON_MPDU_START_INFO0_SEQ_CMPCT);
	ppdu->cur_usr_idx = userid;
}

void
ath12k_wifi8_hal_mon_tx_mpdu_start_info_parse(const void *tlv_data, u32 userid,
					      struct hal_tx_mon_ppdu_info *ppdu_info,
					      u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_MPDU_START_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_mpdu_start_info_get_compact(tlv_data,
								    userid,
								    ppdu_info);
	else
		ath12k_wifi8_hal_mon_tx_mpdu_start_info_get(tlv_data,
							    userid,
							    ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_fes_status_end_info_get
			(const void *tlv_data,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info,
			 struct hal_tx_mon_status_info *tx_status_info)
{
	struct hal_tx_mon_fes_status_end *tx_fes_status_end =
				(struct hal_tx_mon_fes_status_end *)tlv_data;
	u32 info[2], phytx_abort_request_info_valid;

	info[0] = __le32_to_cpu(tx_fes_status_end->info0);
	info[1] = __le32_to_cpu(tx_fes_status_end->info1);

	phytx_abort_request_info_valid =
		u32_get_bits(info[0],
			     HAL_TX_MON_FES_STATUS_END_INFO0_PHYTX_ABORT_VALID);

	if (phytx_abort_request_info_valid) {
		tx_status_info->phy_abort_reason =
			u32_get_bits(info[0],
				     HAL_TX_MON_FES_STATUS_END_INFO0_PHYTX_ABORT_REASON);
		tx_status_info->phy_abort_user_number =
		u32_get_bits(info[0],
			     HAL_TX_MON_FES_STATUS_END_INFO0_PHYTX_ABORT_USER_NUM);
	}

	tx_status_info->response_type =
		u32_get_bits(info[1],
			     HAL_TX_MON_FES_STATUS_END_INFO1_RESPONSE_TYPE);
	tx_status_info->r2r_to_follow =
		u32_get_bits(info[1],
			     HAL_TX_MON_FES_STATUS_END_INFO1_R2R_END_STATUS);

	if (ppdu_info->num_users == 1 && ppdu_info->rx_status.he_flags) {
		ppdu_info->rx_status.he_data1 =
			ppdu_info->rx_status.userstats[userid].he_data1;
		ppdu_info->rx_status.he_data2 =
			ppdu_info->rx_status.userstats[userid].he_data2;
		ppdu_info->rx_status.he_data3 =
			ppdu_info->rx_status.userstats[userid].he_data3;
		ppdu_info->rx_status.he_data4 =
			ppdu_info->rx_status.userstats[userid].he_data4;
		ppdu_info->rx_status.he_data5 =
			ppdu_info->rx_status.userstats[userid].he_data5;
		ppdu_info->rx_status.he_data6 =
			ppdu_info->rx_status.userstats[userid].he_data6;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_fes_status_end_info_get_compact
			(const void *tlv_data,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info,
			 struct hal_tx_mon_status_info *tx_status_info)
{
	struct hal_tx_mon_fes_status_end_compact *tx_fes_status_end =
			(struct hal_tx_mon_fes_status_end_compact *)tlv_data;
	u32 info[2], phytx_abort_request_info_valid;

	info[0] = __le32_to_cpu(tx_fes_status_end->info0);
	info[1] = __le32_to_cpu(tx_fes_status_end->info1);

	phytx_abort_request_info_valid =
		u32_get_bits(info[0],
			     HAL_TX_MON_FES_STATUS_END_INFO0_PHYTX_ABORT_VALID_CMPCT);

	if (phytx_abort_request_info_valid) {
		tx_status_info->phy_abort_reason =
		u32_get_bits(info[0],
			     HAL_TX_MON_FES_STATUS_END_INFO0_PHYTX_ABORT_REASON_CMPCT);
		tx_status_info->phy_abort_user_number =
		u32_get_bits(info[0],
			     HAL_TX_MON_FES_STATUS_END_INFO0_PHYTX_ABORT_USER_NUM_CMPCT);
	}

	tx_status_info->response_type =
		u32_get_bits(info[1],
			     HAL_TX_MON_FES_STATUS_END_INFO1_RESPONSE_TYPE_CMPCT);
	tx_status_info->r2r_to_follow =
		u32_get_bits(info[1],
			     HAL_TX_MON_FES_STATUS_END_INFO1_R2R_END_STATUS_CMPCT);

	if (ppdu_info->num_users == 1 && ppdu_info->rx_status.he_flags) {
		ppdu_info->rx_status.he_data1 =
			ppdu_info->rx_status.userstats[userid].he_data1;
		ppdu_info->rx_status.he_data2 =
			ppdu_info->rx_status.userstats[userid].he_data2;
		ppdu_info->rx_status.he_data3 =
			ppdu_info->rx_status.userstats[userid].he_data3;
		ppdu_info->rx_status.he_data4 =
			ppdu_info->rx_status.userstats[userid].he_data4;
		ppdu_info->rx_status.he_data5 =
			ppdu_info->rx_status.userstats[userid].he_data5;
		ppdu_info->rx_status.he_data6 =
			ppdu_info->rx_status.userstats[userid].he_data6;
	}
}

void ath12k_wifi8_hal_mon_tx_fes_status_end_info_parse(
				const void *tlv_data, u32 userid,
				struct hal_tx_mon_ppdu_info *ppdu_info,
				struct hal_tx_mon_status_info *tx_status_info,
				u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_FES_STATUS_END_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_fes_status_end_info_get_compact(tlv_data,
									userid,
									ppdu_info,
									tx_status_info);
	else
		ath12k_wifi8_hal_mon_tx_fes_status_end_info_get(tlv_data,
								userid,
								ppdu_info,
								tx_status_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_response_end_status_info_get
			(const void *tlv_data,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info,
			 struct hal_tx_mon_status_info *tx_status_info)
{
	struct hal_tx_mon_response_end_status *tx_response_end =
			(struct hal_tx_mon_response_end_status *)tlv_data;
	u32 info = __le32_to_cpu(tx_response_end->info0);

	ppdu_info->rx_status.bw =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_COEX_BASED_TX_BW);
	tx_status_info->generated_response =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_GENERATED_RESPONSE);
	tx_status_info->mba_count =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_MBA_USER_COUNT);
	tx_status_info->mba_fake_bitmap_count =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_MBA_FAKE_BA_COUNT);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_response_end_status_info_get_compact
			(const void *tlv_data,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info,
			 struct hal_tx_mon_status_info *tx_status_info)
{
	struct hal_tx_mon_response_end_status_compact *tx_response_end =
			(struct hal_tx_mon_response_end_status_compact *)tlv_data;
	u32 info = __le32_to_cpu(tx_response_end->info0);

	ppdu_info->rx_status.bw =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_COEX_BASED_TX_BW_CMPCT);
	tx_status_info->generated_response =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_GENERATED_RESPONSE_CMPCT);
	tx_status_info->mba_count =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_MBA_USER_COUNT_CMPCT);
	tx_status_info->mba_fake_bitmap_count =
		u32_get_bits(info,
			     HAL_TX_MON_RESPONSE_END_INFO0_MBA_FAKE_BA_COUNT_CMPCT);
}

void ath12k_wifi8_hal_mon_tx_response_end_status_info_parse(
				const void *tlv_data, u32 userid,
				struct hal_tx_mon_ppdu_info *ppdu_info,
				struct hal_tx_mon_status_info *tx_status_info,
				u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_RESPONSE_END_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_response_end_status_info_get_compact
								(tlv_data,
								 userid,
								 ppdu_info,
								 tx_status_info);
	else
		ath12k_wifi8_hal_mon_tx_response_end_status_info_get(tlv_data,
								     userid,
								     ppdu_info,
								     tx_status_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_fes_status_prot_info_get(const void *tlv_data,
						 u32 userid,
						 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_fes_status_prot *tx_fes_status_prot =
			(struct hal_tx_mon_fes_status_prot *)tlv_data;
	u32 info = __le32_to_cpu(tx_fes_status_prot->info0);

	ppdu_info->cts_recvd =
		u32_get_bits(info,
			     HAL_TX_MON_FES_STATUS_PROT_INFO0_SUCCESS);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_fes_status_prot_info_get_compact
					(const void *tlv_data,
					 u32 userid,
					 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_fes_status_prot_compact *tx_fes_status_prot =
			(struct hal_tx_mon_fes_status_prot_compact *)tlv_data;
	u32 info = __le32_to_cpu(tx_fes_status_prot->info0);

	ppdu_info->cts_recvd =
		u32_get_bits(info,
			     HAL_TX_MON_FES_STATUS_PROT_INFO0_SUCCESS_CMPCT);
}

void ath12k_wifi8_hal_mon_tx_fes_status_prot_info_parse(
						const void *tlv_data, u32 userid,
						struct hal_tx_mon_ppdu_info *ppdu_info,
						u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_FES_STATUS_PROT_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_fes_status_prot_info_get_compact(tlv_data,
									 userid,
									 ppdu_info);
	else
		ath12k_wifi8_hal_mon_tx_fes_status_prot_info_get(tlv_data,
								 userid,
								 ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_pcu_ppdu_setup_init_info_get
					(const void *tlv_data,
					 struct hal_tx_mon_status_info *tx_status_info)
{
	struct hal_tx_mon_pcu_ppdu_setup_init *tx_pcu_ppdu_setup =
			(struct hal_tx_mon_pcu_ppdu_setup_init *)tlv_data;
	u32 info[6];
	u32 addr_32;
	u16 addr_16;

	info[0] = __le32_to_cpu(tx_pcu_ppdu_setup->info0);
	info[1] = __le32_to_cpu(tx_pcu_ppdu_setup->info1);
	info[2] = __le32_to_cpu(tx_pcu_ppdu_setup->info2);
	info[3] = __le32_to_cpu(tx_pcu_ppdu_setup->info3);
	info[4] = __le32_to_cpu(tx_pcu_ppdu_setup->info4);
	info[5] = __le32_to_cpu(tx_pcu_ppdu_setup->info5);

	tx_status_info->protection_addr =
		u32_get_bits(info[0],
			     HAL_TX_MON_PPDU_SETUP_INFO0_PROTECTION_ADDRESS_FIELDS);

	addr_32 = u32_get_bits(info[1],
			       HAL_TX_MON_PPDU_SETUP_INFO1_PROT_FRAME_ADDR1_31_0);
	addr_16 = u32_get_bits(info[2],
			       HAL_TX_MON_PPDU_SETUP_INFO2_PROT_FRAME_ADDR1_47_32);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    tx_status_info->addr1,
				    true);

	addr_16 = u32_get_bits(info[2],
			       HAL_TX_MON_PPDU_SETUP_INFO2_PROT_FRAME_ADDR2_15_0);
	addr_32 = u32_get_bits(info[3],
			       HAL_TX_MON_PPDU_SETUP_INFO3_PROT_FRAME_ADDR2_47_16);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    tx_status_info->addr2,
				    false);

	addr_32 = u32_get_bits(info[4],
			       HAL_TX_MON_PPDU_SETUP_INFO4_PROT_FRAME_ADDR3_31_0);
	addr_16 = u32_get_bits(info[5],
			       HAL_TX_MON_PPDU_SETUP_INFO5_PROT_FRAME_ADDR3_47_32);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    tx_status_info->addr3,
				    true);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_pcu_ppdu_setup_init_info_get_compact
					(const void *tlv_data,
					 struct hal_tx_mon_status_info *tx_status_info)
{
	struct hal_tx_mon_pcu_ppdu_setup_init_compact *tx_pcu_ppdu_setup =
		(struct hal_tx_mon_pcu_ppdu_setup_init_compact *)tlv_data;
	u32 info[6];
	u32 addr_32;
	u16 addr_16;

	info[0] = __le32_to_cpu(tx_pcu_ppdu_setup->info0);
	info[1] = __le32_to_cpu(tx_pcu_ppdu_setup->info1);
	info[2] = __le32_to_cpu(tx_pcu_ppdu_setup->info2);
	info[3] = __le32_to_cpu(tx_pcu_ppdu_setup->info3);
	info[4] = __le32_to_cpu(tx_pcu_ppdu_setup->info4);
	info[5] = __le32_to_cpu(tx_pcu_ppdu_setup->info5);

	tx_status_info->protection_addr =
		u32_get_bits(info[0],
			     HAL_TX_MON_PPDU_SETUP_INFO0_PROTECTION_ADDRESS_FIELDS_CMPCT);

	addr_32 = u32_get_bits(info[1],
			       HAL_TX_MON_PPDU_SETUP_INFO1_PROT_FRAME_ADDR1_31_0_CMPCT);
	addr_16 = u32_get_bits(info[2],
			       HAL_TX_MON_PPDU_SETUP_INFO2_PROT_FRAME_ADDR1_47_32_CMPCT);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    tx_status_info->addr1,
				    true);

	addr_16 = u32_get_bits(info[2],
			       HAL_TX_MON_PPDU_SETUP_INFO2_PROT_FRAME_ADDR2_15_0_CMPCT);
	addr_32 = u32_get_bits(info[3],
			       HAL_TX_MON_PPDU_SETUP_INFO3_PROT_FRAME_ADDR2_47_16_CMPCT);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    tx_status_info->addr2,
				    false);

	addr_32 = u32_get_bits(info[4],
			       HAL_TX_MON_PPDU_SETUP_INFO4_PROT_FRAME_ADDR3_31_0_CMPCT);
	addr_16 = u32_get_bits(info[5],
			       HAL_TX_MON_PPDU_SETUP_INFO5_PROT_FRAME_ADDR3_47_32_CMPCT);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16,
				    tx_status_info->addr3,
				    true);
}

void ath12k_wifi8_hal_mon_tx_pcu_ppdu_setup_init_info_parse(
					const void *tlv_data,
					struct hal_tx_mon_status_info *tx_status_info,
					u16 tlv_len)
{
	if (likely(tlv_len < HAL_MON_TX_PCU_PPDU_SETUP_INIT_TLV_SIZE))
		ath12k_wifi8_hal_mon_tx_pcu_ppdu_setup_init_info_get_compact
								(tlv_data,
								 tx_status_info);
	else
		ath12k_wifi8_hal_mon_tx_pcu_ppdu_setup_init_info_get(tlv_data,
								     tx_status_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_he_sig_a_su
			(const struct hal_tx_mon_he_sig_a_su *he_sig_a_su,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data6;
	u16 he_mu_flag_1 = 0;
	u16 he_mu_flag_2 = 0;
	u16 num_users = 0;
	u8  mcs_of_sig_b = 0;
	u8  dcm_of_sig_b = 0;
	u8  sig_a_bw = 0;
	u8  i = 0;
	u8  bss_color_id;
	u8  coding;
	u8  stbc;
	u8  beam_change;
	u8  txop;
	u32 info[2];

	info[0] = __le32_to_cpu(he_sig_a_su->info0);
	info[1] = __le32_to_cpu(he_sig_a_su->info1);

	num_users = ppdu_info->num_users;
	beam_change = u32_get_bits(info[0],
				   HAL_TX_MON_HE_SIG_A_SU_INFO0_BEAM_CHANGE);
	mcs_of_sig_b = u32_get_bits(info[0],
				    HAL_TX_MON_HE_SIG_A_SU_INFO0_TRANSMIT_MCS);
	dcm_of_sig_b = u32_get_bits(info[0],
				    HAL_TX_MON_HE_SIG_A_SU_INFO0_DCM);
	sig_a_bw = u32_get_bits(info[0],
				HAL_TX_MON_HE_SIG_A_SU_INFO0_TRANSMIT_BW);
	bss_color_id = u32_get_bits(info[0],
				    HAL_TX_MON_HE_SIG_A_SU_INFO0_BSS_COLOR_ID);
	coding = u32_get_bits(info[1],
			      HAL_TX_MON_HE_SIG_A_SU_INFO1_CODING);
	stbc = u32_get_bits(info[1],
			    HAL_TX_MON_HE_SIG_A_SU_INFO1_STBC);
	txop = u32_get_bits(info[1],
			    HAL_TX_MON_HE_SIG_A_SU_INFO1_TXOP_DURATION);

	he_mu_flag_1 |= mcs_of_sig_b;
	he_mu_flag_1 |= dcm_of_sig_b << HE_STA_DCM_SHIFT;
	he_mu_flag_1 |= IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_MCS_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_DCM_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH2_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU_KNOWN;
	he_mu_flag_2 |= sig_a_bw;
	he_mu_flag_2 |= IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_KNOWN;

	ppdu_info->rx_status.he_mu_flags = (!!(num_users & HE_MU_NUM_USER_MASK));

	he_data1 = (IEEE80211_RADIOTAP_HE_DATA1_BSS_COLOR_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA1_CODING_KNOWN);
	he_data2 = (IEEE80211_RADIOTAP_HE_DATA2_TXBF_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA2_PE_DISAMBIG_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA2_TXOP_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA2_PRE_FEC_PAD_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA2_MIDAMBLE_KNOWN);
	he_data3 = (bss_color_id | (beam_change << HE_BEAM_CHANGE_SHIFT) |
			(coding << HE_CODING_SHIFT) | (stbc << HE_STBC_SHIFT));
	he_data6 = (txop << HE_TXOP_SHIFT);

	for (i = 0; i < num_users; i++) {
		ppdu_info->rx_status.userstats[i].he_flags1 |= he_mu_flag_1;
		ppdu_info->rx_status.userstats[i].he_flags2 |= he_mu_flag_2;
		ppdu_info->rx_status.userstats[i].he_data1 |= he_data1;
		ppdu_info->rx_status.userstats[i].he_data2 |= he_data2;
		ppdu_info->rx_status.userstats[i].he_data3 |= he_data3;
		ppdu_info->rx_status.userstats[i].he_data6 |= he_data6;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_he_sig_a_mu_dl
			(const struct hal_tx_mon_he_sig_a_mu_dl *he_sig_a_mu_dl,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data6;
	u16 he_mu_flag_1 = 0;
	u16 he_mu_flag_2 = 0;
	u16 num_users = 0;
	u8  bss_color_id;
	u8  txop;
	u8  mcs_of_sig_b = 0;
	u8  dcm_of_sig_b = 0;
	u8  sig_a_bw = 0;
	u8  num_sig_b_symb = 0;
	u8  comp_mode_sig_b = 0;
	u8  i = 0;
	u32 info[2];

	info[0] = __le32_to_cpu(he_sig_a_mu_dl->info0);
	info[1] = __le32_to_cpu(he_sig_a_mu_dl->info1);

	num_users = ppdu_info->num_users;
	mcs_of_sig_b = u32_get_bits(info[0],
				    HAL_TX_MON_HE_SIG_A_MU_DL_INFO0_MCS_OF_SIG_B);
	dcm_of_sig_b = u32_get_bits(info[0],
				    HAL_TX_MON_HE_SIG_A_MU_DL_INFO0_DCM_OF_SIG_B);
	sig_a_bw = u32_get_bits(info[0],
				HAL_TX_MON_HE_SIG_A_MU_DL_INFO0_TRANSMIT_BW);
	num_sig_b_symb = u32_get_bits(info[0],
				      HAL_TX_MON_HE_SIG_A_MU_DL_INFO0_NUM_SIG_B_SYMBOLS);
	comp_mode_sig_b = u32_get_bits(info[0],
				       HAL_TX_MON_HE_SIG_A_MU_DL_INFO0_COMP_MODE_SIG_B);
	bss_color_id = u32_get_bits(info[0],
				    HAL_TX_MON_HE_SIG_A_MU_DL_INFO0_BSS_COLOR_ID);
	txop = u32_get_bits(info[1],
			    HAL_TX_MON_HE_SIG_A_MU_DL_INFO1_TXOP_DURATION);

	he_mu_flag_1 |= mcs_of_sig_b;
	he_mu_flag_1 |= dcm_of_sig_b << HE_STA_DCM_SHIFT;
	he_mu_flag_1 |= IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_MCS_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_DCM_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_SYMS_USERS_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH2_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_COMP_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_SYMS_USERS_KNOWN;
	he_mu_flag_2 |= sig_a_bw;
	he_mu_flag_2 |= sig_a_bw << HE_SIG_A_PUNC_BW_SHIFT;
	he_mu_flag_2 |= num_sig_b_symb << HE_NUM_SIG_B_SYMBOLS_SHIFT;
	he_mu_flag_2 |= comp_mode_sig_b << HE_SIG_B_COMPRESSION_FLAG_2_SHIFT;
	he_mu_flag_2 |= IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_KNOWN;
	he_mu_flag_2 |= IEEE80211_RADIOTAP_HE_MU_FLAGS2_PUNC_FROM_SIG_A_BW_KNOWN;

	he_data1 = IEEE80211_RADIOTAP_HE_DATA1_BSS_COLOR_KNOWN;
	he_data2 = IEEE80211_RADIOTAP_HE_DATA2_TXOP_KNOWN;
	he_data3 = bss_color_id;
	he_data6 = (txop << HE_TXOP_SHIFT);

	ppdu_info->rx_status.he_mu_flags = (!!(num_users & HE_MU_NUM_USER_MASK));

	for (i = 0; i < num_users; i++) {
		ppdu_info->rx_status.userstats[i].he_flags1 |= he_mu_flag_1;
		ppdu_info->rx_status.userstats[i].he_flags2 |= he_mu_flag_2;
		ppdu_info->rx_status.userstats[i].he_data1 |= he_data1;
		ppdu_info->rx_status.userstats[i].he_data2 |= he_data2;
		ppdu_info->rx_status.userstats[i].he_data3 |= he_data3;
		ppdu_info->rx_status.userstats[i].he_data6 |= he_data6;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_he_sig_b2_mu
			(const struct hal_tx_mon_he_sig_b2_mu *he_sig_b2_mu,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *usr = &ppdu_info->rx_status.userstats[userid];
	u16 sta_id;
	u8 sta_mcs;
	u8 coding;
	u8 nss;
	u32 info0;

	info0 = __le32_to_cpu(he_sig_b2_mu->info0);

	ppdu_info->cur_usr_idx = userid;
	sta_id = u32_get_bits(info0,
			      HAL_TX_MON_HE_SIG_B2_MU_INFO0_STA_ID);
	sta_mcs = u32_get_bits(info0,
			       HAL_TX_MON_HE_SIG_B2_MU_INFO0_STA_MCS);
	coding = u32_get_bits(info0,
			      HAL_TX_MON_HE_SIG_B2_MU_INFO0_STA_CODING);
	nss = u32_get_bits(info0,
			   HAL_TX_MON_HE_SIG_B2_MU_INFO0_NSTS) + 1;

	usr->he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
			 IEEE80211_RADIOTAP_HE_DATA1_CODING_KNOWN;
	usr->mcs = sta_mcs;
	usr->he_data3 |= (sta_mcs << HE_TRANSMIT_MCS_SHIFT) |
			 (coding << HE_CODING_SHIFT);
	usr->he_data4 |= sta_id << HE_STA_ID_SHIFT;
	usr->nss = nss;
	usr->he_data6 |= nss;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_he_sig_b2_ofdma
			(const struct hal_tx_mon_he_sig_b2_ofdma *he_sig_b2_ofdma,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *usr = &ppdu_info->rx_status.userstats[userid];
	u16 sta_id;
	u8 nss;
	u8 txbf;
	u8 sta_mcs;
	u8 sta_dcm;
	u8 coding;
	u32 info0;

	info0 = __le32_to_cpu(he_sig_b2_ofdma->info0);

	ppdu_info->cur_usr_idx = userid;
	sta_id = u32_get_bits(info0,
			      HAL_TX_MON_HE_SIG_B2_OFDMA_INFO0_STA_ID);
	nss = u32_get_bits(info0,
			   HAL_TX_MON_HE_SIG_B2_OFDMA_INFO0_NSTS);
	txbf = u32_get_bits(info0,
			    HAL_TX_MON_HE_SIG_B2_OFDMA_INFO0_TXBF);
	sta_mcs = u32_get_bits(info0,
			       HAL_TX_MON_HE_SIG_B2_OFDMA_INFO0_STA_MCS);
	sta_dcm = u32_get_bits(info0,
			       HAL_TX_MON_HE_SIG_B2_OFDMA_INFO0_STA_DCM);
	coding = u32_get_bits(info0,
			      HAL_TX_MON_HE_SIG_B2_OFDMA_INFO0_STA_CODING);

	usr->he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
			 IEEE80211_RADIOTAP_HE_DATA1_CODING_KNOWN |
			 IEEE80211_RADIOTAP_HE_DATA1_DATA_DCM_KNOWN;
	usr->he_data2 |= IEEE80211_RADIOTAP_HE_DATA2_TXBF_KNOWN;
	usr->mcs = sta_mcs;
	usr->he_data3 |= (sta_mcs << HE_TRANSMIT_MCS_SHIFT) |
			 (sta_dcm << HE_DCM_SHIFT) |
			 (coding << HE_CODING_SHIFT);
	usr->he_data4 |= sta_id << HE_STA_ID_SHIFT;
	usr->he_data5 |= txbf << HE_TXBF_SHIFT;
	usr->nss = nss;
	usr->he_data6 |= nss;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_vht_sig_a
				(const struct hal_tx_mon_mactx_vht_sig_a *vht_sig_a,
				 u32 userid,
				 struct hal_tx_mon_ppdu_info *ppdu_info,
				 struct hal_tx_mon_status_info *status_info)
{
	u8  bandwidth = 0;
	u8  is_stbc = 0;
	u8  group_id = 0;
	u32 nss_comb = 0;
	u8  nss_su = 0;
	u8  nss_mu[4] = {0};
	u8  sgi = 0;
	u8  coding = 0;
	u8  mcs = 0;
	u8  beamformed = 0;
	u16 partial_aid = 0;
	u32 info[2];

	info[0] = __le32_to_cpu(vht_sig_a->info0);
	info[1] = __le32_to_cpu(vht_sig_a->info1);

	bandwidth = u32_get_bits(info[0],
				 HAL_TX_MON_VHT_SIG_A_INFO0_BANDWIDTH);
	is_stbc = u32_get_bits(info[0],
			       HAL_TX_MON_VHT_SIG_A_INFO0_STBC);
	group_id = u32_get_bits(info[0],
				HAL_TX_MON_VHT_SIG_A_INFO0_GROUP_ID);
	nss_comb = u32_get_bits(info[0],
				HAL_TX_MON_VHT_SIG_A_INFO0_N_STS);
	sgi = u32_get_bits(info[1],
			   HAL_TX_MON_VHT_SIG_A_INFO1_GI_SETTING);
	coding = u32_get_bits(info[1],
			      HAL_TX_MON_VHT_SIG_A_INFO1_SU_MU_CODING);
	mcs = u32_get_bits(info[1],
			   HAL_TX_MON_VHT_SIG_A_INFO1_MCS);
	beamformed = u32_get_bits(info[1],
				  HAL_TX_MON_VHT_SIG_A_INFO1_BEAMFORMED);

	nss_su = (nss_comb & VHT_SIG_SU_NSS_MASK) + 1;
	partial_aid = (nss_comb >> 3) & VHT_SIG_SU_PARTIAL_AID_MASK;
	nss_mu[0] = (nss_comb & VHT_SIG_SU_NSS_MASK) + 1;
	nss_mu[1] = ((nss_comb >> 3) & VHT_SIG_SU_NSS_MASK) + 1;
	nss_mu[2] = ((nss_comb >> 6) & VHT_SIG_SU_NSS_MASK) + 1;
	nss_mu[3] = ((nss_comb >> 9) & VHT_SIG_SU_NSS_MASK) + 1;

	ppdu_info->rx_status.ldpc =
			(coding == HAL_RX_SU_MU_CODING_LDPC) ? 1 : 0;
	status_info->sw_frame_group_id = group_id;
	ppdu_info->rx_status.sgi = sgi;
	ppdu_info->rx_status.is_stbc = is_stbc;
	ppdu_info->rx_status.bw = bandwidth;
	ppdu_info->rx_status.beamformed = beamformed;

	if (group_id == 0 || group_id == 63) {
		ppdu_info->rx_status.reception_type =
			HAL_RX_RECEPTION_TYPE_SU;
		ppdu_info->rx_status.mcs = mcs;
		ppdu_info->rx_status.nss = nss_su & VHT_SIG_SU_NSS_MASK;
		ppdu_info->rx_status.userstats[userid].vht_flag_values3[0] =
			((mcs << 4) | nss_su);
	} else {
		ppdu_info->rx_status.reception_type =
			HAL_RX_RECEPTION_TYPE_MU_MIMO;
		ppdu_info->rx_status.userstats[userid].mcs = mcs;
		ppdu_info->rx_status.userstats[userid].nss =
			nss_su & VHT_SIG_SU_NSS_MASK;
		ppdu_info->rx_status.userstats[userid].vht_flag_values3[0] =
			((mcs << 4) | nss_mu[0]);
		ppdu_info->rx_status.userstats[userid].vht_flag_values3[1] =
			((mcs << 4) | nss_mu[1]);
		ppdu_info->rx_status.userstats[userid].vht_flag_values3[2] =
			((mcs << 4) | nss_mu[2]);
		ppdu_info->rx_status.userstats[userid].vht_flag_values3[3] =
			((mcs << 4) | nss_mu[3]);
	}

	ppdu_info->rx_status.userstats[userid].vht_flag_values2 = bandwidth;
	ppdu_info->rx_status.userstats[userid].vht_flag_values4 = coding;
	ppdu_info->rx_status.userstats[userid].vht_flag_values5 = group_id;
	ppdu_info->rx_status.userstats[userid].vht_flag_values6 = partial_aid;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_l_sig_a(const struct hal_tx_mon_l_sig_a *l_sig_a,
				      struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u8 rate = 0;
	u32 info0;

	info0 = __le32_to_cpu(l_sig_a->info0);

	rate = u32_get_bits(info0, HAL_TX_MON_L_SIG_A_INFO0_RATE);

	switch (rate) {
	case 8:
		ppdu_info->rx_status.rate = HAL_11A_RATE_0MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS0;
		break;
	case 9:
		ppdu_info->rx_status.rate = HAL_11A_RATE_1MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS1;
		break;
	case 10:
		ppdu_info->rx_status.rate = HAL_11A_RATE_2MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS2;
		break;
	case 11:
		ppdu_info->rx_status.rate = HAL_11A_RATE_3MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS3;
		break;
	case 12:
		ppdu_info->rx_status.rate = HAL_11A_RATE_4MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS4;
		break;
	case 13:
		ppdu_info->rx_status.rate = HAL_11A_RATE_5MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS5;
		break;
	case 14:
		ppdu_info->rx_status.rate = HAL_11A_RATE_6MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS6;
		break;
	case 15:
		ppdu_info->rx_status.rate = HAL_11A_RATE_7MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS7;
		break;
	default:
		break;
	}

	ppdu_info->rx_status.ofdm_flag = 1;
	ppdu_info->rx_status.reception_type = HAL_RX_RECEPTION_TYPE_SU;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_l_sig_b(const struct hal_tx_mon_l_sig_b *l_sig_b,
				      struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u8 rate = 0;
	u32 info0;

	info0 = __le32_to_cpu(l_sig_b->info0);

	rate = u32_get_bits(info0, HAL_TX_MON_L_SIG_B_INFO0_RATE);

	switch (rate) {
	case 1:
		ppdu_info->rx_status.rate = HAL_11B_RATE_3MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS3;
		break;
	case 2:
		ppdu_info->rx_status.rate = HAL_11B_RATE_2MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS2;
		break;
	case 3:
		ppdu_info->rx_status.rate = HAL_11B_RATE_1MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS1;
		break;
	case 4:
		ppdu_info->rx_status.rate = HAL_11B_RATE_0MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS0;
		break;
	case 5:
		ppdu_info->rx_status.rate = HAL_11B_RATE_6MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS6;
		break;
	case 6:
		ppdu_info->rx_status.rate = HAL_11B_RATE_5MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS5;
		break;
	case 7:
		ppdu_info->rx_status.rate = HAL_11B_RATE_4MCS;
		ppdu_info->rx_status.mcs = HAL_LEGACY_MCS4;
		break;
	default:
		break;
	}

	ppdu_info->rx_status.cck_flag = 1;
	ppdu_info->rx_status.reception_type = HAL_RX_RECEPTION_TYPE_SU;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_hdr(const void *tlv_data,
					struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_u_sig_eht_su_mu *su_mu = tlv_data;
	struct hal_mon_tx_usig_hdr *usig = (struct hal_mon_tx_usig_hdr *)tlv_data;
	struct hal_mon_tx_usig_cmn *usig_1 = &usig->usig_1;
	u8 bad_usig_crc;

	u32 info = __le32_to_cpu(su_mu->info0);

	bad_usig_crc = u32_get_bits(info,
				    HAL_TX_MON_U_SIG_EHT_SU_MU_INFO0_CRC) ? 0 : 1;

	ppdu_info->rx_status.usig_flags = 1;
	ppdu_info->rx_status.bw = usig_1->bw;
	ppdu_info->rx_status.usig_common |= bad_usig_crc;
	ppdu_info->rx_status.usig_common |= (usig_1->bw << USIG_BW_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->txop << USIG_TXOP_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->ul_dl << USIG_UL_DL_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->bss_color << USIG_BSS_COLOR_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->phy_version <<
			USIG_PHY_VERSION_SHIFT);
	ppdu_info->rx_status.usig_common |=
			IEEE80211_RADIOTAP_EHT_USIG_COMMON_PHY_VER_KNOWN |
			IEEE80211_RADIOTAP_EHT_USIG_COMMON_BW_KNOWN |
			IEEE80211_RADIOTAP_EHT_USIG_COMMON_UL_DL_KNOWN |
			IEEE80211_RADIOTAP_EHT_USIG_COMMON_BSS_COLOR_KNOWN |
			IEEE80211_RADIOTAP_EHT_USIG_COMMON_TXOP_KNOWN;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_eht_sig_non_mumimo_user_info
					(const void *tlv_data,
					 u32 userid,
					 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *usr = &ppdu_info->rx_status.userstats[userid];
	struct hal_mon_tx_eht_sig_non_mu_mimo_user_info *user_info =
		(struct hal_mon_tx_eht_sig_non_mu_mimo_user_info *)tlv_data;

	ppdu_info->rx_status.eht_flags = 1;
	ppdu_info->rx_status.mcs = user_info->mcs;
	ppdu_info->rx_status.nss = user_info->nss + 1;
	ppdu_info->rx_status.num_eht_user_info_valid += 1;
	usr->eht_user_info |= (user_info->mcs << EHT_MCS_SHIFT) |
			      (user_info->nss << EHT_NSS_SHIFT) |
			      (user_info->coding << EHT_CODING_SHIFT) |
			      (user_info->sta_id << EHT_STA_ID_SHIFT) |
			      (user_info->beamformed << EHT_BEAMFORMING_SHIFT) |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID_KNOWN |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_MCS_KNOWN |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_CODING_KNOWN |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_NSS_KNOWN_O |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_BEAMFORMING_KNOWN_O;

	/**
	 * num_user_info is a fill counter tracking number of written entries for
	 * user_info[]. The guard caps writes at EHT_MAX_USER_INFO regardless
	 * of num_users. user_idx is the destination slot (counter-derived); userid is the
	 * TLV-reported user index used to select the correct userstats entry.
	 * The two are independent and must not be conflated.
	 */
	if (ppdu_info->rx_status.eht_info.num_user_info <
		ARRAY_SIZE(ppdu_info->rx_status.eht_info.user_info)) {
		u32 user_idx = ppdu_info->rx_status.eht_info.num_user_info++;

		ppdu_info->rx_status.eht_info.user_info[user_idx] =
			ppdu_info->rx_status.userstats[userid].eht_user_info;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_eht_sig_mumimo_user_info
					(const void *tlv_data,
					 u32 userid,
					 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *usr = &ppdu_info->rx_status.userstats[userid];
	struct hal_mon_tx_eht_sig_mu_mimo_user_info *user_info =
			(struct hal_mon_tx_eht_sig_mu_mimo_user_info *)tlv_data;

	ppdu_info->rx_status.eht_flags = 1;
	ppdu_info->rx_status.mcs = user_info->mcs;
	ppdu_info->rx_status.num_eht_user_info_valid += 1;
	usr->eht_user_info |= (user_info->mcs << EHT_MCS_SHIFT) |
			      (user_info->sta_id << EHT_STA_ID_SHIFT) |
			      (user_info->coding << EHT_CODING_SHIFT) |
			      (user_info->spatial_coding << EHT_SPATIAL_CONFIG_SHIFT) |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID_KNOWN |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_MCS_KNOWN |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_CODING_KNOWN |
			      IEEE80211_RADIOTAP_EHT_USER_INFO_SPATIAL_CONFIG_KNOWN_M;

	/**
	 * num_user_info is a fill counter tracking number of written entries for
	 * user_info[]. The guard caps writes at EHT_MAX_USER_INFO regardless
	 * of num_users. user_idx is the destination slot (counter-derived); userid is the
	 * TLV-reported user index used to select the correct userstats entry.
	 * The two are independent and must not be conflated.
	 */
	if (ppdu_info->rx_status.eht_info.num_user_info <
		ARRAY_SIZE(ppdu_info->rx_status.eht_info.user_info)) {
		u32 user_idx = ppdu_info->rx_status.eht_info.num_user_info++;

		ppdu_info->rx_status.eht_info.user_info[user_idx] =
			ppdu_info->rx_status.userstats[userid].eht_user_info;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_common
			(struct hal_tx_mon_ppdu_info *ppdu_info,
			 const struct hal_mon_tx_u_sig_uhr_common_info *common_info)
{
	struct ieee80211_radiotap_uhr_usig *uhr_usig =
					&ppdu_info->rx_status.u_sig_info.uhr_usig;
	struct hal_mon_tx_u_sig_uhr_common_enc_info common_enc;
	u32 common;

	ppdu_info->rx_status.uhr_usig = true;
	ppdu_info->rx_status.is_uhr = true;
	ppdu_info->rx_status.bw = common_info->tx_bw;

	common_enc.phy_version =
			u32_encode_bits(common_info->phy_version,
					IEEE80211_RADIOTAP_UHR_USIG_COMMON_PHY_VER);
	common_enc.tx_bw = u32_encode_bits(common_info->tx_bw,
					   IEEE80211_RADIOTAP_UHR_USIG_COMMON_BW);
	common_enc.ul_dl = u32_encode_bits(common_info->ul_dl,
					   IEEE80211_RADIOTAP_UHR_USIG_COMMON_UL_DL);
	common_enc.bss_color =
			u32_encode_bits(common_info->bss_color,
					IEEE80211_RADIOTAP_UHR_USIG_COMMON_BSS_COLOR);
	common_enc.txop = u32_encode_bits(common_info->txop,
					  IEEE80211_RADIOTAP_UHR_USIG_COMMON_TXOP);

	common = __le32_to_cpu(uhr_usig->common);
	common |= IEEE80211_RADIOTAP_UHR_USIG_COMMON_PHY_VER_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_BW_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_UL_DL_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_BSS_COLOR_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_TXOP_KNOWN |
		  common_enc.phy_version | common_enc.tx_bw |
		  common_enc.ul_dl | common_enc.bss_color | common_enc.txop;
	uhr_usig->common = cpu_to_le32(common);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_mu
			(struct hal_tx_mon_ppdu_info *ppdu_info,
			 const struct hal_mon_tx_u_sig_uhr_mu_info *mu_info)
{
	struct ieee80211_radiotap_uhr_usig *uhr_usig =
					&ppdu_info->rx_status.u_sig_info.uhr_usig;
	struct hal_mon_tx_u_sig_uhr_mu_enc_info mu_enc;
	u32 common, value, mask;

	common = __le32_to_cpu(uhr_usig->common);
	value = __le32_to_cpu(uhr_usig->value);
	mask = __le32_to_cpu(uhr_usig->mask);

	mu_enc.ppdu_type =
		u32_encode_bits(mu_info->ppdu_type,
				IEEE80211_RADIOTAP_UHR_USIG2_MU_B0_B1_PPDU_TYPE);
	mu_enc.validate_or_cobf_cosr =
		u32_encode_bits(mu_info->validate_or_cobf_cosr,
				IEEE80211_RADIOTAP_UHR_USIG2_MU_B2_COBF_OR_COSR);
	mu_enc.punc_ch_info =
		u32_encode_bits(mu_info->punc_ch_info,
				IEEE80211_RADIOTAP_UHR_USIG2_MU_B3_B7_PUNCTURED_INFO);
	mu_enc.mcs = u32_encode_bits(mu_info->mcs,
				     IEEE80211_RADIOTAP_UHR_USIG2_MU_B9_B10_SIG_MCS);
	mu_enc.num_uhr_sig_symbols =
		u32_encode_bits(mu_info->num_uhr_sig_symbols,
				IEEE80211_RADIOTAP_UHR_USIG2_MU_B11_B15_NUM_UHR_SYG_SYM);
	mu_enc.crc = u32_encode_bits(mu_info->crc,
				     IEEE80211_RADIOTAP_UHR_USIG2_MU_B16_B19_CRC);
	mu_enc.tail = u32_encode_bits(mu_info->tail,
				      IEEE80211_RADIOTAP_UHR_USIG2_MU_B20_B25_TAIL);
	mu_enc.bad_crc =
		u32_encode_bits(!mu_info->rx_integ_check,
				IEEE80211_RADIOTAP_UHR_USIG_COMMON_BAD_USIG_CRC);
	mu_enc.shared_ap_bss_color =
		u32_encode_bits(mu_info->shared_ap_bss_color,
				IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B25_BSS_COLOR_2);

	common |= mu_enc.bad_crc;
	value |= mu_enc.ppdu_type | mu_enc.validate_or_cobf_cosr | mu_enc.punc_ch_info |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B8_VALIDATE |
		 mu_enc.mcs | mu_enc.num_uhr_sig_symbols | mu_enc.crc | mu_enc.tail;

	if (mu_info->validate_or_cobf_cosr)
		value |= IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B24_DISREGARD |
			 IEEE80211_RADIOTAP_UHR_USIG1_MU_B25_VALIDATE;
	else
		value |= mu_enc.shared_ap_bss_color;

	mask |= IEEE80211_RADIOTAP_UHR_USIG2_MU_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B2_COBF_OR_COSR |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B3_B7_PUNCTURED_INFO |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B8_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B9_B10_SIG_MCS |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B11_B15_NUM_UHR_SYG_SYM |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_MU_B20_B25_TAIL;

	if (mu_info->validate_or_cobf_cosr)
		mask |= IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B24_DISREGARD |
			IEEE80211_RADIOTAP_UHR_USIG1_MU_B25_VALIDATE;
	else
		mask |= IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B25_BSS_COLOR_2;

	uhr_usig->common = cpu_to_le32(common);
	uhr_usig->value = cpu_to_le32(value);
	uhr_usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_su_mu(const void *tlv_data,
					      struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_u_sig_uhr_su_mu *su_mu = tlv_data;
	u32 info0 = __le32_to_cpu(su_mu->info0);
	u32 info1 = __le32_to_cpu(su_mu->info1);
	struct hal_mon_tx_u_sig_uhr_mu_info uhr_mu_info;

	uhr_mu_info.common.phy_version = u32_get_bits(info0,
					HAL_TX_MON_U_SIG_UHR_SU_MU_INFO0_PHY_VERSION);
	uhr_mu_info.common.tx_bw = u32_get_bits(info0,
					   HAL_TX_MON_U_SIG_UHR_SU_MU_INFO0_TRANSMIT_BW);
	uhr_mu_info.common.ul_dl = u32_get_bits(info0,
					   HAL_TX_MON_U_SIG_UHR_SU_MU_INFO0_DL_UL_FLAG);
	uhr_mu_info.common.bss_color =
		u32_get_bits(info0,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO0_BSS_COLOR_ID);
	uhr_mu_info.common.txop =
		u32_get_bits(info0,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO0_TXOP_DURATION);
	uhr_mu_info.ppdu_type =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_UHR_PPDU_SIG_CMN_TYPE);
	uhr_mu_info.validate_or_cobf_cosr =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_VALIDATE_OR_COBF_COSR);
	uhr_mu_info.punc_ch_info =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_PUNC_CHAN_INFO);
	uhr_mu_info.mcs = u32_get_bits(info1,
				  HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_MCS_OF_UHR_SIG);
	uhr_mu_info.num_uhr_sig_symbols =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_NUM_UHR_SIG_SYMBOLS);
	uhr_mu_info.crc = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_CRC);
	uhr_mu_info.tail = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_TAIL);
	uhr_mu_info.rx_integ_check =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_SU_MU_INFO1_RX_INTEG_CHECK);
	uhr_mu_info.shared_ap_bss_color = uhr_mu_info.common.bss_color;

	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_common(ppdu_info,
							 &uhr_mu_info.common);
	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_mu(ppdu_info, &uhr_mu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_cobf(const void *tlv_data,
					     struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_u_sig_uhr_cobf *cobf = tlv_data;
	u32 info0 = __le32_to_cpu(cobf->info0);
	u32 info1 = __le32_to_cpu(cobf->info1);
	struct hal_mon_tx_u_sig_uhr_mu_info uhr_mu_info;

	uhr_mu_info.common.phy_version = u32_get_bits(info0,
					HAL_TX_MON_U_SIG_UHR_COBF_INFO0_PHY_VERSION);
	uhr_mu_info.common.tx_bw = u32_get_bits(info0,
					   HAL_TX_MON_U_SIG_UHR_COBF_INFO0_TRANSMIT_BW);
	uhr_mu_info.common.ul_dl = u32_get_bits(info0,
					   HAL_TX_MON_U_SIG_UHR_COBF_INFO0_DL_UL_FLAG);
	uhr_mu_info.common.bss_color =
		u32_get_bits(info0,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO0_SHARING_AP_BSS_COLOR_ID);
	uhr_mu_info.common.txop = u32_get_bits(info0,
					  HAL_TX_MON_U_SIG_UHR_COBF_INFO0_TXOP_DURATION);
	uhr_mu_info.ppdu_type =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO1_UHR_PPDU_SIG_CMN_TYPE);
	uhr_mu_info.validate_or_cobf_cosr =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO1_COBF_COSR_DISABLE);
	uhr_mu_info.punc_ch_info =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO1_PUNC_CHAN_INFO);
	uhr_mu_info.mcs = u32_get_bits(info1,
				  HAL_TX_MON_U_SIG_UHR_COBF_INFO1_MCS_OF_UHR_SIG);
	uhr_mu_info.num_uhr_sig_symbols =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO1_NUM_UHR_SIG_SYMBOLS);
	uhr_mu_info.crc = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_COBF_INFO1_CRC);
	uhr_mu_info.tail = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_COBF_INFO1_TAIL);
	uhr_mu_info.rx_integ_check =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO1_RX_INTEG_CHECK);
	uhr_mu_info.shared_ap_bss_color =
		u32_get_bits(info0,
			     HAL_TX_MON_U_SIG_UHR_COBF_INFO0_SHARED_AP_BSS_COLOR_ID);

	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_common(ppdu_info,
							 &uhr_mu_info.common);
	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_mu(ppdu_info, &uhr_mu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_cosr(const void *tlv_data,
					     struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_u_sig_uhr_cosr *cosr = tlv_data;
	u32 info0 = __le32_to_cpu(cosr->info0);
	u32 info1 = __le32_to_cpu(cosr->info1);
	struct hal_mon_tx_u_sig_uhr_mu_info uhr_mu_info;

	uhr_mu_info.common.phy_version = u32_get_bits(info0,
					HAL_TX_MON_U_SIG_UHR_COSR_INFO0_PHY_VERSION);
	uhr_mu_info.common.tx_bw = u32_get_bits(info0,
					   HAL_TX_MON_U_SIG_UHR_COSR_INFO0_TRANSMIT_BW);
	uhr_mu_info.common.ul_dl = u32_get_bits(info0,
					   HAL_TX_MON_U_SIG_UHR_COSR_INFO0_DL_UL_FLAG);
	uhr_mu_info.common.bss_color =
		u32_get_bits(info0,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO0_SHARING_AP_BSS_COLOR_ID);
	uhr_mu_info.common.txop = u32_get_bits(info0,
					  HAL_TX_MON_U_SIG_UHR_COSR_INFO0_TXOP_DURATION);
	uhr_mu_info.ppdu_type =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO1_UHR_PPDU_SIG_CMN_TYPE);
	uhr_mu_info.validate_or_cobf_cosr =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO1_COBF_COSR_DISABLE);
	uhr_mu_info.punc_ch_info =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO1_PUNC_CHAN_INFO);
	uhr_mu_info.mcs =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO1_MCS_OF_UHR_SIG);
	uhr_mu_info.num_uhr_sig_symbols =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO1_NUM_UHR_SIG_SYMBOLS);
	uhr_mu_info.crc = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_COSR_INFO1_CRC);
	uhr_mu_info.tail = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_COSR_INFO1_TAIL);
	uhr_mu_info.rx_integ_check =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO1_RX_INTEG_CHECK);
	uhr_mu_info.shared_ap_bss_color =
		u32_get_bits(info0,
			     HAL_TX_MON_U_SIG_UHR_COSR_INFO0_SHARED_AP_BSS_COLOR_ID);

	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_common(ppdu_info,
							 &uhr_mu_info.common);
	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_mu(ppdu_info, &uhr_mu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_tb(const void *tlv_data,
					   struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_u_sig_uhr_tb *tb = tlv_data;
	struct ieee80211_radiotap_uhr_usig *uhr_usig =
					&ppdu_info->rx_status.u_sig_info.uhr_usig;
	struct hal_mon_tx_u_sig_uhr_tb_info uhr_tb_info;
	struct hal_mon_tx_u_sig_uhr_tb_enc_info uhr_tb_enc;
	u32 info0, info1, common, value, mask;

	info0 = __le32_to_cpu(tb->info0);
	info1 = __le32_to_cpu(tb->info1);
	uhr_tb_info.common.phy_version = u32_get_bits(info0,
				 HAL_TX_MON_U_SIG_UHR_TB_INFO0_PHY_VERSION);
	uhr_tb_info.common.tx_bw = u32_get_bits(info0,
			 HAL_TX_MON_U_SIG_UHR_TB_INFO0_TRANSMIT_BW);
	uhr_tb_info.common.ul_dl = u32_get_bits(info0,
			 HAL_TX_MON_U_SIG_UHR_TB_INFO0_DL_UL_FLAG);
	uhr_tb_info.common.bss_color = u32_get_bits(info0,
				 HAL_TX_MON_U_SIG_UHR_TB_INFO0_BSS_COLOR_ID);
	uhr_tb_info.common.txop = u32_get_bits(info0,
			    HAL_TX_MON_U_SIG_UHR_TB_INFO0_TXOP_DURATION);
	uhr_tb_info.ppdu_type = u32_get_bits(info1,
				 HAL_TX_MON_U_SIG_UHR_TB_INFO1_UHR_PPDU_SIG_CMN_TYPE);
	uhr_tb_info.spatial_reuse = u32_get_bits(info1,
				     HAL_TX_MON_U_SIG_UHR_TB_INFO1_SPATIAL_REUSE);
	uhr_tb_info.crc = u32_get_bits(info1,
			   HAL_TX_MON_U_SIG_UHR_TB_INFO1_CRC);
	uhr_tb_info.tail = u32_get_bits(info1,
			    HAL_TX_MON_U_SIG_UHR_TB_INFO1_TAIL);
	uhr_tb_info.rx_integ_check = u32_get_bits(info1,
				      HAL_TX_MON_U_SIG_UHR_TB_INFO1_RX_INTEG_CHECK);

	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_common(ppdu_info,
							 &uhr_tb_info.common);
	uhr_tb_enc.spatial_reuse_1 =
		u32_encode_bits(uhr_tb_info.spatial_reuse &
				UHR_SPATIAL_REUSE_MASK,
				IEEE80211_RADIOTAP_UHR_USIG2_TB_B3_B6_SPATIAL_REUSE_1);
	uhr_tb_enc.spatial_reuse_2 =
		u32_encode_bits((uhr_tb_info.spatial_reuse >>
				UHR_SPATIAL_REUSE_SHFT) &
				UHR_SPATIAL_REUSE_MASK,
				IEEE80211_RADIOTAP_UHR_USIG2_TB_B7_B10_SPATIAL_REUSE_2);
	uhr_tb_enc.ppdu_type =
		u32_encode_bits(uhr_tb_info.ppdu_type,
				IEEE80211_RADIOTAP_UHR_USIG2_TB_B0_B1_PPDU_TYPE);
	uhr_tb_enc.crc = u32_encode_bits(uhr_tb_info.crc,
					 IEEE80211_RADIOTAP_UHR_USIG2_TB_B16_B19_CRC);
	uhr_tb_enc.tail = u32_encode_bits(uhr_tb_info.tail,
					  IEEE80211_RADIOTAP_UHR_USIG2_TB_B20_B25_TAIL);
	uhr_tb_enc.bad_crc =
		u32_encode_bits(!uhr_tb_info.rx_integ_check,
				IEEE80211_RADIOTAP_UHR_USIG_COMMON_BAD_USIG_CRC);

	common = __le32_to_cpu(uhr_usig->common);
	value = __le32_to_cpu(uhr_usig->value);
	mask = __le32_to_cpu(uhr_usig->mask);

	common |= uhr_tb_enc.bad_crc;
	value |= IEEE80211_RADIOTAP_UHR_USIG1_TB_B20_B25_DISREGARD |
		 uhr_tb_enc.ppdu_type |
		 IEEE80211_RADIOTAP_UHR_USIG2_TB_B2_VALIDATE |
		 uhr_tb_enc.spatial_reuse_1 |
		 uhr_tb_enc.spatial_reuse_2 |
		 IEEE80211_RADIOTAP_UHR_USIG2_TB_B11_B15_DISREGARD |
		 uhr_tb_enc.crc | uhr_tb_enc.tail;
	mask |= IEEE80211_RADIOTAP_UHR_USIG1_TB_B20_B25_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B2_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B3_B6_SPATIAL_REUSE_1 |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B7_B10_SPATIAL_REUSE_2 |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B11_B15_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B20_B25_TAIL;

	uhr_usig->common = cpu_to_le32(common);
	uhr_usig->value = cpu_to_le32(value);
	uhr_usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_elr(const void *tlv_data,
					    struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_u_sig_uhr_elr *elr = tlv_data;
	struct ieee80211_radiotap_uhr_usig *uhr_usig =
						&ppdu_info->rx_status.u_sig_info.uhr_usig;
	struct hal_mon_tx_u_sig_uhr_elr_info uhr_elr_info;
	struct hal_mon_tx_u_sig_uhr_elr_enc_info uhr_elr_enc;
	u32 info0, info1, value, mask;

	info0 = __le32_to_cpu(elr->info0);
	info1 = __le32_to_cpu(elr->info1);
	uhr_elr_info.common.phy_version = u32_get_bits(info0,
				   HAL_TX_MON_U_SIG_UHR_ELR_INFO0_PHY_VERSION);
	uhr_elr_info.common.tx_bw = u32_get_bits(info0,
				     HAL_TX_MON_U_SIG_UHR_ELR_INFO0_TRANSMIT_BW);
	uhr_elr_info.common.ul_dl = u32_get_bits(info0,
				     HAL_TX_MON_U_SIG_UHR_ELR_INFO0_DL_UL_FLAG);
	uhr_elr_info.common.bss_color = u32_get_bits(info0,
					 HAL_TX_MON_U_SIG_UHR_ELR_INFO0_BSS_COLOR_ID);
	uhr_elr_info.common.txop = u32_get_bits(info0,
				    HAL_TX_MON_U_SIG_UHR_ELR_INFO0_TXOP_DURATION);
	uhr_elr_info.ppdu_type =
		u32_get_bits(info1,
			     HAL_TX_MON_U_SIG_UHR_ELR_INFO1_UHR_PPDU_SIG_CMN_TYPE);
	uhr_elr_info.sta_id = u32_get_bits(info1,
					   HAL_TX_MON_U_SIG_UHR_ELR_INFO1_STA_ID);
	uhr_elr_info.validate = u32_get_bits(info1,
					     HAL_TX_MON_U_SIG_UHR_ELR_INFO1_VALIDATE_1);
	uhr_elr_info.crc = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_ELR_INFO1_CRC);
	uhr_elr_info.tail = u32_get_bits(info1, HAL_TX_MON_U_SIG_UHR_ELR_INFO1_TAIL);

	ath12k_wifi8_hal_mon_tx_update_u_sig_uhr_common(ppdu_info,
							 &uhr_elr_info.common);
	uhr_elr_enc.ppdu_type =
		u32_encode_bits(uhr_elr_info.ppdu_type,
				IEEE80211_RADIOTAP_UHR_USIG2_ELR_B0_B1_PPDU_TYPE);
	uhr_elr_enc.sta_id =
		u32_encode_bits(uhr_elr_info.sta_id,
				IEEE80211_RADIOTAP_UHR_USIG2_ELR_B2_B12_STA_ID);
	uhr_elr_enc.validate =
		u32_encode_bits(uhr_elr_info.validate,
				IEEE80211_RADIOTAP_UHR_USIG2_ELR_B13_B15_ELR_VALIDATE);
	uhr_elr_enc.crc = u32_encode_bits(uhr_elr_info.crc,
					  IEEE80211_RADIOTAP_UHR_USIG2_ELR_B16_B19_CRC);
	uhr_elr_enc.tail = u32_encode_bits(uhr_elr_info.tail,
					   IEEE80211_RADIOTAP_UHR_USIG2_ELR_B20_B25_TAIL);

	value = __le32_to_cpu(uhr_usig->value);
	mask = __le32_to_cpu(uhr_usig->mask);

	value |= IEEE80211_RADIOTAP_UHR_USIG1_ELR_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_UHR_USIG1_ELR_B25_VALIDATE |
		 uhr_elr_enc.ppdu_type | uhr_elr_enc.sta_id | uhr_elr_enc.validate |
		 uhr_elr_enc.crc | uhr_elr_enc.tail;
	mask |= IEEE80211_RADIOTAP_UHR_USIG1_ELR_B20_B24_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG1_ELR_B25_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B2_B12_STA_ID |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B13_B15_ELR_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B20_B25_TAIL;

	uhr_usig->value = cpu_to_le32(value);
	uhr_usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_uhr_sig_non_mumimo_user_info
				(u32 info0,
				 u32 userid, bool is_ofdma,
				 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->rx_status.uhr_info;
	u32 user_idx, user_known = 0, user_info = 0;
	u16 sta_id;
	u8 mcs, nss, ueqm, txbf_coding;

	if (userid >= UHR_MAX_USER_INFO ||
			userid >= ARRAY_SIZE(ppdu_info->rx_status.userstats))
		return;

	user_idx = userid;
	if (uhr_info->num_user_info <= user_idx)
		uhr_info->num_user_info = user_idx + 1;

	user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_KNOWN_O |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN;

	sta_id = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_SU_INFO0_STA_ID);
	mcs = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_SU_INFO0_STA_MCS);
	nss = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_SU_INFO0_NSS) + 1;
	ueqm = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_SU_INFO0_UNEQ_MODULATION);
	txbf_coding = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_SU_INFO0_TXBF_CODING);

	user_info |= u32_encode_bits(sta_id, IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		     u32_encode_bits(mcs, IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		     u32_encode_bits(nss, IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_O);

	if (is_ofdma || ueqm) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_KNOWN_O;
		user_info |= u32_encode_bits(ueqm,
					     IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_O) |
			     u32_encode_bits
				(txbf_coding,
				 IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_O);
	} else {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_KNOWN_O;
		user_info |=
			u32_encode_bits(!!(txbf_coding &
					UHR_USER_INFO_BEAMFORMING_KNOWN),
					IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_NO) |
			u32_encode_bits(!!(txbf_coding & UHR_USER_INFO_CODING_KNOWN),
					IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO);
	}

	/* TX UHR user TLV bit layout is not fully aligned with RX decode for
	 * per-user LDPC in monitor format. Do not emit 2xLDPC user bit from
	 * this field to avoid misreporting in radiotap user_info.
	 */
	user_known &= ~IEEE80211_RADIOTAP_UHR_USER_INFO_DATA_FOR_USER;

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);
	put_unaligned_le32(user_info, &uhr_info->user_info[user_idx]);

	ppdu_info->rx_status.is_uhr = true;
	ppdu_info->rx_status.mcs = mcs;
	ppdu_info->rx_status.nss = nss;
	ppdu_info->rx_status.ldpc = 0;
	ppdu_info->rx_status.userstats[user_idx].eht_user_info = user_info;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_ofdma(const void *tlv_data,
						u32 userid,
						struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_uhr_sig_usr_ofdma *ofdma = tlv_data;

	ath12k_wifi8_hal_mon_tx_parse_uhr_sig_non_mumimo_user_info(
			__le32_to_cpu(ofdma->info0), userid, true, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_cobf(const void *tlv_data,
					       u32 userid,
					       struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_uhr_sig_usr_cobf *cobf = tlv_data;

	ath12k_wifi8_hal_mon_tx_parse_uhr_sig_non_mumimo_user_info(
			__le32_to_cpu(cobf->info0), userid, true, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_cosr(const void *tlv_data,
					       u32 userid,
					       struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_uhr_sig_usr_cosr *cosr = tlv_data;

	ath12k_wifi8_hal_mon_tx_parse_uhr_sig_non_mumimo_user_info(
			__le32_to_cpu(cosr->info0), userid, true, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_su(const void *tlv_data,
					     u32 userid,
					     struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_uhr_sig_usr_su *su = tlv_data;

	ath12k_wifi8_hal_mon_tx_parse_uhr_sig_non_mumimo_user_info(
			__le32_to_cpu(su->info0), userid, false, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_uhr_sig_mumimo_user_info(
					const void *tlv_data,
					u32 userid,
					struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_uhr_sig_usr_mu_mimo *mu_mimo = tlv_data;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->rx_status.uhr_info;
	u32 user_idx, user_known = 0, user_info = 0, info0;
	u16 sta_id;
	u8 mcs, spatial_cfg;

	if (userid >= UHR_MAX_USER_INFO ||
				userid >= ARRAY_SIZE(ppdu_info->rx_status.userstats))
		return;

	info0 = __le32_to_cpu(mu_mimo->info0);
	sta_id = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_MU_MIMO_INFO0_STA_ID);
	mcs = u32_get_bits(info0, HAL_TX_MON_UHR_SIG_USR_MU_MIMO_INFO0_STA_MCS);
	spatial_cfg =
		u32_get_bits(info0,
			     HAL_TX_MON_UHR_SIG_USR_MU_MIMO_INFO0_STA_SPATIAL_CONFIG);

	user_idx = userid;
	if (uhr_info->num_user_info <= user_idx)
		uhr_info->num_user_info = user_idx + 1;

	user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_SPATIAL_CONFIG_KNOWN_NO |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_KNOWN_O |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		      IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN;

	user_info |= u32_encode_bits(sta_id, IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		     u32_encode_bits(mcs, IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		     u32_encode_bits(spatial_cfg,
				     IEEE80211_RADIOTAP_UHR_USER_INFO_SPATIAL_CONFIG_NO);

	user_known &= ~IEEE80211_RADIOTAP_UHR_USER_INFO_DATA_FOR_USER;

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);
	put_unaligned_le32(user_info, &uhr_info->user_info[user_idx]);

	ppdu_info->rx_status.is_uhr = true;
	ppdu_info->rx_status.mcs = mcs;
	ppdu_info->rx_status.ldpc = 0;
	ppdu_info->rx_status.userstats[user_idx].eht_user_info = user_info;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_get_user_desc_per_user
			(const struct hal_tx_mon_user_desc_per_user *desc_per_user,
			 struct hal_mon_tx_user_desc_per_user *usr)
{
	u32 info[4];

	info[0] = __le32_to_cpu(desc_per_user->info0);
	info[1] = __le32_to_cpu(desc_per_user->info1);
	info[2] = __le32_to_cpu(desc_per_user->info2);
	info[3] = __le32_to_cpu(desc_per_user->info3);

	usr->psdu_length =
		u32_get_bits(info[0],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO0_PSDU_LENGTH);
	usr->ru_start_index =
		u32_get_bits(info[1],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO1_RU_START_INDEX);
	usr->ru_size =
		u32_get_bits(info[1],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO1_RU_SIZE);
	usr->ofdma_mu_mimo_enabled =
		u32_get_bits(info[1],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO1_OFDMA_MU_MIMO_ENABLED);
	usr->nss = u32_get_bits(info[1],
				HAL_TX_MON_USER_DESC_PER_USER_INFO1_NSS);
	usr->stream_offset =
		u32_get_bits(info[1],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO1_STREAM_OFFSET);
	usr->mcs = u32_get_bits(info[1],
				HAL_TX_MON_USER_DESC_PER_USER_INFO1_MCS);
	usr->dcm = u32_get_bits(info[1],
				HAL_TX_MON_USER_DESC_PER_USER_INFO1_DCM);
	usr->fec_type =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO2_FEC_TYPE);
	usr->user_bf_type =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO2_USER_BF_TYPE);
	usr->drop_user_cbf =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO2_DROP_USER_CBF);
	usr->ldpc_extra_symbol =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO2_LDPC_EXTRA_SYMBOL);
	usr->force_extra_symbol =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO2_FORCE_EXTRA_SYMBOL);
	usr->sw_peer_id =
		u32_get_bits(info[3],
			     HAL_TX_MON_USER_DESC_PER_USER_INFO3_SW_PEER_ID);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_populate_he_data_per_user
			(struct hal_mon_tx_user_desc_per_user *usr,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u32 he_data1 = ppdu_info->rx_status.userstats[userid].he_data1;
	u32 he_data2 = ppdu_info->rx_status.userstats[userid].he_data2;
	u32 he_data3 = ppdu_info->rx_status.userstats[userid].he_data3;
	u32 he_data5 = ppdu_info->rx_status.userstats[userid].he_data5;
	u32 he_data6 = ppdu_info->rx_status.userstats[userid].he_data6;

	he_data1 |= IEEE80211_RADIOTAP_HE_DATA2_TXBF_KNOWN;
	he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_UL_DL_KNOWN;
	he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN;
	he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_DATA_DCM_KNOWN;
	he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_BEAM_CHANGE_KNOWN;
	he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_LDPC_XSYMSEG_KNOWN;

	he_data3 |= (1 << HE_DL_UL_SHIFT);
	he_data3 |= (usr->dcm << HE_DCM_SHIFT);
	he_data3 |= (usr->mcs << HE_TRANSMIT_MCS_SHIFT);
	he_data3 |= (!!usr->user_bf_type << HE_BEAM_CHANGE_SHIFT);
	he_data3 |= (usr->ldpc_extra_symbol << HE_LDPC_EXTRA_SYMBOL_SHIFT);

	he_data5 |= (!!usr->user_bf_type << HE_TXBF_SHIFT);

	if (ppdu_info->su_or_mu) {
		he_data2 |= IEEE80211_RADIOTAP_HE_DATA2_RU_OFFSET_KNOWN;
		he_data2 |= (ru_alloc_offset[usr->ru_size]
					[usr->ru_start_index] << HE_RU_ALLOCATION_SHIFT);
	}

	if (usr->ru_size < MAX_RU_INDEX) {
		he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN;
		he_data5 = (he_data5 & HE_DATA5_INFO_MASK) | (4 + usr->ru_size);
	}

	he_data6 |= (usr->nss & HE_NSTS_DATA6_MASK);

	ppdu_info->rx_status.userstats[userid].mcs = usr->mcs;
	ppdu_info->rx_status.userstats[userid].he_data1 = he_data1;
	ppdu_info->rx_status.userstats[userid].he_data2 = he_data2;
	ppdu_info->rx_status.userstats[userid].he_data3 = he_data3;
	ppdu_info->rx_status.userstats[userid].he_data5 = he_data5;
	ppdu_info->rx_status.userstats[userid].he_data6 = he_data6;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_populate_eht_sig_per_user
			(struct hal_mon_tx_user_desc_per_user *usr,
			 u32 userid,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	ppdu_info->rx_status.eht_known |= IEEE80211_RADIOTAP_EHT_KNOWN_LDPC_EXTRA_SYM_OM;
	ppdu_info->rx_status.eht_data[0] |= (usr->ldpc_extra_symbol <<
					EHT_LDPC_EXTRA_SYMBOL_SEG_SHIFT);
	ppdu_info->rx_status.eht_info.eht.known = ppdu_info->rx_status.eht_known;
	ppdu_info->rx_status.eht_info.eht.data[0] = ppdu_info->rx_status.eht_data[0];
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_user_desc_per_user(const void *tlv_data,
						 u32 userid,
						 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_mon_tx_user_desc_per_user usr_info = {0};

	ath12k_wifi8_hal_mon_tx_get_user_desc_per_user(tlv_data, &usr_info);
	if (ppdu_info->rx_status.he_flags) {
		ath12k_wifi8_hal_mon_tx_populate_he_data_per_user(&usr_info,
								  userid,
								  ppdu_info);
	}
	ath12k_wifi8_hal_mon_tx_populate_eht_sig_per_user(&usr_info,
							  userid,
							  ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_tx_mon_user_desc_info_compact(
	u32 *info, const struct hal_tx_mon_user_desc_common_compact *c)
{
	info[0]  = __le32_to_cpu(c->info0);
	info[1]  = __le32_to_cpu(c->info1);
	info[2]  = __le32_to_cpu(c->info2);
	info[3]  = __le32_to_cpu(c->info3);
	info[4]  = __le32_to_cpu(c->info4);
	info[5]  = __le32_to_cpu(c->info5);
	info[6]  = __le32_to_cpu(c->info6);
	info[7]  = __le32_to_cpu(c->info7);
	info[8]  = __le32_to_cpu(c->info8);
	info[9]  = __le32_to_cpu(c->info9);
	info[10] = __le32_to_cpu(c->info10);
	info[11] = __le32_to_cpu(c->info11);
	info[12] = __le32_to_cpu(c->info12);
	info[13] = __le32_to_cpu(c->info13);
}

static __always_inline void
ath12k_wifi8_hal_tx_mon_user_desc_info(u32 *info,
				       const struct hal_tx_mon_user_desc_common *f)
{
	info[0]  = __le32_to_cpu(f->info0);
	info[1]  = __le32_to_cpu(f->info1);
	info[2]  = __le32_to_cpu(f->info2);
	info[3]  = __le32_to_cpu(f->info3);
	info[4]  = __le32_to_cpu(f->info4);
	info[5]  = __le32_to_cpu(f->info5);
	info[6]  = __le32_to_cpu(f->info6);
	info[7]  = __le32_to_cpu(f->info7);
	info[8]  = __le32_to_cpu(f->info8);
	info[9]  = __le32_to_cpu(f->info9);
	info[10] = __le32_to_cpu(f->info10);
	info[11] = __le32_to_cpu(f->info11);
	info[12] = __le32_to_cpu(f->info12);
	info[13] = __le32_to_cpu(f->info13);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_get_user_desc_common
			(const void *tlv_data,
			 struct hal_mon_tx_usr_desc_common *usr_common,
			 u16 tlv_len)
{
	u8 num_ltf_symbols;
	u32 info[15];

	if (likely(tlv_len < HAL_MON_TX_USER_DESC_COMMON_TLV_SIZE))
		ath12k_wifi8_hal_tx_mon_user_desc_info_compact(info, tlv_data);
	else
		ath12k_wifi8_hal_tx_mon_user_desc_info(info, tlv_data);
	info[14] = info[3];

	usr_common->ltf_size =
		u32_get_bits(info[0],
			     HAL_TX_MON_USER_INFO0_LTF_SIZE);
	usr_common->pkt_extn_pe =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_INFO2_PE_DISAMBIGUITY);
	usr_common->a_factor =
		u32_get_bits(info[2],
			     HAL_TX_MON_USER_INFO2_PE_A_FACTOR);
	usr_common->center_ru_0 =
		u32_get_bits(info[3],
			     HAL_TX_MON_USER_INFO3_CENTER_RU_0);
	usr_common->center_ru_1 =
		u32_get_bits(info[3],
			     HAL_TX_MON_USER_INFO3_CENTER_RU_1);
	num_ltf_symbols =
		u32_get_bits(info[1],
			     HAL_TX_MON_USER_INFO1_NUM_LTF_SYMBOLS);

	usr_common->num_ltf_symbols = (num_ltf_symbols + 1) >> 1;
	usr_common->doppler_indication =
		u32_get_bits(info[12],
			     HAL_TX_MON_USER_INFO12_DOPPLER_INDICATION);
	usr_common->spatial_reuse =
		u32_get_bits(info[13],
			     HAL_TX_MON_USER_INFO13_SPATIAL_REUSE);
	usr_common->gi =
		u32_get_bits(info[14],
			     HAL_TX_MON_PHY_DESC_INFO3_CP_SETTING);
	usr_common->ru_channel_0[0] =
		u32_get_bits(info[4],
			     HAL_TX_MON_USER_INFO4_RU_ALLOC_0123_BAND0_0);
	usr_common->ru_channel_0[1] =
		u32_get_bits(info[4],
			     HAL_TX_MON_USER_INFO4_RU_ALLOC_0123_BAND0_1);
	usr_common->ru_channel_0[2] =
		u32_get_bits(info[5],
			     HAL_TX_MON_USER_INFO5_RU_ALLOC_0123_BAND0_2);
	usr_common->ru_channel_0[3] =
		u32_get_bits(info[5],
			     HAL_TX_MON_USER_INFO5_RU_ALLOC_0123_BAND0_3);
	usr_common->ru_channel_0[4] =
		u32_get_bits(info[8],
			     HAL_TX_MON_USER_INFO8_RU_ALLOC_4567_BAND0_0);
	usr_common->ru_channel_0[5] =
		u32_get_bits(info[8],
			     HAL_TX_MON_USER_INFO8_RU_ALLOC_4567_BAND0_1);
	usr_common->ru_channel_0[6] =
		u32_get_bits(info[9],
			     HAL_TX_MON_USER_INFO9_RU_ALLOC_4567_BAND0_2);
	usr_common->ru_channel_0[7] =
		u32_get_bits(info[9],
			     HAL_TX_MON_USER_INFO9_RU_ALLOC_4567_BAND0_3);
	usr_common->ru_channel_1[0] =
		u32_get_bits(info[6],
			     HAL_TX_MON_USER_INFO6_RU_ALLOC_0123_BAND1_0);
	usr_common->ru_channel_1[1] =
		u32_get_bits(info[6],
			     HAL_TX_MON_USER_INFO6_RU_ALLOC_0123_BAND1_1);
	usr_common->ru_channel_1[2] =
		u32_get_bits(info[7],
			     HAL_TX_MON_USER_INFO7_RU_ALLOC_0123_BAND1_2);
	usr_common->ru_channel_1[3] =
		u32_get_bits(info[7],
			     HAL_TX_MON_USER_INFO7_RU_ALLOC_0123_BAND1_3);
	usr_common->ru_channel_1[4] =
		u32_get_bits(info[10],
			     HAL_TX_MON_USER_INFO10_RU_ALLOC_4567_BAND1_0);
	usr_common->ru_channel_1[5] =
		u32_get_bits(info[10],
			     HAL_TX_MON_USER_INFO10_RU_ALLOC_4567_BAND1_1);
	usr_common->ru_channel_1[6] =
		u32_get_bits(info[11],
			     HAL_TX_MON_USER_INFO11_RU_ALLOC_4567_BAND1_2);
	usr_common->ru_channel_1[7] =
		u32_get_bits(info[11],
			     HAL_TX_MON_USER_INFO11_RU_ALLOC_4567_BAND1_3);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_populate_he_data_common
			(struct hal_mon_tx_usr_desc_common *usr_common,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u16 he_data1;
	u16 he_data2;
	u16 he_data5;
	u16 he_data6;
	u16 i = 0;

	he_data1 = IEEE80211_RADIOTAP_HE_DATA1_DOPPLER_KNOWN;
	he_data2 = (IEEE80211_RADIOTAP_HE_DATA2_PE_DISAMBIG_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA2_NUM_LTF_SYMS_KNOWN);
	he_data5 = ((usr_common->pkt_extn_pe << HE_PE_DISAMBIGUITY_SHIFT) |
			(usr_common->a_factor << HE_PRE_FEC_PAD_SHIFT) |
			((1 + usr_common->ltf_size) << HE_LTF_SIZE_SHIFT) |
			(usr_common->num_ltf_symbols << HE_LTF_SYM_SHIFT));
	he_data6 = (usr_common->doppler_indication << HE_DOPPLER_SHIFT);

	for (i = 0; i < usr_common->num_users; i++) {
		ppdu_info->rx_status.userstats[i].he_data1 |= he_data1;
		ppdu_info->rx_status.userstats[i].he_data2 |= he_data2;
		ppdu_info->rx_status.userstats[i].he_data5 |= he_data5;
		ppdu_info->rx_status.userstats[i].he_data6 |= he_data6;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_populate_he_mu_common
			(struct hal_mon_tx_usr_desc_common *usr_common,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u16 he_mu_flag_1 = 0;
	u16 he_mu_flag_2 = 0;
	u16 i = 0;

	he_mu_flag_1 |= (IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH2_CTR_26T_RU_KNOWN |
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU_KNOWN |
			((usr_common->center_ru_0 <<
			HE_CHANNEL_1_CENTER_26_RU_SHIFT) &
			IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU));
	he_mu_flag_2 |= ((usr_common->center_ru_1 <<
			HE_CHANNEL_2_CENTER_26_RU_SHIFT) &
			IEEE80211_RADIOTAP_HE_MU_FLAGS2_CH2_CTR_26T_RU);

	for (i = 0; i < usr_common->num_users; i++) {
		ppdu_info->rx_status.userstats[i].he_flags1 |= he_mu_flag_1;
		ppdu_info->rx_status.userstats[i].he_flags2 |= he_mu_flag_2;
		ppdu_info->rx_status.userstats[i].he_RU[0] =
						usr_common->ru_channel_0[0];
		ppdu_info->rx_status.userstats[i].he_RU[1] =
						usr_common->ru_channel_0[1];
		ppdu_info->rx_status.userstats[i].he_RU[2] =
						usr_common->ru_channel_0[2];
		ppdu_info->rx_status.userstats[i].he_RU[3] =
						usr_common->ru_channel_0[3];
		ppdu_info->rx_status.userstats[i].he_RU[4] =
						usr_common->ru_channel_1[0];
		ppdu_info->rx_status.userstats[i].he_RU[5] =
						usr_common->ru_channel_1[1];
		ppdu_info->rx_status.userstats[i].he_RU[6] =
						usr_common->ru_channel_1[2];
		ppdu_info->rx_status.userstats[i].he_RU[7] =
						usr_common->ru_channel_1[3];
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_populate_eht_sig_common
			(struct hal_mon_tx_usr_desc_common *usr_common,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	u32 eht_known = 0;
	u32 eht_data[4] = {0};
	u8  num_ru_allocation_known = 0;
	u8  i = 0;
	u16 gi, ltf = 0;

	eht_known = (IEEE80211_RADIOTAP_EHT_KNOWN_SPATIAL_REUSE |
			IEEE80211_RADIOTAP_EHT_KNOWN_GI |
			IEEE80211_RADIOTAP_EHT_KNOWN_EHT_LTF |
			EHT_EHT_LTF_KNOWN |
			IEEE80211_RADIOTAP_EHT_KNOWN_PRE_PADD_FACOR_OM |
			IEEE80211_RADIOTAP_EHT_KNOWN_PE_DISAMBIGUITY_OM |
			IEEE80211_RADIOTAP_EHT_KNOWN_DISREGARD_O);
	eht_data[0] |= (usr_common->spatial_reuse << EHT_SPATIAL_REUSE_SHIFT);
	gi = usr_common->gi;
	hal_get_radiotap_gi_ltf(&gi, &ltf);
	eht_data[0] |= (gi << EHT_GI_SHIFT);
	eht_data[0] |= (usr_common->ltf_size << EHT_LTF_SHIFT);
	eht_data[0] |= (usr_common->num_ltf_symbols << EHT_EHT_LTF_SHIFT);
	eht_data[0] |= (usr_common->a_factor << EHT_PRE_FEC_PADDING_FACTOR_SHIFT);
	eht_data[0] |= (usr_common->pkt_extn_pe << EHT_PE_DISAMBIGUITY_SHIFT);
	eht_data[0] |= (EHT_DISREGARD_MASK << EHT_DISREGARD_SHIFT);

	switch (ppdu_info->rx_status.bw) {
	case HAL_EHT_BW_320_2:
	case HAL_EHT_BW_320_1:
		num_ru_allocation_known += 4;
		eht_data[3] |= (usr_common->ru_channel_0[7] <<
						EHT_RU_ALLOCATION2_6_SHIFT);
		eht_data[3] |= (usr_common->ru_channel_0[6] <<
						EHT_RU_ALLOCATION2_5_SHIFT);
		eht_data[3] |= (usr_common->ru_channel_0[5] <<
						EHT_RU_ALLOCATION2_4_SHIFT);
		eht_data[2] |= (usr_common->ru_channel_0[4] <<
						EHT_RU_ALLOCATION2_3_SHIFT);
		fallthrough;
	case HAL_EHT_BW_160:
		num_ru_allocation_known += 2;
		eht_data[2] |= (usr_common->ru_channel_0[3] <<
						EHT_RU_ALLOCATION2_2_SHIFT);
		eht_data[2] |= (usr_common->ru_channel_0[2] <<
						EHT_RU_ALLOCATION2_1_SHIFT);
		fallthrough;
	case HAL_EHT_BW_80:
		num_ru_allocation_known += 1;
		eht_data[1] |= (usr_common->ru_channel_0[1] <<
						EHT_RU_ALLOCATION1_2_SHIFT);
		fallthrough;
	case HAL_EHT_BW_40:
	case HAL_EHT_BW_20:
		num_ru_allocation_known += 1;
		eht_data[1] |= (usr_common->ru_channel_0[0] <<
						EHT_RU_ALLOCATION1_1_SHIFT);
	break;
	default:
	break;
	}

	eht_known |= (num_ru_allocation_known <<
						EHT_NUM_KNOWN_RU_ALLOCATIONS_SHIFT);
	ppdu_info->rx_status.eht_known |= eht_known;
	ppdu_info->rx_status.eht_info.eht.known = ppdu_info->rx_status.eht_known;

	for (i = 0; i < 4; i++) {
		ppdu_info->rx_status.eht_data[i] |= eht_data[i];
		ppdu_info->rx_status.eht_info.eht.data[i] =
						ppdu_info->rx_status.eht_data[i];
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_populate_uhr_sig_common
			(struct hal_mon_tx_usr_desc_common *usr_common,
			 struct hal_tx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->rx_status.uhr_info;
	struct hal_rx_radiotap_uhr *uhr = &uhr_info->uhr;
	u32 known, data0, data1, data2, data3, data4, data5, data6, data7;
	bool is_non_ofdma;
	u16 gi, ltf = 0;

	known = __le32_to_cpu(uhr->known);
	data0 = __le32_to_cpu(uhr->data[0]);
	data1 = __le32_to_cpu(uhr->data[1]);
	data2 = __le32_to_cpu(uhr->data[2]);
	data3 = __le32_to_cpu(uhr->data[3]);
	data4 = __le32_to_cpu(uhr->data[4]);
	data5 = __le32_to_cpu(uhr->data[5]);
	data6 = __le32_to_cpu(uhr->data[6]);
	data7 = __le32_to_cpu(uhr->data[7]);

	known |= IEEE80211_RADIOTAP_UHR_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_GI_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_UHR_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRE_PAD_FACTOR |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PE_DISAMBIGUITY;

	gi = usr_common->gi;
	hal_get_radiotap_gi_ltf(&gi, &ltf);

	data0 |= u32_encode_bits(usr_common->spatial_reuse,
				 IEEE80211_RADIOTAP_UHR_DATA0_SPATIAL_REUSE) |
		 u32_encode_bits(gi,
				 IEEE80211_RADIOTAP_UHR_DATA0_GI) |
		 u32_encode_bits(usr_common->num_ltf_symbols,
				 IEEE80211_RADIOTAP_UHR_DATA0_UHR_LTF) |
		 u32_encode_bits(usr_common->a_factor,
				 IEEE80211_RADIOTAP_UHR_DATA0_PRE_PAD_FACTOR_OM) |
		 u32_encode_bits(usr_common->pkt_extn_pe,
				 IEEE80211_RADIOTAP_UHR_DATA0_PE_DISAMBIGUITY_OM);

	is_non_ofdma = (!ppdu_info->su_or_mu || !ppdu_info->mu_type);
	if (is_non_ofdma) {
		known |= IEEE80211_RADIOTAP_UHR_KNOWN_LDPC_EXTRA_SYM |
			 IEEE80211_RADIOTAP_UHR_KNOWN_CRC1 |
			 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL1 |
			 IEEE80211_RADIOTAP_UHR_KNOWN_INTF_MITG_NO |
			 IEEE80211_RADIOTAP_UHR_KNOWN_DISREGARD_NO |
			 IEEE80211_RADIOTAP_UHR_KNOWN_NR_NON_OFDMA_USERS_NO |
			 IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_CRC_NO |
			 IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_TAIL_NO;
		data7 |=
		u32_encode_bits(usr_common->num_users,
				IEEE80211_RADIOTAP_UHR_DATA7_NUM_OF_NON_OFDMA_USERS);
	} else {
		known |= IEEE80211_RADIOTAP_UHR_KNOWN_RU_ALLOC_TB_FMT_O;
		data1 |= IEEE80211_RADIOTAP_UHR_DATA1_RU_ALLOC_CC_1_1_1_KNOWN |
			 u32_encode_bits(usr_common->ru_channel_0[0],
					 IEEE80211_RADIOTAP_UHR_DATA1_RU_ALLOC_CC_1_1_1);
		switch (ppdu_info->rx_status.bw) {
		case HAL_EHT_BW_320_2:
		case HAL_EHT_BW_320_1:
			data4 |= IEEE80211_RADIOTAP_UHR_DATA4_RU_ALLOC_CC_1_2_3_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[4],
					IEEE80211_RADIOTAP_UHR_DATA4_RU_ALLOC_CC_1_2_3);
			data5 |= IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_4_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[5],
					IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_4) |
			IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_5_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[6],
					IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_5);
			data6 |= IEEE80211_RADIOTAP_UHR_DATA6_RU_ALLOC_CC_1_2_6_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[7],
					IEEE80211_RADIOTAP_UHR_DATA6_RU_ALLOC_CC_1_2_6);
			fallthrough;
		case HAL_EHT_BW_160:
			data3 |= IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_1_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[2],
					IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_1) |
			IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_2_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[3],
					IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_2);
			fallthrough;
		case HAL_EHT_BW_80:
			data2 |= IEEE80211_RADIOTAP_UHR_DATA2_RU_ALLOC_CC_1_1_2_KNOWN |
			u32_encode_bits(usr_common->ru_channel_0[1],
					IEEE80211_RADIOTAP_UHR_DATA2_RU_ALLOC_CC_1_1_2);
			fallthrough;
		case HAL_EHT_BW_40:
		case HAL_EHT_BW_20:
		default:
			break;
		}
	}

	put_unaligned_le32(known, &uhr->known);
	put_unaligned_le32(data0, &uhr->data[0]);
	put_unaligned_le32(data1, &uhr->data[1]);
	put_unaligned_le32(data2, &uhr->data[2]);
	put_unaligned_le32(data3, &uhr->data[3]);
	put_unaligned_le32(data4, &uhr->data[4]);
	put_unaligned_le32(data5, &uhr->data[5]);
	put_unaligned_le32(data6, &uhr->data[6]);
	put_unaligned_le32(data7, &uhr->data[7]);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_elr_sig_uhr(const void *tlv_data,
					  struct hal_tx_mon_ppdu_info *ppdu_info)
{
	const struct hal_tx_mon_elr_sig_uhr *elr = tlv_data;
	struct hal_rx_uhr_elr_info *elr_info = &ppdu_info->rx_status.elr_info;
	u32 known, sig1, sig2;
	bool crc_checked;

	ppdu_info->rx_status.is_uhr = true;
	ppdu_info->rx_status.is_uhr_elr = true;

	known = __le32_to_cpu(elr_info->known);
	known |= IEEE80211_RADIOTAP_UHR_ELR_ELR_VERSION_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_UL_DL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_MCS_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_CODING_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_LENGTH_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_LDPC_EXTRA_SYM_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_TAIL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_STA_ID_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_TAIL_KNOWN;

	crc_checked = !!u32_get_bits(__le32_to_cpu(elr->info1),
				     HAL_TX_MON_ELR_SIG_UHR_INFO1_RX_INTEG_CHECK);
	if (crc_checked)
		known |= IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC_CHECKED |
			 IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC_CHECKED;

	elr_info->known = cpu_to_le32(known);

	sig1 = __le32_to_cpu(elr_info->sig1);
	sig1 |= ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_ELR_VERSION,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_ELR_VERSION) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_DL_UL_FLAG,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_UL_DL) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_STA_MCS,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_MCS) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_STA_CODING,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_CODING) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_NUM_DATA_SYMBOLS,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_LENGTH) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_LDPC_EXTRA_SYMBOL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_LDPC_EXTRA_SYM) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_CRC_0,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC) |
		ATH12K_LE32_DEC_ENC(elr->info0,
				    HAL_TX_MON_ELR_SIG_UHR_INFO0_TAIL_0,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_TAIL);
	elr_info->sig1 = cpu_to_le32(sig1);

	sig2 = __le32_to_cpu(elr_info->sig2);
	sig2 |= ATH12K_LE32_DEC_ENC(elr->info1,
				    HAL_TX_MON_ELR_SIG_UHR_INFO1_STA_ID,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_STA_ID) |
		ATH12K_LE32_DEC_ENC(elr->info1,
				    HAL_TX_MON_ELR_SIG_UHR_INFO1_CRC_1,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC) |
		ATH12K_LE32_DEC_ENC(elr->info1,
				    HAL_TX_MON_ELR_SIG_UHR_INFO1_TAIL_1,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_TAIL);
	elr_info->sig2 = cpu_to_le32(sig2);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_user_desc_common(const void *tlv_data,
					       struct hal_tx_mon_ppdu_info *ppdu_info,
					       u16 tlv_len)
{
	struct hal_mon_tx_usr_desc_common usr_common = {0};
	u8 su_or_mu = ppdu_info->su_or_mu;
	u8 mu_type = ppdu_info->mu_type;
	u8 num_users = ppdu_info->num_users;

	usr_common.num_users = num_users;
	ath12k_wifi8_hal_mon_tx_get_user_desc_common(tlv_data, &usr_common, tlv_len);

	switch (ppdu_info->rx_status.preamble_type) {
	case HAL_RX_PREAMBLE_11AX:
		ppdu_info->rx_status.he_mu_flags =
					(!!(HE_MU_NUM_USER_MASK & usr_common.num_users));
		if (ppdu_info->rx_status.he_flags)
			ath12k_wifi8_hal_mon_tx_populate_he_data_common(&usr_common,
									ppdu_info);
		if (ppdu_info->rx_status.he_mu_flags)
			ath12k_wifi8_hal_mon_tx_populate_he_mu_common(&usr_common,
								      ppdu_info);
	break;
	case HAL_RX_PREAMBLE_11BE:
		ath12k_wifi8_hal_mon_tx_populate_eht_sig_common(&usr_common,
								ppdu_info);
		if (ppdu_info->rx_status.is_uhr && !ppdu_info->rx_status.is_uhr_elr)
			ath12k_wifi8_hal_mon_tx_populate_uhr_sig_common(&usr_common,
									ppdu_info);
		if (su_or_mu && !mu_type) {
			ppdu_info->rx_status.eht_known |=
				IEEE80211_RADIOTAP_EHT_KNOWN_NR_NON_OFDMA_USERS_M;
			ppdu_info->rx_status.eht_data[7] |=
				num_users << EHT_NUM_NON_OFDMA_USERS_SHIFT;
			ppdu_info->rx_status.eht_info.eht.known =
						ppdu_info->rx_status.eht_known;
			ppdu_info->rx_status.eht_info.eht.data[7] =
							ppdu_info->rx_status.eht_data[7];
		}
	break;
	case HAL_RX_PREAMBLE_11BN:
		ath12k_wifi8_hal_mon_tx_populate_uhr_sig_common(&usr_common,
								ppdu_info);
	break;
	}
}

/* To DO: move to common hal */
static __always_inline void
ath12k_hal_mon_tx_parse_mon_buf_addr(const void *tlv_data, void *packet_info)
{
	struct hal_tx_mon_buf_addr *mon_buf_addr =
		(struct hal_tx_mon_buf_addr *)tlv_data;
	struct hal_tx_mon_packet_info *pkt_info =
		(struct hal_tx_mon_packet_info *)packet_info;
	u32 info[3];

	info[0] = __le32_to_cpu(mon_buf_addr->info0);
	info[1] = __le32_to_cpu(mon_buf_addr->info1);
	info[2] = __le32_to_cpu(mon_buf_addr->info2);

	pkt_info->sw_cookie = (u64)info[0] | ((u64)info[1] << 32);
	pkt_info->dma_length =
			u32_get_bits(info[2],
				     HAL_TX_MON_BUF_ADDR_INFO2_DMA_LENGTH) + 1;
	pkt_info->msdu_continuation =
			u32_get_bits(info[2],
				     HAL_TX_MON_BUF_ADDR_INFO2_MSDU_CONTINUATION);
	pkt_info->truncated =
			u32_get_bits(info[2],
				     HAL_TX_MON_BUF_ADDR_INFO2_TRUNCATED);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_fw2sw(const void *tlv_data, u32 userid,
				    struct hal_tx_mon_status_info *status_info)
{
	switch (userid) {
	case HAL_MON_TX_FW2SW_TYPE_FES_SETUP:
	{
		struct hal_tx_mon_fw2sw *fw2sw =
			(struct hal_tx_mon_fw2sw *)tlv_data;
		u32 info[4];
		u8  pkt_id = 0, is_valid = 0;

		info[0] = __le32_to_cpu(fw2sw->info0);
		info[1] = __le32_to_cpu(fw2sw->info1);
		info[2] = __le32_to_cpu(fw2sw->info2);
		info[3] = __le32_to_cpu(fw2sw->info3);

		pkt_id = u32_get_bits(info[3],
				      HAL_TX_MON_FW2SW_INFO3_PACKET_ID);
		is_valid = u32_get_bits(info[3],
					HAL_TX_MON_FW2SW_INFO3_COOKIE_VALID);

		status_info->band_center_freq1 =
			u32_get_bits(info[0],
				     HAL_TX_MON_FW2SW_INFO0_BAND_CENTER_FREQ1);
		status_info->band_center_freq2 =
			u32_get_bits(info[0],
				     HAL_TX_MON_FW2SW_INFO0_BAND_CENTER_FREQ2);
		status_info->freq =
			u32_get_bits(info[1],
				     HAL_TX_MON_FW2SW_INFO1_FREQUENCY);
		status_info->phy_mode =
			u32_get_bits(info[1],
				     HAL_TX_MON_FW2SW_INFO1_PHY_MODE);
		status_info->schedule_id =
			u32_get_bits(info[2],
				     HAL_TX_MON_FW2SW_INFO2_SCHEDULE_ID);

		if (is_valid)
			status_info->dp_tx_pkt_cap_cookie++;

		break;
	}

	case HAL_MON_TX_FW2SW_TYPE_FES_SETUP_USER:
	break;

	case HAL_MON_TX_FW2SW_TYPE_FES_SETUP_EXT:
	break;
	};
}

void ath12k_wifi8_hal_mon_set_mon_buf_desc(void *desc, u32 addr_lo,
					   u32 addr_hi, u64 cookie)
{
	struct hal_mon_buf_ring *mon_buf_desc = (struct hal_mon_buf_ring *)desc;

	mon_buf_desc->paddr_lo = addr_lo;
	mon_buf_desc->paddr_hi = addr_hi;
	mon_buf_desc->cookie = cookie;
}

bool ath12k_wifi8_is_mon_buf_addr_tlv(u32 tlv_tag)
{
	return (tlv_tag == HAL_MON_BUF_ADDR);
}

bool ath12k_wifi8_tx_mon_pkt_buf_cnt_in_desc(void)
{
	return true;
}

enum hal_tx_mon_status
ath12k_wifi8_hal_mon_tx_status_get_num_user(struct ath12k_hal *hal,
					    u16 tlv_tag,
					    const void *tx_tlv,
					    u8 *num_users,
					    u16 tlv_len)
{
	u32 tlv_status = HAL_TX_MON_STATUS_PPDU_NOT_DONE;
	u32 info;

	switch (tlv_tag) {
	case HAL_TX_FES_SETUP: {
		if (likely(tlv_len < HAL_MON_TX_FES_SETUP_TLV_SIZE)) {
			struct hal_tx_mon_fes_setup_compact *tx_fes_setup =
				(struct hal_tx_mon_fes_setup_compact *)tx_tlv;
			info = __le32_to_cpu(tx_fes_setup->info0);
			*num_users =
			u32_get_bits(info,
				     HAL_TX_MON_FES_SETUP_INFO0_NUM_OF_USERS_CMPCT);
		} else {
			struct hal_tx_mon_fes_setup *tx_fes_setup =
				(struct hal_tx_mon_fes_setup *)tx_tlv;
			info = __le32_to_cpu(tx_fes_setup->info0);
			*num_users =
			u32_get_bits(info,
				     HAL_TX_MON_FES_SETUP_INFO0_NUM_OF_USERS);
		}
		tlv_status = HAL_TX_MON_FES_SETUP;
		break;
	}

	case HAL_RX_RESPONSE_REQUIRED_INFO: {
		if (likely(tlv_len < HAL_MON_TX_RX_RESP_REQ_INFO_TLV_SIZE)) {
			const struct hal_tx_mon_rx_resp_req_info_compact *rx_resp_req =
				(const struct hal_tx_mon_rx_resp_req_info_compact *)
				tx_tlv;

			info = __le32_to_cpu(rx_resp_req->info1);
		} else {
			const struct hal_tx_mon_rx_resp_req_info *rx_resp_req =
				(const struct hal_tx_mon_rx_resp_req_info *)tx_tlv;

			info = __le32_to_cpu(rx_resp_req->info1);
		}
		*num_users =
		u32_get_bits(info,
			     HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO1_RESPONSE_STA_COUNT);
		tlv_status = HAL_TX_MON_RESPONSE_REQUIRED_INFO;
		break;
	}
	}

	if (*num_users == 0)
		*num_users = 1;

	return tlv_status;
}

struct dp_mon_tx_ppdu_info *
ath12k_wifi8_hal_mon_tx_ppdu_info(struct ath12k_hal *hal,
				  struct ath12k_mon_data *pmon,
				  u16 tlv_tag)
{
	switch (tlv_tag) {
	case HAL_TX_FES_SETUP:
	case HAL_TX_FLUSH:
	case HAL_PCU_PPDU_SETUP_INIT:
	case HAL_TX_PEER_ENTRY:
	case HAL_TX_QUEUE_EXTENSION:
	case HAL_TX_MPDU_START:
	case HAL_TX_MSDU_START:
	case HAL_TX_DATA:
	case HAL_MON_BUF_ADDR:
	case HAL_TX_MPDU_END:
	case HAL_TX_MSDU_END:
	case HAL_TX_LAST_MPDU_FETCHED:
	case HAL_TX_LAST_MPDU_END:
	case HAL_COEX_TX_REQ:
	case HAL_TX_RAW_OR_NATIVE_FRAME_SETUP:
	case HAL_NDP_PREAMBLE_DONE:
	case HAL_SCH_CRITICAL_TLV_REFERENCE:
	case HAL_TX_FES_SETUP_COMPLETE:
	case HAL_TQM_MPDU_GLOBAL_START:
	case HAL_SCHEDULER_END:
	case HAL_TX_FES_STATUS_USER_PPDU:
	case HAL_TX_FES_STATUS_START_PPDU:
		break;
	case HAL_TX_FES_STATUS_PROT: {
		if (!pmon->prot_ppdu_info.is_used)
			pmon->prot_ppdu_info.is_used = true;
		/* Mark end of protection window so subsequent TLVs
		 * (L-SIG, PHY_DESC for the data frame) go to data_ppdu_info.
		 */
		pmon->prot_ppdu_info.tx_info.prot_tlv_status = tlv_tag;

		return &pmon->prot_ppdu_info;
	}
	case HAL_TX_FES_STATUS_START_PROT: {
		if (!pmon->prot_ppdu_info.is_used)
			pmon->prot_ppdu_info.is_used = true;
		/* Mark start of protection window so subsequent TLVs
		 * (L-SIG A/B, PHY_DESC for the protection frame) are routed
		 * to prot_ppdu_info and carry the correct rate/preamble info.
		 */
		pmon->prot_ppdu_info.tx_info.prot_tlv_status = tlv_tag;

		return &pmon->prot_ppdu_info;
	}
	default:
		if (pmon->prot_ppdu_info.tx_info.prot_tlv_status ==
		    HAL_TX_FES_STATUS_START_PROT) {
			return &pmon->prot_ppdu_info;
		}
	}

	return &pmon->data_ppdu_info;
}

void
ath12k_wifi8_hal_tx_mon_get_wmask_config(struct hal_tx_mon_wmask_config *wmsk)
{
	wmsk->compaction_enable = HAL_TX_MON_WMASK_COMPACT_EN_CFG;
	wmsk->tx_fes_setup = HAL_TX_MON_WMASK_FES_SETUP_CFG;
	wmsk->tx_peer_entry = HAL_TX_MON_WMASK_PEER_ENTRY_CFG;
	wmsk->tx_queue_ext = HAL_TX_MON_WMASK_QUEUE_EXT_CFG;
	wmsk->tx_msdu_start = HAL_TX_MON_WMASK_MSDU_START_CFG;
	wmsk->pcu_ppdu_setup_init = HAL_TX_MON_WMASK_PCU_PPDU_SETUP_INIT_CFG;
	wmsk->tx_mpdu_start = HAL_TX_MON_WMASK_MPDU_START_CFG;
	wmsk->rxpcu_user_setup = HAL_TX_MON_WMASK_RXPCU_USER_SETUP_CFG;
	wmsk->tx_fes_status_end = HAL_TX_MON_WMASK_FES_STATUS_END_CFG;
	wmsk->response_end_status = HAL_TX_MON_WMASK_RESPONSE_END_STATUS_CFG;
	wmsk->tx_fes_status_prot = HAL_TX_MON_WMASK_FES_STATUS_PROT_CFG;
	wmsk->mactx_user_desc_common = HAL_TX_MON_WMASK_USER_DESC_COMMON_CFG;
	wmsk->rx_resp_required_info = HAL_TX_MON_WMASK_RX_RESP_REQUIRED_INFO_CFG;
}

static __always_inline void
ath12k_wifi8_hal_tx_mon_rx_resp_req_info_compact(
	u32 *info, const struct hal_tx_mon_rx_resp_req_info_compact *r)
{
	info[0] = __le32_to_cpu(r->info0);
	info[1] = __le32_to_cpu(r->info1);
	info[2] = __le32_to_cpu(r->info2);
	info[3] = __le32_to_cpu(r->info3);
	info[4] = __le32_to_cpu(r->info4);
}

static __always_inline void
ath12k_wifi8_hal_tx_mon_rx_resp_req_info(u32 *info,
					 const struct hal_tx_mon_rx_resp_req_info *r)
{
	info[0] = __le32_to_cpu(r->info0);
	info[1] = __le32_to_cpu(r->info1);
	info[2] = __le32_to_cpu(r->info2);
	info[3] = __le32_to_cpu(r->info3);
	info[4] = __le32_to_cpu(r->info4);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_rx_resp_req_info
				(const void *tlv_data,
				 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
				 struct hal_tx_mon_status_info *status_info,
				 u16 tlv_len)
{
	u32 info[5];
	u32 ppdu_id;
	u32 addr_32;
	u16 addr_16;
	u8 reception_type;

	if (likely(tlv_len < HAL_MON_TX_RX_RESP_REQ_INFO_TLV_SIZE))
		ath12k_wifi8_hal_tx_mon_rx_resp_req_info_compact(info, tlv_data);
	else
		ath12k_wifi8_hal_tx_mon_rx_resp_req_info(info, tlv_data);

	ppdu_id = u32_get_bits(info[0],
			       HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO0_PHY_PPDU_ID);
	reception_type =
		u32_get_bits(info[0],
			     HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO0_RECEPTION_TYPE);
	addr_32 = u32_get_bits(info[2],
			       HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO2_ADDR1_31_0);
	addr_16 = u32_get_bits(info[3],
			       HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO3_ADDR1_47_32);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16, status_info->addr1, true);

	addr_16 = u32_get_bits(info[3],
			       HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO3_ADDR2_15_0);
	addr_32 = u32_get_bits(info[4],
			       HAL_TX_MON_RX_RESPONSE_REQUIRED_INFO4_ADDR2_47_16);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16, status_info->addr2, false);

	tx_ppdu_info->ppdu_id = ppdu_id;
	tx_ppdu_info->rx_status.ppdu_id = ppdu_id;
	status_info->transmission_type = reception_type ?
					 HAL_RX_RECEPTION_TYPE_MU_MIMO :
					 HAL_RX_RECEPTION_TYPE_SU;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_fes_status_start
				(const void *tlv_data,
				 struct hal_tx_mon_status_info *prot_status_info)
{
	const struct hal_tx_mon_fes_status_start *tx_fes_start = tlv_data;
	u32 info0 = __le32_to_cpu(tx_fes_start->info0);

	prot_status_info->medium_prot_type =
		u32_get_bits(info0, HAL_TX_MON_FES_START_INFO0_MEDIUM_PROT_TYPE);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_fes_status_start_ppdu
					(const void *tlv_data,
					 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					 struct hal_tx_mon_status_info *status_info)
{
	const struct hal_tx_mon_fes_status_start_ppdu *fes_start_ppdu = tlv_data;
	u32 info0 = __le32_to_cpu(fes_start_ppdu->info0);
	u32 info1 = __le32_to_cpu(fes_start_ppdu->info1);
	u32 info2 = __le32_to_cpu(fes_start_ppdu->info2);

	tx_ppdu_info->rx_status.tsft = (u64)info0 | ((u64)info1 << 32);
	status_info->ndp_frame =
		u32_get_bits(info2, HAL_TX_MON_FES_STAT_START_PPDU_INFO2_NDP_FRAME);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_fes_status_start_prot
					(const void *tlv_data,
					 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					 struct hal_tx_mon_status_info *status_info,
					 u16 tlv_tag)
{
	const struct hal_tx_mon_fes_status_start_prot *fes_start_prot = tlv_data;
	u32 info0 = __le32_to_cpu(fes_start_prot->info0);
	u32 info1 = __le32_to_cpu(fes_start_prot->info1);
	u32 info2 = __le32_to_cpu(fes_start_prot->info2);

	tx_ppdu_info->rx_status.tsft = (u64)info0 | ((u64)info1 << 32);
	tx_ppdu_info->prot_tlv_status = tlv_tag;
	status_info->response_type =
		u32_get_bits(info2,
			     HAL_TX_MON_FES_STAT_START_PROT_INFO2_RESPONSE_TYPE);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_fes_status_user_ppdu
				(const void *tlv_data,
				 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
				 u32 userid)
{
	const struct hal_tx_mon_fes_status_user_ppdu *tx_fes_user_ppdu = tlv_data;
	struct hal_rx_user_status *usr = &tx_ppdu_info->rx_status.userstats[userid];
	u32 info0 = __le32_to_cpu(tx_fes_user_ppdu->info0);
	u32 info1 = __le32_to_cpu(tx_fes_user_ppdu->info1);

	tx_ppdu_info->cur_usr_idx = userid;
	usr->tid = u32_get_bits(info0, HAL_TX_MON_FES_STAT_USER_INFO0_PPDU_TID);
	usr->duration = u32_get_bits(info1,
				     HAL_TX_MON_FES_STAT_USER_INFO1_PPDU_DURATION);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_l_sig(const void *tlv_data,
				    struct hal_tx_mon_ppdu_info *tx_ppdu_info,
				    bool is_sig_b)
{
	u32 l_sig_info = *((u32 *)tlv_data);

	if (is_sig_b) {
		ath12k_wifi8_hal_mon_tx_parse_l_sig_b(tlv_data, tx_ppdu_info);
		tx_ppdu_info->rx_status.l_sig_b_info = l_sig_info;
		return;
	}

	ath12k_wifi8_hal_mon_tx_parse_l_sig_a(tlv_data, tx_ppdu_info);
	tx_ppdu_info->rx_status.l_sig_a_info = l_sig_info;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_rx_frame_bitmap_ack
					(const void *tlv_data,
					 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					 struct hal_tx_mon_status_info *status_info,
					 u32 userid,
					 bool is_1k_bitmap)
{
	u32 info[5];
	u32 addr_32;
	u16 addr_16;
	u16 ba_bitmap_sz;
	u64 *ba_bitmap_tmp;
	u8 offset;
	u8 ba_user_idx;
	struct hal_rx_user_status *usr;

	if (!is_1k_bitmap) {
		const struct hal_tx_mon_rx_frame_bitmap_ack *fbm_ack = tlv_data;

		info[0] = __le32_to_cpu(fbm_ack->info0);
		info[1] = __le32_to_cpu(fbm_ack->info1);
		info[2] = __le32_to_cpu(fbm_ack->info2);
		info[3] = __le32_to_cpu(fbm_ack->info3);
		info[4] = __le32_to_cpu(fbm_ack->info4);
	} else {
		const struct hal_tx_mon_rx_frame_1k_bitmap_ack *fbm_1k_ack = tlv_data;

		info[0] = __le32_to_cpu(fbm_1k_ack->info0);
		info[1] = __le32_to_cpu(fbm_1k_ack->info1);
		info[2] = __le32_to_cpu(fbm_1k_ack->info2);
		info[3] = __le32_to_cpu(fbm_1k_ack->info3);
		info[4] = __le32_to_cpu(fbm_1k_ack->info4);
	}

	ba_user_idx = ++tx_ppdu_info->ba_user_id;
	ba_user_idx = ba_user_idx >= tx_ppdu_info->num_users ? 0 : ba_user_idx;
	if (!is_1k_bitmap && ba_user_idx >= HAL_MAX_UL_MU_USERS)
		ba_user_idx = 0;

	tx_ppdu_info->cur_usr_idx = userid;
	usr = &tx_ppdu_info->rx_status.userstats[ba_user_idx];

	if (!is_1k_bitmap) {
		status_info->no_bitmap_avail =
			u32_get_bits(info[0],
				     HAL_TX_MON_RX_FBM_ACK_INFO0_NO_BMP_AVAILABLE);
		status_info->explicit_ack =
			u32_get_bits(info[0],
				     HAL_TX_MON_RX_FBM_ACK_INFO0_EXPLICIT_ACK);
		status_info->explicit_ack_type =
			u32_get_bits(info[0],
				     HAL_TX_MON_RX_FBM_ACK_INFO0_EXPLICIT_ACK_TYPE);
		usr->tid = u32_get_bits(info[0],
					HAL_TX_MON_RX_FBM_ACK_INFO0_BA_TID);
		usr->aid = u32_get_bits(info[0],
					HAL_TX_MON_RX_FBM_ACK_INFO0_STA_FULL_AID);
		usr->ba_bitmap_sz = u32_get_bits(info[0],
						 HAL_TX_MON_RX_FBM_ACK_INFO0_BA_BMP_SIZE);
		addr_32 = u32_get_bits(info[1],
				       HAL_TX_MON_RX_FBM_ACK_INFO1_ADDR1_31_0);
		addr_16 = u32_get_bits(info[2],
				       HAL_TX_MON_RX_FBM_ACK_INFO2_ADDR1_47_32);
		ath12k_hal_mon_get_mac_addr(addr_32, addr_16, status_info->addr2, true);

		addr_16 = u32_get_bits(info[2],
				       HAL_TX_MON_RX_FBM_ACK_INFO2_ADDR2_15_0);
		addr_32 = u32_get_bits(info[3],
				       HAL_TX_MON_RX_FBM_ACK_INFO3_ADDR2_47_16);
		ath12k_hal_mon_get_mac_addr(addr_32, addr_16, status_info->addr1, false);

		usr->start_seq = u32_get_bits(info[4],
					      HAL_TX_MON_RX_FBM_ACK_INFO4_BA_TS_SEQ);
		usr->ba_control = u32_get_bits(info[4],
					       HAL_TX_MON_RX_FBM_ACK_INFO4_BA_TS_CTRL);

		ba_bitmap_sz = usr->ba_bitmap_sz;
		if (ba_bitmap_sz > BA_TS_BITMAP_SZ)
			ba_bitmap_sz = BA_TS_BITMAP_SZ;
		offset = BA_TS_LSB >> BA_TS_OFFSET;
		ba_bitmap_tmp = (u64 *)((u8 *)tlv_data + BA_TS_BITMAP + offset);
		memcpy(usr->ba_bitmap, ba_bitmap_tmp, BA_TS_BITMAP_SZ << ba_bitmap_sz);
		return;
	}

	usr->tid = u32_get_bits(info[0],
				HAL_TX_MON_RX_1K_FBM_ACK_INFO0_BA_TID);
	usr->aid = u32_get_bits(info[0],
				HAL_TX_MON_RX_1K_FBM_ACK_INFO0_STA_FULL_AID);
	usr->ba_bitmap_sz =
		u32_get_bits(info[0],
			     HAL_TX_MON_RX_1K_FBM_ACK_INFO0_BA_BMP_SIZE) + 4;
	addr_32 = u32_get_bits(info[1],
			       HAL_TX_MON_RX_1K_FBM_ACK_INFO1_ADDR1_31_0);
	addr_16 = u32_get_bits(info[2],
			       HAL_TX_MON_RX_1K_FBM_ACK_INFO2_ADDR1_47_32);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16, status_info->addr1, true);
	addr_16 = u32_get_bits(info[2],
			       HAL_TX_MON_RX_1K_FBM_ACK_INFO2_ADDR2_15_0);
	addr_32 = u32_get_bits(info[3],
			       HAL_TX_MON_RX_1K_FBM_ACK_INFO3_ADDR2_47_16);
	ath12k_hal_mon_get_mac_addr(addr_32, addr_16, status_info->addr2, false);

	usr->start_seq = u32_get_bits(info[4],
				      HAL_TX_MON_RX_1K_FBM_ACK_INFO4_BA_TS_SEQ);
	usr->ba_control = u32_get_bits(info[4],
				       HAL_TX_MON_RX_1K_FBM_ACK_INFO4_BA_TS_CTRL);
	ba_bitmap_sz = usr->ba_bitmap_sz;
	offset = 32 >> 3;
	ba_bitmap_tmp = (u64 *)((u8 *)tlv_data + 0x10 + offset);
	memcpy(usr->ba_bitmap, ba_bitmap_tmp, BA_TS_BITMAP_SZ << ba_bitmap_sz);
}

static __always_inline bool
ath12k_wifi8_hal_mon_tx_parse_mactx_phy_desc(const void *tlv_data,
					     struct hal_tx_mon_ppdu_info *tx_ppdu_info)
{
	const struct hal_tx_mon_phy_desc *phy_desc = tlv_data;
	u32 info[4];
	u32 he_data1 = 0, he_data2 = 0, he_data3 = 0, he_data5 = 0;
	u32 pkt_type;
	u16 gi;
	u16 ltf_size;
	u16 num_users = tx_ppdu_info->num_users;
	u8 bandwidth;
	u8 is_stbc;
	u8 he_ppdu_subtype;
	u8 i;
	bool is_triggered;

	info[0] = __le32_to_cpu(phy_desc->info0);
	info[1] = __le32_to_cpu(phy_desc->info1);
	info[2] = __le32_to_cpu(phy_desc->info2);
	info[3] = __le32_to_cpu(phy_desc->info3);

	tx_ppdu_info->su_or_mu =
			u32_get_bits(info[0],
				     HAL_TX_MON_PHY_DESC_INFO0_SU_OR_MU);
	if (tx_ppdu_info->su_or_mu)
		tx_ppdu_info->mu_type =
			u32_get_bits(info[0],
				     HAL_TX_MON_PHY_DESC_INFO0_MU_TYPE);

	pkt_type = u32_get_bits(info[0],
				HAL_TX_MON_PHY_DESC_INFO0_PKT_TYPE);
	is_stbc = u32_get_bits(info[1],
			       HAL_TX_MON_PHY_DESC_INFO1_STBC);
	is_triggered = u32_get_bits(info[2], HAL_TX_MON_PHY_DESC_INFO2_TRIGERRED);
	bandwidth = !is_triggered ?
		    u32_get_bits(info[0],
				 HAL_TX_MON_PHY_DESC_INFO0_BANDWIDTH) :
		    u32_get_bits(info[2],
				 HAL_TX_MON_PHY_DESC_INFO2_AP_PKT_BW);

	gi = u32_get_bits(info[3],
			  HAL_TX_MON_PHY_DESC_INFO3_CP_SETTING);
	ltf_size = u32_get_bits(info[3],
				HAL_TX_MON_PHY_DESC_INFO3_LTF_SIZE);
	he_ppdu_subtype = u32_get_bits(info[3],
				       HAL_TX_MON_PHY_DESC_INFO3_HE_PPDU_SUBTYPE);

	tx_ppdu_info->rx_status.preamble_type = pkt_type;
	tx_ppdu_info->rx_status.ltf_size = ltf_size;
	tx_ppdu_info->rx_status.is_stbc = is_stbc;
	tx_ppdu_info->rx_status.bw = bandwidth;

	switch (tx_ppdu_info->rx_status.preamble_type) {
	case HAL_RX_PREAMBLE_11N:
		tx_ppdu_info->rx_status.ht_flags = 1;
		tx_ppdu_info->rx_status.rtap_flags |= HT_SGI_PRESENT;
		break;
	case HAL_RX_PREAMBLE_11AC:
		tx_ppdu_info->rx_status.vht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AX:
		tx_ppdu_info->rx_status.he_flags = 1;
		break;
	default:
		break;
	}

	if (!tx_ppdu_info->rx_status.he_flags)
		return false;

	switch (he_ppdu_subtype) {
	case HE_SUBTYPE_SU:
		tx_ppdu_info->rx_status.he_data1 |= HE_SU_FORMAT_TYPE;
		break;
	case HE_SUBTYPE_TRIG:
		tx_ppdu_info->rx_status.he_data1 |= HE_TRIG_FORMAT_TYPE;
		break;
	case HE_SUBTYPE_MU:
		tx_ppdu_info->rx_status.he_data1 |= HE_MU_FORMAT_TYPE;
		break;
	case HE_SUBTYPE_EXT_SU:
		tx_ppdu_info->rx_status.he_data1 |= HE_EXT_SU_FORMAT_TYPE;
		break;
	}

	he_data1 |= IEEE80211_RADIOTAP_HE_DATA1_STBC_KNOWN |
		    IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN;
	he_data2 |= IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN |
		    IEEE80211_RADIOTAP_HE_DATA2_NUM_LTF_SYMS_KNOWN;
	he_data3 |= is_stbc << HE_STBC_SHIFT;

	hal_get_radiotap_gi_ltf(&gi, &ltf_size);
	he_data5 |= (gi << HE_GI_SHIFT) | ((1 + ltf_size) << HE_LTF_SIZE_SHIFT);
	he_data5 = (he_data5 & HE_DATA5_INFO_MASK) | bandwidth;

	tx_ppdu_info->rx_status.he_mu_flags = !!(HE_MU_NUM_USER_MASK & num_users);
	for (i = 0; i < num_users; i++) {
		struct hal_rx_user_status *usr = &tx_ppdu_info->rx_status.userstats[i];

		usr->he_data1 = he_data1;
		usr->he_data2 = he_data2;
		usr->he_data3 = he_data3;
		usr->he_data5 = he_data5;
	}

	return true;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_fes_status_ack_or_ba
					(const void *tlv_data,
					 struct hal_tx_mon_ppdu_info *tx_ppdu_info)
{
	const struct hal_tx_mon_fes_status_ack_or_ba *ack = tlv_data;
	u32 info0 = __le32_to_cpu(ack->info0);

	if (!u32_get_bits(info0, HAL_TX_MON_FES_STATUS_ACK_OR_BA_INFO0_STAT_TYPE)) {
		tx_ppdu_info->ack_recvd = true;
		tx_ppdu_info->ack_rssi =
			u32_get_bits(info0,
				     HAL_TX_MON_FES_STATUS_ACK_OR_BA_INFO0_ACK_FRM_RSSI);
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_coex_tx_status(const void *tlv_data,
					     struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					     u32 userid)
{
	const struct hal_tx_mon_coex_tx_status *coex_tx = tlv_data;
	u32 info0 = __le32_to_cpu(coex_tx->info0);
	u16 duration =
		u32_get_bits(info0,
			     HAL_TX_MON_COEX_TX_STATUS_INFO0_CURRENT_TX_DURATION);
	u8 status_reason =
		u32_get_bits(info0,
			     HAL_TX_MON_COEX_TX_STATUS_INFO0_TX_STATUS_REASON);

	if (status_reason == COEX_FES_TX_START ||
	    status_reason == COEX_RESPONSE_TX_START)
		tx_ppdu_info->rx_status.userstats[userid].duration = duration;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_usig_commit(struct hal_tx_mon_ppdu_info *tx_ppdu_info)
{
	tx_ppdu_info->rx_status.u_sig_info.usig.common =
						tx_ppdu_info->rx_status.usig_common;
	tx_ppdu_info->rx_status.u_sig_info.usig.value =
						tx_ppdu_info->rx_status.usig_value;
	tx_ppdu_info->rx_status.u_sig_info.usig.mask =
						tx_ppdu_info->rx_status.usig_mask;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_usig_set_common(struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					u32 disregard_mask, u32 ppdu_type_comp_mode,
					u32 crc, u32 tail)
{
	u32 usig_value = tx_ppdu_info->rx_status.usig_value;

	tx_ppdu_info->rx_status.usig_mask |= USIG_DISREGARD_KNOWN |
					     USIG_PPDU_TYPE_N_COMP_MODE_KNOWN |
					     USIG_VALIDATE_KNOWN |
					     USIG_CRC_KNOWN |
					     USIG_TAIL_KNOWN;
	usig_value |= (disregard_mask << USIG_DISREGARD_SHIFT) |
		      (ppdu_type_comp_mode << USIG_PPDU_TYPE_N_COMP_MODE_SHIFT) |
		      (EHT_USIG_VALIDATE_MASK << USIG_VALIDATE_SHIFT) |
		      (crc << USIG_CRC_SHIFT) |
		      (tail << USIG_TAIL_SHIFT);
	tx_ppdu_info->rx_status.usig_value = usig_value;
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_eht_su_mu(const void *tlv_data,
					      struct hal_tx_mon_ppdu_info *tx_ppdu_info)
{
	const struct hal_mon_tx_usig_hdr *usig = tlv_data;
	const struct hal_mon_tx_usig_mu *usig_mu = &usig->usig_2.mu;
	u32 usig_value;

	ath12k_wifi8_hal_mon_tx_parse_u_sig_hdr(tlv_data, tx_ppdu_info);

	tx_ppdu_info->rx_status.usig_mask |= USIG_MU_VALIDATE1_KNOWN |
					     USIG_MU_PUNCTURE_CH_INFO_KNOWN |
					     USIG_MU_VALIDATE2_KNOWN |
					     USIG_MU_EHT_SIG_MCS_KNOWN |
					     USIG_MU_NUM_EHT_SIG_SYM_KNOWN;
	ath12k_wifi8_hal_mon_tx_usig_set_common(tx_ppdu_info,
						EHT_USIG_SU_MU_DISREGARD_MASK,
						usig_mu->ppdu_type_comp_mode,
						usig_mu->crc,
						usig_mu->tail);
	usig_value = tx_ppdu_info->rx_status.usig_value;
	usig_value |= (EHT_USIG_VALIDATE_MASK << USIG_MU_VALIDATE1_SHIFT) |
		      (usig_mu->punc_ch_info << USIG_MU_PUNCTURE_CH_INFO_SHIFT) |
		      (EHT_USIG_VALIDATE_MASK << USIG_MU_VALIDATE2_SHIFT) |
		      (usig_mu->eht_sig_mcs << USIG_MU_EHT_SIG_MCS_SHIFT) |
		      (usig_mu->num_eht_sig_sym << USIG_MU_NUM_EHT_SIG_SYM_SHIFT);
	tx_ppdu_info->rx_status.usig_value = usig_value;

	ath12k_wifi8_hal_mon_tx_usig_commit(tx_ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_u_sig_eht_tb(const void *tlv_data,
					   struct hal_tx_mon_ppdu_info *tx_ppdu_info)
{
	const struct hal_mon_tx_usig_hdr *usig = tlv_data;
	const struct hal_mon_tx_usig_tb *usig_tb = &usig->usig_2.tb;
	u32 usig_value;

	ath12k_wifi8_hal_mon_tx_parse_u_sig_hdr(tlv_data, tx_ppdu_info);

	tx_ppdu_info->rx_status.usig_mask |= USIG_TB_SPATIAL_REUSE_1_KNOWN |
					     USIG_TB_SPATIAL_REUSE_2_KNOWN |
					     USIG_TB_DISREGARD1_KNOWN;
	ath12k_wifi8_hal_mon_tx_usig_set_common(tx_ppdu_info,
						EHT_USIG_TB_DISREGARD_MASK,
						usig_tb->ppdu_type_comp_mode,
						usig_tb->crc,
						usig_tb->tail);
	usig_value = tx_ppdu_info->rx_status.usig_value;
	usig_value |= (usig_tb->spatial_reuse_1 << USIG_TB_SPATIAL_REUSE_1_SHIFT) |
		      (usig_tb->spatial_reuse_2 << USIG_TB_SPATIAL_REUSE_2_SHIFT) |
		      (EHT_USIG_SU_MU_DISREGARD_MASK << USIG_TB_DISREGARD1_SHIFT);
	tx_ppdu_info->rx_status.usig_value = usig_value;

	ath12k_wifi8_hal_mon_tx_usig_commit(tx_ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_tx_parse_ht_sig(const void *tlv_data,
				     struct hal_tx_mon_ppdu_info *tx_ppdu_info)
{
	const struct hal_tx_mon_ht_sig_info *ht_sig_info = tlv_data;
	u32 info0 = __le32_to_cpu(ht_sig_info->info0);
	u32 info1 = __le32_to_cpu(ht_sig_info->info1);
	u8 mcs = u32_get_bits(info0, HAL_TX_MON_HT_SIG_INFO0_MCS);
	u8 bw = u32_get_bits(info0, HAL_TX_MON_HT_SIG_INFO0_CBW);
	u8 is_stbc = u32_get_bits(info1, HAL_TX_MON_HT_SIG_INFO1_STBC);
	u8 coding = u32_get_bits(info1, HAL_TX_MON_HT_SIG_INFO1_FEC_CODING);
	u8 gi = u32_get_bits(info1, HAL_TX_MON_HT_SIG_INFO1_SHORT_GI);

	tx_ppdu_info->rx_status.ldpc = (coding == HAL_RX_SU_MU_CODING_LDPC) ? 1 : 0;
	tx_ppdu_info->rx_status.ht_mcs = mcs;
	tx_ppdu_info->rx_status.bw = bw;
	tx_ppdu_info->rx_status.sgi = gi;
	tx_ppdu_info->rx_status.is_stbc = is_stbc;
	tx_ppdu_info->rx_status.reception_type = HAL_RX_RECEPTION_TYPE_SU;
}

enum hal_tx_mon_status
ath12k_wifi8_hal_mon_tx_parse_status_tlv(struct ath12k_hal *hal,
					 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					 struct hal_tx_mon_status_info *data_status_info,
					 struct hal_tx_mon_status_info *prot_status_info,
					 bool is_prot_ppdu,
					 u16 tlv_tag,
					 const void *tlv_data,
					 u32 userid,
					 u16 tlv_len,
					 u8 *status_frag)
{
	enum hal_tx_mon_status status = HAL_TX_MON_STATUS_PPDU_NOT_DONE;
	struct hal_tx_mon_status_info *status_info = is_prot_ppdu ?
					prot_status_info : data_status_info;

	switch (tlv_tag) {
	case HAL_TX_FES_SETUP: {
		ath12k_hal_mon_tx_fes_setup_info_get(hal, tlv_data,
						     userid, tx_ppdu_info, tlv_len);
		status = HAL_TX_MON_FES_SETUP;
		break;
	}

	case HAL_TX_FES_STATUS_END: {
		ath12k_hal_mon_tx_fes_status_end_info_get(hal, tlv_data,
							  userid, tx_ppdu_info,
							  status_info, tlv_len);
		status = HAL_TX_MON_FES_STATUS_END;
		break;
	}

	case HAL_RX_RESPONSE_REQUIRED_INFO: {
		ath12k_wifi8_hal_mon_tx_parse_rx_resp_req_info(tlv_data,
							       tx_ppdu_info,
							       status_info,
							       tlv_len);
		status = HAL_TX_MON_RESPONSE_REQUIRED_INFO;
		break;
	}

	case HAL_PCU_PPDU_SETUP_INIT: {
		ath12k_hal_mon_tx_pcu_ppdu_setup_init_info_get(hal, tlv_data,
							       prot_status_info,
							       tlv_len);
		status = HAL_TX_MON_PCU_PPDU_SETUP_INIT;
		break;
	}

	case HAL_TX_QUEUE_EXTENSION: {
		ath12k_hal_mon_tx_queue_ext_info_get(hal, tlv_data,
						     userid, tx_ppdu_info, tlv_len);
		status = HAL_TX_MON_QUEUE_EXTENSION;
		break;
	}

	case HAL_TX_PEER_ENTRY: {
		ath12k_hal_mon_tx_peer_entry_info_get(hal, tlv_data,
						      userid, tx_ppdu_info,
						      status_info, tlv_len);
		status = HAL_TX_MON_PEER_ENTRY;
		break;
	}

	case HAL_TX_FES_STATUS_START: {
		ath12k_wifi8_hal_mon_tx_parse_fes_status_start(tlv_data,
							       prot_status_info);
		status = HAL_TX_MON_FES_STATUS_START;
		break;
	}

	case HAL_TX_FES_STATUS_PROT: {
		ath12k_hal_mon_tx_fes_status_prot_info_get(hal, tlv_data,
							   userid, tx_ppdu_info,
							   tlv_len);
		tx_ppdu_info->prot_tlv_status = tlv_tag;
		status = HAL_TX_MON_FES_STATUS_PROT;
		break;
	}

	case HAL_TX_FES_STATUS_START_PPDU: {
		ath12k_wifi8_hal_mon_tx_parse_fes_status_start_ppdu(tlv_data,
								    tx_ppdu_info,
								    status_info);
		status = HAL_TX_MON_FES_STATUS_START_PPDU;
		break;
	}

	case HAL_TX_FES_STATUS_START_PROT: {
		ath12k_wifi8_hal_mon_tx_parse_fes_status_start_prot(tlv_data,
								    tx_ppdu_info,
								    status_info,
								    tlv_tag);
		status = HAL_TX_MON_FES_STATUS_START_PROT;
		break;
	}

	case HAL_TX_FES_STATUS_USER_PPDU: {
		ath12k_wifi8_hal_mon_tx_parse_fes_status_user_ppdu(tlv_data,
								   tx_ppdu_info,
								   userid);
		status = HAL_TX_MON_FES_STATUS_USER_PPDU;
		break;
	}

	case HAL_MACTX_HE_SIG_A_SU: {
		ath12k_wifi8_hal_mon_tx_parse_he_sig_a_su(tlv_data,
							  tx_ppdu_info);
		status = HAL_TX_MON_MACTX_HE_SIG_A_SU;
		break;
	}

	case HAL_MACTX_HE_SIG_A_MU_DL: {
		ath12k_wifi8_hal_mon_tx_parse_he_sig_a_mu_dl(tlv_data,
							     tx_ppdu_info);
		status = HAL_TX_MON_MACTX_HE_SIG_A_MU_DL;
		break;
	}

	case HAL_MACTX_HE_SIG_B1_MU: {
		status = HAL_TX_MON_MACTX_HE_SIG_B1_MU;
		break;
	}

	case HAL_MACTX_HE_SIG_B2_MU: {
		ath12k_wifi8_hal_mon_tx_parse_he_sig_b2_mu(tlv_data,
							   userid,
							   tx_ppdu_info);
		status = HAL_TX_MON_MACTX_HE_SIG_B2_MU;
		break;
	}

	case HAL_MACTX_HE_SIG_B2_OFDMA: {
		ath12k_wifi8_hal_mon_tx_parse_he_sig_b2_ofdma(tlv_data,
							      userid,
							      tx_ppdu_info);
		status = HAL_TX_MON_MACTX_HE_SIG_B2_OFDMA;
		break;
	}

	case HAL_MACTX_VHT_SIG_A: {
		ath12k_wifi8_hal_mon_tx_parse_vht_sig_a(tlv_data,
							userid,
							tx_ppdu_info,
							status_info);
		status = HAL_TX_MON_MACTX_VHT_SIG;
		break;
	}

	case HAL_MACTX_L_SIG_A: {
		ath12k_wifi8_hal_mon_tx_parse_l_sig(tlv_data, tx_ppdu_info, false);
		status = HAL_TX_MON_MACTX_L_SIG_A;
		break;
	}

	case HAL_MACTX_L_SIG_B: {
		ath12k_wifi8_hal_mon_tx_parse_l_sig(tlv_data, tx_ppdu_info, true);
		status = HAL_TX_MON_MACTX_L_SIG_B;
		break;
	}

	case HAL_RX_FRAME_BITMAP_ACK: {
		ath12k_wifi8_hal_mon_tx_parse_rx_frame_bitmap_ack(tlv_data,
								  tx_ppdu_info,
								  status_info,
								  userid,
								  false);
		status = HAL_TX_MON_FRAME_BITMAP_ACK;
		break;
	}

	case HAL_MACTX_PHY_DESC: {
		if (ath12k_wifi8_hal_mon_tx_parse_mactx_phy_desc(tlv_data,
								 tx_ppdu_info))
			status = HAL_TX_MON_MACTX_PHY_DESC;
		break;
	}

	case HAL_RESPONSE_END_STATUS: {
		ath12k_hal_mon_tx_response_end_status_info_get(hal, tlv_data,
							       userid, tx_ppdu_info,
							       status_info, tlv_len);
		status = HAL_TX_MON_RESPONSE_END_STATUS_INFO;
		break;
	}

	case HAL_TX_MPDU_START: {
		ath12k_hal_mon_tx_mpdu_start_info_get(hal, tlv_data,
						      userid, tx_ppdu_info, tlv_len);
		status = HAL_TX_MON_MPDU_START;
		break;
	}

	case HAL_TX_MPDU_END:
		status = HAL_TX_MON_MPDU_END;
		break;

	case HAL_TX_MSDU_START:
		status = HAL_TX_MON_MSDU_START;
		break;

	case HAL_TX_MSDU_END:
		status = HAL_TX_MON_MSDU_END;
		break;

	case HAL_TX_FES_STATUS_ACK_OR_BA: {
		ath12k_wifi8_hal_mon_tx_parse_fes_status_ack_or_ba(tlv_data,
								   tx_ppdu_info);
		status = HAL_TX_MON_FES_STATUS_ACK_OR_BA;
		break;
	}

	case HAL_RX_FRAME_1K_BITMAP_ACK: {
		ath12k_wifi8_hal_mon_tx_parse_rx_frame_bitmap_ack(tlv_data,
								  tx_ppdu_info,
								  status_info,
								  userid,
								  true);
		status = HAL_TX_MON_FRAME_BITMAP_BLOCK_ACK_1K;
		break;
	}

	case HAL_MON_BUF_ADDR: {
		tx_ppdu_info->cur_usr_idx = userid;
		ath12k_hal_mon_tx_parse_mon_buf_addr(tlv_data,
						     &tx_ppdu_info->packet_info);
		status = HAL_TX_MON_BUFFER_ADDR;
		break;
	}

	case HAL_TX_DATA: {
		tx_ppdu_info->cur_usr_idx = userid;
		status_info->buffer = status_frag;
		status_info->offset = (const u8 *)tlv_data - status_frag;
		status_info->length = tlv_len;
		status = HAL_TX_MON_DATA;
		break;
	}

	case HAL_COEX_TX_STATUS: {
		ath12k_wifi8_hal_mon_tx_parse_coex_tx_status(tlv_data,
							     tx_ppdu_info,
							     userid);
		status = HAL_TX_MON_COEX_TX_STATUS;
		break;
	}

	case HAL_MACTX_U_SIG_EHT_SU_MU: {
		ath12k_wifi8_hal_mon_tx_parse_u_sig_eht_su_mu(tlv_data,
							      tx_ppdu_info);
		break;
	}

	case HAL_MACTX_U_SIG_EHT_TB: {
		ath12k_wifi8_hal_mon_tx_parse_u_sig_eht_tb(tlv_data,
							   tx_ppdu_info);
		break;
	}

	case HAL_MACTX_U_SIG_UHR_SU_MU:
		ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_su_mu(tlv_data,
							      tx_ppdu_info);
		break;
	case HAL_MACTX_U_SIG_UHR_COBF:
		ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_cobf(tlv_data,
							     tx_ppdu_info);
		break;
	case HAL_MACTX_U_SIG_UHR_COSR:
		ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_cosr(tlv_data,
							     tx_ppdu_info);
		break;
	case HAL_MACTX_U_SIG_UHR_TB:
		ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_tb(tlv_data,
							   tx_ppdu_info);
		break;
	case HAL_MACTX_U_SIG_UHR_ELR_SU:
		ath12k_wifi8_hal_mon_tx_parse_u_sig_uhr_elr(tlv_data,
							    tx_ppdu_info);
		break;

	case HAL_MACTX_EHT_SIG_USR_OFDMA:
		ath12k_wifi8_hal_mon_tx_parse_eht_sig_non_mumimo_user_info(tlv_data,
									   userid,
									   tx_ppdu_info);
		break;

	case HAL_MACTX_EHT_SIG_USR_MU_MIMO:
		ath12k_wifi8_hal_mon_tx_parse_eht_sig_mumimo_user_info(tlv_data,
								       userid,
								       tx_ppdu_info);
		break;

	case HAL_MACTX_EHT_SIG_USR_SU:
		ath12k_wifi8_hal_mon_tx_parse_eht_sig_non_mumimo_user_info(tlv_data,
									   userid,
									   tx_ppdu_info);
		break;

	case HAL_MACTX_UHR_SIG_USR_OFDMA:
		ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_ofdma(tlv_data,
								userid,
								tx_ppdu_info);
		break;
	case HAL_MACTX_UHR_SIG_USR_COBF:
		ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_cobf(tlv_data,
							       userid,
							       tx_ppdu_info);
		break;
	case HAL_MACTX_UHR_SIG_USR_COSR:
		ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_cosr(tlv_data,
							       userid,
							       tx_ppdu_info);
		break;
	case HAL_MACTX_UHR_SIG_USR_SU:
		ath12k_wifi8_hal_mon_tx_parse_uhr_sig_usr_su(tlv_data,
							     userid,
							     tx_ppdu_info);
		break;
	case HAL_MACTX_UHR_SIG_USR_MU_MIMO:
		ath12k_wifi8_hal_mon_tx_parse_uhr_sig_mumimo_user_info(tlv_data,
								       userid,
								       tx_ppdu_info);
		break;

	case HAL_MACTX_ELR_SIG_UHR_SU:
		ath12k_wifi8_hal_mon_tx_parse_elr_sig_uhr(tlv_data,
							  tx_ppdu_info);
		break;

	case HAL_MACTX_HT_SIG: {
		ath12k_wifi8_hal_mon_tx_parse_ht_sig(tlv_data, tx_ppdu_info);
		status = HAL_TX_MON_MACTX_HT_SIG;
		break;
	}

	case HAL_MACTX_USER_DESC_PER_USER:
		ath12k_wifi8_hal_mon_tx_parse_user_desc_per_user(tlv_data,
								 userid,
								 tx_ppdu_info);
		break;

	case HAL_MACTX_USER_DESC_COMMON:
		ath12k_wifi8_hal_mon_tx_parse_user_desc_common(tlv_data,
							       tx_ppdu_info,
							       tlv_len);
		break;

	case HAL_FW2SW_MON: {
		ath12k_wifi8_hal_mon_tx_parse_fw2sw(tlv_data, userid, status_info);
		status = HAL_TX_MON_FW2SW;
		break;
	}

	default:
		break;
	}
	return status;
}

int
ath12k_wifi8_extract_tx_mon_ring_desc(struct ath12k_hal *hal,
				      void *ring_entry,
				      struct ath12k_mon_ring_desc_info *desc_info)
{
	struct hal_mon_dest_desc *mon_dst_desc =
		(struct hal_mon_dest_desc *)ring_entry;
	u64 desc_va;
	u32 info0;

	info0 = le32_to_cpu(mon_dst_desc->info0);
	desc_info->ppdu_id = le32_to_cpu(mon_dst_desc->ppdu_id);
	desc_info->empty_desc = le32_get_bits(info0,
					      HAL_MON_DEST_INFO0_EMPTY_DESC);
	desc_info->end_offset = le32_get_bits(info0,
					      HAL_MON_DEST_INFO0_END_OFFSET);
	desc_info->end_reason = le32_get_bits(info0,
					      HAL_MON_DEST_INFO0_END_REASON);
	desc_info->pkt_buf_cnt = le32_get_bits(info0,
					       HAL_MON_DEST_INFO0_PKT_BUFFER_CNT);

	if (desc_info->empty_desc)
		return 0;

	desc_va = le64_to_cpu(mon_dst_desc->stat_buf_va);
	desc_info->mon_desc = (struct ath12k_dp_mon_desc *)(uintptr_t)desc_va;

	if (!desc_info->mon_desc || desc_va < PAGE_SIZE)
		return -EINVAL;

	if (desc_info->mon_desc->magic != ATH12K_MON_MAGIC_VALUE)
		return -EINVAL;

	return 0;
}

void ath12k_wifi8_hal_mon_ops_init(struct ath12k_hal *hal,
				   u8 hw_version)
{
	switch (hw_version) {
	case ATH12K_HW_QCN9625_HW10:
	case ATH12K_HW_QCN9589_HW10:
		ath12k_wifi8_hal_qcn9625_hw10_mon_ops_init(hal);
		break;
	case ATH12K_HW_QCN9625_HW20:
		ath12k_wifi8_hal_qcn9625_hw20_mon_ops_init(hal);
		hal->tlv_hdr_tag_shift = HAL_TLV_64_HDR_TAG_NOSHIFT;
		break;
	default:
		break;
	}
}
