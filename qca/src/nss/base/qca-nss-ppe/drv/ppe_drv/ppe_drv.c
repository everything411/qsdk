/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/if.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <fal/fal_rss_hash.h>
#include <fal/fal_ip.h>
#include <fal/fal_init.h>
#include <fal/fal_qm.h>
#include <fal/fal_servcode.h>
#include <fal/fal_fdb.h>
#include <fal/fal_portvlan.h>
#include <fal/fal_mirror.h>
#ifdef NSS_PPE_PON_SUPPORT
#include <fal/fal_pon.h>
#endif
#include <fal/fal_qos.h>
#include <fal/fal_flow.h>
#ifdef PPE_DRV_PKT_PADDING_STRIP
#include <fal/fal_pktedit.h>
#endif
#include "ppe_drv.h"
#include "tun/ppe_drv_tun.h"
#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
#include <linux/if_vlan.h>
#include <linux/if_arp.h>
#endif
#include <net/dsa.h>
#include <nss_debug.h>

#define PPE_DRV_STATIC_DBG_LEVEL_STR_LEN 8
#define PPE_DRV_UPSTREAM_DEV_LEVEL_STR_LEN 32
#define PPE_DRV_SRC2UNI_LEVEL_STR_LEN 128
#define NSS_PPE_DRV_WHITESPACE		" \t\v\f\n,"
#ifdef PPE_DRV_PKT_PADDING_STRIP
#define PPE_DRV_PACKET_PADDING_STR_LEN 128
#endif

#define PPE_DRV_RFS_COREMASK_MIN	1
#define PPE_DRV_RFS_COREMASK_MAX	((1 << NR_CPUS) - 1)

#ifdef NSS_PPE_DRV_HW_GRO
#define PPE_DRV_GRO_COREMASK_MIN	1
#define PPE_DRV_GRO_COREMASK_MAX	((1 << NR_CPUS) - 1)
#endif

/*
 * Module parameter to enable/disable 2-tuple RSS hash for IP fragments.
 */
static bool ipfrag_2tuple_hash = true;
module_param(ipfrag_2tuple_hash, bool, 0644);
MODULE_PARM_DESC(ipfrag_2tuple_hash, "RSS hash for IP fragments based on SIP & DIP");

/*
 * Module parameter to enable/disable Flush & Deacceleration.
 */
bool flow_deacclr_dis = false;
module_param(flow_deacclr_dis, bool, 0644);
MODULE_PARM_DESC(flow_deacclr_dis, "Disable Flow deacceleration & Flush on Exception");

/*
 * Module parameter to enable/disable passive VP creation for SFE flows.
 */
static bool passive_vp_enable = false;
MODULE_PARM_DESC(passive_vp_enable, "Passive VP creation enable/disable");

/*
 * Module parameter to set eth coremask.
 */
static unsigned int eth_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
MODULE_PARM_DESC(eth_coremask, "Coremask for Ethernet to Ethernet Flows");

#ifdef NSS_PPE_DRV_HW_GRO
/*
 * Module parameter to set eth coremask.
 */
static unsigned int eth_gro_coremask = PPE_DRV_GRO_COREMASK_DEFAULT;
MODULE_PARM_DESC(eth_gro_coremask, "Coremask for Ethernet GRO Flows");
#endif

/*
 * Module parameter to set wlan common coremask.
 */
static unsigned int wlan_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
MODULE_PARM_DESC(wlan_coremask, "Coremask for Ethernet to WLAN Flows");

/*
 * Module parameter to set coremask for tunnel, etc.
 */
static unsigned int tun_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
MODULE_PARM_DESC(tun_coremask, "Coremask for Ethernet to Tunnel, etc Flows");

/*
 * Module parameter to set coremask for multicast RFS flows.
 */
static unsigned int mcast_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
MODULE_PARM_DESC(mcast_coremask, "Coremask for Multicast RFS Flows");

#ifdef NSS_PPE_SIXTH_TUPLE
/*
 * Module parameter to set 6th tuple mode.
 */
static int sixth_tuple_mode = PPE_DRV_SIXTH_TUPLE_MODE_DISABLE;
module_param(sixth_tuple_mode, int, 0644);
MODULE_PARM_DESC(sixth_tuple_mode, "Sixth tuple mode");
#endif

uint32_t if_bm_to_offload;
#ifdef NSS_PPE_PON_SUPPORT
uint32_t gem_port_bitmap;
int dot1p_dest_vp_en = 1;

/*
 * Module parameter to set max number of PQ needed for PON flows.
 */
int pon_max_pq = 110;
module_param(pon_max_pq, int, 0644);
MODULE_PARM_DESC(pon_max_pq, "Max number of PON PQs needed by OMCI");

/*
 * Module parameter to enable PON mode.
 */
static int pon_enable = 1;

static int ppe_drv_pon_enable_set_handler(const char *val, const struct kernel_param *kp)
{
	int res = param_set_int(val, kp);

	if (res)
		return res;
#ifdef NSS_PPE_FEATURE_DOT1P
	/*
	 * ppe_drv_gbl is NULL when this handler is invoked during parse_args()
	 * at module load time, before probe() has run. Skip HW programming here;
	 * ppe_drv_probe() applies pon_enable at the correct time.
	 */
	if (!ppe_drv_gbl)
		return 0;

	if (pon_enable) {
		res = ppe_drv_dot1p_configure_default_rule();
		if (res != PPE_DRV_RET_SUCCESS) {
			ppe_drv_warn("Failed to configure dot1p default rule on pon_enable\n");
			return res;
		}
		ppe_drv_trace("dot1p default rule configured\n");
	} else {
		res = ppe_drv_dot1p_reset_default_rule();
		if (res != PPE_DRV_RET_SUCCESS) {
			ppe_drv_warn("Failed to reset dot1p default rule on pon_enable\n");
			return res;
		}
		ppe_drv_trace("dot1p default rule reset\n");
	}
#endif
	return res;
}

static const struct kernel_param_ops pon_enable_ops = {
	.set = ppe_drv_pon_enable_set_handler,
	.get = param_get_int,
};

module_param_cb(pon_enable, &pon_enable_ops, &pon_enable, 0644);
MODULE_PARM_DESC(pon_enable, "Enable PON mode and configure default dot1p rule");
#endif

#ifdef PPE_DRV_DDRQ_ENABLE
/*
 * Module parameter to control setting of DDRQ LP SC for mapped DDRQ disabled scenarios
 */
bool ddrq_lp_sc_set_en = true;
module_param(ddrq_lp_sc_set_en, bool, 0644);
MODULE_PARM_DESC(ddrq_lp_sc_set_en, "Knob to configure DDRQ LP SC for DDRQ disabled scenarios");
#endif

int disable_port_mtu_check = true;
uint32_t static_dbg_level = 0;
static char static_dbg_level_str[PPE_DRV_STATIC_DBG_LEVEL_STR_LEN];
uint8_t ppe_drv_redir_prio_map[PPE_DRV_MAX_PRIORITY] = {0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7};
#ifdef NSS_PPE_DRV_HW_GRO
uint8_t ppe_drv_gro_prio_map[PPE_DRV_MAX_PRIORITY] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif
uint8_t ppe_drv_16_prio_map[PPE_DRV_MAX_PRIORITY] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static int eth2eth_offload_if_bitmap;
static char upstream_dev_str[PPE_DRV_UPSTREAM_DEV_LEVEL_STR_LEN];
static char src2uni_map[PPE_DRV_SRC2UNI_LEVEL_STR_LEN];
#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
static char lpbk_buffer_map_str[PPE_DRV_LPBK_BUFFER_MAP_STR_LEN];

/*
 * Default 0: automatic DDR buffering disabled
 */
static int ddr_buffer_auto_en;
#endif
#ifdef PPE_DRV_PKT_PADDING_STRIP
static char packet_padding[PPE_DRV_PACKET_PADDING_STR_LEN];
#endif

int l4_checksum_exception_enable = false;
int mac_lrn_exception_en = true;
uint32_t ppe_drv_jhash_rnd __read_mostly;
int wlan_host_assist_qdisc_en = 0;
int unknown_dst_rfs_enable = 0;

#ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
int ppe_drv_ipsec_passth_en = 0;
#endif

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
static uint32_t loopback_port_ft_type;
#endif

#ifdef PPE_DRV_DDRQ_ENABLE
uint32_t ppe_drv_ddrq_en_bitmask[PPE_DRV_DDRQ_MAX_CNT_WORD];
#endif

/*
 * Define the filename to be used for assertions.
 */
struct ppe_drv *ppe_drv_gbl = NULL;

/*
 * ppe_drv_get_vxlan_dport()
 *	Get the VXLAN destination port.
 */
int ppe_drv_get_vxlan_dport(void)
{
	return ppe_drv_gbl->vxlan_dport;
}

#ifdef NSS_PPE_SIXTH_TUPLE
/*
 * ppe_drv_get_sixth_tuple_mode()
 *	Get the sixth tuple mode.
 */
int ppe_drv_get_sixth_tuple_mode(void)
{
	return sixth_tuple_mode;
}

/*
 * ppe_drv_vsi_flow_key_enable()
 *	Enable the L2/L3 vsi as a key globally.
 *  this is needed when using vsi VPN_ID as sixth tuple.
 */
void ppe_drv_vsi_flow_key_enable(void)
{
	sw_error_t ret;
	fal_flow_global_cfg_t cfg = {0};

	ret = fal_flow_global_cfg_get(PPE_DRV_SWITCH_ID, &cfg);
	if (ret != SW_OK) {
		ppe_drv_warn("Failed to fetch the global setting");
		return;
	}

	cfg.l2_vpn_en = 1;
	cfg.l3_vpn_en = 1;
	ret = fal_flow_global_cfg_set(PPE_DRV_SWITCH_ID, &cfg);

	if (ret != SW_OK) {
		ppe_drv_warn("Error in enabling l2/l3 vsi as key");
	} else {
		ppe_drv_trace("Success in enabling l2/l3 vsi as key");
	}
	return;
}

/*
 * ppe_drv_vsi_flow_key_disble()
 *	disable the L2/L3 vsi as a key globally.
 *  this is needed when using vsi VPN_ID as sixth tuple.
 */
void ppe_drv_vsi_flow_key_disable(void)
{
	sw_error_t ret;
	fal_flow_global_cfg_t cfg = {0};
	ret = fal_flow_global_cfg_get(PPE_DRV_SWITCH_ID, &cfg);
	if (ret != SW_OK) {
		ppe_drv_warn("Failed to fetch the global setting");
		return;
	}

	cfg.l2_vpn_en = 0;
	cfg.l3_vpn_en = 0;
	ret = fal_flow_global_cfg_set(PPE_DRV_SWITCH_ID, &cfg);
	if (ret != SW_OK) {
		ppe_drv_warn("Error in disabling l2/l3 vsi as key");
	} else {
		ppe_drv_trace("Success in disabling l2/l3 vsi as key");
	}

	return;
}

/*
 * ppe_drv_vlan_flow_key_disable()
 *	disable vlan key on all active interfaces.
 */
void ppe_drv_vlan_flow_key_disable(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
        struct ppe_drv_iface *iface;
	int i;
	sw_error_t err = SW_OK;

	for (i = 0; i < p->iface_num ; i++) {
		iface = &p->iface[i];
		if ((iface->flags & PPE_DRV_IFACE_FLAG_VALID) != PPE_DRV_IFACE_FLAG_VALID
				|| !iface->port) {
			continue;
		}

		fal_ip_vlan_as_flow_key_t flow_key = {0};
		flow_key.valid = A_FALSE;
		flow_key.mode = FAL_IP_VLAN_AS_FLOW_KEY_DISABLED;
		err = fal_ip_port_vlan_as_flow_key_set(PPE_DRV_SWITCH_ID, iface->port->port, &flow_key);
		if (err != SW_OK) {
			ppe_drv_warn("failed to clear val key setting port:%d", iface->port->port);
			return;
		}
	}
	ppe_drv_trace("Success in disabling vlan flow key");
}

/*
 * ppe_drv_vlan_flow_key_enable()
 *	disable vlan key on all active interfaces.
 */
void ppe_drv_vlan_flow_key_enable(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
        struct ppe_drv_iface *iface;
	int i;
	sw_error_t err = SW_OK;

	for (i = 0; i < p->iface_num ; i++) {
		iface = &p->iface[i];
		if ((iface->flags & PPE_DRV_IFACE_FLAG_VALID) != PPE_DRV_IFACE_FLAG_VALID
				|| !iface->port) {
			continue;
		}

		fal_ip_vlan_as_flow_key_t flow_key = {0};
		flow_key.valid = A_TRUE;
		flow_key.mode = FAL_IP_VLAN_AS_FLOW_KEY_FROM_OUTER;
		err = fal_ip_port_vlan_as_flow_key_set(PPE_DRV_SWITCH_ID, iface->port->port, &flow_key);
		if (err != SW_OK) {
			ppe_drv_warn("failed to clear val key setting port:%d", iface->port->port);
			return;
		}
	}
	ppe_drv_trace("Success in enabling vlan flow key");
}

/*
 * ppe_drv_sixth_tuple_mode_handler()
 *	change the sixth mode.
 */
static int ppe_drv_sixth_tuple_mode_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret, cur_sixth_tuple_mode;

	cur_sixth_tuple_mode = sixth_tuple_mode;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write) {
		return ret;
	}

	if (cur_sixth_tuple_mode == sixth_tuple_mode) {
		return 0;
	}

	/*
	 * Validate the input value (should be 0/1/2)
	 */
	if (sixth_tuple_mode != PPE_DRV_SIXTH_TUPLE_MODE_DISABLE
			&& sixth_tuple_mode != PPE_DRV_SIXTH_TUPLE_MODE_VSI
			&& sixth_tuple_mode != PPE_DRV_SIXTH_TUPLE_MODE_VLAN) {
		ppe_drv_warn("Invalid value for sixth_tuple_mode. Must be 0/1/2\n");
		sixth_tuple_mode = cur_sixth_tuple_mode;
		return -EINVAL;
	}

	/*
	 * Clear the old seting of 6th-tuple
	 */
	if (cur_sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VSI) {
		ppe_drv_vsi_flow_key_disable();
	}

	if (cur_sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VLAN) {
		ppe_drv_vlan_flow_key_disable();
	}

	/*
	 * set new seting of 6th-tuple
	 */
	if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VSI) {
		ppe_drv_vsi_flow_key_enable();
	}

	if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VLAN) {
		ppe_drv_vlan_flow_key_enable();
	}
	return 0;
}
#endif

/*
 * ppe_drv_get_vxlan_gpe_dport()
 *	Get the VXLAN-GPE destination port.
 */
int ppe_drv_get_vxlan_gpe_dport(void)
{
	return ppe_drv_gbl->vxlan_gpe_dport;
}

#ifdef PPE_DRV_DDRQ_ENABLE
/*
 * ppe_drv_set_ddrq_en_bitmask()
 *	API to set the PPE global DDRQ enable bitmask
 */
int ppe_drv_set_ddrq_en_bitmask(uint32_t q_base, uint32_t q_cnt, bool state)
{
	uint32_t word_idx, bit_off, bit_pending, bit_cnt, i, mask;

	if ((q_base + q_cnt) > PPE_DRV_DDRQ_MAX_CNT) {
		ppe_drv_warn("qbase: %d, q_cnt: %d outside the DDRQ range\n", q_base, q_cnt);
		return -1;
	}

	i = q_base;
	bit_pending = q_cnt;

	while (bit_pending) {
		word_idx = i / PPE_DRV_BITS_IN_WORD;
		bit_off = i % PPE_DRV_BITS_IN_WORD;

		bit_cnt = PPE_DRV_BITS_IN_WORD - bit_off;
		if (bit_cnt > bit_pending) {
			bit_cnt = bit_pending;
		}
		mask = ((((uint64_t)1 << bit_cnt) - 1u) << bit_off);

		if (state) {
			ppe_drv_ddrq_en_bitmask[word_idx] |= mask;
		} else {
			ppe_drv_ddrq_en_bitmask[word_idx] &= ~mask;
		}

		i += bit_cnt;
		bit_pending -= bit_cnt;
	}

	for (i = 0 ; i < PPE_DRV_DDRQ_MAX_CNT_WORD; i++) {
		ppe_drv_info("updated DDRQN enable bitmask word:%d value is 0x%0x\n",
					i, ppe_drv_ddrq_en_bitmask[i]);
	}

	return 0;
}
#else
int ppe_drv_set_ddrq_en_bitmask(uint32_t q_base, uint32_t q_cnt, bool state)
{
	ppe_drv_warn("DDRQ feature not enabled in the PPE module\n");
	return 0;
}
#endif
EXPORT_SYMBOL(ppe_drv_set_ddrq_en_bitmask);

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
/*
 * ppe_drv_get_tpid_index()
 *      Get PPE VLAN TPID indexes by TPID values.
 */
int ppe_drv_get_tpid_index(uint16_t tpid, const char *type)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int index = -1;

	spin_lock_bh(&p->lock);

	if (strcmp(type, "ctpid") == 0) {
		if (tpid == p->gbl_ctpid)
			index = PPE_DRV_VLAN_CTPID_IDX;
		if (tpid == p->gbl_ctpid_ext)
			index = PPE_DRV_VLAN_CTPID_EXT_IDX;
	} else if (strcmp(type, "stpid") == 0) {
		if (tpid == p->gbl_stpid)
			index = PPE_DRV_VLAN_STPID_IDX;
		if (tpid == p->gbl_stpid_ext)
			index = PPE_DRV_VLAN_STPID_EXT_IDX;
	}

	spin_unlock_bh(&p->lock);
	return index;
}
EXPORT_SYMBOL(ppe_drv_get_tpid_index);

/*
 * ppe_drv_get_vsi_num()
 *	Get VSI number.
 */
int ppe_drv_get_vsi_num(void)
{
	return ppe_drv_gbl->vsi_num;
}
EXPORT_SYMBOL(ppe_drv_get_vsi_num);
#endif

/*
 * ppe_drv_is_mht_dev()
 *	API to get MHT switch interface flag
 */
bool ppe_drv_is_mht_dev(struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface = NULL;

	spin_lock_bh(&p->lock);
	iface = ppe_drv_iface_get_by_dev_internal(dev);
	if (!iface) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: No valid PPE interface for dev", dev);
		return false;
	}

	/*
	 * Get the MHT switch flag on the interface.
	 */
	if (!(iface->flags & PPE_DRV_IFACE_FLAG_MHT_SWITCH_VALID)) {
		spin_unlock_bh(&p->lock);
		return false;
	}
	spin_unlock_bh(&p->lock);
	return true;
}
EXPORT_SYMBOL(ppe_drv_is_mht_dev);

/*
 * ppe_drv_is_veip_dev()
 *	Check if the given net device is a VEIP net device.
 */
bool ppe_drv_is_veip_dev(struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface;
	bool is_veip;

	spin_lock_bh(&p->lock);
	iface = ppe_drv_iface_get_by_dev_internal(dev);
	if (!iface) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: No valid PPE interface for dev", dev);
		return false;
	}

	is_veip = (iface->type == PPE_DRV_IFACE_TYPE_VEIP);

	spin_unlock_bh(&p->lock);
	return is_veip;
}
EXPORT_SYMBOL(ppe_drv_is_veip_dev);

/*
 * ppe_drv_isram_queue_profile_init()
 *	API to enable ISRAM queue for all the PPE ports
 */
bool ppe_drv_isram_queue_profile_init(uint8_t isram_qbase)
{
	int i;
	sw_error_t err;
	struct ppe_drv *p = ppe_drv_gbl;
	 struct ppe_drv_port *pp_local;
	fal_ucast_queue_dest_t queue_dst = {0};

	queue_dst.sram_queue_type = FAL_ISRAM_QUEUE;
	queue_dst.src_profile = PPE_DRV_PORT_SRC_PROFILE;

	/*
	 * Run the loop for PPE ports
	 * Currently assigning the same ISQ base for all the PPE ports.
	 */
	spin_lock_bh(&p->lock);
	for (i = 1; i < PPE_DRV_PORTS_MAX ; i++) {
		pp_local = &p->port[i];
		queue_dst.dst_port = pp_local->port;
		err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &queue_dst, isram_qbase, PPE_DRV_REDIR_PROFILE_ID);
		if (err != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%p: Unable to set queue base for %d port\n", p, queue_dst.dst_port);
			return false;
		}
	}
	spin_unlock_bh(&p->lock);

	return true;
}
EXPORT_SYMBOL(ppe_drv_isram_queue_profile_init);

/*
 * ppe_drv_hw_stats_sync()
 *	Sync PPE HW stats
 */
static void ppe_drv_hw_stats_sync(struct timer_list *tm)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_v4_conn *cn_v4;
	struct ppe_drv_v6_conn *cn_v6;
#ifdef PPE_TUNNEL_ENABLE
	struct ppe_drv_v4_conn *cn_tun_v4;
	struct ppe_drv_v6_conn *cn_tun_v6;
#endif

	struct ppe_drv_v4_conn_flow *pcf_v4;
	struct ppe_drv_v4_conn_flow *pcr_v4;
	struct ppe_drv_v6_conn_flow *pcf_v6;
	struct ppe_drv_v6_conn_flow *pcr_v6;
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	struct ppe_drv_v6_conn_flow *pcf_hp;
	struct ppe_drv_v6_conn_flow *pcr_hp;
#endif
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
        struct ppe_drv_pm_counter_ctx *pm_ctx;
#endif
	uint16_t id;

	/*
	 * Update hw stats for flow associated with active v4 connections
	 */
	spin_lock_bh(&p->lock);
	if (!list_empty(&p->conn_v4)) {
		list_for_each_entry(cn_v4, &p->conn_v4, list) {
			pcf_v4 = &cn_v4->pcf;
			pcr_v4 = &cn_v4->pcr;

			if (!pcf_v4->no_stats_update) {
				ppe_drv_flow_v4_stats_update(pcf_v4);
			}
			if (pcr_v4 && !pcr_v4->no_stats_update) {
				ppe_drv_flow_v4_stats_update(pcr_v4);
			}
		}
	}

	/*
	 * Update hw stats for flow associated with active v6 connections
	 */
	if (!list_empty(&p->conn_v6)) {
		list_for_each_entry(cn_v6, &p->conn_v6, list) {
			pcf_v6 = &cn_v6->pcf;
			pcr_v6 = &cn_v6->pcr;

			if (!pcf_v6->no_stats_update) {
				ppe_drv_flow_v6_stats_update(pcf_v6);
			}
			if (pcr_v6 && !pcr_v6->no_stats_update) {
				ppe_drv_flow_v6_stats_update(pcr_v6);
			}
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
			if (cn_v6->is_hairpin_nat) {
				pcf_hp = &cn_v6->pcf_hp;
				pcr_hp = &cn_v6->pcr_hp;

				ppe_drv_flow_v6_stats_update(pcf_hp);
				if (pcr_hp) {
					ppe_drv_flow_v6_stats_update(pcr_hp);
				}
			}
#endif
		}
	}

#ifdef PPE_TUNNEL_ENABLE
	/*
	 * Update hw stats for tunnels associated with active v4 connections
	 */

	if (!list_empty(&p->conn_tun_v4)) {
		list_for_each_entry(cn_tun_v4, &p->conn_tun_v4, list) {
			ppe_drv_tun_v4_port_stats_update(cn_tun_v4);
		}
	}

	/*
	 * Update hw stats for tunnels associated with active v6 connections
	 */
	if (!list_empty(&p->conn_tun_v6)) {
		list_for_each_entry(cn_tun_v6, &p->conn_tun_v6, list) {
			/*
			 * Mapt outer rule for tunnel stats must not be updated as
			 * it would be mapped to the innner v4 flow stats
			 */
			if (ppe_drv_v6_conn_flags_check(cn_tun_v6, PPE_DRV_V6_CONN_FLAG_TYPE_MAPT)) {
				continue;
			}

			ppe_drv_tun_v6_port_stats_update(cn_tun_v6);
		}
	}
#endif
	for (id = 0; id < PPE_DRV_ACL_LIST_ID_MAX; id++) {
		if (p->acl->list_id[id].list_id_state == PPE_DRV_ACL_LIST_ID_USED) {
			ppe_drv_acl_stats_update(p->acl->list_id[id].ctx);
		}
	}

	/*
	 * Update h/w stats counter if policer is in use.
	 */
	for (id = 0; id < PPE_DRV_PORT_POLICER_MAX; id++) {
		if (p->pol_ctx->port_pol[id].in_use) {
			ppe_drv_port_policer_stats_update(&p->pol_ctx->port_pol[id]);
		}
	}

	/*
	 * Update h/w stats counter if acl-policer is in use.
	 */
	for (id = 0; id < PPE_DRV_ACL_POLICER_MAX; id++) {
		if (p->pol_ctx->acl_pol[id].is_configured) {
			ppe_drv_acl_policer_stats_update(&p->pol_ctx->acl_pol[id]);
		}
	}
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	/*
	 * Update the PM counter stats.
	 */
	for (id = 0; id < PPE_DRV_PM_COUNTER_ID_MAX; id++) {
		pm_ctx = &p->pm->in_ctx_array[id];
		if (pm_ctx->state == PPE_DRV_PM_COUNTER_ID_USED) {
			ppe_drv_pm_counter_stats_update(pm_ctx);
		}

		pm_ctx = &p->pm->eg_ctx_array[id];
		if (pm_ctx->state == PPE_DRV_PM_COUNTER_ID_USED) {
			ppe_drv_pm_counter_stats_update(pm_ctx);
		}
	}
#endif

#ifdef NSS_PPE_PON_SUPPORT
	ppe_drv_dp_pon_stats_update(p);
#endif

	spin_unlock_bh(&p->lock);

	/*
	 * Re arm the hardware stats timer
	 */
	mod_timer(&p->hw_flow_stats_timer, jiffies + p->hw_flow_stats_ticks);
}

/*
 * ppe_drv_nsm_queue_stats_update()
 *	Get queue stats from fal to nsm.
 */
bool ppe_drv_nsm_queue_stats_update(struct ppe_drv_nsm_stats *nsm_stats, uint32_t queue_id, uint8_t item_id)
{
	fal_queue_stats_t queue_info;
	sw_error_t err;

	if (item_id >= FAL_QM_DROP_ITEMS) {
		ppe_drv_warn("invalid drop item id: %u", item_id);
		return false;
	}

	err = fal_queue_counter_get(PPE_DRV_SWITCH_ID, queue_id, &queue_info);
	if (err != SW_OK) {
		ppe_drv_warn("failed to get stats for queue id: %u", queue_id);
		return false;
	}

	nsm_stats->queue_stats.drop_packets = queue_info.drop_packets[item_id];
	nsm_stats->queue_stats.drop_bytes = queue_info.drop_bytes[item_id];
	ppe_drv_trace("queue id: %u, item id: %u, drop_packet = %llu, drop_bytes = %llu", queue_id, item_id, nsm_stats->queue_stats.drop_packets, nsm_stats->queue_stats.drop_bytes);

	return true;
}
EXPORT_SYMBOL(ppe_drv_nsm_queue_stats_update);

/*
 * ppe_drv_nsm_sawf_sc_stats_read()
 *	Export service-class stats to nsm.
 */
bool ppe_drv_nsm_sawf_sc_stats_read(struct ppe_drv_nsm_stats *nsm_stats, uint8_t service_class)
{
	struct ppe_drv_stats_sawf_sc *sawf_sc_stats;
	struct ppe_drv *p = ppe_drv_gbl;

	if (!PPE_DRV_SERVICE_CLASS_IS_VALID(service_class)) {
		ppe_drv_warn("%u Invalid SAWF service class", service_class);
		return false;
	}

	spin_lock_bh(&p->lock);

	sawf_sc_stats = &ppe_drv_gbl->stats.sawf_sc_stats[service_class];

	nsm_stats->sawf_sc_stats.rx_packets = atomic64_read(&sawf_sc_stats->rx_packets);
	nsm_stats->sawf_sc_stats.rx_bytes = atomic64_read(&sawf_sc_stats->rx_bytes);
	nsm_stats->sawf_sc_stats.flow_count = atomic64_read(&sawf_sc_stats->flow_count);

	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Stats Updated: service class %u : packets %llu : bytes %llu : flows %u\n",
			service_class, nsm_stats->sawf_sc_stats.rx_packets, nsm_stats->sawf_sc_stats.rx_bytes, nsm_stats->sawf_sc_stats.flow_count);
	return true;
}
EXPORT_SYMBOL(ppe_drv_nsm_sawf_sc_stats_read);

/*
 * ppe_drv_hash_init()
 *	Initialize the PPE hash registers
 */
static bool ppe_drv_hash_init(void)
{
	fal_rss_hash_mode_t mode = { 0 };
	fal_rss_hash_config_t config = { 0 };

#ifdef PPE_DRV_RPS_HASH_SELECT
	fal_rss_hash_algm_t rsshash_algm = { 0 };
	rsshash_algm.hash_algm = FAL_RSS_LEGACY_HASH;

	/*
	 * TODO: Add support to select legacy hash and toeplitz hash
	 */
	if ((fal_rsshash_algm_set(PPE_DRV_SWITCH_ID, &rsshash_algm)) != SW_OK) {
		ppe_drv_warn("Failed to set hash algorithm \n");
		return false;
	}
#endif /* !PPE_DRV_RPS_HASH_SELECT */

	mode = FAL_RSS_HASH_IPV4ONLY;
	config.hash_mask = PPE_DRV_HASH_MASK;
	config.hash_fragment_mode = ipfrag_2tuple_hash;
	config.hash_seed = PPE_DRV_HASH_SEED_DEFAULT;
	config.hash_sip_mix[0] = PPE_DRV_HASH_MIX_V4_SIP;
	config.hash_dip_mix[0] = PPE_DRV_HASH_MIX_V4_DIP;
	config.hash_protocol_mix = PPE_DRV_HASH_MIX_V4_PROTO;
	config.hash_sport_mix = PPE_DRV_HASH_MIX_V4_SPORT;
	config.hash_dport_mix = PPE_DRV_HASH_MIX_V4_DPORT;

	config.hash_fin_inner[0] = (PPE_DRV_HASH_FIN_INNER_OUTER_0 & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_outer[0] = ((PPE_DRV_HASH_FIN_INNER_OUTER_0 >> 5) & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_inner[1] = (PPE_DRV_HASH_FIN_INNER_OUTER_1 & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_outer[1] = ((PPE_DRV_HASH_FIN_INNER_OUTER_1 >> 5) & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_inner[2] = (PPE_DRV_HASH_FIN_INNER_OUTER_2 & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_outer[2] = ((PPE_DRV_HASH_FIN_INNER_OUTER_2 >> 5) & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_inner[3] = (PPE_DRV_HASH_FIN_INNER_OUTER_3 & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_outer[3] = ((PPE_DRV_HASH_FIN_INNER_OUTER_3 >> 5) & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_inner[4] = (PPE_DRV_HASH_FIN_INNER_OUTER_4 & PPE_DRV_HASH_FIN_MASK);
	config.hash_fin_outer[4] = ((PPE_DRV_HASH_FIN_INNER_OUTER_4 >> 5) & PPE_DRV_HASH_FIN_MASK);

	if (fal_rss_hash_config_set(PPE_DRV_SWITCH_ID, mode, &config) != SW_OK) {
		ppe_drv_warn("IPv4 hash register initialization failed\n");
		return false;
	}

	mode = FAL_RSS_HASH_IPV6ONLY;
	config.hash_sip_mix[0] = PPE_DRV_HASH_SIPV6_MIX_0;
	config.hash_dip_mix[0] = PPE_DRV_HASH_DIPV6_MIX_0;
	config.hash_sip_mix[1] = PPE_DRV_HASH_SIPV6_MIX_1;
	config.hash_dip_mix[1] = PPE_DRV_HASH_DIPV6_MIX_1;
	config.hash_sip_mix[2] = PPE_DRV_HASH_SIPV6_MIX_2;
	config.hash_dip_mix[2] = PPE_DRV_HASH_DIPV6_MIX_2;
	config.hash_sip_mix[3] = PPE_DRV_HASH_SIPV6_MIX_3;
	config.hash_dip_mix[3] = PPE_DRV_HASH_DIPV6_MIX_3;

	if (fal_rss_hash_config_set(PPE_DRV_SWITCH_ID, mode, &config) != SW_OK) {
		ppe_drv_warn("IPv6 hash register initialization failed\n");
		return false;
	}

	return true;
}

/*
 * ppe_drv phy_port_base_queue_init()
 *	Initialize PPE port 0 - 7 base queue
 */
static bool ppe_drv_phy_port_base_queue_init(struct ppe_drv *p)
{
	fal_ucast_queue_dest_t q_dst = {0};
	uint32_t queue_id = 0;
	sw_error_t err;
	uint8_t profile = 0;
	int i = 0;

	for (i = 0; i < PPE_DRV_PHYSICAL_MAX; i++) {
		q_dst.src_profile = 0;
		q_dst.dst_port = i;

		err = fal_ucast_queue_base_profile_get(PPE_DRV_SWITCH_ID, &q_dst, &queue_id, &profile);
		if (err != SW_OK) {
			ppe_drv_warn("%p unable to get port queue base ID: %d", p, err);
			return false;
		}

		ppe_drv_port_ucast_queue_update(&p->port[i], (uint8_t)queue_id);
	}

	return true;
}

#ifdef PPE_DRV_PKT_PADDING_STRIP
/*
 * ppe_drv_pkt_edit_init()
 *      Initialize the PPE packet editor registers
 */
static bool ppe_drv_pkt_edit_init(void)
{
	fal_pktedit_padding_t padding = { 0 };

	/*
	 * Default config for removing padidng from packet
	 */
	padding.strip_padding_route_en = 1;
	padding.strip_padding_en = 1;
	padding.strip_padding_bridge_en = 0;
	padding.strip_tunnel_inner_padding_en = 0;

	if (fal_pktedit_padding_set(PPE_DRV_SWITCH_ID, &padding) != SW_OK) {
		ppe_drv_warn("Failed to update strip_padding_en config \n");
		return false;
	}

	return true;
}
#endif

/*
 * ppe_drv_fse_feature_enable()
 *	Enable PPE FSE feature
 */
void ppe_drv_fse_feature_enable()
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->fse_enable = true;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_fse_feature_enable);

/*
 * ppe_drv_fse_feature_disable()
 *	Disable PPE FSE feature
 */
void ppe_drv_fse_feature_disable()
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->fse_enable = false;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_fse_feature_disable);

/*
 * ppe_drv_loopback_base_queue()
 */
#if defined(PPE_LOOPBACK_RING_SUPPORT)
void ppe_drv_loopback_base_queue(uint8_t queue_id, uint32_t ft_type)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);

	if (ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_GRETAP_MAPT) {
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING_GRETAP_MAPT, queue_id,
						PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING_MAPT_GRETAP, queue_id,
						PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
	}

#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	if (ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_V6_HAIRPIN_NAT) {
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NPT66_HAIRPIN_NAT, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
	}
#endif

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	if (ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_DDR_BUFFER) {
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
	}
#endif
	p->loopback_ring_info.base_queue = queue_id;
	p->loopback_ring_info.ft_type = ft_type;
	p->loopback_ring_info.enabled = true;

	spin_unlock_bh(&p->lock);

	ppe_drv_info("loopback ring enabled queue_id %u feature type %x\n", queue_id, p->loopback_ring_info.ft_type);
}
EXPORT_SYMBOL(ppe_drv_loopback_base_queue);
#endif

#ifdef NSS_PPE_L2_VP_SC_ENQ_BYPASS
/*
 * ppe_drv_loopback_sc2queue_mapping()
 *	Loopback to queue mapping
 */
static bool ppe_drv_loopback_sc2queue_mapping(struct ppe_drv *p, uint8_t src_profile)
{
#if defined(PPE_LOOPBACK_RING_SUPPORT)
	int base_queue = p->loopback_ring_info.base_queue;
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING, base_queue, src_profile, PPE_DRV_REDIR_PROFILE_ID);
	return true;
#else
	return false;
#endif
}

#else
/*
 * ppe_drv_loopback_sc2queue_mapping()
 *	Loopback to queue mapping
 */
static bool ppe_drv_loopback_sc2queue_mapping(struct ppe_drv *p, uint8_t src_profile)
{
	int base_queue = p->loopback_ring_info.base_queue;
	struct net_device *upstream_dev;
	struct ppe_drv_port *pp = NULL;
	int next_sc_queue = -1;

	upstream_dev = p->upstream_dev;
	pp = ppe_drv_port_from_dev(upstream_dev);
	if (!pp) {
		ppe_drv_warn("%p: invalid ppe port for given upstream dev: %s", p, upstream_dev->name);
		return false;
	}

	next_sc_queue = ppe_drv_port_ucast_queue_get_by_port(pp->port);
	if (next_sc_queue == -1) {
                ppe_drv_warn("%p: ucast queue get failed for pp %d", p, pp->port);
		return false;
        }

	/*
	 * Map loopback ring service code to queue mapping
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING_NEXT, next_sc_queue, src_profile, PPE_DRV_REDIR_PROFILE_ID);
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING, base_queue, src_profile, PPE_DRV_REDIR_PROFILE_ID);
	return true;
}
#endif

/*
 * ppe_drv_queue_from_core()
 *	Get base queue for a specific core.
 */
int16_t ppe_drv_queue_from_core(uint8_t core)
{
	struct ppe_drv *p = ppe_drv_gbl;

	if (core >= NR_CPUS) {
		ppe_drv_warn("%p: invalid core-id: %d", p, core);
		return -1;
	}

	return p->core2queue[core];
}
EXPORT_SYMBOL(ppe_drv_queue_from_core);

/*
 * ppe_drv_enq_vp_queue_reset()
 *	Reset the queue ID of given enqueue vport in PPE.
 */
static bool ppe_drv_enq_vp_queue_reset(struct ppe_drv *p,
					uint32_t enq_vport)
{
        sw_error_t err;
	fal_ucast_queue_dest_t q_dst = {0};

	q_dst.src_profile = PPE_DRV_PORT_SRC_PROFILE;
	q_dst.dst_port = enq_vport;
	err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &q_dst, PPE_DRV_ENQ_VP_QID_NONE, PPE_DRV_REDIR_PROFILE_ID);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Unable to reset PPE queue for enqueue vp :%d", p, enq_vport);
		return false;
	}

	return true;
}

/*
 * ppe_drv_enq_vp_queue_set()
 *	Set the queue ID of given enqueue vport in PPE.
 */
static bool ppe_drv_enq_vp_queue_set(struct ppe_drv *p,
					int16_t enq_vport,
					a_uint32_t queue_id, int profile_id)
{
        sw_error_t err;
	fal_ucast_queue_dest_t q_dst = {0};
	int src_profile = -1;
	struct ppe_drv_port *pp = NULL;

	q_dst.dst_port = enq_vport;

	/*
	 * Enqueue VP need to be mapped to a queue for all the source profile. This is specifically required
	 * for PON setups where interfaces can be put to a different source profile. Queue base is dependent
	 * on source profile and hence queue base need to be updated for all source profile.
	 */
	for (src_profile = 0; src_profile < PPE_DRV_PORT_SRC_PROFILE_MAX; src_profile++) {
		q_dst.src_profile = src_profile;
		err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &q_dst, queue_id, profile_id);
		if (err != SW_OK) {
			ppe_drv_warn("%p: Unable to map enqueue vp with queue:%d for src_profile:%d", p, queue_id, src_profile);
			return false;
		}
	}

	/*
	 * Update drv port structure with the correct ucast queue
	 * mapped to the enqueue vp port for easy access elsewhere.
	 */
	pp = &p->port[enq_vport];
	pp->ucast_queue = queue_id;

	return true;
}

/*
 * ppe_drv_enq_vp_map_to_queue()
 *	Enqueue VP to queue mapping.
 */
ppe_drv_ret_t ppe_drv_enq_vp_map_to_queue(uint8_t queue_id, int8_t enq_vp, int profile_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_enqueue_cfg_t enqueue_cfg = {0};
        sw_error_t ret;
	int8_t pri_profile;

	pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
	if (pri_profile == PPE_DRV_PORT_ENQ_VP_PRI_PRFL_INVALID) {
		ppe_drv_warn("%p: Unable to get pri profile for enqueue vport:%d ", p, enq_vp);
		return PPE_DRV_RET_ENQ_VP_TO_PRI_PROF_FAIL;
	}

	/*
	 * Set queue_id for a given port on PPE.
	 */
	if (!ppe_drv_enq_vp_queue_set(p, enq_vp, queue_id, profile_id)) {
		ppe_drv_warn("%p: Enqueue vp queue init failed for qid:%d", p, queue_id);
		return PPE_DRV_RET_ENQ_VP_QID_SET_FAIL;
	}

	/*
	 * Configure the allocated enqueue vp number on PORT_VSI_ENQUEUE table.
	 *
	 * NOTE: The flow_pri_profile is set directly to pri_profile without adding
	 * PPE_DRV_PORT_ENQVP_VSI_TBL_START_IDX offset. This change was made as part
	 * of HW GRO support to align with hardware requirements for both RFS and GRO
	 * enqueue VP configurations.
	 */
	enqueue_cfg.rule_entry.enqueue_type = FAL_ENQUEUE_FLOW;
	enqueue_cfg.rule_entry.flow_pri_profile = pri_profile;
	enqueue_cfg.index_entry.enqueue_en = (a_bool_t)PPE_DRV_PORT_EVP_ENABLE;
	enqueue_cfg.index_entry.enqueue_vport = enq_vp;
	ret = fal_qm_enqueue_config_set(PPE_DRV_SWITCH_ID, &enqueue_cfg);
	if (ret != SW_OK) {
		ppe_drv_warn("%p: Unable to set the enqueue vp config", p);
		return PPE_DRV_RET_ENQ_VP_EN_FAIL;
	}

	ppe_drv_trace("%p: Enqueue vp node to queue map done qid:%d enq_vp:%d pri_prof:%d", p, queue_id, enq_vp, enqueue_cfg.rule_entry.flow_pri_profile);
	return PPE_DRV_RET_SUCCESS;
}

/*
 * ppe_drv_ds_map_free()
 *	Provides unmapping of node with enqueue vp and queue
 */
ppe_drv_ret_t ppe_drv_ds_map_free(uint8_t node_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_enqueue_cfg_t enqueue_cfg = {0};
        sw_error_t ret;
	int8_t pri_profile;
	int16_t enq_vp;

	spin_lock_bh(&p->lock);
	enq_vp = ppe_drv_port_metadata_to_enq_vp_internal(node_id);
	if (enq_vp == PPE_DRV_PORT_ID_INVALID) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to get enqueue vport for node_id:%d ", p, node_id);
		return PPE_DRV_RET_METADATA_TO_ENQ_VP_FAIL;
	}

	pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
	if (pri_profile == PPE_DRV_PORT_ENQ_VP_PRI_PRFL_INVALID) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to get pri profile for enqueue vport:%d ", p, enq_vp);
		return PPE_DRV_RET_ENQ_VP_TO_PRI_PROF_FAIL;
	}

	/*
	 * Reset queue_id for a given port on PPE.
	 */
	if (!ppe_drv_enq_vp_queue_reset(p, enq_vp)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Enqueue vp queue reset failed for node_id:%d", p, node_id);
		return PPE_DRV_RET_ENQ_VP_QID_RESET_FAIL;
	}

	/*
	 * Disable enqueue vp bit on PORT_VSI_ENQUEUE table.
	 */
	enqueue_cfg.rule_entry.enqueue_type = FAL_ENQUEUE_FLOW;
	enqueue_cfg.rule_entry.flow_pri_profile = pri_profile;
	enqueue_cfg.index_entry.enqueue_en = (a_bool_t)PPE_DRV_PORT_EVP_DISABLE;
	enqueue_cfg.index_entry.enqueue_vport = enq_vp;
	ret = fal_qm_enqueue_config_set(PPE_DRV_SWITCH_ID, &enqueue_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to reset enq_vp:%d disable on PORT VSI", p, enq_vp);
		return PPE_DRV_RET_ENQ_VP_DISABLE_FAIL;
	}

	ppe_drv_port_enq_vp_free(enq_vp);
	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_ds_map_free);

/*
 * ppe_drv_ds_map_node_to_queue()
 *	node to queue mapping
 *
 * This API will be invoked by PPE-DS module to provide node to queue mapping.
 */
ppe_drv_ret_t ppe_drv_ds_map_node_to_queue(uint8_t node_id, uint8_t queue_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t enq_vp;
	ppe_drv_ret_t status;

	spin_lock_bh(&p->lock);
	enq_vp = ppe_drv_port_enq_vp_alloc();
	if (enq_vp == PPE_DRV_PORT_ID_INVALID) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to get the enqueue vport ", p);
		return PPE_DRV_RET_ENQ_VP_ALLOC_FAIL;
	}

	p->port[enq_vp].evp.type = PPE_DRV_ENQ_VP_DEFAULT;

	/*
	 * Map enqueue VP to specific queue
	 */
	status = ppe_drv_enq_vp_map_to_queue(queue_id, enq_vp, PPE_DRV_REDIR_PROFILE_ID);
	if (status != PPE_DRV_RET_SUCCESS) {
		ppe_drv_port_enq_vp_free(enq_vp);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to map enq_vp:%u to queue:%u ", p, enq_vp, queue_id);
		return status;
	}

	/*
	 * Each enqueue vp has metadata value(node_id in case of PPE-DS).
	 * Set enqueue vport metadata value.
	 * Cookie will be used as key to find the corresponding enqueue vp during PPE flow addition.
	 */
	ppe_drv_port_enq_vp_metadata_set(enq_vp, node_id);
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("%p: Enqueue vp node to queue map done qid:%d enq_vp:%d node_id:%d", p, queue_id, enq_vp, node_id);
	return status;
}
EXPORT_SYMBOL(ppe_drv_ds_map_node_to_queue);

#ifdef NSS_PPE_DRV_HW_GRO
/*
 * ppe_drv_gro_map_core_to_enqueue_vp()
 *	Core to queue mapping
 *
 * This API will be invoked at dp-init time to map each core to enq_vp
 */
static ppe_drv_ret_t ppe_drv_gro_map_core_to_enqueue_vp(uint8_t core, uint8_t queue_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t enq_vp;
	ppe_drv_ret_t status;

	/*
	 * Default coremask for the physical interface is set.
	 */
	p->gro_ctx.gro_info.coremask = eth_gro_coremask;
	p->gro_ctx.gro_info.shadow_coremask = eth_gro_coremask;

	enq_vp = ppe_drv_port_enq_vp_alloc();
	if (enq_vp == PPE_DRV_PORT_ID_INVALID) {
		ppe_drv_warn("%p: Unable to get the enqueue vport ", p);
		return PPE_DRV_RET_ENQ_VP_ALLOC_FAIL;
	}

	/*
	 * Maps enqueue with given queue
	 */
	status = ppe_drv_enq_vp_map_to_queue(queue_id, enq_vp, PPE_DRV_GRO_PROFILE_ID);
	if (status != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Unable to map enq_vp:%u to queue:%u ", p, enq_vp, queue_id);
		ppe_drv_port_enq_vp_free(enq_vp);
		return status;
	}

	ppe_drv_trace("%p: GRO core mappped to enqueue VP. qid:%d enq_vp:%d core:%d", p, queue_id, enq_vp, core);

	/*
	 * Core id will be used as key to find the corresponding enqueue vp during PPE flow addition.
	 */
	p->gro_ctx.gro_info.core2enq_vp[core] = enq_vp;
	return status;
}
#endif

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_drv_pon_map_enqueue_vp_to_pq()
 *	maps Pqs to a enqueue vp
 *
 */
ppe_drv_ret_t ppe_drv_pon_map_enqueue_vp_to_pq(struct ppe_drv_port *port)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_portscheduler_resource_t cfg = {0};
	ppe_drv_ret_t ppe_ret;
	int8_t enq_vp;
	uint8_t cnt_enq_vp;
	uint8_t queue_id;
	uint8_t max_enq_vp;
	int8_t allocated_enq_vps[PPE_DRV_PORT_ENQ_VP_MAX_NUM];
	uint8_t allocated_count = 0;
	int32_t pon_port_profile_id;

	pon_port_profile_id = ppe_drv_port_ucast_queue_profile_get(port->port);
	if (!ppe_drv_confgiure_ucast_prio_map_tbl(pon_port_profile_id, ppe_drv_16_prio_map)) {
		ppe_drv_warn("%p: failed to configure ucast priority class setting\n", p);
		return PPE_DRV_RET_UCAST_PRIO_TBL_MAP_FAIL;
	}

	if (fal_port_scheduler_resource_get(0, port->port, &cfg) != 0) {
		ppe_drv_warn("%px:port resources info failed for port:%u", p, port->port);
		return PPE_DRV_RET_FAILURE_NO_RESOURCE;
	}

	p->ppe_drv_pon_port_start_pq = cfg.ucastq_start;
	p->ppe_drv_pon_port_max_pq = (pon_max_pq <= cfg.ucastq_num) ? pon_max_pq : cfg.ucastq_num;

	max_enq_vp = (p->ppe_drv_pon_port_max_pq + PPE_DRV_PORT_PON_PQ_PER_ENQ_VP - 1) /
								PPE_DRV_PORT_PON_PQ_PER_ENQ_VP;

	for (cnt_enq_vp = 0; cnt_enq_vp < max_enq_vp; cnt_enq_vp++) {
		enq_vp = ppe_drv_port_enq_vp_alloc();
		if (enq_vp == PPE_DRV_PORT_ID_INVALID) {
			ppe_drv_warn("%p: Unable to get the enqueue vport ", p);
			ppe_ret = PPE_DRV_RET_ENQ_VP_ALLOC_FAIL;
			goto cleanup_allocated_enq_vps;
		}
		p->port[enq_vp].evp.type = PPE_DRV_ENQ_VP_PON;

		/*
		 * Set queue_id for a given port on PPE.
		 */
		queue_id = p->ppe_drv_pon_port_start_pq + (PPE_DRV_PORT_PON_PQ_PER_ENQ_VP * cnt_enq_vp);
		if (queue_id >= (p->ppe_drv_pon_port_start_pq + p->ppe_drv_pon_port_max_pq)) {
			ppe_drv_port_enq_vp_free(enq_vp);
			ppe_drv_warn("%p: Calculated queue_id %d exceeds available range", p, queue_id);
			ppe_ret = PPE_DRV_RET_ENQ_VP_QID_SET_FAIL;
			goto cleanup_allocated_enq_vps;
		}

		if (!ppe_drv_port_ucast_queue_profile_set(&p->port[enq_vp], PPE_DRV_PORT_SRC_PROFILE,
						queue_id, pon_port_profile_id)) {
			ppe_drv_port_enq_vp_free(enq_vp);
			ppe_drv_warn("%p: Enqueue vp queue init failed for qid:%d", p, queue_id);
			ppe_ret = PPE_DRV_RET_ENQ_VP_QID_SET_FAIL;
			goto cleanup_allocated_enq_vps;
		}

		/*
		 * Map enqueue VP to specific queue
		 */
		ppe_ret = ppe_drv_enq_vp_map_to_queue(queue_id, enq_vp, pon_port_profile_id);
		if (ppe_ret != PPE_DRV_RET_SUCCESS) {
			ppe_drv_port_enq_vp_free(enq_vp);
			ppe_drv_warn("%p: Unable to map enq_vp:%u to queue:%u ", p, enq_vp, queue_id);
			goto cleanup_allocated_enq_vps;
		}
		allocated_enq_vps[allocated_count++] = enq_vp;
		ppe_drv_trace("%p: Enqueue vp node to priority queue map done qid:%d enq_vp:%d",
					p, queue_id, enq_vp);
	}

	return PPE_DRV_RET_SUCCESS;

cleanup_allocated_enq_vps:
	/*
	 * Cleanup all successfully allocated Enq VPs
	 */
	for (uint8_t i = 0; i < allocated_count; i++) {
		ppe_drv_port_enq_vp_free(allocated_enq_vps[i]);
	}
	return ppe_ret;
}

/*
 * ppe_drv_pon_get_pq_config()
 *	map pq to enqueue vp and int_pri.
 *
 */
bool ppe_drv_pon_get_pq_config(uint8_t pq, uint8_t *enq_vp, uint8_t *int_pri)
{
	int internal_pq, q_offset;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = NULL;
	uint16_t i;
	fal_ucast_queue_dest_t q_dst = {0};
	uint32_t queue_id = 0;
	sw_error_t err;
	uint8_t profile = 0;
	int enqueue_vp;

	spin_lock_bh(&p->lock);
	if (pq >= p->ppe_drv_pon_port_max_pq) {
		ppe_drv_warn("pq %d is more than the max supported priority queue %d", pq, p->ppe_drv_pon_port_max_pq);
		spin_unlock_bh(&p->lock);
		return false;
	}

	internal_pq = p->ppe_drv_pon_port_start_pq + pq;
	q_offset = pq % PPE_DRV_PORT_PON_PQ_PER_ENQ_VP;
	*int_pri = q_offset;

	for (i = PPE_DRV_PORT_ENQ_VP_START; i <=PPE_DRV_PORT_ENQ_VP_END; i++) {
		pp = &p->port[i];

		if (!kref_read(&pp->ref_cnt)) {
			continue;
		}

		if (pp->evp.type == PPE_DRV_ENQ_VP_PON) {
			enqueue_vp = pp->port;
			q_dst.src_profile = 0;
			q_dst.dst_port = enqueue_vp;

			err = fal_ucast_queue_base_profile_get(PPE_DRV_SWITCH_ID, &q_dst, &queue_id, &profile);
			if (err != SW_OK) {
				spin_unlock_bh(&p->lock);
				ppe_drv_warn("unable to get base queue id for enq_vp port %d", enqueue_vp);
				return false;
			}

			if ((internal_pq >= queue_id) && (internal_pq < (queue_id + PPE_DRV_PORT_PON_PQ_PER_ENQ_VP))) {
				*enq_vp = enqueue_vp;
				spin_unlock_bh(&p->lock);
				ppe_drv_info("enq_vp =%d int_pri =%d\n", *enq_vp, *int_pri);
				return true;
			}
		}
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_info("No matching enqueue VP found for pq %d", pq);
	return false;
}
EXPORT_SYMBOL(ppe_drv_pon_get_pq_config);

/*
 * ppe_drv_pon_get_enq_vp_mapped_to_base_pq()
 * 	get enqueue_vp mapped to the passed base queue
 */
static int ppe_drv_pon_get_enq_vp_mapped_to_base_pq(uint8_t internal_pq)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = NULL;
	uint16_t i;
	fal_ucast_queue_dest_t q_dst = {0};
	uint32_t queue_id = 0;
	sw_error_t err;
	uint8_t profile = 0;
	int enqueue_vp = PPE_DRV_PORT_ENQ_VP_INVALID;

	for (i = PPE_DRV_PORT_ENQ_VP_START; i <=PPE_DRV_PORT_ENQ_VP_END; i++) {
		pp = &p->port[i];

		if (!kref_read(&pp->ref_cnt)) {
			continue;
		}

		if ((pp->evp.type == PPE_DRV_ENQ_VP_PON) ||
				(pp->evp.type == PPE_DRV_BASEQ_ENQ_VP_PON)) {
			enqueue_vp = pp->port;
			q_dst.src_profile = 0;
			q_dst.dst_port = enqueue_vp;

			err = fal_ucast_queue_base_profile_get(PPE_DRV_SWITCH_ID, &q_dst, &queue_id, &profile);
			if (err != SW_OK) {
				ppe_drv_warn("unable to get base queue id for enq_vp port %d", enqueue_vp);
				return PPE_DRV_PORT_ENQ_VP_INVALID;
			}

			if (internal_pq == queue_id) {
				ppe_drv_info("enq_vp =%d", enqueue_vp);
				return enqueue_vp;
			}
		}
	}
	return PPE_DRV_PORT_ENQ_VP_INVALID;
}

/*
 * ppe_drv_pon_map_enq_vp_to_base_pq()
 *	maps base Pq to a enqueue vp
 */
ppe_drv_ret_t ppe_drv_pon_map_enq_vp_to_base_pq(uint8_t base_pq, uint8_t *enq_vp)
{
	int internal_pq;
	struct ppe_drv *p = ppe_drv_gbl;
	int enqueue_vp;
	ppe_drv_ret_t status;
	struct ppe_drv_port *port;
	int32_t pon_port_profile_id;

	if (base_pq >= p->ppe_drv_pon_port_max_pq) {
		ppe_drv_warn("pq %d is more than the max supported priority queue %d", base_pq, p->ppe_drv_pon_port_max_pq);
		return PPE_DRV_RET_ENQ_VP_QID_SET_FAIL;
	}

	internal_pq = p->ppe_drv_pon_port_start_pq + base_pq;
	if (internal_pq >= (p->ppe_drv_pon_port_start_pq + p->ppe_drv_pon_port_max_pq)) {
		ppe_drv_warn("%p: Calculated queue_id %d exceeds available range", p, internal_pq);
		return PPE_DRV_RET_ENQ_VP_QID_SET_FAIL;
	}

	spin_lock_bh(&p->lock);
	enqueue_vp = ppe_drv_pon_get_enq_vp_mapped_to_base_pq(internal_pq);
	if (enqueue_vp != PPE_DRV_PORT_ENQ_VP_INVALID) {
		*enq_vp = enqueue_vp;
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_SUCCESS;
	}

	enqueue_vp = ppe_drv_port_enq_vp_alloc();
	if (enqueue_vp == PPE_DRV_PORT_ID_INVALID) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Unable to get the enqueue vport", p);
		return PPE_DRV_RET_ENQ_VP_ALLOC_FAIL;
	}
	p->port[enqueue_vp].evp.type = PPE_DRV_BASEQ_ENQ_VP_PON;

	if (!ppe_drv_port_ucast_queue_profile_set(&p->port[enqueue_vp], PPE_DRV_PORT_SRC_PROFILE,
					internal_pq, PON_PORT_ID)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_port_enq_vp_free(enqueue_vp);
		ppe_drv_warn("%p: Enqueue vp queue init failed for qid:%d", p, internal_pq);
		return PPE_DRV_RET_ENQ_VP_QID_SET_FAIL;
	}

	/*
	 * Map enqueue VP to specific queue
	 */
	port = ppe_drv_iface_port_get(p->pon_iface);
	pon_port_profile_id = ppe_drv_port_ucast_queue_profile_get(port->port);
	status = ppe_drv_enq_vp_map_to_queue(internal_pq, enqueue_vp, pon_port_profile_id);
	if (status != PPE_DRV_RET_SUCCESS) {
		spin_unlock_bh(&p->lock);
		ppe_drv_port_enq_vp_free(enqueue_vp);
		ppe_drv_warn("%p: Unable to map enq_vp:%u to queue:%u ", p, enqueue_vp, internal_pq);
		return status;
	}

	spin_unlock_bh(&p->lock);
	*enq_vp = enqueue_vp;
	ppe_drv_trace("%p: Enqueue vp node to priority queue map done qid:%d enq_vp:%d", p, internal_pq, *enq_vp);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_pon_map_enq_vp_to_base_pq);

/*
 * ppe_drv_pon_iface_get()
 *	Returns the PON interface.
 */
struct ppe_drv_iface *ppe_drv_pon_iface_get(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *pon_iface = NULL;

	spin_lock_bh(&p->lock);
	pon_iface = p->pon_iface;
	spin_unlock_bh(&p->lock);

	return pon_iface;
}
EXPORT_SYMBOL(ppe_drv_pon_iface_get);

/*
 * ppe_drv_is_pon_enable_internal()
 *      Returns true if PON is enabled on this RDP.
 */
bool ppe_drv_is_pon_enable_internal(void)
{
        struct ppe_drv *p = ppe_drv_gbl;

        return !!p->pon_iface;
}

/*
 * ppe_drv_is_pon_enable()
 *	Returns true if PON is enabled on this RDP.
 */
bool ppe_drv_is_pon_enable(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	bool is_pon_en = false;

	spin_lock_bh(&p->lock);
	is_pon_en = ppe_drv_is_pon_enable_internal();
	spin_unlock_bh(&p->lock);

	return is_pon_en;
}
EXPORT_SYMBOL(ppe_drv_is_pon_enable);

int ppe_drv_pon_port_max_pq_get()
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint32_t pon_port_max_pq = 0;

	spin_lock_bh(&p->lock);
	pon_port_max_pq = p->ppe_drv_pon_port_max_pq ? p->ppe_drv_pon_port_max_pq : pon_max_pq;
	spin_unlock_bh(&p->lock);

	return pon_port_max_pq;
}
EXPORT_SYMBOL(ppe_drv_pon_port_max_pq_get);

int ppe_drv_pon_port_start_pq_get()
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t pon_start_pq = 0;

	spin_lock_bh(&p->lock);
	pon_start_pq = p->ppe_drv_pon_port_start_pq;
	spin_unlock_bh(&p->lock);

	return pon_start_pq;
}
EXPORT_SYMBOL(ppe_drv_pon_port_start_pq_get);
#endif

/*
 * ppe_drv_rfs_map_core_to_enqueue_vp()
 *	Core to queue mapping
 *
 * This API will be invoked at dp-init time to map each core to enq_vp
 */
static ppe_drv_ret_t ppe_drv_rfs_map_core_to_enqueue_vp(uint8_t core, uint8_t queue_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t enq_vp;
	ppe_drv_ret_t status;

	/*
	 * Set passive vp enable and WLAN coremask in global structure.
	 */
	p->rfs.passive_vp_enable = passive_vp_enable;
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_WLAN] = wlan_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_WLAN] = wlan_coremask;

	/*
	 * Set UDP_ST coremask in global structure
	 */
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_UDP_ST] = eth_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_UDP_ST] = eth_coremask;

	/*
	 * Default coremask for the physical interface is set.
	 */
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_PHYSICAL] = eth_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_PHYSICAL] = eth_coremask;

	/*
	 * Default coremask for the default generic interface is set.
	 */
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_TUNNEL] = tun_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_TUNNEL] = tun_coremask;

	/*
	 * Default coremask for multicast RFS flows is set.
	 */
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_MCAST] = mcast_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_MCAST] = mcast_coremask;

	enq_vp = ppe_drv_port_enq_vp_alloc();
	if (enq_vp == PPE_DRV_PORT_ID_INVALID) {
		ppe_drv_warn("%p: Unable to get the enqueue vport ", p);
		return PPE_DRV_RET_ENQ_VP_ALLOC_FAIL;
	}

	p->port[enq_vp].evp.type = PPE_DRV_ENQ_VP_DEFAULT;

	/*
	 * Maps enqueue with given queue
	 */
	status = ppe_drv_enq_vp_map_to_queue(queue_id, enq_vp, PPE_DRV_REDIR_PROFILE_ID);
	if (status != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Unable to map enq_vp:%u to queue:%u ", p, enq_vp, queue_id);
		ppe_drv_port_enq_vp_free(enq_vp);
		return status;
	}

	ppe_drv_trace("%p: RFS core mappped to enqueue VP. qid:%d enq_vp:%d core:%d", p, queue_id, enq_vp, core);

	/*
	 * Core id will be used as key to find the corresponding enqueue vp during PPE flow addition.
	 */
	p->rfs.core2enq_vp[core] = enq_vp;
	return status;
}

#ifdef NSS_PPE_DRV_HW_GRO
/*
 * ppe_drv_gro_core2queue_mapping()
 *	Core to queue mapping
 *
 * This API will be invoked by DP driver to provide core to queue
 * mapping. This internally will be used to configure service code
 * to queue mapping for GRO feature.
 */
void ppe_drv_gro_core2queue_mapping(uint8_t core, uint8_t queue_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	ppe_drv_ret_t status;

	if (core >= NR_CPUS) {
		ppe_drv_warn("%p: invalid core-id: %d", p, core);
		return;
	}

	/*
	 * This API is called to map a core to an enqueue_vp.
	 */
	spin_lock_bh(&p->lock);
	status = ppe_drv_gro_map_core_to_enqueue_vp(core, queue_id);
	if (status != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Core failed to allocate to an enqueue vp, core %u, fail status: %u", p, core, status);
	}

	ppe_drv_trace("%d: queue mapping called for core(%d)\n", queue_id, core);

	spin_unlock_bh(&p->lock);

	return;

}
EXPORT_SYMBOL(ppe_drv_gro_core2queue_mapping);
#endif

/*
 * ppe_drv_core2queue_mapping()
 *	Core to queue mapping
 *
 * This API will be invoked by DP driver to provide core to queue
 * mapping. This internally will be used to configure service code
 * to queue mapping for PPE RFS feature.
 */
void ppe_drv_core2queue_mapping(uint8_t core, uint8_t queue_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	ppe_drv_ret_t status;

	if (core >= NR_CPUS) {
		ppe_drv_warn("%p: invalid core-id: %d", p, core);
		return;
	}

	/*
	 * This API is called to map a core to an enqueue_vp.
	 */
	spin_lock_bh(&p->lock);
	status = ppe_drv_rfs_map_core_to_enqueue_vp(core, queue_id);
	if (status != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("%p: Core failed to allocate to an enqueue vp, core %u, fail status: %u", p, core, status);
	}

	ppe_drv_trace("%d: queue mapping called for core(%d)\n", queue_id, core);

	switch(core) {
	case 0:
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NOEDIT_REDIR_CORE0, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_EDIT_REDIR_CORE0, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		break;
	case 1:
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NOEDIT_REDIR_CORE1, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_EDIT_REDIR_CORE1, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		break;
	case 2:
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NOEDIT_REDIR_CORE2, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_EDIT_REDIR_CORE2, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		break;
	case 3:
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NOEDIT_REDIR_CORE3, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_EDIT_REDIR_CORE3, queue_id, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
		break;
	default:
		ppe_drv_warn("%d Invalid core(%d)\n", queue_id, core);
		spin_unlock_bh(&p->lock);
		return;
	}

	/*
	 * Store core to queue mapping in ppe_drv structure.
	 */
	p->core2queue[core] = queue_id;
	spin_unlock_bh(&p->lock);

	return;

}
EXPORT_SYMBOL(ppe_drv_core2queue_mapping);

/*
 * ppe_drv_l3_route_ctrl_init()
 *	Initialize PPE global configuration
 */
static bool ppe_drv_l3_route_ctrl_init(struct ppe_drv *p)
{
	fal_ip_global_cfg_t cfg = { 0 };

	/*
	 * Global MTU and MRU cofiguration check; if flows MTU or MRU is not as
	 * expected by PPE then flow will be deaccelerated by PPE and packets will
	 * be redirected to CPU
	 */
	cfg.mru_fail_action = FAL_MAC_RDT_TO_CPU;
	cfg.mru_deacclr_en = flow_deacclr_dis ? A_FALSE : A_TRUE;
	cfg.mtu_fail_action = FAL_MAC_RDT_TO_CPU;
	cfg.mtu_deacclr_en = flow_deacclr_dis ? A_FALSE : A_TRUE;

	/*
	 * Don't deaccelerate flow based on DF bit.
	 */
	cfg.mtu_nonfrag_fail_action = FAL_MAC_RDT_TO_CPU;
	cfg.mtu_df_deacclr_en = A_FALSE;

	if (fal_ip_global_ctrl_set(PPE_DRV_SWITCH_ID, &cfg) != SW_OK) {
		ppe_drv_warn("%p: IP global control configuration failed\n", p);
		return false;
	}

	if (fal_ip_route_mismatch_action_set(PPE_DRV_SWITCH_ID, FAL_MAC_RDT_TO_CPU) != SW_OK) {
		ppe_drv_warn("%p: IP route mismatch action configuration failed\n", p);
		return false;
	}

	return true;
}

/*
 * ppe_drv_fse_ops_free()
 *	Function to release fse_ops related memory
 *
 * Should be called under ppe lock
 */
void ppe_drv_fse_ops_free(struct kref *kref)
{
	struct ppe_drv *p = container_of(kref, struct ppe_drv, fse_ops_ref);
	struct ppe_drv_fse_ops *ops_internal;

	if (!p->fse_ops) {
		ppe_drv_warn("%p: No FSE ops registered\n", p);
		return;
	}

	ops_internal = p->fse_ops;
	p->fse_ops = NULL;
	kfree(ops_internal);
}

/*
 * ppe_drv_fse_ops_unregister()
 *	Un-register FSE rule add/delete callbacks. This function will be called from Wi-Fi driver
 */
void ppe_drv_fse_ops_unregister(void)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	if (!p->fse_ops || !p->is_wifi_fse_up) {
		spin_unlock_bh(&p->lock);
		return;
	}

	p->is_wifi_fse_up = false;
	kref_put(&p->fse_ops_ref, ppe_drv_fse_ops_free);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_fse_ops_unregister);

/**
 * ppe_drv_fse_ops_register()
 *	Register FSE rule add/delete callbacks. This function will be called from Wi-Fi driver
 */
bool ppe_drv_fse_ops_register(struct ppe_drv_fse_ops *ops)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_fse_ops *ops_internal;

	if (!ops->create_fse_rule || !ops->destroy_fse_rule) {
		ppe_drv_warn("%p: Invalid FSE ops passed from Wi-FI driver\n", p);
		return false;
	}

	ops_internal = (struct ppe_drv_fse_ops *)kzalloc(sizeof(struct ppe_drv_fse_ops), GFP_ATOMIC);
	if (!ops_internal) {
		ppe_drv_warn("%p: FSE ops registration failed\n", p);
		return false;
	}

	spin_lock_bh(&p->lock);
	if (p->fse_ops) {
		ppe_drv_warn("%p: FSE ops registration already done\n", p);
		spin_unlock_bh(&p->lock);
		kfree(ops_internal);
		return false;
	}

	ops_internal->create_fse_rule = ops->create_fse_rule;
	ops_internal->destroy_fse_rule = ops->destroy_fse_rule;
	p->fse_ops = ops_internal;
	p->is_wifi_fse_up = true;
	kref_init(&p->fse_ops_ref);
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("%p: FSE ops registration done successfully\n", p);
	return true;
}
EXPORT_SYMBOL(ppe_drv_fse_ops_register);

/*
 * ppe_drv_get_dentry()
 *	Get PPE driver debugfs dentry
 */
struct dentry *ppe_drv_get_dentry()
{
	struct ppe_drv *p = ppe_drv_gbl;
	return p->dentry;
}
EXPORT_SYMBOL(ppe_drv_get_dentry);

static const struct of_device_id ppe_drv_dt_ids[] = {
	{ .compatible =  "qcom,nss-ppe" },
	{},
};
MODULE_DEVICE_TABLE(of, ppe_drv_dt_ids);

/*
 * ppe_drv_confgiure_ucast_prio_map_tbl
 *	Configure unicast priority map table for RFS/DS flows
 */
bool ppe_drv_confgiure_ucast_prio_map_tbl(uint8_t profile_id, uint8_t *prio_map)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t pri_class;
	uint8_t int_pri;
	sw_error_t ret;

	/*
	 * Set the priority class value for every possible priority.
	 */
	for (int_pri = 0; int_pri < PPE_DRV_MAX_PRIORITY; int_pri++) {
		pri_class = prio_map[int_pri];

		/*
		 * Configure priority class for Profile 9 used by RFS and DS.
		 */
		ret = fal_ucast_priority_class_set(PPE_DRV_SWITCH_ID, profile_id, int_pri, pri_class);
		if (ret != SW_OK) {
			ppe_drv_warn("%p Failed to configure ucast priority class for profile_id %d, int_pri: %d with err: %d\n",
					p, profile_id, int_pri, ret);
			return false;
		}

		ppe_drv_info("profile_id: %d, int_priority: %d, pri_class: %d\n", profile_id, int_pri, pri_class);
	}

	return true;
}
EXPORT_SYMBOL(ppe_drv_confgiure_ucast_prio_map_tbl);

/*
 * ppe_drv_port_mcast_priority_class_set()
 *	Set multicast priority to queue class mapping for a port.
 */
bool ppe_drv_port_mcast_priority_class_set(uint32_t port_id, uint8_t priority, uint8_t queue_class)
{
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t ret;

	if (port_id >= PPE_DRV_PHYSICAL_MAX) {
		ppe_drv_warn("%p: Invalid port ID: %u\n", p, port_id);
		return false;
	}

	if (priority >= PPE_DRV_MAX_PRIORITY) {
		ppe_drv_warn("%p: Invalid priority: %u\n", p, priority);
		return false;
	}

	if (queue_class > (PPE_DRV_MCAST_QUEUE_CLASS_MAX - 1)) {
		ppe_drv_warn("%p: Invalid queue class: %u (must be 0-%u)\n",
			     p, queue_class, PPE_DRV_MCAST_QUEUE_CLASS_MAX - 1);
		return false;
	}

	ret = fal_port_mcast_priority_class_set(PPE_DRV_SWITCH_ID, port_id, priority, queue_class);
	if (ret != SW_OK) {
		ppe_drv_warn("%p: Failed to set mcast priority class for port %u, priority %u, error: %d\n",
			     p, port_id, priority, ret);
		return false;
	}

	ppe_drv_trace("%p: Successfully set mcast priority class for port %u, priority %u to class %u\n",
		      p, port_id, priority, queue_class);
	return true;
}
EXPORT_SYMBOL(ppe_drv_port_mcast_priority_class_set);

/*
 * ppe_drv_wlan_rfs_enable_set()
 *	Sets the value of the global variable to enable.
 */
void ppe_drv_wlan_rfs_enable_set(bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	p->rfs.wlan_rfs_enable = enable;
	spin_unlock_bh(&p->lock);

	return;
}
EXPORT_SYMBOL(ppe_drv_wlan_rfs_enable_set);

/*
 * ppe_drv_cpu_port_init()
 *	Initialize the CPU port.
 */
ppe_drv_ret_t ppe_drv_cpu_port_init(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_port *port;
	uint16_t max_mtu = PPE_DRV_PORT_JUMBO_MAX;
	uint8_t status = PPE_DRV_RET_SUCCESS;

	/*
	 * Allocate and initialize the inline port
	 */
	iface = ppe_drv_iface_alloc(PPE_DRV_IFACE_TYPE_PHYSICAL, NULL);
	if (!iface) {
		ppe_drv_warn("%px: failed to allocate PPE interface for init", p);
		status = PPE_DRV_RET_IFACE_INVALID;
		goto iface_fail;
	}

	spin_lock_bh(&p->lock);
	p->rfs.cpu_iface = iface;

	port = ppe_drv_port_alloc(PPE_DRV_PORT_CPU_TYPE, NULL, false, false);
	if (!port) {
		ppe_drv_warn("%p: unable to get a valid port of type(%d), iface index: %u\n", iface, PPE_DRV_PORT_CPU_TYPE, iface->index);
		status = PPE_DRV_RET_PORT_ALLOC_FAIL;
		goto port_fail;
	}

	l3_if = ppe_drv_l3_if_alloc(PPE_DRV_L3_IF_TYPE_CPU_PORT);
	if (!l3_if) {
		ppe_drv_warn("%p: unable to get a valid l3_if of type(%d), iface index: %u\n", iface, PPE_DRV_L3_IF_TYPE_CPU_PORT, iface->index);
		status = PPE_DRV_RET_L3_IF_ALLOC_FAIL;
		goto l3_if_fail;
	}

	if (!ppe_drv_port_mtu_mru_set(port, max_mtu, max_mtu)) {
		ppe_drv_warn("%p: PORT MTU MRU failed, iface index: %u\n", iface, iface->index);
		status = PPE_DRV_RET_MTU_CFG_FAIL;
		goto fail;
	}

	if (!ppe_drv_l3_if_mtu_mru_set(l3_if, max_mtu, max_mtu)) {
		ppe_drv_warn("%p: L3_IF MTU MRU failed, iface index: %u\n", iface, iface->index);
		status = PPE_DRV_RET_MTU_CFG_FAIL;
		goto fail;
	}

	/*
	 * Attach l3_if to port
	 */
	if (!ppe_drv_port_l3_if_attach(port, l3_if)) {
		ppe_drv_warn("%p: unable to attach valid l3_if(%p) to port(%p), iface index(%u)\n", iface, l3_if, port, iface->index);
		status = PPE_DRV_RET_L3_IF_PORT_ATTACH_FAIL;
		goto fail;
	}

	port->port_l3_if = l3_if;
	ppe_drv_iface_port_set(iface, port);
	ppe_drv_iface_l3_if_set(iface, l3_if);

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;

fail:
	ppe_drv_l3_if_deref(l3_if);
l3_if_fail:
	ppe_drv_port_deref(port);
port_fail:
	spin_unlock_bh(&p->lock);
	ppe_drv_iface_deref(iface);
iface_fail:
	return status;
}

#ifdef PPE_DRV_MCAST_ENQ_CTRL
/*
 * ppe_drv_mcast_enqueue_ctrl_init()
 *	Map multicast traffic to port 0 unicast queues to enable EDMA RPS.
 *
 * On Hermosa AP variants, multicast packets (e.g. unknown DA) land on
 * multicast queue 256 by default, Mapping to port 0 unicast queues fixes this.
 * For PON variants, ppe_drv_dp_gem_enable() later overrides this to
 * pon port for T-CONT scheduling.
 */
static void ppe_drv_mcast_enqueue_ctrl_init(void)
{
	sw_error_t sw_err;

	sw_err = fal_qm_mcast_enqueue_ctrl_set(PPE_DRV_SWITCH_ID, PPE_DRV_PORT_CPU, A_TRUE);
	if (sw_err != SW_OK) {
		ppe_drv_warn("failed to set mcast enqueue ctrl to port 0, err=%d\n", sw_err);
		return;
	}

	ppe_drv_trace("mcast enqueue ctrl set to port 0 unicast queues for EDMA RPS\n");
}
#endif

/*
 * ppe_drv_probe()
 *	probe the PPE driver
 */
static int ppe_drv_probe(struct platform_device *pdev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct device_node *np;
	fal_ppe_tbl_caps_t cap;
	int i = 0;
	uint8_t status;

	np = of_node_get(pdev->dev.of_node);

	fal_ppe_capacity_get(PPE_DRV_SWITCH_ID, &cap);

	/*
	 * Fill number of table entries
	 */
	p->l3_if_num = cap.l3_if_caps;
	p->port_num = cap.port_caps;
	p->vsi_num = cap.vsi_caps;
	p->pub_ip_num = cap.pub_ip_caps;
	p->host_num = cap.host_caps;
	p->flow_num = cap.flow_caps;
	p->pppoe_session_max = cap.pppoe_session_caps;
	p->nexthop_num = cap.nexthop_caps;
	p->sc_num = cap.service_code_caps;
	p->iface_num = p->l3_if_num + p->port_num + p->pppoe_session_max;
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	p->prefix_num = cap.ipv6_prefix_caps;
	p->iid_num = cap.ipv6_iid_caps;
#endif

	/*
	 * Initialize locks
	 * Initialize locks before its first usage in "ppe_drv_tun_global_init".
	 */
	spin_lock_init(&p->lock);
	spin_lock_init(&p->stats_lock);
	spin_lock_init(&p->notifier_lock);


	if (!ppe_drv_hash_init()) {
		return -1;
	}

	if (!ppe_drv_l3_route_ctrl_init(p)) {
		return -1;
	}

#ifdef PPE_DRV_PKT_PADDING_STRIP
	if (!ppe_drv_pkt_edit_init()) {
		return -1;
	}
#endif

#ifdef PPE_TUNNEL_ENABLE
	if (!ppe_drv_tun_global_init(p)) {
		ppe_drv_warn("%p: failed to do global config init for tunnels", p);
		return -1;
	}
#endif

	p->pub_ip = ppe_drv_pub_ip_entries_alloc();
	if (!p->pub_ip) {
		ppe_drv_warn("%p: failed to allocate pub_ip entries", p);
		goto fail;
	}

	p->l3_if = ppe_drv_l3_if_entries_alloc();
	if (!p->l3_if) {
		ppe_drv_warn("%p: failed to allocate l3_if entries", p);
		goto fail;
	}

	p->vsi = ppe_drv_vsi_entries_alloc();
	if (!p->vsi) {
		ppe_drv_warn("%p: failed to allocate vsi entries", p);
		goto fail;
	}

	p->pppoe = ppe_drv_pppoe_entries_alloc();
	if (!p->pppoe) {
		ppe_drv_warn("%p: failed to allocate PPPoe entries", p);
		goto fail;
	}

	p->port = ppe_drv_port_entries_alloc();
	if (!p->port) {
		ppe_drv_warn("%p: failed to allocate Port entries", p);
		goto fail;
	}

	p->sc = ppe_drv_sc_entries_alloc();
	if (!p->sc) {
		ppe_drv_warn("%p: failed to allocate service code entries", p);
		goto fail;
	}

	p->iface = ppe_drv_iface_entries_alloc();
	if (!p->iface) {
		ppe_drv_warn("%p: failed to allocate iface entries", p);
		goto fail;
	}

	p->nexthop = ppe_drv_nexthop_entries_alloc();
	if (!p->nexthop) {
		ppe_drv_warn("%p: failed to allocate nexthop entries", p);
		goto fail;
	}

	p->host = ppe_drv_host_entries_alloc();
	if (!p->host) {
		ppe_drv_warn("%p: failed to allocate host entries", p);
		goto fail;
	}

	p->flow = ppe_drv_flow_entries_alloc();
	if (!p->flow) {
		ppe_drv_warn("%p: failed to allocate flow entries", p);
		goto fail;
	}

	p->cc = ppe_drv_cc_entries_alloc();
	if (!p->cc) {
		ppe_drv_warn("%p: failed to allocate cpu-code entries", p);
		goto fail;
	}

	ppe_drv_exception_init();
	status = ppe_drv_cpu_port_init();
	if (status) {
		ppe_drv_warn("%p: failed to initialize cpu port, error code = %u\n", p, status);
		goto fail;
	}

	if (!ppe_drv_phy_port_base_queue_init(p)) {
		ppe_drv_warn("%p: failed to initialize physical port base queue\n", p);
		goto fail;
	}

#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	/*
	 * Map PPE_DRV_SC_NOEDIT_TUN_RPS service code to the CPU port's base
	 * ucast queue.
	 */
	if (!ppe_drv_sc_noedit_tun_rps_queue_init()) {
		ppe_drv_warn("%p: failed to initialize CPU port queues to TUN RPS service code\n", p);
		goto fail;
	}
#endif

	if (!ppe_drv_confgiure_ucast_prio_map_tbl(PPE_DRV_REDIR_PROFILE_ID, ppe_drv_redir_prio_map)) {
		ppe_drv_warn("%p: failed to configure ucast priority class setting\n", p);
		goto fail;
	}

#ifdef NSS_PPE_DRV_HW_GRO
	if (!ppe_drv_confgiure_ucast_prio_map_tbl(PPE_DRV_GRO_PROFILE_ID, ppe_drv_gro_prio_map)) {
		ppe_drv_warn("%p: failed to configure ucast priority class setting\n", p);
		goto fail;
	}
#endif

	/*
	 *
	 * TODO: Check if we need to modify timer flag - specially irqsafe
	 * or pinned to a specific core.
	 */
	p->hw_flow_stats_ticks = msecs_to_jiffies(PPE_DRV_HW_FLOW_STATS_MS);
	timer_setup(&p->hw_flow_stats_timer, ppe_drv_hw_stats_sync, 0);

	/* Initialize list */
	INIT_LIST_HEAD(&p->conn_v4);
	INIT_LIST_HEAD(&p->conn_v6);
	INIT_LIST_HEAD(&p->conn_tun_v4);
	INIT_LIST_HEAD(&p->conn_tun_v6);
	INIT_LIST_HEAD(&p->notifier_list_head);
#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	INIT_LIST_HEAD(&p->lpbk_buffer_map);
	p->lpbk_buffer_map_count = 0;
#endif

	p->toggled_v4 = false;
	p->toggled_v6 = false;
	p->tun_toggled_v4 = false;
	p->tun_toggled_v6 = false;
	p->disable_port_mtu_check = true;
	p->fse_ops = NULL;
	p->fse_enable = false;
        p->is_wifi_fse_up = false;

#ifdef NSS_PPE_DRV_HW_GRO
	atomic_set(&p->gro_ctx.num_hw_gro_flows, 0);
#endif

	p->tun_gbl.tun_l2tp.l2tp_dport = PPE_DRV_L2TP_DEFAULT_UDP_PORT;
	p->tun_gbl.tun_l2tp.l2tp_sport = PPE_DRV_L2TP_DEFAULT_UDP_PORT;
#ifdef PPE_TUNNEL_ENABLE
	/*
	 * Allocate tunnel specific entries
	 */

	p->ptun_ec = ppe_drv_tun_encap_entries_alloc(p);
	if (!p->ptun_ec) {
		ppe_drv_warn("%p: failed to allocate tunnel encap entries", p);
		goto fail;
	}

	p->ptun_dc = ppe_drv_tun_decap_entries_alloc(p);
	if (!p->ptun_dc) {
		ppe_drv_warn("%p: failed to allocate tunnel decap entries", p);
		goto fail;
	}

	p->ptun_l3_if = ppe_drv_tun_l3_if_entries_alloc(p);
	if (!p->ptun_l3_if) {
		ppe_drv_warn("%p: failed to allocate TL L3 interface entries", p);
		goto fail;
	}

	p->decap_map_entries = ppe_drv_tun_decap_map_entries_alloc(p);
	if (!p->decap_map_entries) {
		ppe_drv_warn("%p: failed to allocate TL MAP LPM table interface entries", p);
		goto fail;
	}

	p->encap_xlate_rules = ppe_drv_tun_encap_xlate_rule_entries_alloc(p);
	if (!p->encap_xlate_rules) {
		ppe_drv_warn("%p: failed to allocate EG edit rule entries", p);
		goto fail;
	}

	p->decap_xlate_rules = ppe_drv_tun_decap_xlate_rule_entries_alloc(p);
	if (!p->decap_xlate_rules) {
		ppe_drv_warn("%p: failed to allocate TL MAP LPM action interface entries", p);
		goto fail;
	}
#endif

	p->acl = ppe_drv_acl_entries_alloc();
	if (!p->acl) {
		ppe_drv_warn("%p: failed to allocate ACL entries", p);
		goto fail;
	}

#ifdef NSS_PPE_FEATURE_DOT1P
	p->dot1p = ppe_drv_dot1p_entries_alloc();
	if (!p->dot1p) {
		ppe_drv_warn("%p: failed to allocate DOT1P entries", p);
		goto fail;
	}

	p->dot1p_policer = ppe_drv_dot1p_policer_entries_alloc();
	if (!p->dot1p_policer) {
		ppe_drv_warn("%p: failed to allocate DOT1P policer entries", p);
		goto fail;
	}
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
	p->gemport = ppe_drv_gem_port_entries_alloc();
	if (!p->gemport) {
		ppe_drv_warn("%p: failed to allocate GEMPORT entries", p);
		goto fail;
	}
#endif

	p->pol_ctx = ppe_drv_policer_entries_alloc();
	if (!p->pol_ctx) {
		ppe_drv_warn("%p: failed to allocate global policer context", p);
		goto fail;
	}

#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	/* Allocate PM counter management */
	p->pm = ppe_drv_pm_entries_alloc();
	if (!p->pm) {
		ppe_drv_warn("%p: Failed to allocate PM entries", p);
		goto fail;
	}

	/* Allocate PM counter gen management */
	p->pm_gen = ppe_drv_pm_gen_entries_alloc();
	if (!p->pm_gen) {
		ppe_drv_warn("%p: Failed to allocate PM gen entries", p);
		goto fail;
	}
#endif

#ifdef NSS_PPE_DRV_PORT_MGMT_SUPPORT
	p->port_mgmt = ppe_drv_port_mgmt_entries_alloc();
	if (!p->port_mgmt) {
		ppe_drv_warn("%p: Failed to allocate Port Mgmt entries", p);
		goto fail;
	}
#endif

	/* Allocate VLAN entries */
	p->vlan = ppe_drv_vlan_entries_alloc();
	if (!p->vlan) {
		ppe_drv_warn("%p: Failed to allocate VLAN entries", p);
		goto fail;
	}

#ifdef PPE_TUNNEL_ENABLE
	p->pgm = ppe_drv_tun_prgm_prsr_alloc(p);
	if (!p->pgm) {
		ppe_drv_warn("%p: failed to allocate program parser entries", p);
		goto fail;
	}

	p->pgm_udf = ppe_drv_tun_udf_alloc(p);
	if (!p->pgm_udf) {
		ppe_drv_warn("%p: failed to allocate tunnel udf entries", p);
		goto fail;
	}

#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	p->tun_tpr = ppe_drv_tun_tpr_alloc(p);
	if (!p->tun_tpr) {
		ppe_drv_warn("%p: failed to allocate tunnel tpr entries", p);
		goto fail;
	}
#endif
#endif
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	p->pfx = ppe_drv_nptv6_prefix_entries_alloc();
	if (!p->pfx) {
		ppe_drv_warn("%p: failed to allocate prefix table entries", p);
		goto fail;
	}

	p->iid = ppe_drv_nptv6_iid_entries_alloc();
	if (!p->iid) {
		ppe_drv_warn("%p: failed to allocate IID table entries", p);
		goto fail;
	}
#endif

	for (i = 0; i <  PPE_DRV_PORT_SRC_PROFILE_MAX; i++) {
		PPE_DRV_PROF2PORT_CLEAR_ALL(p->prof2portmap[i]);
	}

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
	ppe_drv_port_loopback_port_get_info();
#endif

	/*
	 * Take a reference
	 */
	kref_init(&p->ref);

	/*
	 * Start sync timer for hardware stats collection.
	 */
	mod_timer(&p->hw_flow_stats_timer, jiffies + p->hw_flow_stats_ticks);

	/*
	 * Non availability of debugfs directory is not a catastrophy
	 * We can still go ahead with other initialization
	 */
	ppe_drv_stats_debugfs_init();

	ppe_drv_flow_dump_init(p->dentry);
	ppe_drv_if_map_init(p->dentry);

	/*
	 * Initialize the enqueue vports.
	 */
	ppe_drv_port_enq_vp_init();

#ifdef NSS_PPE_SIXTH_TUPLE
	/*
	 * Enable vsi as flow key.
	 */
	if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VSI) {
		ppe_drv_vsi_flow_key_enable();
	}
#endif

	nss_ppe_drv_minidump_log(p, sizeof(struct ppe_drv), "ppe_drv");

#ifdef NSS_PPE_FEATURE_DOT1P
	if (pon_enable) {
		ppe_drv_dot1p_configure_default_rule();
	}
#endif

	/*
	 * On Hermosa, map multicast traffic to port 0 unicast queues to enable
	 * EDMA RPS on AP variants. For PON variant, ppe_drv_dp_gem_enable()
	 * overrides this to port 6 for T-CONT scheduling. Other SoCs natively
	 * use unicast queues for multicast and do not have this register.
	 */
#ifdef PPE_DRV_MCAST_ENQ_CTRL
	ppe_drv_mcast_enqueue_ctrl_init();
#endif

	return of_platform_populate(np, NULL, NULL, &pdev->dev);

fail:

#ifdef PPE_TUNNEL_ENABLE
	if (p->decap_map_entries) {
		ppe_drv_tun_decap_entries_free(p->decap_map_entries);
		p->decap_map_entries = NULL;
	}

	if (p->encap_xlate_rules) {
		ppe_drv_tun_encap_xlate_rule_entries_free(p->encap_xlate_rules);
		p->encap_xlate_rules = NULL;
	}

	if (p->decap_xlate_rules) {
		ppe_drv_tun_decap_xlate_rule_entries_free(p->decap_xlate_rules);
		p->decap_xlate_rules = NULL;
	}

	if (p->ptun_ec) {
		ppe_drv_tun_encap_entries_free(p->ptun_ec);
		p->ptun_ec = NULL;
	}

	if (p->ptun_dc) {
		ppe_drv_tun_decap_entries_free(p->ptun_dc);
		p->ptun_dc = NULL;
	}

	if (p->ptun_l3_if) {
		ppe_drv_tun_l3_if_entries_free(p->ptun_l3_if);
		p->ptun_l3_if = NULL;
	}
#endif

	if (p->pol_ctx) {
		ppe_drv_policer_entries_free(p->pol_ctx);
		p->pol_ctx = NULL;
	}

#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	if (p->pm) {
		ppe_drv_pm_entries_free(p->pm);
		p->pm = NULL;
	}

	if (p->pm_gen) {
		ppe_drv_pm_entries_free(p->pm_gen);
		p->pm_gen = NULL;
	}
#endif
#ifdef NSS_PPE_DRV_PORT_MGMT_SUPPORT
	if (p->port_mgmt) {
		ppe_drv_port_mgmt_entries_free(p->port_mgmt);
		p->port_mgmt = NULL;
	}
#endif
	if (p->vlan) {
		ppe_drv_vlan_entries_free(p->vlan);
		p->vlan = NULL;
	}

	if (p->acl) {
		ppe_drv_acl_entries_free(p->acl);
		p->acl = NULL;
	}

#ifdef NSS_PPE_FEATURE_DOT1P
	if (p->dot1p) {
		ppe_drv_dot1p_entries_free(p->dot1p);
		p->dot1p = NULL;
	}

	if (p->dot1p_policer) {
		ppe_drv_dot1p_policer_entries_free(p->dot1p_policer);
		p->dot1p_policer = NULL;
	}
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
	if (p->gemport) {
		ppe_drv_gem_port_entries_free(p->gemport);
		p->gemport = NULL;
	}
#endif

	if (p->pub_ip) {
		ppe_drv_pub_ip_entries_free(p->pub_ip);
		p->pub_ip = NULL;
	}

	if (p->l3_if) {
		ppe_drv_l3_if_entries_free(p->l3_if);
		p->l3_if = NULL;
	}

	if (p->vsi) {
		ppe_drv_vsi_entries_free(p->vsi);
		p->vsi = NULL;
	}

	if (p->pppoe) {
		ppe_drv_pppoe_entries_free(p->pppoe);
		p->pppoe = NULL;
	}

	if (p->port) {
		ppe_drv_port_entries_free(p->port);
		p->port = NULL;
	}

	if (p->sc) {
		ppe_drv_sc_entries_free(p->sc);
		p->sc = NULL;
	}

	if (p->iface) {
		ppe_drv_iface_entries_free(p->iface);
		p->iface = NULL;
	}

	if (p->nexthop) {
		ppe_drv_nexthop_entries_free(p->nexthop);
		p->nexthop = NULL;
	}

	if (p->host) {
		ppe_drv_host_entries_free(p->host);
		p->host = NULL;
	}

	if (p->flow) {
		ppe_drv_flow_entries_free(p->flow);
		p->flow = NULL;
	}

	if (p->cc) {
		ppe_drv_cc_entries_free(p->cc);
		p->cc = NULL;
	}

#ifdef PPE_TUNNEL_ENABLE
	if (p->pgm) {
		ppe_drv_tun_prgm_prsr_free(p->pgm);
		p->pgm = NULL;
	}

	if (p->pgm_udf) {
		ppe_drv_tun_udf_free(p->pgm_udf);
		p->pgm_udf = NULL;
	}

	if (p->ecap_hdr_ctrl) {
		ppe_drv_tun_encap_hdr_ctrl_free(p->ecap_hdr_ctrl);
		p->ecap_hdr_ctrl = NULL;
	}

#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	if (p->tun_tpr) {
		ppe_drv_tun_tpr_free(p->tun_tpr);
		p->tun_tpr = NULL;
	}
#endif
#endif
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	if (p->pfx) {
		ppe_drv_nptv6_prefix_entries_free(p->pfx);
		p->pfx = NULL;
	}

	if (p->iid) {
		ppe_drv_nptv6_iid_entries_free(p->iid);
		p->iid = NULL;
	}
#endif

	ppe_drv_flow_dump_exit();
	ppe_drv_if_map_exit();

	return -1;
}

/*
 * ppe_drv_remove()
 *	remove the ppe driver
 */
static int ppe_drv_remove(struct platform_device *pdev)
{
	struct ppe_drv *p = platform_get_drvdata(pdev);
	int i = 0;

	ppe_drv_stats_debugfs_exit();

#ifdef NSS_PPE_SIXTH_TUPLE
	if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VSI) {
		ppe_drv_vsi_flow_key_disable();
	}
#endif
	for (i = 0; i <  PPE_DRV_PORT_SRC_PROFILE_MAX; i++) {
		PPE_DRV_PROF2PORT_CLEAR_ALL(p->prof2portmap[i]);
	}

	if (p->pub_ip) {
		ppe_drv_pub_ip_entries_free(p->pub_ip);
		p->pub_ip = NULL;
	}

	if (p->l3_if) {
		ppe_drv_l3_if_entries_free(p->l3_if);
		p->l3_if = NULL;
	}

	if (p->vsi) {
		ppe_drv_vsi_entries_free(p->vsi);
		p->vsi = NULL;
	}

	if (p->pppoe) {
		ppe_drv_pppoe_entries_free(p->pppoe);
		p->pppoe = NULL;
	}

	if (p->port) {
		ppe_drv_port_entries_free(p->port);
		p->port = NULL;
	}

	if (p->sc) {
		ppe_drv_sc_entries_free(p->sc);
		p->sc = NULL;
	}

	if (p->iface) {
		ppe_drv_iface_entries_free(p->iface);
		p->iface = NULL;
	}

	if (p->nexthop) {
		ppe_drv_nexthop_entries_free(p->nexthop);
		p->nexthop = NULL;
	}

	if (p->host) {
		ppe_drv_host_entries_free(p->host);
		p->host = NULL;
	}

	if (p->flow) {
		ppe_drv_flow_entries_free(p->flow);
		p->flow = NULL;
	}

	if (p->cc) {
		ppe_drv_cc_entries_free(p->cc);
		p->cc = NULL;
	}

#ifdef PPE_TUNNEL_ENABLE
	if (p->ptun_ec) {
		ppe_drv_tun_encap_entries_free(p->ptun_ec);
		p->ptun_ec = NULL;
	}

	if (p->ptun_dc) {
		ppe_drv_tun_decap_entries_free(p->ptun_dc);
		p->ptun_dc = NULL;
	}

	if (p->ptun_l3_if) {
		ppe_drv_tun_l3_if_entries_free(p->ptun_l3_if);
		p->ptun_l3_if = NULL;
	}

	if (p->decap_map_entries) {
		ppe_drv_tun_decap_entries_free(p->decap_map_entries);
		p->decap_map_entries = NULL;
	}

	if (p->encap_xlate_rules) {
		ppe_drv_tun_encap_xlate_rule_entries_free(p->encap_xlate_rules);
		p->encap_xlate_rules = NULL;
	}

	if (p->decap_xlate_rules) {
		ppe_drv_tun_decap_xlate_rule_entries_free(p->decap_xlate_rules);
		p->decap_xlate_rules = NULL;
	}
#endif

	if (p->acl) {
		ppe_drv_acl_entries_free(p->acl);
		p->acl = NULL;
	}

#ifdef NSS_PPE_FEATURE_DOT1P
	if (p->dot1p) {
		ppe_drv_dot1p_entries_free(p->dot1p);
		p->dot1p = NULL;
	}

	if (p->dot1p_policer) {
		ppe_drv_dot1p_policer_entries_free(p->dot1p_policer);
		p->dot1p_policer = NULL;
	}
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
	if (p->gemport) {
		ppe_drv_gem_port_entries_free(p->gemport);
		p->gemport = NULL;
	}
#endif

#ifdef PPE_TUNNEL_ENABLE
	ppe_drv_tun_vxlan_deconfigure(p);
#endif

	if (p->pol_ctx) {
		ppe_drv_policer_entries_free(p->pol_ctx);
		p->pol_ctx = NULL;
	}

#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	if (p->pm) {
		ppe_drv_pm_entries_free(p->pm);
		p->pm = NULL;
	}

	if (p->pm_gen) {
		ppe_drv_pm_entries_free(p->pm_gen);
		p->pm_gen = NULL;
	}
#endif

	if (p->vlan) {
		ppe_drv_vlan_entries_free(p->vlan);
		p->vlan = NULL;
	}

	if (p->fse_ops) {
		ppe_drv_warn("FSE ops still registered while ppe module getting removed\n");
	}

#ifdef PPE_TUNNEL_ENABLE
	if (p->pgm) {
		ppe_drv_tun_prgm_prsr_free(p->pgm);
		p->pgm = NULL;
	}

	if (p->pgm_udf) {
		ppe_drv_tun_udf_free(p->pgm_udf);
		p->pgm_udf = NULL;
	}

	if (p->ecap_hdr_ctrl) {
		ppe_drv_tun_encap_hdr_ctrl_free(p->ecap_hdr_ctrl);
		p->ecap_hdr_ctrl = NULL;
	}

#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	if (p->tun_tpr) {
		ppe_drv_tun_tpr_free(p->tun_tpr);
		p->tun_tpr = NULL;
	}
#endif
#endif
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	if (p->pfx) {
		ppe_drv_nptv6_prefix_entries_free(p->pfx);
		p->pfx = NULL;
	}

	if (p->iid) {
		ppe_drv_nptv6_iid_entries_free(p->iid);
		p->iid = NULL;
	}
#endif

	ppe_drv_flow_dump_exit();
	ppe_drv_if_map_exit();

	return 0;
}

/*
 * ppe_drv_notify_change_upper_handler()
 *	Call registered notifiers with ppe drv
 *	To-do: Introduce new file for ppe-drv notifier functions
 */
static void ppe_drv_notify_change_upper_handler(void *ptr, int event)
{
	struct ppe_drv_notifier_ops *iterator;
	struct ppe_drv *p = ppe_drv_gbl;
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;
	struct netdev_notifier_changeupper_info *cu_info = (struct netdev_notifier_changeupper_info *)info;

	spin_lock_bh(&p->notifier_lock);
	if (cu_info->linking) {
		ppe_drv_trace("%px: Linking ppe drv event: %d\n", ptr, event);
		list_for_each_entry(iterator, &p->notifier_list_head, entry) {
			if (iterator->notifier_call) {
				(iterator->notifier_call)(iterator, event, info);
			}
		}
		spin_unlock_bh(&p->notifier_lock);
		return;
	}

	/*
	 * Call notifier in reverse order during unlinking
	 * to clear the database in the correct order
	 */
	ppe_drv_trace("%px: Unlinking ppe drv event: %d\n", ptr, event);
	list_for_each_entry_reverse(iterator, &p->notifier_list_head, entry) {
		if (iterator->notifier_call) {
			(iterator->notifier_call)(iterator, event, ptr);
		}
	}
	spin_unlock_bh(&p->notifier_lock);
}

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
/*
 * ppe_drv_netdev_unregister_handler()
 * 	API to handle NETDEV_UNREGISTER handler.
 */
static void ppe_drv_netdev_unregister_handler(struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_lpbk_buffer_map_entry *map_entry, *tmp;

	if (dev->ieee80211_ptr) {
		ppe_drv_trace("Dev: %s a WLAN dev\n", dev->name);
		return;
	}

	spin_lock_bh(&p->lock);

	list_for_each_entry_safe(map_entry, tmp, &p->lpbk_buffer_map, list) {
		if ((map_entry->ingress_dev == dev) || (map_entry->egress_dev == dev)) {
			ppe_drv_trace("%p: lpbk_buffer_map removing entry for dev %s : pair %s->%s\n",
					p, dev->name,
					map_entry->ingress_dev->name,
					map_entry->egress_dev->name);
			list_del(&map_entry->list);
			dev_put(map_entry->ingress_dev);
			dev_put(map_entry->egress_dev);
			kfree(map_entry);
			p->lpbk_buffer_map_count--;
		}
	}

	spin_unlock_bh(&p->lock);

	return;
}
#endif
/*
 * ppe_drv_handle_netdev_event()
 *	Handle events received from network stack
 */
static int ppe_drv_handle_netdev_event(struct notifier_block *nb,
		unsigned long event, void *ptr)
{
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	switch (event) {
	case NETDEV_CHANGEUPPER:
		ppe_drv_notify_change_upper_handler(ptr, PPE_DRV_EVENT_CHANGEUPPER);
		break;
#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	case NETDEV_UNREGISTER:
		ppe_drv_netdev_unregister_handler(dev);
		break;
#endif
	default:
		 ppe_drv_trace("Event not supported through ppe-drv: %ld\n", event);
	}

	return NOTIFY_DONE;
}

/* register netdev notifier callback */
static struct notifier_block nss_ppe_netdevice __read_mostly = {
	.notifier_call = ppe_drv_handle_netdev_event,
};

/*
 * ppe_drv_platform
 *	platform device instance
 */
static struct platform_driver ppe_drv_platform = {
	.probe		= ppe_drv_probe,
	.remove		= ppe_drv_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "nss-ppe",
		.of_match_table = of_match_ptr(ppe_drv_dt_ids),
	},
};

/*
 * ppe_drv_notifier_ops_register()
 *	Register notifier operations
 */
void ppe_drv_notifier_ops_register(struct ppe_drv_notifier_ops *notifier_ops)
{
	struct list_head *ptr;
	struct ppe_drv_notifier_ops *entry_pnb;
	struct ppe_drv *p = ppe_drv_gbl;

	ppe_drv_trace("%px: Add notifier callback with priority: %d\n", notifier_ops, notifier_ops->priority);

	spin_lock_bh(&p->notifier_lock);
	list_for_each(ptr, &p->notifier_list_head) {
		entry_pnb = list_entry(ptr, struct ppe_drv_notifier_ops, entry);
		if (entry_pnb->priority < notifier_ops->priority) {
			list_add(&notifier_ops->entry, &entry_pnb->entry);
			spin_unlock_bh(&p->notifier_lock);
			return;
		}
	}

	list_add(&notifier_ops->entry, &p->notifier_list_head);
	spin_unlock_bh(&p->notifier_lock);
}
EXPORT_SYMBOL(ppe_drv_notifier_ops_register);

/*
 * ppe_drv_notifier_ops_unregister()
 *	Unregister notifier operations
 */
void ppe_drv_notifier_ops_unregister(struct ppe_drv_notifier_ops *notifier_ops)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->notifier_lock);
	list_del(&notifier_ops->entry);
	spin_unlock_bh(&p->notifier_lock);
}
EXPORT_SYMBOL(ppe_drv_notifier_ops_unregister);

/*
 * ppe_drv_set_ppe_if_bm_to_port()
 *	API to set the OFFLOAD flag in the port from the interface bitmask
 */
static void ppe_drv_set_ppe_if_bm_to_port(void)
{
	struct ppe_drv_port *drv_port = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	unsigned int if_bit_count = __builtin_popcount(PPE_DRV_PORT_OFFLOAD_MAX_VAL);
	uint32_t i;

	spin_lock_bh(&p->lock);
	for (i = 1; i <= if_bit_count; i++) {
		drv_port = ppe_drv_port_from_port_num(i);
		if (!drv_port) {
			ppe_drv_warn("failed in getting drv port from %d port\n", i);
			continue;
		}

		if (if_bm_to_offload & (1 << (i - 1))) {
			drv_port->flags |= PPE_DRV_PORT_FLAG_OFFLOAD_ENABLED;
		} else {
			drv_port->flags &= ~PPE_DRV_PORT_FLAG_OFFLOAD_ENABLED;
		}
	}
	spin_unlock_bh(&p->lock);
}

/*
 * ppe_drv_if_bm_to_offload_handler()
 *	API to configure the interface bitmask where the offload is enabled
 */
static int ppe_drv_if_bm_to_offload_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	uint32_t current_value;

	/*
	 * Take the current value
	 */
	current_value = if_bm_to_offload;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if (if_bm_to_offload > PPE_DRV_PORT_OFFLOAD_MAX_VAL) {
		ppe_drv_warn("Incorrect value of offload bitmask. Setting it to"
				" default value. Value: %0x, if_bm_to_offload: %0x\n",
				if_bm_to_offload, PPE_DRV_PORT_OFFLOAD_MAX_VAL);
		if_bm_to_offload = current_value;
	}

	ppe_drv_set_ppe_if_bm_to_port();

	ppe_drv_warn("PPE DRV interface bitmask to offload is %0x\n", if_bm_to_offload);
	return ret;
}

/*
 * ppe_drv_disable_port_mtu_check_handler()
 * 	Set disable port mtu config
 */
static int ppe_drv_disable_port_mtu_check_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	int ret;
	struct ppe_drv *p = ppe_drv_gbl;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	p->disable_port_mtu_check = disable_port_mtu_check;

	ppe_drv_info("Updating disable_port_mtu_check flag as %d\n", p->disable_port_mtu_check);
	return ret;
}

/*
 * ppe_drv_l4_checksum_exception_enable()
 *	API to enable/disable the exception for l4 checksum.
 */
int ppe_drv_l4_checksum_exception_enable(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	uint32_t current_value;

	/*
	 * Take the current value
	 */
	current_value = l4_checksum_exception_enable;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write) {
		return ret;
	}

	/*
	 * Return if you are trying to set the same value.
	 */
	if (current_value == l4_checksum_exception_enable) {
		return 0;
	}

	if(ppe_drv_exception_l4_checksum_enable()) {
		ppe_drv_trace("PPE DRV l4 exception is:%d ret:%d\n", l4_checksum_exception_enable, ret);
		return -1;
	}

	ppe_drv_trace("PPE DRV l4 exception is %d\n", l4_checksum_exception_enable);

	return 0;
}

/*
 * ppe_drv_mac_lrn_exception_en()
 *	API to enable/disable the exception for MAC address learn exception.
 */
int ppe_drv_mac_lrn_exception_en(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write) {
		return ret;
	}

	ppe_drv_trace("PPE DRV MAC address learn exception %d\n", mac_lrn_exception_en);

	return 0;
}

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
/*
 * ppe_drv_loopback_port_feature_write_handler()
 *	sysctl handler for loopback port write operation.
 */
static int ppe_drv_loopback_port_feature_write_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int ret;

	ret = proc_douintvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if (loopback_port_ft_type == PPE_DRV_LOOPBACK_PORT_FT_TYPE_NONE) {
		/*
		 * If needed add the code to disable the previously
		 * enabled feature.
		 */

		p->loopback_port_info.ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_NONE;
		memset(p->loopback_port_info.ctx, 0, sizeof(p->loopback_port_info.ctx));
		return ret;
	}

	if (p->loopback_port_info.enabled == false) {
		ppe_drv_warn("Loopback port is not enabled\n");
		return ret;
	}

	switch (loopback_port_ft_type) {
	/*
	 * configure the direction speific information.
	 * configure the service code.
	 * configure the queue to service code mapping.
	 * set p->loopback_port_info.ft_type.
	 */
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	case PPE_DRV_LOOPBACK_PORT_FT_TYPE_HAIRPIN_NAT:
		 if (!ppe_drv_lpbk_port_info_ctx_fill(PPE_DRV_LOOPBACK_PORT_FT_TYPE_HAIRPIN_NAT)) {
			ppe_drv_warn("%p: Failed to configure hairpin NAT loopback port\n", p);
		 }
		break;
#endif
	default:
		ppe_drv_warn("invalid feature type %u\n", loopback_port_ft_type);
		break;
	}

	return ret;
}
#endif

/*
 * ppe_drv_wlan_host_assist_qdisc_en_handler()
 *	Handler for wlan_host_assist_qdisc_en sysctl
 */
static int ppe_drv_wlan_host_assist_qdisc_en_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write) {
		return ret;
	}

	/*
	 * Validate the input value (should be 0 or 1)
	 */
	if (wlan_host_assist_qdisc_en != 0 && wlan_host_assist_qdisc_en != 1) {
		ppe_drv_warn("Invalid value for wlan_host_assist_qdisc_en. Must be 0 or 1\n");
		wlan_host_assist_qdisc_en = 0;
		return -EINVAL;
	}

	ppe_drv_info("PPE acceleration of WALN flow with host assist qdisc enable is set to %d\n", wlan_host_assist_qdisc_en);

	return 0;
}

/*
 * ppe_drv_get_unknown_dst_rfs_enable()
 *	Get the value of unknown_dst_rfs_enable.
 */
int ppe_drv_get_unknown_dst_rfs_enable(void)
{
	struct ppe_drv *p = ppe_drv_gbl;

	return p->rfs.unknown_dst_rfs_enable;
}
EXPORT_SYMBOL(ppe_drv_get_unknown_dst_rfs_enable);

/*
 * ppe_drv_unknown_dst_rfs_enable_handler()
 *	Handler for unknown_dst_rfs_enable sysctl
 */
static int ppe_drv_unknown_dst_rfs_enable_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write) {
		return ret;
	}

	/*
	 * Validate the input value (should be 0 or 1)
	 */
	if (unknown_dst_rfs_enable != 0 && unknown_dst_rfs_enable != 1) {
		ppe_drv_warn("Invalid value for unknown_dst_rfs_enable. Must be 0 or 1\n");
		unknown_dst_rfs_enable = 0;
		return -EINVAL;
	}

	spin_lock_bh(&p->lock);
	p->rfs.unknown_dst_rfs_enable = unknown_dst_rfs_enable;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("PPE RFS support for unknown destination devices is set to %d\n", unknown_dst_rfs_enable);

	return 0;
}

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_drv_dot1p_dest_vp_en_handler()
 *	API to configure the default gemport dest_vp in Dot1p mapper table.
 */
static int ppe_drv_dot1p_dest_vp_en_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	ppe_drv_trace("dest vp in dot1p mapper table  %d\n", dot1p_dest_vp_en);

        return 0;
}
#endif

#ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
/*
 * ppe_drv_ipsec_pass_through_en_handler()
 *	Handler for ppe_drv_ipsec_passth_en sysctl
 *
 *	This handler configures IPsec passthrough by managing the flow_key_en_bitmap
 *	in the PPE hardware. The bitmap controls which protocol fields are used for
 *	flow key generation.
 *
 *	Flow key bitmap bits:
 *		- BIT(0): FAL_FLOW_KEY_ROUTING
 *		- BIT(1): FAL_FLOW_KEY_IPSEC_AH  (IPsec Authentication Header)
 *		- BIT(2): FAL_FLOW_KEY_IPSEC_ESP (IPsec Encapsulating Security Payload)
 *		- BIT(3): FAL_FLOW_KEY_IPSEC_NATT (IPsec NAT Traversal)
 *
 *	When ppe_drv_ipsec_passth_en is set to 1:
 *		- Reads the current flow_key_en_bitmap configuration
 *		- Sets BIT(1) and BIT(2) to enable IPsec AH and ESP flow keys
 *		- Writes the updated configuration back to hardware
 *
 *	When ppe_drv_ipsec_passth_en is set to 0:
 *		- Reads the current flow_key_en_bitmap configuration
 *		- Clears BIT(1) and BIT(2) to disable IPsec AH and ESP flow keys
 *		- Writes the updated configuration back to hardware
 */
static int ppe_drv_ipsec_pass_through_en_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	fal_flow_global_cfg_t cfg = {0};
	sw_error_t sw_ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write) {
		return ret;
	}

	/*
	 * Validate the input value (should be 0 or 1)
	 */
	if (ppe_drv_ipsec_passth_en != 0 && ppe_drv_ipsec_passth_en != 1) {
		ppe_drv_warn("Invalid value for ppe_drv_ipsec_passth_en. Must be 0 or 1\n");
		ppe_drv_ipsec_passth_en = 0;
		return -EINVAL;
	}

	/*
	 * Read the existing flow_key_en_bitmap value from hardware.
	 * This ensures we preserve other bits that may be set.
	 */
	sw_ret = fal_flow_global_cfg_get(0, &cfg);
	if (sw_ret != SW_OK) {
		ppe_drv_warn("Failed to get flow global config, error: %d\n", sw_ret);
		return -EIO;
	}

	if (ppe_drv_ipsec_passth_en == 1) {
		/*
		 * Enable IPsec passthrough by setting BIT(1) and BIT(2).
		 * Use bitwise OR to set the bits while preserving other bits.
		 * FAL_FLOW_KEY_IPSEC_AH  = BIT(1) - IPsec Authentication Header
		 * FAL_FLOW_KEY_IPSEC_ESP = BIT(2) - IPsec Encapsulating Security Payload
		 */
		cfg.flow_key_en_bitmap |= FAL_FLOW_KEY_IPSEC_AH;
		cfg.flow_key_en_bitmap |= FAL_FLOW_KEY_IPSEC_ESP;
		ppe_drv_info("Enabling IPsec passthrough, flow_key_en_bitmap: 0x%x\n", cfg.flow_key_en_bitmap);
	} else {
		/*
		 * Disable IPsec passthrough by clearing BIT(1) and BIT(2).
		 * Use bitwise AND with complement to clear the bits while preserving other bits.
		 */
		cfg.flow_key_en_bitmap &= ~FAL_FLOW_KEY_IPSEC_AH;
		cfg.flow_key_en_bitmap &= ~FAL_FLOW_KEY_IPSEC_ESP;
		ppe_drv_info("Disabling IPsec passthrough, flow_key_en_bitmap: 0x%x\n", cfg.flow_key_en_bitmap);
	}

	/*
	 * Write the updated configuration back to hardware.
	 * This applies the IPsec passthrough enable/disable setting.
	 */
	sw_ret = fal_flow_global_cfg_set(0, &cfg);
	if (sw_ret != SW_OK) {
		ppe_drv_warn("Failed to set flow global config, error: %d\n", sw_ret);
		return -EIO;
	}

	ppe_drv_info("PPE IPsec passthrough enable set to %d\n", ppe_drv_ipsec_passth_en);

	return 0;
}
#endif

/*
 * ppe_drv_eth2eth_offload_if_bitmap_handler()
 * 	Set eth to eth offload with if bitmap config
 */
static int ppe_drv_eth2eth_offload_if_bitmap_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	int ret;
	struct ppe_drv *p = ppe_drv_gbl;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	p->eth2eth_offload_if_bitmap = eth2eth_offload_if_bitmap;

	ppe_drv_info("Updating eth2eth_offload_if_bitmap flag as %d\n", p->eth2eth_offload_if_bitmap);
	return ret;
}

/*
 * ppe_drv_static_dbg_level_handler()
 *	Set static debug level for ppe-driver.
 */
static int ppe_drv_static_dbg_level_handler(struct ctl_table *table,
					int write, void __user *buffer,
					size_t *lenp, loff_t *ppos)
{
	int ret;
	char *level_str;
	enum ppe_drv_static_dbg_level dbg_level;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	level_str = static_dbg_level_str;
	printk("dbg_level: %s", level_str);

	if (!strcmp(level_str, "warn")) {
		dbg_level = PPE_DRV_STATIC_DBG_LEVEL_WARN;
	} else if (!strcmp(level_str, "info")) {
		dbg_level = PPE_DRV_STATIC_DBG_LEVEL_INFO;
	} else if (!strcmp(level_str, "trace")) {
		dbg_level = PPE_DRV_STATIC_DBG_LEVEL_TRACE;
	} else if (!strcmp(level_str, "none")) {
		dbg_level = PPE_DRV_STATIC_DBG_LEVEL_NONE;
	} else {
		printk("Usage: echo '[warn|info|trace|none]' > /proc/sys/ppe/ppe_drv/static_dbg_level\n");
		return -EINVAL;
	}

	if (dbg_level >= PPE_DRV_DEBUG_LEVEL) {
		printk("debug level: %d not compiled in: %d\n", dbg_level, PPE_DRV_DEBUG_LEVEL);
		return -EINVAL;
	}

	static_dbg_level = dbg_level;
	return ret;
}

#ifdef PPE_DRV_PKT_PADDING_STRIP
/*
 * ppe_drv_pkt_padding_handler
 * 	sysctl to enable/disable stripping of packet padding
 */
static int ppe_drv_pkt_padding_handler(struct ctl_table *table,  int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	fal_pktedit_padding_t padding = { 0 };
	char *start_ch_ptr = NULL;
	char *end_ch_ptr = NULL;
	char config[32];
	int len, i, ret;
	char *map_name;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	map_name = packet_padding;

	/*
	 * echo "route_pad_strip_en=1 br_pad_strip_en=0 tun_inner_pad_strip_en=0" > /proc/sys/ppe/ppe_drv/packet_padding
	 */
	start_ch_ptr = map_name + strspn(map_name, NSS_PPE_DRV_WHITESPACE);
	for (i = 0; *start_ch_ptr; start_ch_ptr = end_ch_ptr + strspn(end_ch_ptr, NSS_PPE_DRV_WHITESPACE), i++) {
		char *config_str, *pad_strip_config;
		int pad_strip_en;

		end_ch_ptr = start_ch_ptr + strcspn(start_ch_ptr, NSS_PPE_DRV_WHITESPACE);
		if (end_ch_ptr != start_ch_ptr) {
			len = end_ch_ptr - start_ch_ptr;
		} else {
			len = strlen(start_ch_ptr);
		}

		if (len <= 32) {
			memcpy(config, start_ch_ptr, len);
		}

		config[len] = '\0';

		/*
		 * Obtain the config, value pair
		 */
		config_str = config;
		pad_strip_config = strsep(&config_str, "=");
		if (pad_strip_config != NULL) {
			pad_strip_en = *config_str - '0';
		} else {
			return -1;
		}

		if ((pad_strip_en != 0) && (pad_strip_en != 1)) {
			pr_err("Invalid config value\n");
			return -1;
		}

		if (!strcmp(pad_strip_config, "route_pad_strip_en")) {
			padding.strip_padding_route_en = pad_strip_en;
			if (padding.strip_padding_route_en) {
				padding.strip_padding_en = 1;
			}
		} else if (!strcmp(pad_strip_config, "br_pad_strip_en")) {
			padding.strip_padding_bridge_en = pad_strip_en;
			if (padding.strip_padding_bridge_en) {
				padding.strip_padding_en = 1;
			}
		} else if (!strcmp(pad_strip_config, "tun_inner_pad_strip_en")) {
			padding.strip_tunnel_inner_padding_en = pad_strip_en;
			if (padding.strip_tunnel_inner_padding_en) {
				padding.strip_padding_en = 1;
			}
		} else {
			pr_err("Invalid config, usage example: echo route_pad_strip_en=1"
					" br_pad_strip_en=0 tun_inner_pad_strip_en=0 > /proc/sys/ppe/ppe_drv/packet_padding/n");
			return -1;
		}
	}

	if (fal_pktedit_padding_set(PPE_DRV_SWITCH_ID, &padding) != SW_OK) {
		ppe_drv_warn("Failed to update strip_padding_en config \n");
		return -1;
	}

	ppe_drv_info("Updating strip_padding_en config to %d\n", padding.strip_padding_en);

	return ret;
}
#endif

/*
 * ppe_drv_upstream_dev_handler()
 *      Set upstream device.
 */
static int ppe_drv_upstream_dev_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	char *dev_name;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = NULL;
	struct net_device *dev;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

#if defined(PPE_LOOPBACK_RING_SUPPORT)
	/*
	 * Check if loopback ring is enabled
	 */
	if (!p->loopback_ring_info.enabled) {
		ppe_drv_warn("Loopback ring is not enabled\n");
		return -1;
	}
#else
	/*
	 * TO-DO: enable this support with loopback feature
	 */
	ppe_drv_warn("loopback feature support not enabled for upstream port\n");
	return -1;
#endif

	dev_name = upstream_dev_str;

	spin_lock_bh(&p->lock);

	if (p->upstream_dev) {
		ppe_drv_warn("%p: Upstream port already set\n", p);
		spin_unlock_bh(&p->lock);
		return -1;
	}

	dev = dev_get_by_name(&init_net, dev_name);
	if (!dev) {
		spin_unlock_bh(&p->lock);
		return -1;
	}

	pp = ppe_drv_port_from_dev(dev);
	if (!pp) {
		ppe_drv_warn("%p: No PPE port for given netdevice %s\n", p, dev->name);
		dev_put(dev);
		spin_unlock_bh(&p->lock);
		return -1;
	}

	/*
	 * Configure L2_VP port table for upstream port with LOOPBACK service code
	 */
	if (!ppe_drv_port_l2_vp_sc_config(pp, PPE_DRV_SC_LOOPBACK_RING, 0)) {
		ppe_drv_warn("%p: Error in configuring L2 VP table  %s\n", p, dev->name);
		dev_put(dev);
		spin_unlock_bh(&p->lock);
		return -1;
	}

	/*
	 * For default source profile, we need to set the corresponding loopback ring to map to upstream port for host originated packet
	 */
	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_LOOPBACK_RING, ppe_drv_port_ucast_queue_get_by_port(pp->port), PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);

	/*
	 * Re-configure L2_SERVICE_TBL for upstream port
	 */
	ret = ppe_drv_sc_in_service_tbl_dest_port(PPE_DRV_SC_LOOPBACK_RING_NEXT, pp->port);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: service code configuration failed for sc", p);
		return false;
        }

	p->upstream_dev = dev;
	dev_put(dev);

	spin_unlock_bh(&p->lock);

	ppe_drv_trace("%p: upstream_dev configured %s\n", p, dev->name);

	return ret;
}

#ifdef PPE_LOOPBACK_PORT_SUPPORT
/*
 * ppe_drv_lpbk_port_info_ctx_fill()
 *	Fill Loopback info in global structure.
 */
bool ppe_drv_lpbk_port_info_ctx_fill(enum ppe_drv_loopback_port_feature_type ft_type)
{
	struct ppe_drv *p = ppe_drv_gbl;
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	int32_t host_queue = -1;
#endif

	if (!ppe_drv_port_loopback_port_get_info()) {
		ppe_drv_warn("%p: loopback port get info failed.", p);
		return false;
	}
	switch(ft_type) {
		case PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON:
			/*
			 * configure the pon pass information.
			 */
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC;

			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC_NEXT].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC_NEXT].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC_NEXT;

			p->loopback_port_info.ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON;
			ppe_drv_info("port_id: %d base queue: %d\n", p->loopback_port_info.port_id, p->loopback_port_info.ucastq_start);

			/*
			 * configure the service code.
			 */
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC].sc, p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC_NEXT].sc,
					p->loopback_port_info.port_id);
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC_NEXT].sc, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);

			/*
			 * configure the queue to service code mapping.
			 */
			ppe_drv_sc_ucast_queue_set(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC].sc, p->loopback_port_info.ucastq_start,
					PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);

			break;
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
		case PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US:
			/*
			 * configure the pon pass information.
			 */
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC;

			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT;

			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT_DDRQ].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT_DDRQ].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT_DDRQ;

			p->loopback_port_info.ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US;
			ppe_drv_info("port_id: %d base queue: %d\n", p->loopback_port_info.port_id, p->loopback_port_info.ucastq_start);

			/*
			 * configure the service code.
			 */
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC].sc, p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT].sc,
					p->loopback_port_info.port_id);
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT].sc, p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT_DDRQ].sc, PPE_DRV_PORT_CPU);
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT_DDRQ].sc, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);

			break;

		case PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS:
			/*
			 * Get the queue base for host queues (CPU port 0).
			 */
			host_queue = ppe_drv_port_ucast_queue_get_by_port(PPE_DRV_PORT_CPU);
			if (host_queue < 0) {
				ppe_drv_warn("Failed to get host queue for CPU port\n");
				return false;
			}

			/*
			 * configure the pon pass information.
			 */
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC;

			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC_NEXT].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS;
			p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC_NEXT].sc = PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC_NEXT;

			p->loopback_port_info.ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS;
			ppe_drv_info("port_id: %d base queue: %d\n", p->loopback_port_info.port_id, p->loopback_port_info.ucastq_start);

			/*
			 * configure the service code.
			 */
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC].sc, p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC_NEXT].sc,
					p->loopback_port_info.port_id);
			ppe_drv_sc_config(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC_NEXT].sc, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);

			/*
			 * configure the queue to service code mapping.
			 */
			ppe_drv_sc_ucast_queue_set(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC_NEXT].sc, host_queue, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_PORT_SRC_PROFILE);
			break;
#endif
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
		case PPE_DRV_LOOPBACK_PORT_FT_TYPE_HAIRPIN_NAT:
			/*
			 * Reconfigure NPT66 hairpin NAT service codes to use loopback PORT
			 * (instead of loopback ring):
			 *   First-pass  (NPT66_HAIRPIN_NAT)      → loopback port
			 *   Second-pass (NPT66_HAIRPIN_NAT_NEXT) → CPU (bypass all)
			 */
			p->loopback_port_info.ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_HAIRPIN_NAT;
			ppe_drv_info("Hairpin NAT loopback port: port_id=%d base_queue=%d\n",
							p->loopback_port_info.port_id, p->loopback_port_info.ucastq_start);
			ppe_drv_sc_config(PPE_DRV_SC_NPT66_HAIRPIN_NAT, PPE_DRV_SC_NPT66_HAIRPIN_NAT_NEXT, p->loopback_port_info.port_id);
			ppe_drv_sc_config(PPE_DRV_SC_NPT66_HAIRPIN_NAT_NEXT, PPE_DRV_SC_BYPASS_ALL, PPE_DRV_PORT_CPU);

			/* Map NPT66_HAIRPIN_NAT SC to loopback port queue */
			ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NPT66_HAIRPIN_NAT, p->loopback_port_info.ucastq_start,
									PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
			break;
#endif
		default:
			ppe_drv_warn("invalid feature type %u\n", ft_type);
			break;
	}
	return true;
}

/*
 * ppe_drv_l2vp_sc_add()
 *	Add the sc configs in L2_VP port tbl.
 */
bool ppe_drv_l2vp_sc_add(uint32_t dest_port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = NULL;

	/*
	 * Configure lpbk_port.
	 */
	if (!ppe_drv_lpbk_port_info_ctx_fill(PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON)) {
		ppe_drv_warn("%p: Error in configuring loopback port info.\n", p);
		return false;
	}

	pp = ppe_drv_port_from_port_num(dest_port_id);
	if (!pp) {
		ppe_drv_warn("%p: No PPE port for given netdevice %d\n", p, dest_port_id);
		return false;
	}

	/*
	 * Configure L2_VP port table for UNI port with LOOPBACK service code
	 */
	if (!ppe_drv_port_l2_vp_sc_config(pp, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC, p->loopback_port_info.port_id)) {
		ppe_drv_warn("%p: Error in configuring L2 VP table  %d\n", p, dest_port_id);
		return false;
	}

	ppe_drv_trace("the L2 service table is configured\n");
	return true;
}

/*
 * ppe_drv_l2vp_sc_rmv()
 *	Remove the sc configs in L2_VP port tbl.
 */
bool ppe_drv_l2vp_sc_rmv(uint32_t dest_port_id)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp = NULL;

	pp = ppe_drv_port_from_port_num(dest_port_id);
	if (!pp) {
		ppe_drv_warn("%p: No PPE port for given netdevice %d\n", p, dest_port_id);
		return false;
	}

	/*
	 * Reset L2_VP port table for UNI port with LOOPBACK service code
	 */
	if (!ppe_drv_port_l2_vp_sc_reset(pp)) {
		ppe_drv_warn("%p: Error in reset L2 VP table  %d\n", p, dest_port_id);
		return false;
	}

	return true;
}
#endif

/*
 * ppe_drv_l2vp_mirror_sc_add()
 *      Add the mirror sc configs in L2_VP port tbl.
 */
bool ppe_drv_l2vp_mirror_sc_add(uint32_t dest_port_id)
{
	struct ppe_drv_port *pp = NULL;
	ppe_drv_sc_t sc;

	pp = ppe_drv_port_from_port_num(dest_port_id);
	if (!pp) {
		ppe_drv_warn("No PPE port for given netdevice %u\n", dest_port_id);
		return false;
	}

	sc = ppe_drv_port_is_gem((uint16_t)dest_port_id)
		? PPE_DRV_SC_DDRQ_PON_PT_MODE
		: PPE_DRV_SC_DDRQ_ETH_PT_MODE;

	/*
	 * Configure L2_VP port table for UNI port with mirror service code
	 */
	if (!ppe_drv_port_l2_vp_mirror_sc_config(pp, sc, dest_port_id)) {
		ppe_drv_warn("Failed to configure L2 VP mirror for port %u\n",
				dest_port_id);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(ppe_drv_l2vp_mirror_sc_add);

/*
 * ppe_drv_l2vp_mirror_sc_rmv()
 *      Remove the mirror sc configs in L2_VP port tbl.
 */
bool ppe_drv_l2vp_mirror_sc_rmv(uint32_t dest_port_id)
{
	struct ppe_drv_port *pp = NULL;

	pp = ppe_drv_port_from_port_num(dest_port_id);
	if (!pp) {
		ppe_drv_warn("No PPE port for given netdevice %u\n", dest_port_id);
		return false;
	}

	/*
	 * Configure L2_VP port table for UNI port with mirror service code
	 */
	if (!ppe_drv_port_l2_vp_mirror_sc_reset(pp)) {
		ppe_drv_warn("Failed to reset L2 VP mirror for port %u\n",
				dest_port_id);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(ppe_drv_l2vp_mirror_sc_rmv);

/*
 * ppe_drv_src2uni_handler()
 *      Set Source Profile to UNI port mapping.
 */
static int ppe_drv_src2uni_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *pp_local;
	fal_ucast_queue_dest_t q_dst = {0};
	struct ppe_drv_port *pp;
	struct net_device *net_dev;
	struct ppe_drv_port *up;
	int ret, profile;
	char *map_name;
	fal_port_t src_port;
	a_uint32_t src_profile = 0;
	char dev[IFNAMSIZ];
	char *start_ch_ptr = NULL;
	char *end_ch_ptr = NULL;
	sw_error_t sw_err = SW_OK;
	int len, i, port_id = 0;
	uint32_t bitmap[8] = {0};

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

#if defined(PPE_LOOPBACK_RING_SUPPORT)
	/*
	 * Check if loopback ring is enabled
	 */
	if (!p->loopback_ring_info.enabled) {
		ppe_drv_warn("Loopback ring is not enabled\n");
		return -1;
	}
#else
	/*
	 * TO-DO: enable this support with loopback feature
	 */
	ppe_drv_warn("loopback feature support not enabled for upstream port\n");
	return -1;
#endif

	/*
	 * This call expect upstream device to be set
	 */
	if (!p->upstream_dev) {
		ppe_drv_warn("Unable to find source upstream dev\n");
		return -1;
	}

	spin_lock_bh(&p->lock);
	up = ppe_drv_port_from_dev(p->upstream_dev);
	if (!up) {
		ppe_drv_warn("Unable to find source upstream port\n");
		spin_unlock_bh(&p->lock);
		return -1;
	}

	map_name = src2uni_map;

	/*
	 * Parse the input here; input can be of two types:
	 *
	 * Case1: Setup src_profile to device mapping
	 *        echo '<src_profile> <net_device>' > /proc/sys/ppe/ppe_drv/src2uni_map
	 *
	 * Case2: Clear source profile to UNI mapping
	 *        echo '<src_profile> update' > /proc/sys/ppe/ppe_drv/src2uni_map
	 */
	start_ch_ptr = map_name + strspn(map_name, NSS_PPE_DRV_WHITESPACE);
	for (i = 0; *start_ch_ptr; start_ch_ptr = end_ch_ptr + strspn(end_ch_ptr, NSS_PPE_DRV_WHITESPACE), i++) {
		if (i == 0) {
			/*
			 * Process src profile number
			 */
			end_ch_ptr = start_ch_ptr + strcspn(start_ch_ptr, NSS_PPE_DRV_WHITESPACE);
			if (end_ch_ptr != start_ch_ptr) {
				len = end_ch_ptr - start_ch_ptr;
			} else {
				len = strlen(start_ch_ptr);
			}

			src_profile = *start_ch_ptr - '0';
			continue;
		}

		if (src_profile >= PPE_DRV_PORT_SRC_PROFILE_MAX) {
			ppe_drv_warn("Invalid src_profile number given from omci\n");
			spin_unlock_bh(&p->lock);
			return -1;
		}

		end_ch_ptr = start_ch_ptr + strcspn(start_ch_ptr, NSS_PPE_DRV_WHITESPACE);
		if (end_ch_ptr != start_ch_ptr) {
			len = end_ch_ptr - start_ch_ptr;
		} else {
			len = strlen(start_ch_ptr);
		}

		if (len <= IFNAMSIZ) {
			memcpy(dev, start_ch_ptr, len);
		}

		dev[len] = '\0';

		if (!strcmp(dev, "flush")) {
			for (port_id = 1; port_id < PPE_DRV_PORTS_MAX; port_id++) {
				pp_local = &p->port[port_id];
				if (pp_local && (pp_local->src_profile == src_profile)) {
					sw_err = fal_qm_port_source_profile_set(PPE_DRV_SWITCH_ID, port_id, PPE_DRV_PORT_SRC_PROFILE);
					if (sw_err != SW_OK) {
						ppe_drv_warn("Unable to reset source profile for port\n");
						spin_unlock_bh(&p->lock);
						return -1;
					}

					pp_local->src_profile = PPE_DRV_PORT_SRC_PROFILE;
				}
			}

			/*
			 * Profile 0 is reserved for all default use cases like packet from host/WLAN and downlink traffic.
			 * Hence, running the below loop from source profile 1
			 */
			for (profile = 1; profile <  PPE_DRV_PORT_SRC_PROFILE_MAX; profile++) {
				uint32_t queue_id;
				if (PPE_DRV_PROF2PORT_IS_EMPTY(p->prof2portmap[profile]))
					continue;

				/*
				 * Run the loop for Physical ports except for port0 and port7
				 */
				for (i = 1; i < PPE_DRV_PORTS_MAX; i++) {
					pp_local = &p->port[i];

					if (!pp_local)
						continue;

					if (!kref_read(&pp_local->ref_cnt))
						continue;

					if (pp_local->port == up->port)
						continue;

					if (pp_local->port == PPE_DRV_PORT_EIP197)
						continue;

					if (!((pp_local->type == PPE_DRV_PORT_PHYSICAL) ||
					      ((pp_local->type == PPE_DRV_PORT_VIRTUAL) && dsa_slave_dev_check(pp_local->dev))))
						continue;

					q_dst.src_profile = src_profile;
					q_dst.dst_port = pp_local->port;

					queue_id = ppe_drv_port_ucast_queue_get_by_port(pp_local->port);
					sw_err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &q_dst, queue_id, pp_local->profile_id);
					if (sw_err != SW_OK) {
						ppe_drv_warn("%p unable to set port queue base ID", p);
						continue;
					}
				}

				PPE_DRV_PROF2PORT_CLEAR_ALL(p->prof2portmap[profile]);
			}

			spin_unlock_bh(&p->lock);
			return ret;
		}

		net_dev = dev_get_by_name(&init_net, dev);
		if (!net_dev) {
			ppe_drv_warn("%p: No valid netdevice found for dev: %s\n", net_dev, dev);
			spin_unlock_bh(&p->lock);
			return -1;
		}

		pp = ppe_drv_port_from_dev(net_dev);
		if (!pp) {
			ppe_drv_warn("%p: No PPE port for given netdevice\n", dev);
			dev_put(net_dev);
			spin_unlock_bh(&p->lock);
			return -1;
		}

		src_port = pp->port;
		pp->src_profile = src_profile;
		sw_err = fal_qm_port_source_profile_set(PPE_DRV_SWITCH_ID, src_port, src_profile);
		if (sw_err != SW_OK) {
			ppe_drv_warn("Unable to configure source profile\n");
			dev_put(net_dev);
			spin_unlock_bh(&p->lock);
			return -1;
		}

		/*
		 * Bitmap of grouped ports
		 */
		PPE_DRV_PROF2PORT_SET(bitmap, pp->port);
		PPE_DRV_PROF2PORT_SET(p->prof2portmap[src_profile], pp->port);

		dev_put(net_dev);
	}

	ppe_drv_loopback_sc2queue_mapping(p, src_profile);

	/*
	 * For port isolation, other port not mapped to current source profile need to be mapped to upstream port queue
	 */
	for (i = 1; i < PPE_DRV_PORTS_MAX; i++) {
		pp_local = &p->port[i];

		if (!kref_read(&pp_local->ref_cnt))
			continue;

		if (PPE_DRV_PROF2PORT_TEST(bitmap, pp_local->port))
			continue;

		if (pp_local->port == up->port)
			continue;

		if (!((pp_local->type == PPE_DRV_PORT_PHYSICAL) ||
		      ((pp_local->type == PPE_DRV_PORT_VIRTUAL) && dsa_slave_dev_check(pp_local->dev))))
			continue;

		q_dst.src_profile = src_profile;
		q_dst.dst_port = pp_local->port;

		sw_err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &q_dst, ppe_drv_port_ucast_queue_get_by_port(up->port), pp_local->profile_id);
		if (sw_err != SW_OK) {
			ppe_drv_warn("Unable to configure destination source profile\n");
		}
	}

	spin_unlock_bh(&p->lock);

	return ret;
}

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
/*
 * ppe_drv_ddr_buffer_resolve_dev()
 *	Resolve a user-supplied net_device to its underlying physical device,
 *	Must be called with p->lock held.
 */
static int ppe_drv_ddr_buffer_resolve_dev(struct ppe_drv *p,
		struct net_device **dev,
		uint16_t *vlan_id,
		uint8_t *vlan_cnt,
		struct ppe_drv_lpbk_buffer_map_entry *map_entry,
		bool is_ingress)
{
	struct net_device *cur = *dev;
	const char *dev_name = cur->name;

	/*
	 * PPPoE
	 */
	if (cur->type == ARPHRD_PPP) {
		struct ppe_drv_iface *pppoe_iface;
		struct ppe_drv_iface *base_iface;
		struct ppe_drv_l3_if *l3_if;
		struct ppe_drv_pppoe *pppoe;

		pppoe_iface = ppe_drv_iface_get_by_dev_internal(cur);
		if (!pppoe_iface || pppoe_iface->type != PPE_DRV_IFACE_TYPE_PPPOE) {
			dev_put(cur);
			pr_err("%p: lpbk_buffer_map: %s: PPPoE session not registered in PPE\n",
					p, dev_name);
			return -ENODEV;
		}

		base_iface = ppe_drv_iface_base_get(pppoe_iface);
		if (!base_iface || !base_iface->dev) {
			dev_put(cur);
			pr_err("%p: lpbk_buffer_map: %s: no base device for PPPoE interface\n",
					p, dev_name);
			return -ENODEV;
		}

		l3_if = ppe_drv_iface_l3_if_get(pppoe_iface);
		pppoe = l3_if ? ppe_drv_l3_if_pppoe_get(l3_if) : NULL;
		if (!pppoe) {
			dev_put(cur);
			pr_err("%p: lpbk_buffer_map: %s: cannot read PPPoE session info\n",
					p, dev_name);
			return -ENODEV;
		}

		if (is_ingress)
			map_entry->ingress_pppoe = true;
		else
			map_entry->egress_pppoe = true;

		map_entry->pppoe_session_id = pppoe->session_id;
		memcpy(map_entry->pppoe_server_mac, pppoe->server_mac, ETH_ALEN);

		dev_hold(base_iface->dev);
		dev_put(cur);
		cur = base_iface->dev;
	}

	/*
	 * VLAN
	 */
	if (is_vlan_dev(cur)) {
		struct net_device *real_dev = vlan_dev_priv(cur)->real_dev;
		uint16_t first_vlan_id = vlan_dev_vlan_id(cur);
		dev_hold(real_dev);
		dev_put(cur);
		cur = real_dev;

		if (is_vlan_dev(cur)) {

			/*
			 * Q-in-Q
			 */
			real_dev = vlan_dev_priv(cur)->real_dev;
			vlan_id[0] = vlan_dev_vlan_id(cur);   /* outer */
			vlan_id[1] = first_vlan_id;            /* inner */
			*vlan_cnt = 2;
			dev_hold(real_dev);
			dev_put(cur);
			cur = real_dev;
		} else {
			/*
			 * Single VLAN
			 */
			vlan_id[0] = first_vlan_id;
			*vlan_cnt = 1;
		}
	}

	*dev = cur;
	return 0;
}

/*
 * ppe_drv_lpbk_buffer_map_entry_match()
 * 	Return true if both entries describe the same resolved map rule.
 */
static inline bool ppe_drv_lpbk_buffer_map_entry_match(struct ppe_drv_lpbk_buffer_map_entry *entry, struct ppe_drv_lpbk_buffer_map_entry *key)
{
	if (entry->ingress_dev != key->ingress_dev ||
			entry->egress_dev != key->egress_dev ||
			entry->ingress_vlan_cnt != key->ingress_vlan_cnt ||
			entry->egress_vlan_cnt != key->egress_vlan_cnt ||
			entry->ingress_pppoe != key->ingress_pppoe ||
			entry->egress_pppoe != key->egress_pppoe) {
		return false;
	}

	if (memcmp(entry->ingress_vlan_id, key->ingress_vlan_id, sizeof(key->ingress_vlan_id)) ||
			memcmp(entry->egress_vlan_id, key->egress_vlan_id, sizeof(key->egress_vlan_id))) {
		return false;
	}

	if (!entry->ingress_pppoe && !entry->egress_pppoe) {
		return true;
	}

	return entry->pppoe_session_id == key->pppoe_session_id &&
		!memcmp(entry->pppoe_server_mac, key->pppoe_server_mac, ETH_ALEN);
}

/*
 * ppe_drv_lpbk_buffer_map_rule_add()
 * 	Add entry in lpbk_buffer_map.
 */
static inline int ppe_drv_lpbk_buffer_map_rule_add(struct net_device *ingress_dev, struct net_device *egress_dev) {
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_lpbk_buffer_map_entry *map_entry, *entry;
	int ret;

	spin_lock_bh(&p->lock);

	if (p->lpbk_buffer_map_count >= PPE_DRV_LPBK_BUFFER_MAP_MAX) {
		dev_put(ingress_dev);
		dev_put(egress_dev);
		spin_unlock_bh(&p->lock);
		pr_err("%p: lpbk_buffer_map: max entries (%d) reached\n",
				p, PPE_DRV_LPBK_BUFFER_MAP_MAX);
		return -ENOSPC;
	}

	map_entry = kzalloc(sizeof(*map_entry), GFP_ATOMIC);
	if (!map_entry) {
		dev_put(ingress_dev);
		dev_put(egress_dev);
		spin_unlock_bh(&p->lock);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&map_entry->list);
	strlcpy(map_entry->ingress_name, ingress_dev->name, IFNAMSIZ);
	strlcpy(map_entry->egress_name, egress_dev->name, IFNAMSIZ);

	ret = ppe_drv_ddr_buffer_resolve_dev(p, &ingress_dev,
			map_entry->ingress_vlan_id, &map_entry->ingress_vlan_cnt,
			map_entry, true);

	if (ret) {
		dev_put(egress_dev);
		kfree(map_entry);
		spin_unlock_bh(&p->lock);
		return ret;
	}

	map_entry->ingress_dev = ingress_dev;


	ret = ppe_drv_ddr_buffer_resolve_dev(p, &egress_dev,
			map_entry->egress_vlan_id, &map_entry->egress_vlan_cnt,
			map_entry, false);

	if (ret) {
		dev_put(ingress_dev);
		kfree(map_entry);
		spin_unlock_bh(&p->lock);
		return ret;
	}

	map_entry->egress_dev = egress_dev;

	/*
	 * Reject duplicates.
	 */
	list_for_each_entry(entry, &p->lpbk_buffer_map, list) {
		if (ppe_drv_lpbk_buffer_map_entry_match(entry, map_entry)) {
			pr_err("%p: lpbk_buffer_map: pair %s->%s already exists\n",
					p, map_entry->ingress_name, map_entry->egress_name);
			dev_put(map_entry->ingress_dev);
			dev_put(map_entry->egress_dev);
			kfree(map_entry);
			spin_unlock_bh(&p->lock);
			return -EEXIST;
		}
	}

	list_add_tail(&map_entry->list, &p->lpbk_buffer_map);
	p->lpbk_buffer_map_count++;

	ppe_drv_info("%p: lpbk_buffer_map: pair added %s->%s (count=%u)\n",
			p, ingress_dev->name, egress_dev->name, p->lpbk_buffer_map_count);

	spin_unlock_bh(&p->lock);

	return ret;
}

/*
 * ppe_drv_lpbk_buffer_map_flush_pair()
 * 	delete a specific rule from lpbk_buffer_map
 */
static inline int ppe_drv_lpbk_buffer_map_flush_pair(struct net_device *ingress_dev, struct net_device *egress_dev) {
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_lpbk_buffer_map_entry *map_entry, *tmp;
	struct ppe_drv_lpbk_buffer_map_entry key = {0};
	bool found = false;
	int ret;

	spin_lock_bh(&p->lock);

	ret = ppe_drv_ddr_buffer_resolve_dev(p, &ingress_dev,
			key.ingress_vlan_id, &key.ingress_vlan_cnt, &key, true);
	if (ret) {
		dev_put(egress_dev);
		spin_unlock_bh(&p->lock);
		return ret;
	}

	ret = ppe_drv_ddr_buffer_resolve_dev(p, &egress_dev,
			key.egress_vlan_id, &key.egress_vlan_cnt, &key, false);
	if (ret) {
		dev_put(ingress_dev);
		spin_unlock_bh(&p->lock);
		return ret;
	}

	key.ingress_dev = ingress_dev;
	key.egress_dev = egress_dev;

	list_for_each_entry_safe(map_entry, tmp, &p->lpbk_buffer_map, list) {
		if (ppe_drv_lpbk_buffer_map_entry_match(map_entry, &key)) {
			list_del(&map_entry->list);
			dev_put(map_entry->ingress_dev);
			dev_put(map_entry->egress_dev);
			kfree(map_entry);
			p->lpbk_buffer_map_count--;
			found = true;
			break;
		}
	}

	dev_put(ingress_dev);
	dev_put(egress_dev);
	spin_unlock_bh(&p->lock);

	if (!found) {
		ppe_drv_warn("%p: lpbk_buffer_map: pair not found for flush\n", p);
		return -ENOENT;
	}

	ppe_drv_info("%p: lpbk_buffer_map: pair removed (count=%u)\n",
			p, p->lpbk_buffer_map_count);
	return 0;
}

/*
 * ppe_drv_lpbk_buffer_map_delete()
 * 	delete lpbk_buffer_map
 */
static inline void ppe_drv_lpbk_buffer_map_delete(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_lpbk_buffer_map_entry *map_entry, *tmp;

	spin_lock_bh(&p->lock);

	list_for_each_entry_safe(map_entry, tmp, &p->lpbk_buffer_map, list) {
		list_del(&map_entry->list);
		dev_put(map_entry->ingress_dev);
		dev_put(map_entry->egress_dev);
		kfree(map_entry);
		p->lpbk_buffer_map_count--;
	}

	spin_unlock_bh(&p->lock);

	return;
}

/*
 * ppe_drv_lpbk_buffer_map_handler()
 * 	sysctl handler for the pairwise DDR-buffer ingress→egress map.
 * 	echo '<ingress_dev> <egress_dev>' : Add a pair
 * 	echo 'flush' : Remove all the entries from the map.
 * 	echo 'flush <ingress_dev> <egress_dev>' : Removes one specific pair
 */
static int ppe_drv_lpbk_buffer_map_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	char *buf;
	char ingress_name[IFNAMSIZ];
	char egress_name[IFNAMSIZ];
	struct ppe_drv *p = ppe_drv_gbl;
	struct net_device *ingress_dev;
	struct net_device *egress_dev;
	size_t len;
	int cmd_mode = PPE_DRV_LPBK_MAP_CMD_INVALID;

	if (!write) {
		if (*ppos == 0) {
			struct ppe_drv_lpbk_buffer_map_entry *map_entry;
			int off = 0;

			spin_lock_bh(&p->lock);
			off += scnprintf(lpbk_buffer_map_str + off,
					sizeof(lpbk_buffer_map_str) - off,
					"Total rules: %u\n", p->lpbk_buffer_map_count);
			list_for_each_entry(map_entry, &p->lpbk_buffer_map, list) {
				off += scnprintf(lpbk_buffer_map_str + off,
						sizeof(lpbk_buffer_map_str) - off,
						"%s %s\n",
						map_entry->ingress_name, map_entry->egress_name);
			}
			spin_unlock_bh(&p->lock);
		}

		return proc_dostring(table, write, buffer, lenp, ppos);
	}

	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret) {
		return ret;
	}

	if (!p->loopback_ring_info.enabled ||
			!(p->loopback_ring_info.ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_DDR_BUFFER)) {
		pr_err("DDR buffer loopback ring not enabled (ft_type=0x%x)\n",
				p->loopback_ring_info.ft_type);
		return -EINVAL;
	}

	buf = lpbk_buffer_map_str;
	size_t slen = strlen(buf);
	while (slen > 0 && (buf[slen - 1] == '\n' || buf[slen - 1] == ' '
				|| buf[slen - 1] == '\t'))
		buf[--slen] = '\0';

	buf += strspn(buf, NSS_PPE_DRV_WHITESPACE);

	/*
	 * flush
	 */
	if (strncmp(buf, "flush", 5) == 0) {
		char *after_flush = buf + 5;
		after_flush += strspn(after_flush, NSS_PPE_DRV_WHITESPACE);

		/*
		 * flush all
		 */
		if (*after_flush == '\0') {
			cmd_mode = PPE_DRV_LPBK_MAP_CMD_FLUSH_ALL;
			goto action;
		} else {

			/*
			 * flush pair
			 */
			cmd_mode = PPE_DRV_LPBK_MAP_CMD_FLUSH_PAIR;
			len = strcspn(after_flush, NSS_PPE_DRV_WHITESPACE);
			if (len == 0 || len >= IFNAMSIZ) {
				pr_err("%p: lpbk_buffer_map: invalid ingress device name\n", p);
				return -EINVAL;
			}

			memcpy(ingress_name, after_flush, len);
			ingress_name[len] = '\0';
			after_flush += len;
			after_flush += strspn(after_flush, NSS_PPE_DRV_WHITESPACE);
			len = strcspn(after_flush, NSS_PPE_DRV_WHITESPACE);
			if (len == 0 || len >= IFNAMSIZ) {
				pr_err("%p: lpbk_buffer_map: invalid egress device name\n", p);
				return -EINVAL;
			}

			memcpy(egress_name, after_flush, len);
			egress_name[len] = '\0';
		}
	} else {
		len = strcspn(buf, NSS_PPE_DRV_WHITESPACE);
		if (len == 0 || len >= IFNAMSIZ) {
			pr_err("%p: lpbk_buffer_map: invalid ingress device name\n", p);
			return -EINVAL;
		}

		memcpy(ingress_name, buf, len);
		ingress_name[len] = '\0';
		buf += len;
		buf += strspn(buf, NSS_PPE_DRV_WHITESPACE);
		len = strcspn(buf, NSS_PPE_DRV_WHITESPACE);
		if (len == 0 || len >= IFNAMSIZ) {
			pr_err("%p: lpbk_buffer_map: invalid egress device name\n", p);
			return -EINVAL;
		}

		cmd_mode = PPE_DRV_LPBK_MAP_CMD_RULE_ADD;
		memcpy(egress_name, buf, len);
		egress_name[len] = '\0';
	}

	/*
	 * get the ingress netdev
	 */
	ingress_dev = dev_get_by_name(&init_net, ingress_name);
	if (!ingress_dev) {
		pr_err("%p: lpbk_buffer_map: no netdevice found for ingress: %s\n",
				p, ingress_name);
		return -ENODEV;
	}

	/*
	 * get the egress netdev
	 */
	egress_dev = dev_get_by_name(&init_net, egress_name);
	if (!egress_dev) {
		dev_put(ingress_dev);
		pr_err("%p: lpbk_buffer_map: no netdevice found for egress: %s\n",
				p, egress_name);
		return -ENODEV;
	}

action:
	switch (cmd_mode) {
		case PPE_DRV_LPBK_MAP_CMD_RULE_ADD:
			return ppe_drv_lpbk_buffer_map_rule_add(ingress_dev, egress_dev);

		case PPE_DRV_LPBK_MAP_CMD_FLUSH_PAIR:
			return ppe_drv_lpbk_buffer_map_flush_pair(ingress_dev, egress_dev);

		case PPE_DRV_LPBK_MAP_CMD_FLUSH_ALL:
			ppe_drv_lpbk_buffer_map_delete();
			break;
		default:
			ppe_drv_warn("%p: Invalid command given for lpbk_buffer_map\n", p);
	}

	return 0;
}

/*
 * ppe_drv_ddr_buffer_auto_en_handler()
 * 	sysctl handler to enable/disable automatic DDR buffering.
 * 	Automatic DDR buffering is DISABLED by default.
 * 	Once enabled, PPE will automatically apply DDR buffering service code for flows where the ingress
 * 	phydev speed exceeds the egress phydev speed.
 *
 * 	Usage:
 * 	echo 1 > /proc/sys/ppe/ppe_drv/ddr_buffer_auto_en (enable)
 * 	echo 0 > /proc/sys/ppe/ppe_drv/ddr_buffer_auto_en (disable)
 */
static int ppe_drv_ddr_buffer_auto_en_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	struct ppe_drv *p = ppe_drv_gbl;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	if (ddr_buffer_auto_en != 0 && ddr_buffer_auto_en != 1) {
		pr_err("ddr_buffer_auto_en: invalid value %d, must be 0 or 1\n",
				ddr_buffer_auto_en);
		ddr_buffer_auto_en = 0;
		return -EINVAL;
	}

	spin_lock_bh(&p->lock);
	p->ddr_buffer_auto_en = !!ddr_buffer_auto_en;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%p: ddr_buffer_auto_en set to %d\n", p, ddr_buffer_auto_en);
	return ret;
}
#endif /* PPE_DRV_DDR_BUFFER_SPEED_MISMATCH */

/*
 * ppe_drv_get_and_hold_qdisc_netdev()
 * 	Get qdisc netdev from flow
 */
uint32_t ppe_drv_get_qos_tag(int32_t flow_index) {
	return ppe_drv_flow_get_qos_tag(flow_index);
}
EXPORT_SYMBOL(ppe_drv_get_qos_tag);

/*
 * ppe_drv_get_and_hold_qdisc_netdev()
 * 	Get qdisc netdev from flow
 */
struct net_device *ppe_drv_get_and_hold_qdisc_netdev(int32_t flow_index) {
	return ppe_drv_flow_get_and_hold_qdisc_netdev(flow_index);
}
EXPORT_SYMBOL(ppe_drv_get_and_hold_qdisc_netdev);

/*
 * ppe_drv_get_qdisc_rule_flag()
 * 	Get qdisc flags from flow
 */
int8_t ppe_drv_get_qdisc_rule_flag(int32_t flow_index) {
	return ppe_drv_flow_get_qdisc_rule_flag(flow_index);
}
EXPORT_SYMBOL(ppe_drv_get_qdisc_rule_flag);

/*
 * ppe_drv_sub
 *	PPE DRV sub directory
 */
static struct ctl_table ppe_drv_sub[] = {
#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	{
		.procname	=	"lpbk_buffer_map",
		.data		=	&lpbk_buffer_map_str,
		.maxlen		=	sizeof(char) * PPE_DRV_LPBK_BUFFER_MAP_STR_LEN,
		.mode		=	0644,
		.proc_handler	=	ppe_drv_lpbk_buffer_map_handler
	},
	{
		.procname	=	"ddr_buffer_auto_en",
		.data		=	&ddr_buffer_auto_en,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_ddr_buffer_auto_en_handler
	},
#endif
	{
		.procname	=	"if_bm_to_offload",
		.data		=	&if_bm_to_offload,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_if_bm_to_offload_handler
	},
	{
		.procname	=	"static_dbg_level",
		.data		=	&static_dbg_level_str,
		.maxlen		=	sizeof(char) * PPE_DRV_STATIC_DBG_LEVEL_STR_LEN,
		.mode		=	0644,
		.proc_handler	=	ppe_drv_static_dbg_level_handler
	},
	{
		.procname       =       "disable_port_mtu_check",
		.data           =       &disable_port_mtu_check,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       ppe_drv_disable_port_mtu_check_handler
	},
	{
		.procname       =       "eth2eth_offload_if_bitmap",
		.data           =       &eth2eth_offload_if_bitmap,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       ppe_drv_eth2eth_offload_if_bitmap_handler
	},
	{
		.procname       =       "upstream_dev",
		.data           =       &upstream_dev_str,
		.maxlen         =       sizeof(char) * PPE_DRV_UPSTREAM_DEV_LEVEL_STR_LEN,
		.mode           =       0644,
		.proc_handler   =       ppe_drv_upstream_dev_handler
	},
	{

		.procname       =       "src2uni_map",
		.data           =       &src2uni_map,
		.maxlen         =       sizeof(char) * PPE_DRV_SRC2UNI_LEVEL_STR_LEN,
		.mode           =       0644,
		.proc_handler   =       ppe_drv_src2uni_handler
	},
#ifdef PPE_DRV_PKT_PADDING_STRIP
	{
		.procname       =       "packet_padding",
		.data           =       &packet_padding,
		.maxlen         =       sizeof(char) * PPE_DRV_PACKET_PADDING_STR_LEN,
		.mode           =       0644,
		.proc_handler   =       ppe_drv_pkt_padding_handler
	},
#endif
	{
		.procname	=	"l4_checksum_exception_enable",
		.data		=	&l4_checksum_exception_enable,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_l4_checksum_exception_enable
	},
	{
		.procname	=	"mac_lrn_exception_en",
		.data		=	&mac_lrn_exception_en,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_mac_lrn_exception_en
	},

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
	{
		.procname	=	"loopback_port_ft_type",
		.data		=	&loopback_port_ft_type,
		.maxlen		=	sizeof(uint32_t),
		.mode		=	0644,
		.proc_handler   =	ppe_drv_loopback_port_feature_write_handler,
	},
#endif

#ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
	{
		.procname	=	"ppe_drv_ipsec_passth_en",
		.data		=	&ppe_drv_ipsec_passth_en,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_ipsec_pass_through_en_handler
	},
#endif

#ifdef NSS_PPE_SIXTH_TUPLE
	{
		.procname	=	"ppe_drv_sixth_tuple_en",
		.data		=	&sixth_tuple_mode,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_sixth_tuple_mode_handler
	},
#endif
	{
		.procname	=	"wlan_host_assist_qdisc_en",
		.data		=	&wlan_host_assist_qdisc_en,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_wlan_host_assist_qdisc_en_handler
	},

	{
		.procname	=	"unknown_dst_rfs_enable",
		.data		=	&unknown_dst_rfs_enable,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_unknown_dst_rfs_enable_handler
	},

#ifdef NSS_PPE_PON_SUPPORT
	{
		.procname	=	"dot1p_dest_vp_en",
		.data		=	&dot1p_dest_vp_en,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	ppe_drv_dot1p_dest_vp_en_handler
	},
#endif

	{}
};

/*
 * ppe_drv_mht_port_from_fdb()
 *	Get the port id corresponding to the destination
 *	mac address and vid
 */
int32_t ppe_drv_mht_port_from_fdb(uint8_t *dmac, uint16_t vid)
{
	fal_fdb_entry_t entry = {0};
	sw_error_t err = SW_OK;
	entry.fid = vid;
	entry.port.id = 0;
	entry.type = SW_ENTRY;

	memcpy(&entry.addr, dmac, ETH_ALEN);

	err = fal_fdb_entry_search(PPE_DRV_MHT_SWITCH_ID, &entry);
	if (err != SW_OK) {
		return -1;
	}

	if (entry.port.id > 0) {
		return ffs(entry.port.id) - 1;
	}

	return -1;
}
EXPORT_SYMBOL(ppe_drv_mht_port_from_fdb);

/*
 * ppe_drv_passive_vp_enable_handler()
 *	Handler function to set value of passive_vp_enable.
 */
static int ppe_drv_passive_vp_enable_handler(const char *val, const struct kernel_param *kp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int res = param_set_bool(val, kp);

	spin_lock_bh(&p->lock);
	p->rfs.passive_vp_enable = passive_vp_enable;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Passive VP is %s.\n", (passive_vp_enable ? "enabled" : "disabled"));

	return res;
}

static const struct kernel_param_ops passive_vp_enable_ops = {
	.set = ppe_drv_passive_vp_enable_handler,
	.get = param_get_bool,
};

module_param_cb(passive_vp_enable, &passive_vp_enable_ops, &passive_vp_enable, 0644);

/*
 * ppe_drv_eth_coremask_set_handler()
 *	Handler function to set value of eth_coremask.
 */
static int ppe_drv_eth_coremask_set_handler(const char *val, const struct kernel_param *kp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int res = param_set_uint(val, kp);

	if ((eth_coremask < PPE_DRV_RFS_COREMASK_MIN) || (eth_coremask > PPE_DRV_RFS_COREMASK_MAX)) {
		printk("Invalid coremask value, should be between %u to %u. Hence setting to default value : %u \n",
						PPE_DRV_RFS_COREMASK_MIN, PPE_DRV_RFS_COREMASK_MAX, PPE_DRV_RFS_COREMASK_DEFAULT);
		eth_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
		res = 0;
	}

	spin_lock_bh(&p->lock);
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_PHYSICAL] = eth_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_PHYSICAL] = eth_coremask;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Ethernet coremask value is set to : %u\n", eth_coremask);

	return res;
}

static const struct kernel_param_ops eth_coremask_ops = {
    .set = ppe_drv_eth_coremask_set_handler,
    .get = param_get_uint,
};

module_param_cb(eth_coremask, &eth_coremask_ops, &eth_coremask, 0644);

#ifdef NSS_PPE_DRV_HW_GRO
bool ppe_drv_hw_gro_feature_set(struct net_device *dev, bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *port;

	spin_lock_bh(&p->lock);
	port = ppe_drv_port_from_dev(dev);
	if (!port) {
		ppe_drv_warn("PPE port invalid for HW GRO setting for dev:%s", dev->name);
		spin_unlock_bh(&p->lock);
		return false;
	}

	port->hw_gro_en = enable;
	spin_unlock_bh(&p->lock);
	return true;
}
EXPORT_SYMBOL(ppe_drv_hw_gro_feature_set);

/*
 * ppe_drv_eth_gro_coremask_set_handler()
 *	Handler function to set value of eth_gro_coremask.
 */
static int ppe_drv_eth_gro_coremask_set_handler(const char *val, const struct kernel_param *kp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int res = param_set_uint(val, kp);

	if (!p) {
		ppe_drv_warn("PPE driver not initialized\n");
		return -EINVAL;
	}

	if ((eth_gro_coremask < PPE_DRV_GRO_COREMASK_MIN) || (eth_gro_coremask > PPE_DRV_GRO_COREMASK_MAX)) {
		ppe_drv_warn("Invalid coremask value, should be between %u to %u. Hence setting to default value : %u \n",
						PPE_DRV_GRO_COREMASK_MIN, PPE_DRV_GRO_COREMASK_MAX, PPE_DRV_GRO_COREMASK_DEFAULT);
		eth_gro_coremask = PPE_DRV_GRO_COREMASK_DEFAULT;
		res = 0;
	}

	spin_lock_bh(&p->lock);
	p->gro_ctx.gro_info.coremask = eth_gro_coremask;
	p->gro_ctx.gro_info.shadow_coremask = eth_gro_coremask;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Ethernet gro coremask value is set to : %u\n", eth_gro_coremask);

	return res;
}

static const struct kernel_param_ops eth_gro_coremask_ops = {
    .set = ppe_drv_eth_gro_coremask_set_handler,
    .get = param_get_uint,
};

module_param_cb(eth_gro_coremask, &eth_gro_coremask_ops, &eth_gro_coremask, 0644);
#endif

/*
 * ppe_drv_wlan_coremask_set_handler()
 *	Handler function to set value of wlan_coremask.
 */
static int ppe_drv_wlan_coremask_set_handler(const char *val, const struct kernel_param *kp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int res = param_set_uint(val, kp);

	if ((wlan_coremask < PPE_DRV_RFS_COREMASK_MIN) || (wlan_coremask > PPE_DRV_RFS_COREMASK_MAX)) {
		printk("Invalid coremask value, should be between %u to %u. Hence setting to default value : %u \n",
						PPE_DRV_RFS_COREMASK_MIN, PPE_DRV_RFS_COREMASK_MAX, PPE_DRV_RFS_COREMASK_DEFAULT);
		wlan_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
		res = 0;
	}

	spin_lock_bh(&p->lock);
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_WLAN] = wlan_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_WLAN] = wlan_coremask;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("WLAN coremask value is set to : %u\n", wlan_coremask);

	return res;
}

static const struct kernel_param_ops wlan_coremask_ops = {
    .set = ppe_drv_wlan_coremask_set_handler,
    .get = param_get_uint,
};

module_param_cb(wlan_coremask, &wlan_coremask_ops, &wlan_coremask, 0644);

/*
 * ppe_drv_tun_coremask_set_handler()
 *	Handler function to set value of tun_coremask.
 */
static int ppe_drv_tun_coremask_set_handler(const char *val, const struct kernel_param *kp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int res = param_set_uint(val, kp);

	if ((tun_coremask < PPE_DRV_RFS_COREMASK_MIN) || (tun_coremask > PPE_DRV_RFS_COREMASK_MAX)) {
		printk("Invalid coremask value, should be between %u to %u. Hence setting to default value : %u \n",
						PPE_DRV_RFS_COREMASK_MIN, PPE_DRV_RFS_COREMASK_MAX, PPE_DRV_RFS_COREMASK_DEFAULT);
		tun_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
		res = 0;
	}

	spin_lock_bh(&p->lock);
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_TUNNEL] = tun_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_TUNNEL] = tun_coremask;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Tunnel coremask value is set to : %u\n", tun_coremask);

	return res;
}

static const struct kernel_param_ops tun_coremask_ops = {
    .set = ppe_drv_tun_coremask_set_handler,
    .get = param_get_uint,
};

module_param_cb(tun_coremask, &tun_coremask_ops, &tun_coremask, 0644);

/*
 * ppe_drv_mcast_coremask_set_handler()
 *	Handler function to set value of mcast_coremask.
 */
static int ppe_drv_mcast_coremask_set_handler(const char *val, const struct kernel_param *kp)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int res = param_set_uint(val, kp);

	if ((mcast_coremask < PPE_DRV_RFS_COREMASK_MIN) || (mcast_coremask > PPE_DRV_RFS_COREMASK_MAX)) {
		printk("Invalid coremask value, should be between %u to %u. Hence setting to default value : %u \n",
						PPE_DRV_RFS_COREMASK_MIN, PPE_DRV_RFS_COREMASK_MAX, PPE_DRV_RFS_COREMASK_DEFAULT);
		mcast_coremask = PPE_DRV_RFS_COREMASK_DEFAULT;
		res = 0;
	}

	spin_lock_bh(&p->lock);
	p->rfs.coremask[PPE_DRV_RFS_INTERFACE_TYPE_MCAST] = mcast_coremask;
	p->rfs.shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_MCAST] = mcast_coremask;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("Multicast coremask value is set to : %u\n", mcast_coremask);

	return res;
}

static const struct kernel_param_ops mcast_coremask_ops = {
    .set = ppe_drv_mcast_coremask_set_handler,
    .get = param_get_uint,
};

module_param_cb(mcast_coremask, &mcast_coremask_ops, &mcast_coremask, 0644);

/*
 * ppe_drv_module_init()
 *	module init for ppe driver
 */
static int __init ppe_drv_module_init(void)
{
	int ret, i;
	struct hlist_head *hash_list;

	if (!of_find_compatible_node(NULL, NULL, "qcom,nss-ppe")) {
		ppe_drv_info("PPE device tree node not found\n");
		return -EINVAL;
	}

	get_random_bytes(&ppe_drv_jhash_rnd, sizeof(ppe_drv_jhash_rnd));

	/*
	* Allocate ppe_drv_gbl
	*/
	ppe_drv_gbl = kzalloc(sizeof(struct ppe_drv), GFP_KERNEL);
	if(!ppe_drv_gbl) {
		ppe_drv_warn("Failed to allocate global ppe_drv structure\n");
		return -EINVAL;
	}

	/*
	 * Serial-Number Hash Table
	 * the connections are organzied in a global list, it is
	 * expensive to locate a given connection, it avoid to lookup by
	 * querying from hardware which is also expensive,
	 * the primary use-case is fast lookup in
	 * acceleration/deceleration/sync.
	 */
	hash_list = ppe_drv_gbl->hash_conn_v4;
	for(i = 0; i < ARRAY_SIZE(ppe_drv_gbl->hash_conn_v4); i++) {
		INIT_HLIST_HEAD(&hash_list[i]);
	}

	hash_list = ppe_drv_gbl->hash_conn_v6;
	for(i = 0; i < ARRAY_SIZE(ppe_drv_gbl->hash_conn_v6); i++) {
		INIT_HLIST_HEAD(&hash_list[i]);
	}

	smp_wmb();

	if (platform_driver_register(&ppe_drv_platform)) {
		ppe_drv_warn("unable to register the driver\n");
		kfree(ppe_drv_gbl);
		return -EIO;
	}

	ret = register_netdevice_notifier(&nss_ppe_netdevice);
	if (ret) {
		ppe_drv_warn("Failed to register NETDEV notifier, error=%d\n", ret);
		platform_driver_unregister(&ppe_drv_platform);
		kfree(ppe_drv_gbl);
		return -EINVAL;
	}

	ppe_drv_trace("Ethernet coremask value is set to : %u\n", eth_coremask);

	/*
	 * Register sysctl framework for PPE DRV
	 */
	ppe_drv_gbl->ppe_drv_header = register_sysctl("ppe/ppe_drv", ppe_drv_sub);
	if (!ppe_drv_gbl->ppe_drv_header) {
		ppe_drv_warn("sysctl table configuration failed");
		unregister_netdevice_notifier(&nss_ppe_netdevice);
		platform_driver_unregister(&ppe_drv_platform);
		kfree(ppe_drv_gbl);
		return -EINVAL;
	}

	return 0;
}
module_init(ppe_drv_module_init);

/*
 * ppe_drv_module_exit()
 *	module exit for ppe driver
 */
static void __exit ppe_drv_module_exit(void)
{
	unregister_sysctl_table(ppe_drv_gbl->ppe_drv_header);
	ppe_drv_gbl->ppe_drv_header = NULL;

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	struct ppe_drv_lpbk_buffer_map_entry *map_entry, *tmp;
	spin_lock_bh(&ppe_drv_gbl->lock);
	list_for_each_entry_safe(map_entry, tmp, &ppe_drv_gbl->lpbk_buffer_map, list) {
		list_del(&map_entry->list);
		dev_put(map_entry->ingress_dev);
		dev_put(map_entry->egress_dev);
		kfree(map_entry);
		ppe_drv_gbl->lpbk_buffer_map_count--;
	}
	spin_unlock_bh(&ppe_drv_gbl->lock);
#endif

	unregister_netdevice_notifier(&nss_ppe_netdevice);
	platform_driver_unregister(&ppe_drv_platform);
	nss_ppe_drv_minidump_free(ppe_drv_gbl, "ppe_drv");
	kfree(ppe_drv_gbl);
}
module_exit(ppe_drv_module_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE driver");
