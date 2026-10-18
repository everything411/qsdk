/*
 **************************************************************************
 * Copyright (c) 2015, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern struct ecm_front_end_connection_instance *ecm_nss_multicast_ipv4_connection_instance_alloc(
								bool can_accel,
								struct ecm_db_connection_instance **nci);

extern int ecm_nss_multicast_ipv4_init(struct dentry *dentry);

extern void ecm_nss_multicast_ipv4_exit(void);
