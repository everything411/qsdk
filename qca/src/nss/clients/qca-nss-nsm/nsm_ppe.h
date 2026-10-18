/*
 **************************************************************************
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

#ifndef __NSM_PPE_H
#define __NSM_PPE_H

#include <linux/ktime.h>
#include <linux/if_ether.h>
#include <ppe_drv.h>
#include "ppe_drv_v4.h"
#include "ppe_drv_v6.h"

struct nsm_ppe_stat {
	uint64_t packets;	/* Number of Packets */
	uint64_t bytes;		/* Total Number of Bytes */
};

typedef struct nsm_ppe_stat nsm_ppe_service_stat_t;
typedef struct nsm_ppe_stat nsm_ppe_flow_stat_t;

struct nsm_ppe_queue_drop_stat {
	uint64_t queue_packets;		/* Number of Packets */
	uint64_t queue_bytes;		/* Total Number of Bytes */
};

struct nsm_ppe_drop_stat {
	uint64_t total_packets;		/* Total Number of Packets */
	uint64_t total_bytes;		/* Total Number of Bytes */
};

typedef struct nsm_ppe_queue_drop_stat nsm_ppe_queue_drop_stat_t;
typedef struct nsm_ppe_drop_stat nsm_ppe_drop_stat_t;

int nsm_ppe_get_v4_flow_stats(nsm_ppe_flow_stat_t *stats, struct ppe_drv_v4_5tuple *tuple);
int nsm_ppe_get_v6_flow_stats(nsm_ppe_flow_stat_t *stats, struct ppe_drv_v6_5tuple *tuple);
int nsm_ppe_get_service_stats(nsm_ppe_service_stat_t *stats, uint8_t service_id);
int nsm_ppe_get_drop_stat(nsm_ppe_drop_stat_t *stats, uint8_t service_id);
int nsm_ppe_get_queue_drop_stat(nsm_ppe_queue_drop_stat_t *stats, uint32_t queue_index, uint32_t drop_queue_id);
#endif
