/*
 ***************************************************************************
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 ***************************************************************************
 */

#include "ecm_classifier_wifi_public.h"

#define ECM_CLASSIFIER_WIFI_INVALID_DS_NODE_ID		0xFF
#define ECM_CLASSIFIER_WIFI_INVALID_AST_INFO		0xFFFF
#define ECM_CLASSIFIER_WIFI_INVALID_PEER_ID		0xFFFF
#define ECM_CLASSIFIER_WIFI_INVALID_STREAM_ID		0xFF
#define ECM_CLASSIFIER_WIFI_INVALID_HLOS_TID_OVERRIDE	0

struct ecm_classifier_wifi_instance;

/*
 * ecm_classifier_wifi_instance_alloc()
 *	Allocate an instance of the wifi classifier
 */
struct ecm_classifier_wifi_instance *ecm_classifier_wifi_instance_alloc(struct ecm_db_connection_instance *ci);

int ecm_classifier_wifi_init(struct dentry *dentry);
void ecm_classifier_wifi_exit(void);
