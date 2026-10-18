// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <clk.h>
#include <dm.h>
#include <wdt.h>
#include <asm/io.h>
#include <linux/delay.h>

#define WDT_RST			0x4
#define WDT_EN			0x8
#define WDT_STS			0xC
#define WDT_BARK_TIME		0x10
#define WDT_BITE_TIME		0x14


struct qcom_wdt_priv {
	void *base;
	ulong rate;
};

static int qcom_wdt_reset(struct udevice *dev)
{
	struct qcom_wdt_priv *priv = dev_get_priv(dev);

        writel(BIT(0), priv->base + WDT_RST);

	return 0;
}

static int qcom_wdt_start(struct udevice *dev, u64 timeout_ms, ulong flags)
{
	struct qcom_wdt_priv *priv = dev_get_priv(dev);
	ulong bark_timeout_s = ((timeout_ms - 1)  * priv->rate) / 1000;
	ulong bite_timeout_s = (timeout_ms * priv->rate) / 1000;

	writel(0, priv->base + WDT_EN);
	writel(BIT(0), priv->base + WDT_RST);
	writel(bark_timeout_s, priv->base + WDT_BARK_TIME);
	writel(bite_timeout_s, priv->base +WDT_BITE_TIME);
	writel(BIT(0), priv->base + WDT_EN);

	return 0;
}

static int qcom_wdt_stop(struct udevice *dev)
{
	struct qcom_wdt_priv *priv = dev_get_priv(dev);

	writel(0, priv->base + WDT_EN);
	writel(BIT(0), priv->base + WDT_RST);
	writel(0x7ffff, priv->base + WDT_BARK_TIME);
	writel(0xfffff, priv->base + WDT_BITE_TIME);

	return 0;
}

static int qcom_wdt_expire_now(struct udevice *dev, ulong flags)
{
	/*
	 * setup 125ms for immediate trigger wdt
	 */
	struct qcom_wdt_priv *priv = dev_get_priv(dev);
	ulong bark_timeout_s = ((125 - 1)  * priv->rate) / 1000;
	ulong bite_timeout_s = (125 * priv->rate) / 1000;

	writel(0, priv->base + WDT_EN);
	writel(BIT(0), priv->base + WDT_RST);
	writel(bark_timeout_s, priv->base + WDT_BARK_TIME);
	writel(bite_timeout_s, priv->base +WDT_BITE_TIME);
	writel(BIT(0), priv->base + WDT_EN);

	return 0;
}

static const struct wdt_ops qcom_wdt_ops = {
	.reset = qcom_wdt_reset,
	.start = qcom_wdt_start,
	.stop = qcom_wdt_stop,
	.expire_now = qcom_wdt_expire_now,
};

static const struct udevice_id qcom_wdt_ids[] = {
	{ .compatible = "qcom,kpss-wdt" },
	{ }
};

static int qcom_wdt_probe(struct udevice *dev)
{
	struct qcom_wdt_priv *priv = dev_get_priv(dev);
	struct clk clk;
	int ret;

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;

	/* clk enable by default */
	priv->rate = clk_get_rate(&clk);

	return 0;
}

U_BOOT_DRIVER(qcom_wdt) = {
	.name = "qcom_wdt",
	.id = UCLASS_WDT,
	.of_match = qcom_wdt_ids,
	.priv_auto = sizeof(struct qcom_wdt_priv),
	.probe = qcom_wdt_probe,
	.ops = &qcom_wdt_ops,
};
