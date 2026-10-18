/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QCN_IE_EXTN_H
#define QCN_IE_EXTN_H

#define OUI_QCN					0x8cfdf0
#define QCN_OUI_TYPE				0x01

/*
 * 32-bit length:
 * vendor type OUI (3 bytes, big-endian)in bits [31:8]
 * OUI type in bits [7:0]
 */
#define QCN_IE_VENDOR_TYPE			((OUI_QCN << 8) | QCN_OUI_TYPE)
#define QCN_ATTRIB_VERSION			0x01
#define QCN_VER_ATTR_VER			0x01
#define QCN_VER_ATTR_SUBVERSION			0x00

/* Per-attribute overhead: id(1) + len(1) = 2 bytes (QCN_ATTRIB_HDR_LEN) */
#define QCN_ATTRIB_HDR_LEN			2

#define QCN_ATTRIB_HE_240_MHZ_SUPP		0X0B
#define QCN_ATTRIB_5GHZ_320MHZ_CSA		0X0C
#define QCN_HE_240_MHZ_MAX_ELEM_LEN		9
#define QCN_5GHZ_320MHZ_CSA_ELEM_LEN		9

/*
 * QCN IE fixed overhead:
 * EID(1) + Len(1) + OUI(3) + type(1) + Version subelement(4) = 10 bytes
 */
#define QCN_IE_HDR_LEN				10

#define QCN_ATTRIB_HE_MCS_12_13_SUPP		0x09
#define QCN_HE_MCS_12_13_SUPP_ATTRIB_LEN	2

/* VHT MCS 10/11 (1024-QAM) support attribute */
#define QCN_ATTRIB_VHT_MCS10_11_SUPP		0x02
#define QCN_VHT_MCS10_11_SUPP_ATTRIB_LEN	1

/* HE 400ns SGI (0.4us Guard Interval) support attribute.
 * 3-byte payload: byte[0]=1xLTF+0.4us, byte[1]=2xLTF+0.4us, byte[2]=4xLTF(rsvd=0)
 */
#define QCN_ATTRIB_HE_400NS_SGI_SUPP		0x03
#define QCN_HE_400NS_SGI_SUPP_ATTRIB_LEN	3

/* HE 2xLTF in 160/80+80 MHz support attribute. 1-byte payload. */
#define QCN_ATTRIB_HE_2XLTF_160_80P80_SUPP	0x04
#define QCN_HE_2XLTF_160_80P80_SUPP_ATTRIB_LEN	1

/*
 * Wire format in QCN IE (2-byte payload):
 *   byte[0] = (self_cap >> QCN_HE_MCS_12_13_L80_SHIFT) & QCN_HE_MCS_12_13_MASK
 *   byte[1] = (self_cap >> QCN_HE_MCS_12_13_G80_SHIFT) & QCN_HE_MCS_12_13_MASK
 */
#define QCN_HE_MCS_12_13_L80_SHIFT		0
#define QCN_HE_MCS_12_13_G80_SHIFT		8
#define QCN_HE_MCS_12_13_MASK			0xff
#define QCN_HE_MCS_12_13_EXTRACT_NSS(c, s)	(((c) >> (s)) & QCN_HE_MCS_12_13_MASK)

#define OUI_QCOM				0x00037f

u8 *qcn_ie_begin(u8 *pos, u8 **len_ptr);
void qcn_ie_end(u8 *len_ptr, const u8 *end);
u8 *qcn_eid_add_he_mcs_12_13_attr(u16 self_cap, bool is_enabled, u8 *pos);
u8 *qcn_eid_add_vht_mcs10_11_attr(bool is_enabled, u8 *pos);
u8 *qcn_eid_add_he_400ns_sgi_attr(bool is_enabled, u8 *pos);
u8 *qcn_eid_add_he_2xltf_160_attr(bool is_enabled, u8 *pos);

#endif /* QCN_IE_EXTN_H */
