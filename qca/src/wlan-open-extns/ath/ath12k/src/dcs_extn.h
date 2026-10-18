/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DCS_EXTN_H
#define ATH12K_DCS_EXTN_H

/* Valid DCS interference bitmap: CW, WLAN */
#define ATH12K_DCS_VALID_INTF_BITMAP (WMI_DCS_CW_INTF | \
                                     WMI_DCS_WLAN_INTF| \
				     WMI_DCS_OBSS_INTF)
#define ATH12K_MAX_20MHZ_SEGMENTS       16

#define ATH12K_CHWIDTH_1                1   /* Channel width 1 MHz */
#define ATH12K_CHWIDTH_2                2   /* Channel width 2 MHz */
#define ATH12K_CHWIDTH_4                4   /* Channel width 4 MHz */
#define ATH12K_CHWIDTH_5                5   /* Channel width 5 MHz */
#define ATH12K_CHWIDTH_8                8   /* Channel width 8 MHz */
#define ATH12K_CHWIDTH_10               10  /* Channel width 10 MHz */
#define ATH12K_CHWIDTH_16               16  /* Channel width 16 MHz */
#define ATH12K_CHWIDTH_20               20  /* Channel width 20 MHz */
#define ATH12K_CHWIDTH_40               40  /* Channel width 40 MHz */
#define ATH12K_CHWIDTH_80               80  /* Channel width 80 MHz */
#define ATH12K_CHWIDTH_160              160 /* Channel width 160 MHz */
#define ATH12K_CHWIDTH_320              320 /* Channel width 320 MHz */

#define ATH12K_WLAN_INTR_CMD	1

static const int intf_map_80[4][4] = {
	{ 0, 1, 2, 3 },
	{ 1, 0, 2, 3 },
	{ 2, 3, 0, 1 },
	{ 3, 2, 0, 1 }
};

static const int intf_map_160[8][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 1, 0, 2, 3, 4, 5, 6, 7 },
	{ 2, 3, 0, 1, 4, 5, 6, 7 },
	{ 3, 2, 0, 1, 4, 5, 6, 7 },
	{ 4, 5, 6, 7, 0, 1, 2, 3 },
	{ 5, 4, 6, 7, 0, 1, 2, 3 },
	{ 6, 7, 4, 5, 0, 1, 2, 3 },
	{ 7, 6, 4, 5, 0, 1, 2, 3 }
};

static const int intf_map_320[16][16] = {
	{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 1,  0,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 2,  3,  0,  1,  4,  5,  6,  7,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 3,  2,  0,  1,  4,  5,  6,  7,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 4,  5,  6,  7,  0,  1,  2,  3,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 5,  4,  6,  7,  0,  1,  2,  3,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 6,  7,  4,  5,  0,  1,  2,  3,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 7,  6,  4,  5,  0,  1,  2,  3,  8,  9,  10,  11,  12,  13,  14,  15 },
	{ 8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 9,  8,  10, 11, 12, 13, 14, 15, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 10, 11, 8,  9,  12, 13, 14, 15, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 11, 10, 8,  9,  12, 13, 14, 15, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 12, 13, 14, 15, 8,  9,  10, 11, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 13, 12, 14, 15, 8,  9,  10, 11, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 14, 15, 12, 13, 8,  9,  10, 11, 0,  1,  2,   3,   4,   5,   6,   7  },
	{ 15, 14, 12, 13, 8,  9,  10, 11, 0,  1,  2,   3,   4,   5,   6,   7  }
};

enum ath12k_dcs_interference_chan_segment {
	ATH12K_DCS_SEG_PRI20                 = 0x1,
	ATH12K_DCS_SEG_SEC20                 = 0x2,
	ATH12K_DCS_SEG_SEC40_LOW             = 0x4,
	ATH12K_DCS_SEG_SEC40_UP              = 0x8,
	ATH12K_DCS_SEG_SEC40                 = 0xC,
	ATH12K_DCS_SEG_SEC80_LOW             = 0x10,
	ATH12K_DCS_SEG_SEC80_LOW_UP          = 0x20,
	ATH12K_DCS_SEG_SEC80_UP_LOW          = 0x40,
	ATH12K_DCS_SEG_SEC80_UP              = 0x80,
	ATH12K_DCS_SEG_SEC80                 = 0xF0,
	ATH12K_DCS_SEG_SEC160_LOW            = 0x0100,
	ATH12K_DCS_SEG_SEC160_LOW_UP         = 0x0200,
	ATH12K_DCS_SEG_SEC160_LOW_UP_UP      = 0x0400,
	ATH12K_DCS_SEG_SEC160_LOW_UP_UP_UP   = 0x0800,
	ATH12K_DCS_SEG_SEC160_UP_LOW_LOW_LOW = 0x1000,
	ATH12K_DCS_SEG_SEC160_UP_LOW_LOW     = 0x2000,
	ATH12K_DCS_SEG_SEC160_UP_LOW         = 0x4000,
	ATH12K_DCS_SEG_SEC160_UP             = 0x8000,
	ATH12K_DCS_SEG_SEC160                = 0xFF00,
};

struct dcs_wlan_intr_metrics {
	u32 reg_ofdm_phyerr_delta;
	u32 reg_cck_phyerr_delta;
	u32 reg_tsf_delta;
	u32 rxclr_delta;
	u32 rxclr_ext_delta;
	u32 cycle_count_delta;
	u32 tx_frame_delta;
	u32 rx_frame_delta;
	u32 reg_total_cu;
	u32 reg_tx_cu;
	u32 reg_rx_cu;
	u32 reg_unused_cu;
	u32 rx_time_cu;
	u32 reg_ofdm_phyerr_cu;
	u32 ofdm_phy_err_rate;
	u32 cck_phy_err_rate;
	u32 max_phy_err_rate;
	u32 max_phy_err_count;
	u32 total_wasted_cu;
	u32 wasted_tx_cu;
	u32 tx_err;
	int too_many_phy_errors;
};

#ifndef CPTCFG_QCN_EXTN
static inline void ath12k_mac_set_intf_detect(struct ath12k *ar,
					      u16 intf_detect_bitmap)
{
}

static inline void cfg80211_intf_notify_extn(struct wiphy *wiphy,
					     struct ath12k *ar,
					     u16 type,
					     u32 interference_bitmap)
{
}

static inline void ath12k_vendor_send_intf_notify_extn(struct ath12k *ar,
						       u16 type,
						       u32 interference_type);
{
}

static inline void ath12k_wmi_dcs_cw_interference_event_extn(struct ath12k_base *ab,
							     struct sk_buff *skb,
							     u32 pdev_id)
{
}

static inline void
ath12k_wmi_dcs_wlan_interference_event_extn(struct ath12k_base *ab,
					    struct sk_buff *skb,
					    u32 pdev_id)
{
}

static inline void ath12k_wlan_intf_update_stats_extn(struct ath12k *ar,
		      const struct wmi_dcs_wlan_interference_stats *stats)
{
}

static inline void ath12k_wlan_intf_rollover_stats_extn(struct ath12k *ar)
{
}
#else
void ath12k_mac_set_intf_detect(struct ath12k *ar, u16 intf_detect_bitmap);
void cfg80211_intf_notify_extn(struct wiphy *wiphy, struct ath12k *ar,
			       u16 type, u32 interference_bitmap);
void ath12k_vendor_send_intf_notify_extn(struct ath12k *ar, u16 type,
					 u32 interference_type);
void ath12k_wmi_dcs_cw_interference_event_extn(struct ath12k_base *ab,
					       struct sk_buff *skb,
					       u32 pdev_id);
void ath12k_wmi_dcs_wlan_interference_event_extn(struct ath12k_base *ab,
						 struct sk_buff *skb,
						 u32 pdev_id);
void
ath12k_wlan_intf_update_stats_extn(struct ath12k *ar,
			const struct wmi_dcs_wlan_interference_stats *stats);
void ath12k_wlan_intf_rollover_stats_extn(struct ath12k *ar);
#endif
#endif
