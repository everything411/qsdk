/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MC_WIFI_EVENTS_H
#define _MC_WIFI_EVENTS_H_

#include <linux/netdevice.h>
#include <linux/if_ether.h>

/*
 * mc_event_start()
 */
int mc_event_start(void);

/*
 * mc_event_stop()
 */
int mc_event_stop(void);
#endif /* _MC_WIFI_EVENTS_H */
