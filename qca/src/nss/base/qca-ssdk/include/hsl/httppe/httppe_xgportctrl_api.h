/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_XGPORTCTRL_API_H_
#define _HTTPPE_XGPORTCTRL_API_H_

sw_error_t
httppe_mac_tx_configuration_get(
        a_uint32_t dev_id,
        a_uint32_t index,
        union mac_tx_configuration_u *value);

sw_error_t
httppe_mac_tx_configuration_set(
        a_uint32_t dev_id,
        a_uint32_t index,
        union mac_tx_configuration_u *value);

#endif
