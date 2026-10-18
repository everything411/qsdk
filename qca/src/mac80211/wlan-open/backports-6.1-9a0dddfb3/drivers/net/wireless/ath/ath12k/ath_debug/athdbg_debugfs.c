// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/debugfs.h>
#include <linux/init.h>

#include "athdbg_core.h"
#include "athdbg_minidump.h"
#include "athdbg_wmi_recording.h"

/*
 * Local duplicates of ath12k_debugfs_ctx / ath12k_debugfs_open /
 * ath12k_debugfs_create_file.
 *
 * ath_debug.ko is a separate module from ath12k.ko.  Referencing
 * ath12k_debugfs_open directly in a static file_operations initialiser
 * creates a hard link-time symbol dependency that causes insmod to fail
 * with "Unknown symbol" when ath_debug.ko is loaded before ath12k.ko.
 *
 * The two helpers only touch fields of struct ath12k_base (is_bypassed,
 * dev_flags, dev) and the small context struct below — all of which are
 * defined in headers already included transitively via athdbg_core.h.
 * Duplicating them here keeps ath_debug.ko self-contained with no
 * cross-module symbol dependency for these paths.
 */
struct athdbg_debugfs_ctx {
	struct ath12k_base *ab;
	void               *data;
};

static int athdbg_debugfs_open(struct inode *inode, struct file *file)
{
	struct athdbg_debugfs_ctx *ctx = inode->i_private;

	if (ctx->ab->is_bypassed) {
		pr_info("athdbg: device is in bypass state, skipping cmd\n");
		return -EPERM;
	}

	if (test_bit(ATH12K_FLAG_RECOVERY, &ctx->ab->dev_flags)) {
		pr_info("athdbg: device is in recovery, skipping cmd\n");
		return -EBUSY;
	}

	file->private_data = ctx->data;
	return 0;
}

static struct dentry *athdbg_debugfs_create_file(const char *name, umode_t mode,
						 struct dentry *parent,
						 struct ath12k_base *ab,
						 void *data,
						 const struct file_operations *fops)
{
	struct athdbg_debugfs_ctx *ctx;

	ctx = devm_kzalloc(ab->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->ab   = ab;
	ctx->data = data;

	return debugfs_create_file(name, mode, parent, ctx, fops);
}

#define ATHDBG_SNAPSHOT_BUF_SIZE 64

extern struct ath_debug_base *athdbg_base;
const struct file_operations debugfs_req_fops;
const struct file_operations debugfs_mask_fops;
const struct file_operations debugfs_qdss_enable_fops;
const struct file_operations debugfs_qdss_collect_fops;
const struct file_operations debugfs_snapshot_fops;

#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
static ssize_t athdbg_minidump_read(struct file *file, char __user *user_buf,
									size_t count, loff_t *ppos)
{
	static const char debugfs_data[] =
	"USAGE:\n"
	"echo 1 > collect\n"
	"echo 1 > show_all_alloc_struct\n"
	"echo <struct_name> > add_to_dump_list\n"
	"echo 1 > show_dump_list\n"
	"echo <0/1> > disable_minidump\n";

	return simple_read_from_buffer(user_buf, count, ppos, debugfs_data, sizeof(debugfs_data));
}


static ssize_t athdbg_minidump_write(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	const char *filename = file->f_path.dentry->d_name.name;
	char buf[128] = {0};
	int ret = 0, len = 0;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len <= 0) {
		pr_err("athdbg_core: Invalid Input %d %zu", len, count);
		ret = count;
		goto exit;
	}

	buf[len] = '\0';

	strim(buf);

	len = strlen(buf);
	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->input_buf = kzalloc(len + 1, GFP_ATOMIC);
	if (!dbg_req->input_buf) {
		kfree(dbg_req);
		return -ENOMEM;
	}
	ret = strscpy(dbg_req->input_buf, buf, len + 1);
	if (ret < 0) {
		ret = count;
		kfree(dbg_req->input_buf);
		kfree(dbg_req);
		return ret;
	}
	dbg_req->data = get_minidump_req(filename);
	dbg_req->req_type = ATH_DBG_REQ_COLLECT_MINI_DUMP;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}

const struct file_operations debugfs_minidump_fops = {
	.read = athdbg_minidump_read,
	.write = athdbg_minidump_write,
	.open = athdbg_debugfs_open,
	.owner = THIS_MODULE,
};

void athdbg_create_minidump_debugfs(struct dentry *dbg_dir,
				    struct ath12k_base *drv_ab)
{
	struct dentry	*minidump_dir;
	int i = 0;

	minidump_dir = debugfs_create_dir("minidump", dbg_dir);

	if (IS_ERR_OR_NULL(minidump_dir))
		return;

	while (athdbg_debugfs_handlers[i].filename != NULL) {
		athdbg_debugfs_create_file(athdbg_debugfs_handlers[i].filename,
					   athdbg_debugfs_handlers[i].permissions,
					   minidump_dir, drv_ab, drv_ab,
					   &debugfs_minidump_fops);
		i++;
	}
}
EXPORT_SYMBOL(athdbg_create_minidump_debugfs);
#endif

static ssize_t athdbg_mask_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	u64 mask = 0;
	char buf[512];
	int len = 0, ret;

	len += scnprintf(buf + len, sizeof(buf) - len,
		       "echo <mask>:<mask>:<mask> > dbgmask \t\n"
		       "echo <mask> > dbgmask\n");

	if (athdbg_base && athdbg_base->dbg_to_ath_ops &&
		athdbg_base->dbg_to_ath_ops->get_dbg_mask)
		mask = athdbg_base->dbg_to_ath_ops->get_dbg_mask();

	len += scnprintf(buf + len, sizeof(buf) - len, "Current: ");
	ret = athdbg_dbgmask_to_str(mask, buf + len, sizeof(buf) - len);
	if (ret < 0)
		return ret;
	len += ret;

	len += scnprintf(buf + len, sizeof(buf) - len, "\n");
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t athdbg_mask_write(struct file *file,
		const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	u64 dbg_mask = 0;
	char buf[256] = {0};
	char ip_mask[256] = {0};
	char *token, *tmp_mask;
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0) {
		pr_err("athdbg_core: Invalid Input %d %zu", ret, count);
		goto exit;
	}

	buf[ret] = '\0';
	strim(buf);

	if (!buf[0])
		goto exit;

	ret = sscanf(buf, "%255s", ip_mask);
	if (ret <= 0)
		goto exit;

	tmp_mask = ip_mask;
	while ((token = strsep(&tmp_mask, ":")) != NULL) {
		if (!*token)
			continue;
		dbg_mask |= athdbg_conv_str_to_dbgmask(token);
	}

	if (!dbg_mask) {
		pr_err("athdbg_core: unknown dbg request");
		ret = count;
		return ret;
	}

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->req_type = ATH_DBG_REQ_SETMASK;
	dbg_req->data = dbg_mask;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}

const struct file_operations debugfs_mask_fops = {
	.read = athdbg_mask_read,
	.write = athdbg_mask_write,
	.open = athdbg_debugfs_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_mask_fops);

static atomic_t athdbg_snapshot_enabled = ATOMIC_INIT(1);

static ssize_t athdbg_snapshot_read(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	char buf[ATHDBG_SNAPSHOT_BUF_SIZE];
	int len;

	len = scnprintf(buf, sizeof(buf), "%d\n",
			atomic_read(&athdbg_snapshot_enabled));
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t athdbg_snapshot_write(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	char buf[8] = {0};
	u32 val;
	int ret;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0)
		return ret;

	buf[ret] = '\0';
	strim(buf);

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	atomic_set(&athdbg_snapshot_enabled, val ? 1 : 0);
	pr_info("athdbg: snapshot collection %s\n", val ? "enabled" : "disabled");

	return count;
}

const struct file_operations debugfs_snapshot_fops = {
	.read  = athdbg_snapshot_read,
	.write = athdbg_snapshot_write,
	.open  = athdbg_debugfs_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_snapshot_fops);

static ssize_t athdbg_qdss_enable_read(struct file *file,
				       char __user *user_buf,
				       size_t count,
					loff_t *ppos)
{
	static const char debugfs_data[] =
		" 1 - enable debugfs\n";

	return simple_read_from_buffer(user_buf, count, ppos,
					debugfs_data,
					sizeof(debugfs_data));
}


static ssize_t athdbg_qdss_enable_write(struct file *file,
					const char __user *user_buf,
					size_t count,
					loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	char buf[128] = {0};
	u8 enable_qdss;
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0) {
		pr_err("Invalid Input %d %zu", ret, count);
		goto exit;
	}

	buf[ret] = '\0';

	if (kstrtou8(buf, 10, &enable_qdss))
		return -EINVAL;

	if (enable_qdss == 1 && ab->is_qdss_tracing)
		return count;

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->req_type = ATH_DBG_REQ_ENABLE_QDSS;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}


const struct file_operations debugfs_qdss_enable_fops = {
	.read = athdbg_qdss_enable_read,
	.write = athdbg_qdss_enable_write,
	.open = athdbg_debugfs_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_qdss_enable_fops);

static ssize_t athdbg_qdss_collect_read(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *ppos)
{
	static const char debugfs_data[] =
		"echo 0x1\t- QDSS Trace Dump\n"
		"echo 0x4\t- ETB SoC Dump (SoC-level Embedded Trace Buffer)\n"
		"echo 0x8\t- ETB Dump (Primary Embedded Trace Buffer)\n"
		"echo 0x40\t- PHY Array 0 Dump (PHYA0 - Primary PHY instance)\n"
		"echo 0x80\t- PHY Array 1 Dump (PHYA1 - Secondary PHY instance, Dual-MAC only)\n"
		"echo 0x400\t- AUX PHY Dump (Auxiliary PHY listen/scan radio)\n"
		"echo 0x800\t- MCSS Dump (MAC Compute SubSystem)\n"
		"echo 0x1000\t- ETB2 Dump (Secondary Embedded Trace Buffer)\n";

	return simple_read_from_buffer(user_buf, count, ppos,
					debugfs_data,
					sizeof(debugfs_data));
}

static ssize_t athdbg_qdss_collect_write(struct file *file,
					 const char __user *user_buf,
					 size_t count,
					 loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	char buf[128] = {0};
	u32 val;
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0) {
		pr_err("Invalid Input %d %zu", ret, count);
		goto exit;
	}

	buf[ret] = '\0';

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	switch (val) {
	case ATHDBG_QDSS_DUMP:
	case ATHDBG_QDSS_PHYA0:
	case ATHDBG_QDSS_ETB_SOC:
	case ATHDBG_QDSS_ETB_WCSS:
	case ATHDBG_QDSS_AUX:
	case ATHDBG_QDSS_MCSS:
	case ATHDBG_QDSS_ETB2_WCSS:
		break;
	case ATHDBG_QDSS_PHYA1:
		if (!ab->is_dualmac) {
			pr_err("PHYA1 dump not supported %x\n", val);
			return -EINVAL;
		}
		break;
	default:
		pr_err("Invalid value %x\n", val);
		return -EINVAL;
	}

	if (!ab->is_qdss_tracing) {
		pr_err("QDSS tracing is not enabled\n");
		return count;
	}

	/* Verify firmware is ready to accept QDSS commands */
	if (!test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags)) {
		pr_err("Firmware not ready for QDSS collection\n");
		return -EAGAIN;
	}

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->req_type = ATH_DBG_REQ_DUMP_QDSS;
	dbg_req->data = val;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}

const struct file_operations debugfs_qdss_collect_fops = {
	.read = athdbg_qdss_collect_read,
	.write = athdbg_qdss_collect_write,
	.open = athdbg_debugfs_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_qdss_collect_fops);

static ssize_t athdbg_wmi_common_read(struct file *file, char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	static const char usage[] =
		"Usage:\n"
		"1. Enable/Disable Recording\n"
		"   echo 0 > enable        : Disable recording\n"
		"   echo 1 > enable        : Enable (Default size 1024)\n"
		"   echo NNNN > enable     : Enable with buffer size NNNN\n"
		"\n"
		"2. Control Verbosity Level of Logs\n"
		"   echo 0 > verbosity     : Minimal (Header only)\n"
		"   echo 1 > verbosity     : Standard (Header + 8 bytes data)\n"
		"   echo 2 > verbosity     : Full (Header + 16 bytes data)\n"
		"\n"
		"3. Print Logs to Console\n"
		"   echo 1 > dump          : Dump WMI Commands\n"
		"   echo 2 > dump          : Dump WMI Events\n"
		"   echo 3 > dump          : Dump WMI TX Completions\n";

	return simple_read_from_buffer(user_buf, count, ppos, usage, sizeof(usage));
}

static ssize_t athdbg_wmi_common_write(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	enum athdbg_wmi_request wmi_req;
	struct athdbg_request *dbg_req;
	char buf[128] = {0};
	int len;

	if (!ab)
		return -EINVAL;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len <= 0)
		return count;

	buf[len] = '\0';
	strim(buf);

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->input_buf = kstrdup(buf, GFP_ATOMIC);
	if (!dbg_req->input_buf) {
		kfree(dbg_req);
		return -ENOMEM;
	}

	dbg_req->ab = ab;
	wmi_req = athdbg_wmi_get_req_from_name(file->f_path.dentry->d_name.name);

	switch (wmi_req) {
	case ATHDBG_WMI_REQ_ENABLE:
		dbg_req->req_type = ATH_DBG_REQ_WMI_ENABLE;
		break;
	case ATHDBG_WMI_REQ_VERBOSITY:
		dbg_req->req_type = ATH_DBG_REQ_WMI_VERBOSITY;
		break;
	case ATHDBG_WMI_REQ_DUMP:
		dbg_req->req_type = ATH_DBG_REQ_WMI_DUMP;
		break;
	default:
		dbg_req->req_type = ATH_DBG_REQ_UNKNOWN;
		break;
	}

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	return count;
}

const struct file_operations debugfs_wmi_common_fops = {
	.owner = THIS_MODULE,
	.open  = athdbg_debugfs_open,
	.read  = athdbg_wmi_common_read,
	.write = athdbg_wmi_common_write,
};
EXPORT_SYMBOL(debugfs_wmi_common_fops);

void athdbg_create_wmi_debugfs(struct dentry *dbg_dir, struct ath12k_base *drv_ab)
{
	struct dentry *wmi_dir;

	wmi_dir = debugfs_create_dir("wmi_recording", dbg_dir);
	if (IS_ERR_OR_NULL(wmi_dir))
		return;

	athdbg_debugfs_create_file("enable", 0644, wmi_dir, drv_ab,
				   drv_ab, &debugfs_wmi_common_fops);
	athdbg_debugfs_create_file("dump", 0200, wmi_dir, drv_ab,
				   drv_ab, &debugfs_wmi_common_fops);
	athdbg_debugfs_create_file("verbosity", 0644, wmi_dir, drv_ab,
				   drv_ab, &debugfs_wmi_common_fops);
}
EXPORT_SYMBOL(athdbg_create_wmi_debugfs);
