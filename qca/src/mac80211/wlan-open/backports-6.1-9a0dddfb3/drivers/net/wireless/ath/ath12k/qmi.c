// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/elf.h>

#include "qmi.h"
#include "core.h"
#include "debug.h"
#include "hif.h"
#include "coredump.h"
#include "ahb.h"
#include <linux/of.h>
#include <linux/firmware.h>
#include <net/sock.h>
#include <linux/of_address.h>
#include <linux/ioport.h>
#include <linux/devcoredump.h>

#ifdef CPTCFG_ATHDEBUG
#include "athdbg_if.h"
#define qmi_handle_init athdbg_qmi_handle_init
#endif

#define SLEEP_CLOCK_SELECT_INTERNAL_BIT	0x02
#define HOST_CSTATE_BIT			0x04
#define PLATFORM_CAP_PCIE_GLOBAL_RESET	0x08
#define ATH12K_QMI_MAX_CHUNK_SIZE	2097152

static bool ath12k_skip_caldata;
module_param_named(skip_caldata, ath12k_skip_caldata, bool, 0444);
MODULE_PARM_DESC(skip_caldata, "Skip caldata download");

bool ath12k_cold_boot_cal = 1;
module_param_named(cold_boot_cal, ath12k_cold_boot_cal, bool, 0644);
MODULE_PARM_DESC(cold_boot_cal,
		 "Decrease the channel switch time but increase the driver load time (Default: true)");

static struct qmi_elem_info wlfw_host_ddr_range_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct
					   qmi_wlanfw_host_ddr_range,
					   start),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct
					   qmi_wlanfw_host_ddr_range,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info wlfw_host_mlo_chip_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v01,
					   chip_id),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v01,
					   num_local_links),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01,
		.elem_size      = sizeof(u8),
		.array_type     = STATIC_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v01,
					   hw_link_id),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01,
		.elem_size      = sizeof(u8),
		.array_type     = STATIC_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v01,
					   valid_mlo_link_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_host_mlo_chip_info_s_v02_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_host_mlo_chip_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v02,
					   mlo_chip_info),
		.ei_array       = wlfw_host_mlo_chip_info_s_v01_ei,
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v02,
					   num_adj_chips),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_ADJ_CHIPS_V01,
		.elem_size      = sizeof(struct wlfw_host_mlo_chip_info_s_v01),
		.array_type     = STATIC_ARRAY,
		.tlv_type       = 0x00,
		.offset         = offsetof(struct wlfw_host_mlo_chip_info_s_v02,
					   mlo_adj_chip_info),
		.ei_array       = wlfw_host_mlo_chip_info_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info qmi_wlanfw_cold_boot_cal_done_ind_msg_v01_ei[] = {
	{
		.data_type = QMI_EOTI,
		.array_type = NO_ARRAY,
	},
};

static const struct qmi_elem_info qmi_wlanfw_host_cap_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   num_clients_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   num_clients),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   wake_msi_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   wake_msi),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   gpios_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   gpios_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_WLFW_MAX_NUM_GPIO_V01,
		.elem_size	= sizeof(u32),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   gpios),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   nm_modem_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   nm_modem),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_cache_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_cache_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_cache_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_cache_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_filesys_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_filesys_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_cache_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_cache_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_done_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_done),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_bucket_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_bucket),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_cfg_mode_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_cfg_mode),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1D,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_duration_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1D,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_duraiton),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1E,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   platform_name_valid),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= QMI_WLANFW_MAX_PLATFORM_NAME_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1E,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   platform_name),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1F,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   ddr_range_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_HOST_DDR_RANGE_SIZE_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_host_ddr_range),
		.array_type	= STATIC_ARRAY,
		.tlv_type	= 0x1F,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   ddr_range),
		.ei_array      = wlfw_host_ddr_range_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x20,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   host_build_type_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_host_build_type),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x20,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   host_build_type),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x21,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_capable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x21,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_capable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x22,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_chip_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x22,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_chip_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x23,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_group_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x23,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_group_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x24,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   max_mlo_peer_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x24,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   max_mlo_peer),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x25,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_num_chips_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x25,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_num_chips),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x26,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_chip_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLFW_MAX_NUM_MLO_CHIPS_V01,
		.elem_size	= sizeof(struct wlfw_host_mlo_chip_info_s_v01),
		.array_type	= STATIC_ARRAY,
		.tlv_type	= 0x26,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mlo_chip_info),
		.ei_array	= wlfw_host_mlo_chip_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x27,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   feature_list_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x27,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   feature_list),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x2E,
		.offset         = offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   fw_cfg_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x2E,
		.offset         = offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   fw_cfg_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x2F,
		.offset         = offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
							mlo_chip_info_v2_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_CHIPS_V01,
		.elem_size      = sizeof(struct wlfw_host_mlo_chip_info_s_v02),
		.array_type     = STATIC_ARRAY,
		.tlv_type       = 0x2F,
		.offset         = offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
							mlo_chip_info_v2),
		.ei_array      = wlfw_host_mlo_chip_info_s_v02_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x33,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   dynamic_mem_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x33,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   dynamic_mem_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x34,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   ftm_mode_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x34,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   ftm_mode),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x35,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cma_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x35,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cma_support),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_host_cap_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_mem_seg_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_mem_seg_info_s_v01,
					   addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_mem_seg_info_s_v01,
					   size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_mem_seg_info_s_v01,
					   type),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wlanfw_cfg_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   file_type_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlanfw_cfg_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   file_type),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLANFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type     = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlanfw_cfg_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wlanfw_cfg_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlanfw_cfg_download_resp_msg_v01,
				   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_mem_seg_resp_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, type),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, restore),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_phy_cap_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_phy_cap_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   num_phy_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   num_phy),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   board_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   board_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   single_chip_mlo_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   single_chip_mlo_support),
	},
	{
		.data_type  = QMI_OPT_FLAG,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x14,
		.offset     = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					mm_coldboot_cal_valid),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x14,
		.offset     = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					mm_coldboot_cal),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   aux_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   aux_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   mcss_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   mcss_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   dynamic_ddr_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct qmi_wlanfw_phy_cap_resp_msg_v01,
					   dynamic_ddr_support),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_ind_register_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_ready_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_ready_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   msa_ready_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   msa_ready_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   pin_connect_result_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   pin_connect_result_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   client_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   client_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_mem_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_mem_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_mem_ready_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_mem_ready_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_init_done_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_init_done_enable),
	},

	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   rejuvenate_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   rejuvenate_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   xo_cal_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   xo_cal_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   cal_done_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   cal_done_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
		                           qdss_trace_req_mem_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
		                           qdss_trace_req_mem_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
		                           qdss_trace_save_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
		                           qdss_trace_save_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   m3_dump_upload_req_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   m3_dump_upload_req_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x26,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   ddr_dump_region_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x26,
		.offset         = offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   ddr_dump_region_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x28,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_mem_free_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x28,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_mem_free),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x29,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_cma_mem_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x29,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_cma_mem_enable),
	},

	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_ind_register_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_resp_msg_v01,
					   fw_status_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_resp_msg_v01,
					   fw_status),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_mem_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, offset),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, size),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, secure_flag),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_mem_seg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01,
				  size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, type),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, mem_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_MEM_CFG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, mem_cfg),
		.ei_array	= qmi_wlanfw_mem_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_request_mem_ind_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_request_mem_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_request_mem_ind_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_respond_mem_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_req_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_resp_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_req_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_respond_mem_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_request_mem_free_ind_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
				    qmi_wlanfw_request_mem_free_ind_msg_v01,
				    mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_info_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_cap_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info qmi_wlanfw_device_info_req_msg_v01_ei[] = {
        {
                .data_type      = QMI_EOTI,
                .array_type     = NO_ARRAY,
                .tlv_type       = QMI_COMMON_TLV_TYPE,
        },
};

struct qmi_elem_info qmi_wlanfw_device_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct qmi_wlanfw_device_info_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct qmi_wlanfw_device_info_resp_msg_v01,
					   bar_addr_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct qmi_wlanfw_device_info_resp_msg_v01,
					   bar_addr),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset		= offsetof(struct qmi_wlanfw_device_info_resp_msg_v01,
					   bar_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_device_info_resp_msg_v01,
					   bar_size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_rf_chip_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_rf_chip_info_s_v01,
					   chip_id),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_rf_chip_info_s_v01,
					   chip_family),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_rf_board_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_rf_board_info_s_v01,
					   board_id),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_soc_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_soc_info_s_v01, soc_id),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_dev_mem_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_dev_mem_info_s_v01,
					   start),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_dev_mem_info_s_v01,
					   size),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_fw_version_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_fw_version_info_s_v01,
					   fw_version),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_fw_version_info_s_v01,
					   fw_build_timestamp),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_cap_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   chip_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_rf_chip_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   chip_info),
		.ei_array	= qmi_wlanfw_rf_chip_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   board_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_rf_board_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   board_info),
		.ei_array	= qmi_wlanfw_rf_board_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   soc_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_soc_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   soc_info),
		.ei_array	= qmi_wlanfw_soc_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_version_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_fw_version_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_version_info),
		.ei_array	= qmi_wlanfw_fw_version_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_build_id_valid),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_build_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   num_macs_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   num_macs),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   voltage_mv_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   voltage_mv),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   time_freq_hz_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   time_freq_hz),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   otp_version_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   otp_version),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   eeprom_caldata_read_timeout_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   eeprom_caldata_read_timeout),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_caps_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01, fw_caps),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   rd_card_chain_cap_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   rd_card_chain_cap),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   dev_mem_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLFW_MAX_DEV_MEM_NUM_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_dev_mem_info_s_v01),
		.array_type	= STATIC_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01, dev_mem),
		.ei_array	= qmi_wlanfw_dev_mem_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x25,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   rxgainlut_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x25,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   rxgainlut_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2B,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   ext_fw_bin_download_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2B,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   ext_fw_bin_download_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2C,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   ext_fw_bin_download_bitmap_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2C,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   ext_fw_bin_download_bitmap),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_bdf_download_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   valid),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   file_id_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_cal_temp_id_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= QMI_WLANFW_MAX_DATA_SIZE_V01,
		.elem_size	= sizeof(u8),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   data),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   end),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   bdf_type_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   bdf_type),
	},

	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_bdf_download_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_m3_info_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_m3_info_req_msg_v01, addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_m3_info_req_msg_v01, size),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_m3_info_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_m3_info_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_ce_tgt_pipe_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_pipedir_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   nentries),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   nbytes_max),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   flags),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_ce_svc_pipe_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01,
					   service_id),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_pipedir_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_shadow_reg_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_shadow_reg_cfg_s_v01, id),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u16),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_shadow_reg_cfg_s_v01,
					   offset),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_shadow_reg_v3_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_shadow_reg_v3_cfg_s_v01,
					   addr),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_wlan_mode_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   mode),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   hw_debug_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   hw_debug),
	},
	{
		.data_type  = QMI_OPT_FLAG,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x13,
		.offset     = offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   do_coldboot_cal_valid),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x13,
		.offset     = offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   do_coldboot_cal),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_wlan_mode_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_wlan_cfg_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   host_version_valid),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= QMI_WLANFW_MAX_STR_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   host_version),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_CE_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   tgt_cfg),
		.ei_array	= qmi_wlanfw_ce_tgt_pipe_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   svc_cfg_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   svc_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_SVC_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   svc_cfg),
		.ei_array	= qmi_wlanfw_ce_svc_pipe_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_SHADOW_REG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_shadow_reg_cfg_s_v01),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg),
		.ei_array	= qmi_wlanfw_shadow_reg_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v3_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v3_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_SHADOW_REG_V3_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_shadow_reg_v3_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v3),
		.ei_array	= qmi_wlanfw_shadow_reg_v3_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   ext_tgt_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   ext_tgt_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLANFW_MAX_EXT_NUM_CE_V01,
		.elem_size      = sizeof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   ext_tgt_cfg),
		.ei_array      = qmi_wlanfw_ce_tgt_pipe_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   ext_svc_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   ext_svc_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLANFW_MAX_EXT_NUM_SVC_V01,
		.elem_size      = sizeof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   ext_svc_cfg),
		.ei_array      = qmi_wlanfw_ce_svc_pipe_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_wlan_cfg_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_mem_ready_ind_msg_v01_ei[] = {
	{
		.data_type = QMI_EOTI,
		.array_type = NO_ARRAY,
	},
};

static const struct qmi_elem_info qmi_wlanfw_fw_ready_ind_msg_v01_ei[] = {
	{
		.data_type = QMI_EOTI,
		.array_type = NO_ARRAY,
	},
};

struct qmi_elem_info wlfw_ini_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_ini_req_msg_v01,
					   enablefwlog_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_ini_req_msg_v01,
					   enablefwlog),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wlfw_ini_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_ini_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_wlan_ini_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_ini_req_msg_v01,
					   enable_fwlog_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_ini_req_msg_v01,
					   enable_fwlog),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_wlan_ini_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_wlan_ini_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info qmi_wlanfw_m3_dump_upload_req_ind_msg_v01_ei[] = {
        {
                .data_type = QMI_UNSIGNED_4_BYTE,
                .elem_len = 1,
                .elem_size = sizeof(u32),
                .array_type = NO_ARRAY,
                .tlv_type = 0x01,
                .offset = offsetof(struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01,
                                   pdev_id),
        },
        {
                .data_type = QMI_UNSIGNED_8_BYTE,
                .elem_len = 1,
                .elem_size = sizeof(u64),
                .array_type = NO_ARRAY,
                .tlv_type = 0x02,
                .offset = offsetof(struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01,
                                   addr),
        },
        {
                .data_type = QMI_UNSIGNED_8_BYTE,
                .elem_len = 1,
                .elem_size = sizeof(u64),
                .array_type = NO_ARRAY,
                .tlv_type = 0x03,
                .offset = offsetof(struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01,
                                   size),
        },
        {
                .data_type = QMI_EOTI,
                .array_type = NO_ARRAY,
                .tlv_type = QMI_COMMON_TLV_TYPE,
        },
};

static struct qmi_elem_info qmi_wlanfw_m3_dump_upload_done_req_msg_v01_ei[] = {
        {
                .data_type = QMI_UNSIGNED_4_BYTE,
                .elem_len = 1,
                .elem_size = sizeof(u32),
                .array_type = NO_ARRAY,
                .tlv_type = 0x01,
                .offset = offsetof(struct
                                   qmi_wlanfw_m3_dump_upload_done_req_msg_v01,
                                   pdev_id),
        },
        {
                .data_type = QMI_UNSIGNED_4_BYTE,
                .elem_len = 1,
                .elem_size = sizeof(u32),
                .array_type = NO_ARRAY,
                .tlv_type = 0x02,
                .offset = offsetof(struct
                                   qmi_wlanfw_m3_dump_upload_done_req_msg_v01,
                                   status),
        },
        {
                .data_type = QMI_EOTI,
                .array_type = NO_ARRAY,
                .tlv_type = QMI_COMMON_TLV_TYPE,
        },
};

static struct qmi_elem_info qmi_wlanfw_m3_dump_upload_done_resp_msg_v01_ei[] = {
        {
                .data_type = QMI_STRUCT,
                .elem_len = 1,
                .elem_size = sizeof(struct qmi_response_type_v01),
                .array_type = NO_ARRAY,
                .tlv_type = 0x02,
                .offset = offsetof(struct qmi_wlanfw_m3_dump_upload_done_resp_msg_v01,
                                   resp),
                .ei_array = qmi_response_type_v01_ei,
        },
        {
                .data_type = QMI_EOTI,
                .array_type = NO_ARRAY,
                .tlv_type = QMI_COMMON_TLV_TYPE,
        },
};

struct qmi_elem_info qmi_wlanfw_mem_read_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct qmi_wlanfw_mem_read_req_msg_v01,
					   offset),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct qmi_wlanfw_mem_read_req_msg_v01,
					   mem_type),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct qmi_wlanfw_mem_read_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_mem_read_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_read_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_read_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_read_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLANFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type     = VAR_LEN_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_read_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_mem_write_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_write_req_msg_v01,
					   offset),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_write_req_msg_v01,
					   mem_type),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_write_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLANFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type     = VAR_LEN_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_write_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_mem_write_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlanfw_mem_write_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info qmi_wlanfw_ext_fw_bin_mem_info_s_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum qmi_wlanfw_ext_fw_bin_type_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_mem_info_s_v01,
					   type),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_mem_info_s_v01,
					   addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_mem_info_s_v01,
					   size),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_mem_info_s_v01,
					   reserved1),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_mem_info_s_v01,
					   reserved2),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_ext_fw_bin_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_download_req_msg_v01,
					   ext_bin_mem_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLANFW_MAX_NUM_EXT_FW_BIN_DOWNLOAD_V01,
		.elem_size      = sizeof(struct qmi_wlanfw_ext_fw_bin_mem_info_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_download_req_msg_v01,
					   ext_bin_mem_info),
		.ei_array      = qmi_wlanfw_ext_fw_bin_mem_info_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_ext_fw_bin_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlanfw_ext_fw_bin_download_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_chip_state_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_chip_state_info_req_msg_v01,
					   partner_chip_state_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_chip_state_info_req_msg_v01,
					   partner_chip_state),
	},

	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},

};

struct qmi_elem_info qmi_wlanfw_chip_state_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlanfw_chip_state_info_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},

	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},

};

int ath12k_qmi_partner_chip_power_info_send(struct ath12k_base *ab, u8 power_state)
{
	struct qmi_wlanfw_chip_state_info_req_msg_v01 req = {};
	struct qmi_wlanfw_chip_state_info_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	int ret;

	req.partner_chip_state_valid = 1;
	req.partner_chip_state = power_state;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_chip_state_info_resp_msg_v01_ei,
			   &resp);
	if (ret < 0) {
		ath12k_warn(ab, "Failed to initialize QMI transaction: %d\n", ret);
		goto out;
	}

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLFW_PARTNER_CHIP_STATE_INFO_REQ_V01,
			       WLFW_PARTNER_CHIP_STATE_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_chip_state_info_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "Failed to send partner chip power state\n");
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));

	if (ret < 0) {
		ath12k_warn(ab, "QMI transaction timeout !\n");
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "Fail to notify power state result: %d err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	return ret;
}

int ath12k_qmi_mem_read(struct ath12k_base *ab, u32 mem_addr, void *mem_value,size_t count)
{
	struct qmi_wlanfw_mem_read_req_msg_v01 *req;
	struct qmi_wlanfw_mem_read_resp_msg_v01 *resp;
	struct qmi_txn txn = {};
	int ret;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->offset = mem_addr;

	/* Firmware uses mem type to map to various memory regions.
	 * If this is set to 0, firmware uses automatic mapping of regions.
	 * i.e, if mem address is given and mem_type is 0, firmware will
	 * find under which memory region that address belongs
	 */
	req->mem_type = QMI_MEM_REGION_TYPE;
	req->data_len = count;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_mem_read_resp_msg_v01_ei, resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_MEM_READ_REQ_V01,
			       QMI_WLANFW_MEM_READ_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_mem_read_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "Failed to send mem read request, err %d\n",
			    ret);

		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi mem read req failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -EINVAL;
		goto out;
	}

	if (!resp->data_valid || resp->data_len != req->data_len) {
		ath12k_warn(ab, "qmi mem read is invalid\n");
		ret = -EINVAL;
		goto out;
	}
	memcpy(mem_value, resp->data, resp->data_len);

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int ath12k_qmi_mem_write(struct ath12k_base *ab, u32 mem_addr, void* mem_value, size_t count)
{
	struct qmi_wlanfw_mem_write_req_msg_v01 *req;
	struct qmi_wlanfw_mem_write_resp_msg_v01 *resp;
	struct qmi_txn txn = {};
	int ret;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->offset = mem_addr;
	req->mem_type = QMI_MEM_REGION_TYPE;
	req->data_len = count;
	memcpy(req->data, mem_value, req->data_len);

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_mem_write_resp_msg_v01_ei, resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_MEM_WRITE_REQ_V01,
			       QMI_WLANFW_MEM_WRITE_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_mem_write_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "Failed to send mem write request, err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi mem write req failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	kfree(req);
	kfree(resp);
	return ret;
}

static inline bool ath12k_cold_boot_cal_needed(struct ath12k_base *ab)
{
	return (!ath12k_waltest_mode && !ab->early_cal_support &&
		ab->hw_params->cold_boot_calib && ath12k_cold_boot_cal &&
		ab->qmi.cal_done == 0);
}

struct qmi_elem_info qmi_wlanfw_mlo_reconfig_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_capable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_capable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_chip_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_chip_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_group_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_group_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   max_mlo_peer_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   max_mlo_peer),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_num_chips_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_num_chips),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_chip_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_CHIPS_V01,
		.elem_size      = sizeof(struct wlfw_host_mlo_chip_info_s_v01),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_chip_info),
		.ei_array      = wlfw_host_mlo_chip_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_chip_info_v2_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_CHIPS_V01,
		.elem_size      = sizeof(struct wlfw_host_mlo_chip_info_s_v02),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   qmi_wlanfw_mlo_reconfig_info_req_msg_v01,
					   mlo_chip_info_v2),
		.ei_array      = wlfw_host_mlo_chip_info_s_v02_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlfw_mlo_reconfig_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlfw_mlo_reconfig_info_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_cma_mem_seg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_cma_mem_seg_s_v01,
					   size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_cma_mem_seg_s_v01,
					   type),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_cma_mem_seg_s_v01,
					   mem_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_MEM_CFG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_cma_mem_seg_s_v01,
					   mem_cfg),
		.ei_array	= qmi_wlanfw_mem_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct
					   qmi_wlanfw_cma_mem_seg_s_v01,
					   persist),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_request_cma_mem_ind_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   qmi_wlanfw_request_cma_mem_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_cma_mem_seg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   qmi_wlanfw_request_cma_mem_ind_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_cma_mem_seg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_respond_cma_mem_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   qmi_wlanfw_respond_cma_mem_req_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_resp_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   qmi_wlanfw_respond_cma_mem_req_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_respond_cma_mem_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   qmi_wlanfw_respond_cma_mem_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};
static void ath12k_host_cap_hw_link_id_init(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab, *partner_ab;
	int i, j, hw_id_base;

	for (i = 0; i < ag->num_devices; i++) {
		hw_id_base = 0;
		ab = ag->ab[i];

		for (j = 0; j < ag->num_devices; j++) {
			partner_ab = ag->ab[j];

			if (partner_ab->wsi_info.index >= ab->wsi_info.index)
				continue;

			hw_id_base += partner_ab->qmi.num_radios;
		}

		ab->wsi_info.hw_link_id_base = hw_id_base;
		ab->bypass_wsi_info.hw_link_id_base = hw_id_base;
	}

	ag->hw_link_id_init_done = true;
}


static int ath12k_qmi_fill_adj_info(struct ath12k_base *ab,
				    struct wlfw_host_mlo_chip_info_s_v02 *info)
{
	struct wlfw_host_mlo_chip_info_s_v01 *adj_info;
	struct ath12k_base *adjacent_ab = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_wsi_info *wsi_info, *adj_wsi_info = NULL;
	u32 chip_idx;
	bool adj_ab_found = false;
	int i, j;
	int adj_index;

	wsi_info = ath12k_core_get_current_wsi_info(ab);
	for (i = 0; i < info->num_adj_chips; i++) {
		chip_idx = wsi_info->adj_chip_idxs[i];
		adj_info = &info->mlo_adj_chip_info[i];

		for (j = 0; j < ag->num_devices; j++) {
			adjacent_ab = ag->ab[j];
			adj_wsi_info = ath12k_core_get_current_wsi_info(adjacent_ab);
			if (adj_wsi_info->index == chip_idx) {
				adj_ab_found = true;
				break;
			}
		}

		if (!adj_ab_found) {
			ath12k_err(ab, "MLO adjacent ab for chip idx not found: %d\n", chip_idx);
			return -EINVAL;
		}

		adj_info->chip_id = adjacent_ab->device_id;
		adj_info->num_local_links = adjacent_ab->qmi.num_radios;

		ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO, "MLO adj chip id %d num_link %d\n",
			   adjacent_ab->device_id, adj_info->num_local_links);

		for (adj_index = 0; adj_index < adj_info->num_local_links; adj_index++) {
			adj_info->hw_link_id[adj_index] = adj_wsi_info->hw_link_id_base + adj_index;
			adj_info->valid_mlo_link_id[adj_index] = true;

			ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO, "MLO adj chip link id %d\n",
				   adj_info->hw_link_id[adj_index]);

		}
	}
	return 0;
}

static int ath12k_host_cap_parse_mlo(struct ath12k_base *ab,
				     struct qmi_wlanfw_host_cap_req_msg_v01 *req)
{
	//struct wlfw_host_mlo_chip_info_s_v01 *info;
	struct wlfw_host_mlo_chip_info_s_v02 *info;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_wsi_info *wsi_info, *adj_wsi_info;
	struct ath12k_base *partner_ab;
	u8 hw_link_id = 0;
	int i, j, ret;

	if (!ag->mlo_capable) {
		ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO,
			   "MLO is disabled hence skip QMI MLO cap");
		return 0;
	}

	wsi_info = ath12k_core_get_current_wsi_info(ab);
	if (ath12k_cold_boot_cal_needed(ab) && !ab->mm_cal_support &&
            ab->qmi.cal_timeout == 0) {
		ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO,
			   "Skip MLO cap send for device id %d since it's in cold_boot\n",
			   ab->device_id);
                return 0;
        }

	if (!ab->qmi.num_radios || ab->qmi.num_radios == U8_MAX) {
		ag->mlo_capable = false;
		ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO,
			   "skip QMI MLO cap due to invalid num_radio %d\n",
			   ab->qmi.num_radios);
		return 0;
	}

	if (ab->device_id == ATH12K_INVALID_DEVICE_ID) {
		ath12k_err(ab, "failed to send MLO cap due to invalid device id\n");
		return -EINVAL;
	}

	req->mlo_capable_valid = 1;
	req->mlo_capable = 1;
	req->mlo_chip_id_valid = 1;
	req->mlo_chip_id = ab->device_id;
	req->mlo_group_id_valid = 1;
	req->mlo_group_id = ag->id;
	req->max_mlo_peer_valid = 1;
	/* Max peer number generally won't change for the same device
	 * but needs to be synced with host driver.
	 */
	req->max_mlo_peer = ab->hw_params->max_mlo_peer;
	req->mlo_num_chips_valid = 1;
	req->mlo_num_chips = ag->num_devices;

	ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO,
		   "mlo capability advertisement device_id %d group_id %d num_devices %d",
		   req->mlo_chip_id, req->mlo_group_id, req->mlo_num_chips);

	mutex_lock(&ag->mutex);

	if (!ag->hw_link_id_init_done)
		ath12k_host_cap_hw_link_id_init(ag);

	for (i = 0; i < ag->num_devices; i++) {
		info = &req->mlo_chip_info_v2[i];
		partner_ab = ag->ab[i];
		adj_wsi_info = ath12k_core_get_current_wsi_info(partner_ab);

		if (partner_ab->device_id == ATH12K_INVALID_DEVICE_ID ||
			partner_ab->qmi.num_radios > QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01) {
			ath12k_err(ab, "failed to send MLO cap due to invalid partner device id\n");
			ret = -EINVAL;
			goto device_cleanup;
		}

		info->mlo_chip_info.chip_id = partner_ab->device_id;
		info->mlo_chip_info.num_local_links = partner_ab->qmi.num_radios;
		info->num_adj_chips = wsi_info->num_adj_chips;

		ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO, "mlo device id %d num_link %d\n",
			   info->mlo_chip_info.chip_id,
			   info->mlo_chip_info.num_local_links);

		for (j = 0; j < info->mlo_chip_info.num_local_links; j++) {
			info->mlo_chip_info.hw_link_id[j] = adj_wsi_info->hw_link_id_base + j;
			info->mlo_chip_info.valid_mlo_link_id[j] = 1; //true

			ath12k_dbg(ab, ATH12K_DBG_QMI | ATH12K_DBG_MLO, "mlo hw_link_id %d\n",
				   info->mlo_chip_info.hw_link_id[j]);

			hw_link_id++;
		}

		ret = ath12k_qmi_fill_adj_info(partner_ab, info);
		if (ret < 0) {
			ath12k_err(ab, "failed to update adjacent information\n");
			goto device_cleanup;
		}
	}

	if (hw_link_id <= 0)
		ag->mlo_capable = false;

	req->mlo_chip_info_v2_valid = 1;

	mutex_unlock(&ag->mutex);

	return 0;

device_cleanup:
	for (i = i - 1; i >= 0; i--) {
		info = &req->mlo_chip_info_v2[i];

		memset(info, 0, sizeof(*info));
	}

	req->mlo_num_chips = 0;
	req->mlo_num_chips_valid = 0;

	req->max_mlo_peer = 0;
	req->max_mlo_peer_valid = 0;
	req->mlo_group_id = 0;
	req->mlo_group_id_valid = 0;
	req->mlo_chip_id = 0;
	req->mlo_chip_id_valid = 0;
	req->mlo_capable = 0;
	req->mlo_capable_valid = 0;

	ag->mlo_capable = false;

	mutex_unlock(&ag->mutex);

	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_host_cap_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_host_cap_req_msg_v01 req = {};
	struct qmi_wlanfw_host_cap_resp_msg_v01 resp = {};
	struct ath12k_board_data bd;
	struct qmi_txn txn;
	int ret = 0;
	struct device_node *root;
	const char *model = NULL;
	u64 iova_start = 0, iova_size = 0;

	req.num_clients_valid = 1;
	req.num_clients = 1;
	req.mem_cfg_mode = ab->qmi.target_mem_mode;
	req.mem_cfg_mode_valid = 1;
	req.bdf_support_valid = 1;
	req.bdf_support = 1;

	if (ab->hw_params->fw.m3_loader == ath12k_m3_fw_loader_driver) {
		req.m3_support_valid = 1;
		req.m3_support = 1;
		req.m3_cache_support_valid = 1;
		req.m3_cache_support = 1;
	}

	req.cal_done_valid = 1;
	req.cal_done = ab->qmi.cal_done;

	if (ab->hw_params->send_platform_model) {
                root = of_find_node_by_path("/");
                if (root) {
                        model = of_get_property(root, "model", NULL);
                        if (model) {
                                req.platform_name_valid = 1;
                                strncpy(req.platform_name, model,
                                        QMI_WLANFW_MAX_PLATFORM_NAME_LEN_V01);
                                ath12k_info(ab, "Platform name: %s", req.platform_name);
                        }
                        of_node_put(root);
                }
        }

	if (!ath12k_hif_get_iova(ab, &iova_start, &iova_size)) {
		req.ddr_range_valid = 1;
		req.ddr_range[0].start = iova_start;
		req.ddr_range[0].size = iova_size;
		ath12k_info(ab, "Sending iova starting 0x%llx with size 0x%llx\n",
			    req.ddr_range[0].start, req.ddr_range[0].size);
	}

	if (ab->hw_params->qmi_cnss_feature_bitmap) {
		req.feature_list_valid = 1;
		req.feature_list = ab->hw_params->qmi_cnss_feature_bitmap;
	}

	/* BRINGUP: here we are piggybacking a lot of stuff using
	 * internal_sleep_clock, should it be split?
	 */
	if (ab->hw_params->internal_sleep_clock) {
		req.nm_modem_valid = 1;

		/* Notify firmware that this is non-qualcomm platform. */
		req.nm_modem |= HOST_CSTATE_BIT;

		/* Notify firmware about the sleep clock selection,
		 * nm_modem_bit[1] is used for this purpose. Host driver on
		 * non-qualcomm platforms should select internal sleep
		 * clock.
		 */
		req.nm_modem |= SLEEP_CLOCK_SELECT_INTERNAL_BIT;
		req.nm_modem |= PLATFORM_CAP_PCIE_GLOBAL_RESET;
	}
	

	ret = ath12k_core_fetch_fw_cfg(ab, &bd);
	if (!ret) {
		req.fw_cfg_support_valid = 1;
		req.fw_cfg_support = 1;
	}
	ab->fw_cfg_support = !!req.fw_cfg_support;
	ath12k_core_free_bdf(ab, &bd);

	ret = ath12k_host_cap_parse_mlo(ab, &req);
	if (ret < 0)
		goto out;

	if (ab->qmi.dynamic_ddr_support) {
		req.dynamic_mem_support_valid = 1;
		req.dynamic_mem_support = 1;
	}

#ifdef ATH12K_CMA_SUPPORT
	if (ab->qmi_mem_dev.rmem_inited) {
		req.cma_support_valid = 1;
		req.cma_support = 1;
	}
#endif

	if (ath12k_ftm_mode) {
		req.ftm_mode_valid = 1;
		req.ftm_mode = 1;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_host_cap_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_HOST_CAP_REQ_V01,
			       QMI_WLANFW_HOST_CAP_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_host_cap_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "Failed to send host capability request,err = %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "Host capability request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static int ath12k_qmi_mlo_reconfig_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_mlo_reconfig_info_req_msg_v01 req = {};
	struct qmi_wlfw_mlo_reconfig_info_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	struct wlfw_host_mlo_chip_info_s_v02 *info;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_wsi_info *wsi_info, *adj_wsi_info;
	struct ath12k_base *partner_ab;
	int i, j, k = 0, ret;

	if (!ag->mlo_capable) {
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS | ATH12K_DBG_MLO,
			   "MLO is disabled hence skip QMI MLO Reconfig");
		return 0;
	}

	wsi_info = ath12k_core_get_current_wsi_info(ab);
	if (ath12k_cold_boot_cal_needed(ab) && !ab->mm_cal_support &&
	    ab->qmi.cal_timeout == 0) {
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS | ATH12K_DBG_MLO,
			   "Skip MLO Reconfig send for device id %d since it's in cold_boot\n",
			   ab->device_id);
		return 0;
	}

	if (!ab->qmi.num_radios || ab->qmi.num_radios == U8_MAX) {
		ag->mlo_capable = false;
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS | ATH12K_DBG_MLO,
			   "skip QMI MLO Reconfi due to invalid num_radio %d\n",
			   ab->qmi.num_radios);
		return 0;
	}

	if (ab->device_id == ATH12K_INVALID_DEVICE_ID) {
		ath12k_err(ab, "failed to send MLO Reconfig due to invalid device id\n");
		return -EINVAL;
	}

	req.mlo_capable_valid = 1;
	req.mlo_capable = 1;
	req.mlo_chip_id_valid = 1;
	req.mlo_chip_id = ab->device_id;
	req.mlo_group_id_valid = 1;
	req.mlo_group_id = ag->id;
	req.max_mlo_peer_valid = 1;
	/* Max peer number generally won't change for the same device
	 * but needs to be synced with host driver.
	 */
	req.max_mlo_peer = ab->hw_params->max_mlo_peer;
	req.mlo_num_chips_valid = 1;
	/* In MLO reconfig, mlo_num_chips should indicate only active chips */
	req.mlo_num_chips = ag->num_devices - ag->num_bypassed;
	req.mlo_chip_info_valid = 0;

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS | ATH12K_DBG_MLO,
		   "mlo reconfig device_id %d group_id %d num_devices %d",
		   req.mlo_chip_id, req.mlo_group_id, req.mlo_num_chips);

	if (!ag->hw_link_id_init_done)
		ath12k_host_cap_hw_link_id_init(ag);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed)
			continue;

		if (partner_ab->device_id == ATH12K_INVALID_DEVICE_ID) {
			ath12k_err(ab, "failed to send MLO cap due to invalid partner device id\n");
			ret = -EINVAL;
			goto out;
		}
		adj_wsi_info = ath12k_core_get_current_wsi_info(partner_ab);
		info = &req.mlo_chip_info_v2[k++];
		info->mlo_chip_info.chip_id = partner_ab->device_id;
		info->mlo_chip_info.num_local_links = partner_ab->qmi.num_radios;
		info->num_adj_chips = wsi_info->num_adj_chips;

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS | ATH12K_DBG_MLO, "mlo device id %d num_link %d\n",
			   info->mlo_chip_info.chip_id,
			   info->mlo_chip_info.num_local_links);

		for (j = 0; j < info->mlo_chip_info.num_local_links; j++) {
			info->mlo_chip_info.hw_link_id[j] = adj_wsi_info->hw_link_id_base + j;
			info->mlo_chip_info.valid_mlo_link_id[j] = 1;

			ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS | ATH12K_DBG_MLO, "mlo hw_link_id %d\n",
				   info->mlo_chip_info.hw_link_id[j]);
		}

		ret = ath12k_qmi_fill_adj_info(partner_ab, info);
		if (ret < 0) {
			ath12k_err(ab, "failed to update adjacent information\n");
			goto out;
		}
	}

	req.mlo_chip_info_v2_valid = 1;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlfw_mlo_reconfig_info_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLFW_MLO_RECONFIG_INFO_REQ_V01,
			       WLFW_MLO_RECONFIG_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_mlo_reconfig_info_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "Failed to send MLO reconfig request,err = %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "MLO reconfig request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static void ath12k_qmi_phy_cap_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_phy_cap_req_msg_v01 req = {};
	struct qmi_wlanfw_phy_cap_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	const char *mm_cal_prop;
	int ret;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_phy_cap_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_PHY_CAP_REQ_V01,
			       QMI_WLANFW_PHY_CAP_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_phy_cap_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "failed to send phy capability request: %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!resp.num_phy_valid) {
		ret = -ENODATA;
		goto out;
	}

	/* if early-cal fails in u-boot, qcom,mm_cal_support will be enabled
	 * dynamically through u-boot
	 */

	ret = of_property_read_string(ab->dev->of_node, "qcom,mm_cal_support",
				     &mm_cal_prop);

	if (!ret && !strcmp(mm_cal_prop, "disabled")) {
		ab->mm_cal_support = false;
	} else if (ath12k_cold_boot_cal && resp.mm_coldboot_cal_valid &&
			resp.mm_coldboot_cal) {
		ab->mm_cal_support = true;
	}

	ab->qmi.num_radios = resp.num_phy;
	if (resp.aux_support_valid)
		ab->qmi.aux_support = !!resp.aux_support;
	if (resp.mcss_support_valid)
		ab->qmi.mcss_support = !!resp.mcss_support;

	if (resp.dynamic_ddr_support_valid)
		ab->qmi.dynamic_ddr_support = resp.dynamic_ddr_support;

	ath12k_dbg(ab, ATH12K_DBG_QMI,
		   "phy capability resp valid %d num_phy %d valid %d board_id %d, valid %u aux support %u valid %u mcss support %u dynamic ddr support %d\n",
		   resp.num_phy_valid, resp.num_phy,
		   resp.board_id_valid, resp.board_id, resp.aux_support_valid,
		   resp.aux_support, resp.mcss_support_valid, resp.mcss_support,
		   resp.dynamic_ddr_support);

	return;

out:
	/* If PHY capability not advertised then rely on default num link */
	ab->qmi.num_radios = ab->hw_params->def_num_link;

	ath12k_dbg(ab, ATH12K_DBG_QMI,
		   "no valid response from PHY capability, choose default num_phy %d\n",
		   ab->qmi.num_radios);
}

static int ath12k_qmi_fw_ind_register_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_ind_register_req_msg_v01 *req;
	struct qmi_wlanfw_ind_register_resp_msg_v01 *resp;
	struct qmi_handle *handle = &ab->qmi.handle;
	struct qmi_txn txn;
	int ret;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto resp_out;
	}

	req->client_id_valid = 1;
	req->client_id = QMI_WLANFW_CLIENT_ID;
	req->fw_ready_enable_valid = 1;
	req->fw_ready_enable = 1;
	req->request_mem_enable_valid = 1;
	req->request_mem_enable = 1;
	req->fw_mem_ready_enable_valid = 1;
	req->fw_mem_ready_enable = 1;
	req->cal_done_enable_valid = 1;
	req->cal_done_enable = 1;
	req->fw_init_done_enable_valid = 1;
	req->fw_init_done_enable = 1;
	req->qdss_trace_req_mem_enable_valid = 1;
	req->qdss_trace_req_mem_enable = 1;
	req->qdss_trace_save_enable_valid = 1;
	req->qdss_trace_save_enable = 1;
	req->qdss_trace_free_enable_valid = 1;
	req->m3_dump_upload_req_enable_valid = 1;
	req->m3_dump_upload_req_enable = 1;
	req->ddr_dump_region_enable_valid = 1;
	req->ddr_dump_region_enable = 1;
	req->request_mem_free_valid = 1;
	req->request_mem_free = 1;
	req->request_cma_mem_enable_valid = 1;
	req->request_cma_mem_enable = 1;

	req->pin_connect_result_enable_valid = 0;
	req->pin_connect_result_enable = 0;

	ret = qmi_txn_init(handle, &txn,
			   qmi_wlanfw_ind_register_resp_msg_v01_ei, resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_IND_REGISTER_REQ_V01,
			       QMI_WLANFW_IND_REGISTER_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_ind_register_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "Failed to send indication register request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "failed to register fw indication %d\n", ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "FW Ind register request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	kfree(resp);
resp_out:
	kfree(req);
	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_respond_fw_mem_request(struct ath12k_base *ab, void *data)
{
	struct qmi_wlanfw_respond_mem_req_msg_v01 *req;
	struct qmi_wlanfw_respond_mem_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	int ret = 0, i = 0;
	bool delayed;
	int idx, seg_start_idx, *curr_seg_cnt = data;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	/* Some targets by default request a block of big contiguous
	 * DMA memory, it's hard to allocate from kernel. So host returns
	 * failure to firmware and firmware then request multiple blocks of
	 * small chunk size memory.
	 */
	if (!test_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags) &&
	    ab->qmi.target_mem_delayed) {
		delayed = true;
		ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi delays mem_request %d\n",
			   ab->qmi.mem_seg_count);
	} else {
		delayed = false;
		req->mem_seg_len = *curr_seg_cnt;
		seg_start_idx = ab->qmi.mem_seg_count - *curr_seg_cnt;
		for (idx = 0, i = seg_start_idx; i < ab->qmi.mem_seg_count; i++) {
			req->mem_seg[idx].addr = ab->qmi.target_mem[i].paddr;
			req->mem_seg[idx].size = ab->qmi.target_mem[i].size;
			req->mem_seg[idx].type = ab->qmi.target_mem[i].type;
			ath12k_dbg(ab, ATH12K_DBG_QMI,
				   "qmi req mem_seg[%d] %pad %u %u\n", idx,
				   &ab->qmi.target_mem[i].paddr,
				   ab->qmi.target_mem[i].size,
				   ab->qmi.target_mem[i].type);
			idx++;
		}
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_respond_mem_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_RESPOND_MEM_REQ_V01,
			       QMI_WLANFW_RESPOND_MEM_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_respond_mem_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to respond memory request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed memory request, err = %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		/* the error response is expected when
		 * target_mem_delayed is true.
		 */
		if (delayed && resp.resp.error == 0)
			goto out;

		ath12k_warn(ab, "Respond mem req failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	kfree(req);
	return ret;
}

void ath12k_qmi_reset_mlo_mem(struct ath12k_hw_group *ag)
{
	struct target_mem_chunk *mlo_chunk;
	int i;

	lockdep_assert_held(&ag->mutex);

	if (!ag->mlo_mem.init_done || ag->num_started)
		return;

	for (i = 0; i < ARRAY_SIZE(ag->mlo_mem.chunk); i++) {
		mlo_chunk = &ag->mlo_mem.chunk[i];

		if (mlo_chunk->v.addr)
		/* TODO: Mode 0 recovery is the default mode hence resetting the
		 * whole memory region for now. Once Mode 1 support is added, this
		 * needs to be handled properly
		 */
		memset(mlo_chunk->v.addr, 0, mlo_chunk->size);
	}
}

/**
 * ath12k_free_mlo_glb_per_device_crash_info() - Free MLO per_device crash info
 * @snapshot_info: Pointer to MLO Global crash info
 *
 * Return: None
 */
static void ath12k_free_mlo_glb_per_device_crash_info(
	struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info)
{
	if (global_device_crash_info->per_device_crash_info) {
		kfree(global_device_crash_info->per_device_crash_info);
		global_device_crash_info->per_device_crash_info = NULL;
	}
}

#if defined(ATH12K_CMA_SUPPORT)
static void ath12k_memdev_release(struct device *dev)
{
	/* No-op release callback required by the device core. */
}

int ath12k_memdev_init(struct ath12k_base *ab, struct ath12k_mem_dev *mem_dev,
		       const char *name)
{
	struct device *dev = &mem_dev->dev;
	struct device_node *rmem_node;
	int ret, idx;

	memset(dev, 0, sizeof(*dev));

	device_initialize(dev);
	dev->parent = ab->dev;
	dev->release = ath12k_memdev_release;
	dev->of_node = of_node_get(ab->dev->of_node);
	dev_set_name(dev, "ath12k_%s.%s", name, dev_name(ab->dev));

	ret = dma_coerce_mask_and_coherent(dev, dma_get_mask(ab->dev));
	if (ret)
		goto err_put;

	ret = device_add(dev);
	if (ret)
		goto err_put;

	mem_dev->dev_registered = true;

	idx = of_property_match_string(dev->of_node, "memory-region-names", name);
	if (idx < 0) {
		ret = idx;
		goto err_del;
	}

	rmem_node = of_parse_phandle(dev->of_node, "memory-region", idx);
	if (!rmem_node || !of_property_read_bool(rmem_node, "reusable")) {
		if (rmem_node)
			of_node_put(rmem_node);
		ath12k_dbg(ab, ATH12K_DBG_QMI,
			   "reserved mem '%s' is not a shared CMA pool, skipping\n",
			   name);
		goto not_cma;
	}
	of_node_put(rmem_node);

	ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, idx);
	if (ret)
		goto err_del;

	mem_dev->rmem_inited = true;
	return 0;

not_cma:
	ret = 0;
err_del:
	device_del(dev);
	mem_dev->dev_registered = false;
err_put:
	of_node_put(dev->of_node);
	dev->of_node = NULL;
	put_device(dev);
	return ret;
}
EXPORT_SYMBOL(ath12k_memdev_init);

void ath12k_memdev_deinit(struct ath12k_base *ab, struct ath12k_mem_dev *mem_dev)
{
	struct device *dev = &mem_dev->dev;

	if (mem_dev->rmem_inited) {
		of_reserved_mem_device_release(dev);
		mem_dev->rmem_inited = false;
	}

	if (mem_dev->dev_registered) {
		device_del(dev);
		mem_dev->dev_registered = false;
	}

	if (dev->of_node) {
		of_node_put(dev->of_node);
		dev->of_node = NULL;
	}

	put_device(dev);
}
EXPORT_SYMBOL(ath12k_memdev_deinit);
#endif

static void ath12k_qmi_free_mlo_mem_chunk(struct ath12k_base *ab,
					  struct target_mem_chunk *chunk,
					  int idx)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_host_mlo_mem_arena *mlomem_arena_ctx = &ag->mlomem_arena;
	struct target_mem_chunk *mlo_chunk;
	bool fixed_mem;
	struct device *dev = ab->mlo_mem_dev.rmem_inited ?
			     &ab->mlo_mem_dev.dev : ab->dev;

	lockdep_assert_held(&ag->mutex);

	if (!ag->mlo_mem.init_done || ag->num_started)
		return;

	if (idx >= ARRAY_SIZE(ag->mlo_mem.chunk)) {
		ath12k_warn(ab, "invalid index for MLO memory chunk free: %d\n", idx);
		return;
	}

	fixed_mem = test_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags);
	mlo_chunk = &ag->mlo_mem.chunk[idx];

	if (fixed_mem && mlo_chunk->v.ioaddr) {
#ifdef PLATFORM_SDX
		dma_free_attrs(dev,
			       mlo_chunk->size,
			       mlo_chunk->v.ioaddr,
			       mlo_chunk->paddr,
			       DMA_ATTR_FORCE_CONTIGUOUS);
#else
		if (ab->mlo_mem_dev.rmem_inited) {
			dma_free_coherent(dev,
					  mlo_chunk->size,
					  mlo_chunk->v.ioaddr,
					  mlo_chunk->paddr);
		} else {
			iounmap(mlo_chunk->v.ioaddr);
		}
#endif
		mlo_chunk->v.ioaddr = NULL;
	} else if (mlo_chunk->v.addr) {
		dma_free_coherent(dev,
				  mlo_chunk->size,
				  mlo_chunk->v.addr,
				  mlo_chunk->paddr);
		mlo_chunk->v.addr = NULL;
	}

	mlo_chunk->paddr = 0;
	mlo_chunk->size = 0;
	ag->mlo_mem.is_mlo_mem_avail = false;
	if (fixed_mem)
		chunk->v.ioaddr = NULL;
	else
		chunk->v.addr = NULL;

	chunk->paddr = 0;
	chunk->size = 0;

	if (mlomem_arena_ctx->init_done) {
		mlomem_arena_ctx->init_done = false;
		ath12k_free_mlo_glb_per_device_crash_info
			(&mlomem_arena_ctx->global_device_crash_info);
	}
}

void ath12k_qmi_free_target_mem_chunk(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int i, mlo_idx;
	struct device *dev;

	for (i = 0, mlo_idx = 0; i < ab->qmi.mem_seg_count; i++) {
		if (ab->qmi.target_mem[i].type == MLO_GLOBAL_MEM_REGION_TYPE) {
			if (ab->is_bypassed)
				continue;

			ath12k_qmi_free_mlo_mem_chunk(ab,
						      &ab->qmi.target_mem[i],
						      mlo_idx++);
		} else {
			dev = ab->qmi_mem_dev.rmem_inited ?
			      &ab->qmi_mem_dev.dev : ab->dev;
			if (test_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags) &&
			    ab->qmi.target_mem[i].v.ioaddr) {
#ifdef PLATFORM_SDX
				if (ab->qmi.target_mem[i].type == AFC_REGION_TYPE &&
				    ab->qmi.target_mem[i].v.addr) {
					dma_free_coherent(dev,
							  ab->qmi.target_mem[i].size,
							  ab->qmi.target_mem[i].v.addr,
							  ab->qmi.target_mem[i].paddr);
					ab->qmi.target_mem[i].v.addr = NULL;
				} else {
					dma_free_attrs(dev,
						       ab->qmi.target_mem[i].size,
						       ab->qmi.target_mem[i].v.ioaddr,
						       ab->qmi.target_mem[i].paddr,
						       DMA_ATTR_FORCE_CONTIGUOUS);
					ab->qmi.target_mem[i].v.ioaddr = NULL;
				}
#else
				if (ab->qmi.target_mem[i].type == AFC_REGION_TYPE &&
				     ab->hif.bus != ATH12K_BUS_HYBRID &&
				     ab->qmi.target_mem[i].v.addr) {
					dma_free_coherent(dev,
							  ab->qmi.target_mem[i].size,
							  ab->qmi.target_mem[i].v.addr,
							  ab->qmi.target_mem[i].paddr);
					ab->qmi.target_mem[i].v.addr = NULL;
				} else {
					iounmap(ab->qmi.target_mem[i].v.ioaddr);
					ab->qmi.target_mem[i].v.ioaddr = NULL;
				}
#endif
			} else {
				if (!ab->qmi.target_mem[i].v.addr)
					continue;
				/* Park persistent chunks into the pool so they
				 * survive recovery and cold-boot power cycles.
				 * ath12k_qmi_alloc_target_mem_chunk() will reuse
				 * them on the next FW mem request, matched by type
				 * and size regardless of request order.
				 */
				if (ab->qmi.target_mem[i].persist) {
					if (ab->qmi.persist_mem_count >=
					    ATH12K_QMI_MAX_PERSIST_MEM_CHUNKS) {
						ath12k_warn(ab, "qmi persist pool full, freeing chunk type %u size %u\n",
							    ab->qmi.target_mem[i].type,
							    ab->qmi.target_mem[i].size);
						dma_free_coherent(dev,
							ab->qmi.target_mem[i].prev_size,
							ab->qmi.target_mem[i].v.addr,
							ab->qmi.target_mem[i].paddr);
						ab->qmi.target_mem[i].v.addr = NULL;
						continue;
					}

					ab->qmi.persist_mem[ab->qmi.persist_mem_count++] =
						ab->qmi.target_mem[i];
					ab->qmi.target_mem[i].v.addr = NULL;
					continue;
				}
				dma_free_coherent(dev,
						  ab->qmi.target_mem[i].prev_size,
						  ab->qmi.target_mem[i].v.addr,
						  ab->qmi.target_mem[i].paddr);
				ab->qmi.target_mem[i].v.addr = NULL;
			}
		}
	}

	ab->qmi.mem_seg_count = 0;
	ab->host_ddr_fixed_mem_off = 0;

	if (!ag->num_started && ag->mlo_mem.init_done) {
		ag->mlo_mem.init_done = false;
		ag->mlo_mem.mlo_mem_size = 0;
	}
}

static int ath12k_qmi_alloc_chunk(struct ath12k_base *ab,
				  struct target_mem_chunk *chunk,
				  struct device *dev)
{
#ifndef ATH12K_CMA_SUPPORT
	/* Firmware reloads in recovery/resume.
	 * In such cases, no need to allocate memory for FW again.
	 */
	if (chunk->v.addr) {
		if (chunk->prev_type == chunk->type &&
		    chunk->prev_size == chunk->size)
			return 0;

		/* cannot reuse the existing chunk */
		dma_free_coherent(ab->dev, chunk->prev_size,
				  chunk->v.addr, chunk->paddr);
		chunk->v.addr = NULL;
	}
#endif

	chunk->v.addr = dma_alloc_coherent(dev,
					   chunk->size,
					   &chunk->paddr,
					   GFP_KERNEL);
	if (!chunk->v.addr) {
		if (!ab->qmi_mem_dev.rmem_inited &&
		    chunk->size > ATH12K_QMI_MAX_CHUNK_SIZE) {
			ab->qmi.target_mem_delayed = true;
			ath12k_warn(ab,
				    "qmi dma allocation failed (%d B type %u), will try later with small size\n",
				    chunk->size,
				    chunk->type);
			return -EAGAIN;
		}
		ath12k_warn(ab, "memory allocation failure for %u size: %d\n",
			    chunk->type, chunk->size);
		return -ENOMEM;
	}
	chunk->prev_type = chunk->type;
	chunk->prev_size = chunk->size;
	return 0;
}

static void ath12k_qmi_free_target_mem_chunk_range(struct ath12k_base *ab,
						   u32 start_idx, u32 end_idx)
{
	struct device *dev = ab->qmi_mem_dev.rmem_inited ?
			      &ab->qmi_mem_dev.dev : ab->dev;
	u32 i;

	for (i = start_idx; i < end_idx; i++) {
		struct target_mem_chunk *chunk = &ab->qmi.target_mem[i];

		if (!chunk->paddr)
			goto clear_entry;

		if (chunk->type != MLO_GLOBAL_MEM_REGION_TYPE ||
		    !ab->ag->num_started) {
			if (chunk->v.ioaddr)
				dma_free_coherent(dev, chunk->size,
						  chunk->v.addr, chunk->paddr);
		}

clear_entry:
		memset(chunk, 0, sizeof(*chunk));
	}
}

static void ath12k_qmi_msg_mem_free_request_cb(struct qmi_handle *qmi_hdl,
					       struct sockaddr_qrtr *sq,
					       struct qmi_txn *txn,
					       const void *data)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;
	struct ath12k_hw_group *ag = ab->ag;
	const struct qmi_wlanfw_request_mem_free_ind_msg_v01 *msg = data;
	int i;
	struct device *dev = ab->qmi_mem_dev.rmem_inited ?
		&ab->qmi_mem_dev.dev : ab->dev;
	struct target_mem_chunk *chunk;

	ath12k_dbg(ab, ATH12K_DBG_QMI,
		   "qmi dynamic free req type %u size %u addr %pad\n",
		   msg->mem_seg.type, msg->mem_seg.size, &msg->mem_seg.addr);

	mutex_lock(&ag->mutex);
	for (i = 0; i < qmi->mem_seg_count; i++) {
		chunk = &ab->qmi.target_mem[i];
		if (chunk->type != (u32)msg->mem_seg.type ||
		    chunk->size != msg->mem_seg.size ||
		    chunk->paddr != msg->mem_seg.addr)
			continue;

		switch (ab->qmi.target_mem[i].type) {
		case QMI_WLANFW_MEM_TYPE_DDR_V01:
		case QMI_WLANFW_MEM_BDF_V01:
		case QMI_WLANFW_MEM_M3_V01:
		case QMI_WLANFW_MEM_CAL_V01:
		case QMI_WLANFW_PAGEABLE_MEM_V01:
		case QMI_WLANFW_AFC_MEM_V01:
			ath12k_dbg(ab, ATH12K_DBG_QMI,
				   "qmi firmware dynamic memory free request for type: %u, size:%u, pa: %pa, va: %p\n",
				   chunk->type, chunk->size,
				   &chunk->paddr, chunk->v.ioaddr);
			if (chunk->v.ioaddr)
				dma_free_coherent(dev, chunk->size,
						  chunk->v.ioaddr,
						  chunk->paddr);
			if (i + 1 < qmi->mem_seg_count)
				memmove(&ab->qmi.target_mem[i],
					&ab->qmi.target_mem[i + 1],
					((qmi->mem_seg_count - i - 1) *
					sizeof(struct target_mem_chunk)));
			memset(&ab->qmi.target_mem[qmi->mem_seg_count - 1],
			       0,
			       sizeof(struct target_mem_chunk));
			qmi->mem_seg_count--;
			goto unlock;
		case QMI_WLANFW_MLO_GLOBAL_MEM_V01:
		default:
			ath12k_dbg(ab, ATH12K_DBG_QMI, "Invalid dynamic memory request type : %d\n",
				   msg->mem_seg.type);
			goto unlock;
		}
	}
unlock:
	mutex_unlock(&ag->mutex);
}

static int ath12k_qmi_alloc_target_mem_chunk(struct ath12k_base *ab,
					     struct target_mem_chunk *req_mem,
					     u32 req_mem_seg_count)
{
	struct target_mem_chunk *chunk, *mlo_chunk;
	struct ath12k_hw_group *ag = ab->ag;
	int i, j, mlo_idx, ret;
	int mlo_size = 0;
	int old_seg_cnt = ab->qmi.mem_seg_count;
	int seg_cnt = old_seg_cnt;
	bool from_pool;
	struct device *dev;

	mutex_lock(&ag->mutex);

	if (!ag->mlo_mem.init_done) {
		memset(ag->mlo_mem.chunk, 0, sizeof(ag->mlo_mem.chunk));
		ag->mlo_mem.init_done = true;
	}
	if (old_seg_cnt + req_mem_seg_count > ATH12K_QMI_MAX_MEM_CHUNKS) {
		ret = -ENOSPC;
		goto err;
	}

	ab->qmi.target_mem_delayed = false;

	for (i = 0, mlo_idx = 0; i < req_mem_seg_count; i++) {
		chunk = &ab->qmi.target_mem[seg_cnt];
		chunk->type = req_mem[i].type;
		chunk->size = req_mem[i].size;
		chunk->persist = req_mem[i].persist;

		/* Allocate memory for the region and the functionality supported
		 * on the host. For the non-supported memory region, host does not
		 * allocate memory, assigns NULL and FW will handle this without crashing.
		 */
		switch (chunk->type) {
		case HOST_DDR_REGION_TYPE:
		case M3_DUMP_REGION_TYPE:
		case AFC_REGION_TYPE:
		case PAGEABLE_MEM_REGION_TYPE:
		case CALDB_MEM_REGION_TYPE:
		case BDF_MEM_REGION_TYPE:
			if ((chunk->type == CALDB_MEM_REGION_TYPE &&
			     !ab->hw_params->cold_boot_calib) ||
			     !chunk->size) {
				chunk->paddr = 0;
				chunk->v.addr = NULL;
				seg_cnt++;
				break;
			}
			/* Search the persistent pool before allocating. Pool
			 * entries are parked across recovery and cold-boot power
			 * cycles. Match on type and size — request order may
			 * differ between boots so slot position is unreliable.
			 */
			from_pool = false;
			for (j = 0; j < ab->qmi.persist_mem_count; j++) {
				struct target_mem_chunk *c =
					&ab->qmi.persist_mem[j];

				if (c->type != chunk->type ||
				    c->size != chunk->size)
					continue;
				/* Transplant into the active table */
				*chunk = *c;
				/* Compact the pool */
				if (j + 1 < ab->qmi.persist_mem_count)
					ab->qmi.persist_mem[j] =
						ab->qmi.persist_mem[
							ab->qmi.persist_mem_count - 1];
				memset(&ab->qmi.persist_mem[
						ab->qmi.persist_mem_count - 1],
				       0, sizeof(*c));
				ab->qmi.persist_mem_count--;
				ath12k_dbg(ab, ATH12K_DBG_QMI,
					   "qmi reusing persist pool entry type %u size %u paddr %pad\n",
					   chunk->type, chunk->size,
					   &chunk->paddr);
				from_pool = true;
				break;
			}
			if (!from_pool) {
				dev = ab->qmi_mem_dev.rmem_inited ?
				      &ab->qmi_mem_dev.dev : ab->dev;
				ret = ath12k_qmi_alloc_chunk(ab, chunk, dev);
				if (ret)
					goto err;
				/* Mark chunk persistent if it's a caldb segment
				 * or if FW explicitly requested persistence
				 * (e.g. via the CMA mem indication persist flag).
				 */
				if (chunk->type == CALDB_MEM_REGION_TYPE ||
				    chunk->persist)
					chunk->persist = true;
			}
			seg_cnt++;
			break;
		case MLO_GLOBAL_MEM_REGION_TYPE:
			if (!chunk->size) {
				chunk->paddr = 0;
				chunk->v.addr = NULL;
				mlo_idx++;
				break;
			}
			mlo_size += chunk->size;
			if (ag->mlo_mem.mlo_mem_size &&
			    mlo_size > ag->mlo_mem.mlo_mem_size) {
				ath12k_err(ab, "QMI MLO memory allocation failure, requested size %d is more than allocated size %d",
					   mlo_size, ag->mlo_mem.mlo_mem_size);
				ret = -EINVAL;
				goto err;
			}

			mlo_chunk = &ag->mlo_mem.chunk[mlo_idx];
			if (mlo_chunk->paddr) {
				if (chunk->size != mlo_chunk->size) {
					ath12k_err(ab, "QMI MLO chunk memory allocation failure for index %d, requested size %d is more than allocated size %d",
						   mlo_idx, chunk->size, mlo_chunk->size);
					ret = -EINVAL;
					goto err;
				}
			} else {
				dev = ab->mlo_mem_dev.rmem_inited ?
				      &ab->mlo_mem_dev.dev : ab->dev;
				mlo_chunk->size = chunk->size;
				mlo_chunk->type = chunk->type;
				ret = ath12k_qmi_alloc_chunk(ab, mlo_chunk, dev);
				if (ret)
					goto err;
				memset(mlo_chunk->v.addr, 0, mlo_chunk->size);
			}

			chunk->paddr = mlo_chunk->paddr;
			chunk->v.addr = mlo_chunk->v.addr;
			ag->mlo_mem.is_mlo_mem_avail = true;
			mlo_idx++;

			seg_cnt++;
			break;
		default:
			ath12k_warn(ab, "memory type %u not supported\n",
				    chunk->type);
			chunk->paddr = 0;
			chunk->v.addr = NULL;
			break;
		}
	}

	if (!ag->mlo_mem.mlo_mem_size) {
		ag->mlo_mem.mlo_mem_size = mlo_size;
	} else if (mlo_idx && ag->mlo_mem.mlo_mem_size != mlo_size) {
		ath12k_err(ab, "QMI MLO memory size error, expected size is %d but requested size is %d",
			   ag->mlo_mem.mlo_mem_size, mlo_size);
		ret = -EINVAL;
		goto err;
	}

	ab->qmi.mem_seg_count = seg_cnt;
	mutex_unlock(&ag->mutex);

	return (seg_cnt - old_seg_cnt);

err:
	ath12k_qmi_free_target_mem_chunk_range(ab, old_seg_cnt, seg_cnt);

	mutex_unlock(&ag->mutex);

	return ret;
}

static void
ath12k_mgmt_mlo_global_per_device_crash_info_tlv(struct ath12k_base *ab, const void *ptr,
					       size_t len, u8 cur_device_id,
					       struct ath12k_host_mlo_glb_per_device_crash_info *per_device_crash_info)
{
	struct mlo_glb_per_device_crash_info *tlv_data;
	u8 *crash_reason, *recovery_mode;

	tlv_data = (struct mlo_glb_per_device_crash_info *)ptr;
	per_device_crash_info->device_id = cur_device_id;
	crash_reason = (u8 *)&tlv_data->crash_reason;
	recovery_mode = (u8 *)&tlv_data->recovery_mode;

	per_device_crash_info->crash_reason = (void *)crash_reason;
	per_device_crash_info->recovery_mode = (void *)recovery_mode;

	return;
}

static int
ath12k_mgmt_mlo_global_device_crash_info(struct ath12k_base *ab,
				       const void *ptr,
				       size_t len,
				       struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info)
{
	u32 device_info;
	struct mlo_glb_device_crash_info *tlv_data;

	tlv_data = (struct  mlo_glb_device_crash_info *)ptr;
	device_info = tlv_data->device_info;

	global_device_crash_info->no_of_devices =
			MLO_SHMEM_CHIP_CRASH_INFO_PARAM_NO_OF_DEVICES_GET(device_info);
	global_device_crash_info->valid_devices_bmap =
			MLO_SHMEM_CHIP_CRASH_INFO_PARAM_VALID_DEVICE_BMAP_GET(device_info);

       /* Allocate memory to extrace per chip crash info */
        global_device_crash_info->per_device_crash_info = kmalloc_array(
						global_device_crash_info->no_of_devices,
						sizeof(*global_device_crash_info->per_device_crash_info),
						GFP_KERNEL);

	if (!global_device_crash_info->per_device_crash_info) {
		ath12k_warn(ab, "Couldn't allocate memory for per chip crash info!\n");
		return -ENOBUFS;
	}

	return 0;
}

static const struct ath12k_qmi_shmem_tlv_policy ath12k_qmi_tlv_policies[] = {
	[ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_DEVICE_CRASH_INFO] =
			{ .min_len = sizeof(struct mlo_glb_device_crash_info) },
	[ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_PER_DEVICE_CRASH_INFO] =
			{ .min_len = sizeof(struct mlo_glb_per_device_crash_info) },
};

/* TODO:Calculate this dynamically in cleanup*/
#define QMI_TLV_MAX_LEN	424

/**
 * ath12k_qmi_parse_mlo_mem_arena() - Parse MLO Global shared memory arena
 * @ptr: Pointer to the start address of MLO memory
 * @len: MLO memory size in bytes
 * @mlomem_arena_ctx: Pointer to MLO Global shared memory arena context.
 * Extracted information will be populated in this data structure.
 *
 * Return: On success, the number of bytes parsed. On failure, errno is returned.
*/

static int ath12k_qmi_parse_mlo_mem_arena(struct ath12k_base *ab,
                                          const void *ptr, size_t len,
        struct ath12k_host_mlo_mem_arena *mlomem_arena_ctx)
{
	int parsed_bytes = 0, ret = 0;
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	u8 valid_devices_bmap = 0, cur_device_id;
	struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info;
	struct ath12k_host_mlo_glb_per_device_crash_info *per_device_crash_info;

	if (!ptr)
		return -EINVAL;

	while (len > parsed_bytes && parsed_bytes < QMI_TLV_MAX_LEN) {
		if (len < sizeof(*tlv)) {
			ath12k_err(ab, "tlv parse failure (%zu bytes left, %zu expected)\n",
					len, sizeof(*tlv));
			return -EINVAL;
		}
		tlv = ptr;
		tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
		ptr += sizeof(*tlv);
		parsed_bytes += sizeof(*tlv);

		if (tlv_len > len - parsed_bytes) {
			ath12k_err(ab, "qmi tlv parse failure of tag %u (%zu bytes left, %u expected)\n",
					tlv_tag, len - parsed_bytes, tlv_len);
			return -EINVAL;
		}

		if (tlv_tag < ARRAY_SIZE(ath12k_qmi_tlv_policies) &&
		    ath12k_qmi_tlv_policies[tlv_tag].min_len &&
		    ath12k_qmi_tlv_policies[tlv_tag].min_len > tlv_len) {
			ath12k_err(ab, "qmi tlv parse failure of tag %u at byte %zd (%u bytes is less than min length %zu)\n",
				   tlv_tag, len - parsed_bytes, tlv_len,
				   ath12k_qmi_tlv_policies[tlv_tag].min_len);
			return -EINVAL;
		}

		switch(tlv_tag) {
		case ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_DEVICE_CRASH_INFO:
			global_device_crash_info = &mlomem_arena_ctx->global_device_crash_info;
			ret = ath12k_mgmt_mlo_global_device_crash_info(ab, ptr, len,
								     global_device_crash_info);
			if (ret < 0){
				ath12k_info(ab,"failed to parse %d so can't proceed to parse per chip crash info\n",tlv_tag);
				return ret;
			}
			valid_devices_bmap = global_device_crash_info->valid_devices_bmap;
			break;
		case ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_PER_DEVICE_CRASH_INFO:
			if(!valid_devices_bmap) {
				ath12k_info(ab,"This TLV is not expected as the valid_devices_bmap is 0\n");
				break;
			}
			cur_device_id = ffs(valid_devices_bmap) - 1;
			valid_devices_bmap &= ~BIT(cur_device_id);

			per_device_crash_info = &global_device_crash_info->per_device_crash_info[cur_device_id];
			ath12k_mgmt_mlo_global_per_device_crash_info_tlv(ab, ptr, len, cur_device_id,
								       per_device_crash_info);
			break;
		default:
			ath12k_dbg(ab, ATH12K_DBG_QMI,"Un supported tag %d skiping this TLV\n",tlv_tag);
			break;
		}
		ptr += tlv_len;
		parsed_bytes += tlv_len;
	}

        return parsed_bytes;
}

int ath12k_qmi_mlo_global_snapshot_mem_init(struct ath12k_base *ab)
{
	struct ath12k_host_mlo_mem_arena *mlomem_arena_ctx = &ab->ag->mlomem_arena;
	struct ath12k_hw_group *ag = ab->ag;
	struct target_mem_chunk *mlo_chunk;
	int ret = 0, mlo_idx = 0;

	if (!ag->mlo_mem.is_mlo_mem_avail)
		return 0;

	mlo_chunk = &ab->ag->mlo_mem.chunk[mlo_idx];

	/* We need to initialize only for the first invocation */
	if (mlomem_arena_ctx->init_done)
		return 0;

	if (test_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags))
		ret = ath12k_qmi_parse_mlo_mem_arena(ab,
						mlo_chunk->v.ioaddr,
						mlo_chunk->size,
						mlomem_arena_ctx);
	else
		ret = ath12k_qmi_parse_mlo_mem_arena(ab,
						mlo_chunk->v.addr,
						mlo_chunk->size,
						mlomem_arena_ctx);

	if (ret < 0) {
		ath12k_err(ab, "parsing of mlo shared memory failed ret %d\n", ret);
		ath12k_free_mlo_glb_per_device_crash_info
			(&mlomem_arena_ctx->global_device_crash_info);
		return ret;
	}

	mlomem_arena_ctx->init_done = true;

	return 0;
}

#ifndef PLATFORM_SDX
static int ath12k_qmi_alloc_afc_reserved_mem(struct ath12k_base *ab, int dst_idx,
					    int afc_seg_idx)
{
	dma_addr_t m3_paddr = 0;
	u32 m3_size = 0;
	int j;

	for (j = 0; j < ab->qmi.mem_seg_count; j++) {
		if (ab->qmi.target_mem[j].type == M3_DUMP_REGION_TYPE) {
			m3_paddr = ab->qmi.target_mem[j].paddr;
			m3_size = ab->qmi.target_mem[j].size;
			break;
		}
	}

	if (!m3_paddr || !m3_size) {
		ath12k_err(ab, "AFC alloc: M3 region not found\n");
		return -EINVAL;
	}

	if (ab->qmi.target_mem[afc_seg_idx].size >
	    (m3_size - ATH12K_HOST_AFC_QCN6432_MEM_OFFSET)) {
		ath12k_dbg(ab, ATH12K_DBG_QMI,
			   "failed to assign mem type %d req size %d avail size %u\n",
			   ab->qmi.target_mem[afc_seg_idx].type,
			   ab->qmi.target_mem[afc_seg_idx].size,
			   m3_size - ATH12K_HOST_AFC_QCN6432_MEM_OFFSET);
		return -EINVAL;
	}

	ab->qmi.target_mem[dst_idx].paddr =
		m3_paddr + ATH12K_HOST_AFC_QCN6432_MEM_OFFSET;
	ab->qmi.target_mem[dst_idx].v.ioaddr =
		ioremap(ab->qmi.target_mem[dst_idx].paddr,
			ab->qmi.target_mem[afc_seg_idx].size);
	if (!ab->qmi.target_mem[dst_idx].v.ioaddr) {
		ath12k_err(ab, "AFC ioremap failed\n");
		ab->qmi.target_mem[dst_idx].paddr = 0;
		return -EIO;
	}

	return 0;
}

static int ath12k_qmi_assign_target_mem_chunk(struct ath12k_base *ab,
					      struct target_mem_chunk *req_mem,
					      u32 req_mem_seg_count)
{
	struct reserved_mem *ddr_rmem = NULL, *rmem = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	int sz = 0, avail_sz;
	int i, idx, ret;
	u64 host_fw_req_total = 0;
	struct device *dev;

	mutex_lock(&ag->mutex);
	ab->qmi.mem_seg_count = req_mem_seg_count;
	memcpy(ab->qmi.target_mem, req_mem,
	       sizeof(struct target_mem_chunk) * req_mem_seg_count);
	if (!ag->mlo_mem.init_done) {
		memset(ag->mlo_mem.chunk, 0, sizeof(ag->mlo_mem.chunk));
		ag->mlo_mem.init_done = true;
	}

	ddr_rmem = ath12k_core_get_reserved_mem_by_name(ab, "host-ddr-mem");

	if (!ddr_rmem) {
		ret = -ENODEV;
		goto out;
	}

	for (i = 0, idx = 0; i < ab->qmi.mem_seg_count; i++) {
		struct target_mem_chunk *mlo_chunk;

		switch (ab->qmi.target_mem[i].type) {
		case CALDB_MEM_REGION_TYPE:
		case HOST_DDR_REGION_TYPE:
		case BDF_MEM_REGION_TYPE:
		case M3_DUMP_REGION_TYPE:
		case PAGEABLE_MEM_REGION_TYPE:
			host_fw_req_total += ab->qmi.target_mem[i].size;
			if ((ab->qmi.target_mem[i].type == CALDB_MEM_REGION_TYPE &&
			     !ab->hw_params->cold_boot_calib) ||
			    !ab->qmi.target_mem[i].size) {
				ab->qmi.target_mem[idx].paddr = 0;
				ab->qmi.target_mem[idx].v.ioaddr = NULL;
				ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
				ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
				idx++;
				break;
			}
			if (ddr_rmem->size - sz < ab->qmi.target_mem[i].size) {
				avail_sz = ddr_rmem->size - sz;
				goto print_err;
			}

			ab->qmi.target_mem[idx].paddr = ddr_rmem->base + sz;
			/*Try to update it to coherent*/
			ab->qmi.target_mem[idx].v.ioaddr =
				ioremap(ab->qmi.target_mem[idx].paddr,
					ab->qmi.target_mem[i].size);
			if (!ab->qmi.target_mem[idx].v.ioaddr) {
				ret = -EIO;
				goto out;
			}
			sz += ab->qmi.target_mem[i].size;
			ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
			ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
			idx++;
			break;
		case MLO_GLOBAL_MEM_REGION_TYPE:
			if (!ab->qmi.target_mem[i].size) {
				ab->qmi.target_mem[idx].paddr = 0;
				ab->qmi.target_mem[idx].v.ioaddr = NULL;
				ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
				ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
				idx++;
				break;
			}
			rmem = ath12k_core_get_reserved_mem_by_name(ab, "mlo-global-mem");
			if (!rmem) {
				ret = -EINVAL;
				goto out;
			}

			if (rmem->size < ab->qmi.target_mem[i].size) {
				avail_sz = rmem->size;
				goto print_err;
			}

			mlo_chunk = &ag->mlo_mem.chunk[0];
			if (!mlo_chunk->paddr) {
				mlo_chunk->size = ab->qmi.target_mem[i].size;
				mlo_chunk->type = ab->qmi.target_mem[i].type;
				mlo_chunk->paddr = rmem->base;
				if (ab->mlo_mem_dev.rmem_inited) {
					dev = &ab->mlo_mem_dev.dev;
					mlo_chunk->v.ioaddr =
						dma_alloc_coherent(dev,
								   mlo_chunk->size,
								   &mlo_chunk->paddr,
								   GFP_KERNEL);
				} else {
					mlo_chunk->v.ioaddr = ioremap(mlo_chunk->paddr,
								      mlo_chunk->size);
				}

				if (!mlo_chunk->v.ioaddr) {
					ath12k_err(ab, "MLO memory allocation failed\n");
					mlo_chunk->paddr = 0;
					ret = -ENOMEM;
					goto out;
				}
				memset_io(mlo_chunk->v.ioaddr, 0, mlo_chunk->size);
			}

			ab->qmi.target_mem[idx].paddr = mlo_chunk->paddr;
			ab->qmi.target_mem[idx].v.ioaddr = mlo_chunk->v.ioaddr;
			ab->qmi.target_mem[idx].size = mlo_chunk->size;
			ab->qmi.target_mem[idx].type = mlo_chunk->type;

			if (!ag->mlo_mem.mlo_mem_size) {
				ag->mlo_mem.mlo_mem_size = mlo_chunk->size;
			} else if (ag->mlo_mem.mlo_mem_size != mlo_chunk->size) {
				ath12k_err(ab, "QMI MLO memory size error, expected size is %dbut requested size is %d",
					   ag->mlo_mem.mlo_mem_size,
					   mlo_chunk->size);

				ret = -EINVAL;
				goto out;
			}
			ag->mlo_mem.is_mlo_mem_avail = true;
			if (ab->qmi.target_mem[i].size < rmem->size) {
				ath12k_info(ab, "mlo: More memory is reserved. reserved=%llu requested=%u\n",
					    (unsigned long long)rmem->size,
					    ab->qmi.target_mem[i].size);
			}
			idx++;
			break;
		case AFC_REGION_TYPE:
			if (ab->qmi.target_mem[i].size > AFC_MEM_SIZE) {
				ath12k_warn(ab, "AFC mem request size %d is larger than allowed value\n",
					    ab->qmi.target_mem[i].size);
				mutex_unlock(&ag->mutex);
				return -EINVAL;
			}

			/* For multi-pd platforms, AFC_REGION_TYPE needs
			 * to be allocated from within M3_DUMP_REGION.
			 * This is because multi-pd platforms cannot access memory
			 * regions allocated outside FW reserved memory.
			 * AFC_REGION_TYPE is supported for 6 GHz.
			 */

			if (ab->hif.bus == ATH12K_BUS_HYBRID &&
			    ab->qmi.target_mem_mode != ATH12K_QMI_TARGET_MEM_MODE_256M) {
				ret = ath12k_qmi_alloc_afc_reserved_mem(ab, idx, i);
				if (ret)
					goto out;
			} else {
				ab->qmi.target_mem[idx].v.addr =
					dma_alloc_coherent(ab->dev,
						ab->qmi.target_mem[i].size,
						&ab->qmi.target_mem[idx].paddr,
						GFP_KERNEL);

				if (!ab->qmi.target_mem[idx].v.addr) {
					ath12k_err(ab, "AFC mem allocation failed\n");
					ab->qmi.target_mem[idx].paddr = 0;
					mutex_unlock(&ag->mutex);
					return -ENOMEM;
				}

			}


			ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
			ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
			idx++;
			break;
		default:
			ath12k_warn(ab, "qmi ignore invalid mem req type %d\n",
				    ab->qmi.target_mem[i].type);
			ab->qmi.target_mem[idx].paddr = 0;
			ab->qmi.target_mem[idx].v.ioaddr = NULL;
			ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
			ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
			idx++;
			break;
		}
	}

	ab->host_ddr_fixed_mem_off = sz;
	ab->qmi.mem_seg_count = idx;
	if (sz < ddr_rmem->size) {
		ath12k_info(ab, "host-ddr: More memory is reserved. reserved=%llu requested=%llu avail=%llu\n",
			    (unsigned long long)ddr_rmem->size,
			    (unsigned long long)host_fw_req_total,
			    (unsigned long long)(ddr_rmem->size - sz));
	}

	mutex_unlock(&ag->mutex);

	return 0;
print_err:
	ath12k_dbg(ab, ATH12K_DBG_QMI, "failed to assign mem type %d req size %d avail size %u\n",
		   ab->qmi.target_mem[i].type,
		   ab->qmi.target_mem[i].size,
		   avail_sz);
	ret = -EINVAL;
out:
	ath12k_qmi_free_target_mem_chunk(ab);

	mutex_unlock(&ag->mutex);

	return ret;
}
#else
static int ath12k_qmi_assign_target_mem_chunk(struct ath12k_base *ab,
					      struct target_mem_chunk *req_mem,
					      u32 req_mem_seg_count)
{
	struct reserved_mem *ddr_rmem = NULL, *rmem = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	int sz = 0, avail_sz;
	int i, idx, ret;

	mutex_lock(&ag->mutex);
	ab->qmi.mem_seg_count = req_mem_seg_count;
	memcpy(ab->qmi.target_mem, req_mem,
	       sizeof(struct target_mem_chunk) * req_mem_seg_count);
	if (!ag->mlo_mem.init_done) {
		memset(ag->mlo_mem.chunk, 0, sizeof(ag->mlo_mem.chunk));
		ag->mlo_mem.init_done = true;
	}

	ddr_rmem = ath12k_core_get_reserved_mem_by_name(ab, "host-ddr-mem");

	if (!ddr_rmem) {
		ret = -ENODEV;
		goto out;
	}

	for (i = 0, idx = 0; i < ab->qmi.mem_seg_count; i++) {
		struct target_mem_chunk *mlo_chunk;

		switch (ab->qmi.target_mem[i].type) {
		case CALDB_MEM_REGION_TYPE:
		case HOST_DDR_REGION_TYPE:
		case BDF_MEM_REGION_TYPE:
		case M3_DUMP_REGION_TYPE:
		case PAGEABLE_MEM_REGION_TYPE:
			if ((ab->qmi.target_mem[i].type == CALDB_MEM_REGION_TYPE &&
			     !ab->hw_params->cold_boot_calib) ||
			    !ab->qmi.target_mem[i].size) {
				ab->qmi.target_mem[idx].paddr = 0;
				ab->qmi.target_mem[idx].v.ioaddr = NULL;
				ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
				ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
				idx++;
				break;
			}
			if (ddr_rmem->size - sz < ab->qmi.target_mem[i].size) {
				avail_sz = ddr_rmem->size - sz;
				goto print_err;
			}

			ab->qmi.target_mem[idx].paddr = ddr_rmem->base + sz;
			/*Try to update it to coherent*/
			ab->qmi.target_mem[idx].v.ioaddr =
					dma_alloc_attrs(ab->dev,
							ab->qmi.target_mem[i].size,
							&ab->qmi.target_mem[idx].paddr,
							GFP_KERNEL,
							DMA_ATTR_FORCE_CONTIGUOUS);
			if (!ab->qmi.target_mem[idx].v.ioaddr) {
				ret = -EIO;
				goto out;
			}
			sz += ab->qmi.target_mem[i].size;
			ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
			ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
			idx++;
			break;
		case MLO_GLOBAL_MEM_REGION_TYPE:
			if (!ab->qmi.target_mem[i].size) {
				ab->qmi.target_mem[idx].paddr = 0;
				ab->qmi.target_mem[idx].v.ioaddr = NULL;
				ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
				ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
				idx++;
				break;
			}
			rmem = ath12k_core_get_reserved_mem_by_name(ab, "mlo-global-mem");
			if (!rmem) {
				ret = -EINVAL;
				goto out;
			}

			if (rmem->size < ab->qmi.target_mem[i].size) {
				avail_sz = rmem->size;
				goto print_err;
			}

			mlo_chunk = &ag->mlo_mem.chunk[0];
			if (!mlo_chunk->paddr) {
				mlo_chunk->size = ab->qmi.target_mem[i].size;
				mlo_chunk->type = ab->qmi.target_mem[i].type;
				mlo_chunk->paddr = rmem->base;
				mlo_chunk->v.ioaddr =
					dma_alloc_attrs(ab->dev,
							mlo_chunk->size,
							&mlo_chunk->paddr,
							GFP_KERNEL,
							DMA_ATTR_FORCE_CONTIGUOUS);
				memset_io(mlo_chunk->v.ioaddr, 0, mlo_chunk->size);
			}

			ab->qmi.target_mem[idx].paddr = mlo_chunk->paddr;
			ab->qmi.target_mem[idx].v.ioaddr = mlo_chunk->v.ioaddr;
			ab->qmi.target_mem[idx].size = mlo_chunk->size;
			ab->qmi.target_mem[idx].type = mlo_chunk->type;

			if (!ag->mlo_mem.mlo_mem_size) {
				ag->mlo_mem.mlo_mem_size = mlo_chunk->size;
			} else if (ag->mlo_mem.mlo_mem_size != mlo_chunk->size) {
				ath12k_err(ab, "QMI MLO memory size error, expected size is %d"
					   "but requested size is %d", ag->mlo_mem.mlo_mem_size,
					   mlo_chunk->size);

					ret = -EINVAL;
					goto out;
			}
			ag->mlo_mem.is_mlo_mem_avail = true;
			idx++;
			break;
		case AFC_REGION_TYPE:
			if (ab->qmi.target_mem[i].size > AFC_MEM_SIZE) {
				ath12k_warn(ab, "AFC mem request size %d is larger than allowed value\n",
					    ab->qmi.target_mem[i].size);
				return -EINVAL;
			}

			ab->qmi.target_mem[idx].v.addr =
				dma_alloc_coherent(ab->dev, ab->qmi.target_mem[i].size,
						&ab->qmi.target_mem[idx].paddr,
						GFP_KERNEL);

			if (!ab->qmi.target_mem[idx].v.addr) {
				ath12k_err(ab, "AFC mem allocation failed\n");
				ab->qmi.target_mem[idx].paddr = 0;
				return -ENOMEM;
			}

			ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
			ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
			idx++;
			break;
		default:
			ath12k_warn(ab, "qmi ignore invalid mem req type %d\n",
				    ab->qmi.target_mem[i].type);
			ab->qmi.target_mem[idx].paddr = 0;
			ab->qmi.target_mem[idx].v.ioaddr = NULL;
			ab->qmi.target_mem[idx].size = ab->qmi.target_mem[i].size;
			ab->qmi.target_mem[idx].type = ab->qmi.target_mem[i].type;
			idx++;
			break;
		}
	}

	ab->host_ddr_fixed_mem_off = sz;
	ab->qmi.mem_seg_count = idx;

	mutex_unlock(&ag->mutex);

	return 0;
print_err:
	ath12k_dbg(ab, ATH12K_DBG_QMI, "failed to assign mem type %d req size %d avail size %u\n",
		   ab->qmi.target_mem[i].type,
		   ab->qmi.target_mem[i].size,
		   avail_sz);
	ret = -EINVAL;
out:
	ath12k_qmi_free_target_mem_chunk(ab);

	mutex_unlock(&ag->mutex);

	return ret;
}
#endif

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_request_target_cap(struct ath12k_base *ab)
{
	struct qmi_wlanfw_cap_req_msg_v01 req = {};
	struct qmi_wlanfw_cap_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	struct device *dev = ab->dev;
	unsigned int board_id;
	int ret = 0;
	int r;
	int i;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_cap_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_CAP_REQ_V01,
			       QMI_WLANFW_CAP_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_cap_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to send target cap request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed target cap request %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi targetcap req failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

	if (resp.chip_info_valid) {
		ab->qmi.target.chip_id = resp.chip_info.chip_id;
		ab->qmi.target.chip_family = resp.chip_info.chip_family;
	}

	if (resp.board_info_valid && resp.board_info.board_id != ATH12K_BOARD_ID_INVALID)
		ab->qmi.target.board_id = resp.board_info.board_id;
	else if (!of_property_read_u32(dev->of_node, "qcom,board_id", &board_id) &&
		 board_id != ATH12K_BOARD_ID_DEFAULT)
		ab->qmi.target.board_id = board_id;
	else
		ab->qmi.target.board_id = ATH12K_BOARD_ID_DEFAULT;

	if (resp.soc_info_valid)
		ab->qmi.target.soc_id = resp.soc_info.soc_id;

	if (resp.fw_version_info_valid) {
		ab->qmi.target.fw_version = resp.fw_version_info.fw_version;
		strscpy(ab->qmi.target.fw_build_timestamp,
			resp.fw_version_info.fw_build_timestamp,
			sizeof(ab->qmi.target.fw_build_timestamp));
	}

	if (resp.fw_build_id_valid)
		strscpy(ab->qmi.target.fw_build_id, resp.fw_build_id,
			sizeof(ab->qmi.target.fw_build_id));

	if (resp.dev_mem_info_valid) {
		for (i = 0; i < ATH12K_QMI_WLFW_MAX_DEV_MEM_NUM_V01; i++) {
			ab->qmi.dev_mem[i].start =
				resp.dev_mem[i].start;
			ab->qmi.dev_mem[i].size =
				resp.dev_mem[i].size;
			ath12k_dbg(ab, ATH12K_DBG_QMI,
				   "devmem [%d] start 0x%llx size %llu\n", i,
				   ab->qmi.dev_mem[i].start,
				   ab->qmi.dev_mem[i].size);
		}
	}

	if (resp.rxgainlut_support_valid)
		ab->rxgainlut_support = !!resp.rxgainlut_support;
	if (resp.ext_fw_bin_download_support_valid)
		ab->qmi.ext_fw_bin_download_support = !!resp.ext_fw_bin_download_support;
	if (resp.ext_fw_bin_download_bitmap_valid)
		ab->qmi.ext_fw_bin_download_bitmap = resp.ext_fw_bin_download_bitmap;

	ath12k_info(ab, "RxGain support %u Extended ucode download support %u Extended ucode download bitmap 0x%x\n",
		    ab->rxgainlut_support, ab->qmi.ext_fw_bin_download_support,
		    ab->qmi.ext_fw_bin_download_bitmap);

	if (resp.eeprom_caldata_read_timeout_valid) {
		ab->qmi.target.eeprom_caldata = resp.eeprom_caldata_read_timeout;
		ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi cal data supported from eeprom\n");
	}

	ath12k_info(ab, "chip_id 0x%x chip_family 0x%x board_id 0x%x soc_id 0x%x\n",
		    ab->qmi.target.chip_id, ab->qmi.target.chip_family,
		    ab->qmi.target.board_id, ab->qmi.target.soc_id);

	ath12k_info(ab, "fw_version 0x%x fw_build_timestamp %s fw_build_id %s",
		    ab->qmi.target.fw_version,
		    ab->qmi.target.fw_build_timestamp,
		    ab->qmi.target.fw_build_id);

	r = ath12k_core_check_smbios(ab);
	if (r)
		ath12k_dbg(ab, ATH12K_DBG_QMI, "SMBIOS bdf variant name not set.\n");

	r = ath12k_acpi_start(ab);
	if (r)
		/* ACPI is optional so continue in case of an error */
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "acpi failed: %d\n", r);

	r = ath12k_acpi_check_bdf_variant_name(ab);
	if (r)
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "ACPI bdf variant name not set.\n");

out:
	return ret;
}

static int ath12k_qmi_load_file_target_mem(struct ath12k_base *ab,
					   const u8 *data, u32 len, u8 type)
{
	struct qmi_wlanfw_bdf_download_req_msg_v01 *req;
	struct qmi_wlanfw_bdf_download_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	const u8 *temp = data;
	int ret = 0;
	u32 remaining = len;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	while (remaining) {
		req->valid = 1;
		req->file_id_valid = 1;
		req->file_id = ab->qmi.target.board_id;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->bdf_type = type;
		req->bdf_type_valid = 1;
		req->end_valid = 1;
		req->end = 0;

		if (remaining > QMI_WLANFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLANFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		if (type == ATH12K_QMI_FILE_TYPE_EEPROM) {
			req->data_valid = 0;
			req->end = 1;
			req->data_len = ATH12K_QMI_MAX_BDF_FILE_NAME_SIZE;
		} else {
			memcpy(req->data, temp, req->data_len);
		}

		ret = qmi_txn_init(&ab->qmi.handle, &txn,
				   qmi_wlanfw_bdf_download_resp_msg_v01_ei,
				   &resp);
		if (ret < 0)
			goto out;

		ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi bdf download req fixed addr type %d\n",
			   type);

		ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
				       QMI_WLANFW_BDF_DOWNLOAD_REQ_V01,
				       QMI_WLANFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_LEN,
				       qmi_wlanfw_bdf_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			goto out;
		}

		ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
		if (ret < 0)
			goto out;

		if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
			ath12k_warn(ab, "qmi BDF download failed, result: %d, err: %d\n",
				    resp.resp.result, resp.resp.error);
			ret = -EINVAL;
			goto out;
		}

		if (type == ATH12K_QMI_FILE_TYPE_EEPROM) {
			remaining = 0;
		} else {
			remaining -= req->data_len;
			temp += req->data_len;
			req->seg_id++;
			ath12k_dbg(ab, ATH12K_DBG_QMI,
				   "qmi bdf download request remaining %i\n",
				   remaining);
		}
	}

out:
	kfree(req);
	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_load_bdf_qmi(struct ath12k_base *ab,
			    enum ath12k_qmi_bdf_type type)
{
	struct device *dev = ab->dev;
	char filename[ATH12K_QMI_MAX_BDF_FILE_NAME_SIZE];
	const struct firmware *fw_entry = NULL;
	struct ath12k_board_data bd;
	struct ath12k_ahb *ab_ahb;
	u32 fw_size, file_type;
	int ret = 0;
	const u8 *tmp;

	memset(&bd, 0, sizeof(bd));

	switch (type) {
	case ATH12K_QMI_BDF_TYPE_ELF:
		ret = ath12k_core_fetch_bdf(ab, &bd);
		if (ret) {
			ath12k_warn(ab, "qmi failed to load bdf:\n");
			goto out;
		}

		if (bd.len >= SELFMAG && memcmp(bd.data, ELFMAG, SELFMAG) == 0)
			type = ATH12K_QMI_BDF_TYPE_ELF;
		else
			type = ATH12K_QMI_BDF_TYPE_BIN;

		break;
	case ATH12K_QMI_BDF_TYPE_REGDB:
		ret = ath12k_core_fetch_regdb(ab, &bd);
		if (ret) {
			ath12k_warn(ab, "qmi failed to load regdb bin:\n");
			goto out;
		}
		break;
	case ATH12K_QMI_BDF_TYPE_RXGAINLUT:
		ret = ath12k_core_fetch_rxgainlut(ab, &bd);
		if (ret < 0) {
			ath12k_warn(ab, "qmi failed to load rxgainlut\n");
			goto out;
		}
		break;
	case ATH12K_QMI_BDF_TYPE_CALIBRATION:
		if (ath12k_skip_caldata) {
			if (ath12k_ftm_mode) {
				ath12k_warn(ab, "Skipping caldata download in FTM mode\n");
				goto out;
			}
			ath12k_err(ab, "failed to skip caldata download. FTM mode is not enabled\n");
			ret = -EOPNOTSUPP;
			goto out;
		}
		if (ab->qmi.target.eeprom_caldata) {
			file_type = ATH12K_QMI_FILE_TYPE_EEPROM;
			tmp = filename;
			fw_size = ATH12K_QMI_MAX_BDF_FILE_NAME_SIZE;
		} else {
			file_type = ATH12K_QMI_FILE_TYPE_CALDATA;

			/* cal-<bus>-<id>.bin */
			snprintf(filename, sizeof(filename), "cal-%s-%s.bin",
				 ath12k_bus_str(ab->hif.bus), dev_name(dev));
			fw_entry = ath12k_core_firmware_request(ab, filename);

			if (!IS_ERR(fw_entry))
				goto success;

			if (ab->hif.bus == ATH12K_BUS_HYBRID) {
				ab_ahb = ath12k_ab_to_ahb(ab);
				snprintf(filename, sizeof(filename), "%s%d%s",
					 ATH12K_QMI_DEF_CAL_FILE_PREFIX,
					 ab_ahb->userpd_id - 1,
					 ATH12K_QMI_DEF_CAL_FILE_SUFFIX);
			} else {
				snprintf(filename, sizeof(filename), "%s",
					 ATH12K_DEFAULT_CAL_FILE);
			}

			fw_entry = ath12k_core_firmware_request(ab, filename);

			if (IS_ERR(fw_entry)) {
				ret = PTR_ERR(fw_entry);
				ath12k_warn(ab,
					    "qmi failed to load CAL data file:%s\n",
					    filename);
				goto out;
			}

success:
			fw_size = min_t(u32, ab->hw_params->fw.board_size,
					fw_entry->size);
			tmp = fw_entry->data;
		}
		ret = ath12k_qmi_load_file_target_mem(ab, tmp, fw_size, file_type);
		if (ret < 0) {
			ath12k_warn(ab, "qmi failed to load caldata\n");
			goto out_qmi_cal;
		}

		ath12k_info(ab, "qmi caldata downloaded: type: %u, size: %u\n",
			    file_type, fw_size);

out_qmi_cal:
		if (!ab->qmi.target.eeprom_caldata)
			release_firmware(fw_entry);
		return ret;
	default:
		ath12k_warn(ab, "unknown file type for load %d", type);
		goto out;
	}

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi bdf_type %d\n", type);

	fw_size = min_t(u32, ab->hw_params->fw.board_size, bd.len);

	ret = ath12k_qmi_load_file_target_mem(ab, bd.data, fw_size, type);
	if (ret < 0)
		ath12k_warn(ab, "qmi failed to load bdf file\n");

out:
	ath12k_core_free_bdf(ab, &bd);
	ath12k_info(ab, "qmi BDF download sequence completed for type: %u, size: %u\n",
		    type, fw_size);

	return ret;
}

static void ath12k_qmi_m3_free(struct ath12k_base *ab)
{
	struct m3_mem_region *m3_mem = &ab->qmi.m3_mem;

	if (ab->hw_params->fw.m3_loader == ath12k_m3_fw_loader_remoteproc)
		return;

	if (!m3_mem->vaddr)
		return;

	dma_free_coherent(ab->dev, m3_mem->total_size,
			  m3_mem->vaddr, m3_mem->paddr);
	m3_mem->vaddr = NULL;
	m3_mem->total_size = 0;
	m3_mem->size = 0;
}

static int ath12k_qmi_m3_load(struct ath12k_base *ab)
{
	struct m3_mem_region *m3_mem = &ab->qmi.m3_mem;
	const struct firmware *fw = NULL;
	const void *m3_data;
	char path[100];
	size_t m3_len;
	int ret;

	if (ab->fw.m3_data && ab->fw.m3_len > 0) {
		/* firmware-N.bin had a m3 firmware file so use that */
		m3_data = ab->fw.m3_data;
		m3_len = ab->fw.m3_len;
	} else {
		/* No m3 file in firmware-N.bin so try to request old
		 * separate m3.bin.
		 */
		fw = ath12k_core_firmware_request(ab, ATH12K_M3_FILE);
		if (IS_ERR(fw)) {
			ret = PTR_ERR(fw);
			ath12k_core_create_firmware_path(ab, ATH12K_M3_FILE,
							 path, sizeof(path));
			ath12k_err(ab, "failed to load %s: %d\n", path, ret);
			return ret;
		}

		m3_data = fw->data;
		m3_len = fw->size;
	}

	/* In recovery/resume cases, M3 buffer is not freed, try to reuse that */
	if (m3_mem->vaddr) {
		if (m3_mem->total_size >= m3_len)
			goto skip_m3_alloc;

		/* Old buffer is too small, free and reallocate */
		ath12k_qmi_m3_free(ab);
	}

	m3_mem->vaddr = dma_alloc_coherent(ab->dev,
					   m3_len, &m3_mem->paddr,
					   GFP_KERNEL);
	if (!m3_mem->vaddr) {
		ath12k_err(ab, "failed to allocate memory for M3 with size %zu\n",
			   m3_len);
		ret = -ENOMEM;
		goto out;
	}

	m3_mem->total_size = m3_len;

skip_m3_alloc:
	memcpy(m3_mem->vaddr, m3_data, m3_len);
	m3_mem->size = m3_len;

	ret = 0;

out:
	release_firmware(fw);

	return ret;
}

static void ath12k_qmi_ext_fw_bin_free(struct ath12k_base *ab,
				       enum qmi_wlanfw_ext_fw_bin_type_enum_v01 type)
{
	struct ext_fw_bin_mem_region *ext_fw_bin_mem = &ab->qmi.ext_fw_bin_mem[type];

	if (!ext_fw_bin_mem->vaddr)
		return;

	dma_free_coherent(ab->dev, ext_fw_bin_mem->size,
			  ext_fw_bin_mem->vaddr, ext_fw_bin_mem->paddr);
	ext_fw_bin_mem->vaddr = NULL;
	ext_fw_bin_mem->size = 0;
}

static void ath12k_qmi_ext_fw_bin_clear(struct ath12k_base *ab)
{
	int type;

	for (type = 0; type < QMI_WLANFW_EXT_FW_MAX_BIN_TYPE; type++) {
		if (ab->qmi.ext_fw_bin_mem[type].vaddr)
			ath12k_qmi_ext_fw_bin_free(ab, type);
	}
}

static int ath12k_qmi_ext_fw_bin_load(struct ath12k_base *ab,
				      enum qmi_wlanfw_ext_fw_bin_type_enum_v01 type)
{
	struct ext_fw_bin_mem_region *ext_fw_bin_mem = NULL;
	const struct firmware *fw = NULL;
	const void *data = NULL;
	size_t len = 0;
	char path[100];
	int ret = 0;

	switch (type) {
	case QMI_WLANFW_EXT_FW_BIN_TYPE_M3_V01:
		/* firmware-N.bin had a m3 firmware file so use that */
		if (ab->fw.m3_data && ab->fw.m3_len > 0) {
			data = ab->fw.m3_data;
			len = ab->fw.m3_len;
		} else {
			fw = ath12k_core_firmware_request(ab, "m3.bin");
			if (IS_ERR(fw)) {
				ret = PTR_ERR(fw);
				ath12k_core_create_firmware_path(ab, "m3.bin",
								 path, sizeof(path));
				ath12k_err(ab, "failed to load %s/m3.bin: %d\n",
					   path, ret);
				return ret;
			}
		}
		break;
	case QMI_WLANFW_EXT_FW_BIN_TYPE_AUX_V01:
		/* firmware-N.bin had a aux firmware file so use that */
		if (ab->fw.aux_data && ab->fw.aux_len > 0) {
			data = ab->fw.aux_data;
			len = ab->fw.aux_len;
		} else {
			fw = ath12k_core_firmware_request(ab, "aux.bin");
			if (IS_ERR(fw)) {
				ret = PTR_ERR(fw);
				ath12k_core_create_firmware_path(ab, "aux.bin",
								 path, sizeof(path));
				ath12k_err(ab, "failed to load %s/aux.bin: %d\n",
					   path, ret);
				return ret;
			}
		}
		break;
	case QMI_WLANFW_EXT_FW_BIN_TYPE_MCSS_V01:
		/* firmware-N.bin had a mcss firmware file so use that */
		if (ab->fw.mcss_data && ab->fw.mcss_len > 0) {
			data = ab->fw.mcss_data;
			len = ab->fw.mcss_len;
		} else {
			fw = ath12k_core_firmware_request(ab, "mcss.bin");
			if (IS_ERR(fw)) {
				ret = PTR_ERR(fw);
				ath12k_core_create_firmware_path(ab, "mcss.bin",
								 path, sizeof(path));
				ath12k_err(ab, "failed to load %s/mcss.bin: %d\n",
					   path, ret);
				return ret;
			}
		}
		break;
	default:
		ath12k_err(ab, "FW bin type %d not supported\n", type);
		return -EINVAL;
	}

	/*
	 * Fallback to the firmware blob returned by
	 * ath12k_core_firmware_request() when no in-memory data was
	 * provided via firmware-N.bin (i.e. data/len are unset). This
	 * ensures we still have a valid buffer to load into the DMA
	 * region for the requested ext FW bin type.
	 */
	if ((!data || !len) && fw) {
		data = fw->data;
		len = fw->size;
	}

	/* In recovery/resume cases, file buffer is not freed, try to reuse that */
	ext_fw_bin_mem = &ab->qmi.ext_fw_bin_mem[type];
	if (ext_fw_bin_mem->vaddr && ext_fw_bin_mem->size >= len)
		goto skip_alloc;

	/* Old buffer is too small, free and reallocate */
	if (ext_fw_bin_mem->vaddr)
		ath12k_qmi_ext_fw_bin_free(ab, type);

	ext_fw_bin_mem->vaddr = dma_alloc_coherent(ab->dev, len, &ext_fw_bin_mem->paddr,
						   GFP_KERNEL);
	if (!ext_fw_bin_mem->vaddr) {
		ret = -ENOMEM;
		goto out;
	}

skip_alloc:
	memcpy(ext_fw_bin_mem->vaddr, data, len);
	ext_fw_bin_mem->size = len;
out:
	if (fw)
		release_firmware(fw);

	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_wlanfw_ext_fw_bin_mem_info_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_ext_fw_bin_download_resp_msg_v01 *resp = NULL;
	struct qmi_wlanfw_ext_fw_bin_download_req_msg_v01 *req = NULL;
	struct ext_fw_bin_mem_region *ext_fw_bin_mem = NULL;
	int num_ext_fw_bin = 0;
	struct qmi_txn txn;
	int type = 0;
	int ret = 0;

	if (ab->qmi.ext_fw_bin_download_bitmap &
			~((1 << QMI_WLANFW_EXT_FW_MAX_BIN_TYPE) - 1)) {
		ath12k_warn(ab, "Invalid ext_fw_bin_download_bitmap 0x%x, unsupported bits set\n",
			    ab->qmi.ext_fw_bin_download_bitmap);
		return -EINVAL;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	for (type = 0; type < QMI_WLANFW_EXT_FW_MAX_BIN_TYPE; type++) {
		if (!(ab->qmi.ext_fw_bin_download_bitmap & (1 << type)))
			continue;

		/* Check if we've reached the maximum number of firmware binaries */
		if (num_ext_fw_bin >= QMI_WLANFW_MAX_NUM_EXT_FW_BIN_DOWNLOAD_V01) {
			ath12k_warn(ab, "Too many firmware binaries to download, skipping type %d\n",
				    type);
			continue;
		}

		ret = ath12k_qmi_ext_fw_bin_load(ab, type);
		if (ret) {
			ath12k_err(ab, "failed to load EXT FW bin type %d ret %d\n",
				   type, ret);
			continue;
		}

		ext_fw_bin_mem = &ab->qmi.ext_fw_bin_mem[type];
		req->ext_bin_mem_info[num_ext_fw_bin].addr = ext_fw_bin_mem->paddr;
		req->ext_bin_mem_info[num_ext_fw_bin].size = ext_fw_bin_mem->size;
		req->ext_bin_mem_info[num_ext_fw_bin].type = type;
		num_ext_fw_bin++;
	}

	req->ext_bin_mem_info_len = num_ext_fw_bin;
	if (!req->ext_bin_mem_info_len) {
		ath12k_err(ab, "No EXT FW bins are found %x\n",
			   ab->qmi.ext_fw_bin_download_bitmap);
		ret = -EINVAL;
		goto out;
	}

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto out;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_ext_fw_bin_download_resp_msg_v01_ei, resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_EXT_FW_BIN_DOWNLOAD_REQ_V01,
			       QMI_WLANFW_EXT_FW_BIN_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_ext_fw_bin_download_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to download EXT FW bin request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi wait failed to download EXT FW bin %d\n", ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi EXT FW bin info request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	kfree(req);
	kfree(resp);

	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_wlanfw_m3_info_send(struct ath12k_base *ab)
{
	struct m3_mem_region *m3_mem = &ab->qmi.m3_mem;
	struct qmi_wlanfw_m3_info_req_msg_v01 req = {};
	struct qmi_wlanfw_m3_info_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	int ret = 0;

	if (ab->hw_params->fw.m3_loader == ath12k_m3_fw_loader_driver) {
		ret = ath12k_qmi_m3_load(ab);
		if (ret) {
			ath12k_err(ab, "failed to load m3 firmware: %d", ret);
			return ret;
		}
		req.addr = m3_mem->paddr;
		req.size = m3_mem->size;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_m3_info_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_M3_INFO_REQ_V01,
			       QMI_WLANFW_M3_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_m3_info_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to send M3 information request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed M3 information request %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi M3 info request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	return ret;
}

static int ath12k_qmi_wlanfw_mode_send(struct ath12k_base *ab,
				       u32 mode)
{
	struct qmi_wlanfw_wlan_mode_req_msg_v01 req = {};
	struct qmi_wlanfw_wlan_mode_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	int ret = 0;

	req.mode = mode;
	req.hw_debug_valid = 1;
	req.hw_debug = 0;

	if ((mode == ATH12K_FIRMWARE_MODE_NORMAL || mode == ATH12K_FIRMWARE_MODE_FTM)
	   && ab->mm_cal_support &&
	    ath12k_cold_boot_cal_needed(ab)) {
		ath12k_info(ab,"cold boot calibration is requested in MISSION/FTM MODE\n");
		req.do_coldboot_cal_valid = 1;
		req.do_coldboot_cal = 1;
	}
	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_wlan_mode_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_WLAN_MODE_REQ_V01,
			       QMI_WLANFW_WLAN_MODE_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_wlan_mode_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to send mode request, mode: %d, err = %d\n",
			    mode, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		if (mode == ATH12K_FIRMWARE_MODE_OFF && ret == -ENETRESET) {
			ath12k_warn(ab, "WLFW service is dis-connected\n");
			return 0;
		}
		ath12k_warn(ab, "qmi failed set mode request, mode: %d, err = %d\n",
			    mode, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "Mode request failed, mode: %d, result: %d err: %d\n",
			    mode, resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static int ath12k_qmi_wlanfw_wlan_cfg_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_wlan_cfg_req_msg_v01 *req;
	struct qmi_wlanfw_wlan_cfg_resp_msg_v01 resp = {};
	int ret = 0, pipe_num, ext_pipe_num;
	struct ce_pipe_config *ce_cfg;
	struct service_to_pipe *svc_cfg;
	struct qmi_txn txn;

	ce_cfg	= (struct ce_pipe_config *)ab->qmi.ce_cfg.tgt_ce;
	svc_cfg	= (struct service_to_pipe *)ab->qmi.ce_cfg.svc_to_ce_map;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->host_version_valid = 1;
	strscpy(req->host_version, ATH12K_HOST_VERSION_STRING,
		sizeof(req->host_version));

	req->tgt_cfg_valid = 1;
	/* This is number of CE configs */
	req->tgt_cfg_len = ab->qmi.ce_cfg.tgt_ce_len;
	if (ab->qmi.ce_cfg.tgt_ce_len > QMI_WLANFW_MAX_NUM_CE_V01) {
		req->tgt_cfg_len = QMI_WLANFW_MAX_NUM_CE_V01;
		req->ext_tgt_cfg_valid = 1;
		req->ext_tgt_cfg_len = ab->qmi.ce_cfg.tgt_ce_len -
						QMI_WLANFW_MAX_NUM_CE_V01;
	}

	for (pipe_num = 0; pipe_num < req->tgt_cfg_len; pipe_num++) {
		req->tgt_cfg[pipe_num].pipe_num = ce_cfg[pipe_num].pipenum;
		req->tgt_cfg[pipe_num].pipe_dir = ce_cfg[pipe_num].pipedir;
		req->tgt_cfg[pipe_num].nentries = ce_cfg[pipe_num].nentries;
		req->tgt_cfg[pipe_num].nbytes_max = ce_cfg[pipe_num].nbytes_max;
		req->tgt_cfg[pipe_num].flags = ce_cfg[pipe_num].flags;
	}

	pipe_num = QMI_WLANFW_MAX_NUM_CE_V01;
	for (ext_pipe_num = 0; ext_pipe_num < req->ext_tgt_cfg_len; ext_pipe_num++) {
		req->ext_tgt_cfg[ext_pipe_num].pipe_num = ce_cfg[pipe_num].pipenum;
		req->ext_tgt_cfg[ext_pipe_num].pipe_dir = ce_cfg[pipe_num].pipedir;
		req->ext_tgt_cfg[ext_pipe_num].nentries = ce_cfg[pipe_num].nentries;
		req->ext_tgt_cfg[ext_pipe_num].nbytes_max = ce_cfg[pipe_num].nbytes_max;
		req->ext_tgt_cfg[ext_pipe_num].flags = ce_cfg[pipe_num].flags;
		pipe_num++;
	}

	req->svc_cfg_valid = 1;
	/* This is number of Service/CE configs */
	req->svc_cfg_len = ab->qmi.ce_cfg.svc_to_ce_map_len;
	if (ab->qmi.ce_cfg.svc_to_ce_map_len > QMI_WLANFW_MAX_NUM_SVC_V01) {
		req->svc_cfg_len = QMI_WLANFW_MAX_NUM_SVC_V01;
		req->ext_svc_cfg_valid = 1;
		req->ext_svc_cfg_len = ab->qmi.ce_cfg.svc_to_ce_map_len -
						QMI_WLANFW_MAX_NUM_SVC_V01;
	}

	for (pipe_num = 0; pipe_num < req->svc_cfg_len; pipe_num++) {
		req->svc_cfg[pipe_num].service_id = svc_cfg[pipe_num].service_id;
		req->svc_cfg[pipe_num].pipe_dir = svc_cfg[pipe_num].pipedir;
		req->svc_cfg[pipe_num].pipe_num = svc_cfg[pipe_num].pipenum;
	}

	pipe_num = QMI_WLANFW_MAX_NUM_SVC_V01;
	for (ext_pipe_num = 0; ext_pipe_num < req->ext_svc_cfg_len; ext_pipe_num++) {
		req->ext_svc_cfg[ext_pipe_num].service_id = svc_cfg[pipe_num].service_id;
		req->ext_svc_cfg[ext_pipe_num].pipe_dir = svc_cfg[pipe_num].pipedir;
		req->ext_svc_cfg[ext_pipe_num].pipe_num = svc_cfg[pipe_num].pipenum;
		pipe_num++;
	}

	/* set shadow v3 configuration */
	if (ab->hw_params->supports_shadow_regs) {
		req->shadow_reg_v3_valid = 1;
		req->shadow_reg_v3_len = min_t(u32,
					       ab->qmi.ce_cfg.shadow_reg_v3_len,
					       QMI_WLANFW_MAX_NUM_SHADOW_REG_V3_V01);
		memcpy(&req->shadow_reg_v3, ab->qmi.ce_cfg.shadow_reg_v3,
		       sizeof(u32) * req->shadow_reg_v3_len);
	} else {
		req->shadow_reg_v3_valid = 0;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_wlan_cfg_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_WLAN_CFG_REQ_V01,
			       QMI_WLANFW_WLAN_CFG_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_wlan_cfg_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to send wlan config request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed wlan config request, err = %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi wlan config request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	kfree(req);
	return ret;
}

static int ath12k_qmi_wlanfw_wlan_ini_send(struct ath12k_base *ab)
{
	struct qmi_wlanfw_wlan_ini_resp_msg_v01 resp = {};
	struct qmi_wlanfw_wlan_ini_req_msg_v01 req = {};
	struct qmi_txn txn;
	int ret;

	req.enable_fwlog_valid = true;
	req.enable_fwlog = 1;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_wlan_ini_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       ATH12K_QMI_WLANFW_WLAN_INI_REQ_V01,
			       QMI_WLANFW_WLAN_INI_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_wlan_ini_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "failed to send QMI wlan ini request: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "failed to receive QMI wlan ini request: %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "QMI wlan ini response failure: %d %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

void ath12k_qmi_firmware_stop(struct ath12k_base *ab)
{
	int ret;

	if (!test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags))
		return;

	clear_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags);

	ret = ath12k_qmi_wlanfw_mode_send(ab, ATH12K_FIRMWARE_MODE_OFF);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send wlan mode off\n");
		return;
	}
}
EXPORT_SYMBOL(ath12k_qmi_firmware_stop);

int ath12k_qmi_firmware_start(struct ath12k_base *ab,
			      u32 mode)
{
	int ret, timeout, calibration_time;

	ret = ath12k_qmi_wlanfw_wlan_ini_send(ab);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send wlan fw ini: %d\n", ret);
		return ret;
	}

	ret = ath12k_qmi_wlanfw_wlan_cfg_send(ab);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send wlan cfg:%d\n", ret);
		return ret;
	}

	ret = ath12k_qmi_wlanfw_mode_send(ab, mode);

	if (ath12k_cold_boot_cal_needed(ab) && ab->mm_cal_support) {
		calibration_time = jiffies;
		ab->in_coldboot_fwreset = true;

		ath12k_dbg(ab, ATH12K_DBG_QMI, "Coldboot calibration wait started\n");
		timeout = wait_event_timeout(ab->qmi.cold_boot_waitq, (ab->qmi.cal_done  == 1),
					     ATH12K_COLD_BOOT_FW_RESET_DELAY);

		if (timeout <= 0) {
			ath12k_warn(ab, "Coldboot Calibration failed - wait ended\n");
			ab->qmi.cal_timeout = 1;
			return -ETIMEDOUT;
		}
		ath12k_info(ab, "Coldboot calibration completed , calibration took %d ms\n",
			   jiffies_to_msecs(jiffies - calibration_time));
		ab->in_coldboot_fwreset = false;
	}

	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send wlan fw mode:%d\n", ret);
		return ret;
	}

	return 0;
}

int ath12k_qmi_m3_dump_upload_done_ind_send(struct ath12k_base *ab,
                                            u32 pdev_id, int status)
{
        struct qmi_wlanfw_m3_dump_upload_done_req_msg_v01 *req;
        struct qmi_wlanfw_m3_dump_upload_done_resp_msg_v01 *resp;
        struct qmi_txn txn;
        int ret;

        req = kzalloc(sizeof(*req), GFP_KERNEL);
        if (!req)
                return -ENOMEM;

        resp = kzalloc(sizeof(*resp), GFP_KERNEL);
        if (!resp) {
                kfree(req);
                return -ENOMEM;
        }

        req->pdev_id = pdev_id;
        req->status = status;

        ret = qmi_txn_init(&ab->qmi.handle, &txn,
                           qmi_wlanfw_m3_dump_upload_done_resp_msg_v01_ei, resp);
        if (ret < 0)
                goto out;

        ret =
        qmi_send_request(&ab->qmi.handle, NULL, &txn,
                         QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01,
                         QMI_WLANFW_M3_DUMP_UPLOAD_DONE_REQ_MSG_V01_MAX_MSG_LEN,
                         qmi_wlanfw_m3_dump_upload_done_req_msg_v01_ei, req);
        if (ret < 0) {
                qmi_txn_cancel(&txn);
                ath12k_warn(ab, "Failed to send M3 dump upload done request, err %d\n",
                            ret);
                goto out;
        }

        ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
        if (ret < 0)
                goto out;

        if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
                ath12k_warn(ab, "qmi M3 upload done req failed, result: %d, err: %d\n",
                            resp->resp.result, resp->resp.error);
                ret = -EINVAL;
                goto out;
        }
        ath12k_info(ab, "qmi m3 dump uploaded\n");

out:
        kfree(req);
        kfree(resp);
        return ret;
}

static void ath12k_qmi_event_m3_dump_upload_req(struct ath12k_qmi *qmi,
                                                void *data)
{
        struct ath12k_base *ab = qmi->ab;
        struct ath12k_qmi_m3_dump_upload_req_data *event_data = data;

        ath12k_coredump_m3_dump(ab, event_data);
}

int ath12k_qmi_process_coldboot_calibration(struct ath12k_base *ab)
{
	int timeout;
	int ret;
	unsigned long calibration_time;

	calibration_time = jiffies;

	ab->in_coldboot_fwreset = true;

	ret = ath12k_qmi_wlanfw_mode_send(ab, ATH12K_FIRMWARE_MODE_COLD_BOOT);

	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send wlan fw mode:%d\n", ret);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_QMI, "Coldboot calibration wait started\n");

	timeout = wait_event_timeout(ab->qmi.cold_boot_waitq,
				     (ab->qmi.cal_done  == 1),
				     ATH12K_COLD_BOOT_FW_RESET_DELAY);
	if (timeout <= 0) {
		ath12k_warn(ab, "Coldboot Calibration failed - wait ended\n");
	} else {
		ath12k_info(ab, "Coldboot calibration completed, calibration took %d ms\n",
              jiffies_to_msecs(jiffies - calibration_time));
	}

	ath12k_info(ab, "power down to restart firmware in mission mode\n");
	ath12k_qmi_firmware_stop(ab);

	ath12k_hif_power_down(ab, false);
	ath12k_qmi_free_target_mem_chunk(ab);
	ath12k_info(ab, "power up to restart firmware in mission mode\n");
	/* reset host fixed mem off to zero */
	ab->host_ddr_fixed_mem_off = 0;
	ath12k_hif_power_up(ab);
	ab->in_coldboot_fwreset = false;

	return 0;
}

static int
ath12k_qmi_driver_event_post(struct ath12k_qmi *qmi,
			     enum ath12k_qmi_event_type type,
			     void *data)
{
	struct ath12k_qmi_driver_event *event;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);
	if (!event)
		return -ENOMEM;

	event->type = type;
	event->data = data;

	spin_lock(&qmi->event_lock);
	list_add_tail(&event->list, &qmi->event_list);
	spin_unlock(&qmi->event_lock);

	queue_work(qmi->event_wq, &qmi->event_work);

	return 0;
}

void ath12k_qmi_trigger_host_cap(struct ath12k_base *ab)
{
	struct ath12k_qmi *qmi = &ab->qmi;

	if (ath12k_check_erp_power_down(ab->ag) &&
	    !ab->powerup_triggered)
		return;

	spin_lock(&qmi->event_lock);

	if (ath12k_qmi_get_event_block(qmi)) {
		ath12k_qmi_set_event_block(qmi, false);
		spin_unlock(&qmi->event_lock);
	} else {
		spin_unlock(&qmi->event_lock);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_QMI, "trigger host cap for device id %d\n",
		   ab->device_id);

	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_HOST_CAP, NULL);
}

static int ath12k_qmi_event_mlo_reconfig(struct ath12k_qmi *qmi)
{
	struct ath12k_base *ab = qmi->ab;
	int ret;

	if (ab->is_bypassed)
		return 0;

	ret = ath12k_qmi_mlo_reconfig_send(ab);
	if (ret < 0) {
		ath12k_warn(ab, "failed to send qmi host cap for device id %d: %d\n",
			    ab->device_id, ret);
		return ret;
	}

	return ret;
}

void ath12k_qmi_trigger_mlo_reconfig(struct ath12k_base *ab)
{
	struct ath12k_qmi *qmi = &ab->qmi;
	int ret = 0;

	spin_lock(&qmi->event_lock);

	if (ath12k_qmi_get_event_block(qmi))
		ath12k_qmi_set_event_block(qmi, false);

	spin_unlock(&qmi->event_lock);

	ath12k_err(ab, "trigger MLO reconfig for device id %d\n",
		   ab->device_id);
	ret = ath12k_qmi_event_mlo_reconfig(qmi);
	if (ret < 0)
		set_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
}

static bool ath12k_qmi_hw_group_host_cap_ready(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab && ab->is_bypassed)
			continue;

		if (!(ab && ab->qmi.num_radios != U8_MAX))
			return false;
		if (!ab->mm_cal_support && ath12k_cold_boot_cal_needed(ab) ) {
			/* don't send host caps until calibration is completed
			 * for all the radios
			 */
			return false;
		}
	}
	return true;
}

static int ath12k_qmi_fw_cfg_send_sync(struct ath12k_base *ab,
				       const u8 *data, u32 len,
				       enum wlanfw_cfg_type_v01 file_type)
{
	struct wlanfw_cfg_download_req_msg_v01 *req;
	struct wlanfw_cfg_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	const u8 *temp = data;
	unsigned int remaining = len;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	while (remaining) {
		req->file_type_valid = 1;
		req->file_type = file_type;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;

		if (remaining > QMI_WLANFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLANFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init(&ab->qmi.handle, &txn,
				   wlanfw_cfg_download_resp_msg_v01_ei,
				   resp);
		if (ret < 0) {
			ath12k_dbg(ab, ATH12K_DBG_QMI, "Failed to initialize txn for FW file download request, err: %d\n",
				   ret);
			goto err;
		}

		ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
				       QMI_WLANFW_CFG_DOWNLOAD_REQ_V01,
				       WLANFW_CFG_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
				       wlanfw_cfg_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			ath12k_dbg(ab, ATH12K_DBG_QMI, "Failed to send FW File download request, err: %d\n",
				   ret);
			goto err;
		}

		ret = qmi_txn_wait(&txn, ATH12K_QMI_WLANFW_TIMEOUT_MS);
		if (ret < 0) {
			ath12k_dbg(ab, ATH12K_DBG_QMI, "Failed to wait for response of FW File download request, err: %d\n",
				   ret);
			goto err;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			ath12k_dbg(ab, ATH12K_DBG_QMI, "FW file download request failed, result: %d, err: %d\n",
				   resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			goto err;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

err:
	kfree(req);
	kfree(resp);

	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_event_server_arrive(struct ath12k_qmi *qmi)
{
	struct ath12k_base *ab = qmi->ab, *partner_ab;
	struct ath12k_hw_group *ag = ab->ag;
	int ret, i;

	ath12k_qmi_phy_cap_send(ab);

	ret = ath12k_qmi_fw_ind_register_send(ab);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send FW indication QMI:%d\n", ret);
		return ret;
	}

	spin_lock(&qmi->event_lock);
	ath12k_qmi_set_event_block(qmi, true);
	spin_unlock(&qmi->event_lock);
	if (!ab->mm_cal_support && ath12k_cold_boot_cal_needed(ab) &&
			ab->qmi.cal_timeout == 0) {
		/* Coldboot calibration mode */
		ath12k_qmi_trigger_host_cap(ab);
	} else {
		mutex_lock(&ag->mutex);
		ath12k_core_hw_group_set_mlo_capable(ag);
		if (ath12k_qmi_hw_group_host_cap_ready(ag)) {

			for (i = 0; i < ag->num_devices; i++) {
				partner_ab = ag->ab[i];
				if (!partner_ab)
					continue;
				ath12k_qmi_trigger_host_cap(partner_ab);
			}
		}
		mutex_unlock(&ag->mutex);
	}

	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_event_mem_request(struct ath12k_qmi *qmi, void *data)
{
	struct ath12k_base *ab = qmi->ab;
	int ret;

	ret = ath12k_qmi_respond_fw_mem_request(ab, data);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to respond fw mem req:%d\n", ret);
		return ret;
	}

	return ret;
}

static noinline_for_stack
int ath12k_qmi_respond_fw_cma_mem_request(struct ath12k_base *ab, void *data)
{
	struct qmi_wlanfw_respond_cma_mem_req_msg_v01 *req;
	struct qmi_wlanfw_respond_cma_mem_resp_msg_v01 resp = {};
	struct qmi_txn txn;
	int ret, idx, i, seg_start_idx;
	int *curr_seg_cnt = data;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->mem_seg_len = *curr_seg_cnt;
	seg_start_idx = ab->qmi.mem_seg_count - *curr_seg_cnt;
	for (idx = 0, i = seg_start_idx; i < ab->qmi.mem_seg_count; i++, idx++) {
		req->mem_seg[idx].addr = ab->qmi.target_mem[i].paddr;
		req->mem_seg[idx].size = ab->qmi.target_mem[i].size;
		req->mem_seg[idx].type = ab->qmi.target_mem[i].type;
		ath12k_dbg(ab, ATH12K_DBG_QMI,
			   "qmi cma req mem_seg[%d] %pad %u %u\n", idx,
			   &ab->qmi.target_mem[i].paddr,
			   ab->qmi.target_mem[i].size,
			   ab->qmi.target_mem[i].type);
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_respond_cma_mem_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_RESPOND_CMA_MEM_REQ_V01,
			       QMI_WLANFW_RESPOND_CMA_MEM_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_respond_cma_mem_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		ath12k_warn(ab, "qmi failed to respond cma mem request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi cma mem request wait failed, err = %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "respond cma mem req failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
	}
out:
	kfree(req);
	return ret;
}

static void ath12k_qmi_msg_cma_mem_request_cb(struct qmi_handle *qmi_hdl,
					       struct sockaddr_qrtr *sq,
					       struct qmi_txn *txn,
					       const void *data)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;
	const struct qmi_wlanfw_request_cma_mem_ind_msg_v01 *msg = data;
	struct target_mem_chunk *req_mem;
	int *event_data;
	int i, ret;
	u32 req_mem_seg_count;

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi firmware request cma memory\n");

	if (msg->mem_seg_len == 0 ||
	    msg->mem_seg_len > ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01) {
		ath12k_warn(ab, "Invalid cma memory segment length: %u\n",
			    msg->mem_seg_len);
		return;
	}

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	req_mem = kcalloc(msg->mem_seg_len, sizeof(*req_mem), GFP_KERNEL);
	if (!req_mem) {
		kfree(event_data);
		return;
	}

	req_mem_seg_count = msg->mem_seg_len;
	for (i = 0; i < req_mem_seg_count; i++) {
		req_mem[i].type = msg->mem_seg[i].type;
		req_mem[i].size = msg->mem_seg[i].size;
		/* Propagate the persist flag from FW so that
		 * ath12k_qmi_alloc_target_mem_chunk() parks this chunk
		 * across recovery just like CALDB segments.
		 */
		req_mem[i].persist = msg->mem_seg[i].persist;
		ath12k_dbg(ab, ATH12K_DBG_QMI,
			   "qmi cma mem seg type %d size %d persist %d\n",
			   msg->mem_seg[i].type, msg->mem_seg[i].size,
			   msg->mem_seg[i].persist);
	}

	ret = ath12k_qmi_alloc_target_mem_chunk(ab, req_mem, req_mem_seg_count);
	if (ret != req_mem_seg_count) {
		ath12k_warn(ab, "qmi failed to alloc cma target memory: %d\n", ret);
		kfree(event_data);
		kfree(req_mem);
		return;
	}

	kfree(req_mem);
	*event_data = ret;
	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_REQUEST_CMA_MEM, event_data);
}

static int ath12k_qmi_request_device_info(struct ath12k_base *ab)
{
	struct qmi_wlanfw_device_info_req_msg_v01 req;
	struct qmi_wlanfw_device_info_resp_msg_v01 resp;
	struct qmi_txn txn = {};
	void *bar_addr_va = NULL;
	int ret = 0;

	/*device info message only supported for internal-PCI devices */
#ifdef CPTCFG_QCN_EXTN
	if (ab->hw_rev != ATH12K_HW_QCN6432_HW10 &&
		ab->hw_rev != ATH12K_HW_QCN9160_HW10)
		return 0;
#else
	if (ab->hw_rev != ATH12K_HW_QCN6432_HW10)
		return 0;
#endif /* CPTCFG_QCN_EXTN */

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_device_info_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_DEVICE_INFO_REQ_V01,
			       QMI_WLANFW_DEVICE_INFO_REQ_MSG_V01,
			       qmi_wlanfw_device_info_req_msg_v01_ei, &req);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to send target device info request, err = %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed target device info request %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "qmi device info req failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

	if (!resp.bar_addr_valid || !resp.bar_size_valid) {
		ath12k_warn(ab, "qmi device info response invalid, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
	if (!resp.bar_addr ||
	    resp.bar_size != QCN6432_DEVICE_BAR_SIZE) {
		ath12k_warn(ab, "qmi device info invalid addr and size, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}

	bar_addr_va = ioremap(resp.bar_addr, resp.bar_size);

	if (!bar_addr_va) {
		ath12k_warn(ab, "qmi device info ioremap failed\n");
		ab->mem_len = 0;
		ret = -EIO;
		goto out;
	}

	ab->mem = bar_addr_va;
	ab->mem_len = resp.bar_size;

	ath12k_dbg(ab, ATH12K_DBG_QMI, "Device BAR Info pa: 0x%llx, va: 0x%p, size: 0x%lx\n",
		   resp.bar_addr, ab->mem, ab->mem_len);

	ath12k_hif_config_static_window(ab);
	return 0;
out:
	return ret;
}

/* clang stack usage explodes if this is inlined */
static noinline_for_stack
int ath12k_qmi_event_load_bdf(struct ath12k_qmi *qmi)
{
	struct ath12k_base *ab = qmi->ab;
	int ret;

	ret = ath12k_qmi_request_target_cap(ab);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to req target capabilities:%d\n", ret);
		return ret;
	}

	ret = ath12k_qmi_request_device_info(ab);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to req device info:%d\n", ret);
		return ret;
	}

	ret = ath12k_qmi_load_bdf_qmi(ab, ATH12K_QMI_BDF_TYPE_REGDB);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to load regdb file:%d\n", ret);
		ath12k_critical_failure_trigger(ab, ATH12K_BDF_LOAD_FAILURE);
		return ret;
	}

	ret = ath12k_qmi_load_bdf_qmi(ab, ATH12K_QMI_BDF_TYPE_ELF);
	if (ret < 0) {
		ath12k_warn(ab, "qmi failed to load board data file:%d\n", ret);
		ath12k_critical_failure_trigger(ab, ATH12K_BDF_LOAD_FAILURE);
		return ret;
	}

	if (ab->rxgainlut_support) {
		ret = ath12k_qmi_load_bdf_qmi(ab, ATH12K_QMI_BDF_TYPE_RXGAINLUT);
		if (ret < 0)
			ath12k_warn(ab, "qmi failed to load rxgainlut: %d\n", ret);
	}

	if (ab->hw_params->download_calib) {
		ret = ath12k_qmi_load_bdf_qmi(ab, ATH12K_QMI_BDF_TYPE_CALIBRATION);
		if (ret < 0) {
			ath12k_warn(ab, "qmi failed to load calibrated data :%d\n", ret);
			ath12k_critical_failure_trigger(ab, ATH12K_BDF_LOAD_FAILURE);
		}
	}

	if (ab->qmi.ext_fw_bin_download_support) {
		ret = ath12k_qmi_wlanfw_ext_fw_bin_mem_info_send(ab);
		if (ret < 0) {
			ath12k_warn(ab, "qmi failed to send FW bin info req:%d\n", ret);
			return ret;
		}
	} else {
		ret = ath12k_qmi_wlanfw_m3_info_send(ab);
		if (ret < 0) {
			ath12k_warn(ab, "qmi failed to send m3 info req:%d\n", ret);
			return ret;
		}
	}
	return ret;
}

static void ath12k_qmi_msg_mem_request_cb(struct qmi_handle *qmi_hdl,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn,
					  const void *data)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;
	const struct qmi_wlanfw_request_mem_ind_msg_v01 *msg = data;
	int i, ret, *event_data;
	struct target_mem_chunk *req_mem = NULL;
	u32 req_mem_seg_count;

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi firmware request memory request\n");

	if (msg->mem_seg_len == 0 ||
	    msg->mem_seg_len > ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01)
		ath12k_warn(ab, "Invalid memory segment length: %u\n",
			    msg->mem_seg_len);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	req_mem = kcalloc(msg->mem_seg_len,
			  sizeof(*req_mem),
			  GFP_KERNEL);
	if (!req_mem) {
		kfree(event_data);
		return;
	}

	req_mem_seg_count = msg->mem_seg_len;

	for (i = 0; i < req_mem_seg_count; i++) {
		req_mem[i].type = msg->mem_seg[i].type;
		req_mem[i].size = msg->mem_seg[i].size;
		ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi mem seg type %d size %d\n",
			   msg->mem_seg[i].type, msg->mem_seg[i].size);
	}

	if (test_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags)) {
		ret = ath12k_qmi_assign_target_mem_chunk(ab, req_mem, req_mem_seg_count);
		if (ret) {
			ath12k_warn(ab, "failed to assign qmi target memory: %d\n",
				    ret);
			kfree(event_data);
			kfree(req_mem);
			return;
		}
		ret = req_mem_seg_count;
	} else {
		ret = ath12k_qmi_alloc_target_mem_chunk(ab, req_mem, req_mem_seg_count);
		if (ret != -EAGAIN && ret != req_mem_seg_count) {
			ath12k_warn(ab, "qmi failed to alloc target memory: %d\n",
				    ret);
			kfree(event_data);
			kfree(req_mem);
			return;
		}
	}

	kfree(req_mem);
	*event_data = ret;
	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_REQUEST_MEM, event_data);
}

static void ath12k_qmi_msg_mem_ready_cb(struct qmi_handle *qmi_hdl,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn,
					const void *decoded)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi firmware memory ready indication\n");
	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_FW_MEM_READY, NULL);
}

static void ath12k_qmi_msg_fw_ready_cb(struct qmi_handle *qmi_hdl,
				       struct sockaddr_qrtr *sq,
				       struct qmi_txn *txn,
				       const void *decoded)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi firmware ready\n");
	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_FW_READY, NULL);
}

static void ath12k_qmi_msg_cold_boot_cal_done_cb(struct qmi_handle *qmi_hdl,
						 struct sockaddr_qrtr *sq,
						 struct qmi_txn *txn,
						 const void *decoded)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl,
					      struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;

	ab->qmi.cal_done = 1;
	wake_up(&ab->qmi.cold_boot_waitq);
	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi cold boot calibration done\n");
}

static void ath12k_qmi_m3_dump_upload_req_ind_cb(struct qmi_handle *qmi_hdl,
                                                 struct sockaddr_qrtr *sq,
                                                 struct qmi_txn *txn,
                                                 const void *data)
{
        struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
        struct ath12k_base *ab = qmi->ab;
        const struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01 *msg = data;
        struct ath12k_qmi_m3_dump_upload_req_data *event_data;

        ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi m3 dump memory request\n");

        event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
        if (!event_data)
                return;

        event_data->pdev_id = msg->pdev_id;
        event_data->addr = msg->addr;
        event_data->size = msg->size;

        ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_M3_DUMP_UPLOAD_REQ,
                                     event_data);
}

int ath12k_enable_fwlog(struct ath12k_base *ab)
{
	struct wlfw_ini_req_msg_v01 *req;
	struct wlfw_ini_resp_msg_v01 resp = {};
	struct qmi_txn txn = {};
	int ret = 0;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->enablefwlog_valid = 1;
	req->enablefwlog = 1;

	ret = qmi_txn_init(&ab->qmi.handle, &txn, wlfw_ini_resp_msg_v01_ei, &resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLFW_INI_REQ_V01,
			       WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ini_req_msg_v01_ei, req);

	if (ret < 0) {
		ath12k_warn(ab, "Failed to send init request for enabling fwlog = %d\n",
			    ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		ath12k_warn(ab, "fwlog enable wait for resp failed: %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ath12k_warn(ab, "fwlog enable request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
	}
out:
	kfree(req);
	return ret;
}

static const struct qmi_msg_handler ath12k_qmi_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_REQUEST_MEM_IND_V01,
		.ei = qmi_wlanfw_request_mem_ind_msg_v01_ei,
		.decoded_size = sizeof(struct qmi_wlanfw_request_mem_ind_msg_v01),
		.fn = ath12k_qmi_msg_mem_request_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_MEM_READY_IND_V01,
		.ei = qmi_wlanfw_mem_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct qmi_wlanfw_fw_mem_ready_ind_msg_v01),
		.fn = ath12k_qmi_msg_mem_ready_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_READY_IND_V01,
		.ei = qmi_wlanfw_fw_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct qmi_wlanfw_fw_ready_ind_msg_v01),
		.fn = ath12k_qmi_msg_fw_ready_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_COLD_BOOT_CAL_DONE_IND_V01,
		.ei = qmi_wlanfw_cold_boot_cal_done_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct qmi_wlanfw_fw_cold_cal_done_ind_msg_v01),
		.fn = ath12k_qmi_msg_cold_boot_cal_done_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_M3_DUMP_UPLOAD_REQ_IND_V01,
		.ei = qmi_wlanfw_m3_dump_upload_req_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01),
		.fn = ath12k_qmi_m3_dump_upload_req_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLANFW_REQUEST_MEM_FREE_IND_V01,
		.ei = qmi_wlanfw_request_mem_free_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct qmi_wlanfw_request_mem_free_ind_msg_v01),
		.fn = ath12k_qmi_msg_mem_free_request_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLANFW_REQUEST_CMA_MEM_IND_V01,
		.ei = qmi_wlanfw_request_cma_mem_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct qmi_wlanfw_request_cma_mem_ind_msg_v01),
		.fn = ath12k_qmi_msg_cma_mem_request_cb,
	},

	/* end of list */
	{},
};

static int ath12k_qmi_ops_new_server(struct qmi_handle *qmi_hdl,
				     struct qmi_service *service)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;
	struct sockaddr_qrtr *sq = &qmi->sq;
	int ret;

	sq->sq_family = AF_QIPCRTR;
	sq->sq_node = service->node;
	sq->sq_port = service->port;

	ret = kernel_connect(qmi_hdl->sock, (struct sockaddr *)sq,
			     sizeof(*sq), 0);
	if (ret) {
		ath12k_warn(ab, "qmi failed to connect to remote service %d\n", ret);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi wifi fw qmi service connected\n");
	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_SERVER_ARRIVE, NULL);

	return ret;
}

static void ath12k_qmi_ops_del_server(struct qmi_handle *qmi_hdl,
				      struct qmi_service *service)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	struct ath12k_base *ab = qmi->ab;

	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi wifi fw del server\n");
	ath12k_qmi_driver_event_post(qmi, ATH12K_QMI_EVENT_SERVER_EXIT, NULL);
}

static const struct qmi_ops ath12k_qmi_ops = {
	.new_server = ath12k_qmi_ops_new_server,
	.del_server = ath12k_qmi_ops_del_server,
};

static int ath12k_qmi_fw_cfg(struct ath12k_base *ab)
{
	struct ath12k_board_data bd;
	int ret;

	ret = ath12k_core_fetch_fw_cfg(ab, &bd);
	if (ret < 0) {
		ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi failed to fetch FW CFG file:%d\n", ret);
		goto out;
	}
	ret = ath12k_qmi_fw_cfg_send_sync(ab, bd.data,
					  bd.len, WLANFW_CFG_FILE_V01);
	if (ret < 0)
		ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi failed to load FW CFG file\n");

out:
	ath12k_core_free_bdf(ab, &bd);
	ath12k_dbg(ab, ATH12K_DBG_QMI, "qmi FW CFG download sequence completed, ret: %d\n",
		   ret);

	return ret;
}

static int ath12k_qmi_event_host_cap(struct ath12k_qmi *qmi)
{
	struct ath12k_base *ab = qmi->ab;
	int ret;

	ret = ath12k_qmi_host_cap_send(ab);
	if (ret < 0) {
		ath12k_warn(ab, "failed to send qmi host cap for device id %d: %d\n",
			    ab->device_id, ret);
		return ret;
	}

	if (!ab->fw_cfg_support) {
		ath12k_dbg(ab, ATH12K_DBG_QMI, "FW CFG file is not supported\n");
		return 0;
	}

	ret = ath12k_qmi_fw_cfg(ab);

	return ret;
}

static int ath12k_wait_for_gic_msi(struct ath12k_base *ab)
{
	int timeout;

	if (ab->hw_rev != ATH12K_HW_QCN6432_HW10)
		return 0;

	timeout = wait_event_timeout(ab->ipci.gic_msi_waitq,
				     (ab->ipci.gic_enabled == 1),
				     ATH12K_RCV_GIC_MSI_HDLR_DELAY);
	if (timeout <= 0) {
		ath12k_warn(ab, "Receive gic msi handler timed out\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static void ath12k_qmi_driver_event_work(struct work_struct *work)
{
	struct ath12k_qmi *qmi = container_of(work, struct ath12k_qmi,
					      event_work);
	struct ath12k_qmi_driver_event *event;
	struct ath12k_base *ab = qmi->ab;
	bool is_ready = false;
	int ret;

	spin_lock(&qmi->event_lock);

	while (!list_empty(&qmi->event_list)) {
		event = list_first_entry(&qmi->event_list,
					 struct ath12k_qmi_driver_event, list);
		list_del(&event->list);
		spin_unlock(&qmi->event_lock);

		if (test_bit(ATH12K_FLAG_UNREGISTERING, &ab->dev_flags))
			goto skip;

		switch (event->type) {
		case ATH12K_QMI_EVENT_SERVER_ARRIVE:
			ret = ath12k_qmi_event_server_arrive(qmi);
			if (ret < 0)
				set_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
			break;
		case ATH12K_QMI_EVENT_SERVER_EXIT:
			set_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);
			clear_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags);
			break;
		case ATH12K_QMI_EVENT_REQUEST_MEM:
			ret = ath12k_qmi_event_mem_request(qmi, event->data);
			if (ret < 0)
				set_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
			break;
		case ATH12K_QMI_EVENT_FW_MEM_READY:
			ret = ath12k_qmi_event_load_bdf(qmi);
			if (ret < 0)
				set_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
			break;
		case ATH12K_QMI_EVENT_FW_READY:
			clear_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
			if (test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags) ||
			    (ath12k_check_erp_power_down(ab->ag) &&
			    ath12k_hw_group_recovery_in_progress(ab->ag))) {
				queue_work(ab->workqueue, &ab->restart_work);
				break;
			}
			ret = ath12k_wait_for_gic_msi(ab);
			if (ret) {
				ath12k_warn(ab, "failed to get qgic handler for dev %d ret: %d\n",
					    ab->hw_rev, ret);
				break;
			}
			if (ath12k_cold_boot_cal_needed(ab) && !ab->mm_cal_support) {
				ath12k_qmi_process_coldboot_calibration(ab);
			} else {
				clear_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);
				clear_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags);
				if (ath12k_check_erp_power_down(ab->ag)) {
					ret = ath12k_hal_srng_init(ab);
					if (ret)
						break;
				}

				ret = ath12k_core_qmi_firmware_ready(ab, &is_ready);
				if (!ret)
					set_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE,
						&ab->dev_flags);

				if (ret || !is_ready)
					break;

				ath12k_core_en_ssr(ab, ATH12K_FW_RECOVERY_ENABLE_MODE2);
			}

			break;
		case ATH12K_QMI_EVENT_HOST_CAP:
			ret = ath12k_qmi_event_host_cap(qmi);
			if (ret < 0)
				set_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
			break;
		case ATH12K_QMI_EVENT_COLD_BOOT_CAL_DONE:
			break;
		case ATH12K_QMI_EVENT_M3_DUMP_UPLOAD_REQ:
			ath12k_qmi_event_m3_dump_upload_req(qmi, event->data);
			break;
		case ATH12K_QMI_EVENT_REQUEST_CMA_MEM:
			ret = ath12k_qmi_respond_fw_cma_mem_request(ab, event->data);
			if (ret < 0)
				set_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags);
			break;
		default:
			ath12k_warn(ab, "invalid event type: %d", event->type);
			break;
		}

skip:
		kfree(event);
		spin_lock(&qmi->event_lock);
	}
	spin_unlock(&qmi->event_lock);
}

int ath12k_qmi_init_service(struct ath12k_base *ab)
{
	int ret;
	int tgt_mem_mode;

	memset(&ab->qmi.target, 0, sizeof(struct target_info));
	memset(&ab->qmi.target_mem, 0, sizeof(struct target_mem_chunk));
	ab->qmi.ab = ab;

	if (!of_property_read_u32(ab->dev->of_node, "qcom,tgt_mem_mode", &tgt_mem_mode))
		ab->qmi.target_mem_mode = tgt_mem_mode;
	else
		ab->qmi.target_mem_mode = ATH12K_QMI_TARGET_MEM_MODE;

	ret = ath12k_hif_set_qrtr_endpoint_id(ab);
	if (ret) {
		ath12k_warn(ab, "failed to set QRTR endpoint ID: %d\n", ret);
		ath12k_warn(ab, "only one device per system will be supported\n");
	}

	ret = qmi_handle_init(&ab->qmi.handle, ATH12K_QMI_RESP_LEN_MAX,
			      &ath12k_qmi_ops, ath12k_qmi_msg_handlers);
	if (ret < 0) {
		ath12k_warn(ab, "failed to initialize qmi handle\n");
		return ret;
	}

	ab->qmi.event_wq = alloc_ordered_workqueue("ath12k_qmi_driver_event", 0);
	if (!ab->qmi.event_wq) {
		ath12k_err(ab, "failed to allocate workqueue\n");
		return -EFAULT;
	}

	INIT_LIST_HEAD(&ab->qmi.event_list);
	spin_lock_init(&ab->qmi.event_lock);
	INIT_WORK(&ab->qmi.event_work, ath12k_qmi_driver_event_work);

	ret = qmi_add_lookup(&ab->qmi.handle, ATH12K_QMI_WLFW_SERVICE_ID_V01,
			     ATH12K_QMI_WLFW_SERVICE_VERS_V01,
			     ab->qmi.service_ins_id);
	if (ret < 0) {
		ath12k_warn(ab, "failed to add qmi lookup\n");
		destroy_workqueue(ab->qmi.event_wq);
		return ret;
	}

#ifdef CPTCFG_ATHDEBUG
	ret = athdbg_if_get_service(ab, ATHDBG_SRV_QMI_INIT);
#endif

	return ret;
}

void ath12k_qmi_deinit_service(struct ath12k_base *ab)
{
	struct device *dev;
	int i;

	if (!ab->qmi.ab)
		return;

#ifdef CPTCFG_ATHDEBUG
	athdbg_if_get_service(ab, ATHDBG_SRV_QMI_DEINIT);
#endif
	qmi_handle_release(&ab->qmi.handle);
	cancel_work_sync(&ab->qmi.event_work);
	destroy_workqueue(ab->qmi.event_wq);
	ath12k_qmi_m3_free(ab);
	ath12k_qmi_ext_fw_bin_clear(ab);
	/* Park active persist chunks into the pool first, then free the
	 * entire pool. This order matters: free_target_mem_chunk() parks
	 * persist chunks into persist_mem[] instead of freeing them, so
	 * the pool must be freed afterwards or those chunks leak on unload.
	 */
	ath12k_qmi_free_resource(ab);

	dev = ab->qmi_mem_dev.rmem_inited ? &ab->qmi_mem_dev.dev : ab->dev;
	for (i = 0; i < ab->qmi.persist_mem_count; i++) {
		if (!ab->qmi.persist_mem[i].v.addr)
			continue;
		dma_free_coherent(dev,
				  ab->qmi.persist_mem[i].prev_size,
				  ab->qmi.persist_mem[i].v.addr,
				  ab->qmi.persist_mem[i].paddr);
		ab->qmi.persist_mem[i].v.addr = NULL;
	}
	ab->qmi.persist_mem_count = 0;
	ab->qmi.ab = NULL;
}

void ath12k_qmi_free_resource(struct ath12k_base *ab)
{
	ath12k_qmi_free_target_mem_chunk(ab);
#ifdef CPTCFG_ATHDEBUG
	athdbg_if_get_service(ab, ATHDBG_SRV_QDSS_MEM_FREE);
#endif
}
EXPORT_SYMBOL(ath12k_qmi_free_resource);
