/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "cmn.h"

void hostapd_iface_init_extn(struct hostapd_iface *iface)
{
	if (!iface)
		return;

	iface->iface_extn.check_hw_blocklist = true;
	iface->iface_extn.agile_capable = true;
}

void hostapd_iface_deinit_extn(struct hostapd_iface *iface)
{
	if (!iface)
		return;

	hostapd_free_hw_blocklist_info_extn(iface->iface_extn.hw_blocklist_info,
					    iface->iface_extn.num_hw_blocklist);
	iface->iface_extn.hw_blocklist_info = NULL;
	iface->iface_extn.num_hw_blocklist = 0;

	iface->iface_extn.is_waiting_for_afc_regd = false;
}

void hostapd_free_hw_features_extn(struct hostapd_hw_modes *hw_features,
				   size_t num_hw_features)
{
#ifdef CONFIG_QCN_APP_EXTN
	size_t i;
#endif

	if (!hw_features)
		return;

#ifdef CONFIG_QCN_APP_EXTN
	for (i = 0; i < num_hw_features; i++) {
		struct qacs_data_extn *data_extn;

		data_extn = ICM_GET_EXTN_DATA_PTR((hw_features + i));
		if (!data_extn || !data_extn->neighbor_report)
			continue;

		free(data_extn->neighbor_report);
		data_extn->neighbor_report = NULL;
		data_extn->num_neighbor_entries = 0;
	}
#endif
}

