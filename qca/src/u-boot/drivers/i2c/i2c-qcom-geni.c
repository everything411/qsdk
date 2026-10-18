// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
// Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.

#include <init.h>
#include <env.h>
#include <common.h>
#include <log.h>
#include <dm/device_compat.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/compat.h>
#include <linux/bitops.h>
#include <linux/iopoll.h>
#include <asm/io.h>
#include <i2c.h>
#include <watchdog.h>
#include <fdtdec.h>
#include <misc.h>
#include <clk.h>
#include <reset.h>
#include <asm/arch/gpio.h>
#include <cpu_func.h>
#include <asm/system.h>
#include <asm/gpio.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <geni_se.h>

#define SE_I2C_TX_TRANS_LEN		0x26c
#define SE_I2C_RX_TRANS_LEN		0x270
#define SE_I2C_SCL_COUNTERS		0x278

#define SE_I2C_ERR  (M_CMD_OVERRUN_EN | M_ILLEGAL_CMD_EN | M_CMD_FAILURE_EN |\
			M_GP_IRQ_1_EN | M_GP_IRQ_3_EN | M_GP_IRQ_4_EN)
#define SE_I2C_ABORT		BIT(1)

/* M_CMD OP codes for I2C */
#define I2C_WRITE		0x1
#define I2C_READ		0x2
#define I2C_WRITE_READ		0x3
#define I2C_ADDR_ONLY		0x4
#define I2C_BUS_CLEAR		0x6
#define I2C_STOP_ON_BUS		0x7
/* M_CMD params for I2C */
#define PRE_CMD_DELAY		BIT(0)
#define TIMESTAMP_BEFORE	BIT(1)
#define STOP_STRETCH		BIT(2)
#define TIMESTAMP_AFTER		BIT(3)
#define POST_COMMAND_DELAY	BIT(4)
#define IGNORE_ADD_NACK		BIT(6)
#define READ_FINISHED_WITH_ACK	BIT(7)
#define BYPASS_ADDR_PHASE	BIT(8)
#define SLV_ADDR_MSK		GENMASK(15, 9)
#define SLV_ADDR_SHFT		9
/* I2C SCL COUNTER fields */
#define HIGH_COUNTER_MSK	GENMASK(29, 20)
#define HIGH_COUNTER_SHFT	20
#define LOW_COUNTER_MSK		GENMASK(19, 10)
#define LOW_COUNTER_SHFT	10
#define CYCLE_COUNTER_MSK	GENMASK(9, 0)

#define I2C_PACK_TX		BIT(0)
#define I2C_PACK_RX		BIT(1)

#define NACK_ERR	BIT(10)

enum geni_i2c_err_code {
	GP_IRQ0,
	NACK,
	GP_IRQ2,
	BUS_PROTO,
	ARB_LOST,
	GP_IRQ5,
	GENI_OVERRUN,
	GENI_ILLEGAL_CMD,
	GENI_ABORT_DONE,
	GENI_TIMEOUT,
};

#define DM_I2C_CB_ERR		((BIT(NACK) | BIT(BUS_PROTO) | BIT(ARB_LOST)) \
									<< 5)

#define I2C_AUTO_SUSPEND_DELAY	250
#define KHZ(freq)		(1000 * freq)
#define PACKING_BYTES_PW	4

#define ABORT_TIMEOUT		HZ
#define XFER_TIMEOUT		HZ
#define RST_TIMEOUT		HZ

struct geni_i2c_dev {
	phys_addr_t base;
	struct udevice *dev;
	struct clk clk;
	u32 tx_wm;
	struct i2c_msg *cur;
	int cur_wr;
	int cur_rd;
	u32 clk_freq_out;
	const struct geni_i2c_clk_fld *clk_fld;
	uint32_t geni_se_version;
};

struct geni_i2c_clk_fld {
	u32	clk_freq_out;
	u8	clk_div;
	u8	t_high_cnt;
	u8	t_low_cnt;
	u8	t_cycle_cnt;
};

/**
 * geni_se_init() - Initialize the GENI serial engine
 * @se:		Pointer to the concerned serial engine.
 * @rx_wm:	Receive watermark, in units of FIFO words.
 * @rx_rfr:	Ready-for-receive watermark, in units of FIFO words.
 *
 * This function is used to initialize the GENI serial engine, configure
 * receive watermark and ready-for-receive watermarks.
 */
void geni_se_init(struct geni_i2c_dev *gi2c, u32 rx_wm, u32 rx_rfr)
{
	u32 val;

	writel(0, gi2c->base + SE_GSI_EVENT_EN);
	geni_se_irq_clear(gi2c->base);
	geni_se_io_set_mode(gi2c->base);

	val = readl(gi2c->base + SE_GENI_M_IRQ_EN);
	val |= M_COMMON_GENI_M_IRQ_EN;
	val &= ~0x1000000;
	writel(val, gi2c->base + SE_GENI_M_IRQ_EN);
}

static void get_geni_se_version(struct udevice *dev)
{
	struct geni_i2c_dev *priv = dev_get_priv(dev);
	struct udevice *parent_dev = dev_get_parent(dev);
	int ret;

	if (device_get_uclass_id(parent_dev) != UCLASS_MISC)
		return;

	ret = misc_read(parent_dev, QUP_HW_VER_REG,
			&priv->geni_se_version, sizeof(priv->geni_se_version));
	if (ret != sizeof(priv->geni_se_version))
		return;
}

/*
 * Hardware uses the underlying formula to calculate time periods of
 * SCL clock cycle. Firmware uses some additional cycles excluded from the
 * below formula and it is confirmed that the time periods are within
 * specification limits.
 *
 * time of high period of SCL: t_high = (t_high_cnt * clk_div) / source_clock
 * time of low period of SCL: t_low = (t_low_cnt * clk_div) / source_clock
 * time of full period of SCL: t_cycle = (t_cycle_cnt * clk_div) / source_clock
 * clk_freq_out = t / t_cycle
 * source_clock = 19.2 MHz
 */
static const struct geni_i2c_clk_fld geni_i2c_clk_map[] = {
	{KHZ(100), 7, 14, 18, 40},
	{KHZ(400), 4,  3, 11, 20},
	{KHZ(1000), 2, 3,  6, 15},
};

static int geni_i2c_clk_map_idx(struct geni_i2c_dev *gi2c)
{
	int i;
	const struct geni_i2c_clk_fld *itr = geni_i2c_clk_map;

	for (i = 0; i < ARRAY_SIZE(geni_i2c_clk_map); i++, itr++) {
		if (itr->clk_freq_out == gi2c->clk_freq_out) {
			gi2c->clk_fld = itr;
			return 0;
		}
	}
	return -EINVAL;
}

static void qcom_geni_i2c_conf(struct geni_i2c_dev *gi2c)
{
	const struct geni_i2c_clk_fld *itr = gi2c->clk_fld;
	u32 val;

	writel(0, gi2c->base + SE_GENI_CLK_SEL);

	val = (itr->clk_div << CLK_DIV_SHFT) | SER_CLK_EN;
	writel(val, gi2c->base + GENI_SER_M_CLK_CFG);

	val = itr->t_high_cnt << HIGH_COUNTER_SHFT;
	val |= itr->t_low_cnt << LOW_COUNTER_SHFT;
	val |= itr->t_cycle_cnt;
	writel(val, gi2c->base + SE_I2C_SCL_COUNTERS);
}

static void geni_i2c_err_misc(struct geni_i2c_dev *gi2c)
{
	u32 m_cmd = readl_relaxed(gi2c->base + SE_GENI_M_CMD0);
	u32 m_stat = readl_relaxed(gi2c->base + SE_GENI_M_IRQ_STATUS);
	u32 geni_s = readl_relaxed(gi2c->base + SE_GENI_STATUS);
	u32 geni_ios = readl_relaxed(gi2c->base + SE_GENI_IOS);
	u32 dma = readl_relaxed(gi2c->base + SE_GENI_DMA_MODE_EN);
	u32 rx_st, tx_st;

	if (dma) {
		rx_st = readl(gi2c->base + SE_DMA_RX_IRQ_STAT);
		tx_st = readl(gi2c->base + SE_DMA_TX_IRQ_STAT);
	} else {
		rx_st = readl(gi2c->base + SE_GENI_RX_FIFO_STATUS);
		tx_st = readl(gi2c->base + SE_GENI_TX_FIFO_STATUS);
	}
	printf("DMA:%d tx_stat:0x%x, rx_stat:0x%x, irq-stat:0x%x\n",
		dma, tx_st, rx_st, m_stat);
	printf("m_cmd:0x%x, geni_status:0x%x, geni_ios:0x%x\n",
		m_cmd, geni_s, geni_ios);
}

static int geni_i2c_irq(struct geni_i2c_dev *gi2c)
{
	phys_addr_t base = gi2c->base;
	unsigned long time_left = 1000;
	int j, p;
	u32 m_stat;
	u32 rx_st;
	u32 val;
	struct i2c_msg *cur;

	m_stat = readl(base + SE_GENI_M_IRQ_STATUS);
	rx_st = readl(base + SE_GENI_RX_FIFO_STATUS);
	cur = gi2c->cur;

	if (!cur ||
	    m_stat & SE_I2C_ERR || m_stat & (M_GP_IRQ_0_EN | M_CMD_ABORT_EN)) {
		geni_i2c_err_misc(gi2c);
		/* Disable the TX Watermark interrupt to stop TX */
		writel(0, base + SE_GENI_TX_WATERMARK_REG);
	} else if (cur->flags & I2C_M_RD &&
		   m_stat & (M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN)) {
		u32 rxcnt = rx_st & RX_FIFO_WC_MSK;

		for (j = 0; j < rxcnt; j++) {
			p = 0;
			val = readl(base + SE_GENI_RX_FIFOn);
			while (gi2c->cur_rd < cur->len && p < sizeof(val)) {
				cur->buf[gi2c->cur_rd++] = val & 0xff;
				val >>= 8;
				p++;
			}
			if (gi2c->cur_rd == cur->len)
				break;
		}
	} else if (!(cur->flags & I2C_M_RD) &&
		   m_stat & M_TX_FIFO_WATERMARK_EN) {
		for (j = 0; j < gi2c->tx_wm; j++) {
			u32 temp;

			val = 0;
			p = 0;
			while (gi2c->cur_wr < cur->len && p < sizeof(val)) {
				temp = cur->buf[gi2c->cur_wr++];
				val |= temp << (p * 8);
				p++;
			}
			writel(val, base + SE_GENI_TX_FIFOn);
			/* TX Complete, Disable the TX Watermark interrupt */
			if (gi2c->cur_wr == cur->len) {
				writel_relaxed(0, base +
						SE_GENI_TX_WATERMARK_REG);
				break;
			}
		}
	}

	do{
		--time_left;
		udelay(10);
	}while(time_left && !(readl(base + SE_GENI_M_IRQ_STATUS) & 0x1));

	m_stat = readl(base + SE_GENI_M_IRQ_STATUS) & SE_I2C_ERR ? -ENODEV : 0;
	writel(m_stat, base + SE_GENI_M_IRQ_CLEAR);

	return m_stat;

}

static void geni_se_setup_m_cmd(phys_addr_t base, u32 cmd, u32 params)
{
	u32 m_cmd;

	m_cmd = (cmd << M_OPCODE_SHFT) | (params & M_PARAMS_MSK);
	writel(m_cmd, base + SE_GENI_M_CMD0);
}

static int geni_i2c_rx_one_msg(struct geni_i2c_dev *gi2c, struct i2c_msg *msg,
				u32 m_param)
{
	size_t len = msg->len;
	struct i2c_msg *cur;

	writel(len, gi2c->base + SE_I2C_RX_TRANS_LEN);
	writel(0, gi2c->base + SE_I2C_TX_TRANS_LEN);

	geni_se_setup_m_cmd(gi2c->base, I2C_READ, m_param);
	udelay(500);

	gi2c->cur_rd = 0;
	cur = gi2c->cur;
	return geni_i2c_irq(gi2c);

}

static int geni_i2c_tx_one_msg(struct geni_i2c_dev *gi2c, struct i2c_msg *msg,
				u32 m_param)
{
	size_t len = msg->len;
	struct i2c_msg *cur;

	writel(len, gi2c->base + SE_I2C_TX_TRANS_LEN);
	writel(0, gi2c->base + SE_I2C_RX_TRANS_LEN);

	geni_se_setup_m_cmd(gi2c->base, I2C_WRITE, m_param);

	gi2c->tx_wm = len;
	gi2c->cur_wr  = 0;

	writel(1, gi2c->base + SE_GENI_TX_WATERMARK_REG);

	cur = gi2c->cur;
	return geni_i2c_irq(gi2c);
}

static int geni_i2c_fifo_xfer(struct geni_i2c_dev *gi2c,
			      struct i2c_msg msgs[], int num)
{
	int i, ret = -1;

	for (i = 0; i < num; i++) {
		u32 m_param = i < (num - 1) ? STOP_STRETCH : 0;

		if(msgs[i].len > 64)
		{
			printf("Given size is larger than the FIFO size\n");
			return -EINVAL;
		}

		m_param |= ((msgs[i].addr << SLV_ADDR_SHFT) & SLV_ADDR_MSK);

		geni_se_config_fifo_mode(gi2c->base);

		gi2c->cur = &msgs[i];
		if (msgs[i].flags & I2C_M_RD)
			ret = geni_i2c_rx_one_msg(gi2c, &msgs[i], m_param);
		else
			ret = geni_i2c_tx_one_msg(gi2c, &msgs[i], m_param);

		if (ret)
			return ret;
	}

	return ret;
}

static int geni_i2c_xfer(struct udevice *bus,
			 struct i2c_msg msgs[],
			 int num)
{
	struct geni_i2c_dev *gi2c = dev_get_priv(bus);
	int ret;

	qcom_geni_i2c_conf(gi2c);

	ret = geni_i2c_fifo_xfer(gi2c, msgs, num);

	gi2c->cur = NULL;
	return ret;
}

/* Probe to see if a chip is present. */
static int geni_i2c_probe_chip(struct udevice *dev, uint chip_addr,
			      uint chip_flags)
{
	struct i2c_msg msgs;
	struct geni_i2c_dev *gi2c = dev_get_priv(dev);
	unsigned long time_left = 0x10;
	uint32_t m_param = 0;
	qcom_geni_i2c_conf(gi2c);
	msgs.addr = chip_addr;
	msgs.flags = 0;
	msgs.len = 0;

	geni_se_config_fifo_mode(gi2c->base);

	writel(msgs.len, gi2c->base + SE_I2C_TX_TRANS_LEN);

	m_param |= ((msgs.addr << SLV_ADDR_SHFT) & SLV_ADDR_MSK);
	geni_se_setup_m_cmd(gi2c->base, I2C_ADDR_ONLY, m_param);


	/* Get FIFO IRQ */
	writel(1, gi2c->base + SE_GENI_TX_WATERMARK_REG);

	do{
		--time_left;
		udelay(10);
	} while(time_left && !(readl(gi2c->base + SE_GENI_M_IRQ_STATUS) & 0x1));

	return !time_left ||
		(readl(gi2c->base + SE_GENI_M_IRQ_STATUS) & SE_I2C_ERR) ?
		-ENODEV : 0;

}

static int geni_i2c_probe(struct udevice *pdev)
{
	struct geni_i2c_dev *gi2c = dev_get_priv(pdev);
	u32 tx_depth, fifo_disable;
	int ret;

	gi2c->dev = pdev;
	gi2c->base = dev_read_addr(pdev);
	if (gi2c->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_by_name(pdev, "se-clk", &gi2c->clk);
	if (ret)
		return ret;

	ret = clk_enable(&gi2c->clk);
	if (ret < 0)
		return ret;

#ifdef CONFIG_QCOM_GENI_SE_FW_LOAD
	/* need to enable clk with default rate */
	ret = geni_se_fw_load(gi2c->base, QUPV3_SE_I2C);
	if(ret)
	{
		printf("Failed to load SE Firmware\n");
		return ret;
	}
#endif /* CONFIG_QCOM_GENI_SE_FW_LOAD */

	gi2c->clk_freq_out = dev_read_u32_default(pdev, "clock-frequency",
							KHZ(100));

	ret = geni_i2c_clk_map_idx(gi2c);
	if (ret) {
		dev_err(pdev, "Invalid clk frequency %d Hz: %d\n",
			gi2c->clk_freq_out, ret);
		return ret;
	}

	fifo_disable = readl(gi2c->base + GENI_IF_DISABLE_RO) & FIFO_IF_DISABLE;
	if (fifo_disable) {
		printf("DMA NOT ENABLED!\n");
		return 0;
	} else {
		get_geni_se_version(pdev);
		tx_depth = geni_se_get_tx_fifo_depth(gi2c->base,
							gi2c->geni_se_version);
		gi2c->tx_wm = tx_depth - 1;
		geni_se_init(gi2c, gi2c->tx_wm, tx_depth);
		geni_se_config_packing(gi2c->base, BITS_PER_BYTE,
				       true, true, true);
	}

	return 0;

}

static const struct dm_i2c_ops geni_i2c_ops = {
	.xfer		= geni_i2c_xfer,
	.probe_chip	= geni_i2c_probe_chip,
};

static const struct udevice_id geni_i2c_dt_match[] = {
	{ .compatible = "qcom,msm-geni-i2c", },
	{}
};

U_BOOT_DRIVER(i2c_qupv3) = {
	.name	= "i2c_geni_qcom",
	.id	= UCLASS_I2C,
	.probe  = geni_i2c_probe,
	.of_match	= geni_i2c_dt_match,
	.priv_auto	= sizeof(struct geni_i2c_dev),
	.ops	= &geni_i2c_ops,
};

MODULE_DESCRIPTION("I2C Controller Driver for GENI based QUP cores");
MODULE_LICENSE("GPL v2");
