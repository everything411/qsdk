/*
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

#ifndef _OS_IF_SPECTRAL_STREAMFS_H
#define _OS_IF_SPECTRAL_STREAMFS_H

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_spectral_public_structs.h>

#if defined(WLAN_CONV_SPECTRAL_ENABLE) && defined(WLAN_SPECTRAL_STREAMFS)
/**
 * os_if_spectral_streamfs_init() - Initialize Spectral data structures
 * and register the handlers with Spectral target_if
 * @pdev: Pointer to pdev
 *
 * Preparing buffer and sending messages to application layer are
 * defined in os_if layer, they need to be registered with Spectral target_if
 *
 * Return: None
 */
QDF_STATUS os_if_spectral_streamfs_init(struct wlan_objmgr_pdev *pdev);
/**
 * os_if_spectral_streamfs_deinit() - De-initialize Spectral data
 * structures and de-register the  handlers from Spectral target_if
 * @pdev: Pointer to pdev
 *
 * Return: None
 */
QDF_STATUS os_if_spectral_streamfs_deinit(struct wlan_objmgr_pdev *pdev);

/**
 * os_if_spectral_streamfs_reset_channel() - Reset spectral streamfs channel
 * @pdev: Pointer to pdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
os_if_spectral_streamfs_reset_channel(struct wlan_objmgr_pdev *pdev);
#else
static inline
QDF_STATUS os_if_spectral_streamfs_init(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS os_if_spectral_streamfs_deinit(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline  QDF_STATUS
os_if_spectral_streamfs_reset_channel(struct wlan_objmgr_pdev *pdev)
{
    return QDF_STATUS_SUCCESS;
}
#endif
#endif /* _OS_IF_SPECTRAL_STREAMFS_H */
