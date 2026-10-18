/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*qca808x_start*/
#include "sw.h"
#include "ssdk_init.h"
#include "fal_init.h"
#include "fal.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "ssdk_init.h"
/*qca808x_end*/
#include "ssdk_dts.h"
#include "hppe_init.h"
#include <linux/kconfig.h>
/*qca808x_start*/
#include <linux/version.h>
/*qca808x_end*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/types.h>
//#include <asm/mach-types.h>
#include <generated/autoconf.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/mdio-bitbang.h>
#include <net/dsa.h>
#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI)
#include <linux/qcom-fpga-pci.h>
#endif
/*qca808x_start*/
#include "ssdk_plat.h"
#include "hsl_phy.h"
/*qca808x_end*/
#include "ssdk_clk.h"
#include "ref_vlan.h"
#include "ref_fdb.h"
#include "ref_mib.h"
#include "ref_port_ctrl.h"
#include "ref_misc.h"
#include "ref_uci.h"
#include "shell.h"

#include "adpt.h"
#include "ssdk_interrupt.h"

#include "hsl_port_prop.h"
/*qca808x_start*/
#include "hppe_portctrl_reg.h"
#include "hppe_xgportctrl_reg.h"
#include "hppe_reg_access.h"
extern struct qca_phy_priv **qca_phy_priv_global;
/*qca808x_end*/

#ifdef BOARD_IPQ806X
#define PLATFORM_MDIO_BUS_NAME		"mdio-gpio"
#endif
/*qca808x_start*/
#define MDIO_BUS_0					0
#define MDIO_BUS_1					1
/*qca808x_end*/
#define PLATFORM_MDIO_BUS_NUM		MDIO_BUS_0

#define ISIS_CHIP_ID 0x18
#define ISIS_CHIP_REG 0
#define SHIVA_CHIP_ID 0x1f
#define SHIVA_CHIP_REG 0x10
#define HIGH_ADDR_DFLT	0x200

static int ssdk_dev_id = 0;
/*qca808x_start*/
a_uint32_t ssdk_log_level = SSDK_LOG_LEVEL_DEFAULT;
/*qca808x_end*/

sw_error_t qca_mii_bus_lock(a_uint32_t dev_id, a_bool_t enable)
{
	struct mii_bus *miibus = ssdk_miibus_get(dev_id, SSDK_MII_DEFAULT_BUS_ID);

	SW_RTN_ON_NULL(miibus);
	if(enable)
		mutex_lock(&miibus->mdio_lock);
	else
		mutex_unlock(&miibus->mdio_lock);

	return SW_OK;
}

sw_error_t
__qca_mii_reg_get(a_uint32_t dev_id, a_uint32_t reg_addr,
		  a_uint8_t value[], a_uint32_t value_len)
{
	a_uint32_t reg_val = 0;

	if (value_len != sizeof (a_uint32_t))
		return SW_BAD_LEN;

	reg_val = __qca_mii_read(dev_id, reg_addr);

	aos_mem_copy(value, &reg_val, sizeof (a_uint32_t));

	return SW_OK;
}

sw_error_t
__qca_mii_reg_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t value[],
                   a_uint32_t value_len)
{
	a_uint32_t reg_val = 0;

	if (value_len != sizeof (a_uint32_t))
		return SW_BAD_LEN;

	aos_mem_copy(&reg_val, value, sizeof (a_uint32_t));

	__qca_mii_write(dev_id, reg_addr, reg_val);

	return SW_OK;
}

sw_error_t
__qca_mii_field_get(a_uint32_t dev_id, a_uint32_t reg_addr,
                    a_uint32_t bit_offset, a_uint32_t field_len,
                    a_uint8_t value[], a_uint32_t value_len)
{
	a_uint32_t reg_val = 0;

	if ((bit_offset >= 32 || (field_len > 32)) || (field_len == 0))
		return SW_OUT_OF_RANGE;

	if (value_len != sizeof (a_uint32_t))
		return SW_BAD_LEN;

	reg_val = __qca_mii_read(dev_id, reg_addr);

	if(32 == field_len) {
		*((a_uint32_t *) value) = reg_val;
	} else  {
		*((a_uint32_t *) value) = SW_REG_2_FIELD(reg_val, bit_offset, field_len);
	}

	return SW_OK;
}

sw_error_t
__qca_mii_field_set(a_uint32_t dev_id, a_uint32_t reg_addr,
                   a_uint32_t bit_offset, a_uint32_t field_len,
                   const a_uint8_t value[], a_uint32_t value_len)
{
	a_uint32_t reg_val = 0;
	a_uint32_t field_val = *((a_uint32_t *) value);

	if ((bit_offset >= 32 || (field_len > 32)) || (field_len == 0))
		return SW_OUT_OF_RANGE;

	if (value_len != sizeof (a_uint32_t))
		return SW_BAD_LEN;

	reg_val = __qca_mii_read(dev_id, reg_addr);

	if(32 == field_len) {
		reg_val = field_val;
	} else {
		SW_REG_SET_BY_FIELD_U32(reg_val, field_val, bit_offset, field_len);
	}

	__qca_mii_write(dev_id, reg_addr, reg_val);

	return SW_OK;
}

sw_error_t
qca_mii_reg_get(a_uint32_t dev_id, a_uint32_t reg_addr,
                   a_uint8_t value[], a_uint32_t value_len)
{
	sw_error_t rv = SW_OK;

	qca_mii_bus_lock(dev_id, A_TRUE);
	rv = __qca_mii_reg_get(dev_id, reg_addr, value, value_len);
	qca_mii_bus_lock(dev_id, A_FALSE);

	return rv;
}

sw_error_t
qca_mii_reg_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t value[],
                   a_uint32_t value_len)
{
	sw_error_t rv = SW_OK;

	qca_mii_bus_lock(dev_id, A_TRUE);
	rv = __qca_mii_reg_set(dev_id, reg_addr, value, value_len);
	qca_mii_bus_lock(dev_id, A_FALSE);

	return rv;
}

sw_error_t
qca_mii_field_get(a_uint32_t dev_id, a_uint32_t reg_addr,
                    a_uint32_t bit_offset, a_uint32_t field_len,
                    a_uint8_t value[], a_uint32_t value_len)
{
	sw_error_t rv = SW_OK;

	qca_mii_bus_lock(dev_id, A_TRUE);
	rv = __qca_mii_field_get(dev_id, reg_addr, bit_offset, field_len, value, value_len);
	qca_mii_bus_lock(dev_id, A_FALSE);

	return rv;
}

sw_error_t
qca_mii_field_set(a_uint32_t dev_id, a_uint32_t reg_addr,
                   a_uint32_t bit_offset, a_uint32_t field_len,
                   const a_uint8_t value[], a_uint32_t value_len)
{
	sw_error_t rv = SW_OK;

	qca_mii_bus_lock(dev_id, A_TRUE);
	rv = __qca_mii_field_set(dev_id, reg_addr, bit_offset, field_len, value, value_len);
	qca_mii_bus_lock(dev_id, A_FALSE);

	return rv;
}

static a_bool_t qca_mii_reg_accessible(a_uint32_t dev_id, a_uint32_t reg)
{

	ssdk_netdev_switch_t *netdev_switch = ssdk_dts_netdev_switch_find_by_devid(dev_id);
	a_bool_t ret = A_TRUE;

	if (netdev_switch && netdev_switch->switch_erp_standby) {
		switch (reg) {
		case 0x0 ... 0x5F000:
		case 0xC000000 ... 0xC05F000:
			/* switch core registers are un-accessible in standby mode */
			ret = A_FALSE;
			break;
		default:
			break;
		}
	}

	return ret;
}

static sw_error_t qca_mii_reg_convert(a_uint32_t dev_id, a_uint32_t *reg)
{
	ssdk_chip_type chip_type = hsl_get_current_chip_type(dev_id);

	switch (chip_type) {
		case CHIP_ISISC:
			*reg |= SSDK_SWITCH_REG_TYPE_QCA8337;
			break;
		case CHIP_MHT:
			if (!qca_mii_reg_accessible(dev_id, *reg))
				return SW_FAIL;
			*reg |= SSDK_SWITCH_REG_TYPE_QCA8386;
			break;
		case CHIP_HTTPPE:
			*reg |= SSDK_SWITCH_REG_TYPE_QCE2204;
			break;
		default:
			*reg |= SSDK_SWITCH_REG_TYPE_QCA8386;
			break;
	}

	return SW_OK;
}

#if IS_ENABLED(CONFIG_MDIO_BITBANG)
sw_error_t qca_bitbang_mii_raw_read_try(struct mii_bus *bus,
				     a_uint32_t reg, a_uint32_t *val)
{
	struct mdiobb_ctrl *gpio_priv = bus->priv;

	if (strncmp(bus->id, "gpio", strlen("gpio")))
		return SW_NOT_FOUND;

	if (gpio_priv && gpio_priv->sw_read) {
		*val = gpio_priv->sw_read(bus, reg);
		return SW_OK;
	}

	return SW_FAIL;
}

sw_error_t qca_bitbang_mii_raw_write_try(struct mii_bus *bus,
					a_uint32_t reg, a_uint32_t val)
{
	struct mdiobb_ctrl *gpio_priv = bus->priv;

	if (strncmp(bus->id, "gpio", strlen("gpio")))
		return SW_NOT_FOUND;

	if (gpio_priv && gpio_priv->sw_write) {
		gpio_priv->sw_write(bus, reg, val);
		return SW_OK;
	}

	return SW_FAIL;
}

sw_error_t qca_bitbang_mii_raw_update_try(struct mii_bus *bus, a_uint32_t reg,
					  a_uint32_t clear, a_uint32_t set)
{
	struct mdiobb_ctrl *gpio_priv = bus->priv;

	if (strncmp(bus->id, "gpio", strlen("gpio")))
		return SW_NOT_FOUND;

	if (gpio_priv && gpio_priv->sw_read && gpio_priv->sw_write) {
		a_uint32_t val;

		val = gpio_priv->sw_read(bus, reg);
		val &= ~clear;
		val |= set;
		gpio_priv->sw_write(bus, reg, val);

		return SW_OK;
	}

	return SW_FAIL;
}
#endif

/* QCE2204 MDIO access functions - reuse from DSA driver pattern */
static inline void qce2204_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid,
				       u16 *reg_high)
{
	/* bit2 is 1 for writing/reading high byte data[31, 16],
	 * bit2 is 0 for writing/reading low byte data[15, 0].
	 */
	*reg_low = FIELD_GET(GENMASK(3, 0), regaddr);
	*reg_low &= 0xc;
	*reg_low <<= 1;

	*reg_mid = FIELD_GET(GENMASK(19, 4), regaddr);

	*reg_high = FIELD_GET(GENMASK(23, 20), regaddr);
	*reg_high <<= 1;
	*reg_high |= BIT(0);
}

static int _qce2204_ahb_read(struct mii_bus *bus, int addr, u32 reg, u32 *val)
{
	u16 reg_low, reg_mid, reg_high;
	int ret, data;

	qce2204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	/* write ahb address bit4~bit23 */
	__mdiobus_write(bus, addr, reg_high & 0x1f, reg_mid);
	udelay(1);

	/* write ahb address bit0~bit3 and read low 16bit data */
	ret = __mdiobus_read(bus, addr, reg_low);
	if (ret >= 0) {
		data = ret;
		/* write ahb address bit0~bit3 and read high 16 bit data */
		ret = __mdiobus_read(bus, addr, (reg_low | BIT(2)));
		if (ret >= 0)
			*val = data | ret << 16;
	}

	return ret < 0 ? ret : 0;
}

static int _qce2204_ahb_write(struct mii_bus *bus, int addr, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	int ret;

	qce2204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	/* write ahb address bit4~bit23 */
	__mdiobus_write(bus, addr, reg_high & 0x1f, reg_mid);
	udelay(1);

	/* write ahb address bit0~bit3 and write low 16 bit data */
	ret = __mdiobus_write(bus, addr, reg_low, lower_16_bits(val));
	/* write ahb address bit0~bit3 and write high 16 bit data */
	if (!ret)
		ret = __mdiobus_write(bus, addr, (reg_low | BIT(2)),
				      upper_16_bits(val));

	return ret;
}

static int _qce2204_ahb_update(struct mii_bus *bus, int addr, u32 reg,
			      u32 mask, u32 value)
{
	u16 reg_low, reg_mid, reg_high;
	u32 data;
	int ret;

	qce2204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	/* write ahb address bit4~bit23 */
	__mdiobus_write(bus, addr, reg_high & 0x1f, reg_mid);
	udelay(1);

	/* write ahb address bit0~bit3 and read low 16bit data */
	ret = __mdiobus_read(bus, addr, reg_low);
	if (ret >= 0) {
		data = ret;
		/* write ahb address bit0~bit3 and read high 16 bit data */
		ret = __mdiobus_read(bus, addr, (reg_low | BIT(2)));
		if (ret >= 0)
			data |= ret << 16;
	}

	if (ret < 0)
		return ret;

	data &= ~mask;
	data |= value;

	/* write ahb address bit0~bit3 and write low 16 bit data */
	ret = __mdiobus_write(bus, addr, reg_low, lower_16_bits(data));
	/* write ahb address bit0~bit3 and write high 16 bit data */
	if (!ret)
		ret = __mdiobus_write(bus, addr, (reg_low | BIT(2)),
				      upper_16_bits(data));

	return ret < 0 ? ret : 0;
}

sw_error_t qca_mii_raw_read(struct mii_bus *bus, a_uint32_t reg, a_uint32_t *val)
{
	struct qca_mdio_data *mdio_priv = bus->priv;
	int chip_type = FIELD_GET(SSDK_SWITCH_REG_TYPE_MASK, reg);

	if (chip_type == SSDK_SWITCH_REG_IS_QCE2204) {
		int addr = FIELD_GET(SSDK_SWITCH_REG_MDIO_ADDR_MASK, reg);

		return _qce2204_ahb_read(bus, addr, reg, val);
	}

#if IS_ENABLED(CONFIG_MDIO_BITBANG)
	sw_error_t rv = SW_OK;

	rv = qca_bitbang_mii_raw_read_try(bus, reg, val);
	if (rv == SW_OK)
		return rv;
#endif

	if (mdio_priv && mdio_priv->sw_read) {
		*val = mdio_priv->sw_read(bus, reg);
		return SW_OK;
	}

	return SW_FAIL;
}

sw_error_t qca_mii_raw_write(struct mii_bus *bus, a_uint32_t reg, a_uint32_t val)
{
	struct qca_mdio_data *mdio_priv = bus->priv;
	int chip_type = FIELD_GET(SSDK_SWITCH_REG_TYPE_MASK, reg);

	if (chip_type == SSDK_SWITCH_REG_IS_QCE2204) {
		int addr = FIELD_GET(SSDK_SWITCH_REG_MDIO_ADDR_MASK, reg);

		return _qce2204_ahb_write(bus, addr, reg, val);
	}

#if IS_ENABLED(CONFIG_MDIO_BITBANG)
	sw_error_t rv = SW_OK;

	rv = qca_bitbang_mii_raw_write_try(bus, reg, val);
	if (rv == SW_OK)
		return rv;
#endif

	if (mdio_priv && mdio_priv->sw_write) {
		mdio_priv->sw_write(bus, reg, val);
		return SW_OK;
	}

	return SW_FAIL;
}

sw_error_t qca_mii_raw_update(struct mii_bus *bus, a_uint32_t reg,
			      a_uint32_t clear, a_uint32_t set)
{
	struct qca_mdio_data *mdio_priv = bus->priv;
	int chip_type = FIELD_GET(SSDK_SWITCH_REG_TYPE_MASK, reg);

	if (chip_type == SSDK_SWITCH_REG_IS_QCE2204) {
		int addr = FIELD_GET(SSDK_SWITCH_REG_MDIO_ADDR_MASK, reg);

		return _qce2204_ahb_update(bus, addr, reg, clear, set);
	}

#if IS_ENABLED(CONFIG_MDIO_BITBANG)
	sw_error_t rv = SW_OK;

	rv = qca_bitbang_mii_raw_update_try(bus, reg, clear, set);
	if (rv == SW_OK)
		return rv;
#endif

	if (mdio_priv && mdio_priv->sw_read && mdio_priv->sw_write) {
		a_uint32_t val;

		val = mdio_priv->sw_read(bus, reg);
		val &= ~clear;
		val |= set;
		mdio_priv->sw_write(bus, reg, val);

		return SW_OK;
	}

	return SW_FAIL;
}

a_uint32_t __qca_mii_read(a_uint32_t dev_id, a_uint32_t reg)
{
	a_uint32_t val = 0xffffffff;
	struct mii_bus *bus = NULL;

	bus = ssdk_miibus_get(dev_id, SSDK_MII_DEFAULT_BUS_ID);
	if (!bus)
		return val;

	if (qca_mii_reg_convert(dev_id, &reg) == SW_OK)
		qca_mii_raw_read(bus, reg, &val);

	return val;
}

void __qca_mii_write(a_uint32_t dev_id, a_uint32_t reg, a_uint32_t val)
{
	struct mii_bus *bus = NULL;

	bus = ssdk_miibus_get(dev_id, SSDK_MII_DEFAULT_BUS_ID);
	if (!bus)
		return;

	if (qca_mii_reg_convert(dev_id, &reg) == SW_OK)
		qca_mii_raw_write(bus, reg, val);
}

int __qca_mii_update(a_uint32_t dev_id, a_uint32_t reg,
		     a_uint32_t mask, a_uint32_t val)
{
	struct mii_bus *bus = NULL;

	bus = ssdk_miibus_get(dev_id, SSDK_MII_DEFAULT_BUS_ID);
	if (!bus)
		return -1;

	if (qca_mii_reg_convert(dev_id, &reg) == SW_OK)
		qca_mii_raw_update(bus, reg, mask, val);

	return 0;
}

a_uint32_t qca_mii_read(a_uint32_t dev_id, a_uint32_t reg)
{
	a_uint32_t val = 0xffffffff;

	qca_mii_bus_lock(dev_id, A_TRUE);
	val = __qca_mii_read(dev_id, reg);
	qca_mii_bus_lock(dev_id, A_FALSE);

	return val;
}

void qca_mii_write(a_uint32_t dev_id, a_uint32_t reg, a_uint32_t val)
{
	qca_mii_bus_lock(dev_id, A_TRUE);
	__qca_mii_write(dev_id, reg, val);
	qca_mii_bus_lock(dev_id, A_FALSE);
}

int qca_mii_update(a_uint32_t dev_id, a_uint32_t reg, a_uint32_t mask, a_uint32_t val)
{
	qca_mii_bus_lock(dev_id, A_TRUE);
	__qca_mii_update(dev_id, reg, mask, val);
	qca_mii_bus_lock(dev_id, A_FALSE);

	return 0;
}

#if defined(SSDK_PCIE_BUS)
extern u32 ppe_mem_read(u32 reg);
extern void ppe_mem_write(u32 reg, u32 val);
#endif

static sw_error_t
__qca_switch_reg_raw_read(a_uint32_t dev_id, a_uint32_t reg_addr,
			  a_uint8_t * reg_data, a_uint32_t len, a_bool_t lock_en)
{
	hsl_reg_mode reg_mode = ssdk_switch_reg_access_mode_get(dev_id);
	ssdk_reg_map_info map;
	uint32_t reg_val = 0;

	if (len != sizeof(a_uint32_t))
		return SW_BAD_LEN;

	if ((reg_addr % 4) != 0)
		return SW_BAD_PARAM;

	ssdk_switch_reg_map_info_get(dev_id, &map);

	switch (reg_mode) {
	case HSL_REG_PCIE_BUS:
		if (reg_addr >= map.size)
			return SW_OUT_OF_RANGE;

#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI) || defined(SSDK_PCIE_BUS)
		uint32_t pcie_base = ssdk_switch_pcie_base_get(dev_id);
#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI)
		reg_val = qcom_fpga_mem_read(pcie_base + reg_addr);
#elif defined(SSDK_PCIE_BUS)
		reg_val = ppe_mem_read(pcie_base + reg_addr);
#endif
#endif
		break;
	case HSL_REG_MDIO:
		struct mii_bus *bus = ssdk_miibus_get(dev_id, 0);

		if (lock_en)
			qca_mii_bus_lock(dev_id, A_TRUE);

		_qce2204_ahb_read(bus, map.base_addr, reg_addr, &reg_val);

		if (lock_en)
			qca_mii_bus_lock(dev_id, A_FALSE);
		break;
	default:
		if (reg_addr >= map.size)
			return SW_OUT_OF_RANGE;

		reg_val = readl(qca_phy_priv_global[dev_id]->hw_addr + reg_addr);
		break;
	}

	aos_mem_copy(reg_data, &reg_val, sizeof (a_uint32_t));
	return 0;
}

static sw_error_t
__qca_switch_reg_raw_write(a_uint32_t dev_id, a_uint32_t reg_addr,
			   a_uint8_t * reg_data, a_uint32_t len, a_bool_t lock_en)
{
	hsl_reg_mode reg_mode = ssdk_switch_reg_access_mode_get(dev_id);
	ssdk_reg_map_info map;
	uint32_t reg_val = 0;

	if (len != sizeof(a_uint32_t))
		return SW_BAD_LEN;

	if ((reg_addr % 4) != 0)
		return SW_BAD_PARAM;

	ssdk_switch_reg_map_info_get(dev_id, &map);
	aos_mem_copy(&reg_val, reg_data, sizeof (a_uint32_t));

	switch (reg_mode) {
	case HSL_REG_PCIE_BUS:
		if (reg_addr >= map.size)
			return SW_OUT_OF_RANGE;

#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI) || defined(SSDK_PCIE_BUS)
		uint32_t pcie_base = ssdk_switch_pcie_base_get(dev_id);
#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI)
		qcom_fpga_mem_write(pcie_base + reg_addr, reg_val);
#elif defined(SSDK_PCIE_BUS)
		ppe_mem_write(pcie_base + reg_addr, reg_val);
#endif
#endif
		break;
	case HSL_REG_MDIO:
		struct mii_bus *bus = ssdk_miibus_get(dev_id, 0);

		if (lock_en)
			qca_mii_bus_lock(dev_id, A_TRUE);

		_qce2204_ahb_write(bus, map.base_addr, reg_addr, reg_val);

		if (lock_en)
			qca_mii_bus_lock(dev_id, A_FALSE);
		break;
	default:
		if (reg_addr >= map.size)
			return SW_OUT_OF_RANGE;

		writel(reg_val, qca_phy_priv_global[dev_id]->hw_addr + reg_addr);
		break;
	}

	return 0;
}

sw_error_t
qca_switch_reg_write(a_uint32_t dev_id, a_uint32_t reg_addr,
		     a_uint8_t * reg_data, a_uint32_t len)
{
	return __qca_switch_reg_raw_write(dev_id, reg_addr, reg_data, len, A_TRUE);
}

sw_error_t
qca_switch_reg_read(a_uint32_t dev_id, a_uint32_t reg_addr,
		    a_uint8_t * reg_data, a_uint32_t len)
{
	return __qca_switch_reg_raw_read(dev_id, reg_addr, reg_data, len, A_TRUE);
}

sw_error_t
qca_switch_reg_raw_write(a_uint32_t dev_id, a_uint32_t reg_addr,
			 a_uint8_t * reg_data, a_uint32_t len)
{
	return __qca_switch_reg_raw_write(dev_id, reg_addr, reg_data, len, A_FALSE);
}

sw_error_t
qca_switch_reg_raw_read(a_uint32_t dev_id, a_uint32_t reg_addr,
			a_uint8_t * reg_data, a_uint32_t len)
{
	return __qca_switch_reg_raw_read(dev_id, reg_addr, reg_data, len, A_FALSE);
}

sw_error_t
qca_psgmii_reg_read(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t * reg_data, a_uint32_t len)
{
#ifdef DESS
	uint32_t reg_val = 0;

	if (len != sizeof (a_uint32_t))
        return SW_BAD_LEN;

	if((reg_addr%4)!=0)
	return SW_BAD_PARAM;

	if (qca_phy_priv_global[dev_id]->psgmii_hw_addr == NULL)
		return SW_NOT_SUPPORTED;

	reg_val = readl(qca_phy_priv_global[dev_id]->psgmii_hw_addr + reg_addr);

	aos_mem_copy(reg_data, &reg_val, sizeof (a_uint32_t));
#endif
	return 0;
}

sw_error_t
qca_psgmii_reg_write(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t * reg_data, a_uint32_t len)
{
#ifdef DESS
	uint32_t reg_val = 0;
	if (len != sizeof (a_uint32_t))
        return SW_BAD_LEN;

	if((reg_addr%4)!=0)
	return SW_BAD_PARAM;

	if (qca_phy_priv_global[dev_id]->psgmii_hw_addr == NULL)
		return SW_NOT_SUPPORTED;

	aos_mem_copy(&reg_val, reg_data, sizeof (a_uint32_t));
	writel(reg_val, qca_phy_priv_global[dev_id]->psgmii_hw_addr + reg_addr);
#endif
	return 0;
}

static sw_error_t
qca_uniphy_csr_access(a_uint32_t dev_id, void __iomem *hppe_uniphy_base,
		      a_uint32_t reg_addr, a_uint32_t *reg_val, a_bool_t is_read)
{
	a_uint32_t reg_addr1, reg_addr2;

	if (adpt_ppe_type_get(dev_id) == JHPPE_TYPE) {
		/* Extract CSR type and actual address */
		a_uint32_t csr_type = (reg_addr & JHPPE_UNIPHY_CSR_BLOCK_MASK) >> JHPPE_UNIPHY_CSR_BLOCK_SHIFT;
		a_uint32_t actual_addr = reg_addr & JHPPE_UNIPHY_REG_ADDR_MASK;

		/* JHPPE: 3 CSR access types */
		switch (csr_type) {
			case JHPPE_UNIPHY_CSR_TYPE_DIRECT:
				/* Type 0: CSR0 or CSR3 - Direct access */
				if (is_read)
					*reg_val = readl(hppe_uniphy_base + actual_addr);
				else
					writel(*reg_val, hppe_uniphy_base + actual_addr);
				break;

			case JHPPE_UNIPHY_CSR_TYPE_INDIRECT1:
				/* Type 1: CSR1 - Indirect access via 0x43FC */
				reg_addr1 = (actual_addr & 0xffffff) >> 8;
				writel(reg_addr1, hppe_uniphy_base + JHPPE_UNIPHY_CSR1_INDIRECT_REG_ADDR);

				reg_addr2 = actual_addr & HPPE_UNIPHY_INDIRECT_LOW_ADDR;
				reg_addr = (JHPPE_UNIPHY_CSR1_INDIRECT_DATA << 10) | (reg_addr2 << 2);

				if (is_read)
					*reg_val = readl(hppe_uniphy_base + reg_addr);
				else
					writel(*reg_val, hppe_uniphy_base + reg_addr);
				break;

			case JHPPE_UNIPHY_CSR_TYPE_INDIRECT2:
				/* Type 2: CSR2 - Indirect access via 0x83FC */
				reg_addr1 = (actual_addr & 0xffffff) >> 8;
				writel(reg_addr1, hppe_uniphy_base + JHPPE_UNIPHY_CSR2_INDIRECT_REG_ADDR);

				reg_addr2 = actual_addr & HPPE_UNIPHY_INDIRECT_LOW_ADDR;
				reg_addr = (JHPPE_UNIPHY_CSR2_INDIRECT_DATA << 10) | (reg_addr2 << 2);

				if (is_read)
					*reg_val = readl(hppe_uniphy_base + reg_addr);
				else
					writel(*reg_val, hppe_uniphy_base + reg_addr);
				break;

			default:
				return SW_BAD_PARAM;
		}
	} else {
		if (reg_addr > HPPE_UNIPHY_MAX_DIRECT_ACCESS_REG) {
			/* CSR1: Indirect access */
			reg_addr1 = (reg_addr & 0xffffff) >> 8;
			writel(reg_addr1, hppe_uniphy_base + HPPE_UNIPHY_INDIRECT_REG_ADDR);

			reg_addr2 = reg_addr & HPPE_UNIPHY_INDIRECT_LOW_ADDR;
			reg_addr = (HPPE_UNIPHY_INDIRECT_DATA << 10) | (reg_addr2 << 2);

			if (is_read)
				*reg_val = readl(hppe_uniphy_base + reg_addr);
			else
				writel(*reg_val, hppe_uniphy_base + reg_addr);
		} else {
			/* CSR0: Direct access */
			if (is_read)
				*reg_val = readl(hppe_uniphy_base + reg_addr);
			else
				writel(*reg_val, hppe_uniphy_base + reg_addr);
		}
	}

	return SW_OK;
}

sw_error_t
qca_uniphy_reg_read(a_uint32_t dev_id, a_uint32_t uniphy_index,
				a_uint32_t reg_addr, a_uint8_t * reg_data, a_uint32_t len)
{
	uint32_t reg_val = 0;
	void __iomem *hppe_uniphy_base = NULL;
	sw_error_t rv;

	SSDK_DEBUG("uniphy %d reg 0x%x value 0x%x", uniphy_index, reg_addr, (*(a_uint32_t*)reg_data));
	if (len != sizeof (a_uint32_t))
		return SW_BAD_LEN;

	if (qca_phy_priv_global[dev_id]->uniphy_hw_addr == NULL)
		return SW_NO_SUCH;

	if (SSDK_UNIPHY_INSTANCE0 == uniphy_index)
		hppe_uniphy_base = qca_phy_priv_global[dev_id]->uniphy_hw_addr;
	else if (SSDK_UNIPHY_INSTANCE1 == uniphy_index)
		hppe_uniphy_base = qca_phy_priv_global[dev_id]->uniphy_hw_addr + HPPE_UNIPHY_BASE1;
	else if (SSDK_UNIPHY_INSTANCE2 == uniphy_index)
		hppe_uniphy_base = qca_phy_priv_global[dev_id]->uniphy_hw_addr + HPPE_UNIPHY_BASE2;
	else
		return SW_BAD_PARAM;

	rv = qca_uniphy_csr_access(dev_id, hppe_uniphy_base, reg_addr, &reg_val, A_TRUE);
	if (rv == SW_OK)
		aos_mem_copy(reg_data, &reg_val, sizeof(a_uint32_t));

	return rv;
}

sw_error_t
qca_uniphy_reg_write(a_uint32_t dev_id, a_uint32_t uniphy_index,
				a_uint32_t reg_addr, a_uint8_t * reg_data, a_uint32_t len)
{
	void __iomem *hppe_uniphy_base = NULL;
	uint32_t reg_val = 0;

	SSDK_DEBUG("uniphy %d reg 0x%x value 0x%x", uniphy_index, reg_addr, (*(a_uint32_t*)reg_data));
	if (len != sizeof (a_uint32_t))
	        return SW_BAD_LEN;

	if (qca_phy_priv_global[dev_id]->uniphy_hw_addr == NULL)
		return SW_NO_SUCH;

	if (SSDK_UNIPHY_INSTANCE0 == uniphy_index)
		hppe_uniphy_base = qca_phy_priv_global[dev_id]->uniphy_hw_addr;
	else if (SSDK_UNIPHY_INSTANCE1 == uniphy_index)
		hppe_uniphy_base = qca_phy_priv_global[dev_id]->uniphy_hw_addr + HPPE_UNIPHY_BASE1;
	else if (SSDK_UNIPHY_INSTANCE2 == uniphy_index)
		hppe_uniphy_base = qca_phy_priv_global[dev_id]->uniphy_hw_addr + HPPE_UNIPHY_BASE2;
	else
		return SW_BAD_PARAM;

	aos_mem_copy(&reg_val, reg_data, sizeof(a_uint32_t));
	return qca_uniphy_csr_access(dev_id, hppe_uniphy_base, reg_addr, &reg_val, A_FALSE);
}
/*qca808x_start*/
struct mii_bus *ssdk_miibus_get(a_uint32_t dev_id, a_uint32_t index)
{
	return qca_phy_priv_global[dev_id]->miibus[index];
}

a_bool_t ssdk_miibus_is_i2c(a_uint32_t dev_id, a_uint32_t index)
{
	struct mii_bus *mdio_i2c = ssdk_miibus_get(dev_id, index);
	if (mdio_i2c && strcmp(mdio_i2c->name, SFP_I2C_BUS) == 0)
		return A_TRUE;
	else
		return A_FALSE;
}

struct mii_bus *ssdk_phy_miibus_get(a_uint32_t dev_id, a_uint32_t phy_addr_e)
{
	a_uint32_t index = 0;

	index = TO_MIIBUS_INDEX(phy_addr_e);
	if(index >= SSDK_MII_INVALID_BUS_ID)
		return NULL;

	return ssdk_miibus_get(dev_id, index);
}

struct mii_bus *ssdk_port_miibus_get(a_uint32_t dev_id, a_uint32_t port_id)
{
	a_uint32_t phy_addr_e = 0;

	phy_addr_e = qca_ssdk_port_to_phy_addr(dev_id, port_id);

	return ssdk_phy_miibus_get(dev_id, phy_addr_e);
}

sw_error_t ssdk_miibus_add(a_uint32_t dev_id, struct mii_bus *miibus,
	a_uint32_t *index)
{
	a_uint32_t i = 0;

	if(!miibus)
		return SW_BAD_PTR;

	for(i = 0; i < SSDK_MII_BUS_MAX; i++)
	{
		if(qca_phy_priv_global[dev_id]->miibus[i] == miibus ||
			qca_phy_priv_global[dev_id]->miibus[i] == NULL)
		{
			*index = i;
			qca_phy_priv_global[dev_id]->miibus[i] = miibus;
			return SW_OK;
		}
	}

	return SW_OUT_OF_RANGE;
}

a_uint32_t ssdk_miibus_index_get(a_uint32_t dev_id, struct mii_bus *miibus)
{
	a_uint32_t index = 0;

	for(index = 0; index < SSDK_MII_BUS_MAX; index++)
	{
		if(qca_phy_priv_global[dev_id]->miibus[index] == miibus)
			return index;
	}

	return SSDK_MII_INVALID_BUS_ID;
}
/*qca808x_end*/
sw_error_t ssdk_miibus_freq_get(a_uint32_t dev_id, a_uint32_t index,
	a_uint32_t *freq)
{
	struct mii_bus *bus = NULL;
	struct qca_mdio_data *mdio_priv = NULL;

	bus = ssdk_miibus_get(dev_id, index);
	if (!bus) {
		SSDK_ERROR("Can't get MDIO bus of device id %d\n", dev_id);
		return SW_BAD_PTR;
	}

	mdio_priv = bus->priv;
	if (!mdio_priv) {
		SSDK_ERROR("MDIO bus private data is NULL\n");
		return SW_BAD_PTR;
	}

	*freq = mdio_priv->clk_div;
	return SW_OK;
}

sw_error_t ssdk_miibus_freq_set(a_uint32_t dev_id, a_uint32_t index,
	a_uint32_t freq)
{
	struct mii_bus *bus = NULL;
	struct qca_mdio_data *mdio_priv = NULL;

	bus = ssdk_miibus_get(dev_id, index);
	if (!bus) {
		SSDK_ERROR("Can't get MDIO bus of device id %d\n", dev_id);
		return SW_BAD_PTR;
	}

	mdio_priv = bus->priv;
	if (!mdio_priv) {
		SSDK_ERROR("MDIO bus private data is NULL\n");
		return SW_BAD_PTR;
	}

	mdio_priv->clk_div = freq;
	return SW_OK;
}

static ssize_t ssdk_dev_id_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t count;
	a_uint32_t num;

	num = (a_uint32_t)ssdk_dev_id;

	count = scnprintf(buf, (ssize_t)PAGE_SIZE, "%u", num);
	return count;
}

static ssize_t ssdk_dev_id_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char num_buf[12];
	a_uint32_t num;

	if (count >= sizeof(num_buf)) return 0;
	memcpy(num_buf, buf, count);
	num_buf[count] = '\0';
	sscanf(num_buf, "%u", &num);

	ssdk_dev_id = num;

	return count;
}

static ssize_t ssdk_log_level_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t count;
	a_uint32_t num;

	num = ssdk_log_level;

	count = scnprintf(buf, (ssize_t)PAGE_SIZE, "%u", num);
	return count;
}

static ssize_t ssdk_log_level_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char num_buf[12];
	a_uint32_t num;

	if (count >= sizeof(num_buf))
		return 0;
	memcpy(num_buf, buf, count);
	num_buf[count] = '\0';
	sscanf(num_buf, "%u", &num);

	ssdk_log_level = (a_uint32_t)num;

	return count;
}

/* Context structure for counter bin attributes */
struct ssdk_counter_ctx {
	struct mutex lock;
	char *buf;
	ssize_t buf_size;
	a_bool_t show_type;  /* A_FALSE = packet counters, A_TRUE = byte counters */
};

static struct ssdk_counter_ctx ssdk_packet_counter_ctx = {
	.lock      = __MUTEX_INITIALIZER(ssdk_packet_counter_ctx.lock),
	.buf       = NULL,
	.buf_size  = 0,
	.show_type = A_FALSE,
};

static struct ssdk_counter_ctx ssdk_byte_counter_ctx = {
	.lock      = __MUTEX_INITIALIZER(ssdk_byte_counter_ctx.lock),
	.buf       = NULL,
	.buf_size  = 0,
	.show_type = A_TRUE,
};

/* Common read implementation shared by packet_counter and byte_counter */
static ssize_t ssdk_counter_bin_read_common(struct ssdk_counter_ctx *ctx,
					    char *buf, loff_t off, size_t count)
{
	mutex_lock(&ctx->lock);

	/* If first read (offset = 0), regenerate data */
	if (off == 0) {
		char *temp_buf = NULL;
		adpt_api_t *p_api;
		ssize_t temp_count = 0;
		sw_error_t rv;

		/* Snapshot dev_id once inside the lock to avoid TOCTOU race
		 * with ssdk_dev_id_set: adpt_api_ptr_get and adpt_debug_counter_get
		 * must use the same dev_id value.
		 */
		a_uint32_t dev_id = READ_ONCE(ssdk_dev_id);

		/* Normally ctx->buf is freed at the end of each read sequence
		 * (after the last chunk is consumed or at EOF). However, if the
		 * previous read was aborted mid-way (e.g. user-space closed the fd
		 * before reading all data), the buffer may still be allocated here.
		 * Free it now to avoid a memory leak before allocating a fresh one.
		 */
		if (ctx->buf) {
			kfree(ctx->buf);
			ctx->buf = NULL;
			ctx->buf_size = 0;
		}

		/* Allocate large buffer */
		temp_buf = kmalloc(SSDK_COUNTER_BUF_SIZE, GFP_KERNEL);
		if (!temp_buf) {
			mutex_unlock(&ctx->lock);
			return -ENOMEM;
		}

		p_api = adpt_api_ptr_get(dev_id);
		if (p_api == NULL || p_api->adpt_debug_counter_get == NULL) {
			temp_count = scnprintf(temp_buf, SSDK_COUNTER_BUF_SIZE, "Unsupported\n");
		} else {
			rv = p_api->adpt_debug_counter_get(dev_id, ctx->show_type,
							   &temp_buf, &temp_count);
			if (rv != SW_OK) {
				kfree(temp_buf);
				mutex_unlock(&ctx->lock);
				return -EIO;
			}
		}

		/* Save to context buffer */
		ctx->buf = temp_buf;
		ctx->buf_size = temp_count;
	}

	/* Check if offset is valid - also free buffer here (EOF signal or empty data) */
	if (off >= ctx->buf_size) {
		kfree(ctx->buf);
		ctx->buf = NULL;
		ctx->buf_size = 0;
		mutex_unlock(&ctx->lock);
		return 0;
	}

	/* Calculate actual bytes to read */
	if (off + count > ctx->buf_size)
		count = ctx->buf_size - off;

	/* Copy data to user buffer */
	memcpy(buf, ctx->buf + off, count);

	/* Free buffer after last chunk is read to release memory promptly */
	if (off + count >= ctx->buf_size) {
		kfree(ctx->buf);
		ctx->buf = NULL;
		ctx->buf_size = 0;
	}

	mutex_unlock(&ctx->lock);
	return count;
}

/* Common write implementation shared by packet_counter and byte_counter (clear counters) */
static ssize_t ssdk_counter_bin_write_common(struct ssdk_counter_ctx *ctx, size_t count)
{
	adpt_api_t *p_api;
	/* Snapshot dev_id once to avoid TOCTOU race with ssdk_dev_id_set */
	a_uint32_t dev_id = READ_ONCE(ssdk_dev_id);

	mutex_lock(&ctx->lock);

	p_api = adpt_api_ptr_get(dev_id);
	if (p_api && p_api->adpt_debug_counter_set)
		p_api->adpt_debug_counter_set(dev_id);
	else
		SSDK_WARN("Unsupported\n");

	/* Clear cached buffer */
	if (ctx->buf) {
		kfree(ctx->buf);
		ctx->buf = NULL;
		ctx->buf_size = 0;
	}
	mutex_unlock(&ctx->lock);

	return count;
}

/* bin_attribute read function for packet_counter */
static ssize_t ssdk_packet_counter_bin_read(struct file *filp,
					     struct kobject *kobj,
					     struct bin_attribute *attr,
					     char *buf, loff_t off, size_t count)
{
	return ssdk_counter_bin_read_common(&ssdk_packet_counter_ctx, buf, off, count);
}

/* bin_attribute write function for packet_counter (clear counters) */
static ssize_t ssdk_packet_counter_bin_write(struct file *filp,
					      struct kobject *kobj,
					      struct bin_attribute *attr,
					      char *buf, loff_t off, size_t count)
{
	return ssdk_counter_bin_write_common(&ssdk_packet_counter_ctx, count);
}

/* bin_attribute read function for byte_counter */
static ssize_t ssdk_byte_counter_bin_read(struct file *filp,
					   struct kobject *kobj,
					   struct bin_attribute *attr,
					   char *buf, loff_t off, size_t count)
{
	return ssdk_counter_bin_read_common(&ssdk_byte_counter_ctx, buf, off, count);
}

/* bin_attribute write function for byte_counter (clear counters) */
static ssize_t ssdk_byte_counter_bin_write(struct file *filp,
					    struct kobject *kobj,
					    struct bin_attribute *attr,
					    char *buf, loff_t off, size_t count)
{
	return ssdk_counter_bin_write_common(&ssdk_byte_counter_ctx, count);
}

#ifdef IN_QOS
void ssdk_dts_port_scheduler_dump(a_uint32_t dev_id)
{
	a_uint32_t i;
	ssdk_dt_portscheduler_cfg *portscheduler_cfg;
	ssdk_dt_scheduler_cfg *scheduler_cfg;
	a_uint8_t srcmsg[7][16];

	scheduler_cfg = ssdk_bootup_shceduler_cfg_get(dev_id);

	printk("===============================port_scheduler_resource===========================\n");
	printk("portid     ucastq     mcastq     10sp     10cdrr     10edrr     11cdrr     11edrr\n");
	for (i = 0; i < SSDK_MAX_PORT_NUM; i++)
	{
		portscheduler_cfg = &scheduler_cfg->pool[i];
		scnprintf(srcmsg[0], sizeof(srcmsg[0]), "<%d %d>", portscheduler_cfg->ucastq_start,
				portscheduler_cfg->ucastq_end);
		scnprintf(srcmsg[1], sizeof(srcmsg[1]), "<%d %d>", portscheduler_cfg->mcastq_start,
				portscheduler_cfg->mcastq_end);
		scnprintf(srcmsg[2], sizeof(srcmsg[2]), "<%d %d>", portscheduler_cfg->l0sp_start,
				portscheduler_cfg->l0sp_end);
		scnprintf(srcmsg[3], sizeof(srcmsg[3]), "<%d %d>", portscheduler_cfg->l0cdrr_start,
				portscheduler_cfg->l0cdrr_end);
		scnprintf(srcmsg[4], sizeof(srcmsg[4]), "<%d %d>", portscheduler_cfg->l0edrr_start,
				portscheduler_cfg->l0edrr_end);
		scnprintf(srcmsg[5], sizeof(srcmsg[5]), "<%d %d>", portscheduler_cfg->l1cdrr_start,
				portscheduler_cfg->l1cdrr_end);
		scnprintf(srcmsg[6], sizeof(srcmsg[6]), "<%d %d>", portscheduler_cfg->l1edrr_start,
				portscheduler_cfg->l1edrr_end);
		printk("%6d%11s%11s%9s%11s%11s%11s%11s\n", i, srcmsg[0], srcmsg[1], srcmsg[2], srcmsg[3],
				srcmsg[4], srcmsg[5], srcmsg[6]);
	}
}

void ssdk_dts_reserved_scheduler_dump(a_uint32_t dev_id)
{
	ssdk_dt_portscheduler_cfg *reserved_cfg;
	ssdk_dt_scheduler_cfg *scheduler_cfg;
	a_uint8_t srcmsg[7][16];

	scheduler_cfg = ssdk_bootup_shceduler_cfg_get(dev_id);
	if (!scheduler_cfg) {
		return;
	}

	reserved_cfg = &scheduler_cfg->reserved_pool;

	printk("=============================reserved_scheduler_resource========================="
			"\n");
	printk("reserved   ucastq     mcastq     10sp     10cdrr     10edrr     11cdrr     11edrr"
			"\n");
	scnprintf(srcmsg[0], sizeof(srcmsg[0]), "<%d %d>", reserved_cfg->ucastq_start,
			reserved_cfg->ucastq_end);
	scnprintf(srcmsg[1], sizeof(srcmsg[1]), "<%d %d>", reserved_cfg->mcastq_start,
			reserved_cfg->mcastq_end);
	scnprintf(srcmsg[2], sizeof(srcmsg[2]), "<%d %d>", reserved_cfg->l0sp_start,
			reserved_cfg->l0sp_end);
	scnprintf(srcmsg[3], sizeof(srcmsg[3]), "<%d %d>", reserved_cfg->l0cdrr_start,
			reserved_cfg->l0cdrr_end);
	scnprintf(srcmsg[4], sizeof(srcmsg[4]), "<%d %d>", reserved_cfg->l0edrr_start,
			reserved_cfg->l0edrr_end);
	scnprintf(srcmsg[5], sizeof(srcmsg[5]), "<%d %d>", reserved_cfg->l1cdrr_start,
			reserved_cfg->l1cdrr_end);
	scnprintf(srcmsg[6], sizeof(srcmsg[6]), "<%d %d>", reserved_cfg->l1edrr_start,
			reserved_cfg->l1edrr_end);
	printk("      %11s%11s%9s%11s%11s%11s%11s\n", srcmsg[0], srcmsg[1], srcmsg[2], srcmsg[3],
			srcmsg[4], srcmsg[5], srcmsg[6]);
}

void ssdk_dts_l0scheduler_dump(a_uint32_t dev_id)
{
	a_uint32_t i;
	ssdk_dt_l0scheduler_cfg *scheduler_cfg;
	ssdk_dt_scheduler_cfg *cfg;

	cfg = ssdk_bootup_shceduler_cfg_get(dev_id);
	printk("==========================l0scheduler_cfg===========================\n");
	printk("queue     portid     cpri     cdrr_id     epri     edrr_id     sp_id\n");
	for (i = 0; i < SSDK_L0SCHEDULER_CFG_MAX; i++)
	{
		scheduler_cfg = &cfg->l0cfg[i];
		if (scheduler_cfg->valid == 1)
			printk("%5d%11d%9d%12d%9d%12d%10d\n", i, scheduler_cfg->port_id,
				scheduler_cfg->cpri, scheduler_cfg->cdrr_id, scheduler_cfg->epri,
				scheduler_cfg->edrr_id, scheduler_cfg->sp_id);
	}
}

void ssdk_dts_l1scheduler_dump(a_uint32_t dev_id)
{
	a_uint32_t i;
	ssdk_dt_l1scheduler_cfg *scheduler_cfg;
	ssdk_dt_scheduler_cfg *cfg;

	cfg = ssdk_bootup_shceduler_cfg_get(dev_id);

	printk("=====================l1scheduler_cfg===============================\n");
	printk("flow     portid     cpri     cdrr_id     epri     edrr_id     sp_id\n");
	for (i = 0; i < SSDK_L1SCHEDULER_CFG_MAX; i++)
	{
		scheduler_cfg = &cfg->l1cfg[i];
		if (scheduler_cfg->valid == 1)
			printk("%4d%11d%9d%12d%9d%12d%10d\n", i, scheduler_cfg->port_id,
				scheduler_cfg->cpri, scheduler_cfg->cdrr_id,
				scheduler_cfg->epri, scheduler_cfg->edrr_id,
				scheduler_cfg->sp_id);
	}
}
#endif
static const a_int8_t *qca_phy_feature_str[QCA_PHY_FEATURE_MAX] = {
	"PHY_CLAUSE45",
	"PHY_COMBO",
	"PHY_QGMAC",
	"PHY_XGMAC",
	"PHY_I2C",
	"PHY_INIT",
	"PHY_FORCE",
	"PHY_SFP",
	"PHY_SFP_SGMII",
};

void ssdk_dts_phyinfo_dump(a_uint32_t dev_id)
{
	a_uint32_t i, j;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	printk("=====================port phyinfo========================\n");
	printk("portid     phy_addr     features\n");

	if(!phy_info)
		return;
	for (i = 0; i <= SSDK_MAX_PORT_NUM; i++) {
		if (A_TRUE == hsl_port_prop_check(dev_id, i, HSL_PP_PHY)) {
			printk("%6d%13d%*s", i,
					TO_PHY_ADDR(phy_info->phy_address[i]), 5, "");
			for (j = 0; j < QCA_PHY_FEATURE_MAX; j++) {
				if (phy_info->phy_features[i] & BIT(j) && BIT(j) != PHY_F_INIT) {
					printk(KERN_CONT "%s ", qca_phy_feature_str[j]);
					if (BIT(j) == PHY_F_FORCE) {
						printk(KERN_CONT "(speed: %d, duplex: %s) ",
								phy_info->port_force_speed[i],
								phy_info->port_force_duplex[i] > 0 ?
								"full" : "half");
					}
				}
			}
			printk(KERN_CONT "\n");
		}
	}
}

static ssize_t ssdk_dts_dump(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t count;
	a_uint32_t dev_id, dev_num;
	ssdk_reg_map_info map;
	hsl_reg_mode mode;

	count = scnprintf(buf, (ssize_t)PAGE_SIZE, "\n");

	dev_num = ssdk_switch_device_num_get();
	for (dev_id = 0; dev_id < dev_num; dev_id ++)
	{
		ssdk_switch_reg_map_info_get(dev_id, &map);
		mode = ssdk_switch_reg_access_mode_get(dev_id);
		printk("=======================================================\n");
		printk("ess-switch\n");
		printk("        reg = <0x%x 0x%x>\n", map.base_addr, map.size);
		if (mode == HSL_REG_LOCAL_BUS)
			printk("        switch_access_mode = <local bus>\n");
		else if (mode == HSL_REG_MDIO)
			printk("        switch_access_mode = <mdio bus>\n");
		else if (mode == HSL_REG_PCIE_BUS)
			printk("        switch_access_mode = <pcie bus>\n");
		else
			printk("        switch_access_mode = <(null)>\n");
		printk("        switch_cpu_bmp = <0x%x>\n", ssdk_cpu_bmp_get(dev_id));
		printk("        switch_lan_bmp = <0x%x>\n", ssdk_lan_bmp_get(dev_id));
		printk("        switch_wan_bmp = <0x%x>\n", ssdk_wan_bmp_get(dev_id));
		printk("        switch_inner_bmp = <0x%x>\n", ssdk_inner_bmp_get(dev_id));
		printk("        switch_mac_mode = <0x%x>\n", ssdk_dt_global_get_mac_mode(dev_id, 0));
		printk("        switch_mac_mode1 = <0x%x>\n", ssdk_dt_global_get_mac_mode(dev_id, 1));
		printk("        switch_mac_mode2 = <0x%x>\n", ssdk_dt_global_get_mac_mode(dev_id, 2));
#ifdef IN_BM
		printk("        bm_tick_mode = <0x%x>\n", ssdk_bm_tick_mode_get(dev_id));
#endif
#ifdef IN_QOS
		printk("        tm_tick_mode = <0x%x>\n", ssdk_tm_tick_mode_get(dev_id));
#endif
#ifdef IN_UNIPHY
		printk("ess-uniphy\n");
		ssdk_uniphy_reg_map_info_get(dev_id, &map);
		mode = ssdk_uniphy_reg_access_mode_get(dev_id);
		printk("        reg = <0x%x 0x%x>\n", map.base_addr, map.size);
		if (mode == HSL_REG_LOCAL_BUS)
			printk("        uniphy_access_mode = <local bus>\n");
		else if (mode == HSL_REG_MDIO)
			printk("        uniphy_access_mode = <mdio bus>\n");
		else
			printk("        uniphy_access_mode = <(null)>\n");
#endif
#ifdef IN_QOS
		printk("\n");
		ssdk_dts_port_scheduler_dump(dev_id);
		printk("\n");
		ssdk_dts_reserved_scheduler_dump(dev_id);
		printk("\n");
		ssdk_dts_l0scheduler_dump(dev_id);
		printk("\n");
		ssdk_dts_l1scheduler_dump(dev_id);
#endif
		printk("\n");
		ssdk_dts_phyinfo_dump(dev_id);
	}

	return count;
}

static a_uint16_t phy_reg_val = 0;
static ssize_t ssdk_phy_write_reg_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char phy_buf[32];
	char *this_opt;
	char *options = phy_buf;
	unsigned int phy_addr, reg_addr, reg_value;
	int ret;

	if (count >= sizeof(phy_buf))
		return 0;
	memcpy(phy_buf, buf, count);
	phy_buf[count] = '\0';

	this_opt = strsep(&options, " ");
	if (!this_opt)
		goto fail;

	ret = kstrtouint(this_opt, 0, &phy_addr);
	if (ret)
		goto fail;

	if ((options - phy_buf) >= (count - 1))
		goto fail;

	this_opt = strsep(&options, " ");
	if (!this_opt)
		goto fail;

	ret = kstrtouint(this_opt, 0, &reg_addr);
	if (ret)
		goto fail;

	if ((options - phy_buf) >= (count - 1))
		goto fail;

	this_opt = strsep(&options, " ");
	if (!this_opt)
		goto fail;

	ret = kstrtouint(this_opt, 0, &reg_value);
	if (ret)
		goto fail;

	hsl_phy_mii_reg_write(0, phy_addr, reg_addr, reg_value);

	return count;

fail:
	printk("Format: phy_addr reg_addr reg_value\n");
	return -EINVAL;
}

static ssize_t ssdk_phy_read_reg_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t count;

	count = scnprintf(buf, (ssize_t)PAGE_SIZE, "reg_val = 0x%x\n", phy_reg_val);
	return count;
}

static ssize_t ssdk_phy_read_reg_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char phy_buf[32];
	char *this_opt;
	char *options = phy_buf;
	unsigned int phy_addr, reg_addr;
	int ret;

	if (count >= sizeof(phy_buf))
		return 0;
	memcpy(phy_buf, buf, count);
	phy_buf[count] = '\0';

	this_opt = strsep(&options, " ");
	if (!this_opt)
		goto fail;

	ret = kstrtouint(this_opt, 0, &phy_addr);
	if (ret)
		goto fail;

	if ((options - phy_buf) >= (count - 1))
		goto fail;

	this_opt = strsep(&options, " ");
	if (!this_opt)
		goto fail;

	ret = kstrtouint(this_opt, 0, &reg_addr);
	if (ret)
		goto fail;

	phy_reg_val = hsl_phy_mii_reg_read(0, phy_addr, reg_addr);

	return count;

fail:
	printk("Format: phy_addr reg_addr\n");
	return -EINVAL;
}

static a_uint32_t ssdk_netdev_to_portid(a_uint32_t dev_id, struct net_device *dev)
{
	a_uint32_t mac_reg = 0, port_id = 0;

	mac_reg = dev->base_addr & 0xffffff;
	if (mac_reg < NSS_MAC_CSR_BASE_ADDR)
		return SW_MAX_NR_PORT;
	/*xgmac*/
	if (mac_reg >= NSS_XGMAC_CSR_BASE_ADDR) {
		a_uint32_t xgmac_id = (mac_reg - NSS_XGMAC_CSR_BASE_ADDR)/MAC_TX_CONFIGURATION_INC;
		port_id = ppe_xgmac_id_to_port(dev_id, xgmac_id);
	} else {
		port_id = (mac_reg - NSS_MAC_CSR_BASE_ADDR)/MAC_ENABLE_INC + 1;
	}

	return port_id;
}

sw_error_t ssdk_netdev_switch_init(struct net_device *dev)
{
	ssdk_netdev_switch_t *netdev_switch = NULL;
	a_uint32_t port_id, dev_id = 0;
	phy_info_t *phyinfo = NULL;

	/* netdev info for PPE port */
	port_id = ssdk_netdev_to_portid(dev_id, dev);
	if (port_id >= SW_MAX_NR_PORT)
		return SW_BAD_VALUE;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_EXCL_CPU))
		return SW_BAD_VALUE;
	phyinfo = hsl_phy_info_get(dev_id);
	SW_RTN_ON_NULL(phyinfo);
	phyinfo->netdev[port_id] = dev;

	/* netdev switch info for PPE port which connected with switch */
	netdev_switch = ssdk_dts_netdev_switch_find(port_id);
	if(!netdev_switch)
		return SW_NOT_FOUND;

	netdev_switch->dev = dev;

	return SW_OK;
}

/* Set switch ERP standby status if required */
void ssdk_switch_set_standby_status(a_uint32_t dev_id, bool enable)
{
	ssdk_netdev_switch_t *netdev_switch = NULL;
	struct mii_bus *bus = NULL;

	netdev_switch = ssdk_dts_netdev_switch_find_by_devid(dev_id);
	if (!netdev_switch)
		return;

	bus = ssdk_miibus_get(dev_id, SSDK_MII_DEFAULT_BUS_ID);
	if (!bus)
		return;

	/* Set ssdk switch erp standby status */
	mutex_lock(&bus->mdio_lock);
	netdev_switch->switch_erp_standby = enable;
	mutex_unlock(&bus->mdio_lock);

#if IS_ENABLED(CONFIG_NET_DSA)
	/* If DSA is enabled, notify to set DSA switch standby status */
	if (netdev_switch->dev && netdev_uses_dsa(netdev_switch->dev)) {
		struct dsa_switch *ds= netdev_switch->dev->dsa_ptr->ds;
		if (!ds || !ds->ops)
			return;

		if (enable && ds->ops->suspend)
			ds->ops->suspend(ds);
		if (!enable && ds->ops->resume)
			ds->ops->resume(ds);
	}
#endif
}

#if IS_ENABLED(CONFIG_NET_DSA)
a_bool_t ssdk_switch_enable_dsa(a_uint32_t dev_id)
{
	ssdk_netdev_switch_t *netdev_switch = ssdk_dts_netdev_switch_find_by_devid(dev_id);
	if (netdev_switch && netdev_switch->dev && netdev_uses_dsa(netdev_switch->dev))
		return A_TRUE;

	return A_FALSE;
}
#endif

#ifdef ISISC
static ssize_t ssdk_eth_switch_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	a_uint32_t index = 0, len = 0, dev_id = 0;
	ssdk_netdev_switch_t *netdev_switch = NULL;
	a_bool_t enable = A_FALSE;
	a_ulong_t port = 0, switch_pbmp = 0;

	len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
		"{\n \"switches\":\n [\n");
	for (index = 0; index < SSDK_NETDEV_SWITCH_NUM; index++) {
		netdev_switch = ssdk_dts_netdev_switch_get(index);
		if(netdev_switch->switch_connected)
		{
			dev_id = netdev_switch->switch_dev_id;
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"   {\n"
				"     \"name\":\"%s\",\n" \
				"     \"switch_connected\":\"%s\",\n" \
				"     \"switch_device_id\":\"%d\",\n" \
				"     \"switch_cpu_port\":\"%d\",\n",
				netdev_switch->dev->name,
				netdev_switch->switch_connected ? "yes": "no",
				dev_id,
				netdev_switch->switch_cpu_port
			);
			fal_portvlan_member_get(dev_id, netdev_switch->switch_cpu_port,
				(fal_pbmp_t*)&netdev_switch->switch_port_bmp);
			switch_pbmp = netdev_switch->switch_port_bmp;
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"     \"switch_ports\":\"");
			for_each_set_bit(port, &switch_pbmp, SSDK_MAX_PORT_NUM) {
				len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"%ld,", port);
			}
			len += snprintf(buf + len - 1, (ssize_t)(PAGE_SIZE - len + 1),
				"\",\n     \"switch_vlan_id\":\"");
			for_each_set_bit(port, &switch_pbmp, SSDK_MAX_PORT_NUM) {
				fal_port_default_cvid_get(dev_id, port,
					&netdev_switch->switch_port_vid[port]);
				len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len), "%d,",
					netdev_switch->switch_port_vid[port]);
			}
			fal_header_type_get(dev_id, &enable, &netdev_switch->switch_athtag);
			len += snprintf(buf + len - 1, (ssize_t)(PAGE_SIZE - len + 1),
				"\",\n     \"switch_athtag\":\"0x%x\"\n   },\n",
				netdev_switch->switch_athtag);
			if (len >= PAGE_SIZE)
				break;
		}
	}
	len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len), " ]\n}\n");

	return len;
}
#endif
static ssize_t ssdk_mac_polling_set(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char num_buf[12];
	a_uint32_t num;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(ssdk_dev_id);
	if (!priv)
		return count;

	if (count >= sizeof(num_buf))
		return 0;
	memcpy(num_buf, buf, count);
	num_buf[count] = '\0';
	sscanf(num_buf, "%u", &num);

	if (num > 0) {
		if (priv->link_polling_required) {
			ssdk_mac_sw_sync_work_start(ssdk_dev_id);
			qm_err_check_work_start(priv);
		} else {
			qca_phy_enable_intr(priv);
			qca_switch_enable_intr(priv, FAL_SWITCH_INTR_LINK_STATUS);
		}
	} else {
		if (priv->link_polling_required) {
			ssdk_mac_sw_sync_work_stop(ssdk_dev_id);
			qm_err_check_work_stop(priv);
		} else {
			qca_phy_disable_intr(priv);
			qca_switch_disable_intr(priv, FAL_SWITCH_INTR_LINK_STATUS);
		}
	}

	return count;
}

static ssize_t ssdk_module_debug_stats_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	a_uint32_t len = 0;
	a_uint32_t dev_id, dev_num;

	dev_num = ssdk_switch_device_num_get();
	for (dev_id = 0; dev_id < dev_num; dev_id ++) {
		struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
		if (!priv)
			return 0;

		len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"Device Id                        : %d\n",priv->device_id);
		if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_INIT_START)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK INIT START              : Success\n");
		if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_DTS_PARSE_FAILURE)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK DTS PARSE               : Failure\n");
		else if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_PLAT_INIT_FAILURE)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK PLAT INIT               : Failure\n");
		else if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_CHIP_VER_GET_FAILURE)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK CHIP VER GET            : Failure\n");
		else if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_HW_INIT_FAILURE)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK HW INIT                 : Failure\n");
		else if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_SWITCH_REGISTER_FAILURE)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK SWITCH REGISTER         : Failure\n");
		if (priv->ssdk_module_cnt.ssdk_init_state == SSDK_INIT_SUCCESS)
			len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK INIT                    : Success\n");
		len += snprintf(buf + len, (ssize_t)(PAGE_SIZE - len),
				"    SSDK MAC Polling Start Count : %d\n" \
				"    SSDK MAC Polling Stop Count  : %d\n",
				priv->ssdk_module_cnt.polling_start_cnt,
				priv->ssdk_module_cnt.polling_stop_cnt);
	}

	return len;
}

static const struct device_attribute ssdk_dev_id_attr =
	__ATTR(dev_id, 0660, ssdk_dev_id_get, ssdk_dev_id_set);
static const struct device_attribute ssdk_log_level_attr =
	__ATTR(log_level, 0660, ssdk_log_level_get, ssdk_log_level_set);
/* New bin_attribute definitions */
static struct bin_attribute ssdk_packet_counter_bin_attr = {
	.attr = { .name = "packet_counter", .mode = 0660 },
	.size = 0,  /* 0 = dynamic size */
	.read = ssdk_packet_counter_bin_read,
	.write = ssdk_packet_counter_bin_write,
};

static struct bin_attribute ssdk_byte_counter_bin_attr = {
	.attr = { .name = "byte_counter", .mode = 0660 },
	.size = 0,  /* 0 = dynamic size */
	.read = ssdk_byte_counter_bin_read,
	.write = ssdk_byte_counter_bin_write,
};
static const struct device_attribute ssdk_dts_dump_attr =
	__ATTR(dts_dump, 0660, ssdk_dts_dump, NULL);
static const struct device_attribute ssdk_phy_write_reg_attr =
	__ATTR(phy_write_reg, 0660, NULL, ssdk_phy_write_reg_set);
static const struct device_attribute ssdk_phy_read_reg_attr =
	__ATTR(phy_read_reg, 0660, ssdk_phy_read_reg_get, ssdk_phy_read_reg_set);
#ifdef ISISC
static const struct device_attribute ssdk_eth_switch_attr =
	__ATTR(eth_switch, 0660, ssdk_eth_switch_get, NULL);
#endif
static const struct device_attribute ssdk_mac_polling_attr =
	__ATTR(mac_polling, 0660, NULL, ssdk_mac_polling_set);
static const struct device_attribute ssdk_module_debug_stats_attr =
	__ATTR(module_state, 0660, ssdk_module_debug_stats_get, NULL);

struct kobject *ssdk_sys = NULL;

int ssdk_sysfs_init (void)
{
	int ret = 0;

	/* create /sys/ssdk/ dir */
	ssdk_sys = kobject_create_and_add("ssdk", NULL);
	if (!ssdk_sys) {
		printk("Failed to register SSDK sysfs\n");
		return ret;
	}

	/* create /sys/ssdk/dev_id file */
	ret = sysfs_create_file(ssdk_sys, &ssdk_dev_id_attr.attr);
	if (ret) {
		printk("Failed to register SSDK dev id SysFS file\n");
		goto CLEANUP_1;
	}

	/* create /sys/ssdk/log_level file */
	ret = sysfs_create_file(ssdk_sys, &ssdk_log_level_attr.attr);
	if (ret) {
		printk("Failed to register SSDK log level SysFS file\n");
		goto CLEANUP_2;
	}

	/* create /sys/ssdk/packet_counter file */
	ret = sysfs_create_bin_file(ssdk_sys, &ssdk_packet_counter_bin_attr);
	if (ret) {
		printk("Failed to register SSDK switch counter SysFS file\n");
		goto CLEANUP_3;
	}

	/* create /sys/ssdk/byte_counter file */
	ret = sysfs_create_bin_file(ssdk_sys, &ssdk_byte_counter_bin_attr);
	if (ret) {
		printk("Failed to register SSDK switch counter bytes SysFS file\n");
		goto CLEANUP_4;
	}

	/* create /sys/ssdk/dts_dump file */
	ret = sysfs_create_file(ssdk_sys, &ssdk_dts_dump_attr.attr);
	if (ret) {
		printk("Failed to register SSDK switch show dts SysFS file\n");
		goto CLEANUP_5;
	}

	/* create /sys/ssdk/phy_write_reg file */
	ret = sysfs_create_file(ssdk_sys, &ssdk_phy_write_reg_attr.attr);
	if (ret) {
		printk("Failed to register SSDK phy write reg file\n");
		goto CLEANUP_6;
	}

	/* create /sys/ssdk/phy_read_reg file */
	ret = sysfs_create_file(ssdk_sys, &ssdk_phy_read_reg_attr.attr);
	if (ret) {
		printk("Failed to register SSDK phy read reg file\n");
		goto CLEANUP_7;
	}
#ifdef ISISC
	/* create /sys/ssdk/switch_external*/
	ret = sysfs_create_file(ssdk_sys, &ssdk_eth_switch_attr.attr);
	if (ret) {
		printk("Failed to register switch_external file\n");
		goto CLEANUP_8;
	}
#endif
	/* create /sys/ssdk/mac_polling */
	ret = sysfs_create_file(ssdk_sys, &ssdk_mac_polling_attr.attr);
	if (ret) {
		printk("Failed to register SSDK mac polling SysFS file\n");
		goto CLEANUP_9;
	}

	/* create /sys/ssdk/ssdk_module_cnt_stats */
	ret = sysfs_create_file(ssdk_sys, &ssdk_module_debug_stats_attr.attr);
	if (ret) {
		printk("Failed to register SSDK module debug SysFS file\n");
		goto CLEANUP_10;
	}

	return 0;

CLEANUP_10:
	sysfs_remove_file(ssdk_sys, &ssdk_mac_polling_attr.attr);
CLEANUP_9:
#ifdef ISISC
	sysfs_remove_file(ssdk_sys, &ssdk_eth_switch_attr.attr);
CLEANUP_8:
#endif
	sysfs_remove_file(ssdk_sys, &ssdk_phy_read_reg_attr.attr);
CLEANUP_7:
	sysfs_remove_file(ssdk_sys, &ssdk_phy_write_reg_attr.attr);
CLEANUP_6:
	sysfs_remove_file(ssdk_sys, &ssdk_dts_dump_attr.attr);
CLEANUP_5:
	sysfs_remove_bin_file(ssdk_sys, &ssdk_byte_counter_bin_attr);
CLEANUP_4:
	sysfs_remove_bin_file(ssdk_sys, &ssdk_packet_counter_bin_attr);
CLEANUP_3:
	sysfs_remove_file(ssdk_sys, &ssdk_log_level_attr.attr);
CLEANUP_2:
	sysfs_remove_file(ssdk_sys, &ssdk_dev_id_attr.attr);
CLEANUP_1:
	kobject_put(ssdk_sys);

	return ret;
}

void ssdk_sysfs_exit (void)
{
	sysfs_remove_file(ssdk_sys, &ssdk_phy_read_reg_attr.attr);
	sysfs_remove_file(ssdk_sys, &ssdk_phy_write_reg_attr.attr);
	sysfs_remove_file(ssdk_sys, &ssdk_dts_dump_attr.attr);
	sysfs_remove_bin_file(ssdk_sys, &ssdk_byte_counter_bin_attr);
	sysfs_remove_bin_file(ssdk_sys, &ssdk_packet_counter_bin_attr);
	sysfs_remove_file(ssdk_sys, &ssdk_log_level_attr.attr);
	sysfs_remove_file(ssdk_sys, &ssdk_dev_id_attr.attr);
#ifdef ISISC
	sysfs_remove_file(ssdk_sys, &ssdk_eth_switch_attr.attr);
#endif
	sysfs_remove_file(ssdk_sys, &ssdk_mac_polling_attr.attr);
	sysfs_remove_file(ssdk_sys, &ssdk_module_debug_stats_attr.attr);
	kobject_put(ssdk_sys);

	/* Free counter buffers */
	mutex_lock(&ssdk_packet_counter_ctx.lock);
	if (ssdk_packet_counter_ctx.buf) {
		kfree(ssdk_packet_counter_ctx.buf);
		ssdk_packet_counter_ctx.buf = NULL;
		ssdk_packet_counter_ctx.buf_size = 0;
	}
	mutex_unlock(&ssdk_packet_counter_ctx.lock);

	mutex_lock(&ssdk_byte_counter_ctx.lock);
	if (ssdk_byte_counter_ctx.buf) {
		kfree(ssdk_byte_counter_ctx.buf);
		ssdk_byte_counter_ctx.buf = NULL;
		ssdk_byte_counter_ctx.buf_size = 0;
	}
	mutex_unlock(&ssdk_byte_counter_ctx.lock);
}

/*qca808x_start*/
int
ssdk_plat_init(ssdk_init_cfg *cfg, a_uint32_t dev_id)
{
/*qca808x_end*/
	hsl_reg_mode reg_mode;
	ssdk_reg_map_info map;

#ifdef IN_UNIPHY
	reg_mode = ssdk_uniphy_reg_access_mode_get(dev_id);
	if(reg_mode == HSL_REG_LOCAL_BUS) {
		ssdk_uniphy_reg_map_info_get(dev_id, &map);
		qca_phy_priv_global[dev_id]->uniphy_hw_addr = ioremap(map.base_addr,
									map.size);
		if (!qca_phy_priv_global[dev_id]->uniphy_hw_addr) {
			SSDK_ERROR("%s ioremap fail.", __func__);
			cfg->reg_func.uniphy_reg_set = NULL;
			cfg->reg_func.uniphy_reg_get = NULL;
			return -1;
		}
		cfg->reg_func.uniphy_reg_set = qca_uniphy_reg_write;
		cfg->reg_func.uniphy_reg_get = qca_uniphy_reg_read;
	}
#endif
	reg_mode = ssdk_switch_reg_access_mode_get(dev_id);
	if (reg_mode == HSL_REG_LOCAL_BUS) {
		ssdk_switch_reg_map_info_get(dev_id, &map);
		qca_phy_priv_global[dev_id]->hw_addr = ioremap(map.base_addr,
								map.size);
		if (!qca_phy_priv_global[dev_id]->hw_addr) {
			SSDK_ERROR("%s ioremap fail.", __func__);
			return -1;
		}

		ssdk_gcc_clock_init(dev_id);
		cfg->reg_mode = HSL_HEADER;
	} else if (reg_mode == HSL_REG_MDIO) {
		cfg->reg_mode = HSL_MDIO;
#if IS_ENABLED(CONFIG_QCOM_FPGA_PCI) || defined(SSDK_PCIE_BUS)
	} else if (reg_mode == HSL_REG_PCIE_BUS) {
		ssdk_pci_ppe_clock_init(dev_id);
#endif
	}
/*qca808x_start*/
	return 0;
}

void
ssdk_plat_exit(a_uint32_t dev_id)
{
/*qca808x_end*/
	hsl_reg_mode reg_mode;
#ifdef DESS
	ssdk_reg_map_info map;
#endif
/*qca808x_start*/
	SSDK_INFO("ssdk_plat_exit\n");
/*qca808x_end*/
	reg_mode = ssdk_switch_reg_access_mode_get(dev_id);
	if (reg_mode == HSL_REG_LOCAL_BUS) {
		struct clk *cmn_clk = NULL;

		iounmap(qca_phy_priv_global[dev_id]->hw_addr);
		cmn_clk = ssdk_dts_cmnclk_get(dev_id);
		if (!IS_ERR(cmn_clk)) {
			ssdk_gcc_clock_exit(dev_id);
		}
	}

#ifdef IN_UNIPHY
	reg_mode = ssdk_uniphy_reg_access_mode_get(dev_id);
	if (reg_mode == HSL_REG_LOCAL_BUS) {
		iounmap(qca_phy_priv_global[dev_id]->uniphy_hw_addr);
	}
#endif
	if (qca_phy_priv_global[dev_id]->core_clk) {
		clk_put(qca_phy_priv_global[dev_id]->core_clk);
		qca_phy_priv_global[dev_id]->core_clk = NULL;
	}
/*qca808x_start*/
}
/*qca808x_end*/

#if !defined(MRPPE)
int ssdk_uniphy_check_by_socid(a_uint32_t dev_id,
		a_uint32_t index, a_uint32_t mode)
{
	int ret = A_TRUE;
	a_uint32_t soc_id = 0;
	int rv = 0;

	rv = qcom_smem_get_soc_id(&soc_id);
	if (rv)
		return A_FALSE;
	switch(soc_id) {
		case QCOM_ID_IPQ8070:
		case QCOM_ID_IPQ8071:
		case QCOM_ID_IPQ8072:
		case QCOM_ID_IPQ8074:
		case QCOM_ID_IPQ8076:
		case QCOM_ID_IPQ8078:
		case QCOM_ID_IPQ8070A:
		case QCOM_ID_IPQ8071A:
		case QCOM_ID_IPQ8072A:
		case QCOM_ID_IPQ8074A:
		case QCOM_ID_IPQ8076A:
		case QCOM_ID_IPQ8078A:
			if (index == SSDK_UNIPHY_INSTANCE0) {
				if ((mode == PORT_WRAPPER_USXGMII) ||
					(mode == PORT_WRAPPER_10GBASE_R))
					ret = A_FALSE;
			}
			if ((mode == PORT_WRAPPER_UQXGMII) ||
				(mode == PORT_WRAPPER_UDXGMII))
				ret = A_FALSE;
			break;
		case QCOM_ID_IPQ6000:
		case QCOM_ID_IPQ6005:
		case QCOM_ID_IPQ6010:
		case QCOM_ID_IPQ6018:
		case QCOM_ID_IPQ6028:
			if (index == SSDK_UNIPHY_INSTANCE0) {
				if ((mode == PORT_WRAPPER_USXGMII) ||
					(mode == PORT_WRAPPER_10GBASE_R))
					ret = A_FALSE;
			}
			if (index > SSDK_UNIPHY_INSTANCE1)
				ret = A_FALSE;
			if ((mode == PORT_WRAPPER_UQXGMII) ||
				(mode == PORT_WRAPPER_UDXGMII))
				ret = A_FALSE;
			break;
		case QCOM_ID_IPQ9570:
		case QCOM_ID_IPQ9574:
			break;
		case QCOM_ID_IPQ9550:
		case QCOM_ID_IPQ9554:
			if (index == SSDK_UNIPHY_INSTANCE1)
				ret = A_FALSE;
			break;
		case QCOM_ID_IPQ9510:
		case QCOM_ID_IPQ9514:
			if ((index == SSDK_UNIPHY_INSTANCE1) ||
				(index == SSDK_UNIPHY_INSTANCE2))
				ret = A_FALSE;
			break;
		case QCOM_ID_IPQ5302:
		case QCOM_ID_IPQ5312:
			if (index == SSDK_UNIPHY_INSTANCE2)
				ret = A_FALSE;
			if ((mode == PORT_WRAPPER_USXGMII) ||
				(mode == PORT_WRAPPER_10GBASE_R) ||
				(mode == PORT_WRAPPER_UQXGMII) ||
				(mode == PORT_WRAPPER_UDXGMII))
				ret = A_FALSE;
			break;
		case QCOM_ID_IPQ5300:
		case QCOM_ID_IPQ5322:
		case QCOM_ID_IPQ5332:
		case QCOM_ID_IPQ5321:
			if (index == SSDK_UNIPHY_INSTANCE2)
				ret = A_FALSE;
			break;
		default:
			ret = A_FALSE;
			break;
	}
	return ret;
}
#endif

int ssdk_uniphy_valid_check(a_uint32_t dev_id,
		a_uint32_t index, a_uint32_t mode)
{
	if (index > SSDK_UNIPHY_INSTANCE2)
		return A_FALSE;

#if defined(MRPPE)
	return ssdk_uniphy_check_by_softsku(dev_id, index);
#else
	return ssdk_uniphy_check_by_socid(dev_id, index, mode);
#endif
}

void ssdk_log_fun(a_uint8_t level, const char *func, unsigned int line, const char *fmt, ...) {
    va_list args;
	char buffer[512] = {0};
	char *level_str;

	switch (level) {
		case SSDK_LOG_LEVEL_ERROR: 	level_str = "ERROR"; 	break;
		case SSDK_LOG_LEVEL_WARN: 	level_str = "WARN"; 	break;
		case SSDK_LOG_LEVEL_INFO: 	level_str = "INFO"; 	break;
		case SSDK_LOG_LEVEL_DEBUG: 	level_str = "DEBUG"; 	break;
		default: 			level_str = "N/A"; 	break;
	}

	if (level<= ssdk_log_level) {
	  va_start(args, fmt);
	  snprintf(buffer, sizeof(buffer), "%s[%u]:%s:", func, line, level_str);
	  vsnprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), fmt, args);
	  printk("%s", buffer);
	  va_end(args);
	}
}
