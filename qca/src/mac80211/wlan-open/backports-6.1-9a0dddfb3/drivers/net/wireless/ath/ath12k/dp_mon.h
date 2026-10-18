/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_MON_H
#define ATH12K_DP_MON_H

#include "core.h"
#include "dp_peer.h"
#include "debug.h"
#include "pktlog.h"

#include "hal_mon_cmn.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ath12k_cmn_extn.h"
#include "qcn_extns/dp_stats_extn.h"
#include "qcn_extns/dp_mon_extn.h"
#include "qcn_extns/ini.h"
#endif /* CPTCFG_QCN_EXTN */
#include "dp_tx_mon.h"

#define DP_RXDMA_MONITOR_BUF_RING_SIZE(ab) \
	((ab)->mem_params.dp_params.rxdma_monitor_buf_ring_size)
#define DP_RXDMA_MONITOR_DST_RING_SIZE(ab) \
	((ab)->mem_params.dp_params.rxdma_monitor_dst_ring_size)
#define DP_MON_NUM_PPDU_DESC(ab) \
	((ab)->mem_params.dp_params.mon_num_ppdu_desc)
#define DP_TX_MON_BUF_RING_SIZE(ab) \
	((ab)->mem_params.dp_params.tx_monitor_buf_ring_size)
#define DP_TX_MON_BUF_RING_FILL_LVL(ab) \
	((ab)->mem_params.dp_params.tx_monitor_buf_ring_fill_lvl)
#define DP_TX_MON_DST_RING_SIZE(ab) \
	((ab)->mem_params.dp_params.tx_monitor_dst_ring_size)
#define DP_TX_MON_NUM_PPDU_DESC(ab) \
	((ab)->mem_params.dp_params.tx_monitor_num_ppdu_desc)
#define DP_TX_MON_NUM_STATUS_BUF(ab) \
	((ab)->mem_params.dp_params.tx_monitor_num_status_buf)
#define DP_TX_MON_SPC_BITMAP(ab) \
	((ab)->mem_params.dp_params.tx_monitor_spc_bitmap)

#ifndef CPTCFG_QCN_EXTN

#define DP_RXDMA_MON_STATUS_RING_SIZE(ab)	({ (void)(ab); 1024; })
#define DP_RXDMA_MONITOR_DESC_RING_SIZE(ab)	({ (void)(ab); 4096; })

#else
#define DP_RXDMA_MON_STATUS_RING_SIZE(ab)	ath12k_cfg_get(ab, \
	ATH12K_INI_DP_RXDMA_MON_STATUS_RING_SIZE)
#define DP_RXDMA_MONITOR_DESC_RING_SIZE(ab)	ath12k_cfg_get(ab, \
	ATH12K_INI_DP_RXDMA_MONITOR_DESC_RING_SIZE)
#endif

#define ATH12K_DP_MON_TX_BUF_SIZE	2048
#define ATH12K_DP_MON_RX_BUF_SIZE	2048
#define ATH12K_MON_MAGIC_VALUE		0xDECAFEED
#define ATH12K_DP_MON_MAX_RADIO_TAP_HDR 128
#define ATH12K_RX_MON_MSDU_CNT_SIZE		sizeof(u16)
#define ATH12K_RX_MON_MSDU_MARKER_SIZE		sizeof(u16)
#define ATH12K_RX_MON_PF_TAG_SIZE		(sizeof(u16) * 2)
#define ATH12K_RX_MON_FSE_TAG_MASK		0xffff
#define ATH12K_RX_MON_MAX_MSDU			16
#define ATH12K_RX_MON_MSDU_MARKER		0xFEED
#define ATH12K_RX_MON_MAX_METADATA_HDR	(ATH12K_RX_MON_MSDU_MARKER_SIZE + \
					 ATH12K_RX_MON_MSDU_CNT_SIZE + \
					 (ATH12K_RX_MON_MAX_MSDU * \
					  ATH12K_RX_MON_PF_TAG_SIZE))
#define ATH12K_RX_MON_MAX_VENDOR_TLV_DATA \
			(sizeof(struct ieee80211_radiotap_vendor_tlv) + \
			 ATH12K_RX_MON_MSDU_CNT_SIZE + \
			 ATH12K_RX_MON_MAX_MSDU * ATH12K_RX_MON_PF_TAG_SIZE)
#define ATH12K_MON_RX_DOT11_OFFSET	5
#define ATH12K_MON_RX_PKT_OFFSET	8
#define ATH12K_DP_WLAN_MAX_AC		4

#define	ATH12K_DP_MON_MIN_FRAGS_RESTITCH	2
#define	ATH12K_DP_MON_L3_HDR_PAD		2
#define	ATH12K_DP_MON_NONRAW_L2_HDR_PAD_BYTE	2
#define	ATH12K_DP_MON_RAW_L2_HDR_PAD_BYTE	0
#define	ATH12K_DP_MON_LLC_SIZE			3
#define	ATH12K_DP_MON_SNAP_SIZE			5
#define	ATH12K_DP_MON_DECAP_HDR_SIZE		14
#define	ATH12K_DP_MON_KEYIV			0x20
#define ATH12K_DP_MON_ETH_TYPE_VLAN_LEN		4
#define ATH12K_DP_MON_ETH_TYPE_DOUBLE_VLAN_LEN	8
#define DP_RXDMA_MONITOR_DEFAULT_RING_FILL_LVL 512

#define ATH12K_DP_SMART_MON_FILTER_DEFAULT \
	((ab)->mem_params.dp_params.smart_mon_filter_default)

#define DP_TX_MONITOR_BUF_SIZE		2048
#define DP_TX_MONITOR_BUF_SIZE_MIN	48
#define DP_TX_MONITOR_BUF_SIZE_MAX	8192

#define DP_RX_MON_BUFFER_SIZE		2048
#define RX_MON_STATUS_BASE_BUF_SIZE	2048
#define RX_MON_STATUS_BUF_ALIGN		128
#define RX_MON_STATUS_BUF_RESERVATION	128
#define RX_MON_STATUS_BUF_SIZE		(RX_MON_STATUS_BASE_BUF_SIZE - \
				 (RX_MON_STATUS_BUF_RESERVATION + \
				  RX_MON_STATUS_BUF_ALIGN + \
				  SKB_DATA_ALIGN(sizeof(struct skb_shared_info))))
#define DP_NOT_PPDU_ID_WRAP_AROUND 20000

#define DP_MON_RXDMA_BUF_COOKIE_BUF_ID		GENMASK(17, 0)
#define DP_MON_RXDMA_BUF_COOKIE_PDEV_ID 	GENMASK(19, 18)
#define DP_MON_RX_HDR_LEN			128

#define DP_SMART_MON_FILTER_MASK        0x0F  /* lower nibble: C/M/D/V */
#define DP_SMART_MON_PROFILE_MASK       0xF0  /* upper nibble: profile */

#define DP_SMART_MON_PROFILE_UNSPEC     0x00
#define DP_SMART_MON_PROFILE_1G         0x10
#define DP_SMART_MON_PROFILE_512M       0x20
#define DP_SMART_MON_PROFILE_256M       0x30

#define DP_SMART_MON_VALID       BIT(0)

#define ATH12K_DP_MON_STATUS_BUF   (ab->mem_params.dp_params.dp_mon_status_buf)

#ifndef CPTCFG_QCN_EXTN
#define ATH12K_EXT_MON_MAX_PEERS	16
#define ATH12K_EXT_MON_NUM_PEERS(ab)	((void)(ab), ATH12K_EXT_MON_MAX_PEERS)
#else
#define ATH12K_EXT_MON_MAX_PEERS	ATH12K_DP_EXT_MON_NUM_PEER_MAX
#define ATH12K_EXT_MON_NUM_PEERS(ab)	ath12k_cfg_get(ab, \
	ATH12K_CFG_DP_EXT_MON_NUM_PEER)
#endif

#define ATH12K_EXT_MON_FILTER_ALL	0xFFFFU

#define ATH12K_EXT_MON_METADATA_RTAP_HDR	BIT(0)
#define ATH12K_EXT_MON_METADATA_META_HDR	BIT(1)
#define ATH12K_EXT_MON_METADATA_OFFCHAN_PKT	BIT(2)
#define ATH12K_EXT_MON_METADATA_VALID_MASK \
	(ATH12K_EXT_MON_METADATA_RTAP_HDR | \
	 ATH12K_EXT_MON_METADATA_META_HDR | \
	 ATH12K_EXT_MON_METADATA_OFFCHAN_PKT)

#define ATH12K_EXT_MON_DEFAULT_PEER_BITMAP	0xFF
#define ATH12K_EXT_MON_PKT_CAP_ALL_PROTOS	0x3F
#define ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX	0xF

#define ATH12K_FC0_TYPE_SHIFT		2
#define ATH12K_FC0_SUBTYPE_SHIFT	4

struct ath12k_mon_data;
struct dp_mon_rx_filter;
struct dp_mon_tx_filter;
struct ath12k_ext_mon_config;
struct ath12k_ext_mon_pkt_config;
struct ath12k_dp_rx_ext_mon;
struct ath12k_dp_tx_ext_mon_config;

struct ath12k_dp_mon_pad_params {
	u32 frag_size;
	u32 msdu_llc_len;
	u32 pad_byte_holder;
	u32 frag_idx;
	bool is_head_msdu;
};

struct dp_rxdma_mon_ring {
	struct dp_srng refill_buf_ring;
	struct idr bufs_idr;
	/* Protects bufs_idr */
	spinlock_t idr_lock;
	int bufs_max;
	int bufs_fill_lvl;
};

struct dp_mon_desc_list_params {
	/* Lock for  @free_list */
	spinlock_t *desc_lock;
	struct list_head *free_list;
	struct list_head *list_local;
	struct page_frag_cache *pf_cache;
	size_t buff_size;
	bool is_tx_monitor;
};

enum dp_mon_stats_mode {
	ATH12k_DP_MON_BASIC_STATS,
	ATH12k_DP_MON_EXTD_STATS
};

enum dp_punctured_modes {
	NO_PUNCTURE,
	PUNCTURED_20MHZ,
	PUNCTURED_40MHZ,
	PUNCTURED_80MHZ,
	PUNCTURED_120MHZ,
	PUNCTURED_MODE_CNT,
};

#define PUNCTURE_80MHZ_MASK 0xF
#define PUNCTURE_160MHZ_MASK 0xFF
#define PUNCTURE_320MHZ_MASK 0xFFFF
#define PUNCTURE_40MHZ_MASK 0x3

/*
 * punc_type:
 * Type of puncturing denoting the number of bits that are punctured.
 * Each bit represents a 20MHz channel and therefore, each enum represents
 * the number of 20MHz channels that are punctured.
 */
enum punc_type {
	PUNC_NONE        = 0,
	PUNC_MINUS20MHZ  = 1,
	PUNC_MINUS40MHZ  = 2,
	PUNC_MINUS60MHZ  = 3,
	PUNC_MINUS80MHZ  = 4,
	PUNC_MINUS100MHZ = 5,
	PUNC_MINUS120MHZ = 6,
	PUNC_INVALID,
};

struct ath12k_dp_arch_mon_ops {
	int (*rx_srng_setup)(struct ath12k_dp *dp);
	void (*rx_srng_cleanup)(struct ath12k_dp *dp);
	int (*rx_ring_init)(struct ath12k_dp *dp);
	void (*rx_ring_deinit)(struct ath12k_dp *dp);
	int (*rx_htt_srng_setup)(struct ath12k_dp *dp);
	int (*mon_pdev_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_pdev_free)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_pdev_rx_srng_setup)(struct ath12k_pdev_dp *dp_pdev,
				      u32 mac_id);
	void (*mon_pdev_rx_srng_cleanup)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_pdev_rx_htt_srng_setup)(struct ath12k_pdev_dp *dp_pdev,
					  u32 mac_id);
	void (*mon_pdev_rx_attach)(struct ath12k_pdev_dp *dp_pdev);
	int (*setup_mon_link_desc)(struct ath12k_pdev_dp *dp_pdev);
	void (*cleanup_mon_link_desc)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_pdev_rx_mpdu_list_init)(struct ath12k_mon_data *pmon);
	int (*mon_rx_srng_process)(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				      struct napi_struct *napi, int *budget);
	int (*update_telemetry_stats)(struct ath12k_base *ab,
				       const int pdev_id);
	int (*rx_filter_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_filter_free)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_stats_enable)(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode);
	void (*rx_stats_disable)(struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_stats_mode mode);
	int (*rx_filter_update)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_monitor_mode_set)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_monitor_mode_reset)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_undecoded_metadata_config_filter)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_undecoded_metadata_reset_filter)(struct ath12k_pdev_dp *dp_pdev);
	int  (*rx_undecoded_metadata_capture_set)(struct ath12k_pdev_dp *dp_pdev,
					      u32 value);
	int  (*rx_undecoded_phy_err_mask_set)(struct ath12k_pdev_dp *dp_pdev,
					      u32 mask, u32 mask_cont);
	int (*setup_ppdu_desc)(struct ath12k_pdev_dp *pdev_dp);
	void (*cleanup_ppdu_desc)(struct ath12k_pdev_dp *pdev_dp);
	int (*mon_rx_wq_init)(struct ath12k_pdev_dp *pdev_dp);
	void (*mon_rx_wq_deinit)(struct ath12k_pdev_dp *pdev_dp, bool destroy);
	void (*rx_nrp_set)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_nrp_reset)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_smart_mon_set)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_smart_mon_reset)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_cfr_rcc_config)(struct ath12k_pdev_dp *dp_pdev,
				  bool enable, u8 rcc_type);
	bool (*rx_get_cfr_rcc_filter_valid)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_rx_wmask)(void *ptr, struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_enable_packet_filters)(void *ptr,
						struct htt_rx_ring_tlv_filter *filter);
	void (*pktlog_config)(struct ath12k_pdev_dp *dp_pdev,
			      enum ath12k_pktlog_mode mode,
			      u32 filter, bool enable);
	void (*htt_rx_filter_rxmon_cfg)(void *ptr,
					struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*htt_rx_phy_err_filter_cfg)(void *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter);
	int (*ext_mon_validate_request)(struct ath12k_pdev_dp *dp_pdev,
					const struct ath12k_ext_mon_config *req);
	int (*ext_mon_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*ext_mon_free)(struct ath12k_pdev_dp *dp_pdev);
	void (*ext_mon_setup_rx_filter)(struct htt_rx_ring_tlv_filter *tlv_filter,
					const struct ath12k_dp_rx_ext_mon *rx_ext_mon);

	/* Below are TxMonitor ops */
	int (*mon_tx_srng_alloc_setup)(struct ath12k_dp *dp);
	void (*mon_tx_srng_cleanup)(struct ath12k_dp *dp);
	int (*mon_tx_srng_init_setup)(struct ath12k_dp *dp);
	int (*mon_tx_htt_srng_setup)(struct ath12k_dp *dp);
	void (*mon_tx_htt_srng_cleanup)(struct ath12k_dp *dp);
	int (*mon_tx_filter_configure)(struct ath12k_pdev_dp *dp_pdev, bool state);
	int (*mon_tx_filter_update)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_tx_dst_ring_alloc_setup)(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
	void (*mon_tx_dst_ring_cleanup)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_tx_wq_start)(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
	void (*mon_tx_wq_stop)(struct ath12k_pdev_dp *dp_pdev);

	/*
	 * NULL on architectures that don't extend the command beyond WiFi7
	 * length; only WiFi8/Boron currently populates words 13-14.
	 */
	void (*htt_tx_mon_cfg_fill_extended_wmask)
		(struct htt_tx_mon_ring_selection_cfg_cmd *cmd,
		 const struct htt_tx_ring_tlv_filter *htt_tlv_filter);

	int (*ext_mon_tx_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*ext_mon_tx_free)(struct ath12k_pdev_dp *dp_pdev);
	int (*ext_mon_filter)(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *mpdu);
	int (*ext_mon_add_wmi_tx_peers)(struct ath12k_pdev_dp *dp_pdev,
					struct ath12k_dp_ext_mon_tx_peer_params
					*peer_param);
	int (*ext_mon_remove_wmi_tx_peers)(struct ath12k_pdev_dp *dp_pdev,
					   struct ath12k_dp_ext_mon_tx_peer_params
					   *peer_param);
	u32 (*mon_tx_get_spc_bitmap)(struct ath12k_base *ab);
	u8 (*ext_mon_get_filter_mode)(struct ath12k_pdev_dp *dp_pdev);
};

/**
 * enum hal_mon_end_reason - HAL monitor descriptor completion reasons
 * @HAL_MON_STATUS_BUFFER_FULL: Monitor status buffer reached capacity limit
 * @HAL_MON_FLUSH_DETECTED: Hardware detected flush condition, forcing completion
 * @HAL_MON_END_OF_PPDU: Normal PPDU completion, all data successfully captured
 * @HAL_MON_PPDU_TRUNCATED: PPDU was truncated due to buffer or hardware limits
 *
 * This enumeration defines the possible reasons why hardware completes a
 * monitor descriptor, as reported in the monitor destination ring descriptor.
 * These values are used by both RX and TX monitor functionality across
 * different WiFi architectures (WiFi7/WiFi8).
 */
enum hal_mon_end_reason {
	HAL_MON_STATUS_BUFFER_FULL,
	HAL_MON_FLUSH_DETECTED,
	HAL_MON_END_OF_PPDU,
	HAL_MON_PPDU_TRUNCATED,
};

struct ath12k_dp_mon {
	struct ath12k_dp *dp;
	struct dp_rxdma_mon_ring rxdma_mon_buf_ring;
	struct dp_rxdma_mon_ring rx_mon_status_refill_ring[MAX_RXDMA_PER_PDEV];
	struct dp_srng rxdma_mon_desc_ring;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	u32 mon_dest_ring_stuck_cnt;
	struct ath12k_dp_mon_desc *mon_desc_pool;
	struct list_head mon_desc_free_list;

	/* lock for mon_desc_pool */
	spinlock_t mon_desc_lock;
	struct page_frag_cache rx_mon_pf_cache;

	u32 num_frag_replenish;
	u32 num_frag_free;

	struct ath12k_dp_tx_mon *dp_tx_mon;
	u32 mon_status_ring_size;
	u32 mon_desc_ring_size;
	u32 mon_buf_ring_size;
	u32 mon_dst_ring_size;
	u32 mon_num_ppdu_desc;
};

enum dp_monitor_type {
	ATH12K_DP_MON_TYPE_QUAD_RING,
	ATH12K_DP_MON_TYPE_DUAL_RING
};

enum ath12k_dp_smart_mon_state {
	ATH12K_DP_SMART_MON_DISABLED,
	ATH12K_DP_SMART_MON_IDLE,
	ATH12K_DP_SMART_MON_ACTIVE,
};

struct ath12k_dp_mon_mpdu_meta {
	u8 decap_type;
	u8 truncated:1,
	   full_pkt:1,
	   fcs_err:1;
	u32 err_bitmap;
};

enum dp_mon_tx_ppdu_info_type {
	DP_MON_TX_PROT_PPDU_INFO,
	DP_MON_TX_DATA_PPDU_INFO
};

enum dp_mon_tx_medium_protection_type {
	DP_MON_TX_MEDIUM_NO_PROTECTION,
	DP_MON_TX_MEDIUM_RTS_LEGACY,
	DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW,
	DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW,
	DP_MON_TX_MEDIUM_CTS2SELF,
	DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR,
	DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR
};

enum dp_mon_status_buf_state {
	DP_MON_STATUS_MATCH,
	DP_MON_STATUS_NO_DMA,
	DP_MON_STATUS_LAG,
	DP_MON_STATUS_LEAD,
	DP_MON_STATUS_REPLINISH,
};

struct dp_mon_qosframe_addr4 {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctrl;
	u8 addr4[ETH_ALEN];
	__le16 qos_ctrl;
} __packed;

struct dp_mon_frame_min_one {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
} __packed;

struct dp_mon_packet_info {
	u64 cookie;
	u32 dma_length:12,
	    rsvd1:4,
	    msdu_continuation:1,
	    truncated:1,
	    rsvd2:14;
	u32 rsvd3;
};

struct dp_mon_mpdu {
	struct list_head list;
	struct sk_buff *head;
	struct sk_buff *tail;
	u32 err_bitmap;
	u8 decap_format;
};

/**
 * struct dp_mon_tx_ppdu_info - TX monitor PPDU information structure
 * @is_used: Flag indicating if this PPDU info structure is currently in use
 * @tx_info: HAL layer TX monitor PPDU information containing hardware-specific
 *           data including TLV parsing results, user status, and packet metadata
 * @dp_tx_mon_mpdu_list: List head for managing MPDUs associated with this PPDU.
 *                       Used to chain multiple MPDU structures for complex
 *                       aggregated transmissions
 * @tx_mon_mpdu: Pointer to the current MPDU structure being processed within
 *               this PPDU. Points to individual MPDU data for frame generation
 * @chan_freq: Channel frequency in MHz on which this PPDU was transmitted.
 *             Used for radiotap channel information and monitor mode delivery
 * @chan_num: Channel number corresponding to the transmission frequency.
 *            Provides channel context for frame analysis and filtering
 * @num_mpdu_fcs_ok: Count of MPDUs within this PPDU that passed FCS validation.
 *                   Used for statistics and determining transmission success rate
 * @buffer_addr: Virtual address of extracted packet buffer from hardware descriptor
 *   - Points to page fragment containing actual packet data
 *   - Ownership transferred from monitor descriptor during BUFFER_ADDR TLV processing
 *   - Set to NULL after fragment is added to SKB (when take_ref=false)
 *   - Used by ath12k_dp_tx_mon_generate_data_frm() to add fragments
 *   - Must be freed with page_frag_free() if not consumed by SKB
 * @buffer_length: Length of valid data in buffer_addr in bytes
 *   - Extracted from packet_info->dma_length during BUFFER_ADDR processing
 *   - Represents actual packet payload size, not buffer allocation size
 *   - Used as fragment length when adding to SKB via skb_add_rx_frag()
 *   - Must be <= ATH12K_DP_MON_TX_BUF_SIZE for validation
 * @msdu_continuation: Indicates if this buffer is part of a fragmented MSDU
 *   - true: More fragments follow for this MSDU
 *                     - false: This is the last (or only) fragment of the MSDU
 *                     - Extracted from packet_info->msdu_continuation
 *                     - Used for proper MSDU reassembly in multi-fragment scenarios
 * @truncated: Indicates if the packet data was truncated by hardware
 *             - true: Packet was larger than buffer size, data is incomplete
 *             - false: Complete packet data is available in buffer
 *             - Extracted from packet_info->truncated
 *             - Used for debugging and packet validation purposes
 * @has_buffer_data: Flag indicating if valid buffer data is available
 *      - true: buffer_addr contains valid packet data
 *      - false: No buffer data available (e.g., control frames without payload)
 *      - Set during BUFFER_ADDR TLV processing
 *      - Checked by ath12k_dp_tx_mon_generate_data_frm() before adding fragments
 *      - Reset to false after buffer ownership is transferred to SKB
 * @contains_host_frames: True if this PPDU contains host-generated frames

 */
struct dp_mon_tx_ppdu_info {
	bool is_used;
	struct hal_tx_mon_ppdu_info tx_info;
	struct list_head dp_tx_mon_mpdu_list;
	struct dp_mon_mpdu *tx_mon_mpdu;
	u16 chan_freq;
	u16 chan_num;
	u32 num_mpdu_fcs_ok;
	void *buffer_addr;
	u32 buffer_length;
	u32 frag_offset;
	struct page *page;
	bool msdu_continuation;
	bool truncated;
	bool has_buffer_data;
	bool contains_host_frames;
};

#define SNR_INVALID 255

#define AVG_MULTIPLIER BIT(8)
#define AVG_MUL(x, mul) ((x) * (mul))
#define AVG_RND(x, mul) ((((x) % (mul)) >= ((mul) / 2)) ? \
		((x) + ((mul) - 1)) / (mul) : (x) / (mul))

#define WEIGHTED_AVG_OUT(x) (AVG_RND((x), AVG_MULTIPLIER))
#define WEIGHTED_AVG_IN(x)  (AVG_MUL((x), AVG_MULTIPLIER))
#define AVG(x, y) ((((x) << 2) + (y) - (x)) >> 2)
#define WEIGHTED_AVG_UPDATE(x, y) ((x) = AVG((x), WEIGHTED_AVG_IN(y)))
#define MAX_PPDU_ID_HIST 128

struct ath12k_pdev_mon_stats {
	u32 status_ppdu_state;
	u32 status_ppdu_start;
	u32 status_ppdu_end;
	u32 status_ppdu_compl;
	u32 status_ppdu_start_mis;
	u32 status_ppdu_end_mis;
	u32 status_ppdu_done;
	u32 status_desc_invalid;
	u32 status_tlv_tag_err;
	u32 status_buf_done_war;
	u32 rx_err_desc_sanity_fail;
	u32 dest_ppdu_done;
	u32 dest_mpdu_done;
	u32 dest_mpdu_drop;
	u32 dup_mon_linkdesc_cnt;
	u32 dup_mon_buf_cnt;
	u32 empty_mon_sw_desc_cnt;
	u32 dest_mon_stuck;
	u32 dest_mon_not_reaped;
	u32 invalid_msdu_cnt;
	u32 ppdu_id_mismatch;
	u32 ppdu_id_match;
	u32 status_ring_ppdu_id_hist[MAX_PPDU_ID_HIST];
	u32 dest_ring_ppdu_id_hist[MAX_PPDU_ID_HIST];
	u32 ppdu_id_hist_idx;
};

#define DP_MON_MAX_STATUS_BUF 32

/**
 * struct ath12k_mon_data - Monitor mode data processing context
 * @link_desc_banks: Array of link descriptor banks for DMA buffer management
 *                   Used for efficient allocation and tracking of monitor buffers
 * @mon_ppdu_info: RX monitor PPDU information structure containing parsed
 *                 frame metadata, PHY parameters, and reception status
 * @mon_ppdu_status: Current PPDU processing status flags indicating parsing
 *                   state and completion status for RX monitor frames
 * @mon_last_buf_cookie: Cookie value of the last processed monitor buffer
 *                       Used for buffer tracking and leak detection
 * @mon_last_linkdesc_paddr: Physical address of last processed link descriptor
 *                           Used for descriptor chain validation and debugging
 * @chan_noise_floor: Channel noise floor measurement in dBm for signal quality
 *                    analysis and RSSI calculations in monitor mode
 * @err_bitmap: Bitmap of error conditions encountered during monitor processing
 *              Used for error tracking and debugging monitor frame issues
 * @decap_format: Decapsulation format for monitor frames (raw, native WiFi, etc.)
 *                Determines how captured frames are presented to upper layers
 * @rx_mon_stats: RX monitor statistics structure containing performance counters
 *                and error tracking for RX monitor functionality
 * @buf_state: Current state of monitor status buffer processing (idle, busy, etc.)
 *             Used for state machine management in monitor buffer handling
 * @mon_lock: Spinlock protecting concurrent access to monitor data structures
 *            Ensures thread safety between interrupt and process contexts
 * @rx_status_q: Queue of RX status sk_buffs awaiting processing or delivery
 *               Used for buffering monitor frames before mac80211 delivery
 * @mon_mpdu: Pointer to current MPDU being processed in monitor mode
 *            Contains frame data and metadata during active processing
 * @dp_rx_mon_mpdu_list: List of RX monitor MPDUs pending processing
 *                       Used for batching and efficient MPDU handling
 * @prot_status_info: TX monitor status information for protection frames
 *                    (RTS/CTS, Block ACK, etc.) containing timing and status data
 * @data_status_info: TX monitor status information for data frames containing
 *                    transmission parameters, retry counts, and completion status
 * @prot_ppdu_info: TX monitor PPDU information for protection frames including
 *                  PHY parameters, timing, and frame generation metadata
 * @data_ppdu_info: TX monitor PPDU information for data frames including
 *                  transmission parameters, MCS, and channel information
 * @rtap_vendor_tlv: Pointer to radiotap vendor-specific TLV data for ATH12K
 *                   chipset metadata including timing and hardware-specific info
 *
 * This structure serves as the central context for all monitor mode operations,
 * encompassing both RX and TX monitor functionality. It maintains state information,
 * statistics, and processing contexts required for efficient monitor frame handling.
 */
struct ath12k_mon_data {
	struct dp_link_desc_bank link_desc_banks[DP_LINK_DESC_BANKS_MAX];
	struct hal_rx_mon_ppdu_info mon_ppdu_info;

	u32 mon_ppdu_status;
	u32 mon_last_buf_cookie;
	u64 mon_last_linkdesc_paddr;
	u16 chan_noise_floor;
	u32 err_bitmap;
	u8 decap_format;

	struct ath12k_pdev_mon_stats rx_mon_stats;
	enum dp_mon_status_buf_state buf_state;
	/* lock for monitor data */
	spinlock_t mon_lock;
	struct sk_buff_head rx_status_q;
	struct dp_mon_mpdu *mon_mpdu;
	struct list_head dp_rx_mon_mpdu_list;
	struct hal_tx_mon_status_info prot_status_info;
	struct hal_tx_mon_status_info data_status_info;
	struct dp_mon_tx_ppdu_info prot_ppdu_info;
	struct dp_mon_tx_ppdu_info data_ppdu_info;
	struct ieee80211_radiotap_vendor_ns *rtap_vendor_tlv;
};

/**
 * struct ath12k_rtap_vendor_ns - ATH12K vendor-specific radiotap namespace
 * @lsig: Legacy Signal field containing PHY-level transmission parameters
 *        including data rate, length, and parity information from L-SIG
 * @device_id: Hardware device identifier for distinguishing between different
 *             ATH12K chipset variants and revisions in multi-device systems
 * @lsig_b: Legacy Signal B field containing additional PHY parameters for
 *          backward compatibility with 802.11b/g legacy rate information
 * @ppdu_start_timestamp: Hardware timestamp marking the start of PPDU
 *                        transmission, used for precise timing analysis
 *                        and frame correlation in monitor mode
 */
struct ath12k_rtap_vendor_ns {
	u32 lsig;
	u32 device_id;
	u32 lsig_b;
	u32 ppdu_start_timestamp;
} __packed;

struct ath12k_pdev_mon_dp_stats {
	u32 status_buf_reaped;
	u32 status_buf_processed;
	u32 status_buf_free;
	u32 status_buf_error_free;
	u32 ring_desc_empty;
	u32 ring_desc_flush;
	u32 ring_desc_trunc;
	u32 pkt_tlv_processed;
	u32 pkt_tlv_free;
	u32 pkt_tlv_error_free;
	u32 pkt_tlv_to_mac80211;
	u32 pkt_tlv_truncated;
	u32 pkt_tlv_reaped;
	u32 num_skb_alloc;
	u32 num_skb_free;
	u32 num_skb_to_mac80211;
	u32 num_ppdu_reaped;
	u32 num_ppdu_processed;
	u32 num_skb_raw;
	u32 num_frag_raw;
	u32 num_skb_eth;
	u32 num_frag_eth;
	u32 drop_tlv;
	u32 ppdu_desc_used;
	u32 ppdu_desc_proc;
	u32 ppdu_desc_free;
	u32 ppdu_desc_free_list_empty_cnt;
	u32 restitch_insuff_frags_cnt;
	u32 invalid_status_magic_num;
	u32 invalid_pkt_magic_num;
	u32 null_mpdu_q;
	u32 skb_alloc_fail;
	u32 rx_hdr_not_rcvd;
	u32 min_frags_unavailable;
	u32 invalid_mpdu_hdr_len;
	u32 invalid_in_use;
	u32 invalid_end_offset;
	u32 status_frag_add_to_skb;
};

/**
 * struct ath12k_dp_tx_mon - DP-level TX monitor context
 * @tx_mon_buf_ring: TX monitor refill buffer ring
 * @tx_mon_desc_pool: TX monitor descriptor pool
 * @tx_mon_desc_lock: Lock protecting descriptor pool and lists
 * @tx_mon_desc_free_list: Free-list for TX monitor descriptors
 * @tx_mon_pf_cache: Page-frag cache for TX monitor buffers
 * @tx_num_frag_replenish: Count of replenished TX monitor fragments
 * @tx_num_frag_free: Count of freed TX monitor fragments
 * @tx_mon_buf_ring_ready: Refill ring descriptor availability state
 * @tx_mon_stats: Buffer lifecycle statistics at SOC level
 */
struct ath12k_dp_tx_mon {
	struct dp_rxdma_mon_ring tx_mon_buf_ring;
	struct ath12k_dp_mon_desc *tx_mon_desc_pool;
	spinlock_t tx_mon_desc_lock;
	struct list_head tx_mon_desc_free_list;
	struct page_frag_cache tx_mon_pf_cache;
	u32 tx_num_frag_replenish;
	u32 tx_num_frag_free;
	bool tx_mon_buf_ring_ready;
	struct ath12k_dp_tx_mon_stats tx_mon_stats;
};

struct ath12k_pdev_mon_dp;

struct ath12k_dp_tx_ext_mon {
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon_config;
	spinlock_t tx_ext_mon_lock;	/* Protects tx_ext_mon_config */
};

/**
 * struct ath12k_pdev_tx_mon - TX monitor context
 * @mon_pdev: Back pointer to parent monitor pdev context
 * @dp_tx_mon: Back pointer to soc-level DP TX monitor context
 * @tx_mon_dst_ring: TX monitor destination ring
 * @tx_mon_filter: TX monitor filter table
 * @tx_monitor_started: TX monitor active state
 * @pdev_tx_mon_stats: TX monitor statistics
 * @txmon_wq: TX monitor worker queue
 * @txmon_work: TX monitor worker
 * @tx_mon_ppdu_desc_lock: Lock for TX monitor PPDU descriptors
 * @tx_mon_ppdu_desc_pool: TX monitor PPDU descriptor pool
 * @tx_mon_desc_work_list: TX monitor descriptor work list
 * @tx_mon_ppdu_desc_used_list: Used TX monitor PPDU descriptor list
 * @tx_mon_ppdu_desc_free_list: Free TX monitor PPDU descriptor list
 * @tx_mon_ppdu_desc_proc_list: Processing TX monitor PPDU descriptor list
 * @tx_mon_ppdu_desc_initialized: TX monitor PPDU descriptor pool init state
 * @tx_mon_wq_initialized: TX monitor workqueue init state
 * @tx_pktlog_hybrid: TX pktlog hybrid mode state
 * @tx_mon_teardown: TX monitor teardown state
 * @tx_ext_mon: Wrapper embedding the TX extended monitor config pointer and
 *		its protecting spinlock.
 */
struct ath12k_pdev_tx_mon {
	struct ath12k_pdev_mon_dp *mon_pdev;
	struct ath12k_dp_tx_mon *dp_tx_mon;
	struct ath12k_dp_tx_ext_mon tx_ext_mon;
	struct dp_srng tx_mon_dst_ring;
	struct dp_mon_tx_filter **tx_mon_filter;
	bool tx_monitor_started:1;
	u8 tx_monitor_mode;
	struct ath12k_pdev_tx_mon_stats pdev_tx_mon_stats;
	struct workqueue_struct *txmon_wq;
	struct work_struct txmon_work;
	spinlock_t tx_mon_ppdu_desc_lock;
	struct ath12k_dp_mon_ppdu_desc **tx_mon_ppdu_desc_pool;
	struct list_head tx_mon_desc_work_list;
	struct list_head tx_mon_ppdu_desc_used_list;
	struct list_head tx_mon_ppdu_desc_free_list;
	struct list_head tx_mon_ppdu_desc_proc_list;
	bool tx_mon_ppdu_desc_initialized:1;
	bool tx_mon_wq_initialized:1;
	bool tx_pktlog_hybrid;
	bool tx_mon_teardown;
};

/**
 * struct ath12k_dp_mon_status_desc - TX Monitor Status Descriptor
 * @paddr: Physical address of the monitor buffer
 * @mon_buf: Virtual address pointer to monitor buffer containing TLV data
 * @buf_len: Length of valid data in the monitor buffer
 * @end_of_ppdu: Flag indicating if this descriptor contains end of PPDU marker
 *
 * This structure represents a single status descriptor containing TLV fragments
 * from the TX monitor destination ring. Multiple status descriptors may be
 * required to represent a complete PPDU.
 *
 * The buffer pointed to by mon_buf contains raw TLV data from hardware that
 * needs to be parsed to extract PPDU information for frame generation.
 */
struct ath12k_dp_mon_status_desc {
	dma_addr_t paddr;
	u8 *mon_buf;
	u16 buf_len:12,
	    end_of_ppdu:1;
	u8 pkt_buf_cnt;
} __packed;

/**
 * struct ath12k_dp_mon_ppdu_desc - TX Monitor PPDU Descriptor
 * @list: List entry for PPDU descriptor management
 * @ppdu_id: Unique PPDU identifier from hardware
 * @timestamp: PPDU timestamp for correlation
 * @status_desc: Pointer to dynamically allocated array of status
 *               descriptors containing TLV data
 * @status_desc_cnt: Number of valid status descriptors in the array
 *
 * This structure represents a complete PPDU for TX monitor processing.
 * It aggregates multiple status descriptors that contain TLV fragments
 * for a single PPDU. Used by both WiFi7 and WiFi8 implementations.
 *
 * The structure is allocated from a free list during ring processing
 * and queued for work queue processing when end_of_ppdu is detected.
 *
 * Note: status_desc is dynamically allocated to support runtime configuration
 * of ATH12K_DP_MON_STATUS_BUF size. Memory must be allocated during initialization
 * and freed during cleanup.
 */
struct ath12k_dp_mon_ppdu_desc {
	struct list_head list;
	u32 ppdu_id;
	u32 timestamp;
	struct ath12k_dp_mon_status_desc *status_desc;
	u32 status_desc_cnt;
};

/**
 * struct ath12k_mon_ring_desc_info - Extracted monitor ring descriptor info
 * @mon_desc: Pointer to monitor descriptor containing frame data and metadata
 * @ppdu_id: PPDU identifier for correlating related descriptors and frames
 * @end_offset: End offset indicating the valid data length in the buffer
 * @end_reason: Hardware-provided reason code for descriptor completion
 * @empty_desc: Flag indicating whether this descriptor contains no frame data
 *
 * This structure serves as an abstraction layer for monitor ring descriptor
 * information, allowing architecture-specific extraction functions to populate
 * common fields that can be processed by generic monitor code.
 */
struct ath12k_mon_ring_desc_info {
	struct ath12k_dp_mon_desc *mon_desc;
	u32 ppdu_id;
	u32 end_offset;
	u32 end_reason;
	bool empty_desc;
	u8 pkt_buf_cnt;
};

/**
 * struct ath12k_pdev_mon_dp - Per-pdev monitor mode data path context
 * @dp_mon: Pointer to global DP monitor context for shared resources
 * @dp_pdev: Pointer to parent pdev DP context for device-specific operations
 * @rxdma_mon_dst_ring: Array of RX DMA monitor destination rings per RXDMA engine
 * @rx_status: IEEE 802.11 RX status structure for monitor frame metadata
 * @mon_data: Monitor data structure containing RX/TX frame processing state
 * @rx_filter: Pointer to array of RX monitor filters for frame selection
 * @ppdu_desc_pool: Pool of PPDU descriptors for RX monitor frame processing
 * @ppdu_desc_used_list: List of currently used RX PPDU descriptors
 * @ppdu_desc_free_list: List of available RX PPDU descriptors for allocation
 * @ppdu_desc_proc_list: List of RX PPDU descriptors pending processing
 * @ppdu_desc_lock: Spinlock protecting RX PPDU descriptor list operations
 * @mon_desc_used_list: List of monitor descriptors currently in use
 * @mon_stats: RX monitor statistics counters for performance tracking
 * @rxmon_work: Work structure for RX monitor processing in work queue context
 * @rxmon_wq: Dedicated work queue for RX monitor frame processing
 * @smart_mon_filter: Smart monitor filter configuration (4-bit CMDV format)
 * @smart_mon_state: Current state of smart monitor functionality
 * @rx_ext_mon_config: Current filter and peer configs for Rx extended monitor.
 * @rx_ext_mon_lock: Spinlock protecting the Rx extended monitor struct.
 *
 * This structure represents the complete monitor mode data path context for a
 * single pdev (physical device). It manages both RX and TX monitor functionality,
 * including frame capture, filtering, and processing infrastructure.
 *
 * Smart Monitor Filter Details:
 * The smart_mon_filter field uses a 4-bit encoding (CMDV format):
 * - Bit 0 (V): Valid bit - must be 1 for filter to be active
 * - Bit 1 (D): Data frame filter (0=capture, 1=filter out)
 * - Bit 2 (M): Management frame filter (0=capture, 1=filter out)
 * - Bit 3 (C): Control frame filter (0=capture, 1=filter out)
 *
 * Smart Monitor Behavior:
 * - 0x0: Regular monitor mode - captures ALL packets immediately
 * - Non-zero: Smart monitor mode - requires NAC (Network Access Control) setup
 *   - Monitor VAP starts but captures no packets initially
 *   - Packet capture begins only after NAC MAC addresses are configured
 *   - Filters applied based on frame type and NAC list matching
 *
 * Work Queue Architecture:
 * - rxmon_wq/rxmon_work: Handles RX monitor frame processing in process context
 * - txmon_wq/txmon_work: Handles TX monitor frame processing in process context
 * This design moves heavy processing out of interrupt/NAPI context for better
 * system responsiveness.
 *
 * Memory Management:
 * The structure maintains separate descriptor pools and lists for RX and TX
 * monitor functionality, using a three-list architecture (free/used/processing)
 * for efficient descriptor lifecycle management.
 */
struct ath12k_pdev_mon_dp {
	struct ath12k_dp_mon *dp_mon;
	struct ath12k_pdev_dp *dp_pdev;
	struct dp_srng rxdma_mon_dst_ring[MAX_RXDMA_PER_PDEV];
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;

	struct ieee80211_rx_status rx_status;
	struct ath12k_mon_data mon_data;
	struct dp_mon_rx_filter **rx_filter;
	struct ath12k_dp_mon_ppdu_desc **ppdu_desc_pool;
	struct list_head ppdu_desc_used_list;
	struct list_head ppdu_desc_free_list;
	struct list_head ppdu_desc_proc_list;

	/* lock for ath12k_dp_mon_ppdu_desc */
	spinlock_t ppdu_desc_lock;
	struct list_head mon_desc_used_list;
	struct ath12k_pdev_mon_dp_stats mon_stats;

	struct work_struct rxmon_work;
	struct workqueue_struct *rxmon_wq;
	/* Monitor RX filter type: 4-bit field (C M D V) for Smart Monitor
	 *
	 * Bit Layout (filter out mechanism: 0=filter in, 1=filter out):
	 *   Bit 0 (V): Valid bit - must be 1 for filter to be active
	 *   Bit 1 (D): Data frame filter
	 *   Bit 2 (M): Management frame filter
	 *   Bit 3 (C): Control frame filter
	 *
	 * Behavior:
	 *   0x0: Regular monitor mode - captures ALL packets
	 *   Non-zero: Smart monitor mode
	 *     - Monitor VAP comes up but NO packets captured initially
	 *     - Filters applied only after NAC MAC addresses are added
	 *     - Captures packets from NAC list based on frame type filter
	 *
	 * Examples:
	 *   0x0 (0000): Regular monitor - all packets
	 *   0x1 (0001): Smart monitor - all frame types (when NAC added)
	 *   0x3 (0011): Smart monitor - only Control + Management
	 *   0xD (1101): Smart monitor - only Data frames
	 *   0xF (1111): Smart monitor - no frames (all filtered out)
	 *
	 * Default: 0x00 (regular monitor mode)
	 */
	u8 smart_mon_filter;
	enum ath12k_dp_smart_mon_state smart_mon_state;

#ifdef CPTCFG_QCN_EXTN
	struct ath12k_pdev_mon_dp_extn pdev_mon_dp_extn;
#endif /* CPTCFG_QCN_EXTN */
	struct ath12k_dp_rx_ext_mon *rx_ext_mon_config;
	spinlock_t rx_ext_mon_lock;
	bool rx_pktlog_cbf;
	u8 rx_pktlog_mode;
	bool nrp_enabled;
};

enum ath12k_dp_mon_desc_in_use {
	DP_MON_DESC_REPLENISH = 1,
	DP_MON_DESC_STATUS_REAP,
	DP_MON_DESC_PACKET_REAP,
	DP_MON_DESC_H_PROC_ERR,
	DP_MON_DESC_TO_HW,
	DP_MON_DESC_H_REPLENISH_ERR,
};

struct ath12k_dp_mon_desc {
	struct list_head list;
	u8 *mon_buf;
	dma_addr_t paddr;
	u32 magic;
	u16 buf_len:12,
	    in_use:3,
	    end_of_ppdu:1;
	u8 pkt_buf_cnt;
} __packed;

enum ath12k_ext_mon_cmd_type {
	ATH12K_EXT_MON_CMD_TYPE_SET_FILTER = 1,
	ATH12K_EXT_MON_CMD_TYPE_GET_FILTER = 2,
	ATH12K_EXT_MON_CMD_TYPE_SET_PEER = 3,
	ATH12K_EXT_MON_CMD_TYPE_GET_PEER = 4,
};

enum ath12k_ext_mon_frame_len {
	ATH12K_EXT_MON_LEN_64B = 1,
	ATH12K_EXT_MON_LEN_128B = 2,
	ATH12K_EXT_MON_LEN_256B = 3,
	ATH12K_EXT_MON_LEN_FULL_PKT = 4,
};

enum ath12k_ext_mon_filter_level {
	ATH12K_EXT_MON_FILTER_LEVEL_MSDU = 1,
	ATH12K_EXT_MON_FILTER_LEVEL_MPDU,
	ATH12K_EXT_MON_FILTER_LEVEL_PPDU,
};

enum ath12k_ext_mon_status_code {
	ATH12K_EXT_MON_SUCCESS = 0,
	ATH12K_EXT_MON_VALIDATION_FAIL = 1,
	ATH12K_EXT_MON_FILTER_SETUP_FAIL = 2,
	ATH12K_EXT_MON_PEER_SETUP_FAIL = 3,
};

enum ath12k_ext_mon_frame_type {
	ATH12K_EXT_MON_FRAME_MGMT = 0,
	ATH12K_EXT_MON_FRAME_CTRL = 1,
	ATH12K_EXT_MON_FRAME_DATA = 2,
	ATH12K_EXT_MON_FRAME_MAX = 3,
};

enum ath12k_ext_mon_peer_action {
	ATH12K_EXT_MON_PEER_ACTION_ADD = 1,
	ATH12K_EXT_MON_PEER_ACTION_REMOVE = 2,
};

enum ath12k_ext_mon_monitor_flags {
	ATH12K_EXT_MON_DEFAULT = 0,
	ATH12K_EXT_MON_PKT_CAP,
	ATH12K_EXT_MON_MAX
};

struct ath12k_ext_mon_data_mpdu_tlv_config {
	u8 mcast;
	u8 ucast;
	u8 null_frm;
	bool tlv_configured;
};

struct ath12k_ext_mon_pkt_config {
	u32 filter[ATH12K_EXT_MON_FRAME_MAX];
	u8 len[ATH12K_EXT_MON_FRAME_MAX];
	struct ath12k_ext_mon_data_mpdu_tlv_config data_mpdu_tlv;
};

struct ath12k_ext_mon_filter_config {
	enum ath12k_ext_mon_filter_level level;
	bool disable;
	enum ath12k_ext_mon_monitor_flags monitor_flags;
	struct ath12k_ext_mon_pkt_config all_peer;
	struct ath12k_ext_mon_pkt_config all_neighbor;
	struct ath12k_ext_mon_pkt_config target_peer;
	struct ath12k_ext_mon_pkt_config target_neighbor;
	u8 meta_data;
};

struct ath12k_ext_mon_snr_info {
	u8 snr;
	u8 avg_snr;
	u64 timestamp;
};

struct ath12k_ext_mon_ta_peer_info {
	u8 bitmap;
	struct ath12k_ext_mon_snr_info snr_info;
};

struct ath12k_ext_mon_peer_info {
	u8 mac_addr[ETH_ALEN];
	bool ra_addr;
	struct ath12k_ext_mon_ta_peer_info ta_info;
};

struct ath12k_ext_mon_peer_config {
	u8 action;
	u8 count;
	struct ath12k_ext_mon_peer_info peer_info[ATH12K_EXT_MON_MAX_PEERS];
};

struct ath12k_ext_mon_config {
	u8 cmd_type;
	u8 direction;
	enum ath12k_ext_mon_status_code status_code;
	struct ath12k_ext_mon_filter_config filter;
	struct ath12k_ext_mon_peer_config peer;
};

struct ath12k_dp_ext_mon_peer {
	struct ath12k_ext_mon_peer_info peer_info;
	struct list_head list;
};

struct ath12k_dp_rx_ext_mon {
	bool enable;
	enum ath12k_ext_mon_filter_level level;
	u8 metadata;
	bool fp_enabled;
	bool mo_enabled;
	bool fpmo_enabled;
	bool md_enabled;
	bool short_pkt_en;
	struct ath12k_ext_mon_pkt_config fp;
	struct ath12k_ext_mon_pkt_config mo;
	struct ath12k_ext_mon_pkt_config fpmo;
	struct ath12k_ext_mon_pkt_config md;
	u8 peer_count;
	u8 ra_peer_count;
	struct list_head peer_list;
};

struct ath12k_dp_tx_ext_mon_config {
	bool enable;
	enum ath12k_ext_mon_filter_level level;
	enum ath12k_ext_mon_monitor_flags monitor_flags;
	u8 metadata;
	bool fp_enabled;
	bool fpmo_enabled;
	struct ath12k_ext_mon_pkt_config fp;
	struct ath12k_ext_mon_pkt_config fpmo;
	u8 peer_count;
	struct list_head peer_list;
};

static inline enum dp_monitor_type
ath12k_dp_get_mon_type(struct ath12k_dp *dp)
{
	return ((dp->hw_params->rxdma1_enable) ? ATH12K_DP_MON_TYPE_DUAL_RING :
				ATH12K_DP_MON_TYPE_QUAD_RING);
}

static inline
const struct ath12k_dp_arch_mon_ops *ath12k_dp_mon_ops_get(struct ath12k_dp *dp)
{
	if (dp && dp->dp_mon)
		return dp->dp_mon->mon_ops;

	return NULL;
}

static inline bool ath12k_dp_mon_rxdma1_enable(struct ath12k_dp *dp)
{
	return dp->hw_params->rxdma1_enable;
}

static inline bool
ath12k_dp_mon_rx_get_quad_ring_support(struct ath12k_dp *dp)
{
	return dp->hw_params->quad_ring_monitor_support;
}

/* Wrapper functions for RX and TX buffer replenishment */
int ath12k_dp_mon_rx_buf_replenish(struct ath12k_dp *dp,
				   struct dp_rxdma_mon_ring *buf_ring,
				   struct list_head *used_list,
				   int req_entries);
int ath12k_dp_mon_tx_buf_replenish(struct ath12k_dp *dp,
				   struct dp_rxdma_mon_ring *buf_ring,
				   struct list_head *used_list,
				   int req_entries);
/* Core buffer replenishment function */
int ath12k_dp_mon_buf_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_mon_ring *buf_ring,
				int req_entries,
				struct dp_mon_desc_list_params *list_params);
void ath12k_dp_mon_rx_process_ulofdma_stats(struct hal_rx_mon_ppdu_info *ppdu_info);
int ath12k_dp_mon_rx_dual_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
				       struct napi_struct *napi, int *budget);
void ath12k_dp_mon_peer_telemetry_stats(const struct ath12k_dp_link_peer *peer,
					struct ath12k_peer_telemetry_stats *stats);

int ath12k_dp_mon_pdev_update_telemetry_stats(struct ath12k_base *ab,
                                             int pdev_id);
void ath12k_dp_mon_cfg_init(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_srng_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_ring_init(struct ath12k_dp *dp);
void ath12k_dp_mon_rx_ring_deinit(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp);
int ath12k_dp_mon_pdev_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev,
				     u32 mac_id);
void ath12k_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev,
					 u32 mac_id);
void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_rx_detach(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_rx_mpdu_list_init(struct ath12k_mon_data *pmon);
void ath12k_dp_rx_mon_dest_process(struct ath12k *ar, int mac_id,
				   u32 quota, struct napi_struct *napi);
int ath12k_dp_mon_rx_set_pktlen(struct sk_buff *skb, u32 len);
void ath12k_dp_mon_rx_update_peer_su_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct hal_rx_mon_ppdu_info *ppdu_info);
void ath12k_dp_mon_rx_update_peer_mu_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct hal_rx_mon_ppdu_info *ppdu_info);
void ath12k_dp_mon_ppdu_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       bool is_stat);
void ath12k_dp_mon_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_mon_ppdu_info *ppdu_info);
void ath12k_dp_mon_rx_stats_enable(struct ath12k_pdev_dp *dp_pdev,
				   enum dp_mon_stats_mode mode);
void ath12k_dp_mon_rx_stats_disable(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode);
void ath12k_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_nrp_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_nrp_reset(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_smart_mon_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_smart_mon_reset(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_rx_monitor_mode_buf_setup(struct ath12k *ar);
size_t ath12k_dp_mon_list_cut_nodes(struct list_head *list, struct list_head *head,
				    size_t count);
void ath12k_dp_mon_rx_deliver_skb(struct ath12k_pdev_dp *dp_pdev,
				  struct napi_struct *napi, struct sk_buff *msdu,
				  struct ieee80211_rx_status *status,
				  struct hal_rx_mon_ppdu_info *ppduinfo);
void ath12k_dp_mon_fill_rx_stats_info(struct hal_rx_mon_ppdu_info *ppdu_info,
				      struct ieee80211_rx_status *rx_status);
void ath12k_dp_mon_update_radiotap(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_mon_ppdu_info *ppduinfo,
				   struct sk_buff *mon_skb,
				   struct ieee80211_rx_status *rxs);
struct sk_buff *ath12k_dp_mon_get_skb_valid_frag(struct ath12k_dp *dp,
						 struct sk_buff *skb);
void ath12k_dp_mon_add_frag_list(struct sk_buff *skb_head, struct sk_buff *frag_list,
				 u32 frag_len);
void ath12k_dp_mon_update_skb_len(struct sk_buff *skb_head, u32 frag_len);
void ath12k_dp_mon_append_skb(struct sk_buff *skb, struct sk_buff *tmp_skb);
void ath12k_dp_mon_skb_remove_frag(struct ath12k_dp *dp, struct sk_buff *skb,
				   u16 idx, u16 truesize);
void ath12k_dp_mon_add_rx_frag(struct sk_buff *skb, const void *mon_buf,
			       int offset, int frag_len, bool take_frag_ref);
int ath12k_dp_mon_get_puncture_type(u16 puncture_pattern, u8 bw);
void ath12k_dp_mon_rx_process_low_thres(struct ath12k_dp *dp);
void
ath12k_dp_mon_cnt_skb_and_frags(struct sk_buff *skb, u32 *skb_count, u32 *frag_count);
void ath12k_dp_mon_pktlog_config_filter(struct ath12k_pdev_dp *dp_pdev,
					enum ath12k_pktlog_mode mode,
					u32 filter, bool enable);
u32 ath12k_dp_mon_get_frag_size_by_idx(struct ath12k_dp *dp, struct sk_buff *skb,
				       u8 idx);
void *ath12k_dp_mon_skb_get_frag_addr(struct sk_buff *skb, u8 idx);
int ath12k_dp_mon_adj_frag_offset(struct sk_buff *skb, u8 idx, int offset);
u32 ath12k_dp_mon_get_num_frags_in_fraglist(struct sk_buff *skb);
u64 ath12k_get_timestamp_in_us(void);
void ath12k_dp_mon_fill_rx_rate(struct ath12k_pdev_dp *dp_pdev,
				struct hal_rx_mon_ppdu_info *ppdu_info,
				struct ieee80211_rx_status *rx_status);
void ath12k_dp_ext_mon_process_request(struct ath12k_pdev_dp *dp_pdev,
				       const struct ath12k_ext_mon_config *req,
				       struct ath12k_ext_mon_config *resp);
int ath12k_dp_ext_mon_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_ext_mon_free(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_ext_mon_reset(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_ext_mon_update_snr(struct hal_rx_mon_ppdu_info *ppdu_info,
				  struct ath12k_dp_rx_ext_mon *config);

void ath12k_dp_mon_reset_ppdu_desc(struct ath12k_dp_mon_ppdu_desc *ppdu_desc);
int ath12k_dp_mon_get_link_peer_rssi(void *ptr, const u8 *peer_mac,
				     s8 *min_rssi, s8 *max_rssi);
size_t
ath12k_dp_mon_get_free_desc_list(struct ath12k_dp *dp,
				 struct dp_rxdma_mon_ring *rx_ring,
				 struct dp_mon_desc_list_params *list_params,
				 size_t max_entries);
void ath12k_dp_rx_pktlog_process(struct ath12k_pdev_dp *dp_pdev,
				 struct ath12k_dp_mon_status_desc *status_desc);
void ath12k_dp_mon_rx_process_dest_pktlog(struct ath12k_pdev_dp *dp_pdev,
					  struct sk_buff *skb,
					  struct hal_rx_mon_ppdu_info *ppdu_info);
int ath12k_dp_mon_rx_wq_init_common(struct ath12k_pdev_dp *dp_pdev,
				    void (*work_handler)(struct work_struct *));
int
ath12k_dp_ext_mon_subtype_check(struct ath12k_ext_mon_pkt_config *config,
				u8 type, u8 sub_type, bool is_mcast);


static inline
int ath12k_dp_mon_rx_alloc(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp || !dp->dp_mon))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_srng_setup) {
		ret = mon_ops->rx_srng_setup(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static inline
void ath12k_dp_mon_rx_free(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_srng_cleanup)
		mon_ops->rx_srng_cleanup(dp);

}

static inline
int ath12k_dp_mon_rx_init(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp || !dp->dp_mon))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_ring_init) {
		ret = mon_ops->rx_ring_init(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static inline
void ath12k_dp_mon_rx_deinit(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_ring_deinit)
		mon_ops->rx_ring_deinit(dp);
}

static inline
int ath12k_dp_mon_rx_htt_setup(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = 0;

	if (unlikely(!dp || !dp->dp_mon))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_htt_srng_setup)
		ret = mon_ops->rx_htt_srng_setup(dp);

	return ret;
}

static inline
int ath12k_dp_mon_pdev_init(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp *dp;
	int ret = 0;

	if (unlikely(!dp_pdev))
		return -EINVAL;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_pdev_alloc)
		ret = mon_ops->mon_pdev_alloc(dp_pdev);

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_dp_mon_pdev_tlv_logger_init(dp_pdev))
		ath12k_warn(dp, "failed to init TLV logger for mon pdev\n");
#endif /* CPTCFG_QCN_EXTN */

	return ret;
}

static inline
void ath12k_dp_mon_pdev_deinit(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp *dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

#ifdef CPTCFG_QCN_EXTN
	ath12k_dp_mon_pdev_tlv_logger_deinit(dp_pdev);
#endif /* CPTCFG_QCN_EXTN */

	if (mon_ops && mon_ops->mon_pdev_free)
		mon_ops->mon_pdev_free(dp_pdev);
}

static inline
int ath12k_dp_mon_pdev_rx_alloc(struct ath12k_pdev_dp *dp_pdev,
				u32 mac_id)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return -EINVAL;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "mon ops is NULL\n");
		return -EINVAL;
	}

	if (mon_ops->mon_pdev_rx_srng_setup) {
		ret = mon_ops->mon_pdev_rx_srng_setup(dp_pdev,
						      mac_id);
		if (ret) {
			ath12k_warn(dp, "failed to setup HAL_RXDMA_MONITOR_DST\n");
			goto srng_cleanup;
		}
	}

	if (mon_ops->setup_ppdu_desc) {
		ret = mon_ops->setup_ppdu_desc(dp_pdev);
		if (ret) {
			ath12k_warn(dp, "failed to setup ppdu desc ret = %d\n",
				    ret);
			goto cleanup;
		}
	}

	if (mon_ops->mon_rx_wq_init) {
		ret = mon_ops->mon_rx_wq_init(dp_pdev);
		if (ret) {
			ath12k_warn(dp,
				    "failed to init mon workqueue for pdev_id %d\n",
				    mac_id);
			goto cleanup;
		}
	}

	if (mon_ops->ext_mon_alloc) {
		ret = mon_ops->ext_mon_alloc(dp_pdev);
		if (ret) {
			ath12k_warn(dp, "failed to alloc ext mon for pdev_id: %d\n",
				    mac_id);
			goto free_rx_wq;
		}
	}

	return 0;

free_rx_wq:
	if (mon_ops && mon_ops->mon_rx_wq_deinit)
		mon_ops->mon_rx_wq_deinit(dp_pdev, true);

cleanup:
	if (mon_ops && mon_ops->cleanup_ppdu_desc)
		mon_ops->cleanup_ppdu_desc(dp_pdev);

srng_cleanup:
	if (mon_ops && mon_ops->mon_pdev_rx_srng_cleanup)
		mon_ops->mon_pdev_rx_srng_cleanup(dp_pdev);

	return ret;
}

static inline
int ath12k_dp_mon_pdev_rx_htt_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return -EINVAL;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "mon ops is NULL\n");
		return -EINVAL;
	}

	if (mon_ops->mon_pdev_rx_htt_srng_setup) {
		ret = mon_ops->mon_pdev_rx_htt_srng_setup(dp_pdev,
							  mac_id);
		if (ret) {
			ath12k_warn(dp, "htt setup failed for HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
	}

	if (mon_ops->mon_pdev_rx_attach)
		mon_ops->mon_pdev_rx_attach(dp_pdev);

	if (mon_ops->rx_filter_alloc) {
		ret = mon_ops->rx_filter_alloc(dp_pdev);
		if (ret) {
			ath12k_warn(dp, "failed to setup monitor rx filter ret = %d\n",
				    ret);
			ath12k_dp_mon_pdev_rx_detach(dp_pdev);
			return ret;
		}
	}
	return 0;
}

static inline
void ath12k_dp_mon_pdev_rx_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "mon ops is NULL during mon pdev free\n");
		return;
	}
	ath12k_dp_mon_pdev_rx_detach(dp_pdev);

	if (mon_ops->mon_rx_wq_deinit)
		mon_ops->mon_rx_wq_deinit(dp_pdev, true);

	if (mon_ops->rx_filter_free)
		mon_ops->rx_filter_free(dp_pdev);

	if (mon_ops && mon_ops->cleanup_ppdu_desc)
		mon_ops->cleanup_ppdu_desc(dp_pdev);

	if (mon_ops && mon_ops->mon_pdev_rx_srng_cleanup)
		mon_ops->mon_pdev_rx_srng_cleanup(dp_pdev);

	/*
	 * ext_mon_free() must be invoked only after all monitor-related work
	 * queues have been deinitialized and no pending work can reference
	 * ext monitor resources. Calling this earlier may lead to use-after-free.
	 */

	if (mon_ops->ext_mon_free)
		mon_ops->ext_mon_free(dp_pdev);
}

static inline
void ath12k_dp_mon_update_telemetry_stats(struct ath12k_base *ab,
					  const int pdev_id)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->update_telemetry_stats)
		mon_ops->update_telemetry_stats(ab, pdev_id);
}

static inline
void ath12k_dp_mon_rx_stats_config(struct ath12k *ar, bool enable,
				  enum dp_mon_stats_mode mode)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (enable) {
		if(mon_ops && mon_ops->rx_stats_enable)
			mon_ops->rx_stats_enable(dp_pdev, mode);
	} else {
		if(mon_ops && mon_ops->rx_stats_disable)
			mon_ops->rx_stats_disable(dp_pdev, mode);
	}
}

static inline
int ath12k_dp_mon_rx_update_filter(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	int ret;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_filter_update) {
		ret = mon_ops->rx_filter_update(dp_pdev);
		if (ret)
			return ret;
	}

	return 0;
}

static inline
void ath12k_dp_mon_rx_config_monitor_mode(struct ath12k *ar, bool reset)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!reset) {
		if (!(dp_pdev->dp_mon_pdev->smart_mon_filter &
		      DP_SMART_MON_FILTER_MASK)) {
			dp_pdev->dp_mon_pdev->smart_mon_state =
					ATH12K_DP_SMART_MON_DISABLED;
			if (mon_ops && mon_ops->rx_monitor_mode_set)
				mon_ops->rx_monitor_mode_set(dp_pdev);
		} else {
			if (dp_pdev->dp_mon_pdev->smart_mon_filter & DP_SMART_MON_VALID)
				dp_pdev->dp_mon_pdev->smart_mon_state =
							ATH12K_DP_SMART_MON_IDLE;
			else
				dp_pdev->dp_mon_pdev->smart_mon_state =
							ATH12K_DP_SMART_MON_DISABLED;
		}
	} else {
		if(mon_ops && mon_ops->rx_monitor_mode_reset)
			mon_ops->rx_monitor_mode_reset(dp_pdev);
	}
}

static inline
void ath12k_dp_mon_rx_nrp_config(struct ath12k *ar, bool reset)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!reset) {
		if (mon_ops && mon_ops->rx_nrp_set)
			mon_ops->rx_nrp_set(dp_pdev);
	} else {
		if (mon_ops && mon_ops->rx_nrp_reset)
			mon_ops->rx_nrp_reset(dp_pdev);
	}
}

static inline
void ath12k_dp_mon_rx_smart_mon_config(struct ath12k *ar, bool reset)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!reset) {
		if (mon_ops && mon_ops->rx_smart_mon_set)
			mon_ops->rx_smart_mon_set(dp_pdev);
		dp_pdev->dp_mon_pdev->smart_mon_state = ATH12K_DP_SMART_MON_ACTIVE;
	} else {
		if (mon_ops && mon_ops->rx_smart_mon_reset)
			mon_ops->rx_smart_mon_reset(dp_pdev);
		dp_pdev->dp_mon_pdev->smart_mon_state = ATH12K_DP_SMART_MON_IDLE;
	}
}

static inline
void ath12k_dp_mon_rx_enable(struct ath12k_dp *dp, void *cmd,
			     struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->htt_rx_filter_rxmon_cfg)
		mon_ops->htt_rx_filter_rxmon_cfg(cmd, tlv_filter);
}

static inline
void ath12k_dp_mon_rx_phy_err_filter_cfg(struct ath12k_dp *dp, void *cmd,
					 struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->htt_rx_phy_err_filter_cfg)
		mon_ops->htt_rx_phy_err_filter_cfg(cmd, tlv_filter);
}

static inline
void ath12k_dp_mon_rx_config_wmask(struct ath12k_dp *dp, void *ptr,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);
	if (mon_ops && mon_ops->mon_rx_wmask)
		mon_ops->mon_rx_wmask(ptr, tlv_filter);
}

static inline void
ath12k_dp_mon_rx_config_packet_type_subtype(struct ath12k_dp *dp, void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_enable_packet_filters)
		mon_ops->rx_enable_packet_filters(ptr, tlv_filter);
}

static inline void
ath12k_dp_mon_rx_config_packet_type_hdr_len(struct ath12k_dp *dp, void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);
}

static inline void
ath12k_dp_mon_pktlog_config(struct ath12k *ar, bool enable,
			    enum ath12k_pktlog_mode mode,
			    u32 filter)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if(mon_ops && mon_ops->pktlog_config)
		mon_ops->pktlog_config(dp_pdev, mode, filter, enable);
}

static inline void
ath12k_dp_mon_cfr_rcc_config(struct ath12k *ar, bool enable, u8 rcc_type)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_cfr_rcc_config)
		mon_ops->rx_cfr_rcc_config(dp_pdev, enable, rcc_type);
}

static inline bool
ath12k_dp_mon_get_cfr_rcc_filter_valid(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return false;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_get_cfr_rcc_filter_valid)
		return mon_ops->rx_get_cfr_rcc_filter_valid(dp_pdev);

	return false;
}

static inline void
ath12k_dp_mon_rx_flush_wq(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_rx_wq_deinit)
		mon_ops->mon_rx_wq_deinit(dp_pdev, false);
}

static inline void
ath12k_dp_mon_desc_reset(struct ath12k_dp_mon_desc *desc)
{
	memset((u8 *)desc + sizeof(desc->list), 0, sizeof(*desc) - sizeof(desc->list));
}

static inline bool
ath12k_dp_ext_mon_is_enabled(struct ath12k *ar)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_pdev_mon_dp *mon_pdev = dp_pdev->dp_mon_pdev;
	bool enabled = false;

	if (!mon_pdev)
		return false;

	spin_lock(&mon_pdev->rx_ext_mon_lock);
	if (mon_pdev->rx_ext_mon_config)
		enabled = mon_pdev->rx_ext_mon_config->enable;
	spin_unlock(&mon_pdev->rx_ext_mon_lock);

	return enabled;
}

static inline void
ath12k_dp_mon_set_nrp(struct ath12k *ar,
		      bool val)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	dp_pdev->dp_mon_pdev->nrp_enabled = val;
}

static inline void
ath12k_dp_smart_mon_filter_type_set(struct ath12k *ar,
				    u8 new_filter)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_base *ab = ar->ab;
	u8 old_filter, smart_mon_profile;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	if (ath12k_dp_ext_mon_is_enabled(ar)) {
		ath12k_warn(dp_pdev->dp, "ext mon enabled\n");
		return;
	}

	smart_mon_profile = dp_pdev->dp_mon_pdev->smart_mon_filter &
				DP_SMART_MON_PROFILE_MASK;
	if (smart_mon_profile == DP_SMART_MON_PROFILE_512M ||
	    smart_mon_profile == DP_SMART_MON_PROFILE_256M) {
		ath12k_warn(dp_pdev->dp,
			    "Low mem: smart mon 0x%x unsupported, disable to 0x%x\n",
			    new_filter, (u8)ATH12K_DP_SMART_MON_FILTER_DEFAULT &
			    DP_SMART_MON_FILTER_MASK);
		new_filter = ATH12K_DP_SMART_MON_FILTER_DEFAULT &
				DP_SMART_MON_FILTER_MASK;
	}

	old_filter = dp_pdev->dp_mon_pdev->smart_mon_filter &
				DP_SMART_MON_FILTER_MASK;
	new_filter &= DP_SMART_MON_FILTER_MASK;
	if (old_filter != new_filter) {
		dp_pdev->dp_mon_pdev->smart_mon_filter = smart_mon_profile | new_filter;
		if (dp_pdev->dp_mon_pdev->smart_mon_state ==
		    ATH12K_DP_SMART_MON_ACTIVE) {
			if (new_filter & DP_SMART_MON_VALID) {
				ath12k_dp_mon_rx_smart_mon_config(ar, true);
				ath12k_dp_mon_rx_update_filter(ar);
				ath12k_dp_mon_rx_smart_mon_config(ar, false);
				ath12k_dp_mon_rx_update_filter(ar);
			}
		}
	}
}

static inline void
ath12k_dp_smart_mon_filter_type_get(struct ath12k *ar,
				    u8 *filter)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	*filter = dp_pdev->dp_mon_pdev->smart_mon_filter &
				DP_SMART_MON_FILTER_MASK;
}

static inline bool
ath12k_dp_smart_mon_enabled(struct ath12k *ar)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return false;

	if (dp_pdev->dp_mon_pdev->smart_mon_state !=
	    ATH12K_DP_SMART_MON_DISABLED)
		return true;

	return false;
}

static inline void
ath12k_dp_mon_pdev_rx_scan_radio_stats_reset(struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
#ifdef CPTCFG_QCN_EXTN
	memset(&dp_mon_pdev->pdev_mon_dp_extn.rx_scan_radio_stats, 0,
	       sizeof(dp_mon_pdev->pdev_mon_dp_extn.rx_scan_radio_stats));
#endif /* CPTCFG_QCN_EXTN */
}

static inline void
ath12k_dp_rx_scan_radio_stats_reset(struct ath12k_hw *ah)
{
	int i = 0;
	struct ath12k *ar;

	wiphy_lock(ah->hw->wiphy);
	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];

		if (!ar->dp.dp_mon_pdev)
			continue;

#ifdef CPTCFG_QCN_EXTN
		ath12k_dp_mon_pdev_rx_scan_radio_stats_reset(ar->dp.dp_mon_pdev);
#endif /* CPTCFG_QCN_EXTN */
	}
	wiphy_unlock(ah->hw->wiphy);
}

static inline void
ath12k_dp_mon_rx_scan_radio_stats_update(struct ath12k *ar,
					 struct ath12k_telemetry_dp_vif *telemetry_vif)
{
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn;
#endif /* CPTCFG_QCN_EXTN */

	if (unlikely(!ar || !telemetry_vif))
		return;

	if (!ar->dp.dp_mon_pdev)
		return;

#ifdef CPTCFG_QCN_EXTN
	mon_dp_extn = &ar->dp.dp_mon_pdev->pdev_mon_dp_extn;
	ath12k_dp_rx_scan_radio_stats_update(telemetry_vif,
					     &mon_dp_extn->rx_scan_radio_stats);
#endif /* CPTCFG_QCN_EXTN */
}

static inline bool
ath12k_dp_ext_mon_is_mode_enabled(const struct ath12k_ext_mon_pkt_config *config)
{
	u8 i;

	for (i = 0; i < ATH12K_EXT_MON_FRAME_MAX; i++)
		if (config->filter[i])
			return true;

	return false;
}

static inline bool
ath12k_dp_ext_mon_full_pkt_enabled(const struct ath12k_dp_rx_ext_mon *config,
				   enum ath12k_ext_mon_frame_type type) {
	return (config->fp.len[type] == ATH12K_EXT_MON_LEN_FULL_PKT ||
		config->mo.len[type] == ATH12K_EXT_MON_LEN_FULL_PKT ||
		config->fpmo.len[type] == ATH12K_EXT_MON_LEN_FULL_PKT ||
		config->md.len[type] == ATH12K_EXT_MON_LEN_FULL_PKT);
}

static inline enum ath12k_ext_mon_frame_len
ath12k_ext_mon_get_max_shortpkt_len(const struct ath12k_dp_rx_ext_mon *config)
{
	enum ath12k_ext_mon_frame_len len, max_len = 0;
	u8 i;

	for (i = 0; i < ATH12K_EXT_MON_FRAME_MAX; i++) {
		len = config->fp.len[i];
		if (len != ATH12K_EXT_MON_LEN_FULL_PKT && len > max_len)
			max_len = len;

		len = config->mo.len[i];
		if (len != ATH12K_EXT_MON_LEN_FULL_PKT && len > max_len)
			max_len = len;

		len = config->md.len[i];
		if (len != ATH12K_EXT_MON_LEN_FULL_PKT && len > max_len)
			max_len = len;

		len = config->fpmo.len[i];
		if (len != ATH12K_EXT_MON_LEN_FULL_PKT && len > max_len)
			max_len = len;
	}

	return max_len;
}

static inline int
ath12k_dp_ext_mon_find_mon_vdev_id(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_link_vif *arvif;

	list_for_each_entry(arvif, &dp_pdev->ar->arvifs, list) {
		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR &&
		    arvif->is_started)
			return arvif->vdev_id;
	}
	return -1;
}

static inline u8
ath12k_dp_get_avg_snr(u8 snr, u8 avg_snr)
{
	/* Calculating the moving average of SNR */
	if (avg_snr != 0) {
		avg_snr =
			((avg_snr -
			  (avg_snr >> 2)) +
			  (snr >> 2));
	} else {
		/* First sample */
		avg_snr = snr;
	}

	return avg_snr;
}

static inline void
ath12k_dp_get_ext_mon_peers(struct ath12k_pdev_dp *dp_pdev,
			    struct list_head *peer_list,
			    struct ath12k_ext_mon_config *resp,
			    spinlock_t *lock)
{
	struct ath12k_dp_ext_mon_peer *peer;
	u8 count = 0;

	lockdep_assert_held(lock);
	list_for_each_entry(peer, peer_list, list) {
		if (count >=
			ATH12K_EXT_MON_NUM_PEERS(dp_pdev->dp->ab)) {
			ath12k_err(NULL,
				   "Number of peers exceeded max limit: %d",
				   ATH12K_EXT_MON_NUM_PEERS(dp_pdev->dp->ab));
			break;
		}

		resp->peer.peer_info[count] = peer->peer_info;
		count++;
	}
	resp->peer.count = count;
}

void ath12k_dp_mon_peer_telemetry_stats(const struct ath12k_dp_link_peer *peer,
					struct ath12k_peer_telemetry_stats *stats);
void
ath12k_dp_mon_rx_add_pf_tag_to_headroom(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_mon_ppdu_info *ppdu_info);
u16 ath12k_dp_mon_rx_vendor_tlv_len(struct sk_buff *mpdu);
void
ath12k_dp_mon_rx_add_vendor_tlv(struct sk_buff *skb,
				struct ieee80211_rx_status *rxs,
				u16 vendor_data_len);
#endif
