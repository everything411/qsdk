/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * kprobe support for ath12k
 */

#ifndef _ATH12K_KPROBE_H_
#define _ATH12K_KPROBE_H_

#ifdef CONFIG_KPROBES

/**
 * ath12k_kprobe_init() - initialize kprobe debugfs entry
 *
 * Creates /sys/kernel/debug/ath12k/kprobe.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_kprobe_init(void);

/**
 * ath12k_kprobe_exit() - clean up all kprobes and the debugfs entry
 *
 * Unregisters all active kprobes and removes the debugfs entry.
 * Called during module unload.
 */
void ath12k_kprobe_exit(void);

#else /* !CONFIG_KPROBES */

static inline int ath12k_kprobe_init(void) { return 0; }
static inline void ath12k_kprobe_exit(void) {}

#endif /* CONFIG_KPROBES */

#endif /* _ATH12K_KPROBE_H_ */
