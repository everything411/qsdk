/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/skbuff.h>
#include <net/mac80211.h>
#include "../ieee80211_i.h"
#include "cmn_extn.h"

/*
 * Extended monitor RX notifier chain.
 *
 * Uses SRCU (Sleepable RCU) notifier chain because:
 *   - Notifications are dispatched from rxmon workqueue (process) context,
 *     so callbacks are allowed to sleep.
 *   - SRCU read-side critical sections are preemptible, making it
 *     suitable for potentially long-running packet processing callbacks.
 *   - Provides RCU-like protection without the atomic constraint of
 *     the PPDU atomic notifier chain above.
 *
 * Ownership semantics:
 *   The driver always frees the MPDU SKB after srcu_notifier_call_chain()
 *   returns. Listeners that need to retain the frame beyond the callback
 *   MUST call skb_clone() inside the callback and take ownership of the
 *   clone; they must NOT free or hold a reference to the original SKB.
 */
SRCU_NOTIFIER_HEAD_STATIC(ieee80211_ext_mon_rx_notifier_chain);

/* Listener count for fast has-listeners check in the RX hot path */
static atomic_t ieee80211_ext_mon_rx_listener_count = ATOMIC_INIT(0);

/**
 * ieee80211_register_ext_mon_rx_notifier_extn - Register extended monitor RX listener
 * @nb: Notifier block with callback function
 *
 * Registers a listener for ext_mon_event_type events. Multiple
 * listeners may be registered simultaneously. Each callback is invoked
 * from the rxmon workqueue (process) context and may sleep.
 *
 * The driver frees the MPDU SKB after the chain returns. Listeners that
 * need to retain the frame must call skb_clone() and free the clone
 * after use.
 *
 * Return: 0 on success, negative errno on failure
 */
int ieee80211_register_ext_mon_rx_notifier_extn(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;

	pr_debug("ext_mon RX notifier register\n");

	ret = srcu_notifier_chain_register(&ieee80211_ext_mon_rx_notifier_chain, nb);
	if (ret)
		return ret;

	atomic_inc(&ieee80211_ext_mon_rx_listener_count);
	return 0;
}
EXPORT_SYMBOL(ieee80211_register_ext_mon_rx_notifier_extn);

/**
 * ieee80211_unregister_ext_mon_rx_notifier_extn - Unregister extended monitor RX listener
 * @nb: Notifier block to remove
 *
 * Return: 0 on success, negative errno on failure
 */
int ieee80211_unregister_ext_mon_rx_notifier_extn(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;

	pr_debug("ext_mon RX notifier unregister\n");

	ret = srcu_notifier_chain_unregister(&ieee80211_ext_mon_rx_notifier_chain, nb);
	if (!ret)
		atomic_dec(&ieee80211_ext_mon_rx_listener_count);

	return ret;
}
EXPORT_SYMBOL(ieee80211_unregister_ext_mon_rx_notifier_extn);

/**
 * ieee80211_ext_mon_rx_notifier_has_listeners_extn - Check for active RX ext_mon listeners
 *
 * Used by the RX extended monitor data path to decide whether to dispatch
 * the MPDU via the RX notifier chain or fall back to mac80211 delivery.
 *
 * Return: true if at least one RX listener is registered, false otherwise
 */
bool ieee80211_ext_mon_rx_notifier_has_listeners_extn(void)
{
	return atomic_read(&ieee80211_ext_mon_rx_listener_count) > 0;
}
EXPORT_SYMBOL(ieee80211_ext_mon_rx_notifier_has_listeners_extn);

/**
 * ieee80211_ext_mon_rx_notifier_call_extn - Dispatch extended monitor RX event
 * @type: Event type (enum ieee80211_ext_mon_event_type_extn)
 * @event: Event payload (struct ieee80211_ext_mon_rx_event_extn *)
 *
 * Invokes all registered RX listeners via the SRCU notifier chain.
 *
 * Return: NOTIFY_DONE / NOTIFY_OK / NOTIFY_STOP_MASK
 */
int
ieee80211_ext_mon_rx_notifier_call_extn(enum ieee80211_ext_mon_event_type_extn type,
					struct ieee80211_ext_mon_rx_event_extn *event)
{
	return srcu_notifier_call_chain(&ieee80211_ext_mon_rx_notifier_chain,
					type, event);
}
EXPORT_SYMBOL(ieee80211_ext_mon_rx_notifier_call_extn);

/*
 * Extended monitor TX notifier chain.
 *
 * Uses SRCU (Sleepable RCU) notifier chain because:
 *   - Notifications are dispatched from txmon workqueue (process) context,
 *     so callbacks are allowed to sleep.
 *   - SRCU read-side critical sections are preemptible, making it
 *     suitable for potentially long-running packet processing callbacks.
 *
 * Ownership semantics:
 *   The driver always frees the MPDU SKB after srcu_notifier_call_chain()
 *   returns. Listeners that need to retain the frame beyond the callback
 *   MUST call skb_clone() inside the callback and take ownership of the
 *   clone; they must NOT free or hold a reference to the original SKB.
 */
SRCU_NOTIFIER_HEAD_STATIC(ieee80211_ext_mon_tx_notifier_chain);

/* Listener count for fast has-listeners check in the TX hot path */
static atomic_t ieee80211_ext_mon_tx_listener_count = ATOMIC_INIT(0);
/**
 * ieee80211_register_ext_mon_tx_notifier_extn - Register extended monitor TX listener
 * @nb: Notifier block with callback function
 *
 * Registers a listener for ext_mon TX events. Multiple listeners may be
 * registered simultaneously. Each callback is invoked from the txmon
 * workqueue (process) context and may sleep.
 *
 * The driver frees the MPDU SKB after the chain returns. Listeners that
 * need to retain the frame must call skb_clone() and free the clone
 * after use.
 *
 * Return: 0 on success, negative errno on failure
 */
int ieee80211_register_ext_mon_tx_notifier_extn(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;

	pr_debug("ext_mon TX notifier register\n");

	ret = srcu_notifier_chain_register(&ieee80211_ext_mon_tx_notifier_chain, nb);
	if (ret)
		return ret;

	atomic_inc(&ieee80211_ext_mon_tx_listener_count);
	return 0;
}
EXPORT_SYMBOL(ieee80211_register_ext_mon_tx_notifier_extn);

/**
 * ieee80211_unregister_ext_mon_tx_notifier_extn - Unregister extended monitor TX listener
 * @nb: Notifier block to remove
 *
 * Return: 0 on success, negative errno on failure
 */
int ieee80211_unregister_ext_mon_tx_notifier_extn(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;

	pr_debug("ext_mon TX notifier unregister\n");

	ret = srcu_notifier_chain_unregister(&ieee80211_ext_mon_tx_notifier_chain, nb);
	if (!ret) {
		/*
		 * Decrement only if positive so an unbalanced or double
		 * unregister cannot drive the listener count negative and
		 * corrupt the has-listeners fast path.
		 */
		if (atomic_dec_if_positive(&ieee80211_ext_mon_tx_listener_count) < 0)
			pr_warn("ext_mon TX notifier listener count already zero on unregister\n");
	}

	return ret;
}
EXPORT_SYMBOL(ieee80211_unregister_ext_mon_tx_notifier_extn);

/**
 * ieee80211_ext_mon_tx_notifier_has_listeners_extn - Check for active TX ext_mon listeners
 *
 * Used by the TX extended monitor data path to decide whether to dispatch
 * the MPDU via the TX notifier chain.
 *
 * Return: true if at least one TX listener is registered, false otherwise
 */
bool ieee80211_ext_mon_tx_notifier_has_listeners_extn(void)
{
	return atomic_read(&ieee80211_ext_mon_tx_listener_count) > 0;
}
EXPORT_SYMBOL(ieee80211_ext_mon_tx_notifier_has_listeners_extn);
/**
 * ieee80211_ext_mon_tx_notify - Dispatch extended monitor TX event
 * @hw: Hardware instance the frame was transmitted on
 * @sdata: Monitor sub-interface the frame is destined for
 * @mpdu: The MPDU SKB to dispatch
 *
 * Builds the TX event and dispatches it via the SRCU chain. Skipped
 * (NOTIFY_DONE) if @sdata is NULL, ext mon is disabled, the event type is not
 * IEEE80211_EXT_MON_POST_RTAP, or no listeners are registered.
 *
 * Return: NOTIFY_DONE / NOTIFY_OK / NOTIFY_STOP_MASK
 */
int
ieee80211_ext_mon_tx_notify(struct ieee80211_hw *hw,
			    struct ieee80211_sub_if_data *sdata,
			    struct sk_buff *mpdu)
{
	struct ieee80211_ext_mon_tx_event_extn event;

	if (!sdata)
		return NOTIFY_DONE;

	if (!(sdata->flags & IEEE80211_SDATA_EXT_TX_MONITOR_ENABLED))
		return NOTIFY_DONE;

	if (sdata->extn_tx_mon_evt_typ != IEEE80211_EXT_MON_POST_RTAP)
		return NOTIFY_DONE;

	if (!ieee80211_ext_mon_tx_notifier_has_listeners_extn())
		return NOTIFY_DONE;

	event.hw = hw;
	event.sdata = sdata;
	event.mpdu = mpdu;

	return ieee80211_ext_mon_tx_notifier_call_extn_nc(sdata->extn_tx_mon_evt_typ,
							  &event);
}
EXPORT_SYMBOL(ieee80211_ext_mon_tx_notify);

/**
 * ieee80211_ext_mon_tx_notifier_call_extn_nc - Dispatch TX event with no checks
 * @type: Event type (enum ieee80211_ext_mon_event_type_extn)
 * @event: Event payload (struct ieee80211_ext_mon_tx_event_extn *)
 *
 * Dispatches via the SRCU chain with no enable check; caller supplies @type.
 *
 * Return: NOTIFY_DONE / NOTIFY_OK / NOTIFY_STOP_MASK
 */
int
ieee80211_ext_mon_tx_notifier_call_extn_nc(enum ieee80211_ext_mon_event_type_extn type,
					   struct ieee80211_ext_mon_tx_event_extn *event)
{
	return srcu_notifier_call_chain(&ieee80211_ext_mon_tx_notifier_chain,
					type, event);
}
EXPORT_SYMBOL(ieee80211_ext_mon_tx_notifier_call_extn_nc);
/**
 * ieee80211_set_ext_tx_monitor - Set extended TX monitor state on a VIF
 * @vif: VIF on which to enable or disable extended TX monitor
 * @enable: true to enable, false to disable
 *
 * Sets or clears IEEE80211_SDATA_EXT_TX_MONITOR_ENABLED on the underlying
 * ieee80211_sub_if_data, mirroring what ieee80211_enable_ext_monitor() does
 * for the RX path (IEEE80211_SDATA_EXT_RX_MONITOR_ENABLED).
 */
void ieee80211_set_ext_tx_monitor(struct ieee80211_vif *vif, bool enable)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);

	if (enable)
		sdata->flags |= IEEE80211_SDATA_EXT_TX_MONITOR_ENABLED;
	else
		sdata->flags &= ~IEEE80211_SDATA_EXT_TX_MONITOR_ENABLED;
}
EXPORT_SYMBOL(ieee80211_set_ext_tx_monitor);

/**
 * ieee80211_set_ext_tx_mon_evt_typ - Set extended TX monitor event type on a VIF
 * @vif: VIF to update
 * @type: Event type to record
 *
 * Stores @type in sdata->extn_tx_mon_evt_typ.
 */
void ieee80211_set_ext_tx_mon_evt_typ(struct ieee80211_vif *vif,
				      enum ieee80211_ext_mon_event_type_extn type)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);

	sdata->extn_tx_mon_evt_typ = type;
}
EXPORT_SYMBOL(ieee80211_set_ext_tx_mon_evt_typ);
