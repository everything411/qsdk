/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2013 David Feng <fenghua@phytium.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm-offsets.h>
#include <config.h>
#include <linux/linkage.h>
#include <asm/macro.h>
#include <asm/armv8/mmu.h>

.globl global_core_array
global_core_array:
        .octa 0

ENTRY(secondary_cpu_init)

.macro	set_vbar, regname, reg
.endm
	/*
	 * Could be EL3/EL2/EL1, Initial State:
	 * Little Endian, MMU Disabled, i/dCache Disabled
	 */
	switch_el x1, 3f, 2f, 1f
3:	set_vbar vbar_el3, x0
	mrs	x0, scr_el3
	orr	x0, x0, #0xf			/* SCR_EL3.NS|IRQ|FIQ|EA */
	msr	scr_el3, x0
	msr	cptr_el3, xzr			/* Enable FP/SIMD */
	b	0f
2:	mrs	x1, hcr_el2
	tbnz	x1, #HCR_EL2_E2H_BIT, 1f	/* HCR_EL2.E2H */
	orr	x1, x1, #HCR_EL2_AMO_EL2	/* Route SErrors to EL2 */
	msr	hcr_el2, x1
	set_vbar vbar_el2, x0
	mov	x0, #0x33ff
	msr	cptr_el2, x0			/* Enable FP/SIMD */
	b	0f
1:	set_vbar vbar_el1, x0
	mov	x0, #3 << 20
	msr	cpacr_el1, x0			/* Enable FP/SIMD */
0:
	msr	daifclr, #0x4			/* Unmask SError interrupts */

	branch_if_not_highest_el x0, 4f
	ldr	x0, =CONFIG_COUNTER_FREQUENCY
	msr	cntfrq_el0, x0			/* Initialize CNTFRQ */

4:	isb

	/* Apply ARM core specific erratas */
	bl	apply_core_errata

	/*
	 * Cache/BPB/TLB Invalidate
	 * i-cache is invalidated before enabled in icache_enable()
	 * tlb is invalidated before mmu is enabled in dcache_enable()
	 * d-cache is invalidated before enabled in dcache_enable()
	 */

	mrs     x0, MPIDR_EL1
#if defined (CONFIG_BASE_CPU_64BIT_BOOTUP)
	lsr     x0, x0, #8
#endif
	and     x1, x0, #0xff
	sub	x0, x1, #1

	/* Now x1 has the cpu number,
	 *  this will serve as index to "struct cpu_entry_arg"
         * struct cpu_entry_arg {
	 * void *stack_ptr;		0x00 0x38 0x70	+0
	 * volatile void *gd_ptr;	0x08 0x40 0x78	+8
	 * void *arg_ptr;		0x10 0x48 0x80	+8
	 * int64_t cpu_up;		0x18 0x50 0x88	+8
	 * int64_t cmd_complete;	0x20 0x58 0x90	+8
	 * int64_t cmd_result;		0x28 0x60 0x98	+8
	 * void *stack_top_ptr;		0x30 0x68 0xA0	+8
	 *				+0x38 (next entry in core[])
	 * };
         */

	ldr x4, =global_core_array
	mov x3, #0x38
	mul x2, x0, x3
	ldr x0, [x4]
	add x0, x0, x2

	/* x0 has the arg structure pointer
		struct cpu_entry_arg {
		        void *stack_ptr;
			void *gd_ptr;
			void *arg_ptr;
			int64_t  cpu_up;
			int64_t cmd_complete;
			int64_t cmd_result;
		};
	*/

	ldr x1, [x0]
	mov sp, x1

	/* gd address is always in x18, but if u-boot changes Tomorrow,
	 * we have to change here. Note :- --> ARM64: use x18 for gd
	*/
	ldr x18, [x0, #0x8]
	mov x3, #1
	str x3, [x0, #0x18] /* cpu_up */
	add x1, x0, #0x20   /* cmd_complete */
	add x2, x0, #0x28   /* cmd_result */
	ldr x0, [x0, #0x10] /* arg_ptr */

	bl secondary_core_entry
self_loop:
	b self_loop
ENDPROC(secondary_cpu_init)
