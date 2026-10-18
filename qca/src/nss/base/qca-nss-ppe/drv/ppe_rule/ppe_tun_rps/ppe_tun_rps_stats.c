/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include "ppe_tun_rps.h"

/*
 * ppe_tun_rps_stats_gen_str
 * 	PPE tunnel RPS connection statistics
 */
static const char *ppe_tun_rps_stats_str[] = {
	"v4_create_ppe_tun_rps_rule",		/* v4 create tun RPS rule request */
	"v4_create_ppe_tun_rps_rule_fail",	/* v4 create tun RPS ppe rule addition failed */
	"v4_destroy_ppe_tun_rps_rule",		/* v4 destroy tun RPS rule request */
	"v4_destroy_ppe_tun_rps_rule_fail",	/* v4 destroy tun RPS ppe rule addition failed */
	"v6_create_ppe_tun_rps_rule",		/* v6 create tun RPS rule request */
	"v6_create_ppe_tun_rps_rule_fail",	/* v6 create tun RPS ppe rule addition failed */
	"v6_destroy_ppe_tun_rps_rule",		/* v6 destroy tun RPS rule request */
	"v6_destroy_ppe_tun_rps_rule_fail"	/* v6 destroy tun RPS ppe rule addition failed */
};

/*
 * ppe_tun_rps_stats_show()
 *	Read ppe tunnel RPS connection statistics
 */
static int ppe_tun_rps_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_tun_rps *g_tun_rps = &gbl_tun_rps;
	uint64_t stats_shadow[ARRAY_SIZE(ppe_tun_rps_stats_str)];
	int i;

	spin_lock_bh(&g_tun_rps->lock);
	stats_shadow[0] = atomic64_read(&g_tun_rps->stats.v4_create_ppe_tun_rps_rule);
	stats_shadow[1] = atomic64_read(&g_tun_rps->stats.v4_create_ppe_tun_rps_rule_fail);
	stats_shadow[2] = atomic64_read(&g_tun_rps->stats.v4_destroy_ppe_tun_rps_rule);
	stats_shadow[3] = atomic64_read(&g_tun_rps->stats.v4_destroy_ppe_tun_rps_rule_fail);
	stats_shadow[4] = atomic64_read(&g_tun_rps->stats.v6_create_ppe_tun_rps_rule);
	stats_shadow[5] = atomic64_read(&g_tun_rps->stats.v6_create_ppe_tun_rps_rule_fail);
	stats_shadow[6] = atomic64_read(&g_tun_rps->stats.v6_destroy_ppe_tun_rps_rule);
	stats_shadow[7] = atomic64_read(&g_tun_rps->stats.v6_destroy_ppe_tun_rps_rule_fail);
	spin_unlock_bh(&g_tun_rps->lock);

	seq_puts(m, "\nPPE stats:\n\n");
	for (i = 0; i < ARRAY_SIZE(ppe_tun_rps_stats_str); i++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", ppe_tun_rps_stats_str[i], stats_shadow[i]);
	}

	return 0;
}

/*
 * ppe_tun_rps_stats_write()
 *	Reset all stats counters to 0 on write
 */
static ssize_t ppe_tun_rps_stats_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct ppe_tun_rps *g_tun_rps = &gbl_tun_rps;

	spin_lock_bh(&g_tun_rps->lock);
	atomic64_set(&g_tun_rps->stats.v4_create_ppe_tun_rps_rule, 0);
	atomic64_set(&g_tun_rps->stats.v4_create_ppe_tun_rps_rule_fail, 0);
	atomic64_set(&g_tun_rps->stats.v4_destroy_ppe_tun_rps_rule, 0);
	atomic64_set(&g_tun_rps->stats.v4_destroy_ppe_tun_rps_rule_fail, 0);
	atomic64_set(&g_tun_rps->stats.v6_create_ppe_tun_rps_rule, 0);
	atomic64_set(&g_tun_rps->stats.v6_create_ppe_tun_rps_rule_fail, 0);
	atomic64_set(&g_tun_rps->stats.v6_destroy_ppe_tun_rps_rule, 0);
	atomic64_set(&g_tun_rps->stats.v6_destroy_ppe_tun_rps_rule_fail, 0);
	spin_unlock_bh(&g_tun_rps->lock);

	return count;
}

/*
 * ppe_tun_rps_stats_general_open()
 *	PPE tun_rps gen open callback API
 */
static int ppe_tun_rps_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_tun_rps_stats_show, inode->i_private);
}

/*
 * ppe_tun_rps_stats_general_file_ops
 *	File operations for EDMA common stats
 */
const struct file_operations ppe_tun_rps_stats_file_ops = {
	.open = ppe_tun_rps_stats_open,
	.read = seq_read,
	.write = ppe_tun_rps_stats_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_tun_rps_rules_show()
 *	Read ppe tunnel RPS active rules detailed configuration
 */
static int ppe_tun_rps_rules_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_tun_rps_rule_entry rules_snapshot[PPE_TUN_RPS_RULES_MAX];
	struct ppe_tun_rps_rule_entry *entry;
	struct ppe_tun_rps *g_tun_rps = &gbl_tun_rps;
	uint32_t active_count = 0;
	int i, j;

	/*
	 * Safe Snapshot under lock
	 */
	spin_lock_bh(&g_tun_rps->lock);
	list_for_each_entry(entry, &g_tun_rps->rules_list, list) {
		if (active_count < PPE_TUN_RPS_RULES_MAX) {
			memcpy(&rules_snapshot[active_count], entry, sizeof(struct ppe_tun_rps_rule_entry));
			active_count++;
		}
	}
	spin_unlock_bh(&g_tun_rps->lock);

	seq_printf(m, "Active Tunnel RPS Rules Count: %u\n", active_count);
	seq_puts(m, "==================================================\n\n");

	/*
	 * Display detailed parameters for each active rule snapshot (safely outside spinlock)
	 */
	for (j = 0; j < active_count; j++) {
		struct ppe_tun_rps_rule_entry *snap = &rules_snapshot[j];
		seq_printf(m, "[Rule %d]\n", j + 1);
		if (snap->ip_proto != IPPROTO_IPV6) {
			struct ppe_tun_rps_ipv4_rule_create_msg *v4 = &snap->msg.v4;
			__be32 sip = htonl(v4->flow_ip);
			__be32 dip = htonl(v4->return_ip);
			seq_printf(m, "  IP Version: IPv4\n");

			if (v4->flow_ip == 0) {
				seq_puts(m, "  Flow SIP: Any (Wildcard)\n");
			} else {
				seq_printf(m, "  Flow SIP: %pI4\n", &sip);
			}

			if (v4->return_ip == 0) {
				seq_puts(m, "  Return DIP: Any (Wildcard)\n");
			} else {
				seq_printf(m, "  Return DIP: %pI4\n", &dip);
			}

			if (v4->protocol == 0) {
				seq_puts(m, "  Protocol: Any (Wildcard)\n");
			} else {
				seq_printf(m, "  Protocol: %u\n", v4->protocol);
			}

			if (v4->protocol == IPPROTO_TCP || v4->protocol == IPPROTO_UDP || v4->protocol == IPPROTO_UDPLITE) {
				if (v4->flow_ident == 0) {
					seq_puts(m, "  Source Port: Any (Wildcard)\n");
				} else {
					seq_printf(m, "  Source Port: %u\n", v4->flow_ident);
				}

				if (v4->return_ident == 0) {
					seq_puts(m, "  Destination Port: Any (Wildcard)\n");
				} else {
					seq_printf(m, "  Destination Port: %u\n", v4->return_ident);
				}
			} else {
				seq_puts(m, "  L4 Ports: Not Applicable (Non-ported protocol)\n");
			}

			seq_printf(m, "  Header Length: %u bytes\n", v4->header_len);
			seq_printf(m, "  Header Length Type: %u\n", v4->header_len_type);
			seq_printf(m, "  Inner Packet Type: %s (%u)\n",
				   (v4->inner_pkt_type == PPE_TUN_RPS_INNER_PKT_TYPE_IP) ? "IP" : "Ethernet", v4->inner_pkt_type);
			seq_printf(m, "  WAN Interface: %s\n", v4->wan_if);

			if (v4->pppoe_en) {
				seq_printf(m, "  PPPoE: Enabled (Session ID: %u, Server MAC: %pM)\n",
					   v4->pppoe_session_id, v4->pppoe_server_mac);
			} else {
				seq_puts(m, "  PPPoE: Disabled\n");
			}

			/*
			 * Header match criteria configuration
			 */
			seq_puts(m, "  Header Match Config:\n");
			switch (v4->header_len_type) {
			case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
				seq_printf(m, "    Type: L2 (ETH)\n");
				seq_printf(m, "    Ethernet Protocol: 0x%04x, Mask: 0x%04x\n",
					   v4->header_match.l2_match.eth_protocol,
					   v4->header_match.l2_match.eth_protocol_mask);
				break;
			case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
				seq_printf(m, "    Type: L3 (IP)\n");
				seq_printf(m, "    IP Protocol: %u, Mask: 0x%02x\n",
					   v4->header_match.l3_match.ip_protocol,
					   v4->header_match.l3_match.ip_protocol_mask);
				break;
			case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
				seq_printf(m, "    Type: L4 (UDP)\n");
				seq_printf(m, "    UDP Source Port: %u, Destination Port: %u\n",
					   v4->header_match.l4_match.udp_sport,
					   v4->header_match.l4_match.udp_dport);
				break;
			default:
				seq_printf(m, "    Type: Unknown (%u)\n", v4->header_len_type);
				break;
			}

			seq_puts(m, "  UDF Config:\n");
			for (i = 0; i < 3; i++) {
				if (v4->usr_data.udf[i].enable) {
					seq_printf(m, "    UDF%d: Enabled (Offset: %u, Val: 0x%x, Mask: 0x%x)\n",
						   i + 1, v4->usr_data.udf[i].offset,
						   v4->usr_data.udf[i].val, v4->usr_data.udf[i].mask);
				} else {
					seq_printf(m, "    UDF%d: Disabled\n", i + 1);
				}
			}

			if (v4->inner_pkt_type == PPE_TUN_RPS_INNER_PKT_TYPE_IP) {
				seq_puts(m, "  Inner IP Protocol Config:\n");
				seq_printf(m, "    Offset: %u\n", v4->inner_ip_proto.ip_proto_offset);
				seq_printf(m, "    IPv4 Val: 0x%x, Mask: 0x%x\n",
					   v4->inner_ip_proto.ip_proto_ipv4_value, v4->inner_ip_proto.ip_proto_ipv4_mask);
				seq_printf(m, "    IPv6 Val: 0x%x, Mask: 0x%x\n",
					   v4->inner_ip_proto.ip_proto_ipv6_value, v4->inner_ip_proto.ip_proto_ipv6_mask);
			}
		} else {
			struct ppe_tun_rps_ipv6_rule_create_msg *v6 = &snap->msg.v6;
			bool sip_all_zero = (v6->flow_ip[0] == 0 && v6->flow_ip[1] == 0 &&
					     v6->flow_ip[2] == 0 && v6->flow_ip[3] == 0);
			bool dip_all_zero = (v6->return_ip[0] == 0 && v6->return_ip[1] == 0 &&
					     v6->return_ip[2] == 0 && v6->return_ip[3] == 0);

			seq_printf(m, "  IP Version: IPv6\n");

			if (sip_all_zero) {
				seq_puts(m, "  Flow SIP: Any (Wildcard)\n");
			} else {
				seq_printf(m, "  Flow SIP: %pI6\n", v6->flow_ip);
			}

			if (dip_all_zero) {
				seq_puts(m, "  Return DIP: Any (Wildcard)\n");
			} else {
				seq_printf(m, "  Return DIP: %pI6\n", v6->return_ip);
			}

			if (v6->protocol == 0) {
				seq_puts(m, "  Protocol: Any (Wildcard)\n");
			} else {
				seq_printf(m, "  Protocol: %u\n", v6->protocol);
			}

			if (v6->protocol == IPPROTO_TCP || v6->protocol == IPPROTO_UDP || v6->protocol == IPPROTO_UDPLITE) {
				if (v6->flow_ident == 0) {
					seq_puts(m, "  Source Port: Any (Wildcard)\n");
				} else {
					seq_printf(m, "  Source Port: %u\n", v6->flow_ident);
				}

				if (v6->return_ident == 0) {
					seq_puts(m, "  Destination Port: Any (Wildcard)\n");
				} else {
					seq_printf(m, "  Destination Port: %u\n", v6->return_ident);
				}
			} else {
				seq_puts(m, "  L4 Ports: Not Applicable (Non-ported protocol)\n");
			}

			seq_printf(m, "  Header Length: %u bytes\n", v6->header_len);
			seq_printf(m, "  Header Length Type: %u\n", v6->header_len_type);
			seq_printf(m, "  Inner Packet Type: %s (%u)\n",
				   (v6->inner_pkt_type == PPE_TUN_RPS_INNER_PKT_TYPE_IP) ? "IP" : "Ethernet", v6->inner_pkt_type);
			seq_printf(m, "  WAN Interface: %s\n", v6->wan_if);

			if (v6->pppoe_en) {
				seq_printf(m, "  PPPoE: Enabled (Session ID: %u, Server MAC: %pM)\n",
					   v6->pppoe_session_id, v6->pppoe_server_mac);
			} else {
				seq_puts(m, "  PPPoE: Disabled\n");
			}

			/*
			 * Header match criteria configuration
			 */
			seq_puts(m, "  Header Match Config:\n");
			switch (v6->header_len_type) {
			case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
				seq_printf(m, "    Type: L2 (ETH)\n");
				seq_printf(m, "    Ethernet Protocol: 0x%04x, Mask: 0x%04x\n",
					   v6->header_match.l2_match.eth_protocol,
					   v6->header_match.l2_match.eth_protocol_mask);
				break;
			case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
				seq_printf(m, "    Type: L3 (IP)\n");
				seq_printf(m, "    IP Protocol: %u, Mask: 0x%02x\n",
					   v6->header_match.l3_match.ip_protocol,
					   v6->header_match.l3_match.ip_protocol_mask);
				break;
			case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
				seq_printf(m, "    Type: L4 (UDP)\n");
				seq_printf(m, "    UDP Source Port: %u, Destination Port: %u\n",
					   v6->header_match.l4_match.udp_sport,
					   v6->header_match.l4_match.udp_dport);
				break;
			default:
				seq_printf(m, "    Type: Unknown (%u)\n", v6->header_len_type);
				break;
			}

			seq_puts(m, "  UDF Config:\n");
			for (i = 0; i < 3; i++) {
				if (v6->usr_data.udf[i].enable) {
					seq_printf(m, "    UDF%d: Enabled (Offset: %u, Val: 0x%x, Mask: 0x%x)\n",
						   i + 1, v6->usr_data.udf[i].offset,
						   v6->usr_data.udf[i].val, v6->usr_data.udf[i].mask);
				} else {
					seq_printf(m, "    UDF%d: Disabled\n", i + 1);
				}
			}

			if (v6->inner_pkt_type == PPE_TUN_RPS_INNER_PKT_TYPE_IP) {
				seq_puts(m, "  Inner IP Protocol Config:\n");
				seq_printf(m, "    Offset: %u\n", v6->inner_ip_proto.ip_proto_offset);
				seq_printf(m, "    IPv4 Val: 0x%x, Mask: 0x%x\n",
					   v6->inner_ip_proto.ip_proto_ipv4_value, v6->inner_ip_proto.ip_proto_ipv4_mask);
				seq_printf(m, "    IPv6 Val: 0x%x, Mask: 0x%x\n",
					   v6->inner_ip_proto.ip_proto_ipv6_value, v6->inner_ip_proto.ip_proto_ipv6_mask);
			}
		}
		seq_puts(m, "\n");
	}

	seq_puts(m, "==================================================\n");
	return 0;
}

/*
 * ppe_tun_rps_rules_open()
 *	PPE tun_rps active rules open callback API
 */
static int ppe_tun_rps_rules_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_tun_rps_rules_show, inode->i_private);
}

/*
 * ppe_tun_rps_rules_file_ops
 *	File operations for active rules config dump
 */
const struct file_operations ppe_tun_rps_rules_file_ops = {
	.open = ppe_tun_rps_rules_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_tun_rps_flush_write()
 *	Flush all active rules on write (trigger safe rules flush)
 */
static ssize_t ppe_tun_rps_flush_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	ppe_tun_rps_flush_rules();
	return count;
}

/*
 * ppe_tun_rps_flush_file_ops
 *	File operations for write-only flushing of active rules
 */
const struct file_operations ppe_tun_rps_flush_file_ops = {
	.write = ppe_tun_rps_flush_write,
};

/*
 * ppe_tun_rps_stats_debugfs_init()
 *	Create PPE tun_rps statistics debug entry.
 */
int ppe_tun_rps_stats_debugfs_init(struct dentry *root)
{
	struct ppe_tun_rps *g_tun_rps = &gbl_tun_rps;
	g_tun_rps->dentry = debugfs_create_dir("ppe-tun-rps", root);
	if (!g_tun_rps->dentry) {
		ppe_tun_rps_warn("%p: Unable to create debugfs stats directory in debugfs\n", g_tun_rps);
		return -1;
	}

	/*
	 * Create debugfs files
	 */
	if (!debugfs_create_file("stats", S_IRUGO | S_IWUSR, g_tun_rps->dentry,
			NULL, &ppe_tun_rps_stats_file_ops)) {
		ppe_tun_rps_warn("%p: Unable to create common statistics file entry in debugfs\n", g_tun_rps);
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("rules", S_IRUGO, g_tun_rps->dentry,
			NULL, &ppe_tun_rps_rules_file_ops)) {
		ppe_tun_rps_warn("%p: Unable to create active rules config file entry in debugfs\n", g_tun_rps);
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("flush", S_IWUSR, g_tun_rps->dentry,
			NULL, &ppe_tun_rps_flush_file_ops)) {
		ppe_tun_rps_warn("%p: Unable to create active rules flush file entry in debugfs\n", g_tun_rps);
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(g_tun_rps->dentry);
	g_tun_rps->dentry = NULL;
	return -1;
}

/*
 * ppe_tun_rps_stats_debugfs_exit()
 *	PPE debugfs exit api
 */
void ppe_tun_rps_stats_debugfs_exit(void)
{
	struct ppe_tun_rps *g_tun_rps = &gbl_tun_rps;
	if (g_tun_rps->dentry) {
		debugfs_remove_recursive(g_tun_rps->dentry);
		g_tun_rps->dentry = NULL;
	}
}
