/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef QCA808X_PTP_REG_API_H
#define QCA808X_PTP_REG_API_H

#include "nss_phy_linux_wrapper.h"
#include "nss_phy_ptp_reg.h"

#define PTP_REG_BASE_ADDR    0x3000

typedef enum {
	PTP_TS_RX0 = 0,
	PTP_TS_RX1,
	PTP_TS_RX2,
	PTP_TS_RX3,
	PTP_TS_TX0,
} ptp_ts_type_t;

int
nss_ptp_imr_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_imr_reg_u *value);

int
nss_ptp_imr_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_imr_reg_u *value);

int
nss_ptp_isr_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_isr_reg_u *value);

int
nss_ptp_isr_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_isr_reg_u *value);

int
nss_ptp_hw_enable_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_hw_enable_reg_u *value);

int
nss_ptp_hw_enable_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_hw_enable_reg_u *value);

int
nss_ptp_main_conf_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_main_conf_reg_u *value);

int
nss_ptp_main_conf_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_main_conf_reg_u *value);

int
nss_ptp_seqid_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 *value);

int
nss_ptp_seqid_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 value);

int
nss_ptp_rtc_clk_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_clk_reg_u *value);

int
nss_ptp_rtc_clk_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_clk_reg_u *value);

int
nss_ptp_portid_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 *clock_id, u16 *port_num);

int
nss_ptp_portid_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 clock_id, u16 port_num);

int
nss_ptp_ts_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 *sec, u32 *nsec, u32 *fsec);

int
nss_ptp_ts_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 sec, u32 nsec, u32 fsec);

int
nss_ptp_msg_type_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 *msg_type);

int
nss_ptp_msg_type_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 msg_type);

int
nss_ptp_orig_corr0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr0_reg_u *value);

int
nss_ptp_orig_corr0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr0_reg_u *value);

int
nss_ptp_orig_corr1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr1_reg_u *value);

int
nss_ptp_orig_corr1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr1_reg_u *value);

int
nss_ptp_orig_corr2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr2_reg_u *value);

int
nss_ptp_orig_corr2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr2_reg_u *value);

int
nss_ptp_orig_corr3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr3_reg_u *value);

int
nss_ptp_orig_corr3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_orig_corr3_reg_u *value);

int
nss_ptp_in_trig0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig0_reg_u *value);

int
nss_ptp_in_trig0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig0_reg_u *value);

int
nss_ptp_in_trig1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig1_reg_u *value);

int
nss_ptp_in_trig1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig1_reg_u *value);

int
nss_ptp_in_trig2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig2_reg_u *value);

int
nss_ptp_in_trig2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig2_reg_u *value);

int
nss_ptp_in_trig3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig3_reg_u *value);

int
nss_ptp_in_trig3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig3_reg_u *value);

int
nss_ptp_tx_latency_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_latency_reg_u *value);

int
nss_ptp_tx_latency_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_latency_reg_u *value);

int
nss_ptp_rtc_inc0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc0_reg_u *value);

int
nss_ptp_rtc_inc0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc0_reg_u *value);

int
nss_ptp_rtc_inc1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc1_reg_u *value);

int
nss_ptp_rtc_inc1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc1_reg_u *value);

int
nss_ptp_rtcoffs0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs0_reg_u *value);

int
nss_ptp_rtcoffs0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs0_reg_u *value);

int
nss_ptp_rtcoffs1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs1_reg_u *value);

int
nss_ptp_rtcoffs1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs1_reg_u *value);

int
nss_ptp_rtcoffs2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs2_reg_u *value);

int
nss_ptp_rtcoffs2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs2_reg_u *value);

int
nss_ptp_rtcoffs3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs3_reg_u *value);

int
nss_ptp_rtcoffs3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs3_reg_u *value);

int
nss_ptp_rtcoffs4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs4_reg_u *value);

int
nss_ptp_rtcoffs4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs4_reg_u *value);

int
nss_ptp_rtc0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc0_reg_u *value);

int
nss_ptp_rtc0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc0_reg_u *value);

int
nss_ptp_rtc1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc1_reg_u *value);

int
nss_ptp_rtc1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc1_reg_u *value);

int
nss_ptp_rtc2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc2_reg_u *value);

int
nss_ptp_rtc2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc2_reg_u *value);

int
nss_ptp_rtc3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc3_reg_u *value);

int
nss_ptp_rtc3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc3_reg_u *value);

int
nss_ptp_rtc4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc4_reg_u *value);

int
nss_ptp_rtc4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc4_reg_u *value);

int
nss_ptp_rtc5_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc5_reg_u *value);

int
nss_ptp_rtc5_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc5_reg_u *value);

int
nss_ptp_rtc6_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc6_reg_u *value);

int
nss_ptp_rtc6_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc6_reg_u *value);

int
nss_ptp_rtcoffs_valid_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs_valid_reg_u *value);

int
nss_ptp_rtcoffs_valid_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs_valid_reg_u *value);

int
nss_ptp_misc_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_config_reg_u *value);

int
nss_ptp_misc_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_config_reg_u *value);

int
nss_ptp_ext_imr_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_imr_reg_u *value);

int
nss_ptp_ext_imr_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_imr_reg_u *value);

int
nss_ptp_ext_isr_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_isr_reg_u *value);

int
nss_ptp_ext_isr_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_isr_reg_u *value);

int
nss_ptp_rtc_ext_conf_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_ext_conf_reg_u *value);

int
nss_ptp_rtc_ext_conf_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_ext_conf_reg_u *value);

int
nss_ptp_rtc_preloaded0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded0_reg_u *value);

int
nss_ptp_rtc_preloaded0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded0_reg_u *value);

int
nss_ptp_rtc_preloaded1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded1_reg_u *value);

int
nss_ptp_rtc_preloaded1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded1_reg_u *value);

int
nss_ptp_rtc_preloaded2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded2_reg_u *value);

int
nss_ptp_rtc_preloaded2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded2_reg_u *value);

int
nss_ptp_rtc_preloaded3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded3_reg_u *value);

int
nss_ptp_rtc_preloaded3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded3_reg_u *value);

int
nss_ptp_rtc_preloaded4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded4_reg_u *value);

int
nss_ptp_rtc_preloaded4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded4_reg_u *value);

int
nss_ptp_gm_conf0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf0_reg_u *value);

int
nss_ptp_gm_conf0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf0_reg_u *value);

int
nss_ptp_gm_conf1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf1_reg_u *value);

int
nss_ptp_gm_conf1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf1_reg_u *value);

int
nss_ptp_ppsin_ts0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts0_reg_u *value);

int
nss_ptp_ppsin_ts0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts0_reg_u *value);

int
nss_ptp_ppsin_ts1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts1_reg_u *value);

int
nss_ptp_ppsin_ts1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts1_reg_u *value);

int
nss_ptp_ppsin_ts2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts2_reg_u *value);

int
nss_ptp_ppsin_ts2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts2_reg_u *value);

int
nss_ptp_ppsin_ts3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts3_reg_u *value);

int
nss_ptp_ppsin_ts3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts3_reg_u *value);

int
nss_ptp_ppsin_ts4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts4_reg_u *value);

int
nss_ptp_ppsin_ts4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts4_reg_u *value);

int
nss_ptp_hwpll_inc0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc0_reg_u *value);

int
nss_ptp_hwpll_inc0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc0_reg_u *value);

int
nss_ptp_hwpll_inc1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc1_reg_u *value);

int
nss_ptp_hwpll_inc1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc1_reg_u *value);

int
nss_ptp_ppsin_latency_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_latency_reg_u *value);

int
nss_ptp_ppsin_latency_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_latency_reg_u *value);

int
nss_ptp_trigger0_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_config_reg_u *value);

int
nss_ptp_trigger0_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_config_reg_u *value);

int
nss_ptp_trigger0_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_status_reg_u *value);

int
nss_ptp_trigger0_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_status_reg_u *value);

int
nss_ptp_trigger1_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_config_reg_u *value);

int
nss_ptp_trigger1_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_config_reg_u *value);

int
nss_ptp_trigger1_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_status_reg_u *value);

int
nss_ptp_trigger1_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_status_reg_u *value);

int
nss_ptp_trigger0_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp0_reg_u *value);

int
nss_ptp_trigger0_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp0_reg_u *value);

int
nss_ptp_trigger0_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp1_reg_u *value);

int
nss_ptp_trigger0_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp1_reg_u *value);

int
nss_ptp_trigger0_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp2_reg_u *value);

int
nss_ptp_trigger0_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp2_reg_u *value);

int
nss_ptp_trigger0_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp3_reg_u *value);

int
nss_ptp_trigger0_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp3_reg_u *value);

int
nss_ptp_trigger0_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp4_reg_u *value);

int
nss_ptp_trigger0_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp4_reg_u *value);

int
nss_ptp_trigger1_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp0_reg_u *value);

int
nss_ptp_trigger1_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp0_reg_u *value);

int
nss_ptp_trigger1_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp1_reg_u *value);

int
nss_ptp_trigger1_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp1_reg_u *value);

int
nss_ptp_trigger1_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp2_reg_u *value);

int
nss_ptp_trigger1_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp2_reg_u *value);

int
nss_ptp_trigger1_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp3_reg_u *value);

int
nss_ptp_trigger1_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp3_reg_u *value);

int
nss_ptp_trigger1_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp4_reg_u *value);

int
nss_ptp_trigger1_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp4_reg_u *value);

int
nss_ptp_event0_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_config_reg_u *value);

int
nss_ptp_event0_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_config_reg_u *value);

int
nss_ptp_event0_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_status_reg_u *value);

int
nss_ptp_event0_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_status_reg_u *value);

int
nss_ptp_event1_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_config_reg_u *value);

int
nss_ptp_event1_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_config_reg_u *value);

int
nss_ptp_event1_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_status_reg_u *value);

int
nss_ptp_event1_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_status_reg_u *value);

int
nss_ptp_event0_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp0_reg_u *value);

int
nss_ptp_event0_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp0_reg_u *value);

int
nss_ptp_event0_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp1_reg_u *value);

int
nss_ptp_event0_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp1_reg_u *value);

int
nss_ptp_event0_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp2_reg_u *value);

int
nss_ptp_event0_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp2_reg_u *value);

int
nss_ptp_event0_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp3_reg_u *value);

int
nss_ptp_event0_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp3_reg_u *value);

int
nss_ptp_event0_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp4_reg_u *value);

int
nss_ptp_event0_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp4_reg_u *value);

int
nss_ptp_event1_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp0_reg_u *value);

int
nss_ptp_event1_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp0_reg_u *value);

int
nss_ptp_event1_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp1_reg_u *value);

int
nss_ptp_event1_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp1_reg_u *value);

int
nss_ptp_event1_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp2_reg_u *value);

int
nss_ptp_event1_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp2_reg_u *value);

int
nss_ptp_event1_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp3_reg_u *value);

int
nss_ptp_event1_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp3_reg_u *value);

int
nss_ptp_event1_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp4_reg_u *value);

int
nss_ptp_event1_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp4_reg_u *value);

int
nss_ptp_gm_conf0_reg_grandmaster_mode_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_phase_adjust_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_0_reg_u *value);

int
nss_ptp_phase_adjust_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_0_reg_u *value);

int
nss_ptp_phase_adjust_0_reg_phase_value_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_phase_adjust_0_reg_phase_value_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_phase_adjust_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_1_reg_u *value);

int
nss_ptp_phase_adjust_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_1_reg_u *value);

int
nss_ptp_phase_adjust_1_reg_phase_value_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_phase_adjust_1_reg_phase_value_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_pps_pul_width_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_0_reg_u *value);

int
nss_ptp_pps_pul_width_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_0_reg_u *value);

int
nss_ptp_pps_pul_width_0_reg_pul_value_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_pps_pul_width_0_reg_pul_value_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_pps_pul_width_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_1_reg_u *value);

int
nss_ptp_pps_pul_width_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_1_reg_u *value);

int
nss_ptp_pps_pul_width_1_reg_pul_value_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_pps_pul_width_1_reg_pul_value_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_freq_waveform_period_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_0_reg_u *value);

int
nss_ptp_freq_waveform_period_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_0_reg_u *value);

int
nss_ptp_freq_waveform_period_0_reg_wave_period_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_freq_waveform_period_0_reg_wave_period_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_freq_waveform_period_0_reg_phase_ali_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_freq_waveform_period_0_reg_phase_ali_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_freq_waveform_period_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_1_reg_u *value);

int
nss_ptp_freq_waveform_period_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_1_reg_u *value);

int
nss_ptp_freq_waveform_period_1_reg_wave_period_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_freq_waveform_period_1_reg_wave_period_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_freq_waveform_period_2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_2_reg_u *value);

int
nss_ptp_freq_waveform_period_2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_2_reg_u *value);

int
nss_ptp_freq_waveform_period_2_reg_wave_period_get(
		struct nss_phy_device *nss_phydev,
		unsigned int *value);

int
nss_ptp_freq_waveform_period_2_reg_wave_period_set(
		struct nss_phy_device *nss_phydev,
		unsigned int value);

int
nss_ptp_rx_com_ts_ctrl_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_ctrl_reg_u *value);

int
nss_ptp_rx_com_ts_ctrl_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_ctrl_reg_u *value);

int
nss_ptp_rx_filt_mac_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da0_reg_u *value);

int
nss_ptp_rx_filt_mac_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da0_reg_u *value);

int
nss_ptp_rx_filt_mac_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da1_reg_u *value);

int
nss_ptp_rx_filt_mac_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da1_reg_u *value);

int
nss_ptp_rx_filt_mac_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da2_reg_u *value);

int
nss_ptp_rx_filt_mac_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da2_reg_u *value);

int
nss_ptp_rx_filt_ipv4_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da0_reg_u *value);

int
nss_ptp_rx_filt_ipv4_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da0_reg_u *value);

int
nss_ptp_rx_filt_ipv4_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da1_reg_u *value);

int
nss_ptp_rx_filt_ipv4_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da1_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da0_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da0_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da1_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da1_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da2_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da2_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da3_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da3_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da4_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da4_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da5_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da5_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da5_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da5_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da6_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da6_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da6_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da6_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da7_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da7_reg_u *value);

int
nss_ptp_rx_filt_ipv6_da7_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da7_reg_u *value);

int
nss_ptp_rx_filt_mac_lengthtype_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_lengthtype_reg_u *value);

int
nss_ptp_rx_filt_mac_lengthtype_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_lengthtype_reg_u *value);

int
nss_ptp_rx_filt_layer4_protocol_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_layer4_protocol_reg_u *value);

int
nss_ptp_rx_filt_layer4_protocol_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_layer4_protocol_reg_u *value);

int
nss_ptp_rx_filt_udp_port_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_udp_port_reg_u *value);

int
nss_ptp_rx_filt_udp_port_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_udp_port_reg_u *value);

int
nss_ptp_rx_com_ts_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_reg_u *value);

int
nss_ptp_rx_com_ts_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_reg_u *value);

int
nss_ptp_rx_com_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp0_reg_u *value);

int
nss_ptp_rx_com_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp0_reg_u *value);

int
nss_ptp_rx_com_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp1_reg_u *value);

int
nss_ptp_rx_com_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp1_reg_u *value);

int
nss_ptp_rx_com_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp2_reg_u *value);

int
nss_ptp_rx_com_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp2_reg_u *value);

int
nss_ptp_rx_com_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp3_reg_u *value);

int
nss_ptp_rx_com_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp3_reg_u *value);

int
nss_ptp_rx_com_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp4_reg_u *value);

int
nss_ptp_rx_com_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp4_reg_u *value);

int
nss_ptp_rx_com_frac_nano_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_reg_u *value);

int
nss_ptp_rx_com_frac_nano_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre0_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre0_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre1_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre1_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre2_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre2_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre3_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre3_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre4_reg_u *value);

int
nss_ptp_rx_com_timestamp_pre4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre4_reg_u *value);

int
nss_ptp_rx_com_frac_nano_pre_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_pre_reg_u *value);

int
nss_ptp_rx_com_frac_nano_pre_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_pre_reg_u *value);

int
nss_ptp_rx_y1731_identify_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_reg_u *value);

int
nss_ptp_rx_y1731_identify_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_reg_u *value);

int
nss_ptp_rx_y1731_identify_pre_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_pre_reg_u *value);

int
nss_ptp_rx_y1731_identify_pre_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_pre_reg_u *value);

int
nss_ptp_tx_com_ts_ctrl_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_ctrl_reg_u *value);

int
nss_ptp_tx_com_ts_ctrl_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_ctrl_reg_u *value);

int
nss_ptp_tx_filt_mac_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da0_reg_u *value);

int
nss_ptp_tx_filt_mac_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da0_reg_u *value);

int
nss_ptp_tx_filt_mac_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da1_reg_u *value);

int
nss_ptp_tx_filt_mac_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da1_reg_u *value);

int
nss_ptp_tx_filt_mac_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da2_reg_u *value);

int
nss_ptp_tx_filt_mac_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da2_reg_u *value);

int
nss_ptp_tx_filt_ipv4_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da0_reg_u *value);

int
nss_ptp_tx_filt_ipv4_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da0_reg_u *value);

int
nss_ptp_tx_filt_ipv4_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da1_reg_u *value);

int
nss_ptp_tx_filt_ipv4_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da1_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da0_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da0_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da1_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da1_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da2_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da2_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da3_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da3_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da4_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da4_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da5_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da5_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da5_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da5_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da6_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da6_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da6_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da6_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da7_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da7_reg_u *value);

int
nss_ptp_tx_filt_ipv6_da7_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da7_reg_u *value);

int
nss_ptp_tx_filt_mac_lengthtype_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_lengthtype_reg_u *value);

int
nss_ptp_tx_filt_mac_lengthtype_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_lengthtype_reg_u *value);

int
nss_ptp_tx_filt_layer4_protocol_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_layer4_protocol_reg_u *value);

int
nss_ptp_tx_filt_layer4_protocol_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_layer4_protocol_reg_u *value);

int
nss_ptp_tx_filt_udp_port_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_udp_port_reg_u *value);

int
nss_ptp_tx_filt_udp_port_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_udp_port_reg_u *value);

int
nss_ptp_tx_com_ts_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_status_reg_u *value);

int
nss_ptp_tx_com_ts_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_status_reg_u *value);

int
nss_ptp_tx_com_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp0_reg_u *value);

int
nss_ptp_tx_com_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp0_reg_u *value);

int
nss_ptp_tx_com_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp1_reg_u *value);

int
nss_ptp_tx_com_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp1_reg_u *value);

int
nss_ptp_tx_com_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp2_reg_u *value);

int
nss_ptp_tx_com_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp2_reg_u *value);

int
nss_ptp_tx_com_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp3_reg_u *value);

int
nss_ptp_tx_com_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp3_reg_u *value);

int
nss_ptp_tx_com_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp4_reg_u *value);

int
nss_ptp_tx_com_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp4_reg_u *value);

int
nss_ptp_tx_com_frac_nano_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_frac_nano_reg_u *value);

int
nss_ptp_tx_com_frac_nano_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_frac_nano_reg_u *value);

int
nss_ptp_tx_y1731_identify_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_y1731_identify_reg_u *value);

int
nss_ptp_tx_y1731_identify_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_y1731_identify_reg_u *value);

int
nss_ptp_y1731_dm_control_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_y1731_dm_control_reg_u *value);

int
nss_ptp_y1731_dm_control_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_y1731_dm_control_reg_u *value);

int
nss_ptp_rx_com_ts_status_pre_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_pre_reg_u *value);

int
nss_ptp_rx_com_ts_status_pre_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_pre_reg_u *value);

int
nss_ptp_baud_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_baud_config_reg_u *value);

int
nss_ptp_baud_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_baud_config_reg_u *value);

int
nss_ptp_uart_configuration_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_uart_configuration_reg_u *value);

int
nss_ptp_uart_configuration_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_uart_configuration_reg_u *value);

int nss_ptp_clock_synce_clock_enable(
		struct nss_phy_device *nss_phydev,
		bool enable);
int
nss_ptp_reset_buffer_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_reset_buffer_reg_u *value);

int
nss_ptp_reset_buffer_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_reset_buffer_reg_u *value);

int
nss_ptp_buffer_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_buffer_status_reg_u *value);

int
nss_ptp_buffer_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_buffer_status_reg_u *value);

int
nss_ptp_tx_buffer_write_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_buffer_write_reg_u *value);

int
nss_ptp_tx_buffer_write_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_buffer_write_reg_u *value);

int
nss_ptp_rx_buffer_read_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_buffer_read_reg_u *value);

int
nss_ptp_rx_buffer_read_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_buffer_read_reg_u *value);

int
nss_ptp_loc_mac_addr_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_0_reg_u *value);

int
nss_ptp_loc_mac_addr_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_0_reg_u *value);

int
nss_ptp_loc_mac_addr_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_1_reg_u *value);

int
nss_ptp_loc_mac_addr_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_1_reg_u *value);

int
nss_ptp_loc_mac_addr_2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_2_reg_u *value);

int
nss_ptp_loc_mac_addr_2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_2_reg_u *value);

int
nss_ptp_link_delay_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_0_reg_u *value);

int
nss_ptp_link_delay_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_0_reg_u *value);

int
nss_ptp_link_delay_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_1_reg_u *value);

int
nss_ptp_link_delay_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_1_reg_u *value);

int
nss_ptp_misc_control_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_control_reg_u *value);

int
nss_ptp_misc_control_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_control_reg_u *value);

int
nss_ptp_ingress_asymmetry_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_0_reg_u *value);

int
nss_ptp_ingress_asymmetry_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_0_reg_u *value);

int
nss_ptp_ingress_asymmetry_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_1_reg_u *value);

int
nss_ptp_ingress_asymmetry_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_1_reg_u *value);

int
nss_ptp_egress_asymmetry_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_0_reg_u *value);

int
nss_ptp_egress_asymmetry_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_0_reg_u *value);

int
nss_ptp_egress_asymmetry_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_1_reg_u *value);

int
nss_ptp_egress_asymmetry_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_1_reg_u *value);

int
nss_ptp_backup_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_backup_reg_u *value);

int
nss_ptp_backup_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_backup_reg_u *value);

int
nss_ptp_version_reg_get(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_version_reg_u *value);
int
nss_ptp_msg_type_spec0_reg(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_msg_type_spec0_reg_u *value);
int
nss_ptp_msg_type_spec1_reg(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_msg_type_spec1_reg_u *value);
int
nss_ptp_domain_number_reg(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_domain_number_reg_u *value);
#endif

