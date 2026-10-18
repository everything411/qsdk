/*
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
#include <linux/kernel.h>
#include <linux/debugfs.h>

#include "nss_dscpstats.h"
#include "exports/nss_dscpstats_nl_public.h"
#include "nss_dscpstats_nl.h"

/*
 * nss_dscpstats_stop_ops()
 *	dscpstats stop handler
 */
static ssize_t nss_dscpstats_stop_ops(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		nss_dscpstats_warn("Error reading the input for l2tp configuration\n");
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		nss_dscpstats_warn("Error reading the input for l2tp configuration\n");
		return status;
	}

	nss_dscpstats_db_stop();

	return len;
}

/*
 * nss_dscpstats_db_stats_ops()
 *	Fill mac and dscp values to user buffer.
 */
static ssize_t nss_dscpstats_mac_db_stats_read(struct file *f, char *buffer, size_t len, loff_t *offset)
{
	struct nss_dscpstats_dbg_read_ctx *read_ctx = (struct nss_dscpstats_dbg_read_ctx *)f->private_data;
	uint32_t bytes_read = 0;
	uint32_t hash_index = 0;
	uint32_t bytes = 0;
	uint32_t lbuf_size;
	char *lbuf;

	nss_dscpstats_trace();

	if (read_ctx->read_done) {
		return 0;
	}

	lbuf_size = len + 100;
	lbuf = kmalloc(lbuf_size, GFP_KERNEL);
	if (!lbuf) {
		return -ENOMEM;
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	for (hash_index = read_ctx->hash_index; hash_index < nss_dscpstats_ctx.hash_size; hash_index++) {
		struct hlist_head *mac_list = &nss_dscpstats_ctx.mac_hash_table[hash_index];
		struct nss_dscpstats_mac_node *mac_obj;

		hlist_for_each_entry(mac_obj, mac_list, mac_node) {
			struct hlist_head *dscp_list = &mac_obj->dscp_list;
			struct nss_dscpstats_dscp_node *dscp_obj;

			/*
			 * In very rare scenario it is possible that previously stored mac node could be deleted.
			 * In that case this function will return bytes_read as zero and the read will end. the user
			 * is expected to read again.
			 */
			if (read_ctx->mac_obj && (read_ctx->mac_obj != mac_obj)) {
				continue;
			}
			read_ctx->mac_obj = NULL;

			if (!read_ctx->mac_addr_written) {
				bytes = snprintf(lbuf + bytes_read, lbuf_size - bytes_read, "  MAC : %pM\n", &mac_obj->mac_addr);
				if ((bytes_read + bytes) > len) {
					spin_unlock(&nss_dscpstats_ctx.lock);

					if (copy_to_user(buffer, lbuf, bytes_read)) {
						kfree(lbuf);
						return -EFAULT;
					}

					read_ctx->mac_obj = mac_obj;
					read_ctx->hash_index = hash_index;
					kfree(lbuf);
					return bytes_read;
				}

				bytes_read += bytes;
				read_ctx->mac_addr_written = true;
			}

			hlist_for_each_entry(dscp_obj, dscp_list, dscp_node) {

				if (read_ctx->dscp_obj && (read_ctx->dscp_obj != dscp_obj)) {
					continue;
				}
				read_ctx->dscp_obj = NULL;

				bytes = snprintf(lbuf + bytes_read, lbuf_size - bytes_read, "\t  DSCP %u rxBytes 0x%llx txBytes 0x%llx\n", dscp_obj->dscp,
								 dscp_obj->total_rxBytes, dscp_obj->total_txBytes);
				if ((bytes_read + bytes) > len) {
					spin_unlock(&nss_dscpstats_ctx.lock);

					if (copy_to_user(buffer, lbuf, bytes_read)) {
						kfree(lbuf);
						return -EFAULT;
					}

					read_ctx->dscp_obj = dscp_obj;
					read_ctx->mac_obj = mac_obj;
					read_ctx->hash_index = hash_index;
					kfree(lbuf);
					return bytes_read;
				}
				bytes_read += bytes;
			}

			read_ctx->mac_addr_written = false;
		}
	}
	spin_unlock(&nss_dscpstats_ctx.lock);

	if (copy_to_user(buffer, lbuf, bytes_read)) {
		kfree(lbuf);
		return -EFAULT;
	}
	read_ctx->read_done = true;
	kfree(lbuf);
	return bytes_read;
}

/*
 * nss_dscpstats_mac_db_stats_open()
 *	Allocate memory for read context.
 */
static int nss_dscpstats_mac_db_stats_open(struct inode *inode, struct file *file)
{
	struct nss_dscpstats_dbg_read_ctx *ws;

	ws = kzalloc(sizeof(struct nss_dscpstats_dbg_read_ctx), GFP_KERNEL);
	if (!ws) {
		return -ENOMEM;
	}

	file->private_data = ws;

	return 0;
}

/*
 * nss_dscpstats_mac_db_stats_release()
 *	Free memory allocated for read context.
 */
static int nss_dscpstats_mac_db_stats_release(struct inode *inode, struct file *file)
{
	struct nss_dscpstats_dbg_read_ctx *ws;

	ws = (struct nss_dscpstats_dbg_read_ctx *)file->private_data;
	kfree(ws);
	file->private_data = NULL;

	return 0;
}

/*
 * nss_dscpstats_wan_if_stats_read()
 *	Fill WAN interface and its dscp bitfields to user buffer.
 */
static ssize_t nss_dscpstats_wan_if_stats_read(struct file *f, char *buffer, size_t len, loff_t *offset)
{
	struct nss_dscpstats_wanif *wan_if_node;
	int bytes_read = 0;
	int lbuf_size;
	int cur_read;
	char *lbuf;
	int ret;

	lbuf_size = len + 100;
	lbuf = kmalloc(lbuf_size, GFP_KERNEL);
	if (!lbuf) {
		return -ENOMEM;
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	hlist_for_each_entry(wan_if_node, &nss_dscpstats_ctx.wan_list, wan_node) {
		cur_read = snprintf(lbuf + bytes_read, lbuf_size - bytes_read, "\t%s : DSCP bitfield 0x%llx\n",
							wan_if_node->dev_name, wan_if_node->dscp_mask);
		if ((bytes_read + cur_read) > len) {
			goto out;
		}
		bytes_read += cur_read;
	}

out:
	spin_unlock(&nss_dscpstats_ctx.lock);
	ret = simple_read_from_buffer(buffer, len, offset, lbuf, bytes_read);
	kfree(lbuf);

	return ret;
}

/*
 * nss_dscpstats_common_stats_read()
 *	Fill general statistics to user buffer.
 */
static ssize_t nss_dscpstats_common_stats_read(struct file *f, char *buffer, size_t len, loff_t *offset)
{
	int bytes_read;
	char *lbuf;
	int ret;

	lbuf = kmalloc(len, GFP_KERNEL);
	if (!lbuf) {
		return -ENOMEM;
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	bytes_read = snprintf(lbuf, len,
		"\tTotal MAC add : %u\n\tTotal MAC del : %u\n\tTotal MAC add failure : %u\n\tTotal stats read request : %u\n\tActive MAC : %u\n",
		nss_dscpstats_ctx.common_stats.tot_mac_add, nss_dscpstats_ctx.common_stats.tot_mac_del,
		nss_dscpstats_ctx.common_stats.mac_node_add_fail, nss_dscpstats_ctx.common_stats.tot_read_req,
		atomic_read(&nss_dscpstats_ctx.common_stats.tot_act_mac));
	spin_unlock(&nss_dscpstats_ctx.lock);
	ret = simple_read_from_buffer(buffer, len, offset, lbuf, bytes_read);
	kfree(lbuf);

	return ret;
}

/*
 * nss_dscpstats_stop_file_ops
 *	File operations for stopping the statistics update.
 */
const struct file_operations nss_dscpstats_stop_file_ops = {
	.owner = THIS_MODULE,
	.write = nss_dscpstats_stop_ops,
};

/*
 * nss_dscpstats_mac_db_file_ops
 *	file operations for reading the MAC database statistics.
 */
const struct file_operations nss_dscpstats_mac_db_file_ops = {
	.owner = THIS_MODULE,
	.open = nss_dscpstats_mac_db_stats_open,
	.read = nss_dscpstats_mac_db_stats_read,
	.release = nss_dscpstats_mac_db_stats_release,
};

/*
 * nss_dscpstats_wan_if_stats_file_ops
 *	file operations for reading the traffic count enabled WAN interfaces.
 */
const struct file_operations nss_dscpstats_wan_if_stats_file_ops = {
	.read = nss_dscpstats_wan_if_stats_read,
};

/*
 * nss_dscpstats_common_stats_file_ops
 *	file operations for reading the general statistics.
 */
const struct file_operations nss_dscpstats_common_stats_file_ops = {
	.read = nss_dscpstats_common_stats_read,
};

/*
 * nss_dscpstats_create_debugfs
 *	create debugfs entries.
 */
static bool nss_dscpstats_create_debugfs(void) {
	nss_dscpstats_ctx.dentry = debugfs_create_dir("qca-nss-dscpstats", NULL);
	if (nss_dscpstats_ctx.dentry == NULL) {
		nss_dscpstats_warn("Unable to create qca-nss-dscpstats directory in debugfs\n");
		return false;
	}

	nss_dscpstats_ctx.conf_dentry = debugfs_create_dir("conf", nss_dscpstats_ctx.dentry);
	if (nss_dscpstats_ctx.conf_dentry == NULL) {
		nss_dscpstats_warn("Unable to create conf directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("stop", 0200, nss_dscpstats_ctx.conf_dentry,
					NULL, &nss_dscpstats_stop_file_ops)) {
		nss_dscpstats_warn("Unable to create flush file in debugfs\n");
		goto debugfs_dir_failed;
	}

	nss_dscpstats_ctx.stats_dentry = debugfs_create_dir("stats", nss_dscpstats_ctx.dentry);
	if (nss_dscpstats_ctx.stats_dentry == NULL) {
		nss_dscpstats_warn("Unable to create statistics directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("dscpstats", 0400, nss_dscpstats_ctx.stats_dentry,
					NULL, &nss_dscpstats_mac_db_file_ops)) {
		nss_dscpstats_warn("Unable to create traffic file in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("wan_enabled", 0400, nss_dscpstats_ctx.stats_dentry,
					NULL, &nss_dscpstats_wan_if_stats_file_ops)) {
		nss_dscpstats_warn("Unable to create wan_if file in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("common_stats", 0400, nss_dscpstats_ctx.stats_dentry,
					NULL, &nss_dscpstats_common_stats_file_ops)) {
		nss_dscpstats_warn("Unable to create common_stats file in debugfs\n");
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(nss_dscpstats_ctx.dentry);
	nss_dscpstats_ctx.dentry = NULL;
	return 1;
}

/*
 * nss_dscpstats_remove_debugfs
 *	remove debugfs entries.
 */
static void nss_dscpstats_remove_debugfs(void) {
	debugfs_remove_recursive(nss_dscpstats_ctx.dentry);
}

/*
 * qca_nss_dscp_stat_init()
 *	register netlink ops and initialize hash table
 *
 * returns 0 on successful initialization else return -1
 */
int __init nss_dscpstats_init(void)
{
	int error = 0;

	nss_dscpstats_info("Init NSS client dscp stats handler\n");

	if (num_clients < NSS_DSCPSTATS_CLIENT_HASH_RATIO) {
		num_clients = NSS_DSCPSTATS_CLIENT_HASH_RATIO;
	}

	error = nss_dscpstats_db_init_table();
	if (error != 0) {
		return error;
	}

	error = nss_dscpstats_nl_init();
	if (error != 0) {
		nss_dscpstats_db_free_table();
		return error;
	}

	error = nss_dscpstats_create_debugfs();
	if (error != 0) {
		nss_dscpstats_nl_exit();
		nss_dscpstats_db_free_table();
		return error;
	}

	nss_dscpstats_info("Init dscpstats complete\n");
	return 0;
}

/*
 * qca_nss_dscp_stat_init()
 *	unregister netlink ops and free the hash table
 *
 */
void __exit nss_dscpstats_exit(void)
{
	nss_dscpstats_info("Exit dscpstats complete\n");

	nss_dscpstats_remove_debugfs();

	nss_dscpstats_nl_exit();

	/*
	 * free memory assigned to hashtable
	 */
	nss_dscpstats_db_free_table();
}

module_init(nss_dscpstats_init);
module_exit(nss_dscpstats_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DSCP STAT");

module_param(num_clients, int, 0644);
MODULE_PARM_DESC(num_clients, "Number of clients supported");
