/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _QCE1204_H_
#define _QCE1204_H_

#include <linux/phy.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/build_bug.h>
#include "../qca81xx/qca81xx.h"

#if IS_ENABLED(CONFIG_HWMON)
#define QCE1204_SENSORS_NUM			2
#endif

#define QCE1204_PORT_NUM			4

#define QCE1204_PHY				0x004dd190
#define QCE1204_TLMM_BASE			0x400000
#define QCE1204_TLMM_GPIO_OFFSET		0x1000
#define QCE1204_TO_TLMM_CFG_REG(pin)		(QCE1204_TLMM_BASE + QCE1204_TLMM_GPIO_OFFSET * pin)
#define QCE1204_TLMM_GPIO_PULL			GENMASK(1, 0)
#define QCE1204_TLMM_FUNC_MASK			GENMASK(5, 2)
#define QCE1204_TLMM_DRV			GENMASK(8, 6)
#define QCE1204_TLMM_LED_MODE			BIT(11)

/* GPIO_PULL field values (bits 1:0) — complete enumeration for future use */
#define QCE1204_TLMM_PULL_NO_PULL		0
#define QCE1204_TLMM_PULL_DOWN			1
#define QCE1204_TLMM_PULL_KEEPER		2
#define QCE1204_TLMM_PULL_UP			3

/* DRV_STRENGTH field values (bits 8:6) — complete enumeration for future use */
#define QCE1204_TLMM_DRV_2_MA			0
#define QCE1204_TLMM_DRV_4_MA			1
#define QCE1204_TLMM_DRV_6_MA			2
#define QCE1204_TLMM_DRV_8_MA			3
#define QCE1204_TLMM_DRV_10_MA			4
#define QCE1204_TLMM_DRV_12_MA			5
#define QCE1204_TLMM_DRV_14_MA			6
#define QCE1204_TLMM_DRV_16_MA			7

/* Sentinel: field left at hardware default, no register write for this field */
#define QCE1204_TLMM_NC				0xFF

#define QCE1204_SKU_REG				0x90607C
#define QCE1204_SKU_ID_MASK			GENMASK(23, 0)

enum qce1204_addr_offset {
	PCS0_ADDR_OFFSET = 4,
	PCS1_ADDR_OFFSET = 5,
	QCE1204_SOC_ADDR_OFFSET = 6,
};

struct qce1204_clk_data {
	struct clk *tx_clk;
	struct clk *rx_clk;
	struct clk *sys_clk;
	struct clk *tx_src_clk;
	struct clk *rx_src_clk;
	/* Reset control */
	struct reset_control *tx_reset;
	struct reset_control *rx_reset;
	struct reset_control *sys_reset;
};

struct qce1204_priv {
	struct qce1204_clk_data clk_data;
#if IS_ENABLED(CONFIG_HWMON)
	struct device *hwmon_dev;
	long temp_warn_mdeg[QCE1204_SENSORS_NUM];
	long temp_crit_mdeg[QCE1204_SENSORS_NUM];
	bool temp_shutdown_latched;
#endif
};

/* Clock type index for each channel */
enum qce1204_clk_type {
	QCE1204_CLK_GMII_TX = 0,
	QCE1204_CLK_GMII_RX,
	QCE1204_CLK_XGMII_TX,
	QCE1204_CLK_XGMII_RX,
	QCE1204_CLK_TYPE_MAX
};

/* Per-channel clocks and resets */
struct qce1204_channel_clk {
	struct clk *clks[QCE1204_CLK_TYPE_MAX];
	struct reset_control *resets[QCE1204_CLK_TYPE_MAX];
};

struct qce1204_shared_clk_data {
	struct qce1204_channel_clk channels[QCE1204_PORT_NUM];
	struct clk *pcs_sys_clk;
	struct clk *ahb_clk;
	struct clk *tx_parent;
	struct clk *rx_parent;
	struct reset_control *pcs_sys_reset;
	struct reset_control *xpcs_reset;
};

enum qce1204_sku_id {
	QCE1204_SKU_QCE1204 = 0x3450e1,
	QCE1204_SKU_QCE2204 = 0x3460e1,
	QCE1204_SKU_QCE2224 = 0x3470e1,
	QCE1204_SKU_QCE1224 = 0x3750e1,
};

struct qce1204_sku_info {
	const char *name;
	bool ptp;
	bool macsec;
	bool p3p4_2p5g;
};

struct qce1204_shared_priv {
	struct qce1204_sku_info sku;
	struct qce1204_shared_clk_data shared_clk_data;
	phy_interface_t package_mode;
	atomic_t ppsin_refcount;   /* PPS_IN GPIO refcount (all ports of one chip) */
	/* protects port_hibernation[], port_active, pcs_assert and probe_complete */
	struct mutex pcs_lock;
	u8 port_active;            /* bitmask of ports that completed probe, bit i = port i */
	/*
	 * Number of ports configured in DTS for this package (1-QCE1204_PORT_NUM).
	 * Written once in qce1204_phy_probe(), inside the phy_package_probe_once()
	 * guard, before any other port has joined the package.
	 */
	u8 num_active_ports;
	/*
	 * Set under pcs_lock once all num_active_ports have set their port_active
	 * bit; never cleared (one-shot).  Guards premature PCS assertion during
	 * the initial probe sequence.
	 */
	bool probe_complete;
	/*
	 * One-shot latch: set when the PCS power saving was suppressed because
	 * probe_complete is unset, so the warning is emitted only once per
	 * package instead of on every read_status poll.
	 */
	bool incomplete_warned;
	/* per-port hibernation state, indexed by (mdio.addr - shared->addr) */
	bool port_hibernation[QCE1204_PORT_NUM];
	bool pcs_assert;           /* current PCS ANA soft-reset state (true = asserted) */
#if IS_ENABLED(CONFIG_HWMON)
	u64 tem_base_code;
#endif
};

/*
 * The MACsec module reads the SKU via a read-only shadow struct cast from
 * phydev->shared->priv, which requires sku to be the first member. Enforce it.
 */
static_assert(offsetof(struct qce1204_shared_priv, sku) == 0,
	      "qce1204_shared_priv.sku must stay the first member (MACsec shadow struct)");

#define QCE1204_GPIO0_FUNC_PHY_INT		1

#define QCE1204_GPIO1_FUNC_P0_LED_0		1
#define QCE1204_GPIO2_FUNC_P1_LED_0		1
#define QCE1204_GPIO3_FUNC_P2_LED_0		1
#define QCE1204_GPIO4_FUNC_P3_LED_0		1

#define QCE1204_GPIO5_FUNC_P0_LED_2		2
#define QCE1204_GPIO6_FUNC_P1_LED_2		2
#define QCE1204_GPIO7_FUNC_P2_LED_2		3
#define QCE1204_GPIO8_FUNC_P3_LED_2		3

#define QCE1204_GPIO9_FUNC_P0_LED_1		3
#define QCE1204_GPIO15_FUNC_P1_LED_1		1
#define QCE1204_GPIO16_FUNC_P2_LED_1		1
#define QCE1204_GPIO17_FUNC_P3_LED_1		1

u32 __qce1204_soc_read(struct phy_device *phydev, u32 reg);
void __qce1204_soc_write(struct phy_device *phydev, u32 reg, u32 val);
int __qce1204_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set);
u32 qce1204_soc_read(struct phy_device *phydev, u32 reg);
int qce1204_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set);
int qce1204_phy_channel_get(struct phy_device *phydev);
int qce1204_phy_soft_reset(struct phy_device *phydev);
int qce1204_phy_config_aneg(struct phy_device *phydev);
int qce1204_phy_probe(struct phy_device *phydev);
void qce1204_phy_remove(struct phy_device *phydev);
int qce1204_phy_config_init(struct phy_device *phydev);
int qce1204_phy_read_status(struct phy_device *phydev);
#if IS_ENABLED(CONFIG_HWMON)
#define QCE1204_THERMAL_HYSTERESIS_MDEG		5000	/* 5C, mirrors QCA81XX_THERMAL_HYSTERESIS_MDEG */
int qce1204_hwmon_hw_init_once(struct phy_device *phydev);
int qce1204_hwmon_hw_init(struct phy_device *phydev);
int qce1204_hwmon_probe(struct phy_device *phydev);
void qce1204_thermal_check(struct phy_device *phydev);
#endif

struct ipq52xx_phy_priv {
	void __iomem *ldo_bias_reg;	/* TCSR_GPHY_LDO_BIAS_EN */
	struct clk *rx_clk;
	struct clk *tx_clk;
	struct clk *raw_clk;
	/* Reset controls */
	struct reset_control *rx_reset;
	struct reset_control *tx_reset;
	struct reset_control *sys_reset;
};

int ipq52xx_phy_probe(struct phy_device *phydev);
void ipq52xx_phy_remove(struct phy_device *phydev);
int ipq52xx_phy_config_init(struct phy_device *phydev);
int ipq52xx_phy_read_status(struct phy_device *phydev);
#endif /* _QCE1204_H_ */
