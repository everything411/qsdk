/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_CMN_H
#define ATH12K_DP_CMN_H

#include "cmn_defs.h"
#include "hw.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ath12k_cmn_extn.h"
#endif
#include <linux/hashtable.h>
#include <linux/kernel.h>

/* Max number of links for MLO connection */
#define ATH12K_DP_PEER_MAX_MLO_LINKS 5
#define ATH12K_DATA_TID_MAX 8

struct ath12k_hw_group;
struct ath12k_sta;
struct ath12k;

struct dp_srng {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	int size;
	u32 ring_id;
	u8 cached;
	u32 num_entries;
};

struct ath12k_dp_hw_link {
	u8 device_id;
	u8 pdev_idx;
};

/* skb_active_profile in linux skbuff.h used to set DP_RX_BUFFER_SIZE
 * Recycler with ipq52xx or ipq96xx non-minent
 *                --> headroom removed --> NET_SKB_PAD added
 *                --> 1664+64 = 1728
 * Recycler without above case
 *                --> headroom present --> padding not added
 *                --> 1856 (32-bit) or 1984 (64-bit)
 * Recycler not enabled --> (1664 + NET_SKB_PAD) allocated
 */
#ifdef CONFIG_SKB_RECYCLER
#if (CONFIG_SKB_RECYCLE_SIZE == 1664)	/* ipq52xx or ipq96xx non-minent */
#define DP_RX_BUFFER_SIZE		((skb_active_profile->value) + NET_SKB_PAD)
#else					/* other profiles */
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#define DP_RX_BUFFER_SIZE		2048
#else
#define DP_RX_BUFFER_SIZE		(skb_active_profile->value)
#endif
#endif
#else					/* recycler not enabled */
#define DP_RX_BUFFER_SIZE		(1664 + NET_SKB_PAD)
#endif

#define ATH12K_PAGE_SIZE	PAGE_SIZE

/* Total 1024 entries in PPT, i.e 4K/4 considering 4K aligned
 * SPT pages which makes lower 12bits 0
 */
#define ATH12K_MAX_PPT_ENTRIES	1024

/* Total 512 entries in a SPT, i.e 4K Page/8 */
#define ATH12K_MAX_SPT_ENTRIES	512

#define ATH12K_PPEDS_TX_SPT_PAGE_OFFSET 0
#define ATH12K_TX_SPT_PAGE_OFFSET ATH12K_NUM_PPEDS_TX_SPT_PAGES
#define ATH12K_TX_SPT_OFFSET ATH12K_NUM_PPEDS_TX_SPT_PAGES
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#define ATH12K_NUM_PPEDS_TX_SPT_PAGES \
	(ATH12K_NUM_POOL_PPEDS_TX_DESC_DEFAULT / ATH12K_MAX_SPT_ENTRIES)
#else
#define ATH12K_NUM_PPEDS_TX_SPT_PAGES 0
#endif

#define ATH12K_DP_RX_DESC_MAGIC	0xBABABABA

/* 4K aligned address have last 12 bits set to 0, this check is done
 * so that two spt pages address can be stored per 8bytes
 * of CMEM (PPT)
 */
#define ATH12K_SPT_4K_ALIGN_CHECK 0xFFF
#define ATH12K_SPT_4K_ALIGN_OFFSET 12
#define ATH12K_PPT_ADDR_OFFSET(ppt_index) (4 * (ppt_index))

/* To indicate HW of CMEM address, b0-31 are cmem base received via QMI */
#define ATH12K_CMEM_ADDR_MSB 0x10

/* Of 20 bits cookie, b0-b8 is to indicate SPT offset and b9-19 for PPT */
#define ATH12K_CC_SPT_MSB 8
#define ATH12K_CC_PPT_MSB 19
#define ATH12K_CC_PPT_SHIFT 9
#define ATH12K_DP_CC_COOKIE_SPT	GENMASK(8, 0)
#define ATH12K_DP_CC_COOKIE_PPT	GENMASK(19, 9)

#define MAX_DP_PEER_LIST_SIZE  16384
#define ATH12K_MAX_PEER_ID	2048
#define ATH12K_MAX_STA_ID	1536
#define ATH12K_MAX_STATS_ID	1624
#define DP_TCL_SFE_NUM_RING_MAX  MIN(NR_CPUS, 5)
#define ATH12K_DP_RX_REGULAR_RING_MAX MIN(NR_CPUS, 5)
#define DP_REO_DST_RING_MAX  (ATH12K_DP_RX_REGULAR_RING_MAX + 1)
#define ATH12K_DP_RX_ROAMING_RING1 ATH12K_DP_RX_REGULAR_RING_MAX
/*
 * Reserve a dedicated stats index for PPEDS stats
 * The ATH12K_DP_RX_ROAMING_RING1 is specific for wifi8.
 * For wifi7, it can be used for DS stats
 */
#define DP_TCL_PPEDS_RING_IDX	DP_TCL_SFE_NUM_RING_MAX
#define DP_REO_PPEDS_RING_IDX	ATH12K_DP_RX_REGULAR_RING_MAX
#define DP_TCL_NUM_RING_MAX	(DP_TCL_SFE_NUM_RING_MAX + 1)
#define DP_TCL_DESC_TYPE_MAX 2
#define ATH12K_MAX_AHVIF_ID	255
#define ATH12K_INVALID_AHVIF_ID	0
#define DP_REO_ERR_RINGS_MAX 4
#define DP_TOTAL_REO_DST_RINGS (DP_REO_DST_RING_MAX + DP_REO_ERR_RINGS_MAX)

#define ATH12K_DP_PCP_TID_MAP_SIZE	8
/* Unified TID map precedence: 0 = DSCP wins, 1 = PCP wins.
 * HLOS is always present but implicitly enabled by other features.
 */
#define ATH12K_DP_MAX_TID_PRECEDENCE_VAL 1

/* Hash table size: 2^11 = 2048 buckets for up to 2048 peers */
#define ATH12K_DP_PEER_HASH_BITS 11
#define ATH12K_DP_HW_STATS_REO_IDX 0

/* Hash table size for link peers: 2^11 = 2048 buckets */
#define ATH12K_DP_LINK_PEER_HASH_BITS 11

struct ath12k_dp_hw {
	struct ath12k_dp_peer __rcu *dp_peer_list[MAX_DP_PEER_LIST_SIZE];
	DECLARE_BITMAP(free_peer_id_map, ATH12K_MAX_PEER_ID);
	DECLARE_BITMAP(free_sta_id_map, ATH12K_MAX_STA_ID);
	u16 last_peer_id;
	u16 last_sta_id;

	/* Lock for protection of linked list of ath12k_dp_peer*/
	spinlock_t peer_list_lock;
	struct list_head peers;

	/* Lock for protection of dp_peer_list and hashtable */
	spinlock_t peer_hash_lock;
	/* Generic hash table for fast MAC address lookup */
	DECLARE_HASHTABLE(peer_hash, ATH12K_DP_PEER_HASH_BITS);

	/*
	 * TQM cleanup list — peers awaiting TQM queue drain.
	 * peer_id is unique: at most one entry per peer_id at any time.
	 * Lifetime: peer create →  peer delete/TQM cleanup
	 */
	struct list_head tqm_cleanup_list;
};

struct ath12k_dp_hw_group {
	struct ath12k_dp_hw_link hw_links[ATH12K_GROUP_MAX_RADIO];
	struct ath12k_dp *dp[ATH12K_MAX_SOCS];
	struct dp_rx_fst *fst;
	/* Number of active FSE rules; shared across all SoCs in the group */
	u32 fse_active_count;
	u8 *tx_status_buf[ATH12K_HW_MAX_QUEUES];
	u8 *rx_status_buf[DP_TOTAL_REO_DST_RINGS];
	struct ath12k_spt_info *spt_info;
	u32 num_spt_pages;
	struct ath12k_tx_desc_info **txbaddr;
	struct list_head tx_desc_free_list[ATH12K_HW_MAX_QUEUES];
	struct list_head tx_spl_desc_free_list[ATH12K_HW_MAX_QUEUES];
	u32 __percpu *tx_desc_used_cnt;
	/* protects the free and used desc lists */
	spinlock_t tx_desc_lock[ATH12K_HW_MAX_QUEUES];
	bool tx_desc_initialized;
	/* protects shared TX SPT page and descriptor pool initialization across SOCs */
	struct mutex tx_init_lock;
	struct device *tx_spt_dev;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	struct ath12k_ppeds_tx_desc_info **ppedstxbaddr;
	struct list_head ppeds_tx_desc_free_list;
	struct list_head ppeds_tx_desc_reuse_list;
	int ppeds_tx_desc_reuse_list_len;
	/* protects the ppeds free and reuse desc lists */
	spinlock_t ppeds_tx_desc_lock;
	bool ppeds_tx_desc_initialized;
	/* protects shared PPEDS TX SPT page and descriptor pool init across SOCs */
	struct mutex ppeds_tx_init_lock;
	struct device *ppeds_tx_spt_dev;
	struct ath12k_spt_info *ppeds_spt_info;
	u32 ppeds_num_spt_pages;
	u32 __percpu *ppeds_tx_desc_used_cnt;
#endif
	u8  pcp_tid_map[ATH12K_DP_PCP_TID_MAP_SIZE];
	u8  tid_map_precedence;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_dp_hw_group_extn extn;
#endif

	/* ST (Seamless transition): single in-flight ext_ctx parking
	 * Used for SLO where target peer doesn't exist during PREPARE.
	 * For MLO, target peer exists so direct transfer is used instead.
	 * SMD BSS Transition: parked Rx Q state (analogous to smd_parked_ext_ctx).
	 * Allocated during PREP phase.
	 * Consumed and freed during EXEC phase.
	 * Protected by smd_transition_lock.
	 */
	spinlock_t smd_transition_lock;
	struct ath12k_dp_peer_ext_ctx *smd_parked_ext_ctx;
	struct ath12k_dp_smd_parked_rx_info *smd_parked_rx_info;
	u8 smd_target_mld_addr[ETH_ALEN];
	/* POC: Reuse peer_id to avoid TQM FLOW Q update */
	u16 smd_old_peer_id;
	/* true from EXEC_RESP until abort/complete */
	bool smd_exec_in_progress;

	/* Keep Last */
	u8 arch_data[] __aligned(sizeof(void *));
};

/* TODO: Move this to a seperate dp_stats file */
struct ath12k_per_peer_tx_stats {
	u32 succ_bytes;
	u32 retry_bytes;
	u32 failed_bytes;
	u32 duration;
	u16 succ_pkts;
	u16 retry_pkts;
	u16 failed_pkts;
	u16 ru_start;
	u16 ru_tones;
	u16 succ_mpdu_pkts;
	u16 retried_mpdu_pkts;
	u16 mpdu_tried;
	u8 ba_fails;
	u8 ppdu_type;
	u8 rate_idx;
	u8 mu_grpid;
	u8 mu_pos;
	u8 rate;
	u8 bw;
	u8 flags;
	u8 mcs;
	u8 tid;
	u8 nss;
	bool is_ampdu;
	bool stbc;
	bool ldpc;
};

struct ath12k_dp_peer_create_params {
	struct ieee80211_sta *sta;
	bool is_mlo;
	bool is_vdev_peer;
	u8 hw_link_id;
	bool is_sta_bss_peer;
	u16 peer_id;
	u16 sta_id;
	bool is_epp_peer;
};

struct ath12k_dp_link_peer_rate_info {
	struct rate_info txrate;
	struct rate_info rxrate;
	u64 rx_duration;
	u64 tx_duration;
	s8 rssi_comb;
	s8 signal_avg;
	u16 tx_retry_count;
	u16 tx_retry_failed;
	u32 rx_retries;
};

enum wme_ac {
	WME_AC_BE,
	WME_AC_BK,
	WME_AC_VI,
	WME_AC_VO,
	WME_NUM_AC
};

struct ath12k_dp_link_peer;
struct ath12k_dp_link_peer_rate_info;
struct ath12k_dp_aggr_vif_stats;
void ath12k_dp_cmn_device_deinit(struct ath12k_dp *dp);
int ath12k_dp_cmn_device_init(struct ath12k_dp *dp);
void ath12k_dp_cmn_hw_group_unassign(struct ath12k_dp *dp,
				     struct ath12k_hw_group *ag);
void ath12k_dp_cmn_hw_group_assign(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag);
int ath12k_dp_srng_alloc_aligned(struct ath12k_base *ab,
				 struct dp_srng *ring,
				 int num_entries,
				 int entry_sz,
				 bool cached);
void ath12k_dp_cmn_update_hw_links(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag,
				   struct ath12k *ar);
int ath12k_dp_link_peer_assign(struct ath12k *ar, u8 vdev_id,
			       struct ieee80211_sta *sta, u8 *addr, u8 link_id,
			       struct ieee80211_vif *vif, u8 vp_type, int vp_num,
			       bool mlo_bridge_peer);
void ath12k_dp_link_peer_unassign(struct ath12k *ar, struct ath12k_link_vif *arvif,
				  u8 *addr, struct ieee80211_sta *sta);
int
ath12k_dp_link_peer_batch_cleanup(struct ath12k *ar,
				  bool (*peer_match)(struct ath12k_dp_link_peer *,
						     void *),
				  void *context);
unsigned long ath12k_link_peer_last_active(struct ath12k_dp_link_peer *link_peer);
void ath12k_link_peer_get_sta_rate_info_stats(struct ath12k_dp_link_peer *link_peer,
					      struct ath12k_dp_link_peer_rate_info *rate_info);
int ath12k_dp_mon_init(struct ath12k_dp *dp);
void ath12k_dp_mon_deinit(struct ath12k_dp *dp);
void ath12k_dp_cp_link_peer_unassign(struct ath12k *ar, struct ath12k_link_vif *arvif,
				     struct ath12k_sta *ahsta, u8 link_id, u8 *addr,
				     bool update_bmap);
u16 ath12k_dp_peer_get_peer_id(struct ath12k_dp_hw *dp_hw, struct ieee80211_vif *vif,
			       u8 *addr);
u16 ath12k_dp_peer_get_sta_id(struct ath12k_dp_hw *dp_hw, struct ieee80211_vif *vif,
			      u8 *addr);

enum ath12k_dp_peer_param {
	ATH12K_DP_PEER_PEERID_PARAM,
	ATH12K_DP_PEER_MSCS_PARAM,
	ATH12K_DP_PEER_DMS_DISABLE_PARAM,
	ATH12K_DP_PEER_AUTHORIZE_PARAM,
	ATH12K_DP_PEER_PN_PARAMS,
	ATH12K_DP_PEER_KEYS_PARAM,
	ATH12K_DP_PEER_CLEAR_KEYS_PARAM,
	ATH12K_DP_PEER_MAC_ADDR_PARAM,
	ATH12K_DP_PEER_PRIMARY_LINK_ID_PARAM,
	ATH12K_DP_PEER_MAX_PARAM,
};

enum ath12k_dp_link_peer_param {
	ATH12K_DP_LINK_PEER_PEERID_PARAM,
	ATH12K_DP_LINK_PEER_ATF_PARAM,
	ATH12K_DP_LINK_PEER_AUTHORIZE_PARAM,
	ATH12K_DP_LINK_PEER_ASSOC_PARAM,
	ATH12K_DP_LINK_PEER_MAC_ADDR_PARAM,
	ATH12K_DP_LINK_PEER_IS_PRIMARY,
	ATH12K_DP_LINK_PEER_MIGRATION_PARAM,
	ATH12K_DP_LINK_PEER_TID_WEIGHT_PARAM,
	ATH12K_DP_LINK_PEER_TXRATE_PARAM,
	ATH12k_DP_LINK_PEER_MAX_PARAM,
};

/* different supported pkt types for routing */
enum ath12k_routing_pkt_type {
	ATH12K_PKT_TYPE_ARP_IPV4,
	ATH12K_PKT_TYPE_NS_IPV6,
	ATH12K_PKT_TYPE_IGMP_IPV4,
	ATH12K_PKT_TYPE_MLD_IPV6,
	ATH12K_PKT_TYPE_DHCP_IPV4,
	ATH12K_PKT_TYPE_DHCP_IPV6,
	ATH12K_PKT_TYPE_DNS_TCP_IPV4,
	ATH12K_PKT_TYPE_DNS_TCP_IPV6,
	ATH12K_PKT_TYPE_DNS_UDP_IPV4,
	ATH12K_PKT_TYPE_DNS_UDP_IPV6,
	ATH12K_PKT_TYPE_ICMP_IPV4,
	ATH12K_PKT_TYPE_ICMP_IPV6,
	ATH12K_PKT_TYPE_TCP_IPV4,
	ATH12K_PKT_TYPE_TCP_IPV6,
	ATH12K_PKT_TYPE_UDP_IPV4,
	ATH12K_PKT_TYPE_UDP_IPV6,
	ATH12K_PKT_TYPE_IPV4,
	ATH12K_PKT_TYPE_IPV6,
	ATH12K_PKT_TYPE_EAP,
	ATH12K_PKT_TYPE_MAX
};

struct ath12k_config_atf_params {
	u8 atf_group_index;
	u32 atf_peer_conf_airtime;
};

struct ath12k_dp_peer_keys_params {
	int len;
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];
};

struct ath12k_dp_peer_pn_params {
	u8 keyidx;
	struct ieee80211_key_conf *key;
};

/**
 * union ath12k_config_param - Generic parameter value container for CP-DP APIs
 *
 * This union serves as a type-safe, extensible value carrier used by the
 * Control Path to Data Path (CP-DP) interface APIs (ath12k_dp_peer_set/get_param
 * and ath12k_dp_link_peer_set/get_param variants).
 *
 * The actual member to read or write is determined by the accompanying
 * ath12k_dp_peer_param or ath12k_dp_link_peer_param enum value
 * passed alongside this union. Using a union allows a single, uniform API
 * signature to accommodate diverse parameter types (integers, flags, structs,
 * etc.) as new param types are introduced.
 *
 */
union ath12k_config_param {
	u16 peer_id;
	u8 addr[ETH_ALEN];
	bool is_authorized;
	struct ath12k_config_atf_params atf_params;
	struct cfg80211_qm_req_desc_data mscs_params;
	struct ath12k_dp_peer_keys_params keys_params;
	bool assoc_success;
	struct ath12k_dp_peer_pn_params pn_params;
	bool dms_disable;
	bool is_primary;
	u8 hw_link_id;
	u8 tid_weight[ATH12K_DATA_TID_MAX];
	struct ath12k_dp_link_peer_rate_info rate_info;
	u8 primary_link_id;
};

/*
 * Control path to Data path interface APIs for set and get params.
 *
 * Two lookup variants are provided for dp_peer-level operations:
 *
 *  - _by_dp_peer(): preferred when ath12k_sta is available in the control
 *    path; caller obtains dp_peer via ath12k_sta_get_dp_peer_wiphy_locked()
 *    and passes it directly, avoiding a peer table lookup.
 *
 *  - _by_mac_addr(): use when ath12k_sta is NOT available, e.g. for
 *    non-associated peers (bcast/mcast, AP self-peer), or paths that
 *    receive only a MAC address (debugfs, vendor commands, OEM callbacks).
 *    Internally takes peer_hash_lock and performs a hash lookup.
 */
int ath12k_dp_peer_set_param_by_dp_peer(void *ptr, enum ath12k_dp_peer_param param,
					union ath12k_config_param *val);
int ath12k_dp_peer_get_param_by_dp_peer(void *ptr, enum ath12k_dp_peer_param param,
					union ath12k_config_param *val);

int ath12k_dp_peer_set_param_by_mac_addr(struct ath12k_dp_hw *dp_hw,
					 struct ieee80211_vif *vif,
					 const u8 *addr,
					 enum ath12k_dp_peer_param param,
					 union ath12k_config_param *val);
int ath12k_dp_peer_get_param_by_mac_addr(struct ath12k_dp_hw *dp_hw,
					 struct ieee80211_vif *vif,
					 const u8 *addr,
					 enum ath12k_dp_peer_param param,
					 union ath12k_config_param *val);
int ath12k_dp_peer_get_param_by_peer_id(struct ath12k_pdev_dp *dp_pdev, u16 peer_id,
					enum ath12k_dp_peer_param param,
					union ath12k_config_param *val);
/*
 * Control path to Data path interface APIs for link peer set and get params.
 *
 * Four lookup variants are provided for dp_link_peer-level operations,
 * differing in how the dp_peer and dp_link_peer are resolved:
 *
 *  - _by_dp_peer_and_link_mac(): preferred when ath12k_sta is available and
 *    the target link is identified by its MAC address. Caller obtains dp_peer
 *    via ath12k_sta_get_dp_peer_wiphy_locked() and passes it directly,
 *    avoiding a peer table lookup; link_peer is resolved by link MAC.
 *
 *  - _by_dp_peer_and_link_id(): preferred when ath12k_sta and ath12k_link_sta
 *    are both available. Caller obtains dp_peer via
 *    ath12k_sta_get_dp_peer_wiphy_locked() and the logical link_id from
 *    arsta; no peer table lookup needed.
 *
 *  - _by_mld_and_link_mac(): use when ath12k_sta is NOT available and only
 *    the MLD MAC and link MAC addresses are known, e.g. for non-associated
 *    peers, or paths driven by MAC addresses (debugfs, vendor commands, OEM
 *    callbacks). Internally takes peer_hash_lock and performs two hash lookups
 *    (MLD MAC -> dp_peer, link MAC -> link_peer).
 *
 *  - _by_mld_mac_and_link_id(): use when ath12k_sta is NOT available and
 *    only the MLD MAC and logical link_id are known. Same cost as
 *    _by_mld_and_link_mac() but resolves the link_peer by link ID instead
 *    of link MAC.
 */
int ath12k_dp_link_peer_set_param_by_dp_peer_and_link_mac(void *ptr, const u8 *link_mac,
							  enum ath12k_dp_link_peer_param param,
							  union ath12k_config_param *val);
int ath12k_dp_link_peer_get_param_by_dp_peer_and_link_mac(void *ptr, const u8 *link_mac,
							  enum ath12k_dp_link_peer_param param,
							  union ath12k_config_param *val);

int ath12k_dp_link_peer_set_param_by_dp_peer_and_link_id(void *ptr, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val);
int ath12k_dp_link_peer_get_param_by_dp_peer_and_link_id(void *ptr, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val);

int ath12k_dp_link_peer_set_param_by_mld_and_link_mac(struct ath12k_dp_hw *dp_hw,
						      struct ieee80211_vif *vif,
						      const u8 *mld_mac,
						      const u8 *link_mac,
						      enum ath12k_dp_link_peer_param param,
						      union ath12k_config_param *val);

int ath12k_dp_link_peer_get_param_by_mld_and_link_mac(struct ath12k_dp_hw *dp_hw,
						      struct ieee80211_vif *vif,
						      const u8 *mld_mac,
						      const u8 *link_mac,
						      enum ath12k_dp_link_peer_param param,
						      union ath12k_config_param *val);

int ath12k_dp_link_peer_set_param_by_mld_mac_and_link_id(struct ath12k_dp_hw *dp_hw,
							 struct ieee80211_vif *vif,
							 const u8 *mld_mac, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val);
int ath12k_dp_link_peer_get_param_by_mld_mac_and_link_id(struct ath12k_dp_hw *dp_hw,
							 struct ieee80211_vif *vif,
							 const u8 *mld_mac, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val);

/**
 * ath12k_sta_get_dp_peer_wiphy_locked() - Get dp_peer with wiphy lock held
 * @ahsta: pointer to ath12k_sta
 *
 * This function retrieves the dp_peer pointer in control path context
 * where wiphy lock is already held. Uses wiphy_dereference() which
 * provides proper RCU dereference with wiphy lock protection.
 *
 * Context: Must be called with wiphy lock held
 * Return: pointer to ath12k_dp_peer or NULL
 */
void *ath12k_sta_get_dp_peer_wiphy_locked(struct wiphy *wiphy, struct ath12k_sta *ahsta);

/**
 * ath12k_sta_get_dp_peer_rcu() - Get dp_peer with RCU read lock held
 * @ahsta: pointer to ath12k_sta
 *
 * This function retrieves the dp_peer pointer in datapath/interrupt
 * context where RCU read lock must be held. This is typically used in
 * interrupt handlers, NAPI contexts, or other data path operations.
 *
 * Context: Must be called with rcu_read_lock held
 * Return: pointer to ath12k_dp_peer or NULL
 */
void *ath12k_sta_get_dp_peer_rcu(struct ath12k_sta *ahsta);

struct ath12k_4addr_params {
	u16 ast_hash;
	u16 hw_peer_id;
	u16 tcl_metadata;
};

bool ath12k_dp_peer_set_4addr_params(void *ptr, int ppe_vp_num);
int ath12k_dp_link_peer_get_4addr_params(void *ptr, const u8 *addr,
					 struct ath12k_4addr_params *params);
int ath12k_dp_peer_set_key_config(struct ath12k_pdev_dp *dp_pdev, const u8 *addr,
				  enum set_key_cmd cmd, struct ieee80211_key_conf *key,
				  struct ieee80211_sta *sta, struct ieee80211_vif *vif,
				  enum hal_encrypt_type *enctype);
void ath12k_dp_peer_cleanup_all(struct ath12k *ar);
void ath12k_dp_vif_peer_stats_update(struct ath12k_dp_hw *dp_hw,
				     struct ath12k_pdev_dp *dp_pdev,
				     const u8 *dp_peer_addr,
				     u8 hw_link_id,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_dp_aggr_vif_stats *aggr_vif_stats);
#endif
