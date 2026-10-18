/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "edma_cfg_tx_loopback.h"
#include "edma_regs.h"
#include "edma_debug.h"

/*
 * edma_cfg_tx_cmpl_loopback_ring_cleanup()
 *	Cleanup resources for one TxCmpl ring
 */
static void edma_cfg_tx_cmpl_loopback_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_txcmpl_ring *txcmpl_ring)
{
	/*
	 * Free TxCmpl ring descriptors
	 */
	dma_free_coherent(&egc->pdev->dev,
		(sizeof(struct edma_txcmpl_desc) * txcmpl_ring->count),
		txcmpl_ring->desc, txcmpl_ring->dma);
	txcmpl_ring->desc = NULL;
	txcmpl_ring->dma = (dma_addr_t)0;
}

/*
 * edma_cfg_tx_cmpl_ring_setup()
 *	Setup resources for one TxCmpl ring
 */
static int edma_cfg_tx_cmpl_loopback_ring_setup(struct edma_txcmpl_ring *txcmpl_ring)
{
	struct edma_gbl_ctx *egc = &edma_gbl_ctx;
	struct platform_device *pdev = egc->pdev;

	txcmpl_ring->desc = dma_alloc_coherent(&pdev->dev, (sizeof(struct edma_txcmpl_desc) * txcmpl_ring->count),
                                &txcmpl_ring->dma, GFP_KERNEL | __GFP_ZERO);
	if (!txcmpl_ring->desc) {
		edma_err("Descriptor alloc for TXCMPL ring %u failed\n",
				txcmpl_ring->id);
		return -ENOMEM;
	}

	return 0;
}

/*
 * edma_cfg_tx_desc_loopback_ring_cleanup()
 *	Cleanup resources for one TxDesc ring
 *
 * This API expects ring to be disabled by caller
 */
static void edma_cfg_tx_desc_loopback_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_txdesc_ring *txdesc_ring)
{
	uint32_t prod_idx, cons_idx, data, cons_idx_prev;;

	/*
	 * Free any buffers assigned to any descriptors
	 */
	data = edma_reg_read(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id));
	prod_idx = data & EDMA_TXDESC_PROD_IDX_MASK;

	data = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(txdesc_ring->id));
	cons_idx_prev = cons_idx = data & EDMA_TXDESC_CONS_IDX_MASK;

	/*
	 * Free Tx ring descriptors
	 */
	kfree(txdesc_ring->pdesc);
	txdesc_ring->pdesc = NULL;
	txdesc_ring->pdma = (dma_addr_t)0;

	kfree(txdesc_ring->sdesc);
	txdesc_ring->sdesc = NULL;
	txdesc_ring->sdma = (dma_addr_t)0;
}

/*
 * edma_cfg_tx_desc_loopback_ring_setup()
 *	Setup resources for one TxDesc ring
 */
static int edma_cfg_tx_desc_loopback_ring_setup(struct edma_txdesc_ring *txdesc_ring)
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
 * edma_cfg_tx_desc_loopback_ring_configure()
 *	Configure one TxDesc ring in EDMA HW
 */
static void edma_cfg_tx_desc_loopback_ring_configure(struct edma_txdesc_ring *txdesc_ring)
{
	uint32_t paddr, saddr;

	/*
	 * Configure TXDESC ring
	 */
	paddr = (uint32_t)(txdesc_ring->pdma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA(txdesc_ring->id), paddr);

	saddr = (uint32_t)(txdesc_ring->sdma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA2(txdesc_ring->id), saddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
        paddr = (uint32_t)((txdesc_ring->pdma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
        edma_reg_write(EDMA_REG_TXDESC_BA_HIGH(txdesc_ring->id), paddr);

        saddr = (uint32_t)((txdesc_ring->sdma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
        edma_reg_write(EDMA_REG_TXDESC_BA2_HIGH(txdesc_ring->id), saddr);
#endif

	edma_reg_write(EDMA_REG_TXDESC_RING_SIZE(txdesc_ring->id),
			(uint32_t)(txdesc_ring->count &
			EDMA_TXDESC_RING_SIZE_MASK));

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id),
			(uint32_t)EDMA_TX_INITIAL_PROD_IDX);

	/*
	 * Configure group ID for flow control for this Tx ring
	 */
	edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id),
			EDMA_TXDESC_CTRL_FC_GRP_ID_SET(txdesc_ring->fc_grp_id));
}

/*
 * edma_cfg_tx_cmpl_loopback_ring_configure()
 *	Configure one TxCmpl ring in EDMA HW
 */
static void edma_cfg_tx_cmpl_loopback_ring_configure(struct edma_txcmpl_ring *txcmpl_ring)
{
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
}

/*
 * edma_cfg_tx_loopback_rings_enable()
 *	API to enable TX rings
 */
void edma_cfg_tx_loopback_rings_enable(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring;
	uint32_t data = 0;
	int i = 0;
	int ring_id;

	/*
	 * Enable Tx rings
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		ring_id = egc->txdesc_loopback_ring_id_arr[i];
		txdesc_ring = &egc->txdesc_loopback_rings[ring_id];
		data = edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id));
		data |= EDMA_TXDESC_CTRL_TXEN_SET(EDMA_TXDESC_TX_ENABLE);
		edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), data);
	}
}

/*
 * edma_cfg_tx_loopback_rings_disable()
 *	API to disable TX rings
 */
void edma_cfg_tx_loopback_rings_disable(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring;
	uint32_t data;
	int ring_id;
	int i = 0;

	/*
	 * Disable Tx rings
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		ring_id = egc->txdesc_loopback_ring_id_arr[i];
		txdesc_ring = &egc->txdesc_loopback_rings[ring_id];
		data = edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id));
		data &= ~EDMA_TXDESC_TX_ENABLE;
		edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), data);
	}
}

/*
 * edma_cfg_tx_loopback_mapping()
 *	API to setup TX ring mapping
 */
void edma_cfg_tx_loopback_mapping(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring = NULL;
	uint32_t data, reg;
	int i = 0;
	int ring_id;

	for (i = 0; i < egc->num_loopback_rings; i++) {
		txdesc_ring = &egc->txdesc_loopback_rings[i];
		ring_id = txdesc_ring->id;

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

		edma_debug("Configure point offload TXDESC:%u to use TXCMPL:%u\n", ring_id, &egc->txcmpl_loopback_ring[ring_id]);

		/*
		 * Set the Tx complete descriptor ring number in the mapping register.
		 * E.g. If (txcmpl ring)desc_index = 31, (txdesc ring)i = 28.
		 * 	reg = EDMA_REG_TXDESC2CMPL_MAP_4
		 * 	data |= (desc_index & 0x1F) << ((i % 6) * 5);
		 * 	data |= (0x1F << 20); -
		 * 	This sets 11111 at 20th bit of register EDMA_REG_TXDESC2CMPL_MAP_4
		 */
		data = edma_reg_read(reg);
		data |= (ring_id & EDMA_TXDESC2CMPL_MAP_TXDESC_MASK) << ((ring_id % 6) * 5);
		edma_reg_write(reg, data);

		egc->tx_to_txcmpl_map[ring_id] = ring_id;
	}
}

/*
 * edma_cfg_tx_loopback_ring_setup()
 *	Allocate/setup resources for EDMA rings
 */
static int edma_cfg_tx_loopback_rings_setup(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring = NULL;
	struct edma_txcmpl_ring *txcmpl_ring = NULL;
	int32_t ret;
	int i = 0;

	/*
	 * Allocate TxDesc ring descriptors
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		txdesc_ring = &egc->txdesc_loopback_rings[i];
		txdesc_ring->count = egc->loopback_ring_size;
		txdesc_ring->id = egc->txdesc_loopback_ring_id_arr[i];

		ret = edma_cfg_tx_desc_loopback_ring_setup(txdesc_ring);
		if (ret != 0) {
			edma_err("Error in setting up %d txdesc ring. ret: %d", txdesc_ring->id, ret);
			return -ENOMEM;
		}

		/*
		 * Allocate TxCmpl ring descriptors
		 */
		txcmpl_ring = &egc->txcmpl_loopback_rings[i];
		txcmpl_ring->count = egc->loopback_ring_size;
		txcmpl_ring->id = egc->txcmpl_loopback_ring_id_arr[i];

		ret = edma_cfg_tx_cmpl_loopback_ring_setup(txcmpl_ring);
		if (ret != 0) {
			edma_err("Error in setting up %d txcmpl ring. ret: %d", txcmpl_ring->id, ret);
			goto txcmpl_mem_alloc_fail;
		}
	}

	edma_info("Tx descriptor count for Tx desc and Tx complete rings: %d\n", egc->loopback_ring_size);

	return 0;

txcmpl_mem_alloc_fail:
	for (i = 0; i < egc->num_loopback_rings; i++) {
		edma_cfg_tx_desc_loopback_ring_cleanup(egc, &egc->txdesc_loopback_rings[i]);
	}

	return -ENOMEM;
}

/*
 * edma_cfg_tx_loopback_rings_alloc()
 *	Allocate EDMA Tx rings
 */
int32_t edma_cfg_tx_loopback_rings_alloc(struct edma_gbl_ctx *egc)
{
	egc->txdesc_loopback_rings = kzalloc(sizeof(struct edma_txdesc_ring) * egc->num_loopback_rings, GFP_KERNEL);
	if (!egc->txdesc_loopback_rings) {
		edma_err("Error in allocating txdesc loopback ring\n");
		return -ENOMEM;
	}

	egc->txcmpl_loopback_rings = kzalloc(sizeof(struct edma_txcmpl_ring) * egc->num_loopback_rings, GFP_KERNEL);
	if (!egc->txcmpl_loopback_rings) {
		edma_err("Error in allocating txcmpl loopback ring\n");
		goto txcmpl_ring_alloc_fail;
	}

	if (edma_cfg_tx_loopback_rings_setup(egc)) {
		edma_err("Error in setting up tx rings\n");
		goto tx_rings_setup_fail;
	}

	return 0;

tx_rings_setup_fail:
	kfree(egc->txcmpl_loopback_rings);
	egc->txcmpl_loopback_rings = NULL;
txcmpl_ring_alloc_fail:
	kfree(egc->txdesc_loopback_rings);
	egc->txdesc_loopback_rings = NULL;
	return -ENOMEM;
}

/*
 * edma_cfg_tx_loopback_rings_cleanup()
 *	Cleanup EDMA rings
 */
void edma_cfg_tx_loopback_rings_cleanup(struct edma_gbl_ctx *egc)
{
	int i = 0;

	/*
	 * Free any buffers assigned to any descriptors
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		edma_cfg_tx_desc_loopback_ring_cleanup(egc, &egc->txdesc_loopback_rings[i]);
		edma_cfg_tx_cmpl_loopback_ring_cleanup(egc, &egc->txcmpl_loopback_rings[i]);
	}

	kfree(egc->txdesc_loopback_rings);
	kfree(egc->txcmpl_loopback_rings);
	egc->txdesc_loopback_rings = NULL;
	egc->txcmpl_loopback_rings = NULL;
}

/*
 * edma_cfg_tx_loopback_rings()
 *	Configure loopbackome EDMA rings
 */
void edma_cfg_tx_loopback_rings(struct edma_gbl_ctx *egc)
{
	int i = 0, ring_id;

	/*
	 * Configure TXDESC ring
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		ring_id = egc->txdesc_loopback_ring_id_arr[i];
		edma_cfg_tx_desc_loopback_ring_configure(&egc->txdesc_loopback_rings[ring_id]);

		ring_id = egc->txcmpl_loopback_ring_id_arr[i];
		edma_cfg_tx_cmpl_loopback_ring_configure(&egc->txcmpl_loopback_rings[ring_id]);
	}
}
