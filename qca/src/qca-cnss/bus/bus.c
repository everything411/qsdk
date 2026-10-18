/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/types.h>
#if defined CNSS_PCI_SUPPORT
#include "pci/pci.h"
#endif
#include "cnss_common/cnss_common.h"
#include "bus/bus.h"
#ifdef CNSS_DEBUG_SUPPORT
#include "debug/debug.h"
#endif
#include <linux/of_address.h>

int cnss_bus_init(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is Null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_init)
		return plat_priv->ops->cnss_bus_init(plat_priv);

	return 0;
}

void cnss_bus_deinit(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is Null", __func__);
		return;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return;
	}

	if (plat_priv->ops->cnss_bus_deinit)
		return plat_priv->ops->cnss_bus_deinit(plat_priv);
}

int cnss_bus_alloc_fw_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_alloc_fw_mem)
		return plat_priv->ops->cnss_bus_alloc_fw_mem(plat_priv);

	return 0;
}

void cnss_bus_free_fw_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return;
	}

	if (plat_priv->ops->cnss_bus_free_fw_mem)
		return plat_priv->ops->cnss_bus_free_fw_mem(plat_priv);
}
EXPORT_SYMBOL(cnss_bus_free_fw_mem);

int cnss_bus_alloc_qdss_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_alloc_qdss_mem)
		return plat_priv->ops->cnss_bus_alloc_qdss_mem(plat_priv);

	return 0;
}

void cnss_bus_free_qdss_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return;
	}

	if (plat_priv->ops->cnss_bus_free_qdss_mem)
		return plat_priv->ops->cnss_bus_free_qdss_mem(plat_priv);
}
EXPORT_SYMBOL(cnss_bus_free_qdss_mem);

int cnss_bus_driver_probe(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_driver_probe)
		return plat_priv->ops->cnss_bus_driver_probe(plat_priv);

	return 0;
}

int cnss_bus_driver_remove(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_driver_remove)
		return plat_priv->ops->cnss_bus_driver_remove(plat_priv);

	return 0;
}

int cnss_bus_dev_powerup(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_dev_powerup)
		return plat_priv->ops->cnss_bus_dev_powerup(plat_priv);

	return 0;
}

int cnss_bus_dev_shutdown(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_dev_shutdown)
		return plat_priv->ops->cnss_bus_dev_shutdown(plat_priv);

	return 0;
}

int cnss_bus_load_m3(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_load_m3)
		return plat_priv->ops->cnss_bus_load_m3(plat_priv);

	return 0;
}

u32 cnss_bus_get_wake_irq(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return EINVAL;
	}

	if (plat_priv->ops->cnss_bus_get_wake_irq)
		return plat_priv->ops->cnss_bus_get_wake_irq(plat_priv);

	return 0;
}

int cnss_bus_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_force_fw_assert_hdlr)
		return plat_priv->ops->cnss_bus_force_fw_assert_hdlr(plat_priv);

	return 0;
}

void cnss_bus_fw_boot_timeout_hdlr(struct timer_list *timer)
{
	struct cnss_plat_data *plat_priv =
			from_timer(plat_priv, timer, fw_boot_timer);
	if (!plat_priv)
		return;

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return;
	}

	if (plat_priv->ops->cnss_bus_fw_boot_timeout_hdlr)
		return plat_priv->ops->
			cnss_bus_fw_boot_timeout_hdlr(plat_priv->bus_priv);
}

int cnss_bus_dev_crash_shutdown(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_dev_crash_shutdown)
		return plat_priv->ops->cnss_bus_dev_crash_shutdown(plat_priv);

	return 0;
}

void cnss_bus_collect_dump_info(struct cnss_plat_data *plat_priv, bool in_panic)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return;
	}

	if (plat_priv->ops->cnss_bus_collect_dump_info)
		return plat_priv->ops->
				cnss_bus_collect_dump_info(plat_priv, in_panic);
}

int cnss_bus_dev_ramdump(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_dev_ramdump)
		return plat_priv->ops->cnss_bus_dev_ramdump(plat_priv);

	return 0;
}

int cnss_bus_register_driver_hdlr(struct cnss_plat_data *plat_priv,
					void *data)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_register_driver_hdlr)
		return plat_priv->ops->
			cnss_bus_register_driver_hdlr(plat_priv, data);

	return 0;
}

int cnss_bus_unregister_driver_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_unregister_driver_hdlr)
		return plat_priv->ops->
			cnss_bus_unregister_driver_hdlr(plat_priv);

	return 0;
}

int cnss_bus_driver_modem_status(struct cnss_plat_data *plat_priv,
						int modem_current_status)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_driver_modem_status)
		return plat_priv->ops->
			cnss_bus_driver_modem_status(plat_priv,
							modem_current_status);

	return 0;
}

int cnss_bus_update_status(struct cnss_plat_data *plat_priv,
				enum cnss_driver_status status)
{
	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_update_status)
		return plat_priv->ops->
			cnss_bus_update_status(plat_priv, status);

	return 0;
}

int cnss_bus_reg_read(struct device *dev, u32 addr, u32 *val,
					void __iomem *base)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_reg_read)
		return plat_priv->ops->
			cnss_bus_reg_read(dev, addr, val, base);

	return 0;
}
EXPORT_SYMBOL(cnss_bus_reg_read);

int cnss_bus_reg_write(struct device *dev, u32 addr, u32 val,
					void __iomem *base)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_reg_write)
		return plat_priv->ops->cnss_bus_reg_write(dev, addr, val, base);

	return 0;
}
EXPORT_SYMBOL(cnss_bus_reg_write);

int cnss_bus_get_soc_info(struct device *dev, struct cnss_soc_info *info)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_get_soc_info)
		return plat_priv->ops->cnss_bus_get_soc_info(dev, info);

	return 0;
}
EXPORT_SYMBOL(cnss_bus_get_soc_info);

u64 cnss_bus_get_q6_time(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_get_q6_time)
		return plat_priv->ops->cnss_bus_get_q6_time(dev);

	return 0;
}
EXPORT_SYMBOL(cnss_bus_get_q6_time);

int cnss_bus_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_get_msi_irq)
		return plat_priv->ops->cnss_bus_get_msi_irq(dev, vector);

	return 0;
}
EXPORT_SYMBOL(cnss_bus_get_msi_irq);

void cnss_bus_get_msi_address(struct device *dev, u32 *msi_addr_low,
			  u32 *msi_addr_high)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return;
	}

	if (plat_priv->ops->cnss_bus_get_msi_address)
		return plat_priv->ops->cnss_bus_get_msi_address(dev,
						msi_addr_low, msi_addr_high);
}
EXPORT_SYMBOL(cnss_bus_get_msi_address);

int cnss_bus_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("%s: plat_priv is null", __func__);
		return -EINVAL;
	}

	if (!plat_priv->ops) {
		cnss_pr_err("%s: callback is not registered", __func__);
		return -EINVAL;
	}

	if (plat_priv->ops->cnss_bus_get_user_msi_assignment)
		return plat_priv->ops->cnss_bus_get_user_msi_assignment(dev,
					user_name, num_vectors, user_base_data,
					base_vector);

	return 0;
}
EXPORT_SYMBOL(cnss_bus_get_user_msi_assignment);
