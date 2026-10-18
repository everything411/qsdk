/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>

#include <libsalrule/sal_rule.h>
#include <cfg80211_nlwrapper_api.h>
#include <qca_vendor.h>
#include <cfg80211_external.h>

#define SCS_ATTR_FLOW_LABEL_LENGTH           3

#define SCS_ATTR_CLASSIFIER_TYPE_TCLAS4      4
#define SCS_ATTR_CLASSIFIER_TYPE_TCLAS10     10

#define SCS_FILTER_ATTR_LEN_MIN              4
#define SCS_FILTER_ATTR_LEN_MAX              12
#define SCS_TCLAS10_SPI_SIZE                 4

/* Below macro to be in-sync with WLAN host driver side macro */
#define SAWF_SVC_CLASS_MAX                   128

enum scs_rule_config_request {
	SCS_RULE_CONFIG_REQUEST_TYPE_ADD,
	SCS_RULE_CONFIG_REQUEST_TYPE_REMOVE,

	WLAN_CFG80211_SAWF_RULE_ADD = 16,
	WLAN_CFG80211_SAWF_RULE_DELETE,
};

/*
 * Reuse TCLAS10 filter value attribute for src mac
 */
#define QCA_WLAN_VENDOR_ATTR_SAWF_RULE_SRC_MAC_ADDR \
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE

enum scs_spm_cmd {
	SCS_SPM_CMD_DELETE = 0,
	SCS_SPM_CMD_ADD = 1,
	SCS_SPM_CMD_QUERY = 2,
};

static int verbose;

#define PRINT_IF_VERB(fmt, ...) \
	do { \
		if (verbose) \
			printf(fmt "\n", ##__VA_ARGS__); \
	} while (0)

#define PRINT(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

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

static void scs_tool_nl_sal_cb(void *cbd, uint8_t *buf, size_t buf_len,
				char *ifname);

static struct sal_ctxt g_sal_ctxt;

static struct nl_ctxt g_nl_ctxt = {
	.scs_cb = {
		QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
			scs_tool_nl_sal_cb, &g_sal_ctxt
	},
};

static struct sal_ctxt *scs_tool_sal_init()
{
	struct sal_rule_socket *rs;

	rs = sal_rule_init();
	if (!rs)
		return NULL;

	memset(&rs->rule, 0, sizeof(sp_rule));

	g_sal_ctxt.rs = rs;

	return &g_sal_ctxt;
}

static void scs_tool_sal_exit(struct sal_ctxt *ctxt)
{
	if (ctxt && ctxt->rs) {
		sal_rule_deinit(ctxt->rs);
		ctxt->rs = NULL;
	}
}

static int scs_tool_send_sal_rule(struct sal_ctxt *ctxt)
{
	return sal_rule_config(ctxt->rs);
}

static void scs_tool_get_ipv4_addr(void *data, uint32_t *addr)
{
	memcpy(addr, data, sizeof(*addr));
}

static void scs_tool_get_ipv6_addr(void *data, struct in6_addr *addr)
{
	memcpy(addr, data, sizeof(*addr));
}

static void scs_tool_get_flow_label(void *data, uint8_t *flow_label)
{
	memcpy(flow_label, data, SCS_ATTR_FLOW_LABEL_LENGTH);
}

static void scs_tool_get_filter_attr(void *data, size_t len, uint32_t *attr_val)
{
	uint32_t val;
	uint8_t spi_idx;

	if (len < SCS_FILTER_ATTR_LEN_MIN || len > SCS_FILTER_ATTR_LEN_MAX) {
		PRINT_IF_VERB("Invalid attribute length %zu", len);
		return;
	}

	spi_idx = len - SCS_TCLAS10_SPI_SIZE;

	/*
	 * SCS classifier in ECM supports SPI rule match only.
	 * Copy MSB 4 bytes of Filter value and mask buffer for SPI
	 */
	memcpy(&val, data + spi_idx, sizeof(val));

	val = htonl(val);

	*attr_val = val;
}

static char *scs_tool_ipv4_str(uint32_t addr)
{
	struct in_addr ip_addr;

	ip_addr.s_addr = addr;
	return inet_ntoa(ip_addr);
}

static char *scs_tool_ipv6_str(struct in6_addr *addr)
{
	static char addr_buf[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, addr, addr_buf, INET6_ADDRSTRLEN);
	return addr_buf;
}

static void scs_tool_get_mac_addr(void *data, uint8_t *mac_addr)
{
	memcpy(mac_addr, data, ETH_ALEN);
}

static int
sawf_handle_rule_config_req(uint8_t *data, size_t len, struct sp_rule *rule)
{
	struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX + 1];
	struct nlattr *tb;
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;

	nla_parse(tb_array,
		  QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX,
		  (struct nlattr *)data, len, NULL);

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID];
	if (tb) {
		val32 = nla_get_u32(tb);
		PRINT_IF_VERB("Rule ID                 %u", val32);
		rule->attr_id = val32;
	} else {
		PRINT_IF_VERB("No Rule ID");
		return -EINVAL;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE];
	if (tb) {
		uint8_t spm_cmd;

		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("Request Type            %u", val8);

		switch (val8) {
		case WLAN_CFG80211_SAWF_RULE_ADD:
			spm_cmd = SCS_SPM_CMD_ADD;
			break;
		case WLAN_CFG80211_SAWF_RULE_DELETE:
			spm_cmd = SCS_SPM_CMD_DELETE;
			break;
		default:
			PRINT_IF_VERB("Invalid Request type %u", val8);
			return -EINVAL;
		}

		rule->add_del_rule = spm_cmd;
	} else {
		PRINT_IF_VERB("No Request Type");
		return -EINVAL;
	}

	if (rule->add_del_rule == SCS_SPM_CMD_DELETE)
		return 0;

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID];
	if (tb) {
		val16 = nla_get_u16(tb);
		rule->service_class_id = (uint8_t)val16;
		rule->flags |= SP_RULE_FLAG_MATCH_SERVICE_CLASS_ID;
		PRINT_IF_VERB("Service Class ID: %u", rule->service_class_id);
	} else {
		PRINT_IF_VERB("No service class");
		return -EINVAL;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SAWF_RULE_SRC_MAC_ADDR];
	if (tb) {
		scs_tool_get_mac_addr(nla_data(tb), rule->src_mac);
		rule->flags |= SP_RULE_FLAG_MATCH_SRC_MAC;
		PRINT_IF_VERB("Source MAC_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
				      rule->src_mac[0], rule->src_mac[1],
				      rule->src_mac[2], rule->src_mac[3],
				      rule->src_mac[4], rule->src_mac[5]);
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR];
	if (tb) {
		scs_tool_get_mac_addr(nla_data(tb), rule->dst_mac);
		rule->flags |= SP_RULE_FLAG_MATCH_DST_MAC;
		PRINT_IF_VERB("Destination MAC_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
			      rule->dst_mac[0], rule->dst_mac[1],
			      rule->dst_mac[2], rule->dst_mac[3],
			      rule->dst_mac[4], rule->dst_mac[5]);
	}

	return 0;
}

static int
scs_tool_fill_sal_rule(uint8_t *data, size_t len, struct sp_rule *rule,
		       uint8_t *req_type)
{
	struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX + 1];
	struct nlattr *tb;
	uint8_t val8;
	uint16_t val16;
	uint32_t val32;
	uint8_t fl[SCS_ATTR_FLOW_LABEL_LENGTH];
	uint8_t rule_precedence = 0;
	uint8_t scs_classifier_type;

	nla_parse(tb_array,
		  QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX,
		  (struct nlattr *)data, len, NULL);

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID];
	if (tb) {
		val32 = nla_get_u32(tb);
		PRINT_IF_VERB("Rule ID                 %u", val32);
		rule->attr_id = val32;
	} else {
		PRINT_IF_VERB("No Rule ID");
		return -EINVAL;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE];
	if (tb) {
		uint8_t spm_cmd;
		int sawf_req = 0;

		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("Request Type            %u", val8);

		switch (val8) {
		case SCS_RULE_CONFIG_REQUEST_TYPE_ADD:
			spm_cmd = SCS_SPM_CMD_ADD;
			break;
		case SCS_RULE_CONFIG_REQUEST_TYPE_REMOVE:
			spm_cmd = SCS_SPM_CMD_DELETE;
			break;
		case WLAN_CFG80211_SAWF_RULE_ADD:
		case WLAN_CFG80211_SAWF_RULE_DELETE:
			sawf_req = 1;
			break;
		default:
			PRINT_IF_VERB("Invalid Request type %u", val8);
			return -EINVAL;
		}

		*req_type = val8;

		if (sawf_req)
			return sawf_handle_rule_config_req(data, len, rule);

		rule->add_del_rule = spm_cmd;

		if (rule->add_del_rule == SCS_SPM_CMD_DELETE)
			goto done;
	} else {
		PRINT_IF_VERB("No Request Type");
		return -EINVAL;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID];
	if (tb) {
		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("TID                     %u", val8);
		rule->flags |= SP_RULE_FLAG_MATCH_RULE_OUTPUT;
		rule->rule_output = val8;
	} else {
		PRINT_IF_VERB("No TID");
		return -EINVAL;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE];
	if (tb) {
		val8 = nla_get_u8(tb);
		if (val8 != SCS_ATTR_CLASSIFIER_TYPE_TCLAS4 &&
		    val8 != SCS_ATTR_CLASSIFIER_TYPE_TCLAS10 &&
		    val8 != (SCS_ATTR_CLASSIFIER_TYPE_TCLAS4 |
			    SCS_ATTR_CLASSIFIER_TYPE_TCLAS10)) {
			PRINT_IF_VERB("Invalid SCS Classifier Type %u", val8);
			return -EINVAL;
		}
		PRINT_IF_VERB("SCS Classifier Type     %u", val8);
		scs_classifier_type = val8;
		if (scs_classifier_type == SCS_ATTR_CLASSIFIER_TYPE_TCLAS10)
			goto parse_tclas10;
	} else {
		PRINT_IF_VERB("No SCS classifier Type");
		return -EINVAL;
	}

	/*
	 * Parse tclas4 attributes
	 */
	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION];
	if (tb) {
		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("IP Version              %u", val8);
		rule->ip_version_type = val8;
		rule->flags |= SP_RULE_FLAG_MATCH_IP_VERSION_TYPE;
		rule_precedence++;
	} else {
		PRINT_IF_VERB("No IP Version");
		return -EINVAL;
	}

	tb = tb_array[
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR];
	if (tb) {
		scs_tool_get_ipv4_addr(nla_data(tb), &rule->src_ipv4_addr);
		rule->flags |= SP_RULE_FLAG_MATCH_SRC_IPV4;
		PRINT_IF_VERB("Src IPv4                %s",
			      scs_tool_ipv4_str(rule->src_ipv4_addr));
		rule_precedence++;
	}

	tb = tb_array[
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR];
	if (tb) {
		scs_tool_get_ipv4_addr(nla_data(tb), &rule->dst_ipv4_addr);
		rule->flags |= SP_RULE_FLAG_MATCH_DST_IPV4;
		PRINT_IF_VERB("Dst IPv4                %s",
			      scs_tool_ipv4_str(rule->dst_ipv4_addr));
		rule_precedence++;
	}

	tb = tb_array[
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR];
	if (tb) {
		scs_tool_get_ipv6_addr(nla_data(tb), &rule->src_ipv6_addr);
		rule->flags |= SP_RULE_FLAG_MATCH_SRC_IPV6;
		PRINT_IF_VERB("Src IPv6                %s",
			      scs_tool_ipv6_str(&rule->src_ipv6_addr));
		rule_precedence++;
	}

	tb = tb_array[
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR];
	if (tb) {
		scs_tool_get_ipv6_addr(nla_data(tb), &rule->dst_ipv6_addr);
		rule->flags |= SP_RULE_FLAG_MATCH_DST_IPV6;
		PRINT_IF_VERB("Dst IPv6                %s",
			      scs_tool_ipv6_str(&rule->dst_ipv6_addr));
		rule_precedence++;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT];
	if (tb) {
		val16 = nla_get_u16(tb);
		PRINT_IF_VERB("Source Port             %u", val16);
		rule->flags |= SP_RULE_FLAG_MATCH_SRC_PORT;
		rule->src_port = val16;
		rule_precedence++;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT];
	if (tb) {
		val16 = nla_get_u16(tb);
		PRINT_IF_VERB("Destination Port        %u", val16);
		rule->flags |= SP_RULE_FLAG_MATCH_DST_PORT;
		rule->dst_port = val16;
		rule_precedence++;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP];
	if (tb) {
		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("DSCP                    %u", val8);
		rule->dscp = val8;
		rule->flags |= SP_RULE_FLAG_MATCH_DSCP;
		rule_precedence++;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER];
	if (tb) {
		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("Next Header / Protocol  %u", val8);
		rule->proto_num = val8;
		rule->flags |= SP_RULE_FLAG_MATCH_PROTO_NUM;
		rule_precedence++;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL];
	if (tb) {
		scs_tool_get_flow_label(nla_data(tb), fl);
		PRINT_IF_VERB("Flow Label: %x %x %x", fl[0], fl[1], fl[2]);
		rule_precedence++;
	}

parse_tclas10:
	/*
	 * Parse tclas10 attributes
	 */
	tb = tb_array[
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE];
	if (tb) {
		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("Protocol Instance       %u", val8);
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER];
	if (tb) {
		val8 = nla_get_u8(tb);
		PRINT_IF_VERB("Next Header / Protocol  %u", val8);
		rule->proto_num = val8;
		rule->flags |= SP_RULE_FLAG_MATCH_PROTO_NUM;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK];
	if (tb) {
		scs_tool_get_filter_attr(nla_data(tb), nla_len(tb),
					 &rule->filter_mask);
		rule->flags |= SP_RULE_FLAG_MATCH_SPI;
		PRINT_IF_VERB("Filter Mask: %x", rule->filter_mask);
	}

	tb = tb_array[
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE];
	if (tb) {
		scs_tool_get_filter_attr(nla_data(tb), nla_len(tb),
					 &rule->filter_value);
		rule->flags |= SP_RULE_FLAG_MATCH_SPI;
		PRINT_IF_VERB("Filter Value: %x", rule->filter_value);
	}

	/*
	 * All the attributes of TCLAS10 are mandatory,
	 * so set precedence to 1 for all TCLAS10 rules.
	 */
	if ((scs_classifier_type & SCS_ATTR_CLASSIFIER_TYPE_TCLAS10) ==
			SCS_ATTR_CLASSIFIER_TYPE_TCLAS10)
		rule_precedence++;

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR];
	if (tb) {
		scs_tool_get_mac_addr(nla_data(tb), rule->dst_mac);
		rule->flags |= SP_RULE_FLAG_MATCH_DST_MAC;
		PRINT_IF_VERB("Destination MAC_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
			      rule->dst_mac[0], rule->dst_mac[1],
			      rule->dst_mac[2], rule->dst_mac[3],
			      rule->dst_mac[4], rule->dst_mac[5]);
		rule_precedence++;
	}

	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX];
	if (tb) {
		val32 = nla_get_u32(tb);
		PRINT_IF_VERB("Netdev Interface Index: %u", val32);
		rule->ifindex = (uint8_t)val32;
		rule->flags |= SP_RULE_FLAG_MATCH_IFINDEX;
		rule_precedence++;
	}

	PRINT_IF_VERB("Rule Precedence:                %u", rule_precedence);
	rule->precedence = rule_precedence;
	rule->flags |= SP_RULE_FLAG_MATCH_PRECEDENCE;

done:
	tb = tb_array[QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID];
	if (tb) {
		val16 = nla_get_u16(tb);
		rule->service_class_id = (uint8_t)val16;
		rule->flags |= SP_RULE_FLAG_MATCH_SERVICE_CLASS_ID;

		/* Classifier type based on svc class id range */
		if (rule->service_class_id <= SAWF_SVC_CLASS_MAX)
			rule->classifier_type = SPM_RULE_TYPE_SAWF_SCS;
		else
			rule->classifier_type = SPM_RULE_TYPE_SCS;

		PRINT_IF_VERB("Service Class ID: %u", rule->service_class_id);
	} else {
		rule->classifier_type = SPM_RULE_TYPE_SCS;
	}

	PRINT_IF_VERB("Rule Type: %u", rule->classifier_type);
	rule->flags |= SP_RULE_FLAG_MATCH_CLASSIFIER_TYPE;

	return 0;
}

static int32_t
scs_tool_nl_prepare_response(struct nl_msg *nlmsg, uint32_t rule_id)
{
	if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID,
	    rule_id))
		return -EIO;

	return 0;
}

static void scs_tool_nl_sal_cb(void *cbd, uint8_t *data, size_t len,
				char *ifname)
{
	int r;
	uint32_t rul_ret = 0;
	uint8_t req_type = 0;
	int ret = 0;

	struct nl_msg *nlmsg = NULL;
	struct sal_ctxt *ctxt = (struct sal_ctxt *)cbd;
	struct nlattr *nl_ven_data = NULL;

	if (!ctxt->rs->sk)
		return;

	PRINT_IF_VERB(".......... SCS EVENT RECEIVED .........\n");

	memset(&ctxt->rs->rule, 0, sizeof(struct sp_rule));
	r = scs_tool_fill_sal_rule(data, len, &ctxt->rs->rule, &req_type);
	if (r < 0) {
		PRINT_IF_VERB("Unable to set SPM rule");
		return;
	}

	rul_ret = scs_tool_send_sal_rule(ctxt);

	/* Send NL msg if rule population failed and request type is ADD */
	if (rul_ret && (req_type == SCS_RULE_CONFIG_REQUEST_TYPE_ADD)) {
		nlmsg = wifi_cfg80211_prepare_command(&g_nl_ctxt.cfg80211_ctxt,
						      QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
						      ifname);
		if (nlmsg) {
			nl_ven_data = (struct nlattr *)start_vendor_data(nlmsg);
			if (!nl_ven_data) {
				PRINT_IF_VERB("Failed to get vender data\n");
				nlmsg_free(nlmsg);
				return;
			}

			ret = scs_tool_nl_prepare_response(nlmsg, rul_ret);
			if (ret) {
				PRINT_IF_VERB("Failed to prepare message\n");
				nlmsg_free(nlmsg);
				return;
			}
			end_vendor_data(nlmsg, nl_ven_data);
			ret = send_nlmsg(&g_nl_ctxt.cfg80211_ctxt, nlmsg, NULL);
			if (ret < 0)
				PRINT_IF_VERB("Couldn't send NL command, ret = %d\n", ret);
		} else {
			PRINT_IF_VERB("Unable to prepare NL command!\n");
		}
	}
}

static void
scs_tool_dispatch_nl_event(struct nl_ctxt *ctxt, uint32_t cmdid,
			   uint8_t *data, size_t len, char *ifname)
{
	if (cmdid == QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG)
		ctxt->scs_cb.cb(ctxt->scs_cb.cbd, data, len, ifname);
}

static void
scs_tool_qca_nl_cb(char *ifname, uint32_t cmdid, uint8_t *data, size_t len)
{
	scs_tool_dispatch_nl_event(&g_nl_ctxt, cmdid, data, len, ifname);
}

static struct nl_ctxt *scs_tool_nl_init(void)
{
	g_nl_ctxt.cfg80211_ctxt.event_callback = scs_tool_qca_nl_cb;

	wifi_init_nl80211(&g_nl_ctxt.cfg80211_ctxt);

	wifi_nl80211_start_event_thread(&g_nl_ctxt.cfg80211_ctxt);

	return &g_nl_ctxt;
}

static void scs_tool_nl_exit(struct nl_ctxt *ctxt)
{
	wifi_destroy_nl80211(&g_nl_ctxt.cfg80211_ctxt);
}

static const char *opt_string = "vh?";
static const struct option long_opts[] = {
	{ "verbose", no_argument, NULL, 'v' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, no_argument, NULL, 0 },
};

static void display_help(void)
{
	printf("\nscs_tool : Receive SCS config parameters\n"
	       "and updates SPM rule database\n");
	printf("\nUsage:\n"
	       "scs_tool [-v][-h | ?]\n"
	       "-v or verbose\n"
	       "    Prints verbose logs\n");
}

int main(int argc, char *argv[])
{
	struct sal_ctxt *sal_ctxt = NULL;
	struct nl_ctxt *nl_ctxt = NULL;
	int c;
	int long_index = 0;

	c = getopt_long(argc, argv, opt_string, long_opts, &long_index);

	while (c != -1) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'h':
		case '?':
			display_help();
			return 0;
		default:
			printf("Unrecognized option %c\n", c);
			display_help();
			return -EINVAL;
		}
		c = getopt_long(argc, argv, opt_string, long_opts, &long_index);
	}

	sal_ctxt = scs_tool_sal_init();
	if (!sal_ctxt) {
		PRINT("Unable to create SAL context");
		goto fail1;
	}

	nl_ctxt = scs_tool_nl_init();
	if (!nl_ctxt) {
		PRINT("Unable to create NL receive context");
		goto fail2;
	}

	while (1)
		usleep(1000);
fail2:
	if (nl_ctxt)
		scs_tool_nl_exit(nl_ctxt);
fail1:
	if (sal_ctxt)
		scs_tool_sal_exit(sal_ctxt);
	return 0;
}
