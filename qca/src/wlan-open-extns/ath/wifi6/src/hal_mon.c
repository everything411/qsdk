// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_rx.h"
#include "hal_rx_desc.h"
#include "../../hal_mon_cmn.h"
#include "hal_mon.h"
#include "hal_qcn9074.h"
#include "hal_mon_qcn9074.h"
#include "dp_rx.h"

static __always_inline void
ath12k_wifi6_hal_mon_get_nrp_mac_addr(u16 addr_l16, u32 addr_h32, u8 *addr)
{
	memcpy(addr, &addr_l16, 2);
	memcpy(addr + 2, &addr_h32, ETH_ALEN - 2);
}

static __always_inline void
ath12k_wifi6_hal_mon_populate_mu_user_info(struct hal_rx_mon_ppdu_info *ppdu_info,
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
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_vht_sig_a(const void *tlv_data,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	u8 group_id = 0;

	uint8_t *vht_sig_a_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_VHT_SIG_A_0,
				      VHT_SIG_A_INFO_PHYRX_VHT_SIG_A_INFO_DETAILS);

	value = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_1, SU_MU_CODING);
	ppdu_info->ldpc = (value == HAL_RX_SU_MU_CODING_LDPC) ? 1 : 0;
	group_id = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_0, GROUP_ID);
	ppdu_info->vht_flag_values5 = group_id;
	ppdu_info->mcs = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_1, MCS);
	ppdu_info->sgi = HAL_RX_GET(vht_sig_a_info,
				    VHT_SIG_A_INFO_1, GI_SETTING);
	ppdu_info->is_stbc = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_0, STBC);
	value =  HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_0, N_STS);
	value = value & VHT_SIG_SU_NSS_MASK;
	if (ppdu_info->is_stbc && (value > 0))
		value = ((value + 1) >> 1) - 1;
	ppdu_info->nss = ((value & VHT_SIG_SU_NSS_MASK) + 1);

	ppdu_info->vht_flag_values3[0] = (((ppdu_info->mcs) << 4) | ppdu_info->nss);
	ppdu_info->bw = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_0, BANDWIDTH);
	ppdu_info->vht_flag_values2 = ppdu_info->bw;
	ppdu_info->vht_flag_values4 = HAL_RX_GET(vht_sig_a_info,
						 VHT_SIG_A_INFO_1, SU_MU_CODING);

	ppdu_info->beamformed = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO_1, BEAMFORMED);
	if (group_id == 0 || group_id == 63)
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;
	else
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_MIMO;

	if (ppdu_info->ppdu_info_extn.phyrx_abort_info) {
		struct ath12k_wifi6_phyrx_abort_info *abort_info =
			ppdu_info->ppdu_info_extn.phyrx_abort_info;

		abort_info->vht_no_txop_ps =
			le32_get_bits(*(__le32 *)vht_sig_a_info,
				      VHT_SIG_A_INFO_0_TXOP_PS_NOT_ALLOWED);
		abort_info->vht_crc =
			le32_get_bits(*(__le32 *)(vht_sig_a_info + sizeof(u32)),
				      VHT_SIG_A_INFO_1_CRC);
	}
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_ht_sig(const void *tlv_data,
				  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	uint8_t *ht_sig_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HT_SIG_0,
				      HT_SIG_INFO_PHYRX_HT_SIG_INFO_DETAILS);

	value = HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, FEC_CODING);
	ppdu_info->ldpc = (value == HAL_RX_SU_MU_CODING_LDPC) ? 1 : 0;
	ppdu_info->mcs = HAL_RX_GET(ht_sig_info, HT_SIG_INFO_0, MCS);
	ppdu_info->bw = HAL_RX_GET(ht_sig_info, HT_SIG_INFO_0, CBW);
	ppdu_info->sgi = HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, SHORT_GI);
	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;
	ppdu_info->nss = (ppdu_info->mcs >> 3) + 1;
	ppdu_info->is_stbc = HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, STBC);

	if (ppdu_info->ppdu_info_extn.phyrx_abort_info) {
		struct ath12k_wifi6_phyrx_abort_info *abort_info =
			ppdu_info->ppdu_info_extn.phyrx_abort_info;

		abort_info->ht_length =
			HAL_RX_GET(ht_sig_info, HT_SIG_INFO_0, LENGTH);
		abort_info->smoothing =
			HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, SMOOTHING);
		abort_info->not_sounding =
			HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, NOT_SOUNDING);
		abort_info->aggregation =
			HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, AGGREGATION);
		abort_info->ht_stbc =
			HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, STBC);
		abort_info->ht_crc =
			HAL_RX_GET(ht_sig_info, HT_SIG_INFO_1, CRC);
	}
}

static __always_inline u8
ath12k_wifi6_hal_mon_map_legacy_rate_to_hw_rate(u8 rate)
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
ath12k_wifi6_hal_mon_parse_l_sig_b(const void *tlv_data,
				   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	u8 rate;
	uint8_t *l_sig_b_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_L_SIG_B_0,
				      L_SIG_B_INFO_PHYRX_L_SIG_B_INFO_DETAILS);

	value = HAL_RX_GET(l_sig_b_info, L_SIG_B_INFO_0, RATE);
	switch (value) {
	case 1:
		rate = HAL_RX_LEGACY_RATE_LP_1_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS3;
		break;
	case 2:
		rate = HAL_RX_LEGACY_RATE_LP_2_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS2;
		break;
	case 3:
		rate = HAL_RX_LEGACY_RATE_LP_5_5_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS1;
		break;
	case 4:
		rate = HAL_RX_LEGACY_RATE_LP_11_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS0;
		break;
	case 5:
		rate = HAL_RX_LEGACY_RATE_SP_2_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS6;
		break;
	case 6:
		rate = HAL_RX_LEGACY_RATE_SP_5_5_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS5;
		break;
	case 7:
		rate = HAL_RX_LEGACY_RATE_SP_11_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS4;
		break;
	default:
		rate = HAL_RX_LEGACY_RATE_INVALID;
		break;
	}

	ppdu_info->rate = ath12k_wifi6_hal_mon_map_legacy_rate_to_hw_rate(rate);
	ppdu_info->cck_flag = 1;
	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_l_sig_a(const void *tlv_data,
				   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	u8 rate;
	uint8_t *l_sig_a_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_L_SIG_A_0,
				      L_SIG_A_INFO_PHYRX_L_SIG_A_INFO_DETAILS);

	value = HAL_RX_GET(l_sig_a_info, L_SIG_A_INFO_0, RATE);
	switch (value) {
	case 8:
		rate = HAL_RX_LEGACY_RATE_OFDM_48_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS0;
		break;
	case 9:
		rate = HAL_RX_LEGACY_RATE_OFDM_24_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS1;
		break;
	case 10:
		rate = HAL_RX_LEGACY_RATE_OFDM_12_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS2;
		break;
	case 11:
		rate = HAL_RX_LEGACY_RATE_OFDM_6_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS3;
		break;
	case 12:
		rate = HAL_RX_LEGACY_RATE_OFDM_54_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS4;
		break;
	case 13:
		rate = HAL_RX_LEGACY_RATE_OFDM_36_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS5;
		break;
	case 14:
		rate = HAL_RX_LEGACY_RATE_OFDM_18_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS6;
		break;
	case 15:
		rate = HAL_RX_LEGACY_RATE_OFDM_9_MBPS;
		ppdu_info->mcs = HAL_LEGACY_MCS7;
		break;
	default:
		rate = HAL_RX_LEGACY_RATE_OFDM_INVALID;
		break;
	}

	ppdu_info->rate = rate;
	ppdu_info->ofdm_flag = 1;
	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;

	if (ppdu_info->ppdu_info_extn.phyrx_abort_info) {
		struct ath12k_wifi6_phyrx_abort_info *abort_info =
			ppdu_info->ppdu_info_extn.phyrx_abort_info;

		abort_info->l_sig_length =
			le32_get_bits(*(__le32 *)l_sig_a_info, L_SIG_A_INFO_0_LENGTH);
		abort_info->l_sig_a_parity =
			le32_get_bits(*(__le32 *)l_sig_a_info, L_SIG_A_INFO_0_PARITY);
		abort_info->l_sig_a_pkt_type =
			le32_get_bits(*(__le32 *)l_sig_a_info, L_SIG_A_INFO_0_RATE_MASK);
	}
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_he_sig_b2_ofdma(const void *tlv_data,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	uint8_t *he_sig_b2_ofdma_info = (uint8_t *)tlv_data +
		HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_B2_OFDMA_0,
			      HE_SIG_B2_OFDMA_INFO_PHYRX_HE_SIG_B2_OFDMA_INFO_DETAILS);

	/*
	 * Not all "HE" fields can be updated from
	 * WIFIPHYRX_HE_SIG_A_MU_DL_E TLV. Use WIFIPHYRX_HE_SIG_B2_MU_E
	 * to populate rest of "HE" fields for MU OFDMA scenarios.
	 */

	/* HE-data1 */
	ppdu_info->he_data1 |= HE_MCS_KNOWN | HE_DCM_KNOWN | HE_CODING_KNOWN;

	/* HE-data2 */
	ppdu_info->he_data2 |= HE_TXBF_KNOWN;

	/* HE-data3 */
	value = HAL_RX_GET(he_sig_b2_ofdma_info, HE_SIG_B2_OFDMA_INFO_0, STA_MCS);
	ppdu_info->mcs = value;
	value = value << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_b2_ofdma_info, HE_SIG_B2_OFDMA_INFO_0, STA_DCM);
	value = value << HE_DCM_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_b2_ofdma_info, HE_SIG_B2_OFDMA_INFO_0, STA_CODING);
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	/* HE-data4 */
	value = HAL_RX_GET(he_sig_b2_ofdma_info, HE_SIG_B2_OFDMA_INFO_0, STA_ID);
	value = value << HE_STA_ID_SHIFT;
	ppdu_info->he_data4 |= value;

	/* HE-data5 */
	value = HAL_RX_GET(he_sig_b2_ofdma_info, HE_SIG_B2_OFDMA_INFO_0, TXBF);
	value = value << HE_TXBF_SHIFT;
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_b2_ofdma_info, HE_SIG_B2_OFDMA_INFO_0, NSTS);
	/* value n indicates n+1 spatial streams */
	value++;
	ppdu_info->nss = value;
	ppdu_info->he_data6 |= value;
	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_OFDMA;
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_he_sig_b2_mu(const void *tlv_data,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	uint8_t *he_sig_b2_mu_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_B2_MU_0,
				      HE_SIG_B2_MU_INFO_PHYRX_HE_SIG_B2_MU_INFO_DETAILS);

	/*
	 * Not all "HE" fields can be updated from
	 * WIFIPHYRX_HE_SIG_A_MU_DL_E TLV. Use WIFIPHYRX_HE_SIG_B2_MU_E
	 * to populate rest of the "HE" fields for MU scenarios.
	 */

	/* HE-data1 */
	ppdu_info->he_data1 |= HE_MCS_KNOWN | HE_CODING_KNOWN;

	/* HE-data2 */

	/* HE-data3 */
	value = HAL_RX_GET(he_sig_b2_mu_info, HE_SIG_B2_MU_INFO_0, STA_MCS);
	ppdu_info->mcs = value;
	value = value << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_b2_mu_info, HE_SIG_B2_MU_INFO_0, STA_CODING);
	ppdu_info->ldpc = value;
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	/* HE-data4 */
	value = HAL_RX_GET(he_sig_b2_mu_info, HE_SIG_B2_MU_INFO_0, STA_ID);
	value = value << HE_STA_ID_SHIFT;
	ppdu_info->he_data4 |= value;

	/* HE-data5 */

	/* HE-data6 */
	value = HAL_RX_GET(he_sig_b2_mu_info, HE_SIG_B2_MU_INFO_0, NSTS);
	/* value n indicates n+1 spatial streams */
	value++;
	ppdu_info->nss = value;
	ppdu_info->he_data6 |= value;
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_he_sig_b1_mu(const void *tlv_data,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	uint8_t *he_sig_b1_mu_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_B1_MU_0,
				      HE_SIG_B1_MU_INFO_PHYRX_HE_SIG_B1_MU_INFO_DETAILS);

	value = HAL_RX_GET(he_sig_b1_mu_info, HE_SIG_B1_MU_INFO_0, RU_ALLOCATION);
	ppdu_info->he_RU[0] = value;
	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_MIMO;
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_he_sig_mu(const void *tlv_data,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	u8 he_stbc = 0;
	u16 he_gi = 0, he_ltf = 0;

	uint8_t *he_sig_a_mu_dl_info = (uint8_t *)tlv_data +
		HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_A_MU_DL_0,
			      HE_SIG_A_MU_DL_INFO_PHYRX_HE_SIG_A_MU_DL_INFO_DETAILS);

	ppdu_info->he_mu_flags = 1;

	/* HE Flags */
	/*data1*/
	ppdu_info->he_data1 = HE_MU_FORMAT_TYPE;
	ppdu_info->he_data1 |=
			HE_BSS_COLOR_KNOWN |
			HE_DL_UL_KNOWN |
			HE_LDPC_EXTRA_SYMBOL_KNOWN |
			HE_STBC_KNOWN |
			HE_DATA_BW_RU_KNOWN |
			HE_DOPPLER_KNOWN;

	/* data2 */
	ppdu_info->he_data2 =
			HE_GI_KNOWN |
			HE_LTF_SYMBOLS_KNOWN |
			HE_PRE_FEC_PADDING_KNOWN |
			HE_PE_DISAMBIGUITY_KNOWN |
			HE_TXOP_KNOWN |
			HE_MIDABLE_PERIODICITY_KNOWN;

	/* data3 */
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, BSS_COLOR_ID);
	ppdu_info->he_data3 = value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, DL_UL_FLAG);
	value = value << HE_DL_UL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_1,
			   LDPC_EXTRA_SYMBOL);
	value = value << HE_LDPC_EXTRA_SYMBOL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_1, STBC);
	he_stbc = value;
	value = value << HE_STBC_SHIFT;
	ppdu_info->he_data3 |= value;

	/* data4 */
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0,
			   SPATIAL_REUSE);
	ppdu_info->he_data4 = value;

	/* data5 */
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, TRANSMIT_BW);
	ppdu_info->he_data5 = value;
	ppdu_info->bw = value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, CP_LTF_SIZE);

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

	ppdu_info->sgi = he_gi;
	ppdu_info->ltf_size = he_ltf;
	hal_get_radiotap_gi_ltf(&he_gi, &he_ltf);
	value = he_gi << HE_GI_SHIFT;
	ppdu_info->he_data5 |= value;

	value = he_ltf << HE_LTF_SIZE_SHIFT;
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_1, NUM_LTF_SYMBOLS);
	value = (value << HE_LTF_SYM_SHIFT);
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_1,
			   PACKET_EXTENSION_A_FACTOR);
	value = value << HE_PRE_FEC_PAD_SHIFT;
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_1,
			   PACKET_EXTENSION_PE_DISAMBIGUITY);
	value = value << HE_PE_DISAMBIGUITY_SHIFT;
	ppdu_info->he_data5 |= value;

	/*data6*/
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0,
			   DOPPLER_INDICATION);
	value = value << HE_DOPPLER_SHIFT;
	ppdu_info->he_data6 |= value;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_1,
			   TXOP_DURATION);
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

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, MCS_OF_SIG_B);
	ppdu_info->he_flags1 |= value;
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, DCM_OF_SIG_B);
	value = value << HE_DCM_FLAG_1_SHIFT;
	ppdu_info->he_flags1 |= value;

	/* HE-MU-flags2 */
	ppdu_info->he_flags2 = HE_BW_KNOWN;

	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, TRANSMIT_BW);
	ppdu_info->he_flags2 |= value;
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0, COMP_MODE_SIG_B);
	value = value << HE_SIG_B_COMPRESSION_FLAG_2_SHIFT;
	ppdu_info->he_flags2 |= value;
	value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO_0,
			   NUM_SIG_B_SYMBOLS);
	value = value - 1;
	value = value << HE_NUM_SIG_B_SYMBOLS_SHIFT;
	ppdu_info->he_flags2 |= value;

	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_MIMO;

	if (ppdu_info->ppdu_info_extn.phyrx_abort_info) {
		struct ath12k_wifi6_phyrx_abort_info *abort_info =
			ppdu_info->ppdu_info_extn.phyrx_abort_info;

		abort_info->he_sig_a_mu_dl_crc =
			le32_get_bits(*(__le32 *)(he_sig_a_mu_dl_info + sizeof(u32)),
				      HE_SIG_A_MU_DL_INFO_1_CRC);
	}
}

static __always_inline void
ath12k_wifi6_hal_mon_parse_he_sig_su(const void *tlv_data,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 value = 0;
	u8 he_dcm = 0, he_stbc = 0;
	u16 he_gi = 0, he_ltf = 0;
	uint8_t *he_sig_a_su_info = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_A_SU_0,
				      HE_SIG_A_SU_INFO_PHYRX_HE_SIG_A_SU_INFO_DETAILS);

	ppdu_info->he_flags = 1;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0,
			   FORMAT_INDICATION);
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

	/* data3 */
	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, BSS_COLOR_ID);
	ppdu_info->he_data3 = value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, BEAM_CHANGE);
	value = value << HE_BEAM_CHANGE_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, DL_UL_FLAG);
	value = value << HE_DL_UL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, TRANSMIT_MCS);
	ppdu_info->mcs = value;
	value = value << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, DCM);
	he_dcm = value;
	value = value << HE_DCM_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, CODING);
	ppdu_info->ldpc = (value == HAL_RX_SU_MU_CODING_LDPC) ? 1 : 0;
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, LDPC_EXTRA_SYMBOL);
	value = value << HE_LDPC_EXTRA_SYMBOL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, STBC);
	he_stbc = value;
	value = value << HE_STBC_SHIFT;
	ppdu_info->he_data3 |= value;

	/* data4 */
	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, SPATIAL_REUSE);
	ppdu_info->he_data4 = value;

	/* data5 */
	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, TRANSMIT_BW);
	ppdu_info->he_data5 = value;
	ppdu_info->bw = value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, CP_LTF_SIZE);
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
	ppdu_info->sgi = he_gi;
	hal_get_radiotap_gi_ltf(&he_gi, &he_ltf);
	value = he_gi << HE_GI_SHIFT;
	ppdu_info->he_data5 |= value;
	value = he_ltf << HE_LTF_SIZE_SHIFT;
	ppdu_info->ltf_size = he_ltf;
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, NSTS);
	value = (value << HE_LTF_SYM_SHIFT);
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1,
			   PACKET_EXTENSION_A_FACTOR);
	value = value << HE_PRE_FEC_PAD_SHIFT;
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, TXBF);
	value = value << HE_TXBF_SHIFT;
	ppdu_info->he_data5 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1,
			   PACKET_EXTENSION_PE_DISAMBIGUITY);
	value = value << HE_PE_DISAMBIGUITY_SHIFT;
	ppdu_info->he_data5 |= value;

	/* data6 */
	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_0, NSTS);
	value++;
	ppdu_info->nss = value;
	ppdu_info->he_data6 = value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, DOPPLER_INDICATION);
	value = value << HE_DOPPLER_SHIFT;
	ppdu_info->he_data6 |= value;

	value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, TXOP_DURATION);
	value = value << HE_TXOP_SHIFT;
	ppdu_info->he_data6 |= value;

	ppdu_info->beamformed = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO_1, TXBF);
	ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;

	if (ppdu_info->ppdu_info_extn.phyrx_abort_info) {
		struct ath12k_wifi6_phyrx_abort_info *abort_info =
			ppdu_info->ppdu_info_extn.phyrx_abort_info;

		abort_info->he_sig_a_su_crc =
			le32_get_bits(*(__le32 *)(he_sig_a_su_info + sizeof(u32)),
				      HE_SIG_A_SU_INFO_1_CRC);
	}
}

static __always_inline void
ath12k_wifi6_hal_update_frame_type_cnt(uint8_t *rx_mpdu_start,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u16 frame_ctrl;
	u8 fc_type;

	if (HAL_RX_GET_FC_VALID(rx_mpdu_start)) {
		frame_ctrl = HAL_RX_GET(rx_mpdu_start, RX_MPDU_INFO_14,
					MPDU_FRAME_CONTROL_FIELD);
		fc_type = HAL_RX_GET_FRAME_CTRL_TYPE(frame_ctrl);
		if (fc_type == HAL_RX_FRAME_CTRL_TYPE_MGMT)
			ppdu_info->ppdu_info_extn.frm_type_info.rx_mgmt_cnt++;
		else if (fc_type == HAL_RX_FRAME_CTRL_TYPE_CTRL)
			ppdu_info->ppdu_info_extn.frm_type_info.rx_ctrl_cnt++;
		else if (fc_type == HAL_RX_FRAME_CTRL_TYPE_DATA)
			ppdu_info->ppdu_info_extn.frm_type_info.rx_data_cnt++;
	}
}

void
ath12k_wifi6_hal_mon_rx_parse_mpdu_start(const void *tlv_data, u32 userid,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	uint8_t *rx_mpdu_start = (uint8_t *)tlv_data;
	uint32_t ppdu_id = HAL_RX_GET_PPDU_ID(rx_mpdu_start);
	uint8_t filter_category = 0;
	u16 peer_id, addr_16;
	u32 addr_32;

	ath12k_wifi6_hal_update_frame_type_cnt(rx_mpdu_start, ppdu_info);
	ppdu_info->nrp_info.fc_valid = HAL_RX_GET_FC_VALID(rx_mpdu_start);
	ppdu_info->nrp_info.to_ds_flag = HAL_RX_GET_TO_DS_FLAG(rx_mpdu_start);
	ppdu_info->nrp_info.frame_control = HAL_RX_GET(rx_mpdu_start,
						       RX_MPDU_INFO_14,
						       MPDU_FRAME_CONTROL_FIELD);

	ppdu_info->ppdu_info_extn.sw_frame_group_id =
				HAL_RX_GET_SW_FRAME_GROUP_ID(rx_mpdu_start);

	peer_id = HAL_RX_GET_SW_PEER_ID(rx_mpdu_start);
	if (peer_id)
		ppdu_info->peer_id = peer_id;
	if (unlikely(userid >= HAL_MAX_UL_MU_USERS))
		return;

	ppdu_info->userstats[userid].sw_peer_id = peer_id;

	ppdu_info->userstats[userid].enc_type =
				HAL_RX_GET_ENCRYPT_TYPE(rx_mpdu_start);

	ppdu_info->userstats[userid].ampdu_id = ppdu_id;

	if (ppdu_info->ppdu_info_extn.sw_frame_group_id ==
	    HAL_MPDU_SW_FRAME_GROUP_NULL_DATA) {
		ppdu_info->frame_control_info_valid =
						ppdu_info->nrp_info.fc_valid;
		ppdu_info->frame_control = ppdu_info->nrp_info.frame_control;
	}

	ppdu_info->nrp_info.mac_addr2_valid =
					HAL_RX_GET_MAC_ADDR2_VALID(rx_mpdu_start);

	addr_16 = HAL_RX_GET(rx_mpdu_start, RX_MPDU_INFO_16, MAC_ADDR_AD2_15_0);
	addr_32 = HAL_RX_GET(rx_mpdu_start, RX_MPDU_INFO_17, MAC_ADDR_AD2_47_16);

	ath12k_wifi6_hal_mon_get_nrp_mac_addr(addr_16, addr_32,
					      ppdu_info->nrp_info.mac_addr2);

	if (ppdu_info->prev_ppdu_id != ppdu_id) {
		ppdu_info->prev_ppdu_id = ppdu_id;
		ppdu_info->ppdu_len = HAL_RX_GET(rx_mpdu_start, RX_MPDU_INFO_13,
						 MPDU_LENGTH);
	} else {
		ppdu_info->ppdu_len += HAL_RX_GET(rx_mpdu_start, RX_MPDU_INFO_13,
						  MPDU_LENGTH);
	}

	filter_category = HAL_RX_GET_FILTER_CATEGORY(rx_mpdu_start);
	if (filter_category == 0)
		ppdu_info->rxpcu_filter_pass = 1;
	else if (filter_category == 1)
		ppdu_info->monitor_direct_used = 1;

	ppdu_info->nrp_info.mcast_bcast = HAL_RX_GET(rx_mpdu_start,
						     RX_MPDU_INFO_13, MCAST_BCAST);
}

static __always_inline void
ath12k_wifi6_hal_mon_handle_ofdma_info(const void *tlv_data,
				       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 =
				HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_11,
					   SW_RESPONSE_REFERENCE_PTR);
	rx_user_status->ul_ofdma_user_v0_word1 =
				HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_22,
					   SW_RESPONSE_REFERENCE_PTR_EXT);
}
static __always_inline void
ath12k_wifi6_hal_mon_populate_byte_count(const void *tlv_data,
					 struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		HAL_RX_GET(tlv_data,
			   RX_PPDU_END_USER_STATS_17, MPDU_OK_BYTE_COUNT);
	rx_user_status->mpdu_err_byte_count =
		HAL_RX_GET(tlv_data,
			   RX_PPDU_END_USER_STATS_19, MPDU_ERR_BYTE_COUNT);
}

static __always_inline void
ath12k_wifi6_hal_rx_update_rssi_chain(struct hal_rx_mon_ppdu_info *ppdu_info,
				      uint8_t *rssi_info_tlv)
{
	HAL_RX_PPDU_UPDATE_RSSI(ppdu_info, rssi_info_tlv)
}

static __always_inline void
ath12k_wifi6_hal_mon_rx_parse_ppdu_end_usr_stats(const void *tlv_data,
						 u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	unsigned long tid_bitmap = 0;
	uint16_t seq = 0;

	ppdu_info->nss = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_1, NSS) + 1;
	ppdu_info->mcs = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_1, MCS);

	ppdu_info->ast_index =
			HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_4,
				   AST_INDEX);
	tid_bitmap = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_12,
				RECEIVED_QOS_DATA_TID_BITMAP);

	ppdu_info->tid = ffs(tid_bitmap) - 1;

	ppdu_info->tcp_msdu_count = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_9,
					       TCP_MSDU_COUNT) +
				    HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_10,
					       TCP_ACK_MSDU_COUNT);
	ppdu_info->udp_msdu_count = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_9,
					       UDP_MSDU_COUNT);
	ppdu_info->other_msdu_count = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_10,
						 OTHER_MSDU_COUNT);

	if (ppdu_info->ppdu_info_extn.sw_frame_group_id !=
	    HAL_MPDU_SW_FRAME_GROUP_NULL_DATA) {
		ppdu_info->frame_control_info_valid =
			HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_3,
				   FRAME_CONTROL_INFO_VALID);

		if (ppdu_info->frame_control_info_valid)
			ppdu_info->frame_control = HAL_RX_GET(tlv_data,
							      RX_PPDU_END_USER_STATS_4,
							      FRAME_CONTROL_FIELD);
	}

	ppdu_info->data_sequence_control_info_valid =
		HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_3,
			   DATA_SEQUENCE_CONTROL_INFO_VALID);

	seq = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_5,
			 FIRST_DATA_SEQ_CTRL);
	if (ppdu_info->data_sequence_control_info_valid)
		ppdu_info->first_data_seq_ctrl = seq;

	ppdu_info->preamble_type = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_3,
					      HT_CONTROL_FIELD_PKT_TYPE);
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
	default:
		break;
	}

	ppdu_info->num_mpdu_fcs_ok = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_3,
						MPDU_CNT_FCS_OK);
	ppdu_info->num_mpdu_fcs_err = HAL_RX_GET(tlv_data, RX_PPDU_END_USER_STATS_2,
						 MPDU_CNT_FCS_ERR);

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
			&ppdu_info->userstats[userid];

		ath12k_wifi6_hal_mon_handle_ofdma_info(tlv_data,
						       rxuser_stats);
		ath12k_wifi6_hal_mon_populate_byte_count(tlv_data,
							 rxuser_stats);

		if (ppdu_info->num_mpdu_fcs_ok > 1 ||
		    ppdu_info->num_mpdu_fcs_err > 1)
			ppdu_info->userstats[userid].ampdu_present = true;

		ppdu_info->num_users += 1;

		ath12k_wifi6_hal_mon_populate_mu_user_info(ppdu_info,
							   rxuser_stats);
	}
}

enum hal_rx_mon_status
ath12k_wifi6_hal_mon_rx_parse_status_tlv(struct ath12k_hal *hal,
					 struct hal_rx_mon_ppdu_info *ppdu_info,
					 struct hal_tlv_parsed_hdr *tlv_parsed_hdr)
{
	const void *tlv_data = tlv_parsed_hdr->data;
	u32 userid;
	u16 tlv_tag, tlv_len;

	tlv_tag = tlv_parsed_hdr->tag;
	tlv_len = tlv_parsed_hdr->len;
	userid = tlv_parsed_hdr->userid;

	switch (tlv_tag) {
	case HAL_RX_PPDU_START: {
		/* Reset ppdu_info before processing the ppdu */
		memset(ppdu_info, 0, sizeof(struct hal_rx_mon_ppdu_info));

		ppdu_info->last_ppdu_id =
			ppdu_info->ppdu_id =
				HAL_RX_GET(tlv_data, RX_PPDU_START_0,
					   PHY_PPDU_ID);
		/* channel number is set in PHY meta data */
		ppdu_info->chan_num =
			(HAL_RX_GET(tlv_data, RX_PPDU_START_1,
				    SW_PHY_META_DATA) & 0x0000FFFF);
		ppdu_info->freq =
			(HAL_RX_GET(tlv_data, RX_PPDU_START_1,
				    SW_PHY_META_DATA) & 0xFFFF0000) >>
						HAL_PPDU_START_FREQ_MASK;

		ppdu_info->ppdu_ts =
			HAL_RX_GET(tlv_data, RX_PPDU_START_2,
				   PPDU_START_TIMESTAMP);
		ppdu_info->ppdu_info_extn.rx_state = HAL_RX_MON_PPDU_START;
		break;
	}
	case HAL_RX_PPDU_END_USER_STATS:
		ath12k_wifi6_hal_mon_rx_parse_ppdu_end_usr_stats(tlv_data,
								 userid, ppdu_info);
		break;
	case HAL_RX_PPDU_END_USER_STATS_EXT:
		break;
	case HAL_PHYRX_HT_SIG:
		ath12k_wifi6_hal_mon_parse_ht_sig(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_L_SIG_B:
		ath12k_wifi6_hal_mon_parse_l_sig_b(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_L_SIG_A:
		ath12k_wifi6_hal_mon_parse_l_sig_a(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_VHT_SIG_A:
		ath12k_wifi6_hal_mon_parse_vht_sig_a(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_A_SU:
		ath12k_wifi6_hal_mon_parse_he_sig_su(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_A_MU_DL:
		ath12k_wifi6_hal_mon_parse_he_sig_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B1_MU:
		ath12k_wifi6_hal_mon_parse_he_sig_b1_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B2_MU:
		ath12k_wifi6_hal_mon_parse_he_sig_b2_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B2_OFDMA:
		ath12k_wifi6_hal_mon_parse_he_sig_b2_ofdma(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_RSSI_LEGACY: {
		uint8_t reception_type;
		int8_t rssi_value;
		uint8_t *rssi_info_tlv = (uint8_t *)tlv_data +
			HAL_RX_OFFSET(UNIFIED_PHYRX_RSSI_LEGACY_19,
				      RECEIVE_RSSI_INFO_PREAMBLE_RSSI_INFO_DETAILS);

		ppdu_info->rssi_comb = HAL_RX_GET(tlv_data,
						  PHYRX_RSSI_LEGACY_35, RSSI_COMB);
		ppdu_info->bw = HAL_RX_GET(tlv_data, PHYRX_RSSI_LEGACY_0,
					   RECEIVE_BANDWIDTH);

		reception_type = HAL_RX_GET(tlv_data, PHYRX_RSSI_LEGACY_0,
					    RECEPTION_TYPE);
		switch (reception_type) {
		case HAL_RECEPTION_TYPE_ULOFMDA:
			ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_OFDMA;
			ppdu_info->ulofdma_flag = 1;
			ppdu_info->he_data1 = HE_TRIG_FORMAT_TYPE;
			break;
		case HAL_RECEPTION_TYPE_ULMIMO:
			ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_MIMO;
			ppdu_info->he_data1 = HE_MU_FORMAT_TYPE;
			break;
		default:
			ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;
			break;
		}

		ath12k_wifi6_hal_rx_update_rssi_chain(ppdu_info, rssi_info_tlv);

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_0, RSSI_PRI20_CHAIN0);
		ppdu_info->ppdu_info_extn.rssi[0] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_2, RSSI_PRI20_CHAIN1);
		ppdu_info->ppdu_info_extn.rssi[1] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_4, RSSI_PRI20_CHAIN2);
		ppdu_info->ppdu_info_extn.rssi[2] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_6, RSSI_PRI20_CHAIN3);
		ppdu_info->ppdu_info_extn.rssi[3] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_8, RSSI_PRI20_CHAIN4);
		ppdu_info->ppdu_info_extn.rssi[4] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_10, RSSI_PRI20_CHAIN5);
		ppdu_info->ppdu_info_extn.rssi[5] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_12, RSSI_PRI20_CHAIN6);
		ppdu_info->ppdu_info_extn.rssi[6] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv,
					RECEIVE_RSSI_INFO_14, RSSI_PRI20_CHAIN7);
		ppdu_info->ppdu_info_extn.rssi[7] = rssi_value;
		break;
	}
	case HAL_PHYRX_OTHER_RECEIVE_INFO:
		break;
	case HAL_RX_PPDU_START_USER_INFO:
		break;

	case HAL_RX_PPDU_END:
		ppdu_info->ppdu_info_extn.rx_state = HAL_RX_MON_PPDU_END;
		break;
	case HAL_PHYRX_PKT_END:
		break;

	case HAL_RXPCU_PPDU_END_INFO: {
		ppdu_info->ppdu_info_extn.rx_antenna = HAL_RX_GET(tlv_data,
								  RXPCU_PPDU_END_INFO_2,
								  RX_ANTENNA);
		ppdu_info->tsft =
			HAL_RX_GET(tlv_data, RXPCU_PPDU_END_INFO_1,
				   WB_TIMESTAMP_UPPER_32);
		ppdu_info->tsft = (ppdu_info->tsft << 32) |
					HAL_RX_GET(tlv_data, RXPCU_PPDU_END_INFO_0,
						   WB_TIMESTAMP_LOWER_32);
		ppdu_info->rx_duration =
			HAL_RX_GET(tlv_data, RXPCU_PPDU_END_INFO_9,
				   RX_PPDU_DURATION);
		ath12k_wifi6_hal_rx_get_phyrx_abort(tlv_data, ppdu_info);
		break;
	}
	case HAL_PHYRX_ABORT_REQUEST:
		/* PHY abort request info is parsed from RXPCU_PPDU_END_INFO TLV */
		break;
	case HAL_RX_MPDU_START:
		ath12k_wifi6_hal_mon_rx_parse_mpdu_start(tlv_data, userid, ppdu_info);
		break;
	case HAL_RX_MSDU_START:
		/* TODO: add msdu start parsing logic */
		break;
	case HAL_RX_MSDU_END:
		return HAL_RX_MON_STATUS_MSDU_END;
	case HAL_RX_MPDU_END:
		return HAL_RX_MON_STATUS_MPDU_END;
	case HAL_DUMMY:
		return HAL_RX_MON_STATUS_BUF_DONE;
	case HAL_RX_HEADER:
		return HAL_RX_MON_STATUS_RX_HDR;
	case HAL_RX_PPDU_END_STATUS_DONE:
	case 0:
		return HAL_RX_MON_STATUS_PPDU_DONE;
	default:
		break;
	}

	return HAL_RX_MON_STATUS_PPDU_NOT_DONE;
}

void
ath12k_wifi6_dp_mon_rx_next_link_desc_get(struct hal_rx_msdu_link *msdu_link,
					  struct hal_rx_buf_info *buf_info)
{
	struct ath12k_buffer_addr *buf_addr_info;

	buf_addr_info = &msdu_link->buf_addr_info;

	buf_info->paddr = (((u64)le32_get_bits(buf_addr_info->info1,
					       WIFI6_BUFFER_ADDR_INFO1_ADDR)) << 32) |
					le32_get_bits(buf_addr_info->info0,
						      WIFI6_BUFFER_ADDR_INFO0_ADDR);

	buf_info->sw_cookie = le32_get_bits(buf_addr_info->info1,
					    WIFI6_BUFFER_ADDR_INFO1_SW_COOKIE);
	buf_info->rbm = le32_get_bits(buf_addr_info->info1,
				      WIFI6_BUFFER_ADDR_INFO1_RET_BUF_MGR);
}


const struct hal_mon_ops hal_qcn9074_mon_ops = {
	.get_mon_mpdu_start_wmask = NULL,
	.get_mon_mpdu_end_wmask = NULL,
	.get_mon_msdu_end_wmask = NULL,
	.get_mon_ppdu_end_usr_stats_wmask = NULL,
};

void ath12k_wifi6_hal_mon_ops_init(struct ath12k_hal *hal,
				   u8 hw_version)
{
	switch (hw_version) {
	case ATH12K_HW_QCN9074_HW10:
		hal->hal_mon_ops = &hal_qcn9074_mon_ops;
		break;
	default:
		break;
	}
}
