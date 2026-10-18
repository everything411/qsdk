/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains centralized cfg definitions of wifi_radar component
 */
#ifndef _WIFI_RADAR_CFG_H_
#define _WIFI_RADAR_CFG_H_

/*
 * <ini>
 * wifi_radar_enable - Bitmap denoting th PDEVs for which wifi radar needs to
 * be enabled
 * @Min: 0
 * @Max: Bitmap with num of set bits equal to total num of PDEVs in the SOC
 * @Default: 0
 *
 * This ini is used to enable wifi radar feature for PDEV.
 *
 * Related: None
 *
 * Supported Feature: wifi_radar
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WIFI_RADAR_ENABLE \
	CFG_INI_UINT("wifi_radar_enable", \
		     0, \
		     0x7, \
		     0, \
		     CFG_VALUE_OR_DEFAULT, \
		     "wifi radar enable bitmap")

#define CFG_WIFI_RADAR_ALL \
	CFG(CFG_WIFI_RADAR_ENABLE)

#endif /* _WIFI_RADAR_CFG_H_ */
