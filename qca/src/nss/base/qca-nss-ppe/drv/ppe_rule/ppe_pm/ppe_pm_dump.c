/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_pm.h"
#include "ppe_pm_dump.h"

/*
 * PM direction strings
 */
static char *ppe_pm_dump_pm_dir_str[] = {
	"invalid",
	"ingress",
	"egress",
};

/*
 * PM rule direction strings
 */
static char *ppe_pm_dump_rule_dir_str[] = {
	"invalid",
	"upstream",
	"downstream",
};

/*
 * PM port type strings
 */
static char *ppe_pm_dump_port_type_str[] = {
	"bitmap",
	"port",
	"gemport",
};

/*
 * PM tag format strings
 */
static char *ppe_pm_dump_tag_format_str[] = {
	"untagged",
	"priority",
	"tagged",
	"pri_untag",
	"tag_untag",
	"pri_tag",
	"all",
};

/*
 * PM IPMC type strings
 */
static char *ppe_pm_dump_ipmc_type_str[] = {
	"non_ipmc",
	"ipv4_mc",
	"ipv6_mc",
	"nonip_ipv4_mc",
	"nonip_ipv6_mc",
	"ipv4_ipv6_mc",
	"all",
};

/*
 * ppe_pm_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_pm_dump_write_reset(struct ppe_pm_dump_instance *pdi, char *base_prefix)
{
	int result;

	pdi->msgp = pdi->msg;
	pdi->msg_len = 0;

	result = snprintf(pdi->prefix, PPE_PM_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_PM_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	pdi->prefix_level = 0;
	pdi->prefix_levels[pdi->prefix_level] = result;

	return 0;
}

/*
 * ppe_pm_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_pm_dump_prefix_add(struct ppe_pm_dump_instance *pdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pdi->prefix_levels[pdi->prefix_level];
	pxremain = PPE_PM_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(pdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pdi->prefix_level++;
	pdi->prefix_levels[pdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_pm_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_pm_dump_prefix_index_add(struct ppe_pm_dump_instance *pdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pdi->prefix_levels[pdi->prefix_level];
	pxremain = PPE_PM_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(pdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pdi->prefix_level++;
	pdi->prefix_levels[pdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_pm_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_pm_dump_prefix_remove(struct ppe_pm_dump_instance *pdi)
{
	int pxsz;

	pdi->prefix_level--;
	pxsz = pdi->prefix_levels[pdi->prefix_level];
	pdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_pm_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_pm_dump_write(struct ppe_pm_dump_instance *pdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_PM_DUMP_BUFFER_SIZE - pdi->msg_len;
	ptr = pdi->msg + pdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", pdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	pdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	pdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	pdi->msg_len += result;
	return 0;
}

/*
 * ppe_pm_dump_one()
 *	Fill pm_gen dump information for one pm gen rule.
 */
int ppe_pm_dump_one(struct ppe_pm_dump_instance *pdi, struct ppe_pm_gen *pm_gen)
{
	int result;
	struct ppe_pm_counter_gen *r;
	r = &pm_gen->rule;

	/*
	 * Current PM gen count.
	 */
	if ((result = ppe_pm_dump_prefix_index_add(pdi, pdi->pm_gen_cnt))) {
		goto error;
	}

	/*
	 * Rule information
	 */
	if ((result = ppe_pm_dump_write(pdi, "counter_id", "%d", r->counter_id))) {
                        goto error;
                }

	if ((result = ppe_pm_dump_write(pdi, "stats.ucast_pkts", "%lld", atomic64_read(&pm_gen->counter_info->ucast_packet)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.bcast_pkts", "%lld", atomic64_read(&pm_gen->counter_info->bcast_packet)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.mcast_pkts", "%lld", atomic64_read(&pm_gen->counter_info->mcast_packet)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.oversize", "%lld", atomic64_read(&pm_gen->counter_info->oversize)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.octets", "%lld", atomic64_read(&pm_gen->counter_info->octets)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.frame_64", "%lld", atomic64_read(&pm_gen->counter_info->frame_64)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.frame_65_127", "%lld", atomic64_read(&pm_gen->counter_info->frame_65_127)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.frame_128_255", "%lld", atomic64_read(&pm_gen->counter_info->frame_128_255)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.frame_256_511", "%lld", atomic64_read(&pm_gen->counter_info->frame_256_511)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.frame_512_1023", "%lld", atomic64_read(&pm_gen->counter_info->frame_512_1023)))) {
		goto error;
	}

	if ((result = ppe_pm_dump_write(pdi, "stats.frame_1024_1518", "%lld", atomic64_read(&pm_gen->counter_info->frame_1024_1518)))) {
		goto error;
	}

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_RULE_DIR) {
		if ((result = ppe_pm_dump_write(pdi, "rule_dir", "%s", ppe_pm_dump_rule_dir_str[r->rule_dir]))) {
			goto error;
		}
	}

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_PM_DIR) {
                if ((result = ppe_pm_dump_write(pdi, "pm_dir", "%s", ppe_pm_dump_pm_dir_str[r->pm_dir]))) {
                        goto error;
                }
        }

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_PORT_TYPE) {
                if ((result = ppe_pm_dump_write(pdi, "port_type", "%s", ppe_pm_dump_port_type_str[r->port_type]))) {
                        goto error;
                }
        }

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_PORT_INFO) {
		switch (r->port_type) {
		case PPE_PM_PORT_TYPE_BITMAP:
			if ((result = ppe_pm_dump_write(pdi, "port_bitmap", "%d", r->port.port_bitmap))) {
				goto error;
			}
			break;
		case PPE_PM_PORT_TYPE_PORT:
			if ((result = ppe_pm_dump_write(pdi, "dev_name", "%s", r->port.dev_name))) {
				goto error;
			}
			break;
		case PPE_PM_PORT_TYPE_GEMPORT:
			if ((result = ppe_pm_dump_write(pdi, "gem_port", "%d", r->port.gem_port))) {
				goto error;
			}
			break;
		default:
			ppe_pm_warn("Invalid port type\n");
		}
	}

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_TAG_FORMAT) {
                if ((result = ppe_pm_dump_write(pdi, "tag_format", "%s", ppe_pm_dump_tag_format_str[r->tag_format]))) {
                        goto error;
                }
        }

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_VID) {
                if ((result = ppe_pm_dump_write(pdi, "vid", "%d", r->vid))) {
                        goto error;
                }
        }

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_PCP) {
                if ((result = ppe_pm_dump_write(pdi, "pcp", "%d", r->pcp))) {
                        goto error;
                }
        }

	if(r->rule_flags & PPE_PM_GEN_RULE_FLAG_IPMC) {
                if ((result = ppe_pm_dump_write(pdi, "ipmc", "%s", ppe_pm_dump_ipmc_type_str[r->ipmc]))) {
                        goto error;
                }
        }

	/*
	 * Remove the index prefix for next interation
	 */
	if ((result = ppe_pm_dump_prefix_remove(pdi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_pm_dump_all()
 *      Prepare pm dump information for all the active pm rules.
 */
static bool ppe_pm_dump_all(struct ppe_pm_dump_instance *pdi)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	struct ppe_pm_gen *pm_gen;
	int result;

	if ((result = ppe_pm_dump_write_reset(pdi, "pm"))) {
		return result;
	}

	/*
	 * Get the first available pm gen rule.
	 */
	spin_lock_bh(&pm_g->lock);
	if (!list_empty(&pm_g->active_rules)) {
		pdi->pm_gen_cnt = 0;
		list_for_each_entry(pm_gen, &pm_g->active_rules, list) {
			result = ppe_pm_dump_one(pdi, pm_gen);
			if (result < 0) {
				ppe_pm_warn("%p: failed to collect dump for pm gen: %p", pm_g, pm_gen);
				spin_unlock_bh(&pm_g->lock);
				return result;
			}

			pdi->pm_gen_cnt++;
		}
	}

	spin_unlock_bh(&pm_g->lock);

	return 0;
}

/*
 * ppe_pm_dump_dev_open()
 *      Open the character device file for pm dump.
 */
static int ppe_pm_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_pm_dump_instance *pdi;
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	/*
	 * Allocate state information for the reading
	 */
	ppe_pm_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	pdi = (struct ppe_pm_dump_instance *)kzalloc(sizeof(struct ppe_pm_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!pdi) {
		ppe_pm_warn("%p: unable to allocate memory for dump instance", pm_g);
		return -ENOMEM;
	}

	pdi->dump_en = true;
	file->private_data = pdi;

	return 0;
}

/*
 * ppe_pm_dump_dev_release()
 *      Close the character device file for pm dump.
 */
static int ppe_pm_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_pm_dump_instance *pdi = (struct ppe_pm_dump_instance *)file->private_data;

	if (pdi) {
		kfree(pdi);
	}

	return 0;
}

/*
 * ppe_pm_dump_dev_read()
 *      Read file operation handler
 */
static ssize_t ppe_pm_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_pm_dump_instance *pdi;
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	pdi = (struct ppe_pm_dump_instance *)file->private_data;
	if (!pdi) {
		ppe_pm_warn("%p: unable to find dump instance", pm_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (pdi->msg_len) {
		goto read_output;
	}

	if (pdi->dump_en) {
		if (ppe_pm_dump_all(pdi)) {
			ppe_pm_warn("Failed to create pm dump\n");
			return -EIO;
		}

		pdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = pdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, pdi->msgp, bytes_read)) {
		return -EIO;
	}

	pdi->msg_len -= bytes_read;
	pdi->msgp += bytes_read;

	ppe_pm_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			pdi, bytes_read, pdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_pm_dump_dev_write()
 *      Write file operation handler
 */
static ssize_t ppe_pm_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with pm dump character device
 */
static struct file_operations ppe_pm_dump_fops = {
	.read = ppe_pm_dump_dev_read,
	.write = ppe_pm_dump_dev_write,
	.open = ppe_pm_dump_dev_open,
	.release = ppe_pm_dump_dev_release
};

/*
 * ppe_pm_dump_exit()
 * 	Unregister character device for pm dump
 */
void ppe_pm_dump_exit(void)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	unregister_chrdev(pm_g->pm_dump_major_id, "ppe_pm_dump_dev");
}

/*
 * ppe_pm_dump_init()
 * 	Register a character device for pm dump
 */
bool ppe_pm_dump_init(struct dentry *dentry)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	int dev_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!debugfs_create_u32("ppe_pm_dump", S_IRUGO, pm_g->dentry, (u32 *)&pm_g->pm_dump_major_id)) {
		ppe_pm_warn("%p: Failed to create PM dump dev major file in debugfs\n", pm_g);
		return false;
	}
#else
	debugfs_create_u32("ppe_pm_dump", S_IRUGO, pm_g->dentry, (u32 *)&pm_g->pm_dump_major_id);
#endif

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_pm_dump_dev", &ppe_pm_dump_fops);
	if (dev_id < 0) {
		ppe_pm_warn("%p: Failed to register chrdev %d\n", pm_g, dev_id);
		return false;
	}

	pm_g->pm_dump_major_id = dev_id;
	ppe_pm_trace("%p: pm dump dev major id %d\n", pm_g, pm_g->pm_dump_major_id);

	return true;
}
