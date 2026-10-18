// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2017-2018, The Linux foundation. All rights reserved.

/* Disable MMIO tracing to prevent excessive logging of unwanted MMIO traces */
#define __DISABLE_TRACE_MMIO__

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeirq.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/dmaengine.h>
#include <linux/dma/qcom-gpi-dma.h>
#include <linux/dma-mapping.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <dt-bindings/interconnect/qcom,icc.h>

/* UART specific GENI registers */
#define SE_UART_LOOPBACK_CFG		0x22c
#define SE_UART_IO_MACRO_CTRL		0x240
#define SE_UART_TX_TRANS_CFG		0x25c
#define SE_UART_TX_WORD_LEN		0x268
#define SE_UART_TX_STOP_BIT_LEN		0x26c
#define SE_UART_TX_TRANS_LEN		0x270
#define SE_UART_RX_TRANS_CFG		0x280
#define SE_UART_RX_WORD_LEN		0x28c
#define SE_UART_RX_STALE_CNT		0x294
#define SE_UART_TX_PARITY_CFG		0x2a4
#define SE_UART_RX_PARITY_CFG		0x2a8
#define SE_UART_MANUAL_RFR		0x2ac

/* SE_UART_TRANS_CFG */
#define UART_TX_PAR_EN			BIT(0)
#define UART_CTS_MASK			BIT(1)

/* SE_UART_TX_STOP_BIT_LEN */
#define TX_STOP_BIT_LEN_1		0
#define TX_STOP_BIT_LEN_2		2

/* SE_UART_RX_TRANS_CFG */
#define UART_RX_PAR_EN			BIT(3)

/* SE_UART_RX_WORD_LEN */
#define RX_WORD_LEN_MASK		GENMASK(9, 0)

/* SE_UART_RX_STALE_CNT */
#define RX_STALE_CNT			GENMASK(23, 0)

/* SE_UART_TX_PARITY_CFG/RX_PARITY_CFG */
#define PAR_CALC_EN			BIT(0)
#define PAR_EVEN			0x00
#define PAR_ODD				0x01
#define PAR_SPACE			0x10

/* SE_UART_MANUAL_RFR register fields */
#define UART_MANUAL_RFR_EN		BIT(31)
#define UART_RFR_NOT_READY		BIT(1)
#define UART_RFR_READY			BIT(0)

/* UART M_CMD OP codes */
#define UART_START_TX			0x1
/* UART S_CMD OP codes */
#define UART_START_READ			0x1
#define UART_PARAM			0x1
#define UART_PARAM_RFR_OPEN		BIT(7)

#define UART_OVERSAMPLING		32
#define STALE_TIMEOUT			16
#define DEFAULT_BITS_PER_CHAR		10
#define STALE_COUNT                     (DEFAULT_BITS_PER_CHAR * STALE_TIMEOUT)
#define GENI_UART_CONS_PORTS		1
#define GENI_UART_PORTS			3

/* Timeout for waiting on completion */
#define POLL_WAIT_TIMEOUT_MSEC		100
#define DEF_FIFO_DEPTH_WORDS		16
#define DEF_TX_WM			2
#define DEF_FIFO_WIDTH_BITS		32
#define UART_RX_WM			2

/* SE_UART_LOOPBACK_CFG */
#define RX_TX_SORTED			BIT(0)
#define CTS_RTS_SORTED			BIT(1)
#define RX_TX_CTS_RTS_SORTED		(RX_TX_SORTED | CTS_RTS_SORTED)

/* UART pin swap value */
#define DEFAULT_IO_MACRO_IO0_IO1_MASK	GENMASK(3, 0)
#define IO_MACRO_IO0_SEL		0x3
#define DEFAULT_IO_MACRO_IO2_IO3_MASK	GENMASK(15, 4)
#define IO_MACRO_IO2_IO3_SWAP		0x4640

/* We always configure 4 bytes per FIFO word */
#define BYTES_PER_FIFO_WORD		4U

#define DMA_RX_BUF_SIZE		2048

/* GSI specific defines */
#define NUM_RX_BUF                      4

#define QCOM_M_IRQ_BITS        (M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN | \
				M_CMD_CANCEL_EN | M_CMD_ABORT_EN | \
				M_IO_DATA_ASSERT_EN)
#define QCOM_S_IRQ_BITS        (S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN | \
				S_CMD_CANCEL_EN | S_CMD_ABORT_EN)

struct qcom_geni_device_data {
	bool console;
	enum geni_se_xfer_mode mode;
};

struct uart_gsi {
	struct dma_chan *tx_c;
	struct dma_chan *rx_c;
	struct qcom_gpi_tre tx_cfg0_t;
	struct qcom_gpi_tre rx_cfg0_t;
	struct qcom_gpi_tre tx_go_t;
	struct qcom_gpi_tre rx_go_t;
	struct qcom_gpi_tre tx_t;
	struct qcom_gpi_tre rx_t[5];
	dma_addr_t tx_ph;
	dma_addr_t rx_ph;
	struct scatterlist tx_sg[5];
	struct scatterlist rx_sg[6];
	struct dma_async_tx_descriptor *tx_desc;
	struct dma_async_tx_descriptor *rx_desc;
	struct qcom_gpi_dma_async_tx_cb_param tx_cb;
	struct qcom_gpi_dma_async_tx_cb_param rx_cb;
};

struct qcom_geni_private_data {
	/* NOTE: earlycon port will have NULL here */
	struct uart_driver *drv;

	u32 poll_cached_bytes;
	unsigned int poll_cached_bytes_cnt;

	u32 write_cached_bytes;
	unsigned int write_cached_bytes_cnt;
};

struct qcom_geni_serial_port {
	struct uart_port uport;
	struct geni_se se;
	const char *name;
	u32 tx_fifo_depth;
	u32 tx_fifo_width;
	u32 rx_fifo_depth;
	dma_addr_t tx_dma_addr;
	dma_addr_t rx_dma_addr;
	bool setup;
	unsigned long poll_timeout_us;
	unsigned long clk_rate;
	void *rx_buf;
	u32 loopback;
	bool brk;

	unsigned int tx_remaining;
	unsigned int tx_queued;
	unsigned int xmit_size;
	int wakeup_irq;
	bool rx_tx_swap;
	bool cts_rts_swap;

	/* GSI mode support */
	bool gsi_mode;
	struct uart_gsi *gsi;
	void *rx_gsi_buf[NUM_RX_BUF];
	dma_addr_t rx_gsi_dma_addr[NUM_RX_BUF];
	int rx_buf_idx;
	struct workqueue_struct *tx_wq;
	struct workqueue_struct *rx_wq;
	struct work_struct tx_xfer_work;
	struct work_struct rx_cancel_work;
	struct work_struct tx_cancel_work;
	struct completion tx_xfer;
	struct completion rx_cancel;
	struct completion xfer;
	bool gsi_rx_done;
	bool port_setup;
	atomic_t stop_rx_inprogress;
	struct qcom_geni_private_data private_data;
	const struct qcom_geni_device_data *dev_data;
};

static const struct uart_ops qcom_geni_console_pops;
static const struct uart_ops qcom_geni_uart_pops;
static struct uart_driver qcom_geni_console_driver;
static struct uart_driver qcom_geni_uart_driver;

static void qcom_geni_serial_cancel_tx_cmd(struct uart_port *uport);
static int qcom_geni_serial_port_setup(struct uart_port *uport);

static inline struct qcom_geni_serial_port *to_dev_port(struct uart_port *uport)
{
	return container_of(uport, struct qcom_geni_serial_port, uport);
}

static void qcom_geni_uart_gsi_tx_cb(void *ptr);
static void qcom_geni_uart_gsi_rx_cb(void *ptr);
static void qcom_geni_uart_gsi_xfer_tx(struct work_struct *work);
static void qcom_geni_uart_gsi_cancel_tx(struct work_struct *work);
static void qcom_geni_uart_gsi_cancel_rx(struct work_struct *work);
static int qcom_geni_uart_gsi_xfer_rx(struct uart_port *uport);
static void qcom_geni_serial_init_gsi(struct uart_port *uport);
static void setup_config0_tre(struct uart_port *uport, unsigned int bits_per_char,
			      unsigned int clk_div, unsigned int stop_bit_len,
			      unsigned int tx_parity, bool cts_mask,
			      unsigned int rx_parity, unsigned int loopback);
static int qcom_geni_serial_alloc_gsi_rx_bufs(struct uart_port *uport);

static struct qcom_geni_serial_port qcom_geni_uart_ports[GENI_UART_PORTS] = {
	[0] = {
		.uport = {
			.iotype = UPIO_MEM,
			.ops = &qcom_geni_uart_pops,
			.flags = UPF_BOOT_AUTOCONF,
			.line = 0,
		},
	},
	[1] = {
		.uport = {
			.iotype = UPIO_MEM,
			.ops = &qcom_geni_uart_pops,
			.flags = UPF_BOOT_AUTOCONF,
			.line = 1,
		},
	},
	[2] = {
		.uport = {
			.iotype = UPIO_MEM,
			.ops = &qcom_geni_uart_pops,
			.flags = UPF_BOOT_AUTOCONF,
			.line = 2,
		},
	},
};

static struct qcom_geni_serial_port qcom_geni_console_port = {
	.uport = {
		.iotype = UPIO_MEM,
		.ops = &qcom_geni_console_pops,
		.flags = UPF_BOOT_AUTOCONF,
		.line = 0,
	},
};

static int qcom_geni_serial_request_port(struct uart_port *uport)
{
	struct platform_device *pdev = to_platform_device(uport->dev);
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	const struct qcom_geni_device_data *data;

	data = of_device_get_match_data(&pdev->dev);
	if (!data)
		return -EINVAL;
	uport->membase = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(uport->membase))
		return PTR_ERR(uport->membase);
	port->se.base = uport->membase;

#ifdef CONFIG_QCOM_GENI_SE_FW_LOAD
	if (!data->console)
		geni_se_fw_load(&port->se, QUPV3_SE_UART);
	else
		pr_info("Skipping GENI FW load for console UART\n");
#endif /* CONFIG_QCOM_GENI_SE_FW_LOAD */

	return 0;
}

static void qcom_geni_serial_config_port(struct uart_port *uport, int cfg_flags)
{
	if (cfg_flags & UART_CONFIG_TYPE) {
		uport->type = PORT_MSM;
		qcom_geni_serial_request_port(uport);
	}
}

static unsigned int qcom_geni_serial_get_mctrl(struct uart_port *uport)
{
	unsigned int mctrl = TIOCM_DSR | TIOCM_CAR;
	u32 geni_ios;

	if (uart_console(uport)) {
		mctrl |= TIOCM_CTS;
	} else {
		geni_ios = readl(uport->membase + SE_GENI_IOS);
		if (!(geni_ios & IO2_DATA_IN))
			mctrl |= TIOCM_CTS;
	}

	return mctrl;
}

static void qcom_geni_serial_set_mctrl(struct uart_port *uport,
							unsigned int mctrl)
{
	u32 uart_manual_rfr = 0;
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	if (uart_console(uport))
		return;

	if (mctrl & TIOCM_LOOP)
		/* For TX-RX loopback without HW CTRL, set
		 * loopback to RX_TX. when HW flow control
		 * is enabled, update loopback to RX_TX_CTS_RTS_SORTED
		 */
		port->loopback = RX_TX_SORTED;

	if (!(mctrl & TIOCM_RTS) && !uport->suspended)
		uart_manual_rfr = UART_MANUAL_RFR_EN | UART_RFR_NOT_READY;
	writel(uart_manual_rfr, uport->membase + SE_UART_MANUAL_RFR);
}

static const char *qcom_geni_serial_get_type(struct uart_port *uport)
{
	return "MSM";
}

static struct qcom_geni_serial_port *get_port_from_line(int line, bool console)
{
	struct qcom_geni_serial_port *port;
	int nr_ports = console ? GENI_UART_CONS_PORTS : GENI_UART_PORTS;

	if (line < 0 || line >= nr_ports)
		return ERR_PTR(-ENXIO);

	port = console ? &qcom_geni_console_port : &qcom_geni_uart_ports[line];
	return port;
}

static bool qcom_geni_serial_main_active(struct uart_port *uport)
{
	return readl(uport->membase + SE_GENI_STATUS) & M_GENI_CMD_ACTIVE;
}

static bool qcom_geni_serial_secondary_active(struct uart_port *uport)
{
	return readl(uport->membase + SE_GENI_STATUS) & S_GENI_CMD_ACTIVE;
}

static bool qcom_geni_serial_poll_bitfield(struct uart_port *uport,
					   unsigned int offset, u32 field, u32 val)
{
	u32 reg;
	struct qcom_geni_serial_port *port;
	unsigned long timeout_us = 200000;
	struct qcom_geni_private_data *private_data = uport->private_data;

	if (private_data->drv) {
		port = to_dev_port(uport);
		if (port->poll_timeout_us)
			timeout_us = port->poll_timeout_us;
	}

	/*
	 * Use custom implementation instead of readl_poll_atomic since ktimer
	 * is not ready at the time of early console.
	 */
	timeout_us = DIV_ROUND_UP(timeout_us, 10) * 10;
	while (timeout_us) {
		reg = readl(uport->membase + offset);
		if ((reg & field) == val)
			return true;
		udelay(10);
		timeout_us -= 10;
	}
	return false;
}

static bool qcom_geni_serial_poll_bit(struct uart_port *uport,
				      unsigned int offset, u32 field, bool set)
{
	return qcom_geni_serial_poll_bitfield(uport, offset, field, set ? field : 0);
}

static void qcom_geni_serial_setup_tx(struct uart_port *uport, u32 xmit_size)
{
	u32 m_cmd;

	writel(xmit_size, uport->membase + SE_UART_TX_TRANS_LEN);
	m_cmd = UART_START_TX << M_OPCODE_SHFT;
	writel(m_cmd, uport->membase + SE_GENI_M_CMD0);
}

static void qcom_geni_serial_poll_tx_done(struct uart_port *uport)
{
	int done;

	done = qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_DONE_EN, true);
	if (!done) {
		writel(M_GENI_CMD_ABORT, uport->membase +
						SE_GENI_M_CMD_CTRL_REG);
		qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
							M_CMD_ABORT_EN, true);
		writel(M_CMD_ABORT_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
	}
}

static void qcom_geni_serial_abort_rx(struct uart_port *uport)
{
	u32 irq_clear = S_CMD_DONE_EN | S_CMD_ABORT_EN;

	writel(S_GENI_CMD_ABORT, uport->membase + SE_GENI_S_CMD_CTRL_REG);
	qcom_geni_serial_poll_bit(uport, SE_GENI_S_CMD_CTRL_REG,
					S_GENI_CMD_ABORT, false);
	writel(irq_clear, uport->membase + SE_GENI_S_IRQ_CLEAR);
	writel(FORCE_DEFAULT, uport->membase + GENI_FORCE_DEFAULT_REG);
}

#ifdef CONFIG_CONSOLE_POLL
static int qcom_geni_serial_get_char(struct uart_port *uport)
{
	struct qcom_geni_private_data *private_data = uport->private_data;
	u32 status;
	u32 word_cnt;
	int ret;

	if (!private_data->poll_cached_bytes_cnt) {
		status = readl(uport->membase + SE_GENI_M_IRQ_STATUS);
		writel(status, uport->membase + SE_GENI_M_IRQ_CLEAR);

		status = readl(uport->membase + SE_GENI_S_IRQ_STATUS);
		writel(status, uport->membase + SE_GENI_S_IRQ_CLEAR);

		status = readl(uport->membase + SE_GENI_RX_FIFO_STATUS);
		word_cnt = status & RX_FIFO_WC_MSK;
		if (!word_cnt)
			return NO_POLL_CHAR;

		if (word_cnt == 1 && (status & RX_LAST))
			/*
			 * NOTE: If RX_LAST_BYTE_VALID is 0 it needs to be
			 * treated as if it was BYTES_PER_FIFO_WORD.
			 */
			private_data->poll_cached_bytes_cnt =
				(status & RX_LAST_BYTE_VALID_MSK) >>
				RX_LAST_BYTE_VALID_SHFT;

		if (private_data->poll_cached_bytes_cnt == 0)
			private_data->poll_cached_bytes_cnt = BYTES_PER_FIFO_WORD;

		private_data->poll_cached_bytes =
			readl(uport->membase + SE_GENI_RX_FIFOn);
	}

	private_data->poll_cached_bytes_cnt--;
	ret = private_data->poll_cached_bytes & 0xff;
	private_data->poll_cached_bytes >>= 8;

	return ret;
}

static void qcom_geni_serial_poll_put_char(struct uart_port *uport,
							unsigned char c)
{
	writel(DEF_TX_WM, uport->membase + SE_GENI_TX_WATERMARK_REG);
	writel(M_CMD_DONE_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
	qcom_geni_serial_setup_tx(uport, 1);
	WARN_ON(!qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_TX_FIFO_WATERMARK_EN, true));
	writel(c, uport->membase + SE_GENI_TX_FIFOn);
	writel(M_TX_FIFO_WATERMARK_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
	qcom_geni_serial_poll_tx_done(uport);
}

static int qcom_geni_serial_poll_init(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	int ret;

	if (!port->setup) {
		ret = qcom_geni_serial_port_setup(uport);
		if (ret)
			return ret;
	}

	if (!qcom_geni_serial_secondary_active(uport))
		geni_se_setup_s_cmd(&port->se, UART_START_READ, 0);

	return 0;
}
#endif

static void qcom_geni_serial_enable_interrupts(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	u32 geni_m_irq_en, geni_s_irq_en;

	geni_m_irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
	geni_s_irq_en = readl(uport->membase + SE_GENI_S_IRQ_EN);

	/* GSI mode: disable FIFO watermark interrupts */
	if (port->gsi_mode) {
		geni_m_irq_en |= QCOM_M_IRQ_BITS;
		geni_s_irq_en |= QCOM_S_IRQ_BITS;
		geni_m_irq_en &= ~M_RX_FIFO_WATERMARK_EN;
		geni_s_irq_en &= ~S_RX_FIFO_WATERMARK_EN;
	}

	writel(geni_m_irq_en, uport->membase + SE_GENI_M_IRQ_EN);
	writel(geni_s_irq_en, uport->membase + SE_GENI_S_IRQ_EN);

	dev_info(uport->dev,
		 "enable_interrupts: M_IRQ_EN=0x%x S_IRQ_EN=0x%x gsi=%d\n",
		 geni_m_irq_en, geni_s_irq_en, port->gsi_mode);
}

#ifdef CONFIG_SERIAL_QCOM_GENI_CONSOLE
static void qcom_geni_serial_drain_fifo(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	qcom_geni_serial_poll_bitfield(uport, SE_GENI_M_GP_LENGTH, GP_LENGTH,
			port->tx_queued);
}

static void qcom_geni_serial_wr_char(struct uart_port *uport, unsigned char ch)
{
	struct qcom_geni_private_data *private_data = uport->private_data;

	private_data->write_cached_bytes =
		(private_data->write_cached_bytes >> 8) | (ch << 24);
	private_data->write_cached_bytes_cnt++;

	if (private_data->write_cached_bytes_cnt == BYTES_PER_FIFO_WORD) {
		writel(private_data->write_cached_bytes,
		       uport->membase + SE_GENI_TX_FIFOn);
		private_data->write_cached_bytes_cnt = 0;
	}
}

static void
__qcom_geni_serial_console_write(struct uart_port *uport, const char *s,
				 unsigned int count)
{
	struct qcom_geni_private_data *private_data = uport->private_data;

	int i;
	u32 bytes_to_send = count;

	for (i = 0; i < count; i++) {
		/*
		 * uart_console_write() adds a carriage return for each newline.
		 * Account for additional bytes to be written.
		 */
		if (s[i] == '\n')
			bytes_to_send++;
	}

	writel(DEF_TX_WM, uport->membase + SE_GENI_TX_WATERMARK_REG);
	writel(M_CMD_DONE_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
	qcom_geni_serial_setup_tx(uport, bytes_to_send);
	for (i = 0; i < count; ) {
		size_t chars_to_write = 0;
		size_t avail = DEF_FIFO_DEPTH_WORDS - DEF_TX_WM;

		/*
		 * If the WM bit never set, then the Tx state machine is not
		 * in a valid state, so break, cancel/abort any existing
		 * command. Unfortunately the current data being written is
		 * lost.
		 */
		if (!qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_TX_FIFO_WATERMARK_EN, true))
			break;
		chars_to_write = min_t(size_t, count - i, avail / 2);
		uart_console_write(uport, s + i, chars_to_write,
						qcom_geni_serial_wr_char);
		writel(M_TX_FIFO_WATERMARK_EN, uport->membase +
							SE_GENI_M_IRQ_CLEAR);
		i += chars_to_write;
	}

	if (private_data->write_cached_bytes_cnt) {
		private_data->write_cached_bytes >>= BITS_PER_BYTE *
			(BYTES_PER_FIFO_WORD - private_data->write_cached_bytes_cnt);
		writel(private_data->write_cached_bytes,
		       uport->membase + SE_GENI_TX_FIFOn);
		private_data->write_cached_bytes_cnt = 0;
	}

	qcom_geni_serial_poll_tx_done(uport);
}

static void qcom_geni_serial_console_write(struct console *co, const char *s,
			     unsigned int count)
{
	struct uart_port *uport;
	struct qcom_geni_serial_port *port;
	u32 m_irq_en, s_irq_en;
	bool locked = true;
	unsigned long flags;

	WARN_ON(co->index < 0 || co->index >= GENI_UART_CONS_PORTS);

	port = get_port_from_line(co->index, true);
	if (IS_ERR(port))
		return;

	uport = &port->uport;
	if (oops_in_progress)
		locked = uart_port_trylock_irqsave(uport, &flags);
	else
		uart_port_lock_irqsave(uport, &flags);

	m_irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
	s_irq_en = readl(uport->membase + SE_GENI_S_IRQ_EN);
	writel(0, uport->membase + SE_GENI_M_IRQ_EN);
	writel(0, uport->membase + SE_GENI_S_IRQ_EN);

	if (qcom_geni_serial_main_active(uport)) {
		/* Wait for completion or drain FIFO */
		if (!locked || port->tx_remaining == 0)
			qcom_geni_serial_poll_tx_done(uport);
		else
			qcom_geni_serial_drain_fifo(uport);

		qcom_geni_serial_cancel_tx_cmd(uport);
	}

	__qcom_geni_serial_console_write(uport, s, count);

	writel(m_irq_en, uport->membase + SE_GENI_M_IRQ_EN);
	writel(s_irq_en, uport->membase + SE_GENI_S_IRQ_EN);

	if (locked)
		uart_port_unlock_irqrestore(uport, flags);
}

static void handle_rx_console(struct uart_port *uport, u32 bytes, bool drop)
{
	u32 i;
	unsigned char buf[sizeof(u32)];
	struct tty_port *tport;
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	tport = &uport->state->port;
	for (i = 0; i < bytes; ) {
		int c;
		int chunk = min_t(int, bytes - i, BYTES_PER_FIFO_WORD);

		ioread32_rep(uport->membase + SE_GENI_RX_FIFOn, buf, 1);
		i += chunk;
		if (drop)
			continue;

		for (c = 0; c < chunk; c++) {
			int sysrq;

			uport->icount.rx++;
			if (port->brk && buf[c] == 0) {
				port->brk = false;
				if (uart_handle_break(uport))
					continue;
			}

			sysrq = uart_prepare_sysrq_char(uport, buf[c]);

			if (!sysrq)
				tty_insert_flip_char(tport, buf[c], TTY_NORMAL);
		}
	}
	if (!drop)
		tty_flip_buffer_push(tport);
}
#else
static void handle_rx_console(struct uart_port *uport, u32 bytes, bool drop)
{

}
#endif /* CONFIG_SERIAL_QCOM_GENI_CONSOLE */

static void handle_rx_uart(struct uart_port *uport, u32 bytes, bool drop)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct tty_port *tport = &uport->state->port;
	int ret;

	ret = tty_insert_flip_string(tport, port->rx_buf, bytes);
	if (ret != bytes) {
		dev_err(uport->dev, "%s:Unable to push data ret %d_bytes %d\n",
				__func__, ret, bytes);
		WARN_ON_ONCE(1);
	}
	uport->icount.rx += ret;
	tty_flip_buffer_push(tport);
}

static unsigned int qcom_geni_serial_tx_empty(struct uart_port *uport)
{
	return !readl(uport->membase + SE_GENI_TX_FIFO_STATUS);
}

static void qcom_geni_serial_stop_tx_dma(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	bool done;

	if (port->gsi_mode) {
		dev_dbg(uport->dev, "stop_tx_dma: queuing GSI TX cancel work\n");
		queue_work(port->tx_wq, &port->tx_cancel_work);
		return;
	}

	if (!qcom_geni_serial_main_active(uport))
		return;

	if (port->tx_dma_addr) {
		geni_se_tx_dma_unprep(&port->se, port->tx_dma_addr,
				      port->tx_remaining);
		port->tx_dma_addr = 0;
		port->tx_remaining = 0;
	}

	geni_se_cancel_m_cmd(&port->se);

	done = qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
					 M_CMD_CANCEL_EN, true);
	if (!done) {
		geni_se_abort_m_cmd(&port->se);
		done = qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						 M_CMD_ABORT_EN, true);
		if (!done)
			dev_err_ratelimited(uport->dev, "M_CMD_ABORT_EN not set");
		writel(M_CMD_ABORT_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
	}

	writel(M_CMD_CANCEL_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
}

static void qcom_geni_serial_start_tx_dma(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	unsigned int xmit_size;
	int ret;

	if (port->gsi_mode) {
		if (port->gsi->tx_ph) {
			dev_dbg(uport->dev,
				"start_tx_dma: GSI TX already in-flight (tx_ph=0x%llx)\n",
				(unsigned long long)port->gsi->tx_ph);
			return;
		}
		dev_dbg(uport->dev, "start_tx_dma: queuing GSI TX xfer work\n");
		queue_work(port->tx_wq, &port->tx_xfer_work);
		return;
	}

	if (port->tx_dma_addr)
		return;

	if (uart_circ_empty(xmit))
		return;

	xmit_size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);

	qcom_geni_serial_setup_tx(uport, xmit_size);

	ret = geni_se_tx_dma_prep(&port->se, &xmit->buf[xmit->tail],
				  xmit_size, &port->tx_dma_addr);
	if (ret) {
		dev_err(uport->dev, "unable to start TX SE DMA: %d\n", ret);
		qcom_geni_serial_stop_tx_dma(uport);
		return;
	}

	port->tx_remaining = xmit_size;
}

static void qcom_geni_serial_start_tx_fifo(struct uart_port *uport)
{
	unsigned char c;
	u32 irq_en;
	struct circ_buf *xmit = &uport->state->xmit;

	/*
	 * Start a new transfer in case the previous command was cancelled and
	 * left data in the FIFO which may prevent the watermark interrupt
	 * from triggering. Note that the stale data is discarded.
	 */
	if (!qcom_geni_serial_main_active(uport) &&
	    !qcom_geni_serial_tx_empty(uport)) {
		if (uart_circ_chars_pending(xmit)) {
			c = xmit->buf[xmit->tail];
			uart_xmit_advance(uport, 1);
			writel(M_CMD_DONE_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
			qcom_geni_serial_setup_tx(uport, 1);
			writel(c, uport->membase + SE_GENI_TX_FIFOn);
		}
	}

	irq_en = readl(uport->membase +	SE_GENI_M_IRQ_EN);
	irq_en |= M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN;
	writel(DEF_TX_WM, uport->membase + SE_GENI_TX_WATERMARK_REG);
	writel(irq_en, uport->membase +	SE_GENI_M_IRQ_EN);
}

static void qcom_geni_serial_stop_tx_fifo(struct uart_port *uport)
{
	u32 irq_en;

	irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
	irq_en &= ~(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN);
	writel(0, uport->membase + SE_GENI_TX_WATERMARK_REG);
	writel(irq_en, uport->membase + SE_GENI_M_IRQ_EN);
}

static void qcom_geni_serial_cancel_tx_cmd(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	if (!qcom_geni_serial_main_active(uport))
		return;

	geni_se_cancel_m_cmd(&port->se);
	if (!qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_CANCEL_EN, true)) {
		geni_se_abort_m_cmd(&port->se);
		qcom_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_ABORT_EN, true);
		writel(M_CMD_ABORT_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);
	}
	writel(M_CMD_CANCEL_EN, uport->membase + SE_GENI_M_IRQ_CLEAR);

	port->tx_remaining = 0;
	port->tx_queued = 0;
}

static void qcom_geni_serial_handle_rx_fifo(struct uart_port *uport, bool drop)
{
	u32 status;
	u32 word_cnt;
	u32 last_word_byte_cnt;
	u32 last_word_partial;
	u32 total_bytes;

	status = readl(uport->membase +	SE_GENI_RX_FIFO_STATUS);
	word_cnt = status & RX_FIFO_WC_MSK;
	last_word_partial = status & RX_LAST;
	last_word_byte_cnt = (status & RX_LAST_BYTE_VALID_MSK) >>
						RX_LAST_BYTE_VALID_SHFT;

	if (!word_cnt)
		return;
	total_bytes = BYTES_PER_FIFO_WORD * (word_cnt - 1);
	if (last_word_partial && last_word_byte_cnt)
		total_bytes += last_word_byte_cnt;
	else
		total_bytes += BYTES_PER_FIFO_WORD;
	handle_rx_console(uport, total_bytes, drop);
}

static void qcom_geni_serial_stop_rx_fifo(struct uart_port *uport)
{
	u32 irq_en;
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	u32 s_irq_status;

	irq_en = readl(uport->membase + SE_GENI_S_IRQ_EN);
	irq_en &= ~(S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN);
	writel(irq_en, uport->membase + SE_GENI_S_IRQ_EN);

	irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
	irq_en &= ~(M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);
	writel(irq_en, uport->membase + SE_GENI_M_IRQ_EN);

	if (!qcom_geni_serial_secondary_active(uport))
		return;

	geni_se_cancel_s_cmd(&port->se);
	qcom_geni_serial_poll_bit(uport, SE_GENI_S_IRQ_STATUS,
					S_CMD_CANCEL_EN, true);
	/*
	 * If timeout occurs secondary engine remains active
	 * and Abort sequence is executed.
	 */
	s_irq_status = readl(uport->membase + SE_GENI_S_IRQ_STATUS);
	/* Flush the Rx buffer */
	if (s_irq_status & S_RX_FIFO_LAST_EN)
		qcom_geni_serial_handle_rx_fifo(uport, true);
	writel(s_irq_status, uport->membase + SE_GENI_S_IRQ_CLEAR);

	if (qcom_geni_serial_secondary_active(uport))
		qcom_geni_serial_abort_rx(uport);
}

static void qcom_geni_serial_start_rx_fifo(struct uart_port *uport)
{
	u32 irq_en;
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	if (qcom_geni_serial_secondary_active(uport))
		qcom_geni_serial_stop_rx_fifo(uport);

	geni_se_setup_s_cmd(&port->se, UART_START_READ, 0);

	irq_en = readl(uport->membase + SE_GENI_S_IRQ_EN);
	irq_en |= S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN;
	writel(irq_en, uport->membase + SE_GENI_S_IRQ_EN);

	irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
	irq_en |= M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN;
	writel(irq_en, uport->membase + SE_GENI_M_IRQ_EN);
}

static void qcom_geni_serial_stop_rx_dma(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	bool done;

	if (port->gsi_mode) {
		if (!port->port_setup) {
			dev_dbg(uport->dev, "stop_rx_dma: port not setup, skip\n");
			return;
		}
		if (atomic_read(&port->stop_rx_inprogress)) {
			dev_dbg(uport->dev, "stop_rx_dma: already in progress, skip\n");
			return;
		}
		if (!qcom_geni_serial_secondary_active(uport)) {
			dev_dbg(uport->dev,
				"stop_rx_dma: SE RX not active, complete immediately\n");
			complete(&port->xfer);
			return;
		}
		atomic_set(&port->stop_rx_inprogress, 1);
		dev_dbg(uport->dev, "stop_rx_dma: queuing GSI RX cancel work\n");
		reinit_completion(&port->xfer);
		queue_work(port->rx_wq, &port->rx_cancel_work);
		return;
	}

	if (!qcom_geni_serial_secondary_active(uport))
		return;

	geni_se_cancel_s_cmd(&port->se);
	done = qcom_geni_serial_poll_bit(uport, SE_DMA_RX_IRQ_STAT,
			RX_EOT, true);
	if (done) {
		writel(RX_EOT | RX_DMA_DONE,
				uport->membase + SE_DMA_RX_IRQ_CLR);
	} else {
		qcom_geni_serial_abort_rx(uport);

		writel(1, uport->membase + SE_DMA_RX_FSM_RST);
		qcom_geni_serial_poll_bit(uport, SE_DMA_RX_IRQ_STAT,
				RX_RESET_DONE, true);
		writel(RX_RESET_DONE | RX_DMA_DONE,
				uport->membase + SE_DMA_RX_IRQ_CLR);
	}

	if (port->rx_dma_addr) {
		geni_se_rx_dma_unprep(&port->se, port->rx_dma_addr,
				      DMA_RX_BUF_SIZE);
		port->rx_dma_addr = 0;
	}
}

static void qcom_geni_serial_start_rx_dma(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	int ret;
	u32 geni_status;

	/* GSI mode: RX sequencer logic */
	if (port->gsi_mode) {
		if (!qcom_geni_serial_secondary_active(uport))
			geni_se_setup_s_cmd(&port->se, UART_START_READ,
					    UART_PARAM_RFR_OPEN);
		geni_status = readl(uport->membase + SE_GENI_STATUS);

		if (geni_status & S_GENI_CMD_ACTIVE) {
			dev_dbg(uport->dev,
				"start_rx_dma: Sec SE still active (geni_status=0x%x), aborting\n",
				geni_status);
			qcom_geni_serial_abort_rx(uport);
		}

		dev_dbg(uport->dev, "start_rx_dma: starting GSI RX DMA xfer\n");
		/* Start GSI RX transfer */
		ret = qcom_geni_uart_gsi_xfer_rx(uport);
		if (ret)
			dev_err(uport->dev, "GSI RX xfer failed: %d\n", ret);
		return;
	}

	if (qcom_geni_serial_secondary_active(uport))
		qcom_geni_serial_stop_rx_dma(uport);

	geni_se_setup_s_cmd(&port->se, UART_START_READ, UART_PARAM_RFR_OPEN);

	ret = geni_se_rx_dma_prep(&port->se, port->rx_buf,
				  DMA_RX_BUF_SIZE,
				  &port->rx_dma_addr);
	if (ret) {
		dev_err(uport->dev, "unable to start RX SE DMA: %d\n", ret);
		qcom_geni_serial_stop_rx_dma(uport);
	}
}

static void qcom_geni_serial_handle_rx_dma(struct uart_port *uport, bool drop)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	u32 rx_in;
	int ret;

	if (!qcom_geni_serial_secondary_active(uport))
		return;

	if (!port->rx_dma_addr)
		return;

	geni_se_rx_dma_unprep(&port->se, port->rx_dma_addr, DMA_RX_BUF_SIZE);
	port->rx_dma_addr = 0;

	rx_in = readl(uport->membase + SE_DMA_RX_LEN_IN);
	if (!rx_in) {
		dev_warn(uport->dev, "serial engine reports 0 RX bytes in!\n");
		return;
	}

	if (!drop)
		handle_rx_uart(uport, rx_in, drop);

	ret = geni_se_rx_dma_prep(&port->se, port->rx_buf,
				  DMA_RX_BUF_SIZE,
				  &port->rx_dma_addr);
	if (ret) {
		dev_err(uport->dev, "unable to start RX SE DMA: %d\n", ret);
		qcom_geni_serial_stop_rx_dma(uport);
	}
}

static void qcom_geni_serial_start_rx(struct uart_port *uport)
{
	uport->ops->start_rx(uport);
}

static void qcom_geni_serial_stop_rx(struct uart_port *uport)
{
	uport->ops->stop_rx(uport);
}

/**
 * setup_config0_tre() - Configure GSI TRE for UART parameters
 * From qcom_geni_serial.c lines 1877-1905
 */
static void setup_config0_tre(struct uart_port *uport, unsigned int bits_per_char,
			      unsigned int clk_div, unsigned int stop_bit_len,
			      unsigned int tx_parity, bool cts_mask,
			      unsigned int rx_parity, unsigned int loopback)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct qcom_gpi_tre *tx_cfg0 = &port->gsi->tx_cfg0_t;
	struct qcom_gpi_tre *rx_cfg0 = &port->gsi->rx_cfg0_t;
	unsigned int char_size = bits_per_char - 5;
	unsigned int flags = (cts_mask << 2) | (loopback & 0x1);
	unsigned int rfr_lvl = port->rx_fifo_depth - 2;

	/* TX config0: Parity-4 for none, packing-101 */
	tx_cfg0->dword[0] = QCOM_GPI_UART_CONFIG0_TRE_DWORD0(1, 0, flags, 4,
							     stop_bit_len, char_size);
	tx_cfg0->dword[1] = QCOM_GPI_UART_CONFIG0_TRE_DWORD1(0, 0);
	tx_cfg0->dword[2] = QCOM_GPI_UART_CONFIG0_TRE_DWORD2(0, clk_div);
	tx_cfg0->dword[3] = QCOM_GPI_UART_CONFIG0_TRE_DWORD3(0, 0, 0, 0, 1);

	/* RX config0 */
	rx_cfg0->dword[0] = QCOM_GPI_UART_CONFIG0_TRE_DWORD0(1, 0, flags, 4,
							     stop_bit_len, char_size);
	rx_cfg0->dword[1] = QCOM_GPI_UART_CONFIG0_TRE_DWORD1(rfr_lvl, STALE_COUNT);
	rx_cfg0->dword[2] = QCOM_GPI_UART_CONFIG0_TRE_DWORD2(0, clk_div);
	rx_cfg0->dword[3] = QCOM_GPI_UART_CONFIG0_TRE_DWORD3(0, 0, 0, 0, 1);

	/* Set callback userdata */
	port->gsi->tx_cb.userdata = port;
	port->gsi->rx_cb.userdata = port;

	dev_dbg(uport->dev,
		"config0_tre: bpc=%u clk_div=%u sbl=%u flags=0x%x rfr_lvl=%u stale=%u\n",
		bits_per_char, clk_div, stop_bit_len, flags, rfr_lvl, STALE_COUNT);
	dev_dbg(uport->dev,
		"config0_tre: TX dw[0]=0x%08x dw[1]=0x%08x dw[2]=0x%08x dw[3]=0x%08x\n",
		tx_cfg0->dword[0], tx_cfg0->dword[1],
		tx_cfg0->dword[2], tx_cfg0->dword[3]);
	dev_dbg(uport->dev,
		"config0_tre: RX dw[0]=0x%08x dw[1]=0x%08x dw[2]=0x%08x dw[3]=0x%08x\n",
		rx_cfg0->dword[0], rx_cfg0->dword[1],
		rx_cfg0->dword[2], rx_cfg0->dword[3]);
}

/**
 * qcom_geni_serial_alloc_gsi_rx_bufs() - Allocate RX buffers for GSI mode
 */
static int qcom_geni_serial_alloc_gsi_rx_bufs(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct device *gpi_dev = port->gsi->rx_c->device->dev;
	int i;

	for (i = 0; i < NUM_RX_BUF; i++) {
		port->rx_gsi_buf[i] = dma_alloc_coherent(gpi_dev,
							 DMA_RX_BUF_SIZE,
							 &port->rx_gsi_dma_addr[i],
							 GFP_KERNEL);
		if (!port->rx_gsi_buf[i])
			goto free_bufs;
		dev_dbg(uport->dev,
			"alloc_rx_bufs: buf[%d] virt=%p dma=0x%llx\n",
			i, port->rx_gsi_buf[i],
			(u64)port->rx_gsi_dma_addr[i]);
	}
	return 0;

free_bufs:
	for (i = 0; i < NUM_RX_BUF; i++) {
		if (port->rx_gsi_buf[i]) {
			dma_free_coherent(gpi_dev, DMA_RX_BUF_SIZE,
					  port->rx_gsi_buf[i],
					  port->rx_gsi_dma_addr[i]);
			port->rx_gsi_buf[i] = NULL;
			port->rx_gsi_dma_addr[i] = 0;
		}
	}
	return -ENOMEM;
}

/**
 * qcom_geni_uart_gsi_tx_cb() - TX DMA completion callback
 */
static void qcom_geni_uart_gsi_tx_cb(void *ptr)
{
	struct qcom_gpi_dma_async_tx_cb_param *tx_cb = ptr;
	struct qcom_geni_serial_port *port = tx_cb->userdata;
	struct uart_port *uport = &port->uport;
	struct circ_buf *xmit = &uport->state->xmit;

	xmit->tail = (xmit->tail + port->xmit_size) & (UART_XMIT_SIZE - 1);
	dma_unmap_single(port->gsi->tx_c->device->dev,
			 port->gsi->tx_ph, port->xmit_size, DMA_TO_DEVICE);
	uport->icount.tx += port->xmit_size;
	dev_info(uport->dev, "gsi_tx_cb: TX complete %u bytes sent\n",
		 port->xmit_size);
	port->gsi->tx_ph = (dma_addr_t)NULL;
	port->xmit_size = 0;
	complete(&port->tx_xfer);

	if (!uart_circ_empty(xmit))
		queue_work(port->tx_wq, &port->tx_xfer_work);
	else
		uart_write_wakeup(uport);
}

static void qcom_geni_uart_rx_queue_dma_tre(int index, struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct dma_async_tx_descriptor *desc;
	struct scatterlist rx_sg;
	dma_cookie_t rx_cookie;

	/* Initialize single-element scatter-gather list */
	sg_init_table(&rx_sg, 1);
	sg_set_buf(&rx_sg, &port->gsi->rx_t[index],
		   sizeof(port->gsi->rx_t[index]));

	/* Prepare RX descriptor with single DATA TRE */
	desc = dmaengine_prep_slave_sg(port->gsi->rx_c,
				       &rx_sg, 1, DMA_DEV_TO_MEM,
				       (DMA_PREP_INTERRUPT | DMA_CTRL_ACK));
	if (!desc) {
		dev_err(uport->dev, "%s: Prep_slave_sg failed\n", __func__);
		return;
	}

	/* Set callback for completion */
	desc->callback = qcom_geni_uart_gsi_rx_cb;
	desc->callback_param = &port->gsi->rx_cb;

	/* Submit descriptor */
	rx_cookie = dmaengine_submit(desc);
	if (dma_submit_error(rx_cookie)) {
		dev_err(uport->dev, "%s: dmaengine_submit failed (%d)\n",
			__func__, rx_cookie);
		dmaengine_terminate_all(port->gsi->rx_c);
		return;
	}

	/* Issue pending DMA */
	dma_async_issue_pending(port->gsi->rx_c);
}

/**
 * qcom_geni_uart_gsi_rx_cb() - RX DMA completion callback
 */
static void qcom_geni_uart_gsi_rx_cb(void *ptr)
{
	struct qcom_gpi_dma_async_tx_cb_param *rx_cb = ptr;
	struct qcom_geni_serial_port *port = rx_cb->userdata;
	struct uart_port *uport = &port->uport;
	struct tty_port *tport = &uport->state->port;
	unsigned int rx_bytes = rx_cb->length;
	int ret;

	dev_info(uport->dev,
		 "gsi_rx_cb: RX complete: received %u bytes\n", rx_bytes);

	if (rx_bytes) {
		ret = tty_insert_flip_string(tport,
					     (unsigned char *)(port->rx_gsi_buf[port->rx_buf_idx]),
					     rx_bytes);
		if (ret != rx_bytes) {
			dev_warn(uport->dev, "RX: inserted %d of %d bytes\n",
				 ret, rx_bytes);
		}

		/* Update RX counter */
		uport->icount.rx += ret;
	}

	/* Push data to TTY layer */
	tty_flip_buffer_push(tport);
	dev_dbg(uport->dev,
		"gsi_rx_cb: pushed %u bytes to TTY (total rx=%u)\n",
		rx_bytes, uport->icount.rx);

	if (!port->loopback)
		/* Queue next RX transfer with current buffer */
		qcom_geni_uart_rx_queue_dma_tre(port->rx_buf_idx, uport);
	else
		dev_dbg(uport->dev,
			"gsi_rx_cb: loopback mode, skip queuing next RX TRE\n");

	/* Update buffer index for next transfer (circular buffer) */
	port->rx_buf_idx = (port->rx_buf_idx + 1) % NUM_RX_BUF;
}

/**
 * qcom_geni_uart_gsi_xfer_tx() - TX transfer work function
 */
static void qcom_geni_uart_gsi_xfer_tx(struct work_struct *work)
{
	struct qcom_geni_serial_port *port = container_of(work,
			struct qcom_geni_serial_port,
			tx_xfer_work);
	struct uart_port *uport = &port->uport;
	struct circ_buf *xmit = &uport->state->xmit;
	struct qcom_gpi_tre *go_t = &port->gsi->tx_go_t;
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	int ret, xmit_size, index = 0;

	if (uart_circ_empty(xmit) || uart_tx_stopped(uport)) {
		dev_dbg(uport->dev, "gsi_xfer_tx: circ empty or TX stopped, skip\n");
		complete(&port->tx_xfer);
		return;
	}

	xmit_size = uart_circ_chars_pending(xmit);
	if (xmit_size > (UART_XMIT_SIZE - xmit->tail))
		xmit_size = UART_XMIT_SIZE - xmit->tail;

	dev_dbg(uport->dev, "gsi_xfer_tx: submitting %d bytes (tail=%d)\n",
		xmit_size, xmit->tail);

	sg_init_table(port->gsi->tx_sg, 3);
	sg_set_buf(&port->gsi->tx_sg[index++], &port->gsi->tx_cfg0_t,
		   sizeof(port->gsi->tx_cfg0_t));

	go_t->dword[0] = QCOM_GPI_UART_GO_TRE_DWORD0(0, 1);
	go_t->dword[1] = QCOM_GPI_UART_GO_TRE_DWORD1;
	go_t->dword[2] = QCOM_GPI_UART_GO_TRE_DWORD2;
	go_t->dword[3] = QCOM_GPI_UART_GO_TRE_DWORD3(0, 0, 0, 0, 1);
	sg_set_buf(&port->gsi->tx_sg[index++], go_t, sizeof(*go_t));

	port->xmit_size = xmit_size;
	port->gsi->tx_ph = dma_map_single(port->gsi->tx_c->device->dev,
					  &xmit->buf[xmit->tail],
					  xmit_size, DMA_TO_DEVICE);
	if (dma_mapping_error(port->gsi->tx_c->device->dev, port->gsi->tx_ph)) {
		dev_err(uport->dev, "TX DMA map error\n");
		port->gsi->tx_ph = (dma_addr_t)NULL;
		complete(&port->tx_xfer);
		return;
	}

	port->gsi->tx_t.dword[0] = QCOM_GPI_DMA_W_BUFFER_TRE_DWORD0(port->gsi->tx_ph);
	port->gsi->tx_t.dword[1] = QCOM_GPI_DMA_W_BUFFER_TRE_DWORD1(port->gsi->tx_ph);
	port->gsi->tx_t.dword[2] = QCOM_GPI_DMA_W_BUFFER_TRE_DWORD2(xmit_size);
	port->gsi->tx_t.dword[3] = QCOM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, 1, 0, 0);

	sg_set_buf(&port->gsi->tx_sg[index++], &port->gsi->tx_t,
		   sizeof(port->gsi->tx_t));

	desc = dmaengine_prep_slave_sg(port->gsi->tx_c,
				       port->gsi->tx_sg, 3,
				       DMA_MEM_TO_DEV,
				       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(uport->dev, "TX prep failed\n");
		goto unmap_tx;
	}

	desc->callback = qcom_geni_uart_gsi_tx_cb;
	desc->callback_param = &port->gsi->tx_cb;

	cookie = dmaengine_submit(desc);
	ret = dma_submit_error(cookie);
	if (ret) {
		dev_err(uport->dev, "TX submit failed: %d\n", ret);
		goto unmap_tx;
	}

	dma_async_issue_pending(port->gsi->tx_c);
	dev_dbg(uport->dev,
		"gsi_xfer_tx: TX issued %d bytes (tail=%d ph=0x%llx)\n"
		"  CONFIG0 dw[0]=0x%08x dw[1]=0x%08x dw[2]=0x%08x dw[3]=0x%08x\n"
		"  GO      dw[0]=0x%08x dw[1]=0x%08x dw[2]=0x%08x dw[3]=0x%08x\n"
		"  DATA    dw[0]=0x%08x dw[1]=0x%08x dw[2]=0x%08x dw[3]=0x%08x\n"
		"  data[0..15]= %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		xmit_size, xmit->tail, (u64)port->gsi->tx_ph,
		port->gsi->tx_cfg0_t.dword[0], port->gsi->tx_cfg0_t.dword[1],
		port->gsi->tx_cfg0_t.dword[2], port->gsi->tx_cfg0_t.dword[3],
		go_t->dword[0], go_t->dword[1], go_t->dword[2], go_t->dword[3],
		port->gsi->tx_t.dword[0], port->gsi->tx_t.dword[1],
		port->gsi->tx_t.dword[2], port->gsi->tx_t.dword[3],
		((u8 *)&xmit->buf[xmit->tail])[0],
		((u8 *)&xmit->buf[xmit->tail])[1],
		((u8 *)&xmit->buf[xmit->tail])[2],
		((u8 *)&xmit->buf[xmit->tail])[3],
		((u8 *)&xmit->buf[xmit->tail])[4],
		((u8 *)&xmit->buf[xmit->tail])[5],
		((u8 *)&xmit->buf[xmit->tail])[6],
		((u8 *)&xmit->buf[xmit->tail])[7],
		((u8 *)&xmit->buf[xmit->tail])[8],
		((u8 *)&xmit->buf[xmit->tail])[9],
		((u8 *)&xmit->buf[xmit->tail])[10],
		((u8 *)&xmit->buf[xmit->tail])[11],
		((u8 *)&xmit->buf[xmit->tail])[12],
		((u8 *)&xmit->buf[xmit->tail])[13],
		((u8 *)&xmit->buf[xmit->tail])[14],
		((u8 *)&xmit->buf[xmit->tail])[15]);

	reinit_completion(&port->tx_xfer);
	if (!wait_for_completion_timeout(&port->tx_xfer,
					 msecs_to_jiffies(POLL_WAIT_TIMEOUT_MSEC)))
		dev_warn(uport->dev, "gsi_xfer_tx: TX completion timeout\n");
	return;

unmap_tx:
	geni_se_tx_dma_unprep(&port->se, port->gsi->tx_ph, xmit_size);
	port->gsi->tx_ph = (dma_addr_t)NULL;
	complete(&port->tx_xfer);
}

static void qcom_geni_uart_gsi_cancel_tx(struct work_struct *work)
{
	struct qcom_geni_serial_port *port = container_of(work,
			struct qcom_geni_serial_port,
			tx_cancel_work);
	struct uart_port *uport = &port->uport;

	if (port->gsi->tx_ph) {
		dev_dbg(uport->dev,
			"gsi_cancel_tx: unmapping in-flight TX buffer (ph=0x%llx size=%u)\n",
			(unsigned long long)port->gsi->tx_ph, port->xmit_size);
		if (port->gsi->tx_c)
			dma_unmap_single(port->gsi->tx_c->device->dev, port->gsi->tx_ph,
					 port->xmit_size, DMA_TO_DEVICE);
		port->gsi->tx_ph = (dma_addr_t)NULL;
		port->xmit_size = 0;
	}
	dev_dbg(uport->dev, "gsi_cancel_tx: terminating TX DMA\n");
	if (port->gsi->tx_c)
		dmaengine_terminate_all(port->gsi->tx_c);
	complete(&port->tx_xfer);
}

static void qcom_geni_uart_gsi_cancel_rx(struct work_struct *work)
{
	struct qcom_geni_serial_port *port = container_of(work,
			struct qcom_geni_serial_port,
			rx_cancel_work);
	dev_dbg(port->uport.dev, "gsi_cancel_rx: terminating RX DMA\n");
	dmaengine_terminate_all(port->gsi->rx_c);
	complete(&port->rx_cancel);
	complete(&port->xfer);
	atomic_set(&port->stop_rx_inprogress, 0);
	dev_dbg(port->uport.dev, "gsi_cancel_rx: done\n");
}

static int qcom_geni_uart_gsi_xfer_rx(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct qcom_gpi_tre *go_t = &port->gsi->rx_go_t;
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	int i, index = 0, ret = 0;

	if (!port->port_setup) {
		dev_err(uport->dev, "%s: Port setup not yet done\n", __func__);
		return -EINVAL;
	}

	if (!port->gsi->rx_c || !port->gsi->tx_c) {
		dev_err(uport->dev, "%s: GSI channels not allocated\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < NUM_RX_BUF; i++) {
		if (!port->rx_gsi_buf[i] || !port->rx_gsi_dma_addr[i]) {
			dev_err(uport->dev, "%s: RX buffer %d not initialized\n",
				__func__, i);
			return -EINVAL;
		}
	}

	port->rx_buf_idx = 0;
	/* Build scatterlist: CONFIG0 TRE + GO TRE + NUM_RX_BUF DATA TREs */
	sg_init_table(port->gsi->rx_sg, 6);

	sg_set_buf(&port->gsi->rx_sg[index++], &port->gsi->rx_cfg0_t,
		   sizeof(port->gsi->rx_cfg0_t));

	go_t->dword[0] = QCOM_GPI_UART_GO_TRE_DWORD0(0, 1);
	go_t->dword[1] = QCOM_GPI_UART_GO_TRE_DWORD1;
	go_t->dword[2] = QCOM_GPI_UART_GO_TRE_DWORD2;
	go_t->dword[3] = QCOM_GPI_UART_GO_TRE_DWORD3(0, 0, 0, 0, 1);
	sg_set_buf(&port->gsi->rx_sg[index++], go_t, sizeof(port->gsi->rx_go_t));

	for (i = 0; i < NUM_RX_BUF; i++) {
		port->gsi->rx_t[i].dword[0] =
			QCOM_GPI_DMA_W_BUFFER_TRE_DWORD0(port->rx_gsi_dma_addr[i]);
		port->gsi->rx_t[i].dword[1] =
			QCOM_GPI_DMA_W_BUFFER_TRE_DWORD1(port->rx_gsi_dma_addr[i]);
		port->gsi->rx_t[i].dword[2] =
			QCOM_GPI_DMA_W_BUFFER_TRE_DWORD2(DMA_RX_BUF_SIZE);
		/*
		 * Intermediate TREs (not last): ch=1 chains to the next TRE.
		 * Last TRE: ch=0 terminates the chain so the GPI ring processor
		 * does not read past the end of the submitted descriptor.
		 * Only the last TRE asserts IEOT+IEOB to signal completion.
		 */
		if (i < NUM_RX_BUF - 1)
			port->gsi->rx_t[i].dword[3] =
				QCOM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, 0, 0, 1);
		else
			port->gsi->rx_t[i].dword[3] =
				QCOM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, 1, 1, 0);
		sg_set_buf(&port->gsi->rx_sg[index++], &port->gsi->rx_t[i],
			   sizeof(port->gsi->rx_t[i]));
		dev_dbg(uport->dev,
			"gsi_xfer_rx: DATA TRE[%d] dma=0x%llx dw[0]=0x%08x dw[3]=0x%08x\n",
			i, (u64)port->rx_gsi_dma_addr[i],
			port->gsi->rx_t[i].dword[0],
			port->gsi->rx_t[i].dword[3]);
	}

	dev_info(uport->dev,
		 "gsi_xfer_rx: submitting 6 TREs (cfg0+go+4xdata) to RX channel\n");
	desc = dmaengine_prep_slave_sg(port->gsi->rx_c, port->gsi->rx_sg, 6,
				       DMA_DEV_TO_MEM,
				       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(uport->dev, "%s: RX descriptor preparation failed\n", __func__);
		ret = -EIO;
		goto exit_gsi_xfer_rx;
	}

	desc->callback = qcom_geni_uart_gsi_rx_cb;
	desc->callback_param = &port->gsi->rx_cb;
	port->gsi->rx_desc = desc;

	cookie = dmaengine_submit(desc);
	if (dma_submit_error(cookie)) {
		dev_err(uport->dev, "%s: DMA submit failed (%d)\n", __func__, cookie);
		ret = -EINVAL;
		goto exit_gsi_xfer_rx;
	}

	dma_async_issue_pending(port->gsi->rx_c);
	dev_dbg(uport->dev, "gsi_xfer_rx: RX transfer started (6 TREs: cfg0+go+4xdata)\n");
	return 0;

exit_gsi_xfer_rx:
	dmaengine_terminate_sync(port->gsi->rx_c);
	return ret;
}

static void qcom_geni_serial_init_gsi(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	dev_dbg(uport->dev, "init_gsi: mode=%d (GPI_DMA=%d)\n",
		port->dev_data->mode, GENI_GPI_DMA);

	if (port->dev_data->mode != GENI_GPI_DMA) {
		dev_dbg(uport->dev, "init_gsi: not GPI_DMA mode, skipping GSI init\n");
		return;
	}

	port->gsi_mode = true;
	uport->ops = &qcom_geni_uart_pops;
	dev_dbg(uport->dev, "init_gsi: gsi_mode set, ops switched to uart_pops\n");
	port->gsi = devm_kzalloc(uport->dev, sizeof(*port->gsi), GFP_KERNEL);
	if (!port->gsi) {
		dev_err(uport->dev, "Failed to allocate GSI structure\n");
		port->gsi_mode = false;
		return;
	}

	port->tx_wq = alloc_workqueue("%s_tx", WQ_UNBOUND | WQ_HIGHPRI, 1,
				      dev_name(uport->dev));
	port->rx_wq = alloc_workqueue("%s_rx", WQ_UNBOUND | WQ_HIGHPRI, 1,
				      dev_name(uport->dev));
	if (!port->tx_wq || !port->rx_wq) {
		dev_err(uport->dev, "Failed to create workqueues\n");
		goto cleanup_wq;
	}

	INIT_WORK(&port->tx_xfer_work, qcom_geni_uart_gsi_xfer_tx);
	INIT_WORK(&port->rx_cancel_work, qcom_geni_uart_gsi_cancel_rx);
	INIT_WORK(&port->tx_cancel_work, qcom_geni_uart_gsi_cancel_tx);

	init_completion(&port->tx_xfer);
	init_completion(&port->rx_cancel);
	init_completion(&port->xfer);
	complete(&port->xfer);

	dev_info(uport->dev, "GSI mode initialized successfully\n");
	return;

cleanup_wq:
	if (port->tx_wq)
		destroy_workqueue(port->tx_wq);
	if (port->rx_wq)
		destroy_workqueue(port->rx_wq);
	devm_kfree(uport->dev, port->gsi);
	port->gsi = NULL;
	port->gsi_mode = false;
}

static void qcom_geni_serial_stop_tx(struct uart_port *uport)
{
	uport->ops->stop_tx(uport);
}

static void qcom_geni_serial_send_chunk_fifo(struct uart_port *uport,
					     unsigned int chunk)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	unsigned int tx_bytes, c, remaining = chunk;
	u8 buf[BYTES_PER_FIFO_WORD];

	while (remaining) {
		memset(buf, 0, sizeof(buf));
		tx_bytes = min(remaining, BYTES_PER_FIFO_WORD);

		for (c = 0; c < tx_bytes ; c++) {
			buf[c] = xmit->buf[xmit->tail];
			uart_xmit_advance(uport, 1);
		}

		iowrite32_rep(uport->membase + SE_GENI_TX_FIFOn, buf, 1);

		remaining -= tx_bytes;
		port->tx_remaining -= tx_bytes;
	}
}

static void qcom_geni_serial_handle_tx_fifo(struct uart_port *uport,
					    bool done, bool active)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	size_t avail;
	size_t pending;
	u32 status;
	u32 irq_en;
	unsigned int chunk;

	status = readl(uport->membase + SE_GENI_TX_FIFO_STATUS);

	/* Complete the current tx command before taking newly added data */
	if (active)
		pending = port->tx_remaining;
	else
		pending = uart_circ_chars_pending(xmit);

	/* All data has been transmitted or command has been cancelled */
	if (!pending && done) {
		qcom_geni_serial_stop_tx_fifo(uport);
		goto out_write_wakeup;
	}

	if (active)
		avail = port->tx_fifo_depth - (status & TX_FIFO_WC);
	else
		avail = port->tx_fifo_depth;

	avail *= BYTES_PER_FIFO_WORD;

	chunk = min(avail, pending);
	if (!chunk)
		goto out_write_wakeup;

	if (!port->tx_remaining) {
		qcom_geni_serial_setup_tx(uport, pending);
		port->tx_remaining = pending;
		port->tx_queued = 0;

		irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
		if (!(irq_en & M_TX_FIFO_WATERMARK_EN))
			writel(irq_en | M_TX_FIFO_WATERMARK_EN,
					uport->membase + SE_GENI_M_IRQ_EN);
	}

	qcom_geni_serial_send_chunk_fifo(uport, chunk);
	port->tx_queued += chunk;

	/*
	 * The tx fifo watermark is level triggered and latched. Though we had
	 * cleared it in qcom_geni_serial_isr it will have already reasserted
	 * so we must clear it again here after our writes.
	 */
	writel(M_TX_FIFO_WATERMARK_EN,
			uport->membase + SE_GENI_M_IRQ_CLEAR);

out_write_wakeup:
	if (!port->tx_remaining) {
		irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
		if (irq_en & M_TX_FIFO_WATERMARK_EN)
			writel(irq_en & ~M_TX_FIFO_WATERMARK_EN,
					uport->membase + SE_GENI_M_IRQ_EN);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(uport);
}

static void qcom_geni_serial_handle_tx_dma(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	struct circ_buf *xmit = &uport->state->xmit;

	uart_xmit_advance(uport, port->tx_remaining);
	geni_se_tx_dma_unprep(&port->se, port->tx_dma_addr, port->tx_remaining);
	port->tx_dma_addr = 0;
	port->tx_remaining = 0;

	if (!uart_circ_empty(xmit))
		qcom_geni_serial_start_tx_dma(uport);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(uport);
}

static irqreturn_t qcom_geni_serial_isr(int isr, void *dev)
{
	u32 m_irq_en;
	u32 m_irq_status;
	u32 s_irq_status;
	u32 geni_status;
	u32 dma;
	u32 dma_tx_status;
	u32 dma_rx_status;
	struct uart_port *uport = dev;
	bool drop_rx = false;
	struct tty_port *tport = &uport->state->port;
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	if (uport->suspended)
		return IRQ_NONE;

	spin_lock(&uport->lock);

	m_irq_status = readl(uport->membase + SE_GENI_M_IRQ_STATUS);
	s_irq_status = readl(uport->membase + SE_GENI_S_IRQ_STATUS);
	dma_tx_status = readl(uport->membase + SE_DMA_TX_IRQ_STAT);
	dma_rx_status = readl(uport->membase + SE_DMA_RX_IRQ_STAT);
	geni_status = readl(uport->membase + SE_GENI_STATUS);
	dma = readl(uport->membase + SE_GENI_DMA_MODE_EN);
	m_irq_en = readl(uport->membase + SE_GENI_M_IRQ_EN);
	writel(m_irq_status, uport->membase + SE_GENI_M_IRQ_CLEAR);
	writel(s_irq_status, uport->membase + SE_GENI_S_IRQ_CLEAR);
	writel(dma_tx_status, uport->membase + SE_DMA_TX_IRQ_CLR);
	writel(dma_rx_status, uport->membase + SE_DMA_RX_IRQ_CLR);

	if (WARN_ON(m_irq_status & M_ILLEGAL_CMD_EN))
		goto out_unlock;

	if (s_irq_status & S_RX_FIFO_WR_ERR_EN) {
		uport->icount.overrun++;
		tty_insert_flip_char(tport, 0, TTY_OVERRUN);
	}

	if (s_irq_status & (S_GP_IRQ_0_EN | S_GP_IRQ_1_EN)) {
		if (s_irq_status & S_GP_IRQ_0_EN)
			uport->icount.parity++;
		drop_rx = true;
	} else if (s_irq_status & (S_GP_IRQ_2_EN | S_GP_IRQ_3_EN)) {
		uport->icount.brk++;
		port->brk = true;
	}

	if (dma) {
		if (dma_tx_status & TX_DMA_DONE)
			qcom_geni_serial_handle_tx_dma(uport);

		if (dma_rx_status) {
			if (dma_rx_status & RX_RESET_DONE)
				goto out_unlock;

			if (dma_rx_status & RX_DMA_PARITY_ERR) {
				uport->icount.parity++;
				drop_rx = true;
			}

			if (dma_rx_status & RX_DMA_BREAK)
				uport->icount.brk++;

			if (dma_rx_status & (RX_DMA_DONE | RX_EOT))
				qcom_geni_serial_handle_rx_dma(uport, drop_rx);
		}
	} else {
		if (m_irq_status & m_irq_en &
		    (M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN))
			qcom_geni_serial_handle_tx_fifo(uport,
					m_irq_status & M_CMD_DONE_EN,
					geni_status & M_GENI_CMD_ACTIVE);

		if (s_irq_status & (S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN))
			qcom_geni_serial_handle_rx_fifo(uport, drop_rx);
	}

out_unlock:
	uart_unlock_and_check_sysrq(uport);

	return IRQ_HANDLED;
}

static int setup_fifos(struct qcom_geni_serial_port *port)
{
	struct uart_port *uport;
	u32 old_rx_fifo_depth = port->rx_fifo_depth;

	uport = &port->uport;
	port->tx_fifo_depth = geni_se_get_tx_fifo_depth(&port->se);
	port->tx_fifo_width = geni_se_get_tx_fifo_width(&port->se);
	port->rx_fifo_depth = geni_se_get_rx_fifo_depth(&port->se);
	uport->fifosize =
		(port->tx_fifo_depth * port->tx_fifo_width) / BITS_PER_BYTE;

	if (port->rx_buf && (old_rx_fifo_depth != port->rx_fifo_depth) && port->rx_fifo_depth) {
		/*
		 * Use krealloc rather than krealloc_array because rx_buf is
		 * accessed as 1 byte entries as well as 4 byte entries so it's
		 * not necessarily an array.
		 */
		port->rx_buf = devm_krealloc(uport->dev, port->rx_buf,
					     port->rx_fifo_depth * sizeof(u32),
					     GFP_KERNEL);
		if (!port->rx_buf)
			return -ENOMEM;
	}

	return 0;
}


static void qcom_geni_serial_shutdown(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	int i, timeout;

	disable_irq(uport->irq);

	if (port->gsi_mode) {
		dev_dbg(uport->dev, "shutdown: GSI cleanup start\n");
		/*
		 * Wait for RX channel reset completion before cleanup.
		 * The framework calls stop_rx_sequencer before closing,
		 * but in atomic context we can't use wait_for_completion_timeout.
		 * So we wait here during shutdown.
		 */
		timeout = wait_for_completion_timeout(&port->xfer,
						      msecs_to_jiffies(POLL_WAIT_TIMEOUT_MSEC));
		if (!timeout)
			dev_warn(uport->dev, "%s: Timeout waiting for Rx reset\n", __func__);
		cancel_work_sync(&port->tx_xfer_work);
		cancel_work_sync(&port->rx_cancel_work);
		cancel_work_sync(&port->tx_cancel_work);
		/* Cleanup RX channel and buffers */
		if (port->gsi->rx_c) {
			dev_dbg(uport->dev, "shutdown: releasing GSI RX channel\n");

			/* Terminate any ongoing RX transfers */
			dmaengine_terminate_sync(port->gsi->rx_c);

			/* Release RX channel */
			dma_release_channel(port->gsi->rx_c);

			/* Flush RX workqueue to ensure no pending work */
			if (port->rx_wq)
				flush_workqueue(port->rx_wq);

			/* Free all RX buffers */
			for (i = 0; i < NUM_RX_BUF; i++) {
				if (port->rx_gsi_buf[i]) {
					dma_free_coherent(port->gsi->rx_c->device->dev,
							  DMA_RX_BUF_SIZE,
							  port->rx_gsi_buf[i],
							  port->rx_gsi_dma_addr[i]);
					port->rx_gsi_buf[i] = NULL;
					port->rx_gsi_dma_addr[i] = 0;
				}
			}

			port->gsi->rx_c = NULL;
			dev_dbg(uport->dev, "shutdown: GSI RX channel released\n");
		}

		/* Cleanup TX channel */
		if (port->gsi->tx_c) {
			dev_dbg(uport->dev, "shutdown: releasing GSI TX channel\n");

			/* Terminate any ongoing TX transfers */
			dmaengine_terminate_sync(port->gsi->tx_c);

			/* Release TX channel */
			dma_release_channel(port->gsi->tx_c);

			/* Flush TX workqueue to ensure no pending work */
			if (port->tx_wq)
				flush_workqueue(port->tx_wq);

			/* Unmap TX buffer if it was mapped */
			if (port->gsi->tx_ph) {
				dma_unmap_single(port->gsi->tx_c->device->dev,
						 port->gsi->tx_ph,
						 port->tx_remaining,
						 DMA_TO_DEVICE);
				port->gsi->tx_ph = 0;
			}

			port->gsi->tx_c = NULL;
			dev_dbg(uport->dev, "shutdown: GSI TX channel released\n");
		}

		/* Mark port as not setup; force port_setup() on next open */
		port->port_setup = false;
		port->setup = false;
		dev_dbg(uport->dev, "shutdown: GSI cleanup done\n");
	}

	uart_port_lock_irq(uport);
	qcom_geni_serial_stop_tx(uport);
	qcom_geni_serial_stop_rx(uport);

	qcom_geni_serial_cancel_tx_cmd(uport);
	uart_port_unlock_irq(uport);
}

static void qcom_geni_serial_flush_buffer(struct uart_port *uport)
{
	qcom_geni_serial_cancel_tx_cmd(uport);
}

static int qcom_geni_serial_port_setup(struct uart_port *uport)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	u32 rxstale = STALE_COUNT;
	u32 proto;
	u32 pin_swap;
	int ret;

	proto = geni_se_read_proto(&port->se);
	if (proto != GENI_SE_UART) {
		dev_err(uport->dev, "Invalid FW loaded, proto: %d\n", proto);
		return -ENXIO;
	}

	qcom_geni_serial_stop_rx(uport);

	ret = setup_fifos(port);
	if (ret)
		return ret;

	writel(rxstale, uport->membase + SE_UART_RX_STALE_CNT);

	pin_swap = readl(uport->membase + SE_UART_IO_MACRO_CTRL);
	if (port->rx_tx_swap) {
		pin_swap &= ~DEFAULT_IO_MACRO_IO2_IO3_MASK;
		pin_swap |= IO_MACRO_IO2_IO3_SWAP;
	}
	if (port->cts_rts_swap) {
		pin_swap &= ~DEFAULT_IO_MACRO_IO0_IO1_MASK;
		pin_swap |= IO_MACRO_IO0_SEL;
	}
	/* Configure this register if RX-TX, CTS-RTS pins are swapped */
	if (port->rx_tx_swap || port->cts_rts_swap)
		writel(pin_swap, uport->membase + SE_UART_IO_MACRO_CTRL);

	/*
	 * Make an unconditional cancel on the main sequencer to reset
	 * it else we could end up in data loss scenarios.
	 */
	if (uart_console(uport))
		qcom_geni_serial_poll_tx_done(uport);

	/* In GSI mode packing is handled by the CONFIG0 TRE */
	if (!port->gsi_mode)
		geni_se_config_packing(&port->se, BITS_PER_BYTE, BYTES_PER_FIFO_WORD,
				       false, true, true);
	geni_se_init(&port->se, UART_RX_WM, port->rx_fifo_depth - 2);

	geni_se_select_mode(&port->se, port->dev_data->mode);
	port->setup = true;
	dev_dbg(uport->dev, "port_setup: done gsi_mode=%d se_mode=%d\n",
		port->gsi_mode, port->dev_data->mode);

	return 0;
}

static int qcom_geni_serial_startup(struct uart_port *uport)
{
	int ret;
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	if (!port->setup) {
		ret = qcom_geni_serial_port_setup(uport);
		if (ret)
			return ret;
	}

	if (port->gsi_mode) {
		dev_dbg(uport->dev, "startup: GSI mode — requesting DMA channels\n");
		if (!port->gsi->tx_c) {
			port->gsi->tx_c = dma_request_chan(uport->dev, "tx");
			if (IS_ERR(port->gsi->tx_c)) {
				ret = PTR_ERR(port->gsi->tx_c);
				dev_err(uport->dev,
					"Failed to request TX channel: %d\n", ret);
				port->gsi->tx_c = NULL;
				return ret;
			}
			dev_dbg(uport->dev, "startup: TX DMA channel acquired\n");
		}

		/*
		 * RX channel: always released in shutdown() (TTY-side only).
		 * Re-request it here whenever it is NULL.
		 */
		if (!port->gsi->rx_c) {
			port->gsi->rx_c = dma_request_chan(uport->dev, "rx");
			if (IS_ERR(port->gsi->rx_c)) {
				ret = PTR_ERR(port->gsi->rx_c);
				dev_err(uport->dev,
					"Failed to request RX channel: %d\n", ret);
				port->gsi->rx_c = NULL;
				if (!uart_console(&port->uport)) {
					dma_release_channel(port->gsi->tx_c);
					port->gsi->tx_c = NULL;
				}
				return ret;
			}
			dev_dbg(uport->dev, "startup: RX DMA channel acquired\n");
		}
		ret = qcom_geni_serial_alloc_gsi_rx_bufs(uport);
		if (ret) {
			dev_err(uport->dev, "Failed to allocate RX buffers: %d\n", ret);
			dma_release_channel(port->gsi->rx_c);
			port->gsi->rx_c = NULL;
			if (!uart_console(&port->uport)) {
				dma_release_channel(port->gsi->tx_c);
				port->gsi->tx_c = NULL;
			}
			return ret;
		}

		port->port_setup = true;
		qcom_geni_serial_enable_interrupts(uport);
		dev_dbg(uport->dev, "startup: GSI RX buffers ready (4 x %d bytes)\n",
			DMA_RX_BUF_SIZE);
	}

	uart_port_lock_irq(uport);
	/*
	 * For GSI mode, rx_cfg0_t is populated by setup_config0_tre() which
	 * is called from set_termios().  Submitting a 6-TRE RX chain here
	 * before set_termios() means the CONFIG0 TRE is all-zeros
	 * (uninitialised), causing a GPI bus error (glob_err 0x86010) that
	 * permanently breaks the RX channel.
	 * RX is started from set_termios() once rx_cfg0_t is properly filled.
	 */
	if (!port->gsi_mode)
		qcom_geni_serial_start_rx(uport);
	uart_port_unlock_irq(uport);

	enable_irq(uport->irq);

	return 0;
}

static unsigned long find_clk_rate_in_tol(struct clk *clk, unsigned int desired_clk,
			unsigned int *clk_div, unsigned int percent_tol)
{
	unsigned long freq;
	unsigned long div, maxdiv;
	u64 mult;
	unsigned long offset, abs_tol, achieved;

	abs_tol = div_u64((u64)desired_clk * percent_tol, 100);
	maxdiv = CLK_DIV_MSK >> CLK_DIV_SHFT;
	div = 1;
	while (div <= maxdiv) {
		mult = (u64)div * desired_clk;
		if (mult != (unsigned long)mult)
			break;

		offset = div * abs_tol;
		freq = clk_round_rate(clk, mult - offset);

		/* Can only get lower if we're done */
		if (freq < mult - offset)
			break;

		/*
		 * Re-calculate div in case rounding skipped rates but we
		 * ended up at a good one, then check for a match.
		 */
		div = DIV_ROUND_CLOSEST(freq, desired_clk);
		achieved = DIV_ROUND_CLOSEST(freq, div);
		if (achieved <= desired_clk + abs_tol &&
		    achieved >= desired_clk - abs_tol) {
			*clk_div = div;
			return freq;
		}

		div = DIV_ROUND_UP(freq, desired_clk);
	}

	return 0;
}

static unsigned long get_clk_div_rate(struct clk *clk, unsigned int baud,
			unsigned int sampling_rate, unsigned int *clk_div)
{
	unsigned long ser_clk;
	unsigned long desired_clk;

	desired_clk = baud * sampling_rate;
	if (!desired_clk)
		return 0;

	/*
	 * try to find a clock rate within 2% tolerance, then within 5%
	 */
	ser_clk = find_clk_rate_in_tol(clk, desired_clk, clk_div, 2);
	if (!ser_clk)
		ser_clk = find_clk_rate_in_tol(clk, desired_clk, clk_div, 5);

	return ser_clk;
}

static void qcom_geni_serial_set_termios(struct uart_port *uport,
					 struct ktermios *termios,
					 const struct ktermios *old)
{
	unsigned int baud;
	u32 bits_per_char;
	u32 tx_trans_cfg;
	u32 tx_parity_cfg;
	u32 rx_trans_cfg;
	u32 rx_parity_cfg;
	u32 stop_bit_len;
	unsigned int clk_div;
	u32 ser_clk_cfg;
	struct qcom_geni_serial_port *port = to_dev_port(uport);
	unsigned long clk_rate;
	u32 ver, sampling_rate;
	unsigned int avg_bw_core;
	unsigned long timeout;

	/* baud rate */
	baud = uart_get_baud_rate(uport, termios, old, 300, 4000000);

	sampling_rate = UART_OVERSAMPLING;
	/* Sampling rate is halved for IP versions >= 2.5 */
	ver = geni_se_get_qup_hw_version(&port->se);
	if (ver >= QUP_SE_VERSION_2_5)
		sampling_rate /= 2;

	clk_rate = get_clk_div_rate(port->se.clk, baud,
		sampling_rate, &clk_div);
	if (!clk_rate) {
		dev_err(port->se.dev,
			"Couldn't find suitable clock rate for %u\n",
			baud * sampling_rate);
		return;
	}

	dev_dbg(port->se.dev, "desired_rate = %u, clk_rate = %lu, clk_div = %u\n",
			baud * sampling_rate, clk_rate, clk_div);

	uport->uartclk = clk_rate;
	port->clk_rate = clk_rate;
	dev_pm_opp_set_rate(uport->dev, clk_rate);
	ser_clk_cfg = SER_CLK_EN;
	ser_clk_cfg |= clk_div << CLK_DIV_SHFT;

	/*
	 * Bump up BW vote on CPU and CORE path as driver supports FIFO mode
	 * only.
	 */
	avg_bw_core = (baud > 115200) ? Bps_to_icc(CORE_2X_50_MHZ)
						: GENI_DEFAULT_BW;
	port->se.icc_paths[GENI_TO_CORE].avg_bw = avg_bw_core;
	port->se.icc_paths[CPU_TO_GENI].avg_bw = Bps_to_icc(baud);
	geni_icc_set_bw(&port->se);

	/* parity */
	tx_trans_cfg = readl(uport->membase + SE_UART_TX_TRANS_CFG);
	tx_parity_cfg = readl(uport->membase + SE_UART_TX_PARITY_CFG);
	rx_trans_cfg = readl(uport->membase + SE_UART_RX_TRANS_CFG);
	rx_parity_cfg = readl(uport->membase + SE_UART_RX_PARITY_CFG);
	if (termios->c_cflag & PARENB) {
		tx_trans_cfg |= UART_TX_PAR_EN;
		rx_trans_cfg |= UART_RX_PAR_EN;
		tx_parity_cfg |= PAR_CALC_EN;
		rx_parity_cfg |= PAR_CALC_EN;
		if (termios->c_cflag & PARODD) {
			tx_parity_cfg |= PAR_ODD;
			rx_parity_cfg |= PAR_ODD;
		} else if (termios->c_cflag & CMSPAR) {
			tx_parity_cfg |= PAR_SPACE;
			rx_parity_cfg |= PAR_SPACE;
		} else {
			tx_parity_cfg |= PAR_EVEN;
			rx_parity_cfg |= PAR_EVEN;
		}
	} else {
		tx_trans_cfg &= ~UART_TX_PAR_EN;
		rx_trans_cfg &= ~UART_RX_PAR_EN;
		tx_parity_cfg &= ~PAR_CALC_EN;
		rx_parity_cfg &= ~PAR_CALC_EN;
	}

	/* bits per char */
	bits_per_char = tty_get_char_size(termios->c_cflag);

	/* stop bits */
	if (termios->c_cflag & CSTOPB)
		stop_bit_len = TX_STOP_BIT_LEN_2;
	else
		stop_bit_len = TX_STOP_BIT_LEN_1;

	/* flow control, clear the CTS_MASK bit if using flow control. */
	if (termios->c_cflag & CRTSCTS)
		tx_trans_cfg &= ~UART_CTS_MASK;
	else
		tx_trans_cfg |= UART_CTS_MASK;

	if (baud) {
		uart_update_timeout(uport, termios->c_cflag, baud);

		/*
		 * Make sure that qcom_geni_serial_poll_bitfield() waits for
		 * the FIFO, two-word intermediate transfer register and shift
		 * register to clear.
		 *
		 * Note that uart_fifo_timeout() also adds a 20 ms margin.
		 */
		timeout = jiffies_to_usecs(uart_fifo_timeout(uport));
		timeout += 3 * timeout / port->tx_fifo_depth;
		WRITE_ONCE(port->poll_timeout_us, timeout);
	}

	if (port->gsi_mode) {
		if (port->port_setup) {
			dev_dbg(uport->dev,
				"set_termios: GSI — stopping RX for reconfiguration\n");
			if (port->tx_wq)
				flush_workqueue(port->tx_wq);
			if (port->rx_wq)
				flush_workqueue(port->rx_wq);
			reinit_completion(&port->xfer);
			qcom_geni_serial_stop_rx(uport);
			if (!wait_for_completion_timeout(&port->xfer,
							 msecs_to_jiffies(2000)))
				dev_warn(uport->dev,
					 "set_termios: GSI stop_rx timeout\n");
		}
		setup_config0_tre(uport, bits_per_char, clk_div, stop_bit_len,
				  tx_parity_cfg, !(tx_trans_cfg & UART_CTS_MASK),
				  rx_parity_cfg, port->loopback);
		writel(port->loopback, uport->membase + SE_UART_LOOPBACK_CFG);
		writel(ser_clk_cfg, uport->membase + GENI_SER_M_CLK_CFG);
		writel(ser_clk_cfg, uport->membase + GENI_SER_S_CLK_CFG);
		dev_dbg(uport->dev,
			"set_termios: GSI baud=%u clk_div=%u loopback=%u clk_cfg=0x%x\n",
			baud, clk_div, port->loopback, ser_clk_cfg);
		/* Restart RX with new config */
		if (port->port_setup) {
			dev_dbg(uport->dev, "set_termios: GSI — restarting RX\n");
			qcom_geni_serial_start_rx(uport);
		}
	} else {
		if (!uart_console(uport))
			writel(port->loopback,
			       uport->membase + SE_UART_LOOPBACK_CFG);
		writel(tx_trans_cfg, uport->membase + SE_UART_TX_TRANS_CFG);
		writel(tx_parity_cfg, uport->membase + SE_UART_TX_PARITY_CFG);
		writel(rx_trans_cfg, uport->membase + SE_UART_RX_TRANS_CFG);
		writel(rx_parity_cfg, uport->membase + SE_UART_RX_PARITY_CFG);
		writel(bits_per_char, uport->membase + SE_UART_TX_WORD_LEN);
		writel(bits_per_char, uport->membase + SE_UART_RX_WORD_LEN);
		writel(stop_bit_len, uport->membase + SE_UART_TX_STOP_BIT_LEN);
		writel(ser_clk_cfg, uport->membase + GENI_SER_M_CLK_CFG);
		writel(ser_clk_cfg, uport->membase + GENI_SER_S_CLK_CFG);
	}
}

#ifdef CONFIG_SERIAL_QCOM_GENI_CONSOLE
static int qcom_geni_console_setup(struct console *co, char *options)
{
	struct uart_port *uport;
	struct qcom_geni_serial_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int ret;

	if (co->index >= GENI_UART_CONS_PORTS  || co->index < 0)
		return -ENXIO;

	port = get_port_from_line(co->index, true);
	if (IS_ERR(port)) {
		pr_err("Invalid line %d\n", co->index);
		return PTR_ERR(port);
	}

	uport = &port->uport;

	if (unlikely(!uport->membase))
		return -ENXIO;

	if (!port->setup) {
		ret = qcom_geni_serial_port_setup(uport);
		if (ret)
			return ret;
	}

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(uport, co, baud, parity, bits, flow);
}

static void qcom_geni_serial_earlycon_write(struct console *con,
					const char *s, unsigned int n)
{
	struct earlycon_device *dev = con->data;

	__qcom_geni_serial_console_write(&dev->port, s, n);
}

#ifdef CONFIG_CONSOLE_POLL
static int qcom_geni_serial_earlycon_read(struct console *con,
					  char *s, unsigned int n)
{
	struct earlycon_device *dev = con->data;
	struct uart_port *uport = &dev->port;
	int num_read = 0;
	int ch;

	while (num_read < n) {
		ch = qcom_geni_serial_get_char(uport);
		if (ch == NO_POLL_CHAR)
			break;
		s[num_read++] = ch;
	}

	return num_read;
}

static void __init qcom_geni_serial_enable_early_read(struct geni_se *se,
						      struct console *con)
{
	geni_se_setup_s_cmd(se, UART_START_READ, 0);
	con->read = qcom_geni_serial_earlycon_read;
}
#else
static inline void qcom_geni_serial_enable_early_read(struct geni_se *se,
						      struct console *con) { }
#endif

static struct qcom_geni_private_data earlycon_private_data;

static int __init qcom_geni_serial_earlycon_setup(struct earlycon_device *dev,
								const char *opt)
{
	struct uart_port *uport = &dev->port;
	u32 tx_trans_cfg;
	u32 tx_parity_cfg = 0;	/* Disable Tx Parity */
	u32 rx_trans_cfg = 0;
	u32 rx_parity_cfg = 0;	/* Disable Rx Parity */
	u32 stop_bit_len = 0;	/* Default stop bit length - 1 bit */
	u32 bits_per_char;
	struct geni_se se;

	if (!uport->membase)
		return -EINVAL;

	uport->private_data = &earlycon_private_data;

	memset(&se, 0, sizeof(se));
	se.base = uport->membase;
	if (geni_se_read_proto(&se) != GENI_SE_UART)
		return -ENXIO;
	/*
	 * Ignore Flow control.
	 * n = 8.
	 */
	tx_trans_cfg = UART_CTS_MASK;
	bits_per_char = BITS_PER_BYTE;

	/*
	 * Make an unconditional cancel on the main sequencer to reset
	 * it else we could end up in data loss scenarios.
	 */
	qcom_geni_serial_poll_tx_done(uport);
	qcom_geni_serial_abort_rx(uport);
	geni_se_config_packing(&se, BITS_PER_BYTE, BYTES_PER_FIFO_WORD,
			       false, true, true);
	geni_se_init(&se, DEF_FIFO_DEPTH_WORDS / 2, DEF_FIFO_DEPTH_WORDS - 2);
	geni_se_select_mode(&se, GENI_SE_FIFO);

	writel(tx_trans_cfg, uport->membase + SE_UART_TX_TRANS_CFG);
	writel(tx_parity_cfg, uport->membase + SE_UART_TX_PARITY_CFG);
	writel(rx_trans_cfg, uport->membase + SE_UART_RX_TRANS_CFG);
	writel(rx_parity_cfg, uport->membase + SE_UART_RX_PARITY_CFG);
	writel(bits_per_char, uport->membase + SE_UART_TX_WORD_LEN);
	writel(bits_per_char, uport->membase + SE_UART_RX_WORD_LEN);
	writel(stop_bit_len, uport->membase + SE_UART_TX_STOP_BIT_LEN);

	dev->con->write = qcom_geni_serial_earlycon_write;
	dev->con->setup = NULL;
	qcom_geni_serial_enable_early_read(&se, dev->con);

	return 0;
}
OF_EARLYCON_DECLARE(qcom_geni, "qcom,geni-debug-uart",
				qcom_geni_serial_earlycon_setup);

static int __init console_register(struct uart_driver *drv)
{
	return uart_register_driver(drv);
}

static void console_unregister(struct uart_driver *drv)
{
	uart_unregister_driver(drv);
}

static struct console cons_ops = {
	.name = "ttyMSM",
	.write = qcom_geni_serial_console_write,
	.device = uart_console_device,
	.setup = qcom_geni_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &qcom_geni_console_driver,
};

static struct uart_driver qcom_geni_console_driver = {
	.owner = THIS_MODULE,
	.driver_name = "qcom_geni_console",
	.dev_name = "ttyMSM",
	.nr =  GENI_UART_CONS_PORTS,
	.cons = &cons_ops,
};
#else
static int console_register(struct uart_driver *drv)
{
	return 0;
}

static void console_unregister(struct uart_driver *drv)
{
}
#endif /* CONFIG_SERIAL_QCOM_GENI_CONSOLE */

static struct uart_driver qcom_geni_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "qcom_geni_uart",
	.dev_name = "ttyHS",
	.nr =  GENI_UART_PORTS,
};

static void qcom_geni_serial_pm(struct uart_port *uport,
		unsigned int new_state, unsigned int old_state)
{
	struct qcom_geni_serial_port *port = to_dev_port(uport);

	/* If we've never been called, treat it as off */
	if (old_state == UART_PM_STATE_UNDEFINED)
		old_state = UART_PM_STATE_OFF;

	if (new_state == UART_PM_STATE_ON && old_state == UART_PM_STATE_OFF) {
		geni_icc_enable(&port->se);
		if (port->clk_rate)
			dev_pm_opp_set_rate(uport->dev, port->clk_rate);
		geni_se_resources_on(&port->se);
	} else if (new_state == UART_PM_STATE_OFF &&
			old_state == UART_PM_STATE_ON) {
		geni_se_resources_off(&port->se);
		dev_pm_opp_set_rate(uport->dev, 0);
		geni_icc_disable(&port->se);
	}
}

static const struct uart_ops qcom_geni_console_pops = {
	.tx_empty = qcom_geni_serial_tx_empty,
	.stop_tx = qcom_geni_serial_stop_tx_fifo,
	.start_tx = qcom_geni_serial_start_tx_fifo,
	.stop_rx = qcom_geni_serial_stop_rx_fifo,
	.start_rx = qcom_geni_serial_start_rx_fifo,
	.set_termios = qcom_geni_serial_set_termios,
	.startup = qcom_geni_serial_startup,
	.request_port = qcom_geni_serial_request_port,
	.config_port = qcom_geni_serial_config_port,
	.shutdown = qcom_geni_serial_shutdown,
	.flush_buffer = qcom_geni_serial_flush_buffer,
	.type = qcom_geni_serial_get_type,
	.set_mctrl = qcom_geni_serial_set_mctrl,
	.get_mctrl = qcom_geni_serial_get_mctrl,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char	= qcom_geni_serial_get_char,
	.poll_put_char	= qcom_geni_serial_poll_put_char,
	.poll_init = qcom_geni_serial_poll_init,
#endif
	.pm = qcom_geni_serial_pm,
};

static const struct uart_ops qcom_geni_uart_pops = {
	.tx_empty = qcom_geni_serial_tx_empty,
	.stop_tx = qcom_geni_serial_stop_tx_dma,
	.start_tx = qcom_geni_serial_start_tx_dma,
	.start_rx = qcom_geni_serial_start_rx_dma,
	.stop_rx = qcom_geni_serial_stop_rx_dma,
	.set_termios = qcom_geni_serial_set_termios,
	.startup = qcom_geni_serial_startup,
	.request_port = qcom_geni_serial_request_port,
	.config_port = qcom_geni_serial_config_port,
	.shutdown = qcom_geni_serial_shutdown,
	.type = qcom_geni_serial_get_type,
	.set_mctrl = qcom_geni_serial_set_mctrl,
	.get_mctrl = qcom_geni_serial_get_mctrl,
	.pm = qcom_geni_serial_pm,
};

static int qcom_geni_serial_probe(struct platform_device *pdev)
{
	int ret = 0;
	int line;
	struct qcom_geni_serial_port *port;
	struct uart_port *uport;
	struct resource *res;
	int irq;
	struct uart_driver *drv;
	const struct qcom_geni_device_data *data;

	data = of_device_get_match_data(&pdev->dev);
	if (!data)
		return -EINVAL;

	if (data->console) {
		drv = &qcom_geni_console_driver;
		line = of_alias_get_id(pdev->dev.of_node, "serial");
	} else {
		drv = &qcom_geni_uart_driver;
		line = of_alias_get_id(pdev->dev.of_node, "serial");
		if (line == -ENODEV) /* compat with non-standard aliases */
			line = of_alias_get_id(pdev->dev.of_node, "hsuart");
	}

	port = get_port_from_line(line, data->console);
	if (IS_ERR(port)) {
		dev_err(&pdev->dev, "Invalid line %d\n", line);
		return PTR_ERR(port);
	}

	uport = &port->uport;
	/* Don't allow 2 drivers to access the same port */
	if (uport->private_data)
		return -ENODEV;

	uport->dev = &pdev->dev;
	port->dev_data = data;
	port->se.dev = &pdev->dev;
	port->se.wrapper = dev_get_drvdata(pdev->dev.parent);
	port->se.clk = devm_clk_get(&pdev->dev, "se");
	if (IS_ERR(port->se.clk)) {
		ret = PTR_ERR(port->se.clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		return ret;
	}

	ret = geni_se_resources_on(&port->se);
	if (ret) {
		dev_err(&pdev->dev, "Error turning on resources %d\n", ret);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	uport->mapbase = res->start;

	port->tx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	port->rx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	port->tx_fifo_width = DEF_FIFO_WIDTH_BITS;

	if (!data->console) {
		port->rx_buf = devm_kzalloc(uport->dev,
					    DMA_RX_BUF_SIZE, GFP_KERNEL);
		if (!port->rx_buf)
			return -ENOMEM;
	}

	ret = geni_icc_get(&port->se, NULL);
	if (ret)
		return ret;
	port->se.icc_paths[GENI_TO_CORE].avg_bw = GENI_DEFAULT_BW;
	port->se.icc_paths[CPU_TO_GENI].avg_bw = GENI_DEFAULT_BW;

	/* Set BW for register access */
	ret = geni_icc_set_bw(&port->se);
	if (ret)
		return ret;

	port->name = devm_kasprintf(uport->dev, GFP_KERNEL,
			"qcom_geni_serial_%s%d",
			uart_console(uport) ? "console" : "uart", uport->line);
	if (!port->name)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	uport->irq = irq;
	uport->has_sysrq = IS_ENABLED(CONFIG_SERIAL_QCOM_GENI_CONSOLE);

	if (!data->console)
		port->wakeup_irq = platform_get_irq_optional(pdev, 1);

	if (of_property_read_bool(pdev->dev.of_node, "rx-tx-swap"))
		port->rx_tx_swap = true;

	if (of_property_read_bool(pdev->dev.of_node, "cts-rts-swap"))
		port->cts_rts_swap = true;

	ret = devm_pm_opp_set_clkname(&pdev->dev, "se");
	if (ret)
		return ret;
	/* OPP table is optional */
	ret = devm_pm_opp_of_add_table(&pdev->dev);
	if (ret && ret != -ENODEV) {
		dev_err(&pdev->dev, "invalid OPP table in device tree\n");
		return ret;
	}

	port->private_data.drv = drv;
	uport->private_data = &port->private_data;
	platform_set_drvdata(pdev, port);

	irq_set_status_flags(uport->irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(uport->dev, uport->irq, qcom_geni_serial_isr,
			IRQF_TRIGGER_HIGH, port->name, uport);
	if (ret) {
		dev_err(uport->dev, "Failed to get IRQ ret %d\n", ret);
		return ret;
	}

	/* Initialize GSI mode */
	qcom_geni_serial_init_gsi(uport);

	ret = uart_add_one_port(drv, uport);
	if (ret)
		return ret;

	if (port->wakeup_irq > 0) {
		device_init_wakeup(&pdev->dev, true);
		ret = dev_pm_set_dedicated_wake_irq(&pdev->dev,
						port->wakeup_irq);
		if (ret) {
			device_init_wakeup(&pdev->dev, false);
			uart_remove_one_port(drv, uport);
			return ret;
		}
	}

	return 0;
}

static int qcom_geni_serial_remove(struct platform_device *pdev)
{
	struct qcom_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_driver *drv = port->private_data.drv;

	dev_pm_clear_wake_irq(&pdev->dev);
	device_init_wakeup(&pdev->dev, false);
	uart_remove_one_port(drv, &port->uport);
	return 0;
}

/**
 * qcom_geni_uart_gsi_suspend_resume() - Suspend/Resume GSI DMA channels
 * @port: Pointer to qcom_geni_serial_port
 * @suspend: true for suspend, false for resume
 *
 * Handles DMA channel pause/resume for GSI mode during power management.
 * Only operates on TX channel as GPI driver handles RX channel internally.
 * Operating on both channels causes state machine issues in the GPI driver.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qcom_geni_uart_gsi_suspend_resume(struct qcom_geni_serial_port *port,
					     bool suspend)
{
	int ret = 0;

	/* Only handle GSI mode */
	if (!port->gsi_mode || !port->gsi)
		return 0;

	/*
	 * Only operate on TX channel - GPI driver handles RX internally.
	 * Operating on both channels causes state machine issues.
	 */
	if (port->gsi->tx_c) {
		if (suspend) {
			/* Pause TX DMA channel */
			ret = dmaengine_pause(port->gsi->tx_c);
			if (ret) {
				dev_err(port->uport.dev,
					"Failed to pause TX channel: %d\n", ret);
				return ret;
			}
			dev_info(port->uport.dev, "GSI TX channel paused\n");
		} else {
			/* Resume TX DMA channel */
			ret = dmaengine_resume(port->gsi->tx_c);
			if (ret) {
				dev_err(port->uport.dev,
					"Failed to resume TX channel: %d\n", ret);
				return ret;
			}
			dev_info(port->uport.dev, "GSI TX channel resumed\n");
		}
	}

	return 0;
}

static int qcom_geni_serial_sys_suspend(struct device *dev)
{
	struct qcom_geni_serial_port *port = dev_get_drvdata(dev);
	struct uart_port *uport = &port->uport;
	struct qcom_geni_private_data *private_data = uport->private_data;
	int ret;

	/*
	 * This is done so we can hit the lowest possible state in suspend
	 * even with no_console_suspend
	 */
	if (uart_console(uport)) {
		geni_icc_set_tag(&port->se, QCOM_ICC_TAG_ACTIVE_ONLY);
		geni_icc_set_bw(&port->se);
	}

	/* Handle GSI mode suspend */
	if (port->gsi_mode) {
		/* Flush work queues before suspending */
		if (port->tx_wq)
			flush_workqueue(port->tx_wq);
		if (port->rx_wq)
			flush_workqueue(port->rx_wq);

		/* Pause GSI DMA channels */
		ret = qcom_geni_uart_gsi_suspend_resume(port, true);
		if (ret) {
			dev_err(dev, "GSI suspend failed: %d\n", ret);
			return ret;
		}
	}

	return uart_suspend_port(private_data->drv, uport);
}

static int qcom_geni_serial_sys_resume(struct device *dev)
{
	int ret;
	struct qcom_geni_serial_port *port = dev_get_drvdata(dev);
	struct uart_port *uport = &port->uport;
	struct qcom_geni_private_data *private_data = uport->private_data;

	ret = uart_resume_port(private_data->drv, uport);
	if (uart_console(uport)) {
		geni_icc_set_tag(&port->se, QCOM_ICC_TAG_ALWAYS);
		geni_icc_set_bw(&port->se);
	}

	/* Handle GSI mode suspend */
	if (port->gsi_mode) {
		/* Flush work queues before suspending */
		if (port->tx_wq)
			flush_workqueue(port->tx_wq);
		if (port->rx_wq)
			flush_workqueue(port->rx_wq);

		/* Pause GSI DMA channels */
		ret = qcom_geni_uart_gsi_suspend_resume(port, false);
		if (ret) {
			dev_err(dev, "GSI suspend failed: %d\n", ret);
			return ret;
		}
	}

	qcom_geni_serial_enable_interrupts(uport);
	return ret;
}

static const struct qcom_geni_device_data qcom_geni_console_data = {
	.console = true,
	.mode = GENI_SE_FIFO,
};

static const struct qcom_geni_device_data qcom_geni_uart_data = {
	.console = false,
	.mode = GENI_SE_DMA,
};

static const struct qcom_geni_device_data qcom_geni_uart_gsi_data = {
	.console = false,
	.mode = GENI_GPI_DMA,
};

static const struct dev_pm_ops qcom_geni_serial_pm_ops = {
	SYSTEM_SLEEP_PM_OPS(qcom_geni_serial_sys_suspend,
					qcom_geni_serial_sys_resume)
};

static const struct of_device_id qcom_geni_serial_match_table[] = {
	{
		.compatible = "qcom,geni-debug-uart",
		.data = &qcom_geni_console_data,
	},
	{
		.compatible = "qcom,geni-uart",
		.data = &qcom_geni_uart_data,
	},
	{
		.compatible = "qcom,geni-uart-gsi",
		.data = &qcom_geni_uart_gsi_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_geni_serial_match_table);

static struct platform_driver qcom_geni_serial_platform_driver = {
	.remove = qcom_geni_serial_remove,
	.probe = qcom_geni_serial_probe,
	.driver = {
		.name = "qcom_geni_serial",
		.of_match_table = qcom_geni_serial_match_table,
		.pm = &qcom_geni_serial_pm_ops,
	},
};

static int __init qcom_geni_serial_init(void)
{
	int ret;

	ret = console_register(&qcom_geni_console_driver);
	if (ret)
		return ret;

	ret = uart_register_driver(&qcom_geni_uart_driver);
	if (ret) {
		console_unregister(&qcom_geni_console_driver);
		return ret;
	}

	ret = platform_driver_register(&qcom_geni_serial_platform_driver);
	if (ret) {
		console_unregister(&qcom_geni_console_driver);
		uart_unregister_driver(&qcom_geni_uart_driver);
	}
	return ret;
}
module_init(qcom_geni_serial_init);

static void __exit qcom_geni_serial_exit(void)
{
	platform_driver_unregister(&qcom_geni_serial_platform_driver);
	console_unregister(&qcom_geni_console_driver);
	uart_unregister_driver(&qcom_geni_uart_driver);
}
module_exit(qcom_geni_serial_exit);

MODULE_DESCRIPTION("Serial driver for GENI based QUP cores");
MODULE_LICENSE("GPL v2");
