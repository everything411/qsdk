// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * PPDU event notifier framework for ath12k.
 *
 * Provides atomic notifier chain for RX/TX PPDU completion events,
 * and an SRCU notifier chain for extended monitor MPDU delivery,
 * allowing external modules to subscribe to data path events without
 * modifying core driver code. Uses per-event reference counting to
 * enable fast listener checks in hot paths.
 */

#include <linux/notifier.h>
#include "ath12k_notif.h"

/*
 * Global notifier chain for ath12k PPDU events.
 * Uses atomic notifier chain because PPDU notifications are
 * triggered from atomic context.
 */
static ATOMIC_NOTIFIER_HEAD(ath12k_ppdu_notifier_chain);
static ATOMIC_NOTIFIER_HEAD(ath12k_fse_update_notifier_chain);

/* Track listener counts per event type for optimization */
static atomic_t ath12k_ppdu_rx_listener_count = ATOMIC_INIT(0);
static atomic_t ath12k_ppdu_tx_listener_count = ATOMIC_INIT(0);
static atomic_t ath12k_fse_update_listener_count = ATOMIC_INIT(0);
/**
 * ath12k_register_ppdu_notifier - Register PPDU event listener
 * @nb: Notifier block with callback
 * @event_mask: Bitmask of events (1 << ATH12K_EVENT_PPDU_RX_COMPLETE)
 *              or (1 << ATH12K_EVENT_PPDU_TX_COMPLETE) or both
 *
 * Registers a listener for PPDU completion events. The callback
 * will be invoked synchronously from atomic context.
 *
 * Return: 0 on success, negative on failure
 */
int ath12k_register_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask)
{
	int ret;

	if (!nb)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "PPDU notifier register, mask=0x%lx\n",
		   event_mask);

	ret = atomic_notifier_chain_register(&ath12k_ppdu_notifier_chain, nb);
	if (ret)
		return ret;

	/* Increment listener counts for requested events */
	if (event_mask & (1 << ATH12K_EVENT_PPDU_RX_COMPLETE))
		atomic_inc(&ath12k_ppdu_rx_listener_count);
	if (event_mask & (1 << ATH12K_EVENT_PPDU_TX_COMPLETE))
		atomic_inc(&ath12k_ppdu_tx_listener_count);

	return 0;
}
EXPORT_SYMBOL(ath12k_register_ppdu_notifier);

/**
 * ath12k_unregister_ppdu_notifier - Unregister PPDU event listener
 * @nb: Notifier block to remove
 * @event_mask: Same bitmask used during registration
 *
 * Return: 0 on success, negative on failure
 */
int ath12k_unregister_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask)
{
	int ret;

	if (!nb)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "PPDU notifier unregister, mask=0x%lx\n",
		   event_mask);
	ret = atomic_notifier_chain_unregister(&ath12k_ppdu_notifier_chain, nb);

	/* Decrement listener counts for registered events */
	if (event_mask & (1 << ATH12K_EVENT_PPDU_RX_COMPLETE))
		atomic_dec(&ath12k_ppdu_rx_listener_count);
	if (event_mask & (1 << ATH12K_EVENT_PPDU_TX_COMPLETE))
		atomic_dec(&ath12k_ppdu_tx_listener_count);

	return ret;
}
EXPORT_SYMBOL(ath12k_unregister_ppdu_notifier);

/**
 * ath12k_ppdu_notifier_has_listeners - Check for active listeners
 * @event_type: Event type to query (ATH12K_EVENT_PPDU_RX_COMPLETE
 *              or ATH12K_EVENT_PPDU_TX_COMPLETE)
 *
 * Used by data path to skip event preparation when no listeners
 * are registered.
 *
 * Return: true if listeners exist, false otherwise
 */
bool ath12k_ppdu_notifier_has_listeners(enum ath12k_event_type event_type)
{
	switch (event_type) {
	case ATH12K_EVENT_PPDU_RX_COMPLETE:
		return atomic_read(&ath12k_ppdu_rx_listener_count) > 0;
	case ATH12K_EVENT_PPDU_TX_COMPLETE:
		return atomic_read(&ath12k_ppdu_tx_listener_count) > 0;
	default:
		return false;
	}
}
EXPORT_SYMBOL(ath12k_ppdu_notifier_has_listeners);

/**
 * ath12k_ppdu_notifier_call_chain - Dispatch PPDU event
 * @val: Event type (ATH12K_EVENT_PPDU_RX_COMPLETE or
 *       ATH12K_EVENT_PPDU_TX_COMPLETE)
 * @v: Event payload (struct ath12k_ppdu_event)
 *
 * Invokes all registered listeners synchronously. The payload
 * is valid only during the callback.
 *
 * Return: NOTIFY_DONE if all listeners processed the event,
 *         NOTIFY_STOP_MASK if any listener requested to stop
 *         the chain
 */
int ath12k_ppdu_notifier_call_chain(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&ath12k_ppdu_notifier_chain, val, v);
}
EXPORT_SYMBOL(ath12k_ppdu_notifier_call_chain);

/* Register: FSE_UPDATE */
int ath12k_register_fse_update_notifier(struct notifier_block *nb,
					 unsigned long event_mask)
{
	int ret;

	if (!nb)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "FSE_UPDATE notifier register, mask=0x%lx\n",
		   event_mask);

	ret = atomic_notifier_chain_register(&ath12k_fse_update_notifier_chain,
					     nb);
	if (ret)
		return ret;

	if (event_mask & (1 << ATH12K_EVENT_FSE_UPDATE))
		atomic_inc(&ath12k_fse_update_listener_count);

	return 0;
}
EXPORT_SYMBOL(ath12k_register_fse_update_notifier);

int ath12k_unregister_fse_update_notifier(struct notifier_block *nb,
					   unsigned long event_mask)
{
	int ret;

	if (!nb)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "FSE_UPDATE notifier unregister, mask=0x%lx\n",
		   event_mask);

	ret = atomic_notifier_chain_unregister(&ath12k_fse_update_notifier_chain,
					       nb);
	if (!ret && (event_mask & (1 << ATH12K_EVENT_FSE_UPDATE)))
		atomic_dec(&ath12k_fse_update_listener_count);

	return ret;
}
EXPORT_SYMBOL(ath12k_unregister_fse_update_notifier);

bool ath12k_fse_update_notif_has_listeners(void)
{
	return atomic_read(&ath12k_fse_update_listener_count) > 0;
}
EXPORT_SYMBOL(ath12k_fse_update_notif_has_listeners);

int ath12k_fse_update_notif_call_chain(void *v)
{
	return atomic_notifier_call_chain(&ath12k_fse_update_notifier_chain,
					  ATH12K_EVENT_FSE_UPDATE, v);
}
EXPORT_SYMBOL(ath12k_fse_update_notif_call_chain);
