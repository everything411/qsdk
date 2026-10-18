// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/soc/qcom/mdt_loader.h>
#include "../../ahb.h"
#include "ahb.h"
#include "../../debug.h"
#include "../../hif.h"
#include "hw.h"
#include "dp.h"
#include "hal.h"
#include "../../pcic.h"

static const struct of_device_id ath12k_wifi6_ahb_of_match[] = {
	{ .compatible = "qcom,qcn9160-wifi",
	  .data = (void *)ATH12K_HW_QCN9160_HW10,
	},
	{ }
};

MODULE_DEVICE_TABLE(of, ath12k_wifi6_ahb_of_match);

static const struct ath12k_ahb_ops ahb_ops_qcn9160 = {
	.mdt_load = qcom_mdt_load,
};

/* Fetch the state of the shared Q6 root PD, which is owned by the
 * IPQ5332 (wifi7) radio. QCN9160 is a userPD and its firmware can only
 * come up once that root PD is running. Returns the rproc state on
 * success, or a negative error code if the rproc handle could not be
 * obtained. The caller decides what to do with a not-running state.
 */
static int ath12k_wifi6_ahb_get_rproc_state(struct ath12k_base *ab)
{
	struct device *dev = ab->dev;
	struct device_node *np;
	struct rproc *prproc;
	enum rproc_state state;

	np = of_parse_phandle(dev->of_node, "qcom,rproc", 0);
	if (!np) {
		ath12k_err(ab, "failed to get q6_rproc handle\n");
		return -ENOENT;
	}

	prproc = rproc_get_by_phandle(np->phandle);
	of_node_put(np);
	if (!prproc) {
		ath12k_err(ab, "failed to get rproc\n");
		return RPROC_OFFLINE;
	}

	state = prproc->state;
	rproc_put(prproc);

	return state;
}


static int ath12k_wifi6_ahb_probe(struct platform_device *pdev)
{
	struct ath12k_ahb *ab_ahb;
	struct ath12k_base *ab;
	int ret;

	ab = platform_get_drvdata(pdev);
	ab_ahb = ath12k_ab_to_ahb(ab);

	switch (ab->hw_rev) {
	case ATH12K_HW_QCN9160_HW10:
		ab_ahb->scm_auth_enabled = true;
		ab_ahb->ahb_ops = &ahb_ops_qcn9160;
		break;
	default:
		return -EOPNOTSUPP;
	}

	ret = ath12k_wifi6_ahb_get_rproc_state(ab);
	if (ret < 0)
		return ret;

	if (ret != RPROC_RUNNING) {
		ath12k_err(ab, "root PD not running, deferring probe\n");
		return dev_err_probe(&pdev->dev, -EPROBE_DEFER,
			"waiting for root PD to be running\n");
	}

 	ab_ahb->window_reg_addr = PCIE_WINDOW_REG_ADDRESS;
	ret = ath12k_wifi6_hw_init(ab);
	if (ret) {
		ath12k_err(ab, "hw_init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct ath12k_reg_base ath12k_wifi6_pcic_reg_base = {
	.umac_base = HAL_SEQ_WCSS_UMAC_OFFSET,
	.ce_reg_base = HAL_CE_WFSS_CE_REG_BASE,
	.window_value_mask = WINDOW_VALUE_MASK,
	.window_static_mask = WINDOW_STATIC_MASK,
	.window_dynamic_mask = WINDOW_DYNAMIC_MASK,
	.ce_window_shift = CE_WINDOW_SHIFT,
	.umac_window_shift = UMAC_WINDOW_SHIFT,
};

static struct ath12k_ahb_driver ath12k_wifi6_ahb_driver = {
	.name = "ath12k_wifi6_ahb",
	.id_table = ath12k_wifi6_ahb_of_match,
	.ops.probe = ath12k_wifi6_ahb_probe,
	.ops.dp_init = ath12k_wifi6_dp_init,
	.ops.dp_deinit = ath12k_wifi6_dp_deinit,
	.reg_base = &ath12k_wifi6_pcic_reg_base,
};

int ath12k_wifi6_ahb_init(void)
{
	return ath12k_ahb_register_driver(ATH12K_DEVICE_FAMILY_WIFI6,
					  &ath12k_wifi6_ahb_driver);
}

void ath12k_wifi6_ahb_exit(void)
{
	ath12k_ahb_unregister_driver(ATH12K_DEVICE_FAMILY_WIFI6);
}
