/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: contains mlo manager structure definitions
 */
#ifndef __MLO_MGR_PUBLIC_STRUCTS_H
#define __MLO_MGR_PUBLIC_STRUCTS_H

#include <wlan_objmgr_cmn.h>
#include <qdf_list.h>
#include <qdf_atomic.h>
#include <qdf_nbuf.h>
#include <qdf_event.h>
#include <wlan_cmn_ieee80211.h>
#include <wlan_cmn.h>
#include <wlan_objmgr_global_obj.h>
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
#include <qdf_event.h>
#endif
#include <wlan_mlo_t2lm.h>
#include <net/cfg80211.h>
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
#include "cfg_mlme_generic.h"
#endif
#include <wlan_crypto_global_def.h>

/* MAX MLO dev support */
#ifndef WLAN_UMAC_MLO_MAX_VDEVS
#define WLAN_UMAC_MLO_MAX_VDEVS 2
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
/* Max bridge vdevs supported */
#define WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS 2
/* Max number of PSOC taking part in topology decision at a time*/
#define WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY 3
#endif

#include <wlan_mlo_epcs.h>

/* MAX instances of ML devices */
#ifndef WLAN_UMAC_MLO_MAX_DEV
#define WLAN_UMAC_MLO_MAX_DEV 2
#endif

/* MAX MLO Assoc Links per MLD */
#ifndef WLAN_UMAC_MLO_ASSOC_MAX_SUPPORTED_LINKS
#if defined(SAP_MULTI_LINK_EMULATION) || defined(WLAN_FEATURE_MULTI_LINK_SAP)
#define WLAN_UMAC_MLO_ASSOC_MAX_SUPPORTED_LINKS 2
#else
#define WLAN_UMAC_MLO_ASSOC_MAX_SUPPORTED_LINKS 1
#endif
#endif


/* Default Initialization value for Max Recommended Simultaneous Links */
#ifndef WLAN_UMAC_MLO_RECOM_MAX_SIMULT_LINKS_DEFAULT
#define WLAN_UMAC_MLO_RECOM_MAX_SIMULT_LINKS_DEFAULT 2
#endif

/* Default initialization for RMSL Advertisement */
#ifndef WLAN_UMAC_MLO_EXTMLDCAP_ENABLE_ADVERTISEMENT
#define WLAN_UMAC_MLO_EXTMLDCAP_ENABLE_ADVERTISEMENT 1
#endif

/* Max PEER support */
#define MAX_MLO_PEER 512

struct mlo_mlme_ext_ops;
struct mlo_osif_ext_ops;
struct vdev_mlme_obj;
struct wlan_t2lm_context;
struct mlo_link_switch_context;
struct wlan_mlo_link_switch_req;
struct mlo_all_link_rssi;
struct ptqm_migrate_link_context;
struct ptqm_migrate_peer_context;

/* Max LINK PEER support */
#define MAX_MLO_LINK_PEERS WLAN_UMAC_MLO_MAX_VDEVS

/* MAX MLO peer_id supported by FW is 1024 */
#define MAX_MLO_PEER_ID 1024
#define MLO_INVALID_PEER_ID 0xFFFF

/* IE nomenclature */
#define ID_POS 0
#define TAG_LEN_POS 1
#define IDEXT_POS 2
#define MIN_IE_LEN 2
#define MIN_VENDOR_IE_LEN 4
#define VENDOR_OUI_POS 2
#define MULTI_LINK_CTRL_1 3
#define MULTI_LINK_CTRL_2 4
#define STA_CTRL_1 2
#define STA_CTRL_2 3
#define STA_PROFILE_SUB_ELEM_ID 0
#define PER_STA_PROF_MAC_ADDR_START 4

/* MLO link id max value */
#define MAX_MLO_LINK_ID 15

#ifdef WLAN_MLO_MULTI_CHIP

#ifndef WLAN_MAX_MLO_GROUPS
#define WLAN_MAX_MLO_GROUPS 2
#endif

/**
 * enum MLO_LINK_STATE - MLO link state enums
 * @MLO_LINK_SETUP_INIT: MLO link SETUP exchange not yet done
 * @MLO_LINK_SETUP_DONE: MLO link SETUP exchange started
 * @MLO_LINK_READY: MLO link SETUP done and READY sent
 * @MLO_LINK_TEARDOWN: MLO teardown done.
 * @MLO_LINK_UNINITIALIZED: MLO link in blank state
 */
enum MLO_LINK_STATE {
	MLO_LINK_SETUP_INIT,
	MLO_LINK_SETUP_DONE,
	MLO_LINK_READY,
	MLO_LINK_TEARDOWN,
	MLO_LINK_UNINITIALIZED,
};

/**
 * enum MLO_SOC_LIST - MLO SOC LIST
 * @WLAN_MLO_GROUP_DEFAULT_SOC_LIST:  All MLO SoCs that are part of this MLO
 *                                    group, (inclusive of both setup sequence
 *                                    completed, not yet completed)
 * @WLAN_MLO_GROUP_CURRENT_SOC_LIST:  Current MLO SoCs that are probed for which
 *                                    the setup sequence has been completed
 */
enum MLO_SOC_LIST {
	WLAN_MLO_GROUP_DEFAULT_SOC_LIST,
	WLAN_MLO_GROUP_CURRENT_SOC_LIST,
};

/*
 * Maximum number of MLO LINKS across the system,
 * this is not the MLO links within and AP-MLD.
 */

#define MAX_MLO_LINKS 6
#define MAX_MLO_CHIPS 5
#define MAX_ADJ_CHIPS 2

/* MLO Bridge link */
#define MLO_NUM_CHIPS_FOR_BRIDGE_LINK 4
#define MLO_MAX_BRIDGE_LINKS_PER_MLD 2
#define MLO_MAX_BRIDGE_LINKS_PER_RADIO WLAN_MAX_PDEV_BRIDGE_VDEVS

/**
 * struct mlo_chip_info: MLO chip info per link
 * @info_valid: If the info here is valid or not
 * @chip_id: Chip ID as assigned by platform
 * @adj_chip_ids: Chip IDs of Adjacent chips
 */
struct mlo_chip_info {
	uint8_t info_valid;
	uint8_t chip_id[MAX_MLO_CHIPS];
	uint8_t adj_chip_ids[MAX_MLO_CHIPS][MAX_ADJ_CHIPS];
};

#define START_STOP_INPROGRESS_BIT 0

/**
 * struct mlo_setup_info: MLO setup status per link
 * @ml_grp_id: Unique id for ML grouping of Pdevs/links
 * @tot_socs: Total number of soc participating in ML group
 * @num_soc: Number of soc ready or probed
 * @tot_links: Total links in ML group
 * @num_links: Number of links probed in ML group
 * @pdev_list: current pdev pointers belonging to this group
 * @curr_soc_list: current psoc pointers belonging to this group
 * @soc_list: Actual psoc pointers part of this group
 * @soc_id_list: list of soc ids part of this mlo group
 * @state: MLO link state
 * @valid_link_bitmap: valid MLO link bitmap
 * @trigger_umac_reset: teardown require umac reset, for mode1 SSR
 * @state_lock: lock to protect access to link state
 * @event: event for teardown completion
 * @start_stop_inprogress: MLO group start/stop in progress
 * @dp_handle: pointer to DP ML context
 * @chip_info: chip specific info of the soc
 * @tsf_sync_enabled: MLO TSF sync is enabled at FW or not
 * @wsi_stats_info_support: WSI stats support at FW or not
 * @wsi_remap_in_progress: Dynamic WSI remap in progress
 * @wsi_remap_support: WSI remap support at FW or not
 * @wsi_remap_netdev_preserve: Preserve netdev during wsi remap
 */
struct mlo_setup_info {
	uint8_t ml_grp_id;
	uint8_t tot_socs;
	uint8_t num_soc;
	uint8_t tot_links;
	uint8_t num_links;
	struct wlan_objmgr_pdev *pdev_list[MAX_MLO_LINKS];
	struct wlan_objmgr_psoc *curr_soc_list[MAX_MLO_CHIPS];
	struct wlan_objmgr_psoc *soc_list[MAX_MLO_CHIPS];
	uint8_t soc_id_list[MAX_MLO_CHIPS];
	enum MLO_LINK_STATE state[MAX_MLO_LINKS];
	uint16_t valid_link_bitmap;
	bool trigger_umac_reset;
	qdf_spinlock_t state_lock;
	qdf_event_t event;
	unsigned long start_stop_inprogress;
	struct cdp_mlo_ctxt *dp_handle;
	struct mlo_chip_info chip_info;
	bool tsf_sync_enabled;
	uint8_t wsi_stats_info_support;
	bool wsi_remap_in_progress;
	uint8_t wsi_remap_support;
	bool wsi_remap_netdev_preserve;
};

/**
 * struct mlo_state_params: MLO state params for pdev iteration
 * @link_state_fail: Flag to check when pdev not in expected state
 * @check_state: State on against which pdev is to be expected
 * @grp_id: Id of the required MLO Group
 */
struct mlo_state_params {
	bool link_state_fail;
	enum MLO_LINK_STATE check_state;
	uint8_t grp_id;
};

#endif

/**
 * enum mlo_link_rej_cfg - Link rejection config
 * @MLO_LINK_REJ_DISABLED: Disabled
 * @MLO_LINK_REJ_RANGE: Range config enabled
 * @MLO_LINK_REJ_THROUGHPUT: Throughput config enabled
 * @MLO_LINK_REJ_CFG_MAX: Max limit for config
 */
enum mlo_link_rej_cfg {
	MLO_LINK_REJ_DISABLED,
	MLO_LINK_REJ_RANGE,
	MLO_LINK_REJ_THROUGHPUT,
	MLO_LINK_REJ_CFG_MAX,
};

/**
 * enum wlan_mlo_link_switch_notify_reason - Enum for link switch notifier
 *                                           callback trigger reason.
 * @MLO_LINK_SWITCH_NOTIFY_REASON_PRE_START_PRE_SER: Prior to start of
 *                                                   link switch and prior to
 *                                                   serializing link switch.
 * @MLO_LINK_SWITCH_NOTIFY_REASON_PRE_START_POST_SER: Prior to link switch start
 *                                                    but link switch is
 *                                                    serialized
 * @MLO_LINK_SWITCH_NOTIFY_REASON_STOP_FAILURE: Link switch failure notify
 * @MLO_LINK_SWITCH_NOTIFY_REASON_STOP_SUCCESS: Link switch success notify
 */
enum wlan_mlo_link_switch_notify_reason {
	MLO_LINK_SWITCH_NOTIFY_REASON_PRE_START_PRE_SER,
	MLO_LINK_SWITCH_NOTIFY_REASON_PRE_START_POST_SER,
	MLO_LINK_SWITCH_NOTIFY_REASON_STOP_FAILURE,
	MLO_LINK_SWITCH_NOTIFY_REASON_STOP_SUCCESS,
};

typedef QDF_STATUS
(*mlo_mgr_link_switch_notifier_cb)(struct wlan_objmgr_vdev *vdev,
				   struct wlan_mlo_link_switch_req *lswitch_req,
				   enum wlan_mlo_link_switch_notify_reason notify_reason);

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/*
 * struct wlan_mlo_link_switch_notifier - Link switch notifier callbacks
 * @in_use: Set to true on successful notifier callback registration
 * @cb: Callback to notify link switch start
 */
struct wlan_mlo_link_switch_notifier {
	bool in_use;
	mlo_mgr_link_switch_notifier_cb cb;
};

/**
 * mlo_mgr_register_link_switch_notifier() - API to register link switch
 * start notifier callback
 * @comp_id: Component requesting notification on link switch start
 * @cb: Callback to register.
 *
 * The @cb will be triggered on start of link switch with params of the
 * link switch.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_mgr_register_link_switch_notifier(enum wlan_umac_comp_id comp_id,
				      mlo_mgr_link_switch_notifier_cb cb);

/**
 * mlo_mgr_unregister_link_switch_notifier() - API to unregister link switch
 * notifier callback.
 * @comp_id: Component to deregister.
 *
 * The API will cleanup the notification callback registered for link switch.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_mgr_unregister_link_switch_notifier(enum wlan_umac_comp_id comp_id);
#else
static inline QDF_STATUS
mlo_mgr_register_link_switch_notifier(enum wlan_umac_comp_id comp_id,
				      mlo_mgr_link_switch_notifier_cb cb)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_unregister_link_switch_notifier(enum wlan_umac_comp_id comp_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */

/*
 * struct mlo_wsi_link_stats - MLO ingress/egress counters of PSOC
 * @ingress_cnt:  Ingress counter
 * @egress_cnt:  Egress counter
 * @send_wmi_cmd: To indicate whether WMI command to be sent or not
 */
struct mlo_wsi_link_stats {
	uint32_t ingress_cnt;
	uint32_t egress_cnt;
	bool  send_wmi_cmd;
};

/*
 * struct mlo_wsi_psoc_grp - MLO WSI PSOC group
 * @psoc_order:  PSOC list in WSI loop order
 * @num_psoc: num psoc in the group
 */
struct mlo_wsi_psoc_grp {
	uint32_t psoc_order[WLAN_OBJMGR_MAX_DEVICES];
	uint32_t num_psoc;
};

#define MLO_WSI_MAX_MLO_GRPS 2
#define MLO_WSI_PSOC_ID_MAX 0xFF

/*
 * struct mlo_wsi_info - MLO ingress/egress link context per-PSOC
 * @mlo_psoc_grp: PSOC IDs for different MLO groups
 * @num_psoc: Total num psoc
 * @link_stats: Ingress and Egress counts for PSOCs
 * @block_wmi_cmd: Blocks WMI command
 */
struct mlo_wsi_info {
	struct mlo_wsi_psoc_grp mlo_psoc_grp[MLO_WSI_MAX_MLO_GRPS];
	uint32_t num_psoc;
	struct mlo_wsi_link_stats link_stats[WLAN_OBJMGR_MAX_DEVICES];
	uint8_t block_wmi_cmd;
};

/**
 * struct mlo_mgr_context - MLO manager context
 * @ml_dev_list_lock: ML DEV list lock
 * @aid_lock: AID global lock
 * @ml_peerid_lock: ML peer ID global lock
 * @ml_dev_list: Array of MLO device context
 * @mlo_peer_id_bmap: bitmap to allocate MLO Peer ID
 * @max_mlo_peer_id: Max MLO Peer ID
 * @last_mlo_peer_id: Previously allocated ML peer ID
 * @max_ml_peer_count: Max MLO peer count
 * @ml_peer_count: ML peer count
 * @setup_info: Pointer to MLO setup_info of all groups
 * @total_grp: Total number of MLO groups
 * @dynamic_wsi_bypassed: Dynamic bypassed performed
 * @mode1_recovery_in_progress: Flag to indicate MODE1 in progress
 * @tsf_sync_enabled: MLO TSF sync is enabled at FW or not
 * @mlme_ops: MLO MLME callback function pointers
 * @osif_ops: MLO to OSIF callback function pointers
 * @msgq_ctx: Context switch mgr
 * @mlo_is_force_primary_umac: Force Primary UMAC enable
 * @mlo_forced_primary_umac_id: Force Primary UMAC ID
 * @force_non_assoc_prim_umac: Force non-assoc link to be primary umac
 * @lswitch_notifier: Link switch notifier callbacks
 * @wsi_info: WSI stats info
 * @disable_eml: Disable Enhanced Multi Link features(eMLSR and eMLMR).
 */
struct mlo_mgr_context {
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t ml_dev_list_lock;
	qdf_spinlock_t aid_lock;
	qdf_spinlock_t ml_peerid_lock;
#else
	qdf_mutex_t ml_dev_list_lock;
	qdf_mutex_t aid_lock;
	qdf_mutex_t ml_peerid_lock;
#endif
	qdf_list_t ml_dev_list;
	qdf_bitmap(mlo_peer_id_bmap, MAX_MLO_PEER_ID);
	uint16_t max_mlo_peer_id;
	uint16_t last_mlo_peer_id;
	uint16_t max_ml_peer_count;
	qdf_atomic_t ml_peer_count;
#ifdef WLAN_MLO_MULTI_CHIP
	struct mlo_setup_info *setup_info;
	uint8_t total_grp;
	bool dynamic_wsi_bypassed;
	bool mode1_recovery_in_progress;
#elif defined(WLAN_FEATURE_11BE_MLO) && !defined(WLAN_MLO_MULTI_CHIP)
	bool tsf_sync_enabled;
#endif
	struct mlo_mlme_ext_ops *mlme_ops;
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
	struct mlo_osif_ext_ops *osif_ops;
#endif
	struct ctxt_switch_mgr *msgq_ctx;
	bool mlo_is_force_primary_umac;
	bool mlo_override_mlsr_ptqm;
	uint8_t mlo_forced_primary_umac_id;
	bool force_non_assoc_prim_umac;
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
	struct wlan_mlo_link_switch_notifier lswitch_notifier[WLAN_UMAC_COMP_ID_MAX];
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */
	struct mlo_wsi_info *wsi_info;
	bool disable_eml;
};

/**
 * struct wlan_ml_vdev_aid_mgr - ML AID manager
 * @aid_bitmap: AID bitmap array
 * @start_aid: start of AID index
 * @max_aid: Max allowed AID
 * @aid_mgr:  Array of link vdev aid mgr
 */
struct wlan_ml_vdev_aid_mgr {
	qdf_bitmap(aid_bitmap, WLAN_UMAC_MAX_AID);
	uint16_t start_aid;
	uint16_t max_aid;
	struct wlan_vdev_aid_mgr *aid_mgr[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * struct wlan_mlo_key_mgmt - MLO key management
 * @keys_saved: keys saved bool
 * @link_id: link id
 */
struct wlan_mlo_key_mgmt {
	bool keys_saved;
	uint8_t link_id;
};

/**
 * struct mlo_link_bss_params - link bss param
 * @link_id: link id
 * @ap_mld_mac: mld mac address
 * @chan: channel
 */
struct mlo_link_bss_params {
	int8_t link_id;
	int8_t ap_mld_mac[QDF_MAC_ADDR_SIZE];
	struct wlan_channel *chan;
};

#ifdef WLAN_FEATURE_11BE_MLO

/**
 * enum mlo_link_info_event_status - link info event status
 * @WLAN_LINK_INFO_EVENT_SUCCESS: success
 * @WLAN_LINK_INFO_EVENT_REJECT_FAILURE: reject due to common failure reason
 * @WLAN_LINK_INFO_EVENT_REJECT_VDEV_NOT_UP: reject as vdev is not up
 * @WLAN_LINK_INFO_EVENT_REJECT_ROAMING_IN_PROGRESS: reject as roaming
 *						     is in progress
 * @WLAN_LINK_INFO_EVENT_REJECT_NON_MLO_CONNECTION: reject as it's not
 *						    MLO connection
 */
enum mlo_link_info_event_status {
	WLAN_LINK_INFO_EVENT_SUCCESS,
	WLAN_LINK_INFO_EVENT_REJECT_FAILURE,
	WLAN_LINK_INFO_EVENT_REJECT_VDEV_NOT_UP,
	WLAN_LINK_INFO_EVENT_REJECT_ROAMING_IN_PROGRESS,
	WLAN_LINK_INFO_EVENT_REJECT_NON_MLO_CONNECTION,
};

/**
 * struct mlo_link_state_cmd_params - MLO link state params
 * @vdev_id: Vdev id
 * @mld_mac: mld mac address
 */
struct mlo_link_state_cmd_params {
	uint8_t vdev_id;
	uint8_t mld_mac[QDF_MAC_ADDR_SIZE];
};

/**
 * struct ml_link_info - ml link information
 * @vdev_id: vdev id for this link
 * @link_id: link id defined as in 802.11 BE spec.
 * @link_status: inactive 0, active 1
 * @reserved: reserved bits
 * @chan_freq: Channel frequency in MHz
 */
struct ml_link_info {
	uint32_t vdev_id:8,
		 link_id:8,
		 link_status:2,
		 reserved:14;
	uint32_t chan_freq;
};

/**
 * struct ml_link_state_info_event - ML link state info response
 * @status: to indicate the status for ml link info
 * @hw_mode_index: current hardware mode index
 * @link_info: link information
 * @num_mlo_vdev_link_info: number of mlo vdev link info
 * @vdev_id: vdev_id
 * @mldaddr: mld addr
 */
struct ml_link_state_info_event {
	uint32_t status;
	uint32_t hw_mode_index;
	struct ml_link_info link_info[WLAN_MAX_ML_BSS_LINKS];
	uint16_t num_mlo_vdev_link_info;
	uint8_t vdev_id;
	struct qdf_mac_addr mldaddr;
};

/**
 * struct ml_link_state_cmd_info - ml link state command info
 * @request_cookie: request cookie
 * @ml_link_state_resp_cb: callback function to handle response
 * @ml_link_state_req_context: request context
 */
struct ml_link_state_cmd_info {
	void *request_cookie;
	void (*ml_link_state_resp_cb)(struct ml_link_state_info_event *ev,
				      void *cookie);
	void *ml_link_state_req_context;
};
#endif

#ifdef WLAN_FEATURE_11BE_MLO_TTLM
/**
 * struct ttlm_comp_priv - TTLM completion private info
 * @ml_peer: ML Peer objmgr
 * @dialog_token: TTLM Dialog token
 * @status: TTLM command status
 */
struct ttlm_comp_priv {
	struct wlan_mlo_peer_context *ml_peer;
	uint8_t dialog_token;
	QDF_STATUS status;
};

/**
 * struct ttlm_rsp_info - TTLM response frame info
 * @t2lm_info: TTLM mapping
 * @t2lm_resp_type: TTLM status corresponds to TTL response frame
 * @token: TTLM request dialog token
 * @dest_addr: Action resp destination address
 */
struct ttlm_rsp_info {
	struct wlan_t2lm_info *t2lm_info;
	enum wlan_t2lm_resp_frm_type t2lm_resp_type;
	uint8_t token;
	struct qdf_mac_addr dest_addr;
};

/**
 * struct ttlm_send_cmd_info - TTLM send command info
 * @cookie: request cookie
 * @ttlm_send_cmd_resp_cb: Callback function to handle response
 * @context: request cookie
 */
struct ttlm_send_cmd_info {
	void *cookie;
	void (*ttlm_send_cmd_resp_cb)(struct ttlm_comp_priv *ev, void *cookie);
	void *context;
};
#endif
/**
 * struct mlo_sta_csa_params - CSA request parameters in mlo mgr
 * @csa_param: csa parameters
 * @link_id: the link index of AP which triggers CSA
 * @mlo_csa_synced: Before vdev is up, csa information is only saved but not
 *                  handled, and this value is false. Once vdev is up, the saved
 *                  csa information is handled, and this value is changed to
 *                  true. Note this value will be true if the vdev is doing
 *                  restart.
 * @csa_offload_event_recvd: True if WMI_CSA_HANDLING_EVENTID is already
 *                           received. False if this is the first
 *                           WMI_CSA_HANDLING_EVENTID.
 * @valid_csa_param: True once csa_param is filled.
 */
struct mlo_sta_csa_params {
	struct csa_offload_params csa_param;
	uint8_t link_id;
	bool mlo_csa_synced;
	bool csa_offload_event_recvd;
	bool valid_csa_param;
};

/**
 * struct mlo_sta_cu_params - critical update parameters in mlo mgr
 * @vdev_id: vdev id
 * @bpcc: bss parameter change count
 * @initialized: flag about the parameter is valid or not
 */
struct mlo_sta_cu_params {
	uint8_t vdev_id;
	uint8_t bpcc;
	bool initialized;
};

/**
 * struct mlo_sta_quiet_status - MLO sta quiet status
 * @link_id: link id
 * @quiet_status: true if corresponding ap in quiet status
 * @valid_status: true if mlo_sta_quiet_status is filled
 */
struct mlo_sta_quiet_status {
	uint8_t link_id;
	bool quiet_status;
	bool valid_status;
};

/* MLO invalid link bitmap for upto 4 links*/
#define MLO_INVALID_LINK_BMAP 0xFFFFFFFF

/* Max number of disallowed bitmap combination sent to FW */
#define MAX_DISALLOW_BMAP_COMB 4

/**
 * enum mlo_disallowed_mode: MLO disallowed mode reasons
 * @MLO_DISALLOWED_MODE_NO_RESTRICTION:
 *  Set disallowed mode has no restrictions(any mode allowed).
 * @MLO_DISALLOWED_MODE_NO_MLMR:
 *  Set disallowed mode has restriction set to no MLMR.
 * @MLO_DISALLOWED_MODE_NO_EMLSR:
 *  Set disallowed mode has restriction set to no EMLSR.
 * @MLO_DISALLOWED_MODE_NO_MLMR_EMLSR:
 *  Set disallowed mode has restriction set to no MLMR or EMLSR
 */
enum mlo_disallowed_mode {
	MLO_DISALLOWED_MODE_NO_RESTRICTION = 0,
	MLO_DISALLOWED_MODE_NO_MLMR = 1,
	MLO_DISALLOWED_MODE_NO_EMLSR = 2,
	MLO_DISALLOWED_MODE_NO_MLMR_EMLSR = 3,
};

/**
 * struct ml_link_disallow_mode_bitmap: MLO link disallow mode bitmap params
 * @disallowed_mode: Bitmap of MLO Modes like MLMR, eMLSR which are not allowed.
 * @ieee_link_id_comb: MLO IEEE Link Ids for which above,
 * disallowed_mode_bitmap is applicable.
 * @ieee_link_id: Each 8-bits in ieee_link_id_comb represents one link ID.
 **/
struct ml_link_disallow_mode_bitmap {
	uint32_t disallowed_mode;
	union {
		uint32_t ieee_link_id_comb;
		uint8_t ieee_link_id[4];
	};
};

/**
 * enum mlo_link_force_mode: MLO link force modes
 * @MLO_LINK_FORCE_MODE_ACTIVE:
 *  Force specific links active
 * @MLO_LINK_FORCE_MODE_INACTIVE:
 *  Force specific links inactive
 * @MLO_LINK_FORCE_MODE_ACTIVE_NUM:
 *  Force active a number of links, firmware to decide which links to inactive
 * @MLO_LINK_FORCE_MODE_INACTIVE_NUM:
 *  Force inactive a number of links, firmware to decide which links to inactive
 * @MLO_LINK_FORCE_MODE_NO_FORCE:
 *  Cancel the force operation of specific links, allow firmware to decide
 * @MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE: Force specific links active and
 *  force specific links inactive
 * @MLO_LINK_FORCE_MODE_NON_FORCE_UPDATE: Only update mlo disallow mode
 */
enum mlo_link_force_mode {
	MLO_LINK_FORCE_MODE_ACTIVE       = 1,
	MLO_LINK_FORCE_MODE_INACTIVE     = 2,
	MLO_LINK_FORCE_MODE_ACTIVE_NUM   = 3,
	MLO_LINK_FORCE_MODE_INACTIVE_NUM = 4,
	MLO_LINK_FORCE_MODE_NO_FORCE     = 5,
	MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE = 6,
	MLO_LINK_FORCE_MODE_NON_FORCE_UPDATE = 7,
};

/**
 * enum mlo_link_force_reason: MLO link force reasons
 * @MLO_LINK_FORCE_REASON_CONNECT:
 *  Set force specific links because of new connection
 * @MLO_LINK_FORCE_REASON_DISCONNECT:
 *  Set force specific links because of new dis-connection
 * @MLO_LINK_FORCE_REASON_LINK_REMOVAL:
 *  Set force specific links because of AP side link removal
 * @MLO_LINK_FORCE_REASON_TDLS:
 *  Set force specific links because of TDLS operation
 */
enum mlo_link_force_reason {
	MLO_LINK_FORCE_REASON_CONNECT    = 1,
	MLO_LINK_FORCE_REASON_DISCONNECT = 2,
	MLO_LINK_FORCE_REASON_LINK_REMOVAL = 3,
	MLO_LINK_FORCE_REASON_TDLS = 4,
};

/**
 * enum set_link_source - set link source
 * @SET_LINK_FROM_CONCURRENCY: concurrent connection request
 * @SET_LINK_FROM_EMLSR_DOWNGRADE: concurrent start and eMLSR downgrade
 * @SET_LINK_FROM_VENDOR_CMD: vendor command request
 * @SET_LINK_FROM_TDLS: tdls command request
 * @SET_LINK_SOURCE_MAX: max num of source
 */
enum set_link_source {
	SET_LINK_FROM_CONCURRENCY,
	SET_LINK_FROM_EMLSR_DOWNGRADE,
	SET_LINK_FROM_VENDOR_CMD,
	SET_LINK_FROM_TDLS,
	SET_LINK_SOURCE_MAX,
};

/**
 * struct set_link_req - set link request
 * @mode: set link mode
 * @reason: reason of set link
 * @force_active_bitmap: force active link bitmap
 * @force_inactive_bitmap: force inactive link bitmap
 * @force_active_num: force active link num
 * @force_active_num_bitmap: force active num link bitmap
 * @force_inactive_num: force inactive link num
 * @force_inactive_num_bitmap: force inactive num link bitmap
 */
struct set_link_req {
	enum mlo_link_force_mode mode;
	enum mlo_link_force_reason reason;
	uint16_t force_active_bitmap;
	uint16_t force_inactive_bitmap;
	uint8_t force_active_num;
	uint16_t force_active_num_bitmap;
	uint8_t force_inactive_num;
	uint16_t force_inactive_num_bitmap;
};

/**
 * struct ml_link_force_state - link force state.
 * @force_active_bitmap: force active link bitmap
 * @force_inactive_bitmap: force inactive link bitmap
 * @force_active_num: force active link num
 * @force_active_num_bitmap: force active num link bitmap
 * @force_inactive_num: force inactive link num
 * @force_inactive_num_bitmap: force inactive num link bitmap
 * @curr_dynamic_inactive_bitmap: dynamic inactive link bitmap
 * @curr_active_bitmap: current active link bitmap
 * @curr_inactive_bitmap: current inactive link bitmap
 */
struct ml_link_force_state {
	uint16_t force_active_bitmap;
	uint16_t force_inactive_bitmap;
	uint8_t force_active_num;
	uint16_t force_active_num_bitmap;
	uint8_t force_inactive_num;
	uint16_t force_inactive_num_bitmap;
	uint16_t curr_dynamic_inactive_bitmap;
	uint16_t curr_active_bitmap;
	uint16_t curr_inactive_bitmap;
};

/**
 * struct wlan_link_force_context - link force ctx.
 * @force_state: current force active/inactive states which
 * have been sent to target
 * @reqs: request of set link
 * @emlsr_disable_req_flags: emlsr disable requests
 * @num_disallow_mode_comb: number of disallowed mode combinations
 * @disallow_mode_link_bmap: disallow mode link bitmap
 */
struct wlan_link_force_context {
	struct ml_link_force_state force_state;
	struct set_link_req reqs[SET_LINK_SOURCE_MAX];
	uint32_t emlsr_disable_req_flags;
	uint32_t num_disallow_mode_comb;
	struct ml_link_disallow_mode_bitmap
		disallow_mode_link_bmap[MAX_DISALLOW_BMAP_COMB];
};

/**
 * struct wlan_rejected_links_info - Data Structure to store
 * rejected links info
 * @num_rej_link_cnt: Number of rejected links
 * @rejected_link_ids: Rejected Link Ids
 */
struct wlan_rejected_links_info {
	uint8_t num_rej_link_cnt;
	uint8_t rejected_link_ids[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * struct wlan_mlo_link_reject_req - Data Structure for link reject
 * request
 * @vdev_id: Vdev id
 * @rej_ieee_link_id: Rejected link id of the ML link
 * @rej_ap_link_addr: Rejected AP link address
 * @acc_ieee_link_id: Accepted Link id of the ML link.
 * @acc_ap_link_addr: Accepted AP link address
 * @link_reject_update_in_progress: Link reject update in progress.
 */
struct wlan_mlo_link_reject_req {
	uint8_t vdev_id;
	uint8_t rej_ieee_link_id;
	struct qdf_mac_addr rej_ap_link_addr;
	uint8_t acc_ieee_link_id;
	struct qdf_mac_addr acc_ap_link_addr;
	bool link_reject_update_in_progress;
};

#if defined(UMAC_SUPPORT_MLNAWDS) || defined(MESH_MODE_SUPPORT)
/**
 * struct mlnawds_config - MLO NAWDS configuration
 * @caps: Bandwidth & NSS capabilities to be configured on NAWDS peer
 * @puncture_bitmap: puncture bitmap to be configured on NAWDS peer
 * @mac: MAC address of the NAWDS peer to which the caps & puncture bitmap is
 * to be configured.
 */
struct mlnawds_config {
	uint64_t caps;
	uint16_t puncture_bitmap;
	uint8_t  mac[QDF_MAC_ADDR_SIZE];
};
#endif

/* AP removed link flag bit position for link_status_flags in
 * struct mlo_link_info
 */
#define LS_F_AP_REMOVAL_BIT 0

/**
 * struct mlo_link_info - ML link info
 * @link_addr: link mac address
 * @link_id: link index
 * @is_bridge : Bridge peer or not
 * @chan_freq: Operating channel frequency
 * @nawds_config: peer's NAWDS configurarion
 * @vdev_id: VDEV ID
 * @mesh_config: peer's MESH configurarion
 * @link_status_flags: Current status of link
 * @ap_link_addr: Associated link BSSID
 * @link_chan_info: Associated link channel info
 * @is_link_active: link state
 * @link_status_code: wlan status code for link
 */
struct mlo_link_info {
	struct qdf_mac_addr link_addr;
	uint8_t link_id;
	bool is_bridge;
	uint16_t chan_freq;
#ifdef UMAC_SUPPORT_MLNAWDS
	struct mlnawds_config nawds_config;
#endif
	uint8_t vdev_id;
#ifdef MESH_MODE_SUPPORT
	struct mlnawds_config mesh_config;
#endif
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
	unsigned long link_status_flags;
	struct qdf_mac_addr ap_link_addr;
	struct wlan_channel *link_chan_info;
#endif
	bool is_link_active;
	enum wlan_status_code link_status_code;
};

/**
 * struct mlo_nstr_info - MLO NSTR capability info
 * @link_id: Lind Id
 * @nstr_lp_present: Flag for NSTR link pair presence
 * @nstr_bmp_size: NSTR Bitmap Size
 * @nstr_lp_bitmap: NSTR link pair bitmap of link_id
 */
struct mlo_nstr_info {
	uint8_t link_id;
	bool nstr_lp_present;
	uint8_t nstr_bmp_size;
	uint16_t nstr_lp_bitmap;
};

#ifndef WLAN_MAX_ML_BSS_LINKS
#define WLAN_MAX_ML_BSS_LINKS WLAN_UMAC_MLO_MAX_VDEVS
#endif

/**
 * struct mlo_partner_info - mlo partner link info
 * @num_partner_links: no. of partner links
 * @partner_link_info: per partner link info
 * @t2lm_enable_val: enum wlan_t2lm_enable
 * @nstr_info: NSTR Capability info
 * @num_nstr_info_links: No. of links for which NSTR info is present
 * @primary_link_id: Link id of primary TQM
 */
struct mlo_partner_info {
	uint8_t num_partner_links;
	uint8_t num_rejected_links;
	struct mlo_link_info partner_link_info[WLAN_MAX_ML_BSS_LINKS];
	struct mlo_link_info rejected_link_info[WLAN_MAX_ML_BSS_LINKS];
#ifdef WLAN_FEATURE_11BE
	enum wlan_t2lm_enable t2lm_enable_val;
	struct mlo_nstr_info nstr_info[WLAN_UMAC_MLO_MAX_VDEVS];
	uint8_t num_nstr_info_links;
#endif
	uint8_t primary_link_id;
};

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * struct emlsr_capability - EMLSR capabilities info
 * @emlsr_supp: EMLSR support is present or not.
 * @trans_timeout: transition timeout
 */
struct emlsr_capability {
	bool emlsr_supp;
	uint8_t trans_timeout;
};
#endif

/**
 * struct wlan_mlo_sta_assoc_pending_list - MLO sta assoc pending list entry
 * @peer_list: MLO peer list
 * @is_timer_started: Indicate timer is started to remove peer mld mac from list
 * @force_remove: To clear all peer mld mac's from the list
 * @rem_peer_mld_mac: timer trigger parameter
 * @list_lock: lock to access members of structure
 */
struct wlan_mlo_sta_assoc_pending_list {
	qdf_list_t peer_list;
	bool is_timer_started;
	bool force_remove;
	qdf_timer_t rem_peer_mld_mac;
	qdf_spinlock_t list_lock;
};

/**
 * struct wlan_mlo_sta - MLO sta additional info
 * @wlan_connect_req_links: list of vdevs selected for connection with the MLAP
 * @wlan_connected_links: list of vdevs associated with this MLO connection
 * @key_mgmt:
 * @connect_req: connect params
 * @copied_conn_req: original connect req
 * @copied_conn_req_lock: lock for the original connect request
 * @assoc_rsp: Raw assoc response frame
 * @mlo_quiet_status:
 * @mlo_csa_param: CSA request parameters for mlo sta
 * @mlo_cu_param: critical update parameters for mlo sta
 * @disconn_req: disconnect req params
 * @copied_reassoc_rsp: Reassoc response copied from assoc link roam handling
 *                      to re-use while link connect in case of deferred/need
 *                      basis link connect (e.g. MLO OWE roaming).
 * @ml_link_state: ml link state command info param
 * @copied_t2lm_ie_assoc_rsp: copy of t2lm ie received in assoc response
 * @ml_partner_info: mlo partner link info
 * @emlsr_cap: EMLSR capabilities info
 * @link_force_ctx: set link force mode context
 * @emlsr_mode_req: store requested emlsr mode
 * @ml_link_control_mode: link control mode configured via user space
 * @ml_chan_switch_in_progress: Flag to track CSA at MLD level
 * @ttlm_send_info: TTLM send command into
 */
struct wlan_mlo_sta {
	qdf_bitmap(wlan_connect_req_links, WLAN_UMAC_MLO_MAX_VDEVS);
	qdf_bitmap(wlan_connected_links, WLAN_UMAC_MLO_MAX_VDEVS);
	struct wlan_mlo_key_mgmt key_mgmt[WLAN_MAX_ML_BSS_LINKS];
	struct wlan_cm_connect_req *connect_req;
	struct wlan_cm_connect_req *copied_conn_req;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t copied_conn_req_lock;
#else
	qdf_mutex_t copied_conn_req_lock;
#endif
	struct element_info assoc_rsp;
	struct mlo_sta_quiet_status mlo_quiet_status[WLAN_UMAC_MLO_MAX_VDEVS];
	struct mlo_sta_csa_params mlo_csa_param[WLAN_UMAC_MLO_MAX_VDEVS];
	struct mlo_sta_cu_params mlo_cu_param[WLAN_UMAC_MLO_MAX_VDEVS];
	struct wlan_cm_disconnect_req *disconn_req;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	struct wlan_cm_connect_resp *copied_reassoc_rsp;
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	struct ml_link_state_cmd_info ml_link_state;
	struct wlan_t2lm_context copied_t2lm_ie_assoc_rsp;
	struct mlo_partner_info ml_partner_info;
	struct emlsr_capability emlsr_cap;
#endif
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
	struct wlan_link_force_context link_force_ctx;
	enum wlan_emlsr_action_mode emlsr_mode_req;
#endif
	uint8_t ml_link_control_mode;
	bool ml_chan_switch_in_progress;
#ifdef WLAN_FEATURE_11BE_MLO_TTLM
	struct ttlm_send_cmd_info ttlm_send_info;
#endif
};

/**
 * struct wlan_mlo_ap - MLO AP related info
 * @vdev_up_candidate_count: Number of vdevs in the MLD to be transitioned to UP
 *                           state.
 * @ml_aid_mgr: ML AID mgr
 * @mlo_ap_lock: lock to sync VDEV SM event
 * @mlo_vdev_quiet_bmap: Bitmap of vdevs for which quiet ie needs to enabled
 * @mlo_vdev_up_bmap: Bitmap of vdevs for which sync complete can be dispatched
 * @assoc_list: MLO sta assoc pending list entry (for FT-over-DS)
 * @mlo_link_reject: Enum to indicate if MLO link rejection configuration
 * @mlrecfg_add_link_rej: MLO setup link reconfiguration add rejection bitmap
 */
struct wlan_mlo_ap {
	uint8_t vdev_up_candidate_count;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t mlo_ap_lock;
#else
	qdf_mutex_t mlo_ap_lock;
#endif
	qdf_bitmap(mlo_vdev_quiet_bmap, WLAN_UMAC_MLO_MAX_VDEVS);
	qdf_bitmap(mlo_vdev_up_bmap, WLAN_UMAC_MLO_MAX_VDEVS);
	struct wlan_mlo_sta_assoc_pending_list assoc_list;
	enum mlo_link_rej_cfg mlo_link_reject;
#ifdef WLAN_MLO_SETUP_LINK_RECFG
	qdf_bitmap(mlrecfg_add_link_rej, MAX_MLO_LINK_ID + 1);
#endif /* WLAN_MLO_SETUP_LINK_RECFG */
};

/**
 * struct wlan_mlo_peer_list - MLO peer list entry
 * @peer_hash: MLO peer hash code
 * @peer_list_lock: lock to access members of structure
 */
struct wlan_mlo_peer_list {
	qdf_list_t peer_hash[WLAN_PEER_HASHSIZE];
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t peer_list_lock;
#else
	qdf_mutex_t peer_list_lock;
#endif
};

/**
 * struct mlo_vdev_link_mac_info - VDEV to link address map
 * @vdev_id: Vdev Id with which this link mac address is associated:
 * @link_mac_addr: link specific mac address
 */
struct mlo_vdev_link_mac_info {
	uint8_t vdev_id;
	struct qdf_mac_addr link_mac_addr;
};

/**
 * struct wlan_mlo_link_mac_update: VDEV to link MAC address list
 * @num_mac_update: Number of mac address
 * @link_mac_info: Each VDEV to link mac address mapping
 */
struct wlan_mlo_link_mac_update {
	int num_mac_update;
	struct mlo_vdev_link_mac_info link_mac_info[3];
};

/**
 * struct wlan_mlo_dev_context - MLO device context
 * @node: QDF list node member
 * @mld_id: MLD id
 * @mld_addr: MLO device MAC address
 * @wlan_vdev_list: list of vdevs associated with this MLO connection
 * @wlan_bridge_vdev_list: list of bridge vdevs associated with this MLO
 * @wlan_bridge_vdev_count: number of elements in the bridge vdev list
 * @bridge_sta_ctx: bridge sta context
 * @br_pdev_list: pdev used for bridge vap creation in the MLD
 * @wlan_vdev_count: number of elements in the vdev list
 * @mlo_peer_list: list peers in this MLO connection
 * @wlan_max_mlo_peer_count: peer count across the links of specific MLO
 * @mlo_dev_lock: lock to access struct
 * @tsf_recalculation_lock: Lock to protect TSF (re)calculation
 * @ref_cnt: reference count
 * @ref_id_dbg: Reference count debug information
 * @sta_ctx: MLO STA related information
 * @ap_ctx: AP related information
 * @t2lm_ctx: T2LM related information
 * @epcs_ctx: EPCS related information
 * @ptqm_migrate_timer: timer for ptqm migration
 * @mlo_peer_id_bmap: mlo_peer_id bitmap for ptqm migration
 * @link_ctx: link related information
 * @mlo_max_recom_simult_links: Max Recommended Simultaneous Links
 * @mlo_extmld_cap_advertisement: Enable/disable Extended MLD Cap and OP
 *                                advertisement
 * @link_ptqm_migrate_ctx: PTQM migration link context
 * @mlo_reconfiguration_supported: Flag to indicate if link reconfiguration
 *                                 is supported
 */
struct wlan_mlo_dev_context {
	qdf_list_node_t node;
	uint8_t mld_id;
	struct qdf_mac_addr mld_addr;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	qdf_work_t bridge_vap_work;
	struct wlan_objmgr_vdev *wlan_bridge_vdev_list[WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS];
	struct wlan_mlo_bridge_sta *bridge_sta_ctx;
	struct wlan_objmgr_pdev *br_pdev_list[MLO_MAX_BRIDGE_LINKS_PER_MLD];
#endif
	uint16_t wlan_vdev_count;
	uint16_t wlan_bridge_vdev_count;
	struct wlan_mlo_peer_list mlo_peer_list;
	uint16_t wlan_max_mlo_peer_count;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t mlo_dev_lock;
	qdf_spinlock_t tsf_recalculation_lock;
#else
	qdf_mutex_t mlo_dev_lock;
	qdf_mutex_t tsf_recalculation_lock;
#endif
	qdf_atomic_t ref_cnt;
	qdf_atomic_t ref_id_dbg[WLAN_REF_ID_MAX];
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_mlo_ap *ap_ctx;
	struct wlan_t2lm_context t2lm_ctx;
	struct wlan_epcs_context epcs_ctx;
#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
	qdf_timer_t ptqm_migrate_timer;
	qdf_bitmap(mlo_peer_id_bmap, MAX_MLO_PEER_ID);
	struct ptqm_migrate_link_req_context *link_ptqm_migrate_ctx
		[WLAN_UMAC_MLO_MAX_VDEVS];
#endif
	struct mlo_link_switch_context *link_ctx;
	uint8_t mlo_max_recom_simult_links;
	bool mlo_extmld_cap_advertisement;
	bool mlo_reconfiguration_supported;
};

/**
 * struct wlan_mlo_link_peer_entry - Link peer entry
 * @link_peer: Object manager peer
 * @link_addr: MAC address of link peer
 * @link_ix: Link index
 * @is_primary: sets true if the peer is primary UMAC’s peer
 * @hw_link_id: HW Link id of peer
 * @assoc_rsp_buf: Assoc resp buffer
 * @peer_assoc_sent: flag to indicate peer assoc sent to FW
 */
struct wlan_mlo_link_peer_entry {
	struct wlan_objmgr_peer *link_peer;
	struct qdf_mac_addr link_addr;
	uint8_t link_ix;
	bool is_primary;
	uint8_t hw_link_id;
	qdf_nbuf_t assoc_rsp_buf;
	bool peer_assoc_sent;
};

/**
 * enum mlo_peer_state - MLO peer state
 * @ML_PEER_CREATED:     Initial state
 * @ML_PEER_ASSOC_DONE:  ASSOC sent on assoc link
 * @ML_PEER_DISCONN_INITIATED: Disconnect initiated on one of the links
 */
enum mlo_peer_state {
	ML_PEER_CREATED,
	ML_PEER_ASSOC_DONE,
	ML_PEER_DISCONN_INITIATED,
};

#ifdef WLAN_MLO_SETUP_LINK_RECFG
enum mlrecfg_op_type {
	MLRECFG_DEL_LINK_OP,
	MLRECFG_ADD_LINK_OP,
	MLRECFG_OP_MAX,
	MLRECFG_OP_INVALID,
};

struct recfg_op {
	enum mlrecfg_op_type op_type;
	int link;
	struct qdf_mac_addr link_mac;
	uint16_t status;
};

struct mlrecfg_op_cont {
	int total_num_ops;
	struct recfg_op op[MAX_MLO_LINK_PEERS];
	struct recfg_op *link_to_op_map[MAX_MLO_LINK_ID + 1];
};

struct recfg_os {
	enum mlrecfg_op_type op_type;
	int link;
	uint16_t status_code;
};

struct mlrecfg_os_cont {
	int size;
	struct recfg_os os[MAX_MLO_LINK_PEERS];
	struct recfg_os *link_to_os_map[MAX_MLO_LINK_ID + 1];
};

enum recfg_state {
	RECFG_S_INIT,
	RECFG_S_HANDLE_REQ,
	RECFG_S_CHECK_KEY_INSTALL,
	RECFG_S_DO_KEY_INSTALL,
	RECFG_S_RESP_AND_KEY_INSTALL_COMPL,
	RECFG_S_PLINK_RESELECTION,
	RECFG_S_CHECK_AUTHORIZE,
	RECFG_S_DO_AUTHORIZE,
	RECFG_S_CHECK_PEER_DEL,
	RECFG_S_DO_PEER_DEL,
	RECFG_S_COMPL,
	RECFG_S_DISCONNECT,
	RECFG_S_MAX,
	RECFG_SS_DEFAULT,
	RECFG_SS_VALIDATION,
	RECFG_SS_DECLINE,
	RECFG_SS_PROCESSING,
	RECFG_SS_DECISION_MAKING,
	RECFG_SS_PEER_ASSOC,
	RECFG_SS_MAX,
};

enum recfg_ev {
	RECFG_EV_NEW_REQ,
	RECFG_EV_HANDLE_NEW_REQ,
	RECFG_EV_START_VALIDATION,
	RECFG_EV_VALIDATION_SUCCESS,
	RECFG_EV_VALIDATION_FAILURE,
	RECFG_EV_DECLINE_REQ,
	RECFG_EV_START_PROCESSING,
	RECFG_EV_ADD_LINK_STA_CMD,
	RECFG_EV_DEL_LINK_STA_CMD,
	RECFG_EV_PEER_CREATE_SUCCESS,
	RECFG_EV_PEER_CREATE_FAILURE,
	RECFG_EV_PEER_CREATE_TIMEOUT,
	RECFG_EV_DRV_OP_RESP,
	RECFG_EV_TGT_OP_RESP,
	RECFG_EV_APP_OP_RESP,
	RECFG_EV_DRV_PROCESSING_COMPL,
	RECFG_EV_TGT_PROCESSING_COMPL,
	RECFG_EV_APP_PROCESSING_COMPL,
	RECFG_EV_PROCESSING_COMPL,
	RECFG_EV_APP_RESP_FRAME,
	RECFG_EV_START_DECISION_MAKING,
	RECFG_EV_DECISION_MAKING_COMPL,
	RECFG_EV_DISPATCH_PEER_ASSOC_CMD,
	RECFG_EV_PEER_ASSOC_SUCCESS,
	RECFG_EV_PEER_ASSOC_FAILURE,
	RECFG_EV_PEER_ASSOC_TIMEOUT,
	RECFG_EV_CHECK_KEY_INSTALL_REQUIRED,
	RECFG_EV_DISPATCH_KEY_INSTALL_CMD,
	RECFG_EV_KEY_INSTALL_SUCCESS,
	RECFG_EV_KEY_INSTALL_FAILURE,
	RECFG_EV_KEY_INSTALL_TIMEOUT,
	RECFG_EV_KEY_INSTALL_COMPL,
	RECFG_EV_DISPATCH_RESP_FRAME,
	RECFG_EV_RESP_TX_COMPL_SUCCESS,
	RECFG_EV_RESP_TX_COMPL_FAILURE,
	RECFG_EV_RESP_TX_COMPL,
	RECFG_EV_RESP_AND_KEY_INSTALL_COMPL,
	RECFG_EV_START_PLINK_RESELECTION,
	RECFG_EV_RESELECT_CUR_PLINK,
	RECFG_EV_PLINK_MIGRATION_SUCCESS,
	RECFG_EV_PLINK_MIGRATION_FAILURE,
	RECFG_EV_SKIP_PLINK_RESELECTION,
	RECFG_EV_PLINK_RESELECTION_COMPL,
	RECFG_EV_CHECK_AUTHORIZE_REQUIRED,
	RECFG_EV_DISPATCH_AUTHORIZE_CMD,
	RECFG_EV_AUTHORIZE_COMPL,
	RECFG_EV_CHECK_PEER_DEL_REQUIRED,
	RECFG_EV_DISPATCH_PEER_DEL_CMD,
	RECFG_EV_PEER_DEL_SUCCESS,
	RECFG_EV_PEER_DEL_FAILURE,
	RECFG_EV_PEER_DEL_TIMEOUT,
	RECFG_EV_PEER_DEL_COMPL,
	RECFG_EV_DRV_OP_COMPL,
	RECFG_EV_CHECK_APP_OP_COMPL,
	RECFG_EV_APP_OP_COMPL_SUCCESS,
	RECFG_EV_APP_OP_COMPL_TIMEOUT,
	RECFG_EV_APP_OP_COMPL,
	RECFG_EV_COMPL,
	RECFG_EV_DISCONNECT,
	RECFG_EV_WAIT_FOR_NEW_REQ,
};

struct key_cache {
	bool valid;
	struct wlan_crypto_req_key key;
};

struct recfg_sm {
	qdf_spinlock_t sm_lock;
	struct wlan_sm *sm;
	enum recfg_state state;
	enum recfg_state substate;
	struct req *active_req;
	qdf_bitmap(peer_assoc_resp_pending, MAX_MLO_LINK_ID + 1);
	qdf_bitmap(key_install_resp_pending, MAX_MLO_LINK_ID + 1);
	qdf_bitmap(peer_del_resp_pending, MAX_MLO_LINK_ID + 1);
	qdf_bitmap(tgt_op_resp_pending, MAX_MLO_LINK_ID + 1);
	struct mlrecfg_os_cont tgt_os_cont;
	qdf_bitmap(drv_op_resp_pending, MAX_MLO_LINK_ID + 1);
	struct mlrecfg_os_cont drv_os_cont;
	qdf_bitmap(app_op_resp_pending, MAX_MLO_LINK_ID + 1);
	struct mlrecfg_os_cont app_os_cont;
	qdf_event_t link_create_compl[MAX_MLO_LINK_ID + 1];
	qdf_nbuf_t app_resp_wbuf;
	bool resp_tx_compl_pending;
	struct mlrecfg_os_cont comb_os_cont;
	struct key_cache ptk_cache;
	qdf_bitmap(app_op_compl_pending, MAX_MLO_LINK_ID + 1);
};

struct recfg_cb {
	QDF_STATUS(*resp_ready)(void *arg, uint8_t req_id,
				qdf_nbuf_t app_resp_wbuf,
				struct mlrecfg_os_cont *status_container);
	QDF_STATUS(*compl)(void *arg, uint8_t req_id);
	void *arg;
};

struct recfg_mgr {
	struct recfg_sm sm;
	struct wlan_mlo_peer_context *mlpeer;
	struct recfg_cb cb;
	bool cb_registered;
};

/* Forward declaration */
struct requestor;
struct responder;

struct mlrecfg_ctx {
	struct requestor *remote_requestor;
	struct responder *local_responder;
	struct recfg_mgr *recfg_mgr;
};
#endif /* WLAN_MLO_SETUP_LINK_RECFG */

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
enum mlrecfg_plink_resel_mode {
	MLRECFG_PLINK_RESEL_ALWAYS = 0,
	MLRECFG_PLINK_RESEL_CONDITIONAL = 1,
	MLRECFG_PLINK_RESEL_MODE_MAX,
};
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */

/**
 * struct mlpeer_auth_params - Deferred Auth params
 * @vdev_id:  VDEV ID
 * @psoc_id:  PSOC ID
 * @link_addr: MAC address
 * @mldaddr: MLD MAC address
 * @algo:  Auth algorithm
 * @seq: Auth sequence number
 * @status_code: Auth status
 * @challenge: Auth Challenge
 * @challenge_length: Auth Challenge length
 * @wbuf:  Auth wbuf
 * @rs: Rx stats
 */
struct mlpeer_auth_params {
	uint8_t vdev_id;
	uint8_t psoc_id;
	struct qdf_mac_addr link_addr;
	struct qdf_mac_addr mldaddr;
	uint16_t algo;
	uint16_t seq;
	uint16_t status_code;
	uint8_t *challenge;
	uint8_t challenge_length;
	qdf_nbuf_t wbuf;
	void *rs;
};

/**
 * struct wlan_mlo_eml_cap - EML capabilities of MLD
 * @emlsr_supp: eMLSR Support
 * @emlsr_pad_delay: eMLSR Padding Delay
 * @emlsr_trans_delay: eMLSR transition delay
 * @emlmr_supp: eMLMR Support
 * @emlmr_delay: eMLMR Delay
 * @trans_timeout: Transition Timeout
 * @reserved: Reserved
 */
struct wlan_mlo_eml_cap {
	uint16_t emlsr_supp:1,
		 emlsr_pad_delay:3,
		 emlsr_trans_delay:3,
		 emlmr_supp:1,
		 emlmr_delay:3,
		 trans_timeout:4,
		 reserved:1;
};

/**
 * struct wlan_mlo_msd_cap - MSD capabilities of MLD
 * @medium_sync_duration: Medium Sync Duration
 * @medium_sync_ofdm_ed_thresh: MSD threshold value
 * @medium_sync_max_txop_num: Max number of TXOP
 */
struct wlan_mlo_msd_cap {
	uint16_t medium_sync_duration:8,
		 medium_sync_ofdm_ed_thresh:4,
		 medium_sync_max_txop_num:4;
};

/**
 * struct wlan_mlo_mld_cap - MLD capabilities of MLD
 * @max_simult_link: Maximum number of simultaneous links
 * @srs_support: SRS support
 * @tid2link_neg_support: TID to Link Negotiation Support
 * @str_freq_sep: Frequency separation suggested by STR non-AP MLD
 *                OR Type of AP-MLD
 * @aar_support: AAR Support
 * @link_reconfig_support: Link Reconfiguration Operation Support
 * @reserved: Reserved
 */
struct wlan_mlo_mld_cap {
	uint16_t max_simult_link:4,
		 srs_support:1,
		 tid2link_neg_support:2,
		 str_freq_sep:5,
		 aar_support:1,
		 link_reconfig_support:1,
		 reserved:2;
};

#ifdef WLAN_FEATURE_11BE_MLO_TTLM
/**
 * struct ttlm_state_sm - TTLM state machine
 * @ttlm_sm_lock: SM lock
 * @sm_hdl: SM handlers
 * @ttlm_state: Current state
 * @ttlm_substate: Current substate
 */
struct ttlm_state_sm {
	qdf_mutex_t ttlm_sm_lock;
	struct wlan_sm *sm_hdl;
	enum wlan_ttlm_sm_state ttlm_state;
	enum wlan_ttlm_sm_state ttlm_substate;
};
#endif

#ifdef WLAN_FEATURE_11BE
/**
 * enum mlreconfig_operation_type - ML Reconfiguration Operation Types
 * @MLRECONFIG_OPERATION_TYPE_AP_REMOVAL: AP removal
 * @MLRECONFIG_OPERATION_TYPE_OPERATION_PARAM_UPDATE: Operation Param Update
 * @MLRECONFIG_OPERATION_TYPE_ADD_LINK: Setup link addition
 * @MLRECONFIG_OPERATION_TYPE_DELETE_LINK: Setup link deletion
 * @MLRECONFIG_OPERATION_TYPE_MAX: Max limit on operation types
 */
enum mlreconfig_operation_type {
	MLRECONFIG_OPERATION_TYPE_AP_REMOVAL = 0,
	MLRECONFIG_OPERATION_TYPE_OPERATION_PARAM_UPDATE = 1,
	MLRECONFIG_OPERATION_TYPE_ADD_LINK = 2,
	MLRECONFIG_OPERATION_TYPE_DELETE_LINK = 3,
	MLRECONFIG_OPERATION_TYPE_MAX = 4,
};
#endif

#ifdef WLAN_MLO_SETUP_LINK_RECFG
/**
 * struct mlreconfig_setup_link_info - ml setup link information
 * @link_id: setup link id
 * @reconfig_optype: reconfiguration operation type
 */
struct mlreconfig_setup_link_info {
	uint8_t link_id : 4,
		reserved: 4;
	enum mlreconfig_operation_type reconfig_optype;
};

/**
 * struct mlreconfig_setup_links_req - link reconfig request information
 * @setup_link_info: setup links information to be reconfigured
 */
struct mlreconfig_setup_links_req {
	struct mlreconfig_setup_link_info
		setup_link_info[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * struct mlreconfig_kde - Group key data
 * @len: key data len
 * @data: key data
 */
struct mlreconfig_kde {
	uint8_t len;
	uint8_t *data;
};

/**
 * struct mlreconfig_setup_links_resp - link reconfig response information
 * @status_container: setup link status code container
 * @kde: accepted add links group key data
 */
struct mlreconfig_setup_links_resp {
	struct mlrecfg_os_cont status_container;
	struct mlreconfig_kde kde;
};

/**
 * enum mlreconfig_setup_links_category - setup link reconfig action
 *                                        categories
 * @MLRECONFIG_SETUP_LINKS_CATEGORY_NOTIFY:  Notify to non-AP MLD
 * @MLRECONFIG_SETUP_LINKS_CATEGORY_REQUEST: Request to AP MLD
 * @MLRECONFIG_SETUP_LINKS_CATEGORY_RESPONSE: Response from AP MLD
 */
enum mlreconfig_setup_links_category {
	MLRECONFIG_SETUP_LINKS_CATEGORY_NOTIFY,
	MLRECONFIG_SETUP_LINKS_CATEGORY_REQUEST,
	MLRECONFIG_SETUP_LINKS_CATEGORY_RESPONSE,
};

/**
 * struct mlreconfig_setup_links_action - ml setup link action infomration
 * @dialog_token: Non-zero dialog token
 * @category: Link reconfig action category
 * @num_operations: Number of reconfiguration operations
 * @reconfig_optype: reconfiguration operation type
 * @mlreconfig_link_req: setup link request information
 * @mlreconfig_link_resp: setup link response information
 */
struct mlreconfig_setup_links_action {
	uint8_t dialog_token;
	enum mlreconfig_setup_links_category category;
	int num_operations;
	union {
		struct mlreconfig_setup_links_req mlreconfig_link_req;
		struct mlreconfig_setup_links_resp mlreconfig_link_resp;
	};
};
#endif /* WLAN_MLO_SETUP_LINK_RECFG */

/**
 * struct wlan_mlo_peer_context - MLO peer context
 *
 * @peer_node:     peer list node for ml_dev qdf list
 * @peer_list: list of peers on the MLO link
 * @link_peer_cnt: Number of link peers attached
 * @max_links: Max links for this ML peer
 * @link_asresp_cnt: Number of reassoc resp generated
 * @mlo_peer_id: unique ID for the peer
 * @peer_mld_addr: MAC address of MLD link
 * @mlo_ie: MLO IE struct
 * @mlo_peer_lock: lock to access peer structure
 * @assoc_id: Assoc ID derived by MLO manager
 * @primary_umac_psoc_id:
 * @ref_cnt: Reference counter to avoid use after free
 * @ml_dev: MLO dev context
 * @mlpeer_state: MLO peer state
 * @recfg_ctx: MLO setup link reconfiguration context
 * @avg_link_rssi: avg RSSI of ML peer
 * @is_nawds_ml_peer: flag to indicate if ml_peer is NAWDS configured
 * @nawds_config: eack link peer's NAWDS configuration
 * @pending_auth: Holds pending auth request
 * @t2lm_policy: TID-to-link mapping information
 * @epcs_info: EPCS information
 * @setup_links_action_info: ML Reconfig setup links action info
 * @msd_cap_present: Medium Sync Capability present bit
 * @mlpeer_emlcap: EML capability information for ML peer
 * @mlpeer_msdcap: Medium Sync Delay capability information for ML peer
 * @is_mesh_ml_peer: flag to indicate if ml_peer is MESH configured
 * @mesh_config: eack link peer's MESH configuration
 * @mlpeer_mldcap: MLD Capability information for ML peer
 * @mlpeer_nstrinfo: NSTR Capability info
 * @migrate_primary_umac_psoc_id: primary umac psoc id selected for umac
 * migration
 * @primary_umac_migration_in_progress: flag to indicate primary umac migration
 * in progress
 * @ttlm_sm: TTLM state machine
 * @ttlm_request_timer: TTLM request timer
 * @peer_ptqm_migrate_ctx: PTQM migration peer context
 * @mlrecfg_plink_resel_mode: MLO reconfiguration primary link reselection mode
 * @recov_peer_hw_link_id_bmap: HW link id bitmap of peer handled through recovery for mlo peer delete
 * @assoc_wbuf: Cached link specific association request
 */
struct wlan_mlo_peer_context {
	qdf_list_node_t peer_node;
	struct wlan_mlo_link_peer_entry peer_list[MAX_MLO_LINK_PEERS];
	uint8_t link_peer_cnt;
	uint8_t max_links;
	uint8_t link_asresp_cnt;
	uint32_t mlo_peer_id;
	struct qdf_mac_addr peer_mld_addr;
	uint8_t *mlo_ie;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t mlo_peer_lock;
#else
	qdf_mutex_t mlo_peer_lock;
#endif
	uint16_t assoc_id;
	uint8_t primary_umac_psoc_id;
	qdf_atomic_t ref_cnt;
	struct wlan_mlo_dev_context *ml_dev;
#ifdef WLAN_MLO_SETUP_LINK_RECFG
	struct mlrecfg_ctx recfg_ctx;
#endif /* WLAN_MLO_SETUP_LINK_RECFG */
	enum mlo_peer_state mlpeer_state;
	int8_t avg_link_rssi;
#ifdef UMAC_SUPPORT_MLNAWDS
	bool is_nawds_ml_peer;
	struct mlnawds_config nawds_config[MAX_MLO_LINK_PEERS];
#endif
#ifdef UMAC_MLO_AUTH_DEFER
	struct mlpeer_auth_params *pending_auth[MAX_MLO_LINK_PEERS];
#endif
#ifdef WLAN_FEATURE_11BE
	struct wlan_mlo_peer_t2lm_policy t2lm_policy;
	struct wlan_mlo_peer_epcs_info epcs_info;
#ifdef WLAN_MLO_SETUP_LINK_RECFG
	struct mlreconfig_setup_links_action setup_links_action_info;
#endif /* WLAN_MLO_SETUP_LINK_RECFG */
#endif
	bool msd_cap_present;
	struct wlan_mlo_eml_cap mlpeer_emlcap;
	struct wlan_mlo_msd_cap mlpeer_msdcap;
#ifdef MESH_MODE_SUPPORT
	bool is_mesh_ml_peer;
	struct mlnawds_config mesh_config[MAX_MLO_LINK_PEERS];
#endif
	struct wlan_mlo_mld_cap mlpeer_mldcap;
	struct mlo_nstr_info mlpeer_nstrinfo[WLAN_UMAC_MLO_MAX_VDEVS];
	uint8_t migrate_primary_umac_psoc_id;
	bool primary_umac_migration_in_progress;
#ifdef WLAN_FEATURE_11BE_MLO_TTLM
	struct ttlm_state_sm ttlm_sm;
	qdf_mc_timer_t ttlm_request_timer;
#endif
#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
	struct ptqm_migrate_peer_context *peer_ptqm_migrate_ctx;
	enum mlrecfg_plink_resel_mode mlrecfg_plink_resel_mode;
#endif
	uint32_t recov_peer_hw_link_id_bmap;
	qdf_nbuf_t assoc_wbuf;
};

/**
 * struct mlo_probereq_info - mlo probe req link info
 * @mlid: MLID requested in the probe req
 * @num_links: no. of link info in probe req
 * @link_id: target link id of APs
 * @is_mld_id_valid: Indicates if mld_id is valid for a given request
 * @skip_mbssid: Skip mbssid IE
 */
struct mlo_probereq_info {
	uint8_t mlid;
	uint8_t num_links;
	uint8_t link_id[WLAN_UMAC_MLO_MAX_VDEVS];
	bool is_mld_id_valid;
	bool skip_mbssid;
};

/**
 * struct ml_rv_partner_link_info: Partner link information of an ML reconfig IE
 * @link_id: Link id advertised by the AP
 * @link_mac_addr: Link mac address
 * @is_ap_removal_timer_p: AP removal timer is present or not
 * @ap_removal_timer: number of TBTTs of the AP removal timer
 * @reconfig_optype: Reconfiguration operation type
 */
struct ml_rv_partner_link_info {
	uint8_t link_id;
	struct qdf_mac_addr link_mac_addr;
	uint8_t is_ap_removal_timer_p;
	uint16_t ap_removal_timer;
	enum mlreconfig_operation_type reconfig_optype;
};

/**
 * struct ml_rv_info: Reconfig Multi link information of a 11be beacon
 * @mld_mac_addr: MLD mac address
 * @num_links: Number of links supported by ML AP
 * @link_info: Array containing partner links information
 */
struct ml_rv_info {
	struct qdf_mac_addr mld_mac_addr;
	uint8_t num_links;
	struct ml_rv_partner_link_info link_info[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * struct mlo_tgt_link_info - ML target link info
 * @vdev_id: link peer vdev id
 * @hw_mld_link_id: HW link id
 * @mlo_enabled: indicate is MLO enabled
 * @mlo_assoc_link: indicate is the link used to initialize the association
 *                  of mlo connection
 * @mlo_primary_umac: indicate is the link on primary UMAC, WIN only flag
 * @mlo_logical_link_index_valid: indicate if the logial link index in is valid
 * @mlo_peer_id_valid: indicate if the mlo peer id is valid
 * @mlo_force_link_inactive: force the peer inactive
 * @emlsr_support: indicate if eMLSR supported
 * @emlmr_support: indicate if eMLMR supported
 * @msd_cap_support: indicate if MSD supported
 * @mlo_bridge_peer: indicate if it is bridge peer
 * @ieee_link_id: IEEE link ID of the link peer
 * @unused: spare bits
 * @logical_link_index: Unique index for links of the mlo. Starts with Zero
 */
struct mlo_tgt_link_info {
	uint8_t vdev_id;
	uint8_t hw_mld_link_id;
	uint32_t mlo_enabled:1,
		 mlo_assoc_link:1,
		 mlo_primary_umac:1,
		 mlo_logical_link_index_valid:1,
		 mlo_peer_id_valid:1,
		 mlo_force_link_inactive:1,
		 emlsr_support:1,
		 emlmr_support:1,
		 msd_cap_support:1,
		 mlo_bridge_peer:1,
		 ieee_link_id:4,
		 mlo_link_add:1,
		 mlo_link_del:1,
		 unused:16;
	uint32_t logical_link_index;

};

/**
 * struct mlo_tgt_partner_info - mlo target partner link info
 * @num_partner_links: no. of partner links
 * @link_info: per partner link info
 */
struct mlo_tgt_partner_info {
	uint8_t num_partner_links;
	struct mlo_tgt_link_info link_info[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * struct wlan_mlo_bridge_sta - MLO bridge sta context
 * @bridge_partners: mlo_partner_info of partners of a bridge
 * @bridge_ml_links: mlo_tgt_partner_info of partners of bridge
 * @bridge_umac_id: umac id for bridge
 * @bridge_link_id: link id used by bridge vdev
 * @is_force_central_primary: Flag to tell if bridge should be primary umac
 * @bridge_vap_exists: If there is bridge vap
 * @bridge_node_auth: Is bridge node auth done
 * @bss_mld_addr: MLD address of the BSS
 */
struct wlan_mlo_bridge_sta {
	struct mlo_partner_info bridge_partners;
	struct mlo_tgt_partner_info bridge_ml_links;
	uint8_t bridge_umac_id;
	uint8_t bridge_link_id;
	bool is_force_central_primary;
	bool bridge_vap_exists;
	bool bridge_node_auth;
	struct qdf_mac_addr bss_mld_addr;
};

/**
 * struct mlo_mlme_ext_ops - MLME callback functions
 * @mlo_mlme_ext_validate_conn_req: Callback to validate connect request
 * @mlo_mlme_ext_create_link_vdev: Callback to create link vdev for ML STA
 * @mlo_mlme_ext_peer_create: Callback to create link peer
 * @mlo_mlme_ext_bridge_peer_create: Callback to create bridge peer
 * @mlo_mlme_ext_peer_assoc: Callback to initiate peer assoc
 * @mlo_mlme_ext_peer_assoc_fail: Callback to notify peer assoc failure
 * @mlo_mlme_ext_peer_delete: Callback to initiate link peer delete
 * @mlo_mlme_ext_assoc_resp: Callback to initiate assoc resp
 * @mlo_mlme_get_link_assoc_req: Callback to get link assoc req buffer
 * @mlo_mlme_ext_deauth: Callback to initiate deauth
 * @mlo_mlme_ext_clone_security_param: Callback to clone mlo security params
 * @mlo_mlme_ext_peer_process_auth: Callback to process pending auth
 * @mlo_mlme_ext_handle_sta_csa_param: Callback to handle sta csa param
 * @mlo_mlme_ext_sta_op_class:
 * @mlo_mlme_ext_peer_reassoc: Callback to process reassoc
 * @mlo_mlme_ext_connect_get_partner_info: Callback to get MLO partner info
 * @mlo_mlme_ext_set_ieee_link_id: Callback to update ieee_link_id in vap
 * @mlo_mlme_ext_teardown_tdls: Callback to teardown TDLS
 * @init_mlrecfg_ctx: Init MLO setup link reconfiguration context
 * @deinit_mlrecfg_ctx: Deinit MLO setup link reconfiguration context
 * @is_mlrecfg_in_progress: Check MLO setup link reconfiguration is in progress
 * @is_mlrecfg_add_op_accepted: Check MLO link addition to the setup links is
 * accepted
 * @is_mlrecfg_del_op_accepted: Check MLO link deletion from the setup links is
 * accepted
 * @is_mlrecfg_add_op_rejected: Check MLO link addition to the setup links is
 * rejected
 * @is_mlrecfg_del_op_rejected: Check MLO link deletion from the setup links is
 * rejected
 */
struct mlo_mlme_ext_ops {
	QDF_STATUS (*mlo_mlme_ext_validate_conn_req)(
		    struct vdev_mlme_obj *vdev_mlme, void *ext_data);
	QDF_STATUS (*mlo_mlme_ext_create_link_vdev)(
		    struct vdev_mlme_obj *vdev_mlme, void *ext_data);
	QDF_STATUS (*mlo_mlme_ext_peer_create)(struct wlan_objmgr_vdev *vdev,
					struct wlan_mlo_peer_context *ml_peer,
					struct qdf_mac_addr *addr,
					qdf_nbuf_t frm_buf);
	QDF_STATUS (*mlo_mlme_ext_bridge_peer_create)(struct wlan_objmgr_vdev *vdev,
					struct wlan_mlo_peer_context *ml_peer,
					struct qdf_mac_addr *addr,
					qdf_nbuf_t frm_buf);
	void (*mlo_mlme_ext_peer_assoc)(struct wlan_objmgr_peer *peer);
	void (*mlo_mlme_ext_peer_assoc_fail)(struct wlan_objmgr_peer *peer);
	void (*mlo_mlme_ext_peer_delete)(struct wlan_objmgr_peer *peer);
	void (*mlo_mlme_ext_assoc_resp)(struct wlan_objmgr_peer *peer);
	qdf_nbuf_t (*mlo_mlme_get_link_assoc_req)(struct wlan_objmgr_peer *peer,
						  uint8_t link_ix);
	void (*mlo_mlme_ext_deauth)(struct wlan_objmgr_peer *peer,
				    uint8_t is_disassoc);
	QDF_STATUS (*mlo_mlme_ext_clone_security_param)(
		    struct vdev_mlme_obj *vdev_mlme,
		    struct wlan_cm_connect_req *req);
#ifdef UMAC_MLO_AUTH_DEFER
	void (*mlo_mlme_ext_peer_process_auth)(
	      struct mlpeer_auth_params *auth_param);
#endif
	void (*mlo_mlme_ext_handle_sta_csa_param)(
				struct wlan_objmgr_vdev *vdev,
				struct csa_offload_params *csa_param);
	QDF_STATUS (*mlo_mlme_ext_sta_op_class)(
			struct vdev_mlme_obj *vdev_mlme,
			uint8_t *ml_ie);
	QDF_STATUS (*mlo_mlme_ext_peer_reassoc)(struct wlan_objmgr_vdev *vdev,
					struct wlan_mlo_peer_context *ml_peer,
					struct qdf_mac_addr *addr,
					qdf_nbuf_t frm_buf);
#ifdef ENABLE_CFG80211_BACKPORTS_MLO
	QDF_STATUS (*mlo_mlme_ext_connect_get_partner_info)(
				struct wlan_objmgr_vdev *vdev,
				const struct cfg80211_connect_params *req,
				struct mlo_partner_info *partner_info);
	QDF_STATUS (*mlo_mlme_ext_set_ieee_link_id)(struct wlan_objmgr_vdev *vdev);
#endif
	QDF_STATUS (*mlo_mlme_ext_teardown_tdls)(struct wlan_objmgr_psoc *psoc);
#ifdef WLAN_MLO_SETUP_LINK_RECFG
	QDF_STATUS (*init_mlrecfg_ctx)(struct wlan_mlo_peer_context *mlpeer);
	QDF_STATUS (*deinit_mlrecfg_ctx)(struct wlan_mlo_peer_context *mlpeer);
	bool (*is_mlrecfg_in_progress)(struct wlan_mlo_peer_context *mlpeer);
	bool (*is_mlrecfg_add_op_accepted)(struct wlan_mlo_peer_context *mlpeer,
					   int link);
	bool (*is_mlrecfg_del_op_accepted)(struct wlan_mlo_peer_context *mlpeer,
					   int link);
	bool (*is_mlrecfg_add_op_rejected)(struct wlan_mlo_peer_context *mlpeer,
					   int link);
	bool (*is_mlrecfg_del_op_rejected)(struct wlan_mlo_peer_context *mlpeer,
					   int link);
	QDF_STATUS (*mlo_mlme_ext_mlpeer_disconnect)(
				struct wlan_mlo_peer_context *ml_peer);
#endif /* WLAN_MLO_SETUP_LINK_RECFG */
};

/*
 * struct mlo_osif_ext_ops - MLO manager to OSIF callback functions
 * @mlo_mgr_osif_update_bss_info: Callback to update each link connection info.
 * @mlo_mgr_osif_update_mac_addr: Callback to notify MAC addr update complete
 *                                from old link id to new link id for the vdev.
 * @mlo_roam_osif_update_mac_addr: Callback to notify MAC addr update during
				   roam sync for the vdev.
 * @mlo_mgr_osif_link_rej_update_mac_addr: Callback to notify MAC addr update
 *                                for link rejection.
 * @mlo_mgr_osif_link_switch_notification: Notify OSIF on start of link switch
 */
struct mlo_osif_ext_ops {
	QDF_STATUS
	(*mlo_mgr_osif_update_bss_info)(struct qdf_mac_addr *self_mac,
					struct qdf_mac_addr *bssid,
					int32_t link_id);

	QDF_STATUS (*mlo_mgr_osif_update_mac_addr)(int32_t ieee_old_link_id,
						   int32_t ieee_new_link_id,
						   uint8_t vdev_id);
	QDF_STATUS (*mlo_roam_osif_update_mac_addr)(struct wlan_objmgr_vdev *vdev,
						    struct qdf_mac_addr *old_self_mac,
						    struct qdf_mac_addr *new_self_mac);

	QDF_STATUS (*mlo_mgr_osif_link_rej_update_mac_addr)(uint8_t ieee_rej_link_id,
				     uint8_t ieee_acc_link_id, uint8_t vdev_id);

	QDF_STATUS
	(*mlo_mgr_osif_link_switch_notification)(struct wlan_objmgr_vdev *vdev,
						 uint8_t non_trans_vdev_id,
						 bool is_start_notify);
};

/* maximum size of vdev bitmap array for MLO link set active command */
#define MLO_VDEV_BITMAP_SZ 2

/* maximum size of link number param array for MLO link set active command */
#define MLO_LINK_NUM_SZ 2

/**
 * struct mlo_link_set_active_resp: MLO link set active response structure
 * @status: Return status, 0 for success, non-zero otherwise
 * @evt_handled: response event is handled
 * @active_sz: size of current active vdev bitmap array
 * @active: current active vdev bitmap array
 * @inactive_sz: size of current inactive vdev bitmap array
 * @inactive: current inactive vdev bitmap array
 * @use_ieee_link_id: link id is valid in active_linkid_bitmap or
 *	inactive_linkid_bitmap
 * @ap_mld_mac_addr: AP MLD mac address
 * @active_linkid_bitmap: current forced active link id bitmap
 * @inactive_linkid_bitmap: current forced inactive link id bitmap
 * @curr_inactive_linkid_bitmap: current inactive link id bitmap
 * @curr_active_linkid_bitmap: current active link id bitmap
 */
struct mlo_link_set_active_resp {
	uint32_t status;
	bool evt_handled;
	uint32_t active_sz;
	uint32_t active[MLO_VDEV_BITMAP_SZ];
	uint32_t inactive_sz;
	uint32_t inactive[MLO_VDEV_BITMAP_SZ];
	bool use_ieee_link_id;
	struct qdf_mac_addr  ap_mld_mac_addr;
	uint32_t active_linkid_bitmap;
	uint32_t inactive_linkid_bitmap;
	uint32_t curr_inactive_linkid_bitmap;
	uint32_t curr_active_linkid_bitmap;
};

/**
 * struct mlo_link_num_param: MLO link set active number params
 * @num_of_link: number of links to active/inactive
 * @vdev_type: type of vdev
 * @vdev_subtype: subtype of vdev
 * @home_freq: home frequency of the link
 */
struct mlo_link_num_param {
	uint32_t num_of_link;
	uint32_t vdev_type;
	uint32_t vdev_subtype;
	uint32_t home_freq;
};

/*
 * struct mlo_control_flags: This structure is used for setting
 * wmi_mlo_control_flags.
 * @overwrite_force_active_bitmap: indicate overwrite all earlier force_active
 * bitmaps
 * @overwrite_force_inactive_bitmap: indicate overwrite all earlier
 * force_inactive bitmaps
 * @dynamic_force_link_num: indicate fw to use force link number instead of
 * force link bitmaps
 * @post_re_evaluate: run link state check again after command response event
 * handled
 * @post_re_evaluate_loops: current re-evaluate count if this set link is
 * from set link event respone handler
 * @dont_reschedule_workqueue: don't reschedule force scc workqueue
 * after set link response
 */
struct mlo_control_flags {
	bool overwrite_force_active_bitmap;
	bool overwrite_force_inactive_bitmap;
	bool dynamic_force_link_num;
	bool post_re_evaluate;
	uint8_t post_re_evaluate_loops;
	bool dont_reschedule_workqueue;
};

/* struct ml_link_force_cmd - force command for links
 * @ap_mld_mac_addr: AP mld mac address
 * @ieee_link_id_bitmap: link id bitmap
 * valid for WMI_MLO_LINK_FORCE_ACTIVE, WMI_MLO_LINK_FORCE_INACTIVE,
 * WMI_MLO_LINK_NO_FORCE, WMI_MLO_LINK_FORCE_ACTIVE_LINK_NUM,
 * WMI_MLO_LINK_FORCE_INACTIVE_LINK_NUM.
 * @ieee_link_id_bitmap2: link id bitmap, only valid for
 * WMI_MLO_LINK_FORCE_ACTIVE_INACTIVE carry the inactive linkid bitmap
 * @link_num: link num
 * only valid on WMI_MLO_LINK_FORCE_ACTIVE_LINK_NUM or
 * WMI_MLO_LINK_FORCE_INACTIVE_LINK_NUM
 */
struct ml_link_force_cmd {
	struct qdf_mac_addr ap_mld_mac_addr;
	uint16_t ieee_link_id_bitmap;
	uint16_t ieee_link_id_bitmap2;
	uint8_t link_num;
};

/**
 * struct mlo_link_set_active_param: MLO link set active params
 * @force_mode: operation to take (enum mlo_link_force_mode)
 * @reason: reason for the operation (enum mlo_link_force_reason)
 * @num_link_entry: number of the valid entries for link_num
 * @num_vdev_bitmap: number of the valid entries for vdev_bitmap
 * @num_inactive_vdev_bitmap: number of the valid entries for
 *  inactive_vdev_bitmap
 * @link_num: link number param array
 *  It's present only when force_mode is MLO_LINK_FORCE_MODE_ACTIVE_NUM or
 *  MLO_LINK_FORCE_MODE_INACTIVE_NUM
 * @vdev_bitmap: active/inactive vdev bitmap array
 *  It will be present when force_mode is MLO_LINK_FORCE_MODE_ACTIVE,
 *  MLO_LINK_FORCE_MODE_INACTIVE, MLO_LINK_FORCE_MODE_NO_FORCE,
 *  MLO_LINK_FORCE_MODE_ACTIVE_NUM or MLO_LINK_FORCE_MODE_INACTIVE_NUM,
 *  and MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE.
 *  For MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE, it includes the active vdev
 *  bitmaps
 * @inactive_vdev_bitmap: inactive vdev bitmap array
 *  It will be present when force_mode is MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
 *  it includes the inactive vdev bitmaps
 * @control_flags: This structure is used for setting wmi_mlo_control_flags.
 * @use_ieee_link_id: send link id bitmap to target.
 *  If this value is true, the "force_cmd" field should be provided and
 *  that will be sent to target
 * @force_cmd: force command which includes link id bitmap
 * @num_disallow_mode_comb: Number of disallowed mode link bitmap combinations
 * @disallow_mode_link_bmap: MLO link disallowed mode link bitmap
 */
struct mlo_link_set_active_param {
	uint32_t force_mode;
	uint32_t reason;
	uint32_t num_link_entry;
	uint32_t num_vdev_bitmap;
	uint32_t num_inactive_vdev_bitmap;
	struct mlo_link_num_param link_num[MLO_LINK_NUM_SZ];
	uint32_t vdev_bitmap[MLO_VDEV_BITMAP_SZ];
	uint32_t inactive_vdev_bitmap[MLO_VDEV_BITMAP_SZ];
	struct mlo_control_flags control_flags;
	bool use_ieee_link_id;
	struct ml_link_force_cmd force_cmd;
	uint32_t num_disallow_mode_comb;
	struct ml_link_disallow_mode_bitmap disallow_mode_link_bmap[MAX_DISALLOW_BMAP_COMB];
};

/**
 * struct mlo_link_set_active_ctx - Context for MLO link set active request
 * @vdev: pointer to vdev on which the request issued
 * @set_mlo_link_cb: callback function for MLO link set active request
 * @validate_set_mlo_link_cb: callback to validate set link request
 * @cb_arg: callback context
 */
struct mlo_link_set_active_ctx {
	struct wlan_objmgr_vdev *vdev;
	void (*set_mlo_link_cb)(struct wlan_objmgr_vdev *vdev, void *arg,
				struct mlo_link_set_active_resp *evt);
	QDF_STATUS (*validate_set_mlo_link_cb)(
			struct wlan_objmgr_psoc *psoc,
			struct mlo_link_set_active_param *param);
	void *cb_arg;
};

/**
 * struct mlo_link_set_active_req - MLO link set active request
 * @ctx: context for MLO link set active request
 * @param: MLO link set active params
 */
struct mlo_link_set_active_req {
	struct mlo_link_set_active_ctx ctx;
	struct mlo_link_set_active_param param;
};

/**
 * enum mlo_chip_recovery_type - MLO chip recovery types
 * @MLO_RECOVERY_MODE_0: CRASH_PARTNER_CHIPS & recover all chips
 * @MLO_RECOVERY_MODE_1: Crash & recover asserted chip alone
 * with SLO and legacy peers on Non-asserted chips
 * @MLO_RECOVERY_MODE_2: Crash & recover asserted chip alone with
 * with SLO/legacy and MLO peers on Non-asserted chips
 * @MLO_RECOVERY_MODE_MAX: Max limit for recovery types
 */
enum mlo_chip_recovery_type {
	MLO_RECOVERY_MODE_0 = 1,
	MLO_RECOVERY_MODE_1 = 2,
	MLO_RECOVERY_MODE_2 = 3,

	/* Add new types above */
	MLO_RECOVERY_MODE_MAX = 0xf
};

/**
 * enum wlan_t2lm_status - Target status codes in event of t2lm
 * @WLAN_MAP_SWITCH_TIMER_TSF: Mapping switch time value in TSF to be included
 * in probe response frames
 * @WLAN_MAP_SWITCH_TIMER_EXPIRED: Indication that the new proposed T2LM has
 * been applied, Update the required data structures and other modules.
 * @WLAN_EXPECTED_DUR_EXPIRED: Indication that the proposed T2LM ineffective
 * after this duration and all TIDs fall back to default mode.
 */
enum wlan_t2lm_status {
	WLAN_MAP_SWITCH_TIMER_TSF,
	WLAN_MAP_SWITCH_TIMER_EXPIRED,
	WLAN_EXPECTED_DUR_EXPIRED,
};

/**
 * struct mlo_vdev_host_tid_to_link_map_resp - TID-to-link mapping response
 * @vdev_id: Vdev id
 * @status: Target status for t2lm ie info
 * @mapping_switch_tsf: Mapping switch time in tsf for probe response frames
 */
struct mlo_vdev_host_tid_to_link_map_resp {
	uint8_t vdev_id;
	enum wlan_t2lm_status status;
	uint32_t mapping_switch_tsf;
};

/**
 * struct mlo_link_removal_cmd_params - MLO link removal command parameters
 * @vdev_id: vdev ID of the link to be removed
 * @reconfig_ml_ie: Entire ML reconfiguration element
 * @reconfig_ml_ie_size: size of the field @reconfig_ml_ie
 */
struct mlo_link_removal_cmd_params {
	uint8_t vdev_id;
	uint8_t *reconfig_ml_ie;
	uint32_t reconfig_ml_ie_size;
};

/**
 * struct mlo_link_removal_tbtt_info - MLO link removal TBTT info. This
 * information will be in correspondence with an outgoing beacon instance.
 * @tbtt_count: AP removal timer TBTT count in the reported beacon
 * @qtimer_reading: Q-timer reading when the reported beacon is sent out
 * @tsf: TSF of the reported beacon
 */
struct mlo_link_removal_tbtt_info {
	uint32_t tbtt_count;
	uint64_t qtimer_reading;
	uint64_t tsf;
};

/**
 * struct mlo_link_removal_evt_params - MLO link removal event parameters
 * @vdev_id: vdev ID of the link undergoing removal
 * @tbtt_info: TBTT information of the link undergoing removal
 */
struct mlo_link_removal_evt_params {
	uint8_t vdev_id;
	struct mlo_link_removal_tbtt_info tbtt_info;
};

/**
 * struct mgmt_rx_mlo_link_removal_info - Information, sent in MGMT Rx event, of
 * a link undergoing removal from its MLD
 * @vdev_id: Vdev ID of the link undergoing removal
 * @hw_link_id: HW link ID of the link undergoing removal
 * @tbtt_count: AP removal timer TBTT count of the link undergoing removal
 */
struct mgmt_rx_mlo_link_removal_info {
	uint8_t vdev_id;
	uint8_t hw_link_id;
	uint16_t tbtt_count;
};

/**
 * struct mlo_link_disable_request_evt_params - MLO link disable
 * request params
 * @mld_addr: disable mld address
 * @link_id_bitmap: Disable Link id bitmap
 */
struct mlo_link_disable_request_evt_params {
	struct qdf_mac_addr mld_addr;
	uint32_t link_id_bitmap;
};

#define MAX_INDEX_FOR_LINK_PRIORITY_BITMAP 5
#define NUM_TID_PER_AC 2
/**
 * struct mlo_tlt_selection_evt_params - MLO tlt selection
 * request params
 * @mld_addr: mld address
 * @link_priority: link priority order based on hw chip id
 * @link_bmap: Link priority bitmap
 */
struct mlo_tlt_selection_evt_params {
	struct qdf_mac_addr mld_addr;
	uint32_t link_priority[MAX_INDEX_FOR_LINK_PRIORITY_BITMAP];
	uint32_t link_bmap[NUM_TID_PER_AC];
};

#define MAX_LINK_SWITCH_TLV 5
/**
 * struct mlo_link_switch_params - Structure to hold link State switch
 * related parameters
 * @mld_addr: MLD address
 * @active_link_bitmap: Bitmap of ieee link id for active links
 * @prev_link_bitmap: Bitmap of ieee link id for previous active links
 * @fw_timestamp: Firmware timestamp in milliseconds
 * @reason_code: Reason code for the switch
 */
struct mlo_link_switch_params {
	struct qdf_mac_addr mld_addr;
	uint32_t active_link_bitmap;
	uint32_t prev_link_bitmap;
	uint32_t fw_timestamp;
	uint32_t reason_code;
};

/**
 * struct mlo_link_switch_state_info  - Structure to hold the link switch
 * related parameters corresponding to all the TLV received in link state switch
 * event.
 * @num_params: Number of the link switch parameters
 * @link_switch_param: Link switch parameters
 */
struct mlo_link_switch_state_info {
	uint8_t num_params;
	struct mlo_link_switch_params link_switch_param[MAX_LINK_SWITCH_TLV];
};

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * struct peer_ptqm_migrate_entry - peer ptqm migrate entry
 * @ml_peer_id: ML peer id
 * @hw_link_id: HW link id
 */
struct peer_ptqm_migrate_entry {
	uint16_t ml_peer_id;
	uint16_t hw_link_id;
};

/**
 * struct peer_ptqm_migrate_params - peer ptqm migrate request parameter
 * @vdev_id: vdev id
 * @num_peers: peer count
 * @num_peers_failed: number of peers for which wmi cmd is failed.
 * This value is expected to be used only in case failure is returned by WMI
 * @peer_list: list of peers to be migrated
 */
struct peer_ptqm_migrate_params {
	uint8_t vdev_id;
	uint16_t num_peers;
	uint16_t num_peers_failed;
	struct peer_ptqm_migrate_entry *peer_list;
};

/**
 * struct peer_ptqm_migrate_list_entry - peer ptqm migrate list
 * @node: QDF list node member
 * @peer: objmgr peer object
 * @mlo_peer_id: mlo peer id
 * @new_hw_link_id: hw link id of new primary
 */
struct peer_ptqm_migrate_list_entry {
	qdf_list_node_t node;
	struct wlan_objmgr_peer *peer;
	uint32_t mlo_peer_id;
	uint8_t new_hw_link_id;
};

/**
 * struct peer_migrate_ptqm_multi_entries - multi ptqm migrate peer entry params
 * @num_entries: Number of entries in the peer_list list
 * @peer_list: List to hold the peer entries to be migrated
 * @rssi_data: RSSI data of all the HW links
 *
 */
struct peer_migrate_ptqm_multi_entries {
	uint16_t num_entries;
	qdf_list_t peer_list;
	const struct mlo_all_link_rssi *rssi_data;
};

enum primary_link_peer_migration_evenr_status {
	PRIMARY_LINK_PEER_MIGRATION_SUCCESS,
	PRIMARY_LINK_PEER_MIGRATION_IN_PROGRESS,
	PRIMARY_LINK_PEER_MIGRATION_DELETE_IN_PROGRESS,
	PRIMARY_LINK_PEER_MIGRATION_DELETED,
	PRIMARY_LINK_PEER_MIGRATION_TX_PIPES_FAILED,
	PRIMARY_LINK_PEER_MIGRATION_RX_PIPES_FAILED,
	PRIMARY_LINK_PEER_MIGRATION_NOT_REQUIRED,

	/* Add any new status above this line */
	PRIMARY_LINK_PEER_MIGRATION_FAIL = 255,
};

/**
 * struct peer_ptqm_migrate_event_params - peer ptqm migrate event parameter
 * @vdev_id: vdev id
 * @num_peers: peer count
 */
struct peer_ptqm_migrate_event_params {
	uint8_t vdev_id;
	uint16_t num_peers;
};

/**
 * struct peer_entry_ptqm_migrate_event_params - peer entry ptqm migrate
 * event parameter
 * @ml_peer_id: ML peer id
 * @status: migration status
 */
struct peer_entry_ptqm_migrate_event_params {
	uint16_t ml_peer_id;
	enum primary_link_peer_migration_evenr_status status;
};
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */

/**
 * struct wlan_mlo_sta_entry - MLO sta entry
 * @mac_node: QDF list mac_node member
 * @time: To indicate when the peer mld mac is added to list
 * @peer_mld_addr: MLO peer MAC address
 */

struct wlan_mlo_sta_entry {
	qdf_list_node_t mac_node;
	qdf_time_t time;
	struct qdf_mac_addr peer_mld_addr;
};

#endif
