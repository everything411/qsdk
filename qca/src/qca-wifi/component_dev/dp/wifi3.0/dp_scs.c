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

#include "dp_peer.h"
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_scs.h"

#define DP_SCS_RULE_PEER_ID_MASK  0x0FFFF000
#define DP_SCS_RULE_PEER_ID_SHIFT 12

/**
 * dp_scs_fetch_peer_id_frm_rule_id - Fetch peer_id from SCS rule_id
 *
 * rule_id: uint32_t SCS rule_id
 *
 * Return: peer_id of type uint16_t
 */
static inline uint16_t dp_scs_fetch_peer_id_frm_rule_id(uint32_t rule_id)
{
	return ((rule_id & DP_SCS_RULE_PEER_ID_MASK) >> DP_SCS_RULE_PEER_ID_SHIFT);
}

/**
 * dp_scs_peer_lookup_n_rule_match() - Fetch peer using mac_addr and check if
 *     SCS rule is applicable for that peer or not
 *
 * @soc_hdl: soc handle
 * @rule_id: SCS rule_id
 * @dst_mac_addr: mac addr for peer lookup
 *
 * Return: bool true on success and false on failure
 */
bool dp_scs_peer_lookup_n_rule_match(struct cdp_soc_t *soc_hdl,
				     uint32_t rule_id, uint8_t *dst_mac_addr)
{
	bool status = false;
	uint16_t scs_peer_id = 0;
	struct dp_peer *peer = NULL;
	struct dp_vdev *vdev = NULL;
	struct cdp_soc_t *cdp_soc = NULL;
	struct dp_soc *dpsoc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dpsoc) {
		QDF_TRACE(QDF_MODULE_ID_SCS, QDF_TRACE_LEVEL_ERROR,
				"%s: Invalid soc\n", __func__);
		return false;
	}

	cdp_soc = &dpsoc->cdp_soc;

	peer = dp_find_peer_by_macaddr(dpsoc, dst_mac_addr, DP_VDEV_ALL,
								DP_MOD_ID_SCS);
	if (!peer)
		return false;

	/* Fetch peer_id from scs rule_id */
	scs_peer_id = dp_scs_fetch_peer_id_frm_rule_id(rule_id);
	/* Check if scs_peer is same as connected_peer_id */
	if (peer->peer_id != scs_peer_id)
		return false;

	vdev = peer->vdev;
	if (!vdev) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SCS);
		return false;
	}

	if (cdp_soc->ol_ops->peer_scs_rule_match)
		status = cdp_soc->ol_ops->peer_scs_rule_match(dpsoc->ctrl_psoc,
							      vdev->vdev_id,
							      rule_id,
							      peer->mac_addr.raw);

	dp_peer_unref_delete(peer, DP_MOD_ID_SCS);

	return status;
}

qdf_export_symbol(dp_scs_peer_lookup_n_rule_match);
