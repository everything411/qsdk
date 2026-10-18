/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/in.h>
#include <net/ipv6.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <fal/fal_flow.h>
#include <fal/fal_qos.h>
#include <linux/phy.h>
#include "ppe_drv.h"
#include "tun/ppe_drv_tun.h"

static struct ppe_drv_flow_table_info *flow_table_info;

static bool ppe_drv_flow_v6_wifi_qos_get(struct ppe_drv_v6_conn_flow *pcf, uint8_t *wifi_qos, bool *wifi_qos_en);
static bool ppe_drv_flow_v4_wifi_qos_get(struct ppe_drv_v4_conn_flow *pcf, uint8_t *wifi_qos, bool *wifi_qos_en);

#if (PPE_DRV_DEBUG_LEVEL == 3)
/*
 * ppe_drv_flow_dump()
 *	Read and dump the flow table entry.
 */
void ppe_drv_flow_dump(struct ppe_drv_flow *pf)
{
	fal_flow_entry_t flow_cfg = {0};
	sw_error_t err;
	uint32_t tree_id;

	flow_cfg.entry_id = pf->index;
	flow_cfg.entry_type = pf->entry_type;
	err = fal_flow_entry_get(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_INDEX, &flow_cfg);
	if (err != SW_OK){
		ppe_drv_warn("%p: failed to get flow entry", pf);
		return;
	}

	ppe_drv_trace("%p: index: %d", pf, flow_cfg.entry_id);
	ppe_drv_trace("%p: entry_type: %d", pf, flow_cfg.entry_type);
	ppe_drv_trace("%p: protocol: %d", pf, flow_cfg.protocol);;
	ppe_drv_trace("%p: age: %d", pf, flow_cfg.age);
	ppe_drv_trace("%p: src_intf_valid: %d", pf, flow_cfg.src_intf_valid);
	ppe_drv_trace("%p: src_intf_index: %d", pf, flow_cfg.src_intf_index);
	ppe_drv_trace("%p: fwd_type: %d", pf, flow_cfg.fwd_type);
	ppe_drv_trace("%p: snat_nexthop: %d", pf, flow_cfg.snat_nexthop);
	ppe_drv_trace("%p: snat_srcport: %d", pf, flow_cfg.snat_srcport);
	ppe_drv_trace("%p: dnat_nexthop: %d", pf, flow_cfg.dnat_nexthop);
	ppe_drv_trace("%p: dnat_dstport: %d", pf, flow_cfg.dnat_dstport);
	ppe_drv_trace("%p: route_nexthop: %d", pf, flow_cfg.route_nexthop);
	ppe_drv_trace("%p: port_valid: %d", pf, flow_cfg.port_valid);
	ppe_drv_trace("%p: route_port: %d", pf, flow_cfg.route_port);
	ppe_drv_trace("%p: bridge_port: %d", pf, flow_cfg.bridge_port);
	ppe_drv_trace("%p: deacclr_en: %d", pf, flow_cfg.deacclr_en);
	ppe_drv_trace("%p: copy_tocpu_en: %d", pf, flow_cfg.copy_tocpu_en);
	ppe_drv_trace("%p: syn_toggle: %d", pf, flow_cfg.syn_toggle);
	ppe_drv_trace("%p: pri_profile: %d", pf, flow_cfg.pri_profile);
	ppe_drv_trace("%p: sevice_code: %d", pf, flow_cfg.sevice_code);
	ppe_drv_trace("%p: src_port: %d", pf, flow_cfg.src_port);
	ppe_drv_trace("%p: dst_port: %d", pf, flow_cfg.dst_port);
	tree_id = flow_cfg.flow_qos.tree_id[0] | (flow_cfg.flow_qos.tree_id[1] << 8) | (flow_cfg.flow_qos.tree_id[2] << 16);
	ppe_drv_trace("%p: tree_id: %d", pf, tree_id);
	ppe_drv_trace("%p: pkt_counter: %d", pf, flow_cfg.pkt_counter);
	ppe_drv_trace("%p: byte_counter: %llu", pf, flow_cfg.byte_counter);
	ppe_drv_trace("%p: pmtu_check_l3: %d", pf, flow_cfg.pmtu_check_l3);
	ppe_drv_trace("%p: pmtu: %d", pf, flow_cfg.pmtu);
	ppe_drv_trace("%p: vpn_id: %d", pf, flow_cfg.vpn_id);
	ppe_drv_trace("%p: vlan_fmt_valid: %d", pf, flow_cfg.vlan_fmt_valid);
	ppe_drv_trace("%p: svlan_fmt: %d", pf, flow_cfg.svlan_fmt);
	ppe_drv_trace("%p: cvlan_fmt: %d", pf, flow_cfg.cvlan_fmt);
	ppe_drv_trace("%p: wifi_qos_en: %d", pf, flow_cfg.flow_qos.qos_valid);
	ppe_drv_trace("%p: wifi_qos: %d", pf, flow_cfg.flow_qos.qos);
	ppe_drv_trace("%p: invalid: %d", pf, flow_cfg.invalid);

	if (pf->type == PPE_DRV_IP_TYPE_V4) {
		ppe_drv_trace("%p: ipv4 address: %pI4", pf, &flow_cfg.flow_ip.ipv4);
	} else {
		ppe_drv_trace("%p: ipv6 address: %pI6", pf, &flow_cfg.flow_ip.ipv6.ul[0]);
	}
}
#else
/*
 * ppe_drv_flow_dump()
 *	Read and dump the flow table entry.
 */
void ppe_drv_flow_dump(struct ppe_drv_flow *pf)
{
}
#endif

/*
 * ppe_drv_flow_valid_set()
 *	Enable ppe flow entry.
 */
bool ppe_drv_flow_valid_set(struct ppe_drv_flow *pf, bool enable)
{
	sw_error_t err;

	err = fal_flow_entry_en_set(PPE_DRV_SWITCH_ID, pf->index, enable);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to set valid bit: %d for flow index: %d", pf, enable, pf->index);
		return false;
	}

	pf->flags |= PPE_DRV_FLOW_VALID;
	ppe_drv_trace("%p: set valid bit: %d for flow index: %d", pf, enable, pf->index);
	return true;
}

/*
 * ppe_drv_flow_stats_clear()
 *	Clears the flow statistics for the given flow index.
 */
void ppe_drv_flow_stats_clear(struct ppe_drv_flow *pf)
{
	sw_error_t err;

	err = fal_flow_counter_cleanup(PPE_DRV_SWITCH_ID, pf->index);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to clear stats for flow at index: %u err: %d",
				pf, pf->index, err);
	}

	ppe_drv_trace("%p: clear stats for flow index: %d", pf, pf->index);
}

/*
 * ppe_drv_flow_sawf_sc_stats_add()
 *	Update stats for given service class.
 */
void ppe_drv_flow_sawf_sc_stats_add(uint8_t service_class, uint32_t delta_pkts, uint32_t delta_bytes)
{
	struct ppe_drv_stats_sawf_sc *sawf_sc_stats;

	if (!PPE_DRV_SERVICE_CLASS_IS_VALID(service_class)) {
		ppe_drv_warn("invalid service class: %u", service_class);
		return;
	}

	sawf_sc_stats = &ppe_drv_gbl->stats.sawf_sc_stats[service_class];

	atomic64_add(delta_pkts, &sawf_sc_stats->rx_packets);
	atomic64_add(delta_bytes, &sawf_sc_stats->rx_bytes);

	ppe_drv_trace("Stats Updated: service class %u : packets %llu : bytes %llu\n",
			service_class, atomic64_read(&sawf_sc_stats->rx_packets), atomic64_read(&sawf_sc_stats->rx_bytes));
}

/*
 * ppe_drv_flow_v6_stats_update()
 *	Updates flow instance's stats counter from PPE flow hit counter.
 */
void ppe_drv_flow_v6_stats_update(struct ppe_drv_v6_conn_flow *pcf)
{
	sw_error_t err;
	uint32_t delta_pkts;
	uint32_t delta_bytes;
	struct ppe_drv_v6_conn_flow *pcr;
	struct ppe_drv_flow *pf = pcf->pf;
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	fal_entry_counter_t flow_cntrs = {0};
	struct ppe_drv_v6_conn *cn = pcf->conn;
	uint8_t service_class;

	if (pcf->tx_port == NULL || pcf->rx_port == NULL) {
		return;
	}

	err = fal_flow_counter_get(PPE_DRV_SWITCH_ID, pf->index, &flow_cntrs);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get stats for flow at index: %u", pf, pf->index);
		return;
	}

	/*
	 * PPE stats are not clear on read, so we need to calculate the delta
	 * between the latest counters and previously read counters.
	 */
	delta_pkts = (flow_cntrs.matched_pkts - pf->pkts + FAL_FLOW_PKT_CNT_MASK + 1) & FAL_FLOW_PKT_CNT_MASK;
	delta_bytes = (flow_cntrs.matched_bytes - pf->bytes + FAL_FLOW_BYTE_CNT_MASK + 1)
		                                              & FAL_FLOW_BYTE_CNT_MASK;

	/*
	 * PPE stats are not updated for tunnel flows which are not completely accelerated in PPE to avoid
	 * updating the stats twice once in PPE and once in Linux host path
	 */
	if (ppe_drv_port_tun_endpoint_and_inactive(pcf->tx_port)
			|| ppe_drv_port_tun_endpoint_and_inactive(pcf->rx_port)) {
		return;
	}


	/*
	 * Update ppe_conn_flow packet and byte counters
	 */
	ppe_drv_v6_conn_flow_rx_stats_add(pcf, delta_pkts, delta_bytes);
	if (ppe_drv_v6_conn_flags_check(cn, PPE_DRV_V6_CONN_FLAG_RETURN_VALID)) {
		pcr = (pcf == &cn->pcf) ? &cn->pcr : &cn->pcf;
		ppe_drv_v6_conn_flow_tx_stats_add(pcr, delta_pkts, delta_bytes);
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
		if (cn->is_hairpin_nat && ((pcf == &cn->pcf_hp) || (pcf == &cn->pcr_hp))) {
			pcr = (pcf == &cn->pcf_hp) ? &cn->pcr_hp : &cn->pcf_hp;
			ppe_drv_v6_conn_flow_tx_stats_add(pcr, delta_pkts, delta_bytes);
		}
#endif
	} else {
		/*
		 * Multicast flows have no counter cme. For this type of flows
		 * ECM expects tx count to be incremented in the same cme.
		 */
		ppe_drv_v6_conn_flow_tx_stats_add(pcf, delta_pkts, delta_bytes);
	}

	pf->pkts = flow_cntrs.matched_pkts;
	pf->bytes = flow_cntrs.matched_bytes;

	/*
	 * Update the stats only if tree_id is configured with SAWF.
	 */
	if (ppe_drv_tree_id_type_get(&pcf->fl_mdata) == PPE_DRV_TREE_ID_TYPE_SAWF) {
		service_class = tree_id_data->info.sawf_metadata.service_class;
		if (PPE_DRV_SERVICE_CLASS_IS_VALID(service_class))
			ppe_drv_flow_sawf_sc_stats_add(service_class, delta_pkts, delta_bytes);
	}
}

/*
 * ppe_drv_flow_v4_stats_update()
 *	Updates flow instance's stats counter from PPE flow hit counter.
 */
void ppe_drv_flow_v4_stats_update(struct ppe_drv_v4_conn_flow *pcf)
{
	sw_error_t err;
	uint32_t delta_pkts;
	uint32_t delta_bytes;
	struct ppe_drv_v4_conn_flow *pcr;
	struct ppe_drv_flow *pf = pcf->pf;
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	fal_entry_counter_t flow_cntrs = {0};
	struct ppe_drv_v4_conn *cn = pcf->conn;
	struct ppe_drv_v6_conn_flow *mapt_pcf_v6, *mapt_pcr_v6;
	struct ppe_drv_v6_conn *mapt_cn_v6;
	uint8_t service_class;

	if (pcf->tx_port == NULL || pcf->rx_port == NULL) {
		return;
	}

	err = fal_flow_counter_get(PPE_DRV_SWITCH_ID, pf->index, &flow_cntrs);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get stats for flow at index: %u", pf, pf->index);
		return;
	}

	/*
	 * PPE stats are not clear on read, so we need to calculate the delta
	 * between the latest counters and previously read counters.
	 */
	delta_pkts = (flow_cntrs.matched_pkts - pf->pkts + FAL_FLOW_PKT_CNT_MASK + 1) & FAL_FLOW_PKT_CNT_MASK;
	delta_bytes = (flow_cntrs.matched_bytes - pf->bytes + FAL_FLOW_BYTE_CNT_MASK + 1)
		                                              & FAL_FLOW_BYTE_CNT_MASK;
	/*
	 * PPE stats are not updated for tunnel flows which are not completely accelerated in PPE to avoid
	 * updating the stats twice once in PPE and once in Linux host path
	 */
	if (ppe_drv_port_tun_endpoint_and_inactive(pcf->tx_port)
			|| ppe_drv_port_tun_endpoint_and_inactive(pcf->rx_port)) {
		return;
	}

	/*
	 * Update ppe_conn_flow packet and byte counters
	 */
	ppe_drv_v4_conn_flow_rx_stats_add(pcf, delta_pkts, delta_bytes);
	if (ppe_drv_v4_conn_flags_check(cn, PPE_DRV_V4_CONN_FLAG_RETURN_VALID)) {
		pcr = (pcf == &cn->pcf) ? &cn->pcr : &cn->pcf;
		ppe_drv_v4_conn_flow_tx_stats_add(pcr, delta_pkts, delta_bytes);
	} else {
		/*
		 * Multicast flows have no counter cme. For this type of flows
		 * ECM expects tx count to be incremented in the same cme.
		 */
		ppe_drv_v4_conn_flow_tx_stats_add(pcf, delta_pkts, delta_bytes);
	}

	/*
	 * Update the stats only if flow is not suspended.
	 */
	if ((pf->flags & PPE_DRV_FLOW_MAPT) && !(pf->flow_cfg)) {
		mapt_pcf_v6 = pf->mapt_info.mapt_v6;
		if (mapt_pcf_v6) {
			mapt_cn_v6 = mapt_pcf_v6->conn;
			delta_bytes += delta_pkts * pf->mapt_info.len_adjust;
			ppe_drv_v6_conn_flow_rx_stats_add(mapt_pcf_v6, delta_pkts, delta_bytes);

			if (ppe_drv_v6_conn_flags_check(mapt_pcf_v6->conn, PPE_DRV_V6_CONN_FLAG_RETURN_VALID)) {
				mapt_pcr_v6 = (mapt_pcf_v6 == &mapt_cn_v6->pcf) ? &mapt_cn_v6->pcr : &mapt_cn_v6->pcf;
				ppe_drv_v6_conn_flow_tx_stats_add(mapt_pcr_v6, delta_pkts, delta_bytes);
			}
		}
	}

	pf->pkts = flow_cntrs.matched_pkts;
	pf->bytes = flow_cntrs.matched_bytes;

	/*
	 * Update the stats only if tree_id has SAWF metadata.
	 */
	if (ppe_drv_tree_id_type_get(&pcf->fl_mdata) == PPE_DRV_TREE_ID_TYPE_SAWF) {
		service_class = tree_id_data->info.sawf_metadata.service_class;
		if (PPE_DRV_SERVICE_CLASS_IS_VALID(service_class))
			ppe_drv_flow_sawf_sc_stats_add(service_class, delta_pkts, delta_bytes);
	}
}

/*
 * ppe_drv_flow_v6_qos_set()
 *	Set QOS mapping for this flow.
 */
bool ppe_drv_flow_v6_qos_set(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_flow *flow)
{
	sw_error_t err;
	fal_qos_cosmap_t qos_cfg = {0};

	/*
	 * Check if flow needs qos update, set priority profile in QoS config
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_QOS_VALID)) {
		qos_cfg.internal_pri = ppe_drv_v6_conn_flow_int_pri_get(pcf);
		qos_cfg.pri_en = A_TRUE;
	}

	/*
	 * Check if flow needs DSCP marking, set DSCP fields in QoS config
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_DSCP_MARKING)) {
		qos_cfg.internal_dscp = ppe_drv_v6_conn_flow_egress_dscp_get(pcf) << PPE_DRV_DSCP_SHIFT;
		qos_cfg.dscp_mask = PPE_DRV_DSCP_MASK;
		qos_cfg.dscp_en = A_TRUE;
	}

	/*
	 * Check if flow needs PCP marking, set PCP fields in QoS config
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_VLAN_PRI_MARKING)) {
		/*
		 * Note: PCP marking is supported for both single and double VLAN tagging.
		 * - For single VLAN: PCP is marked on that VLAN (index 0)
		 * - For double VLAN: PCP is marked on outer VLAN only (index 0)
		 * We do not need to mask unsigned integer.
		 */
		qos_cfg.internal_pcp = ppe_drv_v6_conn_flow_egress_vlan_get(pcf, 0)->tci >> PPE_DRV_VLAN_PRIORITY_SHIFT;;
		qos_cfg.pcp_en = A_TRUE;
	}

	err = fal_qos_cosmap_flow_set(PPE_DRV_SWITCH_ID, 0, flow->index, &qos_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p qos(DSCP/PCP) mapping configuration failed for flow: %p", pcf, flow);
		return false;
	}

	ppe_drv_trace("%p qos(DSCP/PCP) mapping configuration done for flow: %p", pcf, flow);
	return true;
}

/*
 * ppe_drv_flow_v6_qos_clear()
 *	Return service code required for this flow.
 */
bool ppe_drv_flow_v6_qos_clear(struct ppe_drv_flow *pf)
{
	sw_error_t err;
	fal_qos_cosmap_t qos_cfg = {0};

	err = fal_qos_cosmap_flow_set(PPE_DRV_SWITCH_ID, 0, pf->index, &qos_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p qos mapping configuration failed for flow", pf);
		return false;
	}

	ppe_drv_trace("%p qos mapping configuration done for flow", pf);
	return true;
}

#ifdef NSS_PPE_DRV_HW_GRO
static bool ppe_drv_hw_gro_is_enabled(struct ppe_drv *p, struct ppe_drv_port *tx_port, struct ppe_drv_port *rx_port)
{
	if ((ppe_drv_port_flags_check(tx_port, PPE_DRV_PORT_FLAG_NETFN_OL)) && rx_port->hw_gro_en) {
		if (atomic_read(&p->gro_ctx.num_hw_gro_flows) < NSS_PPE_DRV_MAX_GRO_FLOWS)
			return true;
	}

	if ((ppe_drv_port_flags_check(rx_port, PPE_DRV_PORT_FLAG_NETFN_OL)) && tx_port->hw_gro_en) {
		if (atomic_read(&p->gro_ctx.num_hw_gro_flows) < NSS_PPE_DRV_MAX_GRO_FLOWS)
			return true;
	}

	return false;
}
#endif

#ifdef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
/*
 * ppe_drv_flow_v6_flow_cookie48b_get()
 *	fill the flow cookie information
 */
static bool ppe_drv_flow_v6_flow_cookie48b_get(struct ppe_drv_v6_conn_flow *pcf, fal_flow_qos_t *flow_qos)
{
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	bool wifi_qos_en = false;
	uint8_t wifi_qos = 0;
	uint16_t stream_id;
	a_uint64_t tmp = 0;
	uint16_t ast_info;
	bool ret = true;
	uint8_t low_bit = 0;
#ifdef PPE_DRV_GRETAP_MAPT_SINGLEPASS
	struct ppe_drv_tun *ptun_tx;
#endif

	if (!ppe_drv_flow_v6_wifi_qos_get(pcf, &wifi_qos, &wifi_qos_en)) {
		memset(&flow_qos->cookie_48b, 0, sizeof(flow_qos->cookie_48b));
		ppe_drv_warn("%p: failed to obtain wifi qos", pcf);
		return false;
	}

	switch (tree_id_data->type) {
		case PPE_DRV_TREE_ID_TYPE_WIFI8_PPEDS:
			if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_WIFI_AST_VALID)) {
				ast_info = pcf->fl_mdata.tree_id_data.wifi_metadata.ast_info;
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, ast_info);
			}

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_WIFI_STREAM_ID_VALID)) {
				stream_id = pcf->fl_mdata.tree_id_data.wifi_metadata.stream_id;
				PPE_DRV_FLOW_COOKIE_STREAM_ID_VALID_SET(stream_id);
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_LOW, stream_id);
			}

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			ppe_drv_trace("cookie_48b %llx\n", tmp);

			flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
			break;

		case PPE_DRV_TREE_ID_TYPE_NONE:
#ifdef PPE_DRV_GRETAP_MAPT_SINGLEPASS
			ptun_tx = pcf->tx_port ? ppe_drv_port_tun_get(pcf->tx_port) : NULL;

			/*
			 * This change is primarily required for GRETAP over WLAN. When the tx port
			 * is GRETAP, WiFi HW needs the AST index to deliver to the correct client.
			 * Since we are not filling the flow cookie here, setting
			 * BIT(PPE_DRV_FLOW_COOKIE_GRE_AST_FROM_PKT_SHIFT) in cookie[15:0] signals
			 * WiFi HW to read the AST index directly from the packet rather than from
			 * the flow cookie.
			 */
			if (ptun_tx && ptun_tx->th.type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_LOW, BIT(PPE_DRV_FLOW_COOKIE_GRE_AST_FROM_PKT_SHIFT));
				memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
				ppe_drv_trace("gre cookie_48b %llx\n", tmp);
				flow_qos->qos_valid = A_TRUE;
				flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
				break;
			}
#endif
			low_bit = (tree_id_data->info.value >> 16) & 0xFF;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tree_id_data->info.value & 0xFFFF);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, (low_bit | (tree_id_data->type << 4)));

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			break;

		case PPE_DRV_TREE_ID_TYPE_WIFI_TID:
		case PPE_DRV_TREE_ID_TYPE_UDP_ST:
		case PPE_DRV_TREE_ID_TYPE_SCS:
			low_bit = (tree_id_data->info.value >> 16) & 0xFF;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tree_id_data->info.value & 0xFFFF);
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, (low_bit | (tree_id_data->type << 4)));

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			break;

		case PPE_DRV_TREE_ID_TYPE_SAWF:
		case PPE_DRV_TREE_ID_TYPE_MLO_ASSIST:
			low_bit = (tree_id_data->info.value >> 16) & 0xFF;
			memcpy(&tmp, &flow_qos->cookie_48b, sizeof(tmp));

			tmp &= ~FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H;
			tmp &= ~FAL_FLOW_COOKIE_48BIT_MIDDLE;

			if (pcf->tx_port) {
				if ((pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_DS) &&
				    ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_WIFI_AST_VALID)) {
					tmp &= ~FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L;
					ast_info = pcf->fl_mdata.tree_id_data.wifi_metadata.ast_info;
					tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, ast_info);
				} else if (pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_ACTIVE_VP) {
					tmp &= ~FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L;
					tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tree_id_data->info.value & 0xFFFF);
				}
			}

			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, (low_bit | (tree_id_data->type << 4)));

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			break;

		default:
			ppe_drv_warn("Invalid tree_id_type : (%u)", tree_id_data->type);
			ret = false;
	}

	/*
	 * If the flow contains the host Qdisc information, fill the respective QDISC
	 * valid bit in the tree id
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_VP_HOST_QDISC_INFO_VALID)) {
		tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, 0x80);
		memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
		flow_qos->qos_valid = wifi_qos_en;
		ret = true;
	}

	return ret;
}
#elif defined(PPE_DRV_FLOW_COOKIE_40B_SUPPORT)
/*
 * ppe_drv_flow_v6_flow_cookie40b_get()
 *      Find the tree ID associated with a flow
 */
static bool ppe_drv_flow_v6_flow_cookie40b_get(struct ppe_drv_v6_conn_flow *pcf, uint8_t *cookie_40b)
{
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	bool ret = true;

	switch (tree_id_data->type) {
		case PPE_DRV_TREE_ID_TYPE_NONE:
			cookie_40b[0] = tree_id_data->info.value & 0xFF;
			cookie_40b[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			cookie_40b[2] = (tree_id_data->info.value & 0xFF0000) >> 16;
			cookie_40b[3] = (tree_id_data->info.value & 0xFF000000) >> 24;

			/*
			 * Currently info.value is a 32 bit metadata
			 * Hence storing the 8 bits with 0
			 */
			cookie_40b[4] = 0;
			break;

		case PPE_DRV_TREE_ID_TYPE_WIFI_TID:
		case PPE_DRV_TREE_ID_TYPE_SCS:
			cookie_40b[4] = tree_id_data->type << 3;
			break;

		case PPE_DRV_TREE_ID_TYPE_SAWF:
		case PPE_DRV_TREE_ID_TYPE_MLO_ASSIST:
			/*
			 * type(5 bits) | reserved(17bits) | data(18 bits)
			 */
			cookie_40b[0] = tree_id_data->info.value & 0xFF;
			cookie_40b[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			cookie_40b[2] = (tree_id_data->info.value & 0x30000) >> 16;
			cookie_40b[3] = 0;
			cookie_40b[4] = tree_id_data->type << 3;

			break;

		default:
			ppe_drv_warn("Invalid tree_id_type : (%u)", tree_id_data->type);
			ret = false;
	}

	/*
	 * If the flow contains the host Qdisc information, fill the respective QDISC
	 * valid bit in the tree id
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HOST_ASSIST_QDISC)) {
		cookie_40b[4] |= 0x80;
		ret = true;
	}

	return ret;
}
#else
/*
 * ppe_drv_flow_v6_tree_id_get()
 *	Find the tree ID associated with a flow
 */
static bool ppe_drv_flow_v6_tree_id_get(struct ppe_drv_v6_conn_flow *pcf, uint8_t *tree_id)
{
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	bool ret = true;

	switch (tree_id_data->type) {
		case PPE_DRV_TREE_ID_TYPE_NONE:
			tree_id[0] = tree_id_data->info.value & 0xFF;
			tree_id[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			tree_id[2] = (tree_id_data->info.value & 0xFF0000) >> 16;
			break;

		case PPE_DRV_TREE_ID_TYPE_WIFI_TID:
		case PPE_DRV_TREE_ID_TYPE_SCS:
			tree_id[0] = 0;
			tree_id[1] = 0;
			tree_id[2] = tree_id_data->type << 4;
			break;

		case PPE_DRV_TREE_ID_TYPE_SAWF:
		case PPE_DRV_TREE_ID_TYPE_MLO_ASSIST:
			/*
			 * type(4 bits) | reserved (2 bits) | data (18 bits)
			 */
			tree_id[0] = tree_id_data->info.value & 0xFF;
			tree_id[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			tree_id[2] = (tree_id_data->info.value & 0x30000) >> 16;
			tree_id[2] |= tree_id_data->type << 4;
			break;

		case PPE_DRV_TREE_ID_TYPE_UDP_ST:
			tree_id[0] = (tree_id_data->info.value & PPE_DRV_UDP_ST_TREE_ID_0_MASK) >> PPE_DRV_UDP_ST_TREE_ID_0_SHIFT;
			tree_id[1] = (tree_id_data->info.value & PPE_DRV_UDP_ST_TREE_ID_1_MASK) >> PPE_DRV_UDP_ST_TREE_ID_1_SHIFT;
			tree_id[2] = (tree_id_data->info.value & PPE_DRV_UDP_ST_TREE_ID_2_MASK) >> PPE_DRV_UDP_ST_TREE_ID_2_SHIFT;
			tree_id[2] |= tree_id_data->type << PPE_DRV_UDP_ST_TREE_ID_DATA_TYPE_SHIFT;
			break;

		default:
			ppe_drv_warn("Invalid tree_id_type : (%u)", tree_id_data->type);
			ret = false;
	}

	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HOST_ASSIST_QDISC)) {
		tree_id[2] |= 0x80;
		ret = true;
	}

	return ret;
}
#endif

/*
 * ppe_drv_flow_v6_vpn_id_get()
 *	Find the VPN ID associated with a flow
 */
static bool ppe_drv_flow_v6_vpn_id_get(struct ppe_drv_v6_conn_flow *pcf, uint32_t *vpn_id)
{
#ifdef NSS_PPE_SIXTH_TUPLE
	int sixth_tuple_mode = ppe_drv_get_sixth_tuple_mode();

	if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VSI) {
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			struct ppe_drv_vsi *vsi = ppe_drv_v6_conn_flow_eg_top_vsi_get(pcf);
			if (!vsi) {
				ppe_drv_warn("No bridge vsi");
				return false;
			}
			*vpn_id = FAL_VPN_ID_FROM_VSI(vsi->index);
		} else {
			struct ppe_drv_iface *l3_if = ppe_drv_v6_conn_flow_in_l3_if_get(pcf);
			if (!l3_if || !l3_if->l3) {
				ppe_drv_warn("No valid l3 iface");
				return false;
			}
			*vpn_id = FAL_VPN_ID_FROM_INTF(l3_if->l3->l3_if_index);
		}

	} else if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VLAN) {
		uint8_t vlan_hdr_cnt = ppe_drv_v6_conn_flow_ingress_vlan_cnt_get(pcf);
		*vpn_id = PPE_DRV_VLAN_NOT_CONFIGURED;
		if (vlan_hdr_cnt > 0) {
			struct ppe_drv_vlan *vlan_hdr = ppe_drv_v6_conn_flow_ingress_vlan_get(pcf, 0);
			if (!vlan_hdr) {
				ppe_drv_warn("No valid vlan hdr.");
				return false;
			}
			*vpn_id = FAL_VPN_ID_FROM_VLAN(vlan_hdr->tci & PPE_DRV_VLAN_ID_MASK);
		}
	}
#endif
	return true;
}

/*
 * ppe_drv_flow_ds_wifi_qos_set()
 *	Sets the WiFi QoS for DS mode
 */
static void ppe_drv_flow_ds_wifi_qos_set(uint8_t *wifi_qos, bool *wifi_qos_en, uint32_t *msduq_value, bool flow_override_mode, bool flow_queue_id)
{
	uint8_t tid;

	/*
	 * In case of DS mode,
	 *
	 * For flow override mode, wifi qos is configured as below:
	 * --------------------------------------------------------------------------------------
	 * |	Who Classify (2 bits)	|	TID (3 bits)	|	Flow queue ID (1 bit)	|
	 * --------------------------------------------------------------------------------------
	 *
	 * For HLOS TID mode,
	 *
	 * case 1: If wifi_qos_en flag is true
	 *
	 *	------------------------------------------------------------------
	 *	|  (4 bits)   |       TID (3 bits)    |  Flow queue ID (1 bit)   |
	 *	------------------------------------------------------------------
	 *
	 * case 2: If wifi_qos_en flag is false.
	 *
	 *	fill wifi_qos[7]=1 to support hlos_tid Override configuration interpretation
	 *	------------------------------------------------------------------------------
	 *	|  HLOS_TID override mode(1 bit)  |   (3 bits)   | TID (3 bits) |  (1 bit)   |
	 *	------------------------------------------------------------------------------
	 *
	 */

	if (flow_override_mode) {
		/*
		 * In sawf, tid value is mapped from msduq
		 */
		tid = *msduq_value & PPE_DRV_FLOW_TID_MASK;
		flow_queue_id = *msduq_value & PPE_DRV_FLOW_FO_MASK;
		*wifi_qos = (*msduq_value & PPE_DRV_FLOW_WC_MASK) | (tid << PPE_DRV_FLOW_TID_SHIFT) | flow_queue_id;

		return;
	}

	/*
	 * For scs, msduq is passed as tid.
	 */
	*wifi_qos = 0;
	tid = *msduq_value;
#ifdef NSS_PPE_IPQ54XX
	/*
	 * Enable WIFI_QOS flag for hlos tid mode explicitly for IPQ54XX.
	 */
	*wifi_qos_en = true;
	*wifi_qos = (tid << PPE_DRV_FLOW_TID_SHIFT) | flow_queue_id;
#else
	/*
	 * Disabling WIFI_QOS flag for hlos tid mode
	 */
	*wifi_qos_en = false;
	*wifi_qos = PPE_DRV_FLOW_DS_HLOS_TID_OVERRIDE_ENABLE | (tid << PPE_DRV_FLOW_TID_SHIFT);
#endif
}

/*
 * ppe_drv_flow_override_mode_get()
 * 	Return false for hlos override mode.
 */
static bool ppe_drv_flow_override_mode_get(uint32_t *msduq_value)
{
	if (*msduq_value <= PPE_DRV_FLOW_HLOS_OVERRIDE_MSDUQ_MAX)
		return false;

	return true;
}

/*
 * ppe_drv_v6_is_flow_is_non_udp()
 *      Return 1 if the proto type is non UDP.
 */
static int ppe_drv_v6_is_flow_is_non_udp(struct ppe_drv_v6_conn_flow *pcf)
{
	if (ppe_drv_v6_conn_flow_match_protocol_get(pcf) == IPPROTO_UDP)
		return 0;

	return 1;
}

/*
 * ppe_drv_flow_v6_wifi_qos_get()
 *	Find the WIFI QOS associated with a flow
 */
static bool ppe_drv_flow_v6_wifi_qos_get(struct ppe_drv_v6_conn_flow *pcf, uint8_t *wifi_qos, bool *wifi_qos_en)
{
	bool flow_override_mode = true;
	bool flow_queue_id = false;

	/*
	 * If SAWF metadata is valid, set 6 bit MSDUQ in wifi_qos field (bits 0-5).
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_METADATA_TYPE_WIFI_INFO)) {
		/*
		 * MSDUQ representation in wifi_qos field is:
		 * --------------------------------------------------------------------------------------
		 * |	Who Classify (2 bits)	|	Flow override (1 bit)	|	TID (3 bits)	|
		 * --------------------------------------------------------------------------------------
		 */
		*wifi_qos = pcf->fl_mdata.wifi_qos;
		*wifi_qos_en = true;

		/*
		 * In case of DS mode, rearrange the MSDUQ representation in wifi qos field to be aligned with TCL descriptor.
		 */
		if (!ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_VP_VALID) &&
				(pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_DS)) {
			flow_override_mode = ppe_drv_flow_override_mode_get(&pcf->fl_mdata.wifi_qos);

			/*
			 * Get flow queue id type as UDP or TCP.
			 */
			flow_queue_id = ppe_drv_v6_is_flow_is_non_udp(pcf);

			ppe_drv_flow_ds_wifi_qos_set(wifi_qos, wifi_qos_en, &pcf->fl_mdata.wifi_qos, flow_override_mode, flow_queue_id);
			ppe_drv_trace("WiFi_QoS configured in DS descriptor is: 0x%x\n", *wifi_qos);
		}

		ppe_drv_trace("For User type: %u, WiFi_QoS initially: 0x%x and WiFi_QoS configured: 0x%x", pcf->tx_port->user_type, pcf->fl_mdata.wifi_qos, *wifi_qos);
	}

	/*
	 * If the flow contains the host Qdisc information, set the WIFI QoS enable
	 * bit to indicate the PPE to pass the tree-id field (which contains the
	 * QDISC valid bit) in the EDMA Rx secondary descriptor
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HOST_ASSIST_QDISC)) {
		*wifi_qos_en = true;
	}

	/*
	 * If the vp type is active or the flow is active vp, set the WIFI QoS enable
	 * to indicate the PPE to set the WIFI QoS bit in the EDMA rx descriptor
	 * so that EDMA can fill the MLO assit data in skb->mark which is needed by default
	 * for wlan vp flows.
	 */
	if ((pcf->tx_port && (pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_ACTIVE_VP)) ||
	    ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_VP_VALID)) {
		*wifi_qos_en = true;
	}

	/*
	 * If the flow contains the SW Metadata to be valid, set the WIFI Qos enable
	 * to indicate the PPE to set the WIFI Qos bit in the EDMA rx descriptor
	 * to retrieve the flow_index for the flow.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_SW_MDATA_VALID)) {
		*wifi_qos_en = true;
	}

	/*
	 * If the flow metadata tree-id type is UDP-ST, set the WIFI Qos enable
	 * to indicate the PPE to set the WIFI Qos bit in the EDMA rx descriptor
	 * to retrieve the rule id of the udp speedtest flow.
	 */
	if (pcf->fl_mdata.tree_id_data.type == PPE_DRV_TREE_ID_TYPE_UDP_ST) {
		*wifi_qos_en = true;
	}

	return true;
}

#ifdef PPE_DRV_FLOW_TBL_POLICER
/*
 * ppe_drv_flow_v6_policer_get()
 *	Find the Policer get associated with a flow
 */
static bool ppe_drv_flow_v6_policer_get(struct ppe_drv_v6_conn_flow *pcf, uint32_t *policer_index, a_bool_t *policer_valid)
{
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_POLICER_VALID)) {
		*policer_index = pcf->policer_hw_id;
		*policer_valid = A_TRUE;
	}

	return true;
}
#endif

/*
 * ppe_drv_flow_host_qdisc_clear()
 *	call_rcu() callback for the flow host qdisc rule deletion
 */
static void ppe_drv_flow_host_qdisc_clear(struct rcu_head *p)
{
	struct ppe_drv_flow_host_qdisc_info *qdisc_info = container_of(p, struct ppe_drv_flow_host_qdisc_info, rcu);

	kfree(qdisc_info);
}

/*
 * ppe_drv_flow_host_qdisc_info_del()
 *	API to delete host Qdisc info for the particular flow index
 */
static void ppe_drv_flow_host_qdisc_info_del(uint32_t flow_idx)
{
	struct ppe_drv_flow_host_qdisc_info *qdisc_info = NULL;
	struct ppe_drv_flow_table_info *flow_info = flow_table_info;

	rcu_read_lock();
	qdisc_info = rcu_dereference(flow_info->qdisc_info[flow_idx]);
	rcu_assign_pointer(flow_table_info->qdisc_info[flow_idx], NULL);

	if (qdisc_info->flags == PPE_DRV_HOST_QDISC_INVALID) {
		rcu_read_unlock();
		ppe_drv_trace("Host Qdisc flag is INVALID at flow index: %d\n",
				flow_idx);
		return;
	}

	qdisc_info->flags = PPE_DRV_HOST_QDISC_INVALID;
	qdisc_info->class_id = PPE_DRV_HOST_QDISC_CLASS_ID_DEF_VAL;
	dev_put(qdisc_info->qdisc_xmit_dev);
	qdisc_info->qdisc_xmit_dev = NULL;
	rcu_read_unlock();
	call_rcu(&qdisc_info->rcu, ppe_drv_flow_host_qdisc_clear);
	ppe_drv_trace("Host Qdisc information is deleted at flow index: %d\n",
				flow_idx);
}

/*
 * ppe_drv_flow_v6_host_qdisc_info_del()
 *	API to delete the v6 flow based host Qdisc information for the particular flow index
 */
void ppe_drv_flow_v6_host_qdisc_info_del(struct ppe_drv_v6_conn_flow *pcf, uint32_t flow_idx)
{
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HOST_ASSIST_QDISC)) {
		ppe_drv_flow_host_qdisc_info_del(flow_idx);
	}
}

/*
 * ppe_drv_flow_host_qdisc_info_set()
 *	API to set flow based host Qdisc information for the particular flow index
 */
static void ppe_drv_flow_host_qdisc_info_set(struct ppe_drv_flow_host_qdisc_info *pcf_qdisc_info,
				        uint32_t flow_idx)
{
	struct ppe_drv_flow_host_qdisc_info *local_tbl_info;


	local_tbl_info = kzalloc(sizeof(struct ppe_drv_flow_host_qdisc_info), GFP_ATOMIC);
	if (!local_tbl_info) {
		ppe_drv_warn("Failed to allocate qdisc info space\n");
		return;
	}

	local_tbl_info->flags = pcf_qdisc_info->flags;
	local_tbl_info->class_id = pcf_qdisc_info->class_id;
	local_tbl_info->qdisc_xmit_dev = pcf_qdisc_info->qdisc_xmit_dev;
	dev_hold(local_tbl_info->qdisc_xmit_dev);
	rcu_assign_pointer(flow_table_info->qdisc_info[flow_idx], local_tbl_info);
	ppe_drv_trace("Host Qdisc information is stored at flow index: %d\n",
				flow_idx);
}

/*
 * ppe_drv_flow_v6_host_qdisc_info_set()
 *	API to set v6 flow based host Qdisc information for the particular flow index
 */
static void ppe_drv_flow_v6_host_qdisc_info_set(struct ppe_drv_v6_conn_flow *pcf, uint32_t flow_idx)
{
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HOST_ASSIST_QDISC)) {
		ppe_drv_flow_host_qdisc_info_set(&pcf->qdisc_info, flow_idx);
	}
}

/*
 * ppe_drv_flow_v6_pri_profile_get()
 *	Apply appropiate pri_profile for this flow.
 */
bool ppe_drv_flow_v6_pri_profile_get(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_port *pp, uint8_t *pri_profile)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *reverse_flow = NULL;
	struct ppe_drv_v6_5tuple reverse_tuple;
	uint8_t evp_pri_profile = PPE_DRV_PRI_PROF_DEFAULT;
	uint8_t enq_vp;
	uint8_t next_core;

	/*
	 * Add ppe-ds flow with pri profile.
	 * The classifier provides the PPE-DS node data from Wi-Fi driver.
	 * The node metadata could be used to get the enqueue vport and its pri profile
	 * which is programmed into PPE flow rule.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_WIFI_DS)) {
		evp_pri_profile = ppe_drv_port_metadata_to_pri_prof_internal(pcf->wifi_rule_ds_metadata);
		if (evp_pri_profile == PPE_DRV_PORT_ENQ_VP_PRI_PRFL_INVALID) {
			ppe_drv_warn("%p: Enqueue vport pri profile invalid:%d mdata:%d", pcf, evp_pri_profile, pcf->wifi_rule_ds_metadata);
			return false;
		}
		*pri_profile = evp_pri_profile;
		return true;

	}

	/*
	 * Reversing the current flow because in case of RFS in eth2eth flows,
	 * if we map the two unidirectional entries of a flow to different cores then,
	 * we see an imbalance in the CPU utilisation,
	 * hence keeping the two entries of a flow in a single core.
	 */
	reverse_tuple.flow_ip[0] = pcf->match_dest_ip[0];
	reverse_tuple.flow_ip[1] = pcf->match_dest_ip[1];
	reverse_tuple.flow_ip[2] = pcf->match_dest_ip[2];
	reverse_tuple.flow_ip[3] = pcf->match_dest_ip[3];
	reverse_tuple.return_ip[0] = pcf->match_src_ip[0];
	reverse_tuple.return_ip[1] = pcf->match_src_ip[1];
	reverse_tuple.return_ip[2] = pcf->match_src_ip[2];
	reverse_tuple.return_ip[3] = pcf->match_src_ip[3];
	reverse_tuple.flow_ident = pcf->match_dest_ident;
	reverse_tuple.return_ident = pcf->match_src_ident;
	reverse_tuple.protocol = pcf->match_protocol;
	reverse_tuple.flow_rule_id = pcf->conn ? pcf->conn->flow_rule_id : 0;

	/*
	 * This is when its a RFS flow, pcf flag is set as PPE_DRV_V4_CONN_FLAG_PASSIVE_FLOW.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_PASSIVE_FLOW)) {
		/*
		 * This is when WiFi VAP has a passive VP allocated.
		 */
		if ((ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_PASSIVE_WLAN_FLOW) ||
		     ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_UDP_ST_FLOW)) && p->rfs.passive_vp_enable)
		{
			if (!pp) {
				ppe_drv_trace("pcf %p: Port not allocated in PPE for TX dev.\n", pcf);
				return false;
			}
			next_core = __builtin_ffs(pp->shadow_core_mask) - 1;
			pp->shadow_core_mask &= ~(1 << next_core);
			enq_vp = p->rfs.core2enq_vp[next_core];
			evp_pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
			if (!pp->shadow_core_mask) {
				pp->shadow_core_mask = pp->core_mask;
			}

			*pri_profile = evp_pri_profile;
			ppe_drv_trace("pcf %p: Pri profile: %u enq_vp: %u next_core = %u\n", pcf, evp_pri_profile, enq_vp, next_core);
			return true;
		}

		/*
		 * Get the reverse flow table entry if present.
		 */
		reverse_flow = ppe_drv_flow_v6_get(&reverse_tuple);
		if (reverse_flow) {
			*pri_profile = reverse_flow->pri_profile;
			pcf->rfs_core = reverse_flow->pcf.v6->rfs_core;
			ppe_drv_trace("pcf %p: Pri profile same as reverse flow: %u\n", pcf, evp_pri_profile);
			return true;
		}
		if (!*(pcf->fl_mdata.shadow_coremask)) {
			*(pcf->fl_mdata.shadow_coremask) = *(pcf->fl_mdata.coremask);
		}
		next_core = __builtin_ffs(*(pcf->fl_mdata.shadow_coremask)) - 1;
		*(pcf->fl_mdata.shadow_coremask) &= ~(1 << next_core);
		enq_vp = p->rfs.core2enq_vp[next_core];
		evp_pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
		if (evp_pri_profile == PPE_DRV_PORT_ENQ_VP_PRI_PRFL_INVALID) {
			ppe_drv_warn("%p: Enqueue vport pri profile invalid:%d core:%d", pcf, evp_pri_profile, next_core);
			return false;
		}
		pcf->rfs_core = next_core;
		*pri_profile = evp_pri_profile;
		return true;
	}

	if (!pp) {
		ppe_drv_trace("pcf %p: Port not allocated in PPE for TX dev.\n", pcf);
		return false;
	}

	/*
	 * For the flows in different usertype falling back.
	 */
	if (ppe_drv_is_wlan_vp_port_type(pp->user_type)) {
		if (pp->core_mask) {
			next_core = __builtin_ffs(pp->shadow_core_mask) - 1;
			BUG_ON(next_core >= NR_CPUS);
			pp->shadow_core_mask &= ~(1 << next_core);
#if defined(NSS_PPE_DRV_HW_GRO)
        		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_HW_GRO))
				enq_vp = p->gro_ctx.gro_info.core2enq_vp[next_core];
			else
#endif
				enq_vp = p->rfs.core2enq_vp[next_core];
			evp_pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
			if (!pp->shadow_core_mask) {
				pp->shadow_core_mask = pp->core_mask;
			}
		} else {
			ppe_drv_warn("%p: invalid core mask for given user type: %u\n", pcf, pp->user_type);
		}
	}

	*pri_profile = evp_pri_profile;
	return true;
}

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
/*
 * ppe_drv_phydev_speed_compare()
 *	Returns true if rx port's phydev speed exceeds tx port's phydev speed.
 */
static inline bool ppe_drv_phydev_speed_compare(struct ppe_drv_port *rx_port, struct ppe_drv_port *tx_port)
{
	if (!tx_port || !tx_port->dev || !tx_port->dev->phydev)
		return false;

	if (!rx_port || !rx_port->dev || !rx_port->dev->phydev)
		return false;

	return rx_port->dev->phydev->speed > tx_port->dev->phydev->speed;
}

/*
 * ppe_drv_flow_v6_check_hierarchy_speed_mismatch()
 *	Check the speed mismatch between interfaces.
 */
static inline bool ppe_drv_flow_v6_check_hierarchy_speed_mismatch(struct ppe_drv *p, struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_port *pp)
{
	struct ppe_drv_lpbk_buffer_map_entry *map_entry;
	struct ppe_drv_v6_conn *cn = ppe_drv_v6_conn_flow_conn_get(pcf);
	struct ppe_drv_v6_conn_flow *pcr = NULL;
	bool is_speed_mismatch = false;

	if (cn) {
		pcr = (pcf == &cn->pcf) ? &cn->pcr : &cn->pcf;
	}

	if (!list_empty(&p->lpbk_buffer_map) && pcf->rx_port && pcf->rx_port->dev) {
		list_for_each_entry(map_entry, &p->lpbk_buffer_map, list) {
			bool ingress_match;
			bool egress_match;

			if (map_entry->ingress_dev != pcf->rx_port->dev ||
					map_entry->egress_dev != pp->dev) {
				continue;
			}

			/*
			 * PPPoE check
			 */
			if (map_entry->ingress_pppoe) {
				if (!pcr || !ppe_drv_v6_conn_flow_flags_check(pcr, PPE_DRV_V6_CONN_FLOW_FLAG_PPPOE_FLOW) ||
						pcr->pppoe_session_id != map_entry->pppoe_session_id ||
						memcmp(pcr->pppoe_server_mac, map_entry->pppoe_server_mac, ETH_ALEN)) {
					continue;
				}
			} else if (map_entry->egress_pppoe) {
				if (!ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_PPPOE_FLOW) ||
						pcf->pppoe_session_id != map_entry->pppoe_session_id ||
						memcmp(pcf->pppoe_server_mac, map_entry->pppoe_server_mac, ETH_ALEN)) {
					continue;
				}
			}

			/*
			 * Ingress VLAN check
			 */
			if (map_entry->ingress_vlan_cnt == 0) {
				ingress_match = true;
			} else if (map_entry->ingress_vlan_cnt == 1) {
				ingress_match = (pcf->ingress_vlan_cnt >= 1) &&
					((pcf->ingress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->ingress_vlan_id[0]);
			} else {
				ingress_match = (pcf->ingress_vlan_cnt >= 2) &&
					((pcf->ingress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->ingress_vlan_id[0]) &&
					((pcf->ingress_vlan[1].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->ingress_vlan_id[1]);
			}

			/*
			 * Egress VLAN check
			 */
			if (map_entry->egress_vlan_cnt == 0) {
				egress_match = true;
			} else if (map_entry->egress_vlan_cnt == 1) {
				egress_match = (pcf->egress_vlan_cnt >= 1) &&
					((pcf->egress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->egress_vlan_id[0]);
			} else {
				egress_match = (pcf->egress_vlan_cnt >= 2) &&
					((pcf->egress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->egress_vlan_id[0]) &&
					((pcf->egress_vlan[1].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->egress_vlan_id[1]);
			}

			if (ingress_match && egress_match) {
				is_speed_mismatch = true;
				break;
			}
		}
	}

	return is_speed_mismatch;
}
#endif

/*
 * ppe_drv_flow_v6_service_code_get()
 *	Return service code required for this flow.
 */
bool ppe_drv_flow_v6_service_code_get(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_port *pp, uint8_t *scp)
{
	struct ppe_drv *p __maybe_unused = ppe_drv_gbl;
	struct ppe_drv_port *port_rx = pcf->rx_port;
	ppe_drv_sc_t service_code = *scp;
	ppe_drv_sc_t sc = PPE_DRV_SC_NONE;
	ppe_drv_ret_t ret __maybe_unused;
#ifdef PPE_DRV_DDRQ_ENABLE
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_DDRQ_PASSTHROUGH_DIS)) {
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_ENCAP)) {
			*scp = PPE_DRV_SC_DDRQ_LP_SC_IPSEC_ENCAP;
		} else if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_DECAP)) {
			*scp = PPE_DRV_SC_DDRQ_LP_SC_IPSEC_DECAP;
		} else if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW) &&
				ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_DDRQ_VLAN_IF)) {
			*scp = PPE_DRV_SC_DDRQ_LP_SC_BRIDGE;
		} else {
			*scp = PPE_DRV_SC_DDRQ_LP_SC;
		}
		ppe_drv_warn("Flow has the DDRQ PT disable flag set, setting SC:%d for it\n", *scp);
		return true;
	}
#endif
#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	if (p->loopback_ring_info.enabled &&
			(p->loopback_ring_info.ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_DDR_BUFFER) &&
			pp && pp->dev) {
		bool is_speed_mismatch = ppe_drv_flow_v6_check_hierarchy_speed_mismatch(p, pcf, pp);

		/*
		 * check if phydev speed are asymmetric or not.
		 */
		if (!is_speed_mismatch && p->ddr_buffer_auto_en)
			is_speed_mismatch = ppe_drv_phydev_speed_compare(pcf->rx_port, pp);

		/*
		 * set service code to use loopback ring.
		 */
		if (is_speed_mismatch) {
			if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER)) {
				return false;
			}

			*scp = service_code;
			return true;
		}
	}
#endif
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	ret = ppe_drv_veip_gw_port_sc(ppe_drv_v6_conn_flow_tx_port_get(pcf), &sc);
	if (ret == PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD) {
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
		*scp = service_code;
		return true;
	} else if (ret == PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD_FAIL) {
		ppe_drv_warn("%p: VEIP US v6 flow add failed ret=%d, sc=%u\n", pcf, ret, sc);
		return false;
	}
#endif
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_PRIORITY_PPE_ASSIST)) {
		/*
		 * Service code to set priority for PPE assisted flows.
		 * No other service code is supported if priority assist is active.
		 */
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NOEDIT_PRIORITY_SET)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
						pcf, service_code, PPE_DRV_SC_NOEDIT_PRIORITY_SET);
			return false;
		}

		*scp = service_code;
		return true;
	}

	/*
	 * Multicast RFS flows are redirected to the CPU core assigned.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_MCAST_RFS_FLOW)
#ifdef NSS_PPE_PON_SUPPORT
				|| (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW) &&
					ppe_drv_is_pon_enable_internal())
#endif
		  ) {
			ppe_drv_sc_t core_sc;

			switch (pcf->rfs_core) {
				case 0:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE0;
					break;
				case 1:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE1;
					break;
				case 2:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE2;
					break;
				case 3:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE3;
					break;
				default:
					ppe_drv_warn("%p: invalid core %u for mcast RFS flow", pcf, pcf->rfs_core);
					return false;
			}

			if (!ppe_drv_sc_check_and_set(&service_code, core_sc)) {
				ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
						pcf, service_code, core_sc);
				return false;
			}

			*scp = service_code;
			return true;
		}
	}

	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		/*
		 * Service code to be mark for RFS PPE assisted flows so PPE doesn't edit packets.
		 */
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NOEDIT_RFS_RULE)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u", pcf, service_code, PPE_DRV_SC_NOEDIT_RFS_RULE);
			return false;
		}

		*scp = service_code;
		return true;
	}

	/*
	 * Service code to enable PPE to bypass packet header editing and forward them unmodified.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_NO_EDIT_RULE)) {
		/*
		 * Service code to set noedit rule.
		 */
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NOEDIT_RULE)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_NOEDIT_RULE);
			return false;
		}

		*scp = service_code;
		return true;
	}

	/*
	 * If the egress port has MPSK support and if it is a bridge flow,
	 * bypass the egress vlan table for the same.
	 */
	if (ppe_drv_port_flags_check(pp, PPE_DRV_PORT_FLAG_MPSK)) {
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_VP_MPSK)) {
				ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
							pcf, service_code, PPE_DRV_SC_VP_MPSK);
				return false;
			}
		}

		*scp = service_code;
		return true;
	}

#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HAIRPIN_FLOW)) {
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NPT66_HAIRPIN_NAT)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
						pcf, service_code, PPE_DRV_SC_NPT66_HAIRPIN_NAT);
			return false;
		}

		*scp = service_code;
		return true;
	}
#endif

	/*
	 * Get the service code for the flow according to the flow type
	 * and precedence of these features (like DS flows, policer/ACL based service code, etc)
	 */
	if((ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_POLICER_VALID) ||
			ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_ACL_VALID)) && (pcf->acl_sc != PPE_DRV_SC_NONE)) {
		sc = pcf->acl_sc;
	} else if (pp->user_type == PPE_DRV_PORT_USER_TYPE_DS) {
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_VP_VALID)) {
			if (pp->core_mask) {
				sc = PPE_DRV_SC_EDIT_RFS_RULE;
			} else {
				sc = PPE_DRV_SC_VP_RPS;
			}
		} else if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_OFFLOAD_DISABLED) ||
					ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_PASSIVE_FLOW)) {
			if (pp->core_mask) {
				sc = PPE_DRV_SC_NOEDIT_RFS_RULE;
			} else {
				ppe_drv_warn("%p: invalid core mask for ds user type", pcf);
				return false;
			}
		}
	} else if ((pp->user_type == PPE_DRV_PORT_USER_TYPE_ACTIVE_VP) && pp->core_mask) {
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_OFFLOAD_DISABLED) ||
					ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_PASSIVE_FLOW)) {
			sc = PPE_DRV_SC_NOEDIT_RFS_RULE;
		} else {
			sc = PPE_DRV_SC_EDIT_RFS_RULE;
		}
	} else if ((pp->user_type == PPE_DRV_PORT_USER_TYPE_PASSIVE_VP) && pp->core_mask) {
		sc = PPE_DRV_SC_NOEDIT_RFS_RULE;
	}

	if (is_vlan_dev(port_rx->dev) && (port_rx->type == PPE_DRV_PORT_VIRTUAL) && (netif_is_bridge_port(port_rx->dev)) &&
	    !ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
		sc = (port_rx->is_fdb_learn_enabled && pp->is_fdb_learn_enabled) ?
			PPE_DRV_SC_VLAN_FILTER_BYPASS : PPE_DRV_SC_SPF_BYPASS;
	}

	if (sc != PPE_DRV_SC_NONE) {
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
	}

	/*
	 * If the flow is flagged for inline IPsec, assign the EIP decap service code.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_INLINE_IPSEC)) {
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_IPSEC_PPE2EIP_DECAP)) {
			ppe_drv_warn("%p: EIP flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_IPSEC_PPE2EIP_DECAP);
			return false;
		}
	}

	/*
	 * If the flow is flagged for inline IPsec ENCAP direction,
	 * assign the EIP encap service code.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_ENCAP)) {
		sc = PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: EIP flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP);
			return false;
		}
	}

	/*
	 * SC required to accelerate full inline EIP DECAP flow.
	 * It enables ACL matching based on the SPI field in the packet.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_DECAP)) {
		sc = PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: EIP flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH);
			return false;
		}
	}

	/*
	 * Set service code to redirect packet to host for qdisc processing for IPv6.
	 */
	if ((sc == PPE_DRV_SC_NONE) && (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_HOST_ASSIST_QDISC))) {
		sc = PPE_DRV_SC_HOST_QOS;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: Host assisted QoS flow required multiple service codes, existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
	}

	/*
	 * SC required when its a bridge flow or fdb learning is disabled.
	 */
	if ((sc == PPE_DRV_SC_NONE) &&
			(ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW) || !port_rx->is_fdb_learn_enabled)) {
		sc = PPE_DRV_SC_VLAN_FILTER_BYPASS;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: Bridge flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
	}

	if ((sc == PPE_DRV_SC_NONE) &&
	    (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_BRIDGE_VLAN_NETDEV))) {
		sc = PPE_DRV_SC_VLAN_FILTER_BYPASS;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: service code %d update failed in VLAN over bridge sc %d\n", pcf, service_code,
				     sc);
			return false;
		}
	}

	*scp = service_code;
	return true;
}

/*
 * ppe_drv_flow_v6_get()
 *	Find flow table entry.
 */
struct ppe_drv_flow *ppe_drv_flow_v6_get(struct ppe_drv_v6_5tuple *tuple)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;

	flow = ppe_drv_v6_flow_hash_tuple_find(tuple, p->hash_conn_v6);
	if (!flow) {
		ppe_drv_trace("%p: flow get failed", tuple);
		return NULL;
	}

	ppe_drv_assert((flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already decelerated from PPE at index: %d",
			tuple, flow->index);
	ppe_drv_trace("%p: flow_tbl entry found at index: %u", tuple, flow->index);

	return flow;
}

/*
 * ppe_drv_flow_v6_get_by_dev()
 *	Find flow with 5tuple and incoming device.
 */
struct ppe_drv_flow *ppe_drv_flow_v6_get_by_dev(struct ppe_drv_v6_5tuple *tuple, struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;

	flow = ppe_drv_v6_flow_hash_tuple_find_by_dev(tuple, p->hash_conn_v6, dev);
	if (!flow) {
		ppe_drv_trace("%p: flow get failed", tuple);
		return NULL;
	}

	ppe_drv_assert((flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already decelerated from PPE at index: %d",
			tuple, flow->index);
	ppe_drv_trace("%p: flow_tbl entry found at index: %u", tuple, flow->index);
	return flow;
}


/*
 * ppe_drv_flow_v6_wlan_metadata_set()
 *	Set flow QOS information for the v6 flow.
 */
bool ppe_drv_flow_v6_wlan_metadata_set(struct ppe_drv_v6_conn_flow *pcf)
{
	fal_flow_qos_t flow_qos = {0};
	uint16_t index;
	bool wifi_qos_en = false;
	sw_error_t err;

	index = pcf->pf->index;

	err = fal_flow_qos_get(PPE_DRV_SWITCH_ID, index, &flow_qos);
	if (err != SW_OK) {
                ppe_drv_warn("%px: Failed to obtain flow qos", pcf);
                return false;
        }

#ifdef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
        /*
         * Get the flow cookie corresponding to flow.
         */
        if (!ppe_drv_flow_v6_flow_cookie48b_get(pcf, &flow_qos)) {
                ppe_drv_warn("%p: failed to obtain a valid flow cookie", pcf);
                return false;
        }
#elif defined(PPE_DRV_FLOW_COOKIE_40B_SUPPORT)
	/*
	 * Get the flow cookie corresponding to flow.
	 */
	if (!ppe_drv_flow_v6_flow_cookie40b_get(pcf, flow_qos.cookie_40b)) {
		ppe_drv_warn("%px: Failed to obtain a valid flow cookie", pcf);
		return false;
	}

	flow_qos.type = FAL_FLOW_QOS_TYPE_COOKIE_40B;

#else
	/*
	 * Get the tree ID corresponding to flow.
	 */
	if (!ppe_drv_flow_v6_tree_id_get(pcf, flow_qos.tree_id)) {
		ppe_drv_warn("%px: failed to obtain a valid tree ID", pcf);
		return false;
	}
#endif

	if (!ppe_drv_flow_v6_wifi_qos_get(pcf, &flow_qos.qos, &wifi_qos_en)) {
		ppe_drv_warn("%p: failed to obtain wifi qos %d", pcf, flow_qos.qos);
		return false;
	}

	flow_qos.qos_valid = wifi_qos_en;

	err = fal_flow_qos_set(PPE_DRV_SWITCH_ID, index, &flow_qos);
	if (err != SW_OK) {
		ppe_drv_warn("%px: failed to set wifi metadata in PPE with tree id %px and wifi qos %u",
				pcf, flow_qos.tree_id, flow_qos.qos);
		return false;
	}

	return true;
}

/*
 * ppe_drv_flow_v6_add()
 *	Add flow table entry.
 */
struct ppe_drv_flow *ppe_drv_flow_v6_add(struct ppe_drv_v6_conn_flow *pcf, struct ppe_drv_nexthop *nh,
					struct ppe_drv_host *host, bool entry_valid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_stats_sawf_sc *sawf_sc_stats;
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	fal_flow_entry_t flow_cfg = {0};
	uint32_t match_dest_ip[4] = {0};
	struct in6_addr network_dest_ip = {0};
	uint32_t match_protocol = ppe_drv_v6_conn_flow_match_protocol_get(pcf);
	uint8_t vlan_hdr_cnt = ppe_drv_v6_conn_flow_egress_vlan_cnt_get(pcf);
	uint8_t service_class;
	struct ppe_drv_iface *port_if = ppe_drv_v6_conn_flow_eg_port_if_get(pcf);
	struct ppe_drv_port *pp = NULL;
#ifdef NSS_PPE_DRV_HW_GRO
	struct ppe_drv_port *tx_port = NULL;
	struct ppe_drv_port *rx_port = NULL;
#endif
	struct ppe_drv_flow *flow;
	bool tuple_3 = false;
#ifndef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
	bool wifi_qos_en = false;
#endif
	uint16_t xmit_mtu;
	fal_flow_entry_t *unidir_flow_cfg = NULL;
	sw_error_t err;

	/*
	 * PPE port reference is not taken for priority assist portless RFS in PPE. PPE is
	 * used only for priority queue selection. Hence Port would not be valid.
	 */
	if (!ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_PRIORITY_PPE_ASSIST)) {
		if (!port_if) {
			ppe_drv_warn("%p: Invalid egress port_if", pcf);
			return NULL;
		}

		pp = ppe_drv_iface_port_get(port_if);
		if (!pp) {
			ppe_drv_warn("%p: Invalid egress port", pcf);
			return NULL;
		}
	}

	ppe_drv_v6_conn_flow_match_dest_ip_get(pcf, &match_dest_ip[0]);

	/*
	 * Change the destination ip to network byte order
	 */
	PPE_DRV_IPV6_TO_IN6(network_dest_ip, match_dest_ip)

	ppe_drv_trace("%p: flow_tbl[host_idx]: %u", pcf, host->index);
	flow_cfg.host_addr_type = PPE_DRV_HOST_LAN;
	flow_cfg.host_addr_index = host->index;
	flow_cfg.deacclr_en = A_FALSE;
	flow_cfg.invalid = !entry_valid;
	flow_cfg.sevice_code = PPE_DRV_SC_NONE;

#if defined(NSS_PPE_DRV_HW_GRO)
	/*
	 * Enable HW GRO only if:
	 * 1. Flow has valid SW metadata (PPE_DRV_V6_CONN_FLAG_FLOW_SW_MDATA_VALID)
	 *    This ensures the flow has proper metadata tracking for GRO processing
	 * 2. We haven't exceeded the maximum number of concurrent GRO flows
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_SW_MDATA_VALID)) {
		tx_port = ppe_drv_v6_conn_flow_tx_port_get(pcf);
		rx_port = ppe_drv_v6_conn_flow_rx_port_get(pcf);

		if (ppe_drv_hw_gro_is_enabled(p, tx_port, rx_port)) {
			ppe_drv_v6_conn_flow_flags_set(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_HW_GRO);
			atomic_inc(&p->gro_ctx.num_hw_gro_flows);
		}
	}
#endif

	if (!ppe_drv_flow_v6_pri_profile_get(pcf, pp, &flow_cfg.pri_profile)) {
		ppe_drv_warn("%p: failed to obtain a valid pri_profile value", pcf);
		return NULL;
	}

	ppe_drv_trace("%p: Pri profile: %u metadata: %u\n", pcf, flow_cfg.pri_profile,
			pcf->wifi_rule_ds_metadata);

	if (!ppe_drv_flow_v6_service_code_get(pcf, pp, &flow_cfg.sevice_code)) {
		ppe_drv_warn("%p: failed to obtain a valid service code", pcf);
		return NULL;
	}

	ppe_drv_trace("pcf %p: flow_tbl[host_idx]: %u sevice_code %d\n", pcf, host->index, flow_cfg.sevice_code);

#ifdef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
        /*
         * Get the flow cookie corresponding to flow.
         */
        if (!ppe_drv_flow_v6_flow_cookie48b_get(pcf, &flow_cfg.flow_qos)) {
                ppe_drv_warn("%p: failed to obtain a valid flow cookie", pcf);
                return NULL;
        }

	flow_cfg.flow_qos.type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
#elif defined(PPE_DRV_FLOW_COOKIE_40B_SUPPORT)
        /*
         * Get the flow cookie corresponding to flow.
         */
        if (!ppe_drv_flow_v6_flow_cookie40b_get(pcf, flow_cfg.flow_qos.cookie_40b)) {
                ppe_drv_warn("%p: failed to obtain a valid flow cookie", pcf);
                return NULL;
        }

        flow_cfg.flow_qos.type = FAL_FLOW_QOS_TYPE_COOKIE_40B;

        /*
         * Get the WIFI QOS corresponding to flow.
         */
	if (!ppe_drv_flow_v6_wifi_qos_get(pcf, &flow_cfg.flow_qos.qos, &wifi_qos_en)) {
		ppe_drv_warn("%p: failed to obtain wifi qos", pcf);
		return NULL;
	}

	flow_cfg.flow_qos.qos_valid = wifi_qos_en;

#else
	/*
	 * Get the tree ID corresponding to flow.
	 */
	if (!ppe_drv_flow_v6_tree_id_get(pcf, flow_cfg.flow_qos.tree_id)) {
		ppe_drv_warn("%p: failed to obtain a valid tree ID", pcf);
		return NULL;
	}

	/*
	 * Get the WIFI QOS corresponding to flow.
	 */
	if (!ppe_drv_flow_v6_wifi_qos_get(pcf, &flow_cfg.flow_qos.qos, &wifi_qos_en)) {
		ppe_drv_warn("%p: failed to obtain wifi qos", pcf);
		return NULL;
	}

	flow_cfg.flow_qos.qos_valid = wifi_qos_en;

#ifdef NSS_PPE_IPQ53XX
	flow_cfg.flow_qos.type = FAL_FLOW_QOS_TYPE_TREE_ID;
#endif
#endif

	/*
	 * Get the VPN ID corresponding to flow.
	 */
	if (!ppe_drv_flow_v6_vpn_id_get(pcf, &flow_cfg.vpn_id)) {
		ppe_drv_warn("%p: failed to obtain a valid vpn_id", pcf);
		return NULL;
	}

#ifdef PPE_DRV_FLOW_TBL_POLICER
	if (!ppe_drv_flow_v6_policer_get(pcf, &flow_cfg.policer_index, &flow_cfg.policer_valid)) {
		ppe_drv_warn("%p: failed to obtain policer_index", pcf);
		return NULL;
	}

	/*
	 * Get the Source interface index.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_SRC_INTERFACE_CHECK)) {
		/*
		 * Get the source interface index corresponding to flow.
		 * For bridge flow = Rx port no.
		 * For routed flow = ingress l3 if index.
		 */
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			struct ppe_drv_iface *in_port_if = ppe_drv_v6_conn_flow_in_port_if_get(pcf);
			struct ppe_drv_port *port = ppe_drv_iface_port_get(in_port_if);
			if (port) {
				flow_cfg.src_intf_index = port->port;
				flow_cfg.src_intf_valid = A_TRUE;
				ppe_drv_trace("%p: Bridged flow, src_intf_index: %u", pcf, flow_cfg.src_intf_index);
			}
		} else {
			struct ppe_drv_iface *in_l3_if = ppe_drv_v6_conn_flow_in_l3_if_get(pcf);
			struct ppe_drv_l3_if *l3_if = ppe_drv_iface_l3_if_get(in_l3_if);
			if (l3_if) {
				flow_cfg.src_intf_index = l3_if->l3_if_index;
				flow_cfg.src_intf_valid = A_TRUE;
				ppe_drv_trace("%p: Routed flow, src_intf_index: %u", pcf, flow_cfg.src_intf_index);
			}
		}
	}
#endif

	/*
	 * Set forwarding type
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_PPE_POLICER_ASSIST)) {
		flow_cfg.fwd_type = ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW) ?
					FAL_FLOW_BRIDGE: FAL_FLOW_ROUTE;
		ppe_drv_trace("%p: Policer enabled flow\n", pcf);
	} else if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		flow_cfg.port_valid = A_TRUE;
		if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			flow_cfg.fwd_type = FAL_FLOW_BRIDGE;
			flow_cfg.bridge_port = PPE_DRV_PORT_CPU;
		} else {
			flow_cfg.fwd_type = FAL_FLOW_ROUTE;
			flow_cfg.route_port = PPE_DRV_PORT_CPU;
		}
		ppe_drv_trace("%p: RFS enabled flow\n", pcf);
	} else if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_PRIORITY_PPE_ASSIST)) {
		/*
		 * Case PPE is used only to Assist in priority marking of packets
		 */
		flow_cfg.fwd_type = FAL_FLOW_RDT_TO_CPU;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: Priority Assist: %u", pcf, FAL_FLOW_FORWARD);
	} else if (ipv6_addr_is_multicast(&network_dest_ip)) {
		/*
		 * Multicast flow
		 */
		flow_cfg.fwd_type = FAL_FLOW_FORWARD;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: L2-Multicast: %u", pcf, FAL_FLOW_FORWARD);
	} else if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
		/*
		 * Case bridging
		 */
		flow_cfg.fwd_type = FAL_FLOW_BRIDGE;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: L2: %u", pcf, FAL_FLOW_BRIDGE);

#ifdef PPE_DRV_BRIDGE_NEXT_HOP
		/*
		 * VLAN tag addition from NEXTHOP_TBL for flow based bridging.
		 * With VLAN tagging capability from flow table for bridge flows,
		 * we can avoid EG_VLAN_XLT_RULE based VLAN tagging. This would help
		 * doing VLANs between different ingress and egress VSI.
		 */
		if (nh) {
			flow_cfg.bridge_nexthop_valid = A_TRUE;
			flow_cfg.bridge_nexthop = nh->index;
			ppe_drv_trace("%p:nexthop index: %u", pcf, nh->index);
		}
#endif
		flow_cfg.port_valid = A_TRUE;
		flow_cfg.bridge_port = pp->port;
		ppe_drv_trace("%p: xmit interface port: %d", pcf, pp->port);

		if (ppe_drv_port_is_tunnel_vp(pp)) {
			switch (vlan_hdr_cnt) {
				case 2:
					flow_cfg.svlan_fmt = A_TRUE;
					flow_cfg.cvlan_fmt = A_TRUE;
					flow_cfg.vlan_fmt_valid = 1;
					break;
				case 1:
					flow_cfg.cvlan_fmt = A_TRUE;
					flow_cfg.vlan_fmt_valid = 1;
					break;
				case 0:
					break;
				default:
					ppe_drv_warn("%p: cannot acclerate bridge VLAN flows for more than 2 vlans: %u",
							pcf, vlan_hdr_cnt);
					return NULL;
			}
		}
	} else {
		/*
		 * Case simple routing (Non-NAT)
		 */
		flow_cfg.route_nexthop = nh->index;
		flow_cfg.fwd_type = FAL_FLOW_ROUTE;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: L3: %u", pcf, FAL_FLOW_ROUTE);
	}

	/*
	 * Fill relevant details for 3-tuple and 5-tuple flows.
	 */
	switch (match_protocol) {
	case IPPROTO_TCP:
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_TCP;
		ppe_drv_trace("%p: flow_tbl[protocol]: TCP-%u", pcf, FAL_FLOW_PROTOCOL_TCP);
		break;

	case IPPROTO_UDP:
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_UDP;
		ppe_drv_trace("%p: flow_tbl[protocol]: UDP-%u", pcf, FAL_FLOW_PROTOCOL_UDP);
		break;

	case IPPROTO_UDPLITE:
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_UDPLITE;
		ppe_drv_trace("%p: flow_tbl[protocol]: UDP-Lite-%u", pcf, FAL_FLOW_PROTOCOL_UDPLITE);
		break;

	case IPPROTO_ESP:
		tuple_3 = true;
		ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_PROTOCOL_OTHER);
		break;

	case IPPROTO_IPIP:
		tuple_3 = true;
		ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_PROTOCOL_OTHER);
		break;

	case IPPROTO_GRE:
		tuple_3 = true;
		ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_PROTOCOL_OTHER);
		break;

	default:
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_unknown_proto);
		ppe_drv_warn("%p: protocol: %u cannot be offloaded to PPE", pcf, match_protocol);
		return NULL;
	}

	memcpy(flow_cfg.flow_ip.ipv6.ul, &match_dest_ip, sizeof(match_dest_ip));
	ppe_drv_trace("%p: flow_tbl[dest_ip]: %pI6", pcf, &match_dest_ip);
	if (tuple_3) {
		flow_cfg.entry_type = FAL_FLOW_IP6_3TUPLE_ADDR;
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_OTHER;
		flow_cfg.ip_type = match_protocol;
		ppe_drv_trace("%p: flow_tbl[ip_proto]: 0x%x", pcf, match_protocol);
	} else {
		flow_cfg.entry_type = FAL_FLOW_IP6_5TUPLE_ADDR;

		flow_cfg.src_port = ppe_drv_v6_conn_flow_match_src_ident_get(pcf);
		flow_cfg.dst_port = ppe_drv_v6_conn_flow_match_dest_ident_get(pcf);

		ppe_drv_trace("%p: flow_tbl[sport]: %u", pcf, flow_cfg.src_port);
		ppe_drv_trace("%p: flow_tbl[dport]: %u", pcf, flow_cfg.dst_port);

	}

	/*
	 * Fill Path-MTU.
	 *
	 * Note: For multicast flows, it should be ok to program the minimum MTU value
	 * of all the interfaces, since PPE also check MTU for each destination interface
	 * and exception the packet (without cloning) if MTU check fail for any interface.
	 */
	xmit_mtu = ipv6_addr_is_multicast(&network_dest_ip) ? ppe_drv_v6_conn_flow_mc_min_mtu_get(pcf)
		: ppe_drv_v6_conn_flow_xmit_interface_mtu_get(pcf);
	if (xmit_mtu > PPE_DRV_PORT_JUMBO_MAX) {
		ppe_drv_trace("%p: xmit_mtu: %d is larger, restricting to max: %d", pcf, xmit_mtu, PPE_DRV_PORT_JUMBO_MAX);
		xmit_mtu = PPE_DRV_PORT_JUMBO_MAX;
	}

	flow_cfg.pmtu_check_l3 = (a_bool_t)PPE_DRV_FLOW_PMTU_TYPE_L3;

	/*
	 * For RFS assist flows, force PMTU to jumbo to avoid L3 PMTU for fragmentation cases
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		xmit_mtu = PPE_DRV_PORT_JUMBO_MAX;
	}

	flow_cfg.pmtu = xmit_mtu;

	ppe_drv_trace("%p: flow_tbl[PMTU]: %u", pcf, xmit_mtu);

	/*
	 * Changing the fwd_type to RDT_TO_CPU in case of unidirectional acceleration,
	 * when one of the flow is not accelerated (flow or return)
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_ACCEL_DISABLE)) {
		unidir_flow_cfg = kzalloc(sizeof(fal_flow_entry_t), GFP_ATOMIC);
		if (unidir_flow_cfg) {
			memcpy(unidir_flow_cfg, &flow_cfg, sizeof(fal_flow_entry_t));
		} else {
			ppe_drv_warn("%p: failed to allocate flow_cfg, unidirectional updates will be disabled", pcf);
		}
		flow_cfg.fwd_type = FAL_FLOW_RDT_TO_CPU;
	}

	/*
	 * For UDP-ST v6 flows, set the flow fwd_type to DROP. This is needed for locally
	 * terminated UDP-ST traffic where we only want to accelerate the flow
	 * direction (VP -> WAN) and drop the return direction for tx and drop both
	 * for rx.
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_UDP_ST_DROP)) {
		flow_cfg.fwd_type = FAL_FLOW_DROP;
	}

	/*
	 * Add the flow
	 */
	err = fal_flow_entry_add(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_KEY, &flow_cfg);
	if (err != SW_OK) {
		ppe_drv_trace("%p: flow entry add failed", pcf);
		if (unidir_flow_cfg) {
			kfree(unidir_flow_cfg);
		}
		return NULL;
	}

#ifdef NSS_PPE_DRV_HW_GRO
	/*
	 * GRO enable
	 */
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_HW_GRO)) {
		err = fal_flow_gro_en_set(PPE_DRV_SWITCH_ID, flow_cfg.entry_id, A_TRUE);
		if (err != SW_OK) {
			/*
			 * If HW GRO failed, then expectation is to fall back to SW GRO
			 */
			ppe_drv_warn("%p: Setting of GRO failed, err=%d. Rolling back GRO state", pcf, err);
			ppe_drv_v6_conn_flow_flags_clear(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_HW_GRO);
			atomic_dec(&p->gro_ctx.num_hw_gro_flows);
		}
	}
#endif

	/*
	 * Get the sw instance of flow entry.
	 */
	flow = &p->flow[flow_cfg.entry_id];
	ppe_drv_assert(!(flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already accelerated to PPE at index: %d",
			pcf, flow_cfg.entry_id);

	/*
	 * Set the host Qdisc information for the flow at the particular flow index
	 */
	ppe_drv_flow_v6_host_qdisc_info_set(pcf, flow_cfg.entry_id);

	/*
	 * Increment flow count if SAWF service class is configured in tree_id.
	 */
	if (ppe_drv_tree_id_type_get(&pcf->fl_mdata) == PPE_DRV_TREE_ID_TYPE_SAWF) {
		service_class = tree_id_data->info.sawf_metadata.service_class;
		if (PPE_DRV_SERVICE_CLASS_IS_VALID(service_class)) {
			sawf_sc_stats = &p->stats.sawf_sc_stats[service_class];
			ppe_drv_stats_inc(&sawf_sc_stats->flow_count);
			ppe_drv_trace("Stats Updated: service class %u : flow  %llu\n", service_class, atomic64_read(&sawf_sc_stats->flow_count));
		}
	}

	/*
	 * Update the shadow copy
	 */
	flow->host = host;
	flow->nh = nh;
	flow->flags |= PPE_DRV_FLOW_V6;
	flow->type = PPE_DRV_IP_TYPE_V6;
	flow->entry_type = flow_cfg.entry_type;
	flow->pri_profile = flow_cfg.pri_profile;
	flow->pcf.v6 = pcf;
	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_ACCEL_DISABLE)) {
		flow->unidir_info.flow_cfg = unidir_flow_cfg;
	}

	ppe_drv_trace("%p: flow_tbl entry added at index: %u", pcf, flow_cfg.entry_id);
	return flow;
}

/*
 * ppe_drv_flow_v4_qos_set()
 *	Set QOS mapping for this flow.
 */
bool ppe_drv_flow_v4_qos_set(struct ppe_drv_v4_conn_flow *pcf, struct ppe_drv_flow *pf)
{
	sw_error_t err;
	fal_qos_cosmap_t qos_cfg = {0};

	/*
	 * Check if flow needs qos update, set priority profile in QoS config
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_QOS_VALID)) {
		qos_cfg.internal_pri = ppe_drv_v4_conn_flow_int_pri_get(pcf);
		qos_cfg.pri_en = A_TRUE;
	}

	/*
	 * Check if flow needs DSCP marking, set DSCP fields in QoS config
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_DSCP_MARKING)) {
		qos_cfg.internal_dscp = ppe_drv_v4_conn_flow_egress_dscp_get(pcf) << PPE_DRV_DSCP_SHIFT;
		qos_cfg.dscp_mask = PPE_DRV_DSCP_MASK;
		qos_cfg.dscp_en = A_TRUE;
	}

	/*
	 * Check if flow needs PCP marking, set PCP fields in QoS config
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_VLAN_PRI_MARKING)) {
		/*
		 * Note: PCP marking is supported for both single and double VLAN tagging.
		 * - For single VLAN: PCP is marked on that VLAN (index 0)
		 * - For double VLAN: PCP is marked on outer VLAN only (index 0)
		 * We do not need to mask unsigned integer.
		 */
		qos_cfg.internal_pcp = ppe_drv_v4_conn_flow_egress_vlan_get(pcf, 0)->tci >> PPE_DRV_VLAN_PRIORITY_SHIFT;;
		qos_cfg.pcp_en = A_TRUE;
	}

	err = fal_qos_cosmap_flow_set(PPE_DRV_SWITCH_ID, 0, pf->index, &qos_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p qos(DSCP/PCP) mapping configuration failed for flow: %p", pcf, pf);
		return false;
	}

	ppe_drv_trace("%p qos(DSCP/PCP) mapping configuration done for flow: %p", pcf, pf);
	return true;
}

/*
 * ppe_drv_flow_v4_qos_clear()
 *	Return service code required for this flow.
 */
bool ppe_drv_flow_v4_qos_clear(struct ppe_drv_flow *pf)
{
	sw_error_t err;
	fal_qos_cosmap_t qos_cfg = {0};

	err = fal_qos_cosmap_flow_set(PPE_DRV_SWITCH_ID, 0, pf->index, &qos_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p qos clear configuration failed for flow", pf);
		return false;
	}

	ppe_drv_trace("%p qos clear configuration done for flow", pf);
	return true;
}

#ifdef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
/*
 * ppe_drv_flow_v4_flow_cookie48b_get()
 *	fill the flow cookie information
 */
static bool ppe_drv_flow_v4_flow_cookie48b_get(struct ppe_drv_v4_conn_flow *pcf, fal_flow_qos_t *flow_qos)
{
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	bool wifi_qos_en = false;
	uint8_t wifi_qos = 0;
	uint16_t stream_id;
	a_uint64_t tmp = 0;
	uint16_t ast_info;
	bool ret = true;
	uint8_t low_bit = 0;
#ifdef PPE_DRV_GRETAP_MAPT_SINGLEPASS
	uint32_t xlate_dest_ip;
	struct ppe_drv_port *tx_port, *rx_port;
	struct ppe_drv_tun *ptun_tx, *ptun_rx;
#endif

	if (!ppe_drv_flow_v4_wifi_qos_get(pcf, &wifi_qos, &wifi_qos_en)) {
		memset(&flow_qos->cookie_48b, 0, sizeof(flow_qos->cookie_48b));
		ppe_drv_warn("%p: failed to obtain wifi qos", pcf);
		return false;
	}

	switch (tree_id_data->type) {
		case PPE_DRV_TREE_ID_TYPE_WIFI8_PPEDS:
			if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_WIFI_AST_VALID)) {
				ast_info = pcf->fl_mdata.tree_id_data.wifi_metadata.ast_info;;
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, ast_info);
			}

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_WIFI_STREAM_ID_VALID)) {
				stream_id = pcf->fl_mdata.tree_id_data.wifi_metadata.stream_id;
				PPE_DRV_FLOW_COOKIE_STREAM_ID_VALID_SET(stream_id);
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_LOW, stream_id);
			}

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			ppe_drv_trace("cookie_48b %llx\n", tmp);
			break;

		case PPE_DRV_TREE_ID_TYPE_NONE:
#ifdef PPE_DRV_GRETAP_MAPT_SINGLEPASS
			/*
			 * For MAP-T encap (CE->BR) the v4 DIP must be packed into the
			 * flow cookie so HW can reconstruct the IPv6 destination address
			 * via SRC2=FLOW_COOKIE. Guard on TX port being MAP-T so the
			 * BR->CE decap direction (DNAT, no encap) is excluded.
			 * low 16b of DIP -> cookie[15:0], high 16b -> cookie_ext[15:0].
			 */
			tx_port = ppe_drv_v4_conn_flow_tx_port_get(pcf);
			rx_port = ppe_drv_v4_conn_flow_rx_port_get(pcf);
			ptun_tx = tx_port ? ppe_drv_port_tun_get(tx_port) : NULL;
			ptun_rx = rx_port ? ppe_drv_port_tun_get(rx_port) : NULL;

			if (ptun_tx && (ptun_tx->th.type == PPE_DRV_TUN_CMN_CTX_TYPE_MAPT) &&
					ptun_rx && (ptun_rx->th.type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP)) {
				xlate_dest_ip = ppe_drv_v4_conn_flow_xlate_dest_ip_get(pcf);

				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_LOW, xlate_dest_ip & 0xffff);
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, (xlate_dest_ip >> 16) & 0xffff);
				memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
				ppe_drv_trace("mapt cookie_48b %llx xlate_dest_ip %pI4\n", tmp, &xlate_dest_ip);
				flow_qos->qos_valid = A_TRUE;
				flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
				ptun_tx->is_gretap_mapt = true;
				break;
			}

			/*
			 * This change is primarily required for GRETAP over WLAN. When the tx port
			 * is GRETAP, WiFi HW needs the AST index to deliver to the correct client.
			 * Since we are not filling the flow cookie here, setting
			 * BIT(PPE_DRV_FLOW_COOKIE_GRE_AST_FROM_PKT_SHIFT) in cookie[15:0] signals
			 * WiFi HW to read the AST index directly from the packet rather than from
			 * the flow cookie.
			 */
			if (ptun_tx && ptun_tx->th.type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
				tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_LOW, BIT(PPE_DRV_FLOW_COOKIE_GRE_AST_FROM_PKT_SHIFT));
				memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
				ppe_drv_trace("gre cookie_48b %llx\n", tmp);
				flow_qos->qos_valid = A_TRUE;
				flow_qos->type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
				break;
			}
#endif
                        low_bit = (tree_id_data->info.value >> 16) & 0xFF;
                        tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tree_id_data->info.value & 0xFFFF);
                        tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, (low_bit | (tree_id_data->type << 4)));

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			break;

		case PPE_DRV_TREE_ID_TYPE_WIFI_TID:
		case PPE_DRV_TREE_ID_TYPE_UDP_ST:
		case PPE_DRV_TREE_ID_TYPE_SCS:
                        low_bit = (tree_id_data->info.value >> 16) & 0xFF;
                        tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tree_id_data->info.value & 0xFFFF);
                        tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, (low_bit | (tree_id_data->type << 4)));

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			break;

		case PPE_DRV_TREE_ID_TYPE_SAWF:
		case PPE_DRV_TREE_ID_TYPE_MLO_ASSIST:
			low_bit = (tree_id_data->info.value >> 16) & 0xFF;
			memcpy(&tmp, &flow_qos->cookie_48b, sizeof(tmp));

			tmp &= ~FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H;
			tmp &= ~FAL_FLOW_COOKIE_48BIT_MIDDLE;

			if (pcf->tx_port) {
				if ((pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_DS) &&
				    ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_WIFI_AST_VALID)) {
					tmp &= ~FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L;
					ast_info = pcf->fl_mdata.tree_id_data.wifi_metadata.ast_info;
					tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, ast_info);
				} else if (pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_ACTIVE_VP) {
					tmp &= ~FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L;
					tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_16BIT_L, tree_id_data->info.value & 0xFFFF);
				}
			}

			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, (low_bit | (tree_id_data->type << 4)));

			flow_qos->qos_valid = wifi_qos_en;
			tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_MIDDLE, wifi_qos);

			memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
			break;

		default:
			ppe_drv_warn("Invalid tree_id_type : (%u)", tree_id_data->type);
			ret = false;
			break;
	}

	/*
	 * If the flow contains the host Qdisc information, fill the respective QDISC
	 * valid bit in the tree id
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_VP_HOST_QDISC_INFO_VALID)) {
		tmp |= FIELD_PREP(FAL_FLOW_COOKIE_48BIT_HIGH_8BIT_H, 0x80);
		memcpy(&flow_qos->cookie_48b, &tmp, sizeof(flow_qos->cookie_48b));
		flow_qos->qos_valid = wifi_qos_en;
		ret = true;
	}

	return ret;
}

#elif defined(PPE_DRV_FLOW_COOKIE_40B_SUPPORT)
/*
 * ppe_drv_flow_v4_flow_cookie40b_get()
 *      Find the tree ID associated with a flow
 */
static bool ppe_drv_flow_v4_flow_cookie40b_get(struct ppe_drv_v4_conn_flow *pcf, uint8_t *cookie_40b)
{
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	bool ret = true;

	switch (tree_id_data->type) {
		case PPE_DRV_TREE_ID_TYPE_NONE:
			cookie_40b[0] = tree_id_data->info.value & 0xFF;
			cookie_40b[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			cookie_40b[2] = (tree_id_data->info.value & 0xFF0000) >> 16;
			cookie_40b[3] = (tree_id_data->info.value & 0xFF000000) >> 24;

			/*
			 * Currently info.value is a 32 bit metadata
			 * Hence storing the 8 bits with 0
			 */
			cookie_40b[4] = 0;

			break;

		case PPE_DRV_TREE_ID_TYPE_WIFI_TID:
		case PPE_DRV_TREE_ID_TYPE_SCS:
			cookie_40b[4] = tree_id_data->type << 3;

			break;

		case PPE_DRV_TREE_ID_TYPE_SAWF:
		case PPE_DRV_TREE_ID_TYPE_MLO_ASSIST:
			/*
			 * type(5 bits) | reserved(17bits) | data(18 bits)
			 */
			cookie_40b[0] = tree_id_data->info.value & 0xFF;
			cookie_40b[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			cookie_40b[2] = (tree_id_data->info.value & 0x30000) >> 16;
			cookie_40b[3] = 0;
			cookie_40b[4] = tree_id_data->type << 3;

			break;

		default:
			ppe_drv_warn("Invalid tree_id_type : (%u)", tree_id_data->type);
			ret = false;;
	}

	/*
	 * If the flow contains the host Qdisc information, fill the respective QDISC
	 * valid bit in the tree id
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_HOST_ASSIST_QDISC)) {
		cookie_40b[4] |= 0x80;;
		ret = true;
	}

	return ret;
}
#else
/*
 * ppe_drv_flow_v4_tree_id_get()
 *	Find the tree ID associated with a flow
 */
static bool ppe_drv_flow_v4_tree_id_get(struct ppe_drv_v4_conn_flow *pcf, uint8_t *tree_id)
{
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	bool ret = true;

	switch (tree_id_data->type) {
		case PPE_DRV_TREE_ID_TYPE_NONE:
			tree_id[0] = tree_id_data->info.value & 0xFF;
			tree_id[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			tree_id[2] = (tree_id_data->info.value & 0xFF0000) >> 16;
			break;

		case PPE_DRV_TREE_ID_TYPE_WIFI_TID:
		case PPE_DRV_TREE_ID_TYPE_SCS:
			tree_id[0] = 0;
			tree_id[1] = 0;
			tree_id[2] = tree_id_data->type << 4;
			break;

		case PPE_DRV_TREE_ID_TYPE_SAWF:
		case PPE_DRV_TREE_ID_TYPE_MLO_ASSIST:
			/*
			 * type(4 bits) | reserved (2 bits) | data (18 bits)
			 */
			tree_id[0] = tree_id_data->info.value & 0xFF;
			tree_id[1] = (tree_id_data->info.value & 0xFF00) >> 8;
			tree_id[2] = (tree_id_data->info.value & 0x30000) >> 16;
			tree_id[2] |= tree_id_data->type << 4;
			break;

		case PPE_DRV_TREE_ID_TYPE_UDP_ST:
			tree_id[0] = (tree_id_data->info.value & PPE_DRV_UDP_ST_TREE_ID_0_MASK) >> PPE_DRV_UDP_ST_TREE_ID_0_SHIFT;
			tree_id[1] = (tree_id_data->info.value & PPE_DRV_UDP_ST_TREE_ID_1_MASK) >> PPE_DRV_UDP_ST_TREE_ID_1_SHIFT;
			tree_id[2] = (tree_id_data->info.value & PPE_DRV_UDP_ST_TREE_ID_2_MASK) >> PPE_DRV_UDP_ST_TREE_ID_2_SHIFT;
			tree_id[2] |= tree_id_data->type << PPE_DRV_UDP_ST_TREE_ID_DATA_TYPE_SHIFT;
			break;

		default:
			ppe_drv_warn("Invalid tree_id_type : (%u)", tree_id_data->type);
			ret = false;
	}

	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_HOST_ASSIST_QDISC)) {
		tree_id[2] |= 0x80;
		ret = true;
	}

	return ret;
}
#endif

/*
 * ppe_drv_flow_v4_vpn_id_get()
 *	Find the tree ID associated with a flow
 */
static bool ppe_drv_flow_v4_vpn_id_get(struct ppe_drv_v4_conn_flow *pcf, uint32_t *vpn_id)
{
#ifdef NSS_PPE_SIXTH_TUPLE
	int sixth_tuple_mode = ppe_drv_get_sixth_tuple_mode();

	if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VSI) {
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			struct ppe_drv_vsi *vsi = ppe_drv_v4_conn_flow_eg_top_vsi_get(pcf);
			if (!vsi) {
				ppe_drv_warn("No bridge vsi");
				return false;
			}
			*vpn_id = FAL_VPN_ID_FROM_VSI(vsi->index);
		} else {
			struct ppe_drv_iface *l3_if = ppe_drv_v4_conn_flow_in_l3_if_get(pcf);
			if (!l3_if || !l3_if->l3) {
				ppe_drv_warn("No valid l3 iface");
				return false;
			}
			*vpn_id = FAL_VPN_ID_FROM_INTF(l3_if->l3->l3_if_index);
		}
	} else if (sixth_tuple_mode == PPE_DRV_SIXTH_TUPLE_MODE_VLAN) {
		uint8_t vlan_hdr_cnt = ppe_drv_v4_conn_flow_ingress_vlan_cnt_get(pcf);
		*vpn_id = PPE_DRV_VLAN_NOT_CONFIGURED;
		if (vlan_hdr_cnt > 0) {
			struct ppe_drv_vlan *vlan_hdr = ppe_drv_v4_conn_flow_ingress_vlan_get(pcf, 0);
			if (!vlan_hdr) {
				ppe_drv_warn("No valid vlan hdr");
				return false;
			}
			*vpn_id = FAL_VPN_ID_FROM_VLAN(vlan_hdr->tci & PPE_DRV_VLAN_ID_MASK);
		}
	}
#endif
	return true;
}

/*
 * ppe_drv_v4_is_flow_is_non_udp()
 *      Return 1 if the proto type is non UDP.
 */
static int ppe_drv_v4_is_flow_is_non_udp(struct ppe_drv_v4_conn_flow *pcf)
{
	if (ppe_drv_v4_conn_flow_match_protocol_get(pcf) == IPPROTO_UDP)
		return 0;

	return 1;
}

/*
 * ppe_drv_flow_v4_wifi_qos_get()
 *	Find the WIFI QOS associated with a flow
 */
static bool ppe_drv_flow_v4_wifi_qos_get(struct ppe_drv_v4_conn_flow *pcf, uint8_t *wifi_qos, bool *wifi_qos_en)
{
	bool flow_override_mode = true;
	bool flow_queue_id = false;

	/*
	 * If SAWF metadata is valid, set 6 bit MSDUQ in wifi_qos field (bits 0-5).
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_METADATA_TYPE_WIFI_INFO)) {
		/*
		 * MSDUQ representation in wifi_qos field is:
		 * --------------------------------------------------------------------------------------
		 * |	Who Classify (2 bits)	|	Flow override (1 bit)	|	TID (3 bits)	|
		 * --------------------------------------------------------------------------------------
		 */
		*wifi_qos = pcf->fl_mdata.wifi_qos;
		*wifi_qos_en = true;

		/*
		 * In case of DS mode, rearrange the MSDUQ representation in wifi qos field to be aligned with TCL descriptor.
		 */
		if (!ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_VP_VALID) &&
				(pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_DS)) {
			flow_override_mode = ppe_drv_flow_override_mode_get(&pcf->fl_mdata.wifi_qos);

			/*
			 * Get flow queue id type as UDP or TCP.
			 */
			flow_queue_id = ppe_drv_v4_is_flow_is_non_udp(pcf);

			ppe_drv_flow_ds_wifi_qos_set(wifi_qos, wifi_qos_en, &pcf->fl_mdata.wifi_qos, flow_override_mode, flow_queue_id);
			ppe_drv_trace("WiFi_QoS configured in DS descriptor is: 0x%x\n", *wifi_qos);
		}

		ppe_drv_trace("For User type: %u, WiFi_QoS initially: 0x%x and WiFi_QoS configured: 0x%x", pcf->tx_port->user_type, pcf->fl_mdata.wifi_qos, *wifi_qos);
	}

	/*
	 * If the flow contains the host Qdisc information, set the WIFI QoS enable
	 * bit to indicate the PPE to pass the tree-id field (which contains the
	 * QDISC valid bit) in the EDMA Rx secondary descriptor
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_HOST_ASSIST_QDISC)) {
		*wifi_qos_en = true;
	}

	/*
	 * If the vp type is active or the flow is active vp, set the WIFI QoS enable
	 * to indicate the PPE to set the WIFI QoS bit in the EDMA rx descriptor
	 * so that EDMA can fill the MLO assit data in skb->mark which is needed by default
	 * for wlan vp flows.
	 */
	if ((pcf->tx_port && (pcf->tx_port->user_type == PPE_DRV_PORT_USER_TYPE_ACTIVE_VP)) ||
	    ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_VP_VALID)) {
		*wifi_qos_en = true;
	}

	/*
	 * If the flow contains the SW Metadata to be valid, set the WIFI Qos enable
	 * to indicate the PPE to set the WIFI Qos bit in the EDMA rx descriptor
	 * to retrieve the flow_index for the flow.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_SW_MDATA_VALID)) {
		*wifi_qos_en = true;
	}

	/*
	 * If the flow metadata tree-id type is UDP-ST, set the WIFI Qos enable
	 * to indicate the PPE to set the WIFI Qos bit in the EDMA rx descriptor
	 * to retrieve the rule id of the udp speedtest flow.
	 */
	if (pcf->fl_mdata.tree_id_data.type == PPE_DRV_TREE_ID_TYPE_UDP_ST) {
		*wifi_qos_en = true;
	}
	return true;
}

#ifdef PPE_DRV_FLOW_TBL_POLICER
/*
 * ppe_drv_flow_v4_policer_get()
 *	Find the Policer get associated with a flow
 */
static bool ppe_drv_flow_v4_policer_get(struct ppe_drv_v4_conn_flow *pcf, uint32_t *policer_index, a_bool_t *policer_valid)
{
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_POLICER_VALID)) {
		*policer_index = pcf->policer_hw_id;
		*policer_valid = A_TRUE;
	}

	return true;
}
#endif

/*
 * ppe_drv_flow_v4_host_qdisc_info_del()
 *	API to delete v4 based host Qdisc information for the particular flow index
 */
void ppe_drv_flow_v4_host_qdisc_info_del(struct ppe_drv_v4_conn_flow *pcf, uint32_t flow_idx)
{
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_HOST_ASSIST_QDISC)) {
		ppe_drv_flow_host_qdisc_info_del(flow_idx);
	}
}

/*
 * ppe_drv_flow_v4_host_qdisc_info_set()
 *	API to set v4 based host Qdisc information for the particular flow index
 */
static void ppe_drv_flow_v4_host_qdisc_info_set(struct ppe_drv_v4_conn_flow *pcf, uint32_t flow_idx)
{
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_VP_HOST_QDISC_INFO_VALID) ||
			ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_HOST_ASSIST_QDISC)) {
		ppe_drv_flow_host_qdisc_info_set(&pcf->qdisc_info, flow_idx);
	}
}

/*
 * ppe_drv_flow_v4_pri_profile_get()
 *	Apply appropiate pri_profile for this flow.
 */
bool ppe_drv_flow_v4_pri_profile_get(struct ppe_drv_v4_conn_flow *pcf, struct ppe_drv_port *pp, uint8_t *pri_profile)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *reverse_flow = NULL;
	struct ppe_drv_v4_5tuple reverse_tuple;
	uint8_t evp_pri_profile = PPE_DRV_PRI_PROF_DEFAULT;
	uint8_t enq_vp;
	uint8_t next_core;

	/*
	 * Add ppe-ds flow with pri profile.
	 * The classifier provides the PPE-DS node data from Wi-Fi driver.
	 * The node metadata could be used to get the enqueue vport and its pri profile
	 * which is programmed into PPE flow rule.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_WIFI_DS)) {

		evp_pri_profile = ppe_drv_port_metadata_to_pri_prof_internal(pcf->wifi_rule_ds_metadata);
		if (evp_pri_profile == PPE_DRV_PORT_ENQ_VP_PRI_PRFL_INVALID) {
			ppe_drv_warn("%p: Enqueue vport pri profile invalid:%d mdata:%d", pcf, evp_pri_profile, pcf->wifi_rule_ds_metadata);
			return false;
		}
		*pri_profile = evp_pri_profile;
		ppe_drv_trace("%p: Pri profile: %d metadata:%d\n", pcf, evp_pri_profile, pcf->wifi_rule_ds_metadata);
		return true;
	}

	/*
	 * Reversing the current flow because in case of RFS in eth2eth flows,
	 * if we map the two unidirectional entries of a flow to different cores then,
	 * we see an imbalance in the CPU utilisation,
	 * hence keeping the two entries of a flow in a single core.
	 */
	reverse_tuple.flow_ip = pcf->xlate_dest_ip;
	reverse_tuple.return_ip = pcf->xlate_src_ip;
	reverse_tuple.flow_ident = pcf->xlate_dest_ident;
	reverse_tuple.return_ident = pcf->xlate_src_ident;
	reverse_tuple.protocol = pcf->match_protocol;
	reverse_tuple.flow_rule_id = pcf->conn ? pcf->conn->flow_rule_id : 0;

	/*
	 * This is when its a RFS flow, pcf flag is set as PPE_DRV_V4_CONN_FLAG_PASSIVE_FLOW.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_PASSIVE_FLOW)) {
		/*
		 * This is when WiFi VAP has a passive VP allocated.
		 */
		if ((ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_PASSIVE_WLAN_FLOW) ||
		     ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_UDP_ST_FLOW)) && p->rfs.passive_vp_enable)
		{
			if (!pp) {
				ppe_drv_trace("pcf %p: Port not allocated in PPE for TX dev.\n", pcf);
				return false;
			}
			next_core = __builtin_ffs(pp->shadow_core_mask) - 1;
			pp->shadow_core_mask &= ~(1 << next_core);
			enq_vp = p->rfs.core2enq_vp[next_core];
			evp_pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
			if (!pp->shadow_core_mask) {
				pp->shadow_core_mask = pp->core_mask;
			}

			*pri_profile = evp_pri_profile;
			ppe_drv_trace("pcf %p: Pri profile: %u enq_vp: %u next_core = %u\n", pcf, evp_pri_profile, enq_vp, next_core);
			return true;

		}

		/*
		 * Get the reverse flow table entry if present.
		 */
		reverse_flow = ppe_drv_flow_v4_get(&reverse_tuple);
		if (reverse_flow) {
			*pri_profile = reverse_flow->pri_profile;
			pcf->rfs_core = reverse_flow->pcf.v4->rfs_core;
			ppe_drv_trace("pcf %p: Pri profile same as reverse flow: %u\n", pcf, evp_pri_profile);
			return true;
		}
		if (!*(pcf->fl_mdata.shadow_coremask)) {
			*(pcf->fl_mdata.shadow_coremask) = *(pcf->fl_mdata.coremask);
		}
		next_core = __builtin_ffs(*(pcf->fl_mdata.shadow_coremask)) - 1;
		*(pcf->fl_mdata.shadow_coremask) &= ~(1 << next_core);
		enq_vp = p->rfs.core2enq_vp[next_core];
		evp_pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
		if (evp_pri_profile == PPE_DRV_PORT_ENQ_VP_PRI_PRFL_INVALID) {
			ppe_drv_warn("%p: Enqueue vport pri profile invalid:%d core:%d", pcf, evp_pri_profile, next_core);
			return false;
		}
		pcf->rfs_core = next_core;
		*pri_profile = evp_pri_profile;
		ppe_drv_trace("pcf %p: Pri profile: %u enq_vp: %u next_core = %u\n", pcf, evp_pri_profile, enq_vp, next_core);
		return true;
	}

	if (!pp) {
		ppe_drv_trace("pcf %p: Port not allocated in PPE for TX dev.\n", pcf);
		return false;
	}

	/*
	 * For the flows in different usertype falling back.
	 */
	if (ppe_drv_is_wlan_vp_port_type(pp->user_type)) {
		if (pp->core_mask) {
			next_core = __builtin_ffs(pp->shadow_core_mask) - 1;
			BUG_ON(next_core >= NR_CPUS);
			pp->shadow_core_mask &= ~(1 << next_core);
#if defined(NSS_PPE_DRV_HW_GRO)
        		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_HW_GRO))
				enq_vp = p->gro_ctx.gro_info.core2enq_vp[next_core];
			else
#endif
				enq_vp = p->rfs.core2enq_vp[next_core];
			evp_pri_profile = ppe_drv_port_enq_vp_to_pri_prof(enq_vp);
			if (!pp->shadow_core_mask) {
				pp->shadow_core_mask = pp->core_mask;
			}
		} else {
			ppe_drv_warn("%p: invalid core mask for give user type: %u\n", pcf, pp->user_type);
		}
	}

	*pri_profile = evp_pri_profile;
	ppe_drv_trace("pcf %p: Pri profile: %u enq_vp: %u next_core = %u\n", pcf, evp_pri_profile, enq_vp, next_core);
	return true;
}

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
/*
 * ppe_drv_flow_v4_check_hierarchy_speed_mismatch()
 * 	Check the speed mismatch between interfaces.
 */
static inline bool ppe_drv_flow_v4_check_hierarchy_speed_mismatch(struct ppe_drv *p, struct ppe_drv_v4_conn_flow *pcf, struct ppe_drv_port *pp)
{
	struct ppe_drv_lpbk_buffer_map_entry *map_entry;
	struct ppe_drv_v4_conn *cn = ppe_drv_v4_conn_flow_conn_get(pcf);
	struct ppe_drv_v4_conn_flow *pcr = NULL;
	bool is_speed_mismatch = false;

	if (cn) {
		pcr = (pcf == &cn->pcf) ? &cn->pcr : &cn->pcf;
	}

	if (!list_empty(&p->lpbk_buffer_map) && pcf->rx_port && pcf->rx_port->dev) {
		list_for_each_entry(map_entry, &p->lpbk_buffer_map, list) {
			bool ingress_match;
			bool egress_match;

			if (map_entry->ingress_dev != pcf->rx_port->dev ||
					map_entry->egress_dev != pp->dev) {
				continue;
			}

			/*
			 * PPPoE check
			 */
			if (map_entry->ingress_pppoe) {
				if (!pcr || !ppe_drv_v4_conn_flow_flags_check(pcr, PPE_DRV_V4_CONN_FLOW_FLAG_PPPOE_FLOW) ||
						pcr->pppoe_session_id != map_entry->pppoe_session_id ||
						memcmp(pcr->pppoe_server_mac, map_entry->pppoe_server_mac, ETH_ALEN)) {
					continue;
				}
			} else if (map_entry->egress_pppoe) {
				if (!ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_PPPOE_FLOW) ||
						pcf->pppoe_session_id != map_entry->pppoe_session_id ||
						memcmp(pcf->pppoe_server_mac, map_entry->pppoe_server_mac, ETH_ALEN)) {
					continue;
				}
			}

			/*
			 * Ingress VLAN check
			 */
			if (map_entry->ingress_vlan_cnt == 0) {
				ingress_match = true;
			} else if (map_entry->ingress_vlan_cnt == 1) {
				ingress_match = (pcf->ingress_vlan_cnt >= 1) &&
					((pcf->ingress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->ingress_vlan_id[0]);
			} else {
				ingress_match = (pcf->ingress_vlan_cnt >= 2) &&
					((pcf->ingress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->ingress_vlan_id[0]) &&
					((pcf->ingress_vlan[1].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->ingress_vlan_id[1]);
			}

			/*
			 * Egress VLAN check
			 */
			if (map_entry->egress_vlan_cnt == 0) {
				egress_match = true;
			} else if (map_entry->egress_vlan_cnt == 1) {
				egress_match = (pcf->egress_vlan_cnt >= 1) &&
					((pcf->egress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->egress_vlan_id[0]);
			} else {
				egress_match = (pcf->egress_vlan_cnt >= 2) &&
					((pcf->egress_vlan[0].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->egress_vlan_id[0]) &&
					((pcf->egress_vlan[1].tci & PPE_DRV_VLAN_ID_MASK) ==
					 map_entry->egress_vlan_id[1]);
			}

			if (ingress_match && egress_match) {
				is_speed_mismatch = true;
				break;
			}
		}
	}

	return is_speed_mismatch;
}

#endif

/*
 * ppe_drv_flow_v4_service_code_get()
 *	Return service code required for this flow.
 */
bool ppe_drv_flow_v4_service_code_get(struct ppe_drv_v4_conn_flow *pcf, struct ppe_drv_port *pp, uint8_t *scp)
{
	struct ppe_drv *p __maybe_unused = ppe_drv_gbl;
	struct ppe_drv_port *port_rx = pcf->rx_port;
	ppe_drv_sc_t service_code = *scp;
	ppe_drv_sc_t sc = PPE_DRV_SC_NONE;
	ppe_drv_ret_t ret __maybe_unused;
#ifdef PPE_DRV_DDRQ_ENABLE
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_DDRQ_PASSTHROUGH_DIS)) {
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_ENCAP)) {
			*scp = PPE_DRV_SC_DDRQ_LP_SC_IPSEC_ENCAP;
		} else if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_DECAP)) {
			*scp = PPE_DRV_SC_DDRQ_LP_SC_IPSEC_DECAP;
		} else if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW) &&
				ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_DDRQ_VLAN_IF)) {
			*scp = PPE_DRV_SC_DDRQ_LP_SC_BRIDGE;
		} else {
			*scp = PPE_DRV_SC_DDRQ_LP_SC;
		}
		ppe_drv_warn("Flow has the DDRQ PT disable flag set, setting SC:%d for it\n", *scp);
		return true;
	}
#endif

#ifdef PPE_TUNNEL_ENABLE
#if defined(PPE_LOOPBACK_RING_SUPPORT)
#ifndef PPE_DRV_GRETAP_MAPT_SINGLEPASS
	ret = ppe_drv_tun_gretap_to_mapt_sc(ppe_drv_v4_conn_flow_tx_port_get(pcf), ppe_drv_v4_conn_flow_rx_port_get(pcf), &service_code);
	if (ret == PPE_DRV_RET_GRETAP_TO_MAPT_FLOW_ADD) {
		*scp = service_code;
		return true;
	} else if (ret == PPE_DRV_RET_GRETAP_TO_MAPT_FLOW_ADD_FAIL) {
		return false;
	}
#endif
#endif
#endif

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	if (p->loopback_ring_info.enabled &&
			(p->loopback_ring_info.ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_DDR_BUFFER) &&
			pp && pp->dev) {
		bool is_speed_mismatch = ppe_drv_flow_v4_check_hierarchy_speed_mismatch(p, pcf, pp);

		/*
		 * check if phydev speed are asymmetric or not.
		 */
		if (!is_speed_mismatch && p->ddr_buffer_auto_en)
			is_speed_mismatch = ppe_drv_phydev_speed_compare(pcf->rx_port, pp);

		/*
		 * set service code to use loopback ring.
		 */
		if (is_speed_mismatch) {
			if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER)) {
				return false;
			}

			*scp = service_code;
			return true;
		}
	}
#endif
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	ret = ppe_drv_veip_gw_port_sc(ppe_drv_v4_conn_flow_tx_port_get(pcf), &sc);
	if (ret == PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD) {
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
		*scp = service_code;
		return true;
	} else if (ret == PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD_FAIL) {
		ppe_drv_warn("%p: VEIP US v4 flow add failed ret=%d, veip_sc=%u\n", pcf, ret, sc);
		return false;
	}
#endif
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_PRIORITY_PPE_ASSIST)) {
		/*
		 * Service code to set priority for PPE assisted flows.
		 * No other service code is supported if priority assist is active.
		 */
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NOEDIT_PRIORITY_SET)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
						pcf, service_code, PPE_DRV_SC_NOEDIT_PRIORITY_SET);
			return false;
		}

		*scp = service_code;
		return true;
	}

	/*
	 * Multicast RFS flows are redirected to the CPU core assigned.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_MCAST_RFS_FLOW)
#ifdef NSS_PPE_PON_SUPPORT
				|| (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW) &&
					ppe_drv_is_pon_enable_internal())
#endif
		  ) {
			ppe_drv_sc_t core_sc;

			switch (pcf->rfs_core) {
				case 0:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE0;
					break;
				case 1:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE1;
					break;
				case 2:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE2;
					break;
				case 3:
					core_sc = PPE_DRV_SC_NOEDIT_REDIR_CORE3;
					break;
				default:
					ppe_drv_warn("%p: invalid core %u for mcast RFS flow", pcf, pcf->rfs_core);
					return false;
			}

			if (!ppe_drv_sc_check_and_set(&service_code, core_sc)) {
				ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
						pcf, service_code, core_sc);
				return false;
			}

			*scp = service_code;
			return true;
		}
	}

	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		/*
		 * Service code to be mark for RFS PPE assisted flows so PPE doesn't edit packets.
		 */
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NOEDIT_RFS_RULE)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u", pcf, service_code, PPE_DRV_SC_NOEDIT_RFS_RULE);
			return false;
		}

		*scp = service_code;
		return true;
	}

	/*
	 * Service code to enable PPE to bypass packet header editing and forward them unmodified.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_NO_EDIT_RULE)) {
		/*
		 * Service code to set noedit rule.
		 */
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_NOEDIT_RULE)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
						pcf, service_code, PPE_DRV_SC_NOEDIT_RULE);
			return false;
		}

		*scp = service_code;
		return true;
	}

	/*
	 * If the egress port has MPSK support and it is a bridge flow,
	 * bypass the egress vlan table for the same.
	 */
	if (ppe_drv_port_flags_check(pp, PPE_DRV_PORT_FLAG_MPSK)) {
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_VP_MPSK)) {
				ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
							pcf, service_code, PPE_DRV_SC_VP_MPSK);
				return false;
			}
		}

		*scp = service_code;
		return true;
	}

	/*
	 * Get the service code for the flow according to the flow type
	 * and precedence of these features (like DS flows, policer/ACL based service code, etc)
	 */
	if ((ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_POLICER_VALID) ||
		ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_ACL_VALID)) && (pcf->acl_sc != PPE_DRV_SC_NONE)) {
		sc = pcf->acl_sc;
	} else if (pp->user_type == PPE_DRV_PORT_USER_TYPE_DS) {
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_VP_VALID)) {
			if (pp->core_mask) {
				sc = PPE_DRV_SC_EDIT_RFS_RULE;
			} else {
				sc = PPE_DRV_SC_VP_RPS;
			}
		} else if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_OFFLOAD_DISABLED) ||
					ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_PASSIVE_FLOW)) {
			if (pp->core_mask) {
				sc = PPE_DRV_SC_NOEDIT_RFS_RULE;
			} else {
				ppe_drv_warn("%p: invalid core mask for DS user type", pcf);
				return false;
			}
		}
	} else if ((pp->user_type == PPE_DRV_PORT_USER_TYPE_ACTIVE_VP) && pp->core_mask) {
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_OFFLOAD_DISABLED) ||
					ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_PASSIVE_FLOW)) {
			sc = PPE_DRV_SC_NOEDIT_RFS_RULE;
		} else {
			sc = PPE_DRV_SC_EDIT_RFS_RULE;
		}
	} else if ((pp->user_type == PPE_DRV_PORT_USER_TYPE_PASSIVE_VP)) {
		sc = PPE_DRV_SC_NOEDIT_RFS_RULE;
	}

	if (is_vlan_dev(port_rx->dev) && (port_rx->type == PPE_DRV_PORT_VIRTUAL) && (netif_is_bridge_port(port_rx->dev)) &&
	    !ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
		sc = (port_rx->is_fdb_learn_enabled && pp->is_fdb_learn_enabled) ?
			PPE_DRV_SC_VLAN_FILTER_BYPASS : PPE_DRV_SC_SPF_BYPASS;
	}

	if (sc != PPE_DRV_SC_NONE) {
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: flow requires multiple service code, existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
	}

	/*
	 * If the flow is flagged for inline IPsec, assign the EIP decap service code.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_INLINE_IPSEC)) {
		if (!ppe_drv_sc_check_and_set(&service_code, PPE_DRV_SC_IPSEC_PPE2EIP_DECAP)) {
			ppe_drv_warn("%p: EIP flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_IPSEC_PPE2EIP_DECAP);
			return false;
		}
	}

	/*
	 * If the flow is flagged for inline IPsec ENCAP direction,
	 * assign the EIP encap service code.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_ENCAP)) {
		sc = PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: EIP flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP);
			return false;
		}
	}

	/*
	 * SC required to accelerate full inline EIP DECAP flows.
	 * It enables ACL matching based on the SPI field in the packet
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_FULL_INLINE_IPSEC_DECAP)) {
		sc = PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: EIP flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH);
			return false;
		}
	}

	/*
	 * Set service code to redirect packet to host for qdisc processing.
	 */
	if ((sc == PPE_DRV_SC_NONE) && (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_HOST_ASSIST_QDISC))) {
		sc = PPE_DRV_SC_HOST_QOS;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: Host assisted QoS flow required multiple service codes, existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
	}

	/*
	 * SC required when its a bridge flow or fdb learning is disabled.
	 */
	if ((sc == PPE_DRV_SC_NONE) &&
			(ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW) || !port_rx->is_fdb_learn_enabled)) {
		sc = PPE_DRV_SC_VLAN_FILTER_BYPASS;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: Bridge flow requires multiple service codes existing:%u new:%u",
					pcf, service_code, sc);
			return false;
		}
	}

	if ((sc == PPE_DRV_SC_NONE) &&
	    (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_BRIDGE_VLAN_NETDEV))) {
		sc = PPE_DRV_SC_VLAN_FILTER_BYPASS;
		if (!ppe_drv_sc_check_and_set(&service_code, sc)) {
			ppe_drv_warn("%p: service code %d update failed in VLAN over bridge sc %d\n", pcf, service_code,
				     sc);
			return false;
		}
	}

	*scp = service_code;
	return true;
}

/*
 * ppe_drv_flow_del()
 *	Delete a flow entry.
 */
bool ppe_drv_flow_del(struct ppe_drv_flow *pf)
{
	fal_flow_entry_t flow_cfg = {0};
	sw_error_t err;

	/*
	 * Delete the flow
	 */
	flow_cfg.entry_id = pf->index;
	flow_cfg.entry_type = pf->entry_type;
	err = fal_flow_entry_del(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_INDEX, &flow_cfg);
	if (err != SW_OK) {
		ppe_drv_trace("%p: flow entry deletion failed", pf);
		return false;
	}

	/*
	 * Update the shadow copy
	 */
	pf->flags = 0;
	pf->type = 0;
	pf->entry_type = 0;

	/*
	 * Free the dynamically allocated flow_cfg memory
	 */
	if (pf->unidir_info.flow_cfg) {
		kfree(pf->unidir_info.flow_cfg);
		pf->unidir_info.flow_cfg = NULL;
	}
	pf->mapt_info.mapt_v6 = NULL;
	pf->mapt_info.mapt_v4 = NULL;
	pf->mapt_info.len_adjust = 0;

	if (pf->flow_cfg) {
		kfree(pf->flow_cfg);
		pf->flow_cfg = NULL;
	}

	ppe_drv_trace("%p: flow_tbl entry deleted at index: %u", pf, pf->index);
	return true;
}

/*
 * ppe_drv_flow_v4_get()
 *	Find flow table entry.
 */
struct ppe_drv_flow *ppe_drv_flow_v4_get(struct ppe_drv_v4_5tuple *tuple)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;

	flow = ppe_drv_v4_flow_hash_tuple_find(tuple, p->hash_conn_v4, false);
	if (!flow) {
		ppe_drv_trace("%p: flow get failed", tuple);
		return NULL;
	}

	ppe_drv_assert((flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already decelerated from PPE at index: %d",
			tuple, flow->index);
	ppe_drv_trace("%p: flow_tbl entry found at index: %u", tuple, flow->index);
	return flow;
}

/*
 * ppe_drv_flow_v4_get_by_dev()
 *	Find flow table entry with 5tuple and incoming device.
 */
struct ppe_drv_flow *ppe_drv_flow_v4_get_by_dev(struct ppe_drv_v4_5tuple *tuple, struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;

	flow = ppe_drv_v4_flow_hash_tuple_find_by_dev(tuple, p->hash_conn_v4, dev);
	if (!flow) {
		ppe_drv_trace("%p: flow get failed", tuple);
		return NULL;
	}

	ppe_drv_assert((flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already decelerated from PPE at index: %d",
			tuple, flow->index);
	ppe_drv_trace("%p: flow_tbl entry found at index: %u", tuple, flow->index);
	return flow;
}
/*
 * ppe_drv_v4_flow_get_ignore_rule_id()
 *	Find flow table entry ignoring rule id matched or not.
 */
struct ppe_drv_flow *ppe_drv_v4_flow_get_ignore_rule_id(struct ppe_drv_v4_5tuple *tuple)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;

	flow = ppe_drv_v4_flow_hash_tuple_find(tuple, p->hash_conn_v4, true);
	if (!flow) {
		ppe_drv_trace("%p:get flow failed(%pI4:%u->%pI4:%u [%d])", tuple,
				&tuple->flow_ip, tuple->flow_ident,
				&tuple->return_ip, tuple->return_ident,
				tuple->protocol);
		return NULL;
	}

	ppe_drv_assert((flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already decelerated from PPE at index: %d",
			tuple, flow->index);
	ppe_drv_trace("%p: flow_tbl entry found at index: %u", tuple, flow->index);
	return flow;
}

/*
 * ppe_drv_flow_v4_wlan_metadata_set()
 *	Set flow QoS information for the v4 flow.
 */
bool ppe_drv_flow_v4_wlan_metadata_set(struct ppe_drv_v4_conn_flow *pcf)
{
	fal_flow_qos_t flow_qos = {0};
	uint16_t index;
	bool wifi_qos_en = false;
	sw_error_t err;

	index = pcf->pf->index;

	err = fal_flow_qos_get(PPE_DRV_SWITCH_ID, index, &flow_qos);
	if (err != SW_OK) {
                ppe_drv_warn("%px: Failed to obtain flow qos", pcf);
                return false;
        }

#ifdef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
        /*
         * Get the flow cookie corresponding to flow.
         */
        if (!ppe_drv_flow_v4_flow_cookie48b_get(pcf, &flow_qos)) {
            ppe_drv_warn("%p: failed to obtain a valid flow cookie", pcf);
			return false;
        }
#elif defined(PPE_DRV_FLOW_COOKIE_40B_SUPPORT)
	/*
	 * Get the flow cookie corresponding to flow.
	 */
	if (!ppe_drv_flow_v4_flow_cookie40b_get(pcf, flow_qos.cookie_40b)) {
		ppe_drv_warn("%px: failed to obtain a valid flow cookie", pcf);
		return false;
	}

	flow_qos.type = FAL_FLOW_QOS_TYPE_COOKIE_40B;

#else
	/*
	 * Get the tree ID corresponding to flow.
	 */
	if (!ppe_drv_flow_v4_tree_id_get(pcf, flow_qos.tree_id)) {
		ppe_drv_warn("%px: Failed to obtain a valid tree ID", pcf);
		return false;
	}
#endif

	if (!ppe_drv_flow_v4_wifi_qos_get(pcf, &flow_qos.qos, &wifi_qos_en)) {
		ppe_drv_warn("%p: failed to obtain wifi qos %d", pcf, flow_qos.qos);
		return false;
	}

	flow_qos.qos_valid = wifi_qos_en;

	err = fal_flow_qos_set(PPE_DRV_SWITCH_ID, index, &flow_qos);
	if (err != SW_OK) {
		ppe_drv_warn("%px: failed to set wifi metadata in PPE with tree id %px and wifi qos %u",
			pcf, flow_qos.tree_id, flow_qos.qos);
		return false;
	}

	return true;
}

/*
 * ppe_drv_flow_v4_add()
 *	Add flow table entry.
 */
struct ppe_drv_flow *ppe_drv_flow_v4_add(struct ppe_drv_v4_conn_flow *pcf, struct ppe_drv_nexthop *nh,
					struct ppe_drv_host *host, bool entry_valid)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_stats_sawf_sc *sawf_sc_stats;
	struct ppe_drv_flow_tree_id_data *tree_id_data = &(pcf->fl_mdata.tree_id_data);
	fal_flow_entry_t flow_cfg = {0};
	uint32_t match_src_ip = ppe_drv_v4_conn_flow_match_src_ip_get(pcf);
	uint32_t match_dest_ip = ppe_drv_v4_conn_flow_match_dest_ip_get(pcf);
	uint32_t xlate_src_ip = ppe_drv_v4_conn_flow_xlate_src_ip_get(pcf);
	uint32_t xlate_dest_ip = ppe_drv_v4_conn_flow_xlate_dest_ip_get(pcf);
	uint32_t match_protocol = ppe_drv_v4_conn_flow_match_protocol_get(pcf);
	uint8_t vlan_hdr_cnt = ppe_drv_v4_conn_flow_egress_vlan_cnt_get(pcf);
	uint8_t service_class;
	struct ppe_drv_iface *port_if = ppe_drv_v4_conn_flow_eg_port_if_get(pcf);
	struct ppe_drv_port *pp = NULL;
	struct ppe_drv_port *pp_in = NULL;
#ifdef NSS_PPE_DRV_HW_GRO
	struct ppe_drv_port *tx_port = NULL;
	struct ppe_drv_port *rx_port = NULL;
#endif
	struct ppe_drv_flow *flow;
	bool tuple_3 = false;
#ifndef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
	bool wifi_qos_en = false;
#endif
	uint16_t xmit_mtu;
	fal_flow_entry_t *unidir_flow_cfg = NULL;
	sw_error_t err;
	struct net_device *dev_eg;
	struct net_device *dev_in;

	/*
	 * PPE port reference is not taken for priority assist / portless RFS in PPE as PPE is
	 * used only for priority queue selection. Hence port would not be valid.
	 */
	if (!ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_PRIORITY_PPE_ASSIST)) {
		if (!port_if) {
			ppe_drv_warn("%p: Invalid egress port_if", pcf);
			return NULL;
		}

		pp = ppe_drv_iface_port_get(port_if);
		if (!pp) {
			ppe_drv_warn("%p: Invalid egress port", pcf);
			return NULL;
		}
	}

	dev_eg = ppe_drv_port_to_dev(pp);

	pp_in = ppe_drv_v4_conn_flow_rx_port_get(pcf);
	if (pp_in) {
		dev_in = ppe_drv_port_to_dev(pp_in);
	}

	/*
	 * Ensure it falls under SNAT, DNAT or simple L3 routing
	 */
	if ((match_src_ip != xlate_src_ip && match_dest_ip != xlate_dest_ip)
		&& (!ipv4_is_multicast(htonl(match_dest_ip)))) {
		ppe_drv_warn("%p: both source and dest IPs are getting translated", pcf);
		return NULL;
	}

	flow_cfg.host_addr_type = PPE_DRV_HOST_LAN;
	flow_cfg.host_addr_index = host->index;
	flow_cfg.deacclr_en = A_FALSE;
	flow_cfg.invalid = !entry_valid;
	flow_cfg.sevice_code = PPE_DRV_SC_NONE;

#if defined(NSS_PPE_DRV_HW_GRO)
	/*
	 * Enable HW GRO only if:
	 * 1. Flow has valid SW metadata (PPE_DRV_V4_CONN_FLAG_FLOW_SW_MDATA_VALID)
	 *    This ensures the flow has proper metadata tracking for GRO processing
	 * 2. We haven't exceeded the maximum number of concurrent GRO flows
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_SW_MDATA_VALID)) {
		tx_port = ppe_drv_v4_conn_flow_tx_port_get(pcf);
		rx_port = ppe_drv_v4_conn_flow_rx_port_get(pcf);

		if (ppe_drv_hw_gro_is_enabled(p, tx_port, rx_port)) {
			ppe_drv_v4_conn_flow_flags_set(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_HW_GRO);
			atomic_inc(&p->gro_ctx.num_hw_gro_flows);
		}
	}
#endif

	if (!ppe_drv_flow_v4_pri_profile_get(pcf, pp, &flow_cfg.pri_profile)) {
		ppe_drv_warn("%p: failed to obtain a valid pri_profile value", pcf);
		return NULL;
	}

	ppe_drv_trace("%p: Pri profile: %u metadata: %u\n", pcf, flow_cfg.pri_profile, pcf->wifi_rule_ds_metadata);

	if (!ppe_drv_flow_v4_service_code_get(pcf, pp, &flow_cfg.sevice_code)) {
		ppe_drv_warn("%p: failed to obtain a valid service code", pcf);
		return NULL;
	}

	ppe_drv_trace("pcf %p: flow_tbl[host_idx]: %u sevice_code %d\n", pcf, host->index, flow_cfg.sevice_code);

	/*
	 * Get the flow cookie corresponding to flow.
	 */
#ifdef PPE_DRV_FLOW_COOKIE_48B_SUPPORT
	if (!ppe_drv_flow_v4_flow_cookie48b_get(pcf, &flow_cfg.flow_qos)) {
		ppe_drv_warn("%p: failed to obtain a valid flow cookie", pcf);
		return NULL;
	}

	flow_cfg.flow_qos.type = FAL_FLOW_QOS_TYPE_COOKIE_48B;
#elif defined(PPE_DRV_FLOW_COOKIE_40B_SUPPORT)
	if (!ppe_drv_flow_v4_flow_cookie40b_get(pcf, flow_cfg.flow_qos.cookie_40b)) {
		ppe_drv_warn("%p: failed to obtain a valid flow cookie", pcf);
		return NULL;
	}

	flow_cfg.flow_qos.type = FAL_FLOW_QOS_TYPE_COOKIE_40B;

	/*
	 * Get the WIFI QOS corresponding to flow.
	 */
	if (!ppe_drv_flow_v4_wifi_qos_get(pcf, &flow_cfg.flow_qos.qos, &wifi_qos_en)) {
		ppe_drv_warn("%p: failed to obtain wifi qos", pcf);
		return NULL;
	}

	flow_cfg.flow_qos.qos_valid = wifi_qos_en;

#else
	/*
	 * Get the tree ID corresponding to flow.
	 */
	if (!ppe_drv_flow_v4_tree_id_get(pcf, flow_cfg.flow_qos.tree_id)) {
		ppe_drv_warn("%p: failed to obtain a valid tree ID", pcf);
		return NULL;
	}

	/*
	 * Get the WIFI QOS corresponding to flow.
	 */
	if (!ppe_drv_flow_v4_wifi_qos_get(pcf, &flow_cfg.flow_qos.qos, &wifi_qos_en)) {
		ppe_drv_warn("%p: failed to obtain wifi qos", pcf);
		return NULL;
	}

	flow_cfg.flow_qos.qos_valid = wifi_qos_en;

#ifdef NSS_PPE_IPQ53XX
	flow_cfg.flow_qos.type = FAL_FLOW_QOS_TYPE_TREE_ID;
#endif
#endif

	/*
	 * Get the VPN ID corresponding to flow.
	 */
	if (!ppe_drv_flow_v4_vpn_id_get(pcf, &flow_cfg.vpn_id)) {
		ppe_drv_warn("%p: failed to obtain a valid vpn_id", pcf);
		return NULL;
	}

#ifdef PPE_DRV_FLOW_TBL_POLICER
	if (!ppe_drv_flow_v4_policer_get(pcf, &flow_cfg.policer_index, &flow_cfg.policer_valid)) {
		ppe_drv_warn("%p: failed to obtain policer_index", pcf);
		return NULL;
	}

	/*
	 * Get the Source interface index.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_SRC_INTERFACE_CHECK)) {
		/*
		 * Get the source interface index corresponding to flow.
		 * For bridge flow = Rx port no.
		 * For routed flow = ingress l3 if index.
		 */
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			struct ppe_drv_iface *in_port_if = ppe_drv_v4_conn_flow_in_port_if_get(pcf);
			struct ppe_drv_port *port = ppe_drv_iface_port_get(in_port_if);
			if (port) {
				flow_cfg.src_intf_index = port->port;
				flow_cfg.src_intf_valid = A_TRUE;
				ppe_drv_trace("%p: Bridged flow, src_intf_index: %u", pcf, flow_cfg.src_intf_index);
			}
		} else {
			struct ppe_drv_iface *in_l3_if = ppe_drv_v4_conn_flow_in_l3_if_get(pcf);
			struct ppe_drv_l3_if *l3_if = ppe_drv_iface_l3_if_get(in_l3_if);
			if (l3_if) {
				flow_cfg.src_intf_index = l3_if->l3_if_index;
				flow_cfg.src_intf_valid = A_TRUE;
				ppe_drv_trace("%p: Routed flow, src_intf_index: %u", pcf, flow_cfg.src_intf_index);
			}
		}
	}
#endif

	/*
	 * Set forwarding type
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_PPE_POLICER_ASSIST)) {
		flow_cfg.fwd_type = ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW) ?
				    FAL_FLOW_BRIDGE: FAL_FLOW_ROUTE;
		ppe_drv_trace("%p: Policer enabled flow\n", pcf);
	} else if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		flow_cfg.port_valid = A_TRUE;
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
			flow_cfg.fwd_type = FAL_FLOW_BRIDGE;
			flow_cfg.bridge_port = PPE_DRV_PORT_CPU;
		} else {
			flow_cfg.fwd_type = FAL_FLOW_ROUTE;
			flow_cfg.route_port = PPE_DRV_PORT_CPU;
			flow_cfg.route_nexthop = nh->index;
		}
		ppe_drv_trace("%p: RFS enabled flow\n", pcf);
	} else if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_PRIORITY_PPE_ASSIST)) {
		/*
		 * Case PPE is used only to Assist in priority marking of packets
		 */
		flow_cfg.fwd_type = FAL_FLOW_RDT_TO_CPU;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: Priority Assist: %u", pcf, FAL_FLOW_FORWARD);
	} else if (ipv4_is_multicast(htonl(match_dest_ip))) {
		/*
		 * Multicast flow
		 */
		flow_cfg.fwd_type = FAL_FLOW_FORWARD;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: L2-Multicast: %u", pcf, FAL_FLOW_FORWARD);
	} else if ((match_src_ip != xlate_src_ip) || (dev_eg && (dev_eg->priv_flags_ext & IFF_EXT_MAPT))) {
		/*
		 * Case SNAT
		 */
		flow_cfg.fwd_type = FAL_FLOW_SNAT;
		flow_cfg.snat_nexthop = nh->index;
		flow_cfg.snat_srcport = ppe_drv_v4_conn_flow_xlate_src_ident_get(pcf);
		ppe_drv_trace("%p: flow_tbl[nat_type]: SNAT: %u snat_port: %u", pcf, FAL_FLOW_SNAT, flow_cfg.snat_srcport);
	} else if ((match_dest_ip != xlate_dest_ip) || (dev_in && (dev_in->priv_flags_ext & IFF_EXT_MAPT))) {
		/*
		 * Case DNAT
		 */
		flow_cfg.fwd_type = FAL_FLOW_DNAT;
		flow_cfg.dnat_nexthop = nh->index;
		flow_cfg.dnat_dstport = ppe_drv_v4_conn_flow_xlate_dest_ident_get(pcf);
		ppe_drv_trace("%p: flow_tbl[nat_type]: DNAT: %u dnat_port: %u", pcf, FAL_FLOW_DNAT, flow_cfg.dnat_dstport);
	} else if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLOW_FLAG_BRIDGE_FLOW)) {
		/*
		 * Case bridging
		 */
		flow_cfg.fwd_type = FAL_FLOW_BRIDGE;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: L2: %u", pcf, FAL_FLOW_BRIDGE);

#ifdef PPE_DRV_BRIDGE_NEXT_HOP
		/*
		 * VLAN tag addition from NEXTHOP_TBL for flow based bridging.
		 * With VLAN tagging capability from flow table for bridge flows,
		 * we can avoid EG_VLAN_XLT_RULE based VLAN tagging. This would help
		 * doing VLANs between different ingress and egress VSI.
		 */
		if (nh) {
			flow_cfg.bridge_nexthop_valid = A_TRUE;
			flow_cfg.bridge_nexthop = nh->index;
			ppe_drv_trace("%p:nexthop index: %u", pcf, nh->index);
		}
#endif
		flow_cfg.port_valid = A_TRUE;
		flow_cfg.bridge_port = pp->port;
		ppe_drv_trace("%p: xmit interface port: %d", pcf, pp->port);

		if (ppe_drv_port_is_tunnel_vp(pp)) {
			switch (vlan_hdr_cnt) {
			case 2:
				flow_cfg.svlan_fmt = A_TRUE;
				flow_cfg.cvlan_fmt = A_TRUE;
				flow_cfg.vlan_fmt_valid = 1;
				break;
			case 1:
				flow_cfg.cvlan_fmt = A_TRUE;
				flow_cfg.vlan_fmt_valid = 1;
				break;
			case 0:
				break;
			default:
				ppe_drv_warn("%p: cannot accelerate bridge VLAN flows for more than 2 vlans: %u",
						pcf, vlan_hdr_cnt);
				return NULL;
			}
		}
	} else {
		/*
		 * Case simple routing (Non-NAT)
		 */
		flow_cfg.route_nexthop = nh->index;
		flow_cfg.fwd_type = FAL_FLOW_ROUTE;
		ppe_drv_trace("%p: flow_tbl[fwd_type]: L3: %u", pcf, FAL_FLOW_ROUTE);
	}

	/*
	 * Fill relevant details for 3-tuple and 5-tuple flows.
	 */
	switch (match_protocol) {
	case IPPROTO_TCP:
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_TCP;
		ppe_drv_trace("%p: flow_tbl[protocol]: TCP-%u", pcf, FAL_FLOW_PROTOCOL_TCP);
		break;

	case IPPROTO_UDP:
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_UDP;
		ppe_drv_trace("%p: flow_tbl[protocol]: UDP-%u", pcf, FAL_FLOW_PROTOCOL_UDP);
		break;

	case IPPROTO_UDPLITE:
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_UDPLITE;
		ppe_drv_trace("%p: flow_tbl[protocol]: UDP-Lite-%u", pcf, FAL_FLOW_PROTOCOL_UDPLITE);
		break;

	case IPPROTO_ESP:
#ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
		/*
		 * In case if it is an IPSEC pass-through flow with SPI match,
		 * consider this as a 5 tuple flow with custom ESP ID in the port
		 * numbers.
		 */
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_ESP_SPI)) {
			flow_cfg.protocol = FAL_FLOW_IPSEC;
			ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_IPSEC);
			break;
		}
#endif
		tuple_3 = true;
		ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_PROTOCOL_OTHER);
		break;

	case IPPROTO_IPIP:
		tuple_3 = true;
		ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_PROTOCOL_OTHER);
		break;

	case IPPROTO_GRE:
		tuple_3 = true;
		ppe_drv_trace("%p: flow_tbl[protocol]: Other-%u", pcf, FAL_FLOW_PROTOCOL_OTHER);
		break;

	default:
		ppe_drv_stats_inc(&p->stats.gen_stats.fail_unknown_proto);
		ppe_drv_warn("%p: protocol: %u cannot be offloaded to PPE", pcf, match_protocol);
		return NULL;
	}

	flow_cfg.flow_ip.ipv4 = match_dest_ip;
	ppe_drv_trace("%p: flow_tbl[dest_ip]: %pI4", pcf, &match_dest_ip);
	if (tuple_3) {
		flow_cfg.entry_type = FAL_FLOW_IP4_3TUPLE_ADDR;
		flow_cfg.protocol = FAL_FLOW_PROTOCOL_OTHER;
		flow_cfg.ip_type = match_protocol;
		ppe_drv_trace("%p: flow_tbl[ip_proto]: 0x%x", pcf, match_protocol);
	} else {
		flow_cfg.entry_type = FAL_FLOW_IP4_5TUPLE_ADDR;

		flow_cfg.src_port = ppe_drv_v4_conn_flow_match_src_ident_get(pcf);
		flow_cfg.dst_port = ppe_drv_v4_conn_flow_match_dest_ident_get(pcf);

		/*
		 * Fill the SPI value in port numbers in case if this is IPSEC SPI passthrough case.
		 */
#ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
		if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_ESP_SPI)) {
			uint32_t spi = (uint32_t)ppe_drv_v4_conn_flow_esp_spi_get(pcf);
			flow_cfg.src_port = (uint16_t)((spi >> PPE_DRV_ESP_SPI_SHIFT) & PPE_DRV_ESP_SPI_MASK);
			flow_cfg.dst_port = (uint16_t)(spi & PPE_DRV_ESP_SPI_MASK);
		}
#endif
		ppe_drv_trace("%p: flow_tbl[sport]: %u", pcf, flow_cfg.src_port);
		ppe_drv_trace("%p: flow_tbl[dport]: %u", pcf, flow_cfg.dst_port);

	}

	/*
	 * Fill Path-MTU.
	 *
	 * Note: For multicast flows, it should be ok to program the minimum MTU value
	 * of all the interfaces, since PPE also check MTU for each destination interface
	 * and exception the packet (without cloning) if MTU check fail for any interface.
	 */
	xmit_mtu = ipv4_is_multicast(htonl(match_dest_ip)) ? ppe_drv_v4_conn_flow_mc_min_mtu_get(pcf)
		: ppe_drv_v4_conn_flow_xmit_interface_mtu_get(pcf);
	if (xmit_mtu > PPE_DRV_PORT_JUMBO_MAX) {
		ppe_drv_trace("%p: xmit_mtu: %d is larger, restricting to max: %d", pcf, xmit_mtu, PPE_DRV_PORT_JUMBO_MAX);
		xmit_mtu = PPE_DRV_PORT_JUMBO_MAX;
	}

	flow_cfg.pmtu_check_l3 = (a_bool_t)PPE_DRV_FLOW_PMTU_TYPE_L3;

	/*
	 * For RFS assist flows, force PMTU to jumbo to avoid L3 PMTU check as SFE offload can fragment
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		xmit_mtu = PPE_DRV_PORT_JUMBO_MAX;
	}

	flow_cfg.pmtu = xmit_mtu;

	ppe_drv_trace("%p: flow_tbl[PMTU]: %u", pcf, xmit_mtu);

	/*
	 * Changing the fwd_type to RDT_TO_CPU in case of unidirectional acceleration,
	 * when one of the flow is not accelerated (flow or return)
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_ACCEL_DISABLE)) {
		unidir_flow_cfg = kzalloc(sizeof(fal_flow_entry_t), GFP_ATOMIC);
		if (unidir_flow_cfg) {
			memcpy(unidir_flow_cfg, &flow_cfg, sizeof(fal_flow_entry_t));
		} else {
			ppe_drv_warn("%p: failed to allocate flow_cfg, unidirectional updates will be disabled", pcf);
		}
		flow_cfg.fwd_type = FAL_FLOW_RDT_TO_CPU;
	}

	/*
	 * For UDP-ST v4 flows, set the flow fwd_type to DROP. This is needed for locally
	 * terminated UDP-ST traffic where we only want to accelerate the flow
	 * direction (VP -> WAN) and drop the return direction for tx and drop both
	 * for rx.
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_UDP_ST_DROP)) {
		flow_cfg.fwd_type = FAL_FLOW_DROP;
	}

	/*
	 * Add the flow
	 */
	err = fal_flow_entry_add(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_KEY, &flow_cfg);
	if (err != SW_OK) {
		ppe_drv_trace("%p: flow entry add failed", pcf);
		if (unidir_flow_cfg) {
			kfree(unidir_flow_cfg);
		}
		return NULL;
	}

#ifdef NSS_PPE_DRV_HW_GRO
	/*
	 * Enable GRO
	 */
	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_HW_GRO)) {
		err = fal_flow_gro_en_set(PPE_DRV_SWITCH_ID, flow_cfg.entry_id, A_TRUE);
		if (err != SW_OK) {
			/*
			 * If HW GRO failed, then expectation is to fall back to SW GRO
			 */
			ppe_drv_warn("%p: Setting of GRO failed, err=%d. Rolling back GRO state", pcf, err);
			ppe_drv_v4_conn_flow_flags_clear(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_HW_GRO);
			atomic_dec(&p->gro_ctx.num_hw_gro_flows);
		}
	}
#endif

	/*
	 * Get the sw instance of flow entry.
	 */
	flow = &p->flow[flow_cfg.entry_id];
	ppe_drv_assert(!(flow->flags & PPE_DRV_FLOW_VALID), "%p: flow entry is already accelerated to PPE at index: %d",
			pcf, flow_cfg.entry_id);


	/*
	 * Set the host Qdisc information for the flow at the particular flow index
	 */
	ppe_drv_flow_v4_host_qdisc_info_set(pcf, flow_cfg.entry_id);

	/*
	 * Increment flow count if SAWF service is configured in tree_id.
	 */
	if (ppe_drv_tree_id_type_get(&pcf->fl_mdata) == PPE_DRV_TREE_ID_TYPE_SAWF) {
		service_class = tree_id_data->info.sawf_metadata.service_class;
		if (PPE_DRV_SERVICE_CLASS_IS_VALID(service_class)) {
			sawf_sc_stats = &p->stats.sawf_sc_stats[service_class];
			ppe_drv_stats_inc(&sawf_sc_stats->flow_count);
			ppe_drv_trace("Stats Updated: service class %u : flow  %llu\n", service_class, atomic64_read(&sawf_sc_stats->flow_count));
		}
	}

	/*
	 * Update the shadow copy
	 */
	flow->host = host;
	flow->nh = nh;
	flow->flags |= PPE_DRV_FLOW_V4;
	flow->type = PPE_DRV_IP_TYPE_V4;
	flow->entry_type = flow_cfg.entry_type;
	flow->pri_profile = flow_cfg.pri_profile;
	flow->pcf.v4 = pcf;

	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_ACCEL_DISABLE)) {
		flow->unidir_info.flow_cfg = unidir_flow_cfg;
	}

	ppe_drv_trace("%p: flow_tbl entry added at index: %u", pcf, flow_cfg.entry_id);
	return flow;
}

/*
 * ppe_drv_flow_v4_attach_mapt_v6_conn
 *	Attach v6 flow entry to the corresponding v4 flow entry for mapt.
 *	Also caches the v4 pcf in the v6 flow's pf->mapt_info so the detach
 *	path can find the v4 flow without re-running xlate_6_to_4().
 */
bool ppe_drv_flow_v4_attach_mapt_v6_conn(struct ppe_drv_v4_conn_flow *pcf_v4, struct ppe_drv_v6_conn_flow *pcf_v6, uint8_t length_adjust)
{
	struct ppe_drv_flow *pf = pcf_v4->pf;
	struct ppe_drv_flow_mapt_info *mapt_info = &pf->mapt_info;
	struct ppe_drv_v4_conn_flow *old_pcf_v4;

	if (!(pf->flags & PPE_DRV_FLOW_V4)) {
		ppe_drv_trace("%p: flow is invalid", pf);
		return false;
	}

	mapt_info->mapt_v6 = pcf_v6;
	mapt_info->len_adjust = length_adjust;
	pf->flags |= PPE_DRV_FLOW_MAPT;

	/*
	 * pf might not be valid as we didn't push the inner flow rule.
	 */
	if (!pcf_v6->pf) {
		pcf_v6->pf = kzalloc(sizeof(struct ppe_drv_flow), GFP_ATOMIC);
		if (!pcf_v6->pf) {
			ppe_drv_warn("%p: failed to allocate standalone pf for MAP-T outer v6 flow\n", pcf_v6);
			return false;
		}
	}

	/*
	 * Ideally there is a 1:1 mapping between a v4 and v6 connection in MAP-T.
	 * However, in a corner case the same v6 outer flow can get attached to one
	 * v4 inner flow and later re-attached to a different v4 inner flow, possibly
	 * due to an incorrect NAT46 translation result at the first attach.
	 * Clear the stale mapt_v6 back-reference from the old v4 flow so that when
	 * it is eventually destroyed it does not attempt to detach from a v6 flow
	 * that now belongs to a different v4 connection.
	 */
	old_pcf_v4 = pcf_v6->pf->mapt_info.mapt_v4;
	if (old_pcf_v4 && old_pcf_v4 != pcf_v4 && old_pcf_v4->pf) {
		old_pcf_v4->pf->mapt_info.mapt_v6 = NULL;
	}

	/*
	 * Cache the v4 pcf in the v6 flow's pf->mapt_info so the detach path
	 * can find the v4 flow without re-running xlate_6_to_4().
	 */
	pcf_v6->pf->mapt_info.mapt_v4 = pcf_v4;

	return true;
}

/*
 * ppe_drv_flow_v4_detach_mapt_v6_conn
 *	Detach v6 flow entry from the corresponding v4 flow entry for mapt.
 *	Also clears the cached v4 pcf from the v6 flow's pf->mapt_info and
 *	frees the standalone pf if it was allocated for a MAP-T outer v6 flow.
 */
bool ppe_drv_flow_v4_detach_mapt_v6_conn(struct ppe_drv_v4_conn_flow *pcf_v4)
{
	struct ppe_drv_flow *pf = pcf_v4->pf;
	struct ppe_drv_flow_mapt_info *mapt_info;
	struct ppe_drv_v6_conn_flow *pcf_v6;

	if (!pf || !(pf->flags & PPE_DRV_FLOW_MAPT)) {
		ppe_drv_warn("%p: flow is invalid", pcf_v4);
		return false;
	}

	mapt_info = &pf->mapt_info;
	pcf_v6 = mapt_info->mapt_v6;

	/*
	 * Clear the cached v4 pcf from the v6 flow's pf.
	 */
	if (pcf_v6 && pcf_v6->pf && pcf_v6->pf->mapt_info.mapt_v4 == pcf_v4) {
		pcf_v6->pf->mapt_info.mapt_v4 = NULL;
		kfree(pcf_v6->pf);
		pcf_v6->pf = NULL;
	}

	pf->flags &= ~PPE_DRV_FLOW_MAPT;
	mapt_info->mapt_v6 = NULL;
	mapt_info->len_adjust = 0;

	return true;
}

/*
 * ppe_drv_flow_table_free()
 *	API to free up the flow table space if it was allocated
 */
static void ppe_drv_flow_table_free(void)
{
	if (flow_table_info) {
		vfree(flow_table_info->qdisc_info);
		vfree(flow_table_info);
	}
}

/*
 * ppe_drv_flow_accel_enable()
 *	Accelerate the de-accelerated flow by updating the fwd_type.
 * This will be called under the ppe_drv lock, taken at the
 * time of update.
 */
bool ppe_drv_flow_accel_enable(struct ppe_drv_flow *flow)
{
	sw_error_t err;

	if (!flow || !flow->unidir_info.flow_cfg) {
		ppe_drv_warn("%p: invalid flow for acceleration", flow);
		return false;
	}

	/*
	 * Update the flow in FAL to change the forward type.
	 */
	err = fal_flow_entry_add(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_KEY, flow->unidir_info.flow_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: flow entry %u resume failed", flow, flow->unidir_info.flow_cfg->entry_id);
		return false;
	}

	/*
	 * Delete the flow_cfg reference
	 */
	if (flow->unidir_info.flow_cfg) {
		kfree(flow->unidir_info.flow_cfg);
		flow->unidir_info.flow_cfg = NULL;
	}

	return true;
}

/*
 * ppe_drv_flow_entries_free()
 *	Free flow table entries if it was allocated.
 */
void ppe_drv_flow_entries_free(struct ppe_drv_flow *flow)
{
	ppe_drv_flow_table_free();
	nss_ppe_drv_minidump_free(flow, "ppe_drv_flow");
	kfree(flow);
}

/*
 * ppe_drv_flow_table_alloc()
 *	API to allocate flow table space to store the per flow metadata
 */
static bool ppe_drv_flow_table_alloc(struct ppe_drv *p)
{
	flow_table_info = vzalloc(sizeof(struct ppe_drv_flow_table_info));
	if (!flow_table_info) {
		ppe_drv_warn("%px: Failed to allocate flow table space\n", p);
		return false;
	}

	flow_table_info->qdisc_info = (struct ppe_drv_flow_host_qdisc_info **)vzalloc(sizeof(struct ppe_drv_flow_host_qdisc_info *) * p->flow_num);
	if (!flow_table_info->qdisc_info) {
		vfree(flow_table_info);
		return false;
	}

	return true;
}

/*
 * ppe_drv_flow_entries_alloc()
 *	Allocated and initialize requested number of flow table entries.
 */
struct ppe_drv_flow *ppe_drv_flow_entries_alloc()
{
	uint16_t i;
	struct ppe_drv_flow *flow;
	struct ppe_drv *p = ppe_drv_gbl;

	flow = kzalloc(sizeof(struct ppe_drv_flow) * p->flow_num, GFP_KERNEL);
	if (!flow) {
		ppe_drv_warn("%p: failed to allocate flow entries", p);
		return NULL;
	}

	/*
	 * Allocate flow table area to storing the flow based metadata
	 */
	if (!ppe_drv_flow_table_alloc(p)) {
		ppe_drv_warn("%p: failed to allocate flow table space\n", p);
		kfree(flow);
		return NULL;
	}

	nss_ppe_drv_minidump_log(flow, sizeof(struct ppe_drv_flow) * p->flow_num, "ppe_drv_flow");

	/*
	 * Assign flow index values to the flow entries
	 */
	for (i = 0; i < p->flow_num; i++) {
		flow[i].index = i;
	}

	return flow;
}

/*
 * ppe_drv_flow_get_and_hold_qdisc_netdev()
 * 	Returns qdisc netdev for a qdisc interface
 */
struct net_device *ppe_drv_flow_get_and_hold_qdisc_netdev(int32_t flow_index)
{
	struct ppe_drv_flow_host_qdisc_info *qdisc_info = NULL;
	struct ppe_drv_flow_table_info *flow_info = flow_table_info;

	qdisc_info = rcu_dereference(flow_info->qdisc_info[flow_index]);
	if (!qdisc_info) {
		return NULL;
	}

	if (qdisc_info->flags == PPE_DRV_HOST_QDISC_INVALID) {
		return NULL;
	}

	/*
	 * This will be released after packet is xmitted,
	 * in ppe_vp_rx_dp_cb()
	 */
	if (qdisc_info->qdisc_xmit_dev) {
		dev_hold(qdisc_info->qdisc_xmit_dev);
	}

	return qdisc_info->qdisc_xmit_dev;
}

/*
 * ppe_drv_get_qdisc_rule_flag()
 * 	Return flags for Qdisc rule
 */
int8_t ppe_drv_flow_get_qdisc_rule_flag(int32_t flow_index)
{
	struct ppe_drv_flow_host_qdisc_info *qdisc_info = NULL;
	struct ppe_drv_flow_table_info *flow_info = flow_table_info;

	qdisc_info = rcu_dereference(flow_info->qdisc_info[flow_index]);
	if (!qdisc_info) {
		return PPE_DRV_HOST_QDISC_INVALID;
	}

        if (qdisc_info->flags != PPE_DRV_HOST_QDISC_INVALID) {
		return qdisc_info->flags;
	}

	return PPE_DRV_HOST_QDISC_INVALID;
}

/*
 * ppe_drv_flow_get_qos_tag()
 * 	Returns QoS tag.
 */
uint32_t ppe_drv_flow_get_qos_tag(int32_t flow_index)
{
	struct ppe_drv_flow_host_qdisc_info *qdisc_info = NULL;
	struct ppe_drv_flow_table_info *flow_info = flow_table_info;

	qdisc_info = rcu_dereference(flow_info->qdisc_info[flow_index]);
	if (!qdisc_info) {
		return PPE_DRV_HOST_QDISC_INVALID;
	}

        if (qdisc_info->flags != PPE_DRV_HOST_QDISC_INVALID) {
		return qdisc_info->class_id;
	}

	return PPE_DRV_HOST_QDISC_INVALID;
}

/*
 * ppe_drv_flow_accel_suspend()
 * 	flow is not offloaded to PPE.
 */
bool ppe_drv_flow_accel_suspend(struct ppe_drv_flow *pf)
{
	fal_flow_entry_t flow_cfg = {0};
	void *flow_cfg_ctx;
	sw_error_t err;

	if (!pf) {
		ppe_drv_warn("pf for flow suspend is NULL\n");
		return false;
	}

	/*
	 * Do not proceed if the accel is already suspended
	 * or software context for flow config is allocated.
	 */
	if (pf->flow_cfg) {
		ppe_drv_warn("%p: flow entry %u is already suspended\n", pf, flow_cfg.entry_id);
		return true;
	}

	/*
	 * fetch the flow_cfg from FAL.
	 */
	flow_cfg.entry_id = pf->index;
	flow_cfg.entry_type = pf->entry_type;
	err = fal_flow_entry_get(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_INDEX, &flow_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get flow entry %u\n", pf, flow_cfg.entry_id);
		return false;
	}

	/*
	 * If fwd_type is already RDT_TO_CPU, skip the accel suspension.
	 */
	if (flow_cfg.fwd_type == FAL_FLOW_RDT_TO_CPU) {
		ppe_drv_warn("%p: flow entry %u is already RDT_TO_CPU\n", pf, flow_cfg.entry_id);
		return true;
	}

	/*
	 * Maintain a s/w context for flow_cfg
	 */
	flow_cfg_ctx = kmalloc(sizeof(fal_flow_entry_t), GFP_ATOMIC);
	if (!flow_cfg_ctx) {
		ppe_drv_warn("%p: Failed to allocate memory of flow_cfg for flow entry %u\n", pf, flow_cfg.entry_id);
		return false;
	}

	memcpy((fal_flow_entry_t *)flow_cfg_ctx, &flow_cfg, sizeof(fal_flow_entry_t));

	/*
	 * Update the flow forward type.
	 */
	flow_cfg.fwd_type = FAL_FLOW_RDT_TO_CPU;
	err = fal_flow_entry_add(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_KEY, &flow_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: flow entry %u suspend failed\n", pf, flow_cfg.entry_id);
		kfree(flow_cfg_ctx);
		return false;
	}

	ppe_drv_trace("%p: flow entry %u is suspended\n", pf, flow_cfg.entry_id);
	pf->flow_cfg = flow_cfg_ctx;
	return true;
}

/*
 * ppe_drv_flow_accel_resume()
 *      flow is offloaded to PPE.
 */
ppe_drv_ret_t ppe_drv_flow_accel_resume(struct ppe_drv_flow *pf)
{
	sw_error_t err;
	fal_flow_entry_t *flow_cfg;

	if (!pf) {
		ppe_drv_warn("pf for flow resume is NULL\n");
		return PPE_DRV_RET_ACCEL_RESUME_FAIL;
	}

	/*
	 * Do not proceed if the flow is accelerated
	 * or there is no sw context for flow config.
	 */
	flow_cfg = (fal_flow_entry_t *)pf->flow_cfg;
	if (!flow_cfg) {
		ppe_drv_warn("%p: flow not suspended / already resumed / already RDT_TO_CPU\n", pf);
		return PPE_DRV_RET_ACCEL_RESUME_FAIL;
	}

	/*
	 * Update the flow in FAL based on the ppe flow's local context.
	 * This restores the original forwarding type that was captured and saved
	 * in pf->flow_cfg before suspension, thereby resuming acceleration.
	 */
	err = fal_flow_entry_add(PPE_DRV_SWITCH_ID, FAL_FLOW_OP_MODE_KEY, flow_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: flow entry %u resume failed", pf, flow_cfg->entry_id);
		pf->flow_cfg = NULL;
		kfree(flow_cfg);
		return PPE_DRV_RET_ACCEL_RESUME_FAIL;
	}

	ppe_drv_trace("%p: flow entry %u is resumed with fwd_type %u\n", pf, flow_cfg->entry_id, flow_cfg->fwd_type);

	/*
	 * Free the s/w context memory for flow_cfg.
	 */
	pf->flow_cfg = NULL;
	kfree(flow_cfg);

	return PPE_DRV_RET_SUCCESS;
}
