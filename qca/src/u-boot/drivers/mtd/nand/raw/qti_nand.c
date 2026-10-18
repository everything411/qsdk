// Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.

// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#include <common.h>
#include <malloc.h>
#include <memalign.h>
#include <command.h>
#include <asm/io.h>
#include <errno.h>
#include <nand.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/rawnand.h>
#include <mach/bam.h>
#include <fdtdec.h>
#include <cpu_func.h>
#include <dm/pinctrl.h>
#include <dm.h>
#include <clk.h>
#include <watchdog.h>

#include "qti_nand.h"

DECLARE_GLOBAL_DATA_PTR;

/*
 * NAND Flash Configs
 */
#define MICRON_DEVICE_ID	0x152c152c
#define WINBOND_MFR_ID		0xef
#define CMD3_MASK		0xfff0ffff
#define TRAINING_PART_OFFSET	0x3c00000
#define DEFAULT_CLK_200_MHZ	200000000
#define CHECK_QUAD_CONFIG	0x00000001
#define QUAD_MODE		0x00000020

#define MAXIMUM_ALLOCATED_TRAINING_BLOCK	4

#define TOTAL_NUM_PHASE	7

struct nand_flash_dev qti_nand_flash_ids[] = {
	{"GD5F1GQ4RE9IG",
		{ .id = {0xc8, 0xc1} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"GD5F1GQ5REYIG",
		{ .id = {0xc8, 0x41} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"GD5F1GQ4RE9IH",
		{ .id = {0xc8, 0xc9} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"GD5F4GQ6REYIHR",
		{ .id = {0xc8, 0x25} },
		SZ_2K, SZ_512, SZ_128K, CHECK_QUAD_CONFIG, 2, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"MT29F1G01ABBFDWB-IT",
		{ .id = {0x2c, 0x15} },
		SZ_2K, SZ_128, SZ_128K, QUAD_MODE, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"W25N01JW",
		{ .id = {0xef, 0xbc, 0x21} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 3, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"F50D1G41LB(2M)",
		{ .id = {0xc8, 0x11} },
		SZ_2K, SZ_128, SZ_128K, QUAD_MODE, 2, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"GD5F1GM7REYIG",
		{ .id = {0xc8, 0x81} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"GD5F2GM7REYIG",
		{ .id = {0xc8, 0x82} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"GD5F1GQ5REYIH",
		{ .id = {0xc8, 0x21} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"W25N02JWZEIF",
		{ .id = {0xef, 0xbf, 0x22} },
		SZ_2K, SZ_256, SZ_128K, CHECK_QUAD_CONFIG, 3, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"W25N01GWZEIG",
		{ .id = {0xef, 0xba, 0x21} },
		SZ_2K, SZ_128, SZ_128K, QUAD_MODE, 3, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"W25N512GW",
		{ .id = {0xef, 0xba, 0x20} },
		SZ_2K, SZ_64, SZ_128K, QUAD_MODE, 3, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"W25N02KWZEIR",
		{ .id = {0xef, 0xba, 0x22} },
		SZ_2K, SZ_256, SZ_128K, QUAD_MODE, 3, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"W25N04KWZEIR",
		{ .id = {0xef, 0xba, 0x23} },
		SZ_2K, SZ_512, SZ_128K, QUAD_MODE, 3, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"MX35UF1GE4AC",
		{ .id = {0xc2, 0x92} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"F50D2G41KA-83YIG2V",
		{ .id = {0xc8, 0x51} },
		SZ_2K, SZ_256, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"DS35M1GA",
		{ .id = {0xe5, 0x21} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 64,
		NAND_ECC_INFO(4, SZ_512), 0},
	{"GD5F2GQ5REYIG",
		{ .id = {0xc8, 0x42} },
		SZ_2K, SZ_256, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"MX35UF2GE4AD",
		{ .id = {0xc2, 0xa6} },
		SZ_2K, SZ_256, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"MX35UF1G24AD",
		{ .id = {0xc2, 0x94} },
		SZ_2K, SZ_128, SZ_128K, CHECK_QUAD_CONFIG, 2, 128,
		NAND_ECC_INFO(8, SZ_512), 0},
	{"GD5F2GQ5REYIH SPI NAND 2G 4-bit",
		{ .id = {0xc8, 0x22} },
		SZ_2K, SZ_256, SZ_128K, CHECK_QUAD_CONFIG, 2, 64,
		NAND_ECC_INFO(4, SZ_512) },
	{"MX35UF4GE4AD-Z4I SPI NAND 4G 1.8V",
		{ .id = {0xc2, 0xb7} },
		SZ_4K, SZ_512, SZ_256K, CHECK_QUAD_CONFIG, 2, 256,
		NAND_ECC_INFO(8, SZ_512)},
	{NULL}
};

struct nand_flash_dev qpic_nand_flash_ids_2k[] = {
	{"MX35UF4GE4AD-Z4I SPI NAND 1G 1.8V",
			{ .id = {0xc2, 0xb7} },
		SZ_2K, SZ_512, SZ_128K, 0, 2, 128, NAND_ECC_INFO(8, SZ_512), 0},
	{NULL}
};

extern int ipq_get_training_part_info(uint32_t *offset, uint32_t *size);
int qti_nand_deinit(struct udevice *device);

static int qti_read_page(struct mtd_info *mtd, uint32_t page,
				enum nand_cfg_value cfg_mode,
				struct mtd_oob_ops *ops);

size_t memlcpy(void *dest, size_t dst_size, const void *src, size_t copy_size)
{
	size_t min_size = dst_size < copy_size ? dst_size : copy_size;
	memcpy(dest, src, min_size);
	return min_size;
}

static void qti_nandc_wait_for_cmd_exec(struct qcom_nand_controller *nandc,
							uint32_t num_desc)
{
	/* Create a read/write event
	to notify the periperal of the added desc. */
	q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, num_desc);

	/* Wait for the descriptors to be processed */
	q_bam_wait_for_interrupt(&nandc->bam, CMD_PIPE_INDEX,
						P_PRCSD_DESC_EN_MASK);

	/* Read offset update for the circular FIFO */
	q_bam_read_offset_update(&nandc->bam, CMD_PIPE_INDEX);
}

static void
qti_nandc_wait_for_data(struct qcom_nand_controller *nandc, uint32_t pipe_num)
{
	/* Wait for the descriptors to be processed */
	q_bam_wait_for_interrupt(&nandc->bam, pipe_num, P_PRCSD_DESC_EN_MASK);

	/* Read offset update for the circular FIFO */
	q_bam_read_offset_update(&nandc->bam, pipe_num);
}

static uint32_t qti_nandc_reg_read(struct mtd_info *mtd, uint32_t reg_addr,
					uint8_t flags)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_read_ptr = nandc->ce_read_array;
	uint32_t *buffer = nandc->reg_buffer;

	bam_add_cmd_element(cmd_list_read_ptr, reg_addr,
			   (uintptr_t)buffer, CE_READ_TYPE);

	/* Enqueue the desc for the above command */
	q_bam_add_one_desc(&nandc->bam,
			 CMD_PIPE_INDEX,
			 (uint8_t*)((uintptr_t)cmd_list_read_ptr),
			 BAM_CE_SIZE,
			 BAM_DESC_CMD_FLAG| BAM_DESC_INT_FLAG | flags);

	qti_nandc_wait_for_cmd_exec(nandc, 1);
#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((uintptr_t)nandc->reg_buffer,
			   (uintptr_t)nandc->reg_buffer +
					CONFIG_SYS_CACHELINE_SIZE);
#endif
	return *buffer;
}


static void multi_page_cmd_reg_reset(struct qcom_nand_controller *nandc,
			struct cmd_element *cmd_list_ptr, uint8_t flags)
{
	bam_add_cmd_element(cmd_list_ptr, NAND_MULTI_PAGE_CMD,
					(uint32_t)0, CE_WRITE_TYPE);

	/* Enqueue the desc for the above command */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr,
			BAM_CE_SIZE,  BAM_DESC_CMD_FLAG | BAM_DESC_INT_FLAG);

	qti_nandc_wait_for_cmd_exec(nandc, 1);
}

static void reset_addr_reg(struct qcom_nand_controller *nandc,
			struct cmd_element *cmd_list_ptr, uint8_t flags)
{
	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR0, (uint32_t)0,
			CE_WRITE_TYPE);

	/* Enqueue the desc for the above command */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr,
			BAM_CE_SIZE,  BAM_DESC_CMD_FLAG | BAM_DESC_INT_FLAG);

	qti_nandc_wait_for_cmd_exec(nandc, 1);
}

/*
 * Assume the BAM is in a locked state.
 */
void qti_nandc_erased_status_reset(struct qcom_nand_controller *nandc,
					struct cmd_element *cmd_list_ptr,
					uint8_t flags)
{
	uint32_t val = 0;

	/* Reset the Erased Codeword/Page detection controller. */
	val = NAND_ERASED_CW_DETECT_CFG_RESET_CTRL;

	bam_add_cmd_element(cmd_list_ptr,
					NAND_ERASED_CW_DETECT_CFG, val,
					CE_WRITE_TYPE);

	/* Enqueue the desc for the above command */
	q_bam_add_one_desc(&nandc->bam,
			 CMD_PIPE_INDEX,
			 (uint8_t*)cmd_list_ptr,
			 BAM_CE_SIZE,
			 BAM_DESC_CMD_FLAG | BAM_DESC_INT_FLAG | flags);

	qti_nandc_wait_for_cmd_exec(nandc, 1);

	/* Enable the Erased Codeword/Page detection
	 * controller to check the data as it arrives.
	 * Also disable ECC reporting for an erased CW.
	 */
	val = NAND_ERASED_CW_DETECT_CFG_ACTIVATE_CTRL |
		NAND_ERASED_CW_DETECT_ERASED_CW_ECC_MASK;

	bam_add_cmd_element(cmd_list_ptr,
					NAND_ERASED_CW_DETECT_CFG, val,
					CE_WRITE_TYPE);

	/* Enqueue the desc for the above command */
	q_bam_add_one_desc(&nandc->bam,
			 CMD_PIPE_INDEX,
			 (uint8_t*)cmd_list_ptr,
			 BAM_CE_SIZE,
			 BAM_DESC_CMD_FLAG | BAM_DESC_INT_FLAG |
			 BAM_DESC_UNLOCK_FLAG);

	qti_nandc_wait_for_cmd_exec(nandc, 1);
}

static int qti_nandc_check_read_status(struct mtd_info *mtd,
					struct read_stats *stats)
{
	uint32_t status = stats->flash_sts;

	/* Check for errors */
	if (!(status & NAND_FLASH_ERR)) {
		uint32_t corrected = stats->buffer_sts & NUM_ERRORS_MASK;
		mtd->ecc_stats.corrected += corrected;
		return corrected;
	}

	if (status & NAND_FLASH_MPU_ERR)
		return -EPERM;

	if (status & NAND_FLASH_TIMEOUT_ERR)
		return -ETIMEDOUT;

	if (stats->buffer_sts & NAND_BUFFER_UNCORRECTABLE) {
		/* Check if this is an ECC error on an erased page. */
		if ((stats->erased_cw_sts & NAND_CW_ERASED) != NAND_CW_ERASED)
			return -EBADMSG;

		return 0;
	}

	return -EIO;
}

static int qti_nandc_check_status(struct mtd_info *mtd, uint32_t status)
{
	/* Check for errors */
	if (status & NAND_FLASH_ERR) {
		printf("Nand Flash error Status = 0x%x\n", status);

		if (status & NAND_FLASH_MPU_ERR)
			return -EPERM;

		if (status & NAND_FLASH_TIMEOUT_ERR)
			return -ETIMEDOUT;

		return -EIO;
	}

	return 0;
}

static uint32_t qti_nandc_get_id(struct mtd_info *mtd)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	int num_desc = 0;
	uint32_t status;
	uint32_t id;
	uint32_t flash_cmd = NAND_CMD_FETCH_ID;
	uint32_t exec_cmd = 1;
	int nand_ret = NANDC_RESULT_SUCCESS;
	uint32_t vld = NAND_CMD_VALID_BASE;
	uint32_t cmd_vld = NAND_FLASH_DEV_CMD_VLD;


	flash_cmd |= QTI_SPI_WP_SET | QTI_SPI_HOLD_SET |
		QTI_SPI_TRANSFER_MODE_X1;
	vld = FLASH_DEV_CMD_VLD;

	/* Issue the Fetch id command to the NANDc */
	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD,
					(uint32_t)flash_cmd, CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr, cmd_vld, (uint32_t)vld,
			    CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Execute the cmd */
	bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD,
					(uint32_t)exec_cmd, CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Prepare the cmd desc for the above commands */
	q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr - \
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_LOCK_FLAG | BAM_DESC_INT_FLAG |
			BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG);

	/* Keep track of the number of desc added. */
	num_desc++;
	qti_nandc_wait_for_cmd_exec(nandc, num_desc);

	/* Read the status register */
	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, 0);

	/* Check for errors */
	nand_ret = qti_nandc_check_status(mtd, status);
	if (nand_ret) {
		printf("Read ID cmd status failed\n");
		goto qti_nandc_get_id_err;
	}

	/* Read the id */
	id = qti_nandc_reg_read(mtd, NAND_READ_ID, BAM_DESC_UNLOCK_FLAG);

	nandc->id = id;
	nandc->vendor = id & 0xff;
	nandc->device = (id >> 8) & 0xffff;
	nandc->data_buffers[0] = (uint8_t)nandc->vendor;
	nandc->data_buffers[1] = (uint8_t)nandc->device;
	nandc->data_buffers[2] = (uint8_t)(nandc->device >> 8);
	nandc->dev_cfg = (id >> 24) & 0xFF;
	nandc->widebus = 0;
	nandc->widebus &= (id >> 24) & 0xFF;
	nandc->widebus = nandc->widebus? 1: 0;

qti_nandc_get_id_err:
	return nand_ret;
}

static int qti_bam_init(struct qcom_nand_controller *nandc,
				struct qti_nand_init_config *config)
{
	uint32_t bam_ret = NANDC_RESULT_SUCCESS;

	nandc->bam.base = config->bam_base;
	/* Set Read pipe params. */
	nandc->bam.pipe[DATA_PRODUCER_PIPE_INDEX].pipe_num =
							config->pipes.read_pipe;
	/* System consumer */
	nandc->bam.pipe[DATA_PRODUCER_PIPE_INDEX].trans_type = BAM2SYS;
	nandc->bam.pipe[DATA_PRODUCER_PIPE_INDEX].fifo.size =
					QTI_BAM_DATA_FIFO_SIZE;
	nandc->bam.pipe[DATA_PRODUCER_PIPE_INDEX].fifo.head =
					nandc->qti_data_desc_fifo;
	nandc->bam.pipe[DATA_PRODUCER_PIPE_INDEX].lock_grp =
					config->pipes.read_pipe_grp;

	/* Set Write pipe params. */
	nandc->bam.pipe[DATA_CONSUMER_PIPE_INDEX].pipe_num =
					config->pipes.write_pipe;
	/* System producer */
	nandc->bam.pipe[DATA_CONSUMER_PIPE_INDEX].trans_type = SYS2BAM;
	nandc->bam.pipe[DATA_CONSUMER_PIPE_INDEX].fifo.size =
					QTI_BAM_DATA_FIFO_SIZE;
	nandc->bam.pipe[DATA_CONSUMER_PIPE_INDEX].fifo.head =
					nandc->qti_data_desc_fifo;
	nandc->bam.pipe[DATA_CONSUMER_PIPE_INDEX].lock_grp =
					config->pipes.write_pipe_grp;

	/* Set Cmd pipe params. */
	nandc->bam.pipe[CMD_PIPE_INDEX].pipe_num = config->pipes.cmd_pipe;
	/* System consumer */
	nandc->bam.pipe[CMD_PIPE_INDEX].trans_type = SYS2BAM;
	nandc->bam.pipe[CMD_PIPE_INDEX].fifo.size = QTI_BAM_CMD_FIFO_SIZE;
	nandc->bam.pipe[CMD_PIPE_INDEX].fifo.head = nandc->qti_cmd_desc_fifo;
	nandc->bam.pipe[CMD_PIPE_INDEX].lock_grp = config->pipes.cmd_pipe_grp;

	/* Set Status pipe params. */
	nandc->bam.pipe[BAM_STATUS_PIPE_INDEX].pipe_num =
					config->pipes.status_pipe;
	/* System consumer */
	nandc->bam.pipe[BAM_STATUS_PIPE_INDEX].trans_type = BAM2SYS;
	nandc->bam.pipe[BAM_STATUS_PIPE_INDEX].fifo.size =
					QTI_BAM_STATUS_FIFO_SIZE;
	nandc->bam.pipe[BAM_STATUS_PIPE_INDEX].fifo.head =
					nandc->qti_status_desc_fifo;
	nandc->bam.pipe[BAM_STATUS_PIPE_INDEX].lock_grp =
					config->pipes.status_pipe_grp;

	/* Programs the threshold for BAM transfer
	 * When this threshold is reached, BAM signals the peripheral via the
	 * pipe_bytes_available interface.
	 * The peripheral is signalled with this notification
	 * in the following cases:
	 * a.  It has accumulated all the descriptors.
	 * b.  It has accumulated more than threshold bytes.
	 * c.  It has reached EOT (End Of Transfer).
	 * Note: this value needs to be set by the h/w folks and is specific
	 * for each peripheral.
	*/
	nandc->bam.threshold = 32;

	/* Set the EE.  */
	nandc->bam.ee = config->ee;

	/* Set the max desc length for this BAM. */
	nandc->bam.max_desc_len = config->max_desc_len;

	/* BAM Init. */
	bam_init(&nandc->bam, nandc->varient_info->bam_cfg,
				nandc->varient_info->bam_threshold_reg_write);

	/* Initialize BAM QTI read pipe */
	bam_sys_pipe_init(&nandc->bam, DATA_PRODUCER_PIPE_INDEX);

	/* Init read fifo */
	bam_ret = bam_pipe_fifo_init(&nandc->bam,
			nandc->bam.pipe[DATA_PRODUCER_PIPE_INDEX].pipe_num);

	if (bam_ret) {
		printf("QTI:NANDc BAM Read FIFO init error\n");
		bam_ret = NANDC_RESULT_FAILURE;
		goto qti_nand_bam_init_error;
	}

	/* Initialize BAM QTI write pipe */
	bam_sys_pipe_init(&nandc->bam, DATA_CONSUMER_PIPE_INDEX);

	/* Init write fifo. Use the same fifo as read fifo. */
	bam_ret = bam_pipe_fifo_init(&nandc->bam,
			nandc->bam.pipe[DATA_CONSUMER_PIPE_INDEX].pipe_num);

	if (bam_ret) {
		printf("QTI: NANDc: BAM Write FIFO init error\n");
		bam_ret = NANDC_RESULT_FAILURE;
		goto qti_nand_bam_init_error;
	}

	/* Initialize BAM QTI cmd pipe */
	bam_sys_pipe_init(&nandc->bam, CMD_PIPE_INDEX);

	/* Init cmd fifo */
	bam_ret = bam_pipe_fifo_init(&nandc->bam,
				nandc->bam.pipe[CMD_PIPE_INDEX].pipe_num);

	if (bam_ret) {
		printf("QTI:NANDc BAM CMD FIFO init error\n");
		bam_ret = NANDC_RESULT_FAILURE;
		goto qti_nand_bam_init_error;
	}

	/* Initialize BAM QTI status pipe */
	bam_sys_pipe_init(&nandc->bam, BAM_STATUS_PIPE_INDEX);

	/* Init status fifo */
	bam_ret = bam_pipe_fifo_init(&nandc->bam,
			nandc->bam.pipe[BAM_STATUS_PIPE_INDEX].pipe_num);

	if (bam_ret) {
		printf("QTI:NANDc BAM STATUS FIFO init error\n");
		bam_ret = NANDC_RESULT_FAILURE;
		goto qti_nand_bam_init_error;
	}

	/*
	 * Once BAM_MODE_EN bit is set then QTI_NAND_CTRL register
	 * should be written with BAM instead of writel.
	 * Check if BAM_MODE_EN is already set by bootloader and write only
	 * if this bit is not set.
	 */
	if (!(readl(QTI_NAND_CTRL) & BAM_MODE_EN)) {

		writel(BAM_MODE_EN | NANDC_READ_DELAY_COUNTER_VAL,
					(uintptr_t)QTI_NAND_CTRL);
	}

qti_nand_bam_init_error:
return bam_ret;
}

/* Adds command elements for addr and cfg register writes.
 * cfg: Defines the configuration for the flash cmd.
 * start: Address where the command elements are added.
 *
 * Returns the address where the next cmd element can be added.
 */
struct cmd_element*
qti_nand_add_addr_n_cfg_ce(struct qcom_nand_controller *nandc,
						struct cfg_params *cfg,
						struct cmd_element *start)
{
	struct cmd_element *cmd_list_ptr = start;

	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR0,
					(uint32_t)cfg->addr0, CE_WRITE_TYPE);
	cmd_list_ptr++;
	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR1,
					(uint32_t)cfg->addr1, CE_WRITE_TYPE);
	cmd_list_ptr++;
	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_CFG0,
					(uint32_t)cfg->cfg0, CE_WRITE_TYPE);
	cmd_list_ptr++;
	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_CFG1,
					(uint32_t)cfg->cfg1, CE_WRITE_TYPE);
	cmd_list_ptr++;

	return cmd_list_ptr;
}

static void qti_serial_update_dev_params(struct mtd_info *mtd)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);

	nandc->page_size = mtd->writesize;
	nandc->block_size = mtd->erasesize;
	nandc->num_blocks = chip->chipsize / mtd->erasesize;
	nandc->widebus = 0x0;
	nandc->density = chip->chipsize;
	nandc->spare_size = mtd->oobsize;
	nandc->num_pages_per_blk = mtd->erasesize/mtd->writesize;
	nandc->num_pages_per_blk_mask = nandc->num_pages_per_blk - 1;
	nandc->timing_mode_support = chip->onfi_timing_mode_default;
	mtd->ecc_strength = chip->ecc_strength_ds;
	mtd->bitflip_threshold = DIV_ROUND_UP(mtd->ecc_strength * 3, 4);
	mtd->size = chip->chipsize;
	nandc->check_quad_config = (chip->options & CHECK_QUAD_CONFIG);
	nandc->quad_mode = (chip->options & QUAD_MODE) ? true : false;

	printf("ID = %x\n", nandc->id);
	printf("Vendor = %x\n", nandc->vendor);
	printf("Device = %x\n", nandc->device);
	printf("Serial NAND device Manufacturer:%s\n", mtd->name);
	printf("Device Size:%d MiB, Page size:%d, Spare Size:%d, ECC:%d-bit\n",
		(int)(nandc->density >> 20),
		nandc->page_size, mtd->oobsize, mtd->ecc_strength);
}

static int qti_nand_save_config(struct mtd_info *mtd)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);
	uint32_t qti_oob_size;
	uint32_t no_of_address_cycle = 5;
	uint32_t disable_status_after_write = 0;
	uint32_t recovery_cycle = 7;
	uint32_t wr_rd_busy_gap = 2;

	/* Save Configurations */
	nandc->cws_per_page = nandc->page_size >> NAND_CW_DIV_RIGHT_SHIFT;

	/*
	 * Verify that we have enough buffer to handle all the cws in a page.
	 */
	if (!(nandc->cws_per_page <= QTI_NAND_MAX_CWS_IN_PAGE)) {
		printf("Not enough buffer to handle CW\n");
		return -EINVAL;
	}

	/* Codeword Size = UD_SIZE_BYTES + ECC_PARITY_SIZE_BYTES
	 *                          + SPARE_SIZE_BYTES + Bad Block size
	 */
	if (mtd->ecc_strength == 8) {
		nandc->cw_size = NAND_CW_SIZE_8_BIT_ECC;
		/* Use 8-bit ecc */
		nandc->ecc_bch_cfg |= (1 << NAND_DEV0_ECC_MODE_SHIFT);

		if (nandc->widebus) {
			nandc->ecc_bytes_hw = 14;
			nandc->spare_bytes = 0;
			nandc->bbm_size = 2;
		} else {
			nandc->ecc_bytes_hw = 13;
			nandc->spare_bytes = 2;
			nandc->bbm_size = 1;
		}
	} else {
		nandc->cw_size = NAND_CW_SIZE_4_BIT_ECC;
		if (nandc->widebus) {
			nandc->ecc_bytes_hw = 8;
			nandc->spare_bytes = 2;
			nandc->bbm_size = 2;
		} else {
			nandc->ecc_bytes_hw = 7;
			nandc->spare_bytes = 4;
			nandc->bbm_size = 1;
		}
	}

	/* spare size bytes in each CW */
	nandc->cfg0 |= nandc->spare_bytes <<
			NAND_DEV0_CFG0_SPARE_SZ_BYTES_SHIFT;
	/* parity bytes in each CW */
	nandc->ecc_bch_cfg |=
		nandc->ecc_bytes_hw << NAND_DEV0_ECC_PARITY_SZ_BYTES_SHIFT;

	qti_oob_size = nandc->cw_size * nandc->cws_per_page - mtd->writesize;

	if (mtd->oobsize < qti_oob_size) {
		printf("%s: ecc data doesn't fit in available OOB area\n",
			__func__);
		return -EINVAL;
	}

	if (mtd->oobsize > qti_oob_size)
		printf("qti_nand: changing oobsize to %d from %d bytes\n",
			qti_oob_size,  mtd->oobsize);

	/* Make the device OOB size as QTI supported OOB size. */
	mtd->oobsize = qti_oob_size;
	mtd->oobavail = (DATA_BYTES_IN_IMG_PER_CW - USER_DATA_BYTES_PER_CW) *
				nandc->cws_per_page;
	nandc->oob_per_page = mtd->oobavail;
	mtd->writebufsize =  mtd->writesize;
	/* BAD_BLOCK_BYTE_NUM = Page Size - (CW_PER_PAGE * Codeword Size) + 1
	 * Note: Set CW_PER_PAGE to 1 less than the actual number.
	 */
	nandc->bad_blk_loc = nandc->page_size - nandc->cw_size *
				 (nandc->cws_per_page - 1) + 1;

	no_of_address_cycle = 3;
	disable_status_after_write = 1;
	recovery_cycle = 0;
	wr_rd_busy_gap = 20;

	nandc->cfg0 |= ((nandc->cws_per_page - 1) <<
			NAND_DEV0_CFG0_CW_PER_PAGE_SHIFT)
			/* 4/8 cw/pg for 2/4k */
			|(DATA_BYTES_IN_IMG_PER_CW <<
			NAND_DEV0_CFG0_UD_SIZE_BYTES_SHIFT)
			/* 516 user data bytes */
			|(no_of_address_cycle <<
			NAND_DEV0_CFG0_ADDR_CYCLE_SHIFT)
			/* 5 address cycles */
			|(disable_status_after_write <<
			NAND_DEV0_CFG0_DIS_STS_AFTER_WR_SHIFT);
			/* Send read status cmd after each write. */

	nandc->cfg1 |= (recovery_cycle <<
			NAND_DEV0_CFG1_RECOVERY_CYCLES_SHIFT)
			/* 8 recovery cycles */
			|(0 <<NAND_DEV0_CFG1_CS_ACTIVE_BSY_SHIFT)
			/* Allow CS deassertion */
			|(nandc->bad_blk_loc <<
			NAND_DEV0_CFG1_BAD_BLK_BYTE_NUM_SHIFT)
			/* Bad block marker location */
			|(0 << NAND_DEV0_CFG1_BAD_BLK_IN_SPARE_SHIFT)
			/* Bad block in user data area */
			|(wr_rd_busy_gap <<
			NAND_DEV0_CFG1_WR_RD_BSY_GAP_SHIFT)
			/* 8 cycle tWB/tRB */
			|(nandc->widebus <<
			NAND_DEV0_CFG1_WIDE_BUS_SHIFT);
			/* preserve wide flash flag */

	nandc->cfg0_raw = ((nandc->cws_per_page- 1) <<
			NAND_DEV0_CFG0_CW_PER_PAGE_SHIFT)
			|(no_of_address_cycle <<
			NAND_DEV0_CFG0_ADDR_CYCLE_SHIFT)
			|(nandc->cw_size <<
			NAND_DEV0_CFG0_UD_SIZE_BYTES_SHIFT)
			//figure out the size of cw
			| (disable_status_after_write <<
			NAND_DEV0_CFG0_DIS_STS_AFTER_WR_SHIFT);

	nandc->cfg1_raw = (recovery_cycle <<
			NAND_DEV0_CFG1_RECOVERY_CYCLES_SHIFT)
			| (0 <<  NAND_DEV0_CFG1_CS_ACTIVE_BSY_SHIFT)
			| (17 <<  NAND_DEV0_CFG1_BAD_BLK_BYTE_NUM_SHIFT)
			| (1 << NAND_DEV0_CFG1_BAD_BLK_IN_SPARE_SHIFT)
			| (wr_rd_busy_gap << NAND_DEV0_CFG1_WR_RD_BSY_GAP_SHIFT)
			| (nandc->widebus << NAND_DEV0_CFG1_WIDE_BUS_SHIFT)
			| 1 ;
		/* to disable reed solomon ecc..this feild is now read only. */

	nandc->ecc_bch_cfg |= (0 << NAND_DEV0_ECC_DISABLE_SHIFT)
			/* Enable ECC */
			| (0 << NAND_DEV0_ECC_SW_RESET_SHIFT)
			/* Put ECC core in op mode */
			| (DATA_BYTES_IN_IMG_PER_CW <<
			NAND_DEV0_ECC_NUM_DATA_BYTES)
			| (1 << NAND_DEV0_ECC_FORCE_CLK_OPEN_SHIFT);
			/* Enable all clocks */

	/*
	 * Safe to use a single instance global variable,
	 * fake_ecc_layout, since we will be called only once for the
	 * lifetime of the driver. We can be called more than once,
	 * but the previous instance of the driver would have been
	 * deinit before the next one is inited.
	 */
	memset(&nandc->fake_ecc_layout, 0, sizeof(nandc->fake_ecc_layout));
	chip->ecc.layout = &nandc->fake_ecc_layout;

	return 0;
}


static int qti_serial_get_feature(struct mtd_info *mtd, uint32_t ftr_addr)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	uint8_t num_desc = 0;
	uint32_t status, nand_ret;
	uint32_t exec_cmd = 1;

	uint32_t cmd_val = (QTI_SPI_TRANSFER_MODE_X1 | QTI_SPI_HOLD_SET |
			QTI_SPI_WP_SET | NAND_CMD_ACC_FEATURE);

	/* Set the feature address to NAND_ADDR0 register */
	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR0, ftr_addr,
			CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Set the value 0x0 to NAND_ADDR1 register */
	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR1, 0,
			CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* First Clear the feature register to get the fresh feature value */
	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_FEATURES, 0,
			    CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* cmd_val = 0x3800000E
	 * bit-31 is clear means set feature
	 * bit-30-29 means x1 mode
	 * bit-28 is set , this is for wp pin
	 * wp# pin should be set to high then only we can get the feature
	 * bit-27 SPI_HOLD : this pin also should be high
	 */
	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD, cmd_val,
			CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Execute the cmd */
	bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD, exec_cmd,
			    CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Prepare the cmd desc for the above commands */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
			(uint8_t *)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG |
			BAM_DESC_INT_FLAG);

	/* Keep track of the number of desc added. */
	num_desc++;

	qti_nandc_wait_for_cmd_exec(nandc, num_desc);

	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, 0);

	/* Check for errors */
	nand_ret = qti_nandc_check_status(mtd, status);
	if (nand_ret) {
		printf("%s : CMD status failed\n", __func__);
		goto err;
	}
	/* read the feature register value and update in feature
	 * Feature value will get updated in [15:8]
	 */
	nand_ret = qti_nandc_reg_read(mtd, NAND_FLASH_FEATURES, 0);

	qspi_debug("NAND Feature Register Addr:0x%02x and Val:0x%08x\n",
			ftr_addr,nand_ret);
err:
	return nand_ret;

}

static int qti_set_feature(struct mtd_info *mtd, uint32_t ftr_addr,
	       uint32_t ftr_val)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	uint8_t num_desc = 0;
	uint32_t status, nand_ret;

	uint32_t cmd_val = (QTI_SPI_SET_FEATURE | QTI_SPI_WP_SET |
			QTI_SPI_HOLD_SET | QTI_SPI_TRANSFER_MODE_X1 |
			NAND_CMD_ACC_FEATURE);

	uint32_t exec_cmd = 1;

	/* set the feature value to NAND_FLASH_FEATURES feature register */
	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_FEATURES,
					ftr_val, CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Set the feature address to NAND_ADDR0 register */
	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR0, ftr_addr,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Set the value 0x0 to NAND_ADDR1 register */
	bam_add_cmd_element(cmd_list_ptr, NAND_ADDR1, 0,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* cmd_val = 0xB800000E
	 * bit-31 is set means set feature
	 * bit-30-29 means x1 mode
	 * bit-28 is set , this is for wp pin
	 * wp# pin should be set to high then only we can set the feature
	 * bit-27 SPI_HOLD : this pin also should be high
	 */
	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD, cmd_val,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Execute the cmd */
	bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD, exec_cmd,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Prepare the cmd desc for the above commands */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
			(uint8_t *)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG |
			BAM_DESC_INT_FLAG);

	/* Keep track of the number of desc added. */
	num_desc++;

	qti_nandc_wait_for_cmd_exec(nandc, num_desc);

	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, 0);

	/* Check for errors */
	nand_ret = qti_nandc_check_status(mtd, status);
	if (nand_ret) {
		printf("%s : CMD status failed\n", __func__);
		goto err;
	}
err:
	return nand_ret;
}

int qti_spi_nand_config(struct mtd_info *mtd)
{
	uint32_t status = 0x0;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	uint32_t cmd3_val = NAND_FLASH_DEV_CMD3_VAL;

	/* For micron device the READ_CACHE_SEQ command is different than
	 * Giga device. for Giga 0x31 and for Micron 0x30.
	 * so based on id update the command configuration register
	 * CMD3.
	 */
	if (nandc->id == MICRON_DEVICE_ID) {
		cmd3_val = (NAND_FLASH_DEV_CMD3_VAL & CMD3_MASK);
		writel(cmd3_val, (uintptr_t)SPI_NAND_DEV_CMD3);
	}

	/* Get the block protection status*/
	status = qti_serial_get_feature(mtd, FLASH_SPI_NAND_BLK_PROCT_ADDR);
	if (status < 0) {
		printf("%s : Error in getting feature.\n",__func__);
		return status;
	}

	if ((status >> 8) & FLASH_SPI_NAND_BLK_PROCT_ENABLE) {
		qspi_debug("%s: Block protection is enabled\n",__func__);
		qspi_debug("%s: Issuing set feature command to disable it.\n",
				__func__);

		status  = qti_set_feature(mtd, FLASH_SPI_NAND_BLK_PROCT_ADDR,
				FLASH_SPI_NAND_BLK_PROCT_DISABLE);
		if (status < 0) {
			printf("%s : Error in disabling block protection.\n",
				__func__);
			return status;
		}
		/* After disabling the block protection again read the status
		 * i.e again call the get feature command to get the status
		 */
		status = qti_serial_get_feature(mtd,
					FLASH_SPI_NAND_BLK_PROCT_ADDR);
		if (status < 0) {
			printf("%s : Error in getting feature.\n",__func__);
			return status;
		}
		if ((status >> 8) & FLASH_SPI_NAND_BLK_PROCT_ENABLE) {
			printf("%s : block protection still enabled.We \
					can't erase a block\n",	__func__);
			return -QTI_SERIAL_ERROR;
		} else
			qspi_debug("%s : Block protection Disabled.\n",
								__func__);
	} else
		qspi_debug("%s: Block protection Disabled on Power on.\n",
								__func__);

	/* Get Internal ECC status */
	status = qti_serial_get_feature(mtd, FLASH_SPI_NAND_FR_ADDR);
	if (status < 0) {
		printf("%s : Error in getting feature.\n",__func__);
		return status;
	}

	if ((status  >> 8) & FLASH_SPI_NAND_FR_ECC_ENABLE) {
		qspi_debug("%s : Internal ECC enabled, disabling \
						internal ECC\n",__func__);

		status >>= 8;
		status &= ~(FLASH_SPI_NAND_FR_ECC_ENABLE);
		status = qti_set_feature(mtd, FLASH_SPI_NAND_FR_ADDR,
			status);

		if (status < 0) {
			printf("%s : Error in disabling internal \
							ECC.\n",__func__);
			return status;
		}
		/* again check internal ECC is disabled or
		 * not using get feature command
		 */
		status = qti_serial_get_feature(mtd, FLASH_SPI_NAND_FR_ADDR);
		if (status < 0) {
			printf("%s : Error in getting feature.\n",__func__);
			return status;
		}

		if ((status  >> 8) & FLASH_SPI_NAND_FR_ECC_ENABLE) {
			printf("%s: Failed to disabled device internal ECC\n",
					__func__);
			return -QTI_SERIAL_ERROR;
		} else
			qspi_debug("%s : Internal ECC disabled.\n",__func__);
	} else
		qspi_debug("%s : Internal ECC disabled on power on.\n",
								__func__);

	if (nandc->vendor == WINBOND_MFR_ID) {
		status = qti_serial_get_feature(mtd, FLASH_SPI_NAND_FR_ADDR);
		if (status < 0) {
			printf("%s : Error in getting feature.\n",__func__);
			return status;
		}

		if (!((status >> 8) & FLASH_SPI_NAND_FR_BUFF_ENABLE)) {
			qspi_debug("%s :continous buffer mode disabled\n",
				__func__);
			qspi_debug("%s : Issuing set feature command \
					to enable it\n", __func__);
			status = qti_set_feature(mtd, FLASH_SPI_NAND_FR_ADDR,
				(FLASH_SPI_NAND_FR_BUFF_ENABLE |
				(status >> 8)));
			if (status < 0) {
				printf("%s : Error in disabling continous \
						buffer bit.\n",	__func__);
				return status;
			}
		} else {
			qspi_debug("%s : continous buffer mode enabled on \
						power on\n", __func__);
		}
	}
	/* Enable QUAD mode if device supported. Check this condition only
	 * if nandc->quad_mode = true , means device will support Quad mode
	 * else no need to check for Quad mode.
	 * For Micron device there is no quad config bit so no need to check
	 * quad config bit.
	 */
	/* Get QUAD bit status */
	if (nandc->check_quad_config == 1) {

		status = qti_serial_get_feature(mtd, FLASH_SPI_NAND_FR_ADDR);
		if (status < 0) {
			printf("%s : Error in getting feature.\n",__func__);
			return status;
		}

		if (!((status >> 8) & FLASH_SPI_NAND_FR_QUAD_ENABLE)) {
			qspi_debug("%s : Quad bit not enabled.\n",__func__);
			qspi_debug("%s : Issuning set feature command to \
						enable it.\n", __func__);
			/* Enable quad bit */
			status = qti_set_feature(mtd, FLASH_SPI_NAND_FR_ADDR,
			FLASH_SPI_NAND_FR_QUAD_ENABLE);
			if (status < 0) {
				printf("%s : Error in enabling Quad bit.\n",
								__func__);
				return status;
			}

			/* Read status again to know wether Quad bit
			 * enabled or not */
			status = qti_serial_get_feature(mtd,
							FLASH_SPI_NAND_FR_ADDR);
				if (status < 0) {
				printf("%s : Error in getting feature.\n",
								__func__);
				return status;
			}

			if (!((status >> 8) & FLASH_SPI_NAND_FR_QUAD_ENABLE)) {
				qspi_debug("%s:Quad mode not enabled,so use \
						x1 Mode.\n", __func__);
				nandc->quad_mode = false;
			} else {
				nandc->quad_mode = true;
				qspi_debug("%s: Quad mode enabled. using X4 \
							mode\n",__func__);
			}
		} else {
			nandc->quad_mode = true;
			qspi_debug("%s: Quad mode enabled on Opwer on.\n",
								__func__);
		}
	}

	return 0;
}

static void qti_spi_init(struct mtd_info *mtd)
{
	uint32_t xfer_start = NAND_XFR_STEPS_V1_5_20;
	int i;
	unsigned int default_clk_rate;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	int num_desc = 0;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	unsigned int val;

	val = readl(NAND_QSPI_MSTR_CONFIG);

	default_clk_rate = DEFAULT_CLK_200_MHZ;
	val |= FB_CLK_BIT;
	if ((readl(QTI_NAND_CTRL) & BAM_MODE_EN)) {

		bam_add_cmd_element(cmd_list_ptr,
						NAND_QSPI_MSTR_CONFIG,
						(uint32_t)val, CE_WRITE_TYPE);
		cmd_list_ptr++;

		bam_add_cmd_element(cmd_list_ptr,
						NAND_FLASH_SPI_CFG,
						(uint32_t)0, CE_WRITE_TYPE);
		cmd_list_ptr++;

		bam_add_cmd_element(cmd_list_ptr,
						NAND_FLASH_SPI_CFG,
						(uint32_t)SPI_CFG_VAL,
						CE_WRITE_TYPE);
		cmd_list_ptr++;

		val = SPI_CFG_VAL & ~SPI_LOAD_CLK_CNTR_INIT_EN;
		bam_add_cmd_element(cmd_list_ptr,
						NAND_FLASH_SPI_CFG,
						(uint32_t)val, CE_WRITE_TYPE);
		cmd_list_ptr++;

		q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_CMD_FLAG);
		num_desc++;

		/* Notify BAM HW about the newly added descriptors */
		q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, num_desc);
	} else {
		writel(val, (uintptr_t)NAND_QSPI_MSTR_CONFIG);
		writel(0x0, (uintptr_t)NAND_FLASH_SPI_CFG);
		writel(SPI_CFG_VAL, (uintptr_t)NAND_FLASH_SPI_CFG);
		val = SPI_CFG_VAL & ~SPI_LOAD_CLK_CNTR_INIT_EN;
		writel(val, (uintptr_t)NAND_FLASH_SPI_CFG);
	}

	num_desc = 0;

	/* According to HPG Setting Xfer steps and spi_num_addr_cycles
	 * is part of initialization flow before reset.However these
	 * values differ from NAND part to part.sitting in QTI layer
	 * we won't know which NAND we don't know which NAND is connected.
	 * So we are not following HPG init sequence.Instead we reset and
	 * read id of NAND,then based on NAND ID we get Xfer steps
	 * and spi_num_addr_cycles and configure them in this function.Since
	 * Xfer steps and spi_num_addr_cycles are required for read/write/erase
	 * functionality.
	 *
	 * NOTE: For now address cycle is same for Giga devices & Micron devices
	 * so we can configure no of addess cycle here only
	 * The NAND_FLASH_XFR_STEP register also fixed for both the devices so we
	 * can configure this register here only . later change this logic as per
	 * device
	 *
	 * NOTE: The XFER register value is now fixed as HPG.
	 *
	 */
	for (i = 0; i < QTI_NUM_XFER_STEPS; i++)
		writel(nandc->qti_onfi_mode_to_xfer_steps[0][i],
		       (uintptr_t)xfer_start + 4 * i);

	writel(NAND_FLASH_DEV_CMD0_VAL, (uintptr_t)SPI_NAND_DEV_CMD0);
	writel(NAND_FLASH_DEV_CMD1_VAL, (uintptr_t)SPI_NAND_DEV_CMD1);
	writel(NAND_FLASH_DEV_CMD2_VAL, (uintptr_t)SPI_NAND_DEV_CMD2);
	writel(NAND_FLASH_DEV_CMD3_VAL, (uintptr_t)SPI_NAND_DEV_CMD3);
	writel(NAND_FLASH_DEV_CMD7_VAL, (uintptr_t)SPI_NAND_DEV_CMD7);

	/* NAND_DEV_CMD8 & NAND_DEV_CMD9 default value will be used for
	 * QSPI
	 */
	writel(FLASH_DEV_CMD_VLD, (uintptr_t)NAND_FLASH_DEV_CMD_VLD);

	/* No of address cycle is same for Giga device & Micron so
	 * configure no of address cycle now.
	 */
	if ((readl(QTI_NAND_CTRL) & BAM_MODE_EN)) {
		cmd_list_ptr = nandc->ce_array;
		bam_add_cmd_element(cmd_list_ptr,
						NAND_SPI_NUM_ADDR_CYCLES,
						(uint32_t)SPI_NUM_ADDR_CYCLES,
						CE_WRITE_TYPE);

		cmd_list_ptr++;

		bam_add_cmd_element(cmd_list_ptr,
					NAND_SPI_BUSY_CHECK_WAIT_CNT,
					(uint32_t)SPI_BUSY_CHECK_WAIT_CNT,
					CE_WRITE_TYPE);

		cmd_list_ptr++;

		q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_CMD_FLAG);
		num_desc++;

		/* Notify BAM HW about the newly added descriptors */
		q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, num_desc);
	} else {
		writel(SPI_NUM_ADDR_CYCLES,
				(uintptr_t)NAND_SPI_NUM_ADDR_CYCLES);
		writel(SPI_BUSY_CHECK_WAIT_CNT,
				(uintptr_t)NAND_SPI_BUSY_CHECK_WAIT_CNT);
	}
}

static int reset(struct mtd_info *mtd)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	uint8_t num_desc = 0;
	uint32_t status, nand_ret;
	uint32_t exec_cmd = 1;
	uint32_t flash_cmd = NAND_CMD_RESET_DEVICE;


	flash_cmd |= (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET |
					QTI_SPI_TRANSFER_MODE_X1);
	uint32_t cfg0 = SPI_NAND_DEVn_CFG0 & 0xff00f0;
	uint32_t cfg1 = SPI_NAND_DEVn_CFG1_RESET;
	uint32_t ecc_cfg = ((SPI_NAND_DEVn_ECC_CFG & 0x0f000002) | (1 << 0))
				& ~(1 << 1);
	/* As per HPG the reset sequence as follow
	 * NAND_DEV0_CFG0	0x005400D0 or 0x00540010
	 * NAND_DEVn_CFG1	0x087476B1
	 * NAND_DEV0_ECC_CFG	0x02000001
	 * NAND_FLASH_CMD 	0x3800000D
	 * NAND_EXEC_CMD	0x00000001
	 */
	/* write the reset sequence as per HPG */
	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_CFG0,
					(uint32_t)cfg0, CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_CFG1,
					(uint32_t)cfg1, CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_ECC_CFG,
					(uint32_t)ecc_cfg, CE_WRITE_TYPE);
	cmd_list_ptr++;


	/* Issue the Reset device command to the NANDc */
	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD,
					(uint32_t)flash_cmd, CE_WRITE_TYPE);
	cmd_list_ptr++;
	/* Execute the cmd */
	bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD,
					(uint32_t)exec_cmd, CE_WRITE_TYPE);
	cmd_list_ptr++;
	/* Prepare the cmd desc for the above commands */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
			(uint8_t *)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG |
			BAM_DESC_INT_FLAG);
	/* Keep track of the number of desc added. */
	num_desc++;
	qti_nandc_wait_for_cmd_exec(nandc, num_desc);
	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, 0);

	/* Check for errors */
	nand_ret = qti_nandc_check_status(mtd, status);
	if (nand_ret) {
		printf("Reset cmd status failed\n");
		return nand_ret;
	}

	return nand_ret;
}

/* Enquues a desc for a flash cmd with NWD flag set:
 * cfg: Defines the configuration for the flash cmd.
 * start: Address where the command elements are added.
 *
 * Returns the address where the next cmd element can be added.
 */
struct cmd_element*
qti_nand_add_cmd_ce(struct qcom_nand_controller *nandc, struct cfg_params *cfg,
			struct cmd_element *start)
{
	struct cmd_element *cmd_list_ptr;

	cmd_list_ptr = qti_nand_add_addr_n_cfg_ce(nandc, cfg, start);

	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD,
					(uint32_t)cfg->cmd, CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD,
					(uint32_t)cfg->exec, CE_WRITE_TYPE);
	cmd_list_ptr++;

	return cmd_list_ptr;
}

/* Reads nand_flash_status */
struct cmd_element*
qti_nand_add_read_ce(struct qcom_nand_controller *nandc,
			struct cmd_element *start, uintptr_t *flash_status_read)
{
	struct cmd_element *cmd_list_ptr = start;

	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_STATUS,
				(uintptr_t)flash_status_read,
				CE_READ_TYPE);
	cmd_list_ptr++;

	return cmd_list_ptr;
}

/* Resets nand_flash_status and nand_read_status */
struct cmd_element*
reset_status_ce(struct qcom_nand_controller *nandc, struct cmd_element *start,
		uint32_t read_status)
{
	struct cmd_element *cmd_list_ptr = start;
	uint32_t flash_status_reset;
	uint32_t read_status_reset;

	/* Read and reset the status registers. */
	flash_status_reset = NAND_FLASH_STATUS_RESET;
	read_status_reset = NAND_READ_STATUS_RESET;

	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_STATUS,
			   (uint32_t)flash_status_reset, CE_WRITE_TYPE);
	cmd_list_ptr++;

	if (read_status) {
		bam_add_cmd_element(cmd_list_ptr, NAND_READ_STATUS,
				   (uint32_t)read_status_reset, CE_WRITE_TYPE);
		cmd_list_ptr++;
	}

	return cmd_list_ptr;
}

struct cmd_element*
qti_nand_add_isbad_cmd_ce(struct qcom_nand_controller *nandc,
				struct cfg_params *cfg,
				struct cmd_element *start)
{
	struct cmd_element *cmd_list_ptr = start;

	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_ECC_CFG,
			   (uint32_t)cfg->ecc_cfg, CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(0),
					(uint32_t)cfg->addr_loc_0,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	cmd_list_ptr = qti_nand_add_cmd_ce(nandc, cfg, cmd_list_ptr);

	return cmd_list_ptr;
}

static int
qti_nandc_block_isbad_exec(struct mtd_info *mtd,
			   struct cfg_params *params,
			   uint8_t *bad_block)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	uint8_t desc_flags = BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG
				| BAM_DESC_LOCK_FLAG | BAM_DESC_INT_FLAG;
	int num_desc = 0;
	uint32_t status = 0;
	int nand_ret = NANDC_RESULT_SUCCESS;

	cmd_list_ptr = qti_nand_add_isbad_cmd_ce(nandc, params, cmd_list_ptr);

	/* Enqueue the desc for the above commands */
	q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			desc_flags);

	num_desc++;

	/* Add Data desc */
	bam_add_desc(&nandc->bam,
		     DATA_PRODUCER_PIPE_INDEX,
		     (uint8_t *)((uintptr_t)bad_block),
		     4,
		     BAM_DESC_INT_FLAG);

	qti_nandc_wait_for_cmd_exec(nandc, num_desc);

	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, 0);

	nand_ret = qti_nandc_check_status(mtd, status);

	/* Dummy read to unlock pipe. */
	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS,
					BAM_DESC_UNLOCK_FLAG);

	if (nand_ret)
		return nand_ret;

	qti_nandc_wait_for_data(nandc, DATA_PRODUCER_PIPE_INDEX);

	return nand_ret;
}

/**
 * qti_nandc_block_isbad() - Checks is given block is bad
 * @page - number of page the block starts at
 *
 * Returns nand_result_t
 */
static int qti_nandc_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	unsigned cwperpage;
	struct cfg_params params;
	unsigned nand_ret = NANDC_RESULT_SUCCESS;
	uint32_t page;
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	uint8_t *bad_block = (uint8_t*)nandc->nandc_buffer;

	/* Check for invalid offset */
	if (offs > mtd->size)
		return -EINVAL;

	if (offs & (mtd->erasesize - 1))
		return -EINVAL;

	page = offs >> chip->page_shift;

	/* Read the bad block value from the flash.
	 * Bad block value is stored in the first page of the block.
	 */
	/* Read the first page in the block. */
	cwperpage = (nandc->cws_per_page);

	params.cmd = NAND_CMD_PAGE_READ_ECC;

	/* Read page cmd */

	params.cmd = NAND_CMD_PAGE_READ;
	if (nandc->quad_mode)
		params.cmd |= QTI_SPI_TRANSFER_MODE_X4;
	else
		params.cmd |= QTI_SPI_TRANSFER_MODE_X1;

	params.cmd |= (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET);

	/* Clear the CW per page bits */
	params.cfg0 = nandc->cfg0_raw & ~(7U <<
			 NAND_DEV0_CFG0_CW_PER_PAGE_SHIFT);
	params.cfg1 = nandc->cfg1_raw;

	/* addr0 - Write column addr + few bits in row addr upto 32 bits. */
	params.addr0 = ((page << 16) | (USER_DATA_BYTES_PER_CW  *
					 (cwperpage)));

	/* addr1 - Write rest of row addr.
	 * This will be all 0s.
	 */
	params.addr1 = (page >> 16) & 0xff;
	params.addr_loc_0 = NAND_RD_LOC_OFFSET(0);
	params.addr_loc_0 |= NAND_RD_LOC_LAST_BIT(1);
	params.addr_loc_0 |= NAND_RD_LOC_SIZE(4); /* Read 4 bytes */
	params.ecc_cfg = 0x1; /* Disable ECC */
	params.exec = 1;

	if (qti_nandc_block_isbad_exec(mtd, &params, bad_block)) {
		printf("Could not read bad block value\n");
		return NANDC_RESULT_FAILURE;
	}

#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((uintptr_t)nandc->nandc_buffer,
			   (uintptr_t)nandc->nandc_buffer +
					CONFIG_SYS_CACHELINE_SIZE);
#endif

	if (nandc->widebus) {
		if (bad_block[0] != 0xFF && bad_block[1] != 0xFF) {
			nand_ret = NANDC_RESULT_BAD_BLOCK;
		}
	} else if (bad_block[0] != 0xFF) {
		nand_ret = NANDC_RESULT_BAD_BLOCK;
	}
	return nand_ret;
}

/* Return num of desc added. */
static void
qti_nandc_add_wr_page_cws_cmd_desc(struct mtd_info *mtd, struct cfg_params *cfg,
				   uint32_t status[],
				   enum nand_cfg_value cfg_mode)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_read_ptr = nandc->ce_read_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	struct cmd_element *cmd_list_read_ptr_start = nandc->ce_read_array;
	uint32_t ecc;
	int num_desc = 0;
	int int_flag = 0;
	unsigned int i;

	/* For Serial NAND devices the page program sequence as
	 * 02H (PROGRAM LOAD)/32H (PROGRAM LOAD x4)
	 * 06H (WRITE ENABLE)
	 * 10H (PROGRAM EXECUTE)
	 * 0FH (GET FEATURE command to read the status)
	 * No need to 0x02 & 0x32 command manually, controller
	 * automatically send this command to device. we have already mapped
	 * these command in  QTI_FLASH_DEV_CMD9 register, similar for command
	 * 0x06 & 0x0F, controller will take care internally
	 *
	 * NOTE: While initializing we have already enabeld quad bit i.e QE-bit
	 * and disable write protection so no need to check here.
	 */
	if (nandc->quad_mode)
		cfg->cmd |= QTI_SPI_TRANSFER_MODE_X4;
	else
		cfg->cmd |= QTI_SPI_TRANSFER_MODE_X1;

	cfg->cmd |= (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET);


	if (cfg_mode == NAND_CFG) {
		ecc = nandc->ecc_bch_cfg;
	} else {
		ecc = 0x1; /* Disable ECC */
	}
	/* Add ECC configuration */
	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_ECC_CFG,
						(uint32_t)ecc, CE_WRITE_TYPE);
	cmd_list_ptr++;

	cmd_list_ptr = qti_nand_add_addr_n_cfg_ce(nandc, cfg, cmd_list_ptr);

	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD,
						(uint32_t)cfg->cmd,
						CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Enqueue the desc for the above commands */
	q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_CMD_FLAG | BAM_DESC_LOCK_FLAG);

	num_desc++;

	/* Add CE for all the CWs */
	for (i = 0; i < (nandc->cws_per_page); i++) {
		schedule();
		cmd_list_ptr_start = cmd_list_ptr;
		int_flag = BAM_DESC_INT_FLAG;

		bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD,
					(uint32_t)cfg->exec,
					CE_WRITE_TYPE);
		cmd_list_ptr++;

		/* Enqueue the desc for the above commands */
		q_bam_add_one_desc(&nandc->bam,
				CMD_PIPE_INDEX,
				(uint8_t*)cmd_list_ptr_start,
				((uintptr_t)cmd_list_ptr -
				(uintptr_t)cmd_list_ptr_start),
				BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG);

		num_desc++;
		cmd_list_ptr_start = cmd_list_ptr;
		cmd_list_read_ptr_start = cmd_list_read_ptr;

		cmd_list_read_ptr = qti_nand_add_read_ce(nandc,
							cmd_list_read_ptr_start,
							(uintptr_t*)&status[i]);
		/* Enqueue the desc for the NAND_FLASH_STATUS read command */
		q_bam_add_one_desc(&nandc->bam,
				 CMD_PIPE_INDEX,
				 (uint8_t*)cmd_list_read_ptr_start,
				 ((uintptr_t)cmd_list_read_ptr -
				 (uintptr_t)cmd_list_read_ptr_start),
				 BAM_DESC_CMD_FLAG);

		/* Set interrupt bit only for the last CW */
		if (i == (nandc->cws_per_page) - 1)
			cmd_list_ptr = reset_status_ce(nandc, cmd_list_ptr,
							1);
		else
			cmd_list_ptr = reset_status_ce(nandc, cmd_list_ptr,
							0);

		/* Enqueue the desc for NAND_FLASH_STATUS and NAND_READ_STATUS
		 * write commands */
		q_bam_add_one_desc(&nandc->bam,
				CMD_PIPE_INDEX,
				(uint8_t*)cmd_list_ptr_start,
				((uintptr_t)cmd_list_ptr -
				(uintptr_t)cmd_list_ptr_start),
				int_flag | BAM_DESC_CMD_FLAG |
				BAM_DESC_UNLOCK_FLAG);
		num_desc += 2;

		qti_nandc_wait_for_cmd_exec(nandc, num_desc);

#if !defined(CONFIG_SYS_DCACHE_OFF)
		flush_dcache_range((uintptr_t)nandc->nandc_buffer,
				   (uintptr_t)nandc->nandc_buffer +
						CONFIG_SYS_CACHELINE_SIZE);
#endif

		status[i] = qti_nandc_check_status(mtd, status[i]);

		num_desc = 0;
	}
	return;
}

void
qti_add_wr_page_cws_data_desc(struct mtd_info *mtd, const void *buffer,
			       enum nand_cfg_value cfg_mode,
			       const void *spareaddr)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	int len;
	int flags;
	uint64_t start;
	unsigned num_desc = 0;
	unsigned i;

	for( i = 0; i < (nandc->cws_per_page); i++) {
		flags = 0;

		/* Set the interrupt flag on the last CW write for the page. */
		if( i == (nandc->cws_per_page) - 1)
			flags |= BAM_DESC_INT_FLAG;

		if (cfg_mode != NAND_CFG_RAW) {
			start = (uintptr_t)buffer + i * DATA_BYTES_IN_IMG_PER_CW;
			if (i < ((nandc->cws_per_page) - 1)) {
				len = DATA_BYTES_IN_IMG_PER_CW;
				flags |= BAM_DESC_EOT_FLAG;
			} else {
				/* Allow space for spare bytes in the last page */
				len = USER_DATA_BYTES_PER_CW -
					 (((nandc->cws_per_page) -  1) << 2);
				flags = 0;
			}
		} else {
			start = (uintptr_t)buffer + i * (nandc->cw_size);
			if (i < ((nandc->cws_per_page) - 1)) {
				len = (nandc->cw_size);
				flags |= BAM_DESC_EOT_FLAG;
			}
			else {
				len = (nandc->cw_size - mtd->oobsize);
				flags = 0;
			}
		}
		q_bam_add_one_desc(&nandc->bam, DATA_CONSUMER_PIPE_INDEX,
						(void *)(uintptr_t)(start),
				 len, flags);
		num_desc++;

		if ((i == ((nandc->cws_per_page) - 1))) {
			/* write extra data */
			start = (uintptr_t)spareaddr;
			if (cfg_mode == NAND_CFG)
				len = ((nandc->cws_per_page) << 2);
			else
				len = mtd->oobsize;
			flags = BAM_DESC_EOT_FLAG | BAM_DESC_INT_FLAG;
			q_bam_add_one_desc(&nandc->bam, DATA_CONSUMER_PIPE_INDEX,
					(void *)(uintptr_t)(start), len, flags);
			num_desc++;
		}
	}

	q_bam_sys_gen_event(&nandc->bam, DATA_CONSUMER_PIPE_INDEX, num_desc);
}

static nand_result_t
qti_nandc_write_page(struct mtd_info *mtd, uint32_t pg_addr,
					 enum nand_cfg_value cfg_mode,
					 struct mtd_oob_ops *ops)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cfg_params cfg;
	int nand_ret = NANDC_RESULT_SUCCESS;
	unsigned i;

	if (cfg_mode == NAND_CFG_RAW) {
		cfg.cfg0 = nandc->cfg0_raw;
		cfg.cfg1 = nandc->cfg1_raw;
	} else {
		cfg.cfg0 = nandc->cfg0;
		cfg.cfg1 = nandc->cfg1;
	}

	cfg.cmd = NAND_CMD_PRG_PAGE;
	cfg.exec = 1;

	cfg.addr0 = pg_addr << 16;
	cfg.addr1 = (pg_addr >> 16) & 0xff;

	qti_add_wr_page_cws_data_desc(mtd, ops->datbuf, cfg_mode, ops->oobbuf);

	qti_nandc_add_wr_page_cws_cmd_desc(mtd, &cfg, nandc->nandc_buffer,
								cfg_mode);

	/* Check for errors */
	for(i = 0; i < (nandc->cws_per_page); i++) {
		nand_ret = qti_nandc_check_status(mtd, nandc->nandc_buffer[i]);
		if (nand_ret) {
			printf(
				"Failed to write CW %d for page: %d\n",
				i, pg_addr);
			break;
		}
	}

	/* Wait for data to be available */
	qti_nandc_wait_for_data(nandc, DATA_CONSUMER_PIPE_INDEX);

	ops->retlen += mtd->writesize;
	ops->datbuf += mtd->writesize;

	if (ops->oobbuf != NULL) {
			ops->oobretlen += nandc->oob_per_page;
			ops->oobbuf += nandc->oob_per_page;
	}

	return nand_ret;
}

static int
qti_nandc_mark_badblock(struct mtd_info *mtd, loff_t offs)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);
	uint32_t page;
	int nand_ret = NANDC_RESULT_SUCCESS;
	struct mtd_oob_ops ops;

	/* Check for invalid offset */
	if (offs > mtd->size)
		return -EINVAL;

	if (offs & (mtd->erasesize - 1))
		return -EINVAL;

	page = offs >> chip->page_shift;

	ops.mode = MTD_OPS_RAW;
	ops.len = mtd->writesize;
	ops.retlen = 0;
	ops.ooblen = mtd->oobsize;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = nandc->zero_page;
	ops.oobbuf = nandc->zero_oob;

	/* Going to first page of the block */
	if (page & (nandc->num_pages_per_blk_mask))
		page = page - (page & (nandc->num_pages_per_blk_mask));

	nand_ret = qti_nandc_write_page(mtd, page, NAND_CFG_RAW, &ops);
	if (!nand_ret)
		mtd->ecc_stats.badblocks++;
	return nand_ret;

}

/*
 * Populate flash parameters from the configuration byte.
 */
static void qti_nandc_sync(struct mtd_info *mtd)
{
	/* Nop */
}

static int qti_nandc_scan_bbt_nop(struct mtd_info *mtd)
{
	return 0;
}

/*
 * Estimate the no. of pages to read, based on the data length and oob
 * length.
 */
static u_long qti_get_read_page_count(struct mtd_info *mtd,
				       struct mtd_oob_ops *ops, loff_t to)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);
	uint32_t start_page, end_page;

	if (ops->datbuf != NULL) {
		/*
		 * Determine start page (can be non page aligned) and end page
		 * and calculate number of pages.
		 */
		start_page = to >> chip->page_shift;
		end_page = (to + ops->len - 1) >> chip->page_shift;
		return end_page - start_page + 1;
	} else {
	if (nandc->oob_per_page == 0)
		return 0;

	return (ops->ooblen + nandc->oob_per_page - 1) / nandc->oob_per_page;
	}
}

/*
 * Return the buffer where the next OOB data should be stored. If the
 * user buffer is insufficient to hold one page worth of OOB data,
 * return an internal buffer, to hold the data temporarily.
 */
static uint8_t *qti_nandc_read_oobbuf(struct mtd_info *mtd,
				     struct mtd_oob_ops *ops)
{
	size_t read_ooblen;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	if (ops->oobbuf == NULL)
		return NULL;

	read_ooblen = ops->ooblen - ops->oobretlen;
	if (read_ooblen < nandc->oob_per_page)
		return nandc->pad_oob;

	return ops->oobbuf + ops->oobretlen;
}
/*
 * Return the buffer where the next in-band data should be stored. If
 * the user buffer is insufficient to hold one page worth of in-band
 * data, return an internal buffer, to hold the data temporarily.
 */
static uint8_t *qti_nandc_read_datbuf(struct mtd_info *mtd,
				     struct mtd_oob_ops *ops, uint32_t col)
{
	size_t read_datlen;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	if (ops->datbuf == NULL)
		return NULL;

	read_datlen = ops->len - ops->retlen;
	if (read_datlen < mtd->writesize || col)
		return nandc->pad_dat;

	return ops->datbuf + ops->retlen;
}

/*
 * Copy the OOB data from the internal buffer, to the user buffer, if
 * the internal buffer was used for the read.
 */
static void qti_nandc_read_oobcopy(struct mtd_info *mtd,
				  struct mtd_oob_ops *ops)
{
	size_t ooblen;
	size_t read_ooblen;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	if (ops->oobbuf == NULL)
		return;

	read_ooblen = ops->ooblen - ops->oobretlen;
	ooblen = (read_ooblen < nandc->oob_per_page ? read_ooblen :
							nandc->oob_per_page);
	if (read_ooblen < nandc->oob_per_page)
		memcpy(ops->oobbuf + ops->oobretlen, nandc->pad_oob, ooblen);

	ops->oobretlen += ooblen;
}

/*
 * Copy the in-band data from the internal buffer, to the user buffer,
 * if the internal buffer was used for the read.
 */
static void
qti_nandc_read_datcopy(struct mtd_info *mtd,
		       struct mtd_oob_ops *ops, uint32_t col)
{
	size_t datlen;
	size_t read_datlen;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	if (ops->datbuf == NULL)
		return;

	read_datlen = ops->len - ops->retlen;

	if (nandc->multi_page_copy) {
		datlen = (mtd->writesize * nandc->multi_page_req_len);
		nandc->multi_page_copy = false;
	} else if (col == 0 && read_datlen >= mtd->writesize) {
		datlen = mtd->writesize;
	} else {
		datlen = min(read_datlen,(size_t)( mtd->writesize - col));
		memcpy(ops->datbuf + ops->retlen, nandc->pad_dat + col, datlen);
	}
	ops->retlen += datlen;
}

static int
qti_nandc_check_erased_buf(uint8_t *buf, int len, int bitflips_threshold)
{
	int bitflips = 0;

	for (; len > 0; len--, buf++) {
		bitflips += 8 - hweight8(*buf);
		if (unlikely(bitflips > bitflips_threshold))
			return -EBADMSG;
	}

	return bitflips;
}

/*
 * Now following logic is being added to identify the erased codeword
 * bitflips.
 * 1. Maintain the bitmasks for the codewords which generated uncorrectable
 *    error.
 * 2. Read the raw data again in temp buffer and count the number of zeros.
 *    Since spare bytes are unused in ECC layout and won’t affect ECC
 *    correctability so no need to count number of zero in spare bytes.
 * 3. If the number of zero is below ECC correctability then it can be
 *    treated as erased CW. In this case, make all the data/oob of actual user
 *    buffers as 0xff.
 */
static int
qti_nandc_check_erased_page(struct mtd_info *mtd, uint32_t page,
			    uint8_t *datbuf,
			    uint8_t *oobbuf,
			    unsigned int uncorrectable_err_cws,
			    unsigned int *max_bitflips)
{
	struct mtd_oob_ops raw_page_ops;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	uint8_t *tmp_datbuf;
	unsigned int tmp_datasize, datasize, oobsize;
	int i, start_cw, last_cw, ret, data_bitflips;

	raw_page_ops.mode = MTD_OPS_RAW;
	raw_page_ops.len = mtd->writesize;
	raw_page_ops.ooblen =  mtd->oobsize;
	raw_page_ops.datbuf = nandc->tmp_datbuf;
	raw_page_ops.oobbuf = nandc->tmp_oobbuf;
	raw_page_ops.retlen = 0;
	raw_page_ops.oobretlen = 0;

	ret = qti_read_page(mtd, page, NAND_CFG_RAW, &raw_page_ops);
	if (ret)
		return ret;

	start_cw = ffs(uncorrectable_err_cws) - 1;
	last_cw = fls(uncorrectable_err_cws);

	tmp_datbuf = nandc->tmp_datbuf + start_cw * nandc->cw_size;
	tmp_datasize = nandc->cw_size - nandc->spare_bytes;
	datasize = DATA_BYTES_IN_IMG_PER_CW;
	datbuf += start_cw * datasize;

	for (i = start_cw; i < last_cw;
	     i++, datbuf += datasize, tmp_datbuf += nandc->cw_size) {
		if (!(BIT(i) & uncorrectable_err_cws))
			continue;

		data_bitflips =
			qti_nandc_check_erased_buf(tmp_datbuf, tmp_datasize,
						   mtd->ecc_strength);
		if (data_bitflips < 0) {
			mtd->ecc_stats.failed++;
			continue;
		}

		*max_bitflips =
			max_t(unsigned int, *max_bitflips, data_bitflips);

		if (i == nandc->cws_per_page - 1) {
			oobsize = nandc->cws_per_page << 2;
			datasize = DATA_BYTES_IN_IMG_PER_CW - oobsize;
			if (oobbuf)
				memset(oobbuf, 0xff, oobsize);
		}

		if (datbuf)
			memset(datbuf, 0xff, datasize);
	}

	return 0;
}

static int qti_read_page(struct mtd_info *mtd, uint32_t page,
				enum nand_cfg_value cfg_mode,
				struct mtd_oob_ops *ops)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cfg_params params;
	uint32_t ecc;
	struct read_stats *stats = NULL;
	uint32_t addr_loc_0;
	uint32_t addr_loc_1;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	uint32_t num_cmd_desc = 0;
	uint32_t num_data_desc = 0;
	uint32_t i;
	int nand_ret = NANDC_RESULT_SUCCESS;
	uint8_t flags = 0;
	uint32_t *cmd_list_temp = NULL;
	uint16_t data_bytes;
	uint16_t ud_bytes_in_last_cw;
	uint16_t oob_bytes;
	uint8_t *buffer, *ops_datbuf = ops->datbuf;
	uint8_t *spareaddr, *ops_oobbuf = ops->oobbuf;
	uint8_t *buffer_st, *spareaddr_st;
	unsigned int max_bitflips = 0, uncorrectable_err_cws = 0;

	memset(nandc->status_buff, 0, nandc->status_buf_size);
	stats = (struct read_stats *)nandc->status_buff;

	/* Check This address for serial NAND later on if any issue
	 * Because as per HPG Page Read	0x13 NAND_ADDR1[7:0]
	 * NAND_ADDR0[31:24] NAND_ADDR0[23:16]
	 */
	params.addr0 = page << 16;
	params.addr1 = (page >> 16) & 0xff;

	if (cfg_mode == NAND_CFG_RAW) {
		params.cfg0 = nandc->cfg0_raw;
		params.cfg1 = nandc->cfg1_raw;
		params.cmd = NAND_CMD_PAGE_READ;
		ecc = 0x1; /* Disable ECC */

		if (nandc->quad_mode)
			params.cmd |= QTI_SPI_TRANSFER_MODE_X4;
		else
			params.cmd |= QTI_SPI_TRANSFER_MODE_X1;

		params.cmd |= (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET);

		data_bytes =  nandc->cw_size;
		oob_bytes = mtd->oobsize;
		ud_bytes_in_last_cw = (nandc->cw_size - mtd->oobsize);
	} else {
		params.cfg0 = nandc->cfg0;
		params.cfg1 = nandc->cfg1;
		params.cmd = NAND_CMD_PAGE_READ_ALL;

		if (nandc->quad_mode)
			params.cmd |= QTI_SPI_TRANSFER_MODE_X4;
		else
			params.cmd |= QTI_SPI_TRANSFER_MODE_X1;

		params.cmd |= (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET);

		ecc = (nandc->ecc_bch_cfg);
		data_bytes = DATA_BYTES_IN_IMG_PER_CW;
		ud_bytes_in_last_cw = USER_DATA_BYTES_PER_CW -
					 (((nandc->cws_per_page) - 1) << 2);
		oob_bytes = DATA_BYTES_IN_IMG_PER_CW - ud_bytes_in_last_cw;
	}

	params.exec = 1;
	/* Read all the Data bytes in the first 3 CWs. */
	addr_loc_0 = NAND_RD_LOC_OFFSET(0);
	addr_loc_0 |= NAND_RD_LOC_SIZE(data_bytes);;
	addr_loc_0 |= NAND_RD_LOC_LAST_BIT(1);

	addr_loc_1 = NAND_RD_LOC_OFFSET(ud_bytes_in_last_cw);
	addr_loc_1 |= NAND_RD_LOC_SIZE(oob_bytes);
	addr_loc_1 |= NAND_RD_LOC_LAST_BIT(1);

	/* Reset and Configure erased CW/page detection controller */
	qti_nandc_erased_status_reset(nandc, nandc->ce_array,
					BAM_DESC_LOCK_FLAG);

	if (ops->datbuf == NULL) {
		buffer = nandc->pad_dat;
	} else {
		buffer = ops->datbuf;
	}

	if (ops->oobbuf == NULL) {
		spareaddr = nandc->pad_oob;
	} else {
		spareaddr = ops->oobbuf;
	}

	buffer_st = buffer;
	spareaddr_st = spareaddr;

	/* Queue up the command and data descriptors for all the codewords
	 * in a page and do a single bam transfer at the end.*/
	for (i = 0; i < (nandc->cws_per_page); i++) {
		num_cmd_desc = 0;
		num_data_desc = 0;

		if (i == 0) {
			cmd_list_ptr = qti_nand_add_addr_n_cfg_ce(nandc,
								&params,
								cmd_list_ptr);

			bam_add_cmd_element(cmd_list_ptr,
							NAND_DEV0_ECC_CFG,
							(uint32_t)ecc,
							CE_WRITE_TYPE);
			cmd_list_ptr++;
		} else
			cmd_list_ptr_start = cmd_list_ptr;

		bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD,
					(uint32_t)params.cmd,
					CE_WRITE_TYPE);
		cmd_list_ptr++;

		if (i == (nandc->cws_per_page) - 1) {
			/* Write addr loc 1 only for the last CW. */
			addr_loc_0 = NAND_RD_LOC_OFFSET(0);
			addr_loc_0 |= NAND_RD_LOC_SIZE(ud_bytes_in_last_cw);
			addr_loc_0 |= NAND_RD_LOC_LAST_BIT(0);

			 /*To read only spare bytes 80 0r 16*/
			bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(1),
					(uint32_t)addr_loc_1, CE_WRITE_TYPE);

			cmd_list_ptr++;
			flags = 0;
			/* Add Data desc */
			q_bam_add_one_desc(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					(uint8_t *)((uintptr_t)(buffer)),
					ud_bytes_in_last_cw,
					flags);
			num_data_desc++;
			q_bam_add_one_desc(&nandc->bam,
					 DATA_PRODUCER_PIPE_INDEX,
					 (uint8_t *)((uintptr_t)(spareaddr)),
					 oob_bytes,
					 BAM_DESC_INT_FLAG);
			num_data_desc++;
			q_bam_sys_gen_event(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					num_data_desc);
		} else {
				/* Add Data desc */
			q_bam_add_one_desc(&nandc->bam,
					 DATA_PRODUCER_PIPE_INDEX,
					 (uint8_t *)((uintptr_t)buffer),
					 data_bytes,
					 0);
			num_data_desc++;
			q_bam_sys_gen_event(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					num_data_desc);
		}

		if (i == (nandc->cws_per_page) - 1)
			bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(0),
					(uint32_t)addr_loc_0,
					CE_WRITE_TYPE);
		else
			bam_add_cmd_element(cmd_list_ptr,
				    NAND_READ_LOCATION_n(0),
				    (uint32_t)addr_loc_0,
				    CE_WRITE_TYPE);
		cmd_list_ptr++;

		bam_add_cmd_element(cmd_list_ptr,
				    NAND_EXEC_CMD,
				    (uint32_t)params.exec,
				    CE_WRITE_TYPE);
		cmd_list_ptr++;

		/* Enqueue the desc for the above commands */
		q_bam_add_one_desc(&nandc->bam,
				 CMD_PIPE_INDEX,
				 (uint8_t*)cmd_list_ptr_start,
				 ((uintptr_t)cmd_list_ptr -
				 (uintptr_t)cmd_list_ptr_start),
				 BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG);
		num_cmd_desc++;

		bam_add_cmd_element(cmd_list_ptr,
				NAND_FLASH_STATUS,
				(uintptr_t)&(stats[i].flash_sts),
				CE_READ_TYPE);

		cmd_list_temp = (uint32_t *)cmd_list_ptr;

		cmd_list_ptr++;

		bam_add_cmd_element(cmd_list_ptr,
				NAND_BUFFER_STATUS,
				(uintptr_t)&(stats[i].buffer_sts),
				CE_READ_TYPE);
		cmd_list_ptr++;

		bam_add_cmd_element(cmd_list_ptr,
			NAND_ERASED_CW_DETECT_STATUS,
			(uintptr_t)&(stats[i].erased_cw_sts),
			CE_READ_TYPE);
		cmd_list_ptr++;

		if (i == (nandc->cws_per_page) - 1) {
			flags = BAM_DESC_CMD_FLAG | BAM_DESC_UNLOCK_FLAG;
		} else
			flags = BAM_DESC_CMD_FLAG;

		/* Enqueue the desc for the above command */
		q_bam_add_one_desc(&nandc->bam,
				CMD_PIPE_INDEX,
				(uint8_t*)((uintptr_t)cmd_list_temp),
				((uintptr_t)cmd_list_ptr -
				(uintptr_t)cmd_list_temp),
				flags);
		num_cmd_desc++;

		if (ops->datbuf != NULL) {
			if (i == (nandc->cws_per_page - 1)) {
				buffer += ud_bytes_in_last_cw;
				ops->datbuf += ud_bytes_in_last_cw;
				ops->retlen += ud_bytes_in_last_cw;
			} else {
				buffer = ops->datbuf + data_bytes;
				ops->datbuf += data_bytes;
				ops->retlen += data_bytes;
			}
		}
		else {
			if (i == (nandc->cws_per_page - 1)) {
				buffer += ud_bytes_in_last_cw;
			} else {
				buffer += data_bytes;
			}
		}
		if ((i == (nandc->cws_per_page) - 1)) {
			if (ops->oobbuf != NULL) {
				spareaddr += oob_bytes;
				ops->oobretlen += oob_bytes;
				ops->oobbuf += oob_bytes;
			} else
				spareaddr += oob_bytes;
		}
		/* Notify BAM HW about the newly added descriptors */
		q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, num_cmd_desc);
	}

	qti_nandc_wait_for_data(nandc, DATA_PRODUCER_PIPE_INDEX);

#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((uintptr_t)nandc->status_buff,
			   (uintptr_t)nandc->status_buff +
			   nandc->status_buf_size);
#endif

	/* Check status */
	for (i = 0; i < (nandc->cws_per_page) ; i ++) {
		if (cfg_mode == NAND_CFG_RAW)
			nand_ret = qti_nandc_check_status(mtd,
							  stats[i].flash_sts);
		else
			nand_ret = qti_nandc_check_read_status(mtd, &stats[i]);

		if (nand_ret < 0) {
			if (nand_ret == -EBADMSG) {
				uncorrectable_err_cws |= BIT(i);
				continue;
			}
			printf("%s: check status failed mode %d "
				" uncorrectable_err_cws %d max_bitflips %d\n",
				__func__, cfg_mode, uncorrectable_err_cws,
				max_bitflips);
			goto error;
		}

		max_bitflips = max_t(unsigned int, max_bitflips, nand_ret);
	}

	if (uncorrectable_err_cws) {
		nand_ret = qti_nandc_check_erased_page(mtd, page, ops_datbuf,
						       ops_oobbuf,
						       uncorrectable_err_cws,
						       &max_bitflips);
		if (nand_ret < 0) {
			printf("%s: check_erased_page failed page "
				"uncorrectable_err_cws %d max_bitflips %d\n",
				__func__, uncorrectable_err_cws,
				max_bitflips);

			goto error;
		}
	}

	return max_bitflips;
error:

	printf("%s page read failed. page: %d status 0x%x\n",
	       __func__, page, nand_ret);

	return nand_ret;
}


int qti_nandc_multi_page_read(struct mtd_info *mtd, uint32_t page,
				enum nand_cfg_value cfg_mode,
				struct mtd_oob_ops *ops, uint32_t num_pages)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cfg_params params;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	struct read_stats *stats = nandc->stats;
	uint32_t auto_status = QTI_SPI_NAND_AUTO_STATUS_VAL;
	uint8_t *buffer, *ops_datbuf = ops->datbuf;
	uint8_t *spareaddr, *ops_oobbuf = ops->oobbuf;
	uint8_t *buffer_st, *spareaddr_st;
	uint8_t *auto_status_buffer = NULL;
	uint8_t *tmp_status_buffer = NULL;
	uint16_t data_bytes;
	uint16_t ud_bytes_in_last_cw;
	uint16_t oob_bytes;
	uint32_t addr_loc_0, addr_loc_1, addr_loc_last, ecc;
	uint32_t num_data_desc = 0;
	uint32_t num_status_desc = 0;
	uint32_t i, j;
	uint8_t flags = 0;
	int nand_ret = NANDC_RESULT_SUCCESS;
	unsigned int max_bitflips = 0, uncorrectable_err_cws = 0;

	params.addr0 = page << 16;
	params.addr1 = (page >> 16) & 0xff;

	memset(nandc->status_buff, 0, nandc->status_buf_size);
	auto_status_buffer = nandc->status_buff;
	tmp_status_buffer = nandc->status_buff;

	nandc->multi_page_copy = true;


	params.cmd = (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET);
	if (nandc->quad_mode)
		params.cmd |= QTI_SPI_TRANSFER_MODE_X4;
	else
		params.cmd |= QTI_SPI_TRANSFER_MODE_X1;

	if (cfg_mode == NAND_CFG_RAW) {
		params.cfg0 = nandc->cfg0_raw;
		params.cfg1 = nandc->cfg1_raw;
		params.cmd |= (NAND_CMD_PAGE_READ | QTI_MULTIPAGE_CMD_EN);
		ecc = 0x1; /* Disable ECC */

		data_bytes =  nandc->cw_size;
		oob_bytes = mtd->oobsize;
		ud_bytes_in_last_cw = (nandc->cw_size - mtd->oobsize);
	} else {
		params.cfg0 = nandc->cfg0;
		params.cfg1 = nandc->cfg1;
		params.cmd |= (NAND_CMD_PAGE_READ_ALL | QTI_MULTIPAGE_CMD_EN);

		ecc = (nandc->ecc_bch_cfg);
		data_bytes = DATA_BYTES_IN_IMG_PER_CW;
		ud_bytes_in_last_cw = USER_DATA_BYTES_PER_CW -
				(((nandc->cws_per_page) - 1) << 2);
		oob_bytes = DATA_BYTES_IN_IMG_PER_CW - ud_bytes_in_last_cw;
	}

	params.exec = 1;

	addr_loc_0 = NAND_RD_LOC_OFFSET(0);
	addr_loc_0 |= NAND_RD_LOC_SIZE(data_bytes);;
	addr_loc_0 |= NAND_RD_LOC_LAST_BIT(1);

	addr_loc_1 = NAND_RD_LOC_OFFSET(ud_bytes_in_last_cw);
	addr_loc_1 |= NAND_RD_LOC_SIZE(oob_bytes);
	addr_loc_1 |= NAND_RD_LOC_LAST_BIT(1);

	addr_loc_last = NAND_RD_LOC_OFFSET(0);
	addr_loc_last |= NAND_RD_LOC_SIZE(ud_bytes_in_last_cw);
	addr_loc_last |= NAND_RD_LOC_LAST_BIT(0);

	/* reset address reg before executing for
	 * next multi page read
	 */
	reset_addr_reg(nandc, nandc->ce_array, 0);

	/* reset multi_page_cmd_reg */
	multi_page_cmd_reg_reset(nandc, nandc->ce_array, 0);

	/* Reset and Configure erased CW/page detection controller */
	qti_nandc_erased_status_reset(nandc, nandc->ce_array,
							BAM_DESC_LOCK_FLAG);

	if (ops->datbuf == NULL) {
		buffer = nandc->pad_dat;
	} else {
		buffer = ops->datbuf;
	}

	if (ops->oobbuf == NULL) {
		spareaddr = nandc->pad_oob;
	} else {
		spareaddr = ops->oobbuf;
	}

	buffer_st = buffer;
	spareaddr_st = spareaddr;

	cmd_list_ptr = qti_nand_add_addr_n_cfg_ce(nandc, &params, cmd_list_ptr);
	bam_add_cmd_element(cmd_list_ptr, NAND_DEV0_ECC_CFG,
					(uint32_t)ecc, CE_WRITE_TYPE);
	cmd_list_ptr++;
	bam_add_cmd_element(cmd_list_ptr, NAND_AUTO_STATUS_EN,
				(uint32_t)auto_status, CE_WRITE_TYPE);
	cmd_list_ptr++;
	bam_add_cmd_element(cmd_list_ptr, NAND_MULTI_PAGE_CMD,
				(uint32_t)num_pages - 1, CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Enqueue the desc for the above commands */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_CMD_FLAG);

	q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, 1);

	/* Queue up the command and data descriptors
	 * for all the requested page
	 * and do a single bam transfer at the end.
	 */
	for (j = 0; j < num_pages; j++) {
		schedule();

		for (i = 0; i < (nandc->cws_per_page); i++) {
			num_data_desc = 0;
			num_status_desc = 0;

			if (i == (nandc->cws_per_page) - 1) {

				if ( j == num_pages - 1) {
					flags = BAM_DESC_INT_FLAG;
				} else {
					flags = 0;
				}
				/* Add Data desc */
				q_bam_add_one_desc(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					(uint8_t *)((uintptr_t)(buffer)),
					ud_bytes_in_last_cw,
					0);
				num_data_desc++;

				q_bam_add_one_desc(&nandc->bam,
					 DATA_PRODUCER_PIPE_INDEX,
					 (uint8_t *)((uintptr_t)(spareaddr)),
					 oob_bytes,
					 flags);
				num_data_desc++;

				/* add data descriptor to read status */
				q_bam_add_one_desc(&nandc->bam,
					 BAM_STATUS_PIPE_INDEX,
					 (uint8_t *)(uintptr_t)
					 (auto_status_buffer),
					 QTI_AUTO_STATUS_DES_SIZE,
					 flags);
				num_status_desc++;

				q_bam_sys_gen_event(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					num_data_desc);

				q_bam_sys_gen_event(&nandc->bam,
					BAM_STATUS_PIPE_INDEX,
					num_status_desc);
			} else {
				/* Add Data desc */
				q_bam_add_one_desc(&nandc->bam,
					 DATA_PRODUCER_PIPE_INDEX,
					 (uint8_t *)((uintptr_t)buffer),
					 data_bytes,
					 0);
				num_data_desc++;

				/* add data descriptor to read status */
				q_bam_add_one_desc(&nandc->bam,
					 BAM_STATUS_PIPE_INDEX,
					 (uint8_t *)(uintptr_t)
					 (auto_status_buffer),
					 QTI_AUTO_STATUS_DES_SIZE,
					 0);
				num_status_desc++;

				q_bam_sys_gen_event(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					num_data_desc);

				q_bam_sys_gen_event(&nandc->bam,
					BAM_STATUS_PIPE_INDEX,
					num_status_desc);
			}

			if (ops->datbuf != NULL) {
				if (i == (nandc->cws_per_page - 1)) {
					buffer += ud_bytes_in_last_cw;
					ops->datbuf += ud_bytes_in_last_cw;
					ops->retlen += ud_bytes_in_last_cw;
				} else {
					buffer = ops->datbuf + data_bytes;
					ops->datbuf += data_bytes;
					ops->retlen += data_bytes;
				}
			}
			else {
				if (i == (nandc->cws_per_page - 1)) {
					buffer += ud_bytes_in_last_cw;
				} else {
					buffer += data_bytes;
				}
			}
			if ((i == (nandc->cws_per_page) - 1)) {
				if (ops->oobbuf != NULL) {
					spareaddr += oob_bytes;
					ops->oobretlen += oob_bytes;
					ops->oobbuf += oob_bytes;
				} else
					spareaddr += oob_bytes;
			}

			auto_status_buffer += QTI_AUTO_STATUS_DES_SIZE;
		}
	}

	cmd_list_ptr = cmd_list_ptr_start;

	bam_add_cmd_element(cmd_list_ptr, NAND_READ_LOCATION_n(0),
					(uint32_t)addr_loc_0, CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(0),
					(uint32_t)addr_loc_last,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	/*To read only spare bytes 80 0r 16*/
	bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(1),
					(uint32_t)addr_loc_1,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr, NAND_FLASH_CMD,
					(uint32_t)params.cmd,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	bam_add_cmd_element(cmd_list_ptr, NAND_EXEC_CMD,
					(uint32_t)params.exec,
					CE_WRITE_TYPE);
	cmd_list_ptr++;

	/* Enqueue the desc for the above commands */
	q_bam_add_one_desc(&nandc->bam, CMD_PIPE_INDEX,
		(uint8_t*)cmd_list_ptr_start,
		((uintptr_t)cmd_list_ptr - (uintptr_t)cmd_list_ptr_start),
		BAM_DESC_CMD_FLAG | BAM_DESC_NWD_FLAG);

	/* Notify BAM HW about the newly added descriptors */
	q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, 1);

	qti_nandc_wait_for_data(nandc, DATA_PRODUCER_PIPE_INDEX);

	qti_nandc_wait_for_data(nandc, BAM_STATUS_PIPE_INDEX);

#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((uintptr_t)nandc->status_buff,
			   (uintptr_t)nandc->status_buff +
			   nandc->status_buf_size);
#endif
	/* Update the auto status structure */
	for (j =0; j < num_pages; j++) {

		for (i = 0; i < (nandc->cws_per_page); i++) {

			memlcpy(&stats[i].flash_sts, 4, tmp_status_buffer, 4);
			memlcpy(&stats[i].buffer_sts, 4,
						tmp_status_buffer + 4, 4);
			memlcpy(&stats[i].erased_cw_sts, 4,
						tmp_status_buffer + 8, 4);

			/* Check status */
			if (cfg_mode == NAND_CFG_RAW)
				nand_ret = qti_nandc_check_status(mtd,
							stats[i].flash_sts);
			else
				nand_ret = qti_nandc_check_read_status(mtd,
							&stats[i]);

			if (nand_ret < 0) {
				if (nand_ret == -EBADMSG) {
					uncorrectable_err_cws |= BIT(i);
						continue;
				}
				printf("%s: check status failed mode %d "
					"uncorrectable_err_cws %d "
					"max_bitflips %d\n",
					__func__, cfg_mode,
					uncorrectable_err_cws,
					max_bitflips);

				goto error;
			}

			max_bitflips = max_t(unsigned int, max_bitflips,
					nand_ret);
			tmp_status_buffer += QTI_SPI_MAX_STATUS_REG;
		}

		if (uncorrectable_err_cws) {
			nand_ret = qti_nandc_check_erased_page(mtd, page + j,
					(ops_datbuf + (j * mtd->writesize)),
					ops_oobbuf,
					uncorrectable_err_cws,
					&max_bitflips);
			if (nand_ret < 0) {
				printf("%s: check_erased_page failed"
					"uncorrectable_err_cws %d "
					"max_bitflips %d\n",
					__func__, uncorrectable_err_cws,
					max_bitflips);

				goto error;
			}
		}
	}

	return max_bitflips;
error:

	printf("%s: page read failed. page: %d status 0x%x\n",
	       __func__, page, nand_ret);

	return nand_ret;
}

int qti_nandc_page_read(struct mtd_info *mtd, uint32_t page,
				enum nand_cfg_value cfg_mode,
				struct mtd_oob_ops *ops)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cfg_params params;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	struct read_stats *stats = nandc->stats;
	uint32_t auto_status = QTI_SPI_NAND_AUTO_STATUS_VAL;
	uint8_t *buffer, *ops_datbuf = ops->datbuf;
	uint8_t *spareaddr, *ops_oobbuf = ops->oobbuf;
	uint8_t *buffer_st, *spareaddr_st;
	uint8_t *auto_status_buffer = NULL;
	uint16_t data_bytes;
	uint16_t ud_bytes_in_last_cw;
	uint16_t oob_bytes;
	uint32_t addr_loc_0, addr_loc_1, ecc;
	uint32_t num_cmd_desc = 0;
	uint32_t num_data_desc = 0;
	uint32_t num_status_desc = 0;
	uint32_t i;
	uint32_t parse_size = 0x0;
	uint8_t flags = 0;
	int nand_ret = NANDC_RESULT_SUCCESS;
	unsigned int max_bitflips = 0, uncorrectable_err_cws = 0;

	params.addr0 = page << 16;
	params.addr1 = (page >> 16) & 0xff;

	memset(nandc->status_buff, 0, nandc->status_buf_size);
	auto_status_buffer = nandc->status_buff;


	params.cmd = (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET);
	if (nandc->quad_mode)
		params.cmd |= QTI_SPI_TRANSFER_MODE_X4;
	else
		params.cmd |= QTI_SPI_TRANSFER_MODE_X1;

	if (cfg_mode == NAND_CFG_RAW) {
		params.cfg0 = nandc->cfg0_raw;
		params.cfg1 = nandc->cfg1_raw;
		params.cmd |= (NAND_CMD_PAGE_READ | QTI_PAGE_SCOPE_CMD_EN);
		ecc = 0x1; /* Disable ECC */

		data_bytes =  nandc->cw_size;
		oob_bytes = mtd->oobsize;
		ud_bytes_in_last_cw = (nandc->cw_size - mtd->oobsize);
	} else {
		params.cfg0 = nandc->cfg0;
		params.cfg1 = nandc->cfg1;
		params.cmd |= (NAND_CMD_PAGE_READ_ALL | QTI_PAGE_SCOPE_CMD_EN);

		ecc = (nandc->ecc_bch_cfg);
		data_bytes = DATA_BYTES_IN_IMG_PER_CW;
		ud_bytes_in_last_cw = USER_DATA_BYTES_PER_CW -
				(((nandc->cws_per_page) - 1) << 2);
		oob_bytes = DATA_BYTES_IN_IMG_PER_CW - ud_bytes_in_last_cw;
	}

	params.exec = 1;

	addr_loc_0 = NAND_RD_LOC_OFFSET(0);
	addr_loc_0 |= NAND_RD_LOC_SIZE(data_bytes);;
	addr_loc_0 |= NAND_RD_LOC_LAST_BIT(1);

	addr_loc_1 = NAND_RD_LOC_OFFSET(ud_bytes_in_last_cw);
	addr_loc_1 |= NAND_RD_LOC_SIZE(oob_bytes);
	addr_loc_1 |= NAND_RD_LOC_LAST_BIT(1);

	/* Reset and Configure erased CW/page detection controller */
	qti_nandc_erased_status_reset(nandc, nandc->ce_array,
					BAM_DESC_LOCK_FLAG);

	if (ops->datbuf == NULL) {
		buffer = nandc->pad_dat;
	} else {
		buffer = ops->datbuf;
	}

	if (ops->oobbuf == NULL) {
		spareaddr = nandc->pad_oob;
	} else {
		spareaddr = ops->oobbuf;
	}

	buffer_st = buffer;
	spareaddr_st = spareaddr;

	/* Queue up the command and data descriptors for all the
	 * codewords in a page and do a single bam transfer at the end.*/
	for (i = 0; i < (nandc->cws_per_page); i++) {
		schedule();
		num_cmd_desc = 0;
		num_data_desc = 0;
		num_status_desc = 0;
		if (i == 0) {
			cmd_list_ptr = qti_nand_add_addr_n_cfg_ce(nandc,
								&params,
								cmd_list_ptr);

			bam_add_cmd_element(cmd_list_ptr,
				NAND_DEV0_ECC_CFG,(uint32_t)ecc,
				CE_WRITE_TYPE);
			cmd_list_ptr++;

			bam_add_cmd_element(cmd_list_ptr,
				NAND_AUTO_STATUS_EN,(uint32_t)auto_status,
				CE_WRITE_TYPE);
			cmd_list_ptr++;

			bam_add_cmd_element(cmd_list_ptr,
							NAND_FLASH_CMD,
							(uint32_t)params.cmd,
							CE_WRITE_TYPE);
			cmd_list_ptr++;
		} else
			cmd_list_ptr_start = cmd_list_ptr;

		if (i == (nandc->cws_per_page) - 1) {
			/* Write addr loc 1 only for the last CW. */
			addr_loc_0 = NAND_RD_LOC_OFFSET(0);
			addr_loc_0 |= NAND_RD_LOC_SIZE(ud_bytes_in_last_cw);
			addr_loc_0 |= NAND_RD_LOC_LAST_BIT(0);

			 /*To read only spare bytes 80 0r 16*/
			bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(1),
					(uint32_t)addr_loc_1, CE_WRITE_TYPE);

			cmd_list_ptr++;

			/* Add Data desc */
			q_bam_add_one_desc(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					(uint8_t *)((uintptr_t)(buffer)),
					ud_bytes_in_last_cw,
					0);
			num_data_desc++;

			q_bam_add_one_desc(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					(uint8_t *)((uintptr_t)(spareaddr)),
					oob_bytes,
					BAM_DESC_INT_FLAG);
			num_data_desc++;

			/* add data descriptor to read status */
			q_bam_add_one_desc(&nandc->bam,
				BAM_STATUS_PIPE_INDEX,
				(uint8_t *)((uintptr_t)(auto_status_buffer +
					i * QTI_SPI_MAX_STATUS_REG)),
				 QTI_AUTO_STATUS_DES_SIZE,
				 BAM_DESC_INT_FLAG);
			num_status_desc++;

			q_bam_sys_gen_event(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					num_data_desc);

			q_bam_sys_gen_event(&nandc->bam,
						BAM_STATUS_PIPE_INDEX,
						num_status_desc);
		} else {
			/* Add Data desc */
			q_bam_add_one_desc(&nandc->bam,
					 DATA_PRODUCER_PIPE_INDEX,
					 (uint8_t *)((uintptr_t)buffer),
					 data_bytes,
					 0);
			num_data_desc++;

			/* add data descriptor to read status */
			q_bam_add_one_desc(&nandc->bam,
				BAM_STATUS_PIPE_INDEX,
				(uint8_t *)((uintptr_t)(auto_status_buffer +
					i * QTI_SPI_MAX_STATUS_REG)),
				QTI_AUTO_STATUS_DES_SIZE,
				0);
			num_status_desc++;

			q_bam_sys_gen_event(&nandc->bam,
					DATA_PRODUCER_PIPE_INDEX,
					num_data_desc);

			q_bam_sys_gen_event(&nandc->bam, BAM_STATUS_PIPE_INDEX,
					  num_status_desc);
		}

		if (i == (nandc->cws_per_page) - 1) {
			bam_add_cmd_element(cmd_list_ptr,
					NAND_READ_LOCATION_LAST_CW_n(0),
					(uint32_t)addr_loc_0,
					CE_WRITE_TYPE);
			cmd_list_ptr++;

			bam_add_cmd_element(cmd_list_ptr,
							NAND_EXEC_CMD,
							(uint32_t)params.exec,
							CE_WRITE_TYPE);

			cmd_list_ptr++;
		} else {
			bam_add_cmd_element(cmd_list_ptr,
				    NAND_READ_LOCATION_n(0),
				    (uint32_t)addr_loc_0,
				    CE_WRITE_TYPE);
			cmd_list_ptr++;
		}

		if (i == (nandc->cws_per_page) - 1) {
			flags = BAM_DESC_CMD_FLAG | BAM_DESC_NWD_FLAG;
		} else
			flags = BAM_DESC_CMD_FLAG;

		/* Enqueue the desc for the above commands */
		q_bam_add_one_desc(&nandc->bam,
				 CMD_PIPE_INDEX,
				 (uint8_t*)cmd_list_ptr_start,
				 ((uintptr_t)cmd_list_ptr -
				 (uintptr_t)cmd_list_ptr_start),
				 flags);
		num_cmd_desc++;

		if (ops->datbuf != NULL) {
			if (i == (nandc->cws_per_page - 1)) {
				buffer += ud_bytes_in_last_cw;
				ops->datbuf += ud_bytes_in_last_cw;
				ops->retlen += ud_bytes_in_last_cw;
			} else {
				buffer = ops->datbuf + data_bytes;
				ops->datbuf += data_bytes;
				ops->retlen += data_bytes;
			}
		}
		else {
			if (i == (nandc->cws_per_page - 1)) {
				buffer += ud_bytes_in_last_cw;
			} else {
				buffer += data_bytes;
			}
		}
		if ((i == (nandc->cws_per_page) - 1)) {
			if (ops->oobbuf != NULL) {
				spareaddr += oob_bytes;
				ops->oobretlen += oob_bytes;
				ops->oobbuf += oob_bytes;
			} else
				spareaddr += oob_bytes;
		}
		/* Notify BAM HW about the newly added descriptors */
		q_bam_sys_gen_event(&nandc->bam,
					CMD_PIPE_INDEX,
					num_cmd_desc);
	}

	qti_nandc_wait_for_data(nandc, BAM_STATUS_PIPE_INDEX);

	qti_nandc_wait_for_data(nandc, DATA_PRODUCER_PIPE_INDEX);

	GET_STATUS_BUFF_PARSE_SIZE_PER_PAGE(mtd->writesize, parse_size);
#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((uintptr_t)nandc->status_buff,
			   (uintptr_t)nandc->status_buff +
						nandc->status_buf_size);
#endif
	/* Update the auto status structure */
	for (i = 0; i < (nandc->cws_per_page); i++) {
		memlcpy(&stats[i].flash_sts, 4, (nandc->status_buff +
			i * QTI_SPI_MAX_STATUS_REG),
				sizeof(int));
		memlcpy(&stats[i].buffer_sts, 4, (nandc->status_buff +
			i * QTI_SPI_MAX_STATUS_REG) + 4,
				sizeof(int));
		memlcpy(&stats[i].erased_cw_sts, 4, (nandc->status_buff +
			i * QTI_SPI_MAX_STATUS_REG) + 8,
				sizeof(int));
	}

	/* Check status */
	for (i = 0; i < (nandc->cws_per_page) ; i ++) {
		if (cfg_mode == NAND_CFG_RAW)
			nand_ret = qti_nandc_check_status(mtd,
							  stats[i].flash_sts);
		else
			nand_ret = qti_nandc_check_read_status(mtd, &stats[i]);

		if (nand_ret < 0) {
			if (nand_ret == -EBADMSG) {
				uncorrectable_err_cws |= BIT(i);
				continue;
			}
			printf("%s: check status failed mode %d "
					"uncorrectable_err_cws %d "
					"max_bitflips %d\n", __func__,
					cfg_mode, uncorrectable_err_cws,
					max_bitflips);
			goto error;
		}

		max_bitflips = max_t(unsigned int, max_bitflips, nand_ret);
	}

	if (uncorrectable_err_cws) {
		nand_ret = qti_nandc_check_erased_page(mtd, page, ops_datbuf,
						       ops_oobbuf,
						       uncorrectable_err_cws,
						       &max_bitflips);
		if (nand_ret < 0) {
			printf("%s check_erased_page Failed "
				"uncorrectable_err_cws %d "
				"max_bitflips %d\n", __func__,
				uncorrectable_err_cws, max_bitflips);
			goto error;
		}
	}

	return max_bitflips;
error:
	printf("%s: page read failed page %d status 0x%x\n",
	       __func__,page, nand_ret);

	return nand_ret;
}

static int qti_alloc_status_buff(struct qcom_nand_controller *nandc,
		struct mtd_info *mtd)
{
	uint32_t size = 0;

	GET_STATUS_BUFF_ALLOC_SIZE(mtd->writesize, size);

	size = roundup(size, CONFIG_SYS_CACHELINE_SIZE);

	nandc->status_buff = (uint8_t *)malloc_cache_aligned(size);
	if (!nandc->status_buff)
		return -ENOMEM;

	nandc->status_buf_size = size;
	memset(nandc->status_buff, 0, nandc->status_buf_size);

	return 0;
}

static int qti_read_page_scope(struct mtd_info *mtd,
		loff_t to, struct mtd_oob_ops *ops)
{
	uint32_t i = 0, ret = 0;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);
	uint32_t start_page;
	uint32_t num_pages, req_pages = 0x0;
	uint32_t col;
	enum nand_cfg_value cfg_mode;
	unsigned int max_bitflips = 0;
	unsigned int ecc_failures = mtd->ecc_stats.failed;

	/* We don't support MTD_OOB_PLACE as of yet. */
	if (ops->mode == MTD_OPS_PLACE_OOB)
		return -ENOSYS;

	/* Check for reads past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size)
		return -EINVAL;

	if (ops->ooboffs != 0)
		return -EINVAL;

	if(ops->mode == MTD_OPS_RAW) {
		cfg_mode = NAND_CFG_RAW;
		nandc->oob_per_page = mtd->oobsize;
	} else {
		cfg_mode = NAND_CFG;
		nandc->oob_per_page = mtd->oobavail;
	}

	start_page = ((to >> chip->page_shift));
	num_pages = qti_get_read_page_count(mtd, ops, to);
	while (1) {

		if (num_pages > MAX_MULTI_PAGE) {

			req_pages = MAX_MULTI_PAGE;

		} else if (num_pages > 1 && num_pages <= MAX_MULTI_PAGE) {

			req_pages = num_pages;

		} else if (num_pages == 1) {

			req_pages = num_pages;
		}

		struct mtd_oob_ops page_ops;

		col = i == 0 ? to & (mtd->writesize - 1) : 0;
		page_ops.mode = ops->mode;
		page_ops.len = mtd->writesize * req_pages;
		page_ops.ooblen = nandc->oob_per_page;
		page_ops.datbuf = qti_nandc_read_datbuf(mtd, ops, col);
		page_ops.oobbuf = qti_nandc_read_oobbuf(mtd, ops);
		page_ops.retlen = 0;
		page_ops.oobretlen = 0;
		nandc->multi_page_req_len = req_pages;

		if (num_pages > 1)
			ret = qti_nandc_multi_page_read(mtd, start_page,
					cfg_mode, &page_ops, req_pages);
		else
			ret = qti_nandc_page_read(mtd, start_page,
					cfg_mode, &page_ops);

		if (ret < 0) {
			printf("%s: reading page %d failed with %d err\n",
			      __func__, start_page, ret);
			return ret;
		}

		max_bitflips = max_t(unsigned int, max_bitflips, ret);
		qti_nandc_read_datcopy(mtd, ops, col);
		qti_nandc_read_oobcopy(mtd, ops);

		num_pages -= req_pages;
		i++;

		if (!num_pages)
			break;

		start_page += req_pages;
	}

	if (ecc_failures != mtd->ecc_stats.failed) {
		printf("%s: ecc failure while reading from %llx\n",
		       __func__, to);
		return -EBADMSG;
	}

	return max_bitflips;
}


static int qti_nandc_read_oob(struct mtd_info *mtd, loff_t to,
                                    struct mtd_oob_ops *ops)
{
	uint32_t i = 0, ret = 0;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);
	uint32_t start_page;
	uint32_t num_pages;
	uint32_t col;
	enum nand_cfg_value cfg_mode;
	unsigned int max_bitflips = 0;
	unsigned int ecc_failures = mtd->ecc_stats.failed;

	/* We don't support MTD_OOB_PLACE as of yet. */
	if (ops->mode == MTD_OPS_PLACE_OOB)
		return -ENOSYS;

	/* Check for reads past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size)
		return -EINVAL;

	if (ops->ooboffs != 0)
		return -EINVAL;

	if(ops->mode == MTD_OPS_RAW) {
		cfg_mode = NAND_CFG_RAW;
		nandc->oob_per_page = mtd->oobsize;
	} else {
		cfg_mode = NAND_CFG;
		nandc->oob_per_page = mtd->oobavail;
	}

	start_page = ((to >> chip->page_shift));
	num_pages = qti_get_read_page_count(mtd, ops, to);

	for (i = 0; i < num_pages; i++) {
		struct mtd_oob_ops page_ops;

		/*
		 * If start address is non page alinged then determine the
		 * column offset
		 */
		col = i == 0 ? to & (mtd->writesize - 1) : 0;
		page_ops.mode = ops->mode;
		page_ops.len = mtd->writesize;
		page_ops.ooblen = nandc->oob_per_page;
		page_ops.datbuf = qti_nandc_read_datbuf(mtd, ops, col);
		page_ops.oobbuf = qti_nandc_read_oobbuf(mtd, ops);
		page_ops.retlen = 0;
		page_ops.oobretlen = 0;

		ret = qti_read_page(mtd, start_page + i, cfg_mode,
					  &page_ops);

		if (ret < 0) {
			printf("%s: reading page %d failed with %d err\n",
			      __func__, start_page + i, ret);
			return ret;
		}

		max_bitflips = max_t(unsigned int, max_bitflips, ret);
		qti_nandc_read_datcopy(mtd, ops, col);
		qti_nandc_read_oobcopy(mtd, ops);

	}

	if (ecc_failures != mtd->ecc_stats.failed) {
		printf("%s: ecc failure while reading from %llx\n",
		       __func__, to);
		return -EBADMSG;
	}

	return max_bitflips;
}

/**
 * qti_nandc_read() - read data
 * @start_page: number of page to begin reading from
 * @num_pages: number of pages to read
 * @buffer: buffer where to store the read data
 * @spareaddr: buffer where to store spare data.
 * 		If null, spare data wont be read
 *
 * This function reads @num_pages starting from @start_page and stores the
 * read data in buffer. Note that it's in the caller responsibility to make
 * sure the read pages are all from same partition.
 *
 */
static int qti_nandc_read(struct mtd_info *mtd, loff_t from, size_t len,
             size_t *retlen, u_char *buf)
{
	unsigned ret = 0;
	struct mtd_oob_ops ops;

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;

	ret = qti_read_page_scope(mtd, from, &ops);

	*retlen = ops.retlen;

	return ret;
}

/*
 * Return the buffer containing the in-band data to be written.
 */
static uint8_t *qti_nandc_write_datbuf(struct mtd_info *mtd,
				       struct mtd_oob_ops *ops)
{
	return ops->datbuf + ops->retlen;
}

/*
 * Return the buffer containing the OOB data to be written. If user
 * buffer does not provide on page worth of OOB data, return a padded
 * internal buffer with the OOB data copied in.
 */
static uint8_t *qti_nandc_write_oobbuf(struct mtd_info *mtd,
				       struct mtd_oob_ops *ops)
{
	size_t write_ooblen;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	memset(nandc->pad_oob, 0xFF, nandc->oob_per_page);

	if (ops->oobbuf == NULL)
		return nandc->pad_oob;

	write_ooblen = ops->ooblen - ops->oobretlen;

	if (write_ooblen < nandc->oob_per_page) {
		memcpy(nandc->pad_oob, ops->oobbuf + ops->oobretlen,
								write_ooblen);
		return nandc->pad_oob;
	}

	return ops->oobbuf + ops->oobretlen;
}

/*
 * Increment the counters to indicate the no. of in-band bytes
 * written.
 */
static void qti_nandc_write_datinc(struct mtd_info *mtd,
				   struct mtd_oob_ops *ops)
{
	ops->retlen += mtd->writesize;
}

/*
 * Increment the counters to indicate the no. of OOB bytes written.
 */
static void qti_nandc_write_oobinc(struct mtd_info *mtd,
				   struct mtd_oob_ops *ops)
{
	size_t write_ooblen;
	size_t ooblen;
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	if (ops->oobbuf == NULL)
		return;

	write_ooblen = ops->ooblen - ops->oobretlen;
	ooblen = (write_ooblen < nandc->oob_per_page ? write_ooblen :
							nandc->oob_per_page);

	ops->oobretlen += ooblen;
}

static int qti_nandc_write_oob(struct mtd_info *mtd, loff_t to,
				struct mtd_oob_ops *ops)

{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	int i, ret = NANDC_RESULT_SUCCESS;
	struct nand_chip *chip = mtd_to_nand(mtd);
	u_long start_page;
	u_long num_pages;
	enum nand_cfg_value cfg_mode;

	/* We don't support MTD_OOB_PLACE as of yet. */
	if (ops->mode == MTD_OPS_PLACE_OOB)
		return -ENOSYS;

	/* Check for writes past end of device. */
	if ((to + ops->len) > mtd->size)
		return -EINVAL;

	if (to & (mtd->writesize - 1))
		return -EINVAL;

	if (ops->len & (mtd->writesize - 1))
		return -EINVAL;

	if (ops->ooboffs != 0)
		return -EINVAL;

	if (ops->datbuf == NULL)
		return -EINVAL;

	if(ops->mode == MTD_OPS_RAW) {
		cfg_mode = NAND_CFG_RAW;
		nandc->oob_per_page = mtd->oobsize;
	}
	else {
		cfg_mode = NAND_CFG;
		nandc->oob_per_page = mtd->oobavail;
	}

	start_page = (to >> chip->page_shift);
	num_pages = ((ops->len) >> chip->page_shift);
	ops->retlen = 0;
	ops->oobretlen = 0;

	for (i = 0; i < (int)num_pages; i++) {
		struct mtd_oob_ops page_ops;

		page_ops.mode = ops->mode;
		page_ops.len = mtd->writesize;
		page_ops.ooblen = nandc->oob_per_page;
		page_ops.datbuf = qti_nandc_write_datbuf(mtd,ops);
		page_ops.oobbuf = qti_nandc_write_oobbuf(mtd, ops);
		page_ops.retlen = 0;
		page_ops.oobretlen = 0;

		ret = qti_nandc_write_page(mtd, start_page + i,
				   cfg_mode, &page_ops);
		if (ret) {
			printf("flash_write: write failure @ page %ld, \
					block %ld\n",
					start_page + i,
				(start_page + i) / (nandc->num_pages_per_blk));
			goto out;
		} else {
			qti_nandc_write_datinc(mtd, ops);
			qti_nandc_write_oobinc(mtd, ops);
		}
	}
out:
	return ret;
}

/**
 * qti_nandc_write() - read data
 * @start_page: number of page to begin writing to
 * @num_pages: number of pages to write
 * @buffer: buffer to be written
 * @write_extra_bytes: true if spare data (ox 0xff) to be written
 *
 * This function writes @num_pages starting from @start_page. Note that it's
 * in the caller responsibility to make sure the written pages are all from
 * same partition.
 *
 */
static int qti_nandc_write(struct mtd_info *mtd, loff_t to, size_t len,
     size_t  *retlen, const u_char *buf)
{
	int ret = NANDC_RESULT_SUCCESS;
	struct mtd_oob_ops ops;

	if (!buf) {
		printf("%s: buffer = null\n", __func__);
		return NANDC_RESULT_PARAM_INVALID;
	}

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;

	ret = qti_nandc_write_oob(mtd, to, &ops);
	*retlen = ops.retlen;

	return ret;
}

/* Function to erase a block on the nand.
 * page: Starting page address for the block.
 */
nand_result_t qti_nandc_block_erase(struct mtd_info *mtd, uint32_t page)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct cfg_params cfg;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_read_ptr = nandc->ce_read_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;
	struct cmd_element *cmd_list_read_ptr_start = nandc->ce_read_array;
	uint32_t status;
	int num_desc = 0;
	uint32_t blk_addr = page / (nandc->num_pages_per_blk);
	struct nand_chip *chip = mtd_to_nand(mtd);

	/* Fill in params for the erase flash cmd */
	cfg.addr0 = page;
	cfg.addr1 = 0;
	/* Clear CW_PER_PAGE in cfg0 */
	cfg.cfg0 = nandc->cfg0 & ~(7U << NAND_DEV0_CFG0_CW_PER_PAGE_SHIFT);
	cfg.cfg1 = nandc->cfg1;

	cfg.cmd = NAND_CMD_BLOCK_ERASE;

	/* For serial NAND devices the block erase sequence is
	 * Issue 06H (WRITE ENBALE command)
	 * Issue D8H (BLOCK ERASE command)
	 * Issue 0FH (GET FEATURES command to read the status register)
	 * But we have already mapped write enable command in register
	 * QTI_FLASH_DEV_CMD7 so here no need to send this command manually
	 * once we will send block erase command then controller internally
	 * send write enable command
	 * similar for Get feature command, no neeed to send this command
	 * also manually controller will take care.
	 *
	 * NOTE: Initially we are disabling block protection, so no need
	 * to do it again here.
	 */
	cfg.addr0 = page << 16;
	cfg.addr1 = (page >> 16) & 0xffff;
	cfg.cmd = 0xA;
	cfg.cmd |= (QTI_SPI_WP_SET | QTI_SPI_HOLD_SET |
			QTI_SPI_TRANSFER_MODE_X1);

	cfg.exec = 1;
	cmd_list_ptr = qti_nand_add_cmd_ce(nandc, &cfg, cmd_list_ptr);

	/* Enqueue the desc for the above commands */
	q_bam_add_one_desc(&nandc->bam,
		CMD_PIPE_INDEX,
		(uint8_t*)cmd_list_ptr_start,
		((uintptr_t)cmd_list_ptr - (uintptr_t)cmd_list_ptr_start),
		BAM_DESC_NWD_FLAG | BAM_DESC_CMD_FLAG | BAM_DESC_INT_FLAG |
			BAM_DESC_LOCK_FLAG);

	cmd_list_ptr_start = cmd_list_ptr;
	num_desc++;

	qti_nandc_wait_for_cmd_exec(nandc, num_desc);

	status = qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, 0);

	cmd_list_ptr_start = cmd_list_ptr;
	cmd_list_read_ptr_start = cmd_list_read_ptr;

	/* QTI controller automatically sends
	* GET_STATUS cmd to the nand card because
	* of the configuration programmed.
	* Read the result of GET_STATUS cmd.
	*/
	cmd_list_read_ptr = qti_nand_add_read_ce(nandc, cmd_list_read_ptr,
							(uintptr_t *)&status);

	/* Enqueue the desc for the NAND_FLASH_STATUS read command */
	q_bam_add_one_desc(&nandc->bam,
		CMD_PIPE_INDEX,
		(uint8_t*)cmd_list_read_ptr_start,
		((uintptr_t)cmd_list_read_ptr -
		(uintptr_t)cmd_list_read_ptr_start),
		BAM_DESC_CMD_FLAG);

	cmd_list_ptr = reset_status_ce(nandc, cmd_list_ptr, 1);

	/* Enqueue the desc for NAND_FLASH_STATUS and
	 * NAND_READ_STATUS write commands */
	q_bam_add_one_desc(&nandc->bam,
		CMD_PIPE_INDEX,
		(uint8_t*)cmd_list_ptr_start,
		((uintptr_t)cmd_list_ptr - (uintptr_t)cmd_list_ptr_start),
		BAM_DESC_INT_FLAG | BAM_DESC_CMD_FLAG);
	num_desc = 2;
	qti_nandc_wait_for_cmd_exec(nandc, num_desc);


	status = qti_nandc_check_status(mtd, status);

	/* Dummy read to unlock pipe. */
	qti_nandc_reg_read(mtd, NAND_FLASH_STATUS, BAM_DESC_UNLOCK_FLAG);

	/* Check for status errors*/
	if (status) {
		printf("%s: Erase error: Block address belongs to "
		       "bad block: %d\n", __func__, blk_addr);
		qti_nandc_mark_badblock(mtd, (page << chip->page_shift));
		return status;
	}

	/* Check for PROG_ERASE_OP_RESULT bit
	 * for the result of erase operation. */
	if (!(status & PROG_ERASE_OP_RESULT))
		return NANDC_RESULT_SUCCESS;

	qti_nandc_mark_badblock(mtd, (page << chip->page_shift));
	return status;
}


int
qti_nandc_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int ret = 0, i;

	loff_t offs;
	u_long blocks;
	u_long start;
	u_long pageno;
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);

	/* Check for erase past end of device. */
	if ((instr->addr + instr->len) > mtd->size)
		return -EINVAL;

	if (instr->addr & (mtd->erasesize - 1))
		return -EINVAL;

	if (instr->len & (mtd->erasesize - 1))
		return -EINVAL;

	start = instr->addr >> chip->phys_erase_shift;
	blocks = instr->len >> chip->phys_erase_shift;

	debug("number of blks to erase: %lu\n", blocks);

	for (i = start; i < (start + blocks); i++) {
		schedule();
		offs = i << chip->phys_erase_shift;
		pageno = offs >> chip->page_shift;

		/* Erase only if the block is not bad */
		if (!instr->scrub && qti_nandc_block_isbad(mtd, offs)) {
			printf("%s: Erase error: Block address belongs to "
				"bad block: %ld\n",__func__,
				(pageno / (nandc->num_pages_per_blk)));
			return -EIO;
	}
		ret = qti_nandc_block_erase(mtd, pageno);
		if (ret) {
			instr->fail_addr = offs;
			printf("Erase operation failed \n");
		}
	}
	return ret;
}

void qti_nandc_command(struct mtd_info *mtd, int dat, unsigned int ctrl) {

	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	switch (dat) {
	case NAND_CMD_RESET:
		reset(mtd);
		break;
	case NAND_CMD_READID:
		/*
		 * reset the buffer index
		 */
		nandc->id_buff = 0;
		qti_nandc_get_id(mtd);
		break;
	case NAND_CMD_NONE:
	default:
		break;
	}
	return;
}

uint8_t qti_nandc_read_byte (struct mtd_info *mtd)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	if(nandc->id_buff >= sizeof(nandc->data_buffers))
		nandc->id_buff = 0;
	return nandc->data_buffers[nandc->id_buff++];
}

void
qti_nand_setup(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;

	mtd->_erase = qti_nandc_erase;
#ifndef __UBOOT__
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
#endif
	mtd->_read = qti_nandc_read;
	mtd->_write = qti_nandc_write;
	mtd->_read_oob = qti_nandc_read_oob;
	mtd->_write_oob = qti_nandc_write_oob;
	mtd->_lock = NULL;
	mtd->_unlock = NULL;
	mtd->_block_isbad = qti_nandc_block_isbad;
	mtd->_block_markbad = qti_nandc_mark_badblock;
	mtd->_sync = qti_nandc_sync;

	mtd->ecclayout = NULL;
	mtd->bitflip_threshold = DIV_ROUND_UP(mtd->ecc_strength * 3, 4);

	chip->page_shift = ffs(mtd->writesize) - 1;
	chip->phys_erase_shift = ffs(mtd->erasesize) - 1;

	/* One of the NAND layer functions that the command layer
	 * tries to access directly.
	 */
	chip->scan_bbt = qti_nandc_scan_bbt_nop;
	chip->cmd_ctrl = qti_nandc_command;
	chip->read_byte = qti_nandc_read_byte;
}


static void qti_reg_write_dma(struct qcom_nand_controller *nandc,
			unsigned int reg, unsigned int val)
{
	int num_desc = 0;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;

	bam_add_cmd_element(cmd_list_ptr, reg,
				(uint32_t)val, CE_WRITE_TYPE);
	cmd_list_ptr++;

	q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_CMD_FLAG);
	num_desc++;

	/* Notify BAM HW about the newly added descriptors */
	q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, num_desc);
}
static void qti_set_phase(struct qcom_nand_controller *nandc, int phase)
{
	int spi_flash_cfg_val = 0x0;

	int num_desc = 0;
	struct cmd_element *cmd_list_ptr = nandc->ce_array;
	struct cmd_element *cmd_list_ptr_start = nandc->ce_array;

	if (phase < 1 || phase > 7) {
		printf("%s : wrong phase value\n", __func__);
		return;
	}
	/* get the current value of NAND_FLASH_SPI_CFG register */
	spi_flash_cfg_val = readl(NAND_FLASH_SPI_CFG);
	/* set SPI_LOAD_CLK_CNTR_INIT_EN bit */
	spi_flash_cfg_val |= SPI_LOAD_CLK_CNTR_INIT_EN;

	if ((readl(QTI_NAND_CTRL) & BAM_MODE_EN)) {

		bam_add_cmd_element(cmd_list_ptr,
						NAND_FLASH_SPI_CFG,
						(uint32_t)spi_flash_cfg_val,
						CE_WRITE_TYPE);
		cmd_list_ptr++;

		spi_flash_cfg_val &= 0xf000ffff;
		/* write the phase value for all the line */
		spi_flash_cfg_val |= ((phase << 16) | (phase << 19) |
			(phase << 22) | (phase << 25));

		bam_add_cmd_element(cmd_list_ptr,
						NAND_FLASH_SPI_CFG,
						(uint32_t)spi_flash_cfg_val,
						CE_WRITE_TYPE);
		cmd_list_ptr++;
		/* clear the SPI_LOAD_CLK_CNTR_INIT_EN bit to load the required
		 * phase value
		 */
		spi_flash_cfg_val &= ~SPI_LOAD_CLK_CNTR_INIT_EN;

		bam_add_cmd_element(cmd_list_ptr,
						NAND_FLASH_SPI_CFG,
						(uint32_t)spi_flash_cfg_val,
						CE_WRITE_TYPE);
		cmd_list_ptr++;

		q_bam_add_one_desc(&nandc->bam,
			CMD_PIPE_INDEX,
			(uint8_t*)cmd_list_ptr_start,
			((uintptr_t)cmd_list_ptr -
			(uintptr_t)cmd_list_ptr_start),
			BAM_DESC_CMD_FLAG);
		num_desc++;

		/* Notify BAM HW about the newly added descriptors */
		q_bam_sys_gen_event(&nandc->bam, CMD_PIPE_INDEX, num_desc);
	} else {
		writel(spi_flash_cfg_val, (uintptr_t)NAND_FLASH_SPI_CFG);

		spi_flash_cfg_val &= 0xf000ffff;
		/* write the phase value for all the line */
		spi_flash_cfg_val |= ((phase << 16) | (phase << 19) |
				(phase << 22) | (phase << 25));
		writel(spi_flash_cfg_val, (uintptr_t)NAND_FLASH_SPI_CFG);

		/* clear the SPI_LOAD_CLK_CNTR_INIT_EN bit to load the required
		 * phase value
		 */
		spi_flash_cfg_val &= ~SPI_LOAD_CLK_CNTR_INIT_EN;
		writel(spi_flash_cfg_val, (uintptr_t)NAND_FLASH_SPI_CFG);
	}
}

static bool IsEven(int num)
{
	return (!(num & 1));
}

static int qti_find_most_appropriate_phase(u8 *phase_table, int phase_count)
{
	unsigned int cnt = 0, i;
	int phase = 0x0;
	u8 phase_ranges[TOTAL_NUM_PHASE] = {'\0'};

	/*currently we are considering continious 3 phase will
	 * pass and tke the middle one out of passed three phase.
	 * if all 7 phase passed so return middle phase i.e 4
	 */
	phase_count -= 2;
	for (i = 0; (i >= 0) && (i < phase_count); i++) {
		if ((phase_table[i] + 1 == phase_table[i + 1]) &&
				(phase_table[i + 1] + 1 == phase_table[i + 2]))
		{
			phase_ranges[cnt++] = phase_table[i + 1];
		}
	}

	/* filter out middle phase
	 * if cnt is odd then one middle phase
	 * if cnt is even then two middile phase
	 * so select lower one
	 */
	if (IsEven(cnt)) {
		phase = phase_ranges[cnt/2 - 1];
	} else {
		phase = phase_ranges[cnt/2];
	}

	return phase;
}

static int qti_serial_training(struct mtd_info *mtd)
{
	struct qcom_nand_controller *nandc = MTD_QTI_NAND_DEV(mtd);
	struct nand_chip *chip = mtd_to_nand(mtd);

	uint32_t start, blk_cnt = 0;
	uint32_t offset, pageno, curr_freq, size;
	uint32_t max_freaqueny = 0;
	int i;
	unsigned int io_macro_freq_tbl[] = {24000000, 100000000, 200000000,
						320000000, 400000000};

	uint8_t *data_buff, trained_phase[TOTAL_NUM_PHASE] = {'\0'};
	int phase, phase_cnt;
	int training_seq_cnt = 5;
	int index = 4, ret, phase_failed=0;
	loff_t training_offset;

	max_freaqueny = dev_read_u32_default(mtd->dev, "max_clk_feaquency",
						320000000);

	for(int cnt =  sizeof(io_macro_freq_tbl)/ sizeof(unsigned int) - 1;
		cnt >= 0;
		--cnt)
	{
		if (io_macro_freq_tbl[cnt] == max_freaqueny) {
			index = cnt;
			training_seq_cnt = cnt + 1;
			break;
		}
	}

	ret = ipq_get_training_part_info(&offset, &size);
	if (ret) {
		printf("Serial Training part offset not found.\n");
		return -EIO;
	}

	training_offset = (loff_t)offset;
	start = (training_offset >> chip->phys_erase_shift);
	offset = (start << chip->phys_erase_shift);
	pageno = (offset >> chip->page_shift);

	/* At 50Mhz frequency check the bad blocks, if training
	 * blocks is not bad then only start training else operate
	 * at 50Mhz with bypassing software serial traning.
	 */
	while (qti_nandc_block_isbad(mtd, offset)) {
		/* block is bad skip this block and goto next
		 * block
		 */
		training_offset += mtd->erasesize;
		start = (training_offset >> chip->phys_erase_shift);
		offset = (start << chip->phys_erase_shift);
		pageno = (offset >> chip->page_shift);
		blk_cnt++;
		if (blk_cnt == MAXIMUM_ALLOCATED_TRAINING_BLOCK)
			break;
	}

	if (blk_cnt == MAXIMUM_ALLOCATED_TRAINING_BLOCK) {
		printf("All training blocks are bad skipping serial training\n");
		ret = -EIO;
		goto err;
	}
	ret = qti_nandc_block_erase(mtd, pageno);
	if (ret) {
		printf("Error in erasing training block @%x\n",offset);
		ret = -EIO;
		goto err;
	}

	data_buff = (uint8_t *)malloc_cache_aligned(mtd->writesize);
	if (!data_buff) {
		printf("%s No enough memory\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	/* prepare clean buffer */
	memset(data_buff, 0xff, mtd->writesize);
	for (i = 0; i < mtd->writesize; i += sizeof(nandc->training_block_64))
		memcpy(data_buff + i, nandc->training_block_64,
			sizeof(nandc->training_block_64));

	/*write training data to flash */
	ret = NANDC_RESULT_SUCCESS;
	struct mtd_oob_ops ops;

	/* write this dumy byte in spare area to avoid bam
	 * transaction error while writing.
	 */
	memset(nandc->pad_oob, 0xFF, nandc->oob_per_page);

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len =  mtd->writesize;
	ops.retlen = 0;
	ops.ooblen = nandc->oob_per_page;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = (uint8_t *)data_buff;
	ops.oobbuf = (uint8_t *)nandc->pad_oob;

	/* write should be only once */
	ret = qti_nandc_write_page(mtd, pageno, NAND_CFG, &ops);
	if (ret) {
		printf("Error in writing training data..\n");
		goto free;
	}
	/* After write verify the the data with read @ lower frequency
	 * after that only start serial tarining @ higher frequency
	 */
	memset(data_buff, 0xff, mtd->writesize);
	ops.datbuf = (uint8_t *)data_buff;

	ret = qti_read_page(mtd, pageno, NAND_CFG, &ops);
	if (ret) {
		printf("%s:Read training data failed before training start\n",
								__func__);
		goto free;
	}

	/* compare original data and read data */
	for (i = 0; i < mtd->writesize; i += sizeof(nandc->training_block_64)) {
		if (memcmp(data_buff + i, nandc->training_block_64,
					sizeof(nandc->training_block_64))) {
			printf("Training data read failed @ lower frequency\n");
			goto free;
		}
	}

	/* disable feed back clock bit to start serial training */
	qti_reg_write_dma(nandc, NAND_QSPI_MSTR_CONFIG,
			(~FB_CLK_BIT & readl(NAND_QSPI_MSTR_CONFIG)));

	/* start serial training here */
	curr_freq = io_macro_freq_tbl[index];
rettry:
	phase = 1;
	phase_cnt = 0;

	/* set frequency, start from higer frequency */
	clk_set_rate(&nandc->clk, curr_freq);

	do {
		/* set the phase */
		qti_set_phase(nandc, phase);

		memset(data_buff, 0, mtd->writesize);
		ops.datbuf = (uint8_t *)data_buff;

		ret = qti_read_page(mtd, pageno, NAND_CFG, &ops);
		if (ret) {
			printf("%s:Read training data failed.\n",__func__);
			goto free;
		}
		/* compare original data and read data */
		for (i = 0; i < mtd->writesize;
					i += sizeof(nandc->training_block_64)) {
			if (memcmp(data_buff + i, nandc->training_block_64,
					sizeof(nandc->training_block_64))) {
				phase_failed++;
				break;
			}
		}
		if (i == mtd->writesize)
			trained_phase[phase_cnt++] = phase;

	} while (phase++ < TOTAL_NUM_PHASE);

	if (phase_cnt) {
		/* Get the appropriate phase */
		phase = qti_find_most_appropriate_phase(trained_phase,
								phase_cnt);
		qti_set_phase(nandc, phase);

	} else {
		/* lower the the clock frequency
		 * and try again
		 */
		curr_freq = io_macro_freq_tbl[--index];
		printf("Retry with lower frequency @:%d\n",curr_freq);
		if (--training_seq_cnt)
			goto rettry;

		/* Training failed */
		printf("%s : Serial training failed\n",__func__);
		ret = -EIO;
		goto free;
	}

	/* if phase_failed == 7 it means serial traing failed
	 * on all the phase. so now we have to go via line by line
	 * i.e first check for MISO_0, with all the phase value i.e
	 * 1-7 and then MISO_1 and so on.
	 * NOTE: But this is the worse case , and it this type of senario
	 * will not come. if it will come then go with this design.
	 * ======To DO=====
	 */
free:
	data_buff ? free(data_buff) : NULL;
err:
	return ret;
}


static int qti_nand_probe(struct udevice *device)
{
	struct qcom_nand_controller *nandc = dev_get_priv(device);
	struct mtd_info *mtd = &nandc->mtd;
	struct nand_chip *chip;
	int val, ret = 0;
	size_t alloc_size;
	uint8_t *buf;
	struct qti_nand_init_config config;
	fdt_addr_t nand_base;
	uint8_t *buff;
	unsigned buf_size;
	uint32_t bam_base;
	unsigned read_pipe;
	unsigned write_pipe;
	unsigned cmd_pipe;
	unsigned status_pipe;
	uint8_t  read_pipe_grp;
	uint8_t  write_pipe_grp;
	uint8_t  cmd_pipe_grp;
	uint8_t status_pipe_grp;
#ifdef CONFIG_QSPI_LAYOUT_SWITCH
	char *env_layout = env_get("nand_layout");
#endif
	struct nand_flash_dev *nand_device_list = qti_nand_flash_ids;


	/*
	 * An array holding the fixed pattern to compare with
	 * training pattern.
	 */
	uint32_t training_block_64[] = {
		0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F,
		0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F,
		0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F,
		0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F, 0x0F0F0F0F,
	};

	uint32_t qti_onfi_mode_to_xfer_steps
		[QTI_MAX_ONFI_MODES][QTI_NUM_XFER_STEPS] = {

		/* Mode 0 */
		{
			0x00e00080, 0x49f04998, 0x8de08d80, 0xc000c000,
			0xc000c000, 0xc000c000, 0xc000c000,
		},
		/* Mode 1 */
		{
			0x00e00080, 0x49f04d99, 0x85e08580, 0xd000d000,
			0xc000c000, 0xc000c000, 0xc000c000,
		},
		/* Mode 2 */
		{
			0x00e00080, 0x45f0459a, 0x85e08580, 0xd000d000,
			0xc000c000, 0xc000c000, 0xc000c000,
		},
		/* Mode 3 */
		{
			0x00e00080, 0x45f04599, 0x81e08180, 0xd000d000,
			0xc000c000, 0xc000c000, 0xc000c000,
		},
	};

	mtd->priv = &nandc->nand_chip[0];
	mtd->dev = device;

	chip = mtd->priv;
	chip->priv = nandc;

	nandc->varient_info = (void *)dev_get_driver_data(device);
	if(!nandc->varient_info)
		return -EINVAL;

	/*Read register base  from dts*/
	nand_base = dev_read_addr(device);
	if (nand_base == FDT_ADDR_T_NONE) {
		printf("No valid NAND base address found in device tree\n");
		return -EINVAL;
        }

	nandc->base = nand_base;
	ebi2nd_base = nand_base;
#ifdef CONFIG_QSPI_LAYOUT_SWITCH
	if(env_layout && !strcmp(env_layout, "sbl")) {
		nandc->do_serial_training = false;
		nand_device_list = qpic_nand_flash_ids_2k;
	}
#endif

	/* Read the Hardware Version register */
	nandc->hw_ver = readl(NAND_VERSION);

	bam_base = dev_read_u32_default(device, "bam_reg", QTI_BAM_CTRL_BASE);
	cmd_pipe = dev_read_u32_default(device, "cmd_pipe", CMD_PIPE);
	cmd_pipe_grp = dev_read_u32_default(device, "cmd_pipe_grp",
							CMD_PIPE_GRP);
	status_pipe = dev_read_u32_default(device, "status_pipe",
					NAND_BAM_STATUS_PIPE);
	status_pipe_grp = dev_read_u32_default(device, "status_pipe_grp",
					NAND_BAM_STATUS_PIPE_GRP);
	read_pipe = dev_read_u32_default(device, "read_pipe",
					DATA_PRODUCER_PIPE);
	read_pipe_grp = dev_read_u32_default(device, "read_pipe_grp",
					DATA_PRODUCER_PIPE_GRP);
	write_pipe = dev_read_u32_default(device, "write_pipe",
					DATA_CONSUMER_PIPE);
	write_pipe_grp = dev_read_u32_default(device, "write_pipe_grp",
					DATA_CONSUMER_PIPE_GRP);

	nandc->do_serial_training = dev_read_bool(device, "serial_training");

	ret = clk_get_by_name(device, "qpic-io-macro-clk", &nandc->clk);
	if (ret)
		return ret;

	ret = clk_get_bulk(device, &(nandc->blk_clk));
	if (ret && ret != -ENOENT)
		return ret;

	ret = clk_enable_bulk(&(nandc->blk_clk));
	if(ret)
		return ret;

	buf_size = 0;
	buf_size += sizeof(struct bam_desc) * QTI_BAM_CMD_FIFO_SIZE
						+ CONFIG_SYS_CACHELINE_SIZE;
	buf_size += sizeof(struct bam_desc) * QTI_BAM_DATA_FIFO_SIZE
						+ CONFIG_SYS_CACHELINE_SIZE;
	buf_size += sizeof(struct bam_desc) * QTI_BAM_STATUS_FIFO_SIZE
						+ CONFIG_SYS_CACHELINE_SIZE;
	buf_size += sizeof(struct cmd_element) * QTI_MAX_NO_CMD_ELEMENT
						+ CONFIG_SYS_CACHELINE_SIZE;
	buf_size += sizeof(struct cmd_element) * QTI_MAX_NO_CMD_ELEMENT
						+ CONFIG_SYS_CACHELINE_SIZE;
	val = sizeof(uint32_t) * QTI_NAND_MAX_CWS_IN_PAGE;
	buf_size += _roundup(val); /* cache allignment */
	buf_size += _roundup(4); /* cache allignment */

	buff = malloc_cache_aligned(buf_size);
	if (buff == NULL) {
		printf("qti_nand: failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_buf;
	}

	nandc->qti_cmd_desc_fifo = (struct bam_desc*)buff;
	buff += sizeof(struct bam_desc) * QTI_BAM_CMD_FIFO_SIZE
						+ CONFIG_SYS_CACHELINE_SIZE;

	nandc->qti_data_desc_fifo = (struct bam_desc*)buff;
	buff += sizeof(struct bam_desc) * QTI_BAM_DATA_FIFO_SIZE
						+ CONFIG_SYS_CACHELINE_SIZE;

	nandc->qti_status_desc_fifo = (struct bam_desc*)buff;
	buff += sizeof(struct bam_desc) * QTI_BAM_STATUS_FIFO_SIZE
						+ CONFIG_SYS_CACHELINE_SIZE;

	nandc->ce_array = (struct cmd_element*)buff;
	buff += sizeof(struct cmd_element) * QTI_MAX_NO_CMD_ELEMENT
						+ CONFIG_SYS_CACHELINE_SIZE;

	nandc->ce_read_array = (struct cmd_element*)buff;
	buff += sizeof(struct cmd_element) * QTI_MAX_NO_CMD_ELEMENT
						+ CONFIG_SYS_CACHELINE_SIZE;

	nandc->nandc_buffer = (uint32_t*)buff;
	buff += _roundup(val);

	nandc->reg_buffer = (uint32_t*)buff;

	memset(nandc->qti_cmd_desc_fifo, 0x0, buf_size);

	memcpy(nandc->training_block_64, training_block_64,
				sizeof(training_block_64));

	memcpy(nandc->qti_onfi_mode_to_xfer_steps,
			qti_onfi_mode_to_xfer_steps,
			sizeof(uint32_t)*QTI_MAX_ONFI_MODES*QTI_NUM_XFER_STEPS);

	config.pipes.status_pipe = status_pipe;
	config.pipes.status_pipe_grp = status_pipe_grp;

	config.pipes.read_pipe = read_pipe;
	config.pipes.write_pipe = write_pipe;
	config.pipes.cmd_pipe = cmd_pipe;

	config.pipes.read_pipe_grp = read_pipe_grp;
	config.pipes.write_pipe_grp = write_pipe_grp;
	config.pipes.cmd_pipe_grp = cmd_pipe_grp;

	config.bam_base = bam_base;
	config.nand_base = nand_base;
	config.ee = QTI_NAND_EE;
	config.max_desc_len = QTI_NAND_MAX_DESC_LEN;

	qti_bam_init(nandc, &config);

	qti_spi_init(mtd);

	qti_nand_setup(mtd);

	/* first scan to find the device and get the page size */
	ret = nand_scan_ident(mtd, CONFIG_SYS_NAND_MAX_CHIPS,
					nand_device_list);
	if (ret) {
		ret = nand_scan_ident(mtd, CONFIG_SYS_NAND_MAX_CHIPS, NULL);
		if (ret) {
			printf("%s: nand_scan_ident failed\n", __func__);
			return ret;
		}
	}

	qti_serial_update_dev_params(mtd);

	/* Save the RAW and read/write configs */
	ret = qti_nand_save_config(mtd);
	if (ret < 0)
		return ret;

	/* Check all blocks of serial NAND device is unlocked or
	 * not if not then unlock the all the blocks of serial NAND
	 * device also check the internal ecc is enabled or not if internal
	 * ecc is enabled then disable internal ecc using get/set feature
	 * command.
	 */
	ret = qti_spi_nand_config(mtd);
	if (ret < 0) {
		printf("%s : Issue with Serial Nand configuration.\n",__func__);
		return ret;
	}

	/* allocate memory for status buffer. we are doing
	 * this here because we do not know the device page
	 * siz ein advance if nand flash is parallel nand and ONFI
	 * complaint. so status buffer size will vary based on page size
	 * e.g if page size is 2KiB then status buffer size for one page
	 * will be 48-bytes similary for 4KiB page , status buffer size
	 * will be 96-bytes for one page and so on.
	 * QTI controller support max page isze is 8 KiB now so maximum
	 * status buffer size for one page will be 192-bytes. for multi page
	 * read the status buffer size will be multiple of maximum pages
	 * supported in multipage.
	 */
	ret = qti_alloc_status_buff(nandc, mtd);
	if (ret) {
		printf("Error in allocating status buffer\n");
		return ret;
	}

	/*
	 * allocate buffer for nandc->pad_dat, nandc->pad_oob, nandc->zero_page,
	 * nandc->zero_oob, nandc->tmp_datbuf, nandc->tmp_oobbuf
	 *
	 */

	alloc_size = 3 * (mtd->writesize + (mtd->oobsize * MAX_MULTI_PAGE));

	nandc->buffers = malloc_cache_aligned(alloc_size);
	if (nandc->buffers == NULL) {
		printf("qti_nand: failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_buf;
	}
	buf = nandc->buffers;

	nandc->pad_dat = buf;
	buf += mtd->writesize;
	nandc->pad_oob = buf;

	buf += mtd->oobsize * MAX_MULTI_PAGE;

	nandc->zero_page = buf;
	buf += mtd->writesize;

	nandc->zero_oob = buf;
	buf += mtd->oobsize;

	memset(nandc->zero_page, 0x0, mtd->writesize);
	memset(nandc->zero_oob, 0x0, mtd->oobsize);

	nandc->tmp_datbuf = buf;
	buf += mtd->writesize;
	nandc->tmp_oobbuf = buf;
	buf += mtd->oobsize;

	/* Register with MTD subsystem. */
	ret = nand_register(dev_seq(device), mtd);
	if (ret < 0) {
		printf("qti_nand: failed to register with MTD subsystem\n");
		goto err_reg;
	}

	if (nandc->do_serial_training) {
		/* start serial training here */
		ret = qti_serial_training(mtd);
	} else {
		ret = 0;
		printf("Skipping Serial trainig\n");
	}

	if (ret) {
		printf("switch back to 50MHz with \n"
			"feed back clock bit enabled\n");
		if ((readl(QTI_NAND_CTRL) & BAM_MODE_EN)) {
			qti_reg_write_dma(nandc,NAND_QSPI_MSTR_CONFIG,
				(FB_CLK_BIT | readl(NAND_QSPI_MSTR_CONFIG)));
			clk_set_rate(&nandc->clk, DEFAULT_CLK_200_MHZ);
			qti_reg_write_dma(nandc,NAND_FLASH_SPI_CFG, 0x0);
			qti_reg_write_dma(nandc, NAND_FLASH_SPI_CFG,
					SPI_CFG_VAL);
			qti_reg_write_dma(nandc, NAND_FLASH_SPI_CFG,
					(SPI_CFG_VAL &
					~SPI_LOAD_CLK_CNTR_INIT_EN));

		} else {
			writel((FB_CLK_BIT | readl(NAND_QSPI_MSTR_CONFIG)),
				(uintptr_t)NAND_QSPI_MSTR_CONFIG);

			clk_set_rate(&nandc->clk, DEFAULT_CLK_200_MHZ);
			writel(0x0, (uintptr_t)NAND_FLASH_SPI_CFG);
			writel(SPI_CFG_VAL, (uintptr_t)NAND_FLASH_SPI_CFG);
			writel((SPI_CFG_VAL & ~SPI_LOAD_CLK_CNTR_INIT_EN),
				(uintptr_t)NAND_FLASH_SPI_CFG);
		}
	}

	return 0;
err_reg:
err_buf:
	nandc->qti_cmd_desc_fifo ? free(nandc->qti_cmd_desc_fifo) : NULL;
	nandc->status_buff ? free(nandc->status_buff) : NULL;
	nandc->buffers ? free(nandc->buffers) : NULL;
	return ret;
}

static const struct target_varient_info qpic_v_2_1_1_info = {
	.bam_cfg			= 0xffffffff & ~(1 << 11),
	.bam_threshold_reg_write	= 1,
};

static const struct target_varient_info qpic_v_2_1_3_info = {
	.bam_cfg			= 0x18003000,
	.bam_threshold_reg_write	= 0,
};

static const struct udevice_id qti_ver_ids[] = {
	{ .compatible = "qti,spi-nand-v2.1.1",
		.data = (ulong)&qpic_v_2_1_1_info},
	{ .compatible = "qti,spi-nand-v2.1.3",
		.data = (ulong)&qpic_v_2_1_3_info},
	{ },
};

U_BOOT_DRIVER(qti_nand) = {
	.name = "qti_nand",
	.id = UCLASS_MTD,
	.of_match = qti_ver_ids,
	.probe = qti_nand_probe,
	.remove = qti_nand_deinit,
	.priv_auto = sizeof(struct qcom_nand_controller),
};

int qti_nand_deinit(struct udevice *device)
{
	int ret = 0;
	struct qcom_nand_controller *nandc = NULL;
	struct mtd_info *mtd = get_nand_dev_by_index(dev_seq(device));
	if(!mtd) {
		printf("%s: mtd device not available\n", __func__);
		return -ENOMEM;
	}

	nandc = MTD_QTI_NAND_DEV(mtd);
	if(!nandc) {
		printf("%s: nand controller not available\n", __func__);
		return -ENOMEM;
	}

	ret = del_mtd_device(mtd);
	if (ret < 0)
		return ret;

	nandc->cfg0 = 0;
	nandc->cfg1 = 0;
	nandc->ecc_bch_cfg = 0;
	nandc->qti_cmd_desc_fifo ? free(nandc->qti_cmd_desc_fifo) : NULL;
	nandc->status_buff ? free(nandc->status_buff) : NULL;
	nandc->buffers ? free(nandc->buffers) : NULL;

	return ret;
}

