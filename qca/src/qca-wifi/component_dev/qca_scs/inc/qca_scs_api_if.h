/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef SCS_API_IF_H
#define SCS_API_IF_H
#ifdef WLAN_SUPPORT_SCS

/* Element ID's */
#define SCS_API_ELEMID_SCS_IE 185
#define SCS_API_ELEMID_INT_ACC_CAT_PRIORITY 184
#define SCS_API_ELEMID_TCLAS 14
#define SCS_API_ELEMID_EXTN 255
#define SCS_API_ELEMID_TCLAS_PROCESS 44
#define SCS_API_ELEMID_EXT_QOS_ATTR 113

/* SCS Procedures */
#define SCS_API_SCS_ADD_RULE                      0
#define SCS_API_SCS_REMOVE_RULE                   1
#define SCS_API_SCS_CHANGE_RULE                   2
#define SCS_API_SCS_SUCCESS                       0
#define SCS_API_SCS_REQUEST_DECLINED              37
#define SCS_API_SCS_REQUESTED_TCLAS_NOT_SUPPORTED 56
#define SCS_API_SCS_INSUFFICIENT_TCLAS_PROCESSING_RESOURCES 57
#define SCS_API_SCS_TCLAS_PROCESSING_TERMINATED   97

/* STATUS Macros */
#define SCS_API_STATUS_SUCCESS     0
#define SCS_API_STATUS_E_INVAL     1
#define SCS_API_STATUS_E_NOSUPPORT 2

/* Traverse Macros */
#define SCS_API_SCS_GET_DIALOG_TOKEN(frm)                     (*((frm) + 2))
#define SCS_API_SCS_GET_SCSID(frm)                            (*((frm) + 2))
#define SCS_API_SCS_ID_TRAVERSE(frm)                           ((frm) + 2)
#define SCS_API_SCS_GET_REQ(frm)                              (*((frm) + 1))
#define SCS_API_SCS_INTRA_ACCESS_CATEGORY_ELEM_TRAVERSE(frm)   ((frm) + 2)
#define SCS_API_SCS_GET_INTRA_ACCESS_CATEGORY_PRI_ELEM(frm)   (*(frm))
#define SCS_API_SCS_GET_INTRA_ACCESS_CATEGORY_PRI(frm)        (*((frm) + 2))

enum scs_api_qos_attr_direction_enum {
	SCS_API_QOS_ATTR_DIRECTION_UPLINK = 0,
	SCS_API_QOS_ATTR_DIRECTION_DOWNLINK,
	SCS_API_QOS_ATTR_DIRECTION_DIRECT_LINK,
};

/* TCLAS Macros*/
typedef enum {
	SCS_API_WNM_TCLAS_CLASSIFIER_TYPE4 = 4,
	SCS_API_WNM_TCLAS_CLASSIFIER_TYPE10 = 10,
} SCS_API_WNM_TCLAS_CLASSIFIER;

typedef enum {
	SCS_API_WNM_TCLAS_CLAS4_VERSION_4 = 4,
	SCS_API_WNM_TCLAS_CLAS4_VERSION_6 = 6,
} SCS_API_WNM_TCLAS_VERSION;

/* QOS Attribute parse */
#define SCS_API_QOS_ATTR_CTRL_INFO_LEN                   4
#define SCS_API_QOS_ATTR_SERVICE_INTERVAL_LEN            4
#define SCS_API_QOS_ATTR_MIN_DATA_RATE_LEN               3
#define SCS_API_QOS_ATTR_DELAY_BOUND_LEN                 3
#define SCS_API_QOS_ATTR_MAX_MSDU_SIZE_LEN               2
#define SCS_API_QOS_ATTR_SERVICE_START_TIME_LEN          4
#define SCS_API_QOS_ATTR_SERVICE_START_TIME_LINK_ID_LEN  1
#define SCS_API_QOS_ATTR_MEAN_DATA_RATE_LEN              3
#define SCS_API_QOS_ATTR_BURST_SIZE_LEN                  4
#define SCS_API_QOS_ATTR_MSDU_LIFETIME_LEN               2
#define SCS_API_QOS_ATTR_MSDU_DELIVERY_RATIO_LEN         1
#define SCS_API_QOS_ATTR_MSDU_COUNT_EXPONENT_LEN         1
#define SCS_API_QOS_ATTR_MEDIUM_TIME_LEN                 2

#define SCS_API_QOS_ATTR_MAX_MSDU_SIZE_PRESENT               0x0001
#define SCS_API_QOS_ATTR_SERVICE_START_TIME_PRESENT          0x0002
#define SCS_API_QOS_ATTR_SERVICE_START_TIME_LINK_ID_PRESENT  0x0004
#define SCS_API_QOS_ATTR_MEAN_DATA_RATE_PRESENT              0x0008
#define SCS_API_QOS_ATTR_BURST_SIZE_PRESENT                  0x0010
#define SCS_API_QOS_ATTR_MSDU_LIFETIME_PRESENT               0x0020
#define SCS_API_QOS_ATTR_MSDU_DELIVERY_RATIO_PRESENT         0x0040
#define SCS_API_QOS_ATTR_MSDU_COUNT_EXPONENT_PRESENT         0x0080
#define SCS_API_QOS_ATTR_MEDIUM_TIME_PRESENT                 0x0100

#define SCS_API_QOS_CTRL_INFO_DIRECTION_MASK         0x00000003
#define SCS_API_QOS_CTRL_INFO_DIRECTION_SHIFT        0
#define SCS_API_QOS_CTRL_INFO_TID_MASK               0x0000003C
#define SCS_API_QOS_CTRL_INFO_TID_SHIFT              2
#define SCS_API_QOS_CTRL_INFO_USER_PRIORITY_MASK     0x000001C0
#define SCS_API_QOS_CTRL_INFO_USER_PRIORITY_SHIFT    6
#define SCS_API_QOS_CTRL_INFO_BITMAP_MASK            0x01FFFE00
#define SCS_API_QOS_CTRL_INFO_BITMAP_SHIFT           9
#define SCS_API_QOS_CTRL_INFO_LINK_ID_MASK           0x1E000000
#define SCS_API_QOS_CTRL_INFO_LINK_ID_SHIFT          25

#define SCS_API_GET_FIELD_FROM_QOS_CTRL_INFO(ctrl_info, FIELD) \
	(((ctrl_info) & SCS_API_QOS_CTRL_INFO_##FIELD##_MASK) >> \
				SCS_API_QOS_CTRL_INFO_##FIELD##_SHIFT)

#define SCS_API_CHECK_QOS_ATTR_PRESENT(bitmap, FIELD) \
	((bitmap) & SCS_API_QOS_ATTR_##FIELD##_PRESENT)

#define SCS_API_SCS_OUTPUT_TID_MASK 0x7

/* Size */
#define SCS_API_IPV4_LEN                4
#define SCS_API_IPV6_LEN                16
#define SCS_API_MAC_ADDR_SIZE           6
#define SCS_API_REQ_BUF_MAX_SIZE        2500
#define SCS_API_DATA_BUF_MAX_SIZE       3100
#define SCS_API_RESP_BUF_MAX_SIZE       100
#define SCS_API_SCS_MAX_SIZE            10
#define SCS_API_TCLAS_MAX_SIZE          5
#define SCS_API_TCLAS10_FILTER_LEN      12
#define SCS_API_FLOW_LABEL_SIZE         3
#define SCS_API_MIN_QOS_ATTR_LEN        21
#define SCS_API_SAWF_SCS_SVC_CLASS_MIN  129
#define WIFI_IFACE_PREFIX_LEN           4

/* SCS API message types */
enum scs_api_msg_type {
	SCS_API_MSG_TYPE_INIT = 1,
	SCS_API_MSG_TYPE_DEINIT = 2,
	SCS_API_MSG_TYPE_SCS_REQ = 3,
	SCS_API_MSG_TYPE_SCS_REQ_PARSED_DATA = 4,
	SCS_API_MSG_TYPE_DRIVER_RESPONSE = 5,
	SCS_API_MSG_TYPE_SCS_RESP = 6,
	SCS_API_MSG_TYPE_MAX,
};

struct scs_api_info {
	uint8_t scs_app_support_enable:1,
		frame_fwd_support:1,
		nw_mgr_support:1,
		reserved:5;
};

struct scs_api_intra_access_priority_category_elem {
	uint8_t elem;
	uint8_t ie_len;
	uint8_t intra_access_priority;
};

struct scs_api_tclas_tuple_type4 {
	uint8_t mask;
	uint8_t version;
	union {
		uint8_t ipv4[SCS_API_IPV4_LEN];
		uint8_t ipv6[SCS_API_IPV6_LEN];
	} src_ip;
	union {
		uint8_t ipv4[SCS_API_IPV4_LEN];
		uint8_t ipv6[SCS_API_IPV6_LEN];
	} dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t dscp;
	/* Protocol in case of IPv4 and Next_Header in case of IPv6 */
	uint8_t next_header;
	uint8_t reserved;
	/* Flow_Label is applicable only in case of IPv6 */
	uint8_t flow_label[SCS_API_FLOW_LABEL_SIZE];
};

struct scs_api_tclas_tuple_type10 {
	uint8_t protocol_instance;
	uint8_t protocol_number;
	uint8_t filter_len;
	uint8_t filter_mask[SCS_API_TCLAS10_FILTER_LEN];
	uint8_t filter_val[SCS_API_TCLAS10_FILTER_LEN];
};

struct scs_api_tclas_tuple {
	uint8_t len;
	uint8_t up;
	uint8_t type;
	union scs_api_tclas {
		struct scs_api_tclas_tuple_type4 type4;
		struct scs_api_tclas_tuple_type10 type10;
	} scs_api_tclas;
};

typedef struct scs_api_tclas_processing {
	uint8_t elem_id;
	uint8_t length;
	uint8_t tclas_process;
} scs_api_tclas_processing;

struct scs_api_qos_attr {
	uint32_t direction:2,
		tid:4,
		user_priority:3,
		bitmap:16,
		link_id:4,
		reserved:3;
	uint32_t min_service_interval;
	uint32_t max_service_interval;
	uint32_t min_data_rate;
	uint32_t delay_bound;
	uint16_t max_msdu_size;
	uint32_t service_start_time;
	uint8_t service_start_time_link_id;
	uint32_t mean_data_rate;
	uint32_t burst_size;
	uint16_t msdu_lifetime;
	uint8_t msdu_delivery_ratio;
	uint8_t msdu_count_exponent;
	uint16_t medium_time;
};

struct scs_api_data {
	uint8_t scsid;
	uint8_t parsing_status_code;
	uint8_t request_type;
	uint8_t access_priority;
	uint8_t tclas_elements;
	bool is_qos_present;
	struct scs_api_tclas_tuple scs_api_tclas_tuple[SCS_API_TCLAS_MAX_SIZE];
	struct scs_api_tclas_processing scs_api_tclas_processing;
	struct scs_api_qos_attr qos_attr;
};

struct scs_api_drv_resp {
	uint8_t scs_id;
	uint8_t svc_id; /* Service Class ID */
	uint8_t status_code;
};

#endif
#endif
