/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/phylink.h>
#include <linux/net_tstamp.h>

#if defined(NSS_DP_PPE_SUPPORT)
#include <fal/fal_vsi.h>
#include <ref/ref_vsi.h>
#endif
#if defined(NSS_DP_EDMA_I2C_BUS_ENABLE)
#include <fal/fal_port_ctrl.h>
#endif
#include <net/switchdev.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 6, 0))
#include <net/netdev_rx_queue.h>
#endif
#if defined(NSS_DP_MAC_POLL_SUPPORT)
#include <init/ssdk_init.h>
#endif
#include "nss_dp_hal.h"
#include <ppe_drv.h>
#include <ppe_drv_port.h>
#ifdef CONFIG_IPQ_PON
#include "nss_dp_gem.h"
#endif
#define JUMBO_MRU_3K 3072
#define NSS_DP_CAPWAP_VP_RX_CORE_INVALID 0XFFFF

extern char *saved_command_line;

/* ipq40xx_mdio_data */
struct ipq40xx_mdio_data {
	struct mii_bus *mii_bus;
	void __iomem *membase;
	int phy_irq[PHY_MAX_ADDR];
};

/* Global data */
struct nss_dp_global_ctx dp_global_ctx;
struct nss_dp_data_plane_ctx dp_global_data_plane_ctx[NSS_DP_MAX_PORTS];
#ifdef CONFIG_IPQ_PON
/* Define GEM callbacks */
nss_dp_gem_rx_cb_t nss_dp_gem_rx_reg_cb_g = NULL;
void * nss_dp_gem_rx_app_data_g = NULL;
nss_dp_gem_tx_cb_t nss_dp_gem_tx_reg_cb_g = NULL;
void * nss_dp_gem_tx_app_data_g = NULL;

/* PON drainout callback for UMAC reset */
nss_dp_pon_drainout_cb_t nss_dp_pon_drainout_reg_cb_g = NULL;
#endif
/* Module params */
static int page_mode;
module_param(page_mode, int, 0);
MODULE_PARM_DESC(page_mode, "enable page mode");

static int overwrite_mode;
module_param(overwrite_mode, int, 0);
MODULE_PARM_DESC(overwrite_mode, "overwrite default page_mode setting");

int jumbo_mru;
module_param(jumbo_mru, int, 0640);
MODULE_PARM_DESC(jumbo_mru, "jumbo mode");

int tx_requeue_stop = 1;
module_param(tx_requeue_stop, int, 0640);
MODULE_PARM_DESC(tx_requeue_stop, "disable tx requeue function");

uint32_t nss_dp_capwap_vp_rx_core = NSS_DP_CAPWAP_VP_RX_CORE_INVALID;
module_param(nss_dp_capwap_vp_rx_core, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_capwap_vp_rx_core, "Capwap VP handling core");

int nss_dp_rx_napi_budget = NSS_DP_HAL_RX_NAPI_BUDGET;
module_param(nss_dp_rx_napi_budget, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rx_napi_budget, "Rx NAPI budget");

int nss_dp_rxfill_napi_budget = NSS_DP_HAL_RXFILL_NAPI_BUDGET;
module_param(nss_dp_rxfill_napi_budget, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rxfill_napi_budget, "Rx-fill NAPI budget");

int nss_dp_tx_napi_budget = NSS_DP_HAL_TX_NAPI_BUDGET;
module_param(nss_dp_tx_napi_budget, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_tx_napi_budget, "Tx NAPI budget");

int nss_dp_recovery_en = 0;
module_param(nss_dp_recovery_en, int, 0640);
MODULE_PARM_DESC(nss_dp_recovery_en, "Enable EDMA recovery (1 for enable, 0 for disable)");

#ifdef NSS_DP_MHT_SW_PORT_MAP
int nss_dp_mht_multi_txring = 0;
module_param(nss_dp_mht_multi_txring, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_mht_multi_txring, "MHT SW ports to Tx rings map");
#endif

#if defined(NSS_DP_EDMA_V2) || defined(NSS_DP_EDMA_V3)
int nss_dp_rx_fc_xoff = NSS_DP_RX_FC_XOFF_DEF;
module_param(nss_dp_rx_fc_xoff, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rx_fc_xoff, "Rx ring's flow control XOFF threshold value");

int nss_dp_rx_fc_xon = NSS_DP_RX_FC_XON_DEF;
module_param(nss_dp_rx_fc_xon, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rx_fc_xon, "Rx ring's flow control XON threshold value");

int nss_dp_rx_ac_fc_threshold = NSS_DP_RX_AC_FC_THRES_DEF;
module_param(nss_dp_rx_ac_fc_threshold, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rx_ac_fc_threshold, "Rx ring's mapped PPE queue's FC threshold value");

int nss_dp_tx_mitigation_timer = NSS_DP_TX_MITIGATION_TIMER_DEF;
module_param(nss_dp_tx_mitigation_timer, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_tx_mitigation_timer, "Tx mitigation timer value in microseconds");

int nss_dp_tx_mitigation_pkt_cnt = NSS_DP_TX_MITIGATION_PKT_CNT_DEF;
module_param(nss_dp_tx_mitigation_pkt_cnt, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_tx_mitigation_pkt_cnt, "Tx mitigation packet count value");

int nss_dp_txcmpl_fc_threshold_cnt = NSS_DP_TXCMPL_FC_THRESHOLD_DEF;
module_param(nss_dp_txcmpl_fc_threshold_cnt, int, 0444);
MODULE_PARM_DESC(nss_dp_txcmpl_fc_threshold_cnt, "Txcmpl FC threshold count value");

int nss_dp_rx_mitigation_timer = NSS_DP_RX_MITIGATION_TIMER_DEF;
module_param(nss_dp_rx_mitigation_timer, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rx_mitigation_timer, "Rx mitigation timer value in microseconds");

int nss_dp_rx_mitigation_pkt_cnt = NSS_DP_RX_MITIGATION_PKT_CNT_DEF;
module_param(nss_dp_rx_mitigation_pkt_cnt, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_rx_mitigation_pkt_cnt, "Rx mitigation packet count value");

#ifdef CONFIG_SKB_TIMESTAMP
static int nss_dp_tstamp_port_id = NSS_DP_EDMA_DEF_TSTAMP_PORT;
module_param(nss_dp_tstamp_port_id, int, S_IRUGO);
MODULE_PARM_DESC(nss_dp_tstamp_port_id, "Port number for time stamping");
#endif

/*
 * Module parameter for priority mapping
 */
uint8_t nss_dp_pri_map[EDMA_PRI_MAX] = {0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7};
module_param_array(nss_dp_pri_map, byte, NULL, S_IRUGO);
MODULE_PARM_DESC(nss_dp_pri_map, "Priority to multi-queue mapping");
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 6, 0))
#define NSS_DP_GRE_CSUM_OFFSET 4
#endif

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
/*
 * Module parameter for Loopback ring
 */
#define EDMA_LOOPBACK_RING_SIZE 16384
#define EDMA_LOOPBACK_BUFFER_SIZE 1536

int edma_loopback_ring_size = EDMA_LOOPBACK_RING_SIZE;
module_param(edma_loopback_ring_size, int, S_IRUGO);
MODULE_PARM_DESC(edma_loopback_ring_size, "Loopback ring size");

int edma_loopback_buffer_size = EDMA_LOOPBACK_BUFFER_SIZE;
module_param(edma_loopback_buffer_size, int, S_IRUGO);
MODULE_PARM_DESC(edma_loopback_buffer_size, "Loopback buffer size");

/*
 * Module parameter to enable / disable spectific loopback feature type
 */
uint32_t edma_loopback_feature_type = 1;
module_param(edma_loopback_feature_type, int, 0644);
MODULE_PARM_DESC(edma_loopback_feature_type, "loopback feature type 0x0: disabled, 0x1: default, 0x2: ddr extended buffer, 0x4: gretap to mapt, 0x8: speed-mismatch DDR buffering");
#endif

uint32_t rx_ring_sz = 0;
module_param(rx_ring_sz, int, 0640);
MODULE_PARM_DESC(rx_ring_sz, "edma rx ring size");

uint32_t tx_ring_sz = 0;
module_param(tx_ring_sz, int, 0640);
MODULE_PARM_DESC(tx_ring_sz, "edma tx ring size");

/*
 * __get_mem_profile_flag()
 *	Get profile flag
 */
static u32 __get_mem_profile_flag(const char *cmdline)
{
	u32 flags = 0;

#ifdef NSS_DP_MEM_PROFILE_MEDIUM
	flags = NSS_DP_MEM_PROFILE_BALANCED;
#elif defined(NSS_DP_MEM_PROFILE_LOW)
	flags = NSS_DP_MEM_PROFILE_OPTIMIZED;
#else
	if (!cmdline)
		return NSS_DP_MEM_PROFILE_HIGH;

	const char *param = strstr(cmdline, "mem-profile=");
	if (!param)
		return NSS_DP_MEM_PROFILE_HIGH;

	param += strlen("mem-profile=");
	if (strncmp(param, "optimized", strlen("optimized")) == 0)
		flags |= NSS_DP_MEM_PROFILE_OPTIMIZED;
	else if (strncmp(param, "balanced", strlen("balanced")) == 0)
		flags |= NSS_DP_MEM_PROFILE_BALANCED;
	else
		flags |= NSS_DP_MEM_PROFILE_HIGH;
#endif
	return flags;
}

/*
 * nss_dp_detect_mem_profile()
 *	Detect memory profile
 */
static void nss_dp_mem_profile_detect(struct edma_gbl_ctx *gbl_ctx)
{
	u32 __rx_ring_sz, __tx_ring_sz;
	u32 flags;

	flags = __get_mem_profile_flag(saved_command_line);

	/*
	 * Default values for each profile, this is not expected
	 * to be changed often.
	 */
	if (flags & NSS_DP_MEM_PROFILE_OPTIMIZED) {
		__rx_ring_sz = 512;
		__tx_ring_sz = 1024;
	} else if (flags & NSS_DP_MEM_PROFILE_BALANCED) {
		__rx_ring_sz = 1024;
		__tx_ring_sz = 1024;
	} else if (flags & NSS_DP_MEM_PROFILE_HIGH) {
		__rx_ring_sz = 4096;
		__tx_ring_sz = 2048;
	} else {
		__rx_ring_sz = 4096;
		__tx_ring_sz = 2048;
	}

	gbl_ctx->mem_profile = flags;
	gbl_ctx->rx_ring_sz = rx_ring_sz ? rx_ring_sz : __rx_ring_sz;
	gbl_ctx->tx_ring_sz = tx_ring_sz ? tx_ring_sz : __tx_ring_sz;

#if (defined(NSS_DP_IPQ96XX) || defined(NSS_DP_IPQ52XX))
	nss_dp_hal_configure_ring_size(flags);
#endif
}

/*
* nss_dp_eth_ioctl()
*	Handle ethernet ioctls with PHY priority for PTP
 */
static int nss_dp_eth_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct nss_dp_dev *dp_priv;

	if (!netdev || !ifr)
		return -EINVAL;

	/*
	 * Try PHY-level PTP first (higher priority, more accurate)
	 * PHY timestamping is closer to the wire and typically more precise
	 */
	if (phy_has_hwtstamp(netdev->phydev))
		return phy_do_ioctl_running(netdev, ifr, cmd);

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);
	if (!dp_priv)
		return -EINVAL;

	/*
	 * Fall back to MAC-level PTP (XGMAC) if PHY doesn't support it
	 */
	if (dp_priv->gmac_hal_ops) {
		switch (cmd) {
		case SIOCSHWTSTAMP:
			if (dp_priv->gmac_hal_ops->hwtstamp_set)
				return dp_priv->gmac_hal_ops->hwtstamp_set(dp_priv->gmac_hal_ctx, ifr);
			break;
		case SIOCGHWTSTAMP:
			if (dp_priv->gmac_hal_ops->hwtstamp_get)
				return dp_priv->gmac_hal_ops->hwtstamp_get(dp_priv->gmac_hal_ctx, ifr);
			break;
		}
	}

	return -EOPNOTSUPP;
}

/*
 * nss_dp_do_ioctl()
 *	Legacy ioctl handler for older kernels
 */
static int32_t nss_dp_do_ioctl(struct net_device *netdev, struct ifreq *ifr,
						   int32_t cmd)
{
	struct nss_dp_dev *dp_priv;

	if (!netdev || !ifr)
		return -EINVAL;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

	/* Only handle PHY MII ioctls */
	if (dp_priv->phydev)
		return phy_mii_ioctl(dp_priv->phydev, ifr, cmd);

	return -EOPNOTSUPP;
}

/*
 * nss_dp_change_mtu()
 */
static int32_t nss_dp_change_mtu(struct net_device *netdev, int32_t newmtu)
{
	int ret = -EINVAL;
	struct nss_dp_dev *dp_priv;

	if (!netdev)
		return ret;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

	/*
	 * Check if data plane init has been done
	 */
	if (!(dp_priv->drv_flags & NSS_DP_PRIV_FLAG(INIT_DONE))) {
		return ret;
	}

	/*
	 * Configure the new MTU value to underlying HW.
	 */
	if (dp_priv->gmac_hal_ops->setmaxframe(dp_priv->gmac_hal_ctx, newmtu)) {
		netdev_dbg(netdev, "GMAC MTU change failed: %d\n", newmtu);
		return ret;
	}

	/*
	 * Let the underlying data plane decide if the newmtu is applicable.
	 */
	if (dp_priv->data_plane_ops->change_mtu(dp_priv->dpc, newmtu)) {
		netdev_dbg(netdev, "Data plane change mtu failed: %d\n",
								newmtu);
		dp_priv->gmac_hal_ops->setmaxframe(dp_priv->gmac_hal_ctx,
								netdev->mtu);
		return ret;
	}

	netdev->mtu = newmtu;
	return 0;
}

/*
 * nss_dp_set_mac_address()
 */
static int32_t nss_dp_set_mac_address(struct net_device *netdev, void *macaddr)
{
	struct nss_dp_dev *dp_priv;
	struct sockaddr *addr = (struct sockaddr *)macaddr;
	int ret = 0;

	if (!netdev)
		return -EINVAL;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

	/*
	 * Check if data plane init has been done
	 */
	if (!(dp_priv->drv_flags & NSS_DP_PRIV_FLAG(INIT_DONE))) {
		return -EINVAL;
	}

	netdev_dbg(netdev, "AddrFamily: %d, %0x:%0x:%0x:%0x:%0x:%0x\n",
			addr->sa_family, addr->sa_data[0], addr->sa_data[1],
			addr->sa_data[2], addr->sa_data[3], addr->sa_data[4],
			addr->sa_data[5]);

	ret = eth_prepare_mac_addr_change(netdev, addr);
	if (ret)
		return ret;

	if (dp_priv->data_plane_ops->mac_addr(dp_priv->dpc, (uint8_t *)addr->sa_data)) {
		netdev_dbg(netdev, "Data plane set MAC address failed\n");
		return -EAGAIN;
	}

	dp_priv->gmac_hal_ops->setmacaddr(dp_priv->gmac_hal_ctx,
			(uint8_t *)addr->sa_data);

	eth_commit_mac_addr_change(netdev, addr);

	return 0;
}

/*
 * nss_dp_get_stats64()
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
static struct rtnl_link_stats64 *nss_dp_get_stats64(struct net_device *netdev,
					     struct rtnl_link_stats64 *stats)
{
	struct nss_dp_dev *dp_priv;

	if (!netdev)
		return stats;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

	/*
	 * Get the GMAC MIB statistics
	 */
	dp_priv->gmac_hal_ops->getndostats(dp_priv->gmac_hal_ctx, stats);
	return stats;
}
#else
static void nss_dp_get_stats64(struct net_device *netdev,
					     struct rtnl_link_stats64 *stats)
{
	struct nss_dp_dev *dp_priv;

	if (!netdev)
		return;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

#ifdef NSS_DP_PON_SUPPORT
	if (dp_priv->gem_port) {
		struct ppe_drv_port_hw_stats pon_stats;

		/*
		 * WAR: Read PPE port statistics instead of MIB for PON port.
		 * Note: This will cover only packet and byte counters.
		 */
		ppe_drv_dp_get_pon_stats(&pon_stats);
		stats->rx_packets = pon_stats.rx_pkt_cnt;
		stats->rx_bytes   = pon_stats.rx_byte_cnt;
		stats->tx_packets = pon_stats.tx_pkt_cnt;
		stats->tx_bytes   = pon_stats.tx_byte_cnt;
		return;
	}
#endif
	/*
	 * Get the GMAC MIB statistics
	 */
	dp_priv->gmac_hal_ops->getndostats(dp_priv->gmac_hal_ctx, stats);
}
#endif

/*
 * nss_dp_xmit()
 */
static netdev_tx_t nss_dp_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct nss_dp_dev *dp_priv;

	if (!skb || !netdev)
		return NETDEV_TX_OK;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);
	netdev_dbg(netdev, "Tx packet, len %d\n", skb->len);

	return dp_priv->data_plane_ops->xmit(dp_priv->dpc, skb);
}

/*
 * nss_dp_close()
 */
static int nss_dp_close(struct net_device *netdev)
{
	struct nss_dp_dev *dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

	if (!dp_priv)
		return -EINVAL;

	netif_stop_queue(netdev);

	/* Notify data plane link is going down */
	if (dp_priv->data_plane_ops->link_state(dp_priv->dpc, 0)) {
		netdev_dbg(netdev, "Data plane set link failed\n");
		return -EAGAIN;
	}

#ifdef CONFIG_PHYLINK
	if (dp_priv->phylink_en && dp_priv->phylink) {
		phylink_stop(dp_priv->phylink);
	} else
#endif
	{
		netif_carrier_off(netdev);
		if (dp_priv->phydev)
			phy_stop(dp_priv->phydev);
	}
	dp_priv->link_state = __NSS_DP_LINK_DOWN;

#if defined(NSS_DP_PPE_SUPPORT)
	/*
	 * Notify data plane to unassign VSI.
	 * This is applicable only for dataplane override mode,
	 * where NPU is expected to manage VSI assigns/un-assigns for ports.
	 */
	if (dp_priv->data_plane_ops->vsi_unassign) {
		if (dp_priv->data_plane_ops->vsi_unassign(dp_priv->dpc,
							dp_priv->vsi)) {
			netdev_err(netdev, "Data plane vsi unassign failed\n");
			return -EAGAIN;
		}
	}
#endif

	/*
	 * Notify GMAC to stop
	 */
	if (dp_priv->gmac_hal_ops->stop) {
		if (dp_priv->gmac_hal_ops->stop(dp_priv->gmac_hal_ctx)) {
			netdev_dbg(netdev, "GMAC stop failed\n");
			return -EAGAIN;
		}
	}

	/*
	 * Notify data plane to close
	 */
	if (dp_priv->data_plane_ops->close(dp_priv->dpc)) {
		netdev_dbg(netdev, "Data plane close failed\n");
		return -EAGAIN;
	}

	clear_bit(__NSS_DP_UP, &dp_priv->flags);

	return 0;
}

/*
 * nss_dp_open()
 */
static int nss_dp_open(struct net_device *netdev)
{
	struct nss_dp_dev *dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);

	if (!dp_priv)
		return -EINVAL;

	if (!dp_priv->phylink_en)
		netif_carrier_off(netdev);

	/*
	 * Call data plane init if it has not been done yet
	 */
	if (!(dp_priv->drv_flags & NSS_DP_PRIV_FLAG(INIT_DONE))) {
		if (dp_priv->data_plane_ops->init(dp_priv->dpc)) {
			netdev_dbg(netdev, "Data plane init failed\n");
			return -ENOMEM;
		}

		dp_priv->drv_flags |= NSS_DP_PRIV_FLAG(INIT_DONE);
	}

#if defined(NSS_DP_MAC_POLL_SUPPORT)
	/*
	 * Enable SSDK PHY polling task to enable GMACs.
	 */
	if (!dp_global_ctx.enable_polling_task) {
		ssdk_mac_sw_sync_work_start(NSS_DP_EDMA_SWITCH_DEV_ID);
		dp_global_ctx.enable_polling_task = true;
	}
#endif

	/*
	 * Inform the Linux Networking stack about the hardware capability of
	 * checksum offloading and other features. Each data_plane is
	 * responsible to maintain the feature set it supports
	 */
	dp_priv->data_plane_ops->set_features(dp_priv->dpc);

	set_bit(__NSS_DP_UP, &dp_priv->flags);

#if defined(NSS_DP_PPE_SUPPORT)
	/*
	 * Notify data plane to assign VSI if required.
	 * This is applicable only for dataplane override mode,
	 * where NPU is expected to manage VSI assigns/un-assigns
	 * based on port status and network configuration.
	 */
	if (dp_priv->data_plane_ops->vsi_assign) {
		if (dp_priv->data_plane_ops->vsi_assign(dp_priv->dpc,
							dp_priv->vsi)) {
			netdev_err(netdev, "Data plane vsi assign failed\n");
			return -EAGAIN;
		}
	}
#endif

	if (dp_priv->data_plane_ops->mac_addr(dp_priv->dpc,
				(unsigned char *)netdev->dev_addr)) {
		netdev_dbg(netdev, "Data plane set MAC address failed\n");
		return -EAGAIN;
	}

	if (dp_priv->data_plane_ops->change_mtu(dp_priv->dpc, netdev->mtu)) {
		netdev_dbg(netdev, "Data plane change mtu failed\n");
		return -EAGAIN;
	}

	if (dp_priv->data_plane_ops->open(dp_priv->dpc, 0, 0, 0)) {
		netdev_dbg(netdev, "Data plane open failed\n");
		return -EAGAIN;
	}

	/*
	 * Notify GMAC to start receive/transmit
	 */
	if (dp_priv->gmac_hal_ops->start) {
		if (dp_priv->gmac_hal_ops->start(dp_priv->gmac_hal_ctx)) {
			netdev_dbg(netdev, "GMAC start failed\n");
			return -EAGAIN;
		}
	}

	netif_start_queue(netdev);

#ifdef CONFIG_PHYLINK
	if (dp_priv->phylink_en && dp_priv->phylink) {
		phylink_start(dp_priv->phylink);
	} else
#endif
	if (!dp_priv->link_poll) {
		/* Notify data plane link is up */
		if (dp_priv->data_plane_ops->link_state(dp_priv->dpc, 1)) {
			netdev_dbg(netdev, "Data plane set link failed\n");
			return -EAGAIN;
		}
		dp_priv->link_state = __NSS_DP_LINK_UP;
		netif_carrier_on(netdev);
	} else {
		dp_priv->link_state = __NSS_DP_LINK_DOWN;
		phy_start(dp_priv->phydev);
		phy_start_aneg(dp_priv->phydev);
	}

	return 0;
}

#ifdef CONFIG_RFS_ACCEL
/*
 * nss_dp_rx_flow_steer()
 *	Steer the flow rule to NSS
 */
static int nss_dp_rx_flow_steer(struct net_device *netdev, const struct sk_buff *_skb,
				uint16_t rxq, uint32_t flow)
{
	struct nss_dp_dev *dp_priv;
	struct netdev_rx_queue *rxqueue;
	struct rps_sock_flow_table *sock_flow_table;
	struct rps_dev_flow_table *flow_table;
	struct rps_dev_flow *rxflow;
	struct sk_buff *skb = (struct sk_buff *)_skb;
	uint16_t index;
	uint32_t hash;
	uint32_t rfscpu;
	uint32_t rxcpu;

	if (!netdev)
		return -EINVAL;

	dp_priv = (struct nss_dp_dev *)netdev_priv(netdev);
	if (!dp_priv)
		return -EINVAL;

	rxqueue = netdev->_rx;

	if (skb_rx_queue_recorded(skb)) {
		index = skb_get_rx_queue(skb);
		rxqueue += index;
	}

	flow_table = rcu_dereference(rxqueue->rps_flow_table);
	if (!flow_table) {
		netdev_dbg(netdev, "RX queue RPS flow table not found\n");
		return -EINVAL;
	}

	hash = skb_get_hash(skb);
	rxflow = &flow_table->flows[hash & flow_table->mask];
	rxcpu = (uint32_t)rxflow->cpu;

	sock_flow_table = rcu_dereference(rps_sock_flow_table);
	if (!sock_flow_table) {
		netdev_dbg(netdev, "Global RPS flow table not found\n");
		return -EINVAL;
	}

	rfscpu = sock_flow_table->ents[hash & sock_flow_table->mask];
	rfscpu &= rps_cpu_mask;

	if (rxcpu == rfscpu)
		return 0;

	/*
	 * check rx_flow_steer is defined in data plane ops
	 */
	if (!dp_priv->data_plane_ops->rx_flow_steer) {
		netdev_dbg(netdev, "Data plane ops not defined for flow steer\n");
		return -EINVAL;
	}

	/*
	 * Delete the old flow rule
	 */
	if (dp_priv->data_plane_ops->rx_flow_steer(dp_priv->dpc, skb, rxcpu, false)) {
		netdev_dbg(netdev, "Data plane delete flow rule failed\n");
		return -EAGAIN;
	}

	/*
	 * Add the new flow rule
	 */
	if (dp_priv->data_plane_ops->rx_flow_steer(dp_priv->dpc, skb, rfscpu, true)) {
		netdev_dbg(netdev, "Data plane add flow rule failed\n");
		return -EAGAIN;
	}

	return 0;
}
#endif

/*
 * nss_dp_select_queue()
 *	Select tx queue
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
static u16 __attribute__((unused)) nss_dp_select_queue(struct net_device *netdev, struct sk_buff *skb,
				void *accel_priv, select_queue_fallback_t fallback)
#else
static u16 __attribute__((unused)) nss_dp_select_queue(struct net_device *netdev, struct sk_buff *skb,
				struct net_device *sb_dev)
#endif
{
	int cpu = get_cpu();
	put_cpu();

	/*
	 * The number of queue is matching the number of CPUs so get_cpu will
	 * always match a valid queue
	 */
	return cpu;
}

/*
 * nss_dp_features_set()
 *	DP feature set ops
 */
static int nss_dp_features_set(struct net_device *dev,
                             netdev_features_t features)
{
#if defined(NSS_DP_HW_GRO)
        netdev_features_t changed = dev->features ^ features;
	if (changed & NETIF_F_GRO_HW) {
		ppe_drv_hw_gro_feature_set(dev, !!(features & NETIF_F_GRO_HW));
	}
#endif

	return 0;
}

static netdev_features_t __attribute__((unused)) nss_dp_feature_check(struct sk_buff *skb,
									struct net_device *dev,
									netdev_features_t features)
{
#ifdef NSS_DP_IPQ50XX
	/*
	 * IPQ50XX does not support HW checksum of double vlan tagged packets.
	 * Disable the feature at runtime during feature check.
	 */
	if (skb_vlan_tagged_multi(skb)) {
		features &= ~(NETIF_F_HW_CSUM | NETIF_F_TSO | NETIF_F_TSO6);
	}
#endif

#ifdef NSS_DP_HW_GRO
	features |= NETIF_F_GRO_HW;
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 6, 0))
	/*
	 * EDMA driver does not support custom checksum offload where
	 * we need to calculate IP style checksum from skb->csum_start
	 * to the end of the packet and update the offset skb->csum_offset.
	 *
	 * GRE CSUM offload feature was added to kernel 6.6 and since we
	 * advertise that edma can support custom checksum offload (NETIF_F_HW_CSUM)
	 * instead of just generic IPv4(NETIF_F_IP_CSUM) and IPv6(NETIF_F_IPV6_CSUM)
	 * GRE module in linux does not calculate the checksum.
	 * Disalbe checksum offload for GRE tunnels.
	 *
	 * TODO: Explore implementing custom tunnel checksum offload support in edma_tx.
	 * EDMA hardware supports it.
	 */
	if (unlikely(skb->csum_offset == NSS_DP_GRE_CSUM_OFFSET)) {
		features &= (~NETIF_F_HW_CSUM | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
	}
#endif
	return features;
}

/*
 * Netdevice operations
 */
struct net_device_ops nss_dp_netdev_ops = {
	.ndo_open = nss_dp_open,
	.ndo_stop = nss_dp_close,
	.ndo_start_xmit = nss_dp_xmit,
	.ndo_get_stats64 = nss_dp_get_stats64,
	.ndo_set_mac_address = nss_dp_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_change_mtu = nss_dp_change_mtu,
	.ndo_do_ioctl = nss_dp_do_ioctl,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	.ndo_eth_ioctl = nss_dp_eth_ioctl,
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	.ndo_bridge_setlink = switchdev_port_bridge_setlink,
	.ndo_bridge_getlink = switchdev_port_bridge_getlink,
	.ndo_bridge_dellink = switchdev_port_bridge_dellink,
#endif

	.ndo_features_check = nss_dp_feature_check,
	.ndo_set_features = nss_dp_features_set,
#ifndef NSS_DP_IPQ50XX
	.ndo_select_queue = nss_dp_select_queue,
#endif

#ifdef CONFIG_RFS_ACCEL
	.ndo_rx_flow_steer = nss_dp_rx_flow_steer,
#endif
};

/*
 * nss_dp_of_get_pdata()
 */
static int32_t nss_dp_of_get_pdata(struct device_node *np,
				   struct net_device *netdev,
				   struct nss_gmac_hal_platform_data *hal_pdata)
{
	uint8_t *maddr;
	uint32_t val = 0;
	struct nss_dp_dev *dp_priv;
	struct resource memres_devtree = {0};
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0))
	uint8_t mac_addr[ETH_ALEN];
#endif

	dp_priv = netdev_priv(netdev);

	if (of_property_read_u32(np, "qcom,id", &dp_priv->macid)) {
		pr_err("%s: error reading id\n", np->name);
		return -EFAULT;
	}

	if (dp_priv->macid > NSS_DP_HAL_MAX_PORTS || !dp_priv->macid) {
		pr_err("%s: invalid macid %d\n", np->name, dp_priv->macid);
		return -EFAULT;
	}

	if (of_property_read_u32(np, "qcom,mactype", &hal_pdata->mactype)) {
		pr_err("%s: error reading mactype\n", np->name);
		return -EFAULT;
	}

	if (of_address_to_resource(np, 0, &memres_devtree) != 0)
		return -EFAULT;

	netdev->base_addr = memres_devtree.start;
	hal_pdata->reg_len = resource_size(&memres_devtree);
	hal_pdata->netdev = netdev;
	hal_pdata->macid = dp_priv->macid;

	if (of_get_phy_mode(np, &dp_priv->phy_mii_type))
		dp_priv->phylink_en = true;

#if (!defined(NSS_DP_IPQ96XX) && !defined(NSS_DP_IPQ52XX))
	dp_priv->link_poll = of_property_read_bool(np, "qcom,link-poll");
#else
	/*
	 * TO-DO: Remove this WAR
	 */
	dp_priv->link_poll = 0;
#endif

	if (of_property_read_u32(np, "qcom,phy-mdio-addr",
		&dp_priv->phy_mdio_addr) && dp_priv->link_poll) {
#ifdef NSS_DP_EDMA_I2C_BUS_ENABLE
		dp_priv->phy_mdio_addr = FAL_SFP_PHY_ADDR;
#else
		pr_err("%s: mdio addr required if link polling is enabled\n",
				np->name);
		return -EFAULT;
#endif
	}

	of_property_read_u32(np, "qcom,forced-speed", &dp_priv->forced_speed);
	of_property_read_u32(np, "qcom,forced-duplex", &dp_priv->forced_duplex);


#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	maddr = (uint8_t *)of_get_mac_address(np);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 4, 0))
	if (IS_ERR((void *)maddr)) {
		maddr = NULL;
	}
#endif
#else
	maddr = mac_addr;
	if (of_get_mac_address(np, maddr))
		maddr = NULL;
#endif /* LINUX_VERSION_CODE 6.1.0 */

	if (maddr && is_valid_ether_addr(maddr)) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
		ether_addr_copy(netdev->dev_addr, (const uint8_t *)maddr);
#else
		eth_hw_addr_set(netdev, maddr);
#endif

	} else {
		eth_hw_addr_random(netdev);
		pr_info("GMAC%d(%px) Invalid MAC@ - using %pM\n", dp_priv->macid,
						dp_priv, netdev->dev_addr);
	}

	if (edma_gbl_ctx->mem_profile & NSS_DP_MEM_PROFILE_OPTIMIZED) {
		/*
		 * Optimized profile disables jumbo & page mode
		 */
		dp_priv->rx_page_mode = false;
		dp_priv->rx_jumbo_mru = 0;
	} else if (edma_gbl_ctx->mem_profile & NSS_DP_MEM_PROFILE_BALANCED) {
		/*
		 * Balanced profile disables page mode and limits jumbo
		 */
		dp_priv->rx_page_mode = false;
		dp_priv->rx_jumbo_mru = jumbo_mru ? JUMBO_MRU_3K : 0;
	} else {
   		u32 __dt_page_mode = 0;
		/*
		 * Load DT page mode and check overwrite settings
		 */
		of_property_read_u32(np, "qcom,rx-page-mode", &__dt_page_mode);
		__dt_page_mode = overwrite_mode ? page_mode : __dt_page_mode;

		/*
		 * Jumbo MRU disables page mode
		 */
		dp_priv->rx_page_mode = jumbo_mru ? false : !!__dt_page_mode;
		dp_priv->rx_jumbo_mru = jumbo_mru ? jumbo_mru : 0;
	}

	pr_info("Page mode = %d, Jumbo MRU = %d\n", dp_priv->rx_page_mode, dp_priv->rx_jumbo_mru);

	of_property_read_u32(np, "qcom,ppe-offload-disabled", &val);
	dp_priv->ppe_offload_disabled = !!val;
	pr_debug("%s: ppe offload disabled: %d for macid %d\n", np->name,
				dp_priv->ppe_offload_disabled, dp_priv->macid);
#ifdef NSS_DP_PON_SUPPORT
	dp_priv->gem_port = of_property_read_bool(np, "qcom,gem_port");
	pr_debug("%s: gem port: %d for macid %d\n", np->name,
			dp_priv->gem_port, dp_priv->macid);
#endif
	dp_priv->is_switch_connected = of_property_read_bool(np, "qcom,is_switch_connected");
	pr_debug("%s: Switch attached to macid %d status: %d\n", np->name, dp_priv->macid, dp_priv->is_switch_connected);

	/*
	 * Read switch dev when MHT switch port to txring mapping is enabled.
	 */
#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (dp_global_ctx.is_mht_dev)
		dp_priv->nss_dp_mht_dev = of_property_read_bool(np, "qcom,mht-dev");
#endif
#ifdef NSS_DP_HTT_SW_PORT_MAP
	dp_priv->nss_dp_htt_dev = of_property_read_bool(np, "qcom,htt-dev");
#endif

	return 0;
}

#if (!defined(NSS_DP_IPQ96XX) && !defined(NSS_DP_IPQ52XX))
/*
 * nss_dp_mdio_attach()
 */
static struct mii_bus *nss_dp_mdio_attach(struct platform_device *pdev)
{
	struct device_node *mdio_node;
#ifdef NSS_DP_EDMA_I2C_BUS_ENABLE
	struct device_node *i2c_node;
#endif
	struct platform_device *mdio_plat;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0))
	struct ipq40xx_mdio_data *mdio_data;
#endif

#ifdef NSS_DP_EDMA_I2C_BUS_ENABLE
	/*
	 * Find mii_bus using "i2c-bus" handle.
	 */
	i2c_node = of_parse_phandle(pdev->dev.of_node, "i2c-bus", 0);
	if (i2c_node) {
		return of_mdio_find_bus(i2c_node);
	}
#endif

	/*
	 * Find mii_bus using "mdio-bus" handle.
	 */
	mdio_node = of_parse_phandle(pdev->dev.of_node, "mdio-bus", 0);
	if (mdio_node) {
		return of_mdio_find_bus(mdio_node);
	}

	mdio_node = of_find_compatible_node(NULL, NULL, "qcom,qca-mdio");
	if (!mdio_node) {
		mdio_node = of_find_compatible_node(NULL, NULL,
							"qcom,ipq40xx-mdio");
		if (!mdio_node) {
			dev_err(&pdev->dev, "cannot find mdio node by phandle\n");
			return NULL;
		}
	}

	mdio_plat = of_find_device_by_node(mdio_node);
	if (!mdio_plat) {
		dev_err(&pdev->dev, "cannot find platform device from mdio node\n");
		of_node_put(mdio_node);
		return NULL;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
	return dev_get_drvdata(&mdio_plat->dev);
#else
	mdio_data = dev_get_drvdata(&mdio_plat->dev);
	if (!mdio_data) {
		dev_err(&pdev->dev, "cannot get mii bus reference from device data\n");
		of_node_put(mdio_node);
		return NULL;
	}

	return mdio_data->mii_bus;
#endif
}

#endif

#ifdef CONFIG_NET_SWITCHDEV
/*
 * nss_dp_is_phy_dev()
 *	Check if it is dp device
 */
bool nss_dp_is_phy_dev(struct net_device *dev)
{
	return (dev->netdev_ops == &nss_dp_netdev_ops);
}
#endif

/*
 * nss_dp_adjust_link()
 */
void nss_dp_adjust_link(struct net_device *netdev)
{
	struct nss_dp_dev *dp_priv = netdev_priv(netdev);
	int current_state = dp_priv->link_state;

	if (!test_bit(__NSS_DP_UP, &dp_priv->flags))
		return;

	if (dp_priv->phydev->link && (current_state == __NSS_DP_LINK_UP))
		return;

	if (!dp_priv->phydev->link && (current_state == __NSS_DP_LINK_DOWN))
		return;

	if (current_state == __NSS_DP_LINK_DOWN) {
		netdev_info(netdev, "PHY Link up speed: %d\n",
						dp_priv->phydev->speed);
		if (dp_priv->data_plane_ops->link_state(dp_priv->dpc, 1)) {
			netdev_dbg(netdev, "Data plane set link up failed\n");
			return;
		}
		dp_priv->link_state = __NSS_DP_LINK_UP;
		netif_carrier_on(netdev);
	} else {
		netdev_info(netdev, "PHY Link is down\n");
		if (dp_priv->data_plane_ops->link_state(dp_priv->dpc, 0)) {
			netdev_dbg(netdev, "Data plane set link down failed\n");
			return;
		}
		dp_priv->link_state = __NSS_DP_LINK_DOWN;
		netif_carrier_off(netdev);
	}
}

/*
 * nss_dp_probe()
 */
static int32_t nss_dp_probe(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct nss_dp_dev *dp_priv;
	struct device_node *np = pdev->dev.of_node;
	struct nss_gmac_hal_platform_data gmac_hal_pdata;
	int32_t ret = 0;
#if (!defined(NSS_DP_IPQ96XX) && !defined(NSS_DP_IPQ52XX))
	uint8_t phy_id[MII_BUS_ID_SIZE + 3];
#endif
#if defined(NSS_DP_PPE_SUPPORT)
	uint32_t vsi_id;
	fal_port_t port_id;
#endif

	/* TODO: See if we need to do some SoC level common init */

	netdev = alloc_etherdev_mqs(sizeof(struct nss_dp_dev),
			NSS_DP_NETDEV_TX_QUEUE_NUM, NSS_DP_NETDEV_RX_QUEUE_NUM);
	if (!netdev) {
		pr_info("alloc_etherdev() failed\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	/* max_mtu is set to 1500 in ether_setup() */
	netdev->max_mtu = ETH_MAX_MTU;
#endif

	dp_priv = netdev_priv(netdev);
	memset((void *)dp_priv, 0, sizeof(struct nss_dp_dev));

	dp_priv->pdev = pdev;
	dp_priv->netdev = netdev;
	netdev->watchdog_timeo = 5 * HZ;
	netdev->priv_flags |= IFF_LIVE_ADDR_CHANGE;
	netdev->netdev_ops = &nss_dp_netdev_ops;
	netdev->gso_max_segs = NSS_DP_GSO_MAX_SEGS;
	nss_dp_set_ethtool_ops(netdev);
#ifdef CONFIG_NET_SWITCHDEV
	nss_dp_switchdev_setup(netdev);
#endif

	nss_dp_hal_init_soc_priv_flags(dp_priv);

	ret = nss_dp_of_get_pdata(np, netdev, &gmac_hal_pdata);
	if (ret != 0) {
		goto fail;
	}

	/* Use data plane ops as per the configured SoC */
	dp_priv->data_plane_ops = nss_dp_hal_get_data_plane_ops();
	if (!dp_priv->data_plane_ops) {
		netdev_dbg(netdev, "Dataplane ops not found.\n");
		goto fail;
	}

	dp_priv->dpc = &dp_global_data_plane_ctx[nss_dp_get_idx_from_macid(dp_priv->macid)];
	dp_priv->dpc->dev = netdev;
	dp_priv->ctx = &dp_global_ctx;

	/* TODO:locks init */

	/*
	 * HAL's init function will return the pointer to the HAL context
	 * (private to hal), which dp will store in its data structures.
	 * The subsequent hal_ops calls expect the DP to pass the HAL
	 * context pointer as an argument
	 */
	dp_priv->gmac_hal_ops = nss_dp_hal_get_gmac_ops(gmac_hal_pdata.mactype);
	if (!dp_priv->gmac_hal_ops) {
		netdev_dbg(netdev, "Unsupported Mac type: %d\n", gmac_hal_pdata.mactype);
		goto fail;
	}

	dp_priv->gmac_hal_ctx = dp_priv->gmac_hal_ops->init(&gmac_hal_pdata);
	if (!(dp_priv->gmac_hal_ctx)) {
		netdev_dbg(netdev, "GMAC hal init failed\n");
		goto fail;
	}

	if (dp_priv->data_plane_ops->init(dp_priv->dpc)) {
		netdev_dbg(netdev, "Data plane init failed\n");
		goto data_plane_init_fail;
	}

	dp_priv->drv_flags |= NSS_DP_PRIV_FLAG(INIT_DONE);

	/* Register the network interface */
	ret = register_netdev(netdev);
	if (ret) {
		netdev_dbg(netdev, "Error registering netdevice %s\n",
				netdev->name);
		goto netdev_register_fail;
	}

#ifdef CONFIG_PHYLINK
	if (dp_priv->phylink_en) {
		dp_priv->phylink = ssdk_port_phylink_setup(0, dp_priv->macid, netdev);
		if (!dp_priv->phylink) {
			netdev_warn(netdev, "Phylink setup failed, falling back to legacy PHY\n");
			dp_priv->phylink_en = false;
		}
	}
#endif

#if (!defined(NSS_DP_IPQ96XX) && !defined(NSS_DP_IPQ52XX))
	if (!dp_priv->phylink_en && dp_priv->link_poll) {
		dp_priv->miibus = nss_dp_mdio_attach(pdev);
		if (!dp_priv->miibus) {
			netdev_dbg(netdev, "failed to find miibus\n");
			goto phy_setup_fail;
		}
		snprintf(phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT,
				dp_priv->miibus->id, dp_priv->phy_mdio_addr);
		dp_priv->phydev = phy_connect(netdev, phy_id,
				&nss_dp_adjust_link,
				dp_priv->phy_mii_type);
		if (IS_ERR(dp_priv->phydev)) {
			netdev_dbg(netdev, "failed to connect to phy device\n");
			goto phy_setup_fail;
		}
	}
#endif

#if defined(NSS_DP_PPE_SUPPORT)
	/* Get port's default VSI */
	port_id = dp_priv->macid;
	if (ppe_port_vsi_get(0, port_id, &vsi_id)) {
		netdev_dbg(netdev, "failed to get port's default VSI\n");
		goto vsi_set_fail;
	}

	dp_priv->vsi = vsi_id;

	/*
	 * Assign the VSI to the port.
	 */
	if (fal_port_vsi_set(0, port_id, vsi_id) < 0) {
		netdev_dbg(netdev, "Data plane vsi assign failed\n");
		goto vsi_set_fail;
	}
#endif

	/* TODO: Features: CSUM, tx/rx offload... configure */

	dp_global_ctx.nss_dp[nss_dp_get_idx_from_macid(dp_priv->macid)] = dp_priv;
	dp_global_ctx.slowproto_acl_bm = 0;

	netdev_dbg(netdev, "Init NSS DP GMAC%d (base = 0x%lx)\n", dp_priv->macid, netdev->base_addr);

#ifdef CONFIG_SKB_TIMESTAMP
	/*
	 * Validate the Latency port's value and fill up the GMAC timer register addresses
	 */
	if ((nss_dp_tstamp_port_id <= 0) || (nss_dp_tstamp_port_id > NSS_DP_HAL_MAX_PORTS)) {
		nss_dp_tstamp_port_id = NSS_DP_EDMA_DEF_TSTAMP_PORT;
	}

	if ((dp_priv->macid == nss_dp_tstamp_port_id) && gmac_hal_pdata.mactype) {
		phys_addr_t sec_addr = NSS_DP_GMAC_TS_ADDR_SEC(netdev->base_addr);
		phys_addr_t nsec_addr = NSS_DP_GMAC_TS_ADDR_NSEC(netdev->base_addr);

		edma_gbl_ctx->tstamp_sec = ioremap_nocache(sec_addr, sizeof(uint32_t));
		edma_gbl_ctx->tstamp_nsec = ioremap_nocache(nsec_addr, sizeof(uint32_t));
		if (unlikely(!edma_gbl_ctx->tstamp_sec || !edma_gbl_ctx->tstamp_nsec)) {
			pr_err("Unable to map the timestamp registers, sec addr:0x%llx,"
					" nsec addr: 0x%llx\n", sec_addr, nsec_addr);
			return 0;
		}
		pr_info("tstamp_sec: 0x%llx, tstamp_nsec: 0x%llx\n", sec_addr, nsec_addr);
	}
#endif
	return 0;

#if defined(NSS_DP_PPE_SUPPORT)
vsi_set_fail:
	if (dp_priv->phydev) {
		phy_disconnect(dp_priv->phydev);
		dp_priv->phydev = NULL;
	}

	if (dp_priv->phylink) {
		ssdk_port_phylink_destroy(0, dp_priv->macid);
		dp_priv->phylink = NULL;
	}
#endif

#if (!defined(NSS_DP_IPQ96XX) && !defined(NSS_DP_IPQ52XX))
phy_setup_fail:
#endif
	unregister_netdev(netdev);
netdev_register_fail:
	dp_priv->data_plane_ops->deinit(dp_priv->dpc);
data_plane_init_fail:
	dp_priv->gmac_hal_ops->exit(dp_priv->gmac_hal_ctx);
fail:
	free_netdev(netdev);
	return -EFAULT;
}

/*
 * nss_dp_remove()
 *	Remove a dataplane port
 *
 * Note: We only remove the physical ports here. Virtual
 * port devices are removed explicitly by the VP module.
 */
static int nss_dp_remove(struct platform_device *pdev)
{
	uint32_t i;
	struct nss_dp_dev *dp_priv;
	struct nss_gmac_hal_ops *hal_ops;
	struct nss_dp_data_plane_ops *dp_ops;

	for (i = 0; i < NSS_DP_HAL_MAX_PORTS; i++) {
		dp_priv = dp_global_ctx.nss_dp[i];
		if (!dp_priv)
			continue;

		dp_ops = dp_priv->data_plane_ops;
		hal_ops = dp_priv->gmac_hal_ops;

#ifdef CONFIG_PHYLINK
		if (dp_priv->phylink_en && dp_priv->phylink) {
			ssdk_port_phylink_destroy(0, dp_priv->macid);
			dp_priv->phylink = NULL;
		} else
#endif
		{
			if (dp_priv->phydev) {
				phy_disconnect(dp_priv->phydev);
				dp_priv->phydev = NULL;
			}
		}

#if defined(NSS_DP_PPE_SUPPORT)
		/*
		 * Unassign the port's VSI.
		 */
		fal_port_vsi_set(0, dp_priv->macid, 0xFFFF);
#endif
		dp_ops->deinit(dp_priv->dpc);

#ifdef CONFIG_NET_SWITCHDEV
		nss_dp_switchdev_cleanup(dp_priv->netdev);
#endif

		nss_dp_hal_deinit_soc_priv_flags(dp_priv);

		/*
		 * Execution of unregister_netdev may access statistics of the
		 * netdevice, which leads to the netdev op accessing netdev
		 * priv's HAL data. So we ensure that netdev is unregistered
		 * before performing hal_ops exit().
		 */
		unregister_netdev(dp_priv->netdev);
		hal_ops->exit(dp_priv->gmac_hal_ctx);
		dp_priv->gmac_hal_ctx = NULL;
		free_netdev(dp_priv->netdev);
		dp_global_ctx.nss_dp[i] = NULL;
	}

	return 0;
}

static struct of_device_id nss_dp_dt_ids[] = {
	{ .compatible = "qcom,nss-dp" },
	{},
};
MODULE_DEVICE_TABLE(of, nss_dp_dt_ids);

static struct platform_driver nss_dp_drv = {
	.probe = nss_dp_probe,
	.remove = nss_dp_remove,
	.driver = {
		   .name = "nss-dp",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(nss_dp_dt_ids),
		  },
};

/*
 * nss_dp_is_netdev_physical()
 *	Check the given net device is in DP
 */
bool nss_dp_is_netdev_physical(struct net_device *netdev)
{
	uint32_t i;
	struct nss_dp_dev *dp_priv;

	for (i = 0; i < NSS_DP_HAL_MAX_PORTS; i++) {
		dp_priv = dp_global_ctx.nss_dp[i];
		if (!dp_priv) {
			continue;
		}

		if (dp_priv->netdev == netdev) {
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL(nss_dp_is_netdev_physical);

/*
 * nss_dp_get_port_num()
 *	Return port number for the given netdevice
 */
int32_t nss_dp_get_port_num(struct net_device *netdev)
{
	struct nss_dp_dev *dp_priv;
	int32_t port_id;

	if (!nss_dp_is_netdev_physical(netdev)) {
		port_id = ppe_drv_port_num_from_dev(netdev);
		return (port_id == PPE_DRV_PORT_ID_INVALID) ? NSS_DP_INVALID_INTERFACE : port_id;
	}

	dp_priv = netdev_priv(netdev);
	return dp_priv->macid;
}
EXPORT_SYMBOL(nss_dp_get_port_num);

/*
 * nss_dp_ppeds_get_ops()
 *	API to get PPE-DS operations
 */
struct nss_dp_ppeds_ops *nss_dp_ppeds_get_wifi_arch_mode_ops(uint32_t mode)
{
	return nss_dp_ppeds_wifi_arch_mode_ops_get(mode);
}
EXPORT_SYMBOL(nss_dp_ppeds_get_wifi_arch_mode_ops);

/*
 * nss_dp_nsm_sawf_sc_stats_read()
 *	Send nsm stats for given service class.
 */
bool nss_dp_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class)
{
	return nss_dp_hal_nsm_sawf_sc_stats_read(nsm_stats, service_class);
}
EXPORT_SYMBOL(nss_dp_nsm_sawf_sc_stats_read);

#ifdef CONFIG_IPQ_PON
/*
 * nss_dp_gem_rx_register_cb()
 *	Register handler for GEM rx processing.
 */
bool nss_dp_gem_rx_register_cb(void *app_data, bool (*cb)(void *app_data, struct sk_buff *skb))
{
	rcu_assign_pointer(nss_dp_gem_rx_app_data_g, app_data);
	rcu_assign_pointer(nss_dp_gem_rx_reg_cb_g, cb);
	synchronize_rcu();
	return true;
}
EXPORT_SYMBOL(nss_dp_gem_rx_register_cb);

/*
 * nss_dp_gem_rx_unregister_cb()
 *	Unregister GEM handler for GEM rx processing.
 */
void nss_dp_gem_rx_unregister_cb(void)
{
	rcu_assign_pointer(nss_dp_gem_rx_app_data_g, NULL);
	rcu_assign_pointer(nss_dp_gem_rx_reg_cb_g, NULL);
	synchronize_rcu();
}
EXPORT_SYMBOL(nss_dp_gem_rx_unregister_cb);

/*
 * nss_dp_gem_tx_register_cb()
 *	Register handler for GEM tx processing.
 */
bool nss_dp_gem_tx_register_cb(void *app_data, bool (*cb)(void *app_data, struct sk_buff *skb, struct nss_dp_gem_tx_info *gem_txi))
{
	rcu_assign_pointer(nss_dp_gem_tx_app_data_g, app_data);
	rcu_assign_pointer(nss_dp_gem_tx_reg_cb_g, cb);
	synchronize_rcu();
	return true;
}
EXPORT_SYMBOL(nss_dp_gem_tx_register_cb);

/*
 * nss_dp_gem_tx_unregister_cb()
 *	Unregister GEM handler for GEM tx processing.
 */
void nss_dp_gem_tx_unregister_cb(void)
{
	rcu_assign_pointer(nss_dp_gem_tx_app_data_g, NULL);
	rcu_assign_pointer(nss_dp_gem_tx_reg_cb_g, NULL);
	synchronize_rcu();
}
EXPORT_SYMBOL(nss_dp_gem_tx_unregister_cb);

/*
 * nss_dp_pon_drainout_register_cb()
 *	Register PON TCONT drainout callback for UMAC reset.
 *
 *	Called by the PON driver to register pon_tcf_bwmrpt_drainout_tcont_by_bitmap
 *	so that qca-nss-dp can invoke it during UMAC reset without a hard
 *	module dependency.
 */
void nss_dp_pon_drainout_register_cb(nss_dp_pon_drainout_cb_t cb)
{
	rcu_assign_pointer(nss_dp_pon_drainout_reg_cb_g, cb);
	synchronize_rcu();
}
EXPORT_SYMBOL(nss_dp_pon_drainout_register_cb);

/*
 * nss_dp_pon_drainout_unregister_cb()
 *	Unregister PON TCONT drainout callback.
 */
void nss_dp_pon_drainout_unregister_cb(void)
{
	rcu_assign_pointer(nss_dp_pon_drainout_reg_cb_g, NULL);
	synchronize_rcu();
}
EXPORT_SYMBOL(nss_dp_pon_drainout_unregister_cb);

/*
 * nss_dp_pon_drainout()
 *	PON TCONT drainout for UMAC reset.
 *
 *	Calls pon_tcf_bwmrpt_drainout_tcont_by_bitmap() to flush TCF SRAM
 *	and zero PPE TCONT credits for all T-CONTs specified by tcont_bitmap.
 */
int nss_dp_pon_drainout(unsigned int tcont_bitmap, const char *context_str)
{
	nss_dp_pon_drainout_cb_t drainout_cb;

	rcu_read_lock();
	drainout_cb = rcu_dereference(nss_dp_pon_drainout_reg_cb_g);
	if (!drainout_cb) {
		rcu_read_unlock();
		pr_debug("nss_dp: PON drainout callback not registered, skipping\n");
		return 0;
	}
	rcu_read_unlock();
	return drainout_cb(tcont_bitmap, context_str);
}
EXPORT_SYMBOL(nss_dp_pon_drainout);
#endif

/*
 * nss_dp_init()
 */
int __init nss_dp_init(void)
{
	int ret, i;

       edma_gbl_ctx = kzalloc(sizeof(struct edma_gbl_ctx), GFP_KERNEL);
       if(!edma_gbl_ctx) {
               pr_err("Failed to allocate edma global structure.\n");
               return -EINVAL;
       }

	dp_global_ctx.common_init_done = false;

	/*
	 * Detect memory profile from bootargs (or compile-time flag).
	 * Must be called before any profile-dependent configuration.
	 */
	nss_dp_mem_profile_detect(edma_gbl_ctx);

#if defined(NSS_DP_MAC_POLL_SUPPORT)
	dp_global_ctx.enable_polling_task = false;
#endif

	/*
	 * Get the buffer size to allocate
	 */
	dp_global_ctx.rx_buf_size = NSS_DP_RX_BUFFER_SIZE;

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	dp_global_ctx.edma_loopback_ring_size = edma_loopback_ring_size;
	dp_global_ctx.edma_loopback_buffer_size = edma_loopback_buffer_size;
#endif

	/*
	 * Configure tx requeue functionality based on module param
	 */
	dp_global_ctx.tx_requeue_stop = false;
	if (tx_requeue_stop != 0) {
		dp_global_ctx.tx_requeue_stop = true;
	}

	/*
	 * Get the module params.
	 * We do not support page_mode or jumbo_mru on low memory profiles.
	 */
	if (edma_gbl_ctx->mem_profile & NSS_DP_MEM_PROFILE_HIGH) {
		dp_global_ctx.overwrite_mode = overwrite_mode;
		dp_global_ctx.page_mode = page_mode;
		dp_global_ctx.jumbo_mru = jumbo_mru;
	} else if (edma_gbl_ctx->mem_profile & NSS_DP_MEM_PROFILE_BALANCED) {
		dp_global_ctx.jumbo_mru = jumbo_mru;
		if (overwrite_mode || page_mode) {
			pr_err("512M profile does not support page mode\n");
			return -1;
		}
	} else {
		if (overwrite_mode || page_mode || jumbo_mru) {
			pr_err("Optimized profile doesn't support overwrite, page & jumbo mode\n");
			return -1;
		}
	}

	/*
	 * Configure MHT switch ports to tx ring mapping
	 * Default mht ports mapping is disabled.
	 */
#ifdef NSS_DP_MHT_SW_PORT_MAP
	dp_global_ctx.is_mht_dev = false;
	if (nss_dp_mht_multi_txring)
		dp_global_ctx.is_mht_dev = true;
#endif

	/*
	 * Initialize nss dp pointer to NULL
	 */
	for (i = 0; i < NSS_DP_MAX_PORTS; i++) {
		dp_global_ctx.nss_dp[i] = NULL;
	}

	/*
	 * Check platform compatibility and
	 * set GMAC and data_plane ops.
	 */
	if (!nss_dp_hal_init()) {
		pr_err("DP hal init failed.\n");
		return -EFAULT;
	}

	ret = platform_driver_register(&nss_dp_drv);
	if (ret)
		pr_info("NSS DP platform drv register failed\n");

	dp_global_ctx.common_init_done = true;
	pr_info("**********************************************************\n");
	pr_info("* NSS Data Plane driver\n");
	pr_info("**********************************************************\n");

	return ret;
}

/*
 * nss_dp_exit()
 */
void __exit nss_dp_exit(void)
{
	/*
	 * Ensure netdev remove is done before HAL cleanup.
	 */
	platform_driver_unregister(&nss_dp_drv);

	if (dp_global_ctx.common_init_done) {
		nss_dp_hal_cleanup();
		dp_global_ctx.common_init_done = false;
	}
}

module_init(nss_dp_init);
module_exit(nss_dp_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS Data Plane Network Driver");
