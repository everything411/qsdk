// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010,2015,2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/dma-mapping.h>
#include <linux/interconnect.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/firmware/qcom/qcom_pas.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/reset-controller.h>
#include <linux/arm-smccc.h>
#include <linux/soc/qcom/smem.h>

#include "qcom_pas.h"
#include "qcom_scm.h"

#define ICE_CRYPTO_AES_XTS_MODE	0x3

#define SDI_DISABLE		BIT(0)
#define ABNORMAL_MAGIC		BIT(1)

#define SMEM_TRYMODE_INFO	507
#define BOOT_INFO_USE_SET_B	BIT(31)

#define DRIVER_NAME "qcom_scm"

static bool download_mode = IS_ENABLED(CONFIG_QCOM_SCM_DOWNLOAD_MODE_DEFAULT);
module_param(download_mode, bool, 0);

struct qcom_scm {
	struct device *dev;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct clk *bus_clk;
	struct icc_path *path;
	struct completion waitq_comp;
	struct reset_controller_dev reset;

	/* control access to the interconnect path */
	struct mutex scm_bw_lock;
	int scm_vote_count;

	u64 dload_mode_addr;
	u64 tcsr_boot_info_addr;
	u32 hvc_log_cmd_id;
	u32 smmu_state_cmd_id;
	/* Atomic context only */
	spinlock_t lock;
};

struct qcom_scm_current_perm_info {
	__le32 vmid;
	__le32 perm;
	__le64 ctx;
	__le32 ctx_size;
	__le32 unused;
};

struct qcom_scm_mem_map_info {
	__le64 mem_addr;
	__le64 mem_size;
};

/* Each bit configures cold/warm boot address for one of the 4 CPUs */
static const u8 qcom_scm_cpu_cold_bits[QCOM_SCM_BOOT_MAX_CPUS] = {
	0, BIT(0), BIT(3), BIT(5)
};
static const u8 qcom_scm_cpu_warm_bits[QCOM_SCM_BOOT_MAX_CPUS] = {
	BIT(2), BIT(1), BIT(4), BIT(6)
};

#define QCOM_SMC_WAITQ_FLAG_WAKE_ONE	BIT(0)
#define QCOM_SMC_WAITQ_FLAG_WAKE_ALL	BIT(1)

#define QCOM_DLOAD_MASK		GENMASK(5, 4)
enum qcom_dload_mode {
	QCOM_DLOAD_NODUMP	= 0,
	QCOM_DLOAD_FULLDUMP	= 1,
};

static const char * const qcom_scm_convention_names[] = {
	[SMC_CONVENTION_UNKNOWN] = "unknown",
	[SMC_CONVENTION_ARM_32] = "smc arm 32",
	[SMC_CONVENTION_ARM_64] = "smc arm 64",
	[SMC_CONVENTION_LEGACY] = "smc legacy",
};

static struct qcom_scm *__scm;

static int qcom_scm_clk_enable(void)
{
	int ret;

	ret = clk_prepare_enable(__scm->core_clk);
	if (ret)
		goto bail;

	ret = clk_prepare_enable(__scm->iface_clk);
	if (ret)
		goto disable_core;

	ret = clk_prepare_enable(__scm->bus_clk);
	if (ret)
		goto disable_iface;

	return 0;

disable_iface:
	clk_disable_unprepare(__scm->iface_clk);
disable_core:
	clk_disable_unprepare(__scm->core_clk);
bail:
	return ret;
}

static void qcom_scm_clk_disable(void)
{
	clk_disable_unprepare(__scm->core_clk);
	clk_disable_unprepare(__scm->iface_clk);
	clk_disable_unprepare(__scm->bus_clk);
}

static int qcom_scm_bw_enable(void)
{
	int ret = 0;

	if (!__scm->path)
		return 0;

	if (IS_ERR(__scm->path))
		return -EINVAL;

	mutex_lock(&__scm->scm_bw_lock);
	if (!__scm->scm_vote_count) {
		ret = icc_set_bw(__scm->path, 0, UINT_MAX);
		if (ret < 0) {
			dev_err(__scm->dev, "failed to set bandwidth request\n");
			goto err_bw;
		}
	}
	__scm->scm_vote_count++;
err_bw:
	mutex_unlock(&__scm->scm_bw_lock);

	return ret;
}

static void qcom_scm_bw_disable(void)
{
	if (IS_ERR_OR_NULL(__scm->path))
		return;

	mutex_lock(&__scm->scm_bw_lock);
	if (__scm->scm_vote_count-- == 1)
		icc_set_bw(__scm->path, 0, 0);
	mutex_unlock(&__scm->scm_bw_lock);
}

enum qcom_scm_convention qcom_scm_convention = SMC_CONVENTION_UNKNOWN;
static DEFINE_SPINLOCK(scm_query_lock);

static enum qcom_scm_convention __get_convention(void)
{
	unsigned long flags;
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
	bool forced = false;

	if (likely(qcom_scm_convention != SMC_CONVENTION_UNKNOWN))
		return qcom_scm_convention;

	/*
	 * Per the "SMC calling convention specification", the 64-bit calling
	 * convention can only be used when the client is 64-bit, otherwise
	 * system will encounter the undefined behaviour.
	 */
#if IS_ENABLED(CONFIG_ARM64)
	/*
	 * Device isn't required as there is only one argument - no device
	 * needed to dma_map_single to secure world
	 */
	probed_convention = SMC_CONVENTION_ARM_64;
	ret = __scm_smc_call(NULL, &desc, probed_convention, &res, true);
	if (!ret && res.result[0] == 1)
		goto found;

	/*
	 * Some SC7180 firmwares didn't implement the
	 * QCOM_SCM_INFO_IS_CALL_AVAIL call, so we fallback to forcing ARM_64
	 * calling conventions on these firmwares. Luckily we don't make any
	 * early calls into the firmware on these SoCs so the device pointer
	 * will be valid here to check if the compatible matches.
	 */
	if (of_device_is_compatible(__scm ? __scm->dev->of_node : NULL, "qcom,scm-sc7180")) {
		forced = true;
		goto found;
	}
#endif

	probed_convention = SMC_CONVENTION_ARM_32;
	ret = __scm_smc_call(NULL, &desc, probed_convention, &res, true);
	if (!ret && res.result[0] == 1)
		goto found;

	probed_convention = SMC_CONVENTION_LEGACY;
found:
	spin_lock_irqsave(&scm_query_lock, flags);
	if (probed_convention != qcom_scm_convention) {
		qcom_scm_convention = probed_convention;
		pr_info("qcom_scm: convention: %s%s\n",
			qcom_scm_convention_names[qcom_scm_convention],
			forced ? " (forced)" : "");
	}
	spin_unlock_irqrestore(&scm_query_lock, flags);

	return qcom_scm_convention;
}

/**
 * qcom_scm_call() - Invoke a syscall in the secure world
 * @dev:	device
 * @desc:	Descriptor structure containing arguments and return values
 * @res:        Structure containing results from SMC/HVC call
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 */
static int qcom_scm_call(struct device *dev, const struct qcom_scm_desc *desc,
			 struct qcom_scm_res *res)
{
	might_sleep();
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		return scm_smc_call(dev, desc, res, false);
	case SMC_CONVENTION_LEGACY:
		return scm_legacy_call(dev, desc, res);
	default:
		pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}

/**
 * qcom_scm_call_atomic() - atomic variation of qcom_scm_call()
 * @dev:	device
 * @desc:	Descriptor structure containing arguments and return values
 * @res:	Structure containing results from SMC/HVC call
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This can be called in atomic context.
 */
static int qcom_scm_call_atomic(struct device *dev,
				const struct qcom_scm_desc *desc,
				struct qcom_scm_res *res)
{
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		return scm_smc_call(dev, desc, res, true);
	case SMC_CONVENTION_LEGACY:
		return scm_legacy_call_atomic(dev, desc, res);
	default:
		pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}

static bool __qcom_scm_is_call_available(struct device *dev, u32 svc_id,
					 u32 cmd_id)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	desc.arginfo = QCOM_SCM_ARGS(1);
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		if (cmd_id == QCOM_SCM_IS_TZ_LOG_ENCRYPTED)
			desc.args[0] = SCM_SMC_FNID(svc_id, cmd_id) |
				(ARM_SMCCC_OWNER_TRUSTED_OS << ARM_SMCCC_OWNER_SHIFT);
		else
			desc.args[0] = SCM_SMC_FNID(svc_id, cmd_id) |
				(ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT);
		break;
	case SMC_CONVENTION_LEGACY:
		desc.args[0] = SCM_LEGACY_FNID(svc_id, cmd_id);
		break;
	default:
		pr_err("Unknown SMC convention being used\n");
		return false;
	}

	ret = qcom_scm_call(dev, &desc, &res);

	return ret ? false : !!res.result[0];
}

int __qcom_context_ice_sec(u32 type, u8 key_size,
			 u8 algo_mode, u8 *data_ctxt, u32 data_ctxt_len,
			 u8 *salt_ctxt, u32 salt_ctxt_len)
{
	int ret;
	struct qcom_scm_res res;
	void *data_ctxbuf = NULL, *salt_ctxbuf = NULL;
	dma_addr_t data_context_phy, salt_context_phy = 0;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SVC_ICE,
		.cmd = QCOM_SCM_ICE_CONTEXT_CMD,
		.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_VAL, QCOM_SCM_RO, QCOM_SCM_VAL,
				QCOM_SCM_RO, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	if (type == OEM_SEED_TYPE) {
		if (!data_ctxt)
			return -EINVAL;

		data_ctxbuf = dma_alloc_coherent(__scm->dev, data_ctxt_len,
				&data_context_phy, GFP_KERNEL);
		if (!data_ctxbuf)
			return -ENOMEM;

		memcpy(data_ctxbuf, data_ctxt, data_ctxt_len);

		if (algo_mode == ICE_CRYPTO_AES_XTS_MODE && salt_ctxt) {
			salt_ctxbuf = dma_alloc_coherent(__scm->dev, salt_ctxt_len,
				&salt_context_phy, GFP_KERNEL);
		if (!salt_ctxbuf) {
			ret = -ENOMEM;
			goto dma_unmap_data_ctxbuf;
		}
		memcpy(salt_ctxbuf, salt_ctxt, salt_ctxt_len);
		}
	}
	desc.args[0] = type;
	desc.args[1] = key_size;
	desc.args[2] = algo_mode;
	desc.args[3] = data_context_phy;
	desc.args[4] = data_ctxt_len;
	desc.args[5] = salt_context_phy;
	desc.args[6] = salt_ctxt_len;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	if (type == OEM_SEED_TYPE &&
	    algo_mode == ICE_CRYPTO_AES_XTS_MODE && salt_ctxt) {
		memzero_explicit(salt_ctxt, salt_ctxt_len);
		dma_free_coherent(__scm->dev, salt_ctxt_len,
			salt_ctxbuf, salt_context_phy);
	}

dma_unmap_data_ctxbuf:
	if (type == OEM_SEED_TYPE) {
		memzero_explicit(data_ctxbuf, data_ctxt_len);
		dma_free_coherent(__scm->dev, data_ctxt_len,
					data_ctxbuf, data_context_phy);
	}

	return ret ?  : res.result[0];

}

int __qcom_config_sec_ice(void *buf, int size)
{
	int ret;
	dma_addr_t conf_phys;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SVC_ICE,
		.cmd = QCOM_SCM_ICE_CMD,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RO),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	conf_phys = dma_map_single(__scm->dev, buf, size, DMA_TO_DEVICE);

	desc.args[0] = (u64)conf_phys;
	desc.args[1] = size;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? false : !!res.result[0];
}

static int qcom_scm_set_boot_addr(void *entry, const u8 *cpu_bits)
{
	int cpu;
	unsigned int flags = 0;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_ADDR,
		.arginfo = QCOM_SCM_ARGS(2),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	for_each_present_cpu(cpu) {
		if (cpu >= QCOM_SCM_BOOT_MAX_CPUS)
			return -EINVAL;
		flags |= cpu_bits[cpu];
	}

	desc.args[0] = flags;
	desc.args[1] = virt_to_phys(entry);

	return qcom_scm_call_atomic(__scm ? __scm->dev : NULL, &desc, NULL);
}

static int qcom_scm_set_boot_addr_mc(void *entry, unsigned int flags)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_ADDR_MC,
		.owner = ARM_SMCCC_OWNER_SIP,
		.arginfo = QCOM_SCM_ARGS(6),
		.args = {
			virt_to_phys(entry),
			/* Apply to all CPUs in all affinity levels */
			~0ULL, ~0ULL, ~0ULL, ~0ULL,
			flags,
		},
	};

	/* Need a device for DMA of the additional arguments */
	if (!__scm || __get_convention() == SMC_CONVENTION_LEGACY)
		return -EOPNOTSUPP;

	return qcom_scm_call(__scm->dev, &desc, NULL);
}

/**
 * qcom_scm_regsave() - pass a buffer to TZ for saving CPU register context
 * @buf:	Allocated buffer which is used to store the cpu context
 * @size:       size of the buffer
 *
 * Return: 0 on success.
 *
 * Upon successful return, TZ Dump the CPU register context in the
 * buffer on crash
 */
int qcom_scm_regsave(void *buf, u32 size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_UTIL,
		.cmd = QCOM_SCM_CMD_SET_REGSAVE,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = (u64)virt_to_phys(buf),
		.args[1] = size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for all cpus
 * @entry: Entry point function for the cpus
 *
 * Set the Linux entry point for the SCM to transfer control to when coming
 * out of a power down. CPU power down may be executed on cpuidle or hotplug.
 */
int qcom_scm_set_warm_boot_addr(void *entry)
{
	if (qcom_scm_set_boot_addr_mc(entry, QCOM_SCM_BOOT_MC_FLAG_WARMBOOT))
		/* Fallback to old SCM call */
		return qcom_scm_set_boot_addr(entry, qcom_scm_cpu_warm_bits);
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_set_warm_boot_addr);

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for all cpus
 * @entry: Entry point function for the cpus
 */
int qcom_scm_set_cold_boot_addr(void *entry)
{
	if (qcom_scm_set_boot_addr_mc(entry, QCOM_SCM_BOOT_MC_FLAG_COLDBOOT))
		/* Fallback to old SCM call */
		return qcom_scm_set_boot_addr(entry, qcom_scm_cpu_cold_bits);
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_set_cold_boot_addr);

/*
 * qcom_qcekey_release_xpu_prot() - release XPU protection
 */
int qcom_qcekey_release_xpu_prot(void)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QCE_CRYPTO_SIP,
		.cmd = QCOM_SCM_QCE_UNLOCK_CMD,
		.arginfo = QCOM_SCM_ARGS(0, QCOM_SCM_VAL),
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_qcekey_release_xpu_prot);

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags:	Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void qcom_scm_cpu_power_down(u32 flags)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_TERMINATE_PC,
		.args[0] = flags & QCOM_SCM_FLUSH_FLAG_MASK,
		.arginfo = QCOM_SCM_ARGS(1),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	qcom_scm_call_atomic(__scm ? __scm->dev : NULL, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_cpu_power_down);

int qcom_scm_set_remote_state(u32 state, u32 id)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_REMOTE_STATE,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = state,
		.args[1] = id,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	int ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_set_remote_state);

int qcom_scm_io_rmw(phys_addr_t addr, unsigned int mask, unsigned int val)
{
	unsigned int old, new;
	int ret;

	if (!__scm)
		return -EINVAL;

	spin_lock(&__scm->lock);
	ret = qcom_scm_io_readl(addr, &old);
	if (ret)
		goto unlock;

	new = (old & ~mask) | (val & mask);

	ret = qcom_scm_io_writel(addr, new);
unlock:
	spin_unlock(&__scm->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_io_rmw);

static int __qcom_scm_set_dload_mode(struct device *dev, bool enable)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_DLOAD_MODE,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = QCOM_SCM_BOOT_SET_DLOAD_MODE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[1] = enable ? QCOM_SCM_BOOT_SET_DLOAD_MODE : 0;

	return qcom_scm_call_atomic(__scm->dev, &desc, NULL);
}

/**
`* qcom_scm_is_feature_available() - Check if a given feature is enabled by TZ,
 * 				     and its version if enabled.
 * @feature_id: ID of the feature to check in TZ for availablilty/version.
 *
 * Return: 0 on success and the version of the feature in result.
 *
 * TZ returns 0xFFFFFFFF if this smc call is not supported or
 * if smc call supported but feature ID not supported
 */
long  qcom_scm_is_feature_available(u32 feature_id)
{
	long ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_IS_FEATURE_AVAIL,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = feature_id,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_is_feature_available);

static void qcom_scm_set_abnormal_magic(bool enable)
{
	int ret;
	u32 val;

	if (!__scm->dload_mode_addr) {
		dev_err(__scm->dev,"Download mode address is null\n");
		return;
	}

	ret = qcom_scm_io_readl(__scm->dload_mode_addr, &val);
	if (ret) {
		dev_err(__scm->dev,
			"failed to read dload mode address value: %d\n", ret);
		return;
	}

	ret = qcom_scm_io_writel(__scm->dload_mode_addr, enable ?
			val | QCOM_SCM_ABNORMAL_MAGIC :
			val & ~(QCOM_SCM_ABNORMAL_MAGIC));
}

void qcom_scm_set_download_mode(bool enable)
{
	u32 val = enable ? QCOM_DLOAD_FULLDUMP : QCOM_DLOAD_NODUMP;
	bool avail;
	int ret = 0;

	avail = __qcom_scm_is_call_available(__scm->dev,
					     QCOM_SCM_SVC_BOOT,
					     QCOM_SCM_BOOT_SET_DLOAD_MODE);
	if (avail) {
		ret = __qcom_scm_set_dload_mode(__scm->dev, enable);
	} else if (__scm->dload_mode_addr) {
		ret = qcom_scm_io_rmw(__scm->dload_mode_addr,
				      QCOM_DLOAD_MASK,
				      FIELD_PREP(QCOM_DLOAD_MASK, val));
	} else {
		dev_err(__scm->dev,
			"No available mechanism for setting download mode\n");
	}

	if (ret)
		dev_err(__scm->dev, "failed to set download mode: %d\n", ret);
}
EXPORT_SYMBOL_GPL(qcom_scm_set_download_mode);

/**
 * qcom_scm_pas_init_image() - Initialize peripheral authentication service
 *			       state machine for a given peripheral, using the
 *			       metadata
 * @peripheral: peripheral id
 * @metadata:	pointer to memory containing ELF header, program header table
 *		and optional blob of data used for authenticating the metadata
 *		and the rest of the firmware
 * @size:	size of the metadata
 * @ctx:	optional metadata context
 *
 * Return: 0 on success.
 *
 * Upon successful return, the PAS metadata context (@ctx) will be used to
 * track the metadata allocation, this needs to be released by invoking
 * qcom_scm_pas_metadata_release() by the caller.
 */
static int qcom_scm_pas_init_image(struct device *dev, u32 peripheral,
				   const void *metadata, size_t size,
				   struct qcom_pas_context *ctx)
{
	dma_addr_t mdata_phys;
	void *mdata_buf;
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_INIT_IMAGE,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_RW),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	/*
	 * During the scm call memory protection will be enabled for the meta
	 * data blob, so make sure it's physically contiguous, 4K aligned and
	 * non-cachable to avoid XPU violations.
	 */
	mdata_buf = dma_alloc_coherent(dev, size, &mdata_phys, GFP_KERNEL);
	if (!mdata_buf)
		return -ENOMEM;

	memcpy(mdata_buf, metadata, size);

	ret = qcom_scm_clk_enable();
	if (ret)
		goto out;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	desc.args[1] = mdata_phys;

	if (__qcom_scm_is_call_available(dev, QCOM_SCM_SVC_PIL,
					 QCOM_SCM_PAS_INIT_IMAGE_V2_CMD)) {
		desc.cmd = QCOM_SCM_PAS_INIT_IMAGE_V2_CMD;
		desc.arginfo =
			QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_RW, QCOM_SCM_VAL);
		desc.args[2] = size;
	}

	ret = qcom_scm_call(dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

out:
	if (ret < 0 || !ctx) {
		dma_free_coherent(dev, size, mdata_buf, mdata_phys);
	} else if (ctx) {
		ctx->ptr = mdata_buf;
		ctx->phys = mdata_phys;
		ctx->size = size;
	}

	return ret ? : res.result[0];
}

/**
 * qcom_scm_pas_metadata_release() - release metadata context
 * @ctx:	metadata context
 */
static void qcom_scm_pas_metadata_release(struct device *dev,
					  struct qcom_pas_context *ctx)
{
	if (!ctx->ptr)
		return;

	dma_free_coherent(dev, ctx->size, ctx->ptr, ctx->phys);

	ctx->ptr = NULL;
	ctx->phys = 0;
	ctx->size = 0;
}

/**
 * qcom_scm_pas_mem_setup() - Prepare the memory related to a given peripheral
 *			      for firmware loading
 * @peripheral:	peripheral id
 * @addr:	start address of memory area to prepare
 * @size:	size of the memory area to prepare
 *
 * Returns 0 on success.
 */
static int qcom_scm_pas_mem_setup(struct device *dev, u32 peripheral,
				  phys_addr_t addr, phys_addr_t size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MEM_SETUP,
		.arginfo = QCOM_SCM_ARGS(3),
		.args[0] = peripheral,
		.args[1] = addr,
		.args[2] = size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	ret = qcom_scm_call(dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}

int qcom_scm_break_q6_start(u32 reset_cmd_id)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = reset_cmd_id,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = QCOM_BREAK_Q6,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	if (ret || res.result[0])
		return ret ? : res.result[0];

	return 0;
}
EXPORT_SYMBOL(qcom_scm_break_q6_start);

/**
 * qcom_scm_pas_auth_and_reset() - Authenticate the given peripheral firmware
 *				   and reset the remote processor
 * @peripheral:	peripheral id
 *
 * Return 0 on success.
 */
static int __qcom_scm_pas_auth_and_reset(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_AUTH_AND_RESET,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	ret = qcom_scm_call(dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}

int qcom_scm_pas_auth_and_reset(u32 peripheral)
{
	return __qcom_scm_pas_auth_and_reset(__scm->dev, peripheral);
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_auth_and_reset);

/**
 * qcom_scm_pas_shutdown() - Shut down the remote processor
 * @peripheral: peripheral id
 *
 * Returns 0 on success.
 */
static int __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_SHUTDOWN,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	ret = qcom_scm_call(dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}

int qcom_scm_pas_shutdown(u32 peripheral)
{
	return __qcom_scm_pas_shutdown(__scm->dev, peripheral);
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_shutdown);

/**
 * qti_scm_int_radio_powerup - Bring up internal radio userpd
 *
 * @peripheral:	peripheral id
 *
 * Return 0 on success.
 */
int qti_scm_int_radio_powerup(u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_PD_LOAD_SVC_ID,
		.cmd = QCOM_SCM_INT_RAD_PWR_UP_CMD_ID,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		return ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	qcom_scm_bw_disable();
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qti_scm_int_radio_powerup);

/**
 * qti_scm_int_radio_powerdown() - Shut down internal radio userpd
 *
 * @peripheral: peripheral id
 *
 * Returns 0 on success.
 */
int qti_scm_int_radio_powerdown(u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_PD_LOAD_SVC_ID,
		.cmd = QCOM_SCM_INT_RAD_PWR_DN_CMD_ID,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		return ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	qcom_scm_bw_disable();
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qti_scm_int_radio_powerdown);

/**
 * qti_scm_pdseg_memcpy_v2() - copy userpd PIL segments data to dma blocks
 *
 * @peripheral:		peripheral id
 * @phno:		program header no
 * @dma:		handle of dma region
 * @seg_cnt:		no of dma blocks
 *
 * Returns 0 if trustzone successfully loads userpd PIL segments from dma
 * blocks to DDR
 */
int qti_scm_pdseg_memcpy_v2(u32 peripheral, int phno, dma_addr_t dma,
			    int seg_cnt)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_PD_LOAD_SVC_ID,
		.cmd = QCOM_SCM_PD_LOAD_V2_CMD_ID,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL, QCOM_SCM_VAL,
						QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = peripheral,
		.args[1] = phno,
		.args[2] = dma,
		.args[3] = seg_cnt,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		return ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	qcom_scm_bw_disable();
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qti_scm_pdseg_memcpy_v2);

/**
 * qcom_scm_pas_supported() - Check if the peripheral authentication service is
 *			      available for the given peripherial
 * @peripheral:	peripheral id
 *
 * Returns true if PAS is supported for this peripheral, otherwise false.
 */
static bool qcom_scm_pas_supported(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_IS_SUPPORTED,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	if (!__qcom_scm_is_call_available(dev, QCOM_SCM_SVC_PIL,
					  QCOM_SCM_PIL_PAS_IS_SUPPORTED))
		return false;

	ret = qcom_scm_call(dev, &desc, &res);

	return ret ? false : !!res.result[0];
}

static struct qcom_pas_ops qcom_pas_ops_scm = {
	.drv_name	= DRIVER_NAME,
	.supported	= qcom_scm_pas_supported,
	.init_image	= qcom_scm_pas_init_image,
	.mem_setup	= qcom_scm_pas_mem_setup,
	.auth_and_reset	= __qcom_scm_pas_auth_and_reset,
	.shutdown	= __qcom_scm_pas_shutdown,
	.metadata_release = qcom_scm_pas_metadata_release,
};

/**
 * qcom_scm_is_pas_available() - Check if the peripheral authentication service
 *				 is available via SCM or not
 *
 * Returns true if PAS is available, otherwise false.
 */
static bool qcom_scm_is_pas_available(void)
{
	if (!__qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_PIL,
					  QCOM_SCM_PIL_PAS_AUTH_AND_RESET))
		return false;

	return true;
}

static int __qcom_scm_pas_mss_reset(struct device *dev, bool reset)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MSS_RESET,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = reset,
		.args[1] = 0,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	int ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}

static int qcom_scm_pas_reset_assert(struct reset_controller_dev *rcdev,
				     unsigned long idx)
{
	if (idx != 0)
		return -EINVAL;

	return __qcom_scm_pas_mss_reset(__scm->dev, 1);
}

static int qcom_scm_pas_reset_deassert(struct reset_controller_dev *rcdev,
				       unsigned long idx)
{
	if (idx != 0)
		return -EINVAL;

	return __qcom_scm_pas_mss_reset(__scm->dev, 0);
}

static const struct reset_control_ops qcom_scm_pas_reset_ops = {
	.assert = qcom_scm_pas_reset_assert,
	.deassert = qcom_scm_pas_reset_deassert,
};

int qcom_scm_io_readl(phys_addr_t addr, unsigned int *val)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_READ,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = addr,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	int ret;


	ret = qcom_scm_call_atomic(__scm->dev, &desc, &res);
	if (ret >= 0)
		*val = res.result[0];

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_io_readl);

int qcom_scm_io_writel(phys_addr_t addr, unsigned int val)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_WRITE,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = addr,
		.args[1] = val,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	return qcom_scm_call_atomic(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_io_writel);

/**
 * qcom_scm_restore_sec_cfg_available() - Check if secure environment
 * supports restore security config interface.
 *
 * Return true if restore-cfg interface is supported, false if not.
 */
bool qcom_scm_restore_sec_cfg_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_MP,
					    QCOM_SCM_MP_RESTORE_SEC_CFG);
}
EXPORT_SYMBOL_GPL(qcom_scm_restore_sec_cfg_available);

int qcom_scm_restore_sec_cfg(u32 device_id, u32 spare)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_RESTORE_SEC_CFG,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = device_id,
		.args[1] = spare,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	int ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_restore_sec_cfg);

int qcom_scm_iommu_secure_ptbl_size(u32 spare, size_t *size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = spare,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	int ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	if (size)
		*size = res.result[0];

	return ret ? : res.result[1];
}
EXPORT_SYMBOL_GPL(qcom_scm_iommu_secure_ptbl_size);

int qcom_scm_iommu_secure_ptbl_init(u64 addr, u32 size, u32 spare)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT,
		.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW, QCOM_SCM_VAL,
					 QCOM_SCM_VAL),
		.args[0] = addr,
		.args[1] = size,
		.args[2] = spare,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	ret = qcom_scm_call(__scm->dev, &desc, NULL);

	/* the pg table has been initialized already, ignore the error */
	if (ret == -EPERM)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_iommu_secure_ptbl_init);

int qcom_scm_iommu_set_cp_pool_size(u32 spare, u32 size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SET_CP_POOL_SIZE,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = size,
		.args[1] = spare,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_iommu_set_cp_pool_size);

int qcom_scm_mem_protect_video_var(u32 cp_start, u32 cp_size,
				   u32 cp_nonpixel_start,
				   u32 cp_nonpixel_size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_VIDEO_VAR,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL, QCOM_SCM_VAL,
					 QCOM_SCM_VAL, QCOM_SCM_VAL),
		.args[0] = cp_start,
		.args[1] = cp_size,
		.args[2] = cp_nonpixel_start,
		.args[3] = cp_nonpixel_size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_mem_protect_video_var);

static int __qcom_scm_assign_mem(struct device *dev, phys_addr_t mem_region,
				 size_t mem_sz, phys_addr_t src, size_t src_sz,
				 phys_addr_t dest, size_t dest_sz)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_ASSIGN,
		.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_RO, QCOM_SCM_VAL,
					 QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_RO,
					 QCOM_SCM_VAL, QCOM_SCM_VAL),
		.args[0] = mem_region,
		.args[1] = mem_sz,
		.args[2] = src,
		.args[3] = src_sz,
		.args[4] = dest,
		.args[5] = dest_sz,
		.args[6] = 0,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(dev, &desc, &res);

	return ret ? : res.result[0];
}

/**
 * qcom_scm_assign_mem() - Make a secure call to reassign memory ownership
 * @mem_addr: mem region whose ownership need to be reassigned
 * @mem_sz:   size of the region.
 * @srcvm:    vmid for current set of owners, each set bit in
 *            flag indicate a unique owner
 * @newvm:    array having new owners and corresponding permission
 *            flags
 * @dest_cnt: number of owners in next set.
 *
 * Return negative errno on failure or 0 on success with @srcvm updated.
 */
int qcom_scm_assign_mem(phys_addr_t mem_addr, size_t mem_sz,
			u64 *srcvm,
			const struct qcom_scm_vmperm *newvm,
			unsigned int dest_cnt)
{
	struct qcom_scm_current_perm_info *destvm;
	struct qcom_scm_mem_map_info *mem_to_map;
	phys_addr_t mem_to_map_phys;
	phys_addr_t dest_phys;
	dma_addr_t ptr_phys;
	size_t mem_to_map_sz;
	size_t dest_sz;
	size_t src_sz;
	size_t ptr_sz;
	int next_vm;
	__le32 *src;
	void *ptr;
	int ret, i, b;
	u64 srcvm_bits = *srcvm;

	src_sz = hweight64(srcvm_bits) * sizeof(*src);
	mem_to_map_sz = sizeof(*mem_to_map);
	dest_sz = dest_cnt * sizeof(*destvm);
	ptr_sz = ALIGN(src_sz, SZ_64) + ALIGN(mem_to_map_sz, SZ_64) +
			ALIGN(dest_sz, SZ_64);

	ptr = dma_alloc_coherent(__scm->dev, ptr_sz, &ptr_phys, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	/* Fill source vmid detail */
	src = ptr;
	i = 0;
	for (b = 0; b < BITS_PER_TYPE(u64); b++) {
		if (srcvm_bits & BIT(b))
			src[i++] = cpu_to_le32(b);
	}

	/* Fill details of mem buff to map */
	mem_to_map = ptr + ALIGN(src_sz, SZ_64);
	mem_to_map_phys = ptr_phys + ALIGN(src_sz, SZ_64);
	mem_to_map->mem_addr = cpu_to_le64(mem_addr);
	mem_to_map->mem_size = cpu_to_le64(mem_sz);

	next_vm = 0;
	/* Fill details of next vmid detail */
	destvm = ptr + ALIGN(mem_to_map_sz, SZ_64) + ALIGN(src_sz, SZ_64);
	dest_phys = ptr_phys + ALIGN(mem_to_map_sz, SZ_64) + ALIGN(src_sz, SZ_64);
	for (i = 0; i < dest_cnt; i++, destvm++, newvm++) {
		destvm->vmid = cpu_to_le32(newvm->vmid);
		destvm->perm = cpu_to_le32(newvm->perm);
		destvm->ctx = 0;
		destvm->ctx_size = 0;
		next_vm |= BIT(newvm->vmid);
	}

	ret = __qcom_scm_assign_mem(__scm->dev, mem_to_map_phys, mem_to_map_sz,
				    ptr_phys, src_sz, dest_phys, dest_sz);
	dma_free_coherent(__scm->dev, ptr_sz, ptr, ptr_phys);
	if (ret) {
		dev_err(__scm->dev,
			"Assign memory protection call failed %d\n", ret);
		return -EINVAL;
	}

	*srcvm = next_vm;
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_assign_mem);

/**
 * qcom_scm_ocmem_lock_available() - is OCMEM lock/unlock interface available
 */
bool qcom_scm_ocmem_lock_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_OCMEM,
					    QCOM_SCM_OCMEM_LOCK_CMD);
}
EXPORT_SYMBOL_GPL(qcom_scm_ocmem_lock_available);

/**
 * qcom_scm_ocmem_lock() - call OCMEM lock interface to assign an OCMEM
 * region to the specified initiator
 *
 * @id:     tz initiator id
 * @offset: OCMEM offset
 * @size:   OCMEM size
 * @mode:   access mode (WIDE/NARROW)
 */
int qcom_scm_ocmem_lock(enum qcom_scm_ocmem_client id, u32 offset, u32 size,
			u32 mode)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_OCMEM,
		.cmd = QCOM_SCM_OCMEM_LOCK_CMD,
		.args[0] = id,
		.args[1] = offset,
		.args[2] = size,
		.args[3] = mode,
		.arginfo = QCOM_SCM_ARGS(4),
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_ocmem_lock);

/**
 * qcom_scm_ocmem_unlock() - call OCMEM unlock interface to release an OCMEM
 * region from the specified initiator
 *
 * @id:     tz initiator id
 * @offset: OCMEM offset
 * @size:   OCMEM size
 */
int qcom_scm_ocmem_unlock(enum qcom_scm_ocmem_client id, u32 offset, u32 size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_OCMEM,
		.cmd = QCOM_SCM_OCMEM_UNLOCK_CMD,
		.args[0] = id,
		.args[1] = offset,
		.args[2] = size,
		.arginfo = QCOM_SCM_ARGS(3),
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_ocmem_unlock);

bool __qcom_scm_ice_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_ES,
					    QCOM_SCM_ES_INVALIDATE_ICE_KEY) &&
		__qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_ES,
					     QCOM_SCM_ES_CONFIG_SET_ICE_KEY);
}

bool __qcom_scm_ice_hwkey_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SVC_ICE,
					    QCOM_SCM_ICE_CMD);
}

/**
 * qcom_qfprom_show_auth_available() - Check if the SCM call to verify
 *					   secure boot fuse enablement is supported?
 *
 * Return: true if the SCM call is supported
 */
bool qcom_qfprom_show_auth_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_FUSE,
							QCOM_QFPROM_IS_AUTHENTICATE_CMD);
}
EXPORT_SYMBOL_GPL(qcom_qfprom_show_auth_available);

/**
 * qcom_sec_dat_fuse_available() - Check if the SCM call to verify
 *				   fuse sec_dat support is available
 *
 * Return: true if the SCM call is supported
 */
bool qcom_sec_dat_fuse_available(u32 cmd_id)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_FUSE,
					    cmd_id);
}
EXPORT_SYMBOL_GPL(qcom_sec_dat_fuse_available);

bool qcom_sec_upgrade_auth_ld_segments_available(u32 cmd_id)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_BOOT,
					    cmd_id);
}
EXPORT_SYMBOL_GPL(qcom_sec_upgrade_auth_ld_segments_available);

/**
 * qcom_qfrom_fuse_row_write_available() - is the fuse row write interface
 *                                         available ?
 *
 * Return: true if the SCM call is supported
 */
bool qcom_qfrom_fuse_row_write_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_FUSE,
					    QCOM_QFPROM_ROW_WRITE_CMD);
}
EXPORT_SYMBOL_GPL(qcom_qfrom_fuse_row_write_available);

/**
 * qcom_qfrom_fuse_row_read_available() - is the fuse row read interface
 *                                        available ?
 *
 * Return: true if the SCM call is supported
 */
bool qcom_qfrom_fuse_row_read_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_FUSE,
					    QCOM_QFPROM_ROW_READ_CMD);
}
EXPORT_SYMBOL_GPL(qcom_qfrom_fuse_row_read_available);

int __qcom_scm_ice_invalidate_key(u32 index)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_ES,
		.cmd = QCOM_SCM_ES_INVALIDATE_ICE_KEY,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = index,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}

int __qcom_scm_ice_set_key(u32 index, const u8 *key, u32 key_size,
			 enum qcom_scm_ice_cipher cipher, u32 data_unit_size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_ES,
		.cmd = QCOM_SCM_ES_CONFIG_SET_ICE_KEY,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_RW,
					 QCOM_SCM_VAL, QCOM_SCM_VAL,
					 QCOM_SCM_VAL),
		.args[0] = index,
		.args[2] = key_size,
		.args[3] = cipher,
		.args[4] = data_unit_size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	void *keybuf;
	dma_addr_t key_phys;
	int ret;

	/*
	 * 'key' may point to vmalloc()'ed memory, but we need to pass a
	 * physical address that's been properly flushed.  The sanctioned way to
	 * do this is by using the DMA API.  But as is best practice for crypto
	 * keys, we also must wipe the key after use.  This makes kmemdup() +
	 * dma_map_single() not clearly correct, since the DMA API can use
	 * bounce buffers.  Instead, just use dma_alloc_coherent().  Programming
	 * keys is normally rare and thus not performance-critical.
	 */

	keybuf = dma_alloc_coherent(__scm->dev, key_size, &key_phys,
				    GFP_KERNEL);
	if (!keybuf)
		return -ENOMEM;
	memcpy(keybuf, key, key_size);
	desc.args[1] = key_phys;

	ret = qcom_scm_call(__scm->dev, &desc, NULL);

	memzero_explicit(keybuf, key_size);

	dma_free_coherent(__scm->dev, key_size, keybuf, key_phys);
	return ret;
}

/**
 * qcom_scm_hdcp_available() - Check if secure environment supports HDCP.
 *
 * Return true if HDCP is supported, false if not.
 */
bool qcom_scm_hdcp_available(void)
{
	bool avail;
	int ret = qcom_scm_clk_enable();

	if (ret)
		return ret;

	avail = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_HDCP,
						QCOM_SCM_HDCP_INVOKE);

	qcom_scm_clk_disable();

	return avail;
}
EXPORT_SYMBOL_GPL(qcom_scm_hdcp_available);

/**
 * qcom_scm_hdcp_req() - Send HDCP request.
 * @req: HDCP request array
 * @req_cnt: HDCP request array count
 * @resp: response buffer passed to SCM
 *
 * Write HDCP register(s) through SCM.
 */
int qcom_scm_hdcp_req(struct qcom_scm_hdcp_req *req, u32 req_cnt, u32 *resp)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_HDCP,
		.cmd = QCOM_SCM_HDCP_INVOKE,
		.arginfo = QCOM_SCM_ARGS(10),
		.args = {
			req[0].addr,
			req[0].val,
			req[1].addr,
			req[1].val,
			req[2].addr,
			req[2].val,
			req[3].addr,
			req[3].val,
			req[4].addr,
			req[4].val
		},
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	if (req_cnt > QCOM_SCM_HDCP_MAX_REQ_CNT)
		return -ERANGE;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	*resp = res.result[0];

	qcom_scm_clk_disable();

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_hdcp_req);

int qcom_scm_iommu_set_pt_format(u32 sec_id, u32 ctx_num, u32 pt_fmt)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_PT_FORMAT,
		.arginfo = QCOM_SCM_ARGS(3),
		.args[0] = sec_id,
		.args[1] = ctx_num,
		.args[2] = pt_fmt, /* 0: LPAE AArch32 - 1: AArch64 */
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_iommu_set_pt_format);

int qcom_scm_qsmmu500_wait_safe_toggle(bool en)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_CONFIG_ERRATA1,
		.arginfo = QCOM_SCM_ARGS(2),
		.args[0] = QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL,
		.args[1] = en,
		.owner = ARM_SMCCC_OWNER_SIP,
	};


	return qcom_scm_call_atomic(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_qsmmu500_wait_safe_toggle);

bool qcom_scm_lmh_dcvsh_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_LMH, QCOM_SCM_LMH_LIMIT_DCVSH);
}
EXPORT_SYMBOL_GPL(qcom_scm_lmh_dcvsh_available);

int qcom_scm_lmh_profile_change(u32 profile_id)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_LIMIT_PROFILE_CHANGE,
		.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_VAL),
		.args[0] = profile_id,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_lmh_profile_change);

int qcom_scm_lmh_dcvsh(u32 payload_fn, u32 payload_reg, u32 payload_val,
		       u64 limit_node, u32 node_id, u64 version)
{
	dma_addr_t payload_phys;
	u32 *payload_buf;
	int ret, payload_size = 5 * sizeof(u32);

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_LIMIT_DCVSH,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL, QCOM_SCM_VAL),
		.args[1] = payload_size,
		.args[2] = limit_node,
		.args[3] = node_id,
		.args[4] = version,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	payload_buf = dma_alloc_coherent(__scm->dev, payload_size, &payload_phys, GFP_KERNEL);
	if (!payload_buf)
		return -ENOMEM;

	payload_buf[0] = payload_fn;
	payload_buf[1] = 0;
	payload_buf[2] = payload_reg;
	payload_buf[3] = 1;
	payload_buf[4] = payload_val;

	desc.args[0] = payload_phys;

	ret = qcom_scm_call(__scm->dev, &desc, NULL);

	dma_free_coherent(__scm->dev, payload_size, payload_buf, payload_phys);
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_lmh_dcvsh);

static int qcom_scm_find_dload_address(struct device *dev, u64 *addr)
{
	struct device_node *tcsr;
	struct device_node *np = dev->of_node;
	struct resource res;
	u32 offset;
	int ret;

	tcsr = of_parse_phandle(np, "qcom,dload-mode", 0);
	if (!tcsr)
		return 0;

	ret = of_address_to_resource(tcsr, 0, &res);
	of_node_put(tcsr);
	if (ret)
		return ret;

	ret = of_property_read_u32_index(np, "qcom,dload-mode", 1, &offset);
	if (ret < 0)
		return ret;

	*addr = res.start + offset;

	return 0;
}

static int qcom_scm_find_tcsr_boot_info_address(struct device *dev, u64 *addr)
{
	struct device_node *tcsr;
	struct device_node *np = dev->of_node;
	struct resource res;
	u32 offset;
	int ret;

	tcsr = of_parse_phandle(np, "qcom,tcsr-boot-info", 0);
	if (!tcsr)
		return 0;

	ret = of_address_to_resource(tcsr, 0, &res);
	of_node_put(tcsr);
	if (ret)
		return ret;

	ret = of_property_read_u32_index(np, "qcom,tcsr-boot-info", 1, &offset);
	if (ret < 0)
		return ret;

	*addr = res.start + offset;
	return 0;
}

/*
 * qcom_set_qcekey_sec() - Configure key securely
 */
int qti_set_qcekey_sec(void *buf, int size)
{
	return __qti_set_qcekey_sec(__scm->dev, buf, size);
}
EXPORT_SYMBOL_GPL(qti_set_qcekey_sec);

int qti_sec_crypt(void *buf, int size)
{
	return __qti_sec_crypt(__scm->dev, buf, size);
}
EXPORT_SYMBOL_GPL(qti_sec_crypt);

int qti_seccrypt_clearkey(void)
{
	return __qti_seccrypt_clearkey(__scm->dev);
}
EXPORT_SYMBOL_GPL(qti_seccrypt_clearkey);

int qcom_scm_get_device_attestation_ephimeral_key(void *key_buf,
				u32 key_buf_len, u32 *key_len)
{
	int ret;
	ret = __qti_scm_get_device_attestation_ephimeral_key(__scm->dev,
				key_buf, key_buf_len, key_len);
	return ret;

}
EXPORT_SYMBOL_GPL(qcom_scm_get_device_attestation_ephimeral_key);

int qcom_scm_get_device_attestation_response(void *req_buf,
			u32 req_buf_len, void *extclaim_buf, u32 extclaim_buf_len,
			void *resp_buf, u32 resp_buf_len, u32 *attest_resp_len)
{
	int ret;

	ret = __qti_scm_get_device_attestation_response(__scm->dev,	req_buf,
			req_buf_len, extclaim_buf, extclaim_buf_len, resp_buf,
			resp_buf_len, attest_resp_len);

	return ret;

}
EXPORT_SYMBOL_GPL(qcom_scm_get_device_attestation_response);

int qcom_scm_get_device_provision_response(void *provreq_buf,
			u32 provreq_buf_len, void *provresp_buf, u32 provresp_buf_len,
			u32 *prov_resp_size)
{
	int ret;

	ret = __qti_scm_get_device_provision_response(__scm->dev, provreq_buf,
			provreq_buf_len, provresp_buf, provresp_buf_len, prov_resp_size);

	return ret;

}
EXPORT_SYMBOL_GPL(qcom_scm_get_device_provision_response);

int __qti_set_qcekey_sec(struct device *dev, void *confBuf, int size)
{
	int ret;
	dma_addr_t conf_phys;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QCE_CRYPTO_SIP,
		.cmd = QCOM_SCM_QCE_CMD,
		.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_RO),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	conf_phys = dma_map_single(dev, confBuf, size, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, conf_phys);
	if (ret) {
		dev_err(dev, "Allocation fail for conf buffer\n");
		return -ENOMEM;
	}
	desc.args[0] = (u64)conf_phys;
	desc.args[1] = size;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	dma_unmap_single(dev, conf_phys, size, DMA_TO_DEVICE);
	return ret ? : res.result[0];
}

int __qti_sec_crypt(struct device *dev, void *confBuf, int size)
{
	int ret;
	dma_addr_t conf_phys;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QCE_CRYPTO_SIP,
		.cmd = QCOM_SCM_QCE_ENC_DEC_CMD,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	conf_phys = dma_map_single(dev, confBuf, size, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, conf_phys);
	if (ret) {
		dev_err(dev, "Allocation fail for conf buffer\n");
		return -ENOMEM;
	}
	desc.args[0] = (u64)conf_phys;
	desc.args[1] = size;

	return qcom_scm_call(__scm->dev, &desc, &res);

	dma_unmap_single(dev, conf_phys, size, DMA_TO_DEVICE);
	return ret ? : res.result[0];
}

int __qti_seccrypt_clearkey(struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QCE_CRYPTO_SIP,
		.cmd = QCOM_SCM_SECCRYPT_CLRKEY_CMD,
		.arginfo = QCOM_SCM_ARGS(0, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}

/**
* __qti_scm_get_device_attestation_ephimeral_key() - Get M3 public ephimeral key from TME-L
*
* key_buf: key buffer to store the M3 public ephimeral key and this is populated by TME-L
* key_buf_len: key buffer length
* key_len : Size of the M3 Ephimeral public key. This is populated by TME-L after
*           storing the key in the key buffer.
*
* This function can be used to get the M3 public ephimeral key from the TME-L.
 */
int __qti_scm_get_device_attestation_ephimeral_key(struct device *dev,
		void *key_buf, u32 key_buf_len, u32 *key_len)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QWES_SVC_ID,
		.cmd = QCOM_SCM_QWES_INIT_ATTEST,
		.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_RW),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	dma_addr_t dma_key_buf;
	dma_addr_t dma_key_len;

	dma_key_buf = dma_map_single(dev, key_buf, key_buf_len, DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_key_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		return ret;
	}

	dma_key_len  = dma_map_single(dev, key_len, sizeof(u32), DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_key_len);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		goto dma_unmap_key_buf;
	}

	desc.args[0] = dma_key_buf;
	desc.args[1] = key_buf_len;
	desc.args[2] = dma_key_len;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	dma_unmap_single(dev, dma_key_len, sizeof(unsigned int), DMA_FROM_DEVICE);

dma_unmap_key_buf:
	dma_unmap_single(dev, dma_key_buf, key_buf_len, DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}

/**
 * __qti_scm_get_device_attestation_response() - Get attestation response from TME-L
 *
 * req_buf: attestation request buffer, it contains a attestation request.
 * req_buf_len: attestation request buffer length.
 * extclaim_buf: External claim buffer, it also contains attestation request when the
                 attestation request is more than 2KB.
 * extclaim_buf_len: size of external buffer.
 * resp_buf: Response Buffer passed to TME to store the Attestation report response.
 *           TME will used this buffer to populate the Attestation report.
 * resp_buf_len: size of the response buffer.
 * attest_resp_len: Length of the Attestation report response. This is populated by TME
 *                  after storing the attestation response.
 *
 * This function can be used to get the attestation response binary from TME-L by
 * passing the attestation report through req_buf and extclaim_buf.
 */
int __qti_scm_get_device_attestation_response(struct device *dev,
		void *req_buf, u32 req_buf_len, void *extclaim_buf,
		u32 extclaim_buf_len, void *resp_buf, u32 resp_buf_len,
		u32 *attest_resp_len)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QWES_SVC_ID,
		.cmd = QCOM_SCM_QWES_ATTEST_REPORT,
		.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_VAL, QCOM_SCM_RW),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	dma_addr_t dma_req_buf;
	dma_addr_t dma_claim_buf = 0;
	dma_addr_t dma_resp_buf;
	dma_addr_t dma_resp_len;

	dma_req_buf = dma_map_single(dev, req_buf, req_buf_len,
			DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_req_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		return ret;
	}

	if (extclaim_buf != NULL) {
		dma_claim_buf = dma_map_single(dev, extclaim_buf, extclaim_buf_len,
				DMA_FROM_DEVICE);
		ret = dma_mapping_error(dev, dma_claim_buf);
		if (ret != 0) {
			pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
			goto dma_unmap_req_buf;
		}
	}

	dma_resp_buf = dma_map_single(dev, resp_buf, resp_buf_len,
			DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_resp_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		goto dma_unmap_extclaim_buf;
	}

	dma_resp_len = dma_map_single(dev, attest_resp_len,
			sizeof(unsigned int), DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_req_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		goto dma_unmap_resp_buf;
	}
	desc.args[0] = dma_req_buf;
	desc.args[1] = req_buf_len;
	desc.args[2] = dma_claim_buf;
	desc.args[3] = extclaim_buf_len;
	desc.args[4] = dma_resp_buf;
	desc.args[5] = resp_buf_len;
	desc.args[6] = dma_resp_len;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	dma_unmap_single(dev, dma_resp_len, sizeof(unsigned int),
			DMA_FROM_DEVICE);
dma_unmap_resp_buf:
	dma_unmap_single(dev, dma_resp_buf, resp_buf_len, DMA_FROM_DEVICE);

dma_unmap_extclaim_buf:
	if (extclaim_buf != NULL) {
		dma_unmap_single(dev, dma_claim_buf, extclaim_buf_len,
				DMA_FROM_DEVICE);
	}

dma_unmap_req_buf:
	dma_unmap_single(dev, dma_req_buf, req_buf_len, DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}

/**
 *__qti_scm_get_device_provision_response() - Get device provisioning response from TME-L
 *
 * provreq_buf: Provsion request buffer, it contains a provision request.
 * provreq_buf_len: Provision request buffer length.
 * provresp_buf: Provision response buffer passed to TME to store the Provision response.
 *           TME will used this buffer to populate the provision response.
 * provresp_buf_len: size allocated to provision response buffer.
 * attest_resp_len: Length of the provision response. This is populated by TME
 *                  after storing the provision response.
 *
 * This function can be used to get the provision response from TME-L by
 * passing the provision report through prov_req.bin file.
 */
int __qti_scm_get_device_provision_response(struct device *dev,
		void *provreq_buf, u32 provreq_buf_len,
		void *provresp_buf, u32 provresp_buf_len, u32 *prov_resp_size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_QWES_SVC_ID,
		.cmd = QCOM_SCM_QWES_DEVICE_PROVISION,
		.arginfo =  QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_RW),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	dma_addr_t dma_req_buf;
	dma_addr_t dma_resp_buf;
	dma_addr_t dma_prov_resp_size;

	dma_req_buf = dma_map_single(dev, provreq_buf, provreq_buf_len,
			DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_req_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		return ret;
	}

	dma_resp_buf = dma_map_single(dev, provresp_buf, provresp_buf_len,
			DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_resp_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		goto dma_unmap_req_buf;
	}

	dma_prov_resp_size = dma_map_single(dev, prov_resp_size,
			sizeof(unsigned int), DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_req_buf);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		goto dma_unmap_resp_buf;
	}

	desc.args[0] = dma_req_buf;
	desc.args[1] = provreq_buf_len;
	desc.args[2] = dma_resp_buf;
	desc.args[3] = provresp_buf_len;
	desc.args[4] = dma_prov_resp_size;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	dma_unmap_single(dev, dma_prov_resp_size, sizeof(unsigned int),
			DMA_FROM_DEVICE);
dma_unmap_resp_buf:
	dma_unmap_single(dev, dma_resp_buf, provresp_buf_len, DMA_FROM_DEVICE);

dma_unmap_req_buf:
	dma_unmap_single(dev, dma_req_buf, provreq_buf_len, DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}

int qcom_scm_enable_try_mode(void)
{
	int ret;
	u32 val;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {0};

	val = qcom_read_dload_reg();
	desc.svc = QCOM_SCM_SVC_IO;
	desc.cmd = QCOM_SCM_IO_WRITE;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_VAL);
	desc.args[0] = __scm->dload_mode_addr;
	desc.args[1] = val | QTI_TRYBIT;
	desc.owner = ARM_SMCCC_OWNER_SIP;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_enable_try_mode);

int qcom_read_dload_reg(void)
{
	int ret;
	u32 dload_addr_val;

	ret = qcom_scm_io_readl(__scm->dload_mode_addr, &dload_addr_val);
	if (ret) {
		dev_err(__scm->dev,
			"failed to read dload mode address value: %d\n", ret);
		return -EINVAL;
	}
	return dload_addr_val;
}
EXPORT_SYMBOL_GPL(qcom_read_dload_reg);

/**
 * qcom_scm_is_available() - Checks if SCM is available
 */
bool qcom_scm_is_available(void)
{
	/* Paired with smp_store_release() in qcom_scm_probe */
	return !!smp_load_acquire(&__scm);
}
EXPORT_SYMBOL_GPL(qcom_scm_is_available);

static int qcom_scm_assert_valid_wq_ctx(u32 wq_ctx)
{
	/* FW currently only supports a single wq_ctx (zero).
	 * TODO: Update this logic to include dynamic allocation and lookup of
	 * completion structs when FW supports more wq_ctx values.
	 */
	if (wq_ctx != 0) {
		dev_err(__scm->dev, "Firmware unexpectedly passed non-zero wq_ctx\n");
		return -EINVAL;
	}

	return 0;
}

int qcom_scm_wait_for_wq_completion(u32 wq_ctx)
{
	int ret;

	ret = qcom_scm_assert_valid_wq_ctx(wq_ctx);
	if (ret)
		return ret;

	wait_for_completion(&__scm->waitq_comp);

	return 0;
}

static int qcom_scm_waitq_wakeup(struct qcom_scm *scm, unsigned int wq_ctx)
{
	int ret;

	ret = qcom_scm_assert_valid_wq_ctx(wq_ctx);
	if (ret)
		return ret;

	complete(&__scm->waitq_comp);

	return 0;
}

static irqreturn_t qcom_scm_irq_handler(int irq, void *data)
{
	int ret;
	struct qcom_scm *scm = data;
	u32 wq_ctx, flags, more_pending = 0;

	do {
		ret = scm_get_wq_ctx(&wq_ctx, &flags, &more_pending);
		if (ret) {
			dev_err(scm->dev, "GET_WQ_CTX SMC call failed: %d\n", ret);
			goto out;
		}

		if (flags != QCOM_SMC_WAITQ_FLAG_WAKE_ONE &&
		    flags != QCOM_SMC_WAITQ_FLAG_WAKE_ALL) {
			dev_err(scm->dev, "Invalid flags found for wq_ctx: %u\n", flags);
			goto out;
		}

		ret = qcom_scm_waitq_wakeup(scm, wq_ctx);
		if (ret)
			goto out;
	} while (more_pending);

out:
	return IRQ_HANDLED;
}

int qti_scm_is_tz_log_encryption_supported(void)
{
	int ret;
	ret = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_BOOT,
					   QCOM_SCM_IS_TZ_LOG_ENCRYPTED);

	return (ret == 1) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(qti_scm_is_tz_log_encryption_supported);

int qti_scm_is_tz_log_encrypted(void)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_IS_TZ_LOG_ENCRYPTED,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.arginfo = QCOM_SCM_ARGS(0),
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qti_scm_is_tz_log_encrypted);

/**
 * qcom_qfprom_show_authenticate() - Checks if secure boot fuse is enabled
 */
int qcom_qfprom_show_authenticate(void)
{
	int ret;
	dma_addr_t auth_phys;
	void *auth_buf;
	char buf;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_FUSE,
		.cmd = QCOM_QFPROM_IS_AUTHENTICATE_CMD,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RO),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	auth_buf = dma_alloc_coherent(__scm->dev, sizeof(buf),
					&auth_phys, GFP_KERNEL);
	if (!auth_buf) {
		dev_err(__scm->dev, "Allocation for auth buffer failed\n");
		return -ENOMEM;
	}
	desc.args[0] = (u64)auth_phys,
	desc.args[1] = sizeof(char),

	ret = qcom_scm_call(__scm->dev, &desc, NULL);
	memcpy(&buf, auth_buf, sizeof(char));
	dma_free_coherent(__scm->dev, sizeof(buf), auth_buf, auth_phys);

	if (ret) {
		 pr_err("%s: Error in QFPROM read : %d\n", __func__, ret);
		 return -1;
	}
	return buf == 1 ? 1 : 0;
}
EXPORT_SYMBOL_GPL(qcom_qfprom_show_authenticate);

int ipq54xx_qcom_qfprom_show_authenticate(void)
{
	int ret;
	struct fuse_payload *fuse = NULL;

	fuse = kzalloc(sizeof(*fuse), GFP_KERNEL);
	if (!fuse)
		return -ENOMEM;

	fuse[0].fuse_addr = SECURE_BOOT_FUSE_ADDR;

	ret = qcom_scm_get_ipq_fuse_list(fuse, sizeof(struct fuse_payload));
	if (ret) {
		pr_err("SCM call for reading ipq54xx fuse failed with error:%d\n", ret);
		ret = -1;
		goto fuse_alloc_err;
	}

	if (fuse[0].lsb_val & OEM_SEC_BOOT_ENABLE)
		ret = 1;
fuse_alloc_err:
	kfree(fuse);
	return ret;
}
EXPORT_SYMBOL_GPL(ipq54xx_qcom_qfprom_show_authenticate);

int qcom_sec_upgrade_auth(unsigned int scm_cmd_id, unsigned int sw_type,
				unsigned int img_size, unsigned int load_addr)
{
	int ret;
	struct qcom_scm_res res;
        struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = scm_cmd_id,
		.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL,
                                                                QCOM_SCM_RW),
                .args[0] = sw_type,
                .args[1] = img_size,
                .args[2] = (u64)load_addr,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_sec_upgrade_auth);

int qti_scm_get_encrypted_tz_log(void *ker_buf, u32 buf_len, u32 log_id,
				 u32 seg_id)
{
	int ret;
	dma_addr_t log_buf;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_GET_TZ_LOG_ENCRYPTED,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
	};

	log_buf = dma_map_single(__scm->dev, ker_buf, buf_len, DMA_FROM_DEVICE);
	ret = dma_mapping_error(__scm->dev, log_buf);

	if (ret) {
		dev_err(__scm->dev, "DMA Mapping error: %d\n", ret);
		return ret;
	}
	desc.args[0] = log_buf;
	desc.args[1] = buf_len;
	desc.args[2] = log_id;

	if (seg_id) {
		desc.args[3] = seg_id;
		desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
					     QCOM_SCM_VAL, QCOM_SCM_VAL);
	} else {
		desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW, QCOM_SCM_VAL,
					     QCOM_SCM_VAL);
	}

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	dma_unmap_single(__scm->dev, log_buf, buf_len, DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qti_scm_get_encrypted_tz_log);

/**
 * qti_scm_tz_log() - Get trustzone diag log
 * ker_buf: kernel buffer to store the diag log
 * buf_len: kernel buffer length
 *
 * Return negative errno on failure or 0 on success. Diag log will
 * be present in the kernel buffer passed.
 */
int qti_scm_tz_log(void *ker_buf, u32 buf_len)
{
	return __qti_scm_tz_hvc_log(__scm->dev, QCOM_SCM_SVC_INFO,
				     QTI_SCM_TZ_DIAG_CMD, ker_buf, buf_len);
}
EXPORT_SYMBOL_GPL(qti_scm_tz_log);

/**
 * qti_scm_hvc_log() - Get hypervisor diag log
 * ker_buf: kernel buffer to store the diag log
 * buf_len: kernel buffer length
 *
 * Return negative errno on failure or 0 on success. Diag log will
 * be present in the kernel buffer passed.
 */
int qti_scm_hvc_log(void *ker_buf, u32 buf_len)
{
	return __qti_scm_tz_hvc_log(__scm->dev, QCOM_SCM_SVC_INFO,
				    __scm->hvc_log_cmd_id, ker_buf, buf_len);
}
EXPORT_SYMBOL_GPL(qti_scm_hvc_log);
/**
 * __qti_scm_tz_hvc_log() - Get trustzone diag log or hypervisor diag log
 * @svc_id: SCM service id
 * @cmd_id: SCM command id
 * ker_buf: kernel buffer to store the diag log
 * buf_len: kernel buffer length
 *
 * This function can be used to get either the trustzone diag log
 * or the hypervisor diag log based on the command id passed to this
 * function.
 */

int __qti_scm_tz_hvc_log(struct device *dev, u32 svc_id, u32 cmd_id,
			 void *ker_buf, u32 buf_len)
{
	int ret;
	dma_addr_t dma_buf;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = svc_id,
		.cmd = cmd_id,
		.owner = ARM_SMCCC_OWNER_SIP,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
	};

	dma_buf = dma_map_single(__scm->dev, ker_buf, buf_len, DMA_FROM_DEVICE);

	ret = dma_mapping_error(__scm->dev, dma_buf);
	if (ret != 0) {
		pr_err("DMA Mapping Error : %d\n", ret);
		return ret;
	}

	desc.args[0] = dma_buf;
	desc.args[1] = buf_len;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	dma_unmap_single(__scm->dev, dma_buf, buf_len, DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}

/**
 * __qti_scm_get_ecdsa_blob() - Get the ECDSA blob from TME-L by sending NONCE
 *
 * @svc_id: SCM service id
 * @cmd_id: SCM command id
 * nonce_buf: NONCE buffer which contains the NONCE recieved from Q6.
 * nonce_buf_len: Variable for NONCE buffer length
 * ecdsa_buf: ECDSA buffer, used to receive the ECDSA blob from TME
 * ecdsa_buf_len: Variable which holds the total ECDSA buffer lenght
 * *ecdsa_consumed_len: Pointer to get the consumed ECDSA buffer lenght from TME
 *
 * This function can be used to get the ECDSA blob from TME-L by passing the
 * NONCE through nonce_buf. nonce_buf and ecdsa_buf should be DMA alloc
 * coherent and caller should take care of it.
 */
int __qti_scm_get_ecdsa_blob(struct device *dev, u32 svc_id, u32 cmd_id,
		dma_addr_t nonce_buf, u32 nonce_buf_len, dma_addr_t ecdsa_buf,
		u32 ecdsa_buf_len, u32 *ecdsa_consumed_len)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = svc_id,
		.cmd = cmd_id,
		.owner = ARM_SMCCC_OWNER_SIP,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_RW),
	};

	dma_addr_t dma_ecdsa_consumed_len;

	dma_ecdsa_consumed_len = dma_map_single(dev, ecdsa_consumed_len,
			sizeof(u32), DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_ecdsa_consumed_len);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		return ret;
	}

	desc.args[0] = nonce_buf;
	desc.args[1] = nonce_buf_len;
	desc.args[2] = ecdsa_buf;
	desc.args[3] = ecdsa_buf_len;
	desc.args[4] = dma_ecdsa_consumed_len;

	ret = qcom_scm_call(dev, &desc, &res);
	dma_unmap_single(dev, dma_ecdsa_consumed_len, sizeof(u32), DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}

int qti_scm_get_ecdsa_blob(u32 svc_id, u32 cmd_id, dma_addr_t nonce_buf,
			u32 nonce_buf_len, dma_addr_t ecdsa_buf,
			u32 ecdsa_buf_len, u32 *ecdsa_consumed_len)
{
	int ret;
	ret = __qti_scm_get_ecdsa_blob(__scm->dev, svc_id, cmd_id, nonce_buf,
			nonce_buf_len, ecdsa_buf, ecdsa_buf_len, ecdsa_consumed_len);
	return ret;
}
EXPORT_SYMBOL_GPL(qti_scm_get_ecdsa_blob);

/**
 * __qti_scm_get_smmustate () - Get SMMU state
 * @svc_id: SCM service id
 * @cmd_id: SCM command id
 *
 * Returns 0 - SMMU_DISABLE_NONE
 *	   1 - SMMU_DISABLE_S2
 *	   2 - SMMU_DISABLE_ALL on success.
 *	  -1 - Failure
 */

int qti_scm_get_smmustate(void)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = __scm->smmu_state_cmd_id,
		.owner = ARM_SMCCC_OWNER_SIP,
		.arginfo = QCOM_SCM_ARGS(0),
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}

int qcom_fuseipq_scm_call(u32 svc_id, u32 cmd_id,void *cmd_buf, size_t size)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {0};
	uint64_t *status;
	struct fuse_blow *fuse_blow = cmd_buf;

	desc.svc = svc_id;
	desc.cmd = cmd_id;
	desc.owner = ARM_SMCCC_OWNER_SIP;
	desc.args[0] = fuse_blow->address;

	if (fuse_blow->size) {
		desc.args[1] = fuse_blow->size;
		desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RO, QCOM_SCM_VAL);
	} else {
		desc.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_RO);
	}

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	status = (uint64_t *)fuse_blow->status;
	*status = res.result[0];
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_fuseipq_scm_call);

/**
 * qcom_scm_get_ipq_fuse_list() - Get OEM Fuse parameter from TME-L
 *
 * @fuse: QFPROM CORR addresses
 * @size: size of fuse structure
 *
 * This function can be used to get the OEM Fuse parameters from TME-L.
 */
int qcom_scm_get_ipq_fuse_list(void *fuse, size_t size)
{
	int ret;
	dma_addr_t dma_fuse;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_FUSE,
		.cmd = QCOM_SCM_OWM_FUSE_CMD_ID,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	dma_fuse  = dma_map_single(__scm->dev, fuse, size, DMA_FROM_DEVICE);
	ret = dma_mapping_error(__scm->dev, dma_fuse);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		return -EINVAL;
	}
	desc.args[0] = dma_fuse;
	desc.args[1] = size;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	if(res.result[0] != 0) {
		pr_err("%s : Response error code is : %#x\n", __func__,
				(unsigned int)res.result[0]);
	}

	dma_unmap_single(__scm->dev, dma_fuse, size, DMA_FROM_DEVICE);

	return ret ? : res.result[0];

}
EXPORT_SYMBOL_GPL(qcom_scm_get_ipq_fuse_list);

/**
 * qcom_scm_sec_auth_available() - Checks if SEC_AUTH is supported.
 *
 * Return true if SEC_AUTH is supported, false if not.
 */
bool qcom_scm_sec_auth_available(unsigned int scm_cmd_id)
{
	int ret;

	ret = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_SEC_AUTH,
						scm_cmd_id);
	return ret > 0 ? true : false;
}
EXPORT_SYMBOL_GPL(qcom_scm_sec_auth_available);

int qcom_sec_upgrade_auth_meta_data(unsigned int scm_cmd_id,unsigned int sw_type,
					unsigned int img_size,unsigned int load_addr,
					void* hash_addr,unsigned int hash_size)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = scm_cmd_id,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_RW, QCOM_SCM_VAL,
							   QCOM_SCM_RW, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	dma_addr_t hash_address;

	hash_address = dma_map_single(__scm->dev, hash_addr, hash_size, DMA_FROM_DEVICE);

	ret = dma_mapping_error(__scm->dev, hash_address);
	if (ret != 0) {
		pr_err("%s: DMA Mapping Error : %d\n", __func__, ret);
		return ret;
	}
	desc.args[0] = sw_type;
	desc.args[1] = (u64)load_addr;
	desc.args[2] = img_size;
	desc.args[3] = hash_address;
	desc.args[4] = hash_size;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	dma_unmap_single(__scm->dev, hash_address, hash_size, DMA_FROM_DEVICE);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_sec_upgrade_auth_meta_data);

/**
 * qcom_sec_upgrade_auth_ld_segments() - Pass the structure pointer containing
 * the start and end addr of the elf loadable segments and the elf address to
 * TZ for authentication. TZ invokes IPC to TME for the image authentication.
 *
 * scm_cmd_id: SCM CMD ID
 * sw_type: SW type of the image to be authenticated
 * elf_addr: Physical address where the elf is loaded
 * meta_data_size: offset + size of the last NULL segment in ELF
 * ld_seg_info: Structure pointer containing the start and end addr of the
 *		elf loadable segments
 * ld_seg_count: count of the lodable segments
 */
int qcom_sec_upgrade_auth_ld_segments(unsigned int scm_cmd_id, unsigned int sw_type,
				      u32 elf_addr, u32 meta_data_size,
				      struct load_segs_info *ld_seg_info,
				      u32 ld_seg_count, u64 *status)
{
	int ret;
	struct qcom_scm_res res;
	u32 ld_seg_buff_size;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = scm_cmd_id,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_RO, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	dma_addr_t ld_seg_addr;

	desc.args[0] = elf_addr;
	desc.args[1] = meta_data_size;
	desc.args[2] = sw_type;

	if (ld_seg_info) {
		ld_seg_buff_size = ld_seg_count * sizeof(struct load_segs_info);
		ld_seg_addr = dma_map_single(__scm->dev, ld_seg_info,
					     ld_seg_buff_size, DMA_TO_DEVICE);

		ret = dma_mapping_error(__scm->dev, ld_seg_addr);
		if (ret != 0) {
			pr_err("%s: DMA Mapping Error: %d\n", __func__, ret);
			return ret;
		}
		desc.args[3] = ld_seg_addr;
		desc.args[4] = ld_seg_buff_size;
	} else {
		/* Passing NULL and zero for ld_seg_addr and ld_seg_buff_size for
		 * rootfs image auth as it does not contain loadable segments
		 */
		desc.args[3] = 0;
		desc.args[4] = 0;
	}

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	*status = res.result[0];

	if (ld_seg_info)
		dma_unmap_single(__scm->dev, ld_seg_addr, ld_seg_buff_size, DMA_TO_DEVICE);

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_sec_upgrade_auth_ld_segments);

int qcom_sec_upgrade_auth_hash_n_metadata(unsigned int scm_cmd_id, unsigned int sw_type,
					  void *md_addr, u32 meta_data_size,
					  struct load_segs_info *ld_seg_info,
					  u32 ld_seg_count, void *hash_buf, u32 hash_size)
{
	int ret;
	struct qcom_scm_res res;
	u32 ld_seg_buff_size;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = scm_cmd_id,
		.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_RO, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	dma_addr_t hash_addr, meta_data_addr, ld_seg_addr;

	meta_data_addr = dma_map_single(__scm->dev, md_addr,
					meta_data_size, DMA_TO_DEVICE);
	ret = dma_mapping_error(__scm->dev, meta_data_addr);
	if (ret) {
		pr_err("%s: DMA Mapping Error: %d\n", __func__, ret);
		return ret;
	}

	hash_addr = dma_map_single(__scm->dev, hash_buf,
				   hash_size, DMA_TO_DEVICE);
	ret = dma_mapping_error(__scm->dev, hash_addr);
	if (ret) {
		pr_err("%s: DMA Mapping Error: %d\n", __func__, ret);
		return ret;
	}
	desc.args[0] = meta_data_addr;
	desc.args[1] = meta_data_size;
	desc.args[2] = sw_type;
	desc.args[5] = hash_addr;
	desc.args[6] = hash_size;

	if (ld_seg_info) {
		ld_seg_buff_size = ld_seg_count * sizeof(struct load_segs_info);
		ld_seg_addr = dma_map_single(__scm->dev, ld_seg_info,
				ld_seg_buff_size, DMA_TO_DEVICE);

		ret = dma_mapping_error(__scm->dev, ld_seg_addr);
		if (ret) {
			pr_err("%s: DMA Mapping Error: %d\n", __func__, ret);
			return ret;
		}
		desc.args[3] = ld_seg_addr;
		desc.args[4] = ld_seg_buff_size;
	} else {
		/* Passing NULL and zero for ld_seg_addr and ld_seg_buff_size
		 * for rootfs image or if reserved memory is not available
		 */
		desc.args[3] = 0;
		desc.args[4] = 0;
	}

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	dma_unmap_single(__scm->dev, hash_addr, hash_size, DMA_TO_DEVICE);
	dma_unmap_single(__scm->dev, meta_data_addr, meta_data_size, DMA_TO_DEVICE);

	if (ld_seg_info)
		dma_unmap_single(__scm->dev, ld_seg_addr, ld_seg_buff_size, DMA_TO_DEVICE);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_sec_upgrade_auth_hash_n_metadata);

int qcom_qfprom_write_version(uint32_t sw_type, uint32_t value, uint32_t qfprom_ret_ptr)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_FUSE,
		.cmd = QCOM_QFPROM_ROW_WRITE_CMD,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_qfprom_write_version);

int qcom_qfprom_read_version(uint32_t sw_type, uint32_t value, uint32_t qfprom_ret_ptr)
{
	int ret;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_FUSE,
		.cmd = QCOM_QFPROM_ROW_READ_CMD,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_RW, QCOM_SCM_VAL,
						QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = sw_type,
		.args[1] = (u64)value,
		.args[2] = sizeof(uint32_t),
		.args[3] = (u64)qfprom_ret_ptr,
		.args[4] = sizeof(uint32_t),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_qfprom_read_version);

int qti_scm_qseecom_remove_xpu()
{
	return __qti_scm_qseecom_remove_xpu(__scm->dev);
}
EXPORT_SYMBOL_GPL(qti_scm_qseecom_remove_xpu);

int qti_scm_qseecom_notify(struct qsee_notify_app *req, size_t req_size,
			   struct qseecom_command_scm_resp *resp,
			   size_t resp_size)
{
	return __qti_scm_qseecom_notify(__scm->dev, req, req_size,
				      resp, resp_size);
}
EXPORT_SYMBOL_GPL(qti_scm_qseecom_notify);

int qti_scm_qseecom_load(uint32_t smc_id, uint32_t cmd_id,
			 union qseecom_load_ireq *req, size_t req_size,
			 struct qseecom_command_scm_resp *resp,
			 size_t resp_size)
{
	return __qti_scm_qseecom_load(__scm->dev, smc_id, cmd_id, req, req_size,
				    resp, resp_size);
}
EXPORT_SYMBOL_GPL(qti_scm_qseecom_load);

int qti_scm_qseecom_send_data(union qseecom_client_send_data_ireq *req,
			      size_t req_size,
			      struct qseecom_command_scm_resp *resp,
			      size_t resp_size)
{
	return __qti_scm_qseecom_send_data(__scm->dev, req, req_size,
					 resp, resp_size);
}
EXPORT_SYMBOL_GPL(qti_scm_qseecom_send_data);

int qti_scm_qseecom_unload(uint32_t smc_id, uint32_t cmd_id,
			   struct qseecom_unload_ireq *req,
			   size_t req_size,
			   struct qseecom_command_scm_resp *resp,
			   size_t resp_size)
{
	return __qti_scm_qseecom_unload(__scm->dev, smc_id, cmd_id, req,
				      req_size, resp, resp_size);
}
EXPORT_SYMBOL_GPL(qti_scm_qseecom_unload);

int qti_scm_register_log_buf(struct device *dev,
				struct qsee_reg_log_buf_req *request,
				size_t req_size,
				struct qseecom_command_scm_resp *response,
				size_t resp_size)
{
	return __qti_scm_register_log_buf(__scm->dev, request, req_size,
					    response, resp_size);
}
EXPORT_SYMBOL_GPL(qti_scm_register_log_buf);

int qti_scm_aes(uint32_t req_addr, uint32_t req_size, u32 cmd_id)
{
	return __qti_scm_aes(__scm->dev, req_addr, req_size, cmd_id);
}
EXPORT_SYMBOL_GPL(qti_scm_aes);

int qti_scm_aes_clear_key_handle(uint32_t key_handle, u32 cmd_id)
{
	return __qti_scm_aes_clear_key_handle(__scm->dev, key_handle, cmd_id);
}
EXPORT_SYMBOL_GPL(qti_scm_aes_clear_key_handle);

int qti_scm_tls_hardening(uint32_t req_addr, uint32_t req_size,
			  uint32_t resp_addr, uint32_t resp_size, u32 cmd_id)
{
	return __qti_scm_tls_hardening(__scm->dev, req_addr, req_size,
				      resp_addr, resp_size, cmd_id);
}
EXPORT_SYMBOL_GPL(qti_scm_tls_hardening);

int __qcom_remove_xpu_scm_call_available(struct device *dev, u32 svc_id, u32 cmd_id)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_IS_CALL_AVAIL_CMD,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = SCM_QSEEOS_FNID(svc_id, cmd_id) |
				(ARM_SMCCC_OWNER_TRUSTED_OS << ARM_SMCCC_OWNER_SHIFT),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}

int __qti_scm_qseecom_remove_xpu(struct device *dev)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_APP_MGR,
		.cmd = QTI_ARMv8_CMD_REMOVE_XPU,
		.owner = QTI_OWNER_QSEE_OS,
	};

	ret = __qcom_remove_xpu_scm_call_available(dev, QTI_SVC_APP_MGR,
					QTI_ARMv8_CMD_REMOVE_XPU);

	if (ret <= 0)
		return -ENOTSUPP;

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}

int __qti_scm_qseecom_notify(struct device *dev,
			     struct qsee_notify_app *req, size_t req_size,
			     struct qseecom_command_scm_resp *resp,
			     size_t resp_size)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_APP_MGR,
		.cmd = QTI_CMD_NOTIFY_REGION_ID,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = req->applications_region_addr,
		.args[1] = req->applications_region_size,
		.owner = QTI_OWNER_QSEE_OS,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	resp->result = res.result[0];
	resp->resp_type = res.result[1];
	resp->data = res.result[2];

	return ret;
}

int __qti_scm_qseecom_load(struct device *dev, uint32_t smc_id,
			   uint32_t cmd_id, union qseecom_load_ireq *req,
			   size_t req_size,
			   struct qseecom_command_scm_resp *resp,
			   size_t resp_size)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_APP_MGR,
		.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL,
					    QCOM_SCM_VAL),
		.args[0] = req->load_lib_req.mdt_len,
		.args[1] = req->load_lib_req.img_len,
		.args[2] = req->load_lib_req.phy_addr,
		.owner = QTI_OWNER_QSEE_OS,
	};


	if (cmd_id == QSEOS_APP_START_COMMAND) {
		desc.cmd = QTI_CMD_LOAD_APP_ID;
		ret = qcom_scm_call(__scm->dev, &desc, &res);
	}
	else {
		desc.cmd = QTI_CMD_LOAD_LIB;
		ret = qcom_scm_call(__scm->dev, &desc, &res);
	}

	resp->result = res.result[0];
	resp->resp_type = res.result[1];
	resp->data = res.result[2];

	return ret;
}

int __qti_scm_qseecom_send_data(struct device *dev,
				union qseecom_client_send_data_ireq *req,
				size_t req_size,
				struct qseecom_command_scm_resp *resp,
				size_t resp_size)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_APP_ID_PLACEHOLDER,
		.cmd = QTI_CMD_SEND_DATA_ID,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_RW, QCOM_SCM_VAL,
					QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = req->v1.app_id,
		.args[1] = req->v1.req_ptr,
		.args[2] = req->v1.req_len,
		.args[3] = req->v1.rsp_ptr,
		.args[4] = req->v1.rsp_len,
		.owner = QTI_OWNER_TZ_APPS,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	resp->result = res.result[0];
	resp->resp_type = res.result[1];
	resp->data = res.result[2];

	return ret;
}

int __qti_scm_qseecom_unload(struct device *dev, uint32_t smc_id,
			     uint32_t cmd_id, struct qseecom_unload_ireq *req,
			     size_t req_size,
			     struct qseecom_command_scm_resp *resp,
			     size_t resp_size)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_APP_MGR,
		.owner = QTI_OWNER_QSEE_OS,
	};

	switch (cmd_id) {
	case QSEOS_APP_SHUTDOWN_COMMAND:
		desc.cmd = QTI_CMD_UNLOAD_APP_ID;
		desc.arginfo = QCOM_SCM_ARGS(1);
		desc.args[0] = req->app_id;
		ret = qcom_scm_call(__scm->dev, &desc, &res);
		break;

	case QSEE_UNLOAD_SERV_IMAGE_COMMAND:
		desc.cmd = QTI_CMD_UNLOAD_LIB;
		ret = qcom_scm_call(__scm->dev, &desc, &res);
		break;

	default:
		pr_info("\nIncorrect command id has been passed");
		return -EINVAL;
	}

	resp->result = res.result[0];
	resp->resp_type = res.result[1];
	resp->data = res.result[2];

	return ret;
}

int __qti_scm_register_log_buf(struct device *dev,
				  struct qsee_reg_log_buf_req *request,
				  size_t req_size,
				  struct qseecom_command_scm_resp *response,
				  size_t resp_size)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_APP_MGR,
		.cmd = QTI_CMD_REGISTER_LOG_BUF,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = request->phy_addr,
		.args[1] = request->len,
		.owner = QTI_OWNER_QSEE_OS,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	response->result = res.result[0];
	response->resp_type = res.result[1];
	response->data = res.result[2];

	return ret;
}

int __qti_scm_tls_hardening(struct device *dev, uint32_t req_addr,
			    uint32_t req_size, uint32_t resp_addr,
			    uint32_t resp_size, u32 cmd_id)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_CRYPTO,
		.cmd = cmd_id,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
					QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = req_addr,
		.args[1] = req_size,
		.args[2] = resp_addr,
		.args[3] = resp_size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	if (res.result[0] == QCOM_SCM_EINVAL_SIZE) {
		pr_err("%s: TZ does not support data larger than 2K bytes: -%llu\n",
					__func__, res.result[0]);
	}
	return ret ? : res.result[0];
}

int __qti_scm_aes(struct device *dev, uint32_t req_addr,
		  uint32_t req_size, u32 cmd_id)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_CRYPTO,
		.cmd = cmd_id,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
		.args[0] = req_addr,
		.args[1] = req_size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = __qcom_scm_is_call_available(__scm->dev, QTI_SVC_CRYPTO, cmd_id);
	if (ret == 1) {
		ret = qcom_scm_call(__scm->dev, &desc, &res);
	} else {
		pr_err("%s : Feature not supported by TZ..!\n", __func__);
		return -EINVAL;
	}

	return res.result[0];
}

int __qti_scm_aes_clear_key_handle(struct device *dev, uint32_t key_handle, u32 cmd_id)
{
	int ret = 0;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SVC_CRYPTO,
		.cmd = cmd_id,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = key_handle,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	return ret ? : res.result[0];
}

int qcom_scm_sdi_disable(struct device *dev)
{
	int ret;
	struct qcom_scm_res res;
	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID,
		.args[0] = 1ull, /* Disable wdog debug */
		.args[1] = 0ull, /* SDI Enable */
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	ret = qcom_scm_call(__scm->dev, &desc, &res);

	qcom_scm_clk_disable();
	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_sdi_disable);

int qcom_scm_derive_and_share_key(u32 key_len, uint8_t *sw_context,
				  u32 sw_context_len, uint8_t *derived_key,
				  uint32_t derived_key_len)
{
	dma_addr_t dma_sw_context_buf;
	dma_addr_t dma_derived_key_buf;
	char *sw_context_buf, *derived_key_buf;
	int ret = -ENOMEM;
	struct qcom_scm_res res;
	struct qcom_scm_desc desc = {
		.svc = QTI_SCM_DERIVE_KEY,
		.cmd = QTI_SCM_DERIVE_KEY_PARAM_ID,
		.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL, QCOM_SCM_RO,
					 QCOM_SCM_VAL, QCOM_SCM_RW,
					 QCOM_SCM_VAL),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	if (sw_context_len) {
		sw_context_buf = dma_alloc_coherent(__scm->dev, PAGE_SIZE,
						    &dma_sw_context_buf,
						    GFP_KERNEL);
		if (!sw_context_buf) {
			pr_err("DMA Allocation failed for sw_context_buf\n");
			return ret;
		}
		memcpy(sw_context_buf, sw_context, sw_context_len);
	}

	derived_key_buf = dma_alloc_coherent(__scm->dev, PAGE_SIZE,
					     &dma_derived_key_buf, GFP_KERNEL);
	if (!derived_key_buf) {
		pr_err("DMA Allocation failed for derived_key_buf\n");
		goto dma_unmap_sw_context_buf;
	}

	desc.args[0] = key_len;
	desc.args[1] = dma_sw_context_buf;
	desc.args[2] = sw_context_len;
	desc.args[3] = dma_derived_key_buf;
	desc.args[4] = derived_key_len;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	if (ret) {
		pr_err("%s: Response error code is : 0x%x\n", __func__,
		       (unsigned int)res.result[0]);
	}

	memcpy(derived_key, derived_key_buf, derived_key_len);
	dma_free_coherent(__scm->dev, PAGE_SIZE, derived_key_buf,
			  dma_derived_key_buf);

dma_unmap_sw_context_buf:
	if (sw_context_len) {
		dma_free_coherent(__scm->dev, PAGE_SIZE, sw_context_buf,
				  dma_sw_context_buf);
	}

	return ret ?  : res.result[0];
}

static ssize_t hlos_done_show(struct device *device,
			      struct device_attribute *attr,
			      char *buf)
{
	u32 val;
	int ret;
	struct scm_priv_data *data = dev_get_drvdata(device);

	ret = qcom_scm_io_readl(__scm->dload_mode_addr, &val);
	if (ret) {
		dev_err(__scm->dev,
			"dload secure read failed with err: %d\n", ret);
		return -EINVAL;
	}

	return sysfs_emit(buf, "%d\n", (val & data->milestone_mask) ? 1 : 0);
}

static ssize_t hlos_done_store(struct device *device,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long input;
	u32 val;
	int ret;
	struct scm_priv_data *data = dev_get_drvdata(device);

	if (kstrtoul(buf, 0, &input))
		return -EINVAL;

	if (input != 0)
		return -EINVAL;

	ret = qcom_scm_io_readl(__scm->dload_mode_addr, &val);
	if (ret) {
		dev_err(__scm->dev,
			"dload secure read failed with err: %d\n", ret);
		return -EINVAL;
	}

	val &= ~(data->milestone_mask);

	ret = qcom_scm_io_writel(__scm->dload_mode_addr, val);
	if (ret) {
		dev_err(__scm->dev,
			"Clearing HLOS milestone bit failed with err: %d\n", ret);
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RW(hlos_done);

static ssize_t trymode_inprogress_show(struct device *device,
				       struct device_attribute *attr,
				       char *buf)
{
	u32 *addr;

	addr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_TRYMODE_INFO, NULL);
	if (IS_ERR(addr)) {
		dev_err(__scm->dev, "Failed to get the trymode information\n");
		return -EINVAL;
	}

	return sysfs_emit(buf, "%x\n", *addr);
}

static ssize_t trybit_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	u32 val;
	int ret;

	ret = qcom_scm_io_readl(__scm->dload_mode_addr, &val);
	if (ret) {
		dev_err(__scm->dev,
			"dload secure read failed with err: %d\n", ret);
		return -EINVAL;
	}

	return sysfs_emit(buf, "%d\n", (val & QTI_TRYBIT) ? 1 : 0);
}

static ssize_t trybit_store(struct device *device,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	u32 val;
	int ret;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val != 1)
		return -EINVAL;

	ret = qcom_scm_io_readl(__scm->dload_mode_addr, &val);
	if (ret) {
		dev_err(__scm->dev,
			"dload secure read failed with err: %d\n", ret);
		return -EINVAL;
	}

	val |= QTI_TRYBIT;

	ret = qcom_scm_io_writel(__scm->dload_mode_addr, val);
	if (ret) {
		dev_err(__scm->dev,
			"Enable Try mode bit failed with err: %d\n", ret);
		return -EINVAL;
	}

	return count;
}

static ssize_t tcsr_boot_info_store(struct device *device,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	u32 val;
	u32 boot_info_val;
	int ret;

	if (!of_device_is_compatible(__scm->dev->of_node, "qcom,scm-ipq5424"))
		return -EINVAL;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val != 1 && val != 0)
		return -EINVAL;

	ret = qcom_scm_io_readl(__scm->tcsr_boot_info_addr, &boot_info_val);
	if (ret) {
		dev_err(__scm->dev,
			"pbl dload secure read failed with err: %d\n", ret);
		return -EINVAL;
	}
	boot_info_val = (val == 1) ? (boot_info_val | BOOT_INFO_USE_SET_B) :
			(boot_info_val & ~BOOT_INFO_USE_SET_B);

	ret = qcom_scm_io_writel(__scm->tcsr_boot_info_addr, boot_info_val);
	if (ret) {
		dev_err(__scm->dev,
			"Enable Pbl Try mode bit failed with err: %d\n", ret);
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RO(trymode_inprogress);
static DEVICE_ATTR_RW(trybit);
static DEVICE_ATTR_WO(tcsr_boot_info);

static struct attribute *qcom_firmware_attrs[] = {
	&dev_attr_hlos_done.attr,
	&dev_attr_trymode_inprogress.attr,
	&dev_attr_trybit.attr,
	&dev_attr_tcsr_boot_info.attr,
	NULL,
};

ATTRIBUTE_GROUPS(qcom_firmware);

static int qcom_scm_probe(struct platform_device *pdev)
{
	struct qcom_scm *scm;
	int irq, ret;
	const struct scm_priv_data *data;

	scm = devm_kzalloc(&pdev->dev, sizeof(*scm), GFP_KERNEL);
	if (!scm)
		return -ENOMEM;

	scm->dev = &pdev->dev;
	ret = qcom_scm_find_dload_address(&pdev->dev, &scm->dload_mode_addr);
	if (ret < 0)
		return ret;

	ret = qcom_scm_find_tcsr_boot_info_address(&pdev->dev,
						   &scm->tcsr_boot_info_addr);
	if (ret < 0)
		return ret;

	data = of_device_get_match_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, (void *)data);

	ret = of_property_read_u32(pdev->dev.of_node, "hvc-log-cmd-id", &scm->hvc_log_cmd_id);
	if (ret)
		scm->hvc_log_cmd_id = QTI_SCM_HVC_DIAG_CMD;

	ret = of_property_read_u32(pdev->dev.of_node, "smmu-state-cmd-id",
				   &scm->smmu_state_cmd_id);
	if (ret)
		scm->smmu_state_cmd_id = QTI_SCM_SMMUSTATE_CMD;

	init_completion(&scm->waitq_comp);
	mutex_init(&scm->scm_bw_lock);
	spin_lock_init(&scm->lock);

	scm->path = devm_of_icc_get(&pdev->dev, NULL);
	if (IS_ERR(scm->path))
		return dev_err_probe(&pdev->dev, PTR_ERR(scm->path),
				     "failed to acquire interconnect path\n");

	scm->core_clk = devm_clk_get_optional(&pdev->dev, "core");
	if (IS_ERR(scm->core_clk))
		return PTR_ERR(scm->core_clk);

	scm->iface_clk = devm_clk_get_optional(&pdev->dev, "iface");
	if (IS_ERR(scm->iface_clk))
		return PTR_ERR(scm->iface_clk);

	scm->bus_clk = devm_clk_get_optional(&pdev->dev, "bus");
	if (IS_ERR(scm->bus_clk))
		return PTR_ERR(scm->bus_clk);

	scm->reset.ops = &qcom_scm_pas_reset_ops;
	scm->reset.nr_resets = 1;
	scm->reset.of_node = pdev->dev.of_node;
	ret = devm_reset_controller_register(&pdev->dev, &scm->reset);
	if (ret)
		return ret;

	/* vote for max clk rate for highest performance */
	ret = clk_set_rate(scm->core_clk, INT_MAX);
	if (ret)
		return ret;

	/* Paired with smp_load_acquire() in qcom_scm_is_available(). */
	smp_store_release(&__scm, scm);

	irq = platform_get_irq_optional(pdev, 0);
	if (irq < 0) {
		if (irq != -ENXIO)
			return irq;
	} else {
		ret = devm_request_threaded_irq(__scm->dev, irq, NULL, qcom_scm_irq_handler,
						IRQF_ONESHOT, "qcom-scm", __scm);
		if (ret < 0)
			return dev_err_probe(scm->dev, ret, "Failed to request qcom-scm irq\n");
	}

	__get_convention();

	if (qcom_scm_is_pas_available()) {
		qcom_pas_ops_scm.dev = scm->dev;
		qcom_pas_ops_register(&qcom_pas_ops_scm);
	}

	/*
	 * If requested enable "download mode", from this point on warmboot
	 * will cause the boot stages to enter download mode, unless
	 * disabled below by a clean shutdown/reboot.
	 */
	if (download_mode) {
		qcom_scm_set_download_mode(true);
	}
	else {
		if (data->flag & SDI_DISABLE)
			qcom_scm_sdi_disable(__scm->dev);
		if (data->flag & ABNORMAL_MAGIC)
			qcom_scm_set_abnormal_magic(true);
	}

	return 0;
}

static void qcom_scm_shutdown(struct platform_device *pdev)
{
	struct scm_priv_data *data = dev_get_drvdata(&pdev->dev);

	/* Clean shutdown, disable download mode to allow normal restart */
	qcom_scm_set_download_mode(false);
	if (data->flag & ABNORMAL_MAGIC)
		qcom_scm_set_abnormal_magic(false);
}

struct scm_priv_data ipq9574_data = {
	.flag = (SDI_DISABLE | ABNORMAL_MAGIC),
	.milestone_mask = BIT(8)
};

struct scm_priv_data ipq5424_data = {
	.milestone_mask = BIT(8)
};

struct scm_priv_data ipq9650_data = {
	.milestone_mask = BIT(8)
};

struct scm_priv_data ipq5210_data = {
	.milestone_mask = BIT(8)
};

static const struct of_device_id qcom_scm_dt_match[] = {
	{ .compatible = "qcom,scm" },

	/* Legacy entries kept for backwards compatibility */
	{ .compatible = "qcom,scm-apq8064" },
	{ .compatible = "qcom,scm-apq8084" },
	{ .compatible = "qcom,scm-ipq4019" },
	{ .compatible = "qcom,scm-ipq5210", .data = &ipq5210_data },
	{ .compatible = "qcom,scm-ipq5424", .data = &ipq5424_data},
	{ .compatible = "qcom,scm-ipq9574", .data = &ipq9574_data},
	{ .compatible = "qcom,scm-ipq9650", .data = &ipq9650_data },
	{ .compatible = "qcom,scm-msm8953" },
	{ .compatible = "qcom,scm-msm8974" },
	{ .compatible = "qcom,scm-msm8996" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_scm_dt_match);

static struct platform_driver qcom_scm_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = qcom_scm_dt_match,
		.dev_groups = qcom_firmware_groups,
		.suppress_bind_attrs = true,
	},
	.probe = qcom_scm_probe,
	.shutdown = qcom_scm_shutdown,
};

static int __init qcom_scm_init(void)
{
	return platform_driver_register(&qcom_scm_driver);
}
subsys_initcall(qcom_scm_init);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. SCM driver");
MODULE_LICENSE("GPL v2");
