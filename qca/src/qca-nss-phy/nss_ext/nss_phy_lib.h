/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _NSS_PHY_LIB_H_
#define _NSS_PHY_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#include "nss_phy_linux_wrapper.h"

void nss_phy_mdelay(u32 msecs);
u32 nss_phydev_id_get(struct phy_device *phydev);
bool nss_phydev_id_compare(struct phy_device *phydev, u32 phy_id, u32 mask);
/*mii registers access*/
int __nss_phy_read(struct nss_phy_device *nss_phy_device, u32 reg);
int __nss_phy_write(struct nss_phy_device *nss_phy_device, u32 reg, u16 val);
int __nss_phy_modify(struct nss_phy_device *nss_phy_device, u32 reg, u16 mask,
	u16 set);
int nss_phy_read(struct nss_phy_device *nss_phy_device, u32 reg);
int nss_phy_write(struct nss_phy_device *nss_phy_device, u32 reg, u16 val);
int nss_phy_modify(struct nss_phy_device *nss_phy_device, u32 reg, u16 mask,
	u16 set);
/*mmd registers access*/
int __nss_phy_read_mmd(struct nss_phy_device *nss_phy_device, int devad,
	u32 reg);
int __nss_phy_write_mmd(struct nss_phy_device *nss_phy_device, int devad,
	u32 reg, u16 val);
int __nss_phy_modify_mmd(struct nss_phy_device *nss_phy_device, int devad,
	u32 reg, u16 mask, u16 set);
int nss_phy_read_mmd(struct nss_phy_device *nss_phy_device, int devad,
	u32 reg);
int nss_phy_write_mmd(struct nss_phy_device *nss_phy_device, int devad,
	u32 reg, u16 val);
int nss_phy_modify_mmd(struct nss_phy_device *nss_phy_device, int devad,
	u32 reg, u16 mask, u16 set);
/*debug registers access*/
int __nss_phy_read_debug(struct nss_phy_device *nss_phy_device, u16 reg);
int __nss_phy_write_debug(struct nss_phy_device *nss_phy_device, u16 reg,
	u16 data);
int __nss_phy_modify_debug(struct nss_phy_device *nss_phy_device, u16 reg,
	u16 mask, u16 set);
int nss_phy_read_debug(struct nss_phy_device *nss_phy_device, u16 reg);
int nss_phy_write_debug(struct nss_phy_device *nss_phy_device, u16 reg,
	u16 data);
int nss_phy_modify_debug(struct nss_phy_device *nss_phy_device, u16 reg,
	u16 mask, u16 set);
bool nss_phy_id_check(struct nss_phy_device *nss_phy_device, u32 phy_id,
	u32 mask);
int nss_phy_addr_get(struct nss_phy_device *nss_phy_device);
int nss_phy_share_addr_get(struct nss_phy_device *nss_phy_device);
int __nss_phy_read_pcs(struct nss_phy_device *nss_phy_device,
	int addr_offset, u16 reg);
int __nss_phy_write_pcs(struct nss_phy_device *nss_phy_device,
	int addr_offset, u16 reg, u16 data);
int __nss_phy_modify_pcs(struct nss_phy_device *nss_phy_device,
	int addr_offset, u16 reg, u16 mask, u16 set);
int nss_phy_read_pcs(struct nss_phy_device *nss_phy_device,
	int addr_offset, u16 reg);
int nss_phy_write_pcs(struct nss_phy_device *nss_phy_device,
	int addr_offset, u16 reg, u16 data);
int nss_phy_modify_pcs(struct nss_phy_device *nss_phy_device,
	int addr_offset, u16 reg, u16 mask, u16 set);
void *nss_phy_kzalloc(unsigned size);
int nss_phydev_speed_get(struct nss_phy_device *nss_phydev);
int nss_phydev_link_get(struct nss_phy_device *nss_phydev);
int nss_phydev_eee_update(struct nss_phy_device *nss_phydev, u32 adv);
int nss_phydev_autoneg_update(struct nss_phy_device *nss_phydev, u32 enable);
int nss_phy_pcs_read_mmd(struct nss_phy_device *nss_phydev,
	unsigned int addr_offset, int devad, u32 regnum);
int nss_phy_pcs_modify_mmd(struct nss_phy_device *nss_phydev,
	unsigned int addr_offset, int devad, u32 regnum, u16 mask, u16 set);
bool nss_phy_support_2500(struct nss_phy_device *nss_phydev);
bool nss_phy_support_10g(struct nss_phy_device *nss_phydev);
bool nss_phy_support_10m(struct nss_phy_device *nss_phydev);
bool nss_phy_is_fiber(struct nss_phy_device *nss_phydev);
u32 __nss_phy_read_soc(struct nss_phy_device *nss_phydev, u32 reg);
int __nss_phy_write_soc(struct nss_phy_device *nss_phydev, u32 reg, u32 val);
int __nss_phy_modify_soc(struct nss_phy_device *nss_phydev, u32 reg,
	u32 mask, u32 set);
u32 nss_phy_read_soc(struct nss_phy_device *nss_phydev, u32 reg);
int nss_phy_write_soc(struct nss_phy_device *nss_phydev, u32 reg, u32 val);
int nss_phy_modify_soc(struct nss_phy_device *nss_phydev, u32 reg,
	u32 mask, u32 set);
bool nss_phy_is_suspended(struct nss_phy_device *nss_phydev);
int nss_phydev_loopback_update(struct nss_phy_device *nss_phydev,
	u32 enable);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _NSS_PHY_LIB_H_ */
