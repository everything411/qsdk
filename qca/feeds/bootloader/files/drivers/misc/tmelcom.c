// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <command.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <time.h>
#include <asm/global_data.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <mailbox.h>
#include <misc.h>
#include <asm/arch-tegra/bpmp_abi.h>
#include <asm/arch-tegra/ivc.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/tmelcom-qmp.h>

static int tmelcom_probe(struct udevice *dev)
{
	struct tmelcom *priv = dev_get_priv(dev);
	int ret;

	ret = mbox_get_by_index(dev, 0, &priv->mbox);
	if (ret) {
		log_err("TMELCOM: Failed to get mailbox: %d\n", ret);
		return ret;
	}

	return 0;
}

static int tmelcom_remove(struct udevice *dev)
{
	struct tmelcom *priv = dev_get_priv(dev);

	mbox_free(&priv->mbox);
	return 0;
}

static const struct udevice_id tmelcom_ids[] = {
	{ .compatible = "qcom,tmelcom" },
	{ }
};

int ipq_get_tmelcom_device(struct tmelcom **tmelcom_priv)
{
	struct udevice *tmelcom_udev;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_MISC, "qcom,tmelcom",
					&tmelcom_udev);
	if (ret) {
		log_err("TMELCOM: Failed to find device node: %d\n", ret);
		return ret;
	}

	*tmelcom_priv = dev_get_priv(tmelcom_udev);
	return 0;
}

U_BOOT_DRIVER(tmelcom) = {
	.name		= "tmelcom",
	.id		= UCLASS_MISC,
	.of_match	= tmelcom_ids,
	.probe		= tmelcom_probe,
	.remove		= tmelcom_remove,
	.priv_auto	= sizeof(struct tmelcom),
	.flags  = DM_FLAG_PRE_RELOC | DM_FLAG_OS_PREPARE,
};
