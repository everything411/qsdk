/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef PPE_DRV_VLAN_H
#define PPE_DRV_VLAN_H

#include <fal/fal_vlan.h>
#include <fal/fal_api.h>
#include <ppe_drv_vlan.h>

/*
 * VLAN HW ID macros.
 */
#if defined(NSS_PPE_IPQ52XX)
#define PPE_DRV_VLAN_HW_ID_IV_MAX		256	/* Maximum number of VLAN rules for Hermosa */
#else
#define PPE_DRV_VLAN_HW_ID_IV_MAX		128	/* Maximum number of VLAN rules for JUHU */
#endif

#define PPE_DRV_VLAN_HW_ID_EG_MAX		128	/* Egress VLAN HW ID max */
#define PPE_DRV_VLAN_HW_ID_USED			1	/* HW ID state used */
#define PPE_DRV_VLAN_HW_ID_FREE			2	/* HW ID state free */
#define PPE_DRV_VLAN_HW_ID_START		0	/* HW ID start index */
#define PPE_DRV_VLAN_HW_ID_IV_END		(PPE_DRV_VLAN_HW_ID_IV_MAX - 1)	/* Ingress VLAN HW ID end */
#define PPE_DRV_VLAN_HW_ID_EG_END		(PPE_DRV_VLAN_HW_ID_EG_MAX - 1)	/* Egress VLAN HW ID end */

#define PPE_DRV_VLAN_INVALID_PORT -1			/* Invalid port index */

#define PPE_DRV_VLAN_CTPID_MAP				0x5	/* CTPID map index */
#define PPE_DRV_VLAN_STPID_MAP				0xa	/* STPID map index */

/*
 * ppe_drv_vlan_ctx
 *	VLAN rule context
 */
struct ppe_drv_vlan_ctx {
	int16_t entry_index;				/* Entry Index. */
	bool is_veip_rule_valid;			/* Rule vaid flags for hgu VLAN rule */
	int16_t veip_rule_entry_index;			/* Entry Index for VEIP rule. */
	bool rule_valid;				/* Rule valid flag to handle failure with partial configuration. */
	ppe_drv_rule_dir_t rule_dir;			/* Rule direction. */
	struct ppe_drv_iface *iface;			/* Source interface. */

	/*
	 * Rule shadow.
	 */
	fal_vlan_trans_adv_rule_t fal_rule;		/* FAL rule field structure */
	fal_vlan_trans_adv_action_t fal_action;		/* FAL action field structure */
};

/*
 * ppe_drv_vlan_t
 *	Vlan rule table.
 */
struct ppe_drv_vlan_t {
	uint8_t hw_id_state;			/* HW ID state */
	struct ppe_drv_vlan_ctx *ctx;		/* VLAN context */
};

/*
 * ppe_drv_vlan_tbl
 *	VLAN rule table for iv and eg VLAN tables.
 */
struct ppe_drv_vlan_tbl {
	struct ppe_drv_vlan_t in_vlan_tbl[PPE_DRV_VLAN_HW_ID_IV_MAX];		/* Ingress VLAN shadow table */
	struct ppe_drv_vlan_t eg_vlan_tbl[PPE_DRV_VLAN_HW_ID_EG_MAX];		/* Egress VLAN shadow table */
};

/*
 * Internal APIs.
 */
struct ppe_drv_vlan_tbl *ppe_drv_vlan_entries_alloc(void);
void ppe_drv_vlan_entries_free(struct ppe_drv_vlan_tbl *vlan);
struct ppe_drv_iface *ppe_drv_vlan_ctx_iface_get(struct ppe_drv_vlan_ctx *ctx);

#endif /* PPE_DRV_VLAN_H */
