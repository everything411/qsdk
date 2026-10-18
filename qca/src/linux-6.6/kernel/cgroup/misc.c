// SPDX-License-Identifier: GPL-2.0
/*
 * Miscellaneous cgroup controller
 *
 * Copyright 2020 Google LLC
 * Author: Vipin Sharma <vipinsh@google.com>
 */

#include <linux/limits.h>
#include <linux/cgroup.h>
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/misc_cgroup.h>
#include <linux/capability.h>

#define MAX_STR "max"
#define MAX_NUM U64_MAX

/* Miscellaneous res name, keep it in sync with enum misc_res_type */
static const char *const misc_res_name[] = {
#ifdef CONFIG_KVM_AMD_SEV
	/* AMD SEV ASIDs resource */
	"sev",
	/* AMD SEV-ES ASIDs resource */
	"sev_es",
#endif
};

/* Root misc cgroup */
static struct misc_cg root_cg;

/*
 * Miscellaneous resources capacity for the entire machine. 0 capacity means
 * resource is not initialized or not present in the host.
 *
 * root_cg.max and capacity are independent of each other. root_cg.max can be
 * more than the actual capacity. We are using Limits resource distribution
 * model of cgroup for miscellaneous controller.
 */
static u64 misc_res_capacity[MISC_CG_RES_TYPES];

/**
 * parent_misc() - Get the parent of the passed misc cgroup.
 * @cgroup: cgroup whose parent needs to be fetched.
 *
 * Context: Any context.
 * Return:
 * * struct misc_cg* - Parent of the @cgroup.
 * * %NULL - If @cgroup is null or the passed cgroup does not have a parent.
 */
static struct misc_cg *parent_misc(struct misc_cg *cgroup)
{
	return cgroup ? css_misc(cgroup->css.parent) : NULL;
}

/**
 * valid_type() - Check if @type is valid or not.
 * @type: misc res type.
 *
 * Context: Any context.
 * Return:
 * * true - If valid type.
 * * false - If not valid type.
 */
static inline bool valid_type(enum misc_res_type type)
{
	return type >= 0 && type < MISC_CG_RES_TYPES;
}

/**
 * misc_cg_res_total_usage() - Get the current total usage of the resource.
 * @type: misc res type.
 *
 * Context: Any context.
 * Return: Current total usage of the resource.
 */
u64 misc_cg_res_total_usage(enum misc_res_type type)
{
	if (valid_type(type))
		return atomic64_read(&root_cg.res[type].usage);

	return 0;
}
EXPORT_SYMBOL_GPL(misc_cg_res_total_usage);

/**
 * misc_cg_set_capacity() - Set the capacity of the misc cgroup res.
 * @type: Type of the misc res.
 * @capacity: Supported capacity of the misc res on the host.
 *
 * If capacity is 0 then the charging a misc cgroup fails for that type.
 *
 * Context: Any context.
 * Return:
 * * %0 - Successfully registered the capacity.
 * * %-EINVAL - If @type is invalid.
 */
int misc_cg_set_capacity(enum misc_res_type type, u64 capacity)
{
	if (!valid_type(type))
		return -EINVAL;

	WRITE_ONCE(misc_res_capacity[type], capacity);
	return 0;
}
EXPORT_SYMBOL_GPL(misc_cg_set_capacity);

/**
 * misc_cg_cancel_charge() - Cancel the charge from the misc cgroup.
 * @type: Misc res type in misc cg to cancel the charge from.
 * @cg: Misc cgroup to cancel charge from.
 * @amount: Amount to cancel.
 *
 * Context: Any context.
 */
static void misc_cg_cancel_charge(enum misc_res_type type, struct misc_cg *cg,
				  u64 amount)
{
	WARN_ONCE(atomic64_add_negative(-amount, &cg->res[type].usage),
		  "misc cgroup resource %s became less than 0",
		  misc_res_name[type]);
}

/**
 * misc_cg_try_charge() - Try charging the misc cgroup.
 * @type: Misc res type to charge.
 * @cg: Misc cgroup which will be charged.
 * @amount: Amount to charge.
 *
 * Charge @amount to the misc cgroup. Caller must use the same cgroup during
 * the uncharge call.
 *
 * Context: Any context.
 * Return:
 * * %0 - If successfully charged.
 * * -EINVAL - If @type is invalid or misc res has 0 capacity.
 * * -EBUSY - If max limit will be crossed or total usage will be more than the
 *	      capacity.
 */
int misc_cg_try_charge(enum misc_res_type type, struct misc_cg *cg, u64 amount)
{
	struct misc_cg *i, *j;
	int ret;
	struct misc_res *res;
	u64 new_usage;

	if (!(valid_type(type) && cg && READ_ONCE(misc_res_capacity[type])))
		return -EINVAL;

	if (!amount)
		return 0;

	for (i = cg; i; i = parent_misc(i)) {
		res = &i->res[type];

		new_usage = atomic64_add_return(amount, &res->usage);
		if (new_usage > READ_ONCE(res->max) ||
		    new_usage > READ_ONCE(misc_res_capacity[type])) {
			ret = -EBUSY;
			goto err_charge;
		}
	}
	return 0;

err_charge:
	for (j = i; j; j = parent_misc(j)) {
		atomic64_inc(&j->res[type].events);
		cgroup_file_notify(&j->events_file);
	}

	for (j = cg; j != i; j = parent_misc(j))
		misc_cg_cancel_charge(type, j, amount);
	misc_cg_cancel_charge(type, i, amount);
	return ret;
}
EXPORT_SYMBOL_GPL(misc_cg_try_charge);

/**
 * misc_cg_uncharge() - Uncharge the misc cgroup.
 * @type: Misc res type which was charged.
 * @cg: Misc cgroup which will be uncharged.
 * @amount: Charged amount.
 *
 * Context: Any context.
 */
void misc_cg_uncharge(enum misc_res_type type, struct misc_cg *cg, u64 amount)
{
	struct misc_cg *i;

	if (!(amount && valid_type(type) && cg))
		return;

	for (i = cg; i; i = parent_misc(i))
		misc_cg_cancel_charge(type, i, amount);
}
EXPORT_SYMBOL_GPL(misc_cg_uncharge);

/**
 * misc_cg_max_show() - Show the misc cgroup max limit.
 * @sf: Interface file
 * @v: Arguments passed
 *
 * Context: Any context.
 * Return: 0 to denote successful print.
 */
static int misc_cg_max_show(struct seq_file *sf, void *v)
{
	int i;
	struct misc_cg *cg = css_misc(seq_css(sf));
	u64 max;

	for (i = 0; i < MISC_CG_RES_TYPES; i++) {
		if (READ_ONCE(misc_res_capacity[i])) {
			max = READ_ONCE(cg->res[i].max);
			if (max == MAX_NUM)
				seq_printf(sf, "%s max\n", misc_res_name[i]);
			else
				seq_printf(sf, "%s %llu\n", misc_res_name[i],
					   max);
		}
	}

	return 0;
}

/**
 * misc_cg_max_write() - Update the maximum limit of the cgroup.
 * @of: Handler for the file.
 * @buf: Data from the user. It should be either "max", 0, or a positive
 *	 integer.
 * @nbytes: Number of bytes of the data.
 * @off: Offset in the file.
 *
 * User can pass data like:
 * echo sev 23 > misc.max, OR
 * echo sev max > misc.max
 *
 * Context: Any context.
 * Return:
 * * >= 0 - Number of bytes processed in the input.
 * * -EINVAL - If buf is not valid.
 * * -ERANGE - If number is bigger than the u64 capacity.
 */
static ssize_t misc_cg_max_write(struct kernfs_open_file *of, char *buf,
				 size_t nbytes, loff_t off)
{
	struct misc_cg *cg;
	u64 max;
	int ret = 0, i;
	enum misc_res_type type = MISC_CG_RES_TYPES;
	char *token;

	buf = strstrip(buf);
	token = strsep(&buf, " ");

	if (!token || !buf)
		return -EINVAL;

	for (i = 0; i < MISC_CG_RES_TYPES; i++) {
		if (!strcmp(misc_res_name[i], token)) {
			type = i;
			break;
		}
	}

	if (type == MISC_CG_RES_TYPES)
		return -EINVAL;

	if (!strcmp(MAX_STR, buf)) {
		max = MAX_NUM;
	} else {
		ret = kstrtou64(buf, 0, &max);
		if (ret)
			return ret;
	}

	cg = css_misc(of_css(of));

	if (READ_ONCE(misc_res_capacity[type]))
		WRITE_ONCE(cg->res[type].max, max);
	else
		ret = -EINVAL;

	return ret ? ret : nbytes;
}

/**
 * misc_cg_current_show() - Show the current usage of the misc cgroup.
 * @sf: Interface file
 * @v: Arguments passed
 *
 * Context: Any context.
 * Return: 0 to denote successful print.
 */
static int misc_cg_current_show(struct seq_file *sf, void *v)
{
	int i;
	u64 usage;
	struct misc_cg *cg = css_misc(seq_css(sf));

	for (i = 0; i < MISC_CG_RES_TYPES; i++) {
		usage = atomic64_read(&cg->res[i].usage);
		if (READ_ONCE(misc_res_capacity[i]) || usage)
			seq_printf(sf, "%s %llu\n", misc_res_name[i], usage);
	}

	return 0;
}

/**
 * misc_cg_capacity_show() - Show the total capacity of misc res on the host.
 * @sf: Interface file
 * @v: Arguments passed
 *
 * Only present in the root cgroup directory.
 *
 * Context: Any context.
 * Return: 0 to denote successful print.
 */
static int misc_cg_capacity_show(struct seq_file *sf, void *v)
{
	int i;
	u64 cap;

	for (i = 0; i < MISC_CG_RES_TYPES; i++) {
		cap = READ_ONCE(misc_res_capacity[i]);
		if (cap)
			seq_printf(sf, "%s %llu\n", misc_res_name[i], cap);
	}

	return 0;
}

static int misc_events_show(struct seq_file *sf, void *v)
{
	struct misc_cg *cg = css_misc(seq_css(sf));
	u64 events;
	int i;

	for (i = 0; i < MISC_CG_RES_TYPES; i++) {
		events = atomic64_read(&cg->res[i].events);
		if (READ_ONCE(misc_res_capacity[i]) || events)
			seq_printf(sf, "%s.max %llu\n", misc_res_name[i], events);
	}
	return 0;
}

/* Misc cgroup interface files */
static struct cftype misc_cg_files[] = {
	{
		.name = "max",
		.write = misc_cg_max_write,
		.seq_show = misc_cg_max_show,
		.flags = CFTYPE_NOT_ON_ROOT,
	},
	{
		.name = "current",
		.seq_show = misc_cg_current_show,
	},
	{
		.name = "capacity",
		.seq_show = misc_cg_capacity_show,
		.flags = CFTYPE_ONLY_ON_ROOT,
	},
	{
		.name = "events",
		.flags = CFTYPE_NOT_ON_ROOT,
		.file_offset = offsetof(struct misc_cg, events_file),
		.seq_show = misc_events_show,
	},
	{}
};

/**
 * misc_cg_alloc() - Allocate misc cgroup.
 * @parent_css: Parent cgroup.
 *
 * Context: Process context.
 * Return:
 * * struct cgroup_subsys_state* - css of the allocated cgroup.
 * * ERR_PTR(-ENOMEM) - No memory available to allocate.
 */
static struct cgroup_subsys_state *
misc_cg_alloc(struct cgroup_subsys_state *parent_css)
{
	enum misc_res_type i;
	struct misc_cg *cg;

	if (!parent_css) {
		cg = &root_cg;
	} else {
		cg = kzalloc(sizeof(*cg), GFP_KERNEL);
		if (!cg)
			return ERR_PTR(-ENOMEM);
	}

	for (i = 0; i < MISC_CG_RES_TYPES; i++) {
		WRITE_ONCE(cg->res[i].max, MAX_NUM);
		atomic64_set(&cg->res[i].usage, 0);
	}

	return &cg->css;
}

/**
 * misc_cg_free() - Free the misc cgroup.
 * @css: cgroup subsys object.
 *
 * Context: Any context.
 */
static void misc_cg_free(struct cgroup_subsys_state *css)
{
	kfree(css_misc(css));
}

/* Cgroup controller callbacks */
struct cgroup_subsys misc_cgrp_subsys = {
	.css_alloc = misc_cg_alloc,
	.css_free = misc_cg_free,
	.legacy_cftypes = misc_cg_files,
	.dfl_cftypes = misc_cg_files,
};

#ifdef CONFIG_CGROUP_SCID_LLCC
/*
 * SCID LLCC cgroup controller
 *
 * Manages SCID (Slice ID) assignments for LLCC (Last Level Cache Controller)
 */

struct llcc_scid {
	struct cgroup_subsys_state css;
	u32 scid;
	bool scid_set;
};

static inline struct llcc_scid *llcc_scid_css(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct llcc_scid, css) : NULL;
}

/* Map SCID to SID register value
 * Only SCID 0, 2, and 3 are valid
 */
static inline int scid_map_to_sid(int scid)
{
	switch (scid) {
	case 0:
		return 2;  /* SCID 0 needs SID register value 2 */
	case 2:
		return 0;  /* SCID 2 needs SID register value 0 */
	case 3:
		return 1;  /* SCID 3 needs SID register value 1 */
	default:
		return 2;  /* Default to SCID 0 (SID value 2) */
	}
}

/* Allocate memory */
static struct cgroup_subsys_state *
scid_llcc_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct llcc_scid *llcc;

	llcc = kzalloc(sizeof(*llcc), GFP_KERNEL);
	if (!llcc)
		return ERR_PTR(-ENOMEM);
	llcc->scid = 0;
	llcc->scid_set = false;
	return &llcc->css;
}

/* free memory */
static void scid_llcc_css_free(struct cgroup_subsys_state *css)
{
	struct llcc_scid *llcc = llcc_scid_css(css);

	kfree(llcc);
}

/*  Attach process to cgroup. Sets the scid_enable and sid_value for that task
 *  based on cgroup configuration
 */
static void scid_llcc_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;

	cgroup_taskset_for_each(task, css, tset) {
		struct llcc_scid *llcc = llcc_scid_css(css);
		int sid_value;

		task_lock(task);

		if (llcc->scid_set) {
			/* Enable SCID for this task */
			task->thread.scid_enable = 1;

			/* Map user SCID to SID register value */
			sid_value = scid_map_to_sid(llcc->scid);
			task->thread.sid_value = sid_value;

			pr_info("SCID: PID %d assigned SCID %u (SID value: %d)\n",
				task->pid, llcc->scid, sid_value);
		} else {
			/* Default - use CPU default SID */
			task->thread.scid_enable = 0;
			pr_debug("SCID: PID %d using default SID\n", task->pid);
		}
		task_unlock(task);
	}
}

/* check if the process can attach to the cgroup  */
static int scid_llcc_can_attach(struct cgroup_taskset *tset)
{
	return 0;
}

/* llcc_scid_show()- display the scid */
static int llcc_scid_show(struct seq_file *sf, void *v)
{
	struct llcc_scid *llcc = llcc_scid_css(seq_css(sf));


	if (llcc->scid_set)
		seq_printf(sf, "%u\n", llcc->scid);
	else
		seq_puts(sf, "default\n");

	return 0;
}

/**
 * llcc_scid_write - Set SCID for cgroup
 * Only allows SCID values 0, 2, and 3
 */
static ssize_t llcc_scid_write(struct kernfs_open_file *of, char *buf,
			       size_t nbytes, loff_t off)
{
	struct llcc_scid *llcc = llcc_scid_css(of_css(of));
	u32 scid;
	int ret;


	/* Parse input */
	buf = strstrip(buf);

	/* Handle "default" keyword */
	if (strcmp(buf, "default") == 0) {
		llcc->scid_set = false;
		pr_info("SCID: cgroup set to default\n");
		return nbytes;
	}

	/* Parse numeric SCID */
	ret = kstrtou32(buf, 0, &scid);
	if (ret)
		return ret;

	/* Validate SCID value - only 0, 2, and 3 are valid */
	if (scid != 0 && scid != 2 && scid != 3) {
		pr_err("SCID: Invalid SCID %u (must be 0, 2, or 3)\n", scid);
		return -EINVAL;
	}

	/* Set SCID */
	llcc->scid = scid;
	llcc->scid_set = true;

	pr_info("SCID: cgroup SCID set to %u\n", scid);

	return nbytes;
}

/**
 * llcc_status_show - Show detailed status
 */
static int llcc_status_show(struct seq_file *sf, void *v)
{
	struct llcc_scid *llcc = llcc_scid_css(seq_css(sf));
	int sid_value;


	seq_printf(sf, "SCID configured: %s\n",
		llcc->scid_set ? "yes" : "no");

	if (llcc->scid_set) {
		/* Show user SCID */
		seq_printf(sf, "User SCID: %u\n", llcc->scid);

		/* Show SID register value */
		sid_value = scid_map_to_sid(llcc->scid);
		seq_printf(sf, "SID register value: %d\n", sid_value);
	}

	return 0;
}

/**
 * scid_llcc_files - cgroup control files
 */
static struct cftype scid_llcc_files[] = {
	{
		.name = "scid",
		.seq_show = llcc_scid_show,
		.write = llcc_scid_write,
		.flags = CFTYPE_NOT_ON_ROOT,
	},
	{
		.name = "status",
		.seq_show = llcc_status_show,
		.flags = CFTYPE_NOT_ON_ROOT,
	},
	{ }  /* terminate */
};

/**
 * scid_llcc_cgrp_subsys - SCID LLCC cgroup subsystem definition
 */
struct cgroup_subsys scid_llcc_cgrp_subsys = {
	.css_alloc = scid_llcc_css_alloc,
	.css_free = scid_llcc_css_free,
	.can_attach = scid_llcc_can_attach,
	.attach = scid_llcc_attach,
	.legacy_cftypes = scid_llcc_files,
	.dfl_cftypes = scid_llcc_files,
	.threaded = true,  /* Allow per-thread control */
};

#endif /* CONFIG_CGROUP_SCID_LLCC */
