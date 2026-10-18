/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_EDMA_DDRQ_API_H__
#define __NSS_PPENL_EDMA_DDRQ_API_H__

void nss_ppenl_edma_ddrq_init_rule(struct nss_ppenl_edma_ddrq_rule *rule, enum nss_ppe_edma_ddrq_message_types type);
int nss_ppenl_edma_ddrq_send_rule(struct nss_ppenl_edma_ddrq_rule *rule);

#endif /* __NSS_PPENL_EDMA_DDRQ_API_H__ */
