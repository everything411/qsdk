/*
 * SPDX-License-Identifier: ISC
 */
#ifndef ROBUST_AV_H
#define ROBUST_AV_H


struct hostapd_data;
struct sta_info;

#include "utils/list.h"

#define LOW_BYTE(val)    ((val & 0x000000ff))
#define HIGH_BYTE(val)   ((val & 0x0000ff00) >> 8)

#define HOSTAPD_SCS_MAX_DESCRIPTORS_PER_PEER		255
#define HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST	4
#define HOSTAPD_SCS_MAX_TCLAS_ELEMENTS_PER_DESCRIPTOR	2

#define HOSTAPD_TCLAS_FLOW_LABEL_SIZE	3
#define HOSTAPD_TCLAS10_FILTER_LEN	12

#define HOSTAPD_SCS_QOS_ATTR_MIN_LEN	21

/* IEEE Std 802.11-2024 - 9.4.2.326 QoS Characteristics element
 * Figure 9-1074bd—Control Info field format
 */
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_DIRECTION_MASK	0x00000003
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_DIRECTION_SHIFT	0
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_TID_MASK		0x0000003C
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_TID_SHIFT	2
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_UP_MASK		0x000001C0
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_UP_SHIFT		6
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_BITMAP_MASK	0x01FFFE00
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_BITMAP_SHIFT	9
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_LINK_ID_MASK	0x1E000000
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_LINK_ID_SHIFT	25

/* IEEE Std 802.11-2024 - 9.4.2.326 QoS Characteristics element
 * Figure 9-1074bc—QoS Characteristics element format
 */
#define HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_LEN			4
#define HOSTAPD_SCS_QOS_ATTR_SERVICE_INTERVAL_LEN		4
#define HOSTAPD_SCS_QOS_ATTR_MIN_DATA_RATE_LEN			3
#define HOSTAPD_SCS_QOS_ATTR_DELAY_BOULND_LEN			3
#define HOSTAPD_SCS_QOS_ATTR_MAX_MSDU_SIZE_LEN			2
#define HOSTAPD_SCS_QOS_ATTR_SERVICE_START_TIME_LEN		4
#define HOSTAPD_SCS_QOS_ATTR_SERVICE_START_TIME_LINK_ID_LEN	1
#define HOSTAPD_SCS_QOS_ATTR_MEAN_DATA_RATE_LEN			3
#define HOSTAPD_SCS_QOS_ATTR_BURST_SIZE_LEN			4
#define HOSTAPD_SCS_QOS_ATTR_MSDU_LIFETIME_LEN			2
#define HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_LEN		1
#define HOSTAPD_SCS_QOS_ATTR_MEDIUM_TIME_LEN			2

#define HOSTAPD_SCS_QOS_ATTR_MAX_MSDU_SIZE_MASK			0x0001
#define HOSTAPD_SCS_QOS_ATTR_SERVICE_START_TIME_MASK		0x0002
#define HOSTAPD_SCS_QOS_ATTR_SERVICE_START_TIME_LINK_ID_MASK	0x0004
#define HOSTAPD_SCS_QOS_ATTR_MEAN_DATA_RATE_MASK		0x0008
#define HOSTAPD_SCS_QOS_ATTR_BURST_SIZE_MASK			0x0010
#define HOSTAPD_SCS_QOS_ATTR_MSDU_LIFETIME_MASK			0x0020
#define HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_MASK		0x0040
#define HOSTAPD_SCS_QOS_ATTR_MEDIUM_TIME_MASK			0x0080

#define HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_RATIO_MASK		0x0F
#define HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_RATIO_SHIFT		0
#define HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_COUNT_EXPONENT_MASK	0xF0
#define HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_COUNT_EXPONENT_SHIFT	4

#define le8toh(x)	x

#define HOSTAPD_GET_QOS_ATTR_CTRL_INFO(ctrl_info, PARAM) \
	((ctrl_info & HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_##PARAM##_MASK) >> \
	 HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_##PARAM##_SHIFT)

#define HOSTAPD_GET_QOS_ATTR_MSDU_DELIVERY_INFO(msdu_delivery_info, PARAM) \
	((msdu_delivery_info & \
		  HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_##PARAM##_MASK) >> \
	 HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_##PARAM##_SHIFT)

#define HOSTAPD_GET_QOS_ATTR_ACTUAL(var, payload, temp_payload, PARAM, bit) \
	do { \
		var = (le##bit##toh(*((u##bit *)temp_payload))); \
		payload += HOSTAPD_SCS_QOS_ATTR_##PARAM##_LEN; \
	} while (0)

#define HOSTAPD_GET_QOS_ATTR(var, payload, PARAM, bit) \
	do { \
		var = (le##bit##toh(*((u##bit *)payload))); \
		payload += HOSTAPD_SCS_QOS_ATTR_##PARAM##_LEN; \
	} while (0)

#define HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(PARAM1, PARAM2) \
	(PARAM1 & HOSTAPD_SCS_QOS_ATTR_##PARAM2##_MASK)

#define HOSTAPD_QOS_SCS_TAG	0xB9
#define HOSTAPD_QOS_MSCS_TAG	0x58

#define NFT_MAX_RULE_COUNT	256
#define NFT_RULE_MAX_WEIGHT	8

#define HOSTAPD_MSCS_WLAN_EID_SUBELEMENT 0
#define HOSTAPD_QM_DEFAULT_QM_ID 0xFF
#define HOSTAPD_MSCS_MAX_FLOW_ENTRIES 255

#define HOSTAPD_MAX_RULES_PER_TCLAS 2

/* QoS MGMT status values */
enum hostapd_qm_status {
	HOSTAPD_QM_STATUS_SUCCESS = 0,
	HOSTAPD_QM_STATUS_DECLINED = 1,
	HOSTAPD_QM_STATUS_E_INVAL = 2,
	HOSTAPD_QM_STATUS_E_NOSUPPORT = 3,
};

struct hostapd_tclas4_params {
	u8 classifier_mask;
	u8 ip_ver;
	union {
		u8 ipv4[IPV4_LEN];
		u8 ipv6[IPV6_LEN];
	} src_ip;
	union {
		u8 ipv4[IPV4_LEN];
		u8 ipv6[IPV6_LEN];
	} dst_ip;
	u16 src_port;
	u16 dst_port;
	u8 dscp;
	u8 protocol;
	u8 next_header;
	u8 flow_label[HOSTAPD_TCLAS_FLOW_LABEL_SIZE];
};

struct hostapd_tclas10_params {
	u8 protocol_instance;
	u8 protocol_number;
	u8 filter_value[HOSTAPD_TCLAS10_FILTER_LEN];
	u8 filter_mask[HOSTAPD_TCLAS10_FILTER_LEN];
	u8 filter_len;
};

struct hostapd_tclas_elements {
	u8 up;
	u8 classifier_type;
	union tclas_elem {
		struct hostapd_tclas4_params type4_params;
		struct hostapd_tclas10_params type10_params;
	} tclas_elem;
	u8 num_rules;
	u64 rule_handle[HOSTAPD_MAX_RULES_PER_TCLAS];
};

struct hostapd_scs_qos_attributes {
	u32 direction:2,
	    tid:4,
	    up:3,
	    bitmap:16,
	    link_id:4,
	    reserved:3;
	u32 min_service_interval;
	u32 max_service_interval;
	u32 min_data_rate;
	u32 delay_bound;
	u16 max_msdu_size;
	u32 service_start_time;
	u8 service_start_time_link_id;
	u32 mean_data_rate;
	u32 burst_size;
	u16 msdu_lifetime;
	u8 msdu_delivery_ratio:4,
	   msdu_count_exponent:4;
	u16 medium_time;
};

struct hostapd_scs_req_desc_data {
	u8 scs_id;
	u8 request_type;
	u8 intra_access_priority;
	u8 num_tclas_elements;
	struct hostapd_tclas_elements
			tclas[HOSTAPD_SCS_MAX_TCLAS_ELEMENTS_PER_DESCRIPTOR];
	u8 tclas_processing;
	bool is_qos_present;
#ifdef CONFIG_IEEE80211BE
	struct hostapd_scs_qos_attributes qos_attr;
#endif /* CONFIG_IEEE80211BE */
	struct dl_list list;
	u8 request_pending:1,      /* deferred; not yet programmed */
	   client_initiated_scs:1; /* request originated from STA */
	u16 qm_id;                 /* QM identifier; 0xFFFF = unassigned */
	u8 scs_sta_mac[ETH_ALEN];  /* end-client STA MAC; used as NFT dmac
				    * equals sta->addr for terminating node,
				    * end-client MAC for intermediate node */
	bool dedicated_queue;
};

struct hostapd_scs_req_data {
	u8 peer_mac[ETH_ALEN];
	u8 dialog_token;
	u8 num_scs_desc;
	struct hostapd_scs_req_desc_data
			scs_req_desc[HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST];
};

struct hostapd_scs_resp_desc_data {
	u8 scs_id;
	u8 status;
};

struct hostapd_scs_resp_data {
	u8 dialog_token;
	u8 num_scs_desc;
	struct hostapd_scs_resp_desc_data
			scs_resp_desc[HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST];
};

struct hostapd_scs_raw_desc {
	const u8 *data;
	u16 len;
};

struct hostapd_mscs_resp {
	u8 status;
	struct hostapd_mscs_desc *mscs;
};

struct hostapd_user_priority_control {
	u8 user_priority_bitmap;
	u8 user_priority_limit:3;
};

struct hostapd_mscs_ctxt {
	u8 user_priority_bitmap;
	u8 user_priority_limit:3;
	u8 tclas_mask;
	u8 available_idx;
	struct hostapd_tclas_elements flow_info[HOSTAPD_MSCS_MAX_FLOW_ENTRIES];
	u16 assoc_req_status;
};

struct hostapd_tclas_mask_elem {
	u8 id;
	u8 ie_len;
	u8 id_ext;
	u8 classifier_type;
	u8 classifier_mask;
};

struct hostapd_mscs_desc {
	u8 req_type;
	struct hostapd_user_priority_control user_priority_control;
	u32 stream_timeout;
	struct hostapd_tclas_mask_elem tclas_mask_elem;
};

size_t hostapd_wfa_cap_ie_len(struct hostapd_data *hapd,
                              struct sta_info *sta);
u8 *hostapd_add_wfa_cap_ie(struct hostapd_data *hapd,
                           struct sta_info *sta,
                           u8 *eid);

int hostapd_scs_configure(struct hostapd_data *hapd, const u8 *peer_mac,
			  const u8 *scs_sta_mac, u16 qm_id, const u8 *desc_buf,
			  u8 desc_len, bool dedicated_queue);
int hostapd_dump_scs_list(struct hostapd_data *hapd, struct sta_info *sta,
			  char *buf, size_t buflen);

int hostapd_dump_scs_info(struct hostapd_data *hapd, struct sta_info *sta,
			  char *buf, size_t buflen, u8 scs_id);
int hostapd_dump_scs_qm_info(struct hostapd_data *hapd,
			     struct sta_info *sta,
			     char *buf, size_t buflen, u16 qm_id);

int hostapd_send_unsolicited_scs_resp(struct hostapd_data *hapd,
				      struct sta_info *sta, u8 scs_id,
				      u8 req_type);
void
hostapd_handle_robust_av(struct hostapd_data *hapd, const u8 *buf, size_t len);
void hostapd_handle_mscs(struct hostapd_data *hapd, const u8 *buf, size_t len);
void hostapd_process_mscs_flow(struct hostapd_data *hapd,
			       struct hostapd_tclas_elements *tclas,
			       u8 *addr, u8 tid);
int hostapd_send_mscs_response(struct hostapd_data *hapd,
			       struct sta_info *sta, const u8 *da,
			       u8 dialog_token, int status_code);
int hostapd_copy_and_send_mscs_data(struct hostapd_data *hapd,
				    struct sta_info *sta, u8 req_type,
				    const u8 dialog_token);
u8 *hostapd_add_mscs_desc(struct hostapd_data *hapd, u8 *eid,
			  struct sta_info *sta);
int hostapd_handle_mscs_ie_assoc(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 const u8 *buf, u8 len);
void hostapd_mscs_delete_nft_rules(struct hostapd_data *hapd,
				    struct sta_info *sta);
#endif
