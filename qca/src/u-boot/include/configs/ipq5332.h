// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/sizes.h>
#include <asm/arch/sysmap-ipq5332.h>

#ifndef __ASSEMBLY__
#include <compiler.h>
extern uint32_t g_board_machid;
extern uint32_t g_load_addr;
#endif

/*
 * Memory layout - Default
 *
   4000_0000-->	 _____________________  DRAM Base
	        |		      |
	        |		      |
	        |		      |
   4A10_0000--> |_____________________|
	        |                     |
	        |    STACK - 502KB    |
	        |_____________________|
	        |		      |
	        |      Global Data    |
	        |_____________________|
	        |		      |
	        |      Board Data     |
   4A18_0000--> |_____________________|
	        |		      |
	        |    HEAP - 1024KB    |
	        |      (inc. ENV)     |
   4A28_0000--> |_____________________|
	        |		      |
                |    TEXT - 1536KB    |
   4A40_0000--> |_____________________|
	        |		      |
	        | NONCACHED MEM - 1MB |
   4A50_0000--> |_____________________|
	        |                     |
	        |                     |
   8000_0000--> |_____________________| DRAM End
 *
 *
 * Memory layout - Tiny
 *
   4000_0000-->	 _____________________  DRAM Base
	        |		      |
	        |		      |
	        |		      |
   4A30_0000--> |_____________________|
	        |                     |
	        |    STACK - 240KB    |
	        |_____________________|
	        |		      |
	        |      Global Data    |
	        |_____________________|
	        |		      |
	        |      Board Data     |
   4A34_0000--> |_____________________|
	        |		      |
	        |    HEAP - 1152KB    |
	        |      (inc. ENV)     |
   4A46_0000--> |_____________________|
	        |		      |
                |    TEXT - 640KB     |
   4A50_0000--> |_____________________|
	        |                     |
	        |                     |
   8000_0000--> |_____________________| DRAM End
 *
 *
 * Memory layout - Tiny v2
 *
 * Use address 4AD0_0000 to 4AF0_0000, memory layout is similar to tiny.
 *
*/

#define CONFIG_HAS_CUSTOM_SYS_INIT_SP_ADDR
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR         	(CONFIG_TEXT_BASE -	\
						CONFIG_SYS_MALLOC_LEN - \
						CONFIG_ENV_SIZE -	\
						GENERATED_GBL_DATA_SIZE)

#define CFG_SYS_BAUDRATE_TABLE			{ 115200, 230400, 	\
							460800, 921600 }

#define CFG_SYS_HZ_CLOCK			24000000
#define CFG_SYS_SDRAM_BASE0_ADDR		0x40000000
#define CFG_SYS_SDRAM_BASE0_SIZE		0xC0000000
#define CFG_SYS_SDRAM_BASE			CFG_SYS_SDRAM_BASE0_ADDR
#define KERNEL_START_ADDR                   	CFG_SYS_SDRAM_BASE
#define BOOT_PARAMS_ADDR                    	(KERNEL_START_ADDR + 0x100)

#define CONFIG_MACH_TYPE			(g_board_machid)
#define CFG_CUSTOM_LOAD_ADDR			(g_load_addr)

#define PHY_ANEG_TIMEOUT			100
#define FDT_HIGH				0x48500000

#define IPQ5332_UBOOT_END_ADDRESS		CONFIG_TEXT_BASE + \
							CONFIG_TEXT_SIZE
#define IPQ5332_DDR_SIZE			(0x3UL * SZ_1G)
#define IPQ5332_DDR_UPPER_SIZE_MAX		(IPQ5332_DDR_SIZE - \
						(CFG_SYS_SDRAM_BASE - \
						IPQ5332_UBOOT_END_ADDRESS))
#define IPQ5332_DDR_LOWER_SIZE			(CONFIG_TEXT_BASE - \
							CFG_SYS_SDRAM_BASE)
#define ROOT_FS_PART_NAME			"rootfs"

#define CONFIG_ROOTFS_LOAD_ADDR		CFG_SYS_SDRAM_BASE + (16 << 20)

#ifndef CONFIG_ETH_LOW_MEM
#define NONCACHED_MEM_REGION_ADDR		((IPQ5332_UBOOT_END_ADDRESS + \
						SZ_1M - 1) & ~(SZ_1M - 1))
#define NONCACHED_MEM_REGION_SIZE		SZ_1M

/*
 * Refer above memory layout,
 * Non-Cached Memory should not begin at above 0x4A400000 since upcoming
 * memory regions are being used in the other boot components
 */
#if (NONCACHED_MEM_REGION_ADDR > 0x4A400000)
#error "###: Text Segment overlaps with the Non-Cached Region"
#endif

#ifdef CONFIG_MULTI_DTB_FIT_USER_DEF_ADDR
/*
 * CONFIG_MULTI_DTB_FIT_USER_DEF_ADDR - memory used to decompress multi dtb
 * NONCACHED_MEM_REGION_ADDR - Non-Cached memory region
 * both uses same address space. So both should be same.
 *
 * Change in CONFIG_TEXT_BASE or CONFIG_TEXT_SIZE various affect this macro.
 * So, according define the CONFIG_TEXT_BASE and CONFIG_TEXT_SIZE macros.
 */
#if (CONFIG_MULTI_DTB_FIT_USER_DEF_ADDR != NONCACHED_MEM_REGION_ADDR)
#error "###: CONFIG_MULTI_DTB_FIT_USER_DEF_ADDR != NONCACHED_MEM_REGION_ADDR"
#endif
#endif

#endif /* ifnot defined CONFIG_ETH_LOW_MEM */

#define CFG_NR_CPUS				4

#ifdef CONFIG_NET_RETRY_COUNT
#undef CONFIG_NET_RETRY_COUNT
#define CONFIG_NET_RETRY_COUNT			500

#define MTDPARTS_MAXLEN				4096

#ifdef CONFIG_BOOTDELAY
#undef CONFIG_BOOTDELAY
#endif
#define CONFIG_BOOTDELAY			2
#endif

#define CFG_NC_RESERVATION
