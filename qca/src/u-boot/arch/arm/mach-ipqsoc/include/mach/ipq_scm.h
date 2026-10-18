/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015,2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#define QCOM_SCM_SVC_INFO               0x06
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

static inline int qcom_scm_remap_error(int err)
{
switch (err) {
	case QCOM_SCM_ERROR:
		return -EIO;
	case QCOM_SCM_EINVAL_ADDR:
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
	SCM_CLEAR_AES_KEY
};

typedef struct {
	struct qcom_scm_res res;
	uint64_t buff[MAX_QCOM_SCM_ARGS];
	uint32_t svc_id;
	uint32_t cmd_id;
	uint32_t len;
	uint8_t arg_type[MAX_QCOM_SCM_ARGS];
	bool get_ret;
	enum scm_type type;
}scm_param;

int qca_scm_sdi(void);
int qca_scm_dload(uintptr_t tcsr_addr, u32 magic_cookie);
int ipq_scm_call(scm_param *param);

#endif
