/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

 #ifndef _SSDK_DTS_H_
#define _SSDK_DTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/version.h>
#include "ssdk_init.h"
#include <linux/phy.h>

typedef struct {
	a_uint16_t ucastq_start;
	a_uint16_t ucastq_end;
	a_uint16_t mcastq_start;
	a_uint16_t mcastq_end;
	a_uint8_t l0sp_start;
	a_uint8_t l0sp_end;
	a_uint8_t l0cdrr_start;
	a_uint8_t l0cdrr_end;
	a_uint8_t l0edrr_start;
	a_uint8_t l0edrr_end;
	a_uint8_t l1sp_start;
	a_uint8_t l1sp_end;
	a_uint8_t l1cdrr_start;
	a_uint8_t l1cdrr_end;
	a_uint8_t l1edrr_start;
	a_uint8_t l1edrr_end;
	a_uint8_t max_pri;
} ssdk_dt_portscheduler_cfg;

typedef struct {
	a_uint8_t valid;
	a_uint8_t port_id;
	a_uint8_t cpri;
	a_uint8_t cdrr_id;
	a_uint8_t epri;
	a_uint8_t edrr_id;
	a_uint8_t sp_id;
} ssdk_dt_l0scheduler_cfg;

typedef struct {
	a_uint8_t valid;
	a_uint8_t port_id;
	a_uint8_t cpri;
	a_uint8_t cdrr_id;
	a_uint8_t epri;
	a_uint8_t edrr_id;
	a_uint8_t sp_id;
} ssdk_dt_l1scheduler_cfg;

typedef struct {
	ssdk_dt_portscheduler_cfg pool[SSDK_MAX_PORT_NUM];
	ssdk_dt_l0scheduler_cfg l0cfg[SSDK_L0SCHEDULER_CFG_MAX];
	ssdk_dt_l1scheduler_cfg l1cfg[SSDK_L1SCHEDULER_CFG_MAX];
	ssdk_dt_portscheduler_cfg reserved_pool;
} ssdk_dt_scheduler_cfg;

typedef struct
{
	a_uint32_t switchreg_base_addr;
	a_uint32_t switchreg_size;
	a_uint32_t psgmiireg_base_addr;
	a_uint32_t psgmiireg_size;
	a_uint8_t *reg_access_mode;
	a_uint8_t *psgmii_reg_access_str;
	hsl_reg_mode switch_reg_access_mode;
	hsl_reg_mode psgmii_reg_access_mode;
	struct clk *ess_clk;
	struct clk *cmnblk_clk;
	ssdk_port_cfg   port_cfg;
	a_uint32_t      mac_mode;
	a_uint32_t      mac_mode1;
	a_uint32_t      mac_mode2;
	a_uint32_t uniphyreg_base_addr;
	a_uint32_t uniphyreg_size;
	a_uint8_t *uniphy_access_mode;
	hsl_reg_mode uniphy_reg_access_mode;
	ssdk_dt_scheduler_cfg scheduler_cfg;
	a_uint8_t bm_tick_mode;
	a_uint8_t tm_tick_mode;
	a_uint32_t device_id;
	struct device_node *of_node;
	a_bool_t is_emulation;
	a_uint32_t emu_chip_ver; /*only valid when is_emulation is true*/
	a_uint32_t clk_mode;
	a_uint32_t pcie_hw_base;
	led_ctrl_pattern_t source_pattern[SSDK_MAX_PORT_NUM][PORT_LED_SOURCE_MAX];
	a_bool_t uniphy_status[SSDK_MAX_UNIPHY_INSTANCE];
	struct mdio_device *clk_mdiodev;
} ssdk_dt_cfg;

#define SSDK_MAX_NR_ETH 6
#define SSDK_PHY_RESET_GPIO_INDEX 0
#define SSDK_NETDEV_SWITCH_NUM 2
typedef struct
{
	struct net_device *dev;
	struct device_node *switch_node;
	a_uint8_t switch_netdev_port;
	a_bool_t switch_connected;
	a_uint8_t switch_dev_id;
	a_uint8_t switch_cpu_port;
	a_uint8_t switch_port_bmp;
	a_uint32_t switch_port_vid[SSDK_MAX_PORT_NUM];
	a_uint32_t switch_athtag;
	a_bool_t switch_erp_standby;
} ssdk_netdev_switch_t;

typedef struct
{
	a_uint32_t num_devices;
	ssdk_dt_cfg **ssdk_dt_switch_nodes;
	a_uint32_t num_intf_mac;
	fal_mac_addr_t intf_mac[SSDK_MAX_NR_ETH];
	ssdk_netdev_switch_t netdev_switch[SSDK_NETDEV_SWITCH_NUM];
} ssdk_dt_global_t;

typedef struct
{
	a_uint32_t base_addr;
	a_uint32_t size;
} ssdk_reg_map_info;

/* DTS info for get */
#ifdef IN_QOS
a_uint8_t ssdk_tm_tick_mode_get(a_uint32_t dev_id);
ssdk_dt_scheduler_cfg* ssdk_bootup_shceduler_cfg_get(a_uint32_t dev_id);
#endif
#ifdef IN_BM
a_uint8_t ssdk_bm_tick_mode_get(a_uint32_t dev_id);
#endif
#ifdef IN_QM
a_uint16_t ssdk_ucast_queue_start_get(a_uint32_t dev_id, a_uint32_t port);
a_uint16_t ssdk_ucast_queue_num_get(a_uint32_t dev_id, a_uint32_t port);
a_uint16_t ssdk_ucast_l0_cdrr_num_get(a_uint32_t dev_id, a_uint32_t port);
a_uint8_t ssdk_port_ucast_max_pri_get(a_uint32_t dev_id, a_uint32_t port);
void ssdk_port_ucast_max_pri_set(a_uint32_t dev_id, a_uint32_t port,
				 a_uint8_t max_pri);
#endif
a_uint32_t ssdk_intf_mac_num_get(void);
a_uint8_t* ssdk_intf_macaddr_get(a_uint32_t index);
a_bool_t ssdk_uniphy_check_by_softsku(a_uint32_t dev_id, a_uint32_t index);
a_uint32_t ssdk_dt_global_get_mac_mode(a_uint32_t dev_id, a_uint32_t index);
a_uint32_t ssdk_dt_get_port_mode(a_uint32_t dev_id, a_uint32_t port_id);
a_uint32_t ssdk_dt_global_set_mac_mode(a_uint32_t dev_id, a_uint32_t index, a_uint32_t mode);
a_uint32_t ssdk_cpu_bmp_get(a_uint32_t dev_id);
a_uint32_t ssdk_lan_bmp_get(a_uint32_t dev_id);
a_uint32_t ssdk_wan_bmp_get(a_uint32_t dev_id);
sw_error_t ssdk_lan_bmp_set(a_uint32_t dev_id, a_uint32_t lan_bmp);
sw_error_t ssdk_wan_bmp_set(a_uint32_t dev_id, a_uint32_t wan_bmp);
a_uint32_t ssdk_inner_bmp_get(a_uint32_t dev_id);
hsl_reg_mode ssdk_switch_reg_access_mode_get(a_uint32_t dev_id);
void ssdk_switch_reg_map_info_get(a_uint32_t dev_id, ssdk_reg_map_info *info);
a_uint32_t ssdk_switch_pcie_base_get(a_uint32_t dev_id);
#ifdef IN_UNIPHY
hsl_reg_mode ssdk_uniphy_reg_access_mode_get(a_uint32_t dev_id);
void ssdk_uniphy_reg_map_info_get(a_uint32_t dev_id, ssdk_reg_map_info *info);
#endif
a_bool_t ssdk_ess_switch_flag_get(a_uint32_t dev_id);
a_uint32_t ssdk_device_id_get(a_uint32_t index);
struct device_node *ssdk_dts_node_get(a_uint32_t dev_id);
struct clk *ssdk_dts_essclk_get(a_uint32_t dev_id);
struct clk *ssdk_dts_cmnclk_get(a_uint32_t dev_id);

int ssdk_switch_device_num_init(void);
void ssdk_switch_device_num_exit(void);
a_uint32_t ssdk_switch_device_num_get(void);
a_bool_t ssdk_is_emulation(a_uint32_t dev_id);
a_uint32_t ssdk_emu_chip_ver_get(a_uint32_t dev_id);
#ifdef IN_LED
sw_error_t ssdk_dt_port_source_pattern_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t source_id, led_ctrl_pattern_t *pattern);
#endif
#if defined(CONFIG_OF) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
sw_error_t ssdk_dt_parse(ssdk_init_cfg *cfg, a_uint32_t num, a_uint32_t *dev_id);
#endif
a_uint32_t ssdk_dts_netdev_switch_alloc(ssdk_netdev_switch_t **netdev_switch);
ssdk_netdev_switch_t *ssdk_dts_netdev_switch_get(a_uint32_t index);
ssdk_netdev_switch_t *ssdk_dts_netdev_switch_find(a_uint32_t port_id);
ssdk_netdev_switch_t *
ssdk_dts_netdev_switch_find_by_netdev(struct net_device *netdev);
ssdk_netdev_switch_t *
ssdk_dts_netdev_switch_find_by_devid(a_uint32_t dev_id);
struct mdio_device *ssdk_dt_clk_mdiodev_get(a_uint32_t dev_id);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SSDK_DTS_H */
