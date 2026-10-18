/*
* Copyright (c) 2012, 2017, The Linux Foundation. All rights reserved.
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw.h"
#include "ssdk_init.h"
#include "fal_init.h"
#include "fal_misc.h"
#include "fal_mib.h"
#include "fal_port_ctrl.h"
#include "fal_portvlan.h"
#include "fal_fdb.h"
#include "fal_stp.h"
#include "fal_igmp.h"
#include "fal_qos.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "ssdk_init.h"
#include <linux/kconfig.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/types.h>
//#include <asm/mach-types.h>
#include <generated/autoconf.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
#include <linux/ar8216_platform.h>
#endif
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include "ssdk_plat.h"
#include "ref_vlan.h"

#if defined(IN_SWCONFIG)
int
_qca_ar8327_sw_capture_port_counter(struct switch_dev *dev, int port)
{
    fal_mib_info_t  mib_Info;
    struct qca_phy_priv *priv = qca_phy_priv_get(dev);

    memset(&mib_Info, 0, sizeof(mib_Info));
    fal_get_mib_info(priv->device_id, port, &mib_Info);

    return 0;
}

int
qca_ar8327_sw_set_reset_mibs(struct switch_dev *dev,
			 						const struct switch_attr *attr,
			 						struct switch_val *val)
{
    int i = 0;
    int len = 0;
    struct qca_phy_priv *priv = qca_phy_priv_get(dev);
    fal_mib_info_t mib_Info;
    len = dev->ports * QCA_MIB_ITEM_NUMBER *
             sizeof(*priv->mib_counters);

    mutex_lock(&priv->mib_lock);
    memset(priv->mib_counters, '\0', len);
    for (i = 0; i < dev->ports; i++)
    {
        fal_get_mib_info(priv->device_id, i, &mib_Info);
        fal_mib_port_flush_counters(priv->device_id, i);
    }
    mutex_unlock(&priv->mib_lock);

    return 0;
}

int
qca_ar8327_sw_set_port_reset_mib(struct switch_dev *dev,
			     					const struct switch_attr *attr,
			     					struct switch_val *val)
{
    int len = 0;
    struct qca_phy_priv *priv = qca_phy_priv_get(dev);
    fal_mib_info_t mib_Info;
    len = QCA_MIB_ITEM_NUMBER * sizeof(*priv->mib_counters);

    mutex_lock(&priv->mib_lock);

    memset(priv->mib_counters + (val->port_vlan * QCA_MIB_ITEM_NUMBER), '\0', len);

    fal_get_mib_info(priv->device_id, val->port_vlan, &mib_Info);
    fal_mib_port_flush_counters(priv->device_id, val->port_vlan);
    mutex_unlock(&priv->mib_lock);

    return 0;
}

static int qca_ar8327_sw_print_xgport_mib(struct switch_dev *dev,
	const struct switch_attr *attr, struct switch_val *val)
{
	int port = 0;
	int len = 0;
	fal_xgmib_info_t xgmib_info;

	struct qca_phy_priv *priv = qca_phy_priv_get(dev);
	char *buf = (char *)(priv->buf);

	port = val->port_vlan;
	if (port >= dev->ports)
	    return -EINVAL;

	fal_get_xgmib_info(priv->device_id, port, &xgmib_info);

	mutex_lock(&priv->mib_lock);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"XG Port %d MIB counters\n",
	port);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxFrame", xgmib_info.RxFrame);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxByte", xgmib_info.RxByte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxByteGood", xgmib_info.RxByteGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxBroadGood", xgmib_info.RxBroadGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxMultiGood", xgmib_info.RxMultiGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxFcsErr", xgmib_info.RxFcsErr);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxRuntErr", xgmib_info.RxRuntErr );

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxJabberError", xgmib_info.RxJabberError);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxUndersizeGood", xgmib_info.RxUndersizeGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxOversizeGood", xgmib_info.RxOversizeGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Rx64Byte", xgmib_info.Rx64Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Rx128Byte", xgmib_info.Rx128Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Rx256Byte", xgmib_info.Rx256Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Rx512Byte", xgmib_info.Rx512Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Rx1024Byte", xgmib_info.Rx1024Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxMaxByte", xgmib_info.RxMaxByte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxUnicastGood", xgmib_info.RxUnicastGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxLengthError", xgmib_info.RxLengthError);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxOutOfRangeError", xgmib_info.RxOutOfRangeError);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxPause", xgmib_info.RxPause);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxOverFlow", xgmib_info.RxOverFlow);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxVLANFrameGoodBad", xgmib_info.RxVLANFrameGoodBad);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxWatchDogError", xgmib_info.RxWatchDogError);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxLPIUsec", xgmib_info.RxLPIUsec);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxLPITran", xgmib_info.RxLPITran);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxDropFrameGoodBad", xgmib_info.RxDropFrameGoodBad);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "RxDropByteGoodBad", xgmib_info.RxDropByteGoodBad);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxByte", xgmib_info.TxByte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxFrame", xgmib_info.TxFrame);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxBroadGood", xgmib_info.TxBroadGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxMultiGood", xgmib_info.TxMultiGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Tx64Byte", xgmib_info.Tx64Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Tx128Byte", xgmib_info.Tx128Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Tx256Byte", xgmib_info.Tx256Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Tx512Byte", xgmib_info.Tx512Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "Tx1024Byte", xgmib_info.Tx1024Byte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxMaxByte", xgmib_info.TxMaxByte);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxUnicast", xgmib_info.TxUnicast);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxMulti", xgmib_info.TxMulti);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxBroad", xgmib_info.TxBroad);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxUnderFlowError", xgmib_info.TxUnderFlowError);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxByteGood", xgmib_info.TxByteGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxByteGood", xgmib_info.TxByteGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxPause", xgmib_info.TxPause);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxVLANFrameGood", xgmib_info.TxVLANFrameGood);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxLPIUsec", xgmib_info.TxLPIUsec);

	len += scnprintf(buf + len, sizeof(priv->buf) - len,
	"%-18s: %llu\n", "TxLPITran", xgmib_info.TxLPITran);

	mutex_unlock(&priv->mib_lock);

	val->value.s = buf;
	val->len = len;

	return 0;
}

int
qca_ar8327_sw_get_port_mib(struct switch_dev *dev,
		       						const struct switch_attr *attr,
		       						struct switch_val *val)
{
    int port = 0;
    int len = 0;
    int pos = 0;

    struct qca_phy_priv *priv = qca_phy_priv_get(dev);
    char *buf = (char *)(priv->buf);

    port = val->port_vlan;
    if (port >= dev->ports)
        return -EINVAL;
    if (qca_ppe_port_mac_type_get(priv->device_id, port) == PORT_XGMAC_TYPE)
    {
        qca_ar8327_sw_print_xgport_mib(dev, attr, val);
        return 0;
    }
    mutex_lock(&priv->mib_lock);
    _qca_ar8327_sw_capture_port_counter(dev, port);
    pos = port * (sizeof(fal_mib_counter_t)/sizeof(a_uint64_t));
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "Port %d MIB counters\n",
                            port);

    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxBroad",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxPause",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxMulti",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxFcsErr",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxAlignErr",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxRunt",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxFragment",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Rx64Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Rx128Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Rx256Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Rx512Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Rx1024Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Rx1518Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxMaxByte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxTooLong",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxGoodByte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxBadByte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxOverFlow",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Filtered",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxBroad",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxPause",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxMulti",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxUnderRun",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Tx64Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Tx128Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Tx256Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Tx512Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Tx1024Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "Tx1518Byte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxMaxByte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxOverSize",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxByte",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxCollision",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxAbortCol",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxMultiCol",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxSingleCol",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxExcDefer",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxDefer",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxLateCol",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "RxUniCast",
                            priv->mib_counters[pos++]);
    len += scnprintf(buf + len, sizeof(priv->buf) - len,
                            "%-12s: %llu\n",
                            "TxUniCast",
                            priv->mib_counters[pos++]);
    mutex_unlock(&priv->mib_lock);

    val->value.s = buf;
    val->len = len;

    return 0;
}
#endif

int
_qca_ar8327_sw_capture_port_tx_counter(a_uint32_t dev_id, int port)
{
    fal_mib_info_t  mib_Info;

    memset(&mib_Info, 0, sizeof(fal_mib_info_t));
    fal_get_tx_mib_info(dev_id, port, &mib_Info);

    return 0;
}

int
_qca_ar8327_sw_capture_port_rx_counter(a_uint32_t dev_id, int port)
{
    fal_mib_info_t  mib_Info;

    memset(&mib_Info, 0, sizeof(fal_mib_info_t));
    fal_get_rx_mib_info(dev_id, port, &mib_Info);
    return 0;
}

void
qca_ar8327_sw_mib_task(struct qca_phy_priv *priv)
{
	mutex_lock(&priv->reg_mutex);
	if ((priv->mib_loop_cnt % 2) == 0)
		_qca_ar8327_sw_capture_port_rx_counter(priv->device_id, priv->mib_loop_cnt/2);
	else
		_qca_ar8327_sw_capture_port_tx_counter(priv->device_id, priv->mib_loop_cnt/2);

	if(++priv->mib_loop_cnt == (2 * (priv->ports_num))) {
		priv->mib_loop_cnt = 0;
	}

	mutex_unlock(&priv->reg_mutex);

	return;
}
