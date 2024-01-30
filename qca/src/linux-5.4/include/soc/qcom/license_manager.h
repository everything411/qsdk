/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LICENSE_MANAGER_H__
#define __LICENSE_MANAGER_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/of_device.h>

#define QMI_LICENSE_MANAGER_SERVICE_MAX_MSG_LEN 10259
#define MAX_NUM_OF_LICENSES 20

#define QMI_LM_SERVICE_ID_V01 0x0423
#define QMI_LM_SERVICE_VERS_V01 0x01

#define QMI_LM_FEATURE_LIST_REQ_V01 0x0102
#define QMI_LM_FEATURE_LIST_RESP_V01 0x0102

#define QMI_LM_MAX_CHIPINFO_ID_LEN_V01 32
#define QMI_LM_MAX_FEATURE_LIST_V01 10
#define QMI_LM_MAX_LICENSE_SIZE_V01 10240

#define CLIENT_MAX 		6
#define FEATURE_LIST_MAX 	100
#define FILE_NAME_MAX 		128

struct qmi_lm_feature_list_req_msg_v01 {
	u32 reserved;
	u8 feature_list_valid;
	u32 feature_list_len;
	u32 feature_list[QMI_LM_MAX_FEATURE_LIST_V01];
};
#define QMI_LM_FEATURE_LIST_REQ_MSG_V01_MAX_MSG_LEN 51

struct qmi_lm_feature_list_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define QMI_LM_FEATURE_LIST_RESP_MSG_V01_MAX_MSG_LEN 7

struct lm_svc_ctx {
	struct device *dev;
	struct qmi_handle *lm_svc_hdl;
	struct list_head clients_connected;
	struct list_head clients_feature_list;
	bool license_feature;
	bool license_buf_valid;
	void *license_buf;
	dma_addr_t license_dma_addr;
	size_t license_buf_len;
	bool soc_bounded;
};

struct client_info {
	int sq_node;
	int sq_port;
	struct list_head node;
};

struct feature_info {
	int sq_node;
	int sq_port;
	uint32_t reserved;
	uint32_t len;
	uint32_t list[QMI_LM_MAX_FEATURE_LIST_V01];
	struct list_head node;
};

struct target_info {
	int sq_node;
	int sq_port;
	uint32_t list_len;
	uint32_t list[FEATURE_LIST_MAX];
	uint32_t lic_file_info_len;
	int file_status[MAX_NUM_OF_LICENSES];
};

struct client_target_info {
	int info_len;
	struct target_info info[CLIENT_MAX];
	int num_of_file;
	char file_name[MAX_NUM_OF_LICENSES][FILE_NAME_MAX];
};

#define GET_FID_INFO 		_IOWR('L', 1, struct client_target_info)
#define LICENSE_RESCAN 		_IO('L', 2)

enum req_type {
	INTERNAL,
	EXTERNAL,
	TYPE_MAX
};

#ifdef CONFIG_QTI_LICENSE_MANAGER
void *lm_get_license(enum req_type type, dma_addr_t *dma_addr, size_t *buf_len, dma_addr_t nonce_dma_addr);
void lm_free_license(void *buf, dma_addr_t dma_addr, size_t buf_len);
#else
static inline void *lm_get_license(enum req_type type, dma_addr_t *dma_addr, size_t *buf_len, dma_addr_t nonce_dma_addr)
{
	return NULL;
}

static inline void lm_free_license(void *buf, dma_addr_t dma_addr, size_t buf_len)
{
}
#endif /* CONFIG_QTI_LICENSE_MANAGER */
#endif /* __LICENSE_MANAGER_H___ */
