/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include "ini.h"
#include "sa_api.h"
#include <linux/etherdevice.h>
#include "../peer.h"
#include "../debug.h"
#include <linux/notifier.h>
#include "ath12k_notif.h"

extern struct ath12k_smart_ant_ops __rcu *g_sa_ops;

/* PPDU notifier block for Smart Antenna */
static struct notifier_block sa_ppdu_nb;
static atomic_t sa_ppdu_nb_registered = ATOMIC_INIT(0);

/* This is internal structure and used only from TX/RX feedback
 * during per PPDU stats processing to carry the content for
 * further action from iterator.
 */
struct sa_txrx_fb_ctx {
	struct ath12k_sa_rx_feedback *rx;
	struct ath12k_sa_tx_feedback *tx;
	struct smart_ant_train_info param;
	u32 status;
	s8 ret;
	u32 pdev_id;
	u32 vdev_id;
	u32 value;
	u32 ant_array[SA_MAX_SA_CHAINS];
};

/*
 * Cached pointer to the hw_group (ag).
 *
 * Set once in sa_core_init() which runs in process context, and cleared
 * in sa_core_deinit() when the last radio is torn down.  The atomic
 * notifier callbacks (sa_core_ppdu_event_notify and its callees) read
 * this pointer without any lock: this is safe because:
 *   - writes happen only in process context, serialised by the driver
 *     init/deinit sequence;
 *   - the pointer is stable for the entire lifetime of the SA session;
 *   - a stale NULL read in the notifier simply causes an early return.
 */
static struct ath12k_hw_group __rcu *sa_cached_ag;

static struct ath12k_base *sa_core_get_ab_from_device_id(u8 device_id)
{
	struct ath12k_hw_group *ag;
	struct ath12k_base *ab = NULL;

	rcu_read_lock();
	/*
	 * Use the cached ag pointer instead of ath12k_core_get_ag().
	 * ath12k_core_get_ag() acquires a sleepable mutex which is illegal
	 * here because this function is called from an ATOMIC_NOTIFIER
	 * callback (BH/softirq context).  sa_cached_ag is set once at
	 * sa_core_init() time (process context) and is stable for the
	 * lifetime of the SA session, so reading it without a lock is safe.
	 */
	ag = rcu_dereference(sa_cached_ag);
	if (!ag) {
		rcu_read_unlock();
		return NULL;
	}

	/* Directly index ab using device_id to avoid iterating */
	if (device_id >= ag->num_devices) {
		rcu_read_unlock();
		return NULL;
	}
	ab = ag->ab[device_id];
	if (!ab) {
		rcu_read_unlock();
		return NULL;
	}
	rcu_read_unlock();

	return ab;
}

static inline int sa_core_get_txantenna(void *ccp, u32 *antenna_array)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_txantenna || !antenna_array) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_txantenna invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_txantenna(ccp, antenna_array);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_txantenna op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_txdefaultantenna(u8 radio_id, u32 *antenna)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_txdefaultantenna || !antenna) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_txdefaultantenna invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_txdefaultantenna(radio_id, antenna);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_txdefaultantenna op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_rxantenna(u8 radio_id, u32 *rx_antenna)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_rxantenna || !rx_antenna) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_rxantenna invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_rxantenna(radio_id, rx_antenna);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_rxantenna op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_traininginfo(void *ccp, u32 *rate_array,
					   u32 *antenna_array, u32 *numpkts)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_traininginfo ||
	    !rate_array || !antenna_array || !numpkts) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_traininginfo invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_traininginfo(ccp, rate_array, antenna_array, numpkts);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_traininginfo op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_bcn_txantenna(u8 radio_id, u32 *bcn_txantenna)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_bcn_txantenna || !bcn_txantenna) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_bcn_txantenna invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_bcn_txantenna(radio_id, bcn_txantenna);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_bcn_txantenna op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_param(struct ath12k_sa_config_params *params)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_param || !params) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_param invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_param(params);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_param op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_node_config(void *ccp, u32 id, u32 *config)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_node_config || !config) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_node_config invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_node_config(ccp, id, config);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_node_config op failed (%d)\n", ret);

	return ret;
}

static inline
int sa_core_get_ext_traininginfo(void *ccp, u16 *minpkts, u16 *per_th, u32 *extn_flag)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_ext_traininginfo || !minpkts ||
	    !per_th || !extn_flag) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_ext_traininginfo invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_ext_traininginfo(ccp, minpkts, per_th, extn_flag);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_ext_traininginfo op failed (%d)\n", ret);

	return ret;
}

static inline
int sa_core_get_gpio_config(u8 radio_id, struct ath12k_sa_gpio_config *gpio_cfg)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_gpio_config || !gpio_cfg) {
		rcu_read_unlock();
		ath12k_err(NULL, "sa_core: get_gpio_config invalid ops/args\n");
		return -1;
	}
	ret = ops->sa_get_gpio_config(radio_id, gpio_cfg);
	rcu_read_unlock();
	if (ret)
		ath12k_err(NULL, "sa_core: get_gpio_config op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_rts_ant_flag(u8 radio_id, u32 *en_rts)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_enable_rts_ant || !en_rts) {
		rcu_read_unlock();
		return -1;
	}
	ret = ops->sa_get_enable_rts_ant(radio_id, en_rts);
	rcu_read_unlock();
	if (ret)
		ath12k_dbg(NULL, ATH12K_DBG_ANY,
			   "sa_core: sa_get_enable_rts_ant op failed (%d)\n", ret);

	return ret;
}

static inline
int sa_core_get_max_mpdu_count_for_training(u8 radio_id, u32 *value)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_max_mpdu_count_for_training || !value) {
		rcu_read_unlock();
		return -1;
	}
	ret = ops->sa_get_max_mpdu_count_for_training(radio_id, value);
	rcu_read_unlock();
	if (ret)
		ath12k_dbg(NULL, ATH12K_DBG_ANY,
			   "sa_core: sa_get_max_mpdu_count_for_training op failed (%d)\n", ret);

	return ret;
}

static inline int sa_core_get_mcs_margin(u8 radio_id, u32 *value)
{
	struct ath12k_smart_ant_ops *ops;
	int ret;

	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_mcs_margin || !value) {
		rcu_read_unlock();
		return -1;
	}
	ret = ops->sa_get_mcs_margin(radio_id, value);
	rcu_read_unlock();
	if (ret)
		ath12k_dbg(NULL, ATH12K_DBG_ANY,
			   "sa_core: sa_get_mcs_margin op failed (%d)\n", ret);

	return ret;
}

void sa_get_cfg_flags(struct ath12k_base *ab, u8 *sa_en, u8 *sa_sw_test)
{
	struct sa_base_extn *sa_base;

	if (ab) {
		if (sa_en)
			*sa_en = !!ath12k_cfg_get(ab, ATH12K_CFG_SA_ENABLE);
		if (sa_sw_test)
			*sa_sw_test = ath12k_cfg_get(ab, ATH12K_CFG_SA_SW_TEST);
		sa_base = &ab->ath12k_base_extn.sa_base;
		sa_base->sa_mode_overwrite =
					ath12k_cfg_get(ab, ATH12K_CFG_SA_MODE);
		sa_base->sa_en_rts_overwrite =
					ath12k_cfg_get(ab, ATH12K_CFG_EN_RTS_ANT);
	}
}

/* Called within rcu_read_lock() */
static void sa_core_peer_connect_notify(struct ath12k *ar,
					struct ath12k_link_sta *arsta,
					struct sa_sta_ratecode_list *param)
{
	struct ath12k_smart_ant_ops *ops;
	struct ieee80211_channel *chan;
	struct ieee80211_link_sta *link_sta;
	struct ath12k_link_vif *arvif;
	struct sa_sta_extn *sa_sta;
	struct ath12k_sa_node_info node_info = {0};

	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_peer_connect)
		return;

	arvif = arsta->arvif;
	link_sta = ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		ath12k_err(ar->ab, "sa_core: unable to access link_sta\n");
		return;
	}
	sa_sta = &arsta->arsta_extn.sa_sta;
	ether_addr_copy(node_info.mac_addr, arsta->addr);
	node_info.radio_id = ar->radio_idx;
	node_info.max_ch_bw = link_sta->bandwidth;
	node_info.nss_tx = arsta->peer_nss;
	node_info.nss_rx = link_sta->rx_nss;
	if (param)
		memcpy(&node_info.rate_cap, &param->rate_cap,
		       sizeof(param->rate_cap));

	chan = arvif->chanctx.def.chan;
	if (chan) {
		node_info.band = chan->band;
		node_info.chan_num = chan->hw_value;
	}

	node_info.opmode = SA_OPMODE_CCK_OFDM;
	if (link_sta->uhr_cap.has_uhr)
		node_info.opmode = SA_OPMODE_UHR;
	else if (link_sta->eht_cap.has_eht)
		node_info.opmode = SA_OPMODE_EHT;
	else if (link_sta->he_cap.has_he)
		node_info.opmode = SA_OPMODE_HE;
	else if (link_sta->vht_cap.vht_supported)
		node_info.opmode = SA_OPMODE_VHT;
	else if (link_sta->ht_cap.ht_supported)
		node_info.opmode = SA_OPMODE_HT;

	if (ops->sa_peer_connect(&sa_sta->ctx, &node_info))
		ath12k_err(ar->ab, "sa_core: unable to update STA info\n");
}

void sa_core_peer_disconnect_notify(struct ath12k_link_sta *arsta)
{
	struct ath12k_smart_ant_ops *ops;
	struct sa_sta_extn *sa_sta;
	struct ath12k *ar;

	rcu_read_lock();
	if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
		rcu_read_unlock();
		return;
	}

	ar = arsta->arvif->ar;
	if (!ar->ab || !ath12k_smart_ant_is_enable(ar->ab)) {
		rcu_read_unlock();
		return;
	}

	sa_sta = &arsta->arsta_extn.sa_sta;
	ops = rcu_dereference(g_sa_ops);
	if (ops && ops->sa_peer_disconnect) {
		if (sa_sta->ctx)
			ops->sa_peer_disconnect(&sa_sta->ctx);
	} else if (sa_sta->ctx) {
		kfree(sa_sta->ctx);
	}

	sa_sta->ctx = NULL;
	rcu_read_unlock();
}

static int
sa_core_wmi_send_parallel_mode_gpio_drive(struct ath12k *ar, u32 value)
{
	struct sa_ar_extn *sa_ar;
	int ret = 0;

	if (!ar)
		return -EINVAL;

	sa_ar = &ar->ar_extn.sa_ar;
	if (sa_ar->sa_mode == WMI_SMART_ANT_MODE_PARALLEL) {
		ret = ath12k_wmi_pdev_set_param(ar,
				WMI_PDEV_PARAM_SA_PARALLEL_MODE_GPIO_DRIVE_CFG,
				value, ar->pdev->pdev_id);
	} else {
		ath12k_err(ar->ab, "Operation not permitted in Serial Mode\n");
		ret = -EPERM;
	}

	return ret;
}

static int
sa_core_wmi_send_smart_ant_enable(struct ath12k *ar, u32 tx_ant, u32 rx_ant,
				  struct ath12k_sa_gpio_config *gpio_cfg,
				  u32 sa_flag)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sa_ar_extn *sa_ar = &ar->ar_extn.sa_ar;
	struct wmi_pdev_set_smart_ant_cmd *cmd;
	struct wmi_pdev_smart_ant_gpio_handle_cmd *gpio_param;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int i, ret, len;
	void *ptr;
	u32 *pdev_sa_flag;

	len = sizeof(*cmd) + TLV_HDR_SIZE +
	      WMI_SMART_ANT_HAL_MAX * sizeof(*gpio_param) +
	      TLV_HDR_SIZE + sizeof(u32);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SMART_ANT_ENABLE_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->mode = sa_ar->sa_mode;
	cmd->tx_default_antenna = tx_ant;
	cmd->rx_antenna = rx_ant;
	cmd->enable = sa_ar->enable_mask & SA_ENABLE_MASK;

	ptr = skb->data + sizeof(*cmd);
	len = WMI_SMART_ANT_HAL_MAX * sizeof(*gpio_param);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;
	gpio_param = ptr;

	for (i = 0; i < WMI_SMART_ANT_HAL_MAX; i++) {
		gpio_param->tlv_header =
			FIELD_PREP(WMI_TLV_TAG,
				   WMI_TAG_PDEV_SMART_ANT_GPIO_HANDLE) |
			FIELD_PREP(WMI_TLV_LEN,
				   sizeof(*gpio_param) - TLV_HDR_SIZE);
		if (sa_ar->sa_mode == WMI_SMART_ANT_MODE_SERIAL) {
			if (i < WMI_SMART_ANT_MAX_SERIAL_ANT) {
				gpio_param->gpio_pin = gpio_cfg->gpio_pin[i];
				gpio_param->gpio_func = gpio_cfg->gpio_func[i];
			} else {
				gpio_param->gpio_pin = 0;
				gpio_param->gpio_func = 0;
			}
		} else if (sa_ar->sa_mode == WMI_SMART_ANT_MODE_PARALLEL) {
			gpio_param->gpio_pin = gpio_cfg->gpio_pin[i];
			gpio_param->gpio_func = gpio_cfg->gpio_func[i];
		}
		gpio_param->pdev_id = ar->pdev->pdev_id;
		gpio_param++;
	}

	ptr += len;
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(u32));
	ptr += TLV_HDR_SIZE;
	pdev_sa_flag = ptr;
	*pdev_sa_flag = (sa_flag & 1);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SMART_ANT_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_SMART_ANT_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int sa_core_wmi_set_rx_antenna(struct ath12k_base *ab,
				      struct ath12k_wmi_pdev *wmi, u32 pdev_id,
				      u32 rx_antenna)
{
	struct wmi_pdev_set_rx_antenna_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_rx_antenna_cmd *) skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SMART_ANT_SET_RX_ANTENNA_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->rx_antenna = rx_antenna;

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID failed\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int sa_core_wmi_set_tx_antenna(struct ath12k_base *ab,
				      struct ath12k_wmi_pdev *wmi, u32 vdev_id,
				      u8 *macaddr, u32 *tx_ant_series)
{
	struct wmi_pdev_set_tx_antenna_cmd *cmd;
	struct wmi_peer_set_smart_tx_ant_series_cmd *series;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int i, ret, len, series_len;

	len = sizeof(*cmd) + TLV_HDR_SIZE;
	len += WMI_SMART_ANT_MAX_RATE_SERIES *
	       sizeof(struct wmi_peer_set_smart_tx_ant_series_cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_SMART_ANT_SET_TX_ANTENNA_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->macaddr.addr, macaddr);

	ptr = skb->data + sizeof(*cmd);

	series_len = WMI_SMART_ANT_MAX_RATE_SERIES *
		     sizeof(struct wmi_peer_set_smart_tx_ant_series_cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, series_len);
	ptr += TLV_HDR_SIZE;

	series = ptr;
	for (i = 0; i < WMI_SMART_ANT_MAX_RATE_SERIES; i++) {
		series->tlv_header =
			FIELD_PREP(WMI_TLV_TAG,
				   WMI_TAG_PEER_SMART_ANT_SET_TX_ANTENNA_SERIES) |
			FIELD_PREP(WMI_TLV_LEN, sizeof(*series) - TLV_HDR_SIZE);
		series->ant_series = tx_ant_series[i];
		series++;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int sa_core_wmi_set_train_info(struct ath12k_base *ab,
				      struct ath12k_wmi_pdev *wmi, u8 *macaddr,
				      struct smart_ant_train_info *param)
{
	struct wmi_peer_set_smart_ant_train_ant_fixed_param_cmd *cmd;
	struct wmi_peer_set_smart_ant_train_ant_param *train_param;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int i, ret, len, itr = 0, series_len;

	len = sizeof(*cmd) + TLV_HDR_SIZE;
	len += WMI_SMART_ANT_MAX_RATE_SERIES *
	       sizeof(struct wmi_peer_set_smart_ant_train_ant_param);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PEER_SMART_ANT_SET_TRAIN_ANTENNA_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = param->vdev_id;
	ether_addr_copy(cmd->macaddr.addr, macaddr);
	cmd->numpkts = param->numpkts;
	if ((param->minpkts != SA_INVALID_PARAM_VALUE) ||
	    (param->per_th != SA_INVALID_PARAM_VALUE)) {
		cmd->per_check_bitmap = 1;
		cmd->per_check_bitmap |=
			(param->per_th & SA_MASK_PER_TH) << SA_PER_TH_BIT_POS;
		cmd->per_check_bitmap |= (param->minpkts & SA_MASK_MIN_PKTS) <<
					  SA_MIN_PKTS_BIT_POS;
	}
	if (param->extn_train_flags & SA_LATENCY_TRAFFIC_DISABLE)
		cmd->per_check_bitmap |= (1 << SA_LTNC_TR_BIT_POS);
	if (param->extn_train_flags & SA_QOS_NULL_ENABLE)
		cmd->per_check_bitmap |= (1 << SA_EN_QOS_NULL_BIT_POS);

	ptr = skb->data + sizeof(*cmd);

	series_len = WMI_SMART_ANT_MAX_RATE_SERIES *
		     sizeof(struct wmi_peer_set_smart_ant_train_ant_param);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, series_len);
	ptr += TLV_HDR_SIZE;

	train_param = ptr;
	for (i = 0; i < WMI_SMART_ANT_MAX_RATE_SERIES; i++) {
		train_param->tlv_header =
			FIELD_PREP(WMI_TLV_TAG,
				   WMI_TAG_PEER_SMART_ANT_SET_TRAIN_ANTENNA_PARAM) |
			FIELD_PREP(WMI_TLV_LEN,
				   sizeof(*train_param) - TLV_HDR_SIZE);
		train_param->train_rate_series_lo =
			((param->rate_array[itr] & SA_MASK_RCODE) |
			 (param->rate_array[itr] & (SA_MASK_RCODE << 16)));
		train_param->train_rate_series_hi =
			((param->rate_array[itr + 1] & SA_MASK_RCODE) |
			 (param->rate_array[itr + 1] & (SA_MASK_RCODE << 16)));
		/* Higher 16-bits are Reserved */
		train_param->train_rate_series_ext =
				param->rate_array[itr + 2] & SA_MASK_RCODE;
		itr += 3;
		train_param->train_antenna_series = param->antenna_array[i];
		train_param->rc_flags = 0;
		train_param++;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int
sa_core_wmi_send_node_config_ops(struct ath12k_base *ab,
				 struct ath12k_wmi_pdev *wmi, u32 vdev_id,
				 u8 *macaddr, u32 cmd_id, u16 args_count,
				 u32 *args_arr)
{
	struct wmi_peer_set_smart_ant_node_config_ops_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int len, ret;
	u32 *cfg_arg;
	u8 i;

	if (!macaddr || !args_count || !args_arr) {
		ath12k_warn(ab, "Cannot send with invalid arguments");
		return -EINVAL;
	}

	len = sizeof(*cmd) + TLV_HDR_SIZE + args_count * sizeof(u32);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->macaddr.addr, macaddr);
	cmd->cmd_id = cmd_id;
	cmd->args_count = args_count;

	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, args_count * sizeof(u32));
	ptr += TLV_HDR_SIZE;
	cfg_arg = (u32 *)ptr;
	for (i = 0; i < args_count; i++)
		cfg_arg[i] = args_arr[i];

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send SMART_ANT_SET_NODE_CONFIG_OPS");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void sa_core_extract_sta_legacy_ratecodes(const void *ptr, void *data)
{
	struct wmi_peer_cck_ofdm_rate_info *ofdm_rate =
				(struct wmi_peer_cck_ofdm_rate_info *)ptr;
	struct sa_sta_ratecode_list *param =
				(struct sa_sta_ratecode_list *)data;
	struct ath12k_sa_rate_cap *rate_cap = &param->rate_cap;
	u8 i = param->legacy_index;

	if (i >= (SA_MAX_CCK_OFDM_RATES - 1))
		return;

	rate_cap->rc_legacy[i] = ofdm_rate->ratecode_legacy & SA_MASK_RCODE;
	rate_cap->rc_legacy[i + 1] =
			(ofdm_rate->ratecode_legacy >> 16) & SA_MASK_RCODE;
	param->legacy_index += 2;
}

static void sa_core_extract_sta_mcs_ratecodes(const void *ptr, void *data)
{
	struct wmi_peer_mcs_rate_info *mcs_rate =
				(struct wmi_peer_mcs_rate_info *)ptr;
	struct sa_sta_ratecode_list *param =
				(struct sa_sta_ratecode_list *)data;
	struct ath12k_sa_rate_cap *rate_cap = &param->rate_cap;
	u8 j = param->mcs_index;

	if (j >= (SA_MAX_MCS_RATES - 1))
		return;

	rate_cap->rc_20[j] = mcs_rate->rt_code_20 & SA_MASK_RCODE;
	rate_cap->rc_40[j] = mcs_rate->rt_code_40 & SA_MASK_RCODE;
	rate_cap->rc_80[j] = mcs_rate->rt_code_80 & SA_MASK_RCODE;
	rate_cap->rc_160[j] = mcs_rate->rt_code_160 & SA_MASK_RCODE;
	rate_cap->rc_320[j] = mcs_rate->rt_code_320 & SA_MASK_RCODE;
	rate_cap->rc_20[j + 1] = (mcs_rate->rt_code_20 >> 16) & SA_MASK_RCODE;
	rate_cap->rc_40[j + 1] = (mcs_rate->rt_code_40 >> 16) & SA_MASK_RCODE;
	rate_cap->rc_80[j + 1] = (mcs_rate->rt_code_80 >> 16) & SA_MASK_RCODE;
	rate_cap->rc_160[j + 1] = (mcs_rate->rt_code_160 >> 16) & SA_MASK_RCODE;
	rate_cap->rc_320[j + 1] = (mcs_rate->rt_code_320 >> 16) & SA_MASK_RCODE;
	param->mcs_index += 2;
}

static int
sa_core_wmi_peer_rc_list_subtlv_parser(struct ath12k_base *ab, u16 tag, u16 len,
				       const void *ptr, void *data)
{
	int ret = 0;

	switch (tag) {
	case WMI_TAG_PEER_CCK_OFDM_RATE_INFO:
		sa_core_extract_sta_legacy_ratecodes(ptr, data);
		break;
	case WMI_TAG_PEER_MCS_RATE_INFO:
		sa_core_extract_sta_mcs_ratecodes(ptr, data);
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid tag for peer ratecode in subtlvs\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void sa_core_peer_rc_list_fixed_param_parser(const void *ptr, void *data)
{
	struct wmi_peer_ratecode_list_fixed_param *fixed_param =
			(struct wmi_peer_ratecode_list_fixed_param *)ptr;
	struct sa_sta_ratecode_list *param =
					(struct sa_sta_ratecode_list *)data;
	u8 i;

	ether_addr_copy(param->mac_addr, fixed_param->macaddr.addr);
	param->pdev_id = fixed_param->pdev_id;
	/* Rate Code counts for legacy, 20Mhz, 40Mhz & 80MHz BW */
	for (i = 0; i < SA_BYTES_IN_DWORD; i++)
		param->rate_cap.num_rc[i] =
			(fixed_param->ratecount >> (i*8)) & SA_MASK_BYTE;
	/* Rate Code counts for 160 MHz BW */
	param->rate_cap.num_rc[i] = fixed_param->ratecount_ext & SA_MASK_BYTE;
	/* Rate Code counts for 320 MHz BW */
	param->rate_cap.num_rc[i+1] =
			(fixed_param->ratecount_ext >> 8) & SA_MASK_BYTE;

}

static int sa_core_wmi_peer_rc_list_ev_parser(struct ath12k_base *ab, u16 tag,
					      u16 len, const void *ptr,
					      void *data)
{
	int ret = 0;

	switch (tag) {
	case WMI_TAG_PEER_RATECODE_LIST_EVENT:
		sa_core_peer_rc_list_fixed_param_parser(ptr, data);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  sa_core_wmi_peer_rc_list_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid TAG for peer ratecode list event\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

void sa_core_wmi_peer_rc_list_ev_handler(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	struct sa_sta_ratecode_list param = {0};
	struct ath12k *ar;
	struct ath12k_link_sta *arsta;
	int ret;

	if (!ath12k_smart_ant_is_enable(ab))
		return;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  sa_core_wmi_peer_rc_list_ev_parser,
				  &param);
	if (ret) {
		ath12k_warn(ab, "failed to parse SA RateCode List ev %d\n",
			    ret);
		return;
	}

	if (!is_valid_ether_addr(param.mac_addr))
		return;

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, param.pdev_id);
	if (!ar) {
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->arsta_lock);
	arsta = ath12k_link_sta_find_by_addr(ar, param.mac_addr);
	if (!arsta) {
		spin_unlock_bh(&ar->arsta_lock);
		rcu_read_unlock();
		return;
	}
	sa_core_peer_connect_notify(ar, arsta, &param);
	spin_unlock_bh(&ar->arsta_lock);
	rcu_read_unlock();
}

/* Called within rcu_read_lock() */
static int sa_core_get_training_info(void *sta_ctx,
				     struct smart_ant_train_info *param,
				     u32 vdev_id)
{
	struct ath12k_smart_ant_ops *ops;
	int ret = 0;

	if (!sta_ctx || !param)
		return -1;

	ops = rcu_dereference(g_sa_ops);
	if (!ops)
		return -1;

	param->vdev_id = vdev_id;
	param->minpkts = SA_INVALID_PARAM_VALUE;
	param->per_th = SA_INVALID_PARAM_VALUE;
	if (ops->sa_get_traininginfo)
		ret = ops->sa_get_traininginfo(sta_ctx,
					       &param->rate_array[0],
					       &param->antenna_array[0],
					       &param->numpkts);

	if (!ret && ops->sa_get_ext_traininginfo)
		ops->sa_get_ext_traininginfo(sta_ctx,
					     &param->minpkts,
					     &param->per_th,
					     &param->extn_train_flags);

	return ret;
}

/* Called within rcu_read_lock() */
static void sa_core_get_txrx_fb_ctx(struct ath12k *ar,
				    struct ath12k_link_sta *arsta,
				    void *data)
{
	struct sa_sta_extn *sa_sta = &arsta->arsta_extn.sa_sta;
	struct sa_txrx_fb_ctx *ctx = (struct sa_txrx_fb_ctx *)data;
	struct ath12k_smart_ant_ops *ops;

	if (!sa_sta->ctx) {
		ctx->ret = -1;
		return;
	}

	ops = rcu_dereference(g_sa_ops);
	if (!ops) {
		ctx->ret = -1;
		return;
	}
	if (ctx->tx && ops->sa_update_txfeedback) {
		if (ops->sa_update_txfeedback(sa_sta->ctx, ctx->tx,
					      &ctx->status)) {
			ath12k_err(ar->ab, "Failed to update TX Feedback");
			ctx->ret = -1;
			return;
		}
	} else if (ctx->rx && ops->sa_update_rxfeedback) {
		if (ops->sa_update_rxfeedback(sa_sta->ctx, ctx->rx,
					      &ctx->status)) {
			ath12k_err(ar->ab, "Failed to update RX Feedback");
			ctx->ret = -1;
			return;
		}
	} else {
		/* No message required as it may flood the console */
		ctx->ret = -1;
		return;
	}

	ctx->pdev_id = ar->pdev->pdev_id;
	ctx->vdev_id = arsta->arvif->vdev_id;

	if (!ctx->status)
		return;

	if (ctx->status & SA_RX_CONFIG_REQUIRED) {
		if (ops->sa_get_rxantenna)
			ops->sa_get_rxantenna(ar->radio_idx, &ctx->value);
	} else if (ctx->status & SA_TX_CONFIG_REQUIRED) {
		if (ops->sa_get_txantenna)
			ops->sa_get_txantenna(sa_sta->ctx, &ctx->ant_array[0]);
	} else if (ctx->status & SA_TRAINING_REQUIRED) {
		ctx->ret = sa_core_get_training_info(sa_sta->ctx, &ctx->param,
						     arsta->arvif->vdev_id);
	} else if (ctx->status & SA_TX_FEEDBACK_CONFIG_REQUIRED) {
		if (ops->sa_get_node_config)
			ops->sa_get_node_config(sa_sta->ctx,
						SA_TX_FEEDBACK_CONFIG,
						&ctx->value);
	}
}

/* To be called only from sa_core_ppdu_event_notify() */
static int sa_core_update_rx_feedback(struct ath12k_ppdu_rx_info *rx_info)
{
	struct sa_txrx_fb_ctx rx_ctx = {0};
	struct ath12k_sa_rx_feedback rx = {0};
	struct hal_rx_mon_ppdu_info *ppdu;
	struct hal_rx_user_status *stats;
	struct ath12k_base *ab;
	struct ath12k *ar;
	u8 i, j;

	/* Quick guard: bail early if no ops registered */
	if (!rcu_access_pointer(g_sa_ops))
		return -1;

	ppdu = &rx_info->ppdu_info;
	if (ppdu->peer_id == HAL_INVALID_PEERID)
		return 0;
	if (ppdu->num_users > 1)
		return 0;

	stats = &ppdu->userstats[0];
	ab = sa_core_get_ab_from_device_id(ppdu->device_id);
	if (!ab)
		return -1;

	if (!ath12k_smart_ant_is_enable(ab))
		return -1;

	if (!is_valid_ether_addr(stats->peer_mac))
		return -1;

	rx.rx_antenna = ppdu->rx_antenna;
	rx.rx_rate_mcs = (ATH12K_HW_RATE_CODE(ppdu->mcs, ppdu->nss,
			 ppdu->preamble_type, 0) & SA_MASK_RCODE);
	rx.npackets = ppdu->tcp_msdu_count +
			ppdu->udp_msdu_count + ppdu->other_msdu_count;
	for (i = 0; i < SA_MAX_RSSI_CHAINS; i++) {
		for (j = 0; j < SA_MAX_BW; j++)
			rx.rx_rssi[i][j] = ppdu->rssi_chain[i][j];
		if (!rx.rx_rssi[i][0])
			rx.rx_rssi[i][0] = ppdu->rssi_chain_pri20[i];
	}
	rx.rx_rate_index = ppdu->bw;
	rx_ctx.rx = &rx;

	if (!ath12k_arsta_itr_on_ab_by_addr(ab, stats->peer_mac,
					    sa_core_get_txrx_fb_ctx,
					    &rx_ctx)) {
		ath12k_err(ab, "Failed to notify rx feedback for %pM\n",
			   stats->peer_mac);
		/* STA not found; NOP */
		return 0;
	}

	if (rx_ctx.ret)
		return -1;

	if (!rx_ctx.status)
		return 0;

	ar = ath12k_mac_get_ar_by_pdev_id(ab, rx_ctx.pdev_id);
	if (!ar)
		return -1;

	if (rx_ctx.status & SA_RX_CONFIG_REQUIRED) {
		sa_core_wmi_set_rx_antenna(ab, ar->wmi,
					   rx_ctx.pdev_id, rx_ctx.value);
	} else if (rx_ctx.status & SA_TRAINING_REQUIRED) {
		sa_core_wmi_set_train_info(ab, ar->wmi, stats->peer_mac,
					   &rx_ctx.param);
	}

	return 0;
}

/* To be called only in sa_core_ppdu_event_notify() call cotext */
static int sa_core_get_tx_user_stats(struct ath12k_base *ab,
				     struct htt_ppdu_user_stats *stats)
{
	struct sa_txrx_fb_ctx tx_ctx = {0};
	struct ath12k_sa_tx_feedback tx = {0};
	struct ath12k *ar;
	u32 rate_flag;
	u8 i, chains, bw, mcs, preamble, nss;

	if (!is_valid_ether_addr(stats->peer_mac))
		return -1;

	tx.tx_antenna[0] = stats->cmpltn_cmn.tx_antenna_mask;
	chains = min_t(u32, SA_MAX_RSSI_CHAINS, HTT_STATS_MAX_CHAINS);
	for (i = 0; i < chains; i++)
		tx.rssi[i] = stats->cmpltn_cmn.rssi_chain[i];

	for (i = 0; i < HTT_STATS_MAX_SA_BW; i++)
		tx.ratemaxphy[i] = stats->cmpltn_cmn.max_rates[i];
	tx.ratemaxphy[4] = stats->cmpltn_cmn.max_rates_ext;

	tx.is_trainpkt = (stats->cmpltn_cmn.is_sa_training & 1);
	if (tx.is_trainpkt)
		tx.goodput = stats->cmpltn_cmn.pending_training_pkts;

	tx.num_packets = stats->cmpltn_cmn.mpdu_tried;
	tx.num_bad = stats->cmpltn_cmn.mpdu_tried -
			stats->cmpltn_cmn.mpdu_success;

	if (u32_get_bits(stats->rate.info1,
			 HTT_PPDU_STATS_USER_RATE_INFO1_RESP_TYPE_VALID))
		rate_flag = stats->rate.resp_rate_flags;
	else
		rate_flag = stats->rate.rate_flags;
	bw = HTT_USR_RATE_BW(rate_flag);
	if (bw == HTT_PPDU_STATS_BANDWIDTH_320MHZ) {
		bw -= 3;
	} else {
		if (bw < 2) {
			ath12k_err(ab, "Unexpected bandwidth value %u\n", bw);
			return -1;
		}
		bw -= 2;
	}
	if (bw >= SA_MAX_RETRIES_INDEX) {
		ath12k_err(ab, "rate_index %u out of range\n", bw);
		return -1;
	}
	tx.rate_index = bw;
	preamble = HTT_USR_RATE_PREAMBLE(rate_flag);
	mcs = HTT_USR_RATE_MCS(rate_flag);
	nss = HTT_USR_RATE_NSS(rate_flag);
	tx.rate_mcs[0] = (ATH12K_HW_RATE_CODE(mcs, nss, preamble, 0) &
			 SA_MASK_RCODE);

	for (i = 0; i < SA_MAX_RETRIES_INDEX; i++) {
		tx.nshort_retries[i] =
			(HTT_USR_CMPLTN_SHORT_RETRY(stats->cmpltn_cmn.flags) >>
			(i * SA_NIBBLE_BITS)) & SA_MASK_LOWER_NIBBLE;
		tx.nlong_retries[i] =
			(HTT_USR_CMPLTN_LONG_RETRY(stats->cmpltn_cmn.flags) >>
			(i * SA_NIBBLE_BITS)) & SA_MASK_LOWER_NIBBLE;
		if (tx.nlong_retries[i])
			if (tx.nlong_retries[i] == tx.nshort_retries[i])
				tx.nshort_retries[i]--;
	}
	if (tx.num_packets != tx.num_bad) {
		if (tx.nlong_retries[tx.rate_index])
			tx.nlong_retries[tx.rate_index] -= 1;
		if (tx.nshort_retries[tx.rate_index])
			tx.nshort_retries[tx.rate_index] -= 1;
	}
	tx_ctx.tx = &tx;

	if (!ath12k_arsta_itr_on_ab_by_addr(ab, stats->peer_mac,
					    sa_core_get_txrx_fb_ctx,
					    &tx_ctx)) {
		ath12k_err(ab, "Failed to notify tx feedback for %pM\n",
			   stats->peer_mac);
		/* STA not found; NOP */
		return 0;
	}

	if (tx_ctx.ret)
		return -1;

	if (!tx_ctx.status)
		return 0;

	ar = ath12k_mac_get_ar_by_pdev_id(ab, tx_ctx.pdev_id);
	if (!ar)
		return -1;

	if (tx_ctx.status & SA_RX_CONFIG_REQUIRED) {
		sa_core_wmi_set_rx_antenna(ab, ar->wmi,
					   tx_ctx.pdev_id, tx_ctx.value);
	} else if (tx_ctx.status & SA_TX_CONFIG_REQUIRED) {
		sa_core_wmi_set_tx_antenna(ab, ar->wmi, tx_ctx.vdev_id,
					   stats->peer_mac, tx_ctx.ant_array);
	} else if (tx_ctx.status & SA_TRAINING_REQUIRED) {
		sa_core_wmi_set_train_info(ab, ar->wmi, stats->peer_mac,
					   &tx_ctx.param);
	} else if (tx_ctx.status & SA_TX_FEEDBACK_CONFIG_REQUIRED) {
		sa_core_wmi_send_node_config_ops(ab, ar->wmi, tx_ctx.vdev_id,
						 stats->peer_mac, SA_TX_FEEDBACK_CONFIG,
						 1, &tx_ctx.value);
	}

	return 0;
}

/* To be called only from sa_core_ppdu_event_notify() */
static int sa_core_update_tx_feedback(struct ath12k_ppdu_tx_info *tx_info)
{
	struct ath12k_base *ab;
	struct htt_ppdu_stats_info *ppdu;
	int i;

	/* Quick guard: bail early if no ops registered */
	if (!rcu_access_pointer(g_sa_ops))
		return -1;

	if (tx_info->ppdu_info.frame_type != HTT_STATS_PPDU_FTYPE_DATA)
		return -1;

	ab = sa_core_get_ab_from_device_id(tx_info->device_id);
	if (!ab)
		return -1;

	if (!ath12k_smart_ant_is_enable(ab))
		return -1;

	ppdu = &tx_info->ppdu_info;
	for (i = 0; (i < ppdu->ppdu_stats.common.num_users) &&
	     (i < HTT_PPDU_STATS_MAX_USERS); i++)
		sa_core_get_tx_user_stats(ab, &ppdu->ppdu_stats.user_stats[i]);

	return 0;
}

/* This Linux notifier is call within RCU lock held. */
static int sa_core_ppdu_event_notify(struct notifier_block *nb,
                                     unsigned long action, void *data)
{
	struct ath12k_ppdu_event *event = data;
	struct sk_buff *skb;

	if (!event || !event->skb)
		return NOTIFY_DONE;

	skb = event->skb;
	if (!skb->data || !skb->len)
		return NOTIFY_DONE;

	switch (action) {
	case ATH12K_EVENT_PPDU_RX_COMPLETE:
		sa_core_update_rx_feedback((struct ath12k_ppdu_rx_info *)skb->data);
		return NOTIFY_OK;
	case ATH12K_EVENT_PPDU_TX_COMPLETE:
		sa_core_update_tx_feedback((struct ath12k_ppdu_tx_info *)skb->data);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int sa_core_ppdu_notifier_register(u8 en_mask)
{
	unsigned long mask = 0;
	int ret;

	if (atomic_cmpxchg(&sa_ppdu_nb_registered, 0, 1) != 0)
		return 0;

	sa_ppdu_nb.notifier_call = sa_core_ppdu_event_notify;
	sa_ppdu_nb.priority = 0;

	if (en_mask & SA_TX_FEEDBACK_MASK)
		mask |= (1UL << ATH12K_EVENT_PPDU_TX_COMPLETE);
	if (en_mask & SA_RX_FEEDBACK_MASK)
		mask |= (1UL << ATH12K_EVENT_PPDU_RX_COMPLETE);

	ret = ath12k_register_ppdu_notifier(&sa_ppdu_nb, mask);
	if (ret)
		atomic_set(&sa_ppdu_nb_registered, 0);

	return ret;
}

static int sa_core_ppdu_notifier_unregister(u8 en_mask)
{
	unsigned long mask = 0;
	int ret;

	if (atomic_read(&sa_ppdu_nb_registered) == 0)
		return 0;

	if (en_mask & SA_TX_FEEDBACK_MASK)
		mask |= (1UL << ATH12K_EVENT_PPDU_TX_COMPLETE);
	if (en_mask & SA_RX_FEEDBACK_MASK)
		mask |= (1UL << ATH12K_EVENT_PPDU_RX_COMPLETE);

	ret = ath12k_unregister_ppdu_notifier(&sa_ppdu_nb, mask);
	if (!ret)
		atomic_set(&sa_ppdu_nb_registered, 0);

	return ret;
}

static inline void sa_core_extract_mac_from_params(u32 *params, u8 *mac)
{
	mac[0] = (u8)((params[0] & 0x0000FF00) >> 8);
	mac[1] = (u8)(params[0] & 0x000000FF);
	mac[2] = (u8)((params[1] & 0xFF000000) >> 24);
	mac[3] = (u8)((params[1] & 0x00FF0000) >> 16);
	mac[4] = (u8)((params[1] & 0x0000FF00) >> 8);
	mac[5] = (u8)(params[1] & 0x000000FF);
}

int sa_core_get_configs(struct ath12k *ar, u32 *params)
{
	struct ath12k_smart_ant_ops *ops;
	struct ath12k_sa_config_params sa_params = {0};
	struct sa_sta_extn *sa_sta = NULL;
	struct ath12k_link_sta *arsta = NULL;
	u8 mac[ETH_ALEN] = {0};
	int ret = 0;

	if (!ar || !ar->ab || !params)
		return -EINVAL;

	if (!ath12k_smart_ant_is_enable(ar->ab))
		return -EPERM;
	/*
	 * Hold rcu_read_lock() for the entire operation so that the ops
	 * pointer and the function pointers it contains remain valid for
	 * the duration of the call into the algorithm module.
	 */
	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_get_param) {
		rcu_read_unlock();
		return -EINVAL;
	}

	sa_params.radio_id = ar->radio_idx;
	sa_params.param_type = (params[0] & SA_PARM_TYPE_MASK) >>
				SA_PARM_TYPE_SHIFT;
	sa_params.param_id = params[2];

	if (sa_params.param_type) {
		sa_core_extract_mac_from_params(params, mac);

		/* ath12k_link_sta_find_by_addr() requires rcu_read_lock(),
		 * which is already held above — no need to nest it again.
		 */
		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, mac);
		if (!arsta) {
			ath12k_err(ar->ab, "STA not found %pM\n", mac);
			spin_unlock_bh(&ar->arsta_lock);
			rcu_read_unlock();
			return -EPERM;
		}
		sa_sta = &arsta->arsta_extn.sa_sta;
		if (!sa_sta->ctx) {
			ath12k_err(ar->ab, "Invalid context for STA %pM\n", mac);
			spin_unlock_bh(&ar->arsta_lock);
			rcu_read_unlock();
			return -EPERM;
		}
		sa_params.ccp = sa_sta->ctx;
		spin_unlock_bh(&ar->arsta_lock);
	}
	ret = ops->sa_get_param(&sa_params);
	rcu_read_unlock();
	params[3] = sa_params.param_value;

	return ret;
}

int sa_core_set_configs(struct ath12k *ar, u32 *params)
{
	struct ath12k_smart_ant_ops *ops;
	struct ath12k_sa_config_params sa_params = {0};
	struct smart_ant_train_info t_param = {0};
	struct sa_sta_extn *sa_sta = NULL;
	struct ath12k_link_sta *arsta = NULL;
	u32 value = 0;
	u32 ant_array[SA_MAX_SA_CHAINS] = {0};
	u8 mac[ETH_ALEN] = {0};
	int ret = 0;
	u32 vdev_id = 0;

	if (!ar || !ar->ab || !params)
		return -EINVAL;

	if (!ath12k_smart_ant_is_enable(ar->ab))
		return -EPERM;

	/*
	 * Hold rcu_read_lock() for the entire operation so that the ops
	 * pointer and the function pointers it contains remain valid for
	 * the duration of the call into the algorithm module.
	 */
	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_set_param) {
		rcu_read_unlock();
		return -EINVAL;
	}

	sa_params.radio_id = ar->radio_idx;
	sa_params.param_type = (params[0] & SA_PARM_TYPE_MASK) >>
				SA_PARM_TYPE_SHIFT;
	sa_params.param_id = params[2];
	sa_params.param_value = params[3];

	if (sa_params.param_type) {
		sa_core_extract_mac_from_params(params, mac);

		/* ath12k_link_sta_find_by_addr() requires rcu_read_lock(),
		 * which is already held above — no need to nest it again.
		 */
		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, mac);
		if (!arsta) {
			ath12k_err(ar->ab, "STA not found %pM\n", mac);
			spin_unlock_bh(&ar->arsta_lock);
			rcu_read_unlock();
			return -EPERM;
		}
		sa_sta = &arsta->arsta_extn.sa_sta;
		if (!sa_sta->ctx) {
			ath12k_err(ar->ab, "Invalid context for STA %pM\n", mac);
			spin_unlock_bh(&ar->arsta_lock);
			rcu_read_unlock();
			return -EPERM;
		}
		sa_params.ccp = sa_sta->ctx;
		if (arsta->arvif)
			vdev_id = arsta->arvif->vdev_id;
		spin_unlock_bh(&ar->arsta_lock);
	}

	ret = ops->sa_set_param(&sa_params);
	rcu_read_unlock();
	if (ret > 0) {
		if (ret & SA_RX_CONFIG_REQUIRED) {
			sa_core_get_rxantenna(ar->radio_idx, &value);
			sa_core_wmi_set_rx_antenna(ar->ab, ar->wmi,
						   ar->pdev->pdev_id, value);
		} else if (ret & SA_DEFAULT_TX_CONFIG_REQUIRED) {
			sa_core_get_txdefaultantenna(ar->radio_idx, &value);
			ath12k_wmi_pdev_set_param(ar,
				WMI_PDEV_PARAM_SMART_ANTENNA_DEFAULT_ANTENNA,
				value, ar->pdev->pdev_id);
		} else if (ret & SA_BEACON_ANT_CONFIG_REQUIRED) {
			sa_core_get_bcn_txantenna(ar->radio_idx, &value);
			ath12k_wmi_pdev_set_param(ar,
				WMI_PDEV_PARAM_SMART_ANTENNA_BEACON_ANTENNA,
				value, ar->pdev->pdev_id);
		} else if (ret & SA_PARALLEL_GPIO_CFG_REQUIRED) {
			sa_core_wmi_send_parallel_mode_gpio_drive(ar,
							sa_params.param_value);
		} else if ((ret & SA_TX_CONFIG_REQUIRED) && sa_sta) {
			sa_core_get_txantenna(sa_sta->ctx, &ant_array[0]);
			sa_core_wmi_set_tx_antenna(ar->ab, ar->wmi,
						   vdev_id, mac, ant_array);
		} else if ((ret & SA_TRAINING_REQUIRED) && sa_sta) {
			if (!sa_core_get_training_info(sa_sta->ctx, &t_param,
						       vdev_id))
				sa_core_wmi_set_train_info(ar->ab, ar->wmi, mac,
							   &t_param);
		} else if ((ret & SA_TX_FEEDBACK_CONFIG_REQUIRED) && sa_sta) {
			sa_core_get_node_config(sa_sta->ctx,
						SA_TX_FEEDBACK_CONFIG, &value);
			sa_core_wmi_send_node_config_ops(ar->ab, ar->wmi,
							 vdev_id, mac,
							 SA_TX_FEEDBACK_CONFIG,
							 1, &value);
		} else if (ret & SA_MPDU_CNT_MCS_MRG_REQUIRED) {
			u32 mpdu_cnt = 0;
			u32 mcs_marg = 0;
			sa_core_get_max_mpdu_count_for_training(ar->radio_idx,
								&mpdu_cnt);
			sa_core_get_mcs_margin(ar->radio_idx, &mcs_marg);
			value = (mpdu_cnt & SA_MASK_MAX_MPDU_CNT) |
				((mcs_marg & SA_MASK_MCS_MARGIN) << SA_MCS_MARGIN_POS);
			ath12k_wmi_pdev_set_param(ar,
				WMI_PDEV_PARAM_SMART_ANTENNA_CONFIG_MPDU_MCS_PARAMS,
				value, ar->pdev->pdev_id);
		}
	}

	return 0;
}

int sa_core_init(struct ath12k *ar,
		 struct ath12k_link_vif *arvif, u8 init_flags)
{
	struct ath12k_smart_ant_ops *ops;
	struct sa_base_extn *sa_base;
	struct sa_ar_extn *sa_ar;
	struct ath12k_base *ab;
	struct ath12k_vif *ahvif;
	struct ath12k_sa_config init_cfg = {0};
	struct ieee80211_channel *chan;
	struct ath12k_sa_gpio_config gpio_cfg = {0};
	int sa_mode;
	u32 rx_antenna, tx_defaultant, en_rts;
	u32 tx_ant[WMI_SMART_ANT_MAX_RATE_SERIES] = {0};
	u8 i;
	int ret;

	if (!ar || !arvif || !ar->ab)
		return -1;

	/* Quick guard: bail early if no ops registered */
	if (!rcu_access_pointer(g_sa_ops))
		return -1;

	ab = ar->ab;
	sa_base = &ab->ath12k_base_extn.sa_base;
	if (!sa_base->sa_enable)
		return -1;

	sa_ar = &ar->ar_extn.sa_ar;
	if (init_flags & SA_NEW_CONFIG) {
		if (sa_ar->state == SA_INIT_DONE_STATE)
			return 0;
	}

	ath12k_dbg(ab, ATH12K_DBG_ANY,
		   "SA Core Init Called with init_flag = %u\n", init_flags);
	ahvif = arvif->ahvif;
	if ((ahvif->vdev_type != WMI_VDEV_TYPE_AP) &&
	    (ahvif->vdev_type != WMI_VDEV_TYPE_STA)) {
		return -1;
	}

	init_cfg.radio_id = ar->radio_idx;
	init_cfg.nss = ar->num_tx_chains;
	init_cfg.bss_mode = (ahvif->vdev_type == WMI_VDEV_TYPE_AP)?
			    SA_BSS_MODE_AP: SA_BSS_MODE_STA;
	init_cfg.txrx_chainmask =
		(ar->cfg_tx_chainmask | (ar->cfg_rx_chainmask << 4));
	init_cfg.txrx_feedback = SA_TX_FEEDBACK_MASK | SA_RX_FEEDBACK_MASK;
	chan = arvif->chanctx.def.chan;
	if (chan) {
		init_cfg.band = chan->band;
		init_cfg.chan_num = chan->hw_value;
	}

	/*
	 * Re-check ops under RCU before calling into the algorithm module.
	 * rmmod sa_test may have raced between the guard above and here.
	 */
	rcu_read_lock();
	ops = rcu_dereference(g_sa_ops);
	if (!ops || !ops->sa_algo_init) {
		rcu_read_unlock();
		return -1;
	}
	sa_mode = ops->sa_algo_init(&init_cfg, (init_flags & SA_NEW_CONFIG));
	rcu_read_unlock();

	if (sa_mode < 0) {
		ath12k_err(ab, "SA Init error (%d)\n", sa_mode);
		return -1;
	}
	if (sa_base->sa_mode_overwrite != -1) {
		sa_mode = sa_base->sa_mode_overwrite;
		ath12k_dbg(ab, ATH12K_DBG_ANY,
			   "SA Mode Overwritten to %d\n", sa_mode);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_ANY, "SA Mode %d\n", sa_mode);
	}
	sa_ar->sa_mode = sa_mode;

	sa_core_get_txdefaultantenna(ar->radio_idx, &tx_defaultant);
	sa_core_get_rxantenna(ar->radio_idx, &rx_antenna);
	/* Create smart antenna enabled and Tx/Rx feedback subscription state
	 * bitmap. bit 0 for smart antenna enabled/disabled, bit 4 for TX
	 * subscription state and bit 5 for RX subscription state.
	 */
	sa_ar->enable_mask = SA_ENABLE_MASK | (init_cfg.txrx_feedback &
			(SA_TX_FEEDBACK_MASK | SA_RX_FEEDBACK_MASK));
	if ((init_flags & SA_NEW_CONFIG) || (init_flags & SA_SSR_CONFIG)) {
		sa_ar->state = SA_INIT_DONE_STATE;
		sa_core_get_gpio_config(ar->radio_idx, &gpio_cfg);
		sa_core_get_rts_ant_flag(ar->radio_idx, &en_rts);
		if (!sa_base->sa_en_rts_overwrite ||
		    sa_base->sa_en_rts_overwrite == 1) {
			en_rts = sa_base->sa_en_rts_overwrite;
			ath12k_dbg(ab, ATH12K_DBG_ANY,
				   "SA Overwritten RTS separate antenna enable to %d\n",
				   en_rts);
		}
		ret = sa_core_wmi_send_smart_ant_enable(ar, tx_defaultant,
							rx_antenna, &gpio_cfg,
							en_rts);
		if (ret) {
			ath12k_err(ab, "SA enable command failed for 0x%x\n",
				   sa_ar->enable_mask);
			return ret;
		}
		if (sa_ar->sa_mode == WMI_SMART_ANT_MODE_PARALLEL)
			sa_core_wmi_send_parallel_mode_gpio_drive(ar, ar->cfg_tx_chainmask);
		/*
		 * Cache ag before registering the PPDU notifier so that the
		 * atomic notifier callbacks can resolve device_id -> ab without
		 * taking the sleepable ath12k_hw_group_mutex.
		 */
		rcu_assign_pointer(sa_cached_ag, ath12k_ab_to_ag(ab));
		synchronize_rcu();
		ret = sa_core_ppdu_notifier_register(sa_ar->enable_mask);
		if (ret)
			ath12k_warn(ab, "SA PPDU registration failed:%d\n",
				    ret);
	} else {
		sa_core_wmi_set_rx_antenna(ab, ar->wmi, ar->pdev->pdev_id,
					   rx_antenna);
		ath12k_wmi_pdev_set_param(ar,
			WMI_PDEV_PARAM_SMART_ANTENNA_DEFAULT_ANTENNA,
			tx_defaultant, ar->pdev->pdev_id);
	}

	for (i = 0; i < WMI_SMART_ANT_MAX_RATE_SERIES; i++)
		tx_ant[i] = tx_defaultant;

	sa_core_wmi_set_tx_antenna(ab, ar->wmi, arvif->vdev_id,
				   ahvif->vif->addr, tx_ant);

	return 0;
}

/* Called from process context with wiphy lock held. */
static void sa_core_peer_reconnect_iter(struct ath12k *ar,
					struct ath12k_link_sta *arsta,
					void *data)
{
	struct sa_sta_extn *sa_sta = &arsta->arsta_extn.sa_sta;

	/*
	 * Only re-notify peers that already have a valid SA context.
	 * Peers that never completed the ratecode-list handshake have
	 * ctx == NULL and cannot be re-notified here.
	 */
	if (!sa_sta->ctx)
		return;

	/* Reuse the existing peer-connect path. rate_cap is not available
	 * from the WMI event here, so pass NULL — sa_core_peer_connect_notify
	 * skips the memcpy when data is NULL and the algorithm module retains
	 * its cached rate capabilities from the initial connect.
	 */
	rcu_read_lock();
	sa_core_peer_connect_notify(ar, arsta, NULL);
	rcu_read_unlock();
}

void sa_core_notify_all_peers_connected(struct ath12k *ar)
{
	u32 bkt;
	struct ath12k_link_sta *arsta;
	struct hlist_node *tmp;

	if (!ar || !ath12k_smart_ant_is_enable(ar->ab))
		return;

	if (!rcu_access_pointer(g_sa_ops))
		return;

	spin_lock_bh(&ar->arsta_lock);
	if (!ar->arsta_list) {
		spin_unlock_bh(&ar->arsta_lock);
		return;
	}
	for (bkt = 0; bkt < BIT(ar->arsta_hash_bits); bkt++) {
		hlist_for_each_entry_safe(arsta, tmp, &ar->arsta_list[bkt],
					  hlist_addr)
			sa_core_peer_reconnect_iter(ar, arsta, NULL);
	}
	spin_unlock_bh(&ar->arsta_lock);
}

int sa_core_deinit(struct ath12k *ar, u8 init_flags)
{
	struct ath12k_smart_ant_ops *ops;
	struct sa_base_extn *sa_base;
	struct sa_ar_extn *sa_ar;
	struct ath12k_base *ab;
	struct ath12k_sa_gpio_config gpio_cfg = {0};
	int ret;

	if (!ar || !ar->ab)
		return -1;

	/* Quick guard: bail early if no ops registered */
	if (!rcu_access_pointer(g_sa_ops))
		return -1;

	ab = ar->ab;
	sa_base = &ab->ath12k_base_extn.sa_base;
	if (!sa_base->sa_enable)
		return -1;

	sa_ar = &ar->ar_extn.sa_ar;
	if (!(sa_ar->enable_mask & SA_ENABLE_MASK))
		return -1;

	if (sa_ar->state == SA_DEINIT_DONE_STATE) {
		ath12k_dbg(ab, ATH12K_DBG_ANY, "SA Core Deinit already done!\n");
		return 0;
	}
	if (init_flags & SA_NEW_CONFIG) {
		/* Call deinit under RCU so the module cannot be unloaded
		 * while we are executing inside it.
		 */
		rcu_read_lock();
		ops = rcu_dereference(g_sa_ops);
		if (ops && ops->sa_algo_deinit)
			ops->sa_algo_deinit(ar->radio_idx);
		rcu_read_unlock();

		/*
		 * Unregister the notifier first, then clear the cached
		 * ag pointer.  This ordering ensures that no new atomic
		 * callbacks can fire after the pointer is nulled.
		 */
		ret = sa_core_ppdu_notifier_unregister(sa_ar->enable_mask);
		if (ret)
			ath12k_warn(ab, "SA PPDU unregistration failed:%d\n",
				    ret);
		rcu_assign_pointer(sa_cached_ag, NULL);
		synchronize_rcu();
		sa_ar->enable_mask = 0;
		ret = sa_core_wmi_send_smart_ant_enable(ar, 0, 0, &gpio_cfg, 0);
		if (ret)
			ath12k_err(ab, "SA disable failed %d\n", ret);
		if (sa_ar->sa_mode == WMI_SMART_ANT_MODE_PARALLEL)
			sa_core_wmi_send_parallel_mode_gpio_drive(ar, 0);

		sa_ar->state = SA_DEINIT_DONE_STATE;
	}

	return 0;
}
