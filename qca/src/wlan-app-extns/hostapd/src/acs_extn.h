/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ACS_EXTN_H
#define ACS_EXTN_H

#include <stdbool.h>

struct hostapd_iface;

#define DYNAMIC_ACS_EVENT_STARTED "DYNAMIC-ACS-EVENT-STARTED "
#define DYNAMIC_ACS_EVENT_COMPLETED "DYNAMIC-ACS-EVENT-COMPLETED "
#define DYNAMIC_ACS_EVENT_FAILED "DYNAMIC-ACS-EVENT-FAILED "

void acs_fill_timestamp(struct hostapd_iface *iface,
			int trigger_type, bool is_trigger_time);

#ifdef CONFIG_ACS
int hostapd_get_center_chan_extn(struct hostapd_iface *iface,
				 struct hostapd_channel_data *chan,
				 enum oper_chan_width oper_bw);
int hostapd_set_channel_penalty_extn(struct hostapd_iface *iface,
				     int freq,
				     int penalty_percent);
#else
static inline int
hostapd_get_center_chan_extn(struct hostapd_iface *iface,
			     struct hostapd_channel_data *chan,
			     enum oper_chan_width oper_bw)
{
	return -EOPNOTSUPP;
}

static inline int
hostapd_set_channel_penalty_extn(struct hostapd_iface *iface,
				 int freq,
				 int penalty_percent)
{
	return -EOPNOTSUPP;
}
#endif
#endif
