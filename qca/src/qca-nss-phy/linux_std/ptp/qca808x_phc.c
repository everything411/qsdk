/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/if_vlan.h>
#include <linux/net_tstamp.h>
#include <linux/phy.h>
#include <linux/ptp_classify.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/time64.h>

#include "../qca81xx/qca81xx.h"
#include "qca808x_phc.h"

#define QCA8081_PHY_ID				0x004dd101
#define QCA8084_PHY_ID				0x004dd180
#define QCA8111_PHY_ID				0x004dd1c0

#define QCA808X_DEBUG_ADDR			0x1d
#define QCA808X_DEBUG_DATA			0x1e

#define QCA808X_DEBUG_ANA_CLOCK_CTRL_REG	0x3e80
#define QCA808X_ANALOG_PHY_SYNCE_CLOCK_EN	0x20

#define QCA808X_MMD7_CLOCK_CTRL_REG		0x8072
#define QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN	BIT(0)
#define QCA808X_SYNCE_CLK_SEL_EN		BIT(1)

#define QCA808X_PTP_EMBEDDED_MODE		0xa

#define QCA808X_PTP_INCVAL_SYNC_MODE		0x8
#define QCA808X_PTP_TICK_RATE_125M		8
#define QCA808X_PTP_TICK_RATE_200M		5

#define QCA808X_MMD3_RTC_EXT_CONF		0x8100
#define QCA808X_RTC_EXT_INCVAL_SYNC_MODE	BIT(3)

#define QCA81XX_DBG_PORT31			0x1f
#define QCA81XX_1588_EN				BIT(1)

/* AFE ADC has the different clock frequency supplied on the different
 * link speed.
 *
 * link_speed	clock_freq	divider		PTP reference clock
 * 10 G		800 MHZ		4		200 MHZ
 * 5 G		400 MHZ		2		200 MHZ
 * 2.5 G	200 MHZ		1		200 MHZ
 * 100 M/1 G	125 MHZ		1		125 MHZ
 */
#define QCA81XX_MMD1_SYNCE_CLK_CTRL		0x2000
#define QCA81XX_SYNCE_CLK_SEL			GENMASK(6, 0)
#define QCA81XX_SYNCE_SEL_AFE_ADC_CLK_0		0x10
#define QCA81XX_SYNCE_SEL_AFE_ADC_CLK_1		0x12
#define QCA81XX_SYNCE_CLK_DISABLE		BIT(7)
#define QCA81XX_SYNCE_CLK_DIV			GENMASK(10, 8)

#define QCA808X_PTP_MAIN_CONFIG			0x8012
#define QCA808X_PTP_CLK_MODE			GENMASK(2, 1)
#define QCA808X_PTP_CLK_MODE_OC_TWO_STEP	0
#define QCA808X_PTP_CLK_MODE_OC_ONE_STEP	1
#define QCA808X_PTP_CLK_MODE_TC_TWO_STEP	2
#define QCA808X_PTP_CLK_MODE_TC_ONE_STEP	3
#define QCA808X_PTP_CLK_MODE_ONE_STEP		BIT(1)
#define QCA808X_PTP_CLK_MODE_TC_MODE		BIT(2)
#define QCA808X_PTP_BYPASS			BIT(3)
#define QCA808X_PTP_TIMESTAMP_ATTACH_EN		BIT(4)
#define QCA808X_WOL_EN				BIT(5)
#define QCA808X_PTP_RTC_SEL_SYNCE		BIT(7)
#define QCA808X_DISABLE_1588_PHY		BIT(8)
#define QCA808X_IPV6_EMBED_FORCE_CHECKSUM_ZERO	BIT(11)

#define QCA808X_PTP_MISC_CONFIG			0x80f0
#define QCA808X_PTP_PKT_ONE_STEP_EN		BIT(11)
#define QCA808X_PTP_CF_FROM_PKT_EN		BIT(12)
#define QCA808X_PTP_EMBED_INGRESS_TS_EN		BIT(13)
#define QCA808X_PTP_TC_OFFLOAD			BIT(15)

#define QCA808X_PTP_RTC_CLK_CONFIG      	0x8017
#define QCA808X_PTP_RTC_SEL_EXTERNAL		BIT(11)

#define QCA808X_RTC_RX_SEQID_0			0x8013
#define QCA808X_RTC_RX_SEQID_1			0x8500
#define QCA808X_RTC_RX_SEQID_2			0x851a
#define QCA808X_RTC_RX_SEQID_3			0x8534
#define QCA808X_RTC_TX_SEQID_0			0x8020

#define QCA808X_RTC_RX_PORTID_0			0x8014
#define QCA808X_RTC_RX_PORTID_1			0x8501
#define QCA808X_RTC_RX_PORTID_2			0x851b
#define QCA808X_RTC_RX_PORTID_3			0x8535
#define QCA808X_RTC_TX_PORTID_0			0x8021

#define QCA808X_RTC_RX_TIMESTAMP_0		0x8019
#define QCA808X_RTC_RX_TIMESTAMP_1		0x8506
#define QCA808X_RTC_RX_TIMESTAMP_2		0x8520
#define QCA808X_RTC_RX_TIMESTAMP_3		0x853a
#define QCA808X_RTC_TX_TIMESTAMP_0		0x8026
#define QCA808X_RTC_RX_FRAC_NSEC_HI		GENMASK(11, 0)
#define QCA808X_RTC_RX_MSG_TYPE			GENMASK(15, 12)
#define QCA808X_RTC_RX_FRAC_NSEC_LO		GENMASK(7, 0)

#define QCA808X_INGRESS_TRIG_NSEC_HI		0x8031
#define QCA808X_INGRESS_TRIG_NSEC_LO		0x8032

#define QCA808X_RTC_SEC_HI			0x803d
#define QCA808X_RTC_SEC_MID			0x803e
#define QCA808X_RTC_SEC_LO			0x803f
#define QCA808X_RTC_NSEC_HI			0x8040
#define QCA808X_RTC_NSEC_LO			0x8041

#define QCA808X_RTC_INC_CONF_0			0x8036
#define QCA808X_RTC_INC_NSEC			GENMASK(15, 10)
#define QCA808X_RTC_INC_FRACTION_NSEC_HI	GENMASK(9, 0)

#define QCA808X_RTC_INC_CONF_1			0x8037
#define QCA808X_RTC_INC_FRACTION_NSEC_LO	GENMASK(15, 0)

#define QCA808X_RTC_OFFSET_NSEC_HI		0x8038
#define QCA808X_RTC_OFFSET_NSEC_LO		0x8039
#define QCA808X_RTC_OFFSET_SEC_HI		0x803a
#define QCA808X_RTC_OFFSET_SEC_MID		0x803b
#define QCA808X_RTC_OFFSET_SEC_LO		0x803c
#define QCA808X_RTC_OFFSET_CONF			0x8044
#define QCA808X_RTC_OFFSET_CONF_VALID		BIT(0)

#define QCA808X_RTC_PRELOAD_SEC_HI		0x8101
#define QCA808X_RTC_PRELOAD_SEC_MID		0x8102
#define QCA808X_RTC_PRELOAD_SEC_LO		0x8103
#define QCA808X_RTC_PRELOAD_NSEC_HI		0x8104
#define QCA808X_RTC_PRELOAD_NSEC_LO		0x8105
#define QCA808X_RTC_EXT_CONF			0x8100
#define QCA808X_RTC_EXT_CONF_LOAD		BIT(0)
#define QCA808X_RTC_EXT_CONF_INC_VALID		BIT(2)

#define QCA81XX_MMD3_PTP_OPTION			0x8550
#define QCA81XX_RX_DELAY_COMPENSATION		GENMASK(1, 0)
#define QCA81XX_TX_DELAY_COMPENSATION		GENMASK(7, 5)

#define QCA808X_PTP_BACKUP_CONFIG		0x9036
#define QCA808X_PTP_P2P_TC_EN			BIT(0)

struct qca808x_ptp_info {
	int hwts_tx_type;
	int hwts_rx_type;
	struct sk_buff_head tx_queue;
	struct sk_buff_head rx_queue;
	struct ptp_clock_info caps;
	struct ptp_clock *ptp_clock;
	struct mutex tsreg_lock;
	struct mii_timestamper mii_ts;
	struct phy_device *phydev;
	int ptp_mode;
	struct list_head list;
};

struct qca808x_ptp_cb {
	int ptp_type;
	struct ptp_header *header;
};

struct qca808x_hwts {
	u64 clock_id;
	u16 port_number;
	u16 seq_id;
	u8 msg_type;
	u64 sec;
	u32 nsec;
	u32 fsec;
};

enum {
	PTP_TS_RX0,
	PTP_TS_RX1,
	PTP_TS_RX2,
	PTP_TS_RX3,
	PTP_TS_TX0,
};

enum {
	PTP_MODE_OC_TWO_STEP,
	PTP_MODE_OC_ONE_STEP,
	PTP_MODE_E2E_TC_TWO_STEP,
	PTP_MODE_E2E_TC_ONE_STEP,
	PTP_MODE_P2P_TC_TWO_STEP,
	PTP_MODE_P2P_TC_ONE_STEP,
};

enum {
	PTP_RTC_REF_CLOCK_LOCAL,
	PTP_RTC_REF_CLOCK_SYNCE,
	PTP_RTC_REF_CLOCK_EXTERNAL,
};

static LIST_HEAD(qca8xxx_phcs);
static DEFINE_MUTEX(qca8xxx_phcs_lock);

static const char *qca_phy_driver_ptp_supported_names[] = {
	"Qualcomm QCA8081",
	"Qualcomm QCA8084",
	"Qualcomm QCA81xx",
};

static int qca808x_debug_reg_read(struct phy_device *phydev, u16 reg)
{
	int ret;

	ret = phy_write(phydev, QCA808X_DEBUG_ADDR, reg);
	if (ret < 0)
		return ret;

	return phy_read(phydev, QCA808X_DEBUG_DATA);
}

static int qca808x_debug_reg_mask(struct phy_device *phydev, u16 reg,
				 u16 clear, u16 set)
{
	u16 val;
	int ret;

	ret = qca808x_debug_reg_read(phydev, reg);
	if (ret < 0)
		return ret;

	val = ret & 0xffff;
	val &= ~clear;
	val |= set;

	return phy_write(phydev, QCA808X_DEBUG_DATA, val);
}

static int qca808x_ptp_rtc_reference_set(struct phy_device *phydev, int ref_clock)
{
	int ret;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_MAIN_CONFIG,
			      QCA808X_PTP_RTC_SEL_SYNCE,
			      ref_clock == PTP_RTC_REF_CLOCK_SYNCE ? QCA808X_PTP_RTC_SEL_SYNCE : 0);
	if (ret)
		return ret;

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_RTC_CLK_CONFIG,
			QCA808X_PTP_RTC_SEL_EXTERNAL,
			ref_clock == PTP_RTC_REF_CLOCK_EXTERNAL ? QCA808X_PTP_RTC_SEL_EXTERNAL : 0);
}

static int qca808x_ptp_clock_synce_clock_enable(struct phy_device *phydev,
						bool enable)
{
	if (phydev_id_compare(phydev, QCA8081_PHY_ID) || phydev_id_compare(phydev, QCA8084_PHY_ID)) {
		int ret;

		/* Enable analog synce clock output or not. */
		ret = qca808x_debug_reg_mask(phydev, QCA808X_DEBUG_ANA_CLOCK_CTRL_REG,
				QCA808X_ANALOG_PHY_SYNCE_CLOCK_EN,
				enable ? QCA808X_ANALOG_PHY_SYNCE_CLOCK_EN : 0);
		if (ret)
			return ret;
	}

	/* Enable digital synce clock output or not. */
	return phy_modify_mmd(phydev, MDIO_MMD_AN, QCA808X_MMD7_CLOCK_CTRL_REG,
			      QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN,
			      enable ? QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN : 0);

}

static int qca808x_ptp_clock_incval_mode_set(struct phy_device *phydev,
					     bool enable)
{
	return phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_MMD3_RTC_EXT_CONF,
			      QCA808X_RTC_EXT_INCVAL_SYNC_MODE,
			      enable ? QCA808X_RTC_EXT_INCVAL_SYNC_MODE : 0);
}

static int qca808x_ptp_seqid_get(struct phy_device *phydev, int ts_type,
				 u16 *seq_id)
{
	int data;
	u32 reg;

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = QCA808X_RTC_RX_SEQID_0;
			break;
		case PTP_TS_RX1:
			reg = QCA808X_RTC_RX_SEQID_1;
			break;
		case PTP_TS_RX2:
			reg = QCA808X_RTC_RX_SEQID_2;
			break;
		case PTP_TS_RX3:
			reg = QCA808X_RTC_RX_SEQID_3;
			break;
		case PTP_TS_TX0:
			reg = QCA808X_RTC_TX_SEQID_0;
			break;
		default:
			return -EINVAL;
	}

	data = phy_read_mmd(phydev, MDIO_MMD_PCS, reg);
	if (data < 0)
		return data;

	*seq_id = data;

	return 0;
}

static int qca808x_ptp_portid_get(struct phy_device *phydev, int ts_type,
				  u64 *clock_id, u16 *port_num)

{
	u64 clock_id_value = 0;
	int data, index;
	u32 reg;

	switch (ts_type) {
	case PTP_TS_RX0:
		reg = QCA808X_RTC_RX_PORTID_0;
		break;
	case PTP_TS_RX1:
		reg = QCA808X_RTC_RX_PORTID_1;
		break;
	case PTP_TS_RX2:
		reg = QCA808X_RTC_RX_PORTID_2;
		break;
	case PTP_TS_RX3:
		reg = QCA808X_RTC_RX_PORTID_3;
		break;
	case PTP_TS_TX0:
		reg = QCA808X_RTC_TX_PORTID_0;
		break;
	default:
		return -EINVAL;
	}

	for (index = 0; index < 4; index++) {
		data = phy_read_mmd(phydev, MDIO_MMD_PCS, reg + index);
		if (data < 0)
			return data;

		clock_id_value |= data << (16 * (3 - index));
	}

	data = phy_read_mmd(phydev, MDIO_MMD_PCS, reg + 4);
	if (data < 0)
		return data;

	*port_num = data;
	*clock_id = clock_id_value;

	return 0;
}

static int qca808x_ptp_msgtype_timestamp_get(struct phy_device *phydev, int ts_type,
					     struct qca808x_hwts *hwts)
{
	u32 nsec_tmp = 0, fsec_tmp = 0;
	int data, index, msg_type;
	u64 sec_tmp = 0;
	u32 reg;

	switch (ts_type) {
		case PTP_TS_RX0:
			reg = QCA808X_RTC_RX_TIMESTAMP_0;
			break;
		case PTP_TS_RX1:
			reg = QCA808X_RTC_RX_TIMESTAMP_1;
			break;
		case PTP_TS_RX2:
			reg = QCA808X_RTC_RX_TIMESTAMP_2;
			break;
		case PTP_TS_RX3:
			reg = QCA808X_RTC_RX_TIMESTAMP_3;
			break;
		case PTP_TS_TX0:
			reg = QCA808X_RTC_TX_TIMESTAMP_0;
			break;
		default:
			return -EINVAL;
	}

	for (index = 0; index < 7; index++) {
		data = phy_read_mmd(phydev, MDIO_MMD_PCS, reg + index);
		if (data < 0)
			return data;

		switch (index) {
		case 0:
		case 1:
		case 2:
			sec_tmp |= data << (16 * (2 - index));
			break;
		case 3:
		case 4:
			nsec_tmp |= data << (16 * (4 - index));
			break;
		case 5:
			fsec_tmp |= FIELD_GET(QCA808X_RTC_RX_FRAC_NSEC_HI, data) << 8;
			msg_type = FIELD_GET(QCA808X_RTC_RX_MSG_TYPE, data);
			break;
		case 6:
			fsec_tmp |= FIELD_GET(QCA808X_RTC_RX_FRAC_NSEC_LO, data);
			break;
		default:
			break;
		}
	}

	hwts->msg_type = msg_type;
	hwts->sec = sec_tmp;
	hwts->nsec = nsec_tmp;
	hwts->fsec = fsec_tmp;

	return 0;
}

static int qca808x_read_hwts(struct phy_device *phydev, int ts_type,
			     struct qca808x_hwts *hwts)
{
	u16 port_num, seq_id;
	u64 clock_id;
	int ret;

	ret = qca808x_ptp_msgtype_timestamp_get(phydev, ts_type, hwts);
	if (ret)
		return ret;

	ret = qca808x_ptp_seqid_get(phydev, ts_type, &seq_id);
	if (ret)
		return ret;

	ret = qca808x_ptp_portid_get(phydev, ts_type, &clock_id, &port_num);
	if (ret)
		return ret;

	hwts->clock_id = clock_id;
	hwts->port_number = port_num;
	hwts->seq_id = seq_id;

	return 0;
}

static bool qca808x_match_hwts(struct ptp_header *header, int type,
			       struct qca808x_hwts *hwts)
{
	return be16_to_cpu(header->sequence_id) == hwts->seq_id &&
	       ptp_get_msgtype(header, type) == hwts->msg_type &&
	       be16_to_cpu(header->source_port_identity.port_number) == hwts->port_number;
}

static bool tx_timestamp_work(struct qca808x_ptp_info *ptp_info)
{
	bool reschedule = false, ts_match = false;
	struct skb_shared_hwtstamps shhwtstamps;
	struct qca808x_hwts hwts = {};
	struct qca808x_ptp_cb *ptp_cb;
	struct timespec64 ts;
	struct sk_buff *skb;
	int ret, times;

	memset(&shhwtstamps, 0, sizeof(shhwtstamps));
	while ((skb = skb_dequeue(&ptp_info->tx_queue))) {
		ptp_cb = (struct qca808x_ptp_cb *)skb->cb;

		times = 0;
		do {
			times++;
			ret = qca808x_read_hwts(ptp_info->phydev, PTP_TS_TX0, &hwts);
			if (ret)
				continue;

			ts_match = qca808x_match_hwts(ptp_cb->header, ptp_cb->ptp_type, &hwts);
			if (ts_match)
				break;

		} while (times < 100);

		if (ts_match) {
			ts.tv_sec = hwts.sec;
			ts.tv_nsec = hwts.nsec;
			shhwtstamps.hwtstamp = ns_to_ktime(timespec64_to_ns(&ts));
			skb_complete_tx_timestamp(skb, &shhwtstamps);
		} else {
			phydev_warn(ptp_info->phydev,
				    "TX timestamp does not match the skb sequence_id %d, msg_type %d\n",
				    be16_to_cpu(ptp_cb->header->sequence_id),
				    ptp_get_msgtype(ptp_cb->header, ptp_cb->ptp_type));
		}
	}

	return reschedule;
}

static bool qca808x_need_ingress_ts_trigger(struct phy_device *phydev, int msg_type)
{
	struct qca808x_ptp_info *ptp_info = container_of(phydev->mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);
	bool triggered = false;

	if (ptp_info->ptp_mode == PTP_MODE_E2E_TC_ONE_STEP ||
	    (ptp_info->ptp_mode == PTP_MODE_P2P_TC_ONE_STEP &&
	     msg_type == PTP_MSGTYPE_SYNC))
		triggered = true;

	return triggered;
}

static int qca808x_ingress_trigger_timestamp_config(struct phy_device *ingress_phydev,
						    int msg_type,
						    struct timespec64 ts)
{
	struct qca808x_ptp_info *ptp_info;
	struct phy_device *phydev;
	struct list_head *this;

	if (!qca808x_need_ingress_ts_trigger(ingress_phydev, msg_type))
		return 0;

	mutex_lock(&qca8xxx_phcs_lock);
	list_for_each(this, &qca8xxx_phcs) {
		ptp_info = list_entry(this, struct qca808x_ptp_info, list);
		phydev = ptp_info->phydev;

		if (phydev == ingress_phydev)
			continue;

		if (qca808x_need_ingress_ts_trigger(phydev, msg_type)) {
			phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_INGRESS_TRIG_NSEC_HI,
				      upper_16_bits(ts.tv_nsec));
			phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_INGRESS_TRIG_NSEC_LO,
				      lower_16_bits(ts.tv_nsec));
		}
	}
	mutex_unlock(&qca8xxx_phcs_lock);

	return 0;
}

static void rx_timestamp_work(struct qca808x_ptp_info *ptp_info)
{
	struct skb_shared_hwtstamps *shhwtstamps = NULL;
	struct qca808x_hwts hwts = {};
	struct qca808x_ptp_cb *ptp_cb;
	int ret, ts_type, msg_type;
	struct timespec64 ts;
	struct sk_buff *skb;
	bool ts_match;

	/* Deliver packets */
	while ((skb = skb_dequeue(&ptp_info->rx_queue))) {
		ptp_cb = (struct qca808x_ptp_cb *)skb->cb;
		msg_type = ptp_get_msgtype(ptp_cb->header, ptp_cb->ptp_type);

		ts_match = false;
		for (ts_type = PTP_TS_RX0; ts_type <= PTP_TS_RX3; ts_type++) {
			ret = qca808x_read_hwts(ptp_info->phydev, ts_type, &hwts);
			if (ret)
				continue;

			ts_match = qca808x_match_hwts(ptp_cb->header, ptp_cb->ptp_type, &hwts);
			if (ts_match)
				break;
		}

		if (ts_match) { 
			ts.tv_sec = hwts.sec;
			ts.tv_nsec = hwts.nsec;

			qca808x_ingress_trigger_timestamp_config(ptp_info->phydev,
								 msg_type, ts);
			shhwtstamps = skb_hwtstamps(skb);
			shhwtstamps->hwtstamp = ns_to_ktime(timespec64_to_ns(&ts));
			netif_rx(skb);
		} else {
			phydev_warn(ptp_info->phydev,
				    "RX timestamp does not match the skb sequence_id %d, msg_type %d\n",
				    be16_to_cpu(ptp_cb->header->sequence_id),
				    msg_type);
		}
	}
}

static int qca808x_ptp_settime(struct ptp_clock_info *ptp,
			       const struct timespec64 *ts)
{
	struct qca808x_ptp_info *clock = container_of(ptp,
						      struct qca808x_ptp_info,
						      caps);
	struct phy_device *phydev = clock->phydev;

	mutex_lock(&clock->tsreg_lock);
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_PRELOAD_SEC_HI, upper_32_bits(ts->tv_sec) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_PRELOAD_SEC_MID, upper_16_bits(ts->tv_sec));
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_PRELOAD_SEC_LO, lower_16_bits(ts->tv_sec));
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_PRELOAD_NSEC_HI, upper_16_bits(ts->tv_nsec));
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_PRELOAD_NSEC_LO, lower_16_bits(ts->tv_nsec));
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_EXT_CONF, 0, QCA808X_RTC_EXT_CONF_LOAD);
	mutex_unlock(&clock->tsreg_lock);

	return 0;
}

static int qca808x_ptp_gettime(struct ptp_clock_info *ptp,
			       struct timespec64 *ts)
{
	struct qca808x_ptp_info *clock = container_of(ptp,
						      struct qca808x_ptp_info,
						      caps);
	struct phy_device *phydev = clock->phydev;
	time64_t sec;
	s64 nsec;

	mutex_lock(&clock->tsreg_lock);
	sec = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_SEC_HI);
	sec <<= 16;
	sec |= phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_SEC_MID);
	sec <<= 16;
	sec |= phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_SEC_LO);

	nsec = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_NSEC_HI);
	nsec <<= 16;
	nsec |= phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_NSEC_LO);
	mutex_unlock(&clock->tsreg_lock);

	set_normalized_timespec64(ts, sec, nsec);

	return 0;
}

static int qca808x_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct qca808x_ptp_info *clock = container_of(ptp,
						      struct qca808x_ptp_info,
						      caps);
	struct phy_device *phydev = clock->phydev;
	struct timespec64 ts;

	ts = ns_to_timespec64(delta);

	mutex_lock(&clock->tsreg_lock);
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_OFFSET_SEC_HI, upper_32_bits(ts.tv_sec) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_OFFSET_SEC_MID, upper_16_bits(ts.tv_sec));
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_OFFSET_SEC_LO, lower_16_bits(ts.tv_sec));
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_OFFSET_NSEC_HI, upper_16_bits(ts.tv_nsec));
	phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_OFFSET_NSEC_LO, lower_16_bits(ts.tv_nsec));
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_OFFSET_CONF, 0, QCA808X_RTC_OFFSET_CONF_VALID);
	mutex_unlock(&clock->tsreg_lock);

	return 0;
}

static int qca808x_ptp_rtc_incval_set(struct phy_device *phydev, u32 nsec, u32 fsec)
{
	u16 data;
	int ret;

	data = FIELD_PREP(QCA808X_RTC_INC_NSEC, nsec);
	data |= FIELD_PREP(QCA808X_RTC_INC_FRACTION_NSEC_HI, (fsec >> 16) && 0x3ff);

	ret = phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_INC_CONF_0, data);
	if (ret)
		return ret;

	data = FIELD_PREP(QCA808X_RTC_INC_FRACTION_NSEC_LO, fsec & 0xffff);

	ret = phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_INC_CONF_1, data);
	if (ret)
		return ret;

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_EXT_CONF,
			      0, QCA808X_RTC_EXT_CONF_INC_VALID);
}

/* adj = scaled_ppm * (8 or 5) * 2^26 / (10^6 * 2^16)
 * which simplifies to:
 * adj = scaled_ppm * 2^7 / 5^6
 */
static int qca808x_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct qca808x_ptp_info *clock = container_of(ptp,
						      struct qca808x_ptp_info,
						      caps);
	struct phy_device *phydev = clock->phydev;
	u32 nsec = QCA808X_PTP_TICK_RATE_125M;
	bool neg_adj = false;
	u64 adj, diff;
	int ret;

	if (phydev->speed >= SPEED_2500)
		nsec = QCA808X_PTP_TICK_RATE_200M;

	if (scaled_ppm < 0) {
		neg_adj = true;
		scaled_ppm = -scaled_ppm;
	}

	adj = scaled_ppm << 4;
	diff = div_u64(adj * nsec, 15625);
	adj = (nsec << 26) + (neg_adj ? -diff : diff);


	mutex_lock(&clock->tsreg_lock);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_INC_CONF_1, adj & 0xffff);
	ret |= phy_write_mmd(phydev, MDIO_MMD_PCS, QCA808X_RTC_INC_CONF_0, adj >> 16);
	mutex_unlock(&clock->tsreg_lock);

	return ret;
}

static int qca808x_ptp_enable(struct ptp_clock_info *ptp,
			      struct ptp_clock_request *rq, int on)
{
	return -EOPNOTSUPP;
}

static int qca808x_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
			      enum ptp_pin_function func, unsigned int chan)
{
	return 1;
}

static void qca81xx_link_state(struct phy_device *phydev)
{
	u16 reg_val, div = 0, compensation = 0;
	int ret;

	/* Enable PTP RX & TX delay compensation on the link speed 100M/1G. */
	switch (phydev->speed) {
	case SPEED_100:
	case SPEED_1000:
		compensation = QCA81XX_RX_DELAY_COMPENSATION |
			       QCA81XX_TX_DELAY_COMPENSATION;
		div = 1;
		break;
	case SPEED_2500:
		div = 1;
		break;
	case SPEED_5000:
		div = 2;
		break;
	case SPEED_10000:
		div = 4;
		break;
	default:
		return;
	}

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PTP_OPTION,
			     QCA81XX_RX_DELAY_COMPENSATION | QCA81XX_TX_DELAY_COMPENSATION,
			     compensation);
	if (ret) {
		phydev_err(phydev, "configure delay compensation failed %d", ret);
		return;
	}

	reg_val = FIELD_PREP(QCA81XX_SYNCE_CLK_SEL, QCA81XX_SYNCE_SEL_AFE_ADC_CLK_0);
	reg_val |= FIELD_PREP(QCA81XX_SYNCE_CLK_DIV, div - 1);

	ret = phy_modify_mmd(phydev, MDIO_MMD_PMAPMD, QCA81XX_MMD1_SYNCE_CLK_CTRL,
			     QCA81XX_SYNCE_CLK_SEL | QCA81XX_SYNCE_CLK_DIV,
			     reg_val);
	if (ret)
		phydev_err(phydev, "configure synce failed %d", ret);
}

static void qca808x_ptp_change_notify(struct mii_timestamper *mii_ts, struct phy_device *phydev)
{
	struct qca808x_ptp_info *clock = container_of(mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);
	int ref_clk;
	u32 nsec;

	mutex_lock(&clock->tsreg_lock);
	if (phy_id_compare(phydev->c45_ids.device_ids[MDIO_MMD_PMAPMD], QCA8111_PHY_ID, 0x00ffffff))
		qca81xx_link_state(phydev);

	switch (phydev->speed) {
	case SPEED_10000:
	case SPEED_5000:
	case SPEED_2500:
		nsec = QCA808X_PTP_TICK_RATE_200M;
		ref_clk = PTP_RTC_REF_CLOCK_SYNCE;
		break;
	case SPEED_1000:
	case SPEED_100:
		nsec = QCA808X_PTP_TICK_RATE_125M;
		ref_clk = PTP_RTC_REF_CLOCK_SYNCE;
		break;
	case SPEED_10:
	default:
		nsec = QCA808X_PTP_TICK_RATE_125M;
		ref_clk = PTP_RTC_REF_CLOCK_LOCAL;
		break;
	}

	qca808x_ptp_rtc_reference_set(phydev, ref_clk);
	qca808x_ptp_rtc_incval_set(phydev, nsec, 0);
	mutex_unlock(&clock->tsreg_lock);
}

static int qca81xx_ptp_synce_pin_config(struct phy_device *phydev, bool en)
{
	int  ret, pin_id;

	for (pin_id = GPIO5_PPS_IN; pin_id <= GPIO7_REFCLK_IN; pin_id++) {
		ret = qca81xx_soc_modify(phydev, TO_TLMM_CFG_REG(pin_id),
					 TLMM_FUNC_MASK, en ? BIT(2) : 0);
		if (ret)
			return ret;
	}

	for (pin_id = GPIO10_PPS_OUT; pin_id <= GPIO12_CLK125_TDI; pin_id++) {
		ret = qca81xx_soc_modify(phydev, TO_TLMM_CFG_REG(pin_id),
					 TLMM_FUNC_MASK, en ? BIT(2) : 0);
		if (ret)
			return ret;
	}

	return 0;
}

static int qca81xx_ptp_clock_set(struct phy_device *phydev, bool enable)
{
	int ret;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PMAPMD, QCA81XX_MMD1_SYNCE_CLK_CTRL,
			     QCA81XX_SYNCE_CLK_DISABLE,
			     enable ? 0 : QCA81XX_SYNCE_CLK_DISABLE);
	if (ret)
		return ret;

	ret = qca81xx_phy_debug_modify(phydev, QCA81XX_DBG_PORT31,
				       QCA81XX_1588_EN,
				       enable ? QCA81XX_1588_EN : 0);
	if (ret)
		return ret;

	return qca81xx_ptp_synce_pin_config(phydev, enable);
}

static int qca808x_ptp_enable_set(struct phy_device *phydev, bool en, bool one_step)
{
	int data = 0, mask;

	mask = QCA808X_PTP_BYPASS;
	mask |= QCA808X_DISABLE_1588_PHY;
	mask |= QCA808X_PTP_CLK_MODE_ONE_STEP;

	if (!en)
		data = QCA808X_PTP_BYPASS | QCA808X_DISABLE_1588_PHY;

	if (one_step)
		data |= QCA808X_PTP_CLK_MODE_ONE_STEP;

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_MAIN_CONFIG,
			      mask, data);
}

static int qca808x_rx_timestamp_mode_set(struct phy_device *phydev, bool embeded)
{
	int misc_data, main_data;
	int ret;

	if (embeded) {
		misc_data = QCA808X_PTP_EMBED_INGRESS_TS_EN;
		main_data = QCA808X_IPV6_EMBED_FORCE_CHECKSUM_ZERO;
	} else {
		misc_data = QCA808X_PTP_CF_FROM_PKT_EN;
		main_data = 0;
	}

	/* Enable Embed mode to get RX timestamp */
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_MISC_CONFIG,
			     QCA808X_PTP_CF_FROM_PKT_EN | QCA808X_PTP_EMBED_INGRESS_TS_EN,
			     misc_data);
	if (ret)
		return ret;

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_MAIN_CONFIG,
			      QCA808X_PTP_TIMESTAMP_ATTACH_EN | QCA808X_IPV6_EMBED_FORCE_CHECKSUM_ZERO,
			      main_data);
}

static int qca808x_ptp_clock_mode_update(struct phy_device *phydev, int *mode)
{
	bool p2p_en, embeded;
	int val, hw_mode;

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_MAIN_CONFIG);
	hw_mode = FIELD_GET(QCA808X_PTP_CLK_MODE, val);

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA808X_PTP_BACKUP_CONFIG);
	p2p_en = val & QCA808X_PTP_P2P_TC_EN ? true : false;

	switch (hw_mode) {
	case QCA808X_PTP_CLK_MODE_OC_TWO_STEP:
		*mode = PTP_MODE_OC_TWO_STEP;
		embeded = true;
		break;
	case QCA808X_PTP_CLK_MODE_OC_ONE_STEP:
		*mode = PTP_MODE_OC_ONE_STEP;
		embeded = false;
		break;
	case QCA808X_PTP_CLK_MODE_TC_TWO_STEP:
		*mode = p2p_en ? PTP_MODE_P2P_TC_TWO_STEP : PTP_MODE_E2E_TC_TWO_STEP;
		embeded = true;
		break;
	case QCA808X_PTP_CLK_MODE_TC_ONE_STEP:
		*mode = p2p_en ? PTP_MODE_P2P_TC_ONE_STEP : PTP_MODE_E2E_TC_ONE_STEP;
		embeded = false;
		break;
	default:
		return -EINVAL;
	}

	return qca808x_rx_timestamp_mode_set(phydev, embeded);
}

static int qca808x_hwtstamp(struct mii_timestamper *mii_ts, struct ifreq *ifr)
{
	struct qca808x_ptp_info *ptp_info = container_of(mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);
	struct phy_device *phydev = ptp_info->phydev;
	bool ptp_en = false, one_step = false;
	struct hwtstamp_config cfg;

	if (copy_from_user(&cfg, ifr->ifr_data, sizeof(cfg)))
		return -EFAULT;

	if (cfg.tx_type < 0 || cfg.tx_type > HWTSTAMP_TX_ONESTEP_P2P)
		return -ERANGE;

	ptp_info->hwts_tx_type = cfg.tx_type;
	switch (cfg.rx_filter) {
		case HWTSTAMP_FILTER_NONE:
			ptp_info->hwts_rx_type = PTP_CLASS_NONE;
			break;
		case HWTSTAMP_FILTER_PTP_V2_EVENT:
		case HWTSTAMP_FILTER_PTP_V2_SYNC:
		case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
			ptp_info->hwts_rx_type = PTP_CLASS_L4 | PTP_CLASS_L2;
			break;
		case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
		case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
		case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
			ptp_info->hwts_rx_type = PTP_CLASS_L4;
			break;
		case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
		case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
		case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
			ptp_info->hwts_rx_type = PTP_CLASS_L2;
			break;
		default:
			break;
	}

	mutex_lock(&ptp_info->tsreg_lock);

	switch (ptp_info->hwts_tx_type) {
		case HWTSTAMP_TX_ONESTEP_SYNC:
		case HWTSTAMP_TX_ONESTEP_P2P:
			one_step = true;
			break;
		case HWTSTAMP_TX_OFF:
		case HWTSTAMP_TX_ON:
		default:
			break;
	}

	if (ptp_info->hwts_tx_type || ptp_info->hwts_tx_type)
		ptp_en = true;

	qca808x_ptp_enable_set(phydev, ptp_en, one_step);
	qca808x_ptp_clock_mode_update(phydev, &ptp_info->ptp_mode);

	if (phy_id_compare(phydev->c45_ids.device_ids[MDIO_MMD_PMAPMD], QCA8111_PHY_ID, 0x00ffffff))
		qca81xx_ptp_clock_set(phydev, ptp_en);

	mutex_unlock(&ptp_info->tsreg_lock);

	return copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)) ? -EFAULT : 0;
}

static bool qca808x_rxtstamp(struct mii_timestamper *mii_ts, struct sk_buff *skb,
			     int type)
{
	struct qca808x_ptp_info *ptp_info = container_of(mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);
	struct skb_shared_hwtstamps *shhwtstamps;
	struct qca808x_ptp_cb *ptp_cb;
	struct ptp_header *header;
	struct timespec64 ts = {};
	int ptp_class;

	if (ptp_info->hwts_rx_type == PTP_CLASS_NONE)
		return false;

	/* The PTP_CLASS_NONE is passed, which indicates that the
	 * PTP class is not determined, calling ptp_classify_raw to
	 * classfy the packet.
	 */
	if (type == PTP_CLASS_NONE) {
		__skb_push(skb, ETH_HLEN);
		/* dissecting the packet content to get ptp class */
		ptp_class = ptp_classify_raw(skb);
		__skb_pull(skb, ETH_HLEN);
		if (ptp_class == PTP_CLASS_NONE) {
			/* this case should not happen, only ptp event packet passed */
			pr_err("%s: No PTP event packet received\n", __func__);
			return false;
		}
		type = ptp_class;
	}

	if ((ptp_info->hwts_rx_type & type) == PTP_CLASS_NONE)
		return false;

	header = ptp_parse_header(skb, type);
	if (((header->ver) >> 4) == QCA808X_PTP_EMBEDDED_MODE) {
		u64 ct_ns_low = FIELD_GET(GENMASK_ULL(63, 40), be64_to_cpu(header->correction));
		u64 ct_org = FIELD_GET(GENMASK_ULL(39, 0), be64_to_cpu(header->correction));

		ts.tv_nsec = header->reserved1;
		ts.tv_nsec <<= 24;
		ts.tv_nsec |= ct_ns_low;
		ts.tv_sec = be32_to_cpu(header->reserved2);

		header->ver &= 0xf;
		header->reserved1 = 0;
		header->reserved2 = 0;

		/* Low 16 bits of frac nanosecond is dropped.
		 * And high 8 bits are also dropped.
		 */
		header->correction = cpu_to_be64(FIELD_PREP(GENMASK_ULL(55, 16), ct_org));

		shhwtstamps = skb_hwtstamps(skb);
		shhwtstamps->hwtstamp = ns_to_ktime(timespec64_to_ns(&ts));
		netif_rx(skb);
	} else {
		ptp_cb = (struct qca808x_ptp_cb *)skb->cb;
		ptp_cb->ptp_type = type;
		ptp_cb->header = header;
		skb_queue_tail(&ptp_info->rx_queue, skb);
		ptp_schedule_worker(ptp_info->ptp_clock, 0);
	}

	return true;
}

static void qca808x_txtstamp(struct mii_timestamper *mii_ts, struct sk_buff *org_skb, int type)
{
	struct qca808x_ptp_info *ptp_info = container_of(mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);
	struct qca808x_ptp_cb *ptp_cb;
	struct ptp_header *ptp_header;
	int ptp_class, msgtype;
	struct sk_buff *skb;


	/* The PTP_CLASS_NONE is passed, which indicates that the
	 * PTP class is not determined, calling ptp_classify_raw to
	 * classfy the packet.
	 */
	if (type == PTP_CLASS_NONE) {
		ptp_class = ptp_classify_raw(org_skb);
		if (ptp_class == PTP_CLASS_NONE) {
			return;
		}
		skb = skb_clone_sk(org_skb);
		if (!skb) {
			pr_err("%s: skb_clone_sk failed\n", __func__);
			return;
		}
		type = ptp_class;
	} else {
		skb = org_skb;
	}

	ptp_header = ptp_parse_header(skb, type);
	if (!ptp_header)
		goto txtstamp_out;

	msgtype = ptp_get_msgtype(ptp_header, type);
	switch (ptp_info->hwts_tx_type) {
		case HWTSTAMP_TX_ONESTEP_P2P:
			if (msgtype == PTP_MSGTYPE_PDELAY_RESP)
				goto txtstamp_out;
			fallthrough;
		case HWTSTAMP_TX_ONESTEP_SYNC:
			if (msgtype == PTP_MSGTYPE_SYNC)
				goto txtstamp_out;
			fallthrough;
		case HWTSTAMP_TX_ON:
			ptp_cb = (struct qca808x_ptp_cb *)skb->cb;
			ptp_cb->ptp_type = type;
			ptp_cb->header = ptp_header;
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
			skb_queue_tail(&ptp_info->tx_queue, skb);
			ptp_schedule_worker(ptp_info->ptp_clock, 0);
			return;
		case HWTSTAMP_TX_OFF:
		default:
			break;
	}

txtstamp_out:
	kfree_skb(skb);
	return;
}

static int qca808x_ts_info(struct mii_timestamper *mii_ts,
			   struct ethtool_ts_info *info)
{
	struct qca808x_ptp_info *ptp_info = container_of(mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);

	info->phc_index = ptp_clock_index(ptp_info->ptp_clock);
	info->so_timestamping = SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;
	info->tx_types = BIT(HWTSTAMP_TX_OFF) |
			 BIT(HWTSTAMP_TX_ON) |
			 BIT(HWTSTAMP_TX_ONESTEP_SYNC) |
			 BIT(HWTSTAMP_TX_ONESTEP_P2P);

	info->rx_filters = BIT(HWTSTAMP_FILTER_NONE) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_EVENT);

	return 0;
}

static long qca808x_ptp_do_aux_work(struct ptp_clock_info *ptp)
{
	struct qca808x_ptp_info *ptp_info = container_of(ptp,
							 struct qca808x_ptp_info,
							 caps);
	bool reschedule = false;

	rx_timestamp_work(ptp_info);
	reschedule = tx_timestamp_work(ptp_info);

	return reschedule ? 1 : -1;
}

static int qca808x_ptp_register(struct qca808x_ptp_info *ptp_info)
{
	struct phy_device *phydev = ptp_info->phydev;

	ptp_info->caps = (struct ptp_clock_info) {
		.owner		= THIS_MODULE,
		.name		= "QCA8XXX PHC",
		.max_adj	= S32_MAX,
		.n_ext_ts	= 1,
		.n_per_out	= 1,
		.verify		= qca808x_ptp_verify,
		.gettime64	= qca808x_ptp_gettime,
		.settime64	= qca808x_ptp_settime,
		.adjfine	= qca808x_ptp_adjfine,
		.adjtime	= qca808x_ptp_adjtime,
		.enable		= qca808x_ptp_enable,
		.do_aux_work	= qca808x_ptp_do_aux_work,
	};

	ptp_info->ptp_clock = ptp_clock_register(&ptp_info->caps,
						 &phydev->mdio.dev);

	if (IS_ERR(ptp_info->ptp_clock))
		return PTR_ERR(ptp_info->ptp_clock);

	return 0;
}

int qca808x_ptp_config_init(struct phy_device *phydev)
{
	int ret;

	/* Set rtc clock to asynchronization mode*/
	ret = qca808x_ptp_clock_incval_mode_set(phydev, false);
	if (ret)
		return ret;

	/* Set frequency to 8ns(125MHz) */
	ret = qca808x_ptp_rtc_incval_set(phydev, QCA808X_PTP_TICK_RATE_125M, 0);
	if (ret)
		return ret;

	/* Set SyncE reference clock */
	ret = qca808x_ptp_rtc_reference_set(phydev, PTP_RTC_REF_CLOCK_SYNCE);
	if (ret)
		return ret;

	/* Enable Embed mode to get RX timestamp */
	ret = qca808x_rx_timestamp_mode_set(phydev, true);
	if (ret)
		return ret;

	/* Disable SYNCE clock output, only BC/TC need to enable it. */
	return qca808x_ptp_clock_synce_clock_enable(phydev, false);
}

int qca808x_ptp_probe(struct phy_device *phydev)
{
	struct qca808x_ptp_info *ptp_info;
	int ret;

	ptp_info = devm_kzalloc(&phydev->mdio.dev, sizeof(*ptp_info), GFP_KERNEL);
	if (!ptp_info)
		return -ENOMEM;

	ptp_info->phydev = phydev;
	ptp_info->hwts_tx_type = HWTSTAMP_TX_OFF;
	ptp_info->hwts_rx_type = PTP_CLASS_NONE;
	ptp_info->mii_ts.rxtstamp = qca808x_rxtstamp;
	ptp_info->mii_ts.txtstamp = qca808x_txtstamp;
	ptp_info->mii_ts.hwtstamp = qca808x_hwtstamp;
	ptp_info->mii_ts.ts_info  = qca808x_ts_info;
	ptp_info->mii_ts.link_state = qca808x_ptp_change_notify;
	ptp_info->phydev->mii_ts = &ptp_info->mii_ts;
	INIT_LIST_HEAD(&ptp_info->list);

	skb_queue_head_init(&ptp_info->tx_queue);
	skb_queue_head_init(&ptp_info->rx_queue);
	mutex_init(&ptp_info->tsreg_lock);

	mutex_lock(&qca8xxx_phcs_lock);
	list_add_tail(&qca8xxx_phcs, &ptp_info->list);
	mutex_unlock(&qca8xxx_phcs_lock);

	ret = qca808x_ptp_register(ptp_info);
	if (ret)
		phydev_err(phydev, "PHC register failed\n");

	return ret;
}

void qca808x_ptp_remove(struct phy_device *phydev)
{
	struct qca808x_ptp_info *ptp_info = container_of(phydev->mii_ts,
							 struct qca808x_ptp_info,
							 mii_ts);

	if (ptp_info->ptp_clock)
		ptp_clock_unregister(ptp_info->ptp_clock);

	mutex_lock(&qca8xxx_phcs_lock);
	list_del(&ptp_info->list);
	mutex_unlock(&qca8xxx_phcs_lock);

	skb_queue_purge(&ptp_info->tx_queue);
	skb_queue_purge(&ptp_info->rx_queue);
	phydev->mii_ts = NULL;
}

static int qca808x_ptp_callback_init(struct device *dev, void *p)
{
	struct phy_device *phydev = to_phy_device(dev);
	int rv = 0;

	/* phydev is null or phydev has been initialzed wiht ptp */
	if (!phydev || phydev->mii_ts)
		return rv;

	rv = qca808x_ptp_probe(phydev);
	if (rv)
		return rv;

	return qca808x_ptp_config_init(phydev);
}

static int qca808x_ptp_callback_cleanup(struct device *dev, void *p)
{
	struct phy_device *phydev = to_phy_device(dev);

	if (phydev && phydev->mii_ts)
		qca808x_ptp_remove(phydev);

	return 0;
}

static int __init qca8xx_phc_module_init(void)
{
	struct device_driver *drv;
	int i, rv;

	for (i = 0; i < ARRAY_SIZE(qca_phy_driver_ptp_supported_names); i++) {
		drv = driver_find(qca_phy_driver_ptp_supported_names[i], &mdio_bus_type);
		if (drv) {
			rv = driver_for_each_device(drv, NULL, NULL,
						    qca808x_ptp_callback_init);
			if (rv)
				return rv;
		}
	}

	return 0;
}

static void __exit qca8xx_phc_module_exit(void)
{
	struct device_driver *drv = NULL;
	int i, rv;

	for (i = 0; i < ARRAY_SIZE(qca_phy_driver_ptp_supported_names); i++) {
		drv = driver_find(qca_phy_driver_ptp_supported_names[i], &mdio_bus_type);
		if (drv) {
			rv = driver_for_each_device(drv, NULL, NULL,
						    qca808x_ptp_callback_cleanup);

			if (rv)
				pr_err("%s with %s failed\n", __func__,
				       qca_phy_driver_ptp_supported_names[i]);
		}
	}
}

module_init(qca8xx_phc_module_init);
module_exit(qca8xx_phc_module_exit);
MODULE_DESCRIPTION("Qualcomm QCA8XXX PHC driver");
MODULE_LICENSE("Dual BSD/GPL");
