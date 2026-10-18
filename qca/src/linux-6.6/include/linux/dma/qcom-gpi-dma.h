/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020, Linaro Limited
 */

#ifndef QCOM_GPI_DMA_H
#define QCOM_GPI_DMA_H

/**
 * enum spi_transfer_cmd - spi transfer commands
 */
enum spi_transfer_cmd {
	SPI_TX = 1,
	SPI_RX,
	SPI_DUPLEX,
};

/**
 * struct gpi_spi_config - spi config for peripheral
 *
 * @loopback_en: spi loopback enable when set
 * @clock_pol_high: clock polarity
 * @data_pol_high: data polarity
 * @pack_en: process tx/rx buffers as packed
 * @word_len: spi word length
 * @clk_div: source clock divider
 * @clk_src: serial clock
 * @cmd: spi cmd
 * @fragmentation: keep CS asserted at end of sequence
 * @cs: chip select toggle
 * @set_config: set peripheral config
 * @rx_len: receive length for buffer
 */
struct gpi_spi_config {
	u8 set_config;
	u8 loopback_en;
	u8 clock_pol_high;
	u8 data_pol_high;
	u8 pack_en;
	u8 word_len;
	u8 fragmentation;
	u8 cs;
	u32 clk_div;
	u32 clk_src;
	enum spi_transfer_cmd cmd;
	u32 rx_len;
};

enum i2c_op {
	I2C_WRITE = 1,
	I2C_READ,
};

/**
 * struct gpi_i2c_config - i2c config for peripheral
 *
 * @pack_enable: process tx/rx buffers as packed
 * @cycle_count: clock cycles to be sent
 * @high_count: high period of clock
 * @low_count: low period of clock
 * @clk_div: source clock divider
 * @addr: i2c bus address
 * @stretch: stretch the clock at eot
 * @set_config: set peripheral config
 * @rx_len: receive length for buffer
 * @op: i2c cmd
 * @muli-msg: is part of multi i2c r-w msgs
 */
struct gpi_i2c_config {
	u8 set_config;
	u8 pack_enable;
	u8 cycle_count;
	u8 high_count;
	u8 low_count;
	u8 addr;
	u8 stretch;
	u16 clk_div;
	u32 rx_len;
	enum i2c_op op;
	bool multi_msg;
};

struct __packed qcom_gpi_tre {
	u32 dword[4];
};

/* UART Go TRE */
#define QCOM_GPI_UART_GO_TRE_DWORD0(en_hunt, command) (((en_hunt) << 8) | (command))
#define QCOM_GPI_UART_GO_TRE_DWORD1 (0)
#define QCOM_GPI_UART_GO_TRE_DWORD2 (0)
#define QCOM_GPI_UART_GO_TRE_DWORD3(link_rx, bei, ieot, ieob, ch) \
	((0x2 << 20) | (0x0 << 16) | ((link_rx) << 11) | ((bei) << 10) | \
	((ieot) << 9) | ((ieob) << 8) | (ch))

/* UART Config0 TRE */
#define QCOM_GPI_UART_CONFIG0_TRE_DWORD0(pack, hunt, flags, parity, sbl, size) \
	(((pack) << 24) | ((hunt) << 16) | ((flags) << 8) | ((parity) << 5) | \
	((sbl) << 3) | (size))
#define QCOM_GPI_UART_CONFIG0_TRE_DWORD1(rfr_level, rx_stale) \
	(((rfr_level) << 24) | (rx_stale))
#define QCOM_GPI_UART_CONFIG0_TRE_DWORD2(clk_source, clk_div) \
	(((clk_source) << 16) | (clk_div))
#define QCOM_GPI_UART_CONFIG0_TRE_DWORD3(link_rx, bei, ieot, ieob, ch) \
	((0x2 << 20) | (0x2 << 16) | ((link_rx) << 11) | ((bei) << 10) | \
	((ieot) << 9) | ((ieob) << 8) | (ch))

/* DMA w. Buffer TRE */
#ifdef CONFIG_ARM64
#define QCOM_GPI_DMA_W_BUFFER_TRE_DWORD0(ptr) ((u32)ptr)
#define QCOM_GPI_DMA_W_BUFFER_TRE_DWORD1(ptr) ((u32)((ptr) >> 32))
#else
#define QCOM_GPI_DMA_W_BUFFER_TRE_DWORD0(ptr) (ptr)
#define QCOM_GPI_DMA_W_BUFFER_TRE_DWORD1(ptr) 0
#endif

#define QCOM_GPI_DMA_W_BUFFER_TRE_DWORD2(length) ((length) & 0xFFFFFF)
#define QCOM_GPI_DMA_W_BUFFER_TRE_DWORD3(link_rx, bei, ieot, ieob, ch) \
	((0x1 << 20) | (0x0 << 16) | ((link_rx) << 11) | ((bei) << 10) | \
	((ieot) << 9) | ((ieob) << 8) | (ch))

enum qcom_gpi_tce_code {
	QCOM_GPI_TCE_SUCCESS = 1,
	QCOM_GPI_TCE_EOT = 2,
	QCOM_GPI_TCE_EOB = 4,
	QCOM_GPI_TCE_UNEXP_ERR = 16,
};

/*
 * gpi specific callback parameters to pass between gpi client and gpi engine.
 * client shall set async_desc.callback_parm to qcom_gpi_dma_async_tx_cb_param
 */
struct qcom_gpi_dma_async_tx_cb_param {
	u32 length;
	enum qcom_gpi_tce_code completion_code; /* TCE event code */
	u32 status;
	struct __packed qcom_gpi_tre imed_tre;
	void *userdata;
};

/**
 * struct gpi_uart_config - UART config for peripheral
 *
 * @set_config: set peripheral config
 * @pack_en: process tx/rx buffers as packed
 * @hunt_char: hunt character for pattern matching
 * @flags: UART control flags
 * @parity: parity mode (0=none, 1=odd, 2=even)
 * @stop_bits: stop bit length (0=0.5, 1=1, 2=1.5, 3=2)
 * @char_size: character size (0=5bits, 7=8bits)
 * @en_hunt: enable hunt mode
 * @command: UART command
 * @rfr_level: RFR watermark level
 * @rx_stale: RX stale timeout count
 * @clk_src: clock source
 * @clk_div: clock divider
 */
struct gpi_uart_config {
	u8 set_config;
	u8 pack_en;
	u8 hunt_char;
	u8 flags;
	u8 parity;
	u8 stop_bits;
	u8 char_size;
	u8 en_hunt;
	u8 command;
	u8 rfr_level;
	u16 rx_stale;
	u16 clk_src;
	u16 clk_div;
};

#endif /* QCOM_GPI_DMA_H */
