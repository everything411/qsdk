/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "ppe_drv.h"

#ifdef CONFIG_NF_CONNTRACK_NPTV6_EXT
#include <net/netfilter/nf_conntrack_nptv6_ext.h>
#endif

/*
 * ppe_drv_nptv6_prefix_free()
 *	FREE the prefix entry in PPE.
 */
static void ppe_drv_nptv6_prefix_free(struct kref *kref)
{
	struct ppe_drv_nptv6_prefix *pfx = container_of(kref, struct ppe_drv_nptv6_prefix, ref);

	fal_flow_npt66_prefix_del(PPE_DRV_SWITCH_ID, pfx->index);
}

/*
 * ppe_drv_nptv6_prefix_ref()
 *	Reference PPE prefix Index
 */
static struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_prefix_ref(struct ppe_drv_nptv6_prefix *pfx)
{
	kref_get(&pfx->ref);
	return pfx;
}

/*
 * ppe_drv_nptv6_prefix_deref()
 *	Let go of reference on pfx.
 */
static bool ppe_drv_nptv6_prefix_deref(struct ppe_drv_nptv6_prefix *pfx)
{
	if (kref_put(&pfx->ref, ppe_drv_nptv6_prefix_free)) {
		ppe_drv_trace("%p: reference goes down to 0 for pfx\n", pfx);
		return true;
	}

	return false;
}

/*
 * ppe_drv_nptv6_add_prefix_entry_ref()
 *	Add an entry into the prefix table
 */
struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_add_prefix_entry_ref(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_flow *pcr,
				struct ppe_drv_v6_conn_npt6 *npt6, bool is_flow)
{
	struct ppe_drv *p = &ppe_drv_gbl;
	fal_ip6_addr_t trans_ip = { 0 };
	struct ppe_drv_nptv6_prefix *pfx;
	struct ppe_drv_iface *in_l3_if = NULL;
	uint32_t l3_if;
	uint32_t pfx_len;
	uint32_t max_pfx_len;
	sw_error_t err;

	max_pfx_len = max(npt6->src_pfx_len, npt6->dst_pfx_len);
	if (is_flow) {
		in_l3_if = ppe_drv_v6_conn_flow_in_l3_if_get(pcr);
		if (!in_l3_if) {
			ppe_drv_warn("%px: Invalid Egress L3 Interface pointer\n", pcr);
			return NULL;
		}

		l3_if = ppe_drv_l3_if_get_index(in_l3_if->l3);
		pfx = &p->pfx[l3_if];
		if (kref_read(&pfx->ref)) {
			/*
			 * Fetch and Take reference on the existing
			 * flow Prefix pointer.
			 */
			ppe_drv_nptv6_prefix_ref(pfx);
			return pfx;
		}

		ppe_drv_trace("Prefix entry not found for the index: %d\n", l3_if);

		/*
		 * Add An entry into the prefix table
		 */
		memcpy(trans_ip.ul, npt6->dst_pfx, sizeof(npt6->dst_pfx));
		pfx_len = max_pfx_len;
		err = fal_flow_npt66_prefix_add(PPE_DRV_SWITCH_ID, l3_if, &trans_ip, pfx_len);
		if (err != SW_OK) {
			ppe_drv_warn("%px: Failed to Add the Prefix Entry for the index: %d\n", pcr, l3_if);
			return NULL;
		}

		/*
		 * Fetch and take the reference on the new
		 * flow Prefix pointer.
		 */
		kref_init(&pfx->ref);
		return pfx;
	}

	in_l3_if = NULL;
	in_l3_if = ppe_drv_v6_conn_flow_in_l3_if_get(pcf);
	if (!in_l3_if) {
		ppe_drv_warn("%px: Invalid Igress L3 Interface pointer\n", pcf);
		return NULL;
	}

	l3_if = ppe_drv_l3_if_get_index(in_l3_if->l3);
	pfx = &p->pfx[l3_if];
	if (kref_read(&pfx->ref)) {
		/*
		 * Fetch and take the reference on existing
		 * return prefix pointer.
		 */
		ppe_drv_nptv6_prefix_ref(pfx);
		return pfx;
	}

	ppe_drv_trace("Prefix entry not found for the index: %d\n", l3_if);

	/*
	 * Add An entry into the prefix table
	 */
	memcpy(trans_ip.ul, npt6->src_pfx, sizeof(npt6->src_pfx));
	pfx_len = max_pfx_len;
	err = fal_flow_npt66_prefix_add(PPE_DRV_SWITCH_ID, l3_if, &trans_ip, pfx_len);
	if (err != SW_OK) {
		ppe_drv_warn("%px: Failed to Add the Prefix Entry for the index: %d\n", pcf, l3_if);
		return NULL;
	}

	/*
	 * Fetch and take the reference on new
	 * return Prefix pointer.
	 */
	kref_init(&pfx->ref);
	return pfx;
}

/*
 * ppe_drv_nptv6_prefix_entry_deref()
 *	Release a reference on an entry from the prefix table.
 */
bool ppe_drv_nptv6_prefix_entry_deref(struct ppe_drv_v6_conn_flow *flow, struct ppe_drv_nptv6_prefix *px)
{
	struct ppe_drv *p = &ppe_drv_gbl;
	struct ppe_drv_iface *in_l3_if = NULL;
	struct ppe_drv_nptv6_prefix *pfx;
	uint32_t l3_if;

	if (!flow) {
		ppe_drv_warn("%px: Invalid pointer for the flow\n", p);
		return false;
	}

	in_l3_if = ppe_drv_v6_conn_flow_in_l3_if_get(flow);
	if (!in_l3_if) {
		ppe_drv_warn("%px: Invalid L3 Interface pointer for the flow\n", flow);
		return false;
	}

	/*
	 * Release the reference on the Prefix pointer.
	 */
	l3_if = ppe_drv_l3_if_get_index(in_l3_if->l3);
	pfx = &p->pfx[l3_if];
	ppe_drv_nptv6_prefix_deref(pfx);

	px = NULL;
	return true;
}

/*
 * ppe_drv_nptv6_add_iid_entry()
 *	Add an entry into the IID table.
 */
struct ppe_drv_nptv6_iid *ppe_drv_nptv6_add_iid_entry(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_flow *pcr,
						struct ppe_drv_v6_conn_npt6 *npt6, bool is_flow)
{
	struct ppe_drv *p = &ppe_drv_gbl;
	fal_flow_npt66_iid_calc_t iid_cal = {0};
	fal_flow_npt66_iid_t iid = {0};
	struct ppe_drv_nptv6_iid *iid_entry;
	struct ppe_drv_nptv6_prefix *pfx;
	uint16_t flow_index;
	sw_error_t ret;

	if (npt6->nptv6_flags & NF_CT_NPTV6_EXT_SNPT) {
		if (is_flow) {
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
			pfx = npt6->pfx_return;
			ppe_drv_nptv6_prefix_ref(pfx);
			return iid_entry;
		}

		flow_index = pcr->pf->index;
		memset(&iid, 0, sizeof(fal_flow_npt66_iid_t));
		iid_entry = &p->iid[flow_index/2];

		ppe_drv_trace("Add IID entry for the index: %d\n", flow_index/2);

		/*
		 * Add the entry into the IID table.
		 */
		iid_cal.prefix_len = npt6->dst_pfx_len;
		iid_cal.tip_prefix_len = npt6->src_pfx_len;
		iid_cal.is_dnat = A_TRUE;
		memcpy(iid_cal.dip.ul, pcr->match_dest_ip, sizeof(pcr->match_dest_ip));
		memcpy(iid_cal.tip.ul, pcr->xlate_dest_ip, sizeof(pcr->xlate_dest_ip));
		ret = fal_flow_npt66_iid_cal(PPE_DRV_SWITCH_ID, &iid_cal, &iid);
		if (ret != SW_OK) {
			ppe_drv_warn("%px: Failed to calculate IID fields for the IID index: %d\n", pcr, flow_index/2);
			return NULL;
		}

		ret = fal_flow_npt66_iid_add(PPE_DRV_SWITCH_ID, flow_index, &iid);
		if (ret != SW_OK) {
			ppe_drv_warn("%px: Failed to add IID entry for the index: %d\n", pcr, flow_index/2);
			return NULL;
		}

		/*
		 * Take Reference on the Prefix pointer
		 */
		pfx = npt6->pfx_flow;
		ppe_drv_nptv6_prefix_ref(pfx);
		return iid_entry;
	}

	if (npt6->nptv6_flags & NF_CT_NPTV6_EXT_DNPT) {
		if (is_flow) {
			flow_index = pcf->pf->index;
			iid_entry = &p->iid[flow_index/2];

			ppe_drv_trace("Add IID entry for the index: %d\n", flow_index/2);

			/*
			 * Add the entry into the IID table.
			 */
			iid_cal.prefix_len = npt6->src_pfx_len;
			iid_cal.tip_prefix_len = npt6->dst_pfx_len;
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
			ppe_drv_nptv6_prefix_ref(pfx);
			return iid_entry;
		}

		flow_index = pcr->pf->index;
		memset(&iid, 0, sizeof(fal_flow_npt66_iid_t));
		iid_entry = &p->iid[flow_index/2];

		ppe_drv_trace("Add IID entry for the index: %d\n", flow_index/2);

		/*
		 * Add the entry into the IID table.
		 */
		iid_cal.prefix_len = npt6->dst_pfx_len;
		iid_cal.tip_prefix_len = npt6->src_pfx_len;
		iid_cal.is_dnat = A_FALSE;
		memcpy(iid_cal.sip.ul, pcr->match_src_ip, sizeof(pcr->match_src_ip));
		memcpy(iid_cal.tip.ul, pcr->xlate_src_ip, sizeof(pcr->xlate_src_ip));
		ret = fal_flow_npt66_iid_cal(PPE_DRV_SWITCH_ID, &iid_cal, &iid);
		if (ret != SW_OK) {
			ppe_drv_warn("%px: Failed to calculate IID fields for the IID index: %d\n", pcr, flow_index/2);
			return NULL;
		}

		ret = fal_flow_npt66_iid_add(PPE_DRV_SWITCH_ID, flow_index, &iid);
		if (ret != SW_OK) {
			ppe_drv_warn("%px: Failed to add IID entry for the index: %d\n", pcr, flow_index/2);
			return NULL;
		}

		/*
		 * Take Reference on the Prefix pointer
		 */
		pfx = npt6->pfx_flow;
		ppe_drv_nptv6_prefix_ref(pfx);
		return iid_entry;
	}

	ppe_drv_trace("Packet doesn't belong to NPTv6 flows\n");
	return NULL;
}

/*
 * ppe_drv_nptv6_del_iid_entry()
 *	Delete an entry from the IID table.
 */
bool ppe_drv_nptv6_del_iid_entry(struct ppe_drv_v6_conn_flow *flow, struct ppe_drv_nptv6_iid *iid)
{
	struct ppe_drv *p = &ppe_drv_gbl;
	struct ppe_drv_iface *in_l3_if = NULL;
	struct ppe_drv_nptv6_iid *iid_entry;
	struct ppe_drv_nptv6_prefix *pfx;
	uint16_t flow_index;
	uint32_t l3_if;
	sw_error_t err;

	if (!flow) {
		ppe_drv_warn("%px: Invalid pointer to the flow\n", p);
		return false;
	}

	in_l3_if = ppe_drv_v6_conn_flow_in_l3_if_get(flow);
	if (!in_l3_if) {
		ppe_drv_warn("%px: Invalid L3 Interface pointer for the flow\n", flow);
		return false;
	}

	/*
	 * Delete the entry in the direction of flow.
	 */
	flow_index = flow->pf->index;
	iid_entry = &p->iid[flow_index/2];
	l3_if = ppe_drv_l3_if_get_index(in_l3_if->l3);
	err = fal_flow_npt66_iid_del(PPE_DRV_SWITCH_ID, flow_index);
	if (err != SW_OK) {
		ppe_drv_trace("%p: IID entry deletion failed", flow);
		return false;
	}

	/*
	 * Release the reference on Prefix pointer
	 */
	pfx = &p->pfx[l3_if];
	ppe_drv_nptv6_prefix_deref(pfx);

	iid = NULL;
	return true;
}

/*
 * ppe_drv_nptv6_prefix_entries_free()
 *	Free prefix table entries if it was allocated.
 */
void ppe_drv_nptv6_prefix_entries_free(struct ppe_drv_nptv6_prefix *pfx)
{
	vfree(pfx);
}

/*
 * ppe_drv_nptv6_prefix_entries_alloc()
 *	Allocated and initialize requested number of prefix table entries.
 */
struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_prefix_entries_alloc()
{
	uint16_t i;
	struct ppe_drv_nptv6_prefix *pfx;
	struct ppe_drv *p = &ppe_drv_gbl;

	pfx = vzalloc(sizeof(struct ppe_drv_nptv6_prefix) * p->prefix_num);
	if (!pfx) {
		ppe_drv_warn("%p: failed to allocate prefix Table entries", p);
		return NULL;
	}

	/*
	 * Assign prefix index values to the prefix entries
	 */
	for (i = 0; i < p->prefix_num; i++) {
		pfx[i].index = i;
	}

	return pfx;
}

/*
 * ppe_drv_nptv6_iid_entries_free()
 *	Free iid table entries if it was allocated.
 */
void ppe_drv_nptv6_iid_entries_free(struct ppe_drv_nptv6_iid *iid)
{
	vfree(iid);
}

/*
 * ppe_drv_nptv6_iid_entries_alloc()
 *	Allocated and initialize requested number of IID table entries.
 */
struct ppe_drv_nptv6_iid *ppe_drv_nptv6_iid_entries_alloc()
{
	uint16_t i;
	struct ppe_drv_nptv6_iid *iid;
	struct ppe_drv *p = &ppe_drv_gbl;

	iid = vzalloc(sizeof(struct ppe_drv_nptv6_iid) * p->iid_num);
	if (!iid) {
		ppe_drv_warn("%p: failed to allocate IID entries", p);
		return NULL;
	}

	/*
	 * Assign IID index values to the IID entries
	 */
	for (i = 0; i < p->iid_num; i++) {
		iid[i].index = i;
	}

	return iid;
}
