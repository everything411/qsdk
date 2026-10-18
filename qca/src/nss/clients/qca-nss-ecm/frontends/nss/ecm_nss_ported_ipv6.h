/*
 **************************************************************************
 * Copyright (c) 2015, 2018, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern void ecm_nss_ported_ipv6_init(struct dentry *dentry);

extern struct ecm_front_end_connection_instance *ecm_nss_ported_ipv6_connection_instance_alloc(
								uint32_t accel_flags,
								int protocol,
								struct ecm_db_connection_instance **nci);
