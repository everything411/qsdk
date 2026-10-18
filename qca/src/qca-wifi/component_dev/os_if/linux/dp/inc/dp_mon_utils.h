/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DP_MON_UTILS_H_
#define _DP_MON_UTILS_H_
/* fwd declartion */
struct ol_ath_softc_net80211;

#ifdef QCA_SUPPORT_LITE_MONITOR
/**
 * wlan_lite_mon_rx_subscribe - subscribe for lite mon rx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_rx_subscribe(struct ol_ath_softc_net80211 *scn);

/**
 * wlan_lite_mon_rx_unsubscribe - unsubscribe for lite mon rx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_rx_unsubscribe(struct ol_ath_softc_net80211 *scn);

/**
 * wlan_lite_mon_tx_subscribe - subscribe for lite mon tx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_tx_subscribe(struct ol_ath_softc_net80211 *scn);

/**
 * wlan_lite_mon_tx_unsubscribe - unsubscribe for lite mon tx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_tx_unsubscribe(struct ol_ath_softc_net80211 *scn);
#endif
#endif /* _DP_MON_UTILS_H_ */
