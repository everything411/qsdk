/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef RNR_H
#define RNR_H

/*
 * Configure 6Ghz RNR advertisement frame wise.
 *
 * Bit mask controlling RNR per frame type:
 * bit0 (0x1) = Beacon
 * bit1 (0x2) = Probe Response
 * bit2 (0x4) = FILS discovery frame
 *
 * In 6Ghz RNR advertisement mode is set, hostapd to peek into frame
 * selection bit to decide if RNR to be added.
 * If default mode, based on 6 GHz colocated with lowerband 2 GHz / 5 GHz
 * will decide if RNR is to be added either inband discovery
 * (6 GHz beacon / probe respose / FILS ) or OOB discovery
 * (lowerband beacon / probe response).
 */
#define WLAN_RNR_IN_BCN    BIT(0)
#define WLAN_RNR_IN_PRB    BIT(1)
#define WLAN_RNR_IN_FILS   BIT(2)

#endif /* RNR_H */

