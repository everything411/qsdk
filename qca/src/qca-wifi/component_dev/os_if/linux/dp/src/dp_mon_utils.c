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

#include <ol_if_athvar.h>
#include <cdp_txrx_ctrl.h>
#include <dp_mon.h>

#ifdef QCA_SUPPORT_LITE_MONITOR

void monitor_osif_process_rx_mpdu(osif_dev *osifp, qdf_nbuf_t mpdu_ind);
void monitor_osif_deliver_tx_capture_data(osif_dev *osifp, struct sk_buff *skb);

/**
 * wlan_lite_mon_rx_process - rx lite mon wdi event handler
 * @pdev_hdl: dp pdev hdl
 * @event: WDI_EVENT_LITE_MON_RX
 * @data: nbuf
 * @peer_id: peer id
 * @status: status
 *
 * Return: void
 */
static void wlan_lite_mon_rx_process(void *pdev_hdl, enum WDI_EVENT event,
				     void *data, uint16_t peer_id,
				     uint32_t status)
{
	/* handle rx mpdus */
	qdf_nbuf_t skb = (qdf_nbuf_t)data;
	struct wlan_objmgr_pdev *pdev_obj =
			(struct wlan_objmgr_pdev *)pdev_hdl;
	struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
	struct ieee80211vap *vap = NULL;

	if (!skb)
		return;

	if (!ic) {
		qdf_nbuf_free(skb);
		qdf_debug("ic is NULL");
		return;
	}

	vap = ic->ic_mon_vap;
	if (!vap) {
		qdf_nbuf_free(skb);
		qdf_debug("No mon vap to dump skb");
		return;
	}

	monitor_osif_process_rx_mpdu((osif_dev *)vap->iv_ifp, skb);
}

/**
 * wlan_lite_mon_tx_process - tx lite mon wdi event handler
 * @pdev_hdl: dp pdev hdl
 * @event: WDI_EVENT_LITE_MON_TX
 * @data: nbuf
 * @peer_id: peer id
 * @status: status
 *
 * Return: void
 */
static void wlan_lite_mon_tx_process(void *pdev_hdl, enum WDI_EVENT event,
				     void *data, uint16_t peer_id,
				     uint32_t status)
{
	qdf_nbuf_t skb = NULL;
	struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *)pdev_hdl;
	struct ieee80211com *ic;
	struct ieee80211vap *vap;
	osif_dev  *osifp = NULL;
	struct cdp_tx_indication_info *ptr_tx_info;

	ptr_tx_info = (struct cdp_tx_indication_info *)data;

	if (!ptr_tx_info->mpdu_nbuf)
		return;

	/* If vap is configured, deliver to that vap, else deliver to monitor
	 * vap. If vap is not configured and monitor vap is also not present,
	 * then free the skb
	 */
	if (ptr_tx_info->osif_vdev) {
		osifp = (osif_dev *)ptr_tx_info->osif_vdev;
	} else {
		ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
		if (!ic) {
			qdf_nbuf_free(ptr_tx_info->mpdu_nbuf);
			ptr_tx_info->mpdu_nbuf = NULL;
			dp_mon_err("ic is NULL");
			return;
		}
		vap = (struct ieee80211vap *)ic->ic_mon_vap;
		if (vap)
			osifp = (osif_dev *)vap->iv_ifp;
	}

	if (!osifp) {
		qdf_nbuf_free(ptr_tx_info->mpdu_nbuf);
		ptr_tx_info->mpdu_nbuf = NULL;
		dp_mon_debug("No vap to deliver data");
		return;
	}

	dp_mon_info("ppdu_id[%d] frm_type[%d] [%p]sending to stack!!!!",
		  ptr_tx_info->mpdu_info.ppdu_id,
		  ptr_tx_info->mpdu_info.frame_type,
		  ptr_tx_info->mpdu_nbuf);

	skb = ptr_tx_info->mpdu_nbuf;
	ptr_tx_info->mpdu_nbuf = NULL;
	monitor_osif_deliver_tx_capture_data(osifp, skb);
}

/**
 * wlan_lite_mon_rx_subscribe - subscribe for lite mon rx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_rx_subscribe(struct ol_ath_softc_net80211 *scn)
{
	ol_txrx_soc_handle soc_txrx_handle =
			wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

	scn->lite_mon_rx_subscriber.callback = wlan_lite_mon_rx_process;
	scn->lite_mon_rx_subscriber.context = scn->sc_pdev;
	cdp_wdi_event_sub(soc_txrx_handle,
			  wlan_objmgr_pdev_get_pdev_id(scn->sc_pdev),
			  &scn->lite_mon_rx_subscriber,
			  WDI_EVENT_LITE_MON_RX);
}

/**
 * wlan_lite_mon_rx_unsubscribe - unsubscribe for lite mon rx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_rx_unsubscribe(struct ol_ath_softc_net80211 *scn)
{
	ol_txrx_soc_handle soc_txrx_handle =
			wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

	cdp_wdi_event_unsub(soc_txrx_handle,
			    wlan_objmgr_pdev_get_pdev_id(scn->sc_pdev),
			    &scn->lite_mon_rx_subscriber,
			    WDI_EVENT_LITE_MON_RX);
}

/**
 * wlan_lite_mon_tx_subscribe - subscribe for lite mon tx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_tx_subscribe(struct ol_ath_softc_net80211 *scn)
{
	ol_txrx_soc_handle soc_txrx_handle =
			wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

	scn->lite_mon_tx_subscriber.callback = wlan_lite_mon_tx_process;
	scn->lite_mon_tx_subscriber.context = scn->sc_pdev;
	cdp_wdi_event_sub(soc_txrx_handle,
			  wlan_objmgr_pdev_get_pdev_id(scn->sc_pdev),
			  &scn->lite_mon_tx_subscriber,
			  WDI_EVENT_LITE_MON_TX);
}

/**
 * wlan_lite_mon_tx_unsubscribe - unsubscribe for lite mon tx wdi event
 * @scn: scn hdl
 *
 * Return: void
 */
void wlan_lite_mon_tx_unsubscribe(struct ol_ath_softc_net80211 *scn)
{
	ol_txrx_soc_handle soc_txrx_handle =
			wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

	cdp_wdi_event_unsub(soc_txrx_handle,
			    wlan_objmgr_pdev_get_pdev_id(scn->sc_pdev),
			    &scn->lite_mon_tx_subscriber,
			    WDI_EVENT_LITE_MON_TX);
}
#endif
