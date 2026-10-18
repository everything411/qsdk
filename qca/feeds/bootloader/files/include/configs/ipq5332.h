/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _IPQ5332_H_
#define _IPQ5332_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/sizes.h>
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
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR	\
		(CONFIG_TEXT_BASE - CONFIG_SYS_MALLOC_LEN -\
			CONFIG_ENV_SIZE - GENERATED_GBL_DATA_SIZE)

#define CONFIG_MACH_TYPE                        (g_board_machid)
#define CFG_CUSTOM_LOAD_ADDR			(g_load_addr)

/* override the counter frequency incase of emulation platform */
#ifdef CFG_EMULATION
#define CFG_EMUL_FREQUENCY_DIVIDER		150
#define CFG_SYS_HZ_CLOCK			(CONFIG_COUNTER_FREQUENCY / \
						CFG_EMUL_FREQUENCY_DIVIDER)
#else
#define CFG_SYS_HZ_CLOCK			CONFIG_COUNTER_FREQUENCY
#endif

#define CFG_SYS_SDRAM_BASE0_ADDR		0x40000000
#define CFG_SYS_SDRAM_BASE0_SIZE		0xC0000000
#define CFG_SYS_SDRAM_BASE			CFG_SYS_SDRAM_BASE0_ADDR

#define CFG_ROOTFS_LOAD_ADDR			(CFG_SYS_SDRAM_BASE + (16 << 20))

#define IPQ5332_UBOOT_END_ADDRESS		CONFIG_TEXT_BASE + \
							CONFIG_TEXT_SIZE

#ifndef CONFIG_ETH_LOW_MEM
#define NONCACHED_MEM_REGION_ADDR		((IPQ5332_UBOOT_END_ADDRESS + \
						SZ_1M - 1) & ~(SZ_1M - 1))
#define NONCACHED_MEM_REGION_SIZE		SZ_1M

#endif /* ifnot defined CONFIG_ETH_LOW_MEM */

#define CFG_NC_RESERVATION			1

#define CFG_NR_CPUS				4

#ifdef CONFIG_NET_RETRY_COUNT
#undef CONFIG_NET_RETRY_COUNT
#define CONFIG_NET_RETRY_COUNT			500
#endif

#endif /* _IPQ5332_H_ */
