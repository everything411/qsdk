/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/bitops.h>
#include <linux/string.h>
#if IS_ENABLED(CONFIG_NET_DSA)
#include <net/dsa.h>
#endif
#include "qca_macsec_lib.h"

/* ===== Shadow SKU structures (read-only, local to this file) ===== */
/*
 * These structures are used to read SKU info from chip private data
 * without depending on chip-specific headers. They assume the SKU info
 * is the first member of the chip private structure: for the QCA81xx
 * family that is phydev->priv, for QCE1204 it is the package-shared
 * priv (phydev->shared->priv). Read-only access is performed.
 */
struct qca_sku_info {
	const char *name;
	bool ptp;
	bool macsec;
};

struct qca_common_private {
	struct qca_sku_info sku; /* MUST match first member of chip priv */
};

#define QCA8081_PHY	0x004DD101
#define QCA8084_PHY	0x004DD180
#define QCA81xx_PHY	0x004DD1C0
#define QCE1204_PHY	0x004DD190
#define IPQ52XX_PHY	0x004DD120


/* ===================== Chip Info Lookup ===================== */

/* Context structure */
struct qca_macsec_ctx {
	struct macsec_ops mdo_ops;
	struct qca_macsec_cfg_t config;
	struct phy_device *phydev;
};

/* Chip capability flags */
#define QCA_MACSEC_CAP_MIXED_MIB    BIT(0)

/* Chip info structure (lightweight, on stack) */
struct qca_macsec_chip_info {
	u32 capabilities;
	int (*init)(struct phy_device *phydev);
	const char *name;
};

/* Get chip info by PHY ID - returns all chip info in one call */
static bool qca_macsec_get_chip_info(u32 phy_id,
				     struct qca_macsec_chip_info *info)
{
	switch (phy_id) {
	case QCA8081_PHY:
		info->capabilities = QCA_MACSEC_CAP_MIXED_MIB;
		info->init = qca808x_macsec_config_init;
		info->name = "QCA8081";
		return true;

	case QCA8084_PHY:
		info->capabilities = QCA_MACSEC_CAP_MIXED_MIB;
		info->init = qca808x_macsec_config_init;
		info->name = "QCA8084";
		return true;

	case QCA81xx_PHY:
		info->capabilities = 0;
		info->init = qca81xx_macsec_config_init;
		info->name = "QCA81x2";
		return true;

	case QCE1204_PHY:
		info->capabilities = 0;
		info->init = qce1204_macsec_config_init;
		info->name = "QCE1204";
		return true;

	case IPQ52XX_PHY:
		info->capabilities = 0;
		info->init = qce1204_macsec_config_init;
		info->name = "IPQ52xx";
		return true;

	default:
		return false;  /* Not supported */
	}
}

/* Get PHY ID from phy_device (C45 or C22) */
static inline u32 qca_macsec_get_phy_id(struct phy_device *phydev)
{
	if (phydev->is_c45)
		return phydev->c45_ids.device_ids[
				__ffs(phydev->c45_ids.mmds_present)];
	else
		return phydev->phy_id;
}

/* Common MIB address calculation function (exported) */
static u32 qca_macsec_mib_addr(u32 sc_index, u32 sa_index, u32 addr)
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

/* Common MIB read32 function (special 32-bit counters) */
static int qca_macsec_secy_mib_read32(struct phy_device *phydev,
			       u32 reg_addr, u32 *out32)
{
	int val = 0;

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, reg_addr + 1);
	if (unlikely(val < 0))
		return val;
	*out32 = (val & 0xffff) << 16;
	val = phy_read_mmd(phydev, MDIO_MMD_PCS, reg_addr);
	if (unlikely(val < 0))
		return val;
	*out32 |= val & 0xffff;

	return 0;
}

/* Common MIB read function (default 64-bit via high+low) */
static int qca_macsec_secy_mib_read(struct phy_device *phydev,
			     u32 addr_high, u32 addr_low, u64 *out64)
{
	int ret;
	u32 high = 0, low = 0;

	ret = qca_macsec_secy_mib_read32(phydev, addr_high, &high);
	if (ret)
		return ret;
	ret = qca_macsec_secy_mib_read32(phydev, addr_low, &low);
	if (ret)
		return ret;

	*out64 = ((u64)high << 32) | low;
	return 0;
}

/* Common RX SA MIB get function */
static int qca_macsec_secy_rx_sa_mib_get(struct phy_device *phydev,
				  u32 sc_index, u32 an,
				  struct secy_rx_sa_mib_t *mib)
{
	u32 sa_index;
	u32 addr_hi, addr_lo;
	int ret;

	if (sc_index >= QCA_SECY_SC_MAX_NUM ||
	    an >= SECY_AN_IDX_MAX_NUM || !mib)
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_UNUSED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_UNUSED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->unused_sa);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_NOUSING_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_NOUSING_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->not_using_sa);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_NOTVALID_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_NOTVALID_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->not_valid_pkts);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_INVALID_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_INVALID_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->invalid_pkts);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_OK_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SA_OK_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->ok_pkts);
	return ret;
}

/* Common TX SA MIB get function */
static int qca_macsec_secy_tx_sa_mib_get(struct phy_device *phydev,
				  u32 sc_index, u32 an,
				  struct secy_tx_sa_mib_t *mib)
{
	u32 sa_index;
	u32 addr_hi, addr_lo;
	int ret;

	if (sc_index >= QCA_SECY_SC_MAX_NUM ||
	    an >= SECY_AN_IDX_MAX_NUM || !mib)
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SA_PROTECTED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SA_PROTECTED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->protected_pkts);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SA_ENCRYPTED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SA_ENCRYPTED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->encrypted_pkts);
	return ret;
}

/* Common RX SC MIB get function */
static int qca_macsec_secy_rx_sc_mib_get(struct phy_device *phydev,
				  u32 sc_index,
				  struct secy_rx_sc_mib_t *mib)
{
	u32 sa_index = 0;
	u32 addr_hi, addr_lo;
	int ret;

	if (sc_index >= QCA_SECY_SC_MAX_NUM || !mib)
		return -EINVAL;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_LATE_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_LATE_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->late_pkts);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_DELAYED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_DELAYED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->delayed_pkts);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_UNCHECKED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_UNCHECKED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->unchecked_pkts);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_VALIDATED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_VALIDATED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->validated_octets);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_DECRYPTED_PKTS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      RX_SC_DECRYPTED_PKTS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->decrypted_octets);
	return ret;
}

/* Common TX SC MIB get function */
static int qca_macsec_secy_tx_sc_mib_get(struct phy_device *phydev,
				  u32 sc_index,
				  struct secy_tx_sc_mib_t *mib)
{
	u32 sa_index = 0;
	u32 addr_hi, addr_lo;
	int ret;

	if (sc_index >= QCA_SECY_SC_MAX_NUM || !mib)
		return -EINVAL;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SC_PROTECTED_OCTETS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SC_PROTECTED_OCTETS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->protected_octets);
	if (ret)
		return ret;

	addr_hi = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SC_ENCRYPTED_OCTETS_HIGH_BASE);
	addr_lo = qca_macsec_mib_addr(sc_index, sa_index,
				      TX_SC_ENCRYPTED_OCTETS_BASE);
	ret = qca_macsec_secy_mib_read(phydev, addr_hi, addr_lo,
				       &mib->encrypted_octets);
	return ret;
}

/* Common RX MIB get function */
static int qca_macsec_secy_rx_mib_get(struct phy_device *phydev,
			       struct secy_rx_mib_t *mib)
{
	u32 hi, lo;
	int ret;

	if (!mib)
		return -EINVAL;

	hi = qca_macsec_mib_addr(0, 0, RX_UNTAGGED_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, RX_UNTAGGED_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->untagged_pkts);
	if (ret)
		return ret;

	hi = qca_macsec_mib_addr(0, 0, RX_NO_TAG_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, RX_NO_TAG_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->notag_pkts);
	if (ret)
		return ret;

	hi = qca_macsec_mib_addr(0, 0, RX_BAD_TAG_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, RX_BAD_TAG_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->bad_tag_pkts);
	if (ret)
		return ret;

	hi = qca_macsec_mib_addr(0, 0, RX_UNKNOWN_SCI_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, RX_UNKNOWN_SCI_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->unknown_sci_pkts);
	if (ret)
		return ret;

	hi = qca_macsec_mib_addr(0, 0, RX_NO_SCI_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, RX_NO_SCI_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->no_sci_pkts);
	if (ret)
		return ret;

	hi = qca_macsec_mib_addr(0, 0, RX_OVERRUN_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, RX_OVERRUN_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->overrun_packets);
	return ret;
}

/* Common TX MIB get function */
static int qca_macsec_secy_tx_mib_get(struct phy_device *phydev,
			       struct secy_tx_mib_t *mib)
{
	u32 hi, lo;
	int ret;

	if (!mib)
		return -EINVAL;

	hi = qca_macsec_mib_addr(0, 0, TX_UNTAGGED_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, TX_UNTAGGED_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->untagged_pkts);
	if (ret)
		return ret;

	hi = qca_macsec_mib_addr(0, 0, TX_TOO_LONG_PKTS_HIGH);
	lo = qca_macsec_mib_addr(0, 0, TX_TOO_LONG_PKTS);
	ret = qca_macsec_secy_mib_read(phydev, hi, lo, &mib->too_long);
	return ret;
}

/* ===== 808x/8084 mixed-width MIB helpers ===== */

/* Device-level RX statistics (32-bit) */
static int qca808x_macsec_secy_rx_mib_get(struct phy_device *phydev,
					  struct secy_rx_mib_t *mib)
{
	u32 val;
	int ret;

	if (!mib)
		return -EINVAL;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, RX_UNTAGGED_PKTS), &val);
	if (ret)
		return ret;
	mib->untagged_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, RX_NO_TAG_PKTS), &val);
	if (ret)
		return ret;
	mib->notag_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, RX_BAD_TAG_PKTS), &val);
	if (ret)
		return ret;
	mib->bad_tag_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, RX_UNKNOWN_SCI_PKTS), &val);
	if (ret)
		return ret;
	mib->unknown_sci_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, RX_NO_SCI_PKTS), &val);
	if (ret)
		return ret;
	mib->no_sci_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, RX_OVERRUN_PKTS), &val);
	if (ret)
		return ret;
	mib->overrun_packets = (u64)val;

	return 0;
}

/* Device-level TX statistics (32-bit) */
static int qca808x_macsec_secy_tx_mib_get(struct phy_device *phydev,
					  struct secy_tx_mib_t *mib)
{
	u32 val;
	int ret;

	if (!mib)
		return -EINVAL;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, TX_UNTAGGED_PKTS), &val);
	if (ret)
		return ret;
	mib->untagged_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
			qca_macsec_mib_addr(0, 0, TX_TOO_LONG_PKTS), &val);
	if (ret)
		return ret;
	mib->too_long = (u64)val;

	return 0;
}

/* RX SC statistics (32-bit) */
static int qca808x_macsec_secy_rx_sc_mib_get(struct phy_device *phydev,
					     u32 sc_index,
					     struct secy_rx_sc_mib_t *mib)
{
	u32 val;
	int ret;

	if (!mib || sc_index >= QCA_SECY_SC_MAX_NUM)
		return -EINVAL;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, RX_SC_LATE_PKTS_BASE), &val);
	if (ret)
		return ret;
	mib->late_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, RX_SC_DELAYED_PKTS_BASE), &val);
	if (ret)
		return ret;
	mib->delayed_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, RX_SC_UNCHECKED_PKTS_BASE), &val);
	if (ret)
		return ret;
	mib->unchecked_pkts = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, RX_SC_VALIDATED_PKTS_BASE), &val);
	if (ret)
		return ret;
	mib->validated_octets = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, RX_SC_DECRYPTED_PKTS_BASE), &val);
	if (ret)
		return ret;
	mib->decrypted_octets = (u64)val;

	return 0;
}

/* TX SC statistics (32-bit) */
static int qca808x_macsec_secy_tx_sc_mib_get(struct phy_device *phydev,
					     u32 sc_index,
					     struct secy_tx_sc_mib_t *mib)
{
	u32 val;
	int ret;

	if (!mib || sc_index >= QCA_SECY_SC_MAX_NUM)
		return -EINVAL;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, TX_SC_PROTECTED_OCTETS_BASE), &val);
	if (ret)
		return ret;
	mib->protected_octets = (u64)val;

	ret = qca_macsec_secy_mib_read32(phydev,
		qca_macsec_mib_addr(sc_index, 0, TX_SC_ENCRYPTED_OCTETS_BASE), &val);
	if (ret)
		return ret;
	mib->encrypted_octets = (u64)val;

	return 0;
}

/* Common TX SAK set function */
static int qca_macsec_secy_tx_sak_set(struct phy_device *phydev, u32 sc_index,
				      u32 an, struct secy_sak_t *key)
{
	u16 val = 0;
	u16 reg;
	int i;
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
		/* Write SAK and EXTENDED_SAK in a single loop (808x style) */
		for (i = 0; i < 8; i++) {
			val = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
			reg = MACSEC_TX_SAK_KEY0(channel) + i;
			phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);

			val = (key->sak1[i * 2 + 1] << 8) | key->sak1[i * 2];
			reg = MACSEC_TX_EXTENDED_SAK_KEY0(channel) + i;
			phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
		}
	} else {
		for (i = 0; i < 8; i++) {
			val = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
			reg = MACSEC_TX_SAK_KEY0(channel) + i;
			phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
		}
	}
	return 0;
}

/* Common RX SAK set function */
static int qca_macsec_secy_rx_sak_set(struct phy_device *phydev, u32 sc_index,
				      u32 an, struct secy_sak_t *key)
{
	u16 val = 0;
	u16 reg;
	int i;
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
		/* Write SAK and EXTENDED_SAK in a single loop (808x style) */
		for (i = 0; i < 8; i++) {
			val = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
			reg = MACSEC_RX_SAK_KEY0(channel) + i;
			phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);

			val = (key->sak1[i * 2 + 1] << 8) | key->sak1[i * 2];
			reg = MACSEC_RX_EXTENDED_SAK_KEY0(channel) + i;
			phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
		}
	} else {
		for (i = 0; i < 8; i++) {
			val = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
			reg = MACSEC_RX_SAK_KEY0(channel) + i;
			phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
		}
	}
	return 0;
}

/* Common TX SA NPN set function */
static int qca_macsec_secy_tx_sa_npn_set(struct phy_device *phydev, u32 sc_index,
					 u32 an, u64 next_pn)
{
	u32 npn = 0;
	u16 val = 0;
	u16 channel = 0;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	channel = (sc_index * 2 + SECY_AN_TO_SA_MAPPING(an));

	/* Always write the XPN high 32 bits. When XPN is disabled the
	 * hardware ignores these registers, so this is harmless and also
	 * ensures they are cleared during initialization.
	 */
	npn = (u32)(next_pn >> 32);
	val = (u16)(npn & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_TX_XPN(channel), val);
	val = (u16)((npn >> 16) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_TX_XPN(channel) + 1, val);

	npn = (u32)(next_pn & 0xffffffff);
	val = (u16)(npn & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_TX_NPN(channel), val);
	val = (u16)((npn >> 16) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_TX_NPN(channel) + 1, val);
	return 0;
}

/* Common RX SA NPN set function */
static int qca_macsec_secy_rx_sa_npn_set(struct phy_device *phydev, u32 sc_index,
					 u32 an, u64 next_pn)
{
	u32 npn = 0;
	u16 val = 0;
	u16 channel = 0;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
		(an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	channel = (sc_index * 2 + SECY_AN_TO_SA_MAPPING(an));

	/* Always write the XPN high 32 bits. When XPN is disabled the
	 * hardware ignores these registers, so this is harmless and also
	 * ensures they are cleared during initialization.
	 */
	npn = (u32)(next_pn >> 32);
	val = (u16)(npn & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_RX_XPN(channel), val);
	val = (u16)((npn >> 16) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_RX_XPN(channel) + 1, val);

	npn = (u32)(next_pn & 0xffffffff);
	val = (u16)(npn & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_RX_NPN(channel), val);
	val = (u16)((npn >> 16) & 0xffff);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
			MACSEC_RX_NPN(channel) + 1, val);
	return 0;
}

/* ===== SSCI and KI setters (common) ===== */

static int qca_macsec_secy_tx_sc_ssci_set(struct phy_device *phydev,
					  u32 sc_index, u32 ssci)
{
	u16 val = 0;
	u16 reg = 0;
	u32 i;

	if (sc_index >= QCA_SECY_SC_MAX_NUM)
		return -EINVAL;

	for (i = 0; i < 2; i++) {
		val = (u16)((ssci >> (i * 16)) & 0xffff);
		reg = MACSEC_SC_TX_SSCI0(sc_index) + i;
		phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
	}

	return 0;
}

static int qca_macsec_secy_rx_sc_ssci_set(struct phy_device *phydev,
					  u32 sc_index, u32 ssci)
{
	u16 val = 0;
	u16 reg = 0;
	u32 i;

	if (sc_index >= QCA_SECY_SC_MAX_NUM)
		return -EINVAL;

	for (i = 0; i < 2; i++) {
		val = (u16)((ssci >> (i * 16)) & 0xffff);
		reg = MACSEC_SC_RX_SSCI0(sc_index) + i;
		phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
	}

	return 0;
}

static int qca_macsec_secy_tx_sa_ki_set(struct phy_device *phydev,
					u32 sc_index, u32 an,
					const struct secy_sa_ki_t *key_identifier)
{
	u16 val = 0;
	u16 reg = 0;
	int i;
	u32 channel, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
	    (an >= SECY_AN_IDX_MAX_NUM) || !key_identifier)
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	channel = sc_index * 2 + sa_index;

	for (i = 0; i < 8; i++) {
		val = ((u16)key_identifier->ki[i * 2 + 1] << 8) |
		       key_identifier->ki[i * 2];
		reg = MACSEC_TX_KI_KEY0(channel) + i;
		phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
	}

	return 0;
}

static int qca_macsec_secy_rx_sa_ki_set(struct phy_device *phydev,
					u32 sc_index, u32 an,
					const struct secy_sa_ki_t *key_identifier)
{
	u16 val = 0;
	u16 reg = 0;
	int i;
	u32 channel, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) ||
	    (an >= SECY_AN_IDX_MAX_NUM) || !key_identifier)
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	channel = sc_index * 2 + sa_index;

	for (i = 0; i < 8; i++) {
		val = ((u16)key_identifier->ki[i * 2 + 1] << 8) |
		       key_identifier->ki[i * 2];
		reg = MACSEC_RX_KI_KEY0(channel) + i;
		phy_write_mmd(phydev, MDIO_MMD_PCS, reg, val);
	}

	return 0;
}

static int qca_macsec_secy_tx_sa_create(struct phy_device *phydev,
					u32 sc_index, u32 an)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index / 4;
	chan_shift = 2 * ((sc_index % 4) * 2 + sa_index);
	reg += MACSEC_TX_AN_BASE;
	val = (((u16)an & AN_MASK) << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
			      (AN_MASK << chan_shift), val);
}

static int qca_macsec_secy_tx_sa_del(struct phy_device *phydev,
				     u32 sc_index, u32 an)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index / 4;
	chan_shift = 2 * ((sc_index % 4) * 2 + sa_index);
	reg += MACSEC_TX_AN_BASE;
	val = (0x0 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
			      (AN_MASK << chan_shift), val);
}

static int qca_macsec_secy_tx_sa_en_set(struct phy_device *phydev,
					u32 sc_index, u32 an, bool enable)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index / 8;
	chan_shift = ((sc_index % 8) * 2 + sa_index);

	reg += MACSEC_TX_SA_CONTROL;

	if (enable)
		val = (1 << chan_shift);
	else
		val &= ~(1 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
			      (1 << chan_shift), val);
}

static int qca_macsec_secy_rx_sa_en_set(struct phy_device *phydev,
					u32 sc_index, u32 an, bool enable)
{
	u16 val = 0;
	u16 reg, chan_shift;
	u32 sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index / 8;
	chan_shift = ((sc_index % 8) * 2 + sa_index);

	reg += MACSEC_RX_SA_CONTROL;

	if (enable)
		val = (1 << chan_shift);
	else
		val &= ~(1 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
			      (1 << chan_shift), val);
}

static int qca_macsec_secy_rx_sa_create(struct phy_device *phydev,
					u32 sc_index, u32 an)
{
	u16 val = 0, chan_shift;
	u32 reg, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	/* init next PN to 1 */
	qca_macsec_secy_rx_sa_npn_set(phydev, sc_index, an, 1);

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index / 4;
	chan_shift = 2 * ((sc_index % 4) * 2 + sa_index);
	reg += MACSEC_RX_AN_BASE;
	val = (((u16)an & AN_MASK) << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
			      (AN_MASK << chan_shift), val);
}

static int qca_macsec_secy_rx_sa_del(struct phy_device *phydev,
				     u32 sc_index, u32 an)
{
	u16 val = 0, chan_shift;
	u32 reg, sa_index;

	if ((sc_index >= QCA_SECY_SC_MAX_NUM) || (an >= SECY_AN_IDX_MAX_NUM))
		return -EINVAL;

	sa_index = SECY_AN_TO_SA_MAPPING(an);
	reg = sc_index / 4;
	chan_shift = 2 * ((sc_index % 4) * 2 + sa_index);
	reg += MACSEC_RX_AN_BASE;
	val = (0x0 << chan_shift);

	return phy_modify_mmd(phydev, MDIO_MMD_PCS, reg,
			      (AN_MASK << chan_shift), val);
}

static int qca_macsec_secy_tx_sc_policy_set(struct phy_device *phydev,
					    u32 rule_index,
					    const struct secy_tx_sc_policy_rule_t *rule)
{
	u16 val = 0, msk = 0;
	u32 shifted_value = 0, i = 0;

	if ((rule_index >= QCA_SECY_SC_MAX_NUM) || (rule == NULL))
		return -EINVAL;

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
		val |= rule->bc_flag ? SC_BIND_TX_IFBC : 0;
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

static int qca_macsec_secy_rx_sc_policy_set(struct phy_device *phydev,
					    u32 rule_index,
					    const struct secy_rx_sc_policy_rule_t *rule)
{
	u16 val = 0, msk = 0;
	u32 shifted_value = 0, i = 0;

	if ((rule_index >= QCA_SECY_SC_MAX_NUM) || (rule == NULL))
		return -EINVAL;

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
		val |= rule->bc_flag ? SC_BIND_RX_IFBC : 0;
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
	if (msk)
		msk |= SC_BIND_MASK_SCI;

	return phy_write_mmd(phydev, MDIO_MMD_PCS,
			     MACSEC_SC_BIND_RXMASK(rule_index), msk);
}

static int qca_macsec_secy_cipher_suite_set(struct phy_device *phydev,
					    struct macsec_secy *secy)
{
	u16 val = 0;
	int ret = 0;

	if (secy->key_len == QCA_GCM_AES_256_SAK_LEN)
		val |= SYS_AES256_EN;

	if (secy->xpn)
		val |= SYS_XPN_EN;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, MACSEC_SYS_FRAME_CTRL,
			     SYS_AES256_EN | SYS_XPN_EN, val);
	return ret;
}

static int qca_macsec_secy_ctrl_set(struct phy_device *phydev, bool enable)
{
	int ret = 0;
	u16 val = enable ? SYS_MACSEC_EN : SYS_BYPASS;
	u32 phyid = 0;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			     MACSEC_SYS_PACKET_CTRL,
			     SYS_SECY_LPBK_M,
			     FIELD_PREP(SYS_SECY_LPBK_M, val));
	if (ret)
		return ret;

	phyid = qca_macsec_get_phy_id(phydev);
	if ((phyid & phydev->drv->phy_id_mask) == QCA8081_PHY) {
		val = enable ?
		      (MACSEC_SHADOW_DUPLEX_EN | MACSEC_SHADOW_LEGACY_DUPLEX_EN) : 0;
		ret = phy_modify_mmd(phydev, MDIO_MMD_AN,
				     MACSEC_SHADOW_REGISTER,
				     MACSEC_SHADOW_DUPLEX_EN |
				     MACSEC_SHADOW_LEGACY_DUPLEX_EN,
				     val);
		if (ret)
			return ret;
	} else if ((phyid & phydev->drv->phy_id_mask) == QCA81xx_PHY) {
		/* WAR: sequence order forward must be off in MACsec mode */
		val = enable ? 0 : I_SEQ_FWD_EN;
		ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
				     MACSEC_INGRESS_CTRL0,
				     I_SEQ_FWD_EN, val);
		if (ret)
			return ret;
	}
	val = enable ? SYS_PORT_EN : 0;
	return phy_modify_mmd(phydev, MDIO_MMD_PCS,
			      MACSEC_SYS_PORT_CTRL, SYS_PORT_EN, val);
}

/* ===== Common init===== */
/* Configure forward AZ pattern and local LPI based on enable flag */
static int qca_macsec_forward_az_en_set(struct phy_device *phydev, bool enable)
{
	int ret;
	u16 fwd_msk;
	u16 fwd_val;
	u16 loc_msk;
	u16 loc_val;

	/* Forward AZ pattern control */
	fwd_msk = LPI_RCOVER_EN | E_FORWARD_PATTERN_EN | I_FORWARD_PATTERN_EN;
	if (enable)
		fwd_val = E_FORWARD_PATTERN_EN | I_FORWARD_PATTERN_EN;
	else
		fwd_val = LPI_RCOVER_EN;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			     MACSEC_FORWARD_AZ_PATTERN_EN_CTRL,
			     fwd_msk, fwd_val);
	if (ret)
		return ret;

	/* Local generated LPI control */
	loc_msk = E_MAC_LPI_EN | I_MAC_LPI_EN;
	if (enable)
		loc_val = 0;
	else
		loc_val = E_MAC_LPI_EN | I_MAC_LPI_EN;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			     MACSEC_LOCAL_AZ_PATTERN_EN_CTRL,
			     loc_msk, loc_val);
	if (ret)
		return ret;

	return 0;
}

static int qca_macsec_sw_set(struct phy_device *phydev, bool enable)
{
	u16 val = enable ? SOFTWARE_EN : SOFTWARE_BYPASS;

	return phy_modify_mmd(phydev, MDIO_MMD_PCS,
			      MACSEC_SOFTWARE_EN_CTRL,
			      SYS_SECY_SOFTWARE_EN_M,
			      FIELD_PREP(SYS_SECY_SOFTWARE_EN_M, val));
}

/* Read-clear all MIBs using 64-bit getters */
static int qca_macsec_mib_clear(struct phy_device *phydev)
{
	struct secy_rx_mib_t rxm = { 0 };
	struct secy_tx_mib_t txm = { 0 };
	struct secy_tx_sc_mib_t txscm = { 0 };
	struct secy_rx_sc_mib_t rxscm = { 0 };
	struct secy_tx_sa_mib_t txsam = { 0 };
	struct secy_rx_sa_mib_t rxsam = { 0 };
	u32 sc, sa;
	int ret;

	/* Device-level read-clear */
	ret = qca_macsec_secy_rx_mib_get(phydev, &rxm);
	if (ret)
		return ret;
	ret = qca_macsec_secy_tx_mib_get(phydev, &txm);
	if (ret)
		return ret;

	/* SC/SA-level read-clear */
	for (sc = 0; sc < QCA_SECY_SC_MAX_NUM; sc++) {
		ret = qca_macsec_secy_tx_sc_mib_get(phydev, sc, &txscm);
		if (ret)
			return ret;
		ret = qca_macsec_secy_rx_sc_mib_get(phydev, sc, &rxscm);
		if (ret)
			return ret;

		for (sa = 0; sa < 2; sa++) {
			ret = qca_macsec_secy_tx_sa_mib_get(phydev, sc, sa, &txsam);
			if (ret)
				return ret;
			ret = qca_macsec_secy_rx_sa_mib_get(phydev, sc, sa, &rxsam);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/* Clear all policy bind masks (TX/RX for all SCs) */
static int qca_macsec_policy_bind_clear(struct phy_device *phydev)
{
	u32 sc;

	for (sc = 0; sc < QCA_SECY_SC_MAX_NUM; sc++) {
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			      MACSEC_SC_BIND_TXMASK(sc), 0);
		phy_write_mmd(phydev, MDIO_MMD_PCS,
			      MACSEC_SC_BIND_RXMASK(sc), 0);
	}
	return 0;
}

/* Initialize all SA PN values (TX/RX for all SCs) */
static int qca_macsec_init_all_pn(struct phy_device *phydev, u64 start_pn)
{
	u32 sc, sa;
	int ret;

	for (sc = 0; sc < QCA_SECY_SC_MAX_NUM; sc++) {
		for (sa = 0; sa < 2; sa++) {
			ret = qca_macsec_secy_tx_sa_npn_set(phydev,
							sc, sa, start_pn);
			if (ret)
				return ret;
			ret = qca_macsec_secy_rx_sa_npn_set(phydev,
							sc, sa, start_pn);
			if (ret)
				return ret;
		}
	}
	return 0;
}

/* ===================== Common MDO Helpers ===================== */

/* SC index lookup helpers (exported for chip-specific use) */
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

static u8 qca_macsec_tx_tci_convert(struct macsec_secy *secy)
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

static int qca_macsec_update_txsc(struct phy_device *phydev,
				  const u32 channel, struct macsec_secy *secy)
{
	int ret = 0;
	u16 val = 0;
	u8 tci = 0;

	tci = qca_macsec_tx_tci_convert(secy);

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

static int qca_macsec_create_txsc(struct phy_device *phydev,
				  const u32 channel, struct macsec_secy *secy)
{
	int ret = 0;
	struct secy_tx_sc_policy_rule_t entry;

	memset(&entry, 0, sizeof(entry));

	entry.rule_valid = true;
	entry.action.tx_sc_index = channel;
	entry.action.tx_tci = qca_macsec_tx_tci_convert(secy);
	entry.action.tx_sci = swab64((__force u64)secy->sci);

	ret = qca_macsec_secy_tx_sc_policy_set(phydev, channel, &entry);
	if (ret) {
		phydev_warn(phydev, "%s: fail to create tx_sc policy %d!\n",
			    __func__, channel);
		return ret;
	}

	ret = qca_macsec_update_txsc(phydev, channel, secy);
	return ret;
}

static int qca_macsec_update_rxsc(struct phy_device *phydev,
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

	val = secy->replay_protect ? SYS_REPLAY_PROTECT_EN : 0;
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			     MACSEC_SYS_CONFIG, SYS_REPLAY_PROTECT_EN, val);
	if (ret)
		return ret;

	val = (u16)(secy->replay_window & 0xffff);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			    MACSEC_SYS_REPLAY_WIN_BASE, val);
	if (ret)
		return ret;
	val = (u16)((secy->replay_window >> 16) & 0xffff);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			    MACSEC_SYS_REPLAY_WIN_BASE + 1, val);
	return ret;
}

static int qca_macsec_create_rxsc(struct phy_device *phydev, const u32 channel,
				  struct macsec_secy *secy,
				  struct macsec_rx_sc *rx_sc)
{
	int ret = 0;
	struct secy_rx_sc_policy_rule_t entry;

	memset(&entry, 0, sizeof(entry));

	entry.rule_valid = true;
	entry.action.rx_sc_index = channel;
	entry.rx_sci = swab64((__force u64)rx_sc->sci);

	ret = qca_macsec_secy_rx_sc_policy_set(phydev, channel, &entry);
	if (ret) {
		phydev_warn(phydev, "%s: fail to create rx_sc policy %d!\n",
			    __func__, channel);
		return ret;
	}

	ret = qca_macsec_update_rxsc(phydev, channel, secy);
	return ret;
}

static int qca_macsec_update_txsa(struct phy_device *phydev,
				  const int channel,
				  const unsigned char an,
				  const struct macsec_tx_sa *tx_sa,
				  bool update_pn)
{
	int ret = 0;

	if (update_pn) {
		ret = qca_macsec_secy_tx_sa_npn_set(phydev, channel, an,
						    tx_sa->next_pn);
		if (ret) {
			phydev_warn(phydev,
				    "%s: fail to tx_sa_next_pn_set!\n",
				    __func__);
			return ret;
		}
	}

	ret = qca_macsec_secy_tx_sa_en_set(phydev, channel, an, tx_sa->active);
	return ret;
}

static int qca_macsec_update_rxsa(struct phy_device *phydev,
				  const int channel,
				  const unsigned char an,
				  const struct macsec_rx_sa *rx_sa,
				  bool update_pn)
{
	int ret = 0;

	if (update_pn) {
		ret = qca_macsec_secy_rx_sa_npn_set(phydev, channel, an,
						    rx_sa->next_pn);
		if (ret) {
			phydev_warn(phydev,
				    "%s: fail to rx_sa_next_pn_set!\n",
				    __func__);
			return ret;
		}
	}

	ret = qca_macsec_secy_rx_sa_en_set(phydev, channel, an, rx_sa->active);
	return ret;
}

static struct qca_macsec_cfg_t *qca_macsec_get_cfg(struct phy_device *phydev)
{
	struct qca_macsec_ctx *ctx;

	ctx = container_of(phydev->macsec_ops, struct qca_macsec_ctx,
			   mdo_ops);
	return &ctx->config;
}

/* ===================== Common MDO Implementations ===================== */

/* Device control */
static int qca_mdo_dev_open(struct macsec_context *ctx)
{
	return qca_macsec_secy_ctrl_set(ctx->phydev, true);
}

static int qca_mdo_dev_stop(struct macsec_context *ctx)
{
	return qca_macsec_secy_ctrl_set(ctx->phydev, false);
}

/* SecY lifecycle */
static int qca_mdo_add_secy(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct qca_macsec_chip_info chip_info;
	u32 txsc_idx = 0;
	int ret = 0;

	if (hweight_long(pcfg->txsc_idx_bits) >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	txsc_idx = ffz(pcfg->txsc_idx_bits);
	if (txsc_idx >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	/* MACsec clock enabled only once */
	if (pcfg->txsc_idx_bits == 0) {
		if (!qca_macsec_get_chip_info(qca_macsec_get_phy_id(ctx->phydev),
				      &chip_info))
			return -EOPNOTSUPP;
		ret = qca_macsec_sw_set(ctx->phydev, true);
		if (ret)
			return ret;
		/* Call chip-specific MACsec hardware init function */
		ret = chip_info.init(ctx->phydev);
		if (ret) {
			phydev_err(ctx->phydev, "%s: chip init failed for %s\n",
				   __func__, chip_info.name);
			return ret;
		}
		/* Ensure secy is in the default state */
		ret = qca_macsec_secy_ctrl_set(ctx->phydev, false);
		if (ret)
			return ret;
	}

	ret = qca_macsec_secy_cipher_suite_set(ctx->phydev, ctx->secy);
	if (ret) {
		phydev_warn(ctx->phydev,
			    "%s: fail to set secy_cipher_suite!\n", __func__);
		return ret;
	}

	ret = qca_macsec_create_txsc(ctx->phydev, txsc_idx, ctx->secy);
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
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	int channel = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	return qca_macsec_update_txsc(ctx->phydev, channel, ctx->secy);
}

static int qca_mdo_del_secy(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_tx_sc_policy_rule_t entry;
	int channel = 0, ret = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&entry, 0, sizeof(entry));
	ret = qca_macsec_secy_tx_sc_policy_set(ctx->phydev, channel, &entry);
	if (ret) {
		phydev_warn(ctx->phydev, "%s: fail to del secy!\n", __func__);
		return ret;
	}

	clear_bit(channel, &pcfg->txsc_idx_bits);
	pcfg->secy_txsc[channel].sw_secy = NULL;
	pcfg->secy_txsc[channel].hw_sc_idx = 0;
	/* last TXSC removed -> disable MACsec clock */
	if (pcfg->txsc_idx_bits == 0) {
		/* Ensure secy returns to the default state */
		ret = qca_macsec_secy_ctrl_set(ctx->phydev, false);
		if (ret)
			return ret;
		ret = qca_macsec_sw_set(ctx->phydev, false);
		if (ret)
			return ret;
	}

	return 0;
}

/* SC lifecycle */
static int qca_mdo_add_rxsc(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	u32 rxsc_idx = 0;
	int ret = 0;

	if (hweight_long(pcfg->rxsc_idx_bits) >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	rxsc_idx = ffz(pcfg->rxsc_idx_bits);
	if (rxsc_idx >= QCA_SECY_SC_MAX_NUM)
		return -ENOSPC;

	ret = qca_macsec_create_rxsc(ctx->phydev, rxsc_idx, ctx->secy, ctx->rx_sc);
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
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	int channel = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	return qca_macsec_update_rxsc(ctx->phydev, channel, ctx->secy);
}

static int qca_mdo_del_rxsc(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_rx_sc_policy_rule_t entry;
	int channel = 0;
	int ret = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&entry, 0, sizeof(entry));
	ret = qca_macsec_secy_rx_sc_policy_set(ctx->phydev, channel, &entry);

	clear_bit(channel, &pcfg->rxsc_idx_bits);
	pcfg->secy_rxsc[channel].sw_secy = NULL;
	pcfg->secy_rxsc[channel].rx_sc = NULL;
	pcfg->secy_rxsc[channel].hw_sc_idx = 0;

	return ret;
}

/* SA lifecycle: TXSA */
static int qca_mdo_add_txsa(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_sak_t tx_sak;
	struct secy_sa_ki_t tx_ki;
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

	ret = qca_macsec_secy_tx_sak_set(ctx->phydev,
					 channel, ctx->sa.assoc_num, &tx_sak);
	if (ret) {
		phydev_warn(ctx->phydev,
			    "%s: fail to secy_tx_sak_set!\n", __func__);
		return ret;
	}

	if (ctx->secy->xpn) {
		ret = qca_macsec_secy_tx_sc_ssci_set(ctx->phydev, channel,
			be32_to_cpu((__force __be32)ctx->sa.tx_sa->ssci));
		if (ret) {
			phydev_warn(ctx->phydev,
				    "%s: fail to secy_tx_ssci_set!\n", __func__);
			return ret;
		}

		memset(&tx_ki, 0, sizeof(struct secy_sa_ki_t));
		for (i = 0; i < 12; i++)
			tx_ki.ki[15 - i] = ctx->sa.tx_sa->key.salt.bytes[i];
		ret = qca_macsec_secy_tx_sa_ki_set(ctx->phydev, channel,
						ctx->sa.assoc_num, &tx_ki);
		if (ret) {
			phydev_warn(ctx->phydev,
				    "%s: fail to secy_tx_sa_ki_set!\n", __func__);
			return ret;
		}
	}

	ret = qca_macsec_secy_tx_sa_create(ctx->phydev,
					   channel, ctx->sa.assoc_num);
	if (ret) {
		phydev_warn(ctx->phydev,
			    "%s: fail to tx_sa_create!\n", __func__);
		return ret;
	}

	ret = qca_macsec_update_txsa(ctx->phydev, channel, ctx->sa.assoc_num,
				     ctx->sa.tx_sa, true);
	return ret;
}

static int qca_mdo_upd_txsa(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	int channel = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	return qca_macsec_update_txsa(ctx->phydev, channel, ctx->sa.assoc_num,
				      ctx->sa.tx_sa, ctx->sa.update_pn);
}

static int qca_mdo_del_txsa(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	int channel = 0, ret = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	ret = qca_macsec_secy_tx_sa_del(ctx->phydev,
					channel, ctx->sa.assoc_num);
	return ret;
}

/* SA lifecycle: RXSA */
static int qca_mdo_add_rxsa(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	struct secy_sak_t rx_sak;
	struct secy_sa_ki_t rx_ki;
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

	ret = qca_macsec_secy_rx_sa_create(ctx->phydev,
					   channel, ctx->sa.assoc_num);
	if (ret) {
		phydev_warn(ctx->phydev,
			    "%s: fail to rx_sa_create!\n", __func__);
		return ret;
	}

	ret = qca_macsec_secy_rx_sak_set(ctx->phydev,
					 channel, ctx->sa.assoc_num, &rx_sak);
	if (ret) {
		phydev_warn(ctx->phydev,
			    "%s: fail to rx_sak_set!\n", __func__);
		return ret;
	}

	if (ctx->secy->xpn) {
		ret = qca_macsec_secy_rx_sc_ssci_set(ctx->phydev, channel,
			be32_to_cpu((__force __be32)ctx->sa.rx_sa->ssci));
		if (ret) {
			phydev_warn(ctx->phydev,
				    "%s: fail to secy_rx_ssci_set!\n", __func__);
			return ret;
		}

		memset(&rx_ki, 0, sizeof(struct secy_sa_ki_t));
		for (i = 0; i < 12; i++)
			rx_ki.ki[15 - i] = ctx->sa.rx_sa->key.salt.bytes[i];
		ret = qca_macsec_secy_rx_sa_ki_set(ctx->phydev, channel,
						ctx->sa.assoc_num, &rx_ki);
		if (ret) {
			phydev_warn(ctx->phydev,
				    "%s: fail to secy_rx_sa_ki_set!\n", __func__);
			return ret;
		}
	}

	ret = qca_macsec_update_rxsa(ctx->phydev, channel, ctx->sa.assoc_num,
				     ctx->sa.rx_sa, true);
	return ret;
}

static int qca_mdo_upd_rxsa(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	int channel = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, rx_sc);
	if (channel < 0)
		return -ENOENT;

	return qca_macsec_update_rxsa(ctx->phydev, channel, ctx->sa.assoc_num,
				      ctx->sa.rx_sa, ctx->sa.update_pn);
}

static int qca_mdo_del_rxsa(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	int channel = 0, ret = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, rx_sc);
	if (channel < 0)
		return -ENOENT;

	ret = qca_macsec_secy_rx_sa_del(ctx->phydev,
					channel, ctx->sa.assoc_num);
	return ret;
}

/* MIB statistics (default 64-bit path) */
static int qca_mdo_get_dev_stats(struct macsec_context *ctx)
{
	struct secy_rx_mib_t rxmib;
	struct secy_tx_mib_t txmib;

	memset(&rxmib, 0, sizeof(rxmib));
	memset(&txmib, 0, sizeof(txmib));

	qca_macsec_secy_rx_mib_get(ctx->phydev, &rxmib);
	qca_macsec_secy_tx_mib_get(ctx->phydev, &txmib);

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
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_tx_sc_mib_t txscmib;
	struct secy_tx_sa_mib_t txsamib;
	int channel = 0;
	u32 i;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&txscmib, 0, sizeof(struct secy_tx_sc_mib_t));

	qca_macsec_secy_tx_sc_mib_get(ctx->phydev, channel, &txscmib);

	ctx->stats.tx_sc_stats->OutOctetsProtected = txscmib.protected_octets;
	ctx->stats.tx_sc_stats->OutOctetsEncrypted = txscmib.encrypted_octets;

	for (i = 0; i < 2; i++) {
		memset(&txsamib, 0, sizeof(struct secy_tx_sa_mib_t));
		qca_macsec_secy_tx_sa_mib_get(ctx->phydev, channel, i, &txsamib);

		ctx->stats.tx_sc_stats->OutPktsProtected += txsamib.protected_pkts;
		ctx->stats.tx_sc_stats->OutPktsEncrypted += txsamib.encrypted_pkts;
	}
	return 0;
}

static int qca_mdo_get_tx_sa_stats(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_tx_sa_mib_t txsamib;
	int channel = 0;

	channel = qca_get_txsc_idx_from_secy(pcfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&txsamib, 0, sizeof(struct secy_tx_sa_mib_t));

	qca_macsec_secy_tx_sa_mib_get(ctx->phydev,
				      channel, ctx->sa.assoc_num, &txsamib);

	ctx->stats.tx_sa_stats->OutPktsProtected = txsamib.protected_pkts;
	ctx->stats.tx_sa_stats->OutPktsEncrypted = txsamib.encrypted_pkts;

	return 0;
}

static int qca_mdo_get_rx_sc_stats(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_rx_sc_mib_t secy_rx_sc_mib;
	struct secy_rx_sa_mib_t secy_rx_sa_mib;
	int channel = 0;
	u32 i;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&secy_rx_sc_mib, 0, sizeof(secy_rx_sc_mib));

	qca_macsec_secy_rx_sc_mib_get(ctx->phydev, channel, &secy_rx_sc_mib);

	ctx->stats.rx_sc_stats->InOctetsValidated = secy_rx_sc_mib.validated_octets;
	ctx->stats.rx_sc_stats->InOctetsDecrypted = secy_rx_sc_mib.decrypted_octets;
	ctx->stats.rx_sc_stats->InPktsUnchecked = secy_rx_sc_mib.unchecked_pkts;
	ctx->stats.rx_sc_stats->InPktsDelayed = secy_rx_sc_mib.delayed_pkts;
	ctx->stats.rx_sc_stats->InPktsLate = secy_rx_sc_mib.late_pkts;

	for (i = 0; i < 2; i++) {
		memset(&secy_rx_sa_mib, 0, sizeof(secy_rx_sa_mib));
		qca_macsec_secy_rx_sa_mib_get(ctx->phydev,
						channel, i, &secy_rx_sa_mib);
		ctx->stats.rx_sc_stats->InPktsOK += secy_rx_sa_mib.ok_pkts;
		ctx->stats.rx_sc_stats->InPktsInvalid += secy_rx_sa_mib.invalid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotValid += secy_rx_sa_mib.not_valid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotUsingSA += secy_rx_sa_mib.not_using_sa;
		ctx->stats.rx_sc_stats->InPktsUnusedSA += secy_rx_sa_mib.unused_sa;
	}
	return 0;
}

static int qca_mdo_get_rx_sa_stats(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *pcfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_rx_sa_mib_t rxsamib;
	int channel = 0;

	channel = qca_get_rxsc_idx_from_rxsc(pcfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&rxsamib, 0, sizeof(struct secy_rx_sa_mib_t));

	qca_macsec_secy_rx_sa_mib_get(ctx->phydev,
				      channel, ctx->sa.assoc_num, &rxsamib);

	ctx->stats.rx_sa_stats->InPktsOK = rxsamib.ok_pkts;
	ctx->stats.rx_sa_stats->InPktsInvalid = rxsamib.invalid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotValid = rxsamib.not_valid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotUsingSA = rxsamib.not_using_sa;
	ctx->stats.rx_sa_stats->InPktsUnusedSA = rxsamib.unused_sa;

	return 0;
}

/* ===== 808x/8084 MDO wrapper functions ===== */

/* Device-level statistics (32-bit) */
static int qca808x_mdo_get_dev_stats(struct macsec_context *ctx)
{
	struct secy_rx_mib_t rxmib;
	struct secy_tx_mib_t txmib;

	memset(&rxmib, 0, sizeof(rxmib));
	memset(&txmib, 0, sizeof(txmib));

	qca808x_macsec_secy_rx_mib_get(ctx->phydev, &rxmib);
	qca808x_macsec_secy_tx_mib_get(ctx->phydev, &txmib);

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

/* TX SC statistics (32-bit SC, 64-bit SA) */
static int qca808x_mdo_get_tx_sc_stats(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *cfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_tx_sc_mib_t txscmib;
	struct secy_tx_sa_mib_t txsamib;
	int channel, i;

	channel = qca_get_txsc_idx_from_secy(cfg, ctx->secy);
	if (channel < 0)
		return -ENOENT;

	memset(&txscmib, 0, sizeof(txscmib));
	qca808x_macsec_secy_tx_sc_mib_get(ctx->phydev, channel, &txscmib);

	ctx->stats.tx_sc_stats->OutOctetsProtected = txscmib.protected_octets;
	ctx->stats.tx_sc_stats->OutOctetsEncrypted = txscmib.encrypted_octets;

	for (i = 0; i < 2; i++) {
		memset(&txsamib, 0, sizeof(txsamib));
		qca_macsec_secy_tx_sa_mib_get(ctx->phydev, channel, i, &txsamib);

		ctx->stats.tx_sc_stats->OutPktsProtected += txsamib.protected_pkts;
		ctx->stats.tx_sc_stats->OutPktsEncrypted += txsamib.encrypted_pkts;
	}

	return 0;
}

/* RX SC statistics (32-bit SC) */
static int qca808x_mdo_get_rx_sc_stats(struct macsec_context *ctx)
{
	struct qca_macsec_cfg_t *cfg = qca_macsec_get_cfg(ctx->phydev);
	struct secy_rx_sc_mib_t rxscmib;
	struct secy_rx_sa_mib_t rxsamib;
	int channel, i;

	channel = qca_get_rxsc_idx_from_rxsc(cfg, ctx->rx_sc);
	if (channel < 0)
		return -ENOENT;

	memset(&rxscmib, 0, sizeof(rxscmib));
	qca808x_macsec_secy_rx_sc_mib_get(ctx->phydev, channel, &rxscmib);

	ctx->stats.rx_sc_stats->InOctetsValidated = rxscmib.validated_octets;
	ctx->stats.rx_sc_stats->InOctetsDecrypted = rxscmib.decrypted_octets;
	ctx->stats.rx_sc_stats->InPktsUnchecked = rxscmib.unchecked_pkts;
	ctx->stats.rx_sc_stats->InPktsDelayed = rxscmib.delayed_pkts;
	ctx->stats.rx_sc_stats->InPktsLate = rxscmib.late_pkts;

	for (i = 0; i < 2; i++) {
		memset(&rxsamib, 0, sizeof(rxsamib));
		qca_macsec_secy_rx_sa_mib_get(ctx->phydev, channel, i, &rxsamib);
		ctx->stats.rx_sc_stats->InPktsOK += rxsamib.ok_pkts;
		ctx->stats.rx_sc_stats->InPktsInvalid += rxsamib.invalid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotValid += rxsamib.not_valid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotUsingSA += rxsamib.not_using_sa;
		ctx->stats.rx_sc_stats->InPktsUnusedSA += rxsamib.unused_sa;
	}

	return 0;
}

/* 808x/8084 hardware configuration. */
int qca808x_macsec_config_init(struct phy_device *phydev)
{
	struct secy_rx_mib_t rxmib = {0};
	struct secy_tx_mib_t txmib = {0};
	struct secy_tx_sc_mib_t txscmib = {0};
	struct secy_rx_sc_mib_t rxscmib = {0};
	struct secy_tx_sa_mib_t txsamib = {0};
	struct secy_rx_sa_mib_t rxsamib = {0};
	u32 sc, sa;
	int ret;

	/* Set AZ forward */
	ret = qca_macsec_forward_az_en_set(phydev, true);
	if (ret)
		return ret;

	/* Read-clear dev-level MIBs (32-bit) */
	qca808x_macsec_secy_rx_mib_get(phydev, &rxmib);
	qca808x_macsec_secy_tx_mib_get(phydev, &txmib);

	/* Read-clear SC-level (32-bit) and SA-level (64-bit) MIBs */
	for (sc = 0; sc < QCA_SECY_SC_MAX_NUM; sc++) {
		qca808x_macsec_secy_tx_sc_mib_get(phydev, sc, &txscmib);
		qca808x_macsec_secy_rx_sc_mib_get(phydev, sc, &rxscmib);

		for (sa = 0; sa < 2; sa++) {
			qca_macsec_secy_tx_sa_mib_get(phydev, sc, sa, &txsamib);
			qca_macsec_secy_rx_sa_mib_get(phydev, sc, sa, &rxsamib);
		}
	}

	/* Clear policies and init PN to 1 */
	ret = qca_macsec_policy_bind_clear(phydev);
	if (ret)
		return ret;

	return qca_macsec_init_all_pn(phydev, 1);
}

/* 81xx hardware configuration. */
int qca81xx_macsec_config_init(struct phy_device *phydev)
{
	int ret = 0;

	/* Recommended configuration for IPG extension (81xx-specific) */
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xE003, 0xb);
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xF003, 0x3);
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xE006, 0x3);
	phy_write_mmd(phydev, MDIO_MMD_PCS, 0xF006, 0x3);

	/* Read-clear all MIBs (64-bit) */
	ret = qca_macsec_mib_clear(phydev);
	if (ret)
		return ret;

	/* Clear policy bind masks */
	ret = qca_macsec_policy_bind_clear(phydev);
	if (ret)
		return ret;

	/* Initialize all PN to 1 */
	return qca_macsec_init_all_pn(phydev, 1);
}

/* 1204 hardware configuration. */
int qce1204_macsec_config_init(struct phy_device *phydev)
{
	int ret = 0;

	/* Set AZ forward */
	ret = qca_macsec_forward_az_en_set(phydev, true);
	if (ret)
		return ret;

	/* Read-clear all MIBs (64-bit) */
	ret = qca_macsec_mib_clear(phydev);
	if (ret)
		return ret;

	/* Clear policy bind masks */
	ret = qca_macsec_policy_bind_clear(phydev);
	if (ret)
		return ret;

	/* Initialize all PN to 1 */
	return qca_macsec_init_all_pn(phydev, 1);
}

static int qca_macsec_device_attach(struct net_device *dev,
				    const struct qca_macsec_chip_info *chip_info)
{
	struct phy_device *phydev = NULL;
	struct qca_macsec_ctx *ctx = NULL;

	/* phydev is null or already initialized */
	if (!dev)
		return 0;
	phydev = dev->phydev;
	if (!phydev || phydev->macsec_ops)
		return 0;

	/* Allocate context */
	ctx = kzalloc(sizeof(struct qca_macsec_ctx), GFP_KERNEL);
	if (!ctx) {
		phydev_err(phydev, "%s: failed to allocate context for %s\n",
				__func__, chip_info->name);
		return -ENOMEM;
	}

	/* Initialize MDO operations structure (common operations) */
	ctx->mdo_ops = (struct macsec_ops){
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
		/* SA stats are always 64-bit for all chips */
		.mdo_get_tx_sa_stats = qca_mdo_get_tx_sa_stats,
		.mdo_get_rx_sa_stats = qca_mdo_get_rx_sa_stats,
	};

	/* Dynamically bind statistics functions based on chip capabilities */
	if (chip_info->capabilities & QCA_MACSEC_CAP_MIXED_MIB) {
		/* QCA808x/8084: Mixed MIB (32-bit dev/SC, 64-bit SA) */
		ctx->mdo_ops.mdo_get_dev_stats = qca808x_mdo_get_dev_stats;
		ctx->mdo_ops.mdo_get_tx_sc_stats = qca808x_mdo_get_tx_sc_stats;
		ctx->mdo_ops.mdo_get_rx_sc_stats = qca808x_mdo_get_rx_sc_stats;
	} else {
		/* QCA81xx/QCE1204: Full 64-bit MIB */
		ctx->mdo_ops.mdo_get_dev_stats = qca_mdo_get_dev_stats;
		ctx->mdo_ops.mdo_get_tx_sc_stats = qca_mdo_get_tx_sc_stats;
		ctx->mdo_ops.mdo_get_rx_sc_stats = qca_mdo_get_rx_sc_stats;
	}

	/* Bind operations to phydev. HW init is deferred to first add_secy. */
	phydev->macsec_ops = &ctx->mdo_ops;
	ctx->phydev = phydev;

	/* Enable HW MACsec feature on netdev */
	dev->features |= NETIF_F_HW_MACSEC;

	return 0;
}

static int qca_macsec_device_detach(struct net_device *dev)
{
	struct phy_device *phydev = dev->phydev;
	struct qca_macsec_ctx *ctx;

	if (!phydev || !phydev->macsec_ops)
		return 0;

	/* Disable MACsec in hardware */
	qca_macsec_sw_set(phydev, false);

	/* Clear netdev feature flag */
	dev->features &= ~NETIF_F_HW_MACSEC;

	/* Get context before clearing ops pointer */
	ctx = container_of(phydev->macsec_ops, struct qca_macsec_ctx,
			   mdo_ops);

	/* Clear ops pointer */
	phydev->macsec_ops = NULL;

	/* Free context */
	if (ctx)
		kfree(ctx);

	return 0;
}

static inline int qca_macsec_dev_event(struct notifier_block *nb,
					unsigned long event, void *info)
{
	struct net_device *event_dev = netdev_notifier_info_to_dev(info);
	struct phy_device *phydev = NULL;
	struct qca_common_private *cpriv = NULL;
	struct qca_macsec_chip_info chip_info;
	const char *sku_name;
	int ret = 0;
	u32 phyid = 0;
	u32 chip = 0;

	if (!event_dev->phydev)
		return NOTIFY_DONE;

	/*
	 * Early filter: only handle netdev events we actually care about.
	 * Add new cases here (kept in sync with the dispatch switch below)
	 * when new events need to be supported.
	 */
	switch (event) {
	case NETDEV_REGISTER:
	case NETDEV_UNREGISTER:
		break;
	default:
		return NOTIFY_DONE;
	}

	phydev = event_dev->phydev;

	/* Get PHY ID */
	phyid = qca_macsec_get_phy_id(phydev);
	chip = phyid & phydev->drv->phy_id_mask;

	/* Look up chip info - early return if not supported */
	if (!qca_macsec_get_chip_info(chip, &chip_info))
		return NOTIFY_DONE;

	/* Filter DSA devices for QCA8084 in MHT Switch mode */
	if (chip == QCA8084_PHY) {
	#if IS_ENABLED(CONFIG_NET_DSA)
		if (dsa_slave_dev_check(event_dev))
			return NOTIFY_DONE;
	#endif
	}

	/*
	 * Gate MACsec attach by SKU. QCA81xx keeps SKU info in phydev->priv;
	 * QCE1204 keeps it in the package-shared priv (shared->priv). In both
	 * cases the SKU info is the first member, so the read-only shadow
	 * struct (struct qca_common_private) applies.
	 */
	if (chip == QCA81xx_PHY)
		cpriv = (struct qca_common_private *)phydev->priv;
	else if (chip == QCE1204_PHY)
		cpriv = phydev->shared ?
			(struct qca_common_private *)phydev->shared->priv : NULL;

	if (chip == QCA81xx_PHY || chip == QCE1204_PHY) {
		sku_name = (cpriv && cpriv->sku.name) ? cpriv->sku.name : NULL;

		/* Read-only access to SKU; skip attach if MACsec not supported */
		if (!cpriv || !cpriv->sku.macsec) {
			phydev_info(phydev,
				    "MACsec disabled by SKU%s%s%s, skip attach\n",
				    sku_name ? " (" : "",
				    sku_name ? sku_name : "",
				    sku_name ? ")" : "");
			return NOTIFY_DONE;
		}
	}

	switch (event) {
	case NETDEV_REGISTER:
		phydev_info(phydev, "%s phyid: 0x%x (%s)\n",
				event_dev->name, phyid, chip_info.name);
		ret = qca_macsec_device_attach(event_dev, &chip_info);
		if (ret)
			ret = NOTIFY_DONE;
		else
			ret = NOTIFY_OK;
		break;
	case NETDEV_UNREGISTER:
		ret = qca_macsec_device_detach(event_dev);
		if (ret)
			ret = NOTIFY_DONE;
		else
			ret = NOTIFY_OK;
		break;
	default:
		ret = NOTIFY_DONE;
		break;
	}

	return ret;
}

/*
 * Linux Net device Notifier
 */
static struct notifier_block qca_macsec_netdev_notifier = {
	.notifier_call = qca_macsec_dev_event,
};

static int __init qca_macsec_module_init(void)
{
	int rv;

	rv = register_netdevice_notifier(&qca_macsec_netdev_notifier);
	if (rv) {
		pr_err("%s: Failed to register NETDEV notifier, error=%d\n",
			__func__, rv);
		return rv;
	}

	return 0;
}

static void __exit qca_macsec_module_exit(void)
{
	struct net_device *ndev;
	struct phy_device *phydev;
	struct qca_macsec_chip_info chip_info;
	struct qca_macsec_ctx *ctx;
	u32 phyid;

	/* Unregister notifier first to prevent new attachments */
	unregister_netdevice_notifier(&qca_macsec_netdev_notifier);

	/* Traverse init_net and cleanup all attached devices */
	rtnl_lock();
	for_each_netdev(&init_net, ndev) {
		phydev = ndev->phydev;

		/* Skip if no phydev or no macsec_ops attached */
		if (!phydev || !phydev->macsec_ops || !phydev->drv)
			continue;

		/* Get PHY ID and check if it's a supported QCA chip */
		phyid = qca_macsec_get_phy_id(phydev);
		if (!qca_macsec_get_chip_info(phyid & phydev->drv->phy_id_mask,
					      &chip_info))
			continue;

		/* Disable MACsec in hardware */
		qca_macsec_sw_set(phydev, false);

		/* Clear netdev feature flag */
		ndev->features &= ~NETIF_F_HW_MACSEC;

		/* Get context before clearing ops pointer */
		ctx = container_of(phydev->macsec_ops, struct qca_macsec_ctx,
				   mdo_ops);

		/* Clear ops pointer */
		phydev->macsec_ops = NULL;

		/* Free context */
		if (ctx)
			kfree(ctx);
	}
	rtnl_unlock();
}

module_init(qca_macsec_module_init);
module_exit(qca_macsec_module_exit);
MODULE_DESCRIPTION("Qualcomm QCA MACsec Lib driver");
MODULE_LICENSE("Dual BSD/GPL");
