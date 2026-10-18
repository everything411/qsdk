/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_QOS_API_H__
#define __NSS_PPENL_QOS_API_H__

/** @addtogroup chapter_nlQOS
 This chapter describes QOS APIs in the user space.
 These APIs are wrapper functions for QOS family-specific operations.
*/

/** @addtogroup nss_ppenl_qos_datatypes @{ */

/**
 * Response callback for QOS.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req QOS req.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_qos_resp_cb_t)(void *user_ctx, struct nss_ppenl_qos_req *req, void *resp_ctx);

void nss_ppenl_qos_init_req(struct nss_ppenl_qos_req *req, enum nss_ppe_qos_message_types type);
int nss_ppenl_qos_send_req(struct nss_ppenl_qos_req *req);

#endif /* __NSS_PPENL_QOS_API_H__ */
