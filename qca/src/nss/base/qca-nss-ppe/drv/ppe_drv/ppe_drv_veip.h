/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef PPE_DRV_VEIP_H
#define PPE_DRV_VEIP_H

#include <linux/list.h>
#include <linux/kref.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv_sc.h>
#include <ppe_drv_port.h>

struct ppe_drv_iface;

/**
 * struct ppe_drv_veip_ctx - VEIP context structure
 */
struct ppe_drv_veip_ctx {
	struct list_head list;		/* List node. */
	struct ppe_drv_port *port;	/* PPE port pointer. */
	struct kref ref;		/* eference counter. */
};

/*
 * Internal APIs used within ppe_drv module
 */
ppe_drv_ret_t ppe_drv_veip_port_list_add(struct ppe_drv_iface *ppe_iface, struct ppe_drv_port *pp);
ppe_drv_ret_t ppe_drv_veip_port_list_get(struct ppe_drv_iface *ppe_iface, struct ppe_drv_port **veip_ports, uint8_t *num_ports);
ppe_drv_ret_t ppe_drv_veip_port_list_del(struct ppe_drv_iface *ppe_iface);
bool ppe_drv_veip_vp_mtu_mru_set(struct ppe_drv_iface *iface, uint16_t mtu, uint16_t mru);
bool ppe_drv_veip_vp_mtu_mru_disable(struct ppe_drv_iface *iface);
void ppe_drv_veip_mac_addr_set(struct ppe_drv_iface *iface, const uint8_t *mac_addr);
void ppe_drv_veip_mac_addr_clear(struct ppe_drv_iface *iface);
ppe_drv_ret_t ppe_drv_veip_eg_vpgroup_set(uint32_t vport_index, uint32_t vpgroup_id);
ppe_drv_ret_t ppe_drv_veip_eg_vpgroup_clear(uint32_t vport_index);
bool ppe_drv_veip_l2_vp_sc_config(struct ppe_drv_port *pp, ppe_drv_sc_t sc, uint32_t phy_port);
ppe_drv_ret_t ppe_drv_veip_gw_port_sc(struct ppe_drv_port *tx_port, ppe_drv_sc_t *service_code);
bool ppe_drv_veip_is_enabled(struct net_device *dev);
int32_t ppe_drv_veip_get_port_internal(struct ppe_drv_iface *iface, enum ppe_drv_port_type type);
int32_t ppe_drv_veip_get_port(struct ppe_drv_iface *iface, enum ppe_drv_port_type type);
bool ppe_drv_veip_is_hgu_rule_valid(struct ppe_drv_iface *iface);
void ppe_drv_veip_flag_set(struct ppe_drv_iface *iface);
void ppe_drv_veip_flag_clear(struct ppe_drv_iface *iface);

#endif /* PPE_DRV_VEIP_H */
