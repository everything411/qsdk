/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_NPTV6_HAIRPIN_H
#define _PPE_DRV_NPTV6_HAIRPIN_H

#define PPE_DRV_L3_IF_NPTV6_PFX_LEN	4

/*
 * ppe_drv_nptv6_hairpin_ctx
 *	PPE l3_if Adhoc context information
 */
struct ppe_drv_nptv6_hairpin_ctx {
	/*
	 * list head for active list
	 */
	struct list_head list;		/* List head for active list */
	struct ppe_drv_l3_if *l3_if;	/* Pointer to store adhoc l3_if context */
	uint32_t pfx[PPE_DRV_L3_IF_NPTV6_PFX_LEN];		/* Prefix for adhoc l3_if */
	struct kref ref;		/* Reference count */
};

struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_hairpin_add_prefix_entry_ref(struct ppe_drv_v6_conn_flow *flow_ptr, struct ppe_drv_v6_conn_npt6 *npt6, bool is_flow);
bool ppe_drv_nptv6_hairpin_prefix_entry_deref(struct ppe_drv_v6_conn_flow *flow_ptr, struct ppe_drv_nptv6_prefix *px);
struct ppe_drv_nptv6_iid *ppe_drv_nptv6_hairpin_add_iid_entry(struct ppe_drv_v6_conn_flow *flow_ptr, struct ppe_drv_v6_conn_npt6 *npt6, bool is_dnat);
bool ppe_drv_nptv6_hairpin_del_iid_entry(struct ppe_drv_v6_conn_flow *flow_ptr, struct ppe_drv_nptv6_iid *iid);
struct ppe_drv_nptv6_hairpin_ctx *ppe_drv_nptv6_hairpin_context_create_and_ref(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_npt6 *npt6);
int ppe_drv_nptv6_hairpin_destroy(struct ppe_drv_v6_conn_flow *flow_ptr);
struct ppe_drv_nptv6_hairpin_ctx *ppe_drv_nptv6_hairpin_ref(struct ppe_drv_nptv6_hairpin_ctx *npt6_hp);
bool ppe_drv_nptv6_hairpin_deref(struct ppe_drv_nptv6_hairpin_ctx *npt6_hp);

#endif /* _PPE_DRV_NPTV6_HAIRPIN_H */
