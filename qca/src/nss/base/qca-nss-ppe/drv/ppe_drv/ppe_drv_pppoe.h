/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/if_ether.h>

/*
 * ns_ppe_pppoe
 *	PPPoE offload information
 */
struct ppe_drv_pppoe {
	struct ppe_drv_l3_if *l3_if;		/* L3 interface corresponding to this pppoe entry */
	struct ppe_drv_tun_l3_if *tl_l3_if;	/* Tunnel L3 interface corresponding to this pppoe entry */
	struct kref ref;			/* Reference count */
	uint8_t port_bitmap;			/* TODO: Ports on which this session applies? */
	uint8_t index;				/* pppoe index number */
	uint8_t server_mac[ETH_ALEN]; 		/* PPPoE Server MAC */
	uint16_t session_id;			/* PPPoE session info */
	bool is_session_added;			/* PPPoE table add done */
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	struct ppe_drv_pppoe *veip_pppoe;	/* Secondary PPPoE entry used for VEIP*/
#endif
};

bool ppe_drv_pppoe_l3_if_deref(struct ppe_drv_pppoe *pppoe);
struct ppe_drv_l3_if *ppe_drv_pppoe_l3_if_get_and_ref(struct ppe_drv_pppoe *pppoe);

bool ppe_drv_pppoe_deref(struct ppe_drv_pppoe *pppoe);
struct ppe_drv_pppoe *ppe_drv_pppoe_ref(struct ppe_drv_pppoe *pppoe);
struct ppe_drv_tun_l3_if *ppe_drv_pppoe_tl_l3_if_get(struct ppe_drv_pppoe *pppoe);
bool ppe_drv_pppoe_tl_l3_if_attach(struct ppe_drv_pppoe *pppoe, struct ppe_drv_tun_l3_if *ptun_l3_if);
bool ppe_drv_pppoe_tl_l3_if_detach(struct ppe_drv_pppoe *pppoe);

void ppe_drv_pppoe_l3_if_attach(struct ppe_drv_pppoe *pppoe, struct ppe_drv_l3_if *l3_if);
void ppe_drv_pppoe_l3_if_detach(struct ppe_drv_pppoe *pppoe);
struct ppe_drv_l3_if *ppe_drv_pppoe_find_l3_if(uint16_t session_id, uint8_t *smac);

struct ppe_drv_pppoe *ppe_drv_pppoe_find_session_by_tl_l3_if(struct ppe_drv_tun_l3_if *tl_l3_if);
struct ppe_drv_pppoe *ppe_drv_pppoe_find_session(uint16_t session_id, uint8_t *smac);
struct ppe_drv_pppoe *ppe_drv_pppoe_alloc(uint16_t session_id, uint8_t *smac);

void ppe_drv_pppoe_entries_free(struct ppe_drv_pppoe *pppoe);
struct ppe_drv_pppoe *ppe_drv_pppoe_entries_alloc(void);

#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
void ppe_drv_pppoe_veip_l3_if_attach(struct ppe_drv_pppoe *pppoe, struct ppe_drv_l3_if *l3_if, struct ppe_drv_iface *base_iface);
void ppe_drv_pppoe_veip_l3_if_detach(struct ppe_drv_pppoe *pppoe);
struct ppe_drv_pppoe *ppe_drv_pppoe_find_session_by_veip(uint16_t session_id, uint8_t *smac, uint32_t gw_vp);
#endif
