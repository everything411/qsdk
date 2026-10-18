// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Inspired by dwc3-of-simple.c
 */

#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/of_clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/extcon.h>
#include <linux/interconnect.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/usb/dwc3-qcom.h>
#include <linux/usb/of.h>
#include <linux/reset.h>
#include <linux/iopoll.h>
#include <linux/usb/hcd.h>
#include <linux/usb.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "core.h"
#include "gadget.h"

/* USB QSCRATCH Hardware registers */
#define QSCRATCH_HS_PHY_CTRL			0x10
#define UTMI_OTG_VBUS_VALID			BIT(20)
#define SW_SESSVLD_SEL				BIT(28)

#define QSCRATCH_SS_PHY_CTRL			0x30
#define LANE0_PWR_PRESENT			BIT(24)

#define QSCRATCH_GENERAL_CFG			0x08
#define PIPE_UTMI_CLK_SEL			BIT(0)
#define PIPE3_PHYSTATUS_SW			BIT(3)
#define PIPE_UTMI_CLK_DIS			BIT(8)

#define PWR_EVNT_IRQ_STAT_REG			0x58
#define PWR_EVNT_LPM_IN_L2_MASK			BIT(4)
#define PWR_EVNT_LPM_OUT_L2_MASK		BIT(5)

#define SDM845_QSCRATCH_BASE_OFFSET		0xf8800
#define SDM845_QSCRATCH_SIZE			0x400
#define SDM845_DWC3_CORE_SIZE			0xcd00

/* EBC/LPC Configuration */
#define LPC_SCAN_MASK				0x1C8
#define LPC_REG					0x1CC

#define LPC_SPEED_INDICATOR			BIT(0)
#define LPC_SSP_MODE				BIT(1)
#define LPC_BUS_CLK_EN				BIT(12)

#define USB30_MODE_SEL_REG			0x1D4
#define USB30_QDSS_MODE_SEL			BIT(0)
#define USB30_QDSS_CONFIG_REG			0x1D8

#define DWC3_DEPCFG_EBC_MODE			BIT(15)
#define DWC3_DEPCFG_RETRY			BIT(15)
#define DWC3_DEPCFG_TRB_WB			BIT(14)

/* Interconnect path bandwidths in MBps */
#define USB_MEMORY_AVG_HS_BW MBps_to_icc(240)
#define USB_MEMORY_PEAK_HS_BW MBps_to_icc(700)
#define USB_MEMORY_AVG_SS_BW  MBps_to_icc(1000)
#define USB_MEMORY_PEAK_SS_BW MBps_to_icc(2500)
#define APPS_USB_AVG_BW 0
#define APPS_USB_PEAK_BW MBps_to_icc(40)

struct dwc3_acpi_pdata {
	u32			qscratch_base_offset;
	u32			qscratch_base_size;
	u32			dwc3_core_base_size;
	int			hs_phy_irq_index;
	int			dp_hs_phy_irq_index;
	int			dm_hs_phy_irq_index;
	int			ss_phy_irq_index;
	bool			is_urs;
};

struct dwc3_hw_ep {
	struct dwc3_ep		*dep;
	enum usb_hw_ep_mode	mode;
	struct dwc3_trb		*ebc_trb_pool;
	u8 dbm_ep_num;
	int num_trbs;

	unsigned long flags;
#define DWC3_QCOM_HW_EP_TRANSFER_STARTED BIT(0)
};

struct dwc3_qcom_req_complete {
	struct list_head list_item;
	struct usb_request *req;
	void (*orig_complete)(struct usb_ep *ep,
			      struct usb_request *req);
};

struct dwc3_qcom {
	struct device		*dev;
	void __iomem		*qscratch_base;
	struct platform_device	*dwc3;
	struct platform_device	*urs_usb;
	struct clk		**clks;
	int			num_clocks;
	struct reset_control	*resets;

	int			hs_phy_irq;
	int			dp_hs_phy_irq;
	int			dm_hs_phy_irq;
	int			ss_phy_irq;
	enum usb_device_speed	usb2_speed;

	struct extcon_dev	*edev;
	struct extcon_dev	*host_edev;
	struct notifier_block	vbus_nb;
	struct notifier_block	host_nb;

	const struct dwc3_acpi_pdata *acpi_pdata;

	enum usb_dr_mode	mode;
	bool			is_suspended;
	bool			pm_suspended;
	bool			phy_mux;
	struct regmap		*phy_mux_map;
	u32			phy_mux_reg;
	struct icc_path		*icc_path_ddr;
	struct icc_path		*icc_path_apps;
	struct dwc3_hw_ep	hw_eps[DWC3_ENDPOINTS_NUM];
	struct dwc3_trb		*ebc_desc_addr;
	const struct usb_ep_ops	*original_ep_ops[DWC3_ENDPOINTS_NUM];
	struct list_head	req_complete_list;
};

static inline void dwc3_qcom_setbits(void __iomem *base, u32 offset, u32 val)
{
	u32 reg;

	reg = readl(base + offset);
	reg |= val;
	writel(reg, base + offset);

	/* ensure that above write is through */
	readl(base + offset);
}

static inline void dwc3_qcom_clrbits(void __iomem *base, u32 offset, u32 val)
{
	u32 reg;

	reg = readl(base + offset);
	reg &= ~val;
	writel(reg, base + offset);

	/* ensure that above write is through */
	readl(base + offset);
}

static inline void dwc3_qcom_ep_writel(void __iomem *base, u32 offset, u32 value)
{
	/*
	 * We requested the mem region starting from the Globals address
	 * space, see dwc3_probe in core.c.
	 * However, the offsets are given starting from xHCI address space.
	 */
	writel_relaxed(value, base + offset - DWC3_GLOBALS_REGS_START);

	/* Ensure writes to DWC3 ep registers are completed */
	mb();
}

static inline u32 dwc3_qcom_ep_readl(void __iomem *base, u32 offset)
{
	/*
	 * We requested the mem region starting from the Globals address
	 * space, see dwc3_probe in core.c.
	 * However, the offsets are given starting from xHCI address space.
	 */
	return readl_relaxed(base + offset - DWC3_GLOBALS_REGS_START);
}

/**
 *
 * Read register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 *
 * @return u32
 */
static inline u32 dwc3_qcom_read_reg(void __iomem *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	return val;
}

/**
 *
 * Write register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @val - value to write.
 *
 */
static inline void dwc3_qcom_write_reg(void __iomem *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}

static int qcom_ep_setup_ebc_trbs(struct usb_ep *ep, struct usb_request *req)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_hw_ep *edep;
	struct dwc3_trb *trb;
	u32 desc_offset = 0, scan_offset = 0x4000;
	int i, num_trbs;

	if (!mdwc->ebc_desc_addr) {
		dev_err(mdwc->dev, "%s: ebc_desc_addr not specified\n", __func__);
		return -EINVAL;
	}

	if (!dep->direction) {
		desc_offset = 0x200;
		scan_offset = 0x8000;
	}

	edep = &mdwc->hw_eps[dep->number];
	edep->ebc_trb_pool = mdwc->ebc_desc_addr + desc_offset;
	num_trbs = req->length / EBC_TRB_SIZE;
	mdwc->hw_eps[dep->number].num_trbs = num_trbs;

	for (i = 0; i < num_trbs; i++) {
		struct dwc3_trb tmp;

		trb = &edep->ebc_trb_pool[i];
		memset(trb, 0, sizeof(*trb));

		/* Setup n TRBs pointing to valid buffers */
		tmp.bpl = scan_offset;
		tmp.bph = 0x8000;
		tmp.size = EBC_TRB_SIZE;
		tmp.ctrl = DWC3_TRBCTL_NORMAL | DWC3_TRB_CTRL_CHN |
				DWC3_TRB_CTRL_HWO;
		if (i == (num_trbs-1)) {
			tmp.bpl = desc_offset;
			tmp.bph = 0x8000;
			tmp.size = 0;
			tmp.ctrl = DWC3_TRBCTL_LINK_TRB | DWC3_TRB_CTRL_HWO;
		}
		memcpy(trb, &tmp, sizeof(*trb));
		scan_offset += trb->size;
	}

	return 0;
}

static int ebc_ep_config(struct usb_ep *ep, struct usb_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	u32 reg, ep_num;
	int ret;

	reg = dwc3_qcom_read_reg(mdwc->qscratch_base, LPC_REG);

	switch (dwc3_qcom_ep_readl(dwc->regs, DWC3_DSTS) & DWC3_DSTS_CONNECTSPD) {
	case DWC3_DSTS_SUPERSPEED_PLUS:
		reg |= LPC_SSP_MODE;
		break;
	case DWC3_DSTS_SUPERSPEED:
		reg |= LPC_SPEED_INDICATOR;
		break;
	default:
		reg &= ~(LPC_SSP_MODE | LPC_SPEED_INDICATOR);
		break;
	}

	dwc3_qcom_write_reg(mdwc->qscratch_base, LPC_REG, reg);
	ret = qcom_ep_setup_ebc_trbs(ep, request);
	if (ret < 0) {
		dev_err(mdwc->dev, "error %d setting up ebc trbs\n", ret);
		return ret;
	}

	ep_num = !dep->direction ? dep->number + 15 :
				   dep->number >> 1;
	reg = dwc3_qcom_read_reg(mdwc->qscratch_base, LPC_SCAN_MASK);
	reg |= BIT(ep_num);
	dwc3_qcom_write_reg(mdwc->qscratch_base, LPC_SCAN_MASK, reg);

	reg = dwc3_qcom_read_reg(mdwc->qscratch_base, LPC_REG);
	reg |= LPC_BUS_CLK_EN;
	dwc3_qcom_write_reg(mdwc->qscratch_base, LPC_REG, reg);

	reg = dwc3_qcom_read_reg(mdwc->qscratch_base, USB30_MODE_SEL_REG);
	reg |= USB30_QDSS_MODE_SEL;
	dwc3_qcom_write_reg(mdwc->qscratch_base, USB30_MODE_SEL_REG, reg);

	return 0;
}

/**
 * Configure QCOM endpoint.
 * This function do specific configurations
 * to an endpoint which need specific implementaion
 * in the QCOM architecture.
 *
 * This function should be called by usb function/class
 * layer which need a support from the specific QCOM HW
 * which wrap the USB3 core. (like EBC or DBM specific endpoints)
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negetive on error.
 */
int qcom_ep_config(struct usb_ep *ep, struct usb_request *request, u32 bam_opts)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&dwc->lock, flags);

	if (mdwc->hw_eps[dep->number].mode == USB_EP_EBC) {
		ret = ebc_ep_config(ep, request);
		if (ret < 0) {
			dev_err(mdwc->dev,
				"error %d after calling ebc_ep_config\n", ret);
			spin_unlock_irqrestore(&dwc->lock, flags);
			return ret;
		}
	}

	mdwc->hw_eps[dep->number].dep = dep;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_ep_config);

/**
 * Un-configure QCOM endpoint.
 * Tear down configurations done in the
 * dwc3_qcom_ep_config function.
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negative on error.
 */
int qcom_ep_unconfig(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	unsigned long flags;
	u32 reg, ep_num;

	spin_lock_irqsave(&dwc->lock, flags);
	if (mdwc->hw_eps[dep->number].mode == USB_EP_EBC) {
		ep_num = !dep->direction ? dep->number + 15 :
					   dep->number >> 1;
		reg = dwc3_qcom_read_reg(mdwc->qscratch_base, LPC_SCAN_MASK);
		reg &= ~BIT(ep_num);
		dwc3_qcom_write_reg(mdwc->qscratch_base, LPC_SCAN_MASK, reg);

		dwc3_qcom_write_reg(mdwc->qscratch_base, LPC_SCAN_MASK, 0);
		reg = dwc3_qcom_read_reg(mdwc->qscratch_base, LPC_REG);
		reg &= ~LPC_BUS_CLK_EN;

		dwc3_qcom_write_reg(mdwc->qscratch_base, LPC_REG, reg);
	}

	mdwc->hw_eps[dep->number].dep = 0;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_ep_unconfig);

/**
 * qcom_ep_clear_ops - Restore default endpoint operations
 * @ep: The endpoint to restore
 *
 * Resets the usb endpoint operations to the default callbacks previously saved
 * when calling qcom_ep_update_ops.
 */
int qcom_ep_clear_ops(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_ep_ops *old_ep_ops;
	unsigned long flags;

	spin_lock_irqsave(&dwc->lock, flags);

	/* Restore original ep ops */
	if (!mdwc->original_ep_ops[dep->number]) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		dev_err(mdwc->dev,
			"ep [%s,%d] was not configured as qcom endpoint\n",
			ep->name, dep->number);
		return -EINVAL;
	}
	old_ep_ops = (struct usb_ep_ops *)ep->ops;
	ep->ops = mdwc->original_ep_ops[dep->number];
	mdwc->original_ep_ops[dep->number] = NULL;
	kfree(old_ep_ops);

	spin_unlock_irqrestore(&dwc->lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_ep_clear_ops);

static int dwc3_core_send_gadget_ep_cmd(struct dwc3_ep *dep, unsigned int cmd,
		struct dwc3_gadget_ep_cmd_params *params)
{
	const struct usb_endpoint_descriptor *desc = dep->endpoint.desc;
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	u32 timeout = 5000;
	u32 saved_config = 0;
	u32 reg;
	int cmd_status = 0;
	int ret = -EINVAL;

	/*
	 * When operating in USB 2.0 speeds (HS/FS), if GUSB2PHYCFG.ENBLSLPM or
	 * GUSB2PHYCFG.SUSPHY is set, it must be cleared before issuing an
	 * endpoint command.
	 *
	 * Save and clear both GUSB2PHYCFG.ENBLSLPM and GUSB2PHYCFG.SUSPHY
	 * settings. Restore them after the command is completed.
	 *
	 * DWC_usb3 3.30a and DWC_usb31 1.90a programming guide section 3.2.2
	 */
	if (dwc->gadget->speed <= USB_SPEED_HIGH) {
		reg = dwc3_qcom_ep_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		if (unlikely(reg & DWC3_GUSB2PHYCFG_SUSPHY)) {
			saved_config |= DWC3_GUSB2PHYCFG_SUSPHY;
			reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;
		}

		if (reg & DWC3_GUSB2PHYCFG_ENBLSLPM) {
			saved_config |= DWC3_GUSB2PHYCFG_ENBLSLPM;
			reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;
		}

		if (saved_config) {
			dwc3_qcom_ep_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
		}
	}

	dwc3_qcom_ep_writel(dep->regs, DWC3_DEPCMDPAR0, params->param0);
	dwc3_qcom_ep_writel(dep->regs, DWC3_DEPCMDPAR1, params->param1);
	dwc3_qcom_ep_writel(dep->regs, DWC3_DEPCMDPAR2, params->param2);

	/*
	 * Synopsys Databook 2.60a states in section 6.3.2.5.6 of that if we're
	 * not relying on XferNotReady, we can make use of a special "No
	 * Response Update Transfer" command where we should clear both CmdAct
	 * and CmdIOC bits.
	 *
	 * With this, we don't need to wait for command completion and can
	 * straight away issue further commands to the endpoint.
	 *
	 * NOTICE: We're making an assumption that control endpoints will never
	 * make use of Update Transfer command. This is a safe assumption
	 * because we can never have more than one request at a time with
	 * Control Endpoints. If anybody changes that assumption, this chunk
	 * needs to be updated accordingly.
	 */
	if (DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_UPDATETRANSFER &&
			!usb_endpoint_xfer_isoc(desc))
		cmd &= ~(DWC3_DEPCMD_CMDIOC | DWC3_DEPCMD_CMDACT);
	else
		cmd |= DWC3_DEPCMD_CMDACT;

	dwc3_qcom_ep_writel(dep->regs, DWC3_DEPCMD, cmd);
	do {
		reg = dwc3_qcom_ep_readl(dep->regs, DWC3_DEPCMD);
		if (!(reg & DWC3_DEPCMD_CMDACT)) {
			cmd_status = DWC3_DEPCMD_STATUS(reg);

			switch (cmd_status) {
			case 0:
				ret = 0;
				break;
			case DEPEVT_TRANSFER_NO_RESOURCE:
				dev_WARN(dwc->dev, "No resource for %s\n",
					 dep->name);
				ret = -EINVAL;
				break;
			case DEPEVT_TRANSFER_BUS_EXPIRY:
				/*
				 * SW issues START TRANSFER command to
				 * isochronous ep with future frame interval. If
				 * future interval time has already passed when
				 * core receives the command, it will respond
				 * with an error status of 'Bus Expiry'.
				 *
				 * Instead of always returning -EINVAL, let's
				 * give a hint to the gadget driver that this is
				 * the case by returning -EAGAIN.
				 */
				ret = -EAGAIN;
				break;
			default:
				dev_WARN(dwc->dev, "UNKNOWN cmd status\n");
			}

			break;
		}
	} while (--timeout);

	if (timeout == 0) {
		ret = -ETIMEDOUT;
		cmd_status = -ETIMEDOUT;
	}

	if (DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_STARTTRANSFER) {
		if (ret == 0) {
			if (mdwc->hw_eps[dep->number].mode == USB_EP_GSI)
				mdwc->hw_eps[dep->number].flags |=
					DWC3_QCOM_HW_EP_TRANSFER_STARTED;
			else
				dep->flags |= DWC3_EP_TRANSFER_STARTED;
		}

		if (ret != -ETIMEDOUT) {
			u32 res_id;

			res_id = dwc3_qcom_ep_readl(dep->regs, DWC3_DEPCMD);
			dep->resource_index = DWC3_DEPCMD_GET_RSC_IDX(res_id);
		}
	}

	if (saved_config) {
		reg = dwc3_qcom_ep_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		reg |= saved_config;
		dwc3_qcom_ep_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
	}

	return ret;
}

/**
 * dwc3_qcom_depcfg_params - Set depcfg parameters for QCOM eps
 * @ep: Endpoint being configured
 * @params: depcmd param being passed to the controller
 *
 * Initializes the dwc3_gadget_ep_cmd_params structure being passed as part of
 * the depcfg command.  This API is explicitly used for initializing the params
 * for QCOM specific HW endpoints.
 *
 * Supported EP types:
 * - USB GSI
 * - USB BAM
 * - USB EBC
 */
static void dwc3_qcom_depcfg_params(struct usb_ep *ep, struct dwc3_gadget_ep_cmd_params *params)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	const struct usb_endpoint_descriptor *desc = ep->desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc = ep->comp_desc;

	params->param0 = DWC3_DEPCFG_EP_TYPE(usb_endpoint_type(desc))
		| DWC3_DEPCFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	/* Burst size is only needed in SuperSpeed mode */
	if (dwc->gadget->speed >= USB_SPEED_SUPER) {
		u32 burst = dep->endpoint.maxburst;

		params->param0 |= DWC3_DEPCFG_BURST_SIZE(burst - 1);
	}

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		params->param1 |= DWC3_DEPCFG_STREAM_CAPABLE
					| DWC3_DEPCFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	/* Set EP number */
	params->param1 |= DWC3_DEPCFG_EP_NUMBER(dep->number);
	if (dep->direction)
		params->param0 |= DWC3_DEPCFG_FIFO_NUMBER(dep->number >> 1);

	params->param0 |= DWC3_DEPCFG_ACTION_INIT;

	if (mdwc->hw_eps[dep->number].mode == USB_EP_EBC) {
		params->param1 |= DWC3_DEPCFG_RETRY | DWC3_DEPCFG_TRB_WB;
		params->param0 |= DWC3_DEPCFG_EBC_MODE;
	}
}

static int dwc3_qcom_set_ep_config(struct dwc3_ep *dep, unsigned int action)
{
	const struct usb_ss_ep_comp_descriptor *comp_desc;
	const struct usb_endpoint_descriptor *desc;
	struct dwc3_gadget_ep_cmd_params params;
	struct usb_ep *ep = &dep->endpoint;

	comp_desc = dep->endpoint.comp_desc;
	desc = dep->endpoint.desc;

	memset(&params, 0x00, sizeof(params));
	dwc3_qcom_depcfg_params(ep, &params);

	return dwc3_core_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETEPCONFIG, &params);
}

/**
 * dwc3_core_calc_tx_fifo_size - calculates the txfifo size value
 * @dwc: pointer to the DWC3 context
 * @nfifos: number of fifos to calculate for
 *
 * Calculates the size value based on the equation below:
 *
 * fifo_size = mult * ((max_packet + mdwidth)/mdwidth + 1) + 1
 *
 * The max packet size is set to 1024, as the txfifo requirements mainly apply
 * to super speed USB use cases.  However, it is safe to overestimate the fifo
 * allocations for other scenarios, i.e. high speed USB.
 */
static int dwc3_core_calc_tx_fifo_size(struct dwc3 *dwc, int mult)
{
	int max_packet = 1024;
	int fifo_size;
	int mdwidth;

	mdwidth = dwc3_mdwidth(dwc);

	/* MDWIDTH is represented in bits, we need it in bytes */
	mdwidth >>= 3;

	fifo_size = mult * ((max_packet + mdwidth) / mdwidth) + 1;
	return fifo_size;
}

/*
 * dwc3_core_resize_tx_fifos - reallocate fifo spaces for current use-case
 * @dwc: pointer to our context structure
 *
 * This function will a best effort FIFO allocation in order
 * to improve FIFO usage and throughput, while still allowing
 * us to enable as many endpoints as possible.
 *
 * Keep in mind that this operation will be highly dependent
 * on the configured size for RAM1 - which contains TxFifo -,
 * the amount of endpoints enabled on coreConsultant tool, and
 * the width of the Master Bus.
 *
 * In general, FIFO depths are represented with the following equation:
 *
 * fifo_size = mult * ((max_packet + mdwidth)/mdwidth + 1) + 1
 *
 * Conversions can be done to the equation to derive the number of packets that
 * will fit to a particular FIFO size value.
 */
static int dwc3_core_resize_tx_fifos(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	int fifo_0_start;
	int ram1_depth;
	int fifo_size;
	int min_depth;
	int num_in_ep;
	int remaining;
	int num_fifos = 1;
	int fifo;
	int tmp;

	if (!dwc->do_fifo_resize)
		return 0;

	/* resize IN endpoints except ep0 */
	if (!usb_endpoint_dir_in(dep->endpoint.desc) || dep->number <= 1)
		return 0;

	ram1_depth = DWC3_RAM1_DEPTH(dwc->hwparams.hwparams7);

	if ((dep->endpoint.maxburst > 1 &&
	     usb_endpoint_xfer_bulk(dep->endpoint.desc)) ||
	    usb_endpoint_xfer_isoc(dep->endpoint.desc))
		num_fifos = 3;

	if (dep->endpoint.maxburst > 6 &&
	    usb_endpoint_xfer_bulk(dep->endpoint.desc) && DWC3_IP_IS(DWC31))
		num_fifos = dwc->tx_fifo_resize_max_num;

	/* FIFO size for a single buffer */
	fifo = dwc3_core_calc_tx_fifo_size(dwc, 1);

	/* Calculate the number of remaining EPs w/o any FIFO */
	num_in_ep = dwc->max_cfg_eps;
	num_in_ep -= dwc->num_ep_resized;

	/* Reserve at least one FIFO for the number of IN EPs */
	min_depth = num_in_ep * (fifo + 1);
	remaining = ram1_depth - min_depth - dwc->last_fifo_depth;
	remaining = max_t(int, 0, remaining);
	/*
	 * We've already reserved 1 FIFO per EP, so check what we can fit in
	 * addition to it.  If there is not enough remaining space, allocate
	 * all the remaining space to the EP.
	 */
	fifo_size = (num_fifos - 1) * fifo;
	if (remaining < fifo_size)
		fifo_size = remaining;

	fifo_size += fifo;
	/* Last increment according to the TX FIFO size equation */
	fifo_size++;

	/* Check if TXFIFOs start at non-zero addr */
	tmp = dwc3_qcom_ep_readl(dwc->regs, DWC3_GTXFIFOSIZ(0));
	fifo_0_start = DWC3_GTXFIFOSIZ_TXFSTADDR(tmp);

	fifo_size |= (fifo_0_start + (dwc->last_fifo_depth << 16));
	if (DWC3_IP_IS(DWC3))
		dwc->last_fifo_depth += DWC3_GTXFIFOSIZ_TXFDEP(fifo_size);
	else
		dwc->last_fifo_depth += DWC31_GTXFIFOSIZ_TXFDEP(fifo_size);

	/* Check fifo size allocation doesn't exceed available RAM size. */
	if (dwc->last_fifo_depth >= ram1_depth) {
		dev_err(dwc->dev, "Fifosize(%d) > RAM size(%d) %s depth:%d\n",
			dwc->last_fifo_depth, ram1_depth,
			dep->endpoint.name, fifo_size);
		if (DWC3_IP_IS(DWC3))
			fifo_size = DWC3_GTXFIFOSIZ_TXFDEP(fifo_size);
		else
			fifo_size = DWC31_GTXFIFOSIZ_TXFDEP(fifo_size);

		dwc->last_fifo_depth -= fifo_size;
		return -ENOMEM;
	}

	dwc3_qcom_ep_writel(dwc->regs, DWC3_GTXFIFOSIZ(dep->number >> 1), fifo_size);
	dwc->num_ep_resized++;

	return 0;
}

static inline dma_addr_t dwc3_trb_dma_offset(struct dwc3_ep *dep,
		struct dwc3_trb *trb)
{
	u32 offset = (char *) trb - (char *) dep->trb_pool;

	return dep->trb_pool_dma + offset;
}

static int __dwc3_qcom_ep_enable(struct dwc3_ep *dep, unsigned int action)
{
	struct dwc3 *dwc = dep->dwc;
	const struct usb_endpoint_descriptor *desc = dep->endpoint.desc;
	u32 reg;
	int ret;

	ret = dwc3_qcom_set_ep_config(dep, action);
	if (ret) {
		dev_err(dwc->dev, "set_ep_config() failed for %s\n", dep->name);
		return ret;
	}

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		struct dwc3_trb	*trb_st_hw;
		struct dwc3_trb	*trb_link;

		dwc3_core_resize_tx_fifos(dep);

		dep->type = usb_endpoint_type(desc);
		dep->flags |= DWC3_EP_ENABLED;

		reg = dwc3_qcom_ep_readl(dwc->regs, DWC3_DALEPENA);
		reg |= DWC3_DALEPENA_EP(dep->number);
		dwc3_qcom_ep_writel(dwc->regs, DWC3_DALEPENA, reg);

		/* Initialize the TRB ring */
		dep->trb_dequeue = 0;
		dep->trb_enqueue = 0;
		memset(dep->trb_pool, 0,
		       sizeof(struct dwc3_trb) * DWC3_TRB_NUM);

		/* Link TRB. The HWO bit is never reset */
		trb_st_hw = &dep->trb_pool[0];

		trb_link = &dep->trb_pool[DWC3_TRB_NUM - 1];
		trb_link->bpl = lower_32_bits(dwc3_trb_dma_offset(dep, trb_st_hw));
		trb_link->bph = upper_32_bits(dwc3_trb_dma_offset(dep, trb_st_hw));
		trb_link->ctrl |= DWC3_TRBCTL_LINK_TRB;
		trb_link->ctrl |= DWC3_TRB_CTRL_HWO;
	}

	return 0;
}

static int dwc3_qcom_ep_enable(struct usb_ep *ep,
			      const struct usb_endpoint_descriptor *desc)
{
	struct dwc3_ep *dep;
	struct dwc3 *dwc;
	struct dwc3_qcom *mdwc;
	unsigned long flags;
	int ret;

	if (!ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT) {
		pr_debug("dwc3: invalid parameters\n");
		return -EINVAL;
	}

	if (!desc->wMaxPacketSize) {
		pr_debug("dwc3: missing wMaxPacketSize\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;
	mdwc = dev_get_drvdata(dwc->dev->parent);

	if (dev_WARN_ONCE(dwc->dev, dep->flags & DWC3_EP_ENABLED,
					"%s is already enabled\n",
					dep->name))
		return 0;

	if (pm_runtime_suspended(dwc->sysdev)) {
		dev_err(dwc->dev, "fail ep_enable %s device is into LPM\n",
					dep->name);
		return -EINVAL;
	}

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_qcom_ep_enable(dep, DWC3_DEPCFG_ACTION_INIT);
	/*dbg_event(dep->number, "ENABLE", ret);*/
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int __dwc3_qcom_ebc_ep_queue(struct dwc3_ep *dep, struct dwc3_request *req)
{
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd, param1;
	int ret = 0;

	req->status = DWC3_REQUEST_STATUS_STARTED;
	req->num_trbs++;
	dep->trb_enqueue++;
	list_add_tail(&req->list, &dep->started_list);
	if (dep->direction)
		param1 = 0x0;
	else
		param1 = 0x200;

	/* Now start the transfer */
	memset(&params, 0, sizeof(params));
	params.param0 = 0x8000; /* TDAddr High */
	params.param1 = param1; /* DAddr Low */

	cmd = DWC3_DEPCMD_STARTTRANSFER;
	ret = dwc3_core_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret < 0) {
		dev_dbg(dep->dwc->dev,
			"%s: failed to send STARTTRANSFER command\n",
			__func__);

		list_del(&req->list);
		return ret;
	}

	return ret;
}

/**
 * Cleanups for qcom endpoint on request complete.
 *
 * Also call original request complete.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to usb_request instance.
 *
 * @return int - 0 on success, negative on error.
 */
static void dwc3_qcom_req_complete_func(struct usb_ep *ep,
				       struct usb_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_qcom_req_complete *req_complete = NULL;

	/* Find original request complete function and remove it from list */
	list_for_each_entry(req_complete, &mdwc->req_complete_list, list_item) {
		if (req_complete->req == request)
			break;
	}
	if (!req_complete || req_complete->req != request) {
		dev_err(dep->dwc->dev, "%s: could not find the request\n",
					__func__);
		return;
	}
	list_del(&req_complete->list_item);

	/*
	 * Call original complete function, notice that dwc->lock is already
	 * taken by the caller of this function (dwc3_gadget_giveback()).
	 */
	request->complete = req_complete->orig_complete;
	if (request->complete)
		request->complete(ep, request);

	kfree(req_complete);
}

/**
 * Queue a usb request to the DBM endpoint.
 * This function should be called after the endpoint
 * was enabled by the ep_enable.
 *
 * This function prepares special structure of TRBs which
 * is familiar with the DBM HW, so it will possible to use
 * this endpoint in DBM mode.
 *
 * The TRBs prepared by this function, is one normal TRB
 * which point to a fake buffer, followed by a link TRB
 * that points to the first TRB.
 *
 * The API of this function follow the regular API of
 * usb_ep_queue (see usb_ep_ops in include/linuk/usb/gadget.h).
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to usb_request instance.
 * @gfp_flags - possible flags.
 *
 * @return int - 0 on success, negative on error.
 */
static int dwc3_qcom_ep_queue(struct usb_ep *ep,
			     struct usb_request *request, gfp_t gfp_flags)
{
	struct dwc3_request *req = to_dwc3_request(request);
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_qcom_req_complete *req_complete;
	unsigned long flags;
	int ret = 0;

	/*
	 * We must obtain the lock of the dwc3 core driver,
	 * including disabling interrupts, so we will be sure
	 * that we are the only ones that configure the HW device
	 * core and ensure that we queuing the request will finish
	 * as soon as possible so we will release back the lock.
	 */
	spin_lock_irqsave(&dwc->lock, flags);
	if (!dep->endpoint.desc) {
		dev_err(mdwc->dev,
			"%s: trying to queue request %pK to disabled ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (!mdwc->original_ep_ops[dep->number]) {
		dev_err(mdwc->dev,
			"ep [%s,%d] was unconfigured as qcom endpoint\n",
			ep->name, dep->number);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	if (!request) {
		dev_err(mdwc->dev, "%s: request is NULL\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	/* HW restriction regarding TRB size (8KB) */
	if (mdwc->hw_eps[dep->number].mode == USB_EP_BAM && req->request.length < 0x2000) {
		dev_err(mdwc->dev, "%s: Min TRB size is 8KB\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	if (dep->number == 0 || dep->number == 1) {
		dev_err(mdwc->dev,
			"%s: trying to queue dbm request %pK to ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (dep->trb_dequeue != dep->trb_enqueue
					|| !list_empty(&dep->pending_list)
					|| !list_empty(&dep->started_list)) {
		dev_err(mdwc->dev,
			"%s: trying to queue dbm request %pK tp ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}
	dep->trb_dequeue = 0;
	dep->trb_enqueue = 0;

	/*
	 * Override req->complete function, but before doing that,
	 * store it's original pointer in the req_complete_list.
	 */
	req_complete = kzalloc(sizeof(*req_complete), gfp_flags);

	if (!req_complete) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -ENOMEM;
	}

	req_complete->req = request;
	req_complete->orig_complete = request->complete;
	list_add_tail(&req_complete->list_item, &mdwc->req_complete_list);
	request->complete = dwc3_qcom_req_complete_func;

	dev_vdbg(dwc->dev, "%s: queuing request %pK to ep %s length %d\n",
			__func__, request, ep->name, request->length);

	if (mdwc->hw_eps[dep->number].mode == USB_EP_EBC)
		ret = __dwc3_qcom_ebc_ep_queue(dep, req);
	if (ret < 0) {
		dev_err(mdwc->dev,
			"error %d after queuing %s req\n", ret,
			mdwc->hw_eps[dep->number].mode == USB_EP_EBC ? "ebc" : "dbm");
		goto err;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;

err:
	spin_unlock_irqrestore(&dwc->lock, flags);
	kfree(req_complete);
	return ret;
}

/**
 * qcom_ep_update_ops - Override default USB ep ops w/ QCOM specific ops
 * @ep: The endpoint to override
 *
 * Replaces the default endpoint operations with QCOM specific operations for
 * handling HW based endpoints, such as DBM or EBC eps.  This does not depend
 * on calling qcom_ep_config beforehand.
 */
int qcom_ep_update_ops(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_ep_ops *new_ep_ops;
	unsigned long flags;

	spin_lock_irqsave(&dwc->lock, flags);

	/* Save original ep ops for future restore*/
	if (mdwc->original_ep_ops[dep->number]) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		dev_err(mdwc->dev,
			"ep [%s,%d] already configured as qcom endpoint\n",
			ep->name, dep->number);
		return -EPERM;
	}
	mdwc->original_ep_ops[dep->number] = ep->ops;

	/* Set new usb ops as we like */
	new_ep_ops = kzalloc(sizeof(struct usb_ep_ops), GFP_ATOMIC);
	if (!new_ep_ops) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -ENOMEM;
	}

	(*new_ep_ops) = (*ep->ops);
	new_ep_ops->queue = dwc3_qcom_ep_queue;
	new_ep_ops->enable = dwc3_qcom_ep_enable;

	ep->ops = new_ep_ops;

	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_ep_update_ops);

int qcom_ep_set_mode(struct usb_ep *ep, enum usb_hw_ep_mode mode)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_qcom *mdwc = dev_get_drvdata(dwc->dev->parent);

	/* Reset QCOM HW ep parameters for subsequent uses */
	if (mode == USB_EP_NONE)
		memset(&mdwc->hw_eps[dep->number], 0,
			sizeof(mdwc->hw_eps[dep->number]));

	mdwc->hw_eps[dep->number].mode = mode;

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_ep_set_mode);

static void dwc3_qcom_vbus_override_enable(struct dwc3_qcom *qcom, bool enable)
{
	if (enable) {
		dwc3_qcom_setbits(qcom->qscratch_base, QSCRATCH_SS_PHY_CTRL,
				  LANE0_PWR_PRESENT);
		dwc3_qcom_setbits(qcom->qscratch_base, QSCRATCH_HS_PHY_CTRL,
				  UTMI_OTG_VBUS_VALID | SW_SESSVLD_SEL);
	} else {
		dwc3_qcom_clrbits(qcom->qscratch_base, QSCRATCH_SS_PHY_CTRL,
				  LANE0_PWR_PRESENT);
		dwc3_qcom_clrbits(qcom->qscratch_base, QSCRATCH_HS_PHY_CTRL,
				  UTMI_OTG_VBUS_VALID | SW_SESSVLD_SEL);
	}
}

static int dwc3_qcom_vbus_notifier(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct dwc3_qcom *qcom = container_of(nb, struct dwc3_qcom, vbus_nb);

	/* enable vbus override for device mode */
	dwc3_qcom_vbus_override_enable(qcom, event);
	qcom->mode = event ? USB_DR_MODE_PERIPHERAL : USB_DR_MODE_HOST;

	return NOTIFY_DONE;
}

static int dwc3_qcom_host_notifier(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct dwc3_qcom *qcom = container_of(nb, struct dwc3_qcom, host_nb);

	/* disable vbus override in host mode */
	dwc3_qcom_vbus_override_enable(qcom, !event);
	qcom->mode = event ? USB_DR_MODE_HOST : USB_DR_MODE_PERIPHERAL;

	return NOTIFY_DONE;
}

static int dwc3_qcom_register_extcon(struct dwc3_qcom *qcom)
{
	struct device		*dev = qcom->dev;
	struct extcon_dev	*host_edev;
	int			ret;

	if (!of_property_read_bool(dev->of_node, "extcon"))
		return 0;

	qcom->edev = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(qcom->edev))
		return dev_err_probe(dev, PTR_ERR(qcom->edev),
				     "Failed to get extcon\n");

	qcom->vbus_nb.notifier_call = dwc3_qcom_vbus_notifier;

	qcom->host_edev = extcon_get_edev_by_phandle(dev, 1);
	if (IS_ERR(qcom->host_edev))
		qcom->host_edev = NULL;

	ret = devm_extcon_register_notifier(dev, qcom->edev, EXTCON_USB,
					    &qcom->vbus_nb);
	if (ret < 0) {
		dev_err(dev, "VBUS notifier register failed\n");
		return ret;
	}

	if (qcom->host_edev)
		host_edev = qcom->host_edev;
	else
		host_edev = qcom->edev;

	qcom->host_nb.notifier_call = dwc3_qcom_host_notifier;
	ret = devm_extcon_register_notifier(dev, host_edev, EXTCON_USB_HOST,
					    &qcom->host_nb);
	if (ret < 0) {
		dev_err(dev, "Host notifier register failed\n");
		return ret;
	}

	/* Update initial VBUS override based on extcon state */
	if (extcon_get_state(qcom->edev, EXTCON_USB) ||
	    !extcon_get_state(host_edev, EXTCON_USB_HOST))
		dwc3_qcom_vbus_notifier(&qcom->vbus_nb, true, qcom->edev);
	else
		dwc3_qcom_vbus_notifier(&qcom->vbus_nb, false, qcom->edev);

	return 0;
}

static int dwc3_qcom_interconnect_enable(struct dwc3_qcom *qcom)
{
	int ret;

	ret = icc_enable(qcom->icc_path_ddr);
	if (ret)
		return ret;

	ret = icc_enable(qcom->icc_path_apps);
	if (ret)
		icc_disable(qcom->icc_path_ddr);

	return ret;
}

static int dwc3_qcom_interconnect_disable(struct dwc3_qcom *qcom)
{
	int ret;

	ret = icc_disable(qcom->icc_path_ddr);
	if (ret)
		return ret;

	ret = icc_disable(qcom->icc_path_apps);
	if (ret)
		icc_enable(qcom->icc_path_ddr);

	return ret;
}

/**
 * dwc3_qcom_interconnect_init() - Get interconnect path handles
 * and set bandwidth.
 * @qcom:			Pointer to the concerned usb core.
 *
 */
static int dwc3_qcom_interconnect_init(struct dwc3_qcom *qcom)
{
	enum usb_device_speed max_speed;
	struct device *dev = qcom->dev;
	int ret;

	if (has_acpi_companion(dev))
		return 0;

	qcom->icc_path_ddr = of_icc_get(dev, "usb-ddr");
	if (IS_ERR(qcom->icc_path_ddr)) {
		return dev_err_probe(dev, PTR_ERR(qcom->icc_path_ddr),
				     "failed to get usb-ddr path\n");
	}

	qcom->icc_path_apps = of_icc_get(dev, "apps-usb");
	if (IS_ERR(qcom->icc_path_apps)) {
		ret = dev_err_probe(dev, PTR_ERR(qcom->icc_path_apps),
				    "failed to get apps-usb path\n");
		goto put_path_ddr;
	}

	max_speed = usb_get_maximum_speed(&qcom->dwc3->dev);
	if (max_speed >= USB_SPEED_SUPER || max_speed == USB_SPEED_UNKNOWN) {
		ret = icc_set_bw(qcom->icc_path_ddr,
				USB_MEMORY_AVG_SS_BW, USB_MEMORY_PEAK_SS_BW);
	} else {
		ret = icc_set_bw(qcom->icc_path_ddr,
				USB_MEMORY_AVG_HS_BW, USB_MEMORY_PEAK_HS_BW);
	}
	if (ret) {
		dev_err(dev, "failed to set bandwidth for usb-ddr path: %d\n", ret);
		goto put_path_apps;
	}

	ret = icc_set_bw(qcom->icc_path_apps, APPS_USB_AVG_BW, APPS_USB_PEAK_BW);
	if (ret) {
		dev_err(dev, "failed to set bandwidth for apps-usb path: %d\n", ret);
		goto put_path_apps;
	}

	return 0;

put_path_apps:
	icc_put(qcom->icc_path_apps);
put_path_ddr:
	icc_put(qcom->icc_path_ddr);
	return ret;
}

/**
 * dwc3_qcom_interconnect_exit() - Release interconnect path handles
 * @qcom:			Pointer to the concerned usb core.
 *
 * This function is used to release interconnect path handle.
 */
static void dwc3_qcom_interconnect_exit(struct dwc3_qcom *qcom)
{
	icc_put(qcom->icc_path_ddr);
	icc_put(qcom->icc_path_apps);
}

/* Only usable in contexts where the role can not change. */
static bool dwc3_qcom_is_host(struct dwc3_qcom *qcom)
{
	struct dwc3 *dwc;

	/*
	 * FIXME: Fix this layering violation.
	 */
	dwc = platform_get_drvdata(qcom->dwc3);

	/* Core driver may not have probed yet. */
	if (!dwc)
		return false;

	return dwc->xhci;
}

static enum usb_device_speed dwc3_qcom_read_usb2_speed(struct dwc3_qcom *qcom)
{
	struct dwc3 *dwc = platform_get_drvdata(qcom->dwc3);
	struct usb_device *udev;
	struct usb_hcd __maybe_unused *hcd;

	/*
	 * FIXME: Fix this layering violation.
	 */
	hcd = platform_get_drvdata(dwc->xhci);

	/*
	 * It is possible to query the speed of all children of
	 * USB2.0 root hub via usb_hub_for_each_child(). DWC3 code
	 * currently supports only 1 port per controller. So
	 * this is sufficient.
	 */
#ifdef CONFIG_USB
	udev = usb_hub_find_child(hcd->self.root_hub, 1);
#else
	udev = NULL;
#endif
	if (!udev)
		return USB_SPEED_UNKNOWN;

	return udev->speed;
}

static void dwc3_qcom_enable_wakeup_irq(int irq, unsigned int polarity)
{
	if (!irq)
		return;

	if (polarity)
		irq_set_irq_type(irq, polarity);

	enable_irq(irq);
	enable_irq_wake(irq);
}

static void dwc3_qcom_disable_wakeup_irq(int irq)
{
	if (!irq)
		return;

	disable_irq_wake(irq);
	disable_irq_nosync(irq);
}

static void dwc3_qcom_disable_interrupts(struct dwc3_qcom *qcom)
{
	dwc3_qcom_disable_wakeup_irq(qcom->hs_phy_irq);

	if (qcom->usb2_speed == USB_SPEED_LOW) {
		dwc3_qcom_disable_wakeup_irq(qcom->dm_hs_phy_irq);
	} else if ((qcom->usb2_speed == USB_SPEED_HIGH) ||
			(qcom->usb2_speed == USB_SPEED_FULL)) {
		dwc3_qcom_disable_wakeup_irq(qcom->dp_hs_phy_irq);
	} else {
		dwc3_qcom_disable_wakeup_irq(qcom->dp_hs_phy_irq);
		dwc3_qcom_disable_wakeup_irq(qcom->dm_hs_phy_irq);
	}

	dwc3_qcom_disable_wakeup_irq(qcom->ss_phy_irq);
}

static void dwc3_qcom_enable_interrupts(struct dwc3_qcom *qcom)
{
	dwc3_qcom_enable_wakeup_irq(qcom->hs_phy_irq, 0);

	/*
	 * Configure DP/DM line interrupts based on the USB2 device attached to
	 * the root hub port. When HS/FS device is connected, configure the DP line
	 * as falling edge to detect both disconnect and remote wakeup scenarios. When
	 * LS device is connected, configure DM line as falling edge to detect both
	 * disconnect and remote wakeup. When no device is connected, configure both
	 * DP and DM lines as rising edge to detect HS/HS/LS device connect scenario.
	 */

	if (qcom->usb2_speed == USB_SPEED_LOW) {
		dwc3_qcom_enable_wakeup_irq(qcom->dm_hs_phy_irq,
						IRQ_TYPE_EDGE_FALLING);
	} else if ((qcom->usb2_speed == USB_SPEED_HIGH) ||
			(qcom->usb2_speed == USB_SPEED_FULL)) {
		dwc3_qcom_enable_wakeup_irq(qcom->dp_hs_phy_irq,
						IRQ_TYPE_EDGE_FALLING);
	} else {
		dwc3_qcom_enable_wakeup_irq(qcom->dp_hs_phy_irq,
						IRQ_TYPE_EDGE_RISING);
		dwc3_qcom_enable_wakeup_irq(qcom->dm_hs_phy_irq,
						IRQ_TYPE_EDGE_RISING);
	}

	dwc3_qcom_enable_wakeup_irq(qcom->ss_phy_irq, 0);
}

static int dwc3_qcom_suspend(struct dwc3_qcom *qcom, bool wakeup)
{
	u32 val;
	int i, ret;

	if (qcom->is_suspended)
		return 0;

	val = readl(qcom->qscratch_base + PWR_EVNT_IRQ_STAT_REG);
	if (!(val & PWR_EVNT_LPM_IN_L2_MASK))
		dev_err(qcom->dev, "HS-PHY not in L2\n");

	for (i = qcom->num_clocks - 1; i >= 0; i--)
		clk_disable_unprepare(qcom->clks[i]);

	ret = dwc3_qcom_interconnect_disable(qcom);
	if (ret)
		dev_warn(qcom->dev, "failed to disable interconnect: %d\n", ret);

	/*
	 * The role is stable during suspend as role switching is done from a
	 * freezable workqueue.
	 */
	if (dwc3_qcom_is_host(qcom) && wakeup) {
		qcom->usb2_speed = dwc3_qcom_read_usb2_speed(qcom);
		dwc3_qcom_enable_interrupts(qcom);
	}

	qcom->is_suspended = true;

	return 0;
}

static int dwc3_qcom_resume(struct dwc3_qcom *qcom, bool wakeup)
{
	int ret;
	int i;

	if (!qcom->is_suspended)
		return 0;

	if (dwc3_qcom_is_host(qcom) && wakeup)
		dwc3_qcom_disable_interrupts(qcom);

	for (i = 0; i < qcom->num_clocks; i++) {
		ret = clk_prepare_enable(qcom->clks[i]);
		if (ret < 0) {
			while (--i >= 0)
				clk_disable_unprepare(qcom->clks[i]);
			return ret;
		}
	}

	ret = dwc3_qcom_interconnect_enable(qcom);
	if (ret)
		dev_warn(qcom->dev, "failed to enable interconnect: %d\n", ret);

	/* Clear existing events from PHY related to L2 in/out */
	dwc3_qcom_setbits(qcom->qscratch_base, PWR_EVNT_IRQ_STAT_REG,
			  PWR_EVNT_LPM_IN_L2_MASK | PWR_EVNT_LPM_OUT_L2_MASK);

	qcom->is_suspended = false;

	return 0;
}

static irqreturn_t qcom_dwc3_resume_irq(int irq, void *data)
{
	struct dwc3_qcom *qcom = data;
	struct dwc3	*dwc = platform_get_drvdata(qcom->dwc3);

	/* If pm_suspended then let pm_resume take care of resuming h/w */
	if (qcom->pm_suspended)
		return IRQ_HANDLED;

	/*
	 * This is safe as role switching is done from a freezable workqueue
	 * and the wakeup interrupts are disabled as part of resume.
	 */
	if (dwc3_qcom_is_host(qcom))
		pm_runtime_resume(&dwc->xhci->dev);

	return IRQ_HANDLED;
}

static void dwc3_qcom_select_utmi_clk(struct dwc3_qcom *qcom)
{
	/* Configure dwc3 to use UTMI clock as PIPE clock not present */
	dwc3_qcom_setbits(qcom->qscratch_base, QSCRATCH_GENERAL_CFG,
			  PIPE_UTMI_CLK_DIS);

	usleep_range(100, 1000);

	dwc3_qcom_setbits(qcom->qscratch_base, QSCRATCH_GENERAL_CFG,
			  PIPE_UTMI_CLK_SEL | PIPE3_PHYSTATUS_SW);

	usleep_range(100, 1000);

	dwc3_qcom_clrbits(qcom->qscratch_base, QSCRATCH_GENERAL_CFG,
			  PIPE_UTMI_CLK_DIS);
}

static int dwc3_qcom_get_irq(struct platform_device *pdev,
			     const char *name, int num)
{
	struct dwc3_qcom *qcom = platform_get_drvdata(pdev);
	struct platform_device *pdev_irq = qcom->urs_usb ? qcom->urs_usb : pdev;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (np)
		ret = platform_get_irq_byname_optional(pdev_irq, name);
	else
		ret = platform_get_irq_optional(pdev_irq, num);

	return ret;
}

static int dwc3_qcom_setup_irq(struct platform_device *pdev)
{
	struct dwc3_qcom *qcom = platform_get_drvdata(pdev);
	const struct dwc3_acpi_pdata *pdata = qcom->acpi_pdata;
	int irq;
	int ret;

	irq = dwc3_qcom_get_irq(pdev, "hs_phy_irq",
				pdata ? pdata->hs_phy_irq_index : -1);
	if (irq > 0) {
		/* Keep wakeup interrupts disabled until suspend */
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(qcom->dev, irq, NULL,
					qcom_dwc3_resume_irq,
					IRQF_ONESHOT,
					"qcom_dwc3 HS", qcom);
		if (ret) {
			dev_err(qcom->dev, "hs_phy_irq failed: %d\n", ret);
			return ret;
		}
		qcom->hs_phy_irq = irq;
	}

	irq = dwc3_qcom_get_irq(pdev, "dp_hs_phy_irq",
				pdata ? pdata->dp_hs_phy_irq_index : -1);
	if (irq > 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(qcom->dev, irq, NULL,
					qcom_dwc3_resume_irq,
					IRQF_ONESHOT,
					"qcom_dwc3 DP_HS", qcom);
		if (ret) {
			dev_err(qcom->dev, "dp_hs_phy_irq failed: %d\n", ret);
			return ret;
		}
		qcom->dp_hs_phy_irq = irq;
	}

	irq = dwc3_qcom_get_irq(pdev, "dm_hs_phy_irq",
				pdata ? pdata->dm_hs_phy_irq_index : -1);
	if (irq > 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(qcom->dev, irq, NULL,
					qcom_dwc3_resume_irq,
					IRQF_ONESHOT,
					"qcom_dwc3 DM_HS", qcom);
		if (ret) {
			dev_err(qcom->dev, "dm_hs_phy_irq failed: %d\n", ret);
			return ret;
		}
		qcom->dm_hs_phy_irq = irq;
	}

	irq = dwc3_qcom_get_irq(pdev, "ss_phy_irq",
				pdata ? pdata->ss_phy_irq_index : -1);
	if (irq > 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(qcom->dev, irq, NULL,
					qcom_dwc3_resume_irq,
					IRQF_ONESHOT,
					"qcom_dwc3 SS", qcom);
		if (ret) {
			dev_err(qcom->dev, "ss_phy_irq failed: %d\n", ret);
			return ret;
		}
		qcom->ss_phy_irq = irq;
	}

	return 0;
}

static int dwc3_qcom_clk_init(struct dwc3_qcom *qcom, int count)
{
	struct device		*dev = qcom->dev;
	struct device_node	*np = dev->of_node;
	int			i;

	if (!np || !count)
		return 0;

	if (count < 0)
		return count;

	qcom->num_clocks = count;

	qcom->clks = devm_kcalloc(dev, qcom->num_clocks,
				  sizeof(struct clk *), GFP_KERNEL);
	if (!qcom->clks)
		return -ENOMEM;

	for (i = 0; i < qcom->num_clocks; i++) {
		struct clk	*clk;
		int		ret;

		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			while (--i >= 0)
				clk_put(qcom->clks[i]);
			return PTR_ERR(clk);
		}

		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			while (--i >= 0) {
				clk_disable_unprepare(qcom->clks[i]);
				clk_put(qcom->clks[i]);
			}
			clk_put(clk);

			return ret;
		}

		qcom->clks[i] = clk;
	}

	return 0;
}

static const struct property_entry dwc3_qcom_acpi_properties[] = {
	PROPERTY_ENTRY_STRING("dr_mode", "host"),
	{}
};

static const struct software_node dwc3_qcom_swnode = {
	.properties = dwc3_qcom_acpi_properties,
};

static int dwc3_qcom_acpi_register_core(struct platform_device *pdev)
{
	struct dwc3_qcom	*qcom = platform_get_drvdata(pdev);
	struct device		*dev = &pdev->dev;
	struct resource		*res, *child_res = NULL;
	struct platform_device	*pdev_irq = qcom->urs_usb ? qcom->urs_usb :
							    pdev;
	int			irq;
	int			ret;

	qcom->dwc3 = platform_device_alloc("dwc3", PLATFORM_DEVID_AUTO);
	if (!qcom->dwc3)
		return -ENOMEM;

	qcom->dwc3->dev.parent = dev;
	qcom->dwc3->dev.type = dev->type;
	qcom->dwc3->dev.dma_mask = dev->dma_mask;
	qcom->dwc3->dev.dma_parms = dev->dma_parms;
	qcom->dwc3->dev.coherent_dma_mask = dev->coherent_dma_mask;

	child_res = kcalloc(2, sizeof(*child_res), GFP_KERNEL);
	if (!child_res) {
		platform_device_put(qcom->dwc3);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get memory resource\n");
		ret = -ENODEV;
		goto out;
	}

	child_res[0].flags = res->flags;
	child_res[0].start = res->start;
	child_res[0].end = child_res[0].start +
		qcom->acpi_pdata->dwc3_core_base_size;

	irq = platform_get_irq(pdev_irq, 0);
	if (irq < 0) {
		ret = irq;
		goto out;
	}
	child_res[1].flags = IORESOURCE_IRQ;
	child_res[1].start = child_res[1].end = irq;

	ret = platform_device_add_resources(qcom->dwc3, child_res, 2);
	if (ret) {
		dev_err(&pdev->dev, "failed to add resources\n");
		goto out;
	}

	ret = device_add_software_node(&qcom->dwc3->dev, &dwc3_qcom_swnode);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add properties\n");
		goto out;
	}

	ret = platform_device_add(qcom->dwc3);
	if (ret) {
		dev_err(&pdev->dev, "failed to add device\n");
		device_remove_software_node(&qcom->dwc3->dev);
		goto out;
	}
	kfree(child_res);
	return 0;

out:
	platform_device_put(qcom->dwc3);
	kfree(child_res);
	return ret;
}

static int dwc3_qcom_of_register_core(struct platform_device *pdev)
{
	struct dwc3_qcom	*qcom = platform_get_drvdata(pdev);
	struct device_node	*np = pdev->dev.of_node, *dwc3_np;
	struct device		*dev = &pdev->dev;
	int			ret;

	dwc3_np = of_get_compatible_child(np, "snps,dwc3");
	if (!dwc3_np) {
		dev_err(dev, "failed to find dwc3 core child\n");
		return -ENODEV;
	}

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed to register dwc3 core - %d\n", ret);
		goto node_put;
	}

	qcom->dwc3 = of_find_device_by_node(dwc3_np);
	if (!qcom->dwc3) {
		ret = -ENODEV;
		dev_err(dev, "failed to get dwc3 platform device\n");
		of_platform_depopulate(dev);
	}

node_put:
	of_node_put(dwc3_np);

	return ret;
}

static struct platform_device *dwc3_qcom_create_urs_usb_platdev(struct device *dev)
{
	struct platform_device *urs_usb = NULL;
	struct fwnode_handle *fwh;
	struct acpi_device *adev;
	char name[8];
	int ret;
	int id;

	/* Figure out device id */
	ret = sscanf(fwnode_get_name(dev->fwnode), "URS%d", &id);
	if (!ret)
		return NULL;

	/* Find the child using name */
	snprintf(name, sizeof(name), "USB%d", id);
	fwh = fwnode_get_named_child_node(dev->fwnode, name);
	if (!fwh)
		return NULL;

	adev = to_acpi_device_node(fwh);
	if (!adev)
		goto err_put_handle;

	urs_usb = acpi_create_platform_device(adev, NULL);
	if (IS_ERR_OR_NULL(urs_usb))
		goto err_put_handle;

	return urs_usb;

err_put_handle:
	fwnode_handle_put(fwh);

	return urs_usb;
}

static void dwc3_qcom_destroy_urs_usb_platdev(struct platform_device *urs_usb)
{
	struct fwnode_handle *fwh = urs_usb->dev.fwnode;

	platform_device_unregister(urs_usb);
	fwnode_handle_put(fwh);
}

static int dwc3_qcom_phy_sel(struct dwc3_qcom *qcom)
{
	struct of_phandle_args args;
	int ret;

	ret = of_parse_phandle_with_fixed_args(qcom->dev->of_node,
			"qcom,phy-mux-regs", 1, 0, &args);
	if (ret) {
		dev_err(qcom->dev, "failed to parse qcom,phy-mux-regs\n");
		return ret;
	}

	qcom->phy_mux_map = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	if (IS_ERR(qcom->phy_mux_map)) {
		pr_err("phy mux regs map failed:%ld\n",
						PTR_ERR(qcom->phy_mux_map));
		return PTR_ERR(qcom->phy_mux_map);
	}

	qcom->phy_mux_reg = args.args[0];
	/*usb phy mux sel*/
	ret = regmap_write(qcom->phy_mux_map, qcom->phy_mux_reg, 0x1);
	if (ret) {
		dev_err(qcom->dev,
			"Not able to configure phy mux selection:%d\n", ret);
		return ret;
	}

	return 0;
}


static int dwc3_qcom_probe(struct platform_device *pdev)
{
	struct device_node	*np = pdev->dev.of_node;
	struct device		*dev = &pdev->dev;
	struct dwc3_qcom	*qcom;
	struct resource		*res, *parent_res = NULL;
	struct resource		local_res;
	int			ret, i;
	bool			ignore_pipe_clk;
	bool			wakeup_source;

	qcom = devm_kzalloc(&pdev->dev, sizeof(*qcom), GFP_KERNEL);
	if (!qcom)
		return -ENOMEM;

	platform_set_drvdata(pdev, qcom);
	qcom->dev = &pdev->dev;

	if (has_acpi_companion(dev)) {
		qcom->acpi_pdata = acpi_device_get_match_data(dev);
		if (!qcom->acpi_pdata) {
			dev_err(&pdev->dev, "no supporting ACPI device data\n");
			return -EINVAL;
		}
	}

	qcom->phy_mux = device_property_read_bool(dev,
				"qcom,multiplexed-phy");
	if (qcom->phy_mux)
		dwc3_qcom_phy_sel(qcom);

	qcom->resets = devm_reset_control_array_get_optional_exclusive(dev);
	if (IS_ERR(qcom->resets)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(qcom->resets),
				     "failed to get resets\n");
	}

	ret = reset_control_assert(qcom->resets);
	if (ret) {
		dev_err(&pdev->dev, "failed to assert resets, err=%d\n", ret);
		return ret;
	}

	usleep_range(10, 1000);

	ret = reset_control_deassert(qcom->resets);
	if (ret) {
		dev_err(&pdev->dev, "failed to deassert resets, err=%d\n", ret);
		return ret;
	}

	ret = dwc3_qcom_clk_init(qcom, of_clk_get_parent_count(np));
	if (ret) {
		dev_err_probe(dev, ret, "failed to get clocks\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (np) {
		parent_res = res;
	} else {
		memcpy(&local_res, res, sizeof(struct resource));
		parent_res = &local_res;

		parent_res->start = res->start +
			qcom->acpi_pdata->qscratch_base_offset;
		parent_res->end = parent_res->start +
			qcom->acpi_pdata->qscratch_base_size;

		if (qcom->acpi_pdata->is_urs) {
			qcom->urs_usb = dwc3_qcom_create_urs_usb_platdev(dev);
			if (IS_ERR_OR_NULL(qcom->urs_usb)) {
				dev_err(dev, "failed to create URS USB platdev\n");
				if (!qcom->urs_usb)
					ret = -ENODEV;
				else
					ret = PTR_ERR(qcom->urs_usb);
				goto clk_disable;
			}
		}
	}

	qcom->qscratch_base = devm_ioremap_resource(dev, parent_res);
	if (IS_ERR(qcom->qscratch_base)) {
		ret = PTR_ERR(qcom->qscratch_base);
		goto free_urs;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		qcom->ebc_desc_addr = devm_ioremap_resource(&pdev->dev, res);

	INIT_LIST_HEAD(&qcom->req_complete_list);
	ret = dwc3_qcom_setup_irq(pdev);
	if (ret) {
		dev_err(dev, "failed to setup IRQs, err=%d\n", ret);
		goto free_urs;
	}

	/*
	 * Disable pipe_clk requirement if specified. Used when dwc3
	 * operates without SSPHY and only HS/FS/LS modes are supported.
	 */
	ignore_pipe_clk = device_property_read_bool(dev,
				"qcom,select-utmi-as-pipe-clk");
	if (ignore_pipe_clk)
		dwc3_qcom_select_utmi_clk(qcom);

	if (np)
		ret = dwc3_qcom_of_register_core(pdev);
	else
		ret = dwc3_qcom_acpi_register_core(pdev);

	if (ret) {
		dev_err(dev, "failed to register DWC3 Core, err=%d\n", ret);
		goto free_urs;
	}

	ret = dwc3_qcom_interconnect_init(qcom);
	if (ret)
		goto depopulate;

	qcom->mode = usb_get_dr_mode(&qcom->dwc3->dev);

	/* enable vbus override for device mode */
	if (qcom->mode != USB_DR_MODE_HOST)
		dwc3_qcom_vbus_override_enable(qcom, true);

	/* register extcon to override sw_vbus on Vbus change later */
	ret = dwc3_qcom_register_extcon(qcom);
	if (ret)
		goto interconnect_exit;

	wakeup_source = of_property_read_bool(dev->of_node, "wakeup-source");
	device_init_wakeup(&pdev->dev, wakeup_source);
	device_init_wakeup(&qcom->dwc3->dev, wakeup_source);

	qcom->is_suspended = false;
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_forbid(dev);

	return 0;

interconnect_exit:
	dwc3_qcom_interconnect_exit(qcom);
depopulate:
	if (np) {
		of_platform_depopulate(&pdev->dev);
	} else {
		device_remove_software_node(&qcom->dwc3->dev);
		platform_device_del(qcom->dwc3);
	}
	platform_device_put(qcom->dwc3);
free_urs:
	if (qcom->urs_usb)
		dwc3_qcom_destroy_urs_usb_platdev(qcom->urs_usb);
clk_disable:
	for (i = qcom->num_clocks - 1; i >= 0; i--) {
		clk_disable_unprepare(qcom->clks[i]);
		clk_put(qcom->clks[i]);
	}

	return ret;
}

static void dwc3_qcom_remove(struct platform_device *pdev)
{
	struct dwc3_qcom *qcom = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int i, ret;

	pm_runtime_get_sync(dev);
	pm_runtime_disable(dev);

	if (np) {
		of_platform_depopulate(&pdev->dev);
	} else {
		device_remove_software_node(&qcom->dwc3->dev);
		platform_device_del(qcom->dwc3);
	}
	platform_device_put(qcom->dwc3);

	if (qcom->urs_usb)
		dwc3_qcom_destroy_urs_usb_platdev(qcom->urs_usb);

	dwc3_qcom_interconnect_exit(qcom);
	reset_control_assert(qcom->resets);

	if (qcom->phy_mux) {
		/*usb phy mux deselection*/
		ret = regmap_write(qcom->phy_mux_map, qcom->phy_mux_reg,
					0x0);
		if (ret)
			dev_err(qcom->dev,
			  "Not able to configure phy mux selection:%d\n", ret);
	}

	for (i = qcom->num_clocks - 1; i >= 0; i--) {
		if (!qcom->is_suspended)
			clk_disable_unprepare(qcom->clks[i]);
		clk_put(qcom->clks[i]);
	}
	qcom->num_clocks = 0;
}

static int __maybe_unused dwc3_qcom_pm_suspend(struct device *dev)
{
	struct dwc3_qcom *qcom = dev_get_drvdata(dev);
	bool wakeup = device_may_wakeup(dev);
	int ret;

	ret = dwc3_qcom_suspend(qcom, wakeup);
	if (ret)
		return ret;

	qcom->pm_suspended = true;

	return 0;
}

static int __maybe_unused dwc3_qcom_pm_resume(struct device *dev)
{
	struct dwc3_qcom *qcom = dev_get_drvdata(dev);
	bool wakeup = device_may_wakeup(dev);
	int ret;

	ret = dwc3_qcom_resume(qcom, wakeup);
	if (ret)
		return ret;

	qcom->pm_suspended = false;

	return 0;
}

static int __maybe_unused dwc3_qcom_runtime_suspend(struct device *dev)
{
	struct dwc3_qcom *qcom = dev_get_drvdata(dev);

	return dwc3_qcom_suspend(qcom, true);
}

static int __maybe_unused dwc3_qcom_runtime_resume(struct device *dev)
{
	struct dwc3_qcom *qcom = dev_get_drvdata(dev);

	return dwc3_qcom_resume(qcom, true);
}

static const struct dev_pm_ops dwc3_qcom_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_qcom_pm_suspend, dwc3_qcom_pm_resume)
	SET_RUNTIME_PM_OPS(dwc3_qcom_runtime_suspend, dwc3_qcom_runtime_resume,
			   NULL)
};

static const struct of_device_id dwc3_qcom_of_match[] = {
	{ .compatible = "qcom,dwc3" },
	{ }
};
MODULE_DEVICE_TABLE(of, dwc3_qcom_of_match);

#ifdef CONFIG_ACPI
static const struct dwc3_acpi_pdata sdm845_acpi_pdata = {
	.qscratch_base_offset = SDM845_QSCRATCH_BASE_OFFSET,
	.qscratch_base_size = SDM845_QSCRATCH_SIZE,
	.dwc3_core_base_size = SDM845_DWC3_CORE_SIZE,
	.hs_phy_irq_index = 1,
	.dp_hs_phy_irq_index = 4,
	.dm_hs_phy_irq_index = 3,
	.ss_phy_irq_index = 2
};

static const struct dwc3_acpi_pdata sdm845_acpi_urs_pdata = {
	.qscratch_base_offset = SDM845_QSCRATCH_BASE_OFFSET,
	.qscratch_base_size = SDM845_QSCRATCH_SIZE,
	.dwc3_core_base_size = SDM845_DWC3_CORE_SIZE,
	.hs_phy_irq_index = 1,
	.dp_hs_phy_irq_index = 4,
	.dm_hs_phy_irq_index = 3,
	.ss_phy_irq_index = 2,
	.is_urs = true,
};

static const struct acpi_device_id dwc3_qcom_acpi_match[] = {
	{ "QCOM2430", (unsigned long)&sdm845_acpi_pdata },
	{ "QCOM0304", (unsigned long)&sdm845_acpi_urs_pdata },
	{ "QCOM0497", (unsigned long)&sdm845_acpi_urs_pdata },
	{ "QCOM04A6", (unsigned long)&sdm845_acpi_pdata },
	{ },
};
MODULE_DEVICE_TABLE(acpi, dwc3_qcom_acpi_match);
#endif

static struct platform_driver dwc3_qcom_driver = {
	.probe		= dwc3_qcom_probe,
	.remove_new	= dwc3_qcom_remove,
	.driver		= {
		.name	= "dwc3-qcom",
		.pm	= &dwc3_qcom_dev_pm_ops,
		.of_match_table	= dwc3_qcom_of_match,
		.acpi_match_table = ACPI_PTR(dwc3_qcom_acpi_match),
	},
};

module_platform_driver(dwc3_qcom_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare DWC3 QCOM Glue Driver");
