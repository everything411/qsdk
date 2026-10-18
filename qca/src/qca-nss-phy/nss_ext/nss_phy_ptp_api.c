/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "nss_phy.h"
#include "nss_phy_lib.h"
#include "nss_phy_linux_wrapper.h"
#include "nss_phy_ptp_api.h"
#include "nss_phy_ptp_reg.h"

static int
nss_phy_ptp_reg_read(struct nss_phy_device *nss_phydev, u32 reg, u16 *val)
{
	int phy_value;

	phy_value = nss_phy_read(nss_phydev, reg);
	if (phy_value < 0)
		return phy_value;

	*val = phy_value;

	return 0;
}

static int
nss_phy_ptp_mmd_read(struct nss_phy_device *nss_phydev, int devad, u32 reg, u16 *val)
{
	int phy_value;

	phy_value = nss_phy_read_mmd(nss_phydev, devad, reg);
	if (phy_value < 0)
		return phy_value;

	*val = phy_value;

	return 0;
}

int
nss_ptp_imr_reg_get(struct nss_phy_device *nss_phydev, union ptp_imr_reg_u *value)
{
	return nss_phy_ptp_reg_read(
			nss_phydev,
			PTP_IMR_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_imr_reg_set(struct nss_phy_device *nss_phydev, union ptp_imr_reg_u *value)
{
	return nss_phy_write(
			nss_phydev,
			PTP_IMR_REG_ADDRESS,
			value->val);
}

int
nss_ptp_isr_reg_get(struct nss_phy_device *nss_phydev, union ptp_isr_reg_u *value)
{
	return nss_phy_ptp_reg_read(
			nss_phydev,
			PTP_ISR_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_isr_reg_set(struct nss_phy_device *nss_phydev, union ptp_isr_reg_u *value)
{
	return nss_phy_write(
			nss_phydev,
			PTP_ISR_REG_ADDRESS,
			value->val);
}

int
nss_ptp_hw_enable_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_hw_enable_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_HW_ENABLE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_hw_enable_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_hw_enable_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_HW_ENABLE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_main_conf_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_main_conf_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_MAIN_CONF_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_main_conf_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_main_conf_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_MAIN_CONF_REG_ADDRESS,
			value->val);
}

int
nss_ptp_seqid_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 *value)
{
	u32 reg = 0;
	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_SEQID0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_SEQID1_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_SEQID2_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_SEQID3_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_SEQID_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, reg,
			value);
}

int
nss_ptp_seqid_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 value)
{
	u32 reg = 0;
	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_SEQID0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_SEQID1_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_SEQID2_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_SEQID3_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_SEQID_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, reg,
			value);
}

int
nss_ptp_portid_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 *clock_id, u16 *port_num)
{
	int rv = 0;
	u32 reg = 0;
	u16 port_id0, port_id1, port_id2, port_id3;

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_PORTID0_0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_PORTID1_0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_PORTID2_0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_PORTID3_0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_PORTID0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg, &port_id0);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 1, &port_id1);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 2, &port_id2);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 3, &port_id3);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 4, port_num);
	if (rv)
		return rv;

	*clock_id = ((u64)port_id0 << 48) | ((u64)port_id1 << 32) |
		((u64)port_id2 << 16) | port_id3;

	return rv;
}

int
nss_ptp_portid_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 clock_id, u16 port_num)
{
	int rv = 0;
	u32 reg = 0;
	u32 port_id0, port_id1, port_id2, port_id3;

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_PORTID0_0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_PORTID1_0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_PORTID2_0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_PORTID3_0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_PORTID0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	port_id0 = (clock_id >> 48) & 0xffff;
	port_id1 = (clock_id >> 32) & 0xffff;
	port_id2 = (clock_id >> 16) & 0xffff;
	port_id3 = clock_id & 0xffff;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg, port_id0);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 1, port_id1);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 2, port_id2);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 3, port_id3);
	if (rv)
		return rv;

	return nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 4, port_num);
}

int
nss_ptp_rtc_clk_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_rtc_clk_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_AN, PTP_RTC_CLK_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_clk_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_rtc_clk_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_AN, PTP_RTC_CLK_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ts_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 *sec, u32 *nsec, u32 *fsec)
{
	int rv = 0;
	u32 reg = 0;
	u16 sec0, sec1, sec2, nsec0, nsec1;
	union ptp_ts5_reg_u ts5 = {0};
	union ptp_ts6_reg_u ts6 = {0};

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_TS0_0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_TS1_0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_TS2_0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_TS3_0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_TS0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg, &sec0);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 1, &sec1);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 2, &sec2);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 3, &nsec0);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 4, &nsec1);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 5, &ts5.val);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 6, &ts6.val);
	if (rv)
		return rv;

	*sec = ((u64)sec0 << 32) | ((u64)sec1 << 16) | sec2;
	*nsec = ((u32)nsec0 << 16) | nsec1;
	*fsec = ((u32)ts5.bf.rx_ts_nfsec << 8) | ts6.bf.rx_ts_nfsec;

	return rv;
}

int
nss_ptp_ts_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u64 sec, u32 nsec, u32 fsec)
{
	int rv = 0;
	u32 reg = 0;
	u32 sec0, sec1, sec2, nsec0, nsec1;
	union ptp_ts5_reg_u ts5 = {0};
	union ptp_ts6_reg_u ts6 = {0};

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_TS0_0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_TS1_0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_TS2_0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_TS3_0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_TS0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	sec0 = (sec >> 32) & 0xffff;
	sec1 = (sec >> 16) & 0xffff;
	sec2 = sec & 0xffff;

	nsec0 = (nsec >> 16) & 0xffff;
	nsec1 = nsec & 0xffff;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg, sec0);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 1, sec1);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 2, sec2);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 3, nsec0);
	if (rv)
		return rv;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 4, nsec1);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 5, &ts5.val);
	if (rv)
		return rv;

	ts5.bf.rx_ts_nfsec = (fsec >> 8) & 0xfff;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 5, ts5.val);
	if (rv)
		return rv;

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 6, &ts6.val);
	if (rv)
		return rv;

	ts6.bf.rx_ts_nfsec = fsec & 0xff;

	return nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 6, ts6.val);
}

int
nss_ptp_msg_type_get(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 *msg_type)
{
	int rv = 0;
	u32 reg = 0;
	union ptp_ts5_reg_u ts5 = {0};

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_TS0_0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_TS1_0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_TS2_0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_TS3_0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_TS0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 5, &ts5.val);
	if (rv)
		return rv;

	*msg_type = ts5.bf.rx_msg_type;

	return rv;
}

int
nss_ptp_msg_type_set(struct nss_phy_device *nss_phydev,
		ptp_ts_type_t ts_type, u16 msg_type)
{
	int rv = 0;
	u32 reg = 0;
	union ptp_ts5_reg_u ts5 = {0}; 

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = PTP_RX_TS0_0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX_TS1_0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX_TS2_0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX_TS3_0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX_TS0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	rv = nss_phy_ptp_mmd_read(nss_phydev, MDIO_MMD_PCS, reg + 5, &ts5.val);
	if (rv)
		return rv;

	ts5.bf.rx_msg_type = msg_type;

	rv = nss_phy_write_mmd(nss_phydev, MDIO_MMD_PCS, reg + 5, ts5.val);

	return rv;
}

int
nss_ptp_orig_corr0_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_orig_corr0_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_orig_corr1_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_orig_corr1_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_orig_corr2_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_orig_corr2_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_orig_corr3_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_orig_corr3_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_orig_corr3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_ORIG_CORR3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_in_trig0_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_in_trig0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_in_trig0_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_in_trig0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_in_trig1_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_in_trig1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_in_trig1_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_in_trig1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_in_trig2_reg_get(struct nss_phy_device *nss_phydev,
		union ptp_in_trig2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_in_trig2_reg_set(struct nss_phy_device *nss_phydev,
		union ptp_in_trig2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_in_trig3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_in_trig3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_in_trig3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_IN_TRIG3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_latency_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_latency_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_LATENCY_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_latency_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_latency_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_LATENCY_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_inc0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_INC0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_inc0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_INC0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_inc1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_INC1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_inc1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_inc1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_INC1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtcoffs0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtcoffs0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtcoffs1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtcoffs1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtcoffs2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtcoffs2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtcoffs3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtcoffs3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtcoffs4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtcoffs4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc5_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc5_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC5_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc5_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc5_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC5_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc6_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc6_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC6_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc6_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc6_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC6_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtcoffs_valid_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs_valid_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS_VALID_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtcoffs_valid_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtcoffs_valid_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTCOFFS_VALID_REG_ADDRESS,
			value->val);
}

int
nss_ptp_misc_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_config_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_MISC_CONFIG_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_misc_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_config_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_MISC_CONFIG_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ext_imr_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_imr_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EXT_IMR_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ext_imr_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_imr_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EXT_IMR_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ext_isr_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_isr_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EXT_ISR_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ext_isr_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ext_isr_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EXT_ISR_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_ext_conf_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_ext_conf_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_EXT_CONF_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_ext_conf_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_ext_conf_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_EXT_CONF_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_preloaded0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_preloaded0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_preloaded1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_preloaded1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_preloaded2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_preloaded2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_preloaded3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_preloaded3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rtc_preloaded4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rtc_preloaded4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rtc_preloaded4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RTC_PRELOADED4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_gm_conf0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_GM_CONF0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_gm_conf0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_GM_CONF0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_gm_conf1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_GM_CONF1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_gm_conf1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_gm_conf1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_GM_CONF1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ppsin_ts0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ppsin_ts0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ppsin_ts1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ppsin_ts1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ppsin_ts2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ppsin_ts2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ppsin_ts3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ppsin_ts3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ppsin_ts4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ppsin_ts4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_ts4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_TS4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_hwpll_inc0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_HWPLL_INC0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_hwpll_inc0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_HWPLL_INC0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_hwpll_inc1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_HWPLL_INC1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_hwpll_inc1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_hwpll_inc1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_HWPLL_INC1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ppsin_latency_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_latency_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_LATENCY_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ppsin_latency_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ppsin_latency_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_PPSIN_LATENCY_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_config_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_CONFIG_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_config_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_CONFIG_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_config_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_CONFIG_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_config_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_CONFIG_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger0_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger0_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger0_timestamp4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER0_TIMESTAMP4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_trigger1_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_trigger1_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_trigger1_timestamp4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TRIGGER1_TIMESTAMP4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_config_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_CONFIG_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_config_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_CONFIG_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_config_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_CONFIG_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_config_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_CONFIG_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event0_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event0_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event0_timestamp4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT0_TIMESTAMP4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_event1_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_event1_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_event1_timestamp4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EVENT1_TIMESTAMP4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_gm_conf0_reg_grandmaster_mode_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_gm_conf0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_gm_conf0_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.grandmaster_mode;
	return ret;
}

int
nss_ptp_phase_adjust_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PHASE_ADJUST_0_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_phase_adjust_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_0_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PHASE_ADJUST_0_REG_ADDRESS,
				value->val);
}

int
nss_ptp_phase_adjust_0_reg_phase_value_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_phase_adjust_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_phase_adjust_0_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.phase_value;
	return ret;
}

int
nss_ptp_phase_adjust_0_reg_phase_value_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_phase_adjust_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_phase_adjust_0_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.phase_value = value;
	ret = nss_ptp_phase_adjust_0_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_phase_adjust_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PHASE_ADJUST_1_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_phase_adjust_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_phase_adjust_1_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PHASE_ADJUST_1_REG_ADDRESS,
				value->val);
}

int
nss_ptp_phase_adjust_1_reg_phase_value_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_phase_adjust_1_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_phase_adjust_1_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.phase_value;
	return ret;
}

int
nss_ptp_phase_adjust_1_reg_phase_value_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_phase_adjust_1_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_phase_adjust_1_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.phase_value = value;
	ret = nss_ptp_phase_adjust_1_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_pps_pul_width_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PPS_PUL_WIDTH_0_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_pps_pul_width_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_0_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PPS_PUL_WIDTH_0_REG_ADDRESS,
				value->val);
}

int
nss_ptp_pps_pul_width_0_reg_pul_value_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_pps_pul_width_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_pps_pul_width_0_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.pul_value;
	return ret;
}

int
nss_ptp_pps_pul_width_0_reg_pul_value_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_pps_pul_width_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_pps_pul_width_0_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.pul_value = value;
	ret = nss_ptp_pps_pul_width_0_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_pps_pul_width_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PPS_PUL_WIDTH_1_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_pps_pul_width_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_pps_pul_width_1_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_PPS_PUL_WIDTH_1_REG_ADDRESS,
				value->val);
}

int
nss_ptp_pps_pul_width_1_reg_pul_value_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_pps_pul_width_1_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_pps_pul_width_1_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.pul_value;
	return ret;
}

int
nss_ptp_pps_pul_width_1_reg_pul_value_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_pps_pul_width_1_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_pps_pul_width_1_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.pul_value = value;
	ret = nss_ptp_pps_pul_width_1_reg_set(nss_phydev, &reg_val);
	return ret;
}


int
nss_ptp_freq_waveform_period_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_FREQ_WAVEFORM_PERIOD_0_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_freq_waveform_period_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_0_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_FREQ_WAVEFORM_PERIOD_0_REG_ADDRESS,
				value->val);
}

int
nss_ptp_freq_waveform_period_0_reg_wave_period_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_freq_waveform_period_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_0_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.wave_period;
	return ret;
}

int
nss_ptp_freq_waveform_period_0_reg_wave_period_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_freq_waveform_period_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_0_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.wave_period = value;
	ret = nss_ptp_freq_waveform_period_0_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_freq_waveform_period_0_reg_phase_ali_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_freq_waveform_period_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_0_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.phase_ali;
	return ret;
}

int
nss_ptp_freq_waveform_period_0_reg_phase_ali_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_freq_waveform_period_0_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_0_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.phase_ali = value;
	ret = nss_ptp_freq_waveform_period_0_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_freq_waveform_period_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_FREQ_WAVEFORM_PERIOD_1_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_freq_waveform_period_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_1_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_FREQ_WAVEFORM_PERIOD_1_REG_ADDRESS,
				value->val);
}

int
nss_ptp_freq_waveform_period_1_reg_wave_period_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_freq_waveform_period_1_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_1_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.wave_period;
	return ret;
}

int
nss_ptp_freq_waveform_period_1_reg_wave_period_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_freq_waveform_period_1_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_1_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.wave_period = value;
	ret = nss_ptp_freq_waveform_period_1_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_freq_waveform_period_2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_FREQ_WAVEFORM_PERIOD_2_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_freq_waveform_period_2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_freq_waveform_period_2_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_FREQ_WAVEFORM_PERIOD_2_REG_ADDRESS,
				value->val);
}

int
nss_ptp_freq_waveform_period_2_reg_wave_period_get(
		struct nss_phy_device *nss_phydev,
		u32 *value)
{
	union ptp_freq_waveform_period_2_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_2_reg_get(nss_phydev, &reg_val);
	*value = reg_val.bf.wave_period;
	return ret;
}

int
nss_ptp_freq_waveform_period_2_reg_wave_period_set(
		struct nss_phy_device *nss_phydev,
		u32 value)
{
	union ptp_freq_waveform_period_2_reg_u reg_val;
	int ret = 0;

	ret = nss_ptp_freq_waveform_period_2_reg_get(nss_phydev, &reg_val);
	if (0 != ret)
		return ret;
	reg_val.bf.wave_period = value;
	ret = nss_ptp_freq_waveform_period_2_reg_set(nss_phydev, &reg_val);
	return ret;
}

int
nss_ptp_rx_com_ts_ctrl_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_ctrl_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TS_CTRL_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_ts_ctrl_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_ctrl_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TS_CTRL_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_mac_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_DA0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_mac_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_DA0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_mac_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_DA1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_mac_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_DA1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_mac_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_DA2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_mac_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_da2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_DA2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv4_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV4_DA0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv4_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV4_DA0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv4_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV4_DA1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv4_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv4_da1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV4_DA1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da5_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da5_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA5_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da5_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da5_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA5_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da6_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da6_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA6_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da6_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da6_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA6_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_ipv6_da7_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da7_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA7_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_ipv6_da7_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_ipv6_da7_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_IPV6_DA7_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_mac_lengthtype_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_lengthtype_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_LENGTHTYPE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_mac_lengthtype_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_mac_lengthtype_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_MAC_LENGTHTYPE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_layer4_protocol_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_layer4_protocol_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_LAYER4_PROTOCOL_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_layer4_protocol_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_layer4_protocol_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_LAYER4_PROTOCOL_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_filt_udp_port_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_udp_port_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_UDP_PORT_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_filt_udp_port_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_filt_udp_port_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_FILT_UDP_PORT_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_ts_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TS_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_ts_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TS_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_frac_nano_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_FRAC_NANO_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_frac_nano_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_FRAC_NANO_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp_pre0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp_pre0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp_pre1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp_pre1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp_pre2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp_pre2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp_pre3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp_pre3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_timestamp_pre4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_timestamp_pre4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_timestamp_pre4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TIMESTAMP_PRE4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_frac_nano_pre_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_pre_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_FRAC_NANO_PRE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_frac_nano_pre_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_frac_nano_pre_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_FRAC_NANO_PRE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_y1731_identify_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_Y1731_IDENTIFY_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_y1731_identify_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_Y1731_IDENTIFY_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_y1731_identify_pre_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_pre_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_Y1731_IDENTIFY_PRE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_y1731_identify_pre_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_y1731_identify_pre_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_Y1731_IDENTIFY_PRE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_ts_ctrl_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_ctrl_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TS_CTRL_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_ts_ctrl_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_ctrl_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TS_CTRL_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_mac_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_DA0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_mac_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_DA0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_mac_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_DA1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_mac_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_DA1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_mac_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_DA2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_mac_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_da2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_DA2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv4_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV4_DA0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv4_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV4_DA0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv4_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV4_DA1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv4_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv4_da1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV4_DA1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da5_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da5_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA5_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da5_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da5_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA5_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da6_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da6_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA6_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da6_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da6_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA6_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_ipv6_da7_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da7_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA7_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_ipv6_da7_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_ipv6_da7_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_IPV6_DA7_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_mac_lengthtype_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_lengthtype_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_LENGTHTYPE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_mac_lengthtype_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_mac_lengthtype_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_MAC_LENGTHTYPE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_layer4_protocol_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_layer4_protocol_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_LAYER4_PROTOCOL_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_layer4_protocol_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_layer4_protocol_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_LAYER4_PROTOCOL_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_filt_udp_port_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_udp_port_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_UDP_PORT_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_filt_udp_port_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_filt_udp_port_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_FILT_UDP_PORT_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_ts_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TS_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_ts_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_ts_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TS_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_timestamp0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_timestamp0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_timestamp1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_timestamp1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_timestamp2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP2_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_timestamp2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp2_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP2_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_timestamp3_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp3_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP3_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_timestamp3_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp3_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP3_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_timestamp4_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp4_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP4_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_timestamp4_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_timestamp4_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_TIMESTAMP4_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_com_frac_nano_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_frac_nano_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_FRAC_NANO_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_com_frac_nano_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_com_frac_nano_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_COM_FRAC_NANO_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_y1731_identify_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_y1731_identify_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_Y1731_IDENTIFY_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_y1731_identify_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_y1731_identify_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_Y1731_IDENTIFY_REG_ADDRESS,
			value->val);
}

int
nss_ptp_y1731_dm_control_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_y1731_dm_control_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_Y1731_DM_CONTROL_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_y1731_dm_control_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_y1731_dm_control_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_Y1731_DM_CONTROL_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_com_ts_status_pre_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_pre_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TS_STATUS_PRE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_com_ts_status_pre_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_com_ts_status_pre_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_COM_TS_STATUS_PRE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_baud_config_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_baud_config_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_BAUD_CONFIG_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_baud_config_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_baud_config_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_BAUD_CONFIG_REG_ADDRESS,
			value->val);
}

int
nss_ptp_uart_configuration_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_uart_configuration_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_UART_CONFIGURATION_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_uart_configuration_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_uart_configuration_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_UART_CONFIGURATION_REG_ADDRESS,
			value->val);
}

#define QCA808X_DEBUG_ANA_CLOCK_CTRL_REG	0x3e80
#define QCA808X_ANALOG_PHY_SYNCE_CLOCK_EN	BIT(5)
#define QCA808X_MMD7_CLOCK_CTRL_REG		0x8072
#define QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN	BIT(0)

int nss_ptp_clock_synce_clock_enable(
		struct nss_phy_device *nss_phydev,
		bool enable)
{
	struct phy_device *phydev = nss_phydev->phydev;

	if (nss_phydev_id_compare(phydev, QCA8084_PHY, GENMASK(31, 4)) ||
			nss_phydev_id_compare(phydev, QCA8081_PHY, GENMASK(31, 4))) {
		int ret;

		/* Enable analog synce clock output or not. */
		ret = nss_phy_modify_debug(nss_phydev, QCA808X_DEBUG_ANA_CLOCK_CTRL_REG,
					   QCA808X_ANALOG_PHY_SYNCE_CLOCK_EN,
					   enable ? QCA808X_ANALOG_PHY_SYNCE_CLOCK_EN : 0);
		if (ret)
			return ret;
	}

	/* Enable digital synce clock output or not. */
	return nss_phy_modify_mmd(nss_phydev, MDIO_MMD_AN, QCA808X_MMD7_CLOCK_CTRL_REG,
				  QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN,
				  enable ? QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN : 0);
}

int
nss_ptp_reset_buffer_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_reset_buffer_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RESET_BUFFER_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_reset_buffer_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_reset_buffer_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RESET_BUFFER_REG_ADDRESS,
			value->val);
}

int
nss_ptp_buffer_status_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_buffer_status_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_BUFFER_STATUS_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_buffer_status_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_buffer_status_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_BUFFER_STATUS_REG_ADDRESS,
			value->val);
}

int
nss_ptp_tx_buffer_write_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_buffer_write_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_BUFFER_WRITE_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_tx_buffer_write_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_tx_buffer_write_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_TX_BUFFER_WRITE_REG_ADDRESS,
			value->val);
}

int
nss_ptp_rx_buffer_read_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_buffer_read_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_BUFFER_READ_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_rx_buffer_read_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_rx_buffer_read_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_RX_BUFFER_READ_REG_ADDRESS,
			value->val);
}

int
nss_ptp_loc_mac_addr_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LOC_MAC_ADDR_0_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_loc_mac_addr_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_0_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LOC_MAC_ADDR_0_REG_ADDRESS,
				value->val);
}

int
nss_ptp_loc_mac_addr_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LOC_MAC_ADDR_1_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_loc_mac_addr_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_1_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LOC_MAC_ADDR_1_REG_ADDRESS,
				value->val);
}

int
nss_ptp_loc_mac_addr_2_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_2_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LOC_MAC_ADDR_2_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_loc_mac_addr_2_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_loc_mac_addr_2_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LOC_MAC_ADDR_2_REG_ADDRESS,
				value->val);
}

int
nss_ptp_link_delay_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LINK_DELAY_0_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_link_delay_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_0_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LINK_DELAY_0_REG_ADDRESS,
				value->val);
}

int
nss_ptp_link_delay_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LINK_DELAY_1_REG_ADDRESS,
				&value->val);
}

int
nss_ptp_link_delay_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_link_delay_1_reg_u *value)
{
	return nss_phy_write_mmd(
				nss_phydev,
				MDIO_MMD_PCS, PTP_LINK_DELAY_1_REG_ADDRESS,
				value->val);
}

int
nss_ptp_misc_control_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_control_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_MISC_CONTROL_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_misc_control_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_misc_control_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_MISC_CONTROL_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ingress_asymmetry_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_INGRESS_ASYMMETRY_0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ingress_asymmetry_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_INGRESS_ASYMMETRY_0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_ingress_asymmetry_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_INGRESS_ASYMMETRY_1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_ingress_asymmetry_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_ingress_asymmetry_1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_INGRESS_ASYMMETRY_1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_egress_asymmetry_0_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_0_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EGRESS_ASYMMETRY_0_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_egress_asymmetry_0_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_0_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EGRESS_ASYMMETRY_0_REG_ADDRESS,
			value->val);
}

int
nss_ptp_egress_asymmetry_1_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_1_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EGRESS_ASYMMETRY_1_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_egress_asymmetry_1_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_egress_asymmetry_1_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_EGRESS_ASYMMETRY_1_REG_ADDRESS,
			value->val);
}

int
nss_ptp_backup_reg_get(
		struct nss_phy_device *nss_phydev,
		union ptp_backup_reg_u *value)
{
	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, PTP_BACKUP_REG_ADDRESS,
			&value->val);
}

int
nss_ptp_backup_reg_set(
		struct nss_phy_device *nss_phydev,
		union ptp_backup_reg_u *value)
{
	return nss_phy_write_mmd(
			nss_phydev,
			MDIO_MMD_PCS, PTP_BACKUP_REG_ADDRESS,
			value->val);
}

int
nss_ptp_version_reg_get(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_version_reg_u *value)
{
	u32 reg = 0;

	switch (type) {
		case PTP_TS_RX0:
			reg = PTP_RX0_VERSION_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX1_VERSION_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX2_VERSION_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX3_VERSION_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX0_VERSION_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, reg,
			&value->val);
}

int
nss_ptp_msg_type_spec0_reg(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_msg_type_spec0_reg_u *value)
{
	u32 reg = 0;

	switch (type) {
		case PTP_TS_RX0:
			reg = PTP_RX0_MSG_TYPE_SPEC0_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX1_MSG_TYPE_SPEC0_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX2_MSG_TYPE_SPEC0_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX3_MSG_TYPE_SPEC0_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX0_MSG_TYPE_SPEC0_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, reg,
			&value->val);
}

int
nss_ptp_msg_type_spec1_reg(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_msg_type_spec1_reg_u *value)
{
	u32 reg = 0;

	switch (type) {
		case PTP_TS_RX0:
			reg = PTP_RX0_MSG_TYPE_SPEC1_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX1_MSG_TYPE_SPEC1_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX2_MSG_TYPE_SPEC1_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX3_MSG_TYPE_SPEC1_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX0_MSG_TYPE_SPEC1_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, reg,
			&value->val);
}

int
nss_ptp_domain_number_reg(struct nss_phy_device *nss_phydev, ptp_ts_type_t type,
		union ptp_domain_number_reg_u *value)
{
	u32 reg = 0;

	switch (type) {
		case PTP_TS_RX0:
			reg = PTP_RX0_DOMAIN_NUMBER_REG_ADDRESS;
			break;
		case PTP_TS_RX1:
			reg = PTP_RX1_DOMAIN_NUMBER_REG_ADDRESS;
			break;
		case PTP_TS_RX2:
			reg = PTP_RX2_DOMAIN_NUMBER_REG_ADDRESS;
			break;
		case PTP_TS_RX3:
			reg = PTP_RX3_DOMAIN_NUMBER_REG_ADDRESS;
			break;
		case PTP_TS_TX0:
			reg = PTP_TX0_DOMAIN_NUMBER_REG_ADDRESS;
			break;
		default:
			return -EINVAL;
	}

	return nss_phy_ptp_mmd_read(
			nss_phydev,
			MDIO_MMD_PCS, reg,
			&value->val);
}
