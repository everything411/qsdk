/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "qca81xx.h"


static u32 qca81xx_mib_addr(u32 sc_index, u32 sa_index, u32 addr)
{
	u32 reg = 0;

	switch (addr) {
	case RX_SA_UNUSED_PKTS_BASE:
	case RX_SA_NOUSING_PKTS_BASE:
	case RX_SA_NOTVALID_PKTS_BASE:
	case RX_SA_INVALID_PKTS_BASE:
	case RX_SA_OK_PKTS_BASE:
	case TX_SA_PROTECTED_PKTS_BASE:
	case TX_SA_ENCRYPTED_PKTS_BASE:
	case TX_SA_PROTECTED_PKTS_HIGH_BASE:
	case TX_SA_ENCRYPTED_PKTS_HIGH_BASE:
	case RX_SA_UNUSED_PKTS_HIGH_BASE:
	case RX_SA_NOUSING_PKTS_HIGH_BASE:
	case RX_SA_NOTVALID_PKTS_HIGH_BASE:
	case RX_SA_INVALID_PKTS_HIGH_BASE:
	case RX_SA_OK_PKTS_HIGH_BASE:
		reg = addr + (sc_index * 2 + sa_index) * 2;
		break;
	case RX_SC_LATE_PKTS_BASE:
	case RX_SC_DELAYED_PKTS_BASE:
	case RX_SC_UNCHECKED_PKTS_BASE:
	case RX_SC_VALIDATED_PKTS_BASE:
	case RX_SC_DECRYPTED_PKTS_BASE:
	case TX_SC_PROTECTED_OCTETS_BASE:
	case TX_SC_ENCRYPTED_OCTETS_BASE:
	case TX_SC_PROTECTED_OCTETS_HIGH_BASE:
	case TX_SC_ENCRYPTED_OCTETS_HIGH_BASE:
	case RX_SC_LATE_PKTS_HIGH_BASE:
	case RX_SC_DELAYED_PKTS_HIGH_BASE:
	case RX_SC_UNCHECKED_PKTS_HIGH_BASE:
	case RX_SC_VALIDATED_PKTS_HIGH_BASE:
	case RX_SC_DECRYPTED_PKTS_HIGH_BASE:
		reg = addr + sc_index * 2;
		break;
	default:
		reg = addr;
		break;
	}
	return reg;
}

static int qca81xx_secy_mib_read(struct phy_device *phydev,
				u32 reg_addr, u32 *mib_val)
{
	int val = 0;

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, reg_addr + 1);
	if (unlikely(val < 0))
		return val;
	*mib_val = (val & 0xffff) << 16;
	val = phy_read_mmd(phydev, MDIO_MMD_PCS, reg_addr);
	if (unlikely(val < 0))
		return val;
	*mib_val |= val & 0xffff;

	return 0;
}

static int qca81xx_secy_rx_sa_mib_get(struct phy_device *phydev,
			u32 sc_index, u32 an, struct secy_rx_sa_mib_t *mib)
{
	u32 mib_low = 0, mib_high = 0;
	u32 sa_index = 0;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM) || (mib == NULL))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);

	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SA_UNUSED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SA_UNUSED_PKTS_BASE), &mib_low);
	mib->unused_sa = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SA_NOUSING_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SA_NOUSING_PKTS_BASE), &mib_low);
	mib->not_using_sa = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SA_NOTVALID_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SA_NOTVALID_PKTS_BASE), &mib_low);
	mib->not_valid_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
		       qca81xx_mib_addr(sc_index, sa_index,
		       RX_SA_INVALID_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
		       qca81xx_mib_addr(sc_index, sa_index,
		       RX_SA_INVALID_PKTS_BASE), &mib_low);
	mib->invalid_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
		       qca81xx_mib_addr(sc_index, sa_index,
		       RX_SA_OK_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
		       qca81xx_mib_addr(sc_index, sa_index,
		       RX_SA_OK_PKTS_BASE), &mib_low);
	mib->ok_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	return 0;
}

static int qca81xx_secy_tx_sa_mib_get(struct phy_device *phydev,
			u32 sc_index, u32 an, struct secy_tx_sa_mib_t *mib)
{
	u32 mib_low = 0, mib_high = 0;
	u32 sa_index = 0;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM) || (mib == NULL))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);

	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SA_PROTECTED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SA_PROTECTED_PKTS_BASE), &mib_low);
	mib->protected_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SA_ENCRYPTED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SA_ENCRYPTED_PKTS_BASE), &mib_low);
	mib->encrypted_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	return 0;
}

static int qca81xx_secy_rx_sc_mib_get(struct phy_device *phydev,
				u32 sc_index, struct secy_rx_sc_mib_t *mib)
{
	u32 mib_low = 0, mib_high = 0;
	u32 sa_index = 0;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (mib == NULL))
		return -EINVAL;

	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_LATE_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_LATE_PKTS_BASE), &mib_low);
	mib->late_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
				RX_SC_DELAYED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_DELAYED_PKTS_BASE), &mib_low);
	mib->delayed_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_UNCHECKED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_UNCHECKED_PKTS_BASE), &mib_low);
	mib->unchecked_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_VALIDATED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_VALIDATED_PKTS_BASE), &mib_low);
	mib->validated_octets = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_DECRYPTED_PKTS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			RX_SC_DECRYPTED_PKTS_BASE), &mib_low);
	mib->decrypted_octets = ((u64)mib_high << 32) | (u64)mib_low;

	return 0;
}

static int qca81xx_secy_tx_sc_mib_get(struct phy_device *phydev,
				u32 sc_index, struct secy_tx_sc_mib_t *mib)
{
	u32 mib_low = 0, mib_high = 0;
	u32 sa_index = 0;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (mib == NULL))
		return -EINVAL;

	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SC_PROTECTED_OCTETS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SC_PROTECTED_OCTETS_BASE), &mib_low);
	mib->protected_octets = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SC_ENCRYPTED_OCTETS_HIGH_BASE), &mib_high);
	qca81xx_secy_mib_read(phydev,
			qca81xx_mib_addr(sc_index, sa_index,
			TX_SC_ENCRYPTED_OCTETS_BASE), &mib_low);
	mib->encrypted_octets = ((u64)mib_high << 32) | (u64)mib_low;

	return 0;
}

static int qca81xx_secy_rx_mib_get(struct phy_device *phydev,
				struct secy_rx_mib_t *mib)
{
	u32 mib_low = 0, mib_high = 0;

	if (mib == NULL)
		return -EINVAL;

	qca81xx_secy_mib_read(phydev, RX_UNTAGGED_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, RX_UNTAGGED_PKTS, &mib_low);
	mib->untagged_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev, RX_NO_TAG_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, RX_NO_TAG_PKTS, &mib_low);
	mib->notag_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev, RX_BAD_TAG_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, RX_BAD_TAG_PKTS, &mib_low);
	mib->bad_tag_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev, RX_UNKNOWN_SCI_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, RX_UNKNOWN_SCI_PKTS, &mib_low);
	mib->unknown_sci_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev, RX_NO_SCI_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, RX_NO_SCI_PKTS, &mib_low);
	mib->no_sci_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0;
	mib_high = 0;
	qca81xx_secy_mib_read(phydev, RX_OVERRUN_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, RX_OVERRUN_PKTS, &mib_low);
	mib->overrun_packets = ((u64)mib_high << 32) | (u64)mib_low;

	return 0;
}

static int qca81xx_secy_tx_mib_get(struct phy_device *phydev,
				struct secy_tx_mib_t *mib)
{
	u32 mib_low = 0, mib_high = 0;

	if (mib == NULL)
		return -EINVAL;

	qca81xx_secy_mib_read(phydev, TX_UNTAGGED_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, TX_UNTAGGED_PKTS, &mib_low);
	mib->untagged_pkts = ((u64)mib_high << 32) | (u64)mib_low;

	mib_low = 0; mib_high = 0;
	qca81xx_secy_mib_read(phydev, TX_TOO_LONG_PKTS_HIGH, &mib_high);
	qca81xx_secy_mib_read(phydev, TX_TOO_LONG_PKTS, &mib_low);
	mib->too_long = ((u64)mib_high << 32) | (u64)mib_low;

	return 0;
}

static int qca81xx_secy_tx_sak_set(struct phy_device *phydev, u32 sc_index,
				u32 an, struct secy_sak_t *key)
{
	u16 val = 0;
	u16 reg;
	int i, j, reg_offset;
	u32 channel, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM) ||
		(key == NULL))
		return -EINVAL;

	if (!((key->len == QCA_GCM_AES_128_SAK_LEN) ||
		(key->len == QCA_GCM_AES_256_SAK_LEN) ||
		(key->len == 0)))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	channel = sc_index * 2 + sa_index;

	if (key->len == QCA_GCM_AES_256_SAK_LEN) {
		for (i = 0; i < 16; i++) {
			if (i < 8) {
				val = (key->sak[i * 2 + 1] << 8) |
					key->sak[i * 2];
				reg = MACSEC_TX_SAK_KEY0(channel);
				reg_offset = i;
			} else {
				j = i - 8;
				val = (key->sak1[j * 2 + 1] << 8) |
					key->sak1[j * 2];
				reg = MACSEC_TX_EXTENDED_SAK_KEY0(channel);
				reg_offset = j;
			}
			phy_write_mmd(phydev, MDIO_MMD_PCS,
					reg + reg_offset, val);
		}
	} else {
		reg = MACSEC_TX_SAK_KEY0(channel);
		for (i = 0; i < 8; i++) {
			val = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
			reg_offset = i;
			phy_write_mmd(phydev, MDIO_MMD_PCS,
					reg + reg_offset, val);
		}
	}
	return 0;
}

static int qca81xx_secy_rx_sak_set(struct phy_device *phydev, u32 sc_index,
				u32 an, struct secy_sak_t *key)
{
	u16 val = 0;
	u16 reg;
	int i, j, reg_offset;
	u32 channel, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM) ||
		(key == NULL))
		return -EINVAL;

	if (!((key->len == QCA_GCM_AES_128_SAK_LEN) ||
		(key->len == QCA_GCM_AES_256_SAK_LEN) ||
		(key->len == 0)))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	channel = sc_index * 2 + sa_index;

	if (key->len == QCA_GCM_AES_256_SAK_LEN) {
		for (i = 0; i < 16; i++) {
			if (i < 8) {
				val = (key->sak[i * 2 + 1] << 8) |
					key->sak[i * 2];
				reg = MACSEC_RX_SAK_KEY0(channel);
				reg_offset = i;
			} else {
				j = i - 8;
				val = (key->sak1[j * 2 + 1] << 8) |
					key->sak1[j * 2];
				reg = MACSEC_RX_EXTENDED_SAK_KEY0(channel);
				reg_offset = j;
			}
			phy_write_mmd(phydev, MDIO_MMD_PCS,
					reg + reg_offset, val);
		}
	} else {
		for (i = 0; i < 8; i++) {
			reg = MACSEC_RX_SAK_KEY0(channel);
			val = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
			phy_write_mmd(phydev, MDIO_MMD_PCS,
					reg + i, val);
		}
	}
	return 0;
}

static int qca81xx_secy_tx_sa_npn_set(struct phy_device *phydev,
					u32 sc_index, u32 an, u64 next_pn)
{
	u32 npn = 0;
	u16 val = 0;
	u16 channel = 0;
	int ret;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	channel = (sc_index * 2 + SECY_AN_TO_SA_MAPPING(an));

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MACSEC_SYS_FRAME_CTRL);
	if (ret < 0)
		return ret;
	if (SYS_XPN_EN & ret) {
		npn = (u32)((next_pn >> 16) >> 16);
		val = (u16)(npn & 0xffff);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
				MACSEC_TX_XPN(channel), val);
		val = (u16)((npn >> 16) & 0xffff);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
				MACSEC_TX_XPN(channel) + 1, val);
	}
	npn = (u32)(next_pn & 0xffffffff);
	val = (u16)(npn & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_TX_NPN(channel), val);
	val = (u16)((npn >> 16) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_TX_NPN(channel) + 1, val);
	return 0;
}

static int qca81xx_secy_rx_sa_npn_set(struct phy_device *phydev,
					u32 sc_index, u32 an, u64 next_pn)
{
	u32 npn = 0;
	u16 val = 0;
	u16 channel = 0;
	int ret;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	channel = (sc_index * 2 + SECY_AN_TO_SA_MAPPING(an));

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MACSEC_SYS_FRAME_CTRL);
	if (ret < 0)
		return ret;
	if (SYS_XPN_EN & ret) {
		npn = (u32)((next_pn >> 16) >> 16);
		val = (u16)(npn & 0xffff);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
				MACSEC_RX_XPN(channel), val);
		val = (u16)((npn >> 16) & 0xffff);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
				MACSEC_RX_XPN(channel) + 1, val);
	}
	npn = (u32)(next_pn & 0xffffffff);
	val = (u16)(npn & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_RX_NPN(channel), val);
	val = (u16)((npn >> 16) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_RX_NPN(channel) + 1, val);
	return 0;
}

static int qca81xx_secy_tx_sa_create(struct phy_device *phydev,
					u32 sc_index, u32 an)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index/4;
	chan_shift = 2*((sc_index%4) * 2 + sa_index);
	reg += MACSEC_TX_AN_BASE;
	val = (((u16)an&AN_MASK) << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
				(AN_MASK << chan_shift), val);
}

static int qca81xx_secy_tx_sa_del(struct phy_device *phydev,
					u32 sc_index, u32 an)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index/4;
	chan_shift = 2*((sc_index%4) * 2 + sa_index);
	reg += MACSEC_TX_AN_BASE;
	val = (0x0 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
				(AN_MASK << chan_shift), val);
}

static int qca81xx_secy_tx_sa_en_set(struct phy_device *phydev,
				u32 sc_index, u32 an, bool enable)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index/8;
	chan_shift = ((sc_index%8) * 2 + sa_index);

	reg += MACSEC_TX_SA_CONTROL;

	if (enable)
		val = (1 << chan_shift);
	else
		val &= ~(1 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
				(1<<chan_shift), val);
}

static int qca81xx_secy_rx_sa_en_set(struct phy_device *phydev,
				u32 sc_index, u32 an, bool enable)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index/8;
	chan_shift = ((sc_index%8) * 2 + sa_index);

	reg += MACSEC_RX_SA_CONTROL;

	if (enable)
		val  = (1 << chan_shift);
	else
		val &= ~(1 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
				(1<<chan_shift), val);
}

static int qca81xx_secy_rx_sa_create(struct phy_device *phydev,
					u32 sc_index, u32 an)
{
	u16 val = 0, chan_shift;
	u32 reg, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	qca81xx_secy_rx_sa_npn_set(phydev, sc_index, an, 1);

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index/4;
	chan_shift = 2*((sc_index%4) * 2 + sa_index);
	reg += MACSEC_RX_AN_BASE;
	val  = (((u16)an&AN_MASK) << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
				(AN_MASK << chan_shift), val);
}

static int qca81xx_secy_rx_sa_del(struct phy_device *phydev,
					u32 sc_index, u32 an)
{
	u16 val = 0, chan_shift;
	u32 reg, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index/4;
	chan_shift = 2*((sc_index%4) * 2 + sa_index);
	reg += MACSEC_RX_AN_BASE;
	val  = (0x0 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
				(AN_MASK << chan_shift), val);
}

static int qca81xx_secy_tx_sc_policy_set(struct phy_device *phydev,
				u32 rule_index,
				struct secy_tx_sc_policy_rule_t *rule)
{
	u16 val = 0, msk = 0;
	u32 shifted_value = 0, i = 0;

	if ((rule_index >= QCA_SECY_SC_MAX_NUM) || (rule == NULL))
		return -1;

	if (rule->rule_valid)
		msk |= SC_BIND_MASK_VALID;

	if (rule->rule_mask & SC_BIND_MASK_DA)
		msk |= SC_BIND_MASK_DA;

	for (i = 0; i < 3; i++) {
		val = (rule->mac_da.addr[i * 2] << 8) |
			(rule->mac_da.addr[i * 2 + 1]);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXDA_BASE(rule_index) + 2 - i, val);
	}

	if (rule->rule_mask & SC_BIND_MASK_SA)
		msk |= SC_BIND_MASK_SA;

	for (i = 0; i < 3; i++) {
		val = (rule->mac_sa.addr[i * 2] << 8) |
			(rule->mac_sa.addr[i * 2 + 1]);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXSA_BASE(rule_index) + 2 - i, val);
	}

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_ETHERTYPE) {
		val = rule->ethtype;
		msk |= SC_BIND_MASK_ETHERTYPE;
	}
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXETHERTYPE(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_OUTER_VLAN) {
		val = rule->outer_vlanid & 0xfff;
		msk |= SC_BIND_MASK_OUTER_VLAN;
	}
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXOUTVTAG(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_INNER_VLAN) {
		val = rule->inner_vlanid & 0xfff;
		msk |= SC_BIND_MASK_INNER_VLAN;
	}
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXINVTAG(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_BCAST) {
		val |= (rule->bc_flag == true) ? SC_BIND_TX_IFBC : 0;
		msk |= SC_BIND_MASK_BCAST;
	}
	val |= FIELD_PREP(SC_BIND_TXCTX_M, rule->action.tx_sc_index);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXCTX(rule_index), val);

	val = 0;
	val |= FIELD_PREP(SC_BIND_TXTCI_M, rule->action.tx_tci);
	val |= FIELD_PREP(SC_BIND_TXOFFSET_M, rule->action.encryption_offset);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXTCI(rule_index), val);

	for (i = 0; i < 4; i++) {
		shifted_value = (i < 2) ? lower_32_bits(rule->action.tx_sci) :
					upper_32_bits(rule->action.tx_sci);
		val = (u16)(shifted_value >> ((i % 2) * 16)) & 0xFFFF;
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXSCI_BASE(rule_index) + i, val);
	}

	return phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXMASK(rule_index), msk);
}

static int qca81xx_secy_rx_sc_policy_set(struct phy_device *phydev,
				u32 rule_index,
				struct secy_rx_sc_policy_rule_t *rule)
{
	u16 val = 0, msk = 0;
	u32 shifted_value = 0, i = 0;

	if ((rule_index >= QCA_SECY_SC_MAX_NUM) || (rule == NULL))
		return -1;

	if (rule->rule_valid)
		msk |= SC_BIND_MASK_VALID;

	if (rule->rule_mask & SC_BIND_MASK_DA)
		msk |= SC_BIND_MASK_DA;

	for (i = 0; i < 3; i++) {
		val = (rule->mac_da.addr[i * 2] << 8) |
			(rule->mac_da.addr[i * 2 + 1]);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_RXDA_BASE(rule_index) + 2 - i, val);
	}

	if (rule->rule_mask & SC_BIND_MASK_SA)
		msk |= SC_BIND_MASK_SA;

	for (i = 0; i < 3; i++) {
		val = (rule->mac_sa.addr[i * 2] << 8) |
			(rule->mac_sa.addr[i * 2 + 1]);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_RXSA_BASE(rule_index) + 2 - i, val);
	}

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_ETHERTYPE) {
		val = rule->ethtype;
		msk |= SC_BIND_MASK_ETHERTYPE;
	}
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		MACSEC_SC_BIND_RXETHERTYPE(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_OUTER_VLAN) {
		val = rule->outer_vlanid & 0xfff;
		msk |= SC_BIND_MASK_OUTER_VLAN;
	}
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		MACSEC_SC_BIND_RXOUTVTAG(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_INNER_VLAN) {
		val = rule->inner_vlanid & 0xfff;
		msk |= SC_BIND_MASK_INNER_VLAN;
	}
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		MACSEC_SC_BIND_RXINVTAG(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_BCAST) {
		val |= (rule->bc_flag == true) ? SC_BIND_RX_IFBC : 0;
		msk |= SC_BIND_MASK_BCAST;
	}
	val |= FIELD_PREP(SC_BIND_RXCTX_M, rule->action.rx_sc_index);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		MACSEC_SC_BIND_RXCTX(rule_index), val);

	val = 0;
	if (rule->rule_mask & SC_BIND_MASK_TCI) {
		val |= FIELD_PREP(SC_BIND_RXTCI_M, rule->rx_tci);
		msk |= SC_BIND_MASK_TCI;
	}
	val |= FIELD_PREP(SC_BIND_RXOFFSET_M, rule->action.decryption_offset);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		MACSEC_SC_BIND_RXTCI(rule_index), val);

	for (i = 0; i < 4; i++) {
		shifted_value = (i < 2) ? lower_32_bits(rule->rx_sci) :
					upper_32_bits(rule->rx_sci);
		val = (u16)(shifted_value >> ((i % 2) * 16)) & 0xFFFF;
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_RXSCI_BASE(rule_index) + i, val);
	}
	if (msk != 0)
		msk |= SC_BIND_MASK_SCI;

	return phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_RXMASK(rule_index), msk);
}

static int qca81xx_secy_cipher_suite_set(struct phy_device *phydev,
					struct macsec_secy *secy)
{
	u16 val = 0;
	int ret = 0;

	if (secy->key_len == QCA_GCM_AES_256_SAK_LEN)
		val |= SYS_AES256_EN;

	if (secy->xpn)
		val |= SYS_XPN_EN;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, MACSEC_SYS_FRAME_CTRL,
				SYS_AES256_EN|SYS_XPN_EN, val);
	return ret;
}

static int qca_get_txsc_idx_from_secy(struct qca_macsec_cfg_t *macsec_cfg,
					const struct macsec_secy *secy)
{
	int i;

	if (unlikely(!secy))
		return -1;

	for (i = 0; i < QCA_SECY_SC_MAX_NUM; i++) {
		if (macsec_cfg->secy_txsc[i].sw_secy == secy)
			return i;
	}
	return -1;
}

static int qca_get_rxsc_idx_from_rxsc(struct qca_macsec_cfg_t *macsec_cfg,
					const struct macsec_rx_sc *rxsc)
{
	int i;

	if (unlikely(!rxsc))
		return -1;

	for (i = 0; i < QCA_SECY_SC_MAX_NUM; i++) {
		if (macsec_cfg->secy_rxsc[i].rx_sc == rxsc)
			return i;
	}
	return -1;
}

static u8 qca_tx_tci_convert(struct macsec_secy *secy)
{
	u8 tci = 0;

	if (secy->tx_sc.encrypt)
		tci |= MACSEC_TCI_E;
	if (secy->tx_sc.scb)
		tci |= MACSEC_TCI_SCB;
	if (secy->tx_sc.send_sci)
		tci |= MACSEC_TCI_SC;
	if (secy->tx_sc.end_station)
		tci |= MACSEC_TCI_ES;
	/* The C bit is clear if and only if the Secure Data is
	 * exactly the same as the User Data and the ICV is 16 octets long.
	 */
	if (!(secy->icv_len == MACSEC_DEFAULT_ICV_LEN && !secy->tx_sc.encrypt))
		tci |= MACSEC_TCI_C;
	return tci;
}

static int qca_update_txsc(struct phy_device *phydev,
			const u32 channel, struct macsec_secy *secy)
{
	int ret = 0;
	u16 val = 0;
	u8 tci = 0;

	tci = qca_tx_tci_convert(secy);

	val |= FIELD_PREP(SC_BIND_TXTCI_M, tci);
	/* default supports encryption_offset 0 */
	val |= FIELD_PREP(SC_BIND_TXOFFSET_M, 0);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SC_BIND_TXTCI(channel), val);
	if (ret)
		return ret;

	val = 0;
	if (tci & MACSEC_TCI_SCB)
		val |= SYS_USE_SCB_EN;
	if (tci & MACSEC_TCI_SC)
		val |= SYS_INCLUDED_SCI_EN;
	if (tci & MACSEC_TCI_ES)
		val |= SYS_USE_ES_EN;
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SYS_CONFIG,
			SYS_INCLUDED_SCI_EN |
			SYS_USE_ES_EN |
			SYS_USE_SCB_EN, val);
	if (ret)
		return ret;

	val = secy->protect_frames ? SYS_FRAME_PROTECT_EN : 0;
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SYS_FRAME_CTRL,
			SYS_FRAME_PROTECT_EN, val);
	return ret;
}

static int qca_create_txsc(struct phy_device *phydev,
			const u32 channel, struct macsec_secy *secy)
{
	int ret = 0;
	struct secy_tx_sc_policy_rule_t entry;

	/* create tx_sc policy channel */
	memset(&entry, 0, sizeof(entry));

	entry.rule_valid = true;
	entry.action.tx_sc_index = channel;
	entry.action.tx_tci = qca_tx_tci_convert(secy);
	entry.action.tx_sci = swab64((__force u64)secy->sci);

	ret = qca81xx_secy_tx_sc_policy_set(phydev, channel, &entry);
	if (ret) {
		phydev_warn(phydev, "%s: fail to create tx_sc policy %d!\n",
			__func__, channel);
		return ret;
	}

	ret = qca_update_txsc(phydev, channel, secy);
	return ret;
}

static int qca_update_rxsc(struct phy_device *phydev,
			const u32 channel, struct macsec_secy *secy)
{
	int ret = 0;
	u16 val;

	/* rx validate frame  */
	if (secy->validate_frames == MACSEC_VALIDATE_STRICT)
		val = SYS_FRAME_VALIDATE_STRICT;
	else if (secy->validate_frames == MACSEC_VALIDATE_CHECK)
		val = SYS_FRAME_VALIDATE_CHECK;
	else
		val = SYS_FRAME_VALIDATE_DIS;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SYS_FRAME_CTRL,
			SYS_FRAME_VALIDATE_M,
			FIELD_PREP(SYS_FRAME_VALIDATE_M, val));
	if (ret)
		return ret;

	val = secy->replay_protect ? SYS_FRAME_PROTECT_EN : 0;
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SYS_CONFIG, SYS_REPLAY_PROTECT_EN, val);
	if (ret)
		return ret;

	val = (u16)(secy->replay_window & 0xffff);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SYS_REPLAY_WIN_BASE, val);
	if (ret)
		return ret;
	val = (u16)((secy->replay_window >> 16)  & 0xffff);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SYS_REPLAY_WIN_BASE + 1, val);
	return ret;
}

static int qca_create_rxsc(struct phy_device *phydev, const u32 channel,
			struct macsec_secy *secy, struct macsec_rx_sc *rx_sc)
{
	int ret = 0;
	struct secy_rx_sc_policy_rule_t entry;

	/* create rx_sc policy channel */
	memset(&entry, 0, sizeof(entry));

	entry.rule_valid = true;
	entry.action.rx_sc_index = channel;
	entry.rx_sci = swab64((__force u64)rx_sc->sci);

	ret = qca81xx_secy_rx_sc_policy_set(phydev, channel, &entry);
	if (ret) {
		phydev_warn(phydev, "%s: fail to create rx_sc policy %d!\n",
			__func__, channel);
		return ret;
	}

	ret = qca_update_rxsc(phydev, channel, secy);
	return ret;
}

static int qca_update_txsa(struct phy_device *phydev,
				const int channel,
				const unsigned char an,
				const struct macsec_tx_sa *tx_sa)
{
	int ret = 0;

	ret = qca81xx_secy_tx_sa_npn_set(phydev,
				channel, an, tx_sa->next_pn_halves.lower);
	if (ret) {
		phydev_warn(phydev,
			"%s: fail to tx_sa_next_pn_set!\n", __func__);
		return ret;
	}
	ret = qca81xx_secy_tx_sa_en_set(phydev,
				channel, an, tx_sa->active);
	return ret;
}

static int qca_update_rxsa(struct phy_device *phydev, const int channel,
		const unsigned char an, const struct macsec_rx_sa *rx_sa)
{
	int ret = 0;

	ret = qca81xx_secy_rx_sa_npn_set(phydev,
				channel, an, rx_sa->next_pn_halves.lower);
	if (ret) {
		phydev_warn(phydev, "%s: fail to rx_sa_next_pn_set!\n",
			__func__);
		return ret;
	}

	ret = qca81xx_secy_rx_sa_en_set(phydev,
				channel, an, rx_sa->active);
	return ret;
}

static int qca_mdo_dev_open(struct macsec_context *ctx)
{
	int ret = 0;

	ret = phy_modify_mmd(ctx->phydev, MDIO_MMD_PCS,
				MACSEC_SYS_PACKET_CTRL,
				SYS_SECY_LPBK_M,
				FIELD_PREP(SYS_SECY_LPBK_M, SYS_MACSEC_EN));
	if (ret)
		return ret;

	ret = phy_set_bits_mmd(ctx->phydev, MDIO_MMD_AN,
				MACSEC_SHADOW_REGISTER,
				MACSEC_SHADOW_DUPLEX_EN |
				MACSEC_SHADOW_LEGACY_DUPLEX_EN);
	if (ret)
		return ret;

	ret = phy_set_bits_mmd(ctx->phydev, MDIO_MMD_PCS,
				MACSEC_SYS_PORT_CTRL, SYS_PORT_EN);
	return ret;
}

static int qca_mdo_dev_stop(struct macsec_context *ctx)
{
	int ret = 0;

	ret = phy_clear_bits_mmd(ctx->phydev, MDIO_MMD_PCS,
				MACSEC_SYS_PORT_CTRL, SYS_PORT_EN);
	if (ret)
		return ret;

	ret = phy_modify_mmd(ctx->phydev, MDIO_MMD_PCS,
				MACSEC_SYS_PACKET_CTRL,
				SYS_SECY_LPBK_M,
				FIELD_PREP(SYS_SECY_LPBK_M, SYS_BYPASS));
	if (ret)
		return ret;

	ret = phy_clear_bits_mmd(ctx->phydev, MDIO_MMD_AN,
				MACSEC_SHADOW_REGISTER,
				MACSEC_SHADOW_DUPLEX_EN |
				MACSEC_SHADOW_LEGACY_DUPLEX_EN);
	return ret;
}

static int qca_mdo_add_secy(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	u32 txsc_idx = 0;
	int ret = 0;

	if (ctx->secy->xpn)
		return -EOPNOTSUPP;

	if (hweight32(pcfg->txsc_idx_bits) >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	txsc_idx = ffz(pcfg->txsc_idx_bits);
	if (txsc_idx >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	ret = qca81xx_secy_cipher_suite_set(ctx->phydev, ctx->secy);
	if (ret) {
		phydev_warn(ctx->phydev,
			"%s: fail to set secy_cipher_suite!\n", __func__);
		return ret;
	}

	ret = qca_create_txsc(ctx->phydev, txsc_idx, ctx->secy);
	if (ret) {
		phydev_warn(ctx->phydev, "%s: fail to add secy!\n", __func__);
		return ret;
	}

	pcfg->secy_txsc[txsc_idx].hw_sc_idx = txsc_idx;
	pcfg->secy_txsc[txsc_idx].sw_secy = ctx->secy;
	set_bit(txsc_idx, &pcfg->txsc_idx_bits);

	return 0;
}

static int qca_mdo_upd_secy(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	int channel = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	return qca_update_txsc(ctx->phydev, channel, ctx->secy);
}

static int qca_mdo_del_secy(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_tx_sc_policy_rule_t entry;
	int channel = 0, ret = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&entry, 0, sizeof(entry));
	ret = qca81xx_secy_tx_sc_policy_set(ctx->phydev, channel, &entry);

	clear_bit(channel, &pcfg->txsc_idx_bits);
	pcfg->secy_txsc[channel].sw_secy = NULL;
	pcfg->secy_txsc[channel].hw_sc_idx = 0;

	return ret;
}

static int qca_mdo_add_txsa(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_sak_t tx_sak;
	int channel = 0, ret = 0;
	int i;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&tx_sak, 0, sizeof(struct secy_sak_t));
	tx_sak.len = ctx->secy->key_len;
	if (ctx->secy->key_len == QCA_GCM_AES_128_SAK_LEN) {
		for (i = 0; i < 16; i++)
			tx_sak.sak[i] = ctx->sa.key[15 - i];

	} else if (ctx->secy->key_len == QCA_GCM_AES_256_SAK_LEN) {
		for (i = 0; i < 16; i++) {
			tx_sak.sak1[i] = ctx->sa.key[15 - i];
			tx_sak.sak[i] = ctx->sa.key[31 - i];
		}
	} else {
		phydev_err(ctx->phydev, "%s: error key_len!\n", __func__);
		return -EOPNOTSUPP;
	}

	ret = qca81xx_secy_tx_sak_set(ctx->phydev,
				channel, ctx->sa.assoc_num, &tx_sak);
	if (ret) {
		phydev_warn(ctx->phydev,
			"%s: fail to secy_tx_sak_set!\n", __func__);
		return ret;
	}

	ret = qca81xx_secy_tx_sa_create(ctx->phydev,
				channel, ctx->sa.assoc_num);
	if (ret) {
		phydev_warn(ctx->phydev,
			"%s: fail to tx_sa_create!\n", __func__);
		return ret;
	}

	ret = qca_update_txsa(ctx->phydev,
			channel, ctx->sa.assoc_num, ctx->sa.tx_sa);
	return ret;
}

static int qca_mdo_upd_txsa(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	int channel = 0, ret = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	ret = qca_update_txsa(ctx->phydev,
			channel, ctx->sa.assoc_num, ctx->sa.tx_sa);
	return ret;
}

static int qca_mdo_del_txsa(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	int channel = 0, ret = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	ret = qca81xx_secy_tx_sa_del(ctx->phydev,
				channel, ctx->sa.assoc_num);
	return ret;
}

static int qca_mdo_add_rxsc(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	u32 rxsc_idx = 0;
	int ret = 0;

	if (hweight32(pcfg->rxsc_idx_bits) >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	rxsc_idx = ffz(pcfg->rxsc_idx_bits);
	if (rxsc_idx >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	ret = qca_create_rxsc(ctx->phydev, rxsc_idx, ctx->secy, ctx->rx_sc);
	if (ret) {
		phydev_warn(ctx->phydev, "%s: fail to add rxsc!\n", __func__);
		return ret;
	}
	pcfg->secy_rxsc[rxsc_idx].hw_sc_idx = rxsc_idx;
	pcfg->secy_rxsc[rxsc_idx].sw_secy = ctx->secy;
	pcfg->secy_rxsc[rxsc_idx].rx_sc = ctx->rx_sc;
	set_bit(rxsc_idx, &pcfg->rxsc_idx_bits);

	return 0;
}

static int qca_mdo_upd_rxsc(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	int channel = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	return qca_update_rxsc(ctx->phydev, channel, ctx->secy);
}

static int qca_mdo_del_rxsc(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_rx_sc_policy_rule_t entry;
	int channel = 0;
	int ret = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&entry, 0, sizeof(entry));
	ret = qca81xx_secy_rx_sc_policy_set(ctx->phydev, channel, &entry);

	clear_bit(channel, &pcfg->rxsc_idx_bits);
	pcfg->secy_rxsc[channel].sw_secy = NULL;
	pcfg->secy_rxsc[channel].rx_sc = NULL;
	pcfg->secy_rxsc[channel].hw_sc_idx = 0;

	return ret;
}

static int qca_mdo_add_rxsa(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	struct secy_sak_t rx_sak;
	int i;
	int channel = 0;
	int ret = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&rx_sak, 0, sizeof(struct secy_sak_t));
	rx_sak.len = ctx->secy->key_len;
	if (ctx->secy->key_len == QCA_GCM_AES_128_SAK_LEN) {
		for (i = 0; i < 16; i++)
			rx_sak.sak[i] = ctx->sa.key[15 - i];

	} else if (ctx->secy->key_len == QCA_GCM_AES_256_SAK_LEN) {
		for (i = 0; i < 16; i++) {
			rx_sak.sak1[i] = ctx->sa.key[15 - i];
			rx_sak.sak[i] = ctx->sa.key[31 - i];
		}
	} else {
		phydev_err(ctx->phydev, "%s: error key_len!\n", __func__);
		return -EOPNOTSUPP;
	}

	ret = qca81xx_secy_rx_sa_create(ctx->phydev,
				channel, ctx->sa.assoc_num);
	if (ret) {
		phydev_warn(ctx->phydev,
			"%s: fail to rx_sa_create!\n", __func__);
		return ret;
	}

	ret = qca81xx_secy_rx_sak_set(ctx->phydev,
				channel, ctx->sa.assoc_num, &rx_sak);
	if (ret) {
		phydev_warn(ctx->phydev,
			"%s: fail to rx_sak_set!\n", __func__);
		return ret;
	}
	ret = qca_update_rxsa(ctx->phydev,
			channel, ctx->sa.assoc_num, ctx->sa.rx_sa);
	return ret;
}

static int qca_mdo_upd_rxsa(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	int channel = 0, ret = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, rx_sc);
	if (channel < 0)
		return -ENOENT;

	ret = qca_update_rxsa(ctx->phydev,
			channel, ctx->sa.assoc_num, ctx->sa.rx_sa);
	return ret;
}

static int qca_mdo_del_rxsa(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	int channel = 0, ret = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, rx_sc);
	if (channel < 0)
		return -ENOENT;

	ret = qca81xx_secy_rx_sa_del(ctx->phydev,
				channel, ctx->sa.assoc_num);
	return ret;
}

static int qca_mdo_get_dev_stats(struct macsec_context *ctx)
{
	struct secy_rx_mib_t rxmib;
	struct secy_tx_mib_t txmib;

	memset(&rxmib, 0, sizeof(struct secy_rx_mib_t));
	memset(&txmib, 0, sizeof(struct secy_tx_mib_t));

	qca81xx_secy_rx_mib_get(ctx->phydev, &rxmib);
	qca81xx_secy_tx_mib_get(ctx->phydev, &txmib);

	ctx->stats.dev_stats->OutPktsUntagged = txmib.untagged_pkts;
	ctx->stats.dev_stats->InPktsUntagged = rxmib.untagged_pkts;
	ctx->stats.dev_stats->OutPktsTooLong = txmib.too_long;
	ctx->stats.dev_stats->InPktsNoTag = rxmib.notag_pkts;
	ctx->stats.dev_stats->InPktsBadTag = rxmib.bad_tag_pkts;
	ctx->stats.dev_stats->InPktsUnknownSCI = rxmib.unknown_sci_pkts;
	ctx->stats.dev_stats->InPktsNoSCI = rxmib.no_sci_pkts;
	ctx->stats.dev_stats->InPktsOverrun = rxmib.overrun_packets;

	return 0;
}

static int qca_mdo_get_tx_sc_stats(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_tx_sc_mib_t txscmib;
	struct secy_tx_sa_mib_t txsamib;
	int channel = 0;
	u32 i;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&txscmib, 0, sizeof(struct secy_tx_sc_mib_t));

	qca81xx_secy_tx_sc_mib_get(ctx->phydev, channel, &txscmib);

	ctx->stats.tx_sc_stats->OutOctetsProtected = txscmib.protected_octets;
	ctx->stats.tx_sc_stats->OutOctetsEncrypted = txscmib.encrypted_octets;

	for (i = 0; i < 2; i++) {
		memset(&txsamib, 0, sizeof(struct secy_tx_sa_mib_t));
		qca81xx_secy_tx_sa_mib_get(ctx->phydev,
					channel, i, &txsamib);

		ctx->stats.tx_sc_stats->OutPktsProtected +=
						txsamib.protected_pkts;
		ctx->stats.tx_sc_stats->OutPktsEncrypted +=
						txsamib.encrypted_pkts;
	}
	return 0;
}

static int qca_mdo_get_tx_sa_stats(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_tx_sa_mib_t txsamib;
	int channel = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&txsamib, 0, sizeof(struct secy_tx_sa_mib_t));

	qca81xx_secy_tx_sa_mib_get(ctx->phydev,
				channel, ctx->sa.assoc_num, &txsamib);

	ctx->stats.tx_sa_stats->OutPktsProtected = txsamib.protected_pkts;
	ctx->stats.tx_sa_stats->OutPktsEncrypted = txsamib.encrypted_pkts;

	return 0;
}

static int qca_mdo_get_rx_sc_stats(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_rx_sc_mib_t secy_rx_sc_mib;
	struct secy_rx_sa_mib_t secy_rx_sa_mib;
	int channel = 0;
	u32 i;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&secy_rx_sc_mib, 0, sizeof(secy_rx_sc_mib));

	qca81xx_secy_rx_sc_mib_get(ctx->phydev, channel, &secy_rx_sc_mib);

	ctx->stats.rx_sc_stats->InOctetsValidated =
					secy_rx_sc_mib.validated_octets;
	ctx->stats.rx_sc_stats->InOctetsDecrypted =
					secy_rx_sc_mib.decrypted_octets;
	ctx->stats.rx_sc_stats->InPktsUnchecked =
					secy_rx_sc_mib.unchecked_pkts;
	ctx->stats.rx_sc_stats->InPktsDelayed =
					secy_rx_sc_mib.delayed_pkts;
	ctx->stats.rx_sc_stats->InPktsLate = secy_rx_sc_mib.late_pkts;

	for (i = 0; i < 2; i++) {
		memset(&secy_rx_sa_mib, 0, sizeof(secy_rx_sa_mib));
		qca81xx_secy_rx_sa_mib_get(ctx->phydev,
					channel, i, &secy_rx_sa_mib);

		ctx->stats.rx_sc_stats->InPktsOK +=
					secy_rx_sa_mib.ok_pkts;
		ctx->stats.rx_sc_stats->InPktsInvalid +=
					secy_rx_sa_mib.invalid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotValid +=
					secy_rx_sa_mib.not_valid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotUsingSA +=
					secy_rx_sa_mib.not_using_sa;
		ctx->stats.rx_sc_stats->InPktsUnusedSA +=
					secy_rx_sa_mib.unused_sa;
	}
	return 0;
}

static int qca_mdo_get_rx_sa_stats(struct macsec_context *ctx)
{
	struct qca81xx_private *priv = ctx->phydev->priv;
	struct qca_macsec_cfg_t *pcfg = &priv->macsec_cfg;
	struct secy_rx_sa_mib_t rxsamib;
	int channel = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&rxsamib, 0, sizeof(struct secy_rx_sa_mib_t));

	qca81xx_secy_rx_sa_mib_get(ctx->phydev,
				channel, ctx->sa.assoc_num, &rxsamib);

	ctx->stats.rx_sa_stats->InPktsOK = rxsamib.ok_pkts;
	ctx->stats.rx_sa_stats->InPktsInvalid = rxsamib.invalid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotValid = rxsamib.not_valid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotUsingSA = rxsamib.not_using_sa;
	ctx->stats.rx_sa_stats->InPktsUnusedSA = rxsamib.unused_sa;

	return 0;
}

static const struct macsec_ops qca_macsec_mdo_ops = {
	.mdo_dev_open = qca_mdo_dev_open,
	.mdo_dev_stop = qca_mdo_dev_stop,
	.mdo_add_secy = qca_mdo_add_secy,
	.mdo_upd_secy = qca_mdo_upd_secy,
	.mdo_del_secy = qca_mdo_del_secy,
	.mdo_add_rxsc = qca_mdo_add_rxsc,
	.mdo_upd_rxsc = qca_mdo_upd_rxsc,
	.mdo_del_rxsc = qca_mdo_del_rxsc,
	.mdo_add_rxsa = qca_mdo_add_rxsa,
	.mdo_upd_rxsa = qca_mdo_upd_rxsa,
	.mdo_del_rxsa = qca_mdo_del_rxsa,
	.mdo_add_txsa = qca_mdo_add_txsa,
	.mdo_upd_txsa = qca_mdo_upd_txsa,
	.mdo_del_txsa = qca_mdo_del_txsa,
	.mdo_get_dev_stats = qca_mdo_get_dev_stats,
	.mdo_get_tx_sc_stats = qca_mdo_get_tx_sc_stats,
	.mdo_get_tx_sa_stats = qca_mdo_get_tx_sa_stats,
	.mdo_get_rx_sc_stats = qca_mdo_get_rx_sc_stats,
	.mdo_get_rx_sa_stats = qca_mdo_get_rx_sa_stats,
};

int qca81xx_macsec_init(struct phy_device *phydev)
{
	struct secy_rx_mib_t rxmib = {0};
	struct secy_tx_mib_t txmib = {0};
	struct secy_tx_sc_mib_t txscmib = {0};
	struct secy_rx_sc_mib_t rxscmib = {0};
	struct secy_tx_sa_mib_t txsamib = {0};
	struct secy_rx_sa_mib_t rxsamib = {0};
	struct net_device *ndev = NULL;
	int ret = 0;
	u32 sc, sa;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_SOFTWARE_EN_CTRL,
			SYS_SECY_SOFTWARE_EN_M,
			FIELD_PREP(SYS_SECY_SOFTWARE_EN_M, SOFTWARE_EN));
	if (ret) {
		phydev_err(phydev, "%s: fail to enable macsec!\n", __func__);
		return ret;
	}

	/* recommended configuration for IPG extension */
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xE003, 0xb);
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xF003, 0x3);
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xE006, 0x3);
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xF006, 0x3);

	phydev->macsec_ops = &qca_macsec_mdo_ops;
	ndev = phydev->attached_dev;
	if (ndev && ndev->features)
		ndev->features |= NETIF_F_HW_MACSEC;

	/* read clear MIB */
	qca81xx_secy_rx_mib_get(phydev, &rxmib);
	qca81xx_secy_tx_mib_get(phydev, &txmib);

	for (sc = 0; sc < QCA_SECY_SC_MAX_NUM; sc++) {
		/* read clear MIB */
		qca81xx_secy_tx_sc_mib_get(phydev, sc, &txscmib);
		qca81xx_secy_rx_sc_mib_get(phydev, sc, &rxscmib);
		/* Policy bind table init */
		phy_write_mmd(phydev, MDIO_MMD_PCS,
				MACSEC_SC_BIND_TXMASK(sc), 0);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
				MACSEC_SC_BIND_RXMASK(sc), 0);
		for (sa = 0; sa < 2; sa++) {
			/* read clear MIB */
			qca81xx_secy_tx_sa_mib_get(phydev, sc, sa, &txsamib);
			qca81xx_secy_rx_sa_mib_get(phydev, sc, sa, &rxsamib);
			/* init next pn */
			qca81xx_secy_tx_sa_npn_set(phydev, sc, sa, 1);
			qca81xx_secy_rx_sa_npn_set(phydev, sc, sa, 1);
		}
	}
	return 0;
}


