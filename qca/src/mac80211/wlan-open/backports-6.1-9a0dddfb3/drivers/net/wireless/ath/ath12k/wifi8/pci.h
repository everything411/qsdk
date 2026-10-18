/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_PCI_WIFI8_H
#define ATH12K_PCI_WIFI8_H

#define QCN9625_WLAON_SOC_RESET_CAUSE_SHADOW_REG 0x1F80718
#define QCN9625_RESET_CAUSE_Q6_BCR BIT(18)

static const struct ath12k_msi_config ath12k_wifi8_msi_config[] = {
	{
		/* MSI spec expects number of interrupts to be a power of 2 */
		.total_vectors = 32,
		.total_users = 4,
		.users = (struct ath12k_msi_user[]) {
			{ .name = "MHI", .num_vectors = 3, .base_vector = 0 },
			{ .name = "CE", .num_vectors = 5, .base_vector = 3 },
			{ .name = "MGMT", .num_vectors = 2, .base_vector = 8 },
			{ .name = "DP", .num_vectors = 22, .base_vector = 10 },
		},
	},
};

int ath12k_wifi8_pci_init(void);
void ath12k_wifi8_pci_exit(void);

#endif /* ATH12K_PCI_WIFI8_H */
