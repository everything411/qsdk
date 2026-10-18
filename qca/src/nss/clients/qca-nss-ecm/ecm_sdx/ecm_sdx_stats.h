/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

void ecm_sdx_stats_update_all_host_stats(void);
void ecm_sdx_stats_update_host_stats(ip_addr_t host_ip, uint64_t host_rx,
			uint64_t host_tx, bool host_removed);
void ecm_sdx_stats_update_per_ip_stats(void);

int ecm_sdx_stats_handler_write(void *buffer, size_t *lenp);
int ecm_sdx_stats_handler_read(void *buffer, size_t *lenp, loff_t *ppos);

int ecm_sdx_stats_per_client_handler_write(void *buffer, size_t *lenp);
int ecm_sdx_stats_per_client_handler_read(void *buffer, size_t *lenp, loff_t *ppos);

int ecm_sdx_stats_interface_type_handler_read(void *buffer, size_t *lenp, loff_t *ppos);
int ecm_sdx_stats_interface_type_handler_write(void *buffer, size_t *lenp);

int ecm_sdx_stats_lan_prefix_handler_write(void *buffer, size_t *lenp);
int ecm_sdx_stats_lan_prefix_handler_read(void *buffer, size_t *lenp, loff_t *ppos);

int ecm_sdx_stats_init(struct dentry *dentry);
void ecm_sdx_stats_exit(void);

int ecm_sdx_init(struct dentry *dentry);
void ecm_sdx_exit(void);
