/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _SSDK_PHY_I2C_H_
#define _SSDK_PHY_I2C_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define QCA_PHY_I2C_MMD_OR_MII_SHIFT	6
#define QCA_PHY_I2C_IS_MMD		1
#define QCA_PHY_I2C_IS_MII		0
#define QCA_PHY_I2C_MMD_ADDR_OR_DATA_SHIFT		5
#define QCA_PHY_I2C_MMD_IS_ADDR		1
#define QCA_PHY_I2C_MMD_IS_DATA		0

#define QCA_PHY_MII_ADDR_C45  			(1<<30)
#define QCA_PHY_MII_ADDR_C45_IS_MMD(reg_addr_c45) ((reg_addr_c45) & QCA_PHY_MII_ADDR_C45)
#define QCA_PHY_MII_ADDR_C45_MMD_NUM(reg_addr_c45) (((reg_addr_c45) >> 16) & 0x1f)
#define QCA_PHY_MII_ADDR_C45_REG_ADDR(reg_addr_c45) ((reg_addr_c45) & 0xffff)

#define QCA_PHY_I2C_PHYCORE_DEVADDR		0x44
#define QCA_PHY_I2C_SERDES_DEVADDR		0x45
#define QCA_PHY_I2C_DEVADDR_MASK		0x47

#define QCA_PHY_MMD1_NUM  1
#define QCA_PHY_MMD3_NUM  3
#define QCA_PHY_MMD7_NUM  7

sw_error_t
__qca_i2c_data_get(a_uint32_t dev_id, a_uint32_t i2c_slave,
                           a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count);
sw_error_t
qca_i2c_data_get(a_uint32_t dev_id, a_uint32_t i2c_slave,
                           a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count);
sw_error_t
__qca_i2c_data_set(a_uint32_t dev_id, a_uint32_t i2c_slave,
                           a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count);
sw_error_t
qca_i2c_data_set(a_uint32_t dev_id, a_uint32_t i2c_slave,
                           a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _SSDK_PHY_I2C_H_ */
