/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <fal/fal_ipmc.h>

#include "ppe_drv.h"

/*
 * ppe_drv_mcast_entry_fill()
 *	Fill multicast info.
 */
static void ppe_drv_mcast_entry_fill(struct ppe_drv_mcast_entry *entry,
			fal_ipmc_entry_t *ipmc_entry, bool is_add)
{
	if (entry->is_v4) {
		if (entry->sip_valid) {
			ipmc_entry->key_type = FAL_IPMC_KEY_TYPE_SIP_GIP;
			ipmc_entry->sip.ip4_addr = ntohl(entry->sip.v4);
		} else {
			ipmc_entry->key_type = FAL_IPMC_KEY_TYPE_GIP;
		}
		ipmc_entry->gip.ip4_addr = ntohl(entry->gip.v4);
	} else {
		if (entry->sip_valid) {
			ipmc_entry->key_type = FAL_IPMC_KEY_TYPE_SIPV6_GIPV6;
			ipmc_entry->sip.ip6_addr.ul[0] = ntohl(entry->sip.v6[0]);
			ipmc_entry->sip.ip6_addr.ul[1] = ntohl(entry->sip.v6[1]);
			ipmc_entry->sip.ip6_addr.ul[2] = ntohl(entry->sip.v6[2]);
			ipmc_entry->sip.ip6_addr.ul[3] = ntohl(entry->sip.v6[3]);
		} else {
			ipmc_entry->key_type = FAL_IPMC_KEY_TYPE_GIPV6;
		}
			ipmc_entry->gip.ip6_addr.ul[0] = ntohl(entry->gip.v6[0]);
			ipmc_entry->gip.ip6_addr.ul[1] = ntohl(entry->gip.v6[1]);
			ipmc_entry->gip.ip6_addr.ul[2] = ntohl(entry->gip.v6[2]);
			ipmc_entry->gip.ip6_addr.ul[3] = ntohl(entry->gip.v6[3]);
	}

	if (entry->vlan_valid) {
		ipmc_entry->vlan_valid = A_TRUE;
		ipmc_entry->vlan_mode = FAL_VLAN_MATCH_VID;
		ipmc_entry->vlan_id = entry->vid;
		ipmc_entry->vlan_fmt_check_en = A_TRUE;
		ipmc_entry->vlan_fmt = 1; /*0 = untagged; 1 = tagged or priority tag; used for VLAN check */
	}

	fal_ipmc_entry_get(0, FAL_IPMC_OP_MODE_HASH, ipmc_entry);

	ipmc_entry->fwd_cmd = FAL_MAC_FRWRD;
	ipmc_entry->dst_info.dest_info_type = FAL_DEST_INFO_PORT_BMP;

	if (is_add) {
		ipmc_entry->dst_info.dest_info_value |= 1 << entry->port;
	} else {
		ipmc_entry->dst_info.dest_info_value &= ~(1 << entry->port);
	}

	/*
	 * Display all ipmc_entry fields before calling SSDK API
	 */
	if (entry->is_v4) {
		ppe_drv_trace("%px: IPMC entry: key_type=%u sip=%pI4h gip=%pI4h vlan_valid=%u vlan_id=%u port_bmp=0x%x fwd_cmd=%u port=%u",
			&ppe_drv_gbl, ipmc_entry->key_type, &ipmc_entry->sip.ip4_addr, &ipmc_entry->gip.ip4_addr,
			entry->vlan_valid, ipmc_entry->vlan_id, ipmc_entry->dst_info.dest_info_value,
			ipmc_entry->fwd_cmd, entry->port);
	} else {
		ppe_drv_trace("%px: IPMC entry: key_type=%u sip=%pI6c gip=%pI6c vlan_valid=%u vlan_id=%u port_bmp=0x%x fwd_cmd=%u port=%u",
			&ppe_drv_gbl, ipmc_entry->key_type, &ipmc_entry->sip.ip6_addr, &ipmc_entry->gip.ip6_addr,
			entry->vlan_valid, ipmc_entry->vlan_id, ipmc_entry->dst_info.dest_info_value,
			ipmc_entry->fwd_cmd, entry->port);
	}
}

/*
 * ppe_drv_mcast_entry_delete()
 *	Deletes multicast entry in PPE.
 */
ppe_drv_ret_t ppe_drv_mcast_entry_delete(struct ppe_drv_mcast_entry *entry)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_ipmc_entry_t ipmc_entry = {0};

	ppe_drv_mcast_entry_fill(entry, &ipmc_entry, false);

	/*
	 * In case, this is the last entry, delete the entry
	 */
	if (ipmc_entry.dst_info.dest_info_value == 0) {
		spin_lock_bh(&p->lock);
		if (fal_ipmc_entry_del(0, FAL_IPMC_OP_MODE_HASH, &ipmc_entry) != 0) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%px:multicast entry deletion failed for port:%u", p, entry->port);
			return PPE_DRV_RET_MCAST_ENTRY_ADD_FAIL;
		}

		spin_unlock_bh(&p->lock);
		goto done;
	}

	spin_lock_bh(&p->lock);
	if (fal_ipmc_entry_add(0, FAL_IPMC_OP_MODE_HASH, &ipmc_entry) != 0) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:multicast entry deletion failed for port:%u", p, entry->port);
		return PPE_DRV_RET_MCAST_ENTRY_ADD_FAIL;
	}

	spin_unlock_bh(&p->lock);

done:
	ppe_drv_info("%px:multicast entry deletion successful for port:%u", p, entry->port);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_mcast_entry_delete);

/*
 * ppe_drv_mcast_entry_add()
 *	Adds multicast entry in PPE.
 */
ppe_drv_ret_t ppe_drv_mcast_entry_add(struct ppe_drv_mcast_entry *entry)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_ipmc_entry_t ipmc_entry = {0};

	ppe_drv_mcast_entry_fill(entry, &ipmc_entry, true);

	spin_lock_bh(&p->lock);
	if (fal_ipmc_entry_add(0, FAL_IPMC_OP_MODE_HASH, &ipmc_entry) != 0) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px:multicast entry addition failed for port:%u", p, entry->port);
		return PPE_DRV_RET_MCAST_ENTRY_ADD_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%px:multicast entry addition successful for port:%u", p, entry->port);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_mcast_entry_add);