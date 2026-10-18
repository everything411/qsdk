/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

/* This file has WIN specific 11BE WMI structures and macros. */

#ifndef _WMI_UNIFIED_AP_11BE_PARAMS_H_
#define _WMI_UNIFIED_AP_11BE_PARAMS_H_

#include <wlan_mlo_mgr_public_structs.h>

/**
 * struct wmi_ml_bcn_offload_quiet_mode_params - Partner link Quiet information
 * @vdev_id: Vdev id
 * @hw_link_id: Unique hw link id across SoCs
 * @beacon_interval: Beacon interval
 * @period: Quite period
 * @duration: Quite duration
 * @next_start: Next quiet start
 * @flag: 0 - disable, 1 - enable and continuous, 3 - enable and single shot
 */
struct wmi_ml_bcn_offload_quiet_mode_params {
	uint32_t vdev_id;
	uint32_t hw_link_id;
	uint32_t bcn_interval;
	uint32_t period;
	uint32_t duration;
	uint32_t next_start;
	uint32_t flag;
};

/**
 * struct wmi_mlo_bcn_offload_partner_links - ML partner links
 * @num_links: Number of links
 * @partner_info: Partner link info
 */
struct wmi_mlo_bcn_offload_partner_links {
	uint8_t num_links;
	struct wmi_ml_bcn_offload_quiet_mode_params partner_info[WLAN_UMAC_MLO_MAX_VDEVS];
};
#endif /* _WMI_UNIFIED_AP_11BE_PARAMS_H_ */
