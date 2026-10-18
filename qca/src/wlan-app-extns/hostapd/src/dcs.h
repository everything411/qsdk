/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef DCS_H
#define DCS_H

#define BASE_24G_FREQ 2407
#define BASE_5G_FREQ 5000
#define BASE_49G_FREQ 4000

#define ALLOWED_DCS_MASK 0x0013

enum hostapd_dcs_chan_seg {
       DCS_SEG_PRI20       = 0x0001,
       DCS_SEG_SEC20       = 0x0002,
       DCS_SEG_SEC40_L     = 0x0004,
       DCS_SEG_SEC40_U     = 0x0008,
       DCS_SEG_SEC40       = 0x000C,
       DCS_SEG_SEC80_L     = 0x0010,
       DCS_SEG_SEC80_LU    = 0x0020,
       DCS_SEG_SEC80_UL    = 0x0040,
       DCS_SEG_SEC80_U     = 0x0080,
       DCS_SEG_SEC80       = 0x00F0,
       DCS_SEG_SEC160_L    = 0x0100,
       DCS_SEG_SEC160_LU   = 0x0200,
       DCS_SEG_SEC160_LUU  = 0x0400,
       DCS_SEG_SEC160_LUUU = 0x0800,
       DCS_SEG_SEC160_ULLL = 0x1000,
       DCS_SEG_SEC160_ULL  = 0x2000,
       DCS_SEG_SEC160_UL   = 0x4000,
       DCS_SEG_SEC160_U    = 0x8000,
       DCS_SEG_SEC160      = 0xFF00,
};

#define DCS_PHYERR_PENALTY		500
#define DCS_PHYERR_THRESHOLD		300
#define DCS_RADARERR_THRESHOLD		1000
#define DCS_COCH_INTR_THRESHOLD	30
#define DCS_TXERR_THRESHOLD		30
#define DCS_USER_MAX_CU		50
#define DCS_INTR_DETECTION_THR		6
#define DCS_SAMPLE_SIZE		50

int hostapd_drv_dcs_config(struct hostapd_data *hapd, u8 link_id,
			   struct driver_dcs_config *params);
void hostapd_dcs_intf_event_extn(struct hostapd_data *hapd,
				 union wpa_event_data *data);
int hostapd_drv_dcs_sim_trigger(struct hostapd_data *hapd, u8 link_id,
                                struct driver_dcs_sim *params);
int
hostapd_trigger_channel_switch_extn(struct hostapd_iface *iface,
				    struct hostapd_channel_data *chan);

#endif
