/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef CBS_H
#define CBS_H

#include <stdbool.h>
#include "utils/os.h"

union wpa_event_data;
struct i802_bss;
struct hostapd_data;


#define CBS_EVENT_STARTED "CBS-STARTED "
#define CBS_EVENT_COMPLETED "CBS-COMPLETED "
#define CBS_EVENT_ABORTED "CBS-ABORTED "

#define HOSTAPD_CBS_RETRIGGER_TIME 300000 /* CBS scan trigger is skipped if it is triggered again under 5 mins */

struct cbs_params_extn {
	u8 cbs_enable; /* enable/disable cbs scan (0:disable | 1:enable CBS scan once | 2:enable cbs scan to run continuously)*/
	int dwellsplit; /* dwell split time on a foreign channel for one scan (msec) */
	int totaldwell; /* total dwell time for scan on single channel (msec) */
	int dwellrest; /* time to rest before issuing the next consecutive scan on the same channel (msec) */
	int resttime; /* time to wait between scans on different channels (msec) */
	int waittime; /* time to wait after scanning all channels and before starting the next scan (msec) */
	struct hostapd_channel_data *best_chan; /* Best channel chosen based on CBS scan results */
	struct os_reltime cbs_scan_complete_ts; /* Timestamp when CBS scan completed */
	struct os_reltime acs_scan_complete_ts; /* Timestamp when ACS scan completed */
	u32 retrigger_time; /* Minimum age (ms) before CBS scan can be retriggered. ACS is directly
			     * triggered by skipping the CBS scan if cbs enable is invoked before this time
			     * (msec)
			     */
};

#ifdef CONFIG_ACS
int hostapd_handle_cli_cbs_extn(struct hostapd_data *hapd,
				char *pos, char *buf,
				size_t buflen);
int hostapd_cbs_handle_scan_complete(struct hostapd_data *hapd,
				     union wpa_event_data *data);

int hostapd_cbs_trigger_csa(struct hostapd_data *hapd);

#else
static inline int
hostapd_cbs_trigger_csa(struct hostapd_data *hapd)
{
	return -EOPNOTSUPP;
}

static inline
int hostapd_handle_cli_cbs_extn(struct hostapd_data *hapd,
				char *pos, char *buf,
				size_t buflen)
{
	return -EOPNOTSUPP;
}

static inline
int hostapd_cbs_handle_scan_complete(struct hostapd_data *hapd,
				     union wpa_event_data *data)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_ACS */
#endif
