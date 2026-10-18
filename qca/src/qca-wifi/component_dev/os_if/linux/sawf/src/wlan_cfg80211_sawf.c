/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <qdf_trace.h>
#include <qdf_types.h>
#include <wlan_cfg80211.h>
#include <wlan_cfg80211_sawf.h>

#define SAWF_CFG80211_BUF_LEN 4000

/*
 * Reuse tclas10 filter value for sawf request
 */
#define QCA_WLAN_VENDOR_ATTR_SAWF_RULE_SRC_MAC_ADDR \
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE

void wlan_cfg80211_sawf_send_rule(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  struct wlan_sawf_rule *rule)
{
	struct sk_buff *skb;

	skb = wlan_cfg80211_vendor_event_alloc
		(wiphy, wdev, SAWF_CFG80211_BUF_LEN,
		 QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG_INDEX, GFP_ATOMIC);

	if (!skb) {
		qdf_err("Unable allocate vendor event");
		return;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID,
			rule->rule_id)) {
		qdf_err("nla_put fail for rule_id.");
		goto fail;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE,
		       rule->req_type)) {
		qdf_err("nla_put fail for request_type.");
		goto fail;
	}

	if (rule->req_type == WLAN_CFG80211_SAWF_RULE_DELETE) {
		wlan_cfg80211_vendor_event(skb, GFP_ATOMIC);
		return;
	}

	if (nla_put_u16(skb,
			QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID,
			rule->service_class_id)) {
		qdf_err("nla_put fail for service_class_id");
		goto fail;
	}

	if (qdf_test_bit(SAWF_RULE_IN_PARAM_DST_MAC, rule->param_bitmap)) {
		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR,
			    QDF_MAC_ADDR_SIZE, rule->dst_mac)) {
			qdf_err("nla_put fail for dst_mac_addr.") ;
			goto fail;
		}
	}

	if (qdf_test_bit(SAWF_RULE_IN_PARAM_SRC_MAC, rule->param_bitmap)) {
		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_SAWF_RULE_SRC_MAC_ADDR,
			    QDF_MAC_ADDR_SIZE, rule->src_mac)) {
			qdf_err("nla_put fail for src_mac_addr.") ;
			goto fail;
		}
	}

	wlan_cfg80211_vendor_event(skb, GFP_ATOMIC);

	return;
fail:
	wlan_cfg80211_vendor_free_skb(skb);
}
