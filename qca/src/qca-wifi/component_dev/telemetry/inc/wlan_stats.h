/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WLAN_STATS_H_
#define _WLAN_STATS_H_

#include <wlan_stats_define.h>

/**
 * Length of SoC interface name passed to user space as soc<psoc_id>
 * including null caracter.
 **/
#define SOC_IF_LEN 5

/**
 * Length of Radio interface name passed to user space as wifi<pdev_id>
 * including null caracter.
 **/
#define RADIO_IF_LEN 6

/**
 * Deriving feature indexes corresponding to feature attributes defined in
 * qca_wlan_vendor_attr_feat dynamically.
 * Feature attribute values starts from 1. So, Deduction of 1 is required to
 * start the index from 0.
 **/
#define DEF_INX(_x) \
	INX_FEAT_##_x = (QCA_WLAN_VENDOR_ATTR_FEAT_##_x - 1)

/**
 * This is to get qca_wlan_vendor_attr_feat attributes from feat_index_e.
 * So, the addition of 1 is required to get corresponding attribute.
 */
#define GET_ATTR(_x) ((_x) + 1)

/**
 * enum stats_feat_index_e: Defines stats feature indexes
 * This will auto generate each index value corresponding to that feature
 * attribute defined in qca_wlan_vendor_attr_feat.
 */
enum stats_feat_index_e {
	DEF_INX(ME),
	DEF_INX(RX),
	DEF_INX(TX),
	DEF_INX(AST),
	DEF_INX(CFR),
	DEF_INX(FWD),
	DEF_INX(RAW),
	DEF_INX(TSO),
	DEF_INX(TWT),
	DEF_INX(VOW),
	DEF_INX(WDI),
	DEF_INX(WMI),
	DEF_INX(IGMP),
	DEF_INX(LINK),
	DEF_INX(MESH),
	DEF_INX(RATE),
	DEF_INX(NAWDS),
	DEF_INX(DELAY),
	DEF_INX(JITTER),
	DEF_INX(TXCAP),
	DEF_INX(MONITOR),
	DEF_INX(SAWFDELAY),
	DEF_INX(SAWFTX),
	DEF_INX(DETER),
	DEF_INX(WMM),
	DEF_INX(MAX),
};

/**
 * struct stats_config: Structure to hold user configurations
 * @wiphy:  Pointer to wiphy structure which came as part of User request
 * @feat:  Feat flag set to dedicated bit of this field
 * @request_id: Indicate the request ID of non-blocking stats request
 * @lvl:  Requested level of Stats (i.e. Basic, Advance or Debug)
 * @obj:  Requested stats for object (i.e. AP, Radio, Vap or STA)
 * @type:  Requested stats category
 * @aggregate: Aggregate in driver
 * @serviceid: service id for checking the level of sawf stats
 * @mld_req: Flag to indicate if request is received for MLD interface
 * @mld_link: Flag to indicate if request is received for link stats on MLD
 * @peer_type: type of peer
 * @intf_name: Interface name for which stats are requested
 * @async_req: Indicate the request for non-blocking stats
 * @link_id: Link id of a MLD object
 * @resolve_sta: Indicate Host driver to find the vdev to which STA is connected
 */
struct stats_config {
	struct wiphy           *wiphy;
	u_int64_t              feat;
	u_int64_t              request_id;
	enum stats_level_e     lvl;
	enum stats_object_e    obj;
	enum stats_type_e      type;
	bool                   aggregate;
	u_int8_t               serviceid;
	bool                   mld_req;
	bool                   mld_link;
	enum stats_peer_type   peer_type;
	char                   intf_name[IFNAMSIZ];
	bool                   async_req;
	u_int8_t               link_id;
	bool                   resolve_sta;
};

/**
 * struct stats_list_entry: Structure used to represent an entry in
 * the non-blocking stats work list
 * @node : linked list node
 * @pdev : Pointer to the PDEV object of the request
 * @vdev : Pointer to the VDEV object of the request
 * @cfg  : User configuration for the request
 * @mac  : Mac address of peer for peer stats
 */
struct stats_list_entry {
	qdf_list_node_t node;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	struct stats_config *cfg;
	uint8_t mac[QDF_MAC_ADDR_SIZE];
};

/**
 * struct stats_work_context: Structure representing the context of stat work
 * @work           : Instance of work
 * @list_lock      : lock for the work list
 * @nb_stats_work_list: queue of non-blocking stats requests
 * @num_entries    : number of entries in the queue
 * @is_initialized : flag to track init and deinit of context
 */
struct stats_work_context {
	qdf_work_t work;
	qdf_spinlock_t list_lock;
	qdf_list_t nb_stats_work_list;
	bool is_initialized;
};

/**
 * struct multi_reply_ctx: Structure to manage multi reply message
 * @next_copy_from: Copy from this index
 * @pending: Flag to detect pending data from previous reply
 * @start_inx: Index from which the stats will be processed
 **/
struct multi_reply_ctx {
	size_t next_copy_from;
	bool pending;
	uint8_t start_inx;
};

/**
 * struct unified_stats: Structure to carry all feature specific stats in driver
 *                       level for stats response setup
 * All features are void pointers and its corresponding sizes.
 * This can hold Basic or Advance or Debug structures independently.
 */
struct unified_stats {
	void *feat[INX_FEAT_MAX];
	u_int32_t size[INX_FEAT_MAX];
};

/**
 * struct iterator_ctx: Structure is used internaly for iteration over all
 *                      peer/vdev to aggregate the stats
 * @pvt: Void pointer to carry stats config
 * @stats: Pointer to unified stats
 */
struct iterator_ctx {
	void *pvt;
	struct unified_stats *stats;
};

/**
 * wlan_stats_get_peer_stats(): Function to get peer specific stats
 * @psoc:  Pointer to Vdev object
 * @peer_mac:  Pointer to Peer mac
 * @cfg:  Pointer to stats config came as part of user request
 * @stats:  Pointer to unified stats object
 *
 * Return: QDF_STATUS_SUCCESS for success and Error code for failure
 */
QDF_STATUS wlan_stats_get_peer_stats(struct wlan_objmgr_vdev *vdev,
				     uint8_t *peer_mac,
				     struct stats_config *cfg,
				     struct unified_stats *stats);

/**
 * wlan_stats_get_vdev_stats(): Function to get vdev specific stats
 * @psoc:  Pointer to Psoc object
 * @vdev:  Pointer to Vdev object
 * @cfg:  Pointer to stats config came as part of user request
 * @stats:  Pointer to unified stats object
 *
 * Return: QDF_STATUS_SUCCESS for success and Error code for failure
 */
QDF_STATUS wlan_stats_get_vdev_stats(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_vdev *vdev,
				     struct stats_config *cfg,
				     struct unified_stats *stats);

/**
 * wlan_stats_get_pdev_stats(): Function to get pdev specific stats
 * @psoc:  Pointer to Psoc object
 * @pdev:  Pointer to Pdev object
 * @cfg:  Pointer to stats config came as part of user request
 * @stats:  Pointer to unified stats object
 *
 * Return: QDF_STATUS_SUCCESS for success and Error code for failure
 */
QDF_STATUS wlan_stats_get_pdev_stats(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_pdev *pdev,
				     struct stats_config *cfg,
				     struct unified_stats *stats);

/**
 * wlan_stats_get_psoc_stats(): Function to get psoc specific stats
 * @psoc:  Pointer to Psoc object
 * @cfg:  Pointer to stats config came as part of user request
 * @stats:  Pointer to unified stats object
 *
 * Return: QDF_STATUS_SUCCESS for success and Error code for failure
 */
QDF_STATUS wlan_stats_get_psoc_stats(struct wlan_objmgr_psoc *psoc,
				     struct stats_config *cfg,
				     struct unified_stats *stats);

/**
 * wlan_stats_is_recursive_valid(): Function to check recursiveness
 * @cfg:  Pointer to stats config came as part of user request
 * @obj:  The object for which recursiveness is being checked
 *
 * So, this function will check if requested feature is valid for
 * underneath objects.
 *
 * Return: True if Recursive is possible or false if not
 */
bool wlan_stats_is_recursive_valid(struct stats_config *cfg,
				   enum stats_object_e obj);

/**
 * wlan_stats_free_unified_stats(): Function to free all feature holder pointers
 * @stats:  Pointer to unified stats object
 *
 * Return: None
 */
void wlan_stats_free_unified_stats(struct unified_stats *stats);

/**
 * wlan_stats_get_vdev_from_sta_mac(): Function for peer lookup
 * @mac: Mac address of peer
 *
 * This function helps to search peer from MLD context or across psoc.
 * If found returns vdev for MLD assoc peer or legacy peer.
 *
 * Return: vdev pointer in success or NULL in failure
 */
struct wlan_objmgr_vdev *wlan_stats_get_vdev_from_sta_mac(uint8_t *mac);

/**
 * wlan_stats_get_tlv_counts_and_total_length(): Function to get valid feature
 * counts and total size of stats packed
 * @stats: Pointer to unified_stats
 * @tlv_count: Pointer to hold valid feature counts
 *
 * This function is useful to pre-calculate the buffer size before allocation
 * to pack entire stats data.
 *
 * Return: Total length of stats data packed
 */
uint32_t wlan_stats_get_tlv_counts_and_total_length(struct unified_stats *stats,
						    uint8_t *tlv_count);

/**
 * wlan_stats_nb_stats_work_attach: API to allocate work for handling
 *                       non-blocking stats request
 */
void wlan_stats_nb_stats_work_attach(void);

/**
 * wlan_stats_schedule_nb_stats_work: API to queue for scheduling non-blocking
 *                         stats request
 * Return: QDF_STATUS_SUCCESS for success and Error code for failure
 */
QDF_STATUS wlan_stats_schedule_nb_stats_work(struct wlan_objmgr_pdev *pdev,
					     struct wlan_objmgr_vdev *vdev,
					     struct stats_config *cfg,
					     uint8_t *mac);

/**
 * wlan_stats_nb_stats_work_detach: API to deallocate work for handling
 *                       non-blocking stats request
 */
void wlan_stats_nb_stats_work_detach(void);
#endif /* _WLAN_STATS_H_ */
