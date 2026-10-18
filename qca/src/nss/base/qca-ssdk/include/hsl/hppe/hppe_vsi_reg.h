/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#if defined(CONFIG_CPU_BIG_ENDIAN)
#include "hppe_vsi_reg_be.h"
#else
/**
 * @defgroup
 * @{
 */
#ifndef HPPE_VSI_REG_H
#define HPPE_VSI_REG_H

/*[table] VSI_TBL*/
#define VSI_TBL
#define VSI_TBL_ADDRESS 0x3800
#define VSI_TBL_NUM     64
#define VSI_TBL_INC     0x10
#define VSI_TBL_TYPE    REG_TYPE_RW
#define VSI_TBL_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP*/
	#define VSI_TBL_MEMBER_PORT_BITMAP
	#define VSI_TBL_MEMBER_PORT_BITMAP_OFFSET  0
	#define VSI_TBL_MEMBER_PORT_BITMAP_LEN     8
	#define VSI_TBL_MEMBER_PORT_BITMAP_DEFAULT 0x0
	/*[field] UUC_BITMAP*/
	#define VSI_TBL_UUC_BITMAP
	#define VSI_TBL_UUC_BITMAP_OFFSET  8
	#define VSI_TBL_UUC_BITMAP_LEN     8
	#define VSI_TBL_UUC_BITMAP_DEFAULT 0x0
	/*[field] UMC_BITMAP*/
	#define VSI_TBL_UMC_BITMAP
	#define VSI_TBL_UMC_BITMAP_OFFSET  16
	#define VSI_TBL_UMC_BITMAP_LEN     8
	#define VSI_TBL_UMC_BITMAP_DEFAULT 0x0
	/*[field] BC_BITMAP*/
	#define VSI_TBL_BC_BITMAP
	#define VSI_TBL_BC_BITMAP_OFFSET  24
	#define VSI_TBL_BC_BITMAP_LEN     8
	#define VSI_TBL_BC_BITMAP_DEFAULT 0x0
	/*[field] NEW_ADDR_LRN_EN*/
	#define VSI_TBL_NEW_ADDR_LRN_EN
	#define VSI_TBL_NEW_ADDR_LRN_EN_OFFSET  32
	#define VSI_TBL_NEW_ADDR_LRN_EN_LEN     1
	#define VSI_TBL_NEW_ADDR_LRN_EN_DEFAULT 0x0
	/*[field] NEW_ADDR_FWD_CMD*/
	#define VSI_TBL_NEW_ADDR_FWD_CMD
	#define VSI_TBL_NEW_ADDR_FWD_CMD_OFFSET  33
	#define VSI_TBL_NEW_ADDR_FWD_CMD_LEN     2
	#define VSI_TBL_NEW_ADDR_FWD_CMD_DEFAULT 0x0
	/*[field] STATION_MOVE_LRN_EN*/
	#define VSI_TBL_STATION_MOVE_LRN_EN
	#define VSI_TBL_STATION_MOVE_LRN_EN_OFFSET  35
	#define VSI_TBL_STATION_MOVE_LRN_EN_LEN     1
	#define VSI_TBL_STATION_MOVE_LRN_EN_DEFAULT 0x0
	/*[field] STATION_MOVE_FWD_CMD*/
	#define VSI_TBL_STATION_MOVE_FWD_CMD
	#define VSI_TBL_STATION_MOVE_FWD_CMD_OFFSET  36
	#define VSI_TBL_STATION_MOVE_FWD_CMD_LEN     2
	#define VSI_TBL_STATION_MOVE_FWD_CMD_DEFAULT 0x0

struct vsi_tbl {
	a_uint32_t  member_port_bitmap:8;
	a_uint32_t  uuc_bitmap:8;
	a_uint32_t  umc_bitmap:8;
	a_uint32_t  bc_bitmap:8;
	a_uint32_t  new_addr_lrn_en:1;
	a_uint32_t  new_addr_fwd_cmd:2;
	a_uint32_t  station_move_lrn_en:1;
	a_uint32_t  station_move_fwd_cmd:2;
	a_uint32_t  _reserved0:26;
};

union vsi_tbl_u {
	a_uint32_t val[2];
	struct vsi_tbl bf;
};

/*[table] VLAN_CNT_TBL*/
#define VLAN_CNT_TBL
#define VLAN_CNT_TBL_ADDRESS 0x78000
#define VLAN_CNT_TBL_NUM     32
#define VLAN_CNT_TBL_INC     0x10
#define VLAN_CNT_TBL_TYPE    REG_TYPE_RW
#define VLAN_CNT_TBL_DEFAULT 0x0
	/*[field] RX_PKT_CNT*/
	#define VLAN_CNT_TBL_RX_PKT_CNT
	#define VLAN_CNT_TBL_RX_PKT_CNT_OFFSET  0
	#define VLAN_CNT_TBL_RX_PKT_CNT_LEN     32
	#define VLAN_CNT_TBL_RX_PKT_CNT_DEFAULT 0x0
	/*[field] RX_BYTE_CNT*/
	#define VLAN_CNT_TBL_RX_BYTE_CNT
	#define VLAN_CNT_TBL_RX_BYTE_CNT_OFFSET  32
	#define VLAN_CNT_TBL_RX_BYTE_CNT_LEN     40
	#define VLAN_CNT_TBL_RX_BYTE_CNT_DEFAULT 0x0

struct vlan_cnt_tbl {
	a_uint32_t  rx_pkt_cnt:32;
	a_uint32_t  rx_byte_cnt_0:32;
	a_uint32_t  rx_byte_cnt_1:8;
	a_uint32_t  _reserved0:24;
};

union vlan_cnt_tbl_u {
	a_uint32_t val[3];
	struct vlan_cnt_tbl bf;
};

/*[table] EG_VSI_COUNTER_TBL*/
#define EG_VSI_COUNTER_TBL
#define EG_VSI_COUNTER_TBL_ADDRESS 0x21000
#define EG_VSI_COUNTER_TBL_NUM     64
#define EG_VSI_COUNTER_TBL_INC     0x10
#define EG_VSI_COUNTER_TBL_TYPE    REG_TYPE_RW
#define EG_VSI_COUNTER_TBL_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define EG_VSI_COUNTER_TBL_TX_PACKETS
	#define EG_VSI_COUNTER_TBL_TX_PACKETS_OFFSET  0
	#define EG_VSI_COUNTER_TBL_TX_PACKETS_LEN     32
	#define EG_VSI_COUNTER_TBL_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define EG_VSI_COUNTER_TBL_TX_BYTES
	#define EG_VSI_COUNTER_TBL_TX_BYTES_OFFSET  32
	#define EG_VSI_COUNTER_TBL_TX_BYTES_LEN     40
	#define EG_VSI_COUNTER_TBL_TX_BYTES_DEFAULT 0x0

struct eg_vsi_counter_tbl {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union eg_vsi_counter_tbl_u {
	a_uint32_t val[3];
	struct eg_vsi_counter_tbl bf;
};

/*[table] PRE_L2_CNT_TBL*/
#define PRE_L2_CNT_TBL
#define PRE_L2_CNT_TBL_ADDRESS 0x7c000
#define PRE_L2_CNT_TBL_NUM     32
#define PRE_L2_CNT_TBL_INC     0x20
#define PRE_L2_CNT_TBL_TYPE    REG_TYPE_RW
#define PRE_L2_CNT_TBL_DEFAULT 0x0
	/*[field] RX_PKT_CNT*/
	#define PRE_L2_CNT_TBL_RX_PKT_CNT
	#define PRE_L2_CNT_TBL_RX_PKT_CNT_OFFSET  0
	#define PRE_L2_CNT_TBL_RX_PKT_CNT_LEN     32
	#define PRE_L2_CNT_TBL_RX_PKT_CNT_DEFAULT 0x0
	/*[field] RX_BYTE_CNT*/
	#define PRE_L2_CNT_TBL_RX_BYTE_CNT
	#define PRE_L2_CNT_TBL_RX_BYTE_CNT_OFFSET  32
	#define PRE_L2_CNT_TBL_RX_BYTE_CNT_LEN     40
	#define PRE_L2_CNT_TBL_RX_BYTE_CNT_DEFAULT 0x0
	/*[field] RX_DROP_PKT_CNT*/
	#define PRE_L2_CNT_TBL_RX_DROP_PKT_CNT
	#define PRE_L2_CNT_TBL_RX_DROP_PKT_CNT_OFFSET  72
	#define PRE_L2_CNT_TBL_RX_DROP_PKT_CNT_LEN     32
	#define PRE_L2_CNT_TBL_RX_DROP_PKT_CNT_DEFAULT 0x0
	/*[field] RX_DROP_BYTE_CNT*/
	#define PRE_L2_CNT_TBL_RX_DROP_BYTE_CNT
	#define PRE_L2_CNT_TBL_RX_DROP_BYTE_CNT_OFFSET  104
	#define PRE_L2_CNT_TBL_RX_DROP_BYTE_CNT_LEN     40
	#define PRE_L2_CNT_TBL_RX_DROP_BYTE_CNT_DEFAULT 0x0

struct pre_l2_cnt_tbl {
	a_uint32_t  rx_pkt_cnt:32;
	a_uint32_t  rx_byte_cnt_0:32;
	a_uint32_t  rx_byte_cnt_1:8;
	a_uint32_t  rx_drop_pkt_cnt_0:24;
	a_uint32_t  rx_drop_pkt_cnt_1:8;
	a_uint32_t  rx_drop_byte_cnt_0:24;
	a_uint32_t  rx_drop_byte_cnt_1:16;
	a_uint32_t  _reserved0:16;
};

union pre_l2_cnt_tbl_u {
	a_uint32_t val[5];
	struct pre_l2_cnt_tbl bf;
};

/*[table] L3_VP_PORT_TBL*/
#define L3_VP_PORT_TBL
#define L3_VP_PORT_TBL_ADDRESS 0x4000
#define L3_VP_PORT_TBL_NUM     256
#define L3_VP_PORT_TBL_INC     0x10
#define L3_VP_PORT_TBL_TYPE    REG_TYPE_RW
#define L3_VP_PORT_TBL_DEFAULT 0x0
	/*[field] L3_IF_VALID*/
	#define L3_VP_PORT_TBL_L3_IF_VALID
	#define L3_VP_PORT_TBL_L3_IF_VALID_OFFSET  0
	#define L3_VP_PORT_TBL_L3_IF_VALID_LEN     1
	#define L3_VP_PORT_TBL_L3_IF_VALID_DEFAULT 0x0
	/*[field] L3_IF_INDEX*/
	#define L3_VP_PORT_TBL_L3_IF_INDEX
	#define L3_VP_PORT_TBL_L3_IF_INDEX_OFFSET  1
	#define L3_VP_PORT_TBL_L3_IF_INDEX_LEN     8
	#define L3_VP_PORT_TBL_L3_IF_INDEX_DEFAULT 0x0
	/*[field] IPV4_SG_EN*/
	#define L3_VP_PORT_TBL_IPV4_SG_EN
	#define L3_VP_PORT_TBL_IPV4_SG_EN_OFFSET  9
	#define L3_VP_PORT_TBL_IPV4_SG_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV4_SG_EN_DEFAULT 0x0
	/*[field] IPV4_SG_VIO_CMD*/
	#define L3_VP_PORT_TBL_IPV4_SG_VIO_CMD
	#define L3_VP_PORT_TBL_IPV4_SG_VIO_CMD_OFFSET  10
	#define L3_VP_PORT_TBL_IPV4_SG_VIO_CMD_LEN     2
	#define L3_VP_PORT_TBL_IPV4_SG_VIO_CMD_DEFAULT 0x0
	/*[field] IPV4_SG_PORT_EN*/
	#define L3_VP_PORT_TBL_IPV4_SG_PORT_EN
	#define L3_VP_PORT_TBL_IPV4_SG_PORT_EN_OFFSET  12
	#define L3_VP_PORT_TBL_IPV4_SG_PORT_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV4_SG_PORT_EN_DEFAULT 0x0
	/*[field] IPV4_SG_SVLAN_EN*/
	#define L3_VP_PORT_TBL_IPV4_SG_SVLAN_EN
	#define L3_VP_PORT_TBL_IPV4_SG_SVLAN_EN_OFFSET  13
	#define L3_VP_PORT_TBL_IPV4_SG_SVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV4_SG_SVLAN_EN_DEFAULT 0x0
	/*[field] IPV4_SG_CVLAN_EN*/
	#define L3_VP_PORT_TBL_IPV4_SG_CVLAN_EN
	#define L3_VP_PORT_TBL_IPV4_SG_CVLAN_EN_OFFSET  14
	#define L3_VP_PORT_TBL_IPV4_SG_CVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV4_SG_CVLAN_EN_DEFAULT 0x0
	/*[field] IPV4_SRC_UNK_CMD*/
	#define L3_VP_PORT_TBL_IPV4_SRC_UNK_CMD
	#define L3_VP_PORT_TBL_IPV4_SRC_UNK_CMD_OFFSET  15
	#define L3_VP_PORT_TBL_IPV4_SRC_UNK_CMD_LEN     2
	#define L3_VP_PORT_TBL_IPV4_SRC_UNK_CMD_DEFAULT 0x0
	/*[field] IPV6_SG_EN*/
	#define L3_VP_PORT_TBL_IPV6_SG_EN
	#define L3_VP_PORT_TBL_IPV6_SG_EN_OFFSET  17
	#define L3_VP_PORT_TBL_IPV6_SG_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV6_SG_EN_DEFAULT 0x0
	/*[field] IPV6_SG_VIO_CMD*/
	#define L3_VP_PORT_TBL_IPV6_SG_VIO_CMD
	#define L3_VP_PORT_TBL_IPV6_SG_VIO_CMD_OFFSET  18
	#define L3_VP_PORT_TBL_IPV6_SG_VIO_CMD_LEN     2
	#define L3_VP_PORT_TBL_IPV6_SG_VIO_CMD_DEFAULT 0x0
	/*[field] IPV6_SG_PORT_EN*/
	#define L3_VP_PORT_TBL_IPV6_SG_PORT_EN
	#define L3_VP_PORT_TBL_IPV6_SG_PORT_EN_OFFSET  20
	#define L3_VP_PORT_TBL_IPV6_SG_PORT_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV6_SG_PORT_EN_DEFAULT 0x0
	/*[field] IPV6_SG_SVLAN_EN*/
	#define L3_VP_PORT_TBL_IPV6_SG_SVLAN_EN
	#define L3_VP_PORT_TBL_IPV6_SG_SVLAN_EN_OFFSET  21
	#define L3_VP_PORT_TBL_IPV6_SG_SVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV6_SG_SVLAN_EN_DEFAULT 0x0
	/*[field] IPV6_SG_CVLAN_EN*/
	#define L3_VP_PORT_TBL_IPV6_SG_CVLAN_EN
	#define L3_VP_PORT_TBL_IPV6_SG_CVLAN_EN_OFFSET  22
	#define L3_VP_PORT_TBL_IPV6_SG_CVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IPV6_SG_CVLAN_EN_DEFAULT 0x0
	/*[field] IPV6_SRC_UNK_CMD*/
	#define L3_VP_PORT_TBL_IPV6_SRC_UNK_CMD
	#define L3_VP_PORT_TBL_IPV6_SRC_UNK_CMD_OFFSET  23
	#define L3_VP_PORT_TBL_IPV6_SRC_UNK_CMD_LEN     2
	#define L3_VP_PORT_TBL_IPV6_SRC_UNK_CMD_DEFAULT 0x0
	/*[field] IP_ARP_SG_EN*/
	#define L3_VP_PORT_TBL_IP_ARP_SG_EN
	#define L3_VP_PORT_TBL_IP_ARP_SG_EN_OFFSET  25
	#define L3_VP_PORT_TBL_IP_ARP_SG_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ARP_SG_EN_DEFAULT 0x0
	/*[field] IP_ARP_SG_VIO_CMD*/
	#define L3_VP_PORT_TBL_IP_ARP_SG_VIO_CMD
	#define L3_VP_PORT_TBL_IP_ARP_SG_VIO_CMD_OFFSET  26
	#define L3_VP_PORT_TBL_IP_ARP_SG_VIO_CMD_LEN     2
	#define L3_VP_PORT_TBL_IP_ARP_SG_VIO_CMD_DEFAULT 0x0
	/*[field] IP_ARP_SG_PORT_EN*/
	#define L3_VP_PORT_TBL_IP_ARP_SG_PORT_EN
	#define L3_VP_PORT_TBL_IP_ARP_SG_PORT_EN_OFFSET  28
	#define L3_VP_PORT_TBL_IP_ARP_SG_PORT_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ARP_SG_PORT_EN_DEFAULT 0x0
	/*[field] IP_ARP_SG_SVLAN_EN*/
	#define L3_VP_PORT_TBL_IP_ARP_SG_SVLAN_EN
	#define L3_VP_PORT_TBL_IP_ARP_SG_SVLAN_EN_OFFSET  29
	#define L3_VP_PORT_TBL_IP_ARP_SG_SVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ARP_SG_SVLAN_EN_DEFAULT 0x0
	/*[field] IP_ARP_SRC_UNK_CMD*/
	#define L3_VP_PORT_TBL_IP_ARP_SRC_UNK_CMD
	#define L3_VP_PORT_TBL_IP_ARP_SRC_UNK_CMD_OFFSET  30
	#define L3_VP_PORT_TBL_IP_ARP_SRC_UNK_CMD_LEN     2
	#define L3_VP_PORT_TBL_IP_ARP_SRC_UNK_CMD_DEFAULT 0x0
	/*[field] IP_ARP_SG_CVLAN_EN*/
	#define L3_VP_PORT_TBL_IP_ARP_SG_CVLAN_EN
	#define L3_VP_PORT_TBL_IP_ARP_SG_CVLAN_EN_OFFSET  32
	#define L3_VP_PORT_TBL_IP_ARP_SG_CVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ARP_SG_CVLAN_EN_DEFAULT 0x0
	/*[field] IP_ND_SG_EN*/
	#define L3_VP_PORT_TBL_IP_ND_SG_EN
	#define L3_VP_PORT_TBL_IP_ND_SG_EN_OFFSET  33
	#define L3_VP_PORT_TBL_IP_ND_SG_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ND_SG_EN_DEFAULT 0x0
	/*[field] IP_ND_SG_VIO_CMD*/
	#define L3_VP_PORT_TBL_IP_ND_SG_VIO_CMD
	#define L3_VP_PORT_TBL_IP_ND_SG_VIO_CMD_OFFSET  34
	#define L3_VP_PORT_TBL_IP_ND_SG_VIO_CMD_LEN     2
	#define L3_VP_PORT_TBL_IP_ND_SG_VIO_CMD_DEFAULT 0x0
	/*[field] IP_ND_SG_PORT_EN*/
	#define L3_VP_PORT_TBL_IP_ND_SG_PORT_EN
	#define L3_VP_PORT_TBL_IP_ND_SG_PORT_EN_OFFSET  36
	#define L3_VP_PORT_TBL_IP_ND_SG_PORT_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ND_SG_PORT_EN_DEFAULT 0x0
	/*[field] IP_ND_SG_SVLAN_EN*/
	#define L3_VP_PORT_TBL_IP_ND_SG_SVLAN_EN
	#define L3_VP_PORT_TBL_IP_ND_SG_SVLAN_EN_OFFSET  37
	#define L3_VP_PORT_TBL_IP_ND_SG_SVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ND_SG_SVLAN_EN_DEFAULT 0x0
	/*[field] IP_ND_SG_CVLAN_EN*/
	#define L3_VP_PORT_TBL_IP_ND_SG_CVLAN_EN
	#define L3_VP_PORT_TBL_IP_ND_SG_CVLAN_EN_OFFSET  38
	#define L3_VP_PORT_TBL_IP_ND_SG_CVLAN_EN_LEN     1
	#define L3_VP_PORT_TBL_IP_ND_SG_CVLAN_EN_DEFAULT 0x0
	/*[field] IP_ND_SRC_UNK_CMD*/
	#define L3_VP_PORT_TBL_IP_ND_SRC_UNK_CMD
	#define L3_VP_PORT_TBL_IP_ND_SRC_UNK_CMD_OFFSET  39
	#define L3_VP_PORT_TBL_IP_ND_SRC_UNK_CMD_LEN     2
	#define L3_VP_PORT_TBL_IP_ND_SRC_UNK_CMD_DEFAULT 0x0
	/*[field] VSI_VALID*/
	#define L3_VP_PORT_TBL_VSI_VALID
	#define L3_VP_PORT_TBL_VSI_VALID_OFFSET  41
	#define L3_VP_PORT_TBL_VSI_VALID_LEN     1
	#define L3_VP_PORT_TBL_VSI_VALID_DEFAULT 0x0
	/*[field] VSI*/
	#define L3_VP_PORT_TBL_VSI
	#define L3_VP_PORT_TBL_VSI_OFFSET  42
	#define L3_VP_PORT_TBL_VSI_LEN     6
	#define L3_VP_PORT_TBL_VSI_DEFAULT 0x0
	/*[field] MAC_VALID*/
	#define L3_VP_PORT_TBL_MAC_VALID
	#define L3_VP_PORT_TBL_MAC_VALID_OFFSET  48
	#define L3_VP_PORT_TBL_MAC_VALID_LEN     1
	#define L3_VP_PORT_TBL_MAC_VALID_DEFAULT 0x0
	/*[field] MAC_DA*/
	#define L3_VP_PORT_TBL_MAC_DA
	#define L3_VP_PORT_TBL_MAC_DA_OFFSET  49
	#define L3_VP_PORT_TBL_MAC_DA_LEN     48
	#define L3_VP_PORT_TBL_MAC_DA_DEFAULT 0x0
	/*[field] IPO_VP_PROFILE*/
	#define L3_VP_PORT_TBL_IPO_VP_PROFILE
	#define L3_VP_PORT_TBL_IPO_VP_PROFILE_OFFSET  97
	#define L3_VP_PORT_TBL_IPO_VP_PROFILE_LEN     8
	#define L3_VP_PORT_TBL_IPO_VP_PROFILE_DEFAULT 0x0

struct l3_vp_port_tbl {
	a_uint32_t  l3_if_valid:1;
	a_uint32_t  l3_if_index:8;
	a_uint32_t  ipv4_sg_en:1;
	a_uint32_t  ipv4_sg_vio_cmd:2;
	a_uint32_t  ipv4_sg_port_en:1;
	a_uint32_t  ipv4_sg_svlan_en:1;
	a_uint32_t  ipv4_sg_cvlan_en:1;
	a_uint32_t  ipv4_src_unk_cmd:2;
	a_uint32_t  ipv6_sg_en:1;
	a_uint32_t  ipv6_sg_vio_cmd:2;
	a_uint32_t  ipv6_sg_port_en:1;
	a_uint32_t  ipv6_sg_svlan_en:1;
	a_uint32_t  ipv6_sg_cvlan_en:1;
	a_uint32_t  ipv6_src_unk_cmd:2;
	a_uint32_t  ip_arp_sg_en:1;
	a_uint32_t  ip_arp_sg_vio_cmd:2;
	a_uint32_t  ip_arp_sg_port_en:1;
	a_uint32_t  ip_arp_sg_svlan_en:1;
	a_uint32_t  ip_arp_src_unk_cmd:2;
	a_uint32_t  ip_arp_sg_cvlan_en:1;
	a_uint32_t  ip_nd_sg_en:1;
	a_uint32_t  ip_nd_sg_vio_cmd:2;
	a_uint32_t  ip_nd_sg_port_en:1;
	a_uint32_t  ip_nd_sg_svlan_en:1;
	a_uint32_t  ip_nd_sg_cvlan_en:1;
	a_uint32_t  ip_nd_src_unk_cmd:2;
	a_uint32_t  vsi_valid:1;
	a_uint32_t  vsi:6;
	a_uint32_t  mac_valid:1;
	a_uint32_t  mac_da_0:15;
	a_uint32_t  mac_da_1:32;
	a_uint32_t  mac_da_2:1;
	a_uint32_t  ipo_vp_profile:8;
	a_uint32_t  _reserved0:23;
};

union l3_vp_port_tbl_u {
	a_uint32_t val[4];
	struct l3_vp_port_tbl bf;
};

#endif
#endif
