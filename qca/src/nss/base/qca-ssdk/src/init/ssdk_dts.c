/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kconfig.h>
#include <linux/types.h>
#if defined(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/of_gpio.h>
#endif
#include <linux/etherdevice.h>
#include <linux/clk.h>

#include "ssdk_init.h"
#include "ssdk_dts.h"
#include "ssdk_plat.h"
#include "hsl_phy.h"

#if defined(MHT)
#include "mht_sec_ctrl.h"
#endif
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>
#if defined(IN_SFP_PHY)
#include "sfp_phy.h"
#endif
#ifdef MRPPE
#include <linux/nvmem-consumer.h>
#endif
#include "qca-nss-phy/mdio-ahb.h"

static ssdk_dt_global_t ssdk_dt_global = {0};
#ifdef IN_QOS
a_uint8_t ssdk_tm_tick_mode_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->tm_tick_mode;
}

ssdk_dt_scheduler_cfg* ssdk_bootup_shceduler_cfg_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return &cfg->scheduler_cfg;
}
#endif
#ifdef IN_BM
a_uint8_t ssdk_bm_tick_mode_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->bm_tick_mode;
}
#endif
#ifdef IN_QM
a_uint16_t ssdk_ucast_queue_start_get(a_uint32_t dev_id, a_uint32_t port)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	if (!cfg || port >= SSDK_MAX_PORT_NUM)
		return 0;

	return cfg->scheduler_cfg.pool[port].ucastq_start;
}

a_uint16_t ssdk_ucast_queue_num_get(a_uint32_t dev_id, a_uint32_t port)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	if (!cfg || port >= SSDK_MAX_PORT_NUM)
		return 0;

	return cfg->scheduler_cfg.pool[port].ucastq_end -
		cfg->scheduler_cfg.pool[port].ucastq_start + 1;
}

a_uint16_t ssdk_ucast_l0_cdrr_num_get(a_uint32_t dev_id, a_uint32_t port)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	if (!cfg || port >= SSDK_MAX_PORT_NUM)
		return 0;

	return cfg->scheduler_cfg.pool[port].l0cdrr_end -
		cfg->scheduler_cfg.pool[port].l0cdrr_start + 1;
}

#ifdef IN_QOS
a_uint8_t ssdk_port_ucast_max_pri_get(a_uint32_t dev_id, a_uint32_t port)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	if (!cfg || port >= SSDK_MAX_PORT_NUM)
		return 0;

	return cfg->scheduler_cfg.pool[port].max_pri;
}

void ssdk_port_ucast_max_pri_set(a_uint32_t dev_id, a_uint32_t port,
				 a_uint8_t max_pri)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	if (!cfg || port >= SSDK_MAX_PORT_NUM)
		return;

	cfg->scheduler_cfg.pool[port].max_pri = max_pri;
}
#endif
#endif
a_uint32_t ssdk_intf_mac_num_get(void)
{
	return ssdk_dt_global.num_intf_mac;
}

a_uint8_t* ssdk_intf_macaddr_get(a_uint32_t index)
{
	return ssdk_dt_global.intf_mac[index].uc;
}

a_bool_t ssdk_uniphy_check_by_softsku(a_uint32_t dev_id,
		a_uint32_t index)
{
	return ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->uniphy_status[index];
}

a_uint32_t ssdk_dt_global_get_mac_mode(a_uint32_t dev_id, a_uint32_t index)
{
	if (index == 0) {
		return ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode;
	}
	if (index == 1) {
		return ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode1;
	}
	if (index == 2) {
		return ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode2;
	}

	return 0;
}

a_uint32_t ssdk_dt_get_port_mode(a_uint32_t dev_id, a_uint32_t port_id)
{
	a_uint32_t uniphy_index = 0, mac_mode = 0;

	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);

	mac_mode = ssdk_dt_global_get_mac_mode(dev_id, uniphy_index);
	return hsl_uniphy_mode_to_port_mode(dev_id, port_id, mac_mode);
}

a_uint32_t ssdk_dt_global_set_mac_mode(a_uint32_t dev_id, a_uint32_t index, a_uint32_t mode)
{
	if (index == 0)
	{
		 ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode= mode;
	}
	if (index == 1)
	{
		 ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode1 = mode;
	}
	if (index == 2)
	{
		 ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode2 = mode;
	}

	return 0;
}

a_uint32_t ssdk_cpu_bmp_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->port_cfg.cpu_bmp;
}

a_uint32_t ssdk_lan_bmp_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->port_cfg.lan_bmp;
}

a_uint32_t ssdk_wan_bmp_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->port_cfg.wan_bmp;
}

sw_error_t ssdk_lan_bmp_set(a_uint32_t dev_id, a_uint32_t lan_bmp)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];
	cfg->port_cfg.lan_bmp = lan_bmp;

	return SW_OK;
}

sw_error_t ssdk_wan_bmp_set(a_uint32_t dev_id, a_uint32_t wan_bmp)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];
	cfg->port_cfg.wan_bmp = wan_bmp;

	return SW_OK;
}

a_uint32_t ssdk_inner_bmp_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->port_cfg.inner_bmp;
}

hsl_reg_mode ssdk_switch_reg_access_mode_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->switch_reg_access_mode;
}
#ifdef IN_UNIPHY
hsl_reg_mode ssdk_uniphy_reg_access_mode_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->uniphy_reg_access_mode;
}
#endif
void ssdk_switch_reg_map_info_get(a_uint32_t dev_id, ssdk_reg_map_info *info)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	info->base_addr = cfg->switchreg_base_addr;
	info->size = cfg->switchreg_size;
}
a_uint32_t ssdk_switch_pcie_base_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->pcie_hw_base;
}
#ifdef IN_UNIPHY
void ssdk_uniphy_reg_map_info_get(a_uint32_t dev_id, ssdk_reg_map_info *info)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	info->base_addr = cfg->uniphyreg_base_addr;
	info->size = cfg->uniphyreg_size;
}
#endif

a_uint32_t ssdk_device_id_get(a_uint32_t index)
{
	return ssdk_dt_global.ssdk_dt_switch_nodes[index]->device_id;
}

struct device_node *ssdk_dts_node_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->of_node;
}

struct clk *ssdk_dts_essclk_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->ess_clk;
}

struct clk *ssdk_dts_cmnclk_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->cmnblk_clk;
}

struct mdio_device *ssdk_dt_clk_mdiodev_get(a_uint32_t dev_id)
{
	ssdk_dt_cfg* cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	return cfg->clk_mdiodev;
}

a_uint32_t ssdk_dts_netdev_switch_alloc(ssdk_netdev_switch_t **netdev_switch)
{
	a_uint32_t index = 0;

	for(index = 0; index < SSDK_NETDEV_SWITCH_NUM; index++) {
		if(ssdk_dt_global.netdev_switch[index].switch_connected == A_FALSE) {
			*netdev_switch = &ssdk_dt_global.netdev_switch[index];
			break;
		}
	}

	return index;
}

ssdk_netdev_switch_t *ssdk_dts_netdev_switch_get(a_uint32_t index)
{
	if(index >= SSDK_NETDEV_SWITCH_NUM)
		return NULL;

	return &ssdk_dt_global.netdev_switch[index];
}

ssdk_netdev_switch_t *ssdk_dts_netdev_switch_find(a_uint32_t port_id)
{
	a_uint32_t index = 0;

	for(index = 0; index < SSDK_NETDEV_SWITCH_NUM; index++) {
		if(ssdk_dt_global.netdev_switch[index].switch_netdev_port == port_id)
			return &ssdk_dt_global.netdev_switch[index];
	}

	return NULL;
}

ssdk_netdev_switch_t *
ssdk_dts_netdev_switch_find_by_netdev(struct net_device *netdev)
{
	a_uint32_t index = 0;

	for (index = 0; index < SSDK_NETDEV_SWITCH_NUM; index++) {
		if (ssdk_dt_global.netdev_switch[index].dev == netdev)
			return &ssdk_dt_global.netdev_switch[index];
	}

	return NULL;
}

ssdk_netdev_switch_t *
ssdk_dts_netdev_switch_find_by_devid(a_uint32_t dev_id)
{
	a_uint32_t index = 0;

	for(index = 0; index < SSDK_NETDEV_SWITCH_NUM; index++) {
		if(ssdk_dt_global.netdev_switch[index].switch_dev_id == dev_id)
			return &ssdk_dt_global.netdev_switch[index];
	}

	return NULL;
}

#if defined(CONFIG_OF) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
static void ssdk_dt_parse_mac_mode(a_uint32_t dev_id,
		struct device_node *switch_node, ssdk_init_cfg *cfg)
{
	const __be32 *mac_mode;
	a_uint32_t len = 0;

	mac_mode = of_get_property(switch_node, "switch_mac_mode", &len);
	if (!mac_mode)
		SSDK_INFO("mac mode doesn't exit!\n");
	else {
		cfg->mac_mode = be32_to_cpup(mac_mode);
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode = cfg->mac_mode;
	}

	mac_mode = of_get_property(switch_node, "switch_mac_mode1", &len);
	if(!mac_mode)
		SSDK_INFO("mac mode1 doesn't exit!\n");
	else {
		cfg->mac_mode1 = be32_to_cpup(mac_mode);
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode1 = cfg->mac_mode1;
	}

	mac_mode = of_get_property(switch_node, "switch_mac_mode2", &len);
	if(!mac_mode)
		SSDK_INFO("mac mode2 doesn't exit!\n");
	else {
		cfg->mac_mode2 = be32_to_cpup(mac_mode);
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->mac_mode2 = cfg->mac_mode2;
	}

	return;
}
#ifdef IN_UNIPHY
#ifdef MRPPE
static void ssdk_softsku_uniphy_parse(a_uint32_t dev_id,
		struct device_node *uniphy_node)
{
	char uniphy_name[][20] = {"uniphy0_disable", "uniphy1_disable", "uniphy2_disable"};
	a_uint32_t i = 0, uniphy_id = sizeof(uniphy_name) / sizeof(uniphy_name[0]);
	struct nvmem_cell *uniphy_nvmem;
	u8 *disable_status;
	size_t uniphy_len;

	for (i = 0; i < uniphy_id; i++) {
		/* By default, the uniphy is enabled unless disabled status is retrieved
		 * from the nvmem cell.
		 */
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->uniphy_status[i] = A_TRUE;
		uniphy_nvmem = of_nvmem_cell_get(uniphy_node, uniphy_name[i]);
		if (IS_ERR(uniphy_nvmem)) {
			if (PTR_ERR(uniphy_nvmem) == -EPROBE_DEFER)
				SSDK_ERROR("mrppe uniphy%d nvmem cell probe fail!\n", i);
			continue;
		}

		disable_status = nvmem_cell_read(uniphy_nvmem, &uniphy_len);
		if (!IS_ERR(disable_status)) {
			if (*disable_status == 1) {
				ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->uniphy_status[i] = A_FALSE;
				SSDK_INFO("IPQ54xx uniphy%d is not available on this SKU!\n", i);
			} else {
				ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->uniphy_status[i] = A_TRUE;
			}
			kfree(disable_status);
		}
		nvmem_cell_put(uniphy_nvmem);
	}
}
#endif
static void ssdk_dt_parse_uniphy(a_uint32_t dev_id)
{
	struct device_node *uniphy_node = NULL;
	a_uint32_t len = 0;
	const __be32 *reg_cfg;
	ssdk_dt_cfg *cfg;

	/* read uniphy register base and address space */
	uniphy_node = of_find_node_by_name(NULL, "ess-uniphy");
	if (!of_device_is_available(uniphy_node))
		SSDK_INFO("ess-uniphy DT doesn't exist!\n");
	else {
		cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];
		reg_cfg = of_get_property(uniphy_node, "reg", &len);
		if(!reg_cfg)
			SSDK_INFO("uniphy reg address doesn't exist!\n");
		else {
			cfg->uniphyreg_base_addr = be32_to_cpup(reg_cfg);
			cfg->uniphyreg_size = be32_to_cpup(reg_cfg + 1);
		}
		if (of_property_read_string(uniphy_node, "uniphy_access_mode",
				            (const char **)&cfg->uniphy_access_mode))
			SSDK_INFO("uniphy access mode doesn't exist!\n");
		else {
			if(!strcmp(cfg->uniphy_access_mode, "local bus"))
				cfg->uniphy_reg_access_mode = HSL_REG_LOCAL_BUS;
		}
#ifdef MRPPE
		/* parse uniphy disable status by softsku register */
		ssdk_softsku_uniphy_parse(dev_id, uniphy_node);
#endif
	}

	return;
}
#endif
#ifdef IN_QOS
static void ssdk_dt_parse_l1_scheduler_cfg(
	struct device_node *port_node,
	a_uint32_t port_id, a_uint32_t dev_id)
{
	struct device_node *scheduler_node;
	struct device_node *child;
	ssdk_dt_scheduler_cfg *cfg = &(ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->scheduler_cfg);
	a_uint32_t tmp_cfg[5] = {0};
	const __be32 *paddr;
	a_uint32_t len, i, j, l0_sp_id, pri_loop = 0;
	a_uint32_t max_pri = SSDK_SP_MAX_PRIORITY;
	int cfg_cnt = 0;

	scheduler_node = of_find_node_by_name(port_node, "l1scheduler");
	if (!scheduler_node) {
		SSDK_ERROR("cannot find l1scheduler node for port\n");
		return;
	}

	for_each_available_child_of_node(scheduler_node, child) {
		paddr = of_get_property(child, "sp", &len);
		len /= sizeof(a_uint32_t);
		if (!paddr) {
			SSDK_ERROR("error reading sp property\n");
			return;
		}

		cfg_cnt = of_property_count_elems_of_size(child, "cfg", sizeof(u32));
		if (cfg_cnt <= 0 || (cfg_cnt != 4 && cfg_cnt !=5)) {
			SSDK_ERROR("wrong number %d of cfg!\n", cfg_cnt);
			return;
		}

		if (of_property_read_u32_array(child,
				"cfg", tmp_cfg, cfg_cnt)) {
			SSDK_ERROR("error reading cfg property!\n");
			return;
		}

		/* update the max_pri if the property sp_max_pri exist */
		if (!of_property_read_u32(child, "sp_max_pri", &max_pri)) {
			SSDK_DEBUG("Configure Max priority per L1 SP: %d\n", max_pri);
		}


		if (of_property_read_u32(child, "sp_loop_pri", &pri_loop)) {
			for (i = 0; i < len; i++) {
				l0_sp_id = be32_to_cpup(paddr+i);
				if (l0_sp_id >= SSDK_L1SCHEDULER_CFG_MAX) {
					SSDK_ERROR("Invalid parameter for sp(%d)\n", l0_sp_id);
					return;
				}
				cfg->l1cfg[l0_sp_id].valid = 1;
				cfg->l1cfg[l0_sp_id].port_id = port_id;
				j = 0;

				if (cfg_cnt == 5)
					cfg->l1cfg[l0_sp_id].sp_id = tmp_cfg[j++];
				else
					cfg->l1cfg[l0_sp_id].sp_id = port_id;

				cfg->l1cfg[l0_sp_id].cpri = tmp_cfg[j++];
				cfg->l1cfg[l0_sp_id].cdrr_id = tmp_cfg[j++];
				cfg->l1cfg[l0_sp_id].epri = tmp_cfg[j++];
				cfg->l1cfg[l0_sp_id].edrr_id = tmp_cfg[j++];
			}
		} else {
			/* should one SP for priority loop */
			if (len != 1) {
				SSDK_ERROR("should one SP for loop!\n");
				return;
			}

			l0_sp_id = be32_to_cpup(paddr);
			if (l0_sp_id >= SSDK_L1SCHEDULER_CFG_MAX) {
				SSDK_ERROR("Invalid parameter for sp(%d)\n", l0_sp_id);
				return;
			}
			for (i = 0; i < pri_loop; i++) {
				cfg->l1cfg[l0_sp_id + i].valid = 1;
				cfg->l1cfg[l0_sp_id + i].port_id = port_id;
				j = 0;

				if (cfg_cnt == 5) {
					cfg->l1cfg[l0_sp_id + i].sp_id = tmp_cfg[j++] + i/max_pri;
					cfg->l1cfg[l0_sp_id + i].cpri = tmp_cfg[j++] + i%max_pri;
					cfg->l1cfg[l0_sp_id + i].cdrr_id = tmp_cfg[j++] + i%max_pri + (i/max_pri) * max_pri;
					cfg->l1cfg[l0_sp_id + i].epri = tmp_cfg[j++] + i%max_pri;
					cfg->l1cfg[l0_sp_id + i].edrr_id = tmp_cfg[j++] + i%max_pri + (i/max_pri) * max_pri;
				} else {
					cfg->l1cfg[l0_sp_id + i].sp_id = port_id;
					cfg->l1cfg[l0_sp_id + i].cpri = tmp_cfg[j++] + i%max_pri;
					cfg->l1cfg[l0_sp_id + i].cdrr_id = tmp_cfg[j++] + i%max_pri;
					cfg->l1cfg[l0_sp_id + i].epri = tmp_cfg[j++] + i%max_pri;
					cfg->l1cfg[l0_sp_id + i].edrr_id = tmp_cfg[j++] + i%max_pri;
				}


			}
		}
	}
}

static void ssdk_dt_parse_l0_queue_cfg(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	struct device_node *node,
	a_uint8_t *queue_name,
	a_uint8_t *loop_name)
{
	ssdk_dt_scheduler_cfg *cfg = &(ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->scheduler_cfg);
	a_uint32_t tmp_cfg[5];
	const __be32 *paddr;
	a_uint32_t len, i, queue_id, pri_loop;
	a_uint32_t max_pri = SSDK_SP_MAX_PRIORITY;
	a_uint8_t cur_max_pri = ssdk_port_ucast_max_pri_get(dev_id, port_id);

	paddr = of_get_property(node, queue_name, &len);
	len /= sizeof(a_uint32_t);
	if (!paddr) {
		SSDK_DEBUG("error reading %s property\n", queue_name);
		return;
	}
	if (of_property_read_u32_array(node, "cfg", tmp_cfg, 5)) {
		SSDK_ERROR("error reading cfg property!\n");
		return;
	}

	/* update the max_pri if the property ucast_max_pri exist */
	if (!of_property_read_u32(node, "ucast_max_pri", &max_pri)) {
		SSDK_DEBUG("Configure Max priority per SP: %d\n", max_pri);
	}

	if (max_pri > cur_max_pri)
		ssdk_port_ucast_max_pri_set(dev_id, port_id, max_pri);

	if (of_property_read_u32(node, loop_name, &pri_loop)) {
		for (i = 0; i < len; i++) {
			queue_id = be32_to_cpup(paddr+i);
			if (queue_id >= SSDK_L0SCHEDULER_CFG_MAX) {
				SSDK_ERROR("Invalid parameter for queue(%d)\n",
					queue_id);
				return;
			}
			cfg->l0cfg[queue_id].valid = 1;
			cfg->l0cfg[queue_id].port_id = port_id;
			cfg->l0cfg[queue_id].sp_id = tmp_cfg[0];
			cfg->l0cfg[queue_id].cpri = tmp_cfg[1];
			cfg->l0cfg[queue_id].cdrr_id = tmp_cfg[2];
			cfg->l0cfg[queue_id].epri = tmp_cfg[3];
			cfg->l0cfg[queue_id].edrr_id = tmp_cfg[4];
		}
	} else {
		/* should one queue for loop */
		if (len != 1) {
			SSDK_ERROR("should one queue for loop!\n");
			return;
		}
		queue_id = be32_to_cpup(paddr);
		if (queue_id >= SSDK_L0SCHEDULER_CFG_MAX) {
			SSDK_ERROR("Invalid parameter for queue(%d)\n",
				queue_id);
			return;
		}
		for (i = 0; i < pri_loop; i++) {
			cfg->l0cfg[queue_id + i].valid = 1;
			cfg->l0cfg[queue_id + i].port_id = port_id;
			cfg->l0cfg[queue_id + i].sp_id = tmp_cfg[0] + i/max_pri;
			cfg->l0cfg[queue_id + i].cpri = tmp_cfg[1] + i%max_pri;
			cfg->l0cfg[queue_id + i].cdrr_id = tmp_cfg[2] + i%max_pri + (i/max_pri) * max_pri;
			cfg->l0cfg[queue_id + i].epri = tmp_cfg[3] + i%max_pri;
			cfg->l0cfg[queue_id + i].edrr_id = tmp_cfg[4] + i%max_pri + (i/max_pri) * max_pri;
		}
	}
}

static void ssdk_dt_parse_l0_scheduler_cfg(
	struct device_node *port_node,
	a_uint32_t port_id, a_uint32_t dev_id)
{
	struct device_node *scheduler_node;
	struct device_node *child;

	scheduler_node = of_find_node_by_name(port_node, "l0scheduler");
	if (!scheduler_node) {
		SSDK_ERROR("Can't find l0scheduler node for port\n");
		return;
	}
	for_each_available_child_of_node(scheduler_node, child) {
		ssdk_dt_parse_l0_queue_cfg(dev_id, port_id, child,
				"ucast_queue", "ucast_loop_pri");
		ssdk_dt_parse_l0_queue_cfg(dev_id, port_id, child,
				"mcast_queue", "mcast_loop_pri");
	}
}

static void ssdk_dt_parse_scheduler_resource(
	struct device_node *port_node,
	a_uint32_t dev_id, a_uint32_t port_id)
{
	a_uint32_t uq[2], mq[2], l0sp[2], l0cdrr[2];
	a_uint32_t l0edrr[2], l1cdrr[2], l1edrr[2], l1sp[2];
	ssdk_dt_portscheduler_cfg *scheduler_cfg = NULL;
	ssdk_dt_scheduler_cfg *cfg = &(ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->scheduler_cfg);
	int ret;

	ret = of_property_read_u32_array(port_node, "l1sp", l1sp, 2);
	if (ret == -EINVAL) {
		l1sp[0] = SSDK_L1_SP_ID_INVALID;
		l1sp[1] = SSDK_L1_SP_ID_INVALID;
	} else if (ret != 0) {
		SSDK_ERROR("error reading L1 SP ID properties of port %d\n", port_id);
		return;
	}

	if (of_property_read_u32_array(port_node, "ucast_queue", uq, 2)
		|| of_property_read_u32_array(port_node, "mcast_queue", mq, 2)
		|| of_property_read_u32_array(port_node, "l0sp", l0sp, 2)
		|| of_property_read_u32_array(port_node, "l0cdrr", l0cdrr, 2)
		|| of_property_read_u32_array(port_node, "l0edrr", l0edrr, 2)
		|| of_property_read_u32_array(port_node, "l1cdrr", l1cdrr, 2)
		|| of_property_read_u32_array(port_node, "l1edrr", l1edrr, 2)){
		SSDK_ERROR("error reading port resource scheduler properties\n");
		return;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (of_node_name_eq(port_node, "reserved")) {
#else
	if (!of_node_cmp(port_node->name, "reserved")) {
#endif
		scheduler_cfg = &cfg->reserved_pool;
	} else {
		scheduler_cfg = &cfg->pool[port_id];
	}

	if (l0sp[0] >= SSDK_L1SCHEDULER_CFG_MAX ||
	    l0sp[1] >= SSDK_L1SCHEDULER_CFG_MAX) {
		SSDK_ERROR("l0sp_start %d or l0sp_end %d out of l1cfg range\n",
			   l0sp[0], l0sp[1]);
		return;
	}

	scheduler_cfg->ucastq_start = uq[0];
	scheduler_cfg->ucastq_end = uq[1];
	scheduler_cfg->mcastq_start = mq[0];
	scheduler_cfg->mcastq_end = mq[1];
	scheduler_cfg->l0sp_start = l0sp[0];
	scheduler_cfg->l0sp_end = l0sp[1];
	scheduler_cfg->l0cdrr_start = l0cdrr[0];
	scheduler_cfg->l0cdrr_end = l0cdrr[1];
	scheduler_cfg->l0edrr_start = l0edrr[0];
	scheduler_cfg->l0edrr_end = l0edrr[1];
	scheduler_cfg->l1sp_start = l1sp[0];
	scheduler_cfg->l1sp_end = l1sp[1];
	scheduler_cfg->l1cdrr_start = l1cdrr[0];
	scheduler_cfg->l1cdrr_end = l1cdrr[1];
	scheduler_cfg->l1edrr_start = l1edrr[0];
	scheduler_cfg->l1edrr_end = l1edrr[1];
}

static void ssdk_dt_apply_qpool_switch(a_uint32_t dev_id)
{
	a_uint16_t dst_uq_start, dst_uq_end, src_uq_start, src_uq_end;
	a_uint16_t dst_mq_start, dst_mq_end, src_mq_start, src_mq_end;
	a_uint8_t dst_sp_start, dst_sp_end, src_sp_start, src_sp_end;
	a_uint8_t dst_l1_sp_id, src_l1_sp_id;
	ssdk_dt_portscheduler_cfg tmp;
	a_uint32_t i, src_port, dst_port;
	ssdk_dt_cfg *dt_cfg = ssdk_dt_global.ssdk_dt_switch_nodes[dev_id];

	ssdk_qpool_switch_ports_get(&src_port, &dst_port);

	if (!dt_cfg || src_port == dst_port ||
	    src_port >= SSDK_MAX_PORT_NUM || dst_port >= SSDK_MAX_PORT_NUM)
		return;

	/* capture ranges and L1 sp_ids before pool swap */
	dst_uq_start = dt_cfg->scheduler_cfg.pool[dst_port].ucastq_start;
	dst_uq_end   = dt_cfg->scheduler_cfg.pool[dst_port].ucastq_end;
	src_uq_start = dt_cfg->scheduler_cfg.pool[src_port].ucastq_start;
	src_uq_end   = dt_cfg->scheduler_cfg.pool[src_port].ucastq_end;
	dst_mq_start = dt_cfg->scheduler_cfg.pool[dst_port].mcastq_start;
	dst_mq_end   = dt_cfg->scheduler_cfg.pool[dst_port].mcastq_end;
	src_mq_start = dt_cfg->scheduler_cfg.pool[src_port].mcastq_start;
	src_mq_end   = dt_cfg->scheduler_cfg.pool[src_port].mcastq_end;
	dst_sp_start = dt_cfg->scheduler_cfg.pool[dst_port].l0sp_start;
	dst_sp_end   = dt_cfg->scheduler_cfg.pool[dst_port].l0sp_end;
	src_sp_start = dt_cfg->scheduler_cfg.pool[src_port].l0sp_start;
	src_sp_end   = dt_cfg->scheduler_cfg.pool[src_port].l0sp_end;
	dst_l1_sp_id = dt_cfg->scheduler_cfg.l1cfg[dst_sp_start].sp_id;
	src_l1_sp_id = dt_cfg->scheduler_cfg.l1cfg[src_sp_start].sp_id;

	/* swap pool resource allocation between the two ports */
	tmp = dt_cfg->scheduler_cfg.pool[dst_port];
	dt_cfg->scheduler_cfg.pool[dst_port] = dt_cfg->scheduler_cfg.pool[src_port];
	dt_cfg->scheduler_cfg.pool[src_port] = tmp;

	/* update port_id for unicast L0 scheduler entries */
	for (i = dst_uq_start; i <= dst_uq_end && i < SSDK_L0SCHEDULER_CFG_MAX; i++) {
		if (dt_cfg->scheduler_cfg.l0cfg[i].valid)
			dt_cfg->scheduler_cfg.l0cfg[i].port_id = src_port;
	}
	for (i = src_uq_start; i <= src_uq_end && i < SSDK_L0SCHEDULER_CFG_MAX; i++) {
		if (dt_cfg->scheduler_cfg.l0cfg[i].valid)
			dt_cfg->scheduler_cfg.l0cfg[i].port_id = dst_port;
	}

	/* update port_id and sp_id for L1 SP scheduler entries */
	for (i = dst_sp_start; i <= dst_sp_end && i < SSDK_L1SCHEDULER_CFG_MAX; i++) {
		if (dt_cfg->scheduler_cfg.l1cfg[i].valid) {
			dt_cfg->scheduler_cfg.l1cfg[i].port_id = src_port;
			dt_cfg->scheduler_cfg.l1cfg[i].sp_id = src_l1_sp_id;
		}
	}
	for (i = src_sp_start; i <= src_sp_end && i < SSDK_L1SCHEDULER_CFG_MAX; i++) {
		if (dt_cfg->scheduler_cfg.l1cfg[i].valid) {
			dt_cfg->scheduler_cfg.l1cfg[i].port_id = dst_port;
			dt_cfg->scheduler_cfg.l1cfg[i].sp_id = dst_l1_sp_id;
		}
	}

	/*
	 * Multicast queues are pinned to their physical PPE port so port_id
	 * must not change.  Update sp_id, cdrr_id and edrr_id positionally
	 * between the two ports to reflect the new pool assignment.
	 */
	for (i = 0; i <= (a_uint32_t)(dst_mq_end - dst_mq_start); i++) {
		a_uint16_t di = dst_mq_start + i;
		a_uint16_t si = src_mq_start + i;
		a_uint8_t tmp_sp, tmp_cdrr, tmp_edrr;

		if (di >= SSDK_L0SCHEDULER_CFG_MAX || si >= SSDK_L0SCHEDULER_CFG_MAX)
			break;

		tmp_sp = dt_cfg->scheduler_cfg.l0cfg[di].sp_id;
		dt_cfg->scheduler_cfg.l0cfg[di].sp_id = dt_cfg->scheduler_cfg.l0cfg[si].sp_id;
		dt_cfg->scheduler_cfg.l0cfg[si].sp_id = tmp_sp;

		tmp_cdrr = dt_cfg->scheduler_cfg.l0cfg[di].cdrr_id;
		dt_cfg->scheduler_cfg.l0cfg[di].cdrr_id = dt_cfg->scheduler_cfg.l0cfg[si].cdrr_id;
		dt_cfg->scheduler_cfg.l0cfg[si].cdrr_id = tmp_cdrr;

		tmp_edrr = dt_cfg->scheduler_cfg.l0cfg[di].edrr_id;
		dt_cfg->scheduler_cfg.l0cfg[di].edrr_id = dt_cfg->scheduler_cfg.l0cfg[si].edrr_id;
		dt_cfg->scheduler_cfg.l0cfg[si].edrr_id = tmp_edrr;
	}

	SSDK_INFO("qpool_switch: swapped queue pool from port %u to port %u\n",
		  src_port, dst_port);
}

static void ssdk_dt_parse_scheduler_cfg(a_uint32_t dev_id, struct device_node *switch_node)
{
	struct device_node *scheduler_node;
	struct device_node *child;
	a_uint32_t port_id = SSDK_MAX_PORT_NUM;

	scheduler_node = of_find_node_by_name(switch_node, "port_scheduler_resource");
	if (!scheduler_node) {
		SSDK_ERROR("cannot find port_scheduler_resource node\n");
		return;
	}
	for_each_available_child_of_node(scheduler_node, child) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		if (!of_node_name_eq(child, "reserved")) {
#else
		if (of_node_cmp(child->name, "reserved")) {
#endif
			if (of_property_read_u32(child, "port_id", &port_id)) {
				SSDK_ERROR("error reading for port_id property!\n");
				return;
			}
			if (port_id >= SSDK_MAX_PORT_NUM) {
				SSDK_ERROR("invalid parameter for port_id(%d)!\n", port_id);
				return;
			}
		}
		ssdk_dt_parse_scheduler_resource(child, dev_id, port_id);
	}

	scheduler_node = of_find_node_by_name(switch_node, "port_scheduler_config");
	if (!scheduler_node) {
		SSDK_ERROR("cannot find port_scheduler_config node\n");
		return ;
	}
	for_each_available_child_of_node(scheduler_node, child) {
		if (of_property_read_u32(child, "port_id", &port_id)) {
			SSDK_ERROR("error reading for port_id property!\n");
			return;
		}
		if (port_id >= SSDK_MAX_PORT_NUM) {
			SSDK_ERROR("invalid parameter for port_id(%d)!\n", port_id);
			return;
		}
		ssdk_dt_parse_l1_scheduler_cfg(child, port_id, dev_id);
		ssdk_dt_parse_l0_scheduler_cfg(child, port_id, dev_id);
	}
}
#endif

static struct device_node *ssdk_dt_get_mdio_node(a_uint32_t dev_id)
{
	struct device_node *mdio_node = NULL;
	hsl_reg_mode reg_mode = ssdk_switch_reg_access_mode_get(dev_id);

	if (reg_mode == HSL_REG_LOCAL_BUS || reg_mode == HSL_REG_PCIE_BUS) {
		mdio_node = of_find_compatible_node(NULL, NULL, "qcom,ipq40xx-mdio");
		if (!mdio_node)
			mdio_node = of_find_compatible_node(NULL, NULL, "qcom,qca-mdio");
	} else
		mdio_node = of_find_compatible_node(NULL, NULL, "virtual,mdio-gpio");

	return mdio_node;
}

static struct device_node *
ssdk_dt_parse_mdio_node(struct device_node *switch_node,
			struct device_node *port_node, a_uint32_t dev_id)
{
	struct device_node *mdio_node = NULL;

	/* port mdio phandle reference parse first */
	if (port_node) {
		mdio_node = of_parse_phandle(port_node, "mdiobus", 0);
		if (mdio_node)
			return mdio_node;
	}

	/* switch mdio phandle refrence parse then */
	if (switch_node) {
		mdio_node = of_parse_phandle(switch_node, "mdio-bus", 0);
		if (mdio_node)
			return mdio_node;
	}

	/* parse mdio dts node directly */
	return ssdk_dt_get_mdio_node(dev_id);
}

static sw_error_t ssdk_dt_parse_phy_info(struct device_node *switch_node, a_uint32_t dev_id,
		ssdk_init_cfg *cfg)
{
	struct device_node *phy_info_node = NULL, *port_node = NULL;
	a_uint32_t port_id = 0, phy_addr = 0, forced_speed = 0,
		forced_duplex = 0, miibus_index = 0,  device_id = 0,
		switch_cpu_port = 0, index = 0;
	a_bool_t phy_c45 = A_FALSE, phy_combo = A_FALSE;
	const char *mac_type = NULL, *media_type = NULL;
	sw_error_t rv = SW_OK;
	struct device_node *mdio_node = NULL, *switch_external_node = NULL,
		*netdev_switch_node = NULL;
	int phy_reset_gpio = 0, sfp_rx_los_pin = 0, sfp_tx_dis_pin = 0,
		sfp_mod_present_pin = 0, sfp_medium_pin = 0;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	phy_features_t phy_features = 0;
	ssdk_netdev_switch_t *netdev_switch = NULL;
#if IS_ENABLED(CONFIG_MDIO_I2C)
	struct device_node *i2c_node = NULL;
	struct mii_bus *mdio_i2c = NULL;
#endif
	phy_interface_t phy_mode;
	int ret;

	phy_info_node = of_get_child_by_name(switch_node, "qcom,port_phyinfo");
	if (!phy_info_node) {
		return SW_NOT_FOUND;
	}

	for_each_available_child_of_node(phy_info_node, port_node) {
		if (of_property_read_u32(port_node, "port_id", &port_id)) {
			of_node_put(port_node);
			of_node_put(phy_info_node);
			return SW_BAD_VALUE;
		}

		if (!cfg->port_cfg.wan_bmp) {
			cfg->port_cfg.wan_bmp = BIT(port_id);
		} else {
			cfg->port_cfg.lan_bmp |= BIT(port_id);
		}

		/* parse and save port info for phylink */
		priv->ports[port_id].dev_id = dev_id;
		priv->ports[port_id].port_id = port_id;
		priv->ports[port_id].np = port_node;
		ret = of_get_phy_mode(port_node, &phy_mode);
		if (ret)
			priv->ports[port_id].interface = PHY_INTERFACE_MODE_NA;
		else
			priv->ports[port_id].interface = phy_mode;

		/* initialize phy_addr in case of undefined dts field */
		mdio_node = ssdk_dt_parse_mdio_node(switch_node, port_node, dev_id);
		if (of_device_is_available(mdio_node))
		{
			ssdk_miibus_add(dev_id, of_mdio_find_bus(mdio_node), &miibus_index);
			phy_reset_gpio = of_get_named_gpio(mdio_node, "phy-reset-gpio",
				SSDK_PHY_RESET_GPIO_INDEX);
			if(phy_reset_gpio > 0)
			{
				hsl_port_phy_reset_gpio_set(dev_id, port_id,
					(a_uint32_t)phy_reset_gpio);
			}
			else
			{
				hsl_port_phy_reset_gpio_set(dev_id, port_id, SSDK_INVALID_GPIO);
			}
		}
#if IS_ENABLED(CONFIG_MDIO_I2C)
		i2c_node = of_parse_phandle(port_node, "i2c-bus", 0);
		if(i2c_node) {
			mdio_i2c = of_mdio_find_bus(i2c_node);
			ssdk_miibus_add(dev_id, mdio_i2c, &miibus_index);
		}
#endif
		phy_addr = 0xff;
		phy_features = 0;
		of_property_read_u32(port_node, "phy_address", &phy_addr);
		if (phy_mode == PHY_INTERFACE_MODE_INTERNAL &&
			of_device_is_compatible(switch_node, "qcom,ess-switch-ipq52xx")) {
			struct mii_bus *mdio_ahb_bus = NULL;

			mdio_ahb_bus = mdio_find_bus(MDIO_AHB_BUS_NAME);
			if (mdio_ahb_bus) {
				ssdk_miibus_add(dev_id, mdio_ahb_bus, &miibus_index);
				hsl_port_phy_access_type_set(dev_id, port_id, PHY_AHB_ACCESS);
				phy_addr = MDIO_AHB_PHY_ADDR;
				phy_c45 = A_TRUE;
			} else {
				SSDK_INFO("IPQ52XX internal PHY on port %d: using default MDIO access\n",
					port_id);
			}
		}
		hsl_phy_address_init(dev_id, port_id, TO_PHY_ADDR_E(phy_addr, miibus_index));
		if (!of_property_read_u32(port_node, "forced-speed", &forced_speed) &&
			!of_property_read_u32(port_node, "forced-duplex", &forced_duplex)) {
			hsl_port_force_speed_set(dev_id, port_id, forced_speed);
			hsl_port_force_duplex_set(dev_id, port_id, (a_uint8_t)forced_duplex);

			/* save the force port address for getting mii bus on force port, this
			 * mii bus can be used to initialize the manhattan after GPIO reset.
			 */
			hsl_phy_address_init(dev_id, port_id,
				TO_PHY_ADDR_E(phy_addr, miibus_index));
		}
		/* if phy_c45 is A_FALSE, need to get the phy c45 from DTS */
		if (phy_c45 == A_FALSE)
			phy_c45 = of_property_read_bool(port_node, "ethernet-phy-ieee802.3-c45");
		hsl_port_phy_c45_capability_set(dev_id, port_id, phy_c45);
		phy_combo = of_property_read_bool(port_node, "ethernet-phy-combo");
		hsl_port_phy_combo_capability_set(dev_id, port_id, phy_combo);

		if (!of_property_read_string(port_node, "port_mac_sel", &mac_type))
		{
			if (!strncmp("QGMAC_PORT", mac_type, 10)) {
				phy_features |= PHY_F_QGMAC;
			}
			else if (!strncmp("XGMAC_PORT", mac_type, 10)) {
				phy_features |= PHY_F_XGMAC;
			}
		}

		if (!of_property_read_string(port_node, "media-type", &media_type)) {
			if (!strncmp("sfp", media_type, strlen(media_type))) {
				phy_features |= PHY_F_SFP;
			} else if (!strncmp("sfp_sgmii", media_type, strlen(media_type))) {
				phy_features |= (PHY_F_SFP | PHY_F_SFP_SGMII);
			}
		}
		hsl_port_feature_set(dev_id, port_id, phy_features | PHY_F_INIT);
		if((phy_features & PHY_F_SFP) || phy_combo) {
			/*get related PINs for SFP port*/
			if(priv) {
				sfp_rx_los_pin = of_get_named_gpio(port_node,
					"sfp_rx_los_pin", 0);
				if(sfp_rx_los_pin > 0)
				{
					priv->ports[port_id].sfp_rx_los_pin = sfp_rx_los_pin;
				}
				else if(sfp_rx_los_pin == -EPROBE_DEFER)
				{
					priv->ports[port_id].sfp_rx_los_pin = SSDK_MAX_GPIO;
				}
				else
				{
					priv->ports[port_id].sfp_rx_los_pin = SSDK_INVALID_GPIO;
				}

				sfp_tx_dis_pin = of_get_named_gpio(port_node,
					"sfp_tx_dis_pin", 0);
				if(sfp_tx_dis_pin > 0)
				{
					priv->ports[port_id].sfp_tx_dis_pin = sfp_tx_dis_pin;
				}
				else
				{
					priv->ports[port_id].sfp_tx_dis_pin = SSDK_INVALID_GPIO;
				}

				sfp_mod_present_pin = of_get_named_gpio(port_node,
					"sfp_mod_present_pin", 0);
				if(sfp_mod_present_pin > 0)
				{
					priv->ports[port_id].sfp_mod_present_pin =
						sfp_mod_present_pin;
				}
				else
				{
					priv->ports[port_id].sfp_mod_present_pin =
						SSDK_INVALID_GPIO;
				}

				sfp_medium_pin = of_get_named_gpio(port_node,
					"sfp_medium_pin", 0);
				if(sfp_medium_pin > 0)
				{
					priv->ports[port_id].sfp_medium_pin = sfp_medium_pin;
				}
				else
				{
					priv->ports[port_id].sfp_medium_pin = SSDK_INVALID_GPIO;
				}
			}
			/*register PHY device and PHY driver for SFP port*/
#ifdef IN_SFP_PHY
			sfp_phy_init(dev_id, port_id, miibus_index);
#endif
		}
		/*parse the switch external node*/
		switch_external_node = of_get_child_by_name(port_node, "switch_external");
		if(switch_external_node) {
			index = ssdk_dts_netdev_switch_alloc(&netdev_switch);
			if(index == SSDK_NETDEV_SWITCH_NUM)
				return SW_NO_RESOURCE;
			netdev_switch->switch_netdev_port = port_id;
			netdev_switch->switch_connected = A_TRUE;
			netdev_switch_node = of_parse_phandle(switch_external_node, "switch_handle",
				0);
			if(netdev_switch_node) {
				netdev_switch->switch_node = netdev_switch_node;
				if(!of_property_read_u32(netdev_switch_node, "device_id",
					&device_id))
					netdev_switch->switch_dev_id = device_id;
			}
			if(!of_property_read_u32(switch_external_node, "switch_cpu_port",
				&switch_cpu_port))
				netdev_switch->switch_cpu_port = switch_cpu_port;
		}
	}

	of_node_put(phy_info_node);

	return rv;
}

static void
ssdk_dt_parse_default_mdio_bus(struct device_node *switch_node, a_uint32_t dev_id)
{
	struct device_node *mdio_node = NULL;
	a_uint32_t miibus_index = 0;

	mdio_node = ssdk_dt_parse_mdio_node(switch_node, NULL, dev_id);

	if (of_device_is_available(mdio_node))
		ssdk_miibus_add(dev_id, of_mdio_find_bus(mdio_node), &miibus_index);
}

static sw_error_t ssdk_dt_parse_clkdev(a_uint32_t dev_id, struct device_node *switch_node)
{
	struct mdio_device *clk_dev = NULL;
	struct device_node *clk_np = NULL;
	sw_error_t ret = SW_OK;

	/* clock controller for qca8xxx device. */
	clk_np = of_parse_phandle(switch_node, "clk-handle", 0);
	if (!clk_np)
		return SW_NOT_FOUND;

	clk_dev = of_mdio_find_device(clk_np);
	if (!clk_dev) {
		SSDK_ERROR("clk-handle device is not found\n");
		ret = SW_FAIL;
		goto parse_clkdev_exit;
	}

	ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->clk_mdiodev = clk_dev;

parse_clkdev_exit:
	of_node_put(clk_np);
	return ret;
}

static void ssdk_dt_parse_mdio(a_uint32_t dev_id, struct device_node *switch_node,
		ssdk_init_cfg *cfg)
{
	struct device_node *mdio_node = NULL;
	struct device_node *child = NULL;
	a_uint32_t len = 0, i = 1;
	const __be32 *phy_addr;
	const __be32 *c45_phy;
	phy_features_t phy_features = 0;
	sw_error_t ret;

	/*parse default mdio bus*/
	ssdk_dt_parse_default_mdio_bus(switch_node, dev_id);

	ret = ssdk_dt_parse_clkdev(dev_id, switch_node);
	/* Clock MDIO device is not specified in DTS, then try to manually
	 * create the MDIO device with address 0x18.
	 */
	if (ret != SW_OK) {
		struct mdio_device *clk_dev = NULL;
		struct mii_bus *bus = ssdk_miibus_get(dev_id, SSDK_MII_DEFAULT_BUS_ID);

		if (bus)
			clk_dev = mdio_device_create(bus, 0x18);

		if (!IS_ERR_OR_NULL(clk_dev))
			ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->clk_mdiodev = clk_dev;
	}

	/* prefer to get phy info from ess-switch node */
	if (SW_OK == ssdk_dt_parse_phy_info(switch_node, dev_id, cfg))
		return;

	mdio_node = of_find_node_by_name(NULL, "mdio");

	if (!mdio_node) {
		SSDK_INFO("mdio DT doesn't exist!\n");
	}
	else {
		for_each_available_child_of_node(mdio_node, child) {
			phy_addr = of_get_property(child, "reg", &len);
			if (phy_addr) {
				hsl_phy_address_init(dev_id, i, be32_to_cpup(phy_addr));
			}

			c45_phy = of_get_property(child, "compatible", &len);
			if (c45_phy) {
				hsl_port_phy_c45_capability_set(dev_id, i, A_TRUE);
			}
			phy_features |= PHY_F_INIT;
			hsl_port_feature_set(dev_id, i, phy_features);
			if (!cfg->port_cfg.wan_bmp) {
				cfg->port_cfg.wan_bmp = BIT(i);
			} else {
				cfg->port_cfg.lan_bmp |= BIT(i);
			}

			i++;
			if (i >= SW_MAX_NR_PORT) {
				break;
			}
		}
	}
	return;
}

static sw_error_t
ssdk_dt_parse_interrupt(a_uint32_t dev_id, struct device_node *switch_node)
{
	const __be32 *link_polling_required = NULL;
	a_int32_t len = 0, intr_gpio_num = 0;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	const char *fdb_sync = NULL;

	SW_RTN_ON_NULL(priv);
	intr_gpio_num = of_get_named_gpio(switch_node, "intr-gpio", 0);
	if(intr_gpio_num < 0) {
		intr_gpio_num = of_get_named_gpio(switch_node, "link-intr-gpio", 0);
		if(intr_gpio_num < 0) {
			SSDK_INFO("link-intr-gpio is not defined, link polling is enabled\n");
		}
	}
	if(intr_gpio_num > 0) {
		if(gpio_is_valid(intr_gpio_num))
		{
			if(gpio_request_one(intr_gpio_num, GPIOF_IN, "ssdk interrupt") < 0) {
				SSDK_ERROR("gpio request faild\n");
				return SW_FAIL;
			}
			priv->intr_gpio_num = intr_gpio_num;
			priv->interrupt_no = gpio_to_irq (intr_gpio_num);
			if (priv->interrupt_no < 0) {
				SSDK_ERROR("gpio to irq faild\n");
				gpio_free(intr_gpio_num);
				return SW_FAIL;
			}
			SSDK_INFO("interrupt gpio:0x%x, interrupt number: 0x%x\n",
				intr_gpio_num, priv->interrupt_no);
		}
	}

	link_polling_required = of_get_property(switch_node, "link-polling-required", &len);
	if (!link_polling_required)
		priv->link_polling_required = A_TRUE;
	else
		priv->link_polling_required  = be32_to_cpup(link_polling_required);


	if(of_property_read_string(switch_node, "fdb_sync", &fdb_sync))
		priv->fdb_sync = FDB_SYNC_DIS;
	else {
		if(!strcmp(fdb_sync, "disable"))
			priv->fdb_sync = FDB_SYNC_DIS;
		else if(!strcmp(fdb_sync, "interrupt"))
			priv->fdb_sync = FDB_SYNC_INTR;
		else if(!strcmp(fdb_sync, "polling"))
			priv->fdb_sync = FDB_SYNC_POLLING;
		else
			return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

static void ssdk_dt_parse_port_bmp(a_uint32_t dev_id,
		struct device_node *switch_node, ssdk_init_cfg *cfg)
{
	a_uint32_t portbmp = 0;

	if (of_property_read_u32(switch_node, "switch_cpu_bmp", &cfg->port_cfg.cpu_bmp)
		|| of_property_read_u32(switch_node, "switch_lan_bmp", &cfg->port_cfg.lan_bmp)
		|| of_property_read_u32(switch_node, "switch_wan_bmp", &cfg->port_cfg.wan_bmp)) {
		SSDK_INFO("port_bmp doesn't exist!\n");
		/*
		 * the bmp maybe initialized already, so just keep ongoing.
		 */
	}

	if (!of_property_read_u32(switch_node, "switch_inner_bmp", &cfg->port_cfg.inner_bmp)) {
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->port_cfg.inner_bmp =
				cfg->port_cfg.inner_bmp;
	}

	ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->port_cfg.cpu_bmp = cfg->port_cfg.cpu_bmp;
	ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->port_cfg.lan_bmp = cfg->port_cfg.lan_bmp;
	ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->port_cfg.wan_bmp = cfg->port_cfg.wan_bmp;

	portbmp = cfg->port_cfg.lan_bmp | cfg->port_cfg.wan_bmp;
	qca_ssdk_port_bmp_set(dev_id, portbmp);

	return;
}

static void ssdk_dt_parse_intf_mac(void)
{
	struct device_node *dp_node = NULL;
	a_uint32_t dp = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
	a_uint8_t *maddr = NULL;
#else
	char maddr[6] = {0};
#endif
	char dp_name[8] = {0};

	for (dp = 1; dp <= SSDK_MAX_NR_ETH; dp++) {
		snprintf(dp_name, sizeof(dp_name), "dp%d", dp);
		dp_node = of_find_node_by_name(NULL, dp_name);
		if (!dp_node) {
			continue;
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
		maddr = (a_uint8_t *)of_get_mac_address(dp_node);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0))
		if (maddr && is_valid_ether_addr(maddr)) {
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
		if (!IS_ERR(maddr) && is_valid_ether_addr(maddr)) {
#else
		if (!of_get_mac_address(dp_node, maddr) && is_valid_ether_addr(maddr)) {
#endif
			ssdk_dt_global.num_intf_mac++;
			ether_addr_copy(ssdk_dt_global.intf_mac[dp-1].uc, maddr);
		}
	}
	return;
}

static sw_error_t ssdk_dt_parse_access_mode(struct device_node *switch_node,
		ssdk_dt_cfg *ssdk_dt_priv)
{
	const __be32 *reg_cfg;
	a_uint32_t len = 0;

	if (of_property_read_string(switch_node, "switch_access_mode",
			(const char **)&ssdk_dt_priv->reg_access_mode)) {
		SSDK_ERROR("%s: error reading properties for switch_access_mode\n",
			        switch_node->name);
		return SW_BAD_PARAM;
	}
	if(!strcmp(ssdk_dt_priv->reg_access_mode, "local bus")) {
		ssdk_dt_priv->switch_reg_access_mode = HSL_REG_LOCAL_BUS;

		reg_cfg = of_get_property(switch_node, "reg", &len);
		if(!reg_cfg) {
			SSDK_ERROR("%s: error reading properties for reg\n",
				switch_node->name);
			return SW_BAD_PARAM;
		}
		ssdk_dt_priv->switchreg_base_addr = be32_to_cpup(reg_cfg);
		ssdk_dt_priv->switchreg_size = be32_to_cpup(reg_cfg + 1);
	} else if (!strcmp(ssdk_dt_priv->reg_access_mode, "pcie bus")) {
		ssdk_dt_priv->switch_reg_access_mode = HSL_REG_PCIE_BUS;

		reg_cfg = of_get_property(switch_node, "reg", &len);
		if(!reg_cfg) {
			SSDK_ERROR("%s: error reading properties for reg\n",
				switch_node->name);
			return SW_BAD_PARAM;
		}
		ssdk_dt_priv->pcie_hw_base = be32_to_cpup(reg_cfg);
		ssdk_dt_priv->switchreg_size = be32_to_cpup(reg_cfg + 1);
		SSDK_INFO("PCIE bus pcie_base_addr: 0x%x\n",
				ssdk_dt_priv->pcie_hw_base);
	} else {
		ssdk_dt_priv->switch_reg_access_mode = HSL_REG_MDIO;
		reg_cfg = of_get_property(switch_node, "reg", &len);
		if(reg_cfg)
			ssdk_dt_priv->switchreg_base_addr = be32_to_cpup(reg_cfg);
	}

	return SW_OK;

}

#ifdef IN_LED
sw_error_t ssdk_dt_port_source_pattern_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t source_id, led_ctrl_pattern_t *pattern)
{
	led_ctrl_pattern_t source_pattern = {0};

	*pattern =
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->source_pattern[port_id][source_id];
	if(!memcmp(pattern, &source_pattern, sizeof(led_ctrl_pattern_t)))
		return SW_NOT_INITIALIZED;

	return SW_OK;
}

static void ssdk_dt_parse_led_source(a_uint32_t dev_id,
	struct device_node *led_device_node)
{
	struct device_node *child = NULL;
	a_uint8_t *led_str = NULL;
	a_uint32_t led_speed_index = 0, source_id = 0, port_id = 0;
	led_ctrl_pattern_t source_pattern = {0};
	a_bool_t port_exist = A_FALSE;

	if(!of_property_read_u32(led_device_node, "port", &port_id)) {
		if(port_id == SSDK_PHYSICAL_PORT0)
			return;
		port_exist = A_TRUE;
	}
	for_each_available_child_of_node(led_device_node, child) {
		memset(&source_pattern, 0, sizeof(source_pattern));
		if(of_property_read_u32(child, "source", &source_id))
			continue;
		if (!of_property_read_string(child, "mode", (const char **)&led_str)) {
			if (!strcmp(led_str, "normal"))
				source_pattern.mode = LED_PATTERN_MAP_EN;
			if (!strcmp(led_str, "on"))
				source_pattern.mode = LED_ALWAYS_ON;
			if (!strcmp(led_str, "blink"))
				source_pattern.mode = LED_ALWAYS_BLINK;
			if (!strcmp(led_str, "off"))
				source_pattern.mode = LED_ALWAYS_OFF;
		}
		led_speed_index = 0;
		while(!of_property_read_string_index(child, "speed", led_speed_index,
			(const char **)&led_str)) {
			if (!strcmp(led_str, "10M"))
				source_pattern.map |= LED_MAP_10M_SPEED;
			if (!strcmp(led_str, "100M"))
				source_pattern.map |= LED_MAP_100M_SPEED;
			if (!strcmp(led_str, "1000M"))
				source_pattern.map |= LED_MAP_1000M_SPEED;
			if (!strcmp(led_str, "2500M"))
				source_pattern.map |= LED_MAP_2500M_SPEED;
			if (!strcmp(led_str, "5000M"))
				source_pattern.map |= LED_MAP_5000M_SPEED;
			if (!strcmp(led_str, "10000M"))
				source_pattern.map |= LED_MAP_10000M_SPEED;
			if (!strcmp(led_str, "all"))
				source_pattern.map |= LED_MAP_ALL_SPEED;

			led_speed_index++;
		}
		if (!of_property_read_string(child, "active", (const char **)&led_str)) {
			if (!strcmp(led_str, "high"))
				source_pattern.active_level = LED_ACTIVE_HIGH;
		}
		if (!of_property_read_string(child, "blink_en", (const char **)&led_str)) {
			if (!strcmp(led_str, "disable"))
				source_pattern.map &= ~(BIT(RX_TRAFFIC_BLINK_EN)|
					BIT(TX_TRAFFIC_BLINK_EN));
		}
		if (!of_property_read_string(child, "freq", (const char **)&led_str)) {
			if (!strcmp(led_str, "2Hz"))
				source_pattern.freq = LED_BLINK_2HZ;
			if (!strcmp(led_str, "4Hz"))
				source_pattern.freq = LED_BLINK_4HZ;
			if (!strcmp(led_str, "8Hz"))
				source_pattern.freq = LED_BLINK_8HZ;
			if (!strcmp(led_str, "16Hz"))
				source_pattern.freq = LED_BLINK_16HZ;
			if (!strcmp(led_str, "32Hz"))
				source_pattern.freq = LED_BLINK_32HZ;
			if (!strcmp(led_str, "64Hz"))
				source_pattern.freq = LED_BLINK_64HZ;
			if (!strcmp(led_str, "128Hz"))
				source_pattern.freq = LED_BLINK_128HZ;
			if (!strcmp(led_str, "256Hz"))
				source_pattern.freq = LED_BLINK_256HZ;
			if (!strcmp(led_str, "auto"))
				source_pattern.freq = LED_BLINK_TXRX;
		} else {
			source_pattern.freq = LED_BLINK_4HZ;
		}
		if(!port_exist) {
			port_id = source_id/PORT_LED_SOURCE_MAX+1;
			source_id = source_id%PORT_LED_SOURCE_MAX;
		}
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->source_pattern[port_id][source_id]
			= source_pattern;
	}

	return;
}

static sw_error_t ssdk_dt_parse_port_ledinfo(a_uint32_t dev_id,
	struct device_node *switch_node)
{
	struct device_node *port_ledinfo_node = NULL, *port_node = NULL;

	port_ledinfo_node = of_get_child_by_name(switch_node, "qcom,port_ledinfo");
	if (!port_ledinfo_node) {
		return SW_NOT_FOUND;
	}
	for_each_available_child_of_node(port_ledinfo_node, port_node) {
		ssdk_dt_parse_led_source(dev_id, port_node);
	}

	return SW_OK;
}

static void ssdk_dt_parse_led(a_uint32_t dev_id, struct device_node *switch_node)
{
	if(!ssdk_dt_parse_port_ledinfo(dev_id, switch_node))
		return;

	return ssdk_dt_parse_led_source(dev_id, switch_node);
}
#endif
static sw_error_t ssdk_dt_get_switch_node(struct device_node **switch_node,
		a_uint32_t num)
{
	struct device_node *switch_instance = NULL;
	char ess_switch_name[64] = {0};

	if (num == 0)
		snprintf(ess_switch_name, sizeof(ess_switch_name), "ess-switch");
	else
		snprintf(ess_switch_name, sizeof(ess_switch_name), "ess-switch%d", num);

	/*
	 * Get reference to ESS SWITCH device node from ess-instance node firstly.
	 */
	switch_instance = of_find_node_by_name(NULL, "ess-instance");
	*switch_node = of_find_node_by_name(switch_instance, ess_switch_name);
	if (!*switch_node) {
		SSDK_WARN("cannot find ess-switch node\n");
		return SW_BAD_PARAM;
	}
	if (!of_device_is_available(*switch_node))
	{
		SSDK_WARN("ess-switch node[%s] is disabled\n", ess_switch_name);
		return SW_DISABLE;
	}

	return SW_OK;
}

sw_error_t ssdk_dt_parse(ssdk_init_cfg *cfg, a_uint32_t num, a_uint32_t *dev_id)
{
	sw_error_t rv = SW_OK;
	struct device_node *switch_node = NULL;
	ssdk_dt_cfg *ssdk_dt_priv = NULL;
	a_uint32_t len = 0;
	const __be32 *device_id;

	rv = ssdk_dt_get_switch_node(&switch_node, num);
	SW_RTN_ON_ERROR(rv);

	device_id = of_get_property(switch_node, "device_id", &len);
	if(!device_id)
		*dev_id = 0;
	else
		*dev_id = be32_to_cpup(device_id);

	ssdk_dt_priv = ssdk_dt_global.ssdk_dt_switch_nodes[*dev_id];
	ssdk_dt_priv->device_id = *dev_id;
	ssdk_dt_priv->of_node = switch_node;
	ssdk_dt_priv->ess_clk= ERR_PTR(-ENOENT);
	ssdk_dt_priv->cmnblk_clk = ERR_PTR(-ENOENT);

	if (of_property_read_bool(switch_node, "qcom,emulation")) {
		ssdk_dt_priv->is_emulation = A_TRUE;
		SSDK_INFO("RUMI emulation\n");
	}

	/* parse common dts info */
	rv = ssdk_dt_parse_access_mode(switch_node, ssdk_dt_priv);
	SW_RTN_ON_ERROR(rv);
	ssdk_dt_parse_mac_mode(*dev_id, switch_node, cfg);
	ssdk_dt_parse_mdio(*dev_id, switch_node, cfg);
	ssdk_dt_parse_port_bmp(*dev_id, switch_node, cfg);
	ssdk_dt_parse_interrupt(*dev_id, switch_node);
#ifdef IN_LED
	ssdk_dt_parse_led(*dev_id, switch_node);
#endif
	if (of_device_is_compatible(switch_node, "qcom,ess-switch-ipq807x") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-ipq95xx") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-ipq60xx") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-ipq53xx") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-ipq54xx") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-ipq96xx") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-ipq52xx") ||
		of_device_is_compatible(switch_node, "qcom,ess-switch-qce22xx")) {
		/* HPPE chip */
		a_uint32_t mode = 0;
#ifdef IN_UNIPHY
		ssdk_dt_parse_uniphy(*dev_id);
#endif
#ifdef IN_QOS
		ssdk_dt_parse_scheduler_cfg(*dev_id, switch_node);
		ssdk_dt_apply_qpool_switch(*dev_id);
#endif
		ssdk_dt_parse_intf_mac();

		ssdk_dt_priv->cmnblk_clk = of_clk_get_by_name(switch_node, "cmn_ahb_clk");
		if (!of_property_read_u32(switch_node, "tm_tick_mode", &mode))
			ssdk_dt_priv->tm_tick_mode = mode;

		if (!of_property_read_u32(switch_node, "bm_tick_mode", &mode))
			ssdk_dt_priv->bm_tick_mode = mode;
	}
	else if (of_device_is_compatible(switch_node, "qcom,ess-switch-qca83xx")) {
		/* s17/s17c chip */
		SSDK_INFO("switch node is qca83xx!\n");
	}
	else if (of_device_is_compatible(switch_node, "qcom,ess-switch-qca8386")) {
#ifdef MHT
		/* manhattan chip */
		SSDK_INFO("switch node is qca8386!\n");
#endif
	}
	else {
		SSDK_WARN("invalid compatible property\n");
	}

	return SW_OK;
}
#endif

int ssdk_switch_device_num_init(void)
{
	struct device_node *switch_instance = NULL;
	a_uint32_t len = 0;
	const __be32 *num_devices;
	a_uint32_t dev_num = 1, dev_id = 0;

	switch_instance = of_find_node_by_name(NULL, "ess-instance");
	if (switch_instance) {
		struct platform_device *pdev = of_find_device_by_node(switch_instance);
		int ret;

		if (!pdev) {
			SSDK_ERROR("ess-instance device not found\n");
			of_node_put(switch_instance);
			return -ENODEV;
		}

		ret = of_platform_populate(switch_instance, NULL, NULL, &pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to populate child devices: %d\n", ret);
			put_device(&pdev->dev);
			of_node_put(switch_instance);
			return ret;
		}

		num_devices = of_get_property(switch_instance, "num_devices", &len);
		if (num_devices)
			dev_num = be32_to_cpup(num_devices);

		put_device(&pdev->dev);
		of_node_put(switch_instance);
	}

	ssdk_dt_global.ssdk_dt_switch_nodes = kzalloc(dev_num * sizeof(ssdk_dt_cfg *), GFP_KERNEL);
	if (ssdk_dt_global.ssdk_dt_switch_nodes == NULL) {
		return -ENOMEM;
	}

	for (dev_id = 0; dev_id < dev_num; dev_id++) {
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id] = kzalloc(sizeof(ssdk_dt_cfg),
								GFP_KERNEL);
		if (ssdk_dt_global.ssdk_dt_switch_nodes[dev_id] == NULL) {
			return -ENOMEM;
		}
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->switch_reg_access_mode = HSL_REG_MDIO;
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->psgmii_reg_access_mode = HSL_REG_MDIO;
	}

	ssdk_dt_global.num_devices = dev_num;

	return 0;
}

void ssdk_switch_device_num_exit(void)
{
	a_uint32_t dev_id = 0;

	for (dev_id = 0; dev_id < ssdk_dt_global.num_devices; dev_id++) {
		if (ssdk_dt_global.ssdk_dt_switch_nodes[dev_id])
			kfree(ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]);
		ssdk_dt_global.ssdk_dt_switch_nodes[dev_id] = NULL;
	}

	if (ssdk_dt_global.ssdk_dt_switch_nodes)
		kfree(ssdk_dt_global.ssdk_dt_switch_nodes);
	ssdk_dt_global.ssdk_dt_switch_nodes = NULL;

	ssdk_dt_global.num_devices = 0;
}

a_uint32_t ssdk_switch_device_num_get(void)
{
	return ssdk_dt_global.num_devices;
}

a_bool_t ssdk_is_emulation(a_uint32_t dev_id)
{
	return ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->is_emulation;
}
a_uint32_t ssdk_emu_chip_ver_get(a_uint32_t dev_id)
{
	return ssdk_dt_global.ssdk_dt_switch_nodes[dev_id]->emu_chip_ver;
}
