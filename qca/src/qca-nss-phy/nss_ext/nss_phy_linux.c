/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "nss_phy.h"
#if defined(NSS_PHY_PTP)
#include "nss_phy_ptp.h"
#endif
#include "qca807x_phy.h"
#include "qca81xx_phy.h"
#include "qca808x_phy.h"
#include "qca803x_phy.h"
#include "qca833x_phy.h"

#define NSS_PHY_DRV_NUM		6
static struct nss_phy_ops *g_ops[NSS_PHY_DRV_NUM] = { NULL };

static int nss_phy_ops_add(struct nss_phy_ops *ops)
{
	int ops_index = 0;

	for (ops_index = 0; ops_index < NSS_PHY_DRV_NUM; ops_index++) {
		if (!g_ops[ops_index]) {
			g_ops[ops_index] = ops;
			break;
		}
	}

	if (ops_index == NSS_PHY_DRV_NUM)
		return -NSS_PHY_ENOSPC;

	return ops_index;
}

static void nss_phy_ops_free(void)
{
	int ops_index = 0;

	for (ops_index = 0; ops_index < NSS_PHY_DRV_NUM; ops_index++) {
		kfree(g_ops[ops_index]);
		g_ops[ops_index] = NULL;
	}
}

static int nss_phy_id_get(struct phy_device *phydev, int addr, u32 *phy_id)
{
	int reg1 = 0, reg2 = 0;

	if (phydev->is_c45) {
		reg1 = mdiobus_c45_read(phydev->mdio.bus, addr, MDIO_MMD_AN,
			MII_PHYSID1);
		reg2 = mdiobus_c45_read(phydev->mdio.bus, addr, MDIO_MMD_AN,
			MII_PHYSID2);
	} else {
		reg1 = mdiobus_read(phydev->mdio.bus, addr, MII_PHYSID1);
		reg2 = mdiobus_read(phydev->mdio.bus, addr, MII_PHYSID2);
	}

	if (reg1 < 0 || reg2 < 0)
		return -NSS_PHY_EINVAL;

	*phy_id = reg1 << 16 | reg2;

	return 0;
}

static int nss_phy_base_addr_get(struct phy_device *phydev)
{
	int ret = 0, times = 4, base_addr = 0;
	int addr = phydev->mdio.addr;
	u32 phy_id = 0;

	if (addr >= PHY_MAX_ADDR || addr < 0)
		return -NSS_PHY_EINVAL;

	while (times--) {
		if (addr < 0)
			break;
		ret = nss_phy_id_get(phydev, addr, &phy_id);
		if (ret < 0)
			return ret;
		if (nss_phydev_id_compare(phydev, phy_id, QCA_PHY_EXACT_MASK))
			base_addr = addr;
		else
			break;
		addr--;
	}

	return base_addr;
}

static int nss_phy_base_addr_init(struct phy_device *phydev)
{
	int base_addr = 0;

	base_addr = nss_phy_base_addr_get(phydev);

	if (!phydev->shared)
		devm_phy_package_join(&phydev->mdio.dev, phydev,
			base_addr, 0);

	return 0;
}

#if defined(NSS_PHY_PTP)
static int nss_phy_ptp_ops_add(struct phy_device *phydev, struct nss_phy_ops *phy_ops)
{
	struct nss_phy_ptp_ops *ptp_ops;
	int ret;

	if (!phy_ops)
		return NSS_PHY_EINVAL;

	if (!(nss_phydev_id_compare(phydev, QCA8111_PHY, GENMASK(31, 0)) ||
	    nss_phydev_id_compare(phydev, QCA8084_PHY, GENMASK(31, 0)) ||
	    nss_phydev_id_compare(phydev, QCA8081_PHY, GENMASK(31, 0))))
		return 0;

	ptp_ops = devm_kzalloc(&phydev->mdio.dev, sizeof(*ptp_ops), GFP_KERNEL);
	if (!ptp_ops) {
		phydev_err(phydev, "nss phy ptp ops kzalloc failed!\n");
		return -NSS_PHY_ENOSPC;
	}

	ret = nss_phy_ptp_ops_init(ptp_ops);
	if (ret)
		return ret;

	phy_ops->ptp_ops = ptp_ops;

	return 0;
}
#endif

static int nss_phy_ops_init(struct phy_device *phydev)
{
	int ret;
	struct nss_phy_ops *ops = NULL;

	if (phydev->drv->driver_data)
		return 0;

	ops = nss_phy_kzalloc(sizeof(struct nss_phy_ops));
	if (!ops) {
		phydev_err(phydev, "nss phy ops kzalloc failed!\n");
		return -NSS_PHY_ENOSPC;
	}

	if (nss_phydev_id_compare(phydev, QCA8075_PHY, QCA807X_MASK)) {
#if defined(CONFIG_NSSPHY_QCA807X)
		ret = qca807x_phy_ops_init(ops);
#endif
	} else if (nss_phydev_id_compare(phydev, QCA8111_PHY, QCA81XX_MASK)) {
#if defined(CONFIG_NSSPHY_QCA81XX)
		ret = qca81xx_phy_ops_init(ops);
#endif
	} else if (nss_phydev_id_compare(phydev, QCA8084_PHY, QCA808X_MASK)) {
#if defined(CONFIG_NSSPHY_QCA808X)
		ret = qca808x_phy_ops_init(ops);
#endif
	} else if (nss_phydev_id_compare(phydev, QCA8033_PHY, QCA803X_MASK)) {
#if defined(CONFIG_NSSPHY_QCA803X)
		ret = qca803x_phy_ops_init(ops);
#endif
	} else if (nss_phydev_id_compare(phydev, QCA8337_PHY_V4,
				       QCA8337_PHY_MASK)) {
#if defined(CONFIG_NSSPHY_QCA833X)
		ret = qca833x_phy_ops_init(ops);
#endif
	} else {
		ret = -NSS_PHY_EOPNOTSUPP;
	}
	if (ret < 0) {
		phydev_err(phydev, "nss phy ops init failed\n");
		kfree(ops);
		ops = NULL;
		return ret;
	}

#if defined(NSS_PHY_PTP)
	ret = nss_phy_ptp_ops_add(phydev, ops);
	if (ret)
		return ret;
#endif
	phydev->drv->driver_data = ops;

	return nss_phy_ops_add(ops);
}

static int nss_phy_match_phy_device(struct phy_device *phydev)
{
	if (!QCA_PHY_MATCH(nss_phydev_id_get(phydev)))
		return false;

	if (phydev->drv == NULL)
		return true;

	/*init nss phy ops*/
	nss_phy_ops_init(phydev);
	/**
	* if upstream driver did not init the base addr,
	* will init it here
	*/
	nss_phy_base_addr_init(phydev);

	return false;
}

static int nss_phy_probe(struct phy_device *phydev)
{
	/*init nss phy ops*/
	nss_phy_ops_init(phydev);
	/*init base addr*/
	nss_phy_base_addr_init(phydev);

	return 0;
}

static int nss_phy_read_abilities(struct phy_device *phydev)
{
	if (phydev->is_c45)
		return genphy_c45_pma_read_abilities(phydev);
	else
		return genphy_read_abilities(phydev);
}

static int nss_phy_read_status(struct phy_device *phydev)
{
	if (phydev->is_c45)
		return genphy_c45_read_status(phydev);
	else
		return genphy_read_status(phydev);
}

static int nss_phy_suspend(struct phy_device *phydev)
{
	if (phydev->is_c45)
		return genphy_c45_pma_suspend(phydev);
	else
		return genphy_suspend(phydev);
}

static int nss_phy_resume(struct phy_device *phydev)
{
	if (phydev->is_c45)
		return genphy_c45_pma_resume(phydev);
	else
		return genphy_resume(phydev);
}

struct phy_driver nss_phy_driver = {
	.name = "nss phy driver",
	.probe = nss_phy_probe,
	.match_phy_device = nss_phy_match_phy_device,
	.get_features = nss_phy_read_abilities,
	.read_status = nss_phy_read_status,
	.suspend = nss_phy_suspend,
	.resume = nss_phy_resume,
};

static int nss_phy_fixup(struct phy_device *phydev)
{
	struct nss_phy_device nss_phydev = {0};
	int ret = 0;

	nss_phydev.phydev = phydev;
	if (nss_phydev_id_compare(phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
#if defined(CONFIG_NSSPHY_QCA808X)
		ret = qca8084_phy_fixup(&nss_phydev);
#endif
	} else if (nss_phydev_id_compare(phydev, QCA8033_PHY, QCA803X_MASK)) {
#if defined(CONFIG_NSSPHY_QCA803X)
		ret = qca803x_phy_fixup(&nss_phydev);
#endif
	} else if (nss_phydev_id_compare(phydev, QCA8075_PHY, QCA807X_MASK)) {
#if defined(CONFIG_NSSPHY_QCA807X)
		ret = qca807x_phy_fixup(&nss_phydev);
#endif
	}

	return ret;
}

static int __init nss_phy_module_init(void)
{
	int ret = 0;

	ret = phy_driver_register(&nss_phy_driver, THIS_MODULE);
	if (!ret)
		pr_info("nss phy driver register successfully\n");

	ret = phy_register_fixup_for_uid(QCA_PHY_ID, QCA_PHY_MASK,
		nss_phy_fixup);

	return ret;
}

static void __exit nss_phy_module_exit(void)
{
	nss_phy_ops_free();
	phy_driver_unregister(&nss_phy_driver);
	phy_unregister_fixup_for_uid(QCA_PHY_ID, QCA_PHY_MASK);
}

module_init(nss_phy_module_init);
module_exit(nss_phy_module_exit);
MODULE_DESCRIPTION("NSS PHY driver");
MODULE_LICENSE("Dual BSD/GPL");
