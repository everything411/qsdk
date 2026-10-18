/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
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

typedef void (*ecm_classifier_dscp_stats_method_t)(struct net_device *from_dev, struct net_device *to_dev,
		uint8_t *smac, uint8_t *dmac, uint8_t dscp_marked, uint8_t flow_mark, uint8_t reply_mark,
		uint8_t flow_dscp, uint8_t reply_dscp, uint32_t tx_bytes, uint32_t rx_bytes, bool is_ipv6);

/**
 * Register for callback of the user module
 */
struct ecm_classifier_dscp_stats_registrant {
	struct module *this_module;		/**< Caller module */
	ecm_classifier_dscp_stats_method_t cb;	/**< callback handler */
};

/**
 * Registers a callback with the DSCP classifier for stats.
 *
 * @param	r		external registrant
 *
 * @return
 * Success / failure
 */
int ecm_classifier_dscp_stats_register(struct ecm_classifier_dscp_stats_registrant *r);

/**
 * Unregisters a callback with the DSCP classifier for stats.
 *
 * @param	r		external registrant
 *
 * @return
 * none
 */
void ecm_classifier_dscp_stats_unregister(struct ecm_classifier_dscp_stats_registrant *r);
