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

#define PPE_DRV_TUN_GRE_UDF_IDX_CSUM 0			/* GRE CSUM fields UDF index */
#define PPE_DRV_TUN_GRE_UDF_OFFSET_CSUM 0		/* GRE CSUM offset */

#define PPE_DRV_TUN_GRE_ACL_VAL_CSUM_DIS 0x0000		/* GRE CSUM disable check ACL value to match */
#define PPE_DRV_TUN_GRE_ACL_VPID_CSUM_DIS 0x01		/* GRE CSUM disable check ACL vp group id */

#define PPE_DRV_TUN_GRE_ACL_VAL_CSUM_EN 0x8000		/* GRE CSUM enable check ACL value to match */
#define PPE_DRV_TUN_GRE_ACL_VPID_CSUM_EN 0x02		/* GRE CSUM enable check ACL vp group id */

#define PPE_DRV_TUN_GRE_ACL_CSUM_MASK 0x8000		/* GRE CSUM bit mask */
#define PPE_DRV_TUN_GRE_ACL_UDF_PROFILE_MASK 0x7	/* GRE CSUM udf profile mask */

/*
 * ppe_drv_tun_gre_acl
 *	Global GRE ACL rule.
 */
struct ppe_drv_tun_gre_acl {
	struct ppe_drv_tun_udf *udf;		/* UDF profile object assocaited to GRE header */
	struct kref ref;			/* Reference handling object */
	uint16_t acl_id_en;			/* GRE CSUM Enable ACL ID */
	uint16_t acl_id_dis;			/* GRE CSUM Disable ACL ID */
	uint8_t acl_vpid_en;			/* GRE CSUM Enable VP group ID */
	uint8_t acl_vpid_dis;			/* GRE CSUM Disable VP grouup ID */
};

struct ppe_drv_tun_gre_acl *ppe_drv_tun_gre_acl_alloc(struct ppe_drv *p);
bool ppe_drv_tun_gre_acl_config(struct ppe_drv_tun_gre_acl *gre);
struct ppe_drv_tun_gre_acl *ppe_drv_tun_gre_acl_ref(struct ppe_drv_tun_gre_acl *gre);
bool ppe_drv_tun_gre_acl_deref(struct ppe_drv_tun_gre_acl *gre);
uint8_t ppe_drv_tun_gre_acl_get_vpid(struct ppe_drv_tun_gre_acl *gre, bool en);
