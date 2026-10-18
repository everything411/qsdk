// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/sizes.h>
#include <asm/arch/sysmap-ipq5424.h>

#ifndef __ASSEMBLY__
#include <compiler.h>
extern uint32_t g_board_machid;
extern uint32_t g_load_addr;
extern uint32_t g_env_offset;
#endif

#if defined(CONFIG_ENV_IS_IN_SPI_FLASH) && defined(CONFIG_ENV_OFFSET)
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
#define CONFIG_CUSTOM_SYS_INIT_SP_ADDR         	(CONFIG_TEXT_BASE -\
						CONFIG_SYS_MALLOC_LEN -\
						CONFIG_ENV_SIZE -\
						GENERATED_GBL_DATA_SIZE)

#define CFG_SYS_BAUDRATE_TABLE			{ 115200, 230400,	\
							460800, 921600 }


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
#define KERNEL_START_ADDR                   	CFG_SYS_SDRAM_BASE
#define BOOT_PARAMS_ADDR                    	(KERNEL_START_ADDR + 0x100)

#define CONFIG_MACH_TYPE			(g_board_machid)
#define CFG_CUSTOM_LOAD_ADDR			(g_load_addr)

#define PHY_ANEG_TIMEOUT			100
#define FDT_HIGH				0x88500000

#define IPQ5424_UBOOT_END_ADDRESS		CONFIG_TEXT_BASE + \
							CONFIG_TEXT_SIZE
#define IPQ5424_DDR_SIZE				(0x3UL * SZ_2G)
#define IPQ5424_DDR_UPPER_SIZE_MAX		(IPQ5424_DDR_SIZE - \
						(CFG_SYS_SDRAM_BASE - \
						IPQ5424_UBOOT_END_ADDRESS))

#define IPQ5424_DDR_LOWER_SIZE			(CONFIG_TEXT_BASE - \
							CFG_SYS_SDRAM_BASE)
#define ROOT_FS_PART_NAME			"rootfs"
#define ROOT_FS_ATL_PART_NAME			"rootfs_1"

#define CONFIG_ROOTFS_LOAD_ADDR			CFG_SYS_SDRAM_BASE + (32 << 20)

#define MTDPARTS_MAXLEN				4096

#define QFPROM_CORR_TME_OEM_ATE_ROW0_LSB	0xA40E0
#define QFPROM_CORR_TME_OEM_ATE_ROW1_LSB	0xA40E8

#ifndef CONFIG_ETH_LOW_MEM
#define NONCACHED_MEM_REGION_ADDR		((IPQ5424_UBOOT_END_ADDRESS + \
						SZ_1M - 1) & ~(SZ_1M - 1))
#define NONCACHED_MEM_REGION_SIZE		SZ_1M

/*
 * Refer above memory layout,
 * Non-Cached Memory should not begin at above 0x8A400000 since upcoming
 * memory regions are being used in the other boot components
 */
#if (NONCACHED_MEM_REGION_ADDR > 0x8A500000)
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

#define CFG_NR_CPUS	4

#ifdef CONFIG_GPT_UPDATE_PARAMS
#define DEFAULT_MMC_FLASH_SIZE 0xE90000
#define DEFAULT_NOR_FLASH_SIZE 0x2000
#endif

#ifdef CONFIG_NET_RETRY_COUNT
#undef CONFIG_NET_RETRY_COUNT
#define CONFIG_NET_RETRY_COUNT			500

#ifdef CONFIG_BOOTDELAY
#undef CONFIG_BOOTDELAY
#endif
#define CONFIG_BOOTDELAY			2
#endif

#ifdef CONFIG_EARLY_CLOCK_ENABLE
#define GCC_BASE 0x1800000
#endif
