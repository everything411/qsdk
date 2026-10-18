/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_HTT_WIFI8_H
#define ATH12K_DP_HTT_WIFI8_H

#include "hal.h"
#include "dp_peer.h"

/* vdev meta data */
#define HTT_TCL_META_DATA_VDEV_ID_V3			GENMASK(8, 1)
#define HTT_TCL_META_DATA_PDEV_ID_V3			GENMASK(10, 9)
#define HTT_TCL_META_DATA_HOST_INSPECTED_MISSION_V3	BIT(11)

/* QoS meta data */
#define HTT_TCL_META_DATA_SAWF_SVC_ID_V3	GENMASK(8, 1)
#define HTT_TCL_META_DATA_SAWF_TID_OVERRIDE_V3	BIT(10)

/* Global sequence number */
#define HTT_TCL_META_DATA_GSN_INSPECTED_V3		BIT(12)
#define HTT_TCL_META_DATA_GLOBAL_SEQ_NUM_V3		GENMASK(11, 0)
#define HTT_TCL_META_DATA_GLOBAL_HTT_EXT_PRESENT_V3	BIT(13)

/**
 * @brief host -> target message to provide msduq and mpduq for given new tid in a peer
 *
 * MSG_TYPE => HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO_HDR
 *
 * @details
 * struct htt_h2t_mpduq_and_msduq_info_hdr:
 *    HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO_HDR message is sent by the host to
 *    target to tell the amount of valid mpduq and msduq info tlv in term of bytes
 *    are present in the htt payload
 *
 *-------------------------------------------------------------------
 *|           htt_h2t_mpduq_and_msduq_info_hdr                      |
 *-------------------------------------------------------------------
 *|           mpduq_info_tlv  -> tid 0 , peer_id 1                  |
 *-------------------------------------------------------------------
 *|           mpduq_info_tlv  -> tid 6,  peer_id 1                  |
 *-------------------------------------------------------------------
 *|           msduq_info_tlv  -> tid 0,  peer_id 1                  |
 *-------------------------------------------------------------------
 *|           msduq_info_tlv  -> tid 0,  peer_id 1                  |
 *-------------------------------------------------------------------
 *|           msduq_info_tlv  -> tid 6,  peer_id 1                  |
 *-------------------------------------------------------------------
 *
 * As shown in the sample a single htt buffer can hold multiple mpduq
 * and msduq info tlvs, the info within the tlvs will indicate the peer and tid
 * for which they belong
 *
 */

#define HTT_MPDUQ_AND_MSDUQ_INFO_HDR_INFO0_MSG_TYPE	GENMASK(7, 0)
#define HTT_MPDUQ_AND_MSDUQ_INFO_HDR_INFO0_PAYLOAD_SIZE	GENMASK(19, 8)

struct htt_mpduq_and_msduq_info_hdr {
	__le32 info0;
} __packed;

enum htt_tx_tid_msduq_mpdu_type {
	HTT_TID_MSDUQ_NONUDP,
	HTT_TID_MSDUQ_UDP,
	HTT_TID_MSDUQ_CUSTOM_0,
	HTT_TID_MSDUQ_CUSTOM_1,
	HTT_TID_MSDUQ_CUSTOM_2,
	HTT_TID_MSDUQ_CUSTOM_3,
	HTT_TID_MSDUQ_CUSTOM_4,
	HTT_TID_MSDUQ_CUSTOM_5,
	HTT_TID_MISC_MSDUQ_TYPE_START,
	HTT_TID_MSDUQ_HOL = HTT_TID_MISC_MSDUQ_TYPE_START,
	HTT_TID_MSDUQ_MCAST,
	HTT_TID_MSDUQ_FAST_ROAMING,
	HTT_TID_MSDUQ_DATA_TYPE_END = 23,
	HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_0,
	HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_1,
	HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_2,
	HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_3,
	HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_4,
	HTT_TID_MSDUQ_MGMT_LINK_AGNOSTIC,
	HTT_TID_MPDUQ_TYPE,
	HTT_TID_MSDUQ_MPDUQ_TYPE_END,
};

/*
 * Enum to denote tid nums that can be used
 * first 8 {0 - 7} numbers correspond to data access category
 * {8 - 15} are for user defined usecases
 * 16 is used to denote the non-qos tid
 */
enum htt_tx_tid {
	HTT_DEFAULT_TID_NUM		= 0,
	HTT_MAX_VALID_DATA_TID_NUM	= 7,
	HTT_NON_QOS_TID_NUM		= 16,
	HTT_MAX_TID_NUM		= 31,
};

enum htt_sdwf_svc_inst_req_type {
	HTT_SDWF_SVC_INST_CREATE_REQ = 0,
	HTT_SDWF_SVC_INST_DEACTIVATE_REQ,
	HTT_SDWF_SVC_INST_REACTIVATE_REQ,
	HTT_SDWF_SVC_INST_DELETE_REQ,
};

/**
 * @brief host -> target message to provide mpduq for a tid in a peer
 *
 * MSG_TYPE => HTT_H2T_MSG_TYPE_MPDUQ_or_MSDUQ_INFO
 *
 * @details
 * struct htt_h2t_mpduq_or_msduq_info:
 *    HTT_H2T_MSG_TYPE_MPDUQ_OR_MSDUQ_INFO message is sent by the host to
 *    update the configuration of new MPDUQ/MSDUQ for a tid in a peer.
 *    This message supports the following configuration
 *    1.  Info provided per MPDUQ
 *        mpduq 32 bit address
 *        pn address space
 *    2. Info provided per MSDUQ
 *        msduq 32 bit address
 *        flow_number
 *        service class id
 *
 *
 *
 * The message is interpreted as follows for mpduq type:
 * dword0 - b'7:0   - msg_type: Identifies msduq and mpduq info to fw
 *                    This will be set to 0x2A
 *                    (HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO)
 *          b'12:8  - msduq_mpduq_type : indicate how to interpret the message for
 *                    mpduq type it will be fixed value of 30 based on enum
 *                    HTT_TID_MSDUQ_MPDUQ_TYPE
 *          b'16:13 - hw_link_id: Indicates which HW link the message is intended for.
 *                    This HW link ID is mainly relevant for split PHY
 *                    usecases to identify the correct link in same SOC.
 * dword1 - b'31:0  - mpduq_address_39_8: 256 byte aligned mpduq address,
 *                    since lower two octets are zero for 256 byte aligned addresses
 *                    just passing upper 32 bits of 40 bit address
 * dword2 - b'23:0  - mpduq_number: Queue number for mpduq
 *                    Note that the mpduq_number is formed from the combination of
 *                    peer_id (in bits 11:0)
 *                    tid_num (in bits 16:12)
 *                    msduq_mpduq_type (in bits 21:17)
 *          b'31:24 - pn_addr_32_39: Upper 40 bits for pn address
 * dword3 - b'31:0  - pn_addr_0_31: First 32 bits for pn address
 * dword4 - b'0:0   - sam_mpduq_allocated: Indicates whether a SAM MPDUQ has
 *                    been allocated for this TQM MPDUQ.
 *          b'11:1  - sam_mpduq_id: The SAM MPDUQ ID assigned by the host for
 *                    this TQM MPDUQ. The value in this field is ignored if
 *                    sam_mpduq_allocated is 0.
 *          b'13:12 - sam_mpduq_priority: The SAM priority assigned by the
 *                    host. A SAM priority MUST ALWAYS be provided for chipsets
 *                    that have SAM support, regardless of whether or not a SAM
 *                    MPDUQ has been allocated.
 *          b'14:14 - sam_mpduq_sched_eligible: Indicates that this SAM MPDUQ
 *                    is eligible for autonomous SAM scheduling. The value in
 *                    this field is ignored if sam_mpduq_allocated is 0.
 *          b'31:15 – reserved
 * Additional reserved dwords for future use cases
 *
 *
 *
 *
 * The message is interpreted as follows for any msduq type:
 * dword0 - b'7:0   - msg_type: Identifies msduq info to fw
 *                    This will be set to 0x2A
 *                    (HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO)
 *          b'12:8  - msduq_mpduq_type : type of the msduq based on enum
 *                    HTT_TID_MSDUQ_MPDUQ_TYPE
 *          b'16:13 - hw_link_id: Indicates which HW link the message is intended for.
 *                    This HW link ID is mainly relevant for split PHY
 *                    usecases to identify the correct link in same SOC.
 * dword1 - b'23:0  - tx_msduq_number : Queue number for the  msduq
 *                    Note that the tx_msduq_number is formed from the combination of
 *                    peer_id (in bits 11:0)
 *                    tid_num (in bits 16:12)
 *                    msduq_mpduq_type (in bits 21:17)
 *          b'31:24 - svc_class_id : service class id of the msduq
 * dword2 - b'31:0  - msduq_address_39_8: 256 byte aligned msduq address, since lower
 *                    two octets are zero for 256 byte aligned addresses just passing
 *                    upper 32 bits of 40 bit address
 * dword3 - b'0:0   - svc_inst_req_type_valid: Indicates whether
 *                    svc_inst_req_type is valid.
 *          b'3:1   - svc_inst_req_type: Contains one of the values from
 *                    HTT_SAWF_SVC_INST_REQ_TYPE. Only valid if
 *                    svc_inst_req_type_valid is 1.
 *          b'4:4   - sam_msduq_allocated: Indicates whether a SAM MSDUQ has
 *                    been allocated for this TQM MSDUQ.
 *          b'17:5  - sam_msduq_id: The SAM MSDUQ ID assigned by the host for
 *                    this TQM MSDUQ. The value in this field is ignored if
 *                    sam_msduq_allocated is 0.
 *          b'19:18 - sam_msduq_priority: The SAM priority assigned by the
 *                    host. A SAM priority MUST ALWAYS be provided for chipsets
 *                    that have SAM support, regardless of whether or not a SAM
 *                    MSDUQ has been allocated.
 *          b'20:20 - sam_msduq_sched_eligible: Indicates that this SAM MSDUQ
 *                    is eligible for autonomous SAM scheduling. The value in
 *                    this field is ignored if sam_msduq_allocated is 0.
 *          b'31:21 – reserved
 * Additional reserved dwords for future use cases
 *
 *
 */

#define HTT_MPDUQ_INFO_CMD_INFO1_MPDUQ_ADDR			GENMASK(31, 0)
#define HTT_MPDUQ_INFO_CMD_INFO2_MPDUQ_NUM			GENMASK(23, 0)
#define HTT_MPDUQ_INFO_CMD_INFO2_PN_ADDR_39_32			GENMASK(31, 24)
#define HTT_MPDUQ_INFO_CMD_INFO3_PN_ADDR_31_0			GENMASK(31, 0)
#define HTT_MPDUQ_INFO_CMD_INFO4_SAM_MPDUQ_ALLOCATED		BIT(0)
#define HTT_MPDUQ_INFO_CMD_INFO4_SAM_MPDUQ_ID			GENMASK(11, 1)
#define HTT_MPDUQ_INFO_CMD_INFO4_SAM_MPDUQ_PRIORITY		GENMASK(13, 12)
#define HTT_MPDUQ_INFO_CMD_INFO4_SAM_MPDUQ_SCHED_ELIGIBLE	BIT(14)

struct htt_mpduq_info {
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 info7;
	__le32 info8;
	__le32 info9;
} __packed;

#define HTT_MSDUQ_INFO_CMD_INFO1_MSDUQ_NUM			GENMASK(23, 0)
#define HTT_MSDUQ_INFO_CMD_INFO1_SVC_CLASS_ID			GENMASK(31, 24)
#define HTT_MSDUQ_INFO_CMD_INFO2_MSDUQ_ADDR			GENMASK(31, 0)
#define HTT_MSDUQ_INFO_CMD_INFO3_SVC_INST_REQ_TYPE_VALID	BIT(0)
#define HTT_MSDUQ_INFO_CMD_INFO3_SVC_INST_REQ_TYPE		GENMASK(3, 1)
#define HTT_MSDUQ_INFO_CMD_INFO3_SAM_MSDUQ_ALLOCATED		BIT(4)
#define HTT_MSDUQ_INFO_CMD_INFO3_SAM_MSDUQ_ID			GENMASK(17, 5)
#define HTT_MPDUQ_INFO_CMD_INFO3_SAM_MSDUQ_PRIORITY		GENMASK(19, 18)
#define HTT_MPDUQ_INFO_CMD_INFO3_SAM_MSDUQ_SCHED_ELIGIBLE	BIT(20)
#define HTT_MPDUQ_INFO_CMD_INFO3_TXOP_INTENT			BIT(21)
#define HTT_MPDUQ_INFO_CMD_INFO3_QMID				GENMASK(29, 22)

struct htt_msduq_info {
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 info7;
	__le32 info8;
	__le32 info9;
} __packed;

#define HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_MSG_TYPE	GENMASK(7, 0)
#define HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_Q_TYPE	GENMASK(12, 8)
#define HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_HW_LINK_ID	GENMASK(16, 13)

struct htt_mpduq_or_msduq_info {
	__le32 info0;
	union {
		struct htt_mpduq_info mpduq;
		struct htt_msduq_info msduq;
	};
} __packed;

/*
 * @brief  host -> target HTT_AST_INFO message
 *
 * MSG_TYPE => HTT_H2T_MSG_TYPE_AST_INFO
 *
 *    The message would appear as follows:
 *    |31           24|23             16|15|14           8|7                  0|
 *    |---------------+-----------------+-----------------+--------------------|
 *    |ast_max_search |          ast_table_size           |     msg_type       |
 *    |------------------------------------------------------------------------|
 *    |                          ast_base_addr_31_0                            |
 *    |------------------------------------------------------------------------|
 *    |                          ase_hash_key1                                 |
 *    |------------------------------------------------------------------------|
 *    |                          ase_hash_key2                                 |
 *    |------------------------------------------------------------------------|
 *    |                          ase_hash_key3                                 |
 *    |------------------------------------------------------------------------|
 *    |                          reserved                 | ast_base_addr_39_32|
 *    |------------------------------------------------------------------------|
 *
 * The message is interpreted as follows:
 * dword0    b'7:0   - msg_type
 *           b'23:8  -  ast table size
 *           b'31:24 - ast max search
 * dword1  - b'31:0  - ast table base address low
 * dword2  - b'31:0  - ase hash key 1
 * dword3  - b'31:0  - ase hash key 2
 * dword4  - b'31:0  - ase hash key 3
 * dword5    b'7:0   - ast table base address high
 *           b'31:8 -  reserved for future use cases
 */

#define HTT_AST_INFO0_MSG_TYPE		GENMASK(7, 0)
#define HTT_AST_INFO0_TABLE_SIZE	GENMASK(23, 8)
#define HTT_AST_INFO0_MAX_SEARCH	GENMASK(31, 24)
#define HTT_AST_INFO1_BASE_ADDR_31_0	GENMASK(31, 0)
#define HTT_AST_INFO2_HASH_KEY_1	GENMASK(31, 0)
#define HTT_AST_INFO3_HASH_KEY_2	GENMASK(31, 0)
#define HTT_AST_INFO4_HASH_KEY_3	GENMASK(31, 0)
#define HTT_AST_INFO5_BASE_ADDR_39_32	GENMASK(7, 0)

struct htt_ast_info_t {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
} __packed;

/* MSG_TYPE => HTT_T2H_MSG_TYPE_GLOBAL_PEER_ID_UNMAP
 *
 * The following diagram shows the format of the global peer unmap message sent
 * from the target to the host. This message is used to send unmap event to host
 * after tid and msduq/mpduq cleanup in FW, host cleans up msduq/mpduq based on
 * message.
 *
 * |31             24|23             20|19              8|7               0|
 * |-----------------------------------------------------------------------|
 * |   reserved      |   hw_link_id    | global_peer_id  |     msg type    |
 * |-----------------------------------------------------------------------|
 * @details
 * struct htt_t2h_global_peer_id_unmap:
 *
 * The message is interpreted as follows:
 * dword0 - b'7:0   - msg_type: This will be set to 0x3e
 *                    (HTT_T2H_MSG_TYPE_GLOBAL_PEER_ID_UNMAP)
 *          b'19:8  - global_peer_id : global peer id assigned by host
 *          b'23:20 - hw_link_id : hw link id for which unmap is being sent
 *
 */

#define HTT_T2H_GLOBAL_PEER_ID_UNMAP_PEER_ID	GENMASK(19, 8)
#define HTT_T2H_GLOBAL_PEER_ID_UNMAP_HW_LINK_ID	GENMASK(23, 20)

struct htt_t2h_global_peer_id_unmap {
	__le32 info;
} __packed;

#define ATH12K_MAX_DP_HTT_MSG_LEN 512

int ath12k_dp_tx_htt_peer_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					    struct ath12k_dp_peer *dp_peer,
					    struct list_head *mpduq_pending_list_head,
					    struct list_head *msduq_pending_list_head,
					    u8 link_id,
					    bool is_mcast_queues);
int ath12k_dp_rx_htt_ast_info_setup(struct ath12k_base *ab,
				    struct ath12k_hal_ast_param *ast_param);
void ath12k_dp_htt_peer_cleanup_indication(struct ath12k_dp *dp,
					   struct sk_buff *skb);
int ath12k_wifi8_dp_msdu_htt_connect(struct ath12k_dp *dp);
void ath12k_dp_tx_htt_retry_work(struct work_struct *work);
#endif
