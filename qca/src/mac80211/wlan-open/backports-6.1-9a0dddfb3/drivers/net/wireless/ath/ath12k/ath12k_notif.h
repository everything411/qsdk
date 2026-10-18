/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _ATH12K_NOTIF_H_
#define _ATH12K_NOTIF_H_

#include <linux/skbuff.h>
#include <linux/notifier.h>
#include "hal_mon_cmn.h"
#include "dp_htt.h"
#include "debug.h"

/**
 * enum ath12k_event_type - WDI event types
 * @ATH12K_EVENT_PPDU_RX_COMPLETE: RX PPDU completion. Notifies the PPDU to
 * listeners even if peer id is invalid when CFR RCC filter is enabled. Listers
 * to filter out the PPDU if not intended to process PPDU for invalid peer.
 * @ATH12K_EVENT_PPDU_TX_COMPLETE: TX PPDU completion
 * @ATH12K_EVENT_FSE_UPDATE: Flow/Search/Steering Engine update event
 */
enum ath12k_event_type {
	ATH12K_EVENT_PPDU_RX_COMPLETE = 1,
	ATH12K_EVENT_PPDU_TX_COMPLETE,
	ATH12K_EVENT_FSE_UPDATE,
};

/**
 * struct ath12k_ppdu_rx_info - RX PPDU event payload
 * @ppdu_info: HAL RX monitor PPDU information
 *
 * Embedded in SKB data buffer passed to notifier callbacks.
 */
struct ath12k_ppdu_rx_info {
	struct hal_rx_mon_ppdu_info ppdu_info;
	/* DRIVER-ADDED CONTEXT FIELDS */
} __packed;

/**
 * struct ath12k_ppdu_tx_info - TX PPDU event payload
 * @ppdu_info: HTT TX PPDU statistics
 * @device_id: Hardware device index (chip ID) in a multi-device (MLO) system.
 *             Used by listeners to resolve the correct ath12k_base when
 *             looking up peers across multiple chips.
 *
 * Embedded in SKB data buffer passed to notifier callbacks.
 */
struct ath12k_ppdu_tx_info {
	struct htt_ppdu_stats_info ppdu_info;
	u8 device_id;
} __packed;

/**
 * struct ath12k_ppdu_event - PPDU notifier event wrapper
 * @skb: SKB containing PPDU info (ath12k_ppdu_rx_info or
 *       ath12k_ppdu_tx_info depending on event type)
 *
 * Listeners must not hold references to @skb or its data beyond
 * the callback context. The SKB is owned by the driver.
 */
struct ath12k_ppdu_event {
	struct sk_buff *skb;      /* SKB containing PPDU info */
};

/**
 * enum ath12k_fse_op - FSE update operation codes
 * @ATH12K_FSE_OP_ADD: Add flow entry
 * @ATH12K_FSE_OP_DELETE: Delete flow entry
 */
enum ath12k_fse_op {
	ATH12K_FSE_OP_ADD = 0,
	ATH12K_FSE_OP_DELETE = 1,
};

/**
 * struct ath12k_fse_update_event - Payload for ATH12K_EVENT_FSE_UPDATE
 * @src_ip:   Source IP address (4 x u32 in host order;
 *            IPv4 uses [0] only, IPv6 uses all four words)
 * @src_port: Source L4 port number (host order)
 * @dest_ip:  Destination IP address (same layout as @src_ip, host order)
 * @dest_port: Destination L4 port number (host order)
 * @protocol: IP protocol number (e.g. 6 = TCP, 17 = UDP)
 * @version:  IP version (4 or 6)
 * @op:       Operation: %ATH12K_FSE_OP_ADD or %ATH12K_FSE_OP_DELETE
 *
 * Passed as the @v argument to ath12k_fse_update_notifier_call_chain().
 * The struct is stack-allocated by the driver and is valid only during
 * the callback. Listeners must not hold a reference to it beyond the
 * callback context.
 *
 * Note: All IP addresses and port numbers are in host byte order, as
 * ntohl() conversion is already applied by ath12k_dp_rx_ppeds_fse_update_flow_info().
 */
struct ath12k_fse_update_event {
	u32 src_ip[4];
	u32 src_port;
	u32 dest_ip[4];
	u32 dest_port;
	u8  protocol;
	u8  version;
	u8  op;
};

int ath12k_register_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask);
int ath12k_unregister_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask);
bool ath12k_ppdu_notifier_has_listeners(enum ath12k_event_type event_type);
int ath12k_ppdu_notifier_call_chain(unsigned long val, void *v);

/* FSE_UPDATE notifier APIs */
int ath12k_register_fse_update_notifier(struct notifier_block *nb,
					 unsigned long event_mask);
int ath12k_unregister_fse_update_notifier(struct notifier_block *nb,
					   unsigned long event_mask);
bool ath12k_fse_update_notif_has_listeners(void);
int ath12k_fse_update_notif_call_chain(void *v);

#endif /* _ATH12K_NOTIF_H_ */
