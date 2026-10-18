/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*qca808x_start*/
#include "sw.h"
#include "ssdk_phy_i2c.h"
#include <linux/i2c.h>
#include "ssdk_init.h"
#include "ssdk_plat.h"
#include "hsl_phy.h"

#define I2C_RW_LIMIT           8
/******************************************************************************
*
* _qca_i2c_read - read data per i2c bus
*
* read data per i2c bus
*/
static inline a_int16_t
__qca_i2c_read(a_uint32_t i2c_bus_id, a_uint32_t i2c_slave,
		a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count)
{
	a_int16_t ret, i;
	struct i2c_adapter *adapt;
	a_uint8_t addrbuf[2];
	struct i2c_msg msg[2];

	ret =0;
	i = 0;

	if (data_addr & 0xff00) {
		addrbuf[i++] = (data_addr >> 8) & 0xff;
	}

	addrbuf[i++] = data_addr & 0xff;

	if (count > I2C_RW_LIMIT) {
		count = I2C_RW_LIMIT;
	}

	/*
	 * msg for configuring the address
	 */
	aos_mem_set(msg, 0, sizeof(msg));
	msg[0].addr = i2c_slave;
	msg[0].len = i;
	msg[0].buf = addrbuf;

	/*
	 * msg for acquiring data
	 */
	msg[1].addr = i2c_slave;
	msg[1].flags = I2C_M_RD;
	msg[1].len = count;
	msg[1].buf = buf;

	adapt = i2c_get_adapter(i2c_bus_id);
	if (adapt) {
		ret = __i2c_transfer(adapt, msg, ARRAY_SIZE(msg));
		i2c_put_adapter(adapt);
	}

	if (ret == ARRAY_SIZE(msg)) {
		return count;
	}

	return ret;
}

/******************************************************************************
*
* qca_i2c_data_get - wrapper of reading data per i2c bus
*
* wrapper of reading data per i2c bus
*/
sw_error_t
__qca_i2c_data_get(a_uint32_t dev_id, a_uint32_t i2c_slave,
		a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count)
{
	a_int16_t ret = 0, cur = 0;
	a_uint16_t cnt = count;

	while (cnt) {
		cur = __qca_i2c_read(I2C_ADAPTER_DEFAULT_ID,
				i2c_slave, data_addr, buf, cnt);

		/* No such i2c_slave device */
		if (cur == -ENXIO) {
			return SW_NO_RESOURCE;
		}

		if (cur <= 0) {
			break;
		}
		/*
		 * loop to acquire the data from the new
		 * address based on the returned count.
		 */
		cnt -= cur;
		buf += cur;
		data_addr += cur;
		ret += cur;
	}

	if (ret != count) {
		return SW_FAIL;
	}

	return SW_OK;
}

sw_error_t
qca_i2c_data_get(a_uint32_t dev_id, a_uint32_t i2c_slave,
		a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count)
{
	sw_error_t rv = SW_OK;
	struct i2c_adapter *adapt = i2c_get_adapter(I2C_ADAPTER_DEFAULT_ID);

	SW_RTN_ON_NULL(adapt);

	i2c_lock_bus(adapt, I2C_LOCK_SEGMENT);
	rv = __qca_i2c_data_get(dev_id, i2c_slave, data_addr, buf, count);
	i2c_unlock_bus(adapt, I2C_LOCK_SEGMENT);

	return rv;
}

/******************************************************************************
*
* _qca_i2c_write - write data per i2c bus
*
* write data per i2c bus
*/
static inline a_int16_t
__qca_i2c_write(a_uint32_t i2c_bus_id, a_uint32_t i2c_slave,
		a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count)
{
	a_int16_t ret, i;
	struct i2c_adapter *adapt;
	struct i2c_msg msg;
	a_uint8_t i2c_wbuf[I2C_RW_LIMIT+2];

	ret = 0;
	i = 0;

	if (data_addr & 0xff00) {
		i2c_wbuf[i++] = (data_addr >> 8) & 0xff;
	}

	/*
	 * the write buffer is for saving address
	 * and the data to sent.
	 */
	i2c_wbuf[i++] = data_addr & 0xff;

	if (count > I2C_RW_LIMIT) {
		count = I2C_RW_LIMIT;
	}
	memcpy(&i2c_wbuf[i], buf, count);

	aos_mem_set(&msg, 0, sizeof(msg));
	msg.addr = i2c_slave;
	msg.len = i + count;
	msg.buf = i2c_wbuf;

	adapt = i2c_get_adapter(i2c_bus_id);
	if (adapt) {
		ret = __i2c_transfer(adapt, &msg, 1);
		i2c_put_adapter(adapt);
	}

	if (ret == 1) {
		return count;
	}

	return ret;
}

/******************************************************************************
*
* qca_i2c_data_set - wrapper of writting data per i2c bus
*
* wrapper of writting data per i2c bus
*/
sw_error_t
__qca_i2c_data_set(a_uint32_t dev_id, a_uint32_t i2c_slave,
		a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count)
{
	a_int16_t ret = 0, cur = 0;
	a_uint16_t cnt = count;

	while (cnt) {
		cur = __qca_i2c_write(I2C_ADAPTER_DEFAULT_ID,
				i2c_slave, data_addr, buf, cnt);

		/* No such i2c_slave device */
		if (cur == -ENXIO) {
			return SW_NO_RESOURCE;
		}

		if (cur <= 0) {
			break;
		}
		/*
		 * loop to write the data to the new
		 * address based on the returned count.
		 */
		cnt -= cur;
		buf += cur;
		data_addr += cur;
		ret += cur;
	}

	if (ret != count) {
		return SW_FAIL;
	}

	return SW_OK;
}

sw_error_t
qca_i2c_data_set(a_uint32_t dev_id, a_uint32_t i2c_slave,
		a_uint32_t data_addr, a_uint8_t *buf, a_uint32_t count)
{
	sw_error_t rv = SW_OK;
	struct i2c_adapter *adapt = i2c_get_adapter(I2C_ADAPTER_DEFAULT_ID);

	SW_RTN_ON_NULL(adapt);

	i2c_lock_bus(adapt, I2C_LOCK_SEGMENT);
	rv = __qca_i2c_data_set(dev_id, i2c_slave, data_addr, buf, count);
	i2c_unlock_bus(adapt, I2C_LOCK_SEGMENT);

	return rv;
}

