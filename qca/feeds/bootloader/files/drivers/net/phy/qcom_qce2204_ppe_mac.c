/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <linux/types.h>
#include <phy.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include "qcom_qce2204_ppe.h"

/**
 * qce2204_port_gmac_hw_init - Initialize GMAC hardware
 * @phydev: PHY device
 * @port: Port number
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce2204_port_gmac_hw_init(struct phy_device *phydev, int port)
{
	u32 reg, val;
	int ret;

	reg = QCE2204_PPE_GMAC_ADDR(port);

	/* GMAC RX and TX are initialized as disabled */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_ENABLE_ADDR,
				      QCE2204_PPE_GMAC_TRXEN, 0);
	if (ret)
		return ret;

	/* GMAC jumbo frame size configuration */
	val = FIELD_PREP(QCE2204_PPE_GMAC_JUMBO_SIZE_M, QCE2204_PORT_MAC_MAX_FRAME_SIZE);
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_JUMBO_SIZE_ADDR,
				      QCE2204_PPE_GMAC_JUMBO_SIZE_M, val);
	if (ret)
		return ret;

	/* GMAC max frame size and TX threshold configuration */
	val = FIELD_PREP(QCE2204_PPE_GMAC_MAXFRAME_SIZE_M, QCE2204_PORT_MAC_MAX_FRAME_SIZE);
	val |= FIELD_PREP(QCE2204_PPE_GMAC_TX_THD_M, QCE2204_PPE_GMAC_TX_THD_DEFAULT);
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_CTRL0_ADDR,
				      QCE2204_PPE_GMAC_CTRL_MASK, val);
	if (ret)
		return ret;

	/* GMAC high IPG configuration */
	val = FIELD_PREP(QCE2204_PPE_GMAC_HIGH_IPG_M, QCE2204_PPE_GMAC_HIGH_IPG_DEFAULT);
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_CTRL1_ADDR,
				      QCE2204_PPE_GMAC_HIGH_IPG_M, val);
	if (ret)
		return ret;

	/* Enable and reset GMAC MIB counters and set as read clear mode */
	ret = qce2204_ppe_set_bits(phydev, reg + QCE2204_PPE_GMAC_MIB_CTRL_ADDR,
				   QCE2204_PPE_GMAC_MIB_CTRL_MASK);
	if (ret)
		return ret;

	ret = qce2204_ppe_clear_bits(phydev, reg + QCE2204_PPE_GMAC_MIB_CTRL_ADDR,
				     QCE2204_PPE_GMAC_MIB_RST);
	if (ret)
		return ret;

	debug("QCE2204: Port %d GMAC hardware initialized\n", port);
	return 0;
}

/**
 * qce2204_port_xgmac_hw_init - Initialize XGMAC hardware
 * @phydev: PHY device
 * @port: Port number (0 or 5)
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce2204_port_xgmac_hw_init(struct phy_device *phydev, int port)
{
	u32 reg, val;
	int ret;

	if (port == 0)
		reg = QCE2204_PPE_XGMAC_ADDR(0);
	else if (port == 5)
		reg = QCE2204_PPE_XGMAC_ADDR(1);
	else
		return -EINVAL;

	/* XGMAC TX disabled and jumbo disable configuration */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_TX_CONFIG_ADDR,
				      QCE2204_PPE_XGMAC_TXEN | QCE2204_PPE_XGMAC_JD,
				      QCE2204_PPE_XGMAC_JD);
	if (ret)
		return ret;

	/* XGMAC RX configuration with max frame size */
	val = FIELD_PREP(QCE2204_PPE_XGMAC_GPSL_M, QCE2204_PORT_MAC_MAX_FRAME_SIZE);
	val |= QCE2204_PPE_XGMAC_GPSLEN;
	val |= QCE2204_PPE_XGMAC_CST;
	val |= QCE2204_PPE_XGMAC_ACS;
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_RX_CONFIG_ADDR,
				      QCE2204_PPE_XGMAC_RX_CONFIG_MASK, val);
	if (ret)
		return ret;

	/* XGMAC watchdog timeout configuration */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_WD_TIMEOUT_ADDR,
				      QCE2204_PPE_XGMAC_WD_TIMEOUT_MASK,
				      QCE2204_PPE_XGMAC_WD_TIMEOUT_VAL);
	if (ret)
		return ret;

	/* XGMAC packet filter configuration */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_PKT_FILTER_ADDR,
				      QCE2204_PPE_XGMAC_PKT_FILTER_MASK,
				      QCE2204_PPE_XGMAC_PKT_FILTER_VAL);
	if (ret)
		return ret;

	/* Enable and reset XGMAC MIB counters */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_MMC_CTRL_ADDR,
				      QCE2204_PPE_XGMAC_MCF | QCE2204_PPE_XGMAC_CNTRST,
				      QCE2204_PPE_XGMAC_CNTRST);

	if (ret)
		return ret;

//	ret = qce2204_ppe_update_bits(phydev, 0x10, BIT(8), BIT(8));

	debug("QCE2204: Port %d XGMAC hardware initialized\n", port);
	return ret;
}

/**
 * qce2204_port_mac_init - Initialize port MAC configuration
 * @phydev: PHY device
 *
 * Initialize MAC configuration for all ports.
 * For port 0 and 5, initialize both GMAC and XGMAC.
 * For ports 1-4, initialize only GMAC.
 *
 * Return: 0 on success, negative error code on failure
 */
int qce2204_port_mac_init(struct phy_device *phydev)
{
	int port, ret;

	debug("QCE2204: Initializing port MAC configurations\n");

	/* Initialize all 6 ports (0-5) */
	for (port = 0; port < QCE2204_NUM_PORTS; port++) {
		debug("QCE2204: Initializing port %d MAC\n", port);

		if (port == 0 || port == 5) {
			/* Initialize both GMAC and XGMAC for ports 0 and 5 */
			debug("QCE2204: Port %d - initializing GMAC and XGMAC\n", port);

			ret = qce2204_port_gmac_hw_init(phydev, port);
			if (ret) {
				debug("QCE2204: Failed to initialize GMAC for port %d: %d\n",
				      port, ret);
				return ret;
			}

			ret = qce2204_port_xgmac_hw_init(phydev, port);
			if (ret) {
				debug("QCE2204: Failed to initialize XGMAC for port %d: %d\n",
				      port, ret);
				return ret;
			}
		} else {
			/* Initialize only GMAC for ports 1-4 */
			debug("QCE2204: Port %d - initializing GMAC only\n", port);

			ret = qce2204_port_gmac_hw_init(phydev, port);
			if (ret) {
				debug("QCE2204: Failed to initialize GMAC for port %d: %d\n",
				      port, ret);
				return ret;
			}
		}
	}

	debug("QCE2204: Port MAC initialization completed successfully\n");
	return 0;
}

/**
 * qce2204_port_mac_deinit - Cleanup port MAC configuration
 * @phydev: PHY device
 *
 * Placeholder for cleanup operations.
 */
void qce2204_port_mac_deinit(struct phy_device *phydev)
{
	debug("QCE2204: Port MAC cleanup completed\n");
}

/**
 * qce2204_port_gmac_link_down - Configure GMAC link down
 * @phydev: PHY device
 * @port: Port number
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce2204_port_gmac_link_down(struct phy_device *phydev, int port)
{
	u32 reg;
	int ret;

	reg = QCE2204_PPE_GMAC_ADDR(port);

	/* Disable GMAC RX and TX */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_ENABLE_ADDR,
				      QCE2204_PPE_GMAC_TRXEN, 0);
	if (ret)
		return ret;

	debug("QCE2204: Port %d GMAC link down\n", port);
	return 0;
}

/**
 * qce2204_port_xgmac_link_down - Configure XGMAC link down
 * @phydev: PHY device
 * @port: Port number (0 or 5)
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce2204_port_xgmac_link_down(struct phy_device *phydev, int port)
{
	u32 reg;
	int ret;

	if (port == 0)
		reg = QCE2204_PPE_XGMAC_ADDR(0);
	else if (port == 5)
		reg = QCE2204_PPE_XGMAC_ADDR(1);
	else
		return -EINVAL;

	/* Disable XGMAC RX */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_RX_CONFIG_ADDR,
				      QCE2204_PPE_XGMAC_RXEN, 0);
	if (ret)
		return ret;

	/* Disable XGMAC TX */
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_TX_CONFIG_ADDR,
				      QCE2204_PPE_XGMAC_TXEN, 0);
	if (ret)
		return ret;

	debug("QCE2204: Port %d XGMAC link down\n", port);
	return 0;
}

/**
 * qce2204_phylink_mac_link_down - Handle phylink MAC link down
 * @phydev: PHY device
 * @port: Port number
 * @interface: PHY interface mode
 *
 * Return: 0 on success, negative error code on failure
 */
int qce2204_phylink_mac_link_down(struct phy_device *phydev, int port,
				  phy_interface_t interface)
{
	enum qce2204_port_mac_type mac_type;
	u32 reg;
	int ret;

	/* Disable PPE port bridge TX MAC */
	reg = QCE2204_PPE_PORT_BRIDGE_CTRL_ADDR + QCE2204_PPE_PORT_BRIDGE_CTRL_INC * port;
	ret = qce2204_ppe_clear_bits(phydev, reg, QCE2204_PPE_PORT_BRIDGE_TXMAC_EN);
	if (ret) {
		debug("QCE2204: Failed to disable bridge TX MAC for port %d: %d\n", port, ret);
		return ret;
	}

	/* Determine MAC type based on interface */
	switch (interface) {
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_USXGMII:
		mac_type = QCE2204_PORT_MAC_TYPE_XGMAC;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_INTERNAL:
	default:
		mac_type = QCE2204_PORT_MAC_TYPE_GMAC;
		break;
	}

	/* Configure MAC link down based on MAC type */
	if (mac_type == QCE2204_PORT_MAC_TYPE_GMAC)
		ret = qce2204_port_gmac_link_down(phydev, port);
	else
		ret = qce2204_port_xgmac_link_down(phydev, port);

	if (ret) {
		debug("QCE2204: Failed to configure port %d link down: %d\n", port, ret);
		return ret;
	}

	debug("QCE2204: Port %d link down completed\n", port);
	return 0;
}

/**
 * qce2204_port_gmac_link_up - Configure GMAC link up
 * @phydev: PHY device
 * @port: Port number
 * @speed: Link speed
 * @duplex: Duplex mode
 * @tx_pause: TX pause enabled
 * @rx_pause: RX pause enabled
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce2204_port_gmac_link_up(struct phy_device *phydev, int port,
				     int speed, int duplex,
				     bool tx_pause, bool rx_pause)
{
	u32 reg, val;
	int ret;

	if (port == 5)
		return 0;
	reg = QCE2204_PPE_GMAC_ADDR(port);

	/* Set GMAC speed */
	switch (speed) {
	case SPEED_2500:
	case SPEED_1000:
		val = QCE2204_PPE_GMAC_SPEED_1000;
		break;
	case SPEED_100:
		val = QCE2204_PPE_GMAC_SPEED_100;
		break;
	case SPEED_10:
		val = QCE2204_PPE_GMAC_SPEED_10;
		break;
	default:
		debug("QCE2204: Invalid GMAC speed %d for port %d\n", speed, port);
		return -EINVAL;
	}

	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_SPEED_ADDR,
				      QCE2204_PPE_GMAC_SPEED_M, val);
	if (ret)
		return ret;

	/* Set duplex, flow control and enable GMAC */
	val = QCE2204_PPE_GMAC_TXEN | QCE2204_PPE_GMAC_RXEN;
	if (duplex == DUPLEX_FULL)
		val |= QCE2204_PPE_GMAC_DUPLEX_FULL;
	if (tx_pause)
		val |= QCE2204_PPE_GMAC_TXFCEN;
	if (rx_pause)
		val |= QCE2204_PPE_GMAC_RXFCEN;

	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_GMAC_ENABLE_ADDR,
				      QCE2204_PPE_GMAC_ENABLE_ALL, val);
	if (ret)
		return ret;

	debug("QCE2204: Port %d GMAC link up: speed=%d, duplex=%s, tx_pause=%d, rx_pause=%d\n",
	      port, speed, duplex == DUPLEX_FULL ? "full" : "half", tx_pause, rx_pause);

	return 0;
}

/**
 * qce2204_port_xgmac_link_up - Configure XGMAC link up
 * @phydev: PHY device
 * @port: Port number (0 or 5)
 * @interface: PHY interface mode
 * @speed: Link speed
 * @duplex: Duplex mode
 * @tx_pause: TX pause enabled
 * @rx_pause: RX pause enabled
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce2204_port_xgmac_link_up(struct phy_device *phydev, int port,
				      phy_interface_t interface,
				      int speed, int duplex,
				      bool tx_pause, bool rx_pause)
{
	u32 reg, val;
	int ret;

	if (port == 0)
		reg = QCE2204_PPE_XGMAC_ADDR(0);
	else if (port == 5)
		reg = QCE2204_PPE_XGMAC_ADDR(1);
	else
		return -EINVAL;

	/* Set XGMAC TX speed and enable TX */
	switch (speed) {
	case SPEED_10000:
		if (interface == PHY_INTERFACE_MODE_USXGMII)
			val = QCE2204_PPE_XGMAC_SPEED_10000_USXGMII;
		else
			val = QCE2204_PPE_XGMAC_SPEED_10000;
		break;
	case SPEED_5000:
		val = QCE2204_PPE_XGMAC_SPEED_5000;
		break;
	case SPEED_2500:
		if (interface == PHY_INTERFACE_MODE_USXGMII)
			val = QCE2204_PPE_XGMAC_SPEED_2500_USXGMII;
		else
			val = QCE2204_PPE_XGMAC_SPEED_2500;
		break;
	case SPEED_1000:
		val = QCE2204_PPE_XGMAC_SPEED_1000;
		break;
	case SPEED_100:
		val = QCE2204_PPE_XGMAC_SPEED_100;
		break;
	case SPEED_10:
		val = QCE2204_PPE_XGMAC_SPEED_10;
		break;
	default:
		debug("QCE2204: Invalid XGMAC speed %d for port %d\n", speed, port);
		return -EINVAL;
	}

	if (interface == PHY_INTERFACE_MODE_10GBASER || port == 0)
		val = QCE2204_PPE_XGMAC_SPEED_10000;
	val |= QCE2204_PPE_XGMAC_TXEN;
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_TX_CONFIG_ADDR,
				      QCE2204_PPE_XGMAC_SPEED_M | QCE2204_PPE_XGMAC_TXEN,
				      val);
	if (ret)
		return ret;

	/* Set XGMAC TX flow control */
	/* Use maximum pause time value (all bits set in the field) */
	val = FIELD_PREP(QCE2204_PPE_XGMAC_PAUSE_TIME_M, 0xFFFF);
	val |= tx_pause ? QCE2204_PPE_XGMAC_TXFCEN : 0;
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_TX_FLOW_CTRL_ADDR,
				      QCE2204_PPE_XGMAC_PAUSE_TIME_M | QCE2204_PPE_XGMAC_TXFCEN,
				      val);
	if (ret)
		return ret;

	/* Set XGMAC RX flow control */
	val = rx_pause ? QCE2204_PPE_XGMAC_RXFCEN : 0;
	ret = qce2204_ppe_update_bits(phydev, reg + QCE2204_PPE_XGMAC_RX_FLOW_CTRL_ADDR,
				      QCE2204_PPE_XGMAC_RXFCEN, val);
	if (ret)
		return ret;

	/* Enable XGMAC RX */
	ret = qce2204_ppe_set_bits(phydev, reg + QCE2204_PPE_XGMAC_RX_CONFIG_ADDR,
				   QCE2204_PPE_XGMAC_RXEN);
	if (ret)
		return ret;

	debug("QCE2204: Port %d XGMAC link up: speed=%d, duplex=%s, tx_pause=%d, rx_pause=%d\n",
	      port, speed, duplex == DUPLEX_FULL ? "full" : "half", tx_pause, rx_pause);

	return 0;
}

/**
 * qce2204_phylink_mac_link_up - Handle phylink MAC link up
 * @phydev: PHY device
 * @port: Port number
 * @speed: Link speed
 * @duplex: Duplex mode
 * @interface: PHY interface mode
 * @tx_pause: TX pause enabled
 * @rx_pause: RX pause enabled
 *
 * Return: 0 on success, negative error code on failure
 */
int qce2204_phylink_mac_link_up(struct phy_device *phydev, int port,
				int speed, int duplex,
				phy_interface_t interface,
				bool tx_pause, bool rx_pause)
{
	enum qce2204_port_mac_type mac_type;
	u32 reg;
	int ret;

	/* Determine MAC type based on interface */
	switch (interface) {
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_USXGMII:
		mac_type = QCE2204_PORT_MAC_TYPE_XGMAC;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_INTERNAL:
	default:
		mac_type = QCE2204_PORT_MAC_TYPE_GMAC;
		break;
	}

	/* Configure MAC link up based on MAC type */
	if (mac_type == QCE2204_PORT_MAC_TYPE_GMAC) {
		ret = qce2204_port_gmac_link_up(phydev, port, speed, duplex,
						tx_pause, rx_pause);

	} else {
		ret = qce2204_port_gmac_link_up(phydev, port, speed, duplex,
						tx_pause, rx_pause);
		if (ret) {
			debug("QCE2204: Failed to configure GMAC"
				" port %d link up: %d\n", port, ret);
			return ret;
		}

		ret = qce2204_port_xgmac_link_up(phydev,
						 0,
						 interface, speed,
						 duplex, tx_pause, rx_pause);

		if (port == 5) {
			ret = qce2204_port_xgmac_link_up(phydev,
							 5,
							 interface, speed,
							 duplex, tx_pause, rx_pause);
		}

	}

	if (ret) {
		debug("QCE2204: Failed to configure port %d link up: %d\n", port, ret);
		return ret;
	}

	/* Set PPE port BM flow control - map port 0-5 to BM port 7-12 */
	reg = QCE2204_PPE_BM_PORT_FC_MODE_ADDR + QCE2204_PPE_BM_PORT_FC_MODE_INC * (port + 7);
	ret = qce2204_ppe_set_bits(phydev, reg, QCE2204_PPE_BM_PORT_FC_MODE_EN);
	if (ret) {
		debug("QCE2204: Failed to enable BM flow control for port %d: %d\n", port, ret);
		return ret;
	}

	/* Enable PPE port bridge TX MAC */
	reg = QCE2204_PPE_PORT_BRIDGE_CTRL_ADDR + QCE2204_PPE_PORT_BRIDGE_CTRL_INC * port;
	ret = qce2204_ppe_set_bits(phydev, reg, QCE2204_PPE_PORT_BRIDGE_TXMAC_EN);
	if (ret) {
		debug("QCE2204: Failed to enable bridge TX MAC for port %d: %d\n", port, ret);
		return ret;
	}

	debug("QCE2204: Port %d link up completed (speed=%d, duplex=%d)\n",
	      port, speed, duplex);
	return 0;
}

/**
 * qce2204_port_link_up - Wrapper function for qce1204 to call
 * @phydev: PHY device
 * @port: Port number
 * @speed: Link speed
 * @duplex: Duplex mode
 * @interface: PHY interface mode
 * @tx_pause: TX pause enabled
 * @rx_pause: RX pause enabled
 *
 * Return: 0 on success, negative error code on failure
 */
int qce2204_port_link_up(struct phy_device *phydev, int port,
			 int speed, int duplex,
			 phy_interface_t interface,
			 bool tx_pause, bool rx_pause)
{
	return qce2204_phylink_mac_link_up(phydev, port, speed, duplex,
					   interface, tx_pause, rx_pause);
}
