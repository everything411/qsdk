/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * TODO: Enable a true synchronous API and remove callback registration
 */
#define PPECFG_QOS_RET_SUCCESS			0	/* Successful operation return value from ppe */
#define PPECFG_QOS_RET_CLASS_NON_LEAF		1	/* Non Leaf class return value from ppe */
#define PPECFG_QOS_RET_INVALID_CLASS		2	/* Invalid class return value from ppe */
#define PPECFG_QOS_RET_INVALID_DEV		3	/* Invalid physical interface return value from ppe */
#define PPECFG_QOS_RET_NO_QDISC_CONFIGURED	4	/* PPE QDISC not configured */

void nss_ppenl_qos_init_req(struct nss_ppenl_qos_req *req, enum nss_ppe_qos_message_types type);
int nss_ppenl_qos_get_int_pri(struct nss_ppenl_qos_req *req);

#endif /* __NSS_PPENL_QOS_API_H__ */
