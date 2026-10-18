/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _QCA81XX_H_
#define _QCA81XX_H_

#include <linux/phy.h>
#if IS_ENABLED(CONFIG_MACSEC)
#include "qca81xx_macsec.h"
#endif

#define QCA8111_PHY		0x004dd1c0
/* in QCOM MDIO bus driver, bit29~31 is for soc type, 2 is for laguna */
/* and bit24~28 is for phy address, 0~23 is for soc address */
#define TO_QCA81XX_PHY_SOC_ADDR(addr, reg)		\
	((BIT(30) | reg) | (addr << 24))

/*SOC TLMM registers*/
#define TLMM_BASE		0x400000
#define TLMM_GPIO_OFFSET	0x1000
#define TO_TLMM_CFG_REG(pin)	(TLMM_BASE + TLMM_GPIO_OFFSET * pin)
#define TLMM_FUNC_MASK		GENMASK(5, 2)

/* GCC registers field */
#define GCC_E2S_SRC_MASK	GENMASK(10, 8)
#define GCC_E2S_SRC0_REF_50MCLK	0
#define GCC_E2S_SRC1_EPHY_TXCLK	1
#define GCC_E2S_SRC2_EPHY_RXCLK	2
#define GCC_E2S_SRC3_SRDS_TXCLK	3
#define GCC_E2S_SRC4_SRDS_RXCLK	4

#define SRC_DIV_MASK		GENMASK(4, 0)
#define CLK_DIV_MASK		GENMASK(3, 0)
#define CLK_CMD_UPDATE		BIT(0)

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
struct qca81xx_private {

#if IS_ENABLED(CONFIG_MACSEC)
	struct qca_macsec_cfg_t macsec_cfg;
#endif
	struct qca81xx_sku_info sku;
#if IS_ENABLED(CONFIG_HWMON)
	struct device *hwmon_dev;
#endif
};

#endif /* _QCA81XX_H_ */
