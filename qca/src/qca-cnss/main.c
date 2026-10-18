/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/rwsem.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/coresight.h>
#include <linux/remoteproc.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
#include <linux/remoteproc/qcom_rproc.h>
#endif
#include <linux/of_address.h>
#ifdef CONFIG_QCOM_SOCINFO
#include <soc/qcom/socinfo.h>
#endif
#include <linux/firmware.h>
#include <linux/major.h>

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
#include <soc/qcom/subsystem_notif.h>
#endif

#ifdef KERNEL_SUPPORTS_QGIC2M
#include <soc/qcom/qgic2m.h>
#endif
#ifdef CONFIG_CNSS2_LEGACY_IRQ
#include "legacyirq/legacyirq.h"
#endif

#include "main.h"
#ifdef CNSS_DEBUG_SUPPORT
#include "debug/debug.h"
#endif
#if defined CNSS_PCI_SUPPORT
#include "pci/pci.h"
#endif
#include "cnss_common/cnss_common.h"
#include "qmi/qmi.h"
#include "bus/bus.h"
#include "genl/genl.h"
#include "qmi/cnss_plat_ipc_qmi.h"
#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
#include <linux/devcoredump.h>
#include <linux/elf.h>
#include <linux/panic_notifier.h>
#else
#include <soc/qcom/ramdump.h>
#endif

#define CNSS_DUMP_FORMAT_VER		0x11
#define CNSS_DUMP_FORMAT_VER_V2		0x22
#define CNSS_DUMP_MAGIC_VER_V2		0x42445953
#define CNSS_DUMP_NAME			"CNSS_WLAN"
#define CNSS_DUMP_DESC_SIZE		0x1000
#define CNSS_MHI_SEG_LEN		SZ_512K
#define CNSS_DUMP_DESC_TOLERANCE	64
#define CNSS_DUMP_SEG_VER		0x1
#define CNSS_DUMP_SEG_VER_V2		0x2
#define WLAN_RECOVERY_DELAY		1000
#define FILE_SYSTEM_READY		1
#define FW_ASSERT_TIMEOUT		5000
#define CNSS_EVENT_PENDING		2989

#define RPROC_ROOTPD_NAME		"remoteproc"

#define CNSS_QUIRKS_DEFAULT		0
#ifdef CONFIG_CNSS_EMULATION
#define CNSS_MHI_TIMEOUT_DEFAULT	3600
#else
#define CNSS_MHI_TIMEOUT_DEFAULT	60
#endif
#define CNSS_QMI_TIMEOUT_DEFAULT	10000
#define CNSS_BDF_TYPE_DEFAULT		CNSS_BDF_ELF
#define CNSS_TIME_SYNC_PERIOD_DEFAULT	900000
#define DEFAULT_FW_FILE_NAME		"amss.bin"
#define DUALMAC_FW_FILE_NAME		"amss_dualmac.bin"

#define CNSS_INTX_SUPPORT_MASK          0xF
#define CNSS_INTX_SUPPORT_SHIFT         4

#define MAX_NUMBER_OF_SOCS		5
#define CNSS_PROBE_ORDER_MASK		0xF
#define CNSS_PROBE_ORDER_DEFAULT	0xFF
#define CNSS_DEFAULT_MLO_CHIP_BITMASK	0xFF
#define CNSS_PROBE_ORDER_SHIFT		4
#ifdef CONFIG_CNSS2_KERNEL_5_15
#define POWER_ON_RETRY_MAX_TIMES        4
#define POWER_ON_RETRY_DELAY_MS         500
#endif

struct cnss_plat_data *plat_env[MAX_NUMBER_OF_SOCS];
int plat_env_index;
struct cnss_mlo_group_info g_mlo_group_info[CNSS_MAX_MLO_GROUPS];
static DEFINE_SPINLOCK(plat_env_spinlock);
static DEFINE_SPINLOCK(rddm_spinlock);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static DEFINE_MUTEX(rproc_list_mutex);
struct notifier_block panic_nb;
#endif

#ifdef CONFIG_CNSS2_PM
static DECLARE_RWSEM(cnss_pm_sem);
#endif

static unsigned int qmi_timeout;
module_param(qmi_timeout, uint, 0600);
MODULE_PARM_DESC(qmi_timeout, "Timeout for QMI message in milliseconds");

bool ramdump_enabled;
module_param(ramdump_enabled, bool, 0600);
MODULE_PARM_DESC(ramdump_enabled, "ramdump_enabled");

static int bdf_integrated;
module_param(bdf_integrated, int, 0644);
MODULE_PARM_DESC(bdf_integrated, "bdf_integrated");

static int bdf_pci0;
module_param(bdf_pci0, int, 0644);
MODULE_PARM_DESC(bdf_pci0, "bdf_pci0");

static int bdf_pci1;
module_param(bdf_pci1, int, 0644);
MODULE_PARM_DESC(bdf_pci1, "bdf_pci1");

static int bdf_pci2;
module_param(bdf_pci2, int, 0644);
MODULE_PARM_DESC(bdf_pci1, "bdf_pci2");

static int bdf_pci3;
module_param(bdf_pci3, int, 0644);
MODULE_PARM_DESC(bdf_pci1, "bdf_pci3");

int timeout_factor = 1;
module_param(timeout_factor, int, 0644);
MODULE_PARM_DESC(timeout_factor, "timeout_factor");

static unsigned int driver_mode;
module_param(driver_mode, uint, 0644);
MODULE_PARM_DESC(driver_mode, "Global driver mode");

static int skip_cnss;
module_param(skip_cnss, int, 0644);
MODULE_PARM_DESC(skip_cnss, "skip_cnss");

static int skip_radio_bmap;
module_param(skip_radio_bmap, int, 0644);
MODULE_PARM_DESC(skip_radio_bmap, "Bitmap to skip device probe");

static int disable_caldata_bmap;
module_param(disable_caldata_bmap, int, 0644);
MODULE_PARM_DESC(disable_caldata_bmap, "Bitmap to Disable Caldata download");

static int disable_regdb_bmap;
module_param(disable_regdb_bmap, int, 0644);
MODULE_PARM_DESC(disable_regdb_bmap, "Bitmap to Disable RegDB download");

/* probe_order needs to be defined in the format of hex.
 * The order of socX can be rearranged based on the given value.
 * For example, if default order is Soc0->Soc1->Soc2, then 0x213 will make
 * the order as Soc1->Soc0->Soc2.
 * If probe_order is 0 or not specified, then default order will be takes place.
 */
static unsigned int probe_order;
module_param(probe_order, uint, 0644);
MODULE_PARM_DESC(probe_order, "Probe order");

static int enable_intx_bmap;
module_param(enable_intx_bmap, int, 0644);
MODULE_PARM_DESC(enable_intx_bmap, "enable_intx_bmap");

static unsigned int mlo_max_peer;
module_param(mlo_max_peer, uint, 0600);
MODULE_PARM_DESC(mlo_max_peer, "MLO max peer");

#define FW_READY_DELAY	100  /* in msecs */

/* In platforms with low power CPU like IPQ5018 or SDX65, if CPU load
 * is high FW_READY might take longer than default value of 15s.
 * Increasing FW_READY timeout to 60s for IPQ5018 and SDX.
 */
#if defined(CONFIG_KASAN) || defined(CONFIG_IPQ_APSS_5018) || \
				defined(CONFIG_CNSS2_KERNEL_MSM)
static int fw_ready_timeout = 60;
static int cold_boot_cal_timeout = 180;
int rddm_done_timeout = 30;
#else
static int fw_ready_timeout = 15;
static int cold_boot_cal_timeout = 60;
int rddm_done_timeout = 15;
#endif
module_param(fw_ready_timeout, int, 0644);
MODULE_PARM_DESC(fw_ready_timeout, "fw ready timeout in seconds");

module_param(cold_boot_cal_timeout, int, 0644);
MODULE_PARM_DESC(cold_boot_cal_timeout, "Cold boot cal timeout in seconds");

module_param(rddm_done_timeout, int, 0644);
MODULE_PARM_DESC(rddm_done_timeout, "RDDM collection timeout in seconds");

static int soc_version_major;
module_param(soc_version_major, int, 0444);
MODULE_PARM_DESC(soc_version_major, "SOC Major Version");

static unsigned int enable_mlo_support = 1;
module_param(enable_mlo_support, uint, 0600);
MODULE_PARM_DESC(enable_mlo_support, "enable_mlo_support");

static unsigned int mlo_chip_bitmask = CNSS_DEFAULT_MLO_CHIP_BITMASK;
module_param(mlo_chip_bitmask, uint, 0600);
MODULE_PARM_DESC(mlo_chip_bitmask, "mlo_chip_bitmask");

/* Experimental module param to avoid FW shutdown/power on after coldboot
 * calibration. Current FW does not support this and should not be enabled
 * without FW support for this feature
 */
static unsigned int soft_switch;
module_param(soft_switch, uint, 0600);
MODULE_PARM_DESC(soft_switch, "soft_switch");

static unsigned int probe_timeout = 200;
module_param(probe_timeout, uint, 0600);
MODULE_PARM_DESC(probe_timeout, "Timeout for cnss_wlan_probe_driver");

enum skip_cnss_options {
	CNSS_SKIP_NONE,
	CNSS_SKIP_ALL,
	CNSS_SKIP_AHB,
	CNSS_SKIP_PCI
};

#define SKIP_INTEGRATED		0x1
#define SKIP_PCI_0		0x2
#define SKIP_PCI_1		0x4
#define SKIP_PCI_2		0x8
#define SKIP_PCI_3		0x10

static struct cnss_fw_files FW_FILES_QCA6174_FW_3_0 = {
	"qwlan30.bin", "bdwlan30.bin", "otp30.bin", "utf30.bin",
	"utfbd30.bin", "epping30.bin", "evicted30.bin"
};

static struct cnss_fw_files FW_FILES_DEFAULT = {
	"qwlan.bin", "bdwlan.bin", "otp.bin", "utf.bin",
	"utfbd.bin", "epping.bin", "evicted.bin"
};

struct cnss_driver_event {
	struct list_head list;
	enum cnss_driver_event_type type;
	bool sync;
	struct completion complete;
	int ret;
	void *data;
};

/* M3 Dump related global structures/variables */
static int m3_dump_major;
static struct class *m3_dump_class;
struct rproc *rproc_rootpd, *rproc_textpd;

atomic_t cal_in_progress_count;
void *cnss_register_qca8074_cb(struct cnss_plat_data *plat_priv);
int cnss_unregister_qca8074_cb(struct cnss_plat_data *plat_priv);
void *cnss_register_qcn9000_cb(struct cnss_plat_data *plat_priv);
int cnss_unregister_qcn9000_cb(struct cnss_plat_data *plat_priv);
#ifndef CONFIG_CNSS2_KERNEL_5_15
static int cnss_qca8074_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static int cnss_get_event(unsigned long subsys_event)
{
	int event = -EINVAL;

	switch (subsys_event) {
	case QCOM_SSR_BEFORE_POWERUP:
		event = CNSS_BEFORE_POWERUP;
		break;
	case QCOM_SSR_AFTER_POWERUP:
		event = CNSS_AFTER_POWERUP;
		break;
	case QCOM_SSR_BEFORE_SHUTDOWN:
		event = CNSS_BEFORE_SHUTDOWN;
		break;
	case QCOM_SSR_AFTER_SHUTDOWN:
		event = CNSS_AFTER_SHUTDOWN;
		break;
	case QCOM_SSR_NOTIFY_CRASH:
		event = CNSS_PREPARE_FOR_FATAL_SHUTDOWN;
		break;
	default:
		event = subsys_event;
		break;
	}

	return event;
}
#else
static int cnss_get_event(unsigned long subsys_event)
{
	int event = -EINVAL;

	switch (subsys_event) {
	case SUBSYS_BEFORE_SHUTDOWN:
		event = CNSS_BEFORE_SHUTDOWN;
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		event = CNSS_AFTER_SHUTDOWN;
		break;
	case SUBSYS_BEFORE_POWERUP:
		event = CNSS_BEFORE_POWERUP;
		break;
	case SUBSYS_AFTER_POWERUP:
		event = CNSS_AFTER_POWERUP;
		break;
	case SUBSYS_RAMDUMP_NOTIFICATION:
		event = CNSS_RAMDUMP_NOTIFICATION;
		break;
	case SUBSYS_POWERUP_FAILURE:
		event = CNSS_POWERUP_FAILURE;
		break;
#ifdef CONFIG_CNSS2_KERNEL_IPQ
	case SUBSYS_PREPARE_FOR_FATAL_SHUTDOWN:
		event = CNSS_PREPARE_FOR_FATAL_SHUTDOWN;
		break;
#endif
	default:
		event = subsys_event;
		break;
	}
	return event;
}
#endif
#endif

void *cnss_get_pci_dev_by_device_id(int device_id)
{
	int i;

	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i]->device_id == device_id)
			return plat_env[i]->pci_dev;
	}
	return NULL;
}
EXPORT_SYMBOL(cnss_get_pci_dev_by_device_id);

void *cnss_get_plat_priv_dev_by_pci_dev(void *pci_dev)
{
	int i;

	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i]->pci_dev == pci_dev)
			return plat_env[i];
	}
	return NULL;
}

void *cnss_get_plat_dev_by_bus_dev(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return NULL;

	return plat_priv->plat_dev;
}
EXPORT_SYMBOL(cnss_get_plat_dev_by_bus_dev);

struct cnss_plat_data *cnss_get_plat_priv_by_qrtr_node_id(int node_id)
{
	int i;

	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i]->qrtr_node_id == node_id)
			return plat_env[i];
	}
	return NULL;
}

struct cnss_plat_data *cnss_get_plat_priv_by_instance_id(int instance_id)
{
	int i;

	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i]->wlfw_service_instance_id == instance_id)
			return plat_env[i];
	}
	return NULL;
}
EXPORT_SYMBOL(cnss_get_plat_priv_by_instance_id);

void cnss_set_recovery_mode(struct device *dev, u8 recovery_mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	cnss_pr_dbg("The recovery mode is %d\n", recovery_mode);
	plat_priv->recovery_mode = recovery_mode;

}
EXPORT_SYMBOL(cnss_set_recovery_mode);

void cnss_set_wsi_remap_state(struct device *dev, bool state)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	cnss_pr_dbg("WSI remap state: %d\n", state);
	plat_priv->wsi_remap_state = state;
}
EXPORT_SYMBOL(cnss_set_wsi_remap_state);

void cnss_set_standby_mode(struct device *dev, u8 standby_mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	cnss_pr_info("The standby mode is %d\n", standby_mode);
	plat_priv->standby_mode = standby_mode;

}
EXPORT_SYMBOL(cnss_set_standby_mode);

#if defined(CNSS_LOWMEM_PROFILE) && defined(CNSS_FW_MOUNT_SUPPORT) && \
	defined(CONFIG_CNSS2_KERNEL_IPQ)
/*
 * For FW_UMOUNT feature umount_firmware_delay is used as delay to trigger
 * umount in delayed_workqueue.
 */
static int umount_firmware_delay = 10000;
module_param(umount_firmware_delay, int, 0644);
MODULE_PARM_DESC(umount_firmware_delay, "umount_firmware_delay");

/*
 * fw_load_in_progress_bmap flag is used to indicate if FW load is in progress
 * set_bit is used based on the plat_env index to represent the fw load
 * progress for each devices.
 */
static unsigned long fw_load_in_progress_bmap;
static struct delayed_work umount_firmware_wq;

/**
 * cnss_mount_firmware - Mount FW Partition to DDR
 *
 * @plat_priv: plat_priv of soc
 *
 * cnss_mount_firmware will call call_usermodehelper which helps calling the
 * script file in MOUNT_PATH path that will mount the FW partition to DDR.
 */
void cnss_mount_firmware(struct cnss_plat_data *plat_priv)
{
	char *argv[] = {MOUNT_PATH, NULL };

	set_bit(cnss_get_plat_env_index_from_plat_priv(plat_priv),
		&fw_load_in_progress_bmap);
	call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_PROC);
}

/**
 * cnss_umount_firmware - Schedule unmount handler
 *
 * @plat_priv: plat_priv of soc
 *
 * cnss_umount_firmware will cancel all pending delayed workqueues and will
 * schedule a new delayed workqueue with a delay of umount_firmware_delay for
 * the wifi load/up/recovery to be completed after which WIFI_FW will be
 * unmounted.
 */
void cnss_unmount_firmware(struct cnss_plat_data *plat_priv)
{
	clear_bit(cnss_get_plat_env_index_from_plat_priv(plat_priv),
		  &fw_load_in_progress_bmap);
	cancel_delayed_work_sync(&umount_firmware_wq);
	schedule_delayed_work(&umount_firmware_wq,
			      msecs_to_jiffies(umount_firmware_delay));
}

/**
 * cnss_schedule_umount_firmware - Unmount FW Partition
 *
 * @work: Work initialized in workqueue
 *
 * cnss_schedule_umount_firmware will call call_usermodehelper which helps in
 * calling script file in UMOUNT_PATH path that will unmount the FW partition.
 */
void cnss_schedule_umount_firmware(struct work_struct *work)
{
	char *argv[] = {UMOUNT_PATH, NULL };

	if (fw_load_in_progress_bmap == 0)
		call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_PROC);
}
#endif

struct cnss_plat_data *cnss_get_plat_priv_by_device_id(int id)
{
	int i;

	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i]->device_id == id &&
		    !plat_env[i]->pci_dev)
			return plat_env[i];
	}
	return NULL;
}

struct cnss_plat_data *cnss_get_plat_priv(struct platform_device
						 *plat_dev)
{
	int i;

	if (!plat_dev)
		return NULL;
	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i]->plat_dev == plat_dev)
			return plat_env[i];
	}
	return NULL;
}

static void cnss_sort_probe_order(void)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < plat_env_index; i++)
		for (j = i + 1; j < plat_env_index; j++)
			if (plat_env[i]->probe_order > plat_env[j]->probe_order)
				swap(plat_env[i], plat_env[j]);
}

struct cnss_plat_data *cnss_get_plat_priv_by_soc_id(int soc_id)
{
	if (soc_id >= MAX_NUMBER_OF_SOCS || soc_id >= plat_env_index)
		return NULL;

	cnss_sort_probe_order();
	return plat_env[soc_id];
}

int cnss_get_plat_env_index_from_plat_priv(struct cnss_plat_data *plat_priv)
{
	int i;

	if (!plat_priv)
		return -EINVAL;

	for (i = 0; i < plat_env_index; i++) {
		if (plat_env[i] == plat_priv)
			return i;
	}

	return -EINVAL;
}

const char *cnss_get_fw_path(struct cnss_plat_data *plat_priv)
{
	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
		return "IPQ8074/";
	case QCA6018_DEVICE_ID:
		return "IPQ6018/";
	case QCA5018_DEVICE_ID:
		return "IPQ5018/";
	case QCA9574_DEVICE_ID:
		return "IPQ9574/";
	case QCA5332_DEVICE_ID:
		return "IPQ5332/";
	case QCN9000_DEVICE_ID:
		return "qcn9000/";
	case QCN6122_DEVICE_ID:
		return "qcn6122/";
	case QCN9160_DEVICE_ID:
		return "qcn9160/";
	case QCN9224_DEVICE_ID:
		return "qcn9224/";
	case QCN6432_DEVICE_ID:
		return "qcn6432/";
	case QCA5424_DEVICE_ID:
		return "IPQ5424/";
	default:
		cnss_pr_err("No such device id 0x%lx\n", plat_priv->device_id);
	}

	return "UNKNOWN";
}

#ifdef CONFIG_CNSS2_PM
static int cnss_pm_notify(struct notifier_block *b,
			  unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&cnss_pm_sem);
		break;
	case PM_POST_SUSPEND:
		up_write(&cnss_pm_sem);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block cnss_pm_notifier = {
	.notifier_call = cnss_pm_notify,
};

static void cnss_pm_stay_awake(struct cnss_plat_data *plat_priv)
{
	if (atomic_inc_return(&plat_priv->pm_count) != 1)
		return;

	cnss_pr_dbg("PM stay awake, state: 0x%lx, count: %d\n",
		    plat_priv->driver_state,
		    atomic_read(&plat_priv->pm_count));
	pm_stay_awake(&plat_priv->plat_dev->dev);
}

static void cnss_pm_relax(struct cnss_plat_data *plat_priv)
{
	int r = atomic_dec_return(&plat_priv->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	cnss_pr_dbg("PM relax, state: 0x%lx, count: %d\n",
		    plat_priv->driver_state,
		    atomic_read(&plat_priv->pm_count));
	pm_relax(&plat_priv->plat_dev->dev);
}
void cnss_lock_pm_sem(struct device *dev)
{
	down_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_lock_pm_sem);

void cnss_release_pm_sem(struct device *dev)
{
	up_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_release_pm_sem);
#endif

int cnss_get_fw_files_for_target(struct device *dev,
				 struct cnss_fw_files *pfw_files,
				 u32 target_type, u32 target_version)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -EINVAL;

	if (!pfw_files)
		return -ENODEV;

	switch (target_version) {
	case QCA6174_REV3_VERSION:
	case QCA6174_REV3_2_VERSION:
		memcpy(pfw_files, &FW_FILES_QCA6174_FW_3_0, sizeof(*pfw_files));
		break;
	default:
		memcpy(pfw_files, &FW_FILES_DEFAULT, sizeof(*pfw_files));
		cnss_pr_err("Unknown target version, type: 0x%X, version: 0x%X",
			    target_type, target_version);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(cnss_get_fw_files_for_target);

int cnss_request_bus_bandwidth(struct device *dev, int bandwidth)
{
#ifdef CONFIG_MSM_PCI
	int ret;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_bus_bw_info *bus_bw_info;

	if (!plat_priv)
		return -ENODEV;

	bus_bw_info = &plat_priv->bus_bw_info;
	if (!bus_bw_info->bus_client)
		return -EINVAL;

	switch (bandwidth) {
	case CNSS_BUS_WIDTH_NONE:
	case CNSS_BUS_WIDTH_IDLE:
	case CNSS_BUS_WIDTH_LOW:
	case CNSS_BUS_WIDTH_MEDIUM:
	case CNSS_BUS_WIDTH_HIGH:
	case CNSS_BUS_WIDTH_VERY_HIGH:
		ret = msm_bus_scale_client_update_request
			(bus_bw_info->bus_client, bandwidth);
		if (!ret)
			bus_bw_info->current_bw_vote = bandwidth;
		else
			cnss_pr_err("Could not set bus bandwidth: %d, err = %d\n",
				    bandwidth, ret);
		break;
	default:
		cnss_pr_err("Invalid bus bandwidth: %d", bandwidth);
		ret = -EINVAL;
	}

	return ret;
#endif
	return 0;
}
EXPORT_SYMBOL(cnss_request_bus_bandwidth);

int cnss_get_platform_cap(struct device *dev, struct cnss_platform_cap *cap)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -ENODEV;

	if (cap)
		*cap = plat_priv->cap;

	return 0;
}
EXPORT_SYMBOL(cnss_get_platform_cap);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
static int cnss_cal_db_mem_update(struct cnss_plat_data *plat_priv,
				  enum cnss_cal_db_op op, u32 *size)
{
	int ret = 0;
	char filename[50];

	u32 timeout = cnss_get_qmi_timeout(plat_priv)
		      + CNSS_DAEMON_CONNECT_TIMEOUT_MS;

	if (op >= CNSS_CAL_DB_INVALID_OP)
		return -EINVAL;

	if ((plat_priv->bus_type == CNSS_BUS_PCI) ||
	    (plat_priv->device_id == QCN6122_DEVICE_ID) ||
	    (plat_priv->device_id == QCN9160_DEVICE_ID) ||
	    (plat_priv->device_id == QCN6432_DEVICE_ID))
		snprintf(filename, sizeof(filename),
			 CNSS_CAL_DB_FILE_PREFIX"_%s"CNSS_CAL_DB_FILE_SUFFIX,
			 plat_priv->device_name);
	else
		snprintf(filename, sizeof(filename),
			 CNSS_CAL_DB_FILE_PREFIX CNSS_CAL_DB_FILE_SUFFIX);

	cnss_pr_info("%s: File Operation %u size %u file name %s\n", __func__,
		     op, *size, filename);

	if (*size == 0) {
		cnss_pr_err("Invalid cal file size\n");
		return -EINVAL;
	}

	/* Ensure cnss-daemon is connected */
	if (!is_ipc_qmi_client_connected(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
					 timeout)) {
		cnss_pr_err("Daemon not yet connected\n");
		CNSS_ASSERT(0);
		return ret;
	}

	if (!plat_priv->cal_mem->va) {
		cnss_pr_err("CAL DB Memory not setup for FW\n");
		return -EINVAL;
	}

	/* Copy CAL DB file contents to/from CAL_TYPE_DDR mem allocated to FW */
	if (op == CNSS_CAL_DB_DOWNLOAD) {
		cnss_pr_dbg("Initiating Calibration file download to mem\n");
		ret = cnss_plat_ipc_qmi_file_download(
					 CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
					 filename, plat_priv->cal_mem->va,
					 size);
	} else {
		cnss_pr_dbg("Initiating Calibration mem upload to file\n");
		ret = cnss_plat_ipc_qmi_file_upload(
					 CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
					 filename, plat_priv->cal_mem->va,
					 *size);
	}

	if (ret)
		cnss_pr_err("Cal DB file %s %s failure\n",
			    filename,
			    op == CNSS_CAL_DB_DOWNLOAD ? "download" : "upload");
	else
		cnss_pr_dbg("Cal DB file %s %s size %d done\n",
			    filename,
			    op == CNSS_CAL_DB_DOWNLOAD ? "download" : "upload",
			    *size);

	return ret;
}

static int cnss_cal_mem_upload_to_file(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->cal_file_size > plat_priv->cal_mem->size) {
		cnss_pr_err("Cal file size is larger than Cal DB Mem size\n");
		return -EINVAL;
	}
	return cnss_cal_db_mem_update(plat_priv, CNSS_CAL_DB_UPLOAD,
				      &plat_priv->cal_file_size);
}

int cnss_cal_file_download_to_mem(struct cnss_plat_data *plat_priv,
				  u32 *cal_file_size)
{
	/* To download pass the total size of cal DB mem allocated.
	 * After cal file is download to mem, its size is updated in
	 * return pointer
	 */
	*cal_file_size = plat_priv->cal_mem->size;
	return cnss_cal_db_mem_update(plat_priv, CNSS_CAL_DB_DOWNLOAD,
				      cal_file_size);
}

static void cnss_cal_report_upload(struct cnss_plat_data *plat_priv)
{
	/* Send cal upload req to cnss-daemon after confirming that
	 * it is connected to cnss2 over QMI.
	 */
	if (is_ipc_qmi_client_connected
			(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01, 0))
		cnss_cal_mem_upload_to_file(plat_priv);
}
#else
static void cnss_cal_report_upload(struct cnss_plat_data *plat_priv)
{
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static void cnss_hif_notifier(struct cnss_plat_data *plat_priv,
				enum cnss_notif_type code)
{
	struct cnss_wlan_driver *driver_ops = NULL;
	enum cnss_notif_type event_code = code;

	if (!plat_priv->cal_in_progress)
		driver_ops = plat_priv->driver_ops;

	if (event_code == CNSS_AFTER_POWERUP) {
		if (driver_ops) {
			driver_ops->probe((struct pci_dev *)plat_priv->plat_dev,
					  (const struct pci_device_id *)
					  plat_priv->plat_dev_id);
			set_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
	} else if (event_code == CNSS_BEFORE_SHUTDOWN) {
		if (driver_ops) {
			driver_ops->remove(
					(struct pci_dev *)plat_priv->plat_dev);

			clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
			clear_bit(CNSS_DEV_ERR_NOTIFY,
					&plat_priv->driver_state);
		}
	} else if (event_code == CNSS_RAMDUMP_NOTIFICATION) {
		if (driver_ops) {
			driver_ops->reinit(
					(struct pci_dev *)plat_priv->plat_dev,
					(const struct pci_device_id *)
					plat_priv->plat_dev_id);

			clear_bit(CNSS_DRIVER_RECOVERY,
					&plat_priv->driver_state);
		}
	} else {
		if (driver_ops)
			driver_ops->update_status(
					(struct pci_dev *)plat_priv->plat_dev,
					(const struct pci_device_id *)
					plat_priv->plat_dev_id,
					(int)event_code);
	}
}

static int cnss_hif_power_up(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_hif_notifier(plat_priv, CNSS_BEFORE_POWERUP);
	plat_priv->target_asserted = 0;
	plat_priv->target_assert_timestamp = 0;
	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
#if defined CNSS_PCI_SUPPORT
	ret = cnss_pci_probe(plat_priv->pci_dev,
			plat_priv->pci_dev_id,
			plat_priv);
	if (ret) {
		pr_err("ERROR : %s:%d ret %d\n", __func__, __LINE__, ret);
		return -ENODEV;
	}
#endif
	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret) {
		cnss_pr_err("%s: cnss_bus_dev_powerup failed(%d)\n", __func__,
			ret);
		CNSS_ASSERT(0);
	}
	cnss_hif_notifier(plat_priv, CNSS_AFTER_POWERUP);

	return ret;
}


static int cnss_hif_shutdown(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_hif_notifier(plat_priv, CNSS_BEFORE_SHUTDOWN);

	if (!plat_priv->driver_state) {
		cnss_pr_dbg("shutdown is ignored\n");
		return 0;
	}
	ret = cnss_bus_dev_shutdown(plat_priv);
	if (ret != 0) {
		cnss_pr_err("%s: cnss_bus_dev_shutdown failed(%d)\n", __func__,
			    ret);
	}
	cnss_hif_notifier(plat_priv, CNSS_AFTER_SHUTDOWN);

	return 0;
}

void *__cnss_hif_get(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return NULL;

	cnss_pr_info("%s: driver_state: 0x%lx\n", __func__,
		     plat_priv->driver_state);

	clear_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER, &plat_priv->driver_state);
	ret = cnss_hif_power_up(plat_priv);
	if (ret) {
		cnss_pr_err("%s: cnss_hif_power_up failed %s\n", __func__,
			plat_priv->device_name);
		goto fail;
	}

	/* Return plat_priv for successful power up */
	return plat_priv;
fail:
	CNSS_ASSERT(0);
	return NULL;
}

void __cnss_hif_put(struct cnss_plat_data *plat_priv)
{
	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);

	if (!plat_priv)
		return;

	cnss_hif_shutdown(plat_priv);
	plat_priv->driver_state = 0;
}
#endif

int __cnss_wlan_enable(struct cnss_plat_data *plat_priv,
		       struct cnss_wlan_enable_cfg *config,
		       enum cnss_driver_mode mode,
		       const char *host_version)
{
	int ret;
	u32 cal_file_size = 0;

	if (!plat_priv)
		return 0;

	cal_file_size = plat_priv->cal_file_size;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	if (mode == CNSS_CALIBRATION || mode == CNSS_WALTEST ||
	    mode == CNSS_FTM_CALIBRATION)
		goto skip_cfg;

	if (!config || !host_version) {
		cnss_pr_err("Invalid config or host_version pointer\n");
		return -EINVAL;
	}

	/* Set wmi diag logging */
	if (!(plat_priv->device_id == QCA8074_DEVICE_ID ||
	      plat_priv->device_id == QCA8074V2_DEVICE_ID ||
	      plat_priv->device_id == QCA6018_DEVICE_ID))
		cnss_wlfw_ini_send_sync(plat_priv, 1);

	cnss_pr_dbg("Mode: %d, config: %pK, host_version: %s\n",
		    mode, config, host_version);

	if (mode == CNSS_WALTEST || mode == CNSS_CCPM)
		goto skip_cfg;

	if (mode != CNSS_CALIBRATION && mode != CNSS_FTM_CALIBRATION) {
		ret = cnss_wlfw_wlan_cfg_send_sync(plat_priv, config,
						   host_version);
		if (ret)
			goto out;
	}

skip_cfg:

	if (mode == CNSS_CALIBRATION || mode == CNSS_FTM_CALIBRATION) {
		set_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);
		/* Only check whether cnss-daemon is connected.
		 * It is not required to wait until it gets connected here.
		 * Hence pass the timeout value as 0.
		 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
		plat_priv->cal_time = jiffies;
#else
		if (is_ipc_qmi_client_connected
				(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01, 0)) {
			cnss_pr_dbg("%s: cal_file_size %u !\n", __func__,
				    cal_file_size);
			cnss_wlfw_cal_report_req_send_sync(plat_priv,
							   cal_file_size);
			plat_priv->cal_time = jiffies;
		}
#endif
	}

	ret = cnss_wlfw_wlan_mode_send_sync(plat_priv, mode);

	if (plat_priv->qdss_support & (1 << mode)) {
		cnss_pr_info("Starting QDSS for %s\n", plat_priv->device_name);
		cnss_wlfw_qdss_dnld_send_sync(plat_priv);
	}

out:
	return ret;
}

int cnss_wlan_enable(struct device *dev,
		     struct cnss_wlan_enable_cfg *config,
		     enum cnss_driver_mode mode,
		     const char *host_version)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	return __cnss_wlan_enable(plat_priv, config, mode, host_version);

}
EXPORT_SYMBOL(cnss_wlan_enable);

int cnss_wlan_disable(struct device *dev, enum cnss_driver_mode mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return 0;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	return cnss_wlfw_wlan_mode_send_sync(plat_priv, CNSS_OFF);
}
EXPORT_SYMBOL(cnss_wlan_disable);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
#define OF_GPIO_ACTIVE_LOW 0x1
#endif

void cnss_set_led_gpio(int led_gpio, unsigned int value, unsigned int flags)
{
	struct gpio_desc *led_gpio_desc;

	led_gpio_desc = gpio_to_desc(led_gpio);

	/* IPQ9574 has active-low GPIO for WiFi LED.
	   Hence check and set the active-low status
	   appropriately from the GPIO flags.
	 */
	if ((flags & OF_GPIO_ACTIVE_LOW) &&
	    !gpiod_is_active_low(led_gpio_desc)) {
		gpiod_toggle_active_low(led_gpio_desc);
	}
	gpiod_set_value(led_gpio_desc, value);
}
EXPORT_SYMBOL(cnss_set_led_gpio);

int cnss_athdiag_read(struct device *dev, u32 offset, u32 mem_type,
		      u32 data_len, u8 *output)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("plat_priv is NULL!\n");
		return -EINVAL;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (!output || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Inv param athdiag read: output %p, data_len %u\n",
			    output, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state for athdiag read: 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_wlfw_athdiag_read_send_sync(plat_priv, offset, mem_type,
					       data_len, output);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_athdiag_read);

int cnss_athdiag_write(struct device *dev, u32 offset, u32 mem_type,
		       u32 data_len, u8 *input)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		pr_err("plat_priv is NULL!\n");
		return -EINVAL;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (!input || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Inv param athdiag write: input %p, data_len %u\n",
			    input, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state for athdiag write: 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_wlfw_athdiag_write_send_sync(plat_priv, offset, mem_type,
						data_len, input);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_athdiag_write);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
/*
 * Return true if target is a lithium target. else return false
 */
bool cnss_check_li_target(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return false;

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCN9000_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		return true;
	}

	return false;
}

/* Waikiki is supported for both li and be in the code*/
bool cnss_check_be_target(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return false;

	switch (plat_priv->device_id) {
	case QCN9224_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		return true;
	}

	return false;
}
#endif

/* Return 0 if device is a multi-pd target.
 * Else return -ENODEV.
 */
int cnss_check_multipd_target(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->device_id) {
	case QCA5018_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		return 0;
	default:
		break;
	}
	return -ENODEV;
}

/* Return 0 if device ID is valid.
 * Else, return -EINVAL.
 */
int cnss_check_device_id_valid(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		return 0;
	default:
		cnss_pr_err("Invalid device id 0x%lx\n", plat_priv->device_id);
		break;
	}
	return -EINVAL;
}

static int cnss_fw_mem_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	set_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

	ret = cnss_wlfw_tgt_cap_send_sync(plat_priv);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCN6122_DEVICE_ID ||
	    plat_priv->device_id == QCN9160_DEVICE_ID ||
	    plat_priv->device_id == QCN6432_DEVICE_ID) {
		ret = cnss_wlfw_device_info_send_sync(plat_priv);
		if (ret) {
			cnss_pr_err("Device info msg failed. ret %d\n", ret);
			goto out;
		}
	}

	if (plat_priv->hds_support) {
		ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv, CNSS_BDF_HDS);
		if (ret) {
			cnss_pr_err("hds load failed. ret %d\n", ret);
			goto out;
		}
	}

	if (plat_priv->regdb_support) {
		ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv,
						   CNSS_BDF_REGDB);
		if (ret) {
			cnss_pr_err("regdb load failed. ret %d\n", ret);
			goto out;
		}
	}

	if (plat_priv->rxgainlut_support) {
		ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv,
						   CNSS_BDF_RXGAINLUT);
		if (ret) {
			cnss_pr_err("rxgainlut load failed. ret %d\n", ret);
			goto out;
		}
	}

	ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv, CNSS_BDF_WIN);
	if (ret) {
		cnss_pr_err("bdf load failed. ret %d\n", ret);
		goto out;
	}

	if (plat_priv->caldata_support) {
		ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv, CNSS_CALDATA_WIN);
		if (ret) {
			cnss_pr_err("caldata load failed. ret %d\n", ret);
			goto out;
		}
	}

	if (plat_priv->device_id == QCN9000_DEVICE_ID ||
	    plat_priv->device_id == QCN9224_DEVICE_ID) {
		ret = cnss_bus_load_m3(plat_priv);
		if (ret) {
			cnss_pr_err("m3 load failed. ret %d\n", ret);
			goto out;
		}
	}

	ret = cnss_wlfw_m3_dnld_send_sync(plat_priv);
	if (ret) {
		cnss_pr_err("m3 dnld failed. ret %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

void cnss_get_ramdump_device_name(struct device *dev,
				  char *ramdump_dev_name,
				  size_t ramdump_dev_name_len)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	bool multi_pd_arch = false;
	const char *subsys_name;

	if (!plat_priv || !ramdump_dev_name)
		return;

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
		snprintf(ramdump_dev_name, ramdump_dev_name_len, "q6mem");
		break;
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
		snprintf(ramdump_dev_name, ramdump_dev_name_len, "ramdump_%s",
			 plat_priv->device_name);
		break;
	case QCA5018_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		multi_pd_arch = of_property_read_bool(dev->of_node,
						      "qcom,multipd_arch");
		if (multi_pd_arch) {
			of_property_read_string(dev->of_node,
						"qcom,userpd-subsys-name",
						&subsys_name);
			snprintf(ramdump_dev_name, ramdump_dev_name_len,
				 "%s_mem", subsys_name);
		} else {
			snprintf(ramdump_dev_name, ramdump_dev_name_len,
				 "q6mem");
		}
		break;
	default:
		cnss_pr_info("%s: Unknown device_id 0x%lx",
			     __func__, plat_priv->device_id);
	}
	cnss_pr_dbg("Ramdump device name %s for device 0x%lx\n",
		    ramdump_dev_name, plat_priv->device_id);
}
EXPORT_SYMBOL(cnss_get_ramdump_device_name);

bool cnss_get_global_mlo_support(void)
{
	struct cnss_plat_data *plat_priv = NULL;
	int i;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];
		switch (plat_priv->device_id) {
		case QCN9224_DEVICE_ID:
		case QCA5332_DEVICE_ID:
		case QCN6432_DEVICE_ID:
		case QCA5424_DEVICE_ID:
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL(cnss_get_global_mlo_support);

static void cnss_set_global_mlo_support(bool enable)
{
	struct cnss_plat_data *plat_priv = NULL;
	int i;

	cnss_pr_info("%s MLO support..\n", enable ? "Enabling" : "Disabling");
	enable_mlo_support = enable;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];
		switch (plat_priv->device_id) {
		case QCN9224_DEVICE_ID:
		case QCA5332_DEVICE_ID:
		case QCN6432_DEVICE_ID:
		case QCA5424_DEVICE_ID:
			plat_priv->mlo_support = enable;
			break;
		default:
			cnss_pr_dbg("MLO not supported for %s",
				    plat_priv->device_name);
		}
	}
}

static int cnss_set_adj_chip_ids(struct cnss_mlo_group_info *mlo_group_info)
{
	struct cnss_plat_data *plat_priv = NULL;
	struct cnss_mlo_chip_info *chip_info;
	u8 num_adj_chips = 0;
	int num_chips;
	int i;

	if (!mlo_group_info) {
		cnss_pr_err("%s: failed to get MLO group info\n", __func__);
		return -EINVAL;
	}
	num_chips = mlo_group_info->num_chips;
	cnss_pr_dbg("Number of MLO chips %d\n", num_chips);

	switch (num_chips) {
	case 0:
	case 1:
		num_adj_chips = 0;
		return 0;
	case 2:
		num_adj_chips = 1;
		break;
	default:
		num_adj_chips = 2;
		break;
	}

	for (i = 0; i < num_chips; i++) {
		chip_info = &mlo_group_info->chip_info[i];
		if (!chip_info) {
			cnss_pr_err("%s: failed to get MLO chip info\n",
				    __func__);
			continue;
		}

		chip_info->num_adj_chips = num_adj_chips;
		memset(chip_info->adj_chip_ids, 0,
				sizeof(uint8_t) * CNSS_MAX_LINKS_PER_CHIP);
		chip_info->adj_chip_ids[0] = (chip_info->chip_id + 1) %
								num_chips;
		if (num_adj_chips >= 2)
			chip_info->adj_chip_ids[1] = ((chip_info->chip_id - 1) +
							num_chips) % num_chips;
		cnss_pr_dbg("Adjacent chip IDs (%u, %u) for chip %u\n",
				chip_info->adj_chip_ids[0],
				chip_info->adj_chip_ids[1], chip_info->chip_id);
	}

	return 0;
}

static void cnss_set_adj_mlo_chips(struct cnss_mlo_group_info *mlo_group_info)
{
	struct cnss_plat_data *plat_priv = NULL;
	struct cnss_plat_data *adj_plat_priv = NULL;
	struct cnss_mlo_chip_info *chip_info;
	int mlo_chip_num = mlo_group_info->num_chips;
	int i = 0, k = 0, chip_idx = 0;

	for (i = 0; i < mlo_chip_num; i++) {
		chip_info = &mlo_group_info->chip_info[i];
		plat_priv = cnss_get_plat_priv_by_soc_id(chip_info->soc_id);

		if (!plat_priv)
			continue;

		for (k = 0; k < chip_info->num_adj_chips; k++) {
			chip_idx = chip_info->adj_chip_ids[k];
			adj_plat_priv = cnss_get_plat_priv_by_chip_id(chip_idx);
			if (adj_plat_priv)
				plat_priv->adj_mlo_chip_info[k] =
					adj_plat_priv->mlo_chip_info;
		}
	}
}

static int cnss_set_static_mlo_config(struct cnss_mlo_group_info *in_group_info,
			int num_groups)
{
	struct cnss_mlo_group_info *mlo_group_info;
	struct cnss_mlo_group_info *group_info;
	struct cnss_mlo_chip_info *chip_info;
	struct cnss_plat_data *plat_priv = NULL;
	int i, j, k;

	if (!enable_mlo_support) {
		cnss_pr_info("%s: MLO is disabled\n", __func__);
		return 0;
	}

	if (num_groups > CNSS_MAX_MLO_GROUPS) {
		cnss_pr_err("%s: num_groups %d greater than max %d",
			    __func__, num_groups, CNSS_MAX_MLO_GROUPS);
		return -EINVAL;
	}

	for (i = 0; i < num_groups; i++) {
		mlo_group_info = &g_mlo_group_info[i];
		group_info = &in_group_info[i];

		if (group_info->num_chips > CNSS_MAX_MLO_CHIPS) {
			cnss_pr_err("%s: num_chips %d greater than max %d",
				    __func__, group_info->num_chips,
				    CNSS_MAX_MLO_CHIPS);
			return -EINVAL;
		}

		memset(mlo_group_info, 0, sizeof(struct cnss_mlo_group_info));
		mlo_group_info->group_id = group_info->group_id;
		mlo_group_info->max_num_peers = group_info->max_num_peers;
		mlo_group_info->num_chips = group_info->num_chips;

		for (j = 0; j < group_info->num_chips; j++) {
			chip_info = &mlo_group_info->chip_info[j];

			chip_info->group_id =
				 group_info->chip_info[j].group_id;
			chip_info->soc_id =
				group_info->chip_info[j].soc_id;
			chip_info->chip_id =
				group_info->chip_info[j].chip_id;
			chip_info->num_local_links =
				group_info->chip_info[j].num_local_links;

			for (k = 0; k < CNSS_MAX_LINKS_PER_CHIP; k++) {
				chip_info->hw_link_ids[k] =
				    group_info->chip_info[j].hw_link_ids[k];
				chip_info->valid_link_ids[k] =
				    group_info->chip_info[j].valid_link_ids[k];
			}

			plat_priv =
				cnss_get_plat_priv_by_soc_id(chip_info->soc_id);
			if (!plat_priv || !plat_priv->mlo_support)
				continue;

			plat_priv->mlo_group_info = mlo_group_info;
			plat_priv->mlo_chip_info = chip_info;
			plat_priv->mlo_capable = 1;
			plat_priv->mlo_default_cfg = true;

			cnss_pr_info("%s: Default MLO Config updated for %s",
				     __func__, plat_priv->device_name);
		}
		cnss_set_adj_chip_ids(mlo_group_info);
		cnss_set_adj_mlo_chips(mlo_group_info);
	}

	return 0;
}

int cnss_reset_mlo_config(uint32_t group_id)
{
	struct cnss_plat_data *plat_priv = NULL;
	struct cnss_mlo_chip_info *chip_info;
	int i = 0;

	if (!enable_mlo_support)
		return 0;

	if (group_id >= CNSS_MAX_MLO_GROUPS) {
		cnss_pr_err("%s: Invalid MLO Group ID %d", __func__,
			    group_id);
		return -EINVAL;
	}

	if (g_mlo_group_info[group_id].num_chips > CNSS_MAX_MLO_CHIPS) {
		cnss_pr_err("%s: Invalid MLO num chips %d", __func__,
			    g_mlo_group_info[group_id].num_chips);
		return -EINVAL;
	}

	if (!g_mlo_group_info[group_id].num_chips)
		return 0;

	for (i = 0; i < g_mlo_group_info[group_id].num_chips; i++) {
		chip_info = &g_mlo_group_info[group_id].chip_info[i];
		plat_priv = cnss_get_plat_priv_by_soc_id(chip_info->soc_id);
		if (!plat_priv) {
			cnss_pr_err("%s: Failed to get plat_priv for soc_id: %d",
				    __func__, i);
			continue;
		}

		if (!plat_priv->mlo_support ||
		    ((plat_priv->bus_type == CNSS_BUS_PCI) &&
		     !plat_priv->pci_dev)) {
			continue;
		}

		plat_priv->mlo_capable = 0;
	}

	cnss_pr_dbg("%s: Reset MLO config for group %u", __func__, group_id);
	memset(&g_mlo_group_info[group_id], 0,
	       sizeof(struct cnss_mlo_group_info));

	return 0;
}
EXPORT_SYMBOL(cnss_reset_mlo_config);

struct cnss_plat_data *cnss_get_plat_priv_by_chip_id(int chip_id)
{
	struct cnss_plat_data *plat_priv = NULL;
	int i = 0;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = cnss_get_plat_priv_by_soc_id(i);
		if (!plat_priv) {
			cnss_pr_err("%s: Failed to get plat_priv for soc_id: %d",
				    __func__, i);
			continue;
		}

		if (!plat_priv->mlo_support ||
		    ((plat_priv->bus_type == CNSS_BUS_PCI) &&
		     !plat_priv->pci_dev))
			continue;

		if (!plat_priv->mlo_capable || !plat_env[i]->mlo_chip_info)
			continue;

		if (plat_env[i]->mlo_chip_info->chip_id == chip_id)
			return plat_env[i];
	}

	cnss_pr_err("plat_env is not found for chip %d", chip_id);
	return NULL;
}

int cnss_set_mlo_group_config(struct cnss_mlo_group_info *src_mlo_config,
			      uint8_t group_id)
{
	struct cnss_mlo_group_info *mlo_group_info;
	struct cnss_plat_data *plat_priv = NULL;
	struct cnss_mlo_group_info *mlo_config;
	struct cnss_mlo_chip_info *chip_info;
	bool hw_link_id_node = true;
	struct device *dev = NULL;
	int num_chip = 0;
	int link_id = 0;
	int i, j;

	if (!enable_mlo_support) {
		cnss_pr_info("%s: MLO is disabled\n", __func__);
		return 0;
	}

	if (group_id >= CNSS_MAX_MLO_GROUPS) {
		cnss_pr_err("%s: Invalid group ID %u", __func__, group_id);
		return -EINVAL;
	}

	mlo_group_info = &g_mlo_group_info[group_id];
	mlo_config = &src_mlo_config[group_id];

	if (mlo_config->num_chips > CNSS_MAX_MLO_CHIPS) {
		cnss_pr_err("%s: num_chips %d greater than max %d", __func__,
			    mlo_config->num_chips, CNSS_MAX_MLO_CHIPS);
		return -EINVAL;
	}

	memset(mlo_group_info, 0, sizeof(struct cnss_mlo_group_info));
	mlo_group_info->group_id = group_id;
	mlo_group_info->max_num_peers = mlo_config->max_num_peers;
	mlo_group_info->num_chips = mlo_config->num_chips;
	mlo_group_info->soc_chip_bitmap = mlo_config->soc_chip_bitmap;
	num_chip = 0;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = cnss_get_plat_priv_by_soc_id(i);
		if (!plat_priv) {
			cnss_pr_err("%s: Failed to get plat_priv for soc_id: %d",
				    __func__, i);
			return -EINVAL;
		}

		if (!plat_priv->mlo_support ||
		    ((plat_priv->bus_type == CNSS_BUS_PCI) &&
		     !plat_priv->pci_dev)) {
			continue;
		}

		if (!(mlo_config->soc_chip_bitmap & (1 << i)))
			continue;

		chip_info = &mlo_group_info->chip_info[num_chip];
		chip_info->group_id = group_id;
		chip_info->soc_id = i;
		chip_info->chip_id = mlo_config->chip_info[num_chip].chip_id;

		dev = &plat_priv->plat_dev->dev;
		memset(chip_info->hw_link_ids, 0,
				sizeof(chip_info->hw_link_ids));
		if (of_property_read_u32_array(dev->of_node,
					"hw_link_id", chip_info->hw_link_ids,
					ARRAY_SIZE(chip_info->hw_link_ids))) {
			hw_link_id_node = false;
		}
		if (plat_priv->firmware_type == CNSS_FW_DUAL_MAC) {
			chip_info->num_local_links = 2;
			for (j = 0; j < CNSS_MAX_LINKS_PER_CHIP; j++) {
				if (!hw_link_id_node)
					chip_info->hw_link_ids[j] = link_id++;
				chip_info->valid_link_ids[j] = 1;
			}
		} else {
			chip_info->num_local_links = 1;
			if (!hw_link_id_node)
				chip_info->hw_link_ids[0] = link_id++;
			chip_info->valid_link_ids[0] = 1;
			chip_info->valid_link_ids[1] = 0;
		}

		chip_info->num_adj_chips =
			mlo_config->chip_info[num_chip].num_adj_chips;
		for (j = 0; j < chip_info->num_adj_chips; j++) {
			chip_info->adj_chip_ids[j] =
			mlo_config->chip_info[num_chip].adj_chip_ids[j];
		}

		plat_priv->mlo_group_info = mlo_group_info;
		plat_priv->mlo_chip_info = chip_info;
		plat_priv->mlo_capable = 1;
		plat_priv->mlo_default_cfg = false;
		num_chip++;

		cnss_pr_info("%s: Dynamic MLO Config updated for %s",
			     __func__, plat_priv->device_name);
		if (mlo_group_info->num_chips == num_chip ||
				num_chip >= CNSS_MAX_MLO_CHIPS)
			break;
	}

	cnss_set_adj_mlo_chips(mlo_group_info);

	return 0;
}
EXPORT_SYMBOL(cnss_set_mlo_group_config);

int cnss_get_mlo_master_chip_id(struct cnss_mlo_group_info *mlo_group_info)
{
	int mlo_chip_id,mlo_master_chip_idx = 0;
	struct cnss_plat_data *plat_priv = NULL;

	for (mlo_chip_id = 0; mlo_chip_id < mlo_group_info->num_chips; mlo_chip_id++) {
		plat_priv = cnss_get_plat_priv_by_chip_id(mlo_group_info->chip_info[mlo_chip_id].chip_id);
		if ((plat_priv->mlo_support) && (!plat_priv->wsi_remap_state)) {
			mlo_master_chip_idx = mlo_chip_id;
			break;
		}
	}

	if (mlo_chip_id == mlo_group_info->num_chips) {
		mlo_master_chip_idx = 0;
	}

	return mlo_master_chip_idx;
}

int cnss_set_mlo_config(struct cnss_module_param *modparam,
			struct cnss_mlo_group_info *src_mlo_config)
{
	struct cnss_plat_data *plat_priv = NULL;
	int group_id;

	if (!enable_mlo_support) {
		cnss_pr_info("%s: MLO is disabled\n", __func__);
		return 0;
	}

	if (skip_radio_bmap || skip_cnss ||
	    (mlo_chip_bitmask != CNSS_DEFAULT_MLO_CHIP_BITMASK)) {
		cnss_pr_info("Skip radio is set, proceeding default MLO config.\n");
		cnss_set_default_mlo_config();
		return 0;
	}

	if (modparam->mlo_max_groups > CNSS_MAX_MLO_GROUPS) {
		cnss_pr_err("%s: num_groups %d greater than max %d",
			     __func__, modparam->mlo_max_groups,
			     CNSS_MAX_MLO_GROUPS);
		return -EINVAL;
	}

	for (group_id = 0; group_id < modparam->mlo_max_groups; group_id++)
		if (cnss_set_mlo_group_config(src_mlo_config, group_id))
			return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(cnss_set_mlo_config);

static void cnss_print_chip_info(struct cnss_mlo_chip_info *chip_info)
{
	int i;

	pr_err("\nsoc_id: %u\n\t\tchip_id: %u\n\t\tgroup_id: %u\n\t\tnum_local_links: %u\n\t\tnum_adj_chips: %u\n",
	       chip_info->soc_id,
	       chip_info->chip_id,
	       chip_info->group_id,
	       chip_info->num_local_links,
	       chip_info->num_adj_chips);

	for (i = 0; i < CNSS_MAX_LINKS_PER_CHIP; i++)
		pr_err("\t\thw_link_ids[%d]: %u\n",
		       i, chip_info->hw_link_ids[i]);

	for (i = 0; i < CNSS_MAX_LINKS_PER_CHIP; i++)
		pr_err("\t\tvalid_link_ids[%d]: %u\n",
		       i, chip_info->valid_link_ids[i]);

	for (i = 0; i < CNSS_MAX_LINKS_PER_CHIP; i++)
		pr_err("\t\tadj_chip_ids[%d]: %u\n",
		       i, chip_info->adj_chip_ids[i]);
}

void cnss_print_mlo_config(void)
{
	struct cnss_mlo_group_info *mlo_group_info;
	struct cnss_mlo_chip_info *chip_info;
	int i, j;

	if (!enable_mlo_support) {
		pr_err("MLO is disabled!\n");
		return;
	}

	pr_err("\n****** CNSS MLO CONFIG ******\n");
	for (i = 0; i < CNSS_MAX_MLO_GROUPS; i++) {
		mlo_group_info = &g_mlo_group_info[i];

		pr_err("\ngroup_id: %u\nmax_num_peers: %u\nnum_chips: %u\nsoc_chip_bitmap: 0x%x\n",
		       mlo_group_info->group_id,
		       mlo_group_info->max_num_peers,
		       mlo_group_info->num_chips,
		       mlo_group_info->soc_chip_bitmap);

		for (j = 0; j < mlo_group_info->num_chips; j++) {
			chip_info = &mlo_group_info->chip_info[j];
			cnss_print_chip_info(chip_info);
		}
	}
}
EXPORT_SYMBOL(cnss_print_mlo_config);

int cnss_get_mlo_chip_id(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv || !plat_priv->mlo_support)
		return -EINVAL;

	if (!plat_priv->mlo_capable || !plat_priv->mlo_chip_info)
		return -EINVAL;

	return plat_priv->mlo_chip_info->chip_id;
}
EXPORT_SYMBOL(cnss_get_mlo_chip_id);

bool cnss_get_mlo_capable(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv || !plat_priv->mlo_support)
		return false;

	return plat_priv->mlo_capable;
}
EXPORT_SYMBOL(cnss_get_mlo_capable);

bool cnss_is_mlo_default_cfg_enabled(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv || !plat_priv->mlo_support)
		return false;

	return plat_priv->mlo_default_cfg;
}
EXPORT_SYMBOL(cnss_is_mlo_default_cfg_enabled);

int cnss_get_mlo_global_config_region_info(struct device *dev,
					   void **bar,
					   int *num_bytes)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_fw_mem *fw_mem;
	int i;

	if (!plat_priv || !plat_priv->mlo_support || !plat_priv->mlo_capable)
		return -EINVAL;

	*bar = 0;
	fw_mem = plat_priv->fw_mem;
	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].va) {
			if (fw_mem[i].type == QMI_WLFW_MLO_GLOBAL_MEM_V01) {
				*bar = plat_priv->fw_mem[i].va;
				*num_bytes = plat_priv->fw_mem[i].size;
			}
		}
	}

	if (!*bar) {
		cnss_pr_err("%s: no mlo mem found", __func__);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(cnss_get_mlo_global_config_region_info);

int cnss_get_num_mlo_links(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv || !plat_priv->mlo_support)
		return -EINVAL;

	if (!plat_priv->mlo_capable || !plat_priv->mlo_chip_info)
		return -EINVAL;

	return plat_priv->mlo_chip_info->num_local_links;
}
EXPORT_SYMBOL(cnss_get_num_mlo_links);

int cnss_get_mlo_chip_info(struct device *dev,
			   struct cnss_mlo_chip_info **chip_info)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv || !plat_priv->mlo_support)
		return -EINVAL;

	if (!plat_priv->mlo_capable || !plat_priv->mlo_chip_info)
		return -EINVAL;

	*chip_info = plat_priv->mlo_chip_info;
	return 0;
}
EXPORT_SYMBOL(cnss_get_mlo_chip_info);

int cnss_get_num_mlo_capable_devices(unsigned int *device_id, int num_elements)
{
	struct cnss_plat_data *plat_priv = NULL;
	int num_capable = 0;
	int i;
	int device_count = 0;

	if (!enable_mlo_support)
		return -EINVAL;

	if (!device_id)
		return -EINVAL;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];

		if (plat_priv && plat_priv->mlo_capable) {
			num_capable++;
			if (device_count < num_elements)
				device_id[device_count++] =
					plat_priv->device_id;
		}
	}

	return num_capable;
}
EXPORT_SYMBOL(cnss_get_num_mlo_capable_devices);

int cnss_get_max_mlo_chips(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int group_id, max_mlo_chips = 0;
	struct device_node *mlo;

	if (!enable_mlo_support)
		return -EINVAL;

	mlo = of_parse_phandle(dev->of_node, "qcom,wsi", 0);
	if (!mlo) {
		cnss_pr_err("%s: WSI node is not present\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32(mlo, "id", &group_id)) {
		cnss_pr_err("Group ID property is not present\n");
		return -EINVAL;
	}

	if (of_property_read_u32(mlo, "num_chip", &max_mlo_chips)) {
		cnss_pr_err("Max num chip property is not present\n");
		return -EINVAL;
	}

	return max_mlo_chips;
}
EXPORT_SYMBOL(cnss_get_max_mlo_chips);

int cnss_get_num_mlo_groups(void)
{
	struct cnss_plat_data *plat_priv = NULL;
	int num_mlo_grp = 0;
	int i;
	int group_count = 0;

	if (!enable_mlo_support)
		return 0;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];

		if (!plat_priv) {
			cnss_pr_err("%s: Failed to get plat_priv for soc_id %d",
				    __func__, i);
			continue;
		}

		if (!plat_priv->mlo_capable ||
		    ((plat_priv->bus_type == CNSS_BUS_PCI) &&
		     !plat_priv->pci_dev)) {
			continue;
		}

		group_count = plat_priv->mlo_group_info->group_id;
		if (group_count > num_mlo_grp)
			num_mlo_grp = group_count;
	}

	return ++num_mlo_grp;
}
EXPORT_SYMBOL(cnss_get_num_mlo_groups);

int cnss_get_mlo_group_id(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv || !plat_priv->mlo_support)
		return -EINVAL;

	if (!plat_priv->mlo_capable || !plat_priv->mlo_chip_info)
		return -EINVAL;

	return plat_priv->mlo_chip_info->group_id;
}
EXPORT_SYMBOL(cnss_get_mlo_group_id);

bool cnss_get_mlo_group_info(uint8_t grp_id,
			struct cnss_mlo_group_info *grp_info)
{
	if (grp_id < 0 || grp_id >= CNSS_MAX_MLO_GROUPS)
		return false;
	memcpy(grp_info, &g_mlo_group_info[grp_id],
		sizeof(struct cnss_mlo_group_info));

	return true;
}
EXPORT_SYMBOL(cnss_get_mlo_group_info);

int cnss_get_dev_link_ids(struct device *dev, u8 *link_ids, int max_elements)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int i;

	if (!link_ids) {
		cnss_pr_err("link_ids buffer is null\n");
		return -ENOMEM;
	}

	if (max_elements < CNSS_MAX_LINKS_PER_CHIP) {
		cnss_pr_err("link ids size is less %d\n", max_elements);
		return -EINVAL;
	}

	if (!plat_priv || !plat_priv->mlo_support)
		return -EINVAL;

	if (!plat_priv->mlo_capable || !plat_priv->mlo_chip_info)
		return -EINVAL;

	memset(link_ids, 0, max_elements);

	for (i = 0; i < max_elements; i++)
		link_ids[i] = plat_priv->mlo_chip_info->hw_link_ids[i];

	return i;
}
EXPORT_SYMBOL(cnss_get_dev_link_ids);

static int cnss_get_group_id(struct cnss_plat_data *plat_priv)
{
	struct device *dev = &plat_priv->plat_dev->dev;
	int group_id = 0;

	if (of_property_read_u32(dev->of_node, "group_id", &group_id)) {
		cnss_pr_dbg("%s: Group ID not specified in the DTS. Setting the default group ID 0\n",
			    __func__);
		group_id = 0;
	}

	return group_id;
}

/* Temporary API to set default MLO config, will be removed once driver starts
 * setting MLO config via PLD.
 */
void cnss_set_default_mlo_config(void)
{
	struct cnss_mlo_group_info mlo_group_info[CNSS_MAX_MLO_GROUPS];
	struct cnss_mlo_chip_info *ch_info = NULL;
	struct cnss_plat_data *plat_priv = NULL;
	int num_chip = 0, i = 0, link_id = 0, group_id = 0;
	int grp_chip_id[CNSS_MAX_MLO_GROUPS] = {0};
	int grp_link_id[CNSS_MAX_MLO_GROUPS] = {0};
	bool hw_link_id_node = true;
	struct device *dev = NULL;
	int k = 0;

	if (!enable_mlo_support)
		return;

	memset(&mlo_group_info, 0, sizeof(struct cnss_mlo_group_info));

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = cnss_get_plat_priv_by_soc_id(i);
		if (!plat_priv) {
			cnss_pr_err("%s: Failed to get plat_priv for soc_id: %d",
				    __func__, i);
			return;
		}

		if (!plat_priv->mlo_support ||
		    ((plat_priv->bus_type == CNSS_BUS_PCI) &&
		     !plat_priv->pci_dev))
			continue;

		if (mlo_chip_bitmask == 0xFF) {
			dev = &plat_priv->plat_dev->dev;
			if (of_property_read_bool(dev->of_node, "mlo_skip")) {
				cnss_pr_info("%s: Device %s skipped from mlo config.\n",
					      __func__,
					      plat_priv->device_name);
				plat_priv->mlo_capable = 0;
				mlo_chip_bitmask =
						mlo_chip_bitmask & ~(1 << i);
				continue;
			}
		}

		group_id = cnss_get_group_id(plat_priv);
		if (group_id < 0 && group_id >= CNSS_MAX_MLO_GROUPS) {
			cnss_pr_err("%s: Invalid group id: %d", __func__,
				    group_id);
			return;
		}

		mlo_group_info[group_id].group_id = group_id;
		if (mlo_max_peer == 0)
			mlo_group_info[group_id].max_num_peers = 256;
		else
			mlo_group_info[group_id].max_num_peers = mlo_max_peer;

		if (mlo_chip_bitmask & (1 << i)) {
			/*Temporarily Hard coding group id as 0 */
			num_chip = grp_chip_id[group_id];
			link_id = grp_link_id[group_id];
			ch_info = &mlo_group_info[group_id].chip_info[num_chip];
			ch_info->group_id = group_id;
			ch_info->soc_id = i;
			ch_info->chip_id = num_chip;

			dev = &plat_priv->plat_dev->dev;
			memset(ch_info->hw_link_ids, 0,
					sizeof(ch_info->hw_link_ids));
			if (of_property_read_u32_array(dev->of_node,
			    "hw_link_id", ch_info->hw_link_ids,
			    ARRAY_SIZE(ch_info->hw_link_ids))) {
				hw_link_id_node = false;
			}
			if (plat_priv->firmware_type == CNSS_FW_DUAL_MAC) {
				ch_info->num_local_links = 2;
				for (k = 0; k < CNSS_MAX_LINKS_PER_CHIP; k++) {
					if (!hw_link_id_node)
						ch_info->hw_link_ids[k] =
							link_id + k;
					ch_info->valid_link_ids[k] = 1;
				}
				grp_link_id[group_id] = grp_link_id[group_id] +
							CNSS_MAX_LINKS_PER_CHIP;
			} else {
				ch_info->num_local_links = 1;
				if (!hw_link_id_node)
					ch_info->hw_link_ids[0] = link_id;
				ch_info->valid_link_ids[0] = 1;
				ch_info->valid_link_ids[1] = 0;
				grp_link_id[group_id] = grp_link_id[group_id] +
									1;
			}
			grp_chip_id[group_id] = grp_chip_id[group_id] + 1;
		}
		mlo_group_info[group_id].num_chips = grp_chip_id[group_id];
	}

	cnss_set_static_mlo_config(&mlo_group_info[0], group_id + 1);
	cnss_pr_info("Default MLO configuration is set!");
}
EXPORT_SYMBOL(cnss_set_default_mlo_config);

static int cnss_send_mlo_wsi_remap(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL!\n", __func__);
		return -ENODEV;
	}

	if (!test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state to send QMI message: 0x%lx\n",
			    plat_priv->driver_state);
		return -EINVAL;
	}

	ret = cnss_wlfw_mlo_wsi_remap_send_sync(plat_priv);
	if (ret) {
		cnss_pr_err("Dynamic WSI QMI message failed: 0x%lx, ret = %d\n",
			    plat_priv->driver_state, ret);
		CNSS_ASSERT(0);
		return ret;
	}
	cnss_pr_dbg("Dynamic WSI remap applied for %s\n",
		    plat_priv->device_name);

	return 0;
}

int cnss_set_wsi_remap(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_mlo_chip_info *chip_info;
	struct cnss_mlo_group_info *group_info;
	int chip_idx;
	int ret = 0;

	if (!enable_mlo_support) {
		cnss_pr_info("%s: MLO is disabled\n", __func__);
		return -EINVAL;
	}

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL!\n", __func__);
		return -ENODEV;
	}

	if (cnss_is_mlo_default_cfg_enabled(dev))
		cnss_pr_info("%s: Booted with default MLO config!\n", __func__);

	group_info = plat_priv->mlo_group_info;
	for (chip_idx = 0; chip_idx < group_info->num_chips; chip_idx++) {
		chip_info = &group_info->chip_info[chip_idx];
		plat_priv = cnss_get_plat_priv_by_soc_id(chip_info->soc_id);
		if (!plat_priv) {
			cnss_pr_err("%s: Failed to get plat_priv for soc_id: %d\n",
				    __func__, chip_idx);
			return -ENODEV;
		}

		if (!plat_priv->mlo_support || !plat_priv->mlo_capable ||
		    ((plat_priv->bus_type == CNSS_BUS_PCI) &&
		    !plat_priv->pci_dev)) {
			cnss_pr_info("%s: MLO is disabled\n", __func__);
			continue;
		}

		ret = cnss_send_mlo_wsi_remap(plat_priv);
		if (ret) {
			cnss_pr_err("%s: MLO WSI remap failed, ret = %d\n",
				    __func__, ret);
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL(cnss_set_wsi_remap);

void __cnss_wait_for_fw_ready(struct cnss_plat_data *plat_priv)
{
	int count = 0;

	if (!cnss_check_device_id_valid(plat_priv)) {
		/* Device ID is valid */
		cnss_pr_info("Waiting for FW ready. Device: 0x%lx, FW ready timeout: %d seconds\n",
			     plat_priv->device_id, fw_ready_timeout);
		while (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
			msleep(FW_READY_DELAY);
			if (count++ > fw_ready_timeout * 10) {
				cnss_pr_err("FW ready timed-out %d seconds\n",
					    fw_ready_timeout);
				CNSS_ASSERT(0);
			}
		}
		cnss_pr_info("FW ready received for device 0x%lx\n",
			     plat_priv->device_id);
	}
}

void cnss_wait_for_fw_ready(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	__cnss_wait_for_fw_ready(plat_priv);
}
EXPORT_SYMBOL(cnss_wait_for_fw_ready);

void cnss_wait_for_cold_boot_cal_done(struct cnss_plat_data *plat_priv)
{
	int count = 0;

	if (!plat_priv)
		return;

	if (!cnss_check_device_id_valid(plat_priv)) {
		/* Device ID is valid.
		 * Cold boot Calibration is done parallely for multiple devices
		 * Check if this device has already completed cold boot cal
		 * If already completed, we need not wait
		 */
		if (!test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state)) {
			cnss_pr_dbg("%s: Device already completed cold boot cal!\n",
				    __func__);
			return;
		}

		cnss_pr_info("Coldboot Calbration wait started for Device: 0x%lx, timeout: %d seconds\n",
			     plat_priv->device_id, cold_boot_cal_timeout);
		while (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state)) {
			msleep(FW_READY_DELAY);
			if (count++ > cold_boot_cal_timeout * 10) {
				cnss_pr_err("Coldboot calibration timed out %d seconds\n",
					    cold_boot_cal_timeout);
				/* Collect the FW dump when there is no target
				 * assert instead coldboot timeout happens and
				 * host asserted.
				 */
				if (ramdump_enabled) {
					cnss_bus_collect_dump_info(plat_priv,
								   true);
					cnss_bus_dev_ramdump(plat_priv);
				}
				CNSS_ASSERT(0);
			}
		}
		cnss_pr_info("Coldboot Calibration wait ended for device 0x%lx\n",
			     plat_priv->device_id);
	}
}

void cnss_set_ramdump_enabled(struct device *dev, bool enabled)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("%s: Failed to get plat_priv", __func__);
		return;
	}

	/* This is temporarily same as cnss_set_recovery_enabled until the
	 * wifi driver switches to use cnss_set_recovery_enabled.
	 */
	plat_priv->recovery_enabled = enabled;
	cnss_pr_dbg("Setting recovery_enabled to %d for %s\n",
		    plat_priv->recovery_enabled,
		    plat_priv->device_name);
}
EXPORT_SYMBOL(cnss_set_ramdump_enabled);

void cnss_set_recovery_enabled(struct device *dev, bool enabled)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("%s: Failed to get plat_priv", __func__);
		return;
	}

	plat_priv->recovery_enabled = enabled;
	cnss_pr_dbg("Setting recovery_enabled to %d for %s\n",
		    plat_priv->recovery_enabled,
		    plat_priv->device_name);
}
EXPORT_SYMBOL(cnss_set_recovery_enabled);

static int cnss_fw_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("%s:%d FW ready received for %s\n", __func__, __LINE__,
		    plat_priv->device_name);

	del_timer(&plat_priv->fw_boot_timer);
	set_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);

	if (test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	}

	return 0;
}

static char *cnss_driver_event_to_str(enum cnss_driver_event_type type)
{
	switch (type) {
	case CNSS_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case CNSS_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case CNSS_DRIVER_EVENT_REQUEST_MEM:
		return "REQUEST_MEM";
	case CNSS_DRIVER_EVENT_FW_MEM_READY:
		return "FW_MEM_READY";
	case CNSS_DRIVER_EVENT_FW_READY:
		return "FW_READY";
	case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START:
		return "COLD_BOOT_CAL_START";
	case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE:
		return "COLD_BOOT_CAL_DONE";
	case CNSS_DRIVER_EVENT_REGISTER_DRIVER:
		return "REGISTER_DRIVER";
	case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
		return "UNREGISTER_DRIVER";
	case CNSS_DRIVER_EVENT_RECOVERY:
		return "RECOVERY";
	case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
		return "FORCE_FW_ASSERT";
	case CNSS_DRIVER_EVENT_POWER_UP:
		return "POWER_UP";
	case CNSS_DRIVER_EVENT_POWER_DOWN:
		return "POWER_DOWN";
	case CNSS_DRIVER_EVENT_IDLE_RESTART:
		return "IDLE_RESTART";
	case CNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
		return "IDLE_SHUTDOWN";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
		return "QDSS_TRACE_REQ_MEM";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
		return "QDSS_TRACE_SAVE";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
		return "QDSS_TRACE_FREE";
	case CNSS_DRIVER_EVENT_QDSS_MEM_READY:
		return "QDSS_MEM_READY";
	case CNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ:
		return "M3_DUMP_UPLOAD_REQ";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA:
		return "QDSS_TRACE_REQ_DATA";
	case CNSS_DRIVER_EVENT_RAMDUMP_DONE:
		return "RAMDUMP_DONE";
	case CNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

int cnss_driver_event_post(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_event_type type,
			   u32 flags, void *data)
{
	struct cnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Posting event[%p]: %s(%d)%s, state: 0x%lx flags: 0x%0x\n",
		    plat_priv,
		    cnss_driver_event_to_str(type), type,
		    flags ? "-sync" : "", plat_priv->driver_state, flags);

	if (type >= CNSS_DRIVER_EVENT_MAX) {
		cnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (!event)
		return -ENOMEM;

#ifdef CONFIG_CNSS2_PM
	cnss_pm_stay_awake(plat_priv);
#endif

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = CNSS_EVENT_PENDING;
	event->sync = !!(flags & CNSS_EVENT_SYNC);

	spin_lock_irqsave(&plat_priv->event_lock, irq_flags);
	list_add_tail(&event->list, &plat_priv->event_list);
	spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);

	queue_work(plat_priv->event_wq, &plat_priv->event_work);

	if (!(flags & CNSS_EVENT_SYNC))
		goto out;

	printk(KERN_INFO "Waiting for Event(%s) to complete\n",
		cnss_driver_event_to_str(type));

	if (flags & CNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	cnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		    cnss_driver_event_to_str(type), type,
		    plat_priv->driver_state, ret, event->ret);
	spin_lock_irqsave(&plat_priv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == CNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
#ifdef CONFIG_CNSS2_PM
	cnss_pm_relax(plat_priv);
#endif
	return ret;
}

unsigned int cnss_get_driver_mode(void)
{
	return driver_mode;
}
EXPORT_SYMBOL(cnss_get_driver_mode);

int cnss_set_driver_mode(unsigned int mode)
{
	switch (mode) {
	/* Fall through for all valid modes */
	case CNSS_MISSION:
	case CNSS_FTM:
	case CNSS_EPPING:
	case CNSS_WALTEST:
	case CNSS_OFF:
	case CNSS_CCPM:
	case CNSS_QVIT:
	case CNSS_CALIBRATION:
	case CNSS_FTM_CALIBRATION:
		driver_mode = mode;
		break;
	default:
		pr_err("%s: Invalid driver mode %d", __func__, mode);
		return -EINVAL;
	}

	/* MLO support needs to be enabled only for Mission mode */
	if (mode != CNSS_MISSION)
		cnss_set_global_mlo_support(false);

	return 0;
}
EXPORT_SYMBOL(cnss_set_driver_mode);

unsigned int cnss_get_boot_timeout(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return 0;
	}

	return cnss_get_qmi_timeout(plat_priv);
}
EXPORT_SYMBOL(cnss_get_boot_timeout);

int cnss_power_up(struct device *dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;

	if (!plat_priv) {
		printk(KERN_ERR "plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Powering up device\n");

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_POWER_UP,
				     CNSS_EVENT_SYNC, NULL);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto out;

	timeout = cnss_get_boot_timeout(dev);

	reinit_completion(&plat_priv->power_up_complete);
	ret = wait_for_completion_timeout(&plat_priv->power_up_complete,
					  msecs_to_jiffies(timeout) << 2);
	if (!ret) {
		cnss_pr_err("Timeout waiting for power up to complete\n");
		ret = -EAGAIN;
		goto out;
	}

	return 0;

out:
	return ret;
}
EXPORT_SYMBOL(cnss_power_up);

int cnss_power_down(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		printk(KERN_ERR "plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Powering down device\n");

	return cnss_driver_event_post(plat_priv,
				      CNSS_DRIVER_EVENT_POWER_DOWN,
				      CNSS_EVENT_SYNC, NULL);
}
EXPORT_SYMBOL(cnss_power_down);

int cnss_idle_restart(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Doing idle restart\n");

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_IDLE_RESTART,
				     CNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		ret = cnss_bus_driver_probe(plat_priv);
		goto out;
	}

	timeout = cnss_get_boot_timeout(dev);

	reinit_completion(&plat_priv->power_up_complete);
	ret = wait_for_completion_timeout(&plat_priv->power_up_complete,
					  msecs_to_jiffies(timeout) << 2);
	if (!ret) {
		cnss_pr_err("Timeout waiting for idle restart to complete\n");
		ret = -EAGAIN;
		goto out;
	}

	return 0;

out:
	return ret;
}
EXPORT_SYMBOL(cnss_idle_restart);

int cnss_idle_shutdown(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (test_bit(CNSS_IN_SUSPEND_RESUME, &plat_priv->driver_state)) {
		cnss_pr_dbg("System suspend or resume in progress, ignore idle shutdown\n");
		return -EAGAIN;
	}

	cnss_pr_dbg("Doing idle shutdown\n");

	if (!test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    !test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto skip_wait;

	reinit_completion(&plat_priv->recovery_complete);
	ret = wait_for_completion_timeout(&plat_priv->recovery_complete,
					  RECOVERY_TIMEOUT);
	if (!ret) {
		cnss_pr_err("Timeout waiting for recovery to complete\n");
		CNSS_ASSERT(0);
	}

skip_wait:
	return cnss_driver_event_post(plat_priv,
				      CNSS_DRIVER_EVENT_IDLE_SHUTDOWN,
				      CNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(cnss_idle_shutdown);

static int cnss_get_resources(struct cnss_plat_data *plat_priv)
{
#ifdef CNSS2_NOCLOCK
	int ret;

	ret = cnss_get_vreg_type(plat_priv, CNSS_VREG_PRIM);
	if (ret) {
		cnss_pr_err("Failed to get vreg, err = %d\n", ret);
		goto out;
	}

	ret = cnss_get_clk(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get clocks, err = %d\n", ret);
		goto put_vreg;
	}

	ret = cnss_get_pinctrl(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get pinctrl, err = %d\n", ret);
		goto put_clk;
	}

	return 0;

put_clk:
	cnss_put_clk(plat_priv);
put_vreg:
	cnss_put_vreg_type(plat_priv, CNSS_VREG_PRIM);
out:
	return ret;
#endif
	return 0;
}

static void cnss_put_resources(struct cnss_plat_data *plat_priv)
{
#ifdef CNSS2_NOCLOCK
	cnss_put_clk(plat_priv);
	cnss_put_vreg_type(plat_priv, CNSS_VREG_PRIM);
#endif
}

static int cnss_set_ssr_recovery_type(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCN9000_DEVICE_ID:
		plat_priv->recovery_type = CNSS_ASYNC_RECOVERY;
		break;
	default:
		plat_priv->recovery_type = CNSS_SYNC_RECOVERY;
	}

	return 0;
}

#ifdef CONFIG_CNSS2_KERNEL_IPQ
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
static int cnss_qcn9000_notifier_atomic_nb(struct notifier_block *nb,
					   unsigned long code,
					   void *ss_handle)
{
	/* Fatal Notification to driver already sent as soon as
	 * MHI FATAL_ERR or SYS_ERR is received in cnss_mhi_notify_status
	 */
	return NOTIFY_OK;
}
#endif

static int cnss_qca8074_notifier_atomic_nb(struct notifier_block *nb,
	unsigned long code,
	void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, modem_atomic_nb);
	struct rproc *rproc;
	struct cnss_wlan_driver *driver_ops;
	int event_code = cnss_get_event(code);
	enum cnss_recovery_reason cnss_reason;
	driver_ops = plat_priv->driver_ops;

	if (event_code < 0)
		return NOTIFY_OK;

	if (event_code == CNSS_PREPARE_FOR_FATAL_SHUTDOWN) {
		cnss_pr_err("XXX TARGET ASSERTED XXX\n");
		cnss_pr_err("XXX TARGET %s instance_id 0x%x plat_env idx %d XXX\n",
			    plat_priv->device_name,
			    plat_priv->wlfw_service_instance_id,
			    cnss_get_plat_env_index_from_plat_priv(plat_priv));
		plat_priv->target_asserted = 1;
		plat_priv->target_assert_timestamp = ktime_to_ms(ktime_get());
		if (plat_priv->recovery_type == CNSS_SYNC_RECOVERY) {
			rproc = plat_priv->rproc_handle;
			if (rproc) {
				if (rproc->state != RPROC_OFFLINE) {
					plat_priv->crash_type = CNSS_USERPD_CRASH;
					rproc->state = RPROC_CRASHED;
					cnss_reason = CNSS_REASON_FATAL_SHUTDOWN;
					cnss_schedule_recovery(&plat_priv->plat_dev->dev,
								cnss_reason);
				}
			}
		} else {
			driver_ops->fatal((struct pci_dev *)plat_priv->plat_dev,
					  (const struct pci_device_id *)
					  plat_priv->plat_dev_id);
		}
	}

	return NOTIFY_OK;
}
#endif

void *cnss_register_notifier_cb(struct cnss_plat_data *plat_priv)
{
	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_register_qcn9000_cb(plat_priv);
	case CNSS_BUS_AHB:
		return cnss_register_qca8074_cb(plat_priv);
	default:
		cnss_pr_err("Invalid bus type for %s", plat_priv->device_name);
	}
	return NULL;
}

int cnss_unregister_notifier_cb(struct cnss_plat_data *plat_priv)
{
	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_unregister_qcn9000_cb(plat_priv);
	case CNSS_BUS_AHB:
		return cnss_unregister_qca8074_cb(plat_priv);
	default:
		cnss_pr_err("Invalid bus type for %s", plat_priv->device_name);
	}
	return 0;
}

int cnss_wlan_probe_driver(void)
{
	int ret;
	int i;
	int count = 0;
	struct cnss_plat_data *plat_priv = NULL;
	enum cnss_driver_mode cal_mode;

	cnss_sort_probe_order();
	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];

		if (!plat_priv)
			continue;

		plat_priv->target_asserted = 0;
		plat_priv->target_assert_timestamp = 0;
		plat_priv->driver_status = CNSS_LOAD_UNLOAD;

#if defined CNSS_PCI_SUPPORT
		if (plat_priv->bus_type == CNSS_BUS_PCI) {
			/* If plat_priv->pci_dev is NULL, the PCI device is not
			 * enumerated, set driver status and skip that device
			 * so that other devices can continue to boot.
			 */
			if (!plat_priv->pci_dev) {
				plat_priv->driver_status = CNSS_INITIALIZED;
				continue;
			}
			if (plat_priv->ops->cnss_bus_init)
				plat_priv->ops->cnss_bus_init(plat_priv);
			set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		}
#endif
		if (plat_priv->cold_boot_support && !plat_priv->cal_done)
			plat_priv->cal_in_progress = true;

		ret = cnss_register_subsys(plat_priv);
		if (ret)
			goto reset_ctx;


		if (plat_priv->cal_in_progress) {
			if (driver_mode == CNSS_FTM)
				cal_mode = CNSS_FTM_CALIBRATION;
			else
				cal_mode = CNSS_CALIBRATION;

			__cnss_wait_for_fw_ready(plat_priv);
			__cnss_wlan_enable(plat_priv, NULL, cal_mode, "WIN");

			schedule_work(&plat_priv->cal_work);
			atomic_inc(&cal_in_progress_count);
		} else {
			plat_priv->driver_status = CNSS_INITIALIZED;
		}
	}


	while (atomic_read(&cal_in_progress_count)) {
		msleep(FW_READY_DELAY);
		if (count++ > probe_timeout * 10) {
			cnss_pr_err("CNSS Driver probe timed out\n");
			CNSS_ASSERT(0);
		}
	}
	return 0;

reset_ctx:
	cnss_pr_err("Failed to get subsystem, err = %d\n", ret);
	plat_priv->driver_status = CNSS_UNINITIALIZED;
	plat_priv->driver_ops = NULL;
	return ret;
}
EXPORT_SYMBOL(cnss_wlan_probe_driver);

#ifdef CONFIG_CNSS2_LEGACY_IRQ
static int cnss_assign_lvirq(struct cnss_plat_data *plat_priv)
{
	plat_priv->lvirq = cnss_get_lvirq_by_qrtr_id(plat_priv->qrtr_node_id);
	if (!plat_priv->lvirq) {
		cnss_pr_err("lvirq pointer is NULL");
		CNSS_ASSERT(0);
		return -EINVAL;
	}
	return 0;
}
#endif

int cnss_wlan_register_driver_ops(struct cnss_wlan_driver *driver_ops)
{
	int i;
	struct cnss_plat_data *plat_priv = NULL;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];

		if (!plat_priv)
			continue;

		switch (plat_priv->bus_type) {
		case CNSS_BUS_AHB:
			if (strcmp(driver_ops->name, "pld_ahb") == 0)
				plat_priv->driver_ops = driver_ops;
			break;
		case CNSS_BUS_PCI:
			if (strcmp(driver_ops->name, "pld_pcie") == 0) {
				plat_priv->driver_ops = driver_ops;
#ifdef CONFIG_CNSS2_LEGACY_IRQ
				if (plat_priv->enable_intx) {
					if (cnss_assign_lvirq(plat_priv))
						return -EINVAL;
				}
#endif
			}
			break;
		default:
			cnss_pr_err("%s: Invalid bus type for device 0x%lx\n",
				    __func__, plat_priv->device_id);
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(cnss_wlan_register_driver_ops);

bool cnss_is_dev_initialized(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return NULL;

	return (plat_priv->driver_status == CNSS_INITIALIZED) ? 1 : 0;
}
EXPORT_SYMBOL(cnss_is_dev_initialized);

void *cnss_get_pci_dev_from_plat_dev(void *pdev)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = NULL;

	if (!plat_dev)
		return NULL;

	plat_priv = platform_get_drvdata(plat_dev);

	if (!plat_priv)
		return NULL;

	return plat_priv->pci_dev;
}
EXPORT_SYMBOL(cnss_get_pci_dev_from_plat_dev);

void *cnss_get_pci_dev_id_from_plat_dev(void *pdev)
{
	struct platform_device *plat_dev = (struct platform_device *)pdev;
	struct cnss_plat_data *plat_priv = NULL;

	if (!plat_dev)
		return NULL;

	plat_priv = platform_get_drvdata(plat_dev);

	if (!plat_priv)
		return NULL;

	return plat_priv->pci_dev_id;
}
EXPORT_SYMBOL(cnss_get_pci_dev_id_from_plat_dev);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver_ops)
{
	struct cnss_plat_data *plat_priv = NULL;
	struct cnss_subsys_info *subsys_info;
	struct cnss_wlan_driver *ops;
	int i;

	for (i = 0; i < plat_env_index; i++) {
		plat_priv = plat_env[i];

		if (!plat_priv) {
			printk(KERN_ERR "%s plat_priv is NULL!\n", __func__);
			return;
		}

		plat_priv->driver_status = CNSS_LOAD_UNLOAD;
		ops = plat_priv->driver_ops;

		if ((plat_priv->bus_type == CNSS_BUS_AHB) && ops &&
		    (strcmp(driver_ops->name, "pld_ahb") == 0)) {
			subsys_info = &plat_priv->subsys_info;
			if (subsys_info->subsys_handle &&
			    !subsys_info->subsystem_put_in_progress) {
				subsys_info->subsystem_put_in_progress = true;
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
				subsystem_put(subsys_info->subsys_handle);
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
				rproc_shutdown(subsys_info->subsys_handle);
#endif
				subsys_info->subsystem_put_in_progress = false;
			} else {
				ops->remove((struct pci_dev *)
					    plat_priv->plat_dev);
			}

			subsys_info->subsys_handle = NULL;
			cnss_unregister_notifier_cb(plat_priv);
			plat_priv->driver_ops = NULL;
			plat_priv->driver_status = CNSS_UNINITIALIZED;
			plat_priv->driver_state = 0;
		}

		if ((plat_priv->bus_type == CNSS_BUS_PCI) && ops &&
		    (strcmp(driver_ops->name, "pld_pcie") == 0)) {
			set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
			subsys_info = &plat_priv->subsys_info;
			if (subsys_info->subsys_handle &&
			    !subsys_info->subsystem_put_in_progress) {
				subsys_info->subsystem_put_in_progress = true;
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
				subsystem_put(subsys_info->subsys_handle);
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
				rproc_shutdown(subsys_info->subsys_handle);
#endif
				subsys_info->subsys_handle = NULL;
				subsys_info->subsystem_put_in_progress = false;
			} else {
				if (plat_priv->pci_dev)
					ops->remove((struct pci_dev *)plat_priv->plat_dev);
			}
			cnss_unregister_subsys(plat_priv);
			cnss_unregister_notifier_cb(plat_priv);
#else
			cnss_hif_shutdown(plat_priv);
#endif
			plat_priv->driver_ops = NULL;
			plat_priv->driver_status = CNSS_UNINITIALIZED;
			plat_priv->driver_state = 0;
		}
	}
}
EXPORT_SYMBOL(cnss_wlan_unregister_driver);

#ifdef CONFIG_CNSS2_KERNEL_5_15
static int cnss_rproc_recovery(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static int cnss_rproc_start(struct cnss_plat_data *plat_priv)
{
	return 0;
}

void *cnss_register_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	return NULL;
}

int cnss_unregister_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	return 0;
}

void *cnss_register_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	return NULL;
}

int cnss_unregister_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static int cnss_qca8074_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle)
{
	return 0;
}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static int cnss_rproc_recovery(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc = NULL;
	int ret = 0;

	if (!plat_priv)
		return 0;

	rproc = plat_priv->rproc_handle;

	cnss_bus_update_status(plat_priv, CNSS_FW_DOWN);
	if (rproc) {
		ret = rproc_shutdown(rproc);
		if (ret < 0) {
			cnss_pr_err("User pd rproc_stop failed\n");
			return ret;
		}
	}

	if (rproc_rootpd) {
		ret = rproc_shutdown(rproc_rootpd);
		if (ret < 0) {
			cnss_pr_err("Root pd rproc_stop failed\n");
			return ret;
		}
		rproc->ops->coredump(rproc_rootpd);
	} else {
		if (rproc) {
			cnss_qca8074_notifier_nb(&plat_priv->modem_nb,
					CNSS_RAMDUMP_NOTIFICATION, NULL);
			rproc->ops->coredump(rproc);
		}
	}
	return 0;
}

static int cnss_rproc_start(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc_rpd;
	struct rproc *rproc;
	int ret;

	rproc = plat_priv->rproc_handle;
	rproc_rpd = plat_priv->rproc_rpd_handle;
	if (rproc_rpd) {
		ret = rproc_boot(rproc_rpd);
		if (ret < 0) {
			cnss_pr_err("Root pd rproc_start failed\n");
			return ret;
		}
	} else {
		if (rproc) {
			ret = rproc_boot(rproc);
			if (ret < 0) {
				cnss_pr_err("Root pd rproc_start failed\n");
				return ret;
			}
		}
	}
	return 0;
}

#else
int cnss_stop_rproc(struct cnss_plat_data *plat_priv, struct rproc *rproc)
{
	int ret;

	ret = rproc_stop(rproc, true);
	if (ret < 0) {
		cnss_pr_err("Rproc_stop failed for %s device\n",
				rproc->name);
		return ret;
	}
	cnss_pr_info("The %s device is stopped\n", plat_priv->device_name);
	return 0;
}

int cnss_start_rproc(struct cnss_plat_data *plat_priv, struct rproc *rproc)
{
	const struct firmware *firmware_p = NULL;
	struct device *dev;
	int ret = 0;

	dev = &rproc->dev;
	ret = request_firmware(&firmware_p, rproc->firmware, dev);
	if (ret < 0) {
		cnss_pr_err("Request_firmware failed: %d\n", ret);
		return ret;
	}
	ret = rproc_start(rproc, firmware_p);
	if (ret < 0) {
		cnss_pr_err("Rproc_start failed for device %s\n", rproc->name);
		release_firmware(firmware_p);
		return ret;
	}
	return 0;
}

int cnss_handle_usrpd_in_rpd_crash(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc;
	struct platform_device *pdev;
	struct cnss_mlo_group_info *group_info;

	if (!plat_priv)
		return -1;

	rproc = plat_priv->rproc_handle;
	pdev = plat_priv->plat_dev;
	group_info = plat_priv->mlo_group_info;
	if (of_property_read_bool(pdev->dev.of_node, "qcom,multipd_arch")) {
		if (rproc) {
			if (!plat_priv->recovery_enabled) {
				if (plat_priv->mlo_support) {
					group_info->rddm_dump_all++;
				}
			} else {
				if (rproc->state != RPROC_OFFLINE) {
					cnss_bus_update_status(plat_priv,
								CNSS_FW_DOWN);
					rproc->state = RPROC_CRASHED;
					cnss_stop_rproc(plat_priv, rproc);
				}
			}
		}
	}
	return 0;
}

static int cnss_rproc_recovery(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc = NULL, *rproc_text_pd = NULL;
	int user_pd_handle, ret = 0;

	if (!plat_priv)
		return 0;

	rproc = plat_priv->rproc_handle;

	/*RPROC Rootpd stop handling steps:
	 * 1. Stop all Userpd devices.
	 * 2. Stop any Textpd device if any present.
	 * 3. Stop rootpd device and collect coredump.
	 */
	if ((rproc_rootpd) && (plat_priv->crash_type == CNSS_ROOTPD_CRASH)) {
		if (rproc_rootpd->state != RPROC_OFFLINE) {
			for (user_pd_handle = 0; user_pd_handle < plat_env_index; user_pd_handle++)
				cnss_handle_usrpd_in_rpd_crash(plat_env[user_pd_handle]);
			if (rproc_textpd) {
				rproc_text_pd = rproc_textpd;
				if (rproc_text_pd->state != RPROC_OFFLINE) {
					rproc_text_pd->state = RPROC_CRASHED;
					cnss_stop_rproc(plat_priv,
							rproc_text_pd);
				}
			}
			rproc_rootpd->state = RPROC_RUNNING;
			ret = cnss_stop_rproc(plat_priv, rproc_rootpd);
		}
		if (!ret) {
			cnss_qca8074_notifier_nb(&plat_priv->modem_nb,
					CNSS_RAMDUMP_NOTIFICATION, NULL);
			rproc_rootpd->ops->coredump(rproc_rootpd);
		}
	}

	if (rproc) {
		if ((rproc->state != RPROC_OFFLINE) &&
			(plat_priv->crash_type == CNSS_USERPD_CRASH)) {
			cnss_bus_update_status(plat_priv, CNSS_FW_DOWN);
			rproc->state = RPROC_CRASHED;
			ret = cnss_stop_rproc(plat_priv, rproc);
			if (!ret) {
				cnss_qca8074_notifier_nb(&plat_priv->modem_nb,
						CNSS_RAMDUMP_NOTIFICATION, NULL);
				rproc->ops->coredump(rproc);
			}
		}
	}
	return 0;
}

int cnss_handle_usrpd_in_rpd_start(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc;
	struct platform_device *pdev;
	int ret = 0;

	if (!plat_priv)
		return -1;

	rproc = plat_priv->rproc_handle;
	pdev = plat_priv->plat_dev;
	if (of_property_read_bool(pdev->dev.of_node, "qcom,multipd_arch")) {
		if (rproc->state != RPROC_RUNNING) {
			ret = cnss_start_rproc(plat_priv, rproc);
			if (!ret)
				cnss_pr_info("The userpd device %s is started\n",
					     plat_priv->device_name);
		}
	}
	return 0;
}

static int cnss_rproc_start(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc = NULL, *rproc_text_pd = NULL;
	int user_pd_handle;

	if (!plat_priv)
		return 0;

	rproc = plat_priv->rproc_handle;

	/*RPROC Rootpd start handling steps:
	 * 1. Start rootpd device first.
	 * 2. Start textpd device if any present.
	 * 3. Start all userpd device.
	 */
	if ((rproc_rootpd) && (plat_priv->crash_type == CNSS_ROOTPD_CRASH)) {
		if (rproc_rootpd->state != RPROC_RUNNING) {
			cnss_start_rproc(plat_priv, rproc_rootpd);
			if (rproc_textpd) {
				rproc_text_pd = rproc_textpd;
				if (rproc_text_pd->state != RPROC_RUNNING) {
					cnss_start_rproc(plat_priv,
							 rproc_text_pd);
				}
			}
			for (user_pd_handle = 0; user_pd_handle < plat_env_index; user_pd_handle++)
				cnss_handle_usrpd_in_rpd_start(plat_env[user_pd_handle]);
		}
	}

	if ((rproc) && (plat_priv->crash_type == CNSS_USERPD_CRASH)) {
		if (rproc->state != RPROC_RUNNING)
			cnss_start_rproc(plat_priv, rproc);
	}
	plat_priv->crash_type = CNSS_NO_CRASH;
	return 0;
}

static int cnss_qcn9000_notifier_nb(struct notifier_block *nb,
				    unsigned long code,
				    void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, modem_nb);
	struct cnss_wlan_driver *driver_ops = NULL;
	int event_code = cnss_get_event(code);

	if (!plat_priv->cal_in_progress)
		driver_ops = plat_priv->driver_ops;

	if (event_code < 0)
		return NOTIFY_OK;

	if (event_code == CNSS_AFTER_POWERUP) {
		if (driver_ops)
			driver_ops->probe((struct pci_dev *)plat_priv->plat_dev,
					  (const struct pci_device_id *)
					  plat_priv->plat_dev_id);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		set_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	} else if (event_code == CNSS_BEFORE_SHUTDOWN) {
		if (driver_ops)
			driver_ops->remove(
					(struct pci_dev *)plat_priv->plat_dev);

		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
		clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
	} else if (event_code == CNSS_RAMDUMP_NOTIFICATION) {
		if (driver_ops)
			driver_ops->reinit(
					(struct pci_dev *)plat_priv->plat_dev,
					(const struct pci_device_id *)
					plat_priv->plat_dev_id);

		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		return NOTIFY_DONE;
	} else {
		if (driver_ops)
			driver_ops->update_status(
					(struct pci_dev *)plat_priv->plat_dev,
					(const struct pci_device_id *)
					plat_priv->plat_dev_id, event_code);
	}

	return NOTIFY_OK;
}

#endif

void  *__cnss_subsystem_get(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info = &plat_priv->subsys_info;
	bool boot_after_recovery = false;

	plat_priv->target_asserted = 0;
	plat_priv->target_assert_timestamp = 0;

	cnss_pr_info("%s: driver_state: 0x%lx\n", __func__,
		     plat_priv->driver_state);

	if (test_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER, &plat_priv->driver_state))
		boot_after_recovery = true;

	if (subsys_info->subsys_handle &&
	    !test_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER,
		      &plat_priv->driver_state)) {
		cnss_pr_err("%s: error: subsys handle %pK is not NULL\n",
			    __func__, subsys_info->subsys_handle);
		return NULL;
	}
	clear_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER, &plat_priv->driver_state);

	cnss_mount_firmware(plat_priv);
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	subsys_info->subsys_handle =
				subsystem_get(subsys_info->subsys_desc.name);
	if (!subsys_info->subsys_handle) {
		cnss_pr_err("Failed to get subsys_handle!\n");
		goto fail;
	} else if (IS_ERR(subsys_info->subsys_handle)) {
		cnss_pr_err("Failed to do subsystem_get, err = %ld\n",
			    PTR_ERR(subsys_info->subsys_handle));
		goto fail;
	}
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	if (!plat_priv->rproc_handle) {
		cnss_pr_err("%s: rproc_handle is NULL for %s\n",
			    __func__, plat_priv->device_name);
		return NULL;
	}

	if (plat_priv->recovery_enabled && boot_after_recovery &&
	    (plat_priv->recovery_type == CNSS_SYNC_RECOVERY) &&
	    (plat_priv->bus_type == CNSS_BUS_AHB)) {
		/* In this case, rproc_stop was done and not rproc_shutdown,
		 * hence rproc_start has to be done after SSR recovery
		 * instead of rproc_boot. This would be applicable for AHB
		 * radios from IPQ53xx onwards.
		 */
		cnss_rproc_start(plat_priv);

	} else {
		subsys_info->subsys_handle = plat_priv->rproc_handle;
		if (rproc_boot(subsys_info->subsys_handle)) {
			cnss_pr_err("%s: error: rproc_boot failed for %s\n",
					__func__, plat_priv->device_name);
			goto fail;
		}
	}
#endif
	return subsys_info->subsys_handle;

fail:
	CNSS_ASSERT(0);
	return NULL;
}

void __cnss_subsystem_put(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info = &plat_priv->subsys_info;

	if (!subsys_info->subsys_handle) {
		cnss_pr_err("%s: error: subsys handle is NULL", __func__);
		return;
	}

	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);

	if (!subsys_info->subsystem_put_in_progress) {
		subsys_info->subsystem_put_in_progress = true;
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
		subsystem_put(subsys_info->subsys_handle);
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
		rproc_shutdown(subsys_info->subsys_handle);
#endif
		subsys_info->subsystem_put_in_progress = false;
		subsys_info->subsys_handle = NULL;
		plat_priv->driver_state = 0;
	}
}

void cnss_request_pm_qos(struct device *dev, u32 qos_val)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

#if (KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE)
	cpu_latency_qos_add_request(&plat_priv->qos_request,
				    qos_val);
#else
	pm_qos_add_request(&plat_priv->qos_request, PM_QOS_CPU_DMA_LATENCY,
			   qos_val);
#endif
}
EXPORT_SYMBOL(cnss_request_pm_qos);

void cnss_remove_pm_qos(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

#if (KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE)
	cpu_latency_qos_remove_request(&plat_priv->qos_request);
#else
	pm_qos_remove_request(&plat_priv->qos_request);
#endif
}
EXPORT_SYMBOL(cnss_remove_pm_qos);

static int cnss_qca8074_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, modem_nb);
	struct cnss_wlan_driver *driver_ops = NULL;
	int event_code = cnss_get_event(code);

	if (!plat_priv->cal_in_progress)
		driver_ops = plat_priv->driver_ops;

	if (event_code < 0)
		return NOTIFY_OK;

	if (event_code == CNSS_AFTER_POWERUP) {
		if (driver_ops)
			driver_ops->probe((struct pci_dev *)plat_priv->plat_dev,
					  (const struct pci_device_id *)
					  plat_priv->plat_dev_id);
	} else if (event_code == CNSS_BEFORE_SHUTDOWN) {
		if (driver_ops)
			driver_ops->remove(
					(struct pci_dev *)plat_priv->plat_dev);
	} else if (event_code == CNSS_RAMDUMP_NOTIFICATION) {
#ifdef CONFIG_CNSS2_KERNEL_IPQ
#if IS_ENABLED(CONFIG_CORESIGHT)
		coresight_abort();
#endif
#endif
		if (driver_ops)
			driver_ops->reinit(
					(struct pci_dev *)plat_priv->plat_dev,
					(const struct pci_device_id *)
					plat_priv->plat_dev_id);
		return NOTIFY_DONE;
	} else {
		if (event_code == CNSS_AFTER_SHUTDOWN) {
			clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
			clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);
			/* FW handles coresight settings for QDSS for all
			 * targets from 11be family onwards. Hence, clear QDSS
			 * state to get it started automatically after
			 * SSR recovery.
			 */
			if (plat_priv->device_id == QCA5332_DEVICE_ID ||
				plat_priv->device_id == QCA5424_DEVICE_ID ||
				plat_priv->device_id == QCN6432_DEVICE_ID)
				clear_bit(CNSS_QDSS_STARTED,
					  &plat_priv->driver_state);
			cnss_bus_free_fw_mem(plat_priv);
			cnss_bus_free_qdss_mem(plat_priv);
			cnss_free_soc_info(plat_priv);
		}
		if (driver_ops)
			driver_ops->update_status(
					(struct pci_dev *)plat_priv->plat_dev,
					(const struct pci_device_id *)
					plat_priv->plat_dev_id, event_code);
	}

	return NOTIFY_OK;
}

static int cnss_qca8074_rpd_notifier_atomic_nb(struct notifier_block *nb,
	unsigned long code,
	void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, rpd_atomic_nb);
	struct cnss_wlan_driver *driver_ops;
	int event_code = cnss_get_event(code);
	enum cnss_recovery_reason cnss_reason;

	driver_ops = plat_priv->driver_ops;

	if (event_code < 0)
		return NOTIFY_OK;

	if (event_code == CNSS_PREPARE_FOR_FATAL_SHUTDOWN) {
		cnss_pr_err("XXX TARGET ASSERTED XXX\n");
		cnss_pr_err("XXX TARGET %s instance_id 0x%x plat_env idx %d XXX\n",
			    plat_priv->device_name,
			    plat_priv->wlfw_service_instance_id,
			    cnss_get_plat_env_index_from_plat_priv(plat_priv));
		plat_priv->target_asserted = 1;
		plat_priv->target_assert_timestamp = ktime_to_ms(ktime_get());

		if (rproc_rootpd) {
			if ((rproc_rootpd->state != RPROC_OFFLINE) &&
			   (rproc_rootpd->state != RPROC_CRASHED)) {
				plat_priv->crash_type = CNSS_ROOTPD_CRASH;
				rproc_rootpd->state = RPROC_CRASHED;
				cnss_reason = CNSS_REASON_FATAL_SHUTDOWN;
				cnss_schedule_recovery(&plat_priv->plat_dev->dev,
							cnss_reason);
			}
		}
	}

	return NOTIFY_OK;

}

static int cnss_qca8074_rpd_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle)
{
	return NOTIFY_OK;
}

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
void *cnss_register_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;
	void *ss_handle = NULL;

	subsys_info = &plat_priv->subsys_info;
	subsys_info->subsys_desc.name = plat_priv->device_name;
	plat_priv->modem_nb.notifier_call = cnss_qcn9000_notifier_nb;
	ss_handle = subsys_notif_register_notifier(
		subsys_info->subsys_desc.name, &plat_priv->modem_nb);

	return ss_handle;
}

void *cnss_register_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;
	void *ss_handle = NULL;

	subsys_info = &plat_priv->subsys_info;
	plat_priv->modem_nb.notifier_call = cnss_qca8074_notifier_nb;
	ss_handle = subsys_notif_register_notifier(
		subsys_info->subsys_desc.name, &plat_priv->modem_nb);

	return ss_handle;
}

int cnss_unregister_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	void *handler = plat_priv->esoc_info.modem_notify_handler;

	if (handler) {
		subsys_notif_unregister_notifier(handler, &plat_priv->modem_nb);
		memset(&plat_priv->modem_nb, 0, sizeof(struct notifier_block));
		plat_priv->esoc_info.modem_notify_handler = NULL;
	}
	return 0;
}

int cnss_unregister_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	void *handler = plat_priv->esoc_info.modem_notify_handler;

	if (handler) {
		subsys_notif_unregister_notifier(handler, &plat_priv->modem_nb);
		memset(&plat_priv->modem_nb, 0, sizeof(struct notifier_block));
		plat_priv->esoc_info.modem_notify_handler = NULL;
	}
	return 0;
}

#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
void *cnss_register_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	return NULL;
}
#else
void *cnss_register_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;
	void *ss_handle = NULL;
	int ret = 0;

	subsys_info = &plat_priv->subsys_info;
	subsys_info->subsys_desc.name = plat_priv->device_name;

	plat_priv->modem_nb.notifier_call = cnss_qcn9000_notifier_nb;
	plat_priv->modem_atomic_nb.notifier_call =
				cnss_qcn9000_notifier_atomic_nb;
	ret = rproc_register_subsys_notifier(subsys_info->subsys_desc.name,
			&plat_priv->modem_nb, &plat_priv->modem_atomic_nb);
	if (ret) {
		cnss_pr_err("%s: failed to register rproc ret %d\n",
			    __func__, ret);
		return NULL;
	}
	ss_handle = subsys_info;

	return ss_handle;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
void *cnss_register_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;
	void *ss_handle = NULL;
	struct rproc *rproc_rpd;

	subsys_info = &plat_priv->subsys_info;
	plat_priv->modem_nb.notifier_call = cnss_qca8074_notifier_nb;
	plat_priv->modem_atomic_nb.notifier_call =
					cnss_qca8074_notifier_atomic_nb;
	plat_priv->notifier_list[0] = qcom_register_ssr_notifier(subsys_info->subsys_desc.name, &plat_priv->modem_nb);
	plat_priv->notifier_list[1] =
		qcom_register_ssr_atomic_notifier(subsys_info->subsys_desc.name,
						  &plat_priv->modem_atomic_nb);

	rproc_rpd = plat_priv->rproc_rpd_handle;
	if (rproc_rpd) {
		plat_priv->rpd_nb.notifier_call = cnss_qca8074_rpd_notifier_nb;
		plat_priv->rpd_atomic_nb.notifier_call =
			cnss_qca8074_rpd_notifier_atomic_nb;
		plat_priv->notifier_list[0] = qcom_register_ssr_notifier(rproc_rpd->name, &plat_priv->rpd_nb);
		plat_priv->notifier_list[1] =
			qcom_register_ssr_atomic_notifier(rproc_rpd->name,
						&plat_priv->rpd_atomic_nb);
	}

	ss_handle = subsys_info;
	return ss_handle;
}

int cnss_unregister_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	struct rproc *rproc_rpd;

	if (plat_priv->modem_nb.notifier_call) {
	qcom_unregister_ssr_notifier(plat_priv->notifier_list[0], &plat_priv->modem_nb);
	qcom_unregister_ssr_atomic_notifier(plat_priv->notifier_list[1],
					    &plat_priv->modem_atomic_nb);
		memset(&plat_priv->modem_nb, 0, sizeof(struct notifier_block));
		memset(&plat_priv->modem_atomic_nb, 0,
		       sizeof(struct notifier_block));
	}

	rproc_rpd = plat_priv->rproc_rpd_handle;
	if (rproc_rpd) {
		if (plat_priv->rpd_nb.notifier_call) {
	qcom_unregister_ssr_notifier(plat_priv->notifier_list[0], &plat_priv->rpd_nb);
	qcom_unregister_ssr_atomic_notifier(plat_priv->notifier_list[1],
					    &plat_priv->rpd_atomic_nb);
			memset(&plat_priv->rpd_nb, 0,
					sizeof(struct notifier_block));
			memset(&plat_priv->rpd_atomic_nb, 0,
					sizeof(struct notifier_block));
		}
	}

	return 0;
}
#else
static int cnss_check_ahb_rpd_register(struct cnss_plat_data *plat_priv)
{
	int userpd = 0, rpd_registered = 0;
	struct platform_device *pdev;

	if (!plat_priv)
		return -EINVAL;

	for (userpd = 0; userpd < plat_env_index; userpd++) {
		if (plat_env[userpd]->bus_type == CNSS_BUS_AHB) {
			pdev = plat_env[userpd]->plat_dev;
			if (of_property_read_bool(pdev->dev.of_node,
			   "qcom,multipd_arch")) {
				if(plat_env[userpd]->rpd_nb.notifier_call) {
					rpd_registered = 1;
					break;
				}
			} else {
				rpd_registered = -1;
			}
		}
	}
	return rpd_registered;
}

void *cnss_register_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;
	void *ss_handle = NULL;
	int register_rpd_notifier = 0, ret = 0;

	subsys_info = &plat_priv->subsys_info;
	plat_priv->modem_nb.notifier_call = cnss_qca8074_notifier_nb;
	plat_priv->modem_atomic_nb.notifier_call =
					cnss_qca8074_notifier_atomic_nb;
	ret = rproc_register_subsys_notifier(subsys_info->subsys_desc.name,
			&plat_priv->modem_nb, &plat_priv->modem_atomic_nb);
	if (ret) {
		cnss_pr_err("%s: failed to register rproc ret %d\n",
			    __func__, ret);
		return NULL;
	}

	/* Register Rootpd notifiers only if its not registered and we
	 * need to register only for one AHB SOC.
	 */
	register_rpd_notifier = cnss_check_ahb_rpd_register(plat_priv);

	if ((rproc_rootpd) && (!register_rpd_notifier) &&
	    (plat_priv->recovery_type == CNSS_SYNC_RECOVERY)) {
		plat_priv->rpd_nb.notifier_call = cnss_qca8074_rpd_notifier_nb;
		plat_priv->rpd_atomic_nb.notifier_call =
			cnss_qca8074_rpd_notifier_atomic_nb;
		ret = rproc_register_subsys_notifier(rproc_rootpd->name,
				&plat_priv->rpd_nb, &plat_priv->rpd_atomic_nb);
	}

	ss_handle = subsys_info;
	return ss_handle;
}

int cnss_unregister_qca8074_cb(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (plat_priv->modem_nb.notifier_call) {
		ret = rproc_unregister_subsys_notifier(
				plat_priv->subsys_info.subsys_desc.name,
				&plat_priv->modem_nb,
				&plat_priv->modem_atomic_nb);
		if (ret) {
			cnss_pr_err("%s: failed to unregister ret %d\n",
				    __func__, ret);
			return ret;
		}
		memset(&plat_priv->modem_nb, 0, sizeof(struct notifier_block));
		memset(&plat_priv->modem_atomic_nb, 0,
		       sizeof(struct notifier_block));
	}

	if (rproc_rootpd) {
		if (plat_priv->rpd_nb.notifier_call) {
			ret = rproc_unregister_subsys_notifier(
					rproc_rootpd->name,
					&plat_priv->rpd_nb,
					&plat_priv->rpd_atomic_nb);
			if (ret) {
				cnss_pr_err("%s: failed to unregister rootpd ret %d\n",
						__func__, ret);
				return ret;
			}
			memset(&plat_priv->rpd_nb, 0,
					sizeof(struct notifier_block));
			memset(&plat_priv->rpd_atomic_nb, 0,
					sizeof(struct notifier_block));
		}
	}

	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
int cnss_unregister_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	return 0;
}
#else
int cnss_unregister_qcn9000_cb(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (plat_priv->modem_nb.notifier_call) {
		ret = rproc_unregister_subsys_notifier(
				plat_priv->subsys_info.subsys_desc.name,
				&plat_priv->modem_nb,
				&plat_priv->modem_atomic_nb);
		if (ret) {
			cnss_pr_err("%s: failed to unregister ret %d\n",
				    __func__, ret);
			return ret;
		}
		memset(&plat_priv->modem_nb, 0, sizeof(struct notifier_block));
		memset(&plat_priv->modem_atomic_nb, 0,
		       sizeof(struct notifier_block));
	}

	return 0;
}
#endif
#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
int cnss_handle_usrpd_in_rpd_crash(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static int cnss_get_node_id(struct platform_device *plat_dev,
			    unsigned long device_id, u32 *node_id)
{
	struct cnss_plat_data *plat_priv = NULL;

	if (of_property_read_u32(plat_dev->dev.of_node,
				 "node_id", node_id)) {
		cnss_pr_err("Error: No node_id in device_tree\n");
		CNSS_ASSERT(0);
		return -ENODEV;
	}

	switch (device_id) {
	case QCN9000_DEVICE_ID:
		*node_id = *node_id + QCN9000_0;
		break;
	case QCN9224_DEVICE_ID:
		*node_id = *node_id + QCN9224_0;
		break;
	default:
		cnss_pr_dbg("Invalid device id 0x%lx", device_id);
		break;
	}

	return 0;
}
#else
static int cnss_get_node_id(struct platform_device *plat_dev,
			    unsigned long device_id, u32 *node_id)
{
	struct cnss_plat_data *plat_priv = NULL;

	if (of_property_read_u32(plat_dev->dev.of_node,
				 "qrtr_node_id", node_id)) {
		cnss_pr_err("Error: No qrtr_node_id in device_tree\n");
		CNSS_ASSERT(0);
		return -ENODEV;
	}

	return 0;
}
#endif


void cnss_bus_dev_to_plat_priv_wrapper(struct device *dev,
				       int device_id,
				       struct cnss_plat_data **plat_priv)
{
	struct pci_dev *pcidev;

	if (cnss_get_bus_type(device_id) == CNSS_BUS_AHB) {
		*plat_priv = cnss_bus_dev_to_plat_priv(dev);
	} else {
		pcidev = container_of(dev, struct pci_dev, dev);
		*plat_priv = cnss_get_plat_priv_dev_by_pci_dev(pcidev);
	}
}

#ifdef CONFIG_CNSS2_KERNEL_5_15
void  *cnss_subsystem_get(struct device *dev, int device_id)
{
	struct cnss_plat_data *plat_priv = NULL;

	cnss_bus_dev_to_plat_priv_wrapper(dev, device_id, &plat_priv);
	if (!plat_priv)
		return NULL;

	return __cnss_hif_get(plat_priv);
}
EXPORT_SYMBOL(cnss_subsystem_get);

void cnss_subsystem_put(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	__cnss_hif_put(plat_priv);
}
EXPORT_SYMBOL(cnss_subsystem_put);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
void  *cnss_subsystem_get(struct device *dev, int device_id)
{
	struct cnss_plat_data *plat_priv = NULL;

	cnss_bus_dev_to_plat_priv_wrapper(dev, device_id, &plat_priv);
	if (!plat_priv)
		return NULL;

	if(cnss_get_bus_type(device_id) == CNSS_BUS_AHB)
		return __cnss_subsystem_get(plat_priv);
	else
		return __cnss_hif_get(plat_priv);
}
EXPORT_SYMBOL(cnss_subsystem_get);

void cnss_subsystem_put(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	if(plat_priv->bus_type == CNSS_BUS_AHB)
		__cnss_subsystem_put(plat_priv);
	else
		__cnss_hif_put(plat_priv);
}
EXPORT_SYMBOL(cnss_subsystem_put);
#else
void  *cnss_subsystem_get(struct device *dev, int device_id)
{
	struct cnss_plat_data *plat_priv = NULL;

	cnss_bus_dev_to_plat_priv_wrapper(dev, device_id, &plat_priv);
	if (!plat_priv)
		return NULL;

	return __cnss_subsystem_get(plat_priv);
}
EXPORT_SYMBOL(cnss_subsystem_get);

void cnss_subsystem_put(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	__cnss_subsystem_put(plat_priv);
}
EXPORT_SYMBOL(cnss_subsystem_put);
#endif

#ifdef CONFIG_CNSS2_PM
static int cnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, modem_nb);
	struct cnss_esoc_info *esoc_info;
	int event_code = cnss_get_event(code);
	cnss_pr_dbg("Modem notifier: event %lu\n", event_code);

	if (!plat_priv)
		return NOTIFY_DONE;

	if (event_code)
		return NOTIFY_OK;

	esoc_info = &plat_priv->esoc_info;

	if (event_code == CNSS_AFTER_POWERUP)
		esoc_info->modem_current_status = 1;
	else if (event_code == CNSS_BEFORE_SHUTDOWN)
		esoc_info->modem_current_status = 0;
	else
		return NOTIFY_DONE;

	if (!cnss_bus_driver_modem_status(plat_priv,
					       esoc_info->modem_current_status))
		return NOTIFY_DONE;

	return NOTIFY_OK;
}

static int cnss_register_esoc(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_esoc_info *esoc_info;
	struct esoc_desc *esoc_desc;
	const char *client_desc;

	dev = &plat_priv->plat_dev->dev;
	esoc_info = &plat_priv->esoc_info;

	esoc_info->notify_modem_status =
		of_property_read_bool(dev->of_node,
				      "qcom,notify-modem-status");

	if (!esoc_info->notify_modem_status)
		goto out;

	ret = of_property_read_string_index(dev->of_node, "esoc-names", 0,
					    &client_desc);
	if (ret) {
		cnss_pr_dbg("esoc-names is not defined in DT, skip!\n");
	} else {
		esoc_desc = devm_register_esoc_client(dev, client_desc);
		if (IS_ERR_OR_NULL(esoc_desc)) {
			ret = PTR_RET(esoc_desc);
			cnss_pr_err("Failed to register esoc_desc, err = %d\n",
				    ret);
			goto out;
		}
		esoc_info->esoc_desc = esoc_desc;
	}

	plat_priv->modem_nb.notifier_call = cnss_modem_notifier_nb;
	esoc_info->modem_current_status = 0;
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	esoc_info->modem_notify_handler =
		subsys_notif_register_notifier(esoc_info->esoc_desc ?
					       esoc_info->esoc_desc->name :
					       "modem", &plat_priv->modem_nb);
	if (IS_ERR(esoc_info->modem_notify_handler)) {
		ret = PTR_ERR(esoc_info->modem_notify_handler);
		cnss_pr_err("Failed to register esoc notifier, err = %d\n",
			    ret);
		goto unreg_esoc;
	}
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	ret = rproc_register_subsys_notifier(esoc_info->esoc_desc ?
					 esoc_info->esoc_desc->name :
					 "modem", &plat_priv->modem_nb, NULL);
	if (ret) {
		cnss_pr_err("%s: Failed register rproc. ret %d\n", __func__,
			     ret);
		return ret
	}
#endif

	return 0;
unreg_esoc:
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
out:
	return ret;
}

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
static void cnss_unregister_esoc(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_esoc_info *esoc_info;

	dev = &plat_priv->plat_dev->dev;
	esoc_info = &plat_priv->esoc_info;

	if (esoc_info->notify_modem_status)
		subsys_notif_unregister_notifier
		(esoc_info->modem_notify_handler,
		 &plat_priv->modem_nb);
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
}
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
static void cnss_unregister_esoc(struct cnss_plat_data *plat_priv)
{
	struct cnss_esoc_info *esoc_info;
	int ret = 0;

	esoc_info = &plat_priv->esoc_info;
	ret = rproc_unregister_subsys_notifier("modem",
					       &plat_priv->modem_nb, NULL);
	if (ret) {
		cnss_pr_err("%s: Failed to unregister rproc. ret %d\n",
			    __func__, ret);
		return;
	}
}
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
static int cnss_subsys_powerup(const struct subsys_desc *subsys_desc)
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
static int cnss_subsys_powerup(struct rproc *subsys_desc)
#endif
{
	struct cnss_plat_data *plat_priv = NULL;
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	if (!subsys_desc->dev) {
		printk(KERN_ERR "dev from subsys_desc is NULL\n");
		return -ENODEV;
	}

	plat_priv = dev_get_drvdata(subsys_desc->dev);
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	plat_priv = dev_get_drvdata(subsys_desc->dev.parent);
#endif
	if (!plat_priv)
		return -ENODEV;

	plat_priv->target_asserted = 0;
	plat_priv->target_assert_timestamp = 0;
	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
#if defined CNSS_PCI_SUPPORT
	if (cnss_pci_probe(plat_priv->pci_dev,
			     plat_priv->pci_dev_id,
			     plat_priv)) {
		pr_err("ERROR : %s:%d \n", __func__, __LINE__);
		return -ENODEV;
	}
#endif
	if (!plat_priv->driver_state) {
		cnss_pr_dbg("Powerup is ignored\n");
		return 0;
	}

	return cnss_bus_dev_powerup(plat_priv);
}

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
static int cnss_subsys_shutdown(const struct subsys_desc *subsys_desc,
				bool force_stop)
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
static int cnss_subsys_shutdown(struct rproc *subsys_desc)
#endif
{
	struct cnss_plat_data *plat_priv = NULL;

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	if (!subsys_desc->dev) {
		pr_err("dev from subsys_desc is NULL\n");
		return -ENODEV;
	}

	plat_priv = dev_get_drvdata(subsys_desc->dev);
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	plat_priv = dev_get_drvdata(subsys_desc->dev.parent);
#endif
	if (!plat_priv) {
		printk(KERN_ERR "plat_priv is NULL!\n");
		return -ENODEV;
	}

	if (!plat_priv->driver_state) {
		cnss_pr_dbg("shutdown is ignored\n");
		return 0;
	}

	return cnss_bus_dev_shutdown(plat_priv);
}

#ifdef CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK
static int cnss_subsys_dummy_load(struct rproc *subsys_desc,
				  const struct firmware *fw)
{
	/*no firmware load it will be taken care by pci and mhi*/
	return 0;
}
#endif
#endif

void cnss_device_crashed(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv)
		return;

	subsys_info = &plat_priv->subsys_info;
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	if (subsys_info->subsys_device) {
		set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		subsys_set_crash_status(subsys_info->subsys_device, true);
		subsystem_restart_dev(subsys_info->subsys_device);
	}
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	if (subsys_info->subsys_handle) {
		set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		rproc_report_crash(subsys_info->subsys_handle,
						RPROC_FATAL_ERROR);
	}
#endif
}
EXPORT_SYMBOL(cnss_device_crashed);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
static void cnss_subsys_crash_shutdown(const struct subsys_desc *subsys_desc)
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
static void cnss_subsys_crash_shutdown(struct rproc *subsys_desc)
#endif
{
#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	struct cnss_plat_data *plat_priv =
				dev_get_drvdata(subsys_desc->dev.parent);
#endif

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	cnss_bus_dev_crash_shutdown(plat_priv);
}

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
static int cnss_subsys_ramdump(int enable,
			       const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	return cnss_bus_dev_ramdump(plat_priv);
}
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
static void cnss_subsys_ramdump(struct rproc *subsys_desc,
				struct rproc_dump_segment *segment,
				void  *dest)
{
	struct cnss_plat_data *plat_priv = NULL;

	plat_priv = dev_get_drvdata(subsys_desc->dev.parent);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	cnss_bus_dev_ramdump(plat_priv);
}
#endif

#ifdef CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK
static int cnss_subsys_add_ramdump_callback(struct rproc *subsys_desc,
		const struct firmware *firmware)
{
	struct cnss_plat_data *plat_priv = NULL;
	int ret = 0;

	plat_priv = dev_get_drvdata(subsys_desc->dev.parent);
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is NULL\n", __func__);
		return -1;
	}
	ret = rproc_coredump_add_custom_segment(subsys_desc, 0, 0,
						cnss_subsys_ramdump, NULL);

	if (ret) {
		cnss_pr_err("%s: Failed to add custom segment ret %d\n",
			    __func__, ret);
		return ret;
	}
	return ret;
}
#endif
#endif

void *cnss_get_virt_ramdump_mem(struct device *dev, unsigned long *size)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_ramdump_info *ramdump_info;

	if (!plat_priv)
		return NULL;

	ramdump_info = &plat_priv->ramdump_info;
	*size = ramdump_info->ramdump_size;

	return ramdump_info->ramdump_va;
}
EXPORT_SYMBOL(cnss_get_virt_ramdump_mem);

static const char *cnss_recovery_reason_to_str(enum cnss_recovery_reason reason)
{
	switch (reason) {
	case CNSS_REASON_DEFAULT:
		return "DEFAULT";
	case CNSS_REASON_LINK_DOWN:
		return "LINK_DOWN";
	case CNSS_REASON_RDDM:
		return "RDDM";
	case CNSS_REASON_TIMEOUT:
		return "TIMEOUT";
	case CNSS_REASON_FATAL_SHUTDOWN:
		return "FATAL_SHUTDOWN";
	}

	return "UNKNOWN";
};

static int cnss_do_recovery(struct cnss_plat_data *plat_priv,
			    enum cnss_recovery_reason reason)
{
#ifndef CONFIG_CNSS2_KERNEL_5_15
	struct cnss_subsys_info *subsys_info =
		&plat_priv->subsys_info;
#endif
	unsigned long rddm_lock;
	struct cnss_mlo_group_info *group_info = plat_priv->mlo_group_info;
	int ret = 0, userpd = 0;

	plat_priv->recovery_count++;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto self_recovery;

	if (test_bit(SKIP_RECOVERY, &plat_priv->ctrl_params.quirks)) {
		cnss_pr_dbg("Skip device recovery\n");
		return 0;
	}

	switch (reason) {
	case CNSS_REASON_LINK_DOWN:
		if (test_bit(LINK_DOWN_SELF_RECOVERY,
			     &plat_priv->ctrl_params.quirks))
			goto self_recovery;
		break;
	case CNSS_REASON_RDDM:
	case CNSS_REASON_FATAL_SHUTDOWN:
		if (plat_priv->bus_type == CNSS_BUS_PCI)
			cnss_bus_collect_dump_info(plat_priv, false);
		if (plat_priv->mlo_support && !plat_priv->recovery_enabled &&
				group_info != NULL &&
				plat_priv->crash_type != CNSS_ROOTPD_CRASH) {
			spin_lock_irqsave(&rddm_spinlock, rddm_lock);
			group_info->rddm_dump_all++;
			spin_unlock_irqrestore(&rddm_spinlock, rddm_lock);
		}
		break;
	case CNSS_REASON_DEFAULT:
	case CNSS_REASON_TIMEOUT:
		break;
	default:
		cnss_pr_err("Unsupported recovery reason: %s(%d)\n",
			    cnss_recovery_reason_to_str(reason), reason);
		break;
	}

	/* If recovery is enabled, fatal notification is sent to wifi driver
	 * as soon as MHI notifies error.
	 * If recovery is disabled, send fatal notification here after RDDM
	 * is done so that we have the RDDM information before the assert
	 * is triggered from the wifi driver.
	 */
	if (!plat_priv->recovery_enabled) {
		/* This is a special case where ramdump file is uploaded to
		 * TFTP and then host assert happens. It is disabled by default.
		 */
		if (ramdump_enabled)
			cnss_bus_dev_ramdump(plat_priv);
		if (plat_priv->mlo_support && group_info != NULL &&
		    plat_priv->recovery_mode != MODE_1_RECOVERY_MODE &&
		    !plat_priv->standby_mode) {
			if (plat_priv->crash_type == CNSS_ROOTPD_CRASH) {
				for (userpd = 0; userpd < plat_env_index; userpd++)
					cnss_handle_usrpd_in_rpd_crash(plat_env[userpd]);
			}
			if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state))
				cnss_pr_info("FW_READY not received for the device, so early assert\n");
			else if (group_info->num_chips != group_info->rddm_dump_all)
				return 0;
		}
		ret = cnss_bus_update_status(plat_priv, CNSS_FW_DOWN);
		if (ret) {
			/* Call CNSS_ASSERT if fatal call is missed in down
			 * path. Target assert can happen in down path and
			 * fatal is not called since the driver_ops is NULL.
			 */
#if defined CNSS_PCI_SUPPORT
			struct cnss_pci_data *pci_priv = NULL;
			pci_priv = plat_priv->bus_priv;
			if ((plat_priv->bus_type == CNSS_BUS_PCI) && pci_priv)
				plat_priv = pci_priv->plat_priv;
#endif
			CNSS_ASSERT(0);
		}
	}

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	if (!subsys_info->subsys_device)
		return 0;

	subsys_set_crash_status(subsys_info->subsys_device, true);
	subsystem_restart_dev(subsys_info->subsys_device);
#else
#ifndef CONFIG_CNSS2_KERNEL_5_15
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!subsys_info->subsys_handle)
#else
	if ((!subsys_info->subsys_handle) && (plat_priv->bus_type != CNSS_BUS_PCI))
#endif
		return 0;
#endif
	if (plat_priv->mlo_capable) {
		/* For MLO supported targets, power off the target and collect
		 * dump. The power up would be handled by driver to ensure
		 * multiple targets in the MLO group are all powered up in the
		 * correct sequence
		 */
		if (plat_priv->bus_type == CNSS_BUS_PCI) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
			cnss_hif_shutdown(plat_priv);
			cnss_hif_notifier(plat_priv, CNSS_RAMDUMP_NOTIFICATION);
			cnss_bus_dev_ramdump(plat_priv);
			set_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER,
				&plat_priv->driver_state);
			cnss_hif_notifier(plat_priv,
					  CNSS_RAMDUMP_DONE);
			return 0;
#else
			rproc_shutdown(subsys_info->subsys_handle);
			cnss_qcn9000_notifier_nb(&plat_priv->modem_nb,
					CNSS_RAMDUMP_NOTIFICATION, NULL);
			cnss_subsys_ramdump(subsys_info->subsys_handle,
								NULL, NULL);
			set_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER,
				&plat_priv->driver_state);
			cnss_qcn9000_notifier_nb(&plat_priv->modem_nb,
						 CNSS_RAMDUMP_DONE, NULL);
#endif
		} else if ((plat_priv->bus_type == CNSS_BUS_AHB) &&
			(plat_priv->recovery_type == CNSS_SYNC_RECOVERY)) {
			cnss_rproc_recovery(plat_priv);
			set_bit(CNSS_RECOVERY_WAIT_FOR_DRIVER,
				&plat_priv->driver_state);
			if (plat_priv->crash_type == CNSS_ROOTPD_CRASH) {
				for (userpd = 0; userpd < plat_env_index; userpd++) {
					if (plat_env[userpd]->bus_type == CNSS_BUS_AHB) {
						cnss_qca8074_notifier_nb(&plat_env[userpd]->modem_nb,
									CNSS_RAMDUMP_DONE, NULL);
					}
				}
			} else {
				cnss_qca8074_notifier_nb(&plat_priv->modem_nb,
							CNSS_RAMDUMP_DONE, NULL);
			}
		}
	} else {
		if (plat_priv->bus_type == CNSS_BUS_PCI) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
			schedule_work(&plat_priv->crash_work);
#else
			rproc_report_crash(subsys_info->subsys_handle,
					RPROC_FATAL_ERROR);
#endif
		} else if ((plat_priv->bus_type == CNSS_BUS_AHB) &&
			(plat_priv->recovery_type == CNSS_SYNC_RECOVERY)) {
			cnss_rproc_recovery(plat_priv);
			cnss_rproc_start(plat_priv);
		}
	}
#endif
	return 0;

self_recovery:
	cnss_bus_dev_shutdown(plat_priv);
	cnss_bus_dev_powerup(plat_priv);

	return 0;
}

static int cnss_driver_recovery_hdlr(struct cnss_plat_data *plat_priv,
				     void *data)
{
	struct cnss_recovery_data *recovery_data = data;
	int ret = 0;

	cnss_pr_dbg("Driver recovery is triggered with reason: %s(%d)\n",
		    cnss_recovery_reason_to_str(recovery_data->reason),
		    recovery_data->reason);

	if (!plat_priv->driver_state) {
		cnss_pr_err("Improper driver state, ignore recovery\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_err("Recovery is already in progress, state 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) {
		cnss_pr_err("Driver unload or idle shutdown is in progress, ignore recovery\n");
		BUG_ON(1);
		ret = -EINVAL;
		goto out;
	}

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
		    test_bit(CNSS_DRIVER_IDLE_RESTART,
			     &plat_priv->driver_state)) {
			cnss_pr_err("Driver load or idle restart is in progress, ignore recovery\n");
			ret = -EINVAL;
			goto out;
		}
		break;
	default:
		if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
			set_bit(CNSS_FW_BOOT_RECOVERY,
				&plat_priv->driver_state);
		}
		break;
	}

	set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	ret = cnss_do_recovery(plat_priv, recovery_data->reason);

out:
	kfree(data);
	return ret;
}

int cnss_self_recovery(struct device *dev,
		       enum cnss_recovery_reason reason)
{
	cnss_schedule_recovery(dev, reason);
	return 0;
}
EXPORT_SYMBOL(cnss_self_recovery);

void cnss_schedule_recovery(struct device *dev,
			    enum cnss_recovery_reason reason)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		pr_err("plat_priv is NULL\n");
		return;
	}

	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) {
		cnss_pr_err("Driver unload or idle shutdown is in progress, ignore schedule recovery\n");
		BUG_ON(1);
		return;
	}

	plat_priv->reason = reason;
	queue_work(plat_priv->recovery_wq, &plat_priv->recovery_work);
}
EXPORT_SYMBOL(cnss_schedule_recovery);

int cnss_force_fw_assert(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		printk(KERN_ERR "plat_priv is NULL\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		cnss_pr_info("Forced FW assert is not supported\n");
		return -EOPNOTSUPP;
	}
#if defined CNSS_PCI_SUPPORT
	if (cnss_pci_is_device_down(dev)) {
		cnss_pr_info("Device is already in bad state, ignore force assert\n");
		return 0;
	}
#endif
	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, ignore forced FW assert\n");
		return 0;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_FORCE_FW_ASSERT, 0,
			       NULL);

	return 0;
}
EXPORT_SYMBOL(cnss_force_fw_assert);

int cnss_force_collect_rddm(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		cnss_pr_info("Force collect rddm is not supported\n");
		return -EOPNOTSUPP;
	}
#if defined CNSS_PCI_SUPPORT
	if (cnss_pci_is_device_down(dev)) {
		cnss_pr_info("Device is already in bad state, ignore force collect rddm\n");
		return 0;
	}
#endif
	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, ignore forced collect rddm\n");
		return 0;
	}

	if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) {
		cnss_pr_info("Loading/Unloading/idle restart/shutdown is in progress, ignore forced collect rddm\n");
		return 0;
	}

	ret = cnss_bus_force_fw_assert_hdlr(plat_priv);
	if (ret)
		return ret;

	reinit_completion(&plat_priv->rddm_complete);
	ret = wait_for_completion_timeout
		(&plat_priv->rddm_complete,
		 msecs_to_jiffies(CNSS_RDDM_TIMEOUT_MS));
	if (!ret)
		ret = -ETIMEDOUT;

	return ret;
}
EXPORT_SYMBOL(cnss_force_collect_rddm);

static int cnss_cold_boot_cal_start_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (test_bit(CNSS_FW_READY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		cnss_pr_dbg("Device is already active, ignore calibration\n");
		goto out;
	}

	set_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);
	reinit_completion(&plat_priv->cal_complete);
	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret) {
		complete(&plat_priv->cal_complete);
		clear_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);
	}

out:
	return ret;
}

static int cnss_cold_boot_cal_done_hdlr(struct cnss_plat_data *plat_priv,
					void *data)
{
	struct cnss_cal_info *cal_info = data;

	if (!test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state)) {
		cnss_pr_dbg("%s: Driver not in cold boot cal state, returning!\n",
			    __func__);
		goto out;
	}

	switch (cal_info->cal_status) {
	case CNSS_CAL_DONE:
		cnss_pr_info("Coldboot Calibration completed successfully for device 0x%lx\n",
			     plat_priv->device_id);
		cnss_cal_report_upload(plat_priv);
		plat_priv->cal_done = true;
		break;
	case CNSS_CAL_TIMEOUT:
		cnss_pr_dbg("Calibration timed out!\n");
		break;
	default:
		cnss_pr_err("Unknown calibration status: %u\n",
			    cal_info->cal_status);
		break;
	}

	complete(&plat_priv->cal_complete);
	clear_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);

out:
	kfree(data);
	return 0;
}

static int cnss_power_up_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret;

	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret)
		clear_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state);

	return ret;
}

static int cnss_power_down_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_bus_dev_shutdown(plat_priv);

	return 0;
}

static int cnss_qdss_trace_req_mem_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = cnss_bus_alloc_qdss_mem(plat_priv);
	if (ret < 0)
		return ret;

	return cnss_wlfw_qdss_trace_mem_info_send_sync(plat_priv);
}

void *cnss_qdss_trace_pa_to_va(struct cnss_plat_data *plat_priv,
			       u64 pa, u32 size, int *seg_id)
{
	int i = 0;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	u64 offset = 0;
	void *va = NULL;
	u64 local_pa;
	u32 local_size;

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		local_pa = (u64)qdss_mem[i].pa;
		local_size = (u32)qdss_mem[i].size;
		if (pa == local_pa && size <= local_size) {
			va = qdss_mem[i].va;
			break;
		}
		if (pa > local_pa &&
		    pa < local_pa + local_size &&
		    pa + size <= local_pa + local_size) {
			offset = pa - local_pa;
			va = qdss_mem[i].va + offset;
			break;
		}
	}

	*seg_id = i;
	return va;
}

static void get_updated_qdss_trace_filename(struct cnss_plat_data *plat_priv,
					    char *raw_file_name,
					    char *file_name, size_t size)
{
	char *file_suffix = NULL;
	char file_prefix[QDSS_TRACE_FILE_NAME_MAX] = {};

	file_suffix = strnstr(raw_file_name, ".bin",
			      QDSS_TRACE_FILE_NAME_MAX);

	if (file_suffix) {
		strlcpy(file_prefix, raw_file_name,
			(file_suffix - &raw_file_name[0]) + 1);

		snprintf(file_name, size, "%s_%s%s",
			 file_prefix, plat_priv->device_name, file_suffix);
	} else {
		snprintf(file_name, size, "%s_%s",
			 raw_file_name, plat_priv->device_name);
	}
}

static int cnss_qdss_trace_save_hdlr(struct cnss_plat_data *plat_priv,
				     void *data)
{
	struct cnss_qmi_event_qdss_trace_save_data *event_data = data;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int ret = 0;
	int i;
	char file_name[CNSS_GENL_STR_LEN_MAX];

	if (!plat_priv->qdss_mem_seg_len) {
		cnss_pr_err("Memory for QDSS trace is not available\n");
		return -ENOMEM;
	}

	get_updated_qdss_trace_filename(plat_priv,
					event_data->file_name, file_name,
					sizeof(file_name));

	if (event_data->mem_seg_len == 0) {
		for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
			ret = cnss_genl_send_msg(qdss_mem[i].va,
						 CNSS_GENL_MSG_TYPE_QDSS,
						 file_name,
						 qdss_mem[i].size);
			if (ret < 0) {
				cnss_pr_err("Fail to save QDSS data: %d\n",
					    ret);
				break;
			}
		}
	} else if ((event_data->mem_seg_len == 1) &&
		   (event_data->mem_seg[0].addr == qdss_mem[0].pa) &&
		   (event_data->mem_seg[0].size <= qdss_mem[0].size)) {
		ret = cnss_genl_send_msg(qdss_mem[0].va,
					 CNSS_GENL_MSG_TYPE_QDSS,
					 file_name,
					 event_data->mem_seg[0].size);
		if (ret < 0) {
			cnss_pr_err("Fail to save QDSS data: %d\n", ret);
			goto out;
		}
		cnss_pr_info("QDSS Data saved in /data/vendor/wifi/%s",
			     file_name);
	} else if (event_data->mem_seg_len == 2) {
		/* FW sends the 2 segments in below format, we need to send
		 * segment 0 first then segment 1
		 *
		 *  QDSS ETR Memory - 1MB
		 * +---------------------+
		 * |   segment 1 start   |
		 * |                     |
		 * |                     |
		 * |                     |
		 * |   segment 1 end     |
		 * +---------------------+
		 * |   segment 0 start   |
		 * |                     |
		 * |                     |
		 * |   segment 0 end     |
		 * +---------------------+
		 */
		if (event_data->mem_seg[1].addr != qdss_mem[0].pa) {
			cnss_pr_err("Invalid seg 0 addr 0x%llx",
				    event_data->mem_seg[1].addr);
			goto out;
		}
		if (event_data->mem_seg[0].size + event_data->mem_seg[1].size !=
		    qdss_mem[0].size) {
			cnss_pr_err("Invalid total size 0x%x 0x%x",
				    event_data->mem_seg[0].size,
				    event_data->mem_seg[1].size);
			goto out;
		}

		ret = cnss_genl_send_msg((qdss_mem[0].va +
					 event_data->mem_seg[1].size),
					 CNSS_GENL_MSG_TYPE_QDSS,
					 file_name,
					 event_data->mem_seg[0].size);
		if (ret < 0) {
			cnss_pr_err("Fail to save QDSS data 0: %d\n", ret);
			goto out;
		}
		ret = cnss_genl_send_msg((qdss_mem[0].va),
					 CNSS_GENL_MSG_TYPE_QDSS,
					 file_name,
					 event_data->mem_seg[1].size);
		if (ret < 0) {
			cnss_pr_err("Fail to save QDSS data 1: %d\n", ret);
			goto out;
		}
		cnss_pr_info("QDSS Data saved in /data/vendor/wifi/%s",
			     file_name);
	} else {
		cnss_pr_err("Inavalid mem seg len %d", event_data->mem_seg_len);
	}

out:
	kfree(data);
	return ret;
}

static int cnss_qdss_trace_free_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_bus_free_qdss_mem(plat_priv);

	return 0;
}

static int cnss_qdss_mem_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	return cnss_wlfw_send_qdss_trace_mode_req(plat_priv,
						  QMI_WLFW_QDSS_TRACE_ON_V01,
						  0);
}

static void m3_dump_open_timeout_func(struct timer_list *timer)
{
	struct m3_dump *m3_dump_data =
			from_timer(m3_dump_data, timer, open_timer);

	if (!m3_dump_data) {
		pr_err("%s: Invalid m3_dump_data from timer\n", __func__);
		return;
	}

	atomic_set(&m3_dump_data->open_timedout, 1);
	complete(&m3_dump_data->open_complete);
	pr_err("M3 dump open failed\n");
}

static void m3_dump_read_timeout_func(struct timer_list *timer)
{
	struct m3_dump *m3_dump_data =
			from_timer(m3_dump_data, timer, read_timer);

	if (!m3_dump_data) {
		pr_err("%s: Invalid m3_dump_data from timer\n", __func__);
		return;
	}

	if (m3_dump_data->task)
		send_sig(SIGKILL, m3_dump_data->task, 0);

	atomic_set(&m3_dump_data->read_timedout, 1);
	complete(&m3_dump_data->read_complete);
	pr_err("M3 dump collection failed\n");
}

static int m3_dump_open(struct inode *inode, struct file *file)
{
	struct cnss_plat_data *plat_priv = NULL;
	struct m3_dump *m3_dump_data;

	plat_priv = cnss_get_plat_priv_by_instance_id(iminor(inode));

	if (!plat_priv) {
		cnss_pr_err("%s: Failed to get plat_priv for instance_id 0x%x",
			    __func__, iminor(inode));
		return -ENODEV;
	}

	m3_dump_data = &plat_priv->m3_dump_data;
	if (m3_dump_data->file_open)
		return -EBUSY;

	del_timer_sync(&m3_dump_data->open_timer);
	if (atomic_read(&m3_dump_data->open_timedout) == 1)
		return -ENODEV;

	m3_dump_data->file_open = true;
	m3_dump_data->task = current;
	file->private_data = m3_dump_data;
	nonseekable_open(inode, file);

	init_completion(&m3_dump_data->read_complete);
	timer_setup(&m3_dump_data->read_timer, m3_dump_read_timeout_func, 0);
	mod_timer(&m3_dump_data->read_timer,
		  jiffies + msecs_to_jiffies(M3_DUMP_READ_TIMER_TIMEOUT));

	complete(&m3_dump_data->open_complete);
	return 0;
}

static ssize_t m3_dump_read(struct file *file, char __user *data, size_t len,
			    loff_t *ppos)
{
	struct m3_dump *m3_dump_data = (struct m3_dump *)file->private_data;
	char *bufp;

	if (!m3_dump_data) {
		pr_err("%s: m3_dump_data invalid", __func__);
		return -EFAULT;
	}

	bufp = (char *)m3_dump_data->dump_addr + *ppos;
	mod_timer(&m3_dump_data->read_timer,
		  jiffies + msecs_to_jiffies(M3_DUMP_READ_TIMER_TIMEOUT));

	if (*ppos + len > m3_dump_data->size)
		len = m3_dump_data->size - *ppos;

	if (copy_to_user(data, bufp, len)) {
		pr_err("%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += len;

	return len;
}

static int m3_dump_release(struct inode *inode, struct file *file)
{
	struct m3_dump *m3_dump_data = (struct m3_dump *)file->private_data;
	int dump_minor = iminor(inode);
	int dump_major = imajor(inode);

	if (!m3_dump_data) {
		pr_err("%s: m3_dump_data invalid", __func__);
		return -EFAULT;
	}

	file->private_data = NULL;
	device_destroy(m3_dump_class, MKDEV(dump_major, dump_minor));

	complete(&m3_dump_data->read_complete);
	m3_dump_data->file_open = false;
	m3_dump_data->task = 0;
	return 0;
}

static const struct file_operations m3_dump_fops = {
	.owner          = THIS_MODULE,
	.open		= m3_dump_open,
	.read		= m3_dump_read,
	.release	= m3_dump_release,
};

static int cnss_init_m3_dump_class(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (m3_dump_class) {
		cnss_pr_dbg("m3_dump_class already initialized");
		goto out;
	}

	m3_dump_major = register_chrdev(UNNAMED_MAJOR, "dump",
					&m3_dump_fops);
	if (m3_dump_major < 0) {
		cnss_pr_err("%s: Unable to allocate a major number err = %d",
			    __func__, m3_dump_major);
		ret = m3_dump_major;
		goto out;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	m3_dump_class = class_create("dump");
#else
	m3_dump_class = class_create(THIS_MODULE, "dump");
#endif
	if (IS_ERR(m3_dump_class)) {
		cnss_pr_err("%s: Unable to create class = %ld",
			    __func__, PTR_ERR(m3_dump_class));
		unregister_chrdev(m3_dump_major, "dump");
		m3_dump_major = 0;
		m3_dump_class = NULL;
		ret = -ENODEV;
	}

out:
	return ret;
}

static void cnss_deinit_m3_dump_class(void)
{
	if (m3_dump_class)
		class_destroy(m3_dump_class);

	if (m3_dump_major)
		unregister_chrdev(m3_dump_major, "dump");

	m3_dump_class = NULL;
	m3_dump_major = 0;
}

static int cnss_do_m3_dump_upload(struct cnss_plat_data *plat_priv,
				  const char *dump_file_name)
{
	int ret = 0;
	struct device *dump_dev = NULL;
	struct m3_dump *m3_dump_data = &plat_priv->m3_dump_data;

	init_completion(&m3_dump_data->open_complete);
	atomic_set(&m3_dump_data->open_timedout, 0);
	atomic_set(&m3_dump_data->read_timedout, 0);

	if (!m3_dump_class) {
		cnss_pr_err("M3 dump class not initialized.");
		return -ENODEV;
	}

	dump_dev = device_create(m3_dump_class, NULL,
				 MKDEV(m3_dump_major,
				       plat_priv->wlfw_service_instance_id),
				 NULL, dump_file_name);
	if (IS_ERR(dump_dev)) {
		ret = PTR_ERR(dump_dev);
		cnss_pr_err("%s: Unable to create device = %d",
			    __func__, ret);
		return ret;
	}

	/* This avoids race condition between the scheduled timer and the opened
	 * file discriptor during delay in user space app execution.
	 */
	timer_setup(&m3_dump_data->open_timer, m3_dump_open_timeout_func, 0);

	mod_timer(&m3_dump_data->open_timer,
		  jiffies + msecs_to_jiffies(M3_DUMP_OPEN_TIMEOUT));

	ret = wait_for_completion_timeout(&m3_dump_data->open_complete,
					  msecs_to_jiffies(
					  M3_DUMP_OPEN_COMPLETION_TIMEOUT));
	if (!ret || (atomic_read(&m3_dump_data->open_timedout) == 1)) {
		ret = -ETIMEDOUT;
		cnss_pr_err("%s: Failed to open M3 dump", __func__);
		goto dump_dev_failed;
	}

	ret = wait_for_completion_timeout(&m3_dump_data->read_complete,
					  msecs_to_jiffies(
					  M3_DUMP_COMPLETION_TIMEOUT));
	if (!ret || (atomic_read(&m3_dump_data->read_timedout) == 1)) {
		ret = -ETIMEDOUT;
		cnss_pr_err("%s: Failed to collect M3 dump", __func__);
	} else {
		/* completed before timeout */
		ret = 0;
	}

	del_timer_sync(&m3_dump_data->read_timer);
	return ret;

dump_dev_failed:
	device_destroy(m3_dump_class,
		       MKDEV(m3_dump_major,
			     plat_priv->wlfw_service_instance_id));

	return ret;
}

static int cnss_m3_dump_upload_req_hdlr(struct cnss_plat_data *plat_priv,
					void *data)
{
	struct cnss_qmi_event_m3_dump_upload_req_data *event_data = data;
	struct cnss_fw_mem *m3_mem = NULL;
	char dump_file_name[30];
	struct m3_dump *m3_dump_data = &plat_priv->m3_dump_data;
	int i, ret = 0;

	cnss_pr_dbg("%s: %d pdev_id %d addr 0x%llx size %llu",
		    __func__, __LINE__,
		    event_data->pdev_id, event_data->addr, event_data->size);

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (plat_priv->fw_mem[i].type == QMI_WLFW_MEM_M3_V01) {
			m3_mem = &plat_priv->fw_mem[i];
			break;
		}
	}

	if (!m3_mem) {
		cnss_pr_err("M3 dump memory not allocated\n");
		ret = -ENOMEM;
		goto send_resp;
	}

	if ((event_data->addr != m3_mem->pa) ||
	    (event_data->size > m3_mem->size)) {
		cnss_pr_err("Invalid M3 dump info from FW: addr: %llx, size: %lld; in plat_priv: addr:%pa size: %zd\n",
			    event_data->addr, event_data->size,
			    &m3_mem->pa, m3_mem->size);
		ret = -EINVAL;
		goto send_resp;
	}

	if (!m3_mem->va) {
		cnss_pr_err("M3 mem not remapped!\n");
		ret = -ENOMEM;
		goto send_resp;
	}

	memset(m3_dump_data, 0, sizeof(struct m3_dump));

	m3_dump_data->dump_addr = m3_mem->va;
	m3_dump_data->size = event_data->size;
	m3_dump_data->pdev_id = event_data->pdev_id;
	m3_dump_data->timestamp = ktime_to_ms(ktime_get());
	cnss_pr_dbg("%s: %d: pdev_id: %d va 0x%p size %d\n",
		    __func__, __LINE__,
		    m3_dump_data->pdev_id, m3_dump_data->dump_addr,
		    m3_dump_data->size);

	if (plat_priv->bus_type == CNSS_BUS_PCI)
		snprintf(dump_file_name, sizeof(dump_file_name),
			 "m3_dump_%s.bin", plat_priv->device_name);
	else
		snprintf(dump_file_name, sizeof(dump_file_name),
			 "m3_dump_wifi%d.bin", m3_dump_data->pdev_id);

	ret = cnss_do_m3_dump_upload(plat_priv, (const char *)dump_file_name);
	if (ret)
		cnss_pr_err("M3 Dump upload failed with ret %d", ret);

send_resp:
	cnss_wlfw_m3_dump_upload_done_send_sync(plat_priv,
						event_data->pdev_id,
						ret);

	return ret;
}

static int cnss_qdss_trace_req_data_hdlr(struct cnss_plat_data *plat_priv,
					 void *data)
{
	int ret = 0;
	struct cnss_qmi_event_qdss_trace_save_data *event_data = data;
	char file_name[CNSS_GENL_STR_LEN_MAX];

	if (!plat_priv)
		return -ENODEV;

	get_updated_qdss_trace_filename(plat_priv, event_data->file_name,
					file_name, sizeof(file_name));

	ret = cnss_wlfw_qdss_data_send_sync(plat_priv, file_name,
					    event_data->total_size);

	kfree(data);
	return ret;
}

static void cnss_driver_recovery_work(struct work_struct *work)
{
	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, recovery_work);

	if (!plat_priv) {
		pr_err("plat_priv is NULL!\n");
		return;
	}
	cnss_mount_firmware(plat_priv);
	cnss_do_recovery(plat_priv, plat_priv->reason);
}

#ifdef CONFIG_CNSS2_KERNEL_5_15
static int cnss_event_ramdump_done_handler(struct cnss_plat_data *plat_priv)
{
	cnss_hif_notifier(plat_priv, CNSS_RAMDUMP_DONE);

	return 0;
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static int cnss_event_ramdump_done_handler(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (plat_priv->bus_type == CNSS_BUS_AHB)
		ret = cnss_qca8074_notifier_nb(
				&plat_priv->modem_nb,
				CNSS_RAMDUMP_DONE, NULL);
	else
		cnss_hif_notifier(
				plat_priv,
				CNSS_RAMDUMP_DONE);
	return ret;
}
#else
static int cnss_event_ramdump_done_handler(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (plat_priv->bus_type == CNSS_BUS_AHB)
		ret = cnss_qca8074_notifier_nb(
				&plat_priv->modem_nb,
				CNSS_RAMDUMP_DONE, NULL);
	else
		ret = cnss_qcn9000_notifier_nb(
				&plat_priv->modem_nb,
				CNSS_RAMDUMP_DONE, NULL);
	return ret;
}
#endif


static void cnss_driver_event_work(struct work_struct *work)
{
	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, event_work);
	struct cnss_driver_event *event;
	unsigned long flags;
	int ret = 0;

	if (!plat_priv) {
		printk(KERN_ERR "plat_priv is NULL!\n");
		return;
	}

#ifdef CONFIG_CNSS2_PM
	cnss_pm_stay_awake(plat_priv);
#endif

	spin_lock_irqsave(&plat_priv->event_lock, flags);

	while (!list_empty(&plat_priv->event_list)) {
		event = list_first_entry(&plat_priv->event_list,
					 struct cnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);

		cnss_pr_dbg("Processing driver event: %s%s(%d), state: 0x%lx\n",
			    cnss_driver_event_to_str(event->type),
			    event->sync ? "-sync" : "", event->type,
			    plat_priv->driver_state);

		switch (event->type) {
		case CNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = cnss_wlfw_server_arrive(plat_priv, event->data);
			break;
		case CNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = cnss_wlfw_server_exit(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_REQUEST_MEM:
			ret = cnss_bus_alloc_fw_mem(plat_priv);
			if (ret)
				break;
			ret = cnss_wlfw_respond_mem_send_sync(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_MEM_READY:
			ret = cnss_fw_mem_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_READY:
			ret = cnss_fw_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START:
			ret = cnss_cold_boot_cal_start_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE:
			ret = cnss_cold_boot_cal_done_hdlr(plat_priv,
							   event->data);
			break;
		case CNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = cnss_bus_register_driver_hdlr(plat_priv,
							    event->data);
			break;
		case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = cnss_bus_unregister_driver_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_RECOVERY:
			ret = cnss_driver_recovery_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
			ret = cnss_bus_force_fw_assert_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_IDLE_RESTART:
			set_bit(CNSS_DRIVER_IDLE_RESTART,
				&plat_priv->driver_state);
			/* fall through */
		case CNSS_DRIVER_EVENT_POWER_UP:
			ret = cnss_power_up_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
			set_bit(CNSS_DRIVER_IDLE_SHUTDOWN,
				&plat_priv->driver_state);
			/* fall through */
		case CNSS_DRIVER_EVENT_POWER_DOWN:
			ret = cnss_power_down_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
			ret = cnss_qdss_trace_req_mem_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
			ret = cnss_qdss_trace_save_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
			ret = cnss_qdss_trace_free_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_QDSS_MEM_READY:
			ret = cnss_qdss_mem_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ:
			ret = cnss_m3_dump_upload_req_hdlr(plat_priv,
							   event->data);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA:
			ret = cnss_qdss_trace_req_data_hdlr(plat_priv,
							    event->data);
			break;
		case CNSS_DRIVER_EVENT_RAMDUMP_DONE:
			ret = cnss_event_ramdump_done_handler(plat_priv);
			break;
		default:
			cnss_pr_err("Invalid driver event type: %d",
				    event->type);
			kfree(event);
			spin_lock_irqsave(&plat_priv->event_lock, flags);
			continue;
		}

		spin_lock_irqsave(&plat_priv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&plat_priv->event_lock, flags);
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, flags);

#ifdef CONFIG_CNSS2_PM
	cnss_pm_relax(plat_priv);
#endif
}

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
int cnss_register_subsys(struct cnss_plat_data *plat_priv)
{
	bool multi_pd_arch = false;
	int ret = 0;
	struct cnss_subsys_info *subsys_info = &plat_priv->subsys_info;
	struct device *dev = &plat_priv->plat_dev->dev;
	struct cnss_esoc_info *esoc_info = esoc_info = &plat_priv->esoc_info;

	esoc_info->modem_notify_handler = cnss_register_notifier_cb(plat_priv);

	switch (plat_priv->bus_type) {
	case CNSS_BUS_AHB:
		multi_pd_arch = of_property_read_bool(dev->of_node,
						      "qcom,multipd_arch");
		if (multi_pd_arch &&
		    (!cnss_check_multipd_target(plat_priv)))
			/* Device is multi-pd */
			of_property_read_string(dev->of_node,
						"qcom,userpd-subsys-name",
						&subsys_info->subsys_desc.name);
		else
			subsys_info->subsys_desc.name = "qcom_q6v5_wcss";

		subsys_info->subsys_handle =
			subsystem_get(subsys_info->subsys_desc.name);
		if (!subsys_info->subsys_handle)
			ret = -EINVAL;
		else if (IS_ERR(subsys_info->subsys_handle))
			ret = PTR_ERR(subsys_info->subsys_handle);

		return ret;
	case CNSS_BUS_PCI:
		/* Handled below */
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		ret = -ENODEV;
		goto out;
	}

	if (!plat_priv->pci_dev) {
		ret = -ENODEV;
		goto out;
	}

	subsys_info->subsys_desc.name = plat_priv->device_name;

	subsys_info->subsys_desc.owner = THIS_MODULE;
	subsys_info->subsys_desc.powerup = cnss_subsys_powerup;
	subsys_info->subsys_desc.shutdown = cnss_subsys_shutdown;
	subsys_info->subsys_desc.ramdump = cnss_subsys_ramdump;
	subsys_info->subsys_desc.crash_shutdown = cnss_subsys_crash_shutdown;
	subsys_info->subsys_desc.dev = &plat_priv->plat_dev->dev;

	subsys_info->subsys_device = subsys_register(&subsys_info->subsys_desc);
	if (IS_ERR(subsys_info->subsys_device)) {
		ret = PTR_ERR(subsys_info->subsys_device);
		cnss_pr_err("Failed to register subsys, err = %d\n", ret);
		goto out;
	}
	subsys_info->subsys_handle =
		subsystem_get(subsys_info->subsys_desc.name);
	if (!subsys_info->subsys_handle) {
		cnss_pr_err("Failed to get subsys_handle!\n");
		ret = -EINVAL;
		goto unregister_subsys;
	} else if (IS_ERR(subsys_info->subsys_handle)) {
		ret = PTR_ERR(subsys_info->subsys_handle);
		cnss_pr_err("Failed to do subsystem_get, err = %d\n", ret);
		goto unregister_subsys;
	}
	return 0;

unregister_subsys:
	CNSS_ASSERT(0);
	subsys_unregister(subsys_info->subsys_device);
out:
	return ret;
}
#else
int cnss_register_subsys(struct cnss_plat_data *plat_priv)
{
#ifndef CONFIG_CNSS2_KERNEL_5_15
	struct cnss_subsys_info *subsys_info = &plat_priv->subsys_info;
	struct device *dev = &plat_priv->plat_dev->dev, *rproc_dev;
	struct rproc *rproc_handle, *rproc_parent_pd;
	phandle rproc_node;
#endif
	int ret = 0;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_AHB:
#ifndef CONFIG_CNSS2_KERNEL_5_15
		if (!plat_priv->rproc_handle) {
			/* rproc_handle for AHB targets are allocated inside
			 * remoteproc driver and is never freed.
			 * So get the rproc_handle once during first wifi load
			 * and reuse it for every subsequent loads
			 */
			if (of_property_read_u32(dev->of_node, "qcom,rproc",
						 &rproc_node)) {
				return -ENODEV;
			}

			plat_priv->rproc_handle =
				rproc_get_by_phandle(rproc_node);
			if (IS_ERR_OR_NULL(plat_priv->rproc_handle)) {
				cnss_pr_err("%s: Failed to get rproc handle %ld for device %s\n",
					    __func__,
					    PTR_ERR(plat_priv->rproc_handle),
					    plat_priv->device_name);
				return -EINVAL;
			}

			rproc_handle = plat_priv->rproc_handle;

			/* Register the Rootpd for the first userpd
			 * which is being probed. Skip if already registered.
			 */
			if ((!rproc_rootpd) &&
			    (plat_priv->recovery_type == CNSS_SYNC_RECOVERY)) {
				/* Parent node of userpd will be always be
				 * either rootpd or textpd.
				 */
				rproc_dev = rproc_handle->dev.parent;
				rproc_parent_pd = dev_get_drvdata(rproc_dev->parent);
					/* The device tree structure is represented as,
					 * Rootpd ->
					 *      Textpd (For QCN6432 RDP only) ->
					 *              Userpd_1
					 *              ...
					 *              Userpd_n
					 * All the Rproc PD nodes have the
					 * string "remoteproc" in their name.
					 * Parent of rootpd node doesn't have
					 * the string in its name. Traverse to
					 * the first parent of userpd to get
					 * textpd or rootpd. If there is node
					 * above the first parent then that
					 * parent is the textpd and the parent
					 * of textpd is rootpd.
					 */
				if (rproc_parent_pd) {
					if (strstr(rproc_parent_pd->name, RPROC_ROOTPD_NAME)) {
						rproc_rootpd = rproc_parent_pd;
						rproc_dev = rproc_parent_pd->dev.parent;
						rproc_parent_pd = dev_get_drvdata(rproc_dev->parent);
						if (rproc_parent_pd) {
							if (strstr(rproc_parent_pd->name, RPROC_ROOTPD_NAME)) {
								rproc_textpd = rproc_rootpd;
								cnss_pr_info("%s: Rproc text pd handle %s\n",
									__func__,
									rproc_textpd->name);
								rproc_rootpd = rproc_parent_pd;
							}
						}
						cnss_pr_info("%s: Rproc root pd handle %s\n",
								__func__,
								rproc_rootpd->name);
					}
				}
			}
		}

		rproc_handle = plat_priv->rproc_handle;
		subsys_info->subsys_desc.name = rproc_handle->name;
#endif
		break;
	case CNSS_BUS_PCI:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		ret = cnss_hif_power_up(plat_priv);
		if (ret != 0) {
			cnss_pr_err("%s: cnss_hif_power_up failed(%d)\n",
				    __func__, ret);
		}
#else
		if (!plat_priv->rproc_handle) {
			/* Should never happen as rproc_handle is allocated
			 * during cnss_probe for PCI targets
			 */
			cnss_pr_err("Invalid rproc_handle for %s",
				    plat_priv->device_name);
			return -ENODEV;
		}
		subsys_info->subsys_desc.name = plat_priv->device_name;
#endif
		break;
	default:
		cnss_pr_err("%s: Invalid bus type for device 0x%lx\n",
			    __func__, plat_priv->device_id);
		return -ENODEV;

	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	if (plat_priv->bus_type == CNSS_BUS_PCI)
		return ret;
#endif
	/* plat_priv->rproc_handle is never freed but subsys_handle is set here
	 * and is reset to NULL everytime target is shutdown.
	 */
#ifndef CONFIG_CNSS2_KERNEL_5_15
	subsys_info->subsys_handle = plat_priv->rproc_handle;
	plat_priv->esoc_info.modem_notify_handler =
		cnss_register_notifier_cb(plat_priv);

	cnss_mount_firmware(plat_priv);
	ret = rproc_boot(subsys_info->subsys_handle);
	if (ret) {
		cnss_pr_err("%s: Failed to boot device %s (%d)\n",
				__func__, plat_priv->device_name, ret);
		CNSS_ASSERT(0);
		cnss_unregister_notifier_cb(plat_priv);
	}
#endif

	return ret;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
void cnss_unregister_subsys(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;

	if (plat_priv->bus_type == CNSS_BUS_AHB)
		return;

	subsys_info = &plat_priv->subsys_info;

#ifdef CONFIG_CNSS2_KERNEL_SSR_FRAMEWORK
	if (subsys_info->subsys_handle)
		subsystem_put(subsys_info->subsys_handle);

	subsys_unregister(subsys_info->subsys_device);
	subsys_info->subsys_device = NULL;
#else /* CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK */
	if (subsys_info->subsys_handle)
		rproc_shutdown(subsys_info->subsys_handle);
#endif
	subsys_info->subsys_handle = NULL;
}
#endif

static u32 cnss_get_dump_desc_size(struct cnss_plat_data *plat_priv)
{
	u32 descriptor_size = 0;
	u32 segment_len = CNSS_MHI_SEG_LEN;
	u32 wlan_sram_size = plat_priv->ramdump_info_v2.ramdump_size;

#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
	segment_len = SZ_4K;
#else
	struct pci_dev *pci_dev = plat_priv->pci_dev;

	of_property_read_u32(pci_dev->dev.of_node, "qti,rddm-seg-len",
			     &segment_len);
#endif
	descriptor_size = (((wlan_sram_size / segment_len) +
			    CNSS_DUMP_DESC_TOLERANCE) *
			    sizeof(struct cnss_dump_seg));

	return descriptor_size;
}

#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
int cnss_register_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;
	struct cnss_dump_data *dump_data = dump_data = &info_v2->dump_data;
	struct device *dev = &plat_priv->plat_dev->dev;
	int gfp = GFP_KERNEL;
	u32 ramdump_size = 0;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0)
		info_v2->ramdump_size = ramdump_size;

	cnss_pr_dbg("Ramdump size 0x%lx\n", info_v2->ramdump_size);

	/* Dump data allocated during previous register needs to be freed
	  * before allocating again
	  */
	info_v2->ramdump_dev = NULL;
	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	info_v2->dump_data_valid = false;

	info_v2->dump_data_vaddr = kzalloc(cnss_get_dump_desc_size(plat_priv),
					   gfp);
	if (!info_v2->dump_data_vaddr)
		return -ENOMEM;

	dump_data->paddr = virt_to_phys(info_v2->dump_data_vaddr);
	dump_data->version = CNSS_DUMP_FORMAT_VER_V2;
	dump_data->magic = CNSS_DUMP_MAGIC_VER_V2;
	dump_data->seg_version = CNSS_DUMP_SEG_VER;
	strlcpy(dump_data->name, CNSS_DUMP_NAME,
		sizeof(dump_data->name));

	info_v2->ramdump_dev = dev;

	return 0;
}

void cnss_unregister_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;

	info_v2->ramdump_dev = NULL;
	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	info_v2->dump_data_valid = false;
}
#else /* !CONFIG_CNSS2_KERNEL_5_15 */
static int cnss_init_dump_entry(struct cnss_plat_data *plat_priv)
{
#ifdef CONFIG_QTI_MEMORY_DUMP_V2
	struct cnss_ramdump_info *ramdump_info;
	struct msm_dump_entry dump_entry;

	ramdump_info = &plat_priv->ramdump_info;
	ramdump_info->dump_data.addr = ramdump_info->ramdump_pa;
	ramdump_info->dump_data.len = ramdump_info->ramdump_size;
	ramdump_info->dump_data.version = CNSS_DUMP_FORMAT_VER;
	ramdump_info->dump_data.magic = CNSS_DUMP_MAGIC_VER_V2;
	strlcpy(ramdump_info->dump_data.name, CNSS_DUMP_NAME,
		sizeof(ramdump_info->dump_data.name));
	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(&ramdump_info->dump_data);
#endif

#ifdef NOMINIDUMP
	return msm_dump_data_register_nominidump(MSM_DUMP_TABLE_APPS,
						&dump_entry);
#else
	return 0;
#endif
}

static int cnss_register_ramdump_v1(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_subsys_info *subsys_info;
	struct cnss_ramdump_info *ramdump_info;
	u32 ramdump_size = 0;

	dev = &plat_priv->plat_dev->dev;
	subsys_info = &plat_priv->subsys_info;
	ramdump_info = &plat_priv->ramdump_info;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0) {
		ramdump_info->ramdump_va =
			dma_alloc_coherent(dev, ramdump_size,
					   &ramdump_info->ramdump_pa,
					   GFP_KERNEL);

		if (ramdump_info->ramdump_va)
			ramdump_info->ramdump_size = ramdump_size;
	}

	cnss_pr_dbg("ramdump va: %pK, pa: %pa\n",
		    ramdump_info->ramdump_va, &ramdump_info->ramdump_pa);

	if (ramdump_info->ramdump_size == 0) {
		cnss_pr_info("Ramdump will not be collected");
		goto out;
	}

	ret = cnss_init_dump_entry(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to setup dump table, err = %d\n", ret);
		goto free_ramdump;
	}

	ramdump_info->ramdump_dev =
		create_ramdump_device(subsys_info->subsys_desc.name,
				      subsys_info->subsys_desc.dev);
	if (!ramdump_info->ramdump_dev) {
		cnss_pr_err("Failed to create ramdump device!");
		ret = -ENOMEM;
		goto free_ramdump;
	}

	return 0;
free_ramdump:
	dma_free_coherent(dev, ramdump_info->ramdump_size,
			  ramdump_info->ramdump_va, ramdump_info->ramdump_pa);
out:
	return ret;
}

static void cnss_unregister_ramdump_v1(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_ramdump_info *ramdump_info;

	dev = &plat_priv->plat_dev->dev;
	ramdump_info = &plat_priv->ramdump_info;

	if (ramdump_info->ramdump_dev)
		destroy_ramdump_device(ramdump_info->ramdump_dev);

	if (ramdump_info->ramdump_va)
		dma_free_coherent(dev, ramdump_info->ramdump_size,
				  ramdump_info->ramdump_va,
				  ramdump_info->ramdump_pa);
}

#ifdef CONFIG_QTI_MEMORY_DUMP_V2
static int cnss_set_msm_dump_table(struct cnss_dump_data *dump_data)
{
	struct msm_dump_entry dump_entry;
	int ret = 0;

	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(dump_data);
#ifdef NOMINIDUMP
	ret = msm_dump_data_register_nominidump(MSM_DUMP_TABLE_APPS,
						&dump_entry);
#endif
	return ret;
}
#else
static int cnss_set_msm_dump_table(struct cnss_dump_data *dump_data)
{
	return 0;
}
#endif

static int cnss_register_ramdump_v2(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_subsys_info *subsys_info;
	struct cnss_ramdump_info_v2 *info_v2;
	struct cnss_dump_data *dump_data;
	struct device *dev = &plat_priv->plat_dev->dev;
#ifdef CONFIG_CNSS2_KERNEL_IPQ
	char ramdump_dev_name[CNSS_RAMDUMP_FILE_NAME_MAX_LEN] = {0};
#endif
	u32 ramdump_size = 0;

	subsys_info = &plat_priv->subsys_info;
	info_v2 = &plat_priv->ramdump_info_v2;
	dump_data = &info_v2->dump_data;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0)
		info_v2->ramdump_size = ramdump_size;

	cnss_pr_dbg("Ramdump size 0x%lx\n", info_v2->ramdump_size);

	/* Dump data allocated during previous register needs to be freed
	 * before allocating again
	 */
	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	info_v2->dump_data_valid = false;

	info_v2->dump_data_vaddr = kzalloc(cnss_get_dump_desc_size(plat_priv),
					   GFP_KERNEL);
	if (!info_v2->dump_data_vaddr)
		return -ENOMEM;

	dump_data->paddr = virt_to_phys(info_v2->dump_data_vaddr);
	dump_data->version = CNSS_DUMP_FORMAT_VER_V2;
	dump_data->magic = CNSS_DUMP_MAGIC_VER_V2;
	dump_data->seg_version = CNSS_DUMP_SEG_VER_V2;
	strlcpy(dump_data->name, CNSS_DUMP_NAME,
		sizeof(dump_data->name));

	ret = cnss_set_msm_dump_table(dump_data);
	if (ret) {
		cnss_pr_err("Failed to setup dump table, err = %d\n", ret);
		goto free_ramdump;
	}
#ifdef CONFIG_CNSS2_KERNEL_IPQ
	snprintf(ramdump_dev_name, sizeof(ramdump_dev_name), "ramdump_%s",
		 plat_priv->device_name);
	info_v2->ramdump_dev =
		create_ramdump_device((const char *)ramdump_dev_name,
				      subsys_info->subsys_desc.dev);
#else
	/*
	 * WAR to avoid memory corruption where freed memory of ramdump_device
	 * is getting accessed inside kernel and getting crashed.
	 *
	 * ramdump_device created is not freed during cnss_pci_remove and the
	 * same will be used the next time cnss_pci_probe is called
	 */
	if (plat_priv->rd_dev_present) {
		cnss_pr_info("Skipping rd_dev creation for %s ",
			      plat_priv->device_name);
		return 0;
	}
	info_v2->ramdump_dev =
		create_ramdump_device(subsys_info->subsys_desc.name,
				      subsys_info->subsys_desc.dev);
	plat_priv->rd_dev_present = true;
#endif
	if (!info_v2->ramdump_dev) {
		cnss_pr_err("Failed to create ramdump device!\n");
		ret = -ENOMEM;
		goto free_ramdump;
	}

	return 0;

free_ramdump:
	kfree(info_v2->dump_data_vaddr);
	plat_priv->rd_dev_present = false;
	info_v2->dump_data_vaddr = NULL;
	return ret;
}

static void cnss_unregister_ramdump_v2(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2;

	info_v2 = &plat_priv->ramdump_info_v2;

	if (info_v2->ramdump_dev)
		destroy_ramdump_device(info_v2->ramdump_dev);

	/* Freeing the dump data here causes loss of valid dump data in the
	 * below scenario
	 *  1. wifi down is in progress and target asserts after sending
	 *     mode OFF message
	 *  2. MHI notifies target assert, RDDM dump collection happens and
	 *     dump_data_vaddr has valid dump data
	 *  3. cnss_pci_remove is called as part of wifi down and dump data
	 *     with valid contents is now freed.
	 *
	 * To Avoid this scenario, skip freeing dump_data_vaddr here and free
	 * as part of cnss_register_ramdump_v2
	 *
	 * kfree(info_v2->dump_data_vaddr);
	 * info_v2->dump_data_vaddr = NULL;
	 * info_v2->dump_data_valid = false;
	 */
}

int cnss_register_ramdump(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_register_ramdump_v1(plat_priv);
		break;
	case QCN9000_EMULATION_DEVICE_ID:
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		ret = cnss_register_ramdump_v2(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		ret = -ENODEV;
		break;
	}
	return ret;
}

void cnss_unregister_ramdump(struct cnss_plat_data *plat_priv)
{
	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		cnss_unregister_ramdump_v1(plat_priv);
		break;
	case QCN9000_EMULATION_DEVICE_ID:
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		cnss_unregister_ramdump_v2(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		break;
	}
}
#endif /* !CONFIG_CNSS2_KERNEL_5_15 */

#ifdef CONFIG_CNSS2_PM
static int cnss_register_bus_scale(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_bus_bw_info *bus_bw_info;

	bus_bw_info = &plat_priv->bus_bw_info;

	bus_bw_info->bus_scale_table =
		msm_bus_cl_get_pdata(plat_priv->plat_dev);
	if (bus_bw_info->bus_scale_table)  {
		bus_bw_info->bus_client =
			msm_bus_scale_register_client
			(bus_bw_info->bus_scale_table);
		if (!bus_bw_info->bus_client) {
			cnss_pr_err("Failed to register bus scale client!\n");
			ret = -EINVAL;
			goto out;
		}
	}

	return 0;
out:
	return ret;
}

static void cnss_unregister_bus_scale(struct cnss_plat_data *plat_priv)
{
	struct cnss_bus_bw_info *bus_bw_info;

	bus_bw_info = &plat_priv->bus_bw_info;

	if (bus_bw_info->bus_client)
		msm_bus_scale_unregister_client(bus_bw_info->bus_client);
}
#endif

void cnss_config_param_update_cb(uint32_t instance_id,
			     enum cnss_plat_ipc_qmi_config_param_type_v01 param,
			     uint64_t value)
{
	struct cnss_plat_data *plat_priv = NULL;

	cnss_pr_info("%s: Instance ID: 0x%x Param %d Value: %llu\n", __func__,
		     instance_id, param, value);

	plat_priv = cnss_get_plat_priv_by_instance_id(instance_id);

	if (!plat_priv) {
		cnss_pr_err("Failed to get plat_priv for instance_id 0x%x\n",
			    instance_id);
		return;
	}

	switch (param) {
	case CNSS_PLAT_IPC_PARAM_TYPE_DAEMON_SUPPORT_V01:
		plat_priv->daemon_support = value;
		cnss_pr_info("Setting daemon_support=%llu for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_PLAT_IPC_PARAM_TYPE_COLD_BOOT_SUPPORT_V01:
		plat_priv->cold_boot_support = value;
		cnss_pr_info("Setting cold_boot_support=%llu for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_PLAT_IPC_PARAM_TYPE_HDS_SUPPORT_V01:
		plat_priv->hds_support = value;
		cnss_pr_info("Setting hds_support=%llu for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_PLAT_IPC_PARAM_TYPE_REGDB_SUPPORT_V01:
		plat_priv->regdb_support = value;
		cnss_pr_info("Setting regdb_support=%llu for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_PLAT_IPC_PARAM_TYPE_QDSS_SUPPORT_V01:
		plat_priv->qdss_support = value;
		cnss_pr_info("Setting qdss_support=%llu for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_PLAT_IPC_PARAM_TYPE_QDSS_START_V01:
		plat_priv->qdss_etr_sg_mode = value;
		cnss_pr_info("Starting QDSS for %s", plat_priv->device_name);
		cnss_wlfw_qdss_dnld_send_sync(plat_priv);
		break;
	case CNSS_PLAT_IPC_PARAM_TYPE_QDSS_STOP_V01:
		cnss_pr_info("Stopping QDSS for %s", plat_priv->device_name);
		cnss_wlfw_send_qdss_trace_mode_req(plat_priv,
						   QMI_WLFW_QDSS_TRACE_OFF_V01,
						   value);
		break;
	default:
		cnss_pr_err("Unknown config param type %d\n", param);
		break;
	}
}

void cnss_daemon_connection_update_cb(void *cb_ctx, bool status)
{
	int i;
	struct cnss_plat_data *plat_priv = NULL;

	cnss_pr_dbg("%s: Connection status %u\n", __func__, status);

	for (i = 0; i < plat_env_index; i++) {
		if (status)
			set_bit(CNSS_DAEMON_CONNECTED,
				&plat_env[i]->driver_state);
		else
			clear_bit(CNSS_DAEMON_CONNECTED,
				  &plat_env[i]->driver_state);
	}
}

static ssize_t fs_ready_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int fs_ready = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	if (sscanf(buf, "%du", &fs_ready) != 1)
		return -EINVAL;

	cnss_pr_dbg("File system is ready, fs_ready is %d, count is %zu\n",
		    fs_ready, count);

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks)) {
		printk(KERN_INFO "QMI is bypassed.\n");
		return count;
	}

	if (!plat_priv) {
		printk(KERN_ERR "plat_priv is NULL!\n");
		return count;
	}

	switch (plat_priv->device_id) {
	case QCN9000_EMULATION_DEVICE_ID:
	case QCN9000_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Not supported for device ID 0x%lx\n",
			    plat_priv->device_id);
		return count;
	}

	if (fs_ready == FILE_SYSTEM_READY) {
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START,
				       CNSS_EVENT_SYNC, NULL);
	}

	return count;
}

static DEVICE_ATTR_WO(fs_ready);

static int cnss_create_sysfs(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = device_create_file(&plat_priv->plat_dev->dev, &dev_attr_fs_ready);
	if (ret) {
		cnss_pr_err("Failed to create device file, err = %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static void cnss_remove_sysfs(struct cnss_plat_data *plat_priv)
{
	device_remove_file(&plat_priv->plat_dev->dev, &dev_attr_fs_ready);
}

static int cnss_event_work_init(struct cnss_plat_data *plat_priv)
{
	spin_lock_init(&plat_priv->event_lock);
	plat_priv->event_wq = alloc_workqueue("cnss_driver_event",
					      WQ_UNBOUND, 1);
	if (!plat_priv->event_wq) {
		cnss_pr_err("Failed to create event workqueue!\n");
		return -EFAULT;
	}

	INIT_WORK(&plat_priv->event_work, cnss_driver_event_work);
	INIT_LIST_HEAD(&plat_priv->event_list);

	return 0;
}

static int cnss_recovery_work_init(struct cnss_plat_data *plat_priv)
{
	plat_priv->recovery_wq = alloc_workqueue("cnss_driver_recovery",
					      WQ_UNBOUND, 1);
	if (!plat_priv->recovery_wq) {
		cnss_pr_err("Failed to create event workqueue!\n");
		return -EFAULT;
	}

	INIT_WORK(&plat_priv->recovery_work, cnss_driver_recovery_work);

	return 0;
}

static void cnss_event_work_deinit(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->event_wq)
		destroy_workqueue(plat_priv->event_wq);
}

static void cnss_recovery_work_deinit(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->recovery_wq)
		destroy_workqueue(plat_priv->recovery_wq);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static void cnss_report_crash_work(struct work_struct *work)
{
	int index;
	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, crash_work);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	index = cnss_get_plat_env_index_from_plat_priv(plat_priv);
	if (index < 0) {
		cnss_pr_err("Invalid plat_env index for %s",
			    plat_priv->device_name);
		return;
	}
	cnss_hif_shutdown(plat_priv);
	cnss_hif_notifier(plat_priv, CNSS_RAMDUMP_NOTIFICATION);
	cnss_bus_dev_ramdump(plat_priv);
	cnss_hif_power_up(plat_priv);
}
#endif

#ifdef CONFIG_CNSS2_KERNEL_5_15
static void __cnss_subsystem_get_wrapper(struct cnss_plat_data *plat_priv)
{
	__cnss_hif_get(plat_priv);
}
static void __cnss_subsystem_put_wrapper(struct cnss_plat_data *plat_priv)
{
	__cnss_hif_put(plat_priv);
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static void __cnss_subsystem_get_wrapper(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->bus_type == CNSS_BUS_PCI)
		__cnss_hif_get(plat_priv);
	else
		__cnss_subsystem_get(plat_priv);
}
static void __cnss_subsystem_put_wrapper(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->bus_type == CNSS_BUS_PCI)
		__cnss_hif_put(plat_priv);
	else
		__cnss_subsystem_put(plat_priv);
}
#else
static void __cnss_subsystem_get_wrapper(struct cnss_plat_data *plat_priv)
{
		__cnss_subsystem_get(plat_priv);
}
static void __cnss_subsystem_put_wrapper(struct cnss_plat_data *plat_priv)
{
		__cnss_subsystem_put(plat_priv);
}
#endif

static void cnss_driver_cal_work(struct work_struct *work)
{
	int ret, index, count = 0;
	u64 probe_time = 0;
	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, cal_work);
	struct cnss_plat_data *prev_plat_priv;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	index = cnss_get_plat_env_index_from_plat_priv(plat_priv);
	if (index < 0) {
		cnss_pr_err("Invalid plat_env index for %s",
			    plat_priv->device_name);
		return;
	}

	cnss_wait_for_cold_boot_cal_done(plat_priv);

	ret = cnss_wlfw_wlan_mode_send_sync(plat_priv, CNSS_OFF);
	if (ret) {
		cnss_pr_err("Failed to send Mode OFF for %s. Ret: %d",
			    plat_priv->device_name, ret);
		return;
	}

	if (soft_switch) {
		plat_priv->cal_in_progress = false;
		plat_priv->driver_ops->probe(
				(struct pci_dev *)plat_priv->plat_dev,
				(const struct pci_device_id *)
				plat_priv->plat_dev_id);
	} else {
		__cnss_subsystem_put_wrapper(plat_priv);
		plat_priv->cal_in_progress = false;

		/* Temporary change to preserve probe order */
		if (index > 0) {
			prev_plat_priv = plat_env[index - 1];
			probe_time = jiffies;
			while (prev_plat_priv->driver_status !=
							CNSS_INITIALIZED) {
				msleep(FW_READY_DELAY);
				if (count++ > probe_timeout * 10) {
					cnss_pr_err("CNSS Driver probe timed out %u ms\n",
					jiffies_to_msecs(jiffies - probe_time));
					CNSS_ASSERT(0);
				}
			}
			cnss_pr_info("Previous target probe took %u ms\n",
				     jiffies_to_msecs(jiffies - probe_time));
		}
		__cnss_subsystem_get_wrapper(plat_priv);
	}

	plat_priv->driver_status = CNSS_INITIALIZED;
	atomic_dec(&cal_in_progress_count);
}

static void cnss_cal_work_deinit(struct cnss_plat_data *plat_priv)
{
	cancel_work_sync(&plat_priv->cal_work);
}

static void cnss_cal_work_init(struct cnss_plat_data *plat_priv)
{
	INIT_WORK(&plat_priv->cal_work, cnss_driver_cal_work);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static void cnss_crash_work_deinit(struct cnss_plat_data *plat_priv)
{
	cancel_work_sync(&plat_priv->crash_work);
}

static void cnss_crash_work_init(struct cnss_plat_data *plat_priv)
{
	INIT_WORK(&plat_priv->crash_work, cnss_report_crash_work);
}
#endif

static int cnss_misc_init(struct cnss_plat_data *plat_priv)
{
	int ret;

	timer_setup(&plat_priv->fw_boot_timer,
		    cnss_bus_fw_boot_timeout_hdlr, 0);
#ifdef CONFIG_CNSS2_PM
	register_pm_notifier(&cnss_pm_notifier);
#endif

	ret = device_init_wakeup(&plat_priv->plat_dev->dev, true);
	if (ret)
		cnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			    ret);

	init_completion(&plat_priv->power_up_complete);
	init_completion(&plat_priv->cal_complete);
	init_completion(&plat_priv->rddm_complete);
	init_completion(&plat_priv->recovery_complete);
	init_completion(&plat_priv->soc_reset_request_complete);
	mutex_init(&plat_priv->dev_lock);

	return 0;
}

static void cnss_misc_deinit(struct cnss_plat_data *plat_priv)
{
	complete_all(&plat_priv->soc_reset_request_complete);
	complete_all(&plat_priv->recovery_complete);
	complete_all(&plat_priv->rddm_complete);
	complete_all(&plat_priv->cal_complete);
	complete_all(&plat_priv->power_up_complete);
	device_init_wakeup(&plat_priv->plat_dev->dev, false);
#ifdef CONFIG_CNSS2_PM
	unregister_pm_notifier(&cnss_pm_notifier);
#endif
	del_timer(&plat_priv->fw_boot_timer);
}

static void cnss_init_control_params(struct cnss_plat_data *plat_priv)
{
	plat_priv->ctrl_params.quirks = CNSS_QUIRKS_DEFAULT;
	plat_priv->ctrl_params.mhi_timeout =
		CNSS_MHI_TIMEOUT_DEFAULT * timeout_factor;

	if (qmi_timeout)
		plat_priv->ctrl_params.qmi_timeout =
			qmi_timeout * timeout_factor;
	else
		plat_priv->ctrl_params.qmi_timeout =
			CNSS_QMI_TIMEOUT_DEFAULT * timeout_factor;

	plat_priv->ctrl_params.bdf_type = 0;
	plat_priv->ctrl_params.time_sync_period = CNSS_TIME_SYNC_PERIOD_DEFAULT;
}

static const struct platform_device_id cnss_platform_id_table[] = {
	{ .name = "qca6174", .driver_data = QCA6174_DEVICE_ID, },
	{ .name = "qcn9000", .driver_data = QCN9000_DEVICE_ID, },
	{ .name = "qca8074", .driver_data = QCA8074_DEVICE_ID, },
	{ .name = "qca8074v2", .driver_data = QCA8074V2_DEVICE_ID, },
	{ .name = "qca6018", .driver_data = QCA6018_DEVICE_ID, },
	{ .name = "qca5018", .driver_data = QCA5018_DEVICE_ID, },
	{ .name = "qcn6122", .driver_data = QCN6122_DEVICE_ID, },
	{ .name = "qcn9224", .driver_data = QCN9224_DEVICE_ID, },
	{ .name = "qca9574", .driver_data = QCA9574_DEVICE_ID, },
	{ .name = "qca5332", .driver_data = QCA5332_DEVICE_ID, },
	{ .name = "qcn9160", .driver_data = QCN9160_DEVICE_ID, },
	{ .name = "qcn6432", .driver_data = QCN6432_DEVICE_ID, },
	{ .name = "qca5424", .driver_data = QCA5424_DEVICE_ID, },
};

static const struct of_device_id cnss_of_match_table[] = {
	{
		.compatible = "qcom,cnss",
		.data = (void *)&cnss_platform_id_table[0]},
	{
		.compatible = "qcom,cnss-qcn9000",
		.data = (void *)&cnss_platform_id_table[1]},
	{
		.compatible = "qcom,cnss-qca8074",
		.data = (void *)&cnss_platform_id_table[2]},
	{
		.compatible = "qcom,cnss-qca8074v2",
		.data = (void *)&cnss_platform_id_table[3]},
	{
		.compatible = "qcom,cnss-qca6018",
		.data = (void *)&cnss_platform_id_table[4]},
	{
		.compatible = "qcom,cnss-qca5018",
		.data = (void *)&cnss_platform_id_table[5]},
	{
		.compatible = "qcom,cnss-qcn6122",
		.data = (void *)&cnss_platform_id_table[6]},
	{
		.compatible = "qcom,cnss-qcn9224",
		.data = (void *)&cnss_platform_id_table[7]},
	{
		.compatible = "qcom,cnss-qca9574",
		.data = (void *)&cnss_platform_id_table[8]},
	{
		.compatible = "qcom,cnss-qca5332",
		.data = (void *)&cnss_platform_id_table[9]},
	{
		.compatible = "qcom,cnss-qcn9160",
		.data = (void *)&cnss_platform_id_table[10]},
	{
		.compatible = "qcom,cnss-qcn6432",
		.data = (void *)&cnss_platform_id_table[11]},
	{
		.compatible = "qcom,cnss-qca5424",
		.data = (void *)&cnss_platform_id_table[12]},
	{ },
};
MODULE_DEVICE_TABLE(of, cnss_of_match_table);

static const char *
cnss_module_param_feature_to_str(enum cnss_module_param_feature feature)
{
	switch (feature) {
	case CALDATA:
		return "caldata";
	case REGDB:
		return "regdb";
	default:
		return "unknown";
	}
}

static void
cnss_set_mod_param_feature_support(struct cnss_plat_data *plat_priv,
				   enum cnss_module_param_feature feature)
{
	int bmap = 0x0;
	bool *ptr;
	const char *fname = cnss_module_param_feature_to_str(feature);

	switch (feature) {
	case CALDATA:
		bmap = disable_caldata_bmap;
		/* By default caldata support is enabled */
		plat_priv->caldata_support = 1;
		ptr = &plat_priv->caldata_support;
		break;
	case REGDB:
		bmap = disable_regdb_bmap;
		/* By default regdb support should be enabled.
		 * But for now, it is disabled through module param.
		 */
		plat_priv->regdb_support = 1;
		ptr = &plat_priv->regdb_support;
		break;
	default:
		cnss_pr_err("%s: Unknown feature %u %s", __func__, feature,
			    fname);
		return;
	}

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		if (bmap & SKIP_INTEGRATED) {
			cnss_pr_info("Disabling %s support for %s", fname,
				     plat_priv->device_name);
			*ptr = 0;
		}
		break;
	case QCN9000_DEVICE_ID:
		if ((plat_priv->qrtr_node_id == QCN9000_0 &&
		     (bmap & SKIP_PCI_0)) ||
		    (plat_priv->qrtr_node_id == QCN9000_1 &&
		     (bmap & SKIP_PCI_1)) ||
		    (plat_priv->qrtr_node_id == QCN9000_2 &&
		     (bmap & SKIP_PCI_2)) ||
		    (plat_priv->qrtr_node_id == QCN9000_3 &&
		     (bmap & SKIP_PCI_3))) {
			*ptr = 0;
			cnss_pr_info("Disabling %s support for %s", fname,
				     plat_priv->device_name);
		}
		break;
	case QCN9160_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCN6432_DEVICE_ID:
		if ((plat_priv->userpd_id == USERPD_0 &&
		     (bmap & SKIP_PCI_0)) ||
		    (plat_priv->userpd_id == USERPD_1 &&
		     (bmap & SKIP_PCI_1))) {
			*ptr = 0;
			cnss_pr_info("Disabling %s support for %s", fname,
				     plat_priv->device_name);
		}
		break;
	case QCN9224_DEVICE_ID:
		if ((plat_priv->qrtr_node_id == QCN9224_0 &&
		     (bmap & SKIP_PCI_0)) ||
		    (plat_priv->qrtr_node_id == QCN9224_1 &&
		     (bmap & SKIP_PCI_1)) ||
		    (plat_priv->qrtr_node_id == QCN9224_2 &&
		     (bmap & SKIP_PCI_2)) ||
		    (plat_priv->qrtr_node_id == QCN9224_3 &&
		     (bmap & SKIP_PCI_3))) {
			*ptr = 0;
			cnss_pr_info("Disabling %s support for %s", fname,
				     plat_priv->device_name);
		}
		break;
	default:
		cnss_pr_err("%s: UNKNOWN DEVICE ID", __func__);
		break;
	}
	return;
}

static int cnss_set_device_name(struct cnss_plat_data *plat_priv)
{

	switch (plat_priv->device_id) {
	case QCN9000_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCN9000_PCI%d", plat_priv->pci_slot_id);
		break;
	case QCN9224_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCN9224_PCI%d", plat_priv->pci_slot_id);
		break;
	case QCA8074_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA8074");
		break;
	case QCA8074V2_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA8074v2");
		break;
	case QCA6018_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA6018");
		break;
	case QCA5018_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA5018");
		break;
	case QCN6122_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCN6122_%d", plat_priv->userpd_id);
		break;
	case QCN9160_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCN9160_%d", plat_priv->userpd_id);
		break;
	case QCA9574_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA9574");
		break;
	case QCA5332_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA5332");
		break;
	case QCN6432_DEVICE_ID:
	snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
		"QCN6432_%d", plat_priv->userpd_id);
		break;
	case QCA5424_DEVICE_ID:
		snprintf(plat_priv->device_name, sizeof(plat_priv->device_name),
			 "QCA5424");
		break;
	default:
		cnss_pr_err("No such device id 0x%lx\n", plat_priv->device_id);
		return -ENODEV;
	}

	return 0;
}

void cnss_update_platform_feature_support(u8 type, u32 instance_id, u32 value)
{
	struct cnss_plat_data *plat_priv = NULL;

	plat_priv = cnss_get_plat_priv_by_instance_id(instance_id);
	if (!plat_priv) {
		cnss_pr_err("Failed to get plat_priv for instance_id 0x%x\n",
			    instance_id);
		return;
	}

	switch (type) {
	case CNSS_GENL_MSG_TYPE_DAEMON_SUPPORT:
		plat_priv->daemon_support = value;
		cnss_pr_info("Setting daemon_support=%d for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_GENL_MSG_TYPE_COLD_BOOT_SUPPORT:
		plat_priv->cold_boot_support = value;
		cnss_pr_info("Setting cold_boot_support=%d for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_GENL_MSG_TYPE_HDS_SUPPORT:
		plat_priv->hds_support = value;
		cnss_pr_info("Setting hds_support=%d for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_GENL_MSG_TYPE_REGDB_SUPPORT:
		plat_priv->regdb_support = value;
		cnss_pr_info("Setting regdb_support=%d for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_GENL_MSG_TYPE_QDSS_SUPPORT:
		plat_priv->qdss_support = value;
		cnss_pr_info("Setting qdss_support=%d for instance_id 0x%x\n",
			     value, instance_id);
		break;
	case CNSS_GENL_MSG_TYPE_QDSS_START:
		plat_priv->qdss_etr_sg_mode = value;
		cnss_pr_info("Starting QDSS for %s", plat_priv->device_name);
		cnss_wlfw_qdss_dnld_send_sync(plat_priv);
		break;
	case CNSS_GENL_MSG_TYPE_QDSS_STOP:
		cnss_pr_info("Stopping QDSS for %s", plat_priv->device_name);
		cnss_wlfw_send_qdss_trace_mode_req(plat_priv,
						   QMI_WLFW_QDSS_TRACE_OFF_V01,
						   value);
		break;
	default:
		cnss_pr_err("Unknown type %d\n", type);
		break;
	}
}

static int platform_get_userpd_id(struct platform_device *plat_dev,
					  uint32_t *userpd_id)
{
	int ret = 0;
	const char *subsys_name;

	ret = of_property_read_string(plat_dev->dev.of_node,
				      "qcom,userpd-subsys-name",
				      &subsys_name);
	if (ret) {
		pr_err("subsys name get failed");
		return -EINVAL;
	}

	if (strcmp(subsys_name, "q6v5_wcss_userpd2") == 0) {
		*userpd_id = USERPD_0;
	} else if (strcmp(subsys_name, "q6v5_wcss_userpd3") == 0) {
		*userpd_id = USERPD_1;
	} else if (strcmp(subsys_name, "q6v5_wcss_userpd4") == 0) {
		*userpd_id = USERPD_2;
	} else {
		pr_err("subsys name %s not found", subsys_name);
		ret = -EINVAL;
	}

	return ret;
}

static bool
cnss_check_skip_target_probe(const struct platform_device_id *device_id,
			     u32 userpd_id, u32 node_id)
{
	/* skip_cnss based skip target checks */
	if (skip_cnss == CNSS_SKIP_ALL) {
		pr_err("Skipping cnss_probe for device 0x%lx\n",
		       device_id->driver_data);
		return true;
	} else if (skip_cnss == CNSS_SKIP_PCI &&
		   (device_id->driver_data == QCN9000_DEVICE_ID ||
		    device_id->driver_data == QCN9224_DEVICE_ID)) {
		pr_err("Skipping cnss_probe for device 0x%lx\n",
		       device_id->driver_data);
		return true;
	} else if (skip_cnss == CNSS_SKIP_AHB &&
		   (device_id->driver_data == QCA8074_DEVICE_ID ||
		   device_id->driver_data == QCA8074V2_DEVICE_ID ||
		   device_id->driver_data == QCA6018_DEVICE_ID ||
		   device_id->driver_data == QCN6122_DEVICE_ID ||
		   device_id->driver_data == QCN9160_DEVICE_ID ||
		   device_id->driver_data == QCA5018_DEVICE_ID ||
		   device_id->driver_data == QCA5332_DEVICE_ID ||
		   device_id->driver_data == QCA9574_DEVICE_ID ||
		   device_id->driver_data == QCN6432_DEVICE_ID ||
		   device_id->driver_data == QCA5424_DEVICE_ID)) {
		pr_err("Skipping cnss_probe for device 0x%lx\n",
		       device_id->driver_data);
		return true;
	}

	/* skip_radio_bmap based skip target checks
	 * SKIP_INTEGRATED - skip integrated radios ie. 5018,8074,8074v2,6018
	 * SKIP_PCI_0 - skip PCI_0 radios ie. first qcn9000/qcn6122 radios
	 * SKIP_PCI_1 - skip PCI_1 radios ie. second qcn9000/qcn6122 radios
	 */
	if ((skip_radio_bmap & SKIP_INTEGRATED) &&
	    ((device_id->driver_data == QCA5018_DEVICE_ID) ||
	    (device_id->driver_data == QCA8074_DEVICE_ID) ||
	    (device_id->driver_data == QCA8074V2_DEVICE_ID) ||
	    (device_id->driver_data == QCA6018_DEVICE_ID) ||
	    (device_id->driver_data == QCA5332_DEVICE_ID) ||
	    (device_id->driver_data == QCA9574_DEVICE_ID) ||
	    (device_id->driver_data == QCA5424_DEVICE_ID))) {
		pr_err("Skipping cnss_probe for device 0x%lx\n",
		       device_id->driver_data);
		return true;
	} else if ((skip_radio_bmap & SKIP_PCI_0) &&
		   (((userpd_id == USERPD_0) &&
		   ((device_id->driver_data == QCN6122_DEVICE_ID) ||
  		   (device_id->driver_data == QCN9160_DEVICE_ID) ||
		   (device_id->driver_data == QCN6432_DEVICE_ID))) ||
		   ((node_id == QCN9000_0 || node_id == QCN9224_0) &&
		   (device_id->driver_data == QCN9000_DEVICE_ID ||
		    device_id->driver_data == QCN9224_DEVICE_ID)))) {
		pr_err("Skipping cnss_probe for PCI_0 device 0x%lx\n",
		       device_id->driver_data);
		return true;
	} else if ((skip_radio_bmap & SKIP_PCI_1) &&
		   (((userpd_id == USERPD_1) &&
		   ((device_id->driver_data == QCN6122_DEVICE_ID) ||
		   (device_id->driver_data == QCN9160_DEVICE_ID) ||
                   (device_id->driver_data == QCN6432_DEVICE_ID))) ||
		   ((node_id == QCN9000_1 || node_id == QCN9224_1) &&
		   (device_id->driver_data == QCN9000_DEVICE_ID ||
		    device_id->driver_data == QCN9224_DEVICE_ID)))) {
		pr_err("Skipping cnss_probe for PCI_1 device 0x%lx\n",
		       device_id->driver_data);
		return true;
	} else if ((skip_radio_bmap & SKIP_PCI_2) &&
		   ((node_id == QCN9000_2 || node_id == QCN9224_2) &&
		   (device_id->driver_data == QCN9000_DEVICE_ID ||
		    device_id->driver_data == QCN9224_DEVICE_ID))) {
		pr_err("Skipping cnss_probe for PCI_2 device 0x%lx\n",
		       device_id->driver_data);
		return true;
	} else if ((skip_radio_bmap & SKIP_PCI_3) &&
		   ((node_id == QCN9000_3 || node_id == QCN9224_3) &&
		   (device_id->driver_data == QCN9000_DEVICE_ID ||
		    device_id->driver_data == QCN9224_DEVICE_ID))) {
		pr_err("Skipping cnss_probe for PCI_3 device 0x%lx\n",
		       device_id->driver_data);
		return true;
	}

	return false;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static int cnss_panic_handler(struct notifier_block *this,
                                unsigned long event, void *ptr)
{
	int i;
	struct cnss_plat_data *plat_priv = NULL;

	mutex_lock(&rproc_list_mutex);
	for (i = 0; i < plat_env_index; i++) {
		if ((plat_env[i]->target_asserted == 1) &&
		!(test_bit(CNSS_DRIVER_UNLOADING, &plat_env[i]->driver_state) ||
		test_bit(CNSS_DRIVER_IDLE_SHUTDOWN,
			     &plat_env[i]->driver_state))){
			mutex_unlock(&rproc_list_mutex);
			return 0;
		}
	}

	for (i = 0; i < plat_env_index; i++) {
		cnss_pr_dbg("cnss_panic_handler for plat_env %d\n", i);
		cnss_bus_dev_crash_shutdown(plat_env[i]);
	}

	mutex_unlock(&rproc_list_mutex);
	return 0;
}

static void cnss_panic_notifier_register(void)
{
	int ret;
	struct cnss_plat_data *plat_priv = NULL;
	panic_nb.notifier_call = cnss_panic_handler;
	panic_nb.priority = 100;

	ret = atomic_notifier_chain_register(&panic_notifier_list, &panic_nb);
	if(ret)
		cnss_pr_err("Err(%d): register panic notifier failed.\n", ret);
	else
		cnss_pr_dbg("%s: atomic_notifier_chain_register success.\n", __func__);

	return;
}

static void cnss_panic_notifier_unregister(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_nb);
	return;
}
#endif

#ifdef CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
const struct rproc_ops cnss_rproc_ops = {
	.start = cnss_subsys_powerup,
	.stop = cnss_subsys_shutdown,
	.load = cnss_subsys_dummy_load,
	.parse_fw = cnss_subsys_add_ramdump_callback,
	.report_panic = cnss_subsys_crash_shutdown,
};

static int cnss_rproc_register(struct cnss_plat_data *plat_priv)
{
	struct device *dev = &plat_priv->plat_dev->dev;
	struct rproc *rproc_handle = NULL;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_AHB:
		/* rproc alloc is done from qcom_q6v5_wcss code for AHB
		 * plat_priv->rproc_handle will be filled during the first
		 * wifi load from cnss_register_subsys
		 */
		return 0;
	case CNSS_BUS_PCI:
		rproc_handle = rproc_alloc(dev, plat_priv->device_name,
					   &cnss_rproc_ops,
					   plat_priv->firmware_name, 0);

		if (IS_ERR_OR_NULL(rproc_handle)) {
			cnss_pr_err("%s: Failed to register rproc %ld for device 0x%s\n",
				    __func__, PTR_ERR(rproc_handle),
				    plat_priv->device_name);
			return -ENODEV;
		}

		rproc_handle->auto_boot = false;
		if (rproc_add(rproc_handle)) {
			cnss_pr_err("%s: Failed to add rproc for device %s\n",
				    __func__, plat_priv->device_name);
			rproc_free(rproc_handle);
			return -EINVAL;
		}

		plat_priv->rproc_handle = rproc_handle;
		break;
	default:
		cnss_pr_err("Invalid bus type for device %s\n",
			    plat_priv->device_name);
		return -EINVAL;
	}
	return 0;
}
#endif
static void cnss_rproc_unregister(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->rproc_handle) {
		/* For AHB targets, ref count for rproc is taken during first
		 * wifi load from cnss_register_subsys, put the ref count here.
		 * For PCI targets, rproc_handle is allocated during cnss_prove,
		 * delete and free the proc_handle here.
		 */
		if (plat_priv->bus_type == CNSS_BUS_AHB) {
			rproc_put(plat_priv->rproc_handle);
		} else if (plat_priv->bus_type == CNSS_BUS_PCI) {
			rproc_del(plat_priv->rproc_handle);
			rproc_free(plat_priv->rproc_handle);
		}
	}
}
#endif

static void cnss_fill_probe_order(struct cnss_plat_data *plat_priv)
{
	u32 prb_order = 0;

	if (probe_order) {
		plat_priv->probe_order = (probe_order & CNSS_PROBE_ORDER_MASK);
		probe_order >>= CNSS_PROBE_ORDER_SHIFT;
	} else if (!of_property_read_u32(plat_priv->plat_dev->dev.of_node,
					 "probe-order", &prb_order)) {
		plat_priv->probe_order = prb_order;
	} else {
		plat_priv->probe_order = CNSS_PROBE_ORDER_DEFAULT;
	}
}

#ifdef CONFIG_CNSS2_LEGACY_IRQ
static void cnss_get_legacy_intx_support(struct cnss_plat_data *plat_priv)
{
	int enable_intx;

	if (enable_intx_bmap) {
		enable_intx = (enable_intx_bmap & CNSS_INTX_SUPPORT_MASK);
		enable_intx_bmap >>= CNSS_INTX_SUPPORT_SHIFT;
		if (enable_intx)
			plat_priv->enable_intx = true;
	} else if (of_property_read_bool(plat_priv->plat_dev->dev.of_node,
					 "enable-intx")) {
		plat_priv->enable_intx = true;
	} else {
		plat_priv->enable_intx = false;
	}
}
#endif
static u32 cnss_get_bdf_mod_param(int slot_id)
{
	u32 ret = 0;

	switch (slot_id) {
	case 0:
		ret = (u32)bdf_pci0;
		break;
	case 1:
		ret = (u32)bdf_pci1;
		break;
	case 2:
		ret = (u32)bdf_pci2;
		break;
	case 3:
		ret = (u32)bdf_pci3;
		break;
	default:
		break;
	}
	return ret;
}

/* Set board_id_override field in plat_priv based on the below priority order
 * Bootargs -- bdf_integrated, bdf_pciX...
 * DTS entry
 *
 * If both these are not present, board_id_override would be 0 and board_id
 * from OTP register or target capabilities would be used.
 */
static void cnss_set_board_id(struct cnss_plat_data *plat_priv)
{
	struct wlfw_rf_board_info *board_info = &plat_priv->board_info;
	struct device *dev = &plat_priv->plat_dev->dev;
	const char *board_id_str;

	switch (plat_priv->device_id) {
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA9574_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		board_id_str = "qcom,board_id";
		board_info->num_bytes = 1;
		board_info->board_id_override = (u32)bdf_integrated;
		break;
	case QCN9160_DEVICE_ID:
	case QCN6122_DEVICE_ID:
		board_info->num_bytes = 1;
		board_id_str = "qcom,board_id";
		board_info->board_id_override =
			cnss_get_bdf_mod_param(plat_priv->userpd_id - 1);
		break;
	case QCN6432_DEVICE_ID:
		board_info->num_bytes = 2;
		board_id_str = "qcom,board_id";
		board_info->board_id_override =
			cnss_get_bdf_mod_param(plat_priv->userpd_id - 1);
		break;
	case QCN9000_DEVICE_ID:
		board_id_str = "board_id";
		board_info->num_bytes = 1;
		board_info->board_id_override =
			cnss_get_bdf_mod_param(plat_priv->pci_slot_id);
		break;
	case QCN9224_DEVICE_ID:
		board_id_str = "board_id";
		board_info->num_bytes = 2;
		board_info->board_id_override =
			cnss_get_bdf_mod_param(plat_priv->pci_slot_id);
		break;
	default:
		cnss_pr_err("No such device id 0x%lx\n", plat_priv->device_id);
		return;
	}

	if (!board_info->board_id_override) {
		if (of_property_read_u32(dev->of_node, board_id_str,
					 &board_info->board_id_override))
			cnss_pr_info("No board_id in device tree for %s\n",
					plat_priv->device_name);
	}

	cnss_pr_dbg("%s: board_id_override 0x%x for device %s\n", __func__,
		    board_info->board_id_override,
		    plat_priv->device_name);
}

int cnss_set_fw_type_and_name(struct cnss_plat_data *plat_priv)
{
	const char *firmware_name = NULL;
	struct device *dev = &plat_priv->plat_dev->dev;
	u32 firmware_name_len;
	u32 board_id = plat_priv->board_info.board_id_override;

	/* AHB devices use PIL binaries, firmware_name is applicable
	 * only for PCI devices.
	 */
	if (plat_priv->bus_type == CNSS_BUS_AHB)
		return 0;

	plat_priv->firmware_type =
		(board_id & CNSS_FW_TYPE_MASK) >> CNSS_FW_TYPE_SHIFT;

	firmware_name_len = strlen(cnss_get_fw_path(plat_priv));
	firmware_name = of_get_property(dev->of_node, "firmware_name", NULL);

	/* If firmware_name not defined in DTS, use default FW name */
	if (!firmware_name) {
		switch (plat_priv->firmware_type) {
		case CNSS_FW_DUAL_MAC:
			firmware_name = DUALMAC_FW_FILE_NAME;
			break;
		case CNSS_FW_DEFAULT:
			/* Fall Through */
		default:
			firmware_name = DEFAULT_FW_FILE_NAME;
		}
	}

	firmware_name_len += strlen(firmware_name);
	if (firmware_name_len > PATH_MAX) {
		cnss_pr_err("firmware_name_len too long %d",
			    firmware_name_len);
		return -EINVAL;
	}

	if (!plat_priv->firmware_name) {
		plat_priv->firmware_name =
				kzalloc(firmware_name_len + 1, GFP_KERNEL);
		if (!plat_priv->firmware_name)
			return -ENOMEM;
	} else {
		memset(plat_priv->firmware_name, 0, firmware_name_len + 1);
	}

	snprintf(plat_priv->firmware_name, firmware_name_len + 1,
		 "%s%s", cnss_get_fw_path(plat_priv), firmware_name);

	return 0;
}

static int cnss_probe(struct platform_device *plat_dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = NULL;
	const struct of_device_id *of_id;
	const struct platform_device_id *device_id;
	u32 node_id = 0, userpd_id = 0, node_id_base;
	unsigned long flags;
#ifdef CONFIG_CNSS2_KERNEL_IPQ
	const int *soc_version;
#endif
	if (cnss_get_plat_priv(plat_dev)) {
		pr_err("Driver is already initialized!\n");
		ret = -EEXIST;
		goto out;
	}
	of_id = of_match_device(cnss_of_match_table, &plat_dev->dev);
	if (!of_id || !of_id->data) {
		pr_err("Failed to find of match device!\n");
		ret = -ENODEV;
		goto out;
	}

	device_id = (const struct platform_device_id *)of_id->data;

	if ((device_id->driver_data == QCN6122_DEVICE_ID ||
	     device_id->driver_data == QCN9160_DEVICE_ID ||
	     device_id->driver_data == QCN6432_DEVICE_ID) &&
	    (platform_get_userpd_id(plat_dev, &userpd_id))) {
		pr_err("Error: No userpd_id in device_tree\n");
		CNSS_ASSERT(0);
		ret = -ENODEV;
		goto out;
	}

	if (device_id->driver_data == QCN9000_DEVICE_ID ||
	     device_id->driver_data == QCN9224_DEVICE_ID)
		if (cnss_get_node_id(plat_dev, device_id->driver_data,
				     &node_id))
			goto out;

#ifdef CONFIG_QCOM_SOCINFO
#ifdef CONFIG_CNSS2_KERNEL_IPQ
	/* Check for QCA9574 here and skip probe accordingly */
	if (device_id->driver_data == QCA9574_DEVICE_ID &&
	    !cpu_is_internal_wifi_enabled()) {
		pr_err("Skipping probe for device 0x%lx\n",
		       device_id->driver_data);
		ret = -ENODEV;
		goto out;
	}
#endif
#endif
	if (cnss_check_skip_target_probe(device_id, userpd_id, node_id))
		goto out;

#ifdef CONFIG_CNSS_QCN9000
	if (device_id->driver_data == QCA6174_DEVICE_ID) {
		pr_err("No probe for QCA6174\n");
		ret = -EINVAL;
		goto out;
	}
#else
	if (device_id->driver_data == QCN9000_DEVICE_ID) {
		pr_err("No probe for QCN9000\n");
		ret = -EINVAL;
		goto out;
	}
#endif

#ifdef CONFIG_CNSS2_KERNEL_IPQ
	soc_version = of_get_property(of_find_node_by_path("/"),
				      "soc_version_major", NULL);
	if (!soc_version) {
		pr_err("Failed to get soc_version_major from device tree");
		CNSS_ASSERT(0);
		ret = -EINVAL;
		goto out;
	}

	soc_version_major = *soc_version;
	soc_version_major = le32_to_cpu(soc_version_major);

	if (device_id->driver_data == QCA8074_DEVICE_ID) {
		if (soc_version_major == 2) {
			pr_err("Skip QCA8074V1 in V2 platform\n");
			ret = -ENODEV;
			goto out;
		}
	}

	if (device_id->driver_data == QCA8074V2_DEVICE_ID) {
		if (soc_version_major == 1) {
			pr_err("Skip QCA8074V2 in V1 platform\n");
			ret = -ENODEV;
			goto out;
		}
	}
#endif
	if (plat_env_index >= MAX_NUMBER_OF_SOCS) {
		pr_err("cnss: plat_env_index %d greater than MAX_NUMBER_OF_SOCS\n",
			plat_env_index);
		ret = -ENOMEM;
		goto out;
	}

	plat_priv = devm_kzalloc(&plat_dev->dev, sizeof(*plat_priv),
				 GFP_KERNEL);
	if (!plat_priv) {
		ret = -ENOMEM;
		goto out;
	}

	plat_priv->plat_dev = plat_dev;
	plat_priv->device_id = device_id->driver_data;
	plat_priv->plat_dev_id = (struct platform_device_id *)device_id;
	plat_priv->service_id = WLFW_SERVICE_ID_V01;
	plat_priv->wsi_remap_state = false;

#ifdef CONFIG_CNSS2_DMA_ALLOC
	plat_priv->dma_alloc_supported = true;
#endif

	switch (plat_priv->device_id) {
	case QCN9224_DEVICE_ID:
		plat_priv->mlo_support = !!enable_mlo_support;
		/* Fall Through */
	case QCN9000_DEVICE_ID:
		plat_priv->bus_type = CNSS_BUS_PCI;
		plat_priv->bdf_dnld_method = WLFW_SEND_BDF_OVER_QMI_V01;
		plat_priv->qrtr_node_id = node_id;
		plat_priv->wlfw_service_instance_id = node_id + FW_ID_BASE;

		if (plat_priv->device_id == QCN9224_DEVICE_ID)
			node_id_base = QCN9224_NODE_ID_BASE;
		else
			node_id_base = QCN9000_NODE_ID_BASE;

		plat_priv->pci_slot_id = plat_priv->wlfw_service_instance_id -
						node_id_base;
		break;
	case QCA5332_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		plat_priv->mlo_support = !!enable_mlo_support;
		/* Fall Through */
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA9574_DEVICE_ID:
		plat_priv->bus_type = CNSS_BUS_AHB;
		plat_priv->bdf_dnld_method = WLFW_DIRECT_BDF_COPY_V01;
		plat_priv->wlfw_service_instance_id =
			WLFW_SERVICE_INS_ID_V01_QCA8074;
		break;
	case QCN6432_DEVICE_ID:
		plat_priv->mlo_support = !!enable_mlo_support;
		/* Fall Through */
	case QCN6122_DEVICE_ID:
	case QCN9160_DEVICE_ID:
		plat_priv->bus_type = CNSS_BUS_AHB;
		plat_priv->bdf_dnld_method = WLFW_DIRECT_BDF_COPY_V01;
		plat_priv->userpd_id = userpd_id;
		if (plat_priv->device_id == QCN9160_DEVICE_ID)
			plat_priv->wlfw_service_instance_id =
				WLFW_SERVICE_INS_ID_V01_QCN9160 + userpd_id;
		else if (plat_priv->device_id == QCN6122_DEVICE_ID)
			plat_priv->wlfw_service_instance_id =
				WLFW_SERVICE_INS_ID_V01_QCN6122 + userpd_id;
		else if (plat_priv->device_id == QCN6432_DEVICE_ID) {
			plat_priv->wlfw_service_instance_id =
			WLFW_SERVICE_INS_ID_V01_QCN6432 + userpd_id;
				plat_priv->mlo_support = !!enable_mlo_support;
		}
		if (userpd_id == USERPD_0)
			plat_priv->board_info.board_id_override = bdf_pci0;
		else if (userpd_id == USERPD_1)
			plat_priv->board_info.board_id_override = bdf_pci1;

#ifdef CONFIG_CNSS2_QGIC2M
		plat_priv->tgt_data.qgic2_msi =
			cnss_qgic2_enable_msi(plat_priv);
		if (!plat_priv->tgt_data.qgic2_msi) {
			cnss_pr_err("qgic2_msi fails: dev 0x%lx userpd id %d\n",
				    plat_priv->device_id, userpd_id);
			ret = -ENODEV;
			goto out;
		}
#endif
		break;
	default:
		cnss_pr_err("No such device id %p\n", device_id);
		return -ENODEV;
	}

	if (plat_priv->bus_type == CNSS_BUS_AHB)
		plat_priv->ops = cnss_ahb_get_ops();
#ifdef CNSS_PCI_SUPPORT
	else if (plat_priv->bus_type == CNSS_BUS_PCI)
		plat_priv->ops = cnss_pci_get_ops();
#endif

	ret = cnss_set_device_name(plat_priv);
	if (ret)
		return -ENODEV;

	cnss_set_board_id(plat_priv);

	ret = cnss_set_fw_type_and_name(plat_priv);
	if (ret)
		return -ENODEV;
	cnss_set_ssr_recovery_type(plat_priv);

#if defined(CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	ret = cnss_rproc_register(plat_priv);
	if (ret)
		goto out;
#endif

	/*
	 * plat_env is initialized only at the end of cnss_probe.
	 * plat_env should not be referred to in any functions within cnss_probe
	 * till it is initialized.
	 */
	cnss_fill_probe_order(plat_priv);
#ifdef CONFIG_CNSS2_LEGACY_IRQ
	cnss_get_legacy_intx_support(plat_priv);
#endif
	cnss_set_mod_param_feature_support(plat_priv, CALDATA);
	cnss_set_mod_param_feature_support(plat_priv, REGDB);
	platform_set_drvdata(plat_dev, plat_priv);
	memset(&qmi_log, 0, sizeof(struct qmi_history) * QMI_HISTORY_SIZE);
	INIT_LIST_HEAD(&plat_priv->vreg_list);
	INIT_LIST_HEAD(&plat_priv->clk_list);

#if defined(CNSS_LOWMEM_PROFILE) && defined(CNSS_FW_MOUNT_SUPPORT) && \
	defined(CONFIG_CNSS2_KERNEL_IPQ)
	INIT_DELAYED_WORK(&umount_firmware_wq, cnss_schedule_umount_firmware);
#endif
	cnss_init_control_params(plat_priv);

	ret = cnss_get_resources(plat_priv);
	if (ret)
		goto reset_ctx;

	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks)) {
		ret = cnss_power_on_device(plat_priv, plat_priv->device_id);
		if (ret)
			goto free_res;
	}

#ifdef CONFIG_CNSS2_PM
	ret = cnss_register_esoc(plat_priv);
	if (ret)
		goto deinit_bus;

	ret = cnss_register_bus_scale(plat_priv);
	if (ret)
		goto unreg_esoc;
#endif

	ret = cnss_create_sysfs(plat_priv);
	if (ret)
		goto unreg_bus_scale;

	ret = cnss_event_work_init(plat_priv);
	if (ret)
		goto remove_sysfs;

	ret = cnss_qmi_init(plat_priv);
	if (ret)
		goto deinit_event_work;
	ret = cnss_debugfs_create(plat_priv);
	if (ret)
		goto deinit_qmi;
	ret = cnss_misc_init(plat_priv);
	if (ret)
		goto destroy_debugfs;
#if defined(CNSS2_COEX) || defined(CNSS2_IMS)
	cnss_register_coex_service(plat_priv);
	cnss_register_ims_service(plat_priv);
#endif

	ret = cnss_genl_init();
	if (ret < 0)
		cnss_pr_err("CNSS genl init failed %d\n", ret);

	ret = cnss_init_m3_dump_class(plat_priv);
	if (ret)
		goto deinit_genl;

	ret = cnss_recovery_work_init(plat_priv);
	if (ret)
		goto deinit_genl;
	cnss_cal_work_init(plat_priv);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	cnss_crash_work_init(plat_priv);
#endif

	/* Incrementing plat_env_index only after the probe for the device
	 * is completed
	 */
	spin_lock_irqsave(&plat_env_spinlock, flags);
	plat_env[plat_env_index++] = plat_priv;
	spin_unlock_irqrestore(&plat_env_spinlock, flags);
	cnss_pr_info("Platform driver probed successfully. plat 0x%pK tgt 0x%lx\n",
		     plat_priv, plat_priv->device_id);

	return 0;

deinit_genl:
	cnss_genl_exit();
destroy_debugfs:
	cnss_debugfs_destroy(plat_priv);
deinit_qmi:
	cnss_qmi_deinit(plat_priv);
deinit_event_work:
	cnss_event_work_deinit(plat_priv);
remove_sysfs:
	cnss_remove_sysfs(plat_priv);
unreg_bus_scale:
#ifdef CONFIG_CNSS2_PM
	cnss_unregister_bus_scale(plat_priv);
unreg_esoc:
	cnss_unregister_esoc(plat_priv);
deinit_bus:
#endif
	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks))
		cnss_bus_deinit(plat_priv);
	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks))
		cnss_power_off_device(plat_priv, plat_priv->device_id);
free_res:
	cnss_put_resources(plat_priv);
reset_ctx:
	platform_set_drvdata(plat_dev, NULL);
	plat_env[plat_env_index] = NULL;
#ifdef CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK
	cnss_rproc_unregister(plat_priv);
#endif
out:
	return ret;
}

static int cnss_remove(struct platform_device *plat_dev)
{
	int i = 0;
	unsigned long flags = 0;
	struct cnss_plat_data *plat_priv = platform_get_drvdata(plat_dev);

	if (!plat_priv)
		return 0;

	/* For platforms that support dma_alloc, FW memory is allocated during
	 * first wifi load and not freed during wifi down, so we are freeing
	 * here during rmmod of cnss2
	 */
	if (plat_priv->dma_alloc_supported) {
		cnss_bus_free_fw_mem(plat_priv);
		cnss_bus_free_qdss_mem(plat_priv);
	}
	cnss_deinit_m3_dump_class();
#if defined(CONFIG_CNSS2_KERNEL_MSM) || defined(CONFIG_CNSS2_KERNEL_5_15)
	/*
	 * ramdump_device allocated during pci_probe is not freed during
	 * pci_remove. So we are freeing in cnss2 rmmod.
	 */
	if (plat_priv->rd_dev_present)
		cnss_unregister_ramdump(plat_priv);
#endif

#ifdef CONFIG_CNSS2_QGIC2M
	cnss_qgic2_disable_msi(plat_priv);
#endif
	cnss_genl_exit();
#if defined(CNSS2_COEX) || defined(CNSS2_IMS)
	cnss_unregister_ims_service(plat_priv);
	cnss_unregister_coex_service(plat_priv);
#endif
	cnss_misc_deinit(plat_priv);
#if defined CNSS_DEBUG_SUPPORT
	cnss_debugfs_destroy(plat_priv);
#endif
	cnss_qmi_deinit(plat_priv);
	cnss_event_work_deinit(plat_priv);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	cnss_crash_work_deinit(plat_priv);
#endif
	cnss_cal_work_deinit(plat_priv);
	cnss_recovery_work_deinit(plat_priv);
	cnss_remove_sysfs(plat_priv);
#ifdef CONFIG_CNSS2_PM
	cnss_unregister_bus_scale(plat_priv);
	cnss_unregister_esoc(plat_priv);
#endif
	cnss_bus_deinit(plat_priv);
	cnss_put_resources(plat_priv);

#ifdef CONFIG_CNSS2_KERNEL_RPROC_FRAMEWORK
	cnss_rproc_unregister(plat_priv);
#endif
	platform_set_drvdata(plat_dev, NULL);
	kfree(plat_priv->firmware_name);

	for (i = 0; i < MAX_NUMBER_OF_SOCS; i++) {
		if (plat_env[i] == plat_priv) {
			spin_lock_irqsave(&plat_env_spinlock, flags);
			plat_env[i] = NULL;
			plat_env_index--;
			spin_unlock_irqrestore(&plat_env_spinlock, flags);
			break;
		}
	}
	return 0;
}

static struct platform_driver cnss_platform_driver = {
	.probe  = cnss_probe,
	.remove = cnss_remove,
	.driver = {
		.name = "cnss2",
		.of_match_table = cnss_of_match_table,
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	},
};

static void cnss_init_ipc_qmi_cb(struct cnss_plat_ipc_qmi_cb *ipc_qmi_cb)
{
	ipc_qmi_cb->connection_update_cb = cnss_daemon_connection_update_cb;
	ipc_qmi_cb->config_param_cb = cnss_config_param_update_cb;
}

static int __init cnss_initialize(void)
{
	int ret = 0;
	struct cnss_plat_ipc_qmi_cb ipc_qmi_callbacks;
#if defined CNSS_DEBUG_SUPPORT
	cnss_debug_init();
#endif
	ret = platform_driver_register(&cnss_platform_driver);
	if (ret) {
#if defined CNSS_DEBUG_SUPPORT
		cnss_debug_deinit();
#endif
		return ret;
	}
#ifdef CONFIG_CNSS2_LEGACY_IRQ
	ret = cnss_legacy_irq_init();
	if (ret) {
		platform_driver_unregister(&cnss_platform_driver);
#if defined CNSS_DEBUG_SUPPORT
		cnss_debug_deinit();
#endif
		return ret;
	}
#endif
	cnss_bus_init_by_type(CNSS_BUS_PCI);
	cnss_plat_ipc_qmi_svc_init();
	cnss_init_ipc_qmi_cb(&ipc_qmi_callbacks);
	cnss_plat_ipc_register(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
			       &ipc_qmi_callbacks, NULL);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	cnss_panic_notifier_register();
#endif

	return ret;
}

static void __exit cnss_exit(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	cnss_panic_notifier_unregister();
#endif
	cnss_plat_ipc_unregister(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01, NULL);
	cnss_plat_ipc_qmi_svc_exit();
	cnss_pci_deinit(NULL);
#ifdef CONFIG_CNSS2_LEGACY_IRQ
	cnss_legacy_irq_deinit();
#endif
	platform_driver_unregister(&cnss_platform_driver);
#if defined CNSS_DEBUG_SUPPORT
	cnss_debug_deinit();
#endif
}

module_init(cnss_initialize);
module_exit(cnss_exit);

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS2 Platform Driver");
