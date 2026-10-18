/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _NSS_PHY_LINUX_WRAPPER_H_
#define _NSS_PHY_LINUX_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#include <linux/math64.h>
#include <linux/phy.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>

/* rx_count/tx_count are the since-last-link-up software-accumulated FR
 * counters (reset to 0 on a link-up transition); rx_total/tx_total are
 * never reset. Both are maintained by nss_phy_c45_common_fr_cnt_poll()
 * from the read-clear hardware delta (see struct nss_phy_fr_cnt).
 */
struct nss_phy_fr_sw_cnt {
	u64 rx_count;
	u64 tx_count;
	u64 rx_total;
	u64 tx_total;
};

/* Software link-flap counters + timestamps (seconds since boot) */
struct nss_phy_link_flap_stats {
	atomic64_t up_count;
	atomic64_t down_count;
	atomic64_t last_up_time;	/* ktime_get_seconds() at 0->1; 0 = never */
	atomic64_t last_down_time;	/* ktime_get_seconds() at 1->0; 0 = never */
	atomic64_t last_change_time;	/* ktime_get_seconds() at any transition; 0 = never */
};

struct nss_phy_device {
	struct phy_device *phydev;
	atomic64_t adjust_link_post_count;
	struct nss_phy_link_flap_stats flap_stats;
	/*
	 * Software cache of the Fast Retrain enable state set by the most
	 * recent nss_phy_c45_common_fr_cfg_set() call. MMD1.93 is a
	 * read-clear register (see nss_phy_c45_common_fr_cnt_get()), so
	 * fr_cfg_get()/fr_status_get()/fr_trigger() must derive "is FR
	 * enabled" from this cache instead of reading MMD1.93 again.
	 */
	bool fr_ieee_enabled;
	bool fr_cisco_enabled;
	/*
	 * Software-accumulated FR rx/tx counters, updated once per second by
	 * status_poll_work and read by nss_phy_c45_common_fr_status_get().
	 * fr_sw_cnt_lock protects fr_sw_cnt against that writer/reader race.
	 */
	struct nss_phy_fr_sw_cnt fr_sw_cnt;
	spinlock_t fr_sw_cnt_lock;
	/*
	 * Software-cached autonegotiation failure counter, updated by
	 * status_poll_work on each link state transition by reading the
	 * hardware AN failure count register. Read by
	 * nss_phy_common_an_fail_counter_get(). atomic64_t avoids a
	 * separate lock for this single-writer/many-reader counter.
	 */
	atomic64_t an_fail_count;
	/*
	 * Periodic status poll work — always running while the PHY is bound.
	 * Tracks the previous link state to detect transitions; currently
	 * handles FR counter accumulation only.
	 */
	struct delayed_work status_poll_work;
	bool status_poll_prev_link;
	/*
	 * Serializes the full enable→measure→disable sequence in mse_get,
	 * including pma_monitor_set() for the C45 path, so that a concurrent
	 * caller cannot interleave its own enable/disable with an in-progress
	 * measurement.
	 */
	struct mutex mse_lock;
	/*
	 * Serializes the full ms-swap→measure→restore sequence in
	 * clk_ppm_offset_get so that concurrent callers cannot interleave
	 * their save/restore of the master/slave configuration.
	 */
	struct mutex ppm_lock;
};

#define nss_phy_err(nss_phydev, format, args...)	\
	phydev_err(nss_phydev->phydev, format, ##args)
#define nss_phy_info(nss_phydev, format, args...)	\
	phydev_info(nss_phydev->phydev, format, ##args)
#define nss_phy_warn(nss_phydev, format, args...)	\
	phydev_warn(nss_phydev->phydev, format, ##args)
#define nss_phy_dbg(nss_phydev, format, args...)	\
	phydev_dbg(nss_phydev->phydev, format, ##args)
#define nss_phy_pr_info		pr_info

#define NSS_PHY_EOPNOTSUPP		EOPNOTSUPP
#define NSS_PHY_EINVAL		EINVAL
#define NSS_PHY_ENOSPC		ENOSPC
#define NSS_PHY_ETIMEOUT		ETIMEDOUT
#define NSS_PHY_ENOLINK		ENOLINK

#define NSS_PHY_INTERFACE_MODE_RGMII_AMDET		12
#define nss_phy_interface_t phy_interface_t
#define NSS_NSS_PHY_INTERFACE_MODE_NA		\
	PHY_INTERFACE_MODE_NA
#define NSS_PHY_INTERFACE_MODE_SGMII		\
	PHY_INTERFACE_MODE_SGMII
#define NSS_PHY_INTERFACE_MODE_PSGMII		\
	PHY_INTERFACE_MODE_PSGMII
#define NSS_PHY_INTERFACE_MODE_QSGMII		\
	PHY_INTERFACE_MODE_QSGMII
#define NSS_PHY_INTERFACE_MODE_100BASEX		\
	PHY_INTERFACE_MODE_100BASEX
#define NSS_PHY_INTERFACE_MODE_1000BASEX		\
	PHY_INTERFACE_MODE_1000BASEX
#define NSS_PHY_INTERFACE_MODE_2500BASEX		\
	PHY_INTERFACE_MODE_2500BASEX
#define NSS_PHY_INTERFACE_MODE_USXGMII		\
	PHY_INTERFACE_MODE_USXGMII
#define NSS_PHY_INTERFACE_MODE_QUSGMII		\
	PHY_INTERFACE_MODE_QUSGMII
#define NSS_PHY_INTERFACE_MODE_RGMII		\
	PHY_INTERFACE_MODE_RGMII
#define NSS_PHY_INTERFACE_MODE_MAX		\
	PHY_INTERFACE_MODE_MAX

#define NSS_PHY_SPEED_10		SPEED_10
#define NSS_PHY_SPEED_100		SPEED_100
#define NSS_PHY_SPEED_1000		SPEED_1000
#define NSS_PHY_SPEED_2500		SPEED_2500
#define NSS_PHY_SPEED_5000		SPEED_5000
#define NSS_PHY_SPEED_10000		SPEED_10000

#define nss_phy_pinctrl_map_type	pinctrl_map_type
#define NSS_PHY_PIN_MAP_TYPE_INVALID	PIN_MAP_TYPE_INVALID
#define NSS_PHY_PIN_MAP_TYPE_DUMMY_STATED	PIN_MAP_TYPE_DUMMY_STATE
#define NSS_PHY_PIN_MAP_TYPE_MUX_GROUP	PIN_MAP_TYPE_MUX_GROUP
#define NSS_PHY_PIN_MAP_TYPE_CONFIGS_PIN	PIN_MAP_TYPE_CONFIGS_PIN
#define NSS_PHY_PIN_MAP_TYPE_CONFIGS_GROUP	PIN_MAP_TYPE_CONFIGS_GROUP

#define nss_phy_pin_config_param	pin_config_param
#define NSS_PHY_PIN_CONFIG_BIAS_BUS_HOLD	PIN_CONFIG_BIAS_BUS_HOLD
#define NSS_PHY_PIN_CONFIG_BIAS_DISABLE	PIN_CONFIG_BIAS_DISABLE
#define NSS_PHY_PIN_CONFIG_BIAS_PULL_DOWN	PIN_CONFIG_BIAS_PULL_DOWN
#define NSS_PHY_PIN_CONFIG_BIAS_PULL_UP	PIN_CONFIG_BIAS_PULL_UP
#define NSS_PHY_PIN_CONFIG_DRIVE_STRENGTH	PIN_CONFIG_DRIVE_STRENGTH
#define NSS_PHY_PIN_CONFIG_OUTPUT_ENABLE	PIN_CONFIG_OUTPUT_ENABLE
#define NSS_PHY_PIN_CONFIG_INPUT_ENABLE	PIN_CONFIG_INPUT_ENABLE
#define NSS_PHY_PIN_CONFIG_OUTPUT	PIN_CONFIG_OUTPUT

#define nss_phy_pinconf_to_config_param	pinconf_to_config_param
#define nss_phy_pinconf_to_config_argument	pinconf_to_config_argument

struct nss_phy_linux_mdio_data {
	void __iomem *membase[2];
	void __iomem *eth_ldo_rdy[3];
	int clk_div;
	bool force_c22;
	struct gpio_descs *reset_gpios;
	void (*preinit)(struct mii_bus *bus);
	u32 (*sw_read)(struct mii_bus *bus, u32 reg);
	void (*sw_write)(struct mii_bus *bus, u32 reg, u32 val);
	struct clk *clk[];
};

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _NSS_PHY_LINUX_WRAPPER_H_ */
