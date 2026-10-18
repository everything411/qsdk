/*
 * Copyright (c) 2014, 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <net/sock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "nss_macsec_sdk_api.h"
#include "nss_macsec_fal_api.h"
#include "nss_macsec.h"
#include "nss_macsec_utility.h"

/*
 * SKU shadow: mirrors the first member of qca81xx_private (phydev->priv)
 * and qce1204_shared_priv (phydev->shared->priv). Layout enforced by
 * static_assert in each chip header.
 */
struct qca_sku_info {
	const char *name;
	bool ptp;
	bool macsec;
};

struct qca_common_private {
	struct qca_sku_info sku; /* MUST match first member of chip priv */
};

/* DebugFS root directory */
static struct dentry *macsec_debugfs_root;

/* State string mapping - only used in this file */
static const char *macsec_state_strings[MACSEC_STATE_MAX] = {
	"Initial",
	"Success",
	"Netlink Init Failed",
	"Notifier Registration Failed",
	"Memory Allocation Failed",
	"SecY Initialization Failed",
	"Unregister Failed",
	"Removed"
};

/* Module statistics - only used in this file */
static struct macsec_module_stats macsec_stats = {
	.current_state = MACSEC_INIT,
	.register_count = ATOMIC_INIT(0),
	.unregister_count = ATOMIC_INIT(0),
};

/* Device debug info - shared across files, defined here */
struct macsec_device_debug macsec_device_debug[MACSEC_DEVICE_MAXNUM];

static struct sock *sdk_nl_sk = NULL;

static unsigned int nss_macsec_msg_handle(void *msg_data, unsigned int *sync)
{
	struct sdk_msg_header *header = (struct sdk_msg_header *)msg_data;
	unsigned short cmd_type = header->cmd_type;
	unsigned int ret = 0;

	*sync = 1;
	switch (cmd_type) {
	case SDK_FAL_CMD:{
			ret = nss_macsec_fal_msg_handle(header);
		}
		break;

	default:
		break;
	}

	return ret;
}

static void nss_macsec_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	struct sdk_msg_header *header = NULL;
	void *msg_data;
	u32 pid = 0, msg_type = 0, sync = 0, ret = 0, msg_len = 0;

	if ((skb = skb_clone(__skb, GFP_KERNEL)) != NULL) {
		nlh = nlmsg_hdr(skb);
		pid = nlh->nlmsg_pid;
		msg_data = NLMSG_DATA(nlh);
		msg_type = nlh->nlmsg_type;
		msg_len = sizeof(struct nlmsghdr) + sizeof(struct sdk_msg_header);
		header = (struct sdk_msg_header *)msg_data;
		if (header->buf_len > (U32_MAX - msg_len))
			return;
		msg_len += header->buf_len;

		if(skb->len < msg_len) {
			osal_print("Unexpected msg received! skb_len [0x%x] less than 0x%x\n",
				skb->len, msg_len);
			return;
		} else if (msg_type == SDK_CALL_MSG) {
			ret = nss_macsec_msg_handle(msg_data, &sync);
			header->ret = ret;
		} else {
			osal_print("Unexpected msg:0x%x received!\n", msg_type);
			return;
		}

		NETLINK_CB(skb).portid = 0; /* from kernel */
		NETLINK_CB(skb).dst_group = 0;	/* unicast */
		netlink_unicast(sdk_nl_sk, skb, pid, MSG_DONTWAIT);
	}

	return;
}

static int nss_macsec_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {
			.input	= nss_macsec_netlink_recv,
	};

	sdk_nl_sk = netlink_kernel_create(&init_net, NETLINK_SDK, &cfg);
	if (sdk_nl_sk == NULL) {
		osal_print("Create netlink socket fail\n");
		return -ENODEV;
	}

	return 0;
}

u32 nss_macsec_dt_secy_id_get(u8 *dev_name, u32 *secy_id)
{
	u32 dev_id = INVALID_DEVICE_ID;
	struct net_device *netdev = NULL;

	/* get net device by ifname */
	netdev = __dev_get_by_name(&init_net, (const char *)dev_name);
	if (unlikely(!netdev)) {
		*secy_id = INVALID_DEVICE_ID;
		/* no net device, return not found */
		return ERROR_NOT_FOUND;
	}

	/* get phydev by net device and find secy_id */
	if (netdev->phydev)
		dev_id = macsec_get_device_id(netdev->phydev);

	*secy_id = dev_id;

	/* Update API statistics */
	if (dev_id < MACSEC_DEVICE_MAXNUM) {
		macsec_device_debug[dev_id].api_stats.ifname_to_devid_calls++;
	}

	return ERROR_OK;
}

static void nss_macsec_netlink_fini(void)
{
	if (sdk_nl_sk) {
		sock_release(sdk_nl_sk->sk_socket);
		sdk_nl_sk = NULL;
	}
}

static int nss_macsec_module_status_show(struct seq_file *m, void *v)
{
	u32 device_count = 0;
	int i;
	u32 dev_state;
	const char *state_str;

	/* Part 1: Global module state with aligned format */
	if (macsec_stats.current_state < MACSEC_STATE_MAX) {
		seq_printf(m, "%-27s : %s\n", "NSS_MACsec_Module_State",
			  macsec_state_strings[macsec_stats.current_state]);
	} else {
		seq_printf(m, "%-27s : unknown state %u\n",
				"NSS_MACsec_Module_State",
				macsec_stats.current_state);
	}

	seq_printf(m, "%-27s : %u\n", "register_count",
		   atomic_read(&macsec_stats.register_count));
	seq_printf(m, "%-27s : %u\n", "unregister_count",
		   atomic_read(&macsec_stats.unregister_count));

	/* Get active device count */
	device_count = qca_macsec_get_device_count();
	seq_printf(m, "%-27s : %u\n", "MACsec_device_count", device_count);

	/* Part 2: Device status - show all devices with debug state */
	for (i = 0; i < MACSEC_DEVICE_MAXNUM; i++) {
		if (macsec_device_debug[i].device_state != MACSEC_INIT) {
			dev_state = macsec_device_debug[i].device_state;
			state_str = (dev_state < MACSEC_STATE_MAX) ?
				    macsec_state_strings[dev_state] : "unknown";

			seq_printf(m, "Device %d\n", i);
			seq_printf(m, "    %-23s : %s\n", "Interface",
				   macsec_device_debug[i].interface_name);
			seq_printf(m, "    %-23s : \"%s\"\n", "Driver",
				   macsec_device_debug[i].drv_name);
			seq_printf(m, "    %-23s : %s\n", "State", state_str);
		}
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(nss_macsec_module_status);


/* Helper function to get SC-SA mapping mode string */
static const char *get_sc_sa_mapping_string(
	enum macsec_sc_sa_mapping_mode_t mode)
{
	switch (mode) {
	case MACSEC_SC_SA_MAP_1_1:
		return "1:1";
	case MACSEC_SC_SA_MAP_1_2:
		return "1:2";
	case MACSEC_SC_SA_MAP_1_4:
		return "1:4";
	default:
		return "unknown";
	}
}

/* Helper function to format SCI (MAC address + Port ID) */
static void format_sci_string(char *buf, size_t buf_size, const u8 *sci)
{
	snprintf(buf, buf_size, "%02x:%02x:%02x:%02x:%02x:%02x:%04x",
		 sci[0], sci[1], sci[2], sci[3], sci[4], sci[5],
		 (sci[6] << 8) | sci[7]);
}

/* Helper function to format rule mask meaning */
static void format_rule_mask_string(char *buf, size_t buf_size, u32 mask)
{
	char *pos = buf;
	size_t remaining = buf_size;
	int written = 0;
	bool first = true;

	if (mask & SECY_RULE_MASK_DA) {
		written = snprintf(pos, remaining, "%sDA", first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_SA) {
		written = snprintf(pos, remaining, "%sSA", first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_ETHTYPE) {
		written = snprintf(pos, remaining, "%sEthType",
				   first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_OUTER_VLAN) {
		written = snprintf(pos, remaining, "%sOuterVlan",
				   first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_INNER_VLAN) {
		written = snprintf(pos, remaining, "%sInnerVlan",
				   first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_BROADCAST) {
		written = snprintf(pos, remaining, "%sBroadcast",
				   first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_TCI) {
		written = snprintf(pos, remaining, "%sTCI", first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}
	if (mask & SECY_RULE_MASK_SCI) {
		written = snprintf(pos, remaining, "%sSCI", first ? "" : "+");
		if (written > 0 && written < remaining) {
			pos += written;
			remaining -= written;
			first = false;
		}
	}

	if (first) {
		snprintf(buf, buf_size, "none");
	}
}

/* Helper function to get control filter match type string */
static const char *get_ctl_match_type_string(enum secy_ctl_match_type_t type)
{
	switch (type) {
	case SECY_CTL_COMPARE_NO:
		return "NO";
	case SECY_CTL_COMPARE_DA:
		return "DA";
	case SECY_CTL_COMPARE_SA:
		return "SA";
	case SECY_CTL_COMPARE_HALF_DA_SA:
		return "HALF_DA_SA";
	case SECY_CTL_COMPARE_ETHER_TYPE:
		return "ETHER_TYPE";
	case SECY_CTL_COMPARE_DA_ETHER_TYPE:
		return "DA_ETHER_TYPE";
	case SECY_CTL_COMPARE_SA_ETHER_TYPE:
		return "SA_ETHER_TYPE";
	case SECY_CTL_COMPARE_DA_RANGE:
		return "DA_RANGE";
	default:
		return "unknown";
	}
}

/* Display SSCI table information */
static void show_ssci_table(struct seq_file *m,
			    struct qca_macsec_device_t *device)
{
	int i, configured_count = 0;

	seq_printf(m, "SSCI Table:\n");

	for (i = 0; i < SECY_LUT_MAX; i++) {
		if (device->rx_ssci[i] != 0 || device->tx_ssci[i] != 0) {
			configured_count++;
			seq_printf(m, "    SC%d:", i);
			if (device->rx_ssci[i] != 0) {
				seq_printf(m, " RX=0x%08x",
					   device->rx_ssci[i]);
			}
			if (device->tx_ssci[i] != 0) {
				seq_printf(m, " TX=0x%08x",
					   device->tx_ssci[i]);
			}
			seq_printf(m, "\n");
		}
	}
	seq_printf(m, "    Total: %d/%d configured\n",
		   configured_count, SECY_LUT_MAX);
}

/* Display LUT table information */
static void show_lut_table(struct seq_file *m,
			   struct qca_macsec_device_t *device)
{
	int i, configured_count = 0;
	char sci_str[32];
	char mask_str[128];

	seq_printf(m, "LUT Table:\n");

	for (i = 0; i < SECY_LUT_MAX; i++) {
		if (device->rx_lut[i].valid || device->tx_lut[i].valid) {
			configured_count++;
			seq_printf(m, "    SC%-2d:", i);

			if (device->tx_lut[i].valid) {
				format_rule_mask_string(mask_str,
					sizeof(mask_str),
					device->tx_lut[i].rule_mask);
				format_sci_string(sci_str, sizeof(sci_str),
					device->tx_lut[i].sci);
				seq_printf(m, " TX=mask:0x%02x(%s),"
					      "offset:%d,tci:0x%02x,sci:%s",
					device->tx_lut[i].rule_mask,
					mask_str, device->tx_lut[i].offset,
					device->tx_lut[i].tci, sci_str);
			}

			if (device->rx_lut[i].valid) {
				format_rule_mask_string(mask_str,
					sizeof(mask_str),
					device->rx_lut[i].rule_mask);
				format_sci_string(sci_str, sizeof(sci_str),
					device->rx_lut[i].sci);
				if (device->tx_lut[i].valid) {
					seq_printf(m, "\n         ");
				}
				seq_printf(m, " RX=mask:0x%02x(%s),"
					      "offset:%d,tci:0x%02x,sci:%s",
					device->rx_lut[i].rule_mask,
					mask_str, device->rx_lut[i].offset,
					device->rx_lut[i].tci, sci_str);
			}
			seq_printf(m, "\n");
		}
	}
	seq_printf(m, "    Total: %d/%d configured\n",
		   configured_count, SECY_LUT_MAX);
}

/* Helper function to display single control filter */
static void show_single_control_filter(struct seq_file *m,
					const char *direction,
					const struct secy_ctl_filt_t *filter)
{
	bool skip;

	if (filter->match_type == SECY_CTL_COMPARE_NO)
		return;

	seq_printf(m, "    %s: %s=", direction,
		   get_ctl_match_type_string(filter->match_type));

	skip = false;
	switch (filter->match_type) {
	case SECY_CTL_COMPARE_DA_ETHER_TYPE:
	case SECY_CTL_COMPARE_SA_ETHER_TYPE:
	case SECY_CTL_COMPARE_DA_RANGE:
		skip = true;
		fallthrough;
	case SECY_CTL_COMPARE_DA:
	case SECY_CTL_COMPARE_SA:
	case SECY_CTL_COMPARE_HALF_DA_SA:
		seq_printf(m, " %02x:%02x:%02x:%02x:%02x:%02x",
			   filter->sa_da_addr[0], filter->sa_da_addr[1],
			   filter->sa_da_addr[2], filter->sa_da_addr[3],
			   filter->sa_da_addr[4], filter->sa_da_addr[5]);
		if (!skip)
			break;
		/* fall through */
		fallthrough;
	case SECY_CTL_COMPARE_ETHER_TYPE:
		seq_printf(m, " 0x%04x", filter->ether_type_da_range);
		break;
	default:
		break;
	}

	seq_printf(m, ", mask= 0x%04x", filter->match_mask);

	/* Show bypass status only when disabled */
	if (!filter->bypass)
		seq_printf(m, ", bypass= disabled");

	seq_printf(m, "\n");
}

/* Display control filter information */
static void show_control_filters(struct seq_file *m,
				 struct qca_macsec_device_t *device)
{
	seq_printf(m, "Filter:\n");

	/* TX Control Filter */
	show_single_control_filter(m, "TX", &device->tx_ctl_filt);

	/* RX Control Filter */
	show_single_control_filter(m, "RX", &device->rx_ctl_filt);
}

/*
 * Per-device config_data show function - displays device configuration only
 */
static int nss_macsec_device_config_show(struct seq_file *m, void *v)
{
	u32 dev_id = (u32)(long)m->private;
	struct qca_macsec_device_t *local_device;
	struct macsec_device_debug *dev_debug;

	/* Validate device ID */
	if (dev_id >= MACSEC_DEVICE_MAXNUM)
		return -EINVAL;

	dev_debug = &macsec_device_debug[dev_id];

	/* Check if device exists and is in success state */
	if (dev_debug->device_state != MACSEC_SUCCESS) {
		/* Empty output for non-success devices */
		return 0;
	}

	/* Allocate temporary memory for device data */
	local_device = kmalloc(sizeof(struct qca_macsec_device_t), GFP_KERNEL);
	if (!local_device)
		return -ENOMEM;

	/* Get device data using safe encapsulated access */
	if (qca_macsec_get_device_data(dev_id, local_device) != 0) {
		kfree(local_device);
		return 0;  /* Empty output if device data unavailable */
	}

	/* Section 1: SC-SA Mapping Mode */
	seq_printf(m, "SC-SA Mapping Mode: %s\n",
		  get_sc_sa_mapping_string(local_device->sc_sa_mapping_mode));

	/* Section 2: SSCI Table */
	show_ssci_table(m, local_device);

	/* Section 3: LUT Table */
	show_lut_table(m, local_device);

	/* Section 4: Control Filter */
	show_control_filters(m, local_device);

	/* Free temporary memory */
	kfree(local_device);

	return 0;
}

/*
 * Per-device api_stats show function - displays API statistics only
 */
static int nss_macsec_device_api_stats_show(struct seq_file *m, void *v)
{
	u32 dev_id = (u32)(long)m->private;
	struct macsec_device_debug *dev_debug;

	/* Validate device ID */
	if (dev_id >= MACSEC_DEVICE_MAXNUM)
		return -EINVAL;

	dev_debug = &macsec_device_debug[dev_id];

	/* Display API statistics with four-way aligned vertical format */

	/* Device Query section */
	seq_printf(m, "%-16s : %-20s = %u\n", "Device Query",
		   "ifname_to_device_get", dev_debug->api_stats.ifname_to_devid_calls);

	/* Control section */
	seq_printf(m, "%-16s : %-20s = %u\n", "Control",
		   "secy", dev_debug->api_stats.macsec_en_set_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "controlled_port", dev_debug->api_stats.controlled_port_en_calls);

	/* SC Management section */
	seq_printf(m, "%-16s : %-20s = %u\n", "SC Management",
		   "rx_create", dev_debug->api_stats.rx_sc_create_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "tx_create", dev_debug->api_stats.tx_sc_create_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "rx_del", dev_debug->api_stats.rx_sc_del_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "tx_del", dev_debug->api_stats.tx_sc_del_calls);

	/* PN/Key Updates section */
	seq_printf(m, "%-16s : %-20s = %u\n", "PN/Key Updates",
		   "rx_pn", dev_debug->api_stats.rx_sa_npn_set_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "tx_pn", dev_debug->api_stats.tx_sa_npn_set_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "rx_sak", dev_debug->api_stats.rx_sak_set_calls);
	seq_printf(m, "%-16s   %-20s = %u\n", "",
		   "tx_sak", dev_debug->api_stats.tx_sak_set_calls);

	return 0;
}

/*
 * Per-device api_stats write function - handles reset command
 */
static ssize_t nss_macsec_device_api_stats_write(struct file *file,
						  const char __user *buf,
						  size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	u32 dev_id = (u32)(long)m->private;
	char input[16];

	/* Validate device ID */
	if (dev_id >= MACSEC_DEVICE_MAXNUM)
		return -EINVAL;

	/* Input validation */
	if (count >= sizeof(input))
		return -EINVAL;

	if (copy_from_user(input, buf, count))
		return -EFAULT;

	input[count] = '\0';

	/* Remove trailing newline if present */
	if (count > 0 && input[count - 1] == '\n')
		input[count - 1] = '\0';

	/* Check for reset command */
	if (strcasecmp(input, "reset") == 0) {
		/* Reset API statistics for this device */
		memset(&macsec_device_debug[dev_id].api_stats,
			0, sizeof(struct macsec_api_stats));
		return count;
	}

	return -EINVAL;  /* Only reset command supported */
}

/*
 * Per-device config_data file operations
 */
static int nss_macsec_device_config_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			nss_macsec_device_config_show, inode->i_private);
}

static const struct file_operations device_config_data_fops = {
	.owner = THIS_MODULE,
	.open = nss_macsec_device_config_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * Per-device api_stats file operations
 */
static int nss_macsec_device_api_stats_open(struct inode *inode,
					struct file *file)
{
	return single_open(file,
			nss_macsec_device_api_stats_show, inode->i_private);
}

static const struct file_operations device_api_stats_fops = {
	.owner = THIS_MODULE,
	.open = nss_macsec_device_api_stats_open,
	.read = seq_read,
	.write = nss_macsec_device_api_stats_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * Create per-device debugfs directory and files
 */
static int nss_macsec_create_device_debugfs(u32 dev_id)
{
	char dirname[32];
	struct dentry *device_dir;

	if (dev_id >= MACSEC_DEVICE_MAXNUM)
		return -EINVAL;

	/* Check if debugfs root directory exists */
	if (!macsec_debugfs_root) {
		osal_print("DebugFS dev%d root directory not available!\n",
				dev_id);
		return -ENOENT;
	}

	snprintf(dirname, sizeof(dirname), "device%d", dev_id);
	device_dir = debugfs_create_dir(dirname, macsec_debugfs_root);
	if (!device_dir) {
		osal_print("Failed to create device%d directory\n", dev_id);
		return -ENOMEM;
	}

	/* Create config_data file (read-only) */
	if (!debugfs_create_file("config_data", 0444, device_dir,
				(void *)(long)dev_id,
				&device_config_data_fops)) {
		osal_print("Failed to create dev%d/config_data file\n", dev_id);
		return -ENOMEM;
	}

	/* Create api_stats file (read-write, supports reset) */
	if (!debugfs_create_file("api_stats", 0644, device_dir,
				(void *)(long)dev_id, &device_api_stats_fops)) {
		osal_print("Failed to create dev%d/api_stats file\n", dev_id);
		return -ENOMEM;
	}

	return 0;
}

static int nss_macsec_debugfs_init(void)
{
	macsec_debugfs_root = debugfs_create_dir("qca-nss-macsec", NULL);
	if (!macsec_debugfs_root) {
		osal_print("Failed to create debugfs root directory\n");
		return -ENOMEM;
	}

	if (!debugfs_create_file("module_state", 0444, macsec_debugfs_root,
				NULL, &nss_macsec_module_status_fops)) {
		osal_print("Failed to create module_status debugfs file\n");
		debugfs_remove_recursive(macsec_debugfs_root);
		return -ENOMEM;
	}

	return 0;
}

static void nss_macsec_debugfs_exit(void)
{
	debugfs_remove_recursive(macsec_debugfs_root);
}

static int nss_macsec_dev_event(struct notifier_block  *nb,
				 unsigned long event, void  *info)
{
	struct net_device *event_dev = netdev_notifier_info_to_dev(info);
	struct phy_device *phydev;
	struct qca_common_private *cpriv;
	const char *sku_name;
	int ret = 0;
	u32 phyid = 0;
	u32 dev_id = 0;

	/* Defensive check for event_dev */
	if (unlikely(!event_dev))
		return NOTIFY_DONE;

	if (NULL == event_dev->phydev)
		return NOTIFY_DONE;

	/*
	 * Early filter: only handle netdev events we actually care about.
	 * Add new cases here (kept in sync with the dispatch switch below)
	 * when new events need to be supported.
	 */
	switch (event) {
	case NETDEV_REGISTER:
	case NETDEV_UNREGISTER:
		break;
	default:
		return NOTIFY_DONE;
	}

	phydev = event_dev->phydev;
	phyid = macsec_get_phyid(phydev);

	switch (phyid) {
	case QCA8081_PHY:
	case QCA8084_PHY:
	case QCA8111_PHY:
	case QCE1204_PHY:
	case IPQ52XX_PHY:
		break;
	default:
		return NOTIFY_DONE;
	}

	/* Gate by SKU */
	cpriv = NULL;
	if (phyid == QCA8111_PHY)
		cpriv = (struct qca_common_private *)phydev->priv;
	else if (phyid == QCE1204_PHY)
		cpriv = phydev->shared ?
			(struct qca_common_private *)phydev->shared->priv : NULL;

	if (phyid == QCA8111_PHY || phyid == QCE1204_PHY) {
		sku_name = (cpriv && cpriv->sku.name) ? cpriv->sku.name : NULL;

		if (!cpriv || !cpriv->sku.macsec) {
			phydev_info(phydev,
				    "MACsec disabled by SKU%s%s%s, skip attach\n",
				    sku_name ? " (" : "",
				    sku_name ? sku_name : "",
				    sku_name ? ")" : "");
			return NOTIFY_DONE;
		}
	}

	switch (event) {
	case NETDEV_REGISTER:
		atomic_inc(&macsec_stats.register_count);
		ret = qca_macsec_device_attach(event_dev);
		if (ret == 0) {
			/* Create debugfs if in success state */
			dev_id = macsec_get_device_id(event_dev->phydev);
			if (dev_id < MACSEC_DEVICE_MAXNUM) {
				nss_macsec_create_device_debugfs(dev_id);
			}
		}
		ret = ret ? NOTIFY_DONE : NOTIFY_OK;
		break;
	case NETDEV_UNREGISTER:
		atomic_inc(&macsec_stats.unregister_count);
		ret = qca_macsec_device_detach(event_dev);
		ret = ret ? NOTIFY_DONE : NOTIFY_OK;
		break;
	default:
		ret = NOTIFY_DONE;
		break;
	}

	return ret;
}

/*
 * Linux Net device Notifier
 */
static struct notifier_block nss_macsec_netdev_notifier = {
	.notifier_call = nss_macsec_dev_event,
};

static int __init nss_macsec_init(void)
{
	int ret = 0;

	/* Initialize statistics */
	memset(&macsec_device_debug, 0, sizeof(macsec_device_debug));

	/* Initialize debugfs FIRST - so we can always check status */
	ret = nss_macsec_debugfs_init();
	if (ret) {
		osal_print("Failed to initialize debugfs, error=%d\n", ret);
		/* Continue - debugfs is not critical for core functionality */
	}

	/* System resource initialization */
	ret = nss_macsec_netlink_init();
	if (ret) {
		macsec_stats.current_state = MACSEC_NETLINK_INIT_FAILURE;
		return ret;
	}

	nss_macsec_mutex_init();

	ret = register_netdevice_notifier(&nss_macsec_netdev_notifier);
	if (ret) {
		macsec_stats.current_state = MACSEC_NET_NOTIFIER_FAILURE;
		/* Cleanup resources before exit */
		nss_macsec_netlink_fini();
		nss_macsec_mutex_destroy();
		return ret;
	}

	/* Module initialization successful - set to SUCCESS state */
	macsec_stats.current_state = MACSEC_SUCCESS;
	return 0;
}

static void __exit nss_macsec_fini(void)
{
	unregister_netdevice_notifier(&nss_macsec_netdev_notifier);

	nss_macsec_netlink_fini();
	nss_macsec_debugfs_exit();
	nss_macsec_mutex_destroy();
	qca_macsec_device_cleanup();

	osal_print("nss_macsec exit success\n");
}

module_init(nss_macsec_init);
module_exit(nss_macsec_fini);
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif
