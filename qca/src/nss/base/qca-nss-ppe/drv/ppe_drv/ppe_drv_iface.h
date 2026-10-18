/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * PPE Iface valid handle flag.
 */
#define PPE_DRV_IFACE_FLAG_VALID	0x1
#define PPE_DRV_IFACE_FLAG_PORT_VALID	0x2
#define PPE_DRV_IFACE_FLAG_VSI_VALID	0x4
#define PPE_DRV_IFACE_FLAG_L3_IF_VALID	0x8
#define PPE_DRV_IFACE_VLAN_OVER_BRIDGE  0x10
#define PPE_DRV_IFACE_FLAG_WAN_IF_VALID 0x20
#define PPE_DRV_IFACE_FLAG_MHT_SWITCH_VALID 0x40
#define PPE_DRV_IFACE_FLAG_HGU_RULE_VALID 0x80

#ifdef NSS_PPE_DSA_QUEUE_SUPPORT
#define PPE_DRV_IFACE_FLAG_HTT_SWITCH_VALID 0x100
#endif

/*
 * ppe-iface cleanup function callback
 */
typedef ppe_drv_ret_t (*ppe_drv_iface_cleanup_cb)(struct ppe_drv_iface *iface);

#ifdef PPE_DRV_NPTV6_HW_SUPPORT
struct ppe_drv_v6_conn_npt6;
#endif

/*
 * ppe_drv_iface
 *	PPE interface information
 */
struct ppe_drv_iface {
	struct ppe_drv_iface *base_if;		/* Base list for hierarchy creation */
	struct ppe_drv_iface *parent;		/* Pointer to parent ppe-if– used for bridge/lag slaves */
	struct ppe_drv_port *port;		/* Pointer to port structure */
	struct ppe_drv_vsi *vsi;		/* Pointer to vsi structure */
	struct ppe_drv_l3_if *l3;		/* Pointer to l3_if structure */
	struct kref ref;			/* Reference count */
	struct net_device *dev;			/* Corresponding net-device */
	uint16_t flags;				/* Flag to indicate valid handles */
	uint16_t index;				/* Interface index */
	enum ppe_drv_iface_type type;		/* Interface type */
	ppe_drv_iface_cleanup_cb cleanup_cb;	/* cleanup callback */
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	struct list_head npt6_hp;		/* List of adhoc l3_if for hairpin nat connections */
#endif
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	uint16_t veip_cnt;			/* VEIP interface count */
	struct list_head veip_port;		/* List of vp port for veip interface */
#endif
};

bool ppe_drv_iface_deref_internal(struct ppe_drv_iface *iface);
struct ppe_drv_iface *ppe_drv_iface_ref(struct ppe_drv_iface *iface);

struct ppe_drv_iface *ppe_drv_iface_get_by_dev_internal(struct net_device *dev);
struct ppe_drv_iface *ppe_drv_iface_get_by_idx(ppe_drv_iface_t idx);
int32_t ppe_drv_iface_vsi_idx_get(struct ppe_drv_iface *iface);
int32_t ppe_drv_iface_l3_if_idx_get(struct ppe_drv_iface *iface);

bool ppe_drv_iface_parent_set(struct ppe_drv_iface *iface, struct ppe_drv_iface *parent);
struct ppe_drv_iface *ppe_drv_iface_parent_get(struct ppe_drv_iface *iface);
void ppe_drv_iface_parent_clear(struct ppe_drv_iface *iface);

void ppe_drv_iface_base_set(struct ppe_drv_iface *iface, struct ppe_drv_iface *base);
struct ppe_drv_iface *ppe_drv_iface_base_get(struct ppe_drv_iface *iface);
void ppe_drv_iface_base_clear(struct ppe_drv_iface *iface);

bool ppe_drv_iface_port_set(struct ppe_drv_iface *iface, struct ppe_drv_port *port);
struct ppe_drv_port *ppe_drv_iface_port_get(struct ppe_drv_iface *iface);
void ppe_drv_iface_port_clear(struct ppe_drv_iface *iface);

bool ppe_drv_iface_vsi_set(struct ppe_drv_iface *iface, struct ppe_drv_vsi *vsi);
struct ppe_drv_vsi *ppe_drv_iface_vsi_get(struct ppe_drv_iface *iface);
void ppe_drv_iface_vsi_clear(struct ppe_drv_iface *iface);

bool ppe_drv_iface_l3_if_set(struct ppe_drv_iface *iface, struct ppe_drv_l3_if *l3_if);
struct ppe_drv_l3_if * ppe_drv_iface_l3_if_get(struct ppe_drv_iface *iface);
void ppe_drv_iface_l3_if_clear(struct ppe_drv_iface *iface);

void ppe_drv_iface_entries_free(struct ppe_drv_iface *iface);
struct ppe_drv_iface *ppe_drv_iface_entries_alloc(void);

bool ppe_drv_iface_udp_zero_csum_action_set_internal(struct ppe_drv_iface *iface, ppe_drv_iface_zero_csum_action_t action);

#ifdef PPE_DRV_NPTV6_HW_SUPPORT
bool ppe_drv_iface_l3_if_nptv6_ref(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_v6_conn_npt6 *npt6);
#endif
