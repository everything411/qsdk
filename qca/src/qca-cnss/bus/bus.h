/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CNSS_BUS_H
#define _CNSS_BUS_H

#include "../main.h"

int cnss_bus_init(struct cnss_plat_data *plat_priv);
void cnss_bus_deinit(struct cnss_plat_data *plat_priv);
int cnss_bus_load_m3(struct cnss_plat_data *plat_priv);
int cnss_bus_alloc_fw_mem(struct cnss_plat_data *plat_priv);
void cnss_bus_free_fw_mem(struct cnss_plat_data *plat_priv);
int cnss_bus_alloc_qdss_mem(struct cnss_plat_data *plat_priv);
void cnss_bus_free_qdss_mem(struct cnss_plat_data *plat_priv);
u32 cnss_bus_get_wake_irq(struct cnss_plat_data *plat_priv);
int cnss_bus_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv);
void cnss_bus_collect_dump_info(struct cnss_plat_data *plat_priv,
				bool in_panic);
int cnss_bus_driver_probe(struct cnss_plat_data *plat_priv);
int cnss_bus_driver_remove(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_powerup(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_shutdown(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_crash_shutdown(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_ramdump(struct cnss_plat_data *plat_priv);
int cnss_bus_register_driver_hdlr(struct cnss_plat_data *plat_priv, void *data);
int cnss_bus_unregister_driver_hdlr(struct cnss_plat_data *plat_priv);
int cnss_bus_driver_modem_status(struct cnss_plat_data *plat_priv,
				      int modem_current_status);
int cnss_bus_update_status(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status);
u64 cnss_bus_get_q6_time(struct device *dev);
void cnss_bus_fw_boot_timeout_hdlr(struct timer_list *timer);
int cnss_bus_get_msi_irq(struct device *dev, unsigned int vector);
int cnss_bus_get_soc_info(struct device *dev, struct cnss_soc_info *info);
void cnss_bus_get_msi_address(struct device *dev, u32 *msi_addr_low,
			  u32 *msi_addr_high);
int cnss_bus_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector);

#endif /* _CNSS_BUS_H */
