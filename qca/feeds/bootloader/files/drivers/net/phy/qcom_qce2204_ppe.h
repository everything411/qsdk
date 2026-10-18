/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __QCE2204_PPE_H__
#define __QCE2204_PPE_H__

#include <linux/types.h>
#include <phy.h>
#include <asm/io.h>

/* QCE2204 Switch Configuration */
#define QCE2204_NUM_PORTS		6	/* 5 user ports + 1 CPU port */
#define QCE2204_CPU_PORT_ID		0
#define QCE2204_DEFAULT_VSI		0
#define QCE2204_MAX_VLANS		4096

/* PPE Resource Types */
enum qce2204_ppe_resource_type {
	QCE2204_PPE_RES_UCAST,
	QCE2204_PPE_RES_MCAST,
	QCE2204_PPE_RES_FLOW_ID,
	QCE2204_PPE_RES_L0_NODE,
	QCE2204_PPE_RES_L1_NODE,
};

/* PPE Direction */
enum qce2204_ppe_direction {
	QCE2204_PPE_INGRESS = 0,
	QCE2204_PPE_EGRESS = 1,
};

/* PPE Action Types */
enum qce2204_ppe_action {
	QCE2204_PPE_ACTION_FORWARD = 0,
	QCE2204_PPE_ACTION_DROP = 1,
	QCE2204_PPE_ACTION_COPY_TO_CPU = 2,
	QCE2204_PPE_ACTION_REDIRECT_TO_CPU = 3,
};

/* Buffer Management Configuration */
struct qce2204_ppe_bm_port_config {
	u32 port_id_start;
	u32 port_id_end;
	u32 pre_alloc;
	u32 in_fly_buf;
	u32 ceil;
	u32 weight;
	u32 resume_offset;
	u32 resume_ceil;
	bool dynamic;
};

/* Queue Management Configuration */
struct qce2204_ppe_qm_queue_config {
	u32 queue_start;
	u32 queue_end;
	u32 prealloc_buf;
	u32 ceil;
	u32 weight;
	u32 resume_offset;
	bool dynamic;
};

/* Scheduler Configuration */
struct qce2204_ppe_scheduler_cfg {
	u32 flow_id;
	u32 pri;
	u32 drr_node_id;
	u32 drr_node_wt;
	bool unit_is_packet;
	u32 frame_mode;
};

/* RSS Hash Configuration */
struct qce2204_ppe_rss_hash_cfg {
	u32 hash_seed;
	u32 hash_mask;
	bool hash_fragment_mode;
	u8 hash_sip_mix[4];
	u8 hash_dip_mix[4];
	u8 hash_protocol_mix;
	u8 hash_sport_mix;
	u8 hash_dport_mix;
	u8 hash_fin_inner[5];
	u8 hash_fin_outer[5];
};

/* Port Schedule Resource */
struct qce2204_ppe_port_schedule_resource {
	u32 ucastq_start;
	u32 ucastq_end;
	u32 mcastq_start;
	u32 mcastq_end;
	u32 flow_id_start;
	u32 flow_id_end;
	u32 l0node_start;
	u32 l0node_end;
	u32 l1node_start;
	u32 l1node_end;
};

/* Queue Destination Configuration */
struct qce2204_ppe_queue_ucast_dest {
	u32 dest_port;
	u32 src_profile;
	bool service_code_en;
	u32 service_code;
	bool cpu_code_en;
	u32 cpu_code;
};

/* VSI Member Configuration */
struct qce2204_ppe_vsi_member_cfg {
	u32 member_port_bitmap;
	u32 uuc_bitmap;
	u32 umc_bitmap;
	u32 bc_bitmap;
};

/* Port VSI Configuration */
struct qce2204_ppe_port_vsi_cfg {
	bool vsi_valid;
	u32 vsi;
};

/* STP State Configuration */
struct qce2204_ppe_stp_state_cfg {
	u32 stp_state;
};

/* Port MTU Configuration */
struct qce2204_ppe_port_mtu_cfg {
	u32 mtu;
	u32 mtu_cmd;
};

/* VLAN TPID Configuration */
struct qce2204_ppe_vlan_tpid_cfg {
	u16 ctpid;
	u16 stpid;
	u16 ctpid_ext;
	u16 stpid_ext;
	u8 ctpid_map;
	u8 stpid_map;
};

/* Port VLAN Role Configuration */
struct qce2204_ppe_port_vlan_role_cfg {
	bool port_role;  /* 0=edge port, 1=core port */
};

/* Ingress VLAN Translation Configuration */
struct qce2204_ppe_in_vlan_xlt_cfg {
	u32 port_id;
	u32 svid_fmt;
	bool svid_inc;
	u16 svid;
	u32 cvid_fmt;
	bool cvid_inc;
	u16 cvid;
	u32 svid_xlt_cmd;
	u16 svid_xlt;
	u32 cvid_xlt_cmd;
	u16 cvid_xlt;
	u32 cpcp_xlt_cmd;
	u32 tags_rmv;
	bool cnt_en;
	u32 cnt_id;
	bool vsi_cmd;
	u32 vsi;
	bool src_valid;
	bool src_type;
	u32 src_info;
	u32 fwd_cmd;
	bool dest_valid;
	u32 dest_info;
};

/* PPE Private Data Structure */
struct qce2204_ppe_priv {
	void __iomem *base;
	struct phy_device *phydev;
	bool switch_mode;
	u32 cpu_port_mask;
	u32 user_port_mask;
};

/* Port PPE Resource Allocation */
struct qce2204_port_ppe_res {
	bool allocated;
	u16 standalone_vid;
	u32 in_vlan_xlt_idx;
	u32 cpu_in_vlan_xlt_idx;
	u32 eg_vlan_xlt_idx;
};

/* PPE Constants */
#define QCE2204_PPE_QUEUE_INTER_PRI_NUM		16
#define QCE2204_PPE_QUEUE_HASH_NUM		256
#define QCE2204_PPE_QUEUE_SCH_PRI_NUM		8
#define QCE2204_PPE_EDMA_SC_BYPASS_ID		15
#define QCE2204_PPE_VSI_TBL_ENTRIES_NUM		64
//#define QCE2204_PPE_SCH_WITH_IPG_PREAMBLE_FRAME_CRC	2

/* Queue Base Offsets */
#define QCE2204_PPE_QUEUE_BASE_SERVICE_CODE	512
#define QCE2204_PPE_QUEUE_BASE_CPU_CODE		768

/* Destination Info Macros */
#define QCE2204_PPE_DEST_INFO_PORT_ID		0
#define QCE2204_PPE_DEST_INFO(type, value)	((type << 24) | (value))

/* Function Prototypes - Core PPE Functions */
int qce2204_ppe_hw_init(struct phy_device *phydev);
int qce2204_ppe_port_enable(struct qce2204_ppe_priv *priv, int port);
int qce2204_ppe_port_disable(struct qce2204_ppe_priv *priv, int port);

/* Buffer and Queue Management */
int qce2204_ppe_queue_scheduler_set(struct phy_device *phydev,
				    int node_id, bool flow_level, int port,
				    struct qce2204_ppe_scheduler_cfg scheduler_cfg);
int qce2204_ppe_queue_ucast_base_set(struct phy_device *phydev,
				     struct qce2204_ppe_queue_ucast_dest queue_dst,
				     int queue_base, int profile_id);
int qce2204_ppe_port_resource_get(struct phy_device *phydev, int port,
				  enum qce2204_ppe_resource_type type,
				  int *res_start, int *res_end);

/* VSI Management */
int qce2204_ppe_vsi_member_set(struct phy_device *phydev,
                               u32 vsi_id,
                               struct qce2204_ppe_vsi_member_cfg *cfg);
int qce2204_ppe_port_vsi_set(struct phy_device *phydev,
                             u32 port_id,
                             struct qce2204_ppe_port_vsi_cfg *cfg);

/* Bridge and STP */
int qce2204_ppe_stp_state_set(struct qce2204_ppe_priv *priv,
			      u32 port_id,
			      struct qce2204_ppe_stp_state_cfg *cfg);

/* Port Configuration */
int qce2204_ppe_port_mtu_set(struct qce2204_ppe_priv *priv,
			     u32 port_id,
			     struct qce2204_ppe_port_mtu_cfg *cfg);
int qce2204_ppe_counter_enable_set(struct phy_device *phydev, int port);

/* VLAN Configuration */
int qce2204_ppe_vlan_tpid_set(struct qce2204_ppe_priv *priv,
			      enum qce2204_ppe_direction dir,
			      struct qce2204_ppe_vlan_tpid_cfg *cfg);
int qce2204_ppe_port_vlan_role_set(struct qce2204_ppe_priv *priv,
				   u32 port_id,
				   enum qce2204_ppe_direction direction,
				   struct qce2204_ppe_port_vlan_role_cfg *cfg);
int qce2204_ppe_vlan_in_vlan_xlt_set(struct qce2204_ppe_priv *priv,
				     u32 index,
				     struct qce2204_ppe_in_vlan_xlt_cfg *cfg);

/* Switch Mode Setup Functions */
int qce2204_setup_none_tag_vsi(struct phy_device *phydev);
int qce2204_teardown_none_tag_vsi(struct phy_device *phydev);
int qce2204_setup_8021q_global(struct qce2204_ppe_priv *priv);
int qce2204_teardown_8021q_global(struct qce2204_ppe_priv *priv);
int qce2204_setup_8021q_tagging(struct qce2204_ppe_priv *priv, int port);
int qce2204_teardown_8021q_tagging(struct qce2204_ppe_priv *priv, int port);

/* U-Boot Specific Functions */
int qce2204_ppe_single_port_bridge_setup(struct qce2204_ppe_priv *priv);
int qce2204_ppe_basic_switch_init(struct qce2204_ppe_priv *priv);

/* MAC Management Functions */
int qce2204_port_mac_init(struct phy_device *phydev);
void qce2204_port_mac_deinit(struct phy_device *phydev);
int qce2204_phylink_mac_link_down(struct phy_device *phydev, int port,
				  phy_interface_t interface);
int qce2204_phylink_mac_link_up(struct phy_device *phydev, int port,
				int speed, int duplex,
				phy_interface_t interface,
				bool tx_pause, bool rx_pause);
int qce2204_port_link_up(struct phy_device *phydev, int port,
			int speed, int duplex,
			phy_interface_t interface,
			bool tx_pause, bool rx_pause);

/* Scheduler Configuration Structures (from Linux) */
struct qce2204_ppe_scheduler_bm_config {
	bool valid;
	u32 dir;
	u32 port;
	bool backup_port_valid;
	u32 backup_port;
};

struct qce2204_ppe_scheduler_qm_config {
	u32 ensch_port_bmp;
	u32 ensch_port;
	u32 desch_port;
	bool desch_backup_port_valid;
	u32 desch_backup_port;
};

struct qce2204_ppe_scheduler_port_config {
	u32 port;
	bool flow_level;
	u32 node_id;
	u32 loop_num;
	u32 pri_max;
	u32 flow_id;
	u32 drr_node_id;
};

/* MAC Register Definitions */
#define QCE2204_PPE_GMAC_ADDR(x)			(0x001000 + (x) * 0x200)
#define QCE2204_PPE_XGMAC_ADDR(x)			(0x500000 + (x) * 0x4000)

/* GMAC enable register */
#define QCE2204_PPE_GMAC_ENABLE_ADDR			0x0
#define QCE2204_PPE_GMAC_TXFCEN				BIT(6)
#define QCE2204_PPE_GMAC_RXFCEN				BIT(5)
#define QCE2204_PPE_GMAC_DUPLEX_FULL			BIT(4)
#define QCE2204_PPE_GMAC_TXEN				BIT(1)
#define QCE2204_PPE_GMAC_RXEN				BIT(0)

#define QCE2204_PPE_GMAC_TRXEN				(QCE2204_PPE_GMAC_TXEN | \
							 QCE2204_PPE_GMAC_RXEN)
#define QCE2204_PPE_GMAC_ENABLE_ALL			(QCE2204_PPE_GMAC_TXFCEN | \
							 QCE2204_PPE_GMAC_RXFCEN | \
							 QCE2204_PPE_GMAC_DUPLEX_FULL | \
							 QCE2204_PPE_GMAC_TXEN | \
							 QCE2204_PPE_GMAC_RXEN)

/* GMAC speed register */
#define QCE2204_PPE_GMAC_SPEED_ADDR			0x4
#define QCE2204_PPE_GMAC_SPEED_M			GENMASK(1, 0)
#define QCE2204_PPE_GMAC_SPEED_10			0
#define QCE2204_PPE_GMAC_SPEED_100			1
#define QCE2204_PPE_GMAC_SPEED_1000			2

/* GMAC control register */
#define QCE2204_PPE_GMAC_CTRL0_ADDR			0x18
#define QCE2204_PPE_GMAC_TX_THD_M			GENMASK(27, 24)
#define QCE2204_PPE_GMAC_TX_THD_DEFAULT			0x1
#define QCE2204_PPE_GMAC_MAXFRAME_SIZE_M		GENMASK(21, 8)
#define QCE2204_PPE_GMAC_CRS_SEL			BIT(6)

#define QCE2204_PPE_GMAC_CTRL_MASK			(QCE2204_PPE_GMAC_TX_THD_M | \
							 QCE2204_PPE_GMAC_MAXFRAME_SIZE_M | \
							 QCE2204_PPE_GMAC_CRS_SEL)

/* GMAC debug control register */
#define QCE2204_PPE_GMAC_CTRL1_ADDR			0x1c
#define QCE2204_PPE_GMAC_HIGH_IPG_M			GENMASK(15, 8)
#define QCE2204_PPE_GMAC_HIGH_IPG_DEFAULT		0xc

/* GMAC jumbo size register */
#define QCE2204_PPE_GMAC_JUMBO_SIZE_ADDR		0x30
#define QCE2204_PPE_GMAC_JUMBO_SIZE_M			GENMASK(13, 0)

/* GMAC MIB control register */
#define QCE2204_PPE_GMAC_MIB_CTRL_ADDR			0x34
#define QCE2204_PPE_GMAC_MIB_RD_CLR			BIT(2)
#define QCE2204_PPE_GMAC_MIB_RST			BIT(1)
#define QCE2204_PPE_GMAC_MIB_EN				BIT(0)

#define QCE2204_PPE_GMAC_MIB_CTRL_MASK			(QCE2204_PPE_GMAC_MIB_RD_CLR | \
							 QCE2204_PPE_GMAC_MIB_RST | \
							 QCE2204_PPE_GMAC_MIB_EN)

/* XGMAC TX configuration register */
#define QCE2204_PPE_XGMAC_TX_CONFIG_ADDR		0x0
#define QCE2204_PPE_XGMAC_SPEED_M			GENMASK(31, 29)
#define QCE2204_PPE_XGMAC_SPEED_10000_USXGMII		FIELD_PREP(QCE2204_PPE_XGMAC_SPEED_M, 4)
#define QCE2204_PPE_XGMAC_SPEED_10000			FIELD_PREP(QCE2204_PPE_XGMAC_SPEED_M, 0)
#define QCE2204_PPE_XGMAC_SPEED_5000			FIELD_PREP(QCE2204_PPE_XGMAC_SPEED_M, 5)
#define QCE2204_PPE_XGMAC_SPEED_2500_USXGMII		FIELD_PREP(QCE2204_PPE_XGMAC_SPEED_M, 6)
#define QCE2204_PPE_XGMAC_SPEED_2500			FIELD_PREP(QCE2204_PPE_XGMAC_SPEED_M, 2)
#define QCE2204_PPE_XGMAC_SPEED_1000			FIELD_PREP(QCE2204_PPE_XGMAC_SPEED_M, 3)
#define QCE2204_PPE_XGMAC_SPEED_100			QCE2204_PPE_XGMAC_SPEED_1000
#define QCE2204_PPE_XGMAC_SPEED_10			QCE2204_PPE_XGMAC_SPEED_1000
#define QCE2204_PPE_XGMAC_JD				BIT(16)
#define QCE2204_PPE_XGMAC_TXEN				BIT(0)

/* XGMAC RX configuration register */
#define QCE2204_PPE_XGMAC_RX_CONFIG_ADDR		0x4
#define QCE2204_PPE_XGMAC_GPSL_M			GENMASK(29, 16)
#define QCE2204_PPE_XGMAC_WD				BIT(7)
#define QCE2204_PPE_XGMAC_GPSLEN			BIT(6)
#define QCE2204_PPE_XGMAC_CST				BIT(2)
#define QCE2204_PPE_XGMAC_ACS				BIT(1)
#define QCE2204_PPE_XGMAC_RXEN				BIT(0)

#define QCE2204_PPE_XGMAC_RX_CONFIG_MASK		(QCE2204_PPE_XGMAC_GPSL_M | \
							 QCE2204_PPE_XGMAC_WD | \
							 QCE2204_PPE_XGMAC_GPSLEN | \
							 QCE2204_PPE_XGMAC_CST | \
							 QCE2204_PPE_XGMAC_ACS | \
							 QCE2204_PPE_XGMAC_RXEN)

/* XGMAC packet filter register */
#define QCE2204_PPE_XGMAC_PKT_FILTER_ADDR		0x8
#define QCE2204_PPE_XGMAC_RA				BIT(31)
#define QCE2204_PPE_XGMAC_PCF_M				GENMASK(7, 6)
#define QCE2204_PPE_XGMAC_PR				BIT(0)

#define QCE2204_PPE_XGMAC_PKT_FILTER_MASK		(QCE2204_PPE_XGMAC_RA | \
							 QCE2204_PPE_XGMAC_PCF_M | \
							 QCE2204_PPE_XGMAC_PR)
#define QCE2204_PPE_XGMAC_PKT_FILTER_VAL		(QCE2204_PPE_XGMAC_RA | \
							 QCE2204_PPE_XGMAC_PR | \
							 FIELD_PREP(QCE2204_PPE_XGMAC_PCF_M, 0x2))

/* XGMAC watchdog timeout register */
#define QCE2204_PPE_XGMAC_WD_TIMEOUT_ADDR		0xc
#define QCE2204_PPE_XGMAC_PWE				BIT(8)
#define QCE2204_PPE_XGMAC_WTO_M				GENMASK(3, 0)

#define QCE2204_PPE_XGMAC_WD_TIMEOUT_MASK		(QCE2204_PPE_XGMAC_PWE | \
							 QCE2204_PPE_XGMAC_WTO_M)
#define QCE2204_PPE_XGMAC_WD_TIMEOUT_VAL		(QCE2204_PPE_XGMAC_PWE | \
							 FIELD_PREP(QCE2204_PPE_XGMAC_WTO_M, 0xb))

/* XGMAC TX flow control register */
#define QCE2204_PPE_XGMAC_TX_FLOW_CTRL_ADDR		0x70
#define QCE2204_PPE_XGMAC_PAUSE_TIME_M			GENMASK(31, 16)
#define QCE2204_PPE_XGMAC_TXFCEN			BIT(1)

/* XGMAC RX flow control register */
#define QCE2204_PPE_XGMAC_RX_FLOW_CTRL_ADDR		0x90
#define QCE2204_PPE_XGMAC_RXFCEN			BIT(0)

/* XGMAC management counters control register */
#define QCE2204_PPE_XGMAC_MMC_CTRL_ADDR			0x800
#define QCE2204_PPE_XGMAC_MCF				BIT(3)
#define QCE2204_PPE_XGMAC_CNTRST			BIT(0)

/* QCE2204 port MAC max frame size which including 4bytes FCS */
#define QCE2204_PORT_MAC_MAX_FRAME_SIZE			0x3000

/* PPE port bridge configuration */
#define QCE2204_PPE_PORT_BRIDGE_CTRL_ADDR               0x00540300
#define QCE2204_PPE_PORT_BRIDGE_CTRL_ENTRIES            9
#define QCE2204_PPE_PORT_BRIDGE_CTRL_INC                4
#define QCE2204_PPE_PORT_BRIDGE_NEW_LRN_EN              BIT(0)
#define QCE2204_PPE_PORT_BRIDGE_STA_MOVE_LRN_EN         BIT(3)
#define QCE2204_PPE_PORT_BRIDGE_ISOL_BITMAP             GENMASK(16, 8)
#define QCE2204_PPE_PORT_BRIDGE_TXMAC_EN                BIT(17)

/* BM port flow control mode */
#define QCE2204_PPE_BM_PORT_FC_MODE_ADDR                0x00800100
#define QCE2204_PPE_BM_PORT_FC_MODE_ENTRIES             6
#define QCE2204_PPE_BM_PORT_FC_MODE_INC                 0x4
#define QCE2204_PPE_BM_PORT_FC_MODE_EN                  BIT(0)


/* MAC Type Enumeration */
enum qce2204_port_mac_type {
	QCE2204_PORT_MAC_TYPE_GMAC = 0,
	QCE2204_PORT_MAC_TYPE_XGMAC = 1,
};

/* Register Access Helpers - Implemented in qcom_qce2204_ppe.c */
u32 qce2204_ppe_read(struct phy_device *phydev, u32 offset);
int qce2204_ppe_write(struct phy_device *phydev, u32 offset, u32 value);
int qce2204_ppe_bulk_read(struct phy_device *phydev, u32 offset, u32 *data, size_t count);
int qce2204_ppe_bulk_write(struct phy_device *phydev, u32 offset, const u32 *data, size_t count);

/* PPE Helper Functions for MAC Driver */
static inline int qce2204_ppe_update_bits(struct phy_device *phydev, u32 offset, u32 mask, u32 val)
{
	u32 reg_val;
	int ret;

	reg_val = qce2204_ppe_read(phydev, offset);
	reg_val &= ~mask;
	reg_val |= val;
	ret = qce2204_ppe_write(phydev, offset, reg_val);
	return ret;
}

static inline int qce2204_ppe_set_bits(struct phy_device *phydev, u32 offset, u32 mask)
{
	return qce2204_ppe_update_bits(phydev, offset, mask, mask);
}

static inline int qce2204_ppe_clear_bits(struct phy_device *phydev, u32 offset, u32 mask)
{
	return qce2204_ppe_update_bits(phydev, offset, mask, 0);
}

#endif /* __QCE2204_PPE_H__ */
