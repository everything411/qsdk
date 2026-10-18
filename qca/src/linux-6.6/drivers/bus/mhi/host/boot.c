// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mhi.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include "internal.h"

/* Setup RDDM vector table for RDDM transfer and program RXVEC */
int mhi_rddm_prepare(struct mhi_controller *mhi_cntrl,
		     struct image_info *img_info)
{
	struct mhi_buf *mhi_buf = img_info->mhi_buf;
	struct bhi_vec_entry *bhi_vec = img_info->bhi_vec;
	void __iomem *base = mhi_cntrl->bhie;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	u32 sequence_id;
	unsigned int i;
	int ret;

	for (i = 0; i < img_info->entries - 1; i++, mhi_buf++, bhi_vec++) {
		bhi_vec->dma_addr = mhi_buf->dma_addr;
		bhi_vec->size = mhi_buf->len;
	}

	if (!mhi_cntrl->rddm_prealloc) {
		mhi_buf->dma_addr = dma_map_single(mhi_cntrl->cntrl_dev,
						   mhi_buf->buf, mhi_buf->len,
						   DMA_TO_DEVICE);
		if (dma_mapping_error(mhi_cntrl->cntrl_dev, mhi_buf->dma_addr)) {
			dev_err(dev, "dma mapping failed, Address: %p and len: 0x%zx\n",
				&mhi_buf->dma_addr, mhi_buf->len);
			return -ENOMEM;
		}
	}

	dev_dbg(dev, "BHIe programming for RDDM\n");

	mhi_write_reg(mhi_cntrl, base, BHIE_RXVECADDR_HIGH_OFFS,
		      upper_32_bits(mhi_buf->dma_addr));

	mhi_write_reg(mhi_cntrl, base, BHIE_RXVECADDR_LOW_OFFS,
		      lower_32_bits(mhi_buf->dma_addr));

	mhi_write_reg(mhi_cntrl, base, BHIE_RXVECSIZE_OFFS, mhi_buf->len);
	sequence_id = MHI_RANDOM_U32_NONZERO(BHIE_RXVECSTATUS_SEQNUM_BMSK);

	ret = mhi_write_reg_field(mhi_cntrl, base, BHIE_RXVECDB_OFFS,
				  BHIE_RXVECDB_SEQNUM_BMSK, sequence_id);
	if (ret) {
		dev_err(dev, "Failed to write sequence ID for BHIE_RXVECDB\n");
		return ret;
	}

	dev_dbg(dev, "Address: %p and len: 0x%zx sequence: %u\n",
		&mhi_buf->dma_addr, mhi_buf->len, sequence_id);

	return 0;
}

/* Collect RDDM buffer during kernel panic */
static int __mhi_download_rddm_in_panic(struct mhi_controller *mhi_cntrl)
{
	int ret;
	u32 rx_status;
	enum mhi_ee_type ee;
	const u32 delayus = 2000;
	u32 retry = (mhi_cntrl->timeout_ms * 1000) / delayus;
	const u32 rddm_timeout_us = 400000;
	int rddm_retry = rddm_timeout_us / delayus;
	void __iomem *base = mhi_cntrl->bhie;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	dev_dbg(dev, "Entered with pm_state:%s dev_state:%s ee:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		mhi_state_str(mhi_cntrl->dev_state),
		TO_MHI_EXEC_STR(mhi_cntrl->ee));

	/*
	 * This should only be executing during a kernel panic, we expect all
	 * other cores to shutdown while we're collecting RDDM buffer. After
	 * returning from this function, we expect the device to reset.
	 *
	 * Normaly, we read/write pm_state only after grabbing the
	 * pm_lock, since we're in a panic, skipping it. Also there is no
	 * gurantee that this state change would take effect since
	 * we're setting it w/o grabbing pm_lock
	 */
	mhi_cntrl->pm_state = MHI_PM_LD_ERR_FATAL_DETECT;
	/* update should take the effect immediately */
	smp_wmb();

	/*
	 * Make sure device is not already in RDDM. In case the device asserts
	 * and a kernel panic follows, device will already be in RDDM.
	 * Do not trigger SYS ERR again and proceed with waiting for
	 * image download completion.
	 */
	ee = mhi_get_exec_env(mhi_cntrl);
	if (ee == MHI_EE_MAX)
		goto error_exit_rddm;

	if (ee != MHI_EE_RDDM) {
		dev_dbg(dev, "Trigger device into RDDM mode using SYS ERR\n");
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_SYS_ERR);

		dev_dbg(dev, "Waiting for device to enter RDDM\n");
		while (rddm_retry--) {
			ee = mhi_get_exec_env(mhi_cntrl);
			if (ee == MHI_EE_RDDM)
				break;

			udelay(delayus);
		}

		if (rddm_retry <= 0) {
			/* Hardware reset so force device to enter RDDM */
			dev_dbg(dev,
				"Did not enter RDDM, do a host req reset\n");
			mhi_soc_reset(mhi_cntrl);
			udelay(delayus);
		}

		ee = mhi_get_exec_env(mhi_cntrl);
	}

	dev_dbg(dev,
		"Waiting for RDDM image download via BHIe, current EE:%s\n",
		TO_MHI_EXEC_STR(ee));

	while (retry--) {
		ret = mhi_read_reg_field(mhi_cntrl, base, BHIE_RXVECSTATUS_OFFS,
					 BHIE_RXVECSTATUS_STATUS_BMSK, &rx_status);
		if (ret)
			return -EIO;

		if (rx_status == BHIE_RXVECSTATUS_STATUS_XFER_COMPL)
			return 0;

		udelay(delayus);
	}

	ee = mhi_get_exec_env(mhi_cntrl);
	ret = mhi_read_reg(mhi_cntrl, base, BHIE_RXVECSTATUS_OFFS, &rx_status);

	dev_err(dev, "RXVEC_STATUS: 0x%x\n", rx_status);

error_exit_rddm:
	dev_err(dev, "RDDM transfer failed. Current EE: %s\n",
		TO_MHI_EXEC_STR(ee));
	mhi_dump_errdbg_reg(mhi_cntrl);
	return -EIO;
}

/* Download RDDM image from device */
int mhi_download_rddm_image(struct mhi_controller *mhi_cntrl, bool in_panic)
{
	void __iomem *base = mhi_cntrl->bhie;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	rwlock_t *pm_lock = &mhi_cntrl->pm_lock;
	struct mhi_buf *mhi_buf = NULL;
	u32 rx_status;
	int ret;

	/*
	 * Allocate RDDM table if specified, this table is for debugging purpose
	 */
	if (!mhi_cntrl->rddm_prealloc && mhi_cntrl->rddm_size) {
		ret = mhi_alloc_bhie_table(mhi_cntrl, &mhi_cntrl->rddm_image,
					   mhi_cntrl->rddm_size, IMG_TYPE_RDDM);
		if (ret) {
			dev_err(dev, "Failed to allocate RDDM table memory\n");
			return ret;
		}

		/* setup the RX vector table */
		ret = mhi_rddm_prepare(mhi_cntrl, mhi_cntrl->rddm_image);
		if (ret) {
			dev_err(dev, "Failed to prepare RDDM\n");
			mhi_free_bhie_table(mhi_cntrl, mhi_cntrl->rddm_image,
					    IMG_TYPE_RDDM);
			return ret;
		}
	}

	if (in_panic) {
		ret = __mhi_download_rddm_in_panic(mhi_cntrl);
		goto out;
	}

	dev_dbg(dev, "Waiting for RDDM image download via BHIe\n");

	/* Wait for the image download to complete */
	wait_event_timeout(mhi_cntrl->state_event,
			   mhi_read_reg_field(mhi_cntrl, base,
					      BHIE_RXVECSTATUS_OFFS,
					      BHIE_RXVECSTATUS_STATUS_BMSK,
					      &rx_status) || rx_status,
			   msecs_to_jiffies(mhi_cntrl->timeout_ms));

	ret = (rx_status == BHIE_RXVECSTATUS_STATUS_XFER_COMPL) ? 0 : -EIO;

out:
	mhi_buf = &mhi_cntrl->rddm_image->mhi_buf[mhi_cntrl->rddm_image->entries - 1];

	if (!mhi_cntrl->rddm_prealloc)
		dma_unmap_single(mhi_cntrl->cntrl_dev, mhi_buf->dma_addr,
				 mhi_buf->len, DMA_TO_DEVICE);

	if (ret) {
		dev_err(dev, "RDDM transfer failed. RXVEC_STATUS: 0x%x\n",
			rx_status);
		read_lock_bh(pm_lock);
		if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
			mhi_dump_errdbg_reg(mhi_cntrl);
		read_unlock_bh(pm_lock);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mhi_download_rddm_image);

static int mhi_fw_load_bhie(struct mhi_controller *mhi_cntrl,
			    const struct mhi_buf *mhi_buf)
{
	void __iomem *base = mhi_cntrl->bhie;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	rwlock_t *pm_lock = &mhi_cntrl->pm_lock;
	u32 tx_status, sequence_id;
	int ret;

	read_lock_bh(pm_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state)) {
		read_unlock_bh(pm_lock);
		return -EIO;
	}

	sequence_id = MHI_RANDOM_U32_NONZERO(BHIE_TXVECSTATUS_SEQNUM_BMSK);
	dev_dbg(dev, "Starting image download via BHIe. Sequence ID: %u\n",
		sequence_id);
	mhi_write_reg(mhi_cntrl, base, BHIE_TXVECADDR_HIGH_OFFS,
		      upper_32_bits(mhi_buf->dma_addr));

	mhi_write_reg(mhi_cntrl, base, BHIE_TXVECADDR_LOW_OFFS,
		      lower_32_bits(mhi_buf->dma_addr));

	mhi_write_reg(mhi_cntrl, base, BHIE_TXVECSIZE_OFFS, mhi_buf->len);

	ret = mhi_write_reg_field(mhi_cntrl, base, BHIE_TXVECDB_OFFS,
				  BHIE_TXVECDB_SEQNUM_BMSK, sequence_id);
	read_unlock_bh(pm_lock);

	if (ret)
		return ret;

	/* Wait for the image download to complete */
	ret = wait_event_timeout(mhi_cntrl->state_event,
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) ||
				 mhi_read_reg_field(mhi_cntrl, base,
						   BHIE_TXVECSTATUS_OFFS,
						   BHIE_TXVECSTATUS_STATUS_BMSK,
						   &tx_status) || tx_status,
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) ||
	    tx_status != BHIE_TXVECSTATUS_STATUS_XFER_COMPL) {
		dev_err(dev, "Upper:0x%x Lower:0x%x len:0x%zx sequence:%u\n",
			upper_32_bits(mhi_buf->dma_addr),
			lower_32_bits(mhi_buf->dma_addr),
			mhi_buf->len, sequence_id);

		dev_err(dev, "MHI pm_state: %s tx_status: %d ee: %s\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state), tx_status,
			TO_MHI_EXEC_STR(mhi_get_exec_env(mhi_cntrl)));

		read_lock_bh(pm_lock);
		if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
			mhi_dump_errdbg_reg(mhi_cntrl);
		read_unlock_bh(pm_lock);
		return -EIO;
	}

	return (!ret) ? -ETIMEDOUT : 0;
}

static int mhi_fw_load_bhi(struct mhi_controller *mhi_cntrl,
			   dma_addr_t dma_addr,
			   size_t size)
{
	u32 tx_status, session_id;
	int ret;
	void __iomem *base = mhi_cntrl->bhi;
	rwlock_t *pm_lock = &mhi_cntrl->pm_lock;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	read_lock_bh(pm_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state)) {
		read_unlock_bh(pm_lock);
		goto invalid_pm_state;
	}

	session_id = MHI_RANDOM_U32_NONZERO(BHI_TXDB_SEQNUM_BMSK);
	dev_dbg(dev, "Starting image download via BHI. Session ID: %u\n",
		session_id);
	mhi_write_reg(mhi_cntrl, base, BHI_STATUS, 0);
	mhi_write_reg(mhi_cntrl, base, BHI_IMGADDR_HIGH,
		      upper_32_bits(dma_addr));
	mhi_write_reg(mhi_cntrl, base, BHI_IMGADDR_LOW,
		      lower_32_bits(dma_addr));
	mhi_write_reg(mhi_cntrl, base, BHI_IMGSIZE, size);
	mhi_write_reg(mhi_cntrl, base, BHI_IMGTXDB, session_id);
	read_unlock_bh(pm_lock);

	/* Wait for the image download to complete */
	ret = wait_event_timeout(mhi_cntrl->state_event,
			   MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) ||
			   mhi_read_reg_field(mhi_cntrl, base, BHI_STATUS,
					      BHI_STATUS_MASK, &tx_status) || tx_status,
			   msecs_to_jiffies(mhi_cntrl->timeout_ms));
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		goto invalid_pm_state;

	if (tx_status == BHI_STATUS_ERROR) {
		dev_err(dev, "Image transfer failed\n");
		read_lock_bh(pm_lock);
		if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
			mhi_dump_errdbg_reg(mhi_cntrl);
		read_unlock_bh(pm_lock);
		goto invalid_pm_state;
	}

	return (!ret) ? -ETIMEDOUT : 0;

invalid_pm_state:

	return -EIO;
}

void mhi_free_bhie_table(struct mhi_controller *mhi_cntrl,
			 struct image_info *image_info,
			 enum image_type img_type)
{
	int i;
	struct mhi_buf *mhi_buf = image_info->mhi_buf;

	for (i = 0; i < image_info->entries; i++, mhi_buf++) {
		if (img_type == IMG_TYPE_RDDM && !mhi_cntrl->rddm_prealloc) {
			if (i == (image_info->entries - 1))
				dma_unmap_single(mhi_cntrl->cntrl_dev,
						 mhi_buf->dma_addr,
						 mhi_buf->len,
						 DMA_FROM_DEVICE);
			kfree(mhi_buf->buf);

		} else {
			mhi_fw_free_coherent(mhi_cntrl, mhi_buf->len,
					     mhi_buf->buf, mhi_buf->dma_addr);
		}
	}

	kfree(image_info->mhi_buf);
	kfree(image_info);
}

int mhi_alloc_bhie_table(struct mhi_controller *mhi_cntrl,
			 struct image_info **image_info,
			 size_t alloc_size, enum image_type img_type)
{
	size_t seg_size = mhi_cntrl->seg_len;
	int segments;
	int i;
	struct image_info *img_info;
	struct mhi_buf *mhi_buf;
	/* Maksed __GFP_DIRECT_RECLAIM flag for non-interrupt context
	 * to avoid rcu context sleep issue in kmalloc during panic scenario
	 */
	gfp_t gfp = (in_interrupt() ? GFP_ATOMIC :
		((GFP_KERNEL | __GFP_NORETRY) & ~__GFP_DIRECT_RECLAIM));

	if (img_type == IMG_TYPE_RDDM)
		seg_size = mhi_cntrl->rddm_seg_len;

	segments = DIV_ROUND_UP(alloc_size, seg_size) + 1;

	img_info = kzalloc(sizeof(*img_info), gfp);
	if (!img_info)
		return -ENOMEM;

	/* Allocate memory for entries */
	img_info->mhi_buf = kcalloc(segments, sizeof(*img_info->mhi_buf),
				    gfp);
	if (!img_info->mhi_buf)
		goto error_alloc_mhi_buf;

	/* Allocate and populate vector table */
	mhi_buf = img_info->mhi_buf;
	for (i = 0; i < segments; i++, mhi_buf++) {
		size_t vec_size = seg_size;

		/* Vector table is the last entry */
		if (i == segments - 1)
			vec_size = sizeof(struct bhi_vec_entry) * i;

		mhi_buf->len = vec_size;

		if (img_type == IMG_TYPE_RDDM && !mhi_cntrl->rddm_prealloc) {
			/* Vector table is the last entry */
			if (i == segments - 1) {
				mhi_buf->buf = kzalloc(PAGE_ALIGN(vec_size),
						       gfp);
				if (!mhi_buf->buf)
					goto error_alloc_segment;

				/* Vector table entry will be dma_mapped during
				 * rddm prepare with DMA_TO_DEVICE and unmapped
				 * once the target completes the RDDM XFER.
				 */
				continue;
			}
			mhi_buf->buf = kmalloc(vec_size, gfp);
			if (!mhi_buf->buf)
				goto error_alloc_segment;

			mhi_buf->dma_addr = dma_map_single(mhi_cntrl->cntrl_dev,
							   mhi_buf->buf,
							   vec_size,
							   DMA_FROM_DEVICE);
			if (dma_mapping_error(mhi_cntrl->cntrl_dev,
					      mhi_buf->dma_addr)) {
				kfree(mhi_buf->buf);
				goto error_alloc_segment;
			}
		} else {
			mhi_buf->buf = mhi_fw_alloc_coherent(mhi_cntrl,
							     vec_size,
							     &mhi_buf->dma_addr,
							     GFP_KERNEL);
			if (!mhi_buf->buf)
				goto error_alloc_segment;
		}
	}

	img_info->bhi_vec = img_info->mhi_buf[segments - 1].buf;
	img_info->entries = segments;
	*image_info = img_info;

	return 0;

error_alloc_segment:
	for (--i, --mhi_buf; i >= 0; i--, mhi_buf--) {
		if (img_type == IMG_TYPE_RDDM && !mhi_cntrl->rddm_prealloc) {
			dma_unmap_single(mhi_cntrl->cntrl_dev,
					 mhi_buf->dma_addr, mhi_buf->len,
					 DMA_FROM_DEVICE);
			kfree(mhi_buf->buf);

		} else {
			mhi_fw_free_coherent(mhi_cntrl, mhi_buf->len,
					     mhi_buf->buf, mhi_buf->dma_addr);
		}
	}

error_alloc_mhi_buf:
	kfree(img_info);

	return -ENOMEM;
}

static void mhi_firmware_copy(struct mhi_controller *mhi_cntrl,
			      const u8 *buf, size_t remainder,
			      struct image_info *img_info)
{
	size_t to_cpy;
	struct mhi_buf *mhi_buf = img_info->mhi_buf;
	struct bhi_vec_entry *bhi_vec = img_info->bhi_vec;

	while (remainder) {
		to_cpy = min(remainder, mhi_buf->len);
		memcpy(mhi_buf->buf, buf, to_cpy);
		bhi_vec->dma_addr = mhi_buf->dma_addr;
		bhi_vec->size = to_cpy;

		buf += to_cpy;
		remainder -= to_cpy;
		bhi_vec++;
		mhi_buf++;
	}
}

void mhi_fw_load_handler(struct mhi_controller *mhi_cntrl)
{
	const struct firmware *firmware = NULL;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	enum mhi_pm_state new_state;
	const char *fw_name;
	const u8 *fw_data;
	void *buf;
	dma_addr_t dma_addr;
	size_t size, fw_sz;
	int i, ret;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		dev_err(dev, "Device MHI is not in valid state\n");
		return;
	}

	/* save hardware info from BHI */
	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_SERIALNU,
			   &mhi_cntrl->serial_number);
	if (ret)
		dev_err(dev, "Could not capture serial number via BHI\n");

	for (i = 0; i < ARRAY_SIZE(mhi_cntrl->oem_pk_hash); i++) {
		ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_OEMPKHASH(i),
				   &mhi_cntrl->oem_pk_hash[i]);
		if (ret) {
			dev_err(dev, "Could not capture OEM PK HASH via BHI\n");
			break;
		}
	}

	/* wait for ready on pass through or any other execution environment */
	if (!MHI_FW_LOAD_CAPABLE(mhi_cntrl->ee))
		goto fw_load_ready_state;

	fw_name = (mhi_cntrl->ee == MHI_EE_EDL) ?
		mhi_cntrl->edl_image : mhi_cntrl->fw_image;

	/* check if the driver has already provided the firmware data */
	if (!fw_name && mhi_cntrl->fbc_download &&
	    mhi_cntrl->fw_data && mhi_cntrl->fw_sz) {
		if (!mhi_cntrl->sbl_size) {
			dev_err(dev, "fw_data provided but no sbl_size\n");
			goto error_fw_load;
		}

		size = mhi_cntrl->sbl_size;
		fw_data = mhi_cntrl->fw_data;
		fw_sz = mhi_cntrl->fw_sz;
		goto skip_req_fw;
	}

	if (!fw_name || (mhi_cntrl->fbc_download && (!mhi_cntrl->sbl_size ||
						     !mhi_cntrl->seg_len))) {
		dev_err(dev,
			"No firmware image defined or !sbl_size || !seg_len\n");
		goto error_fw_load;
	}

	ret = request_firmware(&firmware, fw_name, dev);
	if (ret) {
		dev_err(dev, "Error loading firmware: %d\n", ret);
		goto error_fw_load;
	}

	size = (mhi_cntrl->fbc_download) ? mhi_cntrl->sbl_size : firmware->size;

	/* SBL size provided is maximum size, not necessarily the image size */
	if (size > firmware->size)
		size = firmware->size;

	fw_data = firmware->data;
	fw_sz = firmware->size;

skip_req_fw:
	buf = mhi_fw_alloc_coherent(mhi_cntrl, size, &dma_addr, GFP_KERNEL);
	if (!buf) {
		release_firmware(firmware);
		goto error_fw_load;
	}

	/* Download image using BHI */
	memcpy(buf, fw_data, size);
	ret = mhi_fw_load_bhi(mhi_cntrl, dma_addr, size);
	mhi_fw_free_coherent(mhi_cntrl, size, buf, dma_addr);

	/* Error or in EDL mode, we're done */
	if (ret) {
		dev_err(dev, "MHI did not load image over BHI, ret: %d\n", ret);
		release_firmware(firmware);
		goto error_fw_load;
	}

	/* Wait for ready since EDL image was loaded */
	if (fw_name && fw_name == mhi_cntrl->edl_image) {
		release_firmware(firmware);
		goto fw_load_ready_state;
	}

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->dev_state = MHI_STATE_RESET;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/*
	 * If we're doing fbc, populate vector tables while
	 * device transitioning into MHI READY state
	 */
	if (mhi_cntrl->fbc_download) {
		ret = mhi_alloc_bhie_table(mhi_cntrl, &mhi_cntrl->fbc_image, fw_sz,
					   IMG_TYPE_FBC);
		if (ret) {
			release_firmware(firmware);
			goto error_fw_load;
		}

		/* Load the firmware into BHIE vec table */
		mhi_firmware_copy(mhi_cntrl, fw_data, fw_sz, mhi_cntrl->fbc_image);
	}

	release_firmware(firmware);

fw_load_ready_state:
	/* Transitioning into MHI RESET->READY state */
	ret = mhi_ready_state_transition(mhi_cntrl);
	if (ret) {
		dev_err(dev, "MHI did not enter READY state\n");
		goto error_ready_state;
	}

	dev_info(dev, "Wait for device to enter SBL or Mission mode\n");
	return;

error_ready_state:
	if (mhi_cntrl->fbc_download) {
		mhi_free_bhie_table(mhi_cntrl, mhi_cntrl->fbc_image,
				    IMG_TYPE_FBC);
		mhi_cntrl->fbc_image = NULL;
	}

error_fw_load:
	write_lock_irq(&mhi_cntrl->pm_lock);
	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_FW_DL_ERR);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	if (new_state == MHI_PM_FW_DL_ERR)
		wake_up_all(&mhi_cntrl->state_event);
}

int mhi_download_amss_image(struct mhi_controller *mhi_cntrl)
{
	struct image_info *image_info = mhi_cntrl->fbc_image;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	enum mhi_pm_state new_state;
	int ret;

	if (!image_info)
		return -EIO;

	ret = mhi_handle_boot_args(mhi_cntrl);
	if(ret) {
		dev_err(dev, "Failed to handle the boot-args, ret: %d\n",ret);
		return ret;
	}

	if (IS_QCN9224_DEV(mhi_cntrl)) {
		/* Download the License */
		mhi_download_fw_license(mhi_cntrl);
	}

	ret = mhi_fw_load_bhie(mhi_cntrl,
			       /* Vector table is the last entry */
			       &image_info->mhi_buf[image_info->entries - 1]);
	if (ret) {
		dev_err(dev, "MHI did not load AMSS, ret:%d\n", ret);
		write_lock_irq(&mhi_cntrl->pm_lock);
		new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_FW_DL_ERR);
		write_unlock_irq(&mhi_cntrl->pm_lock);
		if (new_state == MHI_PM_FW_DL_ERR)
			wake_up_all(&mhi_cntrl->state_event);
	}

	return ret;
}
