// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#include "../../debug.h"
#include "../../core.h"
#include "../../ce.h"
#include "ce.h"
#include "../../hw.h"
#include "hw.h"
#include "../../qcn_extns/ath12k_cmn_extn.h"
#include "../../mhi.h"
#include "mhi.h"
#include "dp_rx.h"
#include "wmi.h"
#include "../../wow.h"
#include "../../debugfs_sta.h"
#include "../../debugfs.h"
#include "../../testmode.h"
#include "../../dp_peer.h"
#include "../../dp_tx.h"
#include "hal_qcn9074.h"
#include "hal_qcn9160.h"
#include "../../cfr.h"
#include "../../dp_stats.h"
#include "../../dp_mon.h"


static u8 ath12k_wifi6_hw_qcn9074_mac_from_pdev_id(int pdev_idx)
{
	return pdev_idx;
}

static int
ath12k_wifi6_hw_mac_id_to_pdev_id_qcn9074(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static int
ath12k_wifi6_hw_mac_id_to_srng_id_qcn9074(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static u8 ath12k_wifi6_hw_get_ring_selector_qcn9074(struct sk_buff *skb)
{
	return smp_processor_id();
}

static bool ath12k_wifi6_dp_srng_is_comp_ring_qcn9074(int ring_num)
{
	if (ring_num < 3 || ring_num == 4)
		return true;

	return false;
}

void ath12k_hw_qcn9074_fill_cfr_hdr_info(struct ath12k *ar,
					 struct ath12k_csi_cfr_header *header,
					 struct ath12k_cfr_peer_tx_param *params)
{
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->cfr_metadata_len = sizeof(struct cfr_enh_metadata);
	header->host_real_ts = ktime_to_ns(ktime_get_real());

	header->cfr_metadata_version = ATH12K_CFR_META_VERSION_11;
	if (ar->ab->hw_rev == ATH12K_HW_QCN6432_HW10)
		header->chip_type = ATH12K_CFR_RADIO_QCN6432;
	else if (ar->ab->hw_rev == ATH12K_HW_IPQ5424_HW10)
		header->chip_type = ATH12K_CFR_RADIO_IPQ5424;
	else if (ar->ab->hw_rev == ATH12K_HW_IPQ5332_HW10)
		header->chip_type = ATH12K_CFR_RADIO_IPQ5332;
	else
		header->chip_type = ATH12K_CFR_RADIO_QCN9274;

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

static const struct ath12k_hw_ops qcn9074_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi6_hw_qcn9074_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi6_hw_mac_id_to_pdev_id_qcn9074,
	.mac_id_to_srng_id = ath12k_wifi6_hw_mac_id_to_srng_id_qcn9074,
	.rxdma_ring_sel_config = ath12k_wifi6_dp_rxdma_ring_sel_config_qcn9074,
	.get_ring_selector = ath12k_wifi6_hw_get_ring_selector_qcn9074,
	.dp_srng_is_tx_comp_ring = ath12k_wifi6_dp_srng_is_comp_ring_qcn9074,
	.fill_cfr_hdr_info = ath12k_hw_qcn9074_fill_cfr_hdr_info,
	.parse_cfr_enh_dma_hdr = ath12k_cfr_parse_enh_dma_hdr,
};

static const struct ath12k_hw_ops qcn9160_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi6_hw_qcn9074_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi6_hw_mac_id_to_pdev_id_qcn9074,
	.mac_id_to_srng_id = ath12k_wifi6_hw_mac_id_to_srng_id_qcn9074,
	.rxdma_ring_sel_config = ath12k_wifi6_dp_rxdma_ring_sel_config_qcn9160,
	.get_ring_selector = ath12k_wifi6_hw_get_ring_selector_qcn9074,
	.dp_srng_is_tx_comp_ring = ath12k_wifi6_dp_srng_is_comp_ring_qcn9074,
	.parse_cfr_enh_dma_hdr = ath12k_cfr_parse_enh_dma_hdr,
};

#define ATH12K_TX_RING_MASK_0 0x1
#define ATH12K_TX_RING_MASK_1 0x2
#define ATH12K_TX_RING_MASK_2 0x4
#define ATH12K_TX_RING_MASK_3 0x8
#define ATH12K_TX_RING_MASK_4 0x10

#define ATH12K_RX_RING_MASK_0 0x1
#define ATH12K_RX_RING_MASK_1 0x2
#define ATH12K_RX_RING_MASK_2 0x4
#define ATH12K_RX_RING_MASK_3 0x8

#define ATH12K_RX_ERR_RING_MASK_0 0x1

#define ATH12K_RX_WBM_REL_RING_MASK_0 0x1

#define ATH12K_REO_STATUS_RING_MASK_0 0x1

#define ATH12K_HOST2RXDMA_RING_MASK_0 0x1
#define ATH12K_HOST2RXDMA_RING_MASK_1 0x2
#define ATH12K_HOST2RXDMA_RING_MASK_2 0x4

#define	ATH12K_HOST2RXMON_RING_MASK_0	0x1

#define ATH12K_RX_MON_RING_MASK_0 0x1
#define ATH12K_RX_MON_RING_MASK_1 0x2
#define ATH12K_RX_MON_RING_MASK_2 0x4

#define ATH12K_TX_MON_RING_MASK_0 0x1
#define ATH12K_TX_MON_RING_MASK_1 0x2
#define ATH12K_UMAC_RESET_INTR_MASK_0   0x1

#define ATH12K_PPE2TCL_RING_MASK_0 0x1
#define ATH12K_REO2PPE_RING_MASK_0 0x1
#define ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0 0x1

#define ATH12K_RX_MON_STATUS_RING_MASK_0 0x1
#define ATH12K_RX_MON_STATUS_RING_MASK_1 0x2
#define ATH12K_RX_MON_STATUS_RING_MASK_2 0x4
#define ATH12K_RX_MON_DEST_RING_MASK_0 0x1
#define ATH12K_RX_MON_DEST_RING_MASK_1 0x2
#define ATH12K_RX_MON_DEST_RING_MASK_2 0x4

/* To support 8 MSI DP grouping */
static struct ath12k_hw_ring_mask ath12k_wifi6_hw_ring_mask_qcn9074_msi8 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2 | ATH12K_TX_RING_MASK_3,
		0, 0, 0, 0, 0
	},
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	.rx_mon_status = {
		0, 0, 0,
		ATH12K_RX_MON_STATUS_RING_MASK_0,
		ATH12K_RX_MON_STATUS_RING_MASK_1,
		ATH12K_RX_MON_STATUS_RING_MASK_2,
		0, 0,
	},
	.rx_mon_dest = {
		0, 0, 0,
		ATH12K_RX_MON_DEST_RING_MASK_0,
		ATH12K_RX_MON_DEST_RING_MASK_1,
		ATH12K_RX_MON_DEST_RING_MASK_2,
		0, 0,
	},
	.rx = {
		0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2 | ATH12K_RX_RING_MASK_3,
		0, 0
	},
	.rx_err = {
		0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
		0, 0, 0, 0
	},
	.rx_wbm_rel = {
		0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
		0, 0, 0, 0
	},
	.reo_status = {
		0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
		0, 0, 0, 0
	},
	.host2rxdma = {
		0, 0,
		ATH12K_HOST2RXDMA_RING_MASK_0,
		0, 0, 0, 0
	},
	.tx_mon_dest = {
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
		0, 0, 0, 0, 0, 0
	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0,
		0
	},
	.reo2ppe = {
		0, 0, 0, 0, 0,
		ATH12K_REO2PPE_RING_MASK_0,
		0, 0
	},
	.ppeds_tx_cmpln = {
		ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0
	},
#endif
	.umac_dp_reset = {
		0, 0, 0, 0, 0, 0, 0,
		ATH12K_UMAC_RESET_INTR_MASK_0
	},
};

static struct ath12k_hw_ring_mask ath12k_wifi6_hw_ring_mask_qcn9074 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
	},
	.rx_mon_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_RX_MON_STATUS_RING_MASK_0,
		ATH12K_RX_MON_STATUS_RING_MASK_1,
		ATH12K_RX_MON_STATUS_RING_MASK_2,
		0,
	},
	.rx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_RX_MON_DEST_RING_MASK_0,
		ATH12K_RX_MON_DEST_RING_MASK_1,
		ATH12K_RX_MON_DEST_RING_MASK_2,
		0,
	},
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
	},
	.rx_err = {
		0, 0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		0, 0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
	},
	.host2rxdma = {
		0, 0, 0,
		ATH12K_HOST2RXDMA_RING_MASK_0,
	},
};

static struct ath12k_hw_ring_mask ath12k_wifi6_hw_ring_mask_qcn9160 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		0,
		ATH12K_TX_RING_MASK_2,
		0,
		ATH12K_TX_RING_MASK_4,
	},
	.rx_mon_status = {
		0, 0, 0,
		ATH12K_RX_MON_STATUS_RING_MASK_0,
	},
	.rx = {
		0, 0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
	},
	.rx_err = {
		0, ATH12K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		0, ATH12K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		0, ATH12K_REO_STATUS_RING_MASK_0,
	},

	.rx_mon_dest = {
	},

	.host2rxdma = {
	},
};

static struct ath12k_hw_params ath12k_wifi6_hw_params[] = {
	{
		.name = "qcn9074 hw1.0",
		.hw_rev = ATH12K_HW_QCN9074_HW10,
		.fw = {
			.dir = "QCN9074/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9074,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9074_ops,
		.ring_mask = &ath12k_wifi6_hw_ring_mask_qcn9074,

		.host_ce_config = ath12k_wifi6_host_ce_config_qcn9074,
		.ce_count = 6,
		.target_ce_config = ath12k_wifi6_target_ce_config_wlan_qcn9074,
		.target_ce_count = 9,
		.svc_to_ce_map =
			ath12k_wifi6_target_service_to_ce_map_wlan_qcn9074,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
		.supports_monitor = true,

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = false,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.mhi_config = &ath12k_wifi6_mhi_config_qcn9074,

		.wmi_init = ath12k_wifi6_wmi_init_qcn9074,

		.hal_ops = &hal_qcn9074_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0x600000,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = QCN9274_QFPROM_RAW_RFA_PDET_ROW13_LSB,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = false,

		.iova_mask = 0,

		.supports_aspm = false,

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = false,
		.support_fse = false,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_bin_sz = 2,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 1024,
			.fft_size_min = ATH12K_SPECTRAL_FFT_SIZE_MIN,
			.fft_size_max = {
				[ATH12K_SPECTRAL_BW_20MHZ]  = 9,
				[ATH12K_SPECTRAL_BW_40MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_80MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_160MHZ] = 10,
				[ATH12K_SPECTRAL_BW_320MHZ] = 0,
			},
			.fragment_160mhz = false,
			.supports_320mhz = false,
		},
		.supports_ap_ps = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = false,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = false,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 255,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = 16716,
		.mlo_3_link_tx_support = false,
		.quad_ring_monitor_support = true,
		.board_magic = "QCA-ATH11K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
	},
	{
		.name = "qcn9160 hw1.0",
		.hw_rev = ATH12K_HW_QCN9160_HW10,
		.fw = {
			.dir = "QCN9160/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9160,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9160_ops,
		.ring_mask = &ath12k_wifi6_hw_ring_mask_qcn9160,

		.host_ce_config = ath12k_wifi6_host_ce_config_qcn9074,
		.ce_count = 6,
		.target_ce_config = ath12k_wifi6_target_ce_config_wlan_qcn9074,
		.target_ce_count = 9,
		.svc_to_ce_map =
			ath12k_wifi6_target_service_to_ce_map_wlan_qcn9074,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = false,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 1,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
		.supports_monitor = true,

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = false,
		.supports_shadow_regs = false,
		.num_tcl_banks = 48,
		.max_tx_ring = 3,

		.mhi_config = &ath12k_wifi6_mhi_config_qcn9074,

		.wmi_init = ath12k_wifi6_wmi_init_qcn9074,

		.hal_ops = &hal_qcn9160_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0x600000,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = QCN9274_QFPROM_RAW_RFA_PDET_ROW13_LSB,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = false,

		.iova_mask = 0,

		.supports_aspm = false,

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = false,
		.support_fse = false,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_bin_sz = 2,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 1024,
			.fft_size_min = ATH12K_SPECTRAL_FFT_SIZE_MIN,
			.fft_size_max = {
				[ATH12K_SPECTRAL_BW_20MHZ]  = 9,
				[ATH12K_SPECTRAL_BW_40MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_80MHZ]  = 10,
				[ATH12K_SPECTRAL_BW_160MHZ] = 10,
				[ATH12K_SPECTRAL_BW_320MHZ] = 0,
			},
			.fragment_160mhz = false,
			.supports_320mhz = false,
		},
		.supports_ap_ps = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = false,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 255,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = 16716,
		.mlo_3_link_tx_support = false,
		.quad_ring_monitor_support = true,
		.board_magic = "QCA-ATH11K-BOARD",
		.ext_irq_grp_num_max = 9,
	},
};

/* Note: called under rcu_read_lock() */
static void ath12k_wifi6_mac_op_tx(struct ieee80211_hw *hw,
				   struct ieee80211_tx_control *control,
				   struct sk_buff *skb)
{
}

static const struct ieee80211_ops ath12k_ops_wifi6 = {
	.tx				= ath12k_wifi6_mac_op_tx,
	.wake_tx_queue			= ieee80211_handle_wake_tx_queue,
	.start                          = ath12k_mac_op_start,
	.stop                           = ath12k_mac_op_stop,
	.reconfig_complete              = ath12k_mac_op_reconfig_complete,
	.add_interface                  = ath12k_mac_op_add_interface,
	.remove_interface		= ath12k_mac_op_remove_interface,
	.update_vif_offload		= ath12k_mac_op_update_vif_offload,
	.config                         = ath12k_mac_op_config,
	.link_info_changed              = ath12k_mac_op_link_info_changed,
	.vif_cfg_changed		= ath12k_mac_op_vif_cfg_changed,
	.change_vif_links               = ath12k_mac_op_change_vif_links,
	.configure_filter		= ath12k_mac_op_configure_filter,
	.hw_scan                        = ath12k_mac_op_hw_scan,
	.cancel_hw_scan                 = ath12k_mac_op_cancel_hw_scan,
	.set_key                        = ath12k_mac_op_set_key,
	.set_rekey_data	                = ath12k_mac_op_set_rekey_data,
	.sta_state                      = ath12k_mac_op_sta_state,
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
	.sta_statistics			= ath12k_mac_op_sta_statistics,
	.link_sta_statistics		= ath12k_mac_op_link_sta_statistics,
	.remain_on_channel              = ath12k_mac_op_remain_on_channel,
	.cancel_remain_on_channel       = ath12k_mac_op_cancel_remain_on_channel,
	.change_sta_links               = ath12k_mac_op_change_sta_links,
	.can_activate_links             = ath12k_mac_op_can_activate_links,
	.set_dscp_tid                   = ath12k_mac_op_set_dscp_tid,
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
#endif
	.can_neg_ttlm			= ath12k_mac_op_can_neg_ttlm,
	.apply_neg_ttlm_per_client	= ath12k_mac_op_apply_neg_ttlm_per_client,
	.set_radar_background           = ath12k_mac_op_set_radar_background,
	.erp                            = ath12k_mac_op_erp,
	.qos_mgmt_cfg                   = ath12k_mac_op_qos_mgmt_cfg,
	.get_afc_eirp_pwr               = ath12k_mac_op_get_afc_eirp_pwr,
	.get_6ghz_dev_deployment_type	= ath12k_mac_op_get_6ghz_dev_deployment_type,
};

/*
 * ath12k_mem_profile_based_param_wifi6 - Per-profile runtime memory parameters
 * for WiFi6.
 */
static const struct ath12k_mem_profile_based_param
ath12k_mem_profile_based_param_wifi6[] = {
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
			.reo2ppe_ring			= 16384,
			.ppe2tcl_ring			= 8192,
			.tqm2ppe_ring_size		= 32768,
			.tx_comp_ppeds_ring_size	= 32768,
			.ppe_wbm2sw_ring_size		= 32768,
			.rxdma_monitor_buf_ring_size	= 8192,
			.rxdma_monitor_dst_ring_size	= 8192,
			.smart_mon_filter_default	= DP_SMART_MON_PROFILE_1G,
			.mon_num_ppdu_desc		= 128,
			.rx_desc_count			= 12288,
			.dp_max_clients			= 512,
			.num_pool_ppeds_tx_desc		= 0x8000,
			.ppeds_hotlist_len_max		= 1024,
			.dp_num_clients_max		= 64,
			.dp_mon_status_buf		= 320,
			.reo_dst_ring_size		= { 8192, 8192, 8192,
							    8192, 8192 },
			.tcl_data_ring_size		= { 2048, 2048, 2048,
							    2048, 2048 },
			.tx_compl_ring_size		= { 32768, 32768, 32768,
							    8192, 8192 },
			.tx_desc_count                  = { 32768, 32768, 32768,
							    8192, 8192 },
		},
	},
	[ATH12K_MEM_PROFILE_BALANCED] = {
		.num_vdevs			= 8 + 1,
		.num_bridge_vdevs		= 0,
		.num_max_vdevs_nlink		= 0,
		.target_mem_mode		= ATH12K_QMI_TARGET_MEM_MODE_512M,
		.num_stations_single		= 128,
		.num_stations_dbs		= 64,
		.num_stations_dbs_sbs		= 42,
		.monitor_support		= true,
		.cfr_support			= true,
		.spectral_support		= true,
		.tx_monitor_support		= false,
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
			.smart_mon_filter_default	= DP_SMART_MON_PROFILE_512M |
							  (DP_SMART_MON_FILTER_MASK &
							   ~DP_SMART_MON_VALID),
			.mon_num_ppdu_desc		= 128,
			.rx_desc_count			= 8192,
			.dp_max_clients			= 512,
			.num_pool_ppeds_tx_desc		= 0x8000,
			.ppeds_hotlist_len_max		= 1024,
			.dp_num_clients_max		= 64,
			.dp_mon_status_buf		= 320,
			.reo_dst_ring_size		= { 8192, 8192, 8192,
							    8192, 8192 },
			.tcl_data_ring_size		= { 2048, 2048, 2048,
							    2048, 2048 },
			.tx_compl_ring_size		= { 16384, 16384, 16384,
							    4096, 4096 },
			.tx_desc_count                  = { 16384, 16384, 16384,
							    4096, 4096 },
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
			.reo_dst_ring_size		= { 2048, 2048, 2048,
							    512, 512 },
			.tcl_data_ring_size		= { 512, 512, 512,
							    128, 128 },
			.tx_compl_ring_size		= { 8192, 8192, 8192,
							    1024, 1024 },
			.tx_desc_count                  = { 8192, 8192, 8192, 1024, 1024},
		},
	},
};

int ath12k_wifi6_hw_init(struct ath12k_base *ab)
{
	struct ath12k_hw_params *hw_params = NULL;
	struct ath12k_hw_params *hw_params_msi8 = NULL;
	int i;

	/* Set per-device mem_params for WiFi6 */
	ab->mem_params = ath12k_mem_profile_based_param_wifi6[ath12k_active_mem_profile];
	ath12k_max_clients = ab->mem_params.dp_params.dp_max_clients;

	for (i = 0; i < ARRAY_SIZE(ath12k_wifi6_hw_params); i++) {
		hw_params = &ath12k_wifi6_hw_params[i];

		if (hw_params->hw_rev == ab->hw_rev)
			break;
	}

	if (i == ARRAY_SIZE(ath12k_wifi6_hw_params)) {
		ath12k_err(ab, "Unsupported WiFi6 hardware version: 0x%x\n",
			   ab->hw_rev);
		return -EINVAL;
	}

	if ((ab->hif.bus == ATH12K_BUS_PCI &&
	    ab->msi.config->total_vectors == ATH12K_MSI_16)) {
		hw_params_msi8 = kzalloc(sizeof(struct ath12k_hw_params), GFP_KERNEL);
		if (!hw_params_msi8)
			return -ENOMEM;
		memcpy(hw_params_msi8, hw_params, sizeof(struct ath12k_hw_params));
		/*
		 * Include it when PPEDS patch are rebased
		 * hw_params_msi8->ext_irq_grp_num_max = 6;
		 */
		hw_params_msi8->ring_mask = &ath12k_wifi6_hw_ring_mask_qcn9074_msi8;
		ab->hw_params = hw_params_msi8;
	} else {
		ab->hw_params = hw_params;
	}
	ab->ath12k_ops = &ath12k_ops_wifi6;

	ath12k_info(ab, "WiFi6 Hardware name: %s\n", ab->hw_params->name);

	return 0;
}
