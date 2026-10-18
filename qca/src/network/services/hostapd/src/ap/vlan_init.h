/*
 * hostapd / VLAN initialization
 * Copyright 2003, Instant802 Networks, Inc.
 * Copyright 2005, Devicescape Software, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef VLAN_INIT_H
#define VLAN_INIT_H

#ifndef CONFIG_NO_VLAN
int vlan_init(struct hostapd_data *hapd);
void vlan_deinit(struct hostapd_data *hapd);
struct hostapd_vlan * vlan_add_dynamic(struct hostapd_data *hapd,
				       struct hostapd_vlan *vlan,
				       int vlan_id,
				       struct vlan_description *vlan_desc);
int vlan_remove_dynamic(struct hostapd_data *hapd, int vlan_id);
void vlan_cancel_cleanup_for_hapd(struct hostapd_data *hapd);
void vlan_cancel_cleanup_for_vlan(struct hostapd_vlan *vlan);
void vlan_cleanup(void *eloop_ctx, void *timeout_ctx);
#else /* CONFIG_NO_VLAN */
static inline int vlan_init(struct hostapd_data *hapd)
{
	return 0;
}

static inline void vlan_deinit(struct hostapd_data *hapd)
{
}

static inline void vlan_cancel_cleanup_for_hapd(struct hostapd_data *hapd)
{
}

static inline void vlan_cancel_cleanup_for_vlan(struct hostapd_vlan *vlan)
{
}

static inline void vlan_cleanup(void *eloop_ctx, void *timeout_ctx)
{
}

static inline struct hostapd_vlan *
vlan_add_dynamic(struct hostapd_data *hapd, struct hostapd_vlan *vlan,
		 int vlan_id, struct vlan_description *vlan_desc)
{
	return NULL;
}

static inline int vlan_remove_dynamic(struct hostapd_data *hapd, int vlan_id)
{
	return -1;
}
#endif /* CONFIG_NO_VLAN */

#endif /* VLAN_INIT_H */
