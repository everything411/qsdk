/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/irq.h>
#include <linux/reset.h>
#include <linux/skbuff.h>
#include <fal/fal_qm.h>
#include <fal/fal_rss_hash.h>
#include <fal/fal_servcode.h>
#include <ppe_drv_sc.h>
#include <ppe_drv_acl.h>
#include <ppe_drv.h>
#include <ppe_drv_port.h>
#include <linux/clk.h>
#include <linux/cpumask.h>
#include "edma.h"
#include "edma_cfg_tx.h"
#include "edma_cfg_rx.h"
#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
#include "edma_cfg_rx_loopback.h"
#include "edma_cfg_tx_loopback.h"
#endif
#include "edma_regs.h"
#include "edma_debug.h"
#include "edma_debugfs.h"
#include "edma_procfs.h"
#include "nss_dp_dev.h"
#include "nss_dp_vp.h"
#ifdef NSS_DP_DDRQ_SUPPORT
#include "edma_ddrq.h"
#endif
#ifdef NSS_DP_UDP_ST_SUPPORT
#include "edma_cfg_tx_udp_st.h"
#endif

int edma_dp_extension_en = 0;
module_param(edma_dp_extension_en, int, 0640);
MODULE_PARM_DESC(edma_dp_extension_en, "Enable VLAN Insert Functionality (1 for enable, 0 for disable)");

/*
 * Module parameters for the host mode config.
 */
int edma_dp_host_num_rxfill_rings = NR_CPUS;
module_param(edma_dp_host_num_rxfill_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_host_num_rxfill_rings, "Number of Host RX fill rings");

int edma_dp_host_num_rx_rings = NR_CPUS;
module_param(edma_dp_host_num_rx_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_host_num_rx_rings, "Number of Host RX rings");

int edma_dp_host_queues_per_ring = 8;
module_param(edma_dp_host_queues_per_ring, int, 0640);
MODULE_PARM_DESC(edma_dp_host_queues_per_ring, "Number of queues per rx rings");

module_param_array(edma_dp_host_rx_rings, int, NULL, 0);
MODULE_PARM_DESC(edma_dp_host_rx_rings, "RX rings for host");

module_param_array(edma_dp_host_rx_queue_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_host_rx_queue_map, "Queue base for each RX ring");

module_param_array(edma_dp_host_rxfill_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_host_rxfill_map, "RX ring to RX fill ring mapping");

int edma_dp_host_num_tx_rings = NR_CPUS;
module_param(edma_dp_host_num_tx_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_host_num_tx_rings, "Number of Host TX rings");

int edma_dp_host_num_tx_rings_per_core = EDMA_MAX_TX_RINGS_PER_CORE;
module_param(edma_dp_host_num_tx_rings_per_core, int, 0640);
MODULE_PARM_DESC(edma_dp_host_num_tx_rings_per_core, "Number of Host TX rings");

module_param(edma_dp_boot_rx_fill_cnt, int, 0644);
MODULE_PARM_DESC(edma_dp_boot_rx_fill_cnt, "Initial number of buffers to reap for Rx fill rings");

int edma_dp_host_num_txcmpl_rings = NR_CPUS + EDMA_MAX_TXDESC_RING_PPEVP;
module_param(edma_dp_host_num_txcmpl_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_host_num_txcmpl_rings, "Number of Host TX cmpl rings");

module_param_array(edma_dp_host_tx_rings, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_host_tx_rings, "TX rings for host");

module_param_array(edma_dp_host_txcmpl_rings, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_host_txcmpl_rings, "TX cmpl rings for host");

module_param_array(edma_dp_host_txcmpl_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_host_txcmpl_map, "TX to txcmpl map rings for host");

module_param_array(edma_dp_host_tx_ring_to_core_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_host_tx_ring_to_core_map, "TX to core map");

module_param(edma_dp_host_rx_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_host_rx_ring_sz, "Host RX descriptor ring size (default: 2048)");

module_param(edma_dp_host_rxfill_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_host_rxfill_ring_sz, "Host RX fill ring size (default: 2048)");

module_param(edma_dp_host_tx_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_host_tx_ring_sz, "Host TX descriptor ring size (default: 2048)");

module_param(edma_dp_host_txcmpl_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_host_txcmpl_ring_sz, "Host TX completion ring size (default: 2048)");

#ifdef NSS_DP_DDRQ_SUPPORT
#ifdef NSS_DP_SFU_PROFILE
int edma_passthrough_val = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
#else
int edma_passthrough_val = EDMA_PASSTHROUGH_VAL_INVALID;
#endif
module_param(edma_passthrough_val, int, 0640);
MODULE_PARM_DESC(edma_passthrough_val, "EDMA passthrough value to be set in Tx desc");

int edma_ddrq_gbl_en_hw = EDMA_DDRQ_GBL_EN_HW_DEF;
module_param(edma_ddrq_gbl_en_hw, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_gbl_en_hw, "Disable/enable DDRQ feature from EDMA hardware");

int edma_ddrq_gbl_en_sw = EDMA_DDRQ_GBL_EN_SW_DEF;
module_param(edma_ddrq_gbl_en_sw, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_gbl_en_sw, "Disable/enable DDRQ feature from EDMA module");

int edma_ddrq_desc_pf_thres = EDMA_DDRQ_GBL_PF_THRES;
module_param(edma_ddrq_desc_pf_thres, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_desc_pf_thres, "EDMA hardware's prefetch threshold for DDRQ descriptor");

int edma_ddrq_data_offset = EDMA_DDRQ_DATA_OFFSET_DEF;
module_param(edma_ddrq_data_offset, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_data_offset, "Offset bitmap beyond which the packet will be written to DDR buffer");

int edma_ddrq_blk_num = NSS_DP_EDMA_DDRQ_BLK_NUM_DEF;
module_param(edma_ddrq_blk_num, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_blk_num, "Number of blocks for DDRQ");

int edma_ddrq_blk_size = NSS_DP_EDMA_DDRQ_BLK_SIZE_DEF;
module_param(edma_ddrq_blk_size, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_blk_size, "Size of one DDRQ block");

int edma_ddrq_desc_wb_thres = EDMA_DDRQ_GBL_WB_THRES;
module_param(edma_ddrq_desc_wb_thres, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_desc_wb_thres, "EDMA hardware's writeback threshold for DDRQ descriptor");

int edma_ddrq_gbl_data_offset0 = EDMA_DDRQ_GBL_DATA_OFFSET_REG0_VAL;
module_param(edma_ddrq_gbl_data_offset0, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_gbl_data_offset0, "DDRQ data offset 0 value");

int edma_ddrq_en_port_bm = NSS_DP_EDMA_DDRQ_EN_PORT_BM;
module_param(edma_ddrq_en_port_bm, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_en_port_bm, "DDRQ enable bitmap per port");

int edma_ddrq_vp_port_map[NSS_DP_MAX_PORTS] = {0x9, 0xa, 0xb, 0xc, 0xd, 0xe};
module_param_array(edma_ddrq_vp_port_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_vp_port_map, "DDRQ virtual port mapping");

int edma_ddrq_ac_queue_ac_en = EDMA_DDRQ_AC_Q_AC_EN_DEF;
module_param(edma_ddrq_ac_queue_ac_en, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_ac_en, "DDRQ per queue AC configuration");

int edma_ddrq_ac_queue_color_aware = EDMA_DDRQ_AC_Q_COLOR_AWARE_DEF;
module_param(edma_ddrq_ac_queue_color_aware, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_color_aware, "DDRQ per queue color aware configuration");

int edma_ddrq_ac_queue_wred_en = EDMA_DDRQ_AC_Q_WRED_EN_DEF;
module_param(edma_ddrq_ac_queue_wred_en, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_wred_en, "DDRQ per queue WRED configuration");

int edma_ddrq_ac_queue_shared_ceiling = EDMA_DDRQ_NO_OP_DEF_VAL;
module_param(edma_ddrq_ac_queue_shared_ceiling, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_shared_ceiling, "DDRQ per queue shared ceiling configuration");

int edma_ddrq_ac_queue_grp_id = EDMA_DDRQ_AC_Q_GRP_ID_DEF;
module_param(edma_ddrq_ac_queue_grp_id, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_grp_id, "DDRQ per queue group id configuration");

int edma_ddrq_ac_queue_pre_alloc = EDMA_DDRQ_AC_Q_PRE_ALLOC_DEF;
module_param(edma_ddrq_ac_queue_pre_alloc, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_pre_alloc, "DDRQ per queue pre alloc configuration");

int edma_ddrq_ac_queue_shared_dynamic = EDMA_DDRQ_AC_Q_SHRD_DYNAMIC;
module_param(edma_ddrq_ac_queue_shared_dynamic, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_shared_dynamic, "DDRQ per queue shared dynamic configuration");

int edma_ddrq_ac_queue_shared_weight = EDMA_DDRQ_AC_Q_SHRD_WEIGHT;
module_param(edma_ddrq_ac_queue_shared_weight, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_ac_queue_shared_weight, "DDRQ per queue shared weight configuration");

int edma_ddrq_grp_ac_en = EDMA_DDRQ_AC_GRP_AC_EN_DEF;
module_param(edma_ddrq_grp_ac_en, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_grp_ac_en, "DDRQ group AC configuration");

int edma_ddrq_grp_color_aware = EDMA_DDRQ_AC_GRP_COLOR_AWARE_DEF;
module_param(edma_ddrq_grp_color_aware, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_grp_color_aware, "DDRQ group color aware configuration");

int edma_ddrq_grp_shrd_limit = EDMA_DDRQ_NO_OP_DEF_VAL;
module_param(edma_ddrq_grp_shrd_limit, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_grp_shrd_limit, "DDRQ group shared limit configuration");

int edma_ddrq_grp_dp_thrd = EDMA_DDRQ_NO_OP_DEF_VAL;
module_param(edma_ddrq_grp_dp_thrd, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_grp_dp_thrd, "DDRQ group drop threshold configuration");

int edma_ddrq_grp_id_bm = EDMA_DDRQ_AC_GRP_ID_BM_DEF;
module_param(edma_ddrq_grp_id_bm, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_grp_id_bm, "DDRQ group id bitmask");

int edma_ddrq_isq_base = EDMA_DDRQ_ISQ_BASE;
module_param(edma_ddrq_isq_base, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_isq_base, "DDRQ ingress SRAM queue base");

int edma_ddrq_lp_queue_base = EDMA_DDRQ_LP_QUEUE_BASE_DEF;
module_param(edma_ddrq_lp_queue_base, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_lp_queue_base, "DDRQ loopback queue base");

int edma_ddrq_lp_num_queues = EDMA_DDRQ_LP_NUM_QUEUES_DEF;
module_param(edma_ddrq_lp_num_queues, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_lp_num_queues, "DDRQ loopback queue count");

int edma_ddrq_lp_id = EDMA_DDRQ_LP_ID;
module_param(edma_ddrq_lp_id, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_lp_id, "DDRQ EDMA loopback id to be used");

int edma_ddrq_lp_fc_grp_id = EDMA_DDRQ_LP_FC_GRP_ID;
module_param(edma_ddrq_lp_fc_grp_id, int, S_IRUGO);
MODULE_PARM_DESC(edma_ddrq_lp_fc_grp_id, "DDRQ EDMA loopback id's FC GRP ID to map");
#endif

/*
 * PPE-VP ring information
 */
module_param(edma_dp_ppe_vp_num_tx_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_ppe_vp_num_tx_rings, "Number of host ppe_vp TX rings");

module_param_array(edma_dp_ppe_vp_tx_rings, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_tx_rings, "TX rings for host ppe_vp");

module_param_array(edma_dp_ppe_vp_txcmpl_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_txcmpl_map, "TX cmpl rings for host ppe_vp");

module_param(edma_dp_ppe_vp_num_tx_rings_per_core, int, 0640);
MODULE_PARM_DESC(edma_dp_ppe_vp_num_tx_rings_per_core, "Number of host ppe_vp TX rings per core");

module_param_array(edma_dp_ppe_vp_tx_ring_to_core_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_tx_ring_to_core_map, "TX to core map for host ppe_vp");

module_param(edma_dp_ppe_vp_tx_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_ppe_vp_host_tx_ring_sz, "Host TX descriptor ring size (default: 2048)");

/*
 * PPE-VP feature ring information
 */
module_param(edma_dp_ppe_vp_feat_num_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_num_rings, "Number of ppe_vp feature RX rings (0 to disable)");

module_param_array(edma_dp_ppe_vp_feat_rx_rings, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_rx_rings, "RX rings for ppe_vp features");

module_param_array(edma_dp_ppe_vp_feat_rx_queue_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_rx_queue_map, "Queue base for each ppe_vp feature RX ring");

module_param_array(edma_dp_ppe_vp_feat_rxfill_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_rxfill_map, "RX ring to RX fill ring mapping for ppe_vp features");

module_param_array(edma_dp_ppe_vp_feat_type_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_type_map, "VP feature sub-type per ppe_vp feature RX ring");

module_param(edma_dp_ppe_vp_feat_rx_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_rx_ring_sz, "PPE-VP feature RX descriptor ring size (default: 4096)");

module_param(edma_dp_ppe_vp_feat_rxfill_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_ppe_vp_feat_rxfill_ring_sz, "PPE-VP feature RX fill ring size (default: 4096)");

/*
 * Each bit denotes the configured mode for that particular EDMA Tx/Rx rings IDs:
 *   Bit is set : The particular ring is configured in pre-header mode
 *   Bit is not set : The particular ring is configured in secondary ring mode
 */
int edma_rx_ring_mode_bitmask = EDMA_RX_RING_MODE_BITMASK_DEF;
module_param(edma_rx_ring_mode_bitmask, int, 0640);
MODULE_PARM_DESC(edma_rx_ring_mode_bitmask, "EDMA Rx ring mode (preheader/secondary ring) bitmask");

int edma_tx_ring_mode_bitmask = EDMA_TX_RING_MODE_BITMASK_DEF;
module_param(edma_tx_ring_mode_bitmask, int, 0640);
MODULE_PARM_DESC(edma_tx_ring_mode_bitmask, "EDMA Tx ring mode (preheader/secondary ring) bitmask");

#ifdef NSS_DP_HW_GRO
int edma_dp_gro_num_rxfill_rings = 4;
module_param(edma_dp_gro_num_rxfill_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_gro_num_rxfill_rings, "Number of GRO RX fill rings");

int edma_dp_gro_num_rx_rings = EDMA_RX_RING_GRO_NUM_MAX;
module_param(edma_dp_gro_num_rx_rings, int, 0640);
MODULE_PARM_DESC(edma_dp_gro_num_rx_rings, "Number of GRO RX rings");

#if EDMA_RX_RING_GRO_NUM_MAX > 4
#error "Number of GRO rings cannot exceed 4"
#endif

int edma_dp_gro_queues_per_ring = 1;
module_param(edma_dp_gro_queues_per_ring, int, 0640);
MODULE_PARM_DESC(edma_dp_gro_queues_per_ring, "Number of queues per rx rings");

module_param(edma_dp_gro_ppe_queue_base, int, 0640);
MODULE_PARM_DESC(edma_dp_gro_ppe_queue_base, "GRO PPE Queue Base");

int edma_dp_gro_rx_rings[EDMA_MAX_RXDESC_RING_PER_TYPE] = {20, 21, 22, 23, -1, -1, -1, -1};
module_param_array(edma_dp_gro_rx_rings, int, NULL, 0);
MODULE_PARM_DESC(edma_dp_gro_rx_rings, "RX rings for gro");

int edma_dp_gro_rx_queue_map[EDMA_MAX_RXDESC_RING_PER_TYPE] = {0, 1, 2, 3, -1, -1, -1, -1};
module_param_array(edma_dp_gro_rx_queue_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_gro_rx_queue_map, "Queue base for each RX ring");

int edma_dp_gro_rxfill_map[EDMA_MAX_RXFILL_RING_PER_TYPE] = {16, 17, 18, 19, -1, -1, -1, -1};
module_param_array(edma_dp_gro_rxfill_map, int, NULL, S_IRUGO);
MODULE_PARM_DESC(edma_dp_gro_rxfill_map, "RX ring to RX fill ring mapping");

/*
 * Ensure that GRO rx ring size is multiple of 2
 */
module_param(edma_dp_gro_rx_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_gro_rx_ring_sz, "GRO RX descriptor ring size (default: 512)");

module_param(edma_dp_gro_rxfill_ring_sz, uint, 0640);
MODULE_PARM_DESC(edma_dp_gro_rxfill_ring_sz, "GRO RX fill ring size (default: 512)");
#endif

/*
 * PPE-DS ring information
 */
module_param_array(edma_dp_ppe_ds_rx_rings, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_rx_rings, "Rx desc ring ID for ppeds");

module_param_array(edma_dp_ppe_ds_rx_queue_map, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_rx_queue_map, "RX queue map for for ppeds");

module_param_array(edma_dp_ppe_ds_num_rx_queue, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_num_rx_queue, "RX num queue map per ring for for ppeds");

module_param_array(edma_dp_ppe_ds_rxfill_rings, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_rxfill_rings, "RxFill ring ID for ppeds");

module_param_array(edma_dp_ppe_ds_tx_rings, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_tx_rings, "Tx desc ring ID for ppeds");

module_param_array(edma_dp_ppe_ds_num_txdesc_per_node, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_num_txdesc_per_node, "Txdesc rings per ppeds node");

module_param_array(edma_dp_ppe_ds_num_rxdesc_per_node, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_num_rxdesc_per_node, "Rxdesc rings per ppeds node");

module_param_array(edma_dp_ppe_ds_txcmpl_rings, int, NULL, 0640);
MODULE_PARM_DESC(edma_dp_ppe_ds_txcmpl_rings, "Tx completion ring ID for ppeds");

/*
 * Module parameters for the bp stats enabled txcmpl, rxdesc, and rxfill rings config
 */
int edma_bp_stats_en_txcmpl_rings[EDMA_REG_PER_RING_TYPE_BP_COUNTER_MAX] = {0,1,7,8,9,10};
module_param_array(edma_bp_stats_en_txcmpl_rings, bp_stats_en_txcmpl_ring_id, NULL, 0640);
MODULE_PARM_DESC(edma_bp_stats_en_txcmpl_rings, "bp_stats_en_txcmpl_rings");

int edma_bp_stats_en_rxdesc_rings[EDMA_REG_PER_RING_TYPE_BP_COUNTER_MAX] = {0,1,7,8,9,10};
module_param_array(edma_bp_stats_en_rxdesc_rings, bp_stats_en_rxdesc_ring_id, NULL, 0640);
MODULE_PARM_DESC(edma_bp_stats_en_rxdesc_rings, "bp_stats_en_rxdesc_rings");

int edma_bp_stats_en_rxfill_rings[EDMA_REG_PER_RING_TYPE_BP_COUNTER_MAX] = {0,1,7,8,9,10};
module_param_array(edma_bp_stats_en_rxfill_rings, bp_stats_en_rxfill_ring_id, NULL, 0640);
MODULE_PARM_DESC(edma_bp_stats_en_rxfill_rings, "bp_stats_en_rxfill_rings");

#ifdef NSS_DP_UDP_ST_SUPPORT
/*
 * Module parameters for UDP-ST TX rings
 */
int edma_udp_st_tx_ring = -1;
module_param(edma_udp_st_tx_ring, int, 0640);
MODULE_PARM_DESC(edma_udp_st_tx_ring, "TX ring ID for UDP speed test");

int edma_udp_st_tx_cmpl_ring = -1;
module_param(edma_udp_st_tx_cmpl_ring, int, 0640);
MODULE_PARM_DESC(edma_udp_st_tx_cmpl_ring, "TX completion ring ID for UDP speed test");

int edma_udp_st_fc_grp_id = 0;
module_param(edma_udp_st_fc_grp_id, int, 0640);
MODULE_PARM_DESC(edma_udp_st_fc_grp_id, "Flow Control group ID for UDP-ST TX ring (TXDESC_CTRL FC_GRP_ID field)");

int edma_udp_st_ring_size = EDMA_TX_UDP_ST_RING_SIZE;
module_param(edma_udp_st_ring_size, int, 0640);
MODULE_PARM_DESC(edma_udp_st_ring_size, "Ring size for UDP-ST TX descriptor ring");

int edma_udp_st_pass_through_mode = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
module_param(edma_udp_st_pass_through_mode, int, 0640);
MODULE_PARM_DESC(edma_udp_st_pass_through_mode, "Pass-through mode for UDP-ST TX descriptors (0=192B, 1=128B, 2=0B, 3=FULL_DATA)");
#endif

/*
 * Pass-through mode used for skbs that are cloned, or not SFE forwarded
 */
int edma_ddrq_sp_cloned_pt_mode = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
module_param(edma_ddrq_sp_cloned_pt_mode, int, 0640);
MODULE_PARM_DESC(edma_ddrq_sp_cloned_pt_mode, "Pass-through mode for cloned/non-SFE-forwarded skbs (0=192B, 1=128B, 2=0B, 3=FULL_DATA)");

/*
 * Input String length for VLAN insertion.
 */
#define EDMA_VLAN_APPEND_INFO_STR_LEN 40

DEFINE_PER_CPU(struct nss_dp_vp_ctx, g_vp_ctx);
uint32_t edma_hang_recover = 0;
struct edma_gbl_ctx *edma_gbl_ctx;

/*
 * EDMA configuration information
 */
struct edma_init_info init_info;

static char edma_txcmpl_irq_name[EDMA_MAX_TXCMPL_RINGS][EDMA_IRQ_NAME_SIZE];
static char edma_rxdesc_irq_name[EDMA_MAX_RXDESC_RINGS][EDMA_IRQ_NAME_SIZE];
static char edma_rxfill_irq_name[EDMA_MAX_RXFILL_RINGS][EDMA_IRQ_NAME_SIZE];

/*
 * Input String for VLAN insertion.
 */
static char edma_vlan_append_info[EDMA_VLAN_APPEND_INFO_STR_LEN];
uint32_t edma_force_crash = 0;

char *argv[] = {"/usr/bin/edma_recover.sh", NULL };

/*
 * edma_param_set_bp_stats_en_txcmpl_ring_id()
 *      Custom setter for the edma_bp_stats_en_txcmpl_rings module parameter.
 */
static int edma_param_set_bp_stats_en_txcmpl_ring_id(const char *val, const struct kernel_param *kp)
{
        int ret;
        int ring_id;
        struct kernel_param kptmp = *kp;	/* Create a temporary kp pointing to our local ring_id */

        kptmp.arg = &ring_id;

	/*
	 * Use standard helper to parse string -> integer
	 */
        ret = param_set_int(val, &kptmp);
        if (ret < 0)
                return ret;

	/*
	 * Perform Range Check
	 */
        if (ring_id < 0 || ring_id >= EDMA_MAX_TXCMPL_RINGS) {
                pr_err("Invalid txcmpl ring_id: %d, Valid range: [%d, %d]\n", ring_id,
			0, EDMA_MAX_TXCMPL_RINGS - 1);
                return -EINVAL;
        }

	/*
	 * If valid, write to the actual location
	 * Note: param_set_int already wrote to ring_id, now we copy it to the real array element
	 */
	*((int *)kp->arg) = ring_id;

        return 0;
}

/*
 * edma_param_set_bp_stats_en_rxdesc_ring_id()
 *      Custom setter for the edma_bp_stats_en_rxdesc_rings module parameter.
 */
static int edma_param_set_bp_stats_en_rxdesc_ring_id(const char *val, const struct kernel_param *kp)
{
        int ret;
        int ring_id;
        struct kernel_param kptmp = *kp;	/* Create a temporary kp pointing to our local ring_id */

        kptmp.arg = &ring_id;

	/*
	 * Use standard helper to parse string -> integer
	 */
        ret = param_set_int(val, &kptmp);
        if (ret < 0)
                return ret;

	/*
	 * Perform Range Check
	 */
        if (ring_id < 0 || ring_id >= EDMA_MAX_RXDESC_RINGS) {
                pr_err("Invalid rxdesc ring_id: %d, Valid range: [%d, %d]\n", ring_id,
			0, EDMA_MAX_RXDESC_RINGS - 1);
                return -EINVAL;
        }

	/*
	 * If valid, write to the actual location
	 * Note: param_set_int already wrote to ring_id, now we copy it to the real array element
	 */
        *((int *)kp->arg) = ring_id;

        return 0;
}

/*
 * edma_param_set_bp_stats_en_rxfill_ring_id()
 *      Custom setter for the edma_bp_stats_en_rxfill_rings module parameter.
 */
static int edma_param_set_bp_stats_en_rxfill_ring_id(const char *val, const struct kernel_param *kp)
{
        int ret;
        int ring_id;
        struct kernel_param kptmp = *kp;	/* Create a temporary kp pointing to our local ring_id */

        kptmp.arg = &ring_id;

	/*
	 * Use standard helper to parse string -> integer
	 */
        ret = param_set_int(val, &kptmp);
        if (ret < 0)
                return ret;

	/*
	 * Perform Range Check
	 */
        if (ring_id < 0 || ring_id >= EDMA_MAX_RXFILL_RINGS) {
                pr_err("Invalid rxfill ring_id: %d, Valid range: [%d, %d]\n", ring_id,
			0, EDMA_MAX_RXFILL_RINGS - 1);
                return -EINVAL;
        }

	/*
	 * If valid, write to the actual location
	 * Note: param_set_int already wrote to ring_id, now we copy it to the real array element
	 */
        *((int *)kp->arg) = ring_id;

        return 0;
}

/*
 * Defines the ops for the custom 'bp_stats_en_txcmpl_ring_id' parameter type.
 */
static const struct kernel_param_ops param_ops_bp_stats_en_txcmpl_ring_id = {
        .set = edma_param_set_bp_stats_en_txcmpl_ring_id,
        .get = param_get_int,	/* Use standard getter */
};

/*
 * Defines the ops for the custom 'bp_stats_en_rxdesc_ring_id' parameter type.
 */
static const struct kernel_param_ops param_ops_bp_stats_en_rxdesc_ring_id = {
        .set = edma_param_set_bp_stats_en_rxdesc_ring_id,
        .get = param_get_int,	/* Use standard getter */
};

/*
 * Defines the ops for the custom 'bp_stats_en_rxfill_ring_id' parameter type.
 */
static const struct kernel_param_ops param_ops_bp_stats_en_rxfill_ring_id = {
        .set = edma_param_set_bp_stats_en_rxfill_ring_id,
        .get = param_get_int,	/* Use standard getter */
};

/*
 * edma_recovery_work()
 *      Call usermodehelper function to execute edma_recovery user script.
 */
static void edma_recovery_work(struct work_struct *work)
{
        int ret;

        ret = call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_PROC);
        if(ret < 0){
                pr_err("Failed to run EDMA recovery script: %d\n", ret);
        }
}

#if defined(NSS_DP_POINT_OFFLOAD)
/*
 * nss_dp_point_offload_info_get()
 *	Get point offload ring information
 */
void nss_dp_point_offload_info_get(uint32_t *txdesc_num, uint32_t *txcmpl_num,
		uint32_t *rxfill_num, uint32_t *rxdesc_num)
{
	*txdesc_num = edma_gbl_ctx->txdesc_point_offload_ring;
	*txcmpl_num = edma_gbl_ctx->txcmpl_point_offload_ring;
	*rxfill_num = edma_gbl_ctx->rxfill_point_offload_ring;
	*rxdesc_num = edma_gbl_ctx->rxdesc_point_offload_ring;
}
EXPORT_SYMBOL(nss_dp_point_offload_info_get);
#endif

/*
 * edma_nsm_sawf_sc_stats_read()
 *	Read stats for NSM for a given service class.
 */
bool edma_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class)
{
	struct edma_sawf_sc_stats *sawf_sc_stats;
	unsigned int start;

	if (!PPE_DRV_SERVICE_CLASS_IS_VALID(service_class)) {
		edma_warn("%u Invalid SAWF service class.", service_class);
		return false;
	}

	sawf_sc_stats = &edma_gbl_ctx->sawf_sc_stats[service_class];
	do {
		start = edma_dp_stats_fetch_begin(&sawf_sc_stats->syncp);
		nsm_stats->rx_packets = sawf_sc_stats->rx_packets;
		nsm_stats->rx_bytes = sawf_sc_stats->rx_bytes;
	} while (edma_dp_stats_fetch_retry(&sawf_sc_stats->syncp, start));

	return true;
}

/*
 * edma_disable_rx_interrupts()
 *	Disable EDMA RX interrupt masks.
 */
void edma_disable_rx_interrupts(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->rxdesc_ring_max; i++) {
		if (egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxdesc_ring *rxdesc_ring =
					egc->rxdesc_info[i].rxdesc_ring;
			if (!rxdesc_ring) {
				continue;
			}
			edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
					EDMA_MASK_INT_CLEAR);
			edma_debug("Rx desc ring : %d INTR mask (%d) got disabled", rxdesc_ring->ring_id, egc->rxdesc_intr_mask);
		}
	}

	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxfill_ring *rxfill_ring =
					egc->rxfill_info[i].rxfill_ring;
			if (!rxfill_ring) {
				continue;
			}
			edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id),
					EDMA_MASK_INT_CLEAR);
			edma_debug("rxfill ring : %d INTR got disabled\n", rxfill_ring->ring_id);
		}
	}

}

/*
 * edma_disable_interrupts()
 *	Disable EDMA RX/TX interrupt masks.
 */
void edma_disable_interrupts(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->rxdesc_ring_max; i++) {
		if (egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxdesc_ring *rxdesc_ring =
					egc->rxdesc_info[i].rxdesc_ring;
			edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
					EDMA_MASK_INT_CLEAR);
		}
	}

	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxfill_ring *rxfill_ring =
					egc->rxfill_info[i].rxfill_ring;
			edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id),
					EDMA_MASK_INT_CLEAR);
		}
	}

	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_txcmpl_ring *txcmpl_ring =
					egc->txcmpl_info[i].txcmpl_ring;
			edma_reg_write(EDMA_REG_TX_INT_MASK(txcmpl_ring->id),
					EDMA_MASK_INT_CLEAR);
		}
	}

	/*
	 * Clear MISC interrupt mask.
	 */
	edma_reg_write(EDMA_REG_MISC_INT_MASK, EDMA_MASK_INT_CLEAR);
}

/*
 * edma_enable_rx_interrupts()
 *	Enable RX EDMA interrupt masks.
 */
void edma_enable_rx_interrupts(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->rxdesc_ring_max; i++) {
		if (egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxdesc_ring *rxdesc_ring =
					egc->rxdesc_info[i].rxdesc_ring;
			if (!rxdesc_ring) {
				continue;
			}
			edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
					egc->rxdesc_intr_mask);
			edma_debug("Rx desc ring : %d INTR mask (%d) got enabled", rxdesc_ring->ring_id, egc->rxdesc_intr_mask);
		}
	}

	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxfill_ring *rxfill_ring =
					egc->rxfill_info[i].rxfill_ring;
			if (!rxfill_ring) {
				continue;
			}
			/*
			 * Configure just the low threshold value, the interrupts
			 * are enabled when the available number of descriptors
			 * in rx-fill ring goes below low threshold mark.
			 */
			edma_reg_write(EDMA_REG_RXFILL_UGT_THRE(rxfill_ring->ring_id),
					EDMA_RXFILL_UGT_THRESHOLD);
			edma_debug("rxfill ring : %d INTR got enabled\n", rxfill_ring->ring_id);
		}
	}
}

/*
 * edma_enable_interrupts()
 *	Enable RX/TX EDMA interrupt masks.
 */
void edma_enable_interrupts(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->rxdesc_ring_max; i++) {
		if (egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxdesc_ring *rxdesc_ring =
					egc->rxdesc_info[i].rxdesc_ring;
			edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
					egc->rxdesc_intr_mask);
		}
	}

	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_rxfill_ring *rxfill_ring =
					egc->rxfill_info[i].rxfill_ring;
			/*
			 * Configure just the low threshold value, the interrupts
			 * are enabled when the available number of descriptors
			 * in rx-fill ring goes below low threshold mark.
			 */
			edma_reg_write(EDMA_REG_RXFILL_UGT_THRE(rxfill_ring->ring_id),
					EDMA_RXFILL_UGT_THRESHOLD);
		}
	}

	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_txcmpl_ring *txcmpl_ring =
					egc->txcmpl_info[i].txcmpl_ring;
			edma_reg_write(EDMA_REG_TX_INT_MASK(txcmpl_ring->id),
					egc->txcmpl_intr_mask);
		}
	}

	/*
	 * Enable MISC interrupt mask.
	 */
	edma_reg_write(EDMA_REG_MISC_INT_MASK, egc->misc_intr_mask);
}

/*
 * edma_disable_port()
 *	EDMA disable port
 */
static void edma_disable_port(void)
{
	edma_reg_write(EDMA_REG_PORT_CTRL, EDMA_DISABLE);
}

/*
 * edma_cleanup()
 *	EDMA cleanup
 */
void edma_cleanup(bool is_dp_override)
{
	uint8_t profile_mask = 0;

	/*
	 * The cleanup can happen from data plane override
	 * or from module_exit, we want to cleanup only once
	 *
	 * On cleanup, disable EDMA only at module exit time, since
	 * NSS firmware depends on this setting.
	 */
	if (!edma_gbl_ctx->edma_initialized) {
		if (!is_dp_override) {
			edma_disable_port();
		}
		return;
	}

	if (edma_gbl_ctx->ctl_table_hdr) {
		unregister_sysctl_table(edma_gbl_ctx->ctl_table_hdr);
		edma_gbl_ctx->ctl_table_hdr = NULL;
	}

	/*
	 * TODO: Check with HW team about the state of in-flight
	 * packets when the descriptor rings are disabled.
	 */
	edma_cfg_tx_rings_disable(edma_gbl_ctx);
	edma_cfg_rx_rings_disable(edma_gbl_ctx);

	edma_disable_interrupts(edma_gbl_ctx);

	/*
	 * Remove interrupt handlers and NAPI
	 */
	if (edma_gbl_ctx->napi_added) {
		uint32_t i;

		/*
		 * Free IRQ for TXCMPL rings
		 */
		for (i = 0; i < edma_gbl_ctx->txcmpl_ring_max; i++) {
			if (!(edma_gbl_ctx->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
				continue;

			synchronize_irq(edma_gbl_ctx->txcmpl_info[i].intr_num);

			free_irq(edma_gbl_ctx->txcmpl_info[i].intr_num,
					(void *)(edma_gbl_ctx->txcmpl_info[i].txcmpl_ring));
		}

		/*
		 * Free IRQ for RXDESC rings
		 */
		for (i = 0; i < edma_gbl_ctx->rxdesc_ring_max; i++) {
			if (!(edma_gbl_ctx->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
				continue;

			synchronize_irq(edma_gbl_ctx->rxdesc_info[i].intr_num);
			free_irq(edma_gbl_ctx->rxdesc_info[i].intr_num,
					(void *)(edma_gbl_ctx->rxdesc_info[i].rxdesc_ring));
		}

		/*
		 * Free Misc IRQ
		 */
		synchronize_irq(edma_gbl_ctx->misc_intr);
		free_irq(edma_gbl_ctx->misc_intr, (void *)(edma_gbl_ctx->pdev));

		edma_cfg_rx_napi_delete(edma_gbl_ctx);
		edma_cfg_tx_napi_delete(edma_gbl_ctx);
		edma_gbl_ctx->napi_added = false;
	}

	/*
	 * Disable EDMA only at module exit time, since NSS firmware
	 * depends on this setting.
	 */
	if (!is_dp_override) {
		edma_disable_port();
	}

	/*
	 * cleanup rings and free
	 */
	edma_cfg_tx_rings_cleanup(edma_gbl_ctx);
	edma_cfg_rx_rings_cleanup(edma_gbl_ctx);

#ifdef NSS_DP_UDP_ST_SUPPORT
	if (edma_udp_st_tx_ring > EDMA_RING_FLAGS_INVALID_ID) {
		edma_cfg_tx_udp_st_ring_disable(edma_gbl_ctx);
		edma_cfg_tx_udp_st_ring_cleanup(edma_gbl_ctx);
	}
#endif

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (edma_gbl_ctx->loopback_en) {
		edma_cfg_tx_loopback_rings_disable(edma_gbl_ctx);
		edma_cfg_rx_loopback_rings_disable(edma_gbl_ctx);
		edma_cfg_tx_loopback_rings_cleanup(edma_gbl_ctx);
		edma_cfg_rx_loopback_rings_cleanup(edma_gbl_ctx);

		edma_rx_free_buffer_loopback();
		kfree(edma_gbl_ctx->txdesc_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txcmpl_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->rxdesc_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->rxfill_loopback_ring_id_arr);
	}
#endif

#ifdef NSS_DP_PPEDS_SUPPORT
	edma_ppeds_deinit(&edma_gbl_ctx->ppeds_drv);
#endif

	/*
	 * Release EDMA HW reset reference.
	 */
	reset_control_put(edma_gbl_ctx->hw_rst);

	iounmap(edma_gbl_ctx->reg_base);
	release_mem_region((edma_gbl_ctx->reg_resource)->start,
			resource_size(edma_gbl_ctx->reg_resource));

	/*
	 * Clean the debugfs entries for the EDMA
	 */
	edma_debugfs_exit();

	profile_mask = NSS_DP_MEM_PROFILE_HIGH | NSS_DP_MEM_PROFILE_BALANCED;
	if (edma_gbl_ctx->mem_profile & profile_mask) {
		/*
		 * Unregister PTP service code callback function
		 */
		ppe_drv_sc_unregister_cb(PPE_DRV_SC_PTP);
	}
	/*
	 * Unregister mirror core selection API callback with PPE driver
	 */
	ppe_drv_acl_mirror_core_select_unregister_cb();

	/*
	 * Clean the procfs entries for EDMA ring stats
	 */
	edma_procfs_exit();

	/*
	 * Mark initialize false, so that we do not
	 * try to cleanup again
	 */
	edma_gbl_ctx->edma_initialized = false;
}

/*
 * edma_validate_host_txrx_rings()
 *	Validate host TX and RX ring information
 *	Note: Caller is supposed to send NULL if no tx/rx info is given.
 *	      The caller should ensure to send the valid bitmaps.
 */
static int edma_validate_host_txrx_rings(struct edma_rx_rings_info *rx_info,
					struct edma_tx_rings_info *tx_info,
					uint32_t rxfill_ring_bitmap,
					uint32_t txcmpl_ring_bitmap)
{
	uint32_t rx_ring_bitmap = 0;
	uint32_t tx_ring_bitmap = 0;
	int i;

	/*
	 * If no RX / TX is valid, return error.
	 */
	if (!rx_info && !tx_info)
		return -EINVAL;

	if (!rx_info)
		goto validate_tx;

	/*
	 * Validate RX ring counts
	 */
	if ((rx_info->num_rx_rings <= 0) || (rx_info->num_rx_rings > EDMA_MAX_RXDESC_RING_PER_TYPE)) {
		edma_err("Invalid num_rx_rings (%d), valid range: 1-%d\n",
			rx_info->num_rx_rings, EDMA_MAX_RXDESC_RING_PER_TYPE);
		return -EINVAL;
	}

	/*
	 * Validate RX ring mappings and check for duplicates
	 */
	for (i = 0; i < rx_info->num_rx_rings; i++) {
		uint32_t rx_ring_id = rx_info->rx_map[i].rx_ring_id;
		uint32_t rx_fill_ring_id = rx_info->rx_map[i].rx_fill_ring_id;

		if ((rx_ring_id < 0) || (rx_ring_id >= EDMA_MAX_RXDESC_RINGS)) {
			edma_err("Invalid rx_ring_id (%d) at index %d, max allowed: %d\n",
				rx_ring_id, i, EDMA_MAX_RXDESC_RINGS - 1);
			return -EINVAL;
		}

		if ((rx_fill_ring_id < 0) || (rx_fill_ring_id >= EDMA_MAX_RXFILL_RINGS)) {
			edma_err("Invalid rx_fill_ring_id (%d) at index %d, max allowed: %d\n",
				rx_fill_ring_id, i, EDMA_MAX_RXFILL_RINGS - 1);
			return -EINVAL;
		}

		/* Verify that rx_fill_ring_id is present in the validated rxfill_ring_bitmap */
		if (!(rxfill_ring_bitmap & (1 << rx_fill_ring_id))) {
			edma_err("rx_fill_ring_id (%d) at index %d is not present in edma_rxfill_ring_map\n",
				rx_fill_ring_id, i);
			return -EINVAL;
		}

		/* Check for duplicate RX ring ID */
		if (rx_ring_bitmap & (1 << rx_ring_id)) {
			edma_err("Duplicate rx_ring_id (%d) found at index %d\n", rx_ring_id, i);
			return -EINVAL;
		}

		rx_ring_bitmap |= (1 << rx_ring_id);
	}

validate_tx:
	if (!tx_info)
		return 0;

	/*
	 * Validate TX ring counts
	 */
	if ((tx_info->num_tx_rings <= 0) || (tx_info->num_tx_rings > EDMA_MAX_TXDESC_RING_PER_TYPE)) {
		edma_err("Invalid num_tx_rings (%d), valid range: 1-%d\n",
			tx_info->num_tx_rings, EDMA_MAX_TXDESC_RING_PER_TYPE);
		return -EINVAL;
	}

	if ((tx_info->max_rings_per_core <= 0) || (tx_info->max_rings_per_core > EDMA_MAX_TX_RINGS_PER_CORE)) {
		edma_err("Invalid max_rings_per_core (%d), valid range: 1-%d\n",
			tx_info->max_rings_per_core, EDMA_MAX_TX_RINGS_PER_CORE);
		return -EINVAL;
	}

	/*
	 * Validate TX ring mappings and check for duplicates
	 */
	for (i = 0; i < tx_info->num_tx_rings; i++) {
		uint32_t tx_ring_id = tx_info->tx_map[i].tx_ring_id;
		uint32_t tx_cmpl_ring_id = tx_info->tx_map[i].tx_cmpl_ring_id;

		if ((tx_ring_id < 0) || (tx_ring_id >= EDMA_MAX_TXDESC_RINGS)) {
			edma_err("Invalid tx_ring_id (%d) at index %d, max allowed: %d\n",
				tx_ring_id, i, EDMA_MAX_TXDESC_RINGS - 1);
			return -EINVAL;
		}

		if ((tx_cmpl_ring_id < 0) || (tx_cmpl_ring_id >= EDMA_MAX_TXCMPL_RINGS)) {
			edma_err("Invalid tx_cmpl_ring_id (%d) at index %d, max allowed: %d\n",
				tx_cmpl_ring_id, i, EDMA_MAX_TXCMPL_RINGS - 1);
			return -EINVAL;
		}

		/* Verify that tx_cmpl_ring_id is present in the validated txcmpl_ring_bitmap */
		if (!(txcmpl_ring_bitmap & (1 << tx_cmpl_ring_id))) {
			edma_err("tx_cmpl_ring_id (%d) at index %d is not present in edma_txcmpl_ring_map\n",
				tx_cmpl_ring_id, i);
			return -EINVAL;
		}

		/* Check for duplicate TX ring ID */
		if (tx_ring_bitmap & (1 << tx_ring_id)) {
			edma_err("Duplicate tx_ring_id (%d) found at index %d\n", tx_ring_id, i);
			return -EINVAL;
		}

		tx_ring_bitmap |= (1 << tx_ring_id);
	}

	/*
	 * Validate TX ring per core map
	 */
	for (i = 0; i < NR_CPUS; i++) {
		int j;
		for (j = 0; j < tx_info->max_rings_per_core; j++) {
			uint32_t tx_ring_id;

			if (j >= EDMA_MAX_TX_RINGS_PER_CORE) {
				break;
			}

			tx_ring_id = tx_info->tx_ring_per_core_map[i][j];

			if ((tx_ring_id < 0) || (tx_ring_id >= EDMA_MAX_TXDESC_RINGS)) {
				edma_err("Invalid tx_ring_per_core_map[%d][%d] = %d, max allowed: %d\n",
					i, j, tx_ring_id, EDMA_MAX_TXDESC_RINGS - 1);
				return -EINVAL;
			}

			/* Verify that the ring ID was already validated in tx_map */
			if (!(tx_ring_bitmap & (1 << tx_ring_id))) {
				edma_err("tx_ring_per_core_map[%d][%d] = %d is not present in tx_map\n",
					i, j, tx_ring_id);
				return -EINVAL;
			}
		}
	}

	return 0;
}

/*
 * edma_validate_host_ring_info()
 *	Validate common ring information and call SFE ring validation
 */
static int edma_validate_host_ring_info(void)
{
	struct edma_host_info *host_info = &init_info.host_info;
	struct edma_rings_common_info *common_info = &host_info->common_info;
	struct edma_rx_rings_info *rx_info = &host_info->sfe_info.rx_info;
	struct edma_tx_rings_info *tx_info = &host_info->sfe_info.tx_info;
	uint32_t rxfill_ring_bitmap = 0;
	uint32_t txcmpl_ring_bitmap = 0;
	uint32_t vp_feat_rxfill_bitmap = 0;
	int i;

	/*
	 * Validate common FILL ring counts
	 */
	if ((common_info->edma_num_rxfill_rings <= 0) ||
	    (common_info->edma_num_rxfill_rings > EDMA_MAX_RXFILL_RING_PER_TYPE)) {
		edma_err("Invalid edma_num_rxfill_rings (%d), valid range: 1-%d\n",
			common_info->edma_num_rxfill_rings, EDMA_MAX_RXFILL_RING_PER_TYPE);
		return -EINVAL;
	}

	/*
	 * Validate RXFILL ring mappings and check for duplicates
	 */
	for (i = 0; i < common_info->edma_num_rxfill_rings; i++) {
		uint32_t ring_id = common_info->edma_rxfill_ring_map[i];

		if ((ring_id < 0) || (ring_id >= EDMA_MAX_RXFILL_RINGS)) {
			edma_err("Invalid edma_rxfill_ring_map[%d] = %d, max allowed: %d\n",
				i, ring_id, EDMA_MAX_RXFILL_RINGS - 1);
			return -EINVAL;
		}

		/* Check for duplicate RXFILL ring ID */
		if (rxfill_ring_bitmap & (1 << ring_id)) {
			edma_err("Duplicate edma_rxfill_ring_map (%d) found at index %d\n", ring_id, i);
			return -EINVAL;
		}

		rxfill_ring_bitmap |= (1 << ring_id);
	}

	/*
	 * Validate common cmpl ring counts
	 */
	if ((common_info->edma_num_txcmpl_rings <= 0) ||
	    (common_info->edma_num_txcmpl_rings > EDMA_MAX_TXCMPL_RING_PER_TYPE)) {
		edma_err("Invalid edma_num_txcmpl_rings (%d), valid range: 1-%d\n",
			common_info->edma_num_txcmpl_rings, EDMA_MAX_TXCMPL_RING_PER_TYPE);
		return -EINVAL;
	}

	/*
	 * Validate TXCMPL ring mappings and check for duplicates
	 */
	for (i = 0; i < common_info->edma_num_txcmpl_rings; i++) {
		uint32_t ring_id = common_info->edma_txcmpl_ring_map[i];

		if ((ring_id < 0) || (ring_id >= EDMA_MAX_TXCMPL_RINGS)) {
			edma_err("Invalid edma_txcmpl_ring_map[%d] = %d, max allowed: %d\n",
				i, ring_id, EDMA_MAX_TXCMPL_RINGS - 1);
			return -EINVAL;
		}

		/* Check for duplicate TXCMPL ring ID */
		if (txcmpl_ring_bitmap & (1 << ring_id)) {
			edma_err("Duplicate edma_txcmpl_ring_map (%d) found at index %d\n", ring_id, i);
			return -EINVAL;
		}

		txcmpl_ring_bitmap |= (1 << ring_id);
	}

	/*
	 * Validate host SFE rings (RX and TX)
	 * This can be extended further for host GRO rings.
	 */
	if (edma_validate_host_txrx_rings(rx_info, tx_info, rxfill_ring_bitmap, txcmpl_ring_bitmap)) {
		edma_err("Validating host SFE rings failed\n");
		return -EINVAL;
	}

	/*
	 * Validate host PPE VP tx rings
	 */
	tx_info = &host_info->vp_info.tx_info;
	if (edma_validate_host_txrx_rings(NULL, tx_info, 0, txcmpl_ring_bitmap)) {
		edma_err("Validating host PPE VP tx rings failed\n");
		return -EINVAL;
	}

	/*
	 * Validate host PPE VP feature rx rings (optional, num_rx_rings may be 0).
	 * VP feat rings use a dedicated rxfill ring separate from the host rxfill
	 * pool, so build and validate a VP feat-specific rxfill bitmap.
	 */
	rx_info = &host_info->vp_feat_info.rx_info;
	if (!rx_info->num_rx_rings) {
		edma_info("VP feature rings not allocated\n");
		return 0;
	}

	/* Validate VP feat rx ring size: must be non-zero and a power of 2 */
	if (!edma_dp_ppe_vp_feat_rx_ring_sz || (edma_dp_ppe_vp_feat_rx_ring_sz & (edma_dp_ppe_vp_feat_rx_ring_sz - 1))) {
		edma_err("Invalid VP feat rx ring size: %u (must be a non-zero power of 2)\n",
			 edma_dp_ppe_vp_feat_rx_ring_sz);
		return -EINVAL;
	}

	/* Validate VP feat rxfill ring size: must be non-zero and a power of 2 */
	if (!edma_dp_ppe_vp_feat_rxfill_ring_sz || (edma_dp_ppe_vp_feat_rxfill_ring_sz & (edma_dp_ppe_vp_feat_rxfill_ring_sz - 1))) {
		edma_err("Invalid VP feat rxfill ring size: %u (must be a non-zero power of 2)\n",
			 edma_dp_ppe_vp_feat_rxfill_ring_sz);
		return -EINVAL;
	}

	/*
	 * Build the bitmap
	 */
	for (i = 0; i < rx_info->num_rx_rings; i++) {
		uint32_t fill_id = rx_info->rx_map[i].rx_fill_ring_id;

		if ((fill_id < 0) || (fill_id >= EDMA_MAX_RXFILL_RINGS)) {
			edma_err("Invalid VP feat rx_fill_ring_id (%d) at index %d, max allowed: %d\n",
				fill_id, i, EDMA_MAX_RXFILL_RINGS - 1);
			return -EINVAL;
		}

		/* Check for duplicate VP feat RXFILL ring ID */
		if (vp_feat_rxfill_bitmap & (1 << fill_id)) {
			edma_err("Duplicate VP feat rx_fill_ring_id (%d) found at index %d\n", fill_id, i);
			return -EINVAL;
		}

		vp_feat_rxfill_bitmap |= (1 << fill_id);
	}

	if (edma_validate_host_txrx_rings(rx_info, NULL, vp_feat_rxfill_bitmap, 0)) {
		edma_err("Validating host PPE VP feature rx rings failed\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef NSS_DP_HW_GRO
/*
 * edma_validate_gro_ring_info()
 *	Validate GRO ring information
 */
static int edma_validate_gro_ring_info(void)
{
	int i;

	/* Validate GRO module parameters */
	if (edma_dp_gro_num_rxfill_rings <= 0 || edma_dp_gro_num_rxfill_rings > EDMA_MAX_RXFILL_RING_PER_TYPE) {
		edma_err("Invalid GRO rxfill rings: %d (max: %d)\n",
			 edma_dp_gro_num_rxfill_rings, EDMA_MAX_RXFILL_RING_PER_TYPE);
		return -EINVAL;
	}

	if (edma_dp_gro_num_rx_rings <= 0 || edma_dp_gro_num_rx_rings > EDMA_MAX_RXDESC_RING_PER_TYPE) {
		edma_err("Invalid GRO rx rings: %d (max: %d)\n",
			 edma_dp_gro_num_rx_rings, EDMA_MAX_RXDESC_RING_PER_TYPE);
		return -EINVAL;
	}

	if (edma_dp_gro_queues_per_ring <= 0) {
		edma_err("Invalid GRO queues per ring: %d\n", edma_dp_gro_queues_per_ring);
		return -EINVAL;
	}

	/* Validate GRO ring IDs */
	for (i = 0; i < edma_dp_gro_num_rx_rings; i++) {
		if ((edma_dp_gro_rx_rings[i] < 0) || (edma_dp_gro_rx_rings[i] >= EDMA_MAX_RXDESC_RINGS)) {
			edma_err("Invalid GRO rx ring ID at index %d: %d\n", i, edma_dp_gro_rx_rings[i]);
			return -EINVAL;
		}
	}

	/* Validate GRO queue map */
	for (i = 0; i < edma_dp_gro_num_rx_rings; i++) {
		if (edma_dp_gro_rx_queue_map[i] < 0) {
			edma_err("Invalid GRO queue map at index %d: %d\n", i, edma_dp_gro_rx_queue_map[i]);
			return -EINVAL;
		}
	}

	/* Validate GRO rxfill map */
	for (i = 0; i < edma_dp_gro_num_rxfill_rings; i++) {
		if ((edma_dp_gro_rxfill_map[i] < 0) || (edma_dp_gro_rxfill_map[i] >= EDMA_MAX_RXFILL_RINGS)) {
			edma_err("Invalid GRO rxfill map at index %d: %d\n", i, edma_dp_gro_rxfill_map[i]);
			return -EINVAL;
		}
	}

	/* Validate GRO ring size: must be non-zero and a power of 2 */
	if (!edma_dp_gro_rx_ring_sz || (edma_dp_gro_rx_ring_sz & (edma_dp_gro_rx_ring_sz - 1))) {
		edma_err("Invalid GRO rx ring size: %u (must be a non-zero power of 2)\n", edma_dp_gro_rx_ring_sz);
		return -EINVAL;
	}

	/* Validate GRO Fill ring size: must be non-zero and a power of 2 */
	if (!edma_dp_gro_rxfill_ring_sz || (edma_dp_gro_rxfill_ring_sz & (edma_dp_gro_rxfill_ring_sz - 1))) {
		edma_err("Invalid GRO rxfill ring size: %u (must be a non-zero power of 2)\n", edma_dp_gro_rxfill_ring_sz);
		return -EINVAL;
	}

	return 0;
}
#endif

#ifdef NSS_DP_PPEDS_SUPPORT
/*
 * edma_validate_ppeds_ring_info()
 *	Validate PPE-DS ring information
 */
static int edma_validate_ppeds_ring_info(void)
{
	struct edma_ds_info *ds_info = &init_info.ds_info;
	struct edma_ppeds_info *ppeds_info = &ds_info->ppeds_info;
	uint32_t rxfill_ring_bitmap = 0;
	uint32_t txcmpl_ring_bitmap = 0;
	uint32_t rx_ring_bitmap = 0;
	uint32_t tx_ring_bitmap = 0;
	int i, j;

	/*
	 * Validate each PPE-DS node
	 */
	for (i = 0; i < ppeds_info->num_nodes; i++) {
		struct edma_ppeds_node_info *node_info = &ppeds_info->node_info[i];

		/*
		 * Validate RX ring counts
		 */
		if ((node_info->num_rx_rings <= 0) || (node_info->num_rx_rings > EDMA_PPEDS_MAX_RINGS_PER_NODE)) {
			edma_err("Node %d: Invalid num_rx_rings (%d), valid range: 1-%d\n",
				i, node_info->num_rx_rings, EDMA_PPEDS_MAX_RINGS_PER_NODE);
			return -EINVAL;
		}

		/*
		 * Validate TX ring counts
		 */
		if ((node_info->num_tx_rings <= 0) || (node_info->num_tx_rings > EDMA_PPEDS_MAX_RINGS_PER_NODE)) {
			edma_err("Node %d: Invalid num_tx_rings (%d), valid range: 1-%d\n",
				i, node_info->num_tx_rings, EDMA_PPEDS_MAX_RINGS_PER_NODE);
			return -EINVAL;
		}

		/*
		 * Validate RX ring mappings
		 */
		for (j = 0; j < node_info->num_rx_rings; j++) {
			uint32_t rx_ring_id = node_info->rx_map[j].rx_ring_id;
			uint32_t rx_fill_ring_id = node_info->rx_map[j].rx_fill_ring_id;

			if ((rx_ring_id < 0) || (rx_ring_id >= EDMA_MAX_RXDESC_RINGS)) {
				edma_err("Node %d, RX ring %d: Invalid rx_ring_id (%d), max allowed: %d\n",
					i, j, rx_ring_id, EDMA_MAX_RXDESC_RINGS - 1);
				return -EINVAL;
			}

			if ((rx_fill_ring_id < 0) || (rx_fill_ring_id >= EDMA_MAX_RXFILL_RINGS)) {
				edma_err("Node %d, RX ring %d: Invalid rx_fill_ring_id (%d), max allowed: %d\n",
					i, j, rx_fill_ring_id, EDMA_MAX_RXFILL_RINGS - 1);
				return -EINVAL;
			}

			/*
			 * Check for duplicate RX ring ID across all nodes
			 */
			if (rx_ring_bitmap & (1 << rx_ring_id)) {
				edma_err("Node %d, RX ring %d: Duplicate rx_ring_id (%d)\n",
					i, j, rx_ring_id);
				return -EINVAL;
			}

			rx_ring_bitmap |= (1 << rx_ring_id);

			/*
			 * Track RX fill rings used by PPE-DS
			 */
			rxfill_ring_bitmap |= (1 << rx_fill_ring_id);
		}

		/*
		 * Validate TX ring mappings
		 */
		for (j = 0; j < node_info->num_tx_rings; j++) {
			uint32_t tx_ring_id = node_info->tx_map[j].tx_ring_id;
			uint32_t tx_cmpl_ring_id = node_info->tx_map[j].tx_cmpl_ring_id;

			if ((tx_ring_id < 0) || (tx_ring_id >= EDMA_MAX_TXDESC_RINGS)) {
				edma_err("Node %d, TX ring %d: Invalid tx_ring_id (%d), max allowed: %d\n",
					i, j, tx_ring_id, EDMA_MAX_TXDESC_RINGS - 1);
				return -EINVAL;
			}

			if ((tx_cmpl_ring_id < 0) || (tx_cmpl_ring_id >= EDMA_MAX_TXCMPL_RINGS)) {
				edma_err("Node %d, TX ring %d: Invalid tx_cmpl_ring_id (%d), max allowed: %d\n",
					i, j, tx_cmpl_ring_id, EDMA_MAX_TXCMPL_RINGS - 1);
				return -EINVAL;
			}

			/*
			 * Check for duplicate TX ring ID across all nodes
			 */
			if (tx_ring_bitmap & (1 << tx_ring_id)) {
				edma_err("Node %d, TX ring %d: Duplicate tx_ring_id (%d)\n",
					i, j, tx_ring_id);
				return -EINVAL;
			}

			tx_ring_bitmap |= (1 << tx_ring_id);

			/*
			 * Track TX completion rings used by PPE-DS
			 */
			txcmpl_ring_bitmap |= (1 << tx_cmpl_ring_id);
		}

		/*
		 * Validate number of queues per ring
		 */
		if (node_info->num_queues_per_ring <= 0) {
			edma_err("Node %d: Invalid num_queues_per_ring (%d)\n",
				i, node_info->num_queues_per_ring);
			return -EINVAL;
		}
	}

	edma_info("PPE-DS ring validation successful: %d nodes validated, rxfill_bitmap=0x%x, txcmpl_bitmap=0x%x\n",
		  ppeds_info->num_nodes, rxfill_ring_bitmap, txcmpl_ring_bitmap);
	return 0;
}
#endif

/*
 * edma_parse_ini()
 *	parse the ini file and config EDMA rings, queue, mappings, etc.
 */
static int edma_parse_ini(void)
{
	int i;

	/*
	 * TO-DO: Remove the module params and replace them with the
	 * parsing logic to fetch the information from INI file.
	 */

	/*
	 * Get the SFE host config information
	 */
	struct edma_host_info *host_info = &init_info.host_info;
	struct edma_rx_rings_info *rx_info = &host_info->sfe_info.rx_info;
	struct edma_tx_rings_info *tx_info = &host_info->sfe_info.tx_info;
	fal_portscheduler_resource_t cfg = {0};
#ifdef NSS_DP_PPEDS_SUPPORT
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	uint8_t txdesc = 0, rxdesc = 0;
#endif
#ifdef NSS_DP_HW_GRO
	struct edma_rx_rings_info *rx_gro_info;
#endif

	/*
	 * Get the queue base for host queues.
	 */
	if (fal_port_scheduler_resource_get(0, 0, &cfg) != 0) {
		edma_err("Scheduler resource failed to get\n");
		return -EINVAL;
	}

	edma_gbl_ctx->rx_queue_start = cfg.ucastq_start;
	host_info->common_info.edma_num_rxfill_rings = edma_dp_host_num_rxfill_rings;
	host_info->common_info.edma_num_txcmpl_rings = edma_dp_host_num_txcmpl_rings;

	rx_info->num_rx_rings = edma_dp_host_num_rx_rings;
	rx_info->num_queues_per_ring = edma_dp_host_queues_per_ring;

	for (i = 0; i < EDMA_MAX_RXDESC_RING_PER_TYPE; i++) {
		rx_info->rx_map[i].rx_ring_id = edma_dp_host_rx_rings[i];
		rx_info->rx_map[i].ppe_queue_base = edma_dp_host_rx_queue_map[i];
		rx_info->rx_map[i].rx_fill_ring_id = edma_dp_host_rxfill_map[i];
	}

	for (i = 0; i < EDMA_MAX_RXFILL_RING_PER_TYPE; i++) {
		host_info->common_info.edma_rxfill_ring_map[i] = edma_dp_host_rxfill_map[i];
	}

#ifdef NSS_DP_HW_GRO
	/*
	 * GRO setup need to happen after SFE rings are initialized
	 */
	if (edma_validate_gro_ring_info() < 0) {
		edma_err("GRO ring validation failed\n");
		return -EINVAL;
	}

	/*
	 * GRO setup
	 */
	edma_gbl_ctx->hw_gro_ctx.rx_gro_queue_start = edma_dp_gro_ppe_queue_base;
	edma_gbl_ctx->hw_gro_ctx.rx_gro_ring_start = edma_dp_gro_rx_rings[0];

	rx_gro_info = &host_info->gro_info.rx_info;

	/*
	 * configure GRO ctx.
	 */
	rx_gro_info->num_rx_rings = edma_dp_gro_num_rx_rings;
	rx_gro_info->num_queues_per_ring = edma_dp_gro_queues_per_ring;

	for (i = 0; i < EDMA_MAX_RXDESC_RING_PER_TYPE; i++) {
		rx_gro_info->rx_map[i].rx_ring_id = edma_dp_gro_rx_rings[i];
		rx_gro_info->rx_map[i].ppe_queue_base = edma_dp_gro_rx_queue_map[i];
		rx_gro_info->rx_map[i].rx_fill_ring_id = edma_dp_gro_rxfill_map[i];
	}
#endif

	/*
	 * Configure host TX ctx.
	 */
	tx_info->num_tx_rings = edma_dp_host_num_tx_rings;
	tx_info->max_rings_per_core = edma_dp_host_num_tx_rings_per_core;

	for (int i = 0; i < EDMA_MAX_TXDESC_RING_PER_TYPE; i++) {
		tx_info->tx_map[i].tx_ring_id = edma_dp_host_tx_rings[i];
		tx_info->tx_map[i].tx_cmpl_ring_id = edma_dp_host_txcmpl_map[i];
	}

	for (int i = 0; i < EDMA_MAX_TXCMPL_RING_PER_TYPE; i++) {
		host_info->common_info.edma_txcmpl_ring_map[i] = edma_dp_host_txcmpl_rings[i];
	}

	for (int i = 0; i < NR_CPUS; i++) {
		for (int j = 0; j < EDMA_MAX_TX_RINGS_PER_CORE; j++) {
			int c = ((i * EDMA_MAX_TX_RINGS_PER_CORE) + j);
			tx_info->tx_ring_per_core_map[i][j] = edma_dp_host_tx_ring_to_core_map[c];
		}
	}

	/*
	 * PPE VP configurations.
	 */
	tx_info = &host_info->vp_info.tx_info;

	tx_info->num_tx_rings = edma_dp_ppe_vp_num_tx_rings;
	tx_info->max_rings_per_core = edma_dp_ppe_vp_num_tx_rings_per_core;

	for (int i = 0; i < tx_info->num_tx_rings; i++) {
		tx_info->tx_map[i].tx_ring_id = edma_dp_ppe_vp_tx_rings[i];
		tx_info->tx_map[i].tx_cmpl_ring_id = edma_dp_ppe_vp_txcmpl_map[i];
	}

	for (int i = 0; i < NR_CPUS; i++) {
		for (int j = 0; j < EDMA_MAX_TX_RINGS_PER_CORE; j++) {
			int c = ((i * EDMA_MAX_TX_RINGS_PER_CORE) + j);
			tx_info->tx_ring_per_core_map[i][j] = edma_dp_ppe_vp_tx_ring_to_core_map[c];
		}
	}

	/*
	 * PPE VP feature ring configurations.
	 */
	rx_info = &host_info->vp_feat_info.rx_info;
	rx_info->num_queues_per_ring = edma_dp_host_queues_per_ring;
	rx_info->num_rx_rings = edma_dp_ppe_vp_feat_num_rings;

	for (i = 0; i < EDMA_MAX_RXDESC_RING_PER_TYPE; i++) {
		rx_info->rx_map[i].rx_ring_id = edma_dp_ppe_vp_feat_rx_rings[i];
		rx_info->rx_map[i].ppe_queue_base = edma_dp_ppe_vp_feat_rx_queue_map[i];
		rx_info->rx_map[i].rx_fill_ring_id = edma_dp_ppe_vp_feat_rxfill_map[i];
	}

	/*
	 * Validate the ring informatino received.
	 */
	if (edma_validate_host_ring_info()) {
		edma_err("Validating host rings failed\n");
		return -EINVAL;
	}

#ifdef NSS_DP_PPEDS_SUPPORT
	/*
	 * Configure ppeds parameter.
	 */
	if (drv->num_nodes > EDMA_PPEDS_MAX_NODES) {
		edma_err("Invalid number of ppeds nodes (%d), maximum possible"
				" ppeds node count is %u\n",
				drv->num_nodes,
				EDMA_PPEDS_MAX_NODES);
		return -EINVAL;
	}

	for (int j = 0; j < drv->num_nodes; j++) {
		struct edma_ds_info *ds_info = &init_info.ds_info;
		struct edma_ppeds_node_info *node_info = &ds_info->ppeds_info.node_info[j];
		int32_t num_txdesc = edma_dp_ppe_ds_num_txdesc_per_node[j];
		int32_t num_rxdesc = edma_dp_ppe_ds_num_rxdesc_per_node[j];

		if ((num_txdesc <= 0) || (num_txdesc > EDMA_PPEDS_MAX_RINGS_PER_NODE)) {
			edma_err("invalid number of txdesc per node %d\n", num_txdesc);
			return -EINVAL;
		}

		if ((num_rxdesc <= 0) || (num_rxdesc > EDMA_PPEDS_MAX_RINGS_PER_NODE)) {
			edma_err("invalid number of rxdesc per node %d\n", num_rxdesc);
			return -EINVAL;
		}

		if (txdesc >= EDMA_PPEDS_MAX_TX_RINGS || rxdesc >= EDMA_PPEDS_MAX_RX_RINGS) {
			edma_err("TXDESC %d RXDESC %d not available for PPEDS\n", txdesc, rxdesc);
			return -EINVAL;
		}

		for (int i = 0; i < num_rxdesc; i++) {
			node_info->rx_map[i].rx_ring_id = edma_dp_ppe_ds_rx_rings[rxdesc];
			node_info->rx_map[i].ppe_queue_base = edma_dp_ppe_ds_rx_queue_map[rxdesc];
			node_info->rx_map[i].rx_fill_ring_id = edma_dp_ppe_ds_rxfill_rings[rxdesc];
			node_info->num_queues_per_ring = edma_dp_ppe_ds_num_rx_queue[rxdesc];
			node_info->num_rx_rings++;
			rxdesc++;
		}

		for (int i = 0; i < num_txdesc; i++) {
			node_info->tx_map[i].tx_ring_id = edma_dp_ppe_ds_tx_rings[txdesc];
			node_info->tx_map[i].tx_cmpl_ring_id = edma_dp_ppe_ds_txcmpl_rings[txdesc];
			node_info->num_tx_rings++;
			txdesc++;
		}

		ds_info->ppeds_info.num_nodes++;
	}

	/*
	 * Validate PPE-DS ring information
	 */
	if (edma_validate_ppeds_ring_info()) {
		edma_err("Validating PPE-DS rings failed\n");
		return -EINVAL;
	}
#endif

	return 0;
}

/*
 * edma_get_ddr_size()
 *	API to calculate DDR size from device tree.
 */
static uint64_t edma_get_ddr_size(void) {
	struct resource mem;
	struct device_node *np;
	resource_size_t mem_size = 0;
	int i = 0;

	np = of_find_node_by_type(NULL, "memory");
	if (!np) {
		edma_err("Failed to read memory node from device tree\n");
		return 0;
	}

	while (of_address_to_resource(np, i, &mem) == 0) {
		mem_size += resource_size(&mem);
		i++;
	}

	edma_info("DDR size is %lld\n", mem_size);
	return (uint64_t)mem_size;
}

/*
 * edma_get_dma_mask()
 *	API to calculate DMA mask from DDR size.
 */
static uint64_t edma_get_dma_mask(uint64_t ddr_size)
{
 	if (ddr_size <= EDMA_DEFAULT_DDR_SIZE)
		return (uint64_t)DMA_BIT_MASK(EDMA_DEFAULT_DMA_MASK_BIT_HI);

	return (uint64_t)DMA_BIT_MASK(EDMA_MAX_DMA_MASK_BIT_HI);
}

/*
 * edma_of_get_pdata()
 *	Read the device tree details for EDMA
 */
static int edma_of_get_pdata(struct resource *edma_res)
{
	struct platform_device *pdev;
	uint64_t mem_size, mask;
	int ret;
#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	bool ddr_ext_upstream;
	uint32_t loopback_feature_type = 0;
#endif

	/*
	 * Find EDMA node in device tree
	 */
	edma_gbl_ctx->device_node = of_find_node_by_name(NULL,
				EDMA_DEVICE_NODE_NAME);
	if (!edma_gbl_ctx->device_node) {
		edma_err("EDMA device tree node (%s) not found\n",
				EDMA_DEVICE_NODE_NAME);
		return -EINVAL;
	}

	/*
	 * Get EDMA device node
	 */
	pdev = edma_gbl_ctx->pdev = of_find_device_by_node(edma_gbl_ctx->device_node);
	if (!edma_gbl_ctx->pdev) {
		edma_err("Platform device for node %px(%s) not found\n",
				edma_gbl_ctx->device_node,
				(edma_gbl_ctx->device_node)->name);
		return -EINVAL;
	}

	/*
	 * Get EDMA register resource
	 */
	if (of_address_to_resource(edma_gbl_ctx->device_node, 0, edma_res) != 0) {
		edma_err("Unable to get register address for edma device: "
			  EDMA_DEVICE_NODE_NAME"\n");
		return -EINVAL;
	}

	/*
	 * Check the DDR size. This is populated by the bootloader.
	 * Starting from IPQ5424 onwards, DDR size beyond 3GB is supported and we need
	 * to configure EDMA based on the DDR size.
	 */
	edma_gbl_ctx->mem_size = mem_size = edma_get_ddr_size();
	if (!mem_size) {
		edma_err("Failed to get the Memory size\n");
		return -ENOMEM;
	}

	/*
	 * Get the DMA mask to be programmed.
	 */
	mask = edma_get_dma_mask(mem_size);
	pr_info("DDR size: %llx, DMA mask is %llx\n", mem_size, mask);
	if (dma_set_mask(&pdev->dev, mask)) {
		edma_err("dma_set_mask_and_coherent failed for mask (%llx)\n", mask);
		return -ENOMEM;
	}

	/*
	 * Get the Maximum number of RX desc rings
	 */
	if (of_property_read_u32(edma_gbl_ctx->device_node, "qcom,rxdesc-ring-max",
                               &edma_gbl_ctx->rxdesc_ring_max) != 0) {
               edma_err("Unable to read the Max RX desc rings \n");
		return -EINVAL;
        }

	edma_err("RX desc ring max: %d\n", edma_gbl_ctx->rxdesc_ring_max);

	/*
	 * Get the Maximum number of RX fill rings
	 */
	if (of_property_read_u32(edma_gbl_ctx->device_node, "qcom,rxfill-ring-max",
                               &edma_gbl_ctx->rxfill_ring_max) != 0) {
               edma_err("Unable to read the Max RX fill rings \n");
		return -EINVAL;
        }

	 edma_err("RX fill ring max: %d\n", edma_gbl_ctx->rxfill_ring_max);

	/*
	 * Get the Maximum number of TX desc rings
	 */
	if (of_property_read_u32(edma_gbl_ctx->device_node, "qcom,txdesc-ring-max",
                               &edma_gbl_ctx->txdesc_ring_max) != 0) {
               edma_err("Unable to read the Max TX desc rings \n");
		return -EINVAL;
        }

	edma_err("TX desc ring max: %d\n", edma_gbl_ctx->txdesc_ring_max);

	/*
	 * Get the Maximum number of TX cmpl rings
	 */
	if (of_property_read_u32(edma_gbl_ctx->device_node, "qcom,txcomp-ring-max",
                               &edma_gbl_ctx->txcmpl_ring_max) != 0) {
               edma_err("Unable to read the Max TX comp rings \n");
		return -EINVAL;
        }

	edma_err("TX completion ring max: %d\n", edma_gbl_ctx->txcmpl_ring_max);

#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (dp_global_ctx.is_mht_dev) {
		if (of_property_read_u32(edma_gbl_ctx->device_node,
					"qcom,mht-txdesc-rings",
					&edma_gbl_ctx->mht_tx_ports) != 0) {
			edma_err("Unable to read number of mht txdesc rings.\n");
			return -EINVAL;
		}
		edma_gbl_ctx->num_txdesc_rings += edma_gbl_ctx->mht_tx_ports;
	} else {
		edma_gbl_ctx->mht_tx_ports = 0;
	}

	if (dp_global_ctx.is_mht_dev) {
		if (of_property_read_u32(edma_gbl_ctx->device_node,
					"qcom,mht-txcmpl-rings",
					&edma_gbl_ctx->mht_txcmpl_ports) != 0) {
			edma_err("Unable to read number of mht txcmpl rings.\n");
			return -EINVAL;
		}
		edma_gbl_ctx->num_txcmpl_rings += edma_gbl_ctx->mht_txcmpl_ports;
	} else {
		edma_gbl_ctx->mht_txcmpl_ports = 0;
	}
#endif

	/*
	 * Get page_mode of RXFILL rings
	 * TODO: Move this setting to DP common node
	 */
	if (edma_gbl_ctx->mem_profile & NSS_DP_MEM_PROFILE_HIGH)
		of_property_read_u32(edma_gbl_ctx->device_node, "qcom,rx-page-mode",
						&edma_gbl_ctx->rx_page_mode);

	/*
	 * Get Tx Map priority level
	 */
	if (of_property_read_u32(edma_gbl_ctx->device_node, "qcom,tx-map-priority-level",
					&edma_gbl_ctx->tx_priority_level) != 0) {
		edma_err("Unable to read Tx map priority level.\n");
		return -EINVAL;
	}

	if (edma_gbl_ctx->tx_priority_level > EDMA_TX_MAX_PRIORITY_LEVEL) {
		edma_err("Invalid tx priority value (%d), maximum possible"
				" priority value is %u\n", edma_gbl_ctx->tx_priority_level,
				EDMA_TX_MAX_PRIORITY_LEVEL);
		return -EINVAL;
	}
	edma_debug("tx map priority level: %d\n", edma_gbl_ctx->tx_priority_level);

	/*
	 * Get Rx Map priority level
	 */
	if (of_property_read_u32(edma_gbl_ctx->device_node,
					"qcom,rx-map-priority-level",
					&edma_gbl_ctx->rx_priority_level) != 0) {
		edma_err("Unable to read Rx map priority level.\n");
		return -EINVAL;
	}

	if (edma_gbl_ctx->rx_priority_level > EDMA_RX_MAX_PRIORITY_LEVEL) {
		edma_err("Invalid tx priority value (%d), maximum possible"
				" priority value is %u\n",
				edma_gbl_ctx->rx_priority_level,
				EDMA_RX_MAX_PRIORITY_LEVEL);
		return -EINVAL;
	}
	edma_debug("rx map priority level: %d\n",
				edma_gbl_ctx->rx_priority_level);

	/*
	 * Get TXDESC flow control Group ID Map
	 */
	ret = of_property_read_u32_array(edma_gbl_ctx->device_node,
			"qcom,txdesc-fc-grp-map",
			(int32_t *)edma_gbl_ctx->tx_fc_grp_map, EDMA_MAX_FC_GRP);
	if (ret) {
		edma_err("Unable to read TxDesc-Fc-Grp map array. \
				ret: %d\n", ret);
			return -EINVAL;
	}

#if defined(NSS_DP_POINT_OFFLOAD)
	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,txdesc_point_offload_ring", &edma_gbl_ctx->txdesc_point_offload_ring);
	if (ret) {
		edma_err("Unable to parse Tx point offload ring with err: %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,txcmpl_point_offload_ring", &edma_gbl_ctx->txcmpl_point_offload_ring);
	if (ret) {
		edma_err("Unable to read Tx completion point offload ring with err: %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,rxfill_point_offload_ring", &edma_gbl_ctx->rxfill_point_offload_ring);
	if (ret) {
		edma_err("Unable to read RX fill point offload ring with err: %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,rxdesc_point_offload_ring", &edma_gbl_ctx->rxdesc_point_offload_ring);
	if (ret) {
		edma_err("Unable to read RX desc point offload ring with err: %d\n", ret);
		return -EINVAL;
	}

#elif defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	/*
	 * Check if loopback ring enabled
	 */
	if (edma_loopback_feature_type == PPE_DRV_LOOPBACK_FEATURE_TYPE_DISABLED) {
		goto skip_loopback;
	}

	/*
	 * Check in dts file whether any loopback ring feature enabled by default.
	 */
	if (edma_loopback_feature_type == PPE_DRV_LOOPBACK_FEATURE_TYPE_DEFAULT) {
		ddr_ext_upstream = of_property_read_bool(edma_gbl_ctx->device_node, "qcom,loopback_ext_ddr_upstream");
		if (ddr_ext_upstream) {
			loopback_feature_type |= PPE_DRV_LOOPBACK_FEATURE_TYPE_EXT_DDR_UPSTREAM;
		}
	}

	if (edma_loopback_feature_type == PPE_DRV_LOOPBACK_FEATURE_TYPE_EXT_DDR_UPSTREAM) {
		loopback_feature_type |= PPE_DRV_LOOPBACK_FEATURE_TYPE_EXT_DDR_UPSTREAM;
	}

	if (edma_loopback_feature_type == PPE_DRV_LOOPBACK_FEATURE_TYPE_GRETAP_MAPT) {
		loopback_feature_type |= PPE_DRV_LOOPBACK_FEATURE_TYPE_GRETAP_MAPT;
	}

	if (edma_loopback_feature_type == PPE_DRV_LOOPBACK_FEATURE_TYPE_V6_HAIRPIN_NAT) {
		loopback_feature_type |= PPE_DRV_LOOPBACK_FEATURE_TYPE_V6_HAIRPIN_NAT;
	}

	if (loopback_feature_type == 0) {
		goto skip_loopback;
	}

	/*
	 * We dont support more than one loopback ring features at a time.
	 */
	if (!((loopback_feature_type & ~(loopback_feature_type - 1)) == loopback_feature_type)) {
		edma_err("loopback ring feature not supported: %u %u\n", loopback_feature_type, edma_loopback_feature_type);
		return -EINVAL;
	}

	edma_gbl_ctx->loopback_en = true;
	edma_gbl_ctx->loopback_feature_type = loopback_feature_type;

	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,num_loopback_rings", &edma_gbl_ctx->num_loopback_rings);
	if (ret) {
		edma_err("Unable to read edma loopback buf size with err: %d\n", ret);
		return -EINVAL;
	}

	edma_gbl_ctx->txdesc_loopback_ring_id_arr = kmalloc(sizeof(uint8_t) * edma_gbl_ctx->num_loopback_rings, GFP_KERNEL);
	if (!edma_gbl_ctx->txdesc_loopback_ring_id_arr) {
		edma_err("Unable to allocate memory for txdesc loopback ring_id\n");
		return -EINVAL;
	}

	edma_gbl_ctx->txcmpl_loopback_ring_id_arr = kmalloc(sizeof(uint8_t) * edma_gbl_ctx->num_loopback_rings, GFP_KERNEL);
	if (!edma_gbl_ctx->txcmpl_loopback_ring_id_arr) {
		edma_err("Unable to allocate memory for txcmpl loopback ring_id\n");
		kfree(edma_gbl_ctx->txdesc_loopback_ring_id_arr);
		return -EINVAL;
	}

	edma_gbl_ctx->rxdesc_loopback_ring_id_arr = kmalloc(sizeof(uint8_t) * edma_gbl_ctx->num_loopback_rings, GFP_KERNEL);
	if (!edma_gbl_ctx->rxdesc_loopback_ring_id_arr) {
		edma_err("Unable to allocate memory for rxdesc loopback ring_id\n");
		kfree(edma_gbl_ctx->txdesc_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txcmpl_loopback_ring_id_arr);
		return -EINVAL;
	}

	edma_gbl_ctx->rxfill_loopback_ring_id_arr = kmalloc(sizeof(uint8_t) * edma_gbl_ctx->num_loopback_rings, GFP_KERNEL);
	if (!edma_gbl_ctx->rxfill_loopback_ring_id_arr) {
		edma_err("Unable to allocate memory for rxfill loopback ring_id\n");
		kfree(edma_gbl_ctx->rxdesc_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txcmpl_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txdesc_loopback_ring_id_arr);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(edma_gbl_ctx->device_node, "qcom,txdesc_loopback_ring_id", (int32_t *)edma_gbl_ctx->txdesc_loopback_ring_id_arr, edma_gbl_ctx->num_loopback_rings);
	if (ret) {
		edma_err("Unable to read txdesc loopback_ring_id map array. ret: %d\n", ret);
		goto fail;
	}

	ret = of_property_read_u32_array(edma_gbl_ctx->device_node, "qcom,txcmpl_loopback_ring_id", (int32_t *)edma_gbl_ctx->txcmpl_loopback_ring_id_arr, edma_gbl_ctx->num_loopback_rings);
	if (ret) {
		edma_err("Unable to read txcmpl loopback_ring_id map array. ret: %d\n", ret);
		goto fail;
	}

	ret = of_property_read_u32_array(edma_gbl_ctx->device_node, "qcom,rxdesc_loopback_ring_id", (int32_t *)edma_gbl_ctx->rxdesc_loopback_ring_id_arr, edma_gbl_ctx->num_loopback_rings);
	if (ret) {
		edma_err("Unable to read rxdesc loopback_ring_id map array. ret: %d\n", ret);
		goto fail;
	}

	ret = of_property_read_u32_array(edma_gbl_ctx->device_node, "qcom,rxfill_loopback_ring_id", (int32_t *)edma_gbl_ctx->rxfill_loopback_ring_id_arr, edma_gbl_ctx->num_loopback_rings);
	if (ret) {
		edma_err("Unable to read rxfill loopback_ring_id map array. ret: %d\n", ret);
		goto fail;
	}

	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,loopback_queue_base", &edma_gbl_ctx->loopback_queue_base);
	if (ret) {
		edma_err("Unable to read loopback_queue_base ret: %d\n", ret);
		goto fail;
	}

	ret = of_property_read_u32(edma_gbl_ctx->device_node, "qcom,loopback_num_queues", &edma_gbl_ctx->loopback_num_queues);
	if (ret) {
		edma_err("Unable to read loopback_queue_num_queues ret: %d\n", ret);
		goto fail;
	}

skip_loopback:
#endif

#ifdef NSS_DP_PPEDS_SUPPORT
	edma_gbl_ctx->ppeds_drv.num_nodes = EDMA_PPEDS_MAX_NODES;
	edma_debug("PPE-DS num nodes: %d\n", edma_gbl_ctx->ppeds_drv.num_nodes);
#endif

#if defined(NSS_DP_POINT_OFFLOAD)
	if (edma_gbl_ctx->ppeds_drv.num_nodes == EDMA_PPEDS_MAX_NODES) {
		/*
		 * Only enable EDMA_PPEDS_MAX_NODES - 1 PPE-DS nodes when the
		 * point offload feature is enabled (because one pair of EDMA
		 * Rx/Tx rings will be shared between the point offload feature
		 * and the PPE-DS feature).
		 */
		edma_gbl_ctx->ppeds_drv.num_nodes = EDMA_PPEDS_MAX_NODES - 1;
		edma_warn("Error: PPE node count is %d when point offload is enabled.",
				 EDMA_PPEDS_MAX_NODES);
	}
#elif defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (edma_gbl_ctx->loopback_en && (edma_gbl_ctx->ppeds_drv.num_nodes == EDMA_PPEDS_MAX_NODES)) {
		edma_err("Error: PPE node count is %d when loopback is enabled.",
				 EDMA_PPEDS_MAX_NODES);
		goto fail;
	}
#endif

	return 0;

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
fail:
	if (edma_gbl_ctx->loopback_en) {
		kfree(edma_gbl_ctx->rxfill_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->rxdesc_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txcmpl_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txdesc_loopback_ring_id_arr);
	}
#endif

	return -EINVAL;
}

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
/*
 * edma_hw_loopback_init()
 *	Register configuration for loopback ring
 */
static void edma_hw_loopback_init(struct edma_gbl_ctx *egc)
{
	uint32_t data;
	int i = 0;

	for (i = 0; i < egc->num_loopback_rings; i++) {
		int ring_id = egc->rxfill_loopback_ring_id_arr[i];
		data = edma_reg_read(EDMA_REG_RXFILL_RING_SIZE(ring_id));
#ifdef NSS_DP_EDMA_LOOPBACK_BUF_CONFIG
		data |= egc->loopback_buf_size;
#else
		data |= (egc->loopback_buf_size << 16);
#endif
		edma_reg_write(EDMA_REG_RXFILL_RING_SIZE(ring_id), data);

		/*
		 * Generate TXdesc and RXfill ring id map
		 */
		data = EDMA_REG_LOOPBACK_CMPL(ring_id);

		ring_id = egc->txdesc_loopback_ring_id_arr[i];
		data |= EDMA_REG_LOOPBACK_DESC(ring_id);

		edma_reg_write(EDMA_REG_TXCMPL_CTRL(ring_id), 0x1);
		edma_reg_write(EDMA_REG_LOOPBACK_CTRL, data);
	}
}

/*
 * edma_alloc_loopback_ring()
 *	Allocate and initialize EDMA rings
 */
static int edma_alloc_loopback_ring(struct edma_gbl_ctx *egc)
{
	if (edma_cfg_tx_loopback_rings_alloc(egc)) {
		edma_err("Error in allocating tx rings\n");
		return -ENOMEM;
	}

	if (edma_cfg_rx_loopback_rings_alloc(egc)) {
		edma_err("Error in allocating rx rings\n");
		goto rx_rings_alloc_fail;
	}

	return 0;

rx_rings_alloc_fail:
	edma_cfg_tx_loopback_rings_cleanup(egc);
	return -ENOMEM;
}
#endif

/*
 * edma_alloc_rings()
 *	Allocate and initialize EDMA rings
 */
static int edma_alloc_and_setup_rings(struct edma_gbl_ctx *egc)
{
	if (edma_cfg_tx_rings_alloc(egc)) {
		edma_err("Error in allocating tx rings\n");
		goto tx_alloc_fail;
	}

	if (edma_cfg_rx_rings_alloc(egc)) {
		edma_err("Error in allocating rx rings\n");
		goto rx_alloc_fail;
	}

	if (nss_dp_hal_cache_info_setup(egc)) {
		edma_err("Error in writing data into the cache registers\n");
		goto rx_alloc_fail;
	}

	return 0;

rx_alloc_fail:
	edma_cfg_rx_rings_cleanup(egc);
tx_alloc_fail:
	edma_cfg_tx_rings_cleanup(egc);
	return -ENOMEM;
}

/*
 * edma_hw_reset()
 *	Reset EDMA Hardware during initialization
 */
static inline int edma_hw_reset(struct edma_gbl_ctx *egc)
{
	int hw_reset = 0;

	/*
	 * Soc Specific Reset
	 */
	hw_reset = nss_dp_hal_hw_reset(egc->pdev);

	if (hw_reset == 0) {
		edma_info("EDMA HW Reset completed succesfully\n");
	}
	return hw_reset;
}

/*
 * edma_configure_ucast_prio_map_tbl()
 *	Configure unicast priority map table
 *
 * Map int_priority values to priority class and initialize
 * unicast priority map table for default profile_id.
 */
static sw_error_t edma_configure_ucast_prio_map_tbl(void)
{
	uint8_t pri_class;
	uint8_t int_pri;
	sw_error_t ret;

	/*
	 * Set the priority class value for every possible priority.
	 */
	for (int_pri = 0; int_pri < EDMA_PRI_MAX; int_pri++) {
		pri_class = nss_dp_pri_map[int_pri];

		/*
		 * Priority offset should be less than maximum supported queue priority
		 */
		if (pri_class > (EDMA_MAX_PRI_PER_CORE - 1)) {
			edma_err("Configured incorrect priority offset: %d\n", pri_class);
			return SW_BAD_PARAM;
		}

		ret = fal_ucast_priority_class_set(EDMA_SWITCH_DEV_ID, EDMA_CPU_PORT_PROFILE_ID, int_pri, pri_class);
		if (unlikely(ret != SW_OK)) {
			edma_err("Failed with error: %d to set queue priority class for int_pri: %d for profile_id: %d\n",
				  ret, int_pri, EDMA_CPU_PORT_PROFILE_ID);
			return ret;
		}

		edma_info("profile_id: %d, int_priority: %d, pri_class: %d\n", EDMA_CPU_PORT_PROFILE_ID, int_pri, pri_class);
	}

	return ret;
}

/*
 * edma_fetch_mitigation_timer_rate()
 *	Fetch the EDMA mitigation timer rate
 */
static inline void edma_fetch_mitigation_timer_rate(struct edma_gbl_ctx *egc, const char *id)
{
	struct clk *clk = NULL;

	clk = of_clk_get_by_name(egc->device_node, id);
	if (IS_ERR(clk)) {
		edma_err("Error in fetching %s clock reference. Setting the mitigation"
				" timer rate to zero (means, mitigation will be disabled)\n", id);
		egc->edma_timer_rate = 0;
		return;
	}

	egc->edma_timer_rate = clk_get_rate(clk) / MHZ;
	edma_debug("%s clock's rate: %u\n", id, egc->edma_timer_rate);
}

/*
 * edma_configure_rps_hash_map()
 *	Configure RPS hash map
 *
 * Map all possible hash values to queues used by the EDMA Rx
 * rings based on a bitmask, which represents the cores to be mapped.
 * These queues are expected to be mapped to different Rx rings
 * which are assigned to different cores using IRQ affinity configuration.
 */
void edma_configure_rps_hash_map(struct edma_gbl_ctx *egc)
{
	cpumask_t edma_rps_cpumask = {{edma_cfg_rx_rps_bitmap_cores}};
	uint32_t q_map[NR_CPUS] = {0};
	uint32_t hash, cpu;
	uint32_t q_off = 0;
	int map_len = 0;
	int idx = 0;

	for_each_cpu(cpu, &edma_rps_cpumask) {
		q_off = init_info.host_info.sfe_info.rx_info.rx_map[cpu].ppe_queue_base;
		if (q_off == -1)
			break;

		q_map[map_len] = q_off;
		map_len++;
	}

	/*
	 * Based on set bit positon we have selected cores.
	 * Initialize the store
	 */
	for (hash = 0; hash < EDMA_RSS_HASH_MAX; hash++) {
		fal_ucast_hash_map_set(0, EDMA_CPU_PORT_PROFILE_ID, hash, q_map[idx]);
		edma_info("profile_id: %u, hash: %u, q_off: %u\n",
				EDMA_CPU_PORT_PROFILE_ID, hash, q_map[idx]);
		idx = (idx + 1) % map_len;
	}
}

/*
 * edma_configure_mirror_pkt_capture_core()
 *	Configure capture core for mirrored packets.
 */
void edma_configure_mirror_pkt_capture_core(uint8_t core_id, void *app_data)
{
	edma_cfg_rx_mcast_qid_to_core_mapping(edma_gbl_ctx, core_id);
}

/*
 * edma_cfg_ring_to_backpressure_mapping()
 *      Map txcmpl, rxdesc & rxfill ring_id to backpressure counter index after adding offset.
 */
static void edma_cfg_ring_to_backpressure_mapping(void)
{
        uint8_t mapped_ring_id;
        uint8_t ring_id;
        for (uint8_t i = 0; i < EDMA_REG_PER_RING_TYPE_BP_COUNTER_MAX; i++) {
                /*
                 * Map txcmpl rings to BP idx 0-5
                 */
                ring_id = edma_bp_stats_en_txcmpl_rings[i];
                if (ring_id >= EDMA_MAX_TXCMPL_RINGS) {
                        pr_err("Invalid txcmpl ring_id: %u, hence mapping ring_id = 0\n", ring_id);
                        ring_id = 0;
                }
                mapped_ring_id = ring_id + EDMA_REG_BP_TXCMPL_RING_ID_OFFSET;
                edma_reg_write(EDMA_REG_DBG_CNT_PORT_MAP(i + EDMA_REG_TXCMPL_BP_IDX_OFFSET), mapped_ring_id & EDMA_DBG_CNT_PORT_MAP_VAL_MASK);

                /*
                 * Map rxdesc rings to BP idx 6-11
                 */
                ring_id = edma_bp_stats_en_rxdesc_rings[i];
                if (ring_id >= EDMA_MAX_RXDESC_RINGS) {
                        pr_err("Invalid rxdesc ring_id: %u, hence mapping ring_id = 0\n", ring_id);
                        ring_id = 0;
                }
                mapped_ring_id = ring_id + EDMA_REG_BP_RXDESC_RING_ID_OFFSET;
                edma_reg_write(EDMA_REG_DBG_CNT_PORT_MAP(i + EDMA_REG_RXDESC_BP_IDX_OFFSET), mapped_ring_id & EDMA_DBG_CNT_PORT_MAP_VAL_MASK);

                /*
                 * Map rxfill rings to BP idx 12-17
                 */
                ring_id = edma_bp_stats_en_rxfill_rings[i];
                if (ring_id >= EDMA_MAX_RXFILL_RINGS) {
                        pr_err("Invalid rxfill ring_id: %u, hence mapping ring_id = 0\n", ring_id);
                        ring_id = 0;
                }
                mapped_ring_id = ring_id + EDMA_REG_BP_RXFILL_RING_ID_OFFSET;
                edma_reg_write(EDMA_REG_DBG_CNT_PORT_MAP(i + EDMA_REG_RXFILL_BP_IDX_OFFSET), mapped_ring_id & EDMA_DBG_CNT_PORT_MAP_VAL_MASK);
        }

}

/*
 * edma_init_rxfill_rings()
 *	Initialize the RX fill rings in global ring structure.
 */
static void edma_init_rxfill_rings(struct edma_gbl_ctx *egc,
				uint32_t *rxfill_ring_map,
				int num_rxfill_rings,
				edma_ring_types_t ring_type,
				uint32_t type_flags,
				uint32_t desc_count,
				int32_t alloc_size,
				int32_t buf_len,
				bool page_mode)
{
	struct edma_rxfill_ring_info *rxfill_info = egc->rxfill_info;
	int i, ring_id;

	if (type_flags & EDMA_RING_TYPE_FLAGS_PAGE_POOL) {
		page_mode = false;
		alloc_size = SKB_HEAD_ALIGN(alloc_size);
	}

	for (i = 0; i < num_rxfill_rings; i++) {
		ring_id = rxfill_ring_map[i];
		rxfill_info[ring_id].ring_type = ring_type;
		rxfill_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		rxfill_info[ring_id].type_flags |= type_flags;
		rxfill_info[ring_id].desc_count = desc_count;
		rxfill_info[ring_id].buffer_len = buf_len;
		rxfill_info[ring_id].alloc_size = alloc_size;
		rxfill_info[ring_id].page_mode = page_mode;
	}
}

/*
 * edma_init_rxdesc_rings()
 *	Initialize the RX descriptor rings in global ring structure.
 */
static void edma_init_rxdesc_rings(struct edma_gbl_ctx *egc,
				struct edma_rx_per_ring_map *rx_map,
				int num_rx_rings,
				edma_ring_types_t ring_type,
				uint32_t type_flags,
				uint32_t desc_count,
				uint32_t num_queues_per_ring)
{
	struct edma_rxdesc_ring_info *rxdesc_info = egc->rxdesc_info;
	int i, ring_id;

	for (i = 0; i < num_rx_rings; i++) {
		ring_id = rx_map[i].rx_ring_id;
		rxdesc_info[ring_id].ring_type = ring_type;
		rxdesc_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		rxdesc_info[ring_id].type_flags |= type_flags;
		rxdesc_info[ring_id].rxfill_ring_id = rx_map[i].rx_fill_ring_id;
		rxdesc_info[ring_id].ppe_queue_base = rx_map[i].ppe_queue_base;
		rxdesc_info[ring_id].ppe_num_queues = num_queues_per_ring;
		rxdesc_info[ring_id].desc_count = desc_count;
	}
}

/*
 * edma_init_txdesc_rings()
 *	Initialize the TX descriptor rings in global ring structure.
 */
static void edma_init_txdesc_rings(struct edma_gbl_ctx *egc,
				struct edma_tx_per_ring_map *tx_map,
				int num_tx_rings,
				edma_ring_types_t ring_type,
				uint32_t type_flags,
				uint32_t desc_count)
{
	struct edma_txdesc_ring_info *txdesc_info = egc->txdesc_info;
	int i, ring_id;

	for (i = 0; i < num_tx_rings; i++) {
		ring_id = tx_map[i].tx_ring_id;
		txdesc_info[ring_id].ring_type = ring_type;
		txdesc_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		txdesc_info[ring_id].type_flags |= type_flags;
		txdesc_info[ring_id].txcmpl_ring_id = tx_map[i].tx_cmpl_ring_id;
		txdesc_info[ring_id].desc_count = desc_count;
	}
}

/*
 * edma_init_txcmpl_rings()
 *	Initialize the TX completion rings in global ring structure.
 */
static void edma_init_txcmpl_rings(struct edma_gbl_ctx *egc,
				uint32_t *txcmpl_ring_map,
				int num_txcmpl_rings,
				edma_ring_types_t ring_type,
				uint32_t type_flags,
				uint32_t desc_count)
{
	struct edma_txcmpl_ring_info *txcmpl_info = egc->txcmpl_info;
	int i, ring_id;

	for (i = 0; i < num_txcmpl_rings; i++) {
		ring_id = txcmpl_ring_map[i];
		txcmpl_info[ring_id].ring_type = ring_type;
		txcmpl_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		txcmpl_info[ring_id].type_flags |= type_flags;
		txcmpl_info[ring_id].desc_count = desc_count;
	}
}

/*
 * edma_fill_host_rings_info()
 *	Mark the host rings in global ring structure.
 *	Also, store the information related to a particular ring into
 *	the global structure that will be used later in setup / data path.
 */
void edma_fill_host_rings_info(struct edma_gbl_ctx *egc, struct edma_init_info *init_info)
{
#ifdef NSS_DP_HW_GRO
	struct edma_rx_rings_info *gro_rx_rings = &init_info->host_info.gro_info.rx_info;
#endif
	struct edma_rx_rings_info *rx_rings = &init_info->host_info.sfe_info.rx_info;
	struct edma_tx_rings_info *tx_rings = &init_info->host_info.sfe_info.tx_info;
	struct edma_host_info *host_info = &init_info->host_info;
	int num_tx_rings, num_txcmpl_rings;
	int num_rx_rings, num_rxfill_rings;
	int32_t alloc_size, buf_len = 0, i;

	/*
	 * Set buffer allocation size
	 */
	if (egc->rx_jumbo_mru) {
		alloc_size = egc->rx_jumbo_mru + EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN;
		buf_len = alloc_size - EDMA_RX_SKB_HEADROOM - NET_IP_ALIGN;
	} else if (egc->rx_page_mode) {
		alloc_size = EDMA_RX_PAGE_MODE_SKB_SIZE + EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN;
		buf_len = PAGE_SIZE;
	} else {
		alloc_size = dp_global_ctx.rx_buf_size;
		buf_len = alloc_size - EDMA_RX_SKB_HEADROOM - NET_IP_ALIGN;
	}

	/*
	 * Mark RX fill rings
	 */
	num_rxfill_rings = host_info->common_info.edma_num_rxfill_rings;
	edma_init_rxfill_rings(egc, host_info->common_info.edma_rxfill_ring_map,
				num_rxfill_rings, EDMA_RING_TYPE_HOST,
				EDMA_RING_TYPE_FLAGS_HOST_COMMON, edma_dp_host_rxfill_ring_sz,
				alloc_size, buf_len, egc->rx_page_mode);

	/*
	 * Mark TX completion rings
	 */
	num_txcmpl_rings = host_info->common_info.edma_num_txcmpl_rings;
	edma_init_txcmpl_rings(egc, host_info->common_info.edma_txcmpl_ring_map,
				num_txcmpl_rings, EDMA_RING_TYPE_HOST,
				EDMA_RING_TYPE_FLAGS_HOST_COMMON, edma_dp_host_txcmpl_ring_sz);

	/*
	 * Mark the Host SFE TX and RX rings into the global TX RX rings pool.
	 */
	num_rx_rings = rx_rings->num_rx_rings;
	edma_init_rxdesc_rings(egc, rx_rings->rx_map, num_rx_rings,
				EDMA_RING_TYPE_HOST, EDMA_RING_TYPE_FLAGS_HOST_COMMON,
				edma_dp_host_rx_ring_sz, rx_rings->num_queues_per_ring);

	num_tx_rings = tx_rings->num_tx_rings;
	edma_init_txdesc_rings(egc, tx_rings->tx_map, num_tx_rings,
				EDMA_RING_TYPE_HOST, EDMA_RING_TYPE_FLAGS_HOST_COMMON,
				edma_dp_host_tx_ring_sz);

	/*
	 * Mark PPEVP rings.
	 */
	tx_rings = &host_info->vp_info.tx_info;

	/*
	 * Mark PPEVP TX descriptor rings
	 */
	edma_init_txdesc_rings(egc, tx_rings->tx_map, tx_rings->num_tx_rings,
				EDMA_RING_TYPE_HOST, EDMA_RING_TYPE_FLAGS_HOST_VP,
				edma_dp_ppe_vp_tx_ring_sz);

	/*
	 * Mark the GRO RX rings into the global RX rings pool.



	 * GRO rings use a fixed page pool buffer size.
	 * TODO: Parameters will be extracted from GRO structure rather than
	 * module param
	 */
	alloc_size = EDMA_RX_GRO_BUFFER_SIZE;
	buf_len = alloc_size - EDMA_RX_SKB_HEADROOM - NET_IP_ALIGN;

	edma_init_rxfill_rings(egc, edma_dp_gro_rxfill_map,
				edma_dp_gro_num_rxfill_rings, EDMA_RING_TYPE_HOST,
				EDMA_RING_TYPE_FLAGS_HOST_GRO | EDMA_RING_TYPE_FLAGS_PAGE_POOL,
				edma_dp_gro_rxfill_ring_sz,
				alloc_size, buf_len, egc->rx_page_mode);

	/*
	 * TODO: EDMA_RING_TYPE_FLAGS_PAGE_POOL to be added for rxdesc as well.
	 * This requires fixing multiple switch cases using the type_flags.
	 */
	edma_init_rxdesc_rings(egc, gro_rx_rings->rx_map, edma_dp_gro_num_rx_rings,
				EDMA_RING_TYPE_HOST, EDMA_RING_TYPE_FLAGS_HOST_GRO,
				edma_dp_gro_rx_ring_sz, gro_rx_rings->num_queues_per_ring);

	/*
	 * TO-DO: Further for other host rings like VP host rings, SMD host rings,
	 * simply mark them into the global pool so that these will be initialized and setup
	 * at once. This makes it easy to add/delete a new type of host ring.
	 */

	/*
	 * TO-DO: Further for other host rings like VP host rings, SMD host rings,
	 * simply mark them into the global pool so that these will be initialized and setup
	 * at once. This makes it easy to add/delete a new type of host ring.
	 */

	/*
	 * Mark PPE VP feature RX and RXFILL rings.
	 * These rings use the same buffer parameters as host rings since they
	 * receive the same packet types, but are tagged HOST_VP_FEAT to enable
	 * feature-specific (e.g. CAPWAP) processing in the data path.
	 */
	rx_rings = &host_info->vp_feat_info.rx_info;
	if (!(rx_rings->num_rx_rings)) {
		edma_info("VP feature ring not enabled\n");
		return;
	}

	edma_init_rxfill_rings(egc, edma_dp_ppe_vp_feat_rxfill_map,
				rx_rings->num_rx_rings, EDMA_RING_TYPE_HOST,
				EDMA_RING_TYPE_FLAGS_HOST_VP_FEAT,
				edma_dp_ppe_vp_feat_rxfill_ring_sz,
				alloc_size, buf_len, egc->rx_page_mode);

	edma_init_rxdesc_rings(egc, rx_rings->rx_map, rx_rings->num_rx_rings,
				EDMA_RING_TYPE_HOST, EDMA_RING_TYPE_FLAGS_HOST_VP_FEAT,
				edma_dp_ppe_vp_feat_rx_ring_sz, rx_rings->num_queues_per_ring);

	/*
	 * Maps ring to specific feature type.
	 */
	for (i = 0; i < rx_rings->num_rx_rings; i++) {
		uint32_t ring_id = rx_rings->rx_map[i].rx_ring_id;

		egc->rxdesc_info[ring_id].vp_feat_type =
			(edma_vp_feat_type_t)edma_dp_ppe_vp_feat_type_map[i];
	}
}

#ifdef NSS_DP_HW_GRO
/*
 * edma_hw_gro_init()
 *	HW GRO Initialization
 */
static int edma_hw_gro_init(struct edma_gbl_ctx *egc)
{
	int ret = 0;

	egc->hw_gro_ctx.gro_timeout_usecs = EDMA_RX_GRO_TIMEOUT_DEFAULT;
	egc->hw_gro_ctx.gro_buffer_len = EDMA_RX_GRO_BUFFER_LEN_DEFAULT;
	egc->hw_gro_ctx.gro_desc_count = EDMA_RX_GRO_DESC_COUNT_DEFAULT;
	egc->hw_gro_ctx.hw_gro_en = true;

	ret = edma_rx_gro_timeout_configure(&egc->hw_gro_ctx);
	if (ret) {
		edma_err("Failed to configure GRO timeout: %d\n", ret);
		return ret;
	}

	ret = edma_rx_gro_buffer_len_configure(&egc->hw_gro_ctx);
	if (ret) {
		edma_err("Failed to configure GRO buffer len: %d\n", ret);
		return ret;
	}

	ret = edma_rx_gro_desc_count_configure(&egc->hw_gro_ctx);
	if (ret) {
		edma_err("Failed to configure GRO desc count: %d\n", ret);
		return ret;
	}

	ret = edma_rx_gro_slot_vld_configure(egc);
	if (ret) {
		edma_err("Failed to configure GRO slot vld: %d\n", ret);
		return ret;
	}

	return 0;
}
#endif

/*
 * edma_hw_init()
 *	EDMA hardware initialization
 */
static int edma_hw_init(struct edma_gbl_ctx *egc)
{
	int ret = 0;
	uint32_t data;
#ifdef NSS_DP_MHT_SW_PORT_MAP
	fal_athtag_tx_cfg_t tx_cfg = {0};
	sw_error_t fal_ret;
#endif

	data = edma_reg_read(EDMA_REG_MAS_CTRL);
	edma_info("EDMA ver %d hw init\n", data);

	/*
	 * Setup private data structure
	 */
	egc->rxfill_intr_mask = EDMA_RXFILL_INT_MASK;
	egc->rxdesc_intr_mask = EDMA_RXDESC_INT_MASK_PKT_INT;
	egc->txcmpl_intr_mask = EDMA_TX_INT_MASK_PKT_INT;
	egc->edma_initialized = false;
	atomic_set(&egc->active_port_count, 0);

	/*
	 * Reset EDMA
	 */
	ret = edma_hw_reset(egc);
	if (ret) {
		edma_err("Error in resetting the hardware. ret: %d\n", ret);
		return ret;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_HW_RESET_DONE);

#ifdef NSS_DP_DDRQ_SUPPORT
	if (edma_ddrq_init(&egc->ddrq_def_cfg) != 0) {
		edma_err("Error in EDMA DDRQ initialization\n");
		return -EINVAL;
	}
	edma_warn("DDRQ init successful\n");
#endif

	/*
	 * Set EDMA global page mode and jumbo MRU
	 */
	edma_cfg_rx_page_mode_and_jumbo(egc);
	edma_set_init_stage(EDMA_INIT_STAGE_PAGE_MODE_SET);

	/*
	 * Set EDMA Tx max ports.
	 */
#ifdef NSS_DP_MHT_SW_PORT_MAP
	edma_cfg_tx_set_max_ports(egc);
#endif

	/*
	 * Mark and fill the host ring configuration.
	 */
	edma_fill_host_rings_info(egc, &init_info);

	/*
	 * Alloc and setup the software resources of the ring.
	 */
	ret = edma_alloc_and_setup_rings(egc);
	if (ret) {
		edma_err("Error in initializaing the rings. ret: %d\n", ret);
		return ret;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_RINGS_ALLOCATED);

	/*
	 * Disable interrupts
	 */
	edma_disable_interrupts(egc);

	edma_cfg_rx_rings_disable(egc);
	edma_cfg_tx_rings_disable(egc);

	edma_cfg_tx_mapping(egc);
	edma_set_init_stage(EDMA_INIT_STAGE_TX_MAPPING_DONE);
	edma_cfg_rx_mapping(egc);
	edma_set_init_stage(EDMA_INIT_STAGE_RX_MAPPING_DONE);
#if defined(NSS_DP_POINT_OFFLOAD)
	edma_cfg_tx_point_offload_mapping(egc);
	edma_cfg_rx_point_offload_mapping(egc);
#endif

	edma_fetch_mitigation_timer_rate(egc, NSS_DP_EDMA_CLK);
	edma_cfg_tx_rings(egc);
	edma_set_init_stage(EDMA_INIT_STAGE_TX_RINGS_CFG);
	edma_cfg_rx_rings(egc);
	edma_set_init_stage(EDMA_INIT_STAGE_RX_RINGS_CFG);
#if defined(NSS_DP_POINT_OFFLOAD)
	edma_cfg_rx_point_offload_rings(egc);
#endif

	/*
	 * Configure DMA request priority, DMA read burst length,
	 * and AXI write size.
	 */
	data = EDMA_DMAR_BURST_LEN_SET(EDMA_BURST_LEN_ENABLE)
		| EDMA_DMAR_REQ_PRI_SET(0)
		| EDMA_DMAR_TXDATA_OUTSTANDING_NUM_SET(31)
		| EDMA_DMAR_TXDESC_OUTSTANDING_NUM_SET(7)
		| EDMA_DMAR_RXFILL_OUTSTANDING_NUM_SET(7);
	edma_reg_write(EDMA_REG_DMAR_CTRL, data);
	edma_set_init_stage(EDMA_INIT_STAGE_DMA_CTRL_CFG);

	/*
	 * Configure TXQ_CTRL_2 register - TSO IP Identification (IPID) Update Control
	 *
	 * This register controls how the IP Identification field is updated for TSO packets.
	 * The behavior is determined by a hierarchy of control bits:
	 *
	 * GLOBAL_TSO_IDENT_UP_CTRL[3] (bit 19) - Global override control in TXQ_CTRL_2 register:
	 *	When set to 1: Ignore all other configurations and increment IPID by 1
	 *	When set to 0: Check per-descriptor and per ring control bits for IPID increment
	 *
	 * When GLOBAL_TSO_IDENT_UP_CTRL[3] == 0 in TXQ_CTRL_2 register,
	 * behavior depends on EDMA_TXDESC_TSO_IDENT_UPDATE_CTRL[14:13] (which is per ring configuration):
	 *
	 *	Case 1: EDMA_TXDESC_TSO_IDENT_UPDATE_CTRL[14:13] == 00 (bits cleared)
	 *		- Ignore all other settings and increment IPID by 1
	 *
	 *	Case 2: EDMA_TXDESC_TSO_IDENT_UPDATE_CTRL[14:13] == 01 (bit 13 set, bit 14 cleared)
	 *		- Check tso_ipid_mode (per TX descriptor) flag:
	 *			- If tso_ipid_mode == 0: Increment IPID by 1
	 *			- If tso_ipid_mode == 1: Check IP fragmentation flags:
	 *				If (DF && !MF && !offset): No increment
	 *				If (!DF || MF || offset): Increment by 1
	 *
	 *	Case 3: EDMA_TXDESC_TSO_IDENT_UPDATE_CTRL[14:13] == 1x (bit 14 set) - CURRENT CONFIGURATION
	 *		Check IP fragmentation flags:
	 *			-If (DF && !MF && !offset): No increment
	 *			-If (!DF || MF || offset): Increment by 1
	 *
	 * Where:
	 *	DF     = Don't Fragment flag in IP header
	 *	MF     = More Fragments flag in IP header
	 *	offset = Fragment offset field in IP header
	 *
	 * Current Configuration:
	 *	- Clearing bit 19 (GLOBAL_TSO_IDENT_UP_CTRL[3] = 0) to enable per-descriptor and per ring control
	 *	- EDMA_TXDESC_TSO_IDENT_UPDATE_CTRL[14:13] is set to 1x (configured while TX ring setup)
	 *	- This enables fragmentation-aware IPID handling by PPE hardware instead of using per descriptor tso_ipid_mode.
	 */
	data = edma_reg_read(EDMA_REG_TXQ_CTRL_2);
	data &= ~EDMA_GLOBAL_TSO_IDENT_UP_CTRL_MASK;
	edma_reg_write(EDMA_REG_TXQ_CTRL_2, data);
	edma_info("EDMA_REG_TXQ_CTRL_2 configured: 0x%x (bit 19 cleared)\n", data);

	/*
	 * Configure RXQ flow control threshold register
	 * Set bit 5 (EDMA_GLOBAL_FLOW_IDX_CFG) to enable global flow index configuration
	 */
	data = edma_reg_read(EDMA_REG_RXQ_FC_THRE);
	data |= EDMA_GLOBAL_FLOW_IDX_CFG_SET(1);
	edma_reg_write(EDMA_REG_RXQ_FC_THRE, data);

	/*
	 * Configure Tx Timeout Threshold
	 */
#if defined(NSS_DP_MAX_TXCOMP_TIMEOUT)
	data = EDMA_TX_TIMEOUT_THRESH_VAL;
	edma_reg_write(EDMA_REG_TX_TIMEOUT_THRESH, data);
#endif

	/*
	 * Misc error mask
	 */
	data = EDMA_MISC_AXI_RD_ERR_MASK |
		EDMA_MISC_AXI_WR_ERR_MASK |
		EDMA_MISC_RX_DESC_FIFO_FULL_MASK |
		EDMA_MISC_RX_ERR_BUF_SIZE_MASK |
		EDMA_MISC_TX_SRAM_FULL_MASK |
		EDMA_MISC_TX_CMPL_BUF_FULL_MASK |
		EDMA_MISC_DATA_LEN_ERR_MASK;
	data |= EDMA_MISC_TX_TIMEOUT_MASK;
	data |= EDMA_MISC_PASS_THR_ERR_FWD_MASK;
	data |= EDMA_MISC_TXQ_PASSTHR_OFFSET_MIS_MASK;
	data |= EDMA_MISC_TXQ_DS_CMPL_ERR_MASK;
	egc->misc_intr_mask = data;

	edma_cfg_rx_rings_enable(egc);
	edma_cfg_tx_rings_enable(egc);

	/*
	 * Global EDMA enable and padding enable
	 */
	data = edma_reg_read(EDMA_REG_PORT_CTRL);
	data &= ~EDMA_PORT_TX_DROP_EN;
	data |= EDMA_PORT_PAD_EN | EDMA_PORT_EDMA_EN;
	edma_reg_write(EDMA_REG_PORT_CTRL, data);

	/*
	 * Initialize unicast priority map table
	 */
	ret = (int)edma_configure_ucast_prio_map_tbl();
	if (ret) {
		edma_err("Failed to initialize unicast priority map table: %d\n", ret);
		edma_cfg_rx_rings_disable(egc);
		edma_cfg_tx_rings_disable(egc);
		edma_cfg_tx_rings_cleanup(egc);
		edma_cfg_rx_rings_cleanup(egc);
		return ret;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_PRIO_MAP_CFG);

	/*
	 * Initialize RPS hash map table
	 */
	edma_configure_rps_hash_map(egc);
	edma_set_init_stage(EDMA_INIT_STAGE_RPS_HASH_CFG);

#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (dp_global_ctx.is_mht_dev) {

		/*
		 * Mapping of MHT MDIO SLV pause ID to VP_PORTS.
		 */
		edma_cfg_tx_set_mht_mdio_slv_pause(egc);

		/*
		 * Set the atheros header for MHT switch.
		 */
		tx_cfg.athtag_en = A_TRUE;
		tx_cfg.athtag_type = MHT_ATHTAG_TYPE;
		tx_cfg.version = FAL_ATHTAG_VER3;
		tx_cfg.action = FAL_ATHTAG_ACTION_NORMAL;
		tx_cfg.bypass_fwd_en = A_FALSE;
		tx_cfg.field_disable = A_FALSE;
		fal_ret = fal_port_athtag_tx_set(EDMA_SWITCH_DEV_ID,
						EDMA_MHT_SWITCH_PORT_ID,
						&tx_cfg);
		if (fal_ret != SW_OK)
			edma_err("\nMHT SW atheros header set fail:%d\n",
					fal_ret);
	}
#endif

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (edma_gbl_ctx->loopback_en) {
		egc->loopback_ring_size = dp_global_ctx.edma_loopback_ring_size;
		egc->loopback_buf_size = dp_global_ctx.edma_loopback_buffer_size;

		ret = edma_alloc_loopback_ring(egc);
		if (ret) {
			return ret;
		}

		edma_cfg_rx_loopback_rings_disable(egc);
		edma_cfg_tx_loopback_rings_disable(egc);

		edma_cfg_tx_loopback_mapping(egc);
		edma_cfg_rx_loopback_mapping(egc);

		edma_cfg_tx_loopback_rings(egc);
		edma_cfg_rx_loopback_rings(egc);

		edma_cfg_rx_loopback_rings_enable(egc);
		edma_cfg_tx_loopback_rings_enable(egc);

		/*
		 * Loopback register configuration
		 */
		edma_hw_loopback_init(egc);
		edma_set_init_stage(EDMA_INIT_STAGE_LOOPBACK_CFG);
	}
#endif

#ifdef NSS_DP_HW_GRO
	ret = edma_hw_gro_init(egc);
	if (ret) {
		edma_err("HW GRO initialization failed\n");
		return ret;
	}
#endif

#ifdef NSS_DP_UDP_ST_SUPPORT
	/*
	 * Alloc and setup the software resources for UDP-ST ring.
	 */
	if (edma_udp_st_tx_ring > EDMA_RING_FLAGS_INVALID_ID) {
		ret = edma_cfg_tx_udp_st_ring_alloc(egc);
		if (ret) {
			edma_err("Error in initializaing the rings. ret: %d\n", ret);
			return ret;
		}
		edma_set_init_stage(EDMA_INIT_STAGE_RINGS_ALLOCATED);

		edma_cfg_tx_udp_st_ring_disable(egc);
		edma_cfg_tx_udp_st_mapping(egc);
		edma_cfg_tx_udp_st_ring(egc);

		edma_cfg_tx_udp_st_ring_enable(egc);
	}
#endif

	egc->edma_initialized = true;
	edma_set_init_stage(EDMA_INIT_STAGE_PORT_ENABLED);

	return 0;
}

/*
 * edma_configure_clocks()
 *	API to configure EDMA common clocks
 *	TODO: Revisit this during SOD
 */
static int32_t edma_configure_clocks(void)
{
	struct platform_device *pdev = edma_gbl_ctx->pdev;
	int32_t err;

	/*
	 * Configure SoC specific EDMA/NSS clocks
	 */
	err = nss_dp_hal_configure_clocks(pdev);
	if (err) {
		edma_err("DP hal clock config failed\n");
		return -1;
	}

	return 0;
}

#ifdef NSS_DP_DDRQ_SUPPORT
/*
 * edma_passthrough_handler()
 *	API to set EDMA passthrough value dynamically
 */
int edma_passthrough_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	edma_err("### passthrough mode val set to %d\n", edma_passthrough_val);
	return ret;
}
#endif

#ifdef NSS_DP_HW_GRO
/*
 * edma_rx_gro_timeout_cfg()
 *	Code change to configure GRO timeout
 */
int edma_rx_gro_max_timeout_cfg(struct ctl_table *table, int write,
                void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if (edma_gbl_ctx->hw_gro_ctx.gro_timeout_usecs > EDMA_RX_GRO_TIMEOUT_MAX) {
    		edma_err("GRO timeout %d exceeds max %d\n", edma_gbl_ctx->hw_gro_ctx.gro_timeout_usecs, EDMA_RX_GRO_TIMEOUT_MAX);
		return -EINVAL;
	}

	ret = edma_rx_gro_timeout_configure(&edma_gbl_ctx->hw_gro_ctx);
	if (ret) {
		edma_err("Failed to configure GRO timeout: %d\n", ret);
		return ret;
	}

	return ret;
}

/*
 * edma_rx_gro_buffer_len_cfg()
 *	Code change to configure GRO total coalescing buffer len
 */
int edma_rx_gro_max_buffer_len_cfg(struct ctl_table *table, int write,
                void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if (edma_gbl_ctx->hw_gro_ctx.gro_buffer_len >  EDMA_RX_GRO_BUFFER_LEN_MAX) {
		edma_debug("%p: gro buffer len more than max buffer length supported\n", edma_gbl_ctx, edma_gbl_ctx->hw_gro_ctx.gro_buffer_len);
		return -1;
	}

	ret = edma_rx_gro_buffer_len_configure(&edma_gbl_ctx->hw_gro_ctx);
	if (ret) {
		edma_err("Failed to configure GRO buffer len: %d\n", ret);
		return ret;
	}

	return ret;
}


/*
 * edma_rx_gro_max_desc_count_cfg()
 *	Code change to configure GRO total coalescing buffer len
 */
int edma_rx_gro_max_desc_count_cfg(struct ctl_table *table, int write,
                void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if (edma_gbl_ctx->hw_gro_ctx.gro_desc_count > EDMA_RX_GRO_DESC_COUNT_MAX) {
		edma_debug("%p: gro desc count more than max desc count supported\n", edma_gbl_ctx, edma_gbl_ctx->hw_gro_ctx.gro_desc_count);
		return -1;
	}

	ret = edma_rx_gro_desc_count_configure(&edma_gbl_ctx->hw_gro_ctx);
	if (ret) {
		edma_err("Failed to configure GRO desc count: %d\n", ret);
		return ret;
	}

	return ret;
}
#endif

/*
 * edma_sub
 *	EDMA sub directory
 */
static struct ctl_table edma_sub[] = {
	{
		.procname	=	"rx_fc_enable",
		.data		=	&edma_cfg_rx_fc_enable,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	edma_cfg_rx_fc_enable_handler
	},
	{
		.procname	=	"rx_queue_tail_drop_enable",
		.data		=	&edma_cfg_rx_queue_tail_drop_enable,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	edma_cfg_rx_queue_tail_drop_handler
	},
	{
		.procname	=	"rps_num_cores",
		.data		=	&edma_cfg_rx_rps_num_cores,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	edma_cfg_rx_rps,
	},
	{
		.procname	=	"rps_bitmap_cores",
		.data		=	&edma_cfg_rx_rps_bitmap_cores,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	edma_cfg_rx_rps_bitmap
	},
	{
		.procname       =       "edma_hang_recover",
		.data           =       &edma_hang_recover,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       edma_hang_recovery_handler
	},
	{
		.procname       =       "edma_vlan_append",
		.data           =       &edma_vlan_append_info,
		.maxlen         =       sizeof(char) * EDMA_VLAN_APPEND_INFO_STR_LEN,
		.mode           =       0644,
		.proc_handler   =       edma_vlan_append_handler
	},
	{
		.procname       =       "edma_force_crash",
		.data           =       &edma_force_crash,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       edma_force_crash_handler
	},

#ifdef NSS_DP_HW_GRO
	{
		.procname	=	"gro_timeout",
		.data		=	NULL,
		.maxlen		=	sizeof(int),
		.mode		=	0644,
		.proc_handler	=	edma_rx_gro_max_timeout_cfg
	},
	{
		.procname       =       "gro_buffer_len",
		.data           =       NULL,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       edma_rx_gro_max_buffer_len_cfg
	},
	{
		.procname       =       "gro_desc_count",
		.data           =       NULL,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       edma_rx_gro_max_desc_count_cfg
	},
#endif
#ifdef NSS_DP_DDRQ_SUPPORT
	{
		.procname       =       "edma_passthrough_val",
		.data           =       &edma_passthrough_val,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       edma_passthrough_handler
	},
#endif
	{}
};

/*
 * edma_init()
 *	EDMA init
 */
int edma_init(void)
{
	int ret = 0, i;
	struct resource res_edma;
	uint8_t queue_start = 0;
	int cpu, idx;
	struct edma_host_info *host_info;
	struct edma_rx_rings_info *rx_info;
	int min;

	edma_gbl_ctx->hw_init_bitmap = 0;
	edma_gbl_ctx->clk_init_bitmap = 0;
	edma_set_init_stage(EDMA_INIT_STAGE_CTX_ALLOC);

	/*
	 * Check the EDMA state
	 */
	if (likely(edma_gbl_ctx->edma_initialized)) {
		edma_debug("EDMA is already initialized");
		return 0;
	}

	/*
	 * Get all the DTS data needed
	 */
	if (edma_of_get_pdata(&res_edma) < 0) {
		edma_err("Unable to get EDMA DTS data.\n");
		return -EINVAL;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_DTS_PARSED);

#ifdef NSS_DP_HW_GRO
	{
		struct ctl_table *entry;

		for (entry = edma_sub; entry->procname; entry++) {
			if (!strcmp(entry->procname, "gro_timeout"))
				entry->data = &edma_gbl_ctx->hw_gro_ctx.gro_timeout_usecs;
			else if (!strcmp(entry->procname, "gro_buffer_len"))
				entry->data = &edma_gbl_ctx->hw_gro_ctx.gro_buffer_len;
			else if (!strcmp(entry->procname, "gro_desc_count"))
				entry->data = &edma_gbl_ctx->hw_gro_ctx.gro_desc_count;
		}
	}
#endif

	edma_gbl_ctx->ctl_table_hdr = register_sysctl("net/edma", edma_sub);
	if (!edma_gbl_ctx->ctl_table_hdr) {
		edma_err("sysctl table configuration failed");
		return -EINVAL;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_SYSCTL_REG);

	/*
	 * Request memory region for EDMA registers
	 */
	edma_gbl_ctx->reg_resource = request_mem_region(res_edma.start,
				resource_size(&res_edma),
				EDMA_DEVICE_NODE_NAME);
	if (!edma_gbl_ctx->reg_resource) {
		edma_err("Unable to request EDMA register memory.\n");
		unregister_sysctl_table(edma_gbl_ctx->ctl_table_hdr);
		edma_gbl_ctx->ctl_table_hdr = NULL;
		return -EFAULT;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_MEM_REGION_REQ);

	/*
	 * Parse and config EDMA ini
	 */
	if (edma_parse_ini()) {
		edma_err("INI parsing failed \n");
		ret = -EFAULT;
		goto edma_parse_ini_fail;
	}

	/*
	 * Remap register resource
	 */
	edma_gbl_ctx->reg_base = ioremap((edma_gbl_ctx->reg_resource)->start,
			resource_size(edma_gbl_ctx->reg_resource));
	if (!edma_gbl_ctx->reg_base) {
		edma_err("Unable to remap EDMA register memory.\n");
		ret = -EFAULT;
		goto edma_init_remap_fail;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_IOREMAP_DONE);

	/*
	 * Initialize EDMA debugfs entry
	 */
	ret = edma_debugfs_init();
	if (ret < 0) {
		edma_err("Error in EDMA debugfs init API. ret: %d\n", ret);
		ret = -EINVAL;
		goto edma_debugfs_init_fail;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_DEBUGFS_INIT);

#ifdef NSS_DP_PPEDS_SUPPORT
	if (edma_ppeds_init(&edma_gbl_ctx->ppeds_drv) != 0) {
		edma_err("Error in edma ppeds initialization\n");
		ret = -EFAULT;
		goto edma_init_ppeds_init_fail;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_PPEDS_INIT);
#endif

	/*
	 * Configure the EDMA common clocks
	 */
	/*
	 * TODO: Revisit this during SOD
	 */
	ret = edma_configure_clocks();
	if (ret) {
		edma_err("Error in configuring the common EDMA clocks\n");
		ret = -EFAULT;
		goto edma_hw_init_fail;
	}
	edma_set_init_stage(EDMA_INIT_STAGE_CLOCKS_CONFIGURED);

	edma_info("EDMA common clocks are configured\n");

	if (edma_hw_init(edma_gbl_ctx) != 0) {
		edma_err("Error in edma initialization\n");
		ret = -EFAULT;
		goto edma_hw_init_fail;
	}

	if (!(edma_gbl_ctx->mem_profile & NSS_DP_MEM_PROFILE_OPTIMIZED)) {
		/*
		 * Register PTP service code callback function
		 */
		ppe_drv_sc_register_cb(PPE_DRV_SC_PTP, edma_rx_tstamp_buf, NULL);
	}

	/*
	 * Register mirror core selection API callback with PPE driver
	 */
	ppe_drv_acl_mirror_core_select_register_cb(edma_configure_mirror_pkt_capture_core, NULL);

	/*
	 * We add NAPIs and register IRQs at the time of the first netdev open
	 */
	edma_gbl_ctx->napi_added = false;

#ifdef NSS_DP_HW_GRO
	/*
	 * GRO ring to queue mapping
	 */
	for (i = 0; i < edma_dp_gro_num_rx_rings; i++) {
		queue_start = edma_gbl_ctx->hw_gro_ctx.rx_gro_queue_start;
		ppe_drv_gro_core2queue_mapping(i, queue_start + i);
	}
#endif

	/*
	 * DP module maintains queue to ring mapping, and the rings are mapped
	 * to specific host cores. Similar mapping is needed in ppe driver to
	 * redirect packets/flows to specific host cores.
	 */
	host_info = &init_info.host_info;
	rx_info = &host_info->sfe_info.rx_info;

	if (NR_CPUS < rx_info->num_rx_rings) {
		min = NR_CPUS;
	} else {
		min = rx_info->num_rx_rings;
	}

	for (i = 0; i < min; i++) {
		queue_start = edma_gbl_ctx->rx_queue_start + rx_info->rx_map[i].ppe_queue_base;
		ppe_drv_core2queue_mapping(i, queue_start);
	}

	/*
	 * Configure loopback
	 */
#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (edma_gbl_ctx->loopback_en) {
		ppe_drv_loopback_base_queue(edma_gbl_ctx->loopback_queue_base, edma_gbl_ctx->loopback_feature_type);
	}
#endif

	/*
	 * Initialize the mapping of txcmpl, rxdesc and rxfill rings to bp counters
	 */
	edma_cfg_ring_to_backpressure_mapping();

	/*
	 * Initialize the procf entries for enabling EDMA ring stats
	 */
	edma_procfs_init();
	edma_set_init_stage(EDMA_INIT_STAGE_PROCFS_INIT);

	/*
         * Initialize the EDMA global context work task with the edma_recovery_work function
         * which will trigger the edma_hang_recovery_and_reg_dump user script using call_usermodehelper function.
         */
        if(nss_dp_recovery_en){
                INIT_WORK(&edma_gbl_ctx->work, edma_recovery_work);
        }

	for_each_online_cpu(cpu) {
		struct nss_dp_vp_ctx *ctx = per_cpu_ptr(&g_vp_ctx, cpu);

		for (idx = 0; idx < PPE_DRV_VIRTUAL_MAX; idx++) {
			struct nss_dp_vp_node *node = &ctx->nodes[idx];

			skb_queue_head_init(&node->head);
			memset(&node->info, 0, sizeof(node->info));
		}
	}

	return 0;

edma_hw_init_fail:
#ifdef NSS_DP_PPEDS_SUPPORT
	edma_ppeds_deinit(&edma_gbl_ctx->ppeds_drv);
edma_init_ppeds_init_fail:
#endif
	edma_debugfs_exit();

edma_debugfs_init_fail:
	iounmap(edma_gbl_ctx->reg_base);

edma_init_remap_fail:
	release_mem_region((edma_gbl_ctx->reg_resource)->start,
			resource_size(edma_gbl_ctx->reg_resource));
	unregister_sysctl_table(edma_gbl_ctx->ctl_table_hdr);
	edma_gbl_ctx->ctl_table_hdr = NULL;

edma_parse_ini_fail:
	return ret;
}

/*
 * edma_irq_init()
 *	Initialize interrupt handlers for the driver
 */
int edma_irq_init(void)
{
	int err;
	uint32_t entry_num, i;
#ifdef NSS_DP_MHT_SW_PORT_MAP
	uint32_t num_txcmpl_rings = edma_gbl_ctx->num_txcmpl_rings -
					edma_gbl_ctx->mht_txcmpl_ports;
	uint32_t ppeds_nodes = 0;
#endif
	struct device *dev = &edma_gbl_ctx->pdev->dev;

	/*
	 * Get TXCMPL rings IRQ numbers
	 */
	entry_num = of_property_match_string(dev->of_node, "interrupt-names", "txcmpl_0");
	for (i = 0; i < edma_gbl_ctx->txcmpl_ring_max; i++, entry_num++) {
		edma_gbl_ctx->txcmpl_info[i].intr_num =
			platform_get_irq(edma_gbl_ctx->pdev, entry_num);
		if (edma_gbl_ctx->txcmpl_info[i].intr_num < 0) {
			edma_err("%s: txcmpl_intr[%u] irq get failed\n",
					(edma_gbl_ctx->device_node)->name, i);
			return -1;
		}

		edma_debug("%s: txcmpl_intr[%u] = %u\n",
				 (edma_gbl_ctx->device_node)->name,
				 i, edma_gbl_ctx->txcmpl_info[i].intr_num);
	}

	/*
	 * Get RXDESC rings IRQ numbers
	 */
	entry_num = of_property_match_string(dev->of_node, "interrupt-names", "rxdesc_0");
	for (i = 0; i < edma_gbl_ctx->rxdesc_ring_max; i++, entry_num++) {
		edma_gbl_ctx->rxdesc_info[i].intr_num =
			platform_get_irq(edma_gbl_ctx->pdev, entry_num);
		if (edma_gbl_ctx->rxdesc_info[i].intr_num < 0) {
			edma_err("%s: rxdesc_intr[%u] irq get failed\n",
					(edma_gbl_ctx->device_node)->name, i);
			return -1;
		}

		edma_debug("%s: rxdesc_intr[%u] = %u\n",
				 (edma_gbl_ctx->device_node)->name,
				 i, edma_gbl_ctx->rxdesc_info[i].intr_num);
	}

	/*
	 * Get misc IRQ number
	 */
	entry_num = of_property_match_string(dev->of_node, "interrupt-names", "misc_err");
	edma_gbl_ctx->misc_intr = platform_get_irq(edma_gbl_ctx->pdev, entry_num);
	if (edma_gbl_ctx->misc_intr < 0) {
		edma_err("%s: misc_intr irq get failed\n", (edma_gbl_ctx->device_node)->name);
		return -1;
	}

	edma_debug("%s: misc IRQ:%u\n", (edma_gbl_ctx->device_node)->name,
						edma_gbl_ctx->misc_intr);

	/*
	 * Get RXFILL rings IRQ numbers
	 */

	entry_num = of_property_match_string(dev->of_node, "interrupt-names", "rxfill_0");
	for (i = 0; i < edma_gbl_ctx->rxfill_ring_max; i++) {
		edma_gbl_ctx->rxfill_info[i].intr_num = platform_get_irq(edma_gbl_ctx->pdev, entry_num);
		entry_num++;
		if (edma_gbl_ctx->rxfill_info[i].intr_num < 0) {
			edma_err("%s: rxfill_intr[%u] irq get failed\n",
					(edma_gbl_ctx->device_node)->name, i);
			return -1;
		}

		edma_debug("%s: rxfill_intr[%u] = %u\n", (edma_gbl_ctx->device_node)->name,
				 i, edma_gbl_ctx->rxfill_info[i].intr_num);
	}

	/*
	 * Get TXCMPL rings IRQ numbers for MHT txcmpl rings.
	 */
#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (!dp_global_ctx.is_mht_dev)
		goto done;

	/*
	 * Txcmpl IRQ index needs to reach correct index
	 * in case PPEDS code is compiled out.
	 */
	if (!ppeds_nodes)
		entry_num += EDMA_PPEDS_IRQS;

	for (i = num_txcmpl_rings; i < edma_gbl_ctx->num_txcmpl_rings; i++) {
		entry_num++;
		edma_gbl_ctx->txcmpl_info[i].intr_num =
			platform_get_irq(edma_gbl_ctx->pdev, entry_num);
		if (edma_gbl_ctx->txcmpl_info[i].intr_num < 0) {
			edma_err("%s: txcmpl_intr[%u] irq get failed\n",
					(edma_gbl_ctx->device_node)->name, i);
			return -1;
		}

		edma_debug("%s: txcmpl_intr[%u] = %u\n",
				(edma_gbl_ctx->device_node)->name,
				i, edma_gbl_ctx->txcmpl_info[i].intr_num);
	}

done:
#endif
	/*
	 * Request IRQ for Tx complete rings
	 */
	for (i = 0; i < edma_gbl_ctx->txcmpl_ring_max; i++) {
		if (!(edma_gbl_ctx->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		snprintf(edma_txcmpl_irq_name[i], 32, "edma_txcmpl_%d", i);

		irq_set_status_flags(edma_gbl_ctx->txcmpl_info[i].intr_num, IRQ_DISABLE_UNLAZY);

		err = request_irq(edma_gbl_ctx->txcmpl_info[i].intr_num,
				edma_tx_handle_irq, IRQF_SHARED,
				edma_txcmpl_irq_name[i],
				(void *)(edma_gbl_ctx->txcmpl_info[i].txcmpl_ring));
		if (err) {
			edma_err("TXCMPL ring IRQ:%d request %d failed\n",
					edma_gbl_ctx->txcmpl_info[i].intr_num, i);
			return -1;

		}

		edma_debug("TXCMPL ring(%d) IRQ:%d request success(%s)\n",
					i,
					edma_gbl_ctx->txcmpl_info[i].intr_num,
					edma_txcmpl_irq_name[i]);
	}

	/*
	 * Request IRQ for RXDESC rings
	 */
	for (i = 0; i < edma_gbl_ctx->rxdesc_ring_max; i++) {
		if (!(edma_gbl_ctx->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		if (edma_gbl_ctx->rxdesc_info[i].type_flags & EDMA_RING_TYPE_FLAGS_HOST_GRO) {
			snprintf(edma_rxdesc_irq_name[i], 20, "edma_rxdesc_gro_%d", i);
		} else {
			snprintf(edma_rxdesc_irq_name[i], 20, "edma_rxdesc_%d", i);
		}

		irq_set_status_flags(edma_gbl_ctx->rxdesc_info[i].intr_num, IRQ_DISABLE_UNLAZY);

		err = request_irq(edma_gbl_ctx->rxdesc_info[i].intr_num,
				edma_rx_handle_irq, IRQF_SHARED,
				edma_rxdesc_irq_name[i],
				(void *)(edma_gbl_ctx->rxdesc_info[i].rxdesc_ring));
		if (err) {
			edma_err("RXDESC ring IRQ:%d request failed\n",
					edma_gbl_ctx->rxdesc_info[i].intr_num);
			goto rx_desc_ring_intr_req_fail;
		}

		edma_debug("RXDESC ring(%d) IRQ:%d request success(%s)\n",
					i,
					edma_gbl_ctx->rxdesc_info[i].intr_num,
					edma_rxdesc_irq_name[i]);
	}

	/*
	 * Request Misc IRQ
	 */
	err = request_irq(edma_gbl_ctx->misc_intr, edma_misc_handle_irq,
						IRQF_SHARED, "edma_misc",
						(void *)edma_gbl_ctx->pdev);
	if (err) {
		edma_err("MISC IRQ:%d request failed\n",
				edma_gbl_ctx->misc_intr);
		goto misc_intr_req_fail;
	}

	/*
	 * Request IRQ for RXFILL rings
	 */
	for (i = 0; i < edma_gbl_ctx->rxfill_ring_max; i++) {
		if (!(edma_gbl_ctx->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		snprintf(edma_rxfill_irq_name[i], 20, "edma_rxfill_%d", i);

		irq_set_status_flags(edma_gbl_ctx->rxfill_info[i].intr_num, IRQ_DISABLE_UNLAZY);

		err = request_irq(edma_gbl_ctx->rxfill_info[i].intr_num,
				edma_rxfill_handle_irq, IRQF_SHARED,
				edma_rxfill_irq_name[i],
				(void *)(edma_gbl_ctx->rxfill_info[i].rxfill_ring));
		if (err) {
			edma_err("RXFILL ring IRQ:%d request failed\n",
					edma_gbl_ctx->rxfill_info[i].intr_num);
			goto rx_fill_ring_intr_req_fail;
		}

		edma_debug("RXFILL ring(%d) IRQ:%d request success(%s)\n",
					i,
					edma_gbl_ctx->rxfill_info[i].intr_num,
					edma_rxfill_irq_name[i]);
	}

	return 0;

rx_fill_ring_intr_req_fail:
	/*
	 * Free IRQ for MISC interrupt.
	 */
	synchronize_irq(edma_gbl_ctx->misc_intr);
	free_irq(edma_gbl_ctx->misc_intr, (void *)edma_gbl_ctx->pdev);

misc_intr_req_fail:
	/*
	 * Free IRQ for RXDESC rings
	 */
	for (i = 0; i < edma_gbl_ctx->rxdesc_ring_max; i++) {
		if (!(edma_gbl_ctx->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		synchronize_irq(edma_gbl_ctx->rxdesc_info[i].intr_num);

		free_irq(edma_gbl_ctx->rxdesc_info[i].intr_num,
				(void *)(edma_gbl_ctx->rxdesc_info[i].rxdesc_ring));
	}

rx_desc_ring_intr_req_fail:
	/*
	 * Free IRQ for TXCMPL rings
	 */
	for (i = 0; i < edma_gbl_ctx->txcmpl_ring_max; i++) {
		if (!(edma_gbl_ctx->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		synchronize_irq(edma_gbl_ctx->txcmpl_info[i].intr_num);
		free_irq(edma_gbl_ctx->txcmpl_info[i].intr_num,
				(void *)(edma_gbl_ctx->txcmpl_info[i].txcmpl_ring));
	}

	return -1;
}

/*
 * edma_recovery_cleanup()
 *	EDMA cleanup for EDMA recovery
 */
static void edma_recovery_cleanup(bool is_dp_override)
{
	/*
	 * TODO: Check with HW team about the state of in-flight
	 * packets when the descriptor rings are disabled.
	 */
	edma_cfg_tx_rings_disable(edma_gbl_ctx);
	edma_cfg_rx_rings_disable(edma_gbl_ctx);

	/*
	 * Remove interrupt handlers and NAPI
	 */
	if (edma_gbl_ctx->napi_added) {
		uint32_t i;

		/*
		 * Free IRQ for TXCMPL rings
		 */
		for (i = 0; i < edma_gbl_ctx->txcmpl_ring_max; i++) {
			if (edma_gbl_ctx->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
				synchronize_irq(edma_gbl_ctx->txcmpl_info[i].intr_num);

				free_irq(edma_gbl_ctx->txcmpl_info[i].intr_num,
						(void *)(edma_gbl_ctx->txcmpl_info[i].txcmpl_ring));
			}
		}

		/*
		 * Free IRQ for RXDESC rings
		 */
		for (i = 0; i < edma_gbl_ctx->rxdesc_ring_max; i++) {
			if (edma_gbl_ctx->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
				synchronize_irq(edma_gbl_ctx->rxdesc_info[i].intr_num);

				free_irq(edma_gbl_ctx->rxdesc_info[i].intr_num,
						(void *)(edma_gbl_ctx->rxdesc_info[i].rxdesc_ring));
			}
		}

		/*
		 * Free Misc IRQ
		 */
		synchronize_irq(edma_gbl_ctx->misc_intr);
		free_irq(edma_gbl_ctx->misc_intr, (void *)(edma_gbl_ctx->pdev));

		edma_cfg_rx_napi_delete(edma_gbl_ctx);
		edma_cfg_tx_napi_delete(edma_gbl_ctx);
		edma_gbl_ctx->napi_added = false;
	}

	/*
	 * Disable EDMA only at module exit time.
	 */
	if (!is_dp_override) {
		edma_disable_port();
	}

	/*
	 * cleanup rings and free
	 */
	edma_cfg_tx_rings_cleanup(edma_gbl_ctx);
	edma_cfg_rx_rings_cleanup(edma_gbl_ctx);

#ifdef NSS_DP_UDP_ST_SUPPORT
	if (edma_udp_st_tx_ring > EDMA_RING_FLAGS_INVALID_ID) {
		nss_dp_udp_st_deinit();
		edma_cfg_tx_udp_st_ring_disable(edma_gbl_ctx);
		edma_cfg_tx_udp_st_ring_cleanup(edma_gbl_ctx);
	}
#endif

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (edma_gbl_ctx->loopback_en) {
		edma_cfg_tx_loopback_rings_disable(edma_gbl_ctx);
		edma_cfg_rx_loopback_rings_disable(edma_gbl_ctx);
		edma_cfg_tx_loopback_rings_cleanup(edma_gbl_ctx);
		edma_cfg_rx_loopback_rings_cleanup(edma_gbl_ctx);

		edma_rx_free_buffer_loopback();
		kfree(edma_gbl_ctx->rxfill_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->rxdesc_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txcmpl_loopback_ring_id_arr);
		kfree(edma_gbl_ctx->txdesc_loopback_ring_id_arr);
	}
#endif

	iounmap(edma_gbl_ctx->reg_base);
	release_mem_region((edma_gbl_ctx->reg_resource)->start,
			resource_size(edma_gbl_ctx->reg_resource));

	/*
	 * Mark initialize false, so that we do not
	 * try to cleanup again
	 */
	edma_gbl_ctx->edma_initialized = false;
}

/*
 * edma_recovery_setup()
 *	EDMA setup for EDMA recovery
 */
static int edma_recovery_setup(void)
{
	int ret = 0;
	struct resource res_edma;

	/*
	 * Get all the DTS data needed
	 */
	if (edma_of_get_pdata(&res_edma) < 0) {
		edma_err("Unable to get EDMA DTS data.\n");
		return -EINVAL;
	}

	/*
	 * Request memory region for EDMA registers
	 */
	edma_gbl_ctx->reg_resource = request_mem_region(res_edma.start,
			resource_size(&res_edma),
			EDMA_DEVICE_NODE_NAME);
	if (!edma_gbl_ctx->reg_resource) {
		edma_err("Unable to request EDMA register memory.\n");
		unregister_sysctl_table(edma_gbl_ctx->ctl_table_hdr);
		edma_gbl_ctx->ctl_table_hdr = NULL;
		return -EFAULT;
	}

	/*
	 * Remap register resource
	 */
	edma_gbl_ctx->reg_base = ioremap((edma_gbl_ctx->reg_resource)->start,
			resource_size(edma_gbl_ctx->reg_resource));
	if (!edma_gbl_ctx->reg_base) {
		edma_err("Unable to remap EDMA register memory.\n");
		ret = -EFAULT;
		goto edma_init_remap_fail;
	}

	/*
	 * Configure the EDMA common clocks
	 */
	ret = edma_configure_clocks();
	if (ret) {
		edma_err("Error in configuring the common EDMA clocks\n");
		ret = -EFAULT;
		goto edma_hw_init_fail;
	}

	edma_info("EDMA common clocks are configured\n");

	if (edma_hw_init(edma_gbl_ctx) != 0) {
		edma_err("Error in edma initialization\n");
		ret = -EFAULT;
		goto edma_hw_init_fail;
	}

	dp_global_ctx.common_init_done = true;

	return 0;

edma_hw_init_fail:
	iounmap(edma_gbl_ctx->reg_base);

edma_init_remap_fail:
	release_mem_region((edma_gbl_ctx->reg_resource)->start,
			resource_size(edma_gbl_ctx->reg_resource));
	unregister_sysctl_table(edma_gbl_ctx->ctl_table_hdr);
	edma_gbl_ctx->ctl_table_hdr = NULL;

	return ret;

}

/*
 * edma_recovery_deinit()
 *	EDMA remove for EDMA recovery
 */
static int edma_recovery_deinit(void)
{
	reset_control_put(edma_gbl_ctx->hw_rst);

#if defined(NSS_DP_CONFIG_RST)
	reset_control_put(edma_gbl_ctx->cfg_rst);
#endif

	atomic_set(&edma_gbl_ctx->active_port_count, 0);

	if (dp_global_ctx.common_init_done) {
		edma_recovery_cleanup(false);
		dp_global_ctx.common_init_done = false;
	}

	return 0;
}

/*
 * edma_recovery_init()
 *	EDMA reinit for EDMA recovery
 */
static int edma_recovery_init(void)
{
	uint32_t ret;
	uint32_t i;
	uint32_t j;
	struct nss_dp_dev *dp_priv;

	/*
	 * Re-initialize EDMA
	 */
	ret = edma_recovery_setup();
	if (ret) {
		edma_hang_recover = 0;
		edma_err("EDMA recovery setup failed\n");
		return -EINVAL;
	}

	for (i = 0; i < NSS_DP_HAL_MAX_PORTS; i++) {
		struct edma_tx_rings_info *tx_info;
		dp_priv = dp_global_ctx.nss_dp[i];
		edma_cfg_tx_napi_add(edma_gbl_ctx, dp_priv->netdev, dp_priv->macid);

		if (!edma_gbl_ctx->napi_added) {
			edma_cfg_rx_napi_add(edma_gbl_ctx, dp_priv->netdev);
			edma_irq_init();
		}

		edma_gbl_ctx->napi_added = true;

		if (dp_priv->macid == NSS_DP_VP_MAC_ID) {
			tx_info = &init_info.host_info.vp_info.tx_info;
		} else {
			tx_info = &init_info.host_info.sfe_info.tx_info;
		}

		for_each_possible_cpu(j) {
			struct nss_dp_dev *dp_dev = (struct nss_dp_dev *)netdev_priv(dp_priv->netdev);
			struct edma_txdesc_ring *txdesc_ring;
			uint32_t txdesc_ring_id;

			for (int k = 0; k < tx_info->max_rings_per_core; k++) {
				if (k >= EDMA_MAX_TX_RINGS_PER_CORE)
					continue;

				txdesc_ring_id = tx_info->tx_ring_per_core_map[j][k];
				txdesc_ring = edma_gbl_ctx->txdesc_info[txdesc_ring_id].txdesc_ring;
				dp_dev->dp_info.txr_map[j][k] = txdesc_ring;
			}
		}
	}

	return 0;
}

/*
 * edma_hang_recovery()
 *	API to recover from EDMA hang
 */
static int edma_hang_recovery(void)
{
	/*
	 * De-initializing EDMA for hang recovery
	 */
	edma_recovery_deinit();

	/*
	 * Initializing EDMA for hang recovery
	 */
	edma_recovery_init();

	/*
	 * Resetting the `edma_hang_recover` flag to indicate recovery completion
	 */
	edma_hang_recover = 0;

        return 0;
}

/*
 * edma_hang_recovery_handler()
 *	to trigger recovery API for EDMA hang
 */
int edma_hang_recovery_handler(struct ctl_table *table, int write,
                void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if(edma_hang_recover){
		edma_hang_recovery();
	}

	return ret;
}

/*
 * edma_vlan_append_handler()
 *	Add VLAN Info.
 */
int edma_vlan_append_handler(struct ctl_table *table, int write,
                void __user *buffer, size_t *lenp, loff_t *ppos)
{
	uint16_t ether_type_0, ether_type_1;
	struct nss_dp_dev *dp_dev;
	uint32_t vlan_tag_info;
	struct net_device *dev;
	long int result;
	char *work_str;
	char *dev_name;
	int base = 0;
	char *token;
	bool enable;
	uint8_t i;
	int ret;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret) {
		return ret;
	}

	if (!write) {
		for (i = 0; i < NSS_DP_MAX_PORTS; i++) {
			dp_dev = dp_global_ctx.nss_dp[i];
			if ((dp_dev) && (dp_dev->vlan_info.vlan_en)) {
				edma_warn("Enabled the VLAN Append Functionality for dev_name:%s, "
					"vlan_insert_en:%d, vlan_tag_info:0x%x, "
					"ether_type_0:0x%x, ether_type_1:0x%x \n",
					dp_dev->netdev->name,
					dp_dev->vlan_info.vlan_en, ntohl(dp_dev->vlan_info.vlan_tag_info),
					ntohs(dp_dev->vlan_info.ether_types[0]), ntohs(dp_dev->vlan_info.ether_types[1]));
			}
		}
		*lenp = 0;
		return ret;
	}

	edma_debug("Input String: %s\n", edma_vlan_append_info);

	work_str = edma_vlan_append_info;

	token = strsep(&work_str, " ");

	if (strcmp(token, "E") == 0) {
		enable = 1;
	} else if (strcmp(token, "D") == 0) {
		enable = 0;
	} else {
		edma_err("Invalid input, enter valid info, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n");
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -EINVAL;
	}

	token = strsep(&work_str, " ");

	dev_name = token;
	if (!dev_name) {
		edma_err("Invalid WAN Interface: %s, Enter valid info, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n", dev_name);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -EINVAL;
	}

	dev = dev_get_by_name(&init_net, dev_name);
	if (!dev) {
		edma_err("No valid Net Device found for the interface details configured, dev_name:%s", dev_name);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -ENODEV;
	}

	edma_debug("dev_name:%s, ifindex:%d", dev->name, dev->ifindex);

	dp_dev = (struct nss_dp_dev *)netdev_priv(dev);
	if (!dp_dev) {
		edma_err("dp_dev is NULL for ndev in edma vlan sysctl handler");
		dev_put(dev);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -ENODEV;
	}

	if (!enable) {
		memset(&dp_dev->vlan_info, 0, sizeof(dp_dev->vlan_info));
		edma_info("Disabled the VLAN Append Functionality, vlan_insert_en:%d, dev_name:%s",
				dp_dev->vlan_info.vlan_en, dev_name);
		ppe_drv_port_vlan_append_set(dev, false);
		dev_put(dev);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return ret;
	}

	token = strsep(&work_str, " ");
	if(!token) {
		edma_err("Unconfigured vlan_tag_info, Enter valid info, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n");
		dev_put(dev);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -EINVAL;
	}

	if(kstrtou32(token, 0, &vlan_tag_info)) {
		edma_err("Invalid vlan_tag_info: %s, Enter valid info, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n", token);
		dev_put(dev);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -EINVAL;
	}

	edma_debug("VLAN_TAG_INFO: 0x%x\n", vlan_tag_info);

	token = strsep(&work_str, " ");
	if(!token) {
		edma_err("Unconfigured ether_type_0, Enter valid info, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n");
		dev_put(dev);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -EINVAL;
	}

	if (kstrtol(token, base, &result)) {
		edma_err("Invalid ether_type_0: %s, Enter valid info, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n", token);
		dev_put(dev);
		memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
		return -EINVAL;
	}

	ether_type_0 = (uint16_t)result;
	edma_debug("ETHER_TYPE_0: 0x%x\n", ether_type_0);

	/*
	 * We do not return any error when
	 * ether_type_1 is not configured
	 * or invalid being an optional field.
	 * We simply mark the value to 0 and continue
	 * the execution in those cases.
	 */
	token = strsep(&work_str, " ");
	if(!token) {
		edma_info("Unconfigured ether_type_1 "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n");
		ether_type_1 = 0;

	} else {
		if (kstrtol(token, base, &result)) {
			edma_info("Invalid ether_type_1: %s, "
			"Usage: echo 'E/D <WAN_INTF> <VLAN_TAG in Binary/Hex> <ETHER_TYPE in Binary/hex> <ETHER_TYPE in Binary/hex(optional)>' > /proc/sys/net/edma/edma_vlan_append\n", token);
			ether_type_1 = 0;
		} else {
			ether_type_1 = (uint16_t)result;
		}
	}

	edma_debug("ETHER_TYPE_1: 0x%x\n", ether_type_1);

	edma_debug("VLAN insert enable:%d, dev_name:%s, VLAN_TAG_INFO:0x%x, ETHER_TYPE_0 : 0x%x, ETHER_TYPE_1: 0x%x\n",
			enable, dev_name, vlan_tag_info, ether_type_0, ether_type_1);

	dp_dev->vlan_info.vlan_en = 1;
	dp_dev->vlan_info.vlan_tag_info = htonl(vlan_tag_info);
	dp_dev->vlan_info.ether_types[0] = htons(ether_type_0);
	dp_dev->vlan_info.ether_types[1] = htons(ether_type_1);

	edma_info("Enabled the VLAN Append Functionality, vlan_insert_en:%d, dev_name:%s, vlan_tag_info:0x%x, ether_type_0:0x%x, ether_type_1:0x%x",
			dp_dev->vlan_info.vlan_en, dev_name, ntohl(dp_dev->vlan_info.vlan_tag_info), ntohs(dp_dev->vlan_info.ether_types[0]), ntohs(dp_dev->vlan_info.ether_types[1]));

	ppe_drv_port_vlan_append_set(dev, true);
	dev_put(dev);
	memset(edma_vlan_append_info, 0, sizeof(edma_vlan_append_info));
	return ret;
}

/*
 * edma_force_crash_handler()
 *	Function to trigger crash from EDMA module
 */
int edma_force_crash_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!write) {
		return ret;
	}

	if(edma_force_crash) {
		BUG_ON(A_TRUE);
	}

	return ret;
}
