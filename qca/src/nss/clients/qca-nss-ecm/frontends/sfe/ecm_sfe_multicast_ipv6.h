/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern struct ecm_front_end_connection_instance *ecm_sfe_multicast_ipv6_connection_instance_alloc(
								bool can_accel,
								struct ecm_db_connection_instance **nci);

extern int ecm_sfe_multicast_ipv6_init(struct dentry *dentry);

extern void ecm_sfe_multicast_ipv6_exit(void);
