/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Layer b/w umac and target_if (ol) txops
 * It contains wrapers for txops
 */

#include <wlan_wifi_radar_tgt_api.h>
#include <wlan_wifi_radar_utils_api.h>
#include <wifi_radar_defs_i.h>

static inline struct wlan_lmac_if_wifi_radar_tx_ops *
	wlan_psoc_get_wifi_radar_txops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		wifi_radar_err("tx ops is NULL");
		return NULL;
	}
	return &tx_ops->wifi_radar_tx_ops;
}

QDF_STATUS tgt_wifi_radar_init_pdev(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_wifi_radar_tx_ops *wifi_radar_tx_ops = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wifi_radar_tx_ops = wlan_psoc_get_wifi_radar_txops(psoc);
	if (!wifi_radar_tx_ops) {
		wifi_radar_err("Failed to get wifi radar tx_ops");
		return QDF_STATUS_E_FAILURE;
	}

	if (wifi_radar_tx_ops->wifi_radar_init_pdev)
		status = wifi_radar_tx_ops->wifi_radar_init_pdev(psoc, pdev);

	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Error occurred with exit code %d\n", status);

	return status;
}

QDF_STATUS tgt_wifi_radar_deinit_pdev(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_wifi_radar_tx_ops *wifi_radar_tx_ops = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wifi_radar_tx_ops = wlan_psoc_get_wifi_radar_txops(psoc);
	if (!wifi_radar_tx_ops) {
		wifi_radar_err("Failed to get wifi radar tx_ops");
		return QDF_STATUS_E_FAILURE;
	}

	if (wifi_radar_tx_ops->wifi_radar_deinit_pdev)
		status = wifi_radar_tx_ops->wifi_radar_deinit_pdev(psoc, pdev);

	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Error occurred with exit code %d\n", status);

	return status;
}

void tgt_wifi_radar_support_set(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct psoc_wifi_radar *wifi_radar_sc;

	if (!psoc)
		return;

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj
				(psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!wifi_radar_sc)
		return;

	wifi_radar_sc->is_wifi_radar_capable = !!value;
	wifi_radar_info("wifi_radar: FW support advert=%d",
			wifi_radar_sc->is_wifi_radar_capable);
}

uint32_t tgt_wifi_radar_info_send(struct wlan_objmgr_pdev *pdev, void *head,
				  size_t hlen, void *data, size_t dlen,
				  void *tail, size_t tlen)
{
	struct pdev_wifi_radar *pa;
	uint32_t status;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pa) {
		wifi_radar_err("pdev_wifi_radar is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (head)
		status = wifi_radar_streamfs_write(pa, (const void *)head,
						   hlen);

	if (data)
		status = wifi_radar_streamfs_write(pa, (const void *)data,
						   dlen);

	if (tail)
		status = wifi_radar_streamfs_write(pa, (const void *)tail,
						   tlen);

	/* finalize the write */
	status = wifi_radar_streamfs_flush(pa);

	return status;
}

int tgt_wifi_radar_validate_period(struct wlan_objmgr_psoc *psoc,
				   u_int32_t period)
{
	if ((period < MIN_WIFI_RADAR_PRD) ||
	    (period > MAX_WIFI_RADAR_PRD)) {
		wifi_radar_err("Invalid period value");
		return 0;
	}

	return 1;
}

static int
num_chains(u_int32_t chainmask)
{
	int count = 0;

	while (chainmask) {
		count += chainmask & 1;
		chainmask = chainmask >> 1;
	}

	return count;
}

int tgt_wifi_radar_validate_chainmask(struct pdev_wifi_radar *pwr,
				      u_int32_t tx_chainmask,
				      u_int32_t rx_chainmask)
{
	uint8_t txchain_pos = 0, i = 0;
	unsigned long chain = tx_chainmask;

	if (!tx_chainmask) {
		wifi_radar_err("Invalid tx chain mask, 1 chain must be set");
		return 0;
	}

	if (num_chains(tx_chainmask) > 1) {
		wifi_radar_err("Only one tx chain allowed for capture");
		return 0;
	}

	txchain_pos = qdf_find_first_bit(&chain, (sizeof(chain) * 8));
	if (txchain_pos >= HOST_MAX_CHAINS) {
		wifi_radar_err(" invalid tx chain requested for wifi radar");
		return 0;
	}

	if (!num_chains(rx_chainmask)) {
		wifi_radar_err("zero rx chainmask invalid");
		return 0;
	}

	if (num_chains(rx_chainmask) > pwr->max_num_rx_chain) {
		wifi_radar_err("no of rx chains exceeds %d", pwr->max_num_rx_chain);
		return 0;
	}

	if (pwr->best_isolated_chain_pair_sel) {
		if (((txchain_pos != 0) &&
		     qdf_test_bit((txchain_pos - 1), (unsigned long *)(&rx_chainmask))) ||
		    ((txchain_pos != (HOST_MAX_CHAINS - 1)) &&
		     qdf_test_bit((txchain_pos + 1), (unsigned long *)(&rx_chainmask)))) {
			wifi_radar_err("Invalid rx chains - cannot be adjacent to tx chain");
			return 0;
		}
	}

	qdf_spin_lock_bh(&pwr->cal_status_lock);
	for (i = 0; i < HOST_MAX_CHAINS; i++) {
		if (!((rx_chainmask >> i) & 1))
			continue;

		if (pwr->per_chain_comb_cal_status[txchain_pos][i]) {
			continue;
		} else {
			wifi_radar_err("cal fail on txchain %d rx chain %d",
				       txchain_pos, i);
			qdf_spin_unlock_bh(&pwr->cal_status_lock);
			return 0;
		}
	}
	qdf_spin_unlock_bh(&pwr->cal_status_lock);

	return 1;
}

int tgt_wifi_radar_command(struct wlan_objmgr_pdev *pdev,
			   struct wifi_radar_command_params *params)
{
	struct wlan_lmac_if_wifi_radar_tx_ops *wr_tx_ops = NULL;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wr_tx_ops = wlan_psoc_get_wifi_radar_txops(psoc);

	if (wr_tx_ops && wr_tx_ops->wifi_radar_capture_and_cal)
		return wr_tx_ops->wifi_radar_capture_and_cal(pdev, params);

	return 0;
}
