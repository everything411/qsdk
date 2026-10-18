/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv.h"
#include "ppe_drv_nptv6.h"

/*
 * ppe_drv_nptv6_hairpin_alloc()
 *	Allocate memory for nptv6 hairpin context.
 */
static struct ppe_drv_nptv6_hairpin_ctx *ppe_drv_nptv6_hairpin_alloc(void)
{
	return kzalloc(sizeof(struct ppe_drv_nptv6_hairpin_ctx), GFP_ATOMIC);
}

/*
 * ppe_drv_nptv6_hairpin_free()
 *	FREE the hairpin context.
 */
static void ppe_drv_nptv6_hairpin_free(struct kref *kref)
{
	struct ppe_drv_nptv6_hairpin_ctx *npt6_hp= container_of(kref, struct ppe_drv_nptv6_hairpin_ctx, ref);
	ppe_drv_l3_if_deref(npt6_hp->l3_if);
	list_del(&npt6_hp->list);
	kfree(npt6_hp);
}

/*
 * ppe_drv_nptv6_hairpin_ref()
 *	Reference nptv6 hairpin context
 */
struct ppe_drv_nptv6_hairpin_ctx *ppe_drv_nptv6_hairpin_ref(struct ppe_drv_nptv6_hairpin_ctx *npt6_hp)
{
	kref_get(&npt6_hp->ref);
	return npt6_hp;
}

/*
 * ppe_drv_nptv6_hairpin_deref()
 *	Let go of reference on npt6_hp.
 */
bool ppe_drv_nptv6_hairpin_deref(struct ppe_drv_nptv6_hairpin_ctx *npt6_hp)
{
	if (kref_put(&npt6_hp->ref, ppe_drv_nptv6_hairpin_free)) {
		ppe_drv_trace("%p: reference goes down to 0 for l3_if\n", npt6_hp);
		return true;
	}

	return false;
}

/*
 * ppe_drv_nptv6_hairpin_create()
 *	Get reference on nptv6 hairpin context or else create it.
 */
struct ppe_drv_nptv6_hairpin_ctx *ppe_drv_nptv6_hairpin_context_create_and_ref(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_npt6 *npt6)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface = NULL;
	struct ppe_drv_nptv6_hairpin_ctx *npt6_hp;
	struct ppe_drv_l3_if *l3_if = NULL;
	uint8_t mac_addr[6];
	uint32_t pfx[4];
	uint16_t index;
	int status;
	uint32_t mtu, mru;

	iface = ppe_drv_v6_conn_flow_eg_l3_if_get(pcf);
	if (!iface) {
		ppe_drv_warn("%px: Invalid Egress L3 Interface pointer\n", pcf);
		return NULL;
	}

	/*
	 * If the matching npt6 hairpin context is already present, return it
	 */
	list_for_each_entry(npt6_hp, &iface->npt6_hp, list) {
		if (!npt6_hp->l3_if) {
			ppe_drv_warn("%px: Invalid l3_if in npt6 context\n", pcf);
			return NULL;
		}

		index = npt6_hp->l3_if->l3_if_index;
		memset(pfx, 0, sizeof(pfx));
		if ((pcf->flags & PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW) == PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW) {
			memcpy(pfx, npt6->dst_pfx, sizeof(npt6->dst_pfx));
		} else {
			memcpy(pfx, npt6->src_pfx, sizeof(npt6->src_pfx));
		}

		if (ppe_drv_v6_addr_equal(npt6_hp->pfx, pfx)) {
			ppe_drv_nptv6_hairpin_ref(npt6_hp);
			ppe_drv_info("%p, Adhoc nptv6 context already exists : %u", iface, index);
			return npt6_hp;
		}
	}

	/*
	 * Getting MTU, MRU and MAC values from the existing l3_if and assigning the same to new l3_if
	 */
	mtu = iface->l3->mtu;
	mru = iface->l3->mtu;

	memcpy(mac_addr, &iface->l3->eg_mac_addr, sizeof(iface->l3->eg_mac_addr));
	ppe_drv_trace("%p: MTU : %u, MRU : %u, MAC address  is %pM", p, mtu, mru, mac_addr);

	l3_if = ppe_drv_l3_if_alloc(PPE_DRV_L3_IF_TYPE_PORT);
	if (!l3_if) {
		ppe_drv_warn("%p: failed to allocate l3 interface since no l3 is available", p);
		goto fail;
	}

	status = ppe_drv_l3_if_mtu_mru_set(l3_if, mtu, mru);
	if (!status) {
		ppe_drv_warn("%p: Failed to set mtu and mru\n", p);
		goto fail;
	}

	status = ppe_drv_l3_if_eg_mac_addr_set(l3_if, mac_addr);
	if (!status) {
		ppe_drv_warn("%p: Failed to set mac addr\n", p);
		goto fail;
	}

	npt6_hp = ppe_drv_nptv6_hairpin_alloc();
	if (!npt6_hp) {
		ppe_drv_warn("%p: Failed to allocate l2_ctx\n", p);
		goto fail;
	}

	npt6_hp->l3_if = l3_if;

	memset(npt6_hp->pfx, 0, sizeof(npt6_hp->pfx));
	if ((pcf->flags & PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW) == PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW) {
		memcpy(npt6_hp->pfx, npt6->dst_pfx, sizeof(npt6->dst_pfx));
	} else {
		memcpy(npt6_hp->pfx, npt6->src_pfx, sizeof(npt6->src_pfx));
	}

	kref_init(&npt6_hp->ref);
	list_add(&npt6_hp->list, &iface->npt6_hp);

	return npt6_hp;

fail:
	if (l3_if) {
		ppe_drv_l3_if_deref(l3_if);
	}

	return NULL;
}

/*
 * ppe_drv_nptv6_hairpin_destroy()
 *	Destroy nptv6 Hairpin context.
 */
int ppe_drv_nptv6_hairpin_destroy(struct ppe_drv_v6_conn_flow *pcf)
{
	struct ppe_drv_nptv6_hairpin_ctx *npt6_hp= pcf->npt6_hp;

	if (!npt6_hp) {
		ppe_drv_warn("%px: Invalid l3_nptv6 pointer for this flow\n", pcf);
		return -EINVAL;
	}

	ppe_drv_nptv6_hairpin_deref(npt6_hp);
	return 0;
}

/*
 * ppe_drv_nptv6_hairpin_prefix_free()
 *	FREE the prefix entry in PPE.
 */
static void ppe_drv_nptv6_hairpin_prefix_free(struct kref *kref)
{
	struct ppe_drv_nptv6_prefix *pfx = container_of(kref, struct ppe_drv_nptv6_prefix, ref);

	fal_flow_npt66_prefix_del(PPE_DRV_SWITCH_ID, pfx->index);
}

/*
 * ppe_drv_nptv6_hairpin_prefix_ref()
 *	Reference PPE prefix Index
 */
static struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_hairpin_prefix_ref(struct ppe_drv_nptv6_prefix *pfx)
{
	kref_get(&pfx->ref);
	return pfx;
}

/*
 * ppe_drv_nptv6_hairpin_prefix_deref()
 *	Let go of reference on pfx.
 */
static bool ppe_drv_nptv6_hairpin_prefix_deref(struct ppe_drv_nptv6_prefix *pfx)
{
	if (kref_put(&pfx->ref, ppe_drv_nptv6_hairpin_prefix_free)) {
		ppe_drv_trace("%p: reference goes down to 0 for pfx\n", pfx);
		return true;
	}

	return false;
}

/*
 * ppe_drv_nptv6_hairpin_add_prefix_entry_ref()
 *	Add an entry into the prefix table
 */
struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_hairpin_add_prefix_entry_ref(struct ppe_drv_v6_conn_flow *pcf,
				struct ppe_drv_v6_conn_npt6 *npt6, bool hairpin_flow)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_ip6_addr_t trans_ip = { 0 };
	struct ppe_drv_nptv6_prefix *pfx;
	struct ppe_drv_nptv6_hairpin_ctx *npt6_hp;
	struct ppe_drv_l3_if *l3_if;
	uint16_t l3_if_index;
	uint32_t pfx_len;
	uint32_t max_pfx_len;
	sw_error_t err;

	max_pfx_len = max(npt6->src_pfx_len, npt6->dst_pfx_len);

	npt6_hp = pcf->npt6_hp;
	if (!npt6_hp) {
		ppe_drv_warn("%px: npt6 hairpin context is NULL\n", pcf);
		return NULL;
	}

	l3_if = npt6_hp->l3_if;
	if (!l3_if) {
		ppe_drv_warn("%px: l3_if pointer is NULL\n", pcf);
		return NULL;
	}

	l3_if_index = ppe_drv_l3_if_get_index(l3_if);
	pfx = &p->pfx[l3_if_index];
	if (kref_read(&pfx->ref)) {
		/*
		 * Fetch and Take reference on the existing
		 * flow Prefix pointer.
		 */
		ppe_drv_nptv6_hairpin_prefix_ref(pfx);
		return pfx;
	}

	ppe_drv_trace("Prefix entry not found for the index: %d\n", l3_if_index);

	if (!hairpin_flow) {
		/*
		 * Add an entry into the prefix table for a non-hairpin flow
		 */
		memcpy(trans_ip.ul, npt6->dst_pfx, sizeof(npt6->dst_pfx));
	} else {
		/*
		 * Add An entry into the prefix table for a hairpin flow
		 */
		memcpy(trans_ip.ul, npt6->src_pfx, sizeof(npt6->src_pfx));
	}

	pfx_len = max_pfx_len;
	err = fal_flow_npt66_prefix_add(PPE_DRV_SWITCH_ID, l3_if_index, &trans_ip, pfx_len);
	if (err != SW_OK) {
		ppe_drv_warn("%px: Failed to Add the Prefix Entry for the index: %d\n", pcf, l3_if_index);
		return NULL;
	}

	/*
	 * Initialize pfx reference
	 */
	ppe_drv_trace("Prefix entry added for the index for SIP %pI6: %d\n", pcf->match_src_ip, l3_if_index);
	kref_init(&pfx->ref);
	return pfx;
}

/*
 * ppe_drv_nptv6_hairpin_prefix_entry_deref()
 *	Release a reference on an entry from the prefix table.
 */
bool ppe_drv_nptv6_hairpin_prefix_entry_deref(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_nptv6_prefix *px)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_nptv6_prefix *pfx;
	struct ppe_drv_l3_if *l3_if;
	uint16_t l3_if_index;

	if (!pcf) {
		ppe_drv_warn("%px: Invalid pointer for the flow\n", p);
		return false;
	}

	/*
	 * Release the reference on the Prefix pointer.
	 */
	l3_if = pcf->npt6_hp->l3_if;
	if (!l3_if) {
		ppe_drv_warn("%px: l3_if pointer is NULL\n", pcf);
		return NULL;
	}

	l3_if_index = l3_if->l3_if_index;
	pfx = &p->pfx[l3_if_index];
	ppe_drv_nptv6_hairpin_prefix_deref(pfx);

	px = NULL;
	return true;
}

/*
 * ppe_drv_nptv6_hairpin_add_iid_entry()
 *	Add an entry into the IID table.
 */
struct ppe_drv_nptv6_iid *ppe_drv_nptv6_hairpin_add_iid_entry(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_npt6 *npt6, bool dnat_flow)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_flow_npt66_iid_calc_t iid_cal = {0};
	fal_flow_npt66_iid_t iid = {0};
	struct ppe_drv_nptv6_iid *iid_entry;
	struct ppe_drv_nptv6_prefix *pfx;
	uint16_t flow_index;
	sw_error_t ret;

	if (dnat_flow) {
		flow_index = pcf->pf->index;
		/*
		 * Allowed flow_index range is double that of iid_entry,
		 * as flow_index is used for both v4 and v6
		 */
		iid_entry = &p->iid[flow_index/2];

		ppe_drv_trace("%p: Add IID entry for the index: %d\n", pcf, flow_index/2);

		/*
		 * Add the entry into the IID table.
		 */
		iid_cal.prefix_len = npt6->dst_pfx_len;
		iid_cal.tip_prefix_len = npt6->src_pfx_len;
		iid_cal.is_dnat = A_TRUE;
		memcpy(iid_cal.dip.ul, pcf->match_dest_ip, sizeof(pcf->match_dest_ip));
		memcpy(iid_cal.tip.ul, pcf->xlate_dest_ip, sizeof(pcf->xlate_dest_ip));
		ret = fal_flow_npt66_iid_cal(PPE_DRV_SWITCH_ID, &iid_cal, &iid);
		if (ret != SW_OK) {
			ppe_drv_warn("%px: Failed to calculate IID fields for the IID index: %d\n", pcf, flow_index/2);
			return NULL;
		}

		ret = fal_flow_npt66_iid_add(PPE_DRV_SWITCH_ID, flow_index, &iid);
		if (ret != SW_OK) {
			ppe_drv_warn("%px: Failed to add IID entry for the index: %d\n", pcf, flow_index/2);
			return NULL;
		}

		/*
		 * Take Reference on the Prefix pointer
		 */
		pfx = npt6->pfx_return;
		ppe_drv_nptv6_hairpin_prefix_ref(pfx);
		return iid_entry;
	}

	flow_index = pcf->pf->index;
	iid_entry = &p->iid[flow_index/2];

	ppe_drv_trace("Add IID entry for the index: %d\n", flow_index/2);

	/*
	 * Add the entry into the IID table.
	 */
	iid_cal.prefix_len = npt6->src_pfx_len;
	iid_cal.tip_prefix_len = npt6->dst_pfx_len;
	iid_cal.is_dnat = A_FALSE;
	memcpy(iid_cal.sip.ul, pcf->match_src_ip, sizeof(pcf->match_src_ip));
	memcpy(iid_cal.tip.ul, pcf->xlate_src_ip, sizeof(pcf->xlate_src_ip));
	ret = fal_flow_npt66_iid_cal(PPE_DRV_SWITCH_ID, &iid_cal, &iid);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to calculate IID fields for the IID index: %d\n", pcf, flow_index/2);
		return NULL;
	}

	ret = fal_flow_npt66_iid_add(PPE_DRV_SWITCH_ID, flow_index, &iid);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to add IID entry for the index: %d\n", pcf, flow_index/2);
		return NULL;
	}

	/*
	 * Take Reference on the Prefix pointer
	 */
	pfx = npt6->pfx_flow;
	ppe_drv_nptv6_hairpin_prefix_ref(pfx);
	return iid_entry;

}

/*
 * ppe_drv_nptv6_hairpin_del_iid_entry()
 *	Delete an entry from the IID table.
 */
bool ppe_drv_nptv6_hairpin_del_iid_entry(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_nptv6_iid *iid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_nptv6_iid *iid_entry;
	struct ppe_drv_nptv6_prefix *pfx;
	uint16_t flow_index;
	uint16_t l3_if_index;
	sw_error_t err;

	if (!pcf) {
		ppe_drv_warn("%px: Invalid pointer to the flow\n", p);
		return false;
	}

	/*
	 * Delete the entry in the direction of flow.
	 */
	flow_index = pcf->pf->index;
	iid_entry = &p->iid[flow_index/2];

	l3_if = pcf->npt6_hp->l3_if;
	if (!l3_if) {
		ppe_drv_warn("%px: l3_if pointer is NULL\n", pcf);
		return NULL;
	}

	l3_if_index = l3_if->l3_if_index;

	err = fal_flow_npt66_iid_del(PPE_DRV_SWITCH_ID, flow_index);
	if (err != SW_OK) {
		ppe_drv_trace("%p: IID entry deletion failed", pcf);
		return false;
	}

	/*
	 * Release the reference on Prefix pointer
	 */
	pfx = &p->pfx[l3_if_index];
	ppe_drv_nptv6_hairpin_prefix_deref(pfx);
	iid = NULL;
	return true;
}

struct ppe_drv_nptv6_hairpin_ctx *ppe_drv_nptv6_hairpin_context_ref(struct ppe_drv_iface *iface, struct ppe_drv_v6_conn_npt6 *npt6, uint32_t flag)
{
	struct ppe_drv_nptv6_hairpin_ctx *npt6_hp;
	uint32_t pfx[4];
	uint16_t index;

	list_for_each_entry(npt6_hp, &iface->npt6_hp, list) {
		if (!npt6_hp->l3_if) {
			ppe_drv_warn("%p: Invalid l3_if in npt6 context\n", iface);
			return NULL;
		}

		index = npt6_hp->l3_if->l3_if_index;
		memset(pfx, 0, sizeof(pfx));
		if ((flag & PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW) == PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW) {
			memcpy(pfx, npt6->dst_pfx, sizeof(npt6->dst_pfx));
		} else {
			memcpy(pfx, npt6->src_pfx, sizeof(npt6->src_pfx));
		}

		if (ppe_drv_v6_addr_equal(npt6_hp->pfx, pfx)) {
			ppe_drv_nptv6_hairpin_ref(npt6_hp);
			ppe_drv_info("%p, Adhoc nptv6 context already exists : %u", iface, index);
			return npt6_hp;
		}
	}

	return NULL;
}
