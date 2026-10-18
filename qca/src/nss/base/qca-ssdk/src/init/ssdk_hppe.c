/*
 * Copyright (c) 2012, 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ssdk_init.h"
#include "ssdk_dts.h"
#include "adpt.h"
#include "adpt_hppe.h"
#include "fal.h"
#include "ref_vsi.h"
#include "ssdk_clk.h"
#include "hsl_phy.h"
#include <linux/in.h>
#ifdef IN_LED
#include "ssdk_led.h"
#endif
#include "ssdk_appe.h"

#if defined(IN_VSI)
sw_error_t qca_hppe_vsi_hw_init(a_uint32_t dev_id)
{
       return ppe_vsi_init(dev_id);
}
#endif

#if defined(IN_FDB)
sw_error_t qca_hppe_fdb_hw_init(a_uint32_t dev_id)
{
	a_uint32_t port = 0;
	adpt_api_t *p_api;

	SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
	SW_RTN_ON_NULL(p_api->adpt_port_bridge_txmac_set);

	for(port = SSDK_PHYSICAL_PORT0; port < SSDK_MAX_PORT_NUM; port++) {
		if (hsl_port_prop_check(dev_id, port, HSL_PP_CPU)) {
			/* CPU port disable new addr learning and station move */
			fal_fdb_port_learning_ctrl_set(dev_id, port, A_FALSE, FAL_MAC_FRWRD);
			fal_fdb_port_stamove_ctrl_set(dev_id, port, A_FALSE, FAL_MAC_FRWRD);
		} else {
			fal_fdb_port_learning_ctrl_set(dev_id, port, A_TRUE, FAL_MAC_FRWRD);
			fal_fdb_port_stamove_ctrl_set(dev_id, port, A_TRUE, FAL_MAC_FRWRD);
		}

		/* Isolation member includes itself and other ports */
		fal_portvlan_member_update(dev_id, port, GENMASK(SSDK_MAX_PORT_NUM - 1, 0));

		if (hsl_port_prop_check(dev_id, port, HSL_PP_PHY))
			/* For MAC ports, bridge tx mac is enabled latter when port link up */
			p_api->adpt_port_bridge_txmac_set(dev_id, port, A_FALSE);
		else
			p_api->adpt_port_bridge_txmac_set(dev_id, port, A_TRUE);

		fal_port_promisc_mode_set(dev_id, port, A_TRUE);
	}

	fal_fdb_aging_ctrl_set(dev_id, FAL_FDB_AGE_ENABLE);
	fal_fdb_learning_ctrl_set(dev_id, HW_CTRL_LEARNING);

	return SW_OK;
}
#endif

#if defined(IN_CTRLPKT)
#define RFDB_PROFILE_ID_STP 31
#define RFDB_PROFILE_ID_ARP 30
#define APPE_CPU_CODE_CTRL_NUM	256
sw_error_t qca_hppe_ctlpkt_hw_init(a_uint32_t dev_id)
{
	fal_mac_addr_t mcast_mac_addr;
	fal_ctrlpkt_action_t ctrlpkt_action;
	fal_ctrlpkt_profile_t ctrlpkt_profile;
	sw_error_t rv = SW_OK;
	a_uint8_t mac_id = 0, mac_num = 0;
	a_uint8_t *mac = NULL;
	a_uint32_t rfdb_profile_bmp = 0, rfdb_profile_index = 0;
	a_uint32_t cpu_code = 0;

	while (cpu_code < APPE_CPU_CODE_CTRL_NUM) {
		rv = fal_mgmtctrl_tunnel_decap_set(dev_id, cpu_code, A_TRUE);
		SW_RTN_ON_ERROR(rv);
		cpu_code++;
	}

	memset(&ctrlpkt_action, 0, sizeof(ctrlpkt_action));
	memset(&ctrlpkt_profile, 0, sizeof(ctrlpkt_profile));
	memset(&mcast_mac_addr, 0, sizeof(mcast_mac_addr));

	mcast_mac_addr.uc[0] = 0x01;
	mcast_mac_addr.uc[1] = 0x80;
	mcast_mac_addr.uc[2] = 0xc2;
	rv = fal_mgmtctrl_rfdb_profile_set(dev_id, RFDB_PROFILE_ID_STP,
			&mcast_mac_addr);
	SW_RTN_ON_ERROR(rv);

	ctrlpkt_action.action = FAL_MAC_RDT_TO_CPU;
	ctrlpkt_action.in_stp_bypass = A_TRUE;

	ctrlpkt_profile.action = ctrlpkt_action;
	ctrlpkt_profile.port_map = qca_ssdk_port_bmp_get(dev_id);
	ctrlpkt_profile.rfdb_profile_bitmap = (1 << RFDB_PROFILE_ID_STP);
	rv = fal_mgmtctrl_ctrlpkt_profile_add(dev_id, &ctrlpkt_profile);
	SW_RTN_ON_ERROR(rv);

	/*
	 * Redirect ARP response packet to CPU port with a valid CPU code.
	 * queue management assigns the highest priority queue based on this
	 * CPU code.
	 */
	mac_id = 0;
	mac_num = ssdk_intf_mac_num_get();
	if (mac_num == 0)
		return SW_OK;

	while (mac_id < mac_num) {
		/*
		 * The MAC of all ports(ethx) should be involved, since the MAC
		 * of bridge can be any one of them.
		 */
		mac = ssdk_intf_macaddr_get(mac_id);
		memcpy(mcast_mac_addr.uc, mac, 6);

		rfdb_profile_index = RFDB_PROFILE_ID_ARP - mac_id;
		rv = fal_mgmtctrl_rfdb_profile_set(dev_id, rfdb_profile_index, &mcast_mac_addr);
		SW_RTN_ON_ERROR(rv);

		rfdb_profile_bmp |= BIT(rfdb_profile_index);
		mac_id++;
	}

	memset(&ctrlpkt_action, 0, sizeof(ctrlpkt_action));
	memset(&ctrlpkt_profile, 0, sizeof(ctrlpkt_profile));

	ctrlpkt_action.action = FAL_MAC_RDT_TO_CPU;
	ctrlpkt_profile.action = ctrlpkt_action;
	ctrlpkt_profile.port_map = qca_ssdk_port_bmp_get(dev_id);
	ctrlpkt_profile.rfdb_profile_bitmap = rfdb_profile_bmp;
	ctrlpkt_profile.protocol_types.mgt_arp_rep = A_TRUE;

	rv = fal_mgmtctrl_ctrlpkt_profile_add(dev_id, &ctrlpkt_profile);

	return rv;
}
#endif

#if defined(IN_PORTVLAN)
sw_error_t
qca_hppe_portvlan_hw_init(a_uint32_t dev_id)
{
	a_uint32_t port_id = 0, vsi_idx = 0;
	fal_global_qinq_mode_t global_qinq_mode;
	fal_port_qinq_role_t port_qinq_role;
	fal_tpid_t in_eg_tpid;
	fal_vlantag_egress_mode_t vlantag_eg_mode;

	aos_mem_zero(&global_qinq_mode, sizeof(global_qinq_mode));
	aos_mem_zero(&port_qinq_role, sizeof(port_qinq_role));
	aos_mem_zero(&in_eg_tpid, sizeof(in_eg_tpid));
	aos_mem_zero(&vlantag_eg_mode, sizeof(vlantag_eg_mode));

	/* configure ingress/egress global QinQ mode as ctag/ctag */
	global_qinq_mode.mask = 0x3;
	global_qinq_mode.ingress_mode = FAL_QINQ_CTAG_MODE;
	global_qinq_mode.egress_mode = FAL_QINQ_CTAG_MODE;
	fal_global_qinq_mode_set(dev_id, &global_qinq_mode);

	/* configure port0, port7 ingress/egress QinQ role as core/core */
	port_qinq_role.mask = 0x3;
	port_qinq_role.ingress_port_role = FAL_QINQ_CORE_PORT;
	port_qinq_role.egress_port_role = FAL_QINQ_CORE_PORT;
	fal_port_qinq_mode_set(dev_id, SSDK_PHYSICAL_PORT0, &port_qinq_role);
	fal_port_qinq_mode_set(dev_id, SSDK_PHYSICAL_PORT7, &port_qinq_role);
	/* configure port1 - port6 ingress/egress QinQ role as edge/edge */
	port_qinq_role.mask = 0x3;
	port_qinq_role.ingress_port_role = FAL_QINQ_EDGE_PORT;
	port_qinq_role.egress_port_role = FAL_QINQ_EDGE_PORT;
	for (port_id = SSDK_PHYSICAL_PORT1; port_id <= SSDK_PHYSICAL_PORT6;
			port_id++) {
		fal_port_qinq_mode_set(dev_id, port_id, &port_qinq_role);
	}

	/* configure ingress and egress stpid/ctpid as 0x88a8/0x8100 */
	in_eg_tpid.mask = 0x3;
	in_eg_tpid.ctpid = FAL_DEF_VLAN_CTPID;
	in_eg_tpid.stpid = FAL_DEF_VLAN_STPID;
	fal_ingress_tpid_set(dev_id, &in_eg_tpid);
	fal_egress_tpid_set(dev_id, &in_eg_tpid);

	/* configure the port0 - port7 of vsi0 - vsi31 to unmodified */
	for (vsi_idx = 0; vsi_idx <= FAL_VSI_MAX; vsi_idx++) {
		for (port_id = SSDK_PHYSICAL_PORT0;
			port_id <= SSDK_PHYSICAL_PORT7;	port_id++) {
			fal_port_vsi_egmode_set(dev_id, vsi_idx,
						port_id, FAL_EG_UNMODIFIED);
		}
	}

	vlantag_eg_mode.mask = 0x3;
	vlantag_eg_mode.stag_mode = FAL_EG_UNTOUCHED;
	vlantag_eg_mode.ctag_mode = FAL_EG_UNTOUCHED;
	/*stag/ctag egress mode as untouched/untouched*/
	fal_port_vlantag_egmode_set(dev_id, SSDK_PHYSICAL_PORT0,
				&vlantag_eg_mode);
	fal_port_vlantag_egmode_set(dev_id, SSDK_PHYSICAL_PORT7,
				&vlantag_eg_mode);
	/*vsi tag mode control to disable*/
	fal_port_vlantag_vsi_egmode_enable(dev_id, SSDK_PHYSICAL_PORT0,
				A_FALSE);
	fal_port_vlantag_vsi_egmode_enable(dev_id, SSDK_PHYSICAL_PORT7,
				A_FALSE);
	/*ingress vlan translation mismatched command as forward*/
	fal_port_vlan_xlt_miss_cmd_set(dev_id, SSDK_PHYSICAL_PORT0,
				FAL_MAC_FRWRD);

	vlantag_eg_mode.stag_mode = FAL_EG_UNMODIFIED;
	vlantag_eg_mode.ctag_mode = FAL_EG_UNMODIFIED;
	for (port_id = SSDK_PHYSICAL_PORT1; port_id <= SSDK_PHYSICAL_PORT6;
			port_id++) {
		/*ingress vlan translation mismatched command as forward*/
		fal_port_vlan_xlt_miss_cmd_set(dev_id, port_id, FAL_MAC_FRWRD);
		/*vsi tag mode control to enable*/
		fal_port_vlantag_vsi_egmode_enable(dev_id, port_id, A_TRUE);
		/*stag/ctag egress mode as unmodified/unmodified*/
		fal_port_vlantag_egmode_set(dev_id, port_id, &vlantag_eg_mode);
	}

	return SW_OK;
}
#endif

#if defined(IN_BM)
sw_error_t
qca_hppe_bm_hw_init(a_uint32_t dev_id)
{
	a_uint32_t i = 0;
	fal_bm_dynamic_cfg_t cfg;
	a_uint16_t group_buf = 0, share_ceiling = 0, phyport_share_ceiling = 0;
	adpt_ppe_type_t chip_type = adpt_ppe_type_get(dev_id);

	for (i = 0; i <  PPE_BM_PORT_NUM; i++) {
		/*disable fc for phy ports and enable fc for port 0 and port 7*/
		if(i >= PPE_BM_PHY_PORT_OFFSET && i <= PPE_BM_PHY_PORT_MAX)
			fal_port_bm_ctrl_set(dev_id, i, A_FALSE);
		else
			fal_port_bm_ctrl_set(dev_id, i, A_TRUE);
		/* map to group 0 */
		fal_port_bufgroup_map_set(dev_id, i, 0);
	}

	switch (chip_type) {
		case HPPE_TYPE:
			group_buf = 1400;
			break;
		case CPPE_TYPE:
			group_buf = 1024;
			break;
		case APPE_TYPE:
		case MRPPE_TYPE:
			group_buf = 1550;
			break;
		case JHPPE_TYPE:
			group_buf = 3550;
			break;
		case HMSPPE_TYPE:
			group_buf = 2550;
			break;
		case MPPE_TYPE:
			group_buf = 360;
			break;
		default:
			SSDK_ERROR("Unsupported chip type: %d\n", chip_type);
			return SW_OUT_OF_RANGE;
	}

	fal_bm_bufgroup_buffer_set(dev_id, 0, group_buf);

	/* set reserved buffer */
	for (i = 0; i < PPE_BM_PORT_NUM; i++) {
		a_uint16_t prealloc_buf = 0, react_buf = 0;
		switch (chip_type) {
			case HPPE_TYPE:
			case APPE_TYPE:
				if (i < PPE_BM_PHY_PORT_OFFSET) {
					prealloc_buf = 0;
					react_buf = 100;
				} else if (i == PPE_BM_PORT_NUM-1) {
					prealloc_buf = 0;
					react_buf = 40;
				} else {
					prealloc_buf = 0;
					react_buf = 128;
				}
				break;
			case MRPPE_TYPE:
			case JHPPE_TYPE:
			case HMSPPE_TYPE:
				if (i < PPE_BM_PHY_PORT_OFFSET) {
					prealloc_buf = 0;
					react_buf = 100;
				} else {
					prealloc_buf = 0;
					react_buf = 128;
				}
				break;
			case CPPE_TYPE:
				if (i < PPE_BM_PHY_PORT_OFFSET) {
					prealloc_buf = 0;
					react_buf = 100;
				} else if (i >= PPE_BM_PORT_NUM-2) {
					/* port 6 is loopback port on CPPE */
					prealloc_buf = 0;
					react_buf = 40;
				} else {
					prealloc_buf = 0;
					react_buf = 128;
				}
				break;
			case MPPE_TYPE:
				if (i < PPE_BM_PHY_PORT_OFFSET) {
					prealloc_buf = 12;
					react_buf = 40;
				} else {
					prealloc_buf = 12;
					react_buf = 128;
				}
				break;
			default:
				SSDK_ERROR("Unsupported chip type: %d\n", chip_type);
				return SW_OUT_OF_RANGE;
		}

		fal_bm_port_reserved_buffer_set(dev_id, i, prealloc_buf, react_buf);
	}

	/* set dynamic threshold */
	memset(&cfg, 0, sizeof(cfg));
	for (i = 0; i < PPE_BM_PORT_NUM; i++) {
		switch (chip_type) {
			case HPPE_TYPE:
				share_ceiling = 250;
				phyport_share_ceiling = 250;
				cfg.resume_min_thresh = 0;
				cfg.resume_off = 36;
				cfg.weight= 4;
				break;
			case CPPE_TYPE:
				share_ceiling = 216;
				phyport_share_ceiling = 216;
				cfg.resume_min_thresh = 0;
				cfg.resume_off = 36;
				cfg.weight= 4;
				break;
			case JHPPE_TYPE:
			case HMSPPE_TYPE:
				share_ceiling = 250;
				phyport_share_ceiling = 1300;
				cfg.resume_min_thresh = 0;
				cfg.resume_off = 36;
				cfg.weight= 7;
				break;
			case APPE_TYPE:
			case MRPPE_TYPE:
				if (i == PPE_BM_PORT_MIN) {
					share_ceiling = 1146;
					cfg.resume_min_thresh = 0;
					cfg.resume_off = 8;
					cfg.weight= 7;
				} else {
					share_ceiling = 250;
					phyport_share_ceiling = 250;
					cfg.resume_min_thresh = 0;
					cfg.resume_off = 36;
					cfg.weight= 4;
				}
				break;
			case MPPE_TYPE:
				share_ceiling = 30;
				phyport_share_ceiling = 48;
				cfg.resume_min_thresh = 20;
				cfg.resume_off = 5;
				cfg.weight= 7;
				break;
			default:
				SSDK_ERROR("Unsupported chip type: %d\n", chip_type);
				return SW_OUT_OF_RANGE;
		}

		if(i >= PPE_BM_PHY_PORT_OFFSET && i <= PPE_BM_PHY_PORT_MAX)
			cfg.shared_ceiling = phyport_share_ceiling;
		else
			cfg.shared_ceiling = share_ceiling;

		fal_bm_port_dynamic_thresh_set(dev_id, i, &cfg);
	}

	return SW_OK;
}
#endif

#if defined(IN_QM)
#define SSDK_PRI_MAX		16
#define SSDK_CPU_PRI_NUM	1
#define SSDK_MGMT_ARP_REP_CPU_CODE	101

sw_error_t
qca_hppe_qm_hw_init(a_uint32_t dev_id)
{
	a_uint32_t i, total_buf;
	fal_ucast_queue_dest_t queue_dst;
	fal_ac_obj_t obj;
	fal_ac_ctrl_t ac_ctrl;
	fal_ac_group_buffer_t group_buff;
	a_uint32_t qbase = 0;
	a_uint32_t chip_ver = 0;
	a_uint32_t max_pri_supported, pri, class;
	adpt_ppe_type_t chip_type = adpt_ppe_type_get(dev_id);

	memset(&queue_dst, 0, sizeof(queue_dst));

	chip_ver = adpt_chip_revision_get(dev_id);

	/*
	 * Redirect service code 2 to queue 1
	 * TODO: keep sync with  NSS
	 */
	queue_dst.service_code_en = A_TRUE;
	queue_dst.service_code = 2;
	qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT0);
	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase + 8, 0);

	/* Service code configuration for loopback port. */
	queue_dst.service_code = 3;
	switch (chip_type) {
	case JHPPE_TYPE:
		qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT8);
		break;
	case HMSPPE_TYPE:
		qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT7);
		break;
	default:
		qbase = 128;
		break;
	}
	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, 8);

	queue_dst.service_code = 4;
	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, 8);

	queue_dst.service_code = 5;
	qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT0);
	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, 0);

	queue_dst.service_code = 6;
	qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT0);
	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase + 8, 0);

	/* Service code configuration for EIP port. */
	queue_dst.service_code = 7;
	switch (chip_type) {
	case APPE_TYPE:
	case JHPPE_TYPE:
		qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT7);
		break;
	default:
		qbase = 252;
		break;
	}
	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, 0);

	queue_dst.service_code_en = A_FALSE;
	queue_dst.service_code = 0;

	for(i = 0; i < SSDK_MAX_PORT_NUM; i++) {
		queue_dst.dst_port = i;
		qbase = ssdk_ucast_queue_start_get(dev_id, i);
		fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, i);

		/* Initializes the ucast priority map, cpu queue number depends on the
		 * queue number mapped with the EDMA RX ring by EDMA_QID2RID_TABLE,
		 * only queue 0 is enabled for host EDMA rings by default.
		 * physical port has different queue number between hppe and appe.
		 * 16 queues per port for hppe, class 0~15 matches with priority 0~15.
		 * 8 queues per port for appe, class 0~6 matches with priority 0~6, and
		 * class 7 matches with priority >=7.
		 */
		max_pri_supported = ssdk_ucast_l0_cdrr_num_get(dev_id, i);

		for (pri = 0; pri < SSDK_PRI_MAX; pri++) {
			if (pri >= max_pri_supported)
				class = max_pri_supported - 1;
			else
				class = pri;

			if (i == 0) {
				/*
				 * For CPU port, ARM core and direct switch take the common
				 * profile ID 0, point offload(NSS) takes the special profile
				 * ID 15.
				 */
				fal_ucast_priority_class_set(dev_id, FAL_QM_PROFILE_COMMON_ID,
						pri, class);

				fal_ucast_priority_class_set(dev_id, FAL_QM_PROFILE_PO_ID,
						pri, class);

			} else {
				fal_ucast_priority_class_set(dev_id, i, pri, class);
			}

		}

		if (i == 0) {
			a_uint32_t hash = 0;
			/*
			 * For CPU port, we need to initialize the hash map offset to 0 for the
			 * PO and cpu code profile.
			 *
			 * For EIP port, initialize the hash of service code 7 profile ID.
			 */
			for (hash = 0; hash < FAL_QM_PROFILE_PO_RSS_HASH_MAX; hash++) {
				fal_ucast_hash_map_set(dev_id, FAL_QM_PROFILE_PO_ID,
						hash, FAL_QM_PROFILE_PO_RSS_HASH_CLASS);
				fal_ucast_hash_map_set(dev_id, FAL_QM_PROFILE_CPU_CODE_ID,
						hash, FAL_QM_PROFILE_PO_RSS_HASH_CLASS);
				fal_ucast_hash_map_set(dev_id, FAL_QM_PROFILE_EIP_SERVCODE_ID,
						hash, FAL_QM_PROFILE_PO_RSS_HASH_CLASS);
			}
		}
	}

#if defined(HMSPPE)
	if (chip_type == HMSPPE_TYPE) {
		ssdk_dt_scheduler_cfg *dt_cfg;
		a_uint16_t tcont_id = 0, pon_port_qbase = 0, q_num = 0, index = 0;
		fal_queue_tcont_cfg_t tcont_cfg = {0};
		int vport = (SSDK_MAX_VIRTUAL_PORT_ID + 1) / 2;
		a_uint8_t max_pri = ssdk_port_ucast_max_pri_get(dev_id,
								SSDK_PHYSICAL_PORT6);
		a_uint32_t mac2_mode = ssdk_dt_global_get_mac_mode(dev_id, SSDK_UNIPHY_INSTANCE2);

		/* Skip the PON port(6) queue/tcont initialization if it has no
		 * queue resource assigned in the DTS (e.g. PON port unused on
		 * this board), otherwise max_pri is 0 and the while loop below
		 * never advances index, causing a permanent dead loop.
		 */
		if (max_pri != 0 && mac2_mode == PORT_WRAPPER_MAX) {
			/* Assign the queue base of port 6 as the last reserved queue,
			 * and disable the enqueue for the port 6 queue base to make
			 * the packet go to this queue dropped.
			 */
			dt_cfg = ssdk_bootup_shceduler_cfg_get(dev_id);
			if (!dt_cfg)
				return SW_NOT_SUPPORTED;

			pon_port_qbase = dt_cfg->reserved_pool.ucastq_end;
			queue_dst.dst_port = SSDK_PHYSICAL_PORT6;
			fal_ucast_queue_base_profile_set(dev_id, &queue_dst,
							 pon_port_qbase,
							 SSDK_PHYSICAL_PORT6);
			fal_qm_enqueue_ctrl_set(dev_id, pon_port_qbase, A_FALSE);

#if defined(IN_PON)
			fal_qm_mcast_enqueue_ctrl_set(dev_id, PON_PORT_ID, A_TRUE);
			qca_hmsppe_omci_enq_vp_init(dev_id, vport);
#endif

			/* Assign the virtual ports starting from 128 with the queues of PON port 6 */
			qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PHYSICAL_PORT6);
			q_num = ssdk_ucast_queue_num_get(dev_id, SSDK_PHYSICAL_PORT6);
			while (index < q_num) {
				queue_dst.dst_port = vport;
				fal_ucast_queue_base_profile_set(dev_id, &queue_dst,
								 qbase, SSDK_PHYSICAL_PORT6);

				tcont_cfg.valid = A_TRUE;
				tcont_cfg.tcont_id = tcont_id;
				for (i = 0; i < max_pri; i++)
					fal_qm_tcont_set(dev_id, qbase + i, &tcont_cfg);

				qbase += max_pri;
				index += max_pri;
				tcont_id++;
				vport++;
			}
		}
	}
#endif

	/* Initialize the queue base for all CPU code. */
	queue_dst.dst_port = 0;
	queue_dst.cpu_code_en = A_TRUE;
	qbase = ssdk_ucast_queue_start_get(dev_id, SSDK_PORT_CPU);
	for (i = 0; i < SSDK_MAX_CPU_CODE_NUM; i++) {
		queue_dst.cpu_code = i;
		fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, 0);
	}

	/*
	 * Configure the RDTCPU ARP reply packet with the max priority.
	 */
	queue_dst.cpu_code = SSDK_MGMT_ARP_REP_CPU_CODE;
	max_pri_supported = ssdk_port_ucast_max_pri_get(dev_id, SSDK_PORT_CPU);
	if (max_pri_supported > SSDK_PRI_MAX) {
		max_pri_supported = SSDK_CPU_PRI_NUM;
	}

	fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase + max_pri_supported - 1, 0);

	queue_dst.cpu_code_en = A_FALSE;
	queue_dst.cpu_code = 0;

	/*
	 * Enable PPE source profile 1 and map it to PPE queue 4
	 */
	memset(&queue_dst, 0, sizeof(queue_dst));
	queue_dst.src_profile = 1;

	/*
	 * Enable service code mapping for profile 1
	 */
	queue_dst.service_code_en = A_TRUE;
	for (i = 0; i < SSDK_MAX_SERVICE_CODE_NUM; i++) {
		queue_dst.service_code = i;

		if (i == 2 || i == 6) {
			fal_ucast_queue_base_profile_set(dev_id, &queue_dst, 8, 0);
		} else if (i == 3 || i == 4) {
			fal_ucast_queue_base_profile_set(dev_id, &queue_dst, 128, 8);
		} else {
			fal_ucast_queue_base_profile_set(dev_id, &queue_dst, 4, 0);
		}
	}
	queue_dst.service_code_en = A_FALSE;
	queue_dst.service_code = 0;

	/*
	 * Enable cpu code mapping for profile 1
	 */
	queue_dst.cpu_code_en = A_TRUE;
	for (i = 0; i < SSDK_MAX_CPU_CODE_NUM; i++) {
		queue_dst.cpu_code = i;
		fal_ucast_queue_base_profile_set(dev_id, &queue_dst, 4, 0);
	}
	queue_dst.cpu_code_en = A_FALSE;
	queue_dst.cpu_code = 0;

	/*
	 * Enable destination port mappings for profile 1
	 */
	for (i = 0; i < SSDK_MAX_PORT_NUM; i++) {
		queue_dst.dst_port = i;
		qbase = ssdk_ucast_queue_start_get(dev_id, i);
		fal_ucast_queue_base_profile_set(dev_id, &queue_dst, qbase, i);
	}

	for (i = SSDK_MAX_PORT_NUM; i <= SSDK_MAX_VIRTUAL_PORT_ID; i++) {
		queue_dst.dst_port = i;
		fal_ucast_queue_base_profile_set(dev_id, &queue_dst, 4, 0);
	}
	queue_dst.dst_port = 0;

	/* queue ac*/
	ac_ctrl.ac_en = A_TRUE;
	ac_ctrl.ac_fc_en = A_FALSE;
	for (i = 0; i < SSDK_L0SCHEDULER_CFG_MAX; i++) {
		obj.type = FAL_AC_QUEUE;
		obj.obj_id = i;
		fal_ac_ctrl_set(dev_id, &obj, &ac_ctrl);
		fal_ac_queue_group_set(dev_id, i, 0);
		fal_ac_prealloc_buffer_set(dev_id, &obj, 0);
	}

	switch (chip_type) {
		case JHPPE_TYPE:
		case HTTPPE_TYPE:
			total_buf = 4000;
			break;
		case HMSPPE_TYPE:
			total_buf = 2800;
			break;
		case MRPPE_TYPE:
		case APPE_TYPE:
			total_buf = 2000;
			break;
		case MPPE_TYPE:
			total_buf = 500;
			break;
		default:
			SSDK_ERROR("Unsupported chip type: %d\n", chip_type);
			return SW_OUT_OF_RANGE;
	}

	group_buff.prealloc_buffer = 0;
	group_buff.total_buffer = total_buf;
	fal_ac_group_buffer_set(dev_id, 0, &group_buff);

	for (i = 0; i < SSDK_L0SCHEDULER_CFG_MAX; i++)
		fal_qm_threshold_reset(dev_id, i);

	/* enable the queue counter */
	fal_queue_counter_ctrl_set(dev_id, A_TRUE);

	return SW_OK;
}
#endif

#if defined(IN_QOS)

void qca_qos_scheduler_port_id_convert(a_uint32_t dev_id, a_uint32_t *port_id)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	if (priv && *port_id >= priv->ports_num)
		*port_id = SSDK_PHYSICAL_PORT1;
}

sw_error_t
qca_hppe_qos_scheduler_hw_init(a_uint32_t dev_id)
{
	a_uint32_t i = 0, port_id = 0;
	fal_qos_scheduler_cfg_t cfg;
	fal_queue_bmp_t queue_bmp;
	fal_qos_group_t group_sel;
	fal_qos_pri_precedence_t pri_pre;
	ssdk_dt_scheduler_cfg *dt_cfg = ssdk_bootup_shceduler_cfg_get(dev_id);

	memset(&cfg, 0, sizeof(cfg));

	/* L1 shceduler */
	for (i = 0; i < SSDK_L1SCHEDULER_CFG_MAX; i++) {
		if (dt_cfg->l1cfg[i].valid) {
			port_id = dt_cfg->l1cfg[i].port_id;
#if defined(IN_ATHTAG) && !defined(JHPPE)
			qca_qos_scheduler_port_id_convert(dev_id, &port_id);
#endif
			cfg.sp_id = dt_cfg->l1cfg[i].sp_id;
			cfg.c_pri = dt_cfg->l1cfg[i].cpri;
			cfg.e_pri = dt_cfg->l1cfg[i].epri;
			cfg.c_drr_id = dt_cfg->l1cfg[i].cdrr_id;
			cfg.e_drr_id = dt_cfg->l1cfg[i].edrr_id;
			cfg.c_drr_wt = 1;
			cfg.e_drr_wt = 1;
			fal_queue_scheduler_set(dev_id, i, 1,
					port_id, &cfg);
		}
	}

	/* L0 shceduler */
	for (i = 0; i < SSDK_L0SCHEDULER_CFG_MAX; i++) {
		if (dt_cfg->l0cfg[i].valid) {
			port_id = dt_cfg->l0cfg[i].port_id;
#if defined(IN_ATHTAG) && !defined(JHPPE)
			qca_qos_scheduler_port_id_convert(dev_id, &port_id);
#endif
			cfg.sp_id = dt_cfg->l0cfg[i].sp_id;
			cfg.c_pri = dt_cfg->l0cfg[i].cpri;
			cfg.e_pri = dt_cfg->l0cfg[i].epri;
			cfg.c_drr_id = dt_cfg->l0cfg[i].cdrr_id;
			cfg.e_drr_id = dt_cfg->l0cfg[i].edrr_id;
			cfg.c_drr_wt = 1;
			cfg.e_drr_wt = 1;
			fal_queue_scheduler_set(dev_id, i,
					0, port_id, &cfg);
		}
	}

	/* queue--edma ring mapping*/
	memset(&queue_bmp, 0, sizeof(queue_bmp));
	queue_bmp.bmp[0] = 0xF;
	fal_edma_ring_queue_map_set(dev_id, 0, &queue_bmp);
	queue_bmp.bmp[0] = 0xF0;
	fal_edma_ring_queue_map_set(dev_id, 3, &queue_bmp);
	queue_bmp.bmp[0] = 0xF00;
	fal_edma_ring_queue_map_set(dev_id, 1, &queue_bmp);
	queue_bmp.bmp[0] = 0;
	queue_bmp.bmp[4] = 0xFFFF;
	fal_edma_ring_queue_map_set(dev_id, 2, &queue_bmp);

	/* chose qos group 0 */
	group_sel.dscp_group = 0;
	group_sel.flow_group = 0;
	group_sel.pcp_group = 0;
	for (i = SSDK_PHYSICAL_PORT0; i <= SSDK_PHYSICAL_PORT7; i++) {
		fal_qos_port_group_set(dev_id, i, &group_sel);
	}
	/* qos precedence */
	pri_pre.flow_pri = 4;
	pri_pre.acl_pri = 2;
	pri_pre.dscp_pri = 1;
	pri_pre.pcp_pri = 0;
	pri_pre.preheader_pri = 3;
	for (i = SSDK_PHYSICAL_PORT0; i <= SSDK_PHYSICAL_PORT7; i++) {
		fal_qos_port_pri_precedence_set(dev_id, i, &pri_pre);
	}

	return SW_OK;
}
#endif


#if defined(IN_ACL)
#define LIST_ID_BYP_FDB_LRN 63/*reserved for bypass fdb learning*/
#define LIST_PRI_BYP_FDB_LRN 32

sw_error_t qca_hppe_acl_byp_intf_mac_learn(a_uint32_t dev_id)
{
	a_uint32_t index = 0, num;
	fal_acl_rule_t *rule = NULL;
	a_uint8_t* mac;
	a_uint32_t port_bmp = qca_ssdk_port_bmp_get(dev_id);

	num = ssdk_intf_mac_num_get();
	if(num == 0){
		return SW_OK;/*No found interface MAC*/
	}

	/*Bypass fdb learn*/
	rule = (fal_acl_rule_t *)kzalloc(sizeof(fal_acl_rule_t), GFP_ATOMIC);
	if (!rule)
		return SW_FAIL;
	rule->rule_type = FAL_ACL_RULE_MAC;
	rule->bypass_bitmap[0] |= (1<<FAL_ACL_BYPASS_FDB_LEARNING);
	rule->bypass_bitmap[0] |= (1<<FAL_ACL_BYPASS_FDB_REFRESH);

	FAL_FIELD_FLG_SET(rule->field_flg, FAL_ACL_FIELD_MAC_SA);

	fal_acl_list_creat(dev_id, LIST_ID_BYP_FDB_LRN, LIST_PRI_BYP_FDB_LRN);

	for (index = 0; index < SSDK_MAX_NR_ETH; index++) {
		if(index >= num)
			break;
		mac = ssdk_intf_macaddr_get(index);
		memcpy(rule->src_mac_val.uc, mac, 6);
		memset(rule->src_mac_mask.uc, 0xff, 6);
		SSDK_DEBUG("%02x:%02x:%02x:%02x:%02x:%02x\n", rule->src_mac_val.uc[0],
			rule->src_mac_val.uc[1], rule->src_mac_val.uc[2], rule->src_mac_val.uc[3],
			rule->src_mac_val.uc[4], rule->src_mac_val.uc[5]);
		fal_acl_rule_add(dev_id, LIST_ID_BYP_FDB_LRN, index, 1, rule);
	}

	aos_mem_free(rule);
	rule = NULL;
	fal_acl_list_bind(dev_id, LIST_ID_BYP_FDB_LRN, FAL_ACL_DIREC_IN,
				FAL_ACL_BIND_PORTBITMAP, port_bmp);

	return SW_OK;
}

#if defined(IN_PTP)
sw_error_t qca_hppe_acl_remark_ptp_servcode(a_uint32_t dev_id) {
#define LIST_ID_L2_TAG_SERVICE_CODE_PTP 58
#define LIST_ID_L4_TAG_SERVICE_CODE_PTP 59
#define LIST_PRI_TAG_SERVICE_CODE_PTP   1
#define PTP_EVENT_PKT_SERVICE_CODE      0x9
#define PTP_EV_PORT                     319
#define PTP_MSG_SYNC                    0
#define PTP_MSG_PRESP                   3

	sw_error_t ret;
	fal_servcode_config_t servcode_conf = {0};
	fal_acl_rule_t *entry = NULL;
	a_uint32_t index = 0, msg_type = 0;
	a_uint32_t ptp_port_bmp = 0;

	/* only marking ptp packet with service code for the qca808x phy */
	ptp_port_bmp = qca_ssdk_port_bmp_get(dev_id);

	/* Not found the PHY with ptp feature */
	if (ptp_port_bmp == 0) {
		return SW_OK;
	}

	/* Create PTP ACL L2 list */
	ret = fal_acl_list_creat(dev_id, LIST_ID_L2_TAG_SERVICE_CODE_PTP,
			LIST_PRI_TAG_SERVICE_CODE_PTP);
	SW_RTN_ON_ERROR(ret);

	/* Set up UDF2 profile */
	ret = fal_acl_udf_profile_set(dev_id, FAL_ACL_UDF_NON_IP, 2, FAL_ACL_UDF_TYPE_L3, 0);
	SW_RTN_ON_ERROR(ret);

	/* Tag service code for PTP packet */
	entry = (fal_acl_rule_t *)kzalloc(sizeof(fal_acl_rule_t), GFP_ATOMIC);
	if (!entry)
		return SW_FAIL;
	entry->service_code = PTP_EVENT_PKT_SERVICE_CODE;
	entry->pri = LIST_PRI_TAG_SERVICE_CODE_PTP;
	FAL_ACTION_FLG_SET(entry->action_flg, FAL_ACL_ACTION_SERVICE_CODE);
	FAL_ACTION_FLG_SET(entry->action_flg, FAL_ACL_ACTION_PERMIT);

	/* L2 PTP packet */
	entry->rule_type = FAL_ACL_RULE_MAC;

	/* L2 PTP ethernet type 0x88f7 */
	entry->ethtype_val = ETH_P_1588;
	entry->ethtype_mask = 0xffff;
	FAL_FIELD_FLG_SET(entry->field_flg, FAL_ACL_FIELD_MAC_ETHTYPE);

	for (msg_type = PTP_MSG_SYNC; msg_type <= PTP_MSG_PRESP; msg_type++) {
		/* L2 UDF2 for msg type */
		entry->udf2_val = (msg_type << 0x8);
		entry->udf2_mask = 0x0f00;
		FAL_FIELD_FLG_SET(entry->field_flg, FAL_ACL_FIELD_UDF2);

		/* Add PTP L2 rule to ACL list */
		ret = fal_acl_rule_add(dev_id, LIST_ID_L2_TAG_SERVICE_CODE_PTP,
				index++, 1, entry);
		SW_RTN_ON_ERROR(ret);
	}

	/* Unset L2 PTP ethernet type 0x88f7 */
	index = 0;
	FAL_FIELD_FLG_CLR(entry->field_flg, FAL_ACL_FIELD_UDF2);
	FAL_FIELD_FLG_CLR(entry->field_flg, FAL_ACL_FIELD_MAC_ETHTYPE);

	/* Create PTP ACL L4 list */
	ret = fal_acl_list_creat(dev_id, LIST_ID_L4_TAG_SERVICE_CODE_PTP,
			LIST_PRI_TAG_SERVICE_CODE_PTP);
	SW_RTN_ON_ERROR(ret);

	/* IPv4 PTP packet */
	entry->rule_type = FAL_ACL_RULE_IP4;
	entry->is_ip_mask = 1;
	entry->is_ip_val = A_TRUE;
	FAL_FIELD_FLG_SET(entry->field_flg, FAL_ACL_FIELD_IP);
	entry->is_ipv6_mask = 1;
	entry->is_ipv6_val = A_FALSE;
	FAL_FIELD_FLG_SET(entry->field_flg, FAL_ACL_FIELD_IPV6);

	/* PTP over UDP protocol */
	entry->ip_proto_val = IPPROTO_UDP;
	entry->ip_proto_mask = 0xff;
	FAL_FIELD_FLG_SET(entry->field_flg, FAL_ACL_FIELD_IP_PROTO);

	/* PTP UDP dest port 319 */
	entry->dest_l4port_op = FAL_ACL_FIELD_MASK;
	entry->dest_l4port_val = PTP_EV_PORT;
	entry->dest_l4port_mask = 0xffff;
	FAL_FIELD_FLG_SET(entry->field_flg, FAL_ACL_FIELD_L4_DPORT);

	/* Add PTP IPv4 rule to ACL list */
	ret = fal_acl_rule_add(dev_id, LIST_ID_L4_TAG_SERVICE_CODE_PTP, index++, 1, entry);
	SW_RTN_ON_ERROR(ret);

	/* IPv6 PTP packet */
	entry->rule_type = FAL_ACL_RULE_IP6;
	entry->is_ipv6_val = A_TRUE;

	/* Add PTP IPv6 rule to ACL list */
	ret = fal_acl_rule_add(dev_id, LIST_ID_L4_TAG_SERVICE_CODE_PTP, index++, 1, entry);
	SW_RTN_ON_ERROR(ret);
	aos_mem_free(entry);
	entry = NULL;

	/* Bind PTP ACL list to port bmp */
	ret = fal_acl_list_bind(dev_id, LIST_ID_L2_TAG_SERVICE_CODE_PTP,
			FAL_ACL_DIREC_IN, FAL_ACL_BIND_PORTBITMAP, ptp_port_bmp);
	SW_RTN_ON_ERROR(ret);
	ret = fal_acl_list_bind(dev_id, LIST_ID_L4_TAG_SERVICE_CODE_PTP,
			FAL_ACL_DIREC_IN, FAL_ACL_BIND_PORTBITMAP, ptp_port_bmp);
	SW_RTN_ON_ERROR(ret);

	/* configure the next service code of ptp service code, which
	 * is needed for EDMA receiving the packet with service code.
	 */
	ret = fal_servcode_config_get(dev_id, PTP_EVENT_PKT_SERVICE_CODE, &servcode_conf);
	SW_RTN_ON_ERROR(ret);
	servcode_conf.next_service_code = PTP_EVENT_PKT_SERVICE_CODE;
	ret = fal_servcode_config_set(dev_id, PTP_EVENT_PKT_SERVICE_CODE, &servcode_conf);
	SW_RTN_ON_ERROR(ret);

	return ret;
}
#endif
#endif

sw_error_t
qca_hppe_interface_mode_init(a_uint32_t dev_id)
{

	adpt_api_t *p_api;
	sw_error_t rv = SW_OK;
	fal_port_t port_id;
	a_uint32_t index = 0, uniphy_num = 0, mode[SSDK_MAX_UNIPHY_INSTANCE] = {0};
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);
	SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
	SW_RTN_ON_NULL(p_api->adpt_port_mux_mac_type_set);
	SW_RTN_ON_NULL(p_api->adpt_uniphy_mode_set);

	uniphy_num = adpt_ppe_uniphy_number_get(dev_id);

	for (index = SSDK_UNIPHY_INSTANCE0; index < uniphy_num; index ++) {
		mode[index] = ssdk_dt_global_get_mac_mode(dev_id, index);

		rv = p_api->adpt_uniphy_mode_set(dev_id, index, mode[index]);
		SW_RTN_ON_ERROR(rv);

		if (mode[index] == PORT_WRAPPER_MAX)
			ssdk_gcc_uniphy_sys_set(dev_id, index, A_FALSE);
	}

	for(port_id = SSDK_PHYSICAL_PORT1; port_id < priv->ports_num; port_id++) {
		rv = p_api->adpt_port_mux_mac_type_set(dev_id, port_id, mode[0], mode[1], mode[2]);
		if(rv != SW_OK) {
			SSDK_ERROR("port_id:%d, mode0:%d, mode1:%d, mode2:%d\n", port_id,
				mode[0], mode[1], mode[2]);
			break;
		}
	}

	return rv;
}

#if defined(IN_FLOW)
sw_error_t
qca_hppe_flow_hw_init(a_uint32_t dev_id)
{
	fal_flow_direction_t dir, dir_max;
	fal_flow_mgmt_t mgmt;
	sw_error_t rv;

#if defined(MRPPE)
	fal_flow_npt66_status_set(dev_id, A_TRUE);
#endif

	memset(&mgmt, 0, sizeof(fal_flow_mgmt_t));
	dir_max = FAL_FLOW_UNKOWN_DIR_DIR;

	/*set redirect to cpu for multicast flow*/
	for (dir = FAL_FLOW_LAN_TO_LAN_DIR; dir <= dir_max; dir++) {
		rv = fal_flow_mgmt_get(dev_id, FAL_FLOW_MCAST, dir, &mgmt);
		SW_RTN_ON_ERROR(rv);
		mgmt.miss_action = FAL_MAC_RDT_TO_CPU;
		rv = fal_flow_mgmt_set(dev_id, FAL_FLOW_MCAST, dir, &mgmt);
		SW_RTN_ON_ERROR(rv);
	}
	return SW_OK;
}
#endif
