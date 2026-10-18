// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include "athdbg_qmi.h"
#include "athdbg_qdss.h"
#include "athdbg_core.h"
#include "../core.h"
#ifdef CONFIG_UPSTREAM_BUILD
#include <linux/devcoredump.h>
#endif

#include <linux/vmalloc.h>

#ifdef CONFIG_UPSTREAM_BUILD
#define NUM_GENERIC_QMI_HANDLER	3
#else
#define NUM_GENERIC_QMI_HANDLER	7
#endif

extern struct ath_debug_base *athdbg_base;

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

const struct qmi_elem_info qmi_wlanfw_respond_mem_req_msg_v01_ei[] = {
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

const struct qmi_elem_info qmi_wlanfw_mem_cfg_s_v01_ei[] = {
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

const struct qmi_elem_info qmi_wlanfw_request_mem_ind_msg_v01_ei[] = {
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

static struct qmi_msg_handler athdbg_qmi_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01,
		.ei = qmi_wlanfw_request_mem_ind_msg_v01_ei,
		.decoded_size =
				sizeof(struct qmi_wlanfw_request_mem_ind_msg_v01),
		.fn = athdbg_wlfw_qdss_trace_req_mem_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_SAVE_IND_V01,
		.ei = qmi_wlanfw_qdss_trace_save_ind_msg_v01_ei,
		.decoded_size =
				sizeof(struct qmi_wlanfw_qdss_trace_save_ind_msg_v01),
		.fn = athdbg_wlfw_qdss_trace_save_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_DDR_DUMP_REGION_IND_V01,
		.ei = qmi_wlanfw_ddr_dump_region_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlanfw_ddr_dump_region_ind_msg_v01),
		.fn = athdbg_qmi_wlanfw_ddr_dump_region_ind_cb,
	},
	/* end of list */
	{},
};

struct qmi_elem_info qmi_wlanfw_ddr_dump_region_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlanfw_ddr_dump_region_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct qmi_wlanfw_mem_seg_resp_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlanfw_ddr_dump_region_ind_msg_v01,
					   mem_seg),
		.ei_array      = qmi_wlanfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlanfw_ddr_dump_region_ind_msg_v01,
					   file_name_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLANFW_MAX_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlanfw_ddr_dump_region_ind_msg_v01,
					   file_name),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlanfw_ddr_dump_region_ind_msg_v01,
					   indication_type_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlanfw_ddr_dump_region_ind_msg_v01,
					   indication_type),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
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

struct qmi_elem_info qmi_wlanfw_ddr_dump_upload_done_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   qmi_wlanfw_ddr_dump_upload_done_req_msg_v01,
					   status),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_wlanfw_ddr_dump_upload_done_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   qmi_wlanfw_ddr_dump_upload_done_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

#ifdef CONFIG_UPSTREAM_BUILD
#define NUM_HANDLER 5
#else
#define NUM_HANDLER 11
#endif

struct qmi_msg_handler *athdbg_append_dbg_handler(const struct qmi_msg_handler *handlers)
{
	struct qmi_msg_handler *wdbg_handlers;
	int ath12k_qmi_handler_index;

	wdbg_handlers = kzalloc(sizeof(*wdbg_handlers) * NUM_HANDLER, GFP_KERNEL);

	if (!wdbg_handlers)
		return NULL;

	ath12k_qmi_handler_index = NUM_GENERIC_QMI_HANDLER;

	memcpy(wdbg_handlers, handlers,
	       ath12k_qmi_handler_index * sizeof(struct qmi_msg_handler));

	//append debug handler
	memcpy(&wdbg_handlers[ath12k_qmi_handler_index], athdbg_qmi_msg_handlers,
	       sizeof(struct qmi_msg_handler) * 3);

	return wdbg_handlers;
}

int athdbg_qmi_handle_init(struct qmi_handle *qmi, size_t recv_buf_size,
			   const struct qmi_ops *ops,
			   const struct qmi_msg_handler *handlers)
{
	int ret;
	struct qmi_msg_handler *wdbg_handlers;

	wdbg_handlers = athdbg_append_dbg_handler(handlers);

	if (wdbg_handlers) {
		ret = qmi_handle_init(qmi, ATH12K_QMI_RESP_LEN_MAX, ops, wdbg_handlers);

		if (ret < 0) {
			kfree(wdbg_handlers);
			goto out;
		}

		athdbg_base->wdbg_handlers[athdbg_base->wdbg_handlers_cnt++] =
							wdbg_handlers;
	} else {
		pr_err("Fallback to qmi int without dump handlers");
		ret = qmi_handle_init(qmi, ATH12K_QMI_RESP_LEN_MAX, ops, handlers);
	}

out:
	return ret;
}
EXPORT_SYMBOL(athdbg_qmi_handle_init);

static int athdbg_coredump_ddr_dump(struct ath12k_base *ab,
				    struct ath12k_qmi_event_ddr_dump_region *event_data)
{
	struct ath12k_dump_segment *segment;
	void *dump = NULL;
	size_t offset = 0;
	int i;

	segment = vzalloc(sizeof(*segment));
	if (!segment)
		return -ENOMEM;

	if (event_data->total_size) {
		dump = vzalloc(event_data->total_size);
		if (!dump) {
			vfree(segment);
			return -ENOMEM;
		}
	} else {
		pr_err("Invalid total_size: 0\n");
		vfree(segment);
		return -EINVAL;
	}

	for (i = 0; i < event_data->mem_seg_len; i++) {
		if (!event_data->mem_seg[i].va || !event_data->mem_seg[i].size)
			continue;

		pr_debug("Copying seg-%d: va %p, size 0x%zx to offset 0x%zx\n",
			 i, event_data->mem_seg[i].va, event_data->mem_seg[i].size,
			 offset);

		memcpy_fromio(dump + offset, event_data->mem_seg[i].va,
			      event_data->mem_seg[i].size);
		offset += event_data->mem_seg[i].size;
	}

	if (offset != event_data->total_size) {
		pr_warn("Copied size 0x%zx != expected 0x%x\n",
			offset, event_data->total_size);
	}

	segment->len = event_data->total_size;
	segment->vaddr = dump;
	segment->type = FW_CRASH_DUMP_REMOTE_MEM_DATA;

	pr_info("DDR dump collected: %s, size 0x%x bytes\n",
		event_data->file_name, segment->len);

#ifndef CONFIG_UPSTREAM_BUILD
	athdbg_base->dbg_to_ath_ops->coredump_dump_segment(ab, segment, segment->len);
#else
	/* dev_coredumpv() takes ownership of the buffer */
	dev_coredumpv(ab->dev, segment->vaddr, segment->len, GFP_KERNEL);
	vfree(segment);
	return 0;
#endif

	vfree(segment);
	vfree(dump);
	return 0;
}

static int athdbg_qmi_ddr_dump_upload_done_req_send_sync(struct ath12k_base *ab,
							 u32 status)
{
	struct qmi_wlanfw_ddr_dump_upload_done_req_msg_v01 *req;
	struct qmi_wlanfw_ddr_dump_upload_done_resp_msg_v01 *resp;
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

	req->status = status;

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_ddr_dump_upload_done_resp_msg_v01_ei, resp);
	if (ret < 0)
		goto out;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_DDR_DUMP_UPLOAD_DONE_REQ_V01,
			       WLANFW_DDR_DUMP_UPLOAD_DONE_REQ_MSG_V01_MAX_MSG_LEN,
			       qmi_wlanfw_ddr_dump_upload_done_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_warn("Failed to send QDSS Dump upload done request, err %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_warn("QDSS Dump upload done resp failed, result: %d, err: %d\n",
			resp->resp.result, resp->resp.error);
		ret = -EINVAL;
		goto out;
	}

out:
	kfree(req);
	kfree(resp);
	return ret;
}

static void athdbg_qmi_event_ddr_dump_region_req(struct athdbg_qmi *dbg_qmi, void *data)
{
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
	struct ath12k_qmi_event_ddr_dump_region *event_data = data;
	int i, status;

	status = athdbg_coredump_ddr_dump(ab, event_data);
	if (event_data->indication_type_valid && event_data->indication_type)
		athdbg_qmi_ddr_dump_upload_done_req_send_sync(ab, status);

	for (i = 0; i < event_data->mem_seg_len; i++) {
		if (event_data->mem_seg[i].valid && event_data->mem_seg[i].va)
			iounmap(event_data->mem_seg[i].va);
	}
}

static void athdbg_qmi_driver_event_work(struct work_struct *work)
{
	struct athdbg_qmi *dbg_qmi = container_of(work, struct athdbg_qmi,
					      event_work);
	struct athdbg_qmi_driver_event *event;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
	int ret;

	spin_lock(&dbg_qmi->event_lock);

	while (!list_empty(&dbg_qmi->event_list)) {
		event = list_first_entry(&dbg_qmi->event_list,
					 struct athdbg_qmi_driver_event, list);
		list_del(&event->list);
		spin_unlock(&dbg_qmi->event_lock);

		if (test_bit(ATH12K_FLAG_UNREGISTERING, &ab->dev_flags))
			goto skip;

		switch (event->type) {
		case ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_MEM:
			athdbg_qmi_event_qdss_trace_req_mem_hdlr(dbg_qmi);
			break;
		case ATHDBG_QMI_EVENT_QDSS_TRACE_SAVE:
			athdbg_qmi_event_qdss_trace_save_hdlr(dbg_qmi, event->data);
			break;
		case ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_DATA:
			ret = athdbg_qmi_event_qdss_trace_misc_hdlr(dbg_qmi, event->data);

			if (ret < 0)
				pr_err("failed to collect phy logs : %d\n", ret);

			break;
		case ATHDBG_QMI_EVENT_DDR_DUMP_REGION_REQ:
			athdbg_qmi_event_ddr_dump_region_req(dbg_qmi, event->data);
			break;
		default:
			pr_err("invalid event type: %d", event->type);
			break;
		}

skip:
		kfree(event->data);
		kfree(event);
		spin_lock(&dbg_qmi->event_lock);
	}
	spin_unlock(&dbg_qmi->event_lock);
}

void athdbg_qmi_event_qdss_trace_save_hdlr(struct athdbg_qmi *dbg_qmi,
						  void *data)
{
	struct athdbg_qmi_event_qdss_trace_save_data *event_data = data;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);

	if (!ab->dbg_qmi.qdss_mem_seg_len) {
		pr_err("Memory for QDSS trace is not available\n");
		return;
	}

	athdbg_coredump_qdss_dump(ab, event_data);
	athdbg_qmi_qdss_mem_free(ab);
}

int athdbg_qmi_event_qdss_trace_misc_hdlr(struct athdbg_qmi *dbg_qmi, void *data)
{
	struct qmi_wlfw_qdss_trace_data_req_msg_v01 *req;
	struct qmi_wlfw_qdss_trace_data_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct ath12k_qmi_event_qdss_trace_save_data *event_data = data;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
	u32 total_size = event_data->total_size;
	int ret = 0;
	u32 remaining;
	unsigned char *qdss_trace_data_temp, *qdss_trace_data = NULL;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	qdss_trace_data = kzalloc(total_size, GFP_KERNEL);

	if (!qdss_trace_data) {
		ret = -ENOMEM;
		goto out_free_req_resp;
	}

	remaining = total_size;
	qdss_trace_data_temp = qdss_trace_data;

	while (remaining) {
		if (resp->end_valid && resp->end)
			break;
		ret = qmi_txn_init(&ab->qmi.handle, &txn,
				   qmi_wlfw_qdss_trace_data_resp_msg_v01_ei, resp);
		if (ret < 0) {
			pr_err("Fail to initialize qmi txn err %d\n", ret);
			goto out_free_all;
		}

		ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
				      QMI_WLFW_QDSS_TRACE_DATA_REQ_V01,
				      QMI_WLFW_QDSS_TRACE_DATA_REQ_MSG_V01_MAX_MSG_LEN,
				      qmi_wlfw_qdss_trace_data_req_msg_v01_ei, req);
		if (ret < 0) {
			pr_err("qmi send request failed err %d\n", ret);
			qmi_txn_cancel(&txn);
			goto out_free_all;
		}

		ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));

		if (ret < 0) {
			pr_err("qmi failed to qdss data request, err = %d\n", ret);
			goto out_free_all;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			pr_err("Respond qdss data req failed, result: %d, err: %d\n",
				   resp->resp.result, resp->resp.error);
			ret = -EINVAL;
			goto out_free_all;
		}

		if (resp->total_size_valid == 1 && resp->total_size == total_size &&
		    resp->seg_id_valid == 1 && resp->seg_id == req->seg_id &&
		    resp->data_valid == 1 &&
		    resp->data_len <= QMI_WLANFW_MAX_DATA_SIZE_V01 &&
		    resp->data_len <= remaining) {
			memcpy(qdss_trace_data_temp, resp->data, resp->data_len);
		} else {
			pr_err("invalid qmi response\n");
			ret = -EINVAL;
			goto out_free_all;
		}

		remaining -= resp->data_len;
		qdss_trace_data_temp += resp->data_len;
		req->seg_id++;
	}

	if (!remaining && resp->end_valid && resp->end) {
		struct ath12k_dump_segment *segment;

		segment = vzalloc(sizeof(*segment));
		if (!segment) {
			ret = -ENOMEM;
			goto out_free_all;
		}
		segment->len = total_size;
		segment->vaddr = qdss_trace_data;
#ifdef CONFIG_UPSTREAM_BUILD
		dev_coredumpv(ab->dev, segment->vaddr, segment->len, GFP_KERNEL);
#else
		segment->type = FW_CRASH_DUMP_QDSS_DATA;
		athdbg_base->dbg_to_ath_ops->coredump_dump_segment(ab,
							segment, segment->len);
#endif
		vfree(segment);
	} else {
		pr_err("dump collection failed: remaining-%u response end-%u\n",
			   remaining, resp->end);
	}
out_free_all:
	kfree(qdss_trace_data);
out_free_req_resp:
	kfree(req);
	kfree(resp);
	return ret;
}

int athdbg_qmi_qdss_trace_mem_info_send_sync(void  *qmi_ab)
{
	struct qmi_wlanfw_respond_mem_req_msg_v01 *req;
	struct qmi_wlanfw_respond_mem_resp_msg_v01 resp = {};
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;
	struct qmi_txn txn;
	int ret, i;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->mem_seg_len = ab->dbg_qmi.qdss_mem_seg_len;

	for (i = 0; i < req->mem_seg_len ; i++) {
		req->mem_seg[i].addr = ab->dbg_qmi.qdss_mem[i].paddr;
		req->mem_seg[i].size = ab->dbg_qmi.qdss_mem[i].size;
		req->mem_seg[i].type = ab->dbg_qmi.qdss_mem[i].type;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_respond_mem_resp_msg_v01_ei, &resp);

	if (ret < 0) {
		pr_err("Fail to initialize txn for QDSS trace mem request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01,
			       QMI_WLANFW_RESPOND_MEM_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_respond_mem_req_msg_v01_ei, req);

	if (ret < 0) {
		pr_err("qmi failed to respond memory request, err = %d\n",
			    ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn,
			   msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		pr_err("qmi failed memory request, err = %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("Respond mem req failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	kfree(req);
	return ret;
}

int athdbg_send_qdss_trace_mode_req(void *qmi_ab,
				    enum qmi_wlanfw_qdss_trace_mode_enum_v01 mode,
				    u64 value)
{
	int ret;
	struct qmi_txn txn;
	struct qmi_wlanfw_qdss_trace_mode_req_msg_v01 req = {};
	struct qmi_wlanfw_qdss_trace_mode_resp_msg_v01 resp = {};
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;

	req.mode_valid = 1;
	req.mode = mode;
	req.option_valid = 1;
	if (!value) {
		req.option = mode == QMI_WLANFW_QDSS_TRACE_OFF_V01 ?
			QMI_WLANFW_QDSS_STOP_ALL_TRACE : 0;
	} else {
		req.option = value;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_qdss_trace_mode_resp_msg_v01_ei, &resp);
	if (ret < 0)
		return ret;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_QDSS_TRACE_MODE_REQ_V01,
			       QMI_WLANFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_qdss_trace_mode_req_msg_v01_ei, &req);

	if (ret < 0) {
		pr_err("Failed to send QDSS trace mode request,err = %d\n", ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("QDSS trace mode request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	return ret;
}

void athdbg_qmi_qdss_mem_free(struct ath12k_base *ab)
{
	int i;
	bool do_iounmap = false;
#if defined(CONFIG_UPSTREAM_BUILD) || defined(ATH12K_CMA_SUPPORT)
	struct target_mem_chunk *mem_chunk;
#endif

#if defined(CONFIG_UPSTREAM_BUILD)
	for (i = 0; i < ab->dbg_qmi.qdss_mem_seg_len; i++) {
		mem_chunk = &ab->dbg_qmi.qdss_mem[i];
		if (mem_chunk->v.ioaddr) {
			dma_free_coherent(ab->dev, mem_chunk->size,
					  mem_chunk->v.ioaddr,
					  mem_chunk->paddr);
			mem_chunk->v.ioaddr = NULL;
		}
	}
#elif defined(ATH12K_CMA_SUPPORT)
	if (ab->qmi_mem_dev.rmem_inited) {
		for (i = 0; i < ab->dbg_qmi.qdss_mem_seg_len; i++) {
			mem_chunk = &ab->dbg_qmi.qdss_mem[i];
			if (mem_chunk->v.ioaddr) {
				dma_free_coherent(&ab->qmi_mem_dev.dev, mem_chunk->size,
						  mem_chunk->v.ioaddr,
						  mem_chunk->paddr);
				mem_chunk->v.ioaddr = NULL;
			}
		}
	} else {
		do_iounmap = true;
	}
#else
#ifdef PLATFORM_SDX
	if (ab->dbg_qmi.qdss_mem_seg_len && ab->dbg_qmi.qdss_mem[0].v.ioaddr) {
		dma_free_attrs(ab->dev,
			       ab->dbg_qmi.qdss_mem[0].size,
			       ab->dbg_qmi.qdss_mem[0].v.ioaddr,
			       ab->dbg_qmi.qdss_mem[0].paddr,
			       DMA_ATTR_FORCE_CONTIGUOUS);

		ab->dbg_qmi.qdss_mem[0].v.ioaddr = NULL;
		ab->dbg_qmi.qdss_mem[0].size = 0;
		ab->dbg_qmi.qdss_mem[0].paddr = 0;
	}
#else
	do_iounmap = true;
#endif
#endif

	if (do_iounmap) {
		for (i = 0; i < ab->dbg_qmi.qdss_mem_seg_len; i++) {
			if (ab->dbg_qmi.qdss_mem[i].v.ioaddr) {
				iounmap(ab->dbg_qmi.qdss_mem[i].v.ioaddr);
				ab->dbg_qmi.qdss_mem[i].v.ioaddr = NULL;
				ab->dbg_qmi.qdss_mem[i].size = 0;
				ab->dbg_qmi.qdss_mem[i].paddr = 0;
			}
		}
	}

	ab->dbg_qmi.qdss_mem_seg_len = 0;
	ab->is_qdss_tracing = false;
}
EXPORT_SYMBOL(athdbg_qmi_qdss_mem_free);

int athdbg_qmi_send_qdss_trace_config_download_req(void *qmi_ab,
								const u8 *buffer,
								unsigned int buffer_len)
{
	int ret = 0;
	struct qmi_wlanfw_qdss_trace_config_download_req_msg_v01 *req;
	struct qmi_wlanfw_qdss_trace_config_download_resp_msg_v01 resp;
	struct qmi_txn txn;
	const u8 *temp = buffer;
	int  max_len = QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_LEN;
	unsigned int  remaining;
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	remaining = buffer_len;
	while (remaining) {
		memset(&resp, 0, sizeof(resp));
		req->total_size_valid = 1;
		req->total_size = buffer_len;
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
				   qmi_wlanfw_qdss_trace_config_download_resp_msg_v01_ei,
				   &resp);
		if (ret < 0)
			goto out;

		ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
				QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01,
				max_len,
				qmi_wlfw_qdss_trace_config_download_req_msg_v01_ei,
				req);
		if (ret < 0) {
			pr_err("Failed to send QDSS config download request = %d\n",
					ret);
			qmi_txn_cancel(&txn);
			goto out;
		}

		ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
		if (ret < 0)
			goto out;

		if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
			pr_err("QDSS config download request failed, res: %d,err: %d\n",
					resp.resp.result, resp.resp.error);
			ret = -EINVAL;
			goto out;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
		}

out:
		kfree(req);
		return ret;
}

int athdbg_qmi_driver_event_post(struct ath12k_qmi *qmi, enum athdbg_qmi_event_type type,
				 void *data)
{

	struct athdbg_qmi_driver_event *event;
	struct ath12k_base *ab = (struct ath12k_base *)qmi->ab;
	struct athdbg_qmi *dbg_qmi = &ab->dbg_qmi;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);
	if (!event)
		return -ENOMEM;

	event->type = type;
	event->data = data;

	spin_lock(&dbg_qmi->event_lock);
	list_add_tail(&event->list, &dbg_qmi->event_list);
	spin_unlock(&dbg_qmi->event_lock);

	queue_work(dbg_qmi->event_wq, &dbg_qmi->event_work);

	return 0;
}

void athdbg_qmi_wlanfw_ddr_dump_region_ind_cb(struct qmi_handle *qmi_hdl,
					      struct sockaddr_qrtr *sq,
					      struct qmi_txn *txn,
					      const void *data)
{
	struct ath12k_qmi *qmi = container_of(qmi_hdl, struct ath12k_qmi, handle);
	const struct wlanfw_ddr_dump_region_ind_msg_v01 *ind_msg = data;
	struct athdbg_qmi_event_qdss_trace_save_data qdss_data = {0};
	struct ath12k_qmi_event_ddr_dump_region *event_data;
	struct target_mem_chunk *fw_mem = NULL;
	struct ath12k_fw_mem *mem_seg = NULL;
	struct ath12k_base *ab = qmi->ab;
	int status = DUMP_UPLOAD_FAILED;
	int i, j, qdss_seg_count = 0;
	uintptr_t offset = 0;

	if (!txn || !ind_msg) {
		pr_err("Failed with invalid indication received\n");
		return;
	}

	if (!ind_msg->mem_seg_len) {
		pr_err("Failed with number of DDR dump segment is zero\n");
		goto end;
	}

	if (ind_msg->mem_seg_len > ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01) {
		pr_err("Failed DDR Dump region count %u exceeds max %u\n",
		       ind_msg->mem_seg_len, ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01);
		goto end;
	}

	for (i = 0; i < ind_msg->mem_seg_len; i++) {
		if (ind_msg->mem_seg[i].type == QMI_WLANFW_MEM_QDSS_V01)
			qdss_seg_count++;
	}

	if (qdss_seg_count && qdss_seg_count == ind_msg->mem_seg_len) {
		pr_info("Received QMI WLFW QDSS Dump indication\n");
		qdss_data.mem_seg_len = ind_msg->mem_seg_len;
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			qdss_data.total_size += ind_msg->mem_seg[i].size;
			qdss_data.mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			qdss_data.mem_seg[i].size = ind_msg->mem_seg[i].size;
		}
		athdbg_coredump_qdss_dump(ab, &qdss_data);
		status = DUMP_UPLOAD_SUCCESS;
		goto end;
	}

	pr_info("Received QMI WLFW DDR Dump region indication\n");
	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		goto end;

	if (ind_msg->file_name_valid)
		strscpy(event_data->file_name, ind_msg->file_name,
			QMI_WLANFW_MAX_STR_LEN_V01 + 1);
	else
		strscpy(event_data->file_name, "ddr_dump_region",
			QMI_WLANFW_MAX_STR_LEN_V01 + 1);

	if (ind_msg->indication_type_valid && ind_msg->indication_type) {
		event_data->indication_type_valid = 1;
		event_data->indication_type = 1;
	}

	pr_info("DDR Dump region filename: %s\n", event_data->file_name);

	fw_mem = ab->qmi.target_mem;
	for (i = 0, j = 0; i < ab->qmi.mem_seg_count && j < ind_msg->mem_seg_len; i++) {
		if (ind_msg->mem_seg[j].type != fw_mem[i].type)
			continue;

		mem_seg = &event_data->mem_seg[j];

		if (ind_msg->mem_seg[j].addr < fw_mem[i].paddr ||
		    (ind_msg->mem_seg[j].addr + ind_msg->mem_seg[j].size) >
		    (fw_mem[i].paddr + fw_mem[i].size)) {
			pr_err("seg-%d: addr 0x%llx size 0x%x outside allocated region (0x%llx-0x%llx)\n",
			       j, ind_msg->mem_seg[j].addr, ind_msg->mem_seg[j].size,
			       (u64)fw_mem[i].paddr,
			       (u64)(fw_mem[i].paddr + fw_mem[i].size));
			goto cleanup_and_free;
		}

		event_data->total_size += ind_msg->mem_seg[j].size;
		mem_seg->pa = (phys_addr_t)ind_msg->mem_seg[j].addr;
		mem_seg->size = ind_msg->mem_seg[j].size;
		mem_seg->type = ind_msg->mem_seg[j].type;

		if (!fw_mem[i].v.addr) {
			mem_seg->va = ioremap(mem_seg->pa, mem_seg->size);
			if (!mem_seg->va) {
				pr_err("seg-%d: ioremap failed for addr 0x%pa size 0x%zx\n",
				       j, &mem_seg->pa, mem_seg->size);
				goto cleanup_and_free;
			}
			mem_seg->valid = true;
		} else {
			offset = mem_seg->pa - fw_mem[i].paddr;
			mem_seg->va = (void *)((char *)fw_mem[i].v.addr + offset);
			mem_seg->valid = false;
		}

		pr_debug("seg-%d: va 0x%pK, pa 0x%pa, size 0x%zx, type %u\n",
			 j, mem_seg->va, &mem_seg->pa, mem_seg->size, mem_seg->type);
		j++;
		if (j == ind_msg->mem_seg_len)
			break;
	}

	event_data->mem_seg_len = j;

	if (!j) {
		pr_err("Failed with no valid segments found matching allocated DDR regions\n");
		goto cleanup_and_free;
	}

	athdbg_qmi_driver_event_post(qmi, ATHDBG_QMI_EVENT_DDR_DUMP_REGION_REQ,
				     event_data);
	return;

cleanup_and_free:
	for (i = 0; i < j; i++) {
		if (event_data->mem_seg[i].valid && event_data->mem_seg[i].va)
			iounmap(event_data->mem_seg[i].va);
	}
	kfree(event_data);

end:
	if (ind_msg->indication_type_valid && ind_msg->indication_type)
		athdbg_qmi_ddr_dump_upload_done_req_send_sync(ab, status);
}

int athdbg_qmi_worker_init(void *qmi_ab)
{
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;

	if (ab->dbg_qmi.event_wq != NULL)
		return 0;

	ab->dbg_qmi.event_wq = alloc_ordered_workqueue("athdbg_qmi_driver_event", 0);
	if (!ab->dbg_qmi.event_wq) {
		pr_err("failed to allocate workqueue\n");
		return -EFAULT;
	}

	INIT_LIST_HEAD(&ab->dbg_qmi.event_list);
	spin_lock_init(&ab->dbg_qmi.event_lock);
	INIT_WORK(&ab->dbg_qmi.event_work, athdbg_qmi_driver_event_work);

	return 0;
}
EXPORT_SYMBOL(athdbg_qmi_worker_init);

void athdbg_qmi_deinit(struct ath12k_base *ab)
{
	int i;
	bool remaining_handler = false;
	struct qmi_msg_handler *ab_handlers =
			(struct qmi_msg_handler *)ab->qmi.handle.handlers;

	if (ab->dbg_qmi.event_wq) {
		cancel_work_sync(&ab->dbg_qmi.event_work);
		destroy_workqueue(ab->dbg_qmi.event_wq);
	}

	if (!ab_handlers)
		return;

	for (i = 0; i < athdbg_base->wdbg_handlers_cnt; i++) {
		if (athdbg_base->wdbg_handlers[i] != ab_handlers) {
			if (athdbg_base->wdbg_handlers[i] != NULL)
				remaining_handler = true;
			continue;
		}

		kfree(athdbg_base->wdbg_handlers[i]);
		athdbg_base->wdbg_handlers[i] = NULL;
	}

	if (!remaining_handler)
		athdbg_base->wdbg_handlers_cnt = 0;
}
EXPORT_SYMBOL(athdbg_qmi_deinit);
