/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef QCN_ELEM_H
#define QCN_ELEM_H

#define WLAN_OUI_QCN                           0x8CFDF0
#define WLAN_OUI_QCN_TYPE_1                    1
#define WLAN_OUI_QCN_HEADER_LEN                4 /* WLAN_OUI_QCN + type */
#define WLAN_OUI_QCN_ATTR_VER                  1
#define WLAN_OUI_QCN_ATTR_HE_MCS_12_13_SUPP    0x09
#define WLAN_OUI_QCN_ATTR_5GHZ_240MHZ_SUPP     0x0B
#define WLAN_OUI_QCN_ATTR_5GHZ_320MHZ_CSA      0x0C

/* QCN attribute ID for VHT MCS 10/11 (1024-QAM) support.
 * Payload: 1 byte, value 1 = supported, 0 = not supported.
 */
#define WLAN_OUI_QCN_ATTR_VHT_MCS10_11_SUPP    0x02
/* QCN attribute ID for HE 400ns SGI support (0.4us GI).
 * Payload: 3 bytes.
 *   byte[0]: 1 = RX 1xLTF + 0.4us GI supported, 0 = not supported
 *   byte[1]: 1 = RX 2xLTF + 0.4us GI supported, 0 = not supported
 *   byte[2]: reserved (4xLTF + 0.4us GI, not yet supported, always 0)
 */
#define WLAN_OUI_QCN_ATTR_HE_400NS_SGI_SUPP     0x03
/* QCN attribute ID for HE 2xLTF in 160/80+80 MHz support.
 * Payload: 1 byte. Value 1 = supported, 0 = not supported.
 */
#define WLAN_OUI_QCN_ATTR_HE_2XLTF_160_80P80_SUPP 0x04

#define IEEE80211_EHT_240MHZ_PHY_SOUNDING_DIM_320MHZ_MASK       BIT(0) | BIT(1)

struct ieee80211_qcn_version_vendor_elem_extn {
	u8 version;
	u8 subversion;
}__packed;

#endif /* QCN_ELEM_H */

