/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _IPQ5424_H_
#define _IPQ5424_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/sizes.h>
extern uint32_t g_board_machid;
extern uint32_t g_load_addr;
extern uint32_t g_env_offset;
#endif

#if defined(CONFIG_ENV_IS_IN_SPI_FLASH) && defined(CONFIG_ENV_OFFSET) && defined(CONFIG_RUNTIME_SF_ENV_UPDATE)
#undef CONFIG_ENV_OFFSET
#define CONFIG_ENV_OFFSET       g_env_offset
#endif

/*
 * Memory layout
 *
   8000_0000-->	 _____________________  DRAM Base
	        |		      |
	        |		      |
	        |		      |
   8A20_0000--> |_____________________|
	        |                     |
	        |    STACK - 502KB    |
	        |_____________________|
	        |		      |
	        |      Global Data    |
	        |_____________________|
	        |		      |
	        |      Board Data     |
   8A28_0000--> |_____________________|
	        |		      |
	        |    HEAP - 1024KB    |
	        |      (inc. ENV)     |
   8A38_0000--> |_____________________|
	        |		      |
                |    TEXT - 1536KB    |
   8A50_0000--> |_____________________|
	        |		      |
	        | NONCACHED MEM - 1MB |
   8A60_0000--> |_____________________|
	        |                     |
	        |                     |
   C000_0000--> |_____________________| DRAM End
*/

#define CONFIG_HAS_CUSTOM_SYS_INIT_SP_ADDR
#if defined(CONFIG_SPL)
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR		CONFIG_SPL_TEXT_BASE
#else
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR		\
		(CONFIG_TEXT_BASE - CONFIG_SYS_MALLOC_LEN -\
			CONFIG_ENV_SIZE - GENERATED_GBL_DATA_SIZE)
#endif

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

#define CFG_SYS_SDRAM_BASE0_ADDR		0x80000000
#define CFG_SYS_SDRAM_BASE0_SIZE		0x80000000
#if (CONFIG_NR_DRAM_BANKS > 1)
#define CFG_SYS_SDRAM_BASE1_ADDR		0x800000000
#define CFG_SYS_SDRAM_BASE1_SIZE		0x180000000
#endif

#define CFG_SYS_SDRAM_BASE			CFG_SYS_SDRAM_BASE0_ADDR
#define CFG_ROOTFS_LOAD_ADDR			(CFG_SYS_SDRAM_BASE + (32 << 20))

#define IPQ5424_UBOOT_END_ADDRESS		CONFIG_TEXT_BASE + \
							CONFIG_TEXT_SIZE
#ifndef CONFIG_ETH_LOW_MEM
#define NONCACHED_MEM_REGION_ADDR		((IPQ5424_UBOOT_END_ADDRESS + \
						SZ_1M - 1) & ~(SZ_1M - 1))
#define NONCACHED_MEM_REGION_SIZE		SZ_1M

#endif /* ifnot defined CONFIG_ETH_LOW_MEM */

#define CFG_NR_CPUS	4

#ifdef CONFIG_NET_RETRY_COUNT
#undef CONFIG_NET_RETRY_COUNT
#define CONFIG_NET_RETRY_COUNT			500
#endif

#endif /* _IPQ5424_H_ */
