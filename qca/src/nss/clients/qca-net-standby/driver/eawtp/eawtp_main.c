/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 *
 */

#include <linux/kernel.h>
#include <soc/qcom/eawtp.h>
#include <linux/module.h>

/* Global context  */
struct eawtp_reg_info eawtp_info;

/*
 * eawtp_get_active_ports()
 *	Get the number of active ports.
 */
int eawtp_get_active_ports(void *app_data, struct eawtp_port_info *pinfo)
{
	if (!eawtp_info.get_nss_active_port_cnt_cb) {
		pr_debug("\nCannot get active port cnt\n");
		return -1;
	}

	eawtp_info.get_nss_active_port_cnt_cb(app_data, pinfo);
	return 0;
}

/*
 * eawtp_ntfy_active_ports()
 *	Notify number of active ports.
 */
int eawtp_ntfy_active_ports(void *app_data, struct eawtp_port_info *pinfo)
{
	if (!eawtp_info.ntfy_port_status_to_wifi_cb) {
		pr_debug("\nNotify link state fail\n");
		return -1;
	}

	eawtp_info.ntfy_port_status_to_wifi_cb(app_data, pinfo);
	return 0;
}

/*
 * eawtp_module_init()
 *	module init for network eawtp standby
 */
static int __init eawtp_module_init(void)
{
	eawtp_info.get_active_ports_cb = eawtp_get_active_ports;
	eawtp_info.ntfy_active_ports_cb = eawtp_ntfy_active_ports;
	eawtp_nss_get_and_register_cb(&eawtp_info);
	eawtp_wifi_get_and_register_cb(&eawtp_info);
	if (eawtp_info.port_link_notify_register_cb) {
		eawtp_info.port_link_notify_register_cb(&eawtp_info);
	}

	return 0;
}

/*
 * eawtp_module_exit()
 *	module exit for network eawtp standby
 */
static void __exit eawtp_module_exit(void)
{
	if (eawtp_info.port_link_notify_unregister_cb) {
		eawtp_info.port_link_notify_unregister_cb(&eawtp_info);
	}

	eawtp_nss_unregister_cb();
	eawtp_wifi_unregister_cb();
	eawtp_info.get_active_ports_cb = NULL;
	eawtp_info.ntfy_active_ports_cb = NULL;
	eawtp_info.get_nss_active_port_cnt_cb = NULL;
	eawtp_info.ntfy_port_status_to_wifi_cb = NULL;
}

module_init(eawtp_module_init);
module_exit(eawtp_module_exit);

MODULE_DESCRIPTION("Network Standby EAWTP Kernel Module");
MODULE_LICENSE("Dual BSD/GPLv2");

