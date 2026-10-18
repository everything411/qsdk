// Procfs interface for NSS Debug (global controls only)
// Exposes: print_enable, global_level, global_mask, status

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "exports/nss_debug.h"
#include <linux/ctype.h>

#define NSS_DEBUG_PROC_DIR "nss_debug"

static struct proc_dir_entry *proc_dir;

static const char *lvl_to_name(enum nss_log_level lvl)
{
    switch (lvl) {
    case NSS_LOG_LEVEL_ERROR: return "ERROR";
    case NSS_LOG_LEVEL_WARN:  return "WARN";
    case NSS_LOG_LEVEL_INFO:  return "INFO";
    case NSS_LOG_LEVEL_TRACE: return "TRACE";
    }
    return "?";
}

static int name_to_lvl(const char *s, enum nss_log_level *out)
{
    if (!s || !out) return -EINVAL;
    if (!strcmp(s, "ERROR") || !strcmp(s, "0")) { *out = NSS_LOG_LEVEL_ERROR; return 0; }
    if (!strcmp(s, "WARN")  || !strcmp(s, "1")) { *out = NSS_LOG_LEVEL_WARN;  return 0; }
    if (!strcmp(s, "INFO")  || !strcmp(s, "2")) { *out = NSS_LOG_LEVEL_INFO;  return 0; }
    if (!strcmp(s, "TRACE") || !strcmp(s, "3")) { *out = NSS_LOG_LEVEL_TRACE; return 0; }
    return -EINVAL;
}

static nss_debug_cat_mask_t name_to_cat(const char *name)
{
    char buf[64];
    size_t i;
    const char *p;

    if (!name)
        return 0;

    /* Normalize: uppercase, replace spaces and hyphens with underscores */
    i = 0;
    for (p = name; *p && i < sizeof(buf) - 1; ++p) {
        char c = *p;
        if (c == ' ' || c == '-') c = '_';
        buf[i++] = toupper(c);
    }
    buf[i] = '\0';

    if (!strcmp(buf, "ACCELERATION_OFFLOAD_FAILURE")) return NSS_LOG_CAT_ACCEL_OFFLOAD_FAILURE;
    if (!strcmp(buf, "TRAFFIC_STALL")) return NSS_LOG_CAT_TRAFFIC_STALL;
    if (!strcmp(buf, "PING_FAILURE")) return NSS_LOG_CAT_PING_FAILURE;
    if (!strcmp(buf, "HIGH_CPU_USAGE")) return NSS_LOG_CAT_HIGH_CPU_USAGE;
    if (!strcmp(buf, "LOW_THROUGHPUT")) return NSS_LOG_CAT_LOW_THROUGHPUT;
    if (!strcmp(buf, "PRIORITIZATION")) return NSS_LOG_CAT_PRIORITIZATION;
    if (!strcmp(buf, "REFERENCE_COUNT_LEAK")) return NSS_LOG_CAT_REFCOUNT_LEAK;
    if (!strcmp(buf, "RCU_STALL")) return NSS_LOG_CAT_RCU_STALL;
    if (!strcmp(buf, "OOM")) return NSS_LOG_CAT_OOM;
    if (!strcmp(buf, "MEMORY_BLOAT")) return NSS_LOG_CAT_MEMORY_BLOAT;
    if (!strcmp(buf, "OLT_CONNECTIVITY")) return NSS_LOG_CAT_OLT_CONNECTIVITY;
    if (!strcmp(buf, "STC_FAILURE")) return NSS_LOG_CAT_STC_FAILURE;
    if (!strcmp(buf, "GENERIC")) return NSS_LOG_CAT_GENERIC;
    if (!strcmp(buf, "ALL")) return NSS_LOG_CAT_ALL;
    if (!strcmp(buf, "NONE")) return 0;
    return 0;
}

/* print_enable */
static int print_enable_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%u\n", nss_debug_enable_get() ? 1 : 0);
    return 0;
}

static ssize_t print_enable_write(struct file *file, const char __user *ubuf,
                                  size_t len, loff_t *ppos)
{
    char *kbuf;
    bool enable;
    int ret;
    if (len == 0 || len > 64) return -EINVAL;
    kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;
    if (copy_from_user(kbuf, ubuf, len)) { kfree(kbuf); return -EFAULT; }
    kbuf[len] = '\0';
    strim(kbuf);
    if (!strcmp(kbuf, "1") || !strcmp(kbuf, "y") || !strcmp(kbuf, "Y") || !strcmp(kbuf, "true")) enable = true;
    else if (!strcmp(kbuf, "0") || !strcmp(kbuf, "n") || !strcmp(kbuf, "N") || !strcmp(kbuf, "false")) enable = false;
    else { kfree(kbuf); return -EINVAL; }
    ret = nss_debug_enable_set(enable);
    kfree(kbuf);
    return ret ? ret : len;
}

static int print_enable_open(struct inode *inode, struct file *file)
{ return single_open(file, print_enable_show, NULL); }

static const struct proc_ops print_enable_ops = {
    .proc_open    = print_enable_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
    .proc_write   = print_enable_write,
};

/* global_level */
static int global_level_show(struct seq_file *m, void *v)
{
    enum nss_log_level lvl = nss_debug_level_global_get();
    seq_printf(m, "%s (%u)\n", lvl_to_name(lvl), (unsigned)lvl);
    return 0;
}

static ssize_t global_level_write(struct file *file, const char __user *ubuf,
                                  size_t len, loff_t *ppos)
{
    char *kbuf;
    enum nss_log_level lvl;
    int ret;
    if (len == 0 || len > 64) return -EINVAL;
    kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;
    if (copy_from_user(kbuf, ubuf, len)) { kfree(kbuf); return -EFAULT; }
    kbuf[len] = '\0';
    strim(kbuf);
    ret = name_to_lvl(kbuf, &lvl);
    if (ret) { kfree(kbuf); return ret; }
    ret = nss_debug_level_global_set(lvl);
    kfree(kbuf);
    return ret ? ret : len;
}

static int global_level_open(struct inode *inode, struct file *file)
{ return single_open(file, global_level_show, NULL); }

static const struct proc_ops global_level_ops = {
    .proc_open    = global_level_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
    .proc_write   = global_level_write,
};

/* global_mask */
static int global_mask_show(struct seq_file *m, void *v)
{
    seq_printf(m, "0x%08x\n", (unsigned int)nss_debug_global_mask_get());
    return 0;
}

static ssize_t global_mask_write(struct file *file, const char __user *ubuf,
                                 size_t len, loff_t *ppos)
{
    char *kbuf, *p, *tok;
    nss_debug_cat_mask_t new_mask = 0;
    int ret = 0;
    if (len == 0 || len > 256) return -EINVAL;
    kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;
    if (copy_from_user(kbuf, ubuf, len)) { kfree(kbuf); return -EFAULT; }
    kbuf[len] = '\0';
    strim(kbuf);

    if (!strncasecmp(kbuf, "0x", 2)) {
        unsigned long val;
        ret = kstrtoul(kbuf, 16, &val);
        if (ret) { kfree(kbuf); return -EINVAL; }
        new_mask = (nss_debug_cat_mask_t)val;
        /* Set mask exactly: clear all, then enable requested bits */
        nss_debug_category_disable(NSS_LOG_CAT_ALL);
        nss_debug_category_enable(new_mask);
        kfree(kbuf);
        return len;
    }

    /* Comma-separated names */
    if (!strcmp(kbuf, "ALL")) {
        nss_debug_category_enable(NSS_LOG_CAT_ALL);
        kfree(kbuf);
        return len;
    }
    if (!strcmp(kbuf, "NONE")) {
        nss_debug_category_disable(NSS_LOG_CAT_ALL);
        kfree(kbuf);
        return len;
    }

    p = kbuf;
    while ((tok = strsep(&p, ",")) != NULL) {
        strim(tok);
        if (!*tok) continue;
        new_mask |= name_to_cat(tok);
    }
    /* Apply: set to exactly new_mask */
    nss_debug_category_disable(NSS_LOG_CAT_ALL);
    nss_debug_category_enable(new_mask);
    kfree(kbuf);
    return len;
}

static int global_mask_open(struct inode *inode, struct file *file)
{ return single_open(file, global_mask_show, NULL); }

static const struct proc_ops global_mask_ops = {
    .proc_open    = global_mask_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
    .proc_write   = global_mask_write,
};

/* status */
static int status_show(struct seq_file *m, void *v)
{
    seq_printf(m, "print_enable=%u\n", nss_debug_enable_get() ? 1 : 0);
    seq_printf(m, "level=%s (%u)\n", lvl_to_name(nss_debug_level_global_get()), (unsigned)nss_debug_level_global_get());
    seq_printf(m, "mask=0x%08x\n", (unsigned int)nss_debug_global_mask_get());
    return 0;
}

static int status_open(struct inode *inode, struct file *file)
{ return single_open(file, status_show, NULL); }

static const struct proc_ops status_ops = {
    .proc_open    = status_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int nss_debug_procfs_init(void)
{
    struct proc_dir_entry *e;
    proc_dir = proc_mkdir(NSS_DEBUG_PROC_DIR, NULL);
    if (!proc_dir)
        return -ENOMEM;

    e = proc_create("print_enable", 0644, proc_dir, &print_enable_ops);
    if (!e) goto err;
    e = proc_create("global_level", 0644, proc_dir, &global_level_ops);
    if (!e) goto err;
    e = proc_create("global_mask", 0644, proc_dir, &global_mask_ops);
    if (!e) goto err;
    e = proc_create("status", 0444, proc_dir, &status_ops);
    if (!e) goto err;
    return 0;
err:
    remove_proc_subtree(NSS_DEBUG_PROC_DIR, NULL);
    proc_dir = NULL;
    return -ENOMEM;
}

void nss_debug_procfs_exit(void)
{
    if (proc_dir)
        remove_proc_subtree(NSS_DEBUG_PROC_DIR, NULL);
    proc_dir = NULL;
}
