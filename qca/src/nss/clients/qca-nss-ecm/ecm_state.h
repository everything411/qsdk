/*
 **************************************************************************
 * Copyright (c) 2015, 2021, The Linux Foundation.  All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

struct ecm_state_file_instance;

int ecm_state_write_reset(struct ecm_state_file_instance *sfi, char *prefix);

int ecm_state_prefix_add(struct ecm_state_file_instance *sfi, char *prefix);
int ecm_state_prefix_index_add(struct ecm_state_file_instance *sfi, uint32_t index);
int ecm_state_prefix_remove(struct ecm_state_file_instance *sfi);

int ecm_state_write(struct ecm_state_file_instance *sfi, char *name, char *fmt, ...);

int ecm_state_init(struct dentry *dentry);
void ecm_state_exit(void);