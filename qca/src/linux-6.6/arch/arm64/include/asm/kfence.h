/* SPDX-License-Identifier: GPL-2.0 */
/*
 * arm64 KFENCE support.
 *
 * Copyright (C) 2020, Google LLC.
 */

#ifndef __ASM_KFENCE_H
#define __ASM_KFENCE_H

#include <asm/set_memory.h>

static inline bool arch_kfence_init_pool(void) { return true; }

static inline bool kfence_protect_page(unsigned long addr, bool protect)
{
	set_memory_valid(addr, 1, !protect);

	return true;
}

#ifdef CONFIG_KFENCE
extern bool kfence_early_init;
static inline bool arm64_kfence_can_set_direct_map(void)
{
	/*
	 * To avoid NO_{BLOCK|FLAT}_MAPPINGS when kfence is not enabled
	 * during bootup time.
	 */
	return false;
}
#else /* CONFIG_KFENCE */
static inline bool arm64_kfence_can_set_direct_map(void) { return false; }
#endif /* CONFIG_KFENCE */

#endif /* __ASM_KFENCE_H */
