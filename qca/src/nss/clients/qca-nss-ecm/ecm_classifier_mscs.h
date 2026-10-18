/*
 **************************************************************************
 * Copyright (c) 2020, The Linux Foundation.  All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

struct ecm_classifier_mscs_instance;
struct ecm_classifier_mscs_instance *ecm_classifier_mscs_instance_alloc(struct ecm_db_connection_instance *ci);
int ecm_classifier_mscs_init(struct dentry *dentry);
void ecm_classifier_mscs_exit(void);
