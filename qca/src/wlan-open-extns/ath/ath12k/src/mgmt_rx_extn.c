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

/* REO updates HP in multiples of entry size (8) */
#define HAL_SRNG_INT_BATCH_THRESHOLD_RX_HIGHPRI 1
/* REO keeps timers internally in 8 us units */
#define HAL_SRNG_INT_TIMER_THRESHOLD_RX_HIGHPRI 1

int ath12k_pcic_mgmt_interrupt_handler_extn(struct ath12k_mgmt_irq_grp *irq_grp)
{
	if (!irq_grp->irq_grp_extn.priority)
		return -EINVAL;

#if LINUX_VERSION_IS_GEQ(6, 13, 0)
	queue_work(system_bh_highpri_wq, &irq_grp->intr_wq);
#else
	tasklet_hi_schedule(&irq_grp->intr_tq);
#endif

	return 0;
}

int ath12k_mgmt_srng_type_valid_extn(struct ath12k_base *ab, enum hal_ring_type type)
{
	if (type == HAL_REO_DST_HIGH_PRIO)
		return 0;

	return -EINVAL;
}

int ath12k_mgmt_srng_setup_extn(struct ath12k_base *ab, enum hal_ring_type type,
				bool *cached, struct hal_srng_params *params)
{
	if (type != HAL_REO_DST_HIGH_PRIO)
		return -EINVAL;

	*cached = true;

#ifndef CONFIG_IO_COHERENCY
	if (!ab->hw_params->alloc_cacheable_memory)
		*cached = false;
#endif /* !CONFIG_IO_COHERENCY */

	params->intr_batch_cntr_thres_entries = HAL_SRNG_INT_BATCH_THRESHOLD_RX_HIGHPRI;
	params->intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX_HIGHPRI;

	return 0;
}

ssize_t ath12k_debugfs_dump_device_mgmt_srng_stats_extn(struct ath12k_base *ab, char *buf,
							int len, int size)
{
	static const char *frm_stype[ATH12K_SRNG_STATS_EXTN_MGMT_FRM_STYPE_MAX - 1] = {
			"Association request", "Association response",
			"Reassociation request", "Reassociation response",
			"Probe request", "Probe response", "Timing Advertisement",
			"Reserved", "Beacon", "ATIM", "Disassociation", "Authentication",
			"Deauthentication", "Action", "Action NoAck"};
	struct ath12k_device_mgmt_srng_stats_extn *stats_extn;
	int i;

	if (len >= size)
		return 0;

	stats_extn = &ab->mgmt->srng_stats.stats_extn;

	len += scnprintf(buf + len, size - len, "Delivered High-priority packets:\n");
	for (i = 0; i < ATH12K_SRNG_STATS_EXTN_MGMT_FRM_STYPE_MAX-1 && len < size; i++)
		len += scnprintf(buf + len, size - len, "  %s: %u\n",
				 frm_stype[i], stats_extn->rx_highpri_pkts[i]);

	return len;
}
