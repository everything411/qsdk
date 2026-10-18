/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <linux/types.h>
#include <phy.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <malloc.h>
#include "qcom_qce2204_ppe.h"

/* Helper macro for multi-word register field modification */
#define QCE2204_FIELD_MODIFY(field, tbl_cfg, value) \
	do { \
		u32 *_cfg = (tbl_cfg); \
		u32 _val = *_cfg; \
		u64 _field = (field); \
		_val &= ~((u32)_field); \
		_val |= FIELD_PREP((u32)_field, value); \
		*_cfg = _val; \
	} while (0)

#define NSS_PPE_BASE                    0x07000000
/* Global control registers (NSS_SW_GLB_REG module) */
#define QCE2204_PPE_SWITCH_ID_ADDR                      0x00000000
#define QCE2204_PPE_SWITCH_ID_REV_ID                    GENMASK(7, 0)
#define QCE2204_PPE_SWITCH_ID_DEV_ID                    GENMASK(15, 8)
#define QCE2204_PPE_SWITCH_ID_GET_REV_ID(x)             FIELD_GET(QCE2204_PPE_SWITCH_ID_REV_ID, x)
#define QCE2204_PPE_SWITCH_ID_GET_DEVICE_ID(x)          FIELD_GET(QCE2204_PPE_SWITCH_ID_DEV_ID, x)

#define QCE2204_PPE_SWITCH_NSS_SWITCH_PORT_MUX_CTRL	0x10
#define	QCE2204_SWITCH_PORT0_MAC_SEL			BIT(8)
#define	QCE2204_SWITCH_PORT5_MAC_SEL			BIT(12)

/* PPE scheduler configurations for buffer manager block */
#define QCE2204_PPE_BM_SCH_CTRL_ADDR                    0x0000B000
#define QCE2204_PPE_BM_SCH_CTRL_INC                     4
#define QCE2204_PPE_BM_SCH_CTRL_SCH_DEPTH               GENMASK(8, 0)
#define QCE2204_PPE_BM_SCH_CTRL_SCH_OFFSET              GENMASK(23, 16)
#define QCE2204_PPE_BM_SCH_CTRL_SCH_EN                  BIT(31)

/* RSS hash configuration registers */
#define QCE2204_PPE_RSS_HASH_MASK_ADDR                  0x000B4318
#define QCE2204_PPE_RSS_HASH_MASK_HASH_MASK             GENMASK(20, 0)
#define QCE2204_PPE_RSS_HASH_MASK_FRAGMENT              BIT(28)

#define QCE2204_PPE_RSS_HASH_SEED_ADDR                  0x000B431C
#define QCE2204_PPE_RSS_HASH_SEED_VAL                   GENMASK(31, 0)

#define QCE2204_PPE_RSS_HASH_MIX_ADDR                   0x000B4320
#define QCE2204_PPE_RSS_HASH_MIX_ENTRIES                11
#define QCE2204_PPE_RSS_HASH_MIX_INC                    4
#define QCE2204_PPE_RSS_HASH_MIX_VAL                    GENMASK(4, 0)

/* RSS hash mode constants from Linux 6.6 */
#define QCE2204_PPE_RSS_HASH_MODE_IPV4                  BIT(0)
#define QCE2204_PPE_RSS_HASH_MODE_IPV6                  BIT(1)
#define QCE2204_PPE_RSS_HASH_IP_LENGTH                  4
#define QCE2204_PPE_RSS_HASH_TUPLES                     5

#define QCE2204_PPE_RSS_HASH_FIN_ADDR                   0x000B4350
#define QCE2204_PPE_RSS_HASH_FIN_ENTRIES                5
#define QCE2204_PPE_RSS_HASH_FIN_INC                    4
#define QCE2204_PPE_RSS_HASH_FIN_INNER                  GENMASK(4, 0)
#define QCE2204_PPE_RSS_HASH_FIN_OUTER                  GENMASK(9, 5)

#define QCE2204_PPE_RSS_HASH_MASK_IPV4_ADDR             0x000B4380
#define QCE2204_PPE_RSS_HASH_MASK_IPV4_HASH_MASK        GENMASK(20, 0)
#define QCE2204_PPE_RSS_HASH_MASK_IPV4_FRAGMENT         BIT(28)

#define QCE2204_PPE_RSS_HASH_SEED_IPV4_ADDR             0x000B4384
#define QCE2204_PPE_RSS_HASH_SEED_IPV4_VAL              GENMASK(31, 0)

#define QCE2204_PPE_RSS_HASH_MIX_IPV4_ADDR              0x000B4390
#define QCE2204_PPE_RSS_HASH_MIX_IPV4_ENTRIES           5
#define QCE2204_PPE_RSS_HASH_MIX_IPV4_INC               4
#define QCE2204_PPE_RSS_HASH_MIX_IPV4_VAL               GENMASK(4, 0)

#define QCE2204_PPE_RSS_HASH_FIN_IPV4_ADDR              0x000B43B0
#define QCE2204_PPE_RSS_HASH_FIN_IPV4_ENTRIES           5
#define QCE2204_PPE_RSS_HASH_FIN_IPV4_INC               4
#define QCE2204_PPE_RSS_HASH_FIN_IPV4_INNER             GENMASK(4, 0)
#define QCE2204_PPE_RSS_HASH_FIN_IPV4_OUTER             GENMASK(9, 5)

#define QCE2204_PPE_BM_SCH_CFG_TBL_ADDR                 0x0000C000
#define QCE2204_PPE_BM_SCH_CFG_TBL_ENTRIES              256
#define QCE2204_PPE_BM_SCH_CFG_TBL_INC                  0x10
#define QCE2204_PPE_BM_SCH_CFG_TBL_PORT_NUM             GENMASK(3, 0)
#define QCE2204_PPE_BM_SCH_CFG_TBL_DIR                  BIT(4)
#define QCE2204_PPE_BM_SCH_CFG_TBL_VALID                BIT(5)
#define QCE2204_PPE_BM_SCH_CFG_TBL_SECOND_PORT_VALID    BIT(6)
#define QCE2204_PPE_BM_SCH_CFG_TBL_SECOND_PORT          GENMASK(11, 8)

/* PPE service code configuration for ingress */
#define QCE2204_PPE_SERVICE_TBL_ADDR                    0x00015000
#define QCE2204_PPE_SERVICE_TBL_ENTRIES                 256
#define QCE2204_PPE_SERVICE_TBL_INC                     0x10
#define QCE2204_PPE_SERVICE_W0_BYPASS_BITMAP            GENMASK(31, 0)
#define QCE2204_PPE_SERVICE_W1_RX_CNT_EN                BIT(0)

#define QCE2204_PPE_SERVICE_SET_BYPASS_BITMAP(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_SERVICE_W0_BYPASS_BITMAP, tbl_cfg, value)
#define QCE2204_PPE_SERVICE_SET_RX_CNT_EN(tbl_cfg, value)       \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_SERVICE_W1_RX_CNT_EN, (tbl_cfg) + 0x1, value)

/* PORT_EG_VLAN - Port egress VLAN configuration table (EPE module) */
#define QCE2204_PPE_PORT_EG_VLAN_TBL_ADDR               0x00600040
#define QCE2204_PPE_PORT_EG_VLAN_TBL_ENTRIES            9
#define QCE2204_PPE_PORT_EG_VLAN_TBL_INC                0x4
#define QCE2204_PPE_PORT_EG_VLAN_TBL_PORT_VLAN_TYPE     BIT(0)
#define QCE2204_PPE_PORT_EG_VLAN_TBL_TX_COUNTING_EN     BIT(8)

/* PPE queue counters enable/disable control */
#define QCE2204_PPE_EG_BRIDGE_CONFIG_ADDR               0x00600084
#define QCE2204_PPE_EG_BRIDGE_CONFIG_PKT_L2_EDIT_EN     BIT(1)
#define QCE2204_PPE_EG_BRIDGE_CONFIG_QUEUE_CNT_EN       BIT(2)

/* PPE service code configuration on egress */
#define QCE2204_PPE_EG_SERVICE_TBL_ADDR                 0x00612000
#define QCE2204_PPE_EG_SERVICE_TBL_ENTRIES              256
#define QCE2204_PPE_EG_SERVICE_TBL_INC                  0x10
#define QCE2204_PPE_EG_SERVICE_W0_UPDATE_ACTION         GENMASK(31, 0)
#define QCE2204_PPE_EG_SERVICE_W1_NEXT_SERVCODE         GENMASK(7, 0)
#define QCE2204_PPE_EG_SERVICE_W1_HW_SERVICE            GENMASK(13, 8)
#define QCE2204_PPE_EG_SERVICE_W1_OFFSET_SEL            BIT(14)
#define QCE2204_PPE_EG_SERVICE_W1_TX_CNT_EN             BIT(15)

#define QCE2204_PPE_EG_SERVICE_SET_UPDATE_ACTION(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_EG_SERVICE_W0_UPDATE_ACTION, tbl_cfg, value)
#define QCE2204_PPE_EG_SERVICE_SET_NEXT_SERVCODE(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_EG_SERVICE_W1_NEXT_SERVCODE, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_EG_SERVICE_SET_HW_SERVICE(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_EG_SERVICE_W1_HW_SERVICE, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_EG_SERVICE_SET_OFFSET_SEL(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_EG_SERVICE_W1_OFFSET_SEL, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_EG_SERVICE_SET_TX_CNT_EN(tbl_cfg, value)    \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_EG_SERVICE_W1_TX_CNT_EN, (tbl_cfg) + 0x1, value)

/* PPE port control configurations for multicast queues */
#define QCE2204_PPE_MC_MTU_CTRL_TBL_ADDR                0x00540A00
#define QCE2204_PPE_MC_MTU_CTRL_TBL_ENTRIES             9
#define QCE2204_PPE_MC_MTU_CTRL_TBL_INC                 4
#define QCE2204_PPE_MC_MTU_CTRL_TBL_MTU                 GENMASK(13, 0)
#define QCE2204_PPE_MC_MTU_CTRL_TBL_MTU_CMD             GENMASK(15, 14)
#define QCE2204_PPE_MC_MTU_CTRL_TBL_TX_CNT_EN           BIT(16)

/* MC_ENQ_CTRL - Multicast enqueue control table (L2 module) */
#define QCE2204_PPE_MC_ENQ_CTRL_ADDR                    0x00540A80
#define QCE2204_PPE_MC_ENQ_CTRL_ENTRIES                 1
#define QCE2204_PPE_MC_ENQ_CTRL_INC                     0x4
#define QCE2204_PPE_MC_ENQ_CTRL_UC_PORT_ID              GENMASK(3, 0)
#define QCE2204_PPE_MC_ENQ_CTRL_UC_ENQ_EN               BIT(4)

#define QCE2204_PPE_MC_ENQ_CTRL_SET_UC_PORT_ID(tbl_cfg, value)  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MC_ENQ_CTRL_UC_PORT_ID, tbl_cfg, value)
#define QCE2204_PPE_MC_ENQ_CTRL_SET_UC_ENQ_EN(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MC_ENQ_CTRL_UC_ENQ_EN, tbl_cfg, value)

/* PPE VSI configurations */
#define QCE2204_PPE_VSI_TBL_ADDR                        0x00543800
#define QCE2204_PPE_VSI_TBL_ENTRIES                     64
#define QCE2204_PPE_VSI_TBL_INC                         0x10
#define QCE2204_PPE_VSI_W0_MEMBER_PORT_BITMAP           GENMASK(8, 0)
#define QCE2204_PPE_VSI_W0_UUC_BITMAP                   GENMASK(17, 9)
#define QCE2204_PPE_VSI_W0_UMC_BITMAP                   GENMASK(26, 18)
#define QCE2204_PPE_VSI_W0_BC_BITMAP_LO                 GENMASK(31, 27)
#define QCE2204_PPE_VSI_W1_BC_BITMAP_HI                 GENMASK(3, 0)
#define QCE2204_PPE_VSI_W1_NEW_ADDR_LRN_EN              BIT(4)
#define QCE2204_PPE_VSI_W1_NEW_ADDR_FWD_CMD             GENMASK(6, 5)
#define QCE2204_PPE_VSI_W1_STATION_MOVE_LRN_EN          BIT(7)
#define QCE2204_PPE_VSI_W1_STATION_MOVE_FWD_CMD         GENMASK(9, 8)

#define QCE2204_PPE_VSI_SET_MEMBER_PORT_BITMAP(tbl_cfg, value)          \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W0_MEMBER_PORT_BITMAP, tbl_cfg, value)
#define QCE2204_PPE_VSI_SET_UUC_BITMAP(tbl_cfg, value)                  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W0_UUC_BITMAP, tbl_cfg, value)
#define QCE2204_PPE_VSI_SET_UMC_BITMAP(tbl_cfg, value)                  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W0_UMC_BITMAP, tbl_cfg, value)
#define QCE2204_PPE_VSI_SET_BC_BITMAP_LO(tbl_cfg, value)                \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W0_BC_BITMAP_LO, tbl_cfg, value)
#define QCE2204_PPE_VSI_SET_BC_BITMAP_HI(tbl_cfg, value)                \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W1_BC_BITMAP_HI, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_VSI_SET_NEW_ADDR_LRN_EN(tbl_cfg, value)             \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W1_NEW_ADDR_LRN_EN, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_VSI_SET_NEW_ADDR_FWD_CMD(tbl_cfg, value)            \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W1_NEW_ADDR_FWD_CMD, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_VSI_SET_STATION_MOVE_LRN_EN(tbl_cfg, value)         \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W1_STATION_MOVE_LRN_EN, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_VSI_SET_STATION_MOVE_FWD_CMD(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_VSI_W1_STATION_MOVE_FWD_CMD, (tbl_cfg) + 0x1, value)

/* PPE port control configurations for unicast queues */
#define QCE2204_PPE_MRU_MTU_CTRL_TBL_ADDR               0x00545000
#define QCE2204_PPE_MRU_MTU_CTRL_TBL_ENTRIES            256
#define QCE2204_PPE_MRU_MTU_CTRL_TBL_INC                0x10
#define QCE2204_PPE_MRU_MTU_CTRL_W0_MRU                 GENMASK(13, 0)
#define QCE2204_PPE_MRU_MTU_CTRL_W0_MRU_CMD             GENMASK(15, 14)
#define QCE2204_PPE_MRU_MTU_CTRL_W0_MTU                 GENMASK(29, 16)
#define QCE2204_PPE_MRU_MTU_CTRL_W0_MTU_CMD             GENMASK(31, 30)
#define QCE2204_PPE_MRU_MTU_CTRL_W1_RX_CNT_EN           BIT(0)
#define QCE2204_PPE_MRU_MTU_CTRL_W1_TX_CNT_EN           BIT(1)

#define QCE2204_PPE_MRU_MTU_CTRL_SET_MRU(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MRU_MTU_CTRL_W0_MRU, tbl_cfg, value)
#define QCE2204_PPE_MRU_MTU_CTRL_SET_MRU_CMD(tbl_cfg, value)    \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MRU_MTU_CTRL_W0_MRU_CMD, tbl_cfg, value)
#define QCE2204_PPE_MRU_MTU_CTRL_SET_MTU(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MRU_MTU_CTRL_W0_MTU, tbl_cfg, value)
#define QCE2204_PPE_MRU_MTU_CTRL_SET_MTU_CMD(tbl_cfg, value)    \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MRU_MTU_CTRL_W0_MTU_CMD, tbl_cfg, value)
#define QCE2204_PPE_MRU_MTU_CTRL_SET_RX_CNT_EN(tbl_cfg, value)  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MRU_MTU_CTRL_W1_RX_CNT_EN, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_MRU_MTU_CTRL_SET_TX_CNT_EN(tbl_cfg, value)  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_MRU_MTU_CTRL_W1_TX_CNT_EN, (tbl_cfg) + 0x1, value)

/* PPE service code configuration for destination port and counter */
#define QCE2204_PPE_IN_L2_SERVICE_TBL_ADDR              0x00546000
#define QCE2204_PPE_IN_L2_SERVICE_TBL_ENTRIES           256
#define QCE2204_PPE_IN_L2_SERVICE_TBL_INC               0x20
#define QCE2204_PPE_IN_L2_SERVICE_TBL_DST_PORT_ID_VALID BIT(0)
#define QCE2204_PPE_IN_L2_SERVICE_TBL_DST_PORT_ID       GENMASK(4, 1)
#define QCE2204_PPE_IN_L2_SERVICE_TBL_DST_DIRECTION     BIT(5)
#define QCE2204_PPE_IN_L2_SERVICE_TBL_DST_BYPASS_BITMAP GENMASK(29, 6)
#define QCE2204_PPE_IN_L2_SERVICE_TBL_RX_CNT_EN         BIT(30)
#define QCE2204_PPE_IN_L2_SERVICE_TBL_TX_CNT_EN         BIT(31)

/* L2 Port configurations */
#define QCE2204_PPE_L2_VP_PORT_TBL_ADDR                 0x00563000
#define QCE2204_PPE_L2_VP_PORT_TBL_ENTRIES              256
#define QCE2204_PPE_L2_VP_PORT_TBL_INC                  0x10
#define QCE2204_PPE_L2_VP_PORT_W0_INVALID_VSI_FWD_EN    BIT(0)
#define QCE2204_PPE_L2_VP_PORT_W0_DST_INFO              GENMASK(9, 2)

#define QCE2204_PPE_L2_PORT_SET_INVALID_VSI_FWD_EN(tbl_cfg, value)      \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L2_VP_PORT_W0_INVALID_VSI_FWD_EN, tbl_cfg, value)
#define QCE2204_PPE_L2_PORT_SET_DST_INFO(tbl_cfg, value)                \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L2_VP_PORT_W0_DST_INFO, tbl_cfg, value)

/* L3_VP_PORT_TBL - L3 Virtual Port table (L3 module) */
#define QCE2204_PPE_L3_VP_PORT_TBL_ADDR                 0x00204000
#define QCE2204_PPE_L3_VP_PORT_TBL_ENTRIES              256
#define QCE2204_PPE_L3_VP_PORT_TBL_INC                  0x10
#define QCE2204_PPE_L3_VP_PORT_W1_VSI_VALID             BIT(9)
#define QCE2204_PPE_L3_VP_PORT_W1_VSI                   GENMASK(15, 10)

#define QCE2204_PPE_L3_VP_PORT_SET_VSI_VALID(tbl_cfg, value)    \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L3_VP_PORT_W1_VSI_VALID, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L3_VP_PORT_SET_VSI(tbl_cfg, value)          \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L3_VP_PORT_W1_VSI, (tbl_cfg) + 0x1, value)

/* Port scheduler global config */
#define QCE2204_PPE_PSCH_SCH_DEPTH_CFG_ADDR             0x00400000
#define QCE2204_PPE_PSCH_SCH_DEPTH_CFG_INC              4
#define QCE2204_PPE_PSCH_SCH_DEPTH_CFG_SCH_DEPTH        GENMASK(7, 0)

/* PPE queue level scheduler configurations */
#define QCE2204_PPE_L0_FLOW_MAP_TBL_ADDR                0x00402000
#define QCE2204_PPE_L0_FLOW_MAP_TBL_ENTRIES             300
#define QCE2204_PPE_L0_FLOW_MAP_TBL_INC                 0x10
#define QCE2204_PPE_L0_FLOW_MAP_TBL_FLOW_ID             GENMASK(5, 0)
#define QCE2204_PPE_L0_FLOW_MAP_TBL_C_PRI               GENMASK(8, 6)
#define QCE2204_PPE_L0_FLOW_MAP_TBL_E_PRI               GENMASK(11, 9)
#define QCE2204_PPE_L0_FLOW_MAP_TBL_C_NODE_WT           GENMASK(21, 12)
#define QCE2204_PPE_L0_FLOW_MAP_TBL_E_NODE_WT           GENMASK(31, 22)
#define QCE2204_PPE_L0_FLOW_MAP_W1_C_DRR_ID             GENMASK(7, 0)
#define QCE2204_PPE_L0_FLOW_MAP_W1_E_DRR_ID             GENMASK(15, 8)
#define QCE2204_PPE_L0_FLOW_MAP_W1_C_DRR_CREDIT_UNIT    BIT(16)
#define QCE2204_PPE_L0_FLOW_MAP_W1_E_DRR_CREDIT_UNIT    BIT(17)

#define QCE2204_PPE_L0_FLOW_MAP_SET_C_DRR_ID(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L0_FLOW_MAP_W1_C_DRR_ID, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L0_FLOW_MAP_SET_E_DRR_ID(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L0_FLOW_MAP_W1_E_DRR_ID, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L0_FLOW_MAP_SET_C_DRR_CREDIT_UNIT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L0_FLOW_MAP_W1_C_DRR_CREDIT_UNIT, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L0_FLOW_MAP_SET_E_DRR_CREDIT_UNIT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L0_FLOW_MAP_W1_E_DRR_CREDIT_UNIT, (tbl_cfg) + 0x1, value)

#define QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_ADDR           0x00408000
#define QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_ENTRIES        300
#define QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_INC            0x10
#define QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_PORT_NUM       GENMASK(3, 0)

#define QCE2204_PPE_L0_COMP_CFG_TBL_ADDR                0x00428000
#define QCE2204_PPE_L0_COMP_CFG_TBL_ENTRIES             300
#define QCE2204_PPE_L0_COMP_CFG_TBL_INC                 0x10
#define QCE2204_PPE_L0_COMP_CFG_TBL_NODE_METER_LEN      GENMASK(3, 2)

/* PPE queue to Ethernet DMA ring mapping table */
#define QCE2204_PPE_RING_Q_MAP_TBL_ADDR                 0x00434000
#define QCE2204_PPE_RING_Q_MAP_TBL_ENTRIES              175
#define QCE2204_PPE_RING_Q_MAP_TBL_INC                  0x40

/* Table addresses for per-queue dequeue setting */
#define QCE2204_PPE_DEQ_OPR_TBL_ADDR                    0x00430000
#define QCE2204_PPE_DEQ_OPR_TBL_ENTRIES                 300
#define QCE2204_PPE_DEQ_OPR_TBL_INC                     0x10
#define QCE2204_PPE_DEQ_OPR_TBL_DEQ_DISABLE             BIT(0)

/* PPE flow level scheduler configurations */
#define QCE2204_PPE_L1_FLOW_MAP_TBL_ADDR                0x00440000
#define QCE2204_PPE_L1_FLOW_MAP_TBL_ENTRIES             64
#define QCE2204_PPE_L1_FLOW_MAP_TBL_INC                 0x10
#define QCE2204_PPE_L1_FLOW_MAP_TBL_FLOW_ID     GENMASK(3, 0)
#define QCE2204_PPE_L1_FLOW_MAP_TBL_C_PRI       GENMASK(6, 4)
#define QCE2204_PPE_L1_FLOW_MAP_TBL_E_PRI       GENMASK(9, 7)
#define QCE2204_PPE_L1_FLOW_MAP_TBL_C_NODE_WT       GENMASK(19, 10)
#define QCE2204_PPE_L1_FLOW_MAP_TBL_E_NODE_WT       GENMASK(29, 20)
#define QCE2204_PPE_L1_FLOW_MAP_W0_C_DRR_ID_LO      GENMASK(31, 30)
#define QCE2204_PPE_L1_FLOW_MAP_W1_C_DRR_ID_HI      GENMASK(3, 0)
#define QCE2204_PPE_L1_FLOW_MAP_W1_E_DRR_ID     GENMASK(9, 4)
#define QCE2204_PPE_L1_FLOW_MAP_W1_C_DRR_CREDIT_UNIT    BIT(10)
#define QCE2204_PPE_L1_FLOW_MAP_W1_E_DRR_CREDIT_UNIT    BIT(11)

#define QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_ID_LO(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W0_C_DRR_ID_LO, tbl_cfg, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_ID_HI(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_C_DRR_ID_HI, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_E_DRR_ID(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_E_DRR_ID, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_CREDIT_UNIT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_C_DRR_CREDIT_UNIT, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_E_DRR_CREDIT_UNIT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_E_DRR_CREDIT_UNIT, (tbl_cfg) + 0x1, value)

#define QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_ID(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_C_DRR_ID, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_E_DRR_ID(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_E_DRR_ID, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_CREDIT_UNIT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_C_DRR_CREDIT_UNIT, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_L1_FLOW_MAP_SET_E_DRR_CREDIT_UNIT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_L1_FLOW_MAP_W1_E_DRR_CREDIT_UNIT, (tbl_cfg) + 0x1, value)

#define QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_ADDR           0x00446000
#define QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_ENTRIES        64
#define QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_INC            0x10
#define QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_PORT_NUM       GENMASK(3, 0)

#define QCE2204_PPE_L1_COMP_CFG_TBL_ADDR                0x0046A000
#define QCE2204_PPE_L1_COMP_CFG_TBL_ENTRIES             64
#define QCE2204_PPE_L1_COMP_CFG_TBL_INC                 0x10
#define QCE2204_PPE_L1_COMP_CFG_TBL_NODE_METER_LEN      GENMASK(3, 2)

/* PPE port scheduler configurations for egress */
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_ADDR               0x0047A000
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_ENTRIES            128
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_INC                0x10
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_DES_PORT           GENMASK(3, 0)
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_ENS_PORT           GENMASK(7, 4)
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_ENS_PORT_BITMAP    GENMASK(16, 8)
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_DES_SECOND_PORT_EN BIT(17)
#define QCE2204_PPE_PSCH_SCH_CFG_TBL_DES_SECOND_PORT    GENMASK(21, 18)

#define QCE2204_PPE_BM_PORT_GROUP_ID_ADDR               0x00800240
#define QCE2204_PPE_BM_PORT_GROUP_ID_ENTRIES            6
#define QCE2204_PPE_BM_PORT_GROUP_ID_INC                0x4
#define QCE2204_PPE_BM_PORT_GROUP_ID_SHARED_GROUP_ID    GENMASK(1, 0)

#define QCE2204_PPE_BM_SHARED_GROUP_CFG_ADDR            0x00800460
#define QCE2204_PPE_BM_SHARED_GROUP_CFG_ENTRIES         4
#define QCE2204_PPE_BM_SHARED_GROUP_CFG_INC             0x4
#define QCE2204_PPE_BM_SHARED_GROUP_CFG_SHARED_LIMIT    GENMASK(12, 0)

#define QCE2204_PPE_SWITCH_NSS_SWITCH_BM_PORT_FC_MODE	0x800100

#define QCE2204_PPE_BM_PORT_FC_CFG_TBL_ADDR             0x00801000
#define QCE2204_PPE_BM_PORT_FC_CFG_TBL_ENTRIES          6
#define QCE2204_PPE_BM_PORT_FC_CFG_TBL_INC              0x10
#define QCE2204_PPE_BM_PORT_FC_W0_REACT_LIMIT           GENMASK(9, 0)
#define QCE2204_PPE_BM_PORT_FC_W0_RESUME_THRESHOLD      GENMASK(18, 10)
#define QCE2204_PPE_BM_PORT_FC_W0_RESUME_OFFSET         GENMASK(30, 19)
#define QCE2204_PPE_BM_PORT_FC_W0_CEILING_LOW           BIT(31)
#define QCE2204_PPE_BM_PORT_FC_W1_CEILING_HIGH          GENMASK(10, 0)
#define QCE2204_PPE_BM_PORT_FC_W1_WEIGHT                GENMASK(13, 11)
#define QCE2204_PPE_BM_PORT_FC_W1_DYNAMIC               BIT(14)
#define QCE2204_PPE_BM_PORT_FC_W1_PRE_ALLOC             GENMASK(26, 15)

#define QCE2204_PPE_BM_PORT_FC_SET_REACT_LIMIT(tbl_cfg, value)  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W0_REACT_LIMIT, tbl_cfg, value)
#define QCE2204_PPE_BM_PORT_FC_SET_RESUME_THRESHOLD(tbl_cfg, value)     \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W0_RESUME_THRESHOLD, tbl_cfg, value)
#define QCE2204_PPE_BM_PORT_FC_SET_RESUME_OFFSET(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W0_RESUME_OFFSET, tbl_cfg, value)
#define QCE2204_PPE_BM_PORT_FC_SET_CEILING_LOW(tbl_cfg, value)  \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W0_CEILING_LOW, tbl_cfg, value)
#define QCE2204_PPE_BM_PORT_FC_SET_CEILING_HIGH(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W1_CEILING_HIGH, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_BM_PORT_FC_SET_WEIGHT(tbl_cfg, value)       \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W1_WEIGHT, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_BM_PORT_FC_SET_DYNAMIC(tbl_cfg, value)      \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W1_DYNAMIC, (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_BM_PORT_FC_SET_PRE_ALLOC(tbl_cfg, value)    \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_BM_PORT_FC_W1_PRE_ALLOC, (tbl_cfg) + 0x1, value)

/* Queue base configurations */
#define QCE2204_PPE_UCAST_QUEUE_MAP_TBL_ADDR            0x00A10000
#define QCE2204_PPE_UCAST_QUEUE_MAP_TBL_ENTRIES         3072
#define QCE2204_PPE_UCAST_QUEUE_MAP_TBL_INC             0x10
#define QCE2204_PPE_UCAST_QUEUE_MAP_TBL_PROFILE_ID      GENMASK(3, 0)
#define QCE2204_PPE_UCAST_QUEUE_MAP_TBL_QUEUE_ID        GENMASK(11, 4)

/* Queue offset configurations based on RSS hash */
#define QCE2204_PPE_UCAST_HASH_MAP_TBL_ADDR             0x00A30000
#define QCE2204_PPE_UCAST_HASH_MAP_TBL_ENTRIES          4096
#define QCE2204_PPE_UCAST_HASH_MAP_TBL_INC              0x10
#define QCE2204_PPE_UCAST_HASH_MAP_TBL_HASH             GENMASK(7, 0)

/* Queue offset configurations based on PPE internal priority */
#define QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_ADDR         0x00A42000
#define QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_ENTRIES      256
#define QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_INC          0x10
#define QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_CLASS        GENMASK(3, 0)

/* PPE unicast queue (0-255) configurations */
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_TBL_ADDR       0x00A48000
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_TBL_ENTRIES    256
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_TBL_INC        0x20
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_EN          BIT(0)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_GRP_ID      GENMASK(5, 4)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_PRE_LIMIT   GENMASK(17, 6)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_DYNAMIC     BIT(18)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_WEIGHT      GENMASK(21, 19)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_THRESHOLD_LO GENMASK(31, 22)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W1_THRESHOLD_HI GENMASK(1, 0)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W3_GRN_RESUME_LO GENMASK(31, 22)
#define QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W4_GRN_RESUME_HI GENMASK(1, 0)

#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_EN(tbl_cfg, value)     \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_EN, tbl_cfg, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_GRP_ID(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_GRP_ID, tbl_cfg, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_PRE_LIMIT(tbl_cfg, value)      \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_PRE_LIMIT, tbl_cfg, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_DYNAMIC(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_DYNAMIC, tbl_cfg, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_WEIGHT(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_WEIGHT, tbl_cfg, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_THRESHOLD_LO(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W0_THRESHOLD_LO, tbl_cfg, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_THRESHOLD_HI(tbl_cfg, value) \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W1_THRESHOLD_HI, \
			     (tbl_cfg) + 0x1, value)
#define QCE2204_PPE_AC_UNICAST_QUEUE_SET_GRN_RESUME(tbl_cfg, value)     \
	do { \
		QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W3_GRN_RESUME_LO, \
				     (tbl_cfg) + 0x3, (value) & 0x3FF); \
		QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_UNICAST_QUEUE_CFG_W4_GRN_RESUME_HI, \
				     (tbl_cfg) + 0x4, ((value) >> 10) & 0x3); \
	} while (0)

/* PPE multicast queue (256-299) configurations */
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_TBL_ADDR     0x00A50000
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_TBL_ENTRIES  44
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_TBL_INC      0x10
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_EN        BIT(0)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_GRP_ID    GENMASK(4, 3)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_PRE_LIMIT GENMASK(16, 5)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_THRESHOLD GENMASK(28, 17)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W2_RESUME    GENMASK(24, 13)

#define QCE2204_PPE_AC_MULTICAST_QUEUE_SET_EN(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_EN, tbl_cfg, value)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_GRP_ID(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_GRP_ID, tbl_cfg, value)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_PRE_LIMIT(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_PRE_LIMIT, tbl_cfg, value)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_THRESHOLD(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W0_THRESHOLD, tbl_cfg, value)
#define QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_RESUME(tbl_cfg, value)   \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_W2_RESUME, (tbl_cfg) + 0x2, value)

/* PPE admission control group (0-3) configurations */
#define QCE2204_PPE_AC_GRP_CFG_TBL_ADDR                 0x00A51000
#define QCE2204_PPE_AC_GRP_CFG_TBL_ENTRIES              0x4
#define QCE2204_PPE_AC_GRP_CFG_TBL_INC                  0x10
#define QCE2204_PPE_AC_GRP_W1_BUF_LIMIT                 GENMASK(18, 7)

#define QCE2204_PPE_AC_GRP_SET_BUF_LIMIT(tbl_cfg, value)        \
	QCE2204_FIELD_MODIFY(QCE2204_PPE_AC_GRP_W1_BUF_LIMIT, (tbl_cfg) + 0x1, value)

/* Table addresses for per-queue enqueue setting */
#define QCE2204_PPE_ENQ_OPR_TBL_ADDR                    0x00A5C000
#define QCE2204_PPE_ENQ_OPR_TBL_ENTRIES                 300
#define QCE2204_PPE_ENQ_OPR_TBL_INC                     0x10
#define QCE2204_PPE_ENQ_OPR_TBL_ENQ_DISABLE             BIT(0)

/* Address offset for PPE access */
#define QCE2204_PPE_ADDR_OFFSET                       0x07000000
#define QCE2204_PPE_RING_TO_QUEUE_BITMAP_WORD_CNT      8

/**
 * enum qce2204_ppe_scheduler_frame_mode - PPE scheduler frame mode
 * @QCE2204_PPE_SCH_WITH_IPG_PREAMBLE_FRAME_CRC: Frame with IPG, preamble, and CRC
 * @QCE2204_PPE_SCH_WITH_FRAME_CRC: Frame with CRC only
 * @QCE2204_PPE_SCH_WITH_L3_PAYLOAD: L3 payload only
 */
enum qce2204_ppe_scheduler_frame_mode {
	QCE2204_PPE_SCH_WITH_IPG_PREAMBLE_FRAME_CRC = 0,
	QCE2204_PPE_SCH_WITH_FRAME_CRC = 1,
	QCE2204_PPE_SCH_WITH_L3_PAYLOAD = 2,
};

/* Configuration data from Linux - Complete sets */
static const int qce2204_ppe_bm_group_config = 3550;

static const struct qce2204_ppe_bm_port_config qce2204_ppe_bm_port_config[] = {
	{
		.port_id_start  = 0,
		.port_id_end    = 0,
		.pre_alloc      = 0,
		.in_fly_buf     = 200,
		.ceil           = 1200,
		.weight         = 7,
		.resume_offset  = 8,
		.resume_ceil    = 0,
		.dynamic        = true,
	},
	{
		.port_id_start  = 1,
		.port_id_end    = 5,
		.pre_alloc      = 0,
		.in_fly_buf     = 228,
		.ceil           = 650,
		.weight         = 7,
		.resume_offset  = 36,
		.resume_ceil    = 0,
		.dynamic        = true,
	},
};

static const int qce2204_ppe_qm_group_config = 4000;

static const struct qce2204_ppe_qm_queue_config qce2204_ppe_qm_queue_config[] = {
	{
	/* Unicast queues 0-255 */
		.queue_start    = 0,
		.queue_end      = 255,
		.prealloc_buf   = 0,
		.ceil           = 2200,
		.weight         = 7,
		.resume_offset  = 36,
		.dynamic        = true,
	},
	{
	/* Multicast queues 256-291 */
		.queue_start    = 256,
		.queue_end      = 291,
		.prealloc_buf   = 0,
		.ceil           = 250,
		.weight         = 0,
		.resume_offset  = 36,
		.dynamic        = false,
	},
};

static const struct qce2204_ppe_scheduler_bm_config qce2204_ppe_sch_bm_config[] = {
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 1, false, 0},
	{true, QCE2204_PPE_EGRESS,  1, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 2, false, 0},
	{true, QCE2204_PPE_EGRESS,  2, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 3, false, 0},
	{true, QCE2204_PPE_EGRESS,  3, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 4, false, 0},
	{true, QCE2204_PPE_EGRESS,  4, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 1, false, 0},
	{true, QCE2204_PPE_EGRESS,  1, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 2, false, 0},
	{true, QCE2204_PPE_EGRESS,  2, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 3, false, 0},
	{true, QCE2204_PPE_EGRESS,  3, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 4, false, 0},
	{true, QCE2204_PPE_EGRESS,  4, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 1, false, 0},
	{true, QCE2204_PPE_EGRESS,  1, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 2, false, 0},
	{true, QCE2204_PPE_EGRESS,  2, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 3, false, 0},
	{true, QCE2204_PPE_EGRESS,  3, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
	{true, QCE2204_PPE_INGRESS, 4, false, 0},
	{true, QCE2204_PPE_EGRESS,  4, false, 0},
	{true, QCE2204_PPE_INGRESS, 0, false, 0},
	{true, QCE2204_PPE_EGRESS,  0, false, 0},
	{true, QCE2204_PPE_INGRESS, 5, false, 0},
	{true, QCE2204_PPE_EGRESS,  5, false, 0},
};

static const struct qce2204_ppe_scheduler_qm_config qce2204_ppe_sch_qm_config[] = {
	{0x1E, 0xF, 0x0, false, 0x0},
	{0x1C, 0xF, 0x5, false, 0x0},
	{0x1C, 0x0, 0x1, false, 0x0},
	{0x1C, 0xF, 0x5, false, 0x0},
	{0x1A, 0x1, 0x0, false, 0x0},
	{0x1A, 0x5, 0x2, false, 0x0},
	{0x1A, 0xF, 0x0, false, 0x0},
	{0x16, 0x2, 0x5, false, 0x0},
	{0x16, 0x0, 0x3, false, 0x0},
	{0x16, 0xF, 0x5, false, 0x0},
	{0x0E, 0x3, 0x0, false, 0x0},
	{0x0E, 0x5, 0x4, false, 0x0},
	{0x0E, 0xF, 0x0, false, 0x0},
	{0x1C, 0x4, 0x5, false, 0x0},
	{0x1C, 0x0, 0x1, false, 0x0},
	{0x1C, 0xF, 0x5, false, 0x0},
	{0x1A, 0x1, 0x0, false, 0x0},
	{0x1A, 0x5, 0x2, false, 0x0},
	{0x1A, 0xF, 0x0, false, 0x0},
	{0x16, 0x2, 0x5, false, 0x0},
	{0x16, 0x0, 0x3, false, 0x0},
	{0x16, 0xF, 0x5, false, 0x0},
	{0x0E, 0x3, 0x0, false, 0x0},
	{0x0E, 0x5, 0x4, false, 0x0},
	{0x0E, 0xF, 0x0, false, 0x0},
	{0x1C, 0x4, 0x5, false, 0x0},
	{0x1C, 0x0, 0x1, false, 0x0},
	{0x1C, 0xF, 0x5, false, 0x0},
	{0x1A, 0x1, 0x0, false, 0x0},
	{0x1A, 0x5, 0x2, false, 0x0},
	{0x1A, 0xF, 0x0, false, 0x0},
	{0x16, 0x2, 0x5, false, 0x0},
	{0x16, 0x0, 0x3, false, 0x0},
	{0x16, 0xF, 0x5, false, 0x0},
	{0x1E, 0x3, 0x0, false, 0x0},
	{0x1E, 0xF, 0x5, false, 0x0},
	{0x0E, 0xF, 0x0, false, 0x0},
	{0x0E, 0x5, 0x4, false, 0x0},
	{0x0E, 0xF, 0x0, false, 0x0},
	{0x1E, 0x4, 0x5, false, 0x0},
};

static const struct qce2204_ppe_port_schedule_resource qce2204_ppe_scheduler_res[] = {
	{   /* Port 0: CPU */
		.ucastq_start   = 128,
		.ucastq_end     = 135,
		.mcastq_start   = 256,
		.mcastq_end     = 263,
		.flow_id_start  = 26,
		.flow_id_end    = 28,
		.l0node_start   = 128,
		.l0node_end     = 135,
		.l1node_start   = 26,
		.l1node_end     = 28,
	},
	{   /* Port 1 */
		.ucastq_start   = 0,
		.ucastq_end     = 7,
		.mcastq_start   = 272,
		.mcastq_end     = 275,
		.flow_id_start  = 0,
		.flow_id_end    = 1,
		.l0node_start   = 0,
		.l0node_end     = 7,
		.l1node_start   = 0,
		.l1node_end     = 1,
	},
	{   /* Port 2 */
		.ucastq_start   = 8,
		.ucastq_end     = 15,
		.mcastq_start   = 276,
		.mcastq_end     = 279,
		.flow_id_start  = 2,
		.flow_id_end    = 3,
		.l0node_start   = 8,
		.l0node_end     = 15,
		.l1node_start   = 2,
		.l1node_end     = 3,
	},
	{   /* Port 3 */
		.ucastq_start   = 16,
		.ucastq_end     = 23,
		.mcastq_start   = 280,
		.mcastq_end     = 283,
		.flow_id_start  = 4,
		.flow_id_end    = 5,
		.l0node_start   = 16,
		.l0node_end     = 23,
		.l1node_start   = 4,
		.l1node_end     = 5,
	},
	{   /* Port 4 */
		.ucastq_start   = 24,
		.ucastq_end     = 31,
		.mcastq_start   = 284,
		.mcastq_end     = 287,
		.flow_id_start  = 6,
		.flow_id_end    = 7,
		.l0node_start   = 24,
		.l0node_end     = 31,
		.l1node_start   = 6,
		.l1node_end     = 7,
	},
	{   /* Port 5 */
		.ucastq_start   = 32,
		.ucastq_end     = 39,
		.mcastq_start   = 288,
		.mcastq_end     = 291,
		.flow_id_start  = 8,
		.flow_id_end    = 9,
		.l0node_start   = 32,
		.l0node_end     = 39,
		.l1node_start   = 8,
		.l1node_end     = 9,
	},
};

static const struct qce2204_ppe_scheduler_port_config qce2204_ppe_port_sch_config[] = {
	{
		.port       = 0,
		.flow_level = true,
		.node_id    = 26,
		.loop_num   = 2,
		.pri_max    = 1,
		.flow_id    = 0,
		.drr_node_id = 26,
	},
	{
		.port       = 0,
		.flow_level = false,
		.node_id    = 128,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 26,
		.drr_node_id = 128,
	},
	{
		.port       = 0,
		.flow_level = false,
		.node_id    = 256,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 26,
		.drr_node_id = 128,
	},
	{
		.port       = 1,
		.flow_level = true,
		.node_id    = 0,
		.loop_num   = 1,
		.pri_max    = 0,
		.flow_id    = 1,
		.drr_node_id = 0,
	},
	{
		.port       = 1,
		.flow_level = false,
		.node_id    = 0,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 0,
		.drr_node_id = 0,
	},
	{
		.port       = 1,
		.flow_level = false,
		.node_id    = 272,
		.loop_num   = 4,
		.pri_max    = 8,
		.flow_id    = 0,
		.drr_node_id = 0,
	},
	{
		.port       = 2,
		.flow_level = true,
		.node_id    = 2,
		.loop_num   = 1,
		.pri_max    = 0,
		.flow_id    = 2,
		.drr_node_id = 2,
	},
	{
		.port       = 2,
		.flow_level = false,
		.node_id    = 8,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 2,
		.drr_node_id = 8,
	},
	{
		.port       = 2,
		.flow_level = false,
		.node_id    = 276,
		.loop_num   = 4,
		.pri_max    = 8,
		.flow_id    = 2,
		.drr_node_id = 8,
	},
	{
		.port       = 3,
		.flow_level = true,
		.node_id    = 4,
		.loop_num   = 1,
		.pri_max    = 0,
		.flow_id    = 3,
		.drr_node_id = 4,
	},
	{
		.port       = 3,
		.flow_level = false,
		.node_id    = 16,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 4,
		.drr_node_id = 16,
	},
	{
		.port       = 3,
		.flow_level = false,
		.node_id    = 280,
		.loop_num   = 4,
		.pri_max    = 8,
		.flow_id    = 4,
		.drr_node_id = 16,
	},
	{
		.port       = 4,
		.flow_level = true,
		.node_id    = 6,
		.loop_num   = 1,
		.pri_max    = 0,
		.flow_id    = 4,
		.drr_node_id = 6,
	},
	{
		.port       = 4,
		.flow_level = false,
		.node_id    = 24,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 6,
		.drr_node_id = 24,
	},
	{
		.port       = 4,
		.flow_level = false,
		.node_id    = 284,
		.loop_num   = 4,
		.pri_max    = 8,
		.flow_id    = 6,
		.drr_node_id = 24,
	},
	{
		.port       = 5,
		.flow_level = true,
		.node_id    = 8,
		.loop_num   = 1,
		.pri_max    = 0,
		.flow_id    = 5,
		.drr_node_id = 8,
	},
	{
		.port       = 5,
		.flow_level = false,
		.node_id    = 32,
		.loop_num   = 8,
		.pri_max    = 8,
		.flow_id    = 8,
		.drr_node_id = 32,
	},
	{
		.port       = 5,
		.flow_level = false,
		.node_id    = 288,
		.loop_num   = 4,
		.pri_max    = 8,
		.flow_id    = 8,
		.drr_node_id = 32,
	},
};

static void qce1204_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid, u16 *reg_high)
{
	*reg_low = (regaddr & 0xc) << 1;
	*reg_mid = regaddr >> 4 & 0xffff;
	*reg_high = ((regaddr >> 20) & 0xf) << 1 | BIT(0);
}

u32 qce1204_soc_ppe_read(struct phy_device *phydev, u32 reg)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;
	struct phy_device local_phydev;

	addr = 7;
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = addr;

	qce1204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	/* Write AHB address bit4~bit23 */
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(100);

	/* Write AHB address bit0~bit3 and read low 16bit data */
	lo = phy_read(&local_phydev, MDIO_DEVAD_NONE, reg_low);

	/* Write AHB address bit0~bit3 and read high 16 bit data */
	hi = phy_read(&local_phydev, MDIO_DEVAD_NONE, reg_low | BIT(2));
	return (hi << 16) | lo;
}

int qce1204_soc_ppe_write(struct phy_device *phydev, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;
	struct phy_device local_phydev;

	addr = 7;
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = addr;

	qce1204_split_addr(reg, &reg_low, &reg_mid, &reg_high);
	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	/* Write AHB address bit4~bit23 */
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(100);

	/* Write AHB address bit0~bit3 and write low 16 bit data */
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_low, lo);

	/* Write AHB address bit0~bit3 and write high 16 bit data */
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_low | BIT(2), hi);

	return 0;
}

/**
 * qce2204_ppe_read - Read PPE register via SOC window (equivalent to regmap_read)
 * @phydev: PHY device
 * @reg: Register address
 */
u32 qce2204_ppe_read(struct phy_device *phydev, u32 reg)
{
	u32 val;
	u32 soc_addr;
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = 7;

	/* Use SOC window access with PPE base offset */
	soc_addr = QCE2204_PPE_ADDR_OFFSET  + reg;
	val = qce1204_soc_ppe_read(&local_phydev, soc_addr);

	return val;
}

/**
 * qce2204_ppe_write - Write PPE register via SOC window (equivalent to regmap_write)
 * @phydev: PHY device
 * @reg: Register address
 * @val: Value to write
 */
int qce2204_ppe_write(struct phy_device *phydev, u32 reg, u32 val)
{
	int ret;
	u32 soc_addr;
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = 7;

	/* Use SOC window access with PPE base offset */
	soc_addr = QCE2204_PPE_ADDR_OFFSET + reg;


	ret = qce1204_soc_ppe_write(&local_phydev, soc_addr, val);
	
	return ret;
}

/**
 * qce2204_ppe_bulk_read - Bulk read PPE registers (equivalent to regmap_bulk_read)
 * @phydev: PHY device
 * @reg: Starting register address
 * @val: Buffer to store values
 * @val_count: Number of registers to read
 */
int qce2204_ppe_bulk_read(struct phy_device *phydev, u32 reg, u32 *val, size_t val_count)
{
	int i;


	for (i = 0; i < val_count; i++)
		val[i] = qce2204_ppe_read(phydev, reg + (i * 4));
	return 0;
}

/**
 * qce2204_ppe_bulk_write - Bulk write PPE registers (equivalent to regmap_bulk_write)
 * @phydev: PHY device
 * @reg: Starting register address
 * @val: Buffer containing values
 * @val_count: Number of registers to write
 */
int qce2204_ppe_bulk_write(struct phy_device *phydev, u32 reg, const u32 *val, size_t val_count)
{
	int i, ret;


	for (i = 0; i < val_count; i++) {
		ret = qce2204_ppe_write(phydev, reg + (i * 4), val[i]);
		if (ret)
			return ret;
	}
	return 0;
}

/* All Linux PPE functions ported to U-Boot - Complete set */

/**
 * qce2204_ppe_scheduler_l0_queue_map_set - Set PPE queue level scheduler configuration (from Linux)
 */
static int qce2204_ppe_scheduler_l0_queue_map_set(struct phy_device *phydev,
						   int node_id, int port,
						   struct qce2204_ppe_scheduler_cfg scheduler_cfg)
{
	u32 val, reg, flow_map_val[4] = {};
	int ret;

	reg = QCE2204_PPE_L0_FLOW_MAP_TBL_ADDR +
	      node_id * QCE2204_PPE_L0_FLOW_MAP_TBL_INC;

	/* Read current values */
	ret = qce2204_ppe_bulk_read(phydev, reg, flow_map_val, ARRAY_SIZE(flow_map_val));
	if (ret)
		return ret;

	/* Set Word 0 fields */
	val = FIELD_PREP(QCE2204_PPE_L0_FLOW_MAP_TBL_FLOW_ID, scheduler_cfg.flow_id);
	val |= FIELD_PREP(QCE2204_PPE_L0_FLOW_MAP_TBL_C_PRI, scheduler_cfg.pri);
	val |= FIELD_PREP(QCE2204_PPE_L0_FLOW_MAP_TBL_E_PRI, scheduler_cfg.pri);
	val |= FIELD_PREP(QCE2204_PPE_L0_FLOW_MAP_TBL_C_NODE_WT, scheduler_cfg.drr_node_wt);
	val |= FIELD_PREP(QCE2204_PPE_L0_FLOW_MAP_TBL_E_NODE_WT, scheduler_cfg.drr_node_wt);
	flow_map_val[0] = val;

	/* Set Word 1 fields using new macros */
	QCE2204_PPE_L0_FLOW_MAP_SET_C_DRR_ID(flow_map_val, scheduler_cfg.drr_node_id);
	QCE2204_PPE_L0_FLOW_MAP_SET_E_DRR_ID(flow_map_val, scheduler_cfg.drr_node_id);
	QCE2204_PPE_L0_FLOW_MAP_SET_C_DRR_CREDIT_UNIT(flow_map_val, scheduler_cfg.unit_is_packet);
	QCE2204_PPE_L0_FLOW_MAP_SET_E_DRR_CREDIT_UNIT(flow_map_val, scheduler_cfg.unit_is_packet);

	ret = qce2204_ppe_bulk_write(phydev, reg, flow_map_val, ARRAY_SIZE(flow_map_val));
	if (ret)
		return ret;

	reg = QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_ADDR +
	      node_id * QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_INC;
	val = FIELD_PREP(QCE2204_PPE_L0_FLOW_PORT_MAP_TBL_PORT_NUM, port);

	ret = qce2204_ppe_write(phydev, reg, val);
	if (ret)
		return ret;

	reg = QCE2204_PPE_L0_COMP_CFG_TBL_ADDR +
	      node_id * QCE2204_PPE_L0_COMP_CFG_TBL_INC;
	val = FIELD_PREP(QCE2204_PPE_L0_COMP_CFG_TBL_NODE_METER_LEN, scheduler_cfg.frame_mode);

	return qce2204_ppe_update_bits(phydev, reg,
				       QCE2204_PPE_L0_COMP_CFG_TBL_NODE_METER_LEN,
				       val);
}

/**
 * qce2204_ppe_scheduler_l1_queue_map_set - Set PPE flow level scheduler configuration (from Linux)
 */
static int qce2204_ppe_scheduler_l1_queue_map_set(struct phy_device *phydev,
						   int node_id, int port,
						   struct qce2204_ppe_scheduler_cfg scheduler_cfg)
{
	u32 val, reg, flow_map_val[4] = {};
	int ret;

	reg = QCE2204_PPE_L1_FLOW_MAP_TBL_ADDR +
	      node_id * QCE2204_PPE_L1_FLOW_MAP_TBL_INC;

	/* Read current values */
	ret = qce2204_ppe_bulk_read(phydev, reg, flow_map_val, ARRAY_SIZE(flow_map_val));
	if (ret)
		return ret;

	/* Set Word 0 fields */
	val = FIELD_PREP(QCE2204_PPE_L1_FLOW_MAP_TBL_FLOW_ID, scheduler_cfg.flow_id);
	val |= FIELD_PREP(QCE2204_PPE_L1_FLOW_MAP_TBL_C_PRI, scheduler_cfg.pri);
	val |= FIELD_PREP(QCE2204_PPE_L1_FLOW_MAP_TBL_E_PRI, scheduler_cfg.pri);
	val |= FIELD_PREP(QCE2204_PPE_L1_FLOW_MAP_TBL_C_NODE_WT, scheduler_cfg.drr_node_wt);
	val |= FIELD_PREP(QCE2204_PPE_L1_FLOW_MAP_TBL_E_NODE_WT, scheduler_cfg.drr_node_wt);
	flow_map_val[0] = val;

	/* Set Word 1 fields using new macros */
	QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_ID_LO(flow_map_val,
						scheduler_cfg.drr_node_id & 0x3);
	QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_ID_HI(flow_map_val,
						(scheduler_cfg.drr_node_id >> 2) & 0xf);
	QCE2204_PPE_L1_FLOW_MAP_SET_E_DRR_ID(flow_map_val, scheduler_cfg.drr_node_id);
	QCE2204_PPE_L1_FLOW_MAP_SET_C_DRR_CREDIT_UNIT(flow_map_val,
						      scheduler_cfg.unit_is_packet);
	QCE2204_PPE_L1_FLOW_MAP_SET_E_DRR_CREDIT_UNIT(flow_map_val,
						      scheduler_cfg.unit_is_packet);

	ret = qce2204_ppe_bulk_write(phydev, reg, flow_map_val, ARRAY_SIZE(flow_map_val));
	if (ret)
		return ret;

	val = FIELD_PREP(QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_PORT_NUM, port);
	reg = QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_ADDR +
	      node_id * QCE2204_PPE_L1_FLOW_PORT_MAP_TBL_INC;

	ret = qce2204_ppe_write(phydev, reg, val);
	if (ret)
		return ret;

	reg = QCE2204_PPE_L1_COMP_CFG_TBL_ADDR + node_id * QCE2204_PPE_L1_COMP_CFG_TBL_INC;
	val = FIELD_PREP(QCE2204_PPE_L1_COMP_CFG_TBL_NODE_METER_LEN, scheduler_cfg.frame_mode);

	return qce2204_ppe_update_bits(phydev, reg,
				       QCE2204_PPE_L1_COMP_CFG_TBL_NODE_METER_LEN,
				       val);
}

/**
 * qce2204_ppe_queue_scheduler_set - Configure scheduler for PPE hardware queue (from Linux)
 */
int qce2204_ppe_queue_scheduler_set(struct phy_device *phydev,
				    int node_id, bool flow_level, int port,
				    struct qce2204_ppe_scheduler_cfg scheduler_cfg)
{
	if (flow_level)
		return qce2204_ppe_scheduler_l1_queue_map_set(phydev, node_id,
								port, scheduler_cfg);

	return qce2204_ppe_scheduler_l0_queue_map_set(phydev, node_id,
							port, scheduler_cfg);
}

/**
 * qce2204_ppe_queue_ucast_base_set - Set PPE unicast queue base ID (from Linux)
 */
int qce2204_ppe_queue_ucast_base_set(struct phy_device *phydev,
				     struct qce2204_ppe_queue_ucast_dest queue_dst,
				     int queue_base, int profile_id)
{
	int index, profile_size;
	u32 val, reg;

	profile_size = queue_dst.src_profile << 8;
	if (queue_dst.service_code_en)
		index = QCE2204_PPE_QUEUE_BASE_SERVICE_CODE + profile_size +
			queue_dst.service_code;
	else if (queue_dst.cpu_code_en)
		index = QCE2204_PPE_QUEUE_BASE_CPU_CODE + profile_size +
			queue_dst.cpu_code;
	else
		index = profile_size + queue_dst.dest_port;

	val = FIELD_PREP(QCE2204_PPE_UCAST_QUEUE_MAP_TBL_PROFILE_ID, profile_id);
	val |= FIELD_PREP(QCE2204_PPE_UCAST_QUEUE_MAP_TBL_QUEUE_ID, queue_base);
	reg = QCE2204_PPE_UCAST_QUEUE_MAP_TBL_ADDR +
	      index * QCE2204_PPE_UCAST_QUEUE_MAP_TBL_INC;

	return qce2204_ppe_write(phydev, reg, val);
}

/**
 * qce2204_ppe_queue_ucast_offset_pri_set - Set PPE unicast queue offset
 * based on priority (from Linux)
 */
int qce2204_ppe_queue_ucast_offset_pri_set(struct phy_device *phydev,
					   int profile_id,
					   int priority,
					   int queue_offset)
{
	u32 val, reg;
	int index;

	index = (profile_id << 4) + priority;
	val = FIELD_PREP(QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_CLASS, queue_offset);
	reg = QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_ADDR +
	      index * QCE2204_PPE_UCAST_PRIORITY_MAP_TBL_INC;

	return qce2204_ppe_write(phydev, reg, val);
}

/**
 * qce2204_ppe_queue_ucast_offset_hash_set - Set PPE unicast queue offset based on hash (from Linux)
 */
int qce2204_ppe_queue_ucast_offset_hash_set(struct phy_device *phydev,
					    int profile_id,
					    int rss_hash,
					    int queue_offset)
{
	u32 val, reg;
	int index;

	index = (profile_id << 8) + rss_hash;
	val = FIELD_PREP(QCE2204_PPE_UCAST_HASH_MAP_TBL_HASH, queue_offset);
	reg = QCE2204_PPE_UCAST_HASH_MAP_TBL_ADDR +
	      index * QCE2204_PPE_UCAST_HASH_MAP_TBL_INC;

	return qce2204_ppe_write(phydev, reg, val);
}

/**
 * qce2204_ppe_port_resource_get - Get PPE resource per port (from Linux)
 */
int qce2204_ppe_port_resource_get(struct phy_device *phydev, int port,
				  enum qce2204_ppe_resource_type type,
				  int *res_start, int *res_end)
{
	struct qce2204_ppe_port_schedule_resource res;

	if (port >= QCE2204_NUM_PORTS)
		return -EINVAL;

	res = qce2204_ppe_scheduler_res[port];
	switch (type) {
	case QCE2204_PPE_RES_UCAST:
		*res_start = res.ucastq_start;
		*res_end = res.ucastq_end;
		break;
	case QCE2204_PPE_RES_MCAST:
		*res_start = res.mcastq_start;
		*res_end = res.mcastq_end;
		break;
	case QCE2204_PPE_RES_FLOW_ID:
		*res_start = res.flow_id_start;
		*res_end = res.flow_id_end;
		break;
	case QCE2204_PPE_RES_L0_NODE:
		*res_start = res.l0node_start;
		*res_end = res.l0node_end;
		break;
	case QCE2204_PPE_RES_L1_NODE:
		*res_start = res.l1node_start;
		*res_end = res.l1node_end;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * qce2204_ppe_counter_enable_set - Set PPE port counter enabled (from Linux)
 */
int qce2204_ppe_counter_enable_set(struct phy_device *phydev, int port)
{
	u32 reg, mru_mtu_val[3];
	int ret;

	reg = QCE2204_PPE_MRU_MTU_CTRL_TBL_ADDR + QCE2204_PPE_MRU_MTU_CTRL_TBL_INC * port;
	ret = qce2204_ppe_bulk_read(phydev, reg,
				    mru_mtu_val, ARRAY_SIZE(mru_mtu_val));
	if (ret)
		return ret;

	QCE2204_PPE_MRU_MTU_CTRL_SET_RX_CNT_EN(mru_mtu_val, true);
	QCE2204_PPE_MRU_MTU_CTRL_SET_TX_CNT_EN(mru_mtu_val, true);
	ret = qce2204_ppe_bulk_write(phydev, reg,
				     mru_mtu_val, ARRAY_SIZE(mru_mtu_val));
	if (ret)
		return ret;

	reg = QCE2204_PPE_MC_MTU_CTRL_TBL_ADDR + QCE2204_PPE_MC_MTU_CTRL_TBL_INC * port;
	ret = qce2204_ppe_set_bits(phydev, reg, QCE2204_PPE_MC_MTU_CTRL_TBL_TX_CNT_EN);
	if (ret)
		return ret;

	reg = QCE2204_PPE_PORT_EG_VLAN_TBL_ADDR + QCE2204_PPE_PORT_EG_VLAN_TBL_INC * port;

	return qce2204_ppe_set_bits(phydev, reg, QCE2204_PPE_PORT_EG_VLAN_TBL_TX_COUNTING_EN);
}

/**
 * qce2204_ppe_rss_hash_ipv4_config - Configure RSS hash for IPv4 (from Linux)
 */
static int qce2204_ppe_rss_hash_ipv4_config(struct phy_device *phydev, int index,
					    struct qce2204_ppe_rss_hash_cfg cfg)
{
	u32 reg, val;

	switch (index) {
	case 0:
		val = cfg.hash_sip_mix[0];
		break;
	case 1:
		val = cfg.hash_dip_mix[0];
		break;
	case 2:
		val = cfg.hash_protocol_mix;
		break;
	case 3:
		val = cfg.hash_dport_mix;
		break;
	case 4:
		val = cfg.hash_sport_mix;
		break;
	default:
		return -EINVAL;
	}

	reg = QCE2204_PPE_RSS_HASH_MIX_IPV4_ADDR + index * QCE2204_PPE_RSS_HASH_MIX_IPV4_INC;

	return qce2204_ppe_update_bits(phydev, reg,
				       QCE2204_PPE_RSS_HASH_MIX_IPV4_VAL,
				       FIELD_PREP(QCE2204_PPE_RSS_HASH_MIX_IPV4_VAL,
						   val));
}

/**
 * qce2204_ppe_rss_hash_ipv6_config - Configure RSS hash for IPv6 (from Linux)
 */
static int qce2204_ppe_rss_hash_ipv6_config(struct phy_device *phydev, int index,
					    struct qce2204_ppe_rss_hash_cfg cfg)
{
	u32 reg, val;

	switch (index) {
	case 0 ... 3:
		val = cfg.hash_sip_mix[index];
		break;
	case 4 ... 7:
		val = cfg.hash_dip_mix[index - 4];
		break;
	case 8:
		val = cfg.hash_protocol_mix;
		break;
	case 9:
		val = cfg.hash_dport_mix;
		break;
	case 10:
		val = cfg.hash_sport_mix;
		break;
	default:
		return -EINVAL;
	}

	reg = QCE2204_PPE_RSS_HASH_MIX_ADDR + index * QCE2204_PPE_RSS_HASH_MIX_INC;

	return qce2204_ppe_update_bits(phydev, reg,
				       QCE2204_PPE_RSS_HASH_MIX_VAL,
				       FIELD_PREP(QCE2204_PPE_RSS_HASH_MIX_VAL,
						   val));
}

/**
 * qce2204_ppe_rss_hash_config_set - Configure PPE hash settings (from Linux)
 */
int qce2204_ppe_rss_hash_config_set(struct phy_device *phydev, int mode,
				    struct qce2204_ppe_rss_hash_cfg cfg)
{
	u32 val, reg;
	int i, ret;

	if (mode & QCE2204_PPE_RSS_HASH_MODE_IPV4) {
		val = FIELD_PREP(QCE2204_PPE_RSS_HASH_MASK_IPV4_HASH_MASK, cfg.hash_mask);
		val |= FIELD_PREP(QCE2204_PPE_RSS_HASH_MASK_IPV4_FRAGMENT, cfg.hash_fragment_mode);
		ret = qce2204_ppe_write(phydev, QCE2204_PPE_RSS_HASH_MASK_IPV4_ADDR, val);
		if (ret)
			return ret;

		val = FIELD_PREP(QCE2204_PPE_RSS_HASH_SEED_IPV4_VAL, cfg.hash_seed);
		ret = qce2204_ppe_write(phydev, QCE2204_PPE_RSS_HASH_SEED_IPV4_ADDR, val);
		if (ret)
			return ret;

		for (i = 0; i < QCE2204_PPE_RSS_HASH_MIX_IPV4_ENTRIES; i++) {
			ret = qce2204_ppe_rss_hash_ipv4_config(phydev, i, cfg);
			if (ret)
				return ret;
		}

		for (i = 0; i < QCE2204_PPE_RSS_HASH_FIN_IPV4_ENTRIES; i++) {
			val = FIELD_PREP(QCE2204_PPE_RSS_HASH_FIN_IPV4_INNER,
					  cfg.hash_fin_inner[i]);
			val |= FIELD_PREP(QCE2204_PPE_RSS_HASH_FIN_IPV4_OUTER,
					   cfg.hash_fin_outer[i]);
			reg = QCE2204_PPE_RSS_HASH_FIN_IPV4_ADDR +
			      i * QCE2204_PPE_RSS_HASH_FIN_IPV4_INC;

			ret = qce2204_ppe_write(phydev, reg, val);
			if (ret)
				return ret;
		}
	}

	if (mode & QCE2204_PPE_RSS_HASH_MODE_IPV6) {
		val = FIELD_PREP(QCE2204_PPE_RSS_HASH_MASK_HASH_MASK, cfg.hash_mask);
		val |= FIELD_PREP(QCE2204_PPE_RSS_HASH_MASK_FRAGMENT, cfg.hash_fragment_mode);
		ret = qce2204_ppe_write(phydev, QCE2204_PPE_RSS_HASH_MASK_ADDR, val);
		if (ret)
			return ret;

		val = FIELD_PREP(QCE2204_PPE_RSS_HASH_SEED_VAL, cfg.hash_seed);
		ret = qce2204_ppe_write(phydev, QCE2204_PPE_RSS_HASH_SEED_ADDR, val);
		if (ret)
			return ret;

		for (i = 0; i < QCE2204_PPE_RSS_HASH_MIX_ENTRIES; i++) {
			ret = qce2204_ppe_rss_hash_ipv6_config(phydev, i, cfg);
			if (ret)
				return ret;
		}

		for (i = 0; i < QCE2204_PPE_RSS_HASH_FIN_ENTRIES; i++) {
			val = FIELD_PREP(QCE2204_PPE_RSS_HASH_FIN_INNER, cfg.hash_fin_inner[i]);
			val |= FIELD_PREP(QCE2204_PPE_RSS_HASH_FIN_OUTER, cfg.hash_fin_outer[i]);
			reg = QCE2204_PPE_RSS_HASH_FIN_ADDR + i * QCE2204_PPE_RSS_HASH_FIN_INC;

			ret = qce2204_ppe_write(phydev, reg, val);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/**
 * qce2204_ppe_ring_queue_map_set - Set PPE queue to Ethernet DMA ring mapping (from Linux)
 */
int qce2204_ppe_ring_queue_map_set(struct phy_device *phydev,
				   int ring_id, u32 *queue_map)
{
	u32 reg, queue_bitmap_val[QCE2204_PPE_RING_TO_QUEUE_BITMAP_WORD_CNT];

	memcpy(queue_bitmap_val, queue_map, sizeof(queue_bitmap_val));
	reg = QCE2204_PPE_RING_Q_MAP_TBL_ADDR + QCE2204_PPE_RING_Q_MAP_TBL_INC * ring_id;

	return qce2204_ppe_bulk_write(phydev, reg,
				      queue_bitmap_val,
				      ARRAY_SIZE(queue_bitmap_val));
}

/* Configure BM port threshold (from Linux) */
static int qce2204_ppe_config_bm_threshold(struct phy_device *phydev, int bm_port_id,
					   const struct qce2204_ppe_bm_port_config port_cfg)
{
	u32 reg, val, bm_fc_val[4]; /* 16 bytes = 4 words */
	int ret;

	reg = QCE2204_PPE_BM_PORT_FC_CFG_TBL_ADDR +
	      QCE2204_PPE_BM_PORT_FC_CFG_TBL_INC * bm_port_id;
	ret = qce2204_ppe_bulk_read(phydev, reg,
				    bm_fc_val, ARRAY_SIZE(bm_fc_val));
	if (ret)
		return ret;

	/* Configure BM flow control related threshold */
	QCE2204_PPE_BM_PORT_FC_SET_WEIGHT(bm_fc_val, port_cfg.weight);
	QCE2204_PPE_BM_PORT_FC_SET_RESUME_OFFSET(bm_fc_val, port_cfg.resume_offset);
	QCE2204_PPE_BM_PORT_FC_SET_RESUME_THRESHOLD(bm_fc_val, port_cfg.resume_ceil);
	QCE2204_PPE_BM_PORT_FC_SET_DYNAMIC(bm_fc_val, port_cfg.dynamic);
	QCE2204_PPE_BM_PORT_FC_SET_REACT_LIMIT(bm_fc_val, port_cfg.in_fly_buf);
	QCE2204_PPE_BM_PORT_FC_SET_PRE_ALLOC(bm_fc_val, port_cfg.pre_alloc);

	/* Configure low/high bits of ceiling */
	val = FIELD_GET(BIT(0), port_cfg.ceil);
	QCE2204_PPE_BM_PORT_FC_SET_CEILING_LOW(bm_fc_val, val);
	val = FIELD_GET(GENMASK(11, 1), port_cfg.ceil);
	QCE2204_PPE_BM_PORT_FC_SET_CEILING_HIGH(bm_fc_val, val);

	ret = qce2204_ppe_bulk_write(phydev, reg,
				     bm_fc_val, ARRAY_SIZE(bm_fc_val));
	if (ret)
		return ret;

	/* Assign default group ID 0 to BM port */
	val = FIELD_PREP(QCE2204_PPE_BM_PORT_GROUP_ID_SHARED_GROUP_ID, 0);
	reg = QCE2204_PPE_BM_PORT_GROUP_ID_ADDR +
	      QCE2204_PPE_BM_PORT_GROUP_ID_INC * bm_port_id;
	ret = qce2204_ppe_update_bits(phydev, reg,
				      QCE2204_PPE_BM_PORT_GROUP_ID_SHARED_GROUP_ID,
				      val);
	if (ret)
		return ret;

	/* Disable BM port flow control */
	reg = QCE2204_PPE_BM_PORT_FC_MODE_ADDR +
	      QCE2204_PPE_BM_PORT_FC_MODE_INC * bm_port_id;

	return qce2204_ppe_clear_bits(phydev, reg, QCE2204_PPE_BM_PORT_FC_MODE_EN);
}

/* Configure buffer management (from Linux) */
static int qce2204_ppe_config_bm(struct phy_device *phydev)
{
	const struct qce2204_ppe_bm_port_config *port_cfg;
	unsigned int i, bm_port_id, port_cfg_cnt;
	u32 reg, val;
	int ret;

	/* Configure allocated buffer number for group 0 */
	reg = QCE2204_PPE_BM_SHARED_GROUP_CFG_ADDR;
	val = FIELD_PREP(QCE2204_PPE_BM_SHARED_GROUP_CFG_SHARED_LIMIT,
			 qce2204_ppe_bm_group_config);
	ret = qce2204_ppe_update_bits(phydev, reg,
				      QCE2204_PPE_BM_SHARED_GROUP_CFG_SHARED_LIMIT,
				      val);
	if (ret)
		goto bm_config_fail;

	/* Configure buffer thresholds for BM ports */
	port_cfg = qce2204_ppe_bm_port_config;
	port_cfg_cnt = ARRAY_SIZE(qce2204_ppe_bm_port_config);
	for (i = 0; i < port_cfg_cnt; i++) {
		for (bm_port_id = port_cfg[i].port_id_start;
		     bm_port_id <= port_cfg[i].port_id_end; bm_port_id++) {
			ret = qce2204_ppe_config_bm_threshold(phydev, bm_port_id,
							      port_cfg[i]);
			if (ret)
				goto bm_config_fail;
		}
	}

	for(int port = 0; port <= 5; port++ ) {
		ret = qce2204_ppe_update_bits(phydev,
					QCE2204_PPE_SWITCH_NSS_SWITCH_BM_PORT_FC_MODE  +
					(4 * port ),
					BIT(0),
					BIT(0));
		if (ret)
			goto bm_config_fail;
	}
	return 0;

bm_config_fail:
	debug("QCE2204: PPE BM config error %d\n", ret);
	return ret;
}

/* Configure queue management (from Linux) */
static int qce2204_ppe_config_qm(struct phy_device *phydev)
{
	const struct qce2204_ppe_qm_queue_config *queue_cfg;
	int ret, i, queue_id, queue_cfg_count;
	u32 reg, multicast_queue_cfg[3]; /* 12 bytes */
	u32 unicast_queue_cfg[8];        /* 32 bytes */
	u32 group_cfg[4];                /* 16 bytes */

	/* Assign buffer number to group 0 */
	reg = QCE2204_PPE_AC_GRP_CFG_TBL_ADDR;
	ret = qce2204_ppe_bulk_read(phydev, reg,
				    group_cfg, ARRAY_SIZE(group_cfg));
	if (ret)
		goto qm_config_fail;

	QCE2204_PPE_AC_GRP_SET_BUF_LIMIT(group_cfg, qce2204_ppe_qm_group_config);

	ret = qce2204_ppe_bulk_write(phydev, reg,
				     group_cfg, ARRAY_SIZE(group_cfg));
	if (ret)
		goto qm_config_fail;

	queue_cfg = qce2204_ppe_qm_queue_config;
	queue_cfg_count = ARRAY_SIZE(qce2204_ppe_qm_queue_config);
	for (i = 0; i < queue_cfg_count; i++) {
		queue_id = queue_cfg[i].queue_start;

		while (queue_id <= queue_cfg[i].queue_end) {
			if (queue_id < 256) {
				/* Unicast queue */
				reg = QCE2204_PPE_AC_UNICAST_QUEUE_CFG_TBL_ADDR +
					  QCE2204_PPE_AC_UNICAST_QUEUE_CFG_TBL_INC * queue_id;

				ret = qce2204_ppe_bulk_read(phydev, reg,
							    unicast_queue_cfg,
							    ARRAY_SIZE(unicast_queue_cfg));
				if (ret)
					goto qm_config_fail;

				QCE2204_PPE_AC_UNICAST_QUEUE_SET_EN(unicast_queue_cfg, true);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_GRP_ID(unicast_queue_cfg, 0);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_PRE_LIMIT(unicast_queue_cfg,
									    queue_cfg[i].prealloc_buf);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_DYNAMIC(unicast_queue_cfg,
									  queue_cfg[i].dynamic);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_WEIGHT(unicast_queue_cfg,
									 queue_cfg[i].weight);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_THRESHOLD_LO(unicast_queue_cfg,
									       queue_cfg[i].ceil & 0x3FF);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_THRESHOLD_HI(unicast_queue_cfg,
									       (queue_cfg[i].ceil >> 10) & 0x3);
				QCE2204_PPE_AC_UNICAST_QUEUE_SET_GRN_RESUME(unicast_queue_cfg,
									     queue_cfg[i].resume_offset);

				ret = qce2204_ppe_bulk_write(phydev, reg,
							     unicast_queue_cfg,
							     ARRAY_SIZE(unicast_queue_cfg));
				if (ret)
					goto qm_config_fail;
			} else {
				/* Multicast queue */
				reg = QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_TBL_ADDR +
				      QCE2204_PPE_AC_MULTICAST_QUEUE_CFG_TBL_INC * (queue_id - 256);

				ret = qce2204_ppe_bulk_read(phydev, reg,
							    multicast_queue_cfg,
							    ARRAY_SIZE(multicast_queue_cfg));
				if (ret)
					goto qm_config_fail;

				QCE2204_PPE_AC_MULTICAST_QUEUE_SET_EN(multicast_queue_cfg, true);
				QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_GRP_ID(multicast_queue_cfg,
									       0);
				QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_PRE_LIMIT(multicast_queue_cfg,
										  queue_cfg[i].prealloc_buf);
				QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_THRESHOLD(multicast_queue_cfg,
										  queue_cfg[i].ceil);
				QCE2204_PPE_AC_MULTICAST_QUEUE_SET_GRN_RESUME(multicast_queue_cfg,
									       queue_cfg[i].resume_offset);

				ret = qce2204_ppe_bulk_write(phydev, reg,
							     multicast_queue_cfg,
							     ARRAY_SIZE(multicast_queue_cfg));
				if (ret)
					goto qm_config_fail;
			}

			/* Enable enqueue */
			reg = QCE2204_PPE_ENQ_OPR_TBL_ADDR + QCE2204_PPE_ENQ_OPR_TBL_INC * queue_id;
			ret = qce2204_ppe_clear_bits(phydev, reg,
						     QCE2204_PPE_ENQ_OPR_TBL_ENQ_DISABLE);
			if (ret)
				goto qm_config_fail;

			/* Enable dequeue */
			reg = QCE2204_PPE_DEQ_OPR_TBL_ADDR + QCE2204_PPE_DEQ_OPR_TBL_INC * queue_id;
			ret = qce2204_ppe_clear_bits(phydev, reg,
						     QCE2204_PPE_DEQ_OPR_TBL_DEQ_DISABLE);
			if (ret)
				goto qm_config_fail;

			queue_id++;
		}
	}

	/* Enable queue counter */
	ret = qce2204_ppe_set_bits(phydev, QCE2204_PPE_EG_BRIDGE_CONFIG_ADDR,
				   QCE2204_PPE_EG_BRIDGE_CONFIG_QUEUE_CNT_EN);
	if (ret)
		goto qm_config_fail;

	/* Disable ucast enqueue for mcast traffic */
	ret = qce2204_ppe_clear_bits(phydev, QCE2204_PPE_MC_ENQ_CTRL_ADDR,
				     QCE2204_PPE_MC_ENQ_CTRL_UC_ENQ_EN);
	if (ret)
		goto qm_config_fail;

	return 0;

qm_config_fail:
	debug("QCE2204: PPE QM config error %d\n", ret);
	return ret;
}

/* Configure node scheduler (from Linux) */
static int qce2204_ppe_node_scheduler_config(struct phy_device *phydev,
					     const struct qce2204_ppe_scheduler_port_config
					     config)
{
	struct qce2204_ppe_scheduler_cfg sch_cfg;
	int ret, i;

	for (i = 0; i < config.loop_num; i++) {
		if (!config.pri_max) {
			/* Round robin scheduler without priority */
			sch_cfg.flow_id = config.flow_id;
			sch_cfg.pri = 0;
			sch_cfg.drr_node_id = config.drr_node_id;
		} else {
			sch_cfg.flow_id = config.flow_id +
				((config.flow_level) ? (0) : (i / config.pri_max));
			sch_cfg.pri = i % config.pri_max;
			sch_cfg.drr_node_id = config.drr_node_id +
				((config.flow_level) ? (i % config.pri_max) : (i));
		}

		/* Scheduler weight, must be more than 0 */
		sch_cfg.drr_node_wt = 1;
		/* Byte based scheduling */
		sch_cfg.unit_is_packet = false;
		/* Frame + CRC calculated */
		sch_cfg.frame_mode = QCE2204_PPE_SCH_WITH_IPG_PREAMBLE_FRAME_CRC;

		ret = qce2204_ppe_queue_scheduler_set(phydev, config.node_id + i,
						      config.flow_level,
						      config.port,
						      sch_cfg);
		if (ret)
			return ret;
	}

	return 0;
}

/* Configure TDM scheduler (from Linux) */
static int qce2204_ppe_config_scheduler(struct phy_device *phydev)
{
	const struct qce2204_ppe_scheduler_port_config *port_cfg;
	const struct qce2204_ppe_scheduler_qm_config *qm_cfg;
	const struct qce2204_ppe_scheduler_bm_config *bm_cfg;
	int ret, i, count;
	u32 val, reg;

	count = ARRAY_SIZE(qce2204_ppe_sch_bm_config);
	bm_cfg = qce2204_ppe_sch_bm_config;

	/* Configure depth of BM scheduler entries */
	val = FIELD_PREP(QCE2204_PPE_BM_SCH_CTRL_SCH_DEPTH, count);
	val |= FIELD_PREP(QCE2204_PPE_BM_SCH_CTRL_SCH_OFFSET, 0);
	val |= FIELD_PREP(QCE2204_PPE_BM_SCH_CTRL_SCH_EN, 1);

	ret = qce2204_ppe_write(phydev, QCE2204_PPE_BM_SCH_CTRL_ADDR, val);
	if (ret)
		goto sch_config_fail;

	/* Configure each BM scheduler entry */
	for (i = 0; i < count; i++) {
		val = FIELD_PREP(QCE2204_PPE_BM_SCH_CFG_TBL_VALID, bm_cfg[i].valid);
		val |= FIELD_PREP(QCE2204_PPE_BM_SCH_CFG_TBL_DIR, bm_cfg[i].dir);
		val |= FIELD_PREP(QCE2204_PPE_BM_SCH_CFG_TBL_PORT_NUM, bm_cfg[i].port);
		val |= FIELD_PREP(QCE2204_PPE_BM_SCH_CFG_TBL_SECOND_PORT_VALID,
						  bm_cfg[i].backup_port_valid);
		val |= FIELD_PREP(QCE2204_PPE_BM_SCH_CFG_TBL_SECOND_PORT,
						  bm_cfg[i].backup_port);

		reg = QCE2204_PPE_BM_SCH_CFG_TBL_ADDR + i * QCE2204_PPE_BM_SCH_CFG_TBL_INC;
		ret = qce2204_ppe_write(phydev, reg, val);
		if (ret)
			goto sch_config_fail;
	}

	count = ARRAY_SIZE(qce2204_ppe_sch_qm_config);
	qm_cfg = qce2204_ppe_sch_qm_config;

	/* Configure depth of QM scheduler entries */
	val = FIELD_PREP(QCE2204_PPE_PSCH_SCH_DEPTH_CFG_SCH_DEPTH, count);
	ret = qce2204_ppe_write(phydev, QCE2204_PPE_PSCH_SCH_DEPTH_CFG_ADDR, val);
	if (ret)
		goto sch_config_fail;

	/* Configure each QM scheduler entry */
	for (i = 0; i < count; i++) {
		val = FIELD_PREP(QCE2204_PPE_PSCH_SCH_CFG_TBL_ENS_PORT_BITMAP,
				  qm_cfg[i].ensch_port_bmp);
		val |= FIELD_PREP(QCE2204_PPE_PSCH_SCH_CFG_TBL_ENS_PORT,
						  qm_cfg[i].ensch_port);
		val |= FIELD_PREP(QCE2204_PPE_PSCH_SCH_CFG_TBL_DES_PORT,
						  qm_cfg[i].desch_port);
		val |= FIELD_PREP(QCE2204_PPE_PSCH_SCH_CFG_TBL_DES_SECOND_PORT_EN,
						  qm_cfg[i].desch_backup_port_valid);
		val |= FIELD_PREP(QCE2204_PPE_PSCH_SCH_CFG_TBL_DES_SECOND_PORT,
						  qm_cfg[i].desch_backup_port);

		reg = QCE2204_PPE_PSCH_SCH_CFG_TBL_ADDR + i * QCE2204_PPE_PSCH_SCH_CFG_TBL_INC;
		ret = qce2204_ppe_write(phydev, reg, val);
		if (ret)
			goto sch_config_fail;
	}

	count = ARRAY_SIZE(qce2204_ppe_port_sch_config);
	port_cfg = qce2204_ppe_port_sch_config;

	/* Configure scheduler per PPE queue or flow */
	for (i = 0; i < count; i++) {
		if (port_cfg[i].port >= QCE2204_NUM_PORTS)
			break;

		ret = qce2204_ppe_node_scheduler_config(phydev, port_cfg[i]);
		if (ret)
			goto sch_config_fail;
	}

	return 0;

sch_config_fail:
	debug("QCE2204: PPE scheduler config error %d\n", ret);
	return ret;
}

/* Initialize queue destinations (from Linux) */
static int qce2204_ppe_queue_dest_init(struct phy_device *phydev)
{
	int ret, port_id, index, q_base, q_offset, res_start, res_end, pri_max;
	struct qce2204_ppe_queue_ucast_dest queue_dst;

	for (port_id = 0; port_id < QCE2204_NUM_PORTS; port_id++) {
		memset(&queue_dst, 0, sizeof(queue_dst));

		ret = qce2204_ppe_port_resource_get(phydev, port_id, QCE2204_PPE_RES_UCAST,
						    &res_start, &res_end);
		if (ret)
			return ret;

		q_base = res_start;
		queue_dst.dest_port = port_id;

		/* Configure queue base ID and profile ID */
		ret = qce2204_ppe_queue_ucast_base_set(phydev, queue_dst,
						       q_base, port_id);
		if (ret)
			return ret;

		/* Queue priority range */
		ret = qce2204_ppe_port_resource_get(phydev, port_id, QCE2204_PPE_RES_L0_NODE,
						    &res_start, &res_end);
		if (ret)
			return ret;

		pri_max = res_end - res_start;

		/* Redirect ARP reply with max priority on CPU port */
		if (port_id == 0) {
			memset(&queue_dst, 0, sizeof(queue_dst));

			queue_dst.cpu_code_en = true;
			queue_dst.cpu_code = 101;
			ret = qce2204_ppe_queue_ucast_base_set(phydev, queue_dst,
							       q_base + pri_max,
							       0);
			if (ret)
				return ret;
		}

		/* Initialize queue offset of internal priority */
		for (index = 0; index < QCE2204_PPE_QUEUE_INTER_PRI_NUM; index++) {
			q_offset = index > pri_max ? pri_max : index;

			ret = qce2204_ppe_queue_ucast_offset_pri_set(phydev, port_id,
								     index, q_offset);
			if (ret)
				return ret;
		}

		/* Initialize queue offset of RSS hash as 0 */
		for (index = 0; index < QCE2204_PPE_QUEUE_HASH_NUM; index++) {
			ret = qce2204_ppe_queue_ucast_offset_hash_set(phydev, port_id,
								      index, 0);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/* Initialize port configurations (from Linux) */
static int qce2204_ppe_port_config_init(struct phy_device *phydev)
{
	u32 reg, val, mru_mtu_val[3];
	int i, ret;

	/* MTU and MRU settings not required for CPU port 0 */
	for (i = 1; i < QCE2204_NUM_PORTS; i++) {
		/* Enable Ethernet port counter */
		ret = qce2204_ppe_counter_enable_set(phydev, i);
		if (ret)
			return ret;

		reg = QCE2204_PPE_MRU_MTU_CTRL_TBL_ADDR + QCE2204_PPE_MRU_MTU_CTRL_TBL_INC * i;
		ret = qce2204_ppe_bulk_read(phydev, reg,
					    mru_mtu_val, ARRAY_SIZE(mru_mtu_val));
		if (ret)
			return ret;

		/* Drop packet when size > MTU, redirect to CPU when size > MRU */
		QCE2204_PPE_MRU_MTU_CTRL_SET_MRU_CMD(mru_mtu_val,
						     QCE2204_PPE_ACTION_REDIRECT_TO_CPU);
		QCE2204_PPE_MRU_MTU_CTRL_SET_MTU_CMD(mru_mtu_val, QCE2204_PPE_ACTION_DROP);
		ret = qce2204_ppe_bulk_write(phydev, reg,
					     mru_mtu_val, ARRAY_SIZE(mru_mtu_val));
		if (ret)
			return ret;

		reg = QCE2204_PPE_MC_MTU_CTRL_TBL_ADDR + QCE2204_PPE_MC_MTU_CTRL_TBL_INC * i;
		val = FIELD_PREP(QCE2204_PPE_MC_MTU_CTRL_TBL_MTU_CMD,
				  QCE2204_PPE_ACTION_DROP);
		ret = qce2204_ppe_update_bits(phydev, reg,
					      QCE2204_PPE_MC_MTU_CTRL_TBL_MTU_CMD,
					      val);
		if (ret)
			return ret;
	}

	/* Enable CPU port counters */
	return qce2204_ppe_counter_enable_set(phydev, 0);
}

/* Initialize RSS hash (from Linux) */
static int qce2204_ppe_rss_hash_init(struct phy_device *phydev)
{
	u16 fins[QCE2204_PPE_RSS_HASH_TUPLES] = { 0x205, 0x264, 0x227, 0x245, 0x201 };
	u8 ips[QCE2204_PPE_RSS_HASH_IP_LENGTH] = { 0x13, 0xb, 0x13, 0xb };
	struct qce2204_ppe_rss_hash_cfg hash_cfg;
	int i, ret;

	hash_cfg.hash_seed = 0x12345678; /* Fixed seed for U-Boot */
	hash_cfg.hash_mask = 0xfff;
	hash_cfg.hash_fragment_mode = false;

	/* Final common seed configs */
	for (i = 0; i < ARRAY_SIZE(fins); i++) {
		hash_cfg.hash_fin_inner[i] = fins[i] & 0x1f;
		hash_cfg.hash_fin_outer[i] = fins[i] >> 5;
	}

	/* RSS seeds for protocol, ports, and IPs */
	hash_cfg.hash_protocol_mix = 0x13;
	hash_cfg.hash_dport_mix = 0xb;
	hash_cfg.hash_sport_mix = 0x13;
	hash_cfg.hash_dip_mix[0] = 0xb;
	hash_cfg.hash_sip_mix[0] = 0x13;

	/* Configure RSS for IPv4 */
	ret = qce2204_ppe_rss_hash_config_set(phydev, QCE2204_PPE_RSS_HASH_MODE_IPV4, hash_cfg);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(ips); i++) {
		hash_cfg.hash_sip_mix[i] = ips[i];
		hash_cfg.hash_dip_mix[i] = ips[i];
	}

	/* Configure RSS for IPv6 */
	return qce2204_ppe_rss_hash_config_set(phydev, QCE2204_PPE_RSS_HASH_MODE_IPV6, hash_cfg);
}

/* Initialize bridge (from Linux) */
static int qce2204_ppe_bridge_init(struct phy_device *phydev)
{
	const u8 user_ports_mask = 0x3e; /* BIT(1..5) */
	const u8 cpu_ports_mask  = 0x01; /* BIT(0)    */
	u32 reg, mask, bridge_cfg, port_cfg[4], vsi_cfg[2];
	int ret, i;

	for (i = 0; i < QCE2204_NUM_PORTS; i++) {
		reg = QCE2204_PPE_PORT_BRIDGE_CTRL_ADDR + QCE2204_PPE_PORT_BRIDGE_CTRL_INC * i;
		bridge_cfg = (i == 0) ?
			FIELD_PREP(QCE2204_PPE_PORT_BRIDGE_ISOL_BITMAP, user_ports_mask) :
			FIELD_PREP(QCE2204_PPE_PORT_BRIDGE_ISOL_BITMAP,
				   (user_ports_mask & ~BIT(i)) | cpu_ports_mask);
		bridge_cfg |= QCE2204_PPE_PORT_BRIDGE_TXMAC_EN;
		mask = QCE2204_PPE_PORT_BRIDGE_ISOL_BITMAP | QCE2204_PPE_PORT_BRIDGE_TXMAC_EN;
		ret = qce2204_ppe_update_bits(phydev, reg, mask, bridge_cfg);
		if (ret)
			return ret;

		/* Enable invalid VSI forwarding for physical ports to CPU */
		if (i == 0)
			continue;

		reg = QCE2204_PPE_L2_VP_PORT_TBL_ADDR + QCE2204_PPE_L2_VP_PORT_TBL_INC * i;
		ret = qce2204_ppe_bulk_read(phydev, reg,
					    port_cfg, ARRAY_SIZE(port_cfg));
		if (ret)
			return ret;

		QCE2204_PPE_L2_PORT_SET_INVALID_VSI_FWD_EN(port_cfg, true);
		QCE2204_PPE_L2_PORT_SET_DST_INFO(port_cfg, 0);

		ret = qce2204_ppe_bulk_write(phydev, reg,
					     port_cfg, ARRAY_SIZE(port_cfg));
		if (ret)
			return ret;
	}

	for (i = 0; i < QCE2204_PPE_VSI_TBL_ENTRIES_NUM; i++) {
		/* Set VSI forward membership to include only CPU port0 */
		reg = QCE2204_PPE_VSI_TBL_ADDR + QCE2204_PPE_VSI_TBL_INC * i;
		ret = qce2204_ppe_bulk_read(phydev, reg,
					    vsi_cfg, ARRAY_SIZE(vsi_cfg));
		if (ret)
			return ret;

		QCE2204_PPE_VSI_SET_MEMBER_PORT_BITMAP(vsi_cfg, BIT(0));
		QCE2204_PPE_VSI_SET_UUC_BITMAP(vsi_cfg, BIT(0));
		QCE2204_PPE_VSI_SET_UMC_BITMAP(vsi_cfg, BIT(0));
		QCE2204_PPE_VSI_SET_BC_BITMAP_LO(vsi_cfg, BIT(0));
		QCE2204_PPE_VSI_SET_NEW_ADDR_LRN_EN(vsi_cfg, true);
		QCE2204_PPE_VSI_SET_NEW_ADDR_FWD_CMD(vsi_cfg, QCE2204_PPE_ACTION_FORWARD);
		QCE2204_PPE_VSI_SET_STATION_MOVE_LRN_EN(vsi_cfg, true);
		QCE2204_PPE_VSI_SET_STATION_MOVE_FWD_CMD(vsi_cfg, QCE2204_PPE_ACTION_FORWARD);

		ret = qce2204_ppe_bulk_write(phydev, reg,
					     vsi_cfg, ARRAY_SIZE(vsi_cfg));
		if (ret)
			return ret;
	}

	return qce2204_ppe_set_bits(phydev, QCE2204_PPE_EG_BRIDGE_CONFIG_ADDR,
				    QCE2204_PPE_EG_BRIDGE_CONFIG_PKT_L2_EDIT_EN);
}

/**
 * qce2204_ppe_vsi_member_set - Configure VSI member ports (from Linux)
 */
int qce2204_ppe_vsi_member_set(struct phy_device *phydev,
			       u32 vsi_id,
			       struct qce2204_ppe_vsi_member_cfg *cfg)
{
	u32 reg_addr;
	u32 tbl_data[QCE2204_PPE_VSI_TBL_INC / 4];
	int ret;

	if (!cfg)
		return -EINVAL;

	if (vsi_id >= QCE2204_PPE_VSI_TBL_ENTRIES_NUM)
		return -EINVAL;

	reg_addr = QCE2204_PPE_VSI_TBL_ADDR +
			   vsi_id * QCE2204_PPE_VSI_TBL_INC;

	ret = qce2204_ppe_bulk_read(phydev, reg_addr, tbl_data,
				    QCE2204_PPE_VSI_TBL_INC / 4);
	if (ret)
		return ret;

	/* Update fields */
	QCE2204_PPE_VSI_SET_MEMBER_PORT_BITMAP(tbl_data, cfg->member_port_bitmap);
	QCE2204_PPE_VSI_SET_UUC_BITMAP(tbl_data, cfg->uuc_bitmap);
	QCE2204_PPE_VSI_SET_UMC_BITMAP(tbl_data, cfg->umc_bitmap);
	QCE2204_PPE_VSI_SET_BC_BITMAP_LO(tbl_data, cfg->bc_bitmap & 0x1f);
	QCE2204_PPE_VSI_SET_BC_BITMAP_HI(tbl_data, (cfg->bc_bitmap >> 5) & 0xf);

	return qce2204_ppe_bulk_write(phydev, reg_addr, tbl_data,
				      QCE2204_PPE_VSI_TBL_INC / 4);
}

/**
 * qce2204_ppe_port_vsi_set - Configure VSI for port
 */
int qce2204_ppe_port_vsi_set(struct phy_device *phydev,
			     u32 port_id,
			     struct qce2204_ppe_port_vsi_cfg *cfg)
{
	u32 reg_addr;
	u32 tbl_data[QCE2204_PPE_L3_VP_PORT_TBL_INC / 4];
	int ret;

	if (!cfg)
		return -EINVAL;

	if (port_id >= QCE2204_PPE_L3_VP_PORT_TBL_ENTRIES)
		return -EINVAL;

	reg_addr = QCE2204_PPE_L3_VP_PORT_TBL_ADDR +
		   port_id * QCE2204_PPE_L3_VP_PORT_TBL_INC;

	ret = qce2204_ppe_bulk_read(phydev, reg_addr, tbl_data,
				    QCE2204_PPE_L3_VP_PORT_TBL_INC / 4);
	if (ret)
		return ret;

	/* Update VSI fields using SET macros */
	QCE2204_PPE_L3_VP_PORT_SET_VSI_VALID(tbl_data, cfg->vsi_valid ? 1 : 0);
	QCE2204_PPE_L3_VP_PORT_SET_VSI(tbl_data, cfg->vsi);

	return qce2204_ppe_bulk_write(phydev, reg_addr, tbl_data,
				      QCE2204_PPE_L3_VP_PORT_TBL_INC / 4);
}

/**
 * qce2204_setup_none_tag_vsi - Setup VSI configuration for none tag mode
 */
int qce2204_setup_none_tag_vsi(struct phy_device *phydev)
{
	struct qce2204_ppe_port_vsi_cfg port_vsi_cfg = {};
	struct qce2204_ppe_vsi_member_cfg vsi_member_cfg = {};
	u32 cpu_ports_mask = BIT(QCE2204_CPU_PORT_ID);
	u32 user_ports_mask = 0x3e; /* Ports 1-5 */
	int port, ret;

	debug("QCE2204: Setting up none tag VSI\n");

	/* Step 1: Set all ports to use default VSI */
	port_vsi_cfg.vsi_valid = true;
	port_vsi_cfg.vsi = QCE2204_DEFAULT_VSI;

	for (port = 0; port < QCE2204_NUM_PORTS; port++) {
		ret = qce2204_ppe_port_vsi_set(phydev, port, &port_vsi_cfg);
		if (ret) {
			debug("QCE2204: Failed to set VSI for port %d: %d\n", port, ret);
			goto cleanup;
		}
	}

	debug("QCE2204: User ports mask: 0x%lx, CPU ports mask: 0x%lx\n",
	      (unsigned long)user_ports_mask, (unsigned long)cpu_ports_mask);

	/* Step 2: Configure default VSI membership */
	vsi_member_cfg.member_port_bitmap = user_ports_mask | cpu_ports_mask;
	vsi_member_cfg.uuc_bitmap = user_ports_mask | cpu_ports_mask;
	vsi_member_cfg.umc_bitmap = user_ports_mask | cpu_ports_mask;
	vsi_member_cfg.bc_bitmap = user_ports_mask | cpu_ports_mask;

	ret = qce2204_ppe_vsi_member_set(phydev, QCE2204_DEFAULT_VSI, &vsi_member_cfg);
	if (ret) {
		debug("QCE2204: Failed to set default VSI membership: %d\n", ret);
		goto cleanup;
	}

	debug("QCE2204: None tag VSI configuration completed\n");
	return 0;

cleanup:
	/* Attempt to clean up on error */
	qce2204_teardown_none_tag_vsi(phydev);
	return ret;
}

/**
 * qce2204_teardown_none_tag_vsi - Teardown VSI configuration for none tag mode
 */
int qce2204_teardown_none_tag_vsi(struct phy_device *phydev)
{
	struct qce2204_ppe_port_vsi_cfg port_vsi_cfg = {};
	struct qce2204_ppe_vsi_member_cfg vsi_member_cfg = {};
	u32 cpu_ports_mask = BIT(QCE2204_CPU_PORT_ID);
	int port, ret;

	debug("QCE2204: Tearing down none tag VSI configuration\n");

	/* Step 1: Clear VSI configuration for all ports */
	port_vsi_cfg.vsi_valid = false;
	port_vsi_cfg.vsi = 0;

	for (port = 0; port < QCE2204_NUM_PORTS; port++) {
		ret = qce2204_ppe_port_vsi_set(phydev, port, &port_vsi_cfg);
		if (ret) {
			debug("QCE2204: Failed to clear VSI for port %d: %d\n", port, ret);
			/* Continue cleanup even on error */
		}
	}

	/* Step 2: Reset default VSI membership */
	vsi_member_cfg.member_port_bitmap = cpu_ports_mask;
	vsi_member_cfg.uuc_bitmap = cpu_ports_mask;
	vsi_member_cfg.umc_bitmap = cpu_ports_mask;
	vsi_member_cfg.bc_bitmap = cpu_ports_mask;
	ret = qce2204_ppe_vsi_member_set(phydev, QCE2204_DEFAULT_VSI, &vsi_member_cfg);
	if (ret) {
		debug("QCE2204: Failed to clear default VSI membership: %d\n", ret);
		return ret;
	}

	debug("QCE2204: None tag VSI configuration torn down\n");
	return 0;
}

/**
 * qce2204_ppe_hw_init - Initialize PPE hardware
 */
int qce2204_ppe_hw_init(struct phy_device *phydev)
{
	u32 switch_id;
	int ret;

	debug("QCE2204: Initializing PPE hardware (complete Linux sequence)\n");

	/* Read and verify switch ID */
	switch_id = qce2204_ppe_read(phydev, QCE2204_PPE_SWITCH_ID_ADDR);

	debug("QCE2204: Switch ID=0x%02lx, Rev=0x%02lx\n",
	      (unsigned long)QCE2204_PPE_SWITCH_ID_GET_DEVICE_ID(switch_id),
	      (unsigned long)QCE2204_PPE_SWITCH_ID_GET_REV_ID(switch_id));

	ret = qce2204_ppe_write(phydev,
				QCE2204_PPE_SWITCH_NSS_SWITCH_PORT_MUX_CTRL,
				QCE2204_SWITCH_PORT0_MAC_SEL |
				QCE2204_SWITCH_PORT5_MAC_SEL);

	if (ret)
		return ret;

	/* Complete Linux initialization sequence - ALL steps */
	ret = qce2204_ppe_config_bm(phydev);
	if (ret)
		return ret;

	ret = qce2204_ppe_config_qm(phydev);
	if (ret)
		return ret;

	ret = qce2204_ppe_config_scheduler(phydev);
	if (ret)
		return ret;

	ret = qce2204_ppe_queue_dest_init(phydev);
	if (ret)
		return ret;

	ret = qce2204_ppe_port_config_init(phydev);
	if (ret)
		return ret;

	ret = qce2204_ppe_rss_hash_init(phydev);
	if (ret)
		return ret;

	ret = qce2204_ppe_bridge_init(phydev);
	if (ret)
		return ret;

	debug("QCE2204: PPE hardware initialized successfully (complete Linux port)\n");
	return 0;
}
