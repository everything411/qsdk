/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_GEMPORT_API_H__
#define __NSS_PPENL_GEMPORT_API_H__

void nss_ppenl_gem_port_init_rule(struct nss_ppenl_gem_port_rule *rule, enum nss_ppe_gem_port_message_types type);
int nss_ppenl_gem_port_rule_send(struct nss_ppenl_gem_port_rule *rule);

#endif /* __NSS_PPENL_GEMPORT_API_H__ */
