/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_TUN_RPS_API_H__
#define __NSS_PPENL_TUN_RPS_API_H__

void nss_ppenl_tun_rps_init_rule(struct nss_ppenl_tun_rps_rule *rule, enum nss_ppe_tun_rps_message_types type);
int nss_ppenl_tun_rps_rule_add(struct nss_ppenl_tun_rps_rule *rule);
int nss_ppenl_tun_rps_rule_del(struct nss_ppenl_tun_rps_rule *rule);

#endif /* __NSS_PPENL_TUN_RPS_API_H__ */
