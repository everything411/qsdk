/*
 **************************************************************************
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

struct ecm_classifier_dscp_instance;
struct ecm_classifier_dscp_instance *ecm_classifier_dscp_instance_alloc(struct ecm_db_connection_instance *ci);
void ecm_classifier_dscp_update(struct ecm_classifier_instance *aci, enum ecm_rule_update_type type, void *arg);
int ecm_classifier_dscp_init(struct dentry *dentry);
void ecm_classifier_dscp_exit(void);
