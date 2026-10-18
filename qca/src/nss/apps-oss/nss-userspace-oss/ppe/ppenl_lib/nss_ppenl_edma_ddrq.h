/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_EDMA_DDRQ_H__
#define __NSS_PPENL_EDMA_DDRQ_H__

/** @addtogroup chapter_nlEDMADDRQ
 This chapter describes EDMA DDRQ APIs in the user space.
 These APIs are wrapper functions for EDMA DDRQ family specific operations.
*/

/**
 * Response callback for EDMA DDRQ
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] req EDMA DDRQ req.
 * @param[in] resp_ctx User data per callback.
 *
 * @return
 * None
 */
typedef void (*nss_ppenl_edma_ddrq_resp_cb_t)(void *user_ctx, struct nss_ppenl_edma_ddrq_rule *rule, void *resp_ctx);

/**
 * Event callback for EDMA DDRQ
 *
 * @param[in] user_ctx User context (provided at socket open).
 * @param[in] rule EDMA DDRQ rule.
 *
 * @return
 * None
 */
typedef void (*nss_ppenl_edma_ddrq_event_cb_t)(void *user_ctx, struct nss_ppenl_edma_ddrq_rule *rule);

/**
 * NSS NL EDMA DDRQ response.
 */
struct nss_ppenl_edma_ddrq_resp {
	void *data;		/**< Response context. */
	nss_ppenl_edma_ddrq_resp_cb_t cb;		/**< Response callback. */
};

/**
 * NSS NL EDMA DDRQ context.
 */
struct nss_ppenl_edma_ddrq_ctx {
	struct nss_ppenl_sock_ctx sock;		/**< NSS socket context. */
	nss_ppenl_edma_ddrq_event_cb_t event;	/**< NSS event callback function. */
};


/** @} *//* end_addtogroup nss_ppenl_edma_ddrq functions */

#endif /* __NSS_PPENL_EDMA_DDRQ_H__ */
