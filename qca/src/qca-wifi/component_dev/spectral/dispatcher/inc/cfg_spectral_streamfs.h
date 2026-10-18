/**
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * with streamfs data transport functionality.
 */
#ifndef __CONFIG_SPECTRAL_STREAMFS_H
#define __CONFIG_SPECTRAL_STREAMFS_H

/*
 * <ini>
 * spectral_streamfs_num_bufs - Number of sub-buffers allocated.
 * @min: SPECTRAL_STREAMFS_MIN_SUBBUFS
 * @max: SPECTRAL_STREAMFS_MX_SUBBUFS
 * @Default: SPECTRAL_STREAMFS_MIN_SUBBUFS
 *
 * This ini is used to set number of streamfs sub-buffers in a
 * channel buffer.
 *
 * Related: None
 *
 * Supported Feature: Spectral
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SPECTRAL_STREAMFS_NUM_BUFFERS \
	CFG_INI_UINT("spectral_streamfs_num_bufs",\
			SPECTRAL_STREAMFS_MIN_SUBBUFS, \
			SPECTRAL_STREAMFS_MAX_SUBBUFS, \
			SPECTRAL_STREAMFS_MIN_SUBBUFS, \
			CFG_VALUE_OR_CLAMP, \
			"Spectral streamfs no. of buffers")

/*
 * <ini>
 * spectral_streamfs_databuf_size - Size of sub-buffer allocated in bytes.
 * @min: SPECTRAL_MIN_REPORT_SIZE
 * @max: SPECTRAL_MAX_REPORT_SIZE
 * @Default: SPECTRAL_MIN_REPORT_SIZE
 *
 * This ini is used to set the size of each streamfs sub-buffer.
 *
 * Related: None
 *
 * Supported Feature: Spectral
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SPECTRAL_STREAMFS_BUFFER_SIZE \
	CFG_INI_UINT("spectral_streamfs_databuf_size",\
			SPECTRAL_MIN_REPORT_SIZE, \
			SPECTRAL_MAX_REPORT_SIZE, \
			SPECTRAL_MAX_REPORT_SIZE, \
			CFG_VALUE_OR_CLAMP, \
			"Spectral streamfs buffer size")



#define CFG_SPECTRAL_STREAMFS_ALL \
	CFG(CFG_SPECTRAL_STREAMFS_NUM_BUFFERS) \
	CFG(CFG_SPECTRAL_STREAMFS_BUFFER_SIZE)

#endif
