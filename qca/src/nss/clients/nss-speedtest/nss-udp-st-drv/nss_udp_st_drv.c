/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <net/act_api.h>
#include <linux/major.h>
#include <linux/version.h>
#include <linux/math64.h>
#include <linux/if_vlan.h>
#include <net/netfilter/nf_conntrack_core.h>
#ifdef NSS_UDP_ST_DRV_VP_ENABLE
#include <ppe_drv.h>
#include <ppe_drv_vp.h>
#include <ppe_vp_public.h>
#endif
#include "nss_udp_st_public.h"
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
#include <nss_dp_api_if.h>
#include "nss_udp_st_ppe.h"
#endif
#include <nss_dp_udp_st.h>

#define DEVICE_NAME "nss_udp_st"
#define CLASS_NAME "nss_udp_st"

static const struct file_operations nss_udp_st_ops;
static struct class *dump_class;
static int dump_major;

struct nss_udp_st nust;
bool is_dummy_vp_exists = false;
struct delayed_work nss_udp_st_tx_delayed_work;
struct workqueue_struct *work_queue;
void nss_udp_st_update_stats(size_t pkt_size, int64_t num_pkts);
uint64_t nss_udp_st_tx_num_pkt;
struct net_device *nust_dev;
struct nss_udp_st_rules *exception_dport_rules;
uint8_t exception_rules_cnt = 0;

/*
 * nss_udp_st_rx_ipv4_pre_routing_hook
 *	pre-routing hook into netfilter packet monitoring point for IPv4
 */
static struct nf_hook_ops nss_udp_st_nf_ipv4_ops[] __read_mostly = {
	{
		.hook   =   nss_udp_st_rx_ipv4_pre_routing_hook,
		.pf =   NFPROTO_IPV4,
		.hooknum    =   NF_INET_PRE_ROUTING,
		.priority   =   NF_IP_PRI_RAW_BEFORE_DEFRAG,
	},
};

/*
 * nss_udp_st_rx_ipv6_pre_routing_hook
 *	pre-routing hook into netfilter packet monitoring point for IPv6
 */
static struct nf_hook_ops nss_udp_st_nf_ipv6_ops[] __read_mostly = {
	{
		.hook	=	nss_udp_st_rx_ipv6_pre_routing_hook,
		.pf	=	NFPROTO_IPV6,
		.hooknum	=	NF_INET_PRE_ROUTING,
		.priority	=	NF_IP_PRI_RAW_BEFORE_DEFRAG,
	},
};

/*
 * nss_udp_st_check_rules()
 *	check for ARP resolution and valid return mac
 */
static int nss_udp_st_check_rules(struct nss_udp_st_rules *rules)
{
	if (rules->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		if (nss_udp_st_get_macaddr_ipv4(rules->dip.ip.ipv4, (uint8_t *)&rules->dst_mac)) {
			udp_st_err("Error in Updating the Return MAC Address\n");
			return -EINVAL;
		}

		if (nss_udp_st_get_local_macaddr_ipv4(rules->sip.ip.ipv4, (uint8_t *)&rules->src_mac)) {
			udp_st_err("Error in Updating the Source MAC Address\n");
			return -EINVAL;
		}
		pr_info("UDP-ST: resolved src_mac=%pM for sip=%pI4\n", rules->src_mac, &rules->sip.ip.ipv4);
	} else if (rules->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		if (nss_udp_st_get_macaddr_ipv6(rules->dip.ip.ipv6, (uint8_t *)&rules->dst_mac)) {
			udp_st_err("Error in Updating the Return MAC Address\n");
			return -EINVAL;
		}

		if (nss_udp_st_get_local_macaddr_ipv6(rules->sip.ip.ipv6, (uint8_t *)&rules->src_mac)) {
			udp_st_err("Error in Updating the Source MAC Address\n");
			return -EINVAL;
		}
		pr_info("UDP-ST: resolved src_mac=%pM for sip=%pI6\n", rules->src_mac, rules->sip.ip.ipv6);
	} else {
		udp_st_err("invalid ip version flag\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * nss_udp_st_clear_rules()
 *	clear rules list
 */
static void nss_udp_st_clear_rules(void)
{
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;

	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
#ifdef NSS_UDP_ST_DRV_VP_ENABLE
		if ((nust.config.flags & NSS_UDP_ST_FLAGS_VP) && !(nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD)) {
			nss_udp_st_tun_destroy(pos->tun_dev);
		}
#endif
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
			/*
			 * Destroy PPE flow rule for the UDP-ST rule
			 */
			nss_udp_st_destroy_ppe_flow(pos);
			pos->ppe_dev = NULL;
		}
#endif
		list_del(&pos->list);
		kfree(pos);
	}
	nust.rule_count = 0;
	exception_rules_cnt = 0;

	if (nust.pppoe_info.dev) {
		nust.pppoe_info.dev = NULL;
	}
}

/*
 * nss_udp_st_open()
 *	open for file ops on /dev/nss-udp-st
 */
static int nss_udp_st_open(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * nss_udp_st_release()
 *	release /dev/nss-udp-st
 */
static int nss_udp_st_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * nss_udp_st_read()
 *	send stats to userspace
 */
static ssize_t nss_udp_st_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int copied = 0;

	copied = copy_to_user(buf, &nust.stats, sizeof(struct nss_udp_st_stats));
	if (copied) {
		return -EFAULT;
	}
	return sizeof(struct nss_udp_st_stats);
}

/*
 * nss_udp_st_write()
 *	receive rules from userspace
 */
static ssize_t nss_udp_st_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0;
	int cpu = 0;
	struct nss_udp_st_opt opt;
	struct nss_udp_st_rules *rules;

	/*
	 * don't push rules if test has already started
	 */
	if (nust.mode == NSS_UDP_ST_START) {
		udp_st_err("Test already started\n");
		return -EINVAL;
	}

	rules = (struct nss_udp_st_rules *)kzalloc(sizeof(struct nss_udp_st_rules), GFP_KERNEL);
	if (!rules) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_MEMORY_FAILURE]);
		return -EINVAL;
	}

	ret = copy_from_user((void *)(uintptr_t)&opt, (void __user *)buf, sizeof(struct nss_udp_st_opt));
	if (ret) {
		kfree(rules);
		return -EINVAL;
	}

	rules->sport = opt.sport;
	rules->dport = opt.dport;
	if(opt.ip_version == 4) {
		rules->ip_version = NSS_UDP_ST_FLAG_IPV4;
		nss_udp_st_get_ipaddr_ntoh(opt.sip, sizeof(struct in_addr), &rules->sip.ip.ipv4);
		nss_udp_st_get_ipaddr_ntoh(opt.dip, sizeof(struct in_addr), &rules->dip.ip.ipv4);
	} else if(opt.ip_version == 6) {
		rules->ip_version = NSS_UDP_ST_FLAG_IPV6;
		nss_udp_st_get_ipaddr_ntoh(opt.sip, sizeof(struct in6_addr), rules->sip.ip.ipv6);
		nss_udp_st_get_ipaddr_ntoh(opt.dip, sizeof(struct in6_addr), rules->dip.ip.ipv6);
	} else {
		kfree(rules);
		return -EINVAL;
	}

	ret = nss_udp_st_check_rules(rules);
	if (ret) {
		kfree(rules);
		return -EINVAL;
	}

	rules->seq_greatest = 0;
	rules->seq = 0;
	rules->tun_dev = NULL;
	rules->vp_num = -1;
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
	rules->ppe_dev = NULL;
#endif

	if (nust.bitmap_curr == 0) {
		nust.bitmap_curr = nust.config.cpu_bitmap;
	}

	cpu = ffs(nust.bitmap_curr);
	cpu--;
	nust.bitmap_curr &= ~(1 << cpu);
	udp_st_debug("CPU: %d, nust_bitmap %u base: nust.config.cpu_bitmap %u ", cpu, nust.bitmap_curr, nust.config.cpu_bitmap);
	rules->cpu = cpu;

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
	/*
	 * Enforce maximum flow limit for hardware offload.
	 * Reject any rule that would exceed NSS_UDP_ST_PPE_MAX_FLOWS.
	 */
	if ((nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) &&
	    (nust.rule_count >= NSS_UDP_ST_PPE_MAX_FLOWS)) {
		pr_err("UDP-ST: Hardware offload supports a maximum of %d max flows\n",
		       NSS_UDP_ST_PPE_MAX_FLOWS);
		kfree(rules);
		return -EINVAL;
	}
#endif

	list_add_tail(&(rules->list), &(nust.rules.list));
	nust.rule_count++;
	rules->rule_id = nust.rule_count;

	return 0;
}

/*
 * nss_udp_st_reset_stats()
 *	clear stats before starting test
 */
static void nss_udp_st_reset_stats(void)
{
	struct nss_udp_st_rules *rule = NULL;
	struct nss_udp_st_rules *n = NULL;

	memset(&nust.stats, 0, sizeof(struct nss_udp_st_stats));
	nust.stats.first_pkt = true;
	nss_udp_st_tx_num_pkt = 0;
	atomic_set(&nust.xmit_idx, 0);
	atomic64_set(&nust.stats.p_stats.min_latency, U64_MAX);
	atomic64_set(&nust.stats.p_stats.max_latency, 0);

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
	if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD)
		nss_udp_st_ppe_reset_policer_stats();
#endif

	/*
	 * Sequence counters are maintained per connection
	 * and thus need to be reset for each connection
	 */
	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {
		rule->seq_greatest = 0;
		rule->seq = 0;
	}
}

/*
 * nss_udp_st_init_validate_rate()
 *	Validate the configured rate against the max bandwidth of the
*	configured net device at init time.
 */
static bool nss_udp_st_init_validate_rate(void)
{
	struct net_device *dev;
	struct net_device *phys_dev;
	bool valid;

	dev = dev_get_by_name(&init_net, nust.config.net_dev);
	if (!dev) {
		udp_st_err("Cannot find net device %s for rate validation\n", nust.config.net_dev);
		return false;
	}

	if (dev->type == ARPHRD_PPP) {
		/*
		 * Check if the WAN device is pppoe interface
		 */
		if (nss_udp_st_pppoe_iface_config(dev) < 0) {
			udp_st_err("Cannot resolve PPPoE physical device for %s\n", nust.config.net_dev);
			dev_put(dev);
			return false;
		}

		phys_dev = nss_udp_st_get_xmit_dev();
		nust.pppoe_info.dev = NULL;
	} else if (is_vlan_dev(dev)) {
		phys_dev = vlan_dev_next_dev(dev);
	} else {
		phys_dev = dev;
	}

	valid = nss_udp_st_validate_rate(phys_dev, nust.config.rate);
	dev_put(dev);

	return valid;
}

/*
 * nss_udp_st_ioctl()
 *	receive ioctl to init / start / stop test
 */
static long nss_udp_st_ioctl(struct file *file, unsigned int ioctl_num,
				unsigned long arg)
{
	int ret = 0;
	int max_bitmap = 1;

	switch (ioctl_num) {
	case NSS_UDP_ST_IOCTL_INIT:
		memset(&(nust.config), 0, sizeof(struct nss_udp_st_param));
		ret = copy_from_user((void *)&(nust.config), (void __user *)arg, sizeof(struct nss_udp_st_param));
		if (ret) {
			return -EINVAL;
		}

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if ((nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) && nust.config.cpu_bitmap) {
			pr_err("UDP-ST: cpu_bitmap is not supported with hardware offload\n");
			return -EINVAL;
		}
#endif

		if (!nss_udp_st_init_validate_rate()) {
			udp_st_err("Configured rate %u Mbps exceeds max bandwidth of interface %s\n",
				    nust.config.rate, nust.config.net_dev);
			return -EINVAL;
		}

		max_bitmap = (1 << NR_CPUS) - 1;
		nust.bitmap_curr = nust.config.cpu_bitmap;
		if (nust.bitmap_curr == 0) {
			udp_st_trace("Setting default TX CPU to CPU 0");
			nust.bitmap_curr = 1;
		} else if (nust.bitmap_curr > max_bitmap) {
			udp_st_err("Incorrect bitmap: %u\n", nust.bitmap_curr);
			return -EINVAL;
		}

		if (is_dummy_vp_exists) {
			nss_udp_st_rx_free_dummy_vp(nust.dummy_vp_num);
			is_dummy_vp_exists = false;
		}

		/*
		 * Alloc dummy vp: Required for creating PPE RFS rule to enable RFS on the Rx side.
		 */
		nust.dummy_vp_num = nss_udp_st_rx_dummy_vp_alloc();

		if (nust.dummy_vp_num != -1) {
			is_dummy_vp_exists = true;
		}

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
			/*
			 * Timestamp feature is not supported with hardware offload.
			 */
			if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
				pr_err("UDP-ST: Timestamp feature is not supported with hardware offload\n");
				pr_err("UDP-ST: Please disable either hardware offload or timestamp feature\n");
				return -EINVAL;
			}

			/*
			 * Initialize the dedicated EDMA UDP-ST TX ring context.
			 */
			ret = nss_dp_udp_st_init();
			if (ret != 0) {
				pr_err("UDP-ST: EDMA TX ring init failed: %d\n", ret);
				return -EINVAL;
			}

			/*
			 * VP allocation for PPE offload
			 */
			if (!nss_udp_st_ppe_vp_alloc(nust.config.cpu_bitmap)) {
				pr_err("UDP-ST: PPE VP allocation failed\n");
				nust.mode = NSS_UDP_ST_STOP;
				return -EINVAL;
			}

			/*
			 * Initialize policer if rate limiting is configured
			 */
			nss_udp_st_ppe_policer_init();
		}
#endif
		break;

	case NSS_UDP_ST_IOCTL_START_TX:
		if (nust.mode == NSS_UDP_ST_START) {
			udp_st_err("Tx test already started\n");
			return -EINVAL;
		}

		nss_udp_st_reset_stats();
		nust.dir = NSS_UDP_ST_TX;

		memset(&(nust.time), 0, sizeof(nust.time));
		ret = copy_from_user((void *)&(nust.time), (void __user *)arg, sizeof(nust.time));
		if (ret) {
			return -EINVAL;
		}

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if ((nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) && nust.time) {
			pr_err("UDP-ST: time duration is not supported with hardware offload\n");
			pr_err("UDP-ST: HW offload Tx test can be stopped using \"nss-udp-st --mode stop\"\n");
			return -EINVAL;
		}
#endif

		if (!nust.time) {
			nust.time = NSS_UDP_ST_TX_DEFAULT_TIMEOUT;
		}

		nust.mode = NSS_UDP_ST_START;

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
			struct nss_udp_st_rules *pos = NULL;
			struct nss_udp_st_rules *n = NULL;
			ppe_vp_num_t vp_num;

			/*
			 * Propagate VP number and VP netdev to every rule
			 */
			vp_num = nss_udp_st_ppe_vp_num_get();
			list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
				pos->vp_num = vp_num;
				pos->ppe_dev = nss_udp_st_ppe_vp_dev_get();
			}

			/*
			 * Create PPE flow rules (VP as rx_if, WAN as tx_if).
			 */
			ret = nss_udp_st_ppe_create_flows(NSS_UDP_ST_PPE_TX_DIR);
			if (ret < 0) {
				pr_err("UDP-ST: PPE flow creation failed\n");
				nust.mode = NSS_UDP_ST_STOP;
				return -EINVAL;
			}

			/*
			 * Start the 1-second periodic PPE throughput timer.
			 * Every second it queries PPE hardware counters.
			 */
			if (!nss_udp_st_ppe_throughput_timer_start()) {
				nust.mode = NSS_UDP_ST_STOP;
				nss_udp_st_clear_rules();
				pr_err("Unable to start throughput timer\n");
				return -EINVAL;
			}

			/*
			 * Use separate hw_offload TX path
			 */
			if (!nss_udp_st_tx_hw_offload()) {
				nust.mode = NSS_UDP_ST_STOP;
				nss_udp_st_clear_rules();
				nss_udp_st_ppe_throughput_timer_stop();
				pr_err("Unable to start HW offload Tx test\n");
				return -EINVAL;
			}
		} else
#endif
		{
			/*
			 * Software path
			 */
			if (!nss_udp_st_tx()) {
				nust.mode = NSS_UDP_ST_STOP;
				pr_err("Unable to start Tx test\n");
				return -EINVAL;
			}
		}
		break;

	case NSS_UDP_ST_IOCTL_START_RX:
		nss_dp_udp_st_rx_register_cb(nss_udp_st_rx_receive_skb);
		if (nust.mode == NSS_UDP_ST_START) {
			udp_st_err("Rx test already started\n");
			return -EINVAL;
		}

		if (!nss_udp_st_rx_rfs_rule_create()) {
			udp_st_warn("Rx RFS rule create failed");
		}

		nss_udp_st_reset_stats();
		nust.dir = NSS_UDP_ST_RX;

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
			/*
			 * Create PPE flow rules for RX direction
			 */
			ret = nss_udp_st_ppe_create_flows(NSS_UDP_ST_PPE_RX_DIR);
			if (ret < 0) {
				pr_err("UDP-ST: PPE flow creation failed\n");
				return -EINVAL;
			}

			if (!nss_udp_st_ppe_throughput_timer_start()) {
				nss_udp_st_clear_rules();
				pr_err("UDP-ST: PPE throughput timer start failed\n");
				return -EINVAL;
			}
		}
#endif

		/*
		 * register pre-routing hook for rx path
		 */
		ret = nf_register_net_hooks(&init_net, nss_udp_st_nf_ipv4_ops, ARRAY_SIZE(nss_udp_st_nf_ipv4_ops));
		if (ret < 0) {
			udp_st_err("Can't register Rx netfilter hooks.\n");
			return -EINVAL;
		}

		ret = nf_register_net_hooks(&init_net, nss_udp_st_nf_ipv6_ops, ARRAY_SIZE(nss_udp_st_nf_ipv6_ops));
		if (ret < 0) {
			udp_st_err("Can't register Rx netfilter hooks.\n");
			nf_unregister_net_hooks(&init_net, nss_udp_st_nf_ipv4_ops, ARRAY_SIZE(nss_udp_st_nf_ipv4_ops));
			return -EINVAL;
		}
		nust.mode = NSS_UDP_ST_START;
		break;

	case NSS_UDP_ST_IOCTL_STOP:
		if (nust.mode == NSS_UDP_ST_STOP)
			break;

		nust.mode = NSS_UDP_ST_STOP;

		if (nust.dir == NSS_UDP_ST_RX) {
			/*
			 * de-register pre-routing hook for rx path
			 */
			nf_unregister_net_hooks(&init_net, nss_udp_st_nf_ipv4_ops, ARRAY_SIZE(nss_udp_st_nf_ipv4_ops));
			nf_unregister_net_hooks(&init_net, nss_udp_st_nf_ipv6_ops, ARRAY_SIZE(nss_udp_st_nf_ipv6_ops));
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
			if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD)
				nss_udp_st_ppe_throughput_timer_stop();
#endif
			nss_udp_st_rx_rfs_rule_destroy();
		} else {
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
			if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
				/*
				 * Stop the PPE timer
				 */
				nss_udp_st_ppe_throughput_timer_stop();

				/*
				 * Unregister the PPE stats-sync callback before
				 * destroying the flows so no stale callbacks are there.
				 */
				nss_dp_udp_st_reset_indices();
			} else
#endif
			{
				/*
				 * Cleanup hr_timer only for software path
				 */
				nss_udp_st_hrtimer_cleanup();
			}
		}

		/*
		 * Destroy PPE flows
		 */
		nss_udp_st_clear_rules();

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
		if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
			/*
			 * Unregister the policer flow callbacks.
			 */
			nss_udp_st_ppe_policer_detach();

			/*
			 * Destroy the global VP-level policer if it exists.
			 */
			nss_udp_st_ppe_policer_destroy();

			/*
			 * Release the nust_dev ref taken.
			 */
			if (nust_dev) {
				dev_put(nust_dev);
				nust_dev = NULL;
			}
		}
#endif

		nss_udp_st_reset_stats();
		break;

	case NSS_UDP_ST_IOCTL_RATE_CHANGE:
		ret = copy_from_user((void *)&(nust.config.rate), (void __user *)arg, sizeof(nust.config.rate));
		if (ret) {
			return -EINVAL;
		}

		if(!nss_udp_st_tx_rate_change(nust.config.rate)) {
			udp_st_debug("Rate change failed\n");
		}
		break;

	case NSS_UDP_ST_IOCTL_RESET_STATS:
		nss_udp_st_reset_stats();
		break;

	case NSS_UDP_ST_IOCTL_FINAL:
		if (is_dummy_vp_exists) {
			nss_udp_st_rx_free_dummy_vp(nust.dummy_vp_num);
			is_dummy_vp_exists = false;
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*
 * file ops for /dev/nss_udp_st
 */
static const struct file_operations nss_udp_st_ops = {
	.open       =   nss_udp_st_open,
	.read       =   nss_udp_st_read,
	.write      =  nss_udp_st_write,
	.unlocked_ioctl = nss_udp_st_ioctl,
	.release    =   nss_udp_st_release,
};

/*
 * nss_udp_st_init()
 *	create char device /dev/nss_udp_st
 */
static int __init nss_udp_st_init(void)
{
	int ret = 0;
	struct device *dump_dev;

	memset(&nust, 0, sizeof(struct nss_udp_st));
	INIT_LIST_HEAD(&(nust.rules.list));

	exception_dport_rules = (struct nss_udp_st_rules *)kzalloc(sizeof(struct nss_udp_st_rules) * 256, GFP_KERNEL);
	dump_major = register_chrdev(UNNAMED_MAJOR, DEVICE_NAME, &nss_udp_st_ops);
	if (dump_major < 0) {
		ret = dump_major;
		udp_st_err("Unable to allocate a major number err = %d\n", ret);
		goto reg_failed;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	dump_class = class_create(THIS_MODULE, CLASS_NAME);
#else
	dump_class = class_create(CLASS_NAME);
#endif
	if (IS_ERR(dump_class)) {
		ret = PTR_ERR(dump_class);
		udp_st_err("Unable to create dump class = %d\n", ret);
		goto class_failed;
	}

	dump_dev = device_create(dump_class, NULL, MKDEV(dump_major, 0), NULL, DEVICE_NAME);
	if (IS_ERR(dump_dev)) {
		ret = PTR_ERR(dump_dev);
		udp_st_err("Unable to create a device err = %d\n", ret);
		goto device_failed;
	}
	return ret;

device_failed:
	class_destroy(dump_class);
class_failed:
	unregister_chrdev(dump_major, DEVICE_NAME);
reg_failed:
	return ret;
}

/*
 * nss_udp_st_exit()
 *	clean up for /dev/nss_udp_st
 */
static void __exit nss_udp_st_exit(void)
{
	nust.mode = NSS_UDP_ST_STOP;

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
	/*
	 * Perform the full hw-offload teardown unconditionally.
	 */
	nss_udp_st_ppe_throughput_timer_stop();
	nss_dp_udp_st_reset_indices();
	nss_udp_st_clear_rules();
	nss_udp_st_ppe_policer_detach();
	if (nust_dev) {
		dev_put(nust_dev);
		nust_dev = NULL;
	}
	nss_dp_udp_st_deinit();
	nss_udp_st_ppe_vp_free();
#else
	nss_udp_st_clear_rules();
#endif

	if (is_dummy_vp_exists) {
		nss_udp_st_rx_free_dummy_vp(nust.dummy_vp_num);
		is_dummy_vp_exists = false;
	}

	device_destroy(dump_class, MKDEV(dump_major, 0));
	class_destroy(dump_class);
	unregister_chrdev(dump_major, DEVICE_NAME);
}

/*
 * nss_udp_st_update_stats()
 *  update packet and time stats for tx/rx
 *  num_pkts: number of packets to add (1 for software path, >1 for PPE batch)
 */
void nss_udp_st_update_stats(size_t pkt_size, int64_t num_pkts)
{
	long time_curr;
	long time_start;

	if (nust.stats.first_pkt) {
		atomic64_set(&nust.stats.timer_stats[NSS_UDP_ST_STATS_TIME_START], (jiffies * div_u64(1000,HZ)));
		nust.stats.first_pkt = false;
	}

	if (nust.dir == NSS_UDP_ST_TX) {
		atomic64_add(num_pkts, &nust.stats.p_stats.tx_packets);
		atomic64_add((long long)(pkt_size * num_pkts), &nust.stats.p_stats.tx_bytes);
	}

	if (nust.dir == NSS_UDP_ST_RX) {
		atomic64_add(num_pkts, &nust.stats.p_stats.rx_packets);
		atomic64_add((long long)(pkt_size * num_pkts), &nust.stats.p_stats.rx_bytes);
	}

	atomic64_set(&nust.stats.timer_stats[NSS_UDP_ST_STATS_TIME_CURRENT], (jiffies * div_u64(1000,HZ)));

	time_curr = atomic64_read(&nust.stats.timer_stats[NSS_UDP_ST_STATS_TIME_CURRENT]);
	time_start = atomic64_read(&nust.stats.timer_stats[NSS_UDP_ST_STATS_TIME_START]);
	atomic64_set(&nust.stats.timer_stats[NSS_UDP_ST_STATS_TIME_ELAPSED], (long)(time_curr - time_start));
}

module_init(nss_udp_st_init);
module_exit(nss_udp_st_exit);

MODULE_AUTHOR("Qualcomm Technologies");
MODULE_DESCRIPTION("NSS UDP Speedtest");
MODULE_LICENSE("Dual BSD/GPL");
