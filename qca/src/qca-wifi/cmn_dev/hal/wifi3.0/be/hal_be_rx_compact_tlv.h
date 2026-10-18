/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _HAL_BE_RX_TLV_COMPACT_H_
#define _HAL_BE_RX_TLV_COMPACT_H_

#ifdef CONFIG_WORD_BASED_TLV
#ifdef BE_NON_AP_COMPACT_TLV
/* 7 qwords for rx_mpdu_start without tag */
#define MPDU_START_WMASK 0x07B8
/* 8 qwords for rx_msdu_end without tag */
#define MSDU_END_WMASK 0x115CA

#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
struct rx_msdu_end_compact {
	/* qword-0 */
             uint32_t rxpcu_mpdu_filter_in_category                           :  2,
                      sw_frame_group_id                                       :  7,
                      reserved_0                                              :  7,
                      phy_ppdu_id                                             : 16;
             uint32_t ip_hdr_chksum                                           : 16,
                      reported_mpdu_length                                    : 14,
                      reserved_1a                                             :  2;
	/* qword-2 */
             uint32_t ipv6_options_crc;
             uint32_t da_offset                                               :  6,
                      sa_offset                                               :  6,
                      da_offset_valid                                         :  1,
                      sa_offset_valid                                         :  1,
                      reserved_5a                                             :  2,
                      l3_type                                                 : 16;
	/* qword-5 */
             uint32_t sa_sw_peer_id                                           : 16,
                      sa_idx_timeout                                          :  1,
                      da_idx_timeout                                          :  1,
                      to_ds                                                   :  1,
                      tid                                                     :  4,
                      sa_is_valid                                             :  1,
                      da_is_valid                                             :  1,
                      da_is_mcbc                                              :  1,
                      l3_header_padding                                       :  2,
                      first_msdu                                              :  1,
                      last_msdu                                               :  1,
                      fr_ds                                                   :  1,
                      ip_chksum_fail_copy                                     :  1;
             uint32_t sa_idx                                                  : 16,
                      da_idx_or_sw_peer_id                                    : 16;
	/* qword-6 */
             uint32_t msdu_drop                                               :  1,
                      reo_destination_indication                              :  5,
                      flow_idx                                                : 20,
                      use_ppe                                                 :  1,
                      __reserved_g_0003                                       :  2,
                      vlan_ctag_stripped                                      :  1,
                      vlan_stag_stripped                                      :  1,
                      fragment_flag                                           :  1;
             uint32_t fse_metadata                                            : 32;
	/* qword-7 */
             uint32_t cce_metadata                                            : 16,
                      tcp_udp_chksum                                          : 16;
             uint32_t aggregation_count                                       :  8,
                      flow_aggregation_continuation                           :  1,
                      fisa_timeout                                            :  1,
                      tcp_udp_chksum_fail_copy                                :  1,
                      msdu_limit_error                                        :  1,
                      flow_idx_timeout                                        :  1,
                      flow_idx_invalid                                        :  1,
                      cce_match                                               :  1,
                      amsdu_parser_error                                      :  1,
                      cumulative_ip_length                                    : 16;
	/* qword-9 */
             uint32_t msdu_length                                             : 14,
                      stbc                                                    :  1,
                      ipsec_esp                                               :  1,
                      l3_offset                                               :  7,
                      ipsec_ah                                                :  1,
                      l4_offset                                               :  8;
             uint32_t msdu_number                                             :  8,
                      decap_format                                            :  2,
                      ipv4_proto                                              :  1,
                      ipv6_proto                                              :  1,
                      tcp_proto                                               :  1,
                      udp_proto                                               :  1,
                      ip_frag                                                 :  1,
                      tcp_only_ack                                            :  1,
                      da_is_bcast_mcast                                       :  1,
                      toeplitz_hash_sel                                       :  2,
                      ip_fixed_header_valid                                   :  1,
                      ip_extn_header_valid                                    :  1,
                      tcp_udp_header_valid                                    :  1,
                      mesh_control_present                                    :  1,
                      ldpc                                                    :  1,
                      ip4_protocol_ip6_next_header                            :  8;
	/* qword-11 */
             uint32_t user_rssi                                               :  8,
                      pkt_type                                                :  4,
                      sgi                                                     :  2,
                      rate_mcs                                                :  4,
                      receive_bandwidth                                       :  3,
                      reception_type                                          :  3,
                      mimo_ss_bitmap                                          :  7,
                      msdu_done_copy                                          :  1;
             uint32_t flow_id_toeplitz                                        : 32;
	/* qword-15 */
             uint32_t first_mpdu                                              :  1,
                      reserved_30a                                            :  1,
                      mcast_bcast                                             :  1,
                      ast_index_not_found                                     :  1,
                      ast_index_timeout                                       :  1,
                      power_mgmt                                              :  1,
                      non_qos                                                 :  1,
                      null_data                                               :  1,
                      mgmt_type                                               :  1,
                      ctrl_type                                               :  1,
                      more_data                                               :  1,
                      eosp                                                    :  1,
                      a_msdu_error                                            :  1,
                      reserved_30b                                            :  1,
                      order                                                   :  1,
                      wifi_parser_error                                       :  1,
                      overflow_err                                            :  1,
                      msdu_length_err                                         :  1,
                      tcp_udp_chksum_fail                                     :  1,
                      ip_chksum_fail                                          :  1,
                      sa_idx_invalid                                          :  1,
                      da_idx_invalid                                          :  1,
                      amsdu_addr_mismatch                                     :  1,
                      rx_in_tx_decrypt_byp                                    :  1,
                      encrypt_required                                        :  1,
                      directed                                                :  1,
                      buffer_fragment                                         :  1,
                      mpdu_length_err                                         :  1,
                      tkip_mic_err                                            :  1,
                      decrypt_err                                             :  1,
                      unencrypted_frame_err                                   :  1,
                      fcs_err                                                 :  1;
             uint32_t reserved_31a                                            : 10,
                      decrypt_status_code                                     :  3,
                      rx_bitmap_not_updated                                   :  1,
                      reserved_31b                                            : 17,
                      msdu_done                                               :  1;
};

struct rx_mpdu_start_compact {
	/* qword-2 */
             uint32_t pn_31_0                                                 : 32;
             uint32_t pn_63_32                                                : 32;
	/* qword-3 */
             uint32_t pn_95_64                                                : 32;
             uint32_t pn_127_96                                               : 32;
	/* qword-4 */
             uint32_t mpdu_frame_control_valid                                :  1,
                      mpdu_duration_valid                                     :  1,
                      mac_addr_ad1_valid                                      :  1,
                      mac_addr_ad2_valid                                      :  1,
                      mac_addr_ad3_valid                                      :  1,
                      mac_addr_ad4_valid                                      :  1,
                      mpdu_sequence_control_valid                             :  1,
                      mpdu_qos_control_valid                                  :  1,
                      mpdu_ht_control_valid                                   :  1,
                      frame_encryption_info_valid                             :  1,
                      mpdu_fragment_number                                    :  4,
                      more_fragment_flag                                      :  1,
                      reserved_11a                                            :  1,
                      fr_ds                                                   :  1,
                      to_ds                                                   :  1,
                      encrypted                                               :  1,
                      mpdu_retry                                              :  1,
                      mpdu_sequence_number                                    : 12;
             uint32_t peer_meta_data                                          : 32;
	/* qword-6 */
             uint32_t key_id_octet                                            :  8,
                      new_peer_entry                                          :  1,
                      decrypt_needed                                          :  1,
                      decap_type                                              :  2,
                      rx_insert_vlan_c_tag_padding                            :  1,
                      rx_insert_vlan_s_tag_padding                            :  1,
                      strip_vlan_c_tag_decap                                  :  1,
                      strip_vlan_s_tag_decap                                  :  1,
                      pre_delim_count                                         : 12,
                      ampdu_flag                                              :  1,
                      bar_frame                                               :  1,
                      raw_mpdu                                                :  1,
                      reserved_12                                             :  1;
             uint32_t mpdu_length                                             : 14,
                      first_mpdu                                              :  1,
                      mcast_bcast                                             :  1,
                      ast_index_not_found                                     :  1,
                      ast_index_timeout                                       :  1,
                      power_mgmt                                              :  1,
                      non_qos                                                 :  1,
                      null_data                                               :  1,
                      mgmt_type                                               :  1,
                      ctrl_type                                               :  1,
                      more_data                                               :  1,
                      eosp                                                    :  1,
                      fragment_flag                                           :  1,
                      order                                                   :  1,
                      u_apsd_trigger                                          :  1,
                      encrypt_required                                        :  1,
                      directed                                                :  1,
                      amsdu_present                                           :  1,
                      reserved_13                                             :  1;
	/* qword-7 */
             uint32_t mpdu_frame_control_field                                : 16,
                      mpdu_duration_field                                     : 16;
             uint32_t mac_addr_ad1_31_0                                       : 32;
	/* qword-8 */
             uint32_t mac_addr_ad1_47_32                                      : 16,
                      mac_addr_ad2_15_0                                       : 16;
             uint32_t mac_addr_ad2_47_16                                      : 32;
	/* qword-9 */
             uint32_t mac_addr_ad3_31_0                                       : 32;
             uint32_t mac_addr_ad3_47_32                                      : 16,
                      mpdu_sequence_control_field                             : 16;
};
#else
struct rx_msdu_end_compact {
	/* qword-0 */
             uint32_t phy_ppdu_id                                             : 16,
                      reserved_0                                              :  7,
                      sw_frame_group_id                                       :  7,
                      rxpcu_mpdu_filter_in_category                           :  2;
             uint32_t reserved_1a                                             :  2,
                      reported_mpdu_length                                    : 14,
                      ip_hdr_chksum                                           : 16;
	/* qword-2 */
             uint32_t ipv6_options_crc;
             uint32_t l3_type                                                 : 16,
                      reserved_5a                                             :  2,
                      sa_offset_valid                                         :  1,
                      da_offset_valid                                         :  1,
                      sa_offset                                               :  6,
                      da_offset                                               :  6;
	/* qword-5 */
             uint32_t ip_chksum_fail_copy                                     :  1,
                      fr_ds                                                   :  1,
                      last_msdu                                               :  1,
                      first_msdu                                              :  1,
                      l3_header_padding                                       :  2,
                      da_is_mcbc                                              :  1,
                      da_is_valid                                             :  1,
                      sa_is_valid                                             :  1,
                      tid                                                     :  4,
                      to_ds                                                   :  1,
                      da_idx_timeout                                          :  1,
                      sa_idx_timeout                                          :  1,
                      sa_sw_peer_id                                           : 16;
             uint32_t da_idx_or_sw_peer_id                                    : 16,
                      sa_idx                                                  : 16;
	/* qword-6 */
             uint32_t fragment_flag                                           :  1,
                      vlan_stag_stripped                                      :  1,
                      vlan_ctag_stripped                                      :  1,
                      __reserved_g_0003                                       :  2,
                      use_ppe                                                 :  1,
                      flow_idx                                                : 20,
                      reo_destination_indication                              :  5,
                      msdu_drop                                               :  1;
             uint32_t fse_metadata                                            : 32;
	/* qword-7 */
             uint32_t tcp_udp_chksum                                          : 16,
                      cce_metadata                                            : 16;
             uint32_t cumulative_ip_length                                    : 16,
                      amsdu_parser_error                                      :  1,
                      cce_match                                               :  1,
                      flow_idx_invalid                                        :  1,
                      flow_idx_timeout                                        :  1,
                      msdu_limit_error                                        :  1,
                      tcp_udp_chksum_fail_copy                                :  1,
                      fisa_timeout                                            :  1,
                      flow_aggregation_continuation                           :  1,
                      aggregation_count                                       :  8;
	/* qword-9 */
             uint32_t l4_offset                                               :  8,
                      ipsec_ah                                                :  1,
                      l3_offset                                               :  7,
                      ipsec_esp                                               :  1,
                      stbc                                                    :  1,
                      msdu_length                                             : 14;
             uint32_t ip4_protocol_ip6_next_header                            :  8,
                      ldpc                                                    :  1,
                      mesh_control_present                                    :  1,
                      tcp_udp_header_valid                                    :  1,
                      ip_extn_header_valid                                    :  1,
                      ip_fixed_header_valid                                   :  1,
                      toeplitz_hash_sel                                       :  2,
                      da_is_bcast_mcast                                       :  1,
                      tcp_only_ack                                            :  1,
                      ip_frag                                                 :  1,
                      udp_proto                                               :  1,
                      tcp_proto                                               :  1,
                      ipv6_proto                                              :  1,
                      ipv4_proto                                              :  1,
                      decap_format                                            :  2,
                      msdu_number                                             :  8;
	/* qword-11 */
             uint32_t msdu_done_copy                                          :  1,
                      mimo_ss_bitmap                                          :  7,
                      reception_type                                          :  3,
                      receive_bandwidth                                       :  3,
                      rate_mcs                                                :  4,
                      sgi                                                     :  2,
                      pkt_type                                                :  4,
                      user_rssi                                               :  8;
             uint32_t flow_id_toeplitz                                        : 32;
	/* qword-15 */
             uint32_t fcs_err                                                 :  1,
                      unencrypted_frame_err                                   :  1,
                      decrypt_err                                             :  1,
                      tkip_mic_err                                            :  1,
                      mpdu_length_err                                         :  1,
                      buffer_fragment                                         :  1,
                      directed                                                :  1,
                      encrypt_required                                        :  1,
                      rx_in_tx_decrypt_byp                                    :  1,
                      amsdu_addr_mismatch                                     :  1,
                      da_idx_invalid                                          :  1,
                      sa_idx_invalid                                          :  1,
                      ip_chksum_fail                                          :  1,
                      tcp_udp_chksum_fail                                     :  1,
                      msdu_length_err                                         :  1,
                      overflow_err                                            :  1,
                      wifi_parser_error                                       :  1,
                      order                                                   :  1,
                      reserved_30b                                            :  1,
                      a_msdu_error                                            :  1,
                      eosp                                                    :  1,
                      more_data                                               :  1,
                      ctrl_type                                               :  1,
                      mgmt_type                                               :  1,
                      null_data                                               :  1,
                      non_qos                                                 :  1,
                      power_mgmt                                              :  1,
                      ast_index_timeout                                       :  1,
                      ast_index_not_found                                     :  1,
                      mcast_bcast                                             :  1,
                      reserved_30a                                            :  1,
                      first_mpdu                                              :  1;
             uint32_t msdu_done                                               :  1,
                      reserved_31b                                            : 17,
                      rx_bitmap_not_updated                                   :  1,
                      decrypt_status_code                                     :  3,
                      reserved_31a                                            : 10;
};

struct rx_mpdu_start_compact {
	/* qword-2 */
             uint32_t pn_31_0                                                 : 32;
             uint32_t pn_63_32                                                : 32;
	/* qword-3 */
             uint32_t pn_95_64                                                : 32;
             uint32_t pn_127_96                                               : 32;
	/* qword-4 */
             uint32_t mpdu_sequence_number                                    : 12,
                      mpdu_retry                                              :  1,
                      encrypted                                               :  1,
                      to_ds                                                   :  1,
                      fr_ds                                                   :  1,
                      reserved_11a                                            :  1,
                      more_fragment_flag                                      :  1,
                      mpdu_fragment_number                                    :  4,
                      frame_encryption_info_valid                             :  1,
                      mpdu_ht_control_valid                                   :  1,
                      mpdu_qos_control_valid                                  :  1,
                      mpdu_sequence_control_valid                             :  1,
                      mac_addr_ad4_valid                                      :  1,
                      mac_addr_ad3_valid                                      :  1,
                      mac_addr_ad2_valid                                      :  1,
                      mac_addr_ad1_valid                                      :  1,
                      mpdu_duration_valid                                     :  1,
                      mpdu_frame_control_valid                                :  1;
             uint32_t peer_meta_data                                          : 32;
	/* qword-6 */
             uint32_t reserved_12                                             :  1,
                      raw_mpdu                                                :  1,
                      bar_frame                                               :  1,
                      ampdu_flag                                              :  1,
                      pre_delim_count                                         : 12,
                      strip_vlan_s_tag_decap                                  :  1,
                      strip_vlan_c_tag_decap                                  :  1,
                      rx_insert_vlan_s_tag_padding                            :  1,
                      rx_insert_vlan_c_tag_padding                            :  1,
                      decap_type                                              :  2,
                      decrypt_needed                                          :  1,
                      new_peer_entry                                          :  1,
                      key_id_octet                                            :  8;
             uint32_t reserved_13                                             :  1,
                      amsdu_present                                           :  1,
                      directed                                                :  1,
                      encrypt_required                                        :  1,
                      u_apsd_trigger                                          :  1,
                      order                                                   :  1,
                      fragment_flag                                           :  1,
                      eosp                                                    :  1,
                      more_data                                               :  1,
                      ctrl_type                                               :  1,
                      mgmt_type                                               :  1,
                      null_data                                               :  1,
                      non_qos                                                 :  1,
                      power_mgmt                                              :  1,
                      ast_index_timeout                                       :  1,
                      ast_index_not_found                                     :  1,
                      mcast_bcast                                             :  1,
                      first_mpdu                                              :  1,
                      mpdu_length                                             : 14;
	/* qword-7 */
             uint32_t mpdu_duration_field                                     : 16,
                      mpdu_frame_control_field                                : 16;
             uint32_t mac_addr_ad1_31_0                                       : 32;
	/* qword-8 */
             uint32_t mac_addr_ad2_15_0                                       : 16,
                      mac_addr_ad1_47_32                                      : 16;
             uint32_t mac_addr_ad2_47_16                                      : 32;
	/* qword-9 */
             uint32_t mac_addr_ad3_31_0                                       : 32;
             uint32_t mpdu_sequence_control_field                             : 16,
                      mac_addr_ad3_47_32                                      : 16;
};
#endif /* WIFI_BIT_ORDER_BIG_ENDIAN */

#else /* !BE_NON_AP_COMPACT_TLV */

#define MPDU_START_WMASK 0x074C
#define MSDU_END_WMASK 0x137C9

#ifndef BIG_ENDIAN_HOST
struct rx_msdu_end_compact {
	uint32_t ipv6_options_crc			: 32;
	uint32_t da_offset				:  6,
		 sa_offset				:  6,
		 da_offset_valid			:  1,
		 sa_offset_valid			:  1,
		 reserved_5a				:  2,
		 l3_type				: 16;
	uint32_t sa_sw_peer_id				: 16,
		 sa_idx_timeout				:  1,
		 da_idx_timeout				:  1,
		 to_ds					:  1,
		 tid					:  4,
		 sa_is_valid				:  1,
		 da_is_valid				:  1,
		 da_is_mcbc				:  1,
		 l3_header_padding			:  2,
		 first_msdu				:  1,
		 last_msdu				:  1,
		 fr_ds					:  1,
		 ip_chksum_fail_copy			:  1;
	uint32_t sa_idx					: 16,
		 da_idx_or_sw_peer_id			: 16;
	uint32_t msdu_drop				:  1,
		 reo_destination_indication		:  5,
		 flow_idx				: 20,
		 use_ppe				:  1,
		 mesh_sta				:  2,
		 vlan_ctag_stripped			:  1,
		 vlan_stag_stripped			:  1,
		 fragment_flag				:  1;
	uint32_t fse_metadata				: 32;
	uint32_t cce_metadata				: 16,
		 tcp_udp_chksum				: 16;
	uint32_t aggregation_count			:  8,
		 flow_aggregation_continuation		:  1,
		 fisa_timeout				:  1,
		 tcp_udp_chksum_fail_copy		:  1,
		 msdu_limit_error			:  1,
		 flow_idx_timeout			:  1,
		 flow_idx_invalid			:  1,
		 cce_match				:  1,
		 amsdu_parser_error			:  1,
		 cumulative_ip_length			: 16;
	uint32_t key_id_octet				:  8,
		 reserved_8a				: 24;
	uint32_t reserved_9a				:  6,
		 service_code				:  9,
		 priority_valid				:  1,
		 intra_bss				:  1,
		 dest_chip_id				:  2,
		 multicast_echo				:  1,
		 wds_learning_event			:  1,
		 wds_roaming_event			:  1,
		 wds_keep_alive_event			:  1,
		 dest_chip_pmac_id			:  1,
		 reserved_9b				:  8;
	uint32_t msdu_length				: 14,
		 stbc					:  1,
		 ipsec_esp				:  1,
		 l3_offset				:  7,
		 ipsec_ah				:  1,
		 l4_offset				:  8;
	uint32_t msdu_number				:  8,
		 decap_format				:  2,
		 ipv4_proto				:  1,
		 ipv6_proto				:  1,
		 tcp_proto				:  1,
		 udp_proto				:  1,
		 ip_frag				:  1,
		 tcp_only_ack				:  1,
		 da_is_bcast_mcast			:  1,
		 toeplitz_hash_sel			:  2,
		 ip_fixed_header_valid			:  1,
		 ip_extn_header_valid			:  1,
		 tcp_udp_header_valid			:  1,
		 mesh_control_present			:  1,
		 ldpc					:  1,
		 ip4_protocol_ip6_next_header		:  8;
	uint32_t user_rssi				:  8,
		 pkt_type				:  4,
		 sgi					:  2,
		 rate_mcs				:  4,
		 receive_bandwidth			:  3,
		 reception_type				:  3,
		 mimo_ss_bitmap				:  7,
		 msdu_done_copy				:  1;
	uint32_t flow_id_toeplitz			: 32;
	uint32_t ppdu_start_timestamp_63_32;
	uint32_t sw_phy_meta_data			: 32;
	uint32_t first_mpdu				:  1,
		 reserved_16a				:  1,
		 mcast_bcast				:  1,
		 ast_index_not_found			:  1,
		 ast_index_timeout			:  1,
		 power_mgmt				:  1,
		 non_qos				:  1,
		 null_data				:  1,
		 mgmt_type				:  1,
		 ctrl_type				:  1,
		 more_data				:  1,
		 eosp					:  1,
		 a_msdu_error				:  1,
		 reserved_16b				:  1,
		 order					:  1,
		 wifi_parser_error			:  1,
		 overflow_err				:  1,
		 msdu_length_err			:  1,
		 tcp_udp_chksum_fail			:  1,
		 ip_chksum_fail				:  1,
		 sa_idx_invalid				:  1,
		 da_idx_invalid				:  1,
		 amsdu_addr_mismatch			:  1,
		 rx_in_tx_decrypt_byp			:  1,
		 encrypt_required			:  1,
		 directed				:  1,
		 buffer_fragment			:  1,
		 mpdu_length_err			:  1,
		 tkip_mic_err				:  1,
		 decrypt_err				:  1,
		 unencrypted_frame_err			:  1,
		 fcs_err				:  1;
	uint32_t reserved_17a				: 10,
		 decrypt_status_code			:  3,
		 rx_bitmap_not_updated			:  1,
		 reserved_17b				: 17,
		 msdu_done				:  1;
};

struct rx_mpdu_start_compact {
	uint32_t rx_reo_queue_desc_addr_39_32		:  8,
		 receive_queue_number			: 16,
		 pre_delim_err_warning			:  1,
		 first_delim_err			:  1,
		 reserved_0				:  6;
	uint32_t pn_31_0				: 32;
	uint32_t pn_63_32				: 32;
	uint32_t pn_95_64				: 32;
	uint32_t ast_index				: 16,
		 sw_peer_id				: 16;
	uint32_t mpdu_frame_control_valid		:  1,
		 mpdu_duration_valid			:  1,
		 mac_addr_ad1_valid			:  1,
		 mac_addr_ad2_valid			:  1,
		 mac_addr_ad3_valid			:  1,
		 mac_addr_ad4_valid			:  1,
		 mpdu_sequence_control_valid		:  1,
		 mpdu_qos_control_valid			:  1,
		 mpdu_ht_control_valid			:  1,
		 frame_encryption_info_valid		:  1,
		 mpdu_fragment_number			:  4,
		 more_fragment_flag			:  1,
		 reserved_7a				:  1,
		 fr_ds					:  1,
		 to_ds					:  1,
		 encrypted				:  1,
		 mpdu_retry				:  1,
		 mpdu_sequence_number			: 12;
	uint32_t mpdu_frame_control_field		: 16,
		 mpdu_duration_field			: 16;
	uint32_t mac_addr_ad1_31_0			: 32;
	uint32_t mac_addr_ad1_47_32			: 16,
		 mac_addr_ad2_15_0			: 16;
	uint32_t mac_addr_ad2_47_16			: 32;
	uint32_t mac_addr_ad3_31_0			: 32;
	uint32_t mac_addr_ad3_47_32			: 16,
		 mpdu_sequence_control_field		: 16;
};
#else
struct rx_msdu_end_compact {
	uint32_t ipv6_options_crc			: 32;
	uint32_t l3_type				: 16;
		 reserved_5a				:  2,
		 sa_offset_valid			:  1,
		 da_offset_valid			:  1,
		 sa_offset				:  6,
		 da_offset				:  6,
	uint32_t ip_chksum_fail_copy			:  1,
		 fr_ds					:  1,
		 last_msdu				:  1,
		 first_msdu				:  1,
		 l3_header_padding			:  2,
		 da_is_mcbc				:  1,
		 da_is_valid				:  1,
		 sa_is_valid				:  1,
		 tid					:  4,
		 to_ds					:  1,
		 da_idx_timeout				:  1,
		 sa_idx_timeout				:  1,
		 sa_sw_peer_id				: 16;
	uint32_t da_idx_or_sw_peer_id			: 16,
		 sa_idx					: 16;
	uint32_t fragment_flag				:  1,
		 vlan_stag_stripped			:  1,
		 vlan_ctag_stripped			:  1,
		 mesh_sta				:  2,
		 use_ppe				:  1,
		 flow_idx				: 20,
		 reo_destination_indication		:  5,
		 msdu_drop				:  1;
	uint32_t fse_metadata				: 32;
	uint32_t tcp_udp_chksum				: 16,
		 cce_metadata				: 16;
	uint32_t cumulative_ip_length			: 16,
		 amsdu_parser_error			:  1,
		 cce_match				:  1,
		 flow_idx_invalid			:  1,
		 flow_idx_timeout			:  1,
		 msdu_limit_error			:  1,
		 tcp_udp_chksum_fail_copy		:  1,
		 fisa_timeout				:  1,
		 flow_aggregation_continuation		:  1,
		 aggregation_count			:  8;
	uint32_t reserved_8a				: 24,
		 key_id_octet				:  8;
	uint32_t reserved_9b				:  8,
		 dest_chip_pmac_id			:  1,
		 wds_keep_alive_event			:  1,
		 wds_roaming_event			:  1,
		 wds_learning_event			:  1,
		 multicast_echo				:  1,
		 dest_chip_id				:  2,
		 intra_bss				:  1,
		 priority_valid				:  1,
		 service_code				:  9,
		 reserved_9a				:  6;
	uint32_t l4_offset				:  8,
		 ipsec_ah				:  1,
		 l3_offset				:  7,
		 ipsec_esp				:  1,
		 stbc					:  1,
		 msdu_length				: 14;
	uint32_t ip4_protocol_ip6_next_header		:  8,
		 ldpc					:  1,
		 mesh_control_present			:  1,
		 tcp_udp_header_valid			:  1,
		 ip_extn_header_valid			:  1,
		 ip_fixed_header_valid			:  1,
		 toeplitz_hash_sel			:  2,
		 da_is_bcast_mcast			:  1,
		 tcp_only_ack				:  1,
		 ip_frag				:  1,
		 udp_proto				:  1,
		 tcp_proto				:  1,
		 ipv6_proto				:  1,
		 ipv4_proto				:  1,
		 decap_format				:  2,
		 msdu_number				:  8;
	uint32_t msdu_done_copy				:  1,
		 mimo_ss_bitmap				:  7,
		 reception_type				:  3,
		 receive_bandwidth			:  3,
		 rate_mcs				:  4,
		 sgi					:  2,
		 pkt_type				:  4,
		 user_rssi				:  8;
	uint32_t flow_id_toeplitz			: 32;
	uint32_t ppdu_start_timestamp_63_32;
	uint32_t sw_phy_meta_data			: 32;
	uint32_t fcs_err				:  1,
		 unencrypted_frame_err			:  1,
		 decrypt_err				:  1,
		 tkip_mic_err				:  1,
		 mpdu_length_err			:  1,
		 buffer_fragment			:  1,
		 directed				:  1,
		 encrypt_required			:  1,
		 rx_in_tx_decrypt_byp			:  1,
		 amsdu_addr_mismatch			:  1,
		 da_idx_invalid				:  1,
		 sa_idx_invalid				:  1,
		 ip_chksum_fail				:  1,
		 tcp_udp_chksum_fail			:  1,
		 msdu_length_err			:  1,
		 overflow_err				:  1,
		 wifi_parser_error			:  1,
		 order					:  1,
		 reserved_16b				:  1,
		 a_msdu_error				:  1,
		 eosp					:  1,
		 more_data				:  1,
		 ctrl_type				:  1,
		 mgmt_type				:  1,
		 null_data				:  1,
		 non_qos				:  1,
		 power_mgmt				:  1,
		 ast_index_timeout			:  1,
		 ast_index_not_found			:  1,
		 mcast_bcast				:  1,
		 reserved_16a				:  1,
		 first_mpdu				:  1;
	uint32_t msdu_done				:  1,
		 reserved_17b				: 17,
		 rx_bitmap_not_updated			:  1,
		 decrypt_status_code			:  3,
		 reserved_17a				: 10;
};

struct rx_mpdu_start_compact {
	uint32_t reserved_0				:  6,
		 first_delim_err			:  1,
		 pre_delim_err_warning			:  1,
		 receive_queue_number			: 16,
		 rx_reo_queue_desc_addr_39_32		:  8;
	uint32_t pn_31_0				: 32;
	uint32_t pn_63_32				: 32;
	uint32_t pn_95_64				: 32;
	uint32_t sw_peer_id				: 16,
		 ast_index				: 16;
	uint32_t mpdu_sequence_number			: 12,
		 mpdu_retry				:  1,
		 encrypted				:  1,
		 to_ds					:  1,
		 fr_ds					:  1,
		 reserved_7a				:  1,
		 more_fragment_flag			:  1,
		 mpdu_fragment_number			:  4,
		 frame_encryption_info_valid		:  1,
		 mpdu_ht_control_valid			:  1,
		 mpdu_qos_control_valid			:  1,
		 mpdu_sequence_control_valid		:  1,
		 mac_addr_ad4_valid			:  1,
		 mac_addr_ad3_valid			:  1,
		 mac_addr_ad2_valid			:  1,
		 mac_addr_ad1_valid			:  1,
		 mpdu_duration_valid			:  1,
		 mpdu_frame_control_valid		:  1;
	uint32_t mpdu_duration_field			: 16,
		 mpdu_frame_control_field		: 16;
	uint32_t mac_addr_ad1_31_0			: 32;
	uint32_t mac_addr_ad2_15_0			: 16,
		 mac_addr_ad1_47_32			: 16;
	uint32_t mac_addr_ad2_47_16			: 32;
	uint32_t mac_addr_ad3_31_0			: 32;
	uint32_t mpdu_sequence_control_field		: 16,
		 mac_addr_ad3_47_32			: 16;
};
#endif /* BIG_ENDIAN_HOST */
#endif /* BE_NON_AP_COMPACT_TLV */
#endif /* CONFIG_WORD_BASED_TLV */
#endif /* _HAL_BE_RX_COMPACT_TLV_H_ */
