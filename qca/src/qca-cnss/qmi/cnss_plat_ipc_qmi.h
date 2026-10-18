/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

/* Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved. */

#ifndef _CNSS_PLAT_IPC_QMI_H
#define _CNSS_PLAT_IPC_QMI_H

#include "cnss_plat_ipc_service_v01.h"

/* As the value of CNSS_PLAT_IPC_MAX_QMI_CLIENTS will keep changing
 * addition of new QMI client, it cannot be kept in IDL as change in
 * existing value can cause backward compatibily issue. Keep it here
 * and update its value with new QMI client ID added in enum in IDL.
 */
#define CNSS_PLAT_IPC_MAX_QMI_CLIENTS CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01

/**
 * struct cnss_plat_user_config: Config options provided by user space
 * @dms_mac_addr_supported: DMS MAC address provisioning support
 * @qdss_hw_trace_override: QDSS config for HW trace enable
 * @cal_file_available_bitmask: Calibration file available
 */
struct cnss_plat_ipc_daemon_config {
	u8 dms_mac_addr_supported;
	u8 qdss_hw_trace_override;
	u32 cal_file_available_bitmask;
};

typedef void (*cnss_plat_ipc_connection_update)(void *cb_ctx,
						bool connection_status);
typedef void (*cnss_plat_ipc_config_param_req)(uint32_t instance_id,
			     enum cnss_plat_ipc_qmi_config_param_type_v01 param,
			     uint64_t value);

struct cnss_plat_ipc_qmi_cb {
	cnss_plat_ipc_connection_update connection_update_cb;
	cnss_plat_ipc_config_param_req config_param_cb;
};

int cnss_plat_ipc_register(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			   struct cnss_plat_ipc_qmi_cb *ipc_qmi_callbacks,
			   void *cb_ctx);

void cnss_plat_ipc_unregister(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			      void *cb_ctx);
int cnss_plat_ipc_qmi_file_download(enum cnss_plat_ipc_qmi_client_id_v01
				    client_id, char *file_name, char *buf,
				    u32 *size);
int cnss_plat_ipc_qmi_file_upload(enum cnss_plat_ipc_qmi_client_id_v01
				  client_id, char *file_name, u8 *file_buf,
				  u32 file_size);

struct cnss_plat_ipc_daemon_config *cnss_plat_ipc_qmi_daemon_config(void);
bool is_ipc_qmi_client_connected(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
				 unsigned int timeout);
int cnss_plat_ipc_qmi_svc_init(void);
void cnss_plat_ipc_qmi_svc_exit(void);
#endif
