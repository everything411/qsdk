/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_COS_MAP_API_H__
#define __NSS_PPENL_COS_MAP_API_H__

void nss_ppenl_cos_map_init_config(struct nss_ppenl_cos_map_config *config, enum nss_ppe_cos_map_message_types type);
int nss_ppenl_cos_map_send_config(struct nss_ppenl_cos_map_config *config);

#endif /* __NSS_PPENL_COS_MAP_API_H__ */
