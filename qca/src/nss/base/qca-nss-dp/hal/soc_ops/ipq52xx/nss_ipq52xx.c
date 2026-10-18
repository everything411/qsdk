/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <nss_dp_arch.h>
#include "nss_dp_hal.h"
#include "edma.h"
#include "edma_debug.h"

int edma_dp_host_rx_rings[EDMA_MAX_RXDESC_RING_PER_TYPE] = {7,8,9,10,-1,-1,-1,-1};
int edma_dp_host_rx_queue_map[EDMA_MAX_RXDESC_RING_PER_TYPE] = {0,8,16,24,-1,-1,-1,-1};
int edma_dp_host_rxfill_map[EDMA_MAX_RXFILL_RING_PER_TYPE] = {7,8,9,10,-1,-1,-1,-1};
int edma_dp_host_tx_rings[EDMA_MAX_TXDESC_RING_PER_TYPE] = {7,8,9,10,-1,-1,-1,-1};
int edma_dp_host_txcmpl_rings[EDMA_MAX_TXCMPL_RING_PER_TYPE] = {7,8,9,10,2,3,4,5};
int edma_dp_host_txcmpl_map[EDMA_MAX_TXDESC_RING_PER_TYPE] = {7,8,9,10,2,3,4,5};
int edma_dp_host_tx_ring_to_core_map[EDMA_MAX_TXDESC_TO_CORE_MAP_PER_TYPE] = {7,7,8,8,9,9,10,10};
int edma_dp_boot_rx_fill_cnt = 0;
uint32_t edma_dp_host_rx_ring_sz = 2048;
uint32_t edma_dp_host_rxfill_ring_sz = 2048;
uint32_t edma_dp_host_tx_ring_sz = 2048;
uint32_t edma_dp_host_txcmpl_ring_sz = 2048;

int edma_dp_ppe_ds_rx_rings[EDMA_PPEDS_MAX_NODES] = {0, 1};
int edma_dp_ppe_ds_rx_queue_map[EDMA_PPEDS_MAX_NODES] = {210, 218};
int edma_dp_ppe_ds_num_rx_queue[EDMA_PPEDS_MAX_NODES] = {8, 8};
int edma_dp_ppe_ds_num_rxdesc_per_node[EDMA_PPEDS_MAX_NODES] = {1, 1};
int edma_dp_ppe_ds_rxfill_rings[EDMA_PPEDS_MAX_NODES] = {0, 1};
int edma_dp_ppe_ds_tx_rings[EDMA_PPEDS_MAX_NODES] = {0, 1};
int edma_dp_ppe_ds_num_txdesc_per_node[EDMA_PPEDS_MAX_NODES] = {1, 1};
int edma_dp_ppe_ds_txcmpl_rings[EDMA_PPEDS_MAX_NODES] = {0, 1};

/*
 * PPEVP ring info
 */
int edma_dp_ppe_vp_num_tx_rings = EDMA_MAX_TXDESC_RING_PPEVP;
int edma_dp_ppe_vp_tx_rings[EDMA_MAX_TXDESC_RING_PPEVP] = {2,3,4,5};
int edma_dp_ppe_vp_txcmpl_map[EDMA_MAX_TXCMPL_RING_PPEVP] = {2,3,4,5};
int edma_dp_ppe_vp_num_tx_rings_per_core = EDMA_MAX_TX_RINGS_PER_CORE;
int edma_dp_ppe_vp_tx_ring_to_core_map[EDMA_MAX_TXDESC_TO_CORE_MAP_PER_TYPE] = {2, 2, 3, 3, 4, 4, 5, 5};
uint32_t edma_dp_ppe_vp_tx_ring_sz = 2048;

/*
 * VP features ring info - disabled for ipq52xx
 */
int edma_dp_ppe_vp_feat_num_rings = 0;
int edma_dp_ppe_vp_feat_rx_rings[EDMA_MAX_RXDESC_RING_PER_TYPE] = {-1};
int edma_dp_ppe_vp_feat_rx_queue_map[EDMA_MAX_RXDESC_RING_PER_TYPE] = {-1};
int edma_dp_ppe_vp_feat_rxfill_map[EDMA_MAX_RXFILL_RING_PER_TYPE] = {-1};
int edma_dp_ppe_vp_feat_type_map[EDMA_MAX_RXDESC_RING_PER_TYPE] = {-1};
uint32_t edma_dp_ppe_vp_feat_rx_ring_sz = 4096;
uint32_t edma_dp_ppe_vp_feat_rxfill_ring_sz = 4096;

#ifdef NSS_DP_HW_GRO
int edma_dp_gro_ppe_queue_base = EDMA_GRO_PPE_QUEUE_BASE;
uint32_t edma_dp_gro_rx_ring_sz = 512;
uint32_t edma_dp_gro_rxfill_ring_sz = 512;
#endif

/*
 * nss_dp_hal_nsm_sawf_sc_stats_read()
 *	Send nsm stats for the given service-class.
 */
bool nss_dp_hal_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class)
{
	return edma_nsm_sawf_sc_stats_read(nsm_stats, service_class);
}

/*
 * nss_dp_hal_deinit_soc_priv_flags()
 *	API to de-initialize DP DEV flags field
 */
void nss_dp_hal_deinit_soc_priv_flags(struct nss_dp_dev *dp_priv)
{
	return;
}

/*
 * nss_dp_hal_init_soc_priv_flags()
 *	API to initialize DP DEV flags field
 */
void nss_dp_hal_init_soc_priv_flags(struct nss_dp_dev *dp_priv)
{
	return;
}

/*
 * nss_dp_hal_get_data_plane_ops()
 *	Return the data plane ops for registered data plane.
 */
struct nss_dp_data_plane_ops *nss_dp_hal_get_data_plane_ops(void)
{
	return &nss_dp_edma_ops;
}

/*
 * nss_dp_hal_clock_set_and_enable()
 *	API to set and enable the EDMA common clocks
 */
int32_t nss_dp_hal_clock_set_and_enable(struct device *dev, const char *id, unsigned long rate)
{
	struct clk *clk = NULL;
	int err;

	clk = devm_clk_get(dev, id);
	if (IS_ERR(clk)) {
		return -1;
	}

	if (rate) {
		err = clk_set_rate(clk, rate);
		if (err) {
			return -1;
		}
	}

	err = clk_prepare_enable(clk);
	if (err) {
		return -1;
	}

	return 0;
}

/*
 * nss_dp_hal_cache_info_setup()
 *	Dummy wrap-around function.
 *	Returns 0: success
 */
int nss_dp_hal_cache_info_setup(void *ctx)
{
	struct edma_gbl_ctx *egc = (struct edma_gbl_ctx *)ctx;

	egc->cache_data = NULL;
	return 0;
}

/*
 * nss_dp_hal_configure_ring_size()
 *	Configure the ring size.
 */
void nss_dp_hal_configure_ring_size(uint32_t flags)
{
	/*
	 * Set the profile ring sizes according to the profile.
	 */
	if (flags & NSS_DP_MEM_PROFILE_OPTIMIZED) {
		edma_dp_host_rx_ring_sz = edma_dp_host_rxfill_ring_sz = 512;
		edma_dp_host_tx_ring_sz	= edma_dp_host_txcmpl_ring_sz = edma_dp_ppe_vp_tx_ring_sz = 1024;
        } else if (flags & NSS_DP_MEM_PROFILE_BALANCED) {
		edma_dp_host_rx_ring_sz = edma_dp_host_rxfill_ring_sz = 1024;
		edma_dp_host_tx_ring_sz	= edma_dp_host_txcmpl_ring_sz = edma_dp_ppe_vp_tx_ring_sz = 1024;
        } else if (flags & NSS_DP_MEM_PROFILE_HIGH) {
		edma_dp_host_rx_ring_sz = edma_dp_host_rxfill_ring_sz = 2048;
		edma_dp_host_tx_ring_sz	= edma_dp_host_txcmpl_ring_sz = edma_dp_ppe_vp_tx_ring_sz = 2048;
        } else {
		edma_dp_host_rx_ring_sz = edma_dp_host_rxfill_ring_sz = 2048;
		edma_dp_host_tx_ring_sz	= edma_dp_host_txcmpl_ring_sz = edma_dp_ppe_vp_tx_ring_sz = 2048;
        }

	return;
}

/*
 * nss_dp_hal_configure_clocks()
 *	configure the EDMA clock's.
 */
int32_t nss_dp_hal_configure_clocks(void *ctx)
{
	struct platform_device *pdev = (struct platform_device *)ctx;
	int32_t err;

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_CSR_CLK, NSS_DP_EDMA_CSR_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_CSR_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_CSR);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_CSR_CLK, NSS_DP_EDMA_NSSNOC_CSR_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_CSR_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_CSR);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_TS_CLK, NSS_DP_EDMA_TS_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_TS_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_TS);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSCC_CLK, NSS_DP_EDMA_NSSCC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSCC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSCC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSCFG_CLK, NSS_DP_EDMA_NSSCFG_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSCC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSCFG);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_ATB_CLK,
					NSS_DP_EDMA_NSSNOC_ATB_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_ATB_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_ATB);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_NSSCC_CLK,
					NSS_DP_EDMA_NSSNOC_NSSCC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_NSSCC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_NSSCC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK,
					NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_PCNOC_1);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK,
					NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_QOSGEN_REF);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_SNOC_1_CLK,
					NSS_DP_EDMA_NSSNOC_SNOC_1_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_SNOC_1_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_SNOC_1);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_SNOC_CLK,
					NSS_DP_EDMA_NSSNOC_SNOC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_SNOC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_SNOC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK,
					NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_TIMEOUT_REF);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_XO_DCD_CLK,
					NSS_DP_EDMA_NSSNOC_XO_DCD_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_XO_DCD_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_XO_DCD);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK,
					NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_MEM_NOC_1);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_MEMNOC_CLK,
					NSS_DP_EDMA_NSSNOC_MEMNOC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_MEMNOC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_MEMNOC);

	return 0;
}

/*
 * nss_dp_hal_hw_reset()
 *	Reset EDMA hardware.
 */
int32_t nss_dp_hal_hw_reset(void *ctx)
{
	struct reset_control *edma_hw_rst, *edma_cfg_rst;
	struct platform_device *pdev = (struct platform_device *)ctx;

	edma_hw_rst = devm_reset_control_get(&pdev->dev, EDMA_HW_RESET_ID);
	if (IS_ERR(edma_hw_rst)) {
		edma_err("Error: edma HW reset failed\n");
		return -EINVAL;
	}

	edma_cfg_rst = devm_reset_control_get(&pdev->dev, EDMA_CFG_RESET_ID);
	if (IS_ERR(edma_hw_rst)) {
		edma_err("Error: edma HW CFG reset failed\n");
		return -EINVAL;
	}

	/*
 	 * Store the obtained hardware reset handle (`edma_hw_rst`) in the global context
 	 * (`edma_gbl_ctx`) for future use. This allows for centralized reset control
 	 * throughout the driver.
 	 *
 	 * TODO: Revisit if this global storage is actually required.
 	 */
	edma_gbl_ctx->hw_rst = edma_hw_rst;

	/*
	 * Store the obtained edma configuration reset handle (`edma_cfg_rst`) in the global context
	 * (`edma_gbl_ctx`) for future use. This allows for centralized configuration reset control
	 * throughout the driver.
	 */
	edma_gbl_ctx->cfg_rst = edma_cfg_rst;

	reset_control_assert(edma_hw_rst);
	udelay(100);

	reset_control_deassert(edma_hw_rst);
	udelay(100);

	/*
	 * EDMA configuration reset.
	 */
	reset_control_assert(edma_cfg_rst);
	udelay(100);

	reset_control_deassert(edma_cfg_rst);
	udelay(100);

	return 0;
}

#ifdef NSS_DP_DDRQ_SUPPORT
/*
 * nss_dp_hal_ddrq_cfg_get()
 *	API to get DDRQ AC queue configurations
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_cfg_get(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg, uint32_t count)
{
	return edma_ddrq_cfg_get(obj, ddrq_cfg, count);
}

/*
 * nss_dp_hal_ddrq_cfg_set()
 *	API to set DDRQ AC queue configurations
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_cfg_set(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg)
{
	return edma_ddrq_cfg_set(obj, ddrq_cfg);
}

/*
 * nss_dp_hal_ddrq_grp_cfg_get()
 *	API to get DDRQ group configurations
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_grp_cfg_get(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg)
{
	return edma_ddrq_grp_cfg_get(ddrq_grp_id, ddrq_grp_cfg);
}

/*
 * nss_dp_hal_ddrq_grp_cfg_set()
 *	API to set DDRQ group configurations
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_grp_cfg_set(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg)
{
	return edma_ddrq_grp_cfg_set(ddrq_grp_id, ddrq_grp_cfg);
}

nss_dp_ddrq_ret_t nss_dp_hal_ddrq_enqueue_disable(nss_dp_ddrq_obj_id_t *obj, bool disable)
{
	return edma_ddrq_enqueue_disable(obj, disable);
}

nss_dp_ddrq_ret_t nss_dp_hal_ddrq_dequeue_drop(nss_dp_ddrq_obj_id_t *obj, bool drop)
{
	return edma_ddrq_dequeue_drop(obj, drop);
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_reset()
 *	API to reset DDRQ occupancy stats
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_reset(void)
{
	return edma_ddrq_occupancy_stats_reset();
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_start()
 *	API to start the DDRQ occupancy test instance
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_start(void)
{
	return edma_ddrq_occupancy_stats_start();
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_stop()
 *	API to stop the DDRQ occupancy test instance
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_stop(void)
{
	return edma_ddrq_occupancy_stats_stop();
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_restart()
 *	API to restart the DDRQ occupancy test instance
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_restart(void)
{
	return edma_ddrq_occupancy_stats_restart();
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_threshold_set()
 *	API to set DDRQ occupancy threshold configuration
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_threshold_set(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold)
{
	return edma_ddrq_occupancy_stats_threshold_set(ddrq_id, threshold);
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_threshold_get()
 *	API to get DDRQ occupancy threshold configuration
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_threshold_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold)
{
	return edma_ddrq_occupancy_stats_threshold_get(ddrq_id, threshold);
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_get()
 *	API to get DDRQ occupancy stats
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_stats_t *ddrq_stats)
{
	return edma_ddrq_occupancy_stats_get(ddrq_id, ddrq_stats);
}

/*
 * nss_dp_hal_ddrq_occupancy_stats_status_get()
 *	API to get DDRQ occupancy stats run status
 */
nss_dp_ddrq_ret_t nss_dp_hal_ddrq_occupancy_stats_status_get(uint32_t ddrq_id, bool *status)
{
	return edma_ddrq_occupancy_stats_status_get(ddrq_id, status);
}
#endif 		/* NSS_DP_DDRQ_SUPPORT */

/*
 * nss_dp_hal_init()
 *	Initialize EDMA and set gmac ops.
 */
bool nss_dp_hal_init(void)
{
	/*
	 * Bail out on not supported platform
	 */
	if (!of_machine_is_compatible("qcom,ipq5210")) {
		return false;
	}

	if (edma_init()) {
		return false;
	}

	nss_dp_hal_set_gmac_ops(&qcom_gmac_ops, GMAC_HAL_TYPE_QCOM);
	nss_dp_hal_set_gmac_ops(&syn_gmac_ops, GMAC_HAL_TYPE_SYN_XGMAC);

	return true;
}

/*
 * nss_dp_hal_cleanup()
 *	Cleanup EDMA and set gmac ops to NULL.
 */
void nss_dp_hal_cleanup(void)
{
	nss_dp_hal_set_gmac_ops(NULL, GMAC_HAL_TYPE_QCOM);
	nss_dp_hal_set_gmac_ops(NULL, GMAC_HAL_TYPE_SYN_XGMAC);
	edma_cleanup(false);
}

/*
 * nss_dp_ppeds_wifi_arch_mode_ops_get()
 *	API to get PPE-DS operations()
 */
struct nss_dp_ppeds_ops *nss_dp_ppeds_wifi_arch_mode_ops_get(uint32_t mode)
{
#ifdef NSS_DP_PPEDS_SUPPORT
	if (mode == EDMA_PPEDS_WIFI_ARCH_MODE_WIFI8) {
		return &edma_ppeds_ops_wifi8;
	} else {
		return &edma_ppeds_ops_wifi7;
	}
#else
	return NULL;
#endif
}
