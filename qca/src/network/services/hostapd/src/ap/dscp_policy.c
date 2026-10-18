/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "ap_config.h"
#include "ap_drv_ops.h"
#include "dscp_policy.h"

void hostapd_check_dscp_policy_capability(struct sta_info *sta,
					  const u8 *ies, size_t ies_len)
{
	const u8 *pos = ies;
	const u8 *end = ies + ies_len;

	sta->dscp_policy_capable = false;

	if (!(sta->flags & WLAN_STA_MFP))
		return;

	while (pos + 1 < end) {
		u8 id = *pos++;
		u8 len = *pos++;
		if (pos + len > end)
			break;
		if (id == WLAN_EID_VENDOR_SPECIFIC && len >= 5 &&
		    WPA_GET_BE24(pos) == OUI_WFA && pos[3] == WFA_CAPA_OUI_TYPE) {
			u8 cap_len = pos[4];
			if (cap_len >= 1 && len >= 5 + cap_len) {
				u8 cap = pos[5];
				if (cap & WFA_CAPA_QM_DSCP_POLICY) {
					sta->dscp_policy_capable = true;
					wpa_printf(MSG_DEBUG, "DSCP: STA " MACSTR
						   " supports DSCP Policy", MAC2STR(sta->addr));
				}
			}
		}
		pos += len;
	}
}

static int parse_ipv4_params(struct hostapd_dscp_policy *policy, const char *token)
{
	if (os_strncmp(token, "dst_ip=", 7) == 0) {
		if (inet_pton(AF_INET, token + 7, &policy->type4_param.ip_params.v4.dst_ip) <= 0)
			return -EINVAL;
	} else if (os_strncmp(token, "src_ip=", 7) == 0) {
		if (inet_pton(AF_INET, token + 7, &policy->type4_param.ip_params.v4.src_ip) <= 0)
			return -EINVAL;
	} else if (os_strncmp(token, "dst_port=", 9) == 0) {
		policy->type4_param.ip_params.v4.dst_port = atoi(token + 9);
	} else if (os_strncmp(token, "src_port=", 9) == 0) {
		policy->type4_param.ip_params.v4.src_port = atoi(token + 9);
	} else if (os_strncmp(token, "protocol=", 9) == 0) {
		policy->type4_param.ip_params.v4.protocol = atoi(token + 9);
	}
	return 0;
}

static int parse_ipv6_params(struct hostapd_dscp_policy *policy, const char *token)
{
	if (os_strncmp(token, "dst_ip=", 7) == 0) {
		if (inet_pton(AF_INET6, token + 7, &policy->type4_param.ip_params.v6.dst_ip) <= 0)
			return -EINVAL;
	} else if (os_strncmp(token, "src_ip=", 7) == 0) {
		if (inet_pton(AF_INET6, token + 7, &policy->type4_param.ip_params.v6.src_ip) <= 0)
			return -EINVAL;
	} else if (os_strncmp(token, "dst_port=", 9) == 0) {
		policy->type4_param.ip_params.v6.dst_port = atoi(token + 9);
	} else if (os_strncmp(token, "src_port=", 9) == 0) {
		policy->type4_param.ip_params.v6.src_port = atoi(token + 9);
	} else if (os_strncmp(token, "protocol=", 9) == 0) {
		policy->type4_param.ip_params.v6.next_header = atoi(token + 9);
	}

	return 0;
}

static int parse_dscp_value(struct hostapd_dscp_policy *policy, const char *token)
{
	if (os_strncmp(token, "policy_id=", 10) == 0) {
		u8 policy_id = atoi(token + 10);

		if (policy_id > 0 && policy_id <= 255) {
			policy->policy_id = policy_id;
		} else {
			wpa_printf(MSG_ERROR, "Invalid DSCP value: %u", policy_id);
			return -EINVAL;
		}
	} else if (os_strncmp(token, "request_type=", 13) == 0) {
		const char *val = token + 13;

		policy->req_type = os_strcasecmp(val, "Add") == 0 ?
						DSCP_POLICY_REQ_ADD : DSCP_POLICY_REQ_REMOVE;
	} else if (os_strncmp(token, "dscp=", 5) == 0) {
		unsigned long dscp = strtoul(token + 5, NULL, 0);

		if (dscp <= 63) {
			policy->dscp = dscp;
			policy->dscp_info = true;
		} else {
			wpa_printf(MSG_ERROR, "Invalid DSCP value: %lu", dscp);
			return -EINVAL;
		}
	}

	return 0;
}

static int parse_domain_name(struct hostapd_dscp_policy *policy, const char *token)
{
	const char *name;

	if (os_strncmp(token, "domain_name=", 12) == 0) {
		name = token + 12;
		if (os_strlen(name) <= 255) {
			policy->domain_name = (const u8 *) os_strdup(name);
			if (!policy->domain_name)
				return -ENOMEM;
			policy->domain_name_len = os_strlen((const char *) policy->domain_name);
		} else {
			wpa_printf(MSG_ERROR, "Domain name too long");
			return -EINVAL;
		}
	}
	return 0;
}

static int parse_port_range(struct hostapd_dscp_policy *policy, const char *token)
{
	int port;

	if (os_strncmp(token, "start_port=", 11) == 0) {
		port = atoi(token + 11);
		if (port >= 0 && port <= 65535) {
			policy->start_port = port;
			policy->port_range_info = true;
		} else {
			wpa_printf(MSG_ERROR, "Invalid start port: %d", port);
			return -EINVAL;
		}
	} else if (os_strncmp(token, "end_port=", 9) == 0) {
		port = atoi(token + 9);
		if (port >= 0 && port <= 65535) {
			policy->end_port = port;
			policy->port_range_info = true;
		} else {
			wpa_printf(MSG_ERROR, "Invalid end port: %d", port);
			return -EINVAL;
		}
	}
	return 0;
}

int validate_dscp_policy(struct hostapd_dscp_policy *policy)
{
	if (!policy)
		return -1;

	/* Domain name + destination IP not allowed */
	if (policy->domain_name &&
	    (policy->type4_param.classifier_mask & BIT(2))) {
		wpa_printf(MSG_ERROR,
			   "DSCP: Both domain name and destination IP address not expected");
		return -EINVAL;
	}

	/* Port range + Destination port not allowed */
	if ((policy->type4_param.classifier_mask & BIT(4)) &&
	    policy->port_range_info) {
		wpa_printf(MSG_ERROR,
			   "DSCP: Both port range and destination port not expected");
		return -EINVAL;
	}
	return 0;
}

int parse_dscp_policy_string(struct sta_info *sta, struct hostapd_dscp_policy *policy, const char *params)
{
	char *token, *buf;
	char *context = NULL;
	int ret = 0;

	if (!policy || !params)
		return -1;

	buf = os_strdup(params);
	if (!buf)
		return -1;

	os_memset(policy, 0, sizeof(*policy));
	context = buf;

	while ((token = str_token(buf, " ", &context))) {

		if (os_strncmp(token, "classifier_mask=", 16) == 0) {
			policy->type4_param.classifier_mask = strtoul(token + 16, NULL, 0);
		} else if (os_strncmp(token, "ip_version=", 11) == 0) {
			policy->type4_param.ip_version = atoi(token + 11);
		} else if (os_strncmp(token, "policy_id=", 10) == 0 ||
			   os_strncmp(token, "request_type=", 13) == 0 ||
			   os_strncmp(token, "dscp=", 5) == 0) {
			ret = parse_dscp_value(policy, token);
			if (ret < 0)
				goto out;
		} else if (os_strncmp(token, "domain_name=", 12) == 0) {
			ret = parse_domain_name(policy, token);
			if (ret < 0)
				goto out;
		} else if (os_strncmp(token, "start_port=", 11) == 0 ||
			   os_strncmp(token, "end_port=", 9) == 0) {
			ret = parse_port_range(policy, token);
			if (ret < 0)
				goto out;
		} else if (policy->type4_param.ip_version == 4) {
			ret = parse_ipv4_params(policy, token);
			if (ret < 0)
				goto out;
		} else if (policy->type4_param.ip_version == 6) {
			ret = parse_ipv6_params(policy, token);
			if (ret < 0)
				goto out;
		} else {
			wpa_printf(MSG_DEBUG, "DSCP: Unknown attribute '%s' skipped",
				   token);
			ret = -EINVAL;
			goto out;
		}
	}

out:
	os_free(buf);
	return ret;
}

int build_frame_classifier_type4_ipv4(struct hostapd_dscp_policy *policy)
{
       u8 *buf;
       u8 classifier_mask;
       size_t len = IPV4_CLASSIFIER_LEN;

       if (!policy)
               return -1;

       classifier_mask = policy->type4_param.classifier_mask;
       buf = os_zalloc(len);
       if (!buf)
               return -1;

       buf[0] = CLASSIFIER_TYPE_4;
       buf[1] = classifier_mask;
       buf[2] = IP_VERSION_4;

       if (classifier_mask & MASK_SRC_IP)
               os_memcpy(&buf[3], &policy->type4_param.ip_params.v4.src_ip, IPV4_ADDR_LEN);

       if (classifier_mask & MASK_DST_IP)
               os_memcpy(&buf[7], &policy->type4_param.ip_params.v4.dst_ip, IPV4_ADDR_LEN);

       if (classifier_mask & MASK_SRC_PORT)
               WPA_PUT_BE16(&buf[11], policy->type4_param.ip_params.v4.src_port);

       if (classifier_mask & MASK_DST_PORT)
               WPA_PUT_BE16(&buf[13], policy->type4_param.ip_params.v4.dst_port);

       if (classifier_mask & MASK_PROTOCOL)
               buf[16] = policy->type4_param.ip_params.v4.protocol;

	policy->frame_classifier = buf;
	policy->frame_classifier_len = len;
	return 0;
}

int build_frame_classifier_type4_ipv6(struct hostapd_dscp_policy *policy)
{
	u8 *buf;
	u8 classifier_mask;
	size_t len = IPV6_CLASSIFIER_LEN;

	if (!policy)
		return -EINVAL;

	classifier_mask = policy->type4_param.classifier_mask;
	buf = os_zalloc(len);
	if (!buf)
		return -ENOMEM;

	buf[0] = CLASSIFIER_TYPE_4;
	buf[1] = classifier_mask;
	buf[2] = IP_VERSION_6;

	if (classifier_mask & MASK_SRC_IP)
		os_memcpy(&buf[3], &policy->type4_param.ip_params.v6.src_ip, IPV6_ADDR_LEN);

	if (classifier_mask & MASK_DST_IP)
		os_memcpy(&buf[19], &policy->type4_param.ip_params.v6.dst_ip, IPV6_ADDR_LEN);

	if (classifier_mask & MASK_SRC_PORT)
		WPA_PUT_BE16(&buf[35], policy->type4_param.ip_params.v6.src_port);

	if (classifier_mask & MASK_DST_PORT)
		WPA_PUT_BE16(&buf[37], policy->type4_param.ip_params.v6.dst_port);

	if (classifier_mask & MASK_PROTOCOL)
		buf[40] = policy->type4_param.ip_params.v6.next_header;

	policy->frame_classifier = buf;
	policy->frame_classifier_len = len;

	return 0;
}

int build_frame_classifier(struct hostapd_dscp_policy *policy)
{

	switch (policy->type4_param.ip_version) {
	case IP_VERSION_4:
		return build_frame_classifier_type4_ipv4(policy);
	case IP_VERSION_6:
		return build_frame_classifier_type4_ipv6(policy);
	default:
		wpa_printf(MSG_DEBUG, "IP version not specified\n");
		return 0;
	}
}

int add_dscp_policy_to_sta(struct sta_info *sta,
			   struct hostapd_dscp_policy *new_policy)
{
	struct hostapd_dscp_policy **updated_policy_list;
	struct hostapd_dscp_policy *copy;
	u8 i;

	if (new_policy->policy_id < 1 || new_policy->policy_id > 255) {
		wpa_printf(MSG_ERROR, "DSCP Invalid policy_id\n");
		return -EINVAL;
	}

	/* Check for existing policy_id and update */
	for (i = 0; i < sta->num_dscp_policies; i++) {
		struct hostapd_dscp_policy *existing = sta->policies[i];

		if (existing && existing->policy_id == new_policy->policy_id) {
			wpa_printf(MSG_DEBUG, "DSCP: Updating policy ID %u for STA " MACSTR,
				   new_policy->policy_id, MAC2STR(sta->addr));
			os_free((void *)existing->frame_classifier);
			os_free((void *)existing->domain_name);
			os_memcpy(existing, new_policy, sizeof(*existing));
			if (new_policy->domain_name) {
				existing->domain_name = (u8 *)os_strdup((const char *)new_policy->domain_name);
				if (!existing->domain_name)
					return -ENOMEM;
			}

			if (new_policy->frame_classifier &&
			    new_policy->frame_classifier_len > 0) {
				existing->frame_classifier = os_memdup(new_policy->frame_classifier,
								       new_policy->frame_classifier_len);
				if (!existing->frame_classifier)
					return -ENOMEM;
			}
			return 0;
		}
	}

	copy = os_memdup(new_policy, sizeof(*copy));
	if (!copy)
		return -ENOMEM;

	if (new_policy->domain_name) {
		copy->domain_name = (u8 *)os_strdup((const char *)new_policy->domain_name);
		if (!copy->domain_name) {
			os_free(copy);
			return -ENOMEM;
		}
	}

	if (new_policy->frame_classifier &&
	    new_policy->frame_classifier_len > 0) {
		copy->frame_classifier = os_memdup(new_policy->frame_classifier,
						   new_policy->frame_classifier_len);
		if (!copy->frame_classifier) {
			os_free((void *)copy->domain_name);
			os_free(copy);
			return -ENOMEM;
		}
	}

	updated_policy_list = os_realloc_array(sta->policies,
					       sta->num_dscp_policies + 1,
					       sizeof(*sta->policies));
	if (!updated_policy_list) {
		os_free((void *)copy->domain_name);
		os_free((void *)copy->frame_classifier);
		os_free(copy);
		return -1;
	}

	sta->policies = updated_policy_list;
	sta->policies[sta->num_dscp_policies++] = copy;

	return 0;
}

void free_dscp_policy(struct hostapd_dscp_policy *policy)
{
	if (!policy)
		return;

	os_free((void *)policy->frame_classifier);
	os_free((void *)policy->domain_name);
	os_free(policy);
}


void free_dscp_policies(struct sta_info *sta)
{
	if (!sta || !sta->policies)
		return;

	for (u8 i = 0; i < sta->num_dscp_policies; i++)
		free_dscp_policy(sta->policies[i]);

	os_free(sta->policies);
	sta->policies = NULL;
	sta->num_dscp_policies = 0;
}

u8 get_next_unsolicited_dialog_token(struct sta_info *sta)
{
	sta->unsolicited_dialog_token++;
	if (sta->unsolicited_dialog_token == 0)
		sta->unsolicited_dialog_token = 1;
	return sta->unsolicited_dialog_token;
}

struct hostapd_dscp_policy *hostapd_get_dscp_policy_by_id(struct sta_info *sta, int id)
{
	size_t i;

	for (i = 0; i < sta->num_dscp_policies; i++) {
		if (sta->policies[i] && sta->policies[i]->policy_id == id)
			return sta->policies[i];
	}
	return NULL;
}


struct wpabuf *hostapd_build_qos_element(struct hostapd_dscp_policy *policy)
{
	struct wpabuf *elem;
	size_t domain_len;

	elem = wpabuf_alloc(256);
	if (!elem)
		return NULL;

	wpabuf_put_be32(elem, QM_IE_VENDOR_TYPE);
	wpabuf_put_u8(elem, QM_ATTR_DSCP_POLICY);
	wpabuf_put_u8(elem, 3);
	wpabuf_put_u8(elem, policy->policy_id);
	wpabuf_put_u8(elem, policy->req_type);
	wpabuf_put_u8(elem,
		      policy->req_type == DSCP_POLICY_REQ_REMOVE ? 255 : policy->dscp);

	if (policy->frame_classifier && policy->frame_classifier_len > 0) {
		wpabuf_put_u8(elem, QM_ATTR_TCLAS);
		wpabuf_put_u8(elem, policy->frame_classifier_len);
		wpabuf_put_data(elem, policy->frame_classifier, policy->frame_classifier_len);
	}

	if (policy->domain_name) {
		domain_len = os_strlen((const char *)policy->domain_name);
		if (domain_len < 256) {
			wpabuf_put_u8(elem, QM_ATTR_DOMAIN_NAME);
			wpabuf_put_u8(elem, domain_len);
			wpabuf_put_data(elem, policy->domain_name, domain_len);
		}
	}

	if (policy->port_range_info) {
		wpabuf_put_u8(elem, QM_ATTR_PORT_RANGE);
		wpabuf_put_u8(elem, 4);
		wpabuf_put_be16(elem, policy->start_port);
		wpabuf_put_be16(elem, policy->end_port);
	}

	return elem;
}

static struct wpabuf *start_new_dscp_frame(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   u8 dialog_token,
					   u8 reset, u8 more)
{
	u8 request_control = 0;
	struct wpabuf *frame;

	frame = wpabuf_alloc(MAX_DSCP_REQ_SIZE);
	if (!frame)
		return NULL;

	wpabuf_put_u8(frame, WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED);
	wpabuf_put_be32(frame, QM_ACTION_VENDOR_TYPE);
	wpabuf_put_u8(frame, QM_DSCP_POLICY_REQ);
	wpabuf_put_u8(frame, dialog_token);

	if (reset)
		request_control |= 0x02;
	if (more)
		request_control |= 0x01;

	wpabuf_put_u8(frame, request_control);

	return frame;
}

void hostapd_send_unsolicited_dscp_policy_request(struct hostapd_data *hapd,
						  struct sta_info *sta,
						  u8 reset,
						  const int *policy_ids,
						  size_t num_policies)
{
	struct wpabuf *frame = NULL, *elem;
	struct hostapd_dscp_policy *policy;
	struct sta_info *assoc_sta = sta;
	struct hostapd_data *assoc_hapd = hapd;
	u8 dialog_token;
	size_t i;
	size_t frame_len;
	bool more = false;

	if (!hapd || !sta)
		return;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_DEBUG,
				   "Assoc STA not found in DSCP request send");
			return;
		}
	}
#endif /* CONFIG_IEEE80211BE */
	if (!assoc_sta->dscp_policy_capable)
		return;

	dialog_token = get_next_unsolicited_dialog_token(assoc_sta);
	frame = start_new_dscp_frame(assoc_hapd, assoc_sta, dialog_token, reset,
				     more);
	if (!frame) {
		wpa_printf(MSG_INFO, "DSCP frame is NULL\n");
		return;
	}

	frame_len = wpabuf_len(frame);

	for (i = 0; i < num_policies; i++) {
		policy = hostapd_get_dscp_policy_by_id(assoc_sta, policy_ids[i]);
		if (!policy)
			continue;

		if (reset && policy->req_type == DSCP_POLICY_REQ_REMOVE)
			continue;

		elem = hostapd_build_qos_element(policy);
		if (!elem)
			continue;

		if (frame_len + 2 + wpabuf_len(elem) > MAX_DSCP_REQ_SIZE) {
			more = true;
			wpabuf_free(elem);
			break;
		}

		wpabuf_put_u8(frame, WLAN_EID_VENDOR_SPECIFIC);
		wpabuf_put_u8(frame, wpabuf_len(elem));
		wpabuf_put_buf(frame, elem);
		wpabuf_free(elem);
		frame_len = wpabuf_len(frame);
	}

	if (more) {
		u8 *buf = wpabuf_mhead_u8(frame);
		buf[7] |= 0x01;
	}

	if (frame && wpabuf_len(frame) > 5) {
		if (hostapd_drv_send_action(assoc_hapd, assoc_hapd->iface->freq, 0,
					    assoc_sta->addr, wpabuf_head(frame),
					    wpabuf_len(frame))) {
			wpa_printf(MSG_DEBUG, "DSCP: Failed to send policy request to " MACSTR,
				   MAC2STR(assoc_sta->addr));
		}
	}

	wpabuf_free(frame);

	assoc_sta->dscp_state.offset = i;
	assoc_sta->dscp_state.last_dialog_token = dialog_token;
	assoc_sta->dscp_state.pending_more = more;
}


static int hostapd_parse_query_elements(struct hostapd_dscp_policy *query, u8 attr_id,
					u8 attr_len, const u8 *attr_data, u8 *attr_count)
{
	switch (attr_id) {
	case QM_ATTR_PORT_RANGE:
		if (attr_len < 4) {
			wpa_printf(MSG_ERROR,
				   "DAP: Received Port Range attribute with insufficient length %d",
				   attr_len);
			return -EINVAL;
		}
		if (query->port_range_info) {
			wpa_printf(MSG_DEBUG, "DSCP Policy: Duplicate Port Range");
			return -EINVAL;
		}
		query->start_port = WPA_GET_BE16(attr_data);
		query->end_port = WPA_GET_BE16(attr_data + 2);
		query->port_range_info = true;
		attr_count++;
		break;
	case QM_ATTR_TCLAS:
		if (attr_len < 1) {
			wpa_printf(MSG_ERROR,
				   "DAP: Received TCLAS attribute with insufficient length %d",
				   attr_len);
			return -EINVAL;
		}
		if (query->frame_classifier) {
			wpa_printf(MSG_DEBUG, "DSCP Policy: Duplicate TCLAS");
			return -EINVAL;
		}
		query->frame_classifier = attr_data;
		query->frame_classifier_len = attr_len;
		attr_count++;
		break;
	case QM_ATTR_DOMAIN_NAME:
		if (attr_len < 1 || attr_len > 255) {
			wpa_printf(MSG_ERROR,
				   "DAP: Received domain name attribute with insufficient length %d",
				   attr_len);
			return -EINVAL;
		}
		if (query->domain_name) {
			wpa_printf(MSG_DEBUG, "DSCP Policy: Duplicate Domain Name");
			return -EINVAL;
		}
		query->domain_name = attr_data;
		query->domain_name_len = attr_len;
		attr_count++;
		break;
	default:
		break;
	}
	return 0;
}

static bool is_valid_qm_ie(const u8 *ie, size_t rem_len)
{
	if (!ie || rem_len < 6)
		return false;

	if (ie[0] != WLAN_EID_VENDOR_SPECIFIC || ie[1] < 4)
		return false;

	if (WPA_GET_BE32(&ie[2]) != QM_IE_VENDOR_TYPE)
		return false;

	return true;
}

static int set_frame_classifier_type4_ipv4(struct hostapd_dscp_policy *policy)
{
	u8 classifier_mask;
	const u8 *frame_classifier = policy->frame_classifier;
	struct type4_params *type4_param = &policy->type4_param;

	if (policy->frame_classifier_len < 18) {
		wpa_printf(MSG_ERROR,
			   "QM: Received IPv4 frame classifier with insufficient length %d",
			   policy->frame_classifier_len);
		return -1;
	}

	classifier_mask = frame_classifier[1];

	/* Classifier Mask - bit 1 = Source IP Address */
	if (classifier_mask & BIT(1)) {
		type4_param->classifier_mask |= BIT(1);
		os_memcpy(&type4_param->ip_params.v4.src_ip,
			  &frame_classifier[3], 4);
	}

	/* Classifier Mask - bit 2 = Destination IP Address */
	if (classifier_mask & BIT(2)) {
		if (policy->domain_name) {
			wpa_printf(MSG_ERROR,
				   "QM: IPv4: Both domain name and destination IP address not expected");
			return -1;
		}

		type4_param->classifier_mask |= BIT(2);
		os_memcpy(&type4_param->ip_params.v4.dst_ip,
			  &frame_classifier[7], 4);
	}

	/* Classifier Mask - bit 3 = Source Port */
	if (classifier_mask & BIT(3)) {
		type4_param->classifier_mask |= BIT(3);
		type4_param->ip_params.v4.src_port =
			WPA_GET_BE16(&frame_classifier[11]);
	}

	/* Classifier Mask - bit 4 = Destination Port */
	if (classifier_mask & BIT(4)) {
		if (policy->port_range_info) {
			wpa_printf(MSG_ERROR,
				   "QM: IPv4: Both port range and destination port not expected");
			return -1;
		}

		type4_param->classifier_mask |= BIT(4);
		type4_param->ip_params.v4.dst_port =
			WPA_GET_BE16(&frame_classifier[13]);
	}

	/* Classifier Mask - bit 5 = DSCP (ignored) */

	/* Classifier Mask - bit 6 = Protocol */
	if (classifier_mask & BIT(6)) {
		type4_param->classifier_mask |= BIT(6);
		type4_param->ip_params.v4.protocol = frame_classifier[16];
	}

	return 0;
}


static int set_frame_classifier_type4_ipv6(struct hostapd_dscp_policy *policy)
{
	u8 classifier_mask;
	const u8 *frame_classifier = policy->frame_classifier;
	struct type4_params *type4_param = &policy->type4_param;

	if (policy->frame_classifier_len < 44) {
		wpa_printf(MSG_ERROR,
			   "QM: Received IPv6 frame classifier with insufficient length %d",
			   policy->frame_classifier_len);
		return -1;
	}

	classifier_mask = frame_classifier[1];

	/* Classifier Mask - bit 1 = Source IP Address */
	if (classifier_mask & BIT(1)) {
		type4_param->classifier_mask |= BIT(1);
		os_memcpy(&type4_param->ip_params.v6.src_ip,
			  &frame_classifier[3], 16);
	}

	/* Classifier Mask - bit 2 = Destination IP Address */
	if (classifier_mask & BIT(2)) {
		if (policy->domain_name) {
			wpa_printf(MSG_ERROR,
				   "QM: IPv6: Both domain name and destination IP address not expected");
			return -1;
		}
		type4_param->classifier_mask |= BIT(2);
		os_memcpy(&type4_param->ip_params.v6.dst_ip,
			  &frame_classifier[19], 16);
	}

	/* Classifier Mask - bit 3 = Source Port */
	if (classifier_mask & BIT(3)) {
		type4_param->classifier_mask |= BIT(3);
		type4_param->ip_params.v6.src_port =
				WPA_GET_BE16(&frame_classifier[35]);
	}

	/* Classifier Mask - bit 4 = Destination Port */
	if (classifier_mask & BIT(4)) {
		if (policy->port_range_info) {
			wpa_printf(MSG_ERROR,
				   "IPv6: Both port range and destination port not expected");
			return -1;
		}

		type4_param->classifier_mask |= BIT(4);
		type4_param->ip_params.v6.dst_port =
				WPA_GET_BE16(&frame_classifier[37]);
	}

	/* Classifier Mask - bit 5 = DSCP (ignored) */

	/* Classifier Mask - bit 6 = Next Header */
	if (classifier_mask & BIT(6)) {
		type4_param->classifier_mask |= BIT(6);
		type4_param->ip_params.v6.next_header = frame_classifier[40];
	}

	return 0;
}


static int ap_set_frame_classifier_params(struct hostapd_dscp_policy *policy)
{
	const u8 *frame_classifier = policy->frame_classifier;
	u8 frame_classifier_len = policy->frame_classifier_len;

	if (frame_classifier_len < 3) {
		wpa_printf(MSG_ERROR,
			   "QM: Received frame classifier with insufficient length %d",
			   frame_classifier_len);
		return -1;
	}

	/* Only allowed Classifier Type: IP and higher layer parameters (4) */
	if (frame_classifier[0] != 4) {
		wpa_printf(MSG_ERROR,
			   "QM: Received frame classifier with invalid classifier type %d",
			   frame_classifier[0]);
		return -1;
	}

	/* Classifier Mask - bit 0 = Version */
	if (!(frame_classifier[1] & BIT(0))) {
		wpa_printf(MSG_ERROR,
			   "QM: Received frame classifier without IP version");
		return -1;
	}

	/* Version (4 or 6) */
	if (frame_classifier[2] == 4) {
		if (set_frame_classifier_type4_ipv4(policy)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv4 parameters");
			return -1;
		}

		policy->type4_param.ip_version = IPV4;
	} else if (frame_classifier[2] == 6) {
		if (set_frame_classifier_type4_ipv6(policy)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv6 parameters");
			return -1;
		}

		policy->type4_param.ip_version = IPV6;
	} else {
		wpa_printf(MSG_ERROR,
			   "QM: Received unknown IP version %d",
			   frame_classifier[2]);
		return -1;
	}

	return 0;
}

static int parse_single_policy(const u8 *ie, size_t ie_len,
			       struct hostapd_dscp_policy **el_out,
			       size_t index)
{
	const u8 *attr;
	int rem_attrs;
	struct hostapd_dscp_policy *el;
	u8 attr_count = 0;

	if (!ie || ie_len < 6)
		return -EINVAL;

	el = os_zalloc(sizeof(*el));
	if (!el)
		return -ENOMEM;

	attr = ie + 6;
	rem_attrs = ie[1] - 4;

	while (rem_attrs >= 2 + attr[1]) {
		u8 id = attr[0];

		if (id == QM_ATTR_DSCP_POLICY) {
			wpa_printf(MSG_ERROR, "DSCP Query: DSCP Policy attribute not allowed");
			os_free(el);
			return -EINVAL;
		}
		if (hostapd_parse_query_elements(el, attr[0], attr[1], &attr[2], &attr_count)) {
			os_free(el);
			return -EINVAL;
		}
		if (el->frame_classifier) {
			if (ap_set_frame_classifier_params(el)) {
				wpa_printf(MSG_ERROR,
					   "QM: Failed to set frame classifier parameters");
				os_free(el);
				return -EINVAL;
			}
		}
		rem_attrs -= 2 + attr[1];
		attr += 2 + attr[1];
	}

	*el_out = el;
	return 0;
}

int parse_dscp_query(const u8 *data, size_t len,
		     struct dscp_context *ctx)
{
	const u8 *pos, *end;
	struct hostapd_dscp_policy **parsed = NULL;
	size_t count = 0, i = 0;
	int ie_len;

	if (!data || len < 1 || !ctx)
		return -EINVAL;

	pos = data + 1;
	end = data + len;

	ctx->query_policy = NULL;
	ctx->num_query_policies = 0;
	ctx->dialog_token = data[0];
	ctx->is_wildcard = false;

	/* First pass: count valid IEs */
	while (end - pos >= 2) {
		ie_len = 2 + pos[1];
		if (end - pos < ie_len)
			break;

	if (is_valid_qm_ie(pos, ie_len))
		count++;

	pos += ie_len;
	}

	if (count == 0) {
		ctx->is_wildcard  = true;
		return 0;
	}

	parsed = os_calloc(count, sizeof(*parsed));
	if (!parsed)
		return -ENOMEM;

	/* Second pass: parse and populate */
	pos = data + 1;
	while (end - pos >= 2 && i < count) {
		ie_len = 2 + pos[1];
		if (end - pos < ie_len)
			break;

		if (!is_valid_qm_ie(pos, ie_len)) {
			pos += ie_len;
			continue;
		}

		if (parse_single_policy(pos, ie_len, &parsed[i], i) != 0)
			goto error;
		i++;

		pos += ie_len;
	}

	ctx->query_policy = parsed;
	ctx->num_query_policies = i;
	return 0;

error:
	for (size_t j = 0; j < i; j++)
		os_free(parsed[j]);
	os_free(parsed);
	return -1;
}

static bool match_type4_classifier(const struct type4_params *query,
				   const struct type4_params *policy,
				   u8 ip_version)
{
	if (query->classifier_mask != policy->classifier_mask)
		return false;

	if (ip_version == IPV4) {
		if ((query->classifier_mask & BIT(1)) &&
		    os_memcmp(&query->ip_params.v4.src_ip,
		     &policy->ip_params.v4.src_ip, 4) != 0)
			return false;

		if ((query->classifier_mask & BIT(2)) &&
		    os_memcmp(&query->ip_params.v4.dst_ip,
		    &policy->ip_params.v4.dst_ip, 4) != 0)
			return false;

		if ((query->classifier_mask & BIT(3)) &&
		    query->ip_params.v4.src_port != policy->ip_params.v4.src_port)
			return false;

		if ((query->classifier_mask & BIT(4)) &&
		    query->ip_params.v4.dst_port != policy->ip_params.v4.dst_port)
			return false;

		if ((query->classifier_mask & BIT(6)) &&
		    query->ip_params.v4.protocol != policy->ip_params.v4.protocol)
			return false;

	} else if (ip_version == IPV6) {
		if ((query->classifier_mask & BIT(1)) &&
		    os_memcmp(&query->ip_params.v6.src_ip,
		    &policy->ip_params.v6.src_ip, 16) != 0)
			return false;

		if ((query->classifier_mask & BIT(2)) &&
		    os_memcmp(&query->ip_params.v6.dst_ip,
		    &policy->ip_params.v6.dst_ip, 16) != 0)
			return false;

		if ((query->classifier_mask & BIT(3)) &&
		    query->ip_params.v6.src_port != policy->ip_params.v6.src_port)
			return false;

		if ((query->classifier_mask & BIT(4)) &&
		    query->ip_params.v6.dst_port != policy->ip_params.v6.dst_port)
			return false;

		if ((query->classifier_mask & BIT(6)) &&
		    query->ip_params.v6.next_header != policy->ip_params.v6.next_header)
			return false;
	} else {
		return false;
	}

	return true;
}

int policy_matches_query(struct dscp_context *ctx)
{
	struct hostapd_dscp_policy *policy, *query;
	struct hostapd_dscp_policy **tmp;
	size_t max_policies = 0;

	ctx->num_req_policies = 0;
	ctx->req_policy = NULL;

	/* Wildcard query: match all policies for the STA */
	if (ctx->is_wildcard) {
		max_policies = ctx->sta->num_dscp_policies;
		ctx->req_policy = os_malloc(max_policies * sizeof(*ctx->req_policy));
		if (!ctx->req_policy)
			return 0;
		for (size_t j = 0; j < max_policies; j++) {
			policy = ctx->sta->policies[j];
			if (!policy)
				continue;
			ctx->req_policy[ctx->num_req_policies++] = policy;
		}
		return ctx->num_req_policies;
	}

	for (size_t i = 0; i < ctx->num_query_policies; i++) {
		query = ctx->query_policy[i];

		for (size_t j = 0; j < ctx->sta->num_dscp_policies; j++) {
			policy = ctx->sta->policies[j];
			if (!policy)
				continue;
			bool match = false;

			/* Match port range */
			if (query->port_range_info && policy->port_range_info) {
				if (policy->start_port <= query->end_port &&
				    policy->end_port >= query->start_port)
					match = true;
			}


			if (query->domain_name && policy->domain_name &&
			    query->domain_name_len == policy->domain_name_len &&
			    os_memcmp(query->domain_name, policy->domain_name,
			    query->domain_name_len) == 0)
				match = true;

			if (query->frame_classifier && policy->frame_classifier) {
				if (query->type4_param.ip_version == policy->type4_param.ip_version &&
				    query->type4_param.classifier_mask == policy->type4_param.classifier_mask &&
				    match_type4_classifier(&query->type4_param, &policy->type4_param,
							   query->type4_param.ip_version))
					match = true;
			}

			if (!match)
				continue;

			tmp = os_realloc_array(ctx->req_policy, ctx->num_req_policies + 1,
					       sizeof(*ctx->req_policy));
			if (!tmp)
				continue;

			ctx->req_policy = tmp;
			ctx->req_policy[ctx->num_req_policies++] = policy;
			}
	}

	return ctx->num_req_policies;
}

static size_t policy_element_len(const struct hostapd_dscp_policy *policy)
{
	size_t len = 0;

	/* QoS Management element header: Element ID (1), Length (1), OUI (3), OUI Type (1) */
	len += 6;

	/* DSCP Policy attribute */
	len += 6;

	/* Classifier attributes */
	if (policy->frame_classifier_len)
		len += 2 + policy->frame_classifier_len;

	if (policy->port_range_info)
	/* Port Range attribute: Attribute ID (1) + Length (1) + Start Port (2) + End Port (2) */
		len += 6;

	if (policy->domain_name_len)
	/* Domain Name attribute: Attribute ID (1) + Length (1) + domain_name_len */
		len += 2 + policy->domain_name_len;

	return len;
}

static void add_policy_element(struct wpabuf *buf, const struct hostapd_dscp_policy *policy)
{
	struct wpabuf *elem;

	elem = wpabuf_alloc(policy_element_len(policy));
	if (!elem)
		return;

	wpabuf_put_be32(elem, QM_IE_VENDOR_TYPE);

	/* DSCP Policy attribute */
	wpabuf_put_u8(elem, QM_ATTR_DSCP_POLICY);
	wpabuf_put_u8(elem, 3); 
	wpabuf_put_u8(elem, policy->policy_id);
	wpabuf_put_u8(elem, policy->req_type);

	if (policy->req_type == DSCP_POLICY_REQ_REMOVE)
		wpabuf_put_u8(elem, 255); /* DSCP = 255 for REMOVE */
	else
		wpabuf_put_u8(elem, policy->dscp);

	if (policy->frame_classifier && policy->frame_classifier_len) {
		wpabuf_put_u8(elem, QM_ATTR_TCLAS);
		wpabuf_put_u8(elem, policy->frame_classifier_len);
		wpabuf_put_data(elem, policy->frame_classifier, policy->frame_classifier_len);
	}

	/* Classifier: Domain Name */
	if (policy->domain_name && policy->domain_name_len < 256) {
		wpabuf_put_u8(elem, QM_ATTR_DOMAIN_NAME);
		wpabuf_put_u8(elem, policy->domain_name_len);
		wpabuf_put_data(elem, policy->domain_name, policy->domain_name_len);
	}

	if (policy->port_range_info) {
		wpabuf_put_u8(elem, QM_ATTR_PORT_RANGE);
		wpabuf_put_u8(elem, 4);
		wpabuf_put_be16(elem, policy->start_port);
		wpabuf_put_be16(elem, policy->end_port);
	}

	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(buf, wpabuf_len(elem));
	wpabuf_put_buf(buf, elem);

	wpabuf_free(elem);
}

/* Build DSCP Policy Request frame */
static struct wpabuf *build_dscp_policy_request(struct hostapd_data *hapd,
						struct sta_info *sta,
						const struct dscp_context *ctx,
						size_t offset,
						size_t *policies_used,
						u8 dialog_token,
						bool *more)
{
	struct wpabuf *buf;
	u8 *rc_field;
	size_t i;

	buf = wpabuf_alloc(MAX_DSCP_REQ_SIZE);
	if (!buf)
		return NULL;

	/* Action frame header */
	wpabuf_put_u8(buf, WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED);
	wpabuf_put_be32(buf, QM_ACTION_VENDOR_TYPE);
	wpabuf_put_u8(buf, QM_DSCP_POLICY_REQ);

	/* Dialog Token */
	wpabuf_put_u8(buf, dialog_token);

	/* Request Control */
	rc_field = wpabuf_put(buf, 1);
	*rc_field = sta->dscp_reset ? 0x02 : 0x00;
	*more = false;

	for (i = offset; i < ctx->num_req_policies; i++) {
		const struct hostapd_dscp_policy *policy = ctx->req_policy[i];

		if (sta->dscp_reset && policy->req_type == DSCP_POLICY_REQ_REMOVE)
			continue;

		if (wpabuf_len(buf) + policy_element_len(policy) > MAX_DSCP_REQ_SIZE) {
			*rc_field |= 0x01;
			*more = true;
			break;
		}

		add_policy_element(buf, policy);
	}

	if (policies_used)
		*policies_used = i - offset;
	/* If no matching policies, leave QoS Management element list empty */
	if (ctx->num_req_policies == 0 && sta->dscp_reset == 0)
		wpa_printf(MSG_DEBUG, "QM: No matching policies — sending empty response");

	return buf;
}


int hostapd_handle_dscp_policy_query(struct hostapd_data *hapd, struct sta_info *sta,
				     const u8 *data, size_t len)
{
	struct wpabuf *resp;
	struct dscp_context ctx;
	size_t offset = 0, used;
	u8 dialog_token;
	bool more = false;


	if (!hapd->conf->enable_dscp_policy_capa)
		return -1;

	if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_DEBUG, "DSCP Policy: STA not authorized");
		return -1;
	}

	if (!sta->dscp_policy_capable) {
		wpa_printf(MSG_DEBUG, "DSCP: STA " MACSTR " not capable",
			   MAC2STR(sta->addr));
		return -1;
	}

	if (len < 0)
		return -1;

	os_memset(&ctx, 0, sizeof(ctx));
	ctx.hapd = hapd;
	ctx.sta = sta;

	if (parse_dscp_query(data, len, &ctx) < 0) {
		wpa_printf(MSG_DEBUG, "DSCP: Failed to parse query from " MACSTR,
			   MAC2STR(sta->addr));
		return -1;
	}

	if (policy_matches_query(&ctx) <= 0)
		wpa_printf(MSG_DEBUG, "QM: No matching DSCP policies found");

	dialog_token = ctx.dialog_token;

	resp = build_dscp_policy_request(hapd, sta, &ctx,
					 offset, &used, dialog_token, &more);
	if (!resp)
		goto cleanup;

	if (hostapd_drv_send_action(hapd, hapd->iface->freq, 0,
				    sta->addr, wpabuf_head(resp),
				    wpabuf_len(resp))) {
		wpa_printf(MSG_DEBUG, "DSCP: Failed to send policy request to " MACSTR,
			   MAC2STR(sta->addr));
		wpabuf_free(resp);
		goto cleanup;
	}

	wpabuf_free(resp);

	if (used && more) {
		sta->dscp_state.offset = offset + used;
		sta->dscp_state.last_dialog_token = dialog_token;
		sta->dscp_state.pending_more = true;
	}

cleanup:
	for (size_t i = 0; i < ctx.num_query_policies; i++)
		os_free(ctx.query_policy[i]);
	os_free(ctx.query_policy);
	os_free(ctx.req_policy);
	return 0;
}

void update_policy_status(struct sta_info *sta, u8 policy_id, enum dscp_policy_status status)
{
	u8 i;

	if (!sta || !sta->policies)
		return;

	for (i = 0; i < sta->num_dscp_policies; i++) {
		if (sta->policies[i] && sta->policies[i]->policy_id == policy_id) {
			sta->policies[i]->status = status;

			if (status == DSCP_STATUS_SUCCESS) {
				wpa_printf(MSG_DEBUG, "DSCP: Policy %u marked as accepted", policy_id);
			} else {
				wpa_printf(MSG_DEBUG, "DSCP: Policy %u marked as rejected with status %u",
					   policy_id, status);
			}
			return;
		}
	}
	wpa_printf(MSG_DEBUG, "DSCP: Policy %u not found to update status", policy_id);
}

void invalidate_policy_by_id(struct sta_info *sta, u8 policy_id)
{
	u8 i;

	for (i = 0; i < sta->num_dscp_policies; i++) {
		if (sta->policies[i] && sta->policies[i]->policy_id == policy_id) {
			wpa_printf(MSG_DEBUG, "DSCP: Invalidating policy %u", policy_id);
			free_dscp_policy(sta->policies[i]);
			sta->policies[i] = NULL;
			break;
		}
	}
}

struct wpabuf *hostapd_build_dscp_policy_request_from_offset(struct hostapd_data *hapd,
							     struct sta_info *sta,
							     u8 dialog_token,
							     u8 reset,size_t offset,
							     size_t *next_offset,
							     bool *more)
{
	struct wpabuf *buf, *elem;
	size_t i, frame_len;

	buf = start_new_dscp_frame(hapd, sta, dialog_token, reset, 0);
	if (!buf)
		return NULL;

	frame_len = wpabuf_len(buf);
	*more = false;

	for (i = offset; i < sta->num_dscp_policies; i++) {
		struct hostapd_dscp_policy *policy = sta->policies[i];
		if (!policy)
			continue;

		elem = hostapd_build_qos_element(policy);
		if (!elem)
			continue;

		if (frame_len + 2 + wpabuf_len(elem) > MAX_DSCP_REQ_SIZE) {
			wpabuf_free(elem);
			*more = true;
			break;
		}

		wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
		wpabuf_put_u8(buf, wpabuf_len(elem));
		wpabuf_put_buf(buf, elem);
		wpabuf_free(elem);
		frame_len = wpabuf_len(buf);
	}

	if (*more) {
		u8 *buf_ptr = wpabuf_mhead_u8(buf);
		buf_ptr[7] |= 0x01;
	}

	if (next_offset)
		*next_offset = i;

	return buf;
}

void hostapd_send_next_dscp_policy_batch(struct hostapd_data *hapd,
					 struct sta_info *sta,
					 size_t start_offset)
{
	struct wpabuf *frame;
	size_t next_offset = 0;
	u8 dialog_token;
	bool more = false;

	if (!hapd || !sta || !sta->policies || sta->num_dscp_policies == 0)
		return;

	if (!sta->dscp_policy_capable)
		return;

	dialog_token = get_next_unsolicited_dialog_token(sta);

	frame = hostapd_build_dscp_policy_request_from_offset(hapd, sta,
							      dialog_token,
							      0, start_offset,
							      &next_offset, &more);
	if (!frame)
		return;

	if (hostapd_drv_send_action(hapd, hapd->iface->freq, 0,
				    sta->addr, wpabuf_head(frame),
				    wpabuf_len(frame)) < 0) {
		wpa_printf(MSG_WARNING, "DSCP: Failed to send next policy frame to " MACSTR,
			   MAC2STR(sta->addr));
	}

	wpabuf_free(frame);

	sta->dscp_state.offset = next_offset;
	sta->dscp_state.last_dialog_token = dialog_token;
	sta->dscp_state.pending_more = more;
}

/* Handle DSCP Policy Response frame */
int hostapd_handle_dscp_policy_response(struct hostapd_data *hapd, struct sta_info *sta,
					const u8 *data, size_t len)
{
	const u8 *pos = data, *end = data + len;
	u8 dialog_token;
	u8 response_control;
	u8 count = 0;

	if (!hapd->conf->enable_dscp_policy_capa)
		return -1;

	if (!sta->dscp_policy_capable) {
		wpa_printf(MSG_DEBUG, "DSCP: STA " MACSTR " not capable",
			   MAC2STR(sta->addr));
		return -1;
	}

	if (len < 2)
		return -1;

	dialog_token = *pos++;
	response_control = *pos++;

	/* Handle reset case */
	if (response_control & 0x02)
		free_dscp_policies(sta);

	/* Unsolicited Response (Dialog Token 0) */
	if (dialog_token == 0 && (response_control & 0x02)) {
		wpa_printf(MSG_DEBUG, "DSCP: STA issued unsolicited reset");
		free_dscp_policies(sta);
		return 0;
	}

	if (pos < end)
		count = *pos++;

	/* Process status duples */
	while (count-- && pos + 2 <= end) {
		struct hostapd_dscp_policy *policy = NULL;
		u8 policy_id = *pos++;
		u8 status = *pos++;

		for (u8 i = 0; i < sta->num_dscp_policies; i++) {
			if (sta->policies[i] &&
			    sta->policies[i]->policy_id == policy_id) {
				policy = sta->policies[i];
				break;
			}
		}

		if (!policy) {
			wpa_printf(MSG_DEBUG, "DSCP: Unknown policy ID %u in response", policy_id);
			continue;
		}

		switch (status) {
		case DSCP_STATUS_SUCCESS:
			update_policy_status(sta, policy_id, status);
			break;
		case DSCP_STATUS_CLASSIFIER_NOT_SUPPORTED:
		case DSCP_STATUS_REQUEST_DECLINED:
		case DSCP_STATUS_INSUFFICIENT_RESOURCES:
			invalidate_policy_by_id(sta, policy_id);
			break;
		default:
			wpa_printf(MSG_WARNING, "DSCP: Unknown status %u for policy %u",
				   status, policy_id);
			break;
		}
	}

	/* If More bit is set, STA wants more policies */
	if ((response_control & 0x01) && sta->dscp_state.pending_more) {
		wpa_printf(MSG_DEBUG, "DSCP: STA requested additional policy batch");
		hostapd_send_next_dscp_policy_batch(hapd, sta,
						    sta->dscp_state.offset);
	}

	return 0;
}

