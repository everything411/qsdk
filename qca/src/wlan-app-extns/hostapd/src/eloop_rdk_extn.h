/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ELOOP_RDK_EXTN_H
#define ELOOP_RDK_EXTN_H

#ifdef RDK_ONEWIFI

int eloop_sock_table_read_set_fds(fd_set *fds);
int eloop_sock_table_read_get_biggest_fd(void);
void eloop_sock_table_read_dispatch(fd_set *fds);
int eloop_get_timeout_ms(void);
int eloop_timeout_run(void);

#endif /* RDK_ONEWIFI */

#endif /* ELOOP_RDK_EXTN_H */
