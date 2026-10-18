/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/random.h>
#include <asm/current.h>
#include <asm/div64.h>
#include "ppe_drv.h"
#include "ppe_port_mgmt.h"
#include "ppe_drv_port_mgmt.h"
#include "ppe_port_mgmt_gen_dump.h"

/*
 * debugfs directory
 */
static struct dentry *ppe_port_mgmt_gen_dump_dentry;

/*
 * ppe_port_mgmt_gen_dump_init()
 *      Creates debugfs files for PPE PORT_MGMT gen dump
 */
void ppe_port_mgmt_gen_dump_init(struct dentry *d_rule)
{
	ppe_port_mgmt_gen_dump_dentry = debugfs_create_dir("ppe_port_mgmt_gen_dump", d_rule);
	if (!ppe_port_mgmt_gen_dump_dentry) {
		ppe_port_mgmt_warn("Failed to create ppe port mgmt gen dump debugfs directory\n");
		return;
	}
	return;
}
EXPORT_SYMBOL(ppe_port_mgmt_gen_dump_init);

/*
 * ppe_port_mgmt_gen_dump_exit()
 *      Removes PPE PORT_MGMT gen dump debugfs files
 */
void ppe_port_mgmt_gen_dump_exit(void)
{
	debugfs_remove_recursive(ppe_port_mgmt_gen_dump_dentry);
	return;
}
EXPORT_SYMBOL(ppe_port_mgmt_gen_dump_exit);
