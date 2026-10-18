/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/reset.h>
#include <nss_dp_dev.h>
#include "edma.h"
#include "edma_cfg_tx.h"
#include "edma_regs.h"
#include "edma_debug.h"

#ifdef NSS_DP_MHT_SW_PORT_MAP
/*
 * edma_cfg_tx_set_mht_mdio_slv_pause()
 *	Set the MDIO_SLV pause mapping to VP_PORTS
 */
void edma_cfg_tx_set_mht_mdio_slv_pause(struct edma_gbl_ctx *egc)
{
	uint32_t data_reg_0 = 0;
	uint32_t data_reg_1 = 0;
	uint32_t id1;
	uint32_t id2;

	for (id1 = 0; id1 < EDMA_GLOBAL_MDIO_SLV_PAUSE_ID_REG0_MAX; id1++)
		data_reg_0 |= EDMA_GLOBAL_MDIO_SLV_PAUSE_ID_MAP_VPPORT(
				(id1 & EDMA_GLOBAL_MDIO_SLV_VPPORTS_MAX), id1);

	for (id2 = 0; id2 < EDMA_GLOBAL_MDIO_SLV_PAUSE_ID_REG1_MAX; id2++) {
		data_reg_1 |= EDMA_GLOBAL_MDIO_SLV_PAUSE_ID_MAP_VPPORT(
				(id1 & EDMA_GLOBAL_MDIO_SLV_VPPORTS_MAX), id2);
		id1++;
	}
	edma_reg_write(EDMA_MDIO_SLV_PASUE_MAP_0, data_reg_0);
	edma_reg_write(EDMA_MDIO_SLV_PASUE_MAP_1, data_reg_1);
}

/*
 * edma_cfg_tx_set_max_ports()
 *	Configure EDMA Tx max ports
 */
void edma_cfg_tx_set_max_ports(struct edma_gbl_ctx *egc)
{
	/*
	 * Max ports include all MHT switch ports when module param
	 * nss_dp_mht_multi_txring is enabled.
	 * Max ports is Individual ports + MHT SWT ports
	 *
	 */
	if (dp_global_ctx.is_mht_dev) {
		egc->max_tx_ports = NSS_DP_HAL_MAX_TX_PORTS;
		return;
	}

	/*
	 * Max ports equals Individual ports +
	 *		SWT port(single port represents switch) +
	 * 		VP ports
	 */
	egc->max_tx_ports = NSS_DP_MAX_PORTS;
}
#endif

/*
 * edma_cfg_tx_cmpl_ring_cleanup()
 *	Cleanup resources for one TxCmpl ring
 */
static void edma_cfg_tx_cmpl_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_txcmpl_ring *txcmpl_ring)
{
	/*
	 * Free any buffers assigned to any descriptors
	 */
	edma_tx_complete(txcmpl_ring->count, txcmpl_ring);

	/*
	 * Free TxCmpl ring descriptors
	 */
	kfree(txcmpl_ring->desc);
	txcmpl_ring->desc = NULL;
	txcmpl_ring->dma = (dma_addr_t)0;
}

/*
 * edma_cfg_tx_cmpl_ring_setup()
 *	Setup resources for one TxCmpl ring
 */
int edma_cfg_tx_cmpl_ring_setup(struct edma_txcmpl_ring *txcmpl_ring)
{
	txcmpl_ring->desc = kmalloc(roundup((sizeof(struct edma_txcmpl_desc) * txcmpl_ring->count),
						SMP_CACHE_BYTES), GFP_KERNEL | __GFP_ZERO);
	if (!txcmpl_ring->desc) {
		edma_err("Descriptor alloc for TXCMPL ring %u failed\n",
				txcmpl_ring->id);
		return -ENOMEM;
	}

	txcmpl_ring->dma = (dma_addr_t)virt_to_phys(txcmpl_ring->desc);

	return 0;
}

/*
 * edma_cfg_tx_desc_ring_cleanup()
 *	Cleanup resources for one TxDesc ring
 *
 * This API expects ring to be disabled by caller
 */
static void edma_cfg_tx_desc_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_txdesc_ring *txdesc_ring)
{
	struct sk_buff *skb = NULL;
	struct edma_pri_txdesc *txdesc = NULL;
	uint32_t prod_idx, cons_idx, data, cons_idx_prev;;

	/*
	 * Free any buffers assigned to any descriptors
	 */
	data = edma_reg_read(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id));
	prod_idx = data & EDMA_TXDESC_PROD_IDX_MASK;

	data = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(txdesc_ring->id));
	cons_idx_prev = cons_idx = data & EDMA_TXDESC_CONS_IDX_MASK;

	/*
	 * Walk active list, obtain skb from descriptor and free it
	 */
	while (cons_idx != prod_idx) {
		txdesc = EDMA_TXDESC_PRI_DESC(txdesc_ring, cons_idx);
		skb = (struct sk_buff *)EDMA_TXDESC_OPAQUE_GET(txdesc);
		dev_kfree_skb_any(skb);

		cons_idx = ((cons_idx + 1) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count));
	}

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id), cons_idx_prev);

	/*
	 * Free Tx ring descriptors
	 */
	kfree(txdesc_ring->pdesc);
	txdesc_ring->pdesc = NULL;
	txdesc_ring->pdma = (dma_addr_t)0;

	/*
	 * Skip freeing up secondary descriptors if preheader mode is configured
	 */
	if (txdesc_ring->pre_hdr_mode_en) {
		return;
	}

	/*
	 * TODO:
	 * Free any buffers assigned to any secondary descriptors
	 */
	kfree(txdesc_ring->sdesc);
	txdesc_ring->sdesc = NULL;
	txdesc_ring->sdma = (dma_addr_t)0;
}

/*
 * edma_cfg_tx_desc_ring_setup()
 *	Setup resources for one TxDesc ring
 */
int edma_cfg_tx_desc_ring_setup(struct edma_txdesc_ring *txdesc_ring)
{
	/*
	 * Allocate Tx ring descriptors
	 */
	txdesc_ring->pdesc = kmalloc(roundup((sizeof(struct edma_pri_txdesc) * txdesc_ring->count),
						SMP_CACHE_BYTES), GFP_KERNEL | __GFP_ZERO);
	if (!txdesc_ring->pdesc) {
		edma_err("Descriptor alloc for TXDESC ring %u failed\n",
				txdesc_ring->id);
		return -ENOMEM;
	}

	txdesc_ring->pdma = (dma_addr_t)virt_to_phys(txdesc_ring->pdesc);

	/*
	 * Skip allocating secondary descriptors if preheader mode is configured
	 */
	if (txdesc_ring->pre_hdr_mode_en) {
		edma_info("EDMA Tx ring:(%d) is configured in a preheader mode\n", txdesc_ring->id);
		return 0;
	}

	/*
	 * Allocate sencondary Tx ring descriptors
	 */
	txdesc_ring->sdesc = kmalloc(roundup((sizeof(struct edma_sec_txdesc) * txdesc_ring->count),
						SMP_CACHE_BYTES), GFP_KERNEL | __GFP_ZERO);
	if (!txdesc_ring->sdesc) {
		edma_err("Descriptor alloc for secondary TXDESC ring %u failed\n",
				txdesc_ring->id);
		kfree(txdesc_ring->pdesc);
		txdesc_ring->pdesc = NULL;
		txdesc_ring->pdma = (dma_addr_t)0;
		return -ENOMEM;
	}

	txdesc_ring->sdma = (dma_addr_t)virt_to_phys(txdesc_ring->sdesc);

	return 0;
}

/*
 * edma_cfg_tx_desc_ring_configure()
 *	Configure one TxDesc ring in EDMA HW
 */
static void edma_cfg_tx_desc_ring_configure(struct edma_txdesc_ring *txdesc_ring)
{
	uint32_t addr, ring_ctrl_cfg = 0;

	/*
	 * Configure TXDESC ring
	 */
	addr = (uint32_t)(txdesc_ring->pdma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA(txdesc_ring->id), addr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	addr = (uint32_t)((txdesc_ring->pdma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA_HIGH(txdesc_ring->id), addr);
#endif

	edma_reg_write(EDMA_REG_TXDESC_RING_SIZE(txdesc_ring->id),
			(uint32_t)(txdesc_ring->count &
			EDMA_TXDESC_RING_SIZE_MASK));

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id),
			(uint32_t)EDMA_TX_INITIAL_PROD_IDX);

	if (!txdesc_ring->pre_hdr_mode_en) {
		/*
		 * Configure Tx ring in a secondary descriptor mode
		 */
		addr = (uint32_t)(txdesc_ring->sdma & EDMA_RING_DMA_MASK);
		edma_reg_write(EDMA_REG_TXDESC_BA2(txdesc_ring->id), addr);

#if defined(NSS_DP_HIGHMEM_SUPP)
		addr = (uint32_t)((txdesc_ring->sdma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
		edma_reg_write(EDMA_REG_TXDESC_BA2_HIGH(txdesc_ring->id), addr);
#endif
	} else {
		/*
		 * Configure Tx ring in a preheader mode
		 */
		ring_ctrl_cfg = EDMA_TXDESC_CTRL_PH_EN_SET(EDMA_TXDESC_PH_EN);
	}

	/*
	 * Configure group ID for flow control and TSO IDENT UPDATE CTRL for this Tx ring
	 * FC_GRP_ID: Flow control group identifier for this ring
	 * TSO_IDENT_UPDATE_CTRL: Set to EDMA_TXDESC_TSO_IDENT_UPDATE_BY_PARSER to allow
	 * hardware parser to update TSO identifier fields
	 */
	ring_ctrl_cfg |= (EDMA_TXDESC_CTRL_FC_GRP_ID_SET(txdesc_ring->fc_grp_id) |
			EDMA_TXDESC_TSO_IDENT_UPDATE_CTRL_SET(EDMA_TXDESC_TSO_IDENT_UPDATE_BY_PARSER));
	edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), ring_ctrl_cfg);
	edma_info("EDMA_REG_TXDESC_CTRL (%d) configured value is 0x%0x, reg value : 0x%0x\n",
				 txdesc_ring->id, ring_ctrl_cfg,
				 edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id)));
}

/*
 * edma_cfg_tx_cmpl_ring_configure()
 *	Configure one TxCmpl ring in EDMA HW
 */
static void edma_cfg_tx_cmpl_ring_configure(struct edma_txcmpl_ring *txcmpl_ring)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t data;
	uint32_t paddr;

	/*
	 * Configure TxCmpl ring base address
	 */
	paddr = (uint32_t)(txcmpl_ring->dma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_BA(txcmpl_ring->id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((txcmpl_ring->dma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_BA_HIGH(txcmpl_ring->id), paddr);
#endif

	edma_reg_write(EDMA_REG_TXCMPL_RING_SIZE(txcmpl_ring->id),
			(uint32_t)(txcmpl_ring->count
			& EDMA_TXDESC_RING_SIZE_MASK));

	/*
	 * Set TxCmpl ret mode to opaque
	 */
	edma_reg_write(EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id),
			EDMA_TXCMPL_RETMODE_OPAQUE);

	/*
	 * Validate mitigation timer value
	 */
	if ((nss_dp_tx_mitigation_timer < EDMA_TX_MITIGATION_TIMER_MIN) ||
			(nss_dp_tx_mitigation_timer > EDMA_TX_MITIGATION_TIMER_MAX)) {
		edma_err("Invalid Tx mitigation timer configured:%d for ring:%d."
				" Using the default timer value:%d\n",
				nss_dp_tx_mitigation_timer, txcmpl_ring->id,
				NSS_DP_TX_MITIGATION_TIMER_DEF);
		nss_dp_tx_mitigation_timer = NSS_DP_TX_MITIGATION_TIMER_DEF;
	}

	/*
	 * Validate mitigation packet count value
	 */
	if ((nss_dp_tx_mitigation_pkt_cnt < EDMA_TX_MITIGATION_PKT_CNT_MIN) ||
			(nss_dp_tx_mitigation_pkt_cnt > EDMA_TX_MITIGATION_PKT_CNT_MAX)) {
		edma_err("Invalid Tx mitigation packet count configured:%d for ring:%d."
				" Using the default packet counter value:%d\n",
				nss_dp_tx_mitigation_timer, txcmpl_ring->id,
				NSS_DP_TX_MITIGATION_PKT_CNT_DEF);
		nss_dp_tx_mitigation_pkt_cnt = NSS_DP_TX_MITIGATION_PKT_CNT_DEF;
	}

	/*
	 * Configure the Mitigation timer
	 */
	data = MICROSEC_TO_TIMER_UNIT(nss_dp_tx_mitigation_timer, egc->edma_timer_rate);
	data = ((data & EDMA_TX_MOD_TIMER_INIT_MASK)
			<< EDMA_TX_MOD_TIMER_INIT_SHIFT);
	edma_info("EDMA Tx mitigation timer value: %d\n", data);
	edma_reg_write(EDMA_REG_TX_MOD_TIMER(txcmpl_ring->id), data);

	/*
	 * Configure the Mitigation packet count
	 */
	data = (nss_dp_tx_mitigation_pkt_cnt & EDMA_TXCMPL_LOW_THRE_MASK)
			<< EDMA_TXCMPL_LOW_THRE_SHIFT;
	edma_info("EDMA Tx mitigation packet count value: %d\n", data);
	edma_reg_write(EDMA_REG_TXCMPL_UGT_THRE(txcmpl_ring->id), data);

	edma_reg_write(EDMA_REG_TX_INT_CTRL(txcmpl_ring->id), EDMA_TX_NE_INT_EN);
}

/*
 * edma_cfg_tx_fill_per_port_tx_map()
 *	API to fill per-port Tx ring mapping in net device private area.
 */
void edma_cfg_tx_fill_per_port_tx_map(struct net_device *netdev, uint32_t macid)
{
	uint32_t i;
#ifdef NSS_DP_MHT_SW_PORT_MAP
	uint32_t sw_port, j;
#endif
	struct nss_dp_dev *dp_dev = (struct nss_dp_dev *)netdev_priv(netdev);
	struct edma_txdesc_ring *txdesc_ring;
	struct edma_tx_rings_info *tx_info;
	uint32_t txdesc_ring_id;

	if (dp_dev->macid == NSS_DP_VP_MAC_ID) {
		tx_info = &init_info.host_info.vp_info.tx_info;
	} else {
		tx_info = &init_info.host_info.sfe_info.tx_info;
	}

	for_each_possible_cpu(i) {
		for (int j = 0; j < tx_info->max_rings_per_core; j++) {
			if (j >= EDMA_MAX_TX_RINGS_PER_CORE)
				continue;

			txdesc_ring_id = tx_info->tx_ring_per_core_map[i][j];
			txdesc_ring = edma_gbl_ctx->txdesc_info[txdesc_ring_id].txdesc_ring;
			dp_dev->dp_info.txr_map[i][j] = txdesc_ring;
#ifdef NSS_DP_MHT_SW_PORT_MAP
			if (dp_dev->nss_dp_mht_dev)
				dp_dev->dp_info.txr_sw_port_map[0][i] = txdesc_ring;
#endif
		}
	}

#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (!dp_dev->nss_dp_mht_dev)
		return;

	sw_port = 1;
	for (i = NSS_DP_HAL_MAX_PORTS; i < edma_gbl_ctx->max_tx_ports; i++) {
		for_each_possible_cpu(j) {
			txdesc_ring_id = edma_gbl_ctx->tx_map[i][j];
			txdesc_ring = &edma_gbl_ctx->txdesc_rings[txdesc_ring_id - txdesc_start];
			dp_dev->dp_info.txr_sw_port_map[sw_port][j] = txdesc_ring;
		}
		sw_port++;
	}
#endif
}

/*
 * edma_cfg_tx_desc_ring_enable()
 *	API to enable one TX DESC ring.
 */
void edma_cfg_tx_desc_ring_enable(struct edma_txdesc_ring *txdesc_ring)
{
	uint32_t data;

	if (!txdesc_ring) {
		return;
	}

	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id));
	data |= EDMA_TXDESC_CTRL_TXEN_SET(EDMA_TXDESC_TX_ENABLE);
	edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), data);
	edma_debug("tx ring %d enabled\n", txdesc_ring->id);
}

/*
 * edma_cfg_tx_rings_enable()
 *	API to enable TX rings
 */
void edma_cfg_tx_rings_enable(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	/*
	 * Enable Tx rings
	 */
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)
			edma_cfg_tx_desc_ring_enable(egc->txdesc_info[i].txdesc_ring);
	}
}

/*
 * edma_cfg_tx_ring_reset()
 *	API to reset the individual TX ring
 *	NOTE: Caller is expected to ensure that no packets
 *	will be coming on the ring and the producer and consumer
 *	indexes are the same. (Currently used by PPE-DS only)
 */
void edma_cfg_tx_ring_reset(struct edma_txdesc_ring *ring)
{
	uint32_t data = 0;

	/*
	 * Reset the ring - wait untill the reset operation is done.
	 */
	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(ring->id));
	data |= EDMA_TXDESC_TX_RESET;
	edma_reg_write(EDMA_REG_TXDESC_CTRL(ring->id), data);
	do {
		data = edma_reg_read(EDMA_REG_TXDESC_CTRL(ring->id));
		data &= EDMA_TXDESC_TX_RESET;
	} while (data);

	/*
	 * Reset the software producer index.
	 */
	ring->prod_idx = 0;
}

/*
 * edma_cfg_txcmpl_ring_reset()
 *	API to reset Txcmpl ring indexes.
 */
void edma_cfg_txcmpl_ring_reset(struct edma_txcmpl_ring *ring)
{
	uint32_t data = 0;

	/*
	 * Reset the ring - wait untill the reset operation is done.
	 */
	data = edma_reg_read(EDMA_REG_TXCMPL_CTRL(ring->id));
	data |= EDMA_TXCMPL_IDX_RESET;
	edma_reg_write(EDMA_REG_TXCMPL_CTRL(ring->id), data);
	do {
		data = edma_reg_read(EDMA_REG_TXCMPL_CTRL(ring->id));
		data &= EDMA_TXCMPL_IDX_RESET;
	} while (data);

	/*
	 * Reset the software producer index.
	 */
	ring->cons_idx = 0;
}

/*
 * edma_cfg_tx_desc_ring_disable()
 *	API to disable one TXDESC ring.
 */
void edma_cfg_tx_desc_ring_disable(struct edma_txdesc_ring *txdesc_ring)
{
	uint32_t data;

	if (!txdesc_ring) {
		return;
	}

	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id));
	data &= ~EDMA_TXDESC_TX_ENABLE;
	edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), data);
	edma_debug("tx ring %d disabled\n", txdesc_ring->id);
}

/*
 * edma_cfg_tx_rings_disable()
 *	API to disable TX rings
 */
void edma_cfg_tx_rings_disable(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	/*
	 * Disable Tx rings
	 */
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)
			edma_cfg_tx_desc_ring_disable(egc->txdesc_info[i].txdesc_ring);
	}
}

/*
 * edma_cfg_tx_map_tx_ring_to_txcmpl()
 *	API to map a single TX ring to TX complete ring
 */
void edma_cfg_tx_map_tx_ring_to_txcmpl(uint32_t tx_ring_id, uint32_t txcmpl_ring_id)
{
	uint32_t reg, data;

	/*
	 * Setup TxDesc to TxComplete mapping.
	 * 6 registers to hold the completion mapping for total 32
	 * TX desc rings (0-5, 6-11, 12-17, 18-23, 24-29 and rest).
	 * In each entry 5 bits hold the mapping for a particular TX desc ring.
	 */
	if ((tx_ring_id >= 0) && (tx_ring_id <= 5)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_0;
	} else if ((tx_ring_id >= 6) && (tx_ring_id <= 11)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_1;
	} else if ((tx_ring_id >= 12) && (tx_ring_id <= 17)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_2;
	} else if ((tx_ring_id >= 18) && (tx_ring_id <= 23)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_3;
	} else {
		edma_err("Invalid tx_ring_id: %d\n", tx_ring_id);
		return;
	}

	edma_debug("Configure TXDESC:%u to use TXCMPL:%u\n", tx_ring_id, txcmpl_ring_id);

	/*
	 * Set the Tx complete descriptor ring number in the mapping register.
	 * E.g. If (txcmpl ring)txcmpl_ring_id = 19, (txdesc ring)tx_ring_id = 19.
	 * 	reg = EDMA_REG_TXDESC2CMPL_MAP_3
	 * 	data |= (txcmpl_ring_id & 0x1F) << ((tx_ring_id % 6) * 5);
	 * 	data |= (0x1F << 5); -
	 * 	This sets 10011 at 5th bit of register EDMA_REG_TXDESC2CMPL_MAP_3
	 */
	data = edma_reg_read(reg);
	data |= (txcmpl_ring_id & EDMA_TXDESC2CMPL_MAP_TXDESC_MASK) <<
		 ((tx_ring_id % EDMA_TXDESC2CMPL_MAP_NUM_IN_SINGLE_REG) *
		   EDMA_TXDESC2CMPL_MAP_TXDESC_ID_BIT_COUNT);
	edma_reg_write(reg, data);
}

/*
 * edma_cfg_tx_mapping()
 *	API to map TX to TX complete rings
 */
void edma_cfg_tx_mapping(struct edma_gbl_ctx *egc)
{
	uint32_t txcmpl_ring_id, tx_ring_id;

	/*
	 * Clear the TXDESC2CMPL_MAP_xx reg before setting up
	 * the mapping. This register holds TXDESC to TXFILL ring
	 * mapping.
	 */
	edma_reg_write(EDMA_REG_TXDESC2CMPL_MAP_0, 0);
	edma_reg_write(EDMA_REG_TXDESC2CMPL_MAP_1, 0);
	edma_reg_write(EDMA_REG_TXDESC2CMPL_MAP_2, 0);
	edma_reg_write(EDMA_REG_TXDESC2CMPL_MAP_3, 0);

	/*
	 * Map each TX ring to its completion ring
	 */
	for (tx_ring_id = 0; tx_ring_id < egc->txdesc_ring_max; tx_ring_id++) {
		if (egc->txdesc_info[tx_ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			txcmpl_ring_id = egc->txdesc_info[tx_ring_id].txcmpl_ring_id;
			edma_cfg_tx_map_tx_ring_to_txcmpl(tx_ring_id, txcmpl_ring_id);
		}
	}

	edma_debug("EDMA_REG_TXDESC2CMPL_MAP_0: 0x%x\n", edma_reg_read(EDMA_REG_TXDESC2CMPL_MAP_0));
	edma_debug("EDMA_REG_TXDESC2CMPL_MAP_1: 0x%x\n", edma_reg_read(EDMA_REG_TXDESC2CMPL_MAP_1));
	edma_debug("EDMA_REG_TXDESC2CMPL_MAP_2: 0x%x\n", edma_reg_read(EDMA_REG_TXDESC2CMPL_MAP_2));
	edma_debug("EDMA_REG_TXDESC2CMPL_MAP_3: 0x%x\n", edma_reg_read(EDMA_REG_TXDESC2CMPL_MAP_3));
}

#if defined(NSS_DP_POINT_OFFLOAD)
/*
 * edma_cfg_tx_point_offload_mapping()
 *	API to setup TX ring mapping
 */
void edma_cfg_tx_point_offload_mapping(struct edma_gbl_ctx *egc)
{
	uint32_t data, reg, ring_id = egc->txdesc_point_offload_ring;

	if ((ring_id >= 0) && (ring_id <= 5)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_0;
	} else if ((ring_id >= 6) && (ring_id <= 11)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_1;
	} else if ((ring_id >= 12) && (ring_id <= 17)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_2;
	} else if ((ring_id >= 18) && (ring_id <= 23)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_3;
	} else if ((ring_id >= 24) && (ring_id <= 29)) {
		reg = EDMA_REG_TXDESC2CMPL_MAP_4;
	} else {
		reg = EDMA_REG_TXDESC2CMPL_MAP_5;
	}

	edma_debug("Configure point offload TXDESC:%u to use TXCMPL:%u\n", ring_id, egc->txcmpl_point_offload_ring);

	/*
	 * Set the Tx complete descriptor ring number in the mapping register.
	 * E.g. If (txcmpl ring)desc_index = 31, (txdesc ring)i = 28.
	 * 	reg = EDMA_REG_TXDESC2CMPL_MAP_4
	 * 	data |= (desc_index & 0x1F) << ((i % 6) * 5);
	 * 	data |= (0x1F << 20); -
	 * 	This sets 11111 at 20th bit of register EDMA_REG_TXDESC2CMPL_MAP_4
	 */
	data = edma_reg_read(reg);
	data |= (egc->txcmpl_point_offload_ring & EDMA_TXDESC2CMPL_MAP_TXDESC_MASK) << ((ring_id % 6) * 5);
	edma_reg_write(reg, data);

	egc->tx_to_txcmpl_map[ring_id] = egc->txcmpl_point_offload_ring;
}
#endif

/*
 * edma_cfg_tx_rings_setup()
 *	Allocate/setup resources for EDMA rings
 */
static int edma_cfg_tx_rings_setup(struct edma_gbl_ctx *egc)
{
	uint32_t i = 0;

	struct edma_txdesc_ring_info *txdesc_info = egc->txdesc_info;
	struct edma_txcmpl_ring_info *txcmpl_info = egc->txcmpl_info;

	/*
	 * Allocate TxDesc ring descriptors
	 */
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_txdesc_ring *txdesc_ring = NULL;
			int32_t ret;

			txdesc_ring = txdesc_info[i].txdesc_ring;
			txdesc_ring->count = txdesc_info[i].desc_count;

			/*
			 * Fetch the mode in which the particular ring has to be configured
			 */
			txdesc_ring->pre_hdr_mode_en = EDMA_RING_MODE_GET(txdesc_ring->id, edma_tx_ring_mode_bitmask);
			edma_info("Edma Tx ring: (%d) configured mode value is %d. edma_tx_ring_mode_bitmask: 0x%0x\n",
					 txdesc_ring->id, txdesc_ring->pre_hdr_mode_en,
					 edma_tx_ring_mode_bitmask);

			ret = edma_cfg_tx_desc_ring_setup(txdesc_ring);
			if (ret != 0) {
				edma_err("Error in setting up %d txdesc ring. ret: %d",
						 txdesc_ring->id, ret);
				return -ENOMEM;
			}

			txdesc_info[i].status_flags |= EDMA_RING_STATUS_FLAGS_IS_CONFIGURED;
		}
	}

	/*
	 * Allocate TxCmpl ring descriptors
	 */
	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			struct edma_txcmpl_ring *txcmpl_ring = NULL;
			int32_t ret;

			txcmpl_ring = txcmpl_info[i].txcmpl_ring;
			txcmpl_ring->count = txcmpl_info[i].desc_count;

			ret = edma_cfg_tx_cmpl_ring_setup(txcmpl_ring);
			if (ret != 0) {
				edma_err("Error in setting up %d txcmpl ring. ret: %d",
						 txcmpl_ring->id, ret);
				return -ENOMEM;
			}

			txcmpl_info[i].status_flags |= EDMA_RING_STATUS_FLAGS_IS_CONFIGURED;
		}
	}

	return 0;
}

/*
 * edma_cfg_tx_rings_alloc()
 *	Allocate EDMA Tx rings
 */
int32_t edma_cfg_tx_rings_alloc(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring_info *txdesc_info = egc->txdesc_info;
	struct edma_txcmpl_ring_info *txcmpl_info = egc->txcmpl_info;

	for (int i = 0; i < egc->txdesc_ring_max; i++) {
		if (txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			if (txdesc_info[i].txdesc_ring) {
				edma_err("TX desc Ring %d is already allocated \n", i);
				return -ENOMEM;
			}

			txdesc_info[i].txdesc_ring = kzalloc(sizeof(struct edma_txdesc_ring), GFP_KERNEL);
			if (!txdesc_info[i].txdesc_ring) {
				edma_err("Error in allocating txdesc ring\n");
				return -ENOMEM;
			}

			txdesc_info[i].txdesc_ring->id = i;

			/*
			 * Initialize the preheader mode parameter to invalid
			 */
			txdesc_info[i].txdesc_ring->pre_hdr_mode_en = EDMA_RING_MODE_NOT_SET;
		}
	}

	for (int i = 0; i < egc->txcmpl_ring_max; i++) {
		if (txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			if (txcmpl_info[i].txcmpl_ring) {
				edma_err("TX cmpl Ring %d is already allocated \n", i);
				return -ENOMEM;
			}

			txcmpl_info[i].txcmpl_ring = kzalloc(sizeof(struct edma_txcmpl_ring), GFP_KERNEL);
			if (!txcmpl_info[i].txcmpl_ring) {
				edma_err("Error in allocating txcmpl ring\n");
				return -ENOMEM;
			}

			txcmpl_info[i].txcmpl_ring->id = i;
		}
	}

	if (edma_cfg_tx_rings_setup(egc)) {
		edma_err("Error in setting up tx rings\n");
		return -ENOMEM;
	}

	return 0;
}

/*
 * edma_cfg_tx_rings_cleanup()
 *	Cleanup EDMA rings
 */
void edma_cfg_tx_rings_cleanup(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	/*
	 * Free any buffers assigned to any descriptors
	 */
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (!(egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		if (egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED) {
			edma_cfg_tx_desc_ring_cleanup(egc, egc->txdesc_info[i].txdesc_ring);
		}

		if (egc->txdesc_info[i].txdesc_ring) {
			kfree(egc->txdesc_info[i].txdesc_ring);
			egc->txdesc_info[i].txdesc_ring = NULL;
		}

		egc->txdesc_info[i].status_flags = 0;
	}

	/*
	 * Free Tx completion descriptors
	 */
	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		if (egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED) {
			edma_cfg_tx_cmpl_ring_cleanup(egc, egc->txcmpl_info[i].txcmpl_ring);
		}

		if (egc->txcmpl_info[i].txcmpl_ring) {
			kfree(egc->txcmpl_info[i].txcmpl_ring);
			egc->txcmpl_info[i].txcmpl_ring = NULL;
		}

		egc->txcmpl_info[i].status_flags = 0;
	}
}

/*
 * edma_cfg_tx_rings()
 *	Configure EDMA rings
 */
void edma_cfg_tx_rings(struct edma_gbl_ctx *egc)
{
	uint32_t i = 0;

	/*
	 * Configure TXDESC ring
	 */
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)
			edma_cfg_tx_desc_ring_configure(egc->txdesc_info[i].txdesc_ring);
	}

	/*
	 * Configure TXCMPL ring
	 */
	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)
			edma_cfg_tx_cmpl_ring_configure(egc->txcmpl_info[i].txcmpl_ring);
	}
}

/*
 * edma_cfg_tx_napi_enable()
 *	Enable Tx NAPI
 */
void edma_cfg_tx_napi_enable(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		struct edma_txcmpl_ring *txcmpl_ring;

		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		txcmpl_ring = egc->txcmpl_info[i].txcmpl_ring;

		if (!txcmpl_ring->napi_added) {
			continue;
		}

		/*
		 * TODO:
		 * Enable NAPI separately for each port at the time of DP open
		 */
		napi_enable(&txcmpl_ring->napi);
	}
}

/*
 * edma_cfg_tx_napi_disable()
 *	Disable Tx NAPI
 */
void edma_cfg_tx_napi_disable(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		struct edma_txcmpl_ring *txcmpl_ring;

		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		txcmpl_ring = egc->txcmpl_info[i].txcmpl_ring;

		if (!txcmpl_ring->napi_added) {
			continue;
		}

		napi_disable(&txcmpl_ring->napi);
	}
}

/*
 * edma_cfg_tx_napi_delete()
 *	Delete Tx NAPI
 */
void edma_cfg_tx_napi_delete(struct edma_gbl_ctx *egc)
{
	uint32_t i;

	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		struct edma_txcmpl_ring *txcmpl_ring;

		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		txcmpl_ring = egc->txcmpl_info[i].txcmpl_ring;

		if (!txcmpl_ring->napi_added) {
			continue;
		}

		netif_napi_del(&txcmpl_ring->napi);
		txcmpl_ring->napi_added = false;
	}
}

/*
 * edma_cfg_tx_napi_add()
 *	TX NAPI add API
 */
void edma_cfg_tx_napi_add(struct edma_gbl_ctx *egc, struct net_device *netdev, uint32_t macid)
{
	struct edma_txcmpl_ring *txcmpl_ring;
#ifdef NSS_DP_MHT_SW_PORT_MAP
	struct nss_dp_dev *dp_dev = (struct nss_dp_dev *)netdev_priv(netdev);
#endif

	if ((nss_dp_tx_napi_budget < EDMA_TX_NAPI_WORK_MIN) ||
		(nss_dp_tx_napi_budget > EDMA_TX_NAPI_WORK_MAX)) {
		edma_err("Incorrect Tx NAPI budget: %d, setting to default: %d",
				nss_dp_tx_napi_budget, NSS_DP_HAL_TX_NAPI_BUDGET);
		nss_dp_tx_napi_budget = NSS_DP_HAL_TX_NAPI_BUDGET;
	}

	/*
	 * Adding tx napi for a interface with each queue.
	 */
	for (int i = 0; i < egc->txcmpl_ring_max; i++) {
		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE))
			continue;

		txcmpl_ring = egc->txcmpl_info[i].txcmpl_ring;
#ifdef NSS_DP_MHT_SW_PORT_MAP
		if (txcmpl_ring->napi_added) {
			continue;
		}
#endif

		if (txcmpl_ring->napi_added) {
			continue;
		}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
		netif_napi_add(netdev, &txcmpl_ring->napi,
				edma_tx_napi_poll, nss_dp_tx_napi_budget);
#else
		netif_napi_add_weight(netdev, &txcmpl_ring->napi,
				edma_tx_napi_poll, nss_dp_tx_napi_budget);
#endif
		txcmpl_ring->napi_added = true;
		edma_debug("Napi added for txcmpl ring: %u\n", txcmpl_ring->id);
	}

#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (!dp_dev->nss_dp_mht_dev)
		goto done;

	for (index = NSS_DP_MHT_MAP_IDX; index < egc->max_tx_ports; index++) {
		for_each_possible_cpu(i) {
			ring_idx = egc->txcmpl_map[index][i] -
					egc->txcmpl_ring_start;
			txcmpl_ring = &egc->txcmpl_rings[ring_idx];

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
			netif_napi_add(netdev, &txcmpl_ring->napi,
					edma_tx_napi_poll,
					nss_dp_tx_napi_budget);
#else
			netif_napi_add_weight(netdev, &txcmpl_ring->napi,
					edma_tx_napi_poll,
					nss_dp_tx_napi_budget);
#endif
			txcmpl_ring->napi_added = true;
			edma_debug("Napi added for txcmpl ring: %u\n",
					txcmpl_ring->id);
		}
	}
done:
#endif
	edma_info("Tx NAPI budget: %d\n", nss_dp_tx_napi_budget);
}
