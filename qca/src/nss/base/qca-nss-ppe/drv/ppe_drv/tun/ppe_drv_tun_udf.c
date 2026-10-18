/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <ppe_drv/ppe_drv.h>
#include <fal_tunnel.h>
#include "ppe_drv_tun_udf.h"
#include "ppe_drv_tun_prgm_prsr.h"

/*
 * ppe_drv_tun_udf_entry_fill
 *	fill fal entry based on udf structure
 */
static void ppe_drv_tun_udf_entry_fill(fal_tunnel_udf_profile_entry_t *entry, struct ppe_drv_tun_udf_profile *udf_pf)
{
	if (udf_pf->l3_match) {
		FAL_TUNNEL_UDF_PROFILE_ENTRY_FIELD_FLG_SET(entry->field_flag, FAL_TUNNEL_UDF_PROFILE_ENTRY_FIELD_L3_TYPE);
		entry->l3_type = (fal_l3_type_t)udf_pf->l3_type;
	}

	if (udf_pf->l4_match) {
		FAL_TUNNEL_UDF_PROFILE_ENTRY_FIELD_FLG_SET(entry->field_flag, FAL_TUNNEL_UDF_PROFILE_ENTRY_FIELD_L4_TYPE);
		entry->l4_type = (fal_l4_type_t)udf_pf->l4_type;
	}

	if (udf_pf->program_match) {
		FAL_TUNNEL_UDF_PROFILE_ENTRY_FIELD_FLG_SET(entry->field_flag, FAL_TUNNEL_UDF_PROFILE_ENTRY_FIELD_PROGRAM_TYPE);
		entry->program_type = udf_pf->program_type;
	}
}

/*
 * ppe_drv_tun_udf_entry_free
 *	free tunnel udf entry
 */
static void ppe_drv_tun_udf_entry_free(struct kref *kref)
{
	sw_error_t err;
	fal_tunnel_udf_profile_entry_t entry = {0};
	struct ppe_drv_tun_udf  *pgm_udf;
	struct ppe_drv_tun_udf_profile *udf_pf;
	int i;

	pgm_udf = container_of(kref, struct ppe_drv_tun_udf, ref);

	udf_pf = &pgm_udf->udf;
	ppe_drv_tun_udf_entry_fill(&entry, udf_pf);

	err = fal_tunnel_udf_profile_entry_del(PPE_DRV_SWITCH_ID, pgm_udf->udf_index, &entry);
	if (err != SW_OK) {
		ppe_drv_trace("%p: udf profile entry delete failed with error %d", pgm_udf, err);
		return;
	}

	for (i = 0; i < PPE_DRV_TUN_UDF_MAX; i++) {
		err = fal_tunnel_udf_profile_cfg_set(PPE_DRV_SWITCH_ID, pgm_udf->udf_index, i, 0, 0);
		if (err != SW_OK) {
			ppe_drv_warn("UDF configuration delete failed for index %d with error %d", pgm_udf->udf_index, err);
			return;
		}
	}
}

/*
 * ppe_drv_tun_udf_entry_dref
 *	deref tunnel udf entry
 */
bool ppe_drv_tun_udf_entry_dref(struct ppe_drv_tun_udf  *pgm_udf)
{
	int index __maybe_unused = pgm_udf->udf_index;

	ppe_drv_assert(kref_read(&pgm_udf->ref), "ref count under run for udf index: %d", index);
	if (kref_put(&pgm_udf->ref, ppe_drv_tun_udf_entry_free)) {
		ppe_drv_trace("reference count is 0 for udf index: %d", index);
		return true;
	}
	ppe_drv_trace("%p: index: %u ref dnc:%u", pgm_udf, index, kref_read(&pgm_udf->ref));

	return true;
}

/*
 * ppe_drv_tun_udf_entry_ref
 *	take ref on tunnel udf entry
 */
void ppe_drv_tun_udf_entry_ref(struct ppe_drv_tun_udf  *pgm_udf)
{
	kref_get(&pgm_udf->ref);

	ppe_drv_assert(kref_read(&pgm_udf->ref), "%p: ref count rollover for udf index:%d", pgm_udf, pgm_udf->udf_index);
	ppe_drv_trace("%p: idx: %u ref inc:%u", pgm_udf, pgm_udf->udf_index, kref_read(&pgm_udf->ref));
}

/*
 * ppe_drv_tun_udf_compare_udf_profiles
 *	compare udf profile configurations
 *	a -> represents exisiting configuration
 *	b -> proposed new configurations
 */
bool ppe_drv_tun_udf_compare_udf_profiles(struct ppe_drv_tun_udf_profile *a, struct ppe_drv_tun_udf_profile *b)
{
	struct ppe_drv *p __maybe_unused = ppe_drv_gbl;
	int i;

	/*
	 * Input validation
	 */
	if (!a || !b) {
		return false;
	}

	if (a->l3_match != b->l3_match || a->l3_type != b->l3_type ||
	    a->l4_match != b->l4_match || a->l4_type != b->l4_type ||
	    a->program_match != b->program_match || a->program_type != b->program_type ||
	    a->udf_bitmap != b->udf_bitmap) {
		ppe_drv_trace("%p: UDF profile configurations doesnt match", p);
		return false;
	}

	for (i = 0; i < PPE_DRV_TUN_UDF_MAX; i++) {
		if (a->udf[i].offset_type != b->udf[i].offset_type ||
		    a->udf[i].offset != b->udf[i].offset) {
			return false;
		}
	}

	return true;
}

/*
 * ppe_drv_tun_udf_entry_configure
 *	Configure tunnel udf entry - searches for existing matching entry first
 */
struct ppe_drv_tun_udf *ppe_drv_tun_udf_entry_configure(struct ppe_drv_tun_udf_profile *udf_pf)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_tunnel_udf_profile_entry_t entry = {0};
	struct ppe_drv_tun_udf *udf_entry = NULL;
	struct ppe_drv_tun_udf_data *udf_data;
	sw_error_t err;
	int i;

	/*
	 * First, search for an existing tun_udf entry with matching configuration.
	 * If found, take a reference and return it to enable sharing across tunnels.
	 */
	for (i = 0; i < PPE_DRV_TUN_UDF_PROFILE_ID_MAX; i++) {
		udf_entry = &p->pgm_udf[i];

		/*
		 * Skip free entries
		 */
		if (!kref_read(&udf_entry->ref)) {
			continue;
		}

		/*
		 * Compare configurations - if they match, reuse this entry
		 */
		if (ppe_drv_tun_udf_compare_udf_profiles(udf_pf, &udf_entry->udf)) {
			/*
			 * Found matching configuration - take reference and return.
			 * This enables multiple tunnels with identical UDF offsets
			 * to share the same hardware UDF profile entry.
			 */
			ppe_drv_tun_udf_entry_ref(udf_entry);
			ppe_drv_trace("%p: Reusing existing tun_udf entry at index %d, ref count: %u",
				     udf_entry, i, kref_read(&udf_entry->ref));
			return udf_entry;
		}
	}

	/*
	 * No matching entry found - allocate and configure a new one
	 */
	udf_entry = ppe_drv_tun_udf_entry_alloc(p);
	if (!udf_entry) {
		ppe_drv_warn("%p: tunnel udf profile entry allocation failed\n", p);
		return NULL;
	}

	ppe_drv_tun_udf_entry_fill(&entry, udf_pf);

	err = fal_tunnel_udf_profile_entry_add(PPE_DRV_SWITCH_ID, udf_entry->udf_index, &entry);
	if (err != SW_OK) {
		ppe_drv_trace("%p: tunnel udf profile add failed with error %d", p, err);
		goto fail;
	}

	for (i = 0; i < PPE_DRV_TUN_UDF_MAX; i++) {
		udf_data = &udf_pf->udf[i];
		if (ppe_drv_tun_udf_bitmask_check(udf_pf, i)) {
			err = fal_tunnel_udf_profile_cfg_set(PPE_DRV_SWITCH_ID, udf_entry->udf_index,
					i, (fal_tunnel_udf_type_t)udf_data->offset_type, udf_data->offset);
			if (err != SW_OK) {
				ppe_drv_trace("%p: tunnel udf profile cfg set failed with error %d", p, err);
				err = fal_tunnel_udf_profile_entry_del(PPE_DRV_SWITCH_ID, udf_entry->udf_index, &entry);
				if (err != SW_OK) {
					ppe_drv_trace("%p: tunnel udf profile delete failed with error %d", p, err);
				}
				goto fail;
			}
		}
	}

	memcpy(&udf_entry->udf, udf_pf, sizeof(struct ppe_drv_tun_udf_profile));
	ppe_drv_trace("%p: Allocated new tun_udf entry at index %d", udf_entry, udf_entry->udf_index);

	return udf_entry;
fail:
	if (udf_entry) {
		ppe_drv_tun_udf_entry_dref(udf_entry);
	}

	return NULL;
}

/*
 * ppe_drv_tun_udf_entry_alloc
 *	Return free tunnel udf entry
 */
struct ppe_drv_tun_udf *ppe_drv_tun_udf_entry_alloc(struct ppe_drv *p)
{
	uint16_t index = 0;
	struct ppe_drv_tun_udf *pgm_udf;

	/*
	 * Return first free instance
	 */
	for (index = 0; index < PPE_DRV_TUN_UDF_PROFILE_ID_MAX; index++) {
		pgm_udf = &p->pgm_udf[index];
		if (kref_read(&pgm_udf->ref)) {
			continue;
		}

		kref_init(&pgm_udf->ref);
		ppe_drv_trace("%p: Free tunnel udf instance found, index: %d", pgm_udf, index);
		return pgm_udf;
	}

	ppe_drv_warn("%p: Free tunnel udf instance is not found", p);
	return NULL;
}

/*
 * ppe_drv_tun_udf_free
 *	free tunnel parser udf profile entries
 */
void ppe_drv_tun_udf_free(struct ppe_drv_tun_udf *pgm_udf)
{
	nss_ppe_drv_minidump_free(pgm_udf, "ppe_drv_tun_udf");
	kfree(pgm_udf);
}

/*
 * ppe_drv_tun_udf_alloc
 *	Allocate and Initialize tunnel udf profile entries
 */
struct ppe_drv_tun_udf *ppe_drv_tun_udf_alloc(struct ppe_drv *p)
{
	struct ppe_drv_tun_udf *pgm_udf;
	int index;

	ppe_drv_assert(!p->pgm_udf, "%p: tunnel udf entries already allocated", p);

	pgm_udf = kzalloc(sizeof(struct ppe_drv_tun_udf) * PPE_DRV_TUN_UDF_PROFILE_ID_MAX, GFP_KERNEL);
	if (!pgm_udf) {
		ppe_drv_warn("%p: failed to allocate tunnel udf entries", p);
		return NULL;
	}

	nss_ppe_drv_minidump_log(pgm_udf, sizeof(struct ppe_drv_tun_udf) * PPE_DRV_TUN_UDF_PROFILE_ID_MAX, "ppe_drv_tun_udf");

	for (index = 0; index < PPE_DRV_TUN_UDF_PROFILE_ID_MAX; index++) {
		pgm_udf[index].udf_index = index;
	}

	return pgm_udf;
}
