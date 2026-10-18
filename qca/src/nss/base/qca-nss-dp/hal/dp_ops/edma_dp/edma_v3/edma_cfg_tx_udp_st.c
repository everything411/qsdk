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
#include "edma_cfg_tx_udp_st.h"
#include "edma_regs.h"
#include "edma_debug.h"
#include "edma_cfg_tx.h"
#include <ppe_drv_sc.h>
#include <ppe_drv.h>

/*
 * edma_cfg_tx_udp_st_ring_enable()
 *	API to enable TX ring
 */
void edma_cfg_tx_udp_st_ring_enable(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring = egc->udp_st_ctx.tx_ring;
	uint32_t data = 0;

	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id));
	data |= EDMA_TXDESC_CTRL_TXEN_SET(EDMA_TXDESC_TX_ENABLE);
	edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), data);
}

/*
 * edma_cfg_tx_udp_st_ring_disable()
 *	API to disable TX ring
 */
void edma_cfg_tx_udp_st_ring_disable(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring = egc->udp_st_ctx.tx_ring;
	uint32_t data;

	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(txdesc_ring->id));
	data &= ~EDMA_TXDESC_TX_ENABLE;
	edma_reg_write(EDMA_REG_TXDESC_CTRL(txdesc_ring->id), data);
}

/*
 * edma_cfg_tx_cmpl_udp_st_ring_cleanup()
 *	Cleanup resources for one TxCmpl ring
 */
static void edma_cfg_tx_cmpl_udp_st_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_txcmpl_ring *txcmpl_ring)
{
	/*
	 * SKBs are freed in edma_cfg_tx_desc_udp_st_ring_cleanup() which
	 * runs before this function. Only free the TxCmpl ring descriptors here.
	 */
	kfree(txcmpl_ring->desc);
	txcmpl_ring->desc = NULL;
	txcmpl_ring->dma = (dma_addr_t)0;
}

/*
 * edma_cfg_tx_desc_udp_st_ring_cleanup()
 *	Cleanup resources for one TxDesc ring
 *
 * This API expects ring to be disabled by caller
 */
static void edma_cfg_tx_desc_udp_st_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_txdesc_ring *txdesc_ring)
{
	struct edma_pri_txdesc *txdesc;
	struct sk_buff *skb;
	int i, j;

	/*
	 * Walk the TX descriptor ring and free each unique SKB exactly once.
	 *
	 * The UDP-ST ring reuses the same SKB across multiple descriptors —
	 * one SKB per flow, repeated (ring_size / skb_count) times.
	 * Walking cons_idx to prod_idx and freeing each opaque would free
	 * the same SKB multiple times, causing a use-after-free.
	 *
	 * For each non-NULL opaque, free the SKB and then null out every
	 * subsequent descriptor that references the same pointer so that
	 * later iterations skip it.
	 */
	for (i = 0; i < txdesc_ring->count; i++) {
		txdesc = EDMA_TXDESC_PRI_DESC(txdesc_ring, i);
		skb = (struct sk_buff *)EDMA_TXDESC_OPAQUE_GET(txdesc);
		if (!skb)
			continue;

		dev_kfree_skb_any(skb);

		/*
		 * Null out all descriptors (from i onwards) that reference
		 * this same SKB to prevent double-free on subsequent iterations.
		 */
		for (j = i; j < txdesc_ring->count; j++) {
			struct edma_pri_txdesc *d = EDMA_TXDESC_PRI_DESC(txdesc_ring, j);
			if ((struct sk_buff *)EDMA_TXDESC_OPAQUE_GET(d) == skb)
				EDMA_TXDESC_OPAQUE_SET(d, NULL);
		}
	}

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

	kfree(txdesc_ring->sdesc);
	txdesc_ring->sdesc = NULL;
	txdesc_ring->sdma = (dma_addr_t)0;
}

/*
 * edma_cfg_tx_udp_st_mapping()
 *	API to setup TX ring mapping
 */
void edma_cfg_tx_udp_st_mapping(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_ring = egc->udp_st_ctx.tx_ring;
	uint32_t data, reg;
	int ring_id;

	ring_id = txdesc_ring->id;

	if (ring_id > 23) {
		edma_err("UDP-ST: Invalid ring_id %d (max supported: 23)\n", ring_id);
		return;
	}

	reg = EDMA_REG_TXDESC2CMPL_MAP_0 +
		(ring_id / EDMA_TXDESC2CMPL_MAP_NUM_IN_SINGLE_REG) * sizeof(uint32_t);

	edma_debug("Configure point offload TXDESC:%u to use TXCMPL:%u\n",
		   ring_id, egc->udp_st_ctx.tx_cmpl_ring->id);

	data = edma_reg_read(reg);
	data |= (ring_id & EDMA_TXDESC2CMPL_MAP_TXDESC_MASK) << ((ring_id % EDMA_TXDESC2CMPL_MAP_NUM_IN_SINGLE_REG)
						* EDMA_TXDESC2CMPL_MAP_TXDESC_ID_BIT_COUNT);
	edma_reg_write(reg, data);
}

/*
 * edma_cfg_tx_udp_st_ring_setup()
 *	Allocate/setup resources for UDP-ST EDMA ring
 */
static int edma_cfg_tx_udp_st_ring_setup(struct edma_gbl_ctx *egc)
{
	struct edma_txdesc_ring *txdesc_info = egc->udp_st_ctx.tx_ring;
	struct edma_txcmpl_ring *txcmpl_info = egc->udp_st_ctx.tx_cmpl_ring;
	int32_t ret;

	/*
	 * Allocate TxDesc ring descriptors
	 */
	txdesc_info->count = edma_udp_st_ring_size;
	txdesc_info->id = edma_udp_st_tx_ring;

	/*
	 * Set the Flow Control group ID from the module parameter.
	 */
	txdesc_info->fc_grp_id = (uint32_t)edma_udp_st_fc_grp_id;
	edma_info("UDP-ST: TX ring %d assigned FC group ID %u\n",
		  txdesc_info->id, txdesc_info->fc_grp_id);

	ret = edma_cfg_tx_desc_ring_setup(txdesc_info);
	if (ret != 0) {
		edma_err("Error in setting up UDP-ST %d txdesc ring. ret: %d",
					txdesc_info->id, ret);
		return -ENOMEM;
	}

	/*
	 * Allocate TxCmpl ring descriptors
	 */
	txcmpl_info->count = edma_udp_st_ring_size;
	txcmpl_info->id = edma_udp_st_tx_cmpl_ring;

	ret = edma_cfg_tx_cmpl_ring_setup(txcmpl_info);
	if (ret != 0) {
		edma_err("Error in setting up %d txcmpl ring. ret: %d",
					txcmpl_info->id, ret);
		/*
		 * Free the txdesc ring descriptors already allocated by
		 * edma_cfg_tx_desc_ring_setup() to avoid a memory leak.
		 */
		edma_cfg_tx_desc_udp_st_ring_cleanup(egc, txdesc_info);
		return -ENOMEM;
	}

	return 0;
}

/*
 * edma_cfg_tx_desc_udp_st_ring_configure()
 *	Configure one TxDesc ring in EDMA HW
 */
static void edma_cfg_tx_desc_udp_st_ring_configure(struct edma_txdesc_ring *txdesc_ring)
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
 * edma_cfg_tx_cmpl_udp_st_ring_configure()
 *	Configure one TxCmpl ring in EDMA HW
 */
static void edma_cfg_tx_cmpl_udp_st_ring_configure(struct edma_txcmpl_ring *txcmpl_ring)
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
 * edma_cfg_tx_udp_st_ring_alloc()
 *	Allocate EDMA Tx ring
 */
int32_t edma_cfg_tx_udp_st_ring_alloc(struct edma_gbl_ctx *egc)
{
	int ret;

	egc->udp_st_ctx.tx_ring = kzalloc(sizeof(struct edma_txdesc_ring), GFP_KERNEL);
	if (!egc->udp_st_ctx.tx_ring) {
		edma_err("Error in allocating udp-st txdesc ring\n");
		return -ENOMEM;
	}

	egc->udp_st_ctx.tx_cmpl_ring = kzalloc(sizeof(struct edma_txcmpl_ring), GFP_KERNEL);
	if (!egc->udp_st_ctx.tx_cmpl_ring) {
		edma_err("Error in allocating udp-st txcmpl ring\n");
		kfree(egc->udp_st_ctx.tx_ring);
		egc->udp_st_ctx.tx_ring = NULL;
		return -ENOMEM;
	}

	ret = edma_cfg_tx_udp_st_ring_setup(egc);
	if (ret != 0) {
		edma_err("Error in setting up tx ring\n");
		kfree(egc->udp_st_ctx.tx_cmpl_ring);
		egc->udp_st_ctx.tx_cmpl_ring = NULL;
		kfree(egc->udp_st_ctx.tx_ring);
		egc->udp_st_ctx.tx_ring = NULL;
		return -ENOMEM;
	}

	return 0;
}

/*
 * edma_cfg_tx_udp_st_ring_cleanup()
 *	Cleanup EDMA ring
 */
void edma_cfg_tx_udp_st_ring_cleanup(struct edma_gbl_ctx *egc)
{
	/*
	 * Free any buffers assigned to any descriptors
	 */
	edma_cfg_tx_desc_udp_st_ring_cleanup(egc, egc->udp_st_ctx.tx_ring);
	edma_cfg_tx_cmpl_udp_st_ring_cleanup(egc, egc->udp_st_ctx.tx_cmpl_ring);

	kfree(egc->udp_st_ctx.tx_ring);
	kfree(egc->udp_st_ctx.tx_cmpl_ring);
	egc->udp_st_ctx.tx_ring = NULL;
	egc->udp_st_ctx.tx_cmpl_ring = NULL;
}

/*
 * edma_cfg_tx_udp_st_ring()
 *	Configure UDP-ST EDMA ring
 */
void edma_cfg_tx_udp_st_ring(struct edma_gbl_ctx *egc)
{
	/*
	 * Configure TXDESC and TXCMPL ring
	 */
	edma_cfg_tx_desc_udp_st_ring_configure(egc->udp_st_ctx.tx_ring);
	edma_cfg_tx_cmpl_udp_st_ring_configure(egc->udp_st_ctx.tx_cmpl_ring);
}

/*
 * nss_dp_udp_st_init()
 *	Initialise the UDP-ST TX ring context
 */
int nss_dp_udp_st_init(void)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;

	if (edma_udp_st_tx_ring == EDMA_RING_FLAGS_INVALID_ID
		|| edma_udp_st_tx_cmpl_ring == EDMA_RING_FLAGS_INVALID_ID) {
		edma_warn("UDP-ST: no TX rings configured, skipping init\n");
		return 0;
	}

	if (edma_udp_st_tx_ring != NSS_DP_UDP_ST_TX_RING_ID) {
		edma_err("UDP-ST: TX ring must be %d, got %d\n",
			 NSS_DP_UDP_ST_TX_RING_ID, edma_udp_st_tx_ring);
		return -EINVAL;
	}

	if (egc->udp_st_ctx.initialized) {
		edma_warn("UDP-ST: already initialised\n");
		return 0;
	}

	if (!egc->udp_st_ctx.tx_ring || !egc->udp_st_ctx.tx_cmpl_ring) {
		edma_warn("UDP-ST: no rings allocated\n");
		return -EINVAL;
	}

	egc->udp_st_ctx.loop_skb = NULL;

	spin_lock_init(&egc->udp_st_ctx.lock);

	edma_reg_write(EDMA_REG_TX_INT_MASK(egc->udp_st_ctx.tx_cmpl_ring->id), 0);

	edma_cfg_tx_udp_st_ring_enable(egc);

	egc->udp_st_ctx.initialized = true;

	pr_info("UDP-ST: TX ring %u (base_phys=0x%llx, size=%u) mapped to"
		  " TX cmpl ring %u (interrupt MASKED)\n",
		  egc->udp_st_ctx.tx_ring->id,
		  (unsigned long long)egc->udp_st_ctx.tx_ring->pdma,
		  egc->udp_st_ctx.tx_ring->count,
		  egc->udp_st_ctx.tx_cmpl_ring->id);

	return 0;
}
EXPORT_SYMBOL(nss_dp_udp_st_init);

/*
 * nss_dp_udp_st_deinit()
 *	Disable the UDP-ST TX ring and release loaded SKBs.
 *
 * Must be called before the PPE VP or WAN interface is torn down so that
 * EDMA stops referencing those ports/devices.
 */
void nss_dp_udp_st_deinit(void)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txdesc_ring *tx_ring;
	unsigned long flags;
	int i;

	if (!egc->udp_st_ctx.initialized)
		return;

	edma_cfg_tx_udp_st_ring_disable(egc);

	spin_lock_irqsave(&egc->udp_st_ctx.lock, flags);

	tx_ring = egc->udp_st_ctx.tx_ring;

	if (egc->udp_st_ctx.loop_skb) {
		dev_kfree_skb_any(egc->udp_st_ctx.loop_skb);
		egc->udp_st_ctx.loop_skb = NULL;

		/*
		 * Null out all opaque fields in the TX ring descriptors so that
		 * the subsequent edma_cfg_tx_desc_udp_st_ring_cleanup() does not
		 * attempt to free already-released SKBs (double-free).
		 */
		if (tx_ring) {
			for (i = 0; i < tx_ring->count; i++) {
				struct edma_pri_txdesc *txd = EDMA_TXDESC_PRI_DESC(tx_ring, i);
				EDMA_TXDESC_OPAQUE_SET(txd, NULL);
			}
		}
	}

	egc->udp_st_ctx.initialized = false;

	spin_unlock_irqrestore(&egc->udp_st_ctx.lock, flags);

	edma_info("UDP-ST: TX ring %u deinit complete\n",
		  tx_ring ? tx_ring->id : 0);
}
EXPORT_SYMBOL(nss_dp_udp_st_deinit);

/*
 * nss_dp_udp_st_reset_indices()
 *	Set the TX ring producer index and TX completion ring consumer
 *	index to 0
 */
void nss_dp_udp_st_reset_indices(void)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txdesc_ring *tx_ring;
	unsigned long flags;

	if (!egc->udp_st_ctx.initialized) {
		edma_err("UDP-ST: context not initialised\n");
		return;
	}

	tx_ring = egc->udp_st_ctx.tx_ring;

	spin_lock_irqsave(&egc->udp_st_ctx.lock, flags);

	/*
	 * Reset the SOFTWARE and HARDWARE txdesc producer index and tx completion
	 * consumer index to 0 so the rings are ready for normal processing
	 */
	tx_ring->prod_idx = 0;
	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(tx_ring->id), 0);

	edma_info("UDP-ST: TX ring %u producer index reset to 0\n",
		  tx_ring->id);

	spin_unlock_irqrestore(&egc->udp_st_ctx.lock, flags);

	/*
	 * Wait until the hardware TX ring consumer index catches up to the
	 * producer index after reset, confirming the ring is fully drained.
	 */
	while ((edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(tx_ring->id)) &
		EDMA_TXDESC_CONS_IDX_MASK) != tx_ring->prod_idx);

	edma_info("UDP-ST: TX ring %u consumer index reached producer index (0)\n",
		  tx_ring->id);
}
EXPORT_SYMBOL(nss_dp_udp_st_reset_indices);

/*
 * nss_dp_udp_st_xmit()
 *	Copies all the rules skb into txdesc and assigns service code and other
 *	parameters in txdesc
 */
int nss_dp_udp_st_xmit(struct nss_dp_udp_st_xmit_info *xmit_info)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txdesc_ring *tx_ring;
	struct edma_txcmpl_ring *tx_cmpl_ring;
	struct edma_pri_txdesc *txd;
	dma_addr_t buff_addr;
	uint32_t buf_len;
	unsigned long flags;
	struct sk_buff *skb = xmit_info->skb;
	int skb_idx = xmit_info->skb_idx;
	int skb_count = xmit_info->skb_count;
	uint16_t vp_num = xmit_info->vp_num;
	bool is_veip = xmit_info->is_veip;
	bool is_gem_port = xmit_info->is_gem_port;
	int i;

	if (unlikely(!egc->udp_st_ctx.initialized)) {
		edma_err("UDP-ST: xmit: context not initialised\n");
		return -EINVAL;
	}

	if (unlikely(!xmit_info || !skb || skb_idx < 0 || skb_count <= 0 || skb_idx >= skb_count)) {
		edma_err("UDP-ST: xmit: invalid args (skb=%px skb_idx=%d skb_count=%d)\n",
			 skb, skb_idx, skb_count);
		return -EINVAL;
	}

	tx_ring = egc->udp_st_ctx.tx_ring;
	tx_cmpl_ring = egc->udp_st_ctx.tx_cmpl_ring;

	if (unlikely(!tx_ring || !tx_cmpl_ring)) {
		edma_err("UDP-ST: xmit: ring pointers NULL\n");
		return -EINVAL;
	}

	buf_len   = skb_headlen(skb);

	if (edma_udp_st_pass_through_mode == EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA) {
		buff_addr = (dma_addr_t)virt_to_phys(skb->data);
	} else {
		buff_addr = (dma_addr_t)virt_to_phys(skb->data - EDMA_DDRQ_PREHEADER_SIZE);
	}

	spin_lock_irqsave(&egc->udp_st_ctx.lock, flags);

	edma_dmac_clean_range_no_dsb((void *)skb->data,
				     (void *)(skb->data + buf_len));

	for (i = skb_idx; i < tx_ring->count; i += skb_count) {
		txd = EDMA_TXDESC_PRI_DESC(tx_ring, i);
		memset(txd, 0, sizeof(struct edma_pri_txdesc));

		EDMA_TXDESC_BUFFER_ADDR_SET(txd, buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
		EDMA_TXDESC_BUFFER_ADDR_HI_SET(txd, buff_addr);
#endif
		/*
		 * Use the configurable pass-through mode from module parameter.
		 * Default is EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA (3).
		 * If mode is not FULL_DATA, set the data offset.
		 */
		if (edma_udp_st_pass_through_mode != EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA) {
			EDMA_TXDESC_DATA_OFFSET_SET(txd, EDMA_DDRQ_PREHEADER_SIZE);
		}
		EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, edma_udp_st_pass_through_mode);
		EDMA_TXDESC_DATA_LEN_SET(txd, buf_len);

		if (unlikely(skb->ip_summed == CHECKSUM_PARTIAL)) {
			EDMA_TXDESC_ADV_OFFLOAD_SET(txd);
			EDMA_TXDESC_IP_CSUM_SET(txd);
			EDMA_TXDESC_L4_CSUM_SET(txd);
		}

		if (is_gem_port) {
			if (is_veip) {
				/*
				 * HGU case
				 */
				EDMA_TXDESC_SERVICE_CODE_SET(txd, PPE_DRV_SC_UDP_ST_HGU);
			} else {
				/*
				 * SFU case
				 */
				EDMA_TXDESC_SERVICE_CODE_SET(txd, PPE_DRV_SC_UDP_ST_SFU);
			}
		} else {
			EDMA_TXDESC_SERVICE_CODE_SET(txd, PPE_DRV_SC_UDP_ST);
		}

		EDMA_TXDESC_FAKE_MAC_HDR_SET(txd, 0);

		/*
		 * src_info = VP port number (type=2 VP, id=vp_port).
		 * PPE uses this as the ingress port for flow lookup.
		 * dst_info = 0 (PPE routes based on the matched flow rule).
		 */
		EDMA_SRC_INFO_SET(txd, vp_num);
		EDMA_DST_INFO_SET(txd, 0);

		EDMA_TXDESC_OPAQUE_SET(txd, skb);

		EDMA_TXDESC_ENDIAN_SET(txd);
	}

	if (skb_idx == 0) {
		egc->udp_st_ctx.loop_skb = skb;
	}

	if (skb_idx == skb_count - 1) {
		edma_dmac_clean_range_no_dsb(EDMA_TXDESC_PRI_DESC(tx_ring, 0),
					     EDMA_TXDESC_PRI_DESC(tx_ring, 0) + tx_ring->count);

		edma_dsb();

		tx_ring->prod_idx = edma_udp_st_ring_size & EDMA_TXDESC_PROD_IDX_MASK;
		edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(tx_ring->id),
			       tx_ring->prod_idx);

		/*
		 * Set the TX-completion consumer index to INVALID so NAPI
		 * never tries to drain this ring while the loop is running.
		 */
		tx_cmpl_ring->cons_idx = (edma_udp_st_ring_size + 1) & EDMA_TXCMPL_CONS_IDX_MASK;
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(tx_cmpl_ring->id),
			       tx_cmpl_ring->cons_idx);

		edma_info("UDP-ST: xmit ring=%u doorbell=INVALID(%u) vp_port=%u"
			  " skb_count=%d ring_count=%u - HW loop started"
			  " (round-robin descriptors filled)\n",
			  tx_ring->id, (edma_udp_st_ring_size + 1),
			  vp_num, skb_count, tx_ring->count);
	}

	spin_unlock_irqrestore(&egc->udp_st_ctx.lock, flags);

	return 0;
}
EXPORT_SYMBOL(nss_dp_udp_st_xmit);
