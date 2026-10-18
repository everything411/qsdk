/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ9574_H__
#define __IPQ9574_H__

#define TCSR_BOOT_MISC_REG			((u32 *)0x193D100)

/*
 * Rootfs authentication fuse
 */
#define ROOTFS_AUTH_FUSE			0xA6044

struct fuse_payload {
	u32 fuse_addr;
	u32 val;
};

/* Crashdump minimal configs */
#define CFG_CPU_CONTEXT_DUMP_SIZE		0x1000
#define TME_CTXT_SIZE				(300 * 1024)

#define TME_OEM_ATE_FUSE_START			0x000A00C0
#define TME_OEM_ATE_FUSE_CNT			0x2
#define TME_OEM_ATE_FUSE_READ_SIZE		0x4

#define TME_OEM_MRC_HASH_FUSE_START		0x000A00D8
#define TME_OEM_MRC_HASH_FUSE_CNT		0xE
#define TME_OEM_MRC_HASH_FUSE_READ_SIZE		0x4

#define KERNEL_START_ADDR			CFG_SYS_SDRAM_BASE
#define BOOT_PARAMS_ADDR			(CFG_SYS_SDRAM_BASE + 0x100)
#define FDT_HIGH				0x48500000

#define PHY_ANEG_TIMEOUT			100

#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#define TME_AUTH_EN_MASK			0x80
#define TME_OEM_ID_MSK				0xFFFF0000
#define TME_PRODUCT_ID_MSK			0x0000FFFF

#define CRASH_DUMP_ADDR_IMEM			0x8600658
#define CFG_QTI_KERN_WDT_ADDR			*((unsigned int *)0x08600658)
#define CFG_QTI_KERN_TLV_ADDR		(CFG_QTI_KERN_WDT_ADDR + TLV_BUF_OFFSET)

#ifdef CONFIG_BOOT_BANK_FIXUP
#define BOOTED_BANK_ADDR_IMEM			0x86000D0
#endif

#if defined(CONFIG_IPQ_MINIDUMP_VERSION_V2)
#define TLV_BUF_OFFSET				(489 * 1024) - TME_CTXT_SIZE
#define CFG_TLV_DUMP_SIZE			(23 * 1024)
#else
#define TLV_BUF_OFFSET				(500 * 1024) - TME_CTXT_SIZE
#define CFG_TLV_DUMP_SIZE			(12 * 1024)
#endif /* CONFIG_IPQ_MINIDUMP_VERSION_V2 */

#ifdef CONFIG_IPQ_MINIDUMP
/* CPU stack dump configuration for IPQ9574 (Alder) */
#define CPU_STACK_RESET_REASON_ADDR		0x086007A4
#define CPU_STACK_RESET_REASON_HLOS_PANIC	0x47
#define CPU_STACK_RESET_REASON_WD		0x1
#define CPU_STACK_PTR_REG			EL1
#define CPU_STACK_MAX_CPUS			4
#endif /* CONFIG_IPQ_MINIDUMP */
#endif
