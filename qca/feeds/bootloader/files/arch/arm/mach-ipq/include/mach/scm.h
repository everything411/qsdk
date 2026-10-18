/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2019 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IPQ_SCM_H
#define __IPQ_SCM_H

#include <linux/errno.h>

#define MAX_QCOM_SCM_ARGS	10
#define MAX_QCOM_SCM_RETS	3

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			(((a) & 0x3) << 4) | \
			(((b) & 0x3) << 6) | \
			(((c) & 0x3) << 8) | \
			(((d) & 0x3) << 10) | \
			(((e) & 0x3) << 12) | \
			(((f) & 0x3) << 14) | \
			(((g) & 0x3) << 16) | \
			(((h) & 0x3) << 18) | \
			(((i) & 0x3) << 20) | \
			(((j) & 0x3) << 22) | \
			((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

struct qcom_scm_desc {
	uint64_t args[MAX_QCOM_SCM_ARGS];
	uint32_t svc;
	uint32_t cmd;
	uint32_t arginfo;
	uint32_t owner;
};

/**
 * struct arm_smccc_args
 * @args:	The array of values used in registers in smc instruction
 */
struct arm_smccc_args {
	unsigned long args[8];
};

/**
 * struct qcom_scm_res
 * @result:     The values returned by the secure syscall
 */
struct qcom_scm_res {
	uint64_t result[MAX_QCOM_SCM_RETS];
};

#define SCM_SIP_FNID(s, c) (((((s) & 0xFF) << 8) | \
				((c) & 0xFF)) | 0x02000000)

#define SCM_SMC_FNID(s, c)      ((((s) & 0xFF) << 8) | ((c) & 0xFF))
#define scm_smc_call(desc, res, atomic) \
	__scm_smc_call((desc), qcom_scm_convention, (res), (atomic))

#define SCM_SMC_N_REG_ARGS	4
#define SCM_SMC_FIRST_EXT_IDX	(SCM_SMC_N_REG_ARGS - 1)
#define SCM_SMC_N_EXT_ARGS	(MAX_QCOM_SCM_ARGS - SCM_SMC_N_REG_ARGS + 1)
#define SCM_SMC_FIRST_REG_IDX	2
#define SCM_SMC_LAST_REG_IDX	(SCM_SMC_FIRST_REG_IDX + SCM_SMC_N_REG_ARGS - 1)

/* common error codes */
#define QCOM_SCM_V2_EBUSY	-12
#define QCOM_SCM_ENOMEM		-5
#define QCOM_SCM_EOPNOTSUPP	-4
#define QCOM_SCM_EINVAL_ADDR	-3
#define QCOM_SCM_EINVAL_ARG	-2
#define QCOM_SCM_ERROR		-1
#define QCOM_SCM_INTERRUPTED	 1

/* SVC & CMD IDs */
#define QCOM_SCM_SVC_BOOT		0x01
#define QCOM_SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID	0x9
#define QCOM_SCM_EL1SWITCH_ARCH64	0xf
#define QCOM_KERNEL_AUTH_CMD		0x1E
#define QCOM_SCM_SEC_AUTH_CMD		0x1F
#define QCOM_PART_INFO_CMD		0x22
#define QCOM_ROOTFS_HASH_VERIFY_CMD	0x23

#define QCOM_SCM_SVC_INFO		0x06
#define QCOM_SCM_INFO_IS_CALL_AVAIL     0x01
#define QCOM_GET_SECURE_STATE_CMD	0x04

#define QCOM_SCM_SVC_IO			0x05
#define QCOM_SCM_IO_READ		0x01
#define QCOM_SCM_IO_WRITE		0x02

#define QCOM_CHECK_FEATURE_CMD		0x03

#define QCOM_SCM_SVC_FUSE		0x08
#define QCOM_QFPROM_IS_AUTHENTICATE_CMD	0x07
#define QCOM_TZ_BLOW_FUSE_SECDAT_CMD	0x20
#define QCOM_AUTH_FUSE_UIE_KEY_CMD	0x23
#define QCOM_TME_DPR_PROCESSING		0x21
#define QCOM_TZ_READ_FUSE_VALUE_CMD	0x22

#define QCOM_SCM_PHYA0_SVC_ID		0x02
#define QCOM_SCM_PHYA0_READ_CMD		0x22
#define QCOM_SCM_PHYA0_WRITE_CMD	0x23
#define QCOM_SCM_SVC_APP_MGR		0x01	/* Application service manager */
#define QCOM_REGISTER_LOG_BUFFER_ID_CMD	0x06
#define QCOM_REGION_NOTIFICATION_ID_CMD	0x05

#define QCOM_SCM_SVC_SEC_TEST_1		253	/* Secure test calls (continued). */
#define QCOM_SCM_SEC_TEST_ID		0x2C

#define QCOM_SCM_SVC_EXTERNAL		0x03	/* External Image loading */
#define QCOM_LOAD_TZTESTEXEC_IMG_ID_CMD	0x00

#define QCOM_SCM_CMD_AES_256_ENC	0x07
#define QCOM_SCM_CMD_AES_256_DEC	0x08
#define QCOM_SCM_CMD_AES_256_GEN_KEY	0x09
#define QCOM_SCM_CMD_AES_256_MAX_CTXT_GEN_KEY	0x0E
#define QCOM_SCM_SVC_CRYPTO		0x0A
#define QCOM_SCM_CMD_AES_CLEAR_KEY	0x0A

/* scm_arg*/
#define SCM_VAL				0x00
#define SCM_READ_OP			0x01
#define SCM_WRITE_OP			0x02

#define QCOM_SVC_ICE			23
#define QCOM_SCM_ICE_CMD                0x1
#define QCOM_SCM_ICE_CONTEXT_CMD	0x3

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

#define UNUSED_VAR(x)		((void)x)
/*
 * Authenticate kernel image during bootup
 */

#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_AUTHENTICATE_KERNEL_V1(_param, _a, ...)		\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_KERNEL_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#elif IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_AUTHENTICATE_KERNEL_V2(_param, _a, _b, _c, _d, _e)	\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		(_param).buff[3] = _d;					\
		(_param).buff[4] = _e;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_AUTHENTICATE_KERNEL(...) break
#endif

/*
 * Authenticate Rootfs during bootup
 * as well as to authenticate signed image
 * of all the sub-systems
 */

#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_SECURE_AUTHENTICATE_V1(_param, _a, _b, _c, _d, _e)	\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		UNUSED_VAR(_e);						\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_WRITE_OP;			\
		(_param).len = 3;					\
	} while (0)
#elif IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_SECURE_AUTHENTICATE_V2(_param, _a, _b, _c, _d, _e)	\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _c;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _a;					\
		(_param).buff[3] = _d;					\
		(_param).buff[4] = _e;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_SECURE_AUTHENTICATE(...) break
#endif

/*
 * Helps to read fuse valuse
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_READ_FUSE_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_LIST_FUSE;				\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_READ_FUSE(...) break
#endif

/*
 * verify hash value with meta data
 */

#if IS_ENABLED(CONFIG_SCM_V2)
#define _IPQ_SCM_VERIFY_HASH_V1(_param, _a, _b, _c, _d, _e)		\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_ROOTFS_HASH_VERIFY;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		(_param).buff[3] = _d;					\
		(_param).buff[4] = _e;				\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_WRITE_OP;			\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_WRITE_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_VERIFY_HASH(...) break
#endif

/*
 * check for secure boot
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_SECURE_BOOT_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_CHECK_SECURE_FUSE;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_READ_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_SECURE_BOOT(...) break
#endif

/*
 * Set active partition for image version anti roll-back
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_SET_ACTIVE_PARTITION_V1(_param, _a)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SET_ACTIVE_PART;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_SET_ACTIVE_PARTITION(...) break
#endif

/*
 * blow fuse
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_FUSE_IPQ_V1(_param, _a, _b, _c, _d, _e)		\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
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
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		(_param).buff[3] = _d;					\
		(_param).buff[4] = _e;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_FUSE_IPQ(...) break
#endif

/*
 * XPU secure test
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_XPU_SEC_TEST_1_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_XPU_SEC_TEST_1;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_XPU_SEC_TEST_1(...) break
#endif

/*
 * XPU log buffer
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_XPU_LOG_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_XPU_LOG_BUFFER;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_XPU_LOG(...) break
#endif

/*
 * TZT region notification
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_TZT_REGION_NOTIFY_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_TZT_REGION_NOTIFICATION;		\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_TZT_REGION_NOTIFY(...) break
#endif

/*
 * TZT execute image
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_TZT_EXEC_IMG_V1(_param, _a, _b, _c)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
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
#define _IPQ_SCM_TZT_EXEC_IMG(...) break
#endif

/*
 * Generate AES_256 Key
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_GENERATE_AES_256_KEY_V1(_param, _a, _b)		\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_AES_256_GEN_KEY;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_GENERATE_AES_256_KEY(...) break
#endif

/*
 * Generate AES_256 Key with max 128 bytes context
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX_V1(_param, _a, _b)	\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_AES_256_MAX_CTXT_GEN_KEY;		\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_GENERATE_AES_256_KEY_128B_CNTX(...) break
#endif

/*
 * Encrypt AES_256
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_ENCRYPT_AES_256_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_AES_256_ENC;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_ENCRYPT_AES_256(...) break
#endif

/*
 * Decrypt AES_256
 */
#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define _IPQ_SCM_DECRYPT_AES_256_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_AES_256_DEC;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_WRITE_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_DECRYPT_AES_256(...) break
#endif

/*
 * blow fuse
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_CHECK_SCM_SUPPORT_V1(_param, _a)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_CHECK_SUPPORT;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_CHECK_SCM_SUPPORT(...) break
#endif

/*
 * Enable SDI path
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_ENABLE_SDI_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SDI_CLEAR;				\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_ENABLE_SDI(...) break
#endif

/*
 * I/O write
 */
#ifdef CONFIG_SCM
#define _IPQ_SCM_IO_WRITE_V1(_param, _a, _b)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_IO_WRITE;				\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_IO_WRITE(...) break
#endif

/*
 * I/O read
 */
#if CONFIG_SCM
#define _IPQ_SCM_IO_READ_V1(_param, _a)					\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_IO_READ;				\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_IO_READ(...) break
#endif

/*
 * Read PHYA0 region
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_READ_PHY_REG_V1(_param, _a)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_PHYA0_REGION_RD;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#else
#define _IPQ_SCM_READ_PHY_REG(...) break
#endif

/*
 * Write PHYA0 region
 */
#if IS_ENABLED(CONFIG_SCM_V1)
#define _IPQ_SCM_WRITE_PHY_REG_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_PHYA0_REGION_WR;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_WRITE_PHY_REG(...) break
#endif

/*
 * Execute DPR
 */
#if defined(CONFIG_SCM_V1) && defined(CONFIG_DPR_VER_1_0)
#define _IPQ_SCM_EXECUTE_DPR_V1(_param, _a, ...)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_TME_DPR_PROCESSING;			\
		(_param).buff[0] = _a;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).len = 1;					\
	} while (0)
#elif defined(CONFIG_SCM_V1) && defined(CONFIG_DPR_VER_2_0)
#define _IPQ_SCM_EXECUTE_DPR_V2(_param, _a, _b, ...)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
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
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_SECURE_AUTH;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		(_param).buff[3] = _d;					\
		(_param).buff[4] = _e;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).len = 5;					\
	} while (0)
#else
#define _IPQ_SCM_EXECUTE_DPR(...) break
#endif

/*
 * Configure ICE
 */
#if IS_ENABLED(CONFIG_SCM)
#define _IPQ_SCM_ICE_CONFIGURE_V1(_param, _a, _b)			\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type  = SCM_ICE_CONFIGURE;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).arg_type[0] = SCM_READ_OP;			\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).len = 2;					\
	} while (0)
#else
#define _IPQ_SCM_ICE_CONFIGURE(...) break;
#endif

#if IS_ENABLED(CONFIG_SCM)
#define _IPQ_SCM_ICE_KEY_CONFIGURE_V1(_param, _a, _b, _c, _d, _e, _f, _g) \
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type  = SCM_ICE_KEY_CONFIGURE;			\
		(_param).buff[0] = _a;					\
		(_param).buff[1] = _b;					\
		(_param).buff[2] = _c;					\
		(_param).buff[3] = _d;					\
		(_param).buff[4] = _e;					\
		(_param).buff[5] = _f;					\
		(_param).buff[6] = _g;					\
		(_param).arg_type[0] = SCM_VAL;				\
		(_param).arg_type[1] = SCM_VAL;				\
		(_param).arg_type[2] = SCM_VAL;				\
		(_param).arg_type[3] = SCM_READ_OP;			\
		(_param).arg_type[4] = SCM_VAL;				\
		(_param).arg_type[5] = SCM_READ_OP;			\
		(_param).arg_type[6] = SCM_VAL;				\
		(_param).len = 7;					\
	} while (0)
#else
#define _IPQ_SCM_ICE_CONFIGURE(...) break;
#endif
/*
 * Check ATF support
 */
#if defined(CONFIG_SCM_V1)
#define _check_atf_support_V1(_param)					\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_CHECK_ATF_SUPPORT;			\
	} while (0)
#else
#define _check_atf_support(...)	break
#endif

#ifdef CONFIG_SCM
#define _CHECK_FEATURE_V1(_param, _a)					\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
		(_param).type = SCM_CHECK_FEATURE_ID;			\
		(_param).buff[0] = _a;					\
		(_param).len = 1;					\
	} while (0)
#else
#define _CHECK_FEATURE(...) break
#endif

#if IS_ENABLED(CONFIG_SCM) && IS_ENABLED(CONFIG_CMD_AES_256)
#define	_IPQ_SCM_CLEAR_AES_KEY_V1(_param, _a)				\
	do {								\
		memset(&(_param), 0, sizeof(struct scm_param));		\
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
#define	IPQ_SCM_CLEAR_AES_KEY(...)	break
#endif

#ifdef CONFIG_SCM
#define IPQ_SCM_ICE_CONFIGURE(param, a, b) _IPQ_SCM_ICE_CONFIGURE_V1(param, a, b)
#else
#define IPQ_SCM_ICE_CONFIGURE(...)      break;
#endif

#ifdef CONFIG_SCM
#define IPQ_SCM_ICE_KEY_CONFIGURE(param, a, b, c, d, e, f, g)		\
		 _IPQ_SCM_ICE_KEY_CONFIGURE_V1(param, a, b, c, d, e, f, g)
#else
#define IPQ_SCM_ICE_KEY_CONFIGURE(...)      break;
#endif

static inline int qcom_scm_remap_error(int err)
{
	switch (err) {
	case QCOM_SCM_ERROR:
		return -EIO;
	case QCOM_SCM_EINVAL_ADDR:
		fallthrough;
	case QCOM_SCM_EINVAL_ARG:
		return -EINVAL;
	case QCOM_SCM_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case QCOM_SCM_ENOMEM:
		return -ENOMEM;
	case QCOM_SCM_V2_EBUSY:
		return -EBUSY;
	}
	return -EINVAL;
}

enum scm_type {
	SCM_IO_WRITE = 0,
	SCM_IO_READ,
	SCM_SDI_CLEAR,
	SCM_DLODE,
	SCM_CHECK_SUPPORT,
	SCM_SECURE_AUTH,
	SCM_KERNEL_AUTH,
	SCM_CHECK_SECURE_FUSE,
	SCM_SET_ACTIVE_PART,
	SCM_CHECK_ATF_SUPPORT,
	SCM_FUSE_IPQ,
	SCM_FUSE_IPQ_UIE_KEY,
	SCM_LIST_FUSE,
	SCM_TME_DPR_PROCESSING,
	SCM_PHYA0_REGION_WR,
	SCM_PHYA0_REGION_RD,
	SCM_XPU_LOG_BUFFER,
	SCM_XPU_SEC_TEST_1,
	SCM_TZT_REGION_NOTIFICATION,
	SCM_TZT_TESTEXEC_IMG,
	SCM_AES_256_GEN_KEY,
	SCM_AES_256_MAX_CTXT_GEN_KEY,
	SCM_AES_256_ENC,
	SCM_AES_256_DEC,
	SCM_ROOTFS_HASH_VERIFY,
	SCM_CHECK_FEATURE_ID,
	SCM_CLEAR_AES_KEY,
	SCM_ICE_CONFIGURE,
	SCM_ICE_KEY_CONFIGURE
};

struct scm_param {
	struct qcom_scm_res res;
	uint64_t buff[MAX_QCOM_SCM_ARGS];
	uint32_t svc_id;
	uint32_t cmd_id;
	uint32_t len;
	uint8_t arg_type[MAX_QCOM_SCM_ARGS];
	bool get_ret;
	enum scm_type type;
};

#ifdef CONFIG_IPQ_INLINE_ENCRYPTION
int qcom_ice_init_crashdump(void);
#endif

int hex_string_to_binary(const char *hex_str, uint8_t *binary_data,
			 size_t binary_len);
void generate_random_context(uint8_t *context, size_t context_len);

/* ICE SCM implementation functions */
int ipq_ice_configure_scm_impl(void *params);
int ipq_ice_key_configure_scm_impl(void *params);

int qca_scm_sdi(void);
int qca_scm_dload(uintptr_t tcsr_addr, u32 magic_cookie);
int ipq_scm_call(struct scm_param *param);

#endif

int ipq_list_fuse_scm_impl(void *params);
int ipq_dump_fuse_scm_impl(void *params);
int ipq_check_secure_boot_scm_impl(void *params);
int ipq_secure_auth_scm_impl(void *params);
int ipq_aes_256_enc_scm_impl(void *params);
int ipq_aes_256_dec_scm_impl(void *params);
int ipq_aes_derive_key_scm_impl(void *params);
int ipq_aes_derive_key_max_ctxt_scm_impl(void *params);
int ipq_aes_clear_key_scm_impl(void *params);
