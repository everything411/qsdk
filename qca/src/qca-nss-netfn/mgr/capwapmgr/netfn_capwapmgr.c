/*
 * netfn_capwapmgr.c
 *	Network function's CAPWAP manager Initialization.
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/bitmap.h>

#include <netfn_capwapmgr.h>
#include "netfn_capwapmgr_priv.h"

/*
 * Global capwapmanager object.
 */
struct netfn_capwapmgr g_mgr;

/*
 * netfn_capwapmgr_stats_str
 *	Stats strings.
 */
static const char *netfn_capwapmgr_stats_str[] = {
	"WAN Net device NULL",
	"Top Net Device NULL",
	"Unsupported tuple type",
	"Unsupported L4 protocol",
	"CAPWAP offload tunnel alloc failed",
	"CAPWAP offload tunnel free failed",
	"Failed to create flow rule",
	"Failed to destroy flow rule",
	"CAPWAP offload tunnel id alloc failed",
	"CAPWAP offload tunnel id free failed",
	"CAPWAP offload tunnel context get failed",
	"CAPWAP offlaod tunid dev free failed",
	"CAPWAP offlaod tunid dtls alloc failed",
	"CAPWAP offlaod tunid dev bind failed",
	"Failed to get tunnel stats",
	"Invalid tunnel create configuration ",
	"Updating tunnel config when tunnel is enabled ",
	"Failed to Deinitialize the tunnel ",
	"Failed to Initialize the tunnel ",
	"Invalid DTLS configuration ",
	"DTLS Session Switch failed ",
	"DTLS Encap Session Add Failed ",
	"DTLS Decap session Add Failed ",
	"DTLS Tunnel is not configured ",
	"DTLS Tunnel enabled ",
	"Tunnel ID inactive ",
	"Tunnel ID out of range ",
	"Parent Dev Delete Failed : Active tunnels",
	"Flow cookie DB alloc failed",
	"Flow cookie add failed",
	"Flow cookie delete failed",
	"Invalid CAPWAP config",
};

/*
 * netfn_capwapmgr_final
 *	Mark final deref completion.
 */
void netfn_capwapmgr_final(struct kref *kref)
{
        struct netfn_capwapmgr *mgr = container_of(kref, struct netfn_capwapmgr, ref);

        complete(&mgr->completion);
}

/*
 * netfn_capwapmgr_stats_print
 *	API to print error statistics.
 */
static int netfn_capwapmgr_stats_print(struct seq_file *sf, void *ptr)
{

          struct netfn_capwapmgr *mgr = &g_mgr;
          uint64_t error_count;
          int i = 0;

          seq_puts(sf, "\n################ CAPWAPMGR statistics Start ################\n");
          seq_printf(sf, "\tTunnels allocated: %u\n", atomic_read(&mgr->stats.tun_dev_alloc));
          seq_printf(sf, "\tTunnels freed: %u\n", atomic_read(&mgr->stats.tun_dev_free));
          seq_printf(sf, "\tTunid dev allocated : %u\n", atomic_read(&mgr->stats.tunid_dev_alloc));
          seq_printf(sf, "\tTunid dev freed: %u\n", atomic_read(&mgr->stats.tunid_dev_free));
          seq_puts(sf, "\n");

          seq_puts(sf, "Error Statistics\n");

          for (i = 1; i < NETFN_CAPWAPMGR_ERROR_MAX; i++) {
                  error_count = atomic64_read(&mgr->stats.error_stats[i]);
                  seq_printf(sf, "\t[%s]: %llu\n", netfn_capwapmgr_stats_str[i-1],
                                                                  error_count);
          }

          seq_puts(sf, "\n################ CAPWAPMGR Statistics End  ################\n");

          return 0;
}


/*
 * netfn_capwapmgr_snap_read()
 *	Read Snap header
 */
static ssize_t netfn_capwapmgr_snap_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[26];
	uint64_t snap;

	memcpy(&snap, netfn_capwapmgr_snap, NETFN_CAPWAP_SNAP_HDR_LEN);
	len = snprintf(lbuf, sizeof(lbuf), "%llx\n", snap);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * netfn_capwapmgr_snap_write()
 *	Write snap header
 */
static ssize_t netfn_capwapmgr_snap_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	uint64_t res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		netfn_capwapmgr_warn("Error reading snap header from debugfs");
		return size;
	}

	status = kstrtou64(data, 16, &res);
	if (status) {
		netfn_capwapmgr_warn("Error updating snap header from debugfs");
		return status;
	}

	memcpy(netfn_capwapmgr_snap, &res, NETFN_CAPWAP_SNAP_HDR_LEN);
	return len;
}

/*
 * netfn_capwapmgr_snap_ops
 *	File handler to configure snap header
 */
const struct file_operations netfn_capwapmgr_snap_ops = {
	.owner = THIS_MODULE,
	.write = netfn_capwapmgr_snap_write,
	.read = netfn_capwapmgr_snap_read
};

/*
 * netfn_capwapmgr_stats_write()
 *	Clear netfn capwapmgr statistics.
 */
static ssize_t netfn_capwapmgr_stats_write(struct file *filep, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct netfn_capwapmgr *mgr = &g_mgr;
	uint32_t reset;

	if (kstrtou32_from_user(buffer, count, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	memset(&mgr->stats, 0, sizeof(struct netfn_capwapmgr_stats));

	return count;
}

/*
 * netfn_capwapmgr_stats_open
 *	Open file handler for common stats debugfs.
 */
static int netfn_capwapmgr_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, netfn_capwapmgr_stats_print, inode->i_private);
}

/*
 * netfn_capwapmgr_stats_ops
 *	File operations for capwapmanager common stats
 */
const struct file_operations netfn_capwapmgr_stats_ops = {
	.open = netfn_capwapmgr_stats_open,
	.write = netfn_capwapmgr_stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * netfn_capwapmgr_init_module()
 *	netfn capwapmgr module init function
 */
int __init netfn_capwapmgr_init_module(void)
{
	struct netfn_capwapmgr *mgr = &g_mgr;

	/*
	 * Deref: netfn_capwapmgr_exit_module().
	 */
	init_completion(&mgr->completion);
	kref_init(&mgr->ref);
	mutex_init(&mgr->lock);

#if defined(NETFN_CAPWAPMGR_ONE_NETDEV)
	netfn_capwapmgr_dev = netfn_capwapmgr_tunid_dev_alloc();
	if (!netfn_capwapmgr_dev) {
		netfn_capwapmgr_warn("Failed to Capwap Net Device\n");
		return -1;
	}
#endif

        /*
         * Create a debugfs entry for capwap netfn engine.
         */
        mgr->dentry = debugfs_create_dir("qca-nss-netfn-capwapmgr", NULL);
        if (!mgr->dentry) {
                netfn_capwapmgr_warn("%p, Unable to create debugsfs entry for NETFN capwapmgr\n", mgr);
        }

	if (!debugfs_create_file("stats", S_IRUGO, mgr->dentry,
                NULL, &netfn_capwapmgr_stats_ops)) {
		netfn_capwapmgr_warn("%p: Unable to create error statistics file entry in debugfs\n", mgr);
	}

	if (!debugfs_create_file("snap_hdr", (S_IRUGO | S_IWUSR), mgr->dentry,
                NULL, &netfn_capwapmgr_snap_ops)) {
		netfn_capwapmgr_warn("%p: Unable to create debugfs entry for snap header\n", mgr);
	}

	netfn_capwapmgr_info("NETFN CAPWAPMGR loaded: (%s)\n", NETFN_CAPWAPMGR_BUILD_ID);
        return 0;
}

/*
 * netfn_capwapmgr_exit_module()
 *	netfn capwapmgr exit module function
 */
void __exit netfn_capwapmgr_exit_module(void)
{
	struct netfn_capwapmgr *mgr = &g_mgr;

#if defined(NETFN_CAPWAPMGR_ONE_NETDEV)
	netfn_capwapmgr_tunid_dev_free(netfn_capwapmgr_dev);
#endif

	/*
	 * Ref: netfn_capwapmgr_init_module().
	 */
	netfn_capwapmgr_deref(mgr);

	/*
	 * Wait for all the derefs.
	 * Reference is taken on driver during tunnel allocations.
	 */
	wait_for_completion(&mgr->completion);

	/*
	 * Remove debugfs entries
	 */
	debugfs_remove_recursive(mgr->dentry);
}

module_init(netfn_capwapmgr_init_module);
module_exit(netfn_capwapmgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NETFN CAPWAP manager");
