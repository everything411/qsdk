/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_edma_ddrq.h
 *	NSS Netlink EDMA DDRQ definitions
 */
#ifndef __NSS_PPENL_EDMA_DDRQ_H
#define __NSS_PPENL_EDMA_DDRQ_H

bool nss_ppenl_edma_ddrq_init(void);
bool nss_ppenl_edma_ddrq_exit(void);

#if defined(CONFIG_NSS_PPENL_EDMA_DDRQ)
#define NSS_PPENL_EDMA_DDRQ_INIT nss_ppenl_edma_ddrq_init
#define NSS_PPENL_EDMA_DDRQ_EXIT nss_ppenl_edma_ddrq_exit
#else
#define NSS_PPENL_EDMA_DDRQ_INIT 0
#define NSS_PPENL_EDMA_DDRQ_EXIT 0
#endif	/* CONFIG_NSS_PPENL_EDMA_DDRQ */

#endif /* __NSS_PPENL_EDMA_DDRQ_H */
