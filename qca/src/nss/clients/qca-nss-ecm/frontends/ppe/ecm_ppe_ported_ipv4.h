/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern void ecm_ppe_ported_ipv4_init(struct dentry *dentry);

extern struct ecm_front_end_connection_instance *ecm_ppe_ported_ipv4_connection_instance_alloc(
								uint32_t accel_flags,
								int protocol,
								struct ecm_db_connection_instance **nci);
extern void ecm_ppe_ported_ipv4_connection_set(struct ecm_front_end_connection_instance *feci, uint32_t flags);

bool ecm_ppe_ported_ipv4_unidir_rule_update(
		struct ecm_db_connection_instance *ci, struct ecm_classifier_process_response *pr,
		ecm_tracker_sender_type_t sender, uint8_t rule_type);

bool ecm_ppe_ported_ipv4_bidir_sawf_rule_update(
		struct ecm_db_connection_instance *ci, struct ecm_front_end_flowsawf_msg *msg,
		uint8_t rule_type);