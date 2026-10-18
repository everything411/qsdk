/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_htt_reg.h"

sw_error_t
httppe_mac_tx_configuration_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_tx_configuration_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + MAC_TX_CONFIGURATION_ADDRESS + \
				index * MAC_TX_CONFIGURATION_INC,
				&value->val);
}

sw_error_t
httppe_mac_tx_configuration_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_tx_configuration_u *value)
{
		return hppe_reg_set(
					dev_id,
					NSS_XGMAC_CSR_BASE_ADDR + MAC_TX_CONFIGURATION_ADDRESS + \
					index * MAC_TX_CONFIGURATION_INC,
					value->val);
}
