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

#include <net/gre.h>
#include <linux/if_tunnel.h>
#include <linux/version.h>
#include <fal_tunnel.h>
#include <ppe_drv/ppe_drv.h>
#include "ppe_drv_tun.h"

/*
 * ppe_drv_tun_gre_acl_free
 *	Deinit Pre-ACL GRE rules and free the gre object
 */
static void ppe_drv_tun_gre_acl_free(struct kref *kref)
{
	struct ppe_drv_tun_gre_acl *gre = container_of(kref, struct ppe_drv_tun_gre_acl, ref);
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t error;

	if (gre->acl_vpid_en) {
		error = fal_acl_list_unbind(PPE_DRV_SWITCH_ID, gre->acl_id_en, FAL_ACL_DIREC_IN,
			FAL_ACL_BIND_TUNNEL_VP_GROUP, gre->acl_vpid_en);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Failed to unbind acl id %d from vp group %d\n", gre, gre->acl_id_en, gre->acl_vpid_en);
		}
	}

	if (gre->acl_id_en) {
		error = fal_acl_rule_delete(PPE_DRV_SWITCH_ID, gre->acl_id_en, PPE_DRV_ACL_RULE_ID, PPE_DRV_ACL_RULE_NR);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Failed to delete acl rule %d from list %d\n", gre, PPE_DRV_ACL_RULE_ID, gre->acl_id_en);
		}

		error = fal_acl_list_destroy(PPE_DRV_SWITCH_ID, gre->acl_id_en);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Failed to destroy acl list %d\n", gre, gre->acl_id_en);
		}
	}

	if (gre->acl_vpid_dis) {
		error = fal_acl_list_unbind(PPE_DRV_SWITCH_ID, gre->acl_id_dis, FAL_ACL_DIREC_IN,
				FAL_ACL_BIND_TUNNEL_VP_GROUP, gre->acl_vpid_dis);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Failed to unbind acl id %d from vp group %d\n", gre, gre->acl_id_dis, gre->acl_vpid_dis);
		}
	}

	if (gre->acl_id_dis) {
		error = fal_acl_rule_delete(PPE_DRV_SWITCH_ID, gre->acl_id_dis, PPE_DRV_ACL_RULE_ID, PPE_DRV_ACL_RULE_NR);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Failed to delete acl rule %d from list %d\n", gre, PPE_DRV_ACL_RULE_ID, gre->acl_id_dis);
		}

		error = fal_acl_list_destroy(PPE_DRV_SWITCH_ID, gre->acl_id_dis);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Failed to destroy acl list %d\n", gre, gre->acl_id_dis);
		}
	}

	if(gre->udf)
		ppe_drv_tun_udf_entry_dref(gre->udf);

	kfree(gre);
	p->tun_gbl.gre = NULL;
}

/*
 * ppe_drv_tun_gre_acl_get_vpid
 *	API to get the VP group ID assocaited with the ACL rule
 */
uint8_t ppe_drv_tun_gre_acl_get_vpid(struct ppe_drv_tun_gre_acl *gre, bool en)
{
       	ppe_drv_assert(gre, "Invalid/NULL object");

	if (en) {
		return gre->acl_vpid_en;
	} else {
		return gre->acl_vpid_dis;
	}
}

/*
 * ppe_drv_tun_gre_acl_deref
 *	Dereference the global udf rule for gre csum.
 */
bool ppe_drv_tun_gre_acl_deref(struct ppe_drv_tun_gre_acl *gre)
{
        ppe_drv_assert(gre, "Invalid/NULL object");

        ppe_drv_assert(kref_read(&gre->ref), "%p: ref count underflow for GRE csum global object", gre);
        if (kref_put(&gre->ref, ppe_drv_tun_gre_acl_free)) {
                ppe_drv_trace("%p: Refount down to 0, freeing the global gre structure", gre);
                return true;
        }

        return false;
}

/*
 * ppe_drv_tun_gre_acl_ref
 *	Reference the global udf rule for gre csum.
 */
struct ppe_drv_tun_gre_acl *ppe_drv_tun_gre_acl_ref(struct ppe_drv_tun_gre_acl *gre)
{
        ppe_drv_assert(gre, "Invalid/NULL object");

	kref_get(&gre->ref);

        ppe_drv_assert(kref_read(&gre->ref), "%p: ref count rollover for GRE csum global object", gre);
        ppe_drv_trace("%p: Reference taken on GRE csum global object:%u", gre, kref_read(&gre->ref));

        return gre;
}

/*
 * ppe_drv_tun_gre_acl_alloc
 *	Allocate the gre acl object
 */
struct ppe_drv_tun_gre_acl *ppe_drv_tun_gre_acl_alloc(struct ppe_drv *p)
{
	struct ppe_drv_tun_gre_acl *gre = NULL;
	gre = kzalloc(sizeof(struct ppe_drv_tun_gre_acl), GFP_ATOMIC);
	if (!gre) {
		ppe_drv_warn("%p: Failed to allocate memory for global GRE csum object\n", p);
		return NULL;
	}

	kref_init(&gre->ref);
	return gre;
}

/*
 * ppe_drv_tun_gre_acl_config
 *	Configure ACL rules for GRE Checksum handling.
 *
 * TODO: Update PPE ACL driver to support udfprofiles and vport group
 * binding and use it instead of calling the ssdk API's directly.
 */
bool ppe_drv_tun_gre_acl_config(struct ppe_drv_tun_gre_acl *gre)
{
	struct ppe_drv_tun_udf_profile *udf_pf;
	fal_acl_rule_t acl_rule = {0};
	sw_error_t error;

	udf_pf = kzalloc(sizeof(struct ppe_drv_tun_udf_profile), GFP_ATOMIC);
	if (!udf_pf) {
		ppe_drv_warn("%p: Couldn't allocate memory for udf profile", udf_pf);

		/*
		 * TODO: It is not correct to do acl_deref here.
		 * clean up needed for acl_deref.
		 */
		ppe_drv_tun_gre_acl_deref(gre);
		return false;
	}

	/*
	 * The checksum flag is present in the first byte of the GRE header
	 * Configure tunnel UDF to match the first byte of the GRE header to
	 * "checksum present" bit.
	 */
	udf_pf->l4_match = true;
	udf_pf->l4_type = PPE_DRV_TUN_UDF_L4_TYPE_GRE;

	ppe_drv_tun_udf_bitmask_set(udf_pf, PPE_DRV_TUN_GRE_UDF_IDX_CSUM);

	udf_pf->udf[PPE_DRV_TUN_GRE_UDF_IDX_CSUM].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L4;
	udf_pf->udf[PPE_DRV_TUN_GRE_UDF_IDX_CSUM].offset = PPE_DRV_TUN_GRE_UDF_OFFSET_CSUM;
	gre->udf = ppe_drv_tun_udf_entry_configure(udf_pf);
	if (!gre->udf) {
		ppe_drv_warn("%p: Failed to configure UDF profile for GRE CSUM match\n", gre);
		goto err;
	}

	/*
	 * To match GRE csum field we need two ACL lists.
	 * First list will be used to exception packets which has optional csum filed set.
	 * The second list will be used to exception packets which does not have optional csum field.
	 *
	 * When Tunnel is created with csum option, the gre packets without the optional csum field should
	 * also be exceptioned in addition to invalid csum packets.
	 * Similarly when the tunnel is create without csum option, gre packets with optional csum field
	 * present should be exceptioned.
	 *
	 * We cannot use a single list and add both the rules, as we are trying to exception the packets
	 * based on the same field in both the rules.
	 */
	error = fal_acl_list_creat(PPE_DRV_SWITCH_ID, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_EN, 0);
	if (error != SW_OK) {
		ppe_drv_warn("%p:List create failed for GRE CSUM EN(list_id: %d) with error: %d\n",
				gre, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_EN, error);
		goto err;
	}

	gre->acl_id_en = PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_EN;

	acl_rule.rule_type = FAL_ACL_RULE_TUNNEL_UDF;

	FAL_FIELD_FLG_SET(acl_rule.field_flg, FAL_ACL_FIELD_UDFPROFILE);
        acl_rule.udfprofile_val = gre->udf->udf_index;
        acl_rule.udfprofile_mask = PPE_DRV_TUN_GRE_ACL_UDF_PROFILE_MASK;

	/*
         * Match CSUM enable bit with 1.
         */
	FAL_FIELD_FLG_SET(acl_rule.field_flg, FAL_ACL_FIELD_UDF0);
        acl_rule.udf0_val = PPE_DRV_TUN_GRE_ACL_VAL_CSUM_EN;
        acl_rule.udf0_mask = PPE_DRV_TUN_GRE_ACL_CSUM_MASK;

	/*
	 * Redirect the packets matching the rule to CPU with cpu code 220.
	 */
	FAL_ACTION_FLG_SET(acl_rule.action_flg, FAL_ACL_ACTION_RDTCPU);
	FAL_ACTION_FLG_SET(acl_rule.action_flg, FAL_ACL_ACTION_CPU_CODE);
	acl_rule.cpu_code = PPE_DRV_CC_GRE_CSUM;
	error = fal_acl_rule_add(PPE_DRV_SWITCH_ID, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_EN, PPE_DRV_ACL_RULE_ID,
					PPE_DRV_ACL_RULE_NR, &acl_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p:\"GRE CSUM enabled\" acl rule add failed with error: %d\n", gre, error);
		goto err;
	}

	/*
	 * Bind the list to a specific VP group.
	 */
	error = fal_acl_list_bind(PPE_DRV_SWITCH_ID, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_EN, FAL_ACL_DIREC_IN,
			FAL_ACL_BIND_TUNNEL_VP_GROUP, PPE_DRV_TUN_GRE_ACL_VPID_CSUM_EN);
	if (error != SW_OK) {
		ppe_drv_warn("%p:\"GRE CSUM enabled\" acl list bind failed with error: %d\n", gre, error);
		goto err;
	}

	gre->acl_vpid_en = PPE_DRV_TUN_GRE_ACL_VPID_CSUM_EN;

	/*
	 * Create ACL list to match for csum disabled check.
	 */
	error = fal_acl_list_creat(PPE_DRV_SWITCH_ID, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_DIS, 0);
	if (error != SW_OK) {
		ppe_drv_warn("%p:List create failed for GRE_CSUM_DIS(list_id: %d) with error: %d\n",
				gre, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_DIS, error);
		goto err;
	}

	/*
	 * Update UDF0 value to match CSUM enable bit to 0.
	 */
        acl_rule.udf0_val = PPE_DRV_TUN_GRE_ACL_VAL_CSUM_DIS;
	error = fal_acl_rule_add(PPE_DRV_SWITCH_ID, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_DIS, PPE_DRV_ACL_RULE_ID,
					PPE_DRV_ACL_RULE_NR, &acl_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p:\"GRE CSUM disabled\" acl rule add failed with error: %d\n", gre, error);
		goto err;
	}

	gre->acl_id_dis = PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_DIS;

	/*
	 * Bind the list to a specific VP group.
	 */
	error = fal_acl_list_bind(PPE_DRV_SWITCH_ID, PPE_DRV_TUN_GRE_ACL_LIST_ID_CSUM_DIS, FAL_ACL_DIREC_IN,
			FAL_ACL_BIND_TUNNEL_VP_GROUP, PPE_DRV_TUN_GRE_ACL_VPID_CSUM_DIS);
	if (error != SW_OK) {
		ppe_drv_warn("%p:\"GRE CSUM disabled\" acl list bind failed with error: %d\n", gre, error);
		goto err;
	}

	gre->acl_vpid_dis = PPE_DRV_TUN_GRE_ACL_VPID_CSUM_DIS;

	kfree(udf_pf);
	return true;

err:
	ppe_drv_tun_gre_acl_deref(gre);
	kfree(udf_pf);
	return false;
}
