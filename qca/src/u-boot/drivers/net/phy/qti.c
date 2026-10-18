// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <common.h>
#include <errno.h>
#include <phy.h>

#include "qti.h"

int phy_qti_init(void)
{
#ifdef CONFIG_PHY_QTI_8X8X
	phy_8x8x_init();
#endif
#ifdef CONFIG_PHY_QTI_8075
	phy_8075_init();
#endif
#ifdef CONFIG_PHY_QTI_8081
	phy_8081_init();
#endif
#ifdef CONFIG_PHY_QCA_81XX
	phy_81xx_init();
#endif
#ifdef CONFIG_PHY_QTI_8033
	phy_8033_init();
#endif
#ifdef CONFIG_PHY_QCA_8337
	phy_8337_init();
#endif
	return 0;
}
