/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef HAL_MON_CMN_H
#define HAL_MON_CMN_H

#include "hw.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ath12k_cmn_extn.h"
#endif /* CPTCFG_QCN_EXTN */

#define HAL_RX_MON_MAX_AGGR_SIZE	128
#define HAL_RX_MAX_MPDU				256
#define HAL_RX_NUM_WORDS_PER_PPDU_BITMAP	(HAL_RX_MAX_MPDU >> 5)
#define EHT_MAX_USER_INFO	4
#define UHR_MAX_USER_INFO	4
#define HAL_MAX_UL_MU_USERS	37
#define HAL_RX_MON_FCS_LEN	4

#define HAL_RX_MON_MPDU_ERR_FCS			BIT(0)
#define HAL_RX_MON_MPDU_ERR_DECRYPT		BIT(1)
#define HAL_RX_MON_MPDU_ERR_TKIP_MIC		BIT(2)
#define HAL_RX_MON_MPDU_ERR_AMSDU_ERR		BIT(3)
#define HAL_RX_MON_MPDU_ERR_OVERFLOW		BIT(4)
#define HAL_RX_MON_MPDU_ERR_MSDU_LEN		BIT(5)
#define HAL_RX_MON_MPDU_ERR_MPDU_LEN		BIT(6)
#define HAL_RX_MON_MPDU_ERR_UNENCRYPTED_FRAME	BIT(7)

#define HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VALID		BIT(30)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VER		BIT(31)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_NSS		GENMASK(2, 0)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_MCS		GENMASK(6, 3)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_LDPC		BIT(7)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_DCM		BIT(8)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_START	GENMASK(15, 9)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_SIZE		GENMASK(18, 16)

#define HE_GI_0_8 0
#define HE_GI_0_4 1
#define HE_GI_1_6 2
#define HE_GI_3_2 3

#define HE_LTF_1_X 0
#define HE_LTF_2_X 1
#define HE_LTF_4_X 2

#define HE_GI_RADIOTAP_0_8 0
#define HE_GI_RADIOTAP_1_6 1
#define HE_GI_RADIOTAP_3_2 2
#define HE_GI_RADIOTAP_RESERVED 3

#define HE_LTF_RADIOTAP_UNKNOWN 0
#define HE_LTF_RADIOTAP_1_X 1
#define HE_LTF_RADIOTAP_2_X 2
#define HE_LTF_RADIOTAP_4_X 3

#define MAX_RU_INDEX 0x7

#define BA_TS_BITMAP 0x10
#define BA_TS_OFFSET 0x3
#define BA_TS_LSB 0x20
#define BA_TS_BITMAP_SZ 0x4
#define HT_SGI_PRESENT 0x80
#define VHT_SIG_SU_NSS_MASK 0x7
#define VHT_SIG_SU_PARTIAL_AID_MASK 0x1FF
#define HE_NSTS_DATA6_MASK 0xF
#define HE_MU_NUM_USER_MASK 0xFFFE
#define HE_DATA5_INFO_MASK 0xFFF0
#define EHT_DISREGARD_MASK 0xF
#define EHT_USIG_VALIDATE_MASK 0x1
#define EHT_USIG_SU_MU_DISREGARD_MASK 0x1F
#define EHT_USIG_TB_DISREGARD_MASK 0x3F
#define UHR_SPATIAL_REUSE_MASK 0xF
#define UHR_SPATIAL_REUSE_SHFT 0x4
#define UHR_USER_INFO_BEAMFORMING_KNOWN 0x1
#define UHR_USER_INFO_CODING_KNOWN 0x2

enum hal_rx_vht_sig_a_gi_setting {
	HAL_RX_VHT_SIG_A_NORMAL_GI = 0,
	HAL_RX_VHT_SIG_A_SHORT_GI = 1,
	HAL_RX_VHT_SIG_A_SHORT_GI_AMBIGUITY = 3,
};

enum hal_rx_ul_reception_type {
	HAL_RECEPTION_TYPE_ULOFMDA,
	HAL_RECEPTION_TYPE_ULMIMO,
	HAL_RECEPTION_TYPE_OTHER,
	HAL_RECEPTION_TYPE_FRAMELESS
};

enum hal_he_ppdu_subtype {
	HE_SUBTYPE_SU = 0,
	HE_SUBTYPE_TRIG = 1,
	HE_SUBTYPE_MU = 2,
	HE_SUBTYPE_EXT_SU = 3,
};

enum hal_tx_coex_status_reason {
	COEX_FES_TX_START,
	COEX_FES_TX_END,
	COEX_FES_END,
	COEX_RESPONSE_TX_START,
	COEX_RESPONSE_TX_END,
	COEX_NO_TX_ONGOING
};

#define HAL_TLV_STATUS_PPDU_NOT_DONE	HAL_RX_MON_STATUS_PPDU_NOT_DONE
#define HAL_TLV_STATUS_PPDU_DONE	HAL_RX_MON_STATUS_PPDU_DONE
#define HAL_TLV_STATUS_BUF_DONE		HAL_RX_MON_STATUS_BUF_DONE

/* HE Radiotap data1 Mask */
#define HE_SU_FORMAT_TYPE 0x0000
#define HE_EXT_SU_FORMAT_TYPE 0x0001
#define HE_MU_FORMAT_TYPE  0x0002
#define HE_TRIG_FORMAT_TYPE  0x0003
#define HE_BEAM_CHANGE_KNOWN 0x0008
#define HE_DL_UL_KNOWN 0x0010
#define HE_MCS_KNOWN 0x0020
#define HE_DCM_KNOWN 0x0040
#define HE_CODING_KNOWN 0x0080
#define HE_LDPC_EXTRA_SYMBOL_KNOWN 0x0100
#define HE_STBC_KNOWN 0x0200
#define HE_DATA_BW_RU_KNOWN 0x4000
#define HE_DOPPLER_KNOWN 0x8000
#define HE_BSS_COLOR_KNOWN 0x0004

/* HE Radiotap data2 Mask */
#define HE_GI_KNOWN 0x0002
#define HE_TXBF_KNOWN 0x0010
#define HE_PE_DISAMBIGUITY_KNOWN 0x0020
#define HE_TXOP_KNOWN 0x0040
#define HE_LTF_SYMBOLS_KNOWN 0x0004
#define HE_PRE_FEC_PADDING_KNOWN 0x0008
#define HE_MIDABLE_PERIODICITY_KNOWN 0x0080
#define HE_RU_ALLOCATION_SHIFT 8

/* HE radiotap data3 shift values */
#define HE_BEAM_CHANGE_SHIFT 6
#define HE_DL_UL_SHIFT 7
#define HE_TRANSMIT_MCS_SHIFT 8
#define HE_DCM_SHIFT 12
#define HE_CODING_SHIFT 13
#define HE_LDPC_EXTRA_SYMBOL_SHIFT 14
#define HE_STBC_SHIFT 15

/* HE radiotap data4 shift values */
#define HE_STA_ID_SHIFT 4
#define HE_SIG_A_PUNC_BW_SHIFT 8

/* HE radiotap data5 */
#define HE_GI_SHIFT 4
#define HE_LTF_SIZE_SHIFT 6
#define HE_LTF_SYM_SHIFT 8
#define HE_TXBF_SHIFT 14
#define HE_PE_DISAMBIGUITY_SHIFT 15
#define HE_PRE_FEC_PAD_SHIFT 12

/* HE radiotap data6 */
#define HE_DOPPLER_SHIFT 4
#define HE_TXOP_SHIFT 8

/* HE radiotap HE-MU flags1 */
#define HE_SIG_B_MCS_KNOWN 0x0010
#define HE_SIG_B_DCM_KNOWN 0x0040
#define HE_SIG_B_SYM_NUM_KNOWN 0x8000
#define HE_RU_0_KNOWN 0x0100
#define HE_RU_1_KNOWN 0x0200
#define HE_RU_2_KNOWN 0x0400
#define HE_RU_3_KNOWN 0x0800
#define HE_DCM_FLAG_1_SHIFT 5
#define HE_SPATIAL_REUSE_MU_KNOWN 0x0100
#define HE_SIG_B_COMPRESSION_FLAG_1_KNOWN 0x4000
#define HE_CHANNEL_1_CENTER_26_RU_SHIFT 13

/* HE radiotap HE-MU flags2 */
#define HE_SIG_B_COMPRESSION_FLAG_2_SHIFT 3
#define HE_BW_KNOWN 0x0004
#define HE_NUM_SIG_B_SYMBOLS_SHIFT 4
#define HE_SIG_B_COMPRESSION_FLAG_2_KNOWN 0x0100
#define HE_NUM_SIG_B_FLAG_2_SHIFT 9
#define HE_LTF_FLAG_2_SYMBOLS_SHIFT 12
#define HE_LTF_KNOWN 0x8000
#define HE_CHANNEL_2_CENTER_26_RU_SHIFT 11

/* HE radiotap per_user_1 */
#define HE_STA_SPATIAL_SHIFT 11
#define HE_RESERVED_SET_TO_1_SHIFT 19
#define HE_STA_CODING_SHIFT 20

/* HE radiotap per_user_2 */
#define HE_STA_MCS_SHIFT 4
#define HE_STA_DCM_SHIFT 5

/* HE radiotap per user known */
#define HE_USER_FIELD_POSITION_KNOWN 0x01
#define HE_STA_ID_PER_USER_KNOWN 0x02
#define HE_STA_NSTS_KNOWN 0x04
#define HE_STA_TX_BF_KNOWN 0x08
#define HE_STA_SPATIAL_CONFIG_KNOWN 0x10
#define HE_STA_MCS_KNOWN 0x20
#define HE_STA_DCM_KNOWN 0x40
#define HE_STA_CODING_KNOWN 0x80

#define HAL_RX_MPDU_ERR_FCS			BIT(0)
#define HAL_RX_MPDU_ERR_DECRYPT			BIT(1)
#define HAL_RX_MPDU_ERR_TKIP_MIC		BIT(2)
#define HAL_RX_MPDU_ERR_AMSDU_ERR		BIT(3)
#define HAL_RX_MPDU_ERR_OVERFLOW		BIT(4)
#define HAL_RX_MPDU_ERR_MSDU_LEN		BIT(5)
#define HAL_RX_MPDU_ERR_MPDU_LEN		BIT(6)
#define HAL_RX_MPDU_ERR_UNENCRYPTED_FRAME	BIT(7)

/* EHT radiotap known values */
#define EHT_EHT_LTF_KNOWN			0x00000008

/* EHT radiotap shift values */
#define EHT_STA_ID_SHIFT			8
#define EHT_CODING_SHIFT			19
#define EHT_MCS_SHIFT				20
#define EHT_NSS_SHIFT				24
#define EHT_SPATIAL_CONFIG_SHIFT		24
#define EHT_BEAMFORMING_SHIFT			29
#define EHT_NUM_NON_OFDMA_USERS_SHIFT		17
#define EHT_SPATIAL_REUSE_SHIFT			3
#define EHT_GI_SHIFT				7
#define EHT_LTF_SHIFT				9
#define EHT_EHT_LTF_SHIFT			11
#define EHT_PRE_FEC_PADDING_FACTOR_SHIFT	15
#define EHT_PE_DISAMBIGUITY_SHIFT		17
#define EHT_DISREGARD_SHIFT			18
#define EHT_LDPC_EXTRA_SYMBOL_SEG_SHIFT		14
#define EHT_NUM_KNOWN_RU_ALLOCATIONS_SHIFT	10
#define EHT_RU_ALLOCATION1_1_SHIFT		13
#define EHT_RU_ALLOCATION1_2_SHIFT		22
#define EHT_RU_ALLOCATION2_1_SHIFT		0
#define EHT_RU_ALLOCATION2_2_SHIFT		9
#define EHT_RU_ALLOCATION2_3_SHIFT		18
#define EHT_RU_ALLOCATION2_4_SHIFT		0
#define EHT_RU_ALLOCATION2_5_SHIFT		9
#define EHT_RU_ALLOCATION2_6_SHIFT		18

/* U-SIG Common Mask */
#define USIG_PHY_VERSION_SHIFT			12
#define USIG_BW_SHIFT				15
#define USIG_UL_DL_SHIFT			18
#define USIG_BSS_COLOR_SHIFT			19
#define USIG_TXOP_SHIFT				25

/* U-SIG MU/TB Value */
#define USIG_DISREGARD_SHIFT			0
#define USIG_PPDU_TYPE_N_COMP_MODE_SHIFT	6
#define USIG_VALIDATE_SHIFT			8
#define USIG_MU_VALIDATE1_SHIFT			5
#define USIG_MU_PUNCTURE_CH_INFO_SHIFT		9
#define USIG_MU_VALIDATE2_SHIFT			14
#define USIG_MU_EHT_SIG_MCS_SHIFT		15
#define USIG_MU_NUM_EHT_SIG_SYM_SHIFT		17
#define USIG_TB_SPATIAL_REUSE_1_SHIFT		9
#define USIG_TB_SPATIAL_REUSE_2_SHIFT		13
#define USIG_TB_DISREGARD1_SHIFT		17
#define USIG_CRC_SHIFT				22
#define USIG_TAIL_SHIFT				26

/* U-SIG MU/TB Mask */
#define USIG_DISREGARD_KNOWN			0x00000001
#define USIG_PPDU_TYPE_N_COMP_MODE_KNOWN	0x00000004
#define USIG_VALIDATE_KNOWN			0x00000008
#define USIG_MU_VALIDATE1_KNOWN			0x00000002
#define USIG_MU_PUNCTURE_CH_INFO_KNOWN		0x00000010
#define USIG_MU_VALIDATE2_KNOWN			0x00000020
#define USIG_MU_EHT_SIG_MCS_KNOWN		0x00000040
#define USIG_MU_NUM_EHT_SIG_SYM_KNOWN		0x00000080
#define USIG_TB_SPATIAL_REUSE_1_KNOWN		0x00000010
#define USIG_TB_SPATIAL_REUSE_2_KNOWN		0x00000020
#define USIG_TB_DISREGARD1_KNOWN		0x00000040
#define USIG_CRC_KNOWN				0x00000100
#define USIG_TAIL_KNOWN				0x00000200

enum hal_eht_bw {
	HAL_EHT_BW_20,
	HAL_EHT_BW_40,
	HAL_EHT_BW_80,
	HAL_EHT_BW_160,
	HAL_EHT_BW_320_1,
	HAL_EHT_BW_320_2,
};

enum hal_mon_reception_type {
	HAL_RECEPTION_TYPE_SU,
	HAL_RECEPTION_TYPE_DL_MU_MIMO,
	HAL_RECEPTION_TYPE_DL_MU_OFMA,
	HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO,
	HAL_RECEPTION_TYPE_UL_MU_MIMO,
	HAL_RECEPTION_TYPE_UL_MU_OFDMA,
	HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO,
};

/* Rate Table */
#define HAL_11B_RATE_0MCS (11 * 2)
#define HAL_11B_RATE_1MCS (5.5 * 2)
#define HAL_11B_RATE_2MCS (2 * 2)
#define HAL_11B_RATE_3MCS (1 * 2)
#define HAL_11B_RATE_4MCS HAL_11B_RATE_0MCS
#define HAL_11B_RATE_5MCS HAL_11B_RATE_1MCS
#define HAL_11B_RATE_6MCS HAL_11B_RATE_2MCS

#define HAL_11A_RATE_0MCS (48 * 2)
#define HAL_11A_RATE_1MCS (24 * 2)
#define HAL_11A_RATE_2MCS (12 * 2)
#define HAL_11A_RATE_3MCS (6 * 2)
#define HAL_11A_RATE_4MCS (54 * 2)
#define HAL_11A_RATE_5MCS (36 * 2)
#define HAL_11A_RATE_6MCS (18 * 2)
#define HAL_11A_RATE_7MCS (9 * 2)

#define HAL_LEGACY_MCS0 0
#define HAL_LEGACY_MCS1 1
#define HAL_LEGACY_MCS2 2
#define HAL_LEGACY_MCS3 3
#define HAL_LEGACY_MCS4 4
#define HAL_LEGACY_MCS5 5
#define HAL_LEGACY_MCS6 6
#define HAL_LEGACY_MCS7 7

/* Different allowed RU in 11BE */
#define HAL_EHT_RU_26		0ULL
#define HAL_EHT_RU_52		1ULL
#define HAL_EHT_RU_78		2ULL
#define HAL_EHT_RU_106		3ULL
#define HAL_EHT_RU_132		4ULL
#define HAL_EHT_RU_242		5ULL
#define HAL_EHT_RU_484		6ULL
#define HAL_EHT_RU_726		7ULL
#define HAL_EHT_RU_996		8ULL
#define HAL_EHT_RU_996x2	9ULL
#define HAL_EHT_RU_996x3	10ULL
#define HAL_EHT_RU_996x4	11ULL
#define HAL_EHT_RU_NONE		15ULL
#define HAL_EHT_RU_INVALID	31ULL
/* MRUs spanning above 80Mhz
 * HAL_EHT_RU_996_484 = HAL_EHT_RU_484 + HAL_EHT_RU_996 + 4 (reserved)
 */
#define HAL_EHT_RU_996_484	18ULL
#define HAL_EHT_RU_996x2_484	28ULL
#define HAL_EHT_RU_996x3_484	40ULL
#define HAL_EHT_RU_996_484_242	23ULL

#define NUM_RU_BITS_PER80	16
#define NUM_RU_BITS_PER20	4

/* Different per_80Mhz band in 320Mhz bandwidth */
#define HAL_80_0	0
#define HAL_80_1	1
#define HAL_80_2	2
#define HAL_80_3	3

#define HAL_RU_80MHZ(num_band)		((num_band) * NUM_RU_BITS_PER80)
#define HAL_RU_20MHZ(idx_per_80)	((idx_per_80) * NUM_RU_BITS_PER20)

#define HAL_RU_SHIFT(num_band, idx_per_80)	\
		(HAL_RU_80MHZ(num_band) + HAL_RU_20MHZ(idx_per_80))

#define HAL_RU(ru, num_band, idx_per_80)	\
		((u64)(ru) << HAL_RU_SHIFT(num_band, idx_per_80))

/* MRU-996+484 */
#define HAL_EHT_RU_996_484_0	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 1) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_1, 0))
#define HAL_EHT_RU_996_484_1	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_1, 0))
#define HAL_EHT_RU_996_484_2	(HAL_RU(HAL_EHT_RU_996, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1))
#define HAL_EHT_RU_996_484_3	(HAL_RU(HAL_EHT_RU_996, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0))
#define HAL_EHT_RU_996_484_4	(HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_3, 0))
#define HAL_EHT_RU_996_484_5	(HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_3, 0))
#define HAL_EHT_RU_996_484_6	(HAL_RU(HAL_EHT_RU_996, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 1))
#define HAL_EHT_RU_996_484_7	(HAL_RU(HAL_EHT_RU_996, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 0))

/* MRU-996x2+484 */
#define HAL_EHT_RU_996x2_484_0	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_1	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_2	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_3	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_4	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1))
#define HAL_EHT_RU_996x2_484_5	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_6	(HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_7	(HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_8	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_9	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_10	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 1))
#define HAL_EHT_RU_996x2_484_11	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 0))

/* MRU-996x3+484 */
#define HAL_EHT_RU_996x3_484_0	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_1	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_2	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_3	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_4	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_5	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_6	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 1))
#define HAL_EHT_RU_996x3_484_7	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 0))

#define HAL_RU_PER80(ru_per80, num_80mhz, ru_idx_per80mhz) \
			(HAL_RU(ru_per80, num_80mhz, ru_idx_per80mhz))


#define ATH12K_LE32_DEC_ENC(value, dec_bits, enc_bits)	\
		u32_encode_bits(le32_get_bits(value, dec_bits), enc_bits)

#define ATH12K_LE64_DEC_ENC(value, dec_bits, enc_bits) \
		u64_encode_bits(le64_get_bits(value, dec_bits), enc_bits)

struct ath12k_mon_data;
struct ath12k_mon_ring_desc_info;

#define HAL_TLV_64_HDR_TAG		GENMASK(9, 1)
#define HAL_TLV_64_HDR_TAG_CMN		GENMASK(9, 0)
#define HAL_TLV_64_HDR_LEN		GENMASK(21, 10)
#define HAL_TLV_64_USR_ID		GENMASK(31, 26)

#define HAL_TLV_64_HDR_TAG_SHIFT	1	/* WiFi7: tag is bits[9:1] */
#define HAL_TLV_64_HDR_TAG_NOSHIFT	0	/* WiFi8: tag is bits[9:0] */

enum hal_rx_mpdu_filter_category {
	HAL_RX_MPDU_FILTER_CATEGORY_FP,
	HAL_RX_MPDU_FILTER_CATEGORY_MD,
	HAL_RX_MPDU_FILTER_CATEGORY_MO,
	HAL_RX_MPDU_FILTER_CATEGORY_FP_MO
};

struct hal_rx_u_sig_info {
	u8 phy_version;
	bool ul_dl;
	u8 bw;
	u8 ppdu_type_comp_mode;
	u8 eht_sig_mcs;
	u8 num_eht_sig_sym;
	u16 sta_id;
	u8 cosr_cobf_disable;
	struct ieee80211_radiotap_eht_usig usig;
	struct ieee80211_radiotap_uhr_usig uhr_usig;
};

struct hal_rx_tlv_aggr_info {
	bool in_progress;
	u16 cur_len;
	u16 tlv_tag;
	u8 buf[HAL_RX_MON_MAX_AGGR_SIZE];
};

struct hal_rx_uhr_info {
	u8 num_user_info;
	struct hal_rx_radiotap_uhr uhr;
	__le32 user_info[UHR_MAX_USER_INFO];
	__le32 user_known[UHR_MAX_USER_INFO];
};

struct hal_rx_uhr_elr_info {
	__le32 known;
	__le32 sig1;
	__le32 sig2;
	__le32 mark;
};

struct hal_rx_user_status {
	u32 mcs:5,
	    nss:3,
	    ofdma_info_valid:1,
	    ul_ofdma_ru_start_index:7,
	    ul_ofdma_ru_width:7,
	    ul_ofdma_ru_size:8,
	    is_ampdu:1;
	u32 ul_ofdma_user_v0_word0;
	u32 ul_ofdma_user_v0_word1;
	u16 ast_index; // End User Stat
	u16 sw_peer_id;// mpdu start
	u16 tid;
	u16 tcp_msdu_count;
	u16 tcp_ack_msdu_count;
	u16 udp_msdu_count;
	u16 other_msdu_count;
	u16 frame_control;
	u8 frame_control_info_valid:1,
	data_sequence_control_info_valid:1;
	u16 first_data_seq_ctrl;
	u8 preamble_type;
	u16 duration;
	u16 ht_flags;
	u16 vht_flags;
	u16 he_flags;
	u8  vht_flag_values2;
	u8  vht_flag_values3[4];
	u8  vht_flag_values4;
	u8  vht_flag_values5;
	u16 vht_flag_values6;
	u16 he_flags1;
	u16 he_flags2;
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data4;
	u16 he_data5;
	u16 he_data6;
	u32 eht_user_info;
	u8  he_RU[8];
	u8 rs_flags;
	u8 ldpc;
	u16 mpdu_cnt_fcs_ok;
	u16 mpdu_cnt_fcs_err;
	u32 mpdu_ok_byte_count;
	u32 mpdu_err_byte_count;
	bool ampdu_present;
	u16 ampdu_id;
	u32 errmap;
	u32 mpdu_retry;
	u8 filter_category;
	u8 enc_type;
	u16 retried_msdu_count;
	u16 start_seq;
	u16 ba_control;
	u32 ba_bitmap[32];
	u16 ba_bitmap_sz;
	u16 aid;
	u8 peer_mac[ETH_ALEN];
};

struct hal_rx_eht_info {
	u8 num_user_info;
	struct hal_rx_radiotap_eht eht;
	u32 user_info[EHT_MAX_USER_INFO];
};

struct hal_rx_mon_mpdu_info {
	u8  decap_type:3,
	    mpdu_start_received:1,
	    first_rx_hdr_rcvd:1,
	    rx_hdr_rcvd:1,
	    raw_mpdu:1,
	    truncated:1;
	u32 err_bitmap;
	bool mpdu_end_received;
	bool full_pkt;
	bool fcs_err;
};

struct hal_rx_nrp_info {
	uint32_t fc_valid : 1,
		 frame_control : 16,
		 to_ds_flag : 1,
		 mac_addr2_valid : 1,
		 mcast_bcast : 1;
	uint8_t mac_addr2[ETH_ALEN];
};

struct hal_rx_mon_msdu_info {
	u32 first_buffer:1,
	    last_buffer:1,
	    cce_metadata:16;
	u32 fse_metadata;
	u32 flow_idx;
	u8 msdu_index;
};

struct hal_rx_user_ctrl_frm_info {
	 uint8_t bar : 1,
		 ndpa : 1;
};

/**
 * struct hal_rx_ppdu_cfr_info - CFR metadata extracted from PPDU TLVs
 * @bb_captured_channel: Channel capture was triggered and freeze TLV was sent
 * @bb_captured_timeout: Capture condition met, but freeze TLV timed out
 * @bb_captured_reason: Capture trigger reason from RXPCU PPDU END TLV
 * @rx_location_info_valid: PHYRX_LOCATION RTT/CFR DMA pointer validity
 * @chan_capture_status: Capture status reported by ucode
 * @rtt_che_buffer_pointer_high8: High 8 bits of 40-bit RTT CHE buffer pointer
 * @rtt_che_buffer_pointer_low32: Low 32 bits of 40-bit RTT CHE buffer pointer
 * @rtt_cfo_measurement: Signed raw CFO value from PHYRX_LOCATION TLV
 * @agc_gain_info0: Chain0/1 AGC gain information
 * @agc_gain_info1: Chain2/3 AGC gain information
 * @agc_gain_info2: Reserved for higher chains
 * @agc_gain_info3: Reserved for higher chains
 * @rx_start_ts: Rx start timestamp from PHYRX_LOCATION TLV
 * @mcs_rate: MCS/rate indication from PHYRX_LOCATION TLV
 * @gi_type: Guard interval indication from PHYRX_LOCATION TLV
 */
struct hal_rx_ppdu_cfr_info {
	bool bb_captured_channel;
	bool bb_captured_timeout;
	u8 bb_captured_reason;
	bool rx_location_info_valid;
	u8 chan_capture_status;
	u8 rtt_che_buffer_pointer_high8;
	u32 rtt_che_buffer_pointer_low32;
	s16 rtt_cfo_measurement;
	u32 agc_gain_info0;
	u32 agc_gain_info1;
	u32 agc_gain_info2;
	u32 agc_gain_info3;
	u32 rx_start_ts;
	u32 mcs_rate;
	u32 gi_type;
};

struct hal_rx_mon_ppdu_info {
	u16 ppdu_id;
	u16 last_ppdu_id;
	u64 ppdu_ts;
	u16 num_mpdu_fcs_ok;
	u16 num_mpdu_fcs_err;
	u32 mpdu_len;
	u16 chan_num;
	u16 freq;
	u16 tcp_msdu_count;
	u16 tcp_ack_msdu_count;
	u16 udp_msdu_count;
	u16 other_msdu_count;
	u16 peer_id;
	u8 rate;
	u8 vht_flag_values1;
	u8 vht_flag_values2;
	u8 vht_flag_values3[4];
	u8 vht_flag_values4;
	u8 vht_flag_values5;
	u16 vht_flag_values6;
	u8 gi;
	u8 rssi_comb;
	u8 rssi_chain_pri20[HAL_RX_MAX_NSS];
	u16 tid;
	u8 fc_valid;
	u32 ht_flags : 1,
	    vht_flags : 1,
	    he_flags : 1,
	    he_mu_flags : 1,
	    usig_flags : 1,
	    eht_flags : 1,
	    mcs : 5,
	    nss : 3,
	    bw : 4,
	    is_stbc : 1,
	    sgi : 2,
	    he_re : 1,
	    ldpc : 1,
	    beamformed : 1,
	    dcm : 1,
	    preamble_type : 4,
	    reserved : 3;
	u8 ru_alloc;
	u8 reception_type;
	u64 tsft;
	u64 rx_duration;
	u16 frame_control;
	u16 ast_index;
	u8  rtap_flags;
	u8 rs_fcs_err;
	u8 rs_flags;
	u8 cck_flag;
	u8 ofdm_flag;
	u8 ulofdma_flag;
	u8 frame_control_info_valid;
	u16 he_per_user_1;
	u16 he_per_user_2;
	u8 he_per_user_position;
	u8 he_per_user_known;
	u16 he_flags1;
	u16 he_flags2;
	u8 he_RU[4];
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data4;
	u16 he_data5;
	u16 he_data6;
	u32 l_sig_a_info;
	u32 l_sig_b_info;
	u32 usig_common;
	u32 usig_value;
	u32 usig_mask;
	u8  ht_mcs;
	u32 eht_known;
	u32 eht_data[9];
	u8  num_eht_user_info_valid;
	u32 ppdu_len;
	u16 prev_ppdu_id;
	u32 device_id;
	u16 first_data_seq_ctrl;
	u8 monitor_direct_used;
	u8 data_sequence_control_info_valid;
	u8 ltf_size;
	u8 rxpcu_filter_pass;
	s8 rssi_chain[8][8];
	u8 num_users;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	u8 addr4[ETH_ALEN];
	struct hal_rx_user_status userstats[HAL_MAX_UL_MU_USERS];
	bool first_msdu_in_mpdu;
	bool is_ampdu;
	u8 medium_prot_type;
	bool ppdu_continuation;
	bool eht_usig;
	bool uhr_usig;
	u8 usr_nss_sum;
	u16 usr_ru_tones_sum;
	struct hal_rx_u_sig_info u_sig_info;
	bool is_eht;
	bool is_uhr;
	bool is_uhr_elr;
	struct hal_rx_eht_info eht_info;
	struct hal_rx_uhr_info uhr_info;
	struct hal_rx_uhr_elr_info elr_info;
	struct hal_rx_tlv_aggr_info tlv_aggr;
	struct hal_rx_nrp_info nrp_info;
	u32 errmap;
	u32 mpdu_retry;
	u8 grp_id;
	u8 decap_format;
	u16 mpdu_retry_cnt;
	struct hal_rx_mon_mpdu_info mpdu_info[HAL_MAX_UL_MU_USERS];
	struct sk_buff_head mpdu_q[HAL_MAX_UL_MU_USERS];
	bool is_drop_tlv;
	struct hal_rx_mon_msdu_info msdu_info[HAL_MAX_UL_MU_USERS];
	u8 user_id;
	u16 retried_msdu_count;
	s8 rssi_region_offset;
	u16 punctured_pattern;
	u16 punc_bw;
	struct hal_rx_user_ctrl_frm_info ctrl_frm_info[HAL_MAX_UL_MU_USERS];
	struct hal_rx_ppdu_cfr_info cfr_info;
#ifdef CPTCFG_QCN_EXTN
	struct hal_mon_ppdu_info_extn ppdu_info_extn;
#endif /* CPTCFG_QCN_EXTN */
	u8 ampdu_flag;
	u8 band;
	u32 rx_antenna;
	u8 num_non_ofdma_users;
	u16 vendor_tlv_len;
};

#define HAL_RX_MON_OFFSET(block, field) block##_##field##_OFFSET
#define HAL_RX_MON_LSB(block, field) block##_##field##_LSB
#define HAL_RX_MON_MASK(block, field) block##_##field##_MASK

static inline u32 hal_rx_mon_get(const void *ptr, u32 offset, u32 mask, u8 lsb)
{
	const u32 *reg;

	reg = (const u32 *)ptr + (offset >> 2);
	return ((*reg) & mask) >> lsb;
}

#define HAL_RX_MON_UPDATE_PRI20_RSSI_PER_CHAIN(chain, ppdu_info, rssi_info_tlv) \
	( \
		(ppdu_info)->rssi_chain_pri20[(chain)] = \
			hal_rx_mon_get((rssi_info_tlv), \
				       HAL_RX_MON_OFFSET(RECEIVE_RSSI_INFO, \
							 RSSI_PRI20_CHAIN##chain), \
				       HAL_RX_MON_MASK(RECEIVE_RSSI_INFO, \
						       RSSI_PRI20_CHAIN##chain), \
				       HAL_RX_MON_LSB(RECEIVE_RSSI_INFO, \
						      RSSI_PRI20_CHAIN##chain)) \
	)

/* in the bitmap 0 indicates no puncturing and 1 indicate that sub channel is punctured */
#define PUNCTURE_NONE    0x0000
#define PUNCTURE_INVALID 0xFFFF
#define PUNCTURE_80MHZ_MASK 0xF
#define PUNCTURE_160MHZ_MASK 0xFF
#define PUNCTURE_320MHZ_MASK 0xFFFF
#define PUNCTURE_40MHZ_MASK 0x3

/*
 * ieee80211_punc_type:
 * Type of puncturing denoting the number of bits that are punctured.
 * Each bit represents a 20MHz channel and therefore, each enum represents
 * the number of 20MHz channels that are punctured.
 */
enum ieee80211_punc_type {
	IEEE80211_PUNC_NONE        = 0,
	IEEE80211_PUNC_MINUS20MHZ  = 1,
	IEEE80211_PUNC_MINUS40MHZ  = 2,
	IEEE80211_PUNC_MINUS60MHZ  = 3,
	IEEE80211_PUNC_MINUS80MHZ  = 4,
	IEEE80211_PUNC_MINUS100MHZ = 5,
	IEEE80211_PUNC_MINUS120MHZ = 6,
	IEEE80211_PUNC_INVALID,
};

struct hal_rx_mon_status_tlv_hdr {
	u32 hdr;
	u8 value[];
};

enum hal_rx_mon_status {
	HAL_RX_MON_STATUS_PPDU_NOT_DONE,
	HAL_RX_MON_STATUS_PPDU_DONE,
	HAL_RX_MON_STATUS_BUF_DONE,
	HAL_RX_MON_STATUS_BUF_ADDR,
	HAL_RX_MON_STATUS_MPDU_START,
	HAL_RX_MON_STATUS_MPDU_END,
	HAL_RX_MON_STATUS_MSDU_END,
	HAL_RX_MON_STATUS_RX_HDR,
	HAL_RX_MON_STATUS_DROP_TLV,
};

enum hal_tx_mon_status {
	HAL_TX_MON_STATUS_PPDU_NOT_DONE,
	HAL_TX_MON_MPDU_START,
	HAL_TX_MON_MPDU_END,
	HAL_TX_MON_FES_SETUP,
	HAL_TX_MON_FES_STATUS_END,
	HAL_TX_MON_PEER_ENTRY,
	HAL_TX_MON_QUEUE_EXTENSION,
	HAL_TX_MON_RESPONSE_REQUIRED_INFO,
	HAL_TX_MON_FES_STATUS_START,
	HAL_TX_MON_FES_STATUS_PROT,
	HAL_TX_MON_FES_STATUS_START_PPDU,
	HAL_TX_MON_FES_STATUS_START_PROT,
	HAL_TX_MON_FES_STATUS_USER_PPDU,
	HAL_TX_MON_FES_STATUS_ACK_OR_BA,
	HAL_TX_MON_FRAME_BITMAP_ACK,
	HAL_TX_MON_FRAME_BITMAP_BLOCK_ACK_1K,
	HAL_TX_MON_COEX_TX_STATUS,
	HAL_TX_MON_MSDU_START,
	HAL_TX_MON_MSDU_END,
	HAL_TX_MON_RESPONSE_END_STATUS_INFO,
	HAL_TX_MON_PCU_PPDU_SETUP_INIT,
	HAL_TX_MON_MACTX_HE_SIG_A_SU,
	HAL_TX_MON_MACTX_HE_SIG_A_MU_DL,
	HAL_TX_MON_MACTX_HE_SIG_B1_MU,
	HAL_TX_MON_MACTX_HE_SIG_B2_MU,
	HAL_TX_MON_MACTX_HE_SIG_B2_OFDMA,
	HAL_TX_MON_MACTX_VHT_SIG,
	HAL_TX_MON_MACTX_L_SIG_A,
	HAL_TX_MON_MACTX_L_SIG_B,
	HAL_TX_MON_MACTX_HT_SIG,
	HAL_TX_MON_MACTX_PHY_DESC,
	HAL_TX_MON_BUFFER_ADDR,
	HAL_TX_MON_DATA,
	HAL_TX_MON_FW2SW,
};

/**
 * struct hal_tx_mon_packet_info - packet info
 * @sw_cookie: 64-bit SW desc virtual address
 * @dma_length: packet DMA length
 * @msdu_continuation: msdu continulation in next buffer
 * @truncated: packet is truncated
 */
struct hal_tx_mon_packet_info {
	u64 sw_cookie;
	u32 dma_length : 16,
	    msdu_continuation : 1,
	    truncated : 1,
	    reserved : 14;
};

/**
 * struct hal_tx_mon_ppdu_info - tx monitor ppdu information
 * @ppdu_id:  Id of the PLCP protocol data unit
 * @num_users: number of users
 * @cur_usr_idx: Current user index of the PPDU
 * @ack_recvd: boolean flag to indicate if ack is received
 * @cts_recvd: boolean flag to indicate if cts is received
 * @su_or_mu: type of transmission used like su, mu, mu_su transmission
 * @mu_type: mu transmission information
 * @reserved: for future purpose
 * @prot_tlv_status: protection tlv status
 * @ack_rssi: rssi of received ack. Valid only if ack_recvd is set
 * @ba_user_id: block ack user id. keeps track for ba payload build
 * @packet_info: packet information
 * @rx_status: monitor mode rx status information
 * @rx_user_status: per user status
 */
struct hal_tx_mon_ppdu_info {
	u32 ppdu_id;
	u32 num_users      :8,
	    is_used        :1,
	    is_data        :1,
	    cur_usr_idx    :8,
	    ack_recvd      :1,
	    cts_recvd      :1,
	    su_or_mu       :2,
	    mu_type        :1,
	    is_sw_filter_done :1,
	    reserved       :8;
	u32 prot_tlv_status;
	u8  ack_rssi;
	u8  ba_user_id;
	struct hal_tx_mon_packet_info packet_info;
	struct hal_rx_mon_ppdu_info rx_status;
};

/**
 * struct hal_txmon_word_mask_config - hal tx monitor word mask filter setting
 * Add more members to this structure, if extended in upcoming h/ws
 * @pcu_ppdu_setup_init: PCU_PPDU_SETUP TLV word mask
 * @tx_peer_entry: TX_PEER_ENTRY TLV word mask
 * @tx_queue_ext: TX_QUEUE_EXTENSION TLV word mask
 * @tx_fes_status_end: TX_FES_STATUS_END TLV word mask
 * @response_end_status: RESPONSE_END_STATUS TLV word mask
 * @tx_fes_status_prot: TX_FES_STATUS_PROT TLV word mask
 * @tx_fes_setup: TX_FES_SETUP TLV word mask
 * @tx_msdu_start: TX_MSDU_START TLV word mask
 * @tx_mpdu_start: TX_MPDU_START TLV word mask
 * @rxpcu_user_setup: RXPCU_USER_SETUP TLV word mask
 * @compaction_enable: flag to enable word mask compaction
 */
struct hal_tx_mon_wmask_config {
	u32 pcu_ppdu_setup_init;
	u16 tx_peer_entry;
	u16 tx_queue_ext;
	u16 tx_fes_status_end;
	u16 response_end_status;
	u16 tx_fes_status_prot;
	u8 tx_fes_setup;
	u8 tx_msdu_start;
	u8 tx_mpdu_start;
	u8 rxpcu_user_setup;
	u8 compaction_enable;
	/* WiFi8 (Boron) word-mask additions — zero on WiFi7 */
	u8 phytx_pkt_end;
	u8 mactx_user_desc_common;
	u8 mactx_mu_ul_user_setup;
	u8 pdg_response;
	u8 rcvd_resp_info;
	u8 rx_resp_required_info;
	u8 ranging_user_details;
	u8 mactx_pre_phy_desc;
	u16 ofdma_trigger_details;
	u16 tqm_upd_tx_mpdu_cnt_sts;
	u16 phytx_location;
	u32 rcvd_resp_info_part2;
};

/**
 * struct hal_tx_mon_status_info - status info that wasn't populated in rx_status
 * @transmission_type: su or mu transmission type
 * @medium_prot_type: medium protection type
 * @generated_response: Generated frame in response window
 * @band_center_freq1:
 * @band_center_freq2:
 * @freq:
 * @phy_mode:
 * @schedule_id:
 * @no_bitmap_avail: Bitmap available flag
 * @explicit_ack: Explicit Acknowledge flag
 * @explicit_ack_type: Explicit Acknowledge type
 * @response_type: Response type in response window
 * @ndp_frame: NDP frame
 * @reserved: reserved bits
 * @mba_count: MBA count
 * @mba_fake_bitmap_count: MBA fake bitmap count
 * @sw_frame_group_id: software frame group ID
 * @r2r_to_follow: Response to Response follow flag
 * @phy_abort_reason: Reason for PHY abort
 * @phy_abort_user_number: User number for PHY abort
 * @protection_addr: Protection Address flag
 * @buffer: Packet buffer pointer address
 * @offset: Packet buffer offset
 * @length: Packet buffer length
 * @addr1: MAC address 1
 * @addr2: MAC address 2
 * @addr3: MAC address 3
 * @addr4: MAC address 4
 * @dp_tx_pkt_cap_cookie: cookie counter
 */
struct hal_tx_mon_status_info {
	u8  transmission_type;
	u8  medium_prot_type;
	u8  generated_response;
	u16 band_center_freq1;
	u16 band_center_freq2;
	u16 freq;
	u16 phy_mode;
	u32 schedule_id;
	u32 no_bitmap_avail :1,
	    explicit_ack : 1,
	    explicit_ack_type : 4,
	    r2r_end_status_follow :1,
	    response_type : 5,
	    ndp_frame : 2,
	    num_users :8,
	    reserved : 10;
	u8  mba_count;
	u8  mba_fake_bitmap_count;
	u8  sw_frame_group_id;
	u32 r2r_to_follow;
	u16 phy_abort_reason;
	u8  phy_abort_user_number;
	u8  protection_addr;
	void *buffer;
	u32 offset;
	u32 length;
	u8  addr1[ETH_ALEN];
	u8  addr2[ETH_ALEN];
	u8  addr3[ETH_ALEN];
	u8  addr4[ETH_ALEN];
	u8  dp_tx_pkt_cap_cookie;
	u8  spc_pkt_id;
};

struct hal_mon_tx_usig_cmn {
	u32 phy_version : 3,
	    bw : 3,
	    ul_dl : 1,
	    bss_color : 6,
	    txop : 7,
	    disregard : 5,
	    validate_0 : 1,
	    reserved : 6;
};

struct hal_mon_tx_usig_tb {
	u32 ppdu_type_comp_mode : 2,
	    validate_1 : 1,
	    spatial_reuse_1 : 4,
	    spatial_reuse_2 : 4,
	    disregard_1 : 5,
	    crc : 4,
	    tail : 6,
	    rx_integrity_check_passed : 1;
};

struct hal_mon_tx_usig_mu {
	u32 ppdu_type_comp_mode : 2,
	    validate_1 : 1,
	    punc_ch_info : 5,
	    validate_2 : 1,
	    eht_sig_mcs : 2,
	    num_eht_sig_sym : 5,
	    crc : 4,
	    tail : 6,
	    rx_integrity_check_passed : 1;
};

/**
 * struct hal_mon_tx_usig_hdr: U-SIG header for EHT (and subsequent) frames
 * @usig_1: USIG common header fields
 * @usig_2: USIG version dependent fields
 * @tb: trigger based frame USIG header
 * @mu: MU frame USIG header
 */
struct hal_mon_tx_usig_hdr {
	struct hal_mon_tx_usig_cmn usig_1;
	union {
		struct hal_mon_tx_usig_tb tb;
		struct hal_mon_tx_usig_mu mu;
	} usig_2;
};

struct hal_mon_tx_eht_sig_mu_mimo_user_info {
	u32 sta_id : 11,
	    mcs : 4,
	    coding : 1,
	    spatial_coding : 6,
	    crc : 4;
};

struct hal_mon_tx_eht_sig_non_mu_mimo_user_info {
	u32 sta_id : 11,
	    mcs : 4,
	    validate : 1,
	    nss : 4,
	    beamformed : 1,
	    coding : 1,
	    crc : 4;
};

/**
 * union hal_mon_tx_eht_sig_user_field - User field in EHTSIG
 * @mu_mimo_usr: MU-MIMO user field information in EHTSIG
 * @non_mu_mimo_usr: Non MU-MIMO user field information in EHTSIG
 */
union hal_mon_tx_eht_sig_user_field {
	struct hal_mon_tx_eht_sig_mu_mimo_user_info mu_mimo_user;
	struct hal_mon_tx_eht_sig_non_mu_mimo_user_info non_mu_mimo_user;
};

/**
 * struct hal_mon_tx_user_desc_per_user - user desc per user information
 * @psdu_length: PSDU length of the user in octet
 * @ru_start_index: RU number to which user is assigned
 * @ru_size: Size of the RU for that user
 * @ofdma_mu_mimo_enabled: mu mimo transmission within the RU
 * @nss: Number of spatial stream occupied by the user
 * @stream_offset: Stream Offset from which the User occupies the Streams
 * @mcs: Modulation Coding Scheme for the User
 * @dcm: Indicates whether dual sub-carrier modulation is applied
 * @fec_type: Indicates whether it is BCC or LDPC
 * @user_bf_type: user beamforming type
 * @drop_user_cbf: frame dropped because of CBF FCS failure
 * @ldpc_extra_symbol: LDPC encoding process
 * @force_extra_symbol: force an extra OFDM symbol
 * @reserved: reserved
 * @sw_peer_id: user sw peer id
 * @per_user_subband_mask: Per user sub band mask
 */

struct hal_mon_tx_user_desc_per_user {
	u32 psdu_length;
	u32 ru_start_index         :8,
	    ru_size                :4,
	    ofdma_mu_mimo_enabled  :1,
	    nss                    :3,
	    stream_offset          :3,
	    mcs                    :4,
	    dcm                    :1,
	    fec_type               :1,
	    user_bf_type           :2,
	    drop_user_cbf          :1,
	    ldpc_extra_symbol      :1,
	    force_extra_symbol     :1,
	    reserved               :2;
	u32 sw_peer_id             :16,
	    per_user_subband_mask  :16;
};

/**
 * struct hal_mon_tx_usr_desc_common - user desc common information
 * @num_users: Number of users
 * @ltf_size: LTF size
 * @pkt_extn_pe: packet extension duration of the trigger-based PPDU
 * @a_factor: packet extension duration of the trigger-based PPDU
 * @center_ru_0: Center RU is occupied in the lower 80 MHz band
 * @center_ru_1: Center RU is occupied in the upper 80 MHz band
 * @num_ltf_symbols: number of LTF symbols
 * @doppler_indication: doppler indication
 * @reserved: reserved
 * @spatial_reuse: spatial reuse
 * @gi: guard interval
 * @ru_channel_0: RU arrangement for band 0
 * @ru_channel_1: RU arrangement for band 1
 */

struct hal_mon_tx_usr_desc_common {
	u32 num_users              :6,
	    ltf_size               :2,
	    pkt_extn_pe            :1,
	    a_factor               :2,
	    center_ru_0            :1,
	    center_ru_1            :1,
	    num_ltf_symbols        :16,
	    doppler_indication     :1,
	    reserved               :2;
	u16 spatial_reuse;
	u8  gi;
	u16 ru_channel_0[8];
	u16 ru_channel_1[8];
};

enum mon_tx_fw2sw_user_id {
	HAL_MON_TX_FW2SW_TYPE_FES_SETUP      = 0,
	HAL_MON_TX_FW2SW_TYPE_FES_SETUP_USER = 1,
	HAL_MON_TX_FW2SW_TYPE_FES_SETUP_EXT  = 2,
	HAL_MON_TX_FW2SW_TYPE_MAX            = 4
};

static inline u64 ath12k_hal_le32hilo_to_u64(__le32 hi, __le32 lo)
{
	u64 hi64 = le32_to_cpu(hi);
	u64 lo64 = le32_to_cpu(lo);

	return (hi64 << 32) | lo64;
}

static const u8
ru_alloc_offset[HAL_MAX_UL_MU_USERS][MAX_RU_INDEX] = {
	{0, 0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0, 0},
	{3, 1, 0, 0, 0, 0, 0},
	{4, 0, 0, 0, 0, 0, 0},
	{5, 2, 1, 0, 0, 0, 0},
	{6, 2, 1, 0, 0, 0, 0},
	{7, 3, 1, 0, 0, 0, 0},
	{8, 3, 1, 0, 0, 0, 0},
	{9, 4, 2, 1, 0, 0, 0},
	{10, 4, 2, 1, 0, 0, 0},
	{11, 5, 2, 1, 0, 0, 0},
	{12, 5, 2, 1, 0, 0, 0},
	{13, 0, 0, 1, 0, 0, 0},
	{14, 6, 3, 1, 0, 0, 0},
	{15, 6, 3, 1, 0, 0, 0},
	{16, 7, 3, 1, 0, 0, 0},
	{17, 7, 3, 1, 0, 0, 0},
	{18, 0, 0, 0, 0, 0, 0},
	{19, 8, 4, 2, 1, 0, 0},
	{20, 8, 4, 2, 1, 0, 0},
	{21, 9, 4, 2, 1, 0, 0},
	{22, 9, 4, 2, 1, 0, 0},
	{23, 0, 0, 2, 1, 0, 0},
	{24, 10, 5, 2, 1, 0, 0},
	{25, 10, 5, 2, 1, 0, 0},
	{26, 11, 5, 2, 1, 0, 0},
	{27, 11, 5, 2, 1, 0, 0},
	{28, 12, 6, 3, 1, 0, 0},
	{29, 12, 6, 3, 1, 0, 0},
	{30, 13, 6, 3, 1, 0, 0},
	{31, 13, 6, 3, 1, 0, 0},
	{32, 0, 0, 3, 1, 0, 0},
	{33, 14, 7, 3, 1, 0, 0},
	{34, 14, 7, 3, 1, 0, 0},
	{35, 15, 7, 3, 1, 0, 0},
	{36, 15, 7, 3, 1, 0, 0},
};

struct hal_mon_ops {
	enum hal_tx_mon_status
	(*tx_parse_status_tlv)(struct ath12k_hal *hal,
			       struct hal_tx_mon_ppdu_info *ppdu_info,
			       struct hal_tx_mon_status_info *data_status_info,
			       struct hal_tx_mon_status_info *prot_status_info,
			       bool is_prot_ppdu,
			       u16 tlv_tag,
			       const void *tlv_data,
			       u32 userid,
			       u16 tlv_len,
			       u8 *status_frag);
	enum hal_tx_mon_status
	(*tx_status_get_num_user)(struct ath12k_hal *hal,
				  u16 tlv_tag,
				  const void *tlv,
				  u8 *num_users,
				  u16 tlv_len);
	u32 (*get_mon_mpdu_start_wmask)(void);
	u32 (*get_mon_mpdu_end_wmask)(void);
	u32 (*get_mon_msdu_end_wmask)(void);
	u32 (*get_mon_ppdu_end_usr_stats_wmask)(void);
	void (*rx_mpdu_start_info_get)(const void *tlv_data, u32 userid,
				       struct hal_rx_mon_ppdu_info *info,
				       u32 tlv_len);
	void (*rx_msdu_end_info_get)(const void *tlv_data, u32 userid,
				     struct hal_rx_mon_ppdu_info *info,
				     u32 tlv_len);
	void (*rx_ppdu_eu_stats_info_get)(const void *tlv_data, u32 userid,
					  struct hal_rx_mon_ppdu_info *info,
					  u32 tlv_len);
	u8* (*rx_desc_get_msdu_payload)(void *desc);
	struct dp_mon_tx_ppdu_info *
	(*hal_mon_tx_ppdu_info)(struct ath12k_hal *hal,
				struct ath12k_mon_data *pmon,
				u16 tlv_tag);
	void (*hal_mon_set_mon_buf_desc)(void *desc, u32 addr_lo,
					 u32 addr_hi, u64 cookie);
	void (*get_tx_mon_wmask_config)(struct hal_tx_mon_wmask_config *wmsk);
	void (*tx_fes_setup_info_get)(const void *tlv_data, u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      u16 tlv_len);
	void (*tx_peer_entry_info_get)(const void *tlv_data, u32 userid,
				       struct hal_tx_mon_ppdu_info *info,
				       struct hal_tx_mon_status_info *tx_status_info,
				       u16 tlv_len);
	void (*tx_queue_ext_info_get)(const void *tlv_data, u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      u16 tlv_len);
	void (*tx_mpdu_start_info_get)(const void *tlv_data, u32 userid,
				       struct hal_tx_mon_ppdu_info *info,
				       u16 tlv_len);
	void (*tx_fes_status_info_get)(const void *tlv_data, u32 userid,
				       struct hal_tx_mon_ppdu_info *info,
				       struct hal_tx_mon_status_info *tx_status_info,
				       u16 tlv_len);
	void (*tx_response_end_status_info_get)(const void *tlv_data, u32 userid,
						struct hal_tx_mon_ppdu_info *info,
						struct hal_tx_mon_status_info *status,
						u16 tlv_len);
	void (*tx_fes_status_prot_info_get)(const void *tlv_data, u32 userid,
					    struct hal_tx_mon_ppdu_info *info,
					    u16 tlv_len);
	void (*tx_pcu_ppdu_setup_init_info_get)(const void *tlv_data,
						struct hal_tx_mon_status_info *status,
						u16 tlv_len);
	int (*extract_tx_mon_ring_desc)(struct ath12k_hal *hal,
					void *ring_entry,
					struct ath12k_mon_ring_desc_info *desc_info);
	bool (*is_mon_buf_addr_tlv)(u32 tlv_tag);
	bool (*tx_mon_pkt_buf_cnt_in_desc)(void);
};

static inline u16
ath12k_hal_get_tlv_hdr_tag(const struct ath12k_hal *hal, __le64 tl)
{
	return le64_get_bits(tl, HAL_TLV_64_HDR_TAG_CMN) >> hal->tlv_hdr_tag_shift;
}

static inline struct dp_mon_tx_ppdu_info *
ath12k_hal_mon_tx_ppdu_info(struct ath12k_hal *hal,
			      struct ath12k_mon_data *mon_data,
			      u16 tlv_tag)
{
	return hal->hal_mon_ops->hal_mon_tx_ppdu_info(hal, mon_data, tlv_tag);
}

static inline enum hal_tx_mon_status
ath12k_hal_mon_tx_parse_status(struct ath12k_hal *hal,
			       struct hal_tx_mon_ppdu_info *ppdu_info,
			       struct hal_tx_mon_status_info *data_status_info,
			       struct hal_tx_mon_status_info *prot_status_info,
			       bool is_prot_ppdu,
			       u16 tlv_tag, const void *tlv_data,
			       u32 userid, u16 tlv_len,
			       u8 *status_frag)
{
	return hal->hal_mon_ops->tx_parse_status_tlv(hal, ppdu_info,
						     data_status_info,
						     prot_status_info,
						     is_prot_ppdu,
						     tlv_tag, tlv_data,
						     userid, tlv_len,
						     status_frag);
}

static inline enum hal_tx_mon_status
ath12k_hal_mon_tx_status_get_num_user(struct ath12k_hal *hal,
				      u16 tlv_tag,
				      const void *tlv,
				      u8 *num_users,
				      u16 tlv_len)
{
	return hal->hal_mon_ops->tx_status_get_num_user(hal,
							tlv_tag,
							tlv,
							num_users,
							tlv_len);
}

static inline u32 ath12k_hal_mon_rx_mpdu_start_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_mpdu_start_wmask)
		return hal->hal_mon_ops->get_mon_mpdu_start_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_mpdu_end_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_mpdu_end_wmask)
		return hal->hal_mon_ops->get_mon_mpdu_end_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_msdu_end_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_msdu_end_wmask)
		return hal->hal_mon_ops->get_mon_msdu_end_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_ppdu_end_usr_stats_wmask)
		return hal->hal_mon_ops->get_mon_ppdu_end_usr_stats_wmask();

	return 0;
}

static inline
void ath12k_hal_mon_rx_mpdu_start_info_get(struct ath12k_hal *hal,
					   const void *tlv,
					   u32 userid,
					   struct hal_rx_mon_ppdu_info *info,
					   u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_mpdu_start_info_get)
		hal->hal_mon_ops->rx_mpdu_start_info_get(tlv,
							 userid,
							 info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_rx_msdu_end_info_get(struct ath12k_hal *hal,
				    const void *tlv,
				    u32 userid,
				    struct hal_rx_mon_ppdu_info *info,
				    u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_msdu_end_info_get)
		hal->hal_mon_ops->rx_msdu_end_info_get(tlv,
						       userid,
						       info,
						       tlv_len);
}

static inline void
ath12k_hal_mon_rx_ppdu_end_usr_stats_info_get(struct ath12k_hal *hal,
					      const void *tlv,
					      u32 userid,
					      struct hal_rx_mon_ppdu_info *info,
					      u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_ppdu_eu_stats_info_get)
		hal->hal_mon_ops->rx_ppdu_eu_stats_info_get(tlv,
							    userid,
							    info,
							    tlv_len);
}

static inline u8*
ath12k_hal_mon_rx_desc_get_msdu_payload(struct ath12k_hal *hal,
					void *rx_desc)
{
	if (hal->hal_mon_ops->rx_desc_get_msdu_payload)
		return hal->hal_mon_ops->rx_desc_get_msdu_payload(rx_desc);

	return NULL;
}

static inline void ath12k_hal_mon_set_mon_buf_desc(struct ath12k_hal *hal,
						   void *desc, u32 addr_lo,
						   u32 addr_hi, u64 cookie)
{
	return hal->hal_mon_ops->hal_mon_set_mon_buf_desc(desc, addr_lo,
							  addr_hi, cookie);
}

static inline void
ath12k_hal_mon_tx_fes_setup_info_get(struct ath12k_hal *hal,
				     const void *tlv,
				     u32 userid,
				     struct hal_tx_mon_ppdu_info *info,
				     u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_fes_setup_info_get)
		hal->hal_mon_ops->tx_fes_setup_info_get(tlv, userid,
							info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_peer_entry_info_get(struct ath12k_hal *hal,
				      const void *tlv,
				      u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      struct hal_tx_mon_status_info *tx_status_info,
				      u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_peer_entry_info_get)
		hal->hal_mon_ops->tx_peer_entry_info_get(tlv, userid,
							 info, tx_status_info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_tx_queue_ext_info_get(struct ath12k_hal *hal,
				     const void *tlv,
				     u32 userid,
				     struct hal_tx_mon_ppdu_info *info,
				     u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_queue_ext_info_get)
		hal->hal_mon_ops->tx_queue_ext_info_get(tlv, userid,
							info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_mpdu_start_info_get(struct ath12k_hal *hal,
				      const void *tlv,
				      u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_mpdu_start_info_get)
		hal->hal_mon_ops->tx_mpdu_start_info_get(tlv, userid,
							 info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_fes_status_end_info_get(struct ath12k_hal *hal,
					  const void *tlv,
					  u32 userid,
					  struct hal_tx_mon_ppdu_info *info,
					  struct hal_tx_mon_status_info *tx_status_info,
					  u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_fes_status_info_get)
		hal->hal_mon_ops->tx_fes_status_info_get(tlv, userid,
							 info, tx_status_info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_tx_response_end_status_info_get(struct ath12k_hal *hal,
					       const void *tlv,
					       u32 userid,
					       struct hal_tx_mon_ppdu_info *info,
					       struct hal_tx_mon_status_info *status_info,
					       u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_response_end_status_info_get)
		hal->hal_mon_ops->tx_response_end_status_info_get(tlv, userid,
								  info, status_info,
								  tlv_len);
}

static inline void
ath12k_hal_mon_tx_fes_status_prot_info_get(struct ath12k_hal *hal,
					   const void *tlv,
					   u32 userid,
					   struct hal_tx_mon_ppdu_info *info,
					   u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_fes_status_prot_info_get)
		hal->hal_mon_ops->tx_fes_status_prot_info_get(tlv, userid,
							      info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_pcu_ppdu_setup_init_info_get(struct ath12k_hal *hal,
					       const void *tlv,
					       struct hal_tx_mon_status_info *status_info,
					       u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_pcu_ppdu_setup_init_info_get)
		hal->hal_mon_ops->tx_pcu_ppdu_setup_init_info_get(tlv,
								  status_info,
								  tlv_len);
}

static __always_inline void
hal_get_radiotap_gi_ltf(u16 *gi, u16 *ltf)
{
	switch (*gi) {
	case HE_GI_0_8:
		*gi = HE_GI_RADIOTAP_0_8;
		break;
	case HE_GI_1_6:
		*gi = HE_GI_RADIOTAP_1_6;
		break;
	case HE_GI_3_2:
		*gi = HE_GI_RADIOTAP_3_2;
		break;
	default:
		*gi = HE_GI_RADIOTAP_RESERVED;
	}

	switch (*ltf) {
	case HE_LTF_1_X:
		*ltf = HE_LTF_RADIOTAP_1_X;
		break;
	case HE_LTF_2_X:
		*ltf = HE_LTF_RADIOTAP_2_X;
		break;
	case HE_LTF_4_X:
		*ltf = HE_LTF_RADIOTAP_4_X;
		break;
	default:
		*ltf = HE_LTF_RADIOTAP_UNKNOWN;
	}
}

static inline void
ath12k_hal_mon_tx_get_wmask_config(struct ath12k_hal *hal,
				   struct hal_tx_mon_wmask_config *wmsk)
{
	if (hal->hal_mon_ops->get_tx_mon_wmask_config)
		hal->hal_mon_ops->get_tx_mon_wmask_config(wmsk);
}

static inline int
ath12k_hal_mon_extract_tx_mon_ring_desc(struct ath12k_hal *hal,
					void *ring_entry,
					struct ath12k_mon_ring_desc_info *desc_info)
{
	if (hal->hal_mon_ops->extract_tx_mon_ring_desc)
		return hal->hal_mon_ops->extract_tx_mon_ring_desc(hal,
								  ring_entry,
								  desc_info);
	return 0;
}

static __always_inline void
ath12k_hal_mon_get_mac_addr(u32 addr_32, u16 addr_16, u8 *addr,
			    bool use_l32_h16)
{
	if (use_l32_h16) {
		memcpy(addr, &addr_32, 4);
		memcpy(addr + 4, &addr_16, ETH_ALEN - 4);
	} else {
		memcpy(addr, &addr_16, 2);
		memcpy(addr + 2, &addr_32, ETH_ALEN - 2);
	}
}

static inline bool
ath12k_hal_is_mon_buf_addr_tlv(struct ath12k_hal *hal, u32 tlv_tag)
{
	if (hal->hal_mon_ops->is_mon_buf_addr_tlv)
		return hal->hal_mon_ops->is_mon_buf_addr_tlv(tlv_tag);

	return false;
}

static inline bool
ath12k_hal_mon_tx_pkt_buf_cnt_in_desc(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->tx_mon_pkt_buf_cnt_in_desc)
		return hal->hal_mon_ops->tx_mon_pkt_buf_cnt_in_desc();

	return false;
}
#endif
