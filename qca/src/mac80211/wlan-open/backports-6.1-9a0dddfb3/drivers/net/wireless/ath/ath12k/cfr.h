/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_CFR_H
#define ATH12K_CFR_H

#include <linux/timer.h>
#include <linux/notifier.h>
#include "dbring.h"
#include "wmi.h"

#define ATH12K_CFR_NUM_RESP_PER_EVENT   1
#define ATH12K_CFR_EVENT_TIMEOUT_MS     1
#define ATH12K_CFR_LUT_AGE_TIMER        3000

#define ATH12K_CORRELATE_TX_EVENT 1
#define ATH12K_CORRELATE_DBR_EVENT 0

#define ATH12K_MAX_CFR_ENABLED_CLIENTS 10

#define ATH12K_CFR_START_MAGIC 0xDEADBEAF
#define ATH12K_CFR_END_MAGIC 0xBEAFDEAD

#define ATH12K_CFR_RADIO_QCN9274 32
#define ATH12K_CFR_RADIO_WCN7850 33
#define ATH12K_CFR_RADIO_IPQ5332 35
#define ATH12K_CFR_RADIO_QCN6432 38
#define ATH12K_CFR_RADIO_IPQ5424 42
#define ATH12K_CFR_RADIO_QCN9625 43

#define CFR_HDR_MAX_LEN_WORDS_QCN9274 90
#define CFR_DATA_MAX_LEN_QCN9274 64512

#define CFR_HDR_MAX_LEN_WORDS_WCN7850 16
#define CFR_DATA_MAX_LEN_WCN7850 16064

#define CFR_HDR_MAX_LEN_WORDS_IPQ5332 24
#define CFR_DATA_MAX_LEN_IPQ5332 8192

#define CFR_HDR_MAX_LEN_WORDS_QCN6432 32
#define CFR_DATA_MAX_LEN_QCN6432 32152

#define CFR_HDR_MAX_LEN_WORDS_IPQ5424 24
#define CFR_DATA_MAX_LEN_IPQ5424 15744

/* locsens_common_header_t (40B) + cc_upload_header_struct_t base fields (24B)
 * + freeze_capture_tlv (32B) + user_cfr_11az_info[37] (296B)
 * + aoa_cal_gdp_info_t (24B, includes reserved0 -- confirm with ucode team)
 * = 416 bytes = 104 words
 */
#define CFR_HDR_MAX_LEN_WORDS_QCN9625 104
/* 1024 tones * 4 bytes/tone * 5 chains * 4ss (or 512 tones * 8ss, same total) */
#define CFR_DATA_MAX_LEN_QCN9625 81920

#define VENDOR_QCA 0x8cfdf0
#define NUM_CHAINS_FW_TO_HOST(n) ((1 << ((n) + 1)) - 1)

enum ath12k_cfr_meta_version {
	ATH12K_CFR_META_VERSION_NONE,
	ATH12K_CFR_META_VERSION_1,
	ATH12K_CFR_META_VERSION_2,
	ATH12K_CFR_META_VERSION_3,
	ATH12K_CFR_META_VERSION_4,
	ATH12K_CFR_META_VERSION_5,
	ATH12K_CFR_META_VERSION_6,
	ATH12K_CFR_META_VERSION_7,
	ATH12K_CFR_META_VERSION_8,
	ATH12K_CFR_META_VERSION_9,
	ATH12K_CFR_META_VERSION_10,
	ATH12K_CFR_META_VERSION_11,
	ATH12K_CFR_META_VERSION_MAX = 0xFF,
};

enum ath12k_cfr_correlate_status {
	ATH12K_CORRELATE_STATUS_RELEASE,
	ATH12K_CORRELATE_STATUS_HOLD,
	ATH12K_CORRELATE_STATUS_ERR,
};

struct ath12k_cfr_peer_tx_param {
	u32 capture_method;
	u32 vdev_id;
	u8 peer_mac_addr[ETH_ALEN];
	u32 primary_20mhz_chan;
	u32 bandwidth;
	u32 phy_mode;
	u32 band_center_freq1;
	u32 band_center_freq2;
	u32 spatial_streams;
	u32 correlation_info_1;
	u32 correlation_info_2;
	u32 status;
	u32 timestamp_us;
	u32 counter;
	u32 chain_rssi[WMI_MAX_CHAINS];
	u16 chain_phase[WMI_MAX_CHAINS];
	u32 cfo_measurement;
	u8 agc_gain[WMI_MAX_CHAINS];
	u32 rx_start_ts;
	u32 rx_ts_reset;
	uint32_t mcs_rate;
	uint32_t gi_type;
	uint8_t agc_gain_tbl_index[WMI_MAX_CHAINS];
};

#define HOST_MAX_CHAINS 8

/*
 * @status: capture status/histogram tracking
 * @tx_pkt_bw: bandwidth of the transmitted packet that triggered this capture
 * @phy_mode: PHY mode of the captured packet
 * @center_freq1: primary center frequency of the capture
 * @center_freq2: secondary center frequency of the capture (160/80+80 MHz)
 *
 * @num_mu_users: 0 for an SU capture. Non-zero indicates an MU (RCC)
 * capture and gives the user count. Current metadata layout carries only
 * su_peer_addr; multi-user peer-address arrays are not part of this struct
 *
 * @su_peer_addr: peer MAC address for an SU capture
 *
 * @chain_rssi: per-chain RSSI. Not available from ucode's DMA header on
 * either wifi7 or wifi8 -- sourced from the WMI TX capture event
 * (ath12k_cfr_peer_tx_param.chain_rssi), so kept here rather than dropped
 *
 * @chain_phase: per-chain phase. Same as chain_rssi -- no ucode DMA-header
 * equivalent, kept
 *
 * @cfo_measurement: pending FW/ucode alignment, kept for now
 *
 * @agc_gain: per-chain AGC gain. Same as chain_rssi -- no ucode DMA-header
 * equivalent, kept
 *
 * @rx_start_ts: pending FW/ucode alignment, kept for now
 *
 * @mcs_rate: same as chain_rssi -- no ucode DMA-header equivalent, kept
 *
 * @gi_type: pending FW/ucode alignment, kept for now
 *
 * @beamformed: whether the captured packet was beamformed
 *
 * @agc_gain_tbl_index: per-chain AGC gain table index. Same as
 * chain_rssi -- no ucode DMA-header equivalent, kept
 *
 * @puncture_bitmap: puncture pattern of the capture bandwidth
 */
struct cfr_enh_metadata {
	u8 status;
	u8 tx_pkt_bw;
	u8 phy_mode;
	u16 center_freq1;
	u16 center_freq2;
	u8 num_mu_users;
	u8 su_peer_addr[ETH_ALEN];
	u32 chain_rssi[HOST_MAX_CHAINS];
	u16 chain_phase[HOST_MAX_CHAINS];
	u32 cfo_measurement;
	u8 agc_gain[HOST_MAX_CHAINS];
	u32 rx_start_ts;
	u16 mcs_rate;
	u16 gi_type;
	u8 beamformed;
	u8 agc_gain_tbl_index[HOST_MAX_CHAINS];
	u16 puncture_bitmap;
} __packed;

struct ath12k_csi_cfr_header {
	u32 start_magic_num;
	u32 vendorid;
	u8 cfr_metadata_version;
	u8 chip_type;
	u32 cfr_metadata_len;
	u64 host_real_ts;
	struct cfr_enh_metadata meta_enh;
} __packed;

enum ath12k_cfr_preamble_type {
	ATH12K_CFR_PREAMBLE_TYPE_LEGACY,
	ATH12K_CFR_PREAMBLE_TYPE_HT,
	ATH12K_CFR_PREAMBLE_TYPE_VHT,
};

#define TONES_IN_20MHZ  256
#define TONES_IN_40MHZ  512
#define TONES_IN_80MHZ  1024
#define TONES_IN_160MHZ 2048 /* 160 MHz isn't supported yet */
#define TONES_INVALID   0

#define CFIR_DMA_HDR_INFO0_TAG GENMASK(7, 0)
#define CFIR_DMA_HDR_INFO0_LEN GENMASK(13, 8)

#define CFIR_DMA_HDR_INFO1_UPLOAD_DONE	GENMASK(0, 0)
#define CFIR_DMA_HDR_INFO1_CAPTURE_TYPE	GENMASK(3, 1)
#define CFIR_DMA_HDR_INFO1_PREABLE_TYPE	GENMASK(5, 4)
#define CFIR_DMA_HDR_INFO1_NSS		GENMASK(8, 6)
#define CFIR_DMA_HDR_INFO1_NUM_CHAINS	GENMASK(11, 9)
#define CFIR_DMA_HDR_INFO1_UPLOAD_PKT_BW GENMASK(14, 12)
#define CFIR_DMA_HDR_INFO1_SW_PEER_ID_VALID GENMASK(15, 15)

struct ath12k_cfir_dma_hdr {
	u16 info0;
	u16 info1;
	u16 sw_peer_id;
	u16 phy_ppdu_id;
};

#define CFIR_DMA_HDR_INFO2_HDR_VER GENMASK(3, 0)
#define CFIR_DMA_HDR_INFO2_TARGET_ID GENMASK(7, 4)
#define CFIR_DMA_HDR_INFO2_CFR_FMT BIT(8)
#define CFIR_DMA_HDR_INFO2_RSVD BIT(9)
#define CFIR_DMA_HDR_INFO2_MURX_DATA_INC BIT(10)
#define CFIR_DMA_HDR_INFO2_FREEZ_DATA_INC BIT(11)
#define CFIR_DMA_HDR_INFO2_FREEZ_TLV_VER GENMASK(15, 12)

#define CFIR_DMA_HDR_INFO3_MU_RX_NUM_USERS GENMASK(7, 0)
#define CFIR_DMA_HDR_INFO3_DECIMATION_FACT GENMASK(11, 8)
#define CFIR_DMA_HDR_INFO3_RSVD GENMASK(15, 12)

/*
 * @tag: ucode fills this with 0xBA
 *
 * @length: length of CFR header in words (32-bit)
 *
 * @upload_done: ucode sets this to 1 to indicate DMA completion
 *
 * @capture_type:
 *
 *                      0 - None
 *                      1 - RTT-H (Nss = 1, Nrx)
 *                      2 - Debug-H (Nss, Nrx)
 *                      3 - Reserved
 *                      5 - RTT-H + CIR(Nss, Nrx)
 *
 * @preamble_type:
 *
 *                      0 - Legacy
 *                      1 - HT
 *                      2 - VHT
 *                      3 - HE
 *
 * @nss:
 *
 *                      0 - 1-stream
 *                      1 - 2-stream
 *                      ..      ..
 *                      7 - 8-stream
 *
 *@num_chains:
 *
 *                      0 - 1-chain
 *                      1 - 2-chain
 *                      ..  ..
 *                      7 - 8-chain
 *
 *@upload_bw_pkt:
 *
 *                      0 - 20 MHz
 *                      1 - 40 MHz
 *                      2 - 80 MHz
 *                      3 - 160 MHz
 *
 * @sw_peer_id_valid: Indicates whether sw_peer_id field is valid or not,
 * sent from MAC to PHY via the MACRX_FREEZE_CAPTURE_CHANNEL TLV
 *
 * @sw_peer_id: Indicates peer id based on AST search, sent from MAC to PHY
 * via the MACRX_FREEZE_CAPTURE_CHANNEL TLV
 *
 * @phy_ppdu_id: sent from PHY to MAC, copied to MACRX_FREEZE_CAPTURE_CHANNEL
 * TLV
 *
 * @total_bytes: Total size of CFR payload (FFT bins)
 *
 * @header_version:
 *
 *                      1 - IPQ87XX
 *                      2 - IPQ6018
 *                      3 - 11BE chipsets
 *
 * @target_id:
 * @cfr_fmt:
 *
 *                      0 - raw (32-bit format)
 *                      1 - compressed (24-bit format)
 *
 * @mu_rx_data_incl: Indicates whether CFR header contains UL-MU-MIMO info
 *
 * @freeze_data_incl: Indicates whether CFR header contains
 * MACRX_FREEZE_CAPTURE_CHANNEL TLV
 *
 * @freeze_tlv_version: Indicates the version of freeze_tlv
 *                      1 - IPQ87xx, IPQ6018
 *                      2 - IPQ5018
 *                      3 - IPQ9000
 *                      5 - 11BE chipsets
 *
 * @decimation_factor: FFT bins decimation
 * @mu_rx_num_users: Number of users in UL-MU-PPDU
 * @he_ltf_type:
 * @ext_preamble_type:
 * @amplitude_gain_ratio_0_3:
 * @rescale_amt_shift_pri80:
 * @rescale_amt_shift_sec80:
 * @cgim_status:
 * @cgim_filter:
 * @phy_mode:
 * @demf_turbo_mode:
 * @demf_pbs_en:
 * @leg_cfr_mode:
 * @puncture_pattern:
 * @pri20_location:
 * @channel_bandwidth:
 * @_11az_mode:
 * @_11az_node:
 */
struct ath12k_cfir_enh_dma_hdr {
	u16 tag              :  8,
	    length           :  6,
	    rsvd1            :  2;
	u16 upload_done        :  1,
	    capture_type       :  3,
	    preamble_type      :  2,
	    nss                :  3,
	    num_chains         :  3,
	    upload_pkt_bw      :  3,
	    sw_peer_id_valid   :  1;
	u16 sw_peer_id         : 16;
	u16 phy_ppdu_id        : 16;
	u16 total_bytes;
	u16 header_version     :4,
	    target_id          :4,
	    cfr_fmt            :1,
	    cir_fmt            :1,
	    mu_rx_data_incl    :1,
	    freeze_data_incl   :1,
	    freeze_tlv_version :4;
	u16 mu_rx_num_users    :8,
	    decimation_factor  :4,
	    he_ltf_type        :4;
	u16 ext_preamble_type  :1,
	    rsvd2              :15;
	u32 amplitude_gain_ratio_0_3;
	u16 rescale_amt_shift_pri80    : 8,
	    rescale_amt_shift_sec80    : 8;
	u16 cgim_status        : 1,
	    cgim_filter        : 1,
	    phy_mode           : 1,
	    demf_turbo_mode    : 1,
	    demf_pbs_en        : 2,
	    leg_cfr_mode       : 2,
	    puncture_pattern   : 8;
	u16 pri20_location     : 8,
	    channel_bandwidth  : 3,
	    _11az_mode         : 4,
	    _11az_node         : 1;
	u16 rsvd3;
	u16 rsvd4;
	u16 rsvd5;
};

#define CFR_MAX_LUT_ENTRIES 136

struct macrx_freeze_capture_channel_v5 {
	u16 freeze                          :  1, //[0]
            capture_reason                  :  3, //[3:1]
            packet_type                     :  2, //[5:4]
            packet_sub_type                 :  4, //[9:6]
            reserved                        :  5, //[14:10]
            sw_peer_id_valid                :  1; //[15]
        u16 sw_peer_id; //[15:0]
        u16 phy_ppdu_id; //[15:0]
        u16 packet_ta_lower_16; //[15:0]
        u16 packet_ta_mid_16; //[15:0]
        u16 packet_ta_upper_16; //[15:0]
        u16 packet_ra_lower_16; //[15:0]
        u16 packet_ra_mid_16; //[15:0]
        u16 packet_ra_upper_16; //[15:0]
        u16 tsf_timestamp_15_0; //[15:0]
        u16 tsf_timestamp_31_16; //[15:0]
        u16 tsf_timestamp_47_32; //[15:0]
        u16 tsf_timestamp_63_48; //[15:0]
        u16 user_index_or_user_mask_5_0     :  6, //[5:0]
            directed                        :  1, //[6]
            reserved_13                     :  9; //[15:7]
        u16 user_mask_21_6; //[15:0]
        u16 user_mask_36_22                 : 15, //[14:0]
            reserved_15a                    :  1; //[15]
};

struct uplink_user_setup_info_v2 {
        u32 bw_info_valid                   :  1, //[0]
            uplink_receive_type             :  2, //[2:1]
            reserved_0a                     :  1, //[3]
            uplink_11ax_mcs                 :  4, //[7:4]
            nss                             :  3, //[10:8]
            stream_offset                   :  3, //[13:11]
            sta_dcm                         :  1, //[14]
            sta_coding                      :  1, //[15]
            ru_type_80_0                    :  4, //[19:16]
            ru_type_80_1                    :  4, //[23:20]
            ru_type_80_2                    :  4, //[27:24]
            ru_type_80_3                    :  4; //[31:28]
        u32 ru_start_index_80_0             :  6, //[5:0]
            reserved_1a                     :  2, //[7:6]
            ru_start_index_80_1             :  6, //[13:8]
            reserved_1b                     :  2, //[15:14]
            ru_start_index_80_2             :  6, //[21:16]
            reserved_1c                     :  2, //[23:22]
            ru_start_index_80_3             :  6, //[29:24]
            reserved_1d                     :  2; //[31-30]
};

/*
 * wifi8 (QCN9625) CFR upload common header.
 * Distinct layout from ath12k_cfir_enh_dma_hdr (wifi7) starting at byte 0 --
 * not an extension of it, so it is parsed by a separate function
 * rather than shared bitfields.
 *
 * @header_tag: ucode fills this with 0xC0DE00BA
 *
 * @chip_id: chip identifier, per locsens upload header (CHIP_ID_IN_LNS_UPLOAD_HEADER)
 *
 * @header_type: feature type this header describes, per ucode's
 * locsens_upload_feature_header_type_e:
 *
 *			0 - HEADER_INVALID
 *			1 - HEADER_CFR_CHANNEL_COEFF_CIR
 *			2 - HEADER_11AZ_11BK_CIR
 *			3 - HEADER_11AZ_11BK_DEMF_INTEGRITY
 *			4 - HEADER_11AZ_11BK_ZGI_INTEGRITY
 *			5 - HEADER_LOCATION_INFO
 *			6 - HEADER_11BF
 *			7 - HEADER_RTT_SELF_CAL
 *			8 - HEADER_WIFI_RADAR
 *
 * host CFR processing only expects HEADER_CFR_CHANNEL_COEFF_CIR (1)
 *
 * @header_version: ucode header version for the feature header that
 * follows this common header (4 for CFR/CIR per ucode's cc_upload table)
 *
 * @header_size: size of common header + feature header, in BYTES.
 * Unlike ath12k_cfir_enh_dma_hdr.length, this is already byte-granular --
 * do not multiply by 4
 *
 * @payload_size: CFR payload length in bytes (32-bit), unlike
 * ath12k_cfir_enh_dma_hdr.total_bytes which is a 16-bit field
 *
 * @sw_peer_id_valid: Indicates whether sw_peer_id field is valid or not,
 * sent from MAC to PHY via the MACRX_FREEZE_CAPTURE_CHANNEL TLV
 *
 * @sw_peer_id: Indicates peer id based on AST search, sent from MAC to PHY
 * via the MACRX_FREEZE_CAPTURE_CHANNEL TLV
 *
 * @phy_ppdu_id: sent from PHY to MAC, copied to MACRX_FREEZE_CAPTURE_CHANNEL
 * TLV
 *
 * @num_chains: absolute chain count (1 = 1-chain, 5 = 5-chain), unlike
 * ath12k_cfir_enh_dma_hdr.num_chains which is 0-indexed and needs
 * NUM_CHAINS_FW_TO_HOST() to convert
 *
 * @nss: number of spatial streams, ONE-INDEXED (1 = 1-stream), per ucode's
 * locsens_common_header_t::reset() which explicitly sets nss = 1 as the
 * default ("one indexed" per ucode's own comment). Unlike
 * ath12k_cfir_enh_dma_hdr.nss which is 0-indexed (0 = 1-stream) -- do NOT
 * add 1 when deriving sts_count from this field
 *
 * @channel_bw: operating channel bandwidth
 * @packet_bw: bandwidth of the captured packet
 *
 * @preamble_type: preamble type of the captured packet
 * @ltf_type: LTF type used for the capture
 * @gi_type: guard interval type used for the capture
 * @phy_mode: PHY mode of the captured packet
 * @rf_chain_mask: RF chain mask active during capture
 * @sounding_dialog_token: dialog token from the sounding exchange, if any
 * @pri20_location: location of the primary 20 MHz within the capture bandwidth
 * @xbar_config: crossbar configuration used to route chains for this capture
 *
 * @reserved_0: reserved, ignore
 * @reserved_1: reserved, ignore
 * @reserved_2: reserved, ignore
 * @reserved_3: reserved, ignore
 */
struct ath12k_cfir_wifi8_common_hdr {
	u32 header_tag;
	u32 chip_id                :  8,
	    header_type            :  8,
	    common_header_version  :  4,
	    feature_header_version :  4,
	    header_size            :  8;
	u32 payload_size;
	u32 reserved_0          : 15,
	    sw_peer_id_valid    :  1,
	    sw_peer_id          : 16;
	u16 phy_ppdu_id;
	u32 num_chains          :  8,
	    nss                 :  8,
	    channel_bw          :  8,
	    packet_bw           :  8;
	u16 preamble_type       :  8,
	    ltf_type            :  4,
	    gi_type             :  4;
	u16 phy_mode            :  2,
	    rf_chain_mask       :  8,
	    reserved_1          :  6;
	u16 sounding_dialog_token : 8,
	    pri20_location        : 8;
	u32 xbar_config;
	u32 reserved_2;
	u32 reserved_3;
} __packed;

struct ath12k_per_user_cfr_11az_info_t {
	u32 sw_peer_id    : 16,
	    stream_offset : 3,
	    reserved1a    : 13;
	struct uplink_user_setup_info_v2 user_info;
} __packed;

struct ath12k_aoa_cal_gdp {
	u32 combined_ch0;
	u32 combined_ch1;
	u32 combined_ch2;
	u32 combined_ch3;
	u32 combined_ch4;
	u32 reserved0;
} __packed;

/*
 * wifi8 CFR feature-specific header (cc_upload_header_struct_t, CFR/CIR
 * portion only). freeze_capture_tlv, per-user info, and aoa_cal_gdp_info
 * follow this struct at freeze_tlv_offset / per_user_info_offset /
 * aoa_cal_gdp_offset.
 *
 * @capture_type: type of capture, per ucode's cc_upload_header_struct_t:
 *
 *			0 - none
 *			1 - RTT-H
 *			2 - Chan-H
 *			3 - reserved
 *			4 - CCK or CIR
 *			5,6,7 - 11bf
 *			8 - AoA cal
 *
 * @cc_format: 0 - raw (32-bit format), 1 - compressed (24-bit format)
 * @cir_fmt: 0 - legacy (1ss), 1 - AoA
 * @aoa_cal_gdp_incl: 1 if aoa_cal_gdp_info is present after this header
 * @mu_rx_data_incl: 1 if UL-OFDMA per-user info is present after this header
 * @freeze_data_incl: 1 if freeze_capture_tlv is present after this header
 *
 * @freeze_tlv_version: version of the freeze_capture_tlv that follows.
 * Per ucode's cc_upload_header_struct_t comment, values above 5 are not
 * currently defined -- 1->HSTP/Cypress, 2->MMS, 3->Pine, 4->HAM-1/2,
 * 5->WKK. Nothing in the ucode data indicates a new version for wifi8; treat as
 * macrx_freeze_capture_channel_v5 unless ucode says otherwise
 *
 * @mu_rx_num_users: number of UL-MU-PPDU users present in per-user info
 * @decimation_factor: FFT bins decimation, in log2 format (0->1x, 1->2x, 2->4x)
 * @reserved2: reserved, ignore
 *
 * @amplitude_gain_ratio_0_3: amplitude gain ratio for chains 0-3, one byte
 * each ([0:7]-Chain-0, [8:15]-Chain-1, [16:23]-Chain-2, [24:31]-Chain-3)
 *
 * @amplitude_gain_ratio_4: amplitude gain ratio for chain 4
 * @_11azbf_mode: 11az beamforming mode
 * @_11azbf_node: 11az beamforming node role
 *
 * @rescale_amt_shift_pri80: rescale amount shift for the primary 80 MHz
 * @rescale_amt_shift_sec80: rescale amount shift for the secondary 80 MHz
 *
 * @cgim_status: CGIM (coarse gain/interference mitigation) status
 * @cgim_filter: CGIM filter setting
 * @tx_or_rx_based_cfr: whether this capture is TX- or RX-based CFR
 * @demf_turbo_mode: DEMF turbo mode enable
 * @demf_pbs_en: DEMF PBS enable
 * @leg_cfr_mode: legacy CFR mode
 * @reserved7: reserved, ignore
 *
 * @puncture_pattern: puncture pattern applied to the capture bandwidth
 * @total_num_ltfs: total number of LTFs used for the capture
 *
 * @freeze_tlv_offset: offset, in u16 units from the start of this struct,
 * to the embedded freeze_capture_tlv. FW-provided -- use this rather than
 * sizeof(struct ath12k_cfir_wifi8_cc_hdr) to locate the freeze TLV
 *
 * @per_user_info_offset: offset, in u16 units from the start of this
 * struct, to the embedded per-user info array. FW-provided -- use this
 * rather than a computed offset
 *
 * @aoa_cal_gdp_offset: offset, in u16 units from the start of this
 * struct, to the embedded aoa_cal_gdp_info. FW-provided
 *
 * @reserved11: reserved, ignore
 * @reserved12: reserved, ignore
 */
struct ath12k_cfir_wifi8_cc_hdr {
	u16 capture_type         :  4,
	    cc_format            :  2,
	    cir_fmt              :  3,
	    aoa_cal_gdp_incl     :  1,
	    mu_rx_data_incl      :  1,
	    freeze_data_incl     :  1,
	    freeze_tlv_version   :  4;
	u16 mu_rx_num_users      :  8,
	    decimation_factor    :  4,
	    reserved2            :  4;
	u32 amplitude_gain_ratio_0_3;
	u16 amplitude_gain_ratio_4 :  8,
	    _11azbf_mode           :  4,
	    _11azbf_node           :  4;
	u16 rescale_amt_shift_pri80 : 8,
	    rescale_amt_shift_sec80 : 8;
	u16 cgim_status          :  1,
	    cgim_filter          :  1,
	    tx_or_rx_based_cfr   :  1,
	    demf_turbo_mode      :  1,
	    demf_pbs_en          :  2,
	    leg_cfr_mode         :  2,
	    reserved7            :  6;
	u16 puncture_pattern;
	u16 total_num_ltfs;
	u16 freeze_tlv_offset    :  5,
	    per_user_info_offset :  5,
	    aoa_cal_gdp_offset   :  6;
	u16 reserved11;
	u16 reserved12;
} __packed;

struct ath12k_cfr_look_up_table {
	bool dbr_recv;
	bool tx_recv;
	u8 *data;
	u32 data_len;
	u16 dbr_ppdu_id;
	u16 tx_ppdu_id;
	dma_addr_t dbr_address;
	u32 tx_address1;
	u32 tx_address2;
	struct ath12k_csi_cfr_header header;
	union {
		struct ath12k_cfir_dma_hdr hdr;
		struct ath12k_cfir_enh_dma_hdr enh_hdr;
		struct ath12k_cfir_wifi8_common_hdr wifi8_hdr;
	} dma_hdr;
	u64 txrx_tstamp;
	u64 dbr_tstamp;
	u32 header_length;
	u32 payload_length;
	struct ath12k_dbring_element *buff;
};

enum cfr_capture_type {
	CFR_CAPTURE_METHOD_NULL_FRAME = 0,
	CFR_CAPURE_METHOD_NULL_FRAME_WITH_PHASE = 1,
	CFR_CAPTURE_METHOD_PROBE_RESP = 2,
	CFR_CAPTURE_METHOD_TM = 3,
	CFR_CAPTURE_METHOD_FTM = 4,
	CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM = 5,
	CFR_CAPTURE_METHOD_TA_RA_TYPE_FILTER = 6,
	CFR_CAPTURE_METHOD_NDPA_NDP = 7,
	CFR_CAPTURE_METHOD_ALL_PACKET = 8,
	/* Add new capture methods before this line */
	CFR_CAPTURE_METHOD_LAST_VALID,
	CFR_CAPTURE_METHOD_AUTO = 0xff,
	CFR_CAPTURE_METHOD_MAX,
};

/* enum macrx_freeze_tlv_version: Reported by uCode in enh_dma_header
 * MACRX_FREEZE_TLV_VERSION_1: Single MU UL user info reported by MAC
 * MACRX_FREEZE_TLV_VERSION_2: Upto 4 MU UL user info reported by MAC
 * MACRX_FREEZE_TLV_VERSION_3: Upto 37 MU UL user info reported by MAC
 */
enum macrx_freeze_tlv_version {
	MACRX_FREEZE_TLV_VERSION_1 = 1,
	MACRX_FREEZE_TLV_VERSION_2 = 2,
	MACRX_FREEZE_TLV_VERSION_3 = 3,
	MACRX_FREEZE_TLV_VERSION_5 = 5,
	MACRX_FREEZE_TLV_VERSION_MAX
};

enum mac_freeze_capture_reason {
	FREEZE_REASON_TM = 0,
	FREEZE_REASON_FTM,
	FREEZE_REASON_ACK_RESP_TO_TM_FTM,
	FREEZE_REASON_TA_RA_TYPE_FILTER,
	FREEZE_REASON_NDPA_NDP,
	FREEZE_REASON_ALL_PACKET,
	FREEZE_REASON_MAX,
};

#define MACRX_FREEZE_CC_INFO0_FREEZE GENMASK(0, 0)
#define MACRX_FREEZE_CC_INFO0_CAPTURE_REASON GENMASK(3, 1)
#define MACRX_FREEZE_CC_INFO0_PKT_TYPE GENMASK(5, 4)
#define MACRX_FREEZE_CC_INFO0_PKT_SUB_TYPE GENMASK(9, 6)
#define MACRX_FREEZE_CC_INFO0_RSVD GENMASK(14, 10)
#define MACRX_FREEZE_CC_INFO0_SW_PEER_ID_VALID GENMASK(15, 15)

#define MACRX_FREEZE_CC_INFO1_USER_MASK GENMASK(5, 0)
#define MACRX_FREEZE_CC_INFO1_DIRECTED GENMASK(6, 6)
#define MACRX_FREEZE_CC_INFO1_RSVD GENMASK(15, 7)

struct macrx_freeze_capture_channel {
	u16 info0;
	u16 sw_peer_id;
	u16 phy_ppdu_id;
	u16 packet_ta_lower_16;
	u16 packet_ta_mid_16;
	u16 packet_ta_upper_16;
	u16 packet_ra_lower_16;
	u16 packet_ra_mid_16;
	u16 packet_ra_upper_16;
	u16 tsf_timestamp_15_0;
	u16 tsf_timestamp_31_16;
	u16 tsf_timestamp_47_32;
	u16 tsf_timestamp_63_48;
	u16 info1;
};

#define MACRX_FREEZE_CC_V3_INFO0_FREEZE GENMASK(0, 0)
#define MACRX_FREEZE_CC_V3_INFO0_CAPTURE_REASON GENMASK(3, 1)
#define MACRX_FREEZE_CC_V3_INFO0_PKT_TYPE GENMASK(5, 4)
#define MACRX_FREEZE_CC_V3_INFO0_PKT_SUB_TYPE GENMASK(9, 6)
#define MACRX_FREEZE_CC_V3_INFO0_DIRECTED GENMASK(10, 10)
#define MACRX_FREEZE_CC_V3_INFO0_RSVD GENMASK(14, 11)
#define MACRX_FREEZE_CC_V3_INFO0_SW_PEER_ID_VALID GENMASK(15, 15)

/*
 * freeze_tlv v3 used by qcn9074
 */
struct macrx_freeze_capture_channel_v3 {
	u16 info0;
	u16 sw_peer_id;
	u16 phy_ppdu_id;
	u16 packet_ta_lower_16;
	u16 packet_ta_mid_16;
	u16 packet_ta_upper_16;
	u16 packet_ra_lower_16;
	u16 packet_ra_mid_16;
	u16 packet_ra_upper_16;
	u16 tsf_timestamp_15_0;
	u16 tsf_timestamp_31_16;
	u16 tsf_timestamp_47_32;
	u16 tsf_63_48_or_user_mask_36_32;
	u16 user_index_or_user_mask_15_0;
	u16 user_mask_31_16;
};

struct cfr_unassoc_pool_entry {
	u8 peer_mac[ETH_ALEN];
	u32 period;
	bool is_valid;
};

#define MAX_TA_RA_ENTRIES 16
#define MAX_RESET_CFG_ENTRY 0xFFFF
struct ta_ra_cfr_cfg {
	u8 filter_group_id;
	u16 bw :5,
	    nss :8,
	    rsvd0 :3;
	u16 valid_ta :1,
	    valid_ta_mask :1,
	    valid_ra :1,
	    valid_ra_mask :1,
	    valid_bw_mask :1,
	    valid_nss_mask :1,
	    valid_mgmt_subtype :1,
	    valid_ctrl_subtype :1,
	    valid_data_subtype :1,
	    rsvd1 :7;
	u16 mgmt_subtype_filter;
	u16 ctrl_subtype_filter;
	u16 data_subtype_filter;
	u8 ta_addr[ETH_ALEN];
	u8 ra_addr[ETH_ALEN];
	u8 ta_addr_mask[ETH_ALEN];
	u8 ra_addr_mask[ETH_ALEN];
} __packed;

struct cfr_rcc_param {
	u8 pdev_id;
	u8 vdev_id;
	u8 srng_id;
	u32 capture_duration;
	u32 capture_interval;
	u32 ul_mu_user_mask_lower;
	u32 ul_mu_user_mask_upper;
	u16 freeze_tlv_delay_cnt_en  :1,
	    freeze_tlv_delay_cnt_thr :8,
	    rsvd0 :7;
	u16 filter_group_bitmap;
	u8 m_directed_ftm :1,
	   m_all_ftm_ack :1,
	   m_ndpa_ndp_directed :1,
	   m_ndpa_ndp_all :1,
	   m_ta_ra_filter :1,
	   m_all_packet :1,
	   en_ta_ra_filter_in_as_fp :1,
	   rsvd1 : 1;
	u8 num_grp_tlvs;
	struct ta_ra_cfr_cfg curr[MAX_TA_RA_ENTRIES];
	unsigned long modified_in_curr_session;
	u32 capture_count :16,
	    capture_intval_mode_sel  :1,
	    rsvd2 :15;
};

#define ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS WMI_AOA_MAX_GAIN_TABLE_IDX
struct cfr_enhanced_aoa_data {
	u32 freq;
	u32 max_aoa_chains;
	u32 ibf_cal_val[WMI_MAX_CHAINS];
	u32 max_agc_gain_tbls;
	u16 max_agc_gain_per_tbl_2g[ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS];
	u16 max_agc_gain_per_tbl_5g[ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS];
	u16 max_agc_gain_per_tbl_6g[ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS];
	u8 max_bdf_entries_per_tbl[ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS];
	u32 max_entries_all_table;
	u16 *gain_stop_index_array;
	u16 *enh_phase_delta_array;
	u8 start_ent[ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS];
	u32 xbar_config;
};

struct ath12k_cfr {
	struct ath12k_dbring rx_ring;
	/* Protects enabled for ath12k_cfr */
	spinlock_t lock;
	struct rchan *rfs_cfr_capture;
	struct dentry *enable_cfr;
	struct dentry *cfr_unassoc;
	u8 cfr_enabled_peer_cnt;
	struct ath12k_cfr_look_up_table *lut;
	u32 lut_num;
	u32 dbr_buf_size;
	u32 dbr_num_bufs;
	u32 max_mu_users;
	/* protect look up table data */
	spinlock_t lut_lock;
	struct timer_list lut_age_timer;
	bool lut_age_timer_init;
	u64 tx_evt_cnt;
	u64 dbr_evt_cnt;
	u64 total_tx_evt_cnt;
	u64 release_cnt;
	u64 tx_peer_status_cfr_fail;
	u64 tx_evt_status_cfr_fail;
	u64 tx_dbr_lookup_fail;
	u64 last_success_tstamp;
	u64 flush_dbr_cnt;
	u64 invalid_dma_length_cnt;
	u64 clear_txrx_event;
	u64 cfr_dma_aborts;
	u64 flush_timeout_dbr_cnt;
	struct cfr_unassoc_pool_entry unassoc_pool[ATH12K_MAX_CFR_ENABLED_CLIENTS];
	bool cfr_enabled;
	bool rcc_enabled;
	struct notifier_block ppdu_rx_notifier;
	bool ppdu_rx_notifier_registered;
	struct cfr_rcc_param rcc_param;
	struct ta_ra_cfr_cfg global[MAX_TA_RA_ENTRIES];
	bool is_enh_aoa_data;
	struct cfr_enhanced_aoa_data enh_aoa_data;
};

#ifdef CPTCFG_ATH12K_CFR
int ath12k_cfr_init(struct ath12k_base *ab);
void ath12k_cfr_deinit(struct ath12k_base *ab);
int ath12k_cfr_peer_capture_validate(struct ath12k *ar,
				     struct ath12k_link_sta *arsta,
				     u32 link_sta_bw,
				     u32 *cfr_capture_enable,
				     u32 *cfr_capture_bw,
				     u32 *cfr_capture_period,
				     u32 *cfr_capture_method);
void ath12k_cfr_peer_capture_fill_wmi_arg(struct wmi_peer_cfr_capture_conf_arg *arg,
					  u32 cfr_capture_enable,
					  u32 cfr_capture_bw,
					  u32 cfr_capture_period,
					  u32 cfr_capture_method);
void ath12k_cfr_peer_capture_update(struct ath12k *ar,
				    struct ath12k_link_sta *arsta,
				    u32 cfr_capture_enable,
				    u32 cfr_capture_bw,
				    u32 cfr_capture_period,
				    u32 cfr_capture_method);
struct ath12k_dbring *ath12k_cfr_get_dbring(struct ath12k *ar);
int ath12k_process_cfr_capture_event(struct ath12k_base *ab,
				     struct ath12k_cfr_peer_tx_param *params);
u8 freeze_reason_to_capture_type(struct ath12k_base *ab, void *freeze_tlv);
void extract_peer_mac_from_freeze_tlv(void *freeze_tlv, uint8_t *peermac);
bool peer_is_in_cfr_unassoc_pool(struct ath12k *ar, u8 *peer_mac);
int ath12k_cfr_register_ppdu_rx_notifier(struct ath12k *ar);
int ath12k_cfr_unregister_ppdu_rx_notifier(struct ath12k *ar);
void ath12k_cfr_lut_update_paddr(struct ath12k *ar, dma_addr_t paddr,
				 u32 buf_id);
void ath12k_cfr_decrement_peer_count(struct ath12k *ar, struct ath12k_link_sta *arsta);
int ath12k_cfr_parse_enh_dma_hdr(struct ath12k *ar, u8 *data,
				 struct ath12k_cfr_look_up_table *lut,
				 u32 *length);
int ath12k_cfr_get_enhanced_aoa_caps(struct ath12k *ar);
void
ath12k_wmi_cfr_handle_aoa_data(struct ath12k *ar,
			       struct ath12k_wmi_enhanced_aoa_phasedelta_parse *parse);

#else
static inline int ath12k_cfr_init(struct ath12k_base *ab)
{
	return 0;
}
static inline void ath12k_cfr_deinit(struct ath12k_base *ab)
{
}

static inline int ath12k_cfr_peer_capture_validate(struct ath12k *ar,
						   struct ath12k_link_sta *arsta,
						   u32 link_sta_bw,
						   u32 *cfr_capture_enable,
						   u32 *cfr_capture_bw,
						   u32 *cfr_capture_period,
						   u32 *cfr_capture_method)
{
	return 0;
}

static inline
void ath12k_cfr_peer_capture_fill_wmi_arg(struct wmi_peer_cfr_capture_conf_arg *arg,
					  u32 cfr_capture_enable,
					  u32 cfr_capture_bw,
					  u32 cfr_capture_period,
					  u32 cfr_capture_method)
{
}

static inline void ath12k_cfr_peer_capture_update(struct ath12k *ar,
						  struct ath12k_link_sta *arsta,
						  u32 cfr_capture_enable,
						  u32 cfr_capture_bw,
						  u32 cfr_capture_period,
						  u32 cfr_capture_method)
{
}

static inline
struct ath12k_dbring *ath12k_cfr_get_dbring(struct ath12k *ar)
{
	return NULL;
}
static inline bool peer_is_in_cfr_unassoc_pool(struct ath12k *ar, u8 *peer_mac)
{
	return false;
}
static inline
int ath12k_process_cfr_capture_event(struct ath12k_base *ab,
				     struct ath12k_cfr_peer_tx_param *params)
{
	return 0;
}

static inline
u8 freeze_reason_to_capture_type(struct ath12k_base *ab, void *freeze_tlv)
{
	return 0;
}

static inline
void extract_peer_mac_from_freeze_tlv(void *freeze_tlv, uint8_t *peermac)
{
}

static inline void ath12k_cfr_lut_update_paddr(struct ath12k *ar,
					       dma_addr_t paddr, u32 buf_id)
{
}
static inline void ath12k_cfr_decrement_peer_count(struct ath12k *ar,
						struct ath12k_link_sta *arsta)
{
}

static inline int
ath12k_cfr_parse_enh_dma_hdr(struct ath12k *ar, u8 *data,
			     struct ath12k_cfr_look_up_table *lut,
			     u32 *length)
{
	return 0;
}

static inline int ath12k_cfr_get_enhanced_aoa_caps(struct ath12k *ar)
{
	return 0;
}

static inline void
ath12k_wmi_cfr_handle_aoa_data(struct ath12k *ar,
			       struct ath12k_wmi_enhanced_aoa_phasedelta_parse *parse)
{
}

#endif /* CPTCFG_ATH12K_CFR */
#endif /* ATH12K_CFR_H */
