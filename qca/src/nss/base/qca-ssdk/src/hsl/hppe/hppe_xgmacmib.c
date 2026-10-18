/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hppe_reg_access.h"
#include "hppe_xgmacmib_reg.h"
#include "hppe_xgmacmib.h"


sw_error_t
hppe_mmc_control_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mmc_control_u *value)
{
	if (index >= MMC_CONTROL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + MMC_CONTROL_ADDRESS + \
				index * MMC_CONTROL_INC,
				&value->val);
}

sw_error_t
hppe_mmc_control_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mmc_control_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + MMC_CONTROL_ADDRESS + \
				index * MMC_CONTROL_INC,
				value->val);
}

sw_error_t
hppe_tx_octet_count_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_octet_count_good_bad_low_u *value)
{
	if (index >= TX_OCTET_COUNT_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_OCTET_COUNT_GOOD_BAD_LOW_ADDRESS + \
				index * TX_OCTET_COUNT_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_octet_count_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_octet_count_good_bad_high_u *value)
{
	if (index >= TX_OCTET_COUNT_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_OCTET_COUNT_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_OCTET_COUNT_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_frame_count_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_frame_count_good_bad_low_u *value)
{
	if (index >= TX_FRAME_COUNT_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_FRAME_COUNT_GOOD_BAD_LOW_ADDRESS + \
				index * TX_FRAME_COUNT_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_frame_count_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_frame_count_good_bad_high_u *value)
{
	if (index >= TX_FRAME_COUNT_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_FRAME_COUNT_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_FRAME_COUNT_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_broadcast_frames_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_broadcast_frames_good_low_u *value)
{
	if (index >= TX_BROADCAST_FRAMES_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_BROADCAST_FRAMES_GOOD_LOW_ADDRESS + \
				index * TX_BROADCAST_FRAMES_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_broadcast_frames_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_broadcast_frames_good_high_u *value)
{
	if (index >= TX_BROADCAST_FRAMES_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_BROADCAST_FRAMES_GOOD_HIGH_ADDRESS + \
				index * TX_BROADCAST_FRAMES_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_multicast_frames_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_multicast_frames_good_low_u *value)
{
	if (index >= TX_MULTICAST_FRAMES_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_MULTICAST_FRAMES_GOOD_LOW_ADDRESS + \
				index * TX_MULTICAST_FRAMES_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_multicast_frames_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_multicast_frames_good_high_u *value)
{
	if (index >= TX_MULTICAST_FRAMES_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_MULTICAST_FRAMES_GOOD_HIGH_ADDRESS + \
				index * TX_MULTICAST_FRAMES_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_64octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_64octets_frames_good_bad_low_u *value)
{
	if (index >= TX_64OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_64OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_64OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_64octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_64octets_frames_good_bad_high_u *value)
{
	if (index >= TX_64OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_64OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_64OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_65to127octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_65to127octets_frames_good_bad_low_u *value)
{
	if (index >= TX_65TO127OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_65TO127OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_65TO127OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_65to127octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_65to127octets_frames_good_bad_high_u *value)
{
	if (index >= TX_65TO127OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_65TO127OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_65TO127OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_128to255octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_128to255octets_frames_good_bad_low_u *value)
{
	if (index >= TX_128TO255OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_128TO255OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_128TO255OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_128to255octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_128to255octets_frames_good_bad_high_u *value)
{
	if (index >= TX_128TO255OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_128TO255OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_128TO255OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_256to511octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_256to511octets_frames_good_bad_low_u *value)
{
	if (index >= TX_256TO511OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_256TO511OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_256TO511OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_256to511octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_256to511octets_frames_good_bad_high_u *value)
{
	if (index >= TX_256TO511OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_256TO511OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_256TO511OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_512to1023octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_512to1023octets_frames_good_bad_low_u *value)
{
	if (index >= TX_512TO1023OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_512TO1023OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_512TO1023OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_512to1023octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_512to1023octets_frames_good_bad_high_u *value)
{
	if (index >= TX_512TO1023OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_512TO1023OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_512TO1023OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_1024tomaxoctets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_1024tomaxoctets_frames_good_bad_low_u *value)
{
	if (index >= TX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_1024tomaxoctets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_1024tomaxoctets_frames_good_bad_high_u *value)
{
	if (index >= TX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_unicast_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_unicast_frames_good_bad_low_u *value)
{
	if (index >= TX_UNICAST_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_UNICAST_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_UNICAST_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_unicast_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_unicast_frames_good_bad_high_u *value)
{
	if (index >= TX_UNICAST_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_UNICAST_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_UNICAST_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_multicast_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_multicast_frames_good_bad_low_u *value)
{
	if (index >= TX_MULTICAST_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_MULTICAST_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_MULTICAST_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_multicast_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_multicast_frames_good_bad_high_u *value)
{
	if (index >= TX_MULTICAST_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_MULTICAST_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_MULTICAST_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_broadcast_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_broadcast_frames_good_bad_low_u *value)
{
	if (index >= TX_BROADCAST_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_BROADCAST_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * TX_BROADCAST_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_broadcast_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_broadcast_frames_good_bad_high_u *value)
{
	if (index >= TX_BROADCAST_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_BROADCAST_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * TX_BROADCAST_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}


sw_error_t
hppe_tx_underflow_error_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_underflow_error_frames_low_u *value)
{
	if (index >= TX_UNDERFLOW_ERROR_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_UNDERFLOW_ERROR_FRAMES_LOW_ADDRESS + \
				index * TX_UNDERFLOW_ERROR_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_underflow_error_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_underflow_error_frames_high_u *value)
{
	if (index >= TX_UNDERFLOW_ERROR_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_UNDERFLOW_ERROR_FRAMES_HIGH_ADDRESS + \
				index * TX_UNDERFLOW_ERROR_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_octet_count_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_octet_count_good_low_u *value)
{
	if (index >= TX_OCTET_COUNT_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_OCTET_COUNT_GOOD_LOW_ADDRESS + \
				index * TX_OCTET_COUNT_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_octet_count_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_octet_count_good_high_u *value)
{
	if (index >= TX_OCTET_COUNT_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_OCTET_COUNT_GOOD_HIGH_ADDRESS + \
				index * TX_OCTET_COUNT_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_frame_count_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_frame_count_good_low_u *value)
{
	if (index >= TX_FRAME_COUNT_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_FRAME_COUNT_GOOD_LOW_ADDRESS + \
				index * TX_FRAME_COUNT_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_frame_count_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_frame_count_good_high_u *value)
{
	if (index >= TX_FRAME_COUNT_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_FRAME_COUNT_GOOD_HIGH_ADDRESS + \
				index * TX_FRAME_COUNT_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_pause_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_pause_frames_low_u *value)
{
	if (index >= TX_PAUSE_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_PAUSE_FRAMES_LOW_ADDRESS + \
				index * TX_PAUSE_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_pause_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_pause_frames_high_u *value)
{
	if (index >= TX_PAUSE_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_PAUSE_FRAMES_HIGH_ADDRESS + \
				index * TX_PAUSE_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_vlan_frames_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_vlan_frames_good_low_u *value)
{
	if (index >= TX_VLAN_FRAMES_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_VLAN_FRAMES_GOOD_LOW_ADDRESS + \
				index * TX_VLAN_FRAMES_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_tx_vlan_frames_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_vlan_frames_good_high_u *value)
{
	if (index >= TX_VLAN_FRAMES_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_VLAN_FRAMES_GOOD_HIGH_ADDRESS + \
				index * TX_VLAN_FRAMES_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_tx_lpi_usec_cntr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_lpi_usec_cntr_u *value)
{
	if (index >= TX_LPI_USEC_CNTR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_LPI_USEC_CNTR_ADDRESS + \
				index * TX_LPI_USEC_CNTR_INC,
				&value->val);
}

sw_error_t
hppe_tx_lpi_tran_cntr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_lpi_tran_cntr_u *value)
{
	if (index >= TX_LPI_TRAN_CNTR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + TX_LPI_TRAN_CNTR_ADDRESS + \
				index * TX_LPI_TRAN_CNTR_INC,
				&value->val);
}

sw_error_t
hppe_rx_frame_count_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_frame_count_good_bad_low_u *value)
{
	if (index >= RX_FRAME_COUNT_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_FRAME_COUNT_GOOD_BAD_LOW_ADDRESS + \
				index * RX_FRAME_COUNT_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_frame_count_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_frame_count_good_bad_high_u *value)
{
	if (index >= RX_FRAME_COUNT_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_FRAME_COUNT_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_FRAME_COUNT_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_octet_count_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_octet_count_good_bad_low_u *value)
{
	if (index >= RX_OCTET_COUNT_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OCTET_COUNT_GOOD_BAD_LOW_ADDRESS + \
				index * RX_OCTET_COUNT_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_octet_count_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_octet_count_good_bad_high_u *value)
{
	if (index >= RX_OCTET_COUNT_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OCTET_COUNT_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_OCTET_COUNT_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_octet_count_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_octet_count_good_low_u *value)
{
	if (index >= RX_OCTET_COUNT_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OCTET_COUNT_GOOD_LOW_ADDRESS + \
				index * RX_OCTET_COUNT_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_octet_count_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_octet_count_good_high_u *value)
{
	if (index >= RX_OCTET_COUNT_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OCTET_COUNT_GOOD_HIGH_ADDRESS + \
				index * RX_OCTET_COUNT_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_broadcast_frames_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_broadcast_frames_good_low_u *value)
{
	if (index >= RX_BROADCAST_FRAMES_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_BROADCAST_FRAMES_GOOD_LOW_ADDRESS + \
				index * RX_BROADCAST_FRAMES_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_broadcast_frames_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_broadcast_frames_good_high_u *value)
{
	if (index >= RX_BROADCAST_FRAMES_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_BROADCAST_FRAMES_GOOD_HIGH_ADDRESS + \
				index * RX_BROADCAST_FRAMES_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_multicast_frames_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_multicast_frames_good_low_u *value)
{
	if (index >= RX_MULTICAST_FRAMES_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_MULTICAST_FRAMES_GOOD_LOW_ADDRESS + \
				index * RX_MULTICAST_FRAMES_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_multicast_frames_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_multicast_frames_good_high_u *value)
{
	if (index >= RX_MULTICAST_FRAMES_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_MULTICAST_FRAMES_GOOD_HIGH_ADDRESS + \
				index * RX_MULTICAST_FRAMES_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_crc_error_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_crc_error_frames_low_u *value)
{
	if (index >= RX_CRC_ERROR_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_CRC_ERROR_FRAMES_LOW_ADDRESS + \
				index * RX_CRC_ERROR_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_crc_error_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_crc_error_frames_high_u *value)
{
	if (index >= RX_CRC_ERROR_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_CRC_ERROR_FRAMES_HIGH_ADDRESS + \
				index * RX_CRC_ERROR_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_runt_error_frames_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_runt_error_frames_u *value)
{
	if (index >= RX_RUNT_ERROR_FRAMES_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_RUNT_ERROR_FRAMES_ADDRESS + \
				index * RX_RUNT_ERROR_FRAMES_INC,
				&value->val);
}

sw_error_t
hppe_rx_jabber_error_frames_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_jabber_error_frames_u *value)
{
	if (index >= RX_JABBER_ERROR_FRAMES_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_JABBER_ERROR_FRAMES_ADDRESS + \
				index * RX_JABBER_ERROR_FRAMES_INC,
				&value->val);
}

sw_error_t
hppe_rx_undersize_frames_good_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_undersize_frames_good_u *value)
{
	if (index >= RX_UNDERSIZE_FRAMES_GOOD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_UNDERSIZE_FRAMES_GOOD_ADDRESS + \
				index * RX_UNDERSIZE_FRAMES_GOOD_INC,
				&value->val);
}

sw_error_t
hppe_rx_oversize_frames_good_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_oversize_frames_good_u *value)
{
	if (index >= RX_OVERSIZE_FRAMES_GOOD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OVERSIZE_FRAMES_GOOD_ADDRESS + \
				index * RX_OVERSIZE_FRAMES_GOOD_INC,
				&value->val);
}

sw_error_t
hppe_rx_64octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_64octets_frames_good_bad_low_u *value)
{
	if (index >= RX_64OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_64OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_64OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_64octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_64octets_frames_good_bad_high_u *value)
{
	if (index >= RX_64OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_64OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_64OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_65to127octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_65to127octets_frames_good_bad_low_u *value)
{
	if (index >= RX_65TO127OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_65TO127OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_65TO127OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_65to127octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_65to127octets_frames_good_bad_high_u *value)
{
	if (index >= RX_65TO127OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_65TO127OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_65TO127OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_128to255octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_128to255octets_frames_good_bad_low_u *value)
{
	if (index >= RX_128TO255OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_128TO255OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_128TO255OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_128to255octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_128to255octets_frames_good_bad_high_u *value)
{
	if (index >= RX_128TO255OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_128TO255OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_128TO255OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_256to511octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_256to511octets_frames_good_bad_low_u *value)
{
	if (index >= RX_256TO511OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_256TO511OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_256TO511OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_256to511octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_256to511octets_frames_good_bad_high_u *value)
{
	if (index >= RX_256TO511OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_256TO511OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_256TO511OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_512to1023octets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_512to1023octets_frames_good_bad_low_u *value)
{
	if (index >= RX_512TO1023OCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_512TO1023OCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_512TO1023OCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_512to1023octets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_512to1023octets_frames_good_bad_high_u *value)
{
	if (index >= RX_512TO1023OCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_512TO1023OCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_512TO1023OCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_1024tomaxoctets_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_1024tomaxoctets_frames_good_bad_low_u *value)
{
	if (index >= RX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_1024tomaxoctets_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_1024tomaxoctets_frames_good_bad_high_u *value)
{
	if (index >= RX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_1024TOMAXOCTETS_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_unicast_frames_good_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_unicast_frames_good_low_u *value)
{
	if (index >= RX_UNICAST_FRAMES_GOOD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_UNICAST_FRAMES_GOOD_LOW_ADDRESS + \
				index * RX_UNICAST_FRAMES_GOOD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_unicast_frames_good_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_unicast_frames_good_high_u *value)
{
	if (index >= RX_UNICAST_FRAMES_GOOD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_UNICAST_FRAMES_GOOD_HIGH_ADDRESS + \
				index * RX_UNICAST_FRAMES_GOOD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_length_error_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_length_error_frames_low_u *value)
{
	if (index >= RX_LENGTH_ERROR_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_LENGTH_ERROR_FRAMES_LOW_ADDRESS + \
				index * RX_LENGTH_ERROR_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_length_error_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_length_error_frames_high_u *value)
{
	if (index >= RX_LENGTH_ERROR_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_LENGTH_ERROR_FRAMES_HIGH_ADDRESS + \
				index * RX_LENGTH_ERROR_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_outofrange_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_outofrange_frames_low_u *value)
{
	if (index >= RX_OUTOFRANGE_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OUTOFRANGE_FRAMES_LOW_ADDRESS + \
				index * RX_OUTOFRANGE_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_outofrange_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_outofrange_frames_high_u *value)
{
	if (index >= RX_OUTOFRANGE_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_OUTOFRANGE_FRAMES_HIGH_ADDRESS + \
				index * RX_OUTOFRANGE_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_pause_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_pause_frames_low_u *value)
{
	if (index >= RX_PAUSE_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_PAUSE_FRAMES_LOW_ADDRESS + \
				index * RX_PAUSE_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_pause_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_pause_frames_high_u *value)
{
	if (index >= RX_PAUSE_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_PAUSE_FRAMES_HIGH_ADDRESS + \
				index * RX_PAUSE_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_fifooverflow_frames_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifooverflow_frames_low_u *value)
{
	if (index >= RX_FIFOOVERFLOW_FRAMES_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_FIFOOVERFLOW_FRAMES_LOW_ADDRESS + \
				index * RX_FIFOOVERFLOW_FRAMES_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_fifooverflow_frames_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifooverflow_frames_high_u *value)
{
	if (index >= RX_FIFOOVERFLOW_FRAMES_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_FIFOOVERFLOW_FRAMES_HIGH_ADDRESS + \
				index * RX_FIFOOVERFLOW_FRAMES_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_vlan_frames_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_vlan_frames_good_bad_low_u *value)
{
	if (index >= RX_VLAN_FRAMES_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_VLAN_FRAMES_GOOD_BAD_LOW_ADDRESS + \
				index * RX_VLAN_FRAMES_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_vlan_frames_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_vlan_frames_good_bad_high_u *value)
{
	if (index >= RX_VLAN_FRAMES_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_VLAN_FRAMES_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_VLAN_FRAMES_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_watchdog_error_frames_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_watchdog_error_frames_u *value)
{
	if (index >= RX_WATCHDOG_ERROR_FRAMES_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_WATCHDOG_ERROR_FRAMES_ADDRESS + \
				index * RX_WATCHDOG_ERROR_FRAMES_INC,
				&value->val);
}

sw_error_t
hppe_rx_lpi_usec_cntr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_lpi_usec_cntr_u *value)
{
	if (index >= RX_LPI_USEC_CNTR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_LPI_USEC_CNTR_ADDRESS + \
				index * RX_LPI_USEC_CNTR_INC,
				&value->val);
}

sw_error_t
hppe_rx_lpi_tran_cntr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_lpi_tran_cntr_u *value)
{
	if (index >= RX_LPI_TRAN_CNTR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_LPI_TRAN_CNTR_ADDRESS + \
				index * RX_LPI_TRAN_CNTR_INC,
				&value->val);
}

sw_error_t
hppe_rx_discard_frame_count_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_discard_frame_count_good_bad_low_u *value)
{
	if (index >= RX_DISCARD_FRAME_COUNT_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_DISCARD_FRAME_COUNT_GOOD_BAD_LOW_ADDRESS + \
				index * RX_DISCARD_FRAME_COUNT_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_discard_frame_count_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_discard_frame_count_good_bad_high_u *value)
{
	if (index >= RX_DISCARD_FRAME_COUNT_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_DISCARD_FRAME_COUNT_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_DISCARD_FRAME_COUNT_GOOD_BAD_HIGH_INC,
				&value->val);
}

sw_error_t
hppe_rx_discard_octet_count_good_bad_low_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_discard_octet_count_good_bad_low_u *value)
{
	if (index >= RX_DISCARD_OCTET_COUNT_GOOD_BAD_LOW_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_DISCARD_OCTET_COUNT_GOOD_BAD_LOW_ADDRESS + \
				index * RX_DISCARD_OCTET_COUNT_GOOD_BAD_LOW_INC,
				&value->val);
}

sw_error_t
hppe_rx_discard_octet_count_good_bad_high_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_discard_octet_count_good_bad_high_u *value)
{
	if (index >= RX_DISCARD_OCTET_COUNT_GOOD_BAD_HIGH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_XGMAC_CSR_BASE_ADDR + RX_DISCARD_OCTET_COUNT_GOOD_BAD_HIGH_ADDRESS + \
				index * RX_DISCARD_OCTET_COUNT_GOOD_BAD_HIGH_INC,
				&value->val);
}
