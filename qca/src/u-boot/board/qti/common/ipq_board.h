// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _IPQSMEM_H
#define _IPQSMEM_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/byteorder.h>
#include <mtd_node.h>
#include <part.h>
#ifdef CONFIG_TARGET_IPQ9574
#include "../ipq9574/ipq9574.h"
#endif
#ifdef CONFIG_TARGET_IPQ5332
#include "../ipq5332/ipq5332.h"
#endif
#ifdef CONFIG_TARGET_IPQ5424
#include "../ipq5424/ipq5424.h"
#endif

#ifdef CONFIG_SCM
#include <mach/ipq_scm.h>
#else
#define ipq_scm_call(...)		-ENODATA
#endif

#if defined(CONFIG_WDT) && defined(CONFIG_WDT_QTI)
#include <watchdog.h>
#define watchdog_reset()	schedule()
#else
#define watchdog_reset()
#endif

#ifndef __IPQ_RESET__
#define __IPQ_RESET__
#define reset()			do_reset(NULL, 0, 0, NULL)
#endif

#ifndef IPQ_NAND_FLASH_VALID_BIT
#define IPQ_NAND_FLASH_VALID_BIT	3
#endif
#define	CFG_IPQ_NAND_PART		BIT(IPQ_NAND_FLASH_VALID_BIT)

#define __str_fmt(x)		"%-" #x "s"
#define _str_fmt(x)		__str_fmt(x)
#define smem_ptn_name_fmt	_str_fmt(SMEM_PTN_NAME_MAX)
#ifndef IPQ_ETH_FW_PART_NAME
#define IPQ_ETH_FW_PART_NAME	"0:ETHPHYFW"
#endif
#ifndef IPQ_ETH_FW_PART_SIZE
#define IPQ_ETH_FW_PART_SIZE	0x80000
#endif
#define BOARD_DTS_MAX_NAMELEN	30

#define DUMP2MEM_MAGIC1_COOKIE			0x4D494E49
#define DUMP2MEM_MAGIC2_COOKIE			0x44554D50

#define ROOTFS_AUTH_EN			0x20
/*
 * Execute DPR
 */
#ifdef CONFIG_DPR_VER_1_0
#define execute_dpr_fun(a, b, c, d)		execute_dprv1(a, b, c, d)
#elif CONFIG_DPR_VER_2_0
#define execute_dpr_fun(a, b, c, d)		execute_dprv2(a, b, c, d)
#elif CONFIG_DPR_VER_3_0
#define execute_dpr_fun(a, b, c, d)		execute_dprv3(a, b, c, d)
#endif

/*
 * Check secure boot enablement
 */
#ifdef CONFIG_SCM_V1
#define is_secure_boot()	is_secure_boot_v1()
#elif CONFIG_SCM_V2
#define is_secure_boot()	is_secure_boot_v2()
#else
#define is_secure_boot()	is_secure_boot_fake()
#endif

#define UNUSED_VAR(x)	(void)x
/*
 * Authenticate kernel image during bootup
 */

#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_AUTHENTICATE_KERNEL_V1(_param, _a, ...)		\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_KERNEL_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#elif IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_AUTHENTICATE_KERNEL_V2(_param, _a, _b, _c, _d, _e)	\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b; 					\
		(_param).buff[2] = _c;	 				\
		(_param).buff[3] = _d; 					\
		(_param).buff[4] = _e; 					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_AUTHENTICATE_KERNEL(...) break;
#endif

/*
 * Authenticate Rootfs during bootup
 * as well as to authenticate signed image
 * of all the sub-systems
 */

#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_SECURE_AUTHENTICATE_V1(_param, _a, _b, _c, _d, _e)	\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b; 					\
		(_param).buff[2] = _c;	 				\
		UNUSED_VAR(_e);	 					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_WRITE_OP;			\
		(_param).len = 3;					\
	} while (0)
#elif IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_SECURE_AUTHENTICATE_V2(_param, _a, _b, _c, _d, _e)	\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _c;					\
		(_param).buff[1] = _b; 					\
		(_param).buff[2] = _a;	 				\
		(_param).buff[3] = _d; 					\
		(_param).buff[4] = _e; 					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_SECURE_AUTHENTICATE(...) break;
#endif

/*
 * Helps to read fuse valuse
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_READ_FUSE_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_LIST_FUSE;				\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_READ_FUSE(...) break;
#endif

/*
 * verify hash value with meta data
 */

#if IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_VERIFY_HASH_V1(_param, _a, _b, _c, _d, _e)		\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_ROOTFS_HASH_VERIFY;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b; 					\
		(_param).buff[2] = _c;	 				\
		(_param).buff[3] = _d; 					\
		(_param).buff[4] = _e; 					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_WRITE_OP;			\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_WRITE_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_VERIFY_HASH(...) break;
#endif

/*
 * check for secure boot
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_SECURE_BOOT_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_CHECK_SECURE_FUSE;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_READ_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_SECURE_BOOT(...) break;
#endif

/*
 * Set active partition for image version anti roll-back
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_SET_ACTIVE_PARTITION_V1(_param, _a)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SET_ACTIVE_PART;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_SET_ACTIVE_PARTITION(...) break;
#endif

/*
 * blow fuse
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_FUSE_IPQ_V1(_param, _a, _b, _c, _d, _e)		\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_FUSE_IPQ;				\
		(_param).buff[0] = _a;					\
		UNUSED_VAR(_b);						\
		UNUSED_VAR(_c);						\
		UNUSED_VAR(_d);						\
		UNUSED_VAR(_e);						\
		(_param).arg_type[0] = SCM_READ_OP;			\
		(_param).len = 1;					\
	} while (0)
#elif IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_FUSE_IPQ_V2(_param, _a, _b, _c, _d, _e)		\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b; 					\
		(_param).buff[2] = _c;	 				\
		(_param).buff[3] = _d; 					\
		(_param).buff[4] = _e; 					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_FUSE_IPQ(...) break;
#endif

/*
 * XPU secure test
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_XPU_SEC_TEST_1_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_XPU_SEC_TEST_1;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_XPU_SEC_TEST_1(...) break;
#endif

/*
 * XPU log buffer
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_XPU_LOG_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_XPU_LOG_BUFFER;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_XPU_LOG(...) break;
#endif

/*
 * TZT region notification
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_TZT_REGION_NOTIFY_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_TZT_REGION_NOTIFICATION;		\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_TZT_REGION_NOTIFY(...) break;
#endif

/*
 * TZT execute image
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_TZT_EXEC_IMG_V1(_param, _a, _b, _c)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_TZT_TESTEXEC_IMG;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).len = 3;					\
	} while (0)
#else
#define _IPQ_SCM_TZT_EXEC_IMG(...) break;
#endif

/*
 * Generate AES_256 Key
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_GENERATE_AES_256_KEY_V1(_param, _a, _b)		\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_AES_256_GEN_KEY;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_GENERATE_AES_256_KEY(...) break;
#endif

/*
 * Generate AES_256 Key with max 128 bytes context
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX_V1(_param, _a, _b)	\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_AES_256_MAX_CTXT_GEN_KEY;		\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX(...) break;
#endif

/*
 * Encrypt AES_256
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_ENCRYPT_AES_256_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_AES_256_ENC;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_ENCRYPT_AES_256(...) break;
#endif

/*
 * Decrypt AES_256
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_DECRYPT_AES_256_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_AES_256_DEC;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_DECRYPT_AES_256(...) break;
#endif

/*
 * blow fuse
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_CHECK_SCM_SUPPORT_V1(_param, _a)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_CHECK_SUPPORT;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_CHECK_SCM_SUPPORT(...) break;
#endif

/*
 * Enable SDI path
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_ENABLE_SDI_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SDI_CLEAR;				\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_ENABLE_SDI(...) break;
#endif

/*
 * I/O write
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_IO_WRITE_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_IO_WRITE;				\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_IO_WRITE(...) break;
#endif

/*
 * I/O read
 */
#if CONFIG_SCM
#define _IPQ_SCM_IO_READ_V1(_param, _a)					\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_IO_READ;				\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_IO_READ(...) break;
#endif

/*
 * Read PHYA0 region
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_READ_PHY_REG_V1(_param, _a)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_PHYA0_REGION_RD;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_READ_PHY_REG(...) break;
#endif

/*
 * Write PHYA0 region
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_WRITE_PHY_REG_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_PHYA0_REGION_WR;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_WRITE_PHY_REG(...) break;
#endif

/*
 * Execute DPR
 */
#if defined(CONFIG_SCM_V1) && defined(CONFIG_DPR_VER_1_0)
#define _IPQ_SCM_EXECUTE_DPR_V1(_param, _a, ...)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_TME_DPR_PROCESSING;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#elif defined(CONFIG_SCM_V1) && defined(CONFIG_DPR_VER_2_0)
#define _IPQ_SCM_EXECUTE_DPR_V2(_param, _a, _b, ...)			\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_TME_DPR_PROCESSING;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#elif defined(CONFIG_SCM_V2) && defined(CONFIG_DPR_VER_3_0)
#define _IPQ_SCM_EXECUTE_DPR_V3(_param, _a, _b, _c, _d, _e, ...)	\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b; 					\
		(_param).buff[2] = _c;	 				\
		(_param).buff[3] = _d; 					\
		(_param).buff[4] = _e; 					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_EXECUTE_DPR(...) break;
#endif

/*
 * Check ATF support
 */
#if defined(CONFIG_SCM_V1)
#define _check_atf_support_V1(_param)					\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_CHECK_ATF_SUPPORT;			\
	} while (0)
#else
#define _check_atf_support(...)	break;
#endif

#ifdef CONFIG_SCM
#define _CHECK_FEATURE_V1(_param, _a)					\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_CHECK_FEATURE_ID;			\
		(_param).buff[0] = _a;					\
		(_param).len = 1;					\
	} while (0)
#else
#define _CHECK_FEATURE(...) break;
#endif

#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define	_IPQ_SCM_CLEAR_AES_KEY_V1(_param, _a)				\
	do {								\
		memset(&(_param), 0, sizeof(scm_param));		\
		(_param).type = SCM_CLEAR_AES_KEY;			\
		(_param).buff[0] = _a;					\
		(_param).len = 1;					\
	} while (0)
#endif

#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_AUTHENTICATE_KERNEL(param, a, b, c, d, e)		\
		_IPQ_SCM_AUTHENTICATE_KERNEL_V1(param, a, b, c, d, e)
#elif defined(CONFIG_SCM_V2)
#define IPQ_SCM_AUTHENTICATE_KERNEL(param, a, b, c, d, e)		\
		_IPQ_SCM_AUTHENTICATE_KERNEL_V2(param, a, b, c, d, e)
#else
#define IPQ_SCM_AUTHENTICATE_KERNEL(param, a, b, c, d, e)		\
		_IPQ_SCM_AUTHENTICATE_KERNEL(param, a, b, c, d, e)
#endif


#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_SECURE_AUTHENTICATE(param, a, b, c, d, e)		\
		_IPQ_SCM_SECURE_AUTHENTICATE_V1(param, a, b, c, d, e)
#elif defined(CONFIG_SCM_V2)
#define IPQ_SCM_SECURE_AUTHENTICATE(param, a, b, c, d, e)		\
		_IPQ_SCM_SECURE_AUTHENTICATE_V2(param, a, b, c, d, e)
#else
#define IPQ_SCM_SECURE_AUTHENTICATE(param, a, b, c, d, e)		\
		_IPQ_SCM_SECURE_AUTHENTICATE(param, a, b, c, d, e)
#endif

#ifdef CONFIG_SCM
#define IPQ_SCM_READ_FUSE(param, a, b)					\
		_IPQ_SCM_READ_FUSE_V1(param, a, b)
#else
#define IPQ_SCM_READ_FUSE(param, a, b)					\
		_IPQ_SCM_READ_FUSE(param, a, b)
#endif

#if defined(CONFIG_SCM_V2)
#define IPQ_SCM_VERIFY_HASH(param, a, b, c, d, e)			\
		_IPQ_SCM_VERIFY_HASH_V1(param, a, b, c, d, e)
#else
#define IPQ_SCM_VERIFY_HASH(param, a, b, c, d, e)			\
		_IPQ_SCM_VERIFY_HASH(param, a, b, c, d,	e)
#endif

#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_SECURE_BOOT(param, a, b)				\
		_IPQ_SCM_SECURE_BOOT_V1(param, a, b)
#else
#define IPQ_SCM_SECURE_BOOT(param, a, b)				\
		_IPQ_SCM_SECURE_BOOT(param, a, b)
#endif


#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_SET_ACTIVE_PARTITION(param, a)				\
		_IPQ_SCM_SET_ACTIVE_PARTITION_V1(param, a)
#else
#define IPQ_SCM_SET_ACTIVE_PARTITION(param, a)				\
		_IPQ_SCM_SET_ACTIVE_PARTITION(param, a)
#endif


#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_FUSE_IPQ(param, a, b, c, d, e)				\
		_IPQ_SCM_FUSE_IPQ_V1(param, a, b, c, d, e)
#elif defined(CONFIG_SCM_V2)
#define IPQ_SCM_FUSE_IPQ(param, a, b, c, d, e)				\
		_IPQ_SCM_FUSE_IPQ_V2(param, a, b, c, d, e)
#else
#define IPQ_SCM_FUSE_IPQ(param, a, b, c, d, e)				\
		_IPQ_SCM_FUSE_IPQ(param, a)
#endif

#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_XPU_SEC_TEST_1(param, a, b)				\
		_IPQ_SCM_XPU_SEC_TEST_1_V1(param, a, b)
#else
#define IPQ_SCM_XPU_SEC_TEST_1(param, a, b)				\
		_IPQ_SCM_XPU_SEC_TEST_1(param, a, b)
#endif

#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_XPU_LOG(param, a, b)					\
		_IPQ_SCM_XPU_LOG_V1(param, a, b)
#else
#define IPQ_SCM_XPU_LOG(param, a, b)					\
		_IPQ_SCM_XPU_LOG(param, a, b)
#endif

#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_TZT_REGION_NOTIFY(param, a, b)				\
		_IPQ_SCM_TZT_REGION_NOTIFY_V1(param, a, b)
#else
#define IPQ_SCM_TZT_REGION_NOTIFY(param, a, b)				\
		_IPQ_SCM_TZT_REGION_NOTIFY(param, a, b)
#endif


#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_TZT_EXEC_IMG(param, a, b, c)				\
		_IPQ_SCM_TZT_EXEC_IMG_V1(param, a, b, c)
#else
#define IPQ_SCM_TZT_EXEC_IMG(param, a, b, c)				\
		_IPQ_SCM_TZT_EXEC_IMG(param, a, b, c)
#endif


#if defined(CONFIG_SCM) && defined(CONFIG_CMD_AES_256)
#define IPQ_SCM_GENERATE_AES_256_KEY(param, a, b)			\
		_IPQ_SCM_GENERATE_AES_256_KEY_V1(param, a, b)
#else
#define IPQ_SCM_GENERATE_AES_256_KEY(param, a, b)			\
		_IPQ_SCM_GENERATE_AES_256_KEY(param, a, b)
#endif


#if defined(CONFIG_SCM) && defined(CONFIG_CMD_AES_256)
#define IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX(param, a, b)		\
		_IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX_V1(param, a, b)
#else
#define IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX(param, a, b)		\
		_IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX(param, a, b)
#endif


#if defined(CONFIG_SCM) && defined(CONFIG_CMD_AES_256)
#define IPQ_SCM_ENCRYPT_AES_256(param, a, b)				\
		_IPQ_SCM_ENCRYPT_AES_256_V1(param, a, b)
#else
#define IPQ_SCM_ENCRYPT_AES_256(param, a, b)				\
		_IPQ_SCM_ENCRYPT_AES_256(param, a, b)
#endif


#if defined(CONFIG_SCM) && defined(CONFIG_CMD_AES_256)
#define IPQ_SCM_DECRYPT_AES_256(param, a, b)				\
		_IPQ_SCM_DECRYPT_AES_256_V1(param, a, b)
#else
#define IPQ_SCM_DECRYPT_AES_256(param, a, b)				\
		_IPQ_SCM_DECRYPT_AES_256(param, a, b)
#endif


#ifdef CONFIG_SCM
#define IPQ_SCM_CHECK_SCM_SUPPORT(param, a)				\
		_IPQ_SCM_CHECK_SCM_SUPPORT_V1(param, a)
#else
#define IPQ_SCM_CHECK_SCM_SUPPORT(param, a)				\
		_IPQ_SCM_CHECK_SCM_SUPPORT(param, a)
#endif


#ifdef CONFIG_SCM
#define IPQ_SCM_ENABLE_SDI(param, a, b)					\
		_IPQ_SCM_ENABLE_SDI_V1(param, a, b)
#else
#define IPQ_SCM_ENABLE_SDI(param, a, b)					\
		_IPQ_SCM_ENABLE_SDI(param, a, b)
#endif


#ifdef CONFIG_SCM
#define IPQ_SCM_IO_WRITE(param, a, b)					\
		_IPQ_SCM_IO_WRITE_V1(param, a, b)
#else
#define IPQ_SCM_IO_WRITE(param, a, b)					\
		_IPQ_SCM_IO_WRITE(param, a, b)
#endif

#if CONFIG_SCM
#define IPQ_SCM_IO_READ(param, a)	_IPQ_SCM_IO_READ_V1(param, a)
#else
#define IPQ_SCM_IO_READ(param, a)	_IPQ_SCM_IO_READ(param, a)
#endif

#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_READ_PHY_REG(param, a)					\
		_IPQ_SCM_READ_PHY_REG_V1(param, a)
#else
#define IPQ_SCM_READ_PHY_REG(param, a)					\
		_IPQ_SCM_READ_PHY_REG(param, a)
#endif



#if defined(CONFIG_SCM_V1)
#define IPQ_SCM_WRITE_PHY_REG(param, a, b)				\
		_IPQ_SCM_WRITE_PHY_REG_V1(param, a, b)
#else
#define IPQ_SCM_WRITE_PHY_REG(param, a, b)				\
		_IPQ_SCM_WRITE_PHY_REG(param, a, b)
#endif


#if defined(CONFIG_SCM_V1) && defined(CONFIG_DPR_VER_1_0)
#define IPQ_SCM_EXECUTE_DPR(...)					\
		_IPQ_SCM_EXECUTE_DPR_V1(__VA_ARGS__, 0, 0, 0, 0, 0, 0)
#elif defined(CONFIG_SCM_V1) && defined(CONFIG_DPR_VER_2_0)
#define IPQ_SCM_EXECUTE_DPR(...)					\
		_IPQ_SCM_EXECUTE_DPR_V2(__VA_ARGS__, 0, 0, 0, 0, 0, 0)
#elif defined(CONFIG_SCM_V2) && defined(CONFIG_DPR_VER_3_0)
#define IPQ_SCM_EXECUTE_DPR(...)					\
		_IPQ_SCM_EXECUTE_DPR_V3(__VA_ARGS__, 0, 0, 0, 0, 0, 0)
#else
#define IPQ_SCM_EXECUTE_DPR(...)					\
		_IPQ_SCM_EXECUTE_DPR(__VA_ARGS__, 0, 0, 0, 0, 0, 0)
#endif

#ifdef CONFIG_SCM
#define CHECK_FEATURE(param, a)		_CHECK_FEATURE_V1(param, a)
#else
#define CHECK_FEATURE(param, a)		_CHECK_FEATURE(param, a)
#endif

#if defined(CONFIG_SCM_V1)
#define check_atf_support(param)					\
		_check_atf_support_V1(param)
#else
#define check_atf_support(param)					\
		_check_atf_support(param)
#endif

#if defined(CONFIG_SCM) && defined(CONFIG_CMD_AES_256)
#define	IPQ_SCM_CLEAR_AES_KEY(param, a)	_IPQ_SCM_CLEAR_AES_KEY_V1(param, a)
#else
#define	IPQ_SCM_CLEAR_AES_KEY(...)	break;
#endif

#ifdef CONFIG_SMEM_VERSION_C
#define part_which_flash(p)    (((p)->attr & 0xff000000) >> 24)

int gpt_find_which_flash(gpt_entry *p);

#define _SMEM_RAM_PTABLE_MAGIC_1	0x9DA5E0A8
#define _SMEM_RAM_PTABLE_MAGIC_2	0xAF9EC4E2

struct ram_partition_entry
{
	char name[CONFIG_RAM_PART_NAME_LENGTH];
				/**< Partition name, unused for now */
	u64 start_address;	/**< Partition start address in RAM */
	u64 length;		/**< Partition length in RAM in Bytes */
	u32 partition_attribute;/**< Partition attribute */
	u32 partition_category;	/**< Partition category */
	u32 partition_domain;	/**< Partition domain */
	u32 partition_type;	/**< Partition type */
	u32 num_partitions;	/**< Number of partitions on device */
	u32 hw_info;		/**< hw information such as type and freq */
	u8 highest_bank_bit;	/**< Highest bit corresponding to a bank */
	u8 reserve0;		/**< Reserved for future use */
	u8 reserve1;		/**< Reserved for future use */
	u8 reserve2;		/**< Reserved for future use */
	u32 reserved5;		/**< Reserved for future use */
	u64 available_length;	/**< Available Part length in RAM in Bytes */
};

struct usable_ram_partition_table
{
	u32 magic1;		/**< Magic number to identify valid RAM
					partition table */
	u32 magic2;		/**< Magic number to identify valid RAM
					partition table */
	u32 version;		/**< Version number to track structure
					definition changes and maintain
					backward compatibilities */
	u32 reserved1;		/**< Reserved for future use */

	u32 num_partitions;	/**< Number of RAM partition table entries */

	u32 reserved2;		/** < Added for 8 bytes alignment of header */

	/** RAM partition table entries */
	struct ram_partition_entry ram_part_entry[CONFIG_RAM_NUM_PART_ENTRIES];
};
#endif


/*
 * function declaration
 */
int smem_getpart(char *part_name, uint32_t *start, uint32_t *size);
int smem_ram_ptable_init_v2(
		struct usable_ram_partition_table *usable_ram_partition_table);


#define RAM_PARTITION_SDRAM 		14
#define RAM_PARTITION_SYS_MEMORY 	1
#define IPQ_NAND_ROOTFS_SIZE 		(64 << 20)

#define SOCINFO_VERSION_MAJOR(ver) 	((ver & 0xffff0000) >> 16)
#define SOCINFO_VERSION_MINOR(ver) 	(ver & 0x0000ffff)

#define INDEX_LENGTH			2
#define SEP1_LENGTH			1
#define VERSION_STRING_LENGTH		72
#define VARIANT_STRING_LENGTH		20
#define SEP2_LENGTH			1
#define OEM_VERSION_STRING_LENGTH	32
#define BUILD_ID_LEN			32

#define SMEM_PTN_NAME_MAX     		16
#define SMEM_PTABLE_PARTS_MAX 		32
#define SMEM_PTABLE_PARTS_DEFAULT 	16

/*
 * Board type	- 0xFFFFFFFF
 * BIT(0 - 7)	- Flash type
 * BIT(8)	- Secure board
 * BIT(9)	- ATF_SUPPORT
 * BIT(10)	- Kernel Authentication Status
 * BIT(11)	- Rootfs Authentication Status
 * BIT(12)	- Active boot set identifier
 * BIT(13)	- Invalid boot set identifier
 * BIT(14 - 31)	- Reserved
 */


#define SECURE_BOARD			BIT(8)
#define ATF_ENABLED			BIT(9)
#define KERNEL_AUTH_SUCCESS		BIT(10)
#define ROOTFS_AUTH_SUCCESS		BIT(11)
#define ACTIVE_BOOT_SET			BIT(12)
#define INVALID_BOOT			BIT(13)
#define FLASH_TYPE_MASK			0xFF

enum {
	SMEM_BOOT_NO_FLASH        = 0,
	SMEM_BOOT_NOR_FLASH       = 1,
	SMEM_BOOT_NAND_FLASH      = 2,
	SMEM_BOOT_ONENAND_FLASH   = 3,
	SMEM_BOOT_SDC_FLASH       = 4,
	SMEM_BOOT_MMC_FLASH       = 5,
	SMEM_BOOT_SPI_FLASH       = 6,
	SMEM_BOOT_NORPLUSNAND     = 7,
	SMEM_BOOT_NORPLUSEMMC     = 8,
	SMEM_BOOT_QSPI_NAND_FLASH  = 11,
	SMEM_BOOT_NORGPT_FLASH     = 12,
};

enum {
	SECURE_SYS_UPGRADE	= 0,
};

struct version_entry
{
	char index[INDEX_LENGTH];
	char colon_sep1[SEP1_LENGTH];
	char version_string[VERSION_STRING_LENGTH];
	char variant_string[VARIANT_STRING_LENGTH];
	char colon_sep2[SEP2_LENGTH];
	char oem_version_string[OEM_VERSION_STRING_LENGTH];
} __attribute__ ((__packed__));

typedef struct smem_pmic_type
{
	unsigned pmic_model;
	unsigned pmic_die_revision;
}pmic_type;

typedef struct ipq_platform_v1 {
	unsigned format;
	unsigned id;
	unsigned version;
	char     build_id[BUILD_ID_LEN];
	unsigned raw_id;
	unsigned raw_version;
	unsigned hw_platform;
	unsigned platform_version;
	unsigned accessory_chip;
	unsigned hw_platform_subtype;
}ipq_platform_v1;

typedef struct ipq_platform_v2 {
	ipq_platform_v1 v1;
	pmic_type pmic_info[3];
	unsigned foundry_id;
}ipq_platform_v2;

typedef struct ipq_platform_v3 {
	ipq_platform_v2 v2;
	unsigned chip_serial;
} ipq_platform_v3;

union ipq_platform {
	ipq_platform_v1 v1;
	ipq_platform_v2 v2;
	ipq_platform_v3 v3;
};

struct smem_machid_info {
	unsigned format;
	unsigned machid;
};

typedef struct soc_info {
	uint32_t cpu_type;
	uint32_t version;
	uint32_t soc_version_major;
	uint32_t soc_version_minor;
	unsigned int machid;
} socinfo_t;

typedef struct {
	loff_t offset;
	loff_t size;
} ipq_part_entry_t;

struct per_part_info
{
	char name[CONFIG_RAM_PART_NAME_LENGTH];
	uint32_t primaryboot;
};

#ifdef CONFIG_BOOTCONFIG_V2
typedef struct
{
#define _SMEM_DUAL_BOOTINFO_MAGIC_START					0xA3A2A1A0
#define _SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_MODE			0xA3A2A1A1
#define _SMEM_DUAL_BOOTINFO_MAGIC_START_UNIFIED_FAILSAFE		0xA3A2A1A2
#define _SMEM_DUAL_BOOTINFO_MAGIC_START_TRY_UNIFIED_FAILSAFE		0xA3A2A1A3
#define _SMEM_DUAL_BOOTINFO_MAGIC_END					0xB3B2B1B0

	/* Magic number for identification when reading from flash */
	uint32_t magic_start;
	/* upgradeinprogress indicates to attempting the upgrade */
	uint32_t    age;
	/* numaltpart indicate number of alt partitions */
	uint32_t    numaltpart;

	struct per_part_info per_part_entry[CONFIG_NUM_ALT_PARTITION];

	uint32_t magic_end;

} ipq_smem_bootconfig_info_t;
#elif CONFIG_BOOTCONFIG_V3

typedef struct __attribute__((packed))
{
#define  _SMEM_DUAL_BOOTINFO_MAGIC_START		0x72637279
	uint32_t magic_start;   /* Magic number for identification when reading from flash */
	uint32_t image_set_status; /* Represents the health status of the Bank A & B*/
	uint32_t owner;  /* Indicates which component updated the health status of a Bank */
	uint32_t boot_set;    /* Indicates the current active Bank*/
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t crc;  /* CRC field for fields above */
} ipq_smem_bootconfig_info_t;
#endif

#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
typedef struct {
	gpt_entry *gpt_pte;
	int ncount;
} gpt_pte_info_t;
#endif

typedef struct {
	uint32_t		flash_type;
	uint32_t		flash_index;
	uint32_t		flash_chip_select;
	uint32_t		flash_block_size;
	uint32_t		flash_density;
	uint32_t		flash_secondary_type;
	uint32_t		primary_mibib;
#ifdef CONFIG_BOOTCONFIG_V3
	uint32_t		edl_mode;
	uint32_t		try_mode_inprogress;
#endif
	ipq_part_entry_t	hlos;
	ipq_part_entry_t	rootfs;
	ipq_part_entry_t	dtb;
	ipq_part_entry_t	training;
	ipq_smem_bootconfig_info_t *ipq_smem_bootconfig_info;
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
	gpt_pte_info_t mmc_gpt_pte;
	gpt_pte_info_t nor_gpt_pte;
#endif
} ipq_smem_flash_info_t;

typedef struct {
	uint32_t identifier;
	uint32_t smem_size;
	uint64_t smem_base_addr;
	uint16_t smem_max_items;
	uint16_t smem_rsvd;
} ipq_smem_target_info_t;

#define IPQ_SMEM_TARGET_INFO_IDENTIFIER		0x49494953

struct smem_ptn {
	char name[SMEM_PTN_NAME_MAX];
	unsigned start;
	unsigned size;
	unsigned attr;
} __attribute__ ((__packed__));

struct smem_ptable {
#define _SMEM_PTABLE_MAGIC_1 0x55ee73aa
#define _SMEM_PTABLE_MAGIC_2 0xe35ebddb
	unsigned magic[2];
	unsigned version;
	unsigned len;
	struct smem_ptn parts[SMEM_PTABLE_PARTS_MAX];
} __attribute__ ((__packed__));

typedef struct {
        unsigned int image_type;
        unsigned int header_vsn_num;
        unsigned int image_src;
        unsigned int image_dest_ptr;
        unsigned int image_size;
        unsigned int code_size;
        unsigned int signature_ptr;
        unsigned int signature_size;
        unsigned int cert_chain_ptr;
        unsigned int cert_chain_size;
} mbn_header_t;

typedef struct auth_cmd_buf {
	unsigned long type;
	unsigned long size;
	unsigned long addr;
} auth_cmd_buf;

/*
 * NAND Flash Configs
 */
#ifdef CONFIG_QSPI_LAYOUT_SWITCH
#define QTI_NAND_LAYOUT_SBL			0
#define QTI_NAND_LAYOUT_LINUX			1
#define QTI_NAND_LAYOUT_MAX			2

int qti_nand_get_curr_layout(void);
#endif

/*
 * Extern variables
 */
extern struct node_info * fnodes;
extern int * fnode_entires;

#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map
{
    int machid;
    char* dts;
};

extern struct machid_dts_map * machid_dts_info;
extern int * machid_dts_entries;
#endif /* CONFIG_DTB_RESELECT */

enum debug_component {
	DBG_DISABLE = 0,
	DBG_CRASHDUMP,
};

#define DUMP_NAME_STR_MAX_LEN			20

enum {
	FULLDUMP= 0,
	MINIDUMP,
	MINIDUMP_AND_FULLDUMP,
};

enum {
	DUMP_TO_TFTP = 0,
	DUMP_TO_USB,
	DUMP_TO_MEM,
	DUMP_TO_NVMEM,
	DUMP_TO_FLASH,
	DUMP_TO_EMMC,
};

enum {
	RESET_V1 = 1,
	RESET_V2,
};

enum {
	SDX_POWER_CYCLE	= 0,		/* Power cycle the SDX in crash path */
};

typedef struct {
	char name[DUMP_NAME_STR_MAX_LEN];/* dump name */
	uint64_t start_addr;		/* dump start addr */
	uint64_t size;			/* dump size
					   0xBAD0FF5E - get ram_size runtime,
					   otherwise specify size */
	uint8_t dump_level;		/* dump level
					   refer crashdump_level_t */
	uint32_t split_bin_sz;		/* split bin size
					   if non-zero means, if size is
					   greater than split_bin_sz, it will
					   dump entire region as seperate bin
					   of size split_bin_sz */
	uint8_t is_aligned_access:1;	/* If this flag is set,
					   'start' is considered a unaligned
					   address, so content will be copied
					   to a aligned one and gets dumped */
	uint8_t compression_support:1;	/* does this binary need to be
					   compressed ? non-zero means true. */
	uint8_t dumptoflash_support:1;	/* does this binary need to be
					   dumped in flash ? non-zero
					   means true. */
	uint8_t check_dump_support:1;	/* If this flag is set, a check is
					   being made for dump-specific
					   skip conditions. */
} crashdump_infos_t;

extern crashdump_infos_t *board_dumpinfo;
extern uint8_t *board_dump_entries;
extern uint8_t g_recovery_path __attribute__((section(".data")));

#if IS_ENABLED(CONFIG_MMC) || IS_ENABLED(CONFIG_NOR_BLK)
/* BLK part info */
typedef struct {
	struct disk_partition *info;
	struct blk_desc *desc;
	char *name;
	int flash_type;
	int devnum;
	int isnand;
} blkpart_info_t;
#endif

#define BLK_PART_GET_INFO_S(_ptr, _name, _info, _fl)		\
	do {							\
		(&(_ptr))->name = _name;			\
		(&(_ptr))->info = _info;			\
		(&(_ptr))->flash_type = _fl;			\
		(&(_ptr))->devnum = 0;				\
	} while(0)
/*
 * QCN9224 fusing
 */
#define QCN_VENDOR_ID					0x17CB
#define QCN9224_DEVICE_ID				0x1109
#define QCN9000_DEVICE_ID				0x1104
#define MAX_UNWINDOWED_ADDRESS				0x80000
#define WINDOW_ENABLE_BIT				0x40000000
#define WINDOW_SHIFT					19
#define WINDOW_VALUE_MASK				0x3F
#define WINDOW_START					MAX_UNWINDOWED_ADDRESS
#define WINDOW_RANGE_MASK				0x7FFFF

#define QCN9224_PCIE_REMAP_BAR_CTRL_OFFSET		0x310C
#define PCIE_SOC_GLOBAL_RESET_ADDRESS			0x3008
#define QCN9224_TCSR_SOC_HW_VERSION			0x1B00000
#define QCN9224_TCSR_SOC_HW_VERSION_MASK		GENMASK(11,8)
#define QCN9224_TCSR_SOC_HW_VERSION_SHIFT		8
#define PCIE_SOC_GLOBAL_RESET_VALUE			0x5
#define MAX_SOC_GLOBAL_RESET_WAIT_CNT			50 /* x 20msec */

#define QCN9224_TCSR_PBL_LOGGING_REG			0x01B00094
#define QCN9224_SECURE_BOOT0_AUTH_EN			0x01e24010
#define QCN9224_OEM_MODEL_ID				0x01e24018
#define QCN9224_ANTI_ROLL_BACK_FEATURE			0x01e2401c
#define QCN9224_OEM_PK_HASH				0x01e24060
#define QCN9224_SECURE_BOOT0_AUTH_EN_MASK		(0x1)
#define QCN9224_OEM_ID_MASK				GENMASK(31,16)
#define QCN9224_OEM_ID_SHIFT				16
#define QCN9224_MODEL_ID_MASK				GENMASK(15,0)
#define QCN9224_ANTI_ROLL_BACK_FEATURE_EN_MASK		BIT(9)
#define QCN9224_ANTI_ROLL_BACK_FEATURE_EN_SHIFT		9
#define QCN9224_TOTAL_ROT_NUM_MASK			GENMASK(13,12)
#define QCN9224_TOTAL_ROT_NUM_SHIFT			12
#define QCN9224_ROT_REVOCATION_MASK			GENMASK(17,14)
#define QCN9224_ROT_REVOCATION_SHIFT			14
#define QCN9224_ROT_ACTIVATION_MASK			GENMASK(21,18)
#define QCN9224_ROT_ACTIVATION_SHIFT			18
#define QCN9224_OEM_PK_HASH_SIZE			36
#define QCN9224_JTAG_ID					0x01e22b3c
#define QCN9224_SERIAL_NUM				0x01e22b40
#define QCN9224_PART_TYPE_EXTERNAL			0x01f94090
#define QCN9224_PART_TYPE_EXTERNAL_MASK			BIT(3)
#define QCN9224_PART_TYPE_EXTERNAL_SHIFT		3

#define MHICTRL						(0x38)
#define BHI_STATUS					(0x12C)
#define BHI_IMGADDR_LOW					(0x108)
#define BHI_IMGADDR_HIGH				(0x10C)
#define BHI_IMGSIZE					(0x110)
#define BHI_ERRCODE					(0x130)
#define BHI_ERRDBG1					(0x134)
#define BHI_ERRDBG2					(0x138)
#define BHI_ERRDBG3					(0x13C)
#define BHI_IMGTXDB					(0x118)
#define BHI_EXECENV					(0x128)

#define MHICTRL_RESET_MASK				(0x2)
#define BHI_STATUS_MASK					(0xC0000000)
#define BHI_STATUS_SHIFT				(30)
#define BHI_STATUS_SUCCESS				(2)

#define NO_MASK						(0xFFFFFFFF)

#ifdef CONFIG_SYS_MAXARGS
#define MAX_BOOT_ARGS_SIZE	CONFIG_SYS_MAXARGS
#elif
#define MAX_BOOT_ARGS_SIZE	64
#endif

/*
 * Function declaration
 */
unsigned int get_which_flash_param(char *part_name);
int get_current_board_flash_config(int flash_type);
ipq_smem_flash_info_t * get_ipq_smem_flash_info(void);
socinfo_t * get_socinfo(void);
uint32_t get_part_block_size(struct smem_ptn *p, ipq_smem_flash_info_t *sfi);
void *smem_get_item(unsigned int item);
struct smem_ptable * get_ipq_part_table_info(void);
int getpart_offset_size(char *part_name, uint32_t *offset, uint32_t *size);
int smem_getpart_from_offset(uint32_t offset, uint32_t *start, uint32_t *size);
int get_rootfs_active_partition(ipq_smem_flash_info_t *sfi);
int mibib_ptable_init(unsigned int* addr);
void get_kernel_fs_part_details(int flash_type);
void parse_fdt_fixup(char* buf, void *blob);
#if defined(CONFIG_MMC) || defined(CONFIG_NOR_BLK)
int ipq_part_get_info_by_name(blkpart_info_t *blkpart);
gpt_entry* get_gpt_entry(struct blk_desc *dev_desc);
#endif
#ifdef CONFIG_CMD_UBI
int init_ubi_part(void);
#endif
void fdt_fixup_flash(void *blob);
void reset_crashdump(int reset_version);
long long ubi_get_volume_size(char *volume);
bool is_atf_enbled(void);
#ifdef CONFIG_SCM_V1
bool is_secure_boot_v1(void);
#elif CONFIG_SCM_V2
bool is_secure_boot_v2(void);
#else
bool is_secure_boot_fake(void);
#endif
uint8_t * get_boot_mode(void);
#ifdef CONFIG_CMD_NAND
void board_nand_init(void);
int ipq_get_training_part_info(uint32_t *offset, uint32_t *size);
#endif
int get_partition_data(char *part_name, uint32_t offset, uint8_t* buf,
			size_t size, uint32_t fl_type);
int bring_secondary_core_up(unsigned long cpuid, unsigned long entry,
				unsigned long arg);
void bring_secondary_core_down(unsigned long state);
int is_secondary_core_off(unsigned long cpuid);
uint64_t smem_get_flash_size(uint8_t flash_type);
bool is_smem_part_exceed_flash_size(struct smem_ptn *p, uint64_t psize);
struct spi_flash *ipq_spi_probe(void);
#ifdef CONFIG_DPR_VER_1_0
int execute_dprv1(struct cmd_tbl *cmdtp, int flag, int argc,
							char *const argv[]);
#elif CONFIG_DPR_VER_2_0
int execute_dprv2(struct cmd_tbl *cmdtp, int flag, int argc,
							char *const argv[]);
#elif CONFIG_DPR_VER_3_0
int execute_dprv3(struct cmd_tbl *cmdtp, int flag, int argc,
							char *const argv[]);
#endif
uint32_t cal_bootconf_crc(ipq_smem_bootconfig_info_t *binfo);
uint32_t crc32_be(uint8_t const *addr, phys_size_t size);
uint8_t is_valid_bootconfig(ipq_smem_bootconfig_info_t *binfo);
int read_bootconfig(void);
int ipq_read_tcsr_boot_misc(void);
int write_tcsr_boot_misc_reg(uint32_t mask, uint32_t value);

#ifdef CONFIG_GPIO_CONFIG
void ipq_board_gpio_config(int type);
#endif
uint32_t image_auth_check(void);
int check_rootfs_authentication(void);
#ifdef CONFIG_WDT
int ipq_wdt_expire(void);
#endif
int ipq_gpt_getpart_from_offset(uint32_t offset, uint32_t *pstart,
					uint32_t *psize, uint32_t flash_type);
#endif
#ifdef CONFIG_VERSION_ROLLBACK_PARTITION_INFO
bool is_version_rollback_support(void);
#endif
