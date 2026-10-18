/*
 **************************************************************************
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern void ecm_sfe_non_ported_ipv4_init(struct dentry *dentry);

extern struct ecm_front_end_connection_instance *ecm_sfe_non_ported_ipv4_connection_instance_alloc(
								uint32_t flags, int protocol,
								struct ecm_db_connection_instance **nci);
extern void ecm_sfe_non_ported_ipv4_sit_set_peer(struct ecm_front_end_connection_instance *feci, struct sk_buff *skb);
extern void ecm_sfe_non_ported_ipv4_connection_set(struct ecm_front_end_connection_instance *feci, uint32_t flags);
