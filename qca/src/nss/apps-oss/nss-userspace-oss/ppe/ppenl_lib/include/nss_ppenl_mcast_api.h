/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_MCAST_API_H__
#define __NSS_PPENL_MCAST_API_H__

/** @addtogroup chapter_nlmulticast
 This chapter describes multicast APIs in the user space.
 These APIs are wrapper functions for multicast family-specific operations.
*/

/** @addtogroup nss_ppenl_mcast_datatypes @{ */

/**
 * Response callback for multicast.
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req multicast req.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None.
 */
typedef void (*nss_ppenl_mcast_resp_cb_t)(void *user_ctx, struct nss_ppenl_mcast_req *req, void *resp_ctx);

void nss_ppenl_mcast_init_req(struct nss_ppenl_mcast_req *req, enum nss_ppe_mcast_message_types type);
int nss_ppenl_mcast_send_req(struct nss_ppenl_mcast_req *req);

#endif /* __NSS_PPENL_MCAST_API_H__ */
