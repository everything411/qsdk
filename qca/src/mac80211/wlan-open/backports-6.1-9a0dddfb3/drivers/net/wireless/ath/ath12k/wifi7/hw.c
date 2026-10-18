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
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ath12k_cmn_extn.h"
#endif /* CPTCFG_QCN_EXTN */
#include "../mhi.h"
#include "mhi.h"
#include "../pci.h"
#include "pci.h"
#include "../ath_debug/athdbg_hw.h"
#include "dp_rx.h"
#include "wmi.h"
#include "../wow.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../testmode.h"
#include "../dp_peer.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/mesh_util.h"
#endif /* CPTCFG_QCN_EXTN */
#include "../dp_tx.h"
#include "dp_tx.h"
#include "dp.h"
#include "hal_qcn9274.h"
#include "hal_wcn7850.h"
#include "../cfr.h"
#include "../dp_stats.h"
#include "../dp_mon.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/wifi7_dp_extn.h"
#endif /* CPTCFG_QCN_EXTN */

static const guid_t wcn7850_uuid = GUID_INIT(0xf634f534, 0x6147, 0x11ec,
					     0x90, 0xd6, 0x02, 0x42,
					     0xac, 0x12, 0x00, 0x03);

static u8 ath12k_wifi7_hw_qcn9274_mac_from_pdev_id(int pdev_idx)
{
	return pdev_idx;
}

static int
ath12k_wifi7_hw_mac_id_to_pdev_id_qcn9274(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static int
ath12k_wifi7_hw_mac_id_to_srng_id_qcn9274(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static u8 ath12k_wifi7_hw_get_ring_selector_qcn9274(struct sk_buff *skb)
{
	return smp_processor_id();
}

static bool ath12k_wifi7_dp_srng_is_comp_ring_qcn9274(int ring_num)
{
	if (ring_num < 3 || ring_num == 4)
		return true;

	return false;
}

static int
ath12k_wifi7_hw_mac_id_to_pdev_id_wcn7850(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static int
ath12k_wifi7_hw_mac_id_to_srng_id_wcn7850(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static u8 ath12k_wifi7_hw_get_ring_selector_wcn7850(struct sk_buff *skb)
{
	return skb_get_queue_mapping(skb);
}

static bool ath12k_wifi7_dp_srng_is_comp_ring_wcn7850(int ring_num)
{
	if (ring_num == 0 || ring_num == 2 || ring_num == 4)
		return true;

	return false;
}

void ath12k_hw_qcn9274_fill_cfr_hdr_info(struct ath12k *ar,
					 struct ath12k_csi_cfr_header *header,
					 struct ath12k_cfr_peer_tx_param *params)
{
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->cfr_metadata_len = sizeof(struct cfr_enh_metadata);
	header->host_real_ts = ktime_to_ns(ktime_get_real());

	header->cfr_metadata_version = ATH12K_CFR_META_VERSION_11;
	if(ar->ab->hw_rev == ATH12K_HW_QCN6432_HW10)
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

void ath12k_hw_wcn7850_fill_cfr_hdr_info(struct ath12k *ar,
					 struct ath12k_csi_cfr_header *header,
					 struct ath12k_cfr_peer_tx_param *params)
{
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->cfr_metadata_len = sizeof(struct cfr_enh_metadata);
	header->host_real_ts = ktime_to_ns(ktime_get_real());

	header->cfr_metadata_version = ATH12K_CFR_META_VERSION_11;
	header->chip_type = ATH12K_CFR_RADIO_WCN7850;

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

static const struct ath12k_hw_ops qcn9274_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi7_hw_qcn9274_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi7_hw_mac_id_to_pdev_id_qcn9274,
	.mac_id_to_srng_id = ath12k_wifi7_hw_mac_id_to_srng_id_qcn9274,
	.rxdma_ring_sel_config = ath12k_wifi7_dp_rxdma_ring_sel_config_qcn9274,
	.get_ring_selector = ath12k_wifi7_hw_get_ring_selector_qcn9274,
	.dp_srng_is_tx_comp_ring = ath12k_wifi7_dp_srng_is_comp_ring_qcn9274,
	.fill_cfr_hdr_info = ath12k_hw_qcn9274_fill_cfr_hdr_info,
	.parse_cfr_enh_dma_hdr = ath12k_cfr_parse_enh_dma_hdr,
};

static const struct ath12k_hw_ops wcn7850_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi7_hw_qcn9274_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi7_hw_mac_id_to_pdev_id_wcn7850,
	.mac_id_to_srng_id = ath12k_wifi7_hw_mac_id_to_srng_id_wcn7850,
	.rxdma_ring_sel_config = ath12k_wifi7_dp_rxdma_ring_sel_config_wcn7850,
	.get_ring_selector = ath12k_wifi7_hw_get_ring_selector_wcn7850,
	.dp_srng_is_tx_comp_ring = ath12k_wifi7_dp_srng_is_comp_ring_wcn7850,
	.parse_cfr_enh_dma_hdr = ath12k_cfr_parse_enh_dma_hdr,
};

/* To support 8 MSI DP grouping */
static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_qcn9274_msi8 = {
        .tx  = {
                ATH12K_TX_RING_MASK_0,
                ATH12K_TX_RING_MASK_1,
#ifdef CPTCFG_EXT_IPA_OFFLOAD
		ATH12K_TX_RING_MASK_2,
#else
                ATH12K_TX_RING_MASK_2 | ATH12K_TX_RING_MASK_3,
#endif
                0, 0, 0, 0, 0
        },
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
        .rx_mon_dest = {
                0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
		ATH12K_RX_MON_RING_MASK_2,
		0, 0, 0
        },
        .rx = {
                0, 0, 0,
                ATH12K_RX_RING_MASK_0,
                ATH12K_RX_RING_MASK_1,
#ifdef CPTCFG_EXT_IPA_OFFLOAD
		ATH12K_RX_RING_MASK_2,
#else
                ATH12K_RX_RING_MASK_2 | ATH12K_RX_RING_MASK_3,
#endif
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
#ifdef CPTCFG_EXT_IPA_OFFLOAD
		0,
#else
                ATH12K_HOST2RXDMA_RING_MASK_0,
#endif
                0, 0, 0, 0
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

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_qcn9274 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
#ifndef CPTCFG_EXT_IPA_OFFLOAD
		ATH12K_TX_RING_MASK_3,
#else
		0,
#endif
		0, 0, 0, 0,
		0, 0, 0, 0, 0,
	},
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	.rx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
		ATH12K_RX_MON_RING_MASK_2,
		0, 0,
	},
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
#ifndef CPTCFG_EXT_IPA_OFFLOAD
		ATH12K_RX_RING_MASK_3,
#else
		0,
#endif
		0, 0, 0, 0, 0,
	},
	.rx_err = {
		0, 0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	.reo_status = {
		0, 0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	.tx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
		0, 0, 0, 0, 0
	},
	.host2txmon = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_HOST2TX_MON_RING_MASK_0,
	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0, 0, 0
	},
	.reo2ppe = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, ATH12K_REO2PPE_RING_MASK_0, 0
	},
	.ppeds_tx_cmpln = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
	.umac_dp_reset = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_UMAC_RESET_INTR_MASK_0
	},
};

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_ipq5332 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		ATH12K_TX_RING_MASK_3,
	},
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	.rx_mon_dest = {
		0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_RX_MON_RING_MASK_0,
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
	.tx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TX_MON_RING_MASK_0,
	},
	.host2txmon = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_HOST2TX_MON_RING_MASK_0,

	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0, 0, 0
	},
	.reo2ppe = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, ATH12K_REO2PPE_RING_MASK_0, 0
	},
	.ppeds_tx_cmpln = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
};

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_wcn7850 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
	},
	.host2rxmon = {
	},
	.rx_mon_dest = {
	},
	.rx_mon_status = {
		0, 0, 0, 0,
		ATH12K_RX_MON_STATUS_RING_MASK_0,
		ATH12K_RX_MON_STATUS_RING_MASK_1,
		ATH12K_RX_MON_STATUS_RING_MASK_2,
	},
	.rx = {
		0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
	},
	.rx_err = {
		ATH12K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		ATH12K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		ATH12K_REO_STATUS_RING_MASK_0,
	},
	.host2rxdma = {
	},
	.tx_mon_dest = {
	},
	.host2txmon = {
	},
};

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_qcn6432 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_TX_RING_MASK_3,
		0, 0, 0
	},
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	.rx_mon_dest = {
		0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
		ATH12K_RX_MON_RING_MASK_2,
		0, 0, 0, 0
	},
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
 		ATH12K_RX_RING_MASK_3,
                0, 0, 0, 0,
                0, 0, 0

	},
	.rx_err = {
		0, 0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
	.reo_status = {
		0, 0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
	.tx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
	},
	.host2txmon = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_HOST2TX_MON_RING_MASK_0,
	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0, 0, 0
	},
	.reo2ppe = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, ATH12K_REO2PPE_RING_MASK_0, 0
	},
	.ppeds_tx_cmpln = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
};

static const struct ce_ie_addr ath12k_wifi7_ce_ie_addr_ipq5332 = {
	.ie1_reg_addr = CE_HOST_IPQ5332_IE_ADDRESS - HAL_IPQ5332_CE_WFSS_REG_BASE,
	.ie2_reg_addr = CE_HOST_IPQ5332_IE_2_ADDRESS - HAL_IPQ5332_CE_WFSS_REG_BASE,
	.ie3_reg_addr = CE_HOST_IPQ5332_IE_3_ADDRESS - HAL_IPQ5332_CE_WFSS_REG_BASE,
};

static const struct ce_remap ath12k_wifi7_ce_remap_ipq5332 = {
	.base = HAL_IPQ5332_CE_WFSS_REG_BASE,
	.size = HAL_IPQ5332_CE_SIZE,
	.cmem_offset = HAL_SEQ_WCSS_CMEM_OFFSET,
};

const struct pmm_remap ath12k_wifi7_pmm_ipq5332 = {
	.base = HAL_IPQ5332_PMM_REG_BASE,
	.size = HAL_IPQ5332_PMM_SIZE,
};

static const struct ce_ie_addr ath12k_wifi7_ce_ie_addr_ipq5424 = {
	.ie1_reg_addr = CE_HOST_IPQ5424_IE_ADDRESS - HAL_IPQ5424_CE_WFSS_REG_BASE,
	.ie2_reg_addr = CE_HOST_IPQ5424_IE_2_ADDRESS - HAL_IPQ5424_CE_WFSS_REG_BASE,
	.ie3_reg_addr = CE_HOST_IPQ5424_IE_3_ADDRESS - HAL_IPQ5424_CE_WFSS_REG_BASE,
};

static const struct ce_remap ath12k_wifi7_ce_remap_ipq5424 = {
	.base = HAL_IPQ5424_CE_WFSS_REG_BASE,
	.size = HAL_IPQ5424_CE_SIZE,
	.cmem_offset = HAL_SEQ_WCSS_CMEM_OFFSET,
};

static struct ath12k_hw_params ath12k_wifi7_hw_params[] = {
	{
		.name = "qcn9274 hw1.0",
		.hw_rev = ATH12K_HW_QCN9274_HW10,
		.fw = {
			.dir = "QCN9274/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.unified_fw_image = false,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9274,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_qcn9274,

		.host_ce_config = ath12k_wifi7_host_ce_config_qcn9274,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_qcn9274,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_qcn9274,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = false,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
		.supports_monitor = false,
		.supports_tx_monitor = false,

		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.mhi_config = &ath12k_wifi7_mhi_config_qcn9274,

		.wmi_init = ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,
		.cp_arch_ops = &ath12k_wifi7_cp_ops,

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
		.supports_dynamic_smps_6ghz = true,

		.iova_mask = 0,

		.supports_aspm = false,

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = false,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 0,
			.fft_bin_sz = 0,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 0,
			.max_fft_bins = 1024,
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
		.cfr_num_stream_bufs = 127,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
				(CFR_HDR_MAX_LEN_WORDS_QCN9274 *4) +
				CFR_DATA_MAX_LEN_QCN9274,
		.mlo_3_link_tx_support = false,
		.quad_ring_monitor_support = false,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = false,
		.tlv_logger_support = ATH12K_TLV_LOGGER_DISABLED,
	},
	{
		.name = "wcn7850 hw2.0",
		.hw_rev = ATH12K_HW_WCN7850_HW20,

		.fw = {
			.dir = "WCN7850/hw2.0",
			.board_size = 256 * 1024,
			.cal_offset = 256 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.unified_fw_image = false,
		},

		.max_radios = 1,
		.single_pdev_only = true,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_WCN7850,
		.internal_sleep_clock = true,

		.hw_ops = &wcn7850_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_wcn7850,

		.host_ce_config = ath12k_wifi7_host_ce_config_wcn7850,
		.ce_count = 9,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_wcn7850,
		.target_ce_count = 9,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_wcn7850,
		.svc_to_ce_map_len = 14,

		.rxdma1_enable = false,
		.num_rxdma_per_pdev = 2,
		.num_rxdma_dst_ring = 1,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = true,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
				   BIT(NL80211_IFTYPE_AP) |
				   BIT(NL80211_IFTYPE_P2P_DEVICE) |
				   BIT(NL80211_IFTYPE_P2P_CLIENT) |
				   BIT(NL80211_IFTYPE_P2P_GO),
		.supports_monitor = true,
		.supports_tx_monitor = false,

		.idle_ps = true,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = false,
		.supports_suspend = true,
		.reoq_lut_support = true,
		.supports_shadow_regs = true,

		.num_tcl_banks = 7,
		.max_tx_ring = 3,

		.mhi_config = &ath12k_wifi7_mhi_config_wcn7850,

		.wmi_init = ath12k_wifi7_wmi_init_wcn7850,

		.hal_ops = &hal_wcn7850_ops,
		.cp_arch_ops = &ath12k_wifi7_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_PCIE_PERST_NO_PULL_V01),

		.rfkill_pin = 48,
		.rfkill_cfg = 0,
		.rfkill_on_level = 1,

		.rddm_size = 0x780000,

		.def_num_link = 2,
		.max_mlo_peer = 32,

		.otp_board_id_register = 0,

		.supports_sta_ps = true,

		.acpi_guid = &wcn7850_uuid,
		.supports_dynamic_smps_6ghz = false,

		.iova_mask = ATH12K_PCIE_MAX_PAYLOAD_SIZE - 1,

		.supports_aspm = true,

		.current_cc_support = true,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = false,
		.en_qdsslog = true,
		.support_fse = false,
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = false,
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
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_WCN7850 *4) +
					CFR_DATA_MAX_LEN_WCN7850,
		.mlo_3_link_tx_support = false,
		.quad_ring_monitor_support = true,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = false,
		.tlv_logger_support = ATH12K_TLV_LOGGER_DISABLED,
	},
	{
		.name = "qcn9274 hw2.0",
		.hw_rev = ATH12K_HW_QCN9274_HW20,
		.fw = {
			.dir = "QCN92XX/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.unified_fw_image = false,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9274,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_qcn9274,

		.host_ce_config = ath12k_wifi7_host_ce_config_qcn9274,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_qcn9274,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_qcn9274,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
#ifdef CPTCFG_EXT_IPA_OFFLOAD
		.rx_mac_buf_ring = true,
#else
		.rx_mac_buf_ring = false,
#endif
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
		.supports_monitor = true,
		.supports_tx_monitor = true,
		.max_clients_supported = 512,
		.max_clients_dbs = 256,
		.max_clients_dbs_sbs = 170,

		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.mhi_config = &ath12k_wifi7_mhi_config_qcn9274,

		.wmi_init = ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,
		.cp_arch_ops = &ath12k_wifi7_cp_ops,

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
		.supports_dynamic_smps_6ghz = true,

		.iova_mask = 0,

		.supports_aspm = false,

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = true,
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
		.support_ce_manual_poll=true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = true,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 127,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN9274 *4) +
					CFR_DATA_MAX_LEN_QCN9274,
		.mlo_3_link_tx_support = true,
		.quad_ring_monitor_support = false,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = false,
		.tlv_logger_support = ATH12K_TLV_LOGGER_DISABLED,
	},
	{
		.name = "ipq5332 hw1.0",
		.hw_rev = ATH12K_HW_IPQ5332_HW10,
		.fw = {
			.dir = "IPQ5332/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
			.unified_fw_image = false,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ5332,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_ipq5332,

		.host_ce_config = ath12k_wifi7_host_ce_config_ipq5332,
		.ce_count = 12,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_ipq5332,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_ipq5332,
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
		.supports_tx_monitor = true,
		.max_clients_supported = 256,
		.max_clients_dbs = 176,
		.max_clients_dbs_sbs = 85,

		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.wmi_init = &ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,
		.cp_arch_ops = &ath12k_wifi7_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),
		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = 0,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = false,
		.iova_mask = 0,
		.supports_aspm = false,

		.ce_ie_addr = &ath12k_wifi7_ce_ie_addr_ipq5332,
		.ce_remap = &ath12k_wifi7_ce_remap_ipq5332,
		.pmm_remap = &ath12k_wifi7_pmm_ipq5332,
		.en_qdsslog = true,
		.support_fse = true,
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
			.fragment_160mhz = false,
			.supports_320mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = false,
		.credit_flow = false,
		.is_plink_preferable = false,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = ATH12K_UMAC_RESET_IPC_IPQ5332,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = false,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 255,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_IPQ5332 *4) +
					CFR_DATA_MAX_LEN_IPQ5332,
		.mlo_3_link_tx_support = false,
		.send_platform_model = true,
		.quad_ring_monitor_support = false,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = false,
		.tlv_logger_support = ATH12K_TLV_LOGGER_DISABLED,
	},
	{
		.name = "qcn6432 hw1.0",
		.hw_rev = ATH12K_HW_QCN6432_HW10,
		.fw = {
			.dir = "QCN6432/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
			.unified_fw_image = false,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN6432,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_qcn6432,

		.host_ce_config = ath12k_wifi7_host_ce_config_ipq5332,
		.ce_count = 12,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_ipq5332,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_ipq5332,
		.svc_to_ce_map_len = 19,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
		.supports_monitor = true,
		.supports_tx_monitor = true,
		.max_clients_supported = 256,
		.max_clients_dbs = 176,
		.max_clients_dbs_sbs = 85,

		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.wmi_init = &ath12k_wifi7_wmi_init_qcn9274,
		.hal_ops = &hal_qcn9274_ops,
		.cp_arch_ops = &ath12k_wifi7_cp_ops,

		.supports_aspm = true,
		.send_platform_model = true,
		.en_qdsslog = true,
		.support_fse = true,
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
			.fragment_160mhz = false,
			.supports_320mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = true,
		.umac_reset_ipc = ATH12K_UMAC_RESET_IPC_QCN6432,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = true,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 128,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN6432 *4) +
					CFR_DATA_MAX_LEN_QCN6432,
		.mlo_3_link_tx_support = false,
		.quad_ring_monitor_support = false,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = false,
		.tlv_logger_support = ATH12K_TLV_LOGGER_DISABLED,
	},
	{
		.name = "ipq5424 hw1.0",
		.hw_rev = ATH12K_HW_IPQ5424_HW10,
		.fw = {
			.dir = "IPQ5424/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
			.unified_fw_image = false,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ5332,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_ipq5332,

		.host_ce_config = ath12k_wifi7_host_ce_config_ipq5332,
		.ce_count = 12,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_ipq5332,
		.target_ce_count = 12,
		.svc_to_ce_map = ath12k_wifi7_target_service_to_ce_map_wlan_ipq5332,
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
		.supports_tx_monitor = true,
		.max_clients_supported = 512,
		.max_clients_dbs = 256,
		.max_clients_dbs_sbs = 170,

		.idle_ps = false,
		.cold_boot_calib = ATH12K_COLD_BOOT_CALIB,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.wmi_init = &ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,
		.cp_arch_ops = &ath12k_wifi7_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = 0,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = false,
		.iova_mask = 0,
		.supports_aspm = false,

		.ce_ie_addr = &ath12k_wifi7_ce_ie_addr_ipq5424,
		.ce_remap = &ath12k_wifi7_ce_remap_ipq5424,
		.pmm_remap = &ath12k_wifi7_pmm_ipq5332,
		.en_qdsslog = true,
		.support_fse = true,
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
			.fragment_160mhz = false,
			.supports_320mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = false,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = ATH12K_UMAC_RESET_IPC_IPQ5332,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = true,
#endif
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 128,
		/* Max size :
		 * sizeof(ath12k_csi_cfr_header) + 96 bytes(cfr header) +
		 *                              15744 bytes(cfr payload)
		 * where cfr_header = rtt upload header len +
		 *                    freeze_tlv len + uplink user setup info
		 *                  = 32bytes + 32bytes + (8bytes * 4users)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_IPQ5424 * 4) +
					CFR_DATA_MAX_LEN_IPQ5424,
		.mlo_3_link_tx_support = false,
		.send_platform_model = true,
		.quad_ring_monitor_support = false,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.peer_del_all_support = false,
		.tlv_logger_support = ATH12K_TLV_LOGGER_DISABLED,
	},
};

static void ath12k_wifi7_mgmt_handler(struct ieee80211_hw *hw,
				      struct ieee80211_tx_control *control,
				      struct ieee80211_tx_info *info,
				      struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ieee80211_vif *vif = info->control.vif;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif = &ahvif->deflink;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ieee80211_key_conf *key = info->control.hw_key;
	struct ath12k_mgmt_frame_stats *mgmt_stats = &ahvif->mgmt_stats;
	struct ieee80211_sta *sta = control->sta;
	u32 control_flags = info->control.flags;
	u32 info_flags = info->flags;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k *ar;
	struct ieee80211_mgmt *mgmt = NULL;
	struct ieee80211_tx_rate rate;
	u8 link_id = 0, ring_id = 0;
	u16 frm_type = 0;
	int ret;
	bool is_prb_rsp;

	if ((control_flags & IEEE80211_TX_CTRL_MGMT_RATE_EXIST) &&
	     info->control.rates[0].idx >= 0)
		rate = info->control.rates[0];

	is_prb_rsp = ieee80211_is_probe_resp(hdr->frame_control);

	skb_cb->vif = vif;
	link_id = u32_get_bits(info->control.flags, IEEE80211_TX_CTRL_MLO_LINK);
	if (ieee80211_vif_is_mld(vif)) {
		link_id = ath12k_mac_get_tx_link(sta, vif, link_id, skb, info_flags);
		if (link_id >= ATH12K_NUM_MAX_LINKS ||
		    (ATH12K_SCAN_LINKS_MASK & BIT(link_id))) {
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev, sta,
							dp_vif, DP_TX_ENQ_DROP_INV_LINK,
							ring_id, false);
		}
	} else {
		link_id = 0;
	}

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (!arvif || !arvif->ar) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_ARVIF,
						ring_id, false);
		return;
	}

	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
	}

	ar = arvif->ar;
	skb_cb->link_id = link_id;

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

	if (is_prb_rsp && arvif->tbtt_offset) {
		u64 adjusted_tsf;

		mgmt = (struct ieee80211_mgmt *)skb->data;
		adjusted_tsf = cpu_to_le64(0ULL - arvif->tbtt_offset);
		memcpy(&mgmt->u.probe_resp.timestamp, &adjusted_tsf,
		       sizeof(adjusted_tsf));
	}
	if (ath12k_mac_is_bridge_vdev(arvif)) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev, sta, dp_vif,
						DP_TX_ENQ_DROP_BRIDGE_VDEV,
						ring_id, false);
		return;
	}

	if ((control_flags & IEEE80211_TX_CTRL_MGMT_RATE_EXIST) &&
	    rate.idx >= 0) {
		if (ath12k_skb_rhash_insert(ar, skb, rate))
			ath12k_warn(ar->ab,
				    "tx skb rhash entry creation failed\n");
	}

	frm_type = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
	ret = ath12k_mac_mgmt_tx(ar, skb, is_prb_rsp);

	if (ret) {
		if (ret != -EBUSY)
			ath12k_warn(ar->ab,
				    "failed to queue mgmt stype 0x%x frame %d\n",
				    frm_type, ret);
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev, sta, dp_vif,
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

/**
 * ath12k_wifi7_tx_setup_link() - Setup link for transmission
 * @vif: Virtual interface
 * @sta: Station pointer
 * @info: TX info
 * @skb: Socket buffer
 * @link_id: Output link ID
 *
 * Returns: 0 on success, negative on error
 */
int ath12k_wifi7_tx_setup_link(struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta,
			       struct ieee80211_tx_info *info,
			       struct sk_buff *skb,
			       u8 *link_id)
{
	u32 info_flags = info->flags;

	*link_id = u32_get_bits(info->control.flags, IEEE80211_TX_CTRL_MLO_LINK);

	if (ieee80211_vif_is_mld(vif)) {
		*link_id = ath12k_mac_get_tx_link(sta, vif, *link_id, skb, info_flags);
		if (*link_id >= ATH12K_NUM_MAX_LINKS ||
		    (ATH12K_SCAN_LINKS_MASK & BIT(*link_id))) {
			return -EINVAL;
		}
	} else {
		*link_id = 0;
	}

	return 0;
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
int ath12k_dp_mmesh_tx(struct ieee80211_hw *hw, struct ath12k_base *ab,
		       struct ath12k_link_vif *arvif, struct ieee80211_vif *vlan_vif,
		       struct sk_buff *skb, struct ieee80211_sta *sta,
		       struct ath12k_dp_skb_ctrl *skb_ctrl, bool is_eth,
		       u8 link_id, bool is_mcast, bool *htt_mesh,
		       struct ieee80211_tx_info *info, u32 qos_nw_delay)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = NULL;
	struct ath12k *ar = arvif->ar;
	struct sk_buff *skb_cloned = NULL;
	struct ath12k_skb_cb *skb_clone_cb  = NULL;
	enum ath12k_dp_tx_enq_error err;
	struct meta_hdr_s *mhdr = NULL;
	struct ath12k_link_sta *arsta = NULL;
	u8 qos_tag;
	u8 no_enc_frame = 0;
	bool is_sta = false;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	u8 ring_id = smp_processor_id();
	bool checkhdr = false;
	u8 flags;
	u16 len;
	int ret;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_dp_peer *dp_peer = NULL;

	/* Get station info if needed */
	if (sta) {
		is_sta = true;
		ahsta = ath12k_sta_to_ahsta(sta);
		qos_tag = u32_get_bits(skb->mark, QOS_TAG_MASK);
		if (ahsta->use_4addr_set || qos_tag)
			arsta = rcu_dereference(ahsta->link[link_id]);
		dp_peer = ath12k_sta_get_dp_peer_rcu(ahsta);
	}

	if (ahvif->dp_vif.tx_encap_type == ATH12K_HW_TXRX_NATIVE_WIFI ||
			ahvif->dp_vif.tx_encap_type == ATH12K_HW_TXRX_RAW) {
		pr_err("Native Wifi & Raw mode not supported\n");
		err = DP_TX_ENQ_DROP_MISC;
		goto fail;
	}

	len = skb->len;
	dp_vif = &ahvif->dp_vif;

	if (mmeshsim) {
		/* Add meta header */
		if (ahvif->dp_vif.dp_extn.mdbg & MESH_DBG_TX)
			print_hex_dump(KERN_INFO, "PREBUF: ", DUMP_PREFIX_OFFSET, 16, 1,
					skb->data, 64, false);

		ret = ath12k_dp_add_mesh_meta_hdr(skb, ahvif,
				!!ahvif->dp_vif.dp_extn.mdbg,
				&checkhdr);
		if (ret) {
			pr_err("Drop frames. Failure in adding mesh header in simulation\n");
			err = DP_TX_ENQ_DROP_MHDR_ERR;
			goto fail;
		}

		if (ahvif->dp_vif.dp_extn.mdbg & MESH_DBG_TX)
			print_hex_dump(KERN_INFO, "POSTBUF: ", DUMP_PREFIX_OFFSET, 16, 1,
					skb->data, 64, false);
	}

	/* Move the skb data ahead and point to the meta header */
	if (!mmeshsim)
		skb_push(skb, ahvif->dp_vif.dp_extn.mhdr_len);

	mhdr = (struct meta_hdr_s *)skb->data;
	if (mmeshsim && !checkhdr)
		mhdr = NULL;

	if (mhdr) {
		flags = mhdr->flags;
		skb_pull(skb, ahvif->dp_vif.dp_extn.mhdr_len);

		if (arvif->key_cipher != INVALID_CIPHER &&
				(mhdr->flags & METAHDR_FLAG_NOENCRYPT))
			no_enc_frame = 1;

		if (mhdr->flags & METAHDR_FLAG_NOQOS)
			skb->priority =  HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST;

		if ((mhdr->flags & METAHDR_FLAG_INFO_UPDATED) &&
				!no_enc_frame) {
			skb_cloned = skb_clone(skb, GFP_ATOMIC);
			if (!skb_cloned) {
				err = DP_TX_ENQ_DROP_CLONE;
				goto fail;
			}

			skb_clone_cb = ATH12K_SKB_CB(skb_cloned);
			skb_clone_cb->flags |= ATH12K_SKB_MESH_TX_INFO;
			*htt_mesh = true;
		}

		if (skb_cloned) {
			/* Applicable only for mmesh mode.
			 * Clone the SKB and send it to firmware
			 * with the updated rate info.
			 * Firmware uses this to update
			 * the peer cached rate info.
			 */

			local_bh_disable();
			/* Route based on multicast/unicast */
			if (!is_mcast) {
				/* Unicast path */
				ath12k_wifi7_ucast_handler(dp_vif, link_id, arsta,
							   skb_cloned, skb_ctrl,
							   qos_nw_delay,
							   true, dp_peer);
			} else {
				ath12k_wifi7_mcbc_handler(dp_vif, link_id, arsta,
							  skb_cloned, is_eth,
							  false, is_sta, vlan_vif,
							  skb_ctrl, info,
							  qos_nw_delay, false, sta);
				ieee80211_free_txskb(hw, skb_cloned);
			}
			local_bh_enable();

		}

		if (no_enc_frame) {
			skb_cb->flags  |= ATH12K_SKB_MESH_TX_INFO;
			*htt_mesh = true;
		} else {
			skb_cb->flags  &= ~ATH12K_SKB_MESH_TX_INFO;
			*htt_mesh = false;
		}
	}

	ath12k_dbg_level(ab, ATH12K_DBG_MMESH, ATH12K_DBG_L1,
			"skb %p clone %p no_enc_frm %d skb->pri %d tx_info_flag %d",
			skb, skb_cloned,  no_enc_frame, skb->priority,
			!!(skb_cb->flags & ATH12K_SKB_MESH_TX_INFO));

	ath12k_dbg_level(ab, ATH12K_DBG_MMESH, ATH12K_DBG_L1,
			" hdr len %d skb->len %d mhdr flags 0x%x mhdr %p htt_mesh %d\n",
			skb->len - len,        skb->len, flags, mhdr, *htt_mesh);
	return 0;
fail:
	dp_pdev = ath12k_dp_to_dp_pdev(ar->ab->dp, ar->pdev_idx);
	ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
					ahsta ? ath12k_ahsta_to_sta(ahsta) : NULL,
					&ahvif->dp_vif, err, ring_id, true);
	return -EINVAL;
}
#endif

/**
 * ath12k_wifi7_mac_op_tx() - Main MAC operation TX function
 * @hw: ieee80211_hw pointer
 * @control: TX control
 * @skb: Socket buffer
 *
 * Main entry point for packet transmission with bitmap-based routing
 */
void ath12k_wifi7_mac_op_tx(struct ieee80211_hw *hw,
			    struct ieee80211_tx_control *control,
			    struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = NULL;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ieee80211_key_conf *key = info->control.hw_key;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar = NULL;
	struct ieee80211_vif *vlan_vif = control ? control->vlan_vif : NULL;
	struct ieee80211_hdr *hdr = NULL;
	struct ieee80211_sta *sta = control->sta;
	u32 qos_nw_delay = info->sawf.nw_delay;
	u32 info_flags = info->flags;
	struct ieee80211_tx_info info_tx = {0};
	struct ath12k_dp_skb_ctrl skb_ctrl = {0};
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_link_sta *arsta = NULL;
	u8 link_id = 0, qos_tag;
	bool is_mcast = false, is_pkt_classified = false;
	bool is_data = false;
	int ret;
	bool is_eth = false, gsn_valid = true;
	bool is_dvlan = false, is_sta = false;
	bool htt_mesh = false;
	struct ath12k_dp_peer *dp_peer = NULL;

	/* Check queue stop */
	if (unlikely(ah->queue_stop)) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_QUEUE_STOP,
						0, true);
		return;
	}

#ifdef CPTCFG_QCN_EXTN
	/* fast path */
	if (likely(ath12k_dp_tx_check_fast_path(skb, info_flags, dp_vif, &skb_ctrl,
						qos_nw_delay)))
		return;
#endif
	hdr = (struct ieee80211_hdr *)skb->data;
	memcpy(&info_tx, info, sizeof(*info));
	skb_cb = ATH12K_SKB_CB(skb);
	memset(skb_cb, 0, sizeof(*skb_cb));

	/* Check monitor mode */
	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_VIF_TYPE_MON,
						0, false);
		return;
	}

	/* Classify packet */
	is_pkt_classified = ath12k_dp_tx_classify_packet(hw, dp_vif, &info_tx, skb,
							 &is_mcast, &is_eth,
							 &is_data, key, &skb_ctrl);
	if (unlikely(!is_pkt_classified))
		return;

	/* Route to management handler */
	if (!is_data) {
		ath12k_wifi7_mgmt_handler(hw, control, &info_tx, skb);
		return;
	}

	/* TODO once peer clean up changes are done we will optimize below code
	 * and will avoid using the arvif
	 */

	/* Setup link for data frame */
	ret = ath12k_wifi7_tx_setup_link(vif, sta, &info_tx, skb, &link_id);
	if (ret) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, NULL, sta, dp_vif,
						DP_TX_ENQ_DROP_INV_LINK,
						0, false);
		return;
	}

	/* Get link virtual interface */
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

	/* Get station info if needed */
	if (sta) {
		is_sta = true;
		ahsta = ath12k_sta_to_ahsta(sta);
		qos_tag = u32_get_bits(skb->mark, QOS_TAG_MASK);
		if (ahsta->use_4addr_set || qos_tag)
			arsta = rcu_dereference(ahsta->link[link_id]);
		dp_peer = ath12k_sta_get_dp_peer_rcu(ahsta);
	}

	/* to check for if MAC has added the encrption in case of
	 * nwifi AP-VLAN frame
	 */

	if (!test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED,
		      &ah->ag->flags) &&
	    !(info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) &&
	    !(skb_cb->flags & ATH12K_SKB_CIPHER_SET) &&
	    ieee80211_has_protected(hdr->frame_control)) {
		is_dvlan = true;
		skb_ctrl.features |= DP_FEATURE_SW_ENCRPT;
	}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	if (QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(ahvif->vap_submode)) {
		ret = ath12k_dp_mmesh_tx(hw, ar->ab,  arvif, vlan_vif, skb, sta,
					 &skb_ctrl, is_eth, link_id, is_mcast,
					 &htt_mesh, &info_tx, qos_nw_delay);

		if (ret)
			return;
	}
#endif

	/*
	 * TCL ring is lockless (no per-core protection).
	 * Disable bottom halves to prevent concurrent TX enqueue on
	 * the same CPU and ensure serialized access to the TCL ring.
	 */
	local_bh_disable();
	/* Route based on multicast/unicast */
	if (!is_mcast) {
		/* Unicast path */
		ath12k_wifi7_ucast_handler(dp_vif, link_id, arsta, skb,
					   &skb_ctrl, qos_nw_delay, htt_mesh, dp_peer);
	} else {

		if (!vif->valid_links || is_dvlan || (is_eth && sta) ||
		    (!is_eth && (hdr && !is_multicast_ether_addr(hdr->addr1))) ||
		    test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ah->ag->flags))
			gsn_valid = false;

		ath12k_wifi7_mcbc_handler(dp_vif, link_id, arsta, skb, is_eth,
					  gsn_valid, is_sta, vlan_vif, &skb_ctrl,
					  &info_tx, qos_nw_delay, htt_mesh, sta);
		ieee80211_free_txskb(hw, skb);
	}
	local_bh_enable();
}

static void ath12k_wifi7_mac_op_sta_set_4addr(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif,
					      struct ieee80211_sta *sta,
					      bool enabled)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_vlan_iface *vlan_iface = ahvif->vlan_iface;

	if (enabled && !ahsta->use_4addr_set) {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ahsta->ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
		ahsta->vlan_iface = ahvif->vlan_iface;
#endif
		wiphy_work_queue(hw->wiphy, &ahsta->set_4addr_wk);
		ahsta->use_4addr_set = true;
		if (vif->type == NL80211_IFTYPE_AP_VLAN && vlan_iface)
			vlan_iface->is_wds_4addr = true;
	}
}

static const struct ieee80211_ops ath12k_ops_wifi7 = {
	.tx				= ath12k_wifi7_mac_op_tx,
	.wake_tx_queue			= ieee80211_handle_wake_tx_queue,
	.start                          = ath12k_mac_op_start,
	.stop                           = ath12k_mac_op_stop,
	.reconfig_complete              = ath12k_mac_op_reconfig_complete,
	.add_interface                  = ath12k_mac_op_add_interface,
	.remove_interface		= ath12k_mac_op_remove_interface,
	.update_vif_offload		= ath12k_mac_op_update_vif_offload,
	.config                         = ath12k_mac_op_config,
	.sta_set_4addr			= ath12k_wifi7_mac_op_sta_set_4addr,
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
	.sta_pre_rcu_remove             = ath12k_mac_op_sta_pre_rcu_remove,
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
	.set_preserved_link_stats	= ath12k_mac_op_preserved_link_stats,
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
	.get_key_seq                    = ath12k_mac_op_get_key_seq,
	.set_monitor_flags              = ath12k_mac_op_set_monitor_flags,
#ifdef CPTCFG_QCN_EXTN
	.set_muedca_mode		= ath12k_mac_set_muedca_mode,
#endif /* CPTCFG_QCN_EXTN */
};

/*
 * ath12k_mem_profile_based_param_wifi7 - Per-profile runtime memory parameters
 * for WiFi7.
 */
static const struct ath12k_mem_profile_based_param
ath12k_mem_profile_based_param_wifi7[] = {
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
			.rxdma_monitor_buf_ring_size	= 4096,
			.rxdma_monitor_dst_ring_size	= 1024,
			.smart_mon_filter_default	= DP_SMART_MON_PROFILE_1G,
			.mon_num_ppdu_desc		= 128,
			.rx_desc_count			= 12288,
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
			.tx_monitor_spc_bitmap		= 63,
			.reo_dst_ring_size		= { 8192, 8192, 8192,
							    2048, 2048 },
			.tcl_data_ring_size		= { 2048, 2048, 2048,
							    512, 512 },
			.tx_compl_ring_size		= { 32768, 32768, 32768,
							    2048, 2048 },
			.tx_desc_count			= { 32768, 32768, 32768,
							    2048, 2048 },
		},
	},
	[ATH12K_MEM_PROFILE_BALANCED] = {
		.num_vdevs			= 8 + 1,
		.num_bridge_vdevs		= 0,
		.num_max_vdevs_nlink		= 0,
		.target_mem_mode		= ATH12K_QMI_TARGET_MEM_MODE_512M,
		.num_stations_single		= 128,
		.num_stations_dbs		= 128,
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
			.dp_mon_status_buf		= 200,
			.tx_monitor_buf_ring_size	= 256,
			.tx_monitor_buf_ring_fill_lvl	= 256,
			.tx_monitor_dst_ring_size	= 256,
			.tx_monitor_num_ppdu_desc	= 8,
			.tx_monitor_num_status_buf	= 32,
			.tx_monitor_spc_bitmap		= 63,
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
			.tx_monitor_spc_bitmap		= 63,
			.reo_dst_ring_size		= { 2048, 2048, 2048, 512, 512 },
			.tcl_data_ring_size		= { 512, 512, 512, 128, 128 },
			.tx_compl_ring_size		= { 8192, 8192, 8192,
							    1024, 1024 },
			.tx_desc_count			= { 8192, 8192, 8192,
							    1024, 1024 },
		},
	},
};

int ath12k_wifi7_hw_init(struct ath12k_base *ab)
{
	struct ath12k_hw_params *hw_params = NULL;
	struct ath12k_hw_params *hw_params_msi8 = NULL;
	int i;

	/*
	 * Set per-device for WiFi7 from the
	 * wifi7-specific profile table.  Also update ath12k_max_clients
	 */
	ab->mem_params = ath12k_mem_profile_based_param_wifi7[ath12k_active_mem_profile];
	ath12k_max_clients = ab->mem_params.dp_params.dp_max_clients;

	for (i = 0; i < ARRAY_SIZE(ath12k_wifi7_hw_params); i++) {
		hw_params = &ath12k_wifi7_hw_params[i];

		if (hw_params->hw_rev == ab->hw_rev)
			break;
	}

	if (i == ARRAY_SIZE(ath12k_wifi7_hw_params)) {
		ath12k_err(ab, "Unsupported WiFi7 hardware version: 0x%x\n",
			   ab->hw_rev);
		return -EINVAL;
	}

	if (ab->hif.bus == ATH12K_BUS_PCI &&
	    ab->msi.config->total_vectors == ATH12K_MSI_16) {
		hw_params_msi8 = kzalloc(sizeof(struct ath12k_hw_params), GFP_KERNEL);
		if (!hw_params_msi8)
			return -ENOMEM;
		memcpy(hw_params_msi8, hw_params, sizeof(struct ath12k_hw_params));
		/* Include it when PPEDS patch are rebased
		hw_params_msi8->ext_irq_grp_num_max = 6;
		 */
		hw_params_msi8->ring_mask = &ath12k_wifi7_hw_ring_mask_qcn9274_msi8;
		ab->hw_params = hw_params_msi8;
	} else {
		ab->hw_params = hw_params;
	}
	ab->ath12k_ops = &ath12k_ops_wifi7;
	ab->map_event_required = true;

#ifdef CPTCFG_QCN_EXTN
	ath12k_wifi7_hw_init_extn(ab);
#endif /* CPTCFG_QCN_EXTN */

	ath12k_info(ab, "WiFi7 Hardware name: %s\n", ab->hw_params->name);

	return 0;
}
