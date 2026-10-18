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

#include "scs_api.h"
#include <qca_scs_api_if.h>

static int verbose;

struct callback {
	uint32_t cmd_id;
	void (*cb)(void *cbd, uint8_t *buf, size_t buf_len, char *ifname);
	void *cbd;
};

struct nl_ctxt {
	wifi_cfg80211_context cfg80211_ctxt;
	struct callback scs_cb;
};

struct sal_ctxt {
	struct sal_rule_socket *rs;
};

static void scs_api_nl_cb(void *cbd, uint8_t *buf, size_t buf_len,
			  char *ifname);

static struct sal_ctxt g_sal_ctxt;

static struct nl_ctxt g_nl_ctxt = {
	.scs_cb = {
		QCA_NL80211_VENDOR_SUBCMD_SCS_API_MSG, scs_api_nl_cb,
		&g_sal_ctxt
	},
};

struct nla_policy
qca_wlan_vendor_scs_api_app_policy[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_PEER_MAC] = {.type = NLA_BINARY,
					.maxlen = SCS_API_MAC_ADDR_SIZE },
	[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MLD_MAC] = {.type = NLA_BINARY,
					.maxlen = SCS_API_MAC_ADDR_SIZE },
	[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DIALOG_TOKEN] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TOTAL_SCS_IDX] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DATA] = {.type = NLA_BINARY },
};

static int
process_nl_msg_send_scs_api_app_reg(const char *ifname,
				    struct scs_api_info *scs_api_app_info)
{
	uint32_t ret = 0;
	struct nl_msg *nlmsg = NULL;
	struct nlattr *nl_ven_data = NULL;
	int retval = 0;

	PRINT_IF_VERB("Processing to send NL message for SCS App registration");

	nlmsg = wifi_cfg80211_prepare_command(
			&g_nl_ctxt.cfg80211_ctxt,
			QCA_NL80211_VENDOR_SUBCMD_SCS_API_MSG, ifname);

	if (nlmsg) {
		nl_ven_data = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_ven_data) {
			PRINT_IF_VERB("Failed to get vendor data");
			goto error_cleanup;
		}

		if (nla_put_u8(
			nlmsg, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE,
			QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE_REGISTRATION)) {
			PRINT_IF_VERB("nla_put fail for msg_type");
			goto error_cleanup;
		}

		if (nla_put(
			nlmsg,
			QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DATA,
			sizeof(struct scs_api_info), scs_api_app_info)) {
			PRINT_IF_VERB("nla_put fail for SCS Registration data");
			goto error_cleanup;
		}

		end_vendor_data(nlmsg, nl_ven_data);

		PRINT_IF_VERB("Send NL message - APP Registration");
		ret = send_nlmsg(&g_nl_ctxt.cfg80211_ctxt, nlmsg, NULL);
		if (ret < 0) {
			PRINT_IF_VERB("Couldn't send NL command, ret = %d",
				      ret);
			goto error_cleanup;
		}
	} else {
		PRINT_IF_VERB("Unable to prepare NL command!");
		retval = -SCS_API_STATUS_E_INVAL;
	}

	return retval;

error_cleanup:
	PRINT_IF_VERB("Failed to prepare message");
	nlmsg_free(nlmsg);
	retval = -1;
	return retval;
}

static int process_nl_msg_send_scs_api_data(char *ifname,
					    uint8_t *link_mac_addr,
					    uint8_t *mld_mac_addr,
					    bool is_mlo,
					    uint8_t *parsed_scs_data,
					    uint8_t num_scs_desc,
					    uint8_t dialog_token)
{
	uint32_t ret = 0;
	struct nl_msg *nlmsg = NULL;
	struct nlattr *nl_ven_data = NULL;
	uint16_t size;
	int retval = 0;

	size = num_scs_desc * sizeof(struct scs_api_data);
	PRINT_IF_VERB("Total SCS descriptors:%d", num_scs_desc);
	PRINT_IF_VERB("Processing to send NL message with Parsed SCS data");
	PRINT_IF_VERB("Size of SCS data:%d", size);

	if (size > SCS_API_DATA_BUF_MAX_SIZE) {
		PRINT_IF_VERB("Err: Parsed SCS data size > max buf size");
		return -SCS_API_STATUS_E_INVAL;
	}

	nlmsg = wifi_cfg80211_prepare_command(
			&g_nl_ctxt.cfg80211_ctxt,
			QCA_NL80211_VENDOR_SUBCMD_SCS_API_MSG, ifname);

	if (nlmsg) {
		nl_ven_data = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_ven_data) {
			PRINT_IF_VERB("Failed to get vendor data");
			goto error_cleanup;
		}

		if (nla_put_u8(
			nlmsg, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE,
			QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE_SCS_REQUEST_PARSED_DATA)) {
			PRINT_IF_VERB("nla_put fail for msg_type");
			goto error_cleanup;
		}

		if (nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_PEER_MAC,
			    SCS_API_MAC_ADDR_SIZE, link_mac_addr)) {
			PRINT_IF_VERB("nla_put fail for link mac");
			goto error_cleanup;
		}

		if (is_mlo) {
			if (nla_put(nlmsg,
				    QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MLD_MAC,
				SCS_API_MAC_ADDR_SIZE, mld_mac_addr)) {
				PRINT_IF_VERB("nla_put fail for MLD mac");
				goto error_cleanup;
			}
		}

		if (nla_put_u8(nlmsg,
			       QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DIALOG_TOKEN,
			       dialog_token)) {
			PRINT_IF_VERB("nla_put fail for dialog token");
			goto error_cleanup;
		}

		if (nla_put(nlmsg,
			    QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DATA,
			    size, parsed_scs_data)) {
			PRINT_IF_VERB("nla_put fail for Parsed SCS data");
			goto error_cleanup;
		}

		end_vendor_data(nlmsg, nl_ven_data);

		PRINT_IF_VERB("Send NL message - Parsed SCS Data");
		ret = send_nlmsg(&g_nl_ctxt.cfg80211_ctxt, nlmsg, NULL);
		if (ret < 0) {
			PRINT_IF_VERB("Couldn't send NL command, ret = %d",
				      ret);
			goto error_cleanup;
		}
	} else {
		PRINT_IF_VERB("Unable to prepare NL command!");
		retval = -SCS_API_STATUS_E_INVAL;
	}

	return retval;

error_cleanup:
	PRINT_IF_VERB("Failed to prepare message");
	nlmsg_free(nlmsg);
	retval = -SCS_API_STATUS_E_INVAL;
	return retval;
}

static int
process_nl_msg_send_scs_api_response_data(char *ifname,
					  struct scs_api_drv_resp *app_resp,
					  uint8_t *link_mac_addr,
					  uint8_t *mld_mac_addr,
					  bool is_mlo,
					  uint8_t num_scs_desc,
					  uint8_t dialog_token)
{
	uint32_t ret = 0;
	struct nl_msg *nlmsg = NULL;
	struct nlattr *nl_ven_data = NULL;
	uint16_t size;
	int retval = 0;

	size = num_scs_desc * sizeof(struct scs_api_drv_resp);
	PRINT_IF_VERB("Processing to send NL message with SCS Response data");
	PRINT_IF_VERB("Size of SCS response data:%d", size);

	if (size > SCS_API_RESP_BUF_MAX_SIZE) {
		PRINT_IF_VERB("Err: App response size > Max buf size");
		return -SCS_API_STATUS_E_INVAL;
	}

	nlmsg = wifi_cfg80211_prepare_command(
			&g_nl_ctxt.cfg80211_ctxt,
			QCA_NL80211_VENDOR_SUBCMD_SCS_API_MSG, ifname);

	if (nlmsg) {
		nl_ven_data = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_ven_data) {
			PRINT_IF_VERB("Failed to get vendor data");
			goto error_cleanup;
		}

		if (nla_put_u8(
			nlmsg, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE,
			QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE_SCS_RESPONSE)) {
			PRINT_IF_VERB("nla_put fail for msg_type");
			goto error_cleanup;
		}

		if (nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_PEER_MAC,
			    SCS_API_MAC_ADDR_SIZE, link_mac_addr)) {
			PRINT_IF_VERB("nla_put fail for peer mac");
			goto error_cleanup;
		}

		if (is_mlo) {
			if (nla_put(nlmsg,
				    QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MLD_MAC,
				    SCS_API_MAC_ADDR_SIZE, mld_mac_addr)) {
				PRINT_IF_VERB("nla_put fail for MLD mac");
				goto error_cleanup;
			}
		}

		if (nla_put_u8(nlmsg,
			       QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DIALOG_TOKEN,
			       dialog_token)) {
			PRINT_IF_VERB("nla_put fail for dialog token");
			goto error_cleanup;
		}

		if (nla_put_u8(nlmsg,
			       QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TOTAL_SCS_IDX,
			       num_scs_desc)) {
			PRINT_IF_VERB("nla_put fail for SCS Index");
			goto error_cleanup;
		}

		if (nla_put(nlmsg,
			    QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DATA, size,
			    app_resp)) {
			PRINT_IF_VERB("nla_put fail for SCS Response data");
			goto error_cleanup;
		}

		end_vendor_data(nlmsg, nl_ven_data);

		PRINT_IF_VERB("Send NL message - SCS application response");
		ret = send_nlmsg(&g_nl_ctxt.cfg80211_ctxt, nlmsg, NULL);
		if (ret < 0) {
			PRINT_IF_VERB("Couldn't send NL command, ret = %d",
				      ret);
			goto error_cleanup;
		}
	} else {
		PRINT_IF_VERB("Unable to prepare NL command!");
		retval = -SCS_API_STATUS_E_INVAL;
	}

	return retval;

error_cleanup:
	PRINT_IF_VERB("Failed to prepare message");
	nlmsg_free(nlmsg);
	retval = -SCS_API_STATUS_E_INVAL;
	return retval;
}

static
uint16_t parse_scs_tclas_elements(struct scs_api_data *scs_data, uint8_t **frm,
				  uint8_t *frm_length)
{
	uint8_t tclas_len, filter_len;
	uint8_t tclas_idx = 0;
	uint8_t elem_id, version, type;
	struct scs_api_tclas_tuple *tclas_tuple = NULL;
	union scs_api_tclas *tclas = NULL;
	uint16_t retval = SCS_API_STATUS_SUCCESS;

	PRINT_IF_VERB("*** TCLAS Parse ***");

	while (*frm_length > 0) {
		elem_id = **frm;
		/* Process only TCLAS element(s) and TCLAS Processing element
		 * in this loop
		 */
		if (elem_id != SCS_API_ELEMID_TCLAS &&
		    elem_id != SCS_API_ELEMID_TCLAS_PROCESS)
			break;

		*frm += 1;
		if (elem_id == SCS_API_ELEMID_TCLAS) {
			/* Add elem_id, length to tclas_len */
			tclas_len = *(*frm)++;
			tclas_tuple = &scs_data->scs_api_tclas_tuple[tclas_idx];
			tclas = &tclas_tuple->scs_api_tclas;

			tclas_tuple->len = tclas_len;
			tclas_len += 2;
			tclas_tuple->up = *(*frm)++;
			type = *(*frm)++;
			tclas_tuple->type = type;

			if (type == SCS_API_WNM_TCLAS_CLASSIFIER_TYPE4) {
				tclas->type4.mask = *(*frm)++;
				version = *(*frm)++;
				tclas->type4.version = version;

				if (version ==
					SCS_API_WNM_TCLAS_CLAS4_VERSION_4) {
					memcpy(tclas->type4.src_ip.ipv4, *frm,
					       SCS_API_IPV4_LEN);
					*frm += SCS_API_IPV4_LEN;
					memcpy(tclas->type4.dst_ip.ipv4, *frm,
					       SCS_API_IPV4_LEN);
					*frm += SCS_API_IPV4_LEN;

				} else if (version ==
					   SCS_API_WNM_TCLAS_CLAS4_VERSION_6) {
					memcpy(tclas->type4.src_ip.ipv6, *frm,
					       SCS_API_IPV6_LEN);
					*frm += SCS_API_IPV6_LEN;
					memcpy(tclas->type4.dst_ip.ipv6, *frm,
					       SCS_API_IPV6_LEN);
					*frm += SCS_API_IPV6_LEN;
				}

				tclas->type4.src_port =
						be16toh(**(u_int16_t **)frm);
				*frm += 2;
				tclas->type4.dst_port =
						be16toh(**(u_int16_t **)frm);
				*frm += 2;
				tclas->type4.dscp = *(*frm)++;
				tclas->type4.next_header = *(*frm)++;
				if (version ==
					SCS_API_WNM_TCLAS_CLAS4_VERSION_4) {
					tclas->type4.reserved = *(*frm)++;
				} else if (version ==
					   SCS_API_WNM_TCLAS_CLAS4_VERSION_6) {
					memcpy(tclas->type4.flow_label, *frm,
					       SCS_API_FLOW_LABEL_SIZE);
					*frm += SCS_API_FLOW_LABEL_SIZE;
				}

			} else if (type ==
				   SCS_API_WNM_TCLAS_CLASSIFIER_TYPE10) {
				tclas->type10.protocol_instance = *(*frm)++;
				tclas->type10.protocol_number = *(*frm)++;

				filter_len = (tclas_len - 6) / 2;
				if (filter_len < 0) {
					PRINT_IF_VERB("TCLAS Filter len < 0");
					retval = SCS_API_STATUS_E_INVAL;
					goto fail;
				}

				tclas->type10.filter_len = filter_len;
				memcpy(tclas->type10.filter_val, *frm,
				       filter_len);
				*frm += filter_len;
				memcpy(tclas->type10.filter_mask, *frm,
				       filter_len);
				*frm += filter_len;
			} else {
				PRINT_IF_VERB("Unknown tclas classifier: %d",
					      type);
				retval = SCS_API_STATUS_E_INVAL;
				goto fail;
			}

			tclas_idx++;
			scs_data->tclas_elements = tclas_idx;
			*frm_length -= tclas_len;

		} else if (elem_id == SCS_API_ELEMID_TCLAS_PROCESS) {
			scs_data->scs_api_tclas_processing.elem_id = elem_id;
			scs_data->scs_api_tclas_processing.length = *(*frm)++;
			scs_data->scs_api_tclas_processing.tclas_process =
								      *(*frm)++;
			*frm_length -= 3;
		}
	}

fail:
	return retval;
}

static
uint16_t parse_scs_qos_attributes(struct scs_api_data *scs_data, uint8_t *frm,
				  uint8_t *frm_length)
{
	uint16_t bitmap = 0;
	uint32_t temp = 0;
	uint32_t control_info = 0;
	uint8_t elemid, elemid_extension, length, temp_length;
	uint16_t retval = SCS_API_STATUS_SUCCESS;

	scs_data->is_qos_present = false;

	if (*frm_length >= SCS_API_MIN_QOS_ATTR_LEN) {
		elemid = *frm++;
		if (elemid == SCS_API_ELEMID_EXTN) {
			length = *frm++;
			temp_length = length;
			elemid_extension = *frm++;

			if (elemid_extension != SCS_API_ELEMID_EXT_QOS_ATTR) {
				PRINT_IF_VERB("Invalid element ID extension");
				retval = SCS_API_STATUS_E_INVAL;
				goto fail;
			}

			length -= 1; /* Subtract length of elemid_extension */

			scs_data->is_qos_present = true;
			/* Parse QoS attributes */
			control_info = le32toh(*((uint32_t *)frm));
			scs_data->qos_attr.direction =
				SCS_API_GET_FIELD_FROM_QOS_CTRL_INFO(
						control_info, DIRECTION);
			if (scs_data->qos_attr.direction ==
				SCS_API_QOS_ATTR_DIRECTION_DIRECT_LINK) {
				PRINT_IF_VERB(
				  "SCS for direct link is not supported by AP");
				retval = SCS_API_STATUS_E_NOSUPPORT;
				goto fail;
			}

			scs_data->qos_attr.tid =
				SCS_API_GET_FIELD_FROM_QOS_CTRL_INFO(
						control_info, TID);
			scs_data->qos_attr.user_priority =
				SCS_API_GET_FIELD_FROM_QOS_CTRL_INFO(
						control_info, USER_PRIORITY);
			scs_data->qos_attr.bitmap =
				SCS_API_GET_FIELD_FROM_QOS_CTRL_INFO(
						control_info, BITMAP);
			scs_data->qos_attr.link_id =
				SCS_API_GET_FIELD_FROM_QOS_CTRL_INFO(
						control_info, LINK_ID);
			frm += SCS_API_QOS_ATTR_CTRL_INFO_LEN;
			length -= SCS_API_QOS_ATTR_CTRL_INFO_LEN;

			PRINT_IF_VERB("*** QoS Characteristics Element ***");
			PRINT_IF_VERB("Control Info Params:");
			PRINT_IF_VERB("Direction: %u, TID: %u, UP: %u ",
				      scs_data->qos_attr.direction,
				      scs_data->qos_attr.tid,
				      scs_data->qos_attr.user_priority);
			PRINT_IF_VERB("Bitmap: %u, Link_ID: %u",
				      scs_data->qos_attr.bitmap,
				      scs_data->qos_attr.link_id);

			scs_data->qos_attr.min_service_interval =
						le32toh(*((uint32_t *)frm));
			frm += SCS_API_QOS_ATTR_SERVICE_INTERVAL_LEN;
			length -= SCS_API_QOS_ATTR_SERVICE_INTERVAL_LEN;

			scs_data->qos_attr.max_service_interval =
						le32toh(*((uint32_t *)frm));
			frm += SCS_API_QOS_ATTR_SERVICE_INTERVAL_LEN;
			length -= SCS_API_QOS_ATTR_SERVICE_INTERVAL_LEN;

			temp = 0;
			memcpy(&temp, frm, SCS_API_QOS_ATTR_MIN_DATA_RATE_LEN);
			scs_data->qos_attr.min_data_rate = le32toh(temp);
			frm += SCS_API_QOS_ATTR_MIN_DATA_RATE_LEN;
			length -= SCS_API_QOS_ATTR_MIN_DATA_RATE_LEN;

			temp = 0;
			memcpy(&temp, frm, SCS_API_QOS_ATTR_DELAY_BOUND_LEN);
			scs_data->qos_attr.delay_bound = le32toh(temp);
			frm += SCS_API_QOS_ATTR_DELAY_BOUND_LEN;
			length -= SCS_API_QOS_ATTR_DELAY_BOUND_LEN;

			bitmap = scs_data->qos_attr.bitmap;
			if ((length > 0) &&
			    SCS_API_CHECK_QOS_ATTR_PRESENT(bitmap,
							   MAX_MSDU_SIZE)) {
				scs_data->qos_attr.max_msdu_size =
						le16toh(*((uint16_t *)frm));
				frm += SCS_API_QOS_ATTR_MAX_MSDU_SIZE_LEN;
				length -= SCS_API_QOS_ATTR_MAX_MSDU_SIZE_LEN;
			}

			if ((length > 0) &&
			    SCS_API_CHECK_QOS_ATTR_PRESENT(
						bitmap,	SERVICE_START_TIME)) {
				scs_data->qos_attr.service_start_time =
						le32toh(*((uint32_t *)frm));
				frm += SCS_API_QOS_ATTR_SERVICE_START_TIME_LEN;
				length -=
					SCS_API_QOS_ATTR_SERVICE_START_TIME_LEN;
			}

			if ((length > 0) &&
			    SCS_API_CHECK_QOS_ATTR_PRESENT(
					bitmap,	SERVICE_START_TIME_LINK_ID)) {
				scs_data->qos_attr.service_start_time_link_id =
									*frm;
				frm +=
				SCS_API_QOS_ATTR_SERVICE_START_TIME_LINK_ID_LEN;
				length -=
				SCS_API_QOS_ATTR_SERVICE_START_TIME_LINK_ID_LEN;
			}

			if ((length > 0) &&
			    SCS_API_CHECK_QOS_ATTR_PRESENT(bitmap,
							   MEAN_DATA_RATE)) {
				temp = 0;
				memcpy(&temp, frm,
				       SCS_API_QOS_ATTR_MEAN_DATA_RATE_LEN);
				scs_data->qos_attr.mean_data_rate =
								le32toh(temp);
				/* Data rate in Kilo Bytes Per Second */
				frm += SCS_API_QOS_ATTR_MEAN_DATA_RATE_LEN;
				length -= SCS_API_QOS_ATTR_MEAN_DATA_RATE_LEN;
			}

			if ((length > 0) && SCS_API_CHECK_QOS_ATTR_PRESENT(
							bitmap, BURST_SIZE)) {
				scs_data->qos_attr.burst_size =
						le32toh(*((uint32_t *)frm));
				frm += SCS_API_QOS_ATTR_BURST_SIZE_LEN;
				length -= SCS_API_QOS_ATTR_BURST_SIZE_LEN;
			}

			if ((length > 0) && SCS_API_CHECK_QOS_ATTR_PRESENT(
						bitmap, MSDU_LIFETIME)) {
				scs_data->qos_attr.msdu_lifetime =
						le16toh((*(uint16_t *)frm));
				frm += SCS_API_QOS_ATTR_MSDU_LIFETIME_LEN;
				length -= SCS_API_QOS_ATTR_MSDU_LIFETIME_LEN;
			}

			if ((length > 0) && SCS_API_CHECK_QOS_ATTR_PRESENT(
						bitmap, MSDU_DELIVERY_RATIO)) {
				scs_data->qos_attr.msdu_delivery_ratio = *frm;
				frm += SCS_API_QOS_ATTR_MSDU_DELIVERY_RATIO_LEN;
				length -=
				    SCS_API_QOS_ATTR_MSDU_DELIVERY_RATIO_LEN;
			}

			if ((length > 0) && SCS_API_CHECK_QOS_ATTR_PRESENT(
						bitmap, MSDU_COUNT_EXPONENT)) {
				scs_data->qos_attr.msdu_count_exponent = *frm;
				frm += SCS_API_QOS_ATTR_MSDU_COUNT_EXPONENT_LEN;
				length -=
				    SCS_API_QOS_ATTR_MSDU_COUNT_EXPONENT_LEN;
			}

			if ((length > 0) && SCS_API_CHECK_QOS_ATTR_PRESENT(
						bitmap, MEDIUM_TIME)) {
				scs_data->qos_attr.medium_time =
						le16toh((*(uint16_t *)frm));
				frm += SCS_API_QOS_ATTR_MEDIUM_TIME_LEN;
				length -= SCS_API_QOS_ATTR_MEDIUM_TIME_LEN;
			}

			PRINT_IF_VERB("Min_Service_Interval: %u",
				      scs_data->qos_attr.min_service_interval);
			PRINT_IF_VERB("Max_Service_Interval: %u",
				      scs_data->qos_attr.max_service_interval);
			PRINT_IF_VERB("Min_Data_Rate: %u,Delay_Bound: %u",
				      scs_data->qos_attr.min_data_rate,
				      scs_data->qos_attr.delay_bound);
			PRINT_IF_VERB("Max_MSDU_Size: %u",
				      scs_data->qos_attr.max_msdu_size);
			PRINT_IF_VERB("Service_Start_Time: %u",
				      scs_data->qos_attr.service_start_time);
			PRINT_IF_VERB(
				"Service_Start_Time_Link_ID: %u",
				scs_data->qos_attr.service_start_time_link_id);
			PRINT_IF_VERB("Mean_Data_Rate: %u, Burst_Size: %u",
				      scs_data->qos_attr.mean_data_rate,
				      scs_data->qos_attr.burst_size);
			PRINT_IF_VERB("MSDU_Lifetime: %u",
				      scs_data->qos_attr.msdu_lifetime);
			PRINT_IF_VERB("MSDU_Delivery_Ratio: %u",
				      scs_data->qos_attr.msdu_delivery_ratio);
			PRINT_IF_VERB("MSDU_Count_Exponent: %u",
				      scs_data->qos_attr.msdu_count_exponent);
			PRINT_IF_VERB("Medium_Time: %u",
				      scs_data->qos_attr.medium_time);

			if (length == 0)
				PRINT_IF_VERB(
				      "SCS QOS attributes processing complete");

			/* Subtracting length of (Elem ID and length field)
			 *  - 2 bytes and QOS attr length from the frm_length
			 * input.
			 */
			*frm_length = *frm_length - 2 - temp_length;
		}
	}

fail:
	return retval;
}

static
uint16_t scs_api_parse_scs_req(char *ifname, uint8_t *frm, uint8_t len,
			       struct scs_api_data *scs_temp)
{
	uint8_t scsid = 0;
	uint8_t request_type;
	uint16_t retval = SCS_API_SCS_REQUEST_DECLINED;

	frm = SCS_API_SCS_ID_TRAVERSE(frm);
	scsid = *frm;
	request_type = SCS_API_SCS_GET_REQ(frm);

	PRINT_IF_VERB("SCS request - Descriptor wise dump - len:%d", len);
	SCS_API_PKT_HEXDUMP(frm, len);

	len -= 2;

	scs_temp->scsid = scsid;
	scs_temp->request_type = request_type;

	if (request_type == SCS_API_SCS_REMOVE_RULE)
		return SCS_API_SCS_SUCCESS;

	frm = SCS_API_SCS_INTRA_ACCESS_CATEGORY_ELEM_TRAVERSE(frm);

	/* Check for Intra Access Category priority element */
	if (SCS_API_SCS_GET_INTRA_ACCESS_CATEGORY_PRI_ELEM(frm) ==
					SCS_API_ELEMID_INT_ACC_CAT_PRIORITY) {
		scs_temp->access_priority =
			SCS_API_SCS_GET_INTRA_ACCESS_CATEGORY_PRI(frm);
		len -=
		     sizeof(struct scs_api_intra_access_priority_category_elem);
		frm +=
		     sizeof(struct scs_api_intra_access_priority_category_elem);
		PRINT_IF_VERB("Intra Access Category Priority element Present");
		PRINT_IF_VERB("Priority:%d", scs_temp->access_priority);
		PRINT_IF_VERB("SCS request - Frame at TCLAS position. Len:%d",
			      len);
		SCS_API_PKT_HEXDUMP(frm, len);
	}

	/* Check if TCLAS or QOS attr element is present or not */
	if ((*frm != SCS_API_ELEMID_TCLAS) && (*frm != SCS_API_ELEMID_EXTN)) {
		/* TCLAS Mask element is not present */
		PRINT_IF_VERB("AP is declining this request");
		PRINT_IF_VERB("Both TCLAS & QOS attributes element are absent");
		return retval;
	}

	retval = parse_scs_tclas_elements(scs_temp, &frm, &len);
	if (retval != SCS_API_STATUS_SUCCESS) {
		PRINT_IF_VERB("SCS TCLAS element parse error");
		retval = SCS_API_SCS_REQUEST_DECLINED;
		return retval;
	}

	PRINT_IF_VERB("SCS request - QOS element dump, len:%d", len);
	SCS_API_PKT_HEXDUMP(frm, len);

	if (parse_scs_qos_attributes(scs_temp, frm, &len) !=
						SCS_API_STATUS_SUCCESS) {
		retval = SCS_API_SCS_REQUEST_DECLINED;
		return retval;
	}

	PRINT_IF_VERB("SCS descriptor is parsed successfully");

	/* Checking if frame length is parsed rightly */
	if (len != 0) {
		PRINT_IF_VERB("Optional subelements are also present, len:%d",
			      len);
	}

	retval = SCS_API_SCS_SUCCESS;
	return retval;
}

static void scs_api_recv_scs_req(char *ifname, uint8_t *frm,
				 uint32_t frame_length, uint8_t *link_mac_addr,
				 uint8_t *mld_mac_addr, bool is_mlo)
{
	uint8_t elem;
	uint8_t idx = 0;
	uint8_t scs_id;
	uint16_t status;
	uint8_t len;
	uint16_t scs_data_size;
	struct scs_api_data scs_temp;
	uint8_t dialog_token = SCS_API_SCS_GET_DIALOG_TOKEN(frm);
	uint16_t retval = 0;

	scs_data_size = sizeof(struct scs_api_data);
	uint8_t parsed_scs_data[scs_data_size * SCS_API_SCS_MAX_SIZE];

	PRINT_IF_VERB("dialog_token %d", dialog_token);
	/* Moving 3 bytes to move to SCS descriptor*/
	frm += 3;
	frame_length -= 3;

	while (frame_length > 0) {
		status = SCS_API_SCS_REQUEST_DECLINED;
		elem = *frm;
		if (elem != SCS_API_ELEMID_SCS_IE)
			break;

		if (idx >= SCS_API_SCS_MAX_SIZE) {
			PRINT_IF_VERB(
			  "Rcvd more than %d SCS descriptors in 1 SCS Frame",
			  SCS_API_SCS_MAX_SIZE);
			break;
		}

		len = *(frm + 1);
		scs_id = SCS_API_SCS_GET_SCSID(frm);

		memset(&scs_temp, 0, sizeof(struct scs_api_data));
		status = scs_api_parse_scs_req(ifname, frm, len, &scs_temp);

		/* Parsing status of each SCS descriptor is stored in the SCS
		 * data structure here. This will be used by the driver as an
		 * indication to process the specific SCS descriptor
		 * accordingly. Since this sample SCS application does not do
		 * any bookkeeping, this logic is used. If application is
		 * enhanced in future to maintain its own database, this value
		 * can be set to success.
		 */
		scs_temp.parsing_status_code = status;
		if (status != SCS_API_SCS_SUCCESS) {
			PRINT_IF_VERB("SCS Parse failed, status:%d", status);
			PRINT_IF_VERB("scs_id:%d, idx:%d", scs_id, idx);
		}

		memcpy((parsed_scs_data + (idx * scs_data_size)), &scs_temp,
		       scs_data_size);

		frm += (len + 2);
		frame_length -= (len + 2);

		idx++;
	}

	retval = process_nl_msg_send_scs_api_data(ifname, link_mac_addr,
						  mld_mac_addr, is_mlo,
						  parsed_scs_data, idx,
						  dialog_token);

	if (retval)
		PRINT_IF_VERB("Send SCS API data failed in application");
}

static
void scs_api_recv_driver_response(char *ifname, uint8_t *link_mac_addr,
				  uint8_t *mld_mac_addr, uint8_t *drv_resp,
				  uint8_t num_scs_desc, uint8_t dialog_token,
				  bool is_mlo)
{
	uint8_t idx = 0;
	struct scs_api_drv_resp temp_drv_resp;
	struct scs_api_drv_resp scs_api_app_resp[SCS_API_SCS_MAX_SIZE];
	uint8_t resp_data_size = sizeof(struct scs_api_drv_resp);
	uint8_t total_desc = num_scs_desc;
	uint16_t retval = 0;

	if (total_desc > SCS_API_SCS_MAX_SIZE) {
		PRINT_IF_VERB("Total SCS descriptors greater than Max Value");
		return;
	}

	PRINT_IF_VERB("SCS Driver Response: SCS ID - Status code dump");
	while (total_desc > 0) {
		memcpy(&temp_drv_resp, (drv_resp + (idx * resp_data_size)),
		       resp_data_size);

		scs_api_app_resp[idx].scs_id = temp_drv_resp.scs_id;
		scs_api_app_resp[idx].svc_id = temp_drv_resp.svc_id;
		scs_api_app_resp[idx].status_code = temp_drv_resp.status_code;
		PRINT_IF_VERB(
		      "SCS ID:%d, SVC ID:%d, Driver Response status:%d, idx:%d",
		      scs_api_app_resp[idx].scs_id,
		      scs_api_app_resp[idx].svc_id,
		      scs_api_app_resp[idx].status_code, idx);
		total_desc--;
		idx++;
	}

	/* SCS DRV resp can be manipulated by application in future */
	retval = process_nl_msg_send_scs_api_response_data(
						ifname, scs_api_app_resp,
						link_mac_addr, mld_mac_addr,
						is_mlo, num_scs_desc,
						dialog_token);

	if (retval)
		PRINT_IF_VERB("Send SCS API response data failed in app");
}

static void process_nl_msg_recv_scs_api_scs_req(uint8_t *data, size_t len,
						char *ifname)
{
	struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX + 1];
	struct nlattr *tb;
	uint8_t link_mac_addr[SCS_API_MAC_ADDR_SIZE];
	uint8_t mld_mac_addr[SCS_API_MAC_ADDR_SIZE];
	bool is_mlo = false;
	uint8_t scs_req[SCS_API_REQ_BUF_MAX_SIZE];

	PRINT_IF_VERB("Received NL message - SCS Frame from driver");

	nla_parse(tb_array, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX,
		  (struct nlattr *)data, len,
		  qca_wlan_vendor_scs_api_app_policy);

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_PEER_MAC];
	if (tb) {
		/* Driver sends Link mac in case of both MLO or Non-MLO */
		memcpy(link_mac_addr, nla_data(tb), nla_len(tb));
		PRINT_IF_VERB("Len %d", nla_len(tb));
		PRINT_IF_VERB("Link Mac Address: %02x:%02x:%02x:%02x:%02x:%02x",
			      link_mac_addr[0], link_mac_addr[1],
			      link_mac_addr[2], link_mac_addr[3],
			      link_mac_addr[4], link_mac_addr[5]);
	} else {
		PRINT_IF_VERB("Peer mac address not found");
		return;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MLD_MAC];
	if (tb) {
		is_mlo = true;
		memcpy(mld_mac_addr, nla_data(tb), nla_len(tb));
		PRINT_IF_VERB("Len %d", nla_len(tb));
		PRINT_IF_VERB("MLD Mac Address: %02x:%02x:%02x:%02x:%02x:%02x",
			      mld_mac_addr[0], mld_mac_addr[1], mld_mac_addr[2],
			      mld_mac_addr[3], mld_mac_addr[4],
			      mld_mac_addr[5]);
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DATA];
	if (tb) {
		PRINT_IF_VERB("Length of SCS request frame: %d", nla_len(tb));
		if (nla_len(tb) > SCS_API_REQ_BUF_MAX_SIZE) {
			PRINT_IF_VERB("Err: SCS req frame size > max buf size");
			return;
		}
		memcpy(scs_req, nla_data(tb), nla_len(tb));
		SCS_API_PKT_HEXDUMP(scs_req, nla_len(tb));
	} else {
		PRINT_IF_VERB("SCS req frame not found");
		return;
	}

	scs_api_recv_scs_req(ifname, scs_req, len, link_mac_addr, mld_mac_addr,
			     is_mlo);
}

static void process_nl_msg_recv_scs_api_driver_response(uint8_t *data,
							size_t len,
							char *ifname)
{
	struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX + 1];
	struct nlattr *tb;
	uint8_t link_mac_addr[SCS_API_MAC_ADDR_SIZE];
	uint8_t mld_mac_addr[SCS_API_MAC_ADDR_SIZE];
	uint8_t dialog_token;
	uint8_t num_scs_desc;
	uint8_t drv_resp[SCS_API_RESP_BUF_MAX_SIZE];
	bool is_mlo = false;

	PRINT_IF_VERB("Received NL message - SCS driver response");

	nla_parse(tb_array, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX,
		  (struct nlattr *)data, len,
		  qca_wlan_vendor_scs_api_app_policy);

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_PEER_MAC];
	if (tb) {
		/* Driver sends Link mac in case of both MLO/Non-MLO */
		memcpy(link_mac_addr, nla_data(tb), nla_len(tb));
		PRINT_IF_VERB("Len %d", nla_len(tb));
		PRINT_IF_VERB("Link Mac Address: %02x:%02x:%02x:%02x:%02x:%02x",
			      link_mac_addr[0], link_mac_addr[1],
			      link_mac_addr[2], link_mac_addr[3],
			      link_mac_addr[4], link_mac_addr[5]);
	} else {
		PRINT_IF_VERB("Peer mac address not found");
		return;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MLD_MAC];
	if (tb) {
		is_mlo = true;
		memcpy(mld_mac_addr, nla_data(tb), nla_len(tb));
		PRINT_IF_VERB("Len %d", nla_len(tb));
		PRINT_IF_VERB("MLD Mac Address: %02x:%02x:%02x:%02x:%02x:%02x",
			      mld_mac_addr[0], mld_mac_addr[1], mld_mac_addr[2],
			      mld_mac_addr[3], mld_mac_addr[4],
			      mld_mac_addr[5]);
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DIALOG_TOKEN];
	if (tb) {
		dialog_token = nla_get_u8(tb);
		PRINT_IF_VERB("Dialog Token: %d", dialog_token);
	} else {
		PRINT_IF_VERB("Dialog token not found");
		return;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TOTAL_SCS_IDX];
	if (tb) {
		num_scs_desc = nla_get_u8(tb);
		PRINT_IF_VERB("Number of SCS descriptors: %d", num_scs_desc);
		if (num_scs_desc <= 0) {
			PRINT_IF_VERB("Invalid SCS descriptor count value");
			return;
		}
	} else {
		PRINT_IF_VERB("Number of SCS descriptor count is not found");
		return;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_DATA];
	if (tb) {
		PRINT_IF_VERB("Length of SCS Driver response: %d", nla_len(tb));
		if (nla_len(tb) > SCS_API_RESP_BUF_MAX_SIZE) {
			PRINT_IF_VERB("Err: Drv response size > Max buf size");
			return;
		}
		memcpy(drv_resp, nla_data(tb), nla_len(tb));
	} else {
		PRINT_IF_VERB("SCS API Driver response not found");
		return;
	}

	scs_api_recv_driver_response(ifname, link_mac_addr, mld_mac_addr,
				     drv_resp, num_scs_desc, dialog_token,
				     is_mlo);
}

static int
scs_api_send_msg_app_registration(const char *ifname,
				  struct scs_api_info *scs_api_app_info)
{
	int retval;

	PRINT_IF_VERB("SCS Init Info:");
	PRINT_IF_VERB("APP support:%d",
		      scs_api_app_info->scs_app_support_enable);
	PRINT_IF_VERB("Frame Fwd support:%d",
		      scs_api_app_info->frame_fwd_support);
	PRINT_IF_VERB("NW Manager support:%d",
		      scs_api_app_info->nw_mgr_support);

	retval = process_nl_msg_send_scs_api_app_reg(ifname,
						     scs_api_app_info);

	if (retval)
		PRINT_IF_VERB("SCS API registration send failed");

	return retval;
}

static void scs_api_nl_cb(void *cbd, uint8_t *data, size_t len, char *ifname)
{
	struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX + 1];
	struct nlattr *tb;
	uint8_t msg_type = 0;

	nla_parse(tb_array, QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_MAX,
		  (struct nlattr *)data, len,
		  qca_wlan_vendor_scs_api_app_policy);

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE];
	if (tb) {
		msg_type = nla_get_u8(tb);
		PRINT_IF_VERB("MSG_TYPE %u", msg_type);
	} else {
		PRINT_IF_VERB("No MSG TYPE");
		return;
	}

	switch (msg_type) {
	case QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE_SCS_REQUEST:
		process_nl_msg_recv_scs_api_scs_req(data, len, ifname);
		break;

	case QCA_WLAN_VENDOR_ATTR_SCS_API_MSG_TYPE_DRIVER_RESPONSE:
		process_nl_msg_recv_scs_api_driver_response(data, len, ifname);
		break;

	default:
		PRINT_IF_VERB("Wrong message type %d", msg_type);
	}
}

static void scs_api_dispatch_nl_event(struct nl_ctxt *ctxt, uint32_t cmdid,
				      uint8_t *data, size_t len, char *ifname)
{
	if (cmdid == QCA_NL80211_VENDOR_SUBCMD_SCS_API_MSG)
		ctxt->scs_cb.cb(ctxt->scs_cb.cbd, data, len, ifname);
}

static void scs_api_qca_nl_cb(char *ifname, uint32_t cmdid, uint8_t *data,
			      size_t len)
{
	scs_api_dispatch_nl_event(&g_nl_ctxt, cmdid, data, len, ifname);
}

static struct nl_ctxt *scs_api_nl_init(void)
{
	g_nl_ctxt.cfg80211_ctxt.event_callback = scs_api_qca_nl_cb;

	wifi_init_nl80211(&g_nl_ctxt.cfg80211_ctxt);
	wifi_nl80211_start_event_thread(&g_nl_ctxt.cfg80211_ctxt);

	return &g_nl_ctxt;
}

static void scs_api_nl_exit(struct nl_ctxt *ctxt)
{
	wifi_destroy_nl80211(&g_nl_ctxt.cfg80211_ctxt);
}

static const struct option long_options[] = {
	{ "verbose", no_argument, NULL, 'v' },
	{ "app_support", required_argument, NULL, 'a'},
	{ "frame_forwarding_support", required_argument, NULL, 'f'},
	{ "nw_manager_support", required_argument, NULL, 'n'},
	{ "help", no_argument, NULL, 'h' },
	{ NULL, no_argument, NULL, 0 },
};

static void display_help(void)
{
	printf("\nscs_api: Receive SCS frame from Driver and processes it\n");
	printf("\nUsage:\n"
			"scs_api wifix <option> <arguments>\n"
			"[-v][-a][-f][-n][-h | ?]\n"
			"-v --> verbose\n"
			"-a <1/0> --> App support enable\n"
			"-f <1/0> --> Frame forwarding support enable\n"
			"-n <1/0> --> NW manager support enable\n"
			"-h or ? for help with usage\n");
	exit(0);
}

static void scs_api_validate_interface(const char *ifname)
{
	if (strncmp(ifname, "wifi", WIFI_IFACE_PREFIX_LEN)) {
		printf("Interface name wifiX missing/wrong\n");
		printf("Not able to send cfg command\n");
		exit(0);
	}
}

int main(int argc, char *argv[])
{
	struct nl_ctxt *nl_ctxt = NULL;
	int c;
	int option_index = 0;
	const char *ifname = argv[1];
	struct scs_api_info scs_api_app_info;
	int retval;

	while (1) {
		c = getopt_long(argc, argv, "a:f:n:vh?:", long_options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'a': /* SCS API Application support enable */
			scs_api_app_info.scs_app_support_enable = atoi(optarg);
			break;
		case 'f': /* SCS API Frame Forward support enable */
			scs_api_app_info.frame_fwd_support = atoi(optarg);
			break;
		case 'n': /* SCS API - NW manager support enable */
			scs_api_app_info.nw_mgr_support = atoi(optarg);
			break;
		case 'v': /* Verbose level*/
			verbose = 1;
			break;
		case 'h': /* Help */
		case '?':
			display_help();
			return 0;
		default: /* Wrong option */
			printf("Unrecognized option %c\n", c);
			display_help();
			return -EINVAL;
		}
	}

	scs_api_validate_interface(ifname);

	nl_ctxt = scs_api_nl_init();
	if (!nl_ctxt) {
		printf("Unable to create NL receive context");
		goto fail;
	}

	retval = scs_api_send_msg_app_registration(ifname, &scs_api_app_info);
	if (retval) {
		printf("SCS API APP Init failed\n");
		goto fail;
	}

	while (1)
		usleep(1000);
fail:
	if (nl_ctxt)
		scs_api_nl_exit(nl_ctxt);
	return 0;
}
