/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include "nss_ppe_vxlanmgr_priv.h"
#include "nss_ppe_vxlanmgr_tun_stats.h"

#define NSS_PPE_VXLAN_SPORT_BASE 49152
#define NSS_PPE_VXLAN_SPORT_MASK 0xffff
#define NSS_PPE_NACK_LIMIT_BUFFER_SIZE 64

/*
 * VxLAN context
 */
extern struct nss_ppe_vxlanmgr_ctx vxlan_ctx;

/*
 * nss_ppe_vxlan_dev_stats_update()
 * 	Update vxlan dev statistics
 */
bool nss_ppe_vxlan_dev_stats_update(struct net_device *dev, ppe_tun_hw_stats *stats, ppe_tun_data *tun_cb_data)
{
	struct pcpu_sw_netstats *tstats;
	struct net_device *pdev;
	int ifindex;

	if (!dev) {
		return false;
	}

	tstats = this_cpu_ptr(dev->tstats);

	/*
	 * For VXLAN device add the stats to the parent netdevice instead of nss_netdev.
	 */
	if (unlikely(strncmp(dev->name, "ppe_vxlantun", 12) == 0)) {
		ifindex = *(int *)netdev_priv(dev);
		pdev = dev_get_by_index(&init_net, ifindex);
		if (!pdev) {
			nss_ppe_vxlanmgr_warn("%p: Parent dev of the nss-netdev %s is not present.", dev, dev->name);
			return false;
		}

		tstats = this_cpu_ptr(pdev->tstats);
		dev_put(pdev);
	}

	u64_stats_update_begin(&tstats->syncp);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	tstats->tx_bytes += stats->tx_byte_cnt;
	tstats->tx_packets += stats->tx_pkt_cnt;
	tstats->rx_bytes += stats->rx_byte_cnt;
	tstats->rx_packets += stats->rx_pkt_cnt;
#else
	u64_stats_add(&tstats->tx_bytes, stats->tx_byte_cnt);
	u64_stats_add(&tstats->tx_packets,  stats->tx_pkt_cnt);
	u64_stats_add(&tstats->rx_bytes, stats->rx_byte_cnt);
	u64_stats_add(&tstats->rx_packets,  stats->rx_pkt_cnt);
#endif

	u64_stats_update_end(&tstats->syncp);

	/*
	 * TODO: Remove the following check when net_device support for
	 * drop counters is added from Kernel for PPE Tunnel stats.
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	atomic_long_add(stats->tx_drop_pkt_cnt, &dev->tx_dropped);
	atomic_long_add(stats->rx_drop_pkt_cnt, &dev->rx_dropped);
#endif
	return true;
}

/*
 * nss_ppe_vxlanmgr_tun_stats_show()
 *	Read Vxlan Tunnel statistics.
 */
int nss_ppe_vxlanmgr_tun_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;

	tun_ctx = kzalloc(sizeof(struct nss_ppe_vxlanmgr_tun_ctx), GFP_KERNEL);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("Failed to allocate memory for tun_ctx");
		return -ENOMEM;
	}

	spin_lock_bh(&vxlan_ctx.tun_lock);
	memcpy(tun_ctx, m->private, sizeof(struct nss_ppe_vxlanmgr_tun_ctx));
	spin_unlock_bh(&vxlan_ctx.tun_lock);

	/*
	 * Tunnel stats
	 * PPE currently supports only the default source port range (49152 to 65535)
	 */
	seq_printf(m, "\n%s tunnel config details start:\n\n", tun_ctx->parent_dev->name);

	seq_printf(m, "\tparent dev: %s\n", tun_ctx->parent_dev->name);
	seq_printf(m, "\tdest_port = %u\n", be16_to_cpu(tun_ctx->dest_port));
	seq_printf(m, "\ttunnel_flags = 0x%x\n", tun_ctx->tunnel_flags);
	seq_printf(m, "\tsrc_port_min = %u\n", NSS_PPE_VXLAN_SPORT_BASE);
	seq_printf(m, "\tsrc_port_max = %u\n", NSS_PPE_VXLAN_SPORT_MASK);

	/*
	 * Get the nss netdevices detail from vxlan db.
	 */
	nss_ppe_vxlanmgr_read_tunnel_config(tun_ctx->parent_dev, m);

	seq_printf(m, "%s tunnel config details end.\n\n", tun_ctx->parent_dev->name);
	kfree(tun_ctx);
	return 0;
}

/*
 * nss_ppe_vxlanmgr_tun_nack_limit_write()
 *	vxlan write handler
 */
static ssize_t nss_ppe_vxlanmgr_tun_nack_limit_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[NSS_PPE_NACK_LIMIT_BUFFER_SIZE] = {0};
	uint16_t res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0 || size >= NSS_PPE_NACK_LIMIT_BUFFER_SIZE) {
		nss_ppe_vxlanmgr_warn("%px: Error reading the input for vxlan configuration", f);
		return size;
	}

	nss_ppe_vxlanmgr_trace("%px: data:%s", f, data);

	status = kstrtou16(data, 10, &res);
	if (status) {
		res = vxlan_ctx.nack_limit;
		nss_ppe_vxlanmgr_warn("%px: Error reading the input for vxlan configuration. status:%d", f, status);
		return status;
	}

	 vxlan_ctx.nack_limit = res;

	return len;
}

/*
 * nss_ppe_vxlanmgr_tun_nack_limit_read()
 *	vxlan read handler
 */
static ssize_t nss_ppe_vxlanmgr_tun_nack_limit_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[NSS_PPE_NACK_LIMIT_BUFFER_SIZE];

	len = snprintf(lbuf, sizeof(lbuf), "vxlan nack limit %u\n", vxlan_ctx.nack_limit);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * nss_ppe_vxlanmgr_tun_stats_open()
 *	VXLAN open the tunnel stats file.
 */
int nss_ppe_vxlanmgr_tun_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nss_ppe_vxlanmgr_tun_stats_show, inode->i_private);
}

/*
 * nss_ppe_vxlanmgr_tun_stats_dentry_remove()
 *	Remove debugfs file for given tunnel context.
 */
void nss_ppe_vxlanmgr_tun_stats_dentry_remove(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	if (vxlan_ctx.dentry && tun_ctx->dentry) {
		debugfs_remove(tun_ctx->dentry);
	}
}

/*
 * nss_ppe_vxlanmgr_nack_ops
 *	File operations for VxLAN tunnel nack-limit file.
 */
static const struct file_operations nss_ppe_vxlanmgr_nack_ops = { \
	.owner = THIS_MODULE,
	.write = nss_ppe_vxlanmgr_tun_nack_limit_write,
	.read = nss_ppe_vxlanmgr_tun_nack_limit_read,
};

/*
 * nss_ppe_vxlanmgr_tun_nack_dentry_create()
 *	Create dentry for a given tunnel.
 */
bool nss_ppe_vxlanmgr_tun_nack_dentry_create(struct dentry *vxlanmgr)
{
	if (!debugfs_create_file("nack_limit", 0644, vxlanmgr, NULL, &nss_ppe_vxlanmgr_nack_ops)) {
		nss_ppe_vxlanmgr_warn("Debugfs file creation failed for nack limit");
		return false;
	}
	return true;
}

/*
 * nss_ppe_vxlanmgr_tun_stats_ops
 *     File operations for VxLAN tunnel stats.
 */
static const struct file_operations nss_ppe_vxlanmgr_tun_stats_ops = { \
	.open = nss_ppe_vxlanmgr_tun_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * nss_ppe_vxlanmgr_tun_stats_dentry_create()
 *	Create dentry for a given tunnel.
 */
bool nss_ppe_vxlanmgr_tun_stats_dentry_create(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	char dentry_name[IFNAMSIZ];

	scnprintf(dentry_name, sizeof(dentry_name), "%s", tun_ctx->parent_dev->name);
	tun_ctx->dentry = debugfs_create_file(dentry_name, S_IRUGO,
			vxlan_ctx.dentry, tun_ctx, &nss_ppe_vxlanmgr_tun_stats_ops);
	if (!tun_ctx->dentry) {
		nss_ppe_vxlanmgr_warn("Debugfs file creation failed for tun %s", tun_ctx->parent_dev->name);
		return false;
	}
	return true;
}

/*
 * nss_ppe_vxlanmgr_tun_stats_dentry_init()
 *	Create VxLAN tunnel statistics debugfs entry.
 */
bool nss_ppe_vxlanmgr_tun_stats_dentry_init(struct dentry *clients)
{
	vxlan_ctx.dentry = debugfs_create_dir("vxlanmgr", clients);
	if (!vxlan_ctx.dentry) {
		nss_ppe_vxlanmgr_warn("Creating debug directory failed");
		return false;
	}

	return true;
}

/*
 * nss_ppe_vxlanmgr_tun_dentry_init()
 *	Create dentry for a given tunnel.
 */
bool nss_ppe_vxlanmgr_tun_dentry_init()
{
	/*
	 * initialize debugfs.
	 */
	struct dentry *parent;
	struct dentry *clients;
	struct dentry *vxlanmgr;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		nss_ppe_vxlanmgr_warn("parent debugfs entry for qca-nss-ppe not present");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		nss_ppe_vxlanmgr_warn("clients debugfs entry inside qca-nss-ppe not present");
		return false;
	}

	if (!nss_ppe_vxlanmgr_tun_stats_dentry_init(clients)) {
		return false;
	}

	vxlanmgr = debugfs_lookup("vxlanmgr", clients);
	if (!clients) {
		nss_ppe_vxlanmgr_warn("clients debugfs entry inside qca-nss-ppe not present");
		return false;
	}

	if (!nss_ppe_vxlanmgr_tun_nack_dentry_create(vxlanmgr)) {
		nss_ppe_vxlanmgr_warn("Dentry for nack creation failed");
		return false;
	}

	return true;
}

/*
 * nss_ppe_vxlanmgr_tun_stats_dentry_deinit()
 *	Cleanup the debugfs tree.
 */
void nss_ppe_vxlanmgr_tun_stats_dentry_deinit()
{
	debugfs_remove_recursive(vxlan_ctx.dentry);
	vxlan_ctx.dentry = NULL;
}
