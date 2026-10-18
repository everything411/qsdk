/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_PM_API_H__
#define __NSS_PPENL_PM_API_H__

void nss_ppenl_pm_counter_init(struct nss_ppenl_pm_info *pm_info, enum nss_ppe_pm_message_types type);
int nss_ppenl_pm_counter_get(struct nss_ppenl_pm_info *pm_info);
int nss_ppenl_pm_counter_gen_rule_add(struct nss_ppenl_pm_info *pm_info);
int nss_ppenl_pm_counter_gen_rule_del(struct nss_ppenl_pm_info *pm_info);

#endif /* __NSS_PPENL_PM_API_H__ */
