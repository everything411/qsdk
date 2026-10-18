/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_EVENT_H
#define ATH12K_EVENT_H

#include <linux/types.h>
#include <linux/llist.h>
#include <linux/workqueue.h>
#include <net/cfg80211.h>

struct ath12k_event_queue;
struct ath12k_event;

typedef void (*ath12k_event_cb_t)(struct ath12k_event_queue *queue,
				  struct ath12k_event *event);

/**
 * struct ath12k_event - Base event structure
 * @node: Lock-free list node for event queue
 * @callback: Function to call when event is processed
 * @flags: Atomic flags indicating which events are pending
 * @link_id: Link ID for MLO required for arvif lookup
 * @hw_link_id: for AR from AH
 *
 * Base structure for all events. Specific event types should
 * embed this structure and extend with additional fields.
 * The flags field allows multiple event types to be coalesced
 * into a single queue entry.
 */
struct ath12k_event {
	struct llist_node node;
	ath12k_event_cb_t callback;
	atomic_t flags;
	u8 link_id;
	u8 hw_link_id;
};

/**
 * struct ath12k_event_queue - Per-VIF event queue
 * @head: Lock-free list head for pending events
 * @work: Wiphy work for processing events
 * @wiphy: Wiphy pointer for work queue scheduling
 * @priv: Private data (typically ath12k_vif pointer)
 *
 * Each VIF maintains its own event queue for processing
 * peer-related events in a serialized manner with mac80211.
 */
struct ath12k_event_queue {
	struct llist_head head;
	struct wiphy_work work;
	struct wiphy *wiphy;
	void *priv;
};

void ath12k_event_queue_init(struct ath12k_event_queue *queue,
			     struct wiphy *wiphy, void *priv);
void ath12k_event_queue_deinit(struct ath12k_event_queue *queue);
void ath12k_event_enqueue(struct ath12k_event_queue *queue,
			  struct ath12k_event *event);
void ath12k_event_queue_flush(struct ath12k_event_queue *queue);

#endif /* ATH12K_EVENT_H */
