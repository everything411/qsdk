/*
 * hostapd / Tid-to-link Mapping(TTLM)
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef TTLM_H
#define TTLM_H

#define NUM_MAX_TIDS	8

#define TTLM_EXPECTED_DURATION_SIZE		3

/**
 * struct tid_to_link_map - TID-to-link mapping params
 * @tid_num: TID number
 * @link_map: TTLM link map for the given TID
 */
struct tid_to_link_map {
	u8 tid;
	u16 link_map;
};

/**
 * enum ttlm_dir - TID-to-link mapping direction
 * @TTLM_DIRECTION_DL: Downlink
 * @TTLM_DIRECTION_UL: Uplink
 * @TTLM_DIRECTION_BIDI: Bidirectional
 * @TTLM_DIRECTION_MAX: Max direction
 * @TTLM_DIRECTION_INVALID: Invalid direction
 */
enum ttlm_dir {
	TTLM_DIRECTION_DL,
	TTLM_DIRECTION_UL,
	TTLM_DIRECTION_BIDI,
	TTLM_DIRECTION_MAX,
	TTLM_DIRECTION_INVALID
};

/**
 * struct ttlm_info - TID-to-Link mapping information for the frames
 * transmitted on the uplink, downlink and bidirectional.
 *
 * @direction:  0 - Downlink, 1 - uplink 2 - Both uplink and downlink
 * @default_link_mapping: value 1 indicates the default TTLM, where all the TIDs
 *                        are mapped to all the links.
 *                        value 0 indicates the preferred TTLM mapping
 * @ieee_link_map_tid: Indicates ieee link id mapping of all the TIDS
 * @link_mapping_size: value 1 indicates the length of Link Mapping Of TIDn
 *                     field is 1 octet, value 0 indicates the length of the
 *                     Link Mapping of TIDn field is 2 octets
 * @mapping_switch_time_present: Indicates if mapping switch time field present
 *                               in the TTLM IE
 * @expected_duration_present: Indicates if expected duration present in the
 *                             TTLM IE
 * @mapping_switch_time: Mapping switch time of this TTLM IE
 * @expected_duration: Expected duration of this TTLM IE
 */
struct ttlm_info {
	enum ttlm_dir direction;
	bool default_link_mapping;
	u16 ieee_link_map_tid[NUM_MAX_TIDS];
	u8 link_mapping_size;
	bool mapping_switch_time_present;
	bool expected_duration_present;
	u16 mapping_switch_time;
	u32 expected_duration;
};

/**
 * struct mlo_ttlm_ie - TTLM information
 *
 * @disabled_link_bitmap: Bitmap of disabled links. This is used to update the
 *			  disabled link field of RNR IE
 * @ttlm: TTLM info structure
 */
struct mlo_ttlm_ie {
	uint16_t disabled_link_bitmap;
	struct ttlm_info ttlm;
};

/**
 * struct ttlm_context - TTLM IE information
 *
 * @established_ttlm: Indicates the already established broadcast TTLM IE
 *                    advertised by the AP in beacon/probe response frames.
 *                    In this TTLM IE, expected duration flag is set to 1 and
 *                    mapping switch time present flag is set to 0 when the
 *                    mapping is non-default.
 * @upcoming_ttlm: Indicates the new broadcast TTLM IE advertised by the AP in
 *                 beacon/probe response frames. STA needs to use this mapping
 *                 when expected duration in the established TTLM is expires.
 * @established_t2lm_ed_modified_in_case_of_cac: Indicates established expected
 *                 duration was updated due to CAC.
 */
struct ttlm_context {
	struct mlo_ttlm_ie established_ttlm;
	struct mlo_ttlm_ie upcoming_ttlm;
#ifdef CONFIG_QCN_EXTN
	bool established_t2lm_ed_modified_in_case_of_cac;
#endif /* CONFIG_QCN_EXTN */
};

/**
 * enum ttlm_resp_type - TTLM status corresponds to TTLM response frame
 *
 * @TTLM_RESP_TYPE_SUCCESS: TTLM mapping provided in the TTLM request is
 *                       accepted by AP
 * @TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING: TTLM Request denied because
 *                       the requested TID-to-link mapping is unacceptable.
 * @TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING: TTLM Request rejected and
 *                       preferred TID-to-link mapping is suggested.
 * @TTLM_RESP_TYPE_INVALID: Status code is not applicable.
 */
enum ttlm_resp_type {
	TTLM_RESP_TYPE_SUCCESS = 0,
	TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING = 133,
	TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING = 134,
	TTLM_RESP_TYPE_INVALID,
};

/**
 * struct ttlm_onging_negotiation_info - Current ongoing TTLM negotiation
 * (information about transmitted TTLM request/response frame)
 *
 * @dialog_token: Save the dialog token used in TTLM request and response frame.
 * @ttlm_info: Provides the TID-to-link mapping info for UL/DL/BiDi
 * @ttlm_resp_type: TTLM status corresponds to TTLM response frame.
 */
struct ttlm_ongoing_negotiation_info {
	u8 dialog_token;
	struct ttlm_info ttlm_info[TTLM_DIRECTION_MAX];
	enum ttlm_resp_type ttlm_resp_type;
};

/**
 * struct ttlm_prev_negotiated_info - Previous successful TTLM negotiation
 * is saved here.
 *
 * @dialog_token: Save the dialog token used in TTLM request and response frame.
 * @ttlm_info: Provides the TID to LINK mapping information
 */
struct ttlm_prev_negotiated_info {
	u8 dialog_token;
	struct ttlm_info ttlm_info[TTLM_DIRECTION_MAX];
};

/**
 * struct tid_to_link_map_info - TID-to-link mapping information
 *
 * @dialog_token: self generated dialog token used to send TTLM request
 * frame.
 * @ttlm_ongoing_negotiation_info: This has the ongoing TID-to-link mapping info
 * transmitted by this peer to the connected peer.
 * @ttlm_prev_negotiated_info: Previous successful TTLM negotiation is saved here.
 */
struct tid_to_link_map_info {
	u8 dialog_token;
	struct ttlm_ongoing_negotiation_info ttlm_ongoing_negotiation_info;
	struct ttlm_prev_negotiated_info ttlm_prev_negotiated_info;
};

/**
 * struct tid_to_link_mapping_elem - TID-to-link mapping IE
 * @elem_id: TTLM IE
 * @elem_len: TTLM IE len
 * @elem_id_extn: TTLM extension id
 * @data: Variable length data described below
 */
struct tid_to_link_mapping_elem {
	u8 elem_id;
	u8 elem_len;
	u8 elem_id_extn;
	u8 data[];
} STRUCT_PACKED;

/**
 * struct ttlm_of_direction - TID-to-link mapping for a given direction
 * @num_tids: Total number of TIDs for which mapping is given
 * @direction: direction, DL/UL/BIDI
 * @default_mapping: default tid-to-link mapping value
 * @map_tid_to_links: link mappings of the TIDs
 * @link_mapping_size: link map size for TIDn where value 0:2 bytes 1:1 byte
 */
struct ttlm_of_direction {
	u8 num_tids;
	u8 direction;
	u8 default_mapping;
	u8 link_mapping_size;
	struct tid_to_link_map map_tid_to_links[NUM_MAX_TIDS];
} STRUCT_PACKED;

/**
 * struct ml_traffic_indication_elem - Multi-link traffic indication element
 * @elem_id: Multi-link traffic indication IE
 * @elem_len: Multi-link traffic indication IE len
 * @elem_id_extn: Multi-link traffic indication extension id
 * @ml_traffic_ind_control: Multi-link traffic indication control
 * @per_link_traffic_ind_list: Indicates the per-link traffic indication. Each
 *				bit in the per_link_traffic_ind_list corresponds
 *				to a link of the MLD.
 */
struct ml_traffic_indication_elem {
	u8 elem_id;
	u8 elem_len;
	u8 elem_id_extn;
	le16 ml_traffic_ind_control;
	le16 per_link_traffic_ind_list[];
} STRUCT_PACKED;

/**
 * struct ttlm_config - User configured TTLM params
 * @ttlm_direction: TID-to-link mapping params for DL/UL/BIDI
 */
struct ttlm_config {
	struct ttlm_of_direction ttlm_direction[TTLM_DIRECTION_MAX];
};

int hostapd_send_ttlm_req(struct hostapd_data *hapd,
			  struct ttlm_ongoing_negotiation_info *ttlm_negotiation,
			  struct sta_info *sta);
u8 *hostapd_add_ttlm_info_elem(u8 *pos, struct ttlm_info *ttlm,
			       struct hostapd_data *hapd);
int hostapd_get_ttlm_elem_len(struct ttlm_info *ttlm);
int hostapd_build_ttlm_elem(struct ttlm_ongoing_negotiation_info *ttlm,
			    u8 **ttlm_elem, size_t *ttlm_elem_len);
int hostapd_handle_ttlm_resp(struct hostapd_data *hapd, struct sta_info *sta,
			     const u8 *buf, size_t len);
int hostapd_apply_ttlm_mapping_to_driver(struct hostapd_data *hapd, struct sta_info *sta);
int hostapd_handle_ttlm_assoc_req(struct hostapd_data *hapd, const struct ieee80211_mgmt *mgmt,
				  size_t len, struct sta_info *sta, const u8 *ie, size_t ie_len);
bool hostapd_is_mapping_homogeneous(struct ttlm_ongoing_negotiation_info *ongoing_ttlm);
void hostapd_handle_ttlm_req(struct hostapd_data *hapd, struct sta_info *sta,
			     const u8 *buf, size_t len);
int hostapd_ttlm_resp_tx_status(struct hostapd_data *hapd, struct sta_info *sta, int ok);
int hostapd_send_ttlm_resp_action(struct hostapd_data *hapd,
				  struct sta_info *sta);
int hostapd_ttlm_teardown_tx_status(struct hostapd_data *hapd, struct sta_info *sta, int ok);
int hostapd_send_ttlm_teardown(struct hostapd_data *hapd, struct sta_info *sta);
int hostapd_handle_ttlm_teardown(struct hostapd_data *hapd, struct sta_info *sta,
				 const u8 *buf, size_t len);
void hostapd_ttlm_handle_mapping_switch_time_expiry(struct ttlm_context *ttlm_ctx,
						    u8 link_id);
void hostapd_ttlm_handle_expected_duration_expiry(struct ttlm_context *ttlm_ctx,
						  u8 link_id);
int hostapd_send_advertised_ttlm(struct hostapd_data *hapd, struct mlo_ttlm_ie *ttlm_conf);
bool is_valid_negotiated_ttlm(struct mlo_ttlm_ie *established_ttlm,
			      struct ttlm_ongoing_negotiation_info *neg_info);
bool is_sta_ttlm_capable(struct sta_info *sta);
bool hostapd_is_ttlm_active(struct sta_info *sta);
int hostapd_fill_ttlm_params(struct ttlm_info *upcoming_info,
			     struct ttlm_info *established_info,
			     struct drv_adv_ttlm_params *upcoming_ttlm_params,
			     struct drv_adv_ttlm_params *established_ttlm_params);
int hostapd_offload_set_advertised_ttlm(struct hostapd_data *hapd,
					struct mlo_ttlm_ie *upcoming_ttlm,
					struct drv_adv_ttlm_params *upcoming_ttlm_params,
					struct drv_adv_ttlm_params *established_ttlm_params);
int hostapd_offload_set_adv_ttlm_mbssid_enhanced(struct hostapd_data *hapd);
int hostapd_offload_set_adv_ttlm_multi_mbssid(struct hostapd_data *hapd);
#endif /* TTLM_H */
