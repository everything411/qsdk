// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2017-2018, The Linux foundation. All rights reserved.
// Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.

#include <asm/gpio.h>
#include <asm/io.h>
#include <clk.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <linux/delay.h>
#include <cpu_func.h>
#include <spi.h>
#include <misc.h>
#if defined(CONFIG_NOR_BLK)
#include <blk.h>
#include <part.h>
#endif
#ifdef CONFIG_QCOM_GENI_SE_FW_LOAD
#include <geni_se.h>
#endif /* CONFIG_QCOM_GENI_SE_FW_LOAD */

/* SPI SE specific registers and respective register fields */
#define SE_SPI_CPHA			0x224
#define CPHA				BIT(0)

#define SE_SPI_LOOPBACK			0x22c
#define LOOPBACK_ENABLE			0x1

#define SE_SPI_CPOL			0x230
#define CPOL				BIT(2)

#define SE_SPI_DEMUX_OUTPUT_INV		0x24c
#define SE_SPI_DEMUX_SEL		0x250

#define SE_SPI_TRANS_CFG		0x25c
#define CS_TOGGLE			BIT(0)

#define SE_SPI_WORD_LEN			0x268
#define WORD_LEN_MSK			GENMASK(9, 0)
#define MIN_WORD_LEN			4

#define SE_SPI_TX_TRANS_LEN		0x26c
#define SE_SPI_RX_TRANS_LEN		0x270
#define TRANS_LEN_MSK			GENMASK(23, 0)

/* M_CMD OP codes for SPI */
#define SPI_TX_ONLY			1
#define SPI_RX_ONLY			2

/* M_CMD params for SPI */
#define FRAGMENTATION			BIT(2)

/* Registers*/
#define GENI_SER_M_CLK_CFG		0x48
#define SE_GENI_M_CMD0			0x600
#define SE_GENI_M_CMD_CTRL_REG          0x604
#define SE_GENI_M_IRQ_CLEAR		0x618
#define SE_GENI_M_IRQ_STATUS		0x610
#define SE_GENI_M_IRQ_EN		0x614
#define SE_GENI_S_IRQ_CLEAR             0x648
#define SE_GENI_TX_FIFOn		0x700
#define SE_GENI_RX_FIFOn		0x780
#define SE_GENI_RX_FIFO_STATUS		0x804
#define SE_GENI_TX_PACKING_CFG0		0x260
#define SE_GENI_TX_PACKING_CFG1		0x264
#define SE_GENI_RX_PACKING_CFG0		0x284
#define SE_GENI_RX_PACKING_CFG1		0x288

/* GENI_OUTPUT_CTRL fields */
#define SE_GENI_RX_WATERMARK_REG	0x810
#define SE_GENI_RX_RFR_WATERMARK_REG	0x814
#define SE_DMA_TX_IRQ_STAT		0xc40
#define SE_DMA_TX_IRQ_CLR		0xc44
#define SE_DMA_RX_IRQ_STAT		0xd40
#define SE_DMA_RX_IRQ_CLR		0xd44
#define SE_GENI_BYTE_GRAN		0x254

/* GENI_SER_M_CLK_CFG/GENI_SER_S_CLK_CFG */
#define CLK_DIV_SHFT			4

/* SE_HW_PARAM_0 fields */
#define TX_FIFO_WIDTH_SHFT		24
#define TX_FIFO_DEPTH_MSK_256B		(GENMASK(23, 16))
#define TX_FIFO_DEPTH_SHFT		16

/* GENI_M_CMD_CTRL_REG */
#define M_GENI_CMD_CANCEL		BIT(2)
#define M_GENI_CMD_ABORT		BIT(1)
#define SE_IRQ_EN			0xe1c

/* SE_IRQ_EN fields */
#define DMA_RX_IRQ_EN			BIT(0)
#define DMA_TX_IRQ_EN			BIT(1)
#define GENI_M_IRQ_EN			BIT(2)
#define GENI_S_IRQ_EN			BIT(3)

#define SE_GENI_DMA_MODE_EN		0x258
/* SE_GENI_DMA_MODE_EN */
#define GENI_DMA_MODE_EN		BIT(0)
#define SE_GSI_EVENT_EN			0xe18

/* GENI_M_IRQ_EN fields */
#define M_CMD_DONE_EN			BIT(0)
#define M_CMD_OVERRUN_EN		BIT(1)
#define M_ILLEGAL_CMD_EN		BIT(2)
#define M_CMD_FAILURE_EN		BIT(3)
#define M_CMD_CANCEL_EN			BIT(4)
#define M_CMD_ABORT_EN			BIT(5)
#define M_IO_DATA_DEASSERT_EN		BIT(22)
#define M_IO_DATA_ASSERT_EN		BIT(23)
#define M_RX_FIFO_RD_ERR_EN		BIT(24)
#define M_RX_FIFO_WR_ERR_EN		BIT(25)
#define M_RX_FIFO_WATERMARK_EN		BIT(26)
#define M_RX_FIFO_LAST_EN		BIT(27)
#define M_TX_FIFO_RD_ERR_EN		BIT(28)
#define M_TX_FIFO_WR_ERR_EN		BIT(29)
#define M_TX_FIFO_WATERMARK_EN		BIT(30)

#define M_COMMON_GENI_M_IRQ_EN		(GENMASK(6, 1) | \
			M_IO_DATA_DEASSERT_EN | \
			M_IO_DATA_ASSERT_EN | M_RX_FIFO_RD_ERR_EN | \
			M_RX_FIFO_WR_ERR_EN | M_TX_FIFO_RD_ERR_EN | \
			M_TX_FIFO_WR_ERR_EN)

#define M_OPCODE_SHFT			27
#define M_PARAMS_MSK			GENMASK(26, 0)

/*  GENI_RX_FIFO_STATUS fields */
#define RX_LAST				BIT(31)
#define RX_LAST_BYTE_VALID_MSK		GENMASK(30, 28)
#define RX_LAST_BYTE_VALID_SHFT		28
#define RX_FIFO_WC_MSK			GENMASK(24, 0)

/* SE_DMA_TX_IRQ_STAT Register fields */
#define TX_DMA_DONE			BIT(0)
#define TX_RESET_DONE			BIT(3)

/* SE_DMA_RX_IRQ_STAT Register fields */
#define RX_DMA_DONE			BIT(0)
#define RX_RESET_DONE			BIT(3)

#define GENI_SE_DMA_DONE_EN		BIT(0)
#define GENI_SE_DMA_EOT_EN		BIT(1)
#define GENI_SE_DMA_AHB_ERR_EN		BIT(2)
#define GENI_SE_DMA_EOT_BUF		BIT(0)

#define SE_DMA_TX_PTR_L			0xc30
#define SE_DMA_TX_PTR_H			0xc34
#define SE_DMA_TX_ATTR			0xc38
#define SE_DMA_TX_LEN			0xc3c
#define SE_DMA_TX_IRQ_EN_SET		0xc4c
#define SE_DMA_TX_FSM_RST		0xc58
#define SE_DMA_RX_PTR_L			0xd30
#define SE_DMA_RX_PTR_H			0xd34
#define SE_DMA_RX_ATTR			0xd38
#define SE_DMA_RX_LEN			0xd3c
#define SE_DMA_RX_IRQ_EN_SET		0xd4c
#define SE_DMA_RX_FSM_RST		0xd58

#define SPI_BITLEN_MSK			0x07
#define MAX_TIMEOUT			10

enum geni_se_xfer_status {
	XFER_IN_PROGRESS = 0,
	XFER_COMPLETE,
	XFER_ERR,
};

struct qupv3_spi_xfer_info {
	u8 *tx_buf;
	u32 tx_rem;
	u8 *rx_buf;
	u32 rx_rem;
	u32 len;
	bool is_end;
	uint8_t dma_mode;
	uint8_t stat;
};

struct qupv3_spi_priv {
	phys_addr_t base;
	struct clk clk;
	bool dma_disable;
	bool cs_high;
	uint8_t num_cs;
	uint8_t bits_per_word;
	uint32_t max_hz;
	uint32_t bus_speed;
	uint8_t oversampling;
	uint32_t tx_fifo_depth;
	uint32_t fifo_width_bits;
	uint32_t geni_se_version;
};

static void geni_cfg_sclk(phys_addr_t base_address, u32 clk_div)
{
	u32 clk_cfg = 0;

	clk_cfg |= SER_CLK_EN;
	clk_cfg |= (clk_div << CLK_DIV_SHFT);

	writel(clk_cfg, base_address + GENI_SER_M_CLK_CFG);
}

static int qupv3_spi_set_speed(struct udevice *dev, uint speed)
{
	struct qupv3_spi_priv *priv = dev_get_priv(dev);
	u32 clk_div, desired_hz = speed * priv->oversampling;

	if (desired_hz == priv->bus_speed)
		return 0;

	if (desired_hz > priv->max_hz)
		return -EINVAL;

	clk_div = priv->max_hz / desired_hz;
	clk_div += ((priv->max_hz % desired_hz) ? 1 : 0);

	if (clk_div > 0xFFFFF) {
		pr_err("%s: Can't find matching DFS entry for speed %d\n",
				__func__, desired_hz);
		return -EINVAL;
	}

	geni_cfg_sclk(priv->base, clk_div);
	priv->bus_speed = desired_hz;
	return 0;
}

static int qupv3_spi_set_mode(struct udevice *dev, uint mode)
{
	struct qupv3_spi_priv *priv = dev_get_priv(dev);
	u32 cpha, cpol;

	switch (mode) {
	case SPI_MODE_1:
		cpol = 0;
		cpha = CPHA;
		break;
	case SPI_MODE_2:
		cpol = CPOL;
		cpha = 0;
		break;
	case SPI_MODE_3:
		cpol = CPOL;
		cpha = CPHA;
		break;
	default:
	case SPI_MODE_0:
		cpol = 0;
		cpha = 0;
		break;
	}

	if (mode & SPI_LOOP) {
		writel(LOOPBACK_ENABLE, priv->base + SE_SPI_LOOPBACK);
	}

	writel(cpha, priv->base + SE_SPI_CPHA);
	writel(cpol, priv->base + SE_SPI_CPOL);

	if (mode & SPI_CS_HIGH)
		priv->cs_high = true;
	else
		priv->cs_high = false;

	return 0;
}

static void geni_se_setup_m_cmd(phys_addr_t base, u32 cmd, u32 params)
{
	u32 m_cmd;

	m_cmd = (cmd << M_OPCODE_SHFT) | (params & M_PARAMS_MSK);
	writel(m_cmd, base + SE_GENI_M_CMD0);
}

static int qupv3_spi_claim_bus(struct udevice *dev)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	struct dm_spi_slave_plat *slave_plat = dev_get_parent_plat(dev);
	struct spi_slave *slave = dev_get_parent_priv(dev);

	writel(slave_plat->cs, priv->base + SE_SPI_DEMUX_SEL);
	if (priv->cs_high)
		writel(BIT(slave_plat->cs),
				priv->base + SE_SPI_DEMUX_OUTPUT_INV);

	priv->bits_per_word = slave->wordlen;
	geni_se_config_packing(priv->base, priv->bits_per_word,
			!(slave_plat->mode & SPI_LSB_FIRST), true, true);
	writel(((priv->bits_per_word - MIN_WORD_LEN) & WORD_LEN_MSK),
			priv->base + SE_SPI_WORD_LEN);

	return 0;
}

static unsigned int geni_byte_per_fifo_word(struct udevice *dev)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);

	return priv->fifo_width_bits / BITS_PER_BYTE;
}

static void geni_spi_handle_tx(struct udevice *dev,
		struct qupv3_spi_xfer_info* xfer)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	unsigned int max_bytes;
	const u8 *tx_buf;
	unsigned int bytes_per_fifo_word = geni_byte_per_fifo_word(dev);
	unsigned int i = 0;

	max_bytes = (priv->tx_fifo_depth - 1) * bytes_per_fifo_word;
	if (xfer->tx_rem < max_bytes)
		max_bytes = xfer->tx_rem;

	tx_buf = xfer->tx_buf + xfer->len - xfer->tx_rem;
	while (i < max_bytes) {
		unsigned int j;
		unsigned int bytes_to_write;
		u32 fifo_word = 0;
		u8 *fifo_byte = (u8 *)&fifo_word;

		bytes_to_write = min(bytes_per_fifo_word, max_bytes - i);
		for (j = 0; j < bytes_to_write; j++)
			fifo_byte[j] = tx_buf[i++];
		writel(fifo_word, priv->base + SE_GENI_TX_FIFOn);
	}

	xfer->tx_rem -= max_bytes;

	if (!xfer->tx_rem) {
		writel(0, priv->base + SE_GENI_TX_WATERMARK_REG);
	}

	return;
}

static void geni_spi_handle_rx(struct udevice *dev,
		struct qupv3_spi_xfer_info* xfer)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	u32 rx_fifo_status;
	unsigned int rx_bytes;
	unsigned int rx_last_byte_valid;
	u8 *rx_buf;
	unsigned int bytes_per_fifo_word = geni_byte_per_fifo_word(dev);
	unsigned int i = 0;

	rx_fifo_status = readl(priv->base + SE_GENI_RX_FIFO_STATUS);
	rx_bytes = (rx_fifo_status & RX_FIFO_WC_MSK) * bytes_per_fifo_word;
	if (rx_fifo_status & RX_LAST) {
		rx_last_byte_valid = rx_fifo_status & RX_LAST_BYTE_VALID_MSK;
		rx_last_byte_valid >>= RX_LAST_BYTE_VALID_SHFT;
		if (rx_last_byte_valid && rx_last_byte_valid < 4)
			rx_bytes -= bytes_per_fifo_word - rx_last_byte_valid;
	}

	if (!xfer->rx_buf) {
		for (i = 0; i < DIV_ROUND_UP(rx_bytes,
					bytes_per_fifo_word); i++)
			readl(priv->base + SE_GENI_RX_FIFOn);
		return;
	}

	if (xfer->rx_rem < rx_bytes)
		rx_bytes = xfer->rx_rem;

	rx_buf = xfer->rx_buf + xfer->len - xfer->rx_rem;
	while (i < rx_bytes) {
		u32 fifo_word = 0;
		u8 *fifo_byte = (u8 *)&fifo_word;
		unsigned int bytes_to_read;
		unsigned int j;

		bytes_to_read = min(bytes_per_fifo_word, rx_bytes - i);
		fifo_word = readl(priv->base + SE_GENI_RX_FIFOn);
		for (j = 0; j < bytes_to_read; j++)
			rx_buf[i++] = fifo_byte[j];
	}
	xfer->rx_rem -= rx_bytes;
}

static void geni_spi_isr(struct udevice *dev, struct qupv3_spi_xfer_info* xfer)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	u32 m_irq;

	m_irq = readl(priv->base + SE_GENI_M_IRQ_STATUS);
	if (!m_irq)
		return;

	if (m_irq & (M_CMD_OVERRUN_EN | M_ILLEGAL_CMD_EN | M_CMD_FAILURE_EN |
				M_RX_FIFO_RD_ERR_EN | M_RX_FIFO_WR_ERR_EN |
				M_TX_FIFO_RD_ERR_EN | M_TX_FIFO_WR_ERR_EN)) {
		printf("%s: Unexpected IRQ err status %#010x\n",
				__func__, m_irq);
		xfer->stat = XFER_ERR;
	}

	if (xfer->dma_mode) {
                u32 dma_tx_status = readl(priv->base + SE_DMA_TX_IRQ_STAT);
                u32 dma_rx_status = readl(priv->base + SE_DMA_RX_IRQ_STAT);

                if (dma_tx_status)
                        writel(dma_tx_status, priv->base + SE_DMA_TX_IRQ_CLR);
                if (dma_rx_status)
                        writel(dma_rx_status, priv->base + SE_DMA_RX_IRQ_CLR);
                if (dma_tx_status & TX_DMA_DONE)
                        xfer->tx_rem = 0;
                if (dma_rx_status & RX_DMA_DONE) {
			flush_cache((unsigned long)xfer->rx_buf,
					(unsigned long)xfer->rx_rem);
                        xfer->rx_rem = 0;
		}

		if (!xfer->rx_rem && !xfer->tx_rem)
			xfer->stat = XFER_COMPLETE;
	} else {
		if ((m_irq & M_RX_FIFO_WATERMARK_EN) ||
				(m_irq & M_RX_FIFO_LAST_EN))
			geni_spi_handle_rx(dev, xfer);

		if (m_irq & M_TX_FIFO_WATERMARK_EN)
			geni_spi_handle_tx(dev, xfer);

		if (m_irq & M_CMD_DONE_EN) {
			xfer->stat = XFER_COMPLETE;
			if (xfer->tx_rem) {
				xfer->stat = XFER_ERR;
				printf("%s: Premature done. tx_rem = %d\n",
						__func__, xfer->tx_rem);
			}

			if (xfer->rx_rem) {
				xfer->stat = XFER_ERR;
				printf("%s: Premature done. rx_rem = %d\n",
						__func__, xfer->rx_rem);
			}
		}
	}

	writel(m_irq, priv->base + SE_GENI_M_IRQ_CLEAR);
	return;
}


static void geni_se_tx_dma_prep(struct udevice *dev,
					dma_addr_t buf, size_t len)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	u32 val;

	flush_cache((unsigned long)buf, (unsigned long)len);
	val = GENI_SE_DMA_DONE_EN;
	val |= GENI_SE_DMA_EOT_EN;
	val |= GENI_SE_DMA_AHB_ERR_EN;
	writel(val, priv->base + SE_DMA_TX_IRQ_EN_SET);
	writel(lower_32_bits(buf), priv->base + SE_DMA_TX_PTR_L);
	writel(upper_32_bits(buf), priv->base + SE_DMA_TX_PTR_H);
	writel(GENI_SE_DMA_EOT_BUF, priv->base + SE_DMA_TX_ATTR);
	writel(len, priv->base + SE_DMA_TX_LEN);
	return;
}

static void geni_se_rx_dma_prep(struct udevice *dev,
					dma_addr_t buf, size_t len)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	u32 val;

	flush_cache((unsigned long)buf, (unsigned long)len);
	val = GENI_SE_DMA_DONE_EN;
	val |= GENI_SE_DMA_EOT_EN;
	val |= GENI_SE_DMA_AHB_ERR_EN;
	writel(val, priv->base + SE_DMA_RX_IRQ_EN_SET);
	writel(lower_32_bits(buf), priv->base + SE_DMA_RX_PTR_L);
	writel(upper_32_bits(buf), priv->base + SE_DMA_RX_PTR_H);

	/* RX does not have EOT buffer type bit. So just reset RX_ATTR */
	writel(0, priv->base + SE_DMA_RX_ATTR);
	writel(len, priv->base + SE_DMA_RX_LEN);
	return;
}

static void geni_se_config_dma_mode(phys_addr_t base)
{
	u32 val, val_old;

	geni_se_irq_clear(base);

	val_old = val = readl(base + SE_GENI_M_IRQ_EN);
	val &= ~(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN);
	val &= ~(M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);
	if (val != val_old)
		writel(val, base + SE_GENI_M_IRQ_EN);

	val_old = val = readl(base + SE_GENI_DMA_MODE_EN);
	val |= GENI_DMA_MODE_EN;
	if (val != val_old)
		writel(val, base + SE_GENI_DMA_MODE_EN);
}

static int wait_for_cmd_completion(struct udevice *dev, uint32_t cmd_mask)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	uint32_t m_irq;
	uint8_t count = 10;

	do {
		udelay(MAX_TIMEOUT);
		m_irq = readl(priv->base + SE_GENI_M_IRQ_STATUS);
		if (m_irq & (M_CMD_OVERRUN_EN | M_ILLEGAL_CMD_EN |
					M_CMD_FAILURE_EN)) {
			printf("%s: Unexpected CMD err status %#010x\n",
					__func__, m_irq);
			return -EIO;
		}
	} while (!(m_irq & cmd_mask) && (count--));

	if (!(m_irq & cmd_mask) && !count)
		return -ETIMEDOUT;
	else
		writel(cmd_mask, priv->base + SE_GENI_M_IRQ_CLEAR);

	return 0;

}

static int wait_for_dma_rst(phys_addr_t irq_stat_reg,
		phys_addr_t irq_clr_reg, uint32_t stat_mask)
{
	uint32_t val;
	uint8_t count = 10;

	do {
		udelay(MAX_TIMEOUT);
		val = readl(irq_stat_reg);
	} while (!(val & stat_mask) && (count--));

	if (!(val & stat_mask) && !count)
		return -ETIMEDOUT;
	else
		writel(stat_mask, irq_clr_reg);

	return 0;
}

static int spi_geni_handle_err(struct udevice *dev,
		struct qupv3_spi_xfer_info* xfer)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	int ret = 0;

	if (!xfer->dma_mode)
		writel(0, priv->base + SE_GENI_TX_WATERMARK_REG);

	writel(M_GENI_CMD_CANCEL, priv->base + SE_GENI_M_CMD_CTRL_REG);
	ret = wait_for_cmd_completion(dev, M_CMD_CANCEL_EN);
	if (ret)
		goto eret;

	writel(M_GENI_CMD_ABORT, priv->base + SE_GENI_M_CMD_CTRL_REG);
	ret = wait_for_cmd_completion(dev, M_CMD_ABORT_EN);
	if (ret)
		goto eret;

	if (xfer->dma_mode) {
		if (xfer->tx_buf) {
			writel(1, priv->base + SE_DMA_TX_FSM_RST);
			ret = wait_for_dma_rst(priv->base + SE_DMA_TX_IRQ_STAT,
				        priv->base + SE_DMA_TX_IRQ_CLR,
					TX_RESET_DONE);
			if (ret)
				goto eret;
		}

		if (xfer->rx_buf) {
			writel(1, priv->base + SE_DMA_RX_FSM_RST);
			ret = wait_for_dma_rst(priv->base + SE_DMA_RX_IRQ_STAT,
				        priv->base + SE_DMA_RX_IRQ_CLR,
					RX_RESET_DONE);
			if (ret)
				goto eret;
		}
	}

eret:
	return ret;
}

static int do_spi_xfer(struct udevice *dev, struct qupv3_spi_xfer_info *xfer)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	uint32_t len, m_cmd = 0;
	ulong timeout_cnt;
	int ret = 0;

	if (priv->bits_per_word > 16)
		len = xfer->len / sizeof(uint32_t);
	else if (priv->bits_per_word > 8)
		len = xfer->len / sizeof(uint16_t);
	else
		len = xfer->len;
	len &= TRANS_LEN_MSK;

	xfer->tx_rem = 0;
	xfer->rx_rem = 0;

	if (xfer->tx_buf) {
		m_cmd |= SPI_TX_ONLY;
		xfer->tx_rem = xfer->len;
	}

	if (xfer->rx_buf) {
		m_cmd |= SPI_RX_ONLY;
		xfer->rx_rem = xfer->len;
	}

	writel((xfer->tx_buf ? len:0), priv->base + SE_SPI_TX_TRANS_LEN);
	writel((xfer->rx_buf ? len:0), priv->base + SE_SPI_RX_TRANS_LEN);
	timeout_cnt = priv->bits_per_word * len;

	if (xfer->dma_mode) {
		geni_se_config_dma_mode(priv->base);
		geni_se_setup_m_cmd(priv->base, m_cmd,
				(xfer->is_end ? 0x0 : FRAGMENTATION));

		if (m_cmd & SPI_RX_ONLY)
			geni_se_rx_dma_prep(dev, (dma_addr_t)xfer->rx_buf,
					xfer->rx_rem);

		if (m_cmd & SPI_TX_ONLY)
			geni_se_tx_dma_prep(dev, (dma_addr_t)xfer->tx_buf,
					xfer->tx_rem);
	} else {
		geni_se_config_fifo_mode(priv->base);
		writel(1, priv->base + SE_GENI_TX_WATERMARK_REG);
		writel(1, priv->base + SE_GENI_RX_WATERMARK_REG);

		geni_se_setup_m_cmd(priv->base, m_cmd,
				(xfer->is_end ? 0x0 : FRAGMENTATION));
		if (m_cmd & SPI_TX_ONLY) {
			geni_spi_handle_tx(dev, xfer);
		}
	}

	xfer->stat = XFER_IN_PROGRESS;

	do {
		udelay(MAX_TIMEOUT);
		geni_spi_isr(dev, xfer);
	} while (!xfer->stat && timeout_cnt--);

	if (!xfer->stat && !timeout_cnt)
		ret = -ETIMEDOUT;

	if (xfer->stat == XFER_ERR)
		ret = -EIO;

	if (ret)
		spi_geni_handle_err(dev, xfer);

	return ret;
}

static int qupv3_spi_xfer(struct udevice *dev, unsigned int bitlen,
		const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev_get_parent(dev);
	struct qupv3_spi_priv *priv = dev_get_priv(bus);
	struct qupv3_spi_xfer_info xfer;
	uint32_t fifo_max_size = priv->tx_fifo_depth * priv->fifo_width_bits /
		priv->bits_per_word;
	int ret = 0;

	if (bitlen & SPI_BITLEN_MSK) {
		printf("%s: Invalid bit length \n", __func__);
		return -EINVAL;
	}

	xfer.tx_buf = (void *)dout;
	xfer.rx_buf = din;
	xfer.len = bitlen >> 3;
	xfer.is_end = ((flags & SPI_XFER_END) ? true : false);
	xfer.dma_mode = ((xfer.len > fifo_max_size) ?
			(true & !priv->dma_disable): false);
	debug("%s: len:%d out:%p in:%p mode:%s\n", __func__, xfer.len,
			dout, din, xfer.dma_mode ? "DMA" : "FIFO");
	ret = do_spi_xfer(dev, &xfer);
	if (ret != 0)
		return ret;

	return ret;
}

static int qupv3_spi_hw_init(struct udevice *dev)
{
	struct qupv3_spi_priv *priv = dev_get_priv(dev);
	u32 val = 0;

	geni_se_irq_clear(priv->base);
	geni_se_io_set_mode(priv->base);

	val = readl(priv->base + SE_GENI_M_IRQ_EN);
	val |= M_COMMON_GENI_M_IRQ_EN;
	writel(val, priv->base + SE_GENI_M_IRQ_EN);

	geni_se_config_fifo_mode(priv->base);

	/* We always control CS manually */
	val = readl(priv->base + SE_SPI_TRANS_CFG);
	val &= ~CS_TOGGLE;
	writel(val, priv->base + SE_SPI_TRANS_CFG);
	return 0;
}

static void geni_set_oversampling(struct udevice *dev)
{
	struct qupv3_spi_priv *priv = dev_get_priv(dev);
	struct udevice *parent_dev = dev_get_parent(dev);
	int ret;

	if (device_get_uclass_id(parent_dev) != UCLASS_MISC)
		return;

	ret = misc_read(parent_dev, QUP_HW_VER_REG,
			&priv->geni_se_version, sizeof(priv->geni_se_version));
	if (ret != sizeof(priv->geni_se_version))
		return;

	if (priv->geni_se_version == QUP_SE_VERSION_1_0)
		priv->oversampling = 2;
	else
		priv->oversampling = 1;
}

static u32 geni_se_get_tx_fifo_width(const struct udevice *dev)
{
	struct qupv3_spi_priv *priv = dev_get_priv(dev);
	u32 tx_fifo_width;

	tx_fifo_width = ((readl(priv->base + SE_HW_PARAM_0) &
				TX_FIFO_WIDTH_MSK) >> TX_FIFO_WIDTH_SHFT);
	return tx_fifo_width;
}

static int qupv3_spi_probe(struct udevice *dev)
{
	struct qupv3_spi_priv *priv = dev_get_priv(dev);
	int ret;
#if defined(CONFIG_NOR_BLK)
	struct blk_desc *bdesc;
	struct udevice *bdev;
#endif

	priv->max_hz = dev_read_u32_default(dev, "clock-frequency", 0);
	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;

	ret = clk_set_rate(&priv->clk, priv->max_hz);
	if (ret < 0)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret < 0)
		return ret;

#ifdef CONFIG_QCOM_GENI_SE_FW_LOAD
	/* need to enable clk with default rate */
	ret = geni_se_fw_load(priv->base, QUPV3_SE_SPI);
	if(ret)
	{
		printf("Failed to load SE Firmware\n");
		return ret;
	}

#endif /* CONFIG_QCOM_GENI_SE_FW_LOAD */

	priv->num_cs = dev_read_u32_default(dev, "num-cs", 1);
	priv->dma_disable = dev_read_bool(dev, "qup-dma-disable");

	geni_set_oversampling(dev);

	priv->tx_fifo_depth = geni_se_get_tx_fifo_depth(priv->base,
							priv->geni_se_version);
	priv->fifo_width_bits = geni_se_get_tx_fifo_width(dev);

	ret = qupv3_spi_hw_init(dev);
	if (ret)
		return -EIO;

#if defined(CONFIG_NOR_BLK)
	if (dev_read_bool(dev, "nor-blk-enable")) {
		ret = blk_create_devicef(dev, "nor_blk", "blk", UCLASS_SPI,
						dev_seq(dev),
						CONFIG_NOR_BLK_SIZE, 0, &bdev);
		if (ret) {
			printf("Cannot create Nor block device\n");
		} else {
			bdesc = dev_get_uclass_plat(bdev);
			bdesc->removable = 1;
			bdesc->part_type = PART_TYPE_EFI;
		}
	}
#endif
	return 0;
}

static const struct dm_spi_ops qupv3_spi_ops = {
	.claim_bus	= qupv3_spi_claim_bus,
	.xfer		= qupv3_spi_xfer,
	.set_speed	= qupv3_spi_set_speed,
	.set_mode	= qupv3_spi_set_mode,
	/*
	 * cs_info is not needed, since we require all chip selects to be
	 * in the device tree explicitly
	 */
};

static const struct udevice_id qupv3_spi_ids[] = {
	{ .compatible = "qcom,msm-geni-spi", },
	{ }
};

U_BOOT_DRIVER(spi_qupv3) = {
	.name	= "spi_geni_qcom",
	.id	= UCLASS_SPI,
	.of_match = qupv3_spi_ids,
	.ops	= &qupv3_spi_ops,
	.priv_auto	= sizeof(struct qupv3_spi_priv),
	.probe	= qupv3_spi_probe,
};
