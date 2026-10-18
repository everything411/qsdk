// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/ieee80211.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <crypto/hash.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include "core.h"
#include "debug.h"
#include "hw.h"
#include "dp.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "peer.h"
#include "dp_mon.h"
#include "debugfs_htt_stats.h"
#include "erp.h"
#include "fse.h"
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_QCN_EXTN */
#endif
#include "vendor.h"
#include "qca-vendor.h"

void ath12k_dp_rx_tid_free_desc(struct ath12k_base *ab,
				struct ath12k_dp_rx_tid *rx_tid)
{
	if (!ab->hw_params->alloc_cacheable_memory) {
		dma_free_coherent(ab->dev, rx_tid->size, rx_tid->vaddr,
				  rx_tid->paddr);
		rx_tid->paddr = 0;
	} else {
		ath12k_core_dma_unmap_single(ab->dev, rx_tid->paddr, rx_tid->size,
					     DMA_BIDIRECTIONAL);
		kfree(rx_tid->vaddr);
	}
	rx_tid->vaddr = NULL;
}
EXPORT_SYMBOL(ath12k_dp_rx_tid_free_desc);

void ath12k_tid_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].rx_pkt_stats[reason]++;
	tstats->tid_stats[tid].rx_pkt_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_rx_stats);

void ath12k_tid_drop_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].rx_drop_stats[reason]++;
	tstats->tid_stats[tid].rx_drop_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_drop_rx_stats);

size_t ath12k_dp_list_cut_nodes(struct list_head *list,
		struct list_head *head,
		size_t count,
		uint8_t pool_type)
{
	struct list_head *cur;
	struct ath12k_rx_desc_info *rx_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(list);
		goto out;
	}

	list_for_each(cur, head) {
		if (!count)
			break;

		rx_desc = list_entry(cur, struct ath12k_rx_desc_info, list);
		rx_desc->is_ppe_desc = pool_type;
		count--;
		nodes++;
	}

	list_cut_before(list, head, cur);
out:
	return nodes;
}

int ath12k_dp_rx_crypto_mic_len(struct ath12k_dp *dp, enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		return 0;
	case HAL_ENCRYPT_TYPE_CCMP_128:
		return IEEE80211_CCMP_MIC_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_256:
		return IEEE80211_CCMP_256_MIC_LEN;
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		return IEEE80211_GCMP_MIC_LEN;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		break;
	}

	ath12k_warn(dp->ab, "unsupported encryption type %d for mic len\n", enctype);
	return 0;
}

int ath12k_dp_rx_crypto_param_len(struct ath12k_dp *dp, enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
		return 0;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		return IEEE80211_TKIP_IV_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_128:
		return IEEE80211_CCMP_HDR_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_256:
		return IEEE80211_CCMP_256_HDR_LEN;
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		return IEEE80211_GCMP_HDR_LEN;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		break;
	}

	ath12k_warn(dp->ab, "unsupported encryption type %d\n", enctype);
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_rx_crypto_param_len);

int ath12k_dp_rx_crypto_icv_len(struct ath12k_dp *dp, enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
	case HAL_ENCRYPT_TYPE_CCMP_128:
	case HAL_ENCRYPT_TYPE_CCMP_256:
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		return 0;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		return IEEE80211_TKIP_ICV_LEN;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		break;
	}

	ath12k_warn(dp->ab, "unsupported encryption type %d\n", enctype);
	return 0;
}

static int ath12k_dp_rx_extract_tuple(struct sk_buff *skb,
				      struct cfg80211_qm_tclas4_params *flow_params)
{
	struct ethhdr *eth;
	u16 eth_type;
	struct udphdr *uh;

	eth = (struct ethhdr *)skb->data;
	eth_type = ntohs(eth->h_proto);

	if (eth_type != ETH_P_IP &&
	    eth_type != ETH_P_IPV6)
		return -EINVAL;

	if (eth_type == ETH_P_IP) {
		struct iphdr *iph = (struct iphdr *)(skb->data + sizeof(struct ethhdr));

		if (iph->protocol != IPPROTO_TCP &&
		    iph->protocol != IPPROTO_UDP)
			return -EINVAL;

		flow_params->protocol = iph->protocol;
		flow_params->ip_ver = IP_VERSION_4;

		memcpy(flow_params->src_ip.ipv4, &iph->saddr, IPV4_LEN);
		memcpy(flow_params->dst_ip.ipv4, &iph->daddr, IPV4_LEN);

		uh = (struct udphdr *)((u8 *)iph + ip_hdrlen(iph));
		flow_params->src_port = ntohs(uh->source);
		flow_params->dst_port = ntohs(uh->dest);

	} else if (eth_type == ETH_P_IPV6) {
		struct ipv6hdr *ipv6h = (struct ipv6hdr *)(skb->data +
				sizeof(struct ethhdr));

		if (ipv6h->nexthdr != IPPROTO_TCP &&
		    ipv6h->nexthdr != IPPROTO_UDP)
			return -EINVAL;

		flow_params->ip_ver = IP_VERSION_6;
		flow_params->protocol = ipv6h->nexthdr;

		memcpy(flow_params->src_ip.ipv6, &ipv6h->saddr, IPV6_LEN);
		memcpy(flow_params->dst_ip.ipv6, &ipv6h->daddr, IPV6_LEN);

		uh = (struct udphdr *)(ipv6h + 1);
		flow_params->src_port = ntohs(uh->source);
		flow_params->dst_port = ntohs(uh->dest);
	}

	return 0;
}

static int
ath12k_dp_rx_mscs_add_fse_flow_entry(struct ath12k_base *ab,
				     struct cfg80211_qm_tclas4_params
				     *flow_params)
{
	struct rx_flow_info flow_info = {0};
	struct hal_flow_tuple_info *tuple_info = &flow_info.flow_tuple_info;
	u32 *src_ip, *dst_ip;
	u8 version = flow_params->ip_ver;

	tuple_info->src_port = flow_params->src_port;
	tuple_info->dest_port = flow_params->dst_port;
	tuple_info->l4_protocol = flow_params->protocol;

	if (version == IP_VERSION_4) {
		flow_info.is_addr_ipv4 = 1;
		src_ip = (u32 *)flow_params->src_ip.ipv4;
		dst_ip = (u32 *)flow_params->dst_ip.ipv4;
		tuple_info->src_ip_31_0 = *src_ip;
		tuple_info->dest_ip_31_0 = *dst_ip;
	} else if (version == IP_VERSION_6) {
		src_ip = (u32 *)flow_params->src_ip.ipv6;
		dst_ip = (u32 *)flow_params->dst_ip.ipv6;

		tuple_info->src_ip_127_96 = src_ip[0];
		tuple_info->src_ip_95_64  = src_ip[1];
		tuple_info->src_ip_63_32  = src_ip[2];
		tuple_info->src_ip_31_0   = src_ip[3];

		tuple_info->dest_ip_127_96 = dst_ip[0];
		tuple_info->dest_ip_95_64  = dst_ip[1];
		tuple_info->dest_ip_63_32  = dst_ip[2];
		tuple_info->dest_ip_31_0   = dst_ip[3];
	} else {
		return -EINVAL;
	}

	flow_info.fse_metadata = ATH12K_RX_FSE_FLOW_MSCS_RULE_PROGRAMMED;
	return ath12k_dp_rx_flow_add_entry(ab, &flow_info);
}

void ath12k_dp_rx_classify_mscs(struct ath12k_base *ab,
				struct ath12k_dp_peer *peer,
				struct sk_buff *skb, u8 tid)
{
	struct cfg80211_qm_tclas4_params flow_params = {0};

	if (ath12k_dp_rx_extract_tuple(skb, &flow_params))
		return;

	rcu_read_lock();
	if (ieee80211_rx_send_mscs_tuple(ath12k_dp_peer_get_sta(peer), flow_params,
					 tid)) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	if (ath12k_dp_rx_mscs_add_fse_flow_entry(ab, &flow_params))
		return;

	return;
}
EXPORT_SYMBOL(ath12k_dp_rx_classify_mscs);

void ath12k_dp_rx_h_undecap_frag(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				 enum hal_encrypt_type enctype, u32 flags)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	size_t crypto_len;
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;

	if (!flags)
		return;

	hdr = (struct ieee80211_hdr *)(msdu->data + hal_rx_desc_sz);

	if (flags & RX_FLAG_MIC_STRIPPED)
		skb_trim(msdu, msdu->len -
			 ath12k_dp_rx_crypto_mic_len(dp, enctype));

	if (flags & RX_FLAG_ICV_STRIPPED)
		skb_trim(msdu, msdu->len -
			 ath12k_dp_rx_crypto_icv_len(dp, enctype));

	if (flags & RX_FLAG_IV_STRIPPED) {
		hdr_len = ieee80211_hdrlen(hdr->frame_control);
		crypto_len = ath12k_dp_rx_crypto_param_len(dp, enctype);

		memmove(msdu->data + hal_rx_desc_sz + crypto_len,
			msdu->data + hal_rx_desc_sz, hdr_len);
		skb_pull(msdu, crypto_len);
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_h_undecap_frag);

int ath12k_dp_rx_h_michael_mic(struct crypto_shash *tfm, u8 *key,
			       struct ieee80211_hdr *hdr, u8 *data,
			       size_t data_len, u8 *mic)
{
	SHASH_DESC_ON_STACK(desc, tfm);
	u8 mic_hdr[16] = {0};
	u8 tid = 0;
	int ret;

	if (!tfm)
		return -EINVAL;

	desc->tfm = tfm;

	ret = crypto_shash_setkey(tfm, key, 8);
	if (ret)
		goto out;

	ret = crypto_shash_init(desc);
	if (ret)
		goto out;

	/* TKIP MIC header */
	memcpy(mic_hdr, ieee80211_get_DA(hdr), ETH_ALEN);
	memcpy(mic_hdr + ETH_ALEN, ieee80211_get_SA(hdr), ETH_ALEN);
	if (ieee80211_is_data_qos(hdr->frame_control))
		tid = ieee80211_get_tid(hdr);
	mic_hdr[12] = tid;

	ret = crypto_shash_update(desc, mic_hdr, 16);
	if (ret)
		goto out;
	ret = crypto_shash_update(desc, data, data_len);
	if (ret)
		goto out;
	ret = crypto_shash_final(desc, mic);
out:
	shash_desc_zero(desc);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_h_michael_mic);

static void ath12k_dp_rx_mld_addr_conv(struct ath12k_pdev_dp *dp_pdev,
				       struct sk_buff *msdu,
				       struct hal_rx_desc *rx_desc, u16 peer_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_hdr *hdr = (void *)msdu->data;

	spin_lock_bh(&dp_pdev->dp->dp_lock);
	peer = ath12k_dp_rx_h_find_peer(dp_pdev, rx_desc, peer_id);
	if (!peer || !peer->mlo) {
		spin_unlock_bh(&dp_pdev->dp->dp_lock);
		return;
	}
	ether_addr_copy(hdr->addr2, peer->ml_addr);
	spin_unlock_bh(&dp_pdev->dp->dp_lock);
}

void ath12k_dp_rx_h_undecap_raw(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				struct hal_rx_desc *rx_desc,
				enum hal_encrypt_type enctype,
				struct ieee80211_rx_status *status, bool decrypted,
				u16 peer_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	size_t crypto_len;

	pskb_trim(msdu, msdu->len - FCS_LEN);

	if (!decrypted)
		return;

	ath12k_dp_rx_mld_addr_conv(dp_pdev, msdu, rx_desc, peer_id);

	hdr = (void *)msdu->data;

	/* Tail */
	if (status->flag & RX_FLAG_IV_STRIPPED) {
		pskb_trim(msdu, msdu->len -
			  ath12k_dp_rx_crypto_mic_len(dp, enctype));

		pskb_trim(msdu, msdu->len -
			  ath12k_dp_rx_crypto_icv_len(dp, enctype));
	} else {
		/* MIC */
		if (status->flag & RX_FLAG_MIC_STRIPPED)
			pskb_trim(msdu, msdu->len -
				  ath12k_dp_rx_crypto_mic_len(dp, enctype));

		/* ICV */
		if (status->flag & RX_FLAG_ICV_STRIPPED)
			pskb_trim(msdu, msdu->len -
				  ath12k_dp_rx_crypto_icv_len(dp, enctype));
	}

	/* MMIC */
	if ((status->flag & RX_FLAG_MMIC_STRIPPED) &&
	    !ieee80211_has_morefrags(hdr->frame_control) &&
	    enctype == HAL_ENCRYPT_TYPE_TKIP_MIC)
		pskb_trim(msdu, msdu->len - IEEE80211_CCMP_MIC_LEN);

	/* Head */
	if (status->flag & RX_FLAG_IV_STRIPPED) {
		hdr_len = ieee80211_hdrlen(hdr->frame_control);
		crypto_len = ath12k_dp_rx_crypto_param_len(dp, enctype);

		memmove(msdu->data + crypto_len, msdu->data, hdr_len);
		pskb_pull(msdu, crypto_len);
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_h_undecap_raw);

static void ath12k_dp_rx_enqueue_free(struct ath12k_dp *dp,
				      struct list_head *used_list,
				      bool reuse)
{
	struct ath12k_rx_desc_info *rx_desc, *tmp_rx_desc;
	struct ath12k_base *ab;
	struct sk_buff *skb;
	const void *end;

	/* Reset the use flag */
	list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {
		rx_desc->in_use = false;
		rx_desc->is_frag = 0;

		if (rx_desc->skb) {
			skb = rx_desc->skb;

			end = rx_desc->vaddr + DP_RX_BUFFER_SIZE;
			ath12k_core_dmac_inv_range(rx_desc->vaddr, end);
			IPA_SET_RX_BUF_SMMU_UNMAP(dp->ab, skb, true);

			/* Save SKB to queue instead of freeing */
			if (reuse) {
				ab = dp->ab;
				skb_queue_tail(&ab->dp_umac_reset.rx_skb_queue, skb);
			} else {
				dev_kfree_skb_any(skb);
			}
		}
		dp->device_stats.free_excess_alloc_skb++;

		rx_desc->skb = NULL;
		rx_desc->vaddr = NULL;
		rx_desc->paddr = 0;
	}
	spin_lock_bh(&dp->rx_desc_lock);
	list_splice_tail(used_list, &dp->rx_desc_free_list);
	spin_unlock_bh(&dp->rx_desc_lock);
}

#ifdef CPTCFG_EXT_IPA_OFFLOAD
/**
 * ath12k_dp_alloc_rx_skb() - Allocate an RX skb, page-aligned for PPE pool buffers.
 *
 * PPE-pool buffers must start on a page boundary so that the IPA SMMU mapping
 * covers the full buffer without straddling pages.  For DP_RX_PPE_POOL, allocate
 * via alloc_skb to get PAGE_ALIGNED.
 * Regular pool buffers use the normal allocator.
 */
static struct sk_buff *ath12k_dp_alloc_rx_skb(struct ath12k_base *ab,
					      struct ath12k_rx_desc_info *rx_desc)
{
	unsigned long pg_offset;
	struct sk_buff *skb;

	if (rx_desc->is_ppe_desc != DP_RX_PPE_POOL)
		return ath12k_dp_alloc_skb(DP_RX_BUFFER_SIZE);

	skb = alloc_skb(ATH12K_IPA_DP_RX_BUF_SIZE, GFP_ATOMIC);
	if (unlikely(!skb))
		return NULL;

	pg_offset = (unsigned long)skb->data & (PAGE_SIZE - 1);
	if (pg_offset) {
		dev_kfree_skb_any(skb);
		skb = NULL;
		ath12k_warn(ab, "IPA RX buf: Buf is not PAGE ALIGNED\n");
	}

	return skb;
}

/**
 * ath12k_dp_rx_ipa_dma_mask_save() - Restrict DMA mask to 32 bits for IPA+SMMU.
 *
 * When IPA+SMMU is active every RX buffer must reside within 32-bit DMA space.
 * Save the current mask and apply the narrower one; restore with
 * ath12k_dp_rx_ipa_dma_mask_restore() after the alloc loop.
 */
static void ath12k_dp_rx_ipa_dma_mask_save(struct ath12k_base *ab,
					   struct ath12k_dp *dp,
					   u64 *saved_mask, bool *mask_set)
{
	if (!IPA_CTX(ab) || !IPA_CTX(ab)->is_smmu_enabled)
		return;

	*saved_mask = dma_get_mask(dp->dev);
	if (!dma_set_mask(dp->dev, DMA_BIT_MASK(ATH12K_IPA_RX_BUF_DMA_BITS)))
		*mask_set = true;
	else
		ath12k_warn(ab,
			    "IPA RX bufs: failed to set %u-bit DMA mask; SMMU faults likely\n",
			    ATH12K_IPA_RX_BUF_DMA_BITS);
}

/**
 * ath12k_dp_rx_ipa_dma_mask_restore() - Restore the DMA mask saved by
 *                                        ath12k_dp_rx_ipa_dma_mask_save().
 */
static void ath12k_dp_rx_ipa_dma_mask_restore(struct ath12k_dp *dp,
					      u64 saved_mask, bool mask_set)
{
	if (mask_set)
		dma_set_mask(dp->dev, saved_mask);
}

/**
 * ath12k_dp_rx_ipa_smmu_buf_map() - Map a PPE-pool RX buffer into IPA SMMU domain.
 *
 * Called after the WLAN DMA map succeeds for a DP_RX_PPE_POOL buffer.
 * On failure, rolls back the SMMU map, the WLAN DMA map, and frees the skb,
 * leaving rx_desc in a clean state for the caller to break out of the loop.
 *
 * Returns 0 on success, negative errno on failure.
 */
static int ath12k_dp_rx_ipa_smmu_buf_map(struct ath12k_base *ab,
					 struct ath12k_dp *dp,
					 struct ath12k_rx_desc_info *rx_desc,
					 struct sk_buff *skb,
					 dma_addr_t paddr)
{
	int ret;

	if (!(rx_desc->is_ppe_desc == DP_RX_PPE_POOL && IPA_CTX(ab) &&
	      IPA_CTX(ab)->is_smmu_enabled &&
	      IPA_CTX(ab)->ipa_init_state >= ATH12K_IPA_STATE_SETUP_DONE))
		return 0;

	ATH12K_SKB_CB(skb)->paddr = paddr;
	ret = ath12k_dp_ipa_handle_buf_smmu_map_unmap(ab, skb, DP_RX_BUFFER_SIZE,
						      1, IPA_CTX(ab)->hdl);
	if (unlikely(ret)) {
		ath12k_warn(ab, "IPA SMMU map failed for RX buf: %d\n", ret);
		ath12k_dp_ipa_handle_buf_smmu_map_unmap(ab, skb, DP_RX_BUFFER_SIZE,
							false, IPA_CTX(ab)->hdl);
		ath12k_core_dma_unmap_single(dp->dev, paddr, DP_RX_BUFFER_SIZE,
					     DMA_FROM_DEVICE);
		ATH12K_SKB_CB(skb)->paddr = 0;
		rx_desc->skb = NULL;
		rx_desc->paddr = 0;
		rx_desc->vaddr = NULL;
		dev_kfree_skb_any(skb);
	}

	return ret;
}

#else /* !CPTCFG_EXT_IPA_OFFLOAD */

static inline struct sk_buff *ath12k_dp_alloc_rx_skb(struct ath12k_base *ab,
						     struct ath12k_rx_desc_info *rx_desc)
{
#if defined(CONFIG_SKB_RECYCLE_SIZE) && (CONFIG_SKB_RECYCLE_SIZE == 1664)
	return ath12k_dp_alloc_skb(DP_RX_BUFFER_SIZE - NET_SKB_PAD);
#endif
	return ath12k_dp_alloc_skb(DP_RX_BUFFER_SIZE);
}

static inline void ath12k_dp_rx_ipa_dma_mask_save(struct ath12k_base *ab,
						   struct ath12k_dp *dp,
						   u64 *saved_mask, bool *mask_set)
{
}

static inline void ath12k_dp_rx_ipa_dma_mask_restore(struct ath12k_dp *dp,
						      u64 saved_mask, bool mask_set)
{
}

static inline int ath12k_dp_rx_ipa_smmu_buf_map(struct ath12k_base *ab,
						 struct ath12k_dp *dp,
						 struct ath12k_rx_desc_info *rx_desc,
						 struct sk_buff *skb,
						 dma_addr_t paddr)
{
	return 0;
}

#endif /* CPTCFG_EXT_IPA_OFFLOAD */

/* Returns number of Rx buffers replenished */
void ath12k_dp_rx_bufs_replenish(struct ath12k_dp *dp,
				 struct hal_srng *srng,
				 struct list_head *used_list, bool reuse)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_buffer_addr *desc;
	struct sk_buff *skb;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc, *tmp_rx_desc;
	u8 mgr = dp->hal->hal_params->rx_buf_rbm;
	int allocated_entries = 0;

	/* Check if descriptors are already initialized (reuse mode) */
	if (reuse) {
		/* Count entries for reuse */
		list_for_each_entry(rx_desc, used_list, list)
			allocated_entries++;
	} else {
		/* Normal mode: allocate and initialize new descriptors */
		u64 ipa_saved_mask = 0;
		bool ipa_mask_set = false;

		ath12k_dp_rx_ipa_dma_mask_save(ab, dp, &ipa_saved_mask, &ipa_mask_set);

		list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {
			skb = ath12k_dp_alloc_rx_skb(ab, rx_desc);
			if (unlikely(!skb))
				break;

#if defined(CONFIG_SKB_RECYCLE_SIZE) && (CONFIG_SKB_RECYCLE_SIZE == 1664)
			skb_reserve(skb, -NET_SKB_PAD);
#endif
			rx_desc->skb = skb;
			rx_desc->vaddr = skb->data;
			rx_desc->is_frag = 0;
			rx_desc->in_use = true;

			paddr = ath12k_dp_rx_buffer_map(dp, rx_desc);

			if (unlikely(paddr == DMA_MAPPING_ERROR)) {
				ath12k_dp_rx_skb_free(skb, dp, 0,
						      DP_RX_ERR_DROP_REPLENISH,
						      NULL, 0);
				rx_desc->skb = NULL;
				rx_desc->vaddr = NULL;
				break;
			}

			rx_desc->paddr = paddr;
			allocated_entries++;

			if (unlikely(ath12k_dp_rx_ipa_smmu_buf_map(ab, dp, rx_desc,
								   skb, paddr))) {
				allocated_entries--;
				break;
			}
		}

		ath12k_dp_rx_ipa_dma_mask_restore(dp, ipa_saved_mask, ipa_mask_set);
		ath12k_dsb();
	}

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	while (allocated_entries > 0) {
		rx_desc = list_first_entry_or_null(used_list, struct ath12k_rx_desc_info, list);
		if (unlikely(!rx_desc))
			goto out;

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!desc))
			goto out;

		list_del(&rx_desc->list);

		allocated_entries--;

		ath12k_hal_rx_buf_addr_info_set(desc, rx_desc->paddr, rx_desc->cookie, mgr);
	}

out:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	if (unlikely(!list_empty(used_list)))
		ath12k_dp_rx_enqueue_free(dp, used_list, reuse);
}
EXPORT_SYMBOL(ath12k_dp_rx_bufs_replenish);

static int ath12k_dp_rxdma_ring_buf_setup(struct ath12k_base *ab,
					  struct dp_rxdma_ring *rx_ring)
{
	LIST_HEAD(list);
	size_t req_entries;
	struct hal_srng *refill_srng;

	rx_ring->bufs_max = rx_ring->refill_buf_ring.size /
			ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_BUF);

	refill_srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
	req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, refill_srng, &list,
				DP_RX_DEFAULT_POOL);
	if (req_entries)
		ath12k_dp_rx_bufs_replenish(ab->dp, refill_srng, &list, false);

	return 0;
}

int ath12k_dp_rxdma_buf_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	ret = ath12k_dp_rxdma_ring_buf_setup(ab, &dp->rx_refill_buf_ring);
	if (ret) {
		ath12k_warn(ab,
			    "failed to setup HAL_RXDMA_BUF\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_rxdma_buf_setup);

void ath12k_dp_rx_reo_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	for (i = 0; i < ATH12K_DP_RX_REGULAR_RING_MAX; i++)
		ath12k_dp_srng_cleanup(ab, &dp->reo_dst_ring[i]);
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_cleanup);

int ath12k_dp_rx_reo_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;
	int i;

	for (i = 0; i < ATH12K_DP_RX_REGULAR_RING_MAX; i++) {
		ret = ath12k_dp_srng_alloc(ab, &dp->reo_dst_ring[i],
					   HAL_REO_DST, i, 0,
					   ath12k_dp_reo_dst_ring_size[i]);
		if (ret) {
			ath12k_warn(ab, "failed to setup reo_dst_ring\n");
			goto err_reo_cleanup;
		}
	}

	return 0;

err_reo_cleanup:
	ath12k_dp_rx_reo_cleanup(ab);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_alloc);

int ath12k_dp_rx_reo_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;
	int i;

	for (i = 0; i < ATH12K_DP_RX_REGULAR_RING_MAX; i++) {
		ret = ath12k_dp_srng_init(ab, &dp->reo_dst_ring[i],
					  HAL_REO_DST, i, 0);
		if (ret) {
			ath12k_warn(ab, "failed to setup reo_dst_ring\n");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_init);

int ath12k_dp_rx_reo_setup(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_dp_rx_reo_alloc(ab);
	if (ret)
		return ret;

	ret = ath12k_dp_rx_reo_init(ab);
	if (ret) {
		ath12k_dp_rx_reo_cleanup(ab);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_setup);

void ath12k_dp_rx_reo_cmd_list_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_reo_cmd *cmd, *tmp;
	struct ath12k_dp_rx_reo_cache_flush_elem *cmd_cache, *tmp_cache;
	struct ath12k_dp_rx_tid *rx_tid;
	struct dp_reo_update_rx_queue_elem *cmd_queue, *tmp_queue;

	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_for_each_entry_safe(cmd_queue, tmp_queue, &dp->reo_cmd_update_rx_queue_list,
				 list) {
		list_del(&cmd_queue->list);
		rx_tid = &cmd_queue->data;
		if (rx_tid->vaddr) {
			rx_tid->active = false;
			ath12k_dp_rx_tid_free_desc(ab, rx_tid);
		}
		kfree(cmd_queue);
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	spin_lock_bh(&dp->reo_cmd_lock);
	list_for_each_entry_safe(cmd, tmp, &dp->reo_cmd_list, list) {
		struct hal_reo_status drain_status = {};

		list_del(&cmd->list);
		drain_status.uniform_hdr.cmd_status = HAL_REO_CMD_DRAIN;
		cmd->handler(dp, &cmd->u.data, &drain_status);
		kfree(cmd);
	}

	list_for_each_entry_safe(cmd_cache, tmp_cache,
				 &dp->reo_cmd_cache_flush_list, list) {
		list_del(&cmd_cache->list);
		dp->reo_cmd_cache_flush_count--;
		rx_tid = &cmd_cache->data;
		if (rx_tid->vaddr) {
			rx_tid->active = false;
			ath12k_dp_rx_tid_free_desc(ab, rx_tid);
		}
		kfree(cmd_cache);
	}
	spin_unlock_bh(&dp->reo_cmd_lock);
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_cmd_list_cleanup);

void ath12k_dp_reo_cmd_free(struct ath12k_dp *dp, void *ctx,
			    struct hal_reo_status *status)
{
	struct ath12k_dp_rx_tid *rx_tid = ctx;

	if (!status || status->uniform_hdr.cmd_status == HAL_REO_CMD_DRAIN)
		goto free_desc;
	else if (status->uniform_hdr.cmd_status != HAL_REO_CMD_SUCCESS)
		ath12k_warn(dp->ab, "failed to flush rx tid hw desc, tid %d status %d\n",
			    rx_tid->tid, status->uniform_hdr.cmd_status);

free_desc:
	ath12k_hal_reo_shared_qaddr_cache_clear(dp->ab);

	if (rx_tid->vaddr) {
		rx_tid->active = false;
		ath12k_dp_rx_tid_free_desc(dp->ab, rx_tid);
	}
}
EXPORT_SYMBOL(ath12k_dp_reo_cmd_free);

void ath12k_dp_rx_frags_cleanup(struct ath12k_dp_rx_tid *rx_tid,
				       bool rel_link_desc)
{
	struct ath12k_buffer_addr *buf_addr_info;
	struct ath12k_dp *dp = rx_tid->dp;
	enum hal_wbm_rel_bm_act bm_act;

	lockdep_assert_held(&rx_tid->tid_lock);

	if (rx_tid->desc) {
		if (rel_link_desc) {
			bm_act = HAL_WBM_REL_BM_ACT_PUT_IN_IDLE;
			buf_addr_info =
				(struct ath12k_buffer_addr *)rx_tid->desc;
			ath12k_dp_arch_rx_link_desc_return(dp, buf_addr_info,
							   bm_act);
		}

		kfree(rx_tid->desc);
		rx_tid->desc = NULL;
	}

	rx_tid->cur_sn = 0;
	rx_tid->last_frag_no = 0;
	rx_tid->rx_frag_bitmap = 0;
	__skb_queue_purge(&rx_tid->rx_frags);
}
EXPORT_SYMBOL(ath12k_dp_rx_frags_cleanup);

void ath12k_dp_rx_peer_tid_cleanup(struct ath12k *ar,
				   struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_rx_tid *rx_tid;
	int i;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!ath12k_dp_arch_mlo_peer_tid_teardown_ready(dp, peer->dp_peer, peer))
		return;

	for (i = 0; i < ab->hal.hal_params->num_tids; i++) {
		rx_tid = &peer->dp_peer->rx_tid[i];

		spin_lock_bh(&rx_tid->tid_lock);
		ath12k_dp_arch_rx_peer_tid_delete(dp, ar, peer, i);
		ath12k_dp_rx_frags_cleanup(rx_tid, true);
		spin_unlock_bh(&rx_tid->tid_lock);

		del_timer_sync(&rx_tid->frag_timer);
	}
}

int ath12k_dp_rx_peer_tid_setup(struct ath12k *ar, struct ath12k_dp_peer *dp_peer,
				const u8 *peer_mac, int vdev_id,
				u8 tid, u32 ba_win_sz, u16 ssn,
				enum hal_pn_type pn_type)
{
	struct hal_rx_reo_queue *addr_aligned;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_rx_tid *rx_tid;
	dma_addr_t paddr;
	u16 stats_id;
	int ret;

	rcu_read_lock();

	peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, peer_mac);
	if (!peer) {
		rcu_read_unlock();
		ath12k_warn(ab, "failed to find the peer to set up rx tid\n");
		return -ENOENT;
	}

	if (!peer->primary_link) {
		rcu_read_unlock();
		return 0;
	}

	if (tid >= ab->hal.hal_params->num_tids) {
		ath12k_warn(ab, "tid %d doesn't allow reoq setup\n", tid);
		rcu_read_unlock();
		return -EINVAL;
	}

	rx_tid = &peer->dp_peer->rx_tid[tid];
	spin_lock_bh(&rx_tid->tid_lock);
	/* Update the tid queue if it is already setup */
	if (rx_tid->active) {
		paddr = rx_tid->paddr;
		ret = ath12k_dp_arch_peer_rx_tid_reo_update(dp, ar, peer, rx_tid,
							    ba_win_sz, ssn, true);
		spin_unlock_bh(&rx_tid->tid_lock);
		rcu_read_unlock();

		if (ret) {
			ath12k_warn(ab, "failed to update reo for peer %pM rx tid %d\n",
									peer_mac, tid);
			return ret;
		}

		if (!ab->hw_params->reoq_lut_support) {
			ret = ath12k_wmi_peer_rx_reorder_queue_setup(ar, vdev_id,
								     peer_mac,
								     paddr, tid, 1,
								     ba_win_sz);
			if (ret) {
				ath12k_warn(ab, "failed to setup peer rx reorder queuefor tid %d: %d\n",
					    tid, ret);
				return ret;
			}
		}

		return 0;
	}

	rx_tid->tid = tid;

	rx_tid->ba_win_sz = ba_win_sz;

	/*
	 * When VoW stats are disabled, program the peer-level stats_id
	 * into the REO queue descriptor. When VoW stats are enabled,
	 * program the per-TID stats_id so HW telemetry ring delivers
	 * isolated per-TID RX descriptors for this REO queue.
	 */

	if (tid < ATH12K_DATA_TID_MAX &&
	    peer->dp_peer->tid_stats_id[tid] < ATH12K_MAX_STATS_ID)
		stats_id = peer->dp_peer->tid_stats_id[tid];
	else
		stats_id = peer->dp_peer->stats_id;

	ret = ath12k_dp_arch_alloc_reo_qdesc(dp, rx_tid, ssn, pn_type,
					     &addr_aligned, stats_id);
	if (ret < 0) {
		spin_unlock_bh(&rx_tid->tid_lock);
		rcu_read_unlock();
		return ret;
	}

	rx_tid->active = true;

	if (ab->hw_params->reoq_lut_support) {
		/* Update the REO queue LUT at the corresponding peer id
		 * and tid with qaddr.
		 */
		ath12k_dp_arch_peer_rx_tid_qref_setup(dp, peer->dp_peer->peer_id,
						      rx_tid->tid,
						      rx_tid->paddr);

		spin_unlock_bh(&rx_tid->tid_lock);
		rcu_read_unlock();
	} else {
		spin_unlock_bh(&rx_tid->tid_lock);
		rcu_read_unlock();
		ret = ath12k_wmi_peer_rx_reorder_queue_setup(ar, vdev_id,
							     peer_mac,
							     rx_tid->paddr,
							     rx_tid->tid, 1,
							     rx_tid->ba_win_sz);
	}

	return ret;
}

int ath12k_dp_rx_ampdu_start(struct ath12k *ar,
			     struct ieee80211_ampdu_params *params,
			     u8 link_id)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(params->sta);
	struct ath12k_link_sta *arsta;
	int vdev_id;
	int ret;
	struct ath12k_dp_peer *dp_peer;
	struct wiphy *wiphy = ath12k_ar_to_hw(ar)->wiphy;

	lockdep_assert_wiphy(wiphy);

	arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				  ahsta->link[link_id]);
	if (!arsta)
		return -ENOLINK;

	vdev_id = arsta->arvif->vdev_id;

	dp_peer = (struct ath12k_dp_peer *)ath12k_sta_get_dp_peer_wiphy_locked(wiphy,
									       ahsta);
	if (!dp_peer)
		return -ENOENT;

	ret = ath12k_dp_rx_peer_tid_setup(ar, dp_peer, arsta->addr, vdev_id,
					  params->tid, params->buf_size,
					  params->ssn, arsta->ahsta->pn_type);
	if (ret)
		ath12k_warn(ab, "failed to setup rx tid %d\n", ret);

	return ret;
}

int ath12k_dp_rx_ampdu_stop(struct ath12k *ar,
			    struct ieee80211_ampdu_params *params,
			    u8 link_id)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(params->sta);
	bool active;
	int ret;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_peer *dp_peer;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ath12k_ar_to_hw(ar)->wiphy, ahsta);
	if (!dp_peer)
		return -ENOENT;

	rcu_read_lock();

	peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!peer) {
		rcu_read_unlock();
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L0,
				 "failed to find the peer to stop rx aggregation\n");
		return -ENOENT;
	}

	if (!peer->primary_link) {
		rcu_read_unlock();
		return 0;
	}

	spin_lock_bh(&dp_peer->rx_tid[params->tid].tid_lock);
	active = dp_peer->rx_tid[params->tid].active;

	if (!active) {
		spin_unlock_bh(&dp_peer->rx_tid[params->tid].tid_lock);
		rcu_read_unlock();
		return 0;
	}

	ret = ath12k_dp_arch_peer_rx_tid_reo_update(dp, ar, peer,
				&dp_peer->rx_tid[params->tid], 1, 0, false);

	spin_unlock_bh(&dp_peer->rx_tid[params->tid].tid_lock);
	rcu_read_unlock();

	if (ret) {
		ath12k_warn(ab, "failed to update reo for rx tid %d: %d\n",
			    params->tid, ret);
		return ret;
	}

	return ret;
}

int ath12k_dp_rx_peer_pn_replay_config(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr,
				       enum set_key_cmd key_cmd,
				       struct ieee80211_key_conf *key,
				       struct ieee80211_sta *sta,
				       enum ath12k_rxtid_pn_check cfg)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_rx_tid *rx_tid;
	u8 tid;
	int ret = 0;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_hw *dp_hw = ar->dp.dp_hw;
	struct ath12k_dp_peer *dp_peer;
	bool is_mgmt;

	/* NOTE: Enable PN/TSC replay check offload only for unicast frames.
	 * We use mac80211 PN/TSC replay check functionality for bcast/mcast
	 * for now.
	 */
	if (!(key->flags & IEEE80211_KEY_FLAG_PAIRWISE))
		return 0;

	rcu_read_lock();
	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, sta->addr,
						      arvif->ahvif->vif);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, peer_addr,
						      arvif->ahvif->vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		ath12k_warn(ab, "failed to find the peer %pM to configure pn replay detection\n",
			    peer_addr);
		return -ENOENT;
	}

	peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, peer_addr);
	if (!peer || !peer->primary_link) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return 0;
	}

	for (tid = 0; tid < ab->hal.hal_params->num_tids; tid++) {
		is_mgmt = ath12k_hw_is_mgmt_reoq_tid(ab->hw_params, tid);

		if (ath12k_dp_rx_peer_tid_skip_pn_replay(dp, tid) ||
		    (cfg == ATH12K_RXTID_PN_CHECK_DATA_TIDS && is_mgmt) ||
		    (cfg == ATH12K_RXTID_PN_CHECK_MGMT_TIDS && !is_mgmt))
			continue;

		rx_tid = &dp_peer->rx_tid[tid];

		spin_lock_bh(&rx_tid->tid_lock);
		if (!rx_tid->active) {
			spin_unlock_bh(&rx_tid->tid_lock);
			continue;
		}

		ath12k_dp_arch_setup_pn_check_reo_cmd(dp, &cmd, rx_tid, key->cipher,
						      key_cmd);
		ret = ath12k_dp_arch_reo_cmd_send(dp, rx_tid, sizeof(*rx_tid),
						  HAL_REO_CMD_UPDATE_RX_QUEUE,
						  &cmd, NULL);
		spin_unlock_bh(&rx_tid->tid_lock);

		if (ret) {
			ath12k_warn(ab, "failed to configure rx tid %d queue of peer %pM for pn replay detection %d\n",
				    tid, peer_addr, ret);
			break;
		}
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	rcu_read_unlock();

	return ret;
}

int ath12k_dp_rx_peer_epp_pn_replay_config(struct ath12k_dp_peer *dp_peer,
					   struct ath12k_link_vif *arvif,
					   const u8 *peer_addr,
					   enum set_key_cmd key_cmd,
					   enum ath12k_rxtid_pn_check cfg)
{
	struct ieee80211_sta *sta = ath12k_dp_peer_get_sta(dp_peer);
	struct ieee80211_key_conf *key = NULL;
	int i, ret = 0;

	if (!dp_peer || !sta)
		return -ENOENT;

	if (!dp_peer->is_epp_peer)
		return 0;

	spin_lock_bh(&dp_peer->keys_lock);
	for (i = 0; i < ARRAY_SIZE(dp_peer->keys); i++) {
		if (!dp_peer->keys[i] ||
		    !(dp_peer->keys[i]->flags & IEEE80211_KEY_FLAG_PAIRWISE))
			continue;

		key = dp_peer->keys[i];
		break;
	}

	if (key)
		ret = ath12k_dp_rx_peer_pn_replay_config(arvif, peer_addr, key_cmd, key,
							 sta, cfg);
	spin_unlock_bh(&dp_peer->keys_lock);

	return ret;
}

struct sk_buff *ath12k_dp_rx_get_msdu_last_buf(struct sk_buff_head *msdu_list,
					       struct sk_buff *first)
{
	struct sk_buff *skb;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(first);

	if (!rxcb->is_continuation)
		return first;

	skb_queue_walk(msdu_list, skb) {
		rxcb = ATH12K_SKB_RXCB(skb);
		if (!rxcb->is_continuation)
			return skb;
	}

	return NULL;
}

static struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_addr(struct ath12k_pdev_dp *dp_pdev, const u8 *addr)
{
	u32 hash;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = dp_pdev->dp;

	lockdep_assert_held(&dp->dp_lock);

	hash = jhash(addr, ETH_ALEN, 0);

	/* Compare mac address and hw_link_id */
	hash_for_each_possible(dp->link_peer_htbl, peer, hash_addr_node, hash) {
		if (ether_addr_equal(peer->addr, addr) &&
		    dp_pdev->ar->hw_link_id == peer->hw_link_id)
			return peer;
	}

	return NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_rx_h_find_peer(struct ath12k_pdev_dp *dp_pdev,
			 struct hal_rx_desc *rx_desc,
			 u16 peer_id)
{
	struct ath12k_dp_link_peer *peer = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	void *peer_mac;


	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (peer)
		return peer;

	peer_mac = ath12k_hal_rxdesc_get_mpdu_start_addr2(dp->hal, rx_desc);
	if (peer_mac)
		peer = ath12k_dp_link_peer_find_by_addr(dp_pdev, peer_mac);

	return peer;
}

void ath12k_dp_rx_deliver_msdu(struct ath12k_pdev_dp *dp_pdev,
			       struct napi_struct *napi,
			       struct sk_buff *msdu,
			       struct ieee80211_rx_status *status,
			       u8 hw_link_id, bool is_mcbc, u16 peer_id, u16 tid)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_vif *ahvif;
	u8 addr[ETH_ALEN] = {0};

	rcu_read_lock();

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);

	if (peer) {
		pubsta = ath12k_dp_peer_get_sta(peer);
		memcpy(addr, peer->addr, ETH_ALEN);
	}

	if (pubsta) {
		if (pubsta->valid_links)
			status->link_valid = 1;
		status->link_id = peer->hw_links[hw_link_id];

		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     peer_id);
		if (link_peer && link_peer->is_bridge_peer) {
			dev_kfree_skb_any(msdu);
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "Packet received on bridge peer link_id %d, drop it\n",
				   link_peer->link_id);
			rcu_read_unlock();
			return;
		}

		if (link_peer)
			WRITE_ONCE(link_peer->peer_stats.last_rx, jiffies);
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		if (peer) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(peer));
			ath12k_tid_rx_stats(ahvif, tid, msdu->len,
					    ATH_RX_TOTAL_OUT_PKTS);
		}
		msdu->priority = tid;
	}

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "rx skb %p len %u peer %pM %d %s %s%s%s%s%s%s%s%s%s%s%s rate_idx %u vht_nss %u freq %u band %u flag 0x%x fcs-err %i mic-err %i amsdu-more %i\n",
		   msdu,
		   msdu->len,
		   addr,
		   tid,
		   is_mcbc ? "mcast" : "ucast",
		   (status->encoding == RX_ENC_LEGACY) ? "legacy" : "",
		   (status->encoding == RX_ENC_HT) ? "ht" : "",
		   (status->encoding == RX_ENC_VHT) ? "vht" : "",
		   (status->encoding == RX_ENC_HE) ? "he" : "",
		   (status->encoding == RX_ENC_EHT) ? "eht" : "",
		   (status->encoding == RX_ENC_UHR) ? "uhr" : "",
		   (status->bw == RATE_INFO_BW_40) ? "40" : "",
		   (status->bw == RATE_INFO_BW_80) ? "80" : "",
		   (status->bw == RATE_INFO_BW_160) ? "160" : "",
		   (status->bw == RATE_INFO_BW_320) ? "320" : "",
		   status->enc_flags & RX_ENC_FLAG_SHORT_GI ? "sgi " : "",
		   status->rate_idx,
		   status->nss,
		   status->freq,
		   status->band, status->flag,
		   !!(status->flag & RX_FLAG_FAILED_FCS_CRC),
		   !!(status->flag & RX_FLAG_MMIC_ERROR),
		   !!(status->flag & RX_FLAG_AMSDU_MORE));

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_RX, NULL, "dp rx msdu: ",
			msdu->data, msdu->len);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	/* TODO: trace rx packet */

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
	rcu_read_unlock();
}
EXPORT_SYMBOL(ath12k_dp_rx_deliver_msdu);

void ath12k_dp_rx_frag_timer(struct timer_list *timer)
{
	struct ath12k_dp_rx_tid *rx_tid = from_timer(rx_tid, timer, frag_timer);

	spin_lock_bh(&rx_tid->tid_lock);
	if (rx_tid->last_frag_no &&
	    rx_tid->rx_frag_bitmap == GENMASK(rx_tid->last_frag_no, 0)) {
		spin_unlock_bh(&rx_tid->tid_lock);
		return;
	}
	ath12k_dp_rx_frags_cleanup(rx_tid, true);
	spin_unlock_bh(&rx_tid->tid_lock);
}
EXPORT_SYMBOL(ath12k_dp_rx_frag_timer);

int ath12k_dp_rx_peer_frag_setup(struct ath12k *ar,
				 struct ath12k_dp_link_peer *peer,
				 struct crypto_shash *tfm)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_tid *rx_tid;
	int i;

	lockdep_assert(&dp->dp_lock);

	for (i = 0; i < ab->hal.hal_params->num_tids; i++) {
		rx_tid = &peer->dp_peer->rx_tid[i];
		rx_tid->dp = dp;
		timer_setup(&rx_tid->frag_timer, ath12k_dp_rx_frag_timer, 0);
		skb_queue_head_init(&rx_tid->rx_frags);
	}

	peer->dp_peer->tfm_mmic = tfm;
	peer->dp_peer->primary_link_frag_setup = true;

	return 0;
}

int
ath12k_dp_rx_htt_rxdma_rxole_ppe_cfg_set(struct ath12k_base *ab,
					 struct ath12k_dp_htt_rxdma_ppe_cfg_param *param)
{
	struct htt_h2t_msg_type_rxdma_rxole_ppe_cfg *cmd;
	struct ath12k_dp *dp = ab->dp;
	struct sk_buff *skb;
	int len = sizeof(*cmd), ret, val;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_h2t_msg_type_rxdma_rxole_ppe_cfg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 =
		u32_encode_bits(HTT_H2T_MSG_TYPE_RXDMA_RXOLE_PPE_CFG,
				HTT_H2T_RXOLE_PPE_CFG_MSG_TYPE) |
		u32_encode_bits(param->override, HTT_H2T_RXOLE_PPE_CFG_OVERRIDE) |
		u32_encode_bits(param->reo_dst_ind,
				HTT_H2T_RXOLE_PPE_CFG_REO_DST_IND) |
		u32_encode_bits(param->multi_buffer_msdu_override_en,
				HTT_H2T_RXOLE_PPE_CFG_MULTI_BUF_MSDU_OVRD_EN) |
		u32_encode_bits(param->intra_bss_override,
				HTT_H2T_RXOLE_PPE_CFG_INTRA_BUS_OVRD) |
		u32_encode_bits(param->decap_raw_override,
				HTT_H2T_RXOLE_PPE_CFG_DECAP_RAW_OVRD) |
		u32_encode_bits(param->decap_nwifi_override,
				HTT_H2T_RXOLE_PPE_CFG_NWIFI_OVRD) |
		u32_encode_bits(param->ip_frag_override,
				HTT_H2T_RXOLE_PPE_CFG_IP_FRAG_OVRD);

	val = cmd->info0;
	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type H2T rx ole ppe config request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_PPE, "RXOLE ppe config request sent val 0x%x\n", val);

	return 0;
}

static int ath12k_dp_rx_flow_send_fst_setup(struct ath12k_base *ab,
					    struct dp_rx_fst *fst)
{
	struct htt_rx_flow_fst_setup fst_setup = {0};
	int ret;

	fst_setup.max_entries = fst->hal_rx_fst->max_entries;
	fst_setup.max_search = fst->hal_rx_fst->max_skid_length;
	fst_setup.base_addr_lo = lower_32_bits(fst->hal_rx_fst->base_paddr);
	fst_setup.base_addr_hi = upper_32_bits(fst->hal_rx_fst->base_paddr);
	fst_setup.ip_da_sa_prefix =
		HAL_FST_IP_DA_SA_PFX_TYPE_IPV4_COMPATIBLE_IPV6;
	fst_setup.hash_key = fst->hal_rx_fst->key;
	fst_setup.hash_key_len = HAL_FST_HASH_KEY_SIZE_BYTES;

	ret = ath12k_dp_htt_rx_flow_fst_setup(ab, &fst_setup);
	if (ret) {
		ath12k_err(ab, "Failed to send Rx FSE Setup:status %d\n", ret);
		return ret;
	}

	return 0;
}

struct dp_rx_fst *ath12k_dp_rx_fst_attach(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst;
	int ret;

	if (!ab->hw_params->support_fse)
		return NULL;

	fst = kzalloc(sizeof(*fst), GFP_KERNEL);
	if (!fst)
		return NULL;

	ret = ath12k_dp_arch_rx_fst_attach(dp, fst);
	if (ret) {
		kfree(fst);
		return NULL;
	}

	spin_lock_init(&fst->fst_lock);

	ath12k_info(ab, "Rx FST attach successful\n");

	return fst;
}

void ath12k_dp_rx_fst_detach(struct ath12k_base *ab, struct dp_rx_fst *fst)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!fst)
		return;

	ath12k_dp_arch_rx_fst_detach(dp, fst);
	kfree(fst);
}

static void ath12k_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info)
{
	ath12k_dp_arch_rx_flow_dump_entry(dp, flow_info);
}

int ath12k_dp_rx_flow_add_entry(struct ath12k_base *ab,
				struct rx_flow_info *flow_info)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	int ret;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}
	/* lock the FST table to prevent concurrent access */
	spin_lock_bh(&fst->fst_lock);

	ret = ath12k_dp_arch_rx_flow_add_entry(dp, flow_info);
	if (ret) {
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		fst->flow_add_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	ret = ath12k_dp_arch_rx_flow_fse_cache_op(ab->dp,
						  DP_FST_CACHE_INVALIDATE_ENTRY,
						  &flow_info->flow_tuple_info);
	if (ret) {
		ath12k_err(ab, "Unable to invalidate cache entry ret %d", ret);
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		ath12k_dp_arch_rx_flow_delete_entry(dp, flow_info);
		fst->flow_add_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	spin_unlock_bh(&fst->fst_lock);

	if (flow_info->is_addr_ipv4)
		fst->ipv4_fse_rule_cnt++;
	else
		fst->ipv6_fse_rule_cnt++;

	dp->dp_hw_grp->fse_active_count++;

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_flow_add_entry);

int ath12k_dp_rx_flow_delete_entry(struct ath12k_base *ab,
				   struct rx_flow_info *flow_info)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	int ret;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	/* lock the FST table to prevent concurrent access */
	spin_lock_bh(&fst->fst_lock);

	ret = ath12k_dp_arch_rx_flow_delete_entry(dp, flow_info);
	if (ret) {
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		fst->flow_del_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	ret = ath12k_dp_arch_rx_flow_fse_cache_op(ab->dp,
						  DP_FST_CACHE_INVALIDATE_ENTRY,
						  &flow_info->flow_tuple_info);
	if (ret) {
		ath12k_err(ab, "Rx flow delete fail due to invalidate ret %d", ret);
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		fst->flow_del_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	spin_unlock_bh(&fst->fst_lock);

	if (flow_info->is_addr_ipv4)
		fst->ipv4_fse_rule_cnt--;
	else
		fst->ipv6_fse_rule_cnt--;

	if (dp->dp_hw_grp->fse_active_count)
		dp->dp_hw_grp->fse_active_count--;

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_flow_delete_entry);

int ath12k_dp_rx_flow_delete_all_entries(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	int ret;

	spin_lock_bh(&fst->fst_lock);

	ret = ath12k_dp_arch_rx_flow_delete_all_entries(dp);
	if (ret) {
		ath12k_err(ab, "Rx flow delete all entries failed ret %d", ret);
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	ret = ath12k_dp_arch_rx_flow_fse_cache_op(dp,
						  DP_FST_CACHE_INVALIDATE_FULL,
						  NULL);
	if (ret) {
		ath12k_err(ab, "Rx flow delete all fail due to invalidate ret %d", ret);
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	spin_unlock_bh(&fst->fst_lock);

	fst->ipv4_fse_rule_cnt = 0;
	fst->ipv6_fse_rule_cnt = 0;
	dp->dp_hw_grp->fse_active_count = 0;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);
out:
	return ret;
}

/**
 * dp_rx_update_protocol_tag() - stamp rxcb->protocol_tag from CCE metadata
 *
 * Called in the REO hot path for every MSDU after MPDU validation. Reads
 * cce_match from the scratchpad; if set, decodes proto_type from cce_metadata
 * (proto_type = cce_metadata - ATH12K_RX_PROTOCOL_TAG_START_OFFSET), fetches
 * the user-programmed u16 tag from the host software map via direct O(1) index,
 * stamps rxcb->protocol_tag, and increments the per-protocol CCE hit counter.
 * No-op when CCE_MATCH is clear or no protocols are configured.
 *
 * This mirrors the PROP approach: only proto_type+offset is carried through
 * hardware; the user tag lives exclusively in the host map with no hardware
 * width constraint (full u16 range supported).
 */
void dp_rx_update_protocol_tag(struct ath12k_pdev_dp *dp_pdev,
			       struct sk_buff *msdu,
			       const struct dp_rx_tag_params *tag)
{
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	u16 cce_meta;
	u32 proto_type;

	if (!tag->cce_match)
		return;

	if (!dp_pdev->protocol_tag_active_count)
		return;

	cce_meta = tag->cce_metadata;

	/* Decode proto_type from hardware-stamped metadata (PROP style).
	 * Bounds check guards against stale or unexpected metadata values.
	 */
	if (cce_meta < ATH12K_RX_PROTOCOL_TAG_START_OFFSET)
		return;

	proto_type = cce_meta - ATH12K_RX_PROTOCOL_TAG_START_OFFSET;
	if (proto_type >= ATH12K_PKT_TYPE_MAX)
		return;

	if (!dp_pdev->protocol_tag_map[proto_type].enabled)
		return;

	/* Fetch the full u16 user tag from the host software map — O(1) direct
	 * index, no scan. Tag value has no hardware width constraint.
	 */
	rxcb->protocol_tag = dp_pdev->protocol_tag_map[proto_type].tag;
	dp_pdev->fse_cce_stats.cce_tagged_pkts[proto_type]++;
}
EXPORT_SYMBOL_GPL(dp_rx_update_protocol_tag);

/**
 * dp_rx_update_flow_tag() - stamp rxcb->flow_tag from FSE metadata
 *
 * Called in the REO hot path for every MSDU after MPDU validation.  Copies
 * the lower 16 bits of fse_metadata (the user-programmed flow tag) into
 * rxcb->flow_tag and increments the FSE hit counter when non-zero.
 */
void dp_rx_update_flow_tag(struct ath12k_pdev_dp *dp_pdev,
			   struct sk_buff *msdu,
			   const struct dp_rx_tag_params *tag)
{
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	u16 flow_tag;

	if (!dp_pdev->dp->dp_hw_grp->fse_active_count)
		return;

	if (tag->flow_idx_invalid || tag->flow_idx_timeout)
		return;

	flow_tag = tag->flow_metadata;
	rxcb->flow_tag = flow_tag;

	if (flow_tag)
		dp_pdev->fse_cce_stats.fse_tagged_pkts++;
}
EXPORT_SYMBOL_GPL(dp_rx_update_flow_tag);

void ath12k_dp_fst_core_map_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_fst_config *fst_config = &dp->fst_config;
	u32 fst_core_mask = fst_config->fst_core_mask;
	int i;
	int core_map_index = 0;

	for (i = 0; i < ATH12K_DP_FST_NUM_CORES; i++) {
		fst_config->fst_core_map[i] = ATH12K_DP_MAX_FST_CORE_MASK;
		if ((fst_core_mask >> i) & 0x1) {
			fst_config->fst_core_map[core_map_index] = i;
			core_map_index++;
		}
	}

	fst_config->fst_num_cores = core_map_index;
	fst_config->core_idx = 0;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "FST core_mask %x num_cores %d\n",
		   fst_config->fst_core_mask, fst_config->fst_num_cores);

	for (i = 0; i < ATH12K_DP_FST_NUM_CORES; i++)
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "FST core map[%d] %x\n",
			   i, fst_config->fst_core_map[i]);
}

void ath12k_dp_rx_fst_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	u32 tuple_mask = 0;
	int ret, i;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return;
	}

	fst->ipv4_fse_rule_cnt = 0;
	fst->ipv6_fse_rule_cnt = 0;
	dp->dp_hw_grp->fse_active_count = 0;
	dp->fst_config.fst_core_mask = 0x7;
	dp->fst_config.fst_num_cores = 0;

	ath12k_dp_fst_core_map_init(ab);
	ath12k_dp_rx_flow_send_fst_setup(ab, fst);

	if (!ath12k_fse_3_tuple_enabled)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		tuple_mask = HTT_H2T_FLOW_CLASSIFY_3_TUPLE_FIELD_ENABLE;
		ret = ath12k_dp_htt_rx_fse_3_tuple_config_send(ab, tuple_mask, i);
		if (ret) {
			ath12k_warn(ab, "FSE 3 tuple config failed for pdev:%d\n",
					i);
			return;
		}
	}
}

ssize_t ath12k_dp_dump_fst_table(struct ath12k_base *ab, char *buf, int size)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	return ath12k_dp_arch_dump_fst_table(dp, buf, size);
}

static void
ath12k_dp_tid_cleanup_cb(struct ath12k_pdev_dp *dp_pdev,
			 struct ath12k_dp_link_peer *link_peer, void *context)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
        struct ath12k_dp_rx_tid *rx_tid;
        int tid;
        void *vaddr;
        u32 *addr_aligned;

	if (!link_peer->primary_link)
		return;

	if (link_peer->dp_peer) {
		for (tid = 0; tid < ab->hal.hal_params->num_tids; tid++) {
			rx_tid = &link_peer->dp_peer->rx_tid[tid];

			spin_lock_bh(&rx_tid->tid_lock);
			if (rx_tid->active) {
				vaddr = rx_tid->vaddr;
				addr_aligned = PTR_ALIGN(vaddr,
							 HAL_LINK_DESC_ALIGN);
				ath12k_hal_reset_rx_reo_tid_q(&ab->hal,
							      addr_aligned,
							      rx_tid->ba_win_sz,
							      tid);
			}
			spin_unlock_bh(&rx_tid->tid_lock);
		}
	}
}

void ath12k_dp_tid_cleanup(struct ath12k_base *ab)
{
	ath12k_dp_link_peer_iterate_by_device(ab->dp, ath12k_dp_tid_cleanup_cb, NULL);
}
EXPORT_SYMBOL(ath12k_dp_tid_cleanup);

void
ath12k_dp_primary_peer_migrate_setup(struct ath12k_dp *dp, void *ctx,
				     struct hal_reo_status *status)
{
	struct ath12k_dp_rx_tid *rx_tid = ctx;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *mig_ab;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_dp *mig_dp;
	u16 peer_id = rx_tid->peer_id;
	u8 chip_id = rx_tid->chip_id;
	u8 pdev_id = rx_tid->pdev_id;
	int ret, tid;
	struct ath12k_pdev_dp *dp_pdev;

	if (!status || status->uniform_hdr.cmd_status != HAL_REO_CMD_SUCCESS)
		goto migration_fail;

	mig_ab = dp->ab->ag->ab[chip_id];
	mig_dp = ath12k_ab_to_dp(mig_ab);

	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(mig_dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		goto migration_fail;
	}

	spin_lock_bh(&mig_dp->dp_lock);

	/* Get peer from the pdev to which the peer is going to migrate */
	link_peer = ath12k_dp_link_peer_find_by_peerid_index(mig_dp, dp_pdev, peer_id);
	if (!link_peer || !link_peer->dp_peer) {
		ath12k_warn(mig_ab, "failed to find peer for peer_id %d\n", peer_id);
		spin_unlock_bh(&mig_dp->dp_lock);
		rcu_read_unlock();
		goto migration_fail;
	}

	ath12k_info(mig_ab, "htt new primary peer to %pM peer_id 0x%x ml_peer_id 0x%x link_id 0x%x chip_id 0x%x\n",
		    link_peer->addr, link_peer->peer_id, link_peer->ml_id,
		    link_peer->link_id, chip_id);

	ahsta = ath12k_sta_to_ahsta(ath12k_dp_link_peer_get_sta(link_peer));
	arsta = ahsta->link[link_peer->link_id];
	if (!arsta || !arsta->arvif) {
		spin_unlock_bh(&mig_dp->dp_lock);
		rcu_read_unlock();
		goto migration_fail;
	}

	if (!link_peer->dp_peer->primary_link_frag_setup) {
		ath12k_warn(mig_ab, "peer tid setup is not done for the peer_id %x in migration event\n",
			    link_peer->peer_id);
		spin_unlock_bh(&mig_dp->dp_lock);
		rcu_read_unlock();
		WARN_ON(1);
		goto migration_fail;
	}

	if (mig_ab->hw_params->reoq_lut_support) {
		/* Update the REO queue LUT at the corresponding peer id
		 * and tid with qaddr.
		 */
		for (tid = 0; tid < mig_ab->hal.hal_params->num_tids; tid++) {
			ath12k_dp_arch_peer_rx_tid_qref_setup(mig_dp,
							      link_peer->dp_peer->peer_id,
							      rx_tid->tid,
							      rx_tid->paddr);
		}
	}

	arsta->arvif->primary_sta_link = true;
	link_peer->primary_link = true;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	ath12k_info(mig_ab, "primary_link migration complete. sending WLAN_CLIENT_CONNECT_EX ml_addr=%pM",
		    link_peer->ml_addr);
	ath12k_ipa_enqueue_evt(WLAN_CLIENT_CONNECT_EX, arsta->arvif, link_peer->ml_addr,
			       true);
#endif

	ret = ath12k_vendor_put_umac_migration_notif(ath12k_dp_link_peer_get_vif(link_peer),
						     ath12k_dp_link_peer_get_sta(link_peer)->addr,
						     link_peer->link_id);
	spin_unlock_bh(&mig_dp->dp_lock);
	rcu_read_unlock();

	if (ret)
		ath12k_warn(mig_ab, "failed to send notify UMAC migration event\n");
	complete(&ahsta->dp_migration_event);
	return;

migration_fail:
	if (ahsta)
		complete(&ahsta->dp_migration_event);

	return;
}
EXPORT_SYMBOL(ath12k_dp_primary_peer_migrate_setup);

int
ath12k_dp_peer_migrate(struct ath12k_sta *ahsta, u16 peer_id,
		       u8 chip_id, u8 pdev_id)
{
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	struct ath12k *ar;
	int ret;
	struct ath12k_dp_peer *dp_peer = ath12k_sta_get_dp_peer_rcu(ahsta);

	if (!dp_peer)
		goto out;

	arsta = ahsta->link[ahsta->primary_link_id];
	if (!arsta->arvif || !arsta->arvif->ar || !arsta->arvif->ar->ab)
		goto out;

	ar = arsta->arvif->ar;
	ab = ar->ab;
	dp = ath12k_ab_to_dp(ab);

	lockdep_assert(&dp->dp_lock);

	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer,
								ahsta->primary_link_id);
	if (!link_peer || !link_peer->primary_link) {
		ath12k_warn(ab,
			    "failed to fetch primary peer for peer addr %pM in MLO pri link migration event\n",
			    arsta->addr);
		goto out;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "htt current primary peer %pM peer_id 0x%x ml_peer_id 0x%x link_id 0x%x\n",
		   link_peer->addr, link_peer->peer_id, link_peer->ml_id,
		   link_peer->link_id);

	link_peer->primary_link = false;
	arsta->arvif->primary_sta_link = false;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	ath12k_info(ab, "primary_link migration started. sending WLAN_CLIENT_DISCONNECT ml_addr=%pM",
		    link_peer->ml_addr);
	ath12k_ipa_enqueue_evt(WLAN_CLIENT_DISCONNECT, arsta->arvif, link_peer->ml_addr,
			       true);
#endif

	ret = ath12k_dp_arch_peer_migrate_reo_cmd(dp, link_peer, peer_id,
						  chip_id, pdev_id);
	if (ret) {
		ath12k_warn(ab, "failed to send reo cmd, ret:%d\n", ret);
		goto out;
	}

	return 0;
out:
	complete(&ahsta->dp_migration_event);
	return -EINVAL;
}

/* Sends WMI config to filter packets to route packets to WBM release ring */
int ath12k_dp_rx_pkt_type_filter(struct ath12k *ar,
				 enum ath12k_routing_pkt_type pkt_type,
				 u32 meta_data)
{
	struct ath12k_wmi_pkt_route_param param;
	struct ath12k_base *ab = ar->ab;
	int ret;

	/* Routing Eapol/ARP packets to CCE is only allowed now */
	if (pkt_type != ATH12K_PKT_TYPE_EAP &&
	    pkt_type != ATH12K_PKT_TYPE_ARP_IPV4)
		return -EINVAL;

	param.opcode = ATH12K_WMI_PKTROUTE_ADD;
	param.meta_data = meta_data;
	param.dst_ring = ab->hal.hal_params->dp_rx_err_rdi;
	param.dst_ring_handler = ATH12K_WMI_PKTROUTE_USE_CCE;
	param.route_type_bmap = 1 << pkt_type;

	ret = ath12k_wmi_send_pdev_pkt_route(ar, &param);
	if (ret)
		ath12k_warn(ar->ab, "failed to configure pkt route %d", ret);

	return ret;
}

void ath12k_dp_rx_skb_free(struct sk_buff *skb, struct ath12k_dp *dp, int ring,
			   enum ath12k_dp_rx_error drop_reason,
			   struct ath12k_pdev_dp *dp_pdev, u8 tid)
{
	if (ring >= DP_REO_DST_RING_MAX) {
		ath12k_dbg(dp->ab, ATH12K_DBG_TELEMETRY, "Invalid Rx Ring %u\n",
			   ring);
		ring = 0;
	}

	if (unlikely(drop_reason > DP_RX_ERR_MAX))
		DP_DEVICE_STATS_INC(dp, rx.rx_err[DP_RX_ERR_DROP_MISC][ring], 1);
	else
		DP_DEVICE_STATS_INC(dp, rx.rx_err[drop_reason][ring], 1);

	/* Update VoW stats for invalid peer drops */
	if (dp_pdev && drop_reason == DP_RX_ERR_DROP_INV_PEER) {
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_dp_vow_stats_enabled(dp_pdev)) {
			DP_PDEV_TID_RX_REASON_INC(dp_pdev, ring,
						  ath12k_vow_tid_validate(tid),
						  fail_cnt,
						  DP_TID_RX_INVALID_PEER_VDEV);
		}
	}

	IPA_SET_RX_BUF_SMMU_UNMAP(dp->ab, skb, true);
	dev_kfree_skb_any(skb);
}
EXPORT_SYMBOL(ath12k_dp_rx_skb_free);

void ath12k_dp_rx_peer_tid_ba_config(struct ath12k_dp *dp, u8 tid, u32 *ba_win_size,
				     u16 *ssn)
{
	*ba_win_size = 1;
	*ssn = 0;

	if (dp->ab->hw_params->hw_ops->rx_peer_ba_config)
		dp->ab->hw_params->hw_ops->rx_peer_ba_config(dp->ab, tid, ba_win_size,
							    ssn);
}
EXPORT_SYMBOL(ath12k_dp_rx_peer_tid_ba_config);

bool ath12k_dp_rx_peer_tid_skip_pn_replay(struct ath12k_dp *dp, u8 tid)
{
	if  (dp->ab->hw_params->hw_ops->rx_peer_tid_skip_pn_replay)
		return dp->ab->hw_params->hw_ops->rx_peer_tid_skip_pn_replay(dp->ab, tid);

	return false;
}

int ath12k_dp_rxdma_ring_sel_config(struct ath12k_base *ab)
{
	if (ab->hw_params->hw_ops->rxdma_ring_sel_config)
		return ab->hw_params->hw_ops->rxdma_ring_sel_config(ab);

	return 0;
}

void
ath12k_dp_rx_update_eapol_stats(struct ath12k_dp *dp, struct sk_buff *msdu)
{
	enum ath12k_dp_eapol_key_type subtype;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)msdu->data;
	size_t hdr_len = ieee80211_hdrlen(hdr->frame_control);
	struct ath12k_dp_rx_rfc1042_hdr *llc =
		(struct ath12k_dp_rx_rfc1042_hdr *)(msdu->data + hdr_len);

	if (llc->snap_type == cpu_to_be16(ETH_P_PAE)) {
		dp->device_stats.rx_eapol[dp->device_id]++;
		subtype = ath12k_dp_get_eapol_subtype(msdu->data + hdr_len +
						      LLC_SNAP_HDR_LEN);
		if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
			dp->device_stats.rx_eapol_type[subtype - 1][dp->device_id]++;
			ath12k_dbg_level(dp->ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
					 "Received %s%d EAPOL frame from STA %pM\n",
					 subtype <= 4 ? "M" : "G",
					 subtype <= 4 ? subtype : (subtype - 4),
					 hdr->addr2);
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_update_eapol_stats);

void
ath12k_dp_rx_update_delay_stats(struct ath12k_dp_peer *peer, struct sk_buff *msdu,
				u8 tid, u8 ring)
{
	u32 current_ts, rx_delay;
	struct ath12k_dp_peer_delay_stats *delay_stats;
	struct ath12k_dp_peer_delay_tid_stats *delay_tid_stats;

	delay_stats = peer->mld_stats.delay_stats;

	if (!delay_stats)
		return;

	delay_tid_stats = &delay_stats->delay_tid_stats[tid][ring];
	current_ts = (u32)ktime_to_ms(ktime_get_real());

	rx_delay = current_ts - (u32)ktime_to_ms(msdu->tstamp);
	ath12k_dp_update_hist_stats(&delay_tid_stats->rx_delay.to_stack_delay,
				    rx_delay);
}
EXPORT_SYMBOL(ath12k_dp_rx_update_delay_stats);

void
ath12k_dp_rx_update_vow_delay_stats(struct ath12k_pdev_dp *dp_pdev,
				    struct link_peer_rx_tid_stats *stats,
				    struct sk_buff *msdu,
				    bool da_is_mcbc, u8 tid,
				    struct ath12k_tid_rx_stats *tid_stats_ring)
{
	u32 current_ts;
	u32 reap_delay, intfrm_delay;
	struct ath12k_tid_rx_stats *tid_rx_stats;
	const u8 *da = NULL;

	/* Use cached pointer with offset for TID */
	tid_rx_stats = &tid_stats_ring[tid];

	current_ts = (u32)ktime_to_ms(ktime_get_real());

	reap_delay = current_ts - (u32)ktime_to_ms(msdu->tstamp);
	ath12k_dp_update_hist_stats(&tid_rx_stats->to_stack_delay, reap_delay);

	if (dp_pdev->prev_rx_timestamp) {
		intfrm_delay = current_ts - dp_pdev->prev_rx_timestamp;
		ath12k_dp_update_hist_stats(&tid_rx_stats->intfrm_delay, intfrm_delay);
	}
	dp_pdev->prev_rx_timestamp = current_ts;

	if (da_is_mcbc) {
		stats->mcast_cnt++;
		da = ((struct ethhdr *)msdu->data)->h_dest;
		if (da) {
			if (is_broadcast_ether_addr(da))
				stats->bcast_cnt++;
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_update_vow_delay_stats);

static void
ath12k_dp_rx_update_peer_stats(struct ath12k_pdev_dp *pdev,
			       struct ath12k_dp_peer *peer,
			       struct link_peer_rx_tid_stats *stats,
			       int ring_id, u8 hw_link_id,
			       u8 active_tid_mask)
{
	int i;
	struct ath12k_dp_peer_stats *pstats = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_dp_peer_rx_stats *rx = NULL;
	struct ath12k_dp_peer_rx_dbg_stats *rx_dbg = NULL;
	bool skip = ath12k_dp_hw_peer_stats_enabled(pdev);

	hw_link_id = ath12k_dp_validate_hw_link_id(hw_link_id);
	pstats = &peer->stats[hw_link_id];
	rx = &pstats->rx[ring_id];
	rx_dbg = &pstats->rx_dbg;

	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, hw_link_id);
	if (link_peer)
		WRITE_ONCE(link_peer->peer_stats.last_rx, jiffies);

	for (i = 0; i < MAX_TP_TIDS; i++) {
		if (!(active_tid_mask & (1 << i))) {
			stats++;
			continue;
		}

		if (!skip) {
			rx->recv_from_reo.packets += stats->received_frm_reo_cnt;
			rx->recv_from_reo.bytes += stats->received_frm_reo_bytes;
		}

		/* ideally we should have both ucast and mcast pkts sent to stack
		 * stats rather than just one sent_to_stack_fast stats
		 */
		rx->sent_to_stack_fast.packets += stats->sent_to_stack_ucast_fast +
						stats->sent_to_stack_mcast_fast;

		rx->sent_to_stack_fast.bytes += stats->sent_to_stack_ucast_fast_bytes +
						stats->sent_to_stack_mcast_fast_bytes;

		rx->sent_to_stack_ucast_fast.packets += stats->sent_to_stack_ucast_fast;
		rx->sent_to_stack_ucast_fast.bytes += stats->sent_to_stack_ucast_fast_bytes;
		rx->sent_to_stack_mcast_fast.packets += stats->sent_to_stack_mcast_fast;
		rx->sent_to_stack_mcast_fast.bytes += stats->sent_to_stack_mcast_fast_bytes;

		rx_dbg->msdu_part_of_amsdu += stats->amsdu;
		rx_dbg->non_amsdu += stats->non_amsdu;
		rx_dbg->mpdu_retry += stats->mpdu_retry;

		if (stats->sent_to_stack_ucast) {
			if (!skip) {
				rx_dbg->ucast.packets += stats->sent_to_stack_ucast;
				rx_dbg->ucast.bytes += stats->sent_to_stack_ucast_bytes;
			}

			/* ideally we should have both ucast and mcast pkts sent to stack
			 * stats rather than just one sent_to_stack stats.
			 */
			rx->sent_to_stack.packets += stats->sent_to_stack_ucast;
			rx->sent_to_stack.bytes +=  stats->sent_to_stack_ucast_bytes;
		}

		if (stats->sent_to_stack_mcast) {
			rx_dbg->mcast.packets += stats->sent_to_stack_mcast;
			rx_dbg->mcast.bytes += stats->sent_to_stack_mcast_bytes;

			/* ideally we should have both ucast and mcast pkts sent to stack
			 * stats rather than just one sent_to_stack stats.
			 */
			rx->sent_to_stack.packets += stats->sent_to_stack_mcast;
			rx->sent_to_stack.bytes +=  stats->sent_to_stack_mcast_bytes;
		}

		if (stats->sg_cnt) {
			rx_dbg->sg.packets += stats->sg_cnt;
			rx_dbg->sg.bytes += stats->sg_bytes;
		}
		stats++;
	}
}

static void
ath12k_dp_rx_update_vif_stats(struct ath12k_pdev_dp *pdev,
			      struct ath12k_dp_peer *peer,
			      struct link_peer_rx_tid_stats *stats,
			      int ring_id, u8 hw_link_id,
			      u8 active_tid_mask)
{
	int i;
	struct pcpu_netdev_tid_stats *tstats;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;

	if (ath12k_dp_stats_enabled(pdev) && ath12k_tid_stats_enabled(pdev)) {
		vif = ath12k_dp_peer_get_vif(peer);
		ahvif = ath12k_vif_to_ahvif(vif);

		tstats = this_cpu_ptr(ahvif->tstats);
		u64_stats_update_begin(&tstats->syncp);
		for (i = 0; i < MAX_TP_TIDS; i++) {
			if (!(active_tid_mask & (1 << i))) {
				stats++;
				continue;
			}

			tstats->tid_stats[i].rx_pkt_stats[ATH_RX_REO_PKTS] +=
						stats->received_frm_reo_cnt;
			tstats->tid_stats[i].rx_pkt_bytes[ATH_RX_REO_PKTS] +=
						stats->received_frm_reo_bytes;

			if (peer->rx_decap_type == DP_RX_DECAP_TYPE_ETHERNET2_DIX) {
				tstats->tid_stats[i].rx_pkt_stats[ATH_RX_ETH_PKTS] +=
					stats->sent_to_stack_ucast_fast +
					stats->sent_to_stack_mcast_fast +
					stats->sent_to_stack_ucast +
					stats->sent_to_stack_mcast;
				tstats->tid_stats[i].rx_pkt_bytes[ATH_RX_ETH_PKTS] +=
					stats->sent_to_stack_ucast_fast_bytes +
					stats->sent_to_stack_mcast_fast_bytes +
					stats->sent_to_stack_ucast_bytes +
					stats->sent_to_stack_mcast_bytes;
			} else if (peer->rx_decap_type == DP_RX_DECAP_TYPE_NATIVE_WIFI) {
				tstats->tid_stats[i].rx_pkt_stats[ATH_RX_NATIVE_WIFI_PKTS] +=
					stats->sent_to_stack_ucast +
					stats->sent_to_stack_mcast;
				tstats->tid_stats[i].rx_pkt_bytes[ATH_RX_NATIVE_WIFI_PKTS] +=
					stats->sent_to_stack_ucast_bytes +
					stats->sent_to_stack_mcast_bytes;
			} else {
				tstats->tid_stats[i].rx_pkt_stats[ATH_RX_RAW_PKTS] +=
					stats->sent_to_stack_ucast +
					stats->sent_to_stack_mcast;
				tstats->tid_stats[i].rx_pkt_bytes[ATH_RX_RAW_PKTS] +=
					stats->sent_to_stack_ucast_bytes +
					stats->sent_to_stack_mcast_bytes;
			}
			tstats->tid_stats[i].rx_pkt_stats[ATH_RX_TOTAL_OUT_PKTS] +=
						stats->sent_to_stack_ucast +
						stats->sent_to_stack_mcast;
			tstats->tid_stats[i].rx_pkt_bytes[ATH_RX_TOTAL_OUT_PKTS] +=
						stats->sent_to_stack_ucast_bytes +
						stats->sent_to_stack_mcast_bytes;

			stats++;
		}
		u64_stats_update_end(&tstats->syncp);
	}
}

static void
ath12k_dp_rx_update_wmm_stats(struct ath12k_pdev_dp *pdev,
			      struct ath12k_dp_peer *peer,
			      struct link_peer_rx_tid_stats *stats,
			      int ring_id, u8 hw_link_id,
			      u8 active_tid_mask)
{
	int i;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	enum wme_ac ac;

	vif = ath12k_dp_peer_get_vif(peer);
	ahvif = ath12k_vif_to_ahvif(vif);

	/* update of wmm stats happens at both dp_pdev and athvif level */
	for (i = 0; i < MAX_TP_TIDS; i++) {
		if (!(active_tid_mask & (1 << i))) {
			stats++;
			continue;
		}

		ac = ath12k_tid_to_ac(i > ATH12K_DSCP_PRIORITY ? 0 : i);

		pdev->wmm_stats.total_wmm_rx_pkts[ac]++;
		ahvif->wmm_stats.total_wmm_rx_pkts[ac]++;

		stats++;
	}
}

static void
ath12k_dp_rx_update_vow_stats(struct ath12k_pdev_dp *pdev,
			      struct link_peer_rx_tid_stats *stats,
			      int ring_id, u8 active_tid_mask)
{
	struct ath12k_tid_rx_stats *tid_rx_stats;
	int i;

	if (!ath12k_dp_stats_enabled(pdev) ||
	    !ath12k_dp_vow_stats_enabled(pdev))
		return;

	for (i = 0; i < MAX_TP_TIDS; i++, stats++) {
		if (!(active_tid_mask & (1 << i)))
			continue;

		tid_rx_stats = &pdev->tid_stats.tid_rx[ring_id][i];

		if (!ath12k_dp_hw_peer_stats_enabled(pdev))
			tid_rx_stats->msdu_cnt   += stats->received_frm_reo_cnt;

		tid_rx_stats->mcast_msdu_cnt     += stats->mcast_cnt;
		tid_rx_stats->bcast_msdu_cnt     += stats->bcast_cnt;
		tid_rx_stats->delivered_to_stack += stats->sent_to_stack_ucast +
					   stats->sent_to_stack_mcast +
					   stats->sent_to_stack_ucast_fast +
					   stats->sent_to_stack_mcast_fast;
	}
}

void
ath12k_dp_rx_update_stats(struct ath12k_pdev_dp *pdev,
			  struct ath12k_dp_peer *peer,
			  struct link_peer_rx_tid_stats *stats,
			  int ring_id, u8 hw_link_id,
			  u8 active_tid_mask)
{
	ath12k_dp_rx_update_peer_stats(pdev, peer, stats, ring_id, hw_link_id,
				       active_tid_mask);

	ath12k_dp_rx_update_vif_stats(pdev, peer, stats, ring_id, hw_link_id,
				      active_tid_mask);

	ath12k_dp_rx_update_wmm_stats(pdev, peer, stats, ring_id, hw_link_id,
				      active_tid_mask);

	ath12k_dp_rx_update_vow_stats(pdev, stats, ring_id, active_tid_mask);
}
EXPORT_SYMBOL(ath12k_dp_rx_update_stats);

bool ath12k_dp_rx_h_mec_drop(struct ath12k_pdev_dp *dp_pdev,
			     struct ath12k_vif *ahvif,
			     int link_id, int peer_id)

{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;

	struct ath12k_link_vif *arvif = rcu_dereference(ahvif->link[link_id]);

	if (!arvif)
		return true;

	if (ahvif->vif && ahvif->vif->type != NL80211_IFTYPE_STATION) {
		ath12k_warn(ab, "vif type is not station for peer with peer_id %u\n",
			    peer_id);
		goto drop;
	}

	arvif->link_stats.rx_dropped++;
drop:
	return true;
}
EXPORT_SYMBOL(ath12k_dp_rx_h_mec_drop);

u32 ath12k_fill_reo_drop_reason(enum hal_reo_dest_ring_error_code err_code)
{
	switch (err_code) {
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO:
		return ATH_RX_DESC_ADDR_ZERO;
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_INVALID:
		return ATH_RX_DESC_INVALID;
	case HAL_REO_DEST_RING_ERROR_CODE_AMPDU_IN_NON_BA:
		return ATH_RX_NON_BA;
	case HAL_REO_DEST_RING_ERROR_CODE_NON_BA_DUPLICATE:
		return ATH_RX_NON_BA_DUP;
	case HAL_REO_DEST_RING_ERROR_CODE_BA_DUPLICATE:
		return ATH_RX_BA_DUP;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP:
	case HAL_REO_DEST_RING_ERROR_CODE_BAR_2K_JUMP:
		return ATH_RX_2K_JUMP;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR:
	case HAL_REO_DEST_RING_ERROR_CODE_BAR_OOR:
		return ATH_RX_ERR_OOR;
	case  HAL_REO_DEST_RING_ERROR_CODE_NO_BA_SESSION:
		return ATH_RX_NO_BA;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN:
		return ATH_RX_EQUALS_SSN;
	case HAL_REO_DEST_RING_ERROR_CODE_2K_ERR_FLAG_SET:
	case HAL_REO_DEST_RING_ERROR_CODE_PN_ERR_FLAG_SET:
		return ATH_RX_ERR_FLAG_SET;
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_BLOCKED:
		return ATH_RX_DESC_BLOCKED;
	case HAL_REO_DEST_RING_ERROR_CODE_PN_CHECK_FAILED:
		return ATH_RX_PN_FAIL;
	default:
		return ATH_RX_ERR_UNKNOWN;
	}
}
EXPORT_SYMBOL(ath12k_fill_reo_drop_reason);

bool ath12k_dp_rx_check_nwifi_hdr_len_valid(struct ath12k_dp *dp,
					    u8 decap_type,
					    struct sk_buff *msdu)
{
	struct ieee80211_hdr *hdr;
	u32 hdr_len;

	if (decap_type != DP_RX_DECAP_TYPE_NATIVE_WIFI)
		return true;

	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);

	if ((likely(hdr_len <= DP_MAX_NWIFI_HDR_LEN)))
		return true;

	dp->device_stats.invalid_rbm++;

	if (ath12k_rx_nwifi_err_dump)
		WARN_ON_ONCE(1);

	return false;
}
EXPORT_SYMBOL(ath12k_dp_rx_check_nwifi_hdr_len_valid);

int ath12k_dp_get_rx_frame_type(u8 rx_decap_type)
{
	u32 pkt_reason = 0;

	switch (rx_decap_type) {
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		pkt_reason = ATH_RX_NATIVE_WIFI_PKTS;
		break;
	case DP_RX_DECAP_TYPE_RAW:
		pkt_reason = ATH_RX_RAW_PKTS;
		break;
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		pkt_reason = ATH_RX_ETH_PKTS;
		break;
	case DP_RX_DECAP_TYPE_8023:
		pkt_reason = ATH_RX_8023_PKTS;
		break;
	}

	return pkt_reason;
}
EXPORT_SYMBOL(ath12k_dp_get_rx_frame_type);

void ath12k_dp_tid_wbm_err_stats(struct ath12k_pdev_dp *dp_pdev,
				 u8 tid,
				 bool is_reo,
				 u32 error_code)
{
	tid = ath12k_vow_tid_validate(tid);

	if (is_reo) {
		if (error_code < HAL_REO_DEST_RING_ERROR_CODE_MAX)
			dp_pdev->tid_stats.tid_reo_err[tid].reo_code[error_code]++;
		else
			dp_pdev->tid_stats.tid_reo_err[tid].reo_code_inv++;
	} else {
		if (error_code < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX)
			dp_pdev->tid_stats.tid_rxdma_err[tid].rxdma_code[error_code]++;
		else
			dp_pdev->tid_stats.tid_rxdma_err[tid].rxdma_code_inv++;
	}
}
EXPORT_SYMBOL(ath12k_dp_tid_wbm_err_stats);
