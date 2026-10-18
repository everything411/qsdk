// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/llist.h>
#include "core.h"
#include "event.h"
#include "debug.h"

/**
 * ath12k_event_work - Work queue handler for processing events
 * @wiphy: Wiphy pointer passed by work queue
 * @work: Work structure
 *
 * This function is called in wiphy context to process all pending
 * events in the queue. It drains the lock-free list and calls each
 * event's callback function.
 */
static void ath12k_event_work(struct wiphy *wiphy, struct wiphy_work *work)
{
	struct ath12k_event_queue *queue = container_of(work,
							struct ath12k_event_queue,
							work);
	struct llist_node *node;
	struct ath12k_event *event, *tmp;

	/* Drain all pending events atomically */
	node = llist_del_all(&queue->head);
	if (!node)
		return;

	/* Process each event in FIFO order (llist_del_all returns reversed) */
	llist_for_each_entry_safe(event, tmp, node, node) {
		if (event->callback)
			event->callback(queue, event);
	}
}

/**
 * ath12k_event_queue_init - Initialize an event queue
 * @queue: Event queue to initialize
 * @wiphy: Wiphy for work queue scheduling
 * @priv: Private data (typically ath12k_vif)
 *
 * Initializes the event queue structure and registers the work handler.
 * Must be called during VIF creation.
 */
void ath12k_event_queue_init(struct ath12k_event_queue *queue,
			     struct wiphy *wiphy, void *priv)
{
	init_llist_head(&queue->head);
	wiphy_work_init(&queue->work, ath12k_event_work);
	queue->wiphy = wiphy;
	queue->priv = priv;
}

/**
 * ath12k_event_queue_deinit - Deinitialize an event queue
 * @queue: Event queue to deinitialize
 *
 * Cancels any pending work and drains remaining events.
 * Must be called during VIF removal to prevent use-after-free.
 */
void ath12k_event_queue_deinit(struct ath12k_event_queue *queue)
{
	struct llist_node *node;

	/* Cancel any pending work */
	wiphy_work_cancel(queue->wiphy, &queue->work);

	/* Drain any remaining events */
	node = llist_del_all(&queue->head);
	/* Events are embedded in peer structures, so no free needed */

	/* Clear pointers to prevent accidental use */
	queue->wiphy = NULL;
	queue->priv = NULL;
}

/**
 * ath12k_event_enqueue - Enqueue an event for processing
 * @queue: Event queue
 * @event: Event to enqueue
 *
 * Adds the event to the lock-free queue and schedules work if needed.
 * Can be called from any context, including data path.
 */
void ath12k_event_enqueue(struct ath12k_event_queue *queue,
			  struct ath12k_event *event)
{
	/* llist_add returns true if list was empty (first event) */
	if (llist_add(&event->node, &queue->head))
		wiphy_work_queue(queue->wiphy, &queue->work);
}

/**
 * ath12k_event_queue_flush - Flush pending events
 * @queue: Event queue to flush
 *
 * Waits for the event worker to finish processing the current queue.
 * This ensures that any events currently in the llist are processed
 * and the callback is executed.
 *
 * IMPORTANT: This uses wiphy_work_flush(), not wiphy_work_cancel().
 * wiphy_work_cancel() would stop the worker and leave the llist nodes
 * stranded (dangling pointers), which leads to crashes.
 * wiphy_work_flush() ensures the llist is fully drained.
 */
void ath12k_event_queue_flush(struct ath12k_event_queue *queue)
{
	/* Check if wiphy is valid to avoid NULL dereference */
	if (!queue->wiphy)
		return;

	/* Use wiphy_work_flush to FORCE execution of ath12k_event_work.
	 * This drains the llist, removing all nodes safely.
	 */
	wiphy_work_flush(queue->wiphy, &queue->work);
}

