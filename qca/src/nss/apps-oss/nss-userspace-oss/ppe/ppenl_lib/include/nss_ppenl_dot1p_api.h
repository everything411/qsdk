/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_DOT1P_API_H__
#define __NSS_PPENL_DOT1P_API_H__

void nss_ppenl_dot1p_init_rule(struct nss_ppenl_dot1p_rule *rule, enum nss_ppe_dot1p_message_types type);
int nss_ppenl_dot1p_rule_send(struct nss_ppenl_dot1p_rule *rule);

#endif /* __NSS_PPENL_DOT1P_API_H__ */
