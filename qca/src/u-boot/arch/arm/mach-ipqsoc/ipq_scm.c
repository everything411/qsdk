// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010,2015,2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <ubi_uboot.h>
#include <linux/stddef.h>
#include <linux/compat.h>
#include <linux/dma-mapping.h>
#include <linux/arm-smccc.h>
#include <errno.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <hang.h>
#include <mach/ipq_scm.h>
#include <common.h>
#include <asm/system.h>
#include <malloc.h>
#include <memalign.h>

#ifdef DEBUG
#define debugf(fmt, args...)						\
	do { printf("%s(): ", __func__); printf(fmt, ##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

#define ARG_LEN_MASK	0xF

enum qcom_scm_convention {
	SMC_CONVENTION_UNKNOWN,
	SMC_CONVENTION_LEGACY,
	SMC_CONVENTION_ARM_32,
	SMC_CONVENTION_ARM_64,
};

enum qcom_scm_convention qcom_scm_convention
__attribute__ ((section(".data"))) = SMC_CONVENTION_UNKNOWN;

#ifdef DEBUG
static const char * const qcom_scm_convention_names[] = {
        [SMC_CONVENTION_UNKNOWN] = "unknown",
        [SMC_CONVENTION_ARM_32] = "smc arm 32",
        [SMC_CONVENTION_ARM_64] = "smc arm 64",
        [SMC_CONVENTION_LEGACY] = "smc legacy",
};
#endif

static void __scm_smc_do_quirk(const struct arm_smccc_args *smc,
				struct arm_smccc_res *res)
{
	unsigned long a0 = smc->args[0];
	struct arm_smccc_quirk quirk = { .id = ARM_SMCCC_QUIRK_QCOM_A6 };

	quirk.state.a6 = 0;

	do {
		arm_smccc_smc_quirk(a0, smc->args[1], smc->args[2],
					smc->args[3], smc->args[4],
					smc->args[5], quirk.state.a6,
					smc->args[7], res, &quirk);

		if (res->a0 == QCOM_SCM_INTERRUPTED)
			a0 = res->a0;

	} while (res->a0 == QCOM_SCM_INTERRUPTED);
}

int __scm_smc_call(const struct qcom_scm_desc *desc,
		   enum qcom_scm_convention qcom_convention,
		   struct qcom_scm_res *res, bool atomic)
{
	int arglen = desc->arginfo & ARG_LEN_MASK;
	void *args_phys = NULL;
	size_t alloc_len;
	int i;
	u32 smccc_call_type = atomic ? ARM_SMCCC_FAST_CALL : ARM_SMCCC_STD_CALL;
	u32 qcom_smccc_convention = (qcom_convention == SMC_CONVENTION_ARM_32) ?
				    ARM_SMCCC_SMC_32 : ARM_SMCCC_SMC_64;
	struct arm_smccc_res smc_res;
	struct arm_smccc_args smc = {0};

	smc.args[0] = ARM_SMCCC_CALL_VAL(
		smccc_call_type,
		qcom_smccc_convention,
		desc->owner,
		SCM_SMC_FNID(desc->svc, desc->cmd));
	smc.args[1] = desc->arginfo;

	for (i = 0; i < SCM_SMC_N_REG_ARGS; i++)
		smc.args[i + SCM_SMC_FIRST_REG_IDX] = desc->args[i];

	if (unlikely(arglen > SCM_SMC_N_REG_ARGS)) {
		alloc_len = roundup(SCM_SMC_N_EXT_ARGS * sizeof(u64),
					CONFIG_SYS_CACHELINE_SIZE);
		args_phys = malloc_cache_aligned(alloc_len);

		if (!args_phys)
			return -ENOMEM;
		memset(args_phys, 0, alloc_len);

		if (qcom_smccc_convention == ARM_SMCCC_SMC_32) {
			__le32 *args = args_phys;

			for (i = 0; i < SCM_SMC_N_EXT_ARGS; i++)
				args[i] = cpu_to_le32(desc->args[i +
						      SCM_SMC_FIRST_EXT_IDX]);
		} else {
			__le64 *args = args_phys;

			for (i = 0; i < SCM_SMC_N_EXT_ARGS; i++)
				args[i] = cpu_to_le64(desc->args[i +
						      SCM_SMC_FIRST_EXT_IDX]);
		}
#if !defined(CONFIG_SYS_DCACHE_OFF)
		flush_dcache_range((uintptr_t)args_phys,
					(uintptr_t)args_phys +
					alloc_len);
#endif

		smc.args[SCM_SMC_LAST_REG_IDX] = (uintptr_t)args_phys;
	}

	__scm_smc_do_quirk(&smc, &smc_res);

	if (res) {
		res->result[0] = smc_res.a1;
		res->result[1] = smc_res.a2;
		res->result[2] = smc_res.a3;
	}

	if(args_phys)
		free(args_phys);

	return (long)smc_res.a0 ? qcom_scm_remap_error(smc_res.a0) : 0;
}

static enum qcom_scm_convention __get_convention(void)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.args[0] = SCM_SMC_FNID(QCOM_SCM_SVC_INFO,
					   QCOM_SCM_INFO_IS_CALL_AVAIL) |
			   (ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT),
		.arginfo = QCOM_SCM_ARGS(1),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	enum qcom_scm_convention probed_convention;
	int ret;
#ifdef DEBUG
	bool forced = false;
#endif
	if (likely(qcom_scm_convention != SMC_CONVENTION_UNKNOWN))
		return qcom_scm_convention;

	/*
	 * When running on 32bit kernel, SCM call with convention
	 * SMC_CONVENTION_ARM_64 is causing the system crash. To avoid that
	 * use SMC_CONVENTION_ARM_64 for 64bit kernel and SMC_CONVENTION_ARM_32
	 * for 32bit kernel.
	 */
#if IS_ENABLED(CONFIG_ARM64)
	/*
	 * Device isn't required as there is only one argument - no device
	 * needed to dma_map_single to secure world
	 */
	probed_convention = SMC_CONVENTION_ARM_64;
	ret = __scm_smc_call(&desc, probed_convention, &res, true);
	if (!ret && res.result[0] == 1)
		goto found;
#endif

	probed_convention = SMC_CONVENTION_ARM_32;
	ret = __scm_smc_call(&desc, probed_convention, &res, true);
	if (!ret && res.result[0] == 1)
		goto found;

	probed_convention = SMC_CONVENTION_LEGACY;
found:
	if (probed_convention != qcom_scm_convention) {
		qcom_scm_convention = probed_convention;
		debugf("qcom_scm: convention: %s%s\n",
			qcom_scm_convention_names[qcom_scm_convention],
			forced ? " (forced)" : "");
	}

	return qcom_scm_convention;
}

/**
 * qcom_scm_call() - qcom_scm_call()
 * @desc:       Descriptor structure containing arguments and return values
 * @res:        Structure containing results from SMC/HVC call
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This can be called in atomic context.
 */
static int qcom_scm_call(const struct qcom_scm_desc *desc,
                                struct qcom_scm_res *res)
{
	switch (__get_convention()) {
		case SMC_CONVENTION_ARM_32:
		case SMC_CONVENTION_ARM_64:
			return scm_smc_call(desc, res, true);
		case SMC_CONVENTION_LEGACY:
		default:
			pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}

int ipq_scm_call(scm_param *param)
{

	struct qcom_scm_desc desc = {0};
	struct qcom_scm_res res;
	int ret;

	desc.owner = ARM_SMCCC_OWNER_SIP;

	for(int i = 0; i < param->len; i++)
		desc.args[i] = param->buff[i];

	for(int i = 0; i < MAX_QCOM_SCM_ARGS; i++) {
		/* Every 2 bits from 4th bit of the arginfo
		 * represents the type if argument
		 */
		desc.arginfo |= (param->arg_type[i] & 0x3) << ((i*2)+4);
	}

	/* Least significant Nibble represents the number of arguments */
	desc.arginfo |= param->len & 0xf;

	switch(param->type) {
	case SCM_IO_WRITE:
		desc.svc = QCOM_SCM_SVC_IO;
		desc.cmd = QCOM_SCM_IO_WRITE;
		break;
	case SCM_IO_READ:
		desc.svc = QCOM_SCM_SVC_IO;
		desc.cmd = QCOM_SCM_IO_READ;
		break;
	case SCM_SDI_CLEAR:
		desc.svc = QCOM_SCM_SVC_BOOT;
		desc.cmd = QCOM_SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID;
		break;
	case SCM_CHECK_SUPPORT:
		desc.svc = QCOM_SCM_SVC_INFO;
		desc.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL;
		break;
	case SCM_SET_ACTIVE_PART:
		desc.svc = QCOM_SCM_SVC_BOOT;
		desc.cmd = QCOM_PART_INFO_CMD;
		break;
	case SCM_CHECK_ATF_SUPPORT:
		desc.svc = QCOM_SCM_SVC_INFO;
		desc.cmd = QCOM_GET_SECURE_STATE_CMD;
		break;
	case SCM_TME_DPR_PROCESSING:
		desc.svc = QCOM_SCM_SVC_FUSE;
		desc.cmd = QCOM_TME_DPR_PROCESSING;
		break;
	case SCM_XPU_LOG_BUFFER:
		desc.svc = QCOM_SCM_SVC_APP_MGR;
		desc.cmd = QCOM_REGISTER_LOG_BUFFER_ID_CMD;
		desc.owner = ARM_SMCCC_OWNER_TRUSTED_OS;
		break;
	case SCM_XPU_SEC_TEST_1:
		desc.svc = QCOM_SCM_SVC_SEC_TEST_1;
		desc.cmd = QCOM_SCM_SEC_TEST_ID;
		break;
	case SCM_TZT_REGION_NOTIFICATION:
		desc.svc = QCOM_SCM_SVC_APP_MGR;
		desc.cmd = QCOM_REGION_NOTIFICATION_ID_CMD;
		desc.owner = ARM_SMCCC_OWNER_TRUSTED_OS;
		break;
	case SCM_TZT_TESTEXEC_IMG:
		desc.svc = QCOM_SCM_SVC_EXTERNAL;
		desc.cmd = QCOM_LOAD_TZTESTEXEC_IMG_ID_CMD;
		desc.owner = ARM_SMCCC_OWNER_TRUSTED_OS;
		break;
	case SCM_PHYA0_REGION_WR:
		desc.svc = QCOM_SCM_PHYA0_SVC_ID;
		desc.cmd = QCOM_SCM_PHYA0_WRITE_CMD;
		break;
	case SCM_PHYA0_REGION_RD:
		desc.svc = QCOM_SCM_PHYA0_SVC_ID;
		desc.cmd = QCOM_SCM_PHYA0_READ_CMD;
		break;
	case SCM_AES_256_GEN_KEY:
		desc.svc = QCOM_SCM_SVC_CRYPTO;
		desc.cmd = QCOM_SCM_CMD_AES_256_GEN_KEY;
		break;
	case SCM_AES_256_MAX_CTXT_GEN_KEY:
		desc.svc = QCOM_SCM_SVC_CRYPTO;
		desc.cmd = QCOM_SCM_CMD_AES_256_MAX_CTXT_GEN_KEY;
		break;
	case SCM_AES_256_ENC:
		desc.svc = QCOM_SCM_SVC_CRYPTO;
		desc.cmd = QCOM_SCM_CMD_AES_256_ENC;
		break;
	case SCM_AES_256_DEC:
		desc.svc = QCOM_SCM_SVC_CRYPTO;
		desc.cmd = QCOM_SCM_CMD_AES_256_DEC;
		break;
	case SCM_ROOTFS_HASH_VERIFY:
		desc.svc = QCOM_SCM_SVC_BOOT;
		desc.cmd = QCOM_ROOTFS_HASH_VERIFY_CMD;
		break;
#ifdef CONFIG_IPQ_SECURE
	case SCM_KERNEL_AUTH:
		desc.svc = QCOM_SCM_SVC_BOOT;
		desc.cmd = QCOM_KERNEL_AUTH_CMD;
		break;
	case SCM_SECURE_AUTH:
		desc.svc = QCOM_SCM_SVC_BOOT;
		desc.cmd = QCOM_SCM_SEC_AUTH_CMD;
		break;
	case SCM_CHECK_SECURE_FUSE:
		desc.svc = QCOM_SCM_SVC_FUSE;
		desc.cmd = QCOM_QFPROM_IS_AUTHENTICATE_CMD;
		break;
	case SCM_LIST_FUSE:
		desc.svc = QCOM_SCM_SVC_FUSE;
		desc.cmd = QCOM_TZ_READ_FUSE_VALUE_CMD;
		break;
	case SCM_FUSE_IPQ:
		desc.svc = QCOM_SCM_SVC_FUSE;
		desc.cmd = QCOM_TZ_BLOW_FUSE_SECDAT_CMD;
		break;
	case SCM_FUSE_IPQ_UIE_KEY:
		desc.svc = QCOM_SCM_SVC_FUSE;
		desc.cmd = QCOM_AUTH_FUSE_UIE_KEY_CMD;
		break;
#endif
	case SCM_CHECK_FEATURE_ID:
		desc.svc = QCOM_SCM_SVC_INFO;
		desc.cmd = QCOM_CHECK_FEATURE_CMD;
		break;
	case SCM_CLEAR_AES_KEY:
		desc.svc = QCOM_SCM_SVC_CRYPTO;
		desc.cmd = QCOM_SCM_CMD_AES_CLEAR_KEY;
		break;
	default:
		printf("Invalid call ID: %d\n", param->type);
		ret = -EINVAL;
		break;
	}

	/*
	 *flush dcache
	 */
	flush_dcache_all();

	ret = qcom_scm_call(&desc, param->get_ret ? &res : NULL);

	if(param->get_ret)
	{
		param->res.result[0] =  res.result[0];
		param->res.result[1] =  res.result[1];
		param->res.result[2] =  res.result[2];
	}

	return ret;

}
