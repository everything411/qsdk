// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2012,2015-2018 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <common.h>
#include <cpu_func.h>
#include <asm/io.h>
#include <dm.h>
#include <clk.h>
#include <dma.h>
#include <dma-uclass.h>
#include <errno.h>
#include <memalign.h>
#include <dm/device_compat.h>
#include <linux/soc/ipqsoc/bam_dma.h>

#define BAM_CTRL_REG(b)					(0x0000 + (b))
#define BAM_DESC_CNT_TRSHLD_REG(b)			(0x0008 + (b))
#define BAM_CNFG_BITS(b)				(0x0000007C + (b))
#define BAM_NUM_PIPES(b)				(0x00001008 + (b))

#define BAM_IRQ_SRCS(b, n)              (0x00003000 + 0x1000 * (n) + (b))
#define BAM_IRQ_SRCS_MSK(b, n)          (0x00003004 + 0x1000 * (n) + (b))
#define BAM_P_CTRLn(b, n)               (0x00013000 + 0x1000 * (n) + (b))
#define BAM_P_RSTn(b, n)                (0x00013004 + 0x1000 * (n) + (b))
#define BAM_P_IRQ_STTSn(b, n)           (0x00013010 + 0x1000 * (n) + (b))
#define BAM_P_IRQ_CLRn(b, n)            (0x00013014 + 0x1000 * (n) + (b))
#define BAM_P_IRQ_ENn(b, n)             (0x00013018 + 0x1000 * (n) + (b))
#define BAM_P_SW_OFSTSn(b, n)           (0x00013800 + 0x1000 * (n) + (b))
#define BAM_P_EVNT_REGn(b, n)           (0x00013818 + 0x1000 * (n) + (b))
#define BAM_P_DESC_FIFO_ADDRn(b, n)     (0x0001381C + 0x1000 * (n) + (b))
#define BAM_P_FIFO_SIZESn(b, n)         (0x00013820 + 0x1000 * (n) + (b))

#define BAM_NUM_PIPES_MSK				(0xFF)
#define BAM_ENABLE_BIT_MASK				BIT(1)
#define BAM_IRQ_SRCS_PIPE_MASK				0x7FFF
#define BAM_IRQ_TIMEOUT_IN_MS				2000

#define P_DIRECTION_SHIFT				3
#define P_LOCK_GRP_SHIFT				16
#define P_SYS_MODE_MASK					BIT(5)
#define P_ENABLE					BIT(1)


/* Enum to define the BAM type:
 * BAM2BAM:Producer BAM to Consumer BAM.
 * SYS2BAM:Producer System to Consumer BAM.
 * BAM2SYS:Producer BAM to Consumer System.
 */
enum bam_transaction_type {
        SYS2BAM,
        BAM2SYS,
        BAM2BAM,
};

/* Pipe Interrupt masks */
enum p_int_type
{
        P_PRCSD_DESC_EN_MASK = 1,
        P_OUT_OF_DESC_EN_MASK = (1 << 3),
        P_ERR_EN_MASK = (1 << 4),
        P_TRNSFR_END_EN_MASK = (1 << 5)
};


/* Structure to define BAM descriptors that describe the data
 * descriptors written to the data FIFO.
 * addr:Descriptor address.
 * size:Each descriptor is 8 bytes. Size of the descriptor fifo must
 *      contain an integer number of Descriptors.
 */
struct bam_desc {
        uint32_t addr;
        uint16_t size;
        uint8_t reserved;
        uint8_t flags;
} __attribute__ ((packed));

#define BAM_DESC_SIZE					sizeof(struct bam_desc)

struct bam_desc_fifo {
        struct bam_desc *head;
        struct bam_desc *current;
        uint16_t offset;
};

struct bam_dma_pipe {
	struct bam_dma_dev *pdev;
	char name[20];
	u8 pipe_num;
	u16 threshold;
	u16 ee;
	u16 max_desc_len;
	u16 fifo_size;
	u32 rx_len;
	uint8_t lock_grp;
	struct bam_desc_fifo fifo;
	enum bam_transaction_type dir;
	bool in_use;
	bool enabled;
};

/**
 * struct bam_dma_dev - Private structure
 * @dev: Device uclass for video_ops
 */
struct bam_dma_dev {
	struct udevice *dev;
	struct bam_dma_pipe **pipe;
	phys_addr_t base;
	struct clk clk;
	u32 n_pipe;
};

static inline int ispow2(uint val)
{
        return ((val - 1) & val) == 0;
}

/* Function to get the next desc address.
 * Keeps track of circular properties of the FIFO
 * and returns the appropriate address.
 */
static struct bam_desc* fifo_getnext(struct bam_desc_fifo *fifo, u32 fifo_size,
                                     struct bam_desc* desc)
{
	uint16_t offset;

	offset = desc - fifo->head;

	if (offset == (fifo_size - 1))
		return fifo->head;
	else
		return desc + 1;
}

/* A blocking function that waits till an interrupt is signalled.
 * bam : BAM instance for the descriptors to be queued.
 * pipe_num : pipe number for the descriptors to be queued.
 * interrupt: interrupt to wait for.
 */
int bam_wait_for_interrupt(struct bam_dma_pipe *pipe,
		enum p_int_type interrupt)
{
	struct bam_dma_dev *priv = pipe->pdev;
	uint32_t val;
	uint32_t start;

	while (1)
	{
		start = get_timer(0);
		/* Wait for a interrupt on the right pipe */
		do{
			/* Determine the pipe causing the interrupt */
			val = readl(BAM_IRQ_SRCS(priv->base, pipe->ee));
			if(get_timer(start) >= BAM_IRQ_TIMEOUT_IN_MS)
				return -1;
			/* Flush out the right most global interrupt bit */
		} while (!((val & BAM_IRQ_SRCS_PIPE_MASK) &
				   (1 << pipe->pipe_num)));

		/* Check the interrupt type */
		/* Read interrupt status register */
		val = readl(BAM_P_IRQ_STTSn(priv->base, pipe->pipe_num));

		/* Check for error */
		if (val & P_ERR_EN_MASK)
			goto bam_wait_int_error;

		if (val & interrupt)
		{
			/* Correct interrupt was fired. */
			 /* Clear the other interrupts */
			val = P_OUT_OF_DESC_EN_MASK | P_PRCSD_DESC_EN_MASK
				| P_TRNSFR_END_EN_MASK;
			writel (val, BAM_P_IRQ_CLRn(priv->base,
						pipe->pipe_num));
			return 0;
		}
	}

bam_wait_int_error:

	printf("bam: Unexpected interrupt : val %u\n", val);
	return -1;
}

/* Function to read the updates for FIFO offsets.
 * bam : BAM that uses the FIFO.
 * pipe : BAM pipe that uses the FIFO.
 * return : void.
 * Note : As per IPCAT This register denotes the pointer Offset of the first
 *	  un-Acknowledged Descriptor. This register is only used by the
 *	  Software. After receiving an interrupt, software reads this register
 *        in order to know what descriptors has been processed. Although being
 *        Writable, Software should never write to this register.
 */
void bam_read_offset_update(struct bam_dma_pipe *pipe)
{
	struct bam_dma_dev *priv = pipe->pdev;
	uint32_t offset;

	offset = readl(BAM_P_SW_OFSTSn(priv->base, pipe->pipe_num));
	offset &= 0xFFFF;
}

/* Function to notify written descriptors to BAM.
 * bam : BAM instance for the descriptors to be queued.
 * pipe_num : pipe number for the descriptors to be queued.
 * num_desc : number of the descriptors.
 * fifo : Circular FIFO used for the descriptors.
 */
void bam_sys_gen_event(struct bam_dma_pipe *pipe, unsigned int num_desc)
{
	struct bam_dma_dev *priv = pipe->pdev;
	uint32_t val = 0;

	if (num_desc >= pipe->fifo_size) {
		printf("bam: Max allowed desc is one less than "
				"the fifo length\n");
		return;
	}

	/* bits 0:15 of BAM_P_EVNT_REGn denotes the offset. We read the offset,
	 * and update the offset to notify BAM HW that new descriptors
	 * have been written
	 */
	val = readl(BAM_P_EVNT_REGn(priv->base, pipe->pipe_num));

	/* Update the fifo peer offset */
	val += (num_desc) * BAM_DESC_SIZE;
	val &= (pipe->fifo_size * BAM_DESC_SIZE - 1);

	writel(val, BAM_P_EVNT_REGn(priv->base, pipe->pipe_num));
}

/* Function to add a BAM descriptor for a given fifo.
 * bam : BAM instance to be used.
 * data_ptr : Memory address for data transfer.
 * data_len : Length of the data_ptr.
 * flags : Flags to be set on the desc added.
 *
 * Note: This function does not notify the BAM about the added descriptor.
 */
int bam_add_one_desc(struct bam_dma_pipe *pipe,
                     unsigned char* data_ptr,
                     uint32_t len,
                     uint8_t flags)
{
	struct bam_desc *desc = pipe->fifo.current;
	int ret = 0;

	if (data_ptr == NULL || len == 0) {
		printf("bam: Wrong params for BAM transfer \n");
		ret = -EINVAL;
		goto bam_add_one_desc_error;
	}

	if ((flags & BAM_DESC_LOCK_FLAG) && (flags & BAM_DESC_UNLOCK_FLAG)) {
		printf("bam: Can't lock and unlock in the same desc\n");
		ret = -EINVAL;
		goto bam_add_one_desc_error;
	}

	/* Setting EOT flag on a CMD desc is not valid */
	if ((flags & BAM_DESC_EOT_FLAG) && (flags & BAM_DESC_CMD_FLAG)) {
		printf("bam: EOT flag set on the CMD desc\n");
		ret = -EINVAL;
		goto bam_add_one_desc_error;
	}

	/* Check for the length of the desc. */
	if (len > pipe->max_desc_len) {
		printf("bam: len of the desc exceeds max length"
				" %d > %d\n", len, pipe->max_desc_len);
		ret = -EINVAL;
		goto bam_add_one_desc_error;
	}

	desc->flags    = flags;
	desc->addr     = (uintptr_t)data_ptr;
	desc->size     = (uint16_t)len;
	desc->reserved = 0;

	/* Update the FIFO to point to the head */
	pipe->fifo.current = fifo_getnext(&pipe->fifo, pipe->fifo_size, desc);

bam_add_one_desc_error:
	return ret;
}

static int bam_dma_of_xlate(struct dma *dma,
				struct ofnode_phandle_args *args)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;

	debug("%s(dma id=%u)\n", __func__, args->args[0]);

	if (args->args[0] >= priv->n_pipe)
		return -EINVAL;

	dma->id = args->args[0];
	priv->pipe[dma->id] = malloc(sizeof(struct bam_dma_pipe));
	if (!priv->pipe[dma->id])
		return -ENOMEM;

	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	pipe->pipe_num = dma->id;

	switch (args->args[1]) {
	case DMA_MEM_TO_DEV:
		pipe->dir = SYS2BAM;
		break;
	case DMA_DEV_TO_MEM:
		pipe->dir = BAM2SYS;
		break;
	default:
		return -EINVAL;
	}

	pipe->threshold = args->args[2];
	pipe->ee = args->args[3];
	pipe->max_desc_len = args->args[4];
	pipe->fifo_size = args->args[5];
	pipe->pdev = priv;
	debug("%s(dma id=%lu dir=%d)\n", __func__, dma->id, pipe->dir);
	return 0;
}

static int bam_dma_request(struct dma *dma)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;

	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (pipe->in_use)
		return -EBUSY;

	pipe->fifo.head = malloc_cache_aligned(
			sizeof(struct bam_desc) * pipe->fifo_size);
	if (!pipe->fifo.head)
		return -ENOMEM;

	pipe->in_use = true;
	debug("%s(dma id=%lu in_use=%d)\n", __func__, dma->id, pipe->in_use);

	return 0;
}

static int bam_dma_rfree(struct dma *dma)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;

	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (!pipe->in_use)
		return -EBUSY;

	pipe->in_use = false;
	debug("%s(dma id=%lu in_use=%d)\n", __func__, dma->id, pipe->in_use);

	return 0;
}

static int bam_dma_enable(struct dma *dma)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;
	u32 val = 0, int_mask = P_ERR_EN_MASK | P_OUT_OF_DESC_EN_MASK |
		P_PRCSD_DESC_EN_MASK;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;

	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (!pipe->in_use)
		return -EBUSY;
	if (pipe->enabled)
		return -EINVAL;

	/* Program the threshold count */
	writel(pipe->threshold, BAM_DESC_CNT_TRSHLD_REG(priv->base));

	/* Program config register for H/W bug fixes */
	val = 0xffffffff & ~(1 << 11);
	writel(val, BAM_CNFG_BITS(priv->base));

	/* Enable the BAM */
	val = readl(BAM_CTRL_REG(priv->base));
	val = val | BAM_ENABLE_BIT_MASK;
	writel(val, BAM_CTRL_REG(priv->base));

	debug("initializating pipe %ld \n", dma->id);
	/* Start sw reset of the pipe to be allocated */
	writel(1, BAM_P_RSTn(priv->base, dma->id));

	/* No delay required */

	/* Stop sw reset of the pipe to be allocated */
	writel(0, BAM_P_RSTn(priv->base, dma->id));

	/* Leave BAM error interrupts disabled. */
	/* Enable the interrupts for the pipe by enabling the relevant bits
	 * in the BAM_PIPE_INTERRUPT_ENABLE register.
	 */
	writel(int_mask, BAM_P_IRQ_ENn(priv->base, dma->id));

	/* Enable pipe interrups */
	/* Do read-modify-write */
	val = readl(BAM_IRQ_SRCS_MSK(priv->base, pipe->ee));
	writel((1 << dma->id) | val, BAM_IRQ_SRCS_MSK(priv->base, pipe->ee));

	/* Pipe event threshold register is not relevant in sys modes */
	writel(P_SYS_MODE_MASK | pipe->lock_grp <<  P_LOCK_GRP_SHIFT |
			(pipe->dir << P_DIRECTION_SHIFT),
			BAM_P_CTRLn(priv->base, dma->id));

	debug("initializating pipe %ld fifo \n", dma->id);
        if (pipe->fifo_size > 0x7FFF) {
                printf("bam: Size exceeds max size for a "
				"descriptor(0x7FFF)\n");
                return -EINVAL;
        }

        /* Check if fifo start is 8-byte alligned */
        if ((unsigned long)pipe->fifo.head & 0x7) {
                printf("bam: fifo address is not 8-bit aligned \n");
                return -EINVAL;
        }

        /* Check if fifo size is a power of 2.
         * The circular fifo logic in lk expects this.
         */
        if (!(ispow2(pipe->fifo_size))) {
                return -EINVAL;
        }

        pipe->fifo.current = pipe->fifo.head;

        /* Set the descriptor buffer size. Must be a multiple of 8 */
        writel(pipe->fifo_size * BAM_DESC_SIZE,
                BAM_P_FIFO_SIZESn(priv->base, dma->id));

        /* Write descriptors FIFO base addr must be 8-byte aligned */
        /* Needs a physical address conversion as we are setting up
         * the base of the FIFO for the BAM state machine.
         */
        writel((uint32_t)((unsigned long)pipe->fifo.head),
                BAM_P_DESC_FIFO_ADDRn(priv->base, dma->id));

        /* Initialize FIFO offset for the first read */
        pipe->fifo.offset = BAM_DESC_SIZE;

        writel(P_ENABLE | readl(BAM_P_CTRLn(priv->base, dma->id)),
			BAM_P_CTRLn(priv->base, dma->id));

        /* Everything is set.
         * Flag pipe init done.
         */

	pipe->enabled = true;
	debug("%s(dma id=%lu enabled=%d)\n", __func__, dma->id, pipe->enabled);

	return 0;
}

static int bam_dma_disable(struct dma *dma)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;

	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (!pipe->in_use)
		return -EBUSY;
	if (!pipe->enabled)
		return -EINVAL;

	pipe->enabled = false;
	debug("%s(dma id=%lu enabled=%d)\n", __func__, dma->id, pipe->enabled);

	return 0;
}

static int bam_dma_send(struct dma *dma, void *src,
		size_t len, void *metadata)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;
	unsigned int desc_len = 0, n = 0;
	unsigned int desc_flags;
	unsigned char *data_ptr = (unsigned char*)src;
	u8 flags = 0;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;
	if (!src || !metadata)
		return -EINVAL;

	debug("%s(dma id=%lu)\n", __func__, dma->id);

	flags = *((uint8_t*)metadata);
	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (pipe->dir != SYS2BAM)
		return -EINVAL;
	if (!pipe->in_use)
		return -EBUSY;
	if (!pipe->enabled)
		return -EINVAL;

	if (src == NULL || len == 0) {
		printf("bam: Wrong params for BAM transfer \n");
		return -EINVAL;
	}

	/* Check if we have enough space in FIFO */
	if (len > (unsigned)pipe->fifo_size * pipe->max_desc_len) {
		printf("bam: Data transfer exceeds desc fifo length.\n");
		return -EINVAL;
	}

	while (len) {
		/* There are only 16 bits to write data length.
		 * If more bits are needed, create more
		 * descriptors.
		 */
		if (len > pipe->max_desc_len) {
			desc_len = pipe->max_desc_len;
			len -= pipe->max_desc_len;
			desc_flags = 0;
		} else {
			desc_len = len;
			len = 0;
			/* Set correct flags on the last desc. */
			desc_flags = flags;
		}

		/* Write descriptor */
		bam_add_one_desc(pipe, data_ptr, desc_len, desc_flags);

		data_ptr += pipe->max_desc_len;
		n++;
	}

#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((unsigned long)pipe->fifo.head,
			((unsigned long)pipe->fifo.head +
			 (sizeof(struct bam_desc) * pipe->fifo_size)));
#endif

	/* Create a read/write event to notify the periperal of the
	 * added desc. */
	bam_sys_gen_event(pipe, n);

	/* Wait for the descriptors to be processed */
	bam_wait_for_interrupt(pipe, P_PRCSD_DESC_EN_MASK);

	/* Read offset update for the circular FIFO */
	bam_read_offset_update(pipe);

	return 0;
}

static int bam_dma_prepare_rcv_buf(struct dma *dma, void *dst, size_t size)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;

	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (pipe->dir != BAM2SYS)
		return -EINVAL;
	if (!pipe->in_use)
		return -EBUSY;
	if (!pipe->enabled)
		return -EINVAL;

	if (size == 0) {
		printf("bam: Wrong params for BAM transfer \n");
		return -EINVAL;
	}

	/* Check if we have enough space in FIFO */
	if (size > (unsigned)pipe->fifo_size * pipe->max_desc_len) {
		printf("bam: Data transfer exceeds desc fifo length.\n");
		return -EINVAL;
	}

	pipe->rx_len = size;

	return 0;
}

static int bam_dma_receive(struct dma *dma, void **dst, void *metadata)
{
	struct bam_dma_dev *priv = dev_get_priv(dma->dev);
	struct bam_dma_pipe *pipe;
	unsigned int desc_len = 0, n = 0;
	unsigned int desc_flags;
	unsigned char *data_ptr = (unsigned char*)dst;
	u8 flags = 0;
	size_t len;

	if (dma->id >= priv->n_pipe)
		return -EINVAL;
	if (!dst || !metadata)
		return -EINVAL;

	debug("%s(dma id=%lu)\n", __func__, dma->id);

	flags = *((uint8_t*)metadata);
	pipe = (struct bam_dma_pipe *)priv->pipe[dma->id];
	if (pipe->dir != BAM2SYS)
		return -EINVAL;
	if (!pipe->in_use)
		return -EBUSY;
	if (!pipe->enabled)
		return -EINVAL;

	len = pipe->rx_len;
	if (dst == NULL) {
		printf("bam: Wrong params for BAM transfer \n");
		return -EINVAL;
	}

	while (len) {
		/* There are only 16 bits to write data length.
		 * If more bits are needed, create more
		 * descriptors.
		 */
		if (len > pipe->max_desc_len) {
			desc_len = pipe->max_desc_len;
			len -= pipe->max_desc_len;
			desc_flags = 0;
		} else {
			desc_len = len;
			len = 0;
			/* Set correct flags on the last desc. */
			desc_flags = flags;
		}

		/* Write descriptor */
		bam_add_one_desc(pipe, data_ptr, desc_len, desc_flags);

		data_ptr += pipe->max_desc_len;
		n++;
	}

#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((unsigned long)pipe->fifo.head,
			((unsigned long)pipe->fifo.head +
			 (sizeof(struct bam_desc) * pipe->fifo_size)));
#endif

	/* Create a read/write event to notify the periperal of the
	 * added desc.
	 */
	bam_sys_gen_event(pipe, n);

	/* Wait for the descriptors to be processed */
	bam_wait_for_interrupt(pipe, P_PRCSD_DESC_EN_MASK);

	/* Read offset update for the circular FIFO */
	bam_read_offset_update(pipe);

	return 0;
}

static int bam_dma_probe(struct udevice *dev)
{
	struct bam_dma_dev *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret < 0)
		return ret;

	priv->n_pipe = (readl(BAM_NUM_PIPES(priv->base)) & BAM_NUM_PIPES_MSK);
	if (priv->n_pipe == 0)
		return -EIO;

	priv->pipe = (struct bam_dma_pipe **)malloc(
			sizeof(struct bam_dma_pipe*) * priv->n_pipe);
	if (!priv->pipe)
		return -ENOMEM;

	return 0;
}

static const struct dma_ops bam_dma_ops = {
	.of_xlate	= bam_dma_of_xlate,
	.request	= bam_dma_request,
	.rfree		= bam_dma_rfree,
	.enable		= bam_dma_enable,
	.disable	= bam_dma_disable,
	.send		= bam_dma_send,
	.prepare_rcv_buf = bam_dma_prepare_rcv_buf,
	.receive	= bam_dma_receive,
};

static const struct udevice_id bam_dma_ids[] = {
	{ .compatible = "qti,bam-v1.7.0" },
	{ }
};

U_BOOT_DRIVER(bam_dma) = {
	.name = "bam_dma",
	.id = UCLASS_DMA,
	.of_match = bam_dma_ids,
	.ops = &bam_dma_ops,
	.probe = bam_dma_probe,
	.priv_auto = sizeof(struct bam_dma_dev),
};
