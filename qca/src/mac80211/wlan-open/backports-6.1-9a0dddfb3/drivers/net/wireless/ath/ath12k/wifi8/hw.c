// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#include "../debug.h"
#include "../core.h"
#include "../ce.h"
#include "ce.h"
#include "../hw.h"
#include "hw.h"
#include "../mhi.h"
#include "mhi.h"
#include "dp_rx.h"
#include "wmi.h"
#include "../wow.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../testmode.h"
#include "../dp_peer.h"
#include "../dp_tx.h"
#include "../mac.h"
#include "dp_tx.h"
#include "dp.h"
#include "hal_qcn9625.h"
#include "mgmt_rx.h"
#include "dp_peer.h"
#include "dp_tx_queue.h"
#include "../dp_mon.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/wifi8_dp_extn.h"
#include "../qcn_extns/ipa/dp_ipa_pub.h"
#endif /* CPTCFG_QCN_EXTN */
#include "../cfr.h"

/*
 * The roaming RX ring uses the slot immediately after the regular REO
 * destination rings (`ATH12K_DP_RX_ROAMING_RING1`). On 4-CPU targets,
 * that evaluates to ring index 4, which is the same bit position normally used
 * by the 5th regular RX ring.
 *
 * Keep the regular RX interrupt masks bounded by
 * `ATH12K_DP_RX_REGULAR_RING_MAX` so the roaming ring can exclusively own its
 * dedicated interrupt group entry below. Without this gating, both group 11
 * (5th regular RX ring) and group 13 (roaming RX ring) would advertise bit 4,
 * and the generic MSI group lookup would pick the first match instead of the
 * dedicated roaming group.
 */
#define ATH12K_WIFI8_REGULAR_RX_RING_MASK(_ring) \
	((ATH12K_DP_RX_REGULAR_RING_MAX > (_ring)) ? BIT(_ring) : 0)

#define ATH12K_WIFI8_BOARD_SIZE		(750 * 1024)

static u8 ath12k_wifi8_hw_qcn9625_mac_from_pdev_id(int pdev_idx)
{
	return pdev_idx;
}

static int
ath12k_wifi8_hw_mac_id_to_pdev_id_qcn9625(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static int
ath12k_wifi8_hw_mac_id_to_srng_id_qcn9625(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static u8 ath12k_wifi8_hw_get_ring_selector_qcn9625(struct sk_buff *skb)
{
	return smp_processor_id();
}

static bool ath12k_wifi8_dp_srng_is_comp_ring_qcn9625(int ring_num)
{
	return false;
}

static bool ath12k_wifi8_hw_link_id_required_in_mgmt_send_qcn9625(struct ath12k_base *ab)
{
	return true;
}

static void ath12k_wifi8_hw_rx_peer_ba_config_qcn9625(struct ath12k_base *ab, u8 tid,
						      u32 *ba_win_size, u16 *ssn)
{
	u32 ba_win_size_val = 1;
	u16 ssn_val = 0;

	if (ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid))
		ba_win_size_val = ab->ag->num_devices;

	if (ba_win_size)
		*ba_win_size = ba_win_size_val;

	if (ssn)
		*ssn = ssn_val;
}

static bool
ath12k_wifi8_rx_peer_tid_skip_pn_replay_qcn9625(struct ath12k_base *ab, u8 tid)
{
	return (tid == HAL_MGMT_BCAST_TID || tid == HAL_MGMT_SENSING_TID);
}

/* Note: called under rcu_read_lock() */
static void
ath12k_dp_peer_migration_qcn9625(struct ath12k_link_vif *arvif,
				 struct ath12k_mac_pri_link_migr_peer_node *peer_node)
{
	struct ath12k_link_sta *arsta, *old_arsta;
	struct ath12k_dp_link_peer *old_peer, *new_peer;
	struct ath12k_link_vif *old_arvif;
	struct ath12k_hw *ah = arvif->ar->ah;
	struct ath12k_dp_peer *ml_peer;
	struct ath12k_sta *ahsta;
	u8 old_link_id;

	rcu_read_lock();
	/* The primary_link_id needs to be updated here based on the link_id sent
	 * in the migration command.
	 */
	if (peer_node->ml_peer_id >= MAX_DP_PEER_LIST_SIZE) {
		ath12k_warn(arvif->ar->ab,
			    "peer migration: peer_id %u out of range (max %u), skipping\n",
			    peer_node->ml_peer_id, MAX_DP_PEER_LIST_SIZE);
		rcu_read_unlock();
		return;
	}

	ml_peer = rcu_dereference(ah->dp_hw.dp_peer_list[peer_node->ml_peer_id]);
	if (ml_peer && ml_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED) {
		ahsta = ath12k_sta_to_ahsta(ml_peer->sta);
		old_link_id = ahsta->primary_link_id;

		/* Clear old primary link flags */
		if (old_link_id != peer_node->pri_link_id) {
			old_arsta = rcu_dereference(ahsta->link[old_link_id]);
			if (old_arsta && old_arsta->arvif) {
				old_arvif = old_arsta->arvif;
				old_arvif->primary_sta_link = false;
			}

			old_peer =
				ath12k_dp_link_peer_find_by_logical_link_id(ml_peer,
									    old_link_id);
			if (old_peer)
				old_peer->primary_link = false;
		}

		/* Update to new primary link */
		ahsta->primary_link_id = peer_node->pri_link_id;
		ml_peer->primary_link_id = peer_node->pri_link_id;

		/* Set new primary link flags */
		arsta = rcu_dereference(ahsta->link[peer_node->pri_link_id]);
		arsta->arvif->primary_sta_link = true;

		new_peer =
		ath12k_dp_link_peer_find_by_logical_link_id(ml_peer,
							    peer_node->pri_link_id);
		if (new_peer)
			new_peer->primary_link = true;

		ahsta->is_migration_in_progress = false;

		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_WMI,
			   "Updated primary_link_id from %u to %u for sta %pM\n",
			   old_link_id, peer_node->pri_link_id, ml_peer->sta->addr);
	}
	rcu_read_unlock();
}

void ath12k_hw_qcn9625_fill_cfr_hdr_info(struct ath12k *ar,
					 struct ath12k_csi_cfr_header *header,
					 struct ath12k_cfr_peer_tx_param *params)
{
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->cfr_metadata_len = sizeof(struct cfr_enh_metadata);
	header->host_real_ts = ktime_to_ns(ktime_get_real());

	header->cfr_metadata_version = ATH12K_CFR_META_VERSION_11;
	header->chip_type = ATH12K_CFR_RADIO_QCN9625;

	header->meta_enh.status = FIELD_GET(WMI_CFR_PEER_CAPTURE_STATUS,
					    params->status);
	header->meta_enh.tx_pkt_bw = params->bandwidth;
	header->meta_enh.phy_mode = params->phy_mode;
	header->meta_enh.center_freq1 = params->band_center_freq1;
	header->meta_enh.center_freq2 = params->band_center_freq2;
	header->meta_enh.num_mu_users = 0;
	header->meta_enh.rx_start_ts = params->rx_start_ts;
	header->meta_enh.cfo_measurement = params->cfo_measurement;
	header->meta_enh.mcs_rate = params->mcs_rate;
	header->meta_enh.gi_type = params->gi_type;
	header->meta_enh.beamformed = 0;

	memcpy(header->meta_enh.su_peer_addr,
	       params->peer_mac_addr, ETH_ALEN);
	memcpy(header->meta_enh.chain_rssi, params->chain_rssi,
	       sizeof(params->chain_rssi));
	memcpy(header->meta_enh.chain_phase, params->chain_phase,
	       sizeof(params->chain_phase));
	memcpy(header->meta_enh.agc_gain, params->agc_gain,
	       sizeof(params->agc_gain));
	memcpy(header->meta_enh.agc_gain_tbl_index, params->agc_gain_tbl_index,
	       sizeof(params->agc_gain_tbl_index));
}

/*
 * Parses the wifi8 (QCN9625) CFR upload header -- ucode's
 * locsens_common_header_t + cc_upload_header_struct_t layout. This is a
 * distinct byte layout from ath12k_cfir_enh_dma_hdr (wifi7), not an
 * extension of it, so it is not shared with
 * ath12k_hw_wifi7_parse_cfr_enh_dma_hdr().
 *
 * freeze_capture_tlv itself is assumed unchanged from wifi7
 * (macrx_freeze_capture_channel_v5 / MACRX_FREEZE_TLV_VERSION_5) --
 * ucode's cc_upload_header_struct_t comment lists no version beyond 5
 * and documents the freeze TLV size only as HMT vs WKK(16 x u16), which
 * matches _v5. freeze_reason_to_capture_type() only reads freeze->info0,
 * which is common to all versions, so no version dispatch is done here.
 *
 */
int ath12k_hw_qcn9625_parse_cfr_enh_dma_hdr(struct ath12k *ar, u8 *data,
					    struct ath12k_cfr_look_up_table *lut,
					    u32 *length)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_cfir_wifi8_common_hdr common_hdr;
	struct ath12k_cfir_wifi8_cc_hdr cc_hdr;
	struct cfr_enh_metadata *meta;
	u8 *cc_hdr_wire;
	void *freeze_tlv = NULL;
	u8 *peer_macaddr;
	u8 capture_type;

	memcpy(&common_hdr, data, sizeof(struct ath12k_cfir_wifi8_common_hdr));

	if (common_hdr.header_tag != 0xC0DE00BA) {
		ath12k_warn(ab, "unexpected wifi8 CFR header tag 0x%x\n",
			    common_hdr.header_tag);
		return -EINVAL;
	}

	cc_hdr_wire = data + sizeof(struct ath12k_cfir_wifi8_common_hdr);
	memcpy(&cc_hdr, cc_hdr_wire, sizeof(struct ath12k_cfir_wifi8_cc_hdr));

	if (cc_hdr.freeze_data_incl) {
		/* freeze tlv starts from common_header + freeze_tlv_offset * 4
		 * after 32 words from common header
		 */
		freeze_tlv = data + (cc_hdr.freeze_tlv_offset * 4);
		capture_type = freeze_reason_to_capture_type(ab, freeze_tlv);
	} else {
		capture_type = CFR_CAPTURE_METHOD_AUTO;
	}

	/* header_size is already in bytes for the wifi8 layout, unlike
	 * ath12k_cfir_enh_dma_hdr.length (words) on wifi7.
	 */
	*length = common_hdr.header_size;
	*length += common_hdr.payload_size;

	lut->dbr_ppdu_id = common_hdr.phy_ppdu_id;
	lut->header_length = common_hdr.header_size;
	lut->payload_length = common_hdr.payload_size;
	memcpy(&lut->dma_hdr.wifi8_hdr, &common_hdr,
	       sizeof(struct ath12k_cfir_wifi8_common_hdr));

	meta = &lut->header.meta_enh;

	if (capture_type != CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM) {
		if (!cc_hdr.mu_rx_data_incl) {
			peer_macaddr = meta->su_peer_addr;
			if (cc_hdr.freeze_data_incl)
				extract_peer_mac_from_freeze_tlv(freeze_tlv,
								 peer_macaddr);
		}
	}

	return 0;
}

static const struct ath12k_hw_ops qcn9625_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi8_hw_qcn9625_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi8_hw_mac_id_to_pdev_id_qcn9625,
	.mac_id_to_srng_id = ath12k_wifi8_hw_mac_id_to_srng_id_qcn9625,
	.rxdma_ring_sel_config = ath12k_wifi8_dp_rxdma_ring_sel_config_qcn9625,
	.get_ring_selector = ath12k_wifi8_hw_get_ring_selector_qcn9625,
	.dp_srng_is_tx_comp_ring = ath12k_wifi8_dp_srng_is_comp_ring_qcn9625,
	.fill_cfr_hdr_info = ath12k_hw_qcn9625_fill_cfr_hdr_info,
	.parse_cfr_enh_dma_hdr = ath12k_hw_qcn9625_parse_cfr_enh_dma_hdr,
	.hw_link_id_required_in_mgmt_send =
		ath12k_wifi8_hw_link_id_required_in_mgmt_send_qcn9625,
	.mgmt_rxdma_ring_sel_config = ath12k_wifi8_mgmt_wbm_ring_sel_config_qcn9625,
	.rx_peer_ba_config = ath12k_wifi8_hw_rx_peer_ba_config_qcn9625,
	.rx_peer_tid_skip_pn_replay = ath12k_wifi8_rx_peer_tid_skip_pn_replay_qcn9625,
	.dp_peer_migration = ath12k_dp_peer_migration_qcn9625,
	.is_mgmt_reoq_tid = ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid,
	.update_tqm_status_ring_tp = ath12k_wifi8_update_tqm_status_ring_tp,
};

/* Interrupt Grouping is as follows
 * Group 0-3: Tx completion
 * Group 4-7: Rx ring
 * Group 8: Tx exception ring, fw_tqm2sw
 * Group 9: Rx error, Reo Status, TCL status, TQM status
 * Group 10,11 : Monitor destination(TX,RX)
 * Group 12: Monitor buffer(TX,RX)
 * Group 13: Roaming RX ring, TX/RX peer telemetry
 * Group 17: UMCMN interrupts
 * Group 18-20: PPE interrupts
 * Group 21: UMAC reset
 */
static struct ath12k_hw_ring_mask ath12k_wifi8_hw_ring_mask_qcn9625 = {
	/* Group 0-3, 5th ring uses group 10 */
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		ATH12K_TX_RING_MASK_3,
		0, 0, 0, 0,
		0, 0,
		ATH12K_TX_RING_MASK_4,
	},
	/* Group 4-7, 5th regular RX ring uses group 11 (not roaming ring).
	 * Regular RX rings are gated by ATH12K_DP_RX_REGULAR_RING_MAX to avoid
	 * collisions with the roaming ring index.
	 */
	.rx = {
		0, 0, 0, 0,
		ATH12K_WIFI8_REGULAR_RX_RING_MASK(0),
		ATH12K_WIFI8_REGULAR_RX_RING_MASK(1),
		ATH12K_WIFI8_REGULAR_RX_RING_MASK(2),
		ATH12K_WIFI8_REGULAR_RX_RING_MASK(3),
		0, 0, 0,
#ifdef CPTCFG_EXT_IPA_OFFLOAD
		0,
#else
		ATH12K_WIFI8_REGULAR_RX_RING_MASK(4),
#endif
		0,
		BIT(ATH12K_DP_RX_ROAMING_RING1),
	},
	/* Group 8: Tx exception ring */
	.tx_exception = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_TX_EXCEPTION_RING_MASK_0,
	},
	/* Group 8: fw_tqm2sw ring
	 * (FW-owned buffer completion WAR - HAL_TQM_HOST_STATUS_RING=1)
	 */
	.tqm2sw_fw = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_FW_TQM2SW_RING_MASK_0,
	},
	/* Group 9 */
	.rx_err = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_RX_ERR_RING_MASK_0,
	},
	/* Group 9 */
	.reo_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_REO_STATUS_RING_MASK_0,
	},
	/* Group 9 */
	.tcl_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TCL_STATUS_RING_MASK_0,
	},
	/* Group 9 */
	.tqm_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TQM_STATUS_RING_MASK_0,
	},
	.sam_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_SAM_STATUS_RING_MASK_0,
	},
	/* Group 9 */
	.ase_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_ASE_STATUS_RING_MASK_0,
	},
	/* Group 9*/
	.reo_flush = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_REO_FLUSH_RING_MASK_0,
	},
	/* Group 10, 11 */
	.rx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
	},
	/* Group 12 */
	.host2rxmon = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	/* Group 10, 11 */
	.tx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
	},
	/* Group 12 */
	.host2txmon = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_HOST2TX_MON_RING_MASK_0,
	},
	/* Group 13 */
	.tx_peer_telemetry = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TX_PEER_TELEMETRY_RING_MASK
	},
	.rx_peer_telemetry = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_RX_PEER_TELEMETRY_RING_MASK
	},
	/* Group 17 */
	.umcmn_interrupts = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_UMCMN_INTR_MASK_0,
	},
	/* Group 18-20 */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_PPE2TCL_RING_MASK_0,
	},
	.reo2ppe = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0,
		ATH12K_REO2PPE_RING_MASK_0,
	},
	.ppeds_tx_cmpln = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_PPE_TQM2SW_RELEASE_RING_MASK_0
	},
#endif
	/* Group 21 */
	.umac_dp_reset = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_UMAC_RESET_INTR_MASK_0
	},
};

static struct ath12k_hw_params ath12k_wifi8_hw_params[] = {
	{
		/* fw_tqm2sw WAR: enabled only on Trestles V1 (HW1.0).
		 * This hardware issue is fixed in V2; do not set on HW2.0.
		 */
		.name = "qcn9625 hw1.0",
		.hw_rev = ATH12K_HW_QCN9625_HW10,
		.fw = {
			.dir = "QCN9625/hw1.0",
			.board_size = ATH12K_WIFI8_BOARD_SIZE,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.unified_fw_image = true,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9625,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9625_ops,
		.ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625,

		.host_ce_config = ath12k_wifi8_host_ce_config_qcn9625,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi8_target_ce_config_wlan_qcn9625,
		.target_ce_count = 13,
		/* TODO: CP: update CE maps and definitions later when available */
		.svc_to_ce_map =
			ath12k_wifi8_target_service_to_ce_map_wlan_qcn9625,
		.svc_to_ce_map_len = 21,
		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
		.supports_monitor = true,
		.max_clients_supported = 512,
		.max_clients_dbs = 256,
		.max_clients_dbs_sbs = 170,
		.supports_tx_monitor = true,
		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 128,
		.max_tx_ring = MIN(NR_CPUS, 5),

		.mhi_config = &ath12k_wifi8_mhi_config_qcn9625,

		.wmi_init = ath12k_wifi8_wmi_init_qcn9625,

		.hal_ops = &hal_qcn9625_ops,
		.cp_arch_ops = &ath12k_wifi8_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_AUX_UC_SUPPORT_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0x600000,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = QCN9625_QFPROM_RAW_FEATURE_CONFIG_ROW4_LSB,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = true,

		.iova_mask = 0,

#ifdef PLATFORM_SDX
		.supports_aspm = false,
#else
		.supports_aspm = true,
#endif

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = true,
		.cumac_support = true,
		.cumac_chip_priority = 1,
		.tqm_status_war = true,
		.support_umcmn_interrupts = UMCMN_INTERRUPT_POLL,
#ifdef PLATFORM_SDX
		.alloc_cacheable_memory = false,
#else
		.alloc_cacheable_memory = true,
#endif
		.spectral = {
			.fft_sz = 7,
			.fft_bin_sz = 1,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 1024,
			.fft_size_min = ATH12K_SPECTRAL_FFT_SIZE_MIN,
			.fft_size_max = {
				[ATH12K_SPECTRAL_BW_20MHZ]  = 9,
				[ATH12K_SPECTRAL_BW_40MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_80MHZ]  = 11,
				[ATH12K_SPECTRAL_BW_160MHZ] = 11,
				[ATH12K_SPECTRAL_BW_320MHZ] = 11,
			},
			.fragment_160mhz = true,
			.supports_320mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = true,
		.ds_txrx_hw_auto_idx = true,
		.ds_hw_buff_mgmt = true,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_wifi8_common_hdr),
		.cfr_num_stream_bufs = 128,
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN9625 * 4) +
					CFR_DATA_MAX_LEN_QCN9625,
		.mlo_3_link_tx_support = true,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = true,
		.tqm2sw_fw_war = true,
	},
	{
		/* fw_tqm2sw WAR: enabled only on Trestles V1 (HW1.0).
		 * This hardware issue is fixed in V2; do not set on HW2.0.
		 */
		.name = "qcn9589 hw1.0",
		.hw_rev = ATH12K_HW_QCN9589_HW10,
		.fw = {
			.dir = "QCN9589/hw1.0",
			.board_size = ATH12K_WIFI8_BOARD_SIZE,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.unified_fw_image = true,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9625,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9625_ops,
		.ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625,

		.host_ce_config = ath12k_wifi8_host_ce_config_qcn9625,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi8_target_ce_config_wlan_qcn9625,
		.target_ce_count = 13,
		.svc_to_ce_map =
			ath12k_wifi8_target_service_to_ce_map_wlan_qcn9625,
		.svc_to_ce_map_len = 21,
		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
		.supports_monitor = true,
		.max_clients_supported = 512,
		.max_clients_dbs = 256,
		.max_clients_dbs_sbs = 170,
		.supports_tx_monitor = true,
		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 128,
		.max_tx_ring = MIN(NR_CPUS, 5),

		.mhi_config = &ath12k_wifi8_mhi_config_qcn9625,

		.wmi_init = ath12k_wifi8_wmi_init_qcn9625,

		.hal_ops = &hal_qcn9625_ops,
		.cp_arch_ops = &ath12k_wifi8_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_AUX_UC_SUPPORT_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0x680000,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = QCN9625_QFPROM_RAW_FEATURE_CONFIG_ROW4_LSB,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = true,

		.iova_mask = 0,

#ifdef PLATFORM_SDX
		.supports_aspm = false,
#else
		.supports_aspm = true,
#endif

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = true,
		.cumac_support = true,
		.cumac_chip_priority = 2,
		.tqm_status_war = true,
		.support_umcmn_interrupts = UMCMN_INTERRUPT_POLL,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_bin_sz = 1,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 1024,
			.fft_size_min = ATH12K_SPECTRAL_FFT_SIZE_MIN,
			.fft_size_max = {
				[ATH12K_SPECTRAL_BW_20MHZ]  = 9,
				[ATH12K_SPECTRAL_BW_40MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_80MHZ]  = 11,
				[ATH12K_SPECTRAL_BW_160MHZ] = 11,
				[ATH12K_SPECTRAL_BW_320MHZ] = 11,
			},
			.fragment_160mhz = true,
			.supports_320mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = true,
		.ds_txrx_hw_auto_idx = true,
		.ds_hw_buff_mgmt = true,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_wifi8_common_hdr),
		.cfr_num_stream_bufs = 128,
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN9625 * 4) +
					CFR_DATA_MAX_LEN_QCN9625,
		.mlo_3_link_tx_support = true,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = true,
		.tqm2sw_fw_war = true,
	},
	{
		.name = "qcn9625 hw2.0",
		.hw_rev = ATH12K_HW_QCN9625_HW20,
		.fw = {
			.dir = "QCN9625/hw2.0",
			.board_size = ATH12K_WIFI8_BOARD_SIZE,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.unified_fw_image = true,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9625,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9625_ops,
		.ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625,

		.host_ce_config = ath12k_wifi8_host_ce_config_qcn9625,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi8_target_ce_config_wlan_qcn9625,
		.target_ce_count = 13,
		.svc_to_ce_map =
			ath12k_wifi8_target_service_to_ce_map_wlan_qcn9625,
		.svc_to_ce_map_len = 21,
		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
		.supports_monitor = true,
		.max_clients_supported = 512,
		.max_clients_dbs = 256,
		.max_clients_dbs_sbs = 170,
		.supports_tx_monitor = true,
		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 128,
		.max_tx_ring = MIN(NR_CPUS, 5),

		.mhi_config = &ath12k_wifi8_mhi_config_qcn9625,

		.wmi_init = ath12k_wifi8_wmi_init_qcn9625,

		.hal_ops = &hal_qcn9625_ops,
		.cp_arch_ops = &ath12k_wifi8_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_AUX_UC_SUPPORT_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0x600000,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = QCN9625_QFPROM_RAW_FEATURE_CONFIG_ROW4_LSB,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = true,

		.iova_mask = 0,

#ifdef PLATFORM_SDX
		.supports_aspm = false,
#else
		.supports_aspm = true,
#endif
		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = true,
		.cumac_support = true,
		.cumac_chip_priority = 1,
		.tqm_status_war = false,
		.support_umcmn_interrupts = UMCMN_INTERRUPT_ENABLE,
#ifdef PLATFORM_SDX
		.alloc_cacheable_memory = false,
#else
		.alloc_cacheable_memory = true,
#endif
		.spectral = {
			.fft_sz = 7,
			.fft_bin_sz = 1,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 1024,
			.fft_size_min = ATH12K_SPECTRAL_FFT_SIZE_MIN,
			.fft_size_max = {
				[ATH12K_SPECTRAL_BW_20MHZ]  = 9,
				[ATH12K_SPECTRAL_BW_40MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_80MHZ]  = 11,
				[ATH12K_SPECTRAL_BW_160MHZ] = 11,
				[ATH12K_SPECTRAL_BW_320MHZ] = 11,
			},
			.fragment_160mhz = true,
			.supports_320mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = true,
		.ds_txrx_hw_auto_idx = true,
		.ds_hw_buff_mgmt = true,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_wifi8_common_hdr),
		.cfr_num_stream_bufs = 128,
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN9625 * 4) +
					CFR_DATA_MAX_LEN_QCN9625,
		.mlo_3_link_tx_support = true,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = true,
		.tqm2sw_fw_war = false,
	},
};

static bool ath12k_wifi8_mac_is_mgmt_action_link_agnostic(struct ath12k_base *ab,
							  struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
	const u8 *buf = (u8 *)&mgmt->u.action;
	u8 category, action_code, iv_len;

	/* Offset by iv_len if it is a protected frame */
	if (ieee80211_has_protected(mgmt->frame_control)) {
		switch (ATH12K_SKB_CB(skb)->cipher) {
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_CCMP_256:
			iv_len = IEEE80211_CCMP_HDR_LEN;
			break;
		case WLAN_CIPHER_SUITE_GCMP:
		case WLAN_CIPHER_SUITE_GCMP_256:
			iv_len = IEEE80211_GCMP_HDR_LEN;
			break;
		default:
			iv_len = 0;
			break;
		}

		buf += iv_len;
	}

	category = *buf++;
	action_code = *buf++;

	switch (category) {
	case WLAN_CATEGORY_SPECTRUM_MGMT:
		if (action_code == WLAN_ACTION_SPCT_CHL_SWITCH) {
			bool ml_link = ath12k_mgmt_has_ml_link_info_ie(ab, skb);

			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
					 "%s spectrum-mgmt csa link agnostic %s\n",
					 __func__, ml_link ? "set" : "clear");
			return ml_link;
		}
		break;

	case WLAN_CATEGORY_PROTECTED_EHT:
		switch (action_code) {
		case WLAN_PROTECTED_EHT_ACTION_ML_OP_UPDATE_REQ:
		case WLAN_PROTECTED_EHT_ACTION_ML_OP_UPDATE_RESP:
			fallthrough;
		case WLAN_PROTECTED_EHT_ACTION_LINK_RECONFIG_REQ:
		case WLAN_PROTECTED_EHT_ACTION_LINK_RECONFIG_RESP:
			/* Exempt Link Reconfig Req/Resp frames from tx-ed as
			 * link-agnostic since both frames should be exchanged on
			 * the same link.
			 *
			 * IEEE P802.11be/D7.0, 35.3.6.4 - Link reconfiguration to
			 * the ML setup
			 */
			return false;
		}
		break;
	case WLAN_CATEGORY_RADIO_MEASUREMENT:
		return false;
	case WLAN_CATEGORY_WNM:
		switch (action_code) {
		case WLAN_WNM_ACTION_EVENT_REQ:
		case WLAN_WNM_ACTION_EVENT_RESP:
		case WLAN_WNM_ACTION_DIAGNOSTIC_REQ:
		case WLAN_WNM_ACTION_DIAGNOSTIC_RESP:
		case WLAN_WNM_ACTION_LOCATION_CFG_REQ:
		case WLAN_WNM_ACTION_LOCATION_CFG_RESP:
		case WLAN_WNM_ACTION_BSS_TM_QUERY:
			return false;
		}
		break;
	case WLAN_CATEGORY_PROTECTED_UHR: {
		u8 type = *(++buf);

		if (type == IEEE80211_UHR_LINK_RECONF_TYPE_ST_EXEC)
			return false;
		else
			return true;
		break;
	}
	default:
		/* Extend as per feature addition */
		break;
	}

	return true;
}

/* This function should be called only for mgmt frames to a Multi-Link device,
 * after meeting master link eligibility. Hence, such sanity checks are skipped.
 */
static bool ath12k_wifi8_mac_is_mgmt_link_agnostic(struct ath12k_base *ab,
						   struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
	u16 fc = le16_to_cpu(mgmt->frame_control);

	switch (fc & IEEE80211_FCTL_STYPE) {
	case IEEE80211_STYPE_DEAUTH:
	case IEEE80211_STYPE_DISASSOC:
		return true;
	case IEEE80211_STYPE_ACTION:
		return ath12k_wifi8_mac_is_mgmt_action_link_agnostic(ab, skb);
	default:
		break;
	}

	return false;
}

/* Note: called under rcu_read_lock() */
static u8
ath12k_wifi8_mac_get_tx_link(struct ieee80211_sta *sta, struct ieee80211_vif *vif,
			     u8 link, struct sk_buff *skb, u32 info_flags)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ieee80211_link_sta *link_sta;
	struct ieee80211_bss_conf *bss_conf;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *ab = NULL;
	struct ath12k_sta *ahsta;
	struct ath12k *ar = NULL;
	unsigned long links;
	u8 link_id;

	/* Use the link id passed or the first available link */
	if (!sta) {
		if (link != IEEE80211_LINK_UNSPECIFIED &&
		    (ahvif->links_map & BIT(link)))
			return link;

		return ffs(ahvif->links_map) - 1;
	}

	ahsta = ath12k_sta_to_ahsta(sta);

	/* Below translation ensures we pass proper A2 & A3 for non ML clients.
	 * Also it assumes for now support only for MLO AP in this path
	 */
	if (!sta->mlo) {
		link = ahsta->deflink.link_id;

		if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
			return link;

		bss_conf = rcu_dereference(vif->link_conf[link]);
		if (bss_conf) {
			ether_addr_copy(hdr->addr2, bss_conf->addr);
			if (!ieee80211_has_tods(hdr->frame_control) &&
			    !ieee80211_has_fromds(hdr->frame_control))
				ether_addr_copy(hdr->addr3, bss_conf->addr);
		}

		return link;
	}

	/* enqueue eth enacap & data frames on primary link, FW does link
	 * selection and address translation.
	 */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP ||
	    ieee80211_is_data(hdr->frame_control))
		return ahsta->primary_link_id;

	/* 802.11 frame cases */
	if (link == IEEE80211_LINK_UNSPECIFIED) {
		link = ahsta->deflink.link_id;
		/* deflink.link_id may be stale (255) after a link is removed from
		 * the serving-AP STA during SMD transition. Fall back to
		 * primary_link_id which is stable for the lifetime of the session.
		 */
		if (link >= ATH12K_NUM_MAX_LINKS || !(ahsta->links_map & BIT(link)))
			link = ahsta->primary_link_id;
	}

	if (!ieee80211_is_mgmt(hdr->frame_control))
		return link;

	if (ahsta->deflink.arvif && ahsta->deflink.arvif->ar) {
		ar = ahsta->deflink.arvif->ar;
		ab = ar->ab;
	} else {
		/* deflink.arvif is stale; derive ar/ab from the resolved link */
		struct ath12k_link_sta *_arsta = rcu_dereference(ahsta->link[link]);

		if (_arsta && _arsta->arvif && _arsta->arvif->ar) {
			ar = _arsta->arvif->ar;
			ab = ar->ab;
		}
	}

	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    ieee80211_has_protected(hdr->frame_control))
		goto skip_link_agnostic_tx;

	if (ab && test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) &&
	    ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
		/* If disassoc frame comes in crash link, need to
		 * change the link which is active at that instance.
		 */
		if (ieee80211_is_disassoc(hdr->frame_control)) {
			link = ahsta->deflink.link_id;
			links = ahsta->links_map;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta = rcu_dereference(ahsta->link[link_id]);
				if (!arsta)
					continue;
				ab = arsta->arvif->ar->ab;
				if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) {
					link = arsta->link_id;
					break;
				}
			}
			goto skip_link_agnostic_tx;
		}
	}

	/* Check if this mgmt frame can be queued at MLD level, in which case,
	 * the frame will be transmitted on master (primary) link. An individually
	 * addressed mgmt frame can be transmitted on master link after peer assoc.
	 */
	if (ahsta->state < IEEE80211_STA_ASSOC ||
	    !ath12k_wifi8_mac_is_mgmt_link_agnostic(ab, skb))
		goto skip_link_agnostic_tx;

	ATH12K_SKB_CB(skb)->flags |= ATH12K_SKB_MGMT_LINK_AGNOSTIC;
	link = ahsta->primary_link_id;

skip_link_agnostic_tx:
	/* Perform address conversion for ML STA Tx */
	bss_conf = rcu_dereference(vif->link_conf[link]);
	link_sta = rcu_dereference(sta->link[link]);

	if (bss_conf && link_sta) {
		ether_addr_copy(hdr->addr1, link_sta->addr);
		ether_addr_copy(hdr->addr2, bss_conf->addr);

		if (vif->type == NL80211_IFTYPE_STATION && bss_conf->bssid)
			ether_addr_copy(hdr->addr3, bss_conf->bssid);
		else if (vif->type == NL80211_IFTYPE_AP)
			ether_addr_copy(hdr->addr3, bss_conf->addr);

		return link;
	}

	if (bss_conf) {
		/* In certain cases where a ML sta associated and added subset of
		 * links on which the ML AP is active, but now sends some frame
		 * (ex. Probe request) on a different link which is active in our
		 * MLD but was not added during previous association, we can
		 * still honor the Tx to that ML STA via the requested link.
		 * The control would reach here in such case only when that link
		 * address is same as the MLD address or in worst case clients
		 * used MLD address at TA wrongly which would have helped
		 * identify the ML sta object and pass it here.
		 * If the link address of that STA is different from MLD address,
		 * then the sta object would be NULL and control won't reach
		 * here but return at the start of the function itself with !sta
		 * check. Also this would not need any translation at hdr->addr1
		 * from MLD to link address since the RA is the MLD address
		 * (same as that link address ideally) already.
		 */
		ether_addr_copy(hdr->addr2, bss_conf->addr);

		if (vif->type == NL80211_IFTYPE_STATION && bss_conf->bssid)
			ether_addr_copy(hdr->addr3, bss_conf->bssid);
		else if (vif->type == NL80211_IFTYPE_AP)
			ether_addr_copy(hdr->addr3, bss_conf->addr);
	}

	return link;
}

/**
 * ath12k_wifi8_tx_setup_link() - Setup link for transmission
 * @vif: Virtual interface
 * @sta: Station pointer
 * @info: TX info
 * @skb: Socket buffer
 * @link_id: Output link ID
 *
 * Returns: 0 on success, negative on error
 */
int ath12k_wifi8_tx_setup_link(struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta,
			       struct ieee80211_tx_info *info,
			       struct sk_buff *skb,
			       u8 *link_id)
{
	u32 info_flags = info->flags;

	*link_id = u32_get_bits(info->control.flags, IEEE80211_TX_CTRL_MLO_LINK);
	if (ieee80211_vif_is_mld(vif)) {
		*link_id = ath12k_wifi8_mac_get_tx_link(sta, vif, *link_id, skb,
							info_flags);
		if (*link_id >= ATH12K_NUM_MAX_LINKS ||
		    (ATH12K_SCAN_LINKS_MASK & BIT(*link_id))) {
			return -EINVAL;
		}
	} else {
		*link_id = 0;
	}

	return 0;
}

static void ath12k_wifi8_mgmt_handler(struct ieee80211_hw *hw,
				      struct ieee80211_tx_control *control,
				      struct ieee80211_tx_info *info,
				      struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ieee80211_vif *vif = info->control.vif;
	struct ieee80211_key_conf *key = info->control.hw_key;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_sta *ahsta = NULL;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ath12k_mgmt_frame_stats *mgmt_stats = &ahvif->mgmt_stats;
	struct ieee80211_sta *sta = control->sta;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	u32 control_flags = info->control.flags;
	struct ath12k *ar;
	struct ieee80211_mgmt *mgmt = NULL;
	u8 link_id = 0, ring_id = 0;
	u16 frm_type = 0;
	int ret;
	bool is_prb_rsp;

	ret = ath12k_wifi8_tx_setup_link(vif, sta, info, skb, &link_id);
	if (ret) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_LINK,
						0, false);
		return;
	}

	/* MLO params should always be added to class-3 frames queued
	 * for peers on Wi-Fi 8.
	 */
	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		if (ahsta->state >= IEEE80211_STA_ASSOC)
			skb_cb->flags |= ATH12K_SKB_MGMT_MLO_PARAMS;
	}

	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
	}

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (!arvif || !arvif->ar) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
						sta, dp_vif,
						DP_TX_ENQ_DROP_INV_ARVIF,
						ring_id, false);
		return;
	}

	ar = arvif->ar;
	skb_cb->link_id = link_id;
	skb_cb->vif = vif;

	if (unlikely(test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags))) {
		ieee80211_free_txskb(hw, skb);
		return;
	}

	skb_cb->u.ar = ar;

	/* Get DP pdev */
	dp_pdev = ath12k_dp_to_dp_pdev(ar->ab->dp, ar->pdev_idx);
	if (!dp_pdev) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_PDEV,
						ring_id, false);
		return;
	}

	is_prb_rsp = ieee80211_is_probe_resp(hdr->frame_control);

	if (is_prb_rsp && arvif->tbtt_offset) {
		u64 adjusted_tsf;

		mgmt = (struct ieee80211_mgmt *)skb->data;
		adjusted_tsf = cpu_to_le64(0ULL - arvif->tbtt_offset);
		memcpy(&mgmt->u.probe_resp.timestamp, &adjusted_tsf,
		       sizeof(adjusted_tsf));
	}

	if ((control_flags & IEEE80211_TX_CTRL_MGMT_RATE_EXIST) &&
	    info->control.rates[0].idx >= 0) {
		if (ath12k_skb_rhash_insert(ar, skb,
					    info->control.rates[0]))
			ath12k_warn(ar->ab,
				    "tx skb rhash entry creation failed\n");
	}

	frm_type = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
	ret = ath12k_mac_mgmt_tx(ar, skb, is_prb_rsp);
	if (ret) {
		if (ret != -EBUSY)
			ath12k_warn(ar->ab, "failed to queue mgmt stype 0x%x frame %d\n",
				    frm_type, ret);

		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
						sta, dp_vif,
						DP_TX_ENQ_DROP_MGMT_FRAME,
						ring_id, false);
		spin_lock_bh(&ar->data_lock);
		mgmt_stats->tx_fail_cnt[frm_type]++;
		mgmt_stats->aggr_tx_mgmt_fail_cnt++;
		spin_unlock_bh(&ar->data_lock);
	} else {
		spin_lock_bh(&ar->data_lock);
		mgmt_stats->tx_succ_cnt[frm_type]++;
		mgmt_stats->aggr_tx_mgmt_success_cnt++;
		spin_unlock_bh(&ar->data_lock);
	}
}

/* Note: called under rcu_read_lock() */
static void ath12k_wifi8_mac_op_tx(struct ieee80211_hw *hw,
				   struct ieee80211_tx_control *control,
				   struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = NULL;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ieee80211_vif *vlan_vif = control->vlan_vif;
	struct ath12k_vif *vlan_ahvif = vlan_vif ? ath12k_vif_to_ahvif(vlan_vif) : NULL;
	struct ath12k_link_vif *arvif;
	struct ieee80211_hdr *hdr = NULL;
	struct ieee80211_key_conf *key;
	struct ieee80211_sta *sta = control->sta;
	struct ath12k_link_sta *arsta = NULL;
	struct ath12k_sta *ahsta = NULL;
#ifdef CPTCFG_QCN_EXTN
	u32 info_flags = info->flags;
#endif /* CPTCFG_QCN_EXTN */
	struct ieee80211_tx_info info_tx;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	bool is_mcast = false, is_eth = false, is_data = false;
	bool is_dvlan = false, is_sta = false;
	u32 qos_nw_delay = info->sawf.nw_delay;
	u8 link_id;
	u8 qos_tag;
	bool is_pkt_classified = false, gsn_valid = true;
	struct ath12k_dp_skb_ctrl skb_ctrl = {0};
	bool htt_mesh;
	int ret;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k *ar = NULL;

	/* Check queue stop.*/
	if (unlikely(ah->queue_stop)) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_QUEUE_STOP,
						0, true);
		return;
	}

#ifdef CPTCFG_QCN_EXTN
	/* Fast path.*/
	if (likely(ath12k_wifi8_dp_tx_check_fast_path(skb, info_flags, dp_vif, &skb_ctrl,
						      qos_nw_delay, vlan_ahvif)))
		return;
#endif
	arvif = &ahvif->deflink;
	key = info->control.hw_key;

	hdr = (struct ieee80211_hdr *)skb->data;
	memcpy(&info_tx, info, sizeof(*info));
	skb_cb = ATH12K_SKB_CB(skb);
	memset(skb_cb, 0, sizeof(*skb_cb));

	/* Check monitor mode.*/
	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_VIF_TYPE_MON,
						0, false);
		return;
	}

	/* Classify packets.*/
	is_pkt_classified = ath12k_dp_tx_classify_packet(hw, dp_vif, &info_tx, skb,
							 &is_mcast, &is_eth,
							 &is_data, key, &skb_ctrl);

	if (unlikely(!is_pkt_classified))
		return;

	/* Route to management handler */
	if (!is_data) {
		ath12k_wifi8_mgmt_handler(hw, control, &info_tx, skb);
		return;
	}

	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
	}

	if (is_eth)
		skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;

	/* Setup link for data frame.*/
	ret = ath12k_wifi8_tx_setup_link(vif, sta, &info_tx, skb, &link_id);
	if (ret) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_LINK,
						0, false);
		return;
	}

	/* Get link virtual interface.*/
	arvif = rcu_dereference(ahvif->link[link_id]);
	if (!arvif) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_ARVIF,
						0, false);
		return;
	}

	ar = arvif->ar;
	if (!ar) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_ARVIF,
						0, false);
		return;
	}

	/* Setup SKB control block */
	skb_cb->u.ar = ar;
	skb_cb->link_id = link_id;
	skb_cb->vif = vif;

	/* Get station info if needed. */
	if (sta) {
		is_sta = true;
		ahsta = ath12k_sta_to_ahsta(sta);
		qos_tag = u32_get_bits(skb->mark, QOS_TAG_MASK);
		if (ahsta->use_4addr_set || qos_tag)
			arsta = rcu_dereference(ahsta->link[link_id]);
		dp_peer = ath12k_sta_get_dp_peer_rcu(ahsta);
	}

#ifdef CPTCFG_QCN_EXTN
	/* ULSO: IPA TX accelerator bypass */
	if (ath12k_wifi8_dp_ipa_xmit(arvif->ar, skb, skb_cb,
				     ahvif, ahsta, arvif, is_mcast) == 0)
		return;
#endif

	/* Checking if it is a DVLAN frame */
	if (!test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ah->ag->flags) &&
	    !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	    !(skb_cb->flags & ATH12K_SKB_CIPHER_SET) &&
	    ieee80211_has_protected(hdr->frame_control)) {
		is_dvlan = true;
		skb_ctrl.features |= DP_FEATURE_SW_ENCRPT;
	}

	local_bh_disable();
	/* Route based on multicast/unicast. */
	if (!is_mcast) {
		ath12k_wifi8_ucast_handler(dp_vif, link_id, arsta, skb,
					   &skb_ctrl, qos_nw_delay, vlan_ahvif, dp_peer);
	} else {
		if (!vif->valid_links || is_dvlan || (is_eth && sta) ||
		    test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ah->ag->flags))
			gsn_valid = false;

		ath12k_wifi8_mcbc_handler(dp_vif, link_id, arsta, skb, is_eth,
					  gsn_valid, is_sta, &skb_ctrl, qos_nw_delay,
					  htt_mesh, vlan_ahvif, &info_tx, sta);
		ieee80211_free_txskb(hw, skb);
	}

	local_bh_enable();
}

static void ath12k_wifi8_mac_op_sta_set_4addr(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif,
					      struct ieee80211_sta *sta,
					      bool enabled)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_vlan_iface *vlan_iface = ahvif->vlan_iface;
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);

	if (enabled && !ahsta->use_4addr_set) {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ahsta->ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
		ahsta->vlan_iface = ahvif->vlan_iface;
#endif
		wiphy_work_queue(hw->wiphy, &ahsta->set_4addr_wk);
		ahsta->use_4addr_set = true;
		if (vif->type == NL80211_IFTYPE_AP_VLAN) {
			if (vlan_iface)
				vlan_iface->is_wds_4addr = true;

			ath12k_wifi8_dp_vif_update_4addr(&ah->dp_hw, &ahvif->dp_vif,
							 sta);
		}
	}
}

static int ath12k_wifi8_smd_ensure_tx_tid_queues(struct ath12k_base *ab,
						 struct ath12k_dp *dp,
						 struct ath12k_dp_hw *dp_hw,
						 struct ieee80211_vif *vif,
						 const u8 *peer_addr, u8 tid)
{
	struct ath12k_dp_tx_queue_metadata tx_queue_params = {};
	struct ath12k_dp_peer *dp_peer;
	int ret = 0;

	if (ATH12K_DEFAULT_TID_MAP & BIT(tid))
		return 0;

	/* peer_hash_lock is dropped before ath12k_peer_alloc_dynamic_queue()
	 * since that acquires tx_q_lock internally.  The caller holds wiphy_lock
	 * (asserted in ath12k_wifi8_mac_op_set_smd_ctx), which serialises peer
	 * removal, so dp_peer remains valid across the lock gap.
	 */
	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, peer_addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "SMD DL alloc: peer %pM not found for tid %u\n",
			    peer_addr, tid);
		return -ENOENT;
	}
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	tx_queue_params.tidno = tid;
	tx_queue_params.encap_type = dp_peer->tx_encap_type;

	tx_queue_params.flow_type = HTT_TID_MSDUQ_NONUDP;
	ret = ath12k_peer_alloc_dynamic_queue(dp->dp_hw_grp, dp_peer,
					      &tx_queue_params);
	if (ret) {
		ath12k_err(ab,
			   "SMD DL alloc: failed to alloc non-udp tx queue for peer %pM tid %u err %d\n",
			   peer_addr, tid, ret);
		return ret;
	}

	tx_queue_params.flow_type = HTT_TID_MSDUQ_UDP;
	ret = ath12k_peer_alloc_dynamic_queue(dp->dp_hw_grp, dp_peer,
					      &tx_queue_params);
	if (ret)
		ath12k_err(ab,
			   "SMD DL alloc: failed to alloc udp tx queue for peer %pM tid %u err %d\n",
			   peer_addr, tid, ret);
	return ret;
}

static int ath12k_wifi8_mac_op_set_smd_ctx(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif,
					   struct ieee80211_sta *sta,
					   struct cfg80211_smd_transition_info *st_info)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_dp_hw *dp_hw = &ahvif->ah->dp_hw;
	struct ieee80211_smd_ctx *ctx = st_info->ctx;
	struct ath12k_smd_ctx *drv_ctx __free(kfree) = NULL; /* to parse vendor ctx */
	struct ath12k_rx_smd_ctx_per_tid rx_tid;
	struct ath12k_tx_smd_ctx_per_tid tx_tid;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	int ret;
	u8 tid;

	lockdep_assert_wiphy(hw->wiphy);

	/* fetch default link */
	arvif = ath12k_get_arvif_from_link_id(ahvif, 0);
	ab = arvif->ar->ab;
	dp = ab->dp;

	ath12k_dbg(ab, ATH12K_DBG_SMD, "%s for %pM", __func__, sta->addr);

	drv_ctx = kzalloc(sizeof(*drv_ctx), GFP_ATOMIC);
	if (!drv_ctx)
		return -ENOMEM;

	ath12k_smd_parse_vendor_ctx(ctx, drv_ctx);
	drv_ctx->pn_len = ctx->pn_len;

	/* UL context - Data TIDs */
	for_each_set_bit(tid, ctx->ul.valid_tid_bmap, IEEE80211_SMD_CTX_NUM_TIDS) {
		struct ieee80211_smd_ctx_ba *ul_ba = &ctx->ul.ba[tid];

		memset(&rx_tid, 0, sizeof(rx_tid));
		rx_tid.tid = tid;
		memcpy(rx_tid.peer_addr, sta->addr, ETH_ALEN);
		rx_tid.ssn = ctx->ul.sn[tid];
		rx_tid.pn_len = ctx->pn_len;
		if (rx_tid.pn_len) {
			rx_tid.pn_31_0 = get_unaligned_le32(&ctx->ul.pn[tid][0]);
			rx_tid.pn_47_32 = get_unaligned_le16(&ctx->ul.pn[tid][4]);
		}
		if (rx_tid.pn_len > 6)
			rx_tid.pn_127_48_info = 1;
		rx_tid.ba_win_sz =
			ath12k_smd_ctx_decode_ba_buf_size(ul_ba->buffer_size,
							  ul_ba->ext_buffer_size);
		ath12k_smd_get_vendor_ctx_bitmaps(drv_ctx, &rx_tid);

		ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "UL tid: %u sn: %u pn_len: %u pn: %*ph ba_buf_size: %u",
				 tid, rx_tid.ssn, rx_tid.pn_len,
				 rx_tid.pn_len, ctx->ul.pn[tid], rx_tid.ba_win_sz);

		/* for WMI smd roam config cmd */
		ahsta->smd_info.rx_ba_buf_size[tid] = rx_tid.ba_win_sz;

		ret = ath12k_dp_arch_peer_rx_tid_reo_update_for_smd(dp, dp_hw, vif,
								    sta->addr, &rx_tid);
		if (ret)
			ath12k_err(ab,
				   "Failed to set SMD UL ctx for %pM tid: %d err: %d",
				   sta->addr, tid, ret);
	}

	/* DL context - Data TIDs */
	for_each_set_bit(tid, ctx->dl.valid_tid_bmap, IEEE80211_SMD_CTX_NUM_TIDS) {
		struct ieee80211_smd_ctx_ba *dl_ba = &ctx->dl.ba[tid];
		u16 ba_buf_size;

		memset(&tx_tid, 0, sizeof(tx_tid));
		tx_tid.tid = tid;
		tx_tid.ssn = ctx->dl.sn[tid];
		ath12k_smd_ctx_get_tx_lsn_offset(drv_ctx, &tx_tid);

		memcpy(tx_tid.pn_number, ctx->dl.pn, IEEE80211_SMD_CTX_MAX_PN_LEN);

		ba_buf_size =
			ath12k_smd_ctx_decode_ba_buf_size(dl_ba->buffer_size,
							  dl_ba->ext_buffer_size);
		ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "DL tid: %u sn: %u pn_len: %u pn: %*ph ba_buf_size: %u",
				 tid, tx_tid.ssn, ctx->pn_len,
				 ctx->pn_len, tx_tid.pn_number, ba_buf_size);

		/*
		 * Skip TQM queue setup, TQM SN/PN update and WMI roam config
		 * state for TIDs with no active BA session on the SAP
		 * (ba_buf_size == 0).  There is no queue to seed and sending
		 * zero ba_window_size to firmware for these TIDs is spurious.
		 */
		if (!ba_buf_size)
			continue;

		ret = ath12k_wifi8_smd_ensure_tx_tid_queues(ab, dp, dp_hw, vif,
							    sta->addr, tid);
		if (ret) {
			ath12k_err(ab,
				   "Failed to ensure TX queues for SMD DL ctx peer %pM tid: %d err: %d",
				   sta->addr, tid, ret);
			continue;
		}

		ret = ath12k_dp_arch_peer_tx_tid_update_for_smd(dp, dp_hw, vif,
								sta->addr, &tx_tid);
		if (ret) {
			ath12k_err(ab,
				   "Failed to set SMD DL ctx for %pM tid: %d err: %d",
				   sta->addr, tid, ret);
			continue;
		}

		/* for WMI smd roam config cmd — written only after TQM is
		 * seeded so firmware receives state that reflects what TQM
		 * was actually programmed with.
		 */
		ahsta->smd_info.sn[tid] = tx_tid.ssn + tx_tid.lsn_offset;
		ahsta->smd_info.lsn_offset[tid] = tx_tid.lsn_offset;
		ahsta->smd_info.tx_ba_buf_size[tid] = ba_buf_size;
	}

	/* Vendor context */
	if (drv_ctx->vendor_ctx.version != SMD_CTX_VENDOR_INVALID_VERSION) {
		ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "SMD vendor context in set_ctx: %*ph",
				 (int)ctx->drv_ctx_size, ctx->drv_ctx);
		ret = ath12k_smd_set_vendor_ctx(dp, dp_hw, vif, drv_ctx, sta);
		if (ret)
			ath12k_err(ab,
				   "Failed to set SMD vendor ctx for %pM, err: %d",
				   sta->addr, ret);
	}

	return 0;
}

static int ath12k_wifi8_mac_op_get_smd_ctx(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif,
					   struct ieee80211_sta *sta,
					   struct cfg80211_smd_transition_info *st_info)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_smd_info *smd_info = &ahsta->smd_info;
	struct ieee80211_smd_ctx *ctx = st_info->ctx;
	struct ath12k_smd_ctx_req *req = NULL;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = ath12k_get_arvif_from_link_id(ahvif, ahvif->deflink.link_id);
	if (!arvif)
		return -EINVAL;

	ab = arvif->ar->ab;

	if (!sta->smd_params.smd_enabled) {
		ath12k_err(ab, "Cannot fetch SMD context from a non-SMD STA %pM",
			   sta->addr);
		return -EINVAL;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	/* Set common context validity bits */
	set_bit(ATH12K_SMD_CTX_VALID_PN, req->ctx.valid_ctx_bmap);
	set_bit(ATH12K_SMD_CTX_VALID_BA_PARAMS, req->ctx.valid_ctx_bmap);

	if (test_bit(IEEE80211_SMD_CTX_VALID_DL_SN, ctx->valid_ctx_bmap))
		set_bit(ATH12K_SMD_CTX_VALID_DL_SN, req->ctx.valid_ctx_bmap);
	if (test_bit(IEEE80211_SMD_CTX_VALID_UL_SN, ctx->valid_ctx_bmap))
		set_bit(ATH12K_SMD_CTX_VALID_UL_SN, req->ctx.valid_ctx_bmap);

	spin_lock_init(&req->lock);
	req->mmpdu = NULL;
	req->state = SMD_CTX_INIT;
	req->type = (u8)st_info->type;
	req->vif = vif;
	req->handler = ath12k_smd_update_ctx_for_user;
	req->enqueued_ts = ktime_get();
	memcpy(req->sta_addr, sta->addr, ETH_ALEN);
	bitmap_copy(req->ctx.dl.valid_tid_bmap, ctx->dl.valid_tid_bmap,
		    IEEE80211_MAX_NUM_TIDS);
	bitmap_copy(req->ctx.ul.valid_tid_bmap, ctx->ul.valid_tid_bmap,
		    IEEE80211_MAX_NUM_TIDS);
	req->exec_via_target = true;

	spin_lock_bh(&smd_info->smd_lock);

	if (st_info->type == CFG80211_ST_TYPE_PREP &&
	    !ath12k_smd_reuse_sta_session_prep_ctx(smd_info, req)) {
		ath12k_dbg(ab, ATH12K_DBG_SMD, "Using cached ST Prep context for %pM",
			   sta->addr);
		spin_unlock_bh(&smd_info->smd_lock);
		/* synchronous cache-hit: fill caller's ctx and return 0 */
		ath12k_smd_ctx_to_ieee80211_ctx(&req->ctx, ctx);
		kfree(req);
		return 0;
	}

	if (!smd_info->ctx_inflight) {
		int ret;

		rcu_read_lock();
		arvif = rcu_dereference(ahsta->ahvif->link[ahsta->assoc_link_id]);
		if (!arvif || !arvif->ar) {
			rcu_read_unlock();
			spin_unlock_bh(&smd_info->smd_lock);
			kfree(req);
			return -ENOLINK;
		}

		ret = ath12k_smd_post_sta_session_ctx_req(ahsta->ahvif, arvif, req,
							  smd_info);
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "Posted user ctx request (0x%*pb) for %pM (ret: %d)",
			   ATH12K_SMD_CTX_NUM_VALID_CTX, req->ctx.valid_ctx_bmap,
			   sta->addr, ret);
		rcu_read_unlock();

		if (ret) {
			spin_unlock_bh(&smd_info->smd_lock);
			kfree(req);
			return ret;
		}
	} else {
		list_add_tail(&req->list, &smd_info->ctx_list);
		ath12k_smd_ctx_queue_work(&smd_info->ctx_wk);
	}

	spin_unlock_bh(&smd_info->smd_lock);

	return -EINPROGRESS;
}

static const struct ieee80211_ops ath12k_ops_wifi8 = {
	.tx				= ath12k_wifi8_mac_op_tx,
	.wake_tx_queue			= ieee80211_handle_wake_tx_queue,
	.start                          = ath12k_mac_op_start,
	.stop                           = ath12k_mac_op_stop,
	.reconfig_complete              = ath12k_mac_op_reconfig_complete,
	.add_interface                  = ath12k_mac_op_add_interface,
	.remove_interface		= ath12k_mac_op_remove_interface,
	.update_vif_offload		= ath12k_mac_op_update_vif_offload,
	.config                         = ath12k_mac_op_config,
	.sta_set_4addr			= ath12k_wifi8_mac_op_sta_set_4addr,
	.link_info_changed              = ath12k_mac_op_link_info_changed,
	.start_ap                       = ath12k_mac_op_start_ap,
	.link_going_down                = ath12k_mac_op_link_going_down,
	.vif_cfg_changed		= ath12k_mac_op_vif_cfg_changed,
	.change_vif_links               = ath12k_mac_op_change_vif_links,
	.configure_filter		= ath12k_mac_op_configure_filter,
	.hw_scan                        = ath12k_mac_op_hw_scan,
	.cancel_hw_scan                 = ath12k_mac_op_cancel_hw_scan,
	.set_key                        = ath12k_mac_op_set_key,
	.set_rekey_data	                = ath12k_mac_op_set_rekey_data,
	.sta_state                      = ath12k_mac_op_sta_state,
	.sta_set_mapc_params            = ath12k_mac_op_sta_set_mapc_params,
	.sta_set_txpwr			= ath12k_mac_op_sta_set_txpwr,
	.link_sta_rc_update		= ath12k_mac_op_link_sta_rc_update,
	.conf_tx                        = ath12k_mac_op_conf_tx,
	.set_antenna			= ath12k_mac_op_set_antenna,
	.get_antenna			= ath12k_mac_op_get_antenna,
	.ampdu_action			= ath12k_mac_op_ampdu_action,
	.add_chanctx			= ath12k_mac_op_add_chanctx,
	.remove_chanctx			= ath12k_mac_op_remove_chanctx,
	.change_chanctx			= ath12k_mac_op_change_chanctx,
	.assign_vif_chanctx		= ath12k_mac_op_assign_vif_chanctx,
	.unassign_vif_chanctx		= ath12k_mac_op_unassign_vif_chanctx,
	.switch_vif_chanctx		= ath12k_mac_op_switch_vif_chanctx,
	.get_txpower			= ath12k_mac_op_get_txpower,
	.set_rts_threshold		= ath12k_mac_op_set_rts_threshold,
	.set_frag_threshold		= ath12k_mac_op_set_frag_threshold,
	.set_bitrate_mask		= ath12k_mac_op_set_bitrate_mask,
	.get_survey			= ath12k_mac_op_get_survey,
	.flush				= ath12k_mac_op_flush,
	.set_preserved_link_stats	= ath12k_mac_op_preserved_link_stats,
	.sta_statistics			= ath12k_mac_op_sta_statistics,
	.link_sta_statistics		= ath12k_mac_op_link_sta_statistics,
	.remain_on_channel              = ath12k_mac_op_remain_on_channel,
	.cancel_remain_on_channel       = ath12k_mac_op_cancel_remain_on_channel,
	.change_sta_links               = ath12k_mac_op_change_sta_links,
	.can_activate_links             = ath12k_mac_op_can_activate_links,
	.set_dscp_tid                   = ath12k_mac_op_set_dscp_tid,
	.uhr_link_reconfig              = ath12k_mac_op_uhr_link_reconfig,
	.uhr_smd_update			= ath12k_mac_op_uhr_smd_update,
#ifdef CONFIG_PM
	.suspend			= ath12k_wow_op_suspend,
	.resume				= ath12k_wow_op_resume,
	.set_wakeup			= ath12k_wow_op_set_wakeup,
#endif
#ifdef CPTCFG_ATH12K_DEBUGFS
	.vif_add_debugfs                = ath12k_debugfs_op_vif_add,
#endif
	CFG80211_TESTMODE_CMD(ath12k_tm_cmd)
#ifdef CPTCFG_ATH12K_DEBUGFS
	.sta_add_debugfs                = ath12k_debugfs_sta_op_add,
	.link_sta_add_debugfs           = ath12k_debugfs_link_sta_op_add,
#endif
	.link_reconfig_remove           = ath12k_mac_op_link_reconfig_remove,
	.removed_link_is_primary        = ath12k_mac_op_removed_link_is_primary,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.change_mtu			= ath12k_mac_op_set_mtu,
	.init_interface			= ath12k_mac_op_create_datapath_offload_if,
	.deinit_interface		= ath12k_mac_op_destroy_datapath_offload_if,
#endif
	.can_neg_ttlm			= ath12k_mac_op_can_neg_ttlm,
	.apply_neg_ttlm_per_client	= ath12k_mac_op_apply_neg_ttlm_per_client,
	.set_radar_background           = ath12k_mac_op_set_radar_background,
	.abort_radar_background		= ath12k_mac_op_abort_radar_background,
	.erp                            = ath12k_mac_op_erp,
	.qos_mgmt_cfg                   = ath12k_mac_op_qos_mgmt_cfg,
	.get_netstats                   = ath12k_mac_op_get_netstats,
	.get_afc_eirp_pwr               = ath12k_mac_op_get_afc_eirp_pwr,
	.get_6ghz_dev_deployment_type	= ath12k_mac_op_get_6ghz_dev_deployment_type,
	.ap_power_save                  = ath12k_mac_op_ap_power_save,
	.set_monitor_flags		= ath12k_mac_op_set_monitor_flags,
	.uhr_mode_update		= ath12k_mac_op_sta_uhr_mode_update,
	.critical_update		= ath12k_mac_op_critical_update,
	.set_smd_ctx			= ath12k_wifi8_mac_op_set_smd_ctx,
	.get_smd_ctx			= ath12k_wifi8_mac_op_get_smd_ctx,
#ifdef CPTCFG_QCN_EXTN
	.set_muedca_mode		= ath12k_mac_set_muedca_mode,
#endif /* CPTCFG_QCN_EXTN */
};

/*
 * ath12k_mem_profile_based_param_wifi8 - Per-profile runtime memory parameters
 * for WiFi8.
 */
static const struct ath12k_mem_profile_based_param
ath12k_mem_profile_based_param_wifi8[] = {
	[ATH12K_MEM_PROFILE_DEFAULT] = {
		.num_vdevs			= 16 + 1,
		.num_bridge_vdevs		= 8,
		.num_max_vdevs_nlink		= (16 + 1) + 8,
		.target_mem_mode		= ATH12K_QMI_TARGET_MEM_MODE_DEFAULT,
		.num_stations_single		= 0,
		.num_stations_dbs		= 0,
		.num_stations_dbs_sbs		= 0,
		.monitor_support		= true,
		.cfr_support			= true,
		.spectral_support		= true,
		.tx_monitor_support		= true,
		.dp_params = {
			.rxdma_buf_ring_size		= 8192,
			.rx_release_ring_size		= 16384,
			.reo2ppe_ring			= 4096,
			.ppe2tcl_ring			= 8192,
			.tqm2ppe_ring_size		= 8192,
			.tx_comp_ppeds_ring_size	= 32768,
			.ppe_wbm2sw_ring_size		= 32768,
			.rxdma_monitor_buf_ring_size	= 4096,
			.rxdma_monitor_dst_ring_size	= 1024,
			.smart_mon_filter_default	= DP_SMART_MON_PROFILE_1G,
			.mon_num_ppdu_desc		= 128,
			.rx_desc_count			= 24576,
			.dp_max_clients			= 512,
			.num_pool_ppeds_tx_desc		= 0x8000,
			.ppeds_hotlist_len_max		= 1024,
			.dp_num_clients_max		= 64,
			.dp_mon_status_buf		= 320,
			.tx_monitor_buf_ring_size	= 8192,
			.tx_monitor_buf_ring_fill_lvl	= 8192,
			.tx_monitor_dst_ring_size	= 8192,
			.tx_monitor_num_ppdu_desc	= 128,
			.tx_monitor_num_status_buf	= 320,
			.reo_dst_ring_size		= { 8192, 8192, 8192,
							    2048, 2048 },
			.tcl_data_ring_size		= { 2048, 2048, 2048,
							    512, 512 },
			.tx_compl_ring_size		= { 16384, 16384, 16384,
							    2048, 2048 },
			.tx_desc_count			= { 16384, 16384, 16384,
							    2048, 2048 },
		},
	},
	[ATH12K_MEM_PROFILE_BALANCED] = {
		.num_vdevs			= 16 + 1,
		.num_bridge_vdevs		= 8,
		.num_max_vdevs_nlink		= (16 + 1) + 8,
		.target_mem_mode		= ATH12K_QMI_TARGET_MEM_MODE_DEFAULT,
		.num_stations_single		= 0,
		.num_stations_dbs		= 0,
		.num_stations_dbs_sbs		= 0,
		.monitor_support		= true,
		.cfr_support			= true,
		.spectral_support		= true,
		.tx_monitor_support		= true,
		.dp_params = {
			.rxdma_buf_ring_size		= 8192,
			.rx_release_ring_size		= 8192,
			.reo2ppe_ring			= 2048,
			.ppe2tcl_ring			= 2048,
			.tqm2ppe_ring_size		= 8192,
			.tx_comp_ppeds_ring_size	= 16384,
			.ppe_wbm2sw_ring_size		= 8192,
			.rxdma_monitor_buf_ring_size	= 256,
			.rxdma_monitor_dst_ring_size	= 512,
			.smart_mon_filter_default	= DP_SMART_MON_PROFILE_1G,
			.mon_num_ppdu_desc		= 128,
			.rx_desc_count			= 16384,
			.dp_max_clients			= 512,
			.num_pool_ppeds_tx_desc		= 0x8000,
			.ppeds_hotlist_len_max		= 1024,
			.dp_num_clients_max		= 64,
			.dp_mon_status_buf		= 320,
			.tx_monitor_buf_ring_size	= 256,
			.tx_monitor_buf_ring_fill_lvl	= 256,
			.tx_monitor_dst_ring_size	= 256,
			.tx_monitor_num_ppdu_desc	= 8,
			.tx_monitor_num_status_buf	= 320,
			.reo_dst_ring_size		= { 8192, 8192, 8192,
							    1024, 1024 },
			.tcl_data_ring_size		= { 2048, 2048, 2048,
							    512, 512 },
			.tx_compl_ring_size		= { 16384, 16384, 16384,
							    2048, 2048 },
			.tx_desc_count			= { 16384, 16384, 16384,
							    2048, 2048 },
		},
	},
	[ATH12K_MEM_PROFILE_OPTIMIZED] = {
		.num_vdevs			= 8 + 1,
		.num_bridge_vdevs		= 0,
		.num_max_vdevs_nlink		= 0,
		.target_mem_mode		= ATH12K_QMI_TARGET_MEM_MODE_256M,
		.num_stations_single		= 128,
		.num_stations_dbs		= 64,
		.num_stations_dbs_sbs		= 42,
		.monitor_support		= true,
		.cfr_support			= false,
		.spectral_support		= false,
		.tx_monitor_support		= false,
		.dp_params = {
			.rxdma_buf_ring_size		= 2048,
			.rx_release_ring_size		= 4096,
			.reo2ppe_ring			= 2048,
			.ppe2tcl_ring			= 2048,
			.tqm2ppe_ring_size		= 8192,
			.tx_comp_ppeds_ring_size	= 8192,
			.ppe_wbm2sw_ring_size		= 8192,
			.rxdma_monitor_buf_ring_size	= 256,
			.rxdma_monitor_dst_ring_size	= 512,
			.smart_mon_filter_default	= DP_SMART_MON_PROFILE_256M |
							  (DP_SMART_MON_FILTER_MASK &
							   ~DP_SMART_MON_VALID),
			.mon_num_ppdu_desc		= 8,
			.rx_desc_count			= 8192,
			.dp_max_clients			= 512,
			.num_pool_ppeds_tx_desc		= 0x4000,
			.ppeds_hotlist_len_max		= 256,
			.dp_num_clients_max		= 56,
			.dp_mon_status_buf		= 20,
			.tx_monitor_buf_ring_size	= 256,
			.tx_monitor_buf_ring_fill_lvl	= 256,
			.tx_monitor_dst_ring_size	= 256,
			.tx_monitor_num_ppdu_desc	= 8,
			.tx_monitor_num_status_buf	= 32,
			.reo_dst_ring_size		= { 2048, 2048, 2048, 512, 512 },
			.tcl_data_ring_size		= { 512, 512, 512, 128, 128 },
			.tx_compl_ring_size		= { 8192, 8192, 8192,
							    1024, 1024 },
			.tx_desc_count			= { 8192, 8192, 8192,
							    1024, 1024 },
		},
	},
};

int ath12k_wifi8_hw_init(struct ath12k_base *ab)
{
	struct ath12k_hw_params *hw_params = NULL;
	int i;

	/*
	 * Set per-device for WiFi8 from the
	 * wifi8-specific profile table.  Also update ath12k_max_clients
	 */
	ab->mem_params = ath12k_mem_profile_based_param_wifi8[ath12k_active_mem_profile];
	ath12k_max_clients = ab->mem_params.dp_params.dp_max_clients;

	for (i = 0; i < ARRAY_SIZE(ath12k_wifi8_hw_params); i++) {
		hw_params = &ath12k_wifi8_hw_params[i];

		if (hw_params->hw_rev == ab->hw_rev)
			break;
	}

	if (i == ARRAY_SIZE(ath12k_wifi8_hw_params)) {
		ath12k_err(ab, "Unsupported WiFi8 hardware version: 0x%x\n",
			   ab->hw_rev);
		return -EINVAL;
	}

	ab->hw_params = hw_params;
	ab->ath12k_ops = &ath12k_ops_wifi8;
	ab->map_event_required = false;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ath12k_ftm_mode && (ath12k_ppe_ds_enabled || ath12k_ppe_ds_wifi8_enabled)) {
		ath12k_info(ab,
			    "WiFi8 FTM mode enabled, disabling ppe_ds_enable\n");
		ath12k_ppe_ds_enabled = 0;
		ath12k_ppe_ds_wifi8_enabled = 0;
	}
#endif

	ath12k_info(ab, "WiFi8 Hardware name: %s\n", ab->hw_params->name);

#ifdef CPTCFG_QCN_EXTN
	ath12k_wifi8_hw_init_extn(ab);
#endif

	return 0;
}
