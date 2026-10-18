/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef RANGING_EXTN_H
#define RANGING_EXTN_H

/* LOWI channel info entry used by the channel-info response builder */
struct ath12k_lowi_wmi_channel {
	__le32 mhz;
	__le32 band_center_freq1;
	__le32 band_center_freq2;
	__le32 info;
	__le32 reg_info_1;
	__le32 reg_info_2;
} __packed;

struct ath12k_lowi_channel_info_entry {
	__le32 chan_id;
	__le32 reserved0;
	struct ath12k_lowi_wmi_channel channel_info;
} __packed;

struct ath12k_wmi_lowi_oem_resp {
	const u8 *data1;
	const u8 *data2;
	u32 len1;
	u32 len2;
	bool indirect;
};

#define ATH12K_LOWI_OEM_TIMEOUT_HZ (3 * HZ)
/* reserved0[7:0] = radio_idx */
#define ATH12K_LOWI_RESERVED0_RADIO_IDX_MASK		0xff
#define ATH12K_LOWI_RESERVED0_RADIO_IDX_SHIFT		0
#define ATH12K_LOWI_RESERVED0_SET_RADIO_IDX(v)		\
	(((v) & ATH12K_LOWI_RESERVED0_RADIO_IDX_MASK) << ATH12K_LOWI_RESERVED0_RADIO_IDX_SHIFT)

/* Bit 10 of ch_info.info indicates DFS — mirrors WIFI_POS_FLAG_DFS */
#define ATH12K_LOWI_CH_INFO_FLAG_DFS			BIT(10)
/* info[5:0] encodes phy_mode (channel mode bits) */
#define ATH12K_LOWI_CH_INFO_MODE_MASK			0x3f
/* reg_info_1[23:16] = reg power (dBm); reg_info_2[15:8] = max tx power */
#define ATH12K_LOWI_REG_INFO1_SET_PWR(v)		(((v) & 0xff) << 16)
#define ATH12K_LOWI_REG_INFO2_SET_PWR(v)		(((v) & 0xff) << 8)

/**
 * ath12k_vendor_send_lowi_oem_data_extn() - Send a LOWI OEM data event to userspace
 * @hw: ieee80211_hw instance whose wiphy the vendor event is sent on
 * @ar: ath12k radio the data was received on, used to fill the LINK_ID
 *      attribute; may be NULL if the data is not associated with a radio
 * @data: OEM response payload to attach as vendor event data, may be NULL
 *        if @len is 0
 * @len: length of @data in bytes
 *
 * Wraps the payload in a QCA_NL80211_VENDOR_SUBCMD_OEM_DATA_INDEX vendor
 * event of type QCA_LOWI_EVT_MEAS_RESULT and delivers it to the LOWI
 * userspace daemon.
 *
 * Return: 0 on success, negative error code on failure.
 */
int ath12k_vendor_send_lowi_oem_data_extn(struct ieee80211_hw *hw,
					  struct ath12k *ar,
					  const u8 *data, u32 len);

/**
 * ath12k_wmi_lowi_oem_response_event_extn() - Handle WMI_OEM_RESPONSE_EVENTID
 * @ab: ath12k_base the event was received on
 * @skb: WMI event skb containing the TLV-encoded OEM response
 *
 * Parses the direct-data TLVs out of the firmware's OEM response, stitching
 * together up to two byte arrays into a single buffer when the response is
 * split across TLVs, then forwards the result to userspace via
 * ath12k_vendor_send_lowi_oem_data_extn(). Indirect DMA data is not
 * supported and is logged as a warning. Completes ar_extn.lowi_oem_done to
 * unblock any caller waiting in ath12k_wmi_send_lowi_oem_data_extn().
 */
void ath12k_wmi_lowi_oem_response_event_extn(struct ath12k_base *ab,
					     struct sk_buff *skb);

#endif /* RANGING_EXTN_H */
