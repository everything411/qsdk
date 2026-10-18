/*
 **************************************************************************
 * Copyright (c) 2018, The Linux Foundation.  All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

struct ecm_classifier_mark_instance;
struct ecm_classifier_mark_instance *ecm_classifier_mark_instance_alloc(struct ecm_db_connection_instance *ci);
int ecm_classifier_mark_init(struct dentry *dentry);
void ecm_classifier_mark_exit(void);
