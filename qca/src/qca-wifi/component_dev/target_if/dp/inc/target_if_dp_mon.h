/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_TARGET_IF_DP_MON_H_
#define _WLAN_TARGET_IF_DP_MON_H_

#include <qdf_types.h>
#include <qdf_status.h>
#include <wmi_unified_priv.h>
#include <wlan_objmgr_psoc_obj.h>
#include <target_if.h>
#include <cdp_txrx_ops.h>

#ifdef QCA_SUPPORT_LITE_MONITOR
int target_if_config_lite_mon_peer(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
				   uint8_t vdev_id, enum cdp_nac_param_cmd cmd,
				   uint8_t *peer_mac);
int target_if_config_lite_mon_tx_peer(struct cdp_ctrl_objmgr_psoc *psoc,
				      uint8_t pdev_id, uint8_t vdev_id,
				      enum cdp_tx_filter_action cmd,
				      uint8_t *peer_mac);
#endif
#endif /* _WLAN_TARGET_IF_DP_MON_H_ */
