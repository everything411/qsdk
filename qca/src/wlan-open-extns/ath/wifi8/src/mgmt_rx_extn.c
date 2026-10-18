// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/of.h>
#include "../mgmt_rx.h"
#include "../../debug.h"
#include "../../core.h"

#define MGMT_REO_DST_HIGH_PRIO_RING_SIZE 128

enum ath12k_mgmt_srng_pkt_type_extn {
	ATH12K_MGMT_SRNG_PKT_TYPE_EXTN_RX_HIGHPRI = ATH12K_MGMT_SRNG_PKT_TYPE_MAX,
	ATH12K_MGMT_SRNG_PKT_TYPE_EXTN_MAX,
};


void ath12k_wifi8_mgmt_rx_process_highpri_extn(struct ath12k_base *ab,
					       struct ath12k_mgmt_irq_grp *irq_grp,
					       struct mgmt_srng *ring)
{
	struct sk_buff_head mmpdu_list;
	int ret;

	__skb_queue_head_init(&mmpdu_list);

	ret = ath12k_wifi8_mgmt_rx_reap_packets(ab, ring, &mmpdu_list);
	if (!ret)
		return;

	ath12k_wifi8_mgmt_rx_process_packets(ab->mgmt, &mmpdu_list,
					     ATH12K_MGMT_SRNG_PKT_TYPE_EXTN_RX_HIGHPRI);
}

void ath12k_wifi8_mgmt_highpri_service_srng_extn(struct ath12k_base *ab,
						 struct ath12k_mgmt_irq_grp *irq_grp)
{
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(ab->mgmt);
	struct mgmt_srng_extn *rx_highpri_ring =
		&mgmt_wifi8->mgmt_wifi8_extn.reo_dst_highpri_rx_ring;

	ath12k_wifi8_mgmt_rx_process_highpri_extn(ab, irq_grp,
						  (struct mgmt_srng *)rx_highpri_ring);
}

#if LINUX_VERSION_IS_GEQ(6, 13, 0)
void ath12k_wifi8_mgmt_highpri_workqueue_extn(struct work_struct *work)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_work(irq_grp, work, intr_wq);
	struct ath12k_base *ab = irq_grp->ab;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		goto exit;

	ath12k_wifi8_mgmt_highpri_service_srng_extn(ab, irq_grp);
exit:
	ath12k_mgmt_irq_grp_enable(irq_grp);
}
#else
void ath12k_wifi8_mgmt_highpri_tasklet_extn(struct tasklet_struct *t)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_tasklet(irq_grp, t, intr_tq);
	struct ath12k_base *ab = irq_grp->ab;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		goto exit;

	ath12k_wifi8_mgmt_highpri_service_srng_extn(ab, irq_grp);
exit:
	ath12k_mgmt_irq_grp_enable(irq_grp);
}
#endif

int ath12k_wifi8_mgmt_rx_ring_setup_extn(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct mgmt_srng_extn *rx_highpri_ring =
		&mgmt_wifi8->mgmt_wifi8_extn.reo_dst_highpri_rx_ring;
	int ret;

	ret = ath12k_mgmt_srng_setup(ab, (struct mgmt_srng *)rx_highpri_ring,
				     HAL_REO_DST_HIGH_PRIO, 0, 0, 1,
				     MGMT_REO_DST_HIGH_PRIO_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt reo_dst: %d", ret);
		return ret;
	}

	return 0;
}

void ath12k_mgmt_srng_hw_disable_extn(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct mgmt_srng_extn *rx_highpri_ring =
		&mgmt_wifi8->mgmt_wifi8_extn.reo_dst_highpri_rx_ring;

	ath12k_mgmt_srng_hw_disable(ab, (struct mgmt_srng *)rx_highpri_ring);
}

void ath12k_wifi8_mgmt_rx_ring_free_extn(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct mgmt_srng_extn *rx_highpri_ring =
		&mgmt_wifi8->mgmt_wifi8_extn.reo_dst_highpri_rx_ring;

	ath12k_mgmt_srng_cleanup(ab, (struct mgmt_srng *)rx_highpri_ring);
}

int ath12k_wifi8_mgmt_rx_deliver_mmpdu_extn(struct ath12k_mgmt *mgmt,
					    struct ath12k *partner_ar,
					    struct sk_buff *mmpdu, u32 pkt_type)
{
	struct ath12k_device_mgmt_srng_stats_extn *stats_extn;
	struct ath12k_mgmt *partner_mgmt;
	struct ieee80211_hdr *hdr;
	u16 frm_stype, fc;

	if (pkt_type != ATH12K_MGMT_SRNG_PKT_TYPE_EXTN_RX_HIGHPRI)
		return -EINVAL;

	if (mmpdu->len < sizeof(struct ieee80211_hdr))
		return -EINVAL;

	hdr = (struct ieee80211_hdr *)mmpdu->data;
	fc = le16_to_cpu(hdr->frame_control);
	frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, fc);

	partner_mgmt = partner_ar->ab->mgmt ? partner_ar->ab->mgmt : mgmt;

	stats_extn = &partner_mgmt->srng_stats.stats_extn;
	if (unlikely(frm_stype >= ARRAY_SIZE(stats_extn->rx_highpri_pkts)))
		return -EINVAL;

	stats_extn->rx_highpri_pkts[frm_stype]++;

	return 0;
}

int ath12k_wifi8_mgmt_dump_ring_stats_extn(struct ath12k_mgmt *mgmt, char *buf, int size,
					   int len)
{
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct mgmt_srng_extn *rx_highpri_ring =
		&mgmt_wifi8->mgmt_wifi8_extn.reo_dst_highpri_rx_ring;
	u32 ring_id = rx_highpri_ring->ring_id;
	struct ath12k_base *ab = mgmt->ab;

	if (len < size)
		len += ath12k_hal_dump_ring_stats(ab, HAL_REO_DST_HIGH_PRIO, ring_id,
						  buf + len, size - len);

	return len;
}

int ath12k_wifi8_mgmt_init_extn(struct ath12k_mgmt *mgmt)
{
	struct ath12k_mgmt_irq_grp *irq_grp;
	size_t irq_grp_size;

	if (!mgmt || !mgmt->irq_grp)
		return -EINVAL;

	if ((mgmt->num_irq_grp + 1) > ATH12K_MGMT_IRQ_GRP_NUM_MAX) {
		ath12k_err(mgmt->ab, "MGMT num irq more than supported (%u > %u)",
			   mgmt->num_irq_grp + 1, ATH12K_MGMT_IRQ_GRP_NUM_MAX);
		return -EINVAL;
	}

	irq_grp_size = (mgmt->num_irq_grp + 1) * sizeof(*irq_grp);
	irq_grp = krealloc(mgmt->irq_grp, irq_grp_size, GFP_KERNEL);
	if (!irq_grp)
		return -EINVAL;

	*(irq_grp + mgmt->num_irq_grp) = (struct ath12k_mgmt_irq_grp){
		.grp_id = 1,
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		.irq_grp_handler = ath12k_wifi8_mgmt_highpri_workqueue_extn,
#else
		.irq_grp_handler = ath12k_wifi8_mgmt_highpri_tasklet_extn,
#endif
		.num_irq = 1,
		.irq_grp_extn.priority = true,
	};

	mgmt->irq_grp = irq_grp;
	mgmt->num_irq_grp++;

	return 0;
}

void ath12k_wifi8_mgmt_deinit_extn(struct ath12k_mgmt *mgmt)
{
	kfree(mgmt->irq_grp);
	mgmt->irq_grp = NULL;
}
