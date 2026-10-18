/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <ppe_qos.h>
#include "ppe_qos.h"
#include "ppe_qos_dump.h"

/*
 * ppe_qos_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_qos_dump_write_reset(struct ppe_qos_dump_instance *pqdi, char *base_prefix)
{
	int result;

	pqdi->msgp = pqdi->msg;
	pqdi->msg_len = 0;

	result = snprintf(pqdi->prefix, PPE_QOS_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_QOS_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	pqdi->prefix_level = 0;
	pqdi->prefix_levels[pqdi->prefix_level] = result;

	return 0;
}

/*
 * ppe_qos_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_qos_dump_prefix_add(struct ppe_qos_dump_instance *pqdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pqdi->prefix_levels[pqdi->prefix_level];
	pxremain = PPE_QOS_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(pqdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pqdi->prefix_level++;
	pqdi->prefix_levels[pqdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_qos_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_qos_dump_prefix_index_add(struct ppe_qos_dump_instance *pqdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pqdi->prefix_levels[pqdi->prefix_level];
	pxremain = PPE_QOS_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(pqdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pqdi->prefix_level++;
	pqdi->prefix_levels[pqdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_qos_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_qos_dump_prefix_remove(struct ppe_qos_dump_instance *pqdi)
{
	int pxsz;

	pqdi->prefix_level--;
	pxsz = pqdi->prefix_levels[pqdi->prefix_level];
	pqdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_qos_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_qos_dump_write(struct ppe_qos_dump_instance *pqdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_QOS_DUMP_BUFFER_SIZE - pqdi->msg_len;
	ptr = pqdi->msg + pqdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", pqdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	pqdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	pqdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	pqdi->msg_len += result;
	return 0;
}

/*
 * ppe_qos_dump_one_queue()
 *	Prepare QoS dump information for a queue.
 */
static bool ppe_qos_dump_one_queue(struct ppe_qos_dump_instance *pqdi,
				struct ppe_qos_interface_res *tm_if,
				struct ppe_qos_interface_queue *queue,
				int queue_num,
				ppe_qos_queue_type_t queue_type)
{
	struct ppe_drv_qos_port *port = &tm_if->port;
	uint32_t qid;
	int result;

	/*
	 * Get the appropriate base queue ID based on queue type
	 */
	if (queue_type == PPE_QOS_QUEUE_TYPE_MCAST) {
		qid = port->base[PPE_DRV_QOS_RES_TYPE_MCAST_QUEUE];
	} else {
		qid = port->base[PPE_DRV_QOS_RES_TYPE_UCAST_QUEUE];
	}

	/*
	 * Queue information
	 */
	if ((result = ppe_qos_dump_prefix_add(pqdi, "queue"))) {
		goto error;
	}

	if ((result = ppe_qos_dump_prefix_index_add(pqdi, queue_num))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "type", "%s",
					queue_type == PPE_QOS_QUEUE_TYPE_MCAST ? "mcast" : "ucast"))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "offset", "%d", queue->offset))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "id", "%d", qid + queue->offset))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "priority", "%d", queue->priority))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "weight", "%d", queue->weight))) {
		goto error;
	}

	if (!queue->limit.is_configured) {
		goto prefix_remove;
	}

	if ((result = ppe_qos_dump_write(pqdi, "ceiling", "%d", queue->limit.ceiling))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "color_en", "%s", queue->limit.color_en ? "true" : "false"))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "wred_en", "%s", queue->limit.wred_en ? "true" : "false"))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "green_min_off", "%d", queue->limit.green_min_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "yellow_max_off", "%d", queue->limit.yellow_max_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "yellow_min_off", "%d", queue->limit.yellow_min_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "red_max_off", "%d", queue->limit.red_max_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "red_min_off", "%d", queue->limit.red_min_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "green_resume_off", "%d", queue->limit.green_resume_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "yellow_resume_off", "%d", queue->limit.yellow_resume_off))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "red_resume_off", "%d", queue->limit.red_resume_off))) {
		goto error;
	}

prefix_remove:
	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
		goto error;
	}

	/*
	 * Remove the queue prefix for next interation
	 */
	if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
		goto error;
	}

error:
	return result;

}

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_qos_dump_all_tconts()
 *	Prepare QoS dump information for all the valid tconts.
 */
static bool ppe_qos_dump_all_tconts(struct ppe_qos_dump_instance *pqdi)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_qos_interface_queue *queue;
	int queue_num, i, result;

	/*
	 * Dump all interfaces
	 */
	for (i = 0; i < PPE_DRV_QOS_TCONT_MAX; i++) {
		if (!g_qos->tcont[i].valid) {
			continue;
		}

		tm_if = &g_qos->tcont[i];
		if ((result = ppe_qos_dump_prefix_add(pqdi, "tcont"))) {
			goto error;
		}

		if ((result = ppe_qos_dump_prefix_index_add(pqdi, i))) {
			goto error;
		}

		if ((result = ppe_qos_dump_write(pqdi, "id", "%d", tm_if->id))) {
			goto error;
		}

		if ((result = ppe_qos_dump_write(pqdi, "shaper", "%s", strlen(tm_if->shaper_name) ? tm_if->shaper_name : "None"))) {
			goto error;
		}

		if ((result = ppe_qos_dump_write(pqdi, "num_queues", "%d", tm_if->num_queues))) {
			goto error;
		}

		/*
		 * Dump all queues of the tcont (unicast only for T-cont)
		 */
		queue_num = 0;
		if (!list_empty(&tm_if->q_list)) {
			list_for_each_entry(queue, &tm_if->q_list, list) {
				if(!queue->valid) {
					continue;
				}

				queue_num++;
				result = ppe_qos_dump_one_queue(pqdi, tm_if, queue, queue_num, PPE_QOS_QUEUE_TYPE_UCAST);
				if (result < 0) {
					ppe_qos_warn("%p: failed to collect dump for queue: %p", g_qos, queue);
					return result;
				}
			}
		}

		/*
		 * Remove the 'index' prefix for next interation
		 */
		if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
			goto error;
		}

		/*
		 * Remove the tcont prefix for next interation
		 */
		if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
			goto error;
		}
	}

	return 0;

error:
	return result;
}
#endif

/*
 * ppe_qos_dump_all_interfaces()
 *	Prepare QoS dump information for all the valid interfaces.
 */
static bool ppe_qos_dump_all_interfaces(struct ppe_qos_dump_instance *pqdi)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_interface_res *tm_if = NULL;
	struct ppe_qos_interface_queue *queue;
	struct net_device *dev;
	int queue_num, j, result;

	/*
	 * Dump all interfaces
	 */
	if (!list_empty(&g_qos->port_list)) {
		list_for_each_entry(tm_if, &g_qos->port_list, list) {

			if (!tm_if->valid) {
				continue;
			}

			if ((result = ppe_qos_dump_prefix_add(pqdi, "interface"))) {
				goto error;
			}

			if ((result = ppe_qos_dump_prefix_index_add(pqdi, tm_if->id))) {
				goto error;
			}

			if ((result = ppe_qos_dump_write(pqdi, "type", "%s", "UNI" ))) {
				goto error;
			}

			dev = ppe_drv_port_num_to_dev(tm_if->id);
			if ((result = ppe_qos_dump_write(pqdi, "dev", "%s", dev->name))) {
				goto error;
			}

			/* Display multicast priority map if configured */
			if (tm_if->mcast_prio_map_valid) {
				char mcast_prio_map_str[256];
				int offset = 0;

				/* Format the map as comma-separated values */
				for (j = 0; j < PPE_DRV_MAX_PRIORITY; j++) {
					if (j == 0) {
						offset += snprintf(mcast_prio_map_str + offset, sizeof(mcast_prio_map_str) - offset,
								  "%u", tm_if->mcast_prio_map[j]);
					} else {
						offset += snprintf(mcast_prio_map_str + offset, sizeof(mcast_prio_map_str) - offset,
								  ",%u", tm_if->mcast_prio_map[j]);
					}
				}

				if ((result = ppe_qos_dump_write(pqdi, "mcast_prio_map", "%s", mcast_prio_map_str))) {
					goto error;
				}
			}

			/* Display unicast priority map if configured */
			if (tm_if->ucast_prio_map_valid) {
				char ucast_prio_map_str[256];
				int offset = 0;
					/* Format the map as comma-separated values */
				for (j = 0; j < PPE_DRV_MAX_PRIORITY; j++) {
					if (j == 0) {
						offset += snprintf(ucast_prio_map_str + offset, sizeof(ucast_prio_map_str) - offset,
								  "%u", tm_if->ucast_prio_map[j]);
					} else {
						offset += snprintf(ucast_prio_map_str + offset, sizeof(ucast_prio_map_str) - offset,
								  ",%u", tm_if->ucast_prio_map[j]);
					}
				}

				if ((result = ppe_qos_dump_write(pqdi, "ucast_prio_map", "%s", ucast_prio_map_str))) {
					goto error;
				}
			}

			if ((result = ppe_qos_dump_write(pqdi, "shaper_name", "%s", strlen(tm_if->shaper_name) ? tm_if->shaper_name : "None"))) {
				goto error;
			}

			if ((result = ppe_qos_dump_write(pqdi, "num_queues", "%d", tm_if->num_queues))) {
				goto error;
			}

			if ((result = ppe_qos_dump_write(pqdi, "num_mcast_queues", "%d", tm_if->num_mcast_queues))) {
				goto error;
			}

			/*
			 * Dump all unicast queues of the interface
			 */
			queue_num = 0;
			if (!list_empty(&tm_if->q_list)) {
				list_for_each_entry(queue, &tm_if->q_list, list) {
					if (!queue->valid) {
						continue;
					}

					queue_num++;
					result = ppe_qos_dump_one_queue(pqdi, tm_if, queue, queue_num, PPE_QOS_QUEUE_TYPE_UCAST);
					if (result < 0) {
						ppe_qos_warn("%p: failed to collect dump for queue: %p", g_qos, queue);
						return result;
					}
				}
			}

			/*
			 * Dump all multicast queues of the interface
			 */
			queue_num = 0;
			if (!list_empty(&tm_if->mq_list)) {
				list_for_each_entry(queue, &tm_if->mq_list, list) {
					if (!queue->valid) {
						continue;
					}

					queue_num++;
					result = ppe_qos_dump_one_queue(pqdi, tm_if, queue, queue_num, PPE_QOS_QUEUE_TYPE_MCAST);
					if (result < 0) {
						ppe_qos_warn("%p: failed to collect dump for mcast queue: %p", g_qos, queue);
						return result;
					}
				}
			}

			/*
			 * Remove the 'index' prefix for next interation
			 */
			if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
				goto error;
			}

			/*
			 * Remove the interface prefix for next interation
			 */
			if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
				goto error;
			}
		}
	}

#ifdef NSS_PPE_PON_SUPPORT
	if (g_qos->pon_port) {
		if ((result = ppe_qos_dump_prefix_add(pqdi, "interface"))) {
			goto error;
		}

		if ((result = ppe_qos_dump_prefix_index_add(pqdi, g_qos->pon_port))) {
			goto error;
		}

		if ((result = ppe_qos_dump_write(pqdi, "type", "%s", "ANI" ))) {
			goto error;
		}

		result = ppe_qos_dump_all_tconts(pqdi);
		if (result < 0) {
			ppe_qos_warn("%p: failed to collect dump for tconts", g_qos);
			return result;
		}

		/*
		 * Remove the 'index' prefix for next interation
		 */
		if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
			goto error;
		}

		/*
		 * Remove the interface prefix for next interation
		 */
		if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
			goto error;
		}
	}
#endif

	return 0;

error:
	return result;
}

/*
 * ppe_qos_dump_one_shaper()
 *	Fill QoS dump information for one shaper rule.
 */
int ppe_qos_dump_one_shaper(struct ppe_qos_dump_instance *pqdi, struct ppe_qos_shaper_profile *profile)
{
	int result;
	struct ppe_drv_qos_shaper *shaper;
	shaper = &profile->shaper;

	/*
	 * Rule information
	 */
	if ((result = ppe_qos_dump_prefix_add(pqdi, "shaper"))) {
		goto error;
	}

	/*
	 * Current QoS shaper count.
	 */
	if ((result = ppe_qos_dump_prefix_index_add(pqdi, pqdi->shaper_cnt))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "name", "%s", profile->name))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "cir", "%d", shaper->rate))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "eir", "%d", shaper->crate))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "cbs", "%d", shaper->burst))) {
		goto error;
	}

	if ((result = ppe_qos_dump_write(pqdi, "ebs", "%d", shaper->cburst))) {
		goto error;
	}

	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
		goto error;
	}

	/*
	 * Remove the shaper prefix for next interation
	 */
	if ((result = ppe_qos_dump_prefix_remove(pqdi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_qos_dump_all_shapers()
 *	Prepare QoS dump information for all the active QoS shapers.
 */
static bool ppe_qos_dump_all_shapers(struct ppe_qos_dump_instance *pqdi)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	struct ppe_qos_shaper_profile *profile;
	int result;

	/*
	 * Dump all shaper profiles.
	 */
	if (!list_empty(&g_qos->shaper_list)) {
		pqdi->shaper_cnt = 0;
		list_for_each_entry(profile, &g_qos->shaper_list, list) {
			result = ppe_qos_dump_one_shaper(pqdi, profile);
			if (result < 0) {
				ppe_qos_warn("%p: failed to collect dump for shaper: %p", g_qos, profile);
				return result;
			}

			pqdi->shaper_cnt++;
		}
	}

	return 0;
}

/*
 * ppe_qos_dump_all()
 *	Prepare QoS dump information for all the active QoS shapers.
 */
static bool ppe_qos_dump_all(struct ppe_qos_dump_instance *pqdi)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	int result;

	if ((result = ppe_qos_dump_write_reset(pqdi, "qos"))) {
		return result;
	}

	spin_lock_bh(&g_qos->lock);
	result = ppe_qos_dump_all_shapers(pqdi);
	if (result < 0) {
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%p: failed to collect dump for shapers", g_qos);
		return result;
	}

	result = ppe_qos_dump_all_interfaces(pqdi);
	if (result < 0) {
		spin_unlock_bh(&g_qos->lock);
		ppe_qos_warn("%p: failed to collect dump for interfaces", g_qos);
		return result;
	}

	spin_unlock_bh(&g_qos->lock);
	return 0;
}

/*
 * ppe_qos_dump_dev_open()
 *	Open the character device file for QoS dump.
 */
static int ppe_qos_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_qos_dump_instance *pqdi;
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;

	/*
	 * Allocate state information for the reading
	 */
	ppe_qos_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	pqdi = (struct ppe_qos_dump_instance *)kzalloc(sizeof(struct ppe_qos_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!pqdi) {
		ppe_qos_warn("%p: unable to allocate memory for dump instance", g_qos);
		return -ENOMEM;
	}

	pqdi->dump_en = true;
	file->private_data = pqdi;

	return 0;
}

/*
 * ppe_qos_dump_dev_release()
 *	Close the character device file for QoS dump.
 */
static int ppe_qos_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_qos_dump_instance *pqdi = (struct ppe_qos_dump_instance *)file->private_data;
	if (pqdi) {
		kfree(pqdi);
	}

	return 0;
}

/*
 * ppe_qos_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_qos_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_qos_dump_instance *pqdi;
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;

	pqdi = (struct ppe_qos_dump_instance *)file->private_data;
	if (!pqdi) {
		ppe_qos_warn("%p: unable to find dump instance", g_qos);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (pqdi->msg_len) {
		goto read_output;
	}

	if (pqdi->dump_en) {
		if (ppe_qos_dump_all(pqdi)) {
			ppe_qos_warn("Failed to create QoS dump\n");
			return -EIO;
		}

		pqdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = pqdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, pqdi->msgp, bytes_read)) {
		return -EIO;
	}

	pqdi->msg_len -= bytes_read;
	pqdi->msgp += bytes_read;

	ppe_qos_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			pqdi, bytes_read, pqdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_qos_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_qos_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with dot1p dump character device
 */
static struct file_operations ppe_qos_dump_fops = {
	.read = ppe_qos_dump_dev_read,
	.write = ppe_qos_dump_dev_write,
	.open = ppe_qos_dump_dev_open,
	.release = ppe_qos_dump_dev_release
};

/*
 * ppe_gem-port_dump_exit()
 *	Unregister character device for QoS dump
 */
void ppe_qos_dump_exit(void)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;

	unregister_chrdev(g_qos->qos_dump_major_id, "ppe_qos_dump_dev");
}

/*
 * ppe_qos_dump_init()
 *	Register a character device for QoS dump
 */
bool ppe_qos_dump_init(struct dentry *dentry)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	int dev_id = -1;

	debugfs_create_u32("ppe_qos_dump", S_IRUGO, g_qos->dentry, (u32 *)&g_qos->qos_dump_major_id);

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_qos_dump_dev", &ppe_qos_dump_fops);
	if (dev_id < 0) {
		ppe_qos_warn("%p: Failed to register chrdev %d\n", g_qos, dev_id);
		return false;
	}

	g_qos->qos_dump_major_id = dev_id;
	ppe_qos_trace("%p: qos dump dev major id %d\n", g_qos, g_qos->qos_dump_major_id);

	return true;
}
