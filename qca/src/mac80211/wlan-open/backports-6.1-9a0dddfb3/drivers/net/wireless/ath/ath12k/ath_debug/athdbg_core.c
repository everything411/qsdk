// SPDX-License-Identifier: BSD-3-Clause-Clear
/*Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/devcoredump.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include "athdbg_minidump.h"
#include "athdbg_core.h"
#include "athdbg_wmi_recording.h"
#include "athdbg_uio.h"
#include "../debug.h"
#include "athdbg_netlink.h"

MODULE_SOFTDEP("post: ath12k ath12k_wifi7");

struct ath_debug_base *athdbg_base;

static bool athdbg_drv_ready(struct ath12k_base *ab)
{
	if (!ab)
		return FALSE;

	if ((athdbg_base->dbg_to_ath_ops != NULL &&
			(athdbg_base->dbg_to_ath_ops->dev_running_status(ab) != TRUE))) {
		pr_err("athdbg_core: dev[%s] not running", dev_name(ab->dev));
		return FALSE;
	}

	return TRUE;
}

u64 athdbg_conv_str_to_dbgmask(const char *dbgmask)
{
	if (!strcmp(dbgmask, "ahb"))
		return ATH12K_DBG_AHB;
	else if (!strcmp(dbgmask, "wmi"))
		return ATH12K_DBG_WMI;
	else if (!strcmp(dbgmask, "htc"))
		return ATH12K_DBG_HTC;
	else if (!strcmp(dbgmask, "htt"))
		return ATH12K_DBG_DP_HTT;
	else if (!strcmp(dbgmask, "mac"))
		return ATH12K_DBG_MAC;
	else if (!strcmp(dbgmask, "boot"))
		return ATH12K_DBG_BOOT;
	else if (!strcmp(dbgmask, "qmi"))
		return ATH12K_DBG_QMI;
	else if (!strcmp(dbgmask, "data"))
		return ATH12K_DBG_DATA;
	else if (!strcmp(dbgmask, "mgmt"))
		return ATH12K_DBG_MGMT;
	else if (!strcmp(dbgmask, "reg"))
		return ATH12K_DBG_REG;
	else if (!strcmp(dbgmask, "hal"))
		return ATH12K_DBG_HAL;
	else if (!strcmp(dbgmask, "pci"))
		return ATH12K_DBG_PCI;
	else if (!strcmp(dbgmask, "dp_tx"))
		return ATH12K_DBG_DP_TX;
	else if (!strcmp(dbgmask, "dp_rx"))
		return ATH12K_DBG_DP_RX;
	else if (!strcmp(dbgmask, "wow"))
		return ATH12K_DBG_WOW;
#ifndef CONFIG_UPSTREAM_BUILD
	else if (!strcmp(dbgmask, "fst"))
		return ATH12K_DBG_DP_FST;
	else if (!strcmp(dbgmask, "mlme"))
		return ATH12K_DBG_MLME;
	else if (!strcmp(dbgmask, "eapol"))
		return ATH12K_DBG_EAPOL;
	else if (!strcmp(dbgmask, "peer"))
		return ATH12K_DBG_PEER;
	else if (!strcmp(dbgmask, "scan"))
		return ATH12K_DBG_SCAN;
	else if (!strcmp(dbgmask, "assoc"))
		return ATH12K_DBG_ASSOC;
	else if (!strcmp(dbgmask, "roam"))
		return ATH12K_DBG_ROAM;
	else if (!strcmp(dbgmask, "dfs"))
		return ATH12K_DBG_DFS;
	else if (!strcmp(dbgmask, "crypto"))
		return ATH12K_DBG_CRYPTO;
	else if (!strcmp(dbgmask, "offchan"))
		return ATH12K_DBG_OFFCHAN;
	else if (!strcmp(dbgmask, "action"))
		return ATH12K_DBG_ACTION;
	else if (!strcmp(dbgmask, "mlo"))
		return ATH12K_DBG_MLO;
	else if (!strcmp(dbgmask, "power"))
		return ATH12K_DBG_POWER;
	else if (!strcmp(dbgmask, "tx_mon"))
		return ATH12K_DBG_DP_MON_TX;
#endif
	else
		return 0;
}

int athdbg_dbgmask_to_str(u64 mask, char *buf, size_t buflen)
{
	int n = 0;
	const char *sep = "";

	struct { u64 bit; const char *name; } map[] = {
		{ ATH12K_DBG_AHB, "ahb" },
		{ ATH12K_DBG_WMI, "wmi" },
		{ ATH12K_DBG_HTC, "htc" },
		{ ATH12K_DBG_DP_HTT, "htt" },
		{ ATH12K_DBG_MAC, "mac" },
		{ ATH12K_DBG_BOOT, "boot" },
		{ ATH12K_DBG_QMI, "qmi" },
		{ ATH12K_DBG_DATA, "data" },
		{ ATH12K_DBG_MGMT, "mgmt" },
		{ ATH12K_DBG_REG, "reg" },
		{ ATH12K_DBG_TESTMODE, "testmode" },
		{ ATH12K_DBG_HAL, "hal" },
		{ ATH12K_DBG_PCI, "pci" },
		{ ATH12K_DBG_DP_TX, "dp_tx" },
		{ ATH12K_DBG_DP_RX, "dp_rx" },
		{ ATH12K_DBG_WOW, "wow" },
#ifndef CONFIG_UPSTREAM_BUILD
		{ ATH12K_DBG_DP_FST, "fst" },
		{ ATH12K_DBG_MLME, "mlme" },
		{ ATH12K_DBG_EAPOL, "eapol" },
		{ ATH12K_DBG_PEER, "peer" },
		{ ATH12K_DBG_SCAN, "scan" },
		{ ATH12K_DBG_ASSOC, "assoc" },
		{ ATH12K_DBG_ROAM, "roam" },
		{ ATH12K_DBG_DFS, "dfs" },
		{ ATH12K_DBG_CRYPTO, "crypto" },
		{ ATH12K_DBG_OFFCHAN, "offchan" },
		{ ATH12K_DBG_ACTION, "action" },
		{ ATH12K_DBG_MLO, "mlo" },
		{ ATH12K_DBG_POWER, "power" },
		{ ATH12K_DBG_DP_MON_TX, "tx_mon" },
#endif
	};

	int i;

	if (!buf || buflen == 0)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (mask & map[i].bit) {
			n += scnprintf(buf + n, buflen - n, "%s%s", sep, map[i].name);
			sep = ":";
			if (n >= buflen)
				break;
		}
	}

	if (n == 0)
		n = scnprintf(buf, buflen, "0");

	return n;
}

static void athdbg_process_request(struct work_struct *work)
{
	struct ath_debug_base *athdbg_base = container_of(work, struct ath_debug_base, dbg_wk);
	struct athdbg_request *dbg_req;
	int ret;

	mutex_lock(&athdbg_base->req_lock);

	while (!list_empty(&athdbg_base->req_list)) {
		dbg_req = list_first_entry(&athdbg_base->req_list,
					 struct athdbg_request, req_list);
		list_del(&dbg_req->req_list);
		mutex_unlock(&athdbg_base->req_lock);

		switch (dbg_req->req_type) {
		case ATH_DBG_REQ_SETMASK:
			if (athdbg_drv_ready(dbg_req->ab) != TRUE) {
				pr_err("athdbg_core: Setmask Failure - device not running");
			}
			athdbg_base->dbg_to_ath_ops->set_dbg_mask(dbg_req->data);
			break;
		case ATH_DBG_REQ_COLLECT_MINI_DUMP:
			athdbg_process_minidump_request(dbg_req->ab, dbg_req);
			break;
		case ATH_DBG_REQ_ENABLE_QDSS:
			athdbg_config_qdss(dbg_req->ab);
			break;

		case ATH_DBG_REQ_DUMP_QDSS:
		{
			ret = athdbg_send_qdss_trace_mode_req(dbg_req->ab,
							QMI_WLANFW_QDSS_TRACE_OFF_V01,
							dbg_req->data);
			if (ret < 0)
				pr_warn("athdbg_core: Failed to stop QDSS: %d\n", ret);
		}
			break;

		case ATH_DBG_REQ_WMI_ENABLE:
		case ATH_DBG_REQ_WMI_DUMP:
		case ATH_DBG_REQ_WMI_VERBOSITY:
			athdbg_process_wmi_request(dbg_req->ab, dbg_req);
			break;

		case ATH_DBG_REQ_UNKNOWN:
			pr_err("athdbg_core: Unknown Request");
			break;
		}

		kfree(dbg_req);
		mutex_lock(&athdbg_base->req_lock);
	}
	mutex_unlock(&athdbg_base->req_lock);
}

static int __init athdbg_driver_init(void)
{
	struct ath_debug_base *athdbg;

	athdbg = kzalloc(sizeof(struct ath_debug_base), GFP_KERNEL);

	if (!athdbg) {
		pr_err("athdbg_core: alloc failure");
		return -ENOMEM;
	}

	athdbg_base = athdbg;

	athdbg_base->dbg_wq = create_singlethread_workqueue("athdbg_wq");

	if (!athdbg_base->dbg_wq) {
		kfree(athdbg_base);
		athdbg_base = NULL;
		return -ENOMEM;
	}

	if (athdbg_netlink_init())
		pr_err("athdbg_core: genl family register failed for FW_SS_HANDLER, netlink unavailable\n");

	athdbg_create_minidump_struct_list();

	INIT_WORK(&athdbg_base->dbg_wk, athdbg_process_request);
	INIT_LIST_HEAD(&athdbg_base->req_list);
	mutex_init(&athdbg_base->req_lock);
	spin_lock_init(&athdbg_base->uio_lock);

#ifdef CPTCFG_ATHDEBUG_UIO_LOGGING
	if (athdbg_uio_register()) {
		pr_err("athdbg_core: UIO register failed\n");
		return -EINVAL;
	}
#endif

	return 0;
}

EXPORT_SYMBOL(athdbg_base);

static void __exit athdbg_driver_exit(void)
{
	athdbg_clear_minidump_struct_list();
	cancel_work_sync(&athdbg_base->dbg_wk);
	destroy_workqueue(athdbg_base->dbg_wq);
#ifdef CPTCFG_ATHDEBUG_UIO_LOGGING
	athdbg_uio_unregister();
#endif
	athdbg_netlink_exit();
	kfree(athdbg_base);
}

module_init(athdbg_driver_init);
module_exit(athdbg_driver_exit);

MODULE_DESCRIPTION("Driver support debug options for Qualcomm Technologies WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
