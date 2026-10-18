/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ARM_SMMU_QCOM_H
#define _ARM_SMMU_QCOM_H

struct qcom_smmu {
	struct arm_smmu_device smmu;
	const struct qcom_smmu_match_data *data;
	bool bypass_quirk;
	u8 bypass_cbndx;
	u32 stall_enabled;
	/*
	 * Shared Stage-2 identity-mapped context bank.
	 * When s2_identity_quirk is set, all Stage-1 context banks are
	 * switched from CBAR_TYPE_S1_TRANS_S2_BYPASS (TYPE=1) to
	 * CBAR_TYPE_S1_TRANS_S2_TRANS (TYPE=3), pointing at a shared
	 * Stage-2 CB configured with an identity-mapped page table
	 * (IPA == PA).
	 */
	bool s2_identity_quirk;
	u8 s2_identity_cbndx;
	u8 s2_identity_vmid;
	u64 *s2_identity_pgtbl;
};

enum qcom_smmu_impl_reg_offset {
	QCOM_SMMU_TBU_PWR_STATUS,
	QCOM_SMMU_STATS_SYNC_INV_TBU_ACK,
	QCOM_SMMU_MMU2QSS_AND_SAFE_WAIT_CNTR,
};

struct qcom_smmu_config {
	const u32 *reg_offset;
};

struct qcom_smmu_match_data {
	const struct qcom_smmu_config *cfg;
	const struct arm_smmu_impl *impl;
	const struct arm_smmu_impl *adreno_impl;
	const struct of_device_id * const client_match;
	/* Enable the shared Stage-2 identity-mapped CB for nested translation */
	bool s2_identity;
};

#ifdef CONFIG_ARM_SMMU_QCOM_DEBUG
void qcom_smmu_tlb_sync_debug(struct arm_smmu_device *smmu);
#else
static inline void qcom_smmu_tlb_sync_debug(struct arm_smmu_device *smmu) { }
#endif

#endif /* _ARM_SMMU_QCOM_H */
