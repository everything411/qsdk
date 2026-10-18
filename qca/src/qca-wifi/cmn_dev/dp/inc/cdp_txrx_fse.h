/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _CDP_TXRX_FSE_H_
#define _CDP_TXRX_FSE_H_

#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static inline QDF_STATUS
cdp_fse_flow_add(ol_txrx_soc_handle soc,
		 uint32_t *src_ip, uint32_t src_port,
		 uint32_t *dest_ip, uint32_t dest_port,
		 uint8_t protocol, uint8_t version, uint32_t svc_id,
		 uint32_t tid, uint8_t *dest_mac, uint8_t pdev_id, bool drop,
		 uint8_t ring_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->fse_ops ||
	    !soc->ops->fse_ops->fse_rule_add) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->fse_ops->fse_rule_add(soc,
					       src_ip, src_port,
					       dest_ip, dest_port,
					       protocol, version, svc_id, tid,
					       dest_mac, pdev_id, drop,
					       ring_id);
}

static inline QDF_STATUS
cdp_fse_flow_delete(ol_txrx_soc_handle soc,
		    uint32_t *src_ip, uint32_t src_port,
		    uint32_t *dest_ip, uint32_t dest_port,
		    uint8_t protocol, uint8_t version, uint8_t pdev_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->fse_ops ||
	    !soc->ops->fse_ops->fse_rule_delete) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->fse_ops->fse_rule_delete(soc,
						  src_ip, src_port,
						  dest_ip, dest_port,
						  protocol, version, pdev_id);
}

static inline QDF_STATUS
cdp_fse_dump_flow_entries(ol_txrx_soc_handle soc,
			  uint8_t pdev_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->fse_ops || !soc->ops->fse_ops->fse_rule_dump)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->fse_ops->fse_rule_dump(soc, pdev_id);
}

#endif /* WLAN_SUPPORT_RX_FLOW_TAG */
#endif /* _CDP_TXRX_FSE_H_ */
