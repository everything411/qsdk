/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */
#include <linux/version.h>
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/etherdevice.h>

#define DEBUG_LEVEL ECM_SDX_PCC_DEBUG_LEVEL

#include "ecm_classifier_pcc_public.h"
#include "ecm_types.h"

/*
 * This is a SDX PCC external module for the ECM's PCC Classifier.
 * When userspace writes to proc sys, kernel space (this module)
 * adds/deletes the hw mac filter rule to/from its list.
 *
 * Case 1: No HW MAC Filter rules in the list
 * By default, permit acceleration for all connections
 *
 * Case 2: Set HW MAC Filtering - before data transfer/connection established
 * Write each HW MAC Filter rules to the proc sys and add rules to the list.
 * When a connection is established, the connection instance info
 * 7-tuple will be checked against the ecm_sdx_rules to permit/deny
 * acceleration.
 *
 * Case 3: Set HW MAC Filtering - after start data transfer / connection established
 * Connection is established on a specific mac address/ip segment range/iface.
 * Write each HW MAC Filter rules to the proc sys and add rules to the list.
 * Defunct connection by specific mac address/ip segment range/iface
 * New connection instance instance is created and connection info
 * 7-tuple will be checked against the ecm_sdx_rules to permit/deny
 * acceleration.
 */

/*
 * Number of fields in rule write
 */
#define ECM_SDX_PCC_RULE_FIELDS 7

/*
 * Size of the rule name
 */
#define ECM_SDX_PCC_RULE_NAME_SIZE 50

/*
 * Size of read buf
 */
#define ECM_SDX_PCC_READ_BUF_SIZE 4096

/*
 * MAC address string format
 */
#define ECM_SDX_PCC_MAC_FMT "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx"

/*
 * IPv4 address string format
 */
#define ECM_SDX_PCC_IP_ADDR_DOT_FMT "%u.%u.%u.%u"

/*
 * uint32_t IPv4 address to dot
 */
#define ECM_SDX_PCC_IP_ADDR_TO_DOT(ip_addr) ((ip_addr >> 24) & 0xFF), ((ip_addr >> 16) & 0xFF), ((ip_addr >> 8) & 0xFF), (ip_addr & 0xFF)

/*
 * Sysctl table header
 */
static struct ctl_table_header *ecm_sdx_pcc_ctl_tbl_hdr;

/*
 * Registration
 */
struct ecm_classifier_pcc_registrant *ecm_sdx_pcc_registrant = NULL;

/*
 * ecm_sdx_pcc_rule_param
 * 	Defines the type of parameters of a SDX PCC rule
 */
enum ecm_sdx_pcc_rule_param {
	ECM_SDX_PCC_RULE_PARAM_NONE,
	ECM_SDX_PCC_RULE_PARAM_MAC_ADDR,	/* mac address rule param */
	ECM_SDX_PCC_RULE_PARAM_IP_ADDR,		/* ip segment range rule param */
	ECM_SDX_PCC_RULE_PARAM_IFACE		/* iface rule param */
};
typedef enum ecm_sdx_pcc_rule_param ecm_sdx_pcc_rule_param_t;

/*
 * ecm_sdx_pcc_rule_action
 * 	Defines set of action that can be performed for a SDX PCC rule
 */
enum ecm_sdx_pcc_rule_action {
	ECM_SDX_PCC_RULE_ACTION_DELETE,		/* action to delete rule */
	ECM_SDX_PCC_RULE_ACTION_ADD,		/* action to add rule */
	ECM_SDX_PCC_RULE_ACTION_CLEAR		/* action to clear rule(s) */
};
typedef enum ecm_sdx_pcc_rule_action ecm_sdx_pcc_rule_action_t;

/*
 * ecm_sdx_pcc_ip_ver
 * 	Defines the ip version type
 */
enum ecm_sdx_pcc_ip_ver {
	ECM_SDX_PCC_IPV4 = 4,		/* ip version v4 */
	ECM_SDX_PCC_IPV6 = 6		/* ip version v6 */
};
typedef enum ecm_sdx_pcc_ip_ver ecm_sdx_pcc_ip_ver_t;

/*
 * Rule table
 */
struct ecm_sdx_pcc_rule {
	struct list_head list;
	char name[ECM_SDX_PCC_RULE_NAME_SIZE];	/* rule name */
	ecm_classifier_pcc_result_t accel;	/* permit or deny acceleration */
	uint8_t mac_addr[ETH_ALEN];		/* mac address */
	struct in6_addr ip_addr_start;		/* ip segment range start */
	struct in6_addr ip_addr_end; 		/* ip sengment range end */
	char iface[IFNAMSIZ];			/* iface */
};
LIST_HEAD(ecm_sdx_pcc_rules);
DEFINE_SPINLOCK(ecm_sdx_pcc_rules_lock);

/*
 * proc entry value to unregister ecm_sdx_pcc_registrant from ecm_classifier_pcc
 */
int ecm_sdx_pcc_unregister;

/*
 * ecm_sdx_pcc_registrant_ref()
 *	Invoked when an additional hold is kept upon the registrant
 */
static void ecm_sdx_pcc_registrant_ref(struct ecm_classifier_pcc_registrant *r)
{
	int remain;

	/*
	 * Increment the ref count by 1.
	 * This causes the registrant structure to remain in existance until
	 * released (deref).
	 * By definition the caller of this method has a hold on the registrant
	 * already so it cannot 'go away'.
	 * This is because either:
	 * 1. The caller itself has been passed it in a function parameter;
	 * 2. It has its own explicit hold.
	 */
	remain = atomic_inc_return(&r->ref_count);
	if (remain <= 0) {
		pr_debug("REFERENCE COUNT WRAP!\n");
	} else {
		pr_debug("ECM PCC Registrant ref: %d\n", remain);
	}
}

/*
 * ecm_sdx_pcc_registrant_deref()
 *	Caller is releasing its hold upon the registrant.
 */
static void ecm_sdx_pcc_registrant_deref(struct ecm_classifier_pcc_registrant *r)
{
	int remain;
	struct ecm_sdx_pcc_rule *tmp = NULL;
	struct list_head *pos, *q;

	/*
	 * Decrement the reference count
	 */
	remain = atomic_dec_return(&r->ref_count);
	if (remain > 0) {
		/*
		 * Something still holds a reference
		 */
		pr_debug("ECM PCC Registrant deref: %d\n", remain);
		return;
	}

	/*
	 * Last hold upon the registrant is released and so we can now
	 * destroy it.
	 */
	if (remain < 0) {
		pr_info("REFERENCE COUNT WRAP!\n");
	}

	spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	list_for_each_safe(pos, q, &ecm_sdx_pcc_rules) {
		tmp = list_entry(pos, struct ecm_sdx_pcc_rule, list);
		list_del(pos);
		kfree(tmp);
	}
	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);

	pr_info("ECM PCC Registrant DESTROYED\n");
	kfree(r);
}

/*
 * __ecm_sdx_pcc_rule_find_by_name()
 *	Find rule by rule name.
 *	Note: Caller has to take and release lock on this API.
 */
static struct ecm_sdx_pcc_rule *__ecm_sdx_pcc_rule_find_by_name(char *name)
{
	struct ecm_sdx_pcc_rule *rule = NULL;

	list_for_each_entry(rule , &ecm_sdx_pcc_rules, list) {
		if (strcmp(name, rule->name) == 0) {
			pr_debug("Rule name matches! name=%s, rule->name=%s\n", name, rule->name);
			return rule;
		}
	}
	pr_warn("name=%s does NOT match any rules!\n", name);
	return NULL;
}

/*
 * __ecm_sdx_pcc_check_mac_addr()
 *	Return true if mac_addr matches with the rule mac_addr
 */
static bool __ecm_sdx_pcc_check_mac_addr(uint8_t *mac_addr,
				struct ecm_sdx_pcc_rule *rule)
{
	if (!ether_addr_equal(mac_addr, rule->mac_addr) || is_zero_ether_addr(mac_addr) ||
				is_zero_ether_addr(rule->mac_addr)) {
		pr_warn("mac_addr and rule->mac_addr does NOT match or empty mac_addr/rule->mac_addr: mac_addr=%pM, rule->mac_addr=%pM\n", mac_addr, rule->mac_addr);
		return false;
	}

	pr_debug("found match: mac_addr=%pM, rule->mac_addr=%pM\n", mac_addr, rule->mac_addr);
	return true;
}

/*
 * __ecm_sdx_pcc_check_iface()
 *	Return true if iface matches with the rule iface
 */
static bool __ecm_sdx_pcc_check_iface(struct net_device *dev,
				struct ecm_sdx_pcc_rule *rule)
{
	if (!dev) {
		pr_warn("dev is null\n");
		return false;
	}

	if (strcmp(dev->name, rule->iface) == 0) {
		pr_debug("Device name matches! dev->name=%s, iface=%s\n", dev->name, rule->iface);
		return true;
	}

	pr_warn("Device name does NOT match! dev->name=%s, iface=%s\n", dev->name, rule->iface);
	return false;
}

/*
 * __ecm_sdx_pcc_check_ip_addr()
 *	Return true if ip_addr is within rule ip_addr range
 */
static bool __ecm_sdx_pcc_check_ip_addr(struct in6_addr *ip_addr,
				struct ecm_sdx_pcc_rule *rule)
{
	if (ipv6_addr_cmp(&rule->ip_addr_start, ip_addr) <= 0 && ipv6_addr_cmp(ip_addr, &rule->ip_addr_end) <= 0) {
		pr_debug("Compare ip range: start=%pI6, ip_addr=%pI6, end=%pI6\n", &rule->ip_addr_start, ip_addr, &rule->ip_addr_end);
		return true;
	}

	pr_warn("Not in ip range: start=%pI6, ip_addr=%pI6, end=%pI6\n", &rule->ip_addr_start, ip_addr, &rule->ip_addr_end);
	return false;
}

/*
 * __ecm_sdx_pcc_rule_find_mac_addr()
 *	Return true if rule is found for mac_addr
 */
static bool __ecm_sdx_pcc_rule_find_mac_addr(uint8_t *src_mac,
				uint8_t *dest_mac,
				struct ecm_sdx_pcc_rule *rule)
{
	pr_debug("Compare src_mac=%pM, rule->mac_addr=%pM\n", src_mac, rule->mac_addr);
	if (__ecm_sdx_pcc_check_mac_addr(src_mac, rule)) {
		return true;
	}

	pr_debug("Compare dest_mac=%pM, rule->mac_addr=%pM\n", dest_mac, rule->mac_addr);
	if (__ecm_sdx_pcc_check_mac_addr(dest_mac, rule)) {
		return true;
	}

	return false;
}

/*
 * __ecm_sdx_pcc_rule_find_iface()
 *	Return true if rule is found for iface
 */
static bool __ecm_sdx_pcc_rule_find_iface(struct net_device *in_dev,
				struct net_device *out_dev,
				struct ecm_sdx_pcc_rule *rule)
{
	pr_debug("Compare in_dev=%s, rule->iface=%s\n", in_dev->name, rule->iface);
	if (__ecm_sdx_pcc_check_iface(in_dev, rule)) {
		return true;
	}

	pr_debug("Compare out_dev=%s, rule->iface=%s\n", out_dev->name, rule->iface);
	if (__ecm_sdx_pcc_check_iface(out_dev, rule)) {
		return true;
	}

	return false;
}

/*
 * __ecm_sdx_pcc_rule_find_ip_addr()
 *	Return true if rule is found for ip_addr
 */
static bool __ecm_sdx_pcc_rule_find_ip_addr(struct in6_addr *src_addr,
				struct in6_addr *dest_addr,
				ecm_sdx_pcc_ip_ver_t ipv,
				struct ecm_sdx_pcc_rule *rule)
{
	if (ipv != ECM_SDX_PCC_IPV4) {
		return false;
	}

	/*
	 * Check if start or end address is unspecified (zero)
	 */
	if (ipv6_addr_any(&rule->ip_addr_start) || ipv6_addr_any(&rule->ip_addr_end)) {
		pr_warn("Skipping rule: IP range is not set (start=%pI6, end=%pI6)\n", &rule->ip_addr_start, &rule->ip_addr_end);
		return false;
	}

	/*
	 * Check src_addr is in range: ip_addr_start and ip_addr_end
	 */
	pr_debug("Compare ip range: start=%pI6, src=%pI6, end=%pI6\n", &rule->ip_addr_start, src_addr, &rule->ip_addr_end);
	if (__ecm_sdx_pcc_check_ip_addr(src_addr, rule)) {
		return true;
	}

	/*
	 * Check dest_addr is in range: ip_addr_start and ip_addr_end
	 */
	pr_debug("Compare ip range: start=%pI6, dest=%pI6, end=%pI6\n", &rule->ip_addr_start, dest_addr, &rule->ip_addr_end);
	if (__ecm_sdx_pcc_check_ip_addr(dest_addr, rule)) {
		return true;
	}

	return false;
}

/*
 * __ecm_sdx_pcc_rule_find()
 *	Return matching rule
 */
static struct ecm_sdx_pcc_rule *__ecm_sdx_pcc_rule_find(uint8_t *src_mac,
				uint8_t *dest_mac,
				struct in6_addr *src_addr,
				struct in6_addr *dest_addr,
				struct ecm_classifier_pcc_info *cinfo,
				ecm_sdx_pcc_ip_ver_t ipv)
{
	struct ecm_sdx_pcc_rule *rule = NULL;
	struct net_device *in_dev = cinfo->input_params.dev_info.in_dev;
	struct net_device *out_dev = cinfo->input_params.dev_info.out_dev;

	list_for_each_entry(rule, &ecm_sdx_pcc_rules, list) {
		/*
		 * Check mac address
		 */
		if (__ecm_sdx_pcc_rule_find_mac_addr(src_mac, dest_mac, rule)) {
			return rule;
		}

		/*
		 * Check iface
		 */
		if (__ecm_sdx_pcc_rule_find_iface(in_dev, out_dev, rule)) {
			return rule;
		}

		/*
		 * Check ipv4 address
		 */
		if (__ecm_sdx_pcc_rule_find_ip_addr(src_addr, dest_addr, ipv, rule)) {
			return rule;
		}
	}

	return NULL;
}

/*
 * ecm_sdx_pcc_get_accel_info_v4()
 *	Invoked by the ECM to query if the given connection may be accelerated
 *	and to get the set of features to be enabled on it.
 */
static ecm_classifier_pcc_result_t
ecm_sdx_pcc_get_accel_info_v4(struct ecm_classifier_pcc_registrant *r,
			      uint8_t *src_mac, __be32 src_ip, int src_port,
			      uint8_t *dest_mac, __be32 dest_ip, int dest_port,
			      int protocol, struct ecm_classifier_pcc_info *cinfo)
{
	struct ecm_sdx_pcc_rule *rule;
	ecm_classifier_pcc_result_t accel;
	struct in6_addr src_addr = IN6ADDR_ANY_INIT;
	struct in6_addr dest_addr = IN6ADDR_ANY_INIT;
	ecm_sdx_pcc_ip_ver_t ipv = ECM_SDX_PCC_IPV4;

	src_addr.s6_addr32[0] = src_ip;
	dest_addr.s6_addr32[0] = dest_ip;

	if (!cinfo) {
		pr_warn("Invalid input parameter\n");
		return ECM_CLASSIFIER_PCC_RESULT_NOT_YET;
	}

	spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	rule = __ecm_sdx_pcc_rule_find(src_mac, dest_mac, &src_addr, &dest_addr,
				cinfo, ipv);
	if (!rule) {
		spin_unlock_bh(&ecm_sdx_pcc_rules_lock);
		pr_warn("Rule not found\n");
		return ECM_CLASSIFIER_PCC_RESULT_PERMITTED;
	}
	accel = rule->accel;
	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);

	return accel;
}

/*
 * ecm_sdx_pcc_get_accel_info_v6()
 *	Invoked by the ECM to query if the given connection may be accelerated
 *	and to get the set of features to be enabled on it.
 */
static ecm_classifier_pcc_result_t
ecm_sdx_pcc_get_accel_info_v6(struct ecm_classifier_pcc_registrant *r,
			      uint8_t *src_mac, struct in6_addr *src_addr,
			      int src_port, uint8_t *dest_mac,
			      struct in6_addr *dest_addr, int dest_port,
			      int protocol, struct ecm_classifier_pcc_info *cinfo)
{
	struct ecm_sdx_pcc_rule *rule;
	ecm_classifier_pcc_result_t accel;
	ecm_sdx_pcc_ip_ver_t ipv = ECM_SDX_PCC_IPV6;

	if (!cinfo) {
		pr_warn("Invalid input parameter\n");
		return ECM_CLASSIFIER_PCC_RESULT_NOT_YET;
	}

	spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	rule = __ecm_sdx_pcc_rule_find(src_mac, dest_mac, src_addr, dest_addr,
				cinfo, ipv);
	if (!rule) {
		spin_unlock_bh(&ecm_sdx_pcc_rules_lock);
		pr_warn("Rule not found\n");
		return ECM_CLASSIFIER_PCC_RESULT_PERMITTED;
	}
	accel = rule->accel;
	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);

	return accel;
}

/*
 * ecm_sdx_pcc_str_to_ip()
 *	Convert string IP to in6_addr.  Return 4 for IPv4 or 6 for IPv6.
	Return 0 for error.
 *
 * NOTE: When string is IPv4 the lower 32 bit word of the in6 address
 * contains the address. Network order.
 */
static unsigned int ecm_sdx_pcc_str_to_ip(char *ip_str, struct in6_addr *addr)
{
	uint8_t *ptr = (uint8_t *)(addr->s6_addr);

	/*
	 * IPv4 address in addr->s6_addr32[0]
	 */
	if (in4_pton(ip_str, -1, ptr, '\0', NULL) > 0) {
		return 4;
	}

	/*
	 * IPv6
	 */
	if (in6_pton(ip_str, -1, ptr, '\0', NULL) > 0) {
		return 6;
	}

	return 0;
}

/*
 * ecm_sdx_pcc_defunct_connections()
 *	defunct connection by mac_addr, ip_addr, iface (if there is a connection established)
 *	after connection is defunct, a new connection instance is created
 *	and the connection instance info 7-tuple is checked against the rule,
 *	if there is no match then permit accel
*/
static void ecm_sdx_pcc_defunct_connections(uint8_t *mac_addr,
					struct in6_addr *ip_addr_start,
					struct in6_addr *ip_addr_end,
					char *iface)
{
	struct net_device *dev;

	/*
	 * defunct by mac addr
	 */
	if (!is_zero_ether_addr(mac_addr)) {
		pr_debug("decel mac_addr=%pM", mac_addr);
		ecm_classifier_pcc_decel_by_mac_addr(mac_addr);
		return;
	}

	/*
	 * defunct by ip addr
	 */
	pr_debug("ip_addr_start=0x%x, ip_addr_end=0x%x\n", ip_addr_start->s6_addr32[0], ip_addr_end->s6_addr32[0]);
	if (!ipv6_addr_any(ip_addr_start) && !ipv6_addr_any(ip_addr_end)) {
		pr_debug("decel ip segment range addresses\n");
		for (__be32 ip_addr = ntohl(ip_addr_start->s6_addr32[0]); ip_addr <= ntohl(ip_addr_end->s6_addr32[0]); ip_addr++) {
			pr_debug("decel ip_addr(0x%x)=" ECM_SDX_PCC_IP_ADDR_DOT_FMT "\n", ip_addr, ECM_SDX_PCC_IP_ADDR_TO_DOT(ip_addr));
			ecm_classifier_pcc_decel_by_ip_addr_v4(htonl(ip_addr));
		}
		return;
	}

	/*
	 * defunct by iface
	 */
	if (strlen(iface) > 0) {
		pr_debug("decel iface=%s", iface);
		dev = dev_get_by_name(&init_net, iface);
		if (dev) {
			pr_debug("dev:ifindex=%d, name=%s", dev->ifindex, dev->name);
			ecm_classifier_pcc_decel_by_dev(dev);
			dev_put(dev);
		}
		return;
	}
}

/*
 * ecm_sdx_pcc_delete_rule()
 *	Delete a rule, return true for success.
 */
static bool ecm_sdx_pcc_delete_rule(char *name)
{
	struct ecm_sdx_pcc_rule *rule = NULL;
	uint8_t mac_addr[ETH_ALEN];
	struct in6_addr ip_addr_start = IN6ADDR_ANY_INIT;
	struct in6_addr ip_addr_end = IN6ADDR_ANY_INIT;
	char iface[IFNAMSIZ] = {0};

	/*
	 * get rule by name
	 */
	spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	rule = __ecm_sdx_pcc_rule_find_by_name(name);
	if (!rule) {
		spin_unlock_bh(&ecm_sdx_pcc_rules_lock);
		pr_warn("Cannot find rule with name=%s\n", name);
		return false;
	}

	ether_addr_copy(mac_addr, rule->mac_addr);
	ip_addr_start = rule->ip_addr_start;
	ip_addr_end = rule->ip_addr_end;
	strscpy(iface, rule->iface, sizeof(iface));

	list_del(&rule->list);
	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);
	kfree(rule);

	pr_debug("deleted rule from ecm_sdx_pcc_rules list\n");

	/*
	 * defunct connections
	 */
	ecm_sdx_pcc_defunct_connections(mac_addr, &ip_addr_start, &ip_addr_end, iface);

	return true;
}

/*
 * ecm_sdx_pcc_add_rule()
 *	Add a new rule, return true for success. Given accel the ecm is informed
 *	of permit/deny accel status.
 */
static bool ecm_sdx_pcc_add_rule(char *name,
				ecm_classifier_pcc_result_t accel,
				uint8_t *mac_addr,
				struct in6_addr *ip_addr_start,
				struct in6_addr *ip_addr_end,
				char *iface)
{
	struct ecm_sdx_pcc_rule *new_rule;

	new_rule = kzalloc(sizeof(struct ecm_sdx_pcc_rule), GFP_ATOMIC);
	if (!new_rule)
		return false;

	strscpy(new_rule->name, name, sizeof(new_rule->name));
	new_rule->accel = accel;
	ether_addr_copy(new_rule->mac_addr, mac_addr);
	new_rule->ip_addr_start = *ip_addr_start;
	new_rule->ip_addr_end = *ip_addr_end;
	strscpy(new_rule->iface, iface, IFNAMSIZ);
	INIT_LIST_HEAD(&new_rule->list);

	spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	list_add(&new_rule->list, &ecm_sdx_pcc_rules);
	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);

	pr_debug("added rule to ecm_sdx_pcc_rules list\n");

	/*
	 * defunct connections
	 */
	ecm_sdx_pcc_defunct_connections(mac_addr, ip_addr_start, ip_addr_end, iface);

	return true;
}

/*
 * ecm_sdx_pcc_clear_rules()
 *	Clear all rules, return true for success.
 */
static bool ecm_sdx_pcc_clear_rules(void)
{
	struct ecm_sdx_pcc_rule *rule = NULL;
	struct ecm_sdx_pcc_rule *tmp = NULL;
	uint8_t mac_addr[ETH_ALEN];
	struct in6_addr ip_addr_start = IN6ADDR_ANY_INIT;
	struct in6_addr ip_addr_end = IN6ADDR_ANY_INIT;
	char iface[IFNAMSIZ] = {0};

	spin_lock_bh(&ecm_sdx_pcc_rules_lock);

	list_for_each_entry_safe(rule, tmp, &ecm_sdx_pcc_rules, list) {
		ether_addr_copy(mac_addr, rule->mac_addr);
		ip_addr_start = rule->ip_addr_start;
		ip_addr_end = rule->ip_addr_end;
		strscpy(iface, rule->iface, sizeof(iface));

		list_del(&rule->list);
		spin_unlock_bh(&ecm_sdx_pcc_rules_lock);
		kfree(rule);

		/*
		 * defunct connections
		 */
		ecm_sdx_pcc_defunct_connections(mac_addr, &ip_addr_start, &ip_addr_end, iface);

		spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	}

    	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);

	pr_debug("cleared all rules from ecm_sdx_pcc_rules list\n");

	return true;
}

/*
 * ecm_sdx_pcc_rule_write()
 *	Write a rule
 */
static int ecm_sdx_pcc_rule_write(void *buffer, size_t *lenp, loff_t *ppos)
{
	int count = 0;
	int ipv = 0;
	char *rule_buf;
	int field_count;
	char *field_ptr;
	char *fields[ECM_SDX_PCC_RULE_FIELDS];
	char name[50];
	unsigned int action;
	ecm_classifier_pcc_result_t accel;
	uint8_t mac_addr[ETH_ALEN];
	struct in6_addr ip_addr_start = IN6ADDR_ANY_INIT;
	struct in6_addr ip_addr_end = IN6ADDR_ANY_INIT;
	char iface[IFNAMSIZ] = {0};

	/*
	 * buf is formed as:
	 * [0]    [1]                 	    [2]                           [3]        [4]             [5]           [6]
	 * <name>/<0=delete,1=add,2=clear>/<1=accel_denied, 2=accel_permitted>/<mac_addr>/<ip_addr_start>/<ip_addr_end>/<iface>
	 * e.g.:
	 *
	 * Adding Rules
	 * mac_addr: echo "my_rule/1/1/00:1b:22:32:27:2b///" > /proc/sys/net/ecm/ecm_sdx_pcc_rule
	 * ip segment range: echo "my_rule2/1/1/00:00:00:00:00:00/192.168.224.100/192.168.224.110/" > /proc/sys/net/ecm/ecm_sdx_pcc_rule
	 * iface: echo "my_rule3/1/1/00:00:00:00:00:00///eth0" > /proc/sys/net/ecm/ecm_sdx_pcc_rule
	 * cat /proc/sys/net/ecm/ecm_sdx_pcc_rule (shows all rules)
	 *
	 * Deleting Rules (delete by rule name - provide the correct rule name)
	 * mac_addr: echo "my_rule/0/1/00:1b:22:32:27:2b/0/0/0" > /proc/sys/net/ecm/ecm_sdx_pcc_rule
	 *
	 * Clearing Rules
	 * echo "my_rule/2/1/00:00:00:00:00:00/0/0/0" > /proc/sys/net/ecm/ecm_sdx_pcc_rule
	 */

	count = *lenp;

	rule_buf = kzalloc(count * sizeof(char), GFP_KERNEL);
	if (!rule_buf)
		return -EINVAL;

	memcpy(rule_buf, buffer, count);

	/*
	 * Split the buffer into its fields
	 */
	field_count = 0;
	field_ptr = rule_buf;
	fields[field_count] = strsep(&field_ptr, "/");
	while (fields[field_count] != NULL) {
		pr_debug("Fields[%d]=%s\n", field_count, fields[field_count]);
		field_count++;
		if (field_count == ECM_SDX_PCC_RULE_FIELDS)
			break;

		fields[field_count] = strsep(&field_ptr, "/ \n");
	}

	if (field_count != ECM_SDX_PCC_RULE_FIELDS) {
		pr_warn("Invalid field count %d\n", field_count);
		goto fail;
	}

	/*
	 * Convert fields
	 */
	strscpy(name, fields[0], sizeof(name));

	if (sscanf(fields[1], "%u", &action) != 1) {
		pr_warn("sscanf read error\n");
		goto fail;
	}

	if (sscanf(fields[2], "%d", (int *)&accel) != 1) {
		pr_warn("sscanf read error\n");
		goto fail;
	}

	switch (accel) {
	case ECM_CLASSIFIER_PCC_RESULT_DENIED:
	case ECM_CLASSIFIER_PCC_RESULT_PERMITTED:
		break;
	default:
		pr_warn("Bad accel: %u\n", accel);
		goto fail;
	}

	if (sscanf(fields[3], ECM_SDX_PCC_MAC_FMT, mac_addr, mac_addr + 1, mac_addr + 2,
		mac_addr + 3, mac_addr + 4, mac_addr + 5) != 6) {
		pr_warn("sscanf read error\n");
		goto fail;
	}

	ipv = ecm_sdx_pcc_str_to_ip(fields[4], &ip_addr_start);
	if (ipv != ecm_sdx_pcc_str_to_ip(fields[5], &ip_addr_end)) {
		pr_warn("Conflicting IP address types\n");
		goto fail;
	}

	strscpy(iface, fields[6], sizeof(iface));

	kfree(rule_buf);

	pr_info("name: %s\n"
		"action: %u\n"
		"accel: %d\n"
		"mac_addr: %pM\n"
		"ip_addr_start: " ECM_SDX_PCC_IP_ADDR_DOT_FMT "\n"
		"ip_addr_end: " ECM_SDX_PCC_IP_ADDR_DOT_FMT "\n"
		"iface: %s\n",
		name,
		action,
		(int)accel,
		mac_addr,
		ECM_SDX_PCC_IP_ADDR_TO_DOT(ntohl(ip_addr_start.s6_addr32[0])),
		ECM_SDX_PCC_IP_ADDR_TO_DOT(ntohl(ip_addr_end.s6_addr32[0])),
		iface);

	switch(action) {
	case ECM_SDX_PCC_RULE_ACTION_DELETE:
		pr_debug("Delete\n");
		if (!ecm_sdx_pcc_delete_rule(name)) {
			return -EINVAL;
		}
		break;

	case ECM_SDX_PCC_RULE_ACTION_ADD:
		pr_debug("Add\n");
		if (!ecm_sdx_pcc_add_rule(name, accel, mac_addr, &ip_addr_start, &ip_addr_end, iface)) {
			return -EINVAL;
		}
		break;

	case ECM_SDX_PCC_RULE_ACTION_CLEAR:
		pr_debug("Clear\n");
		if (!ecm_sdx_pcc_clear_rules()) {
			return -EINVAL;
		}
		break;

	default:
		pr_warn("Unknown action: %u\n", action);
		return -EINVAL;
	}

	return *lenp;

fail:
	kfree(rule_buf);
	return -EINVAL;
}

/*
 * ecm_sdx_pcc_rule_read()
 *	Read a rule
 *	return 0 for success
 */
static int ecm_sdx_pcc_rule_read(void *buffer, size_t *lenp, loff_t *ppos)
{
	size_t len = 0;
	struct ecm_sdx_pcc_rule *rule = NULL;
	char *read_buf;
	size_t read_buf_size = ECM_SDX_PCC_READ_BUF_SIZE;

	read_buf = kzalloc(read_buf_size, GFP_KERNEL);
	if (!read_buf) {
		return -ENOMEM;
	}

	spin_lock_bh(&ecm_sdx_pcc_rules_lock);
	list_for_each_entry(rule, &ecm_sdx_pcc_rules, list) {
		len += scnprintf(read_buf + len, read_buf_size - len,
			"RULE:\n"
			"\tname: %s\n"
			"\taccel: %d\n"
			"\tmac_addr: %pM\n"
			"\tip_addr_start: " ECM_SDX_PCC_IP_ADDR_DOT_FMT "\n"
			"\tip_addr_end: " ECM_SDX_PCC_IP_ADDR_DOT_FMT "\n"
			"\tiface: %s\n",
			rule->name,
			(int)(rule->accel),
			rule->mac_addr,
			ECM_SDX_PCC_IP_ADDR_TO_DOT(ntohl(rule->ip_addr_start.s6_addr32[0])),
			ECM_SDX_PCC_IP_ADDR_TO_DOT(ntohl(rule->ip_addr_end.s6_addr32[0])),
			rule->iface
		);
        }
	spin_unlock_bh(&ecm_sdx_pcc_rules_lock);
	*lenp = memory_read_from_buffer(buffer, *lenp, ppos, read_buf, len);

	kfree(read_buf);

	return 0;
}

/*
 * ecm_sdx_pcc_rule_handler()
 *	Handle rule proc entry
 */
static int ecm_sdx_pcc_rule_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	if (write) {
		return ecm_sdx_pcc_rule_write(buffer, lenp, ppos);
	}

	return ecm_sdx_pcc_rule_read(buffer, lenp, ppos);
}


/*
 * ecm_sdx_pcc_unregister_handler()
 *	Handle unregister proc entry
 *	return 0 for success
 */
static int ecm_sdx_pcc_unregister_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	int current_value;

	/*
	 * Take the current value
	 */
	current_value = ecm_sdx_pcc_unregister;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		return ret;
	}

	if ((ecm_sdx_pcc_unregister != 1)) {
		pr_warn("Invalid input. Valid values 1\n");
		ecm_sdx_pcc_unregister = current_value;
		return -EINVAL;
	}

	/*
	 * echo 1 > /proc/sys/net/ecm/ecm_sdx_pcc_unregister
	 * 	unregister ecm_sdx_pcc from ecm classifier pcc
	 */
	if (ecm_sdx_pcc_registrant) {
		ecm_classifier_pcc_unregister_begin(ecm_sdx_pcc_registrant);
		ecm_sdx_pcc_registrant = NULL;
		pr_info("ECM SDX PCC registrant UNREGISTERED\n");
	}

	return 0;
}

static struct ctl_table ecm_sdx_pcc_sysctl_tbl[] = {
	{
		.procname		= "ecm_sdx_pcc_rule",
		.data			= &ecm_sdx_pcc_rules,
		.maxlen			= sizeof(ecm_sdx_pcc_rules),
		.mode			= 0666,
		.proc_handler		= &ecm_sdx_pcc_rule_handler,
	},
	{
		.procname		= "ecm_sdx_pcc_unregister",
		.data			= &ecm_sdx_pcc_unregister,
		.maxlen			= sizeof(ecm_sdx_pcc_unregister),
		.mode			= 0644,
		.proc_handler		= &ecm_sdx_pcc_unregister_handler,
	},
};

/*
 * ecm_sdx_pcc_init()
 * return 0 for success
 */
static int __init ecm_sdx_pcc_init(void)
{
	int result;

	pr_info("ECM SDX PCC INIT\n");

	/*
	 * Create our registrant structure
	 */
	ecm_sdx_pcc_registrant = (struct ecm_classifier_pcc_registrant *)
			kzalloc(sizeof(struct ecm_classifier_pcc_registrant),
				GFP_ATOMIC | __GFP_NOWARN);
	if (!ecm_sdx_pcc_registrant) {
		pr_warn("ECM PCC Failed to alloc registrant\n");
		return -1;
	}

	ecm_sdx_pcc_registrant->version = 1;
	ecm_sdx_pcc_registrant->this_module = THIS_MODULE;
	ecm_sdx_pcc_registrant->ref = ecm_sdx_pcc_registrant_ref;
	ecm_sdx_pcc_registrant->deref = ecm_sdx_pcc_registrant_deref;

	/*
	 * Set the callback for pcc classifier to check if the connection can be
	 * accelerated or not with the ecm_sdx_pcc rules
	 */
	ecm_sdx_pcc_registrant->get_accel_info_v4 =
		ecm_sdx_pcc_get_accel_info_v4;
	ecm_sdx_pcc_registrant->get_accel_info_v6 =
		ecm_sdx_pcc_get_accel_info_v6;

	/*
	 * Register with the PCC Classifier. ECM classifier will take a ref for
	 * registrant.
	 */
	result = ecm_classifier_pcc_register(ecm_sdx_pcc_registrant);
	if (result != 0) {
		pr_warn("ECM PCC registrant failed to register: %d\n", result);
		kfree(ecm_sdx_pcc_registrant);
		return -2;
	}

	pr_info("ECM SDX PCC registrant REGISTERED\n");

	/*
	 * Register sysctl to create proc entry for control functions
	 */
	ecm_sdx_pcc_ctl_tbl_hdr = register_sysctl("net/ecm", ecm_sdx_pcc_sysctl_tbl);
	if (!ecm_sdx_pcc_ctl_tbl_hdr) {
		pr_warn("Unable to register ecm_sdx_pcc_ctl_tbl_hdr");
		if (ecm_sdx_pcc_registrant) {
			ecm_classifier_pcc_unregister_begin(ecm_sdx_pcc_registrant);
			ecm_sdx_pcc_registrant = NULL;
		}
		return -3;
	}

	return 0;
}

/*
 * ecm_sdx_pcc_exit()
 */
static void __exit ecm_sdx_pcc_exit(void)
{
	pr_info("ECM SDX PCC EXIT\n");
	if (ecm_sdx_pcc_ctl_tbl_hdr) {
		unregister_sysctl_table(ecm_sdx_pcc_ctl_tbl_hdr);
		ecm_sdx_pcc_ctl_tbl_hdr = NULL;
	}
}

module_init(ecm_sdx_pcc_init)
module_exit(ecm_sdx_pcc_exit)

MODULE_DESCRIPTION("ECM SDX PCC");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif
