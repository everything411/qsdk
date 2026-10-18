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

#ifndef _PPE_DRV_NPTV6_H
#define _PPE_DRV_NPTV6_H

struct ppe_drv_v6_conn_npt6;

/*
 * ppe_drv_nptv6_prefix
 *	Prefix information
 */
struct ppe_drv_nptv6_prefix {
	struct kref ref;	/* Reference count */
	uint8_t index;		/* Index into the Prefix table */
};

/*
 * ppe_drv_nptv6_iid
 *	IID table information
 */
struct ppe_drv_nptv6_iid {
	uint16_t index;		/* Index into the IID table */
};

struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_add_prefix_entry_ref(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_flow *pcr,
							struct ppe_drv_v6_conn_npt6 *npt6, bool is_flow);
bool ppe_drv_nptv6_prefix_entry_deref(struct ppe_drv_v6_conn_flow *flow, struct ppe_drv_nptv6_prefix *px);

struct ppe_drv_nptv6_iid *ppe_drv_nptv6_add_iid_entry(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_flow *pcr,
							struct ppe_drv_v6_conn_npt6 *npt6, bool is_flow);
bool ppe_drv_nptv6_del_iid_entry(struct ppe_drv_v6_conn_flow *flow, struct ppe_drv_nptv6_iid *iid);

void ppe_drv_nptv6_prefix_entries_free(struct ppe_drv_nptv6_prefix *pfx);
struct ppe_drv_nptv6_prefix *ppe_drv_nptv6_prefix_entries_alloc(void);

void ppe_drv_nptv6_iid_entries_free(struct ppe_drv_nptv6_iid *iid);
struct ppe_drv_nptv6_iid *ppe_drv_nptv6_iid_entries_alloc(void);
#endif /* _PPE_DRV_NPTV6_H */
