/*
 **************************************************************************
 * Copyright (c) 2015, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern void ecm_sfe_ported_ipv6_init(struct dentry *dentry);

extern struct ecm_front_end_connection_instance *ecm_sfe_ported_ipv6_connection_instance_alloc(
								uint32_t accel_flags,
								int protocol,
								struct ecm_db_connection_instance **nci);
extern void ecm_sfe_ported_ipv6_connection_set(struct ecm_front_end_connection_instance *feci, uint32_t flags);
bool ecm_sfe_ported_ipv6_unidir_rule_update(struct ecm_db_connection_instance *ci,
		struct ecm_classifier_process_response *pr, ecm_tracker_sender_type_t sender, struct sfe_ipv6_msg *msg);
