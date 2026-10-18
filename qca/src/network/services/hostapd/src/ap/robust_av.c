/*
 * SPDX-License-Identifier: ISC
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "hostapd.h"
#include "robust_av.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "ap_drv_ops.h"
#include <linux/netfilter.h>


static struct hostapd_scs_req_desc_data *
hostapd_scs_find_by_id(struct sta_info *sta, u8 scs_id)
{
	struct hostapd_scs_req_desc_data *desc;

	dl_list_for_each(desc, &sta->scs_req_desc,
			 struct hostapd_scs_req_desc_data, list) {
		if (desc->scs_id == scs_id)
			return desc;
	}
	return NULL;
}





/*
 * hostapd_scs_find_client_initiated - find a client-initiated pending entry.
 *
 * Returns the descriptor with matching scs_id that has client_initiated_scs
 * set, or NULL if not found.
 */
static struct hostapd_scs_req_desc_data *
hostapd_scs_find_client_initiated(struct sta_info *sta, u8 scs_id)
{
	struct hostapd_scs_req_desc_data *desc;

	dl_list_for_each(desc, &sta->scs_req_desc,
			 struct hostapd_scs_req_desc_data, list) {
		if (desc->scs_id == scs_id && desc->client_initiated_scs)
			return desc;
	}
	return NULL;
}

/*
 * hostapd_scs_find_by_qmid - find a stored descriptor by QM ID.
 *
 * Returns the descriptor whose qm_id matches, or NULL if not found
 * (including when qm_id == 0xFFFF which means unassigned).
 */
static struct hostapd_scs_req_desc_data *
hostapd_scs_find_by_qmid(struct sta_info *sta, u16 qm_id)
{
	struct hostapd_scs_req_desc_data *desc;

	if (qm_id == 0xFFFF)
		return NULL;

	dl_list_for_each(desc, &sta->scs_req_desc,
			 struct hostapd_scs_req_desc_data, list) {
		if (desc->qm_id == qm_id)
			return desc;
	}
	return NULL;
}


int hostapd_dump_scs_list(struct hostapd_data *hapd, struct sta_info *sta,
			  char *buf, size_t buflen)
{
	struct hostapd_scs_req_desc_data *desc;
	struct sta_info *assoc_sta = NULL;
	struct hostapd_data *assoc_hapd;
	int reply_len = 0, res;
	int i = 0;

#ifdef CONFIG_IEEE80211BE
	if (!sta->mld_info.mld_sta) {
		wpa_printf(MSG_DEBUG,
			   "Assign sta to assoc_sta for Non-MLD STA");
		assoc_sta = sta;
	}
#endif

	assoc_hapd = hapd;

	if (!assoc_sta) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_ERROR,
				   "Assoc STA not found to dump scs list");
			return -1;
		}
	}

	if (!assoc_sta->scs_session_count) {
		res = os_snprintf(buf + reply_len, buflen - reply_len,
				  "No SCS sessions configured\n");
		if (os_snprintf_error(buflen - reply_len, res))
			return -1;

		reply_len += res;
		return reply_len;
	}

	res = os_snprintf(buf + reply_len, buflen - reply_len,
			  "Configured SCS sessions for the client "
			  MACSTR " are:\n", MAC2STR(assoc_sta->addr));

	if (os_snprintf_error(buflen - reply_len, res))
		return -1;
	reply_len += res;

	dl_list_for_each(desc, &assoc_sta->scs_req_desc,
			 struct hostapd_scs_req_desc_data, list) {
		if (!hapd->conf->deferred_scs)
			res = os_snprintf(buf + reply_len, buflen - reply_len,
					  "  Index: %d, SCS ID: %u\n",
					  i++, desc->scs_id);

		else
			res = os_snprintf(buf + reply_len, buflen - reply_len,
					  "  Index: %d, SCS ID: %u, QM ID: %u\n",
					  i++, desc->scs_id, desc->qm_id);
		if (os_snprintf_error(buflen - reply_len, res))
			return -1;
		reply_len += res;
	}

	return reply_len;
}


static int
hostapd_dump_scs_desc_info(struct hostapd_data *hapd,
			   struct sta_info *assoc_sta,
			   struct hostapd_scs_req_desc_data *desc,
			   char *buf, size_t buflen)
{
	const char *qos_type, *direction;
	int reply_len = 0, res;

#define APPEND(...) \
	do { \
		res = os_snprintf(buf + reply_len, buflen - reply_len, \
				  __VA_ARGS__); \
		if (os_snprintf_error(buflen - reply_len, res)) \
			return -1; \
		reply_len += res; \
	} while (0)

	APPEND("SCS Info for STA: " MACSTR "\n", MAC2STR(assoc_sta->addr));
	APPEND("Number of SCS Descriptors: %u\n\n",
	       assoc_sta->scs_session_count);

	qos_type = desc->is_qos_present ? "QoS R3" : "QoS R2";
	direction = "Downlink"; /* Default value */

#ifdef CONFIG_IEEE80211BE
	if (desc->is_qos_present)
		direction = (desc->qos_attr.direction == 0) ?
			    "Uplink" : "Downlink";
#endif

	APPEND("Descriptor type: %s %s\n", qos_type, direction);
	APPEND("  SCS ID: %u\n", desc->scs_id);
	if (hapd->conf->deferred_scs) {
		if (desc->qm_id != 0xFFFF)
			APPEND("  QM ID: %u\n", desc->qm_id);
		else
			APPEND("  QM ID: unassigned\n");
	}
	APPEND("  Request Type: %u\n", desc->request_type);
	if (hapd->conf->deferred_scs) {
		APPEND("  Request Pending: %s\n",
		       desc->request_pending ? "yes" : "no");
		APPEND("  Client Initiated: %s\n",
		       desc->client_initiated_scs ? "yes" : "no");
	}
	APPEND("  Intra Access Priority: %u\n",
	       desc->intra_access_priority);
	APPEND("  TCLAS Processing: %u\n",
	       desc->tclas_processing);
	APPEND("  Number of TCLAS Elements: %u\n",
	       desc->num_tclas_elements);

	for (int j = 0; j < desc->num_tclas_elements; j++) {
		struct hostapd_tclas_elements *tclas = &desc->tclas[j];

		APPEND("\n  TCLAS Element %d:\n", j);
		APPEND("    UP: %u\n", tclas->up);
		APPEND("    Classifier Type: %u\n",
		       tclas->classifier_type);
		if (tclas->classifier_type == 4) {
			struct hostapd_tclas4_params *tclas4 =
					&tclas->tclas_elem.type4_params;
			APPEND("    IP Version: %u\n", tclas4->ip_ver);
			if (tclas4->ip_ver == 4) {
				APPEND("    Src IP: %u.%u.%u.%u\n",
				       tclas4->src_ip.ipv4[0],
				       tclas4->src_ip.ipv4[1],
				       tclas4->src_ip.ipv4[2],
				       tclas4->src_ip.ipv4[3]);
				APPEND("    Dst IP: %u.%u.%u.%u\n",
				       tclas4->dst_ip.ipv4[0],
				       tclas4->dst_ip.ipv4[1],
				       tclas4->dst_ip.ipv4[2],
				       tclas4->dst_ip.ipv4[3]);
				APPEND("    Protocol: %u\n",
				       tclas4->protocol);
			} else if (tclas4->ip_ver == 6) {
				APPEND("    Src IP: ");
				for (int k = 0; k < 16; k += 2) {
					if (k > 0)
						APPEND(":");
					APPEND("%02x%02x",
					       tclas4->src_ip.ipv6[k],
					       tclas4->src_ip.ipv6[k + 1]);
				}
				APPEND("\n");

				APPEND("    Dst IP: ");
				for (int k = 0; k < 16; k += 2) {
					if (k > 0)
						APPEND(":");
					APPEND("%02x%02x",
					       tclas4->dst_ip.ipv6[k],
					       tclas4->dst_ip.ipv6[k + 1]);
				}
				APPEND("\n");

				APPEND("    Next Header: %u\n",
				       tclas4->next_header);
				APPEND("    Flow Label: %02x %02x %02x\n",
				       tclas4->flow_label[0],
				       tclas4->flow_label[1],
				       tclas4->flow_label[2]);
			}

			APPEND("    Src Port: %u\n", tclas4->src_port);
			APPEND("    Dst Port: %u\n", tclas4->dst_port);
			APPEND("    DSCP: %u\n", tclas4->dscp);

			} else if (tclas->classifier_type == 10) {
				struct hostapd_tclas10_params *tclas10 =
				       &tclas->tclas_elem.type10_params;
				APPEND("    Protocol Instance: %u\n",
				       tclas10->protocol_instance);
				APPEND("    Protocol Number: %u\n",
				       tclas10->protocol_number);
				APPEND("    Filter Value: ");
				for (int k = 0; k < tclas10->filter_len; k++)
					APPEND("%02x ",
					       tclas10->filter_value[k]);
				APPEND("\n    Filter Mask: ");
				for (int k = 0; k < tclas10->filter_len; k++)
					APPEND("%02x ",
					       tclas10->filter_mask[k]);
				APPEND("\n    Filter Length: %u\n",
				       tclas10->filter_len);
			}
		}

#ifdef CONFIG_IEEE80211BE
	if (desc->is_qos_present) {
		struct hostapd_scs_qos_attributes *qos_attr = &desc->qos_attr;

		APPEND("\n  QoS Attributes:\n");
		APPEND("    Direction: %s\n", qos_attr->direction == 0 ?
		       "Uplink" : "Downlink");
		APPEND("    TID: %u\n", qos_attr->tid);
		APPEND("    UP: %u\n", qos_attr->up);
		APPEND("    Bitmap: 0x%04x\n", qos_attr->bitmap);
		APPEND("    Link ID: %u\n", qos_attr->link_id);
		APPEND("    Min Service Interval: %u (us)\n",
		       qos_attr->min_service_interval);
		APPEND("    Max Service Interval: %u (us)\n",
		       qos_attr->max_service_interval);
		APPEND("    Min Data Rate: %u (kbps)\n",
		       qos_attr->min_data_rate);
		APPEND("    Delay Bound: %u (us)\n",
		       qos_attr->delay_bound);
		APPEND("    Max MSDU Size: %u\n",
		       qos_attr->max_msdu_size);
		APPEND("    Service Start Time: %u (us)\n",
		       qos_attr->service_start_time);
		APPEND("    Service Start Time Link ID: %u\n",
		       qos_attr->service_start_time_link_id);
		APPEND("    Mean Data Rate: %u (kbps)\n",
		       qos_attr->mean_data_rate);
		APPEND("    Burst Size: %u (bytes)\n",
		       qos_attr->burst_size);
		APPEND("    MSDU Lifetime: %u (ms)\n",
		       qos_attr->msdu_lifetime);
		APPEND("    MSDU Delivery Ratio: %u\n",
		       qos_attr->msdu_delivery_ratio);
		APPEND("    MSDU Count Exponent: %u\n",
		       qos_attr->msdu_count_exponent);
		APPEND("    Medium Time: %u (256us/s)\n",
		       qos_attr->medium_time);
	}
#endif
	APPEND("\n");

	return reply_len;
}

int hostapd_dump_scs_info(struct hostapd_data *hapd, struct sta_info *sta,
			  char *buf, size_t buflen, u8 scs_id)
{
	struct hostapd_scs_req_desc_data *desc;
	struct sta_info *assoc_sta = NULL;
	struct hostapd_data *assoc_hapd;
	int reply_len = 0, res;

#ifdef CONFIG_IEEE80211BE
	if (!sta->mld_info.mld_sta) {
		wpa_printf(MSG_DEBUG,
			   "Assign sta to assoc_sta for Non-MLD STA");
		assoc_sta = sta;
	}
#endif

	assoc_hapd = hapd;

	if (!assoc_sta) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_ERROR,
				   "Assoc STA not found to dump scs info");
			return -1;
		}
	}

	if (!assoc_sta->scs_session_count) {
		res = os_snprintf(buf + reply_len, buflen - reply_len,
				  "No SCS sessions configured\n");
		if (os_snprintf_error(buflen - reply_len, res))
			return -1;

		reply_len += res;
		return reply_len;
	}

	desc = hostapd_scs_find_by_id(assoc_sta, scs_id);
	if (!desc) {
		wpa_printf(MSG_ERROR, "SCS ID %u not found for STA " MACSTR,
			   scs_id, MAC2STR(assoc_sta->addr));
		return -1;
	}

	return hostapd_dump_scs_desc_info(hapd, assoc_sta, desc, buf, buflen);
}


int hostapd_dump_scs_qm_info(struct hostapd_data *hapd, struct sta_info *sta,
			     char *buf, size_t buflen, u16 qm_id)
{
	struct hostapd_scs_req_desc_data *desc;
	struct sta_info *assoc_sta = NULL;
	struct hostapd_data *assoc_hapd;
	int reply_len = 0, res;

#ifdef CONFIG_IEEE80211BE
	if (!sta->mld_info.mld_sta) {
		wpa_printf(MSG_DEBUG,
			   "Assign sta to assoc_sta for Non-MLD STA");
		assoc_sta = sta;
	}
#endif

	assoc_hapd = hapd;

	if (!assoc_sta) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_ERROR,
				   "Assoc STA not found to dump scs qm info");
			return -1;
		}
	}

	if (!assoc_sta->scs_session_count) {
		res = os_snprintf(buf + reply_len, buflen - reply_len,
				  "No SCS sessions configured\n");
		if (os_snprintf_error(buflen - reply_len, res))
			return -1;

		reply_len += res;
		return reply_len;
	}

	desc = hostapd_scs_find_by_qmid(assoc_sta, qm_id);
	if (!desc) {
		wpa_printf(MSG_ERROR, "QM ID %u not found for STA " MACSTR,
			   qm_id, MAC2STR(assoc_sta->addr));
		return -1;
	}

	return hostapd_dump_scs_desc_info(hapd, assoc_sta, desc, buf, buflen);
}


static int hostapd_parse_tclas4_params(const u8 **payload,
				       union tclas_elem *tclas_elem)
{
	enum qm_ip_ver ip_version;

	tclas_elem->type4_params.classifier_mask = *(*payload)++;
	tclas_elem->type4_params.ip_ver = *(*payload)++;
	ip_version = tclas_elem->type4_params.ip_ver;

	wpa_printf(MSG_DEBUG, "TCLAS4: Classifier Mask: 0x%x, IP Version: %u",
		   tclas_elem->type4_params.classifier_mask, ip_version);

	switch (ip_version) {
	case IP_VERSION_4:
		os_memcpy(tclas_elem->type4_params.src_ip.ipv4, *payload,
			  IPV4_LEN);
		*payload += IPV4_LEN;
		os_memcpy(tclas_elem->type4_params.dst_ip.ipv4, *payload,
			  IPV4_LEN);
		*payload += IPV4_LEN;

		wpa_hexdump(MSG_DEBUG, "IPv4 SRC IP",
			    tclas_elem->type4_params.src_ip.ipv4, IPV4_LEN);
		wpa_hexdump(MSG_DEBUG, "IPv4 DST IP",
			    tclas_elem->type4_params.dst_ip.ipv4, IPV4_LEN);

		break;

	case IP_VERSION_6:
		os_memcpy(tclas_elem->type4_params.src_ip.ipv6, *payload,
			  IPV6_LEN);
		*payload += IPV6_LEN;
		os_memcpy(tclas_elem->type4_params.dst_ip.ipv6, *payload,
			  IPV6_LEN);
		*payload += IPV6_LEN;

		wpa_hexdump(MSG_DEBUG, "IPv6 SRC IP",
			    tclas_elem->type4_params.src_ip.ipv6, IPV6_LEN);
		wpa_hexdump(MSG_DEBUG, "IPv6 DST IP",
			    tclas_elem->type4_params.dst_ip.ipv6, IPV6_LEN);

		break;

	default:
		wpa_printf(MSG_ERROR, "Invalid TCLAS4 IP version");
		return HOSTAPD_QM_STATUS_E_INVAL;
	}

	tclas_elem->type4_params.src_port = WPA_GET_BE16(*payload);
	*payload += 2;
	tclas_elem->type4_params.dst_port = WPA_GET_BE16(*payload);
	*payload += 2;
	tclas_elem->type4_params.dscp = *(*payload)++;


	wpa_printf(MSG_DEBUG, "SRC port: %u, DST port: %u, DSCP: %u",
		   tclas_elem->type4_params.src_port,
		   tclas_elem->type4_params.dst_port,
		   tclas_elem->type4_params.dscp);

	if (ip_version == IP_VERSION_4) {
		tclas_elem->type4_params.protocol = *(*payload)++;
		wpa_printf(MSG_DEBUG, "Protocol: %u",
			   tclas_elem->type4_params.protocol);
		/* Reserved octet */
		*payload += 1;

	} else if (ip_version == IP_VERSION_6) {
		tclas_elem->type4_params.next_header = *(*payload)++;
		os_memcpy(tclas_elem->type4_params.flow_label, *payload,
			  HOSTAPD_TCLAS_FLOW_LABEL_SIZE);
		*payload += HOSTAPD_TCLAS_FLOW_LABEL_SIZE;

		wpa_printf(MSG_DEBUG, "Next header: %u",
			   tclas_elem->type4_params.next_header);
		wpa_hexdump(MSG_DEBUG, "Flow Label",
			    tclas_elem->type4_params.flow_label,
			    HOSTAPD_TCLAS_FLOW_LABEL_SIZE);
	}

	wpa_printf(MSG_DEBUG, "TCLAS4 element parsing complete");

	return HOSTAPD_QM_STATUS_SUCCESS;
}


static int hostapd_parse_tclas10_params(const u8 **payload,
					union tclas_elem *tclas_elem,
					u8 tclas_len)
{
	u8 filter_len;

	tclas_elem->type10_params.protocol_instance = *(*payload)++;
	tclas_elem->type10_params.protocol_number = *(*payload)++;

	wpa_printf(MSG_DEBUG, "Protocol Instance: %u, Protocol Number: %u",
		   tclas_elem->type10_params.protocol_instance,
		   tclas_elem->type10_params.protocol_number);

	/* Calculating filter length from total Tclas element length.
	 * tclas_len contains length of Elem ID (1) + Length (1) + UP (1) +
	 * Classifier type (1) + Proto Instance (1) + Protocol number (1) +
	 * Filter mask (Variable) + Filter value (Variable).
	 * Subtracting fixed length field (6) from tclas_len to calculate
	 * filter length value for filter_value and filter_mask
	 */
	filter_len = (tclas_len - 6) / 2;
	if ((filter_len < 0) || (filter_len > HOSTAPD_TCLAS10_FILTER_LEN)) {
		wpa_printf(MSG_ERROR, "TCLAS10 filter length Invalid");
		return HOSTAPD_QM_STATUS_E_INVAL;
	}

	tclas_elem->type10_params.filter_len = filter_len;
	os_memcpy(tclas_elem->type10_params.filter_value, *payload, filter_len);
	*payload += filter_len;
	os_memcpy(tclas_elem->type10_params.filter_mask, *payload, filter_len);
	*payload += filter_len;

	wpa_printf(MSG_DEBUG, "Filter Len: %u", filter_len);
	wpa_hexdump(MSG_DEBUG, "Filter Value",
		    tclas_elem->type10_params.filter_value, filter_len);
	wpa_hexdump(MSG_DEBUG, "Filter Mask",
		    tclas_elem->type10_params.filter_mask, filter_len);

	wpa_printf(MSG_DEBUG, "TCLAS10 element parsing complete");

	return HOSTAPD_QM_STATUS_SUCCESS;
}


static int hostapd_parse_scs_tclas_elements(
			const u8 **payload,
			struct hostapd_scs_req_desc_data *scs_req_desc, u8 *len)
{
	struct hostapd_tclas_elements *tclas_tuple = NULL;
	union tclas_elem *tclas_elem = NULL;
	int ret = HOSTAPD_QM_STATUS_SUCCESS;
	u8 elem_id, type, tclas_len;
	u8 tclas_idx = 0;

	while (*len > 0) {
		elem_id = **payload;
		/* Process only TCLAS element(s) and TCLAS Processing element
		 * in this loop
		 */
		if (elem_id != WLAN_EID_TCLAS &&
		    elem_id != WLAN_EID_TCLAS_PROCESSING)
			break;

		*payload += 1;
		if (elem_id == WLAN_EID_TCLAS) {
			if (tclas_idx >=
			    HOSTAPD_SCS_MAX_TCLAS_ELEMENTS_PER_DESCRIPTOR) {
				wpa_printf(MSG_ERROR, "TCLAS: Max elements per "
					   "request exceeded");
				ret = HOSTAPD_QM_STATUS_E_NOSUPPORT;
				goto fail;
			}

			tclas_len = *(*payload)++;

			/* Add elem_id, length to tclas_len */
			tclas_len += 2;

			if (*len < tclas_len) {
				wpa_printf(MSG_ERROR, "Invalid TCLAS length:%d",
					   tclas_len);
				ret = HOSTAPD_QM_STATUS_E_INVAL;
				goto fail;
			}

			tclas_tuple = &scs_req_desc->tclas[tclas_idx];
			tclas_elem = &tclas_tuple->tclas_elem;
			tclas_tuple->up = *(*payload)++;
			tclas_tuple->classifier_type = *(*payload)++;
			type = tclas_tuple->classifier_type;

			wpa_printf(MSG_DEBUG,
				   "TCLAS[%u]: UP:%u, Classifier type:%u",
				   tclas_idx, tclas_tuple->up,
				   tclas_tuple->classifier_type);

			switch (type) {
			case QM_TCLAS_CLASSIFIER_TYPE4:
				ret = hostapd_parse_tclas4_params(payload,
								  tclas_elem);
				if (ret)
					goto fail;
				break;
			case QM_TCLAS_CLASSIFIER_TYPE10:
				ret = hostapd_parse_tclas10_params(payload,
								   tclas_elem,
								   tclas_len);
				if (ret)
					goto fail;
				break;
			default:
				wpa_printf(MSG_ERROR, "Unknown tclas "
					   "classifier: %d", type);
				ret = HOSTAPD_QM_STATUS_E_INVAL;
				goto fail;
			}

			tclas_idx++;
			*len -= tclas_len;

		} else if (elem_id == WLAN_EID_TCLAS_PROCESSING) {
			/* TCLAS processing element length */
			*payload += 1;
			scs_req_desc->tclas_processing = *(*payload)++;
			/* Length of Element ID, len, TCLAS processing field */
			*len -= 3;
			wpa_printf(MSG_DEBUG, "Tclas processing value:%d",
				   scs_req_desc->tclas_processing);
		}

		scs_req_desc->num_tclas_elements = tclas_idx;
		wpa_printf(MSG_INFO, "TCLAS element parsing complete, num:%u",
			   scs_req_desc->num_tclas_elements);
	}

fail:
	return ret;
}


#ifdef CONFIG_IEEE80211BE
static int hostapd_parse_scs_qos_attributes(
				const u8 *payload,
				struct hostapd_scs_req_desc_data *scs_req_desc,
				u8 *len)
{
	struct hostapd_scs_qos_attributes *qos_attr;
	u8 elem_id, elem_id_extension;
	u8 qos_length, temp_length;
	u8 msdu_delivery_info;
	u16 bitmap;
	u32 control_info;
	int ret = HOSTAPD_QM_STATUS_SUCCESS;

	if (*len < HOSTAPD_SCS_QOS_ATTR_MIN_LEN)
		return ret;

	elem_id = *payload++;
	if (elem_id != WLAN_EID_EXTENSION)
		return ret;

	qos_length = *payload++;
	temp_length = qos_length;
	elem_id_extension = *payload++;

	if (elem_id_extension != WLAN_EID_EXT_QOS_CHARACTERISTICS) {
		wpa_printf(MSG_ERROR, "Invalid QoS characteristics element ID:%d",
			   elem_id_extension);
		ret = HOSTAPD_QM_STATUS_E_INVAL;
		goto fail;
	}

	/* Subtract length of elemid_extension */
	qos_length -= 1;

	qos_attr = &scs_req_desc->qos_attr;

	/* Parse QoS attributes - Mandatory parameters*/
	/* Control Info */
	HOSTAPD_GET_QOS_ATTR(control_info, payload, CTRL_INFO, 32);

	qos_attr->direction = HOSTAPD_GET_QOS_ATTR_CTRL_INFO(control_info,
							     DIRECTION);
	if (qos_attr->direction == SCS_DIRECTION_DIRECT) {
		wpa_printf(MSG_ERROR, "SCS for direct link is not supported");
		ret = HOSTAPD_QM_STATUS_E_NOSUPPORT;
		goto fail;
	}
	qos_attr->tid = HOSTAPD_GET_QOS_ATTR_CTRL_INFO(control_info, TID);
	qos_attr->up = HOSTAPD_GET_QOS_ATTR_CTRL_INFO(control_info, UP);
	qos_attr->bitmap = HOSTAPD_GET_QOS_ATTR_CTRL_INFO(control_info, BITMAP);
	qos_attr->link_id = HOSTAPD_GET_QOS_ATTR_CTRL_INFO(control_info,
							   LINK_ID);
	qos_length -= HOSTAPD_SCS_QOS_ATTR_CTRL_INFO_LEN;

	wpa_printf(MSG_DEBUG, "*** QoS Chacteristics Element ***");
	wpa_printf(MSG_DEBUG, "Control Info Params: Direction: %u, TID: %u, "
		   "UP: %u, Bitmap: 0x%x, Link_ID: %u", qos_attr->direction,
		   qos_attr->tid, qos_attr->up, qos_attr->bitmap,
		   qos_attr->link_id);

	/* Min Service Interval */
	HOSTAPD_GET_QOS_ATTR(qos_attr->min_service_interval, payload,
			     SERVICE_INTERVAL, 32);
	qos_length -= HOSTAPD_SCS_QOS_ATTR_SERVICE_INTERVAL_LEN;

	/* Max Service Interval */
	HOSTAPD_GET_QOS_ATTR(qos_attr->max_service_interval, payload,
			     SERVICE_INTERVAL, 32);
	qos_length -= HOSTAPD_SCS_QOS_ATTR_SERVICE_INTERVAL_LEN;

	/* Min Data rate */
	os_memcpy(&qos_attr->min_data_rate, payload,
		  HOSTAPD_SCS_QOS_ATTR_MIN_DATA_RATE_LEN);
	HOSTAPD_GET_QOS_ATTR_ACTUAL(qos_attr->min_data_rate, payload,
				    &qos_attr->min_data_rate, MIN_DATA_RATE,
				    32);
	qos_length -= HOSTAPD_SCS_QOS_ATTR_MIN_DATA_RATE_LEN;

	/* Delay Bound */
	os_memcpy(&qos_attr->delay_bound, payload,
		  HOSTAPD_SCS_QOS_ATTR_DELAY_BOULND_LEN);
	HOSTAPD_GET_QOS_ATTR_ACTUAL(qos_attr->delay_bound, payload,
				    &qos_attr->delay_bound, DELAY_BOULND, 32);
	qos_length -= HOSTAPD_SCS_QOS_ATTR_DELAY_BOULND_LEN;

	/* Parse QoS attributes - Optional parameters */
	bitmap = scs_req_desc->qos_attr.bitmap;

	wpa_printf(MSG_DEBUG, "Mandatory QoS Params: Min_Service_Interval: %u, "
		   "Max_Service_Interval: %u, Min_Data_Rate: %u, "
		   "Delay_Bound: %u", qos_attr->min_service_interval,
		   qos_attr->max_service_interval, qos_attr->min_data_rate,
		   qos_attr->delay_bound);

	/* Max MSDU Size */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, MAX_MSDU_SIZE)) {
		HOSTAPD_GET_QOS_ATTR(qos_attr->max_msdu_size, payload,
				     MAX_MSDU_SIZE, 16);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_MAX_MSDU_SIZE_LEN;
	}

	/* Service Start Time */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, SERVICE_START_TIME)) {
		HOSTAPD_GET_QOS_ATTR(qos_attr->service_start_time, payload,
				     SERVICE_START_TIME, 32);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_SERVICE_START_TIME_LEN;
	}

	/* Service Start Time Link ID */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap,
					    SERVICE_START_TIME_LINK_ID)) {
		HOSTAPD_GET_QOS_ATTR(qos_attr->service_start_time_link_id,
				     payload, SERVICE_START_TIME_LINK_ID, 8);
		qos_length -=
			HOSTAPD_SCS_QOS_ATTR_SERVICE_START_TIME_LINK_ID_LEN;
	}

	/* Mean Data Rate */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, MEAN_DATA_RATE)) {
		os_memcpy(&qos_attr->mean_data_rate, payload,
			  HOSTAPD_SCS_QOS_ATTR_MEAN_DATA_RATE_LEN);
		HOSTAPD_GET_QOS_ATTR_ACTUAL(qos_attr->mean_data_rate, payload,
					    &qos_attr->mean_data_rate,
					    MEAN_DATA_RATE, 32);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_MEAN_DATA_RATE_LEN;
	}

	/* Burst Size */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, BURST_SIZE)) {
		HOSTAPD_GET_QOS_ATTR(qos_attr->burst_size, payload,
				     BURST_SIZE, 32);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_BURST_SIZE_LEN;
	}

	/* MSDU Lifetime */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, MSDU_LIFETIME)) {
		HOSTAPD_GET_QOS_ATTR(qos_attr->msdu_lifetime, payload,
				     MSDU_LIFETIME, 16);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_MSDU_LIFETIME_LEN;
	}

	/* MSDU Delivery Info */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, MSDU_DELIVERY_INFO)) {
		HOSTAPD_GET_QOS_ATTR(msdu_delivery_info, payload,
				     MSDU_DELIVERY_INFO, 8);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_MSDU_DELIVERY_INFO_LEN;

		/* MSDU Delivery Ratio */
		qos_attr->msdu_delivery_ratio =
			HOSTAPD_GET_QOS_ATTR_MSDU_DELIVERY_INFO(
					msdu_delivery_info, RATIO);

		/* MSDU Count Exponent */
		qos_attr->msdu_count_exponent =
			HOSTAPD_GET_QOS_ATTR_MSDU_DELIVERY_INFO(
					msdu_delivery_info, COUNT_EXPONENT);
	}

	/* Medium Time */
	if ((qos_length > 0) &&
	    HOSTAPD_SCS_IS_QOS_ATTR_PRESENT(bitmap, MEDIUM_TIME)) {
		HOSTAPD_GET_QOS_ATTR(qos_attr->medium_time, payload,
				     MEDIUM_TIME, 16);
		qos_length -= HOSTAPD_SCS_QOS_ATTR_MEDIUM_TIME_LEN;
	}

	scs_req_desc->is_qos_present = true;

	wpa_printf(MSG_DEBUG, "Optional QoS params: Max_MSDU_Size: %u, "
		   "Service_Start_Time: %u, Service_Start_Time_Link_ID: %u, "
		   "Mean_Data_Rate: %u, Burst_Size: %u, MSDU_Lifetime: %u, "
		   "MSDU_Delivery_Ratio: %u, MSDU_Count_Exponent: %u, "
		   "Medium_Time: %u", qos_attr->max_msdu_size,
		   qos_attr->service_start_time,
		   qos_attr->service_start_time_link_id,
		   qos_attr->mean_data_rate, qos_attr->burst_size,
		   qos_attr->msdu_lifetime, qos_attr->msdu_delivery_ratio,
		   qos_attr->msdu_count_exponent, qos_attr->medium_time);


	if (qos_length == 0)
		wpa_printf(MSG_INFO, "SCS QoS attributes parsing complete");

	/* Subtracting length of (Elem ID and length field) - 2 bytes and
	 * QOS attr length from the frm_length
	 */
	*len = *len - 2 - temp_length;

fail:
	return ret;
}
#endif /* CONFIG_IEEE80211BE */



/*
 * hostapd_scs_validate_desc - validate SCS descriptor against DB state.
 *
 * Encapsulates all DB-state checks for SCS ADD/UPDATE/DELETE across
 * non-deferred, deferred client-frame and deferred CLI paths.
 *
 * Returns 0 if the operation is permitted, -1 if it should be declined.
 */
static int hostapd_scs_validate_desc(struct sta_info *sta,
				     u8 scs_id, u8 req_type,
				     bool deferred_scs,
				     bool is_action_frame_from_sta,
				     u16 qm_id)
{
	struct hostapd_scs_req_desc_data *client_entry;
	struct hostapd_scs_req_desc_data *qmid_entry;

	if (!deferred_scs) {
		/*
		 * Non-deferred path — SCS ID is the key.
		 * Only client can send SCS requests; one entry per scs_id.
		 */
		client_entry = hostapd_scs_find_client_initiated(sta, scs_id);

		if (req_type == QM_ADD_REQ) {
			if (sta->scs_session_count >=
					HOSTAPD_SCS_MAX_DESCRIPTORS_PER_PEER) {
				wpa_printf(MSG_ERROR,
					   "AP has already configured maximum "
					   "supported SCS desc per peer");
				return -1;
			}
			if (client_entry) {
				wpa_printf(MSG_ERROR,
					   "Client-initiated entry already "
					   "exists for scs_id:%u", scs_id);
				return -1;
			}
		} else {
			/* UPDATE or DELETE */
			if (!sta->scs_session_count) {
				wpa_printf(MSG_ERROR,
					   "Request Declined: SCS session "
					   "inactive");
				return -1;
			}
			if (!client_entry) {
				wpa_printf(MSG_ERROR,
					   "Request Declined: no client-initiated "
					   "entry for scs_id:%u", scs_id);
				return -1;
			}
		}

	} else {
		if (is_action_frame_from_sta) {
			/*
			 * Deferred path, client SCS frame — qm_id not available.
			 * Use client_initiated flag as discriminator.
			 */
			client_entry = hostapd_scs_find_client_initiated(sta,
									  scs_id);

			if (req_type == QM_ADD_REQ) {
				if (client_entry) {
					wpa_printf(MSG_ERROR,
						   "Deferred SCS: client-initiated "
						   "entry already exists for "
						   "scs_id:%u", scs_id);
					return -1;
				}
			} else {
				/* UPDATE or DELETE */
				if (!client_entry) {
					wpa_printf(MSG_ERROR,
						   "Deferred SCS: no client-initiated "
						   "entry for scs_id:%u", scs_id);
					return -1;
				}
			}

		} else {
			/*
			 * Deferred path, CLI/controller — qm_id is the key.
			 */
			qmid_entry   = hostapd_scs_find_by_qmid(sta, qm_id);
			client_entry = hostapd_scs_find_client_initiated(sta,
									 scs_id);

			if (req_type == QM_ADD_REQ) {
				if (qmid_entry) {
					wpa_printf(MSG_ERROR,
						   "SCS_CONFIGURE ADD: qm_id=%u "
						   "already exists", qm_id);
					return -1;
				}
				if (client_entry && client_entry->request_pending) {
					wpa_printf(MSG_ERROR,
						   "SCS_CONFIGURE ADD: client-initiated "
						   "entry exists and pending for scs_id=%u, "
						   "use UPDATE", scs_id);
					return -1;
				}
			} else if (req_type == QM_CHANGE_REQ) {
				if (!qmid_entry) {
					if (!(client_entry &&
					      client_entry->request_pending)) {
						wpa_printf(MSG_ERROR,
							   "SCS_CONFIGURE UPDATE: no "
							   "valid entry for scs_id=%u "
							   "qm_id=%u", scs_id, qm_id);
						return -1;
					}
				}
			} else {
				/* DELETE */
				if (!qmid_entry) {
					wpa_printf(MSG_ERROR,
						   "SCS_CONFIGURE DELETE: qm_id=%u "
						   "not found", qm_id);
					return -1;
				}
			}
		}
	}

	return 0;
}

static int hostapd_parse_scs_desc(
		const u8 *payload, struct sta_info *sta,
		struct hostapd_scs_req_desc_data *scs_req_desc, u8 len,
		bool deferred_scs, bool is_action_frame_from_sta, u16 qm_id)
{
	int ret = WLAN_STATUS_REQUEST_DECLINED;
	u8 scs_id, req_type;
	u8 elem_id;

	wpa_hexdump(MSG_MSGDUMP, "SCS Request - Descriptor wise dump",
		    payload, len);

	/* SCS ID */
	scs_req_desc->scs_id = *payload++;
	/* Request Type */
	scs_req_desc->request_type = *payload++;
	len -= 2;

	/* Default QM ID — updated when application assigns one */
	scs_req_desc->qm_id = 0xFFFF;
	/* Default scs_sta_mac to the directly associated STA; overridden
	 * by hostapd_scs_configure for the intermediate (repeater) case */
	os_memcpy(scs_req_desc->scs_sta_mac, sta->addr, ETH_ALEN);

	scs_id = scs_req_desc->scs_id;
	req_type = scs_req_desc->request_type;

	wpa_printf(MSG_INFO, "SCS ID:%u, Request type:%u, len:%u",
		   scs_id, req_type, len);

	if (hostapd_scs_validate_desc(sta, scs_id, req_type,
				      deferred_scs, is_action_frame_from_sta,
				      qm_id) != 0)
		goto decline;

	/* Only SCS ID and request type are present in Remove request */
	if (req_type == SCS_REQ_REMOVE)
		return WLAN_STATUS_SUCCESS;

	elem_id = *payload;
	/* Parse Intra-Access category */
	if (elem_id == WLAN_EID_INTRA_ACCESS_CATEGORY_PRIORITY) {
		/* Element ID */
		payload++;
		/* Length */
		payload++;
		scs_req_desc->intra_access_priority = *payload++;
		/* Length of Intra Access Category element */
		len -= 3;
		wpa_printf(MSG_DEBUG, "Intra access priority:%u, len:%u",
			   scs_req_desc->intra_access_priority, len);
	}

	elem_id = *payload;
	/* Check if TCLAS element or QOS attributes is present or not */
	if (elem_id != WLAN_EID_TCLAS && elem_id != WLAN_EID_EXTENSION) {
		wpa_printf(MSG_ERROR, "Declining this request: Both "
			   "TCLAS element & QOS attributes are absent");
		goto decline;
	}

	/* Parse TCLAS elements */
	ret = hostapd_parse_scs_tclas_elements(&payload, scs_req_desc, &len);
	if (ret != HOSTAPD_QM_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR, "SCS TCLAS element parse error");
		ret = WLAN_STATUS_REQUEST_DECLINED;
		goto decline;
	}

	scs_req_desc->is_qos_present = false;

	/* Parse QoS attributes */
#ifdef CONFIG_IEEE80211BE
	ret = hostapd_parse_scs_qos_attributes(payload, scs_req_desc, &len);
	if (ret != HOSTAPD_QM_STATUS_SUCCESS) {
		ret = WLAN_STATUS_REQUEST_DECLINED;
		goto decline;
	}
#endif /* CONFIG_IEEE80211BE */

	wpa_printf(MSG_INFO, "SCS descriptor is parsed successfully, SCS ID:%u",
		   scs_id);

	if (len != 0)
		wpa_printf(MSG_DEBUG, "Optional subelements are also present, "
			  "len:%u", len);

	return WLAN_STATUS_SUCCESS;

decline:
	wpa_printf(MSG_ERROR, "Decline SCS descriptor - id:%u, type:%u, ret:%d",
		   scs_id, req_type, ret);
	return ret;
}


static void
hostapd_copy_tclas4_elem(struct qm_tclas_type4_params *qm_tclas4,
			 struct hostapd_tclas4_params scs_tclas4)
{
	qm_tclas4->classifier_mask = scs_tclas4.classifier_mask;
	qm_tclas4->ip_ver = scs_tclas4.ip_ver;
	qm_tclas4->src_port = scs_tclas4.src_port;
	qm_tclas4->dst_port = scs_tclas4.dst_port;
	qm_tclas4->dscp = scs_tclas4.dscp;

	if (qm_tclas4->ip_ver == IP_VERSION_4) {
		os_memcpy(qm_tclas4->src_ip.ipv4, scs_tclas4.src_ip.ipv4,
			  IPV4_LEN);
		os_memcpy(qm_tclas4->dst_ip.ipv4, scs_tclas4.dst_ip.ipv4,
			  IPV4_LEN);
		qm_tclas4->protocol = scs_tclas4.protocol;

	} else if (qm_tclas4->ip_ver == IP_VERSION_6) {
		os_memcpy(qm_tclas4->src_ip.ipv4, scs_tclas4.src_ip.ipv4,
			  IPV6_LEN);
		os_memcpy(qm_tclas4->dst_ip.ipv4, scs_tclas4.dst_ip.ipv4,
			  IPV6_LEN);
		qm_tclas4->next_header = scs_tclas4.next_header;
		os_memcpy(qm_tclas4->flow_label, scs_tclas4.flow_label,
			  TCLAS4_FLOW_LABEL_SIZE);
	}
}


static void
hostapd_copy_tclas10_elem(struct qm_tclas_type10_params *qm_tclas10,
			  struct hostapd_tclas10_params scs_tclas10)
{
	u8 filter_len;

	qm_tclas10->protocol_instance = scs_tclas10.protocol_instance;
	qm_tclas10->protocol_number = scs_tclas10.protocol_number;
	qm_tclas10->filter_len = scs_tclas10.filter_len;
	filter_len = qm_tclas10->filter_len;

	/* For SCS protocol, filter_len is equal for filter mask and value.
	 * This filter_len is extracted from frame parsing.
	 */
	os_memcpy(qm_tclas10->filter_value, scs_tclas10.filter_value,
		  filter_len);
	os_memcpy(qm_tclas10->filter_mask, scs_tclas10.filter_mask, filter_len);
}


static void
hostapd_copy_scs_tclas_elem(struct qm_tclas_elements *qm_tclas,
			    struct hostapd_tclas_elements scs_tclas)
{
	u8 type;

	qm_tclas->up = scs_tclas.up;
	qm_tclas->classifier_type = scs_tclas.classifier_type;
	type = qm_tclas->classifier_type;

	wpa_printf(MSG_DEBUG, "UP:%u, Classifier type:%u", qm_tclas->up,
		   qm_tclas->classifier_type);

	switch (type) {
	case QM_TCLAS_CLASSIFIER_TYPE4:
		hostapd_copy_tclas4_elem(&qm_tclas->tclas_elem.type4_params,
					 scs_tclas.tclas_elem.type4_params);
		break;
	case QM_TCLAS_CLASSIFIER_TYPE10:
		hostapd_copy_tclas10_elem(&qm_tclas->tclas_elem.type10_params,
					  scs_tclas.tclas_elem.type10_params);
		break;
	default:
		wpa_printf(MSG_ERROR, "Unknown TCLAS classifier:%d", type);
		break;
	}
}


static void hostapd_copy_scs_qos_attr(
			struct qm_qos_attributes *qm_qos_attr,
			struct hostapd_scs_qos_attributes scs_qos_attr)
{
	/* Control Info */
	qm_qos_attr->direction = scs_qos_attr.direction;
	qm_qos_attr->tid = scs_qos_attr.tid;
	qm_qos_attr->up = scs_qos_attr.up;
	qm_qos_attr->bitmap = scs_qos_attr.bitmap;
	qm_qos_attr->link_id = scs_qos_attr.link_id;

	/* Mandatory QoS Parameters */
	qm_qos_attr->min_service_interval = scs_qos_attr.min_service_interval;
	qm_qos_attr->max_service_interval = scs_qos_attr.max_service_interval;
	qm_qos_attr->min_data_rate = scs_qos_attr.min_data_rate;
	qm_qos_attr->delay_bound = scs_qos_attr.delay_bound;

	/* Optional QoS Parameters */
	qm_qos_attr->max_msdu_size = scs_qos_attr.max_msdu_size;
	qm_qos_attr->service_start_time = scs_qos_attr.service_start_time;
	qm_qos_attr->service_start_time_link_id =
				scs_qos_attr.service_start_time_link_id;
	qm_qos_attr->mean_data_rate = scs_qos_attr.mean_data_rate;
	qm_qos_attr->burst_size = scs_qos_attr.burst_size;
	qm_qos_attr->msdu_lifetime = scs_qos_attr.msdu_lifetime;
	qm_qos_attr->msdu_delivery_ratio = scs_qos_attr.msdu_delivery_ratio;
	qm_qos_attr->msdu_count_exponent = scs_qos_attr.msdu_count_exponent;
	qm_qos_attr->medium_time = scs_qos_attr.medium_time;
}


static void hostapd_copy_scs_desc(struct hostapd_data *hapd,
				  struct qm_req_desc_data *qm_data,
				  struct hostapd_scs_req_desc_data scs_data)
{
	int tclas_idx;

	if (hapd->conf->deferred_scs)
		qm_data->qm_id = scs_data.qm_id;
	else
		qm_data->qm_id = scs_data.scs_id;
	qm_data->request_type = scs_data.request_type;
	qm_data->priority = scs_data.intra_access_priority;
	qm_data->num_tclas_elements = scs_data.num_tclas_elements;

	wpa_printf(MSG_DEBUG, "QM ID:%u, Req type:%u, Priority:%u, Num TCLAS:%u",
		   qm_data->qm_id, qm_data->request_type, qm_data->priority,
		   qm_data->num_tclas_elements);

	/* Copy TCLAS elements if only present */
	for (tclas_idx = 0;
	     tclas_idx < qm_data->num_tclas_elements;
	     tclas_idx++) {
		hostapd_copy_scs_tclas_elem(&qm_data->tclas[tclas_idx],
					    scs_data.tclas[tclas_idx]);
	}

	/* Copy TCLAS Processing value if more than one TCLAS elem is present */
	if (qm_data->num_tclas_elements > 1)
		qm_data->tclas_processing = scs_data.tclas_processing;

	wpa_printf(MSG_DEBUG, "QoS attributes present:%s",
		   scs_data.is_qos_present ? "True" : "False");

	qm_data->is_qos_present = scs_data.is_qos_present;

#ifdef CONFIG_IEEE80211BE
	/* Copy QoS Attribute if only present */
	if (qm_data->is_qos_present)
		hostapd_copy_scs_qos_attr(&qm_data->qos_attr,
					  scs_data.qos_attr);
#endif

	qm_data->dedicated_queue = scs_data.dedicated_queue;
}


static void
hostapd_copy_scs_resp_desc(struct hostapd_scs_resp_desc_data *scs_resp_desc,
			   struct qm_resp_desc_data qm_resp_desc)
{
	scs_resp_desc->scs_id = qm_resp_desc.qm_id;
	scs_resp_desc->status = qm_resp_desc.status;

	wpa_printf(MSG_DEBUG, "SCS ID: %u, Status: %u",
		   scs_resp_desc->scs_id, scs_resp_desc->status);
}


static void hostapd_copy_scs_resp(struct hostapd_scs_resp_data *scs_resp,
				  struct qm_resp_data qm_resp)
{
	int idx;

	scs_resp->dialog_token = qm_resp.dialog_token;
	scs_resp->num_scs_desc = qm_resp.num_qm_desc;

	wpa_printf(MSG_DEBUG, "SCS Resp: Dialog Token: %u, Num SCS desc: %u",
		   scs_resp->dialog_token, scs_resp->num_scs_desc);

	for (idx = 0; idx < scs_resp->num_scs_desc; idx++) {
		wpa_printf(MSG_DEBUG, "SCS Resp Descriptor Index: %u", idx);
		hostapd_copy_scs_resp_desc(&(scs_resp->scs_resp_desc[idx]),
					   qm_resp.qm_resp_desc[idx]);
	}
}


static int
hostapd_copy_and_send_scs_data(struct hostapd_data *hapd,
			       struct hostapd_scs_req_data *scs_req,
			       struct hostapd_scs_resp_data *scs_resp)
{
	int ret = HOSTAPD_QM_STATUS_SUCCESS;
	struct qm_resp_data qm_resp = {0};
	struct qm_req_data qm_req = {0};
	int idx;

	if (!hapd->driver)
		return HOSTAPD_QM_STATUS_E_INVAL;

	os_memcpy(qm_req.peer_mac, scs_req->peer_mac, ETH_ALEN);
	wpa_printf(MSG_DEBUG, "STA MAC: " MACSTR, MAC2STR(qm_req.peer_mac));

	qm_req.qm_type = HOSTAPD_QM_TYPE_SCS;
	qm_req.dialog_token = scs_req->dialog_token;
	qm_req.num_qm_desc = scs_req->num_scs_desc;

	wpa_printf(MSG_DEBUG, "QM type: %u, Dialog token: %u, Num QM desc: %u",
		   qm_req.qm_type, qm_req.dialog_token, qm_req.num_qm_desc);

	for (idx = 0; idx < qm_req.num_qm_desc; idx++) {
		hostapd_copy_scs_desc(hapd, &qm_req.qm_req_desc[idx],
				      scs_req->scs_req_desc[idx]);
	}

	ret = hostapd_drv_set_qos(hapd, &qm_req, &qm_resp);
	if (ret == 0) {
		int ridx, qidx;

		hostapd_copy_scs_resp(scs_resp, qm_resp);

		/* In deferred mode the driver echoes back qm_id in the
		 * response. Translate it to scs_id so that
		 * hostapd_process_scs_req can match resp to req
		 * descriptors by scs_id as it always does. */
		if (hapd->conf->deferred_scs) {
			for (ridx = 0; ridx < scs_resp->num_scs_desc; ridx++) {
				u16 qm_id = scs_resp->scs_resp_desc[ridx].scs_id;

				for (qidx = 0; qidx < scs_req->num_scs_desc; qidx++) {
					if (scs_req->scs_req_desc[qidx].qm_id == qm_id) {
						scs_resp->scs_resp_desc[ridx].scs_id =
							scs_req->scs_req_desc[qidx].scs_id;
						break;
					}
				}
			}
		}
	} else {
		wpa_printf(MSG_ERROR, "set_qos failed, ret: %d", ret);
	}

	return ret;
}

static void hostapd_qm_prepare_nft_rule(struct hostapd_data *hapd,
					struct sta_info *sta,
					struct hostapd_tclas_elements *te,
					struct hostapd_nft_rule_params *rule,
					u8 qm_id, u8 qm_tag,
					const u8 *scs_sta_mac)
{
	struct hostapd_tclas4_params *type4_params = &te->tclas_elem.type4_params;
	struct hostapd_tclas10_params *type10_params = &te->tclas_elem.type10_params;

	if (te->classifier_type == QM_TCLAS_CLASSIFIER_TYPE4) {
		if (type4_params->classifier_mask & BIT(0)) {
			rule->ip_family = type4_params->ip_ver;
			rule->weight++;
		}

		if (type4_params->classifier_mask & BIT(1)) {
			if (rule->ip_family == 4) {
				os_memcpy(&rule->saddr4,
					  type4_params->src_ip.ipv4,
					  IPV4_LEN);
				htonl(rule->saddr4);
			} else {
				os_memcpy(rule->saddr6,
					  type4_params->src_ip.ipv6,
					  IPV6_LEN);
			}
			rule->valid_flags |= NFT_RULE_PARAM_SADDR;
			rule->weight++;
		}

		if (type4_params->classifier_mask & BIT(2)) {
			if (rule->ip_family == 4) {
				os_memcpy(&rule->daddr4,
					  type4_params->dst_ip.ipv4,
					  IPV4_LEN);
				htonl(rule->daddr4);
			} else {
				os_memcpy(rule->daddr6,
					  type4_params->dst_ip.ipv6,
					  IPV6_LEN);
			}
			rule->valid_flags |= NFT_RULE_PARAM_DADDR;
			rule->weight++;
		}

		if (type4_params->classifier_mask & BIT(3)) {
			rule->sport = type4_params->src_port;
			rule->valid_flags |= NFT_RULE_PARAM_SPORT;
			rule->weight++;
		}

		if (type4_params->classifier_mask & BIT(4)) {
			rule->dport = type4_params->dst_port;
			rule->valid_flags |= NFT_RULE_PARAM_DPORT;
			rule->weight++;
		}

		if (type4_params->classifier_mask & BIT(6)) {
			if (rule->ip_family == 4) {
				rule->proto = type4_params->protocol;
			} else {
				rule->proto = type4_params->next_header;
			}
			rule->valid_flags |= NFT_RULE_PARAM_PROTO;
			rule->weight++;
		}

		if (te->tclas_elem.type4_params.classifier_mask & BIT(5)) {
			rule->dscp = type4_params->dscp;
			rule->valid_flags |= NFT_RULE_PARAM_DSCP;
			rule->weight++;
		}

		wpa_printf(MSG_INFO, "qm_id: %d, classifier_type: 0x%x, "
			   "classifier_mask: 0x%x, weight: %u",
			   qm_id, te->classifier_type,
			   type4_params->classifier_mask, rule->weight);

	} else if (te->classifier_type == QM_TCLAS_CLASSIFIER_TYPE10) {

		rule->valid_flags |= NFT_RULE_PARAM_PROTO;
		rule->proto = type10_params->protocol_number;

		if (rule->proto == IPPROTO_UDP) {
			rule->valid_flags |= NFT_RULE_PARAM_DPORT;
			rule->dport = 4500;
		}

		if (type10_params->filter_len == 4) {
			rule->esp_spi =  ((type10_params->filter_value[0] &
					  type10_params->filter_mask[0]) << 24) | \
					  ((type10_params->filter_value[1] &
					  type10_params->filter_mask[1]) << 16) | \
					  ((type10_params->filter_value[2] &
					  type10_params->filter_mask[2]) << 8)  | \
					  ((type10_params->filter_value[3] &
					  type10_params->filter_mask[3]) << 0);
			rule->esp_spi = htonl(rule->esp_spi);
			rule->valid_flags |= NFT_RULE_PARAM_SPI;
		} else if (type10_params->filter_len == 12) {
			rule->esp_spi =  ((type10_params->filter_value[8]  &
					  type10_params->filter_mask[8]) << 24) | \
					  ((type10_params->filter_value[9] &
					  type10_params->filter_mask[9]) << 16) | \
					  ((type10_params->filter_value[10] &
					  type10_params->filter_mask[10]) << 8)  | \
					  ((type10_params->filter_value[11] &
					  type10_params->filter_mask[11]) << 0);
			rule->esp_spi = htonl(rule->esp_spi);
			rule->valid_flags |= NFT_RULE_PARAM_SPI;
		}

		rule->weight++;
	}

	/* This check handles the case when 2 x TCLAS 4 elements have
	 * overlapped parameters, which is an undesired case. So, the weight is
	 * capped to NFT_RULE_MAX_WEIGHT which is the maximum value.
	 */
	if (rule->weight > NFT_RULE_MAX_WEIGHT)
		rule->weight = NFT_RULE_MAX_WEIGHT;

	memcpy(rule->dmac, scs_sta_mac, ETH_ALEN);
	rule->valid_flags |= NFT_RULE_PARAM_DMAC;
	rule->mark = (qm_id << 8) | qm_tag;
	os_snprintf(rule->chain, sizeof(rule->chain), "%s_%s", CHAIN_NAME,
		    (sta && sta->ifname_wds) ? sta->ifname_wds : hapd->conf->iface);
	os_snprintf(rule->table, sizeof(rule->table), "%s", TABLE_NAME);
	rule->nf_family = NFPROTO_NETDEV;
}

static bool hostapd_mscs_flow_exists(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     struct hostapd_tclas_elements *new_te)
{
	struct hostapd_tclas_elements te = {0};
	int idx;

	if (!sta || !sta->mscs_ctxt || !sta->mscs_session_exists)
		return false;

	for (idx = 0; idx < sta->mscs_ctxt->available_idx; idx++) {
	     te = sta->mscs_ctxt->flow_info[idx];
	     if (!os_memcmp(new_te, &te, sizeof(struct hostapd_tclas_elements)))
		 return true;
	}

	return false;
}

static int hostapd_mscs_add_flow_info(struct sta_info *sta,
				      struct hostapd_tclas_elements *new_te)
{
	u8 available_idx;

	if (!sta)
		return -EINVAL;

	if (!sta->mscs_ctxt || !sta->mscs_session_exists)
		return -EINVAL;

	available_idx = sta->mscs_ctxt->available_idx;

	if (available_idx >= HOSTAPD_MSCS_MAX_FLOW_ENTRIES)
		return -EINVAL;

	os_memcpy(&sta->mscs_ctxt->flow_info[available_idx], new_te,
		  sizeof(struct hostapd_tclas_elements));
	sta->mscs_ctxt->available_idx++;

	return 0;
}

static int hostapd_nft_rule_get_cap_for_count(int count)
{
	int cap;

	if (count == 0)
		return 0;

	cap = 8;
	while (cap < count)
		cap <<= 1;
	return cap;
}

static void hostapd_qm_add_nft_rule_list(struct hostapd_nft_rule_params *rule,
					 struct hostapd_nft_rule_params **rules,
					 int *rule_count)
{
	struct hostapd_nft_rule_params *tmp;
	int cur_cap, new_cap;

	if (!rule || !rules || !rule_count) {
		wpa_printf(MSG_ERROR, "Invalid rule input to add rule");
		return;
	}

	if (*rule_count >= NFT_MAX_RULE_COUNT) {
		wpa_printf(MSG_ERROR, "QM: Rule count exceeded max limit %d",
			   NFT_MAX_RULE_COUNT);
		return;
	}

	cur_cap = hostapd_nft_rule_get_cap_for_count(*rule_count);
	new_cap = hostapd_nft_rule_get_cap_for_count(*rule_count + 1);

	if (new_cap > cur_cap) {
		tmp = os_realloc(*rules,
				 new_cap * sizeof(struct hostapd_nft_rule_params));
		if (!tmp) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to grow rules array to %d",
				   new_cap);
			return;
		}
		*rules = tmp;
	}

	(*rules)[*rule_count] = *rule;

	wpa_printf(MSG_INFO, "QM: Rule added at index %u with weight %u",
		   *rule_count, rule->weight);

	(*rule_count)++;
}


static int hostapd_scs_add_nft_rule(struct hostapd_data *hapd,
				    struct sta_info *sta,
				    struct hostapd_scs_req_desc_data *scs_req_desc,
				    struct hostapd_nft_rule_params **rules,
				    int *rule_count)
{
	struct hostapd_tclas_elements te;
	int i = 0;

	struct hostapd_nft_rule_params rule = {0};

	for (i = 0; i < scs_req_desc->num_tclas_elements; i++) {

		te = scs_req_desc->tclas[i];

		rule.scs_desc = scs_req_desc;
		rule.tclas_ele_idx = i;

		hostapd_qm_prepare_nft_rule(hapd, sta, &te, &rule,
					    hapd->conf->deferred_scs ?
					    scs_req_desc->qm_id :
					    scs_req_desc->scs_id,
					    HOSTAPD_QOS_SCS_TAG,
					    scs_req_desc->scs_sta_mac);

		if (scs_req_desc->tclas_processing != 0) {
			if ((rule.valid_flags & NFT_RULE_PARAM_DPORT ||
			    rule.valid_flags & NFT_RULE_PARAM_SPORT) &&
			    !(rule.valid_flags & NFT_RULE_PARAM_PROTO)) {

				rule.valid_flags |= NFT_RULE_PARAM_PROTO;

				rule.proto = IPPROTO_UDP;
				hostapd_qm_add_nft_rule_list(&rule, rules,
							     rule_count);
				rule.proto = IPPROTO_TCP;
			} else if (te.classifier_type == QM_TCLAS_CLASSIFIER_TYPE10) {
				rule.ip_family = 4;
				hostapd_qm_add_nft_rule_list(&rule, rules,
							     rule_count);
				rule.ip_family = 6;
			}

			hostapd_qm_add_nft_rule_list(&rule, rules, rule_count);

			os_memset(&rule, 0, sizeof(rule));
		}

		wpa_printf(MSG_INFO, "scs_id:%u rule valid flag : 0x%x ",
			   scs_req_desc->scs_id, rule.valid_flags);
	}

	if (scs_req_desc->tclas_processing == 0) {
		if ((rule.valid_flags & NFT_RULE_PARAM_DPORT ||
		    rule.valid_flags & NFT_RULE_PARAM_SPORT) &&
		    !(rule.valid_flags & NFT_RULE_PARAM_PROTO)) {

			rule.valid_flags |= NFT_RULE_PARAM_PROTO;

			rule.proto = IPPROTO_UDP;
			hostapd_qm_add_nft_rule_list(&rule, rules, rule_count);

			rule.proto = IPPROTO_TCP;
		} else if (!rule.ip_family) {
			rule.ip_family = 4;
			hostapd_qm_add_nft_rule_list(&rule, rules, rule_count);
			rule.ip_family = 6;
		}

		hostapd_qm_add_nft_rule_list(&rule, rules, rule_count);

		wpa_printf(MSG_INFO, "scs_id:%u rule valid flag : 0x%x ",
			   scs_req_desc->scs_id, rule.valid_flags);
	}
	return 0;
}

static int hostapd_scs_delete_nft_rule(struct hostapd_data *hapd,
				       struct sta_info *sta,
				       struct hostapd_scs_req_desc_data *scs_data)
{
	struct hostapd_tclas_elements *te;
	struct hostapd_nft_rule_params rule = {0};
	int i, j;

	if (!scs_data)
		return -EINVAL;


	for (i = 0; i < scs_data->num_tclas_elements; i++) {

		te = &scs_data->tclas[i];

		os_snprintf(rule.chain, sizeof(rule.chain), "%s_%s", CHAIN_NAME,
			    (sta && sta->ifname_wds) ? sta->ifname_wds : hapd->conf->iface);
		os_snprintf(rule.table, sizeof(rule.table), "%s", TABLE_NAME);
		rule.nf_family = NFPROTO_NETDEV;

		for (j = 0; j < te->num_rules; j++) {
			rule.handle = te->rule_handle[j];
			hostapd_config_nft_rule(&rule, false);
			te->rule_handle[j] = 0;
		}

		te->num_rules = 0;

		wpa_printf(MSG_DEBUG, "scs_id:%u all rules deleted", scs_data->scs_id);
	}
	return 0;
}

static int
hostapd_process_scs_add(struct hostapd_data *hapd, struct sta_info *sta,
			struct hostapd_scs_req_desc_data *scs_req_desc_tmp,
			u8 status)
{
	struct hostapd_scs_req_desc_data *scs_req_desc;
	u8 scs_id = scs_req_desc_tmp->scs_id;

	if (status != HOSTAPD_QM_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR, "SCS add request failed for scs_id:%u, "
			   "status:%u", scs_id, status);
		return -EINVAL;
	}

	if (sta->scs_session_count >= HOSTAPD_SCS_MAX_DESCRIPTORS_PER_PEER) {
		wpa_printf(MSG_ERROR, "SCS add request failed for scs_id:%u, "
			   "maximum sessions per peer reached", scs_id);
		return -EINVAL;
	}

	scs_req_desc = os_memdup(scs_req_desc_tmp, sizeof(*scs_req_desc_tmp));
	if (!scs_req_desc) {
		wpa_printf(MSG_ERROR, "scs_req mem alloc failure size %zu",
			   sizeof(*scs_req_desc));
		return -ENOMEM;
	}

	dl_list_init(&scs_req_desc->list);

	dl_list_add_tail(&sta->scs_req_desc, &scs_req_desc->list);
	sta->scs_session_count++;

	wpa_printf(MSG_DEBUG, "STA MAC: " MACSTR, MAC2STR(sta->addr));
	wpa_printf(MSG_DEBUG, "SCS add success - SCS ID: %u, Session count: %u",
		   scs_req_desc->scs_id, sta->scs_session_count);

	return 0;
}


static int
hostapd_process_scs_remove(struct hostapd_data *hapd, struct sta_info *sta,
			   struct hostapd_scs_req_desc_data *scs_req_desc_tmp,
			   u8 status)
{
	u8 scs_id = scs_req_desc_tmp->scs_id;
	struct hostapd_scs_req_desc_data *scs_req_desc;

	if (status != HOSTAPD_QM_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR, "SCS del request failed for scs_id:%u, "
			   "status:%u - Declined in driver", scs_id, status);
	}

	scs_req_desc = (hapd->conf->deferred_scs &&
			scs_req_desc_tmp->qm_id != 0xFFFF)
			? hostapd_scs_find_by_qmid(sta, scs_req_desc_tmp->qm_id)
			: hostapd_scs_find_by_id(sta, scs_id);
	if (!scs_req_desc) {
		wpa_printf(MSG_ERROR, "SCS del request failed for scs_id:%u, "
			   "entry not found", scs_id);
		return -EINVAL;
	}

	hostapd_scs_delete_nft_rule(hapd, sta, scs_req_desc);

	wpa_printf(MSG_DEBUG, "Freeing memory for SCS ID:%u", scs_id);
	dl_list_del(&scs_req_desc->list);
	os_free(scs_req_desc);
	sta->scs_session_count--;

	wpa_printf(MSG_DEBUG, "STA MAC: " MACSTR, MAC2STR(sta->addr));
	wpa_printf(MSG_DEBUG, "SCS del success - SCS ID: %u, Session count: %u",
		   scs_id, sta->scs_session_count);

	return 0;
}


static int
hostapd_process_scs_change(struct hostapd_data *hapd, struct sta_info *sta,
			   struct hostapd_scs_req_desc_data *scs_req_desc_tmp,
			   u8 status)
{
	u8 scs_id = scs_req_desc_tmp->scs_id;
	struct qm_req_desc_data qm_desc;

	if (status != HOSTAPD_QM_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR, "SCS change request failed for scs_id:%u,"
			   " status:%u - Old SCS data retained", scs_id,
			   status);
		return -EINVAL;
	}

	struct hostapd_scs_req_desc_data *scs_req_desc;
	struct dl_list saved_list;

	scs_req_desc = (hapd->conf->deferred_scs &&
			scs_req_desc_tmp->qm_id != 0xFFFF)
			? hostapd_scs_find_by_qmid(sta, scs_req_desc_tmp->qm_id)
			: hostapd_scs_find_by_id(sta, scs_id);
	if (!scs_req_desc) {
		wpa_printf(MSG_ERROR, "SCS change request failed for scs_id:%u,"
			   " entry not found", scs_id);
		return -EINVAL;
	}

	hostapd_scs_delete_nft_rule(hapd, sta, scs_req_desc);

	os_memset(&qm_desc, 0, sizeof(qm_desc));
	hostapd_copy_scs_desc(hapd, &qm_desc, *scs_req_desc);
	hostapd_drv_rule_config_notify(hapd, sta->addr, &qm_desc,
				       HOSTAPD_QM_TYPE_SCS);

	/* Save the list node, overwrite the descriptor content, restore
	 * the node so the entry stays linked in the list. */
	saved_list = scs_req_desc->list;
	os_memcpy(scs_req_desc, scs_req_desc_tmp, sizeof(*scs_req_desc_tmp));
	scs_req_desc->list = saved_list;

	wpa_printf(MSG_DEBUG, "STA MAC: " MACSTR, MAC2STR(sta->addr));
	wpa_printf(MSG_DEBUG,
		   "SCS change success - SCS ID: %u, Session count: %u",
		   scs_id, sta->scs_session_count);

	return 0;
}


void
hostapd_mscs_delete_nft_rules(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct hostapd_nft_rule_params rule;
	struct hostapd_tclas_elements *te;
	int idx, j;

	if (!sta->mscs_ctxt)
		return;

	for (idx = 0; idx < sta->mscs_ctxt->available_idx; idx++) {
		te = &sta->mscs_ctxt->flow_info[idx];
		os_memset(&rule, 0, sizeof(rule));
		os_snprintf(rule.chain, sizeof(rule.chain), "%s_%s",
			    CHAIN_NAME, hapd->conf->iface);
		os_snprintf(rule.table, sizeof(rule.table), "%s", TABLE_NAME);
		rule.nf_family = NFPROTO_NETDEV;

		for (j = 0; j < te->num_rules; j++) {
			rule.handle = te->rule_handle[j];
			hostapd_config_nft_rule(&rule, false);
			te->rule_handle[j] = 0;
		}
		te->num_rules = 0;
	}

	wpa_printf(MSG_DEBUG,
		   "MSCS: Deleted rules for %d flows for STA " MACSTR,
		   sta->mscs_ctxt->available_idx, MAC2STR(sta->addr));
}


static void
hostapd_mscs_add_nft_rules(struct hostapd_data *hapd, struct sta_info *sta,
				struct hostapd_nft_rule_params **rules,
				int *rule_count)
{
	struct hostapd_tclas_elements *te;
	struct hostapd_nft_rule_params rule;
	u8 tid;
	u8 idx;

	if (sta->scs_session_count == 0 || !sta->mscs_ctxt)
		return;

	/* Collect MSCS flow rules into the shared sorted list so they share a
	 * single delete-all/reinsert cycle with SCS rules.
	 */
	for (idx = 0; idx < sta->mscs_ctxt->available_idx; idx++) {
		te = &sta->mscs_ctxt->flow_info[idx];
		os_memset(&rule, 0, sizeof(rule));

		tid = te->up;
		rule.flow_idx = idx;
		rule.tclas_ele_idx = idx;

		hostapd_qm_prepare_nft_rule(hapd, sta, te, &rule,
					    tid, HOSTAPD_QOS_MSCS_TAG,
					    sta->addr);

		hostapd_qm_add_nft_rule_list(&rule, rules, rule_count);
	}
}

static void
hostapd_prepare_nft_rule_list(struct hostapd_data *hapd, struct sta_info *sta,
			      struct hostapd_nft_rule_params **rules,
			      int *rule_count)
{
	struct hostapd_scs_req_desc_data *desc;

	dl_list_for_each(desc, &sta->scs_req_desc,
			 struct hostapd_scs_req_desc_data, list) {
		/* SCS Uplink descriptors do not have TCLAS elements and do not
		 * need rule prepare for NF table programming.
		 */
		if (!desc->num_tclas_elements)
			continue;
		hostapd_scs_add_nft_rule(hapd, sta, desc, rules, rule_count);
	}

	hostapd_mscs_add_nft_rules(hapd, sta, rules, rule_count);
}

static int compare_rule_weight(const void *a, const void *b)
{
	const struct hostapd_nft_rule_params *rule_a =
			(const struct hostapd_nft_rule_params *)a;
	const struct hostapd_nft_rule_params *rule_b =
			(const struct hostapd_nft_rule_params *)b;
	int pri_a, pri_b;
	u8 tag_a, tag_b;

	tag_a = rule_a->mark & 0xff;
	tag_b = rule_b->mark & 0xff;

	/*
	 * Primary sort: MSCS rules before SCS rules.  nftables uses
	 * last-write-wins semantics, so SCS handles must be inserted last
	 * (i.e. have higher nftables handles) to take precedence.
	 */
	pri_a = (tag_a == HOSTAPD_QOS_SCS_TAG) ? 1 : 0;
	pri_b = (tag_b == HOSTAPD_QOS_SCS_TAG) ? 1 : 0;
	if (pri_a != pri_b)
		return pri_a - pri_b;

	/* Secondary sort: ascending weight within each tag group. */
	return (rule_a->weight > rule_b->weight) -
	       (rule_a->weight < rule_b->weight);
}


static void
hostapd_sort_nft_rule_list(struct hostapd_nft_rule_params *rules,
			   int rule_count)
{
	if (!rules || rule_count <= 0) {
		wpa_printf(MSG_INFO, "QM: No rules to sort");
		return;
	}

	qsort(rules, rule_count, sizeof(struct hostapd_nft_rule_params),
	      compare_rule_weight);

	wpa_printf(MSG_DEBUG, "QM: Sorted %u rules by weight", rule_count);
}

/*
 * hostapd_delete_all_qm_nft_rules - Delete all nftables rules for SCS and MSCS
 * @hapd: Pointer to hostapd data
 * @sta: Pointer to station info
 *
 * Deletes all existing nftables rules for every SCS and MSCS session of the
 * station.  SCS uplink descriptors without TCLAS elements are skipped.
 */
static void
hostapd_delete_all_qm_nft_rules(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct hostapd_scs_req_desc_data *desc;

	dl_list_for_each(desc, &sta->scs_req_desc,
			 struct hostapd_scs_req_desc_data, list) {
		/* SCS uplink descriptors carry no TCLAS elements and require
		 * no rule deletion.
		 */
		if (!desc->num_tclas_elements) {
			wpa_printf(MSG_DEBUG,
				   "QM: Skipping uplink descriptor SCS ID %u (no TCLAS)",
				   desc->scs_id);
			continue;
		}
		wpa_printf(MSG_DEBUG,
			   "QM: Deleting rules for SCS ID %u", desc->scs_id);
		hostapd_scs_delete_nft_rule(hapd, sta, desc);
	}

	hostapd_mscs_delete_nft_rules(hapd, sta);

	wpa_printf(MSG_DEBUG,
		   "QOS: Completed deletion of all nftables rules for STA " MACSTR,
		   MAC2STR(sta->addr));
}

/*
 * hostapd_configure_nft_rule_list - Configure nftables rules for SCS and MSCS
 * @hapd: Pointer to hostapd data
 * @sta: Pointer to station info
 * @rules: Array of prepared nftables rule parameters
 * @rule_count: Number of rules in @rules
 *
 * Deletes all existing SCS and MSCS nftables rules for the station, then
 * inserts the pre-sorted @rules in order and stores their handles for later
 * deletion.
 */
static void
hostapd_configure_nft_rule_list(struct hostapd_data *hapd,
				struct sta_info *sta,
				struct hostapd_nft_rule_params *rules,
				int rule_count)
{
	int idx;
	int ret;
	struct hostapd_nft_rule_params *rule;
	struct hostapd_scs_req_desc_data *scs_req_desc;
	struct hostapd_tclas_elements *te;

	wpa_printf(MSG_DEBUG,
		   "QM: Configuring %d nftables rules for STA " MACSTR,
		   rule_count, MAC2STR(sta->addr));

	/* Delete existing rules for all SCS and MSCS sessions */
	hostapd_delete_all_qm_nft_rules(hapd, sta);

	/* Create new rules and store their handles */
	for (idx = 0; idx < rule_count; idx++) {
		rule = &rules[idx];

		wpa_printf(MSG_DEBUG,
			   "QM: Creating rule %d/%d - %s=%u tclas_idx=%d weight=%u valid_flags=0x%x",
			   idx + 1, rule_count,
			   ((rule->mark & 0xff) == HOSTAPD_QOS_MSCS_TAG) ?
			   "flow_idx" : "scs_id",
			   ((rule->mark & 0xff) == HOSTAPD_QOS_MSCS_TAG) ?
			   (unsigned int)rule->flow_idx :
			   (rule->scs_desc ? (unsigned int)rule->scs_desc->scs_id : 0xff),
			   rule->tclas_ele_idx, rule->weight, rule->valid_flags);

		ret = hostapd_config_nft_rule(rule, true);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "QM: Rule creation failed table='%s', chain='%s'",
				   rule->table, rule->chain);
			continue;
		}

		if ((rule->mark & 0xff) == HOSTAPD_QOS_MSCS_TAG) {
			u8 flow_idx = rule->flow_idx;

			if (!sta->mscs_ctxt ||
			    flow_idx >= sta->mscs_ctxt->available_idx) {
				wpa_printf(MSG_ERROR,
					   "MSCS: Invalid flow_idx %u for rule %d",
					   flow_idx, idx);
				continue;
			}

			te = &sta->mscs_ctxt->flow_info[flow_idx];

			if (te->num_rules >= HOSTAPD_MAX_RULES_PER_TCLAS) {
				wpa_printf(MSG_ERROR,
					   "MSCS: Maximum rules per flow exceeded (%u) for rule %d",
					   te->num_rules, idx);
				continue;
			}

			te->rule_handle[te->num_rules] = rule->handle;
			te->num_rules++;

			wpa_printf(MSG_DEBUG,
				   "MSCS: Stored rule handle %llu at flow_idx=%u pos=%u",
				   (unsigned long long) rule->handle,
				   flow_idx, te->num_rules - 1);
			continue;
		}

		scs_req_desc = rule->scs_desc;
		if (!scs_req_desc) {
			wpa_printf(MSG_ERROR,
				   "QM: NULL scs_desc in rule %d", idx);
			continue;
		}


		if (rule->tclas_ele_idx < 0 ||
		    rule->tclas_ele_idx >= scs_req_desc->num_tclas_elements) {
			wpa_printf(MSG_ERROR,
				   "QM: Invalid tclas_ele_idx %d (valid range: 0-%d) for rule %d",
				   rule->tclas_ele_idx,
				   scs_req_desc->num_tclas_elements - 1, idx);
			continue;
		}

		te = &scs_req_desc->tclas[rule->tclas_ele_idx];

		if (te->num_rules >= HOSTAPD_MAX_RULES_PER_TCLAS) {
			wpa_printf(MSG_ERROR,
				   "QM: Maximum rules per TCLAS exceeded (%u) for rule %d (scs_id=%u, tclas_idx=%d)",
				   te->num_rules, idx,
				   scs_req_desc->scs_id,
				   rule->tclas_ele_idx);
			continue;
		}

		te->rule_handle[te->num_rules] = rule->handle;
		te->num_rules++;

		wpa_printf(MSG_DEBUG,
			   "QM: Stored rule handle %llu at position %u (tclas_idx=%d, SCS_ID=%u)",
			   (unsigned long long) rule->handle, te->num_rules - 1,
			   rule->tclas_ele_idx, scs_req_desc->scs_id);
	}

	wpa_printf(MSG_DEBUG,
		   "QM: Successfully configured %d nftables rules for STA " MACSTR,
		   rule_count, MAC2STR(sta->addr));
}

static void hostapd_process_nft_rules(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	struct hostapd_nft_rule_params *rules = NULL;
	int rule_count = 0;

	hostapd_prepare_nft_rule_list(hapd, sta, &rules, &rule_count);
	hostapd_sort_nft_rule_list(rules, rule_count);
	hostapd_configure_nft_rule_list(hapd, sta, rules, rule_count);

	os_free(rules);
}

static int hostapd_mscs_add_nft_rule(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     struct hostapd_tclas_elements *te)
{
	struct hostapd_nft_rule_params rule = {0};
	u8 flow_idx;
	u8 tid = te->up;

	if (!sta || !sta->mscs_ctxt || !sta->mscs_session_exists) {
		wpa_printf(MSG_ERROR, "MSCS: Missing context for STA");
		return -EINVAL;
	}

	if (hostapd_mscs_flow_exists(hapd, sta, te))
		return -EEXIST;

	if (hostapd_mscs_add_flow_info(sta, te)) {
		wpa_printf(MSG_ERROR, "MSCS: Rule could not be programmed");
		return -EINVAL;
	}

	if (sta->scs_session_count > 0) {
		/*
		 * SCS sessions are active - rebuild the full sorted rule set
		 * so MSCS and SCS rules get consistent nftables handles.
		 * Pipeline: prepare (SCS + MSCS) -> sort by tag/weight ->
		 * delete-all -> reinsert.
		 */
		hostapd_process_nft_rules(hapd, sta);
	} else {
		hostapd_qm_prepare_nft_rule(hapd, sta, te, &rule,
					    tid, HOSTAPD_QOS_MSCS_TAG,
					    sta->addr);
		hostapd_config_nft_rule(&rule, true);
		flow_idx = sta->mscs_ctxt->available_idx - 1;
		if (sta->mscs_ctxt->flow_info[flow_idx].num_rules == 0) {
			sta->mscs_ctxt->flow_info[flow_idx].rule_handle[0] =
				rule.handle;
			sta->mscs_ctxt->flow_info[flow_idx].num_rules++;
		} else {
			wpa_printf(MSG_ERROR,
				   "MSCS: Unexpected rule count %d at flow_idx %u",
				   sta->mscs_ctxt->flow_info[flow_idx].num_rules,
				   flow_idx);
		}
	}

	return 0;
}

static void hostapd_process_scs_req(struct hostapd_data *hapd,
				    struct sta_info *sta,
				    struct hostapd_scs_req_data *scs_req,
				    struct hostapd_scs_resp_data *scs_resp)
{
	struct hostapd_scs_resp_desc_data *scs_resp_desc;
	u8 scs_id_req, scs_id_resp, request_type, status;
	struct hostapd_scs_req_desc_data *scs_req_desc;
	struct qm_req_desc_data qm_desc;
	struct hostapd_scs_req_desc_data notify_descs[HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST];
	int notify_count = 0;
	int idx, idx1, scs_req_idx;
	bool nft_update = false;
	bool idx_found;
	int ret;

	wpa_printf(MSG_DEBUG, "Processing SCS descriptors (num: %d)",
		   scs_resp->num_scs_desc);

	for (idx = 0; idx < scs_resp->num_scs_desc; idx++) {
		scs_resp_desc = &scs_resp->scs_resp_desc[idx];
		scs_id_resp = scs_resp_desc->scs_id;
		status = scs_resp_desc->status;

		wpa_printf(MSG_DEBUG, "SCS Resp descriptor[%d]: SCS ID: %u, "
			   "Initial Status: %u", idx, scs_id_resp, status);

		/* Loop through SCS req desc to find matching SCS ID */
		idx_found = false;
		for (idx1 = 0; idx1 < scs_req->num_scs_desc; idx1++) {
			scs_id_req = scs_req->scs_req_desc[idx1].scs_id;
			if (scs_id_resp == scs_id_req) {
				scs_req_idx = idx1;
				idx_found = true;
				wpa_printf(MSG_DEBUG,
					   "Found matching request descriptor "
					   "Req desc[%d]: SCS ID: %u",
					   scs_req_idx, scs_id_req);
				break;
			}
		}

		if (idx_found != true) {
			scs_resp_desc->status = WLAN_STATUS_REQUEST_DECLINED;
			wpa_printf(MSG_ERROR, "No matching request descriptor "
				   "found for SCS ID: %u. Request declined.",
				   scs_id_resp);
			continue;
		}

		scs_req_desc = &scs_req->scs_req_desc[scs_req_idx];
		request_type = scs_req_desc->request_type;

		wpa_printf(MSG_DEBUG, "Processing Request: SCS ID: %u, Type: %u",
			   scs_id_resp, request_type);

		switch (request_type) {
		case QM_ADD_REQ:
			ret = hostapd_process_scs_add(hapd, sta, scs_req_desc,
						      status);
			if (!ret)
				scs_resp_desc->status = WLAN_STATUS_SUCCESS;
			else
				scs_resp_desc->status =
					WLAN_STATUS_REQUEST_DECLINED;

			nft_update = true;

			break;

		case QM_REMOVE_REQ:
			ret = hostapd_process_scs_remove(hapd, sta, scs_req_desc,
							 status);
			if (!ret)
				scs_resp_desc->status =
					WLAN_STATUS_TCLAS_PROCESSING_TERMINATED;
			else
				scs_resp_desc->status =
					WLAN_STATUS_REQUEST_DECLINED;

			break;

		case QM_CHANGE_REQ:
			ret = hostapd_process_scs_change(hapd, sta, scs_req_desc,
							 status);
			if (!ret)
				scs_resp_desc->status = WLAN_STATUS_SUCCESS;
			else
				scs_resp_desc->status =
					WLAN_STATUS_REQUEST_DECLINED;

			nft_update = true;

			break;

		default:
			wpa_printf(MSG_ERROR, "Invalid request type");
			scs_resp_desc->status = WLAN_STATUS_REQUEST_DECLINED;
		}

		notify_descs[notify_count++] = *scs_req_desc;
	}

	if (nft_update)
		hostapd_process_nft_rules(hapd, sta);

	for (idx = 0; idx < notify_count; idx++) {
		os_memset(&qm_desc, 0, sizeof(qm_desc));
		hostapd_copy_scs_desc(hapd, &qm_desc, notify_descs[idx]);
		hostapd_drv_rule_config_notify(hapd, sta->addr, &qm_desc,
					       HOSTAPD_QM_TYPE_SCS);
	}
}


static void hostapd_update_scs_resp_err(struct hostapd_scs_req_data *scs_req,
					struct hostapd_scs_resp_data *scs_resp)
{
	struct hostapd_scs_req_desc_data *scs_req_desc;
	struct hostapd_scs_resp_desc_data *scs_resp_desc;
	int idx;

	scs_resp->dialog_token = scs_req->dialog_token;
	scs_resp->num_scs_desc = scs_req->num_scs_desc;

	for (idx = 0; idx < scs_resp->num_scs_desc; idx++) {
		scs_req_desc = &scs_req->scs_req_desc[idx];
		scs_resp_desc = &scs_resp->scs_resp_desc[idx];

		scs_resp_desc->scs_id = scs_req_desc->scs_id;
		scs_resp_desc->status = WLAN_STATUS_REQUEST_DECLINED;
	}
}


static int hostapd_send_scs_response(struct hostapd_data *hapd, const u8 *da,
				     struct hostapd_scs_resp_data *scs_resp)
{
	u8 scs_id, dialog_token, num_scs_desc, status;
	size_t len, scs_resp_len;
	struct wpabuf *buf;
	int idx = 0;
	int ret = 0;

	dialog_token = scs_resp->dialog_token;
	num_scs_desc = scs_resp->num_scs_desc;

	wpa_printf(MSG_DEBUG, "Dialog token: %u, num_scs_desc: %u",
		   dialog_token, num_scs_desc);

	/* SCS ID (1), Status code (1) and padding (1) per descriptor */
	scs_resp_len = 3 * num_scs_desc;

	/* Header len and SCS response len */
	len = 16 + scs_resp_len;

	wpa_printf(MSG_DEBUG, "Allocating buffer of length: %zu", len);
	buf = wpabuf_alloc(len);
	if (!buf) {
		wpa_printf(MSG_ERROR,
			   "Failed to allocate buffer for SCS response frame");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_ROBUST_AV_STREAMING);
	wpabuf_put_u8(buf, ROBUST_AV_SCS_RESP);
	wpabuf_put_u8(buf, dialog_token);
	wpabuf_put_u8(buf, num_scs_desc);

	while ((num_scs_desc) &&
	       (idx < HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST)) {
		scs_id = scs_resp->scs_resp_desc[idx].scs_id;
		wpabuf_put_u8(buf, scs_id);
		status = scs_resp->scs_resp_desc[idx].status;
		wpabuf_put_u8(buf, status);
		wpabuf_put_u8(buf, 0x00);
		wpa_printf(MSG_DEBUG,
			   "SCS: Adding descriptor[%d], SCS ID: %u, Status: %u",
			   idx, scs_id, status);
		num_scs_desc--;
		idx++;
	}

	len = wpabuf_len(buf);

	wpa_printf(MSG_DEBUG,
		   "Sending SCS response frame of len:%zu to DA: " MACSTR, len,
		   MAC2STR(da));

	if (hostapd_drv_send_action(hapd, hapd->iface->freq, 0, da,
				    wpabuf_head(buf), len)) {
		wpa_printf(MSG_ERROR, "SCS response send action failed");
		ret = -1;
		goto error;
	}

	wpa_printf(MSG_INFO, "Successfully sent SCS response frame");

error:
	wpabuf_free(buf);
	return ret;
}

int hostapd_copy_and_send_mscs_data(struct hostapd_data *hapd,
				    struct sta_info *sta, u8 req_type,
				    const u8 dialog_token)
{
	struct qm_req_data qm_req = {0};
	struct qm_resp_data qm_resp = {0};
	struct qm_req_desc_data qm_desc_data = {0};

	if (!hapd->driver)
		return HOSTAPD_QM_STATUS_E_INVAL;

	os_memcpy(qm_req.peer_mac, sta->addr, ETH_ALEN);

	qm_req.qm_type = HOSTAPD_QM_TYPE_MSCS;
	qm_req.dialog_token = dialog_token;
	qm_req.num_qm_desc = 1;

	qm_desc_data.qm_id = HOSTAPD_QM_DEFAULT_QM_ID;
	qm_desc_data.request_type = req_type;
	qm_desc_data.user_priority_bitmap = sta->mscs_ctxt->user_priority_bitmap;
	qm_desc_data.user_priority_limit = sta->mscs_ctxt->user_priority_limit;
	qm_desc_data.tclas_mask = sta->mscs_ctxt->tclas_mask;

	/* Support added to parse one MSCS descriptor
	 * per MSCS request
	 */
	qm_req.qm_req_desc[0] = qm_desc_data;
	wpa_printf(MSG_DEBUG,
		   "MSCS: mscs info qm_type 0x%x dialog_token 0x%x num_desc 0x%x\n",
		   qm_req.qm_type,
		   qm_req.dialog_token,
		   qm_req.num_qm_desc);

	wpa_printf(MSG_DEBUG,
		   "MSCS: mscs info params req %x bmap 0x%x limit 0x%x mask 0x%x\n",
		   req_type, qm_desc_data.user_priority_bitmap,
		   qm_desc_data.user_priority_limit, qm_desc_data.tclas_mask);

	return hostapd_drv_set_qos(hapd, &qm_req, &qm_resp);
}

const u8 *hostapd_parse_mscs_desc(const u8 *payload,
		struct hostapd_mscs_desc *mscs)
{
	if (!payload || !mscs)
	    return NULL;

	mscs->req_type = *payload++;
	mscs->user_priority_control.user_priority_bitmap = *payload++;
	mscs->user_priority_control.user_priority_limit = *payload++;
	os_memcpy(&mscs->stream_timeout, payload, sizeof(uint32_t));
	payload += sizeof(uint32_t);
	mscs->tclas_mask_elem.id = *payload++;
	mscs->tclas_mask_elem.ie_len = *payload++;

	wpa_printf(MSG_DEBUG, "%s:MSCS descriptors parsed successfully\n", __func__);
	return payload;

}

static const u8 *hostapd_parse_tclas_mask(const u8 *payload,
		struct hostapd_mscs_desc *mscs)
{
	if (!payload || !mscs)
		return NULL;

	mscs->tclas_mask_elem.id_ext = *payload++;
	mscs->tclas_mask_elem.classifier_type = *payload++;
	mscs->tclas_mask_elem.classifier_mask = *payload++;

	return payload;

}

int hostapd_process_mscs_req(struct hostapd_data *hapd,
		struct sta_info *sta, const u8 *payload,
		struct hostapd_mscs_desc *mscs_desc, const u8 dialog_token)
{
	int ret = HOSTAPD_QM_STATUS_SUCCESS;
	int req_type = mscs_desc->req_type;

	if (req_type != QM_ADD_REQ && !sta->mscs_session_exists) {
		wpa_printf(MSG_ERROR, "MSCS: Session is inactive\n");
		goto decline;
	}

	if (req_type == QM_ADD_REQ && sta->mscs_session_exists) {
		wpa_printf(MSG_ERROR, "MSCS: Session already active\n");
		goto decline;
	}

	payload = hostapd_parse_tclas_mask(payload, mscs_desc);

	if (req_type != QM_REMOVE_REQ &&
		mscs_desc->tclas_mask_elem.id_ext != WLAN_EID_EXT_TCLAS_MASK) {
		wpa_printf(MSG_ERROR, "MSCS: TCLAS mask absent\n");
		goto decline;
	}

	/* Allocate MSCS context if request is ADD */
	if (req_type == QM_ADD_REQ) {
		sta->mscs_ctxt =
			(struct hostapd_mscs_ctxt *)os_zalloc(
					sizeof(struct hostapd_mscs_ctxt));
		if (!sta->mscs_ctxt)
		    goto decline;
	}

	switch (req_type) {
	case QM_ADD_REQ:
	case QM_CHANGE_REQ:
		sta->mscs_ctxt->user_priority_bitmap =
			mscs_desc->user_priority_control.user_priority_bitmap;
		sta->mscs_ctxt->user_priority_limit =
			mscs_desc->user_priority_control.user_priority_limit;
		sta->mscs_ctxt->tclas_mask =
			mscs_desc->tclas_mask_elem.classifier_mask;
		ret = hostapd_copy_and_send_mscs_data(hapd, sta, req_type,
			dialog_token);
		/**
		 * Set mscs session exists to true if
		 * driver returns SUCCESS.
		 * In case of CHANGE_REQ, the session
		 * would be present already, so it
		 * is okay to re-write it here.
		 * If drv returns an ADD failure, delete the
		 * context.
		 */
		if (ret == HOSTAPD_QM_STATUS_SUCCESS) {
			sta->mscs_session_exists = true;
			wpa_printf(MSG_INFO, "MSCS:Session created for "MACSTR,
				   MAC2STR(sta->addr));
		}
		else if (req_type == QM_ADD_REQ) {
			if (sta->mscs_ctxt) {
				os_free(sta->mscs_ctxt);
				sta->mscs_ctxt = NULL;
			}
			goto decline;
		}
		break;
	case QM_REMOVE_REQ:
		if (!sta->mscs_ctxt)
		    goto decline;
		ret = hostapd_copy_and_send_mscs_data(hapd, sta, req_type,
						      dialog_token);
		sta->mscs_session_exists = false;
		hostapd_mscs_delete_nft_rules(hapd, sta);
		os_free(sta->mscs_ctxt);
		sta->mscs_ctxt = NULL;
		wpa_printf(MSG_INFO, "MSCS:Session deleted for "MACSTR,
			   MAC2STR(sta->addr));
		ret = WLAN_STATUS_TCLAS_PROCESSING_TERMINATED;
		break;
	default:
		goto decline;
	}
	return ret;

decline:
	wpa_printf(MSG_ERROR, "MSCS: Decline Request %d", req_type);
	return HOSTAPD_QM_STATUS_DECLINED;
}

u8 *hostapd_add_mscs_desc(struct hostapd_data *hapd, u8 *eid,
			  struct sta_info *sta)
{
	u8 *pos = eid;
	size_t len;
	struct hostapd_mscs_desc mscs_desc = {0};

	if (!hapd->conf->mscs || !sta || !sta->mscs_session_exists ||
	    !sta->mscs_ctxt)
		return pos;
	/**
	 * Add MSCS descriptor containing these items:
	 * ELEMID_EXT_MSCS_DESCRIPTOR
	 * Request type
	 * User priority control
	 * Stream timeout
	 * Assoc response status
	 */
	len = 4 + sizeof(struct hostapd_user_priority_control)
	      + sizeof(u32) + sizeof(u16);
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = len;
	*pos++ = WLAN_EID_EXT_MSCS_DESCRIPTOR;

	*pos++ = QM_ADD_REQ;
	os_memcpy(pos, &mscs_desc.user_priority_control,
		  sizeof(struct hostapd_user_priority_control));

	pos += sizeof(struct hostapd_user_priority_control);

	os_memcpy(pos, &mscs_desc.stream_timeout,
		  sizeof(u32));
	pos += sizeof(u32);

	/**
	 * Add MSCS Subelement IE - id = 0
	 * Length = 2
	 * Status code of MSCS handshake
	 */
	*pos++ = HOSTAPD_MSCS_WLAN_EID_SUBELEMENT;
	*pos++ = sizeof(u16);
	os_memcpy(pos, &sta->mscs_ctxt->assoc_req_status, sizeof(u16));
	pos += sizeof(u16);

	wpa_printf(MSG_INFO, "Added MSCS descriptor len %d",
		   (int)len);
	return pos;
}

int hostapd_send_mscs_response(struct hostapd_data *hapd,
			       struct sta_info *sta, const u8 *da,
			       u8 dialog_token, int status_code)
{
	struct wpabuf *buf;
	size_t len;

	/** exact needed size:
	 *  Action frm header (3 bytes) +
	 *  Status code (2 bytes) +
	 *  MSCS descriptor
	 **/
	len = 5 + sizeof(struct hostapd_mscs_desc);
	buf = wpabuf_alloc(len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "Failed to allocate buffer for MSCS response");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_ROBUST_AV_STREAMING);
	wpabuf_put_u8(buf, ROBUST_AV_MSCS_RESP);
	wpabuf_put_u8(buf, dialog_token);
	wpabuf_put_u8(buf, LOW_BYTE(status_code));
	wpabuf_put_u8(buf, HIGH_BYTE(status_code));

	len = wpabuf_len(buf);
	if (hostapd_drv_send_action(hapd, hapd->iface->freq, 0, da,
				    wpabuf_head(buf), len)) {
		wpa_printf(MSG_ERROR, "MSCS response send action failed");
		wpabuf_free(buf);
		return -1;
	}

	wpa_printf(MSG_INFO, "Successfully sent MSCS response frame len %d\n",
		   (int)len);
	wpabuf_free(buf);
	return 0;
}

static int hostapd_handle_mscs_req(struct hostapd_data *hapd,
		const u8 *buf, size_t frame_length)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	struct hostapd_mscs_desc mscs = {0};
	struct sta_info *sta;
	const u8 *payload, *payload_start;
	int ret = 0;
	u8 dialog_token, elem_id, elem_id_ext, length;

	if (!hapd->conf->mscs) {
		wpa_printf(MSG_ERROR, "MSCS feature not enabled");
		return -1;
	}

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_printf(MSG_ERROR, "%s: STA not found", __func__);
		return -1;
	}

	dialog_token = mgmt->u.action.u.robust_av_req.dialog_token;
	payload_start = mgmt->u.action.u.robust_av_req.variable;

	wpa_hexdump(MSG_DEBUG, "MSCS Request", payload_start, frame_length);
	wpa_printf(MSG_DEBUG, "frame_len:%zu", frame_length);
	/**
	 * Check if elem id has Extension tag.
	 * If it does not, then it means MSCS
	 * Descriptor IE might be missing
	 */
	elem_id = *payload_start++;

	if (elem_id != WLAN_EID_EXTENSION)
		return -1;

	length = *payload_start++;

	if (length < IEEE80211_MSCS_DESC_MIN_LEN)
		return -1;

	elem_id_ext = *payload_start++;
	if (elem_id_ext != WLAN_EID_EXT_MSCS_DESCRIPTOR) {
		wpa_printf(MSG_ERROR,
			   "MSCS:mscs elem %d is not available in this frame\n",
			   WLAN_EID_EXT_MSCS_DESCRIPTOR);
		return -1;
	}

	payload = hostapd_parse_mscs_desc(payload_start, &mscs);
	if (!payload)
		return -1;

	ret = hostapd_process_mscs_req(hapd, sta, payload, &mscs, dialog_token);

	hostapd_drv_rule_config_notify(hapd, sta->addr, NULL,
				       HOSTAPD_QM_TYPE_MSCS);

	return hostapd_send_mscs_response(hapd, sta, mgmt->sa, dialog_token, ret);
}


int hostapd_handle_mscs_ie_assoc(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 const u8 *buf, u8 len)
{
	struct hostapd_mscs_desc mscs = {0};
	const u8 *payload_start = buf;
	const u8 *payload;
	size_t frame_length = len;
	int ret = 0;

	if (!sta) {
		wpa_printf(MSG_ERROR, "%s: STA not found", __func__);
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "MSCS Request", payload_start, frame_length);
	wpa_printf(MSG_DEBUG, "frame_len:%zu", frame_length);

	payload = hostapd_parse_mscs_desc(payload_start, &mscs);
	if (!payload)
		return -1;

	ret = hostapd_process_mscs_req(hapd, sta, payload, &mscs, 0);

	/**
	 * Fill the MSCS descriptor in response, only when an
	 * MSCS context is present
	 */
	if (sta->mscs_ctxt)
		sta->mscs_ctxt->assoc_req_status = ret;
	return 0;

}

/*
 * hostapd_scs_get_sta - MLD-aware STA lookup.
 * If out_hapd is non-NULL it is updated to the link hapd that owns the STA.
 */
static struct sta_info *
hostapd_scs_get_sta(struct hostapd_data *hapd,
		    struct hostapd_data **out_hapd,
		    const u8 *addr)
{
	struct hostapd_data *temp_hapd = hapd;
	struct sta_info *sta = NULL;

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#else
	if (hapd->conf->mld_ap) {
		for_each_mld_link(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#endif

	if (out_hapd)
		*out_hapd = temp_hapd;
	return sta;
}


/*
 * hostapd_scs_configure_send - populate scs_req from desc and send to driver.
 * Calls hostapd_copy_and_send_scs_data then hostapd_process_scs_req.
 * Returns 0 on success, -1 on driver failure.
 */
static int
hostapd_scs_configure_send(struct hostapd_data *hapd,
			   struct sta_info *sta,
			   const u8 *peer_mac,
			   struct hostapd_scs_req_desc_data *desc)
{
	struct hostapd_scs_resp_data scs_resp = {0};
	struct hostapd_scs_req_data scs_req = {0};
	int ret;

	os_memcpy(scs_req.peer_mac, peer_mac, ETH_ALEN);
	scs_req.num_scs_desc = 1;
	scs_req.scs_req_desc[0] = *desc;

	ret = hostapd_copy_and_send_scs_data(hapd, &scs_req, &scs_resp);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE: set_qos failed, ret:%d", ret);
		return -1;
	}

	hostapd_process_scs_req(hapd, sta, &scs_req, &scs_resp);

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		WPA_EVENT_SCS_STATUS_NOTIFY
		"sta_mac=" MACSTR " bssid=" MACSTR
		" qm_id=%u status=%u",
		MAC2STR(peer_mac), MAC2STR(hapd->own_addr),
		desc->qm_id, scs_resp.scs_resp_desc[0].status);

	return 0;
}


/*
 * hostapd_scs_configure_qmid_found - handle SCS_CONFIGURE when the incoming
 * qm_id already exists in the STA's descriptor database.
 *
 * *desc points to the stored entry that matched the qm_id.
 *
 * ADD    - reject (duplicate qm_id)
 * UPDATE - reprogram driver + NFT, clear request_pending
 * DELETE - reprogram driver + NFT; if client-initiated and terminating
 *          node, send unsolicited SCS REMOVE to the STA
 */
static int
hostapd_scs_configure_qmid_found(struct hostapd_data *hapd,
				 struct sta_info *sta, const u8 *peer_mac,
				 struct hostapd_scs_req_desc_data *scs_desc,
				 struct hostapd_scs_req_desc_data *desc)
{
	int ret;

	switch (scs_desc->request_type) {

	case QM_ADD_REQ:
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE ADD: qm_id=%u already exists",
			   desc->qm_id);
		return -1;

	case QM_CHANGE_REQ:
		wpa_printf(MSG_DEBUG,
			   "SCS_CONFIGURE UPDATE: qm_id=%u found, updating",
			   desc->qm_id);
		ret = hostapd_scs_configure_send(hapd, sta, peer_mac, scs_desc);
		if (!ret) {
			if (desc->request_pending)
				desc->request_pending = 0;
		}
		//Todo: To review all the request_pending clearing areas to handle zombie states
		//Todo: To think of timer logic for proper clearance and notification to EM
		return ret;

	case QM_REMOVE_REQ: {
		struct hostapd_scs_resp_data scs_resp = {0};
		struct hostapd_scs_req_data  scs_req  = {0};
		u8 client_initiated = desc->client_initiated_scs;

		wpa_printf(MSG_DEBUG,
			   "SCS_CONFIGURE DELETE: qm_id=%u found, deleting",
			   desc->qm_id);

		os_memcpy(scs_req.peer_mac, peer_mac, ETH_ALEN);
		scs_req.num_scs_desc = 1;
		scs_req.scs_req_desc[0] = *scs_desc;

		ret = hostapd_copy_and_send_scs_data(hapd, &scs_req, &scs_resp);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "SCS_CONFIGURE DELETE: set_qos failed, ret:%d",
				   ret);
			return -1;
		}

		hostapd_process_scs_req(hapd, sta, &scs_req, &scs_resp);

		/*
		 * If this was a client-initiated session on the terminating
		 * node (peer_mac == scs_sta_mac), send the 802.11 SCS Response
		 * frame directly to the STA. scs_resp already has
		 * WLAN_STATUS_TCLAS_PROCESSING_TERMINATED set by
		 * process_scs_req.
		 */
		if (client_initiated &&
		    os_memcmp(peer_mac, scs_desc->scs_sta_mac, ETH_ALEN) == 0)
			hostapd_send_scs_response(hapd, sta->addr, &scs_resp);

		wpa_msg(hapd->msg_ctx, MSG_INFO,
			WPA_EVENT_SCS_STATUS_NOTIFY
			"sta_mac=" MACSTR " bssid=" MACSTR
			" qm_id=%u status=%u",
			MAC2STR(peer_mac), MAC2STR(hapd->own_addr),
			desc->qm_id, scs_resp.scs_resp_desc[0].status);

		return 0;
	}

	default:
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE: unknown request_type=%u",
			   scs_desc->request_type);
		return -1;
	}
}


/*
 * hostapd_scs_configure_qmid_not_found - handle SCS_CONFIGURE when the
 * incoming qm_id is not yet in the STA's descriptor database.
 *
 * ADD    - controller-initiated ADD; reject if a client-initiated entry
 *          already exists for this scs_id (agent should use UPDATE)
 * UPDATE - deferred client-initiated path: a pending client entry must
 *          exist; assign qm_id and send as ADD to driver (first time)
 * DELETE - reject (nothing to remove)
 */
static int
hostapd_scs_configure_qmid_not_found(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     const u8 *peer_mac,
				     struct hostapd_scs_req_desc_data *scs_desc)
{
	struct hostapd_scs_req_desc_data *desc;
	int ret;

	switch (scs_desc->request_type) {

	case QM_ADD_REQ:
		wpa_printf(MSG_DEBUG,
			   "SCS_CONFIGURE ADD: controller-initiated, "
			   "scs_id=%u qm_id=%u",
			   scs_desc->scs_id, scs_desc->qm_id);

		return hostapd_scs_configure_send(hapd, sta, peer_mac, scs_desc);

	case QM_CHANGE_REQ:
		/*
		 * Deferred path: Controller assigns qm_id for the first time
		 * to a pending client-initiated descriptor. The driver has
		 * never seen this session so we send ADD (not CHANGE).
		 */
		desc = hostapd_scs_find_client_initiated(sta, scs_desc->scs_id);
		if (! (desc && desc->request_pending)) {
			wpa_printf(MSG_ERROR,
				   "SCS_CONFIGURE UPDATE: no pending "
				   "client-initiated entry for scs_id=%u",
				   scs_desc->scs_id);
			return -1;
		}
		wpa_printf(MSG_DEBUG,
			   "SCS_CONFIGURE UPDATE (deferred): assigning "
			   "qm_id=%u to scs_id=%u",
			   scs_desc->qm_id, scs_desc->scs_id);

		/*
		 * Step 1: Send ADD to driver - first time this session
		 * is programmed. Use the incoming scs_desc which carries
		 * the full TCLAS/QoS content from the controller.
		 */
		struct hostapd_scs_resp_data scs_resp = {0};
		struct hostapd_scs_req_data  scs_req  = {0};

		scs_desc->request_type      = QM_ADD_REQ;
		os_memcpy(scs_req.peer_mac, peer_mac, ETH_ALEN);
		scs_req.num_scs_desc        = 1;
		scs_req.scs_req_desc[0]     = *scs_desc;

		ret = hostapd_copy_and_send_scs_data(hapd, &scs_req, &scs_resp);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "SCS_CONFIGURE UPDATE (deferred): "
				   "set_qos failed, ret:%d", ret);
			return -1;
		}

		/*
		 * Step 2: Update stored entry in-place via process_scs_req with
		 * QM_CHANGE_REQ - routes to hostapd_process_scs_change which
		 * does os_memcpy over the existing list entry and rebuilds NFT.
		 * No duplicate is added to the list.
		 */
		scs_desc->request_type      = QM_CHANGE_REQ;
		hostapd_process_scs_req(hapd, sta, &scs_req, &scs_resp);

		desc->request_pending = 0;

		wpa_msg(hapd->msg_ctx, MSG_INFO,
			WPA_EVENT_SCS_STATUS_NOTIFY
			"sta_mac=" MACSTR " bssid=" MACSTR
			" qm_id=%u status=%u",
			MAC2STR(peer_mac), MAC2STR(hapd->own_addr),
			scs_desc->qm_id, scs_resp.scs_resp_desc[0].status);

		return 0;

	case QM_REMOVE_REQ:
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE DELETE: qm_id=%u not found",
			   scs_desc->qm_id);
		return -1;

	default:
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: unknown request_type=%u",
			   scs_desc->request_type);
		return -1;
	}
}


int hostapd_scs_configure(struct hostapd_data *hapd, const u8 *peer_mac,
			  const u8 *scs_sta_mac, u16 qm_id, const u8 *desc_buf,
			  u8 desc_len, bool dedicated_queue)
{
	struct hostapd_scs_req_desc_data scs_desc = {0};
	struct hostapd_scs_req_desc_data *desc;
	struct hostapd_data *temp_hapd = hapd;
	struct sta_info *sta = NULL;
	u8 elem_id, elem_len;
	int ret;
	const u8 *drv_peer_mac;

	if (!hapd->conf->scs || !hapd->conf->deferred_scs) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: SCS feature disabled or Deferred SCS not configured");
		return -1;
	}

	sta = hostapd_scs_get_sta(hapd, &temp_hapd, peer_mac);
	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE: peer " MACSTR " not found",
			   MAC2STR(peer_mac));
		return -1;
	}

	/*
	 * The kernel sta_info_get_bss() lookup is keyed on sta->sta.addr
	 * which is the MLD MAC for MLD STAs.  Translate peer_mac to the
	 * MLD MAC so the NL set_qos call succeeds regardless of whether
	 * the operator passed a link MAC or the MLD MAC on the CLI.
	 */
#ifdef CONFIG_IEEE80211BE
	if (sta->mld_info.mld_sta)
		drv_peer_mac = sta->mld_info.common_info.mld_addr;
	else
#endif
		drv_peer_mac = peer_mac;

	if (desc_len < 2) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: Invalid SCS frame, lower len");
		return -1;
	}

	elem_id = *desc_buf++;

	if (elem_id != WLAN_EID_SCS_DESCRIPTOR) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: unexpected EID %u", elem_id);
		return -1;
	}

	elem_len = *desc_buf++;

	/* Parse the SCS descriptor */
	ret = hostapd_parse_scs_desc(desc_buf, sta, &scs_desc, elem_len,
				     true, false, qm_id);
	if (ret != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE: descriptor parse failed, ret:%d",
			   ret);
		return -1;
	}

	scs_desc.qm_id = qm_id;
	scs_desc.dedicated_queue = dedicated_queue;
	os_memcpy(scs_desc.scs_sta_mac, scs_sta_mac, ETH_ALEN);

	desc_len -= (elem_len + 2);
	if (desc_len != 0) {
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE: Todo: Desc len to be checked later, "
			   "desc_len:%u, elem_len:%u", desc_len, elem_len);
	}

	wpa_printf(MSG_DEBUG,
		   "SCS_CONFIGURE: peer=" MACSTR " sta=" MACSTR
		   " qm_id=%u scs_id=%u req_type=%u",
		   MAC2STR(peer_mac), MAC2STR(scs_sta_mac),
		   qm_id, scs_desc.scs_id, scs_desc.request_type);

	desc = hostapd_scs_find_by_qmid(sta, qm_id);
	if (desc)
		return hostapd_scs_configure_qmid_found(temp_hapd, sta,
							drv_peer_mac, &scs_desc,
							desc);

	return hostapd_scs_configure_qmid_not_found(temp_hapd, sta, drv_peer_mac,
						    &scs_desc);
}

/*
 * hostapd_handle_scs_req_deferred - process a parsed SCS request frame
 * when deferred_scs is enabled.
 *
 * Stores each descriptor in the STA database, sets request_pending and
 * client_initiated_scs flags, fires a CTRL-EVENT-SCS-REQUEST-NOTIFY event
 * per descriptor, and sends the SCS Response frame to the station.
 * The driver is NOT programmed; the application is expected to call
 * SCS_CONFIGURE once it has assigned QM IDs.
 *
 * Returns 0 on success, negative on send failure.
 */
static int
hostapd_handle_scs_req_deferred(struct hostapd_data *hapd,
				const struct ieee80211_mgmt *mgmt,
				struct hostapd_data *assoc_hapd,
				struct sta_info *assoc_sta,
				struct hostapd_scs_req_data *scs_req,
				struct hostapd_scs_resp_data *scs_resp,
				const struct hostapd_scs_raw_desc *raw_desc)
{
	u8 index;
	int ret;

	scs_resp->dialog_token = scs_req->dialog_token;
	scs_resp->num_scs_desc = scs_req->num_scs_desc;

	for (index = 0; index < scs_req->num_scs_desc; index++) {
		struct hostapd_scs_req_desc_data *desc =
			&scs_req->scs_req_desc[index];
		struct hostapd_scs_req_desc_data *stored;
		char *hex;

		scs_resp->scs_resp_desc[index].scs_id = desc->scs_id;

		/*
		 * If a client-initiated request is already pending for
		 * this scs_id, decline the new request regardless of
		 * type — stacking requests adds complexity and the
		 * pending one must be resolved first.
		 */
		stored = hostapd_scs_find_client_initiated(assoc_sta,
							   desc->scs_id);
		if (stored && stored->request_pending) {
			wpa_printf(MSG_ERROR,
				   "Deferred SCS: request already pending "
				   "for SCS ID:%u, declining",
				   desc->scs_id);
			scs_resp->scs_resp_desc[index].status =
				WLAN_STATUS_REQUEST_DECLINED;
			continue;
		}

		if (desc->request_type == QM_ADD_REQ) {
			ret = hostapd_process_scs_add(assoc_hapd, assoc_sta,
						      desc,
						      HOSTAPD_QM_STATUS_SUCCESS);
			if (ret) {
				wpa_printf(MSG_ERROR,
					   "Deferred SCS: store failed for SCS ID:%u",
					   desc->scs_id);
				scs_resp->scs_resp_desc[index].status =
					WLAN_STATUS_REQUEST_DECLINED;
				continue;
			}

			/* Set flags on the newly stored entry.*/
			stored = dl_list_last(&assoc_sta->scs_req_desc,
					      struct hostapd_scs_req_desc_data,
					      list);
			if (stored) {
				stored->request_pending = 1;
				stored->client_initiated_scs = 1;
			}

			scs_resp->scs_resp_desc[index].status = WLAN_STATUS_SUCCESS;
		} else if (desc->request_type == QM_REMOVE_REQ ||
			   desc->request_type == QM_CHANGE_REQ) {
			if (!stored) {
				wpa_printf(MSG_ERROR,
					   "Deferred SCS: no client-initiated "
					   "descriptor found for SCS ID:%u",
					   desc->scs_id);
				scs_resp->scs_resp_desc[index].status =
					WLAN_STATUS_REQUEST_DECLINED;
				continue;
			}

			stored->request_pending = 1;
			scs_resp->scs_resp_desc[index].status =
				(desc->request_type == QM_REMOVE_REQ) ?
				WLAN_STATUS_TCLAS_PROCESSING_TERMINATED :
				WLAN_STATUS_SUCCESS;
		} else {
			wpa_printf(MSG_ERROR,
				   "Deferred SCS: unknown request type %u for SCS ID:%u",
				   desc->request_type, desc->scs_id);
			scs_resp->scs_resp_desc[index].status =
				WLAN_STATUS_REQUEST_DECLINED;
			continue;
		}

		/* Notify application once per descriptor */
		hex = os_malloc(raw_desc[index].len * 2 + 1);
		if (!hex) {
			wpa_printf(MSG_ERROR,
				   "Deferred SCS: hex alloc failed for SCS ID:%u",
				   desc->scs_id);
			continue;
		}
		wpa_snprintf_hex(hex, raw_desc[index].len * 2 + 1,
				 raw_desc[index].data,
				 raw_desc[index].len);
		wpa_msg(hapd->msg_ctx, MSG_INFO,
			WPA_EVENT_SCS_REQUEST_NOTIFY
			"sta_mac=" MACSTR " bssid=" MACSTR
			" scs_desc=%s",
			MAC2STR(mgmt->sa),
			MAC2STR(hapd->own_addr),
			hex);
		os_free(hex);
	}

	ret = hostapd_send_scs_response(hapd, mgmt->sa, scs_resp);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "Deferred SCS response frame send failed, ret:%d",
			   ret);
	return ret;
}


static int hostapd_handle_scs_req(struct hostapd_data *hapd, const u8 *buf,
				  size_t frame_length)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	struct hostapd_scs_resp_data scs_resp = {0};
	struct hostapd_scs_req_data scs_req = {0};
	const u8 *payload, *payload_start;
	struct sta_info *assoc_sta = NULL;
	struct hostapd_data *assoc_hapd;
	struct sta_info *sta = NULL;
	struct hostapd_scs_raw_desc
		raw_desc[HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST];
	u8 elem_id, elem_len;
	u8 index = 0;
	int ret = 0;
	u8 scs_id;

	wpa_printf(MSG_INFO, "Received SCS request frame from:" MACSTR,
		   MAC2STR(mgmt->sa));

	if (!hapd->conf->scs) {
		wpa_printf(MSG_ERROR, "SCS feature not enabled");
		return -1;
	}

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_printf(MSG_ERROR, "STA not found in scs_req handler");
		return -1;
	}

#ifdef CONFIG_IEEE80211BE
	if (!sta->mld_info.mld_sta) {
		wpa_printf(MSG_DEBUG,
			   "Assign sta to assoc_sta for Non-MLD STA");
		assoc_sta = sta;
	}
#endif

	assoc_hapd = hapd;

	if (!assoc_sta) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_DEBUG,
				   "Assoc STA not found in scs_req handler");
			return -1;
		}
	}

	os_memcpy(scs_req.peer_mac, mgmt->sa, ETH_ALEN);

	scs_req.dialog_token = mgmt->u.action.u.robust_av_req.dialog_token;
	payload_start = mgmt->u.action.u.robust_av_req.variable;

	wpa_hexdump(MSG_MSGDUMP, "SCS Request", payload_start, frame_length);
	wpa_printf(MSG_MSGDUMP, "Frame length:%zu, Dialog_token:%u",
		   frame_length, scs_req.dialog_token);
	wpa_printf(MSG_DEBUG, "Active SCS session count:%u",
		   assoc_sta->scs_session_count);

	while (frame_length > 0) {
		payload = payload_start;
		elem_id = *payload++;

		if (elem_id != WLAN_EID_SCS_DESCRIPTOR)
			break;

		if (index >= HOSTAPD_SCS_MAX_DESCPRIPTORS_PER_REQUEST) {
			wpa_printf(MSG_ERROR, "SCS Request: Max descriptors "
				   "per request exceeded");
			break;
		}

		elem_len = *payload++;
		scs_id = *payload;

		ret = hostapd_parse_scs_desc(payload, assoc_sta,
					     &scs_req.scs_req_desc[index],
					     elem_len,
					     hapd->conf->deferred_scs,
					     true, 0xFFFF);
		if (ret != HOSTAPD_QM_STATUS_SUCCESS) {
			wpa_printf(MSG_ERROR, "Parsing failure: SCS ID:%u, "
				   "Index:%u, status:%d", scs_id, index, ret);
			return ret;
		}

		scs_req.scs_req_desc[index].client_initiated_scs = 1;

		/* capture raw element bytes (EID + len + payload) */
		raw_desc[index].data = payload_start;
		raw_desc[index].len  = elem_len + 2;

		payload_start += (elem_len + 2);
		frame_length -= (elem_len + 2);

		index++;
	}

	scs_req.num_scs_desc = index;

	if (hapd->conf->deferred_scs)
		return hostapd_handle_scs_req_deferred(hapd, mgmt,
						       assoc_hapd, assoc_sta,
						       &scs_req, &scs_resp,
						       raw_desc);


	ret = hostapd_copy_and_send_scs_data(assoc_hapd, &scs_req, &scs_resp);
	if (ret) {
		wpa_printf(MSG_ERROR, "Send SCS data failed, ret:%d", ret);
		hostapd_update_scs_resp_err(&scs_req, &scs_resp);
		goto send_error_resp;
	}

	hostapd_process_scs_req(assoc_hapd, assoc_sta, &scs_req, &scs_resp);

send_error_resp:
	ret = hostapd_send_scs_response(hapd, mgmt->sa, &scs_resp);
	if (ret)
		wpa_printf(MSG_ERROR, "SCS response frame send failed, ret:%d",
			   ret);

	return ret;
}


int hostapd_send_unsolicited_scs_resp(struct hostapd_data *hapd,
				      struct sta_info *sta, u8 scs_id,
				      u8 req_type)
{
	struct hostapd_scs_resp_data scs_resp = {0};
	struct hostapd_scs_req_data scs_req = {0};
	struct sta_info *assoc_sta = NULL;
	struct hostapd_data *assoc_hapd;
	int ret;
	u8 addr[6];

	wpa_printf(MSG_INFO, "Received SCS unsolicited Resp cmd from:" MACSTR,
		   MAC2STR(sta->addr));

	if (req_type != QM_REMOVE_REQ) {
		wpa_printf(MSG_ERROR, "Send unsolicited response supported "
			   "only for Remove request");
		return -1;
	}

	if (!hostapd_scs_find_by_id(sta, scs_id)) {
		wpa_printf(MSG_ERROR, "SCS resp cmd failed for scs_id:%u, "
			   "Not active", scs_id);
		return -1;
	}

	os_memcpy(addr, sta->addr, ETH_ALEN);

#ifdef CONFIG_IEEE80211BE
	if (!sta->mld_info.mld_sta) {
		wpa_printf(MSG_DEBUG,
			   "Assign sta to assoc_sta for Non-MLD STA");
		assoc_sta = sta;
	} else {
		os_memcpy(addr, sta->mld_info.common_info.mld_addr, ETH_ALEN);
	}
#endif

	assoc_hapd = hapd;

	if (!assoc_sta) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_DEBUG,
				   "Assoc STA not found in scs resp send");
			return -1;
		}
	}

	if (!assoc_sta->scs_session_count) {
		wpa_printf(MSG_ERROR, "No SCS sessions configured");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SCS Unsolicited: Active SCS session count:%u",
		   assoc_sta->scs_session_count);

	os_memcpy(scs_req.peer_mac, addr, ETH_ALEN);
	scs_req.num_scs_desc = 1;
	scs_req.scs_req_desc[0].scs_id = scs_id;
	scs_req.scs_req_desc[0].request_type = req_type;

	wpa_printf(MSG_DEBUG, "Send SCS response - SCS ID:%u, Request type:%u",
		   scs_id, req_type);

	ret = hostapd_copy_and_send_scs_data(assoc_hapd, &scs_req, &scs_resp);
	if (ret) {
		wpa_printf(MSG_ERROR, "Send SCS data failed, ret:%d", ret);
		hostapd_update_scs_resp_err(&scs_req, &scs_resp);
		goto send_error_resp;
	}

	hostapd_process_scs_req(assoc_hapd, assoc_sta, &scs_req, &scs_resp);

send_error_resp:
	ret = hostapd_send_scs_response(hapd, addr, &scs_resp);
	if (ret)
		wpa_printf(MSG_ERROR, "SCS response frame send failed, ret:%d",
			   ret);

	return ret;
}


u8 hostapd_mscs_get_tid(struct hostapd_data *hapd, struct sta_info *sta, u8 tid)
{
	u8 up_bitmap, up_limit;

	if (!sta || !sta->mscs_ctxt)
		return 0;

	up_bitmap = sta->mscs_ctxt->user_priority_bitmap;
	up_limit = sta->mscs_ctxt->user_priority_limit;
	if (BIT(tid) & up_bitmap) {
		if (up_limit < tid)
			return up_limit;
		else
			return tid;
	}

	return 0;
}

void hostapd_process_mscs_flow(struct hostapd_data *hapd,
			       struct hostapd_tclas_elements *te,
			       u8 *addr, u8 tid)
{
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "MSCS: Qos Flow received for " MACSTR " from driver"
		   , MAC2STR(addr));

	sta = ap_get_sta(hapd, addr);

	if (!sta || !sta->mscs_ctxt || !sta->mscs_session_exists)
		goto fail;

	if (!hapd->conf->mscs)
		goto fail;

	if (!te)
		goto fail;

	tid = hostapd_mscs_get_tid(hapd, sta, tid);
	te->classifier_type = QM_TCLAS_CLASSIFIER_TYPE4;
	te->tclas_elem.type4_params.classifier_mask = sta->mscs_ctxt->tclas_mask;
	te->up = tid;

	hostapd_mscs_add_nft_rule(hapd, sta, te);

fail:
	os_free(addr);
	return;
}

void
hostapd_handle_robust_av(struct hostapd_data *hapd, const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;

	if (len < IEEE80211_HDRLEN + 3) {
		wpa_printf(MSG_ERROR, "Robust AV frame length error - len %zu",
			   len);
		return;
	}

	wpa_printf(MSG_INFO, "Received Robust AV streaming action frame: %d",
		   mgmt->u.action.u.robust_av_req.action);

	switch (mgmt->u.action.u.robust_av_req.action) {
	case ROBUST_AV_SCS_REQ:
		if (hostapd_handle_scs_req(hapd, buf, len))
			wpa_printf(MSG_ERROR, "SCS Request handling failed");
		break;
	case ROBUST_AV_MSCS_REQ:
		if (hostapd_handle_mscs_req(hapd, buf, len))
			wpa_printf(MSG_ERROR, "MSCS Request handling failed");
		break;
	default:
		break;
	}
}
