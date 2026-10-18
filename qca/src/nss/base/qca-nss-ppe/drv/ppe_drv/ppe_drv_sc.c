/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <fal/fal_servcode.h>
#include <fal/fal_qm.h>
#include "ppe_drv.h"

#if (PPE_DRV_DEBUG_LEVEL == 3)
/*
 * ppe_drv_sc_dump()
 *	Dumps service code related tables.
 */
static void ppe_drv_sc_dump(ppe_drv_sc_t sc)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_servcode_config_t sc_cfg = {0};
	sw_error_t err;

	/*
	 * Get the service code table details through SSDK.
	 */
	err = fal_servcode_config_get(PPE_DRV_SWITCH_ID, sc, &sc_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: service code query failed for sc: %u", p, sc);
		return;
	}

	ppe_drv_trace("%p: dest_port_valid: %u", p, sc_cfg.dest_port_valid);
	ppe_drv_trace("%p: dest_port_id: %u", p, sc_cfg.dest_port_id);
	ppe_drv_trace("%p: bypass_bitmap[IN_SERVICE_TBL]: 0x%llx", p, sc_cfg.bypass_bitmap[0]);
	ppe_drv_trace("%p: bypass_bitmap[L2_SERVICE_TBL]: 0x%llx", p, sc_cfg.bypass_bitmap[1]);
	ppe_drv_trace("%p: bypass_bitmap[CNTRS]: 0x%llx", p, sc_cfg.bypass_bitmap[2]);
	ppe_drv_trace("%p: bypass_bitmap[TL_SERVICE_TBL]: 0x%llx", p, sc_cfg.bypass_bitmap[3]);
	ppe_drv_trace("%p: direction: %u", p, sc_cfg.direction);
	ppe_drv_trace("%p: field_update_bitmap: 0x%llx-%llx",
			p, sc_cfg.field_update_bitmap[1], sc_cfg.field_update_bitmap[0]);
	ppe_drv_trace("%p: next_service_code: %u", p, sc_cfg.next_service_code);
	ppe_drv_trace("%p: hw_services: %u", p, sc_cfg.hw_services);
	ppe_drv_trace("%p: offset_sel: %u", p, sc_cfg.offset_sel);
}
#else
static void ppe_drv_sc_dump(ppe_drv_sc_t sc)
{
}
#endif

/*
 * ppe_drv_sc_in_service_tbl_dest_port()
 *	Configure service table for upstream port
 */
sw_error_t ppe_drv_sc_in_service_tbl_dest_port(ppe_drv_sc_t sc, uint8_t redir_port)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_servcode_config_t sc_cfg = {0};
	sw_error_t err;

	err = fal_servcode_config_get(PPE_DRV_SWITCH_ID, sc, &sc_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: service code configuration get failed for sc: %u", p, sc);
		return err;
	}

	sc_cfg.dest_port_id = redir_port;
	sc_cfg.dest_port_valid = A_TRUE;

	/*
	 * Program the service code tables through SSDK.
	 */
	err = fal_servcode_config_set(PPE_DRV_SWITCH_ID, sc, &sc_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: service code configuration failed for sc: %u", p, sc);
		return err;
	}

	return err;
}

/*
 * ppe_drv_sc_ucast_queue_set()
 *	Set queue ID of a given port in PPE for RFS.
 */
void ppe_drv_sc_ucast_queue_set(ppe_drv_sc_t sc, uint8_t queue_id, uint8_t src_profile, uint8_t profile_id)
{
	sw_error_t err;
	fal_ucast_queue_dest_t q_dst = {0};

	q_dst.src_profile = src_profile;
	q_dst.service_code_en = A_TRUE;
	q_dst.service_code = sc;

	err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &q_dst, queue_id, profile_id);
	if (err != SW_OK) {
		ppe_drv_warn("unable to change port queue base ID: %d", queue_id);
		return;
	}

	ppe_drv_trace("set port ucast queue base id: %d for sc: %d", queue_id, sc);
}

#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
/*
 * ppe_drv_sc_noedit_tun_rps_queue_init()
 *	Map PPE_DRV_SC_NOEDIT_TUN_RPS service code to the CPU port's base ucast queue.
 */
bool ppe_drv_sc_noedit_tun_rps_queue_init(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	int cpu_ucast_queue;

	cpu_ucast_queue = ppe_drv_port_ucast_queue_get_by_port(PPE_DRV_PORT_CPU);
	if (cpu_ucast_queue < 0) {
		ppe_drv_warn("%p: Failed to get CPU port ucast queue for TUN RPS SC mapping", p);
		return false;
	}

	ppe_drv_sc_ucast_queue_set(PPE_DRV_SC_NOEDIT_TUN_RPS, (uint8_t)cpu_ucast_queue,
				   PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_PORT_SRC_PROFILE);
	ppe_drv_trace("%p: Mapped PPE_DRV_SC_NOEDIT_TUN_RPS to CPU ucast queue base: %d",
		      p, cpu_ucast_queue);

	return true;
}
#endif

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
static void ppe_drv_sc_config_lpbk_port_pon(ppe_drv_sc_t sc, fal_servcode_config_t *sc_cfg)
{
	switch (sc) {
		case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC:
			sc_cfg->bypass_bitmap[1] = (1 << EG_VLAN_MEMBER_CHECK_BYP);
			sc_cfg->bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
			sc_cfg->direction = PPE_DRV_SC_IN_L2_DIR_SRC;
			sc_cfg->field_update_bitmap[0] = (1 << FLD_UPDATE_SRC_INFO_BYPASS);
			sc_cfg->dest_port_valid = A_TRUE;
			break;

		case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC_NEXT:
			sc_cfg->bypass_bitmap[1] = (1 << L2_VP_SERVICE_CODE_ENQ_BYP);
			sc_cfg->dest_port_valid = A_FALSE;
			break;

		default:
			ppe_drv_warn("Invalid service code for pon loopback port\n");
			break;
	}
}
#endif

/*
 * ppe_drv_sc_ucast_qbase_profile_set()
 *	API to configure qbase profile for input service code
 */
bool ppe_drv_sc_ucast_qbase_profile_set(uint8_t sc, uint16_t qbase)
{
	struct ppe_drv *p = ppe_drv_gbl;

	if (sc >= PPE_DRV_SC_MAX) {
		ppe_drv_warn("Incorrect SC:%d passed for SC qbase profile configuration\n", sc);
		return false;
	}

	spin_lock_bh(&p->lock);
	ppe_drv_sc_ucast_queue_set(sc, qbase, PPE_DRV_PORT_SRC_PROFILE, PPE_DRV_REDIR_PROFILE_ID);
	spin_unlock_bh(&p->lock);
	ppe_drv_info("port ucast queue base id: %d for sc: %d\n", qbase, sc);
	return true;
}
EXPORT_SYMBOL(ppe_drv_sc_ucast_qbase_profile_set);

#ifdef PPE_DRV_DDRQ_ENABLE
/*
 * ppe_drv_sc_spl_ddrq_config()
 *	API to configure DDRQ special service codes
 */
void ppe_drv_sc_spl_ddrq_config(void)
{
	sw_error_t err;
	fal_servcode_config_t sc_cfg = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	uint32_t servcode;

	/*
	 * Set all the first 32-bits in the bypass bitmap 0/1/3
	 */

	sc_cfg.bypass_bitmap[0] = ~((1 << FAKE_MAC_HEADER_BYP)
			| (1 << SERVICE_CODE_BYP)
			| (1 << FAKE_L2_PROTO_BYP)
			| (1 << MY_MAC_CHECK_BYP));
	sc_cfg.bypass_bitmap[1] = PPE_DRV_SC_BYPASS_BITMAP_32BITS_VAL;
	sc_cfg.bypass_bitmap[3] = PPE_DRV_SC_BYPASS_BITMAP_32BITS_VAL;
	sc_cfg.bypass_bitmap[2] = ~(1 << TX_VLAN_COUNTER_BYP);
	servcode = FAL_SERVCODE(FAL_SERVCODE_TYPE_PASSTHROUGH, PPE_DRV_SC_DDRQ_FULL_PKT_SPL_BYPASS_SC);

	err = fal_servcode_config_set(PPE_DRV_SWITCH_ID, servcode, &sc_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: service code configuration failed for sc: %u", p, servcode);
		return;
	}

	/*
	 * Reset all the bits in the bypass bitmap 0/1/3
	 */
	sc_cfg.bypass_bitmap[0] = 0;
	sc_cfg.bypass_bitmap[1] = 0;
	sc_cfg.bypass_bitmap[3] = 0;
	sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
	servcode = FAL_SERVCODE(FAL_SERVCODE_TYPE_PASSTHROUGH, PPE_DRV_SC_DDRQ_PT_SPL_BYPASS_SC);

	err = fal_servcode_config_set(PPE_DRV_SWITCH_ID, servcode, &sc_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: service code configuration failed for sc: %u", p, servcode);
		return;
	}

	ppe_drv_warn("%p: DDRQ SPL SC configurations done\n", p);
	return;
}
#endif

/*
 * ppe_drv_sc_config()
 *	Configured service code related tables based on input information.
 */
void ppe_drv_sc_config(ppe_drv_sc_t sc, ppe_drv_sc_t next_sc, uint8_t redir_port)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_servcode_config_t sc_cfg = {0};
	sw_error_t err;

	sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_DST;
	sc_cfg.dest_port_id = redir_port;
	sc_cfg.next_service_code = next_sc;
	sc_cfg.dest_port_valid = A_TRUE;

	switch (sc) {
	case PPE_DRV_SC_BYPASS_ALL:
	case PPE_DRV_SC_SPF_BYPASS_NEXT:
		sc_cfg.bypass_bitmap[0] = ~((1 << FAKE_MAC_HEADER_BYP)
					| (1 << SERVICE_CODE_BYP)
					| (1 << FAKE_L2_PROTO_BYP)
					| (1 << MY_MAC_CHECK_BYP));

		sc_cfg.bypass_bitmap[1] = ~((1ULL << ACL_POST_ROUTING_CHECK_BYP)
#ifdef NSS_PPE_IPQ52XX
				| (1ULL << DOT1P_MAPPER_BYP)
				| (1ULL << DOT1P_DST_LOOKUP_BYPASS)
#endif
#if defined(NSS_PPE_IPQ96XX) || defined(NSS_PPE_IPQ52XX)
				| (1ULL << IN_MIRROR_BYP)
				| (1ULL << EG_MIRROR_BYP)
#endif
				);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_ADV_QOS_BRIDGED:
	case PPE_DRV_SC_ADV_QOS_ROUTED:
	case PPE_DRV_SC_PTP:
		break;

	case PPE_DRV_SC_LOOPBACK_QOS:
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SRC_INFO)
					| (1 << FLD_UPDATE_HASH_FLOW_INDEX);
		break;

	case PPE_DRV_SC_IPSEC_PPE2EIP_DECAP:
		sc_cfg.offset_sel = PPE_DRV_SC_IN_L2_OFF_L3;
		sc_cfg.bypass_bitmap[1] = ((1 << L3_PKT_EDIT_BYP) | (1 << L2_SOURCE_SEC_BYP));
		sc_cfg.hw_services = PPE_DRV_EIP_HWSERVICE_IPSEC_AIIP;
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO)
					| (1 << FLD_UPDATE_HASH_FLOW_INDEX)
					| (1 << FLD_UPDATE_FAKE_L2_PROT_EN)
#ifdef PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
					| (1ULL << FLD_UP_SRC_WITH_DST) /* Update source with destination for second pass */
#endif /* PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG */
					);
		break;

	case PPE_DRV_SC_IPSEC_EIP2PPE_ENCAP:
	case PPE_DRV_SC_IPSEC_EIP2PPE_DECAP:
		/*
		 * This is used as a tag while handling exception from PPE post EIP processing.
		 */
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_VLAN_FILTER_BYPASS:
		/*
		 * Avoid packet drop due to EG VLAN member check while accelerating bridge flows
		 * between ports with different VSIs.
		 *
		 * Avoid getting this service code in EDMA Rx descriptor, this help us to avoid
		 * an unnecessary service code callback lookup in EDMA Rx processing on all such
		 * packets.
		 *
		 * Disable source port filtering for bridge flows between two virtual ports.
		 *
		 * Avoid exception due to new MAC address learn when FDB learning is disabled.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L2_SOURCE_SEC_BYP));
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SERVICE_CODE);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_HOST_QOS:
		/*
		 * Avoid packet drop due to EG VLAN member check while accelerating bridge flows
		 * between ports with different VSIs.
		 * Avoid exception due to new MAC address learn when FDB learning is disabled.
		 * Disable source port filtering for bridge flows between two virtual ports.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L2_SOURCE_SEC_BYP));
		sc_cfg.dest_port_valid = A_FALSE;

		break;

	case PPE_DRV_SC_L3_EXCEPT:
		/*
		 * This service code is used to recognize that the packet exceptioned from PPE start
		 * from L3 header and it doesn't have a valid MAC header.
		 *
		 * We need to skip service code in flow table, so that if PPE exception the packet after
		 * flow lookup, it doesn't overwrite the service code with flow table service code (which
		 * is typically 0 for non-QoS flows).
		 */
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_SPF_BYPASS:
#ifdef PPE_DRV_DDRQ_ENABLE
		/*
		 * DDRQ WAR: Bypass packet format checks in the first pass for avoiding
		 * the PPE DATA incomplete exception.
		 */
		sc_cfg.bypass_bitmap[0] = ((1 << WRONG_PKT_FMT_L2_BYP)
						| (1 << WRONG_PKT_FMT_L3_IPV4_BYP)
						| (1 << WRONG_PKT_FMT_L3_IPV6_BYP));
#endif
		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << BRIDGING_FWD_BYP)
						| (1 << L2_SOURCE_SEC_BYP));
		sc_cfg.field_update_bitmap[0] = ((1ULL << EPE_PRE_HEADER_VALID));
		sc_cfg.dest_port_valid = A_TRUE;
		break;

#ifdef PPE_UDP_ST_SUPPORT
	case PPE_DRV_SC_UDP_ST:
#ifdef PPE_DRV_DDRQ_ENABLE
		/*
		 * DDRQ WAR: Bypass packet format checks in the first pass for avoiding
		 * the PPE DATA incomplete exception.
		 */
		sc_cfg.bypass_bitmap[0] = ((1 << WRONG_PKT_FMT_L2_BYP)
						| (1 << WRONG_PKT_FMT_L3_IPV4_BYP)
						| (1 << WRONG_PKT_FMT_L3_IPV6_BYP)
						| (1 << WRONG_PKT_FMT_L4_BYP));

		sc_cfg.field_update_bitmap[0] = ((1ULL << EPE_PRE_HEADER_VALID)
						| (1ULL << FLD_UP_SERVICE_CODE_DDRQ));
#endif
		sc_cfg.bypass_bitmap[0] |= (1 << FLOW_SERVICE_CODE_BYP);

		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
					| (1 << SOURCE_FLTR_BYP)
					| (1 << BRIDGING_FWD_BYP)
					| (1 << L2_SOURCE_SEC_BYP));

		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_UDP_ST_NEXT:
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.bypass_bitmap[1] = (1 << FLOW_POLICER_BYP);
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_UDP_ST_HGU:
#ifdef PPE_DRV_DDRQ_ENABLE
		/*
		 * DDRQ WAR: Bypass packet format checks in the first pass for avoiding
		 * the PPE DATA incomplete exception.
		 */
		sc_cfg.bypass_bitmap[0] = ((1 << WRONG_PKT_FMT_L2_BYP)
					| (1 << WRONG_PKT_FMT_L3_IPV4_BYP)
					| (1 << WRONG_PKT_FMT_L3_IPV6_BYP)
					| (1 << WRONG_PKT_FMT_L4_BYP));
#endif

		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
					| (1 << SOURCE_FLTR_BYP)
					| (1 << BRIDGING_FWD_BYP)
					| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * Handle the HGU case
		 */
		sc_cfg.field_update_bitmap[0] = (1ULL << FLD_UP_SRC_WITH_DST)
					| (1ULL << EPE_DST_INFO_WITH_PROFILE)
					| (1ULL << FLD_UPDATE_FLOW_IDX)
					| (1UL << FLD_UPDATE_DST_INFO_BYPASS);
		sc_cfg.bypass_bitmap[1] |= (1 << L2_VP_SERVICE_CODE_ENQ_BYP);

		sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_DST;

		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_UDP_ST_HGU_NEXT:
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP)
					| (1 << FLOW_LOOKUP_BYP);
		sc_cfg.bypass_bitmap[1] = (1 << L2_SOURCE_SEC_BYP)
					| (1 << EG_VLAN_MEMBER_CHECK_BYP);
		sc_cfg.dest_port_valid = A_FALSE;

		sc_cfg.bypass_bitmap[1] |= (1 << FLOW_POLICER_BYP);
		break;

	case PPE_DRV_SC_UDP_ST_SFU:
#ifdef PPE_DRV_DDRQ_ENABLE
		/*
		 * DDRQ WAR: Bypass packet format checks in the first pass for avoiding
		 * the PPE DATA incomplete exception.
		 */
		sc_cfg.bypass_bitmap[0] = ((1 << WRONG_PKT_FMT_L2_BYP)
					| (1 << WRONG_PKT_FMT_L3_IPV4_BYP)
					| (1 << WRONG_PKT_FMT_L3_IPV6_BYP)
					| (1 << WRONG_PKT_FMT_L4_BYP));
#endif
		sc_cfg.bypass_bitmap[0] |= (1 << FLOW_SERVICE_CODE_BYP)
							| (1 << MY_MAC_CHECK_BYP);

		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
					| (1 << SOURCE_FLTR_BYP)
					| (1 << BRIDGING_FWD_BYP)
					| (1 << L2_SOURCE_SEC_BYP)
					| (1 << L2_PKT_EDIT_BYP)
					| (1 << L3_PKT_EDIT_BYP));

		sc_cfg.field_update_bitmap[0] = ((1ULL << EPE_PRE_HEADER_VALID)
						| (1ULL << FLD_UP_SERVICE_CODE_DDRQ));

		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_UDP_ST_SFU_NEXT:
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP)
							| (1 << MY_MAC_CHECK_BYP);
		sc_cfg.bypass_bitmap[1] = ((1 << FLOW_POLICER_BYP)
					| (1 << L2_PKT_EDIT_BYP)
					| (1 << L3_PKT_EDIT_BYP));
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;
#endif

	case PPE_DRV_SC_NOEDIT_RFS_RULE:
	case PPE_DRV_SC_NOEDIT_REDIR_CORE0:
	case PPE_DRV_SC_NOEDIT_REDIR_CORE1:
	case PPE_DRV_SC_NOEDIT_REDIR_CORE2:
	case PPE_DRV_SC_NOEDIT_REDIR_CORE3:
		/*
		 * For service code PPE_DRV_SC_NOEDIT_RFS_RULE we don't want it to take priority over destination port.
		 */
		if (sc == PPE_DRV_SC_NOEDIT_RFS_RULE) {
			sc_cfg.dest_port_valid = A_FALSE;
		}

		/*
		 * Don't update destination information and service code in EDMA
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO) | (1 << FLD_UPDATE_SERVICE_CODE) | (1 << FLD_UPDATE_MAC_HDR_BYPASS));

		/*
		 * Avoid packet drop due to source port filtering for no edit service codes, in this
		 * case when FDB entry is found, the destination generated by service code overwrites
		 * the FDB based destination, this is needed to overcome ACL match counter problem in PPE.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << SOURCE_FLTR_BYP)
						| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * Avoid any packet editing
		 */
		sc_cfg.bypass_bitmap[1] |= ((1 << L2_PKT_EDIT_BYP) | (1 << L3_PKT_EDIT_BYP));

		/*
		 * Avoid packet drop due to FAKE MAC entry mismatch
		 */
		sc_cfg.bypass_bitmap[1] |= (1 << FAKE_MAC_DROP_BYP);

		break;

	case PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH:
		/*
		 * For service code PPE_DRV_SC_NOEDIT_RFS_RULE we don't want it to take priority over destination port.
		 */
		if (sc == PPE_DRV_SC_NOEDIT_RFS_RULE) {
			sc_cfg.dest_port_valid = A_FALSE;
		}

		/*
		 * This service code is generated for IPsec outer flows. On SPI ACL
		 * lookup failure, bypasses updates to the following fields and
		 * preserves the original packet contents:
		 * - Destination information
		 * - Service code
		 * - MAC header update
		 * - PPPoE header update
		 * - TTL update
		 * - DSCP update
		 * - Source IP update
		 * - Destination IP update
		 * - L4 port update
		 * - VLAN translation
		 * - VLAN formatting
		 */
		sc_cfg.field_update_bitmap[0] = ((1ULL << FLD_UPDATE_DEST_INFO)
				| (1ULL << FLD_UPDATE_SERVICE_CODE)
				| (1ULL << FLD_UPDATE_MAC_HDR_BYPASS)
				| (1ULL << FLD_UP_PPPOE_BYPASS)
				| (1ULL << FLD_UP_TTL_BYPASS)
				| (1ULL << FLD_UP_DSCP_BYPASS)
				| (1ULL << FLD_UP_SIP_BYPASS)
				| (1ULL << FLD_UP_DIP_BYPASS)
				| (1ULL << FLD_UP_L4_PORT_BYPASS)
				| (1ULL << EPE_VLAN_XLT_BYPASS)
				| (1ULL << EPE_VLAN_FMT_BYPASS));

		/*
		 * Avoid packet drop due to source port filtering for no edit service codes, in this
		 * case when FDB entry is found, the destination generated by service code overwrites
		 * the FDB based destination, this is needed to overcome ACL match counter problem in PPE.
		 * Avoid packet drop due to FAKE MAC entry mismatch.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << SOURCE_FLTR_BYP)
				| (1 << L2_SOURCE_SEC_BYP)
				| (1 << FAKE_MAC_DROP_BYP));

#ifdef PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
		/*
		 * Full inline IPsec path - align with LP_NEXT(96XX) service code behavior:
		 * - Bypass flow service code lookup to avoid re-classification on second pass.
		 * - Bypass VLAN member check, bridging, VP service code enqueue and
		 *   DOT1P/VLAN loopback processing.
		 */
		sc_cfg.bypass_bitmap[0] |= (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.bypass_bitmap[1] |= ((1 << EG_VLAN_MEMBER_CHECK_BYP)
				| (1 << BRIDGING_FWD_BYP)
				| (1 << L2_VP_SERVICE_CODE_ENQ_BYP)
				| (1 << DOT1P_VLAN_FROM_LOOPBACK));
#endif /* PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG */

		break;

	case PPE_DRV_SC_EDIT_RFS_RULE:
	case PPE_DRV_SC_EDIT_REDIR_CORE0:
	case PPE_DRV_SC_EDIT_REDIR_CORE1:
	case PPE_DRV_SC_EDIT_REDIR_CORE2:
	case PPE_DRV_SC_EDIT_REDIR_CORE3:
		/*
		 * For service code PPE_DRV_SC_EDIT_RFS_RULE we don't want it to take priority over destination port.
		 */
		if (sc == PPE_DRV_SC_EDIT_RFS_RULE) {
			sc_cfg.dest_port_valid = A_FALSE;
		}

		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * Don't update service code in EDMA
		 */
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SERVICE_CODE);
		break;

	case PPE_DRV_SC_LOOPBACK_RING:
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_LOOPBACK_RING_NEXT:
		/*
		 * Set BYPASS_ALL parameters here to bypass the flow lookup again for
		 * routing case
		 */
		sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_DST;
		sc_cfg.dest_port_id = redir_port;
		sc_cfg.next_service_code = next_sc;
		sc_cfg.dest_port_valid = A_FALSE;
        	sc_cfg.bypass_bitmap[0] = ~((1 << FAKE_MAC_HEADER_BYP)
                                        | (1 << SERVICE_CODE_BYP)
                                        | (1 << FAKE_L2_PROTO_BYP)
                                        | (1 << MY_MAC_CHECK_BYP));
       		sc_cfg.bypass_bitmap[0] |= (1 << FLOW_SERVICE_CODE_BYP);

       		sc_cfg.bypass_bitmap[1] = ~(1 << ACL_POST_ROUTING_CHECK_BYP);

#ifdef NSS_PPE_L2_VP_SC_ENQ_BYPASS
		sc_cfg.bypass_bitmap[1] |= (1 << L2_VP_SERVICE_CODE_ENQ_BYP);
#endif
		break;

	case PPE_DRV_SC_LOOPBACK_RING_GRETAP_MAPT:
		sc_cfg.bypass_bitmap[1] = (1 << L2_SOURCE_SEC_BYP);
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
		sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_SRC;
		break;

	case PPE_DRV_SC_LOOPBACK_RING_MAPT_GRETAP:
		sc_cfg.bypass_bitmap[1] = (1 << L2_SOURCE_SEC_BYP);
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP) | (1 << TX_COUNTER_BYP);
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SRC_INFO_BYPASS);
		break;

	case PPE_DRV_SC_LOOPBACK_RING_NEXT_GRETAP_N_MAPT:
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP) | (1 << FLOW_LOOKUP_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	/*
	 * Based on the type of loopback port feature enabled, configure
	 * the service code.
	 */
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_FLOW_SC:
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_FLOW_SC_NEXT:
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_RETURN_SC:
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_RETURN_SC_NEXT:
		break;

#ifdef PPE_LOOPBACK_PORT_SUPPORT
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC:
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC_NEXT:
		if (ppe_drv_loopback_port_ft_pon_enabled(p)) {
			ppe_drv_sc_config_lpbk_port_pon(sc, &sc_cfg);
		}
		break;
#endif
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC:
		if (ppe_drv_loopback_port_ft_pon_hgu_us_enabled(p)) {
			sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_DST;
			sc_cfg.field_update_bitmap[0] = (1ULL << FLD_UP_SRC_WITH_DST)
							| (1ULL << EPE_DST_INFO_WITH_PROFILE)
							| (1ULL << FLD_UPDATE_FLOW_IDX)
							| (1UL << FLD_UPDATE_DST_INFO_BYPASS);
			sc_cfg.bypass_bitmap[1] = (1 << L2_VP_SERVICE_CODE_ENQ_BYP)
						| (1 << L2_SOURCE_SEC_BYP);
			sc_cfg.dest_port_valid = A_FALSE;
			ppe_drv_trace("Configured sc here bitmap[1]:%llu bitmap[2]:%llu dir:%u update_bitmap:%llu\n",
					sc_cfg.bypass_bitmap[0], sc_cfg.bypass_bitmap[1], sc_cfg.direction, sc_cfg.field_update_bitmap[0]);
		}
		break;

	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT:
		if (ppe_drv_loopback_port_ft_pon_hgu_us_enabled(p)) {
			sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP)
						| (1 << FLOW_LOOKUP_BYP);
			sc_cfg.bypass_bitmap[1] = (1 << L2_SOURCE_SEC_BYP)
						| (1 << EG_VLAN_MEMBER_CHECK_BYP);
			sc_cfg.field_update_bitmap[0] = (1ULL << FLD_UP_DST_INFO_WITH_GEM)
						| (1ULL << FLD_UP_DST_INFO_FORCE)
						| (1ULL << EPE_PRE_HEADER_VALID)
						| (1ULL << EPE_LOOPBACK_VLAN_START);
			sc_cfg.dest_port_valid = A_FALSE;
		}
		break;

	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT_DDRQ:
		if (ppe_drv_loopback_port_ft_pon_hgu_us_enabled(p)) {
			sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP);
			sc_cfg.dest_port_valid = A_FALSE;
		}
		break;

	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC:
		if (ppe_drv_loopback_port_ft_pon_hgu_ds_enabled(p)) {
			sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_SRC;
			sc_cfg.field_update_bitmap[0] = (1ULL << FLD_UP_SRC_WITH_DST)
							| (1ULL << EPE_DST_INFO_WITH_PROFILE);
			sc_cfg.dest_port_valid = A_FALSE;
			ppe_drv_trace("Configured sc here bitmap[1]:%llu bitmap[2]:%llu dir:%u update_bitmap:%llu\n",
					sc_cfg.bypass_bitmap[0], sc_cfg.bypass_bitmap[1], sc_cfg.direction, sc_cfg.field_update_bitmap[0]);
		}
		break;

	case PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC_NEXT:
		if (ppe_drv_loopback_port_ft_pon_hgu_ds_enabled(p)) {
			sc_cfg.bypass_bitmap[0] = (1 << L3_MY_MAC_DST_BYP);
			sc_cfg.bypass_bitmap[1] = (1 << L2_SOURCE_SEC_BYP);
			sc_cfg.dest_port_valid = A_TRUE;
		}
		break;
	case PPE_DRV_SC_FEATURE_PON_HGU_PPTP:
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_LOOKUP_BYP);
		sc_cfg.bypass_bitmap[1] = (1 << EG_VLAN_MEMBER_CHECK_BYP);
		sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_DST;
		sc_cfg.dest_port_valid = A_FALSE;
		break;
#endif

#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	case PPE_DRV_SC_NPT66_HAIRPIN_NAT:
		sc_cfg.bypass_bitmap[1] = ((1 << L2_PKT_EDIT_BYP) | (1 << L2_SOURCE_SEC_BYP));
		break;

	case PPE_DRV_SC_NPT66_HAIRPIN_NAT_NEXT:
		sc_cfg.bypass_bitmap[0] = (1 << MY_MAC_CHECK_BYP) | (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.bypass_bitmap[1] = ((1 << FDB_LEARN_BYP) | (1 << L2_SOURCE_SEC_BYP) | (1 << FDB_REFRESH_BYP));
		sc_cfg.dest_port_valid = A_FALSE;
		break;
#endif

#if defined (PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	case PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER:
		sc_cfg.dest_port_valid = A_TRUE;
		sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_DST;
		break;

	case PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER_NEXT:
		sc_cfg.direction = PPE_DRV_SC_IN_L2_DIR_SRC;
		sc_cfg.bypass_bitmap[0] = ~((1 << FAKE_MAC_HEADER_BYP)
					| (1 << SERVICE_CODE_BYP)
					| (1 << FAKE_L2_PROTO_BYP)
					| (1 << MY_MAC_CHECK_BYP));
		sc_cfg.bypass_bitmap[1] = ~((1ULL << ACL_POST_ROUTING_CHECK_BYP)
#ifdef NSS_PPE_IPQ52XX
				| (1ULL << DOT1P_MAPPER_BYP)
				| (1ULL << DOT1P_DST_LOOKUP_BYPASS)
#endif
#if defined(NSS_PPE_IPQ96XX) || defined(NSS_PPE_IPQ52XX)
				| (1ULL << IN_MIRROR_BYP)
				| (1ULL << EG_MIRROR_BYP)
#endif
				);
		sc_cfg.dest_port_valid = A_FALSE;
		break;
#endif
	case PPE_DRV_SC_VP_RPS:
		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * Don't update service code in EDMA
		 */
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SERVICE_CODE);
		break;

	case PPE_DRV_SC_NOEDIT_ACL_POLICER:
		/*
		 * Don't update destination information and service code in EDMA
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO) | (1 << FLD_UPDATE_SERVICE_CODE));

		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << SOURCE_FLTR_BYP)
						| (1 << BRIDGING_FWD_BYP)
						| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * Avoid any packet editing
		 */
		sc_cfg.bypass_bitmap[1] |= ((1 << L2_PKT_EDIT_BYP) | (1 << L3_PKT_EDIT_BYP));

		break;

	case PPE_DRV_SC_L2_TUNNEL_EXCEPTION:
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SERVICE_CODE);
		sc_cfg.bypass_bitmap[1] = ((1 << BRIDGING_FWD_BYP) | (1 << L2_SOURCE_SEC_BYP));
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case  PPE_DRV_SC_NOEDIT_PRIORITY_SET:
		/*
		 * Don't update destination information and service code in EDMA
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO) | (1 << FLD_UPDATE_SERVICE_CODE));

		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, with SPF bypass service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << SOURCE_FLTR_BYP)
						| (1 << BRIDGING_FWD_BYP)
						| (1 << L2_SOURCE_SEC_BYP));
		/*
		 * Avoid any packet editing
		 */
		sc_cfg.bypass_bitmap[1] |= ((1 << L2_PKT_EDIT_BYP) | (1 << L3_PKT_EDIT_BYP));

		/*
		 * Enable ring selection based on int_pri value
		 */
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case  PPE_DRV_SC_NOEDIT_RULE:
		/*
		 * Avoid any packet editing
		 */
		sc_cfg.bypass_bitmap[1] |= ((1 << L2_PKT_EDIT_BYP) | (1 << L3_PKT_EDIT_BYP));
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_FMAC_BYPASS:
#ifdef PPE_DRV_DDRQ_ENABLE
		/*
		 * DDRQ WAR: Bypass packet format checks in the first pass for avoiding
		 * the PPE DATA incomplete exception.
		 */
		sc_cfg.bypass_bitmap[0] = ((1 << WRONG_PKT_FMT_L2_BYP)
						| (1 << WRONG_PKT_FMT_L3_IPV4_BYP)
						| (1 << WRONG_PKT_FMT_L3_IPV6_BYP)
						| (1 << WRONG_PKT_FMT_L4_BYP));
#endif
		sc_cfg.bypass_bitmap[0] |= ((1 << FAKE_MAC_HEADER_BYP) | (1 <<  FLOW_LOOKUP_BYP));
		sc_cfg.bypass_bitmap[1] = ((1 << FAKE_MAC_DROP_BYP) | (1<< BRIDGING_FWD_BYP));
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_VP_MPSK:
		/*
		 * Avoid packet drop due to source port filtering and avoid FDB based forwarding for
		 * packets sent to PPE, also bypass the egress VLAN table with MPSK specific service code.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L2_SOURCE_SEC_BYP)
						| (1 << EG_VLAN_XLT_BYP));
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_FDB_BYPASS:
		/*
		 * bypass the dest mac lookup forward
		 */
		sc_cfg.bypass_bitmap[1] =(1 << BRIDGING_FWD_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	case PPE_DRV_SC_PKT_EXCEPTION_EDIT_EN:
		/*
		 * Program the service code table with exception edit enable.
		 */
		sc_cfg.field_update_bitmap[0] = (uint64_t)(1ULL << FLD_UPDATE_CPU_EDIT_EN);
		sc_cfg.dest_port_valid = A_FALSE;
		break;
#endif
	case PPE_DRV_SC_GEM_LOOKUP:
		/*
		 * This service code is for GEM port lookup.
		 * Initially bypass flow lookup and editing.
		 */
		sc_cfg.bypass_bitmap[0] = ~((1 << FAKE_MAC_HEADER_BYP)
				| (1 << SERVICE_CODE_BYP)
				| (1 << FAKE_L2_PROTO_BYP)
				| (1 << MY_MAC_CHECK_BYP));
		sc_cfg.bypass_bitmap[1] = ~((1ULL << ACL_POST_ROUTING_CHECK_BYP)
				| (1ULL << DOT1P_MAPPER_BYP)
				| (1 << PCP_QOS_BYP)
#if defined(NSS_PPE_IPQ96XX) || defined(NSS_PPE_IPQ52XX)
				| (1ULL << IN_MIRROR_BYP)
				| (1ULL << EG_MIRROR_BYP)
#endif
				);
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
		sc_cfg.bypass_bitmap[3] = (1 << TL_PRE_IPO_BYP);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP:
		sc_cfg.offset_sel = PPE_DRV_SC_IN_L2_OFF_L3;
		sc_cfg.hw_services = PPE_DRV_EIP_HWSERVICE_IPSEC_IIP;

		/*
		 * PPE2EIP encap specific bypass bits - avoid L3 packet editing
		 * and source security check.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << L3_PKT_EDIT_BYP)
				| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * PPE2EIP encap base field update bits.
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO)
				| (1 << FLD_UPDATE_HASH_FLOW_INDEX)
				| (1 << FLD_UPDATE_FAKE_L2_PROT_EN));

#ifdef PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
		/*
		 * Full inline IPsec path - combine LP_NEXT(96XX) bypass and field update bits
		 * with PPE2EIP encap bits:
		 * - Bypass flow service code lookup to avoid re-classification on second pass.
		 * - Bypass VLAN member check, bridging, VP service code enqueue and
		 *   DOT1P/VLAN loopback processing.
		 * - Update source with destination for second pass.
		 */
		sc_cfg.bypass_bitmap[0] |= (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.bypass_bitmap[1] |= ((1 << EG_VLAN_MEMBER_CHECK_BYP)
				| (1 << BRIDGING_FWD_BYP)
				| (1 << SOURCE_FLTR_BYP)
				| (1 << L2_VP_SERVICE_CODE_ENQ_BYP)
				| (1 << DOT1P_VLAN_FROM_LOOPBACK));
		sc_cfg.field_update_bitmap[0] |= (1ULL << FLD_UP_SRC_WITH_DST);
#endif /* PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG */
		break;

#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	case PPE_DRV_SC_NOEDIT_TUN_RPS:
		/*
		 * Don't update destination information and service code in EDMA.
		 * Also disable decap as the packets would be decapsulated in host
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO) | (1 << FLD_UPDATE_SERVICE_CODE) |
							(1 << FLD_UPDATE_MAC_HDR_BYPASS) | (1 << FLD_UPDATE_DECAP_DIS));

		/*
		 * Avoid any packet editing
		 */
		sc_cfg.bypass_bitmap[1] |= ((1 << L2_PKT_EDIT_BYP) | (1 << L3_PKT_EDIT_BYP));

		/*
		 * Avoid packet drop due to FAKE MAC entry mismatch
		 */
		sc_cfg.bypass_bitmap[1] |= (1 << FAKE_MAC_DROP_BYP);
		break;
#endif

	case PPE_DRV_SC_FLOW_ACL_FIRST ... PPE_DRV_SC_FLOW_ACL_LAST:
		/*
		 * These are primarily used for N-tuple lookup or flow+policer combintation.
		 * Don't update service code in EDMA and there is no redirection needed with these SCs.
		 */
		sc_cfg.field_update_bitmap[0] = (1 << FLD_UPDATE_SERVICE_CODE);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

#ifdef PPE_DRV_DDRQ_ENABLE
	case PPE_DRV_SC_DDRQ_ETH_PT_MODE:

		sc_cfg.bypass_bitmap[0] = ~((1 << MY_MAC_CHECK_BYP)
				| (1 << FAKE_MAC_HEADER_BYP)
				| (1 << SERVICE_CODE_BYP)
				| (1 << FAKE_L2_PROTO_BYP));

		sc_cfg.bypass_bitmap[1] = ~((1 << ACL_POST_ROUTING_CHECK_BYP)
				| (1ULL << QM_QID_MISMATCH_BYPASS)
				| (1ULL << QM_DDRQ_ID_GEN_BYPASS)
				| (1ULL << QM_DDRQ_ID_GEN_FORCE)
				| (1ULL << SAWF_BYP)
				| (1ULL << QM_PASSTHROUGH_CPU_CODE_1_SEL)
				| (1ULL << DROP_CPUCODE_CNT_BYP)
				| (1ULL << (DROP_CPUCODE_CNT_BYP + 1))
				| (1ULL << PCP_QOS_BYP)
				| (1ULL << IN_MIRROR_BYP)
				| (1ULL << EG_MIRROR_BYP));
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
		sc_cfg.bypass_bitmap[3] = (1 << TL_PRE_IPO_BYP);
		sc_cfg.field_update_bitmap[0] = (1ULL << EPE_PRE_HEADER_VALID);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_DDRQ_PON_PT_MODE:

		sc_cfg.bypass_bitmap[0] = ~((1 << MY_MAC_CHECK_BYP)
				| (1 << FAKE_MAC_HEADER_BYP)
				| (1 << SERVICE_CODE_BYP)
				| (1 << FAKE_L2_PROTO_BYP));

		sc_cfg.bypass_bitmap[1] = ~((1ULL << ACL_POST_ROUTING_CHECK_BYP)
				| (1ULL << DOT1P_MAPPER_BYP)
				| (1ULL << DOT1P_DST_LOOKUP_BYPASS)
				| (1ULL << DOT1P_UNI_LOOKUP_REVERSE)
				| (1ULL << QM_DDRQ_ID_GEN_BYPASS)
				| (1ULL << QM_PASSTHROUGH_CPU_CODE_0_SEL)
				| (1ULL << QM_PASSTHROUGH_CPU_CODE_1_SEL)
				| (1ULL << DROP_CPUCODE_CNT_BYP)
				| (1ULL << PCP_QOS_BYP)
				| (1ULL << IN_MIRROR_BYP)
				| (1ULL << EG_MIRROR_BYP));
		sc_cfg.bypass_bitmap[2] = (1 << RX_COUNTER_BYP);
		sc_cfg.bypass_bitmap[3] = (1 << TL_PRE_IPO_BYP);
		sc_cfg.field_update_bitmap[0] = (1ULL << EPE_PRE_HEADER_VALID);
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_DDRQ_LP_SC:
	case PPE_DRV_SC_DDRQ_LP_SC_BRIDGE:
	case PPE_DRV_SC_DDRQ_LP_SC_IPSEC_ENCAP:
	case PPE_DRV_SC_DDRQ_LP_SC_IPSEC_DECAP:
		sc_cfg.field_update_bitmap[0] = ((1ULL << EPE_LOOPBACK_VLAN_START)
				| (1ULL << FLD_UP_DST_INFO_WITH_GEM)
				| (1ULL << EPE_PRE_HEADER_VALID)
				| (1ULL << FLD_UP_SERVICE_CODE_DDRQ));
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << L2_SOURCE_SEC_BYP)
						| (1 << BRIDGING_FWD_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L3_PKT_EDIT_BYP));
		if (sc != PPE_DRV_SC_DDRQ_LP_SC_BRIDGE) {
			sc_cfg.bypass_bitmap[1] |= (1 << L2_PKT_EDIT_BYP);
		}
		sc_cfg.dest_port_valid = A_TRUE;
		break;

	case PPE_DRV_SC_DDRQ_LP_SC_NEXT:
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_SERVICE_CODE_BYP);
		sc_cfg.bypass_bitmap[1] = ((1 << EG_VLAN_MEMBER_CHECK_BYP)
						| (1 << L2_SOURCE_SEC_BYP)
						| (1 << BRIDGING_FWD_BYP)
						| (1 << SOURCE_FLTR_BYP)
						| (1 << L2_VP_SERVICE_CODE_ENQ_BYP)
						| (1 << DOT1P_VLAN_FROM_LOOPBACK));
		sc_cfg.dest_port_valid = A_FALSE;
		break;

	case PPE_DRV_SC_IPSEC_EDMA_VP_ENCAP:
		sc_cfg.offset_sel = PPE_DRV_SC_IN_L2_OFF_L3;
		sc_cfg.hw_services = PPE_DRV_EIP_HWSERVICE_IPSEC_IIP;

		/*
		 * PPE2EIP encap specific bypass bits - avoid L3 packet editing
		 * and source security check.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << L3_PKT_EDIT_BYP)
				| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * PPE2EIP encap base field update bits.
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO)
				| (1 << FLD_UPDATE_HASH_FLOW_INDEX)
				| (1 << FLD_UPDATE_FAKE_L2_PROT_EN));

#ifdef PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
		/*
		 * Full inline IPsec path - combine LP_NEXT(96XX) bypass and field update bits
		 * with PPE2EIP encap bits:
		 * - Bypass flow service code lookup to avoid re-classification on second pass.
		 * - Bypass VLAN member check, bridging, VP service code enqueue and
		 *   DOT1P/VLAN loopback processing.
		 * - Update source with destination for second pass.
		 */
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_LOOKUP_BYP);
		sc_cfg.bypass_bitmap[1] |= ((1 << EG_VLAN_MEMBER_CHECK_BYP)
				| (1 << BRIDGING_FWD_BYP)
				| (1 << SOURCE_FLTR_BYP)
				| (1 << L2_VP_SERVICE_CODE_ENQ_BYP)
				| (1 << DOT1P_VLAN_FROM_LOOPBACK)
				| (1 << FAKE_MAC_DROP_BYP));
		sc_cfg.field_update_bitmap[0] |= (1ULL << FLD_UP_SRC_WITH_DST);
#endif /* PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG */
		break;

	case PPE_DRV_SC_IPSEC_EDMA_VP_DECAP:
		sc_cfg.offset_sel = PPE_DRV_SC_IN_L2_OFF_L3;
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_LOOKUP_BYP);
		sc_cfg.bypass_bitmap[1] = ((1 << L3_PKT_EDIT_BYP)
				| (1 << L2_SOURCE_SEC_BYP)
				| (1 << FAKE_MAC_DROP_BYP));
		sc_cfg.hw_services = PPE_DRV_EIP_HWSERVICE_IPSEC_AIIP;
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO)
					| (1 << FLD_UPDATE_HASH_FLOW_INDEX)
					| (1 << FLD_UPDATE_FAKE_L2_PROT_EN)
#ifdef PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
					| (1ULL << FLD_UP_SRC_WITH_DST) /* Update source with destination for second pass */
#endif /* PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG */
					);
		break;

	case PPE_DRV_SC_IPSEC_EDMA_VP_3_TUPLE_ENCAP:
		sc_cfg.offset_sel = PPE_DRV_SC_IN_L2_OFF_L3;
		sc_cfg.hw_services = PPE_DRV_EIP_HWSERVICE_IPSEC_AIIP;

		/*
		 * PPE2EIP encap specific bypass bits - avoid L3 packet editing
		 * and source security check.
		 */
		sc_cfg.bypass_bitmap[1] = ((1 << L3_PKT_EDIT_BYP)
				| (1 << L2_SOURCE_SEC_BYP));

		/*
		 * PPE2EIP encap base field update bits.
		 */
		sc_cfg.field_update_bitmap[0] = ((1 << FLD_UPDATE_DEST_INFO)
				| (1 << FLD_UPDATE_HASH_FLOW_INDEX)
				| (1 << FLD_UPDATE_FAKE_L2_PROT_EN));

#ifdef PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG
		/*
		 * Full inline IPsec path - combine LP_NEXT(96XX) bypass and field update bits
		 * with PPE2EIP encap bits:
		 * - Bypass flow service code lookup to avoid re-classification on second pass.
		 * - Bypass VLAN member check, bridging, VP service code enqueue and
		 *   DOT1P/VLAN loopback processing.
		 * - Update source with destination for second pass.
		 */
		sc_cfg.bypass_bitmap[0] = (1 << FLOW_LOOKUP_BYP);
		sc_cfg.bypass_bitmap[1] |= ((1 << EG_VLAN_MEMBER_CHECK_BYP)
				| (1 << BRIDGING_FWD_BYP)
				| (1 << SOURCE_FLTR_BYP)
				| (1 << L2_VP_SERVICE_CODE_ENQ_BYP)
				| (1 << DOT1P_VLAN_FROM_LOOPBACK)
				| (1 << FAKE_MAC_DROP_BYP));
		sc_cfg.field_update_bitmap[0] |= (1ULL << FLD_UP_SRC_WITH_DST);
#endif /* PPE_DRV_IPSEC_FULL_INLINE_SC_CONFIG */
		break;
#endif

	default:
		ppe_drv_warn("%p: service code %u not supported", p, sc);
		return;
	}

	/*
	 * Program the service code tables through SSDK.
	 */
	err = fal_servcode_config_set(PPE_DRV_SWITCH_ID, sc, &sc_cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: service code configuration failed for sc: %u", p, sc);
	}

	ppe_drv_sc_dump(sc);
}

/*
 * ppe_drv_sc_process_skbuff()
 *	Pass on skbuff to the registered callback.
 */
bool ppe_drv_sc_process_skbuff(struct ppe_drv_sc_metadata *sc, struct sk_buff *skb)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_sc *psc;
	ppe_drv_sc_callback_t cb;
	struct ppe_drv_sc_vp_info *vp_info;
	void *app_data;
	bool ret;
	uint16_t port_num;
	uint16_t service_code;

	/*
	 * Handle VP callbacks for the service codes.
	 */
	service_code = sc->service_code;
	if ((sc->vp_num >= PPE_DRV_VIRTUAL_START) && (sc->vp_num < PPE_DRV_VIRTUAL_END)) {
		rcu_read_lock();
		psc = &p->sc[service_code];
		port_num = sc->vp_num - PPE_DRV_VIRTUAL_START;
		vp_info = rcu_dereference(psc->vp_info[port_num]);
		if (!vp_info){
			rcu_read_unlock();
			ppe_drv_trace("%p: No VP specific callback info in service code %u VP %u\n", p, service_code, sc->vp_num);
			goto check_common_sc_cb;
		}

		cb = vp_info->cb;
		app_data = vp_info->app_data;

		ppe_drv_trace("%p: processing skb:%p skb->mark: 0x%x sc:%u vp:%u vp_cb:%p app:%p", p, skb, skb->mark, service_code, sc->vp_num, cb, app_data);

		if (unlikely(!cb)) {
			rcu_read_unlock();
			ppe_drv_info("%p: callback not registered for SC:%u VP: %d", p, service_code, sc->vp_num);
			ppe_drv_stats_inc(&p->stats.sc_stats[service_code].sc_vp_cb_unregister);
			goto check_common_sc_cb;
		}

		ret = cb(app_data, skb, sc);
		if (ret) {
			ppe_drv_stats_inc(&p->stats.sc_stats[service_code].sc_vp_cb_packet_consumed);
		} else {
			ppe_drv_stats_inc(&p->stats.sc_stats[service_code].sc_vp_cb_packet_processed);
		}

		rcu_read_unlock();
		return ret;
	}

check_common_sc_cb:
	spin_lock_bh(&p->lock);
	psc = &p->sc[service_code];
	cb = psc->cb;
	app_data = psc->app_data;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("%p: processing skb:%p sc:%u cb:%p app:%p", p, skb, service_code, cb, app_data);

	if (!cb) {
		ppe_drv_info("%p: callback not registered for SC:%u", p, service_code);
		ppe_drv_stats_inc(&p->stats.sc_stats[service_code].sc_cb_unregister);
		return false;
	}

	ret = cb(app_data, skb, sc);
	if (ret) {
		ppe_drv_stats_inc(&p->stats.sc_stats[service_code].sc_cb_packet_consumed);
	} else {
		ppe_drv_stats_inc(&p->stats.sc_stats[service_code].sc_cb_packet_processed);
	}

	return ret;
}
EXPORT_SYMBOL(ppe_drv_sc_process_skbuff);

/*
 * ppe_drv_sc_unregister_cb()
 *	Unregister callback for a give service code
 */
void ppe_drv_sc_unregister_cb(ppe_drv_sc_t sc)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_sc *psc;

	spin_lock_bh(&p->lock);
	psc = &p->sc[sc];
	ppe_drv_assert(psc->cb, "%p: no cb registered for sc: %u", p, sc);
	psc->cb = NULL;
	psc->app_data = NULL;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%p: unregistered cb/app_data for sc:%u", p, sc);
}
EXPORT_SYMBOL(ppe_drv_sc_unregister_cb);

/*
 * ppe_drv_sc_register_cb()
 *	Registers a pnode for a given service code for redirection.
 */
void ppe_drv_sc_register_cb(ppe_drv_sc_t sc, ppe_drv_sc_callback_t cb, void *app_data)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_sc *psc;

	ppe_drv_assert(cb, "%p: cannot register null cb for sc %u", p, sc);

	spin_lock_bh(&p->lock);
	psc = &p->sc[sc];
	ppe_drv_assert(!psc->cb, "%p: multiple registration for sc:%u - "
				"prev cb:%p current cb:%p", p, sc, psc->cb, cb);
	psc->cb = cb;
	psc->app_data = app_data;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%p: registered cb:%p app_data:%p for sc:%u", p, cb, app_data, sc);
}
EXPORT_SYMBOL(ppe_drv_sc_register_cb);

/*
 * ppe_drv_sc_unregister_cb()
 *	Unregister callback for a give service code
 */
void ppe_drv_sc_unregister_vp_cb(ppe_drv_sc_t sc, uint16_t vp_num)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_sc *psc;
	struct ppe_drv_sc_vp_info *vp_info;

	ppe_drv_assert(((vp_num >= PPE_DRV_VIRTUAL_START) && (vp_num < PPE_DRV_VIRTUAL_END)), "%px: Invalid port number: %d", p, vp_num);

	if ((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END)) {
		ppe_drv_warn("%px: invalid VP number %d", p, vp_num);
		return;
	}

	spin_lock_bh(&p->lock);
	psc = &p->sc[sc];
	vp_info = rcu_dereference_protected(psc->vp_info[vp_num - PPE_DRV_VIRTUAL_START], 1);
	if (!vp_info) {
		ppe_drv_warn("%p: no vp information for sc %u vp %u\n", p, sc, vp_num);
		spin_unlock_bh(&p->lock);
		return;
	}

	ppe_drv_assert(vp_info->cb, "%p: no cb registered for sc: %u vp: %u", p, sc, vp_num);
	rcu_assign_pointer(psc->vp_info[vp_num - PPE_DRV_VIRTUAL_START], NULL);

	spin_unlock_bh(&p->lock);
	synchronize_rcu();
	kfree(vp_info);

	ppe_drv_info("%p: unregistered cb/app_data for sc:%u vp: %u", p, sc, vp_num);
}
EXPORT_SYMBOL(ppe_drv_sc_unregister_vp_cb);

/*
 * ppe_drv_sc_register_vp_cb()
 *	Registers a vp pnode for a given service code for redirection.
 */
bool ppe_drv_sc_register_vp_cb(ppe_drv_sc_t sc, ppe_drv_sc_callback_t cb, void *app_data, uint16_t vp_num)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_sc *psc;
	struct ppe_drv_sc_vp_info *vp_info;

	ppe_drv_assert(((vp_num >= PPE_DRV_VIRTUAL_START) && (vp_num < PPE_DRV_VIRTUAL_END)), "%px: Invalid port number: %d", p, vp_num);

	if ((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END)) {
		ppe_drv_warn("%px: invalid VP number %d", p, vp_num);
		return false;
	}

	ppe_drv_assert(cb, "%p: cannot register null cb for sc %u vp %u", p, sc, vp_num);

	spin_lock_bh(&p->lock);
	psc = &p->sc[sc];
	vp_info = rcu_dereference_protected(psc->vp_info[vp_num - PPE_DRV_VIRTUAL_START], 1);
	if (vp_info) {
		ppe_drv_warn("%p: vp info is already present for service code %u vp %u\n", p, sc, vp_num);
		spin_unlock_bh(&p->lock);
		return false;
	}

	vp_info = kzalloc(sizeof(struct ppe_drv_sc_vp_info), GFP_ATOMIC);
	if (!vp_info) {
		ppe_drv_warn("%p Failed to allocate VP info for a service code %u vp %u\n", p, sc, vp_num);
		spin_unlock_bh(&p->lock);
		return false;
	}

	ppe_drv_assert(!vp_info->cb, "%p: multiple registration for sc:%u vp:%u - "
				"prev cb:%p current cb:%p", p, sc, vp_num, vp_info->cb, cb);
	vp_info->cb = cb;
	vp_info->app_data = app_data;
	rcu_assign_pointer(psc->vp_info[vp_num - PPE_DRV_VIRTUAL_START], vp_info);

	spin_unlock_bh(&p->lock);
	synchronize_rcu();

	ppe_drv_info("%p: registered cb:%p app_data:%p for sc:%u vp:%u", p, cb, app_data, sc, vp_num);
	return true;
}
EXPORT_SYMBOL(ppe_drv_sc_register_vp_cb);

/*
 * ppe_drv_sc_free()
 *	Free sc instance if it was allocated.
 */
void ppe_drv_sc_entries_free(struct ppe_drv_sc *sc)
{
	nss_ppe_drv_minidump_free(sc, "ppe_drv_sc");
	kfree(sc);
}

/*
 * ppe_drv_sc_alloc()
 *	Allocates and initializes service code tables with static configuration.
 */
struct ppe_drv_sc *ppe_drv_sc_entries_alloc(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_sc *sc;
	ppe_drv_sc_t acl_sc;

	sc = kzalloc(sizeof(struct ppe_drv_sc) * p->sc_num, GFP_KERNEL);
	if (!sc) {
		ppe_drv_warn("%p: Failed to allocate service code table entries", p);
		return NULL;
	}

	nss_ppe_drv_minidump_log(sc, sizeof(struct ppe_drv_sc) * p->sc_num, "ppe_drv_sc");

	/*
	 * Initialize service codes
	 */
	ppe_drv_sc_config(PPE_DRV_SC_BYPASS_ALL, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_ADV_QOS_BRIDGED, PPE_DRV_SC_ADV_QOS_BRIDGED, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_ADV_QOS_ROUTED, PPE_DRV_SC_ADV_QOS_ROUTED, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_QOS, PPE_DRV_SC_BYPASS_ALL, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_VLAN_FILTER_BYPASS, PPE_DRV_SC_VLAN_FILTER_BYPASS, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_L3_EXCEPT, PPE_DRV_SC_L3_EXCEPT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_SPF_BYPASS, PPE_DRV_SC_SPF_BYPASS_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_REDIR_CORE0, PPE_DRV_SC_NOEDIT_REDIR_CORE0, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_REDIR_CORE1, PPE_DRV_SC_NOEDIT_REDIR_CORE1, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_REDIR_CORE2, PPE_DRV_SC_NOEDIT_REDIR_CORE2, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_REDIR_CORE3, PPE_DRV_SC_NOEDIT_REDIR_CORE3, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_EDIT_REDIR_CORE0, PPE_DRV_SC_EDIT_REDIR_CORE0, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_EDIT_REDIR_CORE1, PPE_DRV_SC_EDIT_REDIR_CORE1, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_EDIT_REDIR_CORE2, PPE_DRV_SC_EDIT_REDIR_CORE2, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_EDIT_REDIR_CORE3, PPE_DRV_SC_EDIT_REDIR_CORE3, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_RFS_RULE, PPE_DRV_SC_NOEDIT_RFS_RULE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_EDIT_RFS_RULE, PPE_DRV_SC_EDIT_RFS_RULE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_VP_RPS, PPE_DRV_SC_VP_RPS, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_ACL_POLICER, PPE_DRV_SC_NOEDIT_ACL_POLICER, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_L2_TUNNEL_EXCEPTION, PPE_DRV_SC_L2_TUNNEL_EXCEPTION, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_PRIORITY_SET, PPE_DRV_SC_NOEDIT_PRIORITY_SET, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_RULE, PPE_DRV_SC_NOEDIT_RULE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_FMAC_BYPASS, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_FDB_BYPASS, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_GEM_LOOKUP, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_HOST_QOS, PPE_DRV_SC_HOST_QOS, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_SPF_BYPASS_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);


	/*
	 * Initialize IPsec/EIP service code
	 */
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_EIP2PPE_DECAP, PPE_DRV_SC_IPSEC_EIP2PPE_DECAP, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_EIP2PPE_ENCAP, PPE_DRV_SC_IPSEC_EIP2PPE_ENCAP, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_PPE2EIP_DECAP, PPE_DRV_SC_IPSEC_EIP2PPE_DECAP, PPE_DRV_PORT_EIP197);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP, PPE_DRV_SC_IPSEC_EIP2PPE_ENCAP, PPE_DRV_PORT_EIP197);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH, PPE_DRV_SC_NOEDIT_REDIR_CORE0, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_EDMA_VP_ENCAP, PPE_DRV_SC_IPSEC_EIP2PPE_ENCAP, PPE_DRV_PORT_EIP197);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_EDMA_VP_DECAP, PPE_DRV_SC_IPSEC_EIP2PPE_DECAP, PPE_DRV_PORT_EIP197);
	ppe_drv_sc_config(PPE_DRV_SC_IPSEC_EDMA_VP_3_TUPLE_ENCAP, PPE_DRV_SC_IPSEC_EIP2PPE_ENCAP, PPE_DRV_PORT_EIP197);

	/*
	 * Initialize FLOW ACL service code
	 */
	for (acl_sc = PPE_DRV_SC_FLOW_ACL_FIRST; acl_sc <= PPE_DRV_SC_FLOW_ACL_LAST; acl_sc++) {
		ppe_drv_sc_config(acl_sc, acl_sc, PPE_DRV_PORT_CPU);
	}

	/*
	 * Initialize loopback ring service code
	 */
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING, PPE_DRV_SC_LOOPBACK_RING_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING_NEXT, PPE_DRV_SC_BYPASS_ALL, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING_NEXT_GRETAP_N_MAPT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING_GRETAP_MAPT, PPE_DRV_SC_LOOPBACK_RING_NEXT_GRETAP_N_MAPT,
				PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING_MAPT_GRETAP, PPE_DRV_SC_LOOPBACK_RING_NEXT_GRETAP_N_MAPT,
				PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_VP_MPSK, PPE_DRV_SC_VP_MPSK, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NPT66_HAIRPIN_NAT, PPE_DRV_SC_NPT66_HAIRPIN_NAT_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_NPT66_HAIRPIN_NAT_NEXT, PPE_DRV_SC_BYPASS_ALL, PPE_DRV_PORT_CPU);

	/*
	 * Configure the service code for packet exception edit enable.
	 */
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	ppe_drv_sc_config(PPE_DRV_SC_PKT_EXCEPTION_EDIT_EN, PPE_DRV_SC_PKT_EXCEPTION_EDIT_EN, PPE_DRV_PORT_CPU);
#endif
#ifdef PPE_LOOPBACK_PORT_SUPPORT
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC_NEXT, p->loopback_port_info.port_id);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_SC_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
#endif
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT, p->loopback_port_info.port_id);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT_DDRQ, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_US_SC_NEXT_DDRQ, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC, PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC_NEXT, p->loopback_port_info.port_id);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_PORT_FEATURE_PON_HGU_DS_SC_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_FEATURE_PON_HGU_PPTP, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
#endif
#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	ppe_drv_sc_config(PPE_DRV_SC_NOEDIT_TUN_RPS, PPE_DRV_SC_NOEDIT_TUN_RPS, PPE_DRV_PORT_CPU);
#endif
#ifdef PPE_DRV_DDRQ_ENABLE
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_ETH_PT_MODE, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_PON_PT_MODE, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_spl_ddrq_config();
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_LP_SC, PPE_DRV_SC_DDRQ_LP_SC_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_LP_SC_BRIDGE, PPE_DRV_SC_DDRQ_LP_SC_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_LP_SC_IPSEC_ENCAP, PPE_DRV_SC_IPSEC_PPE2EIP_ENCAP, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_LP_SC_IPSEC_DECAP, PPE_DRV_SC_IPSEC_PPE2EIP_ACL_MATCH, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_DDRQ_LP_SC_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
#endif
#ifdef PPE_DRV_DDR_BUFFER_SPEED_MISMATCH
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER, PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_LOOPBACK_RING_DDR_BUFFER_NEXT, PPE_DRV_SC_BYPASS_ALL, PPE_DRV_PORT_CPU);
#endif
#ifdef PPE_UDP_ST_SUPPORT
	ppe_drv_sc_config(PPE_DRV_SC_UDP_ST, PPE_DRV_SC_UDP_ST_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_UDP_ST_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_UDP_ST_HGU, PPE_DRV_SC_UDP_ST_HGU_NEXT, p->loopback_port_info.port_id);
	ppe_drv_sc_config(PPE_DRV_SC_UDP_ST_HGU_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_UDP_ST_SFU, PPE_DRV_SC_UDP_ST_SFU_NEXT, PPE_DRV_PORT_CPU);
	ppe_drv_sc_config(PPE_DRV_SC_UDP_ST_SFU_NEXT, PPE_DRV_SC_NONE, PPE_DRV_PORT_CPU);
#endif
	return sc;
}
