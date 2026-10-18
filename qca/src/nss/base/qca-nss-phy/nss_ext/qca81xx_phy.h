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

#ifndef _QCA81XX_PHY_H_
#define _QCA81XX_PHY_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

/*Debug registers*/
#define QCA81XX_PHY_DEBUG_AFE_DAC8_DP		0x2f80
#define QCA81XX_PHY_DEBUG_AFE_DAC9_DP		0x3080

/*MMD registers*/
#define QCA81XX_PHY_MMD3_BYPASS_SIGNAL		0x2003

/*MMD registers field*/
#define QCA81XX_PHY_MMD3_PCS_BYPASS_LINK		0x200
#define QCA81XX_PHY_MMD31_AUTO_SOFT_RESET_EN		0x8

/*SERDES registers*/
#define QCA81XX_PHY_PCS_PLL_POWER_ON_AND_RESET		0x1e0
#define QCA81XX_PHY_PCS_ANA_SOFT_RESET_MASK		0x40
#define QCA81XX_PHY_PCS_ANA_SOFT_RELEASE		0x40
#define QCA81XX_PHY_PCS_ANA_SOFT_RESET		0

enum qca81xx_phy_addr_offset {
	QCA81XX_PHY_SERDES_ADDR_OFFSET = 1,
};

int qca81xx_phy_ops_init(struct nss_phy_ops *ops);

/* PPM offset clock frequency divisors for QCA81XX (Laguna) */
#define QCA81XX_PPM_DIV_1G		(768ULL << 18)
#define QCA81XX_PPM_DIV_2_5G		(1ULL << 25)
#define QCA81XX_PPM_DIV_5G_10G		(1ULL << 26)

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _qca81XX_PHY_H_ */
