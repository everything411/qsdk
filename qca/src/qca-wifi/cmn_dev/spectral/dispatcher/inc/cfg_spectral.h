/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains centralized cfg definitions of Spectral component
 */
#ifndef __CONFIG_SPECTRAL_H
#define __CONFIG_SPECTRAL_H

#ifdef WLAN_SPECTRAL_STREAMFS
#include <cfg_spectral_streamfs.h>
#else
#define CFG_SPECTRAL_STREAMFS_ALL
#endif

/*
 * <ini>
 * spectral_disable - disable spectral feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to disable spectral feature.
 *
 * Related: None
 *
 * Supported Feature: Spectral
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SPECTRAL_DISABLE \
	CFG_INI_BOOL("spectral_disable", false, \
			"Spectral disable")

/*
 * <ini>
 * poison_spectral_bufs - enable poisoning of spectral buffers
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable the poisoning of spectral buffers.
 *
 * Related: None
 *
 * Supported Feature: Spectral
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SPECTRAL_POISON_BUFS \
	CFG_INI_BOOL("poison_spectral_bufs", false, \
			"Enable spectral bufs poison at init")

/*
 * <ini>
 * spectral_data_transport_mechanism - Set the transport mechanism to be
 * used for spectral data buffers
 * @Netlink: 0
 * @Streamfs: 1
 * @Default: 0
 *
 * This ini is used to choose the transport mechanism for spectral data
 * transfer from driver to userspace.
 *
 * Related: None
 *
 * Supported Feature: Spectral
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SPECTRAL_DATA_TRANSPORT_MECHANISM \
	CFG_INI_UINT("spectral_data_transport_mechanism", 0, \
		1, 0, CFG_VALUE_OR_DEFAULT, \
		"Streamfs/Netlink mode to transfer spectral data")

#define CFG_SPECTRAL_ALL \
	CFG(CFG_SPECTRAL_DISABLE) \
	CFG(CFG_SPECTRAL_POISON_BUFS) \
	CFG(CFG_SPECTRAL_DATA_TRANSPORT_MECHANISM) \
	CFG_SPECTRAL_STREAMFS_ALL

#endif
