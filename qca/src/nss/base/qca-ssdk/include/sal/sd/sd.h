/*
 * Copyright (c) 2012, 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*qca808x_start*/
#ifndef _SD_H_
#define _SD_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

	sw_error_t
		sd_reg_mdio_set(a_uint32_t dev_id, a_uint32_t phy, a_uint32_t reg,
				a_uint16_t data);

	sw_error_t
		sd_reg_mdio_get(a_uint32_t dev_id, a_uint32_t phy, a_uint32_t reg,
				a_uint16_t * data);

	sw_error_t
		sd_reg_i2c_set(a_uint32_t dev_id, a_uint32_t phy, a_uint32_t reg,
				a_uint16_t data);

	sw_error_t
		sd_reg_i2c_get(a_uint32_t dev_id, a_uint32_t phy, a_uint32_t reg,
				a_uint16_t * data);
	/*qca808x_end*/
	sw_error_t
		sd_reg_hdr_set(a_uint32_t dev_id, a_uint32_t reg_addr,
				a_uint8_t * reg_data, a_uint32_t len);

	sw_error_t
		sd_reg_hdr_get(a_uint32_t dev_id, a_uint32_t reg_addr,
				a_uint8_t * reg_data, a_uint32_t len);

	sw_error_t
		sd_reg_uniphy_set(a_uint32_t dev_id, a_uint32_t index,
				a_uint32_t reg_addr, a_uint8_t * reg_data, a_uint32_t len);

	sw_error_t
		sd_reg_uniphy_get(a_uint32_t dev_id, a_uint32_t index,
				a_uint32_t reg_addr, a_uint8_t * reg_data, a_uint32_t len);

	void
		sd_reg_mii_set(a_uint32_t dev_id, a_uint32_t reg, a_uint32_t val);

	a_uint32_t
		sd_reg_mii_get(a_uint32_t dev_id, a_uint32_t reg);

	sw_error_t
		sd_reg_mii_mdio_get(a_uint32_t dev_id, a_uint32_t reg_addr,
				a_uint8_t value[], a_uint32_t value_len);

	sw_error_t
		sd_reg_mii_mdio_set(a_uint32_t dev_id, a_uint32_t reg_addr,
				a_uint8_t value[], a_uint32_t value_len);
	/*qca808x_start*/
	sw_error_t sd_init(a_uint32_t dev_id, ssdk_init_cfg * cfg);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SD_H_ */
/*qca808x_end*/

