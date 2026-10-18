/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <cpu_func.h>
#include <asm/io.h>
#include <command.h>
#include <blk.h>
#include <part.h>
#include <spi.h>
#include <spi_flash.h>
#include <memalign.h>

struct spi_flash *spi_detect(void)
{
	struct spi_flash *flash = NULL;
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct udevice *spi_dev;

	spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				&spi_dev);
	flash = dev_get_uclass_priv(spi_dev);
#else
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
#endif
	if (!flash) {
		printf("Failed to initialize SPI flash\n");
	}

	return flash;
}

ulong nor_bread(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
		void *dst)
{
	struct spi_flash *flash = spi_detect();
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	int ret, blksz;

	if ((blkcnt == 0) || !flash)
		return 0;

	blksz = block_dev->blksz;

	block_dev->lba = lldiv(flash->size, blksz);

	ret = spi_flash_read(flash, start * blksz, blkcnt * blksz, dst);
	if (ret)
		return 0;

#if !defined(CONFIG_SYS_DCACHE_OFF)
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + (blkcnt * blksz));
#endif
	return blkcnt;
}

ulong nor_bwrite(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
		 const void *src)
{
	struct spi_flash *flash = spi_detect();
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	int ret, blksz, lblkcnt, totalblkcnt, startoffset, erase_size;
	uint8_t *buff;
	const uint8_t *lsrc = src;

	if ((blkcnt == 0) || !flash || !block_dev)
		return 0;

	lblkcnt = blkcnt;

	erase_size = flash->erase_size;

	blksz = block_dev->blksz;

	block_dev->lba = lldiv(flash->size, blksz);

	totalblkcnt = erase_size / blksz;

	startoffset = start * blksz;

	if ((startoffset & (erase_size - 1)) || ((blkcnt % totalblkcnt))) {
		buff = (uint8_t *)malloc_cache_aligned(erase_size);
		if (buff == NULL) {
			printf(" block write: No memory \n");
			return 0;
		}
	}

	while (lblkcnt) {
		int offset, gap, tempcnt;
		bool backup = false;

		if (startoffset & (erase_size - 1)) {
			offset = startoffset & ~(erase_size - 1);
			gap = startoffset - offset;
			tempcnt = totalblkcnt - (gap / blksz);
			backup = true;

			if (lblkcnt < tempcnt)
				tempcnt = lblkcnt;
		} else {
			if (lblkcnt > totalblkcnt) {
				offset = startoffset;
				tempcnt = totalblkcnt;
			} else {
				offset = startoffset;
				tempcnt = lblkcnt;
				gap = 0;
				backup = true;
			}
		}

		if (backup) {
			ret = spi_flash_read(flash, offset, erase_size, buff);
			if (ret)
				break;

			memcpy(buff + gap, lsrc, tempcnt * blksz);
		}

		ret = spi_flash_erase(flash, offset, erase_size);
		if (ret)
			break;

		ret = spi_flash_write(flash, offset, erase_size,
					(backup)? buff : lsrc);
		if (ret)
			break;

		lblkcnt -= tempcnt;

		startoffset += (tempcnt * blksz);

		lsrc += tempcnt * blksz;
	}

	if (buff)
		free(buff);

	return (lblkcnt)? 0 : blkcnt;
}

unsigned long nor_berase(struct udevice *dev, lbaint_t start, lbaint_t blkcnt)
{
	/*
	* The single block erase cannot be done since the
	* NOR erase size is 64 KB and the block size is 4096 bytes.
	* The start and block count might be unaligned
	* so the block erase taken cared in nor_bwrite function
	*/
	return blkcnt;
}

static const struct blk_ops nor_blk_ops = {
	.read	= nor_bread,
	.write	= nor_bwrite,
	.erase = nor_berase,
};

U_BOOT_DRIVER(nor_blk) = {
	.name		= "nor_blk",
	.id		= UCLASS_BLK,
	.ops		= &nor_blk_ops,
	.flags		= DM_FLAG_PRE_RELOC,
};

static int do_nor_blk(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	const char *cmd;
	struct blk_desc *spi_dev;

	if (argc < 2)
		return CMD_RET_USAGE;

	cmd = argv[1];

	if ((strcmp(cmd, "part") == 0)) {
		if (spi_detect() == NULL)
			return CMD_RET_FAILURE;

		spi_dev = blk_get_devnum_by_uclass_id(UCLASS_SPI, 0);

		if (spi_dev != NULL && spi_dev->type != DEV_TYPE_UNKNOWN) {
			part_print(spi_dev);
		} else
			return CMD_RET_FAILURE;
	} else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static const char help[] =
	"part - dispaly GPT partition table\n"
	;

U_BOOT_CMD(
	nor, 2, 0, do_nor_blk,
	"spi-nor blk support", help
);
