/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/phy.h>
#include <linux/i2c.h>
#include "nss_macsec_types.h"
#include "nss_macsec.h"
#include "dal_utility.h"


#ifndef BIT
#define BIT(_n)		(1UL << (_n))
#endif

#define TO_SFP_PHY_ADDR(phy_addr)	(phy_addr + 0x2f)

#define PHY_REG_MAX_LEN  16

#define QCA_PHY_I2C_MMD_OR_MII_SHIFT		6
#define QCA_PHY_I2C_IS_MMD			1
#define QCA_PHY_I2C_MMD_ADDR_OR_DATA_SHIFT	5
#define QCA_PHY_I2C_MMD_IS_ADDR			1
#define QCA_PHY_I2C_MMD_IS_DATA			0


struct qca808x_phy_info {
	struct list_head list;
	u32 dev_id;
	/* phy real address,it is the mdio addr or the i2c slave addr */
	u32 phy_addr;
	/* the address of phy device, it is a fake addr for
	   the i2c accessed phy */
	u32 phydev_addr;
};

typedef struct {
	struct phy_device *phydev;
	struct qca808x_phy_info *phy_info;

} qca808x_priv;


static g_error_t
qca_phy_i2c_mmd_read(u32 dev_id, u32 phy_addr, u16 mmd_num,
			u32 reg_addr, u16 *reg_data)
{
	u8 ret;
	struct i2c_adapter *adapt;
	/*
		TX buffer to send:
		1st byte: Bit[6](1:MMD 0:MII)
			  Bit[5](1:MMD address 0:MMD data) Bit[4:0](MMD num)
		2nd byte is high 8bits of reg addr
		3rd byte is low 8bits of reg addr
		4th byte: add one extra byte at the end of address
			  writing msg to avoid qca808x
                          treat it as data writing operation
	*/
	u8 tx[4] = {
		((QCA_PHY_I2C_IS_MMD << QCA_PHY_I2C_MMD_OR_MII_SHIFT) |
		(QCA_PHY_I2C_MMD_IS_ADDR <<
		QCA_PHY_I2C_MMD_ADDR_OR_DATA_SHIFT) | mmd_num),
		(reg_addr >> 8) & 0xff, reg_addr & 0xff, 0 };
	/*
		RX buffer to receive:
		1st byte is high 8bits of reg data
		2nd byte is low 8bits of reg data
	*/
	u8 rx[2] = { 0 };
	struct i2c_msg msg[] = {
		{ .addr = phy_addr, .flags = 0,
		  .buf = tx, .len = sizeof(tx) },
		{ .addr = phy_addr, .flags = I2C_M_RD,
		  .buf = rx, .len = sizeof(rx) } };

	if(mmd_num > 31)
		return ERROR;

	adapt = i2c_get_adapter(0);
	if (!adapt)
		return ERROR;

	ret = i2c_transfer(adapt, msg, ARRAY_SIZE(msg));
	*reg_data = (rx[0] << 8) | rx[1];

	i2c_put_adapter(adapt);

	if (ret != ARRAY_SIZE(msg))
		return ERROR;

	return OK;
}

static g_error_t
qca_phy_i2c_mmd_write(u32 dev_id, u32 phy_addr, u16 mmd_num,
			u32 reg_addr, u16 reg_data)
{
	u8 ret;
	struct i2c_adapter *adapt;
	/*
		First TX buffer to send:
		1st byte: Bit[6](1:MMD 0:MII) Bit[5](1:MMD address 0:MMD data)
			  Bit[4:0](MMD num)
		2nd byte is high 8bits of reg addr
		3rd byte is low 8bits of reg addr
		4th byte: add one extra byte at the end of address
			  writing msg to avoid qca808x
                          treat it as data writing operation
	*/
	u8 tx[4] = {
		((QCA_PHY_I2C_IS_MMD << QCA_PHY_I2C_MMD_OR_MII_SHIFT) |
		(QCA_PHY_I2C_MMD_IS_ADDR <<
		QCA_PHY_I2C_MMD_ADDR_OR_DATA_SHIFT) | mmd_num),
		(reg_addr >> 8) & 0xff, reg_addr & 0xff, 0 };

	/*
		Second TX buffer to send:
		1st byte: Bit[6](1:MMD 0:MII) Bit[5](1:MMD address 0:MMD data)
			  Bit[4:0](MMD num)
		2nd byte is high 8bits of reg data
		3rd byte is low 8bits of reg data
	*/
	u8 tx2[3] = {
		((QCA_PHY_I2C_IS_MMD << QCA_PHY_I2C_MMD_OR_MII_SHIFT) |
		(QCA_PHY_I2C_MMD_IS_DATA <<
		QCA_PHY_I2C_MMD_ADDR_OR_DATA_SHIFT) | mmd_num),
		(reg_data >> 8) & 0xff, reg_data & 0xff };

	struct i2c_msg msg[] = {
		{ .addr = phy_addr, .flags = 0,
		  .buf = tx, .len = sizeof(tx) },
		{ .addr = phy_addr, .flags = 0,
		  .buf = tx2, .len = sizeof(tx2) } };

	if(mmd_num > 31)
		return ERROR;

	adapt = i2c_get_adapter(0);
	if (!adapt)
		return ERROR;

	ret = i2c_transfer(adapt, msg, ARRAY_SIZE(msg));

	i2c_put_adapter(adapt);

	if (ret != ARRAY_SIZE(msg))
		return ERROR;

	return OK;
}

static g_error_t phy_mmd_reg_write(struct phy_device *phydev,
				enum access_layer_t phy_layer,
				u32 reg_addr, u16 val16)
{
	struct qca808x_phy_info *pdata = NULL;
	qca808x_priv *priv = NULL;
	g_error_t rlt = OK;
	int ret  = 0;
	int devad = 0;

	if (phy_layer == PHY_MMD3)
		devad = MDIO_MMD_PCS;
	else if (phy_layer == PHY_MMD7)
		devad = MDIO_MMD_AN;
	else
		return ERROR_NOT_SUPPORT;

	if(phydev->priv) {
		priv = (qca808x_priv *)phydev->priv;
		if(priv->phy_info)
			pdata = priv->phy_info;
	}

	if(pdata && (pdata->phy_addr & BIT(24))) {
		rlt = qca_phy_i2c_mmd_write(pdata->dev_id,
				TO_SFP_PHY_ADDR((pdata->phy_addr & 0x7f)),
				devad, reg_addr, val16 );
	} else {
		ret = phy_write_mmd(phydev, devad, reg_addr, val16);
		if (ret)
			rlt = ERROR;
	}
	return rlt;
}

static g_error_t phy_mmd_reg_read(struct phy_device *phydev,
				enum access_layer_t phy_layer,
				u32 reg_addr, u16 *p_val16)
{
	struct qca808x_phy_info *pdata = NULL;
	qca808x_priv *priv = NULL;
	g_error_t rlt = OK;
	int val  = 0;
	int devad = 0;

	if (phy_layer == PHY_MMD3)
		devad = MDIO_MMD_PCS;
	else if (phy_layer == PHY_MMD7)
		devad = MDIO_MMD_AN;
	else
		return ERROR_NOT_SUPPORT;

	if(phydev->priv) {
		priv = (qca808x_priv *)phydev->priv;
		if(priv->phy_info)
			pdata = priv->phy_info;
	}

	if(pdata && (pdata->phy_addr & BIT(24))) {
		rlt = qca_phy_i2c_mmd_read(pdata->dev_id,
				TO_SFP_PHY_ADDR((pdata->phy_addr & 0x7f)),
				devad, reg_addr, p_val16);
	} else {
		val = phy_read_mmd(phydev, devad, reg_addr);
		if (val < 0)
			rlt = ERROR;
		else
			*p_val16 = val;
	}
	return rlt;
}

g_error_t reg_access_write(struct macsec_port *port, enum access_layer_t reg_layer,
	u32 reg_addr, u16 val16)
{
	g_error_t ret  = OK;

	if (port->type == MACSEC_IN_PHY)
		ret = phy_mmd_reg_write(port->phydev, reg_layer, reg_addr, val16);
	else
		return ERROR_NOT_SUPPORT;

	return ret;
}

g_error_t reg_access_read(struct macsec_port *port, enum access_layer_t reg_layer,
	u32 reg_addr, u16 *p_val16)
{
	g_error_t ret  = OK;

	if (port->type == MACSEC_IN_PHY)
		ret = phy_mmd_reg_read(port->phydev, reg_layer, reg_addr, p_val16);
	else
		return ERROR_NOT_SUPPORT;

	return ret;
}

g_error_t reg_acces_field_write(struct macsec_port *port, enum access_layer_t reg_layer,
	u32 reg_addr, u8 offset, u8 length, u16 val16)
{
	g_error_t ret  = OK;
	u16  data = 0;
	u16  data16 = 0;

	if (length == 0
		|| offset >= PHY_REG_MAX_LEN
		|| length > (PHY_REG_MAX_LEN - offset))
		return ERROR_PARAM;

	ret = reg_access_read(port, reg_layer, reg_addr, &data);
	if (unlikely(ret != OK))
		return ret;

	val16 &= ((0x1UL << length) - 1);
	data16 = (data & ~(((0x1UL << length) - 1) << offset)) |
		(val16 << offset);

	if ((data & 0xffff) == data16)
		return ret;

	ret = reg_access_write(port, reg_layer, reg_addr, data16);
	return ret;
}

g_error_t reg_acces_field_read(struct macsec_port *port, enum access_layer_t reg_layer,
	u32 reg_addr, u8 offset, u8 length, u16 *p_val16)
{
	g_error_t ret = OK;
	u16 data = 0;

	if (length == 0 || offset >= PHY_REG_MAX_LEN
		|| length > (PHY_REG_MAX_LEN - offset)
		|| !p_val16)
		return ERROR_PARAM;

	ret = reg_access_read(port, reg_layer, reg_addr, &data);
	if (unlikely(ret != OK))
		return ret;

	if ((offset == 0) && (length == 16))
		*p_val16 = data & 0xffff;
	else
		*p_val16 = (data >> offset) & ((0x1UL << length) - 1);

	return ret;
}

g_error_t reg_access_mask_write(struct macsec_port *port, enum access_layer_t reg_layer,
	u32 reg_addr, u16 mask, u16 val16)
{
	g_error_t ret = OK;
	u16 data = 0;
	u16 data16 = val16;

	if (mask != 0xffff) {
		ret = reg_access_read(port, reg_layer, reg_addr, &data);
		if (unlikely(ret != OK))
			return ret;

		data &= ~mask;
		data16 = (data & 0xffff) | (val16 & mask);
	}

	ret = reg_access_write(port, reg_layer, reg_addr, data16);

	return ret;
}


