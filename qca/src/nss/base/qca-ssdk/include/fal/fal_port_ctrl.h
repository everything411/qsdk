/*
 * Copyright (c) 2012, 2015-2019, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*qca808x_start*/
/**
 * @defgroup fal_port_ctrl FAL_PORT_CONTROL
 * @{
 */
#ifndef _FAL_PORTCTRL_H_
#define _FAL_PORTCTRL_H_

#ifdef __cplusplus
extern "c" {
#endif

#include "sw.h"
#include "fal/fal_type.h"
#include <linux/phy.h>
#include "fal_port_ctrl_legacy.h"

	typedef enum {
		FAL_HALF_DUPLEX = 0,
		FAL_FULL_DUPLEX,
		FAL_DUPLEX_BUTT = 0xffff
	} fal_port_duplex_t;

	typedef enum {
		FAL_SPEED_10 = 10,
		FAL_SPEED_100 = 100,
		FAL_SPEED_1000 = 1000,
		FAL_SPEED_2500 = 2500,
		FAL_SPEED_5000 = 5000,
		FAL_SPEED_10000 = 10000,
		FAL_SPEED_25000 = 25000,
		FAL_SPEED_BUTT = 0xffff,
	} fal_port_speed_t;

	typedef enum {
		FAL_CABLE_STATUS_NORMAL = 0,
		FAL_CABLE_STATUS_SHORT = 1,
		FAL_CABLE_STATUS_OPENED = 2,
		FAL_CABLE_STATUS_INVALID = 3,
		FAL_CABLE_STATUS_CROSSOVERA = 4,
		FAL_CABLE_STATUS_CROSSOVERB = 5,
		FAL_CABLE_STATUS_CROSSOVERC = 6,
		FAL_CABLE_STATUS_CROSSOVERD = 7,
		FAL_CABLE_STATUS_LOW_MISMATCH =8,
		FAL_CABLE_STATUS_HIGH_MISMATCH =9,
		FAL_CABLE_STATUS_BUTT = 0xffff,
	} fal_cable_status_t;

struct port_phy_status
{
	a_uint32_t link_status;
	fal_port_speed_t speed;
	fal_port_duplex_t duplex;
	a_bool_t tx_flowctrl;
	a_bool_t rx_flowctrl;
};

#define FAL_ENABLE                1
#define FAL_DISABLE               0
#define FAL_MAX_PORT_NUMBER       8
#define FAL_DEFAULT_LOOPBACK_RATE 14

//phy autoneg adv
#define FAL_PHY_ADV_10T_HD      0x01
#define FAL_PHY_ADV_10T_FD      0x02
#define FAL_PHY_ADV_100TX_HD    0x04
#define FAL_PHY_ADV_100TX_FD    0x08
#define FAL_PHY_ADV_PAUSE       0x10
#define FAL_PHY_ADV_ASY_PAUSE   0x20
#define FAL_PHY_ADV_AUTONEG     0x40
//#define FAL_PHY_ADV_1000T_HD  0x100
#define FAL_PHY_ADV_1000T_FD    0x200
#define FAL_PHY_ADV_1000BX_HD   0x400
#define FAL_PHY_ADV_1000BX_FD   0x800
#define FAL_PHY_ADV_2500T_FD    0x1000
#define FAL_PHY_ADV_5000T_FD    0x2000
#define FAL_PHY_ADV_10000T_FD   0x4000
#define FAL_PHY_ADV_10G_R_FD    0x8000

#define FAL_DEFAULT_MAX_FRAME_SIZE 0x5ee

#define FAL_PHY_ADV_FE_SPEED_ALL   \
    (FAL_PHY_ADV_10T_HD | FAL_PHY_ADV_10T_FD | FAL_PHY_ADV_100TX_HD |\
     FAL_PHY_ADV_100TX_FD)

#define FAL_PHY_ADV_GE_SPEED_ALL   \
    (FAL_PHY_ADV_10T_HD | FAL_PHY_ADV_10T_FD | FAL_PHY_ADV_100TX_HD |\
     FAL_PHY_ADV_100TX_FD | FAL_PHY_ADV_1000T_FD)

#define FAL_PHY_ADV_BX_SPEED_ALL   \
    (FAL_PHY_ADV_1000BX_HD | FAL_PHY_ADV_1000BX_FD |FAL_PHY_ADV_10G_R_FD)

#define FAL_PHY_ADV_XGE_SPEED_ALL   \
		(FAL_PHY_ADV_2500T_FD | FAL_PHY_ADV_5000T_FD | FAL_PHY_ADV_10000T_FD)

#define FAL_PHY_FE_ADV_ALL         \
    (FAL_PHY_ADV_FE_SPEED_ALL | FAL_PHY_ADV_PAUSE | FAL_PHY_ADV_ASY_PAUSE)
#define FAL_PHY_GE_ADV_ALL         \
    (FAL_PHY_ADV_GE_SPEED_ALL | FAL_PHY_ADV_PAUSE | FAL_PHY_ADV_ASY_PAUSE)

#define FAL_PHY_COMBO_ADV_ALL         \
    (FAL_PHY_ADV_BX_SPEED_ALL | FAL_PHY_ADV_GE_SPEED_ALL | FAL_PHY_ADV_XGE_SPEED_ALL|\
FAL_PHY_ADV_PAUSE | FAL_PHY_ADV_ASY_PAUSE)

//phy interrupt flag
#define FAL_PHY_INTR_SPEED_CHANGE         0x1
#define FAL_PHY_INTR_DUPLEX_CHANGE        0x2
#define FAL_PHY_INTR_STATUS_UP_CHANGE     0x4
#define FAL_PHY_INTR_STATUS_DOWN_CHANGE   0x8
#define FAL_PHY_INTR_BX_FX_STATUS_UP_CHANGE   0x10
#define FAL_PHY_INTR_BX_FX_STATUS_DOWN_CHANGE   0x20
#define FAL_PHY_INTR_MEDIA_STATUS_CHANGE   0x40
#define FAL_PHY_INTR_WOL_STATUS   0x80
#define FAL_PHY_INTR_POE_STATUS   0x100

/* phy eee */
#define FAL_PHY_EEE_10BASE_T  0x1
#define FAL_PHY_EEE_100BASE_T  0x2
#define FAL_PHY_EEE_1000BASE_T  0x4
#define FAL_PHY_EEE_2500BASE_T  0x8
#define FAL_PHY_EEE_5000BASE_T  0x10
#define FAL_PHY_EEE_10000BASE_T  0x20
#define FAL_PHY_EEE_ALL_ADV (FAL_PHY_EEE_100BASE_T | FAL_PHY_EEE_1000BASE_T |\
	FAL_PHY_EEE_2500BASE_T | FAL_PHY_EEE_5000BASE_T |FAL_PHY_EEE_10000BASE_T)

#define FAL_SFP_PHY_ADDR 29

	typedef struct {
		a_uint16_t pair_a_status;
		a_uint16_t pair_b_status;
		a_uint16_t pair_c_status;
		a_uint16_t pair_d_status;
		a_uint32_t pair_a_len;
		a_uint32_t pair_b_len;
		a_uint32_t pair_c_len;
		a_uint32_t pair_d_len;
	} fal_port_cdt_t;

/*below is new add for malibu phy*/

/** Phy mdix mode */
	typedef enum {
		PHY_MDIX_AUTO = 0,
			      /**< Auto MDI/MDIX */
		PHY_MDIX_MDI = 1,
			      /**< Fixed MDI */
		PHY_MDIX_MDIX = 2
			      /**< Fixed MDIX */
	} fal_port_mdix_mode_t;

/** Phy mdix status */
	typedef enum {
		PHY_MDIX_STATUS_MDI = 0,
				    /**< Fixed MDI */
		PHY_MDIX_STATUS_MDIX = 1
				    /**< Fixed MDIX */
	} fal_port_mdix_status_t;

/** Phy master mode */
	typedef enum {
		PHY_MASTER_MASTER = 0,
				/**< Phy manual MASTER configuration */
		PHY_MASTER_SLAVE = 1,
				/**< Phy manual SLAVE configuration */
		PHY_MASTER_AUTO = 2
				/**< Phy automatic MASTER/SLAVE configuration */
	} fal_port_master_t;

/** Phy preferred medium type */
	typedef enum {
		PHY_MEDIUM_COPPER = 0,
				/**< Copper */
		PHY_MEDIUM_FIBER = 1,
				/**< Fiber */
		PHY_MEDIUM_MAX

	} fal_port_medium_t;

/** Phy pages */
	typedef enum {
		PHY_SGBX_PAGES = 0,
				/**< sgbx pages */
		PHY_COPPER_PAGES = 1
				/**< copper pages */
	} fal_port_reg_pages_t;

/** Phy preferred Fiber mode */
	typedef enum {
		PHY_FIBER_100FX = 0,
				/**< 100FX fiber mode */
		PHY_FIBER_1000BX = 1,
				/**< 1000BX fiber mode */
		PHY_FIBER_10G_R = 2,
	} fal_port_fiber_mode_t;

/** Phy reset status */
	typedef enum {
		PHY_RESET_DONE = 0,
				/**< Phy reset done */
		PHY_RESET_BUSY = 1
				/**< Phy still in reset process */
	} fal_port_reset_status_t;

/** Phy auto-negotiation status */
	typedef enum {
		PHY_AUTO_NEG_STATUS_BUSY = 0,
				    /**< Phy still in auto-negotiation process */
		PHY_AUTO_NEG_STATUS_DONE = 1
				    /**< Phy auto-negotiation done */
	} fal_port_auto_neg_status_t;


/** Phy interface mode */
	typedef enum {
		PHY_SGMII_BASET = PHY_INTERFACE_MODE_SGMII,
				/**< SGMII mode */
		PORT_RGMII_BASET = PHY_INTERFACE_MODE_RGMII,
				/**< RGMII mode */
		PHY_PSGMII_BASET = PHY_INTERFACE_MODE_PSGMII,
				/**< PSGMII mode */
		PORT_QSGMII = PHY_INTERFACE_MODE_QSGMII,
				/**<QSGMII mode*/
		PORT_SGMII_FIBER = PHY_INTERFACE_MODE_1000BASEX,
				/** <PORT SGMII_FIBER mode*/
		PORT_SGMII_PLUS = PHY_INTERFACE_MODE_2500BASEX,
				/**<PORT_SGMII_PLUS mode*/
		PORT_10GBASE_R = PHY_INTERFACE_MODE_10GBASER,
				/**<PORT_10GBASE_R mode*/
		PORT_USXGMII = PHY_INTERFACE_MODE_USXGMII,
				/**<PORT_USXGMII mode*/
		PORT_UQXGMII = PHY_INTERFACE_MODE_QUSGMII,
				/**<PORT_UQXGMII mode*/
		PORT_25GBASE_R = PHY_INTERFACE_MODE_25GBASER,
				/**<PORT_25GBASE_R mode*/
		PORT_INTERNAL = PHY_INTERFACE_MODE_INTERNAL,
				/**<PORT_INTERNAL mode*/
		PORT_GPON = 0xFB,
				/**<GPON mode*/
		PORT_XGPON,
				/**<XGPON mode*/
		PORT_XGSPON,
				/**<XGSPON mode*/
		PORT_INTERFACE_MODE_AUTO,
				/**PORT AUTO INTERFACE MODE*/
		PORT_INTERFACE_MODE_MAX = 0xFF
	} fal_port_interface_mode_t;

/** Phy counter information */
typedef struct {
	a_uint64_t RxGoodFrame;
	a_uint64_t RxFcsErr;
	a_uint64_t TxGoodFrame;
	a_uint64_t TxFcsErr;
	a_uint64_t SysRxGoodFrame;
	a_uint64_t SysRxFcsErr;
	a_uint64_t SysTxGoodFrame;
	a_uint64_t SysTxFcsErr;
} fal_port_counter_info_t;

/*above is new add for malibu phy*/
/*qca808x_end*/
typedef struct {
	a_bool_t enable;
	a_uint32_t capability;
	a_uint32_t lpi_sleep_timer; /* unit is us*/
	a_uint32_t advertisement;
	a_uint32_t lpi_tx_enable;
	a_uint32_t eee_status;
	a_uint32_t lpi_wakeup_timer; /* unit is us*/
	a_uint32_t link_partner_advertisement;
} fal_port_eee_cfg_t;

/* Bit values match nss_phy.h's NSS_PHY_FR_* bitmap exactly, since
 * nss-phy passes its bitmap straight through as this struct's
 * negotiated field.
 */
#define FAL_PORT_FR_2500BASE_T                  0x40
#define FAL_PORT_FR_5000BASE_T                  0x80
#define FAL_PORT_FR_10000BASE_T                 0x100
#define FAL_PORT_FR_THP_BYPASS_2500BASE_T       0x200
#define FAL_PORT_FR_THP_BYPASS_5000BASE_T       0x400
#define FAL_PORT_FR_THP_BYPASS_10000BASE_T      0x800
#define FAL_PORT_FR_CISCO_ABILITY               0x1000
#define FAL_PORT_FR_CISCO_THP_BYPASS_ABILITY    0x2000
#define FAL_PORT_FR_CISCO_EXTEND_WAIT_ABILITY   0x4000
#define FAL_PORT_FR_CISCO_DISABLE_TIMER_ABILITY 0x8000

typedef struct {
	a_bool_t   ieee_fr_en;
	a_bool_t   cisco_fr_en;
} fal_port_fr_cfg_t;

typedef struct {
	a_bool_t   ieee_enabled;
	a_bool_t   cisco_enabled;
	a_uint32_t negotiated;      /* bitmap of FAL_PORT_FR_* */
	a_bool_t   active;
	a_bool_t   success;
	a_bool_t   fail;
	a_uint64_t rx_count;        /* since last link-up, SW accumulated */
	a_uint64_t tx_count;        /* since last link-up, SW accumulated */
	a_uint64_t rx_total;        /* cumulative since bootup, SW accumulated */
	a_uint64_t tx_total;        /* cumulative since bootup, SW accumulated */
} fal_port_fr_status_t;

typedef enum {
	FAL_MTU_ETHERNET = 0,
	FAL_MTU_IP,
} fal_mtu_type_t;

typedef struct {
	a_uint32_t	mtu_size;
	fal_fwd_cmd_t	action;
} fal_mtu_ctrl_t;

typedef struct {
	a_bool_t	mtu_enable;/*add it for ipq95xx*/
	fal_mtu_type_t	mtu_type;/*add it for ipq95xx*/
	a_uint32_t	extra_header_len;/*add it for ipq95xx*/
	a_uint32_t	eg_vlan_tag_flag;/*bit 0 ctag, bit 1 stag,add it for ipq95xx*/
} fal_mtu_cfg_t;

typedef struct {
	a_uint32_t 		mru_size;
	fal_fwd_cmd_t 	action;
} fal_mru_ctrl_t;

typedef enum {
	FAL_SRC_FILTER_MODE_VP = 0,
	FAL_SRC_FILTER_MODE_PHYSICAL = 1,
} fal_src_filter_mode_t;

typedef struct {
	a_bool_t	src_filter_enable;
	fal_src_filter_mode_t	src_filter_mode;
} fal_src_filter_config_t;

typedef struct {
	a_bool_t enable;
	a_bool_t crc_stripped;
	a_uint32_t loopback_rate; /*Mpps*/
}fal_loopback_config_t;

typedef struct {
	a_bool_t loopback_enable;
} fal_port_8023ah_ctrl_t;

typedef enum {
	FAL_PORT_CNT_MODE_IP_PKT, /* the outer IP header + innder packet counted */
	FAL_PORT_CNT_MODE_FULL_PKT, /* for RX, inner packet length,
				      * for TX vport, the full packet(outer header + inner packet)
				      * lenght counter */
	FAL_PORT_CNT_MODE_BUTT,
} fal_port_cnt_mode_t;

typedef struct {
	a_bool_t rx_cnt_en; /* Enable/disable port rx counter */
	a_bool_t tl_rx_cnt_en; /* Enable/disable tunnel source port rx counter */
	a_bool_t uc_tx_cnt_en; /* Enable/disable port unicast tx and l3_if tx counter */
	a_bool_t mc_tx_cnt_en; /* Enable/disable physical port multicast tx counter */
	fal_port_cnt_mode_t rx_cnt_mode; /* as described as fal_port_cnt_mode_t */
	fal_port_cnt_mode_t tx_cnt_mode; /* as described as fal_port_cnt_mode_t */
} fal_port_cnt_cfg_t;

typedef struct {
	a_uint32_t rx_pkt_cnt; /* rx packet counter */
	a_uint64_t rx_byte_cnt; /* rx byte counter */
	a_uint32_t rx_drop_pkt_cnt; /* rx drop packet counter */
	a_uint64_t rx_drop_byte_cnt; /* rx drop byte counter */
	a_uint32_t tx_pkt_cnt; /* tx packet counter */
	a_uint64_t tx_byte_cnt; /* tx byte counter */
	a_uint32_t tx_drop_pkt_cnt; /* tx drop packet counter */
	a_uint64_t tx_drop_byte_cnt; /* tx drop byte counter */
	a_uint32_t buff_empty_drop; /* ingress buffer empty drop packet counter*/
} fal_port_cnt_t;

typedef struct {
	a_bool_t  copper_link_status; /*copper link status*/
	a_bool_t  fiber_link_status; /*fiber link status*/
} fal_port_combo_link_status_t;

typedef enum {
	FAL_ERP_ACTIVE = 0, /* erp active mode */
	FAL_ERP_LOW_POWER = 1, /* erp low power mode */
} fal_port_erp_power_mode_t;

typedef struct {
	a_uint32_t pps_out_sel; /* 0: port0... 5: port5 */
	a_uint32_t pps_in_sel;  /* 0: input pps 1: output pps */
} fal_port_pps_ctrl_t;

typedef struct {
	a_uint32_t supported_fec; /* supported fec */
	a_uint32_t configured_fec; /* configured fec */
	a_uint32_t active_fec; /* active fec */
} fal_port_fec_config_t;

sw_error_t
fal_port_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t max_frame);

sw_error_t
fal_port_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t *max_frame);

sw_error_t
fal_port_mtu_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_ctrl_t *ctrl);

sw_error_t
fal_port_mtu_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_ctrl_t *ctrl);

sw_error_t
fal_port_mru_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mru_ctrl_t *ctrl);

sw_error_t
fal_port_mru_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mru_ctrl_t *ctrl);

sw_error_t
fal_port_mtu_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_cfg_t *mtu_cfg);

sw_error_t
fal_port_mtu_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_cfg_t *mtu_cfg);
sw_error_t
fal_port_mru_mtu_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t mru_size, a_uint32_t mtu_size);

sw_error_t
fal_port_mru_mtu_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t *mru_size, a_uint32_t *mtu_size);
/*qca808x_start*/
sw_error_t
fal_port_duplex_set(a_uint32_t dev_id, fal_port_t port_id,
				fal_port_duplex_t duplex);

sw_error_t
fal_port_duplex_get(a_uint32_t dev_id, fal_port_t port_id,
				fal_port_duplex_t * pduplex);

sw_error_t
fal_port_speed_set(a_uint32_t dev_id, fal_port_t port_id,
			       fal_port_speed_t speed);

sw_error_t
fal_port_speed_get(a_uint32_t dev_id, fal_port_t port_id,
			       fal_port_speed_t * pspeed);

sw_error_t
fal_port_autoneg_status_get(a_uint32_t dev_id, fal_port_t port_id,
					a_bool_t * status);

sw_error_t
fal_port_autoneg_enable(a_uint32_t dev_id, fal_port_t port_id);

sw_error_t
fal_port_autoneg_restart(a_uint32_t dev_id, fal_port_t port_id);

sw_error_t
fal_port_autoneg_adv_set(a_uint32_t dev_id, fal_port_t port_id,
				     a_uint32_t autoadv);
sw_error_t
fal_port_autoneg_adv_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_uint32_t * autoadv);
/*qca808x_end*/
sw_error_t
fal_port_flowctrl_set(a_uint32_t dev_id, fal_port_t port_id,
				  a_bool_t enable);
sw_error_t
fal_port_flowctrl_get(a_uint32_t dev_id, fal_port_t port_id,
				  a_bool_t * enable);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_mac_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t enable);

sw_error_t
fal_port_mac_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t * enable);
#endif
sw_error_t
fal_port_flowctrl_forcemode_set(a_uint32_t dev_id,
					    fal_port_t port_id,
					    a_bool_t enable);
sw_error_t
fal_port_flowctrl_forcemode_get(a_uint32_t dev_id,
					    fal_port_t port_id,
					    a_bool_t * enable);
sw_error_t
fal_port_txmac_status_set(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t enable);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_txmac_status_get(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t * enable);
#endif
sw_error_t
fal_port_rxmac_status_set(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t enable);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_rxmac_status_get(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t * enable);
#endif
sw_error_t
fal_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t enable);

sw_error_t
fal_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t * enable);

sw_error_t
fal_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t enable);

sw_error_t
fal_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t * enable);
/*qca808x_start*/
sw_error_t
fal_port_link_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t * status);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_ports_link_status_get(a_uint32_t dev_id, a_uint32_t * status);
/*qca808x_end*/


sw_error_t
fal_vch_bp_thres_get(a_uint32_t dev_id, a_uint32_t vch_id,
					 a_uint16_t * on_thres,
					 a_uint16_t * off_thres);
/*qca808x_start*/
sw_error_t
fal_port_powersave_set(a_uint32_t dev_id, fal_port_t port_id,
			  a_bool_t enable);


sw_error_t
fal_port_powersave_get(a_uint32_t dev_id, fal_port_t port_id,
			  a_bool_t * enable);


sw_error_t
fal_port_hibernate_set(a_uint32_t dev_id, fal_port_t port_id,
			  a_bool_t enable);


sw_error_t
fal_port_hibernate_get(a_uint32_t dev_id, fal_port_t port_id,
			  a_bool_t * enable);
#endif
sw_error_t
fal_port_cdt(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t mdi_pair, fal_cable_status_t * cable_status,
		a_uint32_t * cable_len);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_8023az_set(a_uint32_t dev_id, fal_port_t port_id,
				a_bool_t enable);

sw_error_t
fal_port_8023az_get(a_uint32_t dev_id, fal_port_t port_id,
				a_bool_t * enable);

sw_error_t
fal_port_mdix_set(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_mdix_mode_t mode);

sw_error_t
fal_port_mdix_get(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_mdix_mode_t * mode);

sw_error_t
fal_port_mdix_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     fal_port_mdix_status_t * mode);
/*qca808x_end*/
#endif

sw_error_t
fal_port_combo_prefer_medium_set(a_uint32_t dev_id,
					     a_uint32_t port_id,
					     fal_port_medium_t medium);

sw_error_t
fal_port_combo_prefer_medium_get(a_uint32_t dev_id,
						     a_uint32_t port_id,
						     fal_port_medium_t *
						     medium);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_combo_medium_status_get(a_uint32_t dev_id,
						     a_uint32_t port_id,
						     fal_port_medium_t *
						     medium);
sw_error_t
fal_port_combo_fiber_mode_set(a_uint32_t dev_id,
						  a_uint32_t port_id,
						  fal_port_fiber_mode_t mode);

sw_error_t
fal_port_combo_fiber_mode_get(a_uint32_t dev_id,
						  a_uint32_t port_id,
						  fal_port_fiber_mode_t * mode);
/*qca808x_start*/
sw_error_t
fal_port_local_loopback_set(a_uint32_t dev_id,
						fal_port_t port_id,
						a_bool_t enable);

sw_error_t
fal_port_local_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
					a_bool_t * enable);

sw_error_t
fal_port_remote_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
					 a_bool_t enable);

sw_error_t
fal_port_remote_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
					 a_bool_t * enable);
sw_error_t
fal_port_reset(a_uint32_t dev_id, fal_port_t port_id);
#endif
sw_error_t
fal_port_power_off(a_uint32_t dev_id, fal_port_t port_id);

sw_error_t
fal_port_power_on(a_uint32_t dev_id, fal_port_t port_id);

sw_error_t
fal_port_interface_mode_get(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t * mode);

sw_error_t
fal_port_interface_mode_set(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t mode);

sw_error_t
fal_port_interface_mode_apply(a_uint32_t dev_id);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_magic_frame_mac_set(a_uint32_t dev_id, fal_port_t port_id,
				   fal_mac_addr_t * mac);

sw_error_t
fal_port_magic_frame_mac_get(a_uint32_t dev_id, fal_port_t port_id,
				   fal_mac_addr_t * mac);

sw_error_t
fal_port_phy_id_get(a_uint32_t dev_id, fal_port_t port_id,
		      a_uint16_t * org_id, a_uint16_t * rev_id);

sw_error_t
fal_port_wol_status_set(a_uint32_t dev_id, fal_port_t port_id,
			      a_bool_t enable);

 /*all indirect */
sw_error_t
fal_port_wol_status_get(a_uint32_t dev_id, fal_port_t port_id,
			      a_bool_t * enable);

sw_error_t
fal_port_interface_mode_status_get(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t * mode);

sw_error_t
fal_debug_phycounter_set(a_uint32_t dev_id, fal_port_t port_id,
		   a_bool_t enable);

sw_error_t
fal_debug_phycounter_get(a_uint32_t dev_id, fal_port_t port_id,
		   a_bool_t * enable);

sw_error_t
fal_debug_phycounter_show(a_uint32_t dev_id, fal_port_t port_id,
				 fal_port_counter_info_t * port_counter_info);
#endif
/*qca808x_end*/
sw_error_t
fal_port_source_filter_status_get(a_uint32_t dev_id,
				fal_port_t port_id, a_bool_t * enable);

sw_error_t
fal_port_source_filter_enable(a_uint32_t dev_id,
				fal_port_t port_id, a_bool_t enable);

sw_error_t
fal_port_source_filter_config_get(a_uint32_t dev_id,
	fal_port_t port_id, fal_src_filter_config_t * src_filter_config);

sw_error_t
fal_port_source_filter_config_set(a_uint32_t dev_id,
	fal_port_t port_id, fal_src_filter_config_t *src_filter_config);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_interface_3az_status_set(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t enable);

sw_error_t
fal_port_interface_3az_status_get(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t * enable);
#endif

sw_error_t
fal_port_promisc_mode_get(a_uint32_t dev_id,fal_port_t port_id,a_bool_t *enable);

sw_error_t
fal_port_promisc_mode_set(a_uint32_t dev_id,fal_port_t port_id,a_bool_t enable);
sw_error_t
fal_port_interface_eee_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg);
sw_error_t
fal_port_interface_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg);
sw_error_t
fal_switch_port_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_loopback_config_t *loopback_cfg);
sw_error_t
fal_switch_port_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_loopback_config_t *loopback_cfg);

sw_error_t
fal_switch_loopback_port_get(a_uint32_t dev_id, fal_port_t *port_id);

#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_8023ah_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_8023ah_ctrl_t *port_8023ah_ctrl);

sw_error_t
fal_port_8023ah_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_8023ah_ctrl_t *port_8023ah_ctrl);
#endif

sw_error_t
fal_port_flow_ctrl_thres_set(a_uint32_t dev_id, a_uint32_t port_id,
		a_uint16_t on_thres, a_uint16_t off_thres);
sw_error_t
fal_port_flow_ctrl_thres_get(a_uint32_t dev_id, a_uint32_t port_id,
		a_uint16_t *on_thres, a_uint16_t *off_thres);
sw_error_t
fal_port_rx_fifo_thres_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint16_t thres);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_rx_fifo_thres_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint16_t *thres);
sw_error_t
fal_vch_bp_config_get(a_uint32_t dev_id, a_uint32_t vch_id, a_bool_t *status);
sw_error_t
fal_vch_bp_config_set(a_uint32_t dev_id, a_uint32_t vch_id, a_bool_t status);
sw_error_t
fal_vch_bp_stats_get(a_uint32_t dev_id, a_uint32_t vch_id, a_uint32_t *stats);
#endif
sw_error_t
fal_port_cnt_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg);

sw_error_t
fal_port_cnt_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg);

sw_error_t
fal_port_cnt_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_t *port_cnt);

sw_error_t
fal_port_cnt_flush(a_uint32_t dev_id, fal_port_t port_id);
#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_vch_bp_thres_set(a_uint32_t dev_id, a_uint32_t vch_id,
		a_uint16_t on_thres, a_uint16_t off_thres);
#endif
sw_error_t fal_port_combo_link_status_get (a_uint32_t dev_id,
		fal_port_t port_id, fal_port_combo_link_status_t * status);
sw_error_t
fal_port_erp_power_mode_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_erp_power_mode_t power_mode);
sw_error_t
fal_port_erp_power_mode_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_erp_power_mode_t *power_mode);
sw_error_t
fal_erp_standby_enter(a_uint32_t dev_id, a_uint32_t active_pbmap);
sw_error_t
fal_erp_standby_exit(a_uint32_t dev_id);

sw_error_t
fal_port_pps_ctrl_set(a_uint32_t dev_id, fal_port_pps_ctrl_t *pps_ctrl);

sw_error_t
fal_port_pps_ctrl_get(a_uint32_t dev_id, fal_port_pps_ctrl_t *pps_ctrl);

sw_error_t
fal_port_fastretrain_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_fr_cfg_t *cfg);
sw_error_t
fal_port_fastretrain_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_fr_cfg_t *cfg);
sw_error_t
fal_port_fastretrain_status_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_fr_status_t *status);
sw_error_t
fal_port_fastretrain_trigger(a_uint32_t dev_id, fal_port_t port_id);

sw_error_t
fal_port_fec_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_fec_config_t *ptfec);

sw_error_t
fal_port_fec_get(a_uint32_t dev_id, a_uint32_t port_id, fal_port_fec_config_t *ptfec);
/*qca808x_start*/
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _FAL_PORTCTRL_H_ */
/**
 * @}
 */
/*qca808x_end*/
