/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
#include <net/dsa.h>
#include <linux/dsa/8021q.h>
#endif
#include <fal/fal_fdb.h>
#include <fal/fal_rss_hash.h>
#include <fal/fal_ip.h>
#include <fal/fal_init.h>
#include <fal/fal_pppoe.h>
#include <fal/fal_tunnel.h>
#include <fal/fal_api.h>
#include <fal/fal_vsi.h>
#include <ref/ref_vsi.h>
#include <fal/fal_portvlan.h>
#include "ppe_drv.h"
#include "ppe_drv_stats.h"
#include "tun/ppe_drv_tun_encap.h"

/*
 * ppe_drv_vlan_del_untag_ingress_rule()
 *	Delete ingress VLAN translation rule for untagged frames
 */
bool ppe_drv_vlan_del_untag_ingress_rule(struct ppe_drv_port *port, struct ppe_drv_l3_if *src_l3_if)
{
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_port_t fal_port;
	sw_error_t err;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port->port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port->port)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);

	/*
	 * Fields for match
	 */
	xlt_rule.s_tagged = 0x1;				/* Accept untagged svlan */
	xlt_rule.c_tagged = 0x1;				/* Accept untagged cvlan */

	/*
	 * Fields for action
	 */
	xlt_action.src_info_enable = A_TRUE;
	xlt_action.src_info_type = 1;
	xlt_action.src_info = src_l3_if->l3_if_index;

	err = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule,
				&xlt_action);
	if (err != SW_OK) {
		ppe_drv_warn("%px: Failed to delete ingress translation rule for untagged VLAN port: %d, error: %d\n",
				p, fal_port, err);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_untag_vlan_del);
		return false;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	ppe_drv_info("%p: Deleted untag vlan rule for port: %d, with src_l3_if: %d", p, port->port, src_l3_if->l3_if_index);
	return true;
}

/*
 * ppe_drv_vlan_add_untag_ingress_rule()
 *	Add Ingress VLAN translation rule for untagged frames
 */
bool ppe_drv_vlan_add_untag_ingress_rule(struct ppe_drv_port *port, struct ppe_drv_l3_if *src_l3_if)
{
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_port_t fal_port;
	sw_error_t err;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port->port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port->port)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);

	/*
	 * Fields for match
	 */
	xlt_rule.s_tagged = 0x1;				/* Accept untagged svlan */
	xlt_rule.c_tagged = 0x1;				/* Accept untagged cvlan */

	/*
	 * Fields for action
	 */
	xlt_action.src_info_enable = A_TRUE;
	xlt_action.src_info_type = 1;
	xlt_action.src_info = src_l3_if->l3_if_index;

	err = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule,
				&xlt_action);
	if (err != SW_OK) {
		ppe_drv_warn("%px: Failed to update ingress translation rule for untagged VLAN port: %d, error: %d\n",
				p, fal_port, err);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_untag_vlan_add);
		return false;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	ppe_drv_info("%p: Added untag vlan rule for port: %d, with src_l3_if: %d", p, port->port, src_l3_if->l3_if_index);
	return true;
}

/*
 * ppe_drv_vlan_tpid_set()
 *	Set PPE vlan TPID
 */
ppe_drv_ret_t ppe_drv_vlan_tpid_set(uint16_t *tpid_arr, uint32_t mask, fal_qinq_port_role_t port_role)
{
	struct ppe_drv *p = ppe_drv_gbl;

	fal_tpid_t tpid;

	tpid.mask = mask;
	tpid.ctpid = tpid_arr[0];
	tpid.stpid = tpid_arr[1];
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	tpid.ext_ctpid = tpid_arr[2];
	tpid.ext_stpid = tpid_arr[3];
	tpid.ctpid_map = PPE_DRV_VLAN_CTPID_MAP;
	tpid.stpid_map = PPE_DRV_VLAN_STPID_MAP;
#endif
	tpid.tunnel_ctpid = tpid_arr[0];
	tpid.tunnel_stpid = tpid_arr[1];
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	tpid.ext_tunnel_ctpid = tpid_arr[2];
	tpid.ext_tunnel_stpid = tpid_arr[3];
	tpid.tunnel_ctpid_map = PPE_DRV_VLAN_CTPID_MAP;
	tpid.tunnel_stpid_map = PPE_DRV_VLAN_STPID_MAP;
#endif
	spin_lock_bh(&p->lock);
	if (port_role == FAL_QINQ_CORE_PORT) {
		fal_global_qinq_mode_t mode = {0};

		fal_global_qinq_mode_get(PPE_DRV_SWITCH_ID, &mode);
		mode.mask = FAL_GLOBAL_QINQ_MODE_INGRESS_EN | FAL_GLOBAL_QINQ_MODE_EGRESS_EN;
		mode.ingress_mode = mode.egress_mode = (tpid.ctpid == tpid.stpid) ? FAL_QINQ_STAG_MODE : FAL_QINQ_CTAG_MODE;
		if (fal_global_qinq_mode_set(PPE_DRV_SWITCH_ID, &mode) != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("failed to set vlan mode with ctpid: %d stpid: %d\n", tpid.ctpid, tpid.stpid);
			return PPE_DRV_RET_PORT_ROLE_FAIL;
		}
	}

	if ((fal_ingress_tpid_set(PPE_DRV_SWITCH_ID, &tpid) != SW_OK) || (fal_egress_tpid_set(PPE_DRV_SWITCH_ID, &tpid) != SW_OK)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("failed to set TPIDs: [%d, %d, %d, %d]\n",
				tpid_arr[0], tpid_arr[1], tpid_arr[2], tpid_arr[3]);
		return PPE_DRV_RET_VLAN_TPID_FAIL;
	}

	p->gbl_ctpid = tpid_arr[0];
	p->gbl_stpid = tpid_arr[1];
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	p->gbl_ctpid_ext = tpid_arr[2];
	p->gbl_stpid_ext = tpid_arr[3];
#endif
	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;

}
EXPORT_SYMBOL(ppe_drv_vlan_tpid_set);

/*
 * ppe_drv_vlan_port_role_set()
 *	Set VLAN port role
 */
ppe_drv_ret_t ppe_drv_vlan_port_role_set(struct ppe_drv_iface *iface, uint32_t port_id, fal_port_qinq_role_t *mode)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_port_t fal_port;

	spin_lock_bh(&p->lock);
	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_id)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_id);

	if (fal_port_qinq_mode_set(PPE_DRV_SWITCH_ID, fal_port, mode) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("failed to set %d as edge port\n", fal_port);
		return PPE_DRV_RET_PORT_ROLE_FAIL;
	}

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_port_role_set);

/*
 * ppe_drv_vlan_ingress_rule_action_set_vp()
 *	Set xlt_rule and xlt_action structure for ingress.
 */
void ppe_drv_vlan_ingress_rule_action_set_vp(fal_vlan_trans_adv_rule_t *xlt_rule,
					fal_vlan_trans_adv_action_t *xlt_action, struct ppe_drv_vlan_xlate_info *info)
{
	/*
         * Field for ingress match.
	 * Accept tagged/untagged/priority tagged svlan and cvlan.
         */
	xlt_rule->s_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED
			| FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	xlt_rule->c_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED
			| FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	xlt_rule->c_vid = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_rule->c_vid_enable = (info->cvid == 0xFFFF) ? A_FALSE : A_TRUE;
	xlt_rule->s_vid = (info->svid == 0xFFFF) ? 0 : info->svid;
	xlt_rule->s_vid_enable = (info->svid == 0xFFFF) ? A_FALSE : A_TRUE;
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	xlt_rule->dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule->mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
#endif

	/*
	 * field for ingress action.
	 */
	xlt_action->cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
	xlt_action->cvid_xlt = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_action->svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
	xlt_action->svid_xlt = (info->svid == 0xFFFF) ? 0 : info->svid;
	xlt_action->src_info_enable = A_TRUE;
	xlt_action->src_info = info->port_id;
	xlt_action->src_info_type = FAL_CHG_SRC_TYPE_VP;
}

/*
 * ppe_drv_vlan_egress_rule_action_set_vp()
 *	Set xlt_rule and xlt_action structure for egress.
 */
void ppe_drv_vlan_egress_rule_action_set_vp(fal_vlan_trans_adv_rule_t *xlt_rule,
					fal_vlan_trans_adv_action_t *xlt_action, struct ppe_drv_vlan_xlate_info *info)
{
	/*
	 * Fields for egress match.
	 * Accept tagged/untagged/priority tagged svlan and cvlan.
	 */
	xlt_rule->s_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED | FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	xlt_rule->c_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED | FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	xlt_rule->dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule->mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
#endif

	/*
	 * Fields for egress action.
	 */
	xlt_action->cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action->cvid_xlt = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_action->svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action->svid_xlt = (info->svid == 0xFFFF) ? 0 : info->svid;
}

/*
 * ppe_drv_vlan_as_vp_del_xlate_rules()
 *	Delete Ingress and Egress VLAN translation rules for VP.
 */
ppe_drv_ret_t ppe_drv_vlan_as_vp_del_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};
	fal_port_t fal_port, base_f_port;
	sw_error_t rc;
	struct net_device *base_dev;
	struct ppe_drv_iface *base_if;
	struct vlan_dev_priv *dev_priv;
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	struct dsa_port *dp __maybe_unused = NULL;
#endif
	uint8_t b_port;

	/*
	 * Handle VLAN or Q-in-Q (eg: eth0.10, eth0.10.20, lan1.10)
	 * Post these checks, base_dev will point to base net device (eth0/lan1) for VLAN dev,
	 * or NULL for non-VLAN interface (eg: eth0/lan1).
	 */
	dev_priv = (iface->dev && is_vlan_dev(iface->dev) ? vlan_dev_priv(iface->dev): NULL);
	base_dev = dev_priv ? dev_priv->real_dev: NULL;
	if (base_dev && is_vlan_dev(base_dev)) {
		base_dev = vlan_dev_priv(base_dev)->real_dev;
	}

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	/*
	 * Handling for DSA/VLAN on DSA interface
	 */
	if (!base_dev && dsa_slave_dev_check(iface->dev)) {
		/*
		 * We reach here if it's not a VLAN dev, eg: lan1.
		 * base_dev is NULL at this point. Set base_dev as lan1's CPU port.
		 */
		dp = dsa_port_from_netdev(iface->dev);
		if (dp) {
			base_dev = dsa_port_to_master(dp);
		}
	} else if (base_dev && dsa_slave_dev_check(base_dev)) {
		/*
		 * VLAN over DSA interface, eg: lan1.10.
		 * base_dev would already been set to lan1. Set base_dev as lan1's CPU port.
		 * Under NSS_PPE_DSA_QINQ_SUPPORT, keep base_dev = lan1 so ingress rules
		 * are keyed on lan1's VP, identical to VLAN over VP (e.g. mld0.10).
		 */
#ifndef NSS_PPE_DSA_QINQ_SUPPORT
		dp = dsa_port_from_netdev(base_dev);
		if (dp) {
			base_dev = dsa_port_to_master(dp);
		}
#endif
	}
#endif

	if (!base_dev) {
		ppe_drv_warn("%s: failed to obtain base_dev", iface->dev->name);
		return PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
	}

	spin_lock_bh(&p->lock);
	base_if = ppe_drv_iface_get_by_dev_internal(base_dev);
	if (!base_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: NULL base interface for iface\n", base_dev);
		return PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
	}

	b_port = ppe_drv_iface_port_idx_get(base_if);
	if (b_port == -1) {
		spin_unlock_bh(&p->lock);
		ppe_drv_info("%px: %s:%d is invalid port\n", base_dev, base_dev->name, b_port);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	ppe_drv_iface_base_set(iface, base_if);

	ppe_drv_vlan_ingress_rule_action_set_vp(&xlt_rule, &xlt_action, info);
	base_f_port = PPE_DRV_VIRTUAL_PORT_CHK(b_port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, b_port)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, b_port);

	ppe_drv_trace("%px: rule cvid: %d, rule svid: %d, act cvid: %d, act svid: %d, act src info: %d, port: %d\n",
			iface, xlt_rule.c_vid, xlt_rule.s_vid, xlt_action.cvid_xlt,
			xlt_action.svid_xlt, xlt_action.src_info, b_port);
	/*
	 * Delete ingress vlan translation rule.
	 */
	rc = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, base_f_port, FAL_PORT_VLAN_INGRESS,
			&xlt_rule, &xlt_action);
	if (rc != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to delete old ingress vlan translation of port %d, error: %d\n"
				, base_f_port, rc);
		return PPE_DRV_RET_VLAN_INGRESS_DEL_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	ppe_drv_vlan_egress_rule_action_set_vp(&xlt_rule, &xlt_action, info);
	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(info->port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, info->port_id)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, info->port_id);

	ppe_drv_trace("%px: act cvid: %d, act svid: %d, port: %d\n", iface, xlt_action.cvid_xlt,
			xlt_action.svid_xlt, info->port_id);
	/*
	 * Delete egress vlan translation rule.
	 */
	rc = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_EGRESS,
			&xlt_rule, &xlt_action);
	if (rc != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to delete old egress vlan translation of port %d, error: %d\n", fal_port, rc);
		return PPE_DRV_RET_VLAN_EGRESS_DEL_FAIL;
	}

	vlan->eg_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_as_vp_del_xlate_rules);

/*
 * ppe_drv_vlan_as_vp_add_xlate_rules()
 *	Add Ingress and Egress VLAN translation rules for VLAN created as VP.
 */
ppe_drv_ret_t ppe_drv_vlan_as_vp_add_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info)
{
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_port_t fal_port, base_f_port;
	struct ppe_drv_iface *base_if;
	struct net_device *base_dev;
	struct vlan_dev_priv *dev_priv;
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	struct dsa_port *dp __maybe_unused = NULL;
#endif
	uint8_t b_port;
	int ret;

	/*
	 * Handle VLAN or Q-in-Q (eg: eth0.10, eth0.10.20, lan1.10)
	 * Post these checks, base_dev will point to base net device (eth0/lan1) for VLAN dev,
	 * or NULL for non-VLAN interface (eg: eth0/lan1).
	 */
	dev_priv = (iface->dev && is_vlan_dev(iface->dev) ? vlan_dev_priv(iface->dev): NULL);
	base_dev = dev_priv ? dev_priv->real_dev: NULL;
	if (base_dev && is_vlan_dev(base_dev)) {
		base_dev = vlan_dev_priv(base_dev)->real_dev;
	}

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	/*
	 * Handling for DSA/VLAN on DSA interface
	 */
	if (!base_dev && dsa_slave_dev_check(iface->dev)) {
		/*
		 * We reach here if it's not a VLAN dev, eg: lan1.
		 * base_dev is NULL at this point. Set base_dev as lan1's CPU port.
		 */
		dp = dsa_port_from_netdev(iface->dev);
		if (dp) {
			base_dev = dsa_port_to_master(dp);
		}
	} else if (base_dev && dsa_slave_dev_check(base_dev)) {
		/*
		 * VLAN over DSA interface, eg: lan1.10.
		 * base_dev would already been set to lan1. Set base_dev as lan1's CPU port.
		 * Under NSS_PPE_DSA_QINQ_SUPPORT, keep base_dev = lan1 so ingress rules
		 * are keyed on lan1's VP, identical to VLAN over VP (e.g. mld0.10).
		 */
#ifndef NSS_PPE_DSA_QINQ_SUPPORT
		dp = dsa_port_from_netdev(base_dev);
		if (dp) {
			base_dev = dsa_port_to_master(dp);
		}
#endif
	}
#endif

	if (!base_dev) {
		ppe_drv_warn("%s: failed to obtain base_dev", iface->dev->name);
		return PPE_DRV_RET_BASE_DEV_NOT_FOUND;
	}

	spin_lock_bh(&p->lock);
	base_if = ppe_drv_iface_get_by_dev_internal(base_dev);
	if (!base_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: NULL base interface for iface\n", base_dev);
		return PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
	}

	b_port = ppe_drv_iface_port_idx_get(base_if);
	if (b_port == -1) {
		spin_unlock_bh(&p->lock);
		ppe_drv_info("%px: %s:%d is invalid port\n", base_dev, base_dev->name, b_port);
		return PPE_DRV_RET_BASE_PORT_NOT_FOUND;
	}

	ppe_drv_iface_base_set(iface, base_if);

	ppe_drv_vlan_ingress_rule_action_set_vp(&xlt_rule, &xlt_action, info);
	base_f_port = PPE_DRV_VIRTUAL_PORT_CHK(b_port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, b_port)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, b_port);

	ppe_drv_trace("%px: rule cvid: %d, rule svid: %d, act cvid: %d, act svid: %d, act src info: %d, port: %d, base_dev: %s\n",
			iface, xlt_rule.c_vid, xlt_rule.s_vid, xlt_action.cvid_xlt,
			xlt_action.svid_xlt, xlt_action.src_info, b_port, base_dev->name);

	/*
	 * Add ingress vlan translation rule.
	 * For adding ingress rule we are using base physical port number
	 * becasue packet rx happen on gmac, so we need to add a ingress
	 * rule/action to match tagged packet, which untag the packet and
	 * give it to src_info port, which is VP.
	 */
	ret = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, base_f_port,
			FAL_PORT_VLAN_INGRESS, &xlt_rule,
			&xlt_action);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to update ingress translation rule for port: %d, error: %d\n"
				, iface, base_f_port, ret);
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_add);
		return PPE_DRV_RET_INGRESS_VLAN_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	ppe_drv_vlan_egress_rule_action_set_vp(&xlt_rule, &xlt_action, info);
	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(info->port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, info->port_id)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, info->port_id);

	ppe_drv_trace("%px: act cvid: %d, act svid: %d, port: %d\n", iface, xlt_action.cvid_xlt,
			xlt_action.svid_xlt, info->port_id);

	/*
	 * Add egress vlan translation rule.
	 */
	ret = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_EGRESS, &xlt_rule,
				&xlt_action);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to update egress translation rule for port: %d, error: %d\n",
				iface, fal_port, ret);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_egress_vlan_add);

		/*
		 * Delete ingress vlan translation rule.
		 */
		if (ret != SW_ALREADY_EXIST) {
			memset(&xlt_rule, 0, sizeof(xlt_rule));
			memset(&xlt_action, 0, sizeof(xlt_action));
			ppe_drv_vlan_ingress_rule_action_set_vp(&xlt_rule, &xlt_action, info);
			fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, base_f_port, FAL_PORT_VLAN_INGRESS,
												&xlt_rule, &xlt_action);
			/*
			 * Resetting HW table index state.
			 */
			vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
		}

		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_EGRESS_VLAN_FAIL;
	}

	vlan->eg_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_as_vp_add_xlate_rules);

/*
 * ppe_drv_vlan_del_xlate_rule()
 *	Delete Ingress and Egress VLAN translation rules
 */
ppe_drv_ret_t ppe_drv_vlan_del_xlate_rule(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_vlan_trans_adv_rule_t xlt_rule;	/* VLAN Translation Rule */
	fal_vlan_trans_adv_action_t xlt_action;	/* VLAN Translation Action */
	struct ppe_drv_vsi *vsi;
	fal_port_t fal_port;
	int vsi_idx, rc;
	fal_vsi_member_t vsi_member;
	uint32_t port_value = 0, vport_value = 0;
	sw_error_t rv;

	/*
	 * Check with vlan device created under bridge
	 */
	spin_lock_bh(&p->lock);
	if (info->br) {
		vsi = ppe_drv_iface_vsi_get(info->br);
	} else {
		vsi = ppe_drv_iface_vsi_get(iface);
	}

	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	vsi_idx = vsi->index;

	/*
	 * Delete old ingress vlan translation rule
	 */
	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(info->port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, info->port_id)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, info->port_id);

	/*
	 * VSI member get
	 */
	port_value = FAL_PORT_ID_VALUE (fal_port);
	vport_value = port_value - SSDK_MIN_VIRTUAL_PORT_ID;
	ppe_drv_trace("port_id:0x%x, port_value:%d\n", fal_port, port_value);

	rv = fal_vsi_member_get(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if( rv != SW_OK ) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Invalid VSI memebr for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_FOUND;
	}

	if(FAL_IS_VPORT(fal_port))
	{
		vsi_member.member_vports[vport_value/32] &= ~(1<<vport_value%32);
		ppe_drv_trace("vsi_member.member_vports[%d]: 0x%x\n",
				vport_value/32, vsi_member.member_vports[vport_value/32]);
	} else {
		vsi_member.member_ports &= ~(1<<port_value);
		ppe_drv_trace("vsi_member.member_ports :0x%x\n", vsi_member.member_ports);
	}

	/*
	 * Add new ingress vlan translation rule
	 */
	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	/*
	 * Fields for match
	 */
	xlt_rule.s_tagged = (info->svid == 0xFFFF) ? 0x1 : ((info->svid == 0) ? 0x2 : 0x4);
	xlt_rule.c_tagged = (info->cvid == 0xFFFF) ? 0x1 : ((info->cvid == 0) ? 0x2 : 0x4);
	xlt_rule.s_vid = (info->svid == 0xFFFF) ? 0 : info->svid;
	xlt_rule.s_vid_enable = (info->svid == 0xFFFF) ? A_FALSE : A_TRUE;
	xlt_rule.c_vid = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_rule.c_vid_enable = (info->cvid == 0xFFFF) ? A_FALSE : A_TRUE;
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	xlt_rule.dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule.mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
#endif

	/*
	 * Fields for action
	 */
	xlt_action.cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
#ifdef NSS_PPE_SIXTH_TUPLE
	if (PPE_DRV_SIXTH_TUPLE_MODE_VLAN == ppe_drv_get_sixth_tuple_mode()) {
		xlt_action.svid_xlt_cmd = 0;
	} else {
		xlt_action.svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
	}
#else
	xlt_action.svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
#endif
	xlt_action.vsi_xlt = vsi_idx;
	xlt_action.vsi_xlt_enable = A_TRUE;

	rc = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS,
			&xlt_rule, &xlt_action);
	if (rc != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to delete old ingress vlan translation rule of port %d, error: %d\n", fal_port, rc);
		return PPE_DRV_RET_VLAN_INGRESS_DEL_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	/*
	 * VSI member set
	 */
	rv = fal_vsi_member_set(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if ( rv != SW_OK ) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("VSI member updated failed for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_SET;
	}

	/*
	 * Add egress vlan translation rule
	 */
	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	/*
	 * Fields for match
	 */
	xlt_rule.vsi_valid = A_TRUE;				/* Use vsi as search key */
	xlt_rule.vsi_enable = A_TRUE;				/* Use vsi as search key */
	xlt_rule.vsi = vsi_idx;					/* Use vsi as search key */
	xlt_rule.s_tagged = 0x7;				/* Accept tagged/untagged/priority tagged svlan */
	xlt_rule.c_tagged = 0x7;				/* Accept tagged/untagged/priority tagged cvlan */
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	xlt_rule.dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule.mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
#endif

	/*
	 * Fields for action
	 */
	xlt_action.cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action.cvid_xlt = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_action.svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action.svid_xlt = (info->svid == 0xFFFF) ? 0 : info->svid;

	/*
	 * Delete old egress vlan translation rule
	 */
	rc = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_EGRESS,
			&xlt_rule, &xlt_action);
	if (rc != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to delete old egress vlan translation of port %d, error: %d\n", fal_port, rc);
		return PPE_DRV_RET_VLAN_EGRESS_DEL_FAIL;
	}

	vlan->eg_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_del_xlate_rule);

/*
 * ppe_drv_vlan_add_xlate_rule()
 *	Add Ingress and Egress VLAN translation rules
 */
ppe_drv_ret_t ppe_drv_vlan_add_xlate_rule(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_vlan_trans_adv_rule_t xlt_rule_in, xlt_rule_eg;
	fal_vlan_trans_adv_action_t xlt_action_in, xlt_action_eg;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	struct ppe_drv_vsi *vsi;
	int vsi_idx, ret, rc;
	fal_port_t fal_port;
	fal_vsi_member_t vsi_member;
	uint32_t port_value = 0, vport_value = 0;
	sw_error_t rv;

	/*
	 * Check with vlan device created under bridge
	 */
	spin_lock_bh(&p->lock);
	if (info->br) {
		vsi = ppe_drv_iface_vsi_get(info->br);
	} else {
		vsi = ppe_drv_iface_vsi_get(iface);
	}

	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	vsi_idx = vsi->index;

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(info->port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, info->port_id)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, info->port_id);

	/*
	 * VSI member get
	 */
	port_value = FAL_PORT_ID_VALUE (fal_port);
	vport_value = port_value - SSDK_MIN_VIRTUAL_PORT_ID;
	ppe_drv_trace("port_id:0x%x, port_value:%d\n", fal_port, port_value);

	rv = fal_vsi_member_get(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if( rv != SW_OK ) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Invalid VSI memebr for a given VSI index: %d\n", vsi_idx);
		return PPE_DRV_RET_VSI_MEMBER_NOT_FOUND;
	}

	if(FAL_IS_VPORT(fal_port))
	{
		vsi_member.member_vports[vport_value/32] |= (1<<vport_value%32);
		ppe_drv_trace("vsi_member.member_vports[%d]: 0x%x\n",
				vport_value/32, vsi_member.member_vports[vport_value/32]);
	} else {
		vsi_member.member_ports |= (1<<port_value);
		ppe_drv_trace("vsi_member.member_ports :0x%x\n", vsi_member.member_ports);
	}

	/*
	 * Add new ingress vlan translation rule
	 */
	memset(&xlt_rule_in, 0, sizeof(xlt_rule_in));
	memset(&xlt_action_in, 0, sizeof(xlt_action_in));

	/*
	 * Fields for match
	 */
	xlt_rule_in.s_tagged = (info->svid == 0xFFFF) ? 0x1 : ((info->svid == 0) ? 0x2 : 0x4);
	xlt_rule_in.c_tagged = (info->cvid == 0xFFFF) ? 0x1 : ((info->cvid == 0) ? 0x2 : 0x4);
	xlt_rule_in.s_vid_enable = (info->svid == 0xFFFF) ? A_FALSE : A_TRUE;
	xlt_rule_in.s_vid = (info->svid == 0xFFFF) ? 0 : info->svid;
	xlt_rule_in.c_vid_enable = (info->cvid == 0xFFFF) ? A_FALSE : A_TRUE;
	xlt_rule_in.c_vid = (info->cvid == 0xFFFF) ? 0 : info->cvid;
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	xlt_rule_in.dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule_in.mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
#endif

	/*
	 * Fields for action
	 */
	xlt_action_in.cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;

	/*
	 * Keep the svid for use in flow match
	 */
#ifdef NSS_PPE_SIXTH_TUPLE
	if (PPE_DRV_SIXTH_TUPLE_MODE_VLAN == ppe_drv_get_sixth_tuple_mode()) {
		xlt_action_in.svid_xlt_cmd = 0;
	} else {
		xlt_action_in.svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
	}
#else
	xlt_action_in.svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
#endif
	xlt_action_in.vsi_xlt = vsi_idx;
	xlt_action_in.vsi_xlt_enable = A_TRUE;

	rc = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule_in,
			&xlt_action_in);
	if (rc != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_add);
		ppe_drv_warn("Failed to update ingress vlan translation of port %d, error: %d\n", fal_port, rc);
		return PPE_DRV_RET_INGRESS_VLAN_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule_in.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	/*
	 * VSI member set
	 */
	rv = fal_vsi_member_set(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);
	if ( rv != SW_OK ) {
		ppe_drv_warn("VSI member updated failed for a given VSI index: %d\n", vsi_idx);

		/*
		 * Deleting Ingress VLAN rule
		 */
		fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS,
				&xlt_rule_in, &xlt_action_in);

		/*
		 * Resetting HW index state to free.
		 */
		vlan->in_vlan_tbl[xlt_rule_in.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_VSI_MEMBER_NOT_SET;
	}

	/*
	 * Add egress vlan translation rule
	 */
	memset(&xlt_rule_eg, 0, sizeof(xlt_rule_eg));
	memset(&xlt_action_eg, 0, sizeof(xlt_action_eg));

	/*
	 * Fields for match
	 */
	xlt_rule_eg.vsi_valid = A_TRUE;				/* Use vsi as search key */
	xlt_rule_eg.vsi_enable = A_TRUE;			/* Use vsi as search key */
	xlt_rule_eg.vsi = vsi_idx;				/* Use vsi as search key */
	xlt_rule_eg.s_tagged = 0x7;				/* Accept tagged/untagged/priority tagged svlan */
	xlt_rule_eg.c_tagged = 0x7;				/* Accept tagged/untagged/priority tagged cvlan */
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	xlt_rule_eg.dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule_eg.mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
#endif

	/*
	 * Fields for action
	 */
	xlt_action_eg.cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action_eg.cvid_xlt = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_action_eg.svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action_eg.svid_xlt = (info->svid == 0xFFFF) ? 0 : info->svid;

	ret = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_EGRESS, &xlt_rule_eg,
			&xlt_action_eg);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to update egress translation rule for port: %d, error: %d\n",
				iface, fal_port, ret);

		/*
		 * Delete ingress vlan translation rule
		 */
		if (ret != SW_ALREADY_EXIST) {
			if(FAL_IS_VPORT(fal_port))
			{
				vsi_member.member_vports[vport_value/32] &= ~(1<<vport_value%32);
				ppe_drv_trace("vsi_member.member_vports[%d]: 0x%x\n",
						vport_value/32, vsi_member.member_vports[vport_value/32]);
			} else {
				vsi_member.member_ports &= ~(1<<port_value);
				ppe_drv_trace("vsi_member.member_ports :0x%x\n", vsi_member.member_ports);
			}
			fal_vsi_member_set(PPE_DRV_SWITCH_ID, vsi_idx, &vsi_member);

			/*
			 * Delete the ingress rule
			 */
			fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS,
					&xlt_rule_in, &xlt_action_in);

			/*
			 * Resetting HW table index state.
			 */
			vlan->in_vlan_tbl[xlt_rule_in.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
		}

		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_egress_vlan_add);

		return PPE_DRV_RET_EGRESS_VLAN_FAIL;
	}

	vlan->eg_vlan_tbl[xlt_rule_eg.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_add_xlate_rule);


/*
 * ppe_drv_vlan_over_bridge_del_ig_rule
 *	Delete ingress xlate rule for the iface
 */
ppe_drv_ret_t ppe_drv_vlan_over_bridge_del_ig_rule(struct ppe_drv_iface *slave_iface,
						   struct ppe_drv_iface *vlan_iface)
{
	uint32_t port_id, ret = 0;
	fal_port_t fal_port;
	fal_vlan_trans_adv_action_t xlt_action = {0};
	fal_vlan_trans_adv_rule_t xlt_rule =  {0};
	struct ppe_drv_vsi *vsi;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;

	spin_lock_bh(&p->lock);
	vsi = ppe_drv_iface_vsi_get(vlan_iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", vlan_iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	port_id = ppe_drv_iface_port_idx_get(slave_iface);

	if (port_id == -1) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("PortId is invalid for %s\n", slave_iface->dev->name);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_id)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_id);

	/*
	 * Field for match
	 */
	if (vsi->vlan.outer_vlan == PPE_DRV_VLAN_HDR_VLAN_NOT_CONFIGURED) {
		xlt_rule.s_tagged = FAL_PORT_VLAN_XLT_MATCH_UNTAGGED;
		xlt_rule.s_vid_enable = A_FALSE;
	} else {
		xlt_rule.s_tagged = FAL_PORT_VLAN_XLT_MATCH_TAGGED;
		xlt_rule.s_vid_enable = A_TRUE;
		xlt_rule.s_vid = vsi->vlan.outer_vlan;
	}
	xlt_rule.c_tagged = FAL_PORT_VLAN_XLT_MATCH_TAGGED;
	xlt_rule.c_vid_enable = A_TRUE;
	xlt_rule.c_vid = vsi->vlan.inner_vlan;

	/*
	 * Field for action
	 */
	xlt_action.vsi_xlt_enable = A_TRUE;
	xlt_action.vsi_xlt = vsi->index;

	ret = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule,
					  &xlt_action);

	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_over_bridge_del);
		ppe_drv_warn("Delete rule failed for %s portid %d ret %d\n", slave_iface->dev->name, port_id, ret);
		return PPE_DRV_RET_VLAN_INGRESS_DEL_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Delete ingress success rule Outer VID %d Inner VID %d fal_port %d dev %s port_id %d\n",
		       vsi->vlan.outer_vlan, vsi->vlan.inner_vlan, fal_port, slave_iface->dev->name, port_id);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_over_bridge_del_ig_rule);

/*
 * ppe_drv_vlan_over_bridge_add_ig_rule
 *	Installing ingress xlate rule for the iface
 */
ppe_drv_ret_t ppe_drv_vlan_over_bridge_add_ig_rule(struct ppe_drv_iface *slave_iface,
						   struct ppe_drv_iface *vlan_iface)
{
	uint32_t port_id, ret = 0;
	fal_port_t fal_port;
	fal_vlan_trans_adv_action_t xlt_action = {0};
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	struct ppe_drv_vsi *vsi;

	spin_lock_bh(&p->lock);
	vsi = ppe_drv_iface_vsi_get(vlan_iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", vlan_iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	port_id = ppe_drv_iface_port_idx_get(slave_iface);

	if (port_id == -1) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("PortId is invalid for %s\n", slave_iface->dev->name);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_id)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_id);

	/*
	 * Field for match
	 */
	if (vsi->vlan.outer_vlan == PPE_DRV_VLAN_HDR_VLAN_NOT_CONFIGURED) {
		xlt_rule.s_tagged = FAL_PORT_VLAN_XLT_MATCH_UNTAGGED;
		xlt_rule.s_vid_enable = A_FALSE;
	} else {
		xlt_rule.s_tagged = FAL_PORT_VLAN_XLT_MATCH_TAGGED;
		xlt_rule.s_vid_enable = A_TRUE;
		xlt_rule.s_vid = vsi->vlan.outer_vlan;
	}
	xlt_rule.c_tagged = FAL_PORT_VLAN_XLT_MATCH_TAGGED;
	xlt_rule.c_vid_enable = A_TRUE;
	xlt_rule.c_vid = vsi->vlan.inner_vlan;

	/*
	 * Field for action
	 */
	xlt_action.vsi_xlt_enable = A_TRUE;
	xlt_action.vsi_xlt = vsi->index;

	ret = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule,
					  &xlt_action);

	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_over_bridge_add);
		ppe_drv_warn("Add ingress rule failed for %s portid %d ret %d\n", slave_iface->dev->name, port_id, ret);
		return PPE_DRV_RET_INGRESS_VLAN_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Add ingress rule success Outer VID %d Inner VID %d fal_port %d dev %s port_id %d\n",
		      vsi->vlan.outer_vlan, vsi->vlan.inner_vlan, fal_port, slave_iface->dev->name, port_id);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_over_bridge_add_ig_rule);

/*
 * ppe_drv_vlan_deinit()
 *	De-Initialize VLAN interfaces
 */
void ppe_drv_vlan_deinit(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *port_if;
	struct ppe_drv_vsi *vsi;
	struct ppe_drv_port *pp;

	spin_lock_bh(&p->lock);
	vsi = ppe_drv_iface_vsi_get(iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI vlan iface", iface);
		return;
	}

	/*
	 * Detach VLAN vsi to port.
	 * Use base interface for double vlan
	 */
	port_if = ppe_drv_iface_base_get(iface);
	if (port_if->type == PPE_DRV_IFACE_TYPE_VLAN) {
		port_if = ppe_drv_iface_base_get(port_if);
	}

	pp = ppe_drv_iface_port_get(port_if);
	if (pp) {
		ppe_drv_port_vsi_detach(pp, vsi);
	}

	if (vsi) {
		ppe_drv_l3_if_deref(vsi->l3_if);
		ppe_drv_vsi_deref(vsi);
	}

	ppe_drv_iface_base_clear(iface);
	ppe_drv_iface_vsi_clear(iface);
	ppe_drv_iface_l3_if_clear(iface);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_vlan_deinit);

/*
 * ppe_drv_vlan_fdb_learn_disable()
 *	Configure VLAN based VSI FDB learning
 */
ppe_drv_ret_t ppe_drv_vlan_fdb_learn_disable(struct ppe_drv_iface *vlan_iface, bool vlan_fdb_learn_dis)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vsi *vsi;
	fal_vsi_newaddr_lrn_t newaddr_lrn = {0};

	spin_lock_bh(&p->lock);
	vsi = ppe_drv_iface_vsi_get(vlan_iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", vlan_iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	/*
	 * Set FDB learning in PPE
	 * If FDB learning is enabled on the given VSI, enable new MAC address learn exception.
	 */
	newaddr_lrn.lrn_en = !vlan_fdb_learn_dis;
	newaddr_lrn.action = FAL_MAC_FRWRD;
	if (!vlan_fdb_learn_dis && mac_lrn_exception_en) {
		newaddr_lrn.action = FAL_MAC_RDT_TO_CPU;
	}

	if (fal_vsi_newaddr_lrn_set(PPE_DRV_SWITCH_ID, vsi->index, &newaddr_lrn) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Failed to configure FDB learning %u", vlan_iface, vlan_fdb_learn_dis);
		return PPE_DRV_RET_NEW_ADDR_LRN_FAIL;
	}

	/*
	 * Update vsi shadow copy
	 */
	vsi->is_fdb_learn_enabled = !vlan_fdb_learn_dis;

	/*
	 * Flush FDB table for the VLAN vsi
	 */
	if (fal_fdb_entry_del_byfid(PPE_DRV_SWITCH_ID, vsi->index, FAL_FDB_DEL_STATIC) != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: failed to flush existing FDB entries", vlan_iface);
		return PPE_DRV_RET_FDB_FLUSH_VSI_FAIL;
	}

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_fdb_learn_disable);

/*
 * ppe_drv_vlan_init()
 *	VLAN init
 */
ppe_drv_ret_t ppe_drv_vlan_init(struct ppe_drv_iface *ppe_iface, struct net_device *base_dev, uint32_t vlan_id, bool vlan_over_bridge)
{
	struct ppe_drv_iface *base_if, *port_if;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_vsi *vsi;
	struct ppe_drv_port *pp;

	spin_lock_bh(&p->lock);
	base_if = ppe_drv_iface_get_by_dev_internal(base_dev);
	if (!base_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: NULL base interface for iface\n", ppe_iface);
		return PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
	}

	ppe_drv_iface_base_set(ppe_iface, base_if);

	vsi = ppe_drv_vsi_alloc(PPE_DRV_VSI_TYPE_VLAN);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: NULL VSI for iface\n", ppe_iface);
		return PPE_DRV_RET_VSI_ALLOC_FAIL;
	}

	if (vlan_over_bridge) {
		ppe_drv_trace("VLAN over bridge baseif dev %s iface dev %s base_dev %s vlan_id %d type %d\n",
			      base_if->dev->name, ppe_iface->dev->name, base_dev->name, vlan_id, base_if->type);
		if (!is_vlan_dev(base_dev)) {
			base_if->flags |= PPE_DRV_IFACE_VLAN_OVER_BRIDGE;
		}
	}

	ppe_drv_iface_vsi_set(ppe_iface, vsi);
	/*
	 * Set inner and outer vlan for a given VSI
	 */
	if (!ppe_drv_vsi_set_vlan(vsi, vlan_id, base_if)) {
		ppe_drv_vsi_deref(vsi);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Set vlan failed for iface\n", ppe_iface);
		return PPE_DRV_RET_VSI_ALLOC_FAIL;
	}

	l3_if = ppe_drv_vsi_l3_if_get_and_ref(vsi);
	if (!l3_if) {
		ppe_drv_vsi_deref(vsi);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: L3_IF not found for vsi(%p)\n", ppe_iface, vsi);
		return PPE_DRV_RET_FAILURE_NO_RESOURCE;
	}

	ppe_drv_iface_l3_if_set(ppe_iface, l3_if);

	/*
	 * Attach VLAN vsi to port.
	 * Use base interface for double vlan
	 */
	port_if = base_if;
	if (port_if->type == PPE_DRV_IFACE_TYPE_VLAN) {
		port_if = ppe_drv_iface_base_get(port_if);
	}

	pp = ppe_drv_iface_port_get(port_if);
	if (pp) {
		ppe_drv_port_vsi_attach(pp, vsi);
#ifdef NSS_PPE_SIXTH_TUPLE
		if (PPE_DRV_SIXTH_TUPLE_MODE_VLAN == ppe_drv_get_sixth_tuple_mode()) {
			fal_ip_vlan_as_flow_key_t flow_key;
			flow_key.valid = A_TRUE;
			flow_key.mode = FAL_IP_VLAN_AS_FLOW_KEY_FROM_OUTER;
			if (SW_OK != fal_ip_port_vlan_as_flow_key_set(PPE_DRV_SWITCH_ID, pp->port, &flow_key)) {
				ppe_drv_port_vsi_detach(pp, vsi);
				ppe_drv_iface_l3_if_clear(ppe_iface);
				ppe_drv_l3_if_deref(l3_if);
				ppe_drv_vsi_deref(vsi);
				spin_unlock_bh(&p->lock);
				ppe_drv_warn("failed to set VLAN_AS_FLOW_KEY for vp port:%d", pp->port);
				return PPE_DRV_RET_VLAN_VPN_MODE_SET_FAIL;
			}
		}
#endif
	}

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_init);

/*
 * ppe_drv_vlan_lag_slave_leave()
 *	lag slaves leave vlan
 */
ppe_drv_ret_t ppe_drv_vlan_lag_slave_leave(struct ppe_drv_iface *vlan_iface, struct net_device *slave_dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface;
	struct ppe_drv_port *port;
	struct ppe_drv_vsi *vsi;

	spin_lock_bh(&p->lock);
	vsi = ppe_drv_iface_vsi_get(vlan_iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", vlan_iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	iface = ppe_drv_iface_get_by_dev_internal(slave_dev);
	if (!iface) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%px: PPE interface cannot be found for slave %s\n", vlan_iface, slave_dev->name);
		return PPE_DRV_RET_IFACE_INVALID;
	}

	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to get port from iface of slave %s\n", iface, slave_dev->name);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/*
	 * Detach slave dev to vsi.
	 * This API is needed to attach port's l3_if of slave port back so that PPE
	 * can use l3_if associated with slave port instead of bond vlan interface.
	 */
	ppe_drv_port_vsi_detach(port, vsi);
	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_lag_slave_leave);

/*
 * ppe_drv_vlan_lag_slave_join()
 *	lag slaves join vlan
 */
ppe_drv_ret_t ppe_drv_vlan_lag_slave_join(struct ppe_drv_iface *vlan_iface, struct net_device *slave_dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface;
	struct ppe_drv_port *port;
	struct ppe_drv_vsi *vsi;

	spin_lock_bh(&p->lock);
	vsi = ppe_drv_iface_vsi_get(vlan_iface);
	if (!vsi) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Invalid VSI for given iface\n", vlan_iface);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	iface = ppe_drv_iface_get_by_dev_internal(slave_dev);
	if (!iface) {
		ppe_drv_warn("%px: PPE interface cannot be found for slave %s\n", vlan_iface, slave_dev->name);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_IFACE_INVALID;
	}

	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface of slave %s\n", iface, slave_dev->name);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/*
	 * Attach slave dev to vsi.
	 * This API is needed to detach port's l3_if of slave port so that PPE
	 * can use l3_if associated with bond vlan interface.
	 */
	ppe_drv_port_vsi_attach(port, vsi);
	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_lag_slave_join);

#ifdef PPE_TUNNEL_ENABLE
/*
 * ppe_drv_vlan_wlan_tun_encap_config
 *	Configure EG_XLAT_TUN_CTRL table for vlan wlan tunnel
 */
static bool ppe_drv_vlan_wlan_tun_encap_config(struct ppe_drv_tun_encap *ptec, int16_t vp_num)
{
	sw_error_t err;
	fal_tunnel_encap_cfg_t encap_cfg = {0};

	/*
	 * For WLAN VLAN custom tunnel, the encap entry is used to
	 * update the dest_info only.
	 */

	/*
	 * 0: encapsulation, 1:translation.
	 */
	encap_cfg.encap_type = 1;

	/*
	 * Encap entry used to update DEST_INFO.
	 * Update the target type accordingly.
	 */
	encap_cfg.encap_target = FAL_TUNNEL_ENCAP_TARGET_NO_UPDATE;
	encap_cfg.vport_en = A_TRUE;
	encap_cfg.vport = vp_num;

	/*
	 * Configure encap entry into HW
	 */
	err = fal_tunnel_encap_entry_add(PPE_DRV_SWITCH_ID, ptec->tun_idx, &encap_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%px: failed to configure encap entry at %u", ptec, ptec->tun_idx);
		return false;
	}

	ppe_drv_trace("%px:Added tunnel encap entry for vlan wlan tunnel vp_num %d tun_idx %d\n", ptec, vp_num, ptec->tun_idx);

	return true;
}

/*
 * ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach()
 *	Attach WLAN VP to the tunnel encap context.
 *
 * A reference is held on encap entry on success.
 */
bool ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach(struct ppe_drv_tun_encap *ptec, int16_t vp_num)
{
	sw_error_t err;
	uint32_t v_port;
	fal_vport_state_t vp_state = {0};

	/*
	 * deref:ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach.
	 */
	ppe_drv_tun_encap_ref(ptec);

	if (!ppe_drv_tun_encap_tun_idx_configure(ptec, vp_num, true)) {
		ppe_drv_warn("%p: Failed to configure tun index for vp %d", ptec, vp_num);
		ppe_drv_tun_encap_deref(ptec);
		return false;
	}

	v_port = FAL_PORT_ID(FAL_PORT_TYPE_VPORT, vp_num);

	err = fal_vport_state_check_get(PPE_DRV_SWITCH_ID, v_port, &vp_state);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get vp port state for port vp %d", ptec, vp_num);
		ppe_drv_tun_encap_deref(ptec);
		return false;
	}

	vp_state.eg_data_valid = A_TRUE;
	vp_state.vp_active = A_TRUE;
	err = fal_vport_state_check_set(PPE_DRV_SWITCH_ID, v_port, &vp_state);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to set vp state port vp %d", ptec, vp_num);
		ppe_drv_tun_encap_deref(ptec);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach);

/*
 * ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach()
 *	Detach WLAN VP from tunnel encap context
 *
 * Reference is decremented on tunnel encap entry on success.
 */
bool ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach(struct ppe_drv_tun_encap *ptec, int16_t vp_num)
{
	sw_error_t err;
	fal_vport_state_t vp_state = {0};
	uint32_t v_port = FAL_PORT_ID(FAL_PORT_TYPE_VPORT, vp_num);

	err = fal_vport_state_check_get(PPE_DRV_SWITCH_ID, v_port, &vp_state);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get vp port state for port vp %d", ptec, vp_num);
		return false;
	}

	vp_state.eg_data_valid = A_FALSE;
	vp_state.vp_active = A_FALSE;
	err = fal_vport_state_check_set(PPE_DRV_SWITCH_ID, v_port, &vp_state);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to reset vp state port vp %d", ptec, vp_num);
		return false;
	}

	if (!ppe_drv_tun_encap_tun_idx_configure(ptec, vp_num, false)) {
		ppe_drv_warn("%p: Failed to disable tun id for  vp %d", ptec, vp_num);
		return false;
	}

	/*
	 * ref:ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach.
	 */
	ppe_drv_tun_encap_deref(ptec);
	return true;
}
EXPORT_SYMBOL(ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach);

/*
 * ppe_drv_vlan_wlanif_tun_enc_destroy()
 *	Destroy WLAN VP tunnel encap context.
 */
void ppe_drv_vlan_wlanif_vp_tun_enc_destroy(struct ppe_drv_tun_encap *ptec)
{
	ppe_drv_tun_encap_deref(ptec);
}
EXPORT_SYMBOL(ppe_drv_vlan_wlanif_vp_tun_enc_destroy);

/*
 * ppe_drv_vlan_wlanif_tun_enc_setup()
 *	Allocate and configure tunnel encap context.
 *
 * API to allocate and configure tunnel encap context to update
 * DEST_INFO with WLAN Realdev vp number.
 */
struct ppe_drv_tun_encap *ppe_drv_vlan_wlanif_tun_enc_setup(struct net_device *dev, int16_t vp_num)
{
	struct ppe_drv_tun_encap *ptec = NULL;
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	ptec = ppe_drv_tun_encap_alloc(p);
	if (!ptec) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p:%s:Failed to allocate tun encap entry for wlan vlan vp %d\n", dev, dev->name, vp_num);
		return NULL;
	}
	spin_unlock_bh(&p->lock);

	if (!ppe_drv_vlan_wlan_tun_encap_config(ptec, vp_num)) {
		ppe_drv_tun_encap_deref(ptec);
		ppe_drv_warn("%p:%s:Failed to configure encap entry for wlan vlan vp %d\n", dev, dev->name, vp_num);
		return NULL;
	}

	return ptec;
}
EXPORT_SYMBOL(ppe_drv_vlan_wlanif_tun_enc_setup);
#endif /* PPE_TUNNEL_ENABLE */

/*
 * ppe_drv_vlan_entries_free()
 *	Free vlan instance.
 */
void ppe_drv_vlan_entries_free(struct ppe_drv_vlan_tbl *vlan)
{
	vfree(vlan);
}

/*
 * ppe_drv_vlan_entries_alloc()
 *	Allocates VLAN entries.
 */
struct ppe_drv_vlan_tbl *ppe_drv_vlan_entries_alloc(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan;
	uint16_t i;

	vlan = vzalloc(sizeof(struct ppe_drv_vlan_tbl));
	if (!vlan) {
		ppe_drv_warn("%p: Failed to allocate VLAN table entries", p);
		return NULL;
	}

	/*
	 * Initialize ingress vlan_tbl
	 */
	for (i = 0; i < PPE_DRV_VLAN_HW_ID_IV_MAX; i++) {
		vlan->in_vlan_tbl[i].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
		vlan->in_vlan_tbl[i].ctx = NULL;
	}

	/*
	 * Initialize egress vlan_tbl
	 */
	for (i = 0; i < PPE_DRV_VLAN_HW_ID_EG_MAX; i++) {
		vlan->eg_vlan_tbl[i].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
		vlan->eg_vlan_tbl[i].ctx = NULL;
	}

	return vlan;
}

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
/*
 * ppe_drv_vlan_gen_hw_id_get()
 *	Get an available hw_id from the specified direction's vlan table.
 *	NOTE: Caller must hold p->lock
 */
static int16_t ppe_drv_vlan_hw_id_get(ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	int16_t id;

	/*
	 * Check if VLAN structure is initialized
	 */
	if (!p->vlan) {
		ppe_drv_warn("VLAN structure not initialized");
		return -1;
	}

	/*
	 * Select the appropriate table based on direction
	 */
	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		/*
		 * Ingress direction: Use vlan_in_tbl
		 */
		for (id = PPE_DRV_VLAN_HW_ID_IV_END; id >= PPE_DRV_VLAN_HW_ID_START; id--) {
			if (vlan->in_vlan_tbl[id].hw_id_state == PPE_DRV_VLAN_HW_ID_FREE) {
				vlan->in_vlan_tbl[id].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;
				ppe_drv_trace("Ingress tbl_state: %d, hw_id: %d\n",
						vlan->in_vlan_tbl[id].hw_id_state, id);
				return id;
			}
		}
	} else if (rule_dir == PPE_DRV_RULE_EGRESS) {
		/*
		 * Egress direction: Use eg_vlan_tbl
		 */
		for (id = PPE_DRV_VLAN_HW_ID_EG_END; id >= PPE_DRV_VLAN_HW_ID_START; id--) {
			if (vlan->eg_vlan_tbl[id].hw_id_state == PPE_DRV_VLAN_HW_ID_FREE) {
				vlan->eg_vlan_tbl[id].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;
				ppe_drv_trace("Egress tbl_state: %d, hw_id: %d\n",
						vlan->eg_vlan_tbl[id].hw_id_state, id);
				return id;
			}
		}
	} else {
		ppe_drv_warn("Invalid rule direction: %d", rule_dir);
		return -1;
	}

	/*
	 * No free hw_id found
	 */
	ppe_drv_warn("No available hw_id for direction %d", rule_dir);
	return -1;
}

/*
 * ppe_drv_vlan_ctx_iface_get()
 *	Return the iface associated with VLAN context.
 */
struct ppe_drv_iface *ppe_drv_vlan_ctx_iface_get(struct ppe_drv_vlan_ctx *ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface = NULL;

	spin_lock_bh(&p->lock);
	if (ctx) {
		iface = ctx->iface;
	}
	spin_unlock_bh(&p->lock);
	return iface;
}
EXPORT_SYMBOL(ppe_drv_vlan_ctx_iface_get);

/*
 * ppe_drv_vlan_alloc()
 *	Allocate ctx for VLAN rules.
 */
struct ppe_drv_vlan_ctx *ppe_drv_vlan_alloc(ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv_vlan_ctx *ctx = NULL;
	int16_t hw_id = -1;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;

	spin_lock_bh(&p->lock);

	hw_id = ppe_drv_vlan_hw_id_get(rule_dir);
	if (hw_id < 0) {
		ppe_drv_warn("No available hw_id in VLAN table for direction: %d", rule_dir);
		spin_unlock_bh(&p->lock);
		return NULL;
	}

	ctx = kzalloc(sizeof(struct ppe_drv_vlan_ctx), GFP_ATOMIC);
	if (!ctx) {
		ppe_drv_warn("No free ctx \n");
		spin_unlock_bh(&p->lock);
		return NULL;
	}

	ctx->entry_index = hw_id;
	ctx->rule_dir = rule_dir;

	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		vlan->in_vlan_tbl[hw_id].ctx = ctx;
	} else {
		vlan->eg_vlan_tbl[hw_id].ctx = ctx;
	}

	spin_unlock_bh(&p->lock);

	return ctx;
}
EXPORT_SYMBOL(ppe_drv_vlan_alloc);

/*
 * ppe_drv_vlan_hw_id_return()
 *      Return a hw id  to free pool.
 */
static void ppe_drv_vlan_hw_id_return(int16_t id, ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv_vlan_tbl *vlan = ppe_drv_gbl->vlan;

	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		vlan->in_vlan_tbl[id].ctx = NULL;
		vlan->in_vlan_tbl[id].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
	} else {
		vlan->eg_vlan_tbl[id].ctx = NULL;
		vlan->eg_vlan_tbl[id].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
	}
}

/*
 * ppe_drv_vlan_rule_fill()
 *	Fill rule relation information.
 */
static bool ppe_drv_vlan_rule_fill(struct ppe_drv_vlan_ctx *ctx, struct ppe_drv_vlan_cfg *info)
{
	fal_vlan_trans_adv_rule_t *fal_rule = &ctx->fal_rule;
	struct ppe_drv_vlan_rule_match *rule = &info->rule_f;

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_PORT_TYPE) {
		int32_t port_info;
		fal_port_t fal_port;

		switch(rule->port_type) {
			case PPE_DRV_VLAN_PORT_TYPE_BITMAP:
				fal_port = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, rule->port_val);
				fal_rule->port_bitmap = fal_port;
				break;
			case PPE_DRV_VLAN_PORT_TYPE_PORT:
				ctx->iface = ppe_drv_iface_get_by_dev(info->src_dev);
				if (!ctx->iface) {
					ppe_drv_warn("Failed to get iface for interface: %s\n", info->src_dev->name);
					return false;
				}

#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
				if (ppe_drv_veip_is_enabled(info->src_dev)) {
					port_info = ppe_drv_veip_get_port(ctx->iface, PPE_DRV_PORT_VIRTUAL_PON);
				} else
#endif
				{
					port_info = ppe_drv_iface_port_idx_get(ctx->iface);
				}

				if (port_info == PPE_DRV_INVALID_PORT) {
					ppe_drv_warn("Invalid port id: %d\n", port_info);
					return false;
				}

				fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_info) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_info)
					: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_info);

				if (PPE_DRV_VIRTUAL_PORT_CHK(port_info)) {
					fal_rule->port_bitmap = fal_port;
				} else {
					fal_rule->port_bitmap = (1ULL << fal_port);
				}
				break;
			case PPE_DRV_VLAN_PORT_TYPE_GEMPORT:
				fal_port = FAL_PORT_ID(FAL_PORT_TYPE_GEM_PORT, rule->port_val);
				fal_rule->port_bitmap = fal_port;
				break;
		}

		ppe_drv_trace("%p: rule PORT: 0x%x", ctx, fal_rule->port_bitmap);

	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_STAG_FORMAT) {
		fal_rule->s_tagged = rule->stag_format;
	} else {
		/*
		 * Default tag format value.
		 */
		fal_rule->s_tagged = (PPE_DRV_VLAN_XLT_MATCH_UNTAGGED | PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_TAGGED);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_SVID_VAL) {
		fal_rule->s_vid = rule->svid;
		fal_rule->s_vid_enable = A_TRUE;
		ppe_drv_trace("%p: rule SVID: %d", ctx, fal_rule->s_vid);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_SPCP_VAL) {
		fal_rule->s_pcp = rule->spcp;
		fal_rule->s_pcp_enable = A_TRUE;
		ppe_drv_trace("%p: rule SPCP: %d", ctx, fal_rule->s_pcp);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_SDEI_VAL) {
		fal_rule->s_dei = rule->sdei;
		fal_rule->s_dei_enable = A_TRUE;
		ppe_drv_trace("%p: rule SDEI: %d", ctx, fal_rule->s_dei);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_CTAG_FORMAT) {
		fal_rule->c_tagged = rule->ctag_format;
	} else {
		/*
		 * Default tag format value.
		 */
		fal_rule->c_tagged = (PPE_DRV_VLAN_XLT_MATCH_UNTAGGED | PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_TAGGED);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_CVID_VAL) {
		fal_rule->c_vid = rule->cvid;
		fal_rule->c_vid_enable = A_TRUE;
		ppe_drv_trace("%p: rule CVID: %d", ctx, fal_rule->c_vid);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_CPCP_VAL) {
		fal_rule->c_pcp = rule->cpcp;
		fal_rule->c_pcp_enable = A_TRUE;
		ppe_drv_trace("%p: rule CPCP: %d", ctx, fal_rule->c_pcp);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_CDEI_VAL) {
		fal_rule->c_dei = rule->cdei;
		fal_rule->c_dei_enable = A_TRUE;
		ppe_drv_trace("%p: rule CDEI: %d", ctx, fal_rule->c_dei);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_PROTO_VAL) {
		fal_rule->protocol = rule->proto;
		fal_rule->protocol_enable = A_TRUE;
		ppe_drv_trace("%p: rule PROTOCOL: 0x%x", ctx, fal_rule->protocol);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_FTYPE_VAL) {
		switch (rule->frame_type) {
			case PPE_DRV_VLAN_FRAME_TYPE_ETHERNET:
				fal_rule->frmtype = FAL_FRAMETYPE_ETHERNET;
				break;
			case PPE_DRV_VLAN_FRAME_TYPE_RFC_1024:
				fal_rule->frmtype = FAL_FRAMETYPE_RFC_1024;
				break;
			case PPE_DRV_VLAN_FRAME_TYPE_LLC_OTHER:
				fal_rule->frmtype = FAL_FRAMETYPE_LLC_OTHER;
				break;
			case PPE_DRV_VLAN_FRAME_TYPE_ETHORRFC1024:
				fal_rule->frmtype = FAL_FRAMETYPE_ETHORRFC1024;
				break;
		}
		fal_rule->frmtype_enable = A_TRUE;
		ppe_drv_trace("%p: rule FTYPE: %d", ctx, fal_rule->frmtype);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_VSI_VAL) {
		fal_rule->vsi = rule->vsi;
		fal_rule->vsi_enable = A_TRUE;
		fal_rule->vsi_valid = A_TRUE;
		ppe_drv_trace("%p: rule VSI: %d", ctx, fal_rule->vsi);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_TYP) {
		switch (rule->vni_resv_type) {
			case PPE_DRV_VLAN_VNI_RESV_TYPE_VNI_ONLY:
				fal_rule->vni_resv_type = 0;
				break;
			case PPE_DRV_VLAN_VNI_RESV_TYPE_VNI_RESV:
				fal_rule->vni_resv_type = 1;
				break;
		}
		ppe_drv_trace("%p: rule VNI_TYPE: %d", ctx, fal_rule->vni_resv_type);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_VAL) {
		fal_rule->vni_resv = rule->vni_resv;
		fal_rule->vni_resv_enable = A_TRUE;
		ppe_drv_trace("%p: rule VNI: %d", ctx, fal_rule->vni_resv);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_STPID) {
		fal_rule->stpid_idx = rule->stpid;
		fal_rule->stpid_idx_en = A_TRUE;
		ppe_drv_trace("%p: rule STPID: 0x%x", ctx, rule->stpid);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_CTPID) {
		fal_rule->ctpid_idx = rule->ctpid;
		fal_rule->ctpid_idx_en = A_TRUE;
		ppe_drv_trace("%p: rule CTPID: 0x%x", ctx, rule->ctpid);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_DHCP_TYPE) {
		fal_rule->dhcp_type = rule->dhcp_type;
		ppe_drv_trace("%p: rule DHCP_TYPE: %d", ctx, fal_rule->dhcp_type);
	} else {
		/*
		 * Default value.
		 */
		fal_rule->dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	}

	if (rule->flags & PPE_DRV_VLAN_RULE_FLAG_MC_TYPE) {
		fal_rule->mc_type = rule->mc_type;
		ppe_drv_trace("%p: rule MC_TYPE: %d", ctx, fal_rule->mc_type);
	} else {
		/*
		 * Default value.
		 */
		fal_rule->mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
	}

	return true;
}

/*
 * ppe_drv_vlan_action_fill()
 *	Fill action relation information.
 */
static bool ppe_drv_vlan_action_fill(struct ppe_drv_vlan_ctx *ctx, struct ppe_drv_vlan_cfg *info)
{
	fal_vlan_trans_adv_action_t *fal_action = &ctx->fal_action;
	struct ppe_drv_vlan_action *action = &info->action_f;

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_VID_SWP) {
		fal_action->swap_svid_cvid = action->swap_svid_cvid;
		ppe_drv_trace("%p: action VID_SWP: %d", ctx, fal_action->swap_svid_cvid);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SVID_XLT_CMD) {
		switch ( action->svid_xlate_cmd) {
			case PPE_DRV_VLAN_VID_XLT_CMD_UNCHANGED:
				fal_action->svid_xlt_cmd = FAL_VID_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_ADDORREPLACE:
				fal_action->svid_xlt_cmd = FAL_VID_XLT_CMD_ADDORREPLACE;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_DELETE:
				fal_action->svid_xlt_cmd = FAL_VID_XLT_CMD_DELETE;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_CP_SVID:
				fal_action->svid_xlt_cmd = FAL_VID_XLT_CMD_CPFRM_SVID;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_CP_CVID:
				fal_action->svid_xlt_cmd = FAL_VID_XLT_CMD_CPFRM_CVID;
				break;
		}
		ppe_drv_trace("%p: action SVID_XLT_CMD: %d", ctx, fal_action->svid_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SVID_XLT_VAL) {
		fal_action->svid_xlt = action->svidxlate;
		ppe_drv_trace("%p: action SVID_XLT: %d", ctx, fal_action->svid_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CVID_XLT_CMD) {
		switch ( action->cvid_xlate_cmd) {
			case PPE_DRV_VLAN_VID_XLT_CMD_UNCHANGED:
				fal_action->cvid_xlt_cmd = FAL_VID_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_ADDORREPLACE:
				fal_action->cvid_xlt_cmd = FAL_VID_XLT_CMD_ADDORREPLACE;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_DELETE:
				fal_action->cvid_xlt_cmd = FAL_VID_XLT_CMD_DELETE;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_CP_SVID:
				fal_action->cvid_xlt_cmd = FAL_VID_XLT_CMD_CPFRM_SVID;
				break;
			case PPE_DRV_VLAN_VID_XLT_CMD_CP_CVID:
				fal_action->cvid_xlt_cmd = FAL_VID_XLT_CMD_CPFRM_CVID;
				break;
		}
		ppe_drv_trace("%p: action CVID_XLT_CMD: %d", ctx, fal_action->cvid_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CVID_XLT_VAL) {
		fal_action->cvid_xlt = action->cvidxlate;
		ppe_drv_trace("%p: action CVID_XLT: %d", ctx, fal_action->cvid_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_PCP_SWP) {
		fal_action->swap_spcp_cpcp = action->swap_spcp_cpcp;
		ppe_drv_trace("%p: action PCP_SWP: %d", ctx, fal_action->swap_spcp_cpcp);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SPCP_XLT_CMD) {
		switch ( action->spcp_xlate_cmd) {
			case PPE_DRV_VLAN_PCP_XLT_CMD_UNCHANGED:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_REPLACE:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_REPLACE;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_CP_SPCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_CPFRM_SPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_CP_CPCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_CPFRM_CPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_DSCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_MAPFRM_DSCP;
				if (action->dscp_p_bit_map_ind == PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0) {
					fal_action->dscp_map_idx = 0;
				} else {
					fal_action->dscp_map_idx = 1;
				}
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_REP_PCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_REPLACE;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_SPCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_CPFRM_SPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_CPCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_CPFRM_CPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP:
				fal_action->spcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_MAPFRM_DSCP;
				if (action->dscp_p_bit_map_ind == PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0) {
					fal_action->dscp_map_idx = 0;
				} else {
					fal_action->dscp_map_idx = 1;
				}
				break;
		}
		ppe_drv_trace("%p: action SPCP_XLT_CMD: %d", ctx, fal_action->spcp_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SPCP_XLT_VAL) {
		fal_action->spcp_xlt = action->spcptranslation;
		ppe_drv_trace("%p: action SPCP_XLT: %d", ctx, fal_action->spcp_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CPCP_XLT_CMD) {
		switch ( action->cpcp_xlate_cmd) {
			case PPE_DRV_VLAN_PCP_XLT_CMD_UNCHANGED:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_REPLACE:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_REPLACE;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_CP_SPCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_CPFRM_SPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_CP_CPCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_CPFRM_CPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_DSCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_MAPFRM_DSCP;
				if (action->dscp_p_bit_map_ind == PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0) {
					fal_action->dscp_map_idx = 0;
				} else {
					fal_action->dscp_map_idx = 1;
				}
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_REP_PCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_REPLACE;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_SPCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_CPFRM_SPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_CPCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_CPFRM_CPCP;
				break;
			case PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP:
				fal_action->cpcp_xlt_cmd = FAL_PCP_XLT_CMD_ADD_TAG_AND_MAPFRM_DSCP;
				if (action->dscp_p_bit_map_ind == PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0) {
					fal_action->dscp_map_idx = 0;
				} else {
					fal_action->dscp_map_idx = 1;
				}
				break;
		}
		ppe_drv_trace("%p: action CPCP_XLT_CMD: %d", ctx, fal_action->cpcp_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CPCP_XLT_VAL) {
		fal_action->cpcp_xlt = action->cpcptranslation;
		ppe_drv_trace("%p: action CPCP_XLT: %d", ctx, fal_action->cpcp_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_DEI_SWP) {
		fal_action->swap_sdei_cdei = action->swap_sdei_cdei;
		ppe_drv_trace("%p: action DEI_SWP: %d", ctx, fal_action->swap_sdei_cdei);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SDEI_XLT_CMD) {
		switch ( action->sdei_xlate_cmd) {
			case PPE_DRV_VLAN_DEI_XLT_CMD_UNCHANGED:
				fal_action->sdei_xlt_cmd = FAL_DEI_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_DEI_XLT_CMD_REPLACE:
				fal_action->sdei_xlt_cmd = FAL_DEI_XLT_CMD_REPLACE;
				break;
			case PPE_DRV_VLAN_DEI_XLT_CMD_CP_SDEI:
				fal_action->sdei_xlt_cmd = FAL_DEI_XLT_CMD_CPFRM_SDEI;
				break;
			case PPE_DRV_VLAN_DEI_XLT_CMD_CP_CDEI:
				fal_action->sdei_xlt_cmd = FAL_DEI_XLT_CMD_CPFRM_CDEI;
				break;
		}
		ppe_drv_trace("%p: action SDEI_XLT_CMD: %d", ctx, fal_action->sdei_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SDEI_XLT_VAL) {
		fal_action->sdei_xlt = action->sdeitranslation;
		ppe_drv_trace("%p: action SDEI_XLT: %d", ctx, fal_action->sdei_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CDEI_XLT_CMD) {
		switch ( action->cdei_xlate_cmd) {
			case PPE_DRV_VLAN_DEI_XLT_CMD_UNCHANGED:
				fal_action->cdei_xlt_cmd = FAL_DEI_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_DEI_XLT_CMD_REPLACE:
				fal_action->cdei_xlt_cmd = FAL_DEI_XLT_CMD_REPLACE;
				break;
			case PPE_DRV_VLAN_DEI_XLT_CMD_CP_SDEI:
				fal_action->cdei_xlt_cmd = FAL_DEI_XLT_CMD_CPFRM_SDEI;
				break;
			case PPE_DRV_VLAN_DEI_XLT_CMD_CP_CDEI:
				fal_action->cdei_xlt_cmd = FAL_DEI_XLT_CMD_CPFRM_CDEI;
				break;
		}
		ppe_drv_trace("%p: action CDEI_XLT_CMD: %d", ctx, fal_action->cdei_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CDEI_XLT_VAL) {
		fal_action->cdei_xlt = action->cdeitranslation;
		ppe_drv_trace("%p: action CDEI_XLT: %d", ctx, fal_action->cdei_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_TAGS_TO_REMOVE) {
		fal_action->tags_to_rm = action->tags_to_remove;
		ppe_drv_trace("%p: action TAGS_TO_REMOVE: %d", ctx, fal_action->tags_to_rm);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_STPID_CMD) {
		switch (action->stpid_cmd) {
			case PPE_DRV_VLAN_TPID_CMD_UNCHANGED:
				fal_action->stpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_TPID_CMD_REPLACE:
				fal_action->stpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_REPLACE;
				break;
			case PPE_DRV_VLAN_TPID_CMD_CP_STPID:
				fal_action->stpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_CPFRM_STPID_IDX;
				break;
			case PPE_DRV_VLAN_TPID_CMD_CP_CTPID:
				fal_action->stpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_CPFRM_CTPID_IDX;
				break;
		}
		ppe_drv_trace("%p: action STPID_CMD: %d", ctx, fal_action->stpid_idx_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_STPID) {
		fal_action->stpid_idx_xlt = action->stpid_action;
		ppe_drv_trace("%p: action STPID: %d", ctx, action->stpid_action);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CTPID_CMD) {
		switch (action->ctpid_cmd) {
			case PPE_DRV_VLAN_TPID_CMD_UNCHANGED:
				fal_action->ctpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_UNCHANGED;
				break;
			case PPE_DRV_VLAN_TPID_CMD_REPLACE:
				fal_action->ctpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_REPLACE;
				break;
			case PPE_DRV_VLAN_TPID_CMD_CP_STPID:
				fal_action->ctpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_CPFRM_STPID_IDX;
				break;
			case PPE_DRV_VLAN_TPID_CMD_CP_CTPID:
				fal_action->ctpid_idx_xlt_cmd = FAL_TPID_IDX_XLT_CMD_CPFRM_CTPID_IDX;
				break;
		}
		ppe_drv_trace("%p: action CTPID_CMD: %d", ctx, fal_action->ctpid_idx_xlt_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CTPID) {
		fal_action->ctpid_idx_xlt = action->ctpid_action;
		ppe_drv_trace("%p: action CTPID: %d", ctx, action->ctpid_action);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CNTR_ID) {
		fal_action->counter_id = action->counter_id;
		fal_action->counter_enable = A_TRUE;
		ppe_drv_trace("%p: action COUNTER_ID: %d", ctx, fal_action->counter_id);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_CNTR_MODE) {
		switch (action->counter_mode) {
			case PPE_DRV_VLAN_COUNTER_MODE_VLAN:
				fal_action->counter_mode = 0;
				break;
			case PPE_DRV_VLAN_COUNTER_MODE_PONPM:
				fal_action->counter_mode = 1;
				break;
		}
		ppe_drv_trace("%p: action COUNTER_MODE: %d", ctx, fal_action->counter_mode);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_VSI_XLT_VAL) {
		fal_action->vsi_xlt = action->vsitranslation;
		fal_action->vsi_xlt_enable = A_TRUE;
		ppe_drv_trace("%p: action VSI: %d", ctx, fal_action->vsi_xlt);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SRC_INFO_TYP) {
		switch (action->src_info_type) {
			case PPE_DRV_VLAN_SRC_INFO_TYPE_VP:
				fal_action->src_info_type = action->src_info_type;
				break;
			case PPE_DRV_VLAN_SRC_INFO_TYPE_L3IF:
				fal_action->src_info_type = action->src_info_type;
				break;
		}
		ppe_drv_trace("%p: action SRC_INFO_TYPE: %d", ctx, fal_action->src_info_type);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SRC_INFO_VAL) {
		fal_action->src_info = action->src_info;
		fal_action->src_info_enable = A_TRUE;
		ppe_drv_trace("%p: action SRC_INFO: %d", ctx, fal_action->src_info);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_VNI_RESV_VAL) {
		fal_action->vni_resv = action->vni_resv_action;
		fal_action->vni_resv_enable = action->vni_resv_enable_action;
		ppe_drv_trace("%p: action VNI_RESV: %d", ctx, fal_action->vni_resv);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_FWD_CMD) {
		switch (action->fwd_cmd) {
			case PPE_DRV_VLAN_FWD_CMD_FORWARD:
				fal_action->fwd_cmd = FAL_MAC_FRWRD;
				break;
			case PPE_DRV_VLAN_FWD_CMD_DROP:
				fal_action->fwd_cmd = FAL_MAC_DROP;
				break;
			case PPE_DRV_VLAN_FWD_CMD_COPY:
				fal_action->fwd_cmd = FAL_MAC_CPY_TO_CPU;
				break;
			case PPE_DRV_VLAN_FWD_CMD_REDIRECT:
				fal_action->fwd_cmd = FAL_MAC_RDT_TO_CPU;
				break;
		}
		ppe_drv_trace("%p: action FWD_CMD: %d", ctx, fal_action->fwd_cmd);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_SVC_CODE) {
		fal_action->svc_code_en = A_TRUE;
		fal_action->svc_code = action->sc;
		ppe_drv_trace("%p: action SC: %d", ctx, fal_action->svc_code);
	}

	if (action->flags & PPE_DRV_VLAN_ACTION_FLAG_DEST_INFO) {
		uint32_t port_info = 0;
		fal_port_t fal_port;

		port_info = ppe_drv_port_num_from_dev(info->dst_dev);

		fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_info) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_info)
			: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_info);
		fal_action->dst_valid = A_TRUE;
		fal_action->dst_port.dest_info_type = FAL_DEST_INFO_PORT_ID;
		fal_action->dst_port.dest_info_value = (1UL << fal_port);

		ppe_drv_trace("%p: action DEST_INFO: %d", ctx, fal_action->dst_port.dest_info_value);
	}

	return true;
}

/*
 * ppe_drv_vlan_fill()
 *	Fill VLAN rule and action information
 */
static bool ppe_drv_vlan_fill(struct ppe_drv_vlan_ctx *ctx, struct ppe_drv_vlan_cfg *info)
{
	if (!ppe_drv_vlan_rule_fill(ctx, info)) {
		ppe_drv_warn("%p: VLAN rule fill fail: %p\n", ctx, info);
		return false;
	}

	if (!ppe_drv_vlan_action_fill(ctx, info)) {
		ppe_drv_warn("%p: VLAN action fill fail: %p\n", ctx, info);
		return false;
	}

	return true;
}

/*
 * ppe_drv_vlan_rule_create
 *	Create the VLAN rule in PPE.
 */
ppe_drv_ret_t ppe_drv_vlan_rule_create(struct ppe_drv_vlan_ctx *ctx, struct ppe_drv_vlan_cfg *rule)
{
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t error = SW_OK;

	if (!ppe_drv_vlan_fill(ctx, rule)) {
		ppe_drv_warn("%p: Invalid VLAN rule %p\n", ctx, rule);
		return PPE_DRV_RET_VLAN_RULE_INVALID;
	}

	spin_lock_bh(&p->lock);

	if (rule->rule_dir == PPE_DRV_RULE_INGRESS) {
		/*
		 * Upstream direction: Configure VLAN_XLT_RULE.
		 */
		error = fal_port_vlan_trans_adv_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_INGRESS, ctx->entry_index, &ctx->fal_rule, &ctx->fal_action);
		if (error != SW_OK) {
			ppe_drv_warn("Failed to update ingress translation rule for port: %d, error: %d\n",
					ctx->fal_rule.port_bitmap, error);

			spin_unlock_bh(&p->lock);
			ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_add);
			return PPE_DRV_RET_INGRESS_VLAN_FAIL;
		}
	} else {
		/*
		 * Downstream direction: Configure EG_VLAN_XLT_RULE.
		 */
		error = fal_port_vlan_trans_adv_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_EGRESS, ctx->entry_index, &ctx->fal_rule, &ctx->fal_action);
		if (error != SW_OK) {
			ppe_drv_warn("Failed to update egress translation rule for port: %d, error: %d\n",
					ctx->fal_rule.port_bitmap, error);

			spin_unlock_bh(&p->lock);
			ppe_drv_stats_inc(&p->stats.gen_stats.fail_egress_vlan_add);
			return PPE_DRV_RET_EGRESS_VLAN_FAIL;
		}
	}

	ctx->rule_valid = true;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("VLAN rule created successfully\n");
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_rule_create);

/*
 * ppe_drv_vlan_ctx_free
 *	Free the VLAN rule contexts.
 */
void ppe_drv_vlan_ctx_free(struct ppe_drv_vlan_ctx *ctx)
{
	kfree(ctx);
}

/*
 * ppe_drv_vlan_destroy
 *	Destroy the VLAN rule and context.
 */
void ppe_drv_vlan_destroy(struct ppe_drv_vlan_ctx *ctx)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};

	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	spin_lock_bh(&p->lock);
	if (ctx->rule_valid) {
		if (ctx->rule_dir == PPE_DRV_RULE_INGRESS) {
			error = fal_port_vlan_trans_adv_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_INGRESS, ctx->entry_index, &xlt_rule, &xlt_action);
			if (error != SW_OK) {
				ppe_drv_warn("Failed to delete old ingress vlan translation rule with error: %d\n", error);
			}
		} else {
			error = fal_port_vlan_trans_adv_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_EGRESS, ctx->entry_index, &xlt_rule, &xlt_action);
			if (error != SW_OK) {
				ppe_drv_warn("Failed to delete old egress vlan translation rule with error: %d\n", error);
			}
		}
	}

	ppe_drv_info("%p: VLAN rule destroy is successful ", ctx);

	ppe_drv_vlan_hw_id_return(ctx->entry_index, ctx->rule_dir);
	ppe_drv_vlan_ctx_free(ctx);
	spin_unlock_bh(&p->lock);

}
EXPORT_SYMBOL(ppe_drv_vlan_destroy);
#endif

#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
/*
 * ppe_drv_vlan_ingress_rule_action_set_veip()
 *	Set xlt_rule and xlt_action structure for ingress.
 */
void ppe_drv_vlan_ingress_rule_action_set_veip(fal_vlan_trans_adv_rule_t *xlt_rule,
		fal_vlan_trans_adv_action_t *xlt_action, struct ppe_drv_vlan_xlate_info *info)
{
	/*
	 * Field for ingress match.
	 * Accept tagged/untagged/priority tagged svlan and cvlan.
	 */
	xlt_rule->s_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED
			| FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	xlt_rule->c_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED
			| FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	xlt_rule->c_vid = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_rule->c_vid_enable = (info->cvid == 0xFFFF) ? A_FALSE : A_TRUE;
	xlt_rule->s_vid = (info->svid == 0xFFFF) ? 0 : info->svid;
	xlt_rule->s_vid_enable = (info->svid == 0xFFFF) ? A_FALSE : A_TRUE;
	xlt_rule->dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule->mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);

	/*
	 * field for ingress action.
	 */
	xlt_action->cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
	xlt_action->svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_DELETE;
	xlt_action->src_info_enable = A_TRUE;
	xlt_action->src_info = info->port_id;
	xlt_action->src_info_type = FAL_CHG_SRC_TYPE_VP;

	ppe_drv_trace("Ingress VLAN: s_tagged=0x%x c_tagged=0x%x s_vid=%u c_vid=%u "
			"svid_cmd=%d svid=%u cvid_cmd=%d c_vid=%u src_info=%u src_type=%u\n",
			xlt_rule->s_tagged, xlt_rule->c_tagged, xlt_rule->s_vid, xlt_rule->c_vid,
			xlt_action->svid_xlt_cmd, xlt_action->svid_xlt, xlt_action->cvid_xlt_cmd,
			xlt_action->cvid_xlt, xlt_action->src_info, xlt_action->src_info_type);
}

/*
 * ppe_drv_vlan_egress_rule_action_set_veip()
 *	Set xlt_rule and xlt_action structure for egress.
 */
void ppe_drv_vlan_egress_rule_action_set_veip(fal_vlan_trans_adv_rule_t *xlt_rule,
		fal_vlan_trans_adv_action_t *xlt_action, struct ppe_drv_vlan_xlate_info *info)
{
	/*
	 * Fields for egress match.
	 * Accept tagged/untagged/priority tagged svlan and cvlan.
	 */
	if (info->svid != FAL_VLAN_INVALID) {
		xlt_rule->s_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED
				| FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	} else {
		xlt_rule->s_tagged = FAL_PORT_VLAN_XLT_MATCH_UNTAGGED;
	}

	if (info->cvid != FAL_VLAN_INVALID) {
		xlt_rule->c_tagged = (FAL_PORT_VLAN_XLT_MATCH_UNTAGGED | FAL_PORT_VLAN_XLT_MATCH_TAGGED
				| FAL_PORT_VLAN_XLT_MATCH_PRIO_TAG);
	} else {
		xlt_rule->c_tagged = FAL_PORT_VLAN_XLT_MATCH_UNTAGGED;
	}
	xlt_rule->dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule->mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);

	/*
	 * Fields for egress action.
	 */
	xlt_action->cvid_xlt_cmd = (info->cvid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action->cvid_xlt = (info->cvid == 0xFFFF) ? 0 : info->cvid;
	xlt_action->svid_xlt_cmd = (info->svid == 0xFFFF) ? 0 : FAL_VID_XLT_CMD_ADDORREPLACE;
	xlt_action->svid_xlt = (info->svid == 0xFFFF) ? 0 : info->svid;

	ppe_drv_info("Egress VLAN: Rule[s_tagged=0x%x c_tagged=0x%x] "
			"Action[svid_cmd=%d svid=%u cvid_cmd=%d cvid=%u]\n", xlt_rule->s_tagged, xlt_rule->c_tagged,
			xlt_action->svid_xlt_cmd, xlt_action->svid_xlt, xlt_action->cvid_xlt_cmd, xlt_action->cvid_xlt);

}

/*
 * ppe_drv_vlan_as_veip_add_xlate_rules()
 *	Add Ingress and Egress VLAN translation rules for VLAN created as VEIP.
 */
ppe_drv_ret_t ppe_drv_vlan_as_veip_add_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info)
{
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_port_t fal_port;
	sw_error_t ret;

	spin_lock_bh(&p->lock);

	/*
	 * Setting VLAN rule and action fields for ingress rules.
	 */
	ppe_drv_vlan_ingress_rule_action_set_veip(&xlt_rule, &xlt_action, info);

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(info->port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, info->port_id)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, info->port_id);

	/*
	 * Add ingress vlan translation rule.
	 * For adding ingress rule we are using base physical port number
	 * becasue packet rx happen on gmac, so we need to add a ingress
	 * rule/action to match tagged packet, which untag the packet and
	 * give it to src_info port, which is VP.
	 */
	ret = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule,
			&xlt_action);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to update ingress translation rule for port: 0x%x, error: %d\n"
				, iface, fal_port, ret);
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_add);
		return PPE_DRV_RET_INGRESS_VLAN_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	ppe_drv_vlan_egress_rule_action_set_veip(&xlt_rule, &xlt_action, info);

	/*
	 * Add egress vlan translation rule.
	 */
	ret = fal_port_vlan_trans_adv_add(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_EGRESS, &xlt_rule,
			&xlt_action);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to update egress translation rule for port: %d, error: %d\n",
				iface, fal_port, ret);
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_egress_vlan_add);

		/*
		 * Delete ingress vlan translation rule
		 */
		if (ret != SW_ALREADY_EXIST) {
			memset(&xlt_rule, 0, sizeof(xlt_rule));
			memset(&xlt_action, 0, sizeof(xlt_action));
			ppe_drv_vlan_ingress_rule_action_set_veip(&xlt_rule, &xlt_action, info);
			fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS,
					&xlt_rule, &xlt_action);

			/*
			 * Resetting HW index state to free.
			 */
			vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

		}

		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_EGRESS_VLAN_FAIL;
	}

	vlan->eg_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_USED;

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_as_veip_add_xlate_rules);

/*
 * ppe_drv_vlan_as_veip_del_xlate_rules()
 *	Delete Ingress and Egress VLAN translation rules for VLAN created as VEIP.
 */
ppe_drv_ret_t ppe_drv_vlan_as_veip_del_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info)
{
	fal_vlan_trans_adv_rule_t xlt_rule = {0};
	fal_vlan_trans_adv_action_t xlt_action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_tbl *vlan = p->vlan;
	fal_port_t fal_port;
	sw_error_t ret;

	spin_lock_bh(&p->lock);

	/*
	 * Setting VLAN rule and action fields for ingress rules.
	 */
	ppe_drv_vlan_ingress_rule_action_set_veip(&xlt_rule, &xlt_action, info);

	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(info->port_id) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, info->port_id)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, info->port_id);

	/*
	 * Delete ingress vlan translation rule..
	 */
	ret = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_INGRESS, &xlt_rule,
			&xlt_action);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to delete ingress translation rule for port: 0x%x, error: %d\n"
				, iface, fal_port, ret);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_INGRESS_VLAN_FAIL;
	}

	vlan->in_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;

	memset(&xlt_rule, 0, sizeof(xlt_rule));
	memset(&xlt_action, 0, sizeof(xlt_action));

	ppe_drv_vlan_egress_rule_action_set_veip(&xlt_rule, &xlt_action, info);

	/*
	 * Delete egress vlan translation rule.
	 */
	ret = fal_port_vlan_trans_adv_del(PPE_DRV_SWITCH_ID, fal_port, FAL_PORT_VLAN_EGRESS, &xlt_rule,
			&xlt_action);
	if (ret != SW_OK) {
		ppe_drv_warn("%px: Failed to update egress translation rule for port: %d, error: %d\n",
				iface, fal_port, ret);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_EGRESS_VLAN_FAIL;
	}

	vlan->eg_vlan_tbl[xlt_rule.index].hw_id_state = PPE_DRV_VLAN_HW_ID_FREE;
	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_as_veip_del_xlate_rules);

/*
 * ppe_drv_vlan_hgu_rule_create
 *      Create the VLAN rule in PPE for HGU use case.
 */
ppe_drv_ret_t ppe_drv_vlan_hgu_rule_create(struct ppe_drv_vlan_cfg *info, struct ppe_drv_vlan_ctx *ctx)
{
	fal_vlan_trans_adv_rule_t xlt_rule_in = {0};
	fal_vlan_trans_adv_action_t xlt_action_in = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_vlan_rule_match *rule = &info->rule_f;
	struct ppe_drv_vlan_action *action = &info->action_f;
	struct net_device *base_dev;
	struct vlan_dev_priv *dev_priv;
	fal_port_t fal_port;
	int32_t port_info, vsi_idx = 0;
	sw_error_t ret;
	int16_t hw_id = -1;

	/*
	 * Handle VLAN (eg: eth0.10, eth0.10.20, lan1.10)
	 * Post these checks, base_dev will point to base net device (eth0/lan1) for VLAN dev,
	 * or NULL for non-VLAN interface (eg: eth0/lan1).
	 */
	dev_priv = (info->src_dev && is_vlan_dev(info->src_dev) ? vlan_dev_priv(info->src_dev): NULL);
	base_dev = dev_priv ? dev_priv->real_dev: NULL;
	if (base_dev && is_vlan_dev(base_dev)) {
		base_dev = vlan_dev_priv(base_dev)->real_dev;
	}

	port_info = ppe_drv_port_num_from_dev(base_dev);
	if (port_info == PPE_DRV_INVALID_PORT) {
		ppe_drv_warn("Invalid Port ID: %d\n", port_info);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}
	fal_port = PPE_DRV_VIRTUAL_PORT_CHK(port_info) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port_info)
		: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port_info);

	xlt_rule_in.port_bitmap = (1ULL << fal_port);

	spin_lock_bh(&p->lock);

	hw_id = ppe_drv_vlan_hw_id_get(PPE_DRV_RULE_INGRESS);
	if (hw_id < 0) {
		ppe_drv_warn("No available hw_id in VLAN table for hgu rule\n");
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_VLAN_RULE_HW_ID_NOT_FOUND;
	}

	/*
	 * Getting vsi_idx information.
	 */
	vsi_idx = ppe_drv_iface_vsi_idx_get(ctx->iface);
	if (vsi_idx < 0) {
		ppe_drv_warn("Invalid VSI index: %d\n", vsi_idx);
		ppe_drv_vlan_hw_id_return(hw_id, PPE_DRV_RULE_INGRESS);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_VSI_NOT_FOUND;
	}

	if (info->rule_dir == PPE_DRV_RULE_EGRESS) {
		xlt_rule_in.s_vid = rule->svid;
		xlt_rule_in.c_vid = rule->cvid;
		xlt_rule_in.s_vid_enable = A_TRUE;
		xlt_rule_in.c_vid_enable = A_TRUE;
	} else {
		xlt_rule_in.s_vid = action->svidxlate;
		xlt_rule_in.c_vid = action->cvidxlate;
		xlt_rule_in.s_vid_enable = (action->svidxlate == 0) ? A_FALSE : A_TRUE;
		xlt_rule_in.c_vid_enable = (action->cvidxlate == 0) ? A_FALSE : A_TRUE;
	}

	/*
	 * Fields for match
	 */
	xlt_rule_in.s_tagged = (rule->svid == 0xFFFF) ? 0x1 : 0x7;
	xlt_rule_in.c_tagged = (rule->cvid == 0xFFFF) ? 0x1 : 0x7;
	xlt_rule_in.dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
	xlt_rule_in.mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);

	/*
	 * Fields for action
	 */
	xlt_action_in.vsi_xlt = vsi_idx;
	xlt_action_in.vsi_xlt_enable = A_TRUE;

	ret = fal_port_vlan_trans_adv_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_INGRESS, hw_id, &xlt_rule_in, &xlt_action_in);
	if (ret != SW_OK) {
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_ingress_vlan_add);
		ppe_drv_warn("Failed to update ingress vlan translation of port %d, error: %d\n", fal_port, ret);
		ppe_drv_vlan_hw_id_return(hw_id, PPE_DRV_RULE_INGRESS);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_INGRESS_VLAN_FAIL;
	}

	ppe_drv_info("INGRESS_VLAN rule programmed successfully for HGU case.\n");

	ctx->is_veip_rule_valid = A_TRUE;
	ctx->veip_rule_entry_index = hw_id;

	/*
	 * Setting VEIP flag to ensure that this rule is
	 * not pushed with all VLAN rules with src_info as VLAN as VEIP.
	 */
	if (ctx->iface) {
		ppe_drv_veip_flag_set(ctx->iface);
	}

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_hgu_rule_create);

/*
 * ppe_drv_vlan_hgu_rule_destroy
 *      Destroy the VLAN rule in PPE for HGU used case.
 */
ppe_drv_ret_t ppe_drv_vlan_hgu_rule_destroy(struct ppe_drv_vlan_ctx *ctx)
{
	fal_vlan_trans_adv_rule_t xlt_rule_in = {0};
	fal_vlan_trans_adv_action_t xlt_action_in = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t ret;

	spin_lock_bh(&p->lock);

	ret = fal_port_vlan_trans_adv_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_INGRESS, ctx->veip_rule_entry_index, &xlt_rule_in, &xlt_action_in);
	if (ret != SW_OK) {
		ppe_drv_warn("Failed to delete old ingress vlan translation rule with error: %d\n", ret);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_VLAN_INGRESS_DEL_FAIL;
	}

	ppe_drv_vlan_hw_id_return(ctx->veip_rule_entry_index, PPE_DRV_RULE_INGRESS);

	/*
	 * Clearing the flag to ensure that IN_VLAN rule is deleted
	 * which got pushed with src_info as VEIP iface.
	 */
	if (ctx->iface) {
		ppe_drv_veip_flag_clear(ctx->iface);
	}

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_hgu_rule_destroy);
#endif


#ifdef NSS_PPE_DSA_QINQ_SUPPORT
/*
 * ppe_drv_vlan_dsa_cfg_set()
 *      Map VID to VP for ingress and egress; program global private TPID on first DSA interface.
 */
ppe_drv_ret_t ppe_drv_vlan_dsa_cfg_set(uint32_t port_id, uint32_t tpid, uint32_t vid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = &p->port[port_id];
	fal_pvtvlan_rx_cfg_t vlan_port_ctrl_cfg;
	fal_pvtvlan_tx_cfg_t vlan_tx_cfg;
	fal_pvtvlan_map_t vlan_port_map_cfg;
	fal_pvtvlan_map_t vlan_port_map_clr;
	uint32_t phy_port;
	sw_error_t ret;

	spin_lock_bh(&p->lock);

	/*
	 * Global TPID is switch-level state shared across all DSA interfaces.
	 * Program it only on the first DSA interface creation.
	 */
	if (p->dsa_if_count == 0) {
		phy_port = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pp->xmit_port);

		vlan_port_ctrl_cfg.map_en = A_TRUE;
		vlan_port_ctrl_cfg.tpid = tpid;

		ret = fal_private_vlan_rx_cfg_set(PPE_DRV_SWITCH_ID, phy_port, &vlan_port_ctrl_cfg);
		if (ret != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to set vlan-to-port mapping ctrl for port 0x%x, error: %d\n", port_id, ret);
			return PPE_DRV_RET_VLAN_TPID_CONFIG_FAIL;
		}

		/*
		 * Set egress private TPID (EG_GLOBAL_CTRL_PRIVATE_TAG_TPID).
		 * Must match ingress TPID so PPE adds the correct DSA tag on egress.
		 */
		vlan_tx_cfg.tpid = tpid;

		ret = fal_private_vlan_tx_cfg_set(PPE_DRV_SWITCH_ID, &vlan_tx_cfg);
		if (ret != SW_OK) {
			memset(&vlan_port_ctrl_cfg, 0, sizeof(vlan_port_ctrl_cfg));
			vlan_port_ctrl_cfg.map_en = A_FALSE;
			fal_private_vlan_rx_cfg_set(PPE_DRV_SWITCH_ID, phy_port, &vlan_port_ctrl_cfg);
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to set private vlan tx TPID for port 0x%x, error: %d\n", port_id, ret);
			return PPE_DRV_RET_VLAN_TPID_CONFIG_FAIL;
		}
	}

	/*
	 * Per-interface: map this VID to this VP for ingress and egress.
	 */
	vlan_port_map_cfg.ptmap_en = A_TRUE;
	vlan_port_map_cfg.vlan_tci = vid;
	vlan_port_map_cfg.vlan_tci_mask = 0x0FFF;
	vlan_port_map_cfg.int_port = port_id;

	ret = fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_INGRESS, &vlan_port_map_cfg);
	if (ret != SW_OK) {
		if (p->dsa_if_count == 0) {
			memset(&vlan_tx_cfg, 0, sizeof(vlan_tx_cfg));
			fal_private_vlan_tx_cfg_set(PPE_DRV_SWITCH_ID, &vlan_tx_cfg);
			memset(&vlan_port_ctrl_cfg, 0, sizeof(vlan_port_ctrl_cfg));
			vlan_port_ctrl_cfg.map_en = A_FALSE;
			fal_private_vlan_rx_cfg_set(PPE_DRV_SWITCH_ID,
					FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pp->xmit_port), &vlan_port_ctrl_cfg);
		}
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to set vlan-to-port ingress mapping for port 0x%x, error: %d\n", port_id, ret);
		return PPE_DRV_RET_INGRESS_VLAN_MAP_FAIL;
	}

	ret = fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_EGRESS, &vlan_port_map_cfg);
	if (ret != SW_OK) {
		memset(&vlan_port_map_clr, 0, sizeof(vlan_port_map_clr));
		vlan_port_map_clr.ptmap_en = A_FALSE;
		vlan_port_map_clr.vlan_tci = vid;
		vlan_port_map_clr.vlan_tci_mask = 0x0FFF;
		vlan_port_map_clr.int_port = port_id;
		fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_INGRESS, &vlan_port_map_clr);
		if (p->dsa_if_count == 0) {
			memset(&vlan_tx_cfg, 0, sizeof(vlan_tx_cfg));
			fal_private_vlan_tx_cfg_set(PPE_DRV_SWITCH_ID, &vlan_tx_cfg);
			memset(&vlan_port_ctrl_cfg, 0, sizeof(vlan_port_ctrl_cfg));
			vlan_port_ctrl_cfg.map_en = A_FALSE;
			fal_private_vlan_rx_cfg_set(PPE_DRV_SWITCH_ID,
					FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pp->xmit_port), &vlan_port_ctrl_cfg);
		}
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to set vlan-to-port egress mapping for port 0x%x, error: %d\n", port_id, ret);
		return PPE_DRV_RET_EGRESS_VLAN_MAP_FAIL;
	}

	p->dsa_if_count++;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("DSA QinQ: configured VLAN port mapping for port 0x%x, VID %d, TPID 0x%x\n",
			port_id, vid, tpid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_dsa_cfg_set);

/*
 * ppe_drv_vlan_dsa_cfg_reset()
 *      Clear VID-to-VP mapping for this interface; clear global private TPID on last DSA interface removal.
 */
int ppe_drv_vlan_dsa_cfg_reset(uint32_t port_id, uint32_t vid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = &p->port[port_id];
	fal_pvtvlan_rx_cfg_t vlan_port_ctrl_cfg;
	fal_pvtvlan_tx_cfg_t vlan_tx_cfg;
	fal_pvtvlan_map_t vlan_port_map_cfg;
	sw_error_t ret;

	spin_lock_bh(&p->lock);

	if (p->dsa_if_count == 0) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("dsa_if_count underflow for port 0x%x\n", port_id);
		return -EINVAL;
	}

	/*
	 * Per-interface: clear ingress and egress VID-to-VP mapping.
	 */
	memset(&vlan_port_map_cfg, 0, sizeof(vlan_port_map_cfg));
	vlan_port_map_cfg.ptmap_en = A_FALSE;
	vlan_port_map_cfg.vlan_tci = vid;
	vlan_port_map_cfg.vlan_tci_mask = 0x0FFF;
	vlan_port_map_cfg.int_port = port_id;

	ret = fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_INGRESS, &vlan_port_map_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to clear vlan-to-port ingress mapping for port 0x%x, error: %d\n", port_id, ret);
		return -1;
	}

	ret = fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_EGRESS, &vlan_port_map_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to clear vlan-to-port egress mapping for port 0x%x, error: %d\n", port_id, ret);
		return -1;
	}

	/*
	 * Global TPID config: clear only when the last DSA interface is removed.
	 */
	if (p->dsa_if_count == 1) {
		memset(&vlan_port_ctrl_cfg, 0, sizeof(vlan_port_ctrl_cfg));
		vlan_port_ctrl_cfg.map_en = A_FALSE;

		ret = fal_private_vlan_rx_cfg_set(PPE_DRV_SWITCH_ID,
				FAL_PORT_ID(FAL_PORT_TYPE_PPORT, pp->xmit_port), &vlan_port_ctrl_cfg);
		if (ret != SW_OK) {
			ppe_drv_warn("Failed to clear vlan-to-port mapping ctrl for port 0x%x, error: %d\n", port_id, ret);
		}

		memset(&vlan_tx_cfg, 0, sizeof(vlan_tx_cfg));

		ret = fal_private_vlan_tx_cfg_set(PPE_DRV_SWITCH_ID, &vlan_tx_cfg);
		if (ret != SW_OK) {
			ppe_drv_warn("Failed to clear private vlan tx TPID for port 0x%x, error: %d\n", port_id, ret);
		}
	}

	p->dsa_if_count -= 1;
	spin_unlock_bh(&p->lock);

	return 0;
}
EXPORT_SYMBOL(ppe_drv_vlan_dsa_cfg_reset);

/*
 * ppe_drv_vlan_dsa_vp_egress_map_set()
 *      Set egress private VLAN mapping (VP -> VID) for a VLAN-over-DSA VP.
 */
ppe_drv_ret_t ppe_drv_vlan_dsa_vp_egress_map_set(uint32_t port_id, uint32_t vid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_pvtvlan_map_t vlan_port_map_cfg;
	sw_error_t ret;

	memset(&vlan_port_map_cfg, 0, sizeof(vlan_port_map_cfg));
	vlan_port_map_cfg.ptmap_en = A_TRUE;
	vlan_port_map_cfg.vlan_tci = vid;
	vlan_port_map_cfg.vlan_tci_mask = 0x0FFF;
	vlan_port_map_cfg.int_port = port_id;

	spin_lock_bh(&p->lock);
	ret = fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_EGRESS, &vlan_port_map_cfg);
	spin_unlock_bh(&p->lock);

	if (ret != SW_OK) {
		ppe_drv_warn("Failed to set egress VLAN-to-VP mapping for port %u vid %u, error: %d\n",
				port_id, vid, ret);
		return PPE_DRV_RET_EGRESS_VLAN_MAP_FAIL;
	}

	ppe_drv_info("DSA VLAN VP: set egress mapping VP %u -> VID port %u\n", port_id, vid);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_vlan_dsa_vp_egress_map_set);

/*
 * ppe_drv_vlan_dsa_vp_egress_map_reset()
 *      Clear egress private VLAN mapping for a VLAN-over-DSA VP.
 */
int ppe_drv_vlan_dsa_vp_egress_map_reset(uint32_t port_id, uint32_t vid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_pvtvlan_map_t vlan_port_map_cfg;
	sw_error_t ret;

	memset(&vlan_port_map_cfg, 0, sizeof(vlan_port_map_cfg));
	vlan_port_map_cfg.ptmap_en = A_FALSE;
	vlan_port_map_cfg.vlan_tci = vid;
	vlan_port_map_cfg.vlan_tci_mask = 0x0FFF;
	vlan_port_map_cfg.int_port = port_id;

	spin_lock_bh(&p->lock);
	ret = fal_private_vlan_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_EGRESS, &vlan_port_map_cfg);
	spin_unlock_bh(&p->lock);

	if (ret != SW_OK) {
		ppe_drv_warn("Failed to clear egress VLAN-to-VP mapping for port %u vid %u, error: %d\n",
				port_id, vid, ret);
		return -1;
	}

	ppe_drv_info("DSA VLAN VP: cleared egress mapping VID %u -> VP port %u\n", vid, port_id);
	return 0;
}
EXPORT_SYMBOL(ppe_drv_vlan_dsa_vp_egress_map_reset);
#endif /* NSS_PPE_DSA_QINQ_SUPPORT */
