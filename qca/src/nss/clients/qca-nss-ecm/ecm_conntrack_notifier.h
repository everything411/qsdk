/*
 **************************************************************************
 * Copyright (c) 2015, 2016, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

extern int ecm_conntrack_ipv6_event(unsigned long events, struct nf_conn *ct);
extern int ecm_conntrack_ipv4_event(unsigned long events, struct nf_conn *ct);
extern void ecm_conntrack_notifier_stop(int num);
extern int ecm_conntrack_notifier_init(struct dentry *dentry);
extern void ecm_conntrack_notifier_exit(void);
