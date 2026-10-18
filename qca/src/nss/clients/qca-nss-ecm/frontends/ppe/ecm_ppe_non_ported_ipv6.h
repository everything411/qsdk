/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern void ecm_ppe_non_ported_ipv6_init(struct dentry *dentry);

extern struct ecm_front_end_connection_instance *ecm_ppe_non_ported_ipv6_connection_instance_alloc(
								uint32_t accel_flags,
								int protocol,
								struct ecm_db_connection_instance **nci);
extern void ecm_ppe_non_ported_ipv6_connection_set(struct ecm_front_end_connection_instance *feci, uint32_t flags);
