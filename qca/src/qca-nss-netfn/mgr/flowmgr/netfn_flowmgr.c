/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr.c
 *      Network function manager
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/sysctl.h>

#include "netfn_flowmgr_priv.h"
#include "aes/ppe/netfn_flowmgr_ppe.h"
#include "aes/sfe/netfn_flowmgr_sfe.h"

#define NETFN_FLOWMGR_STATIC_DBG_LEVEL_STR_LEN 8

/*
 * Module parameter to enable/disable stats sync from AEs.
 */
bool enable_stats_sync = false;
module_param(enable_stats_sync, bool, 0644);
MODULE_PARM_DESC(enable_stats_sync, "Enable or disable stats sync for IPv4 and IPv6");

uint32_t static_dbg_level = 0;
uint16_t ipv4_stats_sync_period = 60;
uint16_t ipv6_stats_sync_period = 60;
static char static_dbg_level_str[NETFN_FLOWMGR_STATIC_DBG_LEVEL_STR_LEN];

struct netfn_flowmgr netfn_flowmgr_gbl;

/*
 * netfn_flowmgr_rule_accel()
 *	Flow acceleration to AE
 */
netfn_flowmgr_ret_t netfn_flowmgr_rule_accel(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply, netfn_flowmgr_accel_mode_t mode)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ret_t status = 0;

	/*
	 * Check if both original and reply direction has valid rules
	 * Note: Currently AEs does not support single direction flow rule.
	 */
	if (!original && !reply) {
		netfn_flowmgr_warn("Invalid create rule\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_RULE, 0);
	}

	/*
	 * AE mode should either be PPE or SFE
	 */
	if ((mode != NETFN_FLOWMGR_ACCEL_MODE_PPE) && (mode != NETFN_FLOWMGR_ACCEL_MODE_SFE)) {
		netfn_flowmgr_warn("Invalid accel mode\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_ACCEL_MODE, 0);
	}

	/*
	 * Ops should be registered
	 */
	if (f->ae_ops[mode] == NULL) {
		netfn_flowmgr_warn("Ops not registerd for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Check if create rule ops is valid
	 */
	if (f->ae_ops[mode]->create_rule == NULL) {
		netfn_flowmgr_warn("create rule not registered for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Call create rule function registered by AEs
	 * If status is non-zero that means rule creation is failed and it
	 * will contain both errors (AE specific error and Netfn error)
	 */
	status = f->ae_ops[mode]->create_rule(original, reply);
	if (status)
		netfn_flowmgr_warn("Create rule failed\n");

	return status;
}
EXPORT_SYMBOL(netfn_flowmgr_rule_accel);

/*
 * netfn_flowmgr_rule_decel()
 *	API for flow deacceleration
 */
netfn_flowmgr_ret_t netfn_flowmgr_rule_decel(struct netfn_flowmgr_destroy_rule *original, struct netfn_flowmgr_destroy_rule *reply, netfn_flowmgr_accel_mode_t mode)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ret_t status = 0;

	/*
	 * Check if both original and reply direction has valid rules
	 * Note: Currently AEs does not support single direction flow rule.
	 */
	if (!original && !reply) {
		netfn_flowmgr_warn("Invalid destroy rule\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_RULE, 0);
	}

	/*
	 * AE mode should either be PPE or SFE
	 */
	if ((mode != NETFN_FLOWMGR_ACCEL_MODE_PPE) && (mode != NETFN_FLOWMGR_ACCEL_MODE_SFE)) {
		netfn_flowmgr_warn("Invalid accel mode\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_ACCEL_MODE, 0);
	}

	/*
	 * Ops should be registered
	 */
	if (f->ae_ops[mode] == NULL) {
		netfn_flowmgr_warn("Ops not registerd for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Check if destroy rule ops is valid
	 */
	if (f->ae_ops[mode]->destroy_rule == NULL) {
		netfn_flowmgr_warn("destroy rule not registered for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Call destroy rule function registered by AEs.
	 * If status is non-zero that means rule destroy is failed and it
	 * will contain both errors (AE specific error and Netfn error)
	 */
	status = f->ae_ops[mode]->destroy_rule(original, reply);
	if (status)
		netfn_flowmgr_warn("Destroy rule failed\n");

	return status;
}
EXPORT_SYMBOL(netfn_flowmgr_rule_decel);

/*
 * netfn_flowmgr_dscp_pri_add()
 *	Add dscp priority acl rule
 */
netfn_flowmgr_ret_t netfn_flowmgr_dscp_pri_add(struct netfn_flowmgr_dscp_priority *dscpinfo, netfn_flowmgr_accel_mode_t mode)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ret_t status = 0;

	if (!dscpinfo) {
		netfn_flowmgr_warn("dscpinfo is NULL\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_FAIL_INVALID_PARAM, 0);
	}

	/*
	 * AE mode should either be PPE or SFE
	 */
	if ((mode != NETFN_FLOWMGR_ACCEL_MODE_PPE) && (mode != NETFN_FLOWMGR_ACCEL_MODE_SFE)) {
		netfn_flowmgr_warn("Invalid accel mode\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_ACCEL_MODE, 0);
	}

	/*
	 * Ops should be registered
	 */
	if (f->ae_ops[mode] == NULL) {
		netfn_flowmgr_warn("Ops not registerd for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Check if add dscp priority ops is valid
	 */
	if (f->ae_ops[mode]->add_dscp_priority == NULL) {
		netfn_flowmgr_warn("add dscp priority not registered for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Call add dscp priority callback function registered by AEs
	 * If status is non-zero that means rule add is failed and it
	 * will contain both errors (AE specific error and Netfn error)
	 */
	status = f->ae_ops[mode]->add_dscp_priority(dscpinfo);
	if (status)
		netfn_flowmgr_warn("Add DSCP priority failed\n");

	return status;
}
EXPORT_SYMBOL(netfn_flowmgr_dscp_pri_add);

/*
 * netfn_flowmgr_dscp_pri_del()
 *	Delete the dscp priority
 */
netfn_flowmgr_ret_t netfn_flowmgr_dscp_pri_del(int16_t rule_id, netfn_flowmgr_accel_mode_t mode)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ret_t status = 0;

	/*
	 * AE mode should either be PPE or SFE
	 */
	if ((mode != NETFN_FLOWMGR_ACCEL_MODE_PPE) && (mode != NETFN_FLOWMGR_ACCEL_MODE_SFE)) {
		netfn_flowmgr_warn("Invalid accel mode\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_ACCEL_MODE, 0);
	}

	/*
	 * Ops should be registered
	 */
	if (f->ae_ops[mode] == NULL) {
		netfn_flowmgr_warn("Ops not registerd for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Check if del dscp priority ops is valid
	 */
	if (f->ae_ops[mode]->del_dscp_priority == NULL) {
		netfn_flowmgr_warn("del dscp priority not registered for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Call delete dscp priority callback function registered by AEs
	 * If status is non-zero that means rule delete is failed and it
	 * will contain both errors (AE specific error and Netfn error)
	 */
	status = f->ae_ops[mode]->del_dscp_priority(rule_id);
	if (status)
		netfn_flowmgr_warn("Delete DSCP priority failed\n");

	return status;
}
EXPORT_SYMBOL(netfn_flowmgr_dscp_pri_del);

/*
 * netfn_flowmgr_get_conn_stats()
 *	API to get stats of a connection based on tuple info.
 */
netfn_flowmgr_ret_t netfn_flowmgr_get_conn_stats(struct netfn_flowmgr_flow_conn_stats *conn_stats, netfn_flowmgr_accel_mode_t mode)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ret_t netfn_status = NETFN_FLOWMGR_RET_SUCCESS;

	/*
	 * Check if conn_stats is valid
	 */
	if (!conn_stats) {
		netfn_flowmgr_warn("Invalid conn stats\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_CONN_STATS, 0);
	}

	/*
	 * AE mode should either be PPE or SFE
	 */
	if ((mode != NETFN_FLOWMGR_ACCEL_MODE_PPE) && (mode != NETFN_FLOWMGR_ACCEL_MODE_SFE)) {
		netfn_flowmgr_warn("Invalid accel mode\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_ACCEL_MODE, 0);
	}

	/*
	 * Ops should be registered
	 */
	if (f->ae_ops[mode] == NULL) {
		netfn_flowmgr_warn("Ops not registerd for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_OPS_NOT_REGISTERED, 0);
	}

	/*
	 * Check if get stats ops is valid
	 */
	if (f->ae_ops[mode]->get_stats == NULL) {
		netfn_flowmgr_warn("get stats no registered for mode = %d\n", mode);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_GET_STATS_NOT_REGISTERED, 0);
	}

	/*
	 * Call get stats function registered by AEs.
	 * If status is non-zero that means get stats is failed and it
	 * will contain both errors (AE specific error and Netfn error)
	 */
	netfn_status = f->ae_ops[mode]->get_stats(conn_stats);
	if (netfn_status) {
		netfn_flowmgr_warn("Get stats failed\n");
		return netfn_status;
	}

	return netfn_status;
}
EXPORT_SYMBOL(netfn_flowmgr_get_conn_stats);

/*
 * netfn_flowmgr_ipv6_stats_callback_unregister()
 *      Unregister a notifier callback for IPv6 stats from netfn flowmgr
 */
void netfn_flowmgr_ipv6_stats_callback_unregister(void)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;

	spin_lock_bh(&f->lock);
	if (f->stats_sync_v6_cb) {
                f->stats_sync_v6_cb = NULL;
                f->stats_sync_v6_data = NULL;
        }
	spin_lock_bh(&f->lock);

	netfn_flowmgr_info("Netfn IPv6 stats callback unregistered successfully");
}
EXPORT_SYMBOL(netfn_flowmgr_ipv6_stats_callback_unregister);

/*
 * netfn_flowmgr_ipv6_stats_callback_register()
 *      Register a notifier callback for IPv6 stats from netfn flowmgr
 */
netfn_flowmgr_ret_t netfn_flowmgr_ipv6_stats_callback_register(void *app_data, netfn_flowmgr_ipv6_stats_callback_t cb)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;

	spin_lock_bh(&f->lock);
	f->stats_sync_v6_cb = cb;
	f->stats_sync_v6_data = app_data;
	spin_lock_bh(&f->lock);

	netfn_flowmgr_info("Netfn IPv6 stats callback registered successfully, cb: %p", cb);
	return NETFN_FLOWMGR_RET_SUCCESS;
}
EXPORT_SYMBOL(netfn_flowmgr_ipv6_stats_callback_register);

/*
 * netfn_flowmgr_ipv4_stats_callback_unregister()
 *      Unregister a notifier callback for IPv4 stats from netfn flowmgr
 */
void netfn_flowmgr_ipv4_stats_callback_unregister(void)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;

	spin_lock_bh(&f->lock);
	if (f->stats_sync_v4_cb) {
                f->stats_sync_v4_cb = NULL;
                f->stats_sync_v4_data = NULL;
        }
	spin_lock_bh(&f->lock);

	netfn_flowmgr_info("Netfn IPv4 stats callback unregistered successfully");
}
EXPORT_SYMBOL(netfn_flowmgr_ipv4_stats_callback_unregister);

/*
 * netfn_flowmgr_ipv4_stats_callback_register()
 *      Register a notifier callback for IPv4 stats from netfn flowmgr
 */
netfn_flowmgr_ret_t netfn_flowmgr_ipv4_stats_callback_register(void *app_data, netfn_flowmgr_ipv4_stats_callback_t cb)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;

	spin_lock_bh(&f->lock);
	f->stats_sync_v4_cb = cb;
	f->stats_sync_v4_data = app_data;
	spin_lock_bh(&f->lock);

	netfn_flowmgr_info("Netfn IPv4 stats callback registered successfully, cb: %p", cb);
	return NETFN_FLOWMGR_RET_SUCCESS;
}
EXPORT_SYMBOL(netfn_flowmgr_ipv4_stats_callback_register);

/*
 * netfn_flowmgr_ae_ops_unregister()
 *	Unregister AEs create/destroy callbacks
 */
void netfn_flowmgr_ae_ops_unregister(netfn_flowmgr_ae_type_t ae_type)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_ae_ops *ae_ops;

	/*
	 * Check if ops was registered or not
	 */
	if (!f->ae_ops[ae_type]) {
		netfn_flowmgr_warn("AE ops for this type is not registered\n");
		return;
	}

	ae_ops = f->ae_ops[ae_type];
	kfree(ae_ops);
}
EXPORT_SYMBOL(netfn_flowmgr_ae_ops_unregister);

/*
 * netfn_flowmgr_ae_ops_register()
 * 	Register AEs create/destroy callbacks
 */
bool netfn_flowmgr_ae_ops_register(netfn_flowmgr_ae_type_t ae_type, struct netfn_flowmgr_ae_ops *ops)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_ae_ops *ae_ops;

	/*
	 * Check if create and destroy both ops are provided
	 */
	if (!ops->create_rule && !ops->destroy_rule) {
		netfn_flowmgr_warn("Invalid ops passed by AE\n");
		return false;
	}

	/*
	 * Check if AE type is neither PPE nor SFE
	 */
	if ((ae_type != NETFN_FLOWMGR_AE_TYPE_PPE) && (ae_type != NETFN_FLOWMGR_AE_TYPE_SFE)) {
		netfn_flowmgr_warn("Unsupported AE trying to register\n");
		return false;
	}

	/*
	 * Check if ops is already registered
	 */
	if (f->ae_ops[ae_type] != NULL) {
		netfn_flowmgr_warn("AE already registered\n");
		return false;
	}

	/*
	 * Allocations memory for ae ops
	 */
	ae_ops = (struct netfn_flowmgr_ae_ops *)kzalloc(sizeof(struct netfn_flowmgr_ae_ops), GFP_KERNEL);
	if (!ae_ops) {
		netfn_flowmgr_warn("AE ops registration failed due to memory\n");
		return false;
	}

	ae_ops->create_rule = ops->create_rule;
	ae_ops->destroy_rule = ops->destroy_rule;
	ae_ops->get_stats = ops->get_stats;
	ae_ops->add_dscp_priority = ops->add_dscp_priority;
	ae_ops->del_dscp_priority = ops->del_dscp_priority;

	/*
	 * Storing the ops in global structure pointer
	 */
	f->ae_ops[ae_type] = ae_ops;

	netfn_flowmgr_info("AE ops registration done successfully\n");
	return true;
}
EXPORT_SYMBOL(netfn_flowmgr_ae_ops_register);

/*
 * netfn_flowmgr_ipv6_stats_sync_period_handler()
 *      Set ipv6_stats_sync_period for netfn flowmgr.
 */
static int netfn_flowmgr_ipv6_stats_sync_period_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	int current_value;

	/*
	 * Take the current value
	 */
	current_value = ipv6_stats_sync_period;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return failure.
		 */
		return ret;
	}

	if (ipv6_stats_sync_period == current_value) {
		netfn_flowmgr_info("New ipv6 sync timer is equal to old timer\n");
		return -EINVAL;
	}

	return ret;
}

/*
 * netfn_flowmgr_ipv4_stats_sync_period_handler()
 *      Set ipv4_stats_sync_period for netfn flowmgr.
 */
static int netfn_flowmgr_ipv4_stats_sync_period_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	int current_value;

	/*
	 * Take the current value
	 */
	current_value = ipv4_stats_sync_period;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return failure.
		 */
		return ret;
	}

	if (ipv4_stats_sync_period == current_value) {
		netfn_flowmgr_info("New ipv4 sync timer is equal to old timer\n");
		return -EINVAL;
	}

	return ret;
}

/*
 * netfn_flowmgr_static_dbg_level_handler()
 *      Set static debug level for netfn flowmgr.
 */
static int netfn_flowmgr_static_dbg_level_handler(struct ctl_table *table,
		int write, void __user *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	char *level_str;
	enum netfn_flowmgr_static_dbg_level dbg_level;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	level_str = static_dbg_level_str;
	printk("dbg_level: %s", level_str);

	if (!strcmp(level_str, "warn")) {
		dbg_level = NETFN_FLOWMGR_STATIC_DBG_LEVEL_WARN;
	} else if (!strcmp(level_str, "info")) {
		dbg_level = NETFN_FLOWMGR_STATIC_DBG_LEVEL_INFO;
	} else if (!strcmp(level_str, "trace")) {
		dbg_level = NETFN_FLOWMGR_STATIC_DBG_LEVEL_TRACE;
	} else if (!strcmp(level_str, "none")) {
		dbg_level = NETFN_FLOWMGR_STATIC_DBG_LEVEL_NONE;
	} else {
		printk("Usage: echo '[warn|info|trace|none]' > /proc/sys/netfn-mgr/flowmgr/static_dbg_level\n");
		return -EINVAL;
	}

	if (dbg_level >= NETFN_FLOWMGR_DEBUG_LEVEL) {
		printk("debug level: %d not compiled in: %d\n", dbg_level, NETFN_FLOWMGR_DEBUG_LEVEL);
		return -EINVAL;
	}

	static_dbg_level = dbg_level;
	return ret;
}

/*
 * netfn_flowmgr_sub
 *	Netfn flowmgr sub directory
 */
static struct ctl_table netfn_flowmgr_sub[] = {
	{
		.procname       =       "static_dbg_level",
		.data           =       &static_dbg_level_str,
		.maxlen         =       sizeof(char) * NETFN_FLOWMGR_STATIC_DBG_LEVEL_STR_LEN,
		.mode           =       0644,
		.proc_handler   =       netfn_flowmgr_static_dbg_level_handler
	},
	{
		.procname       =       "ipv4_stats_sync_period",
		.data           =       &ipv4_stats_sync_period,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       netfn_flowmgr_ipv4_stats_sync_period_handler,
	},
	{
		.procname       =       "ipv6_stats_sync_period",
		.data           =       &ipv6_stats_sync_period,
		.maxlen         =       sizeof(int),
		.mode           =       0644,
		.proc_handler   =       netfn_flowmgr_ipv6_stats_sync_period_handler,
	},
	{}
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
/*
 * netfn_flowmgr_main
 *      Netfn flowmgr main directory
 */
static struct ctl_table netfn_flowmgr_main[] = {
	{
		.procname       =       "flowmgr",
		.mode           =       0555,
		.child          =       netfn_flowmgr_sub,
	},
	{}
};

/*
 * netfn_flowmgr_root
 *	Netfn flowmgr root directory
 */
static struct ctl_table netfn_flowmgr_root[] = {
	{
		.procname       =       "netfn",
		.mode           =       0555,
		.child          =       netfn_flowmgr_main,
	},
	{}
};
#endif

/*
 * netfn_flowmgr_ae_deinit()
 *      Netfn flow manager AEs deinitialization
 */
void netfn_flowmgr_ae_deinit(void)
{
#ifdef NETFN_FLOWMGR_AE_PPE_ENABLE
	netfn_flowmgr_ppe_deinit();
#endif
#ifdef NETFN_FLOWMGR_AE_SFE_ENABLE
	netfn_flowmgr_sfe_deinit();
#endif
}

/*
 * netfn_flowmgr_ae_init()
 *      Netfn flow manager AEs initialization
 */
int netfn_flowmgr_ae_init(void) {
	bool ppe_status = false;
	bool sfe_status = false;
#ifdef NETFN_FLOWMGR_AE_PPE_ENABLE
	ppe_status = netfn_flowmgr_ppe_init();
	if(!ppe_status)
		netfn_flowmgr_warn("PPE netfn initialization failed\n");
#endif
#ifdef NETFN_FLOWMGR_AE_SFE_ENABLE
	sfe_status = netfn_flowmgr_sfe_init();
	if(!sfe_status)
		netfn_flowmgr_warn("SFE netfn initialization failed\n");
#endif
	if (!ppe_status && !sfe_status)
		return -1;

	return 0;
}

/*
 * netfn_flowmgr_exit_module()
 *	Netfn flow manager exit function
 */
void __exit netfn_flowmgr_exit_module(void)
{
	/*
	 * Unregister sysctl framework
	 */
	netfn_flowmgr_ae_deinit();

	/*
	 * Unregister sysctl framework
	 */
	unregister_sysctl_table(netfn_flowmgr_gbl.netfn_flowmgr_header);
	netfn_flowmgr_gbl.netfn_flowmgr_header = NULL;
	netfn_flowmgr_info("module unloaded\n");
}

/*
 * netfn_flowmgr_init_module()
 *	Netfn flow manager init function
 */
static int __init netfn_flowmgr_init_module(void)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	int debugfs_status = 0;
	int ret = 0;

	/*
	 * Initialize lock
	 */
	spin_lock_init(&f->lock);

	/*
	 * AEs initialization
	 */
	ret = netfn_flowmgr_ae_init();
	if (ret) {
		netfn_flowmgr_warn("Failed to initialize AEs, error=%d\n", ret);
		return -EIO;
	}

	/*
	 * Register sysctl framework
	 * TODO: Change the sysctl regsitration as per the newer kernel.
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	netfn_flowmgr_gbl.netfn_flowmgr_header = register_sysctl_table(netfn_flowmgr_root);
#else
	netfn_flowmgr_gbl.netfn_flowmgr_header = register_sysctl("netfn/flowmgr/", netfn_flowmgr_sub);
#endif
	if (!netfn_flowmgr_gbl.netfn_flowmgr_header) {
		netfn_flowmgr_warn("sysctl table configuration failed");
		return -EINVAL;
	}

	/*
	 * Initialization of debugfs
	 */
	debugfs_status = netfn_flowmgr_stats_debugfs_init();
	if (debugfs_status)
		netfn_flowmgr_warn("Netfn flow manager debugfs initialization failed");

	netfn_flowmgr_info("Netfn flow manager module loaded\n");

	return ret;
}

module_init(netfn_flowmgr_init_module);
module_exit(netfn_flowmgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Network function manager");
