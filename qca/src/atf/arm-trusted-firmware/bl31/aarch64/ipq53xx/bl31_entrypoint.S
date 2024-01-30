/*
 * Copyright (c) 2013-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <platform_def.h>

#include <arch.h>
#include <common/bl_common.h>
#include <el3_common_macros.S>
#include <lib/pmf/pmf_asm_macros.S>
#include <lib/runtime_instr.h>
#include <lib/xlat_tables/xlat_mmu_helpers.h>

#define APCS_MPU_AXI_CH_XPU_CR 0xB1D2080
#define APCS_MPU_AXI_CH_XPU_SCR 0xB1D2000
#define APCS_MPU_AXI_CH_XPU_MCR 0xB1D2100
#define APCS_MPU_AXI_CH_XPU_SESRRESTORE 0xB1D204C
#define APCS_MPU_AXI_CH_XPU_ESRRESTORE 0xB1D20CC
#define APCS_MPU_AXI_CH_XPU_MESRRESTORE 0xB1D214C

#define XBL_SEC_IMG_BASE 0x08004000
#define XBL_SEC_IMG_SIZE 0x20000 /* 128KB */

#define APCS_ALIAS0_TCM_START_ADDR 0xB110010

	.globl	bl31_entrypoint
	.globl	bl31_warm_entrypoint
	.globl  bl31_entrypoint_qti
	/* -----------------------------------------------------
	 * bl31_entrypoint() is the cold boot entrypoint,
	 * executed only by the primary cpu.
	 * -----------------------------------------------------
	 */

func bl31_entrypoint_qti
	ldr     x9, =bl31_entrypoint
	br      x9
endfunc bl31_entrypoint_qti

func bl31_entrypoint
	/* ---------------------------------------------------------------
	 * Stash the previous bootloader arguments x0 - x3 for later use.
	 * ---------------------------------------------------------------
	 */
	mov	x20, x0
	mov	x21, x1
	mov	x22, x2
	mov	x23, x3

	adrp	x0, __BSS_START__
	add	x0, x0, :lo12:__BSS_START__
	adrp	x1, __BSS_END__
	add	x1, x1, :lo12:__BSS_END__
	sub	x1, x1, x0
	bl	zeromem

	/* Clear the upper 32 bits, sometimes it contains junk */
	mov	x0, x20
	mov     x1, #0xffff
	lsl     x1, x1, #0x10
	orr     x1, x1, #0xffff
	lsl     x1, x1, #0x20
	bic     x0, x0, x1

	/* Populate xbl_params using bootloader passed parameters */
	mov	x1, x0
	adr     x0, xbl_params
	mov	x2, #0xB88 /* 0xB88 is sizeof(xbl_params) */
	bl	memcpy1

	/* Work around to ignore xBL Sec L2 TCM xPU access violation */
	mov_imm x1, 0x106
	mov_imm x0, APCS_MPU_AXI_CH_XPU_CR
	str w1, [x0]
	dsb sy
	isb

	mov_imm x0, APCS_MPU_AXI_CH_XPU_SCR
	str w1, [x0]
	dsb sy
	isb

	mov_imm x0, APCS_MPU_AXI_CH_XPU_MCR
	str w1, [x0]
	dsb sy
	isb

	mov_imm x1, 0x0
	mov_imm x0, APCS_MPU_AXI_CH_XPU_SESRRESTORE
	str w1, [x0]
	dsb sy
	isb

	mov_imm x0, APCS_MPU_AXI_CH_XPU_ESRRESTORE
	str w1, [x0]
	dsb sy
	isb

	mov_imm x0, APCS_MPU_AXI_CH_XPU_MESRRESTORE
	str w1, [x0]
	dsb sy
	isb

	/* Zero out xBL Sec image */
	mov_imm x0, XBL_SEC_IMG_BASE
	add x1, x0, XBL_SEC_IMG_SIZE
	sub x1, x1, x0
	bl zeromem

	/* Disable L2 TCM */
	mov_imm x0, APCS_ALIAS0_TCM_START_ADDR
	mov_imm x1, 0x0
	str w1, [x0]
	dsb sy
	isb

	/* --------------------------------------------------------------------
	 * If PIE is enabled, fixup the Global descriptor Table and dynamic
	 * relocations
	 * --------------------------------------------------------------------
	 */
#if ENABLE_PIE
	mov_imm	x0, BL31_BASE
	mov_imm	x1, BL31_LIMIT
	bl	fixup_gdt_reloc
#endif /* ENABLE_PIE */

#if !RESET_TO_BL31
	/* ---------------------------------------------------------------------
	 * For !RESET_TO_BL31 systems, only the primary CPU ever reaches
	 * bl31_entrypoint() during the cold boot flow, so the cold/warm boot
	 * and primary/secondary CPU logic should not be executed in this case.
	 *
	 * Also, assume that the previous bootloader has already initialised the
	 * SCTLR_EL3, including the endianness, and has initialised the memory.
	 * ---------------------------------------------------------------------
	 */
	el3_entrypoint_common					\
		_init_sctlr=0					\
		_warm_boot_mailbox=0				\
		_secondary_cold_boot=0				\
		_init_memory=0					\
		_init_c_runtime=1				\
		_exception_vectors=runtime_exceptions
#else

	/* ---------------------------------------------------------------------
	 * For RESET_TO_BL31 systems which have a programmable reset address,
	 * bl31_entrypoint() is executed only on the cold boot path so we can
	 * skip the warm boot mailbox mechanism.
	 * ---------------------------------------------------------------------
	 */

	el3_entrypoint_common					\
		_init_sctlr=1					\
		_warm_boot_mailbox=!PROGRAMMABLE_RESET_ADDRESS	\
		_secondary_cold_boot=!COLD_BOOT_SINGLE_CPU	\
		_init_memory=1					\
		_init_c_runtime=1				\
		_exception_vectors=runtime_exceptions

	/* ---------------------------------------------------------------------
	 * For RESET_TO_BL31 systems, BL31 is the first bootloader to run so
	 * there's no argument to relay from a previous bootloader. Zero the
	 * arguments passed to the platform layer to reflect that.
	 * ---------------------------------------------------------------------
	 */
	mov	x20, 0
	mov	x21, 0
	mov	x22, 0
	mov	x23, 0
#endif /* RESET_TO_BL31 */

	/* --------------------------------------------------------------------
	 * Perform BL31 setup
	 * --------------------------------------------------------------------
	 */
	mov	x0, x20
	mov	x1, x21
	mov	x2, x22
	mov	x3, x23
	bl	bl31_setup

	/* --------------------------------------------------------------------
	 * Enable pointer authentication
	 * --------------------------------------------------------------------
	 */
#if ENABLE_PAUTH
	mrs	x0, sctlr_el3
	orr	x0, x0, #SCTLR_EnIA_BIT
	msr	sctlr_el3, x0
	isb
#endif /* ENABLE_PAUTH */

	/* --------------------------------------------------------------------
	 * Jump to main function.
	 * --------------------------------------------------------------------
	 */
	bl	bl31_main

	/* --------------------------------------------------------------------
	 * Clean the .data & .bss sections to main memory. This ensures
	 * that any global data which was initialised by the primary CPU
	 * is visible to secondary CPUs before they enable their data
	 * caches and participate in coherency.
	 * --------------------------------------------------------------------
	 */
	adr	x0, __DATA_START__
	adr	x1, __DATA_END__
	sub	x1, x1, x0
	bl	clean_dcache_range

	adr	x0, __BSS_START__
	adr	x1, __BSS_END__
	sub	x1, x1, x0
	bl	clean_dcache_range

	b	el3_exit
endfunc bl31_entrypoint

	/* --------------------------------------------------------------------
	 * This CPU has been physically powered up. It is either resuming from
	 * suspend or has simply been turned on. In both cases, call the BL31
	 * warmboot entrypoint
	 * --------------------------------------------------------------------
	 */
func_in_section bl31_warm_entrypoint, QTI_IMEM_RESET_ENTRY
	/* Save X0,X1 and X30, since those registers will be clobbered*/
	msr     TPIDR_EL0, x0
	msr     TPIDR_EL1, x1
	msr     TPIDRRO_EL0, x30

	bl	qti_sysdbg_reset_check

#if ENABLE_RUNTIME_INSTRUMENTATION
	/*
	 * This timestamp update happens with cache off.  The next
	 * timestamp collection will need to do cache maintenance prior
	 * to timestamp update.
	 */
	pmf_calc_timestamp_addr rt_instr_svc, RT_INSTR_EXIT_HW_LOW_PWR
	mrs	x1, cntpct_el0
	str	x1, [x0]
#endif
	/*
	 * On the warm boot path, most of the EL3 initialisations performed by
	 * 'el3_entrypoint_common' must be skipped:
	 *
	 *  - Only when the platform bypasses the BL1/BL31 entrypoint by
	 *    programming the reset address do we need to initialise SCTLR_EL3.
	 *    In other cases, we assume this has been taken care by the
	 *    entrypoint code.
	 *
	 *  - No need to determine the type of boot, we know it is a warm boot.
	 *
	 *  - Do not try to distinguish between primary and secondary CPUs, this
	 *    notion only exists for a cold boot.
	 *
	 *  - No need to initialise the memory or the C runtime environment,
	 *    it has been done once and for all on the cold boot path.
	 */
	el3_entrypoint_common					\
		_init_sctlr=PROGRAMMABLE_RESET_ADDRESS		\
		_warm_boot_mailbox=0				\
		_secondary_cold_boot=0				\
		_init_memory=0					\
		_init_c_runtime=0				\
		_exception_vectors=runtime_exceptions

	/*
	 * We're about to enable MMU and participate in PSCI state coordination.
	 *
	 * The PSCI implementation invokes platform routines that enable CPUs to
	 * participate in coherency. On a system where CPUs are not
	 * cache-coherent without appropriate platform specific programming,
	 * having caches enabled until such time might lead to coherency issues
	 * (resulting from stale data getting speculatively fetched, among
	 * others). Therefore we keep data caches disabled even after enabling
	 * the MMU for such platforms.
	 *
	 * On systems with hardware-assisted coherency, or on single cluster
	 * platforms, such platform specific programming is not required to
	 * enter coherency (as CPUs already are); and there's no reason to have
	 * caches disabled either.
	 */
#if HW_ASSISTED_COHERENCY || WARMBOOT_ENABLE_DCACHE_EARLY
	mov	x0, xzr
#else
	mov	x0, #DISABLE_DCACHE
#endif
	bl	bl31_plat_enable_mmu

	/* --------------------------------------------------------------------
	 * Enable pointer authentication
	 * --------------------------------------------------------------------
	 */
#if ENABLE_PAUTH
	bl	pauth_load_bl_apiakey

	mrs	x0, sctlr_el3
	orr	x0, x0, #SCTLR_EnIA_BIT
	msr	sctlr_el3, x0
	isb
#endif /* ENABLE_PAUTH */

	bl	psci_warmboot_entrypoint

#if ENABLE_RUNTIME_INSTRUMENTATION
	pmf_calc_timestamp_addr rt_instr_svc, RT_INSTR_EXIT_PSCI
	mov	x19, x0

	/*
	 * Invalidate before updating timestamp to ensure previous timestamp
	 * updates on the same cache line with caches disabled are properly
	 * seen by the same core. Without the cache invalidate, the core might
	 * write into a stale cache line.
	 */
	mov	x1, #PMF_TS_SIZE
	mov	x20, x30
	bl	inv_dcache_range
	mov	x30, x20

	mrs	x0, cntpct_el0
	str	x0, [x19]
#endif
	b	el3_exit
endfunc bl31_warm_entrypoint
