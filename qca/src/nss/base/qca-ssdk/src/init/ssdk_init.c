/*
 * Copyright (c) 2012, 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*qca808x_start*/
#include "sw.h"
#include "ssdk_init.h"
#include "fal_init.h"
#include "fal.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_phy.h"
/*qca808x_end*/
#include "ssdk_dts.h"
#include "ssdk_interrupt.h"
#include <linux/kconfig.h>
#include <linux/version.h>
#include <linux/kernel.h>
/*qca808x_start*/
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/types.h>
/*qca808x_end*/
//#include <asm/mach-types.h>
#include <generated/autoconf.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
/*qca808x_start*/
#include <linux/phy.h>
#include <linux/mdio.h>
/*qca808x_end*/
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <net/dsa.h>

#if defined(IN_SWCONFIG)
#include <linux/switch.h>
#endif
/*qca808x_start*/
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_net.h>
#include <linux/of_address.h>
#include <linux/reset.h>
#include "ssdk_plat.h"
/*qca808x_end*/
#include "ssdk_clk.h"
#include "ssdk_led.h"
#include "ref_vlan.h"
#include "ref_fdb.h"
#include "ref_mib.h"
#include "ref_port_ctrl.h"
#include "ref_misc.h"
#include "ref_uci.h"
#include "ref_vsi.h"
#include "shell.h"
#include "adpt.h"
#include "ssdk_hppe.h"
#include "adpt_hppe.h"
#include "ssdk_appe.h"
#ifdef IN_NETLINK
#include "ssdk_netlink.h"
#endif

#if defined(MHT)
#include "ssdk_mht.h"
#endif
#ifdef IN_LED
#include "ssdk_led.h"
#endif
#include "ssdk_plat.h"
struct notifier_block ssdk_dev_notifier;
#ifdef IN_SFP_PHY
#include "sfp_phy.h"
#endif
#if defined(HTTPPE)
#include "ssdk_httppe.h"
#endif

extern void qca_ar8327_sw_mib_task(struct qca_phy_priv *priv);

//#define PSGMII_DEBUG

#define QCA_QM_WORK_DELAY	100
#define QCA_QM_ITEM_NUMBER 41
#define QCA_MAC_SW_SYNC_WORK_DELAY	1000
#define QCA_FDB_SW_SYNC_WORK_DELAY	1000
/*qca808x_start*/
struct qca_phy_priv **qca_phy_priv_global;

static a_uint32_t qpool_switch_ports[2] = {SSDK_PHYSICAL_PORT6, SSDK_PHYSICAL_PORT6};
module_param_array(qpool_switch_ports, uint, NULL, 0444);
MODULE_PARM_DESC(qpool_switch_ports, "Source and destination port IDs for queue pool switch (e.g. 6,2)");

void ssdk_qpool_switch_ports_get(a_uint32_t *src_port, a_uint32_t *dst_port)
{
	*src_port = qpool_switch_ports[0];
	*dst_port = qpool_switch_ports[1];
}

struct qca_phy_priv* ssdk_phy_priv_data_get(a_uint32_t dev_id)
{
	if (dev_id >= SW_MAX_NR_DEV || !qca_phy_priv_global)
		return NULL;

	return qca_phy_priv_global[dev_id];
}
/*qca808x_end*/

a_uint32_t
qca_ppe_port_mac_type_get(a_uint32_t dev_id, a_uint32_t port_id)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	if (!priv)
		return 0;

	if (priv->version == QCA_VER_HTTPPE) {
		if (priv->ports[port_id].port_mac_type != 0)
			return priv->ports[port_id].port_mac_type;
		else
			/* SSDK interface init and MAC polling is not avaiable.
			 * MAC type is selected by DSA PHYLINK and can be get
			 * from current port mux control configuration.
			 */
			return adpt_ppe_mac_type_get(dev_id, port_id);
	} else {
		if (port_id < SSDK_PHYSICAL_PORT1 || port_id >= SW_MAX_NR_PORT)
			return 0;
		return priv->ports[port_id].port_mac_type;
	}
}

sw_error_t
qca_ppe_port_mac_type_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t port_type)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	if (priv->version == QCA_VER_HTTPPE) {
		priv->ports[port_id].port_mac_type = port_type;
	} else {
		if (port_id < SSDK_PHYSICAL_PORT1 || port_id >= SW_MAX_NR_PORT)
			return SW_BAD_PARAM;
		priv->ports[port_id].port_mac_type = port_type;
	}

	return SW_OK;
}

a_uint32_t
ssdk_ifname_to_port(a_uint32_t dev_id, const char *ifname)
{
	struct net_device *eth_dev = NULL;
	eth_dev = dev_get_by_name(&init_net, ifname);
	if (!eth_dev || !eth_dev->phydev)
	{
		return 0;
	}
	dev_put(eth_dev);
	return qca_ssdk_phydev_to_port(dev_id, eth_dev->phydev);
}

char *
ssdk_port_to_ifname(a_uint32_t dev_id, a_uint32_t port_id)
{
	struct phy_device *phydev = NULL;
	phy_info_t *phyinfo = hsl_phy_info_get(dev_id);

	if (phyinfo->netdev[port_id])
		return phyinfo->netdev[port_id]->name;

	hsl_port_phydev_get(dev_id, port_id, &phydev);
	if (phydev && phydev->attached_dev)
	{
		return phydev->attached_dev->name;
	}
	else
	{
		return NULL;
	}
}

static void
ssdk_phy_rgmii_set(struct qca_phy_priv *priv)
{
	struct device_node *np = NULL;
	u32 rgmii_en = 0, tx_delay = 0, rx_delay = 0;

	if (priv->of_node)
		np = priv->of_node;
	if (!np)
		return;

	if (!of_property_read_u32(np, "phy_rgmii_en", &rgmii_en)) {
		/*enable RGMII  mode */
		hsl_phy_modify_debug(0, AR8327_PORT5_PHY_ADDR,
			AR8327_PHY_REG_MODE_SEL, AR8327_PHY_RGMII_MODE,
			AR8327_PHY_RGMII_MODE);
		if (!of_property_read_u32(np, "txclk_delay_en", &tx_delay)
				&& tx_delay == 1) {
			hsl_phy_modify_debug(0, AR8327_PORT5_PHY_ADDR,
				AR8327_PHY_REG_SYS_CTRL, AR8327_PHY_RGMII_TX_DELAY,
				AR8327_PHY_RGMII_TX_DELAY);
		}
		if (!of_property_read_u32(np, "rxclk_delay_en", &rx_delay)
				&& rx_delay == 1) {
			hsl_phy_modify_debug(0, AR8327_PORT5_PHY_ADDR,
				AR8327_PHY_REG_TEST_CTRL, AR8327_PHY_RGMII_RX_DELAY,
				AR8327_PHY_RGMII_RX_DELAY);
		}
	}
}

static void
qca_ar8327_phy_fixup(struct qca_phy_priv *priv, int phy)
{
	switch (priv->revision) {
	case 1:
		/* 100m waveform */
		hsl_phy_debug_reg_write(priv->device_id, phy, 0, 0x02ea);
		/* turn on giga clock */
		hsl_phy_debug_reg_write(priv->device_id, phy, 0x3d, 0x68a0);
		break;

	case 2:
		hsl_phy_mmd_reg_write(priv->device_id, phy, A_FALSE,
			0x7, 0x3c, 0);
#if defined(FALLTHROUGH)
		fallthrough;
#else
		/* fall through */
#endif
	case 4:
		if(priv->version == QCA_VER_AR8327)
		{
			hsl_phy_mmd_reg_write(priv->device_id, phy, A_FALSE,
				0x3, 0x800d, 0x803f);
			hsl_phy_debug_reg_write(priv->device_id, phy, 0x3d, 0x6860);
			hsl_phy_debug_reg_write(priv->device_id, phy, 0x5, 0x2c46);
			hsl_phy_debug_reg_write(priv->device_id, phy, 0x3c, 0x6000);
		}
		break;
	}
}

#ifdef IN_PORTVLAN
static void qca_port_isolate(a_uint32_t dev_id)
{
	a_uint32_t port_id, mem_port_id, mem_port_map[AR8327_NUM_PORTS]={0};

	for(port_id = 0; port_id < AR8327_NUM_PORTS; port_id++)
	{
		if(port_id == 6)
			for(mem_port_id = 1; mem_port_id<= 4; mem_port_id++)
				mem_port_map[port_id]  |= (1 << mem_port_id);
		else if (port_id == 0)
			mem_port_map[port_id]  |= (1 << 5);
		else if (port_id >= 1 && port_id <= 4)
			mem_port_map[port_id]  |= (1 << 6);
		else
			mem_port_map[port_id]  |= 1;
	}

	for(port_id = 0; port_id < AR8327_NUM_PORTS; port_id++)

		 fal_portvlan_member_update(dev_id, port_id, mem_port_map[port_id]);

}

void ssdk_portvlan_init(a_uint32_t dev_id)
{
	a_uint32_t port = 0;
	a_uint32_t cpu_bmp, lan_bmp, wan_bmp;

	cpu_bmp = ssdk_cpu_bmp_get(dev_id);
	lan_bmp = ssdk_lan_bmp_get(dev_id);
	wan_bmp = ssdk_wan_bmp_get(dev_id);

	if (!(cpu_bmp | lan_bmp | wan_bmp)) {
		qca_port_isolate(dev_id);
		return;
	}

	for(port = 0; port < SSDK_MAX_PORT_NUM; port++)
	{
		if(cpu_bmp & (1 << port))
		{
			fal_portvlan_member_update(dev_id, port, lan_bmp|wan_bmp);
		}
		if(lan_bmp & (1 << port))
		{
			fal_portvlan_member_update(dev_id, port, (lan_bmp|cpu_bmp)&(~(1<<port)));
		}
		if(wan_bmp & (1 << port))
		{
			fal_portvlan_member_update(dev_id, port, (wan_bmp|cpu_bmp)&(~(1<<port)));
		}
	}
}
#endif

void ssdk_port_eee_init(a_uint32_t dev_id, a_uint32_t port_id)
{
	struct phy_device *phydev = NULL;
	fal_port_eee_cfg_t port_eee_cfg = {0};

	if (A_FALSE == hsl_port_phy_connected(dev_id, port_id))
		return;

	hsl_port_phydev_get(dev_id, port_id, &phydev);
	if (phydev && phydev->eee_enabled) {
		port_eee_cfg.enable = A_TRUE;
		port_eee_cfg.advertisement = FAL_PHY_EEE_ALL_ADV;
		port_eee_cfg.lpi_tx_enable = A_TRUE;
		fal_port_interface_eee_cfg_set(dev_id, port_id, &port_eee_cfg);
	}
}

sw_error_t
qca_switch_init(a_uint32_t dev_id)
{
#if (defined(DESS) || defined(ISISC)) && defined(IN_QOS)
	a_uint32_t nr = 0, react_nr = 0;
#endif
#if defined(MHT)
	a_uint32_t port_hol_ctrl[2] = {0}, queue_hol_ctrl[6] = {0};
	a_uint32_t j = 0, cpu_bmp = 0;
#endif
	int i = 0;
	a_uint32_t port_bmp = 0;
	hsl_reg_mode reg_mode = HSL_REG_MDIO;
	ssdk_chip_type chip_type = hsl_get_current_chip_type(dev_id);

	/*fal_reset(dev_id);*/
	/*enable cpu and disable mirror*/
#ifdef ISISC
#ifdef IN_MISC
	fal_cpu_port_status_set(dev_id, A_TRUE);
	/* setup MTU */
	fal_frame_max_size_set(dev_id, 9216);
#endif
#ifdef IN_IGMP
	fal_igmp_mld_rp_set(dev_id, 0);
#endif
#endif
#ifdef IN_MIB
	/* Enable MIB counters */
	fal_mib_status_set(dev_id, A_TRUE);
	fal_mib_cpukeep_set(dev_id, A_FALSE);
#endif
	/*set mirror analysis port as 0xf in default*/
#ifdef IN_MIRROR
	fal_mirr_analysis_port_set(dev_id, 0xf);
#endif
	reg_mode = ssdk_switch_reg_access_mode_get(dev_id);

	if (reg_mode == HSL_REG_MDIO && ssdk_dts_node_get(dev_id) == NULL) {
		/* For legacy S17C without defining ess-switch device node
		 * and port bmp in DTS.
		 */
		port_bmp = 0x7f;
	} else {
		port_bmp = qca_ssdk_port_bmp_get(dev_id);
		/* Including CPU port */
		port_bmp |= ssdk_cpu_bmp_get(dev_id);
	}

	i = 0;
	while (port_bmp) {
		if (port_bmp & 1) {
#if defined(ISISC)
#ifdef IN_MISC
        		/* forward multicast and broadcast frames to CPU */
			fal_port_unk_uc_filter_set(dev_id, i, A_FALSE);
			fal_port_unk_mc_filter_set(dev_id, i, A_FALSE);
			fal_port_bc_filter_set(dev_id, i, A_FALSE);
#endif
#ifdef IN_PORTVLAN
			fal_port_default_svid_set(dev_id, i, 0);
			fal_port_default_cvid_set(dev_id, i, 0);
			fal_port_1qmode_set(dev_id, i, FAL_1Q_DISABLE);
			fal_port_egvlanmode_set(dev_id, i, FAL_EG_UNTOUCHED);
			fal_port_vlan_propagation_set(dev_id, i, FAL_VLAN_PROPAGATION_REPLACE);
#endif
#ifdef IN_IGMP
			fal_port_igmps_status_set(dev_id, i, A_FALSE);
			fal_port_igmp_mld_join_set(dev_id, i, A_FALSE);
			fal_port_igmp_mld_leave_set(dev_id, i, A_FALSE);
			fal_igmp_mld_entry_creat_set(dev_id, A_FALSE);
			fal_igmp_mld_entry_v3_set(dev_id, A_FALSE);
#endif
#endif
#ifdef IN_FDB
			fal_fdb_port_learn_set(dev_id, i, A_TRUE);
#endif
#ifdef IN_STP
			fal_stp_port_state_set(dev_id, 0, i, FAL_STP_FARWARDING);
#endif

			switch (chip_type) {
				case CHIP_ISISC:
				case CHIP_ISIS:
#if defined(ISISC)
#ifdef IN_PORTCONTROL
					ssdk_port_eee_init(dev_id, i);
					fal_port_flowctrl_forcemode_set(dev_id, i, A_TRUE);
					fal_port_flowctrl_set(dev_id, i, A_FALSE);

					if (i != 0 && i != 6) {
						fal_port_flowctrl_forcemode_set(dev_id, i,
								A_FALSE);
					}
#endif
					if (i == 0 || i == 5 || i == 6) {
#ifdef IN_QOS
						nr = 240; /*30*8*/
						fal_qos_port_tx_buf_nr_set(dev_id, i, &nr);
						fal_qos_port_rx_buf_nr_get(dev_id, i, &nr,
							&react_nr);
						nr = 48; /*6*8*/
						fal_qos_port_rx_buf_nr_set(dev_id, i, &nr,
							&react_nr);
						fal_qos_port_red_en_set(dev_id, i, A_TRUE);
						if (chip_type == CHIP_ISISC) {
							nr = 64; /*8*8*/
						} else if (chip_type == CHIP_ISIS) {
							nr = 60;
						}

						fal_qos_queue_tx_buf_nr_set(dev_id, i, 5, &nr);
						nr = 48; /*6*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 4, &nr);
						nr = 32; /*4*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 3, &nr);
						nr = 32; /*4*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 2, &nr);
						nr = 32; /*4*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 1, &nr);
						nr = 24; /*3*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 0, &nr);
#endif
					} else {
#ifdef IN_QOS
						nr = 200; /*25*8*/
						fal_qos_port_tx_buf_nr_set(dev_id, i, &nr);
						fal_qos_port_rx_buf_nr_get(dev_id, i, &nr,
							&react_nr);
						nr = 48; /*6*8*/
						fal_qos_port_rx_buf_nr_set(dev_id, i, &nr,
							&react_nr);
						fal_qos_port_red_en_set(dev_id, i, A_TRUE);
						if (chip_type == CHIP_ISISC) {
							nr = 64; /*8*8*/
						} else if (chip_type == CHIP_ISIS) {
							nr = 60;
						}
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 3, &nr);
						nr = 48; /*6*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 2, &nr);
						nr = 32; /*4*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 1, &nr);
						nr = 24; /*3*8*/
						fal_qos_queue_tx_buf_nr_set(dev_id, i, 0, &nr);
#endif
					}
#endif
					break;
				case CHIP_MHT:
#if defined(MHT)
					aos_mem_zero(&port_hol_ctrl, sizeof(port_hol_ctrl));
					aos_mem_zero(&queue_hol_ctrl, sizeof(queue_hol_ctrl));
					cpu_bmp = ssdk_cpu_bmp_get(dev_id);
#if defined(IN_PORTCONTROL)
					ssdk_port_eee_init(dev_id, i);
					fal_port_txmac_status_set(dev_id, i, A_FALSE);
					fal_port_rxmac_status_set(dev_id, i, A_FALSE);
					fal_port_flowctrl_set(dev_id, i, A_TRUE);
#endif
					if (cpu_bmp & BIT(i)) {
#if defined(IN_PORTCONTROL)
						fal_port_flowctrl_forcemode_set(dev_id, i, A_TRUE);
						fal_port_flow_ctrl_thres_set(dev_id, i,
								MHT_PORT0_XON_THRES, MHT_PORT0_XOFF_THRES);
						fal_header_type_set(dev_id,
								A_TRUE, MHT_HEADER_TYPE_VAL);
						fal_port_rxhdr_mode_set(dev_id,
								i, FAL_ONLY_MANAGE_FRAME_EN);
						fal_port_txhdr_mode_set(dev_id,
								i, FAL_NO_HEADER_EN);
#endif
						/* port tx buf number */
						port_hol_ctrl[0] = 600;
						/* port rx buf number */
						port_hol_ctrl[1] = 48;
						/* queue0 tx buf number */
						queue_hol_ctrl[0] = 24;
						/* queue1 tx buf number */
						queue_hol_ctrl[1] = 32;
						/* queue2 tx buf number */
						queue_hol_ctrl[2] = 32;
						/* queue3 tx buf number */
						queue_hol_ctrl[3] = 32;
						/* queue4 tx buf number */
						queue_hol_ctrl[4] = 48;
						/* queue5 tx buf number */
						queue_hol_ctrl[5] = 64;
					} else {
#if defined(IN_PORTCONTROL)
						fal_port_flowctrl_forcemode_set(dev_id, i,
								A_FALSE);
#endif
						/* port tx buf number */
						port_hol_ctrl[0] = 500;
						/* port rx buf number */
						port_hol_ctrl[1] = 48;
						/* queue0 tx buf number */
						queue_hol_ctrl[0] = 24;
						/* queue1 tx buf number */
						queue_hol_ctrl[1] = 32;
						/* queue2 tx buf number */
						queue_hol_ctrl[2] = 48;
						/* queue3 tx buf number */
						queue_hol_ctrl[3] = 64;
					}
#if defined(IN_QOS)
					fal_qos_port_red_en_set(dev_id, i, A_TRUE);
					fal_qos_port_tx_buf_nr_set(dev_id, i, &port_hol_ctrl[0]);
					fal_qos_port_rx_buf_nr_get(dev_id, i, &nr, &react_nr);
					nr = port_hol_ctrl[1];
					react_nr = 60;
					fal_qos_port_rx_buf_nr_set(dev_id, i, &nr, &react_nr);

					for (j = 0; j < ARRAY_SIZE(queue_hol_ctrl)
							&& queue_hol_ctrl[j] != 0; j++)
						fal_qos_queue_tx_buf_nr_set(dev_id, i, j,
								&queue_hol_ctrl[j]);
#endif
#endif
					break;
				default:
					break;
			}
		}
		port_bmp >>=1;
		i++;
	}

	return SW_OK;
}

void qca_ar8327_phy_linkdown(a_uint32_t dev_id)
{
	int i;

	for (i = 0; i < AR8327_NUM_PHYS; i++) {
		hsl_phy_mii_reg_write(dev_id, i, 0x0, 0x0800);	// phy powerdown

		hsl_phy_modify_debug(dev_id, i, 0x3d, 0x0040, 0);
		/*PHY will stop the tx clock for a while when link is down
			1. en_anychange  debug port 0xb bit13 = 0  //speed up link down tx_clk
			2. sel_rst_80us  debug port 0xb bit10 = 0  //speed up speed mode change to 2'b10 tx_clk
		*/
		hsl_phy_modify_debug(dev_id, i, 0xb, 0x2400, 0);
	}
}

void
qca_mac_disable(a_uint32_t device_id)
{
	hsl_api_t *p_api;

	p_api = hsl_api_ptr_get (device_id);
	if(p_api
		&& p_api->interface_mac_pad_set
		&& p_api->interface_mac_sgmii_set)
	{
		p_api->interface_mac_pad_set(device_id,0,0);
		p_api->interface_mac_pad_set(device_id,5,0);
		p_api->interface_mac_pad_set(device_id,6,0);
		p_api->interface_mac_sgmii_set(device_id,AR8327_REG_PAD_SGMII_CTRL_HW_INIT);
	}
	else
	{
		SSDK_ERROR("API not support \n");
	}
}

static void qca_switch_set_mac_force(struct qca_phy_priv *priv)
{
	a_uint32_t value, value0, i;
	if (priv == NULL || (priv->mii_read == NULL) || (priv->mii_write == NULL)) {
		SSDK_ERROR("In qca_switch_set_mac_force, private data is NULL!\r\n");
		return;
	}

	for (i=0; i < AR8327_NUM_PORTS; ++i) {
		/* b3:2=0,Tx/Rx Mac disable,
		 b9=0,LINK_EN disable */
		value0 = priv->mii_read(priv->device_id, AR8327_REG_PORT_STATUS(i));
		value = value0 & ~(AR8327_PORT_STATUS_LINK_AUTO |
						AR8327_PORT_STATUS_TXMAC |
						AR8327_PORT_STATUS_RXMAC);
		priv->mii_write(priv->device_id, AR8327_REG_PORT_STATUS(i), value);

		/* Force speed to 1000M Full */
		value = priv->mii_read(priv->device_id, AR8327_REG_PORT_STATUS(i));
		value &= ~(AR8327_PORT_STATUS_DUPLEX | AR8327_PORT_STATUS_SPEED);
		value |= AR8327_PORT_SPEED_1000M | AR8327_PORT_STATUS_DUPLEX;
		priv->mii_write(priv->device_id, AR8327_REG_PORT_STATUS(i), value);
	}
	return;
}

void
qca_ar8327_phy_enable(struct qca_phy_priv *priv)
{
	int i = 0;

        ssdk_phy_rgmii_set(priv);
	for (i = 0; i < AR8327_NUM_PHYS; i++) {
		if (priv->version == QCA_VER_AR8327 || priv->version == QCA_VER_AR8337)
			qca_ar8327_phy_fixup(priv, i);

		/* start autoneg*/
		hsl_phy_mii_reg_write(priv->device_id, i, MII_ADVERTISE, ADVERTISE_ALL |
						     ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);
		//phy reg 0x9, b10,1 = Prefer multi-port device (master)
		hsl_phy_mii_reg_write(priv->device_id, i, MII_CTRL1000, (0x0400|ADVERTISE_1000FULL));

		hsl_phy_mii_reg_write(priv->device_id, i, MII_BMCR, BMCR_RESET | BMCR_ANENABLE);

		hsl_phy_modify_debug(priv->device_id, i, 0, BIT(12), 0);
		msleep(100);
	}
}
void qca_ar8327_sw_soft_reset(struct qca_phy_priv *priv)
{
	a_uint32_t value = 0;

	value = priv->mii_read(priv->device_id, AR8327_REG_CTRL);
	value |= 0x80000000;
	priv->mii_write(priv->device_id, AR8327_REG_CTRL, value);
	/*Need wait reset done*/
	do {
		udelay(10);
		value = priv->mii_read(priv->device_id, AR8327_REG_CTRL);
	} while(value & AR8327_CTRL_RESET);
	do {
		udelay(10);
		value = priv->mii_read(priv->device_id, 0x20);
	} while ((value & SSDK_GLOBAL_INITIALIZED_STATUS) !=
			SSDK_GLOBAL_INITIALIZED_STATUS);

	return;
}

int qca_ar8327_hw_init(struct qca_phy_priv *priv)
{
	struct device_node *np = NULL;
	const __be32 *paddr;
	a_uint32_t reg, value, i;
	a_int32_t len;

	if (priv->of_node)
		np = priv->of_node;
	if(!np)
		return -EINVAL;

	/*Before switch software reset, disable PHY and clear  MAC PAD*/
	qca_ar8327_phy_linkdown(priv->device_id);
	qca_mac_disable(priv->device_id);
	msleep(1000);

	/*First software reset S17 chip*/
	qca_ar8327_sw_soft_reset(priv);

	/*After switch software reset, need disable all ports' MAC with 1000M FULL*/
	qca_switch_set_mac_force(priv);

	/* Configure switch register from DT information */
	paddr = of_get_property(np, "qca,ar8327-initvals", &len);
	if (paddr) {
		if (len < (2 * sizeof(*paddr))) {
			SSDK_ERROR("len:%d < 2 * sizeof(*paddr):%zu\n", len, 2 * sizeof(*paddr));
			return -EINVAL;
		}

		len /= sizeof(*paddr);

		for (i = 0; i < len - 1; i += 2) {
			reg = be32_to_cpup(paddr + i);
			value = be32_to_cpup(paddr + i + 1);
			priv->mii_write(priv->device_id, reg, value);
		}
	}

	value = priv->mii_read(priv->device_id, AR8327_REG_MODULE_EN);
	value &= ~AR8327_REG_MODULE_EN_QM_ERR;
	value &= ~AR8327_REG_MODULE_EN_LOOKUP_ERR;
	priv->mii_write(priv->device_id, AR8327_REG_MODULE_EN, value);

	qca_switch_init(priv->device_id);
#ifdef IN_PORTVLAN
	ssdk_portvlan_init(priv->device_id);
#endif
	qca_switch_enable_intr(priv, FAL_SWITCH_INTR_LINK_STATUS);
	qca_ar8327_phy_enable(priv);

	return 0;
}

#if defined(IN_SWCONFIG)
static struct switch_attr qca_ar8327_globals[] = {
#if defined(IN_VLAN)
#ifdef ISISC
	{
		.name = "enable_vlan",
		.description = "Enable 8021q VLAN",
		.type = SWITCH_TYPE_INT,
		.set = qca_ar8327_sw_set_vlan,
		.get = qca_ar8327_sw_get_vlan,
		.max = 1
	},
#endif
#endif
#if defined(IN_MISC)
	{
		.name = "max_frame_size",
		.description = "Set Max frame Size Of Mac",
		.type = SWITCH_TYPE_INT,
		.set = qca_ar8327_sw_set_max_frame_size,
		.get = qca_ar8327_sw_get_max_frame_size,
		.max = 9018
	},
#endif
#if defined(IN_MIB)
	{
		.name = "reset_mibs",
		.description = "Reset All MIB Counters",
		.type = SWITCH_TYPE_NOVAL,
		.set = qca_ar8327_sw_set_reset_mibs,
	},
#endif
#ifdef IN_FDB
	{
		.name = "flush_arl",
		.description = "Flush All ARL table",
		.type = SWITCH_TYPE_NOVAL,
		.set = qca_ar8327_sw_atu_flush,
	},
	{
		.name = "dump_arl",
		.description = "Dump All ARL table",
		.type = SWITCH_TYPE_STRING,
		.get = qca_ar8327_sw_atu_dump,
	},
#endif
	{
		.name = "switch_ext",
		.description = "Switch extended configuration",
		.type = SWITCH_TYPE_EXT,
		.set = qca_ar8327_sw_switch_ext,
	},
};

static struct switch_attr qca_ar8327_port[] = {
#if defined(IN_MIB)
	{
		.name = "reset_mib",
		.description = "Reset Mib Counters",
		.type = SWITCH_TYPE_NOVAL,
		.set = qca_ar8327_sw_set_port_reset_mib,
	},
	{
		.name = "mib",
		.description = "Get Mib Counters",
		.type = SWITCH_TYPE_STRING,
		.set = NULL,
		.get = qca_ar8327_sw_get_port_mib,
	},
#endif
#if defined(IN_PORTCONTROL)
	{
		.type = SWITCH_TYPE_INT,
		.name = "enable_eee",
		.description = "Enable EEE",
		.set = qca_ar8327_sw_set_eee,
		.get = qca_ar8327_sw_get_eee,
		.max = 1,
	},
#endif
};

#if defined(IN_VLAN)
#ifdef ISISC
static struct switch_attr qca_ar8327_vlan[] = {
	{
		.name = "vid",
		.description = "Configure Vlan Id",
		.type = SWITCH_TYPE_INT,
		.set = qca_ar8327_sw_set_vid,
		.get = qca_ar8327_sw_get_vid,
		.max = 4094,
	},
};
#endif
#endif

const struct switch_dev_ops qca_ar8327_sw_ops = {
	.attr_global = {
		.attr = qca_ar8327_globals,
		.n_attr = ARRAY_SIZE(qca_ar8327_globals),
	},
	.attr_port = {
		.attr = qca_ar8327_port,
		.n_attr = ARRAY_SIZE(qca_ar8327_port),
	},
#if defined(IN_VLAN)
#ifdef ISISC
	.attr_vlan = {
		.attr = qca_ar8327_vlan,
		.n_attr = ARRAY_SIZE(qca_ar8327_vlan),
	},
	.get_port_pvid = qca_ar8327_sw_get_pvid,
	.set_port_pvid = qca_ar8327_sw_set_pvid,
	.get_vlan_ports = qca_ar8327_sw_get_ports,
	.set_vlan_ports = qca_ar8327_sw_set_ports,
	.apply_config = qca_ar8327_sw_hw_apply,
#endif
#endif
#if defined(IN_MISC)
	.reset_switch = qca_ar8327_sw_reset_switch,
#endif
#if defined(IN_PORTCONTROL)
	.get_port_link = qca_ar8327_sw_get_port_link,
#endif
};

/* standalone swconfig for htt to avoid impact to legacy devices */
#if defined(HTTPPE) && IS_ENABLED(CONFIG_NET_DSA)
static struct switch_attr qce_2204_globals[] = {
#if defined(IN_MISC)
	{
		.name = "max_frame_size",
		.description = "Set Max frame Size Of Mac",
		.type = SWITCH_TYPE_INT,
		.set = qca_ar8327_sw_set_max_frame_size,
		.get = qca_ar8327_sw_get_max_frame_size,
		.max = 9018
	},
#endif
#ifdef IN_FDB
	{
		.name = "dump_arl",
		.description = "Dump All ARL table",
		.type = SWITCH_TYPE_STRING,
		.get = qca_ar8327_sw_atu_dump,
	},
#endif
	{
		.name = "switch_ext",
		.description = "Switch extended configuration",
		.type = SWITCH_TYPE_EXT,
		.set = qca_ar8327_sw_switch_ext,
	},
};

static struct switch_attr qce_2204_port[] = {
#if defined(IN_PORTCONTROL)
	{
		.type = SWITCH_TYPE_INT,
		.name = "enable_eee",
		.description = "Enable EEE",
		.set = qca_ar8327_sw_set_eee,
		.get = qca_ar8327_sw_get_eee,
		.max = 1,
	},
#endif
};

const struct switch_dev_ops qce_2204_sw_ops = {
	.attr_global = {
		.attr = qce_2204_globals,
		.n_attr = ARRAY_SIZE(qce_2204_globals),
	},
	.attr_port = {
		.attr = qce_2204_port,
		.n_attr = ARRAY_SIZE(qce_2204_port),
	},
#if defined(IN_PORTCONTROL)
	.get_port_link = qca_ar8327_sw_get_port_link,
#endif
};
#endif
#endif

#define SSDK_MIB_CHANGE_WQ

static int
qca_phy_mib_task(struct qca_phy_priv *priv)
{
	qca_ar8327_sw_mib_task(priv);
	return 0;
}

static void
qca_phy_mib_work_task(struct work_struct *work)
{
	struct qca_phy_priv *priv = container_of(work, struct qca_phy_priv,
                                            mib_dwork.work);

	mutex_lock(&priv->mib_lock);

    qca_phy_mib_task(priv);

	mutex_unlock(&priv->mib_lock);
#ifndef SSDK_MIB_CHANGE_WQ
	schedule_delayed_work(&priv->mib_dwork,
			      msecs_to_jiffies(QCA_PHY_MIB_WORK_DELAY));
#else
	queue_delayed_work_on(0, system_long_wq, &priv->mib_dwork,
					msecs_to_jiffies(QCA_PHY_MIB_WORK_DELAY));
#endif
}

int
qca_phy_mib_work_start(struct qca_phy_priv *priv)
{
	mutex_init(&priv->mib_lock);
	if(SW_OK != fal_mib_counter_alloc(priv->device_id, &priv->mib_counters)){
		SSDK_ERROR("Memory allocation fail\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&priv->mib_dwork, qca_phy_mib_work_task);
#ifndef SSDK_MIB_CHANGE_WQ
	schedule_delayed_work(&priv->mib_dwork,
			               msecs_to_jiffies(QCA_PHY_MIB_WORK_DELAY));
#else
	queue_delayed_work_on(0, system_long_wq, &priv->mib_dwork,
					msecs_to_jiffies(QCA_PHY_MIB_WORK_DELAY));
#endif

	return 0;
}

void
qca_phy_mib_work_stop(struct qca_phy_priv *priv)
{
	if(!priv)
		return;
	if(priv->mib_counters)
		kfree(priv->mib_counters);
	cancel_delayed_work_sync(&priv->mib_dwork);
}

void
qca_phy_mib_work_pause(struct qca_phy_priv *priv)
{
	if(!priv)
		return;
	cancel_delayed_work_sync(&priv->mib_dwork);
}

int
qca_phy_mib_work_resume(struct qca_phy_priv *priv)
{
#ifndef SSDK_MIB_CHANGE_WQ
	schedule_delayed_work(&priv->mib_dwork,
			               msecs_to_jiffies(QCA_PHY_MIB_WORK_DELAY));
#else
	queue_delayed_work_on(0, system_long_wq, &priv->mib_dwork,
					msecs_to_jiffies(QCA_PHY_MIB_WORK_DELAY));
#endif
	return 0;
}

#define SSDK_QM_CHANGE_WQ

static void
qm_err_check_work_task_polling(struct work_struct *work)
{
	struct qca_phy_priv *priv = container_of(work, struct qca_phy_priv,
                                            qm_dwork_polling.work);
	qca_link_change_task(priv);

	if (priv->polling_once == A_FALSE) {
#ifndef SSDK_QM_CHANGE_WQ
		schedule_delayed_work(&priv->qm_dwork,
								msecs_to_jiffies(QCA_QM_WORK_DELAY));
#else
		queue_delayed_work_on(0, system_long_wq, &priv->qm_dwork_polling,
								msecs_to_jiffies(QCA_QM_WORK_DELAY));
#endif
	}
}

static sw_error_t
_qm_err_work_chip_check(struct qca_phy_priv *priv)
{
	sw_error_t rv = SW_OK;

	switch (priv->version) {
	case QCA_VER_AR8337:
	case QCA_VER_AR8327:
	case QCA_VER_DESS:
	case QCA_VER_MHT:
		break;
	default:
		SSDK_DEBUG("Unsupported chip version %d\n", priv->version);
		rv = SW_NOT_SUPPORTED;
	}

	return rv;
}

void qm_err_check_work_init(struct qca_phy_priv *priv)
{
	if (_qm_err_work_chip_check(priv) != SW_OK)
		return;

	mutex_init(&priv->qm_lock);
	INIT_DELAYED_WORK(&priv->qm_dwork_polling, qm_err_check_work_task_polling);
}

void qm_err_check_work_start(struct qca_phy_priv *priv)
{
	if (_qm_err_work_chip_check(priv) != SW_OK)
		return;

	priv->polling_once = A_FALSE;
#ifndef SSDK_MIB_CHANGE_WQ
	schedule_delayed_work(&priv->qm_dwork_polling,
							msecs_to_jiffies(QCA_QM_WORK_DELAY));
#else
	queue_delayed_work_on(0, system_long_wq, &priv->qm_dwork_polling,
							msecs_to_jiffies(QCA_QM_WORK_DELAY));
#endif
}

void qm_err_check_work_stop(struct qca_phy_priv *priv)
{
	if (_qm_err_work_chip_check(priv) != SW_OK)
		return;

	cancel_delayed_work_sync(&priv->qm_dwork_polling);
}

void
qca_mac_port_status_init(a_uint32_t dev_id, a_uint32_t port_id)
{
	if(port_id < SSDK_PHYSICAL_PORT1 || port_id >= SW_MAX_NR_PORT)
	{
		SSDK_ERROR("port %d does not support status init\n", port_id);
		return;
	}
	qca_phy_priv_global[dev_id]->ports[port_id].port_old_link = 0;
	qca_phy_priv_global[dev_id]->ports[port_id].port_old_speed = FAL_SPEED_BUTT;
	qca_phy_priv_global[dev_id]->ports[port_id].port_old_duplex = FAL_DUPLEX_BUTT;
	if(hsl_port_feature_get(dev_id, port_id, PHY_F_FORCE) || hsl_port_feature_get
		(dev_id, port_id, PHY_F_SFP))
	{
		qca_phy_priv_global[dev_id]->ports[port_id].port_tx_flowctrl_forcemode = A_TRUE;
		qca_phy_priv_global[dev_id]->ports[port_id].port_rx_flowctrl_forcemode = A_TRUE;
	}

	return;
}

void
qca_mac_sw_sync_work_task(struct work_struct *work)
{
	adpt_api_t *p_adpt_api;

	struct qca_phy_priv *priv = container_of(work, struct qca_phy_priv,
					mac_sw_sync_dwork.work);

	mutex_lock(&priv->mac_sw_sync_lock);

	 if((p_adpt_api = adpt_api_ptr_get(priv->device_id)) != NULL) {
		if (NULL == p_adpt_api->adpt_port_polling_sw_sync_set)
			return;
		p_adpt_api->adpt_port_polling_sw_sync_set(priv);
	}

	mutex_unlock(&priv->mac_sw_sync_lock);

	schedule_delayed_work(&priv->mac_sw_sync_dwork,
					msecs_to_jiffies(QCA_MAC_SW_SYNC_WORK_DELAY));
}

int
qca_mac_sw_sync_work_init(struct qca_phy_priv *priv)
{
	mutex_init(&priv->mac_sw_sync_lock);

	INIT_DELAYED_WORK(&priv->mac_sw_sync_dwork,
			qca_mac_sw_sync_work_task);

	return 0;
}

static sw_error_t
_ssdk_mac_sw_sync_chip_check(struct qca_phy_priv *priv)
{
	sw_error_t rv = SW_OK;

	switch (priv->version) {
		case QCA_VER_HPPE:
		case QCA_VER_SCOMPHY:
		case QCA_VER_MRPPE:
		case QCA_VER_APPE:
		case QCA_VER_JHPPE:
		case QCA_VER_HMSPPE:
			break;
		default:
			SSDK_DEBUG("Unsupported chip version %d\n", priv->version);
			rv = SW_NOT_SUPPORTED;
	}

	return rv;
}

sw_error_t
ssdk_mac_sw_sync_work_stop(a_uint32_t dev_id)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	rv = _ssdk_mac_sw_sync_chip_check(priv);
	SW_RTN_ON_ERROR(rv);

	cancel_delayed_work_sync(&priv->mac_sw_sync_dwork);
	priv->ssdk_module_cnt.polling_stop_cnt++;

	return rv;
}
EXPORT_SYMBOL(ssdk_mac_sw_sync_work_stop);

sw_error_t
ssdk_mac_sw_sync_work_start(a_uint32_t dev_id)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	rv = _ssdk_mac_sw_sync_chip_check(priv);
	SW_RTN_ON_ERROR(rv);

	schedule_delayed_work(&priv->mac_sw_sync_dwork,
			msecs_to_jiffies(QCA_MAC_SW_SYNC_WORK_DELAY));
	priv->ssdk_module_cnt.polling_start_cnt++;
	return rv;
}
EXPORT_SYMBOL(ssdk_mac_sw_sync_work_start);

static const phy_interface_t mac_interfaces[] = {
	PHY_INTERFACE_MODE_SGMII,
	PHY_INTERFACE_MODE_QSGMII,
	PHY_INTERFACE_MODE_PSGMII,
	PHY_INTERFACE_MODE_1000BASEX,
	PHY_INTERFACE_MODE_2500BASEX,
	PHY_INTERFACE_MODE_USXGMII,
	PHY_INTERFACE_MODE_10GBASER,
	PHY_INTERFACE_MODE_QUSGMII,
	PHY_INTERFACE_MODE_25GBASER,
};

static void ssdk_phylink_mac_config(struct phylink_config *config,
				unsigned int mode,
				const struct phylink_link_state *state)
{
	struct ssdk_port_priv *port_priv = container_of(config, struct ssdk_port_priv,
							 phylink_config);

	SSDK_DEBUG("PPE port %d mac config interface %s\n",
			port_priv->port_id, phy_modes(state->interface));
}

static void ssdk_phylink_mac_link_up(struct phylink_config *config,
				 struct phy_device *phy,
				 unsigned int mode,
				 phy_interface_t interface,
				 int speed, int duplex,
				 bool tx_pause, bool rx_pause)
{
	struct ssdk_port_priv *port_priv = container_of(config, struct ssdk_port_priv,
							 phylink_config);
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(port_priv->dev_id);

	SSDK_DEBUG("PPE port %d mac link up: interface %s speed %d, duplex %d, tx_pause %d, "
			"rx_pause %d\n", port_priv->port_id, phy_modes(interface), speed,
			duplex, tx_pause, rx_pause);

	if (!priv || !hsl_port_feature_get(port_priv->dev_id,
					   port_priv->port_id, PHY_F_FORCE))
		return;

	mutex_lock(&priv->mac_sw_sync_lock);
	WRITE_ONCE(port_priv->port_phylink_up, A_TRUE);
	mutex_unlock(&priv->mac_sw_sync_lock);

	if (priv->link_polling_required &&
	    _ssdk_mac_sw_sync_chip_check(priv) == SW_OK)
		mod_delayed_work(system_wq, &priv->mac_sw_sync_dwork, 0);
}

static void ssdk_phylink_mac_link_down(struct phylink_config *config,
				   unsigned int mode,
				   phy_interface_t interface)
{
	struct ssdk_port_priv *port_priv = container_of(config, struct ssdk_port_priv,
							 phylink_config);
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(port_priv->dev_id);

	SSDK_DEBUG("PPE port %d mac link down: interface %s\n",
			port_priv->port_id, phy_modes(interface));

	if (!priv || !hsl_port_feature_get(port_priv->dev_id,
					   port_priv->port_id, PHY_F_FORCE))
		return;

	mutex_lock(&priv->mac_sw_sync_lock);
	WRITE_ONCE(port_priv->port_phylink_up, A_FALSE);
	mutex_unlock(&priv->mac_sw_sync_lock);

	if (priv->link_polling_required &&
	    _ssdk_mac_sw_sync_chip_check(priv) == SW_OK)
		mod_delayed_work(system_wq, &priv->mac_sw_sync_dwork, 0);
}

static struct phylink_pcs *ssdk_phylink_mac_select_pcs(struct phylink_config *config,
	phy_interface_t interface)
{
	struct ssdk_port_priv *port_priv = container_of(config, struct ssdk_port_priv,
		phylink_config);

	return &(port_priv->phylink_pcs);
}

static const struct phylink_mac_ops ssdk_phylink_ops = {
	.mac_config = ssdk_phylink_mac_config,
	.mac_link_up = ssdk_phylink_mac_link_up,
	.mac_link_down = ssdk_phylink_mac_link_down,
	.mac_select_pcs = ssdk_phylink_mac_select_pcs,
};

static int ssdk_pcs_config(struct phylink_pcs *pcs, unsigned int mode,
	phy_interface_t interface, const unsigned long *advertising, bool permitted)
{
	/* Save negotiated interface; actual PCS register configuration done in SSDK polling task */
	struct qca_phy_priv *priv = NULL;
	struct ssdk_port_priv *port_priv = container_of(pcs, struct ssdk_port_priv,
		phylink_pcs);

	if (port_priv->port_id >= SW_MAX_NR_PORT)
		return 0;
	priv = ssdk_phy_priv_data_get(port_priv->dev_id);
	if (!priv)
		return 0;
	mutex_lock(&priv->mac_sw_sync_lock);
	/* update phylink_link_state for SSDK polling */
	port_priv->phylink_link_state.interface = interface;
	mutex_unlock(&priv->mac_sw_sync_lock);

	SSDK_DEBUG("port id:%d, phylink_link_state interface:%d\n",
		port_priv->port_id, port_priv->phylink_link_state.interface);

	return 0;
}

static void ssdk_pcs_an_restart(struct phylink_pcs *pcs)
{
	/* pcs no need to restart autoneg here as pcs is configured in SSDK polling task */
}

static void ssdk_pcs_get_state(struct phylink_pcs *pcs, struct phylink_link_state *state)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = NULL;
	struct port_phy_status phy_status = {0};
	adpt_api_t *p_api;
	struct ssdk_port_priv *port_priv = container_of(pcs, struct ssdk_port_priv,
		phylink_pcs);
	int retries = 10;  /* Maximum 10 retries to wait for PCS state synchronization */

	if (!pcs || !state)
		return;

	if (port_priv->port_id >= SW_MAX_NR_PORT)
		return;
	priv = ssdk_phy_priv_data_get(port_priv->dev_id);
	if (!priv)
		return;
	p_api = adpt_api_ptr_get(port_priv->dev_id);
	if (!p_api || !p_api->adpt_port_phy_status_get)
		return;

	mutex_lock(&priv->mac_sw_sync_lock);
	/*
	 * This API is called when link state changes
	 * - If current link is UP, try to get link down state directly
	 * - If current link is DOWN, need to loop retry to wait for link up state
	 */
	if (port_priv->phylink_link_state.link) {
		/* Current link is UP, read status directly (detect if it becomes DOWN) */
		rv = p_api->adpt_port_phy_status_get(port_priv->dev_id,
			port_priv->port_id, &phy_status);
		if (rv != SW_OK) {
			mutex_unlock(&priv->mac_sw_sync_lock);
			return;
		}
	} else {
		/*
		 * Current link is DOWN, need to loop wait for link up
		 *
		 * [IMPORTANT] For 25G SFP modules, this loop mechanism is MANDATORY:
		 * the kernel driver sfp.c use interrupt mechanism to notify link changes. When
		 * the interrupt is triggered, the PCS link state may not have fully
		 * synchronized yet. Therefore, loop retry (100ms interval each time) is
		 * needed to wait for PCS state synchronization, otherwise link up state
		 * may not be read.
		 *
		 * Retry up to 10 times (total ~1 second). Exit loop immediately if
		 * link up is detected during this period.
		 */
		do {
			mutex_unlock(&priv->mac_sw_sync_lock);
			msleep(100);  /* 100ms interval per retry, wait for PCS state sync */
			mutex_lock(&priv->mac_sw_sync_lock);
			rv = p_api->adpt_port_phy_status_get(port_priv->dev_id,
				port_priv->port_id, &phy_status);
			if (rv != SW_OK) {
				mutex_unlock(&priv->mac_sw_sync_lock);
				return;
			}
			if (phy_status.link_status == PORT_LINK_UP)
				break;  /* Link up detected, exit loop immediately */
		} while (--retries);
	}

	/* Update returned link state information */
	state->speed = phy_status.speed;
	state->duplex = phy_status.duplex;
	state->link = (bool)phy_status.link_status;

	/* Update internal phylink link state for SSDK polling task.
	 * Note: phylink_link_state.interface is intentionally NOT updated here;
	 * it is set once in ssdk_pcs_config() when the interface is negotiated
	 * and remains valid for the lifetime of the link.
	 */
	port_priv->phylink_link_state.speed = phy_status.speed;
	port_priv->phylink_link_state.duplex = phy_status.duplex;
	port_priv->phylink_link_state.link = (bool)phy_status.link_status;
	mutex_unlock(&priv->mac_sw_sync_lock);

	SSDK_DEBUG("port id:%d, link:%d, speed:%d, duplex:%d\n",
		port_priv->port_id, phy_status.link_status, phy_status.speed, phy_status.duplex);
}

static const struct phylink_pcs_ops ssdk_phylink_pcs_ops = {
	.pcs_get_state = ssdk_pcs_get_state,
	.pcs_config = ssdk_pcs_config,
	.pcs_an_restart = ssdk_pcs_an_restart,
};

#ifdef CONFIG_PHYLINK
struct phylink* ssdk_port_phylink_setup(a_uint32_t dev_id,
					a_uint32_t port_id, struct net_device *netdev)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	struct ssdk_port_priv *port_priv = NULL;
	struct device_node *sfp_np = NULL;
	struct device_node *fixed_link = NULL;
	struct phy_device *phydev = NULL;
	struct fwnode_handle *fwnode = NULL;
	bool is_i2c_phy = false, using_mdio_ahb_bus = false, is_mdio_ahb_phy = false;
	int i, ret;

	if (!priv || port_id >= SW_MAX_NR_PORT)
		return NULL;

	port_priv = &priv->ports[port_id];
	if (!port_priv->np) {
		SSDK_ERROR("No PPE port %d node found.\n", port_id);
		return NULL;
	}

	/* Port phylink capability */
	port_priv->phylink_config.dev = &netdev->dev;
	port_priv->phylink_config.type = PHYLINK_NETDEV;
	port_priv->phylink_config.mac_capabilities = MAC_ASYM_PAUSE |
		MAC_SYM_PAUSE | MAC_10 | MAC_100 | MAC_1000 |
		MAC_2500FD | MAC_5000FD | MAC_10000FD | MAC_25000FD;

	for (i = 0; i < ARRAY_SIZE(mac_interfaces); i++)
		__set_bit(mac_interfaces[i],
			  port_priv->phylink_config.supported_interfaces);

	/* PCS configuration */
	port_priv->phylink_pcs.ops = &ssdk_phylink_pcs_ops;
	port_priv->phylink_pcs.poll = true;
	port_priv->phylink_pcs.neg_mode = true;

	fwnode = of_fwnode_handle(port_priv->np);

	/* Check if this is i2c based qca81xx or mdio ahb phy*/
	if (hsl_port_phy_access_type_get(dev_id, port_id) == PHY_AHB_ACCESS)
		using_mdio_ahb_bus = A_TRUE;
	if (of_find_property(port_priv->np, "i2c-bus", NULL) || using_mdio_ahb_bus) {
		hsl_port_phydev_get(dev_id, port_id, &phydev);
		if (phydev && phydev->is_c45) {
			fwnode = NULL;
			if (phydev->c45_ids.device_ids[__ffs(phydev->c45_ids.mmds_present)]
			== QCA8111_PHY)
				is_i2c_phy = true;
			if (using_mdio_ahb_bus) {
				is_mdio_ahb_phy = true;
				mutex_lock(&phydev->lock);
				phydev->interface = port_priv->interface;
				mutex_unlock(&phydev->lock);
			}
		}
	}
	/* Create phylink */
	port_priv->phylink = phylink_create(&port_priv->phylink_config,
		fwnode, port_priv->interface, &ssdk_phylink_ops);
	if (IS_ERR(port_priv->phylink)) {
		SSDK_ERROR("PPE port %d failed to create phylink, ret %d\n",
			port_priv->port_id, PTR_ERR(port_priv->phylink));
		port_priv->phylink = NULL;
		return NULL;
	}

	/* for i2c based qca81xx and mdio ahb phy*/
	if (is_i2c_phy || is_mdio_ahb_phy) {
		ret = phylink_connect_phy(port_priv->phylink, phydev);
		if (ret) {
			SSDK_ERROR("PPE port %d failed to connect i2c PHY or ahb PHY, ret %d\n",
				port_id, ret);
			goto err_free_phylink;
		}
		return port_priv->phylink;
	}

	/* for SFP port */
	sfp_np = of_parse_phandle(port_priv->np, "sfp", 0);
	if (sfp_np) {
		of_node_put(sfp_np);
		ret = phylink_fwnode_phy_connect(port_priv->phylink, fwnode, 0);
		if (ret && ret != -ENODEV) {
			SSDK_ERROR("PPE port %d SFP fwnode connect failed, ret %d\n",
				port_id, ret);
			goto err_free_phylink;
		}
		return port_priv->phylink;
	}

	/* for fixed-link port */
	fixed_link = of_get_child_by_name(port_priv->np, "fixed-link");
	if (fixed_link) {
		of_node_put(fixed_link);
		return port_priv->phylink;
	}

	/* for normal PHY port */
	ret = phylink_of_phy_connect(port_priv->phylink, port_priv->np, 0);
	if (ret) {
		SSDK_ERROR("PPE port %d failed to connect PHY, ret %d\n",
			port_id, ret);
		goto err_free_phylink;
	}
	return port_priv->phylink;

err_free_phylink:
	/* Disconnect PHY if it was connected */
	if (port_priv->phylink) {
		if (!sfp_np && !fixed_link && !is_i2c_phy) {
			rtnl_lock();
			phylink_disconnect_phy(port_priv->phylink);
			rtnl_unlock();
		}
	}
	phylink_destroy(port_priv->phylink);
	port_priv->phylink = NULL;

	if (sfp_np)
		of_node_put(sfp_np);

	if (fixed_link)
		of_node_put(fixed_link);

	return NULL;
}
EXPORT_SYMBOL(ssdk_port_phylink_setup);

void ssdk_port_phylink_destroy(a_uint32_t dev_id, a_uint32_t port_id)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	struct ssdk_port_priv *port_priv;

	if (!priv || port_id >= SW_MAX_NR_PORT)
		return;

	port_priv = &priv->ports[port_id];

	if (port_priv->phylink) {
		rtnl_lock();
		if (hsl_port_phy_connected(dev_id, port_id))
			phylink_disconnect_phy(port_priv->phylink);
		rtnl_unlock();
		phylink_destroy(port_priv->phylink);
		port_priv->phylink = NULL;
	}
}
EXPORT_SYMBOL(ssdk_port_phylink_destroy);
#endif

void
qca_fdb_sw_sync_work_task(struct work_struct *work)
{
	struct qca_phy_priv *priv = container_of(work, struct qca_phy_priv,
		fdb_sw_sync_dwork.work);

#ifdef IN_FDB
	ref_fdb_sw_sync_task(priv);
#endif
	schedule_delayed_work(&priv->fdb_sw_sync_dwork,
		msecs_to_jiffies(QCA_FDB_SW_SYNC_WORK_DELAY));
}

int
qca_fdb_sw_sync_work_init(struct qca_phy_priv *priv)
{
	if ((priv->version != QCA_VER_HPPE) && (priv->version != QCA_VER_APPE) &&
		(priv->version != QCA_VER_MRPPE) && (priv->version != QCA_VER_MHT) &&
		(priv->version != QCA_VER_JHPPE) && (priv->version != QCA_VER_HMSPPE))
	{
		return 0;
	}

	aos_lock_init(&priv->fdb_sw_sync_lock);

	return 0;
}

/**
 * add fdb polling on port_map
 */
int
qca_fdb_sw_sync_work_start(struct qca_phy_priv *priv, fal_pbmp_t port_map)
{
	if ((priv->version != QCA_VER_HPPE) && (priv->version != QCA_VER_APPE) &&
		(priv->version != QCA_VER_MRPPE) && (priv->version != QCA_VER_MHT) &&
		(priv->version != QCA_VER_JHPPE) && (priv->version != QCA_VER_HMSPPE))
	{
		return 0;
	}

	aos_lock_bh(&priv->fdb_sw_sync_lock);
	SSDK_DEBUG("fdb_sw_sync_port_map 0x%x\n", priv->fdb_sw_sync_port_map);
	if (priv->fdb_sw_sync_port_map == 0 && priv->fdb_polling_started == A_FALSE)
	{
		INIT_DELAYED_WORK(&priv->fdb_sw_sync_dwork,
						qca_fdb_sw_sync_work_task);
		schedule_delayed_work(&priv->fdb_sw_sync_dwork,
						msecs_to_jiffies(QCA_FDB_SW_SYNC_WORK_DELAY));
		priv->fdb_polling_started = A_TRUE;
	}
	SW_PBMP_OR(priv->fdb_sw_sync_port_map, port_map);
	SSDK_DEBUG("fdb_sw_sync_port_map 0x%x\n", priv->fdb_sw_sync_port_map);
	aos_unlock_bh(&priv->fdb_sw_sync_lock);

	return 0;
}

/**
 * stop fdb polling on port_map
 */
void
qca_fdb_sw_sync_work_stop(struct qca_phy_priv *priv, fal_pbmp_t port_map)
{
	if ((priv->version != QCA_VER_HPPE) && (priv->version != QCA_VER_APPE) &&
		(priv->version != QCA_VER_MRPPE) &&
		(priv->version != QCA_VER_JHPPE) && (priv->version != QCA_VER_HMSPPE))
	{
		return;
	}

	aos_lock_bh(&priv->fdb_sw_sync_lock);
	SSDK_DEBUG("fdb_sw_sync_port_map 0x%x\n", priv->fdb_sw_sync_port_map);
	SW_PBMP_AND(priv->fdb_sw_sync_port_map, ~port_map);
	if (priv->fdb_sw_sync_port_map == 0 && priv->fdb_polling_started == A_TRUE)
	{
		aos_unlock_bh(&priv->fdb_sw_sync_lock);
		cancel_delayed_work_sync(&priv->fdb_sw_sync_dwork);
		priv->fdb_polling_started = A_FALSE;
	}
	else
		aos_unlock_bh(&priv->fdb_sw_sync_lock);
#ifdef IN_FDB
	ref_fdb_sw_sync_reset(priv, port_map);
#endif
	SSDK_DEBUG("fdb_sw_sync_port_map 0x%x\n", priv->fdb_sw_sync_port_map);
}

sw_error_t ssdk_ppe_hw_recover(a_uint32_t dev_id)
{
	adpt_ppe_type_t chip_type = adpt_ppe_type_get(dev_id);
	sw_error_t rv = SW_OK;

	switch (chip_type) {
	case APPE_TYPE:
	case MPPE_TYPE:
	case MRPPE_TYPE:
	case JHPPE_TYPE:
	case HMSPPE_TYPE:
		rv = qca_appe_hw_init(dev_id);
		SW_RTN_ON_ERROR(rv);

		SSDK_INFO("ssdk ppe hw recover successfully!\n");
		break;
	default:
		return SW_NOT_SUPPORTED;
	}

	return rv;
}
EXPORT_SYMBOL(ssdk_ppe_hw_recover);

#if defined(IN_SWCONFIG)
int qca_switchdev_register(struct qca_phy_priv *priv)
{
	struct switch_dev *sw_dev;
	int ret = SW_OK;
	sw_dev = &priv->sw_dev;

	switch (priv->version) {
		case QCA_VER_AR8227:
			sw_dev->name = "QCA AR8227";
			sw_dev->alias = "QCA AR8227";
			break;
		case QCA_VER_AR8327:
			sw_dev->name = "QCA AR8327";
			sw_dev->alias = "QCA AR8327";
			break;
		case QCA_VER_AR8337:
			sw_dev->name = "QCA AR8337";
			sw_dev->alias = "QCA AR8337";
			break;
		case QCA_VER_DESS:
			sw_dev->name = "QCA DESS";
			sw_dev->alias = "QCA DESS";
			break;
		case QCA_VER_APPE:
		case QCA_VER_MRPPE:
		case QCA_VER_JHPPE:
		case QCA_VER_HMSPPE:
		case QCA_VER_HPPE:
			sw_dev->name = "QCA "PPE_STR;
			sw_dev->alias = "QCA "PPE_STR;
			break;
		case QCA_VER_MHT:
			sw_dev->name = "QCA MHT";
			sw_dev->alias = "QCA MHT";
			break;
		case QCA_VER_HTTPPE:
			sw_dev->name = "QCA HTTPPE";
			sw_dev->alias = "QCA HTTPPE";
			break;
		default:
			sw_dev->name = "unknown switch";
			sw_dev->alias = "unknown switch";
			break;
	}

#if defined(HTTPPE) && IS_ENABLED(CONFIG_NET_DSA)
	if (adpt_ppe_type_get(priv->device_id) == HTTPPE_TYPE) {
		sw_dev->ops = &qce_2204_sw_ops;
	} else
#endif
	{
		sw_dev->ops = &qca_ar8327_sw_ops;
		sw_dev->vlans = AR8327_MAX_VLANS;
	}
	sw_dev->ports = priv->ports_num;

	ret = register_switch(sw_dev, NULL);
	if (ret != SW_OK) {
		SSDK_ERROR("register_switch failed for %s\n", sw_dev->name);
		return ret;
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_NET_DSA)
static struct qca_phy_priv* ssdk_get_priv_with_dsa_dev(struct net_device *dev)
{
	struct dsa_port *dp = NULL;
	ssdk_netdev_switch_t *netdev_switch = NULL;
	a_uint32_t dev_id = 0;

	if (!dsa_slave_dev_check(dev))
		return NULL;

	/* find dp,master by dsa helper and get net_switch */
	dp = dsa_port_from_netdev(dev);
	netdev_switch = ssdk_dts_netdev_switch_find_by_netdev(dp->cpu_dp->master);
	if (netdev_switch) {
		dev_id = netdev_switch->switch_dev_id;
	}

	return ssdk_phy_priv_data_get(dev_id);
}

static int ssdk_dsa_event_link_change(struct net_device *dev, bool link)
{
	struct qca_phy_priv* priv = ssdk_get_priv_with_dsa_dev(dev);
	if (!priv) {
		SSDK_ERROR("Priv handle could not be obtained by %s.\n", dev->name);
		return NOTIFY_DONE;
	}

	priv->polling_once = A_TRUE;
	mod_delayed_work(system_wq, &priv->qm_dwork_polling, 0);

	return NOTIFY_OK;
}

static void ssdk_dsa_intf_register(struct net_device *dev)
{
	struct qca_phy_priv *priv = ssdk_get_priv_with_dsa_dev(dev);
	if (!priv) {
		SSDK_ERROR("Priv handle could not be obtained by %s.\n", dev->name);
		return;
	}

	/* Initialization for QCE2204-DSA uninitialized modules */
	if (ssdk_httppe_init_with_dsa(priv)){
		SSDK_ERROR("HTTPPE Initializing for QCE DSA failed.\n");
		return;
	}
}

static int ssdk_dsa_event_nb(struct notifier_block *unused,
		unsigned long event, void *ptr)
{
	switch (event) {
		case DSA_NOTIFIER_PORT_LINK:
			struct dsa_notifier_link *info = (struct dsa_notifier_link *)ptr;

			SSDK_DEBUG("%s, link: %d.\n", info->info.dev->name, info->link);
			return ssdk_dsa_event_link_change(info->info.dev, info->link);

		default:
			SSDK_INFO("DSA event %lu is not supported\n", event);
	}

	return NOTIFY_DONE;
}

static struct notifier_block ssdk_dsa_notifier_nb = {
	.notifier_call = ssdk_dsa_event_nb,
};
#endif

static int ssdk_switch_register(a_uint32_t dev_id, ssdk_chip_type  chip_type)
{
	struct qca_phy_priv *priv;
	int ret = 0;
	a_uint32_t chip_id = 0;
	priv = qca_phy_priv_global[dev_id];

	priv->mii_read = qca_mii_read;
	priv->mii_write = qca_mii_write;

	if (fal_reg_get(dev_id, 0, (a_uint8_t *)&chip_id, 4) == SW_OK) {
		priv->version = ((chip_id >> 8) & 0xff);
		priv->revision = (chip_id & 0xff);
	}

	mutex_init(&priv->reg_mutex);

#if defined(IN_SWCONFIG)
	ret = qca_switchdev_register(priv);
	if (ret != SW_OK) {
		return ret;
	}
#endif
	priv->qca_ssdk_sw_dev_registered = A_TRUE;
	ret = qca_phy_mib_work_start(qca_phy_priv_global[dev_id]);
	if (ret != 0) {
			SSDK_ERROR("qca_phy_mib_work_start failed for chip 0x%02x%02x\n", priv->version, priv->revision);
			return ret;
	}
	qm_err_check_work_init(priv);
	if(priv->link_polling_required)
	{
		qm_err_check_work_start(priv);

		if (_ssdk_mac_sw_sync_chip_check(priv) != SW_OK) {
			return 0;
		}

		ret = qca_mac_sw_sync_work_init(priv);
		if (ret != 0) {
			SSDK_ERROR("qca_mac_sw_sync_work_init failed on chip 0x%02x%02x\n",
					priv->version, priv->revision);
			return ret;
		}
	}
	ret = qca_fdb_sw_sync_work_init(priv);
	if (ret != 0) {
		SSDK_ERROR("qca_fdb_sw_sync_work_init failed on chip 0x%02x%02x\n",
				priv->version, priv->revision);
		return ret;
	}
	if(priv->interrupt_no > 0)
	{
		snprintf(priv->intr_name, IFNAMSIZ, "switch%d", dev_id);
		switch(priv->version)
		{
#ifdef MHT
			case QCA_VER_MHT:
				priv->interrupt_flag = IRQF_TRIGGER_HIGH;
				break;
#endif
#ifdef ISISC
			case QCA_VER_AR8337:
				priv->interrupt_flag = IRQF_TRIGGER_LOW;
				break;
#endif
			default:
				priv->interrupt_flag = IRQF_TRIGGER_NONE;
				break;
		}
		SSDK_INFO("intr_number:%d, intr_name:%s, intr_flag:%d\n",
			priv->interrupt_no, priv->intr_name, priv->interrupt_flag);
		ret = qca_intr_init(priv);
		if(ret)
		{
			SSDK_ERROR("the interrupt init faild !\n");
			return ret;
		}
	}

#if IS_ENABLED(CONFIG_NET_DSA)
	if (priv->version == QCA_VER_MHT ||
		priv->version == QCA_VER_AR8337 ||
		priv->version == QCA_VER_AR8327)
		register_dsa_blocking_notifier(&ssdk_dsa_notifier_nb);
#endif

	return 0;
}

static int ssdk_switch_unregister(a_uint32_t dev_id)
{
	struct qca_phy_priv *priv = qca_phy_priv_global[dev_id];

	qca_phy_mib_work_stop(priv);

	if (priv->link_polling_required) {
		qm_err_check_work_stop(priv);
		ssdk_mac_sw_sync_work_stop(dev_id);
	}

	if (priv->interrupt_no > 0)
		qca_intr_deinit(priv);

#if defined(IN_SWCONFIG)
	unregister_switch(&qca_phy_priv_global[dev_id]->sw_dev);
#endif

#if IS_ENABLED(CONFIG_NET_DSA)
	if (priv->version == QCA_VER_MHT ||
		priv->version == QCA_VER_AR8337 ||
		priv->version == QCA_VER_AR8327)
		unregister_dsa_blocking_notifier(&ssdk_dsa_notifier_nb);
#endif

	return 0;
}

char ssdk_driver_name[] = "ess_ssdk";

static int ssdk_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct ssdk_driver_priv *priv = NULL;
	struct device *dev = &pdev->dev;
	const adpt_ppe_type_t *type;

	np = of_node_get(pdev->dev.of_node);
	if (of_device_is_compatible(np, "qcom,ess-instance"))
		return 0;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	of_property_read_u32(np, "device_id", &(priv->dev_id));
	type = of_device_get_match_data(dev);
	priv->ppe_type = (adpt_ppe_type_t)(uintptr_t)type;

	platform_set_drvdata(pdev, priv);

	return 0;
}

static void ssdk_shutdown(struct platform_device *pdev)
{
	struct ssdk_driver_priv *priv = platform_get_drvdata(pdev);

	if (!priv)
		return;
	ssdk_mac_sw_sync_work_stop(priv->dev_id);
}

static const struct of_device_id ssdk_of_mtable[] = {
	{.compatible = "qcom,ess-switch", .data = (void *)HPPE_TYPE },
	{.compatible = "qcom,ess-switch-ipq60xx", .data = (void *)CPPE_TYPE },
	{.compatible = "qcom,ess-switch-ipq807x", .data = (void *)HPPE_TYPE},
	{.compatible = "qcom,ess-switch-ipq95xx", .data = (void *)APPE_TYPE },
	{.compatible = "qcom,ess-switch-ipq53xx", .data = (void *)MPPE_TYPE },
	{.compatible = "qcom,ess-switch-ipq54xx", .data = (void *)MRPPE_TYPE },
	{.compatible = "qcom,ess-switch-ipq96xx", .data = (void *)JHPPE_TYPE },
	{.compatible = "qcom,ess-switch-ipq52xx", .data = (void *)HMSPPE_TYPE },
	{.compatible = "qcom,ess-switch-qce22xx", .data = (void *)HTTPPE_TYPE },
	{.compatible = "qcom,ess-instance", .data = (void *)MAX_PPE_TYPE },
	{}
};

static struct platform_driver ssdk_driver = {
        .driver = {
                .name    = ssdk_driver_name,
                .owner   = THIS_MODULE,
                .of_match_table = ssdk_of_mtable,
        },
        .probe    = ssdk_probe,
        .shutdown = ssdk_shutdown,
};

/*qca808x_start*/
sw_error_t
ssdk_init(a_uint32_t dev_id, ssdk_init_cfg * cfg)
{
	sw_error_t rv;

	rv = fal_init(dev_id, cfg);
	if (rv != SW_OK)
		SSDK_ERROR("ssdk fal init failed: %d. \r\n", rv);

/*qca808x_end*/
/*qca808x_start*/
	rv = ssdk_phy_driver_init(dev_id);
		if (rv != SW_OK)
			SSDK_ERROR("ssdk phy init failed: %d. \r\n", rv);

	return rv;
}

sw_error_t
ssdk_cleanup(a_uint32_t dev_id)
{
	sw_error_t rv;

	rv = fal_cleanup(dev_id);
	SW_RTN_ON_ERROR(rv);
/*qca808x_end*/
/*qca808x_start*/
	return  ssdk_phy_driver_cleanup(dev_id);
}
/*qca808x_end*/

static void ssdk_driver_register(void)
{
	platform_driver_register(&ssdk_driver);
}

static void ssdk_driver_unregister(void)
{
	platform_driver_unregister(&ssdk_driver);
}

/*qca808x_start*/
static inline a_uint32_t qca_detect_phyid(a_uint32_t dev_id)
{
	a_uint32_t phy_id = 0, port_id = 0;
	a_uint32_t port_bmp = qca_ssdk_port_bmp_get(dev_id);
	while (port_bmp) {
		if (port_bmp & 0x1) {
			phy_id = hsl_phyid_get(dev_id, port_id);
			if (INVALID_PHY_ID != phy_id)
				break;
		}
		port_bmp >>= 1;
		port_id++;
	}
	return phy_id;
}

static int chip_is_scomphy(a_uint32_t dev_id, ssdk_init_cfg* cfg)
{
	int rv = -ENODEV;
	a_uint32_t phy_id = qca_detect_phyid(dev_id);

	switch (phy_id) {
		/*qca808x_end*/
		case QCA8030_PHY:
		case QCA8033_PHY:
		case QCA8035_PHY:
		case MP_GEPHY:
			/*qca808x_start*/
		case QCA8081_PHY:
			cfg->chip_type = CHIP_SCOMPHY;
			/*MP GEPHY is always the first port*/
			if(cfg->phy_id == 0) {
				cfg->phy_id = phy_id;
			}
			rv = SW_OK;
			break;
		default:
			break;
	}

	return rv;
}

#if defined(JHPPE) || defined(HMSPPE)
/**
 * ssdk_tcsr_soc_hw_version_get - Read chip revision from TCSR SOC HW VERSION.
 * Valid only on IPQ96xx (JHPPE) and IPQ52xx (HMSPPE) SoCs.
 * Returns encoded revision 0xMMmm, or 0 on failure.
 * Result is cached after the first successful read.
 */
static a_uint32_t ssdk_tcsr_soc_hw_version_get(void)
{
	static a_uint32_t cached_revision = 0;
	static a_bool_t cached = A_FALSE;
	void __iomem *tcsr_base;
	a_uint32_t val;

	if (cached)
		return cached_revision;

	tcsr_base = ioremap(TCSR_SOC_HW_VERSION_PHYS, TCSR_SOC_HW_VERSION_SIZE);
	if (!tcsr_base) {
		SSDK_ERROR("ioremap failed for TCSR_SOC_HW_VERSION 0x%x\n",
			   TCSR_SOC_HW_VERSION_PHYS);
		return 0;
	}

	val = readl(tcsr_base);
	iounmap(tcsr_base);

	cached_revision = TCSR_SOC_CHIP_REVISION(val);
	cached = A_TRUE;

	SSDK_INFO("TCSR_SOC_HW_VERSION: reg=0x%08x major=%d minor=%d revision=0x%04x\n",
		  val,
		  TCSR_SOC_VERSION_MAJOR(val),
		  TCSR_SOC_VERSION_MINOR(val),
		  cached_revision);

	return cached_revision;
}
#endif

static int chip_ver_get(a_uint32_t dev_id, ssdk_init_cfg* cfg)
{
	int rv = SW_OK;
	a_uint8_t chip_ver = 0;
	a_uint8_t chip_revision = 0;
/*qca808x_end*/
	hsl_reg_mode reg_mode;
	a_uint32_t reg_val = 0;

	reg_mode= ssdk_switch_reg_access_mode_get(dev_id);
	if(reg_mode == HSL_REG_MDIO) {
		a_uint32_t phy_id = qca_detect_phyid(dev_id);
		switch (phy_id) {
			case QCA8084_PHY:
				chip_ver = QCA_VER_MHT;
				break;
			case QCE1204_PHY:
				chip_ver = QCA_VER_HTTPPE;
				break;
			case QCA8337_PHY_V4:
				chip_ver = QCA_VER_AR8337;
			default:
				break;
		}
	} else {
		qca_switch_reg_read(dev_id,0,(a_uint8_t *)&reg_val, 4);
		chip_ver = (reg_val&0xff00)>>8;
		chip_revision = reg_val&0xff;
	}
	qca_phy_priv_global[dev_id]->version = chip_ver;
	qca_phy_priv_global[dev_id]->revision = chip_revision;
/*qca808x_start*/
	switch (chip_ver) {
		case QCA_VER_AR8337:
			cfg->chip_type = CHIP_ISISC;
			break;
		case QCA_VER_AR8327:
			cfg->chip_type = CHIP_ISIS;
			break;
		case QCA_VER_DESS:
			cfg->chip_type = CHIP_DESS;
			break;
		case QCA_VER_HPPE:
			cfg->chip_type = CHIP_HPPE;
			cfg->chip_revision = chip_revision;
			break;
		case QCA_VER_APPE:
			cfg->chip_type = CHIP_APPE;
			cfg->chip_revision = chip_revision;
			break;
		case QCA_VER_MRPPE:
			cfg->chip_type = CHIP_MRPPE;
			break;
		case QCA_VER_MHT:
			cfg->chip_type = CHIP_MHT;
			break;
		case QCA_VER_JHPPE:
			cfg->chip_type = CHIP_JHPPE;
#if defined(JHPPE)
			cfg->chip_revision = ssdk_tcsr_soc_hw_version_get();
#endif
			break;
		case QCA_VER_HMSPPE:
			cfg->chip_type = CHIP_HMSPPE;
#if defined(HMSPPE)
			cfg->chip_revision = ssdk_tcsr_soc_hw_version_get();
#endif
			break;
		case QCA_VER_HTTPPE:
			cfg->chip_type = CHIP_HTTPPE;
			break;
		default:
			/* try single phy without switch connected */
			rv = chip_is_scomphy(dev_id, cfg);
	}

	return rv;
}
/*qca808x_end*/

/*qca808x_start*/
static void ssdk_cfg_default_init(ssdk_init_cfg *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->cpu_mode = HSL_CPU_1;
	cfg->nl_prot = 30;
/*qca808x_end*/

	cfg->reg_func.header_reg_set = qca_switch_reg_write;
	cfg->reg_func.header_reg_get = qca_switch_reg_read;
	cfg->reg_func.mii_reg_set = qca_mii_write;
	cfg->reg_func.mii_reg_get = qca_mii_read;
/*qca808x_start*/
}
/*qca808x_end*/

//#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
static int ssdk_dev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	int rv = 0;
	ssdk_init_cfg cfg;
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	ssdk_cfg_default_init(&cfg);
	rv = chip_ver_get(0, &cfg);
	if (rv) {
		SSDK_ERROR("chip verfion get failed\n");
		return NOTIFY_DONE;
	}
	switch (event) {
#ifdef ISISC
		case NETDEV_CHANGEMTU:
			if(dev->type == ARPHRD_ETHER) {
				if (cfg.chip_type == CHIP_DESS ||
					cfg.chip_type == CHIP_ISIS ||
					cfg.chip_type == CHIP_ISISC) {
					struct net_device *eth_dev = NULL;
					unsigned int mtu= 0;

					if(!strcmp(dev->name, "eth0")) {
						eth_dev = dev_get_by_name(&init_net, "eth1");
					} else if (!strcmp(dev->name, "eth1")) {
						eth_dev = dev_get_by_name(&init_net, "eth0");
					} else {
						return NOTIFY_DONE;
					}
					if (!eth_dev) {
						return NOTIFY_DONE;
					}
					mtu = dev->mtu > eth_dev->mtu ? dev->mtu : eth_dev->mtu;
#ifdef IN_MISC
					fal_frame_max_size_set(0, mtu + 18);
#endif
					dev_put(eth_dev);
				}
			}
			break;
#endif
		case NETDEV_REGISTER:
			if (strstr(dev->name, "eth") && !(dev->priv_flags & IFF_802_1Q_VLAN))
				ssdk_netdev_switch_init(dev);
#if IS_ENABLED(CONFIG_NET_DSA)
			if (dsa_slave_dev_check(dev))
				ssdk_dsa_intf_register(dev);
#endif
			break;
	}

	return NOTIFY_DONE;
}

/*qca808x_start*/
static void ssdk_free_priv(void)
{
	a_uint32_t dev_id, dev_num = 1;

	if(!qca_phy_priv_global) {
		return;
	}
/*qca808x_end*/
	dev_num = ssdk_switch_device_num_get();
/*qca808x_start*/
	for (dev_id = 0; dev_id < dev_num; dev_id++) {
		if (qca_phy_priv_global[dev_id]) {
			/* Free shaper private data */
			if (qca_phy_priv_global[dev_id]->shaper_priv) {
				kfree(qca_phy_priv_global[dev_id]->shaper_priv);
			}
			kfree(qca_phy_priv_global[dev_id]);
		}

		qca_phy_priv_global[dev_id] = NULL;
	}

	kfree(qca_phy_priv_global);

	qca_phy_priv_global = NULL;
/*qca808x_end*/
	ssdk_switch_device_num_exit();
/*qca808x_start*/
}

static int ssdk_alloc_priv(a_uint32_t dev_num)
{
	int rev = SW_OK;
	a_uint32_t dev_id = 0;

	qca_phy_priv_global = kzalloc(dev_num * sizeof(struct qca_phy_priv *), GFP_KERNEL);
	if (qca_phy_priv_global == NULL) {
		return -ENOMEM;
	}

	for (dev_id = 0; dev_id < dev_num; dev_id++) {
		qca_phy_priv_global[dev_id] = kzalloc(sizeof(struct qca_phy_priv), GFP_KERNEL);
		if (qca_phy_priv_global[dev_id] == NULL) {
			return -ENOMEM;
		}
		/* Allocate shaper private data for multi-device support */
		qca_phy_priv_global[dev_id]->shaper_priv = kzalloc(sizeof(ssdk_ppe_shaper_priv_t), GFP_KERNEL);
		if (qca_phy_priv_global[dev_id]->shaper_priv == NULL) {
			return -ENOMEM;
		}
/*qca808x_end*/
		qca_phy_priv_global[dev_id]->qca_ssdk_sw_dev_registered = A_FALSE;
/*qca808x_start*/
		qca_ssdk_port_bmp_init(dev_id);
		qca_ssdk_phy_info_init(dev_id);
	}

	return rev;
}

#ifndef SSDK_STR
#define SSDK_STR "ssdk"
#endif
#if defined (ISISC) || defined (ISIS)
static void qca_ar8327_gpio_reset(struct qca_phy_priv *priv)
{
	struct device_node *np = NULL;
	const __be32 *reset_gpio;
	a_int32_t len;
	int gpio_num = 0, ret = 0;

	if (priv->of_node)
		np = priv->of_node;
	if(!np)
		return;
	gpio_num = of_get_named_gpio(np, "reset_gpio", 0);
	if(gpio_num < 0)
	{
		reset_gpio = of_get_property(np, "reset_gpio", &len);
		if (!reset_gpio )
		{
			SSDK_INFO("reset_gpio node does not exist\n");
			return;
		}
		gpio_num = be32_to_cpup(reset_gpio);
		if(gpio_num <= 0)
		{
			SSDK_INFO("reset gpio doesn't exist\n ");
			return;
		}
	}
	ret = gpio_request(gpio_num, "reset_gpio");
	if(ret)
	{
		SSDK_ERROR("gpio%d request failed, ret:%d\n", gpio_num, ret);
		return;
	}
	gpio_direction_output(gpio_num, SSDK_GPIO_RESET);
	msleep(200);
	gpio_set_value(gpio_num, SSDK_GPIO_RELEASE);
	msleep(10);
	SSDK_INFO("GPIO%d reset switch done\n", gpio_num);

	gpio_free(gpio_num);

	return;
}
#endif

void ssdk_init_status_debug_state(a_uint32_t dev_id, ssdk_init_state_t state, int rv)
{
	struct qca_phy_priv *priv;

	if (dev_id >= SW_MAX_NR_DEV || !qca_phy_priv_global)
		return;

	priv = qca_phy_priv_global[dev_id];
	if (!priv)
		return;

	if (rv != SW_OK || state == SSDK_INIT_START || state == SSDK_INIT_SUCCESS) {
		priv->ssdk_module_cnt.ssdk_init_state = state;
	}
}

static int __init regi_init(void)
{
	a_uint32_t num = 0, dev_id = 0, dev_num = 1;
	ssdk_init_cfg cfg = {0};
/*qca808x_end*/
/*qca808x_start*/
	int rv = 0;
/*qca808x_end*/

	ssdk_driver_register();

	/*init switch device num firstly*/
	ssdk_switch_device_num_init();

	dev_num = ssdk_switch_device_num_get();
/*qca808x_start*/
	rv = ssdk_alloc_priv(dev_num);
	if (rv)
		goto out;


	for (num = 0; num < dev_num; num++) {
		ssdk_cfg_default_init(&cfg);
/*qca808x_end*/
		if(SW_DISABLE == ssdk_dt_parse(&cfg, num, &dev_id)) {
			SSDK_INFO("ess-switch node is unavalilable\n");
			ssdk_init_status_debug_state(dev_id, SSDK_DTS_PARSE_FAILURE, SW_DISABLE);
			continue;
		}
		ssdk_init_status_debug_state(dev_id, SSDK_INIT_START, SW_OK);
		/* device id is the array index */
		qca_phy_priv_global[dev_id]->device_id = ssdk_device_id_get(dev_id);
		qca_phy_priv_global[dev_id]->of_node = ssdk_dts_node_get(dev_id);
		INIT_LIST_HEAD(&(qca_phy_priv_global[dev_id]->sw_fdb_tbl));
		qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT7;

/*qca808x_start*/
		rv = ssdk_plat_init(&cfg, dev_id);
		ssdk_init_status_debug_state(dev_id, SSDK_PLAT_INIT_FAILURE,rv);
		SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
/*qca808x_end*/
/*qca808x_start*/
		rv = chip_ver_get(dev_id, &cfg);
		ssdk_init_status_debug_state(dev_id, SSDK_CHIP_VER_GET_FAILURE, rv);
		SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);

		rv = ssdk_init(dev_id, &cfg);
		SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
/*qca808x_end*/


		switch (cfg.chip_type)
		{
			case CHIP_ISIS:
			case CHIP_ISISC:
#if defined (ISISC) || defined (ISIS)
				if (qca_phy_priv_global[dev_id]->of_node) {
					qca_ar8327_gpio_reset(qca_phy_priv_global[dev_id]);
					rv = ssdk_switch_register(dev_id, cfg.chip_type);
					ssdk_init_status_debug_state(dev_id, SSDK_SWITCH_REGISTER_FAILURE, rv);
					SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
					rv = qca_ar8327_hw_init(qca_phy_priv_global[dev_id]);
					ssdk_init_status_debug_state(dev_id, SSDK_HW_INIT_FAILURE, rv);
					SSDK_INFO("Initializing ISISC Done!!\n");
				}
#endif
				break;
			case CHIP_MHT:
#if defined(MHT)
				qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT6;
				rv = qca_mht_hw_init(&cfg, dev_id);
				ssdk_init_status_debug_state(dev_id, SSDK_HW_INIT_FAILURE, rv);
				SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
				rv = ssdk_switch_register(dev_id, cfg.chip_type);
				ssdk_init_status_debug_state(dev_id, SSDK_SWITCH_REGISTER_FAILURE, rv);
				SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
				SSDK_INFO("Initializing MHT Done!!\n");
#endif
				break;
			case CHIP_HMSPPE:
			case CHIP_JHPPE:
			case CHIP_MRPPE:
			case CHIP_APPE:
				if(adpt_ppe_type_get(dev_id) == MRPPE_TYPE) {
					qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT4;
				}
				else if(adpt_ppe_type_get(dev_id) == MPPE_TYPE) {
					qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT3;
				}
				else if(adpt_ppe_type_get(dev_id) == HMSPPE_TYPE) {
					/* port0 cpu port, port1-port6 MAC ports, port7 loopback */
					qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT8;
				}
				else if(adpt_ppe_type_get(dev_id) == JHPPE_TYPE) {
					/* port0 cpu port, port1-port6 MAC ports, port7 EIP, port8 loopback */
					qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT8 + 1;
				}
				else if (adpt_ppe_type_get(dev_id) == APPE_TYPE) {
					qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT8;
				}
				rv = qca_appe_hw_init(dev_id);
				ssdk_init_status_debug_state(dev_id, SSDK_HW_INIT_FAILURE, rv);
				rv = ssdk_switch_register(dev_id, cfg.chip_type);
				ssdk_init_status_debug_state(dev_id, SSDK_SWITCH_REGISTER_FAILURE, rv);
				SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
				SSDK_INFO("Initializing %s Done!!\n", PPE_STR);
				break;
#if defined(HTTPPE)
			case CHIP_HTTPPE: {
				qca_phy_priv_global[dev_id]->ports_num = SSDK_PHYSICAL_PORT6;
				struct device_node *dsa_node = of_find_compatible_node(NULL, NULL, "qcom,qce2204");
				if (dsa_node) {
					struct clk *core_clk = of_clk_get_by_name(dsa_node, "core");
					if (!IS_ERR(core_clk)) {
						qca_phy_priv_global[dev_id]->core_clk = core_clk;
					} else {
						SSDK_ERROR("Failed to get HTTPPE core clock from DSA node\n");
					}
					mutex_init(&qca_phy_priv_global[dev_id]->reg_mutex);
					of_node_put(dsa_node);
				} else {
					rv = qca_httppe_hw_init(dev_id);
					ssdk_init_status_debug_state(dev_id, SSDK_HW_INIT_FAILURE, rv);
					rv = ssdk_switch_register(dev_id, cfg.chip_type);
					ssdk_init_status_debug_state(dev_id, SSDK_SWITCH_REGISTER_FAILURE, rv);
					SW_CNTU_ON_ERROR_AND_COND1_OR_GOTO_OUT(rv, -ENODEV);
					SSDK_INFO("Initializing HTTPPE Done!!\n");
				}
				break;
			}
#endif
			case CHIP_UNSPECIFIED:
				break;
			default:
				break;
		}
		ssdk_init_status_debug_state(dev_id, SSDK_INIT_SUCCESS, SW_OK);
	}

	if (rv == 0){
		/* register the notifier later should be ok */
		ssdk_dev_notifier.notifier_call = ssdk_dev_event;
		ssdk_dev_notifier.priority = 1;
		register_netdevice_notifier(&ssdk_dev_notifier);
	}
/*qca808x_start*/

out:
	ssdk_sysfs_init();
#ifdef IN_NETLINK
	ssdk_genl_init();
#endif
	if (rv == 0)
		SSDK_INFO("qca-%s module init succeeded!\n", SSDK_STR);
	else {
		if (rv == -ENODEV) {
			rv = 0;
			SSDK_INFO("qca-%s module init, no device found!\n", SSDK_STR);
		} else {
			SSDK_INFO("qca-%s module init failed! (code: %d)\n", SSDK_STR, rv);
			ssdk_free_priv();
		}
	}

	return rv;
}

static void __exit
regi_exit(void)
{
	a_uint32_t dev_id, dev_num = 1;
	sw_error_t rv = SW_OK;
/*qca808x_end*/
	unregister_netdevice_notifier(&ssdk_dev_notifier);

	dev_num = ssdk_switch_device_num_get();
	for (dev_id = 0; dev_id < dev_num; dev_id++) {
		if (qca_phy_priv_global[dev_id]->qca_ssdk_sw_dev_registered == A_TRUE)
			ssdk_switch_unregister(dev_id);
#ifdef IN_SFP_PHY
		sfp_phy_exit(dev_id);
#endif
		rv = ssdk_cleanup(dev_id);
	}

	ssdk_driver_unregister();

	if (rv == SW_OK)
		SSDK_INFO("qca-%s module exit  done!\n", SSDK_STR);
	else
		SSDK_ERROR("qca-%s module exit failed! (code: %d)\n", SSDK_STR, rv);
/*qca808x_end*/

	ssdk_sysfs_exit();
	for (dev_id = 0; dev_id < dev_num; dev_id++) {
		ssdk_plat_exit(dev_id);
	}
#ifdef IN_NETLINK
	ssdk_genl_deinit();
#endif

/*qca808x_start*/
	ssdk_free_priv();
}

module_init(regi_init);
module_exit(regi_exit);

MODULE_DESCRIPTION("QCA SSDK Driver");
MODULE_LICENSE("Dual BSD/GPL");
/*qca808x_end*/
