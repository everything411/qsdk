/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _QCA81XX_H_
#define _QCA81XX_H_

#include <linux/phy.h>
#include <linux/build_bug.h>

#define QCA8111_PHY		0x004dd1c0
#if IS_ENABLED(CONFIG_HWMON)
#define QCA81XX_SENSORS_NUM		3
#define QCA81XX_THERMAL_HYSTERESIS_MDEG	5000	/* 5C */
#endif
/* in QCOM MDIO bus driver, bit29~31 is for soc type, 2 is for laguna */
/* and bit24~28 is for phy address, 0~23 is for soc address */
#define TO_QCA81XX_PHY_SOC_ADDR(addr, reg)		\
	((BIT(30) | reg) | (addr << 24))

/*SOC TLMM registers*/
#define TLMM_BASE		0x400000
#define TLMM_GPIO_OFFSET	0x1000
#define TO_TLMM_CFG_REG(pin)	(TLMM_BASE + TLMM_GPIO_OFFSET * pin)
#define TLMM_FUNC_MASK		GENMASK(5, 2)

enum {
	GPIO0_WOL_INT = 0,
	GPIO1_PHY_INT,
	GPIO2_LED0,
	GPIO3_LED1,
	GPIO4_LED3,
	GPIO5_PPS_IN = 5,
	GPIO6_TOD_IN = 6,
	GPIO7_REFCLK_IN = 7,
	GPIO10_PPS_OUT = 10,
	GPIO11_TOD_OUT = 11,
	GPIO12_CLK125_TDI = 12,
	GPIO_MAX
};

enum {
	QCA8101 = 0x2990E1,
	QCA8102 = 0x29A0E1,
	QCA8111 = 0x29B0E1,
	QCA8112 = 0x29C0E1,
};

struct qca81xx_sku_info {
	const char *name;
	bool ptp;
	bool macsec;
};

int __qca81xx_phy_debug_write(struct phy_device *phydev,
	unsigned int reg, u16 val);
int qca81xx_phy_debug_write(struct phy_device *phydev,
	unsigned int reg, u16 val);
int __qca81xx_phy_debug_read(struct phy_device *phydev,
	unsigned int reg);
int qca81xx_phy_debug_read(struct phy_device *phydev,
	unsigned int reg);
int qca81xx_phy_debug_modify(struct phy_device *phydev,
			     unsigned int reg, u16 clear, u16 set);
u32 __qca81xx_soc_read(struct phy_device *phydev, u32 reg);
int __qca81xx_soc_write(struct phy_device *phydev,
	u32 reg, u32 val);
int __qca81xx_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set);
u32 qca81xx_soc_read(struct phy_device *phydev, u32 reg);
int qca81xx_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set);
#if IS_ENABLED(CONFIG_HWMON)
int qca81xx_hwmon_hw_init(struct phy_device *phydev);
int qca81xx_hwmon_probe(struct phy_device *phydev);
#endif
int qca81xx_phy_stats_enable(struct phy_device *phydev);
enum qca81xx_init_state {
	QCA81XX_INIT_START = 0,
	QCA81XX_INIT_GCC_PRE_INIT_FAILURE,
	QCA81XX_INIT_ANA_CONFIG_FAILURE,
	QCA81XX_INIT_PCS_USXGMII_FAILURE,
	QCA81XX_INIT_GCC_POST_INIT_FAILURE,
	QCA81XX_INIT_SEC_CTRL_FAILURE,
	QCA81XX_INIT_TLMM_FAILURE,
	QCA81XX_INIT_SUCCESS,
	QCA81XX_INIT_INVALID_STATE = 0xff,
};

int qca81xx_debugfs_init(struct phy_device *phydev);
void qca81xx_debugfs_exit(struct phy_device *phydev);

struct qca81xx_debug_stats {
	atomic64_t read_status_count;
	atomic64_t config_aneg_count;
	atomic64_t soft_reset_count;
	atomic64_t fifo_reset_count;
	atomic64_t suspend_count;
	atomic64_t resume_count;
};

struct qca81xx_private {

	struct qca81xx_sku_info sku;
#if IS_ENABLED(CONFIG_HWMON)
	struct device *hwmon_dev;
#endif
	u16 afe_dac8;
	u16 afe_dac9;
	struct qca81xx_debug_stats debug_stats;
	enum qca81xx_init_state init_state;
	bool pcs_assert;
	bool hibernation;
#if IS_ENABLED(CONFIG_HWMON)
	long temp_warn_mdeg[QCA81XX_SENSORS_NUM];	/* milli-C, default 100000 */
	long temp_crit_mdeg[QCA81XX_SENSORS_NUM];	/* milli-C, default 120000 */
	bool temp_shutdown_latched;			/* set on shutdown, cleared on auto-recover */
#endif
};

/*
 * The MACsec module reads the SKU via a read-only shadow struct cast from
 * phydev->priv, which requires sku to be the first member. Enforce it.
 */
static_assert(offsetof(struct qca81xx_private, sku) == 0,
	      "qca81xx_private.sku must stay the first member (MACsec shadow struct)");

ssize_t qca81xx_phy_show_snr(struct device *dev, struct device_attribute *attr, char *buf);
int qca81xx_phy_suspend(struct phy_device *phydev);
int qca81xx_phy_resume(struct phy_device *phydev);
int qca81xx_phy_hibernation_get(struct phy_device *phydev, bool *enabled);
#if IS_ENABLED(CONFIG_HWMON)
void qca81xx_thermal_check(struct phy_device *phydev);
#endif

/* Master/slave control bits in MDIO_AN_10GBT_CTRL (MMD7, reg 0x20) */
#define QCA81XX_MS_FORCE_EN		BIT(15)
#define QCA81XX_MS_MASTER		BIT(14)
#define QCA81XX_MS_PREFER_MASTER	BIT(13)
#define QCA81XX_MS_CTRL_MASK		(BIT(15) | BIT(14) | BIT(13))

int qca81xx_phy_master_slave_set(struct phy_device *phydev);
int qca81xx_phy_master_slave_get(struct phy_device *phydev);

#endif /* _QCA81XX_H_ */
